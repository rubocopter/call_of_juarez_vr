#include "backends/openxr/d3d11_session.hpp"
#include "backends/openxr/d3d11_swapchains.hpp"
#include "runtime/openxr_runtime.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>

namespace {

constexpr std::uint32_t kSubmittedFrameTarget = 180;
constexpr std::uint32_t kSessionReadyPollLimit = 1000;
constexpr std::uint32_t kFrameCycleLimit = 5400;
constexpr std::uint32_t kNonRunningPollLimit = 1000;

bool ClearEye(
    ID3D11Device* device,
    ID3D11DeviceContext* context,
    ID3D11Texture2D* texture,
    DXGI_FORMAT view_format,
    const std::array<float, 4>& color) {
    if (!device || !context || !texture) return false;

    D3D11_TEXTURE2D_DESC texture_desc{};
    texture->GetDesc(&texture_desc);

    D3D11_RENDER_TARGET_VIEW_DESC view_desc{};
    view_desc.Format = view_format;
    view_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    view_desc.Texture2D.MipSlice = 0;

    ID3D11RenderTargetView* render_target = nullptr;
    const HRESULT result = device->CreateRenderTargetView(texture, &view_desc, &render_target);
    if (FAILED(result) || !render_target) {
        std::cerr << "CreateRenderTargetView failed: hr=0x" << std::hex
                  << static_cast<unsigned long>(result)
                  << " format=0x" << static_cast<unsigned>(texture_desc.Format)
                  << std::dec
                  << " size=" << texture_desc.Width << 'x' << texture_desc.Height
                  << " array=" << texture_desc.ArraySize
                  << " mips=" << texture_desc.MipLevels
                  << " samples=" << texture_desc.SampleDesc.Count
                  << " bind=0x" << std::hex << texture_desc.BindFlags << std::dec
                  << '\n';
        return false;
    }

    context->ClearRenderTargetView(render_target, color.data());
    render_target->Release();
    return true;
}

} // namespace

int main() {
    using cojvr::backends::openxr::D3D11SessionBridge;
    using cojvr::backends::openxr::D3D11StereoSwapchains;
    using namespace cojvr::runtime;

    constexpr std::array<const char*, 1> required_extensions{{"XR_KHR_D3D11_enable"}};
    OpenXrRuntime runtime;
    if (!runtime.Initialize(OpenXrInitOptions{required_extensions})) {
        std::cerr << "OpenXR initialization failed: " << runtime.last_error() << '\n';
        return 2;
    }

    D3D11SessionBridge bridge;
    if (!bridge.Initialize(runtime)) {
        std::cerr << "OpenXR D3D11 session failed: " << bridge.last_error() << '\n';
        return 3;
    }

    D3D11StereoSwapchains swapchains;
    if (!swapchains.Initialize(runtime, bridge)) {
        std::cerr << "OpenXR D3D11 swapchain creation failed: "
                  << swapchains.last_error() << '\n';
        return 4;
    }

    std::cout << "OpenXR D3D11 session created; feature_level=0x"
              << std::hex << static_cast<unsigned>(bridge.feature_level())
              << " swapchain_format=0x" << static_cast<unsigned long long>(swapchains.format())
              << std::dec << '\n';

    for (std::uint32_t poll = 0;
         poll < kSessionReadyPollLimit && runtime.status() == OpenXrStatus::session_ready;
         ++poll) {
        if (!runtime.PollEvents()) {
            std::cerr << "OpenXR event polling failed: " << runtime.last_error() << '\n';
            return 5;
        }
        if (runtime.status() == OpenXrStatus::session_ready) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    if (runtime.status() != OpenXrStatus::running) {
        std::cerr << "OpenXR session did not enter the running state; status="
                  << static_cast<unsigned>(runtime.status()) << '\n';
        return 6;
    }

    constexpr std::array<float, 4> left_color{{0.08F, 0.12F, 0.18F, 1.0F}};
    constexpr std::array<float, 4> right_color{{0.18F, 0.12F, 0.08F, 1.0F}};
    std::uint32_t submitted_frames = 0;
    std::uint32_t frame_cycles = 0;
    std::uint32_t non_running_polls = 0;
    bool waiting_for_renderable_session_reported = false;

    while (submitted_frames < kSubmittedFrameTarget && frame_cycles < kFrameCycleLimit) {
        if (!runtime.PollEvents()) {
            std::cerr << "OpenXR event polling failed: " << runtime.last_error() << '\n';
            return 7;
        }
        if (runtime.status() == OpenXrStatus::exit_requested) break;
        if (runtime.status() != OpenXrStatus::running) {
            if (++non_running_polls >= kNonRunningPollLimit) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        non_running_polls = 0;
        ++frame_cycles;

        OpenXrFrameState frame;
        if (!runtime.WaitBeginFrame(frame)) {
            std::cerr << "OpenXR frame begin failed: " << runtime.last_error() << '\n';
            return 8;
        }

        if (!frame.should_render && !waiting_for_renderable_session_reported) {
            std::cout << "OpenXR session is running but not yet renderable; "
                         "waiting for the headset/session to become visible\n";
            waiting_for_renderable_session_reported = true;
        }

        std::array<EyeView, 2> views{};
        bool submit_projection = frame.should_render && runtime.LocateStereoViews(frame, views);
        if (frame.should_render && !submit_projection) {
            std::cerr << "OpenXR view location failed: " << runtime.last_error() << '\n';
        }

        if (submit_projection) {
            for (std::size_t eye_index = 0; eye_index < views.size(); ++eye_index) {
                const Eye eye = eye_index == 0 ? Eye::left : Eye::right;
                cojvr::backends::openxr::AcquiredEyeImage image;
                if (!swapchains.Acquire(eye, image)) {
                    std::cerr << "OpenXR swapchain acquire failed: "
                              << swapchains.last_error() << '\n';
                    submit_projection = false;
                    break;
                }

                const auto& color = eye == Eye::left ? left_color : right_color;
                if (!ClearEye(
                        bridge.device(), bridge.context(), image.texture,
                        static_cast<DXGI_FORMAT>(swapchains.format()), color)) {
                    std::cerr << "D3D11 render-target clear failed\n";
                    (void)swapchains.Release(eye);
                    submit_projection = false;
                    break;
                }
                if (!swapchains.Release(eye)) {
                    std::cerr << "OpenXR swapchain release failed: "
                              << swapchains.last_error() << '\n';
                    submit_projection = false;
                    break;
                }
            }
        }

        if (submit_projection) {
            if (waiting_for_renderable_session_reported && submitted_frames == 0) {
                std::cout << "OpenXR session became renderable; starting projection-frame validation\n";
            }
            const auto layer = swapchains.BuildProjectionLayer(views);
            const std::array<OpenXrCompositionLayer, 1> layers{{layer}};
            if (!runtime.EndFrame(frame, layers)) {
                std::cerr << "OpenXR frame submission failed: " << runtime.last_error() << '\n';
                return 9;
            }
            ++submitted_frames;
        } else {
            if (!runtime.EndFrame(frame)) {
                std::cerr << "OpenXR empty frame submission failed: " << runtime.last_error() << '\n';
                return 10;
            }
        }
    }

    if (submitted_frames != kSubmittedFrameTarget) {
        std::cerr << "OpenXR compositor probe incomplete; submitted_frames="
                  << submitted_frames << " target=" << kSubmittedFrameTarget << '\n';
        return 11;
    }

    std::cout << "OpenXR compositor probe passed; submitted_frames="
              << submitted_frames << '\n';
    return 0;
}

#include "backends/openvr/d3d11_compositor.hpp"
#include "backends/openvr/d3d11_session.hpp"
#include "backends/openvr/d3d11_sync.hpp"
#include "runtime/openvr_runtime.hpp"

#include <d3d11.h>
#include <wrl/client.h>

#include <array>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>

namespace {

const char* EyeName(const cojvr::runtime::Eye eye) {
    return eye == cojvr::runtime::Eye::left ? "left" : "right";
}

bool HasArgument(const int argc, wchar_t** argv, const std::wstring_view expected) {
    for (int index = 1; index < argc; ++index) {
        if (std::wstring_view(argv[index]) == expected) return true;
    }
    return false;
}

int IntegerArgument(
    const int argc,
    wchar_t** argv,
    const std::wstring_view name,
    const int fallback) {
    for (int index = 1; index + 1 < argc; ++index) {
        if (std::wstring_view(argv[index]) != name) continue;
        try {
            const long parsed = std::stol(argv[index + 1]);
            if (parsed <= 0 || parsed > std::numeric_limits<int>::max()) return fallback;
            return static_cast<int>(parsed);
        } catch (...) {
            return fallback;
        }
    }
    return fallback;
}

cojvr::backends::openvr::D3D11SyncStrategy SyncStrategyArgument(
    const int argc,
    wchar_t** argv) {
    using cojvr::backends::openvr::D3D11SyncStrategy;
    for (int index = 1; index + 1 < argc; ++index) {
        if (std::wstring_view(argv[index]) != L"--gpu-sync") continue;
        const std::wstring_view value(argv[index + 1]);
        if (value == L"flush") return D3D11SyncStrategy::flush;
        if (value == L"event" || value == L"event_query") {
            return D3D11SyncStrategy::event_query;
        }
        return D3D11SyncStrategy::none;
    }
    return D3D11SyncStrategy::none;
}

} // namespace

int wmain(const int argc, wchar_t** argv) {
    using namespace cojvr::runtime;

    OpenVrRuntime runtime;
    if (!runtime.Initialize("Call of Juarez VR OpenVR probe")) {
        std::cerr << runtime.last_error() << '\n';
        return 1;
    }

    const OpenVrSystemInfo& info = runtime.system_info();
    std::cout << "OpenVR initialized\n"
              << "recommended_eye_target=" << info.recommended_width << 'x'
              << info.recommended_height << '\n'
              << "dxgi_adapter_index=" << info.dxgi_adapter_index << '\n';

    cojvr::backends::openvr::D3D11SessionBridge d3d11;
    if (!d3d11.Initialize(runtime)) {
        std::cerr << d3d11.last_error() << '\n';
        return 1;
    }
    std::cout << "d3d11_feature_level=0x" << std::hex
              << static_cast<unsigned>(d3d11.feature_level()) << std::dec << '\n';

    std::array<EyeView, 2> eyes{};
    if (!runtime.ReadEyeConfiguration(eyes)) {
        std::cerr << runtime.last_error() << '\n';
        return 1;
    }

    std::cout << std::fixed << std::setprecision(6);
    for (const EyeView& eye : eyes) {
        std::cout << EyeName(eye.eye)
                  << " eye_to_head_pos=(" << eye.pose.position.x << ','
                  << eye.pose.position.y << ',' << eye.pose.position.z << ")"
                  << " fov=(" << eye.fov.angle_left << ',' << eye.fov.angle_right
                  << ',' << eye.fov.angle_up << ',' << eye.fov.angle_down << ")\n";
    }

    const bool request_visible_submit = HasArgument(argc, argv, L"--submit-visible");
    const bool request_submit = request_visible_submit || HasArgument(argc, argv, L"--submit");
    const bool request_pose = request_submit || HasArgument(argc, argv, L"--pose");
    if (request_pose) {
        Pose pose{};
        if (!runtime.WaitForHmdPose(pose)) {
            std::cerr << runtime.last_error() << '\n';
            return 1;
        }
        std::cout << "hmd_pose_valid="
                  << (pose.orientation_valid && pose.position_valid ? "true" : "false")
                  << " pos=(" << pose.position.x << ',' << pose.position.y << ','
                  << pose.position.z << ")\n";
    }

    if (request_submit) {
        using Microsoft::WRL::ComPtr;
        D3D11_TEXTURE2D_DESC texture_desc{};
        texture_desc.Width = info.recommended_width;
        texture_desc.Height = info.recommended_height;
        texture_desc.MipLevels = 1;
        texture_desc.ArraySize = 1;
        texture_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        texture_desc.SampleDesc.Count = 1;
        texture_desc.Usage = D3D11_USAGE_DEFAULT;
        texture_desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

        std::array<ComPtr<ID3D11Texture2D>, 2> textures{};
        std::array<ComPtr<ID3D11RenderTargetView>, 2> render_targets{};
        for (std::size_t index = 0; index < textures.size(); ++index) {
            HRESULT hr = d3d11.device()->CreateTexture2D(
                &texture_desc, nullptr, &textures[index]);
            if (FAILED(hr)) {
                std::cerr << "CreateTexture2D failed for eye " << index << '\n';
                return 1;
            }
            hr = d3d11.device()->CreateRenderTargetView(
                textures[index].Get(), nullptr, &render_targets[index]);
            if (FAILED(hr)) {
                std::cerr << "CreateRenderTargetView failed for eye " << index << '\n';
                return 1;
            }
        }

        const int frame_count = request_visible_submit
            ? IntegerArgument(argc, argv, L"--frames", 1800)
            : 1;
        const auto gpu_sync = SyncStrategyArgument(argc, argv);
        double gpu_sync_total_ms = 0.0;
        if (request_visible_submit) {
            std::cout << "visible_probe_pattern=animated_counter"
                      << " requested_frames=" << frame_count
                      << " gpu_sync="
                      << cojvr::backends::openvr::D3D11SyncStrategyName(gpu_sync) << '\n';
        }
        for (int frame = 0; frame < frame_count; ++frame) {
            if (frame > 0) {
                Pose frame_pose{};
                if (!runtime.WaitForHmdPose(frame_pose)) {
                    std::cerr << runtime.last_error() << '\n';
                    return 1;
                }
            }

            // Deliberately recognizable animation: the two eyes remain visibly
            // distinct while a shared counter pulse changes every submitted
            // frame. This makes a frozen/repeated compositor result obvious.
            const float phase = static_cast<float>(frame % 240) / 239.0F;
            const float pulse = 0.15F + 0.65F *
                (0.5F - 0.5F * std::cos(phase * 6.28318530718F));
            const std::array<float, 4> left_color{
                0.04F + pulse * 0.20F, 0.08F, 0.25F + pulse * 0.45F, 1.0F};
            const std::array<float, 4> right_color{
                0.25F + pulse * 0.45F, 0.08F, 0.04F + pulse * 0.20F, 1.0F};
            d3d11.context()->ClearRenderTargetView(
                render_targets[0].Get(), left_color.data());
            d3d11.context()->ClearRenderTargetView(
                render_targets[1].Get(), right_color.data());

            cojvr::backends::openvr::D3D11SyncResult sync_result{};
            std::string sync_error;
            if (!cojvr::backends::openvr::SynchronizeD3D11(
                    d3d11.device(), d3d11.context(), gpu_sync, 1000,
                    sync_result, sync_error)) {
                std::cerr << sync_error << '\n';
                return 1;
            }
            gpu_sync_total_ms += sync_result.elapsed_ms;

            std::string submit_error;
            if (!cojvr::backends::openvr::SubmitStereoD3D11(
                    runtime, textures[0].Get(), textures[1].Get(), submit_error)) {
                std::cerr << submit_error << '\n';
                return 1;
            }
        }

        if (request_visible_submit) {
            std::cout << "submitted_stereo_d3d11_frames=" << frame_count
                      << " gpu_sync_total_ms=" << gpu_sync_total_ms
                      << " gpu_sync_avg_ms=" << (gpu_sync_total_ms / frame_count) << '\n';
        } else {
            std::cout << "submitted_one_stereo_d3d11_frame=true\n";
        }
    }

    return 0;
}

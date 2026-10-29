// SPDX-License-Identifier: GPL-3.0-or-later
#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include "NeuralPass.h"
#include "neural_protocol.hpp"
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace protocol = cojvr::backends::openvr::neural;
using Microsoft::WRL::ComPtr;
static_assert(sizeof(void*) == 8, "The companion must be x64");

namespace {
struct Handle {
    HANDLE value{};
    ~Handle() { if (value) CloseHandle(value); }
    Handle() = default;
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
};
struct Ipc {
    Handle mapping, ready, request, complete, stop, parent;
    protocol::Header* header{};
    ~Ipc() { if (header) UnmapViewOfFile(header); }
    bool Open(const std::wstring& base) {
        // Open only: the parent owns names, reset types and creation lifetime.
        ready.value = OpenEventW(EVENT_MODIFY_STATE, FALSE, (base + L"-ready").c_str());
        mapping.value = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, (base + L"-map").c_str());
        if (mapping.value)
            header = static_cast<protocol::Header*>(MapViewOfFile(mapping.value, FILE_MAP_ALL_ACCESS, 0, 0, protocol::mapping_bytes));
        request.value = OpenEventW(SYNCHRONIZE, FALSE, (base + L"-request").c_str());
        complete.value = OpenEventW(EVENT_MODIFY_STATE, FALSE, (base + L"-complete").c_str());
        stop.value = OpenEventW(SYNCHRONIZE, FALSE, (base + L"-stop").c_str());
        return header && ready.value && request.value && complete.value && stop.value;
    }
    void Ready(unsigned result) const {
        if (header) { header->process_bits = 64; header->result = result; }
        MemoryBarrier();
        if (ready.value) SetEvent(ready.value);
    }
    bool Cancelled(DWORD timeout = 0) const {
        HANDLE waits[]{stop.value, parent.value};
        return WaitForMultipleObjects(2, waits, FALSE, timeout) != WAIT_TIMEOUT;
    }
};

enum class Mode { identity, edit, delay, fault, stale, model };
bool ParseMode(const wchar_t* text, Mode& mode) {
    if (!std::wcscmp(text, L"identity")) mode = Mode::identity;
    else if (!std::wcscmp(text, L"edit")) mode = Mode::edit;
    else if (!std::wcscmp(text, L"delay")) mode = Mode::delay;
    else if (!std::wcscmp(text, L"fault")) mode = Mode::fault;
    else if (!std::wcscmp(text, L"stale")) mode = Mode::stale;
    else if (!std::wcscmp(text, L"model")) mode = Mode::model;
    else return false;
    return true;
}
DWORD ParentPid(const wchar_t* text) {
    if (!text[0]) return 0;
    for (auto* p = text; *p; ++p) if (*p < L'0' || *p > L'9') return 0;
    errno = 0;
    wchar_t* end{};
    const auto value = std::wcstoull(text, &end, 10);
    return errno || !end || *end || !value || value > MAXDWORD ? 0 : static_cast<DWORD>(value);
}

Transport::Tracking Tracking(const protocol::Header& snapshot) {
    Transport::Tracking tracking{};
    tracking.valid = 1;
    tracking.id = snapshot.input.pose;
    tracking.tick = GetTickCount64();
    tracking.head.orientation.w = 1;
    for (unsigned eye = 0; eye != 2; ++eye) {
        const auto& source = snapshot.views[eye];
        auto& target = tracking.eyes[eye];
        // ABI already supplies view->world x/y/z/w and FOV L/R/U/D radians.
        // There is no OpenVR matrix, tangent or centimetre conversion here.
        target.pose.orientation = {source.orientation[0], source.orientation[1], source.orientation[2], source.orientation[3]};
        target.left = source.fov[0]; target.right = source.fov[1];
        target.up = source.fov[2]; target.down = source.fov[3];
    }
    return tracking;
}

class Model {
public:
    bool Initialize(const wchar_t* directory) {
        const std::filesystem::path path(directory);
        if (!path.is_absolute() || !std::filesystem::is_regular_file(path / L"nvngx_dlssnr.dll") ||
            !std::filesystem::is_regular_file(path / L"nvngx.dll_nr_bridge.dll")) return false;
        constexpr D3D_FEATURE_LEVEL levels[]{D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
        if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            levels, 2, D3D11_SDK_VERSION, &device_, nullptr, &context_))) return false;
        NeuralOptions options;
        options.enabled = true;
        return neural_.Initialize(device_.Get(), options, path.wstring());
    }
    void Suspend() { neural_.Suspend(); }
    bool Process(const protocol::Header& snapshot, const std::vector<std::uint8_t>& input, std::vector<std::uint8_t>& output) {
        if (!image_ || width_ != snapshot.width || height_ != snapshot.height) {
            // Publish resource ownership only once both allocations succeed.
            ComPtr<ID3D11Texture2D> image, staging;
            D3D11_TEXTURE2D_DESC desc{};
            desc.Width = snapshot.width; desc.Height = snapshot.height;
            desc.ArraySize = 2; desc.MipLevels = 1; desc.SampleDesc.Count = 1;
            // RGBA8 encoded UNORM bytes, matching DirectNR's residual contract.
            desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
            if (FAILED(device_->CreateTexture2D(&desc, nullptr, &image))) return false;
            desc.Usage = D3D11_USAGE_STAGING; desc.BindFlags = 0;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            if (FAILED(device_->CreateTexture2D(&desc, nullptr, &staging))) return false;
            image_ = std::move(image); staging_ = std::move(staging);
            width_ = snapshot.width; height_ = snapshot.height;
        }
        const auto eyeBytes = protocol::eye_bytes(snapshot.width, snapshot.height);
        const auto rowBytes = snapshot.width * 4;
        for (unsigned eye = 0; eye != 2; ++eye)
            context_->UpdateSubresource(image_.Get(), eye, nullptr, input.data() + eye * eyeBytes, rowBytes, 0);
        if (!neural_.Process(context_.Get(), image_.Get(), Tracking(snapshot), snapshot.input.frame, DXGI_FORMAT_R8G8B8A8_UNORM)) return false;
        context_->CopyResource(staging_.Get(), image_.Get());
        output.resize(input.size());
        for (unsigned eye = 0; eye != 2; ++eye) {
            D3D11_MAPPED_SUBRESOURCE mapped{};
            if (FAILED(context_->Map(staging_.Get(), eye, D3D11_MAP_READ, 0, &mapped))) { neural_.Suspend(); return false; }
            const bool valid = mapped.pData && mapped.RowPitch >= rowBytes;
            if (valid)
                for (unsigned row = 0; row != snapshot.height; ++row)
                    std::memcpy(output.data() + eye * eyeBytes + row * rowBytes,
                        static_cast<const std::uint8_t*>(mapped.pData) + std::size_t(row) * mapped.RowPitch, rowBytes);
            context_->Unmap(staging_.Get(), eye);
            if (!valid) { neural_.Suspend(); return false; }
        }
        return SUCCEEDED(device_->GetDeviceRemovedReason());
    }
private:
    ComPtr<ID3D11Device> device_;
    ComPtr<ID3D11DeviceContext> context_;
    ComPtr<ID3D11Texture2D> image_, staging_;
    std::uint32_t width_{}, height_{};
    NeuralPass neural_;
};
} // namespace

int wmain(int argc, wchar_t** argv) {
    if (argc != 5) {
        std::fputs("CoJNeuralHost.exe <base-name> <parent-pid> <runtime-dir> <mode>\n", stderr);
        return 2;
    }
    Ipc ipc;
    std::unique_ptr<Model> model;
    bool initialized = false;
    try {
        const bool opened = ipc.Open(argv[1]);
        const auto pid = ParentPid(argv[2]);
        if (pid) ipc.parent.value = OpenProcess(SYNCHRONIZE, FALSE, pid);
        Mode mode{};
        if (!opened || !ipc.parent.value || !ParseMode(argv[4], mode) ||
            ipc.header->signature != protocol::magic || ipc.header->protocol != protocol::version) {
            ipc.Ready(9); return 9;
        }
        // Diagnostics never allocate a Model, D3D device or NVIDIA runtime.
        if (mode == Mode::model) {
            model = std::make_unique<Model>();
            if (!model->Initialize(argv[3])) { ipc.Ready(9); return 9; }
        }
        ipc.Ready(0); initialized = true;
        std::uint64_t previousEpoch{};
        for (;;) {
            // Windows selects the first signaled handle: stop, parent, request.
            HANDLE waits[]{ipc.stop.value, ipc.parent.value, ipc.request.value};
            const DWORD wait = WaitForMultipleObjects(3, waits, FALSE, INFINITE);
            if (wait != WAIT_OBJECT_0 + 2 || ipc.Cancelled()) break;
            const protocol::Header snapshot = *ipc.header;
            bool success = protocol::Valid(snapshot);
            std::vector<std::uint8_t> input, output;
            if (success) {
                const auto bytes = protocol::eye_bytes(snapshot.width, snapshot.height) * 2;
                input.assign(protocol::input_pixels(ipc.header), protocol::input_pixels(ipc.header) + bytes);
                if (model && snapshot.input.epoch != previousEpoch) model->Suspend();
                previousEpoch = snapshot.input.epoch;
                if (mode == Mode::fault && snapshot.request == 2) ExitProcess(23);
                if (mode == Mode::delay && snapshot.request == 2 && ipc.Cancelled(100)) break;
                if (model) success = model->Process(snapshot, input, output);
                else {
                    output = input;
                    if (mode == Mode::edit)
                        for (std::size_t i = 0; i < output.size(); ++i)
                            if (i % 4 != 3) output[i] = static_cast<std::uint8_t>(std::min(255, int(output[i]) + 16));
                }
            } else if (model) model->Suspend();
            if (ipc.Cancelled()) break;
            // Full CPU pair is private until both model readbacks succeeded.
            if (success) std::memcpy(protocol::output_pixels(ipc.header), output.data(), output.size());
            ipc.header->output = snapshot.input;
            if (success && mode == Mode::stale && snapshot.request == 2) ++ipc.header->output.revision;
            ipc.header->completed = snapshot.request;
            ipc.header->result = success ? 1 : 2;
            ipc.header->process_bits = 64;
            MemoryBarrier();
            if (!SetEvent(ipc.complete.value)) return 3;
        }
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Neural helper failure: %s\n", error.what());
        if (!initialized) ipc.Ready(9);
        return 9;
    }
}

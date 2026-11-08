#pragma once

#include "runtime/hud_text.hpp"

#include <d3d11.h>
#include <wrl/client.h>

#include <array>
#include <charconv>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <future>
#include <memory>
#include <sstream>

namespace cojvr::backends::openvr {

struct SubtitleEyeCaptureIdentity {
    std::uint64_t device = 0, generation = 0, frame = 0, pose = 0;
    bool operator==(const SubtitleEyeCaptureIdentity&) const = default;
};

constexpr bool SubtitleEyeCaptureExtentFits(UINT width, UINT height) noexcept {
    constexpr std::uint64_t max_bytes = 64 * 1024 * 1024;
    return width > 0 && height > 1 &&
        width <= D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION / 2 &&
        height <= D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION - 1 &&
        std::uint64_t(width) * (height - height / 2) * 16 <= max_bytes;
}

// Opt-in, one lower-half contact sheet per configured cinematic line. This
// observer only copies the presenter's owned textures, outside native leases.
class SubtitleEyeCapture {
public:
    void Load(const std::filesystem::path& config) noexcept {
        try {
            std::ifstream input(config, std::ios::binary);
            if (!input) return;
            char bytes[64]{};
            input.read(bytes, sizeof(bytes));
            if (!input.eof() || input.bad()) return;
            std::istringstream tokens(std::string(bytes, static_cast<std::size_t>(input.gcount())));
            std::array<std::uint64_t, 2> fingerprints{};
            for (auto& fingerprint : fingerprints) {
                std::string token;
                if (!(tokens >> token)) return;
                const auto end = token.data() + token.size();
                const auto parsed = std::from_chars(token.data(), end, fingerprint);
                if (parsed.ec != std::errc{} || parsed.ptr != end || !fingerprint) return;
            }
            std::string extra;
            if ((tokens >> extra) || fingerprints[0] == fingerprints[1]) return;
            const auto root = config.parent_path() / L"subtitle-eye-capture";
            std::filesystem::create_directories(root);
            auto directory = root / (std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64()));
            if (!std::filesystem::create_directory(directory)) return;
            output_ = std::move(directory);
            targets_ = fingerprints;
        } catch (...) {}
    }

    bool Begin(ID3D11DeviceContext* context, const std::array<ID3D11Texture2D*, 2>& eyes,
        const SubtitleEyeCaptureIdentity& id, const runtime::StereoHudTextOverlay& hud) noexcept {
        if (!targets_[0] || pending_.texture || writer_.valid() || !context || !eyes[0] || !eyes[1] || eyes[0] == eyes[1] ||
            !id.device || !id.generation || !id.frame || !id.pose) return false;
        const auto& source = hud.subtitle_capture;
        const auto text = hud.text.subtitle.view();
        if (hud.frame_sequence != id.frame || source.pose_sequence != id.pose || !source.owner_epoch ||
            source.source != runtime::SubtitleSource::cinematic || !source.read_valid || text.empty()) return false;
        const auto fingerprint = runtime::SubtitleFingerprint(text);
        const unsigned role = fingerprint == targets_[0] ? 0 : 1;
        if (fingerprint != targets_[role] || captured_[role] || attempts_[role] >= 3) return false;
        ++attempts_[role];

        D3D11_TEXTURE2D_DESC left{}, right{};
        eyes[0]->GetDesc(&left);
        eyes[1]->GetDesc(&right);
        const UINT top = left.Height / 2;
        const UINT rows = left.Height - top;
        if (!SubtitleEyeCaptureExtentFits(left.Width, left.Height) ||
            left.Width != right.Width || left.Height != right.Height || left.Format != right.Format ||
            left.SampleDesc.Count != 1 || right.SampleDesc.Count != 1 ||
            (left.Format != DXGI_FORMAT_B8G8R8A8_UNORM && left.Format != DXGI_FORMAT_B8G8R8X8_UNORM) ||
            left.ArraySize != 1 || right.ArraySize != 1 || left.MipLevels != 1 || right.MipLevels != 1) return false;

        D3D11_TEXTURE2D_DESC sheet{};
        sheet.Width = left.Width * 2;
        sheet.Height = rows * 2;
        sheet.MipLevels = sheet.ArraySize = sheet.SampleDesc.Count = 1;
        sheet.Format = left.Format;
        sheet.Usage = D3D11_USAGE_STAGING;
        sheet.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        Microsoft::WRL::ComPtr<ID3D11Device> device;
        context->GetDevice(&device);
        if (FAILED(device->CreateTexture2D(&sheet, nullptr, &pending_.texture))) return false;
        pending_.id = id;
        pending_.source = source;
        pending_.role = role;
        pending_.eyes = eyes;
        pending_.crop = {0, top, 0, left.Width, left.Height, 1};
        CopyEyes(context, eyes, 0);
        return true;
    }

    bool BeforeSubmit(ID3D11DeviceContext* context, const std::array<ID3D11Texture2D*, 2>& eyes,
        const SubtitleEyeCaptureIdentity& id) noexcept {
        if (!pending_.texture || pending_.deadline_ms || pending_.id != id || pending_.eyes != eyes) return false;
        CopyEyes(context, eyes, pending_.crop.bottom - pending_.crop.top);
        pending_.composed = true;
        return true;
    }

    void ConfirmSubmitted(std::uint64_t submit) noexcept {
        if (!pending_.composed || pending_.deadline_ms || !submit) return;
        pending_.submit = submit;
        pending_.deadline_ms = GetTickCount64() + 2000;
    }

    void DiscardUnsubmitted() noexcept {
        // Keep already queued copies alive until the same nonblocking retirement
        // as a submitted sample. A retry cannot allocate a second atlas.
        if (pending_.texture && !pending_.deadline_ms) pending_.deadline_ms = GetTickCount64() + 2000;
    }

    [[nodiscard]] bool busy() const noexcept { return pending_.texture || writer_.valid(); }

    void Poll(ID3D11DeviceContext* context) noexcept {
        if (writer_.valid() && writer_.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
            writer_ = {};
        if (!pending_.deadline_ms) return;
        if (GetTickCount64() >= pending_.deadline_ms) { pending_ = {}; return; }
        D3D11_MAPPED_SUBRESOURCE mapped{};
        const HRESULT result = context->Map(pending_.texture.Get(), 0, D3D11_MAP_READ,
            D3D11_MAP_FLAG_DO_NOT_WAIT, &mapped);
        if (result == DXGI_ERROR_WAS_STILL_DRAWING) return;
        if (FAILED(result)) { pending_ = {}; return; }
        if (!pending_.submit) {
            context->Unmap(pending_.texture.Get(), 0);
            pending_ = {};
            return;
        }
        const UINT width = pending_.crop.right * 2;
        const UINT height = (pending_.crop.bottom - pending_.crop.top) * 2;
        const UINT pitch = width * 4;
        const auto bytes = std::size_t(pitch) * height;
        std::unique_ptr<char[]> pixels;
        try {
            if (mapped.RowPitch >= pitch) {
                pixels = std::make_unique_for_overwrite<char[]>(bytes);
                const auto* rows = static_cast<const char*>(mapped.pData);
                for (UINT y = 0; y < height; ++y)
                    std::memcpy(pixels.get() + std::size_t(y) * pitch, rows + std::size_t(y) * mapped.RowPitch, pitch);
            }
        } catch (...) {}
        context->Unmap(pending_.texture.Get(), 0);

        if (pixels) {
            try {
                const auto& id = pending_.id;
                const auto& source = pending_.source;
                const unsigned role = pending_.role;
                auto name = std::wstring(role == 0 ? L"affected" : L"control") +
                    L"-f" + std::to_wstring(id.frame) + L"-s" + std::to_wstring(pending_.submit) +
                    L"-p" + std::to_wstring(id.pose) + L"-g" + std::to_wstring(id.generation) +
                    L"-d" + std::to_wstring(id.device) + L"-e" + std::to_wstring(source.owner_epoch) +
                    L"-r" + std::to_wstring(source.recenter_sequence) + L"-t" + std::to_wstring(targets_[role]) +
                    L"-h" + std::to_wstring(pending_.crop.bottom) + L"-y" + std::to_wstring(pending_.crop.top) + L".bmp";
                writer_ = std::async(std::launch::async, [path = output_ / name, width, height, bytes, pixels = std::move(pixels)] {
                    WriteSheet(path, width, height, pixels.get(), bytes);
                });
                captured_[role] = true;
            } catch (...) {}
        }
        pending_ = {};
    }

private:
    void CopyEyes(ID3D11DeviceContext* context, const std::array<ID3D11Texture2D*, 2>& eyes, UINT y) noexcept {
        for (UINT eye = 0; eye < eyes.size(); ++eye)
            context->CopySubresourceRegion(pending_.texture.Get(), 0, eye * pending_.crop.right, y, 0,
                eyes[eye], 0, &pending_.crop);
    }

    static void WriteSheet(const std::filesystem::path& path, UINT width, UINT height,
        const char* pixels, std::size_t bytes) noexcept {
        try {
            BITMAPFILEHEADER file{};
            file.bfType = 0x4d42;
            file.bfOffBits = sizeof(file) + sizeof(BITMAPINFOHEADER);
            file.bfSize = file.bfOffBits + static_cast<DWORD>(bytes);
            BITMAPINFOHEADER bitmap{};
            bitmap.biSize = sizeof(bitmap);
            bitmap.biWidth = static_cast<LONG>(width);
            bitmap.biHeight = -static_cast<LONG>(height);
            bitmap.biPlanes = 1;
            bitmap.biBitCount = 32;
            auto temporary = path;
            temporary += L".partial";
            std::ofstream output(temporary, std::ios::binary);
            output.write(reinterpret_cast<const char*>(&file), sizeof(file));
            output.write(reinterpret_cast<const char*>(&bitmap), sizeof(bitmap));
            output.write(pixels, static_cast<std::streamsize>(bytes));
            output.close();
            if (output) std::filesystem::rename(temporary, path);
        } catch (...) {}
    }

    struct Pending {
        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
        SubtitleEyeCaptureIdentity id{};
        runtime::SubtitleCaptureDiagnostic source{};
        std::array<ID3D11Texture2D*, 2> eyes{};
        D3D11_BOX crop{};
        unsigned role = 0;
        bool composed = false;
        std::uint64_t submit = 0, deadline_ms = 0;
    } pending_;
    std::filesystem::path output_;
    std::array<std::uint64_t, 2> targets_{};
    std::array<unsigned, 2> attempts_{};
    std::array<bool, 2> captured_{};
    std::future<void> writer_;
};

} // namespace cojvr::backends::openvr

#pragma once

#include <d3d9.h>

#include <cstdint>

namespace cojvr::games::call_of_juarez {

enum class LegacyTextureKind {
    texture_2d,
    volume_texture,
    cube_texture,
};

enum class LegacyTextureCreatePhase {
    enter,
    result,
};

struct LegacyTextureCreateEvent {
    LegacyTextureKind kind = LegacyTextureKind::texture_2d;
    LegacyTextureCreatePhase phase = LegacyTextureCreatePhase::enter;
    std::uint64_t sequence = 0;
    std::uint32_t thread_id = 0;
    std::uintptr_t device = 0;
    std::uintptr_t caller = 0;
    UINT width = 0;
    UINT height = 0;
    UINT depth = 0;
    UINT levels = 0;
    DWORD requested_usage = 0;
    DWORD effective_usage = 0;
    D3DFORMAT format = D3DFMT_UNKNOWN;
    D3DPOOL requested_pool = D3DPOOL_DEFAULT;
    D3DPOOL effective_pool = D3DPOOL_DEFAULT;
    bool translated = false;
    HRESULT result = S_OK;
};

using LegacyTextureEventCallback =
    void (*)(void* context, const LegacyTextureCreateEvent& event) noexcept;

struct LegacyTextureCompatibilityCallbacks {
    void* context = nullptr;
    LegacyTextureEventCallback event = nullptr;
    void (*resource_destroy)(
        void* context, std::uintptr_t resource, LegacyTextureKind kind,
        ULONG refcount) noexcept = nullptr;
};

[[nodiscard]] bool InstallD3D9ExLegacyTextureCompatibility(
    IDirect3DDevice9* device,
    LegacyTextureCompatibilityCallbacks callbacks = {}) noexcept;

[[nodiscard]] bool ReacquireD3D9ExLegacyTextureCompatibility(
    IDirect3DDevice9* device) noexcept;

[[nodiscard]] bool RestoreD3D9ExLegacyTextureCompatibility() noexcept;

} // namespace cojvr::games::call_of_juarez

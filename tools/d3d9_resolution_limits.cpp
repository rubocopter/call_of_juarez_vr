// Necessary adapter-0 caps only: no device, allocation or rendering qualification.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "backends/d3d9/system_d3d9.hpp"
#include <wrl/client.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <string_view>

namespace {
struct FormatCheck {
    const char* name;
    D3DFORMAT format;
    DWORD usage;
    D3DRESOURCETYPE type;
    const char* result = "null";
};
struct Limits {
    std::uint32_t width = 0, height = 0;
    D3DCAPS9 caps{};
    const char* status = "unavailable";
    const char* error = "";
    std::array<FormatCheck, 5> formats{{
        {"X8R8G8B8", D3DFMT_X8R8G8B8, D3DUSAGE_RENDERTARGET, D3DRTYPE_TEXTURE},
        {"A8R8G8B8", D3DFMT_A8R8G8B8, D3DUSAGE_RENDERTARGET, D3DRTYPE_TEXTURE},
        {"A16B16G16R16F", D3DFMT_A16B16G16R16F, D3DUSAGE_RENDERTARGET, D3DRTYPE_TEXTURE},
        {"D16", D3DFMT_D16, D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE},
        {"D24S8", D3DFMT_D24S8, D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE}}};
};

bool PositiveInt32(const std::wstring_view text, std::uint32_t& output) {
    if (text.empty()) return false;
    std::uint32_t value = 0;
    for (const wchar_t character : text) {
        if (character < L'0' || character > L'9') return false;
        const auto digit = static_cast<std::uint32_t>(character - L'0');
        if (value > (2147483647u - digit) / 10u) return false;
        value = value * 10u + digit;
    }
    if (!value) return false;
    output = value;
    return true;
}

bool Parse(const int argc, wchar_t** argv, Limits& limits) {
    if (argc != 5) return false;
    std::uint32_t width = 0, height = 0;
    for (int index = 1; index < argc; index += 2) {
        const std::wstring_view option(argv[index]);
        if (option == L"--width" && !width) {
            if (!PositiveInt32(argv[index + 1], width)) return false;
        } else if (option == L"--height" && !height) {
            if (!PositiveInt32(argv[index + 1], height)) return false;
        } else return false;
    }
    if (!width || !height) return false;
    limits.width = width;
    limits.height = height;
    return true;
}

void Query(Limits& limits) {
    using namespace cojvr::backends::d3d9;
    const auto create = SystemDirect3DCreate9();
    if (!create || !IsExpectedSystemD3D9Module()) {
        limits.error = "system_d3d9_unavailable";
        return;
    }
    Microsoft::WRL::ComPtr<IDirect3D9> factory;
    factory.Attach(create(D3D_SDK_VERSION));
    if (!factory || factory->GetAdapterCount() == 0) {
        limits.error = "adapter_0_unavailable";
        return;
    }
    D3DCAPS9 caps{};
    if (FAILED(factory->GetDeviceCaps(0, D3DDEVTYPE_HAL, &caps)) ||
        !caps.MaxTextureWidth || !caps.MaxTextureHeight) {
        limits.error = "hal_caps_unavailable";
        return;
    }
    limits.caps = caps;
    D3DDISPLAYMODE mode{};
    if (FAILED(factory->GetAdapterDisplayMode(0, &mode)) || mode.Format == D3DFMT_UNKNOWN) {
        limits.error = "adapter_display_format_unavailable";
        return;
    }
    bool unsupported = false, query_failed = false;
    for (auto& format : limits.formats) {
        const HRESULT result = factory->CheckDeviceFormat(0, D3DDEVTYPE_HAL,
            mode.Format, format.usage, format.type, format.format);
        if (SUCCEEDED(result)) format.result = "true";
        else if (result == D3DERR_NOTAVAILABLE) {
            format.result = "false";
            unsupported = true;
        } else query_failed = true;
    }
    if (query_failed) {
        limits.error = "format_query_unavailable";
        return;
    }
    const std::uint64_t width = limits.width, height = limits.height;
    const auto power_of_two = [](const auto value) { return (value & (value - 1)) == 0; };
    const char* reason = "";
    if (width > caps.MaxTextureWidth || height > caps.MaxTextureHeight)
        reason = "texture_axis_limit_exceeded";
    else if (caps.MaxTextureAspectRatio &&
        (width > height * caps.MaxTextureAspectRatio || height > width * caps.MaxTextureAspectRatio))
        reason = "texture_aspect_ratio_limit_exceeded";
    else if ((caps.TextureCaps & D3DPTEXTURECAPS_SQUAREONLY) && width != height)
        reason = "square_texture_required";
    else if ((caps.TextureCaps & D3DPTEXTURECAPS_POW2) &&
        !(caps.TextureCaps & D3DPTEXTURECAPS_NONPOW2CONDITIONAL) &&
        (!power_of_two(width) || !power_of_two(height)))
        reason = "power_of_two_texture_required";
    else if (unsupported) reason = "required_format_unsupported";
    limits.error = reason;
    limits.status = *reason ? "incompatible" : "caps_compatible";
}

int Emit(const Limits& limits) {
    std::printf("{\"schemaVersion\":1,\"recordType\":\"cojvr-native-resolution-limits\","
        "\"status\":\"%s\",\"requestedWidth\":%lu,\"requestedHeight\":%lu,\"adapterIndex\":0,"
        "\"maxTextureWidth\":%lu,\"maxTextureHeight\":%lu,\"maxAspectRatio\":%lu,\"formatChecks\":{",
        limits.status, static_cast<unsigned long>(limits.width), static_cast<unsigned long>(limits.height),
        static_cast<unsigned long>(limits.caps.MaxTextureWidth),
        static_cast<unsigned long>(limits.caps.MaxTextureHeight),
        static_cast<unsigned long>(limits.caps.MaxTextureAspectRatio));
    bool first = true;
    for (const auto& format : limits.formats) {
        std::printf("%s\"%s\":%s", first ? "" : ",", format.name, format.result);
        first = false;
    }
    std::printf("},\"allocationValidated\":false,\"memoryQualified\":false,"
        "\"theaterQualified\":false,\"error\":\"%s\"}\n", limits.error);
    return std::string_view(limits.status) == "caps_compatible" ? 0 : 2;
}
} // namespace

int wmain(const int argc, wchar_t** argv) {
    Limits limits;
    if (!Parse(argc, argv, limits)) limits.error = "invalid_arguments";
    else {
        try { Query(limits); }
        catch (...) { limits.status = "unavailable"; limits.error = "query_exception"; }
    }
    return Emit(limits);
}

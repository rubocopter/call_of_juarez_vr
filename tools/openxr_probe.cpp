#include "runtime/openxr_runtime.hpp"

#include <array>
#include <iostream>

int main() {
    using namespace cojvr::runtime;

    constexpr std::array<const char*, 1> required_extensions{{"XR_KHR_D3D11_enable"}};
    OpenXrRuntime runtime;
    if (!runtime.Initialize(OpenXrInitOptions{required_extensions})) {
        std::cerr << "OpenXR probe failed: " << runtime.last_error() << '\n';
        return 2;
    }

    const auto& system = runtime.system_info();
    const auto& views = runtime.recommended_views();
    std::cout << "OpenXR system: " << system.name << '\n';
    std::cout << "vendor_id=" << system.vendor_id
              << " max_swapchain=" << system.max_swapchain_width << 'x'
              << system.max_swapchain_height << '\n';
    std::cout << "tracking orientation=" << (system.orientation_tracking ? 1 : 0)
              << " position=" << (system.position_tracking ? 1 : 0) << '\n';
    std::cout << "left_recommended=" << views[0].width << 'x' << views[0].height << '\n';
    std::cout << "right_recommended=" << views[1].width << 'x' << views[1].height << '\n';
    return 0;
}

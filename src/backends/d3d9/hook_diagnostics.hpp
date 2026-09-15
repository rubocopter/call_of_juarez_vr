#pragma once

#include <cstddef>
#include <string_view>
#include <vector>

namespace cojvr::backends::d3d9 {

struct HookSlotDiagnostic {
    std::string_view interface_name{};
    std::string_view slot_name{};
    void** vtable = nullptr;
    std::size_t index = 0;
    void* original = nullptr;
    void* replacement = nullptr;
    void* current = nullptr;
    bool owned = false;
};

using HookDiagnostics = std::vector<HookSlotDiagnostic>;

} // namespace cojvr::backends::d3d9

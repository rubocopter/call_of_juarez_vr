#pragma once

#include <unknwn.h>

namespace cojvr::backends::d3d9 {

// Diagnostics only. A zero refcount means the operation did not return one.
using ComTraceCallback = void (*)(
    void* object, const char* event, const IID* requested_interface,
    void* returned_interface, HRESULT result, ULONG refcount) noexcept;

} // namespace cojvr::backends::d3d9

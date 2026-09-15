#pragma once

#include <d3d9.h>

#include <cstdint>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

namespace cojvr::backends::d3d9 {

struct FactoryDiagnosticContext {
    std::uint64_t factory_id = 0;
    DWORD creation_thread_id = 0;
    void* identity = nullptr;
    void** vtable = nullptr;
};

struct DeviceDiagnosticContext {
    std::uint64_t factory_id = 0;
    std::uint64_t device_id = 0;
    std::uint64_t swapchain_id = 0;
    std::uint64_t generation = 0;
    DWORD creation_thread_id = 0;
    bool factory_identity_matches = false;
    void* device_identity = nullptr;
    void* swapchain_identity = nullptr;
    void** device_vtable = nullptr;
    void** swapchain_vtable = nullptr;
};

// Process-local diagnostic identities. This registry never retains COM references;
// successful CreateDevice observations replace an address-reused device entry with
// a new ID, while factories recovered through GetDirect3D resolve to the same
// canonical IUnknown identity.
class D3D9DiagnosticContextRegistry {
public:
    [[nodiscard]] FactoryDiagnosticContext RegisterCreatedFactory(
        IDirect3D9* factory) noexcept;
    [[nodiscard]] FactoryDiagnosticContext RegisterFactory(IDirect3D9* factory) noexcept;
    [[nodiscard]] std::optional<FactoryDiagnosticContext> FindFactory(
        IDirect3D9* factory) const noexcept;

    [[nodiscard]] DeviceDiagnosticContext RegisterDevice(
        IDirect3D9* observed_factory, IDirect3DDevice9* device) noexcept;
    [[nodiscard]] std::optional<DeviceDiagnosticContext> FindDevice(
        IDirect3DDevice9* device) const noexcept;
    [[nodiscard]] std::optional<DeviceDiagnosticContext> AdvanceGeneration(
        IDirect3DDevice9* device) noexcept;
    [[nodiscard]] std::vector<FactoryDiagnosticContext> RegisteredFactories() const noexcept;
    [[nodiscard]] std::vector<DeviceDiagnosticContext> RegisteredDevices() const noexcept;

private:
    [[nodiscard]] static void* CanonicalIdentity(IUnknown* object) noexcept;
    [[nodiscard]] static void** Vtable(void* object) noexcept;

    mutable std::mutex mutex_{};
    std::uint64_t next_factory_id_ = 1;
    std::uint64_t next_device_id_ = 1;
    std::uint64_t next_swapchain_id_ = 1;
    std::unordered_map<void*, FactoryDiagnosticContext> factories_{};
    std::unordered_map<void*, DeviceDiagnosticContext> devices_{};
    std::unordered_map<IDirect3DDevice9*, void*> device_interfaces_{};
};

} // namespace cojvr::backends::d3d9

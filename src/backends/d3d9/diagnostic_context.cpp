#include "backends/d3d9/diagnostic_context.hpp"

#include <wrl/client.h>

namespace cojvr::backends::d3d9 {

using Microsoft::WRL::ComPtr;

void* D3D9DiagnosticContextRegistry::CanonicalIdentity(IUnknown* object) noexcept {
    if (!object) return nullptr;
    IUnknown* identity = nullptr;
    if (FAILED(object->QueryInterface(IID_IUnknown, reinterpret_cast<void**>(&identity))) ||
        !identity) {
        return nullptr;
    }
    void* value = identity;
    identity->Release();
    return value;
}

void** D3D9DiagnosticContextRegistry::Vtable(void* object) noexcept {
    return object ? *reinterpret_cast<void***>(object) : nullptr;
}

FactoryDiagnosticContext D3D9DiagnosticContextRegistry::RegisterFactory(
    IDirect3D9* factory) noexcept {
    FactoryDiagnosticContext empty{};
    void* identity = CanonicalIdentity(factory);
    if (!identity) return empty;

    try {
        std::lock_guard lock(mutex_);
        if (const auto existing = factories_.find(identity); existing != factories_.end()) {
            return existing->second;
        }
        FactoryDiagnosticContext context{
            .factory_id = next_factory_id_++,
            .creation_thread_id = GetCurrentThreadId(),
            .identity = identity,
            .vtable = Vtable(factory),
        };
        factories_.emplace(identity, context);
        return context;
    } catch (...) {
        return empty;
    }
}

FactoryDiagnosticContext D3D9DiagnosticContextRegistry::RegisterCreatedFactory(
    IDirect3D9* factory) noexcept {
    FactoryDiagnosticContext empty{};
    void* identity = CanonicalIdentity(factory);
    if (!identity) return empty;
    try {
        std::lock_guard lock(mutex_);
        FactoryDiagnosticContext context{
            .factory_id = next_factory_id_++,
            .creation_thread_id = GetCurrentThreadId(),
            .identity = identity,
            .vtable = Vtable(factory),
        };
        factories_.insert_or_assign(identity, context);
        return context;
    } catch (...) {
        return empty;
    }
}

std::optional<FactoryDiagnosticContext> D3D9DiagnosticContextRegistry::FindFactory(
    IDirect3D9* factory) const noexcept {
    void* identity = CanonicalIdentity(factory);
    if (!identity) return std::nullopt;
    try {
        std::lock_guard lock(mutex_);
        const auto found = factories_.find(identity);
        return found == factories_.end()
            ? std::optional<FactoryDiagnosticContext>{}
            : std::optional<FactoryDiagnosticContext>{found->second};
    } catch (...) {
        return std::nullopt;
    }
}

DeviceDiagnosticContext D3D9DiagnosticContextRegistry::RegisterDevice(
    IDirect3D9* observed_factory, IDirect3DDevice9* device) noexcept {
    DeviceDiagnosticContext empty{};
    void* device_identity = CanonicalIdentity(device);
    if (!device_identity) return empty;

    const FactoryDiagnosticContext observed = RegisterFactory(observed_factory);
    ComPtr<IDirect3D9> recovered_factory;
    const bool recovered = SUCCEEDED(device->GetDirect3D(&recovered_factory)) && recovered_factory;
    const FactoryDiagnosticContext recovered_context = recovered
        ? RegisterFactory(recovered_factory.Get())
        : FactoryDiagnosticContext{};
    const bool factory_identity_matches = observed.factory_id != 0 &&
        observed.factory_id == recovered_context.factory_id;

    ComPtr<IDirect3DSwapChain9> swapchain;
    void* swapchain_identity = nullptr;
    void** swapchain_vtable = nullptr;
    if (SUCCEEDED(device->GetSwapChain(0, &swapchain)) && swapchain) {
        swapchain_identity = CanonicalIdentity(swapchain.Get());
        swapchain_vtable = Vtable(swapchain.Get());
    }

    try {
        std::lock_guard lock(mutex_);
        DeviceDiagnosticContext context{
            .factory_id = observed.factory_id,
            .device_id = next_device_id_++,
            .swapchain_id = swapchain_identity ? next_swapchain_id_++ : 0,
            .generation = 1,
            .creation_thread_id = GetCurrentThreadId(),
            .factory_identity_matches = factory_identity_matches,
            .device_identity = device_identity,
            .swapchain_identity = swapchain_identity,
            .device_vtable = Vtable(device),
            .swapchain_vtable = swapchain_vtable,
        };
        devices_.insert_or_assign(device_identity, context);
        device_interfaces_.insert_or_assign(device, device_identity);
        return context;
    } catch (...) {
        return empty;
    }
}

std::optional<DeviceDiagnosticContext> D3D9DiagnosticContextRegistry::FindDevice(
    IDirect3DDevice9* device) const noexcept {
    if (!device) return std::nullopt;
    try {
        std::lock_guard lock(mutex_);
        const auto interface_entry = device_interfaces_.find(device);
        if (interface_entry == device_interfaces_.end()) return std::nullopt;
        void* identity = interface_entry->second;
        const auto found = devices_.find(identity);
        return found == devices_.end()
            ? std::optional<DeviceDiagnosticContext>{}
            : std::optional<DeviceDiagnosticContext>{found->second};
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<DeviceDiagnosticContext> D3D9DiagnosticContextRegistry::AdvanceGeneration(
    IDirect3DDevice9* device) noexcept {
    if (!device) return std::nullopt;
    ComPtr<IDirect3DSwapChain9> swapchain;
    void* swapchain_identity = nullptr;
    void** swapchain_vtable = nullptr;
    if (SUCCEEDED(device->GetSwapChain(0, &swapchain)) && swapchain) {
        swapchain_identity = CanonicalIdentity(swapchain.Get());
        swapchain_vtable = Vtable(swapchain.Get());
    }
    try {
        std::lock_guard lock(mutex_);
        const auto interface_entry = device_interfaces_.find(device);
        if (interface_entry == device_interfaces_.end()) return std::nullopt;
        void* identity = interface_entry->second;
        const auto found = devices_.find(identity);
        if (found == devices_.end()) return std::nullopt;
        ++found->second.generation;
        found->second.device_vtable = Vtable(device);

        if (swapchain_identity) {
            const bool same_instance =
                found->second.swapchain_identity == swapchain_identity;
            found->second.swapchain_identity = swapchain_identity;
            found->second.swapchain_vtable = swapchain_vtable;
            if (!same_instance) {
                found->second.swapchain_id = next_swapchain_id_++;
            }
        } else {
            found->second.swapchain_identity = nullptr;
            found->second.swapchain_vtable = nullptr;
            found->second.swapchain_id = 0;
        }
        return found->second;
    } catch (...) {
        return std::nullopt;
    }
}

std::vector<FactoryDiagnosticContext>
D3D9DiagnosticContextRegistry::RegisteredFactories() const noexcept {
    try {
        std::lock_guard lock(mutex_);
        std::vector<FactoryDiagnosticContext> result;
        result.reserve(factories_.size());
        for (const auto& [identity, context] : factories_) {
            (void)identity;
            result.push_back(context);
        }
        return result;
    } catch (...) {
        return {};
    }
}

std::vector<DeviceDiagnosticContext>
D3D9DiagnosticContextRegistry::RegisteredDevices() const noexcept {
    try {
        std::lock_guard lock(mutex_);
        std::vector<DeviceDiagnosticContext> result;
        result.reserve(devices_.size());
        for (const auto& [identity, context] : devices_) {
            (void)identity;
            result.push_back(context);
        }
        return result;
    } catch (...) {
        return {};
    }
}

} // namespace cojvr::backends::d3d9

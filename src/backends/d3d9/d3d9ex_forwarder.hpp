#pragma once

#include <d3d9.h>

namespace cojvr::backends::d3d9 {

// Forwarder that implements both IDirect3D9 and IDirect3D9Ex by delegating
// to a real IDirect3D9Ex. This allows the game to query either interface
// while we observe device creation through the Ex path.
class Direct3D9ExForwarder final : public IDirect3D9Ex {
public:
    explicit Direct3D9ExForwarder(IDirect3D9Ex* inner) noexcept : inner_(inner) {
        if (inner_) inner_->AddRef();
    }

    virtual ~Direct3D9ExForwarder() noexcept = default;

    // IUnknown
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** object) override {
        if (!object) return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_IDirect3D9 || riid == IID_IDirect3D9Ex) {
            *object = static_cast<IDirect3D9Ex*>(this);
            AddRef();
            return S_OK;
        }
        return inner_->QueryInterface(riid, object);
    }

    ULONG STDMETHODCALLTYPE AddRef() override {
        return ++references_;
    }

    ULONG STDMETHODCALLTYPE Release() override {
        const ULONG remaining = --references_;
        if (remaining == 0) {
            inner_->Release();
            delete this;
        }
        return remaining;
    }

    // IDirect3D9
    HRESULT STDMETHODCALLTYPE RegisterSoftwareDevice(void* initialize_function) override {
        return inner_->RegisterSoftwareDevice(initialize_function);
    }

    UINT STDMETHODCALLTYPE GetAdapterCount() override {
        return inner_->GetAdapterCount();
    }

    HRESULT STDMETHODCALLTYPE GetAdapterIdentifier(
        UINT adapter, DWORD flags, D3DADAPTER_IDENTIFIER9* identifier) override {
        return inner_->GetAdapterIdentifier(adapter, flags, identifier);
    }

    UINT STDMETHODCALLTYPE GetAdapterModeCount(UINT adapter, D3DFORMAT format) override {
        return inner_->GetAdapterModeCount(adapter, format);
    }

    HRESULT STDMETHODCALLTYPE EnumAdapterModes(
        UINT adapter, D3DFORMAT format, UINT mode, D3DDISPLAYMODE* display_mode) override {
        return inner_->EnumAdapterModes(adapter, format, mode, display_mode);
    }

    HRESULT STDMETHODCALLTYPE GetAdapterDisplayMode(UINT adapter, D3DDISPLAYMODE* mode) override {
        return inner_->GetAdapterDisplayMode(adapter, mode);
    }

    HRESULT STDMETHODCALLTYPE CheckDeviceType(
        UINT adapter, D3DDEVTYPE device_type, D3DFORMAT adapter_format,
        D3DFORMAT back_buffer_format, BOOL windowed) override {
        return inner_->CheckDeviceType(
            adapter, device_type, adapter_format, back_buffer_format, windowed);
    }

    HRESULT STDMETHODCALLTYPE CheckDeviceFormat(
        UINT adapter, D3DDEVTYPE device_type, D3DFORMAT adapter_format, DWORD usage,
        D3DRESOURCETYPE resource_type, D3DFORMAT check_format) override {
        return inner_->CheckDeviceFormat(
            adapter, device_type, adapter_format, usage, resource_type, check_format);
    }

    HRESULT STDMETHODCALLTYPE CheckDeviceMultiSampleType(
        UINT adapter, D3DDEVTYPE device_type, D3DFORMAT surface_format, BOOL windowed,
        D3DMULTISAMPLE_TYPE multi_sample_type, DWORD* quality_levels) override {
        return inner_->CheckDeviceMultiSampleType(
            adapter, device_type, surface_format, windowed, multi_sample_type, quality_levels);
    }

    HRESULT STDMETHODCALLTYPE CheckDepthStencilMatch(
        UINT adapter, D3DDEVTYPE device_type, D3DFORMAT adapter_format,
        D3DFORMAT render_target_format, D3DFORMAT depth_stencil_format) override {
        return inner_->CheckDepthStencilMatch(
            adapter, device_type, adapter_format, render_target_format, depth_stencil_format);
    }

    HRESULT STDMETHODCALLTYPE CheckDeviceFormatConversion(
        UINT adapter, D3DDEVTYPE device_type, D3DFORMAT source_format,
        D3DFORMAT target_format) override {
        return inner_->CheckDeviceFormatConversion(
            adapter, device_type, source_format, target_format);
    }

    HRESULT STDMETHODCALLTYPE GetDeviceCaps(UINT adapter, D3DDEVTYPE device_type, D3DCAPS9* caps) override {
        return inner_->GetDeviceCaps(adapter, device_type, caps);
    }

    HMONITOR STDMETHODCALLTYPE GetAdapterMonitor(UINT adapter) override {
        return inner_->GetAdapterMonitor(adapter);
    }

    HRESULT STDMETHODCALLTYPE CreateDevice(
        UINT adapter, D3DDEVTYPE device_type, HWND focus_window, DWORD behavior_flags,
        D3DPRESENT_PARAMETERS* presentation_parameters, IDirect3DDevice9** returned_device) override {
        if (!presentation_parameters || !returned_device) return D3DERR_INVALIDCALL;
        *returned_device = nullptr;

        D3DDISPLAYMODEEX fullscreen_mode{};
        D3DDISPLAYMODEEX* fullscreen_mode_ptr = nullptr;
        if (!presentation_parameters->Windowed) {
            fullscreen_mode.Size = sizeof(fullscreen_mode);
            fullscreen_mode.Width = presentation_parameters->BackBufferWidth;
            fullscreen_mode.Height = presentation_parameters->BackBufferHeight;
            fullscreen_mode.RefreshRate = presentation_parameters->FullScreen_RefreshRateInHz;
            fullscreen_mode.Format = presentation_parameters->BackBufferFormat;
            fullscreen_mode.ScanLineOrdering = D3DSCANLINEORDERING_UNKNOWN;
            fullscreen_mode_ptr = &fullscreen_mode;
        }

        IDirect3DDevice9Ex* ex_device = nullptr;
        const HRESULT result = inner_->CreateDeviceEx(
            adapter, device_type, focus_window, behavior_flags,
            presentation_parameters, fullscreen_mode_ptr, &ex_device);
        if (SUCCEEDED(result) && ex_device != nullptr) {
            *returned_device = static_cast<IDirect3DDevice9*>(ex_device);
        } else if (ex_device != nullptr) {
            ex_device->Release();
        }
        return result;
    }

    // IDirect3D9Ex
    UINT STDMETHODCALLTYPE GetAdapterModeCountEx(UINT adapter, CONST D3DDISPLAYMODEFILTER* filter) {
        return inner_->GetAdapterModeCountEx(adapter, filter);
    }

    HRESULT STDMETHODCALLTYPE EnumAdapterModesEx(
        UINT adapter, CONST D3DDISPLAYMODEFILTER* filter, UINT mode, D3DDISPLAYMODEEX* display_mode) {
        return inner_->EnumAdapterModesEx(adapter, filter, mode, display_mode);
    }

    HRESULT STDMETHODCALLTYPE GetAdapterDisplayModeEx(UINT adapter, D3DDISPLAYMODEEX* mode, D3DDISPLAYROTATION* rotation) {
        return inner_->GetAdapterDisplayModeEx(adapter, mode, rotation);
    }

    HRESULT STDMETHODCALLTYPE GetAdapterLUID(UINT adapter, LUID* pLUID) {
        return inner_->GetAdapterLUID(adapter, pLUID);
    }

    HRESULT STDMETHODCALLTYPE CreateDeviceEx(
        UINT adapter, D3DDEVTYPE device_type, HWND focus_window, DWORD behavior_flags,
        D3DPRESENT_PARAMETERS* presentation_parameters, D3DDISPLAYMODEEX* fullscreen_mode,
        IDirect3DDevice9Ex** returned_device) {
        if (!presentation_parameters || !returned_device) return D3DERR_INVALIDCALL;
        *returned_device = nullptr;
        const HRESULT result = inner_->CreateDeviceEx(
            adapter, device_type, focus_window, behavior_flags,
            presentation_parameters, fullscreen_mode, returned_device);
        return result;
    }

private:
    std::atomic<ULONG> references_{1};
    IDirect3D9Ex* inner_ = nullptr;
};

} // namespace cojvr::backends::d3d9
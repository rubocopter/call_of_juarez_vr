#pragma once
#include <d3d11.h>
#include <memory>
#include <string>
#include "NeuralTracking.h"
#include "NeuralSettings.h"

// Final-eye experimental Neural pass. Owns private images; a failure never
// publishes a partially processed pair to the OpenXR swapchain.
class NeuralPass {
public:
    using Options=NeuralOptions;
    NeuralPass(); ~NeuralPass();
    NeuralPass(const NeuralPass&)=delete;
    NeuralPass& operator=(const NeuralPass&)=delete;
    bool Initialize(ID3D11Device*,const Options&,const std::wstring& runtimeDirectory,int diagnosticMode=0);
    bool Process(ID3D11DeviceContext*,ID3D11Texture2D*,const Transport::Tracking&,uint64_t sourceFrame,DXGI_FORMAT viewFormat=DXGI_FORMAT_UNKNOWN);
    void Suspend();
    void Reset();
    uint64_t Runs() const;
    double GpuMs() const;
private:
    struct State; std::unique_ptr<State> state;
};

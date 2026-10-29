// nr-bridge "direct" mode: DLSS 5 Neural Rendering evaluated by this layer itself on a private
// D3D12 device, with motion vectors computed from the head rotation between frames. No ReShade, no
// RenoDX add-on, no hidden swap chain, nothing on the desktop mirror.
//
//   game D3D11 device:  eye images --copy--> sharedColor            sharedOut --copy--> eye images
//                                              || fence AB                 /\ fence BA
//   our D3D12 device:   sharedColor -> colorFull -> [downsample] -> colorWork
//                       poses -> CS_MotionVectors -> mv (RG16F, pixels)
//                       NGX feature 18 (colorWork, depth const, mv) -> outWork
//                       CS_Resolve: outFull = colorFull + up(outWork - colorWork)   (or outWork when 1:1)
//                       outFull --copy--> sharedOut
//
// The NGX contract (feature id 18, the driver core's capability parameter block, tuning written at
// create time, vtable slots for the setters) follows the open-source OptiScaler DLSS-NR fork.
// Author: LowPopLabs
#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11_4.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <string>
#include <vector>
#include <algorithm>
#include "ngx/nvsdk_ngx_defs.h"

void BridgeLog(const char *fmt, ...);
using Microsoft::WRL::ComPtr;

struct DirectView
{
    ID3D11Texture2D *texture; UINT arrayIndex; UINT left, top, right, bottom;
    // dynamic viewport support: the largest rect this view can have (0 = the rect itself). The work textures and the
    // model are sized from these, so a per-frame change of the rect (SteamVR adaptive resolution) costs no rebuild.
    UINT maxW = 0, maxH = 0;
    bool flipY = false;  // the image is stored upside down (OpenVR bounds with vMin > vMax): sample and write back mirrored
    float q[4];          // orientation quaternion x,y,z,w (OpenXR, view space -> world)
    float p[3];          // position (unused by rotation-only vectors, logged only)
    float fovL, fovR, fovU, fovD; // XrFovf angles in radians (left/down negative)
};

struct DirectConfig
{
    float scale = 1.0f;            // NR work resolution fraction of the eye pair
    float fovea = 1.0f;            // fraction of each eye (width and height) around the centre that the model sees; 1 = whole eye
    float feather = 400.0f;        // blend width in full-res pixels inside the edge of the fovea region
    int fovea_shape = 1;           // 0 rectangle, 1 ellipse (inscribed in the fovea rectangle)
    float outer_scale = 0.0f;      // > 0: a single cheap pass over the WHOLE eye pair at this scale supplies lighting/tone outside the fovea
    bool outer_pack = true;        // pack the outer tier next to the fovea crops in ONE work texture so each pass is a single model evaluate
    float viewport_ref = 1.0f;     // dynamic-viewport games: the model is sized for this fraction of the largest eye rect (1 = full size, constant cost;
                                   // lower = cheaper, matching where the game's adaptive resolution actually settles; the log line reports that)
    bool residual = true;          // when scale < 1: transfer only the model's edit onto the full-res frame
    bool mv = true;                // rotation motion vectors on/off (off = zero vectors)
    float mv_sign = 1.0f;          // flip if the convention turns out inverted
    float mv_scale = 1.0f;
    float depth_value = 0.5f;      // constant depth handed to the model (no depth from Unity)
    // model tuning (names match the RenoDX panel)
    float intensity = 1.0f, local_structure = 1.0f, local_tone = 1.0f, skin_structure = -1.0f;
    unsigned style = 0, preset = 0; bool auto_mask = true;
    int passes = 1;                // model passes per frame (each pass is its own model instance fed the previous pass's output)
    int ngx = 0;                   // 0 auto (core, then snippet), 1 core only, 2 snippet only, 3 none (debug passthrough), 4 half (debug: edit = input * 0.5), 5 tophalf (debug: upright top half * 0.5)
    unsigned long long app_id = 0x24480451ull;
    int log_frames = 3;
};

class DirectNR
{
public:
    bool Init(ID3D11Device *devA, const DirectConfig &cfg, const std::wstring &gameDir, const std::wstring &ownDir);
    bool Process(const DirectView *views, uint32_t count);
    // Applies a changed configuration at the next frame: geometry changes rebuild the textures, tuning
    // changes recreate the model instances, motion/depth settings apply immediately.
    void Reconfigure(const DirectConfig &cfg);
    void RequestReset() { reset_ = true; }
    void MarkFailed(const char *why) { if (!failed_) BridgeLog("direct: DISABLED: %s", why); failed_ = true; }
    bool failed() const { return failed_; }
    uint64_t frames() const { return frames_; }
    double avgCpuMs() const { return frames_ ? totalMs_ / (double)frames_ : 0.0; }
    double avgGpuMs() const { return gpuSamples_ ? gpuTotalMs_ / (double)gpuSamples_ : 0.0; }
    double avgModelMs() const { return gpuSamples_ ? gpuModelMs_ / (double)gpuSamples_ : 0.0; }
    void Shutdown(bool leak);

private:
    // --- NGX plumbing ---
    typedef NVSDK_NGX_Result (NVSDK_CONV *PFN_Init_Ext)(unsigned long long, const wchar_t *, ID3D12Device *, NVSDK_NGX_Version, const NVSDK_NGX_FeatureCommonInfo *);
    typedef NVSDK_NGX_Result (NVSDK_CONV *PFN_Shutdown1)(ID3D12Device *);
    typedef NVSDK_NGX_Result (NVSDK_CONV *PFN_GetCaps)(void **);
    typedef NVSDK_NGX_Result (NVSDK_CONV *PFN_Create)(ID3D12GraphicsCommandList *, NVSDK_NGX_Feature, void *, NVSDK_NGX_Handle **);
    typedef NVSDK_NGX_Result (NVSDK_CONV *PFN_Evaluate)(ID3D12GraphicsCommandList *, const NVSDK_NGX_Handle *, void *, void *);
    typedef NVSDK_NGX_Result (NVSDK_CONV *PFN_Release)(NVSDK_NGX_Handle *);
    // forwarder exports (nvngx.dll_nr_bridge.dll)
    typedef int (__cdecl *PFN_SnipLoad)(const wchar_t *);
    typedef int (__cdecl *PFN_SnipInit)(unsigned long long, const wchar_t *, ID3D12Device *, int, const void *);
    typedef int (__cdecl *PFN_SnipCreate)(ID3D12GraphicsCommandList *, int, void *, void **);
    typedef int (__cdecl *PFN_SnipEvaluate)(ID3D12GraphicsCommandList *, void *, void *);
    typedef int (__cdecl *PFN_SnipRelease)(void *);

    bool CreateD3D12();
    bool CreateFences();
    bool CreateShaders();
    bool LoadNgx();
    bool EnsureSized(UINT W, UINT H, DXGI_FORMAT fmt);
    void ReleaseSized();
    bool CreateSharedPair(UINT W, UINT H, DXGI_FORMAT fmt);
    ComPtr<ID3D12Resource> CreateTex12(UINT w, UINT h, DXGI_FORMAT fmt, D3D12_RESOURCE_STATES state, bool uav, const wchar_t *name);
    bool CreateFeature(int pass);
    bool CreateFeatureSized(UINT w, UINT h, NVSDK_NGX_Handle **out, const char *tag);
    bool Evaluate(ID3D12GraphicsCommandList *cl, NVSDK_NGX_Handle *f, ID3D12Resource *in, ID3D12Resource *out, ID3D12Resource *mv, ID3D12Resource *depth, UINT w, UINT h, const char *tag);
    void ReleaseFeatures();
    bool Fail(const char *what, HRESULT hr);
    void Barrier(ID3D12GraphicsCommandList *cl, ID3D12Resource *r, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to);
    D3D12_CPU_DESCRIPTOR_HANDLE CpuSlot(UINT i) const { D3D12_CPU_DESCRIPTOR_HANDLE h = heapCpu_; h.ptr += (SIZE_T)i * descSize_; return h; }
    D3D12_GPU_DESCRIPTOR_HANDLE GpuSlot(UINT i) const { D3D12_GPU_DESCRIPTOR_HANDLE h = heapGpu_; h.ptr += (UINT64)i * descSize_; return h; }
    void MakeSrv(UINT slot, ID3D12Resource *r, DXGI_FORMAT f);
    void MakeUav(UINT slot, ID3D12Resource *r, DXGI_FORMAT f);
    void SetUInt(const char *n, unsigned v); void SetFloat(const char *n, float v); void SetRes(const char *n, ID3D12Resource *r);
    bool GetFloat(const char *n, float *v); bool GetUInt(const char *n, unsigned *v);
    void DiscoverFloatSlot();
    static const char *ResultName(unsigned r);
    static void NVSDK_CONV NgxLog(const char *message, NVSDK_NGX_Logging_Level level, NVSDK_NGX_Feature source);

    DirectConfig cfg_; std::wstring gameDir_, ownDir_;
    bool failed_ = false, scaled_ = false, resolve_ = false, reset_ = true;
    uint64_t frames_ = 0; double totalMs_ = 0, lastMs_ = 0; LARGE_INTEGER qpf_ = {}; LONGLONG lastFrameQpc_ = 0; double intervalTotalMs_ = 0; uint64_t intervalCount_ = 0;
    double viewportSum_ = 0; uint64_t viewportCount_ = 0; bool dynamicSeen_ = false;   // fraction of the largest eye rect actually in use (dynamic viewport)
    // GPU timing: 4 timestamps per ring slot (start, before model, after model, end)
    ComPtr<ID3D12QueryHeap> tsHeap_; ComPtr<ID3D12Resource> tsReadback_; UINT64 tsFreq_ = 0; bool tsPending_[3] = {}; double gpuTotalMs_ = 0, gpuModelMs_ = 0; uint64_t gpuSamples_ = 0;

    // D3D11 (game)
    ComPtr<ID3D11Device> devA_; ComPtr<ID3D11Device1> devA1_; ComPtr<ID3D11Device5> devA5_;
    ComPtr<ID3D11DeviceContext> ctxA_; ComPtr<ID3D11DeviceContext4> ctxA4_;
    ComPtr<ID3D11Texture2D> sharedColorA_, sharedOutA_;
    ComPtr<ID3D11Fence> fenceAB_A_, fenceBA_A_;
    // D3D12 (ours)
    ComPtr<ID3D12Device> dev_; ComPtr<ID3D12CommandQueue> queue_;
    static const UINT kFrames = 3;
    ComPtr<ID3D12CommandAllocator> alloc_[kFrames]; ComPtr<ID3D12GraphicsCommandList> list_;
    ComPtr<ID3D12Fence> ringFence_; UINT64 ringValue_[kFrames] = {}; UINT64 ringNext_ = 1; HANDLE ringEvent_ = nullptr; UINT frameSlot_ = 0;
    ComPtr<ID3D12Fence> fenceAB_, fenceBA_; UINT64 vIn_ = 0, vOut_ = 0;
    ComPtr<ID3D12Resource> sharedColor_, sharedOut_;
    ComPtr<ID3D12Resource> colorFull_, colorWork_, mv_, depth_, outWork_, outWork2_, outFull_;
    ComPtr<ID3D12Resource> colorOuter_, mvOuter_, depthOuter_, outOuter_; UINT outerW_ = 0, outerH_ = 0; bool outerDepthFilled_ = false; NVSDK_NGX_Handle *featureOuter_ = nullptr;
    bool packed_ = false; UINT foveaW_ = 0;   // packed: the work texture is an atlas [fovea crops | outer eye pair]; foveaW_ = width of the fovea block
    ComPtr<ID3D12DescriptorHeap> heap_; D3D12_CPU_DESCRIPTOR_HANDLE heapCpu_ = {}; D3D12_GPU_DESCRIPTOR_HANDLE heapGpu_ = {}; UINT descSize_ = 0;
    ComPtr<ID3D12Resource> cb_; uint8_t *cbMapped_ = nullptr; static const UINT kCbStride = 1024, kCbPerFrame = 2; // per-frame constants (two tiers), upload heap, persistently mapped
    static const UINT kDescPerFrame = 64;   // 0-23 the real stages, 24-63 the ngx=4 debug model (8 per evaluate: 4 passes + outer)
    ComPtr<ID3D12RootSignature> rootSig_; ComPtr<ID3D12PipelineState> psoDown_, psoMv_, psoFill_, psoResolve_, psoHalf_, psoTopHalf_;
    void DebugHalf(ID3D12GraphicsCommandList *cl, UINT slot, ID3D12Resource *in, ID3D12Resource *out, ID3D12Resource *mv, ID3D12Resource *depth, UINT gx, UINT gy);
    // copies the top-left w x h block of src into dst (both at 0,0), moving each through the copy state and leaving it in its `to` state
    void CopyBlock(ID3D12GraphicsCommandList *cl, ID3D12Resource *src, D3D12_RESOURCE_STATES srcFrom, D3D12_RESOURCE_STATES srcTo, ID3D12Resource *dst, D3D12_RESOURCE_STATES dstFrom, D3D12_RESOURCE_STATES dstTo, UINT w, UINT h);
    // packed multi-pass: fovea-only buffers for the passes after the first (fovea block size)
    ComPtr<ID3D12Resource> foveaA_, foveaB_, mvF_, depthF_; UINT foveaH_ = 0; bool depthFFilled_ = false;
    UINT W_ = 0, H_ = 0, workW_ = 0, workH_ = 0; DXGI_FORMAT fmt_ = DXGI_FORMAT_UNKNOWN; bool depthFilled_ = false;
    uint32_t viewCount_ = 0; UINT viewW_[4] = {}, viewH_[4] = {};
    // previous poses per view for motion vectors
    struct PosePrev { float q[4]; float fovL, fovR, fovU, fovD; bool valid; } prev_[8] = {};
    // NGX
    HMODULE core_ = nullptr, fwd_ = nullptr;
    PFN_Init_Ext coreInit_ = nullptr; PFN_Shutdown1 coreShutdown_ = nullptr; PFN_GetCaps coreGetCaps_ = nullptr;
    PFN_Create coreCreate_ = nullptr; PFN_Evaluate coreEvaluate_ = nullptr; PFN_Release coreRelease_ = nullptr;
    PFN_SnipLoad snipLoad_ = nullptr; PFN_SnipInit snipInit_ = nullptr; PFN_SnipCreate snipCreate_ = nullptr; PFN_SnipEvaluate snipEvaluate_ = nullptr; PFN_SnipRelease snipRelease_ = nullptr;
    bool coreInited_ = false, snipInited_ = false; int featurePath_ = 0; // 1 core, 2 snippet (same for every pass)
    void *caps_ = nullptr; NVSDK_NGX_Handle *features_[4] = {}; int floatSlot_ = -1;
    std::vector<std::wstring> ngxPaths_; std::vector<const wchar_t *> ngxPathPtrs_; NVSDK_NGX_FeatureCommonInfo fcInfo_ = {};
};

// ================================================================================================

inline const char *DirectNR::ResultName(unsigned r)
{
    switch (r)
    {
    case 1: return "Success";
    case 0xBAD00000: return "Fail"; case 0xBAD00001: return "FeatureNotSupported"; case 0xBAD00002: return "PlatformError";
    case 0xBAD00003: return "FeatureAlreadyExists"; case 0xBAD00004: return "FeatureNotFound"; case 0xBAD00005: return "InvalidParameter";
    case 0xBAD00006: return "ScratchBufferTooSmall"; case 0xBAD00007: return "NotInitialized"; case 0xBAD00008: return "UnsupportedInputFormat";
    case 0xBAD00009: return "RWFlagMissing"; case 0xBAD0000A: return "MissingInput"; case 0xBAD0000B: return "UnableToInitializeFeature";
    case 0xBAD0000C: return "OutOfDate"; case 0xBAD0000D: return "OutOfGPUMemory"; case 0xBAD0000E: return "UnsupportedFormat";
    case 0xBAD0000F: return "UnableToWriteToAppDataPath"; case 0xBAD00010: return "UnsupportedParameter"; case 0xBAD00011: return "Denied";
    case 0xBAD00012: return "NotImplemented"; default: return "?";
    }
}

inline void NVSDK_CONV DirectNR::NgxLog(const char *message, NVSDK_NGX_Logging_Level, NVSDK_NGX_Feature)
{
    if (!message) return;
    std::string s(message); while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
    if (s.empty()) return;
    // the core logs one "NGXLoadFromPath failed" per feature DLL it does not find in the path list; keep a few
    static int pathFails = 0, total = 0;
    if (s.find("NGXLoadFromPath failed") != std::string::npos) { if (++pathFails > 3) return; if (pathFails == 3) { BridgeLog("ngx: (further NGXLoadFromPath messages suppressed; they list feature DLLs absent from the search paths)"); return; } }
    // the core dumps its whole feature config at init; keep only what can explain a failure
    std::string low = s; for (auto &ch : low) ch = (char)tolower((unsigned char)ch);
    const bool keep = low.find("error") != std::string::npos || low.find("fail") != std::string::npos || low.find("warn") != std::string::npos ||
                      low.find("not supported") != std::string::npos || low.find("feature") != std::string::npos || low.find("snippet") != std::string::npos ||
                      low.find("dlssnr") != std::string::npos || low.find("called from module") != std::string::npos || low.find("driverstore") != std::string::npos;
    if (!keep || ++total > 300) return;
    BridgeLog("ngx: %s", s.c_str());
}

inline bool DirectNR::Fail(const char *what, HRESULT hr)
{
    char b[256]; snprintf(b, sizeof(b), "%s failed, hr=0x%08lX", what, (unsigned long)hr); MarkFailed(b); return false;
}

inline void DirectNR::Barrier(ID3D12GraphicsCommandList *cl, ID3D12Resource *r, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to)
{
    if (from == to) return;
    D3D12_RESOURCE_BARRIER b = {}; b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition.pResource = r; b.Transition.StateBefore = from; b.Transition.StateAfter = to; b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    cl->ResourceBarrier(1, &b);
}

// ---- parameter block access through the vtable (driver core block; see OptiScaler notes) ----
typedef void (__thiscall *BNR_PFN_SetULL)(void *, const char *, unsigned long long);
typedef void (__thiscall *BNR_PFN_SetFloat)(void *, const char *, float);
typedef void (__thiscall *BNR_PFN_SetUInt)(void *, const char *, unsigned int);
typedef NVSDK_NGX_Result (__thiscall *BNR_PFN_GetFloat)(void *, const char *, float *);
typedef NVSDK_NGX_Result (__thiscall *BNR_PFN_GetUInt)(void *, const char *, unsigned int *);

// NVIDIA's header declares 8 Set overloads then 8 Get overloads (ULL, float, double, uint, int, d3d11, d3d12, void*).
// MSVC lays same-name overloads out in reverse declaration order, so in the driver's block the ULL setter is
// slot 7, float 6, uint 4, void* 0; the getters follow at 8..15 in the same reversed order (float 14, uint 12).
// Resources go through slot 0 (the 64-bit void* setter) as the OptiScaler fork established. The slots for
// float and uint are confirmed by round-tripping a value at start-up and logged.
static int g_bnrUIntSlot = 4, g_bnrGetFloatSlot = 14, g_bnrGetUIntSlot = 12;
inline void DirectNR::SetUInt(const char *n, unsigned v) { void **vt = *reinterpret_cast<void ***>(caps_); reinterpret_cast<BNR_PFN_SetUInt>(vt[g_bnrUIntSlot])(caps_, n, v); }
inline void DirectNR::SetRes(const char *n, ID3D12Resource *r) { void **vt = *reinterpret_cast<void ***>(caps_); reinterpret_cast<BNR_PFN_SetULL>(vt[0])(caps_, n, (unsigned long long)r); }
inline void DirectNR::SetFloat(const char *n, float v) { void **vt = *reinterpret_cast<void ***>(caps_); reinterpret_cast<BNR_PFN_SetFloat>(vt[floatSlot_ < 0 ? 6 : floatSlot_])(caps_, n, v); }
inline bool DirectNR::GetFloat(const char *n, float *v) { void **vt = *reinterpret_cast<void ***>(caps_); return reinterpret_cast<BNR_PFN_GetFloat>(vt[g_bnrGetFloatSlot])(caps_, n, v) == NVSDK_NGX_Result_Success; }
inline bool DirectNR::GetUInt(const char *n, unsigned *v) { void **vt = *reinterpret_cast<void ***>(caps_); return reinterpret_cast<BNR_PFN_GetUInt>(vt[g_bnrGetUIntSlot])(caps_, n, v) == NVSDK_NGX_Result_Success; }

inline void DirectNR::DiscoverFloatSlot()
{
    if (floatSlot_ >= 0) return;
    void **vt = *reinterpret_cast<void ***>(caps_);
    const float probe = 0.3125f;
    // float: try the expected getter (14) and the header-order getter (9) against every setter slot
    for (int g : { 14, 9 })
    {
        for (int slot : { 6, 1, 0, 2, 3, 4, 5, 7 })
        {
            float back = 0.0f;
            reinterpret_cast<BNR_PFN_SetFloat>(vt[slot])(caps_, "DLSSNR.ProbeF", probe);
            if (reinterpret_cast<BNR_PFN_GetFloat>(vt[g])(caps_, "DLSSNR.ProbeF", &back) == NVSDK_NGX_Result_Success && back == probe)
            { floatSlot_ = slot; g_bnrGetFloatSlot = g; break; }
        }
        if (floatSlot_ >= 0) break;
    }
    // uint: same idea, expected getter 12 (or 11 in header order)
    bool uintOk = false;
    for (int g : { 12, 11 })
    {
        for (int slot : { 4, 3 })
        {
            unsigned back = 0;
            reinterpret_cast<BNR_PFN_SetUInt>(vt[slot])(caps_, "DLSSNR.ProbeU", 0xC0FFEEu);
            if (reinterpret_cast<BNR_PFN_GetUInt>(vt[g])(caps_, "DLSSNR.ProbeU", &back) == NVSDK_NGX_Result_Success && back == 0xC0FFEEu)
            { g_bnrUIntSlot = slot; g_bnrGetUIntSlot = g; uintOk = true; break; }
        }
        if (uintOk) break;
    }
    if (floatSlot_ < 0) { floatSlot_ = 6; BridgeLog("direct: WARNING float parameter slot could not be confirmed, assuming 6 (tuning may not apply)"); }
    BridgeLog("direct: parameter block slots: float set %d/get %d (%s), uint set %d/get %d (%s)", floatSlot_, g_bnrGetFloatSlot, "confirmed", g_bnrUIntSlot, g_bnrGetUIntSlot, uintOk ? "confirmed" : "ASSUMED");
}

// ================================================================================================

inline bool DirectNR::Init(ID3D11Device *devA, const DirectConfig &cfg, const std::wstring &gameDir, const std::wstring &ownDir)
{
    cfg_ = cfg; gameDir_ = gameDir; ownDir_ = ownDir;
    cfg_.scale = std::min(1.0f, std::max(0.25f, cfg_.scale)); scaled_ = cfg_.scale < 0.999f;
    cfg_.fovea = std::min(1.0f, std::max(0.2f, cfg_.fovea)); resolve_ = scaled_ || cfg_.fovea < 0.999f;
    if (cfg_.fovea >= 0.999f) { cfg_.feather = 0.0f; cfg_.outer_scale = 0.0f; cfg_.fovea_shape = 0; }   // whole eye: no tiers, and no ellipse (it would drop the corners)
    if (cfg_.outer_scale > 0.0f) cfg_.outer_scale = std::min(1.0f, std::max(0.1f, cfg_.outer_scale));
    cfg_.passes = std::min(4, std::max(1, cfg_.passes));
    QueryPerformanceFrequency(&qpf_);
    devA_ = devA; devA_->GetImmediateContext(&ctxA_);
    if (FAILED(devA_.As(&devA1_)) || FAILED(devA_.As(&devA5_)) || FAILED(ctxA_.As(&ctxA4_))) return Fail("device A D3D11.4 interfaces", E_NOINTERFACE);
    if (!CreateD3D12() || !CreateFences() || !CreateShaders()) return false;
    if (cfg_.ngx < 3 && !LoadNgx()) return false;
    BridgeLog("direct: init ok. scale=%.2f fovea=%.2f (%s, feather %.0f) outer_scale=%.2f (%s) residual=%d passes=%d mv=%d (sign %+.0f x%.2f) ngx=%d style=%u preset=%u intensity=%.2f struct=%.2f tone=%.2f skin=%.2f automask=%d",
              cfg_.scale, cfg_.fovea, cfg_.fovea_shape ? "ellipse" : "rect", cfg_.feather, cfg_.outer_scale, cfg_.outer_pack ? "packed" : "separate", cfg_.residual, cfg_.passes, cfg_.mv, cfg_.mv_sign, cfg_.mv_scale, cfg_.ngx, cfg_.style, cfg_.preset, cfg_.intensity, cfg_.local_structure, cfg_.local_tone, cfg_.skin_structure, cfg_.auto_mask);
    return true;
}

inline void DirectNR::Reconfigure(const DirectConfig &in)
{
    DirectConfig n = in;
    n.scale = std::min(1.0f, std::max(0.25f, n.scale)); n.fovea = std::min(1.0f, std::max(0.2f, n.fovea)); n.passes = std::min(4, std::max(1, n.passes));
    if (n.fovea >= 0.999f) { n.feather = 0.0f; n.outer_scale = 0.0f; n.fovea_shape = 0; }
    if (n.outer_scale > 0.0f) n.outer_scale = std::min(1.0f, std::max(0.1f, n.outer_scale));
    const bool geometry = n.scale != cfg_.scale || n.fovea != cfg_.fovea || n.feather != cfg_.feather || n.residual != cfg_.residual || n.passes != cfg_.passes ||
                          n.fovea_shape != cfg_.fovea_shape || n.outer_scale != cfg_.outer_scale || n.outer_pack != cfg_.outer_pack || n.viewport_ref != cfg_.viewport_ref;
    const bool tuning = n.intensity != cfg_.intensity || n.local_structure != cfg_.local_structure || n.local_tone != cfg_.local_tone || n.skin_structure != cfg_.skin_structure ||
                        n.style != cfg_.style || n.preset != cfg_.preset || n.auto_mask != cfg_.auto_mask;
    const bool motion = n.mv != cfg_.mv || n.mv_sign != cfg_.mv_sign || n.mv_scale != cfg_.mv_scale || n.depth_value != cfg_.depth_value;
    const int ngxOld = cfg_.ngx; const unsigned long long appOld = cfg_.app_id; const int logOld = cfg_.log_frames;
    cfg_ = n; cfg_.ngx = ngxOld; cfg_.app_id = appOld; cfg_.log_frames = logOld; // paths and NGX init are fixed for the session
    scaled_ = cfg_.scale < 0.999f; resolve_ = scaled_ || cfg_.fovea < 0.999f;
    if (geometry) { W_ = 0; depthFilled_ = false; }   // EnsureSized rebuilds everything at the next frame
    else if (tuning) { ReleaseFeatures(); }            // model instances recreated with the new create-time values
    if (geometry || tuning || motion) reset_ = true;
    if (failed_ && (geometry || tuning)) { failed_ = false; BridgeLog("direct: re-enabled after reconfigure"); }
    BridgeLog("direct: reconfigured (%s%s%s) scale=%.2f fovea=%.2f (%s, feather %.0f) outer_scale=%.2f (%s) residual=%d passes=%d mv=%d (sign %+.0f x%.2f) depth=%.2f style=%u preset=%u intensity=%.2f struct=%.2f tone=%.2f skin=%.2f automask=%d",
              geometry ? "geometry " : "", tuning ? "tuning " : "", motion ? "motion " : "", cfg_.scale, cfg_.fovea, cfg_.fovea_shape ? "ellipse" : "rect", cfg_.feather, cfg_.outer_scale, cfg_.outer_pack ? "packed" : "separate", cfg_.residual, cfg_.passes, cfg_.mv, cfg_.mv_sign, cfg_.mv_scale, cfg_.depth_value,
              cfg_.style, cfg_.preset, cfg_.intensity, cfg_.local_structure, cfg_.local_tone, cfg_.skin_structure, cfg_.auto_mask);
}

inline bool DirectNR::CreateD3D12()
{
    ComPtr<IDXGIDevice> dxgiA; ComPtr<IDXGIAdapter> adapter;
    if (FAILED(devA_.As(&dxgiA)) || FAILED(dxgiA->GetAdapter(&adapter))) return Fail("adapter of device A", E_FAIL);
    DXGI_ADAPTER_DESC ad = {}; adapter->GetDesc(&ad);
    HRESULT hr = D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&dev_));
    if (FAILED(hr)) return Fail("D3D12CreateDevice", hr);
    D3D12_COMMAND_QUEUE_DESC qd = {}; qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    if (FAILED(hr = dev_->CreateCommandQueue(&qd, IID_PPV_ARGS(&queue_)))) return Fail("CreateCommandQueue", hr);
    for (UINT i = 0; i < kFrames; ++i) if (FAILED(hr = dev_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&alloc_[i])))) return Fail("CreateCommandAllocator", hr);
    if (FAILED(hr = dev_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc_[0].Get(), nullptr, IID_PPV_ARGS(&list_)))) return Fail("CreateCommandList", hr);
    list_->Close();
    if (FAILED(hr = dev_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&ringFence_)))) return Fail("CreateFence ring", hr);
    ringEvent_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    D3D12_DESCRIPTOR_HEAP_DESC hd = {}; hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV; hd.NumDescriptors = kFrames * kDescPerFrame; hd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (FAILED(hr = dev_->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&heap_)))) return Fail("CreateDescriptorHeap", hr);
    heapCpu_ = heap_->GetCPUDescriptorHandleForHeapStart(); heapGpu_ = heap_->GetGPUDescriptorHandleForHeapStart();
    descSize_ = dev_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    D3D12_HEAP_PROPERTIES up = {}; up.Type = D3D12_HEAP_TYPE_UPLOAD;
    D3D12_RESOURCE_DESC bd = {}; bd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; bd.Width = kCbStride * kCbPerFrame * kFrames; bd.Height = 1; bd.DepthOrArraySize = 1; bd.MipLevels = 1; bd.SampleDesc.Count = 1; bd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    if (FAILED(hr = dev_->CreateCommittedResource(&up, D3D12_HEAP_FLAG_NONE, &bd, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&cb_)))) return Fail("constants buffer", hr);
    D3D12_RANGE noRead = { 0, 0 };
    if (FAILED(hr = cb_->Map(0, &noRead, (void **)&cbMapped_))) return Fail("constants Map", hr);
    // GPU timestamps
    D3D12_QUERY_HEAP_DESC qh = {}; qh.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP; qh.Count = kFrames * 4;
    if (SUCCEEDED(dev_->CreateQueryHeap(&qh, IID_PPV_ARGS(&tsHeap_))))
    {
        D3D12_HEAP_PROPERTIES rb = {}; rb.Type = D3D12_HEAP_TYPE_READBACK;
        D3D12_RESOURCE_DESC rd = bd; rd.Width = kFrames * 4 * sizeof(UINT64);
        if (FAILED(dev_->CreateCommittedResource(&rb, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&tsReadback_)))) tsHeap_.Reset();
        if (FAILED(queue_->GetTimestampFrequency(&tsFreq_))) tsFreq_ = 0;
    }
    BridgeLog("direct: D3D12 device on '%ls' luid=%08lX%08lX", ad.Description, ad.AdapterLuid.HighPart, ad.AdapterLuid.LowPart);
    return true;
}

inline bool DirectNR::CreateFences()
{
    auto make = [&](ComPtr<ID3D12Fence> &f12, ComPtr<ID3D11Fence> &f11, const char *name) -> bool {
        HRESULT hr = dev_->CreateFence(0, D3D12_FENCE_FLAG_SHARED, IID_PPV_ARGS(&f12));
        if (FAILED(hr)) return Fail(name, hr);
        HANDLE h = nullptr; hr = dev_->CreateSharedHandle(f12.Get(), nullptr, GENERIC_ALL, nullptr, &h);
        if (FAILED(hr)) return Fail("fence CreateSharedHandle", hr);
        hr = devA5_->OpenSharedFence(h, IID_PPV_ARGS(&f11)); CloseHandle(h);
        if (FAILED(hr)) return Fail("D3D11 OpenSharedFence", hr);
        return true;
    };
    return make(fenceAB_, fenceAB_A_, "fence AB") && make(fenceBA_, fenceBA_A_, "fence BA");
}

// ---- compute shaders ----
static const char *kDirectShaders = R"HLSL(
cbuffer C : register(b0)
{
    uint2 workSize; uint2 fullSize;
    uint tier; uint viewCount; float depthValue; float mvScale;      // tier 0 = fovea texture, 1 = separate outer texture, 2 = packed atlas [fovea | outer]
    uint residual; float feather; uint shape; uint outerOn;          // shape 0 rect, 1 ellipse
    uint2 outerSize; uint2 allocSize;                                // allocSize = the full texture's allocation (fullSize = the part in use this frame)
    uint4 flip;                                                      // per view: 1 = the eye is stored upside down
    // per view (max 2 used by the layer, 4 reserved)
    float4 rectWork[4];   // this view's region in the fovea work texture: x0, y0, w, h
    float4 rectOuter[4];  // this view's region in the outer work texture: x0, y0, w, h
    float4 eyeFull[4];    // this view's whole eye rect in the full (side-by-side) texture: x0, y0, w, h
    float4 cropFull[4];   // the part of the eye the fovea tier sees, in full texture pixels: x0, y0, w, h
    float4 qCur[4];       // orientation x,y,z,w
    float4 qPrev[4];
    float4 tanCur[4];     // tanL, tanR, tanU, tanD
    float4 tanPrev[4];
};
Texture2D<float4> tColorFull  : register(t0);
Texture2D<float4> tColorWork  : register(t1);
Texture2D<float4> tOutWork    : register(t2);
Texture2D<float4> tColorOuter : register(t3);
Texture2D<float4> tOutOuter   : register(t4);
RWTexture2D<float4> uColor   : register(u0);
RWTexture2D<float2> uMv      : register(u1);
RWTexture2D<float>  uDepth   : register(u2);
SamplerState sLinear : register(s0);

float3 qrot(float4 q, float3 v) { return v + 2.0 * cross(q.xyz, cross(q.xyz, v) + q.w * v); }
float4 qconj(float4 q) { return float4(-q.xyz, q.w); }

// a region's destination rect in the tier texture and the source rect it maps from (t 0 = fovea crop, 1 = whole eye)
float4 DstRect(uint v, uint t) { return t != 0 ? rectOuter[v] : rectWork[v]; }
float4 SrcRect(uint v, uint t) { return t != 0 ? eyeFull[v] : cropFull[v]; }
uint2 DstSize() { return tier == 1 ? outerSize : workSize; }
bool Inside(float2 p, float4 r) { return p.x >= r.x && p.x < r.x + r.z && p.y >= r.y && p.y < r.y + r.w; }

// which view (return, -1 = none) and which tier (t) a destination pixel belongs to. In the packed atlas the
// fovea crops come first and the outer eye pair follows, so both sets of rects are searched.
int RegionOf(uint2 p, out uint t)
{
    t = tier == 1 ? 1u : 0u;
    [loop] for (uint v = 0; v < viewCount; ++v) if (Inside(p, DstRect(v, t))) return (int)v;
    if (tier == 2)
    {
        t = 1u;
        [loop] for (uint v2 = 0; v2 < viewCount; ++v2) if (Inside(p, rectOuter[v2])) return (int)v2;
    }
    return -1;
}
int ViewOfFull(uint2 p)
{
    [loop] for (uint v = 0; v < viewCount; ++v)
    {
        float4 r = eyeFull[v];
        if (p.x >= r.x && p.x < r.x + r.z && p.y >= r.y && p.y < r.y + r.w) return (int)v;
    }
    return -1;
}

[numthreads(8, 8, 1)]
void CS_Downsample(uint3 id : SV_DispatchThreadID)
{
    uint2 sz = DstSize();
    if (id.x >= sz.x || id.y >= sz.y) return;
    uint tr; int v = RegionOf(id.xy, tr);
    if (v < 0) { uColor[id.xy] = 0; return; }
    float4 d = DstRect(v, tr), s = SrcRect(v, tr);
    float2 t = (float2(id.xy) + 0.5 - d.xy) / d.zw;      // 0..1 within the source region
    if (flip[v] != 0) t.y = 1.0 - t.y;                     // upside-down source: the tier image is built upright
    float2 uv = (s.xy + t * s.zw) / float2(allocSize);
    uColor[id.xy] = tColorFull.SampleLevel(sLinear, uv, 0);
}

// debug stand-in (ngx=5): the top half of the tier image (upright) at half brightness, to check the orientation mapping
[numthreads(8, 8, 1)]
void CS_TopHalf(uint3 id : SV_DispatchThreadID)
{
    uint2 sz = DstSize();
    if (id.x >= sz.x || id.y >= sz.y) return;
    uColor[id.xy] = tColorFull[id.xy] * (id.y < sz.y / 2 ? 0.5 : 1.0);
}

// debug stand-in for the model (ngx=4): the "edit" is the input at half brightness, so a wrong region mapping shows up in the resolve
[numthreads(8, 8, 1)]
void CS_Half(uint3 id : SV_DispatchThreadID)
{
    uint2 sz = DstSize();
    if (id.x >= sz.x || id.y >= sz.y) return;
    uColor[id.xy] = tColorFull[id.xy] * 0.5;
}

[numthreads(8, 8, 1)]
void CS_Fill(uint3 id : SV_DispatchThreadID)
{
    uint2 sz = DstSize();
    if (id.x >= sz.x || id.y >= sz.y) return;
    uDepth[id.xy] = depthValue;
}

[numthreads(8, 8, 1)]
void CS_MotionVectors(uint3 id : SV_DispatchThreadID)
{
    uint2 sz = DstSize();
    if (id.x >= sz.x || id.y >= sz.y) return;
    float2 mv = 0;
    uint tr; int v = RegionOf(id.xy, tr);
    if (v >= 0)
    {
        float4 d = DstRect(v, tr), s = SrcRect(v, tr);
        // tier pixel -> position within the whole eye, in full-resolution pixels
        float2 t = (float2(id.xy) + 0.5 - d.xy) / d.zw;
        float2 pixEye = (s.xy - eyeFull[v].xy) + t * s.zw;
        float2 uvc = pixEye / eyeFull[v].zw;
        float tx = lerp(tanCur[v].x, tanCur[v].y, uvc.x);     // left .. right
        float ty = lerp(tanCur[v].z, tanCur[v].w, uvc.y);     // up .. down (image y grows downward)
        float3 dView = float3(tx, ty, -1.0);                  // OpenXR view space: -Z forward, +Y up
        float3 dWorld = qrot(qCur[v], dView);
        float3 dPrev = qrot(qconj(qPrev[v]), dWorld);
        if (dPrev.z < -1e-5)
        {
            float ptx = -dPrev.x / dPrev.z, pty = -dPrev.y / dPrev.z;
            float2 uvp = float2((ptx - tanPrev[v].x) / (tanPrev[v].y - tanPrev[v].x),
                                (pty - tanPrev[v].z) / (tanPrev[v].w - tanPrev[v].z));
            float2 prevPixEye = uvp * eyeFull[v].zw;
            float2 mvFull = prevPixEye - pixEye;                              // current -> previous, full-res pixels
            mv = mvFull * (d.zw / s.zw) * mvScale;                            // into tier pixels
        }
    }
    uMv[id.xy] = mv;
}

// weight of the fovea tier at a full-res pixel: 1 inside, fading to 0 over `feather` pixels inside the edge
float FoveaWeight(float2 p, float4 c)
{
    if (shape != 0)
    {
        float2 half = c.zw * 0.5, centre = c.xy + half;
        float r = length((p - centre) / half);                 // 1.0 on the inscribed ellipse
        float fr = feather / min(half.x, half.y);              // feather as a fraction of the radius
        return 1.0 - smoothstep(saturate(1.0 - fr), 1.0, r);
    }
    if (p.x < c.x || p.x >= c.x + c.z || p.y < c.y || p.y >= c.y + c.w) return 0.0;
    if (feather <= 0.0) return 1.0;
    float dx = min(p.x - c.x, c.x + c.z - p.x), dy = min(p.y - c.y, c.y + c.w - p.y);
    return smoothstep(0.0, feather, min(dx, dy));
}

[numthreads(8, 8, 1)]
void CS_Resolve(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= fullSize.x || id.y >= fullSize.y) return;
    float4 base = tColorFull[id.xy];
    int v = ViewOfFull(id.xy);
    if (v < 0) { uColor[id.xy] = base; return; }
    float2 p = float2(id.xy) + 0.5;
    float4 c = cropFull[v];
    // fovea tier
    float w = FoveaWeight(p, c);
    float4 editC = 0, proxyC = 0;
    if (w > 0.0)
    {
        float2 t = saturate((p - c.xy) / c.zw);
        if (flip[v] != 0) t.y = 1.0 - t.y;
        float2 uv = (rectWork[v].xy + t * rectWork[v].zw) / float2(workSize);
        editC = tOutWork.SampleLevel(sLinear, uv, 0);
        proxyC = tColorWork.SampleLevel(sLinear, uv, 0);
    }
    // outer tier (whole eye at low resolution), if enabled
    float4 editO = base, proxyO = base;
    if (outerOn != 0)
    {
        float4 e = eyeFull[v];
        float2 t = (p - e.xy) / e.zw;
        if (flip[v] != 0) t.y = 1.0 - t.y;
        float2 uv = (rectOuter[v].xy + t * rectOuter[v].zw) / float2(outerSize);
        editO = tOutOuter.SampleLevel(sLinear, uv, 0);
        proxyO = tColorOuter.SampleLevel(sLinear, uv, 0);
    }
    if (residual != 0)
    {
        float4 deltaO = outerOn != 0 ? (editO - proxyO) : 0;
        float4 delta = lerp(deltaO, editC - proxyC, w);
        uColor[id.xy] = saturate(base + delta);
    }
    else
    {
        float4 outer = outerOn != 0 ? editO : base;
        uColor[id.xy] = lerp(outer, editC, w);
    }
}
)HLSL";

struct DirectConstants
{
    UINT workW, workH, fullW, fullH; UINT tier, viewCount; float depthValue, mvScale; UINT residual; float feather; UINT shape, outerOn; UINT outerW, outerH, allocW, allocH; UINT flip[4];
    float rectWork[4][4]; float rectOuter[4][4]; float eyeFull[4][4]; float cropFull[4][4]; float qCur[4][4]; float qPrev[4][4]; float tanCur[4][4]; float tanPrev[4][4];
};

inline bool DirectNR::CreateShaders()
{
    // root: 0 = 32-bit constants (sizeof(DirectConstants)/4), 1 = SRV table t0..t2, 2 = UAV table u0..u2, static sampler
    D3D12_DESCRIPTOR_RANGE rs[2] = {};
    rs[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; rs[0].NumDescriptors = 5; rs[0].BaseShaderRegister = 0; rs[0].OffsetInDescriptorsFromTableStart = 0;
    rs[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV; rs[1].NumDescriptors = 3; rs[1].BaseShaderRegister = 0; rs[1].OffsetInDescriptorsFromTableStart = 0;
    D3D12_ROOT_PARAMETER rp[3] = {};
    rp[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; rp[0].Descriptor.ShaderRegister = 0; rp[0].Descriptor.RegisterSpace = 0; rp[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rp[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; rp[1].DescriptorTable.NumDescriptorRanges = 1; rp[1].DescriptorTable.pDescriptorRanges = &rs[0]; rp[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rp[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; rp[2].DescriptorTable.NumDescriptorRanges = 1; rp[2].DescriptorTable.pDescriptorRanges = &rs[1]; rp[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    D3D12_STATIC_SAMPLER_DESC ss = {}; ss.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR; ss.AddressU = ss.AddressV = ss.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP; ss.MaxLOD = D3D12_FLOAT32_MAX; ss.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    D3D12_ROOT_SIGNATURE_DESC rd = {}; rd.NumParameters = 3; rd.pParameters = rp; rd.NumStaticSamplers = 1; rd.pStaticSamplers = &ss;
    ComPtr<ID3DBlob> sig, err;
    HRESULT hr = D3D12SerializeRootSignature(&rd, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &err);
    if (FAILED(hr)) { if (err) BridgeLog("rootsig: %s", (const char *)err->GetBufferPointer()); return Fail("D3D12SerializeRootSignature", hr); }
    if (FAILED(hr = dev_->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(), IID_PPV_ARGS(&rootSig_)))) return Fail("CreateRootSignature", hr);
    auto make = [&](const char *entry, ComPtr<ID3D12PipelineState> &pso) -> bool {
        ComPtr<ID3DBlob> cs, e;
        HRESULT h2 = D3DCompile(kDirectShaders, strlen(kDirectShaders), "direct", nullptr, nullptr, entry, "cs_5_0", 0, 0, &cs, &e);
        if (FAILED(h2)) { if (e) BridgeLog("%s: %s", entry, (const char *)e->GetBufferPointer()); return Fail(entry, h2); }
        D3D12_COMPUTE_PIPELINE_STATE_DESC pd = {}; pd.pRootSignature = rootSig_.Get(); pd.CS.pShaderBytecode = cs->GetBufferPointer(); pd.CS.BytecodeLength = cs->GetBufferSize();
        if (FAILED(h2 = dev_->CreateComputePipelineState(&pd, IID_PPV_ARGS(&pso)))) return Fail("CreateComputePipelineState", h2);
        return true;
    };
    return make("CS_Downsample", psoDown_) && make("CS_MotionVectors", psoMv_) && make("CS_Fill", psoFill_) && make("CS_Resolve", psoResolve_) && make("CS_Half", psoHalf_) && make("CS_TopHalf", psoTopHalf_);
}

inline bool DirectNR::LoadNgx()
{
    // the driver's own NGX core
    core_ = GetModuleHandleW(L"_nvngx.dll");
    if (!core_)
    {
        wchar_t path[MAX_PATH] = {}; DWORD sz = sizeof(path); HKEY k = nullptr;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"System\\CurrentControlSet\\Services\\nvlddmkm\\NGXCore", 0, KEY_READ, &k) == ERROR_SUCCESS)
        { RegQueryValueExW(k, L"NGXPath", nullptr, nullptr, (LPBYTE)path, &sz); RegCloseKey(k); }
        if (path[0]) { std::wstring p = std::wstring(path) + L"\\_nvngx.dll"; core_ = LoadLibraryExW(p.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH); BridgeLog("direct: NGX core from registry: %ls -> %p", p.c_str(), core_); }
        if (!core_) { core_ = LoadLibraryW(L"nvngx.dll"); BridgeLog("direct: NGX core via nvngx.dll search -> %p", core_); }
    }
    else BridgeLog("direct: NGX core already loaded in process");
    if (!core_) { MarkFailed("NVIDIA NGX core (_nvngx.dll) not found"); return false; }
    coreInit_ = (PFN_Init_Ext)GetProcAddress(core_, "NVSDK_NGX_D3D12_Init_Ext");
    coreShutdown_ = (PFN_Shutdown1)GetProcAddress(core_, "NVSDK_NGX_D3D12_Shutdown1");
    coreGetCaps_ = (PFN_GetCaps)GetProcAddress(core_, "NVSDK_NGX_D3D12_GetCapabilityParameters");
    coreCreate_ = (PFN_Create)GetProcAddress(core_, "NVSDK_NGX_D3D12_CreateFeature");
    coreEvaluate_ = (PFN_Evaluate)GetProcAddress(core_, "NVSDK_NGX_D3D12_EvaluateFeature");
    coreRelease_ = (PFN_Release)GetProcAddress(core_, "NVSDK_NGX_D3D12_ReleaseFeature");
    if (!coreInit_ || !coreGetCaps_ || !coreCreate_ || !coreEvaluate_) { MarkFailed("NGX core lacks D3D12 exports"); return false; }

    ngxPaths_ = { gameDir_, ownDir_ };
    for (auto &p : ngxPaths_) ngxPathPtrs_.push_back(p.c_str());
    fcInfo_ = {}; fcInfo_.PathListInfo.Path = ngxPathPtrs_.data(); fcInfo_.PathListInfo.Length = (unsigned)ngxPathPtrs_.size();
    fcInfo_.LoggingInfo.LoggingCallback = &DirectNR::NgxLog; fcInfo_.LoggingInfo.MinimumLoggingLevel = NVSDK_NGX_LOGGING_LEVEL_ON; fcInfo_.LoggingInfo.DisableOtherLoggingSinks = true;
    const NVSDK_NGX_Result ir = coreInit_(cfg_.app_id, ownDir_.c_str(), dev_.Get(), (NVSDK_NGX_Version)0x15, &fcInfo_);
    BridgeLog("direct: core Init_Ext(app 0x%llx, sdk 0x15, paths [%ls ; %ls]) -> 0x%08X %s", cfg_.app_id, gameDir_.c_str(), ownDir_.c_str(), (unsigned)ir, ResultName((unsigned)ir));
    if (ir != NVSDK_NGX_Result_Success) { MarkFailed("NGX core init failed"); return false; }
    coreInited_ = true;
    const NVSDK_NGX_Result cr = coreGetCaps_(&caps_);
    BridgeLog("direct: GetCapabilityParameters -> 0x%08X %s, block=%p", (unsigned)cr, ResultName((unsigned)cr), caps_);
    if (cr != NVSDK_NGX_Result_Success || !caps_) { MarkFailed("no capability parameter block"); return false; }
    DiscoverFloatSlot();

    // the forwarder for the snippet path (optional)
    std::wstring fp = ownDir_ + L"\\nvngx.dll_nr_bridge.dll";
    fwd_ = LoadLibraryExW(fp.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!fwd_) { fp = ownDir_ + L"\\nvngx.dll_bonelab_nr.dll"; fwd_ = LoadLibraryExW(fp.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH); }   // the forwarder's name before the rename (standalone installs)
    if (fwd_)
    {
        snipLoad_ = (PFN_SnipLoad)GetProcAddress(fwd_, "bnr_snip_load"); snipInit_ = (PFN_SnipInit)GetProcAddress(fwd_, "bnr_snip_init");
        snipCreate_ = (PFN_SnipCreate)GetProcAddress(fwd_, "bnr_snip_create"); snipEvaluate_ = (PFN_SnipEvaluate)GetProcAddress(fwd_, "bnr_snip_evaluate");
        snipRelease_ = (PFN_SnipRelease)GetProcAddress(fwd_, "bnr_snip_release");
    }
    BridgeLog("direct: forwarder %ls -> %s", fp.c_str(), (fwd_ && snipLoad_ && snipInit_ && snipCreate_ && snipEvaluate_) ? "loaded" : "not available (snippet path disabled)");
    return true;
}

inline ComPtr<ID3D12Resource> DirectNR::CreateTex12(UINT w, UINT h, DXGI_FORMAT fmt, D3D12_RESOURCE_STATES state, bool uav, const wchar_t *name)
{
    D3D12_HEAP_PROPERTIES hp = {}; hp.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC d = {}; d.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D; d.Width = w; d.Height = h; d.DepthOrArraySize = 1; d.MipLevels = 1; d.Format = fmt; d.SampleDesc.Count = 1;
    d.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN; d.Flags = uav ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE;
    ComPtr<ID3D12Resource> r;
    HRESULT hr = dev_->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &d, state, nullptr, IID_PPV_ARGS(&r));
    if (FAILED(hr)) { Fail("CreateCommittedResource", hr); return nullptr; }
    r->SetName(name);
    return r;
}

inline bool DirectNR::CreateSharedPair(UINT W, UINT H, DXGI_FORMAT fmt)
{
    D3D11_TEXTURE2D_DESC d = {}; d.Width = W; d.Height = H; d.MipLevels = 1; d.ArraySize = 1; d.Format = fmt; d.SampleDesc.Count = 1;
    d.Usage = D3D11_USAGE_DEFAULT; d.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET; d.MiscFlags = D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED;
    auto make = [&](ComPtr<ID3D11Texture2D> &t11, ComPtr<ID3D12Resource> &t12, const char *name) -> bool {
        HRESULT hr = devA_->CreateTexture2D(&d, nullptr, &t11); if (FAILED(hr)) return Fail(name, hr);
        ComPtr<IDXGIResource1> r; HANDLE h = nullptr;
        if (FAILED(t11.As(&r))) return Fail("IDXGIResource1", E_NOINTERFACE);
        if (FAILED(hr = r->CreateSharedHandle(nullptr, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, nullptr, &h))) return Fail("CreateSharedHandle", hr);
        hr = dev_->OpenSharedHandle(h, IID_PPV_ARGS(&t12)); CloseHandle(h);
        if (FAILED(hr)) return Fail("D3D12 OpenSharedHandle(texture)", hr);
        return true;
    };
    return make(sharedColorA_, sharedColor_, "shared colour") && make(sharedOutA_, sharedOut_, "shared output");
}

inline void DirectNR::ReleaseFeatures()
{
    // let the GPU finish with them first
    if (queue_ && ringFence_) { const UINT64 v = ringNext_++; queue_->Signal(ringFence_.Get(), v); if (ringFence_->GetCompletedValue() < v) { ringFence_->SetEventOnCompletion(v, ringEvent_); WaitForSingleObject(ringEvent_, 5000); } }
    for (auto &f : features_)
    {
        if (!f) continue;
        if (featurePath_ == 1 && coreRelease_) coreRelease_(f);
        else if (featurePath_ == 2 && snipRelease_) snipRelease_(f);
        f = nullptr;
    }
    if (featureOuter_)
    {
        if (featurePath_ == 1 && coreRelease_) coreRelease_(featureOuter_);
        else if (featurePath_ == 2 && snipRelease_) snipRelease_(featureOuter_);
        featureOuter_ = nullptr;
    }
}

inline void DirectNR::ReleaseSized()
{
    ReleaseFeatures();
    colorFull_.Reset(); colorWork_.Reset(); mv_.Reset(); depth_.Reset(); outWork_.Reset(); outWork2_.Reset(); outFull_.Reset();
    colorOuter_.Reset(); mvOuter_.Reset(); depthOuter_.Reset(); outOuter_.Reset(); outerW_ = outerH_ = 0; outerDepthFilled_ = false; packed_ = false; foveaW_ = foveaH_ = 0;
    foveaA_.Reset(); foveaB_.Reset(); mvF_.Reset(); depthF_.Reset(); depthFFilled_ = false;
    sharedColor_.Reset(); sharedOut_.Reset(); sharedColorA_.Reset(); sharedOutA_.Reset();
    W_ = H_ = workW_ = workH_ = 0; fmt_ = DXGI_FORMAT_UNKNOWN; depthFilled_ = false;
    for (auto &p : prev_) p.valid = false;
}

inline bool DirectNR::EnsureSized(UINT W, UINT H, DXGI_FORMAT fmt)
{
    if (W == W_ && H == H_ && fmt == fmt_) return true;
    if (W_) BridgeLog("direct: layout changed %ux%u -> %ux%u, rebuilding", W_, H_, W, H);
    ReleaseSized();
    // the work texture holds each view's fovea crop side by side, scaled; sizes rounded to even numbers
    UINT ww = 0, wh = 0;
    // the model is sized for viewport_ref of the largest rect (the whole rect unless a dynamic-viewport game is tuned lower)
    const float vr = std::min(1.0f, std::max(0.25f, cfg_.viewport_ref));
    for (uint32_t i = 0; i < viewCount_; ++i) { ww += (UINT)(viewW_[i] * vr * cfg_.fovea * cfg_.scale) & ~1u; wh = std::max(wh, (UINT)(viewH_[i] * vr * cfg_.fovea * cfg_.scale) & ~1u); }
    ww = std::max<UINT>(16, ww); wh = std::max<UINT>(16, wh);
    if (!resolve_) { ww = W; wh = H; }
    // outer tier size (the whole eye pair at outer_scale); packed = it lives to the right of the fovea crops in the same texture
    UINT ow = 0, oh = 0;
    if (resolve_ && cfg_.outer_scale > 0.0f)
    {
        for (uint32_t i = 0; i < viewCount_; ++i) { ow += (UINT)(viewW_[i] * vr * cfg_.outer_scale) & ~1u; oh = std::max(oh, (UINT)(viewH_[i] * vr * cfg_.outer_scale) & ~1u); }
        ow = std::max<UINT>(16, ow); oh = std::max<UINT>(16, oh);
    }
    packed_ = ow != 0 && cfg_.outer_pack; foveaW_ = ww; foveaH_ = wh;
    if (packed_) { ww += ow; wh = std::max(wh, oh); }
    if (!CreateSharedPair(W, H, fmt)) return false;
    colorFull_ = CreateTex12(W, H, fmt, D3D12_RESOURCE_STATE_COPY_DEST, true, L"bnr colorFull");
    colorWork_ = resolve_ ? CreateTex12(ww, wh, fmt, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, true, L"bnr colorWork") : nullptr;
    mv_ = CreateTex12(ww, wh, DXGI_FORMAT_R16G16_FLOAT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, true, L"bnr mv");
    depth_ = CreateTex12(ww, wh, DXGI_FORMAT_R32_FLOAT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, true, L"bnr depth");
    outWork_ = CreateTex12(ww, wh, fmt, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, true, L"bnr outWork");
    outWork2_ = (cfg_.passes > 1 && !packed_) ? CreateTex12(ww, wh, fmt, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, true, L"bnr outWork2") : nullptr;
    if (packed_ && cfg_.passes > 1)
    {
        foveaA_ = CreateTex12(foveaW_, foveaH_, fmt, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, true, L"bnr foveaA");
        foveaB_ = CreateTex12(foveaW_, foveaH_, fmt, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, true, L"bnr foveaB");
        mvF_ = CreateTex12(foveaW_, foveaH_, DXGI_FORMAT_R16G16_FLOAT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, true, L"bnr mvF");
        depthF_ = CreateTex12(foveaW_, foveaH_, DXGI_FORMAT_R32_FLOAT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, true, L"bnr depthF");
    }
    outFull_ = resolve_ ? CreateTex12(W, H, fmt, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, true, L"bnr outFull") : nullptr;
    // separate outer tier: its own textures and its own one-pass model instance
    if (ow && !packed_)
    {
        colorOuter_ = CreateTex12(ow, oh, fmt, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, true, L"bnr colorOuter");
        mvOuter_ = CreateTex12(ow, oh, DXGI_FORMAT_R16G16_FLOAT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, true, L"bnr mvOuter");
        depthOuter_ = CreateTex12(ow, oh, DXGI_FORMAT_R32_FLOAT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, true, L"bnr depthOuter");
        outOuter_ = CreateTex12(ow, oh, fmt, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, true, L"bnr outOuter");
    }
    if (failed_) return false;
    W_ = W; H_ = H; workW_ = ww; workH_ = wh; outerW_ = ow; outerH_ = oh; fmt_ = fmt; reset_ = true;
    if (packed_)
        BridgeLog("direct: eye pair %ux%u, packed atlas %ux%u = fovea %ux%u (fovea %.2f x scale %.2f) + outer %ux%u (scale %.2f), %.0f%% of the pixels in one evaluate%s, format %d",
                  W, H, ww, wh, foveaW_, foveaH_, cfg_.fovea, cfg_.scale, ow, oh, cfg_.outer_scale, 100.0 * ww * wh / ((double)W * H),
                  cfg_.passes > 1 ? (std::string(", then ") + std::to_string(cfg_.passes - 1) + " fovea-only pass(es) at " + std::to_string(foveaW_) + "x" + std::to_string(foveaH_)).c_str() : "", (int)fmt);
    else
        BridgeLog("direct: eye pair %ux%u, fovea tier %ux%u (fovea %.2f x scale %.2f = %.0f%% of the pixels) x %d pass(es)%s, format %d", W, H, ww, wh, cfg_.fovea, cfg_.scale, 100.0 * ww * wh / ((double)W * H), cfg_.passes,
                  ow ? (std::string(", outer tier ") + std::to_string(ow) + "x" + std::to_string(oh) + " (" + std::to_string((int)(100.0 * ow * oh / ((double)W * H))) + "%) x 1 pass").c_str() : "", (int)fmt);
    return true;
}

inline void DirectNR::MakeSrv(UINT slot, ID3D12Resource *r, DXGI_FORMAT f)
{
    D3D12_SHADER_RESOURCE_VIEW_DESC d = {}; d.Format = f; d.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; d.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING; d.Texture2D.MipLevels = 1;
    dev_->CreateShaderResourceView(r, &d, CpuSlot(slot));
}
inline void DirectNR::MakeUav(UINT slot, ID3D12Resource *r, DXGI_FORMAT f)
{
    D3D12_UNORDERED_ACCESS_VIEW_DESC d = {}; d.Format = f; d.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    dev_->CreateUnorderedAccessView(r, nullptr, &d, CpuSlot(slot));
}

inline void DirectNR::DebugHalf(ID3D12GraphicsCommandList *cl, UINT slot, ID3D12Resource *in, ID3D12Resource *out, ID3D12Resource *mv, ID3D12Resource *depth, UINT gx, UINT gy)
{
    // in is already an SRV, out a UAV; mv/depth are bound only to satisfy the table and get their states restored
    for (UINT s = 0; s < 5; ++s) MakeSrv(slot + s, in, fmt_);
    MakeUav(slot + 5, out, fmt_); MakeUav(slot + 6, mv, DXGI_FORMAT_R16G16_FLOAT); MakeUav(slot + 7, depth, DXGI_FORMAT_R32_FLOAT);
    Barrier(cl, mv, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    Barrier(cl, depth, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    cl->SetComputeRootDescriptorTable(1, GpuSlot(slot)); cl->SetComputeRootDescriptorTable(2, GpuSlot(slot + 5));
    cl->SetPipelineState(cfg_.ngx == 5 ? psoTopHalf_.Get() : psoHalf_.Get()); cl->Dispatch(gx, gy, 1);
    Barrier(cl, mv, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    Barrier(cl, depth, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
}

inline void DirectNR::CopyBlock(ID3D12GraphicsCommandList *cl, ID3D12Resource *src, D3D12_RESOURCE_STATES srcFrom, D3D12_RESOURCE_STATES srcTo, ID3D12Resource *dst, D3D12_RESOURCE_STATES dstFrom, D3D12_RESOURCE_STATES dstTo, UINT w, UINT h)
{
    Barrier(cl, src, srcFrom, D3D12_RESOURCE_STATE_COPY_SOURCE); Barrier(cl, dst, dstFrom, D3D12_RESOURCE_STATE_COPY_DEST);
    D3D12_TEXTURE_COPY_LOCATION s = {}; s.pResource = src; s.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX; s.SubresourceIndex = 0;
    D3D12_TEXTURE_COPY_LOCATION d = s; d.pResource = dst;
    D3D12_BOX box = { 0, 0, 0, w, h, 1 };
    cl->CopyTextureRegion(&d, 0, 0, 0, &s, &box);
    Barrier(cl, src, D3D12_RESOURCE_STATE_COPY_SOURCE, srcTo); Barrier(cl, dst, D3D12_RESOURCE_STATE_COPY_DEST, dstTo);
}

inline bool DirectNR::CreateFeature(int pass)
{
    char tag[16]; snprintf(tag, sizeof(tag), "pass %d", pass);
    // packed: the passes after the first run on the fovea block alone
    const bool foveaOnly = packed_ && pass > 0;
    return CreateFeatureSized(foveaOnly ? foveaW_ : workW_, foveaOnly ? foveaH_ : workH_, &features_[pass], tag);
}

inline bool DirectNR::Evaluate(ID3D12GraphicsCommandList *cl, NVSDK_NGX_Handle *f, ID3D12Resource *in, ID3D12Resource *out, ID3D12Resource *mv, ID3D12Resource *depth, UINT w, UINT h, const char *tag)
{
    SetRes("DLSSNR.Color", in); SetRes("DLSSNR.Depth", depth); SetRes("DLSSNR.MVec", mv); SetRes("DLSSNR.Output", out);
    SetUInt("DLSSNR.Enabled", 1u); SetUInt("DLSSNR.Width", w); SetUInt("DLSSNR.Height", h);
    SetUInt("DLSSNR.DepthInverted", 0u); SetUInt("DLSSNR.Reset", reset_ ? 1u : 0u);
    SetUInt("DLSSNR.ColorSubrectBaseX", 0u); SetUInt("DLSSNR.ColorSubrectBaseY", 0u); SetUInt("DLSSNR.ColorSubrectWidth", w); SetUInt("DLSSNR.ColorSubrectHeight", h);
    SetUInt("DLSSNR.OutputSubrectBaseX", 0u); SetUInt("DLSSNR.OutputSubrectBaseY", 0u); SetUInt("DLSSNR.OutputSubrectWidth", w); SetUInt("DLSSNR.OutputSubrectHeight", h);
    SetUInt("DLSSNR.DepthSubrectBaseX", 0u); SetUInt("DLSSNR.DepthSubrectBaseY", 0u); SetUInt("DLSSNR.DepthSubrectWidth", w); SetUInt("DLSSNR.DepthSubrectHeight", h);
    SetUInt("DLSSNR.MVecSubrectBaseX", 0u); SetUInt("DLSSNR.MVecSubrectBaseY", 0u); SetUInt("DLSSNR.MVecSubrectWidth", w); SetUInt("DLSSNR.MVecSubrectHeight", h);
    SetFloat("DLSSNR.MVecScaleX", 1.0f); SetFloat("DLSSNR.MVecScaleY", 1.0f);
    SetFloat("DLSSNR.Intensity", cfg_.intensity); SetUInt("DLSSNR.Style", cfg_.style);
    SetFloat("DLSSNR.LocalStructureStrength", cfg_.local_structure); SetFloat("DLSSNR.LocalToneStrength", cfg_.local_tone);
    SetFloat("DLSSNR.SkinStructureStrength", cfg_.skin_structure); SetUInt("DLSSNR.UseAutoMask", cfg_.auto_mask ? 1u : 0u);
    unsigned r = featurePath_ == 1 ? (unsigned)coreEvaluate_(cl, f, caps_, nullptr) : (unsigned)snipEvaluate_(cl, f, caps_);
    if (r != 1) { BridgeLog("direct: EvaluateFeature %s -> 0x%08X %s", tag, r, ResultName(r)); return false; }
    return true;
}

inline bool DirectNR::CreateFeatureSized(UINT workW_, UINT workH_, NVSDK_NGX_Handle **out, const char *tag)
{
    // tuning is read by the model at create time
    SetUInt("DLSSNR.Enabled", 1u); SetUInt("DLSSNR.Width", workW_); SetUInt("DLSSNR.Height", workH_);
    SetUInt("CreationNodeMask", 1u); SetUInt("VisibilityNodeMask", 1u);
    SetUInt("DLSSNR.Hint.Render.Preset", cfg_.preset);
    SetFloat("DLSSNR.Intensity", cfg_.intensity); SetUInt("DLSSNR.Style", cfg_.style);
    SetFloat("DLSSNR.LocalStructureStrength", cfg_.local_structure); SetFloat("DLSSNR.LocalToneStrength", cfg_.local_tone);
    SetFloat("DLSSNR.SkinStructureStrength", cfg_.skin_structure); SetUInt("DLSSNR.UseAutoMask", cfg_.auto_mask ? 1u : 0u);
    SetUInt("DLSSNR.UICorrection", 1u);

    // once a path has worked, keep using it for the remaining passes
    const bool tryCore = (cfg_.ngx == 0 || cfg_.ngx == 1) && featurePath_ != 2, trySnip = (cfg_.ngx == 0 || cfg_.ngx == 2) && snipCreate_ && featurePath_ != 1;
    if (tryCore)
    {
        NVSDK_NGX_Handle *h = nullptr;
        const NVSDK_NGX_Result r = coreCreate_(list_.Get(), (NVSDK_NGX_Feature)18, caps_, &h);
        BridgeLog("direct: core CreateFeature(18) %s %ux%u -> 0x%08X %s handle=%p", tag, workW_, workH_, (unsigned)r, ResultName((unsigned)r), h);
        if (r == NVSDK_NGX_Result_Success && h) { *out = h; featurePath_ = 1; return true; }
    }
    if (trySnip)
    {
        if (!snipInited_)
        {
            std::wstring sp = gameDir_ + L"\\nvngx_dlssnr.dll";
            DWORD attr = GetFileAttributesW(sp.c_str()); if (attr == INVALID_FILE_ATTRIBUTES) sp = ownDir_ + L"\\nvngx_dlssnr.dll";
            const int lr = snipLoad_(sp.c_str());
            const int ir = lr > 0 ? snipInit_(cfg_.app_id, ownDir_.c_str(), dev_.Get(), 0x15, &fcInfo_) : 0;
            BridgeLog("direct: snippet %ls load=%d Init_Ext -> 0x%08X %s", sp.c_str(), lr, (unsigned)ir, ResultName((unsigned)ir));
            snipInited_ = lr > 0 && ir == (int)NVSDK_NGX_Result_Success;
        }
        if (snipInited_)
        {
            void *h = nullptr;
            const int r = snipCreate_(list_.Get(), 18, caps_, &h);
            BridgeLog("direct: snippet CreateFeature(18) %s %ux%u -> 0x%08X %s handle=%p", tag, workW_, workH_, (unsigned)r, ResultName((unsigned)r), h);
            if (r == (int)NVSDK_NGX_Result_Success && h) { *out = (NVSDK_NGX_Handle *)h; featurePath_ = 2; return true; }
        }
    }
    MarkFailed("no NGX path could create the Neural Rendering feature (see results above)");
    return false;
}

inline bool DirectNR::Process(const DirectView *views, uint32_t count)
{
    if (failed_ || count == 0 || count > 4) return false;
    // W/H: the allocation (each view's maximum rect, side by side); Wa/Ha: the part in use this frame. Only the
    // allocation drives the textures and the model size, so a per-frame rect change (dynamic viewport) costs nothing.
    UINT W = 0, H = 0, Wa = 0, Ha = 0; bool layoutChanged = count != viewCount_, dynamic = false;
    for (uint32_t i = 0; i < count; ++i)
    {
        const UINT vw = views[i].right - views[i].left, vh = views[i].bottom - views[i].top;
        const UINT rw = std::max(views[i].maxW, vw), rh = std::max(views[i].maxH, vh);
        if (rw != viewW_[i] || rh != viewH_[i]) layoutChanged = true;
        if (rw != vw || rh != vh) dynamic = true;
        W += rw; H = std::max(H, rh); Wa += vw; Ha = std::max(Ha, vh);
    }
    if (layoutChanged) { viewCount_ = count; for (uint32_t i = 0; i < count; ++i) { viewW_[i] = std::max(views[i].maxW, views[i].right - views[i].left); viewH_[i] = std::max(views[i].maxH, views[i].bottom - views[i].top); } W_ = 0; }
    D3D11_TEXTURE2D_DESC td = {}; views[0].texture->GetDesc(&td);
    DXGI_FORMAT fmt = DXGI_FORMAT_UNKNOWN;
    switch (td.Format)
    {
    case DXGI_FORMAT_R8G8B8A8_TYPELESS: case DXGI_FORMAT_R8G8B8A8_UNORM: case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: fmt = DXGI_FORMAT_R8G8B8A8_UNORM; break;
    case DXGI_FORMAT_B8G8R8A8_TYPELESS: case DXGI_FORMAT_B8G8R8A8_UNORM: case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: fmt = DXGI_FORMAT_B8G8R8A8_UNORM; break;
    case DXGI_FORMAT_R16G16B16A16_TYPELESS: case DXGI_FORMAT_R16G16B16A16_FLOAT: fmt = DXGI_FORMAT_R16G16B16A16_FLOAT; break;
    default: { char b[80]; snprintf(b, sizeof(b), "unsupported XR image format %d", (int)td.Format); MarkFailed(b); return false; }
    }
    if (!EnsureSized(W, H, fmt)) return false;
    LARGE_INTEGER t0, t1; QueryPerformanceCounter(&t0);
    const bool verbose = frames_ < (uint64_t)cfg_.log_frames;

    // ---- A: gather eyes into the shared colour texture ----
    UINT x = 0;
    for (uint32_t i = 0; i < count; ++i)
    {
        const DirectView &v = views[i]; D3D11_TEXTURE2D_DESC vd = {}; v.texture->GetDesc(&vd);
        D3D11_BOX box = { v.left, v.top, 0, v.right, v.bottom, 1 };
        ctxA_->CopySubresourceRegion(sharedColorA_.Get(), 0, x, 0, 0, v.texture, D3D11CalcSubresource(0, v.arrayIndex, vd.MipLevels), &box);
        x += v.right - v.left;
    }
    ctxA4_->Signal(fenceAB_A_.Get(), ++vIn_); ctxA_->Flush();

    // ---- ring slot ----
    frameSlot_ = (frameSlot_ + 1) % kFrames;
    if (ringValue_[frameSlot_] && ringFence_->GetCompletedValue() < ringValue_[frameSlot_]) { ringFence_->SetEventOnCompletion(ringValue_[frameSlot_], ringEvent_); WaitForSingleObject(ringEvent_, 2000); }
    if (tsPending_[frameSlot_] && tsReadback_ && tsFreq_)
    {
        // this slot's previous frame has finished on the GPU: read its timestamps
        const D3D12_RANGE rr = { (SIZE_T)frameSlot_ * 4 * sizeof(UINT64), (SIZE_T)(frameSlot_ + 1) * 4 * sizeof(UINT64) }; UINT64 *p = nullptr;
        if (SUCCEEDED(tsReadback_->Map(0, &rr, (void **)&p)))
        {
            const UINT64 *q = p + frameSlot_ * 4;
            if (q[3] > q[0] && q[2] >= q[1]) { gpuTotalMs_ += (q[3] - q[0]) * 1000.0 / (double)tsFreq_; gpuModelMs_ += (q[2] - q[1]) * 1000.0 / (double)tsFreq_; ++gpuSamples_; }
            const D3D12_RANGE none = { 0, 0 }; tsReadback_->Unmap(0, &none);
        }
        tsPending_[frameSlot_] = false;
    }
    alloc_[frameSlot_]->Reset(); list_->Reset(alloc_[frameSlot_].Get(), nullptr);
    ID3D12GraphicsCommandList *cl = list_.Get();
    ID3D12DescriptorHeap *heaps[] = { heap_.Get() }; cl->SetDescriptorHeaps(1, heaps);
    // descriptors per frame: 0-4 srv / 5-7 uav (fovea tier), 8-12 srv / 13-15 uav (outer tier), 16-20 srv / 21-23 uav (resolve)
    const UINT base = frameSlot_ * kDescPerFrame;
    const bool outer = outerW_ != 0, packed = packed_, outerSeparate = outer && !packed;

    // constants (packed: the outer rects live in the work atlas, so the resolve samples both tiers from the same texture)
    DirectConstants c = {}; c.workW = workW_; c.workH = workH_; c.fullW = Wa; c.fullH = Ha; c.allocW = W_; c.allocH = H_; c.viewCount = count; c.depthValue = cfg_.depth_value;
    c.mvScale = cfg_.mv ? cfg_.mv_sign * cfg_.mv_scale : 0.0f; c.residual = cfg_.residual ? 1u : 0u; c.feather = cfg_.feather;
    c.shape = (UINT)cfg_.fovea_shape; c.outerOn = outer ? 1u : 0u; c.outerW = packed ? workW_ : outerW_; c.outerH = packed ? workH_ : outerH_;
    x = 0; bool poseReset = false; UINT wx = 0, ox = packed ? foveaW_ : 0;
    for (uint32_t i = 0; i < count && i < 4; ++i)
    {
        const DirectView &v = views[i]; const UINT w = v.right - v.left, h = v.bottom - v.top;   // this frame's rect
        const float vr = std::min(1.0f, std::max(0.25f, cfg_.viewport_ref));
        const float Rw = viewW_[i] * vr, Rh = viewH_[i] * vr;                                          // the model's reference rect (allocation x viewport_ref), same expression as EnsureSized
        c.flip[i] = v.flipY ? 1u : 0u;
        // eye rect in the full texture, the fovea crop inside it, and the crop's region in the work texture (sized from the max rect)
        c.eyeFull[i][0] = (float)x; c.eyeFull[i][1] = 0; c.eyeFull[i][2] = (float)w; c.eyeFull[i][3] = (float)h;
        const float cw = resolve_ ? floorf(w * cfg_.fovea) : (float)w, ch = resolve_ ? floorf(h * cfg_.fovea) : (float)h;
        c.cropFull[i][0] = x + floorf((w - cw) * 0.5f); c.cropFull[i][1] = floorf((h - ch) * 0.5f); c.cropFull[i][2] = cw; c.cropFull[i][3] = ch;
        const UINT rw = resolve_ ? ((UINT)(Rw * cfg_.fovea * cfg_.scale) & ~1u) : w, rh = resolve_ ? ((UINT)(Rh * cfg_.fovea * cfg_.scale) & ~1u) : h;
        c.rectWork[i][0] = (float)wx; c.rectWork[i][1] = 0; c.rectWork[i][2] = (float)rw; c.rectWork[i][3] = (float)rh;
        wx += rw;
        if (outer)
        {
            const UINT orw = (UINT)(Rw * cfg_.outer_scale) & ~1u, orh = (UINT)(Rh * cfg_.outer_scale) & ~1u;
            c.rectOuter[i][0] = (float)ox; c.rectOuter[i][1] = 0; c.rectOuter[i][2] = (float)orw; c.rectOuter[i][3] = (float)orh;
            ox += orw;
        }
        for (int k = 0; k < 4; ++k) c.qCur[i][k] = v.q[k];
        c.tanCur[i][0] = tanf(v.fovL); c.tanCur[i][1] = tanf(v.fovR); c.tanCur[i][2] = tanf(v.fovU); c.tanCur[i][3] = tanf(v.fovD);
        PosePrev &p = prev_[i];
        if (!p.valid) { poseReset = true; for (int k = 0; k < 4; ++k) c.qPrev[i][k] = v.q[k]; c.tanPrev[i][0] = c.tanCur[i][0]; c.tanPrev[i][1] = c.tanCur[i][1]; c.tanPrev[i][2] = c.tanCur[i][2]; c.tanPrev[i][3] = c.tanCur[i][3]; }
        else { for (int k = 0; k < 4; ++k) c.qPrev[i][k] = p.q[k]; c.tanPrev[i][0] = tanf(p.fovL); c.tanPrev[i][1] = tanf(p.fovR); c.tanPrev[i][2] = tanf(p.fovU); c.tanPrev[i][3] = tanf(p.fovD); }
        for (int k = 0; k < 4; ++k) p.q[k] = v.q[k]; p.fovL = v.fovL; p.fovR = v.fovR; p.fovU = v.fovU; p.fovD = v.fovD; p.valid = true;
        x += w;
    }
    if (poseReset) reset_ = true;
    static_assert(sizeof(DirectConstants) <= kCbStride, "constants exceed the per-frame slot");
    const UINT64 cbFovea = (UINT64)(frameSlot_ * kCbPerFrame + 0) * kCbStride, cbOuter = (UINT64)(frameSlot_ * kCbPerFrame + 1) * kCbStride;
    c.tier = packed ? 2u : 0u; memcpy(cbMapped_ + cbFovea, &c, sizeof(c));
    c.tier = 1; memcpy(cbMapped_ + cbOuter, &c, sizeof(c));
    cl->SetComputeRootSignature(rootSig_.Get());
    cl->SetComputeRootConstantBufferView(0, cb_->GetGPUVirtualAddress() + cbFovea);

    // ---- wait for A, pull the colour in ----
    queue_->Wait(fenceAB_.Get(), vIn_);
    const bool ts = tsHeap_ != nullptr; const UINT tsBase = frameSlot_ * 4;
    if (ts) cl->EndQuery(tsHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, tsBase + 0);
    Barrier(cl, sharedColor_.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_SOURCE);
    cl->CopyResource(colorFull_.Get(), sharedColor_.Get());
    Barrier(cl, sharedColor_.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COMMON);
    Barrier(cl, colorFull_.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

    ID3D12Resource *modelIn = colorFull_.Get();
    const UINT gx = (workW_ + 7) / 8, gy = (workH_ + 7) / 8;
    for (UINT s = 0; s < 5; ++s) MakeSrv(base + s, colorFull_.Get(), fmt_);
    MakeUav(base + 5, resolve_ ? colorWork_.Get() : outWork_.Get(), fmt_); MakeUav(base + 6, mv_.Get(), DXGI_FORMAT_R16G16_FLOAT); MakeUav(base + 7, depth_.Get(), DXGI_FORMAT_R32_FLOAT);
    cl->SetComputeRootDescriptorTable(1, GpuSlot(base + 0)); cl->SetComputeRootDescriptorTable(2, GpuSlot(base + 5));
    if (resolve_)
    {
        cl->SetPipelineState(psoDown_.Get()); cl->Dispatch(gx, gy, 1);
        Barrier(cl, colorWork_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        modelIn = colorWork_.Get();
    }
    if (!depthFilled_) { cl->SetPipelineState(psoFill_.Get()); cl->Dispatch(gx, gy, 1); depthFilled_ = true; Barrier(cl, depth_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE); }
    cl->SetPipelineState(psoMv_.Get()); cl->Dispatch(gx, gy, 1);
    Barrier(cl, mv_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

    // ---- the model: one instance per pass, each fed the previous pass's output ----
    // Packed multi-pass: pass 0 covers the whole atlas (fovea crops + outer eye pair); the later passes run on the fovea
    // block alone, at its own size, so the outer strip and the atlas padding are not paid for again. The final fovea
    // result is copied back into the atlas, which then holds the outer tier from pass 0 and the fovea from the last pass.
    if (ts) cl->EndQuery(tsHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, tsBase + 1);
    const bool foveaOnlyPasses = packed && cfg_.passes > 1;
    ID3D12Resource *passIn = modelIn, *passOut = outWork_.Get();
    for (int pass = 0; pass < cfg_.passes; ++pass)
    {
        UINT pw = workW_, ph = workH_, pgx = gx, pgy = gy; ID3D12Resource *pmv = mv_.Get(), *pdepth = depth_.Get();
        if (pass > 0 && foveaOnlyPasses)
        {
            pw = foveaW_; ph = foveaH_; pgx = (foveaW_ + 7) / 8; pgy = (foveaH_ + 7) / 8; pmv = mvF_.Get(); pdepth = depthF_.Get();
            if (pass == 1)
            {
                // fovea block of the atlas result -> foveaA_, the atlas's fovea motion vectors -> mvF_ (and depth once); outWork_ stays COPY_DEST for the copy back
                CopyBlock(cl, outWork_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_DEST, foveaA_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, foveaW_, foveaH_);
                CopyBlock(cl, mv_.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, mvF_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, foveaW_, foveaH_);
                if (!depthFFilled_) { CopyBlock(cl, depth_.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, depthF_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, foveaW_, foveaH_); depthFFilled_ = true; }
                passIn = foveaA_.Get(); passOut = foveaB_.Get();
            }
            else
            {
                ID3D12Resource *next = (passOut == foveaA_.Get()) ? foveaB_.Get() : foveaA_.Get();
                Barrier(cl, passOut, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
                Barrier(cl, next, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);   // it was the input of the previous pass
                passIn = passOut; passOut = next;
            }
        }
        else if (pass > 0)
        {
            // previous output becomes this pass's input (SRV); write into the other work buffer (UAV)
            Barrier(cl, passOut, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            passIn = passOut; passOut = (pass & 1) ? outWork2_.Get() : outWork_.Get();
        }
        if (cfg_.ngx == 3)
        {
            // debug passthrough: the "edit" is the input itself
            Barrier(cl, passOut, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_DEST);
            Barrier(cl, passIn, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_SOURCE);
            cl->CopyResource(passOut, passIn);
            Barrier(cl, passIn, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            Barrier(cl, passOut, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        }
        else if (cfg_.ngx >= 4) DebugHalf(cl, base + 24 + pass * 8, passIn, passOut, pmv, pdepth, pgx, pgy);
        else
        {
            if (!features_[pass] && !CreateFeature(pass)) { cl->Close(); return false; }
            char tag[16]; snprintf(tag, sizeof(tag), "pass %d", pass);
            if (!Evaluate(cl, features_[pass], passIn, passOut, pmv, pdepth, pw, ph, tag)) { cl->Close(); MarkFailed("Neural Rendering evaluate failed"); return false; }
        }
    }
    // the final pass's output is in passOut (UAV); earlier pass buffers are left as SRV and restored below
    ID3D12Resource *modelOut = passOut;
    if (foveaOnlyPasses)
    {
        // final fovea result back into the atlas (outWork_ has been COPY_DEST since pass 1); both fovea buffers back to UAV
        ID3D12Resource *other = (passOut == foveaA_.Get()) ? foveaB_.Get() : foveaA_.Get();
        CopyBlock(cl, passOut, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, outWork_.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, foveaW_, foveaH_);
        Barrier(cl, other, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        modelOut = outWork_.Get();
    }
    else if (cfg_.passes > 1)
    {
        // the buffer that ended as an input (SRV) goes back to UAV for next frame
        ID3D12Resource *other = (modelOut == outWork_.Get()) ? outWork2_.Get() : outWork_.Get();
        Barrier(cl, other, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    }

    // ---- separate outer tier: the whole eye pair at outer_scale, one pass, for lighting/tone outside the fovea ----
    // (packed: the outer eye pair was part of the atlas the passes above already processed)
    if (outerSeparate)
    {
        const UINT ogx = (outerW_ + 7) / 8, ogy = (outerH_ + 7) / 8;
        cl->SetComputeRootConstantBufferView(0, cb_->GetGPUVirtualAddress() + cbOuter);
        for (UINT s = 0; s < 5; ++s) MakeSrv(base + 8 + s, colorFull_.Get(), fmt_);
        MakeUav(base + 13, colorOuter_.Get(), fmt_); MakeUav(base + 14, mvOuter_.Get(), DXGI_FORMAT_R16G16_FLOAT); MakeUav(base + 15, depthOuter_.Get(), DXGI_FORMAT_R32_FLOAT);
        cl->SetComputeRootDescriptorTable(1, GpuSlot(base + 8)); cl->SetComputeRootDescriptorTable(2, GpuSlot(base + 13));
        cl->SetPipelineState(psoDown_.Get()); cl->Dispatch(ogx, ogy, 1);
        Barrier(cl, colorOuter_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        if (!outerDepthFilled_) { cl->SetPipelineState(psoFill_.Get()); cl->Dispatch(ogx, ogy, 1); outerDepthFilled_ = true; Barrier(cl, depthOuter_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE); }
        cl->SetPipelineState(psoMv_.Get()); cl->Dispatch(ogx, ogy, 1);
        Barrier(cl, mvOuter_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        if (cfg_.ngx == 3)
        {
            Barrier(cl, outOuter_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_DEST);
            Barrier(cl, colorOuter_.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_SOURCE);
            cl->CopyResource(outOuter_.Get(), colorOuter_.Get());
            Barrier(cl, colorOuter_.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            Barrier(cl, outOuter_.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        }
        else if (cfg_.ngx >= 4) DebugHalf(cl, base + 56, colorOuter_.Get(), outOuter_.Get(), mvOuter_.Get(), depthOuter_.Get(), ogx, ogy);
        else
        {
            if (!featureOuter_ && !CreateFeatureSized(outerW_, outerH_, &featureOuter_, "outer")) { cl->Close(); return false; }
            if (!Evaluate(cl, featureOuter_, colorOuter_.Get(), outOuter_.Get(), mvOuter_.Get(), depthOuter_.Get(), outerW_, outerH_, "outer")) { cl->Close(); MarkFailed("Neural Rendering evaluate failed"); return false; }
        }
        Barrier(cl, outOuter_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        cl->SetComputeRootConstantBufferView(0, cb_->GetGPUVirtualAddress() + cbFovea);
    }
    if (reset_ && verbose) BridgeLog("direct: history reset this frame");
    reset_ = false;
    if (ts) cl->EndQuery(tsHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, tsBase + 2);

    // ---- resolve / hand back ----
    if (resolve_)
    {
        Barrier(cl, modelOut, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        MakeSrv(base + 16, colorFull_.Get(), fmt_); MakeSrv(base + 17, colorWork_.Get(), fmt_); MakeSrv(base + 18, modelOut, fmt_);
        MakeSrv(base + 19, packed ? colorWork_.Get() : outer ? colorOuter_.Get() : colorFull_.Get(), fmt_); MakeSrv(base + 20, packed ? modelOut : outer ? outOuter_.Get() : colorFull_.Get(), fmt_);
        MakeUav(base + 21, outFull_.Get(), fmt_); MakeUav(base + 22, mv_.Get(), DXGI_FORMAT_R16G16_FLOAT); MakeUav(base + 23, depth_.Get(), DXGI_FORMAT_R32_FLOAT);
        Barrier(cl, mv_.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        Barrier(cl, depth_.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        cl->SetComputeRootDescriptorTable(1, GpuSlot(base + 16)); cl->SetComputeRootDescriptorTable(2, GpuSlot(base + 21));
        cl->SetPipelineState(psoResolve_.Get()); cl->Dispatch((Wa + 7) / 8, (Ha + 7) / 8, 1);
        if (outerSeparate)
        {
            Barrier(cl, colorOuter_.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            Barrier(cl, outOuter_.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            Barrier(cl, mvOuter_.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        }
        Barrier(cl, outFull_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
        Barrier(cl, sharedOut_.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
        cl->CopyResource(sharedOut_.Get(), outFull_.Get());
        Barrier(cl, sharedOut_.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON);
        Barrier(cl, outFull_.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        Barrier(cl, modelOut, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        Barrier(cl, colorWork_.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    }
    else
    {
        Barrier(cl, modelOut, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
        Barrier(cl, sharedOut_.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
        cl->CopyResource(sharedOut_.Get(), modelOut);
        Barrier(cl, sharedOut_.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON);
        Barrier(cl, modelOut, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        Barrier(cl, mv_.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        Barrier(cl, depth_.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    }
    Barrier(cl, colorFull_.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
    if (ts)
    {
        cl->EndQuery(tsHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, tsBase + 3);
        cl->ResolveQueryData(tsHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, tsBase, 4, tsReadback_.Get(), (UINT64)tsBase * sizeof(UINT64));
    }
    if (FAILED(cl->Close())) { MarkFailed("command list Close failed (a D3D12 validation error in recording)"); return false; }
    ID3D12CommandList *lists[] = { cl }; queue_->ExecuteCommandLists(1, lists);
    queue_->Signal(fenceBA_.Get(), ++vOut_);
    ringValue_[frameSlot_] = ringNext_++; queue_->Signal(ringFence_.Get(), ringValue_[frameSlot_]); tsPending_[frameSlot_] = ts;
    if (FAILED(dev_->GetDeviceRemovedReason())) { char b[96]; snprintf(b, sizeof(b), "D3D12 device removed (0x%08lX)", (unsigned long)dev_->GetDeviceRemovedReason()); MarkFailed(b); return false; }

    // ---- A: scatter back ----
    ctxA4_->Wait(fenceBA_A_.Get(), vOut_);
    x = 0;
    for (uint32_t i = 0; i < count; ++i)
    {
        const DirectView &v = views[i]; D3D11_TEXTURE2D_DESC vd = {}; v.texture->GetDesc(&vd);
        const UINT w = v.right - v.left, h = v.bottom - v.top; D3D11_BOX box = { x, 0, 0, x + w, h, 1 };
        ctxA_->CopySubresourceRegion(v.texture, D3D11CalcSubresource(0, v.arrayIndex, vd.MipLevels), v.left, v.top, 0, sharedOutA_.Get(), 0, &box);
        x += w;
    }
    ctxA_->Flush();

    QueryPerformanceCounter(&t1); lastMs_ = (t1.QuadPart - t0.QuadPart) * 1000.0 / (double)qpf_.QuadPart; totalMs_ += lastMs_; ++frames_;
    if (lastFrameQpc_) { intervalTotalMs_ += (t0.QuadPart - lastFrameQpc_) * 1000.0 / (double)qpf_.QuadPart; ++intervalCount_; }
    if (dynamic) { dynamicSeen_ = true; viewportSum_ += (double)Wa / (double)W_; ++viewportCount_; }
    lastFrameQpc_ = t0.QuadPart;
    if (verbose) BridgeLog("direct: frame %llu done, cpu %.3f ms, path=%s, passes=%d%s", (unsigned long long)frames_, lastMs_, featurePath_ == 1 ? "core" : featurePath_ == 2 ? "snippet" : "none", cfg_.passes,
                           dynamic ? (std::string(", dynamic viewport: ") + std::to_string(Wa) + "x" + std::to_string(Ha) + " of " + std::to_string(W_) + "x" + std::to_string(H_)).c_str() : "");
    else if (frames_ % 600 == 0)
    {
        const double iv = intervalCount_ ? intervalTotalMs_ / (double)intervalCount_ : 0.0;
        BridgeLog("direct: %llu frames | frame interval %.1f ms (%.0f Hz) | gpu: this layer %.2f ms of which model %.2f ms (%d pass%s) | cpu %.2f ms | fovea %ux%u (%.2f @ %.2f)%s", (unsigned long long)frames_, iv, iv > 0 ? 1000.0 / iv : 0.0, avgGpuMs(), avgModelMs(), cfg_.passes,
                  packed_ ? " packed" : outerW_ ? " + outer" : "", avgCpuMs(), packed_ ? foveaW_ : workW_, workH_, cfg_.fovea, cfg_.scale,
                  (std::string(outerW_ ? " outer " + std::to_string(outerW_) + "x" + std::to_string(outerH_) + (packed_ ? " atlas " + std::to_string(workW_) + "x" + std::to_string(workH_) : "") : "") +
                   (dynamicSeen_ ? " | viewport " + std::to_string((int)(100.0 * (viewportCount_ ? viewportSum_ / (double)viewportCount_ : 1.0))) + "% of max (model sized for " + std::to_string((int)(100.0 * std::min(1.0f, std::max(0.25f, cfg_.viewport_ref)))) + "%)" : "")).c_str());
        intervalTotalMs_ = 0; intervalCount_ = 0; gpuTotalMs_ = 0; gpuModelMs_ = 0; gpuSamples_ = 0; viewportSum_ = 0; viewportCount_ = 0;
    }
    return true;
}

inline void DirectNR::Shutdown(bool leak)
{
    if (leak)
    {
        // same reasoning as the ReShade bridge: drop game-side references, leave the D3D12 device and NGX alive
        sharedColorA_.Reset(); sharedOutA_.Reset(); fenceAB_A_.Reset(); fenceBA_A_.Reset();
        ctxA4_.Reset(); ctxA_.Reset(); devA5_.Reset(); devA1_.Reset(); devA_.Reset();
        BridgeLog("direct: detached after %llu frames (D3D12 side left alive)", (unsigned long long)frames_);
        return;
    }
    ReleaseSized();
    if (coreInited_ && coreShutdown_) coreShutdown_(dev_.Get());
    psoDown_.Reset(); psoMv_.Reset(); psoFill_.Reset(); psoResolve_.Reset(); rootSig_.Reset(); heap_.Reset();
    fenceAB_.Reset(); fenceBA_.Reset(); ringFence_.Reset(); list_.Reset(); for (auto &a : alloc_) a.Reset(); queue_.Reset(); dev_.Reset();
    if (ringEvent_) { CloseHandle(ringEvent_); ringEvent_ = nullptr; }
    fenceAB_A_.Reset(); fenceBA_A_.Reset(); ctxA4_.Reset(); ctxA_.Reset(); devA5_.Reset(); devA1_.Reset(); devA_.Reset();
    BridgeLog("direct: shut down after %llu frames", (unsigned long long)frames_);
}

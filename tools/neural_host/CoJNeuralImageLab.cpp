// SPDX-License-Identifier: GPL-3.0-or-later
#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include "NeuralPass.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

using Microsoft::WRL::ComPtr;
namespace {
using Clock = std::chrono::steady_clock;
struct Arguments {
    std::filesystem::path runtime, input, output;
    unsigned width{}, height{}, scale{}, warmup=2, iterations=10, timeout_ms=60000;
    unsigned style=0, preset=0, intensity=60;
    int diagnostic=0;
    const char* mode="model";
};
void Usage() {
    std::fputs("CoJNeuralImageLab --runtime <absolute-directory> --input <pair.rgba> --output <new-pair.rgba>\n"
        "  --width <1..1280> --height <1..1280> (--scale25|--scale50|--scale100)\n"
        "  [--warmup <1..32>] [--iterations <1..1000>] [--timeout-ms <100..120000>]\n"
        "  [--style <0|1|2>] [--preset <0|1>] [--intensity <0..100>]\n"
        "  (defaults: style 0, preset 0, intensity 60; exactly one neural pass)\n"
        "  [--diagnostic identity|edit]\n"
        "Reduced scales require each eye axis >=8 at scale25 or >=4 at scale50.\n"
        "Input/output: tightly packed RGBA8 encoded UNORM, entire left eye then right eye.\n"
        "Default mode requires local real-model dependencies. Output must be new.\n"
        "Fixed full-crop comparison variants: --scale100 --style 1 --intensity 100;\n"
        "  --scale100 --style 2 --intensity 100. These are experiments, not quality presets.\n"
        "Timings are offline CPU upload/pass/blocking-readback roundtrips, excluding warmup.\n"
        "Controlled still-image poses/FOV; duplicated mono input provides no binocular validation.\n",
        stderr);
}
int Error(int code, const char* category) {
    std::printf("{\"ok\":false,\"error\":\"%s\"}\n",category);
    return code;
}
bool Number(const std::wstring& text, unsigned low, unsigned high, unsigned& out) {
    if(text.empty())return false;
    unsigned value=0;
    for(const auto ch:text) {
        if(ch<L'0'||ch>L'9'||value>high/10)return false;
        value=value*10+static_cast<unsigned>(ch-L'0');
        if(value>high)return false;
    }
    if(value<low)return false;
    out=value;return true;
}
bool Parse(int argc,wchar_t** argv,Arguments& args) {
    std::set<std::wstring> seen;
    for(int index=1;index<argc;++index) {
        std::wstring key=argv[index];
        if(key==L"--scale25"||key==L"--scale50"||key==L"--scale100") {
            if(args.scale)return false;
            args.scale=key==L"--scale25"?25:key==L"--scale50"?50:100;
            continue;
        }
        if(!seen.insert(key).second||index+1>=argc)return false;
        const std::wstring value=argv[++index];
        if(value.empty())return false;
        if(key==L"--runtime")args.runtime=value;
        else if(key==L"--input")args.input=value;
        else if(key==L"--output")args.output=value;
        else if(key==L"--width") {if(!Number(value,1,1280,args.width))return false;}
        else if(key==L"--height") {if(!Number(value,1,1280,args.height))return false;}
        else if(key==L"--warmup") {if(!Number(value,1,32,args.warmup))return false;}
        else if(key==L"--iterations") {if(!Number(value,1,1000,args.iterations))return false;}
        else if(key==L"--timeout-ms") {if(!Number(value,100,120000,args.timeout_ms))return false;}
        else if(key==L"--style") {if(!Number(value,0,2,args.style))return false;}
        else if(key==L"--preset") {if(!Number(value,0,1,args.preset))return false;}
        else if(key==L"--intensity") {if(!Number(value,0,100,args.intensity))return false;}
        else if(key==L"--diagnostic") {
            if(value==L"identity") {args.diagnostic=3;args.mode="identity";}
            else if(value==L"edit") {args.diagnostic=4;args.mode="edit";}
            else return false;
        } else return false;
    }
    // DirectNR rounds each reduced eye axis down to an even pixel extent.
    // Its whole-atlas minimum does not rescue a zero-sized per-eye region.
    if(args.scale&&args.scale<100&&
        (((args.width*args.scale/100)&~1U)==0||((args.height*args.scale/100)&~1U)==0))return false;
    return !args.runtime.empty()&&args.runtime.is_absolute()&&!args.input.empty()&&
        !args.output.empty()&&args.width&&args.height&&args.scale;
}
struct File {
    HANDLE handle=INVALID_HANDLE_VALUE;
    ~File(){if(handle!=INVALID_HANDLE_VALUE)CloseHandle(handle);}
    File()=default;
    File(const File&)=delete;
    File& operator=(const File&)=delete;
};
// GPU/model calls can block in drivers. The whole GPU phase has a hard process
// budget, not a game-thread wait. No output is created until it completes.
class Deadline {
public:
    explicit Deadline(unsigned milliseconds):worker_([this,milliseconds] {
        std::unique_lock lock(mutex_);
        if(condition_.wait_for(lock,std::chrono::milliseconds(milliseconds),[this]{return done_;}))return;
        std::fputs("{\"ok\":false,\"error\":\"gpu_deadline\"}\n",stdout);std::fflush(stdout);
        TerminateProcess(GetCurrentProcess(),5);
    }){}
    ~Deadline(){
        {std::lock_guard lock(mutex_);done_=true;}
        condition_.notify_one();worker_.join();
    }
private:
    std::mutex mutex_;std::condition_variable condition_;bool done_=false;std::thread worker_;
};

class ImagePass {
public:
    bool Initialize(const Arguments& args,const NeuralOptions& options) {
        constexpr D3D_FEATURE_LEVEL levels[]{D3D_FEATURE_LEVEL_11_1,D3D_FEATURE_LEVEL_11_0};
        auto hr=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            levels,2,D3D11_SDK_VERSION,&device_,nullptr,&context_);
        if(hr==E_INVALIDARG)hr=D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            levels+1,1,D3D11_SDK_VERSION,&device_,nullptr,&context_);
        if(FAILED(hr))return false;
        if(!neural_.Initialize(device_.Get(),options,args.runtime.wstring(),args.diagnostic))return false;
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width=args.width;desc.Height=args.height;desc.ArraySize=2;desc.MipLevels=1;
        desc.SampleDesc.Count=1;desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.Usage=D3D11_USAGE_DEFAULT;
        desc.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET;
        if(FAILED(device_->CreateTexture2D(&desc,nullptr,&image_)))return false;
        desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
        if(FAILED(device_->CreateTexture2D(&desc,nullptr,&staging_)))return false;
        width_=args.width;height_=args.height;
        return true;
    }
    bool Process(const std::vector<std::uint8_t>& input,std::vector<std::uint8_t>& output,std::uint64_t frame) {
        const auto row_bytes=width_*4;
        const auto eye_bytes=static_cast<std::size_t>(row_bytes)*height_;
        for(unsigned eye=0;eye<2;++eye)
            context_->UpdateSubresource(image_.Get(),eye,nullptr,input.data()+eye*eye_bytes,row_bytes,0);
        Transport::Tracking tracking{};
        tracking.valid=1;tracking.id=frame;tracking.tick=GetTickCount64();tracking.head.orientation.w=1;
        for(auto& eye:tracking.eyes) {
            eye.pose.orientation.w=1;
            eye.left=-.7F;eye.right=.7F;eye.up=.7F;eye.down=-.7F;
        }
        if(!neural_.Process(context_.Get(),image_.Get(),tracking,frame,DXGI_FORMAT_R8G8B8A8_UNORM))return false;
        context_->CopyResource(staging_.Get(),image_.Get());
        for(unsigned eye=0;eye<2;++eye) {
            D3D11_MAPPED_SUBRESOURCE mapped{};
            if(FAILED(context_->Map(staging_.Get(),eye,D3D11_MAP_READ,0,&mapped)))return false;
            const bool valid=mapped.pData&&mapped.RowPitch>=row_bytes;
            if(valid)for(unsigned row=0;row<height_;++row)
                std::memcpy(output.data()+eye*eye_bytes+static_cast<std::size_t>(row)*row_bytes,
                    static_cast<const std::uint8_t*>(mapped.pData)+static_cast<std::size_t>(row)*mapped.RowPitch,row_bytes);
            context_->Unmap(staging_.Get(),eye);
            if(!valid)return false;
        }
        // Appearance comparisons retain source coverage. Full-scale DirectNR's
        // model/diagnostic output can contain generated alpha; report this policy.
        for(std::size_t index=3;index<output.size();index+=4)output[index]=input[index];
        return SUCCEEDED(device_->GetDeviceRemovedReason());
    }
    std::uint64_t Runs() const{return neural_.Runs();}
private:
    ComPtr<ID3D11Device> device_;ComPtr<ID3D11DeviceContext> context_;
    ComPtr<ID3D11Texture2D> image_,staging_;
    NeuralPass neural_;unsigned width_{},height_{};
};

int Run(const Arguments& args) {
    // Do not admit existing destinations, including input aliases, directories,
    // symlinks and case variants. CREATE_NEW below also closes the preflight race.
    if(GetFileAttributesW(args.output.c_str())!=INVALID_FILE_ATTRIBUTES)return Error(3,"output_exists");
    auto error=GetLastError();
    if(error!=ERROR_FILE_NOT_FOUND&&error!=ERROR_PATH_NOT_FOUND)return Error(3,"output_path");
    const auto parent=std::filesystem::absolute(args.output).parent_path();
    const auto parent_attributes=GetFileAttributesW(parent.c_str());
    if(parent_attributes==INVALID_FILE_ATTRIBUTES||!(parent_attributes&FILE_ATTRIBUTE_DIRECTORY))return Error(3,"output_path");
    File input;
    input.handle=CreateFileW(args.input.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(input.handle==INVALID_HANDLE_VALUE||GetFileType(input.handle)!=FILE_TYPE_DISK)return Error(3,"input_read");
    const auto byte_count=static_cast<std::size_t>(args.width)*args.height*4*2;
    LARGE_INTEGER size{};
    if(!GetFileSizeEx(input.handle,&size))return Error(3,"input_read");
    if(size.QuadPart!=static_cast<LONGLONG>(byte_count))return Error(3,"input_size");
    std::vector<std::uint8_t> pixels(byte_count),output(byte_count);
    DWORD read=0;
    if(!ReadFile(input.handle,pixels.data(),static_cast<DWORD>(byte_count),&read,nullptr)||read!=byte_count)return Error(3,"input_read");
    const auto directory=GetFileAttributesW(args.runtime.c_str());
    if(directory==INVALID_FILE_ATTRIBUTES||!(directory&FILE_ATTRIBUTE_DIRECTORY))return Error(4,"runtime_dependencies");
    if(!args.diagnostic)for(const auto* dependency:{L"nvngx_dlssnr.dll",L"nvngx.dll_nr_bridge.dll"}) {
        const auto attributes=GetFileAttributesW((args.runtime/dependency).c_str());
        if(attributes==INVALID_FILE_ATTRIBUTES||(attributes&FILE_ATTRIBUTE_DIRECTORY))return Error(4,"runtime_dependencies");
    }
    std::vector<double> samples;
    samples.reserve(args.iterations);
    double warmup_ms=0;std::uint64_t runs=0;
    // The same effective options drive initialization and JSON evidence.
    NeuralOptions options;
    options.enabled=true;
    options.scalePercent=static_cast<int>(args.scale); // Only model downscale. No transport resize.
    options.style=static_cast<int>(args.style);options.preset=static_cast<int>(args.preset);
    options.intensityPercent=static_cast<int>(args.intensity);
    options.foveaPercent=100;options.viewportPercent=100;options.outerEnabled=false;
    options.passes=1;options.logFrames=0;
    {
        Deadline deadline(args.timeout_ms);
        ImagePass pass;
        if(!pass.Initialize(args,options))return Error(4,"gpu_initialize");
        const auto warmup_start=Clock::now();
        for(unsigned index=0;index<args.warmup;++index)
            if(!pass.Process(pixels,output,index+1))return Error(4,"warmup_process");
        warmup_ms=std::chrono::duration<double,std::milli>(Clock::now()-warmup_start).count();
        for(unsigned index=0;index<args.iterations;++index) {
            const auto start=Clock::now();
            if(!pass.Process(pixels,output,args.warmup+index+1))return Error(4,"measured_process");
            samples.push_back(std::chrono::duration<double,std::milli>(Clock::now()-start).count());
        }
        runs=pass.Runs();
    } // Includes cleanup in the hard GPU deadline. Never overwrite either input eye.
    File destination;
    destination.handle=CreateFileW(args.output.c_str(),GENERIC_WRITE|DELETE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(destination.handle==INVALID_HANDLE_VALUE)return Error(3,GetLastError()==ERROR_FILE_EXISTS?"output_exists":"output_write");
    DWORD written=0;
    if(!WriteFile(destination.handle,output.data(),static_cast<DWORD>(byte_count),&written,nullptr)||
        written!=byte_count||!FlushFileBuffers(destination.handle)) {
        FILE_DISPOSITION_INFO remove{TRUE};
        SetFileInformationByHandle(destination.handle,FileDispositionInfo,&remove,sizeof(remove));
        return Error(3,"output_write");
    }
    std::uint64_t changed=0;unsigned max_delta=0;
    for(std::size_t index=0;index<byte_count;++index)if(index%4!=3) {
        const auto delta=static_cast<unsigned>(std::abs(int(output[index])-int(pixels[index])));
        changed+=delta!=0;max_delta=std::max(max_delta,delta);
    }
    const auto eye_bytes=byte_count/2;
    const bool mono=std::equal(pixels.begin(),pixels.begin()+eye_bytes,pixels.begin()+eye_bytes);
    // DirectNR packs both views and rounds reduced eye extents down to even
    // pixels, clamping the whole atlas to 16. Full scale uses the exact source.
    const auto reduced_w=(args.width*args.scale/100)&~1U;
    const auto reduced_h=(args.height*args.scale/100)&~1U;
    const auto atlas_width=args.scale==100?2*args.width:std::max(16U,2*reduced_w);
    const auto atlas_height=args.scale==100?args.height:std::max(16U,reduced_h);
    auto sorted=samples;std::sort(sorted.begin(),sorted.end());
    const auto middle=sorted.size()/2;
    const auto median=sorted.size()%2?sorted[middle]:(sorted[middle-1]+sorted[middle])/2;
    const auto p95=sorted[static_cast<std::size_t>(std::ceil(.95*sorted.size()))-1];
    std::cout<<std::setprecision(17)<<"{\"ok\":true,\"mode\":\""<<args.mode<<"\",\"width\":"<<args.width
        <<",\"height\":"<<args.height<<",\"scale_percent\":"<<args.scale
        <<",\"style\":"<<options.style<<",\"preset\":"<<options.preset
        <<",\"passes\":"<<options.passes<<",\"intensity_percent\":"<<options.intensityPercent
        <<",\"auto_mask\":"<<(options.autoMask?"true":"false")
        <<",\"local_structure\":"<<options.localStructure<<",\"local_tone\":"<<options.localTone
        <<",\"skin_auto\":"<<(options.skinAuto?"true":"false")<<",\"skin_strength\":"<<options.skinStrength
        <<",\"fovea_percent\":100,\"viewport_percent\":100,\"model_atlas_width\":"<<atlas_width
        <<",\"model_atlas_height\":"<<atlas_height<<",\"model_atlas_pixels\":"<<atlas_width*atlas_height
        <<",\"warmup_iterations\":"<<args.warmup<<",\"measured_iterations\":"<<args.iterations
        <<",\"successful_pairs\":"<<runs<<",\"warmup_ms\":"<<warmup_ms
        <<",\"changed_rgb_bytes\":"<<changed<<",\"max_rgb_delta\":"<<max_delta
        <<",\"identical_eye_inputs\":"<<(mono?"true":"false")
        <<",\"binocular_validated\":false,\"headset_validated\":false"
        <<",\"tracking\":\"controlled_identity_quaternions_symmetric_fov_radians\""
        <<",\"alpha_policy\":\"source_preserved_after_readback\""
        <<",\"timing_scope\":\"cpu_wall_upload_pass_blocking_readback_pair_excluding_warmup\""
        <<",\"roundtrip_ms\":{\"median\":"<<median<<",\"p95\":"<<p95<<",\"samples\":[";
    for(std::size_t index=0;index<samples.size();++index)std::cout<<(index?",":"")<<samples[index];
    std::cout<<"]}}\n";
    return 0;
}
}
int wmain(int argc,wchar_t** argv) {
    static_assert(sizeof(void*)==8,"The offline image laboratory requires x64");
    try {
        Arguments args;
        if(!Parse(argc,argv,args)){Usage();return Error(2,"arguments");}
        return Run(args);
    }catch(...){return Error(4,"exception");}
}

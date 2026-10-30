#include "backends/openvr/neural_candidate.hpp"
#include <windows.h>
#include <iostream>
#include <fstream>
#include <stdexcept>
using namespace cojvr::backends::openvr::neural;
namespace rt=cojvr::runtime;
void Check(bool b,const char* why){if(!b)throw std::runtime_error(why);}
int main()try{
    ContextFence context;
    Check(context.Observe(true),"first context missing reset");
    Check(!context.Observe(true),"stable context reset repeatedly");
    Check(context.Observe(false),"loss without capture was ignored");
    Check(context.Observe(true),"reacquisition before next capture was ignored");
    rt::Pose head{};head.orientation_valid=head.position_valid=true;
    std::array<rt::EyeView,2> eyes{};eyes[0].eye=rt::Eye::left;eyes[1].eye=rt::Eye::right;
    for(auto& e:eyes){e.eye_to_head.orientation_valid=true;e.fov={-.7F,.8F,.6F,-.9F};}
    std::array<View,2> out{};
    Check(BuildViews(head,eyes,out),"valid captured optics rejected");
    Check(out[0].fov[0]==-.7F&&out[1].fov[3]==-.9F,"FOV radians changed");
    head.orientation={0,.70710678F,0,.70710678F};eyes[1].eye_to_head.orientation={0,.70710678F,0,.70710678F};
    Check(BuildViews(head,eyes,out)&&std::abs(out[1].orientation[1]-1.F)<.0001F,"head/eye composition incorrect");
    head.orientation_valid=false;Check(!BuildViews(head,eyes,out),"invalid capture pose accepted");
    head.orientation_valid=true;eyes[1].eye=rt::Eye::left;Check(!BuildViews(head,eyes,out),"duplicate eye admitted");
    auto folder=std::filesystem::temp_directory_path()/std::filesystem::path(L"CoJ-NeuralConfig-"+std::to_wstring(GetCurrentProcessId()));
    std::filesystem::create_directory(folder);
    struct Cleanup{std::filesystem::path root;~Cleanup(){std::error_code ec;std::filesystem::remove(root/L"config.ini",ec);std::filesystem::remove(root,ec);}}cleanup{folder};
    auto path=folder/L"config.ini";
    Check(!LoadOptions(path).enabled,"missing config enabled feature");
    WritePrivateProfileStringW(L"Neural",L"Enabled",L"1",path.c_str());
    WritePrivateProfileStringW(L"Neural",L"HostPath",L"C:\\local\\helper.exe",path.c_str());
    WritePrivateProfileStringW(L"Neural",L"RuntimeDirectory",L"C:\\local\\model",path.c_str());
    Check(LoadOptions(path).enabled,"valid explicit config rejected");
    WritePrivateProfileStringW(L"Neural",L"TimeoutMs",L"1000",path.c_str());
    Check(!LoadOptions(path).enabled,"unbounded frame deadline admitted");
    WritePrivateProfileStringW(L"Neural",L"TimeoutMs",L"8x",path.c_str());
    Check(!LoadOptions(path).enabled,"malformed timeout accepted");
    WritePrivateProfileStringW(L"Neural",L"TimeoutMs",L"8",path.c_str());
    WritePrivateProfileStringW(L"Neural",L"StageHeight",L"64",path.c_str());
    Check(!LoadOptions(path).enabled,"unqualified model stage admitted");
    WritePrivateProfileStringW(L"Neural",L"StageHeight",L"384",path.c_str());
    WritePrivateProfileStringW(L"Neural",L"HostPath",L"relative.exe",path.c_str());
    Check(!LoadOptions(path).enabled,"relative helper path accepted");
    std::cout<<"Neural candidate startup/optics checks passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}

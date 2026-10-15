#include "runtime/native_render_fault.hpp"
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <cstdlib>
#include <cctype>
#include <algorithm>
using namespace cojvr::runtime;
void Check(bool ok,const char* why){if(!ok){std::cerr<<why<<'\n';std::exit(1);}}
LONG Filter(EXCEPTION_POINTERS* pointers){return RecordNativeRenderFault(pointers,123);}
// No C++ objects with destructors in an SEH function. The outer native handler
// must receive precisely the exception observed by our non-handling filter.
bool Escape(){
    bool caught=false;
    __try {
        __try {ULONG_PTR args[]{1,0x12345678};RaiseException(EXCEPTION_ACCESS_VIOLATION,0,2,args);}
        __except(Filter(GetExceptionInformation())) {return false;}
    }__except(EXCEPTION_EXECUTE_HANDLER){caught=GetExceptionCode()==EXCEPTION_ACCESS_VIOLATION;}
    return caught;
}
std::string Read(const std::filesystem::path& path){std::ifstream f(path);return {std::istreambuf_iterator<char>(f),{}};}
int main(){
    const auto path=std::filesystem::temp_directory_path()/(L"cojvr-render-fault-host-"+std::to_wstring(GetCurrentProcessId())+L".txt");
    std::filesystem::remove(path);
    const std::string build(64,'A');
    Check(ConfigureNativeRenderFaultReporter(path,"host-run",build),"configure bound fault report");
    EXCEPTION_RECORD record{};CONTEXT context{};EXCEPTION_POINTERS pointers{&record,&context};
    record.ExceptionCode=0xE06D7363;
    Check(RecordNativeRenderFault(&pointers,1)==EXCEPTION_CONTINUE_SEARCH&&!std::filesystem::exists(path),"ordinary C++/JNI exceptions must not create a crash report");
    Check(RecordNativeRenderFault(nullptr,1)==EXCEPTION_CONTINUE_SEARCH,"unavailable exception must preserve native behavior");
    Check(Escape(),"diagnostic filter swallowed the hardware exception");
    const auto report=Read(path);
    Check(report.find("run_id=host-run")!=std::string::npos&&report.find("build_manifest_id="+build)!=std::string::npos,
        "report lost exact run/build identity");
    Check(report.find("exception_code=0xc0000005")!=std::string::npos&&report.find("access_address=0x12345678")!=std::string::npos&&
        report.find("access_operation=1")!=std::string::npos&&report.find("frame_sequence=123")!=std::string::npos&&
        report.find("exception_address=0x")!=std::string::npos&&report.find("module_rva=0x")!=std::string::npos,
        "fault addresses and scope not recorded");
    const auto instruction=report.find("fault_instruction_hex=");
    Check(report.find("fault_instruction_bytes=16\n")!=std::string::npos&&
        instruction!=std::string::npos&&
        instruction+22+32<=report.size()&&
        std::all_of(report.begin()+instruction+22,report.begin()+instruction+22+32,
            [](char c){return std::isxdigit(static_cast<unsigned char>(c))!=0;}),
        "escaped fault must preserve a bounded readable instruction window");
    Check(Escape()&&Read(path)==report,"nested/repeated faults replaced the first report or changed propagation");
    const auto unavailable_path=std::filesystem::path(path.wstring()+L".unmapped");
    std::filesystem::remove(unavailable_path);
    Check(ConfigureNativeRenderFaultReporter(unavailable_path,"host-unmapped",build),"reconfigure unmapped instruction case");
    EXCEPTION_RECORD bad_record{};CONTEXT bad_context{};
    bad_record.ExceptionCode=EXCEPTION_ACCESS_VIOLATION;
    bad_record.ExceptionAddress=reinterpret_cast<void*>(1);
    bad_context.Eip=1;
    EXCEPTION_POINTERS bad_pointers{&bad_record,&bad_context};
    SetLastError(0x4eed);
    Check(RecordNativeRenderFault(&bad_pointers,1)==EXCEPTION_CONTINUE_SEARCH,
        "unreadable instruction must preserve exception propagation");
    Check(GetLastError()==0x4eed,"fault diagnosis must preserve thread last-error state");
    const auto unavailable=Read(unavailable_path);
    Check(unavailable.find("fault_instruction_bytes=0\n")!=std::string::npos&&
        unavailable.find("fault_instruction_hex=unavailable\n")!=std::string::npos,
        "unmapped fault instructions must be marked unavailable without dereferencing them");
    Check(!ConfigureNativeRenderFaultReporter(path,"unbound",build),"unbound diagnostic report admitted");
    Check(Escape()&&Read(path)==report,"disabled report changed exception behavior or old evidence");
    std::filesystem::remove(unavailable_path);
    std::filesystem::remove(path);
    std::cout<<"Bound render-fault reporting and native exception propagation passed\n";
}

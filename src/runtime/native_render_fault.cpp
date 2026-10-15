#include "runtime/native_render_fault.hpp"
#include <atomic>
#include <cstdio>
#include <algorithm>
#include <cstring>
namespace cojvr::runtime {
namespace {
wchar_t g_path[32768]{};
char g_run[128]{},g_build[65]{};
bool g_bound=false;
std::atomic<bool> g_attempted=false;
bool Token(std::string_view value) noexcept {
    return !value.empty()&&value.size()<128&&value!="unbound"&&std::all_of(value.begin(),value.end(),[](char c){
        return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.';});
}
}
bool ConfigureNativeRenderFaultReporter(const std::filesystem::path& path,
    std::string_view run,std::string_view build) noexcept {
    g_bound=false;g_attempted=false;
    if(path.empty()||!path.is_absolute()||path.native().size()>=std::size(g_path)||!Token(run)||build.size()!=64||
       !std::all_of(build.begin(),build.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F');}))return false;
    std::copy(path.native().begin(),path.native().end(),g_path);g_path[path.native().size()]=0;
    std::copy(run.begin(),run.end(),g_run);g_run[run.size()]=0;
    std::copy(build.begin(),build.end(),g_build);g_build[64]=0;
    g_bound=true;return true;
}
LONG RecordNativeRenderFault(EXCEPTION_POINTERS* pointers,std::uint64_t frame) noexcept {
    const DWORD previous_error=GetLastError();
    const auto continue_search=[&]() noexcept {
        SetLastError(previous_error);
        return EXCEPTION_CONTINUE_SEARCH;
    };
    if(!g_bound||!pointers||!pointers->ExceptionRecord||!pointers->ContextRecord)return continue_search();
    const auto& record=*pointers->ExceptionRecord;const auto& registers=*pointers->ContextRecord;
    if(record.ExceptionCode!=EXCEPTION_ACCESS_VIOLATION&&record.ExceptionCode!=EXCEPTION_IN_PAGE_ERROR)return continue_search();
    if(g_attempted.exchange(true))return continue_search();
    // Fixed storage and Win32 file writes avoid the telemetry mutex, JNI,
    // renderer/COM state and heap-backed stream formatting in a fault filter.
    MEMORY_BASIC_INFORMATION memory{};
    const bool mapped=VirtualQuery(record.ExceptionAddress,&memory,sizeof(memory))==sizeof(memory);
    const auto address=reinterpret_cast<std::uintptr_t>(record.ExceptionAddress);
    const auto module_base=mapped?reinterpret_cast<std::uintptr_t>(memory.AllocationBase):0;
    wchar_t module[260]{};char module_utf8[1040]{};
    if(module_base&&GetModuleFileNameW(reinterpret_cast<HMODULE>(module_base),module,static_cast<DWORD>(std::size(module))))
        WideCharToMultiByte(CP_UTF8,0,module,-1,module_utf8,static_cast<int>(std::size(module_utf8)),nullptr,nullptr);
    const auto operation=record.NumberParameters>=2?record.ExceptionInformation[0]:static_cast<ULONG_PTR>(-1);
    const auto accessed=record.NumberParameters>=2?record.ExceptionInformation[1]:0;
    // A faulting EIP is not safe to dereference inside an exception filter.
    // A bounded Win32 read can recover the exact instruction bytes for later
    // disassembly; a partially mapped or unreadable window is unavailable.
    std::uint8_t instruction[16]{};
    SIZE_T instruction_read=0;
    const bool instruction_valid=ReadProcessMemory(GetCurrentProcess(),
        reinterpret_cast<LPCVOID>(static_cast<std::uintptr_t>(registers.Eip)),
        instruction,sizeof(instruction),&instruction_read)!=FALSE&&
        instruction_read==sizeof(instruction);
    char instruction_hex[sizeof(instruction)*2+1]{};
    if(instruction_valid){
        constexpr char digits[]="0123456789abcdef";
        for(std::size_t i=0;i<sizeof(instruction);++i){
            instruction_hex[2*i]=digits[instruction[i]>>4];
            instruction_hex[2*i+1]=digits[instruction[i]&15];
        }
    }
    char text[4096]{};
    int size=std::snprintf(text,std::size(text),
        "schema_version=1\nscope=coj_render_hook_escape\nrun_id=%s\nbuild_manifest_id=%s\npid=%lu\nthread_id=%lu\nframe_sequence=%llu\n"
        "exception_code=0x%08lx\nexception_address=0x%08lx\nmodule_base=0x%08lx\nmodule_rva=0x%08lx\nmodule_path=%s\n"
        "access_operation=%lu\naccess_address=0x%08lx\neip=0x%08lx\nesp=0x%08lx\nebp=0x%08lx\neax=0x%08lx\nebx=0x%08lx\necx=0x%08lx\nedx=0x%08lx\n"
        "fault_instruction_bytes=%u\nfault_instruction_hex=%s\n",
        g_run,g_build,GetCurrentProcessId(),GetCurrentThreadId(),static_cast<unsigned long long>(frame),record.ExceptionCode,
        static_cast<unsigned long>(address),static_cast<unsigned long>(module_base),static_cast<unsigned long>(module_base?address-module_base:0),module_utf8,
        static_cast<unsigned long>(operation),static_cast<unsigned long>(accessed),registers.Eip,registers.Esp,registers.Ebp,
        registers.Eax,registers.Ebx,registers.Ecx,registers.Edx,
        instruction_valid?static_cast<unsigned>(sizeof(instruction)):0U,
        instruction_valid?instruction_hex:"unavailable");
    if(size<=0||static_cast<std::size_t>(size)>=std::size(text))return continue_search();
    void* stack[24]{};const auto count=CaptureStackBackTrace(0,static_cast<DWORD>(std::size(stack)),stack,nullptr);
    for(USHORT i=0;i<count&&static_cast<std::size_t>(size)+40<std::size(text);++i)
        size+=std::snprintf(text+size,std::size(text)-size,"stack_%u=0x%08lx\n",i,static_cast<unsigned long>(reinterpret_cast<std::uintptr_t>(stack[i])));
    const HANDLE file=CreateFileW(g_path,GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file!=INVALID_HANDLE_VALUE){DWORD written=0;(void)WriteFile(file,text,static_cast<DWORD>(size),&written,nullptr);CloseHandle(file);}
    return continue_search();
}
}

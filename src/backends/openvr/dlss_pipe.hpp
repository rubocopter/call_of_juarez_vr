#pragma once
#include <windows.h>
#include <algorithm>
#include <cstdint>

namespace cojvr::backends::openvr::dlss {
struct Handle {
    HANDLE value=INVALID_HANDLE_VALUE;
    Handle()=default;
    explicit Handle(HANDLE h) noexcept:value(h){}
    ~Handle(){Reset();}
    Handle(const Handle&)=delete;Handle& operator=(const Handle&)=delete;
    void Reset(HANDLE h=INVALID_HANDLE_VALUE) noexcept {
        if(value&&value!=INVALID_HANDLE_VALUE)CloseHandle(value);value=h;
    }
};
// Byte pipes with overlapped I/O. One deadline covers the whole transfer;
// cancellation is drained before stack-owned OVERLAPPED/event storage dies.
inline bool Transfer(HANDLE pipe,void* bytes,std::uint32_t size,bool write,
                     DWORD timeout_ms,HANDLE peer=nullptr) noexcept {
    Handle event(CreateEventW(nullptr,TRUE,FALSE,nullptr));if(!event.value)return false;
    const auto end=GetTickCount64()+timeout_ms;auto* cursor=static_cast<unsigned char*>(bytes);
    while(size) {
        OVERLAPPED io{};io.hEvent=event.value;ResetEvent(event.value);DWORD done=0;
        const BOOL immediate=write?WriteFile(pipe,cursor,size,&done,&io):ReadFile(pipe,cursor,size,&done,&io);
        if(!immediate) {
            if(GetLastError()!=ERROR_IO_PENDING)return false;
            const auto now=GetTickCount64();HANDLE waits[2]{event.value,peer};
            const auto wait=WaitForMultipleObjects(peer?2U:1U,waits,FALSE,
                timeout_ms==INFINITE?INFINITE:now>=end?0:static_cast<DWORD>(end-now));
            if(wait!=WAIT_OBJECT_0) {
                CancelIoEx(pipe,&io);GetOverlappedResult(pipe,&io,&done,TRUE);return false;
            }
            if(!GetOverlappedResult(pipe,&io,&done,FALSE))return false;
        }
        if(!done||done>size)return false;cursor+=done;size-=done;
    }
    return true;
}
}

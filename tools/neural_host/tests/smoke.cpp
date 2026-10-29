// SPDX-License-Identifier: GPL-3.0-or-later
// GPU/model-free process tests. The parent laboratory owns rendering tests.
#include <windows.h>
#include "neural_protocol.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace protocol = cojvr::backends::openvr::neural;
static void Check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
struct Handle {
    HANDLE value{};
    ~Handle() { if (value) CloseHandle(value); }
    Handle() = default;
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
};
struct Session {
    Handle mapping, ready, request, complete, stop, child;
    protocol::Header* frame{};
    std::wstring base;
    Session(const wchar_t* exe, const wchar_t* mode, DWORD parent = GetCurrentProcessId(), bool stopped = false) {
        static unsigned sequence{};
        base = L"Local\\CoJNeuralSmoke-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(++sequence);
        mapping.value = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
            static_cast<DWORD>(protocol::mapping_bytes), (base + L"-map").c_str());
        Check(mapping.value != nullptr, "create mapping");
        frame = static_cast<protocol::Header*>(MapViewOfFile(mapping.value, FILE_MAP_ALL_ACCESS, 0, 0, protocol::mapping_bytes));
        Check(frame != nullptr, "map view");
        *frame = protocol::Header{};
        frame->width = 32; frame->height = 48;
        frame->input = {1, 2, 3, 4, 5, 6}; frame->request = 1;
        ready.value = CreateEventW(nullptr, TRUE, FALSE, (base + L"-ready").c_str());
        request.value = CreateEventW(nullptr, FALSE, stopped, (base + L"-request").c_str());
        complete.value = CreateEventW(nullptr, TRUE, FALSE, (base + L"-complete").c_str());
        stop.value = CreateEventW(nullptr, TRUE, stopped, (base + L"-stop").c_str());
        Check(ready.value && request.value && complete.value && stop.value, "create events");
        std::wstring command = L"\"" + std::wstring(exe) + L"\" \"" + base + L"\" " + std::to_wstring(parent) + L" \"missing-runtime\" " + mode;
        STARTUPINFOW startup{sizeof(startup)};
        PROCESS_INFORMATION process{};
        Check(CreateProcessW(exe, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process) != FALSE,
            "start helper");
        CloseHandle(process.hThread); child.value = process.hProcess;
    }
    ~Session() {
        if (stop.value) SetEvent(stop.value);
        if (child.value && WaitForSingleObject(child.value, 3000) != WAIT_OBJECT_0) {
            TerminateProcess(child.value, 99); WaitForSingleObject(child.value, 3000);
        }
        if (frame) UnmapViewOfFile(frame);
    }
    void Ready(unsigned expected = 0) {
        HANDLE waits[]{ready.value, child.value};
        Check(WaitForMultipleObjects(2, waits, FALSE, 5000) == WAIT_OBJECT_0, "helper exited or timed out before ready");
        Check(frame->process_bits == 64 && frame->result == expected, "ready result/process bits");
        Check(WaitForSingleObject(ready.value, 0) == WAIT_OBJECT_0, "ready must remain signaled");
    }
    std::vector<unsigned char> Fill() {
        std::vector<unsigned char> bytes(protocol::eye_bytes(frame->width, frame->height) * 2);
        for (std::size_t i = 0; i < bytes.size(); ++i) bytes[i] = static_cast<unsigned char>((i * 37 + i / 173) % 256);
        std::memcpy(protocol::input_pixels(frame), bytes.data(), bytes.size());
        std::memset(protocol::output_pixels(frame), 0xcd, bytes.size());
        return bytes;
    }
    void Submit(std::uint64_t id) {
        ResetEvent(complete.value); frame->request = id; frame->input.frame = id;
        frame->process_bits = 0; // Response belongs to the helper, including bitness.
        Check(SetEvent(request.value) != FALSE, "signal request");
    }
    void Complete(unsigned result = 1) {
        HANDLE waits[]{complete.value, child.value};
        Check(WaitForMultipleObjects(2, waits, FALSE, 5000) == WAIT_OBJECT_0, "request failed to complete");
        Check(frame->result == result && frame->completed == frame->request, "completion result/request");
        Check(frame->process_bits == 64, "helper failed to publish per-response bitness");
        Check(protocol::Same(frame->input, frame->output), "completion identity snapshot");
        Check(WaitForSingleObject(complete.value, 0) == WAIT_OBJECT_0, "complete must remain signaled");
    }
    void Exit(unsigned expected = 0) {
        Check(WaitForSingleObject(child.value, 3000) == WAIT_OBJECT_0, "helper did not exit");
        DWORD code{}; Check(GetExitCodeProcess(child.value, &code) && code == expected, "unexpected helper exit code");
    }
};

int wmain(int argc, wchar_t** argv) {
    if (argc != 2) return 2;
    try {
        for (const auto* mode : {L"identity", L"edit"}) {
            Session s(argv[1], mode); s.Ready(); const auto input = s.Fill();
            auto expected = input;
            if (std::wstring(mode) == L"edit")
                for (std::size_t i = 0; i < expected.size(); ++i)
                    if (i % 4 != 3) expected[i] = static_cast<unsigned char>(std::min(255, int(expected[i]) + 16));
            s.Submit(1); s.Complete();
            Check(protocol::Completed(*s.frame), "valid pair rejected");
            Check(!std::memcmp(protocol::input_pixels(s.frame), input.data(), input.size()), "input modified");
            Check(!std::memcmp(protocol::output_pixels(s.frame), expected.data(), expected.size()), "wrong paired output or alpha");
            s.frame->width = protocol::max_extent + 1; s.Submit(2); s.Complete(2);
            Check(!std::memcmp(protocol::output_pixels(s.frame), expected.data(), expected.size()), "invalid request published pixels");
            s.frame->width = 64; s.frame->height = 32; ++s.frame->input.epoch;
            const auto resized = s.Fill(); s.Submit(3); s.Complete();
            Check(protocol::Completed(*s.frame), "resized new epoch failed");
            SetEvent(s.stop.value); s.Exit();
        }
        {
            Session s(argv[1], L"delay"); s.Ready(); s.Fill(); s.Submit(1); s.Complete();
            s.Submit(2);
            Check(WaitForSingleObject(s.complete.value, 30) == WAIT_TIMEOUT, "request 2 was not delayed");
            s.Complete(); ++s.frame->input.epoch; s.Submit(3); s.Complete();
        }
        {
            Session s(argv[1], L"stale"); s.Ready(); s.Fill(); s.Submit(1); s.Complete(); s.Submit(2);
            Check(WaitForSingleObject(s.complete.value, 5000) == WAIT_OBJECT_0, "stale completion missing");
            Check(s.frame->result == 1 && s.frame->completed == 2 && !protocol::Completed(*s.frame), "stale output accepted");
            Check(s.frame->output.revision != s.frame->input.revision, "stale revision not changed");
            s.Submit(3); s.Complete();
        }
        {
            Session s(argv[1], L"fault"); s.Ready(); s.Fill(); s.Submit(1); s.Complete(); s.Submit(2); s.Exit(23);
            Check(WaitForSingleObject(s.complete.value, 0) == WAIT_TIMEOUT, "fault published completion");
        }
        {
            Session s(argv[1], L"identity", GetCurrentProcessId(), true); s.Ready(); s.Exit();
            Check(WaitForSingleObject(s.complete.value, 0) == WAIT_TIMEOUT, "request won over stop");
        }
        {
            Session s(argv[1], L"identity", 0); s.Ready(9); s.Exit(9);
        }
        {
            Session s(argv[1], L"unknown"); s.Ready(9); s.Exit(9);
        }
        {
            Session s(argv[1], L"model"); s.Ready(9); s.Exit(9);
        }
        std::puts("PASS: synthetic modes, paired bytes/alpha, invalid request, epoch/resize, stop priority, init failure");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what()); return 1;
    }
}

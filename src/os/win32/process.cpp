// bof3::os, the process, on Windows: SetEnvironmentVariable and CreateProcessW.
// See os/process.h.

#include "os/process.h"

#include <windows.h>

#include <cwctype>

namespace bof3::os {

bool SetEnv(const char* name, const char* value) noexcept { return SetEnvironmentVariableA(name, value) != 0; }
bool SetEnvW(const wchar_t* name, const wchar_t* value) noexcept { return SetEnvironmentVariableW(name, value) != 0; }

namespace {

// The name of a block entry "NAME=value" (an entry that starts with '=' - the
// drive's folders, "=C:=C:\..." - keeps its first '=' in the name).
std::wstring EntryName(const std::wstring& e) {
    const std::size_t eq = e.find(L'=', 1);
    return eq == std::wstring::npos ? e : e.substr(0, eq);
}

bool SameVar(const std::wstring& a, const std::wstring& b) {   // Windows' names: case does not count
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (std::towupper(a[i]) != std::towupper(b[i])) return false;
    return true;
}

// This process's environment with `add` over it, as a block for
// CREATE_UNICODE_ENVIRONMENT: entries ended by a 0, the block by one more.
bool EnvBlock(const std::vector<std::pair<std::wstring, std::wstring>>& add, std::vector<wchar_t>& block) {
    std::vector<std::wstring> entries;
    if (wchar_t* env = GetEnvironmentStringsW()) {
        for (const wchar_t* p = env; *p; p += std::wcslen(p) + 1) entries.emplace_back(p);
        FreeEnvironmentStringsW(env);
    } else {
        return false;
    }
    for (const auto& [name, value] : add) {
        for (std::size_t i = 0; i < entries.size();)
            if (SameVar(EntryName(entries[i]), name)) entries.erase(entries.begin() + static_cast<std::ptrdiff_t>(i));
            else ++i;
        if (!value.empty()) entries.push_back(name + L"=" + value);
    }
    block.clear();
    for (const std::wstring& e : entries) {
        block.insert(block.end(), e.begin(), e.end());
        block.push_back(L'\0');
    }
    if (entries.empty()) block.push_back(L'\0');
    block.push_back(L'\0');
    return true;
}

}  // namespace

bool ChildStart(const ChildSpec& spec, Child* child, std::uint32_t* error) noexcept {
    try {
        *child = Child{};
        std::wstring line = L"\"" + spec.program + L"\"";
        for (const std::wstring& a : spec.args) line += L" \"" + a + L"\"";
        std::vector<wchar_t> block;
        if (!spec.env.empty() && !EnvBlock(spec.env, block)) {
            if (error) *error = GetLastError();
            return false;
        }
        STARTUPINFOW si{};
        si.cb = sizeof si;
        HANDLE rd = nullptr, wr = nullptr, nul = INVALID_HANDLE_VALUE;
        if (spec.capture) {
            SECURITY_ATTRIBUTES sa{sizeof sa, nullptr, TRUE};
            if (!CreatePipe(&rd, &wr, &sa, 0)) {
                if (error) *error = GetLastError();
                return false;
            }
            SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
            nul = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr);
            si.dwFlags = STARTF_USESTDHANDLES;
            si.hStdInput = nul;
            si.hStdOutput = wr;
            si.hStdError = wr;
        }
        DWORD flags = 0;
        if (spec.suspended) flags |= CREATE_SUSPENDED;
        if (spec.no_window) flags |= CREATE_NO_WINDOW;
        if (!block.empty()) flags |= CREATE_UNICODE_ENVIRONMENT;
        PROCESS_INFORMATION pi{};
        const BOOL ok = CreateProcessW(spec.program.c_str(), line.data(), nullptr, nullptr, spec.capture ? TRUE : FALSE,
                                       flags, block.empty() ? nullptr : block.data(),
                                       spec.directory.empty() ? nullptr : spec.directory.c_str(), &si, &pi);
        const DWORD why = ok ? 0 : GetLastError();
        if (wr) CloseHandle(wr);
        if (nul != INVALID_HANDLE_VALUE) CloseHandle(nul);
        if (!ok) {
            if (rd) CloseHandle(rd);
            if (error) *error = why;
            return false;
        }
        child->process = reinterpret_cast<std::intptr_t>(pi.hProcess);
        child->thread = reinterpret_cast<std::intptr_t>(pi.hThread);
        child->out = rd ? reinterpret_cast<std::intptr_t>(rd) : -1;
        child->pid = pi.dwProcessId;
        return true;
    } catch (...) {
        if (error) *error = ERROR_NOT_ENOUGH_MEMORY;
        return false;
    }
}

bool ChildResume(Child& child) noexcept {
    return child.thread != -1 && ResumeThread(reinterpret_cast<HANDLE>(child.thread)) != static_cast<DWORD>(-1);
}

long long ChildRead(Child& child, void* to, std::size_t n) noexcept {
    if (child.out == -1) return -1;
    DWORD got = 0;
    const DWORD want = n > 0x7FFFFFFF ? 0x7FFFFFFF : static_cast<DWORD>(n);
    if (!ReadFile(reinterpret_cast<HANDLE>(child.out), to, want, &got, nullptr))
        return GetLastError() == ERROR_BROKEN_PIPE ? 0 : -1;   // the child's end closed
    return got;
}

bool ChildWait(Child& child, std::uint32_t* code) noexcept {
    if (child.process == -1) return false;
    const HANDLE p = reinterpret_cast<HANDLE>(child.process);
    if (WaitForSingleObject(p, INFINITE) != WAIT_OBJECT_0) return false;
    DWORD c = 1;
    if (!GetExitCodeProcess(p, &c)) return false;
    if (code) *code = c;
    return true;
}

bool ChildEnd(Child& child, std::uint32_t code) noexcept {
    return child.process != -1 && TerminateProcess(reinterpret_cast<HANDLE>(child.process), code);
}

void ChildClose(Child& child) noexcept {
    for (std::intptr_t* h : {&child.out, &child.thread, &child.process})
        if (*h != -1) {
            CloseHandle(reinterpret_cast<HANDLE>(*h));
            *h = -1;
        }
}

}  // namespace bof3::os

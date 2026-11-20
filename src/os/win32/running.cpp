// bof3::os, what runs on Windows: the toolhelp snapshot of the processes, and
// FindWindowW. See os/running.h.
#include "os/running.h"

#include <windows.h>
#include <tlhelp32.h>

#include <cwchar>

namespace bof3::os {

namespace {

// UTF-8 as a wide name; false when it does not convert or does not fit.
bool Wide(const char* text, wchar_t* out, int room) {
    const int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, out, room);
    return n > 0;
}

}  // namespace

int ProgramsRunning(const char* file) noexcept {
    wchar_t want[MAX_PATH];
    if (!file || !Wide(file, want, MAX_PATH)) {
        SetLastError(ERROR_INVALID_NAME);
        return -1;
    }
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return -1;
    PROCESSENTRY32W e;
    e.dwSize = sizeof e;
    int count = 0;
    for (BOOL more = Process32FirstW(snap, &e); more; more = Process32NextW(snap, &e))
        if (CompareStringOrdinal(e.szExeFile, -1, want, -1, TRUE) == CSTR_EQUAL) ++count;
    const DWORD error = GetLastError();
    CloseHandle(snap);
    if (error != ERROR_NO_MORE_FILES) {   // the walk did not reach its end
        SetLastError(error);
        return -1;
    }
    return count;
}

bool WindowClassOpen(const char* name) noexcept {
    wchar_t cls[256];
    if (!name || !Wide(name, cls, 256)) {
        SetLastError(ERROR_INVALID_NAME);
        return false;
    }
    return FindWindowW(cls, nullptr) != nullptr;
}

}  // namespace bof3::os

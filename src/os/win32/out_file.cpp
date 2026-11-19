// bof3::os unbuffered output files on Windows: CreateFileW, WriteFile. See os/out_file.h.

#include "os/out_file.h"
#include "os/os.h"

#include <windows.h>

namespace bof3::os {

namespace {
HANDLE H(OutFile f) { return reinterpret_cast<HANDLE>(f); }
}  // namespace

OutFile OutCreate(const std::filesystem::path& path) noexcept {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    return h == INVALID_HANDLE_VALUE ? kNoFile : reinterpret_cast<OutFile>(h);
}

// One WriteFile: a synchronous write to a disk file takes it all or fails.
// One that succeeds with fewer bytes taken has no error of its own, and the
// thread's last error is some earlier call's: cleared, so LastError() says 0
// and no message quotes a number that is not this write's (os/out_file.h).
bool OutWrite(OutFile f, const void* bytes, std::size_t n) noexcept {
    if (f == kNoFile) {
        SetLastError(ERROR_INVALID_HANDLE);
        return false;
    }
    DWORD written = 0;
    if (!WriteFile(H(f), bytes, static_cast<DWORD>(n), &written, nullptr)) return false;
    if (written != n) {
        SetLastError(ERROR_SUCCESS);
        return false;
    }
    return true;
}

bool OutFlush(OutFile f) noexcept { return f != kNoFile && FlushFileBuffers(H(f)); }

void OutClose(OutFile f) noexcept {
    if (f != kNoFile) CloseHandle(H(f));
}

}  // namespace bof3::os

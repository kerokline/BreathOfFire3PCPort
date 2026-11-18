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
bool OutWrite(OutFile f, const void* bytes, std::size_t n) noexcept {
    DWORD written = 0;
    return f != kNoFile && WriteFile(H(f), bytes, static_cast<DWORD>(n), &written, nullptr) && written == n;
}

bool OutFlush(OutFile f) noexcept { return f != kNoFile && FlushFileBuffers(H(f)); }

void OutClose(OutFile f) noexcept {
    if (f != kNoFile) CloseHandle(H(f));
}

}  // namespace bof3::os

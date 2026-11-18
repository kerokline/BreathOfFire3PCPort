// bof3::os unbuffered output files on Linux: open, write, fsync, close. See os/out_file.h.

#include "os/out_file.h"
#include "os/os.h"

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

namespace bof3::os {

OutFile OutCreate(const std::filesystem::path& path) noexcept {
    const int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    return fd < 0 ? kNoFile : static_cast<OutFile>(fd);
}

// write() may take part of a buffer (a signal, a pipe): the rest is written after it.
bool OutWrite(OutFile f, const void* bytes, std::size_t n) noexcept {
    if (f == kNoFile) return false;
    const char* p = static_cast<const char*>(bytes);
    while (n) {
        const ssize_t w = write(static_cast<int>(f), p, n);
        if (w < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        p += w;
        n -= static_cast<std::size_t>(w);
    }
    return true;
}

bool OutFlush(OutFile f) noexcept { return f != kNoFile && fsync(static_cast<int>(f)) == 0; }

void OutClose(OutFile f) noexcept {
    if (f != kNoFile) close(static_cast<int>(f));
}

}  // namespace bof3::os

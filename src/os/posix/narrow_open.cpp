// bof3::os::OpenNarrow on POSIX: fopen on Path's form of the name (the
// backslash a separator; Path's own case check before the open). See
// os/narrow_open.h.
#include "os/narrow_open.h"

#include <cerrno>
#include <filesystem>

#include "os/file.h"

namespace bof3::os {

std::FILE* OpenNarrow(const char* narrow, const char* mode) noexcept {
    if (!narrow || !mode) {
        errno = EINVAL;
        return nullptr;
    }
    const std::filesystem::path p = Path(narrow);
    if (p.empty()) {
        errno = ENOENT;   // an empty name names no file, as fopen("") answers
        return nullptr;
    }
    return std::fopen(p.c_str(), mode);
}

}  // namespace bof3::os

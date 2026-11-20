// bof3::os::OpenNarrow on Windows: std::fopen on the narrow name, then the
// case check. See os/narrow_open.h.
#include "os/narrow_open.h"

#include <cerrno>

#include "os/case_check.h"

namespace bof3::os {

std::FILE* OpenNarrow(const char* narrow, const char* mode) noexcept {
    std::FILE* const f = std::fopen(narrow, mode);
    if (f && CaseCheckOn()) {
        const int error = errno;   // what fopen left, as the caller would read it
        CaseCheckNarrow(narrow);
        errno = error;
    }
    return f;
}

}  // namespace bof3::os

// The reason a media function gives when it refuses: printf-style into the
// caller's buffer, which may be null or empty.
#pragma once

#include <cstdarg>
#include <cstddef>
#include <cstdio>

namespace media {

inline bool Fail(char* why, std::size_t why_n, const char* fmt, ...) {
    if (why != nullptr && why_n > 0) {
        std::va_list ap;
        va_start(ap, fmt);
        std::vsnprintf(why, why_n, fmt, ap);
        va_end(ap);
    }
    return false;
}

}  // namespace media

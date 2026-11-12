// bof3::os narrow names on POSIX: the bytes themselves (UTF-8 by convention; the
// kernel does not look), with the game's backslash read as a separator. See
// os/file.h.

#include "os/file.h"

#include <algorithm>

namespace bof3::os {

std::filesystem::path Path(const char* narrow) noexcept {
    try {
        std::string s(narrow ? narrow : "");
        std::replace(s.begin(), s.end(), '\\', '/');
        return std::filesystem::path(std::move(s));
    } catch (...) {
        return std::filesystem::path();
    }
}

std::string Narrow(const std::filesystem::path& path) noexcept {
    try {
        return path.native();
    } catch (...) {
        return std::string();
    }
}

}  // namespace bof3::os

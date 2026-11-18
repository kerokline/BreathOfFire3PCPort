// bof3::os::OpenPath on Windows: _wfopen on the path's wide name. See
// os/path_file.h.
#include "os/path_file.h"

#include <cerrno>
#include <cwchar>

namespace bof3::os {

std::FILE* OpenPath(const std::filesystem::path& path, const char* mode) noexcept {
    wchar_t wide[16];
    unsigned n = 0;
    for (; mode[n]; ++n) {
        if (n + 1 >= sizeof wide / sizeof wide[0]) {
            errno = EINVAL;
            return nullptr;
        }
        wide[n] = static_cast<wchar_t>(static_cast<unsigned char>(mode[n]));
    }
    wide[n] = 0;
    return _wfopen(path.c_str(), wide);
}

}  // namespace bof3::os

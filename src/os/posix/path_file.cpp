// bof3::os::OpenPath on POSIX: fopen on the path's bytes. See os/path_file.h.
#include "os/path_file.h"

namespace bof3::os {

std::FILE* OpenPath(const std::filesystem::path& path, const char* mode) noexcept { return std::fopen(path.c_str(), mode); }

}  // namespace bof3::os

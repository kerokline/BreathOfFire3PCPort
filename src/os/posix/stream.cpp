// bof3::os::StreamDescriptor / DescriptorLength on POSIX: fileno and fstat.
// The length is narrowed to 32 bits as MSVC's _filelength answers it.
#include "os/stream.h"

#include <sys/stat.h>

namespace bof3::os {

int StreamDescriptor(std::FILE* stream) noexcept { return fileno(stream); }

std::int32_t DescriptorLength(int descriptor) noexcept {
    struct stat st;
    if (fstat(descriptor, &st) != 0) return -1;
    return static_cast<std::int32_t>(st.st_size);
}

}  // namespace bof3::os

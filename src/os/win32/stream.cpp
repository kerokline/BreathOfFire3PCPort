// bof3::os::StreamDescriptor / DescriptorLength on Windows: the C runtime's
// _fileno and _filelength, as the game's file layer called them before the
// seam.
#include "os/stream.h"

#include <io.h>

namespace bof3::os {

int StreamDescriptor(std::FILE* stream) noexcept { return _fileno(stream); }

std::int32_t DescriptorLength(int descriptor) noexcept { return static_cast<std::int32_t>(_filelength(descriptor)); }

}  // namespace bof3::os

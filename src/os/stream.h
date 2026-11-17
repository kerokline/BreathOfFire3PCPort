// bof3::os, two questions about a C stream the standard library does not
// answer: its descriptor's number, and the length of the file behind a
// descriptor. The game's file layer asks both (Crt_fileno, Crt_filelength:
// File_Size is _filelength(_fileno(stream))).
#pragma once

#include <cstdint>
#include <cstdio>

namespace bof3::os {

// The descriptor of an open stream (MSVC's _fileno, POSIX's fileno).
int StreamDescriptor(std::FILE* stream) noexcept;
// The length in bytes of the file an open descriptor names, as a 32-bit
// number (MSVC's _filelength); -1 when the descriptor is not one.
std::int32_t DescriptorLength(int descriptor) noexcept;

}  // namespace bof3::os

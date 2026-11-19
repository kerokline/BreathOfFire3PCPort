// bof3::os, a C stream on a narrow name (os/file.h's narrow encoding): the
// game's own fopen (crt_rest.cpp's Crt_fopen, MSVC 6's 0x5B9B6D), which every
// name the game opens through File_Open passes - "DAT\\AREA000.EMI", a save,
// a cache path dat_cache or music_seq built with backslashes.
//
// Windows: std::fopen on the name itself, as Crt_fopen called it before this
// header - the C runtime reads the name in the code page the A file calls use,
// so nothing a Windows player can tell changes; then the case check
// (os/case_check.h) on a name that opened. POSIX: fopen on Path's form of the
// name, so the game's backslash reads as a separator there as everywhere else
// in the seam (Path also gives the case check its look before the open).
// The case of a name is NOT folded here (docs/linux-round.md, LB8: a name is
// fixed at its source; the fold for the player's own files is wave B's).
// Part of the operating-system seam (docs/linux-round.md, LB1): what the C++20
// standard library cannot do, behind one header per service, with a backend per
// system in win32/ and posix/. No Windows or POSIX type appears in a header, and
// nothing here logs or stops: the caller owns the message (bof3::Fatal).
#pragma once

#include <cstdio>

namespace bof3::os {

// Null on an error (errno), as std::fopen.
std::FILE* OpenNarrow(const char* narrow, const char* mode) noexcept;

}  // namespace bof3::os

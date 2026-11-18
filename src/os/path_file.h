// bof3::os, a C stream on a std::filesystem::path: the one open the standard
// library lacks, since std::fopen takes a narrow name and a wide or UTF-8 path
// (the launcher's settings, a module's folder, a BOF3X_* path read with
// GetEnvW) does not cross the narrow encoding on Windows (os/file.h). The
// writers that use it: the WAV dump and the stall log (hook/wav_dump.cpp,
// hook/stallwatch.cpp). For a narrow name, std::fopen or the game's Crt_fopen
// stay right.
//
// Windows: _wfopen on the path's wide name, the mode widened. POSIX: fopen on
// its bytes. The mode is fopen's, C11's "x" included ("wbx": fail when the
// file is there), which both C libraries take.
// Part of the operating-system seam (docs/linux-round.md, LB1): what the C++20
// standard library cannot do, behind one header per service, with a backend per
// system in win32/ and posix/. No Windows or POSIX type appears in a header, and
// nothing here logs or stops: the caller owns the message (bof3::Fatal).
#pragma once

#include <cstdio>
#include <filesystem>

namespace bof3::os {

// Null on an error (errno, and on Windows LastError too).
std::FILE* OpenPath(const std::filesystem::path& path, const char* mode) noexcept;

}  // namespace bof3::os

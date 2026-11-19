// A file written straight to the system, with no buffer of the process's own.
//
// For the log (src/hook/log.cpp): every write is in the system's hands when
// the call returns, so a process that dies the next instant - a crash, a
// TerminateProcess - loses nothing it wrote, and no C runtime lock stands
// between a write and the crash path. A std::ofstream or a FILE* buffers in
// the process and locks the stream.
//
// Windows: CreateFileW (CREATE_ALWAYS, others may read it while it is open,
// not write it), WriteFile, FlushFileBuffers, CloseHandle. POSIX: open
// (O_CREAT | O_TRUNC), write, fsync, close.
// Part of the operating-system seam (docs/linux-round.md, LB1): what the C++20
// standard library cannot do, behind one header per service, with a backend per
// system in win32/ and posix/. No Windows or POSIX type appears in a header, and
// nothing here logs or stops: the caller owns the message (bof3::Fatal).
#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>

namespace bof3::os {

using OutFile = std::intptr_t;
inline constexpr OutFile kNoFile = -1;   // INVALID_HANDLE_VALUE's value; no descriptor is -1

// The file made empty, or made, for writing. kNoFile on failure (LastError).
OutFile OutCreate(const std::filesystem::path& path) noexcept;
// `n` bytes appended. False if the system took fewer: LastError is the
// system's refusal, or 0 where it took a part and reported no error (Windows;
// the POSIX backend writes the rest, so a false there always has its errno).
bool OutWrite(OutFile f, const void* bytes, std::size_t n) noexcept;
// What was written, onto the disk.
bool OutFlush(OutFile f) noexcept;
void OutClose(OutFile f) noexcept;

}  // namespace bof3::os

// The process: environment, own path, last error, thread and process numbers.
//
// GetEnv keeps GetEnvironmentVariableA's size contract, which 113 call sites
// rely on and std::getenv does not have. Its value is in the engine's narrow
// encoding (os/file.h): the ANSI code page on Windows, as before the seam, and
// the bytes of the environment on POSIX - so a path in BOF3X_* goes to Path
// and to the game's own file calls unchanged. GetEnvW is for the wide paths
// the launcher and common/ code hold.
// LastError, ThreadId and ProcessId have no standard spelling: std::thread::id
// does not print as a number, and the log lines and DIV messages cite one.
// Part of the operating-system seam (docs/linux-round.md, LB1): what the C++20
// standard library cannot do, behind one header per service, with a backend per
// system in win32/ and posix/. No Windows or POSIX type appears in a header, and
// nothing here logs or stops: the caller owns the message (bof3::Fatal).
#pragma once

#include <cstdint>
#include <string>

namespace bof3::os {

// The length without the terminator if the value fits `cap`, the length needed
// with it if not, 0 if the variable is not set. `buf` may be null with cap 0.
std::uint32_t GetEnv(const char* name, char* buf, std::uint32_t cap);
std::uint32_t GetEnvW(const wchar_t* name, wchar_t* buf, std::uint32_t cap);

// The running program's path, and the path of the module (the DLL, the shared
// object) that holds the address `inside`, with its load address if `base` is
// given. False if the path is not known. These two are UTF-8 on both systems
// (os/text.h's ToWide takes them back to a wide path); on Windows that is not
// the narrow encoding, so they are not names for Path or the game's file calls.
bool ExecutablePath(std::string* utf8);
bool ModulePath(const void* inside, std::string* utf8, std::uintptr_t* base = nullptr);

// The system's last error number: GetLastError, errno. For a message only.
std::uint32_t LastError();

std::uint32_t ThreadId();
std::uint32_t ProcessId();

}  // namespace bof3::os

// bof3::os, the process: this process's own environment, which a child
// inherits, and a child started with a working folder, more variables, its
// output on a pipe, and a wait for its exit code. For the launcher's start of
// the game (launcher/start.cpp), and later the importer and tools/mods.py
// (docs/launcher-portable.md, stage P0 and P4).
//
// Windows: SetEnvironmentVariableA / W, and CreateProcessW as the launcher
// called it before the seam - the program's full name, a command line of the
// program and each argument in double quotes with one space between them (no
// escaping: an argument holds no double quote), no security attributes, the
// handles inherited only when the output goes to a pipe, no flags but those a
// ChildSpec asks for, no environment block of its own unless `env` adds to
// it, the working folder given. POSIX: setenv / unsetenv, and fork and
// execve, the exec's failure carried back on a pipe that the exec closes, so
// that ChildStart is false when the program does not start, as on Windows.
//
// Part of the operating-system seam (docs/linux-round.md, LB1): what the C++20
// standard library cannot do, behind one header per service, with a backend per
// system in win32/ and posix/. No Windows or POSIX type appears in a header, and
// nothing here logs or stops: the caller owns the message (bof3::Fatal).
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace bof3::os {

// A variable of this process's environment set, or removed with a null value.
// SetEnv's name and value are in the narrow encoding (os/file.h: the ANSI code
// page on Windows, the bytes on POSIX); SetEnvW's are wide (UTF-8 on POSIX).
// False when the system refuses (LastError).
bool SetEnv(const char* name, const char* value) noexcept;
bool SetEnvW(const wchar_t* name, const wchar_t* value) noexcept;

struct ChildSpec {
    std::wstring program;                 // the file to run, its full name
    std::vector<std::wstring> args;       // after the program's own name; none holds a double quote
    std::wstring directory;               // the working folder; empty: this process's
    // Set for the child only, over a copy of this process's environment; an
    // empty value removes the variable. Empty: the child gets the environment
    // as it is (Windows: no environment block of its own).
    std::vector<std::pair<std::wstring, std::wstring>> env;
    bool capture = false;     // stdout and stderr on one pipe (ChildRead), stdin the null device
    bool no_window = false;   // Windows: CREATE_NO_WINDOW (no console for a console program). POSIX: no meaning
    bool suspended = false;   // Windows: CREATE_SUSPENDED, until ChildResume. POSIX: ChildStart refuses it (EINVAL)
};

// A started child. `process` and `thread` are the system's own (Windows: the
// two handles; POSIX: the process id, and no thread), for a caller that is a
// Windows file by design; everything else goes through the calls below.
struct Child {
    std::intptr_t process = -1;
    std::intptr_t thread = -1;
    std::intptr_t out = -1;   // the read end of the pipe, when captured
    std::uint32_t pid = 0;
};

// Starts the child. False, with nothing to close, when it does not start: the
// system's error number in `error` (GetLastError, errno).
bool ChildStart(const ChildSpec& spec, Child* child, std::uint32_t* error) noexcept;
// A suspended child's first thread let run. False when the system refuses.
bool ChildResume(Child& child) noexcept;
// Up to `n` bytes of the captured output. The count read, 0 when the child has
// closed its end (it ended, or closed its output), -1 on an error or when the
// output is not captured.
long long ChildRead(Child& child, void* to, std::size_t n) noexcept;
// Waits for the child's end. Its exit code: the code it gave on Windows (a
// process ended by an exception has the exception's code), the code it gave
// on POSIX, and there 128 and the signal's number for a child a signal ended.
// False when the wait fails.
bool ChildWait(Child& child, std::uint32_t* code) noexcept;
// Ends the child at once with exit code `code` (Windows: TerminateProcess;
// POSIX: SIGKILL, so its code is 128 + 9 whatever `code` says).
bool ChildEnd(Child& child, std::uint32_t code) noexcept;
// What the child holds of ours closed (the handles, the pipe). The child goes on.
void ChildClose(Child& child) noexcept;

}  // namespace bof3::os

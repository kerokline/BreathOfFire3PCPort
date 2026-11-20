// bof3::os, what runs on the system: the processes of a program, by the
// program's file name, and a window, by its class. For the save editor's
// refusal to write a save the game holds (launcher/ui/page_saves.cpp; the
// Win32 editor asked FindWindowW for the class the game registers, "Bof3":
// docs/known-defects.md D428): the game's process is what a launcher on any
// system can ask for, and on Windows its window too.
//
// Windows: a snapshot of the processes (CreateToolhelp32Snapshot,
// Process32FirstW / NextW), each one's file name compared with `file` as
// Windows compares names (case not counted), and FindWindowW for the class.
// POSIX: the folders of /proc, each process's name (/proc/<id>/comm, which
// holds the first 15 bytes of the program's file name; where `file` is longer,
// the name /proc/<id>/exe links to settles it when it can be read). There is
// no list of windows by class: WindowClassOpen refuses.
// Part of the operating-system seam (docs/linux-round.md, LB1): no Windows or
// POSIX type appears here, and nothing logs or stops: the caller owns the
// message (bof3::Fatal).
#pragma once

namespace bof3::os {

// How many processes of the system run the program whose file name is `file`
// (UTF-8, no folder: "bof3x.exe", "bof3x"); -1 when the system does not say
// (LastError).
int ProgramsRunning(const char* file) noexcept;

// Whether a top-level window of the class `name` (UTF-8) is open. False when
// none is, or when the system cannot say (LastError: ENOSYS on POSIX).
bool WindowClassOpen(const char* name) noexcept;

}  // namespace bof3::os

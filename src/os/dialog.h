// The fatal message box: the one thing the player must read before the process
// goes on, shown where bof3::Fatal and the start-up refusals (DIV-0039) cannot
// rely on the game's own window. Blocks until it is read.
//
// Windows: MessageBoxA, with MB_TASKMODAL if `modal`. POSIX: stderr, and
// SDL3's message box in a build that links SDL's video (BOF3X_OS_SDL).
// Part of the operating-system seam (docs/linux-round.md, LB1): what the C++20
// standard library cannot do, behind one header per service, with a backend per
// system in win32/ and posix/. No Windows or POSIX type appears in a header, and
// nothing here logs or stops: the caller owns the message (bof3::Fatal).
#pragma once

namespace bof3::os {

// `error`: drawn as an error. `modal`: the program's own windows take no input meanwhile.
void ShowMessage(const char* title, const char* text, bool error, bool modal);

}  // namespace bof3::os

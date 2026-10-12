// The exe host's start-up (docs/state3-design.md section 4): what
// bof3x.exe's entry calls once the loader is done. In the hosted build
// DllMain does the same work under the loader lock (dllmain.cpp).
#pragma once

// Maps, binds and starts the game: Game_WinMain's answer is the exit code
// the entry hands to ExitProcess. `command_line` is GetCommandLineA()'s whole
// line; the program name is skipped as Capcom's _wincmdln skipped it.
// Exported by name from bof3x.dll; bof3x.exe imports it.
extern "C" __declspec(dllexport) int __cdecl Engine_Main(void* hinstance, const char* command_line, int show);

#include <cstdint>

namespace bof3 {

// Whether the player's BOF3.exe is there to verify against: always in the
// hosted build (Capcom's image is mapped); in the exe host, whether the
// game directory held the file at start (it is optional when BOF3X_EXEIMAGE
// names the .data source - docs/disc-only-round.md, group RD). Every check
// that compares something of ours with the file's bytes asks this first and,
// without the file, says in the log that it did not compare.
bool ExeFile_Present();

// The exe host only: `n` bytes at virtual address `va` of the player's
// BOF3.exe, read from the file (.text, .rdata or .data's initialised bytes).
// A Fatal hosted (read Capcom's image at its address there), without the
// file, or outside those bytes. For the start-up's verifications: not
// serialised with the code-byte store's reads of the same handle.
void ExeFile_Read(std::uint32_t va, void* to, std::uint32_t n);

}  // namespace bof3

// The exe host's start-up (docs/state3-design.md section 4): what
// bof3x.exe's entry calls once the loader is done. In the hosted build
// DllMain does the same work under the loader lock (dllmain.cpp).
#pragma once

// Maps, binds and starts the game: Game_WinMain's answer is the exit code
// the entry hands to ExitProcess. `command_line` is GetCommandLineA()'s whole
// line; the program name is skipped as Capcom's _wincmdln skipped it.
// Exported by name from bof3x.dll; bof3x.exe imports it.
extern "C" __declspec(dllexport) int __cdecl Engine_Main(void* hinstance, const char* command_line, int show);

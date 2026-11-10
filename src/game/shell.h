// The seven "Windows shell" starts, which are game code filed by their address:
// the pad latch, BOF3.CFG and the default key table, the disc probe, the boot
// set-up and its double-buffer blocks, and the frame's ordering-table links -
// group PW of the platform round (docs/platform-read-pass.md section 2).
// docs/shell.md.
//
// Every prototype is symbols.gen.h's (symbols.toml); this header declares the
// group's inject.
#pragma once

void Shell_Inject();

// Key_TableDefault 0x66C648 from the engine's rule (input::DefaultKeyTable),
// compared first with BOF3.exe's bytes where there is a file (a Fatal on a
// difference): the exe host's map step calls it after .data is placed; hosted,
// Shell_Inject does, before the fuzz.
void Shell_MapDefaultKeys();

// Game_Init's body (shell.cpp), which Game_Init calls from its own file since the source-layout
// round's moves (platform/magic_080.cpp): it returns where Game_Init jumps.
extern "C" void* __cdecl Shell_GameInitBody(void);

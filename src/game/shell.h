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

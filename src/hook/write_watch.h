// BOF3X_WRITEWATCH: a diagnostic log of every write to a few words of the
// image, naming the instruction that wrote (docs/write-watch.md).
//
//   BOF3X_WRITEWATCH=<hex VA>[:<length>][,<hex VA>[:<length>]...]
//
// Each range is split into aligned four-byte words, at most four in all
// (the CPU has four debug registers). The words go into the main thread's
// debug registers as write-only data breakpoints: the CPU traps after any
// instruction on that thread writes a byte of them, whoever's code it is -
// Capcom's, ours, the C run time's memcpy. Reads do not trap. The trap's
// handler logs, in `writewatch` lines, the word's value before and after,
// the writing instruction's address (the image's as its VA, a module's as
// name+offset; tools/gapwatch.py's llvm-symbolizer use names bof3x.dll
// offsets), the return addresses into our DLL on the stack, the logic frame,
// the recipe frame and the area. A writer seen before is logged again only
// every 256th write, so a fade that writes a byte each frame does not flood
// the log; a new writer is always logged.
//
// Made for D226 (docs/known-defects.md): area 196's sand hill shades
// 0x65EF0C..0x65EF17 found zeroed on a first visit, and only the hill's own
// fade known to write them:
//
//   BOF3X_WRITEWATCH=65EF0C:12
//
// Limits: the main thread only (the game's logic and its task stacks run
// there; a write from another thread is not caught); a write from inside a
// system call (a file read into the word) does not trap. It refuses to run
// beside BOF3X_GAPWATCH, which uses the same registers.
//
// Instrumentation, not a replacement: unset, Start returns at once and
// nothing is installed. Set, no byte the game keeps is changed and no
// instruction is skipped. No DIVERGENCE.md entry: it changes no behaviour.
#pragma once

namespace write_watch {

// BOF3X_WRITEWATCH parsed and the registers loaded; call after InjectAll, on
// the main thread (dllmain, exe_host, beside draw_order::Start).
void Start();

}  // namespace write_watch

// The gap watch (BOF3X_GAPWATCH=<file>; docs/install-gap-reads.md): which of
// the ranges a disc-built image leaves zero where the PC has a value does
// play actually read? IDEAS.md I43 step 2. Diagnostic only, off by default.
//
// In the exe host with a disc-built image (exe_image.h), every page of .data
// that holds a range of the watched classes (`gap` by default, the ranges
// recipes/exe-gaps.tsv named and the log's `gap` lines list) is given
// PAGE_GUARD. A vectored handler - the pattern calltrace.cpp and crash.cpp
// install - takes the guard fault, notes the access (and, the first time a
// range is touched, the reader's address, the return addresses on its stack,
// the logic frame, the recipe frame, the area and the route), and resumes the
// instruction; a single step after it puts the guard back. Nothing is
// written to game memory and no instruction is skipped: the reader reads the
// zero it would have read.
//
// A page that shares a range with hot data would fault on every access. A
// page is therefore given a budget of faults per logic frame
// (BOF3X_GAPWATCH_BUDGET, default 16); past it the page is parked - left
// unguarded - until the next latch re-arms it, and the page's own line
// counts faults, parks and its busiest readers instead: the page-granular
// fallback, which says how much of the page's traffic the watch saw.
//
// Instrumentation, not a replacement: it changes no byte the game keeps and
// needs no DIVERGENCE.md entry. It does change page protections for the life
// of the run, which is why it is never on by default. One limit, by how
// Windows treats guard pages: an access to a guarded page from inside a
// system call (a file read into a .data buffer) fails that call instead of
// raising the fault. No such buffer is known on a watched page; the
// reference runs' state hash against an unwatched run is the check
// (docs/install-gap-reads.md section 2).
#pragma once

namespace bof3 {

// BOF3X_GAPWATCH names the output file, rewritten in place every 60 latches
// (the runs are ended by taskkill); a directory instead gets
// gapwatch_<build>_<route>.tsv in it. BOF3X_GAPWATCH_CLASSES: the classes to
// watch, comma separated, from gap, unread and language (default gap).
// Called by Engine_Main after InputScript_Start; a Fatal in the hosted build
// or without a disc-built image, since the PC's .data has no gap.
void GapWatch_Start(void* dll_module);

// Called by the latch (input_script.cpp DeviceLatch) on either side of the
// state hash's tick: Pause unguards every watched page while the hash reads
// them all (the hash is not a reader of the game's), Tick re-arms every page
// - the parked ones too - and writes the file every 60th time. Nothing while
// the watch is off.
void GapWatch_Pause();
void GapWatch_Tick();

// Writes the file now: Engine_Main after Game_WinMain returns (a run ended
// by taskkill keeps what the last rewrite wrote).
// Nothing while the watch is off.
void GapWatch_Flush();

// BOF3X_SHADOW=gapwatch (from InjectAll): the watch on a scratch page with a
// planted read - logged once, the value untouched, the page guarded again
// after it; a write, a copy across two pages, the budget's park and re-arm,
// and an unguarded page that never faults; and, with BOF3X_GAPWATCH unset,
// no page of .data guarded.
void GapWatch_SelfTest();

}  // namespace bof3

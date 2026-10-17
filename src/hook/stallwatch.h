// BOF3X_STALLWATCH=<ms>: where the main thread is when the frame loop stops
// turning (known-defects D304). A watcher thread samples the loop's turns and
// Frame_Counter every 50 ms; once neither the loop has turned nor - with the
// loop turning - a logic frame has run for <ms> (at least 100), it suspends
// the main thread, copies its eip and the dwords of its stack that point into
// executable memory (return-address candidates, nearest first), resumes it,
// and writes one `stall` line (local time first) with each as module+offset - again every second
// while the stall lasts, and an `stall ... over` line with its length when it
// ends. A module offset is what llvm-symbolizer wants with the module's
// ImageBase added (bof3x.dll 0x10000000).
//
// The lines go to bof3x.stall.log beside bof3x.log on their own handle: a
// main thread held inside a write to bof3x.log would hold a second writer to
// that handle too. Nothing is allocated, logged or resolved while the main
// thread is suspended (it may hold the heap or loader lock).
//
// Instrumentation, not a replacement: it reads and logs. It needs no
// DIVERGENCE.md entry.
#pragma once

namespace bof3 {

// On the main thread, before the frame loop: reads BOF3X_STALLWATCH and, when
// set, starts the watcher. A value that is not a number of milliseconds of at
// least 100 is a Fatal.
void StallWatch_Start(const volatile void* frame_counter);   // a 32-bit counter

// Once a turn of the frame loop.
void StallWatch_Beat();

}  // namespace bof3

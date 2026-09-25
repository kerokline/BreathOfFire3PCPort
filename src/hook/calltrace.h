// First-call tracer: which original functions does a run reach, from where,
// and on which logic frame. Diagnostic only, off unless BOF3X_CALLTRACE is set
// (docs/call-trace.md).
//
// Every listed function entry gets a one-byte int3. The first time control
// arrives, a vectored exception handler records (function, return address,
// logic frame, thread), puts the original byte back and resumes at it - so
// each function costs one exception per run and then runs as Capcom built it.
// The exception is Task_RunAll, which is re-armed after every hit by a
// single-step: WinMain calls it once per logic frame, so counting its hits is
// an exact frame counter. BOF3X_CALLTRACE_MODE=all re-arms every entry the
// same way, for per-frame call counts, edges and a call hash
// (bof3x.callframes.tsv, bof3x.callcounts.tsv; docs/call-trace.md sections 5-6).
//
// This is instrumentation, not a replacement: no original code is skipped or
// reimplemented, so it needs no DIVERGENCE.md entry. It does make .text
// writable for the life of the process, which is why it is never on by
// default.
#pragma once

namespace bof3 {

// BOF3X_CALLTRACE names a text file of function entries, one per line: hex
// address, then hex size (tools/calltrace.py entries). Hits go to
// bof3x.calltrace.tsv next to the DLL. Call after InjectAll: functions
// registered there are left unarmed, in both directions of the A/B switch.
void CallTrace_Start(void* dll_module);

}  // namespace bof3

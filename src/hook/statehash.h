// The state hash: one hash per 4 KiB page of BOF3.exe's .data, once a logic
// frame, written to a file - so two runs can be compared by what the game's
// memory held, frame for frame, whoever's code wrote it. Diagnostic only, off
// unless BOF3X_STATEHASH is set (docs/state-hash.md).
//
// The call trace compares the calls made into Capcom's code, and sees less as
// less of it is left; this compares the result, and does not care where a
// function's boundaries are. tools/statehash.py reads the files.
//
// Instrumentation, not a replacement: it reads memory and writes a file. It
// needs no DIVERGENCE.md entry.
#pragma once

namespace bof3 {

// BOF3X_STATEHASH names the output file. BOF3X_STATEHASH_SKIP names a list of
// byte ranges hashed as zero; BOF3X_STATEHASH_DUMP a comma-separated list of
// ticks at which all of .data is also written raw, beside the output file.
// Returns true when the hash is on.
bool StateHash_Start();

// Called at every latch (input_script.cpp), before Input_Latch runs: hashes
// when Frame_Counter has moved since the last call, so once a logic frame,
// at the same point of the loop under Capcom's WinMain and ours.
void StateHash_Tick();

}  // namespace bof3

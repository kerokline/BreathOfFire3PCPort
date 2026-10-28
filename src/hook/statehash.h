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

// Where the game's image lies, for a host that places it somewhere other than
// BOF3.exe's own base 0x400000 (IDEAS I44 stage 3, the relocatable image):
// the loader calls this before StateHash_BeforeInjects. Every host today maps
// the image at 0x400000 and does not call it. From then on .data is read at
// its place in the moved image, and every word whose value lies in the moved
// image is hashed as at 0x400000 (docs/state-hash.md section 8). The base is
// a multiple of 64 KiB and the image ends at or below 0x80000000; anything
// else, or a call after the census, aborts.
void StateHash_SetImageBase(unsigned long base);

// Called by DllMain before InjectAll, with our module: reads our image's
// extent from its PE headers and records which aligned words of .data hold a
// pointer into BOF3.exe's image as the loader mapped it - Capcom's pointer
// slots, the only words the hash normalises (docs/state-hash.md section 7).
// Logs the census: the words of .data whose value already lies in our image.
// Does nothing unless BOF3X_STATEHASH is set - but under BOF3X_SELFTEST_ONLY
// it first runs the hash's own self-test on synthetic pages (a page moved by a
// constant hashes the same), whatever BOF3X_STATEHASH says.
void StateHash_BeforeInjects(void* dll_module);

// Called by DllMain after InjectAll: the same census again, the words our
// injects wrote. Does nothing unless BOF3X_STATEHASH is set.
void StateHash_AfterInjects();

// BOF3X_STATEHASH names the output file. BOF3X_STATEHASH_SKIP names a list of
// byte ranges hashed as zero; BOF3X_STATEHASH_DUMP a comma-separated list of
// ticks at which all of .data is also written raw, beside the output file.
// Returns true when the hash is on.
bool StateHash_Start();

// Called at every latch (input_script.cpp), before Input_Latch runs: hashes
// when Frame_Counter has moved since the last call, so once a logic frame,
// at the same point of the loop under Capcom's WinMain and ours.
void StateHash_Tick();

// Hands what the file holds so far to the OS: called by Fatal and by the
// crash reporter, whose ends (TerminateProcess, the fault) run no exit
// handlers, so the frames up to the end are kept (round fourteen's review,
// item 15). Does nothing while the hash is off.
void StateHash_Flush();

}  // namespace bof3

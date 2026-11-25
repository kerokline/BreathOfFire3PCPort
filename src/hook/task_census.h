// BOF3X_TASKCENSUS: a diagnostic census of where the four tasks stand at the
// frame boundary (docs/state-by-name-round.md 2, IDEAS I54 tier 2).
//
//   BOF3X_TASKCENSUS=<path of a .tsv>
//
// A task is a stackful coroutine (docs/task_sched.md): between two logic
// frames each live one is suspended inside Task_Sleep, somewhere in its call
// tree. A quicksave copies the stack slices because that place is nowhere
// else. This asks how deep the place is. At the quicksave point of every
// frame (WinMain's loop, beside Snapshot_Pending), for each task record that
// is not free, it reads the saved stack pointer, the bytes the slice holds
// above it, and the chain of return addresses into our code found there by a
// scan (a word that names an executable byte of ours right after a call
// instruction; a stale word a deeper frame left can pass the test, so the
// chain is a reading aid and the depth is the measure). Rows aggregate by task
// and chain, and by the frame's four chains together; the file is rewritten
// every 60 frames (a harness ends the game by ending the process at the
// recipe's done, so nothing is written at exit).
//
// The yield word (docs/state-by-name-sites.md 5): each row also carries the
// site the task's yield entry held on those frames (hook/task_yield.h; 0 for
// none), a row per chain and word; the file lists the build's sites
// (#yieldsite) and the frames as runs of the four tasks' words (run rows, by
// Frame_Counter). tools/task_census.py --sites checks the chain and the word
// against each other; --list-runs finds frames at a site.
//
// Instrumentation, not a replacement: unset, the hook is one test of a bool
// that is never set. Set, it reads game memory and writes this module's own
// tables and file. No DIVERGENCE.md entry: it changes no behaviour.
#pragma once

namespace task_census {

// WinMain's loop, at the quicksave point. Reads BOF3X_TASKCENSUS at its
// first call.
void Frame();

}  // namespace task_census

// The yield word: the seam between the snapshot core (src/hook/snapshot.cpp,
// the state-by-name round's group A1) and the task bodies that say where they
// stand (group A2: src/game/core/task.cpp, transition.cpp, title.cpp,
// sound.cpp). docs/state-by-name-round.md 3.2 and 3.3;
// docs/state-by-name-format.md section 4.
//
// ---------------------------------------------------------------------------
// WHAT A2 MUST PROVIDE. Nothing below is a stub: with no site listed, the
// snapshot core's real test (snapshot.cpp TaskPlace) finds every task that is
// neither free nor created and not yet run at no named place, and the take is
// written "not portable" - which is right until a site exists. A2's work
// turns that test true; it changes nothing in snapshot.cpp.
//
//   1. Task_YieldAt(site, frames, locals, bytes) (task.cpp), in place of
//      Task_Sleep(frames) where a resume entry stands for the yield: for the
//      running task k (Task_CurrentOffset >> 5) g_entries[k] = {site, bytes,
//      the bytes of locals} (bytes <= kLocalsMax), then Task_Sleep(frames),
//      then g_entries[k] cleared on the way back.
//   2. The task's entry cleared wherever a task is dropped where it stood:
//      Task_Create(slot, ...) and Task_Restart clear g_entries[slot], and
//      Task_Exit clears the current task's. Else a task created again over
//      one that slept at a site would keep that site in its entry, and a take
//      would read it there.
//   3. One row in kSites per site, after the row of id 0: a permanent id
//      (like the ledger's, never reused when a site goes; ids are 1, 2, ...),
//      its name, its resume entry and its locals' size in bytes.
//   4. Each resume entry: a task body as a created task's entry is (no
//      arguments; entered by the scheduler's `ret`). It reads its locals from
//      g_entries[k], k = Task_CurrentOffset >> 5, clears g_entries[k] - as
//      Task_YieldAt clears it on the way back - and goes on from the yield:
//      for a loop that sleeps at its end the loop; for one that sleeps at its
//      top the rest of that pass, then the loop (round doc 3.2). One text of
//      each loop, entered two ways.
//
// What the snapshot core does with them (snapshot.cpp):
//   - at a take, for each task record (TaskPlace): free (state 0); created
//     and not yet run (its saved stack pointer is its slice's first frame:
//     its place is its entry word, a code place, written as the VA it names);
//     at a site (g_entries[k].site != 0, the site and the locals written);
//     otherwise nowhere named, and the take is not portable;
//   - at an exact load, g_entries as the file has them (each task's stack
//     comes back with it, so its Task_YieldAt frame clears the entry);
//   - at a portable load, for each task not free: a fresh first frame on its
//     slice by Task_Create itself, with the code place of the VA, or the
//     site's resume entry; the record's state and sleep word put back as the
//     file has them (a task that slept n frames wakes in n); g_entries[k] the
//     file's, which the resume entry reads. A site this build does not list,
//     or locals of another size, is a refusal before a byte is written.
// ---------------------------------------------------------------------------
//
// The entries are a static of ours, not in the span: Capcom's layout is not
// touched and no state hash moves (round doc 3.2). They are the TASKS record
// of a take (docs/state-by-name-format.md 2).
#pragma once

#include <cstdint>

namespace bof3::task_yield {

constexpr unsigned kTasks = 4;        // Task_Records' four (task_sched_callees.h kTasks)
constexpr unsigned kLocalsMax = 16;   // a site's locals, at most

struct Entry {
    std::uint32_t site = 0;    // 0: the task stands at no site
    std::uint32_t bytes = 0;   // of locals, 0 .. kLocalsMax
    unsigned char locals[kLocalsMax] = {};
};

// One entry a task, by its record's number.
inline Entry g_entries[kTasks];

// A task body as Task_Create's entry is: no arguments, entered by a `ret`.
using ResumeFn = void (*)();

struct Site {
    std::uint32_t id;       // permanent; 0 is "no site" and has no resume entry
    const char* name;       // the function that yields there, for the log
    ResumeFn resume;        // the resume entry (null only for id 0)
    std::uint32_t bytes;    // the locals the yield writes and the resume entry reads
};

// The site list. A2 adds its rows after this one (item 3 above).
inline constexpr Site kSites[] = {
    {0, "no site", nullptr, 0},
};

// The site of `id`, or null: id 0 and an id this build does not list.
inline const Site* FindSite(std::uint32_t id) {
    if (id == 0) return nullptr;
    for (const Site& s : kSites)
        if (s.id == id && s.resume) return &s;
    return nullptr;
}

}  // namespace bof3::task_yield

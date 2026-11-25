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
//      (As built, docs/state-by-name-sites.md 2: Task_Create clears; Task_Restart
//      and Task_Exit, in the untouched task_switch_*.h, need no clear - they act
//      on the running task, whose entry is always clear.)
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

// A task body as Task_Create's entry is: no arguments, entered by a `ret`,
// __cdecl as every task body of ours (ms_abi on Linux: cmake/linux_target.cmake).
using ResumeFn = void (__cdecl*)();

struct Site {
    std::uint32_t id;       // permanent; 0 is "no site" and has no resume entry
    const char* name;       // the function that yields there, for the log
    ResumeFn resume;        // the resume entry (null only for id 0)
    std::uint32_t bytes;    // the locals the yield writes and the resume entry reads
};

// --- the sites (group A2; docs/state-by-name-sites.md) ------------------------
//
// The ids are permanent, as the ledger's: a site that goes keeps its number
// unused. The locals are fixed-width little-endian words, the same bytes in
// the 32-bit and the 64-bit build. Each resume entry is defined beside the
// body it stands for and is the same loop text entered a second way.
enum SiteId : std::uint32_t {
    kSiteFieldTask = 1,      // Field_Task (transition.cpp, 0x495800): the loop's end; no locals
    kSiteTitleTaskRun = 2,   // TitleTask_Run (sound.cpp, 0x588E70): the loop's top; no locals
    kSiteTitleTask = 3,      // Title_Task (title.cpp, 0x4621C0): the loop's top; no locals
    kSiteFadeSub = 4,        // Transition_FadeSub (transition.cpp, 0x495620): FadeLocals
    kSiteFadeAdd = 5,        // Transition_FadeAdd (transition.cpp, 0x4956A0): FadeLocals
    kSiteSwingOut = 6,       // SwingOut, kinds 10 and 17 (0x495270, 0x495450): SwingOutLocals
    kSiteSwingInOpen = 7,    // SwingIn's five frames of fade-in, kinds 11 and 18 (0x4952D0, 0x4954B0): SwingInOpenLocals
    kSiteSwingInTurn = 8,    // SwingIn's turn back: SwingInTurnLocals
};

// A fade's frame: the level (only its low 16 bits are ever written, by
// Transition_DrawTile; the high 16 are the 0 the start gave), and the three
// arguments the loop passes on, as the body was called with them. Whether
// the fade goes out is the step's sign and is not kept.
struct FadeLocals {
    std::int32_t level;
    std::int32_t step;
    std::uint32_t semi;
    std::uint32_t slot;
};
struct SwingOutLocals {
    std::int32_t level;
    std::int32_t step;
};
struct SwingInOpenLocals {
    std::int32_t level;
    std::int32_t frame;   // the opening frame that has just slept, 0..4
};
struct SwingInTurnLocals {
    std::int32_t level;
    std::uint32_t done;   // the tile's last answer, 0 or 1 (a byte in the body)
};
static_assert(sizeof(FadeLocals) == 16 && sizeof(FadeLocals) <= kLocalsMax, "a fade's locals fill the entry");
static_assert(sizeof(SwingOutLocals) == 8 && sizeof(SwingInOpenLocals) == 8 && sizeof(SwingInTurnLocals) == 8,
              "the swings' locals are two words");

// The resume entries, each beside its body.
void __cdecl ResumeFieldTask();       // transition.cpp
void __cdecl ResumeTitleTaskRun();    // sound.cpp
void __cdecl ResumeTitleTask();       // title.cpp
void __cdecl ResumeFadeSub();         // transition.cpp
void __cdecl ResumeFadeAdd();         // transition.cpp
void __cdecl ResumeSwingOut();        // transition.cpp
void __cdecl ResumeSwingInOpen();     // transition.cpp
void __cdecl ResumeSwingInTurn();     // transition.cpp

// The site list (item 3 above).
inline constexpr Site kSites[] = {
    {0, "no site", nullptr, 0},
    {kSiteFieldTask, "Field_Task", &ResumeFieldTask, 0},
    {kSiteTitleTaskRun, "TitleTask_Run", &ResumeTitleTaskRun, 0},
    {kSiteTitleTask, "Title_Task", &ResumeTitleTask, 0},
    {kSiteFadeSub, "Transition_FadeSub", &ResumeFadeSub, sizeof(FadeLocals)},
    {kSiteFadeAdd, "Transition_FadeAdd", &ResumeFadeAdd, sizeof(FadeLocals)},
    {kSiteSwingOut, "SwingOut", &ResumeSwingOut, sizeof(SwingOutLocals)},
    {kSiteSwingInOpen, "SwingIn (opening)", &ResumeSwingInOpen, sizeof(SwingInOpenLocals)},
    {kSiteSwingInTurn, "SwingIn (turn)", &ResumeSwingInTurn, sizeof(SwingInTurnLocals)},
};

// The sleep a body yields through: its own callee table's (the fuzz hands a
// body a recorder there), Task_Sleep in the game.
using SleepFn = void (__cdecl*)(int);

// The site of `id`, or null: id 0 and an id this build does not list.
inline const Site* FindSite(std::uint32_t id) {
    if (id == 0) return nullptr;
    for (const Site& s : kSites)
        if (s.id == id && s.resume) return &s;
    return nullptr;
}

}  // namespace bof3::task_yield

// task.cpp (item 1). `sleep` is the body's own Task_Sleep callee, so that a
// fuzz that records the body's calls sees the same sleep. The entry is the
// running task's (Task_CurrentOffset), written before the sleep and cleared
// when it returns. A Task_CurrentOffset that is not one of the four records'
// offsets (only a fuzz has one: the game's walk writes 0, 0x20, 0x40, 0x60)
// writes no entry, so such a yield is at no site.
void Task_YieldAt(std::uint32_t site, int frames, const void* locals, std::uint32_t bytes, bof3::task_yield::SleepFn sleep);
// A resume entry's first step (item 4): the running task's entry must hold
// `site` with exactly `bytes` of locals; they are copied to `locals` and the
// entry cleared. Anything else is a Fatal: a resume entry entered at another
// site's place.
void Task_YieldResume(std::uint32_t site, void* locals, std::uint32_t bytes);

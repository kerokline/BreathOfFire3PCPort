# State by name, group A2: the yield word and the first sites

**Status:** STABLE (2026-11-25, verified the same day on the laptop, the
Linux x86-64 build of `sbn/A2`; section 5 has the measurements). The state-by-name round's group A2
([`state-by-name-round.md`](state-by-name-round.md) 3.2, 3.3, 4, 4a).
Built: `Task_YieldAt` and `Task_YieldResume` (`src/game/core/task.cpp`), the
site list (`src/hook/task_yield.h`), the resume entries beside their bodies
(`transition.cpp`, `title.cpp`, `sound.cpp`), the census probe's yield column
(`src/hook/task_census.{h,cpp}`, `tools/task_census.py`), and two small
changes in `src/hook/snapshot.cpp` (section 4). No ledger entry: no behaviour
changes (section 3). Not played by the owner.

## 1. The sites

Eight sites, permanent ids 1 to 8 (`task_yield.h` `SiteId`, `kSites`). A
site is a sleep a resume entry can stand for; each body sleeps through
`Task_YieldAt(site, 1, &locals, sizeof locals, its callee's sleep)` in
place of its callee's `sleep(1)`.

| Id | Name | Body (original) | Resume entry | Locals (bytes) | What else is alive across the yield, and where it lives |
|--:|---|---|---|---|---|
| 1 | `Field_Task` | `Field_Task` `0x495800`, task 0 | `ResumeFieldTask`: the loop (`FieldTaskLoop`) | none (0) | the mode and step: `Game_Mode`, `Game_Step`, task 0's record words `+0x18`, `+0x1A`, in the span; the handler is read from `GameMode_Handlers` afresh each pass |
| 2 | `TitleTask_Run` | `TitleTask_Run` `0x588E70`, task 0 at the title menu | `ResumeTitleTaskRun`: the rest of the pass (`TitleTaskRunPass`: the mode's entry, `Field_RunTaskRecords`), then the loop | none (0) | `Game_Mode` (the span), read afresh |
| 3 | `Title_Task` | `Title_Task` `0x4621C0`, task 1 | `ResumeTitleTask`: the rest of the pass (`TitleTaskPass`: `Title_CheckStart`, the state's entry, the backdrop, the logo), then the loop | none (0) | the title's state: task 1's private words (`Title_State` and the rest, `0x66C7F8..0x66C80B`), in the span |
| 4 | `Transition_FadeSub` | `0x495620`, task 2 | `ResumeFadeSub`: the loop (`FadeSubLoop`) from its test | `FadeLocals` (16): the level, the step, `semi`, `slot` | whether the fade goes out: the step's sign, re-derived; the kind that chose the arguments: `ModeFlow_TransitionKind`, in the span (not read: the arguments are kept) |
| 5 | `Transition_FadeAdd` | `0x4956A0`, task 2 | `ResumeFadeAdd`: the loop (`FadeAddLoop`) from its test | `FadeLocals` (16) | as site 4 |
| 6 | `SwingOut` | kinds 10 and 17 (`0x495270`, `0x495450`), task 2 | `ResumeSwingOut`: the loop (`SwingOutLoop`) from its test | `SwingOutLocals` (8): the level, the step | the camera words it turns (`Camera_Angles`, `Cond_AngleFB`, `Camera_Distance`, `MapView_Redraw`): the span |
| 7 | `SwingIn (opening)` | kinds 11 and 18 (`0x4952D0`, `0x4954B0`), task 2: the five frames of fade-in | `ResumeSwingInOpen`: the loop's step (`frame + 1`), then the loop (`SwingInOpenLoop`) | `SwingInOpenLocals` (8): the level, the frame that slept (0..4) | the camera words, set before the loop: the span |
| 8 | `SwingIn (turn)` | the same body: the turn back | `ResumeSwingInTurn`: the rest of the pass (`SwingInTurnRest`: the tile while not done), then the loop (`SwingInTurnLoop`) | `SwingInTurnLocals` (8): the level, `done` (a byte in the body, kept as a word) | the camera words: the span |

**Read against the source.** The round doc (2.1) has the fades' arguments as
"constants of the kind" and the level as the one word alive. That holds
(only the 21 kinds call the fades: a grep of `fade_sub` / `fade_add`, the
other hit being the callee table's originals), but the resume entry does not
re-derive them from the kind: it would need a second copy of the kind table.
The three arguments and the level are 16 bytes, exactly `kLocalsMax`, so the
entry keeps all four. The swing-in has **two** loops with a sleep each, not
one: two sites, 7 and 8. The level's high 16 bits are the 0 the start gave
(`Transition_DrawTile` writes a `short`), and the locals keep the whole word.

**Read against the compiled frame** (`objdump` of the ELF of `sbn/A2`):
`FieldTaskLoop` keeps nothing on its stack across the call to
`Task_YieldAt` - the handler's table and `Game_Mode` are loaded again after
it through `g_runBase`, and the callee table's `sleep` is reloaded from
`mode_flow::g`. The fade and swing loops hold their locals in the one
struct whose address they pass. Every resume entry is `__cdecl` (`ms_abi` on
Linux, as every task body): entered by the scheduler's `ret` at `top - 0x28`
(8 mod 16), it pushes two registers and saves `xmm6..xmm15` with `movaps` at
0 mod 16, as a Win64 frame wants.

**One text of each loop, entered two ways** (round doc 3.2): each loop is
one function, entered by the body (with the start's values) and by the
resume entry (with the take's). For a loop that sleeps at its end (site 1)
the resume entry is the loop. For one that sleeps at its top (2, 3) the pass
after the sleep is a function of its own, called by the loop and by the
resume entry. For a sleep mid-pass (8) the same with the rest of that pass.
For the loops that sleep at their end and step a counter (7) the resume
entry makes the step.

**Not sites, and left plain** (a take on them is "not portable"):
`Transition_Kind20` (a sleeping loop of its own; reached by no route of the
census), `Field_WaitTransition`, `Menu_WaitTransition`, and the mode steps'
sleeps - group B2's.

## 2. The yield word

`Task_YieldAt(site, frames, locals, bytes, sleep)` (`task.cpp`): checks that
the site is a row of `kSites` with that many bytes (else a `Fatal`), writes
the running task's entry (`Task_CurrentOffset / 0x20`), calls `sleep(frames)`
and clears the entry when it returns. The fifth argument is the body's own
callee for `Task_Sleep`: the fuzz of each body hands it a recorder there, so
the body's calls are the same calls as before. A `Task_CurrentOffset` that
is not one of the four records' (the game's walk writes only 0, 0x20, 0x40,
0x60; the scheduler's fuzz writes any number there) writes no entry.

`Task_YieldResume(site, locals, bytes)`: a resume entry's first step. The
running task's entry must hold that site with that many bytes, else a
`Fatal`; the locals are copied out and the entry cleared.

**Where an entry is cleared.** `Task_Create` clears the slot it makes. The
contract (`task_yield.h` item 2) also asks `Task_Restart` and `Task_Exit` to
clear, and they are in `task_switch_*.h`, which stays untouched. They need
no clear: they act on the running task, and **a running task's entry is
always clear** - an entry is written only by `Task_YieldAt` immediately
before its sleep switches away, and cleared by `Task_YieldAt` as the sleep
returns or by the resume entry before anything else runs; `Task_Create`
clears a slot dropped while it slept. Two more cases: an exact load puts the
take's entries back with the take's stacks, so each task wakes inside its
`Task_YieldAt` and clears its own; a portable load writes them for the resume
entries (section 4). The census checks the consequence on every frame: a
site is carried only by a task whose chain stands in `Task_YieldAt` at that
site, never by one created and not yet run.

**A caveat for the developer's switches.** Under `BOF3X_ORIGINAL` naming
`Task_Create`, Capcom's body creates the task and nothing clears the slot's
entry. A task dropped while it slept at a site and then sleeping at no site
would carry the old site. The round doc already treats takes under such
switches as the developer's (8); it is not measured.

**The fuzz.** A body's fuzz long-jumps out of its sleep's recorder after a
few sleeps, so a `Task_YieldAt` interrupted there leaves its entry set. That
is harmless as long as the fuzz runs before the game's tasks are made: every
task the game runs is made by `Task_Create`, which clears its slot, and a
free record's entry is never read (A1's `TaskPlace` tests "free" first). The
fuzz is built only into the 32-bit hosted build (`CMakeLists.txt:340`: the
64-bit and Linux builds drop every `*_fuzz.cpp`), so it was not run here
(section 6).

## 3. No behaviour changes

The game's functions call what they called, in the order they called it:
the restructured loops are the same statements, and the yield goes through
the same callee. The write and the clear are to `task_yield::g_entries`, a
static of ours outside the span. Shown by the state hash (section 5): three
routes identical tick for tick against the round base. The hosted build with
Capcom's body (`BOF3X_ORIGINAL`) never calls `Task_YieldAt`, so its entry
stays clear and its takes are never portable.

## 4. What changed in `snapshot.cpp`, and why

A1 expected no change. Two were needed:

1. **`PlaceTasks` wrote the site's entry before `Task_Create`**, and
   `Task_Create` now clears the slot's entry (item 2 of A1's own contract),
   so a portable load would have handed every resume entry an empty entry
   and stopped at `Task_YieldResume`'s `Fatal`. The entry is now written
   after `Task_Create`. Six lines moved, none added to the logic.
2. **`BOF3X_QSPORTABLE=1`, the developer's switch** the proof's first row
   needs ("a build, the same build, as a portable load forced"; round doc
   5). `DoLoad` reads it (the value `1` exactly) into a static that `Parse`
   reads at the point where it decides exact against portable: with the
   switch set and nothing else differing, the reason is "forced portable",
   and the file takes the portable road with all its checks (a file that is
   not portable is refused as "forced portable (not portable)"). Set by
   `DoLoad` alone and cleared after its `Parse`, so the self-test's `Parse`
   calls never see it. About fifteen lines.

## 5. Measured (2026-11-25, the laptop, the Linux x86-64 ELF of `sbn/A2`)

The captures are outside the tree, in `~/bof3x-runs/sbn1125/A2/root/` on the
laptop (they hold game data): `sh/` the state-hash runs, `tc/` the census,
`qs/` the quicksaves, `selftest2.log`.

- **The build** (`cmake --build build-linux-x86_64 -j 2`): clean; no warning
  in the files of this change.
- **The self-test** (`BOF3X_LAUNCHER=1 BOF3X_SELFTEST_ONLY=1`, the two cache
  variables, offscreen and dummy SDL): exit 0, 10,403 log lines, `snapshot:
  self-test passed` with every format-2 case.
- **No state hash moved.** `bridgeWalk`, `combat` and
  `worldMapAndAreaTransition` as state-hash runs under `orig6`'s conditions
  ([`state-hash.md`](state-hash.md) 6: `--only-slot0`, `BOF3X_SCALE=2`,
  `BOF3X_OPT=none`, `BOF3X_LAYERING=0`, the skip list, `en-US`), this build
  (with the census probe on) against the round base's ELF: `statehash.py
  diff` **identical on all 2,074, 2,602 and 2,113 ticks**.
- **The census agrees.** Six routes (`bridgeWalk`, `combat`,
  `worldMapAndAreaTransition` from the runs above; `shop`, `attract_cycle`,
  `newGameName` headless, `--direct`, `en-US`), `task_census.py --sites`:

  | Route | Frames | Flat by the chain | Of those, every live task carrying its site | Disagreeing |
  |---|--:|--:|--:|--:|
  | `bridgeWalk` | 2,040 | 2,005 | 2,005 | 0 |
  | `combat` | 2,580 | 2,564 | 2,564 | 0 |
  | `worldMapAndAreaTransition` | 2,100 | 1,797 | 1,797 | 0 |
  | `shop` | 3,120 | 2,906 | 2,906 | 0 |
  | `attract_cycle` | 14,100 | 13,853 | 13,853 | 0 |
  | `newGameName` | 3,000 | 2,968 | 2,968 | 0 |
  | **all six** | **26,940** | **26,093 (96.9 %)** | **26,093** | **0** |

  Each route's flat count is the staging census's
  ([`state-by-name-round.md`](state-by-name-round.md) 2.1) to the frame. The
  frames per site, task by task (created-and-not-yet-run frames left out):
  site 1 `Field_Task` 20,100; site 2 `TitleTask_Run` 874; site 3
  `Title_Task` 15,279; site 4 `Transition_FadeSub` 1,482; site 5
  `Transition_FadeAdd` 30; site 6 `SwingOut` 62; site 7 `SwingIn (opening)`
  12; site 8 `SwingIn (turn)` 58. Task 0 at no site: 847 frames, all in
  `Field_WaitTransition` (816), `GameMode_Field` (21), `Menu_WaitTransition`
  (8), `GameMode3_Leave` (2) - B2's list. No row carried a site its chain
  did not stand at, and no task created and not yet run carried one. The
  yield's depth from the slice's top is now `0x258` for the loops of sites
  1 to 5 (`0x1E8` before: `Task_YieldAt` is a frame between the body and
  `Task_Sleep`).
- **A portable load in a live game, forced** (the proof's first row; on Linux
  as A1 ran it: the live proof form, then the cross form with `--run-game
  none` in the live run's folder, here with `BOF3X_QSPORTABLE=1`; then
  `quicksave_check.py cross`; the same `orig6` conditions). Every loaded run
  logged `restored PORTABLE`:

  | Take (route, recipe frame, `Frame_Counter`) | Where the tasks stood (the file's TASKS record) | Cross |
  |---|---|---|
  | `combat` 900, FC 901 (A1's `quick_combat`: a battle mid-turn) | 0 site 1; 1, 2, 3 free | **equal, 1,680 ticks** |
  | `worldMapAndAreaTransition` 199, FC 200 (the title menu) | 0 site 2; 2 site 4, level `0x4000`, step `0x800`, semi 1, slot 0 (a fade to black, half done) | **equal, 1,920 ticks** |
  | `worldMapAndAreaTransition` 149, FC 150 (the title) | 1 site 3; 2 site 4, level `0x3000`, step `0x800` | **equal, 1,983 ticks** |
  | `attract4k` 1333, FC 1334 (a fade in the attract demo) | 0 site 1; 1 site 3; 2 site 4, level `0x4FFF`, step `-0x800` (a fade in) | **equal, 2,654 ticks** |
  | `worldMapAndAreaTransition` 327, FC 328 (task 0 just restarted) | 0 created and not yet run at `0x495800` (`Field_Task`) | **equal, 1,792 ticks** |
  | `worldMapAndAreaTransition` 1110, FC 1111 (a swing in play) | 0 nowhere (`Field_WaitTransition`); 2 site 8, level `0x3BFF`, done 0 | written **not portable** ("a task stands at no named place (task 0)"); the forced load refused it, "forced portable (not portable)", the save half offered |

  So the takes stand in the field's loop, in a fade (on the title menu and
  in play), in the title and in the attract cycle, and on a nested frame.
  `attract4k` is the first 4,000 frames of `attract_cycle.txt` with its
  shots left out (it has no input); the recipes were made by
  `quicksave_check.py make --dir` in the captures' folder and are not
  committed. **No hash differed**, so there was nothing to chase: the resume
  entries stand for the stacks on every take tried.
- **The no-data checks**: `ledger_check`, `ledger_switches`,
  `ledger_registry`, `width_census`, `data_census --check` and `--strict`,
  `reloc_census`, `reloc_bare`, `region_table`, `tu_renames`, `tu_census
  --strict`, `retire_shells --strict`, `os_census` (three forms), `exe_own`,
  `tables` pass. `data_census --strict` wanted two keys of
  `analysis/typed_left.tsv` renamed (a raw site moved from `TitleTask_Run`
  to `TitleTaskRunPass`, one from `SwingIn` to `SwingInTurnLoop`, their
  reasons kept); `reloc_census` rose by one `addr_eq` site that is A3's
  (`src/hook/region_table.cpp`, already in the round base), and
  `analysis/reloc_census.tsv` is regenerated as its message says.

## 6. Not checked

- **The bodies' fuzz** (`mode_flow_fuzz.cpp` for the fades, the swings and
  `Field_Task`; `shop_states2_fuzz.cpp` for `TitleTask_Run`; `Title_Task`
  has no fuzz of its own - `mode_flow_fuzz.cpp` stands it in as
  `StubTitleTask` - so for it the census and the state hash are the check): built only into the 32-bit hosted build, which this
  laptop cannot build. By the source the recorded calls are the same, in the
  same order, through the same callees; that is a reading, not a run. Owed
  on the desktop.
- The 32-bit build: not built; the locals are fixed-width words for it.
- Portable loads **across builds**: B3's (the round's base against its tip
  is runnable here; the width and system rows are the desktop's).
- `Transition_Kind20`'s loop (not a site), and the swings of kinds 17 and
  18: the census's chains show kinds 10 and 11 only; 17 and 18 run the same
  loops with another step.
  `FadeAdd` was reached on one route only (30 frames), and no take stood in
  it, nor in a swing with task 0 flat (none was seen: every swing ran with
  task 0 in `Field_WaitTransition`).
- `BOF3X_ORIGINAL` naming `Task_Create`, `Task_Sleep` or a body (section 2).

---

# Part B2: the nested waits (sections 7 to 12)

**Status:** STABLE (2026-11-25, verified the same day on the laptop, the
Linux x86-64 build of `sbn/B2`; section 11 has the measurements). The
state-by-name round's group B2 ([`state-by-name-round.md`](state-by-name-round.md)
2.1, 4, 4a). Built: seven sites, ids 9 to 15 (`task_yield.h`), for task 0's
waits `Field_WaitTransition` and `Menu_WaitTransition` and the sleep between
them in `GameMode3_Leave`; their resume entries and the steps cut into
functions the resume entries share (`src/game/core/transition.cpp`); the
census tool's chain check for nested sites and its `--callers` view
(`tools/task_census.py`). No change to `snapshot.cpp`, `task.cpp` or the probe.
No ledger entry: no behaviour changes (section 9). Not played by the owner.
Part A2 above is unchanged; where it lists the waits as "not sites" (1, the
last paragraph) this part supersedes it.

## 7. The callers, counted and measured

**From the code.** `Field_WaitTransition` (`0x4967F0`) is reached through
two callee tables, `mode_flow::Callees::wait_transition` (filled in
`src/game/battle/dragon_cmd.cpp`) and `mode_tasks::Callees::wait_transition`
(`src/game/core/title.cpp`), and by name through `SH_CALL`;
`Menu_WaitTransition` (`0x496830`) by name only. A search for the two
functions, the two members, both addresses and every `kOriginals` that names
them finds **six calls, all in `transition.cpp`**: so the round doc's "six
direct calls" (2.1, answer 3) is the whole count, and the tables add no
caller.

| Wait | Caller (original) | How it is called | `run` | Rest of the step after it |
|---|---|---|--:|---|
| `Field_WaitTransition` | `GameMode_Enter` (`0x495900`, mode 1) | `mode_flow::g.wait_transition(1)` | 1 | with input bit 1 the entry point; the next area change's kind, the zone roll, `Field_Request` 0, mode 2 |
| `Field_WaitTransition` | `GameMode_Field` (`0x4959F0`, mode 2), `Field_Request` 1 | `mode_tasks::g.wait_transition(0)` | 0 | step 0, mode 3 |
| `Field_WaitTransition` | `GameMode_Field`, `Field_Request` 5 | `mode_tasks::g.wait_transition(0)` | 0 | the track read again, `Music_FadeOutStop(0xA)` if it changes, mode 1 |
| `Field_WaitTransition` | `GameMode3_Leave` (`0x495C50`, mode 3 step 2) | `SH_CALL` | 1 | the music, the camp or the area asked, `Window_ResetAll`, mode 2 |
| `Menu_WaitTransition` | `GameMode3_Enter` (`0x495BC0`, mode 3 step 0) | `SH_CALL` | 1 | the music, step up |
| `Menu_WaitTransition` | `GameMode3_Leave` | `SH_CALL` | 0 | a sleep, the party set and the loads, `Transition_Start(3)`, the field's wait, the rest above |

The mode handlers run only under `Field_Task`: `GameMode_Handlers` is read by
`FieldTaskLoop` alone (`TitleTask_Run` reads its own `TitleTask_Modes`), and
`GameMode3_Run`'s `StepDispatch` calls the step last. So every chain is
`Field_Task` (or its loop) -> the handler -> (for mode 3, `StepDispatch` ->)
the step -> the wait, and after the step returns nothing of the frames above
it is left but `Field_Task`'s pass end (the clock, the sleep).

**A premise that did not hold: three of the "other" nested yields are the
waits.** Both waits end with a tail jump into their sleep (`objdump` of A2's
ELF: `jmp *mode_flow::g` at `Field_WaitTransition+0x153`, `jmp
*mode_states::g+0xb8` at `Menu_WaitTransition+0x153`, the
`disable_tail_calls` on the second notwithstanding), so on the frame a
`run` 0 wait spends in its last sleep the sleep returns straight into the
caller and the census names the caller. The staging census's
"`GameMode_Field` 55 frames" is `Field_WaitTransition`'s last sleep under
`GameMode_Field` (both return addresses are the two wait calls'), and A2's
"`GameMode3_Leave` 2" is one frame of `Menu_WaitTransition`'s last sleep
(`+0xc3`, the wait call's return) and one of the step's own sleep after it
(`+0xec`). With the yield word the last sleep goes through `Task_YieldAt`,
which is not a tail call, and the census names the wait.

**What the routes reach.** The staging ELF's chains do not name the callers
(its words above the wait are stale ones); A2's ELF and this one name them,
so the measure is this build's census (section 11). Every one of the six
calls is reached on `worldMapAndAreaTransition`; section 11 has the frames
by site and route.

**What is alive across each wait.** In every caller, nothing of its frame:
each step reads its cells, flags and kind before the wait and nothing of them
after, and every word the rest reads (`Field_InputFlags`, `Game_AreaNumber`,
`Music_Track`, the camp and party bytes, the leader's place) is read from the
span afresh after the wait. Checked against the compiled code for
`GameMode_Enter`: after the wait's call it loads only `g_runBase` and the
span, and its frame only in the epilogue. In the wait itself: `run`, a byte
read once from the argument, and which of its two sleeps it stands in. The
uniform tail is what makes the rest cheap: each site's resume entry is "the
rest of the wait, the rest of the step, `Field_Task`'s pass end, its loop",
with no local of any caller to keep.

## 8. The sites

| Id | Name | The yield | Resume entry | Locals (bytes) |
|--:|---|---|---|---|
| 9 | `Field_WaitTransition (GameMode_Enter)` | either of the wait's two sleeps under `GameMode_Enter` | `ResumeWaitEnter`: the rest of the wait, `EnterAfterWait`, `EnterEnd`, `FieldTaskPassEnd`, `FieldTaskLoop` | `WaitLocals` (8): `run`, the phase |
| 10 | `Field_WaitTransition (GameMode_Field, to the menu)` | under `GameMode_Field`, `Field_Request` 1 | `ResumeWaitToMenu`: the rest of the wait, `ToMenuEnd`, the pass end, the loop | `WaitLocals` (8) |
| 11 | `Field_WaitTransition (GameMode_Field, the area change)` | under `GameMode_Field`, `Field_Request` 5 | `ResumeWaitArea`: the rest of the wait, `AreaChangeEnd(Music_Track)`, the pass end, the loop | `WaitLocals` (8) |
| 12 | `Field_WaitTransition (GameMode3_Leave)` | under `GameMode3_Leave` | `ResumeWaitMenuBack`: the rest of the wait, `MenuCloseAfterFieldWait`, the pass end, the loop | `WaitLocals` (8) |
| 13 | `Menu_WaitTransition (GameMode3_Enter)` | under `GameMode3_Enter` | `ResumeMenuWaitOpen`: the rest of the wait, `MenuOpenEnd`, the pass end, the loop | `WaitLocals` (8) |
| 14 | `Menu_WaitTransition (GameMode3_Leave)` | under `GameMode3_Leave` | `ResumeMenuWaitClose`: the rest of the wait, `MenuCloseAfterWait` (site 15's sleep, then the rest), the pass end, the loop | `WaitLocals` (8) |
| 15 | `GameMode3_Leave (its sleep)` | the step's own `Task_Sleep(1)` after its menu wait | `ResumeMenuLeaveSleep`: `MenuCloseAfterSleep` (the loads, the field's wait at site 12, the rest), the pass end, the loop | none (0) |

`WaitLocals` is `run` (the byte, kept as a word) and the phase:
`kWaitTop` (0) for the loop's sleep, `kWaitLast` (1) for the one more sleep
a `run` 0 wait makes when the wait word is 0. A `run` 1 wait never reaches
the second (sites 9, 12, 13 always write phase 0).

**How a wait knows its site.** The waits are called with one byte through
callee tables whose cells a fuzz or the hosted build may fill with Capcom's
code or a recorder, so the call stays as it was and the caller names the
site beside it: `const WaitAt at(site);` sets a static of ours,
`g_waitSite`, for the length of the call, and the wait takes it at its entry
(`std::exchange`, so a nested call could not inherit it). A wait that is not
ours leaves the word, and the `WaitAt` clears it when the call returns. A
wait entered with no site named (Capcom's code calling ours in the hosted
build, a fuzz) sleeps through its plain callee, at no site, as before. In the
compiled `GameMode_Field` the store of 10 or 11 sits right before each wait
call and the store of 0 right after (section 11).

**One text, entered two ways.** Each wait is two functions: the pass after
the loop's sleep (`FieldWaitPass` / `MenuWaitPass`: with the word 0 and
`run` 0 the last sleep and "over", else the frame and "over" when the word is
0) and the loop (`FieldWaitLoop` / `MenuWaitLoop`: the sleep, then the pass).
The wait calls the loop; a resume entry at phase 0 calls the pass and, if the
wait is not over, the loop; at phase 1 nothing of the wait is left. Each
step's rest after a wait is a function the step calls and the resume entry
calls (`EnterAfterWait` and `EnterEnd`, `ToMenuEnd`, `AreaChangeEnd`,
`MenuOpenEnd`, `MenuCloseAfterWait` -> `MenuCloseAfterSleep` ->
`MenuCloseAfterFieldWait`), and `Field_Task`'s pass end is one
(`FieldTaskPassEnd`), called by its loop and by every resume entry here.
`GameMode3_Leave` is cut at all three of its sites, each piece calling the
next, so a resume entry at any of them runs exactly the rest, including the
later yields (the loads' `WaitLoad`, which stay at no site, and site 12).

`WaitResume` checks the locals before anything runs: a `run` above 0xFF or
a phase that is neither sleep is a `Fatal` ("not a wait's frame").

## 9. No behaviour changes

The game's functions call what they called, in the order they called it,
through the same callees: the waits sleep through `mode_flow::g.sleep` /
`mode_states::g.sleep` (now as `Task_YieldAt`'s fifth argument when a site is
named), the steps' statements are the same statements in the same order, cut
into functions. The writes are to `g_waitSite` and `task_yield::g_entries`,
statics of ours outside the span. What changes on the machine: the waits'
last sleep is no longer a tail call, and the stack is deeper by
`Task_YieldAt`'s frame (the wait's yield is `0x458`..`0x498` from the slice's
top on this build, `0x3E8`..`0x3F8` on the round base). The state hash shows it (section 11): four
routes identical tick for tick to the round base.

## 10. Left, for the list after this round

Every nested yield the staging census (2.1) or this round's measured, that
is not a site. A take that stands in one is written "not portable" ("a task
stands at no named place (task 0)"), as before; section 11 shows one.

| Function (original) | Frames (staging, 16 routes; this round, 8) | The yields | Alive across them | What a site would need | Why left |
|---|---|---|---|---|---|
| `GameMode8_Leave` (`0x4964E0`, mode 8 step 2, the mode's way back to mode 1) | 16 (`masterAndManillo`); 16 (the same) | `WaitLoadRest3g(&Mode8Frame)` (the bank's load, a framed sleep a try) and `WaitWord(&Mode8Frame)` (the transition word, a framed sleep while it is set); the 16 frames are all the second (`GameMode8_Leave+0x167`, both helpers inlined) | nothing of the step's frame (the bank byte and the music test are read before, the return cells after); the helpers' frame callback, which is constant per call | the two helpers taking a site from their caller as the waits do (`WaitAt`), the step cut after each (`Music_FadeOut` test, `Transition_Start(0)`, the word's wait; then `Field_ChangeArea` from the return cells, `Music_FadeOutStop` test, mode 1, step 0), two sites with no locals, resume entries "the rest of the helper's loop, the rest of the step, the pass end, the loop" | not a caller of the two waits (the brief's first order); one route, one 16-frame window; its proof needs takes inside that window on `masterAndManillo` |
| `GameMode9_Leave` (`0x496660`, modes 9 and 10 step 2) | 6 (`shisuMinigame`); not run this round | two `Task_Sleep(1)`, then `WaitWord(&LoadingFrame)` | nothing (the bank, the transition, the mode words are all after) | three sites (the two sleeps, the framed wait), the step cut after each, no locals | as above; `shisuMinigame` was not among this round's routes |
| `MenuModeEnter` (`0x4965D0` / `0x4966F0`, modes 9 and 10 step 0) | 6 (`shisuMinigame`); not run | `WaitWord(&LoadingFrame)`, two `Task_Sleep(1)`, `WaitLoadRest3g(nullptr)` | **the argument `file`** (the DAT number, `0xCB` or `0x31A`), an `int` read after the first three yields: four bytes of locals, or one site per caller (`GameMode9_Enter`, `GameMode10_Enter`) | four sites (or eight, per caller), `file` in the locals, the step cut after each yield | as above |
| `WaitLoad` in `GameMode3_Enter` (the menu's DAT) and `GameMode3_Leave` (the party set, the commu file) | 0 on every route measured (File_LoadDone answers at once here) | a sleep a try while `File_LoadDone` is 0 | nothing | a site each, no locals, resume "the loop's test, the rest of the step" (for `GameMode3_Leave` the cut functions are already there) | never reached: no take could prove it |
| `Transition_Kind20` (task 2; A2's 1) | 0 | its own sleeping loop | A2's 1 | A2's 1 | A2's |
| The scene scripts' and the battle's waits | 0 on 16 routes (2.1, answer 1) | - | - | - | none seen; the round doc's 8 |

The census of a route that reaches none of these leaves task 0 at a site on
every frame (section 11): of the eight routes this round, seven.

## 11. Measured (2026-11-25, the laptop, the Linux x86-64 ELF of `sbn/B2`)

The captures are outside the tree, in `~/bof3x-runs/sbn1125/B2/root/` on the
laptop (they hold game data): `sh/` the state-hash runs, `tc/` the census
(`census_table.txt`, the table below), `qs/` the quicksaves, their recipes
and logs, `bin/` the scripts, `selftest1.log`.

- **The build** (`cmake --build build-linux-x86_64 -j 2`): clean; no warning in the files of this change (the one warning, an unused `kDialogueGlyphs` in `title.cpp`, is the round base's: `title.cpp` is rebuilt because it includes `task_yield.h`).
- **The self-test**: exit 0, 10,406 log lines, `snapshot: self-test passed` (the same after the last build, `selftest2.log`).
- **No state hash moved.** `bridgeWalk`, `combat`, `worldMapAndAreaTransition`
  and `shop` as state-hash runs under `orig6`'s conditions (as in 5), this
  build against the round base's ELF, both with the census probe on:
  `statehash.py diff` **identical on all 2,074, 2,602, 2,113 and 3,137
  ticks**.
- **The census agrees.** Eight routes, each run on this build and on the
  round base (the four above from the state-hash runs; `fairyVillage`,
  `masterAndManillo`, `whelpBoss`, `menu_screens` headless, `--direct`,
  `en-US`). `task_census.py --sites` on this build: every frame on which each
  live task is at a site or fresh carries, for each, the site its chain
  names, and **no row carries a site its chain does not stand at (0
  disagreements on all eight)**. Task 0 at no site, by the function that
  yields (the base's names: `GameMode_Field` and `GameMode3_Leave` there are
  the waits' tail-called last sleep and the step's own sleep, section 7):

  | Route | Frames | Task 0 at no site, base | of which | B2 | The nested sites, B2 (frames) |
  |---|--:|--:|---|--:|---|
  | `worldMapAndAreaTransition` | 2,100 | 303 | `Field_WaitTransition` 286, `Menu_WaitTransition` 8, `GameMode_Field` 7, `GameMode3_Leave` 2 | **0** | 9: 150, 10: 5, 11: 134, 12: 4, 13: 4, 14: 5, 15: 1 |
  | `shop` | 3,120 | 214 | `Field_WaitTransition` 208, `GameMode_Field` 6 | **0** | 9: 112, 11: 102 |
  | `fairyVillage` | 4,680 | 268 | `Field_WaitTransition` 262, `GameMode_Field` 6 | **0** | 9: 150, 11: 118 |
  | `masterAndManillo` | 5,520 | 234 | `Field_WaitTransition` 214, `GameMode8_Leave` 16, `GameMode_Field` 4 | **16** (`GameMode8_Leave`) | 9: 118, 11: 100 |
  | `whelpBoss` | 13,140 | 412 | `Field_WaitTransition` 400, `GameMode_Field` 12 | **0** | 9: 208, 11: 204 |
  | `menu_screens` | 1,740 | 25 | `Field_WaitTransition` 20, `Menu_WaitTransition` 4, `GameMode_Field` 1 | **0** | 9: 16, 10: 5, 13: 4 |
  | `bridgeWalk` | 2,040 | 35 | `Field_WaitTransition` 34, `GameMode_Field` 1 | **0** | 9: 18, 11: 17 |
  | `combat` | 2,580 | 16 | `Field_WaitTransition` 16 | **0** | 9: 16 |
  | **all eight** | **34,920** | **1,507** | | **16** | |

  So on seven of the eight routes every live task stands at a site on every
  frame, and on the eighth all but the 16 frames of `GameMode8_Leave`
  (section 10). (`masterAndManillo`'s base file was last written at 5,460
  frames: the probe writes every 60.) The yield's depth from the slice's top
  is `0x458`..`0x498` at sites 9 to 14 and `0x338` at 15.
- **Sites 10 and 11, by the return address.** The chain names
  `GameMode_Field` for both. Every frame carrying 10 has `GameMode_Field+0x221`
  in its chain and every frame carrying 11 `+0x385`; in this ELF's
  `objdump`, `+0x221` follows `Transition_Start` called with `cl` 2, the
  store of 10 to `g_waitSite` and the wait's call (`Field_Request` 1), and
  `+0x385` follows the `cmp $0xff` of the kind, `Transition_Start`, the store
  of 11 and the wait's call (`Field_Request` 5).
- **Portable loads in a live game, forced** (as in 5: `qs/run.sh` - the live
  proof form, then the cross form in the live run's folder with `--run-game
  none` and `BOF3X_QSPORTABLE=1`, then `quicksave_check.py cross`; `orig6`'s
  conditions). The routes were cut to the take's frame and 500 after
  (`bin/cut_route.py`), so the comparison runs about 480 ticks past the
  take: the rest of the wait, the step's tail and some hundreds of frames of
  play. Most frames inside a wait hold a button in the recording (the player
  walks on through the fade), and `make` wants F6 to replace an idle frame:
  the cut route has that one frame made idle (a `wait 1` in place of one
  frame of the hold), which the live run and the loaded run both play, so the
  comparison is of the same inputs. Every loaded run logged `restored
  PORTABLE`. The locals are `run` then the phase (`WaitLocals`); task 2's are
  A2's.

  | Take (`worldMapAndAreaTransition`, `Frame_Counter`) | Where in the wait | Task 0 (site, `run`, phase) | Task 2 | Cross |
  |---|---|---|---|---|
  | 330 | site 9, the first frame | 9, 1, top | site 4, level `0x77FF`, a fade in | **equal, 467 ticks** |
  | 337 | site 9, the middle | 9, 1, top | site 4, level `0x3FFF` | **equal, 460** |
  | 345 | site 9, the last frame (the tail runs next) | 9, 1, top | free | **equal, 490** |
  | **1111** | site 9 during a swing (A2's take, "not portable" then) | 9, 1, top | site 8, level `0x3BFF`, done 0 | **equal, 483** |
  | 1129 | site 9, the last frame after the swing | 9, 1, top | free | **equal, 475** |
  | 358 | meant as site 11's first frame; the idle frame moved the area change by one, and the take stood at site 1 | 1 | free | equal, 480 (not a site-11 take) |
  | 360 | site 11, early (taken again for 358) | 11, 0, top | site 4, level `0x1800`, a fade out | **equal, 478** |
  | 365 | site 11, the middle | 11, 0, top | site 4, level `0x4000`, a fade out | **equal, 493** |
  | 373 | site 11, its last loop frame | 11, 0, top | free | **equal, 488** |
  | 374 | site 11, the last sleep | 11, 0, last | free | **equal, 487** |
  | 633 | site 10, the first frame | 10, 0, top | site 4, level `0x2000` | **equal, 484** |
  | 636 | site 10, its last loop frame | 10, 0, top | free | **equal, 481** |
  | 637 | site 10, the last sleep | 10, 0, last | free | **equal, 480** |
  | 639 | site 13, the first frame | 13, 1, top | site 4, level `0x5FFF`, a fade in | **equal, 478** |
  | 641 | site 13, the middle | 13, 1, top | site 4, level `0x1FFF` | **equal, 476** |
  | 642 | site 13, the last frame | 13, 1, top | free | **equal, 475** |
  | 674 | site 14, the first frame | 14, 0, top | site 4, level `0x2000` | **equal, 473** |
  | 676 | site 14, the middle | 14, 0, top | site 4, level `0x6000` | **equal, 471** |
  | 678 | site 14, the last sleep | 14, 0, last | free | **equal, 469** |
  | 679 | site 15, the step's sleep | 15 | free | **equal, 480** |
  | 680 | site 12, the first frame | 12, 1, top | site 4, level `0x5FFF`, a fade in | **equal, 480** |
  | 682 | site 12, the middle | 12, 1, top | site 4, level `0x1FFF` | **equal, 480** |
  | 683 | site 12, the last frame | 12, 1, top | free | **equal, 481** |

  Twenty-three takes, every one of the seven sites in it, each wait early, in
  the middle and on its last frame (and, for the three `run` 0 waits, on the
  last sleep), with task 2 in a fade (site 4), in a swing (site 8) or free:
  **every one equal tick for tick**. So no resume entry skips or repeats any
  of the work after its yield, as far as some 470 ticks of play after it show.
  The take A2 could not make portable (FC 1111) is portable now.
- **A take on a yield left is still not portable.** `masterAndManillo` at
  `Frame_Counter` 3297, task 0 in `GameMode8_Leave`'s framed wait (section
  10), task 2 at site 4: written **not portable** ("a task stands at no named
  place (task 0)"; the TASKS record: task 0 nowhere); the exact restore in the
  live run worked (back to 3297 at 3397); the forced portable load **refused**
  it, "forced portable (not portable)", the save half offered.
- **The no-data checks**: `ledger_check`, `ledger_registry`,
  `ledger_switches`, `tu_renames`, `tu_census --strict`, `retire_shells
  --strict`, `os_census` (three forms), `exe_own`, `tables`, `reloc_bare`,
  `width_census`, `data_census --check` and `--strict`, `reloc_census`,
  `region_table` pass. `data_census --strict` wanted four keys of
  `analysis/typed_left.tsv` renamed, their reasons kept: two raw sites moved
  from `GameMode_Enter` into `EnterAfterWait`, two from `GameMode3_Leave`
  into `MenuCloseAfterFieldWait`.

## 12. Not checked

- **The bodies' fuzz** (`mode_flow_fuzz.cpp` covers `Field_WaitTransition`
  and `GameMode_Enter`, `mode_tasks.cpp`'s covers `GameMode_Field`,
  `mode_rest`'s and `mode_states`'s the mode-3 steps and the menu's wait):
  built only into the 32-bit hosted build, which this laptop cannot build. By
  the source the recorded calls are the same, in the same order, through the
  same callees: with no site named (a fuzz calls the function itself, not
  through a `WaitAt`) a wait sleeps through its plain callee exactly as
  before; the steps' `WaitAt` writes only `g_waitSite`. A fuzz's long jump out
  of a sleep recorder skips a `WaitAt`'s end, but the wait took the word at
  its entry, before any sleep, so it is clear after the jump too. A reading, not a run;
  owed on the desktop.
- The 32-bit build: not built. `WaitLocals` are fixed-width words.
- Portable loads **across builds**: B3's.
- Sites 10, 11, 13 and 14 on routes other than `worldMapAndAreaTransition`
  for the takes (the census reaches 10, 11 and 13 elsewhere: section 11);
  sites 12, 14 and 15 are reached by that one route alone (one menu visit).
- `GameMode3_Leave`'s camp branch (`MenuLists_CampChosen`) and its party and
  commu loads: inside the cut functions, but not reached by a take (the route's
  menu visit takes none of them).
- The hosted build with Capcom's waits in the callee tables: a step's
  `WaitAt` sets the word, Capcom's wait never takes it, the `WaitAt` clears
  it, and the take is not portable (no entry written). By the source; not run.

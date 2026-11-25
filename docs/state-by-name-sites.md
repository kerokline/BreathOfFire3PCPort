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

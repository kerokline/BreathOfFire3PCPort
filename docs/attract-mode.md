# The attract sequence, the task system, and why the RNG is deterministic

**Status:** IN PROGRESS (2026-09-19 — the oracle works; see §6)

Groundwork for [`IDEAS.md`](IDEAS.md) I6 — using the attract sequence as a
whole-engine regression oracle, the way TR1X used demo playback
([`prior-art/tr1x.md`](prior-art/tr1x.md) §2.5). Observations are from a live
session on 2026-09-19, made **read-only** (`tools/attract_watch.py`:
`ReadProcessMemory` only, no input, nothing written to the game). Recordings
live under `analysis/attract/`, gitignored.

## 1. It exists on the PC port

Owner, 2026-09-19: left idle on the title screen for about a minute, the port
plays through several areas with story dialogue, the logo overlaid — real
engine scenes with real message boxes, not FMV, as on the PlayStation. It keeps
running across focus changes (frames are replayed on refocus, not dropped —
[`windowed-mode.md`](windowed-mode.md), "Focus loss").

**The game only advances while its window is the foreground window.** An
unattended recording must put the window in front first (`SetForegroundWindow`
is enough; no input reaches the game) and must treat `active = 0` rows as a
frozen game, not a quiet one.

## 2. The task system: four cooperative coroutines

Found while reading the top-level loop. Hand-written assembly — fixed
push/pop sets around a raw `esp` swap, not compiler output — and the PC
counterpart of the PSX task system. Per-claim evidence is in
[`symbols.toml`](../symbols.toml), "Task system".

- **`Task_Records` `0x66C7D0`**: four records of `0x20` bytes — `u16` state
  (0 free / 1 sleeping / 2 runnable), `u16` frames left to sleep, saved `esp`,
  then `0x18` bytes of task-private words.
- **`Task_Create(slot, entry)` `0x5A9914`**: gives the task a **`0x4000`-byte
  stack** carved downward from `[0x66C858]`, and plants `entry` where the
  scheduler's `ret` will find it. WinMain creates task 0 at `0x496B60`.
- **`Task_Sleep(frames)` `0x5A9949`**: saves callee state onto the task's own
  stack, records `esp`, jumps back into the scheduler. `Task_Sleep(1)` is
  "yield until next frame" and is how every wait loop in the game is written.
- **`Task_RunAll()` `0x5A98A0`**: once per logic frame from WinMain's loop;
  resumes each runnable task in slot order.

Consequences:

- **All game logic runs on one OS thread**, in a fixed slot order, once per
  30 Hz logic frame. There is no scheduling nondeterminism to worry about. (The
  process has five threads; the others are not running game logic — DirectSound
  / MCI / CRT, by elimination, unread.)
- **Our replacement code runs on those 16 KB stacks** —
  [`SCAFFOLDING.md`](SCAFFOLDING.md) §3, "Hazard".
- Task 0's private words at `0x66C7E8` / `0x66C7EA` are the top-level **mode**
  and **step**; `0x588E88` loops `Task_Sleep(1)` → `call [0x667294 + 4*mode]`.
  That table has two real entries, `0x588EB0` (boot: `LoadDatFile(0x31D)`, then
  waits) and `0x587DB0` (title/load flow, itself a 15-state machine on
  `0x6BDF86` through the table at `0x66720C`).
- For decompilation: a function that calls `Task_Sleep` is a coroutine body.
  It can be written as ordinary C++ and detoured like anything else — it is
  resumed on its own stack — but it must not hold a lock or an RAII guard
  across the yield.

## 3. `Rand` is a pure function of its call count

- `Rand` `0x5B93D2` is MSVC6 `rand()`: `seed = seed * 0x343FD + 0x269EC3`,
  return `(seed >> 16) & 0x7FFF`, with the seed in the calling thread's CRT
  block at `+0x14`.
- The seed is initialised to **1** (`mov dword [reg+0x14], 1` at `0x5BAD5C`, in
  the CRT's per-thread init).
- **`srand` is not in the binary.** A byte-pattern search of `.text` for its
  body (`call _getptd; mov ecx,[esp+4]; mov [eax+0x14],ecx; ret`) finds
  nothing, and the only store to `+0x14` after `_getptd` is the one inside
  `rand`. The linker dropped it as unreferenced. 745 call sites call `rand`.

So with all game logic on one thread (§2), **the RNG state at any moment is
determined by how many times `Rand` has been called since launch.** Nothing
seeds it from the clock. Two launches that make the same calls in the same
order get the same numbers.

This also sharpens [`DIVERGENCE.md`](DIVERGENCE.md)'s differential-testing
caveat: PSX and PC sequences differ because the *generators* differ, but each
side is reproducible on its own.

Measured: `tools/attract_watch.py` recovers the call count by reading the seed
from the main thread's CRT block (TLS index at `0x672950`) and stepping the LCG
from 1. At the title screen, ~1 minute after launch, the count was **60**;
during the first attract scene it rose by **8 per logic frame** for stretches
and stood still for others. So the attract sequence *does* consume random
numbers — a drift in any replaced function that calls `Rand` once too often
would desynchronise everything after it, which is exactly the sensitivity an
oracle wants.

## 4. What an oracle would compare

Cheap, external, and already working: the per-frame tuple the sampler logs —
**logic frame, `Rand` call count, message index `0x7DEE48`, area word
`0x904EFC`, mode/step words**. The area word was doubted earlier the same day
(it sat at 4 through a 100 s watch); it does change during the attract
sequence (`0x1F → 0x02 → 0x04` in the first 90 s), so the address is live and
that watch simply caught one long scene.

The sampler counts frames from outside by watching a byte flip, which
undercounts if the sampler stalls. A real harness should log from **inside**
the process — a detour on `Task_RunAll` gives an exact frame number for free —
and that is the natural next use of the injected DLL.

## 5. Results of the first recording

`analysis/attract/run1.tsv`, 2026-09-19: 25 minutes, unattended, window in the
foreground throughout (0 inactive rows), launched through `bof3x-launcher` with
`File_Read` and `Save_WriteFile` ours. 46,875 logic frames in 1,500 s =
31.25 per second by the external count. 8,814 change rows; eight complete
cycles.

**The sequence is exactly periodic: 5,522 logic frames per cycle (about
2 min 57 s), eight cycles out of eight.** Aligned on the frame the area word
becomes `0xFFFF`, the message/area timeline is **identical in every cycle, to
the frame**:

| frame | message index | area word |
|---|---|---|
| +0 | 2 | `0xFFFF` (one frame — a load in progress) |
| +1 | 2 | `0x0004` |
| +241, +429 | 7, 8 | `0x0004` |
| +1201, +1356, +1602, +1827 | `0x0C`, `0x0D`, `0x0E`, `0x0F` | `0x0004` |
| +2128 | | `0x001F` (the title screen: 416 frames, under 14 s) |
| +2544 | | `0x0002` |
| +2561, +2694 | 1, 2 | `0x0002`, which then runs 2,828 frames to the end |

As recorded that is two area *numbers* (4 and 2) either side of a short return
to the title. It is more than two *locations*: §7.

Including the mode word, cycles differ only in whether the sampler caught a
state that lasts a single frame (26-28 segments per cycle, all shared
transitions at identical offsets) — a limit of sampling from outside, and the
argument in §4 for logging from inside.

**`Rand` consumption is *not* identical per cycle — and that is the expected
result, not a failure.** Calls per cycle: 14126, 14130, 14116, 14112, 14120,
14112, 14126, 14104. Decomposed:

- the area-2 scene consumes **exactly 14,064 calls in every cycle** (8 per
  frame over 1,758 frames — a fixed-rate effect);
- the area-4 scene consumes **40 to 66**, always even, different each cycle.

The generator is never reset between cycles, so each cycle starts from a
different state; something in the area-4 scene draws a number and uses it to
decide *when it next draws* (a random-interval effect), so the count in a fixed
window depends on the values drawn. Nothing about the *timeline* depends on
them. This is what a deterministic program with a free-running RNG looks like,
and it means **the cycle-to-cycle comparison cannot test RNG determinism — only
a launch-to-launch comparison can.** What this run does establish: scripted
timing is frame-exact and independent of the RNG; no audio-synchronised or
clock-dependent wait perturbs it over 25 minutes; and running two of our
functions in the process did not disturb it.

## 6. Cross-launch determinism: confirmed, and the oracle's first run

2026-09-19, four fresh launches, no input of any kind, each recorded from the
first frame by `tools/attract_run.py` and compared by `tools/attract_diff.py`:

| Run | Configuration | Frames compared |
|---|---|---|
| `orig_a` | `BOF3X_ORIGINAL=*` — Capcom's code throughout | reference |
| `orig_b` | `BOF3X_ORIGINAL=*` | 7,478 |
| `ours_c` | `File_Read` and `Save_WriteFile` ours | 7,478 |
| `explore_noinput` | same as `ours_c`, shorter | 2,835 |

**Every run agrees with the reference at every logged frame: `Rand` call
count, message index and area word** — `attract_diff.py` exit 0, all three
comparisons. In all four, the first attract frame is logic frame **1,311**
after launch, and `Rand` has been called **0** times before it.

So §3's prediction holds by measurement, not only by construction: **the port
is deterministic from launch through the attract sequence, to the frame and to
the random number**, across processes, and sampling from outside at ~5 ms was
fine enough that no frame was lost in 5 minutes. It also means the 58 s of
wall-clock time before the first scene (two FMVs, modal, outside the logic
loop) contributes no variation.

`orig_a` vs `ours_c` is, in effect, **the project's first regression test of
our code against Capcom's**: same process, same loader, one environment
variable apart, identical result. Its reach is honest-sized: the attract
sequence exercises `File_Read` heavily (every asset load) and
`Save_WriteFile` not at all.

**Negative control.** The comparer does fail when it should: `orig_a` against
the 25-minute `run1` (joined mid-session, so its first aligned frame carries
`Rand` = 14,124 rather than 0) disagrees from frame +0.

One more fact these runs settle: a cycle begun from a fresh launch uses 60
`Rand` calls in the area-4 scene — so §5's "varies with the generator's state"
is reproducible too: same state in, same count out.

### Using it

```
python tools/attract_run.py --out analysis/attract/ref.tsv --original "*"
python tools/attract_run.py --out analysis/attract/new.tsv
python tools/attract_diff.py analysis/attract/ref.tsv analysis/attract/new.tsv
```

Five minutes per run by default (launch, ~1 min to the first scene, one full
5,522-frame cycle). The runner ends any running `BOF3.exe`, starts the game
through the launcher, keeps its window in front, sends nothing, and ends the
game afterwards. Do not use the machine's keyboard or mouse on the game window
during a run; using other windows is fine in principle — the runner takes
focus back — but each focus loss freezes the game until it does.

A **ledgered divergence that changes what the attract sequence does will fail
this comparison by design.** That is the point at which the reference has to
be re-recorded deliberately and the ledger entry cited as the reason — never
regenerated to make a failure go away
([`prior-art/openrct2.md`](prior-art/openrct2.md) §2.5).

## 7. What it loads: three area files, four locations

The owner remembered more locations than the two area numbers in §5 (2026-09-19)
and guessed the reason: one area file can hold several locations. **Correct**,
shown two ways.

*From the recordings:* inside the area-4 stretch, the mode word drops to 1 for
17 frames at **+1010** and returns to 2 — the same signature as every area load
(+1, +2128, +2544) — while the area word stays 4, and the dialogue jumps from
messages 7-8 to `0x0C`-`0x0F`. A scene change with no area change.

*From inside the process:* `File_Open` became ours for this (faithful, plus one
log line per open; `src/game/file_io.cpp`), and `tools/attract_opens.py` groups
the log into load bursts. One launch and the start of the second cycle:

| after first open | opened |
|---|---|
| +0.0 s | `DAT/FIRST.DAT`, `DAT/DEMO.DAT`, `BGM/141.DAT` (**fails**), `BGM/141N.DAT` |
| +41.9 s | `DAT/PL27A.DAT`, `BGM/006.DAT`, `DAT/AREA004.DAT` |
| +110.0 s | `DAT/AREA031.DAT` |
| +122.8 s | `BGM/002.DAT`, `DAT/AREA002.DAT` |
| +176.7 s | `DAT/DEMO.DAT`, `BGM/141.DAT` (fails), `BGM/141N.DAT` |
| +218.6 s | `BGM/006.DAT`, `DAT/AREA004.DAT` |

**Nothing is opened at the +1010 scene change** (about 34 s into area 4): the
second location is drawn from `AREA004.DAT` already in memory. So a cycle is
four locations from three area files — area 4 twice, the title area 31, area 2
— and whether area 2's long 2,828-frame stretch also moves between locations
without a mode-1 blip is not something these three words can show.

Also settled by this log:

- **The area word is the file number.** `0x904EFC` read 4, `0x1F`, 2 while
  `AREA004`, `AREA031`, `AREA002` were opened: now `Game_AreaNumber` in
  `symbols.toml`, evidence tier. **Area 31 is the title screen.**
- **`DAT/DEMO.DAT` is reloaded at the top of every cycle** and at boot. Six
  chunks (`tools/dat.py list`): four images, one `0x1000`-byte data chunk at
  tag `0x8800`, and a nine-sample audio bank. The images are presumably the
  logo overlay; the data chunk is the first place to look for what *drives*
  the sequence. Unread.
- **`BGM/NNN.DAT` then `BGM/NNNN.DAT`.** For track 141 the plain file does not
  exist in the install and the open fails; the game then opens `141N.DAT`.
  Tracks 006 and 002 exist plain and no `N` variant is tried. So the `N` name
  is a **fallback**, which answers half of [`DAT_CONTAINER.md`](DAT_CONTAINER.md)
  §5's numbering question. **`N` = no loop, read 2026-09-19**
  ([`asset-loading-path.md`](asset-loading-path.md) §1a): the failed open is
  the designed probe for which kind of track this is, not a missing file.
- A failed open costs nothing visible — but note the original reports it only
  as -1, and the two callers in the sound module evidently handle that.

Regression: the run with `File_Open` ours compares **identical** to the
all-original reference over 7,478 frames (`attract_diff.py` exit 0).

## 8. Open

- **What starts it**: the idle timer on the title screen and the table of
  scenes it walks. Unread.
- ~~Cross-launch determinism~~ — confirmed, §6.
- **Reach.** Two field scenes, text, area loads, one fixed-rate and one
  random-interval effect. No battle, no menu, no input path. A scripted-input
  replay would extend it, and now has a deterministic base to stand on — the
  hard part would be delivering input on an exact logic frame, which needs the
  in-process logger below.
- **Log from inside.** Take over `Task_RunAll` `0x5A98A0` (hand-written
  assembly — read its register contract first) or hook the frame loop, for an
  exact frame counter and a state hash richer than three words.
- **Receipts.** This is the differential run [`STATUS.md`](STATUS.md)'s receipt
  policy was waiting for: git SHA, fixture ids, the two configurations, frames
  compared, result.
- ~~Does anything read the wall clock inside game logic?~~ **No** (import
  xref, 2026-09-19). The only time sources imported are `GetTickCount` — one
  reference in the whole of `.text`, WinMain loading it into `esi` for frame
  pacing and the FPS counter — plus `FileTimeToSystemTime` /
  `FileTimeToLocalFileTime` / `GetTimeZoneInformation`, each referenced once,
  inside the CRT (`_findfirst`'s time conversion and `tzset`). No
  `QueryPerformanceCounter`, no `timeGetTime`, no `GetSystemTime` /
  `GetLocalTime`. **Game logic has no clock.** What remains as a
  nondeterminism source is anything that *waits on a device*: DirectSound
  play-position or MCI status polled from logic (an audio-synchronised wait
  would make frame counts vary run to run), and input. Unread.
- **`BOF3X_ORIGINAL=*` for oracle runs**, or every ledgered change reads as a
  failure ([`prior-art/tr1x.md`](prior-art/tr1x.md) §3).

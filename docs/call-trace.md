# First-call trace: which functions a run reaches

**Status:** IN PROGRESS (2026-09-19 — tool works; first attract trace taken;
re-checked 2026-09-24 — §9 and §10 carry "since" notes)

An in-process answer to "what code does the attract sequence actually run, in
what order, called from where" — [`IDEAS.md`](IDEAS.md) I4 and the "log from
inside" item of [`attract-mode.md`](attract-mode.md) §8. Code:
`src/hook/calltrace.cpp`, `tools/calltrace.py`. Outputs are derived from game
code and live under `analysis/calltrace/`, gitignored.

## 1. How it works

With `BOF3X_CALLTRACE` naming an entry list, the DLL writes a one-byte `int3`
over every listed function entry before the game's entry point runs. A vectored
exception handler takes the first arrival at each: it records **(logic frame,
entry, return address at `[esp]`, thread)**, restores the original byte, and
resumes at it. Each function therefore costs one exception per run and
afterwards executes exactly as Capcom built it.

One entry is re-armed after every hit, by a single-step: `Task_RunAll`
`0x5A98A0`. WinMain calls it once per logic frame, so counting its hits is the
**exact in-process frame counter** `attract-mode.md` §4 asked for. Hits are
flushed to `build/bof3x.calltrace.tsv` from that breakpoint — on WinMain's
stack — so the handler does no I/O on a 16 KB task stack, and a `taskkill`
loses at most one frame of hits.

*Since 2026-09-25 `Task_RunAll` is ours* ([`task_sched.md`](task_sched.md)
section 5): it is the one owned entry the tracer still arms - its `int3`
then sits on the detour's `jmp` - and our WinMain calls it through
`0x5A98A0` (`bof3::orig::Task_RunAll`), so a frame is still an arrival
there.

It is instrumentation, not replacement: no original code is skipped, so there
is no ledger entry. It does leave `.text` writable for the process's life,
which is why it is off unless asked for.

## 2. The entry list, and what it cannot see

`tools/calltrace.py entries` takes the 2,952 entries of
`analysis/pc_funcs.json` and keeps those whose preceding instruction ends a
function (`ret`, `jmp`, `int3`, `nop`): **2,936 kept, 16 dropped.** The filter
exists because `pe_funcs.py` takes every direct call target from a linear
sweep, and the sweep also decodes the jump tables MSVC leaves in `.text`; an
`int3` written into a table would corrupt data instead of trapping. The 16 are
printed by the tool; most are in the CRT range above `0x5B0000`.

**Blind spot: functions reached only through pointers.** An entry that is never
the target of a direct `call` is not in `pc_funcs.json` and so is not armed —
task entry points, the mode table's `0x588EB0` / `0x587DB0`, script-opcode
handlers. Their *callees* are seen, with a return address inside the unlisted
function, so the report's `caller_func` column attributes those to whatever
listed entry precedes them. Treat `caller_func` as "nearest entry below",
not as proof.

## 3. First trace: one attract cycle from launch

`analysis/calltrace/trace_a.tsv`, 2026-09-19: `attract_run.py` defaults
(300 s, 8,872 logic frames), `BOF3X_ORIGINAL=*`.

- **540 of 2,936 armed functions are reached (18.4%)**, all on one thread —
  consistent with [`attract-mode.md`](attract-mode.md) §2's "all game logic on
  one OS thread"; nothing in `BOF3.exe`'s `.text` ran on the other four.
- **The tracer does not perturb the run.** `attract_diff.py` against the
  all-original reference `orig_a.tsv`: identical `Rand` count, message index
  and area word at all 7,478 frames; first attract frame 1,311 in both.
- New code arrives in two bursts: **256 functions by frame 300** (boot, title)
  and **230 in frames 1,200-1,499** (the first area load — the external sampler
  puts the first attract frame at 1,311, and `Rand`'s first call lands at
  1,313). After that only 54 more, the last at frame **4,305**; the area-2
  scene and the second cycle add almost nothing the area-4 scene had not
  already run.
- Of the 44 named functions armed, 28 are reached. Not reached: the three
  `Battle_*`, all save functions, `Msg_OpenSystem` / `Msg_SystemPtr`,
  `Fmv_EnterFullscreen` (the run was windowed), and the write/seek half of the
  file layer — which states the oracle's reach
  ([`attract-mode.md`](attract-mode.md) §8) in function terms.
- `LoadDatFile`'s three sinks (`Snd_LoadBank`, `Gfx_LoadImage`,
  `Font_SetGlyphData`) are all first called on frame 1, from `LoadDatFile`,
  confirming [`asset-loading-path.md`](asset-loading-path.md) §2 dynamically.

## 4. Run-to-run stability: identical but for one audio-timed call

A second traced launch (`trace_b.tsv`, 2026-09-19, same configuration; its
`attract_diff.py` against `orig_a.tsv` is also identical):
`calltrace.py compare` finds **the same 540 functions, in the same order, from
the same return addresses**, and 539 of 540 on the same frame. The one
exception:

| entry | caller | run A | run B |
|---|---|---|---|
| `0x5A7050` | `0x5A726E` (in `0x5A7230`) | frame 1,034 | frame 1,035 |

Read 2026-09-19, and it is the device wait [`attract-mode.md`](attract-mode.md)
§8 left open. `0x5A7230` is called from `0x587C70`, which WinMain's loop calls
on every spin, not once per logic frame (§5: about 350 times per frame); it polls two
event handles at `0x7DE3B4` with `MsgWaitForMultipleObjects(2, ..., timeout 0)`
(import slot `0x5C4164`, resolved from the import table). When one has fired
and the flag at `0x7DE3E0` is set, it calls `0x5A7050`, which on the COM object
at `0x7DE3C4` calls vtable `+0x24` for a status dword and, if bit 0 is set,
vtable `+0x48` — on `IDirectSoundBuffer` those slots are `GetStatus` and
`Stop`, bit 0 `DSBSTATUS_PLAYING`. So: the streaming-audio pump, stopping a
buffer when DirectSound signals a play-position event. **That event is timed
by the audio device's clock, not by logic frames**, so which frame sees it can
move by one. Frame 1,034 is before the first attract frame (1,311); that this
is the non-looping title track `141N` ending is a guess, not checked.

Consequences:

- The *set and order* of first calls is a run-to-run invariant; *frames* are,
  except downstream of `0x5A7230`. A trace comparison used as a regression
  check should compare order and callers, and treat a frame-only difference
  under `0x5A7230` as known noise — `compare` prints frame-only differences
  separately for this reason (and still exits 1).
- Nothing the oracle watches depends on it: `Rand`, message and area agree at
  every frame in both runs. Audio end-of-stream does not feed game logic here.
  Whether anything *waits* on audio elsewhere (a scene holding for a jingle)
  is still open.

## 5. Every call: counts, edges, and what slowing the game down shows

`BOF3X_CALLTRACE_MODE=all` re-arms **every** entry after each hit, the way
`Task_RunAll` is, and adds two files next to the DLL: `bof3x.callcounts.tsv`
(calls per entry and per `(entry, return address)` edge, rewritten every 256
frames because the run ends by `taskkill`) and `bof3x.callframes.tsv` (per
logic frame: calls made, and an FNV-1a hash of their `(entry, caller)`
sequence).

One run, 2026-09-19 (`analysis/calltrace/all_a/`), 300 s, `BOF3X_ORIGINAL=*`:

- **Cost: the game runs at under half speed.** 4,188 logic frames against
  8,872 untraced; 26.6 million calls to frame 4,096, two exceptions each.
  Mean 6,438 calls per frame, peak 29,657. 524 functions, 1,178 edges; 62
  functions are called exactly once.
- **Game logic does not notice.** By the in-process frame counter, `Rand` is
  first called on frame 1,313, `Window_Task` on 1,310, `Msg_OpenScript` on
  1,552 — the same frames as the full-speed traces. Logic is a function of the
  frame count, not of time, under a 2x slowdown too.
- **The external sampler does notice, and is wrong.** `attract_diff.py` reports
  the first attract frame at 1,095 instead of 1,311 and 26 frames of `Rand`
  disagreement. The in-process numbers above contradict it: the sampler counts
  frames by watching a byte and reads its words non-atomically, and both fail
  when a frame is long. This is [`attract-mode.md`](attract-mode.md) §4's
  argument for logging from inside, now measured. **Do not run the oracle with
  mode `all` on.**
- **The presentation layer is what moves.** Against `trace_a`, every first-call
  difference is in the draw path `0x59F0xx`..`0x5A3xxx` (ten functions first
  called on frame 6 instead of 2, five on 1,359 instead of 1,330, thirteen not
  reached by frame 3,935), plus the audio call of §4 and `0x461FC0`
  (1,312 vs 1,359, unread). The port skips drawing when it is behind —
  [`windowed-mode.md`](windowed-mode.md)'s "replays the missed time
  unrendered" — so draw calls depend on wall-clock time and logic calls do
  not. That gives a mechanical test for [`IDEAS.md`](IDEAS.md) I8: **a function
  whose call count changes with speed is presentation; one whose count does
  not is logic.**
- The hottest entries are all unnamed: `0x5A7BF0` (2.6 M), `0x5A8380`
  (2.5 M), `0x5B3760` (2.0 M), `0x5A8340` (1.6 M), then the audio pump pair
  `0x587C70` / `0x5A7230` at 1.45 M each — single caller `0x4FCEC1` in
  WinMain's loop, about 350 calls per logic frame, which is why §4's event can
  land on either side of a frame boundary.

Consequence for the per-frame hash: over all entries it covers draw and pump
calls, so it cannot be compared across runs. §6 removes them.

## 6. A per-frame call hash that is identical across launches

2026-09-19. The aim: a per-frame state check far richer than the oracle's
three words — every logic call of the frame, in order, with its caller.

**Finding the speed-dependent entries.** Two every-call runs to the same frame
(`BOF3X_CALLTRACE_STOP=3072`), the second slowed by
`BOF3X_CALLTRACE_SPIN=3000` (`all_fast`, `all_slow`): 508 functions called in
both, 19.82 M vs 19.97 M calls, **76 totals differ** — 48 in the CRT range
`0x5B....`, 18 in `0x5A....`, 6 in `0x59....`, and one each in `0x58`, `0x46`,
`0x45`, `0x43` (`analysis/calltrace/volatile.txt`). The speed contrast was
modest (the spin was too small), so equal totals are weak evidence of
independence, as the next step showed. `calltrace.py volatile` writes the
entry list without them, `entries_logic.txt`; unarmed entries also cost
nothing, and a run with it reaches ~4,380 frames in 270 s against ~3,500.

**Two things the totals missed**, found by comparing launches and then logging
every call of the differing frames (`BOF3X_CALLTRACE_DETAIL=lo-hi`):

| what moves | between frames | calls |
|---|---|---|
| `0x461FC0` (from `0x461F6C`) → `Gfx_LoadImage` `0x59EA70` → `0x59EB00`, `0x59EBB0` x15, `0x59E700`, and four each of `0x5A2CA0` / `0x5A04C0` / `0x59F840` x2 / `0x59F900` / `0x5A9E1E` | 1,334 / 1,335 | 43 |
| `0x5A0080` from `0x5A001C`, `0x5A9BFA` from `0x5A0405`, four each (draw path; rare, found in §7) | 2 / 3 | 8 |
| `0x5A6FB0` from `0x587C9B` (inside the audio pump `0x587C70`) | 1,333 / 1,334 | 3 |

Same totals in every run, different frame: the block lands whole on one frame
or the next, each launch one way or the other. The second is §4's audio pump
again. **The first is frame skipping, read 2026-09-19.** WinMain's loop reads
`GetTickCount` and compares it with the frame deadline at `0x6BC628`
(`0x4FCE24`..`0x4FCE3D`); only if there is time left does it take the branch
that calls, in order, `0x5A7860`, `0x5A7890`, `Gfx_FlushDirtyStrip` `0x454960`,
`Gfx_FlushUploadQueue` `0x461F00`, and the draw `0x59EE50`, and counts a
rendered frame for the FPS title. It then spins on the audio pump `0x587C70`
and `GetTickCount` until the deadline. `0x461F00` walks a queue — count byte
`0x9035A0`, x and y arrays at `0x903680` / `0x9036A8`, record pointers at
`0x92BF20` — and for kind-1 records calls `0x461FC0`, which unpacks six 5-bit
fields per dword and hands the result to `Gfx_LoadImage`. Evidence per claim is
in [`symbols.toml`](../symbols.toml). Measured: `0x461F00`, `0x454960` and
`0x59EE50` have the same call count, **1,200 in 3,072 logic frames** in one
run and 1,158 in the slower one.

So game logic *queues* an image upload deterministically, and the upload
*happens* on the next rendered frame — a wall-clock matter. Nothing logic
reads depends on it, which is why the oracle never saw it. Two consequences:

- **For the `Gfx_LoadImage` takeover:** its calls have no fixed logic frame,
  and `tools/mem_dump.py`'s VRAM-shadow comparison is only sound at a moment
  when nothing is pending — count byte `0x9035A0` and flag `0x937F90` both
  zero. **`mem_dump.py` now waits for that** (2026-09-19): at the target frame
  it reads both under suspension and snapshots only when both are zero,
  recording what was pending and how many logic frames the wait cost in
  `<label>_meta.json`. Both are cleared only *after* their flush finishes
  (`0x461FA7`, `0x45499A`), so zero under suspension also means no flush is
  half done. The `LoadDatFile` pass was a quiet moment, and a likely one
  rather than a certain one: two all-original runs (`drain_a` / `drain_b`)
  found count 0 and flag 0 at the default point and dumped identical bytes in
  both regions, while 60 s of sampling one of them (8.9 M reads, areas 4, 31
  and 2) saw the flag set in 2,945 reads and the count non-zero in 81, never
  above 1. The wait path itself has only run against a simulated process —
  no live dump has yet landed on a pending moment. Not covered: a suspension
  that lands inside a *direct* `Gfx_LoadImage` call from `LoadDatFile`, which
  bypasses the queue; the original-vs-original pair is the guard for that.
- **This is the presentation seam, found from the other side**
  ([`IDEAS.md`](IDEAS.md) I8, I10 item 3): the rendered-frame branch at
  `0x4FCE3F`..`0x4FCEB8` is where deferred work crosses from logic to display,
  and the two queues are its interface.

**Result.** With those eleven also dropped (`volatile --also`, 2,850 entries
armed), two fresh launches `hash_e` / `hash_f`: **calls and hash identical on
all 4,486 frames** (`calltrace.py frames`, exit 0) — 20.1 M calls, mean 4,485
per frame, each hashed with its return address. One pair is a small sample:
the two jitters above each showed in roughly half of launches, so a rarer one
could still be hiding. When a pair differs, the DETAIL log names the calls in
two short runs.

### The exclusion list, rebuilt from structure

Offline, 2026-09-19, from `all_a`'s call edges. Once the frame-skip read above
named the wall-clock-timed call sites in WinMain's loop — the rendered-frame
branch `0x4FCE3F`..`0x4FCEB8`, the pump call returning to `0x4FCEC1`, and the
once-a-second FPS `sprintf` returning to `0x4FCF08` — the list no longer has to
be found one jitter at a time: `calltrace.py wallclock` drops **everything
reachable from those sites by observed edges**. 7 roots, 89 functions.

Checked against the list built by totals and by hand (76 + 13): **87 of its 89
are reproduced**, including all 13 hand-found ones. The two it misses are CRT
entries `0x5BC8E0` and `0x5BDA20`, whose totals differed but which no `all_a`
edge ties to a wall-clock site (kept out with `--also`; why is unread). The two
it *adds* are `0x5A7050` — §4's audio stop, a known jitter the totals never
caught — and `0x5A9A59`. So the structural rule found a real one the empirical
rule had missed. Current `entries_logic.txt`: 2,845 entries. Not yet re-verified
with a pair of launches (the game was in use).

**The seam, as measured** ([`IDEAS.md`](IDEAS.md) I10 item 3). Of the
wall-clock-side functions below the CRT/decoder range, only **seven are also
called from logic code** in `all_a` — the whole dynamic interface between the
two sides in the attract run: `0x437CC0` (4,770 calls), `Gfx_LoadImage`
`0x59EA70` (1,136), `0x5A6AF0` (77), `0x5A6F30` and `0x5A6FB0` (3 each, the
audio pair of §4 and §6), `0x59F840` (2) and the texture-cache invalidation
`0x59E700` (1). Everything else logic does to the screen goes through the two
queues. What `0x437CC0` and `0x5A6AF0` are is unread.

Limit: "observed edges" means edges seen in one attract run with the full list
armed. A scene that reaches new draw code needs a new full-list run first.

### The list was too short, and speed is what showed it (2026-09-20)

With 70 functions ours the A/B pair `ab10` **differed on 29 of 7,428 frames**,
scattered over 3602-3644, 3946-3998 and a few later ones, call counts swapping
by about 185 between neighbouring frames. Every earlier pair, `ab3` to `ab9`,
had been identical over the same frames. It was not the takeovers, and the
record of how that was settled is the point of this section.

1. **`BOF3X_CALLTRACE_DETAIL=3600-3616`, both sides** (`det10_orig`,
   `det10_ours`). Frames 3601-3616 - several of which had differed in `ab10` -
   were now identical call for call, in order; only 3600 differed, by 184
   calls, all made **from inside the draw `0x59EE50`**: `0x5A0C40` from
   `0x59F0D9` x180, `0x5A2EB0` from `0x59F1C7` x2, `0x5A3160` from `0x5A2F07`
   x2. The differences move between launches.
2. **The noise floor, from runs already on disk.** `ab10_orig` against
   `det10_orig` - original against original - differs on **17 of 4,440
   frames**, in the same two windows; ours against ours on 24. Frames 0-3599
   agree in every pairing.
3. **Why now.** None of the three functions was called even once in `all_a`
   or in any pair up to `ab9`. They hang off the draw's *second* primitive
   dispatcher (jump table `0x59F3D8`; the first is `0x59F2A0`), which no slow
   run entered - not even `all_b`, a full-list run of 12,813 frames in which
   the draw ran 1,247 times. `ab10` was the first fast one: taking over the
   library layer removed several million trapped calls a run, and the traced
   game went from about 17 logic frames a second to about 27 (4,738 frames in
   270 s, then 7,428). What makes the draw take its second dispatcher is
   unread; that it depends on how fast the game is running is measured.

So "reachable by observed edges" was the wrong rule for the draw: a wall-clock
function's branches need not all run in the launch that was traced, and a
frame hash that gets *faster* as functions are taken over will keep finding
new ones. **`calltrace.py wallclock --static LO-HI[,LO-HI]`** now also follows
the disassembly's call targets from everything reached, into the given
address ranges only. With the renderer's, `59E000-5A6000,5A9600-5AB000`: 155
functions dropped where there were 91, the 64 new ones all primitive handlers
and unpackers of the draw, the three above among them; 2,782 entries armed.
Unrestricted, the same walk reaches the C runtime through `sprintf` and takes
555 functions, most of them logic's too - hence the ranges.

Edges from two runs are merged for it (`all_a` + `all_b`, concatenated): a
wall-clock root that is *ours* is unarmed and so absent from a recent run's
counts, and present in `all_a`, recorded when almost nothing was owned.

**Validated the same day** with 73 functions ours, three fresh launches under the new list (`ab11_orig`, `ab11_origb`, `ab11_ours`): original against original **identical on all 7,428 frames**, and original against ours identical on all 7,428 - the frame hash's longest run yet, the whole attract cycle and into its repeat.

The general lesson, for the receipt policy too ([`STATUS.md`](STATUS.md) open
decisions): **an A/B check needs an A/A check beside it.** The pair of
originals is the noise floor; without it a difference cannot be read at all,
and this one would have cost the takeovers a day of suspicion.

## 7. Original vs ours under the frame hash — passed 2026-09-19

[`IDEAS.md`](IDEAS.md) I10 item 1. Two rules make the two configurations
comparable, both in `src/hook/calltrace.cpp`:

- **A function registered with `Inject` is never armed**, whichever way the
  A/B switch points. Our `LoadDatFile` calls our `File_Open` directly, never
  through `0x5A7380`, so calls *between* owned functions are invisible in one
  configuration and must be in the other. (The entry list now carries each
  function's size so the tracer knows the owned ranges.)
- **A call made from inside an owned function is recorded with caller
  `0xFFFFFFFF`** — whether the return address is inside Capcom's body of it or
  inside `bof3x.dll`.

What is left is the sequence of calls the owned code makes *into code that is
still Capcom's*, in order, interleaved with every other logic call of the
frame. A faithful takeover must reproduce it exactly.

`ab_orig` (`BOF3X_ORIGINAL=*`) against `ab_ours` (all ten functions ours),
2,839 entries armed, 10 unarmed as owned: **calls and hash identical on all
4,489 frames.** The owned code's own edges, to frame 4,352, are the same eight
in both runs with the same counts: `Snd_LoadBank` 6, `Font_SetGlyphData` 1,
`Crt_free` 6, `Crt_fclose` 9, `Crt_fopen` 11, `Crt_fread` 9,
`Crt_filelength` 9, `Crt_fileno` 9.

**Repeated 2026-09-19 with the structural entry list of §6 and eleven
functions ours** (`ab2_orig` / `ab2_ours`, 2,834 armed): identical on all
4,484 frames. This is also the first verification of that list, and of
`Gfx_BeginFrame` with DIV-0004 — whose early drain fired during the ours run,
since tracing makes the game skip rendering, without changing one logic call.

What this does **not** cover, stated so the pass is not over-read:

- `Gfx_LoadImage` is on the §6 exclusion list, so `LoadDatFile`'s image
  uploads are not in the hash; `tools/mem_dump.py` checks their result in
  bytes instead. Any excluded entry (the 76 + 11) is a blind spot of this
  check. `Crt_malloc` `0x5B9660` is one of the 76 (it is in
  `volatile.txt`), which is why `LoadDatFile`'s allocations are not among the
  owned edges.
- Arguments are not compared, only that the call happened, in order.
- The attract sequence never reaches `Save_WriteFile`, `File_OpenWrite`,
  `File_Write`, `File_Seek` or `File_CdRoot` (§3).

**Negative control, 2026-09-19 — fails as it should.** A throwaway build whose
`File_Size` called `Crt_fileno` twice instead of once: same return value, no
side effect, so nothing the game does changes — exactly the class of mistake
the three-word oracle cannot see. `ab_orig` against that run (`ab_broken`):
**8 of 4,489 frames differ**, exit 1. Six of them are one or two calls over on
the frames where files are sized — 1, 1,310 (+2), 1,312, 3,439, 3,837, 3,855.
The other two are frames 2 and 3, at 360 vs 354 and 50 vs 58 calls: the
remaining +2 of the nine `File_Size` calls, plus a third jitter — chased the
same day with `BOF3X_CALLTRACE_DETAIL=1-4` on one good and two sabotaged
launches. The good run and one sabotaged run differ by exactly the extra
`Crt_fileno` calls (1 in frame 1, 2 in frame 2) and nothing else. The other
sabotaged run additionally has **eight draw-path calls in frame 3 instead of
frame 2**: `0x5A0080` from `0x5A001C` and `0x5A9BFA` from `0x5A0405`, four
each. Not caused by the sabotage — the two sabotaged runs differ from each
other — and rare: frames 2-3 agree in all sixteen earlier runs. Both
functions are in §5's draw path and escaped §6's list because their totals
match; they are now on the `--also` list (13 entries, 2,837 armed after the
owned ten). The source was restored and the DLL rebuilt straight after the run; the
sabotaged line was never committed.

(`attract_diff.py` on the same two runs also reports differences, but under
mode `all` that is the sampler failing, §5 — not evidence of anything.)

## 8. Using it

```
python tools/calltrace.py entries
BOF3X_CALLTRACE=<absolute path to analysis/calltrace/entries.txt> \
    python tools/attract_run.py --out analysis/attract/x.tsv --original "*"
python tools/calltrace.py report build/bof3x.calltrace.tsv
python tools/calltrace.py compare A.tsv B.tsv
```

The path must be absolute (the game's working directory is its own) and, from
Git Bash, Windows-style (`cygpath -m`). Copy `build/bof3x.calltrace.tsv`
somewhere before the next launch; it is recreated each time. Add
`BOF3X_CALLTRACE_MODE=all` for §5's counts, and expect half speed.

The cross-launch hash check of §6, about ten minutes:

```
BOF3X_CALLTRACE=<abs path>/entries_logic.txt BOF3X_CALLTRACE_MODE=all \
    python tools/attract_run.py --out analysis/attract/h1.tsv --original "*" --minutes 4.5
(copy build/bof3x.callframes.tsv aside; run again)
python tools/calltrace.py frames A/bof3x.callframes.tsv B/bof3x.callframes.tsv
```

For §7's A/B check, make one of the two runs without `--original`.

`entries_logic.txt` is regenerated by the `volatile` command line recorded in
§6; it is local and gitignored, like everything under `analysis/`.

## 9. The takeover work queue

`calltrace.py queue COUNTS` -> `analysis/calltrace/queue.csv`
([`IDEAS.md`](IDEAS.md) I10 item 2): every reached function not yet in
`symbols.toml`, in layers — layer 0 calls nothing unnamed (by the static
callees of `pc_funcs.json`), layer n only layers below it — hottest first
within a layer, with size, indirect-call and x87 counts, how many of its static
callees the run never reached, and whether it is logic or wall-clock-timed.

From `all_a`: **495 functions; layers 0-7 hold 205 / 64 / 33 / 17 / 7 / 1 / 1 /
1, and 166 sit in or above a call cycle** (layer 99). 409 are logic, and those
are the ones §7's check can regression-test today. Of the layer-0 logic
functions, 80 lie below `0x5A6000` — game code, not the support library
that the hottest entries (`0x5A7BF0`, `0x5A8380`, ...) live in. What any of
them *is* is unread; the queue orders work, it names nothing.

*Since then (noted 2026-09-24):* seven takeover rounds have worked through
this queue and its successors. The attract queue closed at 0 in scope on
2026-09-23 with round five ([`takeover-queue-round5.md`](takeover-queue-round5.md);
[`STATUS.md`](STATUS.md) order of work, stage 1), the input-reached queues
carried on from recorded routes, and **1,025 functions are ours** (`inject: 1025 ours`, 2026-09-24); each round's doc names what its functions are.

Caveats: static callees miss indirect calls, so a layer-0 function with
`indirect_calls > 0` may not be a leaf; and `unreached_callees > 0` means part
of the function's behaviour is outside what the attract run can verify.

## 10. Open

- **Who fills the upload queue** at `0x9035A0`, the other two record kinds
  (`0x462070` and the fall-through), and what `Gfx_FlushDirtyStrip`'s 256 x 32
  strip at y = 480 holds (§6).
- **Make `mem_dump.py` wait for empty queues** before dumping the VRAM shadow.
- A clean same-configuration pair with the structural list (§6); the A/B pair
  of §7 has passed with it.
- More than one pair of launches behind §6's "identical" claim.
- **The `--also` list lives in a command line.** It should be a committed file
  of addresses with reasons once it has settled.
- **Cheaper counting.** Two exceptions per call halves the frame rate; skipping
  the few hottest leaf entries would recover most of it.
- **Arm the pointer-reached entries** (§2) once a list of them exists — the
  mode tables and task entries are already in `symbols.toml` evidence strings.
  *Since: the list exists, [`attract-remaining.md`](attract-remaining.md) §3.*
- **Work queue.** The 540 are the functions a takeover can be regression-tested
  against today; ordering them by first-call frame and caller is the I4
  harvester's output. *Since: built (§9) and worked to 0 in scope for the
  attract sequence; see the note under §9.*

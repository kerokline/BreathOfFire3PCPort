# The state hash

**Status:** WORKING (2026-10-04; the attract sequence and the combat route measured, section 4)

A regression check that compares two runs by **what the game's memory held,
frame for frame**, whoever's code wrote it. Built before round fourteen
because the call trace's frame hash ([`call-trace.md`](call-trace.md)) arms
only entries that are not ours: 635 of 9,265 after round thirteen
([`takeover-queue-round13.md`](takeover-queue-round13.md) section 18 item 6),
and none of the game's own once round fourteen has taken the remainder. The
call hash also only works while our functions keep Capcom's boundaries; this
one does not care where a function begins.

Instrumentation, not a replacement: it reads memory and writes a file. No
`DIVERGENCE.md` entry.

## 1. What it does

`src/hook/statehash.cpp`. With `BOF3X_STATEHASH=<file>` set, at the first
input latch of every logic frame (the first latch after `Frame_Counter`
`0x937F94` moved) each 4 KiB page of `BOF3.exe`'s `.data` -
`0x5DA000..0x93E000`, 868 pages, the section header's extent rounded to the
page - is hashed, and the pages whose hash changed since the frame before are
written. The latch is the same point of the loop on both sides: Capcom's
WinMain calls `Input_Latch` at `0x4FCDDE` (the site the input recipe already
retargets), ours calls `InputScript_Latch`. The tick runs before the latch,
so a recipe's pad words are the previous frame's on both sides.

| Variable | |
|---|---|
| `BOF3X_STATEHASH` | the output file; the switch |
| `BOF3X_STATEHASH_SKIP` | a list of `ADDRESS LENGTH # why` ranges hashed as zero - [`tools/statehash_skip.txt`](../tools/statehash_skip.txt) |
| `BOF3X_STATEHASH_DUMP` | ticks, comma-separated, at which all of `.data` is also written raw to `<file>.<tick>.bin` (3.5 MB each; game-derived, `analysis/` only) |

A **tick** is one logic frame seen by the latch, counted from the first; the
record also carries `Frame_Counter` and the recipe frame. The file is flushed
every 64 ticks, since the runners end the game with `taskkill`. A six-minute
attract run writes about 1 MB.

What it costs: 3.5 MB hashed a frame. Its time was not measured; the hashed
attract runs kept the game's pace (10,305 to 10,317 logic frames in 360 s).

## 2. The tool

`python tools/statehash.py`:

- `info RUN` - records, tick and `Frame_Counter` range, pages that changed.
- `diff A B` - per page, the first and last tick it differed and how many,
  with the symbols the page holds (`symbols.toml`).
- `check REF REFB NEW` - the regression question. A page is noise at a tick
  when the two references disagree there; what is reported, and what sets the
  exit code, is NEW against REF where the references agree. `--noise` lists
  the noise too.
- `bytes A.bin B.bin` - two raw dumps byte for byte, each range with its
  symbol; `--skip-out` writes the ranges in skip-list form **to be read, not
  fed back**.

`--align recipe` aligns scripted runs by recipe frame instead of tick;
`--from` / `--to` bound the comparison.

**The loop when a check reports a page:** `diff` or `check` names the page and
its first tick; run both sides again with `BOF3X_STATEHASH_DUMP=<that tick>`
(runs are deterministic, the tick recurs); `bytes` names the bytes and the
symbol. Then read the code that writes them.

## 3. The skip list

Without one, two runs of Capcom's code differ on 40 pages (7 at every tick)
and ours differs from Capcom's on 64 more. Measured down to bytes with raw
dumps, that is small and nameable, and
[`tools/statehash_skip.txt`](../tools/statehash_skip.txt) holds it as two
kinds:

- **noise** - differs between two runs of the same code: the window handle
  and the frame deadline, DirectSound / DirectDraw / DirectInput object
  pointers, stack addresses, the music stream's staging buffer (its thread's
  timing), the texture caches (surface pointers, and which record a cell took
  depends on what was uploaded when).
- **ours** - differs between Capcom's code and ours by construction: the task
  stacks and saved stack pointers (return addresses into different code), the
  Direct3D state the D3D11 renderer replaces, the key table the launcher
  writes, the draw-item pool (`DrawPool_Grow`, DIV-0062: ours draws from a
  larger pool of its own, so Capcom's arrays lie unused) and everything that
  names a draw item by address - the layer and frame nodes, one ordering-table
  pointer, and **the packet pools**.
  Two ledgered divergences are in this kind, by number: DIV-0028 (a music
  fade steps once a logic frame, so its count and step differ while one runs)
  and DIV-0023 (ours writes 0 to the top halves of `Gte_Vertices[1]`, `[3]`,
  `[5]`, where Capcom's loaders leave stale stack).

A range is added only with the dump that showed it and the reason it is not a
defect: a platform object, or a `DIVERGENCE.md` entry by number.

**What the list costs.** About 426 KiB of 3.5 MiB (3,472 KiB) is not seen. Most of that is
draw output: the packet pools (128 KiB) and the draw items (144 KiB) are what
the frame draws, and the picture A/B (`input_run.py`'s shots, the attract
captures) is what checks them. The sound banks and the stream are not seen at
all. Everything the game's logic keeps - the arena at `0x803580`, the sprite
and effect objects, the party and battle records, the scenario flags, the
VRAM shadow, the task records but their stack pointers - is hashed.

**What the hash does not see at all:** anything outside `.data` - the stack
in use, the heap (`LoadDatFile`'s buffers live in the arena, which is
`.data`), our own DLL's statics, and the state of the devices. And a
difference inside a frame that is gone by the frame's end.

## 4. Measured

2026-10-04, the build `dafd4a3` (8,648 ours), a launcher copy with `wide=0`,
`filter=linear`, `scale=2`, `BOF3X_LAYERING=0`; the reference sides
`--original "*,-Game_Clock"`, foreground held; ours `--no-front`.

| Route | Ticks | Original against original | Ours against the pair |
|---|--:|---|---|
| The attract sequence, 6 minutes (`attract_r14_*`) | 10,305 | identical on every page and tick | **identical on every page and tick** |
| `combat.txt`, the machine quiet (the run before the last range was added) | 2,561 | identical | **identical** |
| `combat.txt`, the machine loaded (`combat_*`: a merge's wide self-test running beside it) | 2,561 | 12 pages at isolated ticks | 1 page, 2 ticks (`0x7DE000`, ticks 75 and 201) |
| The control: `combat.txt` with one `poke` of a byte of the first character record at recipe frame 59 (`combatpoke_ours`) | 2,561 | - | **reported**: page `0x903000` from tick 60 to 301, 242 ticks, until the game wrote the byte again |

The files are in `analysis/statehash/` (game-derived, not committed), the
skip list at 165 ranges. The references are `attract_r14_orig.sh` /
`_origb.sh` and `combat_orig.sh` / `_origb.sh`; the batch scripts
(`sh_attract.sh`, `sh_route.sh`, `ref_final.sh`) are in the session-`7bf3959f`
scratchpad.

**The loaded run is the method's limit, stated.** With the machine busy the
two originals themselves differed: the VRAM shadow on ticks 2 to 8 and once at
411 (an upload landing a frame later - the upload queue drains by the clock),
and the sound page `0x7DE000` at 28 ticks. `check` takes those out tick by
tick, and what was left for ours was the same sound page at two other ticks -
the same kind of noise falling where the pair happened to agree, not yet
measured down to its bytes (section 5). So: **record references and run
checks with the machine quiet**, as the frame hash's always were, and read a
reported page that is also in the `--noise` list as noise until a dump says
otherwise.

## 5. What the first runs found, and what is open

Nothing new, which is the result wanted of a reference at a verified tip. Once
the platform and the pool were masked, the bytes that still differed between
Capcom's code and ours were all in the ledger already, and the hash found each
from its effect on memory alone, with its first tick:

- `Music_FadeCount` `0x6BDE54` and `Music_FadeStep` `0x6BDE60` (DIV-0028),
  from tick 4;
- the top halves of `Gte_Vertices[1]`, `[3]`, `[5]` at `0x7DE46E`,
  `0x7DE476`, `0x7DE47E` (DIV-0023), from tick 981;
- `Pause_LinesGame` / `Pause_LinesTitle`'s pointers `0x66A418`, `0x66A448`
  (DIV-0038, under a language overlay), from tick 1 of the combat route.

`Music_Finished` `0x7DE3E0` looked like a fourth - set at tick 981 in ours and
993 in the original, twice - until a third pair of originals disagreed on it
by a tick: it is set from the stream's own timing, and is in the list as
noise.

**Open:**

1. **The sound page's noise under load** (`0x7DE000`, section 4's third row):
   which bytes. A dump at a differing tick names them; they recur only with
   the machine busy, so the dump wants the same load or luck.
2. **The cost per frame** is not measured (3.5 MB hashed).
3. **Routes other than these two** have no reference yet: the whelp, the
   Nue cutscene, the dragon, the shop and the world map are round thirteen's
   set and each wants a pair (`sh_route.sh`, about four minutes a side).
4. **The wide picture** (`BOF3X_WIDE=1`) patches operands and draws more:
   not run. Its references are a separate pair.

## 6. Use in a round

- **The reference** is a pair of original runs and is recorded per route once
  per skip list: `analysis/statehash/<route>_orig.sh`, `_origb.sh`. A change of
  the skip list, the recipe, the save or the launcher's settings wants a new
  pair. Without a language overlay the original side needs nothing of ours, so the
  reference outlives the rounds - unlike the call hash's, whose armed set
  changed every round.
- **Under a language overlay the pair is recorded with the build under
  test.** The English, French, German and Japanese text is in our DLL and the
  game's tables point at it (`Menu_Verbs`, the label and placement tables, six
  pages in all on the combat route); the DLL's address moves with the build,
  so a pair from another build differs on those pages from tick 1 (round
  fourteen's wave one met it). The attract sequence runs without an overlay
  and its pair does outlive builds: `attract_r14_*`, recorded at `dafd4a3`,
  was identical to ours at `4962b89`.
- **After a merge wave:** one run of ours per route, `check` against the pair.
  Exit 0 is the bar; a reported page goes through section 2's loop.
- **Known and not skipped:** DIV-0073 - after the masters' model has drawn,
  `Gte_Matrix2`'s bytes `0x7DE4E6..0x7DE4FF` are zeros in ours and stale stack
  in the original. No route recorded so far opens that screen.
- **A new `DIVERGENCE.md` entry that changes state** shows as a page from its
  first tick. Compare with the divergence switched off (`BOF3X_ORIGINAL=<its
  name>`), as the route A/Bs already do with `DIVS`.
- It runs beside the oracle (`attract_diff.py`), the `randlog` and the picture
  A/Bs, and does not replace them: the pictures are what see the draw output
  the skip list hides.

# The state hash

**Status:** WORKING (2026-10-05; every reference pair recorded under the 168-range skip list at `main` `2df90d9`, section 6; the shop route's difference was the runner's, section 5; `BOF3SH3`, our pointers in Capcom's slots hashed by what they name, 2026-10-10, section 7 - measured offline, the cross-build pair under an overlay owed live)

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
| `BOF3X_OPT` | **leave unset** on a reference run: an `opt/` layer (DIV-0086) changes the arena and VRAM |
| `BOF3X_STATEHASH_SKIP` | a list of `ADDRESS LENGTH # why` ranges hashed as zero - [`tools/statehash_skip.txt`](../tools/statehash_skip.txt) |
| `BOF3X_STATEHASH_DUMP` | ticks, comma-separated, at which all of `.data` is also written raw to `<file>.<tick>.bin` (3.5 MB each; game-derived, `analysis/` only) |
| `BOF3X_STATEHASH_RAW` | `1`: hash our pointers in Capcom's slots raw, as before 2026-10-10 (section 7); unset or `0`, the default, hashes them by what they name |

A **tick** is one logic frame seen by the latch, counted from the first; the
record also carries `Frame_Counter` and the recipe frame. The file is flushed
every 64 ticks, since the runners end the game with `taskkill`, and on the
way out of `Fatal` and of the crash reporter (2026-10-05, the round-fourteen
review's item 15), so the frames before an abort or a fault - the ones a
crash wants - are in the file; not every tick, which would cost the live
check. A six-minute
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

**What the list costs.** About 426 KiB of 3.39 MiB (3,472 KiB) is not seen. Most of that is
draw output: the packet pools (128 KiB) and the draw items (144 KiB) are what
the frame draws, and the picture A/B (`input_run.py`'s shots, the attract
captures) is what checks them. The sound banks and the stream are not seen at
all. Everything the game's logic keeps - the arena at `0x803580`, the sprite
and effect objects, the party and battle records, the scenario flags, the
VRAM shadow, the task records but their stack pointers - is hashed.

**What the hash does not see at all:** anything outside `.data` - the stack
in use, the heap (`LoadDatFile`'s buffers live in the arena, which is
`.data`), our own DLL's statics (but the text one of Capcom's pointer slots
names there, from `BOF3SH3` on: section 7), and the state of the devices. **`Rand`'s
seed is among them**: `0x5B93D2` is the CRT's `rand()` and its seed is in
the per-thread data (ptd + 0x14), on the CRT's heap, so two runs whose
sequences have parted hash alike until a draw changes something in `.data`.
The instrument for that is the `randlog` line a frame (`input_script.cpp`),
which counts on reference sides too since 2026-10-05 (it read 0 under
`--original "*"` before: round thirteen's review item 2, round fourteen's
item 2). And a difference inside a frame that is gone by the frame's end.

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

**The shop route's difference was the test rig's, not the game's**
(2026-10-05). Twelve bytes at `0x905BC6` from tick 2975 and one more
primitive a frame, since before round fourteen's wave two: slot 0's save
summary, re-read when the inn's save prompt opens (`Save_ReadSummaries`
opens `BISLPS00..09`). `input_run.py` hands the owner's slot 0 back as soon
as ours logs `save loaded slot 0`; Capcom's side never logs it and keeps the
recipe save to the end, so the two sides read different files there - the
bytes ours showed are the owner's `BISLPS00.DAT`'s, byte for byte
([`input-script.md`](input-script.md) section 1a). With `--slot0-hold` on
the ours side the route is identical but three platform pages at one tick
(2981: the Direct3D caches and the sound page, the start-up class of
section 4's third row). The Chinese pairs are recorded with the hold on
neither side, which is right for them (they never see the line); every
ours run of a route that lists the saves wants it.

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

- **The references at `main` `2df90d9` (2026-10-05, 168 ranges, the machine
  quiet):** `attract_r14_orig` / `_origb` (10,305 ticks) and
  `cn_<route>_orig` / `_origb` for all ten routes. Ours at the same tip is
  identical to every pair on every compared tick (`attract_m14_ours`,
  `cn_<route>_ours`), the shop with `--slot0-hold` as above, the world map
  on its second run (the first reported the sound page `0x7DE000` at one
  tick, 1651, and the repeat did not). The pairs hashed under 165 are kept
  in `analysis/statehash/pre168/`.
- **The route pairs re-recorded 2026-10-10 afternoon** as `cn_<route>_orig2` /
  `_orig2b` (`BOF3X_OPT=none`, `BOF3X_LAYERING=0`, the ini-less launcher,
  Capcom's side `--original "*,-Game_Clock"` as section 4 says - a first cut
  with `'*'` alone differed from ours on `Font_TexCache`'s glyph ids and
  `Music_Events`' counters from the field menu on: Capcom's clock is the wall
  clock, the play-time digits drift with the run's pace), because the 2026-10-05 pairs went stale
  without any change of ours: every route differed from them on two pages from
  its first save-list read (`0x92A000`, `Save_Staging` and `MapView_CornerPtr`;
  `0x905000`), and so did a fresh run of Capcom's own code, while ours and
  Capcom's the same afternoon were identical on all 2,561 ticks of `combat`
  with no byte of those pages differing in the raw dumps at ticks 179 and
  226. The owner's save slots had changed since 2026-10-05 (`BISLPS06`, `09`
  and the new `0A..0C`), and the pairs stage the save list. **So the pairs
  depend on the owner's saves as well as the skip list, the recipe and the
  launcher's settings**, and the first move on a differing page is a run of
  Capcom's side against the pair. Against the new pairs, ours at `main`'s tip of 2026-10-10
  (`cn_<route>_cu1010n`, `BOF3X_OPT=none`) is identical on every tick of nine
  routes and on all but one tick of whelpBoss (the sound page, tick 3,227;
  a second run identical throughout).
- **The reference** is a pair of original runs and is recorded per route once
  per skip list: `analysis/statehash/<route>_orig.sh`, `_origb.sh`. A change of
  the skip list, the recipe, the save or the launcher's settings wants a new
  pair. **The tool enforces the skip list's part** (2026-10-05, round
  fourteen's review item 3): `diff` and `check` refuse runs whose headers
  differ - the base, the page count and size, the number of skip ranges and,
  from the `BOF3SH2` header on, a hash of the ranges themselves (a `BOF3SH1`
  file, every reference recorded before that day, has none and is compared by
  the count alone, with a note). `check` also names the pages its two
  references never agree on, where the run under test is never compared. Without a language overlay the original side needs nothing of ours, so the
  reference outlives the rounds - unlike the call hash's, whose armed set
  changed every round.
- **Under a language overlay the pair is recorded with the build under
  test.** The English, French, German and Japanese text is in our DLL and the
  game's tables point at it (`Menu_Verbs`, the label and placement tables, six
  pages in all on the combat route); the DLL's address moves with the build,
  so a pair from another build differs on those pages from tick 1 (round
  fourteen's wave one met it). The attract sequence runs without an overlay
  and its pair does outlive builds: `attract_r14_*`, recorded at `dafd4a3`,
  was identical to ours at `4962b89`. **From `BOF3SH3` on (2026-10-10) the
  hash names those pointers by their text** (section 7), so an overlay pair
  is meant to outlive builds too; that is measured offline and owed live.
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

## 7. Our pointers in Capcom's slots: `BOF3SH3` (2026-10-10)

**The problem.** A Latin language overlay re-aims Capcom's pointer tables at
text in our DLL - the label groups of DIV-0064 (`0x663970`, `0x66B12C`, ...),
the fishing lines and tabs (`0x653B98`, `0x66A088`), the battle banners
(`0x669DE0`), the pause lines (DIV-0038, `0x66A418` / `0x66A448`) - so five
pages hold our addresses (`0x653000`, `0x663000`, `0x669000`, `0x66A000`,
`0x66B000`; section 6's "six pages on the combat route" counted a sixth,
`0x93C000`, which differs from tick 514 for another reason: the enemy records,
not a pointer). Our addresses move with every build, and **not only by the
base**: `bof3x.dll` is ASLR'd (`DllCharacteristics` `0x0140`, dynamic base),
and between the builds of `combat_n_*` and `shop_d_*` its lowest function
moved by `0x110000` while the 72 pointers moved by `0xD6818` - the layout
moved too. So the first design considered, a pointer hashed as its offset
from our base, would not have matched across builds either; the build of this change
loaded at `0x50900000` once and at `0x0FBF0000` after the next link.

**Why not every word that falls in our image.** The game's data is full of
such values. Our image is 94 MB (`SizeOfImage` `0x5E04000`; 41 MB of it the
discardable DWARF sections and `.reloc`), and the census (below) counts
**2,313** aligned words of `.data` inside it before any of ours has written
there, at a load address of `0x50900000` - **3,238** at `0x0FBF0000`, 2,396 of
them in the sections a pointer can name - the static tables' bytes. In game
it is worse: the raw dumps of `combat_n_ours` hold 3,635 such words at tick
100 and 5,060 at tick 1000, `shop_d_ours` 6,187 at tick 1000 (VRAM shadow,
arena, the game's tables; 16-bit colour pairs like `0x5DCD32A2` are common).
Renaming each would make the hash depend on where ASLR put the DLL - the very
difference this is for - on dozens of pages a frame.

**The design.** A word is normalised only where Capcom kept a pointer:
1. **The slots.** In `DllMain`, before `InjectAll`, every aligned word of
   `.data` whose value lies in `BOF3.exe`'s image (`0x400000..0x93F000`) is
   recorded: **26,189** of them, the game's own pointer slots as the loader
   mapped them, the same on both sides of a pair (our DLL is loaded under
   `--original "*"` too).
2. **The test.** At each tick, after the skip list, a slot whose value lies
   in one of our **non-discardable** sections (`.text` .. `.tls`, read from
   our PE headers at run time) is ours. Discardable sections are left out
   because nothing points into them, and because overlay text written in
   place can fall there: the one slot of that kind in the dumps is
   `0x65D1D0`, whose Chinese bytes `84 67 64 00` read as `0x00646784` and
   whose overlay text "Para" reads as `0x61726150` - past `.tls` in both
   measured builds (their bases estimated from their logs, about
   `0x5DBC0000` and `0x5DAB0000`, with this build's section map), so hashed
   raw. The class is not closed: such a text word whose value falls inside
   our kept sections, wherever ASLR puts them, is normalised wrongly and
   shows as a difference between builds on its page - seen, not hidden.
3. **The value.** A pointer into a readable, non-executable section is hashed
   as `0x80000000 | FNV-1a` of the bytes it names up to the NUL (at most 256,
   within the section): the text. A pointer into code is hashed as the tag
   `0x80000000` alone (a function has no build-independent name without
   symbols; the same-build comparison against Capcom's side still sees the
   entry, his address against ours).
4. **The marker.** Both images lie below `0x80000000` (the start-up log says
   so for each run: `identities 0x80000000.. lie above both`), so an identity
   is never a pointer into either. Game data can hold any value, so the
   marker alone cannot be disjoint from it: the page hash also mixes in each
   normalised word's index after hashing, so a substituted word never hashes
   like the same value stored raw. **A page with no normalised word hashes
   bit for bit as it did under `BOF3SH2`.**
5. **Unaligned words are not normalised.** Capcom's compiler lays pointer
   tables and pointer members at 4-byte alignment, which is what the slots
   are; an unaligned scan would multiply the slots with byte data and buy
   nothing. A pointer of ours that is unaligned, or in a word that held no
   pointer of Capcom's (a table that was zeros), is hashed raw - it shows as a
   difference between builds, it is not hidden.

The raw dumps (`BOF3X_STATEHASH_DUMP`) keep the raw bytes, so section 2's
loop still sees what differed.

**Measured.** The census logs at start-up (`statehash   census ...`): before
the injects, **0** of the words in our sections sit in a slot (as it must: a
slot holds Capcom's address then); after the injects, without an overlay,
**0** in a plain headless self-test (`BOF3X_SELFTEST_ONLY=1
BOF3X_STATEHASH=1`, the ini-less launcher: 3,172 words in our image before
and after, 2,347 in our sections, the injects wrote none of them), and
**12** with `BOF3X_SHADOW='*'` - `Scena13_Objects` `0x661810` (4) and
`Scena14_Objects` `0x661884` (8), the code entries chapter 13 and 14's fuzz
seeds with recorder pointers of ours (`scena_sc13_fuzz.cpp`, `Seed`), hashed
as the code tag; a self-test ends the process there, and no hashed run sets
`BOF3X_SHADOW`. Both runs: exit 0, `inject: 10086 ours`, the shadow run's
copies 0 mismatches. **The load address moves even within one build**: the
same `bof3x.dll` loaded at `0x0FBF0000` from one launcher copy and at
`0x0FF90000` from another (a copy in another directory is another image to
the loader's ASLR), so before
this change a pair under an overlay wanted the same build *and* the same
launcher directory. Offline, against the raw dumps of two
builds under `en`: 73 slots hold a pointer of ours in `combat_n_ours` and
`shop_d_ours` at ticks 100, 1000 and 3050 (69 on Capcom's side: the four
pause-line pointers are DIV-0038's, ours only) - `0x653000` 13, `0x663000`
14, `0x669000` 12, `0x66A000` 18, `0x66B000` 15 - and 72 of them moved
between the two builds, the 73rd being `0x65D1D0` above. With those words set
aside the five pages are **byte-identical** between the two builds at tick
1000 (`crossbuild.py` in the session-`af75c190` scratchpad). Whether the
texts named are the same, which is what makes the identities equal, a dump
cannot say: owed live, below. The run log says how many words were
normalised, a line each time the number changes (`statehash   tick N: K of
Capcom's pointer slots hold a pointer of ours`).

**The header.** `BOF3SH3\0` adds a `u32` of flags after the skip list hash,
bit 0 set when the normalisation is on. `tools/statehash.py` reads all three
versions; `info` says which way a run hashed our pointers. A normalised run
beside an unnormalised one (a `BOF3SH2` file, or `BOF3X_STATEHASH_RAW=1`) is
**compared, with a printed note**, not refused: without an overlay the two
hash identically (the census after the injects), so every reference of
before today - `attract_r14_*`, the `cn_<route>_orig2*` pairs - still serves.
Shown offline: `attract_cu1010n_ours.sh` rewritten as the `BOF3SH3` file it
would be (flags 1, records unchanged) checks against `attract_r14_orig` /
`_origb` identical on all 10,305 ticks, with the note. Under an overlay the
note says to record the pair again: the overlay pairs of before today
(`combat_n_*`, `en4_*`, `shop_d_*`) hash those five pages raw and differ from
a normalised run there from tick 1.

**The switch.** On by default. `BOF3X_STATEHASH_RAW=1` turns it off and
writes flags 0, so a run can still be matched against a raw reference under
an overlay recorded with the same build; any value but `0` or `1` aborts.

**Owed (tomorrow's quiet machine):**
1. Two runs under `BOF3X_LANG=en-US` from **two different builds** - for
   example a pair `--original "*,-Game_Clock"` with this branch's build and
   ours with `main`'s once this is merged, or any later build - on the
   combat route: identical on `0x653000`, `0x663000`, `0x669000`,
   `0x66A000`, `0x66B000` from tick 1, where `BOF3SH2` differed from tick 1.
   The log of each should show about 73 normalised slots (69 on Capcom's
   side).
2. Then record the overlay pairs once more under `BOF3SH3`; from then on
   they should outlive builds as the Chinese pairs do.
3. If a page of the five still differs: a dump at its first tick on both
   sides, `bytes`, and the word is either a pointer of ours outside a slot
   (point 5 above: hashed raw by design - extend the slots or accept) or a
   text that differs between the builds (a real difference).

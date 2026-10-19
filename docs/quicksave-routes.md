# The quicksave routes and the proof by the state hash

**Status:** MEASURED (2026-10-19). Group RC (branch `quicksave/rc`) built
six routes in three forms, `tools/quicksave_check.py` with its self-test, and
the identity references on the PC install (section 5). Stage C (branch
`quicksave/stagec`, off `quicksave/round` `91ca4590`) ran the proof batch
(section 7). In process, every route equals itself after the restore, but
for the two expected words, on the PC install under both conditions and in
the wide picture. On the US disc and PSP-EU installs the only other
differences are pages that differ between two runs anyway. Across
processes, every route plays on to its end, with the differences named in
7.4.

The round's acceptance test ([`quicksave-round.md`](quicksave-round.md) 0):
run to a tick, quicksave, run 300 frames, quick-load, run 300 frames; the two
hash sequences are equal tick for tick. Then the same across two processes.
A difference names what the snapshot missed, by its page.

## 1. Three forms per route

A recorded route is one pad word a frame ([`input-script.md`](input-script.md)
5a). The keys are `key f6` / `key f7` steps: `WM_KEYDOWN` posted to the game
window, one frame each, with **nothing held** on the pad that frame
(`src/hook/input_script.cpp`, `Kind::Key`). So a key goes **in place of an
idle frame** of the route, and no other frame moves. `tools/quicksave_check.py
make ROUTE --at F --name N` writes three recipes from the route and the frame
F (refusing an F that holds a button), and reads each back to check it holds
exactly the frames meant:

| File | What it is | Length |
|---|---|---|
| `quick_N.txt` | **the proof**: the route to F, `key f6` in place of frame F, the route's frames F+1..F+299, `key f7`, then the route again from F+1 to its end | the route + 300 |
| `quick_N_keys.txt` | **the keys alone**: `key f6` in place of frame F, `key f7` in place of frame F+300 (or the frame `--keys-f7` names, when F+300 holds a button), nothing replayed | the route's |
| `quick_N_cross.txt` | **the cross-process form**: `key f7` at recipe frame 0, then the route from F+1; no save header (nothing goes through slot 0) | the route - F |

Why the proof replays: after a restore the game is back at F, but the recipe
runs on. Handed the route's frames from F+1 again, a game whose restore was
exact sees the same inputs after the restore as after the take, so the 300
ticks after F7 must equal the 300 after F6; and after the window it plays the
rest of the route, to its end, as the route does (the `--plain` check, section
3). The cost is that **while F6 and F7 do nothing, `quick_N.txt` does not
play its route**: from frame F+301 it drives the game from the wrong place.
That is why the identity reference uses `_keys` (section 5), and why a replay
form should run from a harness game folder, not the owner's (a replay that
has lost its place can wander into a save menu).

Per release: the forms hold no buttons of their own (a key step is a window
message, not a pad word), so each plays wherever its route plays: the route's
buttons are the PC's and are pressed as the install's release's
(`sigma::RecipeToPad`, [`input-script.md`](input-script.md) 2a) until the
route's save is loaded, then as recorded. One file serves every install, as the
routes themselves do since PR #85. **One limit, for SN / SC:** the
cross-process form loads no save through the load menu, so on a PSX-West or
PSP-EU install sigma's translation state still says "the release's words
stand" after the quick-load, while the restored Config holds the save's -
the route's confirm would be pressed as cross. Until a quick-load tells
sigma what a save load tells it (`LoadMenu_Apply`'s hook), run the cross form
on a Japan-release install (the PC, PSX-JP, PSP-JP) only. The in-process form
is unaffected: its route loads its save before F6.

## 2. The routes and their ticks

| Route (`quick_*.txt`) | From | F6 / F7 | Why there |
|---|---|---|---|
| `worldMapAndAreaTransition` (a field walk) | `worldMapAndAreaTransition.txt`, save `town` | 595 / 895 | the party standing on a path of area 7 (mode 2) three frames after a cross press that ended the run through the area; the window holds the field menu (631..680) and the walk out of the area (mode 6 from about 720): the restore lands on a field frame whose window crossed a menu and an exit |
| `worldMap` (the world map) | the same route | 1270 / 1570 | on the Yraall Region map (area 33, mode 2) under the Cedar Woods plate, between two walking legs; the window holds the Guide message (from 1290), the woods' area (area 8 from about 1470) and the map again |
| `combat` (a battle mid-turn) | `combat.txt`, save `combat` | 900 / 1200 | the round's commands entered by 858, the turn resolving with nothing held: at 900 Rei's Pilfer on screen, at 1200 Flare; take and restore inside one turn |
| `cutsceneAndNue` (an event, mid-message) | `cutsceneAndNue.txt`, save `nue` | 2165 / 2465 | the night in the house (area 23, mode 4), a line typing out letter by letter (its first six characters drawn at 2160) with nothing held; the window holds the rest of that talk and the next lines (at 2465 another is typing): take and restore both inside a message |
| `shop` | `shop.txt`, save `town` | 1110 / 1410 | the item shop's Sell list (mode 7, area 7) just after a sale - the money moved from 150 to 175 between 1080 and 1110, the list one row shorter; the window holds leaving the shop and the weapon shop's greeting (typing at 1410): a sale's money, inventory and the menu's state put back |
| `musicStopCheck` (a music fade) | `musicStopCheck.txt`, save `eldritchGene` | 2808 / 3108 (the `_keys` form's F7 at 3186) | the fight won, the fanfare (track 165) fading to a stop: `Music_FadeOutStop(10)` at 2804, `Music_Stop` at 2814 (the reference run's `BOF3X_MUSIC_LOG`), so the take holds a fade-to-stop four steps in (DIV-0028's count and step; DIV-0143's case); the window holds the stop and the field's return, where DIV-0092 resumes track 91 at its remembered sample (2838). Frame 3108 holds a button, so `_keys` presses F7 at 3186, the first idle frame after it |

The mode and area are the `BOF3X_SHOT_EVERY=30` lines of the reference runs
(`input frame tNNNNNN saved mode M area A`); what is on screen is from their
frames, looked at.

## 3. The check

```
python tools/quicksave_check.py run tools/recipes/quick_combat.txt --out RUNDIR --lock C:/Users/kerok/q_lock \
    --launcher L/bof3x-launcher.exe --game GAMEDIR [--plain PLAIN.sh] [--defaults]
python tools/quicksave_check.py inproc RUNDIR/quick_combat.sh --log RUNDIR/bof3x.log [--plain PLAIN.sh]
python tools/quicksave_check.py cross LIVE.sh LOADED.sh [--log-live L] [--log-loaded L]
python tools/quicksave_check.py same PLAIN.sh KEYS.sh [--refb PLAIN2.sh]
python tools/quicksave_check.py selftest
```

- **`run`** plays one recipe through `tools/input_run.py` with
  `BOF3X_STATEHASH` (the file `RUNDIR/<recipe>.sh`) and
  `BOF3X_STATEHASH_SKIP=tools/statehash_skip.txt`, `--no-front`,
  `--lang en-US`, `--slot0-hold` (a replay reloads its save after the restore,
  and the shop's inn lists the saves: the recipe save must stay in slot 0 to
  the end; the cross form gets `--no-save`), and the hash conditions
  `BOF3X_OPT=none BOF3X_LAYERING=0` unless `--defaults`. `--lock DIR` takes the
  machine's one-live-game lock for the run (mkdir, retried a minute apart;
  rmdir once `input_run` has ended the game). Then it copies `bof3x.log`
  beside the hash and runs the analysis the recipe's header asks for
  (`# quick replay|keys|cross f6 F f7 G ...`): `inproc` for the proof,
  `same` against `--plain` for the keys.
- **`inproc`** finds the restore without being told where it was: the record
  whose `Frame_Counter` goes back (a quick-load restores the counter with the
  span; the hash ticks and the recipe frames run on). The take is the first
  earlier record holding the same counter. It compares take+k with restore+k
  for k over the window (restore minus take, about 300) and prints the first
  k whose hash differs - both ticks and recipe frames - and per page the
  first and last k and the count, each page named by its symbols and **by the
  skip list's rows on it** (the rows SC's census reads as participants). With
  `--plain`, it also compares the whole run with a hashed run of the route:
  tick for tick before the restore, shifted back by the window after it. It
  prints the log's key lines and any line naming a quicksave, quick-load or
  snapshot, whatever SN's wording.
- **`cross`** takes the loaded run's first `Frame_Counter` jump as its
  restore, finds the live record holding that counter (the live run's own
  restore continuation if it restored, else the take), and compares from
  there to the shorter run's end.
- **`same`** compares two runs tick for tick, pages named as above; with
  `--refb` a second run of the first side, and a page the two disagree on at
  a tick is listed as noise there, not as a difference.
- **"F6/F7 did nothing" is a result, not a crash:** no backward jump in a
  proof run, or no jump at all in a loaded run, prints `RESULT: f7 did
  nothing` / `the loaded run never restored` and exits 2. A loaded run whose
  counter no live record held is "not this live run's quicksave", exit 2.
  Exit 0 is equal, 1 a difference.

What the hash sees and does not ([`state-hash.md`](state-hash.md) 3): `.data`
`0x5DA000..0x93E000` less the 173 skip ranges, once a logic frame. The
snapshot's span is wider (`0x5C4000..0x987000`), and what lies outside
`.data` or in a skip range - the stacks, the sound banks, the stream, the
texture caches - is exactly what the participants rebuild; a restore that
gets one of those wrong shows only through what it does to `.data` next.
The last up to 63 ticks of a run are not in its file (the writer flushes
every 64 ticks and `input_run` ends the game by `taskkill`).

**Which frame the take holds.** A key's `WM_KEYDOWN` reaches `Game_WndProc`
in the pass that latches its frame; the take runs after that frame's logic
(SN's `Snapshot_Pending`). Whether that is frame F or F+1, the restore is
late by the same amount, and the replay from F+1 holds either way: the f6
and f7 frames both hold nothing, so `Input_Previous` is the same word after
both. `inproc` matches by the counter, so it needs no assumption.

## 4. The self-test

`python tools/quicksave_check.py selftest`, on synthetic `BOF3SH3` files
written by the tool (8 pages; a run of 81 frames, the take at Frame_Counter
50, a restore at tick 81, 60 ticks after):

| Case | Expected | Got |
|---|---|---|
| the window equal | exit 0 | exit 0, "equal - the 31 ticks after the restore equal the 31 after the take" |
| page 5 differing from k = 17 | exit 1, first k = 17 at tick 98, page `0x5DF000` | exit 1, `first at k=17: tick 98 (recipe frame 98) against tick 67`, page `0x5DF000` first 17 last 30 count 14 |
| no restore | exit 2 | exit 2, "f7 did nothing" |
| `--plain` against the route | exit 0 | exit 0: 81 ticks before, 60 after, shifted by 31 |
| cross, equal | exit 0 | exit 0 |
| cross, the load did nothing | exit 2 | exit 2 |
| cross, one page differing | exit 1 | exit 1 |
| `same`, a run against itself | exit 0 | exit 0 |
| `same --refb`, a difference only where the references disagree | exit 0 | exit 0 |
| `same --refb`, a difference where they agree | exit 1 | exit 1 |
| a page named by its skip rows | `0x6BC000` carries `Sound_Banks`' row | yes |

## 5. The references (2026-10-19)

The build of `quicksave/round` `f7886490` (this branch's base; nothing of
`src/` changed), an ini-less launcher copy (`C:\Users\kerok\q_rc\launcher`),
the exe host, the PC install (the main checkout's `bof3\`), `--lang en-US`,
`--slot0-hold`, `--no-front`, the 173-range skip list, and on every run
`BOF3X_MUSIC_LOG=1 BOF3X_SHOT_EVERY=30` (instrumentation, on both sides of
every comparison). Two conditions:

- **id**: the state hash's, `BOF3X_OPT=none BOF3X_LAYERING=0`;
- **def**: the player's defaults - the ini-less launcher's own settings, no
  `BOF3X_OPT` / `BOF3X_LAYERING` (layering on, the default optional layers).
  Not the owner's play folder's ini, whose `cheat.exp=10` would change the
  fights.

In this build F6 is no key at all and F7 does nothing (DIV-0040;
`Game_WndProc`, `src/game/win_main.cpp`, returns 0 for both). Each route ran
plain, then as `_keys`, under both conditions; the five plain routes ran a
second time under **id** to have a reference pair (the world map's pair with
raw dumps at ticks 1097, 1098 and 1400). One live game at a time under the
machine lock `C:\Users\kerok\q_lock`, taken and given back per run by
`quicksave_check.py run --lock`. 28 runs.

**Every run played its route to the end:** `recipe done` at the route's last
frame (2,143, 2,620, 7,421, 3,157, 3,975), the recipe save handed back to the
owner's slot 0 at the end of each. Wall time 88 s (the world map), 104 s
(combat), 268 s (cutsceneAndNue), 122 s (shop), 150 s (musicStopCheck). The
key lines were logged at the frames meant: `key f6` / `key f7` at recipe
frames 595 / 895, 1270 / 1570, 900 / 1200, 2165 / 2465, 1110 / 1410,
2808 / 3186, `Frame_Counter` equal to the recipe frame each time.

**Pressing the keys changes nothing today** (`quicksave_check.py same PLAIN
KEYS --refb PLAIN2`; the `--refb` rule is `statehash.py check`'s: a page the
two plain runs disagree on at a tick is noise there):

| Route | id: keys against the pair | def: keys against the one plain run | The id pair itself |
|---|---|---|---|
| worldMapAndAreaTransition (595 / 895) | `0x7DE000` at ticks 94 and 596 | `0x6C3000` from 1098 | `0x6C3000` from 1098 to the end |
| worldMap (1270 / 1570) | **identical** where the pair agrees (all 2,113 ticks; `0x6C3000` noise) | `0x6C3000` from 1098 | as above |
| combat (900 / 1200) | **identical**, all 2,561 ticks | `0x7DE000` at tick 433 only | identical, 2,561 ticks |
| cutsceneAndNue (2165 / 2465) | `0x7DE000` at 6 ticks from 2812, `0x7C9000` at 2859, `0x7BB000` and `0x937000` at 2929 | `0x7DE000` at 4 ticks from 1732, `0x7BB000` at 5665 | `0x7C9000` and `0x7DE000` at 1272 and 1285 |
| shop (1110 / 1410) | `0x7DE000` at tick 491 (seven pages at tick 3 that the pair also disagrees on) | **identical**, all 3,137 ticks | ten pages: VRAM shadow and `0x937000` at ticks 3 and 2426, `0x7DE000` at 10 ticks |
| musicStopCheck (2808 / 3186) | **identical**, all 3,969 ticks | `0x7DE000` at tick 2105 only | `0x7DE000` at 2933 and 2957 |

No difference follows a key: each is of a kind the two plain runs show
between themselves - the sound page `0x7DE000` at isolated ticks
([`state-hash.md`](state-hash.md) 4, and 5's open item 1), a VRAM-shadow
page, `Font_TexCache`'s page or `0x937000` for one tick (an upload landing a
frame apart) - and one class new to the record: **`Gfx_ClutRows`' pointer
words**. On the world map route every run differs from every other on page
`0x6C3000` from tick 1098 (the map's palettes converted) to the end - in the
worldMap form before its F6 at 1270 as much as after. The dumps at tick 1098
name it: five words at `0x6C3584`, `0x6C358C`, `0x6C3594`, `0x6C359C`,
`0x6C35A4` (`Gfx_ClutRows+0xB44..0xB64`, the `+4` word of five rows), high
bytes `0x52` in one run and `0x4E` / `0x4F` in the other: each row's pointer
to its 0x1000-byte block of converted palette cells, a heap block
(`symbols.toml` `Gfx_ClutRows`). The skip list masks only the rows above
`0x3D0` (`0x6C3940 256`); the pointer words of the rows below are heap
addresses and want ranges the next time the list changes
([`state-hash.md`](state-hash.md) 6 holds changes to the list for a reason
of their own, so this is written here and not done).

The run folders: `C:\Users\kerok\q_rc\runs\{id,id2,def}_<route>` and
`{id,id2,def}_quick_<route>_keys` (hash `<recipe>.sh`, `bof3x.log`,
`_run.txt`, the timed frames; game-derived, outside the tree). The batch
scripts and their logs: `C:\Users\kerok\q_rc\scratch\` (`batch.sh`,
`all.sh`, `all2.sh`, `ident.sh`, `ident.txt`).

**Not run today:** the proof forms (`quick_<route>.txt`) and the cross forms.
With F7 doing nothing a proof form replays 300 frames from the wrong place;
it was not played on the owner's game folder for that reason (section 1).
`inproc` on the `_keys` runs reads the "did nothing" case live: on
`id_quick_combat_keys` it prints both key lines from the log and `RESULT: f7
did nothing`, exit 2. Installs other than the PC were not run; stage C runs
the US disc-only install.

## 6. For the other groups

- **SC / SP-G: `Gfx_ClutRows` holds heap pointers in the span.** 512 rows
  of (generation counter, pointer to a 0x1000-byte block of converted
  palette cells), the blocks `malloc`'d. An in-process restore puts back
  pointers to blocks that still exist but hold the current conversion, not
  the take's; a cross-process restore puts back pointers into another
  process's heap. A row for the census, rebuilt from the VRAM shadow's CLUT
  rows (`Gfx_ConvertRow`) as the round doc's section 1 rebuilds the
  textures.
- **SC / SP-A: DIV-0092's remembered song position is the DLL's own**
  (statics in `sound.cpp`; DIVERGENCE.md DIV-0092: "the state hash and the
  save files do not see it"). `musicStopCheck`'s window covers a resume from
  it (`resume track 91 at sample 480384` at frame 2838): game state outside
  the span, so a participant's record.
- **SN: the quicksave files go where the game runs.** F6 writes
  `BISLPS1?.DAT` beside the saves; a proof run on the owner's `bof3\` would
  write and rotate quicksaves in the owner's folder. Stage C wants a harness
  game folder (an `abc_run.py` install) for the proof and cross forms.
- **SN / sigma: a quick-load is a save load for the recipe translation**
  (section 1): on a PSX-West or PSP-EU install the cross form presses the
  release's words after the restore unless the restore sets what
  `LoadMenu_Apply` sets. The in-process form is not affected.
- **SN: the recipe and the hash follow `Frame_Counter` moving, not rising**
  (`ScriptedLatch`: `frame != g_last_frame`; `StateHash_Tick` the same): a
  restore that sets the counter back is a new frame for both, which the
  replay and `inproc` rely on.

Since stage B: SP-G's `GFX` carries the rows' contents and keeps the
pointers the process's; SP-A's `SOUND` saves DIV-0092's position; SN's `ST`
puts sigma back as the take had it (section 7.4 measures it on PSP-EU).

## 7. Stage C: the proof batch (2026-10-19)

**The build:** `quicksave/round` `91ca4590`, with SN (snapshot core), SP-A
(`SOUND`), SP-G (`GFX`) and QM (the F5 menu) merged. It was built in this
worktree and run on the exe host. `bof3x.dll` loads at `0x10000000`;
`this build: v0.1.0, bof3x.dll sha256 7162c8f3...`.

**The harness:** root `C:\Users\kerok\q_rc`, and the ini-less launcher copy
`q_rc\launcher`. The game folders come from `abc_run.py prepare
--installs tools/recipes/sigma-installs.toml --only pc,psx-us,psp-eu`:

- `g_pc`: a copy of the PC install, with no saves;
- `g_psxus`: the US PSX disc alone, cache `c_psxus`;
- `g_pspeu`: the PSP-EU disc alone, cache `c_pspeu`.

No run touched the owner's `bof3\`. The recipe saves came from
`q_rc\saves` through `BOF3X_RECIPE_SAVES`, which has to be in
`input_run`'s own environment (`--env` reaches only the game). Every run
used `BOF3X_MUSIC_LOG=1 BOF3X_SHOT_EVERY=30 BOF3X_FRAMEHASH=1` and the
173-range skip list, and every live run held the machine lock `q_lock`.
The conditions were:

- **id**: `BOF3X_OPT=none BOF3X_LAYERING=0`;
- **def**: the launcher's own defaults;
- **wide**: def with `BOF3X_WIDE=1`.

The language was `en-US`, and `en-150` on PSP-EU. In all, 46 live runs
were made, and every one reached `recipe done` at its last frame.

### 7.1 The headless self-test

`BOF3X_SELFTEST_ONLY=1`, the exe host, the ini-less copy, `g_pc`. The run
ended with `self-test only: done, ending the process`, `inject: 10087 ours`,
and no FAILED or Fatal line:

- `snapshot: self-test passed`: the span copied, scribbled and restored
  equal; a missing participant on either side, another build, DLL base,
  arena top and a truncated file refused; the rotation.
- `gfx_snapshot: self-test passed`: palette rows, the cache objects
  released after the present, 12 display ranges kept, a 15,067-byte
  record.
- `SOUND self-test`: four fades equal tick for tick, two banks rebuilt,
  the voices and the stream playing again, a track stream at its folded
  sample, 28 stand-in buffers. This line does not contain the words
  "passed", and no failure was logged.
- `quick menu self-test` (two lines): the save list and the quick list,
  the cursor, Yes / No.

The log is `q_rc\scratch\selftest_exe.log`.

### 7.2 What the check now expects (`--expect`)

The raw dumps of `d_pc_id_quick_combat` were taken at ticks k = 1, 2, 59,
119, 120 and 121 after the take and after the restore. Outside the skip
list, they differ in only three places:

- **`Overlay_Frames` `0x6BC630`, one count apart** (`0x78` against `0x77`
  at k=1, down to `01` / `00` at k=120). SN's "Quickload" line is set after
  the span is written, one frame behind the take's "Quicksave OK".
  `Overlay_Text` itself is equal.
- **`Gpu_DispEnv+2` `0x7DECE2` and `Gpu_DrawEnv+2` `0x7DED02`, at k=1
  only**: the display's buffer parity, `f0`/`00` against `00`/`f0`.
- **`Key_State+0x2F` at k=59**: DirectInput's keyboard byte for V, read
  from the machine's real keyboard while the game ran unfocused. This is
  machine noise and not part of the quicksave. `Key_State` is in no skip
  row.

The hash is per page, so these differences cannot be masked byte by byte.
`inproc` and `cross` therefore take page rules, `--expect PAGE:K0-K1`. A
page whose every differing k falls inside its rule's window is listed as
expected, and the run counts as equal. The defaults are `0x6BC000:0-140`
and `0x7DE000:0-1`. The overlay window is wider than 120 because the line
counts down on drawn frames only: the disc installs' late ticks stretched
it to k=124 and once to k=132, and the loaded process of a cross run
draws its first frames late, so its line ends at k=122. `--expect none`
turns the rules off. In the cross dumps (loaded ticks 2, 3, 121 and 122
against live 1202, 1203, 1321 and 1322), only `Overlay_Frames` (2 or 3
behind) and the same parity at k=1 differ outside the skip list.

### 7.3 In process (`quicksave_check.py inproc`, `tools/quicksave_frames.py`)

These rows apply the defaults above; "equal*" means equal but for the
expected pages. "Frames" is SP-G's walk hash: frames drawn on both sides,
all equal unless noted.

| Route (F6 / F7) | PC id | PC def | US disc, id | PSP-EU, id | PC wide |
|---|---|---|---|---|---|
| worldMapAndAreaTransition (595 / 895) | exit 1: `0x7DE000` and `0x937000` at k=44 only; frames 298 | equal; 299 | equal; 299 | - | - |
| worldMap (1270 / 1570) | equal*; 298 | equal*; 298 | equal; 297 | - | equal*; 298 |
| combat (900 / 1200) | equal*; 298 | equal*; 298 | exit 1: `0x7DE000` 9 ticks, k 3..222; frames 296 | exit 1: `0x7DE000` 20 ticks, k 3..288; frames 296 | - |
| cutsceneAndNue (2165 / 2465) | equal*; 298 | equal*; 298 | exit 1: `0x7BB000`, `0x937000`, `0x7DE000` and `0x7C9000`, k 3..13 (13 late ticks); frames 286 | - | equal*; 298 |
| shop (1110 / 1410) | equal*; 298 | equal*; 298 | exit 1: VRAM shadow (4 pages), k 3..5; frames 294 | exit 1: `0x7DE000` 285 ticks, k 1..299, and VRAM, k 3..5; a repeat had `0x7DE000` 2 ticks and VRAM, k 3..5; frames 294 | - |
| musicStopCheck (2808 / 3108) | equal*; 298 | equal*; 298 | equal; 296 | - | - |
| `quick_menu.txt` (QM's: three F6, F5, slot 1, Yes) | equal* over the 50-tick window (its restore went back to the second take, k ends at the third); frames, see below | the same | - | - | - |

**Every page left over is noise that two runs show anyway** (section 5):

- the sound page `0x7DE000` at isolated ticks;
- the VRAM shadow, `0x937000` or `Font_TexCache`'s page for a tick or a few
  ticks next to late frames: an upload landing a frame apart.

On PSP-EU the sound page is noisy in its own right. The two proof runs of
`quick_shop` there (`c_pspeu_id_` and `d_pspeu_id_`) differ from each other
on `0x7DE000` at 393 ticks, from tick 3, long before the take. No page of
the game's own state differed after a restore, on any install.

**SP-A's Fatal at k=81 on `quick_musicStopCheck` is gone.** No run's log
holds a `FATAL`, a `CRASH` or "Release of a surface". On the merged build
the music route does what the live game did:

- the fade to a stop is taken six steps from its end (`SOUND saved: ...
  fade 6 steps left (to a stop)`) and restored;
- `Music_Stop` follows at f2814 on both sides;
- `resume track 91 at sample 480384` at f2838 on both sides.

**Frames.** `quicksave_frames.py` found every frame drawn after the restore
equal to the one drawn after the take on all 22 in-process route runs.
`quick_menu` reports 40 of 90 frames differing from k=50. That is not a
failure: the tool pairs the restore with the first take (F6 at 596), and
the restore went back to the second (616). From k=50 the take side has the
F5 menu open and the restore side is in the field. Its first 50 frames are
equal.

### 7.4 Across processes (`quicksave_check.py cross`)

The `_cross` form runs right after its proof run, in the same game folder.
It loads that run's `BISLPS10.DAT` at recipe frame 0 and continues with the
route. Every cross run reached its route's end. The loaded run restores at
its tick 1 (`F7 BISLPS10.DAT restored at Frame_Counter 1: back to
Frame_Counter 901`, etc.), and the build check, DLL base and
`Task_StackTop` matched across launches. The results, PC id and def alike:

| Route | Differences against the live run's restore continuation |
|---|---|
| combat | `0x6BC000` k 1..122 and `0x7DE000` k 1..3 only (1,664 ticks); with the expected rules, `0x7DE000` at k 2..3 is the one page left |
| worldMapAndAreaTransition | as combat, plus the VRAM shadow (4 pages) at k 2..3, and `0x6C3000` from k=502 to the end |
| worldMap | as combat, plus the VRAM shadow (43 pages) at k=3, and `0x6C3000` from k=0 to the end |
| musicStopCheck | as combat, plus the VRAM shadow (3 pages) at k=3 |
| shop, cutsceneAndNue | as combat, plus the VRAM shadow and `0x937000` at k=3; then, with no save held, the save-list pages from k=1864 (shop) or 4819 (cutscene): `0x905000`, `0x92A000`, then `0x904000`, `0x929000`, `0x7E0000`, `0x7C9000` and `0x7CA000` |

What they are:

- **`0x6C3000`, `Gfx_ClutRows`' pointer words.** In the fresh process
  SP-G's restore makes the rows' blocks again (`25 blocks made` in the
  loaded log on the world map, 17 for the field route), at this process's
  heap addresses. The words differ by design, the rows' contents are
  the take's, and the frames drawn after the load are SP-G's to judge.
  This is section 5's noise class, now across processes.
- **The save-list tail** (shop and cutscene): the live run held the route's
  recipe save in slot 0 and the cross run loaded none, so the inn's save
  prompt listed different files. With `run --save town` (the live run's
  save held in slot 0), the shop cross run no longer differs there.
- **A transient on shop at k 1738..1792, when the save is held.** It
  appeared on game pages: `0x66C000` (`Game_Mode`), `0x903000`, `0x7E1000`
  (`Input_Held`), `0x803000` and `0x929000`. It came in two runs of three
  (`d_pc_id_` and `e_pc_id_..._cross2`) and not in the one taken with dumps
  (`e_pc_id_..._cross`), and it converged by k=1792. At its first tick the
  music log has `Music_Stop` at f2848 in the live run and f2849 in the
  loaded run. The route's logic there waits on the music, and when the
  stream ends is `Music_Finished`, set from the stream's own timing (the
  skip list's noise row). One run's audio clock is a frame behind the
  other's after a fresh start. This is timing, not something the snapshot
  missed. The two transient cross runs are in the table file below.

**On PSP-EU (a West release, confirm on cross), the cross form plays.**
`quick_combat_cross` and `quick_shop_cross` ran there to their ends, with
the same pages as on the PC plus the PSP install's sound noise (`0x7DE000`,
91 and 690 ticks). The shop run also had the same no-save tail from
k=1864. So SN's `ST` participant puts sigma back as the take had it
(quicksave.md section 5), and section 1's limit for the cross form is
lifted: a West install's buttons after a quick-load are the save's, as in
the live run.

### 7.5 Captures (outside the tree, game data)

Under `C:\Users\kerok\q_rc\caps\`, each pair puts the timed frame k ticks
after F6 beside the one k ticks after F7 (`pairs.txt`). There are 22 pairs:
the six routes on PC id and def, the six on the US disc, two on PSP-EU and
two wide. **All 22 have 0 pixels differing** (tolerance 4 of 255).

**The overlay lines cannot be captured.** "Quicksave OK" and "Quickload"
are logged and never drawn (`overlay     Quicksave OK` in every log, 120
frames). `Overlay()` (`win_main.cpp`, DIV-0032) logs the line whenever the
back buffer is the backend's, as F12's "Save OK" has been since DIV-0032.
The frames at F6+30 and F7+30 show the game alone.

### 7.6 For the owner

- **F6 and F7 give no sign on screen.** The overlay line goes to the log
  only (7.5). A player gets no "Quicksave OK" today. QM draws its refusal
  inside the F5 menu for the same reason.
- `quick_menu`'s live run went through the menu, loaded slot 1 and closed
  it, on both conditions (`F5 at Frame_Counter 665: open, 16 quicksaves
  listed`; the harness folder held the earlier runs' quicksaves, so the
  rotation's sixteen-slot history was full and kept working; then
  `closed at Frame_Counter 616 (BISLPS11.DAT loaded: Quickload)`).

### 7.7 The runs and the table

- Runs: `C:\Users\kerok\q_rc\runs\c_<install>_<cond>_<recipe>`, plus the
  dump and repeat runs `d_pc_id_*`, `d_pspeu_id_*` and `e_pc_id_*`. Each run
  folder holds the hash, `bof3x.log`, `_run.txt`, the timed frames and the
  `.bin` dumps.
- Batch scripts and outputs: `C:\Users\kerok\q_rc\scratch\` (`stagec.sh`,
  `all_c.sh`, `all_c.txt`, `dumps.sh`, `dumps2.sh`, `table.sh`,
  `table.txt`, `caps.py`, `unskipped.py`).

The installs' preparation log is `q_rc\scratch\prepare.txt` (503 s).

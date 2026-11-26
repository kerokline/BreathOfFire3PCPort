# State by name, group B3: the proof - a quicksave taken by one build, loaded by another

**Status:** MEASURED (2026-11-25, the laptop, the Linux x86-64 ELF only; the
round's tip `56e5461e`, B1b merged). The state-by-name round's group B3
([`state-by-name-round.md`](state-by-name-round.md) 5, 4a). Built:
`tools/quicksave_check.py`'s `matrix` and `qsinfo` modes and `tier0`'s
take / load split; `tools/recipes/state_by_name_takes.txt`. No change to
`src/`. No ledger entry: nothing in the game changed. Not played by the
owner. The width row and the system row are owed on the desktop (section 8).

**In one paragraph.** Seventy-five quicksaves taken by one Linux build were
loaded at frame 0 by another and played on with the state hash compared
tick for tick: the round's tip at `-O2` and the same source at `-O1` (other
code addresses for every function), the tip before and after B1b (real
work, the same records), a build with its code moved by `0x1000`, two older
builds of the round. Forty more were loaded forced-portable in the build that
took them. They stand at every one of the fifteen sites, in battles (entry,
mid-turn, exit, a boss), scenes, the shop, the menu's waits, the fishing,
the camp, the faerie village, fades, swings and the title. **62 of the 75
cross-build loads and 38 of the 40 forced ones are equal to the end of their
windows**, and every other one is explained in section 4: the state hash
reads pointers of ours raw (`Overlay_Text`, which every quicksave key sets,
was the first difference found, section 2; the `Effect_Objects` copies,
which B1b re-aims correctly across builds - the first real test of its rule),
two routes' transitions move a frame after any restore or between plain runs
(an exact load meets them too), and **one wrong restore: B1b's copy rule
renames a number** - in U's layout a word of area data lies inside a text
string, so U's take names it and T's load rewrites it (finding 2, a fix
described for B1b's file). Older builds meet the refusals the design gives
them (another GFX record, a site they do not list, format 1, other settings),
each with the save offered and the save loading by tier 0's road; builds from
before B1 crash on a newer file instead (finding 4). Settings that A1 called
safe cross (wide mode and scale differ only in draw state and the scale's own
words; the cheats and faster text act from the load on; the picture is equal);
a behaviour switch and a preset are refused. The ten state-hash routes are
unmoved against this laptop's `orig6` pair. The width and system rows are the
desktop's: section 8 has their commands and what must match.

## 1. The builds

Every build is the Linux x86-64 ELF, clang, linked at `0x10000000`. Each ran
from a copy in `~/bof3x-runs/sbn1125/B3/root/elf/<label>/`, its libraries and
launcher beside it, so that each run's log (written beside the ELF) is its
own. The build stamp a quicksave carries is the tag and the SHA-256 of the
module's file (`snapshot.cpp` `EnsureBuild`), so a copy is the same build and
any rebuild that changes a byte is another.

| Label | What | SHA-256 of `bof3x` | Records |
|---|---|---|---|
| **T** | the round's tip `56e5461e` (B1b merged), the round's own build (RelWithDebInfo: `-O2 -g`) | `2bf9bde3...` | GFX 4 |
| **U** | the same source (this worktree's `src/` is the tip's), `-O1 -g -DNDEBUG` in `build-u` | `a811e322...` | GFX 4 |
| **P** | the round's tip before B1b, `97bd7b2f` (the build T was at when this group started) | `8dece20d...` | GFX 4 |
| **V** | P's source with one function grown: 4 KB of no-ops (`asm volatile(".skip 4096, 0x90")`) at the top of `Fnv1a` in `src/hook/exe_icon_picture.cpp`, the third file linked; built in `build-v`, the edit reverted after, **not committed** | `27ed1d1f...` | GFX 4 |
| **O** | B2's build, `44e9b343`: fifteen sites, tier 0, GFX version 3 | `e820a26b...` | GFX 3 |
| **A** | A2's build, `5ee4b4df`: eight sites, no tier 0, GFX version 3 | `552e940a...` | GFX 3 |
| **F1** | the main checkout's `game/bof3x`, a local build of `fix/linux-desktop-run` from before the round (copied out: run in place it would write its log into the main checkout) | `ee9e6ede...` | format 1 |

**What differs between T and U, measured.**

- **Code addresses: all of them.** `nm` of the two ELFs: `Field_Task` T
  `0x1010CC90`, U `0x10103AB0`; `Title_Task` `0x108C2D30` / `0x10859BB0`;
  `TitleTask_Run` `0x10123FB0` / `0x101198C0`; `Transition_FadeSub`
  `0x1010C520` / `0x101033F0`; `Task_Sleep` `0x1021C3F0` / `0x102048A0`; every
  resume entry and loop likewise (at `97bd7b2f`, P against U0: `FieldTaskLoop`
  `0x10109BD0` / `0x10100B50`, `ResumeFadeSub` `0x101095A0` / `0x101005A0`,
  `MenuCloseAfterSleep` `0x1010F1B0` / `0x101060A0`). `.text` is 12,116,826
  bytes in T and 11,666,270 in U; U has 97 more symbols (20,231 against
  20,134: less inlining). So every return address a stack holds names
  another place in the other build.
- **The frames on the yield chains: none.** The prologue of every function
  on a yield's chain is the same in both (`objdump` of each,
  `root/bin/frames.sh`): `GameMode_Field`, `GameMode_Enter`,
  `GameMode3_Leave`, `Field_Task`, `Title_Task`, `TitleTask_Run` all `push
  rsi; push rdi; sub $0xc8,%rsp`; `Field_WaitTransition` and
  `Menu_WaitTransition` `sub $0xb8`; `Transition_FadeSub` `sub $0xa8`;
  `Task_YieldAt` `push rbp, r14, rbx; sub $0x20`; `FieldWaitLoop` `sub $0x30`.
  The census agrees: U's yield depths on `worldMapAndAreaTransition` are
  B2's (and so T's) to the byte at every site (`0x258` for sites 1 to 5,
  `0x248` / `0x288` for the swings, `0x458`..`0x498` for 9 to 14, `0x338` for
  15). The task bodies are `ms_abi`; their frames are mostly the ten saved
  `xmm` registers, which no optimisation level changes. **So U is another
  build by its code addresses and layout, not by its frames.**
- **A build whose frames differ was tried and does not link.** `-O0 -g
  -DNDEBUG` (`build-d`): `effect_gte.cpp:225` (`EffectEffectGteFromEffectGte_Inject`)
  references `effect_gte::IdentityCheck()`, which is defined nowhere; only an
  optimiser drops the call. A finding for whoever owns that file (section 6).
- **They refuse each other as exact.** Every load in section 4 between T and
  U logs `restored PORTABLE` without `BOF3X_QSPORTABLE`: `Parse` found
  "another build" (the stamp) and took the portable road. The bound-function
  fingerprint is equal (10,088 VAs, FNV-1a `26C32EC0` in every file written
  by T, U, P, V and O), so it refuses nothing here.

**V against P** is the "changed by real work" pair with the same records:
every function after `Fnv1a` moves by exactly `0x1000` (`Field_Task`
`0x10109B40` -> `0x1010AB40`, `Transition_FadeSub` `0x101093D0` ->
`0x1010A3D0`, `GameMode3_Leave` `0x1010ABD0` -> `0x1010BBD0`). **P against
T** is the same with real work between them (B1b's changes to
`snapshot.cpp` and `span_words.h`).

**Would two releases meet the bound-function refusal?** Only if they bind
other sets of Capcom's functions - a release that takes over one more
function does. From one release to the next that is the normal case, so a
release that adds an `impl` binding refuses every portable quicksave of the
release before it, "another build (other functions bound)", with the save
offered. For two real releases to exchange a quicksave, their bound VAs must
be the same set; the check is A1's design (a code word left unbound by the
writer reads as a number, [`state-by-name-format.md`](state-by-name-format.md)
3), and a converter would be needed to do better.

## 2. The state hash cannot compare two builds as it stands

**The first portable load across builds differed** - T's take on `combat` at
`Frame_Counter` 901, loaded by U: one page, `0x6BC000`, from the first tick
after the restore to the end (489 ticks). The sweep stopped there.

- **Which word.** Raw dumps (`BOF3X_STATEHASH_DUMP=10,200`) of a forced load
  in T and a load in U of the same file, compared with `statehash.py bytes`:
  on that page the one hashed word that differs is **`Overlay_Text`
  (`0x6BC62C`)**, `0x11BBAF60` in the one and `0x11B4BF80` in the other.
  The rest of the page's differences are `Sound_Banks`, already skipped as
  noise.
- **What it is.** A word of Capcom's in the span, the overlay's line. F6, F7
  and the quick menu point it at a buffer of ours (`snapshot.cpp` `Show`:
  `Overlay_Text = g_overlay`; `quick_menu.cpp` `g_f7_line`), so from the first
  quicksave key on the span holds an address in our module's `.bss`, and
  every build links that buffer elsewhere.
- **Which code wrote it, and is the game wrong?** No. The live run's F6
  wrote T's address. The portable load writes the file's (T's) value into
  U's span, and `DoLoad` calls `Show("Quickload")` right after `Restore`
  (`snapshot.cpp:1571`, the same call, before a frame runs), which sets U's
  own address. The game never reads the foreign one.
- **The control that settles it:** the same take recipe played live by U,
  with no load at all, against T's live run: **identical on ticks 0 to 900,
  different on `0x6BC000` from 901** (the F6 frame) to the end. Two builds
  differ there by pressing F6, load or no load.

So the state hash, which folds Capcom's slots and 17 named run-time slots,
reads three kinds of pointer of ours raw, and between two builds each is a
difference that is not the game's:

| Word | What | Seen |
|---|---|---|
| `Overlay_Text` `0x6BC62C` | the overlay's line after a quicksave key | every take of this sweep |
| `Msgbox_Base`, `MenuWindows_ListText`, `Msgbox_Resume` `0x7DEE4C` (12), `BattleFlow_TaskCurrent+0x38` `0x93B8FC` | text in our moved message pool (DIV-0007); B1b's `kTextWords`, which the quicksave writes by name | B1b's build differential: `shop` (page `0x7DE000`, read until then as sound noise), `cutsceneAndNue` (`0x93B000`) |
| six `Effect_Objects` words | copies of and cursors into a text slot's string (DIV-0064's lines) | `masterAndManillo` (`0x7E1000`) |

**The proof's skip list.** Both sides of every comparison in section 4
hash under the state hash's list plus three rows (`quicksave_check.py`
`PROOF_SKIP_EXTRA`, written as `proof_skip.txt` into each matrix's `--out`):
`0x6BC62C 4` and `0x7DEE4C 12`, `0x93B8FC 4`. Same list both sides, so the
headers agree. What the skipped words mean after a load is still checked by
what the game does next: the box prints from them, and the printer's state
is hashed. The `Effect_Objects` words are in records the game allocates, so
they cannot be listed by address; a take that holds them is read in section
4. Row 6 (nothing else moved) uses the state hash's own list.

**Made 2026-11-26 (the owner's call).** The three rows are in
`tools/statehash_skip.txt` (179 ranges, hash `AFD29A9A`), `PROOF_SKIP_EXTRA`
is gone and the matrix hashes under the state hash's own list. The reference
pairs are recorded again under it (section 10.5).

## 3. How every row ran

`quicksave_check.py matrix` (section 7), one game at a time under the
round's lock, from the captures' folder `~/bof3x-runs/sbn1125/B3/root/m/<row>/`
(each holds the take's and every load's recipe, hash, log and quicksave;
`results.tsv` is the row's table). Every run is headless, `--direct`, under
the state hash's conditions as [`state-hash.md`](state-hash.md) 6 has them
for this laptop: `--only-slot0` (slot 0 the recipe's save, the pinned
`BOF3.CFG`), `BOF3X_SCALE=2`, `BOF3X_OPT=none`, `BOF3X_LAYERING=0`, and the
proof's skip list (section 2); `en-US` unless a take says `lang=none`.

**A take** is the route cut to its recipe frame FC-1 plus 500 frames (300 on
`whelpBoss` and `bossAndFlash`'s end), that frame made idle when it held a
button (both sides play the same cut route, so the comparison is of the same
inputs), five peeks on it (`Game_Mode`, `Game_Step`, the transition kind, the
area, `MenuLists_CampChosen`: a peek takes no frame), then `key f6` in its
place. The live run takes and plays on to the cut's end. **A load** is a
second process in a copy of the live run's folder: `key f7` at recipe frame
0, then the same route from frame FC. `cross` matches the loaded run's
restore with the live record of the same `Frame_Counter` and compares every
page of every tick from there to the shorter run's end.

The takes' frames came from the census: B2's files of eight routes and this
group's traced census of six (`combat`, `whelpBoss`, `campingFishing`,
`caughFish`, `bossAndFlash`, `fairyVillage`: the route with a peek of the
five words every five frames, the probe on, the takes' conditions; and row
6's runs with the probe on for the ten state-hash routes). The site each
take stood at is the file's `TASKS` record as read back (`qsinfo`), not the
census's prediction: they agreed on every take. The take list is
`tools/recipes/state_by_name_takes.txt`.

`menu_screens` and `field_menu` are hand-written recipes (`press`, `until`,
`seek`): their frames depend on conditions and cannot be cut at a frame, so
the takes for sites 10 and 13 are on `worldMapAndAreaTransition`, the one
other route that reaches them.

## 4. The rows

### Row 1 and row 2: T takes, T loads forced and U loads; U takes, T loads

n12 (taken by T)

| Take | Route, FC | Where (peeks) | Tasks | loaded by T! | loaded by U |
|---|---|---|---|---|---|
| c901 | `combat` 901 | mode=5 step=5 kind=1 area=29 | 0 site 1, 1 free, 2 free, 3 free | portable, equal 489 | portable, equal 489 |
| s1111 | `shop` 1111 | mode=7 step=1 kind=1 area=7 | 0 site 1, 1 free, 2 free, 3 free | portable, equal 490 | portable, equal 490 |
| s100 | `shop` 100 | mode=0 step=0 kind=0 area=0 | 0 free, 1 site 3, 2 free, 3 free | portable, equal 477 | portable, equal 477 |
| s290 | `shop` 290 | mode=1 step=3 kind=0 area=0 | 0 site 2, 1 free, 2 site 4 (00200000000800000100000000000000), 3 free | portable, equal 479 | portable, equal 479 |
| s2638 | `shop` 2638 | mode=7 step=1 kind=0 area=7 | 0 site 1, 1 free, 2 site 4 (00400000000800000100000000000000), 3 free | portable, **differs** k=210, 8 pages | portable, **differs** k=211, 8 pages |
| w11680 | `whelpBoss` 11680 | mode=2 step=0 kind=15 area=10 | 0 site 1, 1 free, 2 site 5 (00380000000200000100000000000000), 3 free | portable, equal 289 | portable, equal 289 |
| f605 | `fairyVillage` 605 | mode=2 step=0 kind=10 area=65 | 0 site 11 (0000000000000000), 1 free, 2 site 6 (003c000000040000), 3 free | portable, equal 484 | portable, equal 484 |
| f297 | `fairyVillage` 297 | mode=1 step=0 kind=11 area=65 | 0 site 9 (0100000000000000), 1 free, 2 site 7 (ff73000002000000), 3 free | portable, equal 472 | portable, equal 472 |
| f4640 | `fairyVillage` 4640 | mode=1 step=0 kind=11 area=65 | 0 site 9 (0100000000000000), 1 free, 2 site 8 (ff3b000000000000), 3 free | portable, equal 59 | portable, equal 59 |
| s378 | `shop` 378 | mode=1 step=0 kind=1 area=7 | 0 site 9 (0100000000000000), 1 free, 2 site 4 (ff3f000000f8ffff0100000000000000), 3 free | portable, equal 455 | portable, equal 455 |
| s360 | `shop` 360 | mode=2 step=0 kind=0 area=7 | 0 site 11 (0000000000000000), 1 free, 2 site 4 (00400000000800000100000000000000), 3 free | portable, equal 473 | portable, equal 473 |
| f3375 | `fairyVillage` 3375 | mode=2 step=0 kind=0 area=177 | 0 site 11 (0000000000000000), 1 free, 2 site 4 (00400000000800000100000000000000), 3 free | portable, equal 488 | portable, equal 488 |
| w681 | `worldMapAndAreaTransition` 681 | mode=3 step=2 kind=3 area=7 | 0 site 12 (0100000000000000), 1 free, 2 site 4 (ff3f000000e0ffff0100000000000000), 3 free | portable, equal 480 | portable, equal 480 |
| w675 | `worldMapAndAreaTransition` 675 | mode=3 step=2 kind=2 area=7 | 0 site 14 (0000000000000000), 1 free, 2 site 4 (00400000002000000100000000000000), 3 free | portable, equal 478 | portable, equal 478 |
| w679 | `worldMapAndAreaTransition` 679 | mode=3 step=2 kind=2 area=7 | 0 site 15, 1 free, 2 free, 3 free | portable, equal 474 | portable, equal 474 |
| mm3320 | `masterAndManillo` 3320 | mode=1 step=0 kind=11 area=16 | 0 site 9 (0100000000000000), 1 free, 2 site 8 (ff47000000000000), 3 free | portable, equal 479 | portable, **differs** k=0, 1 pages |
| c407 | `combat` 407 | mode=5 step=1 kind=1 area=29 | 0 site 1, 1 free, 2 free, 3 free | portable, equal 479 | portable, equal 479 |
| c412 | `combat` 412 | mode=5 step=2 kind=1 area=29 | 0 site 1, 1 free, 2 free, 3 free | portable, equal 485 | portable, equal 485 |
| c440 | `combat` 440 | mode=5 step=4 kind=1 area=29 | 0 site 1, 1 free, 2 free, 3 free | portable, equal 487 | portable, equal 487 |
| wb1078 | `whelpBoss` 1078 | mode=2 step=0 kind=1 area=2 | 0 site 1, 1 free, 2 free, 3 free | portable, equal 267 | portable, equal 267 |
| wb1090 | `whelpBoss` 1090 | mode=4 step=0 kind=1 area=2 | 0 site 1, 1 free, 2 free, 3 free | portable, equal 296 | portable, equal 296 |
| wb1144 | `whelpBoss` 1144 | mode=2 step=0 kind=3 area=2 | 0 site 1, 1 free, 2 site 4 (ff3f000000e0ffff0100000000000000), 3 free | portable, equal 265 | portable, equal 265 |
| s2500 | `shop` 2500 | mode=7 step=1 kind=1 area=7 | 0 site 1, 1 free, 2 free, 3 free | portable, equal 497 | portable, **differs** k=348, 8 pages |
| mm1600 | `masterAndManillo` 1600 | mode=8 step=1 kind=1 area=30 | 0 site 1, 1 free, 2 free, 3 free | portable, equal 481 | portable, **differs** k=0, 1 pages |
| bf1753 | `bossAndFlash` 1753 | mode=5 step=1 kind=1 area=27 | 0 site 1, 1 free, 2 free, 3 free | portable, equal 469 | portable, equal 469 |
| bf3000 | `bossAndFlash` 3000 | mode=5 step=5 kind=1 area=27 | 0 site 1, 1 free, 2 free, 3 free | portable, equal 478 | portable, equal 478 |
| bf5050 | `bossAndFlash` 5050 | mode=5 step=7 kind=1 area=27 | 0 site 1, 1 free, 2 free, 3 free | portable, equal 448 | portable, equal 448 |
| bf900 | `bossAndFlash` 900 | mode=4 step=0 kind=1 area=27 | 0 site 1, 1 free, 2 free, 3 free | portable, equal 482 | portable, equal 482 |
| cf1500 | `caughFish` 1500 | mode=8 step=1 kind=1 area=30 | 0 site 1, 1 free, 2 free, 3 free | portable, equal 485 | portable, **differs** k=0, 1 pages |
| cf3600 | `caughFish` 3600 | mode=8 step=3 kind=1 area=30 | 0 site 1, 1 free, 2 free, 3 free | portable, equal 256 | portable, **differs** k=0, 1 pages |
| cp1000 | `campingFishing` 1000 | mode=7 step=1 kind=3 area=90 | 0 site 1, 1 free, 2 free, 3 free | portable, equal 473 | portable, equal 473 |
| cp2590 | `campingFishing` 2590 | mode=7 step=1 kind=0 area=90 | 0 site 1, 1 free, 2 site 4 (00400000000800000100000000000000), 3 free | portable, **differs** k=213, 24 pages | portable, **differs** k=213, 24 pages |
| cp370 | `campingFishing` 370 | mode=1 step=0 kind=11 area=16 | 0 site 9 (0100000000000000), 1 free, 2 site 7 (ff6f000003000000), 3 free | portable, equal 463 | portable, equal 463 |
| cp560 | `campingFishing` 560 | mode=2 step=0 kind=10 area=16 | 0 site 11 (0000000000000000), 1 free, 2 site 6 (0028000000040000), 3 free | portable, equal 465 | portable, equal 465 |
| cn2166 | `cutsceneAndNue` 2166 | mode=4 step=0 kind=1 area=23 | 0 site 1, 1 free, 2 free, 3 free | portable, equal 459 | portable, equal 459 |
| cn1020 | `cutsceneAndNue` 1020 | mode=2 step=0 kind=0 area=23 | 0 site 11 (0000000000000000), 1 free, 2 site 4 (00300000000800000100000000000000), 3 free | portable, equal 493 | portable, equal 493 |
| s180 | `shop` 180 | mode=1 step=1 kind=0 area=0 | 0 site 2, 1 free, 2 free, 3 free | portable, equal 484 | portable, equal 484 |
| w634 | `worldMapAndAreaTransition` 634 | mode=2 step=0 kind=2 area=7 | 0 site 10 (0000000000000000), 1 free, 2 site 4 (00400000002000000100000000000000), 3 free | portable, equal 488 | portable, equal 488 |
| w637 | `worldMapAndAreaTransition` 637 | mode=2 step=0 kind=2 area=7 | 0 site 10 (0000000001000000), 1 free, 2 free, 3 free | portable, equal 487 | portable, equal 487 |
| w640 | `worldMapAndAreaTransition` 640 | mode=3 step=0 kind=3 area=7 | 0 site 13 (0100000000000000), 1 free, 2 site 4 (ff3f000000e0ffff0100000000000000), 3 free | portable, equal 490 | portable, equal 490 |

The differences, each found out (the sweep stopped at each until it was):

- **`s2638`, `s2500`** (the shop): the first differing word is task 0's
  `Field_Request` at `Frame_Counter` 2848..2849, where the census has the
  shop's fade start (2849), then the sound page to the end. It is the shop's
  jingle transition (DIV-0201), which waits on `Music_Finished`, set from the
  music stream's own timing; the restore restarts the stream at its sample,
  and the transition lands a frame apart. **An exact load in T differs at
  the same tick (k = 210)**, and three live runs of the take with no load are
  identical to each other: so it is any restore's, not the portable one's. It
  is the window [`state-hash.md`](state-hash.md) 6 records between two plain
  runs on two machines (ticks 2,712..2,756 there). On `s2500` T's forced load
  happened to land on the live run's frame and U's one frame off.
- **`cp2590`** (a fade at the camp): the same shape at `Frame_Counter`
  2802..2803; **a second live run of T, with no load, differs from the first
  at tick 2802**. The route's own noise.
- **`mm1600`, `mm3320`, `cf1500`, `cf3600`** (U only; T's forced loads
  equal): page `0x7E1000` alone, from the first tick. Raw dumps of T's forced
  load and U's load at ticks 10 and 300: the hashed words that differ are
  `Effect_Objects` words holding copies of and cursors into DIV-0064's lines
  (`g_lines`), `0x114FF1CD` in T against `0x114901DD` in U and so on - each
  exactly `0x6EFF0` apart, which is how far `g_lines` moved between the two
  builds (`0x114FF080` / `0x11490090`): **the same offset into the same
  string in each build** (`0x14D`, `0x14C`, `0x112`; on the fishing take
  `0xC0`, `0xCB`). B1b's copy rule re-aimed them right; the hash reads them
  raw (section 2). This is the first real cross-build load of B1b's case.


n2u (taken by U)

| Take | Route, FC | Where (peeks) | Tasks | loaded by T |
|---|---|---|---|---|
| c901 | `combat` 901 | mode=5 step=5 kind=1 area=29 | 0 site 1, 1 free, 2 free, 3 free | portable, equal 489 |
| s1111 | `shop` 1111 | mode=7 step=1 kind=1 area=7 | 0 site 1, 1 free, 2 free, 3 free | portable, equal 490 |
| s100 | `shop` 100 | mode=0 step=0 kind=0 area=0 | 0 free, 1 site 3, 2 free, 3 free | portable, equal 477 |
| s290 | `shop` 290 | mode=1 step=3 kind=0 area=0 | 0 site 2, 1 free, 2 site 4 (00200000000800000100000000000000), 3 free | portable, equal 479 |
| s2638 | `shop` 2638 | mode=7 step=1 kind=0 area=7 | 0 site 1, 1 free, 2 site 4 (00400000000800000100000000000000), 3 free | portable, **differs** k=210, 8 pages |
| w11680 | `whelpBoss` 11680 | mode=2 step=0 kind=15 area=10 | 0 site 1, 1 free, 2 site 5 (00380000000200000100000000000000), 3 free | portable, equal 289 |
| f605 | `fairyVillage` 605 | mode=2 step=0 kind=10 area=65 | 0 site 11 (0000000000000000), 1 free, 2 site 6 (003c000000040000), 3 free | portable, **differs** k=0, 1 pages |
| f297 | `fairyVillage` 297 | mode=1 step=0 kind=11 area=65 | 0 site 9 (0100000000000000), 1 free, 2 site 7 (ff73000002000000), 3 free | portable, **differs** k=0, 1 pages |
| f4640 | `fairyVillage` 4640 | mode=1 step=0 kind=11 area=65 | 0 site 9 (0100000000000000), 1 free, 2 site 8 (ff3b000000000000), 3 free | portable, **differs** k=0, 1 pages |
| s378 | `shop` 378 | mode=1 step=0 kind=1 area=7 | 0 site 9 (0100000000000000), 1 free, 2 site 4 (ff3f000000f8ffff0100000000000000), 3 free | portable, equal 455 |
| s360 | `shop` 360 | mode=2 step=0 kind=0 area=7 | 0 site 11 (0000000000000000), 1 free, 2 site 4 (00400000000800000100000000000000), 3 free | portable, equal 473 |
| w681 | `worldMapAndAreaTransition` 681 | mode=3 step=2 kind=3 area=7 | 0 site 12 (0100000000000000), 1 free, 2 site 4 (ff3f000000e0ffff0100000000000000), 3 free | portable, equal 480 |
| w675 | `worldMapAndAreaTransition` 675 | mode=3 step=2 kind=2 area=7 | 0 site 14 (0000000000000000), 1 free, 2 site 4 (00400000002000000100000000000000), 3 free | portable, equal 478 |
| w679 | `worldMapAndAreaTransition` 679 | mode=3 step=2 kind=2 area=7 | 0 site 15, 1 free, 2 free, 3 free | portable, equal 480 |
| c407 | `combat` 407 | mode=5 step=1 kind=1 area=29 | 0 site 1, 1 free, 2 free, 3 free | portable, equal 479 |
| wb1078 | `whelpBoss` 1078 | mode=2 step=0 kind=1 area=2 | 0 site 1, 1 free, 2 free, 3 free | portable, equal 267 |
| s2500 | `shop` 2500 | mode=7 step=1 kind=1 area=7 | 0 site 1, 1 free, 2 free, 3 free | portable, **differs** k=348, 8 pages |
| mm1600 | `masterAndManillo` 1600 | mode=8 step=1 kind=1 area=30 | 0 site 1, 1 free, 2 free, 3 free | portable, **differs** k=0, 1 pages |
| s180 | `shop` 180 | mode=1 step=1 kind=0 area=0 | 0 site 2, 1 free, 2 free, 3 free | portable, equal 484 |
| w634 | `worldMapAndAreaTransition` 634 | mode=2 step=0 kind=2 area=7 | 0 site 10 (0000000000000000), 1 free, 2 site 4 (00400000002000000100000000000000), 3 free | portable, equal 488 |
| w637 | `worldMapAndAreaTransition` 637 | mode=2 step=0 kind=2 area=7 | 0 site 10 (0000000001000000), 1 free, 2 free, 3 free | portable, equal 487 |
| w640 | `worldMapAndAreaTransition` 640 | mode=3 step=0 kind=3 area=7 | 0 site 13 (0100000000000000), 1 free, 2 site 4 (ff3f000000e0ffff0100000000000000), 3 free | portable, equal 490 |

The differences in this direction:

- **`s2638`, `s2500`, `mm1600`**: as above (the shop's jingle; the
  `Effect_Objects` copies, now U's addresses in the live run and T's in the
  loaded one).
- **`f297`, `f605`, `f4640`** (the faerie village; T to U was equal on all
  three): **a wrong restore**, page `0x826000`, one word, `0x8262EC`, every
  tick. Raw dumps at ticks 10 and 300: in the live runs of **both** builds,
  and in T's own forced load and U's load of T's take, the word holds
  `0x1149015B` - a number, game data in the arena where `LoadDatFile` puts an
  area (`Gfx_ClutStrip+0x16D6C`, four bytes of the area's picture or palette
  data; a pointer written by our code would hold each build's own address).
  In U's process that number happens to lie inside DIV-0064's lines
  (`g_lines`, `0x11490090` in U, plus `0xCB`), so at U's take **B1b's copy
  rule names it** as a cursor into the text slot, and T's load writes T's
  `g_lines` plus `0xCB`, `0x114FF14B`, into the area data. T's loaded game
  then holds four bytes of area data the live game never had (a picture or
  palette word here, by its place; what it does on screen was not looked
  at). B1b measured no false positive on the layouts of its two builds; with
  U's layout a data word falls inside a string. Which data words are exposed
  depends on where each build links its text, so two releases meet it by
  chance (section 6, finding 2).

**Every site.** Sites 1 to 15 each have at least one take that loaded
portably in the other build and compared equal (section 5 lists them by
site). Sites 12, 14 and 15 are reached by `worldMapAndAreaTransition` alone
on all nineteen routes censused (the one menu visit that leaves by
`GameMode3_Leave`); `campingFishing` reaches the camp from the world map, not
through the menu's camp branch.

**Not reached by any route, so no take:** the swings of kinds 17 and 18
(every swing on the traced routes is kind 10 or 11: the transition kind's
peek over six routes), `GameMode3_Leave`'s camp and load branches
(`MenuLists_CampChosen` 0 on every traced frame), `Transition_Kind20`.

### Row 3: older builds of the round, and builds changed by real work

n3o (taken by O)

| Take | Route, FC | Where (peeks) | Tasks | loaded by T | loaded by O | loaded by A |
|---|---|---|---|---|---|---|
| c901 | `combat` 901 | mode=5 step=5 kind=1 area=29 | 0 site 1, 1 free, 2 free, 3 free | refused: "another graphics record", save offered | exact, equal 489 | portable, equal 489 |
| f297 | `fairyVillage` 297 | mode=1 step=0 kind=11 area=65 | 0 site 9 (0100000000000000), 1 free, 2 site 7 (ff73000002000000), 3 free | refused: "another graphics record", save offered | exact, equal 472 | refused: "another build (a task at an unknown place)", save offered |
| s360 | `shop` 360 | mode=2 step=0 kind=0 area=7 | 0 site 11 (0000000000000000), 1 free, 2 site 4 (00400000000800000100000000000000), 3 free | refused: "another graphics record", save offered | exact, equal 473 | refused: "another build (a task at an unknown place)", save offered |

n3t (taken by T)

| Take | Route, FC | Where (peeks) | Tasks | loaded by O | loaded by A | loaded by P |
|---|---|---|---|---|---|---|
| c901 | `combat` 901 | mode=5 step=5 kind=1 area=29 | 0 site 1, 1 free, 2 free, 3 free | **Fatal**: gfx_snapshot: the record is another version | **Fatal**: gfx_snapshot: the record is another version | portable, equal 489 |
| f297 | `fairyVillage` 297 | mode=1 step=0 kind=11 area=65 | 0 site 9 (0100000000000000), 1 free, 2 site 7 (ff73000002000000), 3 free | **Fatal**: gfx_snapshot: the record is another version | refused: "another build (a task at an unknown place)", save offered | portable, equal 472 |
| s360 | `shop` 360 | mode=2 step=0 kind=0 area=7 | 0 site 11 (0000000000000000), 1 free, 2 site 4 (00400000000800000100000000000000), 3 free | **Fatal**: gfx_snapshot: the record is another version | refused: "another build (a task at an unknown place)", save offered | portable, equal 473 |

n3p (taken by P)

| Take | Route, FC | Where (peeks) | Tasks | loaded by T |
|---|---|---|---|---|
| c901 | `combat` 901 | mode=5 step=5 kind=1 area=29 | 0 site 1, 1 free, 2 free, 3 free | portable, equal 489 |
| f297 | `fairyVillage` 297 | mode=1 step=0 kind=11 area=65 | 0 site 9 (0100000000000000), 1 free, 2 site 7 (ff73000002000000), 3 free | portable, equal 472 |
| s360 | `shop` 360 | mode=2 step=0 kind=0 area=7 | 0 site 11 (0000000000000000), 1 free, 2 site 4 (00400000000800000100000000000000), 3 free | portable, equal 473 |

n3v (taken by V)

| Take | Route, FC | Where (peeks) | Tasks | loaded by P | loaded by T |
|---|---|---|---|---|---|
| c901 | `combat` 901 | mode=5 step=5 kind=1 area=29 | 0 site 1, 1 free, 2 free, 3 free | portable, equal 489 | portable, equal 489 |
| f297 | `fairyVillage` 297 | mode=1 step=0 kind=11 area=65 | 0 site 9 (0100000000000000), 1 free, 2 site 7 (ff73000002000000), 3 free | portable, equal 472 | portable, equal 472 |
| s360 | `shop` 360 | mode=2 step=0 kind=0 area=7 | 0 site 11 (0000000000000000), 1 free, 2 site 4 (00400000000800000100000000000000), 3 free | portable, equal 473 | portable, equal 473 |

The pairs with the same records exchange: **P and T both ways** (the round
before and after B1b, real work in `snapshot.cpp` and `span_words.h`),
**V into P and into T** (code moved by `0x1000`), and **O into A** (two older
builds, both GFX version 3). The pairs with other records do not:

- **T refuses O's takes** for their graphics record, the save offered:
  "another graphics record - record 'GFX': GFX's record is version 3; this
  build reads 4 only (version 3 and before wrote the pipe state as the
  writing build's struct) (the save half is whole: it can be loaded as a
  save)". Tier 0's road then loads O's save half in T (`t0/oT`): refused,
  the question, the restart, the title's load, and the party where the
  title's own load of that save puts it (area 29, x `0x1A0000`, z
  `0x190000`, the picture equal).
- **A refuses** a take at a site it does not list: "another build (a task at
  an unknown place) ... task 0 stands at site 9, which this build ...".
- **O and A, loading T's takes, stop with `FATAL: gfx_snapshot: the record is
  another version`** - after the span is written, since neither checks GFX's
  version in `Parse` (B1 moved that check into `Parse`; A1's hand-back to B1).
  O's own F5 road meets the same Fatal before it can ask (`t0/tO`). The
  brief expected "a file from T refused by O": **that premise does not hold
  for builds from before B1**. It is not reachable in play: no release reads
  format 2 before the round's release, and every build after B1 refuses
  another GFX version with its save offered.

### Row 4: a nested frame the round leaves

n4 (taken by T)

| Take | Route, FC | Where (peeks) | Tasks | loaded by T | loaded by T! | loaded by U |
|---|---|---|---|---|---|---|
| mm3297 | `masterAndManillo` 3297 | mode=8 step=2 kind=0 area=30 | 0 nowhere, 1 free, 2 site 4 (00400000000800000100000000000000), 3 free (**not portable**) | exact, equal 480 | refused: "forced portable (not portable)", save offered | refused: "another build (not portable)", save offered |
| cp5100 | `campingFishing` 5100 | mode=8 step=2 kind=0 area=30 | 0 nowhere, 1 free, 2 site 4 (00500000000800000100000000000000), 3 free (**not portable**) | exact, equal 149 | refused: "forced portable (not portable)", save offered | refused: "another build (not portable)", save offered |

Both are `GameMode8_Leave`'s framed wait at the fishing spot (mode 8 step 2,
area 30), B2's frame and a second one on `campingFishing`. Each is written
not portable ("a task stands at no named place (task 0)"), restores exactly
in T, is refused forced, and is refused by U as "another build (not
portable)" with the save offered. Its save half then loads in U by tier 0's
road (`t0/mmU`): the party at the fishing spot (area 30, x `0xF0000`, z
`0x410000`) with the came-from place as the save holds it (area 16, x
`0x300000`, z `0x240000`), all seven words equal to the title's own load of
the same save. The picture differs only in the water's animation phase (the
same place, "Casting...", the party at the same spot: looked at).

### Row 5: settings

n5wide01 (taken by T)

| Take | Route, FC | Where (peeks) | Tasks | loaded by T | loaded by U |
|---|---|---|---|---|---|
| c901 | `combat` 901 | mode=5 step=5 kind=1 area=29 | 0 site 1, 1 free, 2 free, 3 free | exact, **differs** k=0, 10 pages | portable, **differs** k=0, 10 pages |
| f4640 | `fairyVillage` 4640 | mode=1 step=0 kind=11 area=65 | 0 site 9 (0100000000000000), 1 free, 2 site 8 (ff3b000000000000), 3 free | exact, **differs** k=0, 10 pages | portable, **differs** k=0, 10 pages |

n5wide11 (taken by T)

| Take | Route, FC | Where (peeks) | Tasks | loaded by T | loaded by U |
|---|---|---|---|---|---|
| c901 | `combat` 901 | mode=5 step=5 kind=1 area=29 | 0 site 1, 1 free, 2 free, 3 free | exact, equal 489 | portable, equal 489 |
| f4640 | `fairyVillage` 4640 | mode=1 step=0 kind=11 area=65 | 0 site 9 (0100000000000000), 1 free, 2 site 8 (ff3b000000000000), 3 free | exact, equal 59 | portable, equal 59 |

n5wide10 (taken by T)

| Take | Route, FC | Where (peeks) | Tasks | loaded by T | loaded by U |
|---|---|---|---|---|---|
| f4640 | `fairyVillage` 4640 | mode=1 step=0 kind=11 area=65 | 0 site 9 (0100000000000000), 1 free, 2 site 8 (ff3b000000000000), 3 free | exact, **differs** k=0, 10 pages | portable, **differs** k=0, 10 pages |

n5scale (taken by T)

| Take | Route, FC | Where (peeks) | Tasks | loaded by T | loaded by U |
|---|---|---|---|---|---|
| c901 | `combat` 901 | mode=5 step=5 kind=1 area=29 | 0 site 1, 1 free, 2 free, 3 free | exact, **differs** k=0, 3 pages | portable, **differs** k=0, 3 pages |
| f4640 | `fairyVillage` 4640 | mode=1 step=0 kind=11 area=65 | 0 site 9 (0100000000000000), 1 free, 2 site 8 (ff3b000000000000), 3 free | exact, **differs** k=0, 3 pages | portable, **differs** k=0, 3 pages |

n5scale3 (taken by T)

| Take | Route, FC | Where (peeks) | Tasks | loaded by T |
|---|---|---|---|---|
| c901 | `combat` 901 | mode=5 step=5 kind=1 area=29 | 0 site 1, 1 free, 2 free, 3 free | exact, equal 489 |
| f4640 | `fairyVillage` 4640 | mode=1 step=0 kind=11 area=65 | 0 site 9 (0100000000000000), 1 free, 2 site 8 (ff3b000000000000), 3 free | exact, equal 59 |

n5cheat (taken by T)

| Take | Route, FC | Where (peeks) | Tasks | loaded by T | loaded by U |
|---|---|---|---|---|---|
| wb1000 | `whelpBoss` 1000 | mode=5 step=5 kind=1 area=2 | 0 site 1, 1 free, 2 free, 3 free | exact, equal 281 | portable, equal 281 |

n5cheat0 (taken by T)

| Take | Route, FC | Where (peeks) | Tasks | loaded by T | loaded by U |
|---|---|---|---|---|---|
| wb1000 | `whelpBoss` 1000 | mode=5 step=5 kind=1 area=2 | 0 site 1, 1 free, 2 free, 3 free | exact, equal 281 | portable, equal 281 |

n5cheatb (taken by T)

| Take | Route, FC | Where (peeks) | Tasks | loaded by T | loaded by U |
|---|---|---|---|---|---|
| bf4950 | `bossAndFlash` 4950 | mode=5 step=5 kind=1 area=27 | 0 site 1, 1 free, 2 free, 3 free | exact, **differs** k=1, 58 pages | portable, **differs** k=1, 58 pages |

n5cheatb0 (taken by T)

| Take | Route, FC | Where (peeks) | Tasks | loaded by T |
|---|---|---|---|---|
| bf4950 | `bossAndFlash` 4950 | mode=5 step=5 kind=1 area=27 | 0 site 1, 1 free, 2 free, 3 free | exact, equal 384 |

n5text (taken by T)

| Take | Route, FC | Where (peeks) | Tasks | loaded by T | loaded by U |
|---|---|---|---|---|---|
| cn2166 | `cutsceneAndNue` 2166 | mode=4 step=0 kind=1 area=23 | 0 site 1, 1 free, 2 free, 3 free | exact, **differs** k=91, 8 pages | portable, **differs** k=91, 8 pages |

n5pic (taken by T)

| Take | Route, FC | Where (peeks) | Tasks | loaded by T | loaded by U |
|---|---|---|---|---|---|
| c901 | `combat` 901 | mode=5 step=5 kind=1 area=29 | 0 site 1, 1 free, 2 free, 3 free | exact, equal 489 | portable, equal 489 |
| f4640 | `fairyVillage` 4640 | mode=1 step=0 kind=11 area=65 | 0 site 9 (0100000000000000), 1 free, 2 site 8 (ff3b000000000000), 3 free | exact, equal 59 | portable, equal 59 |

n5on (taken by T)

| Take | Route, FC | Where (peeks) | Tasks | loaded by T | loaded by U |
|---|---|---|---|---|---|
| c901 | `combat` 901 | mode=5 step=5 kind=1 area=29 | 0 site 1, 1 free, 2 free, 3 free | refused: "other settings (on)", save offered | refused: "other settings (on)", save offered |

n5preset (taken by T)

| Take | Route, FC | Where (peeks) | Tasks | loaded by Ul | loaded by Tl |
|---|---|---|---|---|---|
| c901 | `combat` 901 | mode=5 step=5 kind=1 area=29 | 0 site 1, 1 free, 2 free, 3 free | portable, equal 489 | exact, equal 489 |

n5off (taken by T)

| Take | Route, FC | Where (peeks) | Tasks | loaded by T | loaded by U |
|---|---|---|---|---|---|
| c901 | `combat` 901 | mode=5 step=5 kind=1 area=29 | 0 site 1, 1 free, 2 free, 3 free | refused: "other settings (off)", save offered | refused: "other settings (off)", save offered |

What each shows:

- **Wide mode** (taken at 4:3, loaded at 16:9; taken and loaded at 16:9;
  taken at 16:9, loaded at 4:3). Taken and loaded at one width: **equal**,
  portable in U as well (the rim's extra layer rebased, B1's version 4).
  Across widths the hash differs by design - a wider picture writes more
  draw state - so the comparison is against a run played at the loading
  width throughout (the 16:9 control's live run, and the 4:3 take's live
  run). Against it, five pages differ on the first one or two ticks only
  (the first frame drawn from the take's width) and three to the end
  (`0x937000`..`0x939000`; `0x801000` `DrawTable` in the field). **The word
  is `MapView_CellItems`**: raw dumps of the 4:3 take loaded at 16:9 by U
  (tick 100) and the 16:9 run (tick 1000) differ, among the words the hash
  hashes, in the map view's cell-to-draw-item numbers alone - the rim filled
  afresh at the load hands out the same items in another order. Draw state,
  not game state: no game word differs. This agrees with
  [`wide-ring.md`](wide-ring.md) 4.5 ("picture pages only"), now at the word.
- **Display scale** (taken at 2, loaded at 3): three pages to the end, the
  three that hold the scale's words (`Gfx_ScreenRect` on `0x66B000`,
  `D3d_ScaleX` / `Y` on `0x7C9000`, `Display_BackPitch` on `0x7DE000`): the
  loading process keeps its own scale, as A1 read (the graphics participant
  keeps those words). Against a run played at 3 throughout (`n5scale3`, taken and loaded at 3), the load at 3 of the take at 2 by U is **equal, 489 and 59 ticks**: nothing else of scale 2 is left.
- **Cheats** (EXP x10, zenny x10, enemy info: the log names them on). On
  `whelpBoss` at 1000 nothing in the window gives EXP or zenny, so the
  load under the cheats is equal to the load without, tick for tick: the
  cheats leave nothing in the span to disagree with. On `bossAndFlash` at 4950 (the boss fight's end) the same take loaded **without** the cheats is equal (384 ticks); **with** them it differs from the second tick on the party's page (`Party_Zenny`'s, `0x904000`) and from tick 43 on the result screen's drawing: the cheats act from the load on, as after a save made without them, by design.
- **Faster text** (`BOF3X_TEXT_SPEED=1` at the load, a scene's message
  typing at the take): equal for 91 ticks, then the message window's pages
  (`WindowRecords`, `Text_LineX`, `Effect_Objects`) and the drawing differ
  on 57 of the remaining ticks: from the next line on the text prints at the
  faster pace, by design. Equal until the setting acts is what can be
  compared; the run plays to its end.
- **The picture** (`BOF3X_FILTER=point`, `BOF3X_BACKGROUND=0`,
  `BOF3X_PRESENT=satpixie`, `BOF3X_INTERP=1` at the load): **equal** tick for
  tick, exact and portable.
- **A behaviour switch** (`BOF3X_ON=CampMasters` at the load) and **a
  preset** (one more switch in `BOF3X_OFF` at the load, `YesNoLayout`; a
  preset is an `off=` list): each refused before a byte is written, "other
  settings (on)" / "other settings (off)", the save offered. The first try at
  the preset - the loading build started through its launcher with no ini -
  was **accepted** (portable and exact, equal 489): the harness sets
  `BOF3X_OFF` itself and the launcher keeps a variable already set, so both
  sides carried the same `off=` line (`n5preset`).

### Row 6: nothing else moved

The ten state-hash routes on T, no quicksave taken, `orig6`'s conditions,
the state hash's own skip list, against this laptop's `orig6` pair
(`statehash.py check cn_R_lx6.sh cn_R_lx6b.sh`; the census probe on, which
only reads):

| Route | Ticks | Result |
|---|--:|---|
| `combat` | 2,602 | identical |
| `caughFish` | 3,881 | identical |
| `cutsceneAndNue` | 7,396 | identical |
| `dragonTransform` | 4,322 | identical |
| `field_menu` | 1,409 | identical |
| `masterAndManillo` | 5,505 | identical |
| `menu_screens` | 1,729 | identical |
| `shop` | 3,137 | identical but one page (`0x7CA000`) at one tick, 2,756, inside its jingle window (2,712..2,756, DIV-0201's timing, where the pair itself disagrees: [`state-hash.md`](state-hash.md) 6); P's run shows the same |
| `whelpBoss` | 13,163 | identical |
| `worldMapAndAreaTransition` | 2,113 | identical |

U at `97bd7b2f` (U0) on `combat` and `worldMapAndAreaTransition`: identical
on all 2,602 and 2,113 ticks - the `-O1` build computes the same game.

**The self-test** on T (`BOF3X_SELFTEST_ONLY=1`, offscreen): exit 0, 10,424
log lines, every participant's and the snapshot's `self-test passed`. The
hosted shadow suite is the 32-bit hosted build's: owed on the desktop.

### Row 7: format 1

No format-1 quicksave was on this machine outside the round's own runs (a
search of `~/bof3x-runs/` and the main checkout's `game/`). F1, a build from
before the round, wrote two:

n7 (taken by F1)

| Take | Route, FC | Where (peeks) | Tasks | loaded by F1 | loaded by T |
|---|---|---|---|---|---|
| c901 | `combat` 901 | mode=5 step=5 kind=1 area=29 | - (**not portable**) | exact, equal 489 | refused: "another build", save offered |
| s100 | `shop` 100 | mode=0 step=0 kind=0 area=0 | - (**not portable**) | exact, equal 477 | refused: "another build", save offered |

T refuses each as "another build - format 1 ... the save half is still a
save (the save half is whole: it can be loaded as a save)", never restoring
it; tier 0's road loads its save half in T where the title's own load puts
it (`t0/f1T`: the seven words and the picture equal).

## 5. Every site, by its takes

| Site | Name | Portable loads in another build | Equal | Takes (route FC, taken by -> loaded by) |
|--:|---|--:|--:|---|
| 1 | `Field_Task` | 34 | 25 | `bossAndFlash` 1753 T->U, `bossAndFlash` 3000 T->U, `bossAndFlash` 5050 T->U, `bossAndFlash` 900 T->U, `campingFishing` 1000 T->U, `campingFishing` 2590 T->U (*), `caughFish` 1500 T->U (*), `caughFish` 3600 T->U (*), and 26 more |
| 2 | `TitleTask_Run` | 4 | 4 | `shop` 180 T->U, `shop` 180 U->T, `shop` 290 T->U, `shop` 290 U->T |
| 3 | `Title_Task` | 2 | 2 | `shop` 100 T->U, `shop` 100 U->T |
| 4 | `Transition_FadeSub` | 24 | 21 | `campingFishing` 2590 T->U (*), `cutsceneAndNue` 1020 T->U, `fairyVillage` 3375 T->U, `shop` 2638 T->U (*), `shop` 2638 U->T (*), `shop` 290 T->U, `shop` 290 U->T, `shop` 360 P->T, and 16 more |
| 5 | `Transition_FadeAdd` | 2 | 2 | `whelpBoss` 11680 T->U, `whelpBoss` 11680 U->T |
| 6 | `SwingOut` | 3 | 2 | `campingFishing` 560 T->U, `fairyVillage` 605 T->U, `fairyVillage` 605 U->T (*) |
| 7 | `SwingIn (opening)` | 7 | 6 | `campingFishing` 370 T->U, `fairyVillage` 297 P->T, `fairyVillage` 297 T->P, `fairyVillage` 297 T->U, `fairyVillage` 297 U->T (*), `fairyVillage` 297 V->P, `fairyVillage` 297 V->T |
| 8 | `SwingIn (turn)` | 3 | 1 | `fairyVillage` 4640 T->U, `fairyVillage` 4640 U->T (*), `masterAndManillo` 3320 T->U (*) |
| 9 | `Field_WaitTransition (GameMode_Enter)` | 12 | 9 | `campingFishing` 370 T->U, `fairyVillage` 297 P->T, `fairyVillage` 297 T->P, `fairyVillage` 297 T->U, `fairyVillage` 297 U->T (*), `fairyVillage` 297 V->P, `fairyVillage` 297 V->T, `fairyVillage` 4640 T->U, and 4 more |
| 10 | `Field_WaitTransition (to the menu)` | 4 | 4 | `worldMapAndAreaTransition` 634 T->U, `worldMapAndAreaTransition` 634 U->T, `worldMapAndAreaTransition` 637 T->U, `worldMapAndAreaTransition` 637 U->T |
| 11 | `Field_WaitTransition (the area change)` | 11 | 10 | `campingFishing` 560 T->U, `cutsceneAndNue` 1020 T->U, `fairyVillage` 3375 T->U, `fairyVillage` 605 T->U, `fairyVillage` 605 U->T (*), `shop` 360 P->T, `shop` 360 T->P, `shop` 360 T->U, and 3 more |
| 12 | `Field_WaitTransition (GameMode3_Leave)` | 2 | 2 | `worldMapAndAreaTransition` 681 T->U, `worldMapAndAreaTransition` 681 U->T |
| 13 | `Menu_WaitTransition (GameMode3_Enter)` | 2 | 2 | `worldMapAndAreaTransition` 640 T->U, `worldMapAndAreaTransition` 640 U->T |
| 14 | `Menu_WaitTransition (GameMode3_Leave)` | 2 | 2 | `worldMapAndAreaTransition` 675 T->U, `worldMapAndAreaTransition` 675 U->T |
| 15 | `GameMode3_Leave (its sleep)` | 2 | 2 | `worldMapAndAreaTransition` 679 T->U, `worldMapAndAreaTransition` 679 U->T |

(*) a difference section 4 explains: the shop's jingle (`s2638`, `s2500`),
the camp's fade (`cp2590`), the `Effect_Objects` copies the hash reads raw
(`mm1600`, `mm3320`, `cf1500`, `cf3600`), and B1b's copy rule renaming a
number (`f297`, `f605`, `f4640` from U to T: finding 2). The counts are of
loads by another build without forcing (T to U, U to T, P to T, T to P, V to P
and T); the forced loads in one build (row 1) are in the row tables.

## 6. Findings

1. **Portable loads across builds restore the game.** T to U, U to T, P to
   T, T to P, V to P and to T, O to A, at every one of the fifteen sites
   (section 5): every portable load compared equal from the restore to the
   end of its window, or differed only where section 4 shows why - the
   route's own music-timed noise, which an exact load meets too; the state
   hash's raw reading of a pointer of ours re-aimed correctly - **with one
   exception, finding 2**. No resume entry and no participant left a
   difference.
2. **B1b's copy rule renames a number** (row 2, `f297`, `f605`, `f4640`).
   `NameWord` (`snapshot.cpp`) writes any word of `.data` whose value lies
   inside a text slot's string as a copy of or cursor into that slot
   (`kWordTextCopy`). In U's layout a word of area data (`0x8262EC`, holding
   `0x1149015B` in every run of both builds) lies inside `g_lines`, so U's take
   names it and T's load writes T's address of the same character into the
   game's data: **a wrong restore**, four bytes, a difference from the first
   tick to the end. The real copies B1b measured are in `Effect_Objects`
   (and the battle block's text word, `kTextWords[3]`, which B1b lets hold
   one); a number elsewhere can fall inside a string in any build's layout
   (this route meets it on every take in the faerie village under U's
   layout, and on none under T's). **The fix described, not made**
   (`snapshot.cpp` and its self-test are B1b's): name a copy only in the words
   where one was measured - `Effect_Objects` (`0x7E11E0`, 20 records of
   `0x80`, `records.h:836`) and `kTextWords` - and write any other word raw,
   logged at the take as the pool rule's misses already are; the self-test's
   copy cases move to those addresses. The cost is B1b's own open case (a
   copy of a text pointer stored elsewhere would be written raw); the gain is
   that no number is ever rewritten. Proof wanted after the fix: this row's
   three `fairyVillage` takes U to T equal, and `mm1600`, `cf1500` still
   re-aimed (T to U and back).

   **Corrected (B1c, 2026-11-25; `sbn/B1` `f8bb996f`, `fab53dad`;
   [`state-by-name-participants.md`](state-by-name-participants.md) 10.7,
   10.8).** A word is now renamed only by where it is. The copy rule is
   tried at the four `kTextWords` (against every slot's text) and at
   `+0x50` / `+0x54` of a kind-0xF `Effect_Objects` record (the fishing
   lines' typing and scrolling cursors, `effect_kind_0f.cpp`), against the
   thirteen line records of `0x653B98` only. Any other word, `0x8262EC`
   among them, is written as it is. A file of a B1b build that named a copy
   anywhere else is refused, "another build (a word this build cannot
   place)", with its save offered: this row's `f297` file by U, loaded by
   the fixed build, is refused so. The proof's measurements of the fix are
   in the participants doc, 10.8.
3. **The state hash reads pointers of ours raw** (section 2): `Overlay_Text`
   after any quicksave key, B1b's `kTextWords`, the `Effect_Objects` copies.
   Between builds they are differences that are not the game's. The proof
   skips the fixed ones on both sides; the hash should fold or skip them
   (section 2, for the hash's owner).
4. **Builds from before B1 crash on a file from after it** (row 3): `GFX`
   checked its version in `Restore`, after the span is written. No release
   meets it.
5. **Two routes have transitions that land a frame apart after any restore,
   or between plain runs**: the shop's jingle (DIV-0201, timed by the music
   stream: every restore, exact included, moves it) and the camp's fade on
   `campingFishing` (two plain runs of one build disagree there). A cross
   comparison over those windows reads them as noise, as `check` does.
6. **The `-O0` build does not link** (section 1):
   `effect_gte::IdentityCheck()` is called from
   `EffectEffectGteFromEffectGte_Inject` (`src/game/effect/effect_gte.cpp:225`)
   and defined nowhere. Not this group's file.
7. **Optimisation does not change the yield chains' frames** (section 1): a
   second optimisation level is another build by its addresses and layout
   only. A test of other frames is the desktop's width row.
8. **Wide mode across widths leaves the map view's draw-item order as the
   load made it** (row 5): draw state, no game word.
9. **A release that binds one more of Capcom's functions refuses every
   portable quicksave of the release before it** ("other functions bound",
   section 1). For releases to exchange quicksaves, a release must keep its
   predecessor's bound set, or the format must learn to place a code word the
   writer left unbound.
10. **Premises that did not hold:** "a file from T refused by O" (O crashes,
    finding 4); "frames differ" between optimisation levels (finding 7);
    B1b's "no false positive" for the copy rule (finding 2: true of its two
    builds' layouts, not of another's); the launcher's preset differing from
    a `--direct` run under the harness (the harness sets `BOF3X_OFF`, the
    launcher keeps it: row 5); sites 10 and 13 on a recipe that can be cut
    other than `worldMapAndAreaTransition`.

## 7. The tool

`tools/quicksave_check.py`:

- **`matrix TAKES --take LABEL=PATH --load LABEL=PATH ... --game G --out DIR`**:
  the takes file is `NAME ROUTE FC [keep=N] [lang=TAG|none] [# why]`. A build
  is an ELF (`--host linux --direct`; from Windows a path in the WSL), an
  ELF with `@launcher` (started by the launcher beside it, `--no-config`), or
  a Windows launcher copy's `.exe` (`--host exe`). A load label ending in `!`
  is forced portable (`BOF3X_QSPORTABLE=1`). `--take-env` / `--load-env`
  cross a setting; `--env` sets both. `--lock-file` (an flock, POSIX) or
  `--lock DIR` (the Windows runs' mkdir lock) is held for each game run and
  the copy of its log. `--resume` keeps the results file and skips the rows
  it has (a stopped matrix starts where it stopped). `--hash-skip-only`
  has no effect since 2026-11-26 (the matrix hashes under the state hash's
  list alone, section 2). **The results file** is
  tab-separated with a header: the take, the route, FC, the taking and
  loading builds, forced, the file's FC, format, portability and why not,
  the `TASKS` record, the settings' FNV-1a, the bound functions' count and
  FNV-1a, the peeks, the answer the loading build logged (`EXACT`,
  `PORTABLE`, `REFUSED`, `FATAL`) and its reason, the comparison's result,
  its ticks, the first differing tick and the pages.
- **`qsinfo FILE...`**: a quicksave's header, format-2 extension, `TASKS`
  record and settings lines.
- **`tier0 ... --take-elf E1 --load-elf E2 --no-doctor`** (A4's command,
  kept): the take by one build, the load and the reference by another, the
  stamp left as taken so the refusal is the loading build's own.
- `inproc`, `cross`, `same`, `run`, `make` and `tier0` without the new
  options behave as before; `selftest` gains the take forms, the reader and
  the log's answer.

## 8. The desktop's rows

The width row (32-bit Windows against 64-bit Windows) and the system row
(Windows against Linux) need builds this laptop cannot make. They are the
owner's desktop's, with the round's files as they stand at its end (after
B1b's merge). Everything below runs on the desktop alone: the Linux ELF runs
there in the WSL, as `input_run.py --host linux` already does from Windows, so
no file has to be carried between machines. Carrying one is described after,
for the case where the WSL is not set up.

### 8.1 What must match for a file to be accepted

A portable load is refused, its save offered, unless all of these hold
(`snapshot.cpp` `Parse`; [`state-by-name-format.md`](state-by-name-format.md) 5):

| What | Where it is in the file | How to see it before a load | On the desktop |
|---|---|---|---|
| format 2 and the state revision (1) | `HeaderExt.state_revision` | `quicksave_check.py qsinfo` | the same source: equal |
| the settings' fingerprint: nine lines, `lang off original on solo opt psp desert-faerie layering` | the settings' lines and their FNV-1a | `qsinfo` prints the lines | **equal under the harness**: `input_run.py` sets `BOF3X_OFF` itself (New Game's two screens, keeping a caller's names), and a launcher keeps a variable already set, so a launcher copy with no `bof3x.ini` and a `--direct` run carry the same `off=` line (measured on the laptop: T's `--direct` take loaded by U and by T through their launchers, portable and exact, equal 489 ticks). A launcher copy **with** an ini whose preset or `off=` differs is refused, "other settings (off)". `psp=` is resolved from the install's cache (`base_policy`), so every build reads one install |
| the bound functions: the count and the FNV-1a of the VAs this build binds | `bind_rows`, `bind_fnv` | `qsinfo`: `N bound VAs (fnv X)` | **not known to be equal across widths and systems**: compare the three files' line first. A difference is the refusal "another build (other functions bound)", and is a finding (8.4) |
| the image's base | `image_base` | `qsinfo` | `0x00400000` in every released build; a moved base is refused as "another span" |
| the span: its base and size, and the pool (`DIV-0062`) | the header | the log's `F6 at` line | the exe host at both widths: `span 0x005DA000+0x3AD000, pool 0+0` on Linux x86-64; the 32-bit exe host is measured by the first take |
| every record a participant of the loading build reads, at its version (`GFX` 4, `SOUND` 1, `BEHAVIOURS` 4, `SOLOCB` 3, `NEWGAME` 1, `ST` 1) | the records | the log's refusal names the record | the same source: equal |
| every task at a site this build lists, every word placeable | `TASKS`, `WORDS` | `qsinfo` | the same source: equal |
| the host: the exe host (`bof3x.exe`), not the hosted `BOF3.exe` | `host` | the log | the hosted build's takes are never portable, by design |

What does **not** have to match, by design: the build stamp (the
module's SHA-256 and tag, which is what makes the load portable rather than
exact), the module's base, the task arena's top and size (`0x4000` slices at
32 bits, `0x8000` at 64), the code places' top byte, the pointer width.

### 8.2 The commands

Builds, from the round's tip in one worktree on the desktop
(`D:\bof3x-runs\sbn\wt`):

```
cmake --preset i686   && cmake --build build -j 8
cmake --preset x86_64 && cmake --build build-x86_64 -j 8
wsl -e sh -c "cmake --preset linux-x86_64 && cmake --build build-linux-x86_64 -j 8"   (or the Windows-hosted cross build, BOF3X_LINUX_SYSROOT)
```

Three ini-less launcher copies, so that each starts the game with the
launcher's defaults and the same preset (the `q_rc` pattern of
[`quicksave-routes.md`](quicksave-routes.md) 5): `D:\bof3x-runs\sbn\w32`
(`build\bof3x-launcher.exe`, `bof3x.dll`, `bof3x.exe`), `D:\bof3x-runs\sbn\w64`
(the same from `build-x86_64`), and the Linux build's own folder in the WSL,
its `bof3x-launcher` beside `bof3x` (the `@launcher` form below). No
`bof3x.ini` in any of them.

The install: the `orig6` one (`D:\bof3x-runs\orig6\home\game`,
[`state-hash-clean-install.md`](state-hash-clean-install.md)), with
`BOF3.CFG` exactly `0\r\n1\r\n` and no slots but slot 0, so that the
Windows runs and the Linux runs read the same inputs
([`state-hash.md`](state-hash.md) 6). The Linux runs make their own run
folder from it.

The takes: `tools/recipes/state_by_name_takes.txt` (section 4's list, one
take a site and the contexts). Then, for the width row, each Windows build
takes and the other loads, both ways:

```
python tools/quicksave_check.py matrix tools/recipes/state_by_name_takes.txt ^
  --take W32=D:\bof3x-runs\sbn\w32\bof3x-launcher.exe ^
  --load W64=D:\bof3x-runs\sbn\w64\bof3x-launcher.exe --load "W32!=D:\bof3x-runs\sbn\w32\bof3x-launcher.exe" ^
  --game D:\bof3x-runs\orig6\home\game --out D:\bof3x-runs\sbn\width32 ^
  --lock C:\Users\kerok\q_lock --env BOF3X_SCALE=2 --resume
python tools/quicksave_check.py matrix tools/recipes/state_by_name_takes.txt ^
  --take W64=D:\bof3x-runs\sbn\w64\bof3x-launcher.exe ^
  --load W32=D:\bof3x-runs\sbn\w32\bof3x-launcher.exe ^
  --game D:\bof3x-runs\orig6\home\game --out D:\bof3x-runs\sbn\width64 ^
  --lock C:\Users\kerok\q_lock --env BOF3X_SCALE=2 --resume
```

and for the system row, the 64-bit Windows build against the Linux build in
the WSL, both ways (the Linux path is inside the WSL; `@launcher` starts it
through its own launcher, as the Windows copies start theirs):

```
python tools/quicksave_check.py matrix tools/recipes/state_by_name_takes.txt ^
  --take W64=D:\bof3x-runs\sbn\w64\bof3x-launcher.exe ^
  --load L=/home/kerok/sbn/wt/build-linux-x86_64/bof3x@launcher ^
  --game D:\bof3x-runs\orig6\home\game --out D:\bof3x-runs\sbn\system64 ^
  --lock C:\Users\kerok\q_lock --env BOF3X_SCALE=2 --resume
python tools/quicksave_check.py matrix tools/recipes/state_by_name_takes.txt ^
  --take L=/home/kerok/sbn/wt/build-linux-x86_64/bof3x@launcher ^
  --load W64=D:\bof3x-runs\sbn\w64\bof3x-launcher.exe --load W32=D:\bof3x-runs\sbn\w32\bof3x-launcher.exe ^
  --game D:\bof3x-runs\orig6\home\game --out D:\bof3x-runs\sbn\systemL ^
  --lock C:\Users\kerok\q_lock --env BOF3X_SCALE=2 --resume
```

Each writes `results.tsv` in its `--out`: one row a take and a loading build,
with the file's portability and TASKS record, the answer the loading build
logged and the comparison. The first thing to read is the `settings_fnv` and
`bound` columns of the first take's rows: if they differ between the builds,
every load of that pair is refused before anything else, and the row's
result is that refusal (8.4).

**Not run here, so read as untested:** the matrix's Windows branch (a
launcher `.exe` as a build) and a Windows take loaded by the Linux build.
The code is in the tool (the quicksave is moved out of the Windows game
folder after the take and copied in for each Windows load; a Linux load of a
Windows take gets a run folder made from the install), and on the laptop
only the Linux branch ran.

### 8.3 Carrying a file between machines instead

If the WSL is not used: on one machine run the matrix with the taking
build alone and a `--load` of the same build (an exact load, as a check).
Carry from its `--out` the take's folder: `BISLPS10.DAT` (the quicksave: game
data, never into the repository) and `live.sh` (the live run's state hash).
On the other machine put `BISLPS10.DAT` in a game folder made from the same
kind of install, run the cross form `cross_NAME.txt` (beside it) with the
loading build (`input_run.py ... --no-save` and the same `--env` as the
take), then `quicksave_check.py cross live.sh loaded.sh`. The hash file
headers must agree (the same skip list: the matrix writes `proof_skip.txt`
into its `--out`; carry it and point `BOF3X_STATEHASH_SKIP` at it).

### 8.4 What a refusal on the desktop would mean

- **"other settings (off)"**: a launcher copy carried an ini (its preset
  or `off=`), or a run was started outside the harness. Not a finding;
  start them alike.
- **"another build (other functions bound)"**: the two builds bind other
  sets of Capcom's functions. Same source at two widths or on two systems
  can do that if a binding is under `#if` for one of them. That is a
  finding: it means two real releases of different widths or systems never
  exchange a quicksave. The fix is a design one (A1's: a code word whose VA
  one build leaves unbound), not a run's.
- **"another span"**: the span's size or the pool differs between the
  builds - the narrow image's premise broken at a width. A finding.
- **a record's refusal** ("another graphics record"): a participant's
  version differs - impossible from one source, so a stale build.

### 8.5 Owed on the desktop, from the groups' "not checked"

Gathered from the six hand-backs and the notes docs' last sections:

| What | Whose | Why the laptop cannot |
|---|---|---|
| The width row and the system row (8.2) | B3, the round doc 5 | no Windows build |
| The task bodies' fuzz: `mode_flow_fuzz.cpp` (the fades, the swings, `Field_Task`, `Field_WaitTransition`, `GameMode_Enter`), `shop_states2_fuzz.cpp` (`TitleTask_Run`), `mode_tasks`' (`GameMode_Field`), `mode_rest`'s and `mode_states`' (the mode-3 steps, the menu's wait); `Title_Task` has none | A2, B2 | built into the 32-bit hosted build alone |
| The hosted shadow suite (`BOF3X_SHADOW='*'`) on the round's tip | the round doc 5, B3 | the 32-bit hosted build |
| The 32-bit build's census (`BOF3X_TASKCENSUS`), its depths and that its places are the 64-bit build's | the round doc 9, A2, B2 | the 32-bit build |
| The Windows builds compiling at all, and the participants' `static_assert`s at 32 bits | B1, A2, B2 | no llvm-mingw here |
| The hosted build with Capcom's waits in the callee tables, and `BOF3X_ORIGINAL` naming `Task_Create` (a dropped slot's entry not cleared) | B2, A2 | the hosted build |
| The owner's eye on tier 0's question (F5) over the field and in a battle, and the road opened from F5 in a battle or a scene | A4 | the owner's |
| The music after tier 0's road (headless runs have a dummy audio device) | A4 | a run with sound |
| The ten state-hash routes on the 32-bit and 64-bit exe hosts at the round's tip, against `orig6` | B3 (row 6 at the widths) | the Windows builds |
| The F5 question: B1's `GFX` and `BEHAVIOURS` refusals were driven through tier 0, `ST`, `SOUND`, `SOLOCB` only in the self-test | B1 | could run here; not done (section 9) |

## 9. Not checked

- **The width row and the system row** (section 8): run on the desktop,
  section 10.
- **A build whose frames differ from T's on the yield chains.** `-O1`
  changed none (section 1) and `-O0` does not link. The portable restore
  reads nothing of the arena by design, so frames cannot matter to it; that
  is a reading, and the desktop's width row (`0x4000` slices against
  `0x8000`, another ABI) is the run that tests it.
- **The matrix's Windows branch** (a launcher `.exe` as a build, the
  quicksave moved in and out of a shared game folder) and a Linux load of a
  Windows take: run on the desktop, section 10.4.
- **Takes on frames no route reaches**: swing kinds 17 and 18,
  `GameMode3_Leave`'s camp and load branches, `Transition_Kind20`, scene
  scripts and battles that sleep (none seen). A take in `Transition_FadeAdd`
  ran on `whelpBoss` only (the one route that reaches it).
- **The `Effect_Objects` copies across builds compared word by word.** B1b's
  `masterAndManillo` take at 1600 holds them; its cross compares under a
  skip list that cannot list them (they are in allocated records), so a
  difference there is read from the pages it lands on (section 4).
- **A pointer of ours that no measured route stores** (B1b's open case):
  it would be written raw and restored as the other build's address. Every
  portable load here compared equal, so none was met on these takes; that
  does not prove there is none.
- **The cheats acting inside a window** only on `bossAndFlash`'s boss end;
  faerie speed, steal, no-fail and learn were not crossed.
- **Text speed and the picture across a portable load in a scene other
  than one**: one take each.
- **The owner's eye**: no picture of a cross-build load was looked at
  beyond the tier-0 pictures named in rows 3, 4 and 7.
- **`linux_tree.py --check` and `fnptr_census.py --check`**: left to CI, as
  the round's rules say.

## 10. The desktop's rows, as run (2026-11-25 and 26)

On the owner's desktop (Windows 11, llvm-mingw; the Linux build in the WSL,
Ubuntu, clang, Vulkan by llvmpipe). The worktree is `D:\bof3x-runs\sbn\wt`
at `sbn/round` `690bef78` plus the desktop's commits; every output is under
`D:\bof3x-runs\sbn`. The install is `orig6`
([`state-hash-clean-install.md`](state-hash-clean-install.md)).

### 10.1 What compiled and what the self-tests said

| Check | Result |
|---|---|
| The 32-bit build (`--preset i686`) | compiles, 1,438 steps |
| The 64-bit build (`--preset x86_64`) | compiles, 1,169 steps |
| The Linux build in the WSL (`linux-x86_64`, `-DSDL_X11_XTEST=OFF`) | compiles |
| Warnings in the round's files | three: `kDialogueGlyphs` unused in `title.cpp`; two unused functions in `sound_rest.cpp` |
| The exe host's self-tests at 32 and at 64 bits (`BOF3X_SELFTEST_ONLY=1`) | pass: the snapshot's, `GFX`'s, the save tail's. The span is `0x005DA000+0x3AD000`, pool `0+0`, at both widths - the Linux build's |
| The participants' `static_assert`s at 32 bits | hold (the build) |
| The hosted shadow suite, 32 bits, `BOF3X_SHADOW='*'` | exit 0 after one fix (below). The task bodies' fuzz: `mode_flow`, `mode_tasks`, `mode_states`, `mode_rest`, `shop_states2`, `save_menu`, 0 mismatches each |
| `ledger_check`, `ledger_switches`, `ledger_registry`, `region_table` (1,630,953 unknown; 1,630,937 after the three skipped ranges were classed), `tables`, `data_census --check` and `--strict`, `reloc_census`, `reloc_bare`, `width_census`, `tu_census`, `retire_shells`, `os_census`, `quicksave_check.py selftest` | pass |

**The one stop was not the round's.** The hosted suite stopped in the sound
module: `SndBuf_Play` differed from `0x5A6BB0` in 55 of 2,000 rounds. The
same on `main`'s build of 2026-11-24. D434's fix had started the data
pointer as the masked wave; the original's is its argument's own stack slot
(`lea eax, [esp+0x10]` at `0x5A6BFA`), which keeps bit 31 when
`Wave_FindData` finds no data chunk. Fixed in `fe203d57` (46,000 rounds, 0
mismatches). No caller of ours calls `SndBuf_Play`, so no player met it.

### 10.2 The width row and the system row

The 44 takes of `tools/recipes/state_by_name_takes.txt`; W32 and W64 are the
Windows exe hosts through ini-less launcher copies, L is the Linux ELF in
the WSL. `settings_fnv` (`114FA51B`) and the bound functions (10,088 VAs,
fnv `26C32EC0`) are **the same in the three builds**, so no load was refused
for the build's sake. The arena's slices differ (`0x4000` against `0x8000`)
and so does the ABI: the frames a portable load never reads.

| Takes by | Loaded by | Portable and equal | Differs | Refused, save offered |
|---|---|---|---|---|
| W32 | W64 | 35 | 7 | 2 |
| W32 | W32, forced portable | 38 | 4 | 2 |
| W64 | W32 | 35 | 7 | 2 |
| W64 | L | 35 | 7 | 2 |
| L | W64 | 34 | 8 | 2 |
| L | W32 | 34 | 8 | 2 |

Every load of the 42 portable takes was answered `PORTABLE`. The two
refusals are `mm3297` and `cp5100` in every row: "a task stands at no named
place (task 0)", the save offered - section 4's reading, the same here.

**The takes that differ**, and the control that reads them (W32 takes, W32
loads the file exactly, not forced: `ctl32`):

| Takes | Where it differs | The exact load in the same build | Reading |
|---|---|---|---|
| `s2638`, `s2500`, `cp2590` | k = 210 to 227 (`s2500`: 348), 7 to 24 pages | `s2638`, `cp2590`: **differ the same way** | Section 4's: the shop's jingle and the camp's fade, timed by the sound. Not the portable load's. `s2500` differs with L on either side only, as on the laptop |
| `s100`, `s180` (the title) | k = 64 and k = 29, 387 and 364 pages | **differ the same way** | **New on the desktop.** Equal on the laptop (section 4); here they differ whenever a Windows build is on either side, an exact load in one build included. So it is the run after a load at the title, not the portable restore. The cause is not established: not dumped |
| `mm1600`, `cf1500`, `cf3600` | k = 0, one page (`0x7E1000`, `Effect_Objects`) | **equal** | Between builds only. The take's log names it: "7 word(s) no rule names hold an address in the message pool, written raw". Section 4's class (`mm1600` there); `cf1500` and `cf3600` are two more takes of it. Read from the page and the log, not from a dump |

So: of the 42 portable takes, 34 or 35 load equal tick for tick across the
widths and across the systems, in both directions; the seven or eight that
differ, differ
under a load that is not portable too, or on the one page the hash reads
raw.

### 10.3 The code-place rule (the owner's call, 2026-11-26)

[`state-by-name-round.md`](state-by-name-round.md) 4b has the rule. What was
measured for it (`D:\bof3x-runs\sbn\codewords.py`, the `WORDS` record of the
88 takes of the two width rows against `rebase.gen.h`):

- The load-time table holds **8,784** code words. Every one is a code word
  in every take.
- A take holds at most **5** code words outside the table, and over the 88
  takes they are five addresses: `BattleHook_End` `0x904B64` (18 takes),
  `BattleHook_Exit` `0x904B68` (18), `BattleHook_Event` `0x904B6C` (16), and
  the first two enemy records' word at `+0xF4`, `0x93BA54` (16) and
  `0x93BB7C` (10).
- So the rule's places are the table, the three hooks, and `+0xF4` of each
  of the eight enemy records (`Battle_Enemies`, stride `0x128`).

After the change: the self-test's case (5b) passes at 32 and 64 bits on
Windows and on Linux; 16 takes (the battles, the boss fights, a shop, a
field, a scene) by W64 loaded by L and by W32, and by L loaded by W64: **48
loads portable and equal**. No take holds a word of the value kind
(`kWordCodeValue`). The three builds chose the same top byte, so the refusal
for another top byte ran in the self-test only.

### 10.4 The commands as they ran (8.2 corrected)

8.2's commands needed these on the desktop:

- `BOF3X_RECIPE_SAVES` set to the main checkout's `tools\recipe_saves` (a
  worktree has none), and `--env BOF3X_HOME=D:/bof3x-runs/orig6/home` (the
  install's cache).
- From Git Bash, `MSYS_NO_PATHCONV=1 MSYS2_ARG_CONV_EXCL="*"`, or the shell
  rewrites `/home/kerok/...` into a Windows path.
- The Linux build as the ELF itself, not `@launcher`: started from Windows,
  the Linux launcher returns and the game it started ends with it. The ELF
  alone needs `--env BOF3X_EXEIMAGE=<cache>` and `--env BOF3X_CACHE=<cache>`.
- A Linux load of a Windows take makes its run folder through the WSL
  helper (`ff0647c0`): `os.symlink` needs a privilege a Windows session does
  not hold.
- The exe host's self-test wants the install too: `BOF3X_EXEIMAGE`,
  `BOF3X_CACHE`, `BOF3X_LANG=en-US`.

The scripts are `D:\bof3x-runs\sbn\chain5.sh` (the system rows and the
control) and `chain6.sh` (10.3's rerun).

### 10.5 The reference pairs under 179 ranges

Recorded 2026-11-26 by `D:\bof3x-runs\sbn\rec7.py` (a run starts on a quiet
machine; one made beside a compiler is set aside - none was), on the round's
builds with the desktop's commits, under the 179-range list (`AFD29A9A`),
`BOF3X_OPT=none`, `BOF3X_LAYERING=0`. The ten routes of
[`state-hash.md`](state-hash.md) 6; 50 runs, each `rc 0` at the first try.
The files are `cn_<route>_<name>.sh` in `D:\bof3x-runs\sbn\pairs`, copied
to the main checkout's `analysis\statehash`.

| Name | What | Install |
|---|---|---|
| `orig7`, `orig7b` | Capcom's side (`--original "*,-Game_Clock"`), hosted, 32 bits | `<repo>\bof3`, as `orig5` |
| `c7` | ours, the exe host, 32 bits | the same |
| `o7w32`, `o7w64` | ours, the exe host at both widths | `orig6`, `BOF3X_SCALE=2`, as `o6w` |

- **`o7w32` against `o7w64`: identical on every route**, all compared ticks
  (1,409 to 13,185).
- **`o6w32` against `o7w32`: identical on every route** on every page but
  the three the added ranges touch. That is row 6 at the widths: the round
  and the desktop's commits moved nothing the ten routes hash.
- **`orig7` against `orig7b`: identical on six routes.** On `field_menu`,
  `shop`, `whelpBoss` and `worldMapAndAreaTransition` seven pages differ at
  tick 3 alone (five of `Gfx_VramShadow`, `0x7DE000`, `0x937000`). `check`
  leaves a page out where the references disagree, so the pair is usable;
  why one start in two differs at tick 3 is not read.
- **`c7` against the pair** shows the signature
  [`state-hash.md`](state-hash.md) 6 gives for `m1116` (`0x5FD000` every
  tick, the music pages, `0x803000`, the glyph switches' pages), and **one
  page more on every route: `0x92A000`** (`MapView_CornerPtr`,
  `Save_Staging`) from the route's load on. It is not this round's:
  `main`'s build of 2026-11-24 shows it the same on `field_menu`, and
  `SaveCameFrom` off does not remove it. A change on `main` since
  2026-11-16; which ledger entry it is, is not read, and no hosted control
  with the switches off was run.

Not recorded again: the laptop's `lx6` pairs (the laptop's), and a Linux
pair in the WSL.

### 10.6 Still owed

- **The cause of `s100` and `s180`** on Windows (10.2): two dumps of one
  build, live against loaded, at the first differing tick.
- **`0x92A000` against Capcom's side** (10.5): name the word and its
  ledger entry.
- The 32-bit build's task census (`BOF3X_TASKCENSUS`).
- `BOF3X_ORIGINAL` naming `Task_Create`, and the hosted build with Capcom's
  waits in the callee tables.
- The moved-base self-test (`BOF3X_IMAGEBASE=0xA00000`).
- The owner's eye on F5's question, and the music after tier 0's road.
- A portable load between two builds with different top bytes, in a game.

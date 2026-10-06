# The platform round, step 3: the C runtime's entries, the hidden-start scan, the last two game functions, and three scaling fixes

**Status:** IN PROGRESS (2026-10-06) - step 3 of
[`platform-layers-plan.md`](platform-layers-plan.md) section 4, run in one
day from `main` `e047ee9b` (PR #42) on `phase-3/platform-round-2`: four agent
groups merged one at a time (sections 1 and 5), three divergences the owner
decided the same morning (section 3), round fourteen's leftover debts
(section 4). **10,065 -> 10,081 ours** (the CRT's 14 entries and the two
game functions no catalogue held). Headless-verified per group and at each
merge; **the live check on the state hash is running** (section 6 - the
result goes there when it lands). Nothing pushed.

## 1. The groups

| Group | Branch, tip | Module / doc | What | Ours after |
|---|---|---|---|--:|
| SCAN | `phase-3/plat2-scan` `d976cace` | `tools/pe_jumptables.py`, [`hidden-start-scan.md`](hidden-start-scan.md) | the complete hidden-start scan `mode-rest.md` section 0 proposed: every indirect jump and call through a table (1,861 sites), every named `[[data]]` table (1,626), every `[[func]]` extent against the code reachable from it; the control (the thirteen mode functions dropped from a scratch copy) returns exactly the thirteen. **Two Capcom functions left in game code**: `Item_UseFlags` `0x591810` (18 direct callers, dropped by `pe_funcs.py` when its sweep decoded across the target) and `ItemTrade_Dispatch` `0x593950` (the trade screen's state dispatcher, hidden in `Sprite_ClutWord`'s extent). The rest of the uncatalogued code is the platform's: 84 software-renderer pixel converters (original-only), 5 decoder targets, ~64 runtime pieces. Why `pe_hidden.py` missed the thirteen: capstone's `disasm` stops silently at a byte it cannot decode, so the sweep fell out of step after an inline jump table; 29 extents (76,512 bytes) were truncated the same way. | 10,065 |
| DEBTS | `phase-3/plat2-debts` `17ecad3f` | [`round-14-cleanup.md`](round-14-cleanup.md) section 7 | 369 raw constants in 36 earlier-round files rebound to `bof3::addr::` names (values unchanged; a scratch check put each name back as its address, 0 lines differ); `kInflict` split into `kRollInflict` / `kInflictStatus`; `Sprite_FlashClut`'s `kEffectStd` row masked to the byte it reads; seven `entries_logic.txt` hosts cut to their extents with nine lines for the starts they covered; nine thin controls raised (`rest_4d` D33 4 -> 1,353, B12 7 -> 733, N47 5 -> 557, N07 7 -> 1,215; `rest_4a` C16 9 -> 348; `effect_1e` C58 3 -> 62, C61 2 -> 791; `field_e2` IT7 3 -> 67, DS9 1 -> 44). Left: `field_e2` DS6, `field_c3` C172, `rest_1g` C76 / C77 / C15 / C71 / D10. | 10,065 |
| CRT | `phase-3/plat2-crt` `0a6bbb30` | `crt_rest`, [`crt-rest.md`](crt-rest.md) | the C runtime's entries the game calls, worked through on the evidence: `Rand` ours and exact (the seed a static from 1, where `_initptd` `0x5BAD51` sets the CRT's; no `srand` in the binary; only the main thread calls it - the exe imports no thread-creating function and every trace runs on one thread; the `randlog` count folded in, its line unchanged; under `BOF3X_ORIGINAL` naming `Rand` the counting copy returns, never both at one address); `Crt_sprintf` ours over the 44 formats of its 188 sites (`%d`, `%Nd` to 8, `%0Nd`, `%X`, `%02X`, `%s`; `Fatal` on any other); `strncpy`, `_stricmp`, `_findfirst` / `_findnext` (the toolchain's `_finddata32_t` is MSVC 6's 0x118-byte layout, `static_assert`ed) and the file layer (`fopen`, `fclose`, `fread`, `fwrite`, `fseek`, `fileno`, `filelength`, `fgets`) bound to the toolchain's through thin named functions. **Left for the cutover:** `malloc` / `free` (the decoder's `Mp3_Destroy` frees what `Mp3_Create` got from the CRT's `calloc`), `Crt_GetPtd`, `Crt_atoi`, `Crt_sscanf`, `Crt_ftol` (the "12 sites" were all self-tests and fuzz tables; nothing of ours on a game path calls it). The A/B hazard: the eight file-layer names switch together or a stream crosses runtimes (`*` switches them all). | 10,079 |
| TWO | `phase-3/plat2-two` `7fed29a2` | `game_last`, [`game-last.md`](game-last.md) | the two SCAN found, taken: `Item_UseFlags` returns the whole eax the original leaves (category 2 and the default case carry id * 13 and id * 11 in the upper bytes); `ItemTrade_Dispatch` aborts at state 3 and above (the count from every write of `0x93985C`: 0, 1, +1 to 2, -1 back). 8,000 rounds, 0 mismatches, 15 plants refused; the harness rows keyed by their addresses to the `_OURS` form; six callee headers and `battle_sprites.cpp`'s `Raw<>` entry rebound. | 10,081 |

Launched from `f18b9d9c` (SCAN, DEBTS, CRT) and `cc7611ab` (TWO, after SCAN
merged, since it needed the two entries). Briefs:
`<session 7d0c9683 scratchpad>/plat2/common.md` + `brief_*.md`.

## 2. What the count says now

"10,065 ours, 0 left original" was a count of catalogued starts; SCAN's
complete scan puts the game's own code at **two left**, both taken by TWO the
same day. With the CRT's fourteen entries the tip reports `inject: 10081
ours, 0 left original`. What is still Capcom's in `BOF3.exe`: the runtime's
start-up, per-thread data, allocator, `atoi`, `sscanf` and `_ftol`
([`crt-rest.md`](crt-rest.md) section 3's left table); the MP3 decoder
([`platform-layers-plan.md`](platform-layers-plan.md) section 2.4); the
software renderer's converters and whatever only Capcom's draw path reaches
(original-only, gone at the cutover).

## 3. The owner's decisions and the three divergences

Decided the morning of 2026-10-06 from the calls the previous round
gathered ([`platform-round.md`](platform-round.md) section 4,
[`round-14-cleanup.md`](round-14-cleanup.md) section 5), each shown first
as a capture:

- **DIV-0077 - a TILE_1 covers the PlayStation pixel's footprint.** The
  handler drew one point; now a `D3d_ScaleX` x `D3d_ScaleY` quad. Shown on
  the `whelpBoss` route's frame 11880 (the dream scene's specks, not the
  fight: the trace's first TILE_1 is tick 11163). `BOF3X_TILE1=0` the point.
- **DIV-0078 - `BOF3.CFG`'s key lines cannot run off `Cfg_Load`'s frame.** A
  zero-filled 32-entry table of our own, the original's packing, lines past
  it ignored. `shell` self-test 21,000 rounds, 0 mismatches with it off.
- **DIV-0079 - LINE primitives are the PlayStation pixel's width.** The owner
  noticed the fishing gauge's thin bar; measured on `caughFish` frame 1680:
  LINE_F2s one screen pixel wide at a window scale of 3.3. The six handlers
  (F2, F3, F4, G2, G3, G4, in four files) call `d3d_lines.cpp`'s shared
  drawer: one quad per segment, square ends, smooth diagonals, armed after
  every module's self-test. The owner: "all elements should scale". Live:
  frame 1680 differs in 11,949 pixels, all in the gauge and the instruction
  banner's outline. `BOF3X_LINES=0` the strip.

Parked at the owner's word, documented: the `0x91` camp cells, the one-texel
FT3 colour. Measured, not changed: debt 3's packet-pool read now logs
`debt3` when `PartyAction_SpawnKind1B` stores 0xFF. The list of what still
wants the owner's eye or ear is [`owner-review.md`](owner-review.md), new
this round.

## 4. Round fourteen's debts

DEBTS' row in section 1; the record is
[`round-14-cleanup.md`](round-14-cleanup.md) section 7. Still open from the
round's end: the five thin controls DEBTS listed; the 61 run-time raw calls
(round thirteen's 1.3, the owner's decision, not asked this round).

## 5. The merges and the verification

Merge order SCAN `cc7611ab`, DEBTS `bea149d6`, CRT `3fab9a2e`, TWO
`e57c31c5`, with the coordinator's own commits between (DIV-0077 `f18b9d9c`,
DIV-0078 `9f38cf29`, DIV-0079 `ebea9279`). Conflicts by hand: README rows
kept both (CRT); `rest_4d_fuzz.cpp`'s `Crt_sprintf` row re-keyed on ours
beside DEBTS' louder rows (CRT); `battle_sprites.cpp` and
`menu_windows_callees.h` DEBTS' names with `Item_UseFlags` by name, and
`GameLast_Inject` placed before `CrtRest_Inject`, which stays last of the
takeovers (its fuzz wants every earlier module's done; TWO).

`'*'` with `BOF3X_LANG=original` (the trap below): at `bea149d6` narrow
1,056 `MISMATCHES` lines all 0, `inject: 10065 ours`; at `3fab9a2e` narrow
1,062 lines all 0, `inject: 10079 ours` (wide running as this was written);
at the final tip narrow and wide in the chain of section 6. Each group ran
`'*'` narrow and wide at its own tip (their docs). `ledger_check` 79
entries, 0 errors at every merge.

**A trap paid for today:** `'*'` in the main checkout's `build/` fails under
the owner's `bof3x.ini` (`language=en`): the launcher exports it as
`BOF3X_LANG`, the `ConfigText` patch re-aims the call at `Config_DrawRowLabel
+ 0x9F`, and `field_c1`'s clone check refuses the site (exit 3 after 978 of
the lines). An environment variable wins over the ini: `BOF3X_LANG=original`
on every self-test. The agents' worktrees never saw it.

## 6. The live check: the state hash

`<session 7d0c9683 scratchpad>/live_plat3.sh`, run by `chain_final.sh` after
the final tip's `'*'`: ours, Chinese, narrow, `BOF3X_LAYERING=0`, the
launcher a copy in the scratchpad (the owner's ini untouched), the attract
window in front (the trap of the previous round), against the reference
pairs of 2026-10-05 - the attract sequence and the ten routes, shop with
`--slot0-hold`; then **the save write** the CRT group asked for: `fwrite` is
the toolchain's now, so `balioAndSunder_2` (saves to slot 6 on purpose) runs
on ours and on Capcom's with the owner's slot 6 backed up and put back, and
the two written slots are compared byte for byte. The owner was away for
the run (2026-10-06 afternoon). **Result: pending.**

## 7. Next

- The live check's result into section 6; HANDOFF and STATUS forward; the PR.
- The owner's eye on [`owner-review.md`](owner-review.md)'s items.
- The platform plan's step 4 (the cutover's design) and the decoder question.

# The ninth round's queue: what the routes still enter, and the spells

**Status:** IN PROGRESS (2026-09-25) - the routes re-traced; EA, SH, HX and spell waves one to three merged: 2,845 ours; the frame hash to re-record again, the owner's eye owed

Round eight left "76 hidden entries the three routes still enter" as the
next queue ([`takeover-queue-round8.md`](takeover-queue-round8.md) "Owed
after the round"). The owner asked on 2026-09-25 evening for the routes to
be re-run and that queue taken, and for the spell round to start.

## 1. The routes re-traced (2026-09-25, 19:09..19:14)

`analysis/trace_hidden_recipes.sh` on a fresh build of `18772a2` (1,465
ours; the traced side all original less the English KEEP list, from a
scratch launcher copy, `--no-front`). The first-call lists are
`analysis/calltrace/hidden_{shop,worldmap,combat}/`; round eight's are kept
beside them in `analysis/calltrace/prev_r8/`.

| Route | First calls | Against round eight's trace |
|---|--:|---|
| shop | 26 | the same, less `0x5A6FF0` and `0x5A7080` (the set-up's, see below) |
| world map | 25 | identical |
| combat | 23 | identical; the run ended `done` where round eight's ended `game exited` |

**76 was a sum over routes. The distinct entries are 29**, and nearly none
of them is game logic:

| Entries | What they are | Taken? |
|---|---|---|
| `0x404180`, `0x4414E0`, `0x497C30`, `0x5916B0`, `0x5917D0` | switch cases inside hosts already ours (round eight's "Result") | nothing to take |
| `0x5A5BC0`, `0x5A5E40`, `0x5A5EA0`, `0x5A5F90`, `0x5A6230` (and shop's `0x5A6FF0`, `0x5A7080` in round eight's trace) | the DirectDraw / Direct3D enumeration callbacks under `Display_Setup` `0x5A5160`; their callers are system DLLs. Reached only because the traced side runs Capcom's set-up; ours (DIV-0031) never calls them | no - retired on our side |
| `0x5B08A0`..`0x5B1160` (14) | the statically linked MP3 decoder's pointer-reached starts | no - library code; replacing the decoder is its own decision (see §3) |
| `Task_RunAll` `0x5A98A0`, `0x5A98F0` | the task scheduler: hand-written stack switching, once per logic frame | **group EA**, merged |
| `0x576CD0`, `0x577B80` | switch cases of `MoveScript_Step` and `MoveScript_GroupF` (jump tables `0x576CF0`, `0x577B90`), hosts already ours; their "caller" was whatever dword sat at `esp` (EA read it). Reached only on an all-original side | nothing to take |

So after round eight **the three routes enter no game logic of Capcom's
except the task scheduler.** What is left to find by
route is what a new route reaches (HANDOFF item 4: `menu_screens.txt`, a
boss fight, an event battle).

## 2. The groups

| Group | Doc | Queue |
|---|---|---|
| EA - the task scheduler | [`task_sched.md`](task_sched.md) | **merged**: the hand-written unit `0x5A98A0`..`0x5A9A21`, eight functions (`Task_RunAll`, the landing `Task_BackToScheduler` `0x5A98F0`, `Task_SetStackBase`, `Task_Create`, `Task_Sleep`, `Task_Restart` `0x5A9976`, `Task_Exit`, `Task_ClearPrivate`); 33 controls, all refused (30 by a count, 3 by a fault) |
| SH - the spell harness | [`takeover-queue-round9-spells.md`](takeover-queue-round9-spells.md), [`magic_harness.md`](magic_harness.md), [`magic_steal.md`](magic_steal.md) | **merged**:  `tools/magic_rows.py` (each `Magic_Rows` row's overlay and its PC extent), a shared fuzz harness for one overlay, one small overlay taken end to end, a reading of engine rows 123 and 128, and the spell round's grouping. Taken: Steal (MAGIC216, row 87), three functions; 28 controls, all refused |
| L, S16..S25 - the first spell wave | [`takeover-queue-round9-spells.md`](takeover-queue-round9-spells.md) §6 | eleven groups, 528 functions: the effect library and MAGIC071..110 (healing, restoring, buffs, the elements). Out 2026-09-25 |

**The spell round's size**, measured before SH started: the 133 overlay
entries of `Magic_Rows` (`0x498FE0`..`0x4FC330`) sit among about 2,070
functions not yet ours, about 640 KB, 1,646 of them pointer-reached starts;
a median of 14 functions between consecutive overlay entries, 56 at most;
few byte-duplicates (1,881 distinct shapes with call targets masked). The
overlays are linked in file-id order (3 inversions in 138). That is more
functions than the 1,465 owned today, so the owner chose the harness first,
then a first wave of about ten groups, the rest in later sessions.

## 3. Left for a decision

- **The MP3 decoder.** Fourteen of its starts run on every route. It is a
  third-party library linked into Capcom's exe, not game logic; cloning it
  function by function buys nothing a modern decoder would not. Replacing
  it (the music path is `Mp3_MemoryIo`'s, HANDOFF Traps on `sscanf`) would
  be a divergence of the platform kind. The owner's call.

## 4. Owed by EA's merge

- **The frame hash's reference is void.** The scheduler's own functions are
  now owned and unarmed on both sides, so every frame's hash moves;
  `r8_orig` / `r8_origb` cannot be compared with a run on this build.
  Re-record the original-vs-original pair on the merged build (with HANDOFF
  item 2's content changes folded in), before 2026-09-27 or after a Restart.
- **The tracer's frame count** now reads `addr::Task_RunAll` and arms that
  one owned entry as the exception, and WinMain calls
  `bof3::orig::Task_RunAll()`. Not testable headless: the first traced run
  must log its armed entries with no `Fatal` and advance its frame numbers.
- **The live look**: boot, the title demo, entering a game
  (`Task_Restart`), area transitions, F9's pause.
- Latent defects for `known-defects.md`: Capcom's `Task_Create` does not
  check its slot (past 3 it writes over `0x66C850` / `0x66C854`); ours
  aborts ([`task_sched.md`](task_sched.md)); SH's engine rows 123 (plays a sound through an unchecked pointer) and 128 (waits with no limit), [`takeover-queue-round9-spells.md`](takeover-queue-round9-spells.md) §3.

## 5. Paused for the usage limit (2026-09-25 night)

The first spell wave was paused before its groups finished: S16..S25 were
told to commit their work in progress with a "Paused" section at the top of
their group doc (what is done, fuzzed, controlled, and the next step) and
stop. L, the effect library, was left running (the smallest group, and
every other group calls it).

To resume: `git worktree list` shows each group's worktree under
`.claude/worktrees/agent-*` on its branch `phase-3/round9-<group>`; read the
group doc's "Paused" section and carry on from there, or resume the agent
with its context. The brief every group was given is its first commit's
context: `docs/takeover-queue-round9-spells.md` §4 for the group's units,
[`magic_harness.md`](magic_harness.md) §3 for what a group writes. Merge one
branch at a time into `phase-3/round-nine`: the build, the group's shadow
and `BOF3X_SHADOW='*'` headless after each.

**Where it stood when the session stopped (2026-09-25, late):** every
group has its work committed on `phase-3/round9-<group>`. Ten of eleven
branches patched `magic_harness.h/.cpp` their own way, so group **HX**
(`phase-3/round9-hx`) was started to consolidate them into one API with a
porting table per group in `magic_harness.md`; it was paused almost at
once. Next, in order: finish HX and merge it; resume each group to port
onto it, run its controls and finish its doc; merge them one at a time.
S16 is furthest along (72 of 72 controls refused; its doc's controls table
to fill). The frame-hash re-record (§4) is due before 2026-09-27 or after a
Restart.

## 6. HX merged; the wave ported (2026-09-26)

HX's harness merged as `ea27991`: one API for the eleven groups' edits,
the per-group port in [`magic_harness.md`](magic_harness.md) §7, Steal's
counts and five of its controls unchanged in HX's worktree. The machine was
restarted overnight, which retires §4's 2026-09-27 deadline.

**A harness count depends on the build directory.** Steal's self-test gives
9,278 stand-in calls in HX's worktree and 9,850 in the main checkout, from
identical source and flags, each the same on every run, 0 mismatches in
both. The harness stores pointers to its own records (inside our DLL) into
game memory, and the DLL's load address differs by path, so the fuzz takes
slightly different branches. Both passes of a round see the same inputs,
so the comparison is sound; what does not carry between checkouts is a
documented call count. Merges are judged by 0 mismatches and controls
refused, not by matching a count.

**The owner on the divide-by-zero aborts (2026-09-26): no DIVERGENCE
entry.** Where Capcom's code divides by the number of live targets
(`Inferno_TargetCentre`, `Blizzard_CenterOnTargets`, `MagicFx_CenterOnSide`)
ours aborts with a message when that number is 0. The original faults at the
same point, so no reachable case behaves better in the original; the owner
expects normal play never casts at an empty side. Unmeasured: a trace of a
fight where the last enemy dies to a multi-target spell would settle it.

## 7. The first spell wave merged (2026-09-26)

All eleven groups merged into `phase-3/round-nine`, one at a time (L, S16,
S18, S21, S24, S17, S19, S22, S23, S25, S20): after each, the build, the
group's shadow and `BOF3X_SHADOW='*'` headless, all exit 0. **527 functions
taken, 1,476 -> 2,003 ours.** `ledger_check` clean after S16's sparkle
injects were written out (a token-pasting macro hid forty of them from its
regex). `entries_logic.txt` consolidated: 3,827 entries, no extent over the
next start (L's `0x4FC2D0` and S16's `0x4BCB70` host extents corrected by
hand).

| Group | Doc | Taken | Controls | Refused | Not refused |
|---|---|--:|--:|--:|---|
| L | [`magic_lib.md`](magic_lib.md) | 25 | 106 | 106 | |
| S16 | [`magic_s16.md`](magic_s16.md) | 60 | 72 | 72 | |
| S17 | [`magic_s17.md`](magic_s17.md) | 48 | 125 | 125 | |
| S18 | [`magic_s18.md`](magic_s18.md) | 42 | 121 | 121 | |
| S19 | [`magic_s19.md`](magic_s19.md) | 43 | 108 | 102 | 6: a re-read across a call that changes nothing the spell reads |
| S20 | [`magic_s20.md`](magic_s20.md) | 51 | 134 | 134 | |
| S21 | [`magic_s21.md`](magic_s21.md) | 48 | 152 | 150 | 2: equivalent (no call between the two reads) |
| S22 | [`magic_s22.md`](magic_s22.md) | 56 | 98 | 98 | |
| S23 | [`magic_s23.md`](magic_s23.md) | 51 | 70 | 69 | 1: equivalent (both arguments always 0) |
| S24 | [`magic_s24.md`](magic_s24.md) | 47 | 80 | 79 | 1: equivalent (both branches write the same bytes at the threshold) |
| S25 | [`magic_s25.md`](magic_s25.md) | 56 | 113 | 107 | 6: equivalent (steps of 0x10, multiples of 8) |

Every group is fuzz-only: no recorded route casts these spells. Their live
check is the owner casting them. The groups' latent defects are described in
their docs and not yet numbered in `known-defects.md`; the commonest kinds are
unbounded stack and `.data` dispatch tables, pool allocators whose "none
free" answer (`0xFF`) is never checked, and the divides by the live-target
count (the owner's call above).

## 8. The frame hash re-recorded (2026-09-26)

`analysis/validate_round9_hash.sh` on `0775a49` (2,003 ours, the other
session's recipe-saves commit on top of the wave), after the overnight
Restart, the entry list consolidated first (3,827 entries, no extent over
the next start). One start at 08:34 was abandoned two minutes in (the owner
was still at the keyboard; its log is `r9_hash_aborted.batchlog`); the
batch that counts ran 08:36..08:54 with the owner away.

| Check | Result |
|---|---|
| `r9_orig` vs `r9_origb`, all original (`*,-Game_Clock`, foreground held) | **identical on all 10,309 frames** |
| `r9_orig` vs `r9_ours` (2,002 ours injected, unfocused) | identical but frame 0, the set-up (438 calls against 286), as since `rb1` made `Display_Setup` ours |
| Oracle, orig vs ours | identical at every logged frame (8,997 from the alignment point): Rand count, message index, area word |

**`r9_orig` / `r9_origb` are the reference from here**; `r8_*` are history
(EA's scheduler takeover moved every frame's hash). The scheduler's tracer
change held: the traced runs armed their entries and counted 10,309 frames.
The spell wave's functions are not on the attract path, so the hash says
nothing about them beyond "nothing the attract sequence runs moved"; their
check is the fuzz and the owner casting them.

## 9. The second wave (2026-09-26, from `973a69d`)

Ten groups out at 12:30 (C1, C2, C3, E, S26..S31), briefed with the first
wave's lessons (`analysis/round9_wave2_brief.md`). Merged so far:

- **E** ([`magic_engine.md`](magic_engine.md)): 22 functions, the five
  engine rows; 102 controls, 100 refused, 2 equivalent. `0x43FE80`, which
  S23 and S25 call by address, is `MagicFx_DoneAndFree`. Paralyzer (row 123)
  reads an enemy's `+0xF8` unchecked, which only the event-battle set-ups
  write, so an ordinary battle reads address 0 - faithful in ours. Head
  Cracker's (row 128) waits end for ordinary targets; what freezes TCRF's
  case is unexplained (candidates in the doc). 2,025 ours.

**A harness blind spot E found, owed after the wave:** a `kFlag` stand-in
answers 0 exactly when its own disturbance did nothing (both come from one
hash), so a re-read after a "no" answer is never exercised. E worked around
it with an `effect` in its own fuzz. Every group whose functions re-read
after a `kFlag` callee may have the same gap. The fix belongs in
`magic_harness.cpp` (draw the answer and the disturbance from separate
bits), after wave two merges, then each group's controls re-run.

Merged since (each: the build, the group's shadow, `'*'`, `ledger_check`,
the entry list consolidated): **C3** ([`magic_c3.md`](magic_c3.md), 20;
row 119 is item magic - item index 32 - row 147 is ability 227, rows 2
and 7 unreachable on the PC), **S29** (49), **S31** (51), **S28** (42, with
`Port_DroppedCall` `0x4DF820`, the linker's one shared empty function),
**S27** (47; `0x4DA3B0` is `SpellFx_Countdown`, S23 now calls it by name),
**S26** (48), **S30** (60; `0x4E47F0` `MagicFx_CountDown9`, `0x4E5200`
`MagicFx_EndWhenChildrenDone`). 2,342 ours, C1 and C2 outstanding.

**Owed after wave two, besides the `kFlag` fix above:**

- **The frame hash again.** `Port_DroppedCall` runs on the attract and
  combat paths; owned now, the tracer arms it on neither side, so the
  2026-09-26 morning reference (`r9_orig` / `r9_origb`) no longer compares
  with this build. Re-record after the wave.
- **`tools/magic_rows.py` over-counts a jump table that another follows**
  (S26: `Magic114_DrawTriangle` `0x4D7D00`'s first table has 8 entries,
  `cmp ecx, 7`; the tool ran on into the second at `0x4D7FE4` and said 14,
  which the harness refuses with a Fatal). Bound the count by the `cmp`
  before the dispatch.
- **The first wave's raw-address calls into the second wave's functions**
  (`0x43FE80`, `0x4E47F0`, `0x4E5200`, ...) can now be rebound to names;
  they work as they are (the harness's stand-in falls back to the address).

**Wave two complete (2026-09-26 evening): all ten merged, 2,003 -> 2,468
ours (465 functions).** Then C2 ([`magic_c2.md`](magic_c2.md), 64: the four
enemy-only skills' PC overlays hold no damage, element or status code - they
draw, play sounds and set target flags; whatever TCRF saw comes from the
battle engine) and C1 ([`magic_c1.md`](magic_c1.md), 62: row 27 reads no
ability id and draws the same glyph cells for The World, Again, Death Bomb,
Roulette and the skills players meet; Pentagram draws nothing when the
target is on the actor's side). The usage cap cut C1 once; its committed
work resumed intact.

| Group | Taken | Controls | Refused | Not refused |
|---|--:|--:|--:|---|
| C1 | 62 | 214 | 211 | 2 equivalent; 1 caught by a fault, its variant by a count |
| C2 | 64 | 206 | 205 | 1 equivalent |
| C3 | 20 | 120 | 117 | 3 equivalent |
| E | 22 | 102 | 100 | 2 equivalent |
| S26 | 48 | 158 | 156 | 2 equivalent |
| S27 | 47 | 167 | 166 | 1 equivalent |
| S28 | 42 | 153 | 152 | 1 equivalent |
| S29 | 49 | 142 | 142 | |
| S30 | 60 | 169 | 169 | |
| S31 | 51 | 197 | 197 | |

Still owed, in order: the frame hash re-recorded (owner away); the `kFlag`
fix in the harness and each group's controls re-run; `magic_rows.py`'s jump
table bound; the raw-address calls rebound to names; `known-defects.md`
numbered for both waves. Then the third wave: S01..S15 and S32..S38, 22
groups.

## 10. Wave three (2026-09-26 evening, from `56b8c71`)

Merged so far: **S01** (26), **S06** (56), **S05** (29): 2,579 ours.

**`entries_logic.txt` was emptied and rebuilt once** (S05's script, 19:35;
[`magic_s05.md`](magic_s05.md) §9): S05 rebuilt it from the 19:32 snapshot
`entries_logic_0926_1932_raw.txt` plus the groups that wrote after it.
Checked after S05's merge: every start of the snapshot is present (4,332 of
4,332; 4,540 now). **34 owned functions have no line, and none had one before
the accident**: 9 are the wall-clock exclusions (`wallclock_reach.json`); 25
are not - the two window procedures, `Gfx_MoveImage` / `Gfx_MoveCells`, five
move-script ops, `Field_ObjectFollow`, three `AreaMap_*`, three
`D3d_Draw*`, the four `Sparkle_*` helpers, `Battle_MemberOutAction`, two
`ClutMap_*`, `Battle_InitBossEncounter`, `Battle_InitEnemies`. The morning's
hash matched with them absent, so they are covered by a host extent or not
on the attract path; audit them with the next re-record.

**`tools/magic_rows.py` misses stack-table handlers loaded through a
register** (S06: `0x4A2DC0`, `0x4A2F40` load three handlers into a register
before storing them; the generated clone would have called Capcom's real
handlers). A group's coverage line missing a handler is the sign. Fix with
the jump-table bound (§9).

Then **S02** (48), **S03** (44), **S08** (59), **S04** (56): 2,786 ours, S07
outstanding.

**A second harness gap, found by S03, S04 and S08 independently:** a
`kFlag` stand-in answers only al, leaving garbage above it; a caller that
tests the whole of eax (`MagicFx_NearSprite`, `_NearSprite3D`) then almost
never sees 0, so its "no" branch never runs. Each group re-listed those
callees as `kBool` in its own fuzz. Fix with the `kFlag` one: the standard
set should list a callee by what its callers test.

**The owner on Blitz's past-the-table step (2026-09-26): keep ours as it
is, no DIVERGENCE entry.** `BlitzBolt_Seek` can leave the bolt's step at 12,
one past `BlitzBolt_Steps` ([`magic_s03.md`](magic_s03.md) §8). On the PC
the overlays are linked in one exe, so Capcom's code then runs MAGIC013's
`SnapWave_Run` (Snap, read one id down), which indexes its own five-entry
table by the same 12; on the PlayStation, where each overlay loads alone,
the entry past the table is whatever follows it in Blitz's own file. So the
wild jump is likely an artifact of the port's linking. Ours aborts at the
bad step, the out-of-table precedent. Reachability is unmeasured.

**Wave three complete (2026-09-26 night): all eight merged, 2,468 -> 2,845
ours (377 functions).** Each merge: the build, the group's shadow, `'*'`,
`ledger_check`, the entry list consolidated.

| Group | Doc | Taken | Controls | Refused | Not refused |
|---|---|--:|--:|--:|---|
| S01 | [`magic_s01.md`](magic_s01.md) | 26 | 174 | 174 | |
| S02 | [`magic_s02.md`](magic_s02.md) | 48 | 231 | 230 | 1 equivalent |
| S03 | [`magic_s03.md`](magic_s03.md) | 44 | 219 | 217 | 2 equivalent |
| S04 | [`magic_s04.md`](magic_s04.md) | 56 | 307 | 307 | |
| S05 | [`magic_s05.md`](magic_s05.md) | 29 | 126 | 126 | |
| S06 | [`magic_s06.md`](magic_s06.md) | 56 | 203 | 202 | 1 equivalent |
| S07 | [`magic_s07.md`](magic_s07.md) | 59 | 274 | 273 | 1 equivalent |
| S08 | [`magic_s08.md`](magic_s08.md) | 59 | 221 | 221 | |

Left: waves four (S09..S15, 320) and five (S32..S38, 345), then the owed
list (§9, §10): the frame hash, the two `kFlag` gaps in the harness and the
controls re-run, `magic_rows.py`'s two misses, the raw-address rebinding,
`known-defects.md` for all three waves.

## 11. Wave four (2026-09-27, from `62b1e37`)

Seven groups, S09..S15 (MAGIC045..069), after the `kFlag` fix and its
controls re-run ([`magic_harness.md`](magic_harness.md) sections 4 and 8)
and `magic_rows.py`'s bounded tables; the brief was wave three's with the
workarounds removed. **320 functions, 3,165 ours.** Merged in address
order after S13 and S11 (which reported first); every merge built, passed
its own shadow and `'*'` headless, `ledger_check` 0 errors, and every
wave-four function has its `entries_logic.txt` line.

| Group | Units | Functions | Controls refused | Doc |
|---|---|--:|---|---|
| S09 | MAGIC045..048, 050 | 47 | 277 of 280; E21, D63 equivalent, D63b beyond the harness (a `kFlag` non-zero always has bit 4) | [`magic_s09.md`](magic_s09.md) |
| S10 | MAGIC052..056 | 60 | 356 of 359; S41, S46, S48 equivalent | [`magic_s10.md`](magic_s10.md) |
| S11 | MAGIC058, 059 | 35 | 226 of 226 | [`magic_s11.md`](magic_s11.md) |
| S12 | MAGIC060, 062 | 44 | 346 of 348; D16, S20 equivalent | [`magic_s12.md`](magic_s12.md) |
| S13 | MAGIC063 | 25 | 152 of 153; F2 equivalent | [`magic_s13.md`](magic_s13.md) |
| S14 | MAGIC064..066 | 57 | 300 of 300 | [`magic_s14.md`](magic_s14.md) |
| S15 | MAGIC067..069 | 52 | 251 of 254; F19, F30, I78 equivalent | [`magic_s15.md`](magic_s15.md) |

Every equivalent has a refused near variant. Six groups strengthened
their own fuzz after a first run left controls standing (S09, S10, S11,
S12, S13, S14 - each re-ran its whole set); none edited the harness. S12
and S14 were cut by the usage limit after their controls; S14 resumed for
its last three.

**Newly named shared functions**, still called by raw address elsewhere
(they work: the stand-in falls back to the address; rebinding is owed):
`MagicFx_UncountAndFree` `0x4AF490` (S11; held by S03, S04, S07, S09,
S10, S15, S26, S31), `MagicFx_CountDownRelease` `0x4B1740` and
`MagicFx_CountDown2Release` `0x4B18B0` (S12; C3, S03, S08, S15, S31),
`ChillRay_Grow` / `_Shrink` `0x4B6B20` / `0x4B6B70` (S15; S31),
`Item_CopyName` `0x4B58F0` (S14; the harness's standard list, Steal).

**Defects described, not fixed** (each group doc): the common set again
- unbounded dispatch tables, `BattleTask_Create`'s `0xFF` unchecked,
divides by a live count (ours aborts) - and: S09's full breath pool writes
record 255 at `0x6861BC` inside `.data` (silent); S12's Identify on a party
member reads an "enemy" record in the task-slot area and can mark an
arbitrary enemy kind identified; S15's `ChillRay_PushMatrix` turns by an
uninitialised stack word past facing 3; S13's burst can skip mote 0 and
never set its 0x10 flag; S10's `EbonfireRing_End` and S11's
`SanctuaryMote_Slow` are unreached.

## 12. Wave five (2026-09-27, from `fb10178`)

Seven groups out at 08:35 (S32..S38, 345 functions; brief
`analysis/round9_wave345_brief.md` re-pointed at `fb10178`).

**The frame hash, re-recorded first** (owed since wave two's S28 took
`Port_DroppedCall`): `analysis/validate_round9_hash.sh`, the reference
sides `--original "*,-Game_Clock"`, `renderer=1`, windowed, foreground
held; `ours` unfocused. The first attempt died in the tracer's own
ceiling - `calltrace.cpp`'s owned-function table was 2,048, raised from
256 on 2026-09-22, and 3,164 were owned - `Fatal("calltrace: more than 2048
owned functions")` before the window; now 8,192 (`ed0cd6f`). The second
attempt's `orig` side ran while the S32 merge was building and self-testing
on the same machine and made 9,675 logic frames in 360 s against ~10,300
for the other two, with four frames of 24,000..33,000 calls where both
other sides had 2 (kept as `r9_orig_0927_loaded`); re-recorded alone.
**Result, at 3,164 ours (`ed0cd6f`): `r9_orig` vs `r9_origb` identical on
all 10,317 frames; `r9_orig` vs `r9_ours` identical on all 10,279 frames
but frame 0, the set-up (as since `rb1`).** The oracle sampled in the same
runs disagrees on 1,473 frames orig-vs-ours and 544 orig-vs-origb - it was
sampled under `BOF3X_CALLTRACE_MODE=all`, where `attract_diff.py` is
documented unreliable (the Traps); the hash is the arbiter. The 2026-09-26
reference is kept as `r9_*_0926`. Do not build or self-test in the main
checkout while a reference side records: the load shows in the frames.

Merged so far (each: the build, the group's shadow, `'*'`, `ledger_check`
0 errors, the entry list consolidated):

| Group | Units | Taken | Controls refused | Doc |
|---|---|--:|---|---|
| S32 | MAGIC144, 150 | 36 | 155 of 159; W36, E54, E73, E84 equivalent | [`magic_s32.md`](magic_s32.md) |
| S33 | MAGIC151, 154 | 57 | 223 of 224; A54 equivalent | [`magic_s33.md`](magic_s33.md) |
| S34 | MAGIC158, 159, 161, 162, 166 | 47 | 265 of 267; E1, X9 equivalent | [`magic_s34.md`](magic_s34.md) |
| S36 | MAGIC172, 173, 218 | 45 | 254 of 255; M77 equivalent | [`magic_s36.md`](magic_s36.md) |
| S37 | MAGIC219, 220/221, 222 | 60 | 331 of 334 by a count; H4 by a fault (H4b by a count); G48, G51 equivalent | [`magic_s37.md`](magic_s37.md) |
| S35 | MAGIC167, 168, 169 | 46 | 228 of 228 (B18 by a hang, B18b by a count) | [`magic_s35.md`](magic_s35.md) |
| S38 | MAGIC223, 225, 226/227 | 54 | 359 of 361; T17, M98 equivalent | [`magic_s38.md`](magic_s38.md) |

**Wave five complete (2026-09-27 midday): all seven merged, 3,165 -> 3,510
ours (345 functions), and with it the spell round - every overlay behind
`Magic_Rows` is ours, 2,064 functions in 43 groups.** Every equivalent has
a refused near variant; four groups strengthened their own fuzz after a
first run left controls standing (S34's B27, S35's U57 with a seed change
that re-ran its whole set, S36's four, S38's T50 / V97); none edited the
harness. S33 and S34 found the queue's row pairing reversed for MAGIC151 /
154 (S33: 151 is row 86, 154 is row 35) and for MAGIC159 / 166 (S34: 159 is
row 120, 166 is row 90) - the queue's "read one id down" line, not the
code. After the wave: 3,510 `impl` entries, 34 without an
`entries_logic.txt` line - the same 34 as section 10, none of them a spell.

**Newly named shared functions**, still called by raw address elsewhere:
`MagicFx_FreeCurrentRecord` `0x4F6290` (S37; raw in C1, C2, S02, S07, S10,
S11, S17, S19, S21, S28, S29, S35, S38), `MagicFx_PushRecordMatrix`
`0x4F6020` (S37; S38), `0x4F7C40` (S37; S10), `0x4F7320` (S37; S09),
`MagicFx_CountDownFlag10` `0x4F9F70` (S38; C2, S10, S28, S30, S31, S37),
`MeteorStrikeRock_DrawRing` `0x4FA440` (S38; S10), `0x4FA390` (S38; S37),
`AuraBreath_InReach` `0x4F5970` (S36; S11's `kNearSprite`),
`BattleFx_ScriptToEnd` `0x4EE560` (S34; S02), `0x4EF840` (S35; S38). The
engine's `0x446770` (a direction turn), `0x4514A0`, `0x494060`,
`0x494110`, `0x4941B0` are called raw by several groups and belong to no
unit.

**Rebinding (2026-09-27, branch `phase-3/round9-rebind`).** Every raw
constant in `src/game/magic_*.cpp` / `magic_*_callees.h` that was *called*
(a `Call0` / `Phase` / `MH_AT`, or an entry of ours' mirror of a stack table
dispatched through `Phase`) and whose target now has an `impl` was rebound
to its name, 99 constants and 16 table literals in 38 files: this list and
section 11's, plus those the enumeration found beyond them - the engine's
`MagicFx_DoneAndFree` / `_FlagTargetEnd` / `_WaitOwnerAnim`, `BattleFx_Finish`
/ `_FreeTask`, S03's `Chlorine_WaitChildren`, S04's `KickImage_Tick` /
`AirRaidImage_FadeOut`, S05's `Magic017_Wait` / `Magic018Row53_Wait`, S06's
`Magic008_DrawFlash`, S07's `FocusMote_Rise` / `EnlightenRays_Fade`, S08's
`Berserk_WaitChildren`, S10's `SacrificeActor_Play`, S17's `Leech_WaitOrbs`,
S18's `MagicFx_DrawDiscRadius`, S19's `BarrierRing_Hold`, S24's
`MagicFx_EndWithChildren`, S26's `Magic114_End` / `Magic117_Brighten`, S28's
`Port_DroppedCall` / `BreathBeam_*`, S30's `MagicFx_EndWhenChildrenDone` /
`_CountDown9` / `_CountUp9By2`, S32's `WallOfFire*`, S35's
`LastResort_WaitChildren` / `BenedictionMote_*`, C1's `ActorFx_TintUp` /
`WhiteFlag_TintSource`, C2's `HolocaustBeam_Grow`, and ten of group L's. The
form: `bof3::addr::Name` inside the existing `Call0` / `Phase` / `MH_AT`
(the same value, so the fuzz's stand-in keys and the game's calls are
unchanged), and `MH_CALL(Name)` for group L's callees whose prototype fits
(`MagicFx_CenterOnSide`, `_StepToward`, `_StepAround`, `_NearSprite`,
`_NearSprite3D`, `_ApplyBuff`, `_BuffPopup`: the harness's standard list
maps ours to the same slot); `MagicFx_LinkByDepth`, `_StepTowardPoint`,
`_NearPoint3D` keep the caller's `MH_AT` type (the depths as `long *`, int
arguments) with the name as its address. **Left raw:** the engine helpers no
unit owns (`0x446770`, `0x4514A0`, `0x494060`, `0x494110`, `0x4941B0`, and
the unnamed `0x43EC10`, `0x435A70`, `0x435A20`, libgpu `0x5A....`); every
`.data` table read in place, the fuzz files' `kImms` / `CallSite` / stand-in
tables; the address constants in `magic_s23_callees.h`, `magic_s24_callees.h`
and `magic_s29_callees.h` that the fuzz lists by address (ours now calls by
name, the constants remain as those keys); S16's tables of its own
functions (in no other group); and `magic_fx_reached_callees.h`'s seven
(round eight's unit with its own `Callees` table, not a spell group).
Self-tests: every touched group's shadow and `'*'` 0 mismatches;
`ledger_check` 0 errors.

**Defects described, not fixed** (each group doc): the common set - every
dispatch table unchecked (ours aborts past one), `BattleTask_Create`'s
`0xFF` unchecked in every start function - and: S33's `Accession_LoadForm*`
fault on a null pointer for a `0x904B89` kind of 10, 19, 20 or above 25,
and path B copies the actor's record over party member 0; S34's
`TimedBlow_Start` indexes enemy records by the actor byte minus 3; S35's
`Benediction_Spawn` and `LastResort_Start` would write past the image end
on `0xFF`, `LastResortBeam_DrawSparks` never ends for a step below 1;
S36's spark loops end on a random walk and its ribbon's gap scan has no
limit; S37's `CombustionSprite_Fade` adds to +0x40 twice and never +0x44
(a copy slip), and the Combustion shake leaves `Camera_Angles[0]` at
`0xFD56`; S38's `Magic225_Spawn` plays one sound 16 times a frame, and a
second overlapping cast clears the first's pool records.

**Numbered (2026-09-27):** the round's latent defects - all five waves' and the scheduler's - are D89..D132 in [`known-defects.md`](known-defects.md), the common classes collapsed into one entry each, and each group doc's defects section points at its numbers.

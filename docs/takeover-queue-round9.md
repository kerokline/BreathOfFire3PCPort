# The ninth round's queue: what the routes still enter, and the spells

**Status:** IN PROGRESS (2026-09-25) - the routes re-traced; EA, SH, HX and the first spell wave (L, S16..S25) merged: 2,003 ours; the frame hash re-recorded and matching; the owner's eye owed

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

# The ninth round's queue: what the routes still enter, and the spells

**Status:** IN PROGRESS (2026-09-25) - the routes re-traced; EA and SH merged (1,476 ours); the first spell wave out (L, S16..S25)

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

# The ninth round's queue: what the routes still enter, and the spells

**Status:** IN PROGRESS (2026-09-25) - the routes re-traced; two groups out (EA the task scheduler, SH the spell harness)

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
| `Task_RunAll` `0x5A98A0`, `0x5A98F0` | the task scheduler: hand-written stack switching, once per logic frame | **group EA** |
| `0x576CD0`, `0x577B80` | first "called" from `0x7DEFA4` / `0x7DF0EC`, a task stack: task entry functions, not the "Not functions" the catalogue says | **group EA** |

So after round eight **the three routes enter no game logic of Capcom's
except the task scheduler and two task bodies.** What is left to find by
route is what a new route reaches (HANDOFF item 4: `menu_screens.txt`, a
boss fight, an event battle).

## 2. The groups

| Group | Doc | Queue |
|---|---|---|
| EA - the task scheduler | `task_sched.md` | `Task_RunAll` and the rest of its hand-written unit (`Task_SetStackBase`, `Task_Create`, `Task_Sleep`, `Task_Exit`, `Task_ClearPrivate`), `0x5A98F0`, the task bodies `0x576CD0` and `0x577B80` |
| SH - the spell harness | `takeover-queue-round9-spells.md` | `tools/magic_rows.py` (each `Magic_Rows` row's overlay and its PC extent), a shared fuzz harness for one overlay, one small overlay taken end to end, a reading of engine rows 123 and 128, and the spell round's grouping |

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

# State 2 proved: no code of Capcom's runs after the entry point's hand-off

**Status:** STABLE (2026-10-11, the tracer run of `platform-layers-plan.md`
section 4 step 5, on `platform/state2-prework` at `31e52cb5`, `inject: 10087
ours`). Game-derived files under `analysis/` (not committed): the traces
`analysis/calltrace/platform_1011/<run>.tsv` and `.log`, the table
`analysis/platform_reach_1011.tsv`, the summary `platform_1011/reach_summary.txt`.

## 1. The measurement

`tools/calltrace.py`'s first-call tracer (`BOF3X_CALLTRACE`, an `int3` at every
listed entry, the first hit recorded with its caller and logic frame) armed on
**550 entries: every start that is not ours** - the remaining catalogue's parts 0
and 1 (`analysis/remaining_catalog.tsv`: the C runtime 242, the MP3 decoder
200, the renderer's 46, the PSX library layer 17, the set-up 10, sound 8, the
shell 7) less the five "not functions" and less the starts that now have an
`impl`, plus the 84 hand-written pixel converters `0x5AA5D0..0x5ADF00`
([`hidden-start-scan.md`](hidden-start-scan.md) 4.2) and `Task_RunAll` for the
frame count. The list is the session scratchpad's `entries_1011.txt`; a line is
`hex-address hex-size name`. Ours ran the ten recorded routes of
[`state-hash.md`](state-hash.md) 6 and the attract sequence (`attract_cycle.txt`,
7 minutes), Chinese, narrow, `BOF3X_LAYERING=0`, `--no-front`, on the owner's
machine with two headless self-tests of other worktrees running beside it.
Then `tools/platform_reach.py --reach platform_1011/*.tsv --dll build/bof3x.dll`.

## 2. The result

| Layer (catalogue label) | starts | runs | held | listed | original | unreferenced |
|---|--:|--:|--:|--:|--:|--:|
| MSVC CRT | 242 | **36** | 181 | 0 | 19 | 6 |
| MP3 decoder (by range) | 200 | **1** | 134 | 3 | 28 | 34 |
| Renderer | 46 | 0 | 14 | 16 | 16 | 0 |
| PSX library layer | 17 | 0 | 17 | 0 | 0 | 0 |
| Platform set-up | 10 | 0 | 1 | 1 | 8 | 0 |
| Sound | 8 | 0 | 8 | 0 | 0 | 0 |
| Windows shell | 7 | 0 | 7 | 0 | 0 | 0 |
| Not functions | 5 | 0 | 0 | 0 | 5 | 0 |

(The 2026-10-05 run, `platform_reach_1005.tsv`: 198 ran. The 161 that no longer
run are the decoder's, taken by DIV-0091, the runtime entries of
[`crt-rest.md`](crt-rest.md), and the small layers of the platform round.)

**The 37 that run, and when:** every one of them was entered at **logic frame
0** on every route and in the attract run (`platform_1011/*.tsv`: 38 hits each,
`Task_RunAll` and 37 starts, none after frame 0). They are:

- **36 of the C runtime's start-up**, the chain [`crt-startup.md`](crt-startup.md)
  section 2 reads, from the entry point `0x5BA057` (its caller is `kernel32`)
  through `_mtinit` `0x5BACFD`, `_initptd` `0x5BAD51`, `__heap_select` /
  `_heap_init` `0x5BB297` / `0x5BB3DF`, `_cinit` `0x5BE74E`, `_initterm`
  `0x5BE854`, `_setargv` `0x5BEAF7`, `_setenvp` `0x5BEA3E`, `_ioinit` `0x5BEE76`,
  `__initmbctable` `0x5C17A6`, `_setmbcp` `0x5C13CE`, `_controlfp` `0x5BF4DA`, the
  locks and the pieces under them - all before `WinMain`.
- **`0x5ACBBC`, DirectSoundCreate's import thunk** (`jmp [0x5C4014]`, DSOUND
  ordinal 1), called once from our `Snd_Init` inside `Game_Init` at frame 0 -
  catalogued "MP3 decoder" by its address range only
  ([`sound-rest.md`](sound-rest.md)). An import, not code: state 3 imports
  `DSOUND.dll` itself.

So **state 2 holds**: after the entry point's hand-off to `WinMain` nothing of
Capcom's code runs on any route - what the 2026-10-05 plan asked the tracer to
show. What the executable still does for us is the runtime's start-up, which
state 3 reproduces as behaviour (the x87 word, the stack, `WinMain`'s arguments
and exit, the "C" locale: `crt-startup.md` 1 and 9), and the import table.

## 3. Also measured in the same runs

- **The stack under ours** (`BOF3X_STACKWATCH=1` on the combat route,
  `platform_1011/combat.log`): Windows gave the main thread `0x40000` bytes
  reserved, `0x10000` committed; `Task_SetStackBase` arrives with `esp`
  `0x294` below the base (ours; `crt-startup.md` 11 predicted `0x200..0x20C`
  plus the thread-start frames), `Task_StackTop` `0x4294` below it and the four
  slices down to `0x14294` below - inside the reservation. **The exit line did
  not come:** `input_run.py` ends the game without `DllMain`'s detach, so the
  deepest page a route touches is still owed to a run ended by a normal quit.
- **The attract run** stopped at its 7-minute cap one shot short of the
  recipe's end (`exit 1`, "timed out"); the trace holds the whole run.

## 4. Not verified

- Routes beyond the ten and the attract sequence (the fishing and camping
  routes, the faerie village, the boss routes) were not traced tonight.
- Capcom's start-up chain was armed by its starts; code it reaches that has no
  start in any list (none is known) would not show.
- `BOF3X_MP3=original`, `BOF3X_ORIGINAL=<anything>` and the software renderer's
  path were not run: they are original-only by construction and gone at the
  cutover.

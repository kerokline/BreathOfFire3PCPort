# Story flags: chapter 11

**Status:** IN PROGRESS (2026-11-09, read from source; not checked in play).

Source: `src/game/scena_sc11.cpp` (`Scena11_*`, original `0x55C040..0x55E4D2`).
The file's row helper is `Flags()` (`scena_sc11.cpp:78`), which reads
`Chapter_FlagRow` afresh. The file calls `Flags_Set` and `Flags_Test` directly with
literal flag numbers. It has no `Flags_Clear`. No other file names chapter 11's row:
no `Cond_Flags + 0x58` and no fixed row 11 in `src/game` (census of
`tools/story_flags.py`, and a grep for `0x58` / `0x903FE8`). The area and effect code
that reads "the current row" was checked for the areas this chapter uses (below).

Areas the chapter's code names: `0x65`, `0x79`, `0x82`, `0x83`, `0x84`, `0x88`,
`0x44`, `0x9C` (`Scena11_EnterArea`, the scenes' `ChangeArea` calls). The chapter
calls no event battle (`docs/boss-fights.md` lists none for chapter 11).

## Start

- **Who starts it.** Chapter 10's `Scena10_Run7` (`scena_sc9b.cpp:1819`, body
  `Run7Body`):
  - step `0xF` (`scena_sc9b.cpp:946..956`): chapter 10's flag `0x12`,
    `Field_ChangeArea(0x79, 0x270000, 0x370000, 7)`, then `Field_SetStatus80`
    (`SetBit80`, `Field_StatusBits |= 0x80`);
  - step `0x14` (`scena_sc9b.cpp:957..965`): a transition (`0xB`) and
    `Field_SetStatus80`, with no area change in that step.
- **How.** `Field_ChangeArea` only requests the change (`Field_Request = 5`,
  `window_task.cpp:138..146`). When the change runs, `Area_Enter`
  (`mode_flow.cpp:548..556`) sees `Field_StatusBits` bit 7, adds one to the
  chapter byte `Cond_ByteFA` (10 to 11) and calls `Scenario_Start(11)`.
- **`Scenario_Start`** (`title_states.cpp:404..424`) zeroes the scenario bytes
  `0x8034E1..0x8034E9` (the state, the step, `MoveScript_Var7`), writes 0 to the
  **first dword only** of the row (`SetLong(At(flags), 0)`: flags 0..31; flags
  32..63 are not touched), and points `Chapter_FlagRow` at `Cond_Flags + 0x58`.
- **`Scena11_Start`** (state 0, `scena_sc11.cpp:146`): `Draw_PassFlags = 0x1F`,
  state 1. It sets no flag and changes no party.
- **`Scena11_EnterArea`** (state 1, `scena_sc11.cpp:191`) then runs for area
  `0x79`: with counter 2 (`0x90384A`) at 1 the pass flags are 0; state 2
  (`scena_sc11.cpp:211..215`). No flag is set at the start.
- **First scene.** `Scena11_ArriveHook` (`scena_sc11.cpp:1358`) starts scene 1 in
  area `0x79` when x is at most `0x160000`: at step 0 when member 0's `+8`
  (`ObjTrio[0].direction`) is 0, 6 or 7, else at step `0x32` (script message 6
  only). With flag 0 already set it starts step `0x3C` (script message 7 only).

## Flags

Steps are `MoveScript_PartyRecords[0x65]`. "Counter 0" is the script counter byte
`0x903848`, which the area's event scripts set.

| Flag | Set by (function, step, file:line) | When | Cleared by | Tested by / what it gates | Kind |
|---:|---|---|---|---|---|
| 0 | `Scena11_Scene1` step 15, `scena_sc11.cpp:402` | Counter 0 at `0x14`, scene 1's last step: `ScriptFlags_Clear40`, the script flags `^ 8`, the scene ends | - | `Scena11_ArriveHook` (`:1360`): area `0x79`, set: scene 1 at step `0x3C` (script message 7) in place of the full scene | progress |
| 1 | `Scena11_Scene1` step 8, `:356` | Counter 0 at 1, after the visit to area `0x9C`; `ChangeArea(0x83, 0x50000, 0x5D0000, 0x84)` | - | nothing in this file | progress |
| 2 | `Scena11_Scene1` step 14, `:396` | Counter 0 at 3; counter 2 = 3; `ChangeArea(0x83, 0x70000, 0x5C0000, 0x86)` | - | nothing in this file | progress |
| 4 | `Scena11_Scene1` step 15, `:403` | With flag 0, at scene 1's end | - | nothing in this file | progress |
| 5 | `Scena11_Scene2` step 1, `:442`; `Scena11_Trigger01`, `:1323` | Trigger 1 starts scene 2 and sets the flag (`:1314..1323`). Step 1 sets it again at counter 0 = 3 | - | `Scena11_Trigger01` (`:1315`): set, the trigger does nothing (scene 2 runs once) | once-only set-up |
| 6 | `Scena11_Scene3` step 1, `:467` | Scene 3 from step 0 (trigger 2): party drop-in 4, counter 0 at 6 | - | nothing in this file | progress (reading) |
| 7 | `Scena11_Scene5` step 1, `:650` | Scene 5 from step 0 (trigger 3): drop-in 1, counter 0 at 3; music faded; `ChangeArea(0x44, 0x300000, 0x198000, 0x82)` | - | nothing in this file | progress |
| 8 | `Scena11_Scene5` step 18, `:767` | Counter 0 at 5 after stream 5; `ChangeArea(0x44, 0x2E8000, 0x178000, 0x86)`; step 19 then gives four of item `0x24` | - | nothing in this file | progress (reading) |
| 9 | `Scena11_Scene3` step 16, `:500` | Scene 3 from step `0xA` (trigger 4): items `0x23`, `0x24`, `0x56` and `0x4D` all held (`:473..476`), step 15 drop-in 5, counter 0 at 5. Without all four: script message `0x35` and no flag | - | nothing in this file | progress, gated on the four items |
| 10 (`0xA`) | `Scena11_Scene6` step 5, `:850`; step 20, `:890` | Step 5: request 0 after the request 8; `ChangeArea(0x82, 0x290000, 0x540000, 0x82)`. Step 20: the same flag, then area `0x82` by entrance `0x83` (counter 3 at 1) or `0x84` | - | nothing in this file | progress |
| 11 (`0xB`) | `Scena11_Scene6` step 45, `:960` | After script message `0x40`; counter 2 = 1; `ChangeArea(0x82, 0x290000, 0x530000, 0x85)`; `Music_Play(0x83)` | - | nothing in this file | progress |
| 12 (`0xC`) | `Scena11_Scene6` step 48, `:978` | Counter 0 at `0xA` after stream 2; scene 6 ends | - | `Scena11_EnterArea` (`:195`): area `0x65`, counter 2 at 0, 12 set and 14 clear: `Area65Start` (scene 8 from step 0) | progress |
| 14 (`0xE`) | `Scena11_Scene8` step 0, `:1002` | Scene 8's first step; `ChangeArea(0x65, 0x240000, 0x250000, 0x80)` | - | `Scena11_EnterArea` (`:195`): set, scene 8 does not start again | once-only set-up |
| 15 (`0xF`) | `Scena11_Scene8` step 27, `:1167` | Counter 0 at `0x16`; sound `0x201`; `ChangeArea(0x84, 0x50000, 0x460000, 0x84)` | - | nothing in this file | progress |
| 16 (`0x10`) | `Scena11_Scene8` step 33, `:1187` | Counter 0 at `0xA`; `ChangeArea(0x83, 0x200000, 0x2F0000, 0x87)` | - | nothing in this file | progress |
| 17 (`0x11`) | `Scena11_Scene8` step 37, `:1211` | Request not 2; `ChangeArea(0x83, 0x50000, 0x5D0000, 0x88)` | - | nothing in this file | progress |
| 18 (`0x12`) | `Scena11_Scene8` step 39, `:1221` | Counter 0 at `0x15`; scene 8 ends | - | nothing in this file | progress |
| 19 (`0x13`) | `Scena11_Scene9` step 1, `:1244` | Scene 9 from step 0 (trigger 7): drop-in 9, counter 0 at 7 | - | nothing in this file | progress (reading) |

Flags 3, 13 and 20..63 are not set, cleared or tested by any chapter 11 code.

**What starts each scene** (`scena_sc11.cpp:1314..1348`; the table index is the
object's `+0x86`): trigger 1 scene 2 (once, flag 5); 2 scene 3 step 0; 3 scene 5 step
0; 4 scene 3 step `0xA` (the item check); 5 scene 6 step 0; 6 run 7 (a bare return,
`scena_sc11.cpp:294`); 7 scene 9 step 0; 8 scene 9 step 5 (the end); 14 scene 5 step
`0x28`. Scene 1 comes from the arrive hook, scene 8 from `Scena11_EnterArea`. Triggers
9..13 set `MoveScript_Var7` past the ten-entry run table (D133, `docs/scena_sc11.md`
section 6).

## End

- **The step.** `Scena11_Scene9` step 9 (`scena_sc11.cpp:1270..1278`): once the
  message is closed, the step and run 0, `Field_SetStatus80` (`kStatusBit80`), the
  script flags `^ 0x16`, counter 2 = 2, `Field_ChangeArea(0x79, 0x290000, 0x330000,
  7)`. `Area_Enter` then starts chapter 12 (`mode_flow.cpp:548`).
- **The path.** Trigger 8 starts scene 9 at step 5 (`:1333`): drop-in `0xA`,
  counter 0 at 6, then `0xF`, transition 4, script message `0x3F` (`:1248..1269`).
  **No flag is set on this path.** Flag 19 belongs to the other entry of scene 9
  (trigger 7, step 0..1).

**The row as left (reading, not checked):**

- Set by the code on the way through: 0, 1, 2, 4, 5, 6, 7, 8, 10, 11, 12, 14, 15,
  16, 17, 18, 19.
- Set only with the four items held: 9 (scene 3 step `0xA`).
- Alternatives: none. Cleared before the end: none (the file has no
  `Flags_Clear`).
- Never set: 3, 13, 20..63.

**Against the saves** (`observed-rows.md`): saves 20 (42:37, chapter 14), 21
(42:44) and 22 (43:25, chapter 15) all hold chapter 11's row as `0..2 4..12
14..19`. That is exactly the set of flags the code sets. Save 21's header says
chapter 10, but its rows 11..14 equal save 20's, so it is past this chapter too.
The saves agree with the reading on every flag, including the two never set (3
and 13). They cannot show which flags are mandatory, because all three saves come
from routes that set every flag.

## Event flags used by this chapter

None. No `Scena11_*` function touches `Cond_Flags + 0xA0`. The engine clears event
flag `0x1C` on every area change (`Area_Enter`, `mode_flow.cpp:560`), in every
chapter.

## Unresolved

- **The scenes' later steps.** Several steps are entered by code outside this file:
  `Scena11_Scene6` step 1 and 11 set request 8 and leave the step (`:839..844`,
  `:871..876`), so steps 5, 10, 12, 20, 30 and 40 are set elsewhere (event scripts
  or the request handler). The same holds for `Scena11_Scene5` steps 15 and `0x28`
  (trigger 14 starts `0x28`) and the counters each step waits on. Which of flags 6,
  7, 8, 10, 11 and 12 are on the one mandatory path needs a run.
- **Scene 1's start test.** Scene 1 runs in full only when member 0's `+8` is 0, 6
  or 7 (`:1368..1370`). Otherwise only script message 6 shows, and flags 0, 1, 2, 4
  stay clear until a later arrival. What `+8` holds at that point is not read.
- **Event scripts.** `Area_TestCondition` and the event-script opcodes
  (`event_script.cpp:679..693`, `field_o.cpp:1304`) can set or test any row by an
  index from the area data. Those uses of row 11 are not visible in source.
- **Effect kind `0x77`.** `Scena11_EnterArea` places one in area `0x88` with
  counter 2 at 10 (`:266..274`). Its end, `EffectKind77_End`
  (`effect_3d.cpp:333`), tests flag `0x2E` (46) of the current row and then starts
  run 5 at step `0x19`. Chapter 11's scene 5 has no step `0x19` (it does nothing),
  and chapter 11 never sets flag 46. `EffectKind77_Tick` (`effect_3d.cpp:319`)
  tests flag `0x14` (20), which chapter 11 never sets. Reading, not checked.
- **Area 136's tail.** `Area136_Tail19` (area `0x88`, `area_w3e.cpp:264`, `:291`)
  sets and clears flag 46 of the current row when key item 6 is held. If it runs in
  chapter 11, the flag is set and cleared within one sequence (transient). Which
  chapters reach it is not read.

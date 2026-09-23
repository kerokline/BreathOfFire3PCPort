# The third round's queue: what is left after groups H and J

**Status:** DONE (2026-09-22) - all three groups merged and through the live batch `ab24`

Drawn from the regenerated catalogue (`python tools/attract_catalog.py
analysis/calltrace/hidden_b/bof3x.callcounts.tsv --also ...`, 353 reached and
not ours before the round) with the three corrections in
[`attract-remaining.md`](attract-remaining.md) §3-§4.6. Groups H (the message
box, [`msgbox.md`](msgbox.md)) and J (the window/task layer,
[`window-task.md`](window-task.md)) are merged. These three are not started;
their boundary callees are **not** registered in `symbols.toml` yet - do that
first, one owner per address, as the round's first commit did (`7175857`).
Counts are `hidden_b`'s; sizes are `pe_funcs.py`'s and several will be wrong.

## K - the top-level task flow (18) -> `src/game/mode_flow.cpp`

`0x495070` (192; holds a 12-way stack-built dispatcher at `0x495125` ->
`0x495250`..`0x4955A0`, none reached), `0x495130` (32), `0x495150` (32),
`0x495620` (126), `0x495750` (176; calls `Gpu_SetTile`), `0x495800` (64),
`0x495840` (192; calls `Field_ChangeArea`), `0x495900` (240), **`0x594E60`**
(765, the area-entry hub `0x495900` calls; calls `Area_ZoneIdAt`,
`Effect_ClearAll`, `Music_Play`, `Gfx_ClutStripRestore`, `0x454A20`,
`File_LoadDone` and round-2 functions), `0x4967F0` `Field_WaitTransition`
(58), `0x496870` (399, 11,901 calls), `0x496B60` (304), `0x496C90` (48); and
§4.9's `0x454810` `File_LoadDone` (6, `return 1`, 152 callers), `0x4549B0`
`Gfx_ClutStripRestore` (52), `0x454A20` (37), `0x454A50` (33), `0x454AB0`
(20), `0x461E10` (63). Task bodies loop on `Task_Sleep`:
[`title-states.md`](title-states.md) is the precedent; the scheduler (§4.2,
hand-written `esp` swaps) stays Capcom's behind stand-ins.

## L - the movement commands and the party (15) -> `src/game/move_cmds.cpp`

`0x5190A0` `Party_MoveMember` (512) with `0x5A7A70` (27) under it, `0x518B00`
`Sprite_SetPoseWait` (22), `0x573400` `MoveCmd_MoveKind2` (237), `0x5734F0`
`Kind2_Place` (102), `0x578D10` `MoveCmd_OpF7` (164), `0x578DC0`
`MoveCmd_HandlePosition` (234), `0x5792A0` `MoveCmd_Attach` (232) +
`0x579390` (29) + `0x5793B0` (146), `0x579450` `MoveScript_FindLabel` (120),
`0x57C310` `MoveScript_Variable` (240), `0x57C840` `MoveScript_ObjectKind`
(82), `0x57CDC0` `Sprite_ScriptPeek` (78), `0x519890` (96, hidden, 3 calls
from `Scena16_Start`). Every caller is already ours
([`movement-script.md`](movement-script.md)).

## M - sprite animation and effects (10) -> `src/game/sprite_pose.cpp`

`0x5891F0` `Sprite_SetAnimation` (16), `0x589200` `Sprite_SetAnimationAt`
(303), `0x589330` (30), `0x5894D0` `Sprite_SetFrameQueueUpload` (189 - the
unbounded queue append in [`known-defects.md`](known-defects.md)), `0x589590`
`Sprite_SetAnimationBank` (206), `0x57C4C0` `Sprite_FaceDirection` (132),
`0x589810` (44), `0x589840` `Effect_Release` (47), `0x589870` (45),
`0x5898A0` `Effect_ClearAll` (35).

## Boundaries between K, L and M

`Effect_ClearAll` (M takes; K's `0x594E60` calls), `Gpu_SetTile` and
`Area_ZoneIdAt` / `Field_ChangeArea` (ours since J; K calls),
`Sprite_SetAnimation` / `SetAnimationAt` (M takes; our `MoveScript_Flow` and
M's own `Sprite_FaceDirection` call), `0x5A7A70` (L, unnamed - name it before
spawning). L's callers are all ours, so L has no boundary with K or M.

## Left out on purpose

§4.1 the Windows shell (I8 / I12 replace it; `Pad_Read` has five indirect
calls), §4.2 the task system (assembly), the renderer (waits on I14), sound,
the MP3 decoder and the CRT.

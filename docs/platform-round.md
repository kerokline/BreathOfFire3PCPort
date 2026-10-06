# The platform round, step 2: the small layers and the renderer's live remainder

**Status:** MEASURED (2026-10-05 night) - step 2 of
[`platform-layers-plan.md`](platform-layers-plan.md) section 4, run in
one night alongside round fourteen's end: five groups from
`phase-3/round14-end` at `e3b98087`, merged one at a time into
`phase-3/platform-round` with the round's end merged after them
(`cdcadb9`). **56 functions ours, 10,009 -> 10,065.** Headless-verified
per group (each group's own shadow and `'*'` narrow and wide) and at the
merged tip (section 5); the state hash live check is the next thing
(section 6). Nothing pushed.

[`platform-read-pass.md`](platform-read-pass.md) measured which of Capcom's
remaining starts still run under ours and named 40 for a takeover round.
This is that round, plus thirteen functions the read pass could not see
(section 2). Each group's doc is the record of its functions; this file is
the round's: what was taken, what the groups found that changes the plan,
how it was merged and verified.

## 1. The groups

| Group | Module / doc | Functions | Ours after | What |
|---|---|--:|--:|---|
| PS | `sound_rest`, [`sound-rest.md`](sound-rest.md) | 9 | 10,018 | `Sound_StopMusic`, `Sound_ResumeAll`, `Sound_MusicPlaying` (`0x587C20`, named here), `Sound_PauseAll`, `SndBuf_SetVolume` `0x5A6C60`, `Music_Halt` `0x5A6FF0`, `Music_Resume` `0x5A7080`, `SndStream_IsPlaying` `0x5A7200`, and **`Snd_Init` `0x5A6830`** (the "platform set-up" start every route runs: DirectSound's device and primary buffer, nothing else - the voices, the stream and the decoder handle are made on first use; `0x5ACBBC` is `DirectSoundCreate`'s import thunk, not a decoder function). 32 controls. Data: `Snd_Primary` `0x7DE3C0`, `SndStream_Buffer` `0x7DE3C8`. |
| PW | `shell`, [`shell.md`](shell.md) | 7 | 10,016 | `Input_Latch`, `Cfg_Load`, `Game_Init`, `Gfx_InitBufferBlock` (`0x4FD200`), `Gfx_LinkOTags`, `Disc_Probe`, `Cfg_SetDefaultKeys` (`0x5A9880`): game code filed as shell by address; every route runs all seven. Only `Disc_Probe` calls Windows (`GetDriveTypeA`: `CAPCOM.AVI` locally, else drives C..L for a CD-ROM holding `BOF3.EXE`; no volume check). `Cfg_Load` and `Game_Init` are naked (the frame layout, the task stacks' base from the incoming esp). 27 controls (26 refused by count, one equivalent). Data: `Gfx_BufferBlocks` `0x903880`; `Crt_fgets`, `Crt_atoi`, `Crt_sscanf` named. |
| PL | `psx_rest`, [`psx-rest.md`](psx-rest.md) | 17 | 10,026 | `Gpu_SetPolyF3` `0x5A7570`, `Gpu_SetPolyFT3` `0x5A7590`, `Gpu_SetLineG4` `0x5A76F0`, `Gpu_SetTexWindow` `0x5A7840` (stores the rect's address, not its contents), `Gte_SquareRoot0` `0x5A7A90`, `Gte_ApplyMatrixSV` `0x5A7C70` (both by Psy-Q signature in the sibling), `Display_Teardown` and its seven (`Gfx_FreeClutRows`, `Gfx_ReleaseTexCache`, `Font_ReleaseTexCache`, `Display_ReleaseOrphans`, `Display_ReleaseBackdrop`, `D3d_ReleaseAfterDraw`, `D3d_FreeCellTextures`), `Display_TextOut`, `Display_ErrorBox`, `Sound_Shutdown`. 42 controls; the `SquareRoot0` precision proof (a 24-bit control word differs on 5,785 rounds). |
| PH | `d3d_rest`, [`d3d-rest.md`](d3d-rest.md) | 10 | 10,019 | `D3d_DrawPolyF3`, `_FT3`, `_GT3`, `D3d_DrawLineG4`, `D3d_DrawTile1`; `0x5A0910` is `D3d_FlattenFT3` and `0x5A0A40` `D3d_PageTexel4`, FT3's helpers; `D3d_SetAlphaModulate`, `D3d_AfterDraw` (a frame capture nothing requests and nothing reads; its back-buffer `Lock` would `Fatal` in the shim under Capcom's code as under ours); and **`0x59E930` = `Gfx_StoreImage`**, `Gfx_LoadImage`'s inverse - PSX library by nature, renderer by address. 39 controls (one blind on the first run: a 16.16 scale always a multiple of `0x4000`; seeded one step below a boundary). `Gfx_DrawOTag`'s table uses the handlers through `Raw` by name (hard rule 3). |
| PM | `mode_rest`, [`mode-rest.md`](mode-rest.md) | 13 | 10,065 | **Game modes 3, 4, 5 and 6 and their steps** - `GameMode3_Run` / `_Enter` / `_Leave`, `GameMode4_Run`, `GameMode5_Run` / `_TurnSense` / `_Turn` / `_ToPlaces` / `_Load` / `_Script` / `_Place` / `_Leave`, `GameMode6_Run` (`0x495BB0..0x496230`): the field menu's, the battle's. 49 controls. Data: `GameMode3_Steps` `0x656A74` (3), `GameMode5_Steps` `0x656A84` (8). Section 2. |

Already ours, met on the way and not taken: `SndBuf_Stop`, `SndStream_Stop`,
`Music_IsPlaying` (PS); `Menu_Frame` `0x5172F0`, `Battle_Frame` `0x42E370`,
`0x496250`, `0x496A00`, `0x517240`, `0x517290` (PM). Every start of the 40
and the 13 turned out to be a function.

## 2. What the count had missed: thirteen functions outside every catalogue

Group PS, placing `0x587C20`, found its two callers at `0x495C27` and
`0x495D26` in code with no symbol. `GameMode_Handlers` `0x656A44` (the
twelve mode pointers `Field_Task` calls every frame) has four entries -
modes 3, 4, 5, 6 - with no `[[func]]`, and under them nine step handlers in
two tables nothing named. **`pc_funcs.json` gave `GameMode_Field` `0x4959F0`
an extent of 2,139 bytes**, to `0x49624B`, which is exactly mode 6's tail
jump; the thirteen are reached only through `.data`, so no direct call
ever split them out, and every start list since - the catalogue, round
eight's and fourteen's cuts, the read pass's 432 armed entries - was built
on that extent. Round eight's [`mode_states.md`](mode_states.md) had listed
them as unowned neighbours; the note never reached a later cut. **"10,009
ours, 0 left original" was true of catalogued starts.** The coordinator's
scan of every named `void *` table in `symbols.toml` found exactly those
four entries without a symbol and no other; the step tables were unnamed and
so unscanned. [`mode-rest.md`](mode-rest.md) section 0 has the full account
and a cheap scan for the class (every `jmp` / `call [reg*4 + imm32]` in
`.text`, its table walked while the entries point into `.text`, each target
checked against the starts; and each catalogue extent against where its code
ends) - not yet run.

These are the first functions of the night a route runs on every play: mode
3 whenever the field menu opens, mode 5 in every fight. They matched the
state hash before tonight only because both sides ran Capcom's.

## 3. The read pass's open questions, answered

- **The software render flag cannot be set under ours** (PH): its only
  setter is `or al, 1` at `0x5A530F` inside Capcom's `Display_Setup`, ours
  since DIV-0031. The 32 software-path starts run only under
  `BOF3X_ORIGINAL=Display_Setup` with `renderer=0`: **original-only, gone at
  the cutover**, with `d3d_list.cpp`'s first table.
- **`0x59E930` is `Gfx_StoreImage`** (PH), the VRAM shadow's read-back:
  taken; four effect states of ours call it, no route reaches it.
- **`0x587C20` was mis-classed "original"** because its callers sat inside
  `GameMode_Field`'s over-long extent (section 2). Held, and now ours.
- **`0x5A6830` is sound alone** (PS), not display or input.
- **Only `Disc_Probe` calls Windows** among the shell's seven (PW); the read
  pass's "every one a leaf or a thin wrapper over a Windows call" was true of
  the sound and library groups, not the shell.
- **The teardown chain** (PL): `0x7CAE20..0x7CAE37`'s six pointers are named
  only by the teardown, nothing makes them, so the release does nothing;
  the fullscreen branch cannot run under ours (the backend never sets bit
  `0x100`); `Display_ErrorBox` is unreachable under ours (`Game_Init`
  ignores the sound set-up's return).

## 4. Wants a ledger entry (proposed by the groups, nothing built)

- **TILE_1 is drawn as one point** (PH, `D3d_DrawTile1`: a `POINTLIST` of
  1, so at scale 2 it lights one of the four pixels a PSX pixel covers). The
  only defect here a recorded route shows: `whelpBoss`'s battle motes. The
  fix would be a scale-by-scale quad. **The owner's call.**
- **A one-texel POLY_FT3 gets the wrong colour** (PH): `D3d_FlattenFT3`
  reads the CLUT and tpage at the PSX offsets (`+0xE`, `+0x16`) not the
  port's (`+0x16`, `+0x26`), and `D3d_PageTexel4` takes the high nibble for
  an even u where `Tex_Convert4` reads the low one first. No route reaches
  it; controls 23 and 27 plant the two fixes and are refused.
- **`Cfg_Load`'s key lines overrun their array** (PW): 0x80 bytes copied
  into a 0x28-byte array, so entries 10..31 of `Key_Table` get the return
  address and WinMain's stack; from the 21st line the return address itself.
  Only a hand-edited `BOF3.CFG` reaches it. A fix: read into a zero-filled
  32-entry array of our own and ignore extra lines.
- **`Sound_ResumeAll` plays the music buffer even after a fade stopped it**
  (PS), so stopped music may come back after a pause or a loss of focus -
  read from the code, not heard. And the primary buffer is 22,050 Hz 8-bit
  stereo under 44.1 kHz 16-bit decoding (an idea for I23's listening set).
- Whether DIV-0010's far-edge rule extends to FT3 / GT3 is not established:
  they index `D3d_TexCoords[u]` as FT4 does, so it depends on what the
  builders put in the primitive (PH).

Noted for the docs, no behaviour: `display-setup.md` section 7's guess that
codes `0xF4..0xF7` request the frame capture is wrong (the walk hands them
to `D3d_SetAlphaModulate`); `window-modes.md` line 34 still lists the shell
seven as Capcom's; `controls.md` section 1's "two integers a line" is two
bytes a line, one key entry over two lines (`shell.md` section 2);
`menu-screens.md` section 1's "reloads the area's file" for mode 3's step 2
is the party set's file, `0x2C2 + set`; `pairs_propagated.json` pairs
`0x80198620 -> 0x4964E0` (correct `0x495BC0`) and `0x80198E54 -> 0x5885D0`
(correct `0x4960D0`).

## 5. The merges and the verification

Merge order PS `49147a8`, PW `c7beac3`, PL `7cdfb9b`, PH `eeb5b97`, PM
(`67e1598` after its fixes), then `phase-3/round14-end` `cdcadb9`; the
keep-both resolver on the four shared files, two collisions by hand: PS and
PL both named `0x7DE3C0` (kept `Snd_Primary`, PS's, `01e9427`); PS and PM
both took `Sound_MusicPlaying` `0x587C20` (kept PS's, PM's definition,
inject, clone row and entry dropped, `sound_rest.h` declaring the thunks for
callers by name, `d9df80f`..`67e1598`); and the harness fold's `_OURS` rows
against PH's by-address row for `Gfx_StoreImage` (PH's kept: the function is
file-local to `d3d_rest.cpp`). Two constants rebound after the fact: PW's
`kSoundSetup` to `Snd_Init`, R3F's `kStoreImage` to `Gfx_StoreImage`.

Each group's own `'*'` narrow and wide passed at its branch (every report:
`self-test only: done`, `inject: 10,0xx ours`, no mismatch). At `67e1598`
(the five groups): `mode_rest` 52,000 rounds and `sound_rest` 27,000, 0
mismatches, `inject: 10065 ours`. **The merged tip `cdcadb9`:** `'*'` narrow
and wide - recorded below when the run ends.

**`analysis/calltrace/entries_logic.txt`** (the main checkout's): PS's two,
PH's nine and PM's thirteen lines appended 2026-10-05 night (10,662 ->
10,686); PW's and PL's were present already.

## 6. Next

1. **The state hash, the machine quiet**: the attract sequence and the ten
   routes against the 168-range references (`state-hash.md` section 6),
   `BOF3X_LAYERING=0`, Chinese. The shell seven, `Snd_Init` and modes 3 and
   5 run on every route, so this is their first live check; TILE_1 in
   `whelpBoss`; `Gpu_SetTexWindow` there too.
2. The plan's step 3: the runtime's seventeen entry points, `rand` first.
3. The owner's calls in section 4; the mode-rest scan for other hidden
   starts (section 2).

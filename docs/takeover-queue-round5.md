# The fifth round's queue: the last of what the attract sequence reaches

**Status:** IN PROGRESS (2026-09-23)

Drawn from the catalogue regenerated 2026-09-23 after round four
(`python tools/attract_catalog.py analysis/calltrace/hidden_b/bof3x.callcounts.tsv
--also analysis/calltrace/all_a/bof3x.callcounts.tsv,analysis/calltrace/all_b/bof3x.callcounts.tsv`):
**241 reached and not ours, 388 reached and ours.** The owner's scope for the
close of stage 1 (2026-09-23): the texture builders and the per-frame glue,
not the platform set-up. What the 241 are:

| Bucket | Count | This round |
|---|--:|---|
| MSVC CRT | 100 | no - a library; replaced whole, not function by function |
| MP3 decoder | 77 | no - the same |
| Windows shell (WinMain, WndProc, pad read) | 12 | no - left out since round one ([`attract-remaining.md`](attract-remaining.md) §4.1); [`IDEAS.md`](IDEAS.md) I8 / I12 |
| Task system | 6 | no - `esp` swaps, assembly or nothing (§4.2) |
| Not functions | 7 | no - five case blocks of functions already ours (`0x576CD0`, `0x577800`, `0x577B80`, `0x56B730`, `0x56B990`), the bare `ret` `0x437CC0`, the empty `Port_DroppedCall` `0x4DF820` |
| Platform set-up | 15 | no - the owner's call: I8 replaces it. `0x5A5160` (the 3.6 KB Direct3D set-up) with `0x5A5130`, `0x5A5BC0`, `0x5A5E40` (a display-mode `qsort` comparator), `0x5A5EA0`, `0x5A5F90`, `0x5A5FF0`, `0x5A6050`, `0x5A60E0`, `0x5A6230`, `0x5A62C0`; `0x5A6830` (`DirectSoundCreate`), `0x5A94C0` (`DirectInputCreateA`); `Fmv_Play`, `Fmv_WndProc` |
| **This round** | **24** | groups S, T, U below |

## S - the display environments and the bank load -> `src/game/display_env.cpp`

The PSX library's display calls, as the port implements them over DirectDraw:
`0x5A7860` (a `PutDispEnv`: copies the 0x14-byte `DISPENV` to `0x7DECE0`,
presenting through `0x59EDF0` when the display's y changes), `0x5A7890` (a
`PutDrawEnv`: copies 0x5C bytes to `0x7DED00`, calling `0x5A5050` when the
clear colour at `+0x19..+0x1B` changes), `0x5A78E0` / `0x5A7910` (the
`SetDefDispEnv` / `SetDefDrawEnv` shapes), `0x59EDF0` (Blt `+0x14` or Flip
`+0x2C` on `[0x7CC338]`, `Restore` `+0x6C` on `DDERR_SURFACELOST`, then
`0x59ECE0` under `[0x7DED18]`), `0x59ECE0`, `0x5A5050`. And `Snd_LoadBank`
`0x587CD0` (224 bytes; `pe_funcs.py`'s 0xBA9 is wrong) - its callees are
group Q's, ours. Names are hypotheses until read; the fuzz needs a fake
DirectDraw surface for `0x59EDF0` / `0x59ECE0` - record the calls, as
`d3d_fuzz.*` does for the device.

## T - the page textures and the surface helpers -> `src/game/tex_page.cpp`, `src/game/ddraw_fuzz.*`

`D3d_BuildPageTexture` `0x5A0080`, `D3d_RefreshPageTexture` `0x5A0510`, the
pixel converters under them `0x5A9A59`, `0x5A9BFA`, `0x5A9D2F`, and the
shared surface helpers `0x59F840` (zeroes a `DDSURFACEDESC`, size `0x7C`,
pixel-format size `0x20`), `0x59F860` (a plain surface through
`IDirectDraw2::CreateSurface` `+0x18`), `0x59F900` (a texture surface, caps
`0x1000`). T builds **the fake DirectDraw surface** - `Lock` `+0x64` hands out
a buffer the fuzz owns, `Unlock`, `Blt` `+0x14`, `SetColorKey` `+0x74`,
`QueryInterface` for the texture - and the fake `IDirectDraw2` whose
`CreateSurface` makes them, in `src/game/ddraw_fuzz.*`, for U to reuse.

## U - the glyph and cell textures -> `src/game/tex_cells.cpp` (after T)

`Font_BuildGlyphTexture` `0x5A2CA0` with its unpacker `0x5A9E1E`;
`D3d_BuildCellTexture` `0x5A32B0`, `D3d_RefreshCellTexture` `0x5A37D0`, and
under them `0x5A2E70`, `0x5A3790`, `0x59F9B0`, `0x5AA03E`, `0x5AA1E0`,
`0x5AA2F6`, `0x5AA4AE`. Runs after T is merged, on T's fake surface.

## The live check

`ab26`: the self-tests, the oracle, the memory dump, the frame hash with an
original-vs-original pair, and the capture A/Bs - which for this round are
the check that matters, since every function here ends in pixels. Run it
with the screen uncovered, after `ab25b`.

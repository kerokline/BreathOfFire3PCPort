# The platform layers' read pass: what of Capcom's code still runs under ours

**Status:** MEASURED (2026-10-05: step 1 of [`platform-layers-plan.md`](platform-layers-plan.md) section 4 measured at `main` `2df90d9`, 10,009 ours; step 2 ran the same night on this table - [`platform-round.md`](platform-round.md) - and corrected it in three places, section 7)

[`platform-layers-plan.md`](platform-layers-plan.md) section 1 counted what
round fourteen leaves in `BOF3.exe` by address range and said what the
count does not establish: which rows are game code mis-filed, which still
run under ours, and which runtime entry points the game calls. This is that
measurement. Two instruments, one new:

- **The reach trace.** `tools/calltrace.py`'s tracer with an entry list of
  every start that is not ours and not a jump-table case inside a body of
  ours - 432 entries (the catalogue's parts 0 and 1 at `main`, less the five
  "not functions", less the hidden starts no earlier run had armed, plus
  `Task_RunAll` for the frame count) - over ours running the attract
  sequence (six minutes) and the ten recorded routes, Chinese, narrow,
  `BOF3X_LAYERING=0`. First call only, with the caller. The files are
  `analysis/calltrace/platform_1005/<run>.tsv` (game-derived, not committed).
- **`tools/platform_reach.py`** (new): for each such start, who refers to it
  in `BOF3.exe` - direct calls and jumps, split by whether the caller's
  function is still Capcom's or is a body of ours (dead while ours runs),
  and the cells that store its address - and how often its address appears
  in our DLL, by section (an immediate in `.text` is a call by address from
  ours; a `.rdata` hit alone is a fuzz row or a table of ours). The traces
  are the arbiter; the scan says why a start that was not entered might
  still be. Its classes:

  | class | meaning |
  |---|---|
  | runs | a traced run entered it |
  | held | not entered, but our DLL's code, a data cell, or a function classed runs or held refers to it |
  | listed | only our DLL's data names it |
  | original | every reference is from a body of ours or from a start that is itself original: reached only by Capcom's code we have replaced |
  | unreferenced | nothing refers to it |

  Output: `analysis/platform_reach_1005.tsv` (not committed).

## 1. The answer by layer

| Layer | Starts | runs | held | listed | original | unreferenced |
|---|--:|--:|--:|--:|--:|--:|
| MSVC CRT | 242 | 112 | 105 | 1 | 19 | 5 |
| MP3 decoder | 200 | 76 | 59 | 3 | 29 | 33 |
| Renderer | 43 | 1 | 4 | 20 | 18 | 0 |
| PSX library layer | 17 | 1 | 16 | 0 | 0 | 0 |
| Platform set-up | 10 | 1 | 0 | 0 | 9 | 0 |
| Sound | 8 | 0 | 7 | 0 | 1 | 0 |
| Windows shell | 7 | 7 | 0 | 0 | 0 | 0 |
| Not functions | 5 | 0 | 0 | 2 | 3 | 0 |

The "held" and "listed" counts in the runtime and the decoder are their
insides, reached through the entry points below; the numbers that matter are
the entry points.

## 2. The small layers, row by row

**Windows shell (7, all run, every run).** Called from our `WinMain` and
`Game_Init`'s chain: `Input_Latch` `0x4FC6A0` (from `InputScript_Latch`,
once a frame), `Cfg_Load` `0x4FD030`, `Game_Init` `0x4FD110`, `0x4FD200`
(from `Game_Init`: eight bytes of a record cleared through `0x5A7960`, four
flags set), `Gfx_LinkOTags` `0x4FD290` (once a frame), `Disc_Probe`
`0x5A72C0`, and `0x5A9880` (from `Cfg_Load`: 32 dwords copied from
`0x66C648` to `0x7DE7A8` - the default key table). Game code, filed as
shell by their address: **a takeover group of seven**, all reached by every
route, so the state hash checks them.

**Platform set-up (10).** `0x5A6830` runs (from `Game_Init`, every run).
The other nine (`0x5A5BC0`, `0x5A5E40`, `0x5A5EA0`, `0x5A5F90`, `0x5A5FF0`,
`0x5A6050`, `0x5A60E0`, `0x5A6230`, `0x5A62C0`) are **original**: their
only references are from `Display_Setup` `0x5A5160` (ours since DIV-0031)
and from each other. The DirectDraw / Direct3D device set-up ours replaced.
Nothing to take; gone at the cutover.

**PSX library layer (17).** One ran: `0x5A7840` (a 12-byte primitive from a
rect, `kPrimFromRect` in three effect groups), in `whelpBoss` from
`EffectKind10_Run`. The other sixteen are **held**: `Display_Teardown`
`0x5A6380` and its seven callees `0x5A64B0..0x5A6760`, `Display_TextOut`,
`Display_ErrorBox`, `Sound_Shutdown` (each called by address from ours at
exit or on an error - no route exits that way), and five one-line libgpu
setters ours calls by address - `0x5A7570` (code `0x20`, fourteen bodies of
ours and 28 call sites in the DLL), `0x5A7590` (`0x24`), `0x5A76F0`
(`0x5C`), `0x5A7A90` (`SquareRoot0`: `fild`, `fsqrt`, `_ftol`), `0x5A7C70`
(a 3x3 matrix times a vector, `>> 12`). None of the five was entered by any
route. **A takeover group of seventeen**, the setters fuzzable in minutes,
the teardown chain a recorded stand-in.

**Sound (8).** Seven **held**, none entered: `Sound_StopMusic` `0x587B80`
and `Sound_ResumeAll` `0x587B90` are five-byte jumps to `0x5A6FF0` (stop
the stream buffer `0x7DE3C4` if its status bit 0 is set) and `0x5A7080`
(play it looping); `Sound_PauseAll` `0x587C30` stops every voice of
`0x6BC8C8..0x6BC924` through `0x5A6C30` and the stream through `0x5A71C0`;
`0x5A6C60` sets a voice's volume (a `DirectSoundBuffer::SetVolume` at vtable
`+0x3C`, the level scaled through `_ftol`); `0x5A7200` answers whether the
stream is playing. Ours calls them by address from 17 bodies; the routes
never stop the music or pause. `0x587C20` (a jump to `0x5A7020`) is
**original**: one caller, a body of ours. **A takeover group of seven**, each
a DirectSound call behind a recorded stand-in.

**Renderer (43).** Three kinds:

- **Alive: the Direct3D primitive handlers that are not yet ours.** Our
  `Gfx_DrawOTag` (`d3d_list.cpp`) dispatches by primitive code through the
  original's handler addresses (`docs/d3d-draw.md` section 3's table), and
  six of the Direct3D table's entries are still Capcom's: POLY_F3
  `0x59FA50`, POLY_FT3 `0x59FDB0`, POLY_GT3 `0x5A1050`, LINE_G4 `0x5A1EA0`,
  TILE_1 `0x5A2220`, and `0x5A0910` / `0x5A0A40` under POLY_FT3; with
  `D3d_SetAlphaModulate` `0x59F520` and `D3d_AfterDraw` `0x59F580`, which
  ours calls by address. **One ran: TILE_1 `0x5A2220`, in `whelpBoss`.**
  The others' codes are primitives no route built (a flat triangle, a
  Gouraud line). **A takeover group of nine**, the live remainder of the
  presentation layer.
- **Listed only: the software surfaces' table** (`0x5A3A60..0x5A4C40`, 21
  entries, and their callees `0x5A3C40`, `0x5A3CC0`, `0x5AA80F`,
  `0x5AA9EE`, `0x5AAB38`, `0x5AAB39`, `0x5AACE3`, `0x5AAD26`, `0x5AAEC0`,
  `0x5AAF93`, `Gfx_PackRgb` `0x5AA79C`). Our `Gfx_DrawOTag` keeps the table
  and picks it when `Gfx_RenderFlags` `0x6C3A4C` has bit 0 (the
  software renderer, `renderer=0`). **Not measured: whether that path can
  run at all under ours** - the Direct3D 11 backend replaced the surfaces
  it draws to. If it cannot, these 32 are original and the table entry is
  the one line to retire; if it can, they are a renderer of their own.
- **Original: Capcom's `Gfx_DrawOTag` and set-up.** `0x59EF20`, `0x59EF90`,
  `0x59F030` (the original walk's blocks), `Fmv_EnterFullscreen` `0x59E4F0`
  (one caller, ours since DIV-0035), `0x5A9A30` and `0x5AA671` (from the
  set-up `0x5A60E0`), `0x455050` (a case of `ClutMap_Mark`, ours). Gone at
  the cutover. `0x59E930` (held: called by address from four bodies and ten
  sites of ours) is the one row of this layer not placed by this pass -
  read it with the handlers.

**Not functions (5).** `0x56B730`, `0x56B990`, `0x576CD0`, `0x577800`,
`0x577B80`: data the start lists took for code. Not armed, not entered, not
to take.

## 3. The C runtime's entry points

The CRT's own 242 starts are not the unit of work; the entries the game
calls are. From the DLL's `.text` immediates and the traces' first callers:

| Entry | What | Called by ours | Entered |
|---|---|---|---|
| `0x5B9380` `Crt_sprintf` | `sprintf` | 286 sites | every run, from ours |
| `0x5B93D2` `Rand` | `rand` | 1,227 sites | every run |
| `0x5B9450` `Crt_strncpy` | `strncpy` (capstone 2026-10-05: the NUL test and the zero fill) | 12 | no route (the save block's name copy) |
| `0x5B9550` | `_ftol` | 12 sites of ours, 44 bodies of ours | every run |
| `0x5B9577` `Crt_free` / `0x5B9660` `Crt_malloc` | | 7 / 9 | every run |
| `0x5B979A` / `0x5B9867` | `_findfirst` / `_findnext` (from `Save_ListFiles`) | 1 / 2 | ten routes |
| `0x5B9993` `Crt_fclose`, `0x5B9B6D` `Crt_fopen`, `0x5B9D4E` `Crt_fread`, `0x5B9F9E` `Crt_fseek`, `0x5C35D6` `Crt_filelength`, `0x5C3660` `Crt_fileno` | the file layer's | 1..2 each | every run |
| `0x5B9E65` `Crt_fwrite` | | 1 | no route (a save's write) |
| `0x5BAD64` `Crt_GetPtd` | the per-thread data | 2 | every run |
| `0x5C2B40` | `_stricmp` | 2 | no route |
| `0x5BA057` | the executable's entry point | - | every run, from `kernel32` |

Seventeen entries, two of them (`_findfirst`, `_findnext`) not named in
`symbols.toml` yet. The plan's section 2.3 stands: `rand` reimplemented
exactly (every recipe and the `randlog` hang on its sequence), `sprintf`
over the formats the call sites use, the rest bound to our toolchain's,
start-up and the per-thread data the executable's until the cutover. The
decoder's I/O callback (`Mp3_MemoryIo`) calls `sscanf` through the runtime
from inside the decoder, which is the decoder's business.

## 4. The MP3 decoder

Entered through the eight named entries ours calls (`Mp3_Create`, `_SetIo`,
`_Open`, `_Start`, `_Seek`, `_Decode`, `_Destroy`, `_MemoryIo`), plus
`0x5B281C`, entered with caller 0 in every run - a thread's start routine,
the decoder's own thread. 76 of its 200 starts run on these routes; 59 more
are reachable from them; 62 are reached by nothing (dead code of the
library). Nothing to read here: the plan's section 2.4 and the owner's
listening set decide its fate.

## 5. What this changes in the plan

Section 1's table of the plan, with real numbers: of the 86 non-runtime,
non-decoder starts, **40 are alive under ours** (7 shell, 1 set-up, 17 PSX
library, 7 sound, 8 renderer handlers and helpers) and want a takeover
round of their own - about the size of one wave group, every one a leaf or
a thin wrapper over a Windows call; **35 are original-only** (the device
set-up, Capcom's walk and its software renderer's helpers - the last on the
condition of section 2's open question) and go at the cutover; **the five
are not code**; and six are unplaced (`0x59E930` and the software table's
status). The plan's step 2, "the small layers and the renderer's live
remainder", is those 40 with the handlers first, since one of them already
runs in a recorded fight.

## 6. Not verified

- The reach is eleven runs' worth. A function classed held may run on a
  route not recorded: exits, errors, a save written, the music stopped. The
  state hash on the new round will see those that a route reaches.
- The E8 / E9 scan is over bytes, not instructions.
- Whether ours can run with the software render flag set (section 2).
- `0x5B281C`'s identity as the decoder thread is from its caller being 0
  and its range; not read.

## 7. Corrected by step 2 (2026-10-05 night, [`platform-round.md`](platform-round.md))

- **Section 2's `0x587C20` was not original but held**: its callers
  `0x495C27` / `0x495D26` sit in game-mode 3's steps, which had no symbol
  because `pc_funcs.json`'s extent for `GameMode_Field` `0x4959F0` (2,139
  bytes) swallowed modes 3..6 and their nine steps. **Thirteen functions
  outside every catalogue**, taken as group PM; the 432 armed entries never
  included them, so no trace here saw them.
- **Section 2's open question on the software surfaces is answered**: the
  flag's only setter is inside Capcom's `Display_Setup` (ours since
  DIV-0031), so the 32 are original-only.
- **`0x59E930` is `Gfx_StoreImage`** (PSX library by nature); **`0x5A6830`
  is `Snd_Init`**, sound alone; of the shell's seven only `Disc_Probe` calls
  Windows.

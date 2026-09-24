# The DirectDraw / Direct3D set-up `0x5A5160`, its helpers, and the teardown

**Status:** IN PROGRESS (2026-09-23 - read to the last instruction, nothing
taken over, nothing in `symbols.toml` changed; the interface in §3 is what a
replacement backend must honour. §7 answers the coordinator's addendum on
`D3d_AfterDraw`.)

All addresses are `BOF3.exe`. Disassembly by capstone over the image
(`tools/pe_disasm.py`'s section mapping; a scratch copy that takes a start and
an end), the Ghidra decompiler over project `BoF3PC` for the control flow of
the COM-heavy parts, `analysis/pc_xref.json` (built 2026-09-18 by
`tools/pe_xref.py`) for who names each global, a raw scan of `.text` for
`E8`/`E9` rel32 for callers and for `68 imm32` for callback addresses, and a
scan of every function for `mov r, [G]; mov r2, [r]; call [r2 + slot]` for the
COM method table in §4. IIDs resolved from `.rdata` bytes; imports from the
IAT. Vtable slots are named from the DirectX 6 headers (`IDirectDraw4`,
`IDirectDrawSurface4`, `IDirect3D3`, `IDirect3DDevice3`, `IDirect3DViewport3`,
`IDirect3DMaterial3`) - a slot number is a fact, its name is the header's.

## 0. Extent

`analysis/pc_funcs.json` gives `0x5A5160` 3,722 bytes (to `0x5A5FEA`) because
`pe_funcs.py`'s recursive descent never sees the four enumeration callbacks
that follow it (`0x5A5BC0`, `0x5A5E40`, `0x5A5EA0`, `0x5A5F90` - reached only
by pointer, the blind spot `attract-remaining.md` §5 describes) and swallows
them. `attract-remaining.md`'s 2,656 is `0x5A5160..0x5A5BC0`, the next
function's start. **The body is `0x5A5160..0x5A5BB4`: 778 instructions, 2,645
bytes to the last `ret` at `0x5A5BB4`, then eleven `nop`s of padding to
`0x5A5BC0`.** Every jump inside stays inside (checked: no `jcc`/`jmp` target
outside `[0x5A5160, 0x5A5BB5)`); the sixteen `ret`s are one success exit
(`0x5A5BB4`, `eax` 0) and fifteen error exits, each `mov eax, code` (§2.12).

Relative calls out: `0x5ACBB6` (a thunk, `jmp [0x5C4000]` =
`DDRAW!DirectDrawEnumerateA`) at `0x5A52DD`; `0x5A5FF0` at `0x5A531F`;
`Dd_InitSurfaceDesc` `0x59F840` at `0x5A5509`; `Dd_ClearSurface` `0x5A2E70`
at `0x5A55FA` and `0x5A56EF`; `0x5A6380` (the teardown) at `0x5A5798`;
`0x5A60E0` at `0x5A585C`; `Dd_CreatePlainSurface` `0x59F860` at `0x5A5886`.
Imports: `GetDC`, `GetDeviceCaps`, `ReleaseDC`, `CoInitialize`. Sixty-three
indirect calls, all COM (the count `asset-loading-path.md` §2 gives).

## 1. Signature and callers

```
int __cdecl DisplaySetup(HWND hwnd, int *fullscreen, int *device, int *mode);
```

`hwnd` `[esp+0x128]`; `fullscreen` `[esp+0x12C]` (read and **written** -
forced to 1, §2.1 and §2.4); `device` `[esp+0x130]` (read and written - wrapped
to 0 when out of range, §2.4); `mode` `[esp+0x134]`, a pointer that every
caller passes as **0**, so the `mode` branch (`0x5A53B7..0x5A5464`, §2.6) is
dead code in this binary. Returns 0 on success, else one of the codes in
§2.12. Frame: `sub esp, 0x114`, `ebx ebp esi edi` saved.

Callers (raw `E8` scan of `.text`, three sites; `pc_funcs.json` attributes the
WndProc ones to `0x4FC6A0` because `pe_funcs.py` starts WndProc there - the
correction in `attract-remaining.md`):

| Site | In | Arguments | What it does with the result |
|---|---|---|---|
| `0x4FCD52` | WinMain `0x4FCB00` | `(hwnd 0x6BC620, &Cfg_Fullscreen 0x65DA44, &Cfg_RenderMode 0x65DA48, 0)` after both `Fmv_Play`s | non-zero: `0x5A6380(hwnd)` (teardown), `0x5A6720(code)` = `MessageBoxA(0, table[code], "ERROR", MB_ICONHAND)`, WinMain returns 1 |
| `0x4FC901` | WndProc `0x4FC6F0`, `WM_KEYDOWN` `0x77` (F8) | same, after `0x5A6380(hwnd)` at `0x4FC86F`, `Cfg_Fullscreen ^= 1`, and windowed a `SetWindowPos` (`0x4FC8EF`) | ignored; then `0x4FCAC0` (`ShowCursor` by `Cfg_Fullscreen`, cached in `0x65DA4C`) |
| `0x4FC94B` | WndProc, `WM_KEYDOWN` `0x76` (F7) | same, after `0x5A6380(hwnd)` at `0x4FC92F` and `Cfg_RenderMode += 1` | ignored; then `0x4FCAC0`, and `0x6BC62C = 0x5A6690(Cfg_RenderMode)`, `0x6BC630 = edi` (a timer - the on-screen device name, §5.2) |

The error strings are a pointer table at `0x66B6A8` indexed by the code
(`0x5A6720`, codes below 100; a second table `0x66B568` above): 1 "DirectDraw
Initial Error!", 2 "DirectDraw4 QueryInterface Error!", 3 "Direct3D3
QueryInterface Error!", 5 "SetCooperativeLevel to fullscreen Error!", 6
"CreateSurface flipping surface Error!", 7 "GetAttachedSurface Error!", 8
"SetCooperativeLevel to normal Error!", 9 "Create PrimarySurface Error!", 10
"Create BackSurface Error!", 11 "Create Clipper Error!", 13 "Create D3D Device
Error!", 14 "Setup Texture Format Error!", 15 "Create D3D Viewport3 Error!",
16 "Add D3D Viewport3 Error!", 17 "Set D3D Viewport3 Error!", 18 "Create D3D
Material Error!". Codes 4 ("Direct3D3 Device"), 12 ("Create ZBuffer") and 19
("Create D3D Light") exist in the table and are returned by nothing: **there
is no Z buffer and no light** in this set-up.

## 2. What it does, in order

### 2.1 The desktop (`0x5A5171..0x5A51C5`)

`GetDC(NULL)`; `GetDeviceCaps` `HORZRES` (8) → `0x7C9F44`, `VERTRES` (10) →
`0x6C9F40`, `BITSPIXEL` (12) x `PLANES` (14) → `0x6BE1E0` (the desktop's
bits per pixel); `ReleaseDC`. **If the desktop is below 16 bpp and
`*fullscreen` is 0, `*fullscreen` = 1** (`0x5A51BC..0x5A51C5`) - the first of
two places the config is overridden.

### 2.2 The caches, reset (`0x5A51CB..0x5A5243`)

`rep stosd` zero: `Gfx_TexCache` `0x6C3F40` (0x6000 bytes), `Gfx_ClutRows`
`0x6C2A40` (0x1000), `Font_TexCache` `0x7C9F50` (0xA00, then the first word of
each 0x14-byte entry set to `0xFFFF` - "no glyph"), the backdrop block
`0x6BE9F8` (0x1C), the after-draw block `0x7CADE8` (0x1C, §7),
`D3d_CellTexCache` `0x7CAE38` (0x1400), `Gfx_PixelFormat` `0x7DED60` (0xC0 -
three 0x40-byte records). Dwords: `Gfx_RenderFlags` `0x6C3A4C` = 0
(`0x5A5211` - **the whole dword, every bit, on every call**), `0x6C3A48` = 0
(written here, read by no instruction - `pe_xref`), `D3d_ShadeModeCache`
`0x6C3A44` = 0. None of the surfaces these caches point at is released here:
that is the teardown's job (§5.4), which every caller runs first.

### 2.3 The texture-coordinate table (`0x5A5249..0x5A5276`)

`D3d_TexCoords` `0x7CA9E0`, 256 floats: `tc[i] = i * [0x5C4640] +
[0x5C4638]` in x87 double, stored as float, with `[0x5C4640]` = 1/256 and
`[0x5C4638]` = 0.002 (bytes read: `0.00390625`, `0.0020000000949949026`).
That is the `(i + 0.512) / 256` of DIV-0010 and `known-defects.md` §1.

### 2.4 `CoInitialize(NULL)`, then the enumeration, once (`0x5A5278..0x5A52DD`)

`0x66B718` is the **device count minus one**, `-1` in the image (bytes at
`0x66B718`: `ff ff ff ff`). On the first call only: device record 0 is
written as the synthetic software renderer - `0x6C3A50` = 1 ("null GUID"),
`0x6C3A54` = 1 ("windowed allowed"), and its name `0x6C3A68..0x6C3A78` is the
four dwords at `0x66BC0C..0x66BC1C`, which are the bytes of **`"Software
Render\0"`**. That is the only reference to the string
(`launcher-settings.md`, `windowed-mode.md` were right that it is here and
did not know why): it is device 0's display name, not a mode switch. The
mode lists `0x6BE1E8` (0x810 bytes) are zeroed, `0x66B718` = 0, and
`DirectDrawEnumerateA(0x5A5BC0, NULL)` fills records 1.. (§6).

Per-device record, 0x13C bytes from `0x6C3A50 + i * 0x13C` (the `0x4F`-dword
stride in every access):

| Offset | Address (i = 0) | What |
|---|---|---|
| `+0` | `0x6C3A50` | 1 = create with a NULL GUID (the primary driver, or device 0's software), 0 = use `+8` |
| `+4` | `0x6C3A54` | 1 = may run windowed |
| `+8` | `0x6C3A58` | the DirectDraw driver GUID, 16 bytes |
| `+0x18` | `0x6C3A68` | driver description string, 0x28 bytes |
| `+0x40` | `0x6C3A90` | the HAL's `D3DDEVICEDESC`, 0xFC bytes (DirectX 6 size) |

Mode list, 0x204 bytes from `0x6BE1E8 + i * 0x204`: `+0` count, then up to
64 entries of 8 bytes at `+4`: u16 width `+0`, u16 height `+2`, u16 bpp
`+4`, u16 unused `+6`.

### 2.5 Which device (`0x5A52E2..0x5A53AF`)

`if (*device > 0x66B718) *device = 0;` - **`Cfg_RenderMode` is the index
into these records**: 0 is "Software Render", 1 the first DirectDraw driver
enumerated (normally the primary display), and so on; F7's `+= 1` cycles
through them with this wrap. `BOF3.CFG`'s line 2 default of 1 is therefore
"the first hardware device", which closes `windowed-mode.md`'s open question.
`*device == 0` sets **`Gfx_RenderFlags` bit 0** - the software renderer,
exactly what `d3d-draw.md`, `tex-page.md` and the rest read it as.

`0x5A5FF0(*device, &dd)` (§5.5): `DirectDrawCreate(NULL or &record.guid,
&dd, NULL)`; failure returns **1**. Then if `record.+4` (windowed allowed):
`Gfx_RenderFlags |= 2`. `dd->QueryInterface(IID_IDirectDraw4
{9c59509a-39bd-11d1-8c4a-00c04fd930c5} at 0x5C42D8, &Dd_DirectDraw 0x7CC334)`
(vtable `+0`; failure returns 2), `dd->Release()` (`+8`). Unless bit 0:
`Dd_DirectDraw->QueryInterface(IID_IDirect3D3
{bb223240-e72b-11d0-a9b4-00aa00c0993e} at 0x5C43A8, &0x7CC34C)` (failure 3).
**Then, if bit 2 is clear and `*fullscreen` is 0, `*fullscreen` = 1** - the
second override: a device that reported no windowed support is run fullscreen
whatever the config says.

### 2.6 Which mode, and the scale (`0x5A53AF..0x5A5509`)

With `mode == NULL` (always) the mode index is the local at `[esp+0x14]`,
initialised 0 at `0x5A516D`: **the first entry of the device's sorted list**,
i.e. its smallest mode of at least 640 x 480 x 16 (the callback's filter and
the `qsort`, §6). The dead branch for a non-null `mode` would wrap it to 0
when out of range and, windowed only, walk the list for an entry whose bpp
equals the desktop's, wrapping to 0 whenever the entry is not smaller than
the desktop in both dimensions - with no exit if no entry matches.

From the chosen entry: `Gfx_ScreenRect.right` `0x66B710` = width,
`Gfx_ScreenRect.bottom` `0x66B714` = height (the rect is `{0, 0, 640, 480}` in
the image and these two words are what the set-up overwrites), `0x7CADE0` =
bpp; **`D3d_ScaleX` `0x7C9F4C` = width x `[0x5C4630]` (1/320) and
`D3d_ScaleY` `0x7C9F48` = height x `[0x5C4628]` (1/240)**, `fild` / `fmul
qword` / `fstp dword`. So 2.0 is not a constant anywhere: it is 640/320. Then
`rep movsd` 0x3F dwords from the record's `+0x40` to `D3d_DeviceDesc`
`0x7CC238` - the current device's `D3DDEVICEDESC`, which `D3d_FitTextureSize`
`0x59F9B0` reads (`+0x84` `dpcTriCaps.dwTextureCaps`, `+0xAC/+0xB0` min
and `+0xB4/+0xB8` max texture size). Then `Dd_InitSurfaceDesc(&desc)` for the
0x7C-byte `DDSURFACEDESC2` at `[esp+0xA8]` that the next three creates share.

### 2.7 Windowed (`0x5A5518..0x5A564B`): `*fullscreen == 0` and bit 2

- `Dd_DirectDraw->SetCooperativeLevel(hwnd, DDSCL_NORMAL 8)` (`+0x50`;
  failure 8), `RestoreDisplayMode()` (`+0x4C`, result ignored).
- `CreateSurface(&desc, &DDraw_Primary 0x7CC338, NULL)` (`+0x18`; failure 9)
  with `dwFlags` 1 (`DDSD_CAPS`), `ddsCaps.dwCaps` `0x200`
  (`DDSCAPS_PRIMARYSURFACE`).
- `CreateSurface(&desc, &DDraw_BackBuffer 0x7CC33C, NULL)` (failure 10):
  `dwFlags` 7 (`CAPS | HEIGHT | WIDTH`), `dwWidth` = `0x66B710`, `dwHeight` =
  `0x66B714`, `dwCaps` = **`0x2040`** (`OFFSCREENPLAIN | 3DDEVICE`) or, under
  bit 0, **`0x840`** (`OFFSCREENPLAIN | SYSTEMMEMORY`) - the `neg / sbb / and
  0xFFFFE800 / add 0x2040` at `0x5A55A2..0x5A55B8`. Then
  `Dd_ClearSurface(DDraw_BackBuffer)`.
- `CreateClipper(0, &0x7CC348, NULL)` (`+0x10`; failure 11);
  `clipper->SetHWnd(0, hwnd)` (`+0x20`); `DDraw_Primary->SetClipper(clipper)`
  (`+0x70`).
- **`Gfx_RenderFlags |= 0x200`** (`or ah, 2` at `0x5A5643`).

`Gfx_WindowRect` is *not* written on this path - it keeps what `0x5A5130`
stored on the last `WM_MOVE` (§5.1).

### 2.8 Fullscreen (`0x5A5650..0x5A5750`): otherwise

- **`Gfx_RenderFlags |= 0x100`** first (`0x5A5662`).
- `SetCooperativeLevel(hwnd, 0x813)` = `DDSCL_FULLSCREEN 1 | EXCLUSIVE 0x10 |
  ALLOWREBOOT 2 | ALLOWMODEX 0x800` (failure 5).
- `SetDisplayMode(width 0x66B710, height 0x66B714, bpp 0x7CADE0, 0, 0)`
  (`+0x54`, the `IDirectDraw4` five-argument form; **result ignored**).
- `CreateSurface(&desc, &DDraw_Primary, NULL)` (failure 6): `dwFlags` `0x21`
  (`CAPS | BACKBUFFERCOUNT`), `dwBackBufferCount` 1, `dwCaps` `0x2218`
  (`PRIMARYSURFACE | FLIP | COMPLEX | 3DDEVICE`). `Dd_ClearSurface(primary)`.
- `DDraw_Primary->GetAttachedSurface(&{DDSCAPS_BACKBUFFER 4}, &DDraw_BackBuffer)`
  (`+0x30`; failure 7).
- `Gfx_WindowRect` `0x6BE1D0..0x6BE1DC` = `Gfx_ScreenRect` (four dwords, so
  `{0, 0, width, height}`).

Bit `0x200` is never set here and `0x100` never on the windowed path; the
teardown reads `0x100` to decide whether to restore the display mode.

### 2.9 The Direct3D device, with a retry (`0x5A5755..0x5A57A7`)

Skipped under bit 0. `IDirect3D3->CreateDevice(IID_IDirect3DHALDevice
{84e63de0-46aa-11cf-816f-0000c020156e} at 0x5C43E8, DDraw_BackBuffer,
&D3d_Device 0x7CC350, NULL)` (`+0x20`). **Only the HAL IID is ever asked
for** - no RGB, MMX, Ramp or Ref fallback exists in the binary (their IIDs
are not in `.rdata`; `0x5C4418` is `IID_IDirect3DNullDevice`, used by the
enumeration callback to skip that device). On failure: if `*fullscreen` was
already set, return 13; else `*fullscreen = 1`, `0x5A6380(hwnd)` (the full
teardown, which releases everything made so far), and **`jmp 0x5A52F0`** -
the whole of §2.5 onward runs again, now fullscreen. This is the "fullscreen
fallback" `windowed-mode.md` lists as unreproduced: a HAL device that refuses
the windowed back buffer flips the config to fullscreen for the rest of the
process (WinMain's later `Cfg_Fullscreen` reads see the 1).

### 2.10 Texture formats, the stage surface, the pitch (`0x5A585C..0x5A58C3`)

`0x5A60E0()` (§5.6) fills `Gfx_PixelFormat`; it returns 1 on every path, so
the return-14 exit at `0x5A5865` is unreachable. Then
**`Dd_CreatePlainSurface(0x140, 0x100, &Dd_StageSurface 0x7CC344, 0)`** - the
320 x 256 staging surface every page, glyph and cell builder locks
(`tex-page.md` §3; format record 0). Then `DDraw_BackBuffer->Lock(NULL, &desc,
DDLOCK_WAIT 1, NULL)` (`+0x64`), **`0x7DED5C` = `desc.lPitch`** (`+0x10`), and
`Unlock(NULL)` (`+0x80`) - the back buffer's pitch, read by the software
glyph and cell-sprite handlers `0x5A4900` / `0x5A4C40` and the software
rasterisers `0x5AA80F`, `0x5AA9EE`, `0x5AAB39`, `0x5AACE3`. Lock results are
not checked.

### 2.11 Viewport, material, states (`0x5A58C3..0x5A5BA8`) - not under bit 0

- `IDirect3D3->CreateViewport(&D3d_Viewport 0x7CC354, NULL)` (`+0x18`;
  failure 15); `D3d_Device->AddViewport(viewport)` (`+0x14`; failure 16).
- `D3DVIEWPORT2` at `[esp+0x6C]`, 11 dwords zeroed then: `dwSize` `0x2C`,
  `dwX` 0, `dwY` 0, `dwWidth` = `0x66B710`, `dwHeight` = `0x66B714`,
  `dvClipX` -1.0, `dvClipY` 1.0, `dvClipWidth` 2.0, `dvClipHeight` 2.0,
  `dvMinZ` 0, `dvMaxZ` 1.0. `viewport->SetViewport2(&vp)` (`+0x44`; failure
  17). `D3d_Device->SetCurrentViewport(viewport)` (`+0x30`).
- `IDirect3D3->CreateMaterial(&D3d_BackMaterial 0x7CC358, NULL)` (`+0x14`;
  failure 18). `D3DMATERIAL` at `[esp+0x1C]`: 0x14 dwords zero, `dwSize`
  `0x50`, `dwRampSize` `0x20` - black, no texture. `material->SetMaterial`
  (`+0xC`), `material->GetHandle(D3d_Device, &D3d_BackMaterialHandle
  0x6C3A40)` (`+0x14`), `viewport->SetBackground(handle)` (`+0x20`). Colour
  comes later from `D3d_SetBackColor` (`display-env.md`).
- `SetTextureStageState` (`+0xA0`), ten calls, in this order:

  | stage | state | value |
  |---|---|---|
  | 0 | 2 `COLORARG1` | 2 `D3DTA_TEXTURE` |
  | 0 | 3 `COLORARG2` | 0 `D3DTA_DIFFUSE` |
  | 0 | 1 `COLOROP` | 4 `D3DTOP_MODULATE` |
  | 0 | 5 `ALPHAARG1` | 2 `D3DTA_TEXTURE` |
  | 0 | 6 `ALPHAARG2` | 0 `D3DTA_DIFFUSE` |
  | 0 | 4 `ALPHAOP` | 4 `D3DTOP_MODULATE` |
  | 1 | 1 `COLOROP` | 1 `D3DTOP_DISABLE` |
  | 1 | 4 `ALPHAOP` | 1 `D3DTOP_DISABLE` |
  | 0 | `0x11` `MINFILTER` | 2 `D3DTFN_LINEAR` (`push 2` at `0x5A5B20`) |
  | 0 | `0x10` `MAGFILTER` | 2 `D3DTFG_LINEAR` (`push 2` at `0x5A5B33`) |

  The last two are the bytes `BOF3X_FILTER=point` patches (DIV-0012,
  `gfx_filter.cpp`).
- `SetRenderState` (`+0x58`), seven calls: `0xF` `ALPHATESTENABLE` = 1;
  `0x18` `ALPHAREF` = 8; `0x19` `ALPHAFUNC` = 5 `D3DCMP_GREATER`; `4`
  `TEXTUREPERSPECTIVE` = 1; `0x1D` `SPECULARENABLE` = 0; `0x16` `CULLMODE`
  = 1 `D3DCULL_NONE`; `0x1C` `FOGENABLE` = 0.
- **No `SetLightState`** (`+0x60`) anywhere in the function, and no
  `SetTransform`: the draw is `D3DTLVERTEX` throughout.

Return 0.

### 2.12 The exits

| Code | At | After |
|---|---|---|
| 1 | `0x5A57AC` | `DirectDrawCreate` failed (`0x5A5FF0` returned 0) |
| 2 | `0x5A57BC` | `QueryInterface(IID_IDirectDraw4)` |
| 3 | `0x5A57CC` | `QueryInterface(IID_IDirect3D3)` |
| 5, 6, 7 | `0x5A581C`, `0x5A582C`, `0x5A583C` | fullscreen: `SetCooperativeLevel`, flip-chain `CreateSurface`, `GetAttachedSurface` |
| 8, 9, 10, 11 | `0x5A57DC`, `0x5A57EC`, `0x5A57FC`, `0x5A580C` | windowed: `SetCooperativeLevel`, primary, back buffer, clipper |
| 13 | `0x5A584C` | `CreateDevice` failed and it was already fullscreen |
| 14 | `0x5A5865` | `0x5A60E0` returned 0 (cannot) |
| 15, 16, 17, 18 | `0x5A58E6`, `0x5A590C`, `0x5A59A0`, `0x5A59D7` | `CreateViewport`, `AddViewport`, `SetViewport2`, `CreateMaterial` |

`SetHWnd`, `SetClipper`, `RestoreDisplayMode`, `SetDisplayMode`, the two
`Dd_ClearSurface`s, `Lock`/`Unlock`, `SetCurrentViewport`, `SetMaterial`,
`GetHandle`, `SetBackground`, every state call: results dropped. A failed
`Lock` leaves `0x7DED5C` holding whatever the stack had.

## 3. Every global it writes

The interface. "Image" is the initial bytes in `.data` (initialised data ends
at `0x676000`; everything at `0x6BE...`/`0x6C...`/`0x7C...`/`0x7D...` is
`.bss`, zero at load).

| Address | Name (`symbols.toml`) | Written at | Holds after the call |
|---|---|---|---|
| `0x7C9F44` | - | `0x5A5187` | desktop width (`HORZRES`) |
| `0x6C9F40` | - | `0x5A5191` | desktop height (`VERTRES`) |
| `0x6BE1E0` | - | `0x5A51A4` | desktop bpp (`BITSPIXEL` x `PLANES`) |
| `0x6C3F40..0x6C9F40` | `Gfx_TexCache` | `0x5A51D7` | zeroed |
| `0x6C2A40..0x6C3A40` | `Gfx_ClutRows` | `0x5A51E3` | zeroed |
| `0x7C9F50..0x7CA950` | `Font_TexCache` | `0x5A51EF`, `0x5A51F6` | zeroed, word `+0` of each 0x14-byte entry `0xFFFF` |
| `0x6C3A4C` | `Gfx_RenderFlags` | `0x5A5211` = 0; `or` at `0x5A530F` (1), `0x5A534A` (2), `0x5A5643` (`0x200`), `0x5A5668` (`0x100`); `0x5A60E0` adds `0x20` | bit 0 software renderer (device 0); bit 1 device allows windowed; bit 5 no alpha texture format - colour key instead; bit 8 fullscreen; bit 9 windowed. Read as a dword, a byte, and `ah` by different readers |
| `0x6BE9F8..0x6BEA14` | - (`Dd_CellBackdrop` is `0x6BEA00`, hypothesis) | `0x5A5217` | zeroed; the teardown sets `0x6BE9FC` to `0x12345678` afterwards |
| `0x7CADE8..0x7CAE04` | (`D3d_AfterDrawRequest` is `0x7CADEA`) | `0x5A5223` | zeroed - §7's block |
| `0x7CAE38..0x7CC238` | `D3d_CellTexCache` | `0x5A522F` | zeroed |
| `0x7DED60..0x7DEE20` | `Gfx_PixelFormat` | `0x5A523B`; filled by `0x5A60E0` | three 0x40-byte records: 0 the alpha texture format, 1 the opaque texture format, 2 the screen's (§5.6) |
| `0x6C3A48` | - | `0x5A523D` | 0; no reader in `.text` |
| `0x6C3A44` | `D3d_ShadeModeCache` | `0x5A5243` | 0 |
| `0x7CA9E0..0x7CADE0` | `D3d_TexCoords` | `0x5A5273` | `(i + 0.512) / 256`, 256 floats |
| `0x66B718` | - | `0x5A52D1`, and `0x5A5BC0` increments | device count - 1; `-1` in the image = not yet enumerated |
| `0x6C3A50 + i * 0x13C` (0x13C x n) | - | `0x5A5293..0x5A52D7` (record 0), `0x5A5BC0` (the rest) | the device records of §2.4 |
| `0x6BE1E8 + i * 0x204` (to `0x6BE9F8`) | - | `0x5A52CA` zero, `0x5A5EA0` fills | the mode lists of §2.4 |
| `0x7CC334` | `Dd_DirectDraw` | `0x5A5362` (out of QI) | `IDirectDraw4*` |
| `0x7CC34C` | - | `0x5A5391` | `IDirect3D3*`; 0 under bit 0 |
| `0x66B710`, `0x66B714` | (`Gfx_ScreenRect` `+8`, `+0xC`) | `0x5A548A`, `0x5A54B0` | mode width, height (640, 480 seen) |
| `0x7C9F4C` | `D3d_ScaleX` | `0x5A54B9` | width / 320 |
| `0x7C9F48` | `D3d_ScaleY` | `0x5A54DC` | height / 240 |
| `0x7CADE0` | - | `0x5A54E2` | mode bpp; read only by this function (`SetDisplayMode`) |
| `0x7CC238..0x7CC334` | `D3d_DeviceDesc` | `0x5A5507` | the chosen device's `D3DDEVICEDESC` |
| `0x7CC338` | `DDraw_Primary` | `0x5A5583` / `0x5A56DE` | `IDirectDrawSurface4*` |
| `0x7CC33C` | `DDraw_BackBuffer` | `0x5A55E9` / `0x5A5717` | `IDirectDrawSurface4*`; windowed its own surface, fullscreen the flip chain's |
| `0x7CC348` | - | `0x5A5613` | `IDirectDrawClipper*`, windowed only |
| `0x6BE1D0..0x6BE1E0` | `Gfx_WindowRect` | `0x5A573A..0x5A5750`, fullscreen only | `{0, 0, width, height}`; windowed left to `0x5A5130` |
| `0x7CC350` | `D3d_Device` | `0x5A577D` | `IDirect3DDevice3*` (HAL); 0 under bit 0 |
| `0x7CC344` | `Dd_StageSurface` | `0x5A5886` (in `Dd_CreatePlainSurface`) | 320 x 256 plain surface, format record 0 |
| `0x7DED5C` | - | `0x5A58B0` | back buffer pitch in bytes |
| `0x7CC354` | `D3d_Viewport` | `0x5A58DF` | `IDirect3DViewport3*` |
| `0x7CC358` | `D3d_BackMaterial` | `0x5A59D0` | `IDirect3DMaterial3*` |
| `0x6C3A40` | `D3d_BackMaterialHandle` | `0x5A5A6E` | `D3DMATERIALHANDLE` |
| `*fullscreen` (`0x65DA44` `Cfg_Fullscreen`) | | `0x5A51C5`, `0x5A53A9`, `0x5A5792` | forced 1 by the desktop depth, the device, or the HAL retry |
| `*device` (`0x65DA48` `Cfg_RenderMode`) | | `0x5A52FD` | wrapped to 0 when past the last record |

Reads without a write, for completeness: `0x66B708`/`0x66B70C`
(`Gfx_ScreenRect` `left`/`top`, copied to the window rect), `0x66BC0C..`
(the name bytes), the four doubles at `0x5C4628..0x5C4648`.

Not in this function but part of the same interface because the same
callers pair them: `0x7CC340` (released by the teardown, written by nothing in
`.text` - a slot for a Z buffer that was never made), `0x6BC62C`/`0x6BC630`
(the device-name overlay, §5.2).

## 4. Who reads them

From `analysis/pc_xref.json` (absolute operands and `mov`/`push`
immediates; the scan cannot see an address reached through a register base),
each function that names the global, marked **ours** when its `symbols.toml`
entry has `impl`. The set-up itself and the teardown `0x5A6380` name almost
all of them and are omitted from each row.

| Global | Ours | Capcom's |
|---|---|---|
| `Dd_DirectDraw` `0x7CC334` | `Dd_CreatePlainSurface` `0x59F860`, `Dd_CreateTextureSurface` `0x59F900` (both `CreateSurface` `+0x18`) | none |
| `DDraw_Primary` `0x7CC338` | `Gfx_ClearPresent` `0x59ECE0` (`Blt`), `Gfx_Present` `0x59EDF0` (`Blt`, `Flip`, `Restore`) | `0x5A60E0` (`GetPixelFormat` `+0x54`) |
| `DDraw_BackBuffer` `0x7CC33C` | `Gfx_ClearPresent` (`Blt` fill), `Gfx_Present` (source of the `Blt`/`Flip`) | `D3d_AfterDraw` `0x59F580` (`Lock`/`Unlock`, §7); the software handlers `0x5A3A60` F3, `0x5A3D70` F4, `0x5A3FB0` G3, `0x5A41A0` G4, `0x5A4400` TILE, `0x5A4500` TILE_1, `0x5A4900` glyph, `0x5A4C40` cell sprite, and `0x5A3C40` (the textured ones' shared body, §5.3) - each one `Lock`/`Unlock` pair; `0x5A66B0` (`GetDC` `+0x44` / `ReleaseDC` `+0x68`, §5.2) |
| `Dd_StageSurface` `0x7CC344` | `D3d_BuildPageTexture` `0x5A0080`, `D3d_RefreshPageTexture` `0x5A0510`, `Font_BuildGlyphTexture` `0x5A2CA0`, `D3d_BuildCellTexture` `0x5A32B0`, `D3d_RefreshCellTexture` `0x5A37D0` (`Lock`/`Unlock`; `0x5A32B0` also `Blt`s into it) | none |
| clipper `0x7CC348`, `IDirect3D3` `0x7CC34C` | none | none |
| `D3d_Device` `0x7CC350` | `Gfx_DrawOTag` `0x59EE50` (`BeginScene`/`EndScene`), `D3d_SetBlend` `0x59FCA0` (11 x `SetRenderState`), `D3d_BindTexture` `0x59FFE0`, `D3d_DrawPolyF4` `0x5A0AB0`, `D3d_DrawPolyFT4` `0x5A0C40`, `D3d_DrawPolyG4` `0x5A1290`, `D3d_DrawPolyGT4` `0x5A14C0`, `D3d_DrawLineF2/F3/F4` `0x5A17A0`/`0x5A1A00`/`0x5A1D10`, `D3d_DrawTile` `0x5A20D0`, `D3d_DrawSprt/8/16` `0x5A2300`/`0x5A2520`/`0x5A2710`, `D3d_DrawGlyph` `0x5A2900` (+ one `SetRenderState`), `Font_GlyphTexture` `0x5A2BC0`, `D3d_DrawCellSprite` `0x5A2EB0`, `D3d_CellTexture` `0x5A3160`, `D3d_BuildCellTexture`, `D3d_RefreshCellTexture` (`DrawPrimitive` `+0x70`, `SetTexture` `+0x98`, `BeginScene`/`EndScene`) | `D3d_SetAlphaModulate` `0x59F520` (2 x `SetTextureStageState`); the Direct3D handlers not yet ours - `0x59FA50` POLY_F3, `0x59FDB0` POLY_FT3, `0x5A0E80` POLY_G3, `0x5A1050` POLY_GT3, `0x5A18B0` LINE_G2, `0x5A1B50` LINE_G3, `0x5A1EA0` LINE_G4, `0x5A2220` TILE_1 (each `DrawPrimitive`, most `SetTexture`); `0x5A60E0` (`EnumTextureFormats` `+0x20`) |
| `D3d_Viewport` `0x7CC354` | `Gfx_ClearPresent` (`Clear` `+0x30`), `D3d_SetBackColor` `0x5A5050` (`SetBackground`) | none |
| `D3d_BackMaterial` `0x7CC358`, handle `0x6C3A40` | `D3d_SetBackColor` | none |
| `D3d_ShadeModeCache` `0x6C3A44` | `D3d_SetBlend` | none |
| `Gfx_RenderFlags` `0x6C3A4C` | `Gfx_ClearPresent`, `Gfx_Present`, `Gfx_DrawOTag`, `Dd_CreatePlainSurface`, `D3d_BuildPageTexture`, `D3d_RefreshPageTexture`, `Font_GlyphTexture`, `Font_BuildGlyphTexture`, `D3d_CellTexture`, `D3d_BuildCellTexture`, `D3d_RefreshCellTexture` | `D3d_AfterDraw`, `0x5A60E0` |
| `Gfx_ScreenRect` `0x66B708` (and `+8`, `+0xC`) | `Gfx_ClearPresent`, `Gfx_Present` | `0x5A4C40` (software cell sprite, reads width and height), `0x5A5130` |
| `Gfx_WindowRect` `0x6BE1D0` | `Gfx_ClearPresent`, `Gfx_Present` | `0x5A5130` (writes) |
| `D3d_ScaleX` `0x7C9F4C`, `D3d_ScaleY` `0x7C9F48` | the fourteen ours in the device row that draw, plus `D3d_DrawTile`, `D3d_DrawGlyph`, `D3d_DrawCellSprite` (38 functions in all, 106 + 104 reads) | `D3d_AfterDraw`; the eight unowned Direct3D handlers above; every software handler `0x5A3A60`, `0x5A3B60`, `0x5A3D70`, `0x5A3E90`, `0x5A3FB0`, `0x5A40C0`, `0x5A41A0`, `0x5A42E0`, `0x5A4400`, `0x5A4500`, `0x5A45B0`, `0x5A46E0`, `0x5A47F0`, `0x5A4900`, `0x5A4C40` |
| `D3d_TexCoords` `0x7CA9E0` | `D3d_DrawPolyFT4`, `D3d_DrawPolyGT4`, `D3d_DrawSprt/8/16` | `0x59FDB0` POLY_FT3, `0x5A1050` POLY_GT3 |
| `D3d_DeviceDesc` `0x7CC238` | `D3d_FitTextureSize` `0x59F9B0` | none (the enumeration callbacks write it) |
| `Gfx_PixelFormat` `0x7DED60` | `Gfx_ConvertRow` `0x59EBB0`, `Gfx_ClutPixels` `0x5A04C0`, `D3d_BuildCellTexture`, `D3d_RefreshCellTexture`, `Tex_Convert4/8/16`, `Font_UnpackGlyph`, `Cell_Unpack4/8/4Flip/8Flip`, `Gfx_MoveCells`; `Gfx_PackRgb` `0x5AA79C` and `Dd_CreatePlainSurface`/`Dd_CreateTextureSurface` (record `+0x20` by index, through `0x7DED80`) | `D3d_AfterDraw` (`0x7DEDE3`, the screen's bytes per pixel), `0x5A0910`, `0x5A0A40`, `0x5A4900`, `0x5A4C40`, `0x5AA671`, `0x5AA9EE`, `0x5A60E0` |
| pitch `0x7DED5C` | none | `0x5A4900`, `0x5A4C40`, `0x5AA80F`, `0x5AA9EE`, `0x5AAB39`, `0x5AACE3` |
| `Gfx_TexCache`, `Gfx_ClutRows`, `Font_TexCache`, `D3d_CellTexCache`, backdrop block | the builders and finders listed in `tex-page.md`, `tex-cells.md`, `glyph-draw.md`; `Gfx_InvalidateTextures` `0x59E700` | `0x5A3CC0` (§5.3), `0x5A4900`, the teardown's helpers `0x5A64B0`..`0x5A65F0` |
| desktop `0x7C9F44`, `0x6C9F40`, `0x6BE1E0`; `0x66B718`; the device and mode records; `0x7CADE0`; `0x6C3A48` | none | only the set-up, `0x5A6690` (record `+0x18`), and the callbacks `0x5A5BC0`, `0x5A5EA0` |
| `Cfg_Fullscreen` `0x65DA44`, `Cfg_RenderMode` `0x65DA48` | none | `Cfg_Load` `0x4FD030`, WinMain, WndProc, `0x4FCAC0` |

**Lock or Blt from the primary / back buffer outside `src/`** (the
coordinator's question), from the method scan: nothing outside `src/` touches
`DDraw_Primary` except `0x5A60E0`'s `GetPixelFormat`. `DDraw_BackBuffer` is
`Lock`ed by `D3d_AfterDraw` and by the nine software handlers named in the
row above (they write pixels into it), and `GetDC`'d by `0x5A66B0`. No
function anywhere `Blt`s *from* either surface except `Gfx_Present` (ours,
back → primary) and `D3d_AfterDraw`'s CPU copy in §7.

## 5. The helpers, fully

### 5.1 `0x5A5130` - `WindowRect_Set(int x, int y)`, 46 bytes to `0x5A515D`

Twelve instructions, no calls, no branches. `Gfx_WindowRect` = `{x, y, x +
Gfx_ScreenRect.right, y + Gfx_ScreenRect.bottom}` (`0x6BE1D0`, `0x6BE1D4`,
`0x6BE1D8`, `0x6BE1DC`; `0x66B710`, `0x66B714` read). Sole caller WndProc at
`0x4FC78D` with `(LOWORD(lParam), HIWORD(lParam))` - `WM_MOVE`'s client
origin, so the rect is the client area in screen coordinates, the
destination of the windowed present. Its `symbols.toml` evidence line already
says this; nothing to add.

### 5.2 `0x5A6690` - `Device_Name(int i)`, 20 bytes to `0x5A66A3`

`return (char *)(0x6C3A68 + i * 0x13C);` - `lea ecx, [eax + eax*4]; shl 4;
sub eax; lea eax, [ecx*4 + 0x6C3A68]`. Not a re-initialisation: it returns
the address of device record `i`'s description string (§2.4), and F7 stores
it in `0x6BC62C` with a timer in `0x6BC630`. `0x5A66B0` (the next function,
48 instructions, called from WinMain's loop at `0x4FCE89` and `0x4FCEB0`) is
what shows it: `DDraw_BackBuffer->GetDC(&hdc)` (`+0x44`), `SetTextColor(hdc,
0xFFFFFF)`, `SetBkMode(hdc, TRANSPARENT 1)`, `TextOutA(hdc, x, y, str,
strlen(str))`, `ReleaseDC(hdc)` (`+0x68`) - GDI text drawn straight onto the
back buffer, the same helper `IDEAS.md` names for the frame counter.
`display-overhaul.md` §1 calls `0x5A6690` "F7's re-init" and
`windowed-mode.md` calls `0x5A66B0` a "flip"; both should say this.

### 5.3 `0x5A3CC0` - `Software_LockPage(u32 tpage, u32 clut, void **surface)`, to `0x5A3D62` (163 bytes)

`slot = tpage & 0x1F`, `mode = (tpage >> 7) & 3`. `i = Gfx_TexCacheFind(slot,
clut, mode)` `0x5A0830`; if `i == 0x20` (none) `i = D3d_BuildPageTexture(slot,
clut, mode)` `0x5A0080`; else if `Gfx_TexCache[slot * 32 + i].state == 2`
(byte `+0` of the 0x18-byte entry) `D3d_RefreshPageTexture(slot, i)`
`0x5A0510`. Then `*surface = entry + 0x10` (the page's plain surface under
bit 0), and `surface->Lock(NULL, &desc {dwSize 0x7C}, DDLOCK_WAIT 1, NULL)`
(`+0x64`), returning `desc.lpSurface` (`+0x24`). Result of the `Lock` not
checked; a build that fails returns 0 and the entry at index 0 is used
(`known-defects.md`'s note). Sole caller `0x5A3C40` (`0x5A3C7E`), which is the
shared body of the seven textured **software** handlers (called from
`0x5A3B60` FT3, `0x5A3E90` FT4, `0x5A40C0` GT3, `0x5A42E0` GT4, `0x5A45B0`
SPRT, `0x5A46E0` SPRT_8, `0x5A47F0` SPRT_16): it `Lock`s the back buffer,
calls this, runs the rasteriser `0x5AA80F(backBits, pageBits, 0x6C2A18)`,
`Unlock`s the page surface and the back buffer. None of it runs unless
`Cfg_RenderMode` is 0.

### 5.4 `0x5A6380` - the teardown, `0x5A6380..0x5A64A1`, `(HWND hwnd)`

Every caller of the set-up runs it first (WinMain on error and at exit
`0x4FD006`, WndProc before F7 and F8, and the set-up itself in the HAL
retry). In order: `0x5A64B0` (`Crt_free` `0x5B9577` every `Gfx_ClutRows`
row buffer `+4`, zero the pair), `0x5A64E0` (`Release` each `Gfx_TexCache`
entry's `+0x14` and `+0x10` for every slot whose entry 0 is live, zero the
entries), `0x5A6540` (the same for `Font_TexCache`'s `+8`/`+0xC`, word `+0`
back to `0xFFFF`), `0x5A6590` (`0x7CAE28`/`+8`, `0x7CAE24`, `0x7CAE20`),
`0x5A65F0` (the backdrop block's `0x6BEA04`/`+8`, `0x6BEA00`; zero it; then
**`0x6BE9FC = 0x12345678`**), `0x5A6760` (`D3d_FreeCellTexture(i)` for i in
0..127), `0x5A6650` (§7's block: `Release` `0x7CAE00`, `0x7CADFC`,
`0x7CADF8`; zero the 0x1C bytes). Then `Release` and null, in this order:
`D3d_BackMaterial`, `D3d_Viewport`, `D3d_Device`, `0x7CC34C`, the clipper,
`0x7CC340`, `Dd_StageSurface`, `DDraw_BackBuffer`, `DDraw_Primary`; then
`Dd_DirectDraw`: **if `Gfx_RenderFlags & 0x100`**, `SetCooperativeLevel(hwnd,
DDSCL_NORMAL)` and `RestoreDisplayMode()` first; `Release`, null. It does not
touch `Gfx_RenderFlags` itself - the next set-up zeroes it.

### 5.5 `0x5A5FF0` - `DirectDraw_Create(int device, IDirectDraw **out)`

`record.+0 ? DirectDrawCreate(NULL, out, NULL) : DirectDrawCreate(&record.+8,
out, NULL)`; returns 1 on `DD_OK`, else 0. `0x5ACBB0` is the import thunk
(`jmp [0x5C4004]`).

### 5.6 `0x5A60E0` - the texture formats, `0x5A60E0..0x5A6224`

Both paths start with `DDraw_Primary->GetPixelFormat(&ddpf {dwSize 0x20})`
(`+0x54`) and `0x5A62C0(1, &ddpf, record 2 = 0x7DEDE0)` - the screen's
format. Hardware: `D3d_Device->EnumTextureFormats(0x5A6230, NULL)` (`+0x20`);
the callback (§6) files 16-bit RGB formats into record 0 (with alpha) and
record 1 (without). Then: no record 1 → record 1 = record 0 with
`DDPF_ALPHAPIXELS` cleared (`0x7DEDC4 = 0x7DED84 & ~1`); no record 0 but a
record 1 → **`Gfx_RenderFlags |= 0x20`** and record 0 = record 1 (the
colour-key path of `Font_BuildGlyphTexture` and `D3d_BuildPageTexture`).
Software (bit 0): the screen format also becomes record `ddpf.dwFlags & 1 ?
0 : 1`, the other copied from it as above; a 24-bit screen has its record 0
rewritten to 4 bytes per pixel and 32 bits (`0x7DED63 = 4`, `0x7DED8C =
0x20`); then `0x5AA671()` picks the software rasteriser's pixel packer from
record 0's masks. Returns 1 always.

`0x5A62C0(rank, ddpf, record)`: only if `record[0] < rank` (a better format
replaces a worse: ranks 3 > 2 > 1 from the callback). Byte `+0` rank, `+1` =
1 when the red mask is `0x1F` or `0xFF` (BGR order), `+2` bpp, `+3` bytes
per pixel, dwords `+4/+8/+0xC` the shift of each mask's top bit from bit
23, `+0x10/+0x14/+0x18` the R/G/B masks, `+0x1C` the alpha mask, `+0x20` the
0x20-byte `DDPIXELFORMAT`. That is `Gfx_PixelFormat`'s record layout, which
`tex-page.md` §4 read from the consumers.

## 6. What the enumeration feeds in

Five callbacks and a comparator, all reached by pointer (the `push imm32`
sites: `0x5A52CC`, `0x5A5C93`, `0x5A5D74`, `0x5A5DB2`, `0x5A61B7`), all
`stdcall`:

- **`0x5A5BC0`** (`0x5A5BC0..0x5A5E2F`, `DDENUMCALLBACKA`; `ret 0x10`) runs
  once per DirectDraw driver: `DirectDrawCreate(guid, &dd, NULL)`; `GetCaps`
  (`+0x2C`, a 0x17C-byte `DDCAPS`) and requires **`DDCAPS_3D`** (bit 0 of
  `dwCaps`); QI `IDirectDraw4` then `IDirect3D3`; `EnumDevices(0x5A5F90,
  &found)` (`+0xC`); rejects the driver unless `found`, `dwMaxTextureWidth`
  (`D3d_DeviceDesc + 0xB4`) and `dwMaxTextureHeight` (`+0xB8`) are both at
  least 256, and `dwDeviceZBufferBitDepth` (`+0xA0`) is non-zero. Accepted:
  `++0x66B718`; the record of §2.4 (`+0` = 1 for a NULL GUID else the GUID
  copied to `+8` and `+0` = 0; `+4` = 1 only for the NULL-GUID driver and only
  if `dwDeviceRenderBitDepth` (`+0x9C`) has the desktop depth's `DDBD_` bit -
  `0x5A6050` maps bpp 1/2/4/8/16/24/32 to `0x4000..0x100` and everything else
  to 0; the description string `strcpy`'d to `+0x18`; `D3d_DeviceDesc` copied
  to `+0x40`); `EnumDisplayModes(0, NULL, NULL, 0x5A5EA0)` (`+0x20`); then
  `qsort` (`0x5B9B80`, by signature) of **every** device's mode list with
  `0x5A5E40`; release the three interfaces; return `DDENUMRET_OK`.
- **`0x5A5F90`** (`D3DENUMDEVICESCALLBACK`): skips `IID_IDirect3DNullDevice`
  (`0x5C4418`) and a device with no HAL description (`lpHALDesc->dwSize ==
  0`); on **`IID_IDirect3DHALDevice`** copies the HAL `D3DDEVICEDESC` to
  `D3d_DeviceDesc`, sets `*context = 1`, returns `D3DENUMRET_CANCEL`. So the
  desc the set-up copies is the HAL's, and a driver whose only device is RGB
  or MMX is never listed.
- **`0x5A5EA0`** (`EnumDisplayModes` callback, `DDSURFACEDESC2*`): keeps a
  mode only if width >= 640, height >= 480, `dwRGBBitCount` >= 16 and the
  device's `dwDeviceRenderBitDepth` has its `DDBD_` bit; appends `{w, h, bpp}`
  to the current device's list (at most 64). Additionally, **while device 1
  is being enumerated, every 640 x 480 mode is appended to device 0's list**
  - the software renderer inherits the primary driver's 640 x 480 modes and
  has no others.
- **`0x5A5E40`** (the comparator): ascending by width, then height, then bpp,
  as unsigned words - so entry 0 is the smallest qualifying mode, and with
  `mode == NULL` that is what the set-up uses (§2.6).
- **`0x5A6230`** (`EnumTextureFormats` callback): only `DDPF_RGB` at 16 bpp.
  Without `DDPF_ALPHAPIXELS`: red mask `0x7C00` rank 3, `0xF800` rank 2,
  `0xF00` rank 1, into record 1. With alpha: alpha mask `0x8000` (1555) rank
  2, `0xF000` (4444) rank 1, into record 0. Filed by `0x5A62C0`, higher rank
  wins - so the port prefers X1R5G5B5 over R5G6B5 for opaque textures and
  A1R5G5B5 over A4R4G4B4 for the alpha ones.

The set-up consumes: `0x66B718`, the records' `+0`, `+4`, `+0x40` and (via
`0x5A6690`) `+0x18`, and mode entry 0 of the chosen device. Nothing else the
callbacks write is read anywhere.

## 7. Addendum: `D3d_AfterDraw` `0x59F580`, read to its last instruction

`pc_funcs.json` folds it into `D3d_SetAlphaModulate` `0x59F520` (799 bytes)
because only `Gfx_DrawOTag`'s tail-jump reaches it; the body is
`0x59F580..0x59F83E` (0x2BF bytes, one `ret`, every jump internal). Frame
`0xA8`, four registers saved, no argument (as `symbols.toml` says).

1. `Gfx_RenderFlags` bit 0 → return.
2. First use (`0x7CADF8 == 0`): `0x7CADF4` = 1.0f, `0x7CADEC` (u16) = 320,
   `0x7CADEE` (u16) = 240; `Dd_CreatePlainSurface(0x140, 0x140, &0x7CADF8,
   2)` - a 320 x 320 plain surface in the **screen's** format (record 2);
   failure → return. `D3d_FitTextureSize(320, 240, &0x7CADF0, &0x7CADF2)` -
   rounded to the device's power-of-two / square / maximum rules from
   `D3d_DeviceDesc`; `Dd_CreateTextureSurface(w', h', &0x7CADFC, &0x7CAE00,
   2)` - a texture surface (`tex-page.md` §4 has the descriptor: flags
   `0x101007`, `DDSCAPS_TEXTURE`, caps2 `DDSCAPS2_TEXTUREMANAGE`; then QI
   `IID_IDirect3DTexture2` {93281502-8cf8-11d0-89ab-00a0c9054129} at
   `0x5C4488` into `0x7CAE00`); failure → return. If w' < 320: `0x7CADEC` =
   w', `0x7CADF4` = w' / 320 (`[0x5C460C]` = 0.003125), `0x7CADEE` = 240 x
   that.
3. Every time: `sx = ftol(D3d_ScaleX * 65536)`, `sy = ftol(D3d_ScaleY *
   65536)` (`[0x5C4608]` = 65536.0; `0x5B9550` is the CRT `ftol`) - 16.16
   steps. `Dd_InitSurfaceDesc`; **`0x7CADF8->Lock(NULL, &desc, DDLOCK_WAIT,
   NULL)`** then **`DDraw_BackBuffer->Lock(NULL, &desc2, DDLOCK_WAIT, NULL)`**
   (either failing → return, leaving the first locked). Then a CPU loop: for
   240 rows and 320 columns, `dst[row][col] = back[(v >> 16) * pitch][(u >>
   16)]` with `u += sx`, `v += sy`, 16-bit pixels when the screen record's
   bytes-per-pixel `0x7DEDE3` is 2 (`shr 15, and 0x1FFFE`), 32-bit otherwise
   (`shr 14, and 0x3FFFC`). **This is a read-back of the rendered back
   buffer**, point-sampled down from the scaled frame to 320 x 240 into the
   plain surface - the only CPU read of the back buffer in the binary.
   `Unlock` both (back buffer first).
4. `0x7CADFC->Blt({0, 0, w', h'}, 0x7CADF8, {0, 0, 320, 240}, DDBLT_WAIT
   0x1000000, NULL)` - the capture stretched into the texture surface if the
   device could not give a 320-wide texture. Then **`0x7CADE8` (u16) = 1**.

**It draws nothing**: no `DrawPrimitive`, no `SetTexture`, no device call at
all. It produces a texture of the frame just drawn and a "ready" flag. And
the consumer is missing: a raw dword scan of `.text` for `0x7CADE8`,
`0x7CADEC`, `0x7CADF0`, `0x7CADF4`, `0x7CADF8`, `0x7CADFC` and `0x7CAE00`
finds them only in this function, the set-up's zeroing at `0x5A521E`, and
the teardown's `0x5A6650`; `0x7CADEA` only in `Gfx_DrawOTag`. Unless some
handler reaches the texture through a computed address, the captured frame
is never bound or drawn - the feature (a PSX-style "previous frame as a
texture", which the `0xF4..0xF7` codes that request it would use for a
screen effect) was left unfinished in the port, or its consumer was compiled
out. Worth a live check: whether `D3d_AfterDrawRequest` is ever non-zero in
a run (`Gfx_DrawOTag` is ours; a counter costs one line), before anyone
spends time reproducing the read-back in a new backend. If it is never set,
the whole block (§3's `0x7CADE8..0x7CAE04`) is dead.

## 8. What this changes in earlier notes, and what is open

- `display-overhaul.md` §1: the DirectDraw interface is **`IDirectDraw4`**
  (not `IDirectDraw2`); `0x5A6690` is the device-name lookup, not a re-init;
  `D3d_ScaleX/Y` are mode / 320 and mode / 240, not a fixed 2.0.
- `windowed-mode.md`: `0x65DA48` is the device-record index (0 = software,
  1 = first hardware driver); `0x5A66B0` is GDI `TextOut` on the back
  buffer, not the flip. The "fullscreen fallback" is §2.9's HAL retry.
- `symbols.toml` `Cfg_RenderMode`'s evidence says "which value means which is
  not read" - it is now (not edited here, per the task).
- Open: whether `SetDisplayMode`'s ignored result ever bites (a driver that
  refuses 640 x 480 x 16 leaves the desktop mode with a flip chain created
  against it); whether device 0's inherited mode list is ever non-empty on a
  machine whose primary driver lacks `DDCAPS_3D` (then device 0 is the only
  record and `Cfg_RenderMode` 1 wraps to it - the software renderer, silently);
  the `0x5A0910` / `0x5A0A40` / `0x5AA671` / `0x5AA9EE` readers of
  `Gfx_PixelFormat`, unread here beyond their names' absence.

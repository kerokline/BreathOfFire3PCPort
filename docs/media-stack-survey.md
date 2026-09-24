# The media stack: what the port uses to draw, play and decode

**Status:** STABLE for the inventory, DRAFT for the recommendations (verified 2026-09-19)

What audio/video technology `BOF3.exe` actually depends on, which parts of it
still exist on Windows 11, and what replacing each part would cost. One item has
since been acted on (§6, `DIVERGENCE.md` DIV-0001); anything else built from §5
needs its own ledger entry first.

**Since then (noted 2026-09-24):** video out is replaced - Direct3D 11 behind
DirectX 6's objects, no exclusive mode (DIV-0031); the FMV player's second
DirectDraw and its mode-set are gone, the videos playing through MCI into the
window (DIV-0035); input goes through SDL3 for the pad, the keyboard kept as
the original read it (DIV-0050, [`controls.md`](controls.md)). Sound and the
MP3 decoder are still Capcom's. The inventory below is the original's.

## 1. The inventory

| Layer | What the port uses | API vintage | On Win11 24H2 (this machine) |
|---|---|---|---|
| Video out | `DDRAW.dll` — `DirectDrawCreate`, `DirectDrawEnumerateA` | DirectDraw 1 entry points, DX7-era interfaces | present, emulated |
| 3D | `IDirect3D3` / `IDirect3DDevice3` / `IDirect3DViewport3`, obtained by `QueryInterface` off DirectDraw | DirectX 6 (1998) | present via `d3dim700.dll` |
| Input | `DINPUT.dll` — `DirectInputCreateA` | DirectInput 7 (version `0x700`; "3" in the first draft, corrected 2026-09-24 from [`controls.md`](controls.md) §1), ANSI | present |
| SFX / voice | `DSOUND.dll` ordinal 1 = `DirectSoundCreate` | DirectSound 1 | present, WASAPI-emulated |
| FMV | `WINMM.mciSendStringA` → `open avivideo!<file> alias vfw` | MCI + VFW (1992) | driver present, **one codec missing** |
| Music | MP3, decoded **inside `BOF3.exe`** | ISO-style MPEG-1 decoder | no OS dependency |

**Evidence.** Full import table of `BOF3.exe` (own PE import parser,
2026-09-19): exactly four multimedia DLLs — `DDRAW.dll`, `DSOUND.dll`,
`DINPUT.dll`, `WINMM.dll` — plus `USER32`/`GDI32`/`ole32`/`KERNEL32`. No
`msvfw32`, no `msacm32`, no `dplay`, no `d3d8`/`d3d9`. The only dynamically
named module in the string table is `user32.dll`. `START.EXE` is a launcher: its
imports are `KERNEL32` only and it reaches the game through `WinExec`.

`IDirect3D3` is inferred from the error strings `Direct3D3 QueryInterface
Error!`, `Create D3D Device Error!`, `Create/Add/Set D3D Viewport3 Error!`,
`Create ZBuffer Error!`, `Setup Texture Format Error!` (string sweep of
`.rdata`). The port renders through DirectDraw surfaces with a Direct3D
immediate-mode device attached — the 1998 idiom, not the later `d3d8`/`d3d9` one.

## 2. FMV — the one thing that is actually broken

Two videos ship, both 640x480, both played through the MCI `avivideo` device
(`open avivideo!%s alias vfw` / `play vfw window from 0 notify` / `stop vfw
wait`, plus `window vfw handle %d`; strings in `.rdata`, alongside the literals
`CAPCOM.AVI` and `LOGOS.AVI`).

| File | Video | Audio | Size |
|---|---|---|---|
| `capcom.avi` | `IV50` — **Indeo Video 5**, 24-bit | PCM 22.05 kHz 16-bit stereo | 2.2 MB |
| `logos.avi` | `CRAM` (`MSVC`) — Microsoft Video 1, 16-bit | none | 3.9 MB |

**Evidence:** RIFF/`strh`/`strf` headers read directly (`xxd -l 512`).
`capcom.avi` `strf` gives `biCompression = 'IV50'`, `biBitCount = 24`; its audio
`strf` is `wFormatTag=1, nChannels=2, nSamplesPerSec=22050, wBitsPerSample=16`.
`logos.avi` `strh` fccHandler `MSVC`, `biCompression = 'CRAM'`, `biBitCount=16`,
single stream.

**Indeo 5 is gone, and the failure is silent.** Confirmed by driving the exact
MCI sequence `BOF3.exe` issues, from a 32-bit host, against a window we own
(`scratchpad/mcishot2.ps1`, 2026-09-19):

- `ir50_32.dll` and `ir32_32.dll` are absent from `C:\Windows\SysWOW64`, and
  `HKLM\SOFTWARE\WOW6432Node\…\Drivers32` has **no `vidc.iv50`** entry. It does
  have `vidc.cvid` → `iccvid.dll` (Cinepak) and `vidc.msvc` → `msvidc32.dll`
  (Video 1). Indeo was dropped from Windows after XP and blocked outright in
  security advisory 954157 after remote-code-execution bugs in its parsers; a
  third-party revival re-introduces exactly the parser that was blocked.
- **Every MCI command returns success anyway.** `open avivideo!capcom.avi alias
  vfw` → `OK`, `status vfw length` → `113`, `play … notify` → `OK`, and the
  position advances to 113 and reaches `stopped` on schedule. Nothing in the
  return codes tells the caller anything is wrong.
- What is actually drawn is `mciavi32`'s own placeholder: a white field of thin
  diagonal hatching, captioned **"Video not available, cannot find 'vids:iv50'
  decompressor."** (screenshot `scratchpad/w_indeo.png`).

So the earlier prediction that the game would *error* was wrong in a way that
matters: **no return-code check anywhere in `BOF3.exe` can catch this.** The
player gets a hatched rectangle with an English error caption where the Capcom
logo should be, for the full 7.5 seconds, and the game proceeds normally.
Whether the PCM audio track still plays underneath is untested.

`logos.avi` needs `vidc.msvc`, which is registered, so it is expected to play —
and the same harness playing a Video-1-class file renders real frames, so the
harness itself is not the reason `capcom.avi` shows nothing.

**The playback code itself is one function**, `Fmv_Play` `0x59E360`, with two
call sites in `0x4FCB00` — and it does something that matters well beyond FMV:
before playing, `Fmv_EnterFullscreen` `0x59E4F0` creates a *second* DirectDraw
object and performs an exclusive-fullscreen `SetDisplayMode(640, 480, 16)`.
Full anatomy in [`replacing-mci.md`](replacing-mci.md) §2; symbols in
[`symbols.toml`](../symbols.toml).

## 3. Audio — not broken, and self-contained

`BGM/*.DAT` are **bare MP3 streams**, no container and no header: the first
bytes of `BGM/000.dat` are `FF FB 92 04` — MPEG-1 Layer III, 128 kb/s,
44.1 kHz, stereo. 166 files, dated 2000-07-16.

`SND/*.DAT` are **RIFF WAVE**, PCM 22.05 kHz 16-bit mono (`SND/004_00.dat`
`fmt ` chunk: tag 1, 1 channel, 22050, 16-bit). 893 files. This agrees with
[`DAT_CONTAINER.md`](DAT_CONTAINER.md), which found the porting house decoded
the PSX ADPCM banks to WAV.

The MP3 decoder is **statically linked into `BOF3.exe`**, which is why no ACM or
MCI MPEG dependency appears in the imports. The complete ISO bitrate matrix sits
at file offset `0x1c6c20` in `.rdata` — five rows of 16 `u32`s: MPEG-1 layers
I/II/III then MPEG-2 layers II/III, ending `...320, 384, 0` / `...256, 320, 0` /
`...144, 160, 0` — followed at `0x1c6d30` by the three sample-rate families
`{44100,48000,32000}`, `{22050,24000,16000}`, `{11025,12000,8000}`. Found by
searching the image for those value sequences packed as `u32` (Python,
2026-09-19).

**Consequence: the audio stack has no platform rot at all.** DirectSound 1 is
emulated over WASAPI and keeps working; the decoder is the port's own code and
will keep working for as long as the x86 does. Nothing here needs updating for
compatibility. The one *quality* note is that the BGM is a lossy 128 kb/s
re-encode of the disc's CD audio — a fidelity ceiling, not a bug, and one the
living-game mandate could lift later by re-ripping from a player-supplied disc.

## 4. Video out — the real long-term liability

DirectDraw and legacy Direct3D still load on Windows 11, but they are a
compatibility shim (`d3dim700.dll`) over the modern driver stack, not a
supported path. The known failure modes of this exact vintage — exclusive
fullscreen fighting the desktop compositor, mode-set flicker, broken alt-tab,
surface-lost handling, 16-bit surface formats no driver wants — are precisely
the "fullscreen fallback, resolution handling" defects already named in
[`STATUS.md`](STATUS.md) step 0. This is where the port eventually breaks for
good, and the plan already routes through it: replacing the presentation layer
is what renderer work in [`PLAN.md`](PLAN.md) is for.

**There are two independent DirectDraw users here, not one.** Before either logo
video plays, the port creates its *own second* DirectDraw object and does an
exclusive-fullscreen `SetDisplayMode(640, 480, 16)` (`Fmv_EnterFullscreen`
`0x59E4F0`, disasm 2026-09-19) — separate from the renderer's. Anyone reading
the display path has to account for both.

**And it currently works.** The game launches, reaches the start screens and
plays the intro level on this machine (owner, 2026-09-19), so the legacy path
including that exclusive mode-set is functional today. This section is about a
fragile foundation on a deprecated shim, not an outage: it is
[`IDEAS.md`](IDEAS.md) I8, gated on [`PLAN.md`](PLAN.md) phase 3, where the
platform layer is already the first decompilation target. The renderer's own
DirectDraw usage has **not** been read yet, and its size is unknown — the FMV
path was small and self-contained, and that should not be assumed of the rest.

`DINPUT.dll`'s `DirectInputCreateA` is DirectInput 7 (called with version
`0x700`, `DInput_Init` `0x5A94C0`; this said 3 until 2026-09-24) and still resolves, but
DirectInput has been deprecated for two decades and does not see XInput
controllers as anything modern; gamepad support is a likely casualty.

## 5. Is any of it worth updating?

Ranked by (player-visible breakage) / (cost), highest first.

1. **`capcom.avi` — yes; done, tested and installed (§6).** Re-encoding to
   Cinepak restores the video through the port's own unmodified playback path:
   no code change, one file, one ffmpeg command.
2. **Replace the MCI path with a real decoder — later, and with the renderer.**
   Costed in [`replacing-mci.md`](replacing-mci.md); filed as
   [`IDEAS.md`](IDEAS.md) I7, LOW. The surface is one function, `Fmv_Play`
   `0x59E360`, and it is modal and blocking, so this is smaller than it looks —
   but with DIV-0001 in place there is no player-visible gain left to collect,
   and the mode-set it would remove belongs to the renderer phase anyway.
3. **Video out — yes, but it is a phase, not a patch.** Already the plan's job.
   Do not reach for a DirectDraw wrapper as a shortcut: `d3dim700.dll` already
   is one, and a second shim layer makes failures harder to attribute.
4. **Audio — no.** Nothing is broken and nothing is going to break. Touching the
   in-binary MP3 decoder is pure risk with no player-visible return. Re-ripping
   BGM at higher quality is a *content* idea, not a compatibility one.
5. **DirectInput — maybe, cheaply, when someone wants a controller.** A feature,
   not a compatibility fix.

## 6. The Cinepak re-encode, measured

Encoded 2026-09-19 with ffmpeg 9.0.1 (installed for this, `winget install
Gyan.FFmpeg`):

```bash
ffmpeg -i capcom.avi -c:v cinepak -c:a copy -fflags +bitexact -flags:v +bitexact capcom_cvid.avi
```

`-pix_fmt yuv420p` is rejected — the encoder auto-selects `rgb24`, which is what
Cinepak wants. Audio is passed through untouched, so the PCM track is bit-exact.

| | Indeo 5 (original) | **Cinepak** | Microsoft Video 1 |
|---|---|---|---|
| Size | 2.2 MB | 4.6 MB | 3.7 MB |
| SSIM vs original | — | **0.9898** | 0.9576 |
| Encode time | — | 66 s | 1.4 s |
| Plays through MCI here | **no** | **yes** | (not run; `vidc.msvc` registered) |

Cinepak is the clear quality choice; Video 1 is 16-bit and visibly worse. Per-frame
PSNR on the busiest frames is ~42 dB (`ffmpeg -lavfi psnr/ssim`, full clip). The
source helps: it is a dark particle logo, and Indeo's `yuv410p` chroma was
already coarse, so there is less for Cinepak to lose than the codec's reputation
suggests.

The cost is size — 2x the original, for a 4.6 MB file. Irrelevant on disk;
worth knowing if the file ever has to fit a CD layout again.

**Playback verified**, same harness and same MCI command sequence as §2: real
frames render (`scratchpad/w_cinepak.png`), position advances, `stop`/`close`
clean.

**Installed** 2026-09-19 as [`DIVERGENCE.md`](DIVERGENCE.md) **DIV-0001**. The
local `bof3/capcom.avi` is now the Cinepak encode and the shipped original is
kept beside it as `capcom.avi.indeo5`; both, and `logos.avi`, are hashed into
[`fixtures.toml`](../fixtures.toml) under `pc-zh`. Re-tested at the real install
path after installing, and it plays.

## 7. Open items

- Confirm end-to-end **in the running game**, not just through the MCI harness.
  This is the last unverified link in DIV-0001 and it folds into step 0 of
  [`STATUS.md`](STATUS.md) at no extra cost.
- Check whether the PCM audio track plays under the failed Indeo video. Cosmetic,
  but it decides how bad the un-fixed state actually looks.
- Identify the linked MP3 decoder's lineage (ISO dist10? Xing?) from the tables
  around `0x1c6c20`. Low value now; relevant only if it is ever reimplemented.

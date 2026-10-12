# The disc's logo movie, converted at import

**Status:** MEASURED (2026-10-12, the disc-only round's group FM,
[`disc-only-round.md`](disc-only-round.md); all five PSX discs read, the
conversion run from the US disc and opened through Video for Windows and MCI
in a 32-bit process; not played in the game, not heard)

The port plays two AVIs through MCI (`Fmv_Play` `0x59E360`,
`src/game/fmv_play.cpp`; DIV-0001, DIV-0035, DIV-0042): `CAPCOM.AVI`, then
`LOGOS.AVI`. A disc-only install has neither. A PlayStation disc carries the
first as an MDEC stream; the owner decided on 2026-10-12 that the player's
ffmpeg converts it at import into the AVI the engine already plays, with no
decoder in the engine. `importer.py build` now does that from any PSX disc.
`LOGOS.AVI` (the porting house's logos) has no disc counterpart and is not
this doc's (the owner's question, `disc-only-round.md` section 2).

## 1. The movie on the discs

`LOGO/CAPCOM30.STR`, the one MDEC stream on the disc (the read-only survey of
2026-10-12 scanned the JP image for the MDEC header; `MAGIC00.STR`,
`S_XA00.STR` and `VOICE.STR` are XA audio only). Found by name with
`tools/psx_disc.py` and read as raw sectors with `tools/xa.py`'s `sectors()`
(`fm_scratch/scan.py`, `frames.py`, 2026-10-12):

| Disc | LBA | Sectors | sha256 of every sector's bytes 16..2351 |
|---|---:|---:|---|
| USA | 186,252 | 1,155 | `c496080b...12e915` |
| Japan | 186,024 | 1,155 | the same |
| Europe | 186,196 | 1,155 | the same |
| France | 186,158 | 1,155 | the same |
| Germany | 186,220 | 1,155 | the same |

**Every PSX disc carries it, and the sectors are identical** from the
subheader on; only the 16-byte sync and address header differs, because the
file sits at a different LBA on each. The hash is held as
`FMV_SOURCE_SHA256` in `tools/importer.py` (a hash, no bytes).

Its contents (the sector headers read directly):

- **Video:** 1,011 sectors (subheader channel 1, submode `0x42`; the last
  `0xC2`, end of file), MDEC header `60 01 01 80`, 320 x 240, frames 1..231,
  bitstream version 2 (the survey's reading). A new frame starts every 5
  sectors on average (174 gaps of 5, 28 of 4, 28 of 6; 1,155 / 231 = 5.0).
- **Audio:** 143 XA sectors on channel 1, one every 8 sectors, stereo
  37.8 kHz 4-bit; plus **one channel-0 sector at index 1,152, mono and
  silent** (`xa.decode`: 4,032 samples, peak 0).
- **Timing:** stereo 37.8 kHz XA one sector in eight is the double-speed
  rate, 150 sectors a second. So a frame every 5 sectors is **30 frames a
  second**, 231 frames are 7.70 s, and the audio is 143 x 2,016 / 37,800 =
  7.627 s.

ffmpeg 9.0.1's `psxstr` demuxer reads the file as two streams (`mdec`
320 x 240, `adpcm_xa` 37.8 kHz stereo) but **stamps the video at 15 fps**, a
fixed rate of its own, not the stream's; the conversion gives `-r 30` on the
input. It also opens the channel-0 sector as a third stream (index 2, "New
audio stream ... at pos:-1"); the conversion maps only the first video and
first audio stream, so that silent sector is dropped.

## 2. The PC's own file

`ffprobe` of the player's install, 2026-10-12:

| File | Container | Video | Size | Rate | Frames | Audio | Length |
|---|---|---|---|---|---:|---|---:|
| `capcom.avi` (DIV-0001's re-encode) | AVI | Cinepak `cvid`, rgb24 | 640 x 480 | 15 fps | 112 | PCM s16le 22,050 Hz stereo | 7.467 s |
| `capcom.avi.indeo5` (as shipped) | AVI | Indeo 5 `IV50` | 640 x 480 | 15 fps | 113 | PCM s16le 22,050 Hz stereo | 7.533 s |
| `logos.avi` | AVI | MS Video 1 `CRAM`, rgb555 | 640 x 480 | 14.98 fps | 120 | none | 8.009 s |

Indeo 5 has no decoder on any current Windows; Cinepak does (`iccvid.dll`,
`vidc.cvid`), and ffmpeg encodes it (DIV-0001, `media-stack-survey.md`
section 6).

## 3. The encoding, and why

```bash
ffmpeg -v error -y -f psxstr -r 30 -i CAPCOM30.STR.tmp -map 0:v:0 -map 0:a:0 \
       -vf scale=640:480:flags=neighbor -c:v cinepak -c:a pcm_s16le -ar 44100 \
       -fflags +bitexact -flags:v +bitexact -f avi base/fmv/CAPCOM.AVI
```

(`FMV_ARGS` in `tools/importer.py`; the `.tmp` is the 1,155 raw 2,352-byte
sectors as read, removed after.)

- **Cinepak, as DIV-0001 made the PC's file.** It is the codec the port's
  path is already proven on, Windows still ships its decoder, and ffmpeg
  writes it. Nothing new is asked of the player's machine.
- **640 x 480, each pixel doubled (`neighbor`).** The PC file's size. The
  engine's DIV-0035 / DIV-0042 rectangle is a whole multiple of 640 x 480, so
  at 1x MCI draws the frame unscaled and at k x by a whole factor: the
  doubling is done once, at import, by a known filter, rather than by
  whatever stretch `mciavi32` / DrawDib and the display driver choose for a
  320 x 240 source. Pixel doubling is the disc's picture with nothing
  invented (the owner prefers the sharp look, `BOF3X_FILTER=point`). The cost
  is size and time: 9.2 MB, and the encode takes about two minutes (2 m 1 s
  alone, 2 m 42 s for the whole US build beside five other agents' builds;
  Cinepak's encoder is single-threaded).
- **30 frames a second**, the stream's own timing (section 1), not the PC
  file's 15: the disc's movie has twice the PC's frames and both are kept.
- **PCM s16le, stereo, 44.1 kHz.** PCM as the PC's file. The rate is the
  XA's 37.8 kHz resampled by ffmpeg to the common rate every wave device
  takes; the PC's 22.05 kHz would cut the disc's audio above 11 kHz.
- **`+bitexact`**: the same ffmpeg writes the same file. The manifest
  records which ffmpeg it was.

**The result** (`ffprobe` of `cache-us-fm/base/fmv/CAPCOM.AVI`): AVI,
Cinepak `cvid` 640 x 480 rgb24, 30 fps, 231 frames, 7.700 s; PCM s16le
44,100 Hz stereo, 336,336 samples (7.627 s); 9,183,260 bytes, sha256
`264c7c74...f48de6` with ffmpeg 9.0.1 (gyan.dev full build). Against the
stream decoded directly by the same ffmpeg and doubled the same way, Cinepak's
loss measures **SSIM 0.970** over all 231 frames (R 0.965, G 0.976, B 0.969;
`-lavfi ssim`); DIV-0001's re-encode of the PC's Indeo file measured 0.990.
The disc's source is busier at twice the frame rate, so Cinepak loses more.

Sync: ffmpeg starts both streams at 0. On the console the first audio sector
is the 9th of the file (index 8, 53 ms in at 150 sectors a second) and the
first frame shows after its 5 sectors (33 ms) and a decode; the difference is
about 20 ms and left as ffmpeg makes it.

## 4. The importer

`tools/importer.py`, section "the disc's movie":

- **`build`** (`fmv_build`, called after the manifest is written): from the
  first PSX disc among the sources, reads `LOGO/CAPCOM30.STR`'s raw sectors,
  checks their hash against `FMV_SOURCE_SHA256`, converts with the player's
  ffmpeg, found as the effects path finds it (`at3.find_ffmpeg`:
  `BOF3X_FFMPEG`, else the `PATH`'s), into `base/fmv/CAPCOM.AVI`, and appends
  an `[fmv]` table to `manifest.toml`: `build`, `source`, `source_sha256`,
  `ffmpeg` (the version line), `file`, `sha256`. A stale `CAPCOM.AVI` from an
  earlier build into the same cache is removed first. **No ffmpeg:** the row
  is skipped with a logged line, as the effects path does (`base/fmv  not
  written: no ffmpeg on the PATH or in BOF3X_FFMPEG: ...`), and the `[fmv]`
  table records `skipped` with that reason (the effects path writes no table;
  this one does, so that `verify` can say why). An ffmpeg that fails is
  skipped the same way with the tail of its error. No PSX disc among the
  sources (a PC or PSP-only build): nothing, no table.
- **`verify`** (`fmv_verify`): `base/fmv/: CAPCOM.AVI from psx-us (ffmpeg
  ...); present, 640 x 480, 231 frames, cvid, as recorded` - the file's hash
  against the manifest, the source hash against `FMV_SOURCE_SHA256`, and the
  AVI's `avih` / `strh` read for size, frames and codec; or `base/fmv/:
  absent - <the reason>` (no PSX disc, no ffmpeg, the ffmpeg's error). Absent
  is not a problem; a `CAPCOM.AVI` the manifest does not name is.
- **`install`** (`fmv_install`): copies `base/fmv/CAPCOM.AVI` into the game
  directory, where `Fmv_Play` opens it (the working directory first, then the
  disc root), **unless the directory already holds a `capcom.avi` under any
  case** - the PC's, which is kept, and the line says so. Note: `Disc_Probe`
  (`src/game/shell.cpp`, original `0x5A72C0`) also opens `CAPCOM.AVI` in the
  game directory as its "this is a local install" test before it looks for a
  CD drive; what a disc-only install does without the file is IN's.

`recipes/pc-zh.toml` is unchanged: the AVI is not a DAT container.

## 5. What was verified, and what was not

Verified (2026-10-12):

- All five PSX discs: the file found by name, 1,155 sectors each, identical
  from the subheader on (section 1).
- `importer.py build --source "<US cue>" --out fm_scratch/cache-us-fm`:
  `base/fmv  from psx-us: CAPCOM.AVI`; the file byte-identical to a separate
  hand run of the same command. `verify` on it: the line above. The other
  rows unchanged (741 `base/` containers, `base/exe`, `base/snd`, `base/bgm`).
- The no-ffmpeg, no-disc and install paths, on scratch directories with
  `at3.find_ffmpeg` returning `None` (`fm_scratch/paths.py`): the skip line,
  the `skipped` row, the stale file removed, `verify` absent with the reason
  (0); no disc, absent (0); install into an empty directory writes
  `CAPCOM.AVI`, into one holding a `capcom.avi` keeps it.
- **The engine's path, outside the engine**, from a 32-bit PowerShell
  (`SysWOW64`, so the 32-bit codecs the game sees; `fm_scratch/vfw.ps1`):
  Video for Windows (`AVIStreamOpenFromFileW`, `AVIStreamGetFrameOpen`)
  finds a decompressor and **decodes 231 of 231 frames** to 640 x 480 x 24;
  MCI with the game's own `open avivideo!<file> alias vfw`, the window
  hidden and the audio off: `status vfw length` 7,700 ms, `frame rate`
  30,000, `where vfw source` `0 0 640 480`, `play vfw from 0 wait` returns 0
  with the position at frame 231, `close vfw wait` 0. The PC's `capcom.avi`
  through the same script, as the control: 112 of 112 frames, 7,467 ms,
  15,000, `0 0 640 480`, position 112.
- A trap met on the way, the original's too: MCI's `open` fails (`304`) when
  the full path is long (the scratch path, ~160 characters, whether given
  whole or relative to the working directory); its 8.3 short form opens.
  A game directory with a path that long would lose the PC's movie as well.

Not verified: the movie **in the game** (no live run in this round's agent
work; the coordinator's batch), and **the audio heard** (MCI's audio was off;
the stream is plain PCM, which the PC's file also is, at a different rate).

## 6. Ledger text (for the coordinator to number)

**DIV-TBD-FM-1 - A disc-only install shows the disc's Capcom logo movie,
re-encoded at import**

- **Original behaviour:** the PC port plays `CAPCOM.AVI` from its install
  (shipped as Indeo 5, 640 x 480, 15 fps, 113 frames, PCM 22.05 kHz stereo,
  7.53 s; DIV-0001 re-encoded it to Cinepak, 112 frames, 7.47 s). The
  PlayStation release plays `LOGO/CAPCOM30.STR` through its MDEC instead
  (320 x 240, 30 fps, 231 frames, 7.70 s; XA 37.8 kHz stereo, 7.63 s).
- **New behaviour:** a cache built from a PSX disc holds `base/fmv/
  CAPCOM.AVI`, converted by the player's ffmpeg from that stream (Cinepak,
  640 x 480 by pixel doubling, 30 fps, 231 frames; PCM 44.1 kHz stereo), and
  `install` puts it where `Fmv_Play` opens it when the game directory has no
  `CAPCOM.AVI` of its own. A PC install keeps the PC's file. The player of a
  disc-only install sees the disc's logo: **twice the frames per second, a
  320 x 240 picture shown doubled rather than the PC's 640 x 480 master,
  about 0.2 s longer, audio at 44.1 kHz from the disc's 37.8 kHz source
  rather than the PC's 22.05 kHz**, and Cinepak's loss on it (SSIM 0.970
  against the direct decode). Without ffmpeg there is no movie (the engine's
  behaviour then is IN's).
- **Why:** rule 1 - a disc-only build carries none of the PC's files, and
  the disc's own stream is the movie Capcom made. Converting at import keeps
  the engine's MCI path unchanged and puts no decoder in the engine (the
  owner, 2026-10-12); the player's own ffmpeg does the work, as for the PSP's
  effects (`psp-only-build.md` section 7 item 4), so nothing is vendored.
- **Also in the PSX version?** The movie is the PSX version's; the
  difference is from the PC port's file.
- **Reversible?** Yes: a `capcom.avi` in the game directory is never
  replaced; putting the PC's file there restores it.

# A PSP-only install: the logo movie, the jingles, and `install --base`

**Status:** MEASURED (2026-10-12, the PSP-only round's group PN, branch
`psp/pn` from `668a9b67`; built and installed from both PSP discs, headless
self-test from the PSP-EU install; **not played, not seen, not heard** - the
end batch's and the owner's)

The PSP's half of the disc-only install path
([`disc-only-install.md`](disc-only-install.md),
[`fmv-import.md`](fmv-import.md)): what `importer.py build` then `install
--base` give from a PSP disc alone, the PSP's logo movie converted the way
FM converts the PSX's, and the 11 jingles in the PC's form. The round is
[`psp-only-round.md`](psp-only-round.md).

Scripts named below are in this group's scratch (`<scratchpad>/psp_pn/`);
the caches and installs are game data and stay there (rule 1).

## 1. `install --base` from a PSP disc, against the PSX disc-only install

Measured with this branch's tools: `importer.py build --source "<PSP-EU iso>"
--out cache_eu1` (333 s; `build_cache.sh`), then `install --base --cache
cache_eu1 --game g_psp`, against `install --base` from the coordinator's
`ref_us` (the US PSX disc alone, the round base's tools) into `g_us`
(`installs.sh`), compared by `cmp_installs.py g_us g_psp`:

| Folder | PSX disc-only (US) | PSP-only (EU) | Same name and bytes |
|---|---|---|---|
| root | `CAPCOM.AVI`, 9,183,260 bytes (the PSX's, FM) | `CAPCOM.AVI`, 9,892,740 bytes (the PSP's, section 2) | 0 of 1 |
| `DAT\` | 742 containers + `area4-walls.AREA004.DAT`; 1 empty (`AFLDKWA`); 327.7 MB | 742 containers; **223 empty**; 152.6 MB | 520 of 742 |
| `SND\` | 880 (875 effects, 5 voices; WAVs byte-identical to the PC's) | **891**: the 880 decoded from ATRAC3plus, **and the 11 jingles** | 0 of 880 |
| `BGM\` | empty | empty | - |

What each difference is:

- **`DAT\`: 222 containers have no `base/` file at all.** A PSP disc alone
  builds 519 of the 741 base containers (`psp-only-build.md` 8: the 228
  `disc` chunks a PSP disc carries in its own form, 16 banks, the arenas, the
  dial page and the rest, until SI's stand-ins and MB's banks land).
  `install_base` wrote each as the chunks the cache had, which for 222 is
  none, and its line called them "written empty for the language layer to
  fill" - true of `AFLDKWA` on a PSX cache, wrong here: `FIRST.DAT`,
  `BATTLE.DAT`, `START.DAT`, 118 `AREA`s, 40 `BOSS`es, the 8 `RYUD` and
  the 37 arena files are not text (counted on `g_psp\DAT` by prefix). This
  branch adds a warning for exactly that case (a container whose `base`
  layer the cache lacks), and leaves the PSX output unchanged (the US cache
  has every base container, so no warning):
  `base: WARNING - 222 of the 742 containers have no base/ file in this
  cache, so their base chunks are missing from DAT/ ...`. It warns rather
  than refuses so that a partial PSP install can still be self-tested until
  SI merges; whether `--base` should refuse an incomplete base then is a
  call for the coordinator (a refusal would be the safer default once SI's
  stand-ins make a PSP base complete).
- **`DAT\`: 83 more** are written without the slots of a layer the cache
  lacks (the Chinese text, as on PSX, where it is 252): a PSP-only cache has
  no `loc/` at all yet (LG). `area4-walls` is a Western **PSX** disc's layer
  (DIV-0080), so a PSP-only install has none.
- **`SND\`: all 880 effects and voices differ**, as they must: the PSP's
  ATRAC3plus decoded by the player's ffmpeg, cut at the measured onsets
  (`at3.py`, `psp-only-build.md` 2.1.1 and 8). Same names, same lengths.
- **`SND\`: the 11 jingles are there** (`AKURYOU DRAGON GAAN01 ITEM01 KARA
  MIMI01 NAKAMA02 OYASUMI PURE SIPPAI01 WIN`). `install_base` already copied
  every file of `base/snd`, so no new call site was needed for them; what
  was wrong was their form (section 3). A PSX-only install has none (the
  owner's call, `psp-only-round.md` 7 item 5).
- **`BGM\`** stays empty on both: the music is the cache's `base/bgm`
  (DIV-0087), which a PSP-only cache does not have until MS stage B.

The PSP-JP cache (`cache_jp1`, 435 s) against PSP-EU (`cmp_caches.py`):
1,413 files each, all byte-identical except `base/exe/data.bin` and
`data.toml` (the two builds' images, IM's). The movie and all 891 sound files
are the same bytes from either disc.

## 2. The logo: `PSMF/CAPLOGO.PMF` to `base/fmv/CAPCOM.AVI`

### 2.1 The file, and what ffmpeg reads

`PSP_GAME/USRDIR/JPN/PSMF/CAPLOGO.PMF` (PSP-JP) and `.../USA/PSMF/...`
(PSP-EU): 1,112,064 bytes, sha256 `7cbe25ef...cab78ad3` on both
(`pmf_extract.py`; held as `psp_fmv.SOURCE_SHA256`, a hash, no bytes).
`PSP_GAME/ICON1.PMF` (PSP-JP only) is the XMB icon movie, not the game's.

A 2,048-byte PSMF header (`PSMF0014`, stream offset 0x800, size 1,110,016),
then an MPEG-2 program stream. Walked packet by packet (`pmf_scan.py`;
`tools/psp_fmv.py probe`):

| Stream | Packets | Payload | First PTS (90 kHz) | Content |
|---|---:|---:|---|---|
| `0xE0` video | 479 | 962,529 B | 90,000 (1.000 s) | H.264 Main, 480 x 272, yuvj420p, **231 frames** at 30000/1001 fps (PTS 90,000 .. 780,690 in steps of 3,003), 7 I + 224 P, **7.708 s** |
| `0xBD` private stream 1, sub-stream `0x00` | 63 | 126,588 B | 85,069 (0.945 s) | ATRAC3plus: a 4-byte private header per packet, then 168 frames, each an 8-byte header (`0F D0 28 5C 00 00 00 00`) and 744 bytes; parameters `0x285C` = 44.1 kHz, stereo, 744-byte frames (128 kbit/s); 168 x 2,048 samples = 7.802 s |

**ffmpeg 9.0.1 (gyan.dev full build) does not read PSMF whole:**

- **By a PSMF demuxer: none.** `ffprobe -f psmf` answers "Unknown input
  format: psmf"; `ffmpeg -demuxers` lists none for PSMF/PMF.
- **By a plain MPEG-PS read: the video only.** `ffprobe` (probed, or `-f
  mpeg`, on the whole file or on the stream past 0x800) finds
  `Stream #0:0[0x1e0]: Video: h264 (Main), yuvj420p(pc), 480x272, 29.97 fps`
  and **no audio stream**: its MPEG-PS demuxer does not map private stream
  1's sub-stream `0x00`. (It also reports the duration as 7.14 s, from the
  packet rate; `-count_frames` gives 231 and the frame PTSs 7.708 s.)
- **Its ATRAC3plus decoder is there** (`atrac3plus`, the one that decodes
  the effects).

So `tools/psp_fmv.py` demuxes the audio itself - the PES walk above, every
frame header checked, the frames laid into the RIFF
`WAVE_FORMAT_EXTENSIBLE` ATRAC3plus form the disc's own `.AT3` files use
(the header **written by rule** from the two parameter bytes: tag `0xFFFE`,
2 channels, 44,100 Hz, 16,020 bytes/s, block 744, 2,048 samples a block, mask
3, the ATRAC3plus sub-format GUID, version 1, the two bytes, eight zeros -
the same fields `at3_fmt.py` reads from `MUSIC/WIN.AT3`) - and hands ffmpeg
two inputs: the program stream for the video, that wrapper for the audio.

### 2.2 The picture is the PSX's 4:3 frame, stretched

The PSP's 231 frames are the PSX movie's 231 frames (`LOGO/CAPCOM30.STR`,
FM section 1), one for one: the PSX and PSP frames 0, 40, 80, ..., 200
tiled side by side (`frames/tile.png`, `frames/tile_psx.png`) show the same
composition, with the `CAPCOM` logo at the same fraction of the width (about
0.70) and of the height (about 0.42 .. 0.59) in both. So the PSP frame is
the whole 4:3 picture resampled to 480 x 272, not a crop. The conversion
scales it back to **640 x 480, the whole frame, no letterbox**: that is the
PlayStation's (and the PC's) shape. 4/3 across and 30/17 down are not whole
factors, so it is **resampled (`lanczos`)**, not pixel-doubled as FM's
320 x 240 is.

### 2.3 The encoding

```
ffmpeg -v error -y -f mpeg -i CAPCOM.AVI.mps.tmp -f wav -i CAPCOM.AVI.at3.tmp \
       -map 0:v:0 -map 1:a:0 -vf scale=640:480:flags=lanczos \
       -af atrim=start_sample=2416,asetpts=PTS-STARTPTS \
       -c:v cinepak -c:a pcm_s16le -ar 44100 -fflags +bitexact -flags:v +bitexact -f avi CAPCOM.AVI.part.avi
```

FM's form otherwise (Cinepak, PCM s16le 44.1 kHz stereo, `+bitexact`,
written to a temporary name and renamed on success). The two `.tmp` inputs
are removed after.

- **The frame rate** is the PSP's own, 30000/1001: 231 frames, 7.708 s (the
  PSX's is 30.000 and 7.700 s).
- **The audio's start.** ffmpeg starts each input at 0, but the audio's
  first PTS is 4,931 ticks (54.8 ms) before the video's, so the decode is
  trimmed by round(4,931 x 44,100 / 90,000) = 2,416 samples (`atrim`; a later
  start would be an `adelay`). Checked against the PSX movie's audio, which
  FM's conversion starts at 0 with its video: the cross-correlation of the
  two (`audio_cmp.py`, mono 44.1 kHz) peaks at **lag 0**, correlation
  **0.9952**, SNR **20.1 dB** over the 336,336 samples they share. Whether
  the PTS includes the ATRAC3plus decoder's own delay is not separately
  known; the lag-0 match says the result lands where the PSX's does.
- The audio is 341,648 samples (7.747 s), sounding to 6.708 s; the PSX's
  is 336,336 (7.627 s), sounding to 6.694 s.

**The result** (`ffprobe` of `cache_eu1/base/fmv/CAPCOM.AVI`): AVI, Cinepak
`cvid` 640 x 480, 30000/1001 fps, 231 frames, 7.708 s; PCM s16le 44,100 Hz
stereo, 341,648 samples; **9,892,740 bytes, sha256 `1e0a6c0a...36a6fff`**
with ffmpeg 9.0.1. Byte-identical from PSP-JP and PSP-EU and in a third,
standalone run (`conv.py`): deterministic. 264 s standalone, about 4.5 min
inside a build with the 891 sound files (Cinepak's encoder is
single-threaded).

**Cinepak's loss** against the same ffmpeg's direct decode scaled the same
way, frame by index (`ssim_try.sh`, `psx_ref.sh`; the same method reproduces
FM's 0.970 for the PSX movie): **SSIM 0.868** (R 0.828, G 0.943, B 0.834),
**PSNR 34.6 dB** (the PSX movie: 0.970, 39.1 dB). By 20-frame block the
SSIM runs 0.65 .. 0.99; frames 220..230 read 0.376, but those are near-flat
frames where SSIM's R and B are unstable (G 0.996 there). The upscaled
smooth H.264 picture suits Cinepak's 4 x 4 vector codebooks worse than the
PSX's pixel-doubled blocks. More strips do not help (`-max_strips 8
-min_strips 8`: 0.872, 9.97 MB). Whether it looks right is the owner's eye.

### 2.4 The importer

- **`build`** (`fmv_build`): unchanged when a PSX disc is among the sources.
  With no PSX disc **and no PC source**, the first PSP disc's `CAPLOGO.PMF`
  is converted by `psp_fmv.convert` into `base/fmv/CAPCOM.AVI`, and the same
  `[fmv]` table is written (`build = "psp-eu"`, `source =
  "PSMF/CAPLOGO.PMF"`, `source_sha256` the whole file's, `ffmpeg`, `file`,
  `sha256`), or `skipped` with the reason: no ffmpeg, a disc without the file,
  a PMF that is not the measured one, a stream the demux refuses, or
  ffmpeg's error. Nothing fails the build. A PC + PSP build (the `--opt`
  presets) gets no movie: the PC's own `capcom.avi` is the install's.
  The manifest-and-print tail is now `fmv_record`, shared by both cases.
- **`verify`** (`fmv_verify`): a `psp-*` row's source hash is checked
  against `psp_fmv.SOURCE_SHA256`; the rest (the file's hash, 640 x 480,
  `cvid`) as for the PSX's. Measured: `base/fmv/: CAPCOM.AVI from psp-eu
  (ffmpeg version 9.0.1 ...); present, 640 x 480, 231 frames, cvid, as
  recorded`; `0 problem(s)`.
- **`install`** (`fmv_install`): unchanged, and it installs the PSP's file
  the same way (`fmv: CAPCOM.AVI, the disc's logo movie, installed`). With
  the file present, `Disc_Probe` passes as it always did (DIV-0100 not
  needed).
- **`check`** (CI): `psp_fmv.check()` demuxes a synthetic PSMF (a pack, a
  video packet, two audio frames split across two packets), checks the time
  stamps, the frames, the trim and every field of the wrapper, and that a
  damaged frame header is refused. No game data.

## 3. The jingles in the PC's form

`at3.py` encoded the 11 jingles with ffmpeg's MP3 muxer defaults. Measured
(`jingles.py`, `mp3walk.py`, `cache_eu0` against `bof3/SND`):

| | The PC's 11 | `at3.py` before | After this branch |
|---|---|---|---|
| First bytes | `FF FB 92 04`: a frame | `49 44 33 04`: **a 44-byte ID3v2 tag**, then frames | `FF FB 90 04`: a frame |
| Channel mode | 0 (stereo), every frame | 1 (joint stereo) | 0, every frame |
| Header walk from byte 0 (`mp3.cpp`'s `WalkFrames` rule) | the whole file | stops at byte 0 | **the whole file**, every frame MPEG-1 layer III 44.1 kHz |
| Frames | e.g. `OYASUMI` 257, `PURE` 10,299 | - | 259, 10,302 (2 or 3 more each: the encoder's priming and padding) |

The engine plays a jingle through `Sound_LoadStream` kind 0 into
`Music_Start` and the MP3 decoder (`save_menu.cpp`, DIV-0089). The game's
MP3s are bare frame streams (`mp3-decoder.md`, `bgm-comparison.md` 6.1), and
the DIV-0091 comparison (`LooksMp3`) only takes a file whose first two bytes
are a sync, so a tagged file was outside everything measured; and Capcom's
decoder (`BOF3X_MP3=original`) has never been shown to skip a tag or to
take joint stereo. So `at3.build` now encodes with `-c:a libmp3lame -b:a
128k -joint_stereo 0 -id3v2_version 0 -write_xing 0`, and refuses (skips,
with the reason) an MP3 that does not open with an MPEG-1 layer III frame.
**Not run through either decoder** (that is the `BOF3X_MP3_COMPARE` run,
which starts the game to `Game_WinMain`: the end batch's).

The 880 effects and voices are unchanged (`at3.py`'s WAVs).

## 4. The headless self-test from the PSP-only install

From a scratch copy of this branch's `bof3x-launcher.exe`, `bof3x.exe`,
`bof3x.dll` (no ini), `BOF3X_SELFTEST_ONLY=1 BOF3X_CACHE=cache_eu1
BOF3X_EXEIMAGE=cache_eu1 BOF3X_LANG=en-150 bof3x-launcher.exe --game g_psp
--no-config`, `g_psp` with no PC file:

- **exit 0**: `self-test only, game process exit code 0`; `inject: 10088
  ours`; `self-test only: done, ending the process`; no `FATAL`, no
  `MISMATCH`.
- `no BOF3.exe ...: a disc-built install`; `.data at 0x5DA000 from
  base/exe/data.bin (psp-eu, disc-built)`; **`gap 501 ranges 5689 bytes`**
  (the US image: 456 / 5,455); `exe_own: 17 regions, 20866 bytes filled over
  a disc-built image`; the key table from the engine's rule.
- `DIV-0089 ... 436 of the manifest's 742 containers held whole, 83 in part`.
- `DIV-0005: language overlays DAT\en-150.*.DAT` and `language en-150: the
  install's` - **there is no `en-150` layer** in the cache or the install
  (a PSP source builds none until LG), and the self-test does not object;
  the game would show a language-less install's text slots.

What it ran on, until SI, MS, MB, IM and LG merge: 519 base containers,
no `base/bgm`, no `loc/`, the PSP-EU image as `exe_tables` builds it today.
The self-tests do not open the 222 missing containers, play a sound or reach
`Game_WinMain`.

## 5. Checks

| Check | Result |
|---|---|
| `python tools/importer.py check` | exit 0; `seq / at3 / psp_fmv check: ... a synthetic PSMF, 0 error(s)` |
| The PC and PSX paths (`unchanged.sh`): JP + PC, US alone, Europe alone, built with this branch, against the coordinator's `ref_jppc`, `ref_us`, `ref_eu` | **all unchanged**: 2,126, 1,873 and 1,865 files, every non-manifest file's hash equal; the manifests equal but for the build time |
| `recipes/exe-gaps.tsv` | not touched |
| `python tools/ledger_check.py` | 101 entries, 0 errors, 2 notes (this doc's draft is not inserted) |
| `src/` | not touched; built only to self-test |

## 6. Not verified

- **The movie in the game**: not seen, not heard (no live run). MCI on the
  file was not run either; FM's run proved MCI and Video for Windows on its
  Cinepak AVI at 30 fps, this one is 30000/1001.
- **The jingles through either MP3 decoder** (section 3), and heard.
- **The effects in play** from a PSP-only install: the self-test does not
  play a sound.
- The audio's sync on the PSP itself (the PSMF player's handling of the
  ATRAC3plus delay); only the match with the PSX movie is measured.
- An ffmpeg without `libmp3lame` (the jingles then skip with the reason) or
  one whose MPEG-PS demuxer maps sub-stream `0x00` (then a second audio
  stream exists, but only `0:v:0` is mapped from that input).
- A start from a PSP-only install that reads the 222 missing containers:
  it would fail; that waits on SI.

## 7. For the other files

### DIV D - the PSP's effects, jingles and logo in a PSP-only install

- **Original behaviour:** the PC port plays its own `SND\` (875 effects and
  5 voices as 22.05 kHz mono WAVs, the 11 jingles as bare 128 kbit/s MP3s)
  and its own `CAPCOM.AVI` (DIV-0001's Cinepak, 15 fps, 112 frames).
- **New behaviour:** an install built from a PSP disc alone (`importer.py
  build` with no PSX disc and no PC source, then `install --base`) has:
  - the 880 effects and voices **decoded from the PSP's ATRAC3plus** by the
    player's ffmpeg, each cut at its measured onset (shifted up to ±250 ms
    to where the PC's clip starts) and padded to the PC file's length:
    against the PSX's XA, correlation median 0.998 (min 0.990), SNR median
    24.9 dB (16.9 .. 30.0) for the effects, 19-25 dB for the voices; the
    lengths are the PC's. Heard in play through `Sound_LoadStream` (the
    cache's `base/snd` under `BOF3X_CACHE`, DIV-0089, or `SND\`);
  - the **11 jingles**, which a PSX-only install lacks, re-encoded from the
    PSP's ATRAC3plus to bare 128 kbit/s stereo MP3s in the PC's form
    (correlation 0.995-0.9996, SNR 20-31 dB against the XA; 2 or 3 frames
    longer than the PC's);
  - the **PSP's logo movie** (`PSMF/CAPLOGO.PMF`, H.264 480 x 272 with
    ATRAC3plus) re-encoded at import as `CAPCOM.AVI`: Cinepak 640 x 480
    (the PSP's frame scaled back to the PlayStation's 4:3 by lanczos),
    30000/1001 fps, 231 frames, 7.708 s; PCM 44.1 kHz stereo aligned with
    the video by the stream's time stamps; Cinepak's loss SSIM 0.868 /
    PSNR 34.6 dB against the direct decode.
  Without ffmpeg none of them is written, with the reason in the build's
  output and the manifest; the install then has no `SND\` effects and no
  movie (DIV-0100 starts it).
- **Why:** the owner's calls of 2026-10-10 (`psp-only-build.md` 7: a
  PSP-only install uses the PSP's audio as it is; the effects decoded by the
  player's own ffmpeg, nothing vendored) and of 2026-10-12 (the logo by the
  player's ffmpeg at import, as FM). Rule 1: a PSP-only install carries no
  PC file.
- **Also in the PSX version?** No: the PSX plays its XA and MDEC; these are
  the PSP release's lossy re-encodes (and ours of them).
- **Reversible?** A PSX disc among the sources gives the PSX's exact files
  instead; a PC install's own files are never replaced.
- **Verification:** built from both PSP discs (byte-identical outputs),
  `verify` 0 problems, the exe host's headless self-test from the PSP-EU
  install exit 0 (this doc, sections 1-5). Not heard, not seen.

### `docs/README.md`

One row: `psp-install.md` - the PSP-only install: `install --base` against
the PSX disc-only one, the PSP's logo movie (PMF) converted at import, the
jingles in the PC's MP3 form; MEASURED.

### `docs/psp-only-build.md`

- 2.5: the PMF is converted at import (`psp-install.md` 2); ffmpeg has no
  PSMF demuxer, its MPEG-PS read gives the video, the audio is demuxed by
  `psp_fmv.py`.
- 8: the jingles are now encoded as bare MP3 frame streams (no ID3v2 tag,
  plain stereo); `install --base` copies `base/snd`, jingles included.

### `docs/psp-only-round.md`

Section 9's "Whether the player's ffmpeg reads `PMF`": no PSMF demuxer;
video by MPEG-PS, audio by our demux (`psp-install.md` 2.1).

### Known defect (for numbering)

`install --base` on a cache with an incomplete `base/` (any PSP-only cache
until SI) writes the missing containers empty or partial and exits 0; now
warned (section 1), not refused.

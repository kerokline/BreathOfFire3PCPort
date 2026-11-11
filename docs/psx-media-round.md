# The PSX media round: the logo movie and the jingles without ffmpeg

**Status:** DONE (2026-11-11, `psx-media/own-decoders` off `main`
`8d2d987d`; M1..M4 as one commit each, section 5 the proof)

A PlayStation disc is the most common source of an install
([`disc-only-install.md`](disc-only-install.md)). Two of its pieces still need
the player's own ffmpeg: the Capcom logo movie (`LOGO/CAPCOM30.STR`,
[`fmv-import.md`](fmv-import.md)) and the 11 kind-0 jingles of `S_XA00.STR`
([`sound-import.md`](sound-import.md) 6 item 3, DIV-0129, DIV-0150). This
round removes ffmpeg from both: the importer copies the disc's own bytes and
the engine decodes them. After it, a PSX-disc install needs nothing the
release does not carry, on Windows, under Proton, and on a native build
later ([`linux-round.md`](linux-round.md)).

Out of scope: the PC `capcom.avi` repair (Indeo 5, DIV-0001) and every PSP
piece (ATRAC3plus, H.264). They keep the player's ffmpeg.

## 1. What decodes what

| Piece | Format | Decoded by, after this round |
|---|---|---|
| The logo movie's picture | STR v2: MDEC bitstream (BS v2) in Form 1 sectors, 320 x 240, 231 frames | `src/media/str_movie` |
| The logo movie's sound | XA ADPCM, stereo, 37,800 Hz, 143 Form 2 sectors | `src/media/xa_adpcm`, played through SDL3 at its own rate |
| The 11 jingles | XA ADPCM, stereo, 37,800 Hz, clips of `S_XA00.STR` | `src/media/xa_adpcm` + `src/media/resample` to 44,100 Hz, behind the MP3 seam (`src/audio/mp3.cpp`) and in the launcher's Jukebox |

Every source is public: psx-spx ("CDROM File Video STR Streaming and BS
Picture Compression", "MDEC", "CDROM XA Audio ADPCM Compression"), and the
owner's own MIT crates in `~/projects/bof3` (`player/psx-machine/src/mdec.rs`,
`player/psx-formats/src/xa.rs`, `adpcm.rs`; the stream's layout in
`wiki/src/content/docs/formats/str.md`). Code ported from those crates keeps
their MIT notice ([`THIRD_PARTY.md`](THIRD_PARTY.md)). No ffmpeg code is read
for this round; ffmpeg's output is the oracle only.

`src/media` includes no operating-system header and builds as a host
library (`tools/media_host`, any C++17 compiler, Linux or Windows), so its
tests run natively in CI.

## 2. The contracts

### 2.1 The movie file: `CAPCOM30.STR`

The importer writes the movie's 1,155 sectors as raw 2,352-byte sectors, in
disc order, unchanged - the bytes `fmv_build` already hands ffmpeg today -
to `base/fmv/CAPCOM30.STR`, and `install` copies it to the game directory.

`Fmv_Play` opens the name it is given as before (the game directory, then
the CD root). When that name is `CAPCOM.AVI` (any case) and neither open
succeeds, it tries `CAPCOM30.STR` the same two ways. A `CAPCOM.AVI` that is
there (the PC's, or one an older importer converted) still plays first.

`video::Movie::Open` reads either form by its bytes: `RIFF....AVI ` is an AVI
as today; a size that is a multiple of 2,352 whose first sector starts with
the CD sync (`00 FF x10 00`) is an STR. The STR movie:

- video: the Form 1 sectors (submode bit 3 clear, data at +24) whose first
  word is `0x80010160`, assembled per frame by `chunk_number` /
  `chunk_count` / `frame_number`; BS v2 only (`version` 2, `id` 0x3800);
  any other version is refused at `Open` with the reason;
- frame rate: `150 x frames / sectors` a second (the double-speed read;
  30 for this stream, the bof3 wiki's `formats/str.md` "CAPCOM30 timing");
- picture: 320 x 240 decoded, given to the player doubled to 640 x 480 by
  pixel repetition, `0x00RRGGBB` rows top-down - the size and form of the
  AVI the importer made before, so DIV-0035's rectangle is unchanged;
- sound: the Form 2 sectors with the audio submode bit (0x04) of the first
  audio channel met (its file and channel bytes); a sector of another
  channel (the trailing mono one) is not played; decoded whole at `Open` to
  16-bit PCM at the stream's own rate and channel count.

### 2.2 The jingle file: an XA clip

`xa.jingles` writes each jingle, where it writes the MP3 today
(`base/snd/NAME.DAT`, or `base/psx-jingles/NAME.DAT` beside a PC install),
as an XA clip: the clip's own sound groups, undecoded.

| Offset | Size | Field |
|---:|---:|---|
| 0 | 8 | magic `BOF3XA\0\1` |
| 8 | 4 | `u32` sample rate: 37,800 or 18,900 |
| 12 | 2 | `u16` channels: 1 or 2 |
| 14 | 2 | `u16` bits per sample: 4 (8 is refused) |
| 16 | 4 | `u32` sound groups `n` |
| 20 | 4 | `u32` 0 |
| 24 | 128 x n | the sound groups, in play order (18 a sector, the sector's 24-byte tail, 20 reserved bytes and the EDC, dropped) |

Little-endian throughout. Each clip decodes from a fresh ADPCM history, as
`xa.jingles` decodes them today.

`src/media/xa_clip` reads one into 44,100 Hz 16-bit stereo: decoded by
`xa_adpcm`, resampled by `resample` (7/6 from 37,800, 7/3 from 18,900;
polyphase, fixed-point, the coefficients a generated header, so both widths
give the same samples), a mono clip doubled to both channels. The MP3 seam's
`Open` takes a file in memory whose first bytes are the magic as a clip, and
`Decode` hands out 1,152-sample frames of it, as minimp3's frames are, the
last one short; `Seek(0)` rewinds; `CountFrames` counts the clip's frames the
same way, so DIV-0081's loop fold and the music resume work unchanged. The
launcher's Jukebox reads the same files through `xa_clip`.

### 2.3 The manifest rows

`[fmv]` for a PSX disc: `build`, `source`, `source_sha256` (as today),
`file = "base/fmv/CAPCOM30.STR"`, `sha256`; no `ffmpeg`, no `args`. `verify`
checks the file is the recorded one and that its sectors hash to
`FMV_SOURCE_SHA256`. The PSP's row is unchanged.

`[snd_jingles]` keeps its fields; each jingle's hash is the clip file's.
No ffmpeg is looked for on a PSX-only import.

## 3. The units

One branch, one commit for each unit, signed off (CLAUDE.md rule 7).

| Unit | What | Proof |
|---|---|---|
| M1 | `src/media`: `xa_adpcm`, `resample` (+ `tools/gen_resample.py`), `xa_clip`, `str_movie`; `tools/media_host` with their tests | Host tests pass on Linux and with the i686 / x86_64 cross builds. On the US disc (scratch, never committed): every frame against ffmpeg's `psxstr` + `mdec` decode of the same file, PSNR stated (not bit-exact: two IDCTs); the movie's PCM and every jingle's 37.8 kHz PCM sample-exact against `tools/xa.py`'s decode |
| M2 | The importer and the launcher's text: `fmv_build` / `fmv_verify` / `fmv_install` copy the STR; `xa.jingles` writes clips; ffmpeg no longer looked for on a PSX-only import; `importer.py check` and the self-tests | A `setup` from the US disc with no ffmpeg on the `PATH` writes both, `verify` passes |
| M3 | The engine: `Movie` opens an STR, `Fmv_Play` falls back to `CAPCOM30.STR` and takes the movie's audio form, the MP3 seam and the Jukebox read a clip | Both widths build; the movie plays with sound and the jingles play in the game (Proton on this machine, the owner's eye and ear on Windows) |
| M4 | The ledger (two entries), `fmv-import.md`, `sound-import.md`, `THIRD_PARTY.md`, STATUS, HANDOFF | `ledger_check.py`, `ledger_registry.py --check` |

## 4. Not verified when staged

- That ffmpeg's `mdec` decode is close enough to be an oracle (expected:
  PSNR above 40 dB if the bitstream is read right).
- The size of the clip files (about 2.6 MB for `PURE` against 4.3 MB for
  its MP3, by arithmetic).

## 5. As run (2026-11-11)

- **M1** (`src/media`): `media_tests` 237,969 checks, 0 failed (also under
  ASan / UBSan). On the US disc's `CAPCOM30.STR`: 231 of 231 frames, mean
  PSNR 53.87 dB (lowest 44.65) against ffmpeg's `psxstr` + `mdec` decode;
  the movie's 576,576 PCM samples and the 11 jingles' 37.8 kHz decode equal
  `tools/xa.py`'s sample for sample. The i686 build (under Wine) gives the
  same frames and resampled samples as the host build.
- **M2** (the importer): a set-up from the US disc with no ffmpeg on the
  `PATH` writes `base/fmv/CAPCOM30.STR` (1,155 raw sectors, source hash as
  measured) and 11 clip files; `verify` passes them.
- **M3** (the engine): both widths build, the imports system DLLs only;
  `bof3x-fmvdump` gives 231 frames at 640 x 480 and 37,800 Hz stereo, the
  i686 and x86_64 outputs byte-identical. Every no-game-data gate of
  `checks.yml` passes.
- **Under Proton Experimental on a Linux machine** (the release zip built
  from this branch, an RTX 5090): the importer's bundled Python sets the
  install up from the US disc with no ffmpeg in the prefix and prints no
  ffmpeg warning; the game logs `DIV-0214 CAPCOM.AVI is not there;
  CAPCOM30.STR plays in its place`, 231 frames at 30 a second, PCM 37,800
  Hz stereo, and goes on to the title; the owner heard the sound. The
  jingles' in-game play (an inn, a fanfare) was not reached in these runs.

**Not verified:** the jingles heard in a scene; the ten state-hash routes
(the change is the movie before the loop and the music seam's clip path;
an MP3 decodes as before); a Windows play by the owner.

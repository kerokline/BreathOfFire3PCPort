# Music: the PC's MP3s against the disc's sequences

**Status:** MEASURED (2026-10-06: the method's steps 1 to 4 in sections 6 to 8,
`tools/bgm/`; the listening, step 5, is the owner's - section 9. Sections 1 to 5
are the 2026-09-26 method, kept as written; section 5's first open item is
answered in 6.2, the second only in part, in 8.3. [`IDEAS.md`](IDEAS.md) I23)

The question: if a disc-only build ([`ASSET_SOURCES.md`](ASSET_SOURCES.md))
played the PlayStation's own music data in place of the PC port's MP3s, would
it sound like the same music, or like a restoration? The answer decides
whether a sequencer and SPU synth — the largest engine item on the disc-only
path — are worth building for fidelity, or only for independence from the PC
install.

## 1. What is established

- **The disc's music is sequenced, not recorded.** 81 `BGM*.EMI` files hold
  instrument samples and note data (VH/VB + SEQ); the SPU synthesises the music
  live ([`DAT_CONTAINER.md`](DAT_CONTAINER.md) §5). There is no recording of
  the music anywhere on the disc.
- **The PC's `BGM/` holds 166 MP3s**, numbered densely `000`–`166`, 9 of them
  `N`-suffixed. The disc's numbers are sparse, up to 197. A name match is not
  evidence, and the mapping is unresolved (same section).
- **So the MP3s are recordings of sequenced playback, renumbered** — an
  inference, not a measurement: a recording is the only way to get audio from
  sequence data. Whether they are captures of hardware or renders in software
  is not known. The data on the disc is the original, and the MP3 is derived
  from it. The two were not made in parallel from one master.
- **The port also dropped sequences.** The SEQ groups at `dest 0` in 38 files
  (35 `BOSS`, `BATTLE`×2, `DEMO`) have no PC counterpart
  ([`DAT_CONTAINER.md`](DAT_CONTAINER.md) §2).
- **The PC loops a track from the start of the file.** On end of stream,
  `0x5A6F30` calls `0x5AEBA0(decoder, 0)` and keeps decoding. Plain `NNN.DAT`
  loops, `NNNN.DAT` plays once ([`asset-loading-path.md`](asset-loading-path.md)
  §1a). A sequence can loop to a point inside the song. So either every PC loop
  replays the track's intro, or the recordings were cut to the loop body and
  the intro is lost.

## 2. Hypotheses, most audible first

| # | Difference | Expectation | Settled by |
|---|---|---|---|
| H1 | **Loop point** | PC replays the intro, or never plays it; the disc does not | step 3 |
| H2 | **Loop seam** | an MP3 encoder pads the start and end of a stream, so a gap or click at every PC loop | step 3 |
| H3 | **Reverb** | the SPU applies reverb the game sets up, possibly per scene; a recording bakes in one setting | step 4 |
| H4 | **Codec** | smeared transients, pre-echo, a high-frequency cutoff from the MP3, on top of samples that are already ADPCM | step 2, step 4 |
| H5 | **Live mixing** | on the PSX, effects and music share the SPU's 24 voices, so an effect may cut a music note off; the PC mixes two separate streams. Inferred from the hardware, not observed in this game | a capture during battle, step 4 |
| H6 | **Track set** | some tracks merged, split, missing or added in renumbering | step 1 |

Notes and arrangement are expected to match. Nothing suggests a rearranged
score.

## 3. Method

All of it runs on the owner's machine; nothing produced goes into the repo
(rule 1). Results are recorded as numbers and plots of numbers, never as
audio.

1. **Inventory both sides.** `ffprobe` every `BGM/*.DAT`: codec, sample rate,
   channels, bitrate, duration, encoder tag. List the disc's `BGM*.EMI` and
   the SEQ groups in other EMIs, with each sequence's length and loop markers
   if the SEQ carries them. Then find the exe's track table beside the
   `BGM\%03d` strings (`0x666FB8`), which maps a game track to a file number.
   That settles H6 and gives the pairs.
2. **Pick three tracks:** one looping field theme, one battle theme, and one
   `N` one-shot. For each, the MP3 and its sequence.
3. **Loop test (H1, H2).** Decode the MP3 to PCM and note where the encoder
   delay ends. Find the sequence's loop point in the SEQ data. Check whether
   the MP3's first seconds reappear later in the file. If they don't, the
   intro was cut. Then play the PC's loop in-game or through the decoder
   rewind, and look for the seam in the waveform.
4. **Timbre test (H3, H4, H5).** Record the same sequence from an accurate
   PSX emulator (DuckStation or Mednafen, reverb on), at 44.1 kHz, with no
   effects playing. Align the recording to the MP3 by cross-correlation, then
   compare spectrograms, the high-frequency cutoff, the noise floor and any DC
   offset. A clean emulator render against a hardware capture shows up in the
   noise floor, which is how "how was the MP3 made" gets answered.
5. **Listen.** The owner A/Bs the aligned pair, blind if practical. Numbers
   say what differs. Only the owner can say whether it *feels* like a
   restoration.

## 4. What the answer changes

- **Loops are wrong, timbre matches:** the cheap fix is loop points for the
  MP3s (a table of our own, measured), and the synth is a disc-only concern.
  A ledger entry either way.
- **Timbre clearly better from the disc:** the SPU synth earns its cost for
  every player, not just disc-only ones. Sequenced music becomes a candidate
  default (a divergence) with the MP3s as an option.
- **Indistinguishable:** the synth waits for phase 5 and the disc-only build.

In every case, the emulator render is the target an SPU synth of our own must
match. That makes step 4's recording the test fixture for the synth.

## 5. Open before starting

- Where each SEQ's loop is encoded (a SEQ marker, or the game's code).
- Whether the PSX changes reverb per scene: read the sound driver in the
  archival sibling's symbols (reference only, rule 5).

## 6. The inventory and the pairing (step 1, H6) - measured 2026-10-06

Scripts: `tools/bgm/inventory.py` (both sides and the song table; writes
`analysis/bgm/inventory.json`), `tools/bgm/loops.py` (every MP3 decoded:
edges, levels, in-file repeats; `analysis/bgm/loops.json`). Paths:
`tools/bgm/bgm_paths.py`. The disc side is `psx-jp` (the sibling's
`isos/Breath of Fire III (Japan).cue` and the extracted `D:\BoFIII\BIN`;
`SLPS_009.90` there and in the sibling's `disc/` have the same md5,
`1d7add4a...`).

### 6.1 The PC's 166 files

| Property | Value, all 166 files (own frame walker, and `ffprobe` 9.0) |
|---|---|
| Stream | bare MPEG audio; no ID3v1 or ID3v2 tag, no Xing / Info / LAME header, no junk between frames |
| Format | MPEG-1 Layer III, 44,100 Hz |
| Bit rate | 128 kbit/s in every frame (constant) |
| Channel mode | stereo (not joint stereo) in every frame |
| Header bits | no CRC; original 1, copyright 0, emphasis none; the first frame's `main_data_begin` is 0 |
| Encoder | not named anywhere: no tag, no encoder string. (Three files contain the bytes `FhG` inside frame data - about the chance rate over 129 MB, so no evidence.) |
| Size, length | 129.5 MB, 134.9 minutes; `ffprobe` durations agree with frames x 1,152 to 0.06 ms |
| Decoded length | exactly frames x 1,152 samples (`ffmpeg`'s decoder trims nothing without a gapless header) - the count the game's `Music_Decode` hands out a frame at a time |
| Lead-in | the first sample above -80 dBFS comes after a median 720 samples (16 ms; 176..2,530) in the 157 looping files |
| Ending | the last frame of a looping file peaks at a median -21 dBFS; 109 of 157 end above -30 dBFS, 75 above -20 dBFS; 3 end in silence. The files are cut while the music sounds, not faded |
| DC | under 1.6e-4 of full scale in every file |

Names: `000`..`166` with `021` absent; `N` on `004 009 028 042 058 096 105
141 150` (as [`asset-loading-path.md`](asset-loading-path.md) section 1a).

### 6.2 The disc's sequences

- **The song table** is at `0x80182830` in `SLPS_009.90` (file offset
  `0xEF030`; found by the sibling, `docs/loader_records/AREA.md`): 165
  entries of `{u16 file id, u8 seq, u8 sub}`. `seq` is 0 in all 165, `sub`
  0..3, and every file id is a `BIN/BGM/*.EMI` (the sibling's
  `tools/file_ids.py`). The PSX `Music_Play` at `0x80162610` (capstone on
  `SLPS_009.90`) loads nothing: it starts sub-song `sub` of the SEP already
  resident (`0x8015DCF0`, `0x8015DE8C`); loading is `0x801625AC` by the
  table's file id, or a file the caller loaded itself.
- **Every SEQ section is a SEP** (`pQES`, version 0) of **four** sub-songs,
  each `{u16 id, u16 resolution, u24 tempo, u16 rhythm, u32 length}` then
  MIDI-style events with running status. 119 files carry one: the 81
  `BGM*.EMI`, 35 `BOSS`, `BATTLE`, `BATTLE2` and `DEMO`; 80 distinct section
  md5s; 186 of the 476 sub-songs are empty placeholders (no notes, 0.5 s).
- **The loop is in the sequence data, not in the game's code** (section 5's
  first item): the Sony convention, controller 99 = 20 with controller 6 =
  127 (loop start, for ever) and controller 99 = 30 (loop end). 156 of the
  165 table songs carry them; **the 9 that do not are exactly the 9 songs
  whose PC file is `N`**, and every other paired file loops. That match, 9
  of 9 and 155 of 155, is the first evidence for the pairing below.
- **The 38 groups the port dropped are copies.** Each `BOSS` / `BATTLE` /
  `DEMO` SEP is a `BGM` file's: sub 0 a table song's (`BGMBAT00`..`06`;
  `BGMOPN` for `DEMO`), sub 2 song 86's, and sub 1 (25.3 s, 680 notes,
  looping) in no table entry - the battle bundles' own. One more non-empty
  sub-song is in no table entry: `BGM039A` sub 3 (12.9 s, 546 notes).
- The 165 songs are 165 distinct (file, sub) pairs but 136 distinct
  sequences: 25 groups share identical sequence bytes (151 and 153; 23 and
  52; 89, 119, 128, 138 and 143; ...); in 21 of the 25 the files' sound
  banks differ.

### 6.3 The pairing

**PC file `NNN` is PSX song `NNN`** for 0..164. Evidence: the PC's
`Music_LoadFile` formats the track number straight into the file name (no
table; `symbols.toml`); the PC's title plays 0x8D where the PSX title
(`GAME.EMI` section 1, `0x801D0CD0`) plays song 0x8D; the 9-of-9 one-shot
match above; durations; and three renders of the disc (section 8) matched
by waveform: song 0 against `000.dat`, 153 against `153.DAT` (and `151.DAT`,
the same sequence and bank), the title's against `141N.DAT`.

| | Count | Which |
|---|---|---|
| PC files paired with a table song | **164** | 0..164 except 21 |
| Table songs with no PC file | **1** | song 21 (`BGM019` sub 0, 24.6 s, looping). A PC `Music_Play(21)` opens neither name ([`known-defects.md`](known-defects.md) D24). Whether the game asks for 21 is not measured |
| PC files from a sequence outside the table | **1** | `165`: the battle bundles' sub 1 (24.66 s; 25.29 s nominal). Played after a fight: the dragon route's open log has `BATTLE2.DAT`, `BGM\153.DAT`, then `BGM\165.DAT` (`analysis/attract/dragon_r12w1_ours.bof3x.log`) |
| PC files with no pair found | **1** | `166` (57.05 s). Its nearest MP3 by onset correlation is `086` at 0.32 (true pairs score 0.67..0.99); song 86 is the battle bundles' sub 2. Not settled |
| Disc sequences with no PC file | 1 + 1 | song 21; `BGM039A` sub 3 (not searched for among the MP3s) |

So H6: no merges, no splits, one song missing, one added from the battle
bundles, one unexplained. The renumbering is the identity on the PSX's own
song numbers; the "sparse numbers up to 197" are file names, not songs.

## 7. The loop test (step 3, H1, H2)

### 7.1 What the PC does at a loop

`Music_Decode` (`0x5A6F30`, ours): at the end of the stream with
`Music_Loops` set, `Mp3_Seek(decoder, 0)`, and decoding goes on into the
same staging buffer. So the PC's loop is the whole file, end to start: the
last frame as decoded, the file's lead-in (16 ms median of near-silence),
then its first note. Modelled here as a decoder starting fresh at frame 0;
whether the third-party decoder keeps synthesis state across the seek is
not measured (the first frame's `main_data_begin` is 0 in every file, so the
bit reservoir plays no part).

### 7.2 The disc's loop, and where the MP3s end

The disc plays intro, body, body, ...: the loop end jumps to the loop start.
Measured on the renders (section 8) as the period at which a render repeats
itself (correlation 0.98..1.00 from the loop start on), with the MP3 aligned
onto the same timeline (`tools/bgm/compare.py`, `analysis/bgm/compare_NNN.json`,
`analysis/bgm/plots/trackNNN_loop.png`):

| Track | Disc intro | Disc body | MP3 length | The MP3 ends | So the PC, at every loop |
|---|---|---|---|---|---|
| 000 (field/town; the shop route's AREA007) | 11.29 s | 98.954 s | 98.534 s | 0.881 of a body after the loop start: one body length from the song's start, 0.44 s short | plays the intro where the disc plays the body's last 11.7 s. Here the two are nearly the same music (correlation 0.73..0.95), so the loop is close - but 0.44 s short, plus a 23 ms lead-in |
| 153 (battle; the dragon route's fight) | 7.45 s | 42.791 s | 49.920 s | 0.992 of a body after the loop start: intro plus one body, 0.32 s short | **replays the 7.4 s intro every loop**; its last 1.5 s differs most from the body's end (correlation 0.17..0.57). Cut while loud (last frame -14 dBFS), then 6 ms of lead-in |
| 141N (title, once) | - | - | 33.38 s | at the song's end | plays once, as the disc does |

**H1 holds:** the PC replays intros, and its loops are a few tenths of a
second short. Across the set the MP3s show it where they repeat inside
themselves: 14 files repeat their loop body with a period within 5% of the
sequence's (`loops.json`, `loop`). 3 end on a whole number of passes after
their loop start (`003` 2.015, `014` 1.998, `036` 1.985); **11 end part-way
through a pass** (`013` 2.21, `051` 1.33, `060` 1.33, `063` 1.34, `064` 1.80,
`079` 2.12, `082` 1.34, `085` 1.93, `090` 1.18, `130` 1.74, `144` 1.32), so
the PC loop of those jumps from mid-phrase back to the intro. 75 of the 156
looping songs have a sequence intro of a second or more (48 of 5 s or
more). The other files either hold less than about two passes or repeat
too inexactly for this test; their loop points need a render each.

**The sequence's nominal timing is not the PSX's.** The renders' loop
bodies are 0.981 (song 0) and 1.010 (song 153) times what the SEQ's ticks x
tempo give. A loop-point table cannot be computed from the SEQ alone to
better than about 2%; it has to be measured.

**H2, the seam:** a cut while sounding (6.1) followed by the file's lead-in
(23 ms for `000`, 6 ms for `153`). Whether it is heard is the owner's call
(section 9).

### 7.3 Clock

Against Mednafen every MP3 drifts linearly, -7.6 to -7.8 samples a second
on all three tracks (`compare_NNN.json`, `drift`): the MP3s run 175 ppm
slow - far below hearing (0.003 semitone), but a loop table in MP3 samples
must be measured in the MP3, not converted from the disc. Two MP3s of the
same sequence and bank (`151`/`153`, `023`/`052`, `054`/`071`, `115`/`118`;
`tools/bgm/twins.py`, `analysis/bgm/twins.json`) do not drift against each
other (0..1 sample over a minute) and correlate at 0.986..0.999, yet are not
sample-identical (residual -14 to -25 dB): two takes on one capture chain,
not one file copied.

## 8. The timbre test (step 4, H3, H4, H5)

### 8.1 The renderer chosen

**Mednafen 1.32.1** (GPL-2.0; Beetle PSX's parent), already on this machine
in the sibling's `mednafen/` with its BIOS images; used as a tool, nothing
of it enters the repo, nothing installed. It runs the `psx-jp` disc itself,
so sequence, sound bank, the SPU's interpolation and envelopes and the
game's own reverb set-up are all the game's. `tools/bgm/mrun.sh` runs it
with a base directory of its own under a scratch directory (the owner's
Mednafen settings untouched), records with `-soundrecord` at 44.1 kHz
through DirectSound with the volume at 0 (the recording is taken before the
volume), and stops it after a set time.

**Making a song play.** The boot (BIOS, `LOGO.EXE`'s logo movie, then the
title from `GAME.EMI` section 1) plays the title music, song 141, from
`DEMO.EMI`'s own SEP (zeroing `DEMO.EMI` in a scratch copy silences it;
zeroing `BGMOPN.EMI` does not). `tools/bgm/patch_disc.py` writes a scratch
copy of the image whose title loads a BGM file in place of `DEMO.EMI`
(`0x801D0CA4`, `addiu a0, zero, 0x25F` -> the file id), plays song N
(`0x801D0CD0`, `0x8D` -> N) and stays on the title (`0x801D0DD4`, state 3's
frame count `0x384` -> `0x7FFF`). The title's graphics are then wrong; its
music is the song alone, no effects.

**Trap, for whoever patches a disc image next:** a few patched bytes are
silently undone. Mednafen's drive applies the sector's L-EC error
correction, which repairs a small change back to the original: six
experiments with 2- to 8-byte patches (the song id, the song table, the
file tables in `SLPS_009.90` and `LOGO.EXE`) changed nothing, while zeroing
a whole file did. `tools/bgm/cdsector.py` regenerates each patched sector's
EDC and ECC (ECMA-130; it reproduces the disc's own on five untouched
sectors), and then the patches take.

Renders, in `analysis/bgm/renders/`: song 0 (330 s, two full passes), song
153 (200 s), and an unpatched boot (100 s), which carries song 141 as the
game plays it.

### 8.2 Results

Aligned on the song's first sound, refined on the waveform, drift fitted
(`tools/bgm/compare.py`; `analysis/bgm/compare_NNN.json`; spectra and
spectrograms in `analysis/bgm/plots/trackNNN_spectra.png`):

| | 000 | 141N | 153 |
|---|---|---|---|
| Waveform correlation, MP3 against render, 2 s windows along the song | 0.67..0.88 | 0.26..0.79 | 0.62..0.87 |
| Spectrum 100..8,000 Hz, MP3 minus render (gain-matched) | -0.47 dB, sd 0.77 | -0.16 dB, sd 0.86 | -0.23 dB, sd 0.98 |
| MP3 3 dB below the render from | 10.5 kHz | 8.4 kHz | 11.8 kHz |
| MP3 10 dB below the render from | 12.0 kHz | 11.3 kHz | 13.3 kHz |
| Band 16..19 kHz, MP3 against render | -18.0 dB | -16.6 dB | -11.7 dB |
| Level (RMS), MP3 / render | -20.5 / -24.6 dBFS | -23.5 / -27.3 dBFS | -20.0 / -24.1 dBFS |
| DC, MP3 / render | -6e-5 / -7e-4 | -6e-5 / -4e-4 | -5e-5 / -2e-4 |

- **H3, reverb: no difference measured.** A different reverb would change
  the waveform and the spectral balance; the MP3s correlate with the renders
  over whole songs and match them within a dB below 8 kHz, so they were made
  with the reverb the game sets up - for these songs, on the title screen.
  Section 5's second item, whether the game changes reverb per scene, is
  **not** measured.
- **H4, codec: the difference is the top octave.** Below about 10 kHz MP3
  and render agree within a dB; above it the MP3 falls away (its band limit
  varies frame to frame between about 11 and 18 kHz - the spectrograms show
  it), while the render carries content to 22 kHz. The MP3s are about 4 dB
  louder than Mednafen's output (a level, not a difference in the music).
- **H5, live mixing: not tested.** No effects play in the renders; it needs
  a capture in battle.
- **How the MP3s were made: not settled.** The waveform match says the same
  synthesis and reverb; the steady 175 ppm clock offset and the
  not-identical twins (7.3) suggest a capture chain with its own clock. The
  lead-ins' floor (median -85 dBFS RMS, `analysis/bgm/floors.json`) is below
  an analogue capture's usual hiss, which leans the other way.

### 8.3 P10: the PSP's sequences (I32)

`tools/bgm/discs_compare.py`, `psp_notes.py`, `psp_cmp2.py`, `psp_cmp3.py`
(`analysis/bgm/discs_compare.json`, `psp_notes*.json`):

- **The Western PSX discs carry the Japanese music bytes**: `psx-us`,
  `psx-eu`, `psx-fr` and `psx-de` (images in the sibling's `isos/`) equal
  `psx-jp` in every VH, SEQ and VB section of the 124 files compared.
- **The PSP re-containers all of it**, identically on `psp-jp` and `psp-eu`:
  the VB gains a 192-byte `pBVC` header in front of **the same sample bytes**
  (119 of 119 files), the VH becomes `PPHD`, the SEP becomes `pPMS`, a table
  of Standard MIDI Files (105 of 476 sub-songs at ten times the tick
  resolution). So every section hash differs, and the comparison is of the
  events.
- **The three tracks**: songs 0, 141 and 153 play the same notes on the same
  programs on the PSP (153's pitch-bend events: 331 on the PSX, 310 on the
  PSP).
- **All 476 sub-songs**: notes the same in 459, tempo changes in 476,
  program changes in 472; the loop markers rewritten (99 = 0 / 1 with 6 = 0).
  Where notes differ: **song 45** (`BGM037` sub 0) moves its channel-0 part,
  780 notes on program 0, to channel 9 - the General MIDI drum channel, if the
  PSP's player treats it so: the best candidate for P10's "instrumentation".
  Also song 112 (12 notes removed), 144 (one removed), 50 and 135 (four
  added), 157 (starts 35 ticks earlier, one added), a short cue shared by
  89, 93, 119, 120, 128, 136, 138, 143 and 147 (a note or two ended 18..28
  ticks earlier), 121 (one note ended earlier); and more pitch-bend events in
  142 sub-songs (5,508 PSP-only against 4,183 PSX-only). Whether any of it is
  audible needs the PSP's player; not run.

## 9. The listening set, and what the owner should do (step 5)

In `analysis/bgm/listen/` (it never leaves `analysis/`, rule 1), with a
`README.txt` saying what to listen for, in plain words:

| Order | Files | Window | What it tests |
|---|---|---|---|
| 1 | `141_disc.wav`, `141_mp3.wav` | 37 s, the whole song | H4 (the top octave), H3 (the room), anything else |
| 2 | `153_disc.wav`, `153_mp3.wav` | 45 s; the PC restarts at 23.6 s | H1 (the replayed 7.4 s intro), H2 (cut while loud), H4 |
| 3 | `000_disc.wav`, `000_mp3.wav` | 50 s; the PC restarts at 17.3 s, the disc loops at about 29 s | H1 in its subtle form (0.44 s short; an intro close to the body's end), H2 |
| 4 | `000_seam.wav`, `153_seam.wav` | 16 s: the file's last 8 s, then its first 8 s | H2 alone |

Each pair is sample-aligned (drift corrected) and loudness-matched: the
disc file scaled to the MP3's RMS over the window (+4.2 dB for 000, +3.9 dB
for 141 and 153; no peak needed more). The "mp3" file is the PC's playback
over the same span, the decoder rewind applied (`tools/bgm/build_listen.py`).

**For the owner:** play each pair disc, mp3, disc. Per pair: same, slightly
different or clearly different, and in what - the loop, the seam, the room,
the brightness, something else. Then the question section 10 feeds: is the
disc's music worth having as a choice?

## 10. What this settles for the plan

- **The decoder swap's target** ([`platform-layers-plan.md`](platform-layers-plan.md)
  2.4): MPEG-1 Layer III only, 44.1 kHz, 128 kbit/s CBR, plain stereo (no
  joint stereo, so no M/S or intensity stereo is ever exercised), no CRC,
  no free format, no gapless header to honour. Any conforming Layer III
  decoder covers it. The ledger entry's PCM error bound is per file against
  the original decoder's output; `ffmpeg`'s decode gives the same sample
  count and stands in until the original's own output is captured.
- **The cheap fix, if the owner hears H1:** a table of our own, per looping
  track, of (loop start, loop end) in MP3 samples, and `Music_Decode`
  seeking to the loop start at the loop end instead of to 0 - a divergence,
  with a DIV entry. Fully correct for a file holding at least one whole body
  after its loop start (the 11 mid-pass files of 7.2 do: 1.18..2.21 passes);
  approximate for files cut just short of one (`000` lacks 0.44 s, `153`
  0.32 s; a short crossfade would stand in). The table must be measured
  (7.2), which `tools/bgm/` does per song in about four minutes of emulator
  time: the 156 looping songs, about ten hours unattended.
- **Whether the synth earns its cost on fidelity:** on these three songs the
  MP3s carry the same music, instruments and reverb; the disc differs in the
  top octave (above about 11 kHz) and in correct loops, and the loops can be
  had without a synth. So on fidelity the synth buys the top octave; it
  stays the disc-only build's requirement either way. The owner's ear says
  whether the top octave matters.
- **The renders are the synth's test fixture** (section 4): songs 0, 141 and
  153 from the game's own SPU set-up, and the method to make more.

Left open: song 21's use, file `166`'s source, per-scene reverb, H5, the
PSP's player for P10, and the original decoder's own PCM.

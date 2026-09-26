# Music: the PC's MP3s against the disc's sequences

**Status:** DRAFT (2026-09-26; a method, nothing measured yet. [`IDEAS.md`](IDEAS.md) I23)

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

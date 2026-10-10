# Sound from a disc: the banks (VAG) and `SND/` (XA)

**Status:** STABLE (2026-10-08, a cloud session; the JP, US, EU-English, FR and DE discs and the PC's `DAT/`, `SND/` and `BOF3.exe` measured; section 10, the effects against the SPU model, measured 2026-10-10)

[`unified-data-plan.md`](unified-data-plan.md) step 6. Two transforms let a
PlayStation disc stand in for the PC install's audio:

- `wave-from-vag` (`tools/vag.py`) builds the PC's kind-2 bank chunks from the
  disc's VAB sections. It gives **901 of 901 byte-identical** from the JP or
  US disc, and 893 from the EU-English, French or German disc.
- `wave-from-xa` (`tools/xa.py`) cuts and converts the disc's XA streams into
  `SND/`'s WAVs by the boot EXE's own cut table. It gives **880 of 880
  byte-identical** from any of the five PSX discs.

Neither needs a tolerance or a hash of its own: what they write is the PC's
bytes. What stays PC-only is in section 4.

## 1. The PC's bank chunk, as the engine reads it

`Snd_LoadBank` `0x587CD0` (ours, [`src/game/display_env.cpp`](../src/game/display_env.cpp))
copies the first `0x380` bytes of a kind-2 payload over bank record
`Sound_Banks + (tag - 1) * 0x384`. It mallocs the rest, and for each of the
64 voice entries whose first dword is non-zero, makes that dword a pointer and
the second dword the DirectSound buffer from `SndBuf_FromWave`.
`Sound_PlayEffect` `0x587740` ([`sound.md`](sound.md) 1) reads the cues. So:

```
0x000  24 cues x 16 bytes: four voice words each, 0 = none
       word: bits 0-7 the voice entry (0xFF = stop the channel)
             bits 8-12 the channel (here always 16 + n)
             bit 15 loop
             bits 16-31 the playback rate in Hz
0x180  64 voice entries x 8 bytes: (offset in the payload, size); entry 0 never used
0x380  the WAVs, contiguous in voice order, each RIFF / PCM / mono / 16-bit,
       header rate 22,050 whatever the sample's (the cue word gives the rate)
```

This answers [`DAT_CONTAINER.md`](DAT_CONTAINER.md)'s open "bank descriptor
area". Its "61 slots from `0x188`" is the same voice table seen from entry 1,
and `tools/dat.py`'s `BANK_TOC` reads it correctly for that reason.

**Measured** (`tools/vag.py compare`, below): 901 chunks in 728 containers,
24 x 4 = 86,496 cue words, 4,779 WAVs, 123 banks with no voice at all.

On the disc, a bank is three EMI sections in a row:
- type 6, the VAB header (`pBAV`: programs, tones of 32 bytes, the sample size
  table of 256 u16 in units of 8 bytes);
- type 8, the cue table, 4 bytes a cue: flags; `0x80` | program; tone << 4 |
  priority; chord << 4 | first voice;
- type 7, the VAB body: the VAG samples back to back.

**Pairing.** The n-th kind-2 chunk of a PC container is the n-th 6 / 8 / 7
group of the same-named EMI, leaving out the groups with a type 10 (SEQ, the
music the port dropped). The chunk's tag is the group's `dest` word in **901 of
901** (728 containers). The recipe's census never paired audio; this pairing
is `vag.groups`, and the byte identity below is its proof.

## 2. `wave-from-vag`

### 2.1 The VAG format, as implemented

A sample is 16-byte blocks of 28 samples:
- byte 0 is the shift (low nibble) and the filter (high nibble, 0..4);
- byte 1 is the flags: bit 0 end, bit 1 repeat, bit 2 loop start;
- bytes 2..15 are 28 signed nibbles, low nibble first.

Each output is the nibble placed at the top of 16 bits and shifted right by
`shift`, plus `(s1*k0 + s2*k1) / 64` from the last two outputs. The filter
pairs are (0, 0), (60, 0), (115, -52), (98, -55), (122, -60). `vag.decode` is
the SPU's integer form of this, clamped.

### 2.2 The port's converter (measured, not assumed)

`vag.decode_port` reproduces **all 4,779** of the port's WAVs sample for
sample. It differs from the SPU's form in four ways, each needed:
- it runs the same recurrence in double precision;
- its history is the unrounded, unclamped value;
- each output is `(int)(x + 0.5)`, which truncates toward zero, so a negative
  half rounds up;
- it keeps the output to 16 bits by wrapping, not clamping.

How the model was found: a least-squares fit of each PC sample against the
SPU decode came out as identity plus noise of up to ±44. Filter 0 blocks were
off by exactly +1 on every negative value. Carrying the float history made the
noise vanish. One sample in `AREA000`'s sample 15 needs the wrap: unclamped it
is 35,924, and the PC holds -29,612.

Against the SPU's decode, over the 801 distinct samples the PC carries
(26,416,936 sample values):
- 92 % differ;
- the RMS difference is 17 LSB;
- 49 % are within 8 LSB;
- outside the wraps the largest difference is 981, where the histories
  diverge after a clip.

**The wraps are a defect of the port's.** 9 samples, in 5 distinct sounds,
wrap where the SPU would clamp: a full-scale click. Those sounds are in the
banks of 76 containers, `AREA000` among them (section 6).

### 2.3 Which blocks, which samples, which words

These rules are measured on all 901 banks.

- **Blocks.** A sample is converted from block 1, because block 0 is the zero
  block every VAG opens with.
  - A sample with a block flagged exactly 1 (end, no repeat) stops after that
    block. That drops the trailing flag-7 block Sony's tools append. 4,452
    samples end this way.
  - A sample without one is a repeating sample, and is converted whole. 327
    samples end with flags 2, 3.
  - One sample has a loop-start flag mid-way (flag 6) and an end-with-repeat
    (3) before its last block. It is converted whole, as the PC has it, in 4
    areas.
- **Loop bit.** The loop bit is "no end-without-repeat block". This fits all
  10,536 voices played.
- **Rate.** The rate is `int(44100 * 2 ** ((key - centre + fine / 128) / 12))`.
  Here `key` is the tone's lowest note (tone byte 6), `centre` is byte 4 and
  `fine` is byte 5. This fits all words.
- **Cue words.** PSX cue i becomes PC cue i. Its chord nibble c (1, 3, 5, 7)
  plays `c // 2 + 1` consecutive tones from its tone. Word j is on channel
  16 + first voice + j.
- **Silent samples become stops.** A tone whose sample decodes to all zeros
  becomes a stop word (`channel << 8 | 0xFF`), and that sample gets no WAV.
  There are 6,711 such words. Only three sample contents are silent: two key-off
  dummies of 352 and 528 bytes, and a zero-length one. No played sample is
  silent.
- **Which samples get a WAV.** Exactly the samples some non-stop word plays,
  in index order (901 of 901 banks). 1,802 of the VABs' 6,581 samples are
  never converted.

The PC's bank keeps none of the tone's volume, pan or ADSR envelope, and no cue
priority (`region-diff.md` 8.3). DirectSound plays each sample flat. That is
the port's format, and it is why an SPU model (I23) is a different thing from
this transform.

### 2.4 Measured

```
python tools/vag.py compare --disc "<JP>.cue" --dat DAT --hardware   (1 min 34 s)
banks: 901 identical, 0 differ, 0 unpaired
```

| Disc | Identical | Differ |
|---|---:|---|
| psx-jp | **901** | 0 |
| psx-us | **901** | 0 |
| psx-eu-en, psx-fr, psx-de | 893 | 8: `AREA100`, `112`, `135`, `138`, `147`, `168`, `170`, `188` |

The 8 are [`region-diff.md`](region-diff.md) 8.4's PAL sample swap. The
transform builds the PAL bank faithfully; its hash is not the recipe's, so the
importer does not take it (section 5). The cue-priority change (8.3) does not
reach the PC's format, so it costs nothing.

## 3. `SND/` and the cut table

### 3.1 What names a file

`Sound_LoadStream(id)` `0x587910` takes a kind (id >> 12) and a name
(`[0x6653B0 + 4*kind][id & 0xFFF]`), then reads `SND\%s.DAT`. Kinds above 0
play the WAV through `SndStream_Play`; kind 0 goes to the MP3 music stream.
`BOF3.exe`'s three name tables hold:

| Kind | Names | Files | On the disc |
|---:|---:|---|---|
| 0 | 11 | `OYASUMI` .. `KARA` | `BIN/SCE_XA/S_XA00.STR` |
| 1 | 880 (875 distinct) | `NNN_KK` | `BIN/BMAG_XA/MAGIC00.STR` |
| 2 | 5 | `VOICE00` .. `04` | `BIN/SCE_XA/VOICE.STR` |

Five names (`063_02` .. `063_06`) are each two stream ids. The two disc clips
behind each pair are sample-identical, so that is not a defect.

### 3.2 The table on the disc

The table is in the boot EXE. At PSX `0x80183BB4` (`SLPS_009.90` file
`0xF0BB4`), three pointers in kind order point to the per-file clip lists:
- `S_XA00` at `0x8018433C`;
- `MAGIC00` at `0x80183C3C` (file `0xF0C3C`);
- `VOICE` at `0x80183C10`.

**Each list is one per channel:**
- u16 start sectors, counted within the channel and rising from 0;
- the last of them flagged `0x8000`;
- then the channel's length in sectors.

Clip n of a kind is the n-th clip counting channel by channel. A clip's length
is the next start, or the channel's length for the last clip.

**How it was found.** The PC's file sizes gave each clip's length. An energy
match of every PC clip against the 16 decoded channels gave candidate starts
(0, 23, 44, 67, ...). The starts, as consecutive u16 values, occur once in the
boot EXE, at `0xF0C3C`. The flagged structure came from reading the words
around it.

**The proof.** The table's 880 MAGIC00 clips, in channel order, are the
PC's kind-1 name table in order. Every clip length gives that name's PC file
size as `46 + 4704 * sectors`: **880 of 880**. The 16 lengths equal the
channels' audio sector counts on the disc (923 .. 963, 14,856 in all).

| Stream | Channels listed | Clips | Totals | On the disc |
|---|---:|---:|---|---|
| `MAGIC00.STR` | 16 of 16 | 880 | 14,856 sectors | 4-bit mono 37.8 kHz, strict 16-way interleave, 552 null sectors at the end |
| `VOICE.STR` | 5 of 5 | 5 | 71, 63, 61, 60, 42 | 4-bit mono 37.8 kHz |
| `S_XA00.STR` | 4 of 8 | 11 (8 + 1 + 1 + 1) | 775, 5045, 90, 5052 | 4-bit stereo 37.8 kHz. Channels 4-7 (128, 79, 76, 57) are not listed; they have the lengths of channel 0's last four clips |

**Every disc.** `xa.stream_lists` finds each list by its shape and the disc's
own channel lengths, with no addresses held:

| Build | Boot EXE | `MAGIC00` lists | `VOICE` | `S_XA00` |
|---|---|---|---|---|
| psx-jp | `SLPS_009.90` | `0xF0C3C` | `0xF0C10` | `0xF133C` |
| psx-us | `SLUS_004.22` | `0xED2C4` | `0xED298` | `0xED9C4` |
| psx-eu-en, fr, de | `SLES_013.04` / `.19` / `.20` | `0xED798` | `0xED76C` | `0xEDE98` |

The XA audio itself, every audio sector's 2,304 data bytes by channel, is
identical on all five discs.

### 3.3 XA, as implemented, and the port's conversion

**The XA sector, as implemented.** An XA audio sector is Mode 2 Form 2:
- the subheader (file, channel, submode, coding);
- then 18 sound groups of 128 bytes.

A four-bit group is 16 parameter bytes, then 28 words of 4 bytes:
- units 0..7 take their parameter from bytes 4..11: shift in the low nibble,
  filter in the high;
- sample j of unit u is in word j, byte u // 2: the low nibble for an even
  unit, the high nibble for an odd one;
- mono plays the units one after another (224 samples a group, 4,032 a
  sector);
- stereo alternates the units, even units left.

The prediction is the SPU's first four filter pairs, **floored** (`>> 6`).
The +32-rounded form matched only 4,113 of 7,728 phase-0 samples in `004_00`;
the floored form matched all of them.

**The port's conversion** (`xa.clip`, `xa.resample`) is exact:
1. Decode the clip's sectors from a fresh history. The clips are separated by
   silence, so the stream's own history would give the same values.
2. Output 0 repeats input 0.
3. Output k (k ≥ 1) interpolates linearly from input i to i + 1. The position
   t = (k - 1) * 37800 / 22050 is computed in double and **stored as a
   float**, and i is t's integer part. The value is truncated toward zero.
4. There are `sectors * 2352 + 1` outputs. That one extra sample is the "46"
   in `46 + 4704 n`.

The float position is the whole difference from exact rational
interpolation. The exact positions left 4,461 of 54,096 samples of `004_00`
off by up to 5. The float-stored position leaves 0 (the
`f32(k1*S64)` / `f32(k1*12/7)` rows of the scratch grid).

**Measured:**

```
python tools/xa.py recipe --disc "<JP>.cue" --exe BOF3.exe --snd SND
880 identical, 0 differ (0 of 35,438,464 samples)
```

`xa.py build` from each of the five discs writes 880 files, each checked
against the recipe. The US build compared file by file with the PC's `SND/`
is 880 of 880 identical.

### 3.4 The recipe

`recipes/pc-zh.snd.toml` is generated by `xa.py recipe`. It has one row per
`SND/` file (898):
- name, kind, stream id;
- stream, channel, start, sectors;
- the PC file's size and sha256;
- `how`: `wave-from-xa` for the 880 WAVs, `pc` for the rest, with a reason.

A `wave` hash and the distance are written only where the conversion would
not reproduce the PC's file. Today there is no such row. The recipe holds
names, numbers and hashes only.

## 4. What stays PC-only, and why

| Files | What they are (measured) | Why PC-only |
|---|---|---|
| The 11 kind-0 files: `OYASUMI`, `AKURYOU`, `ITEM01`, `NAKAMA02`, `MIMI01`, `WIN`, `SIPPAI01`, `GAAN01`, `PURE`, `DRAGON`, `KARA` | MP3s (MPEG-1 layer III, 44.1 kHz stereo, 128 kbit/s) of the 11 `S_XA00.STR` clips, in table order: channel 0's eight, then `PURE` = channel 1 (5,045 sectors, 269.0 s), `DRAGON` = channel 2, `KARA` = channel 3 (5,052, 269.4 s). The durations agree to within 30 ms (ffprobe). The decoded MP3 against the XA decode correlates at 0.994-1.0 for the short ones (`jingle.py`, scratch). `PURE` and `KARA` correlate at 0.98-0.99 once aligned, with a steady drift of 3.5e-5 (about 9 ms over the track), which is the port's resampling. | The engine plays kind 0 through the MP3 decoder. Making them from a disc needs an MP3 encoder (none here; LAME is LGPL, see `LICENSING.md` section 4) or an engine change (section 7). |
| `019_02` | A 20-sector WAV in `NNN_KK` form. No stream id names it (group `019` has no other file), and it matches no clip of `MAGIC00`, `VOICE` or `S_XA00`, in either channel of the stereo stream (probe of 1,500 phase-0 samples at every sector start; best mean error about 6,100, against 0 for a true clip). | No source. The game never opens it. |
| `dir1` | A CR LF list of the 876 numbered file names. | Not audio; nothing reads it. |

`S_XA00.STR`'s channels 4-7 are on the disc and in no list. Nothing on the PC
corresponds to them.

## 5. The importer, and what a disc-only player gets

The hook is the last commit of this step, six lines in `tools/importer.py`:
- an import of `vag` and `xa`;
- in `build`'s source loop, `vag.importer_source`: a `bank` chunk from a PSX
  disc source, **kept only when its hash is the recipe's**;
- after the loop, `xa.importer_snd`: `base/snd/NAME.DAT` from the first PSX
  disc among the sources.

| Sources | Banks from the disc | `base/snd/` | Verify |
|---|---:|---|---|
| JP disc, PC `DAT/` | **901** (3,072 chunks from psx-jp) | 880 from psx-jp | 742 of 742 |
| FR disc, PC `DAT/` | 893, plus the 8 PAL areas from the PC | 880 from psx-fr | 742 of 742 |
| US disc alone | **901**; banks no longer in the missing list | 880 from psx-us | 703 containers written; the rest are step 3's (`importer.md` 4) |
| JP disc alone | 901 | 880 | 706 containers written |

A disc-only player now gets:
- every sound bank, byte-identical to the PC's (JP or US disc; a PAL disc
  lacks 8 unless the owner takes the PAL banks);
- all 880 `SND/` WAVs, byte-identical.

They still lack the 11 jingles (section 4), and the music, which is step 9 or
the PC's `BGM/`.

`importer.py`'s `manifest.toml` records each disc-built bank as
`psx-xx:wave-from-vag:EMI#section`. It does not record `base/snd/`: the
manifest writer is step 3's, and `recipes/pc-zh.snd.toml` plus the build's
check stand in for it until then.

## 6. Owner's calls

1. **The PAL banks.** From an EU-English, French or German disc, 8 area banks
   are the PAL sample swap (`region-diff.md` 8.4). The importer refuses them by
   hash, so a PAL-only player lacks those 8 containers' banks. Should they be
   accepted as that build's? 8.4's verdict was to keep JP / US.
2. **The wrap clicks.** 9 sample values in 5 sounds (76 containers' banks)
   wrap where the SPU clamps. The PC plays a full-scale click; the PlayStation
   does not. Clamping would be a one-line option in `vag.decode_port`, and a
   divergence (a DIV entry, and those banks no longer byte-identical to the
   PC). Is it worth a listen first?
3. **The 11 jingles for a disc-only player.** Three ways:
   - an MP3 encoder in the import, which is a licensing question;
   - the engine plays kind-0 streams from a WAV;
   - they stay PC-only.

   `PURE` and `KARA` are 4.5-minute stereo tracks, about 47 MB each as 44.1
   kHz PCM. A WAV path for them would have to stream, not load whole as
   `SndStream_Play` does.

## 7. The engine change `base/snd/` needs (not made)

The exe builds `SND\%s.DAT` itself (format string at `0x666F9C`,
`Sound_LoadStream` `0x587910`), relative to the working directory. For the
cache to be read, `Sound_LoadStream` must open `base/snd/NAME.DAT` from the
cache root. A plain cache-or-install fallback is enough for the 11 kind-0 MP3s
while they stay PC-only. That is the same seam as `LoadDatFile`'s second prefix
(step 4), and `BGM\%03d.DAT` will want it too.

## 8. For the other files

- **`unified-data-plan.md`**:
  - section 4: the VAG and XA rows are done, 901 / 901 and 880 / 880 with no
    tolerance, and the cut table was found in the boot EXE (not beside the
    `SND` strings in the PC's exe);
  - section 6: `base/snd/`, from a disc or the install;
  - step 6 in section 8 is done, pointing here;
  - section 3 item 3: `wave-from-vag` / `wave-from-xa` are exact.
- **`importer.md`**:
  - section 3's `bank` row: from any PSX disc by `wave-from-vag` (901 JP / US,
    893 PAL);
  - section 4's "US disc alone" row: banks are no longer missing, and
    `base/snd/` is written;
  - section 1: the second recipe `recipes/pc-zh.snd.toml`;
  - the manifest should record `base/snd/` (section 5 here).
- **`DAT_CONTAINER.md`**:
  - section 1's kind-2 layout: cues at `0x000`, voice table at `0x180` (the
    "61 slots from `0x188`" are entries 1..61), and the "bank descriptor area"
    open item is answered;
  - section 5:
    - the cut table;
    - the 11 named files are MP3s of `S_XA00.STR`'s clips, which answers
      "presumably recorded jingles", `KARA` / `PURE` and "`S_XA00.STR` has no
      identified PC counterpart";
    - `019_02` is unreferenced and `dir1` is a name list;
    - the `NNN` = spell hypothesis is untouched;
  - section 4: the bank descriptor area is closed.
- **`STATUS.md` / `HANDOFF.md`**:
  - step 6 is done: banks and `SND/` exact from a disc;
  - next: the engine reading `base/snd/` (section 7), with the step-4 prefix.
- **`owner-review.md`**: section 6's three calls; for item 2, a listen to one
  of the 5 wrapped sounds (`AREA000`'s bank holds one).
- **`fixtures.toml`**: a pc-zh `SND/` manifest (898 files, sizes and sha256).
  The recipe's rows are it in another shape. `BGM/` likewise.
- **`known-defects.md`**: the port's bank converter wraps instead of clamping
  (9 values, 5 sounds), if the owner hears it.
- **`sound.md`** section 1: the cue word's channel field is always 16..23 in
  the shipped banks (the PSX voices 16 + n), so the `>= 23` wrap is reached
  only by channel 23.

## 9. Tools and scratch

- `tools/vag.py`:
  - `decode`, `decode_port`, `trim`, `bank`, `groups`, `bank_from_disc`,
    `importer_source`;
  - `compare --disc --dat [--hardware]`.
- `tools/bgm/se_census.py` and `tools/bgm/host` `se_render` (section 10).
- `tools/xa.py`:
  - `sectors`, `channels`, `decode` (mono / stereo), `stream_lists`,
    `resample`, `clip`, `pc_names`, `build`, `importer_snd`;
  - the commands `ls`, `recipe`, `build`.
- Scratch (deleted or kept under `/workspace/scratch/step6/`, never
  committed): the per-channel decodes, the energy match, the jingle
  correlation (`jingle.py`, `drift.py`) and the resampler grid (`grid.py`).

## 10. The effects through the SPU model: the reading and the measurement (2026-10-10)

[`sequenced-music-plan.md`](sequenced-music-plan.md) section 7 opens it: with
the SPU model in the engine, `Sound_PlayEffect` could key the disc's tones on
voices 16..23 with `SsUtKeyOnV`'s semantics and share the music's reverb - "a
separate DIV; it needs `SE_Play`'s cue set-up read (the sibling's
`SOUND_CUES.md` has most of it)". **Not built**: the plan names it, does not
specify it, and the volume path is unread (below). This section is the reading
and the measurement that come first.

**The reading** (the sibling's `SOUND_CUES.md`, read 2026-10-10; facts about
`SLPS_009.90`, not re-verified here): `SE_Play(cue)` `0x8015E908` -
`cue = flags << 12 | bank << 8 | id` - runs the bank's handler
(`SE_CueSetup_Bank0..6`, table `0x80182CA4`), which fills a voice block from
the cue's 4-byte entry (VAB override, pan flag | program, tone | priority,
chord | first voice) and the tone's attributes; a panned cue scales each
voice's `volL / volR` by the pan (`volL * (0x80 - pan) >> 7`); the voices are
keyed by `SsUtKeyOnV(voice, vab, prog, tone, note, fine, volL, volR)`; a new
cue on the same primary voice is dropped when its priority is lower and that
voice is still keyed (`SE_PollKeyStatus` `0x8015DA34`, once a frame). The
port's format keeps the cue layout and drops the rest (2.3). What
`SsUtKeyOnV` does with `volL / volR` against the tone's own volume and pan,
and where the 0x17FF reset lands, is **not read** - the one thing a build
would need first, from the boot EXE (libsnd 3.7's `SsUtKeyOnV`, matched in the
sibling's `symbols.toml`).

**The measurement**: `tools/bgm/se_census.py` over every bank group of the JP
disc that has a cue table, every cue word that plays a sample by
`vag.bank`'s own rule, each distinct (sample, ADSR, pitch) rendered through
`psx::Spu` by `tools/bgm/host` `se_render` twice - with its tone's ADSR and
with an envelope full at once and held, which is what a flat WAV amounts to -
at the cue word's rate, one-shots to their end and repeating samples for 2 s:

    cmake --build <host build> --target se_render
    python tools/bgm/se_census.py --disc "<JP>.cue" --se-render <host build>/se_render --work <scratch>   (1 min 53 s)

| | Result |
|---|---|
| banks, cue words that play | **901 banks, 10,536 words** - the counts of 2.3 (every PC bank, every voice played), so the census covers what the PC plays |
| distinct (sample, ADSR, pitch) | 1,164 |
| ADSR | **10,359 of 10,536 words (98.3 %) are `80FF / 1FC0`**: an attack at the fastest rate (full in 0.4 ms), no decay below full, a sustain that holds, release at the fastest rate. 15 pairs in all; the next is `9BFF / 5FC0` (124 words: a slower attack). |
| the envelope's own effect | energy with the ADSR against flat: **10,532 words within 0.1 dB**, 4 between -1 and -3 dB (one tone, `AEFF / 5FC0`, -2.4 dB: an attack of 256 ms). Attack to 0x7F00: 0.4 ms in every quartile, 255.8 ms at most |
| repeating samples | 558 words: on the PSX they sound until keyed off, then release at the tone's rate (`5FCx`: 15 release rates); the PC stops its buffer dead |
| the reverb bit (tone mode bit 2) | **4,486 words (42.6 %)**: the PSX sends them into the reverb the music uses (Room, `libsnd-reading.md` 6.2); the PC has no reverb |
| pan | 4,146 words (39.4 %) have a tone pan other than 64 (centre); the PC plays every effect centred |
| volumes | tone volume quartiles 50 / 80 / 100 of 127 (minimum 0); program volume 107..127 (median 127); every VAB master volume 127. The PC plays every sample at its full level |

So the envelope, which a flat WAV was assumed to lose (2.3), is almost never
there to lose: the differences the effects would gain from the model are the
**volume** (a quarter of the words at 50 / 127 of the tone volume or less -
how much of it reaches the voice depends on the unread `SsUtKeyOnV` path),
**the pan** (four in ten), **the reverb** (four in ten), **a looping sound's
release** at its stop, and the **priority rule** that drops a lower cue on a
busy voice. Each would be audible and each is a change from the PC's port -
a DIV of its own, as the plan says - and the volume and the priority need
their reading first. The renders are no oracle here: they are of the title,
which plays no effect. Left for the owner and the next session: read
`SsUtKeyOnV` and the handlers' volume, then decide whether the effects move to
the model (`owner-review.md` has no call for it yet).

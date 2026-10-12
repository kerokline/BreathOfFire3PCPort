# The PSP's music banks, and the dropped tone

**Status:** MEASURED (2026-10-12, group MB of the PSP-only round,
[`psp-only-round.md`](psp-only-round.md); branch `psp/mb`). Built in
`tools/vag.py`: `psp_music_vab`, `psp_bank`, `psp_dropped_tone` and a `check`
on synthetic banks. Measured on PSX-JP, PSP-JP and PSP-EU with the scratch
scripts named in section 1. Nothing played in the game, nothing heard.

The PSP keeps each sound bank as a `PPHD` header and a `pBVC` body
([`psp-only-build.md`](psp-only-build.md) 2.1). This doc covers two things.
The first is the 81 music banks, which have to become the VAB that
`tools/seq.py` reads, so that `base/bgm/bank/` can be written from a PSP disc
(group MS, stage B). The second is the one effects bank, shared by 16 battle
containers, whose program 1 lost a tone on the PSP. No game bytes appear
here: counts, offsets and rules only (rule 1).

## 0. The answer

- **`vag.psp_music_vab(pphd, pbvc) -> (vh, vb)`**, final, committed in
  `ffa7cfce`. It returns a VAB header and body that `seq.parse_vab` accepts
  for all 81 banks from either PSP. `seq.bank_bytes` turns that into
  `base/bgm/bank/NAME.DAT`, the same as for a PSX disc.
- **What is exact:** the samples (81 of 81 banks), and each tone's mode,
  centre, fine tune, range, bend range, ADSR and sample index (2,694 of
  2,694 tones). Program and tone counts and the block layout are exact too.
  PSP-EU's 81 built banks are byte-identical to PSP-JP's.
- **The merge is read exactly.** Over all 119 music banks (2,939 tones), the
  PSP's tone volume is `(tv * pm * bm * 32) // 127**2`: the tone, program
  and VAB master volumes. Its pan is `(tp + mp + 64) // 3`: the tone pan,
  the program pan and 64. Both formulas hold for 2,939 of 2,939 tones.
- **The proposed inverse:** master and program volume 127, and each tone's
  volume the merged one, rounded. Program pan 64, and each tone's pan
  `3p - 128`, clamped. Rendered by our sequencer against the PSX-built bank
  of the same songs, over 166 songs at 60 s each:
  - SNR median **41.9 dB**, 8 songs identical;
  - one song below 30 dB: 22, at 16.8 dB. Its bank's two programs carry
    their own pans, and the PSP keeps no way to split those;
  - level within -0.28..+0.45 dB per side.

  Playing the PSP's squeezed pan as it is gives a median 17.5 dB, and the
  stereo image narrows by a median 2.0 dB of side against mid, up to 7.4 dB.
  So the inverse is the proposal. **The owner's call** (`psp-only-round.md` 7 item 2): hear
  it in V4.
- **The dropped tone is derived, not refused.** On the PSX, program 1's
  count says one tone. Its cue chords two, and the second slot holds the
  sound. The PSP's converter kept only the declared tone, a silent one. The
  rule (section 5) is:
  - a chord reaches one slot past a program whose declared tones are all
    silent;
  - the bank has exactly one sample that no tone references;
  - then that sample is the dropped tone, at the declared tone's pitch.

  The rule fires on exactly the 16 groups, on both PSPs, and nowhere else.
  It gives **901 of 901** sound banks identical to PSX-JP's chunks (885
  before). Every value comes from the PSP's own sections. **The owner's
  call** (`psp-only-round.md` 7 item 4): take the rule, or ship as the PSP does.

## 1. What was run

| Step | Script (scratch, `psp_mb/`) | What it measures |
|---|---|---|
| Load | `load.py` | the type 6, 7, 8 and 10 sections of every EMI on PSX-JP, PSP-JP and PSP-EU (809, 813, 813 EMIs with a bank) |
| The record | `peek.py`, `fields.py` | every PSX-JP music bank's VAB (`seq.parse_vab`) paired with its `PPHD`, tone by tone, in program order |
| The merge | `rule.py`, `volpan.py` | candidate formulas for words 10 and 11; the inverses' errors per tone |
| Banks | `caches.py` | `seq.build` of PSX-JP's `base/bgm`; the same songs with every bank from `psp_music_vab`; the field-by-field comparison |
| Renders | `render.py` on `hb/synth_render.exe` (`tools/bgm/host`, built with `clang++` from this branch) | both caches, 166 songs, 60 s each, compared sample-aligned |
| Effects | `dropped.py`, `orphans.py`, `pastcount.py`, `se_check.py` | the 16 groups; every bank's unreferenced samples and past-the-count cues; every PSP effects bank through `bank` against PSX-JP's, before and after |
| Importer | `builds.py`, `cmp_builds.py` | `importer.py build` with this branch's `vag.py` and `668a9b67`'s (section 7) |

## 2. The `PPTN` record

24 words (0x60 bytes) in all 119 music banks of both PSPs (`fields.py`: one
`PPHD` header shape and record size across them; the effects banks were not
checked for it, and `psp_vab` reads any record size). Paired with PSX-JP's VAB
over 2,939 tones (`fields.py`, both PSPs alike):

| Word | Holds | Against the PSX |
|---|---|---|
| 2 | the sample index, less one (-1: sample 0) | 2,939 equal |
| 6, 7 | lowest, highest note | 2,939 equal |
| 8 | the tone's mode (0 or 4; bit 2 is reverb) | 2,939 equal |
| 10, 12 | the merged volume (section 3) | word 10 = word 12 in 2,939 |
| 11, 13 | the merged pan (section 3) | word 11 = word 13 in 2,939 |
| 14 | the centre note | 2,938 equal; the one other is 128 stored as -128 (`BGM130` program 9), so the byte is equal |
| 15 | the fine tune | 2,939 equal |
| 18, 19 | the pitch-bend range down, up | 2,939 equal |
| 22, 23 | ADSR 1, ADSR 2 | 2,939 equal |
| 0, 1, 9, 17, 21 | -1 | in every record |
| 3, 4, 5, 16, 20 | 0 | in every record |

The PSX's tone priority is 0 and vibrato and portamento are 0 in all 2,939
tones, so writing 0 for them is exact. `psp_music_vab` refuses any record
that is not this shape. It raises on a word that should be constant, on
words 10 and 12 or 11 and 13 that differ, or on a field outside its VAB
byte, rather than drop what it does not know.

The counts are the PSX's: every program with tones on PSX-JP has a `PPPG`
entry with the same number of tones, and no other program has one
(`fields.py`). The samples are the PSX VAB body, sample for sample, in all 81
`BGM` banks (`orphans.py`; 1,195 samples, as `psp-only-build.md` 2.1 found).

## 3. The merge and the inverse

**Volume.** Word 10 is `floor(tv * pm * bm * 32 / 16129)`, with `tv` the
tone's volume, `pm` the program's and `bm` the VAB master's. It holds for
2,939 of 2,939 tones (`rule.py`). The float form `int(tv/127 * pm/127 *
bm/127 * 4064)` matches the same 2,939; the rounding forms miss 935, and the
two-step floor 1,736. So the PSP keeps the product of the three, as a
fraction of 127 in 1/32 steps.

Our sequencer multiplies the same three (`src/audio/seq.cpp` `KeyOnNow`,
`VmSetVol`, `VmSetSeqVol`): `vel * (bm * 0x3FFF) / 16129`, then
`* pm * tv / 16129`. So **a split with `bm = pm = 127` loses nothing but the
rounding**. With 127, the first step is exactly `vel * 129`, and the tone's
volume carries the whole product.

The rule: `tv = (2w + 33) // 64`, which is `round((w + 0.5) / 32)`, the
middle of the interval `w` stands for. At velocity 127 and channel volume 127
the voice's amplitude is:
- exact in **1,197 of 2,939** tones;
- off by a median **0.008 dB**, at the 95th percentile 0.052 dB, and at most
  **0.143 dB** (`rule.py`).

A per-program search (choose `pm` to fit each program's tones) makes 2,490
exact, but recovers the PSX's own `pm` in only 275 of 949 programs. The
fields are not recoverable, only the product. Its gain is below a tenth of a
decibel, so it is not proposed.

**Pan.** Word 11 is `(tp + mp + 64) // 3`, with `tp` the tone's pan and `mp`
the program's. It holds for 2,939 of 2,939 tones (`rule.py`). Every VAB's own
pan is 64 (119 of 119), so whether the 64 is a constant or the VAB pan cannot
be told apart. The PSX gives 944 of 949 programs pan 64. The other five are
`BGM016` program 7 (100), `BGM019` programs 6 and 7 (80, 40), `BGMBAT04`
program 11 (10) and `BGMBAT05` program 13 (0).

The rule: program pan 64, tone pan `3p - 128` clamped to 0..127. This is the
lowest pan of the three that share `p` when `mp` is 64, so 64 maps back to
64. Of the PSX's tone pans:
- 1,918 sit at that member of their bin, 458 one above it and 563 two above
  it;
- the tone and program pans are both recovered in **2,101 of 2,939** tones;
- `3p - 127` gets 833, and "toward the centre" 2,093.

The per-side gain error, through the sequencer's pan law (`PanVolumes`, a
square law on three pans in turn), is a median 0 dB, 0.60 dB at the 95th
percentile, and at most 7.96 dB (`rule.py`). One tone of the 17 under a
program pan loses a side: `BGMBAT04` program 11, tone pan 40 under program
pan 10, where the PSX keeps the right at about -40 dB (40/63 x 10/63, then
the square law).

**The renders** (`render.py`). Both caches hold PSX-JP's 166 songs, and only
the banks differ. Each song is played by `synth_render` for 60 s from its
start at the title's level. SNR is the PSX-built render's energy over the
difference's. Width is side over mid energy.

| Banks | Identical | SNR min / median | Level L, R (dB) | Width change (dB) | Window correlation, lowest |
|---|---:|---|---|---|---|
| **The proposed inverse** | 8 | **16.8 / 41.9** | -0.09..+0.45, -0.28..+0.07 | -0.26..+0.59, median 0.00 | 0.881 |
| Volume inverse, the PSX's pans (measurement only) | 25 | 39.9 / 48.3 | -0.09..+0.08 | -0.08..+0.08 | 0.902 |
| Pan inverse, the PSX's volumes (measurement only) | 24 | 16.8 / 45.3 | 0.00..+0.48, -0.28..+0.07 | -0.25..+0.58 | 0.891 |
| The PSP's pan as is, volume inverse | 4 | 7.0 / 17.5 | -0.17..+1.67, -0.07..+2.42 | **-7.35..+0.26, median -1.99** | 0.312 |

The inverse's songs by SNR:

| SNR | Songs |
|---|---:|
| identical | 8 |
| 50 dB and more | 11 |
| 40..50 dB | 89 |
| 30..40 dB | 57 |
| below 30 dB | 1 (song 22, 16.8 dB) |

The volume half alone costs no song below 39.9 dB. The pan half is the larger
part, and song 22 is all of its tail: it is `BGM019`, the bank with two
program pans. The lowest window correlations (0.88..0.90) are in quiet
windows of a few songs; the median window is 1.000 in every run.

## 4. The fields the PSP lost, and what each does in our sequencer

Counted over the 81 banks `base/bgm` uses, PSX-built against PSP-built
(`caches.py`):

| Field | Differs | What `src/audio/seq.cpp` does with it | Effect of the rule |
|---|---:|---|---|
| VAB master volume | 76 of 81 | a factor of every voice's volume | folded into the tone volume; written 127 |
| Program volume | 254 of 906 | a factor of its voices' volume | folded; written 127 |
| Program pan | 5 of 906 | the second of the three pan stages | folded; written 64; the 5 lose their split (section 3) |
| Tone volume | 2,579 of 2,694 | a factor of the voice's volume | the merged volume, rounded (section 3) |
| Tone pan | 758 of 2,694 | the first pan stage | `3p - 128` (section 3) |
| Program priority | 648 of 906 | **not read**: the allocator reads the tone's priority (`Alloc`, `_SsVmAlloc`) | none; written 0 |
| Program mode | 784 of 906 | **not read**: reverb is the tone's mode bit 2, which is exact | none; written 0 |
| VAB pan, attributes | 0 of 81 differ | `song.cpp` loads them; `seq.cpp` never reads them | none; pan 64, attributes 0, as on every PSX VAB |
| Program entries with no tones | 8,182 of 9,462 | `VSetUp` takes their block (0xFF from `bank_bytes`); `VmKeyOn` reads their volume and pan and then keys no voice | none |
| Tone slots past a program's count | **11,782 of 11,802** (20 are zero on the PSX too) | **never read**: `VmKeyOn` walks `t < tones`; `VmPitchBend`, `VmSetVol` and `VmSetSeqVol` read the keyed voice's tone, which is below the count | none; written 0 |
| Tone priority, vibrato, portamento | 0 of 2,694 | priority: the allocator; vibrato / portamento: loaded, not read | exact (0 on the PSX) |

Only the volume and pan products reach the output, so they are where the
renders differ.

## 5. The 16 containers: the dropped tone

**What the PSX holds.** One effects bank is shared by `BPLD/BPLU 015, 025,
056, 257, 457, 567, 578` and `BRTD/BRTU457`, the 16 groups `se_check.py`
lists. Its program 1 declares one tone. Its cue 4 is a chord of two voices
on program 1, keying tones 0 and 1. On the PSX, tone 0 is silent (sample 0)
and tone 1, past the count, holds the sound: sample 5, at the same centre,
fine tune and range as tone 0 (`dropped.py`). `vag.bank`, like `SsUtKeyOnV`,
keys tone slots by the cue whatever the count, so the PC's chunk plays
sample 5.

**What the PSP keeps.** It keeps program 1 with one tone, the silent one.
Sample 5 is still in `PPVA` and `pBVC`, and no tone references it.

**Whether a rule can tell it apart.** It can (`pastcount.py`, `orphans.py`).
33 cues on PSP-JP reach past their program's count. 202 effects banks have a
sample no tone references (298 samples). On the PSX, the slot past the count
is silent (sample 0) for every cue but the 16 groups' cue 4. The 16 differ
from the other 17 cues in two ways, both read from the PSP's own sections:
- every declared tone of the chorded program is silent, where the other 17
  chord a program that sounds;
- the bank has exactly one sample that no tone references. `AREA003`,
  `AREA008` and `AREA078..081` reach past the count with two such samples;
  the rest with none.

**The rule** (`psp_dropped_tone`). A tone is restored when all of these
hold:
- a cue's chord reaches exactly one slot past a program's count;
- that program's declared tones are all silent;
- exactly one sample of the bank is referenced by no tone.

That sample is then the missing slot's, with the pitch and range of the
program's last declared tone. Any other case returns None, and the bank is
built as the PSP has it. Only what `bank` reads is restored: the sample, the
centre, the fine tune and the range. The tone's mode, volume and pan are not
on the PSP disc. The PC's chunk does not carry them either
([`sound-import.md`](sound-import.md) 1), so the importer's output does not
need them.

**Measured** (`se_check.py`, every `PPHD` effects group of both PSPs through
`_vab` and `bank`, against PSX-JP's chunk):
- before: 885 of 901 equal on each PSP;
- after: **901 of 901** on each;
- 32 outputs changed against the saved baseline, the 16 groups on two discs,
  and no other.

PSX-JP's chunks are the PC's (901 of 901, `sound-import.md` 2), so the 16
now pass the recipe's hash in `importer_source`. `vag.py check` covers the
rule's one case and three refusals on synthetic banks: two orphans, a
declared tone that sounds, and a chord two slots past.

**What it changes.** A PSP-only install's battles with those characters get
that chord's second tone, which the PSP itself may not play. Whether the PSP
plays it is unchecked; it is the owner's ear on a PSP. This makes the
PSP-only install's bank the PC's and the PSX's, not the PSP's: a divergence
from the PSP-only policy (the PSP's audio as it is, `psp-only-build.md` 7
item 2). It is drafted as part of C below.

## 6. The contract for MS

```python
vag.psp_music_vab(pphd: bytes, pbvc: bytes) -> (vh: bytes, vb: bytes)
```

`pphd` and `pbvc` are the `BGM*.EMI`'s type 6 and type 7 sections from a PSP
disc. The result goes where `seq.build` puts a PSX disc's `(vh, vb)`:
`seq.bank_bytes(name, seq.parse_vab(vh, vb))`. It raises `ValueError` on any
layout it does not account for. Final since `ffa7cfce`; the later commits on
`psp/mb` do not change it. `psp_bank(ph, pbvc)` is the shared reader, should
MS want the raw records.

## 7. The importer's paths

`tools/importer.py` is not touched. `vag.py` changed for PSP sections only:
`_vab` still returns a PSX group's sections as they are. `builds.py` ran
`importer.py build` with this branch's `vag.py` and with `668a9b67`'s
(results in section 7.1).

### 7.1 Measured

Sources: the JP disc with the PC's `DAT/` and `BOF3.exe`; the US disc alone;
the European disc alone; each PSP alone. Every file of each cache was
hashed, and the manifests diffed (`builds.py`, `cmp_builds.py`):

| Sources | Files | Containers in `base/` (old, new) | Against `668a9b67`'s `vag.py` |
|---|---:|---|---|
| JP + PC | 2,125 | 741, 741 | identical; the manifest differs in its build time only |
| US alone | 1,872 | 741, 741 | the same |
| European alone | 1,864 | 733, 733 | the same |
| PSP-JP alone | 1,412 → 1,422 | **519, 529** | 16 bank chunks newly from the disc (`psp-jp:wave-from-vag`), the same hashes the recipe holds; 10 more containers complete |
| PSP-EU alone | 1,412 → 1,422 | **519, 529** | the same, `psp-eu` |

The other 6 of the 16 containers (`BPLD015`, `BPLD257`, `BPLD457`,
`BPLD567`, `BPLD578`, `BRTD457`) still lack an arena (`type1` or `pc-edit`),
which is SI's. A PSP-only build now reports no `bank` chunk missing.

Also run: `importer.py check` exits 0; `vag.py check` 0 errors;
`ledger_check.py` 0 errors (101 entries, 2 notes). `recipes/` is not
touched, so `exe-gaps.tsv` cannot have moved.

`tools/importer.py` line 626's description of the `bank` class still says the
16 are missing from a PSP disc. It is printed only when a bank chunk is
missing, which no PSP-only build now does. The new text is in section 9; the
file is not MB's.

## 8. Not verified

- **Nothing heard.** The renders compare our sequencer against itself, on
  the PSX-built and the PSP-built banks. They are not compared with a
  Mednafen render or a PSP. Whether the inverse sounds like the PSX is the
  owner's ear (V4).
- **How the PSP's own driver plays the merged values** was not read. That it
  plays a narrower stereo image is an inference from the 3:1 squeeze, not a
  measurement.
- **Whether the PSP plays the dropped tone's chord** in a battle is
  unchecked.
- The renders are the first 60 s of each song from its start. Loops beyond
  that, and the songs' later sections, are not compared.
- The 38 battle and boss bundles' banks were not built: `base/bgm` uses the
  81 `BGM` banks only, `BGMBAT00` for 165 among them.
- PSP-EU was rendered only through its banks' bytes: its 81 banks are
  byte-identical to PSP-JP's.

Scratch: `<scratchpad>/psp_mb/` (the scripts above, both caches, the render
tables `r_*.json`, the host build `hb/`). The caches and renders are game
data and stay there.

## 9. For the other files

### DIVERGENCE.md: part of draft C (the PSP's music as base; MS owns C)

For MS to fold into C, or to stand alone if the coordinator prefers:

> - **Original behaviour:** the PC port plays MP3 renders. A PSX-disc cache
>   synthesises the disc's sequences on the PSX's banks. The PSP's banks
>   merge each tone's volume with its program's and the VAB master's
>   (`(tv * pm * bm * 32) // 127**2`). Their pan is `(tp + mp + 64) // 3`, a
>   third of the PSX's spread (`psp-music-banks.md` 3).
> - **New behaviour:** a cache built from a PSP disc alone takes its music
>   banks from it with that merge inverted. Master and program volumes are
>   127, and each tone's volume is the merged one rounded. Program pans are
>   64, and each tone's pan is `3p - 128`. Samples, pitch, range, ADSR and
>   mode are the PSP's, which are the PSX's.
> - **Rationale:** the owner's call that a PSP-only install uses the PSP's
>   audio (`psp-only-build.md` 7 item 2). Of the merge, the inverse undoes
>   what the conversion lost, where playing the squeezed pan as it is would
>   narrow the image. Rendered against the PSX-built banks: 166 songs, SNR
>   median 41.9 dB; one song at 16.8 dB (22, whose bank has its own program
>   pans); level within half a decibel. As is: a median 17.5 dB and an image
>   up to 7.4 dB narrower.
> - **Also in the PSX version?** The PSX plays the unmerged fields; the
>   inverse is the nearest the PSP's data allows.
> - **Verification:** `vag.py check`; the field comparison and the renders
>   of `psp-music-banks.md` 3 and 4. Not heard.
> - **Reversible?** A PSX disc among the sources builds the PSX's banks.

### DIVERGENCE.md: the dropped tone (one more bullet in C, or a D-like entry)

> - **Original behaviour:** the PSP's effects bank for 16 battle containers
>   (`BPLD/BPLU 015, 025, 056, 257, 457, 567, 578`, `BRTD/BRTU457`) declares
>   program 1 with one silent tone. The PSX and the PC chord a second tone,
>   past the count, that plays sample 5.
> - **New behaviour:** a PSP-only build restores that tone by the rule of
>   `psp-music-banks.md` 5, from the PSP's own data. The 16 chunks are the
>   PC's, byte for byte.
> - **Rationale:** the PSP's converter kept a program's declared tones only;
>   the chord was meant to sound. Tier: Intent.
> - **Also in the PSX version?** The PSX plays it. Whether the PSP plays it
>   is unchecked.
> - **Reversible?** No switch; the owner's call (`psp-only-round.md` 7 item 4) decides
>   whether it lands.

### psp-only-build.md (the coordinator's)

- 2.1's row "The other 16": derived by rule from the PSP's own data
  (`psp-music-banks.md` 5); 901 of 901.
- 2.1's row "The 81 music banks": read, with the inverse in
  `psp-music-banks.md`; `vag.psp_music_vab`.
- 8: "the 16 banks" leaves the to-do list.

### tools/importer.py (whoever owns it at merge)

Line 626, the `bank` class's description, could read: "audio banks, the
disc's VAG samples as WAV: step 6 (wave-from-vag); from a PSP disc all 901
(the 16 containers' dropped tone by psp-music-banks.md 5's rule)".

### README.md

> | [`psp-music-banks.md`](psp-music-banks.md) | The PSP's music banks and the dropped tone (PSP-only round, group MB, 2026-10-12): the `PPTN` record read against 2,939 PSX tones; the volume merge `(tv * pm * bm * 32) // 127**2` and pan `(tp + mp + 64) // 3`, exact; `vag.psp_music_vab` with the inverse (master and program 127, pan `3p - 128`), rendered against the PSX-built banks over 166 songs (SNR median 41.9 dB; song 22 at 16.8 dB); the lost fields and what our sequencer does with each; the 16 battle containers' dropped tone restored by a rule from the PSP's own data (901 of 901 banks). MEASURED. |

### STATUS.md / HANDOFF.md

- A PSP-only build now writes all 901 sound banks. `base/bgm/bank/` from a
  PSP disc waits on MS stage B.
- Owed to the owner: the inverse's sound (V4: the 16 note-changed songs, and
  song 22 for the program-pan case); the dropped tone's chord in a fight
  with one of the 16 containers' characters.

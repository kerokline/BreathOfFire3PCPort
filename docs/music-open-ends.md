# The music measurement's open ends (2026-10-10)

**Status:** STABLE (measured 2026-10-10 on the owner's machine, branch
`catchup/music-tooling`: the 156 Mednafen renders in `analysis/bgm/renders`,
the cache `analysis/cache/pc-plus-us` built from the US disc, the PC's
`bof3/BGM`; nothing played in the game)

[`HANDOFF.md`](HANDOFF.md) 'Pick up here' item 3 left four music items open:
the five songs at 0.943..0.984 against their renders, the SPU reading R1, the
23 tracks the loop table refuses because their waveform never repeats, and
the 20 near-full files. Each is below with what was measured, what was built
and what is left. Measuring the third turned up a fourth thing the table had
wrong - four of its shipped rows looped at a period the sequence does not have
(section 3) - which is fixed under DIV-0081's own rule.

| # | Item | Measured | Built | Left |
|---|---|---|---|---|
| 1 | The five songs below 0.99 | the oracle was short-windowed (43, 77, 145, 146); 94 is voice-allocation ties decided by the interrupt latency | `synth_check.py --oracle-window`, `synth_render --envx`; 155 of 156 at >= 0.99 | 94 at 0.986, explained, not reproducible without the game's frame |
| 2 | R1 (+32 in the ADPCM filter) | three decoders against the renders on nine songs: no `+32` wins on every one | the model floors with no `+32`, as `vag.decode` | the renders' DC is still lower than ours by 0.5..12 LSB (another floor elsewhere) |
| 3 | The loop periods | 28 of 156 rows off the sequence's period, 4 of the 37 shipped | the period pinned and gated (`seq_periods.py`, `measure_loops.py`); the table's second cut, 39 rows | none |
| 4 | The 23 refused for a waveform that never repeats | 13 of the 23 had a wrong period; 4 now ship as ordinary rows; a long crossfade on the rest helps 2 | `long_fade.py` (measurement only) | building a long-fade row needs a rule DIV-0081 does not have: not built |
| 5 | The near-full files (23 since the second cut) | the original's rewind is within -41..+7 ms of the disc's period on all 23; a slip loop is further off | `long_fade.py`'s near-full mode | the owner's call stands, with numbers |
| 6 | The host suite on Windows (I35) | `spu_tests` 431,706 checks, `seq_tests` 28, 0 failures | `abort_check.h`; the suite in CI | - |

The owner's open music calls ([`owner-review.md`](owner-review.md)) are not
touched here: the town theme `000` (shorter than one loop), DIV-0087's level
(127 against the PSX title's 97), song 21, `165`'s bank.

## 0. The tools and the host build

The host suite (`tools/bgm/host`) builds on the owner's Windows machine with
the `cmake-clang-v1` toolchain's `clang++` (IDEAS I35, section 6):

    cmake -S tools/bgm/host -B <scratch>/hb -G Ninja -DCMAKE_CXX_COMPILER=clang++
    cmake --build <scratch>/hb

`synth_check.py` takes the owner's paths as arguments (`--renders
analysis/bgm/renders --cache analysis/cache/pc-plus-us --synth
<scratch>/hb/synth_render.exe --work <scratch>/check`). Ten of the 166 songs
have no render on this machine: the nine once-only songs and 21 (`4 9 21 28 42
58 96 105 141 150`); the 2026-10-08 table's 165 counted renders made in a cloud
session. Every number below is from the 156 that are here.

## 1. The five songs below 0.99, oracle-timed

The 2026-10-08 log ([`libsnd-reading.md`](libsnd-reading.md) 9.6) had 43
(0.960), 77 (0.979), 94 (0.984), 145 (0.972), 146 (0.943) and two suspects,
the voice allocation on a re-key during release and the release rate's
rounding (`spu-model.md` R5, R8). Re-measured first with the tools as they
were (2,048-sample oracle window): 43 0.954, 77 1.000, 94 0.986, 145 0.965,
146 0.922 - the US-disc cache here, the same banks byte for byte.

**146 at 17.9 s, per voice.** In the failing span (18.2..20.2 s from the first
note) a least-squares fit of the render to our five voices' stems, 0.1 s
windows, gives every gain 0.94..1.06 and leaves the residual where it was
(-46.4 dB with the gains fixed or free at 18.4 s): not a level, so not a
release rate.
Shifting one stem by a fraction of a sample (an FFT delay, -2..+2 in 1/16):
**voice 2 one sample earlier takes the 17.5..18.6 s residual from -50.7 to
-82.9 dB**, voice 0 one sample earlier at 19.2..20.5 s from -46.8 to -59.9. So
the misses are whole-sample key-on offsets, the interrupt latency of
`libsnd-reading.md` 9.4, that the oracle failed to remove: it fits each
key-on VSync's delay over the 2,048 samples after the key on, and these pads'
attacks are still silent there (voice 2's stem at -77.7 dB in its first
0.1 s). Neither suspect.

**The fix is the oracle's**: `synth_check.py --oracle-window S` fits each
key-on VSync over the stretch to the next key on of its voices, at least
2,048 samples, at most S seconds, clipped at the render's end (a first cut
skipped key ons near the end, which left 43 at 0.985). The default stays
2,048 samples, the 2026-10-08 table's.

| Song | 2,048 samples | 0.25 s | 0.5 s | **1.0 s** | 2.0 s |
|---|---|---|---|---|---|
| 43 | 0.954 | 0.992 | - | **1.000** | - |
| 77 | 1.000 | - | - | **1.000** | - |
| 94 | 0.986 | 0.986 | 0.979 | **0.986** | 0.979 |
| 145 | 0.965 | 1.000 | 0.999 | **1.000** | 0.999 |
| 146 | 0.922 | 0.964 | 0.964 | **1.000** | 1.000 |
| 0, 153 (controls) | 1.000, 1.000 | | | 1.000, 1.000 | |

    python tools/bgm/synth_check.py --songs 43,77,94,145,146 --renders analysis/bgm/renders \
        --cache analysis/cache/pc-plus-us --synth <scratch>/hb/synth_render.exe --work <scratch>/check \
        --oracle --oracle-window 1.0

(0.25, 0.5 and 2.0 s were run on 43, 94, 145, 146 only; a dash is not run.)

**94: which voice is stolen.** 94 fills all 16 voices and steals. Its failing
windows at 3.75 and 11.75 s from the first note are a burst of residual (-27
..-29 dB in 5 ms windows, decaying over 80 ms) at the flush of VSync 244, where our voice 10
- releasing at -25 dB - is cut by a new key on. `synth_render --envx` prints
each flush's ENVX, the allocator's view (`_SsVmAlloc`, `libsnd-reading.md`
3.3: the lowest ENVX is stolen): at VSync 243 voice 10 reads `456C` and voice
12 `4572`, six apart, voice 10 falling `0xB88` a VSync and voice 12 `0x1CC`. They
cross within two samples of the read. A scan of the trace for steals decided
by fewer than 64 units where the two slopes differ finds four of that kind
(VSyncs 243, 723, 2643, 3123, margin 6) and five more at margin 2 in the
first 70 s. Moving the
read VSyncs 243, 723, 2643 and 3123 two samples earlier (`--offsets`, -512)
- inside the latency's spread - flips the choice to voice 12 and takes **94
to 1.000**. What the PSX steals at a tie that close is decided by when its
VBlank handler runs, which a player cannot know (9.4); the model's rule is the
read one. The scan is a scratch script over `synth_render --ticks FILE --envx`.

**The whole table with the 1 s window** (R1 as section 2 leaves it):

    python tools/bgm/synth_check.py --songs all ... --oracle --oracle-window 1.0 --json <scratch>/all.json

| | 2026-10-08 (165 renders, 2,048 window) | 2026-10-10 (156 here, 1 s window) |
|---|---|---|
| oracle-timed, >= 0.99 of windows | 160 | **155** (94 at 0.986) |
| plain grid, >= 0.99 | 24 | 22; quartiles 0.76 / 0.90 / 0.96 |
| loop period, ours less the render's | 112 to the sample, 44 by one | 112 to the sample, 44 by one |
| level, ours above the render | 2.24..2.51 dB | 2.24..2.50 dB |

The same run with the `+32` model gives the same window fractions and the same
155; the windows do not see section 2's difference.

## 2. R1: the ADPCM prediction has no +32

The model read the XA formula's `(old*f0 + older*f1 + 32) / 64` as `(... +
32) >> 6`; `tools/vag.py`'s `decode` has no `+32`. A half-LSB bias each sample
goes through the filter, whose DC gain is up to 32 (filter 4: `1 / (1 - 122/64
+ 60/64)`), so it is up to 16 LSB in the decoded sample - a level and a low-
frequency difference, which a correlation does not see and a residual does.
88 % of the music banks' blocks are filtered (filters 1..4: 993,961 of
1,133,815).

Three `synth_render` builds side by side (scratch copies of `src/audio`):
`+32` and `>> 6` (as built), no `+32` (`vag.decode`), `+32` and C's
truncating `/ 64`. Each song rendered by all three with the same oracle
offsets, aligned to its render, one gain fitted on the as-built render, and
the residual `render x gain - ours` taken over the windows above -50 dBFS, in
LSB (scratch `r1cmp.py`, 60..120 s a song):

| Song | 20..300 Hz: +32 / no +32 / +32 and /64 | above 20 Hz: same order | mean: render / +32 / no +32 |
|---|---|---|---|
| 0 | 3.86 / **2.22** / 7.56 | 5.73 / **4.29** / 9.64 | -27.5 / -8.5 / -19.0 |
| 3 | 2.42 / **1.63** / 5.14 | 12.79 / **12.56** / 14.04 | -13.8 / -1.6 / -9.6 |
| 11 | 2.62 / **1.55** / 5.24 | 16.74 / **16.56** / 17.45 | -12.5 / -5.3 / -9.3 |
| 43 | 1.99 / **1.02** / 3.08 | 2.84 / **1.85** / 4.08 | -8.7 / -7.3 / -8.2 |
| 89 | 2.83 / **1.53** / 4.49 | 3.52 / **2.10** / 5.71 | -10.3 / -3.7 / -7.2 |
| 94 | 11.06 / **10.61** / 12.82 | 20.59 / **20.31** / 21.71 | -36.8 / -9.7 / -25.2 |
| 145 | 3.87 / **2.57** / 6.59 | 8.45 / **7.70** / 11.03 | -22.4 / -0.6 / -12.8 |
| 146 | 2.75 / **1.51** / 4.08 | 3.11 / **1.89** / 4.54 | -5.1 / -2.0 / -4.4 |
| 153 | 4.85 / **3.39** / 8.07 | 16.29 / **15.72** / 18.03 | -4.3 / +9.5 / +1.2 |

(Left channel means; the right channel orders the three the same way.) **No `+32` is best on
every song in both bands, and moves the DC towards the render's on every
song**; the division is worst. So the model now floors `(old*f0 + older*f1)
>> 6`, the reading `vag.decode` always took, and the two agree; the unit
tests' by-hand values are recomputed (`spu_tests` 431,706 checks, 0 failures).
`sound-import.md` 2.2's "against the SPU's decode" numbers (92 % of samples
differ from the port's converter, RMS 17 LSB) were taken with `vag.decode`, so
they are now the model's too.

**R2** (shift 13..15 as 9; `vag.decode` takes the raw shift): no block of the
game uses those shifts - 0 of the 81 music banks' 1,133,815 blocks, 0 of the
801 played effect samples' 945,003 - so the two readings give the same samples
on all of its data and the renders cannot settle it.

**Left**: the render's DC is still below ours by 0.5..11.6 LSB (94: -36.8
against -25.2). Some other floor or rounding in the chain - the Gaussian's products
(`SAR 15` each), the envelope and volume multiplies, the reverb - or
Mednafen's recording. Unread; it is a constant offset, not heard.

## 3. The loop periods against the sequence (DIV-0081's second cut)

The 23 refusals of item 4 were said to sit "at the sequence's nominal body".
Checking that against our sequencer, whose loop periods equal the renders' to
the VSync on all 156 songs (section 1's table), showed that the loop table's
measurement had picked wrong periods - for refused rows and shipped ones.

`tools/bgm/seq_periods.py` plays each looping song through `synth_render
--trace` to three loop jumps; the passes' lengths are the jumps' differences
in VSyncs (libsnd's integer tick makes them alternate by one on many songs:
song 23's are 4,212 and 4,213). A row agrees when its period is within one
VSync and four samples of a pass length.

    python tools/bgm/seq_periods.py --synth <scratch>/hb/synth_render.exe --cache analysis/cache/pc-plus-us   (5 min 45 s)

**128 of 156 rows agree; 28 do not, 4 of them shipped:**

| Track | Case | Row's period | The sequence's | Off by |
|---|---|---|---|---|
| **064**, **076** | full, shipped | 1,326,847 (30.09 s) | 1,920 VSyncs, 1,415,303 (32.09 s) | -120 VSyncs: 15 of 16 bars; the 16th never played after the first pass |
| **131** | full, shipped | 2,830,606 (64.19 s) | 4,014 / 4,015 VSyncs (67.10 s) | -174 VSyncs |
| **070** | shifted, shipped | 4,489,740 | 6,092 / 6,093 VSyncs | -1.22 VSyncs (20 ms) |
| 037 | full, refused | 849,182 | 768 VSyncs (566,121) | +384: 1.5 bodies |
| 020, 084, 101 | full, refused | 530,637 | 776 / 777 VSyncs | -56 |
| 023, 052 | full, refused | 2,887,366 | 4,212 / 4,213 VSyncs | -295 |
| 012 035 049 083 095 121 130 137 161 | full, refused | | | -30..+120 VSyncs (130: +1.18) |
| 033 067 077 080 108 110 116 160 | shortened | | | -2.4..+1,397 VSyncs |
| 102 | shifted, refused | | | +213 |

The shipped rows' files say the same: `064`'s MP3 repeats itself at the
sequence's period at a median 0.992 of 0.25 s windows (0.887 at >= 0.97)
against 0.523 (0.138) at the shipped one, from the shipped start; `076` 0.983
against 0.601 (scratch `mp3rep.py`). `prove_loops.py`'s seam score did not
see it - the shipped `064` scored 0.78 against the render's continuation,
because the 2 s after a jump from bar 16 to bar 1 sound like bar 16 going on -
so the period is now checked against the sequence, not scored.

**What changed** (`tools/bgm/measure_loops.py`, the docstring's new
paragraph):

- with `BGM_SEQ_PERIODS` (`analysis/bgm/seq_periods.json`, written by
  `seq_periods.py`), a song's period candidates are the sequence's pass
  lengths alone; the waveform windows' vote decides between the two lengths
  (their match fractions are too close: 078 0.134 against 0.124) and refines
  by +-8 samples, not a hop; inside the MP3 the refinement searches one VSync
  wider, since the file's own repeat may sit on the other length;
- `gate()` refuses a full or shifted row whose period is more than one VSync
  and four samples from every pass length;
- without the file, the measurement and the gates are as before.

Measured again from the renders (`BGM_SEQ_PERIODS=... measure_loops.py run
--workers 3 --redo`, then `regate`; minutes, no render made), on a copy and
then installed as `analysis/bgm/loops.json` (the first cut kept as
`analysis/bgm/loops_2026-10-10_pre-seqpin.json`, the pass lengths as
`analysis/bgm/seq_periods.json`, the run's lines appended to
`analysis/bgm/measure.log`):

| | First cut | Second cut |
|---|---|---|
| rows shipped | 37 (28 full, 9 shifted) | **39 (31 full, 8 shifted)** |
| unchanged byte for byte | | 31 |
| corrected | | `064`, `076`: full at 32.099 s |
| added | | `037`, `084`, `109`, `130` (full) |
| moved | | `164`: one VSync longer (1,182 VSyncs), start 4.8 s earlier |
| refused | | `131` (now *shortened*: no whole body in the file at the true period), `070` (loop correlation 0.638) |
| shortened (`near_full`) | 76 (20) | 81 (23) |

`prove_loops.py`'s seam numbers for the seven new or moved rows (the render's
continuation; in brackets the MP3's own continuation past the end, the
yardstick of how alike file and render are there; scratch `prove_rows.py`):
`037` 0.58 (0.81), `064` 0.78 (0.81), `076` 0.73 (0.75), `084` 0.69 (0.72),
`109` 0.81 (0.85), `130` 0.76 (0.77), `164` 0.65 (0.70); steps at or under the
file's own (`037` 2.25 against 2.38); gaps 0..31 samples; the original's rewind
-0.09..0.41 with gaps of 415..867 samples where measured. `037`'s 0.58 is
the lowest of the shipped full rows (the first cut's were 0.60..0.90).

Rebuilt with the new `music_loops_table.inc`; `BOF3X_LANG=original
BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=sound`: exit 0, "39 table rows in bounds".
DIV-0081 carries the second cut. **Not heard.**

## 4. The 23 refused for a waveform that never repeats

The first cut's 23 (`002 006 016 020 023 026 035 037 041 046 052 083 084 086
095 101 109 121 130 137 149 161 165`), refused on the loop correlation gate
(< 0.8). **13 of them had been measured at a wrong period** (section 3: twelve by
4.8..384 VSyncs, `130` by 1.18), so
their correlation was measured at the wrong lag. After the second cut:

| Outcome | Tracks |
|---|---|
| ship as ordinary full rows | `037 084 109 130` |
| *shortened* (no whole body in the file at the true period) | `023 035 052 101` |
| still refused on correlation, full | `002 006 016 020 041 083 086 095 121 137 161` |
| still refused on correlation, shifted | `026 046 149 165` |

**Why their waveforms never repeat**: our own sequencer does not repeat them
either. Two passes of the synth's output at the sequence's period, best NCC
over 10 s within +-4 samples (scratch `seqper.py`): `083` -0.02, `095` 0.00,
`161` -0.02, `026` 0.28, `086` 0.35, `149` 0.36, `137` 0.37 - against 0.99..1.00
for a song that loops cleanly (`064`, `076`) - and pass 2 against pass 3 no
better (`083` -0.01). The voices held across the loop point and the samples'
own loops run free, so each pass starts from another state. No sample-exact
loop exists in the MP3 because none exists in the music.

**A loop at the sequence's period with a longer crossfade**,
`tools/bgm/long_fade.py`: each still-refused full row's splice (the file's end
of a pass onto its start, from the second cut's row) played offline with
crossfades of 128, 1,024, 4,096, 11,025 and 22,050 samples, linear (the
engine's weights) and equal-power, scored over the 3 s after the seam against
the render: `env`, the 10 ms log-RMS correlation, and `dev`, the largest
50 ms level difference over the fade and 0.5 s after it, less the median
offset. The yardstick is the file playing on with no seam.

    BGM_LOOPS_JSON=<the second cut> python tools/bgm/long_fade.py --tracks 2,6,12,16,20,41,49,70,83,86,95,121,137,161

| Track | 128, linear: env / dev | best long fade | no seam (yardstick) | Reading |
|---|---|---|---|---|
| `006` | 0.51 / 26.0 dB | 22,050 equal-power: **0.77 / 2.2 dB** | 0.83 / 1.3 | the long fade does it |
| `020` | 0.66 / 10.0 dB | 22,050 equal-power: **0.73 / 1.5 dB** | 0.99 / 0.4 | the level step goes; the shape stays a little unlike |
| `041` | 0.85 / 2.4 dB | 22,050 linear 0.87 / 2.0 | 0.97 / 0.8 | the short fade already serves (refused at 0.763) |
| `086` | 0.86 / 0.6 dB | no better | 0.95 / 0.7 | the short fade already serves (refused at 0.789) |
| `016` | 0.91 / 19.0 dB | 4,096 linear 0.97 / 6.8 dB | - (0.11 s of file after the end) | no room for a long fade |
| `002 083 095 121 137 161` | -0.04..0.27 | at most 0.38 | 0.10..0.89 | unlike the disc at any fade |
| `012 049 070` | | at most 0.01 | | unlike at any fade (`012`'s file and render unlike throughout) |

Equal-power fades raise the level of material that does repeat (track `003`,
a clean loop: `dev` 2.1..3.1 dB at 4,096..22,050 samples against 0.7..0.8
linear), so the weights would be per row.

**What building it would take** (not built: DIV-0081's rule is a loop at the
disc's point, sample-accurate, with a 2.9 or 5.8 ms crossfade inside the
frames holding the seam, and its gates are the waveform's - a long-fade row is
a third kind the entry and the 2026-10-10 review's gating do not cover):

1. the engine: `LoopDecode` crossfades only inside the frame holding the end
   (`Fatal` otherwise); a fade of up to 22,050 samples needs the file's tail
   after `end` decoded and kept (88 KB at 16-bit stereo) before the rewind,
   then mixed into the first `fade` samples from `start`, and the row's
   weights (linear or equal-power);
2. a row kind and a gate of its own: the waveform gates cannot pass these
   rows by construction, so an envelope-and-level gate (on this measurement,
   `env` within 0.1 of the yardstick and `dev` under 3 dB - `006` and,
   narrowly, `020`) with the file holding `end + fade`;
3. `041` and `086` need no long fade - a gate that judged them by the seam
   rather than by the confidence would take them as ordinary rows;
4. a DIV-0081 amendment and the owner's ear: two tracks at most.

## 5. The near-full files

A `near_full` row is a file that holds its intro and one body, short of the
whole body by two frames or less (`short_by`); DIV-0081 left whether "a slip
that size a pass beats the rewind" to the owner, on the count. The second cut
has 23: `017 022 023 024 029 032 033 034 035 038 052 054 071 072 073 080 091
097 106 115 118 132 160` (`023`, `035` and `052` joined it when their periods
were corrected; their short_by is 974..1,413 samples).

**The original's rewind is already almost the disc's period.** These files
open with a lead-in of near-silence up to the first note (the loop start) and
end `short_by` before the body's end; rewinding replays the lead-in, which
puts back most of what the file lacks. The rewind's period less the disc's is
the file's length less the body: **-1,812..+324 samples, -41..+7 ms**, a pass
(`017` +1.1 ms, `071` +3.7, `097` +7.3; `029` -41.1, `080` -32.8, `032` -23.6;
median -13 ms). The cost is the lead-in's near-silence at each join.

**A slip loop** - the file to its last whole frame, then on from the loop
start (`long_fade.py`'s near-full mode, 128 and 1,024-sample fades) - removes
the lead-in and so slips the whole `short_by` (15..58 ms) a pass: its first
notes land early against the disc, which the 50 ms level comparison sees
directly (`017`: the splice's first note at 0.00 s, the render's at 0.05 s).
Against the render's continuation, env / dev:

| | Rewind better | Slip better |
|---|---|---|
| by `env` | 16: `017` (0.98 against 0.63) `022 023 029 032 033 034 035 038 052 072 073 097 115` (equal) `132 160` | 7: `024` (0.66 / 0.65), `054` (0.80 / 0.79), `071`, `080` (0.49 / 0.19), `091`, `106` (0.29 / 0.15), `118` |
| by `dev` | 21 | 2: `080` (6.8 against 19.3 dB), `029` (11.4 / 13.9) |

    BGM_LOOPS_JSON=<the second cut> python tools/bgm/long_fade.py --tracks 17,22,23,...,160 --fades 128,256,1024

No loop inside these files can be closer to the disc's period than the rewind:
a loop must end before the file's last frame, so its period is at most the
file less a frame, and the start cannot go before the file's first sample.
**The reading for the owner's call**: the slip would trade a gap of 6..38 ms
of lead-in for a jump of 15..58 ms of music each pass, and is worse by both
scores on most of the 23; the numbers argue for leaving them rewinding. Not
decided here.

## 6. The host suite on Windows (IDEAS I35)

`tools/bgm/host/abort_check.h`: the two test programs check the model's aborts
in a child - `fork` on POSIX as before; on Windows the binary runs itself again
with `--abort-case N` (N counts the `Aborts` calls), skips to that case with
its output discarded, and the parent reads std::abort's exit status 3. The
runtime's abort message box and error report are turned off in the child.
Results on the owner's machine (`clang++` 22.1.8, x86_64-w64-windows-gnu):
`spu_tests` 431,706 checks, `seq_tests` 28, 0 failures, 4.4 s and 1.4 s. CI
builds and runs both on Linux (`.github/workflows/checks.yml`, job `Music host
suite`).

## 7. Files

- Tools: `tools/bgm/synth_check.py` (`--oracle-window`),
  `tools/bgm/host/synth_render.cpp` (`--envx`), `tools/bgm/host/abort_check.h`,
  `tools/bgm/seq_periods.py`, `tools/bgm/measure_loops.py` (the pin and the
  gate), `tools/bgm/bgm_paths.py` (`BGM_SEQ_PERIODS`), `tools/bgm/long_fade.py`.
- Model: `src/audio/spu.cpp` (R1), `src/audio/seq.{h,cpp}` (the trace's ENVX).
- Engine data: `src/game/music_loops_table.inc` (39 rows, generated).
- Ledger: DIV-0081 (the second cut), DIV-0087 (R1 noted).
- Scratch only (session `ba6f0f72`), not committed: `win.py`, `stems.py`,
  `fshift.py`, `ties.py`, `r1cmp.py`, `seqper.py`, `percheck.py`, `mp3rep.py`,
  `prove_rows.py`, the three R1 builds; the renders and every decode are game
  data and stay in `analysis/` and scratch.

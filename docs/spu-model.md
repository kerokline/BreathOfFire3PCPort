# The SPU model: what it implements, from where, and the readings it takes

**Status:** BUILT, UNIT-TESTED (2026-10-08, a cloud session on
`audio/sequence-from-disc`) - the "SPU" group of
[`sequenced-music-plan.md`](sequenced-music-plan.md) section 9. Not yet
compared against a render: that is the HOST group's work, and the readings
in section 3 are where a mismatch is most likely to be found.

`src/audio/spu.h`, `src/audio/spu.cpp`: `psx::Spu`, 24 voices, 512 KiB of SPU
RAM, the register set, the reverb unit and the mix, one sample at a time at
44,100 Hz. The source is nocash's psx-spx, SPU chapter
(<https://psx-spx.consoledev.net/ps1/spu/soundprocessingunitspu/>, read
2026-10-08), plus the XA-ADPCM decode formula of its CDROM Format chapter,
which the SPU chapter points to for the block header. No emulator code was
read or used. The code names the spec section above each block ("spec:
...") and the readings below as `R1`..`R17`.

Tests: `tools/bgm/host/` (`README.md` there), 431,704 checks, all passing
on 2026-10-08 with g++ 13.3. The model compiles without warnings under
`g++ -std=c++17 -Wall -Wextra -Wpedantic -Wshadow -Wconversion` and
`clang++ -Wall -Wextra`. Speed: 60 s of audio with all 24 voices playing and
the Hall reverb renders in 1.3 s on the session's machine (46 times real
time), so a `0x12000`-byte half (18,432 frames) costs about 9 ms.

## 1. What is implemented, and where each piece comes from

| Piece | Spec section | Notes |
|---|---|---|
| SPU RAM, 512 KiB, little-endian halfwords; addresses in 8-byte units in SSA / LSAX / ESA as the registers hold them | SPU Overview, "SPU Memory layout"; SPU ADPCM Samples, SSA, LSAX | `WriteRam` / `ReadRam` stand in for DMA |
| ADPCM decode: 16-byte blocks, shift and filter in byte 0, five filter pairs (0,0) (60,0) (115,-52) (98,-55) (122,-60), `+32 >> 6`, 16-bit clamp | SPU ADPCM Samples, "Sample Data"; CDROM Format, "decode_28_nibbles", "Pos/neg Tables" | R1, R2 |
| Block flags: bit 2 loop start copies the address to LSAX; bit 0 end sets ENDX and jumps to LSAX after the block; bit 0 without bit 1 forces release with the envelope at 0 | SPU ADPCM Samples, LSAX, "Flag Bits" | R4. The decoder's two-sample history runs on across the jump (tested) |
| Pitch counter: 16-bit pitch, `> 3FFFh` becomes `4000h`, bits 12 and up the sample within the block, bits 4..11 the interpolation index | SPU ADPCM Pitch, "Pitch Counter" | R11 |
| Pitch modulation (PMON), including the sign-expansion glitch for pitch `> 7FFFh` and the `AND FFFFh` | SPU ADPCM Pitch, "Pitch Counter" | R14 |
| 4-point Gaussian interpolation with the 512-entry table, each product `SAR 15` separately | SPU ADPCM Pitch, "4-Point Gaussian Interpolation" | Table transcribed; the test checks the spec's own property that every quadruple sums to `7F7Fh..7F81h`, which a typo in any entry breaks |
| ADSR: ADSR1 / ADSR2 fields, the envelope operation (step `7 - StepValue`, NOT for decrease, `SHL max(0, 11 - shift)`, counter increment `8000h SHR max(0, shift - 11)`, the "fake exponential" increase above `6000h` with its three shift cases, exponential decrease `step * level / 8000h`, the `max(increment, 1)` unless all bits set, the saturation rules), sustain level `(N+1) * 800h`, key off to release | SPU Volume and ADSR Generator, ADSR1, ADSR2, "Envelope Operation depending on Shift/Step/Mode/Direction" | R5..R8 |
| Voice volume and main volume, fixed mode (bits 0..14 = volume / 2) and sweep mode through the same envelope operation, phase bit included | SPU Volume and ADSR Generator, VOLL / VOLR / MVOLL / MVOLR, "Sweep Volume Mode" | R10. Read-back: VOLXL / VOLXR / MVOLXL / MVOLXR |
| `OUTX = sample * ENVX >> 15`, then `* VOLX >> 15` per side | same, "VOLXL / VOLXR" ("The final volume applied to each voice") | R14 |
| Key on / key off / ENDX | SPU Voice Flags | R3, R9 |
| Noise: one generator, the timer with step `4..7` and reload `20000h SHR shift` (twice if needed), the parity `bit15 ^ bit12 ^ bit11 ^ bit10 ^ 1` | SPU Noise Generator | R13. The noise voice still reads its ADPCM and obeys its flags |
| Reverb: all 32 registers, EVOL, ESA (a write sets the buffer address), EON; the formula (same-side and different-side reflection, four combs, two all-pass stages), half rate with left and right on alternate samples, the 39-tap resampler in and out, the work area wrap, `BufferAddress = MAX(ESA, (BufferAddress+2) AND 7FFFEh)`, ATTR bit 7 stopping the writes but not the reads | SPU Reverb Registers; SPU Reverb Formula (all subsections); SPU Internal State Machine, "Reverb Computation Order" | R12, R15 |
| The reverb presets: Room, Studio Small / Medium / Large, Hall, Half Echo, Space Echo, Chaos Echo, Delay, Off - register values and work-area sizes | SPU Reverb Examples | `kSpuReverbPresets`, `ApplyReverbPreset`. See section 4 on how they map to libspu's mode numbers |
| Capture buffers: CD left / right at `0..7FFh` (silence - no CD input), voices 1 and 3 after the envelope at `800h..FFFh`, one halfword a sample | SPU Overview, "SPU Memory layout" | R17 |
| The mix: the voices, the reverb return, the main volume, 16-bit clamps; ATTR bit 14 mute | SPU Control and Status Register, ATTR | R16 |
| `WriteRegister(offset, value)`: the register file by I/O offset from `1F801C00h`, for replaying a register trace | SPU I/O Port Summary and the sections per register | Unmodelled registers abort |

## 2. What is left out, and what it says

Each aborts through `psx::SetSpuAbortHook` (default: print to stderr) and
then `std::abort()`. The unit tests check six of them.

| Left out | Abort message (prefix `psx::Spu: `) |
|---|---|
| ADPCM filters 5..7 (the spec describes five) | `ADPCM block header 0x%02X: filter %d (only 0..4 are described)` |
| CD (I2SA) and external (I2SB) inputs, and their reverb, ATTR bits 0..3 | `ATTR 0x%04X: the CD (I2SA) and external (I2SB) inputs and their reverb (bits 0..3) are not modelled` |
| The SPU interrupt (IRQA, ATTR bit 6) | `ATTR 0x%04X: the SPU interrupt (IRQ9, bit 6) is not modelled` |
| The SPU switched off (ATTR bit 15 clear) during `Render` | `Render with the SPU disabled (ATTR bit 15 clear) is not modelled` |
| `vIIR = -8000h`, whose negation the spec describes only in part ("Bug") | `reverb vIIR = -8000h: the negation the spec describes under "Bug" is not modelled` |
| Writes to ENVX (the spec: the generator "does overwrite the setting ... whenever applying a new Step") | `WriteRegister: a write to ENVX (voice %d) is not modelled` |
| RAM_CTRL other than `0004h` | `WriteRegister: RAM_CTRL 0x%04X, only 0x0004 (one 512 KiB bank) is modelled` |
| Every other register through `WriteRegister` (TSA, the data FIFO, IRQA, AVOL / BVOL, STATX ...) | `WriteRegister: offset 0x%03X (0x1F801%03X) is not modelled` |
| Out-of-range voice / reverb register / RAM access | `voice %d out of range 0..23`, `reverb register %d out of range 0..31`, `WriteRam 0x%X+0x%X past the 512 KiB of SPU RAM` |

ATTR bits 4..5 (the transfer mode) are accepted and have no effect:
`WriteRam` is the transfer, and the mode has no audible consequence.

Not modelled and not aborting, because they are timing or state the
description does not pin down rather than features: the key-on latency (the
spec's 2-sample grid, ENVX non-zero 6 or 7 samples after the write - R9),
the "Reverb Precision" residual (the spec's own best emulation is 24 LSB
off on a wet-dominated channel), and the 1F801E60h scratch registers.

## 3. The readings taken where the spec leaves a choice

These are the places a mismatch against a render is to be looked for
first.

- **R1. The ADPCM prediction rounds by `+32` and floors.** The XA formula
  reads `(old*f0 + older*f1+32)/64`; the model does `(... + 32) >> 6`, an
  arithmetic shift, so `-60028 >> 6 = -938` where C's division would give
  `-937` (tested). **This disagrees with `tools/vag.py`'s `decode`**, which
  has no `+32`: `((s1 * k0 + s2 * k1) >> 6)`. The plan's check "the decoder
  against `vag.decode`'s integer form" will fail by one LSB on most samples
  of filtered blocks until one of the two is settled; a render settles it
  (a filter-1..4 sample on a voice at pitch `1000h`, envelope and volumes at
  full, is a direct read of the decoder).
- **R2. Shift values 13..15 act as 9**, from the XA header description ("13..15
  = Reserved/Same as 9"), which the SPU chapter refers to ("reportedly same
  as for CD-XA"). `vag.py` shifts by the raw value (giving 0 or -1). Real
  sample data rarely uses them.
- **R3. Key on resets** the ADPCM decoder's two-sample history, the three
  previous samples the interpolation reads, and the pitch counter (fraction
  included), and reads the block at SSA. The spec says only that SSA is
  copied and the envelope starts at zero; its state-machine notes hint that
  the interpolation history may survive. Audible only in the first block of
  a sample, which in a VAB is the zero block.
- **R4. Flags.** Loop start (bit 2) copies the address to LSAX when the
  block is read, always - also when software wrote LSAX itself. The end flag
  acts after the block's 28th sample has been consumed: ENDX, address = LSAX,
  and with bit 1 clear the release phase with the level at zero (and the
  rate counter at zero). The same applies to a voice in noise mode, which
  still reads its ADPCM.
- **R5. Exponential decrease floors**: `step * level >> 15`. Evidence: with
  truncation toward zero, a release at shift 11 (`-8 * level / 8000h`) would
  stop moving below level 4096 and never reach zero; with the floor every
  step is at least -1. The test's exponential release from `7FFFh` at shift 0
  goes `2^k - 1` down to 0 in 15 samples.
- **R6. "ALL_BITS" is the phase's own field all ones**: the 7-bit rate
  (shift and step) for attack, sustain and the sweeps (`7Fh`), the 5-bit
  shift for release (`1Fh` - the spec's "0x1f for decay/release"); decay's
  4-bit shift never qualifies (its increment is at least `800h` anyway). In
  that case the counter increment is not raised to 1, so the level never
  moves: a release at shift `1Fh` holds its level forever (tested). Shifts
  27..31 otherwise give increment 1, so "0x76 behaves like 0x6A" holds
  (tested).
- **R7. The rate counter** clears bit 15 after a step (the remainder is
  kept) and restarts at zero on key on, on every phase change, on key off
  and on a write to a volume register. Since every increment is a power of
  two up to `8000h`, this matters only when the increment changes mid-count.
- **R8. Phase changes follow the step that reaches the target**, in the same
  sample: attack to decay when the level is `7FFFh`; decay to sustain when
  the level is `<= (N+1) * 800h`; release to off at zero. So decay always
  takes at least one step, even with sustain level 15 (`8000h`). The
  alternative, testing before the step, would differ by one envelope step at
  each change.
- **R9. Key on and key off** are latched and acted on at the start of the
  next sample, key off first, then key on (so a voice keyed off and on
  between two samples restarts). KON clears ENDX for its voices. The
  hardware's 2-sample service grid and its 6-7 sample delay to a non-zero
  ENVX are not modelled: our onset is earlier by a constant few samples,
  which the renders' alignment absorbs; if a render shows a variable offset,
  this is where.
- **R10. Volume registers.** A fixed-mode write sets the current level at
  once (from the next sample); a sweep-mode write starts from the current
  level. The sweep uses the envelope operation as written, phase bit
  included (the spec marks the negative-phase behaviour as partly untested).
- **R11. Interpolation index and counter wrap.** The four samples are the
  block's sample at `counter >> 12` ("new") and the three before it, reaching
  into the previous block's last three. The counter advances after the
  sample is produced; when it reaches `28 << 12` it loses `28 << 12` (the
  fraction kept) and the next block is decoded. So a voice keyed on at pitch
  `1000h` outputs `gauss(0,0,0,s0)` first, and the output is centred about
  two samples behind "new". The pitch limit is the spec's: above `3FFFh` the
  step is `4000h`.
- **R12. Reverb arithmetic.** Every product is `(a * b) >> 15` and every
  product and every sum is saturated to 16 bits (the spec: "Intermediate
  values DO saturate"; where exactly is unknown). The comb sum saturates after
  each addition. `[mX-2]` is the halfword before `mX` (byte offset -2; the
  computation-order table writes it `mX-1`, in halfwords); `[mAPF-dAPF]` is
  `(mAPF - dAPF) * 8` bytes. vLIN / vRIN are applied after the input is
  resampled, inside the 22 kHz step; EVOL after the output is resampled.
  The reads and writes follow the "Reverb Computation Order" table, so the
  APF2 source is read before APF1 is written.
- **R13. Noise** steps once a sample before the voices read it. Level and
  timer start at zero. The spec quotes no period; from zero the generator
  runs through 65,535 states before repeating (the test prints it; all
  ones is the state it never enters, as an XNOR feedback register should).
- **R14. Within a sample**, each voice in order 0..23: the sample (noise or
  interpolation), OUTX with the envelope level from *before* this sample's
  envelope step, then the envelope step, the side volumes (current level
  before their sweep step), then the pitch counter. A modulated voice uses
  voice x-1's OUTX of the same sample. With the envelope at zero the
  interpolation is skipped (an exact shortcut).
- **R15. Reverb timing.** Left on even samples counted from `Reset`, right on
  odd ones; the buffer address advances after the right step. The input
  filter runs over the last 39 input samples including the current one,
  `>> 15`; the output is zero-stuffed at 44.1 kHz and filtered with gain 2
  (`>> 14`). The resulting delay is the spec's 38 samples (19 + 19), and the
  structure test finds echoes at exactly `38 + 8 * (delay in 8-byte units)`
  samples. On hardware the even/odd phase against the voices is fixed per
  power-on; ours is fixed by `Reset`.
- **R16. The mix.** Dry = the sum of all voices' side outputs, clamped;
  plus the reverb return (`resampled * EVOL >> 15`), clamped; times the main
  volume (`>> 15`), clamped. **Whether the main volume scales the reverb
  return** is not stated in the spec (its block diagram has a separate
  "reverb mixer"); the model scales both. A render with the main volume
  below full and reverb on decides it. ATTR bit 14 clear (mute) gives
  silence; the voices and the reverb keep running.
- **R17. Capture buffers** are written every sample from `Reset`, the
  index wrapping at `200h`; CD left / right get zeros since no CD input is
  modelled. A sample stored below `1000h` would be overwritten, as on the
  hardware.

## 4. For the groups that come next

- **libspu's reverb modes.** The spec's preset table is by name; it does not
  say which `SPU_REV_MODE_*` number `SpuSetReverbModeParam` maps to which
  set, nor whether libspu 3.7 writes exactly these values (the spec's values
  are what software writes, not necessarily libspu's). The SEQ group's
  reading of `SLPS_009.90` should take the table libspu carries in the boot
  EXE and compare it with `kSpuReverbPresets`; `SetReverbRegister` takes any
  set.
- **Reverb depth.** `SsUtSetReverbDepth` and `SpuSetReverbDepth` write EVOL;
  `SetReverbOutputVolume` is that register.
- **Voice allocation reads**: `Endx()` (ENDX) and `VoiceEnvelope(v)` (ENVX)
  are the hardware's; `VoiceAdsrPhase(v)` is not a register (tests, traces).
- **The R1 disagreement with `vag.py`** needs one decision before
  `vag.decode` is used as an oracle.

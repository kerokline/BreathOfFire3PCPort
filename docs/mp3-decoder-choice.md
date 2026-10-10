# The MP3 decoder's replacement: minimp3, and why not dr_mp3

**Status:** DECIDED (2026-10-10, the owner: "minimp3 unless there is a known
decoding bug that has been resolved" in dr_mp3 - none was found; nothing
vendored or built yet)

[`platform-layers-plan.md`](platform-layers-plan.md) section 2.4 left the
choice of a permissive decoder open. Since 2026-10-06 the PC's MP3s stay as
a music path beside the disc's sequences (DIV-0081's loop table for them,
DIV-0087 for the disc), so a player with the PC install alone still needs a
decoder after the cutover, and Capcom's (200 starts, `0x5AB7E0..0x5B8DA0`)
must go for state 2. The format to decode is measured
([`bgm-comparison.md`](bgm-comparison.md)): MPEG-1 layer III, 44.1 kHz,
128 kbit/s CBR, plain stereo, no tags.

## 1. The candidates

Two single-header decoders with licences [`LICENSING.md`](LICENSING.md)
section 4 allows:

| | minimp3 | dr_mp3 |
|---|---|---|
| Where | github.com/lieff/minimp3 | github.com/mackron/dr_libs |
| Read at | `ea99364f` (2026-07-27) | `dfe83776` (2026-09-01); `dr_mp3.h` last changed `51e61d3` (2026-08-30), v0.7.4 unreleased |
| Licence | CC0 1.0 | public domain (Unlicense) or MIT-0 |
| Decoder | `minimp3.h`; seeking and stream I/O in `minimp3_ex.h` | minimp3's decoder, renamed, inside its own stream layer |
| Upkeep | four commits in 2026: three hardening the VBR-tag parsing and a seek truncation in `minimp3_ex.h`, one ARMv6-M build fix | steady; v0.7.4's notes harden the same Xing / Info tag parsing |

dr_mp3's own changelog says what it is: "Bring up to date with minimp3"
recurs (v0.6.17, .21, .26, .29). The question the owner set was whether
dr_mp3 has fixed a decoding bug that minimp3 still has.

## 2. The measurement

[`tools/mp3_core_diff.py`](../tools/mp3_core_diff.py) puts both decoders on
one naming (dr_mp3's `drmp3_` / `drmp3dec_` / `drmp3d_` prefixes and
`drmp3_uint8`-style types; minimp3's `_t` typedefs), drops comments,
preprocessor lines and whitespace, splits each into top-level items and
diffs them item by item:

- **42 items token-identical**: every table (`g_pow43`, the Huffman tables,
  the scale-factor bands, the synthesis window), the bit reader, the header
  tests, layer I/II dequantisation, layer III side info, Huffman decoding,
  mid-side and intensity stereo, reordering, antialiasing, the IMDCTs, the
  bit reservoir's save and restore, `L3_decode`, the DCT, frame sync and
  `find_frame`.
- **15 differ**, each read:

| Item | Difference | On the output |
|---|---|---|
| `L12_read_scalefactors`, `L12_read_scale_info`, `L3_read_side_info`, `L3_read_scalefactors`, `L3_decode_scalefactors`, `L3_intensity_stereo` | explicit `(uint8_t)` / `(int)` casts on values minimp3 assigns implicitly; one declaration moved | none: the same conversion |
| `L3_huffman` | a space before a table's closing brace | none |
| `scale_pcm`, `f32_to_s16` | `32766.5` written `32766.5f` (exact in a float); C89 declaration order; NEON constants by `VSET` | none |
| `synth`, `synth_pair`, `synth_granule` | casts on `_mm_extract_epi16`; the sample type's name; NEON's scale constant | none on x86 |
| `have_simd` (x86, and the ARM variant's `(void)`) | dr_mp3 dropped the line that caches "no SSE2" when CPUID reports no leaves, so on such a CPU it answers -1 (true) | none on any x86 that runs Windows (CPUID leaf 1 is always there) |
| `decode_frame` | scratch buffers in the decoder struct, not on the stack; `frame_offset` dropped, `hz` renamed `sample_rate`; **`pcm == NULL`** (below) | none, except `pcm == NULL` |

`bs_t`, `L3_gr_info_t` and `mp3dec_scratch_t` are declared in dr_mp3's
public header (the scratch is part of its decoder struct), outside the
region compared; read side by side, their fields match, as do the size
constants (`MAX_BITRESERVOIR_BYTES` 511, `MAX_FREE_FORMAT_FRAME_SIZE` 2304,
`MAX_L3_FRAME_PAYLOAD_BYTES`, `MAX_FRAME_SYNC_MATCHES` 10).

**The one behavioural difference.** Called with `pcm == NULL` (frame info
only), minimp3 returns before reading the frame; dr_mp3, for layer III,
still reads the side info and restores and saves the bit reservoir, and
returns early only for layers I and II. So a caller that skips frames with
`pcm == NULL` and then decodes gets, from minimp3, a first frame that may
borrow stale reservoir bytes. dr_mp3's v0.7.1 note ("a decoding
inconsistency when seeking") is this, found in its own seek. It is not a
fault in decoding a stream from its start; `minimp3_ex.h`'s seek avoids it
by backing up `MINIMP3_PREDECODE_FRAMES` (2) frames and up to 511 bytes of
reservoir before the target and decoding from there.

**No decoding bug fixed in one and open in the other was found.** The
decoders are the same code; their PCM should be bit-identical on our files.

## 3. The decision, and the rule it brings

**minimp3**, `minimp3.h` at `ea99364f`, under CC0. Its notice goes into
[`THIRD_PARTY.md`](THIRD_PARTY.md) when it is vendored, not before (that
file lists what the repository carries).

**The rule for our side of the seam** (`Music_OpenDecoder`, `Mp3_Decode`,
`Mp3_Seek`, the end-of-stream answer `0xFFFFFDFE`): never skip frames with
`pcm == NULL`. A seek, and DIV-0081's loop jump, decodes from at least two
frames before the target into a buffer it discards (or uses
`minimp3_ex.h`'s seek, which does). The loop-seam measurement of
[`bgm-comparison.md`](bgm-comparison.md) section 12 is the check that
catches a breach.

## 4. What follows (not done)

1. Vendor `minimp3.h`, its notice in `THIRD_PARTY.md`; the decoder behind
   the music seam, Capcom's eight decoder entries in `symbols.toml` without
   `impl` taken ([`exe-import-engine.md`](exe-import-engine.md) 4.1).
2. A ledger entry - two correct decoders differ in the last bits: the PCM
   against Capcom's decoder per track as an error bound, and every looping
   track's seam (platform-layers-plan 2.4).
3. With the decoder gone, `calloc` / `free`, `sscanf`'s second caller and
   `_ftol` lose their reason to stay Capcom's ([`crt-rest.md`](crt-rest.md)
   section 3), and state 2's proof can run.

## Commands

```
git clone https://github.com/lieff/minimp3 <dir>/minimp3
git clone https://github.com/mackron/dr_libs <dir>/dr_libs
python tools/mp3_core_diff.py <dir>/minimp3/minimp3.h <dir>/dr_libs/dr_mp3.h --out core.diff
# identical: 42, differing: 15 at the commits above
```

# The MP3 decoder replaced: minimp3 behind the music seam (DIV-0091)

**Status:** BUILT (2026-10-10, `platform/mp3-decoder`; every one of the 177 MP3s decoded through both decoders in one process and compared; self-tested headless; **heard 2026-10-11**: six songs as WAV pairs, the owner hears no difference, and the pairs' levels agree to 0.0001 dB)

[`mp3-decoder-choice.md`](mp3-decoder-choice.md) chose minimp3. This is
what was built on it, how the seam is shaped, and what the comparison
against Capcom's decoder measured - including two things the choice did not
know: Capcom's decoder stores a stale frame count at the end of a stream,
and it is the less accurate of the two.

## 1. What was built

- `third_party/minimp3/minimp3.h` and its `LICENSE` (CC0 1.0), verbatim
  from `github.com/lieff/minimp3` at `ea99364f`, sha256
  `29735314…13d7` ([`THIRD_PARTY.md`](THIRD_PARTY.md) section 3). The
  header is included once, in `src/audio/mp3.cpp`, with
  `MINIMP3_IMPLEMENTATION`; 16-bit output; its x86 SSE2 path is chosen by
  its own CPUID test.
- `src/audio/mp3.h` / `mp3.cpp`, namespace `mp3dec`: the eight entries the
  seam reaches through `sound::Callees` - `Mp3_Create`, `Mp3_MemoryIo`,
  `Mp3_SetIo`, `Mp3_Open`, `Mp3_Start`, `Mp3_Decode`, `Mp3_Seek`,
  `Mp3_Destroy` - as stdcall functions of the same signatures, put into the
  callee table by `mp3dec::Arm()` after every module's self-test
  (`inject_all.cpp`, beside DIV-0087's). Nothing is injected at Capcom's
  addresses: the decoder's 200 functions are simply no longer called. Under
  `BOF3X_MP3=original` the table keeps Capcom's (the A/B - an agent's and the
  owner's switch, deliberately not a launcher box: the owner, 2026-10-11, and it
  dies at the cutover); under
  `BOF3X_ORIGINAL` naming `Music_Decode` or `Music_OpenDecoder`, Capcom's
  own callers run and call Capcom's decoder directly, as before.
- **The seam's shape is kept, not redesigned.** Every caller of ours reads
  the frame's byte count at `[[decoder + 0x14] + 8]` (`Music_Decode`,
  `music_loops`' `LoopFrame`, the sound fuzz's stubs, the loop self-test's
  fake), so ours keeps a frame-info pointer at `+0x14` whose `+8` is the
  count (`static_assert`ed). `Mp3_Open` takes the name `Music_OpenDecoder`
  formats, `%X@%X` (size, address), and reads the file there; `mode` must
  be `-1`, the one value the game passes. `Mp3_MemoryIo` fills the 0x2C-byte
  table with zeros and `Mp3_SetIo` ignores it: ours has no I/O callbacks.
- **`Mp3_Seek(decoder, 0)`** resets minimp3's state and the position, so the
  frames after it are the first pass's bit for bit (the rule of
  [`mp3-decoder-choice.md`](mp3-decoder-choice.md) section 3: no skipping
  with `pcm == NULL`). Any other position is fatal: the game seeks to 0
  alone (`Music_Decode`'s rewind and `LoopDecode`'s jump).
- Memory is the toolchain's heap (`new` / `delete`), not `Crt_malloc` /
  `calloc`: the reason `Crt_malloc` and `Crt_free` stayed Capcom's
  ([`crt-rest.md`](crt-rest.md) section 2.6) is gone once the decoder is
  ours.
- **`Mp3_Decode` runs on a stack of its own** (a 256 KB block, a guard page
  under it, esp switched around the one call). minimp3 keeps its decoding
  scratch on the stack - some 13 KB a call - where Capcom's decoder kept its
  state in heap blocks, and the game's tasks run on 16 KB slices of the main
  thread's stack below WinMain's frame (`Task_SetStackBase`,
  [`task_sched.md`](task_sched.md)); the title's `Music_Play` is on one.
  **Found by the owner the same evening**, starting the game with no cache
  (the MP3 path): an access violation at a float bit pattern with every
  register a float - the scratch had run through the slice into the next
  task's saved context. The comparison from WinMain never saw it (a shallow
  frame). Seen fixed: the title and the attract sequence through the MP3s.
- A stream that is not 44,100 Hz stereo is fatal (the ring is built for
  that format and Capcom's behaviour on another is unmeasured); the game's
  177 files are all that format.

## 2. The comparison, measured

Capcom's decoder allocates from the runtime's heap, which does not exist
while the start-up self-tests run (the DLL is injected before the
executable's entry point; the first attempt crashed there). So the
comparison runs from `Game_WinMain`'s first line, the first code of ours
with the runtime under it: `BOF3X_MP3_COMPARE=<path.tsv>` decodes every
file of `BGM\` and `SND\` that begins with an MPEG sync (166 + 11; the
eleven `SND\` MP3s are the jingles and the two vocal songs `KARA`, `PURE`,
which `Sound_LoadStream` plays through `Music_Start`) through Capcom's
decoder and through ours, driven exactly as `Music_OpenDecoder`,
`Music_Decode` and `Music_Release` drive the seam, writes one row per file,
and ends the process. 22 seconds for the 177. One row of
`BOF3X_MP3_DUMP=<dir>` / `BOF3X_MP3_DUMP_FILES=<a,b>` writes both decoders'
PCM raw for a third decoder's verdict.

**Shape - identical on all 177 files:**

| | Capcom's | ours |
|---|---|---|
| Frames before the end of the stream | = an independent walk of the file's headers (`144000 * bitrate / 44100 + padding`), every file | the same count |
| Bytes a frame | 4,608 (1,152 stereo samples), every frame of every file | the same |
| After `Seek(0)`, the first 16 frames | byte for byte the first pass's | the same (the self-test replays a whole file) |
| At the end of the stream | returns `0xFFFFFDFE`, **writes nothing to `out`** (a 0xA5 fill untouched), and **leaves the last frame's 4,608 in the count** | the same, by design (below) |

**The end-of-stream count is a defect of the original, mirrored.**
`Music_Decode` adds the count after every call, the end included
(`symbols.toml`'s evidence; ours does the same), so on a looping track the
original steps `filled` over 1,152 samples it never wrote at every rewind -
the staging buffer's bytes from its previous fill, 0.42 s of music earlier,
play for 26 ms at the seam - and on a once-only track those 26 ms precede
the zeros. [`known-defects.md`](known-defects.md) D240. Ours stores the
same 4,608 so the stream's logic is unchanged by the swap; storing 0 there
would be the fix, one line, and is the owner's call (DIV-0081's measured
loops never reach the end of a stream, so its 39 tracks are untouched
either way).

**Samples - the two decoders differ, and ffmpeg sides with ours:**

| Over the 177 files | |
|---|---|
| Frames, samples | 332,187 frames; 765,358,848 samples (stereo interleaved) |
| Samples that differ | 3.38 % |
| Max absolute difference per file (16-bit units) | min 1, median 110, 90th percentile 256, max 370 (`SND\KARA.DAT`) |
| RMS difference per file | min 0.022, median 2.11, 90th percentile 4.78, max 11.9 (`KARA`) |
| Files within 8 / 32 / 128 | 7 / 37 / 99 of 177 |

That is far past the last bits two full-precision decoders differ in. Three
files decoded by `ffmpeg` 9.0.1 (`-f mp3 … -f s16le`), the same length as
both to the sample and aligned at lag 0:

| File | ours against ffmpeg | Capcom's against ffmpeg |
|---|---|---|
| `BGM\001.DAT` (782 frames) | max 1, RMS 0.038, 0.15 % differ | max 14, RMS 0.43, 1.46 % |
| `BGM\005.DAT` (2,178 frames) | max 1, RMS 0.036, 0.13 % | max 219, RMS 3.33, 1.32 % |
| `SND\Win.DAT` (322 frames) | max 1, RMS 0.030, 0.09 % | max 1, RMS 0.026, 0.07 % |

So ours is ffmpeg's to within one unit on every sample; Capcom's agrees
with both on most frames and is wrong in some: on `005.DAT` 48 of 2,178
frames are off by more than 32, each in **one channel** (frame 644: the
left off by up to 183 in 738 of its 1,152 samples, the right by at most 1),
not near full scale (the median magnitude of a sample Capcom's gets wrong
is 1,969 of 32,767 - not clipping). Which decoding case Capcom's library
mishandles is not read; it is Capcom's library's fault, consistent with a
limited-precision decoder of its day, and the files the game plays are
decoded more accurately by ours. Audibility: -68 dBFS RMS at the median
file, -58 at the worst. **Heard 2026-10-11** - the owner, on six songs as WAV pairs from `BOF3X_MP3_DUMP` (the town theme 000, 045, 086, the title 141, the battle theme 153, the fanfare 165; `analysis/bgm/listen2/`): "all sound pretty similar to me ... maybe slight variations in volume, but it could just be placebo". It is: the pairs' RMS levels agree to 0.0001 dB and their peaks to 0.006 dB on every song (the decoders' difference is -68 dBFS at the median file).

## 3. Self-tests and switches

- `BOF3X_SHADOW=sound` runs `mp3dec::SelfTest()`: `BGM\001.DAT` through
  ours alone - as many frames as the header walk counts (782), every frame
  4,608 bytes, 4,608 stored at the end, the replay after `Seek(0)` identical
  over all 782 frames, and the `%X@%X` name's round trip (five malformed
  names refused). Capcom's cannot run there (section 2).
- `BOF3X_MP3` - unset or `minimp3`: ours; `original`: Capcom's; anything
  else fatal. Logged as `DIV-0091`. No launcher box yet (the `music=` box's
  neighbour, with `cache=`'s - HANDOFF).
- `BOF3X_MP3_COMPARE`, `BOF3X_MP3_DUMP`, `BOF3X_MP3_DUMP_FILES`: section 2. `BOF3X_MP3_COMPARE` runs Capcom's decoder, so it is hosted only: `bof3x.exe` refuses it and the launcher takes the hosted build for it (2026-10-29, the relocatable-image round's stage D).
- The state hash is unaffected: `Music_Staging` is in its skip list and the
  frame's logic sees only `Music_Finished` and the fades.

## 4. What follows

1. ~~**The owner's listen**~~ - done 2026-10-11 as offline WAV pairs (section 2's
   dump, six songs): no audible difference, the levels identical. What the
   in-game A/B under `BOF3X_MP3=original` would add is the seams (DIV-0081's
   loops, D240's end), which the owner has heard on our path in play.
2. ~~**D240's fix**~~ - done 2026-10-11 as DIV-0093 (`kEosBytes = 0`; the owner:
   zeros, as the other stale-byte decisions; the self-test exit 0). As planned:
   a ledger entry, `kEosBytes = 0` in
   `mp3.cpp`, and the self-test's expectation.
3. **The C runtime's remainder - done the same evening** on `crt/allocator`
   ([`crt-rest.md`](crt-rest.md) 2.6, 2.7): `malloc` / `calloc` / `free` the
   toolchain's as a set (no guard needed: with all three moved, Capcom's
   decoder under `BOF3X_MP3=original` pairs inside one heap too), `sscanf`
   ours over `Cfg_Load`'s format; `_ftol` has no caller left.
4. **State 2's proof** ([`platform-layers-plan.md`](platform-layers-plan.md)
   section 3): the tracer armed on `0x5AB7E0..0x5B8DA0` over the attract
   sequence and the routes should now record nothing.

## 5. Not verified

Nothing heard - but seen: the title and the attract sequence play through
the MP3s with no cache (2026-10-10 evening, after the stack fix above). Capcom's behaviour on a stream that is not 44,100 Hz stereo,
on a `mode` other than `-1`, on a seek to a position other than 0 (ours
aborts on each). Which decoding case Capcom's library gets wrong. The
`eos_kind` classification saw only "nothing"; the other kinds are untested
code.

## 6. The resume after a battle: the seek (DIV-0092, 2026-10-10 night)

The field's song resumes after a fight where it was heard, as on the owner's
PlayStation. [`DIVERGENCE.md`](DIVERGENCE.md) DIV-0092 and
`src/game/music_resume.h` cover the shared part. On the MP3 path the
position is a sample of the stream `Music_Decode` hands out. The MP3 path
has no position seek. Both decoders take `Seek(0)` alone (ours aborts on
any other position, and Capcom's meaning for one is unmeasured), so the
resume uses the measured loop table's own mechanism: decode from the top
and throw the samples away. Decoding from the top leaves the decoder as the
first pass left it, bit reservoir and overlap included. The samples after
the skip are therefore the first pass's, bit for bit.

- **Where.** `Music_Start`, after the decoder is opened and `LoopBegin` has
  looked up the track's measured row, and before `Music_CreateBuffer`
  decodes the first half. `ResumeSkip` calls `Music_Decode` itself into a
  scratch half, so every rule of the stream applies unchanged: the
  original's rewind at the end of the stream, D240's 1,152 uncounted
  samples, and the measured loop's jump and crossfade. During the skip
  `Music_Loops` is the new track's (the original sets it only after the
  first half); it is put back afterwards.
- **The fold** (`FoldMp3` in `sound.cpp`, at `Remember`). A measured loop
  repeats `start..end` after `end`, its first `fade` samples blended with
  the end's. A point past `end` is taken to `start + k`, or to `end + k`
  inside the blend. A track the original rewinds repeats every
  `(frames + 1) * 1,152` samples: its frames counted by `mp3dec::CountFrames`
  (the header walk of section 3), plus D240's step. The original's path
  decodes whole frames into the caller's buffer, so its point is taken down
  to a frame's start, 26 ms at most. A once-only track is not folded: when
  the skip meets its end (`Music_Finished`), the decoder is rewound
  (`Seek(0)`, `LoopBegin` again) and the track starts from the top.
- **The cost.** Decoding is bounded by one pass, plus one loop for a
  measured row. It has not been timed in the game. Section 2's corpus run
  decodes all 177 files through both decoders in about 25 s, so a few
  minutes of one track is a fraction of a second.
- **Self-test** (`BOF3X_SHADOW=sound`, `music_resume::SelfTest` in
  `sound.cpp`). It uses the loop check's stand-in decoder (every sample
  carries its index, 40 frames). The stream resumed at a folded point
  equals the stream from the top at the point heard, sample for sample,
  except the phantom step's samples, which are whatever the buffer held.
  Four cases are checked: a rewinding track inside its first pass and three
  passes on, a once-only track past its end (from the top), and a measured
  loop two loops on and inside its crossfade. The remembering is checked
  through a stand-in buffer. The heard sample is the play cursor's half's
  first sample plus the cursor's offset. Nothing is remembered with
  DIV-0092's switch off. A remembered position is used once, and only for
  its own track.
- **Not verified.** Nothing has been heard, and the skip's time in the game
  is unmeasured. The route is `tools/recipes/musicStopCheck.txt` under
  `BOF3X_MUSIC=mp3 BOF3X_MUSIC_LOG=1`: `remember track 91 ...` at the fight,
  `resume track 91 at sample S` when the field returns. The ear decides
  whether the song picks up where it left off. The folding of a rewinding
  track trusts that the decoder makes as many frames as the header walk
  counts (section 3 checks that for `001.DAT` only). If the counts
  disagreed, each pass folded away would move the point by a frame.

## Commands

```
# the corpus comparison (the game starts and ends in WinMain; ~25 s)
BOF3X_MP3_COMPARE='C:\...\mp3_compare.tsv' build/bof3x-launcher.exe --game bof3 --no-config
# with PCM dumps of named files, then a third decoder
BOF3X_MP3_DUMP='C:\...\pcm' BOF3X_MP3_DUMP_FILES='001.DAT,005.DAT' BOF3X_MP3_COMPARE=... build/bof3x-launcher.exe --game bof3 --no-config
ffmpeg -f mp3 -i bof3/BGM/001.DAT -f s16le -acodec pcm_s16le 001.DAT.ffmpeg.pcm
# the self-test
BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=sound build/bof3x-launcher.exe --game bof3 --no-config
```

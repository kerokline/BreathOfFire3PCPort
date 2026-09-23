# The sound layer (group Q)

**Status:** IN PROGRESS (2026-09-22) - 23 functions ours in
[`src/game/sound.cpp`](../src/game/sound.cpp), each read to its last
instruction, fuzzed against Capcom's at start-up (`BOF3X_SHADOW=sound`,
46,000 rounds, 0 mismatches) with 70 negative controls: 64 refused by a
count, 1 by a count only after minutes (a would-be hang), 2 only by a fault,
3 that change nothing. **Not yet through a live batch** - and the live batch
covers sound only by the owner's ear (§6). No divergence; three defects of
Capcom's written down ([`known-defects.md`](known-defects.md) D24..D26), one
of which, D26, every fade in the game has.

The fourth parallel round's group Q
([`takeover-queue-round4.md`](takeover-queue-round4.md)): the catalogue's
§4.12 ([`attract-remaining.md`](attract-remaining.md)) - the glue between the
game's sound calls and DirectSound - and `SndBuf_Stop` `0x5A6C30`, which the
catalogue did not list (no trace entry of its own) but `Sound_PlayEffect`
calls. `Snd_LoadBank` `0x587CD0` is **not** ours, whatever the queue says: it
has no `impl` in `symbols.toml` and no source; its caller `LoadDatFile` is
ours, which is why its callers read `0xFFFFFFFF` in the counts. It is left as
it was; it now reaches our `SndBuf_FromWave` and `SndBuf_Release`.

**No PSX twin for any body.** The PSX drives the SPU: `SE_Play` `0x8015E908`
in the sibling's `symbols.toml` keys up to four SPU voices per cue, bank:id
alike. `analysis/pairs_propagated.json` pairs the PC's entry points with PSX
sound functions by call only, several PSX functions to one PC function
(`Music_FadeOutStop` has three), which is the rewrite showing. What the port
kept is the *cue* layout: a cue is four voice words, as on the PSX.

## 1. The functions

Sizes are the bodies' true extents by capstone; `pe_funcs.py` was right for
21 of them and ran on for two (`0x5A6FB0` 0x40, `0x5A7050` 0x30, each into a
pointer-reached twin). Calls are `hidden_b`'s.

| PC | name | bytes | calls | what |
|---|---|---|---|---|
| `0x587740` | `Sound_PlayEffect` | `0x119` | 148 | a cue: four voice words, each a stop or a play on a channel |
| `0x587A20` | `Music_LoadFile` (new) | `0xB8` | 9 | a BGM file read whole; which name opened says loop or once |
| `0x587AE0` | `Music_Play` | `0x60` | 18 | the track started from the top, fading in |
| `0x587B40` | `Music_FadeOutStop` | `0x38` | 8 | fade to nothing, then stop |
| `0x587BA0` | `Music_FadeIn` | `0x36` | 12 | fade up to 127 |
| `0x587BE0` | `Music_FadeOut` | `0x32` | 3 | fade to nothing, keep playing |
| `0x587C70` | `Sound_Tick` (new) | `0x51` | 6,710,895 | the pump, and one fade step - per spin of WinMain's frame wait |
| `0x5A69C0` | `SndBuf_FromWave` (new) | `0x1A` | 176 | a voice's buffer, if there is a device |
| `0x5A69E0` | `SndBuf_CreateFromWave` (new) | `0xBC` | 176 | a static buffer from a RIFF WAVE |
| `0x5A6AA0` | `Wave_FindData` (new) | `0x44` | 176 | the WAVE's data chunk |
| `0x5A6AF0` | `SndBuf_Write` (new) | `0xBB` | 1,328 | Lock, copy, Unlock |
| `0x5A6BB0` | `SndBuf_Play` (new) | `0x75` | 12 | position 0, frequency, Play |
| `0x5A6C30` | `SndBuf_Stop` (new) | `0x23` | not traced | Stop if playing |
| `0x5A6C90` | `SndBuf_Release` (new) | `0x29` | 153 | Stop if playing, Release |
| `0x5A6CC0` | `Music_Start` (new) | `0x9F` | 12 | the stream (re)built from a file in memory |
| `0x5A6D60` | `Music_OpenDecoder` (new) | `0x81` | 12 | the MP3 decoder on that memory |
| `0x5A6DF0` | `Music_SetNotify` (new) | `0x70` | 12 | two events, at 0 and at the half |
| `0x5A6E60` | `Music_CreateBuffer` (new) | `0xCC` | 12 | the 44.1 kHz stereo ring, first half decoded |
| `0x5A6F30` | `Music_Decode` (new) | `0x79` | 1,152 | frames into a buffer; loop or zero-fill at the end |
| `0x5A6FB0` | `Music_SetVolume` (new) | `0x31` | 514 | 0..127 to -10000..0 |
| `0x5A7050` | `Music_Stop` (new) | `0x29` | 11 | Stop the ring if playing |
| `0x5A70A0` | `Music_Release` (new) | `0x99` | 11 | everything the stream holds let go |
| `0x5A7230` | `Music_Pump` (new) | `0x84` | 6,710,895 | refill the half the cursor left |

`symbols.toml`'s group Q block has each one's evidence, the eight decoder
entry points it calls (`Mp3_*`, named by use, `hypothesis`, stdcall - each
ends `ret 4` / `8` / `0xC`), and the data: `Sound_Channels`, `Sound_Banks`,
the `Music_*` globals at `0x6BDE48..0x6BDE63` and `0x7DE3B4..0x7DE3E3`,
`Music_Track` `0x904131` (the byte several of our files already call
`kMusicTrack`), `Music_LoadedTrack` `0x6653BC`, `Music_Staging` `0x7CC378`,
`Snd_BufferDesc`, `Snd_WaveFormat`, `Snd_Device`.

**How it fits together.** Sound effects: `Snd_LoadBank` (at `DAT` load) makes
a static DirectSound buffer per voice entry (`SndBuf_FromWave`);
`Sound_PlayEffect(bank << 8 | cue)` plays up to four of them, remembering a
looping one per channel so a later cue (voice `0xFF`) can stop it. Music:
`Music_Play(track, frames)` loads `BGM\NNN.DAT` (loops) or `BGM\NNNN.DAT`
(plays once) whole, copies it for the decoder, which reads it from memory
through a table of I/O callbacks (`Mp3_MemoryIo`) under the name
`size@address`; a 0x24000-byte ring is filled half by half by `Music_Pump`
as the notifications at 0 and 0x12000 fire. The track is the file's MP3
([`asset-loading-path.md`](asset-loading-path.md) §1a).

## 2. What is kept as Capcom had it

- **Five functions return an eax their `void` type hides.**
  `Sound_PlayEffect` returns 0 (its loop count), the three fades return
  `frames`, `Music_Play` the track when it did nothing and the fade's eax
  otherwise. Of their 1,500 or so call sites (rel32 scan) hundreds end
  `call; ...; ret` and hand eax to their own callers, which were not all
  read. So ours return the same: each is defined under the assembler name of
  the `void` declaration with an `int` result (`sound.cpp`, the
  `__asm__("_Sound_PlayEffect")` declarations) - callers written against
  `void` still compile. The fuzz compares the eax.
- **The x87 is kept.** The fades divide, `Sound_Tick` adds and
  `Music_SetVolume` scales with the same x87 instructions on the image's own
  floats (`0x5C427C` 127.0, `0x5C464C` 1/127, `0x5C4648` 10000.0), so they
  round as the original under any control word; `Music_SetVolume` truncates
  as the CRT's `_ftol` `0x5B9550` does. `Music_FadeOutStop` / `FadeOut`
  divide by `frames` loaded as a 64-bit integer with a zero high half
  (unsigned), `Music_FadeIn` by `frames` as a signed dword. The volume is
  handed on by its bits, as a dword move.
- **Every re-read.** `Sound_PlayEffect` reads each cue word afresh and the
  voice's buffer again after `SndBuf_Play`; `Music_LoadFile` hands File_Read
  `Music_FileSize` as read back after the allocation; `Music_Play` loads
  `Music_File` / `FileSize` / `FileLoops` after `Music_SetVolume`;
  `Music_Start`, `Music_Stop`, `Music_Release` and `Music_Pump` read
  `Music_Buffer` again after calls; `Music_Decode` reads the decoder (and the
  frame size through it) after each decode; `Music_SetNotify` reads the first
  event back from memory and uses the second as returned. Each is a control
  in §5.
- **Argument slots as outputs.** `SndBuf_CreateFromWave` receives its buffer
  in its own `wave` slot, `SndBuf_Play` the data pointer in its `sample`
  slot, `SndBuf_Write` Lock's four outputs in its argument slots,
  `SndBuf_Stop` / `Release` the status in the buffer's slot. Ours starts each
  output as the slot held (`sample`; buffer, offset and size; the pointer),
  for a callee that reports success without writing. Where the original's
  slot is the *caller's* `ecx` (`Music_Stop`, `Music_Release`'s status, the
  data pointer in `SndBuf_CreateFromWave`, Lock's first pointer) ours starts
  at 0: not reproducible, and DirectSound writes them.
- **The unchecked.** `Sound_PlayEffect`: bank 0 reads the 0x384 bytes before
  `Sound_Banks`, a cue past 23 reads into the voice entries, a voice past 63
  into the next bank; channel bits 23..31 wrap by one subtraction.
  `Wave_FindData` walks with no end test. `Music_SetNotify` does not check
  QueryInterface. `Music_OpenDecoder` does not destroy a decoder that fails
  to open. `Music_Decode` counts the frame's bytes whatever the call
  returned and compares signed; a decoder that never progresses loops it for
  ever. `SndBuf_Write` copies the second region only when the first is short,
  and then n2 bytes, not the remainder.
- **Ordering.** `Music_Start` sets `Music_Loops` / `Music_Finished` after the
  first half has been decoded, so that half ends by the *previous* track's
  loop flag (unhearable unless a track is shorter than 0.42 s).
  `Music_LoadFile` sets `Music_FileLoops = 1` before the first open, so a
  failed load leaves it 1 (D24).

## 3. `Sound_Tick` and the frame wait (D26)

WinMain's wait for the next frame, `0x4FCEBC`..`0x4FCEDC`, is `call
Sound_Tick; call esi (timeGetTime); compare with the frame period; loop` - so
`Sound_Tick` runs on every spin: 6,710,895 calls in `hidden_b`'s 16,128
frames. That is fine for the pump (it asks DirectSound, without waiting,
which half is free), and why nothing here may log or allocate. But the fade
step is in the same function: a fade of `frames` is `frames` spins, a few
hundredths of one frame, where the PSX counted frames - fades are near
instant on the PC. The takeover keeps it (it is what the port does), written
down as D26; **the fix is DIV-0028** (2026-09-22, the owner's request): one
step per logic frame, gated on the frame deadline `0x6BC628` moving, off with
`BOF3X_ORIGINAL=MusicFadePerFrame`. What the game sees does not move: the
first tick after a stopping fade starts marks the music stopped
(`Music_Track` 0xFF), as the instant fade did, and a later music command
completes the stop first. The first build of it did not, and the `ab25`
frame hash caught it - frame 3439 (and 8961, a cycle on), where the original
restarts a track with `Music_Play` and ours, still fading the same track,
ignored the call and then stopped the music. With DIV-0028 switched off the
hash was identical on all 5,375 frames of that run; with it corrected and
on, identical on all 10,062 (`analysis/calltrace/ab25_oursc` against
`ab25_orig`, 2026-09-23).

## 4. The fuzz

`src/game/sound_fuzz.cpp`, `BOF3X_SHADOW=sound`, about 0.2 s: 2,000 rounds
per function, round-robin, under the x87 control word 0x027F, and 0x037F /
0x007F one round in five. Each round randomises the whole compared state,
then shapes it; both sides run from the same bytes; everything is compared -
`0x6BC8C8..0x6BDE63` (the channels, the six banks, the 0x460 bytes after
them, the music globals), `0x7DE378..0x7DE3E3` (the buffer description, the
format, the events, the pointers, the flags), `Music_LoadedTrack`,
`Music_Track`, the fuzz's wave, decode, Lock and heap buffers, the fake
decoders' frame sizes, `Wave_FindData`'s out, the control word after, the
eax where a caller reads it, and the stand-ins' log.

- **Every call out is a recorder**, the calls among the 23 included, so each
  function is tested alone. The three Win32 imports are absolute operands
  (`mov esi, [0x5C4080]` in `Music_SetNotify` at +0x19, `mov esi,
  [0x5C4084]` in `Music_Release` at +0x6B, `call [0x5C4164]` in
  `Music_Pump` at +0x25): the copies' operands are moved onto slots of ours
  holding recorders (checked against the expected slot first). `_ftol` is
  kept (plain x87). Nothing reaches the game's C runtime, a file, an event,
  or real audio.
- **DirectSound is faked**: a device, six buffers and two notification
  objects, each with a vtable of recorders for exactly the methods the
  originals call (any other slot is a `Fatal`). They answer from a hash:
  `DSERR_BUFFERLOST` from Lock and Play, other failures elsewhere; Lock hands
  out two regions with n1 of all / none / part and n2 of the rest or
  anything; CreateSoundBuffer sometimes writes its output even when failing.
- **Loud stand-ins.** Every recorder and fake method may change what its
  caller reads after it - the music globals, the track bytes, the device,
  buffer, notify and decoder pointers (only ever swapped for another fake:
  the originals dereference them unchecked), the flags, the events, the
  channels - and under `Sound_PlayEffect` the cue's later words and the
  voices' buffers. `Wave_FindData`'s recorder under `SndBuf_Play` sometimes
  leaves its out unwritten, so the slot's starting value shows.
- **Seeds.** Banks 0, 1..6, 7, 15; cues 0..23, 23, 24, 0x2F, 0xFF; channel
  bits 0, 22, 23, 24, 31; voices 0..63, 0x3F, 0x40, 0x7F, 0xFE, 0xFF; the
  track equal to `Music_Track`, to `Music_LoadedTrack`, and above 0xFF with
  the same low byte; fade frames 0, 1, -1, 0x7FFFFFFF, 0x80000000; volumes
  0, 127, -0, 63.5, the infinities, NaNs, a denormal, out of range; decode
  sizes 1..5, 0x40, 0x41, 0x100, 0x200, and 0 and below; wait results 0, 1,
  2, `WAIT_TIMEOUT`, `WAIT_FAILED`, `WAIT_ABANDONED`; chunk chains with tags
  one letter off, odd sizes, and the fmt chunk's body a sled of small `data`
  chunks, so a walk that starts wrong ends by a count.
- The decoder recorder always makes progress (a frame of at least one byte),
  and never ends a stream past what is left of the round's size - a real
  decoder's end of stream at 0x12000 is on a whole frame.

## 5. Negative controls

One planted bug each, built, `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=sound`,
reverted (the scratchpad's `controls.py`, not committed). **70 controls:
64 refused by a count, 1 by a count after minutes, 2 only by a fault (weak;
said below), 3 that change nothing.** Mismatching rounds of the function's
2,000:

| | control | refused |
|---|---|---|
| PE1 | `Sound_PlayEffect`: channel wrap at > 23 | 365 |
| PE2 | the voice's buffer not read again after the play | 148 |
| PE3 | the loop bit taken from bit 14 | 1,266 |
| PE4 | a stop voice does not forget the channel | 411 |
| PE5 | the stop voice is 0xFE | 862 |
| PE6 | the channel keeps a one-shot's buffer too | 1,171 |
| PE7 | eax 1 | 2,000 |
| PE8 | the four words read up front | 662 |
| LF1 | `Music_LoadFile`: `FileLoops = 1` after the first open | 22 |
| LF2 | the old file not freed | 1,770 |
| LF3 | File_Read asked for File_Size's result, not the global read back | 57 |
| LF4 | `LoadedTrack` not stored | 1,767 |
| LF5 | an N file keeps loops 1 | 355 |
| PL1 | `Music_Play`: the track compared by its low byte | 403 |
| PL2 | eax `frames`, not the fade's | 1,600 |
| PL3 | the volume not zeroed | 1,403 |
| PL4 | always loads | 814 |
| PL5 | loops passed as 1 | 1,081 |
| FO1 | `Music_FadeOutStop` divides signed | 403 |
| FI1 | `Music_FadeIn` divides by the low 16 bits | 489 |
| FI2 | `Music_FadeIn` sets the stop flag | 2,000 |
| FT1 | `Music_FadeOut` stops | 2,000 |
| TK1 | `Sound_Tick`: the count not read again after `Music_SetVolume` | 27 |
| TK2 | no track 0xFF at a stopping fade's end | 417 |
| TK3 | the sum as C++ | **not refused** - changes nothing: the sum of two floats rounded once to 53 or 64 bits and then to 24 is the float sum (double rounding is innocuous at 53 >= 2 * 24 + 2), and at 24 bits it is direct |
| TK4 | the volume passed through an x87 load | **not refused** - changes nothing: only a signalling NaN changes in an x87 load, and the value is the result of an `fadd`, never one |
| CW1 | `SndBuf_CreateFromWave`: cbSize not zeroed | 2,000 |
| CW2 | flags without GLOBALFOCUS | 2,000 |
| CW3 | the buffer returned after a failed create | 407 |
| CW4 | the device read before the data chunk is found | 32 |
| FD1 | `Wave_FindData`: three letters of the tag | 645 |
| FD2 | chunks padded to even sizes | refused by a **fault** only: a walk put out of step reads garbage sizes and leaves the buffer |
| FD3 | the walk starts at 0x24 whatever the fmt size | refused by a **fault** only (the fmt sled catches a start inside it by a count; a start past a short fmt chunk lands mid-chain as FD2 does) |
| SW1 | `SndBuf_Write`: the second region whenever n2 is not 0 | 323 |
| SW2 | no Restore and retry | 396 |
| SW3 | size - n1 bytes to the second region | 265 |
| SP1 | `SndBuf_Play`: no second Play after a lost buffer | 336 |
| SP2 | the data slot starts null | 91 |
| SP3 | frequency before position | 1,645 |
| SS1 | `SndBuf_Stop` stops without asking | 522 |
| SR1 | `SndBuf_Release` releases without Stop | 1,606 |
| MS1 | `Music_Start`: `Music_Loops` set before the buffer is made | 81 |
| MS2 | the buffer not read again for Play | 35 |
| MS3 | the decoder opened on the caller's file, not the copy | 1,758 |
| MS4 | `Music_Finished` cleared without a device | 151 |
| OD1 | `Music_OpenDecoder`: the name's two numbers swapped | 1,591 |
| OD2 | the decoder returned when Start fails | 396 |
| SN1 | `Music_SetNotify`: the second event read back | **not refused** - changes nothing: no call lies between its store and the read |
| SN2 | the first event not read back | 22 |
| SN3 | another IID | 2,000 |
| CB1 | `Music_CreateBuffer`: 22,050 Hz | 2,000 |
| CB2 | the first half written at the half | 1,583 |
| DE1 | `Music_Decode`: no rewind | 503 |
| DE2 | stops only past the size | refused by a **fault** only: one more frame past the size, then the zero fill's count goes negative |
| DE3 | `Music_Finished` not set | 175 |
| DE4 | the decoder not read again after a decode | 63 |
| DE5 | the comparison unsigned | refused by a count, after minutes: a size of -1 decodes toward four gigabytes (the stand-ins' log reached 396 million calls) |
| SV1 | `Music_SetVolume` rounded, not truncated | 592 |
| SV2 | C++ float arithmetic | 104 |
| ST1 | `Music_Stop` stops without asking | 534 |
| ST2 | the buffer not read again for Stop | 12 |
| RL1 | `Music_Release` keeps `Music_Buffer` | 1,426 |
| RL2 | the second event read before the first is closed | 19 |
| RL3 | the decoder kept | 1,616 |
| RL4 | `Music_Notify` cleared after the handles | 101 |
| PU1 | `Music_Pump`: the halves swapped | 223 |
| PU2 | a wait result of 2 accepted | 169 |
| PU3 | `Music_Finished` not cleared | 435 |
| PU4 | the buffer read before decoding | 3 |

A first run of the controls misread every fault as "not refused" (the
script matched `build exit 0`); the table is the re-run.

## 6. What the live check should listen for, and what nothing reaches

The frame hash, the oracle and the captures see none of this: sound is
heard or not. What the owner should listen for, ours against
`BOF3X_ORIGINAL=*` (or against a list of the 23 names), same machine:

1. **The logo / attract music** (track 141, the once-only `N` file) starts
   with the logos and plays once, not looping; the title music after it.
2. **Pressing Start on the title** (`Title_FadeMusic`, `Music_FadeOut(16)`):
   with DIV-0028 (the default) a 16-frame fade, about half a second;
   `BOF3X_ORIGINAL=MusicFadePerFrame` and all-original should both cut
   within a frame (D26). Listen three ways.
3. **An area change with new music**: the old track stops, the new one
   starts from its beginning - fading in over 8 frames with DIV-0028, at
   full volume at once without it (D26).
4. **A track looping** - stay in one area past the end of its music: the
   loop is seamless and the same as the original's (the decoder rewinds; a
   gap or a stutter at the loop point that the original does not have is
   ours).
5. **Sound effects**: the menu cursor and confirm, the message box's text
   sounds (control code `0x0A`), the attract's effects (ops `B0`..`B3`,
   `FB`..`FE`) - none missing, none doubled, the same pitch (the cue word's
   high half is the frequency).
6. **A looping effect that a scene stops** (a cue voice `0xFF`): it stops
   when the original stops it, and none keeps sounding after its scene.
7. **The game in the background**: the original freezes when not the
   foreground window ([`windowed-mode.md`](windowed-mode.md)) while the
   music ring keeps looping its last 0.84 s (no pump runs). Ours should do
   exactly the same - a difference here is ours.

Nothing but the fuzz reaches: real DirectSound (buffers lost and restored,
Lock splitting at the ring's end, notification timing), the real decoder's
frame sizes and end of stream, `Wave_FindData` on the shipped banks' waves,
`Sound_PlayEffect` banks outside 1..6 and cues past 23, `Music_LoadFile`
failing (D24, no missing file in the install), a decoder that fails to open
(D25), and the five eax results past the call sites. `SndBuf_Stop` has no
entry in the trace, so whether the attract reaches it is unmeasured.

## 7. For `analysis/calltrace/entries_logic.txt`

Owned ranges, `start size` as that file has them. Seventeen are there
already, every size right (checked 2026-09-22); six are not there -
`0x587C70`, `0x5A6AF0`, `0x5A6F30`, `0x5A6FB0`, `0x5A7050` and `0x5A7230` -
because the wall-clock rule dropped them (reachable from the pump). An owned
function is never armed,
whichever way `BOF3X_ORIGINAL` points (`call-trace.md` §7), so listing the
wall-clock ones costs nothing and gives their owned ranges; none of them
calls an armed function (their callees are unlisted decoder calls, imports,
`_ftol` and each other), so leaving them out would not break the hash
either. The list below is all 23; the six to add are the six named above.

```
00587740 119
00587A20 B8
00587AE0 60
00587B40 38
00587BA0 36
00587BE0 32
00587C70 51
005A69C0 1A
005A69E0 BC
005A6AA0 44
005A6AF0 BB
005A6BB0 75
005A6C30 23
005A6C90 29
005A6CC0 9F
005A6D60 81
005A6DF0 70
005A6E60 CC
005A6F30 79
005A6FB0 31
005A7050 29
005A70A0 99
005A7230 84
```

## 8. Not taken

The rest of the sound module, none of it reached by any trace: `0x587860`
(stop every channel), `0x587890` (a cue's volume, through `0x5A6C60`),
`Sound_PlayById` `0x587900`, `Sound_LoadStream` `0x587910` and
`Sound_StreamDone` `0x587A00` (the `SND` streams: `0x5A7140`, `0x5A71C0`,
`0x5A7200`), `0x587B80` / `0x587C30` (stops, through `0x5A6FF0`, byte for
byte `Music_Stop`), `0x587B90` (`0x5A7080`, Play the ring again) and
`0x587C20` (`0x5A7020`, is the ring playing). `Snd_LoadBank` and the
DirectSound set-up (`Snd_Device`'s writer, unread) likewise. They call
ours through the patched entries. (Since 2026-09-23 `Sound_LoadStream`,
`Sound_StreamDone` and `0x5A7020`, now `Music_IsPlaying`, are ours - group X
of the sixth round, [`save-menu.md`](save-menu.md) - and `Snd_LoadBank` group
S's.)

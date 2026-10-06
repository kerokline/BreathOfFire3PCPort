# The sound layer's last seven and its set-up (group PS)

**Status:** IN PROGRESS (2026-10-05) - 9 functions ours in
[`src/game/sound_rest.cpp`](../src/game/sound_rest.cpp), each read to its
last instruction, fuzzed against Capcom's at start-up
(`BOF3X_SHADOW=sound_rest`, 27,000 rounds, 0 mismatches) with 32 negative
controls, all refused by a count (section 6). Headless only: no route
enters eight of them, and the ninth, `Snd_Init`, runs once per run, before the first frame - the state
hash sees what it leaves. No divergence; latent defects of Capcom's
described in section 3, none fixed.

The platform round's step 2 ([`platform-layers-plan.md`](platform-layers-plan.md)
section 4), group PS: the sound row of
[`platform-read-pass.md`](platform-read-pass.md) section 2 - the seven
"held" starts [`sound.md`](sound.md) section 8 left - and the one live start
of its "Platform set-up" row, `0x5A6830`. With these the sound module is
ours whole; what stays Capcom's under it is the MP3 decoder (`Mp3_*`, the
plan's section 2.4) and the C runtime's `_ftol`.

## 1. The functions

Sizes are each body's extent to its last instruction by capstone. Two of
the seven turned out to be the targets of the other two (`0x5A6FF0`,
`0x5A7080`), and `0x587C20` (section 5) is taken as an eighth.

| PC | name | bytes | what |
|---|---|---|---|
| `0x587B80` | `Sound_StopMusic` | `0x5` | `jmp Music_Halt` |
| `0x587B90` | `Sound_ResumeAll` | `0x5` | `jmp Music_Resume` |
| `0x587C20` | `Sound_MusicPlaying` (new) | `0x5` | `jmp Music_IsPlaying` (ours, `save_menu.cpp`) |
| `0x587C30` | `Sound_PauseAll` | `0x34` | every live channel's voice stopped, the SND stream stopped and released, `jmp Music_Halt` |
| `0x5A6C60` | `SndBuf_SetVolume` (new) | `0x2F` | a voice's level 0..127 to DirectSound's hundredths of a decibel, `SetVolume` |
| `0x5A6FF0` | `Music_Halt` (new) | `0x29` | the music stopped if it plays - `Music_Stop` `0x5A7050` byte for byte |
| `0x5A7080` | `Music_Resume` (new) | `0x16` | the music played again, looping, from where it stopped |
| `0x5A7200` | `SndStream_IsPlaying` (new) | `0x2A` | whether the SND stream's buffer plays - `Music_IsPlaying` on the other buffer |
| `0x5A6830` | `Snd_Init` (new) | `0x145` | the DirectSound device and its primary buffer |

Data named: `Snd_Primary` `0x7DE3C0` (the primary buffer) and
`SndStream_Buffer` `0x7DE3C8` (the SND stream's, `battle_items_callees.h`'s
`kStreamBuffer`). `0x5ACBBC`, which the read pass filed under the MP3
decoder by its address, is **DirectSoundCreate's import thunk** (`jmp
[0x5C4014]`; the import directory names that slot DSOUND.dll ordinal 1) -
the linker put it after the decoder's code. It is `sound_rest_callees.h`'s
`kDirectSoundCreate`, not a decoder function.

**The Windows / COM calls and their stand-ins.** DirectSoundCreate (through
the thunk); `IDirectSound` `Release` (+8), `CreateSoundBuffer` (+0xC),
`SetCooperativeLevel` (+0x18); `IDirectSoundBuffer` `Release`, `GetStatus`
(+0x24), `Play` (+0x30), `SetFormat` (+0x38), `SetVolume` (+0x3C), `Stop`
(+0x48). In the fuzz each is a recorder (section 4). Every call is to
Capcom's own DirectSound objects, made by `Snd_Init` - not our backend's
(DIV-0031 replaced the display's objects, not these).

**The thunks.** `Sound_StopMusic`, `Sound_ResumeAll` and `Sound_MusicPlaying`
are five-byte `jmp`s: the caller's stack and ecx reach the target as they
were, and `Music_Halt` and `Music_IsPlaying` read that ecx (below). Ours are
naked `jmp *SoundRest_g.<target>` - a jump, not a call, through the callee
table the fuzz points at recorders. `BOF3X_ORIGINAL=Music_Halt` still reaches
Capcom's from our thunk: Inject writes its jmp over our `Music_Halt`.

**`Music_Halt` and `SndStream_IsPlaying` read their caller's ecx.** Each
starts `push ecx` and has `GetStatus` write its out into that slot; a
`GetStatus` that does not write it leaves the caller's ecx to be tested. As
`Music_IsPlaying` (round six), ours enter naked and pass ecx to a C body.
(`sound.cpp`'s `Music_Stop`, the same bytes, starts its slot at 0 instead;
not changed here.)

**`Snd_Init`, what the set-up sets up.** Only the device: already set,
nothing (0). `DirectSoundCreate(NULL, &Snd_Device, NULL)`, failing 100;
`SetCooperativeLevel(Game_Hwnd, 3 = DSSCL_EXCLUSIVE)`, failing the device
released and cleared, 101; `Snd_BufferDesc` zeroed and filled for a
`DSBCAPS_PRIMARYBUFFER` and `CreateSoundBuffer` into `Snd_Primary`, failing
102; `Snd_WaveFormat` = PCM, 2 channels, 22,050 Hz, 8 bits (44,100 bytes a
second, block 2) and the primary's `SetFormat`, failing both released and
cleared, 103; else 0. It is **sound only** - no display, no input - and it
makes **neither the voices, nor the music stream, nor the decoder**: the
voices' buffers come with each DAT's sound bank (`Snd_LoadBank`), the
music's ring and the MP3 decoder with each track (`Music_Start`,
`Music_OpenDecoder`), the SND stream's buffer with each stream
(`SndStream_Play`). Game_Init ignores the answer; a game without a device
plays silent (`SndBuf_FromWave`, `Music_Start` and `SndStream_Play` do
nothing without one). The brief's guess that it initialises the voice table,
the stream and the decoder's handle is answered no, by the disassembly.

## 2. What is kept as Capcom had it

- **Every re-read.** `Sound_PauseAll` reads each channel dword after the call
  before it, and the stream kind `0x6BDE40` after the walk; `Music_Halt`
  reads `Music_Buffer` again for `Stop`; `Snd_Init` reads `Snd_Device`
  again after every call and `Snd_Primary` after the device's `Release` on
  the 103 path, before clearing `Snd_Device`. Each is a control.
- **The answers in eax.** `Music_Halt` 0, `GetStatus`'s or `Stop`'s;
  `Music_Resume` 0 or `Play`'s; the thunks their target's (by being jumps);
  `Sound_PauseAll` `Music_Halt`'s, which the original's tail jump hands
  back - ours is defined under the assembler name of the `void` declaration
  with an `int` result, as `sound.cpp`'s five are; `Snd_Init` 0 / 100..103.
  `SndBuf_SetVolume` leaves its caller's eax for a null buffer, which C
  cannot; its one caller (`Sound_SetCueVolume`, ours) reads nothing.
- **Any non-zero is a failure** in `Snd_Init` (`test eax, eax`): `S_FALSE`
  too. The fuzz's DirectSound answers 1 sometimes.
- **The x87.** `SndBuf_SetVolume` is `fild`, two `fmul`s and an `fsub` on the
  image's floats (`0x5C464C` 1/127, `0x5C4648` 10000.0), truncated as the
  CRT's `_ftol` `0x5B9550` does (control word | 0x0C00, `fistp` to 64 bits,
  the low dword) - the same instructions, so the same rounding under any
  control word; the fuzz runs three.
- **`Sound_PauseAll`'s tail ecx.** The original's `jmp` hands `Music_Halt`
  the ecx its last callee left (or its own caller's, when it called none);
  ours hands it what our code leaves. Neither is a value anyone set; it is
  read only when `GetStatus` fails to write its out, which DirectSound does
  not do for a valid buffer and pointer. Not compared under `Sound_PauseAll`
  (the recorder logs it under the thunks).

## 3. Latent defects (described, not fixed)

Read from the code, not measured live; none is ledgered, and none changes
here.

1. **The primary buffer is set to 22,050 Hz, 8-bit stereo**, while the
   music is decoded at 44.1 kHz 16-bit (`Music_CreateBuffer`). On a
   DirectSound that honours the primary's format (hardware mixing, the
   DirectX 6 era), everything the game plays is mixed down to that. Whether
   a current Windows (DirectSound over the shared audio engine) honours it is
   not measured. A candidate for the owner's ear: the music through ours
   against a build whose `Snd_Init` asks for 44.1 kHz 16-bit.
2. **`Sound_ResumeAll` plays the music whether or not it played before the
   pause.** `Sound_PauseAll` stops the ring only if it plays; `Music_Resume`
   plays whatever buffer is there, looping. A track a stopping fade had
   stopped (`Sound_Tick` calls `Music_Stop`, not `Music_Release`: the buffer,
   the decoder and the pump stay) would come back after a pause and a
   resume, and the pump would feed it. Not seen by any route; a test for the
   owner, ours and `BOF3X_ORIGINAL=*` alike: silence the music by a scene,
   pause with F9, press a key.
3. **Nothing paused is resumed but the music.** `Sound_PauseAll` stops every
   channel's looping voice and stops and *releases* the SND stream's buffer;
   `Sound_ResumeAll` plays only the music. A looping effect sounding at the
   pause stays silent after it until a cue starts it again, and a stream cut
   off counts as done (`SndStream_IsPlaying` answers 0 with no buffer).
4. **`SndBuf_SetVolume` does not clamp.** A level above 127 asks for more
   than 0, below 0 for less than -10000; DirectSound refuses both
   (`DSERR_INVALIDPARAM`) and nothing looks.
5. **`Snd_Init`'s answer is dropped**, and `DSSCL_EXCLUSIVE` is what it asks
   (DirectX 8 and later treat it as `DSSCL_PRIORITY`).

## 4. The fuzz

`src/game/sound_rest_fuzz.cpp`, `BOF3X_SHADOW=sound_rest`, well under a
second: 3,000 rounds per function, round-robin, under the x87 control word
0x027F, and 0x037F / 0x007F one round in five. Each round randomises the
compared state, then shapes it; both sides run from the same bytes, called
with the same ecx (a small assembler shim) and two stack words; everything
is compared - `Sound_Channels` (`0x6BC8C8..0x6BC923`), the stream kind
`0x6BDE40`, the device block `0x7DE378..0x7DE3E3` (the description, the
format, `Snd_Device`, `Snd_Primary`, `Music_Buffer`, `SndStream_Buffer` and
the music's globals after them), the control word after, the eax where a
caller reads it, and the recorders' log.

- **Every call out is a recorder**, the jumps included. The copies' calls
  and jumps are re-aimed at offsets: the thunks' `jmp` at +0,
  `Sound_PauseAll`'s calls at +0xD (`SndBuf_Stop`) and +0x2A
  (`SndStream_Stop`) and its tail `jmp` at +0x2F, `Snd_Init`'s call of the
  thunk at +0x18; `SndBuf_SetVolume`'s `_ftol` at +0x22 is kept (plain x87).
  Ours reach the same recorders through `SoundRest_g`. The recorders for
  the three jump targets are assembler stubs that log the ecx they arrive
  with and the first stack word, so a thunk that touches either is caught.
- **DirectSound is faked**: three devices and six buffers with vtables of
  recorders for exactly the methods the originals call (any other slot is a
  `Fatal`), answering from a hash - success, `S_FALSE`,
  `DSERR_BUFFERLOST`, `E_FAIL`. `GetStatus` writes bit 0 set, bit 8 set and
  bit 0 clear, anything, or **nothing** (with a failure or a success), so
  the slot's starting value - the caller's ecx - is tested. The two creates
  sometimes write their out even when failing.
- **Loud stand-ins.** Every recorder may change what its caller reads after
  it: `Snd_Device`, `Snd_Primary` and `Music_Buffer` (only ever swapped for
  another fake: the originals dereference them unchecked), `SndStream_Buffer`
  (a fake or null), the stream kind, any channel.
- **Seeds.** ecx 0, 1, 0x100, 0xFE, 0x101, 0xFFFFFFFE, -1, anything; channels
  empty, a fake buffer, a stale word, all 23 empty; the stream kind 0, 1, 2,
  0x80000000, anything; each pointer null one round in three to five;
  `Snd_Init` with a device already there one round in five; levels 0..127
  and 0, 1, 63, 64, 126, 127, 128, -1, -127, 0x1000, `INT_MAX`, `INT_MIN`;
  window handles 0, -1, anything.

## 5. `0x587C20` is held, not original

The read pass (section 2) classed `0x587C20` original: "one caller, a body
of ours". Its callers are `0x495C27` and `0x495D26` - not inside
`GameMode_Field` `0x4959F0` (ours, `0x1BC` bytes) but in **Game_Mode 3's
steps `0x495BC0` and `0x495C50`** (the menu's open and close,
[`menu-screens.md`](menu-screens.md)), which `GameMode_Handlers[3]`
`0x495BB0` dispatches through the table `0x656A74`. Those three have no
symbol and no Inject: they are **Capcom's and run** whenever the field menu
opens. `tools/platform_reach.py` binned the two calls to `0x4959F0` because
`pc_funcs.json` gives that start a 2,139-byte extent that swallows them (and
the mode handlers `0x495E60`, `0x495E90`, `0x496230` after them), and none
of them is in `remaining_catalog.tsv`. Each step calls `0x587C20` only when
the word `0x7E0678` is `0x6E` (and then `Music_FadeOut(0x10)` when the music
does not play), which no route met - so it is held, not original, and taken
here: a `jmp` to ours, `Music_IsPlaying`, handed the caller's ecx.

**For the coordinator:** the mode handlers `0x495BB0` (with steps
`0x495BC0`, `0x495C50`), `0x495E60`, `0x495E90` and `0x496230` look like game
code still Capcom's that the round-fourteen count and the read pass both
missed ([`mode_states.md`](mode_states.md) section 7 named most of them
"no group's"). Not measured here beyond the symbol table and the extents.

## 6. Negative controls

One planted bug each, built, `BOF3X_SELFTEST_ONLY=1
BOF3X_SHADOW=sound_rest`, reverted (the scratchpad's `controls.py`, not
committed; each plant anchored on a unique string). Mismatching rounds of
the function's 3,000:

**32 controls, every one refused by a count.** I5 first refused by a fault only (the recorder for DirectSoundCreate left the device unwritten on `S_FALSE`, so ours dereferenced null); the recorder now writes it on `S_FALSE`, as a success does, and I5 was run again.

| | control | refused |
|---|---|---|
| S1 | `Sound_StopMusic`: jumps to Music_Resume | 3,000 |
| S2 | `Sound_StopMusic`: ecx cleared before the jump | 2,609 |
| R1 | `Sound_ResumeAll`: jumps to Music_Halt | 3,000 |
| R2 | `Sound_ResumeAll`: a call, not a jump (the stack one deeper) | 3,000 |
| M1 | `Sound_MusicPlaying`: ecx cleared | 2,615 |
| M2 | `Sound_MusicPlaying`: jumps to Music_Halt | 3,000 |
| P1 | `Sound_PauseAll`: the 23 dwords read up front | 1,178 |
| P2 | `Sound_PauseAll`: 22 channels | 1,302 |
| P3 | `Sound_PauseAll`: the stream stopped whatever the kind | 1,751 |
| P4 | `Sound_PauseAll`: eax 0, not Music_Halt's | 3,000 |
| P5 | `Sound_PauseAll`: the channels cleared | 2,476 |
| V1 | `SndBuf_SetVolume`: rounded, not truncated | 1,175 |
| V2 | `SndBuf_SetVolume`: no - 10000 | 2,371 |
| V3 | `SndBuf_SetVolume`: the level clamped to 0..127 | 406 |
| V4 | `SndBuf_SetVolume`: C++ float arithmetic | 193 |
| H1 | `Music_Halt`: the status starts 0 | 337 |
| H2 | `Music_Halt`: the buffer not read again for Stop | 66 |
| H3 | `Music_Halt`: eax 0 when it did not play | 335 |
| H4 | `Music_Halt`: tests bit 8 | 1,234 |
| U1 | `Music_Resume`: not looping | 2,239 |
| U2 | `Music_Resume`: eax 1 without a buffer | 761 |
| T1 | `SndStream_IsPlaying`: the status starts 0 | 344 |
| T2 | `SndStream_IsPlaying`: asks the music's buffer | 2,566 |
| T3 | `SndStream_IsPlaying`: the whole status tested, not bit 0 | 1,116 |
| I1 | `Snd_Init`: DSSCL_PRIORITY | 1,603 |
| I2 | `Snd_Init`: 44,100 Hz | 689 |
| I3 | `Snd_Init`: the primary read before the device's Release | 19 |
| I4 | `Snd_Init`: the device kept on a refused cooperation | 553 |
| I5 | `Snd_Init`: S_FALSE taken as success | 267 |
| I6 | `Snd_Init`: the description not zeroed | 1,050 |
| I7 | `Snd_Init`: a second call makes a second device | 602 |
| I8 | `Snd_Init`: the device not read again for CreateSoundBuffer | 52 |

## 7. The rebinding

Every raw reference in `src/game` to the nine is a name now: the callee
constants (`kMusicStop` / `kSoundJmp` / `kSound587B80` in the
`scena_sc2`, `sc3`, `sc6`, `sc7`, `sc9b`, `sc11`, `sc13` callees,
`scena_sx2`'s `kSndBufVolume`, `save_menu`'s `kVoiceIsPlaying`) are
`bof3::addr::<Name>`, the value unchanged - the bodies that call through
them call by the address, so `BOF3X_ORIGINAL=<Name>` still restores Capcom's
for them; the clones' call-site tables name them likewise. The stand-in
lists that named `Sound_StopMusic` / `Sound_ResumeAll` as Capcom's
(`*_THEIRS`, keyed by the macro's address) now name them as ours
(`*_OURS`: matched by the address, keyed by our function), since a body that
calls them by name now calls ours: `area_harness.cpp`,
`scenario_harness.cpp`, `area_w1d/w2f/w3d/w4e/w4f_fuzz.cpp`,
`field_e2_fuzz.cpp`, `scena_sc11_fuzz.cpp`. `Game_WndProc` calls
`Sound_PauseAll` / `Sound_ResumeAll` by name, and now reaches ours.
`Game_Init` (Capcom's at this branch; the shell group's tonight) calls
`0x5A6830` by its rel32 - the name is `Snd_Init`.

## 8. Live coverage

| function | reached by |
|---|---|
| `Snd_Init` | every route, once, from `Game_Init` `0x4FD149` (the read pass's traces) - the state hash covers what it writes, the device block |
| the other eight | no recorded route: the routes never pause (no deactivation, no F9), never stop the music for a stream, never set a cue's volume, never play a stream of kind above 0, and never met `0x7E0678 == 0x6E` in the menu |

What the owner could listen for, ours against `BOF3X_ORIGINAL=*`: sound at
all (the set-up); F9 and a key, and the window losing and regaining focus
with `background=0` (the pause and resume, and section 3's items 2 and 3); a
scene that stops the music for a stream and resumes it.

## 9. For `analysis/calltrace/entries_logic.txt`

Owned ranges, `start size`. Seven are listed already, every size right
(checked 2026-10-05 against the main checkout's file); the two to add are
`0x5A6FF0` and `0x5A7080`, which no list had (reached only by jumps).

```
00587B80 5
00587B90 5
00587C20 5
00587C30 34
005A6830 145
005A6C60 2F
005A6FF0 29
005A7080 16
005A7200 2A
```

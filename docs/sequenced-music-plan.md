# Music from the disc: the sequencer, the SPU model, and the cache format

**Status:** PROPOSAL (2026-10-08, a cloud session on
`audio/sequence-from-disc`) - [`unified-data-plan.md`](unified-data-plan.md)
step 9, made concrete. Nothing here is built yet except what section 9 lists;
the owner's calls are in section 8.

The owner's stance of 2026-10-08: **the disc's music is the default wherever
a disc is a source; the PC's MP3s only when nothing else is there**
([`unified-data-plan.md`](unified-data-plan.md) section 6, on the loop
measurement's numbers: 80 of 156 looping MP3s hold less than one loop body).
This file says how the disc's music gets into the engine, in the most
PlayStation-accurate way the evidence allows, with the MP3 path kept whole
as the fallback.

## 0. What the disc holds, and what plays it (established)

- **The music is sequenced.** 81 `BGM*.EMI` (and the 38 battle / boss / demo
  bundles) each carry a VAB header (type 6, `pBAV`), a VAB body (type 7, the
  ADPCM samples) and a SEP (type 10, `pQES` version 0: four sub-songs of
  MIDI-style events). The song table at `0x80182830` maps song N to (file,
  sub-song) - [`bgm-comparison.md`](bgm-comparison.md) 6.2; PC file N is PSX
  song N (6.3).
- **The player is Sony's own.** The boot EXE's sound code is Psy-Q 3.7's
  `LIBSND` and `LIBSPU`, matched object by object by signature in the sibling
  (`BreathOfFire3Recomp/symbols.toml`, `tools/psyq_sigs.py`: `SsSeqCalledTbyT`
  `0x8016CEC0`, `SsSetTickMode` `0x8016E344`, `SsUtSetReverbType` `0x8016FE8C`,
  `SsSetMVol` `0x8016CA38`, `SsSetRVol` `0x8016CA88`, `_SsVmKeyOn`,
  `_SsVmSetVol`, `note2pitch`, `vmNoiseOn` ... 166 functions). The game's own
  layer above it is thin: `Music_Play` `0x80162610` starts sub-song `sub` of
  the resident SEP (`0x8015DCF0`, `0x8015DE8C`), loading by `0x801625AC`;
  the effects go through `SE_Play` `0x8015E908` to `SsUtKeyOnV` on voices
  16..23, the sequencer keeping 0..15 (the sibling's `SOUND_CUES.md`).
- **The loop is in the data**: controller 99 = 20 with controller 6 = 127 at
  the loop start, 99 = 30 at the end - libsnd's own convention, which
  `_SsContNrpn2` / `_SsContDataEntry` implement. 156 of 165 songs loop; the
  9 that do not are the PC's 9 `N` files.
- **The timing is libsnd's, not the MIDI's.** The renders' loop bodies are
  0.981 (song 0) and 1.010 (song 153) times the sequence's ticks x tempo
  (`bgm-comparison.md` 7.2): the sequencer advances in VSync ticks with
  integer arithmetic, so a loop table cannot be computed from the SEQ alone.
  The same arithmetic, reproduced, gives the PSX's timing exactly.
- **The oracle exists.** Mednafen renders of the game's own disc playing each
  song on the title screen (`tools/bgm/patch_disc.py`, `mrun.sh`): three in
  section 8 of `bgm-comparison.md`, 153 from the 2026-10-08 run. Their SPU
  noise voices are a fresh LFSR realisation each pass, so a render compares
  by waveform in clean windows and by onset envelope elsewhere (11.1 there).
- **The MP3 path is ours end to end** (`src/game/sound.cpp`: `Music_Play`,
  `Music_Start`, `Music_Decode`, `Music_Pump`, the loop table of DIV-0081
  behind `LoopDecode`). The seam is one function: `Music_Decode(dst, size)`
  fills `size` bytes of 44.1 kHz 16-bit stereo PCM into the ring's free half
  from whatever source is playing.

## 1. The two ways, and the recommendation

| | A. Render offline, play PCM | B. Synthesise at run time |
|---|---|---|
| What the importer writes | each song rendered once by an SPU model of ours into PCM (intro + one body + loop points) | the sequence and its bank in a container of ours |
| What the engine runs | a PCM stream behind `Music_Decode` with a loop point | a sequencer and an SPU voice model behind `Music_Decode` |
| Loops | sample-exact at the render's seam, but a render cannot carry the SPU's *state* across the loop (the ADPCM filter history, the envelopes still decaying, the reverb tail): every pass is the first pass | as the PSX: the loop end jumps the sequencer, the voices and reverb carry over |
| Reverb per scene, fades, the master volume | baked | live, as the game sets them |
| Effects mixing with the music (H5: on the PSX they share 24 voices and one reverb) | never | possible later, through the same model (section 7) |
| Size in the cache | ~10 MB a minute of PCM; 135 min is 1.4 GB (ADPCM of our own: a quarter) | the disc's own: a few MB in all |
| Engine code | a PCM reader (small) + the SPU model in the importer | the SPU model + a sequencer in the engine (the same model) |
| Needs an SPU model of ours either way | **yes** - a GPL emulator's output cannot ship, and a render is audio derived from the game's data (rule 1): both need the player to render from his own disc | yes |

**Recommendation: B.** Both need the SPU model, so the extra cost of B is
the sequencer - about 600 lines of libsnd's semantics, read from the boot
EXE - and what it buys is the thing the owner asked for: the PlayStation's
own behaviour, including the parts a render cannot hold (the state across a
loop, the reverb the scene sets, the fade curve), and the door to the
effects. A is the fallback if B's sequencer cannot be made to match the
renders; its PCM path would be a day's work on top of the model.

## 2. The fidelity contract

What "PSX-accurate" means here, in order of what is measurable:

1. **The SPU voice model is the hardware's, from the public register-level
   description** (nocash's `psx-spx`, the SPU chapter): ADPCM decode with the
   four filter pairs and clamping, the 2-sample history kept across the loop
   jump; 4-tap Gaussian interpolation with the hardware's 512-entry table;
   the ADSR with its step / shift rate table, linear and exponential modes
   and the pseudo-exponential decrease; the 16-bit volume arithmetic; the
   noise generator (clock and shift); the reverb algorithm at half rate with
   the register set `SpuSetReverbModeParam` writes; the main volume and the
   final clamp. No tolerance is designed in: where our output differs from
   the render in a clean window, one side is wrong and it is found.
2. **The sequencer is libsnd 3.7's**, read from `SLPS_009.90`: the tick (what
   `SsSetTickMode` the game passes, how `SsSeqCalledTbyT` turns resolution
   and tempo into ticks per VSync and what it does with the remainder), the
   event reader (`_SsSeqPlay`, `_SsReadDeltaValue`, running status, the meta
   events), note on / off through `_SsVmKeyOn` / `_SsVmKeyOff` (voice choice
   among 0..15, priority, the volume from program x tone x channel x
   expression x velocity and the pan, the pitch from `note2pitch` with the
   tone's centre / shift / fine, the ADSR words handed to the SPU as they
   are, the reverb bit from the tone's mode), the controllers the songs use
   (`inventory.json` counts them: volume 7, pan 10, expression 11, damper
   64, NRPN 98 / 99 with data entry 6, pitch bend and its range), program
   change, tempo, the loop. Anything the songs never use is left out and
   said so (`Fatal` on an event we did not implement, never a silent skip -
   hard rule 4).
3. **The game's set-up** is read, not assumed: `SsSetMVol`, `SsSetRVol`,
   `SsUtSetReverbType` / `Depth` and every place the game changes them
   (per-scene reverb is `bgm-comparison.md` section 5's open item), the
   reserved voices, the fade `0x8015DE8C` implements (linear in libsnd's
   volume, where the PC's fade is linear in decibels through DirectSound's
   `SetVolume` - section 6).
4. **The clock.** The PSX's SPU runs at 44,100 Hz and the sequencer at the
   VSync rate; NTSC's frame is 263 lines of 3,413 GPU cycles at 53.693 MHz,
   so a tick is 44,100 x (263 x 3,413) / 53,693,175 = 737.2 samples, not
   735. The renders settle it (the 0.981 and 1.010 ratios are the arithmetic
   of item 2 at this rate; the fit says which).

**Acceptance, per song, against its Mednafen render** (the method of
`bgm-comparison.md` 11.1, reused): aligned on the first note, in 0.25 s
windows, the waveform correlation is 0.99 or better in every window without
a noise voice and the onset envelope matches throughout; the loop period
equals the render's to the sample. Reported for all 165 songs (and the
battle bundles' sub 1) in a table, with every miss named. **The owner's ear
on top**: the listening set of section 9 there, with a third file per pair -
ours.

**Testing without the owner's machine.** The SPU model and the sequencer are
plain C++ with no Windows in them, built twice: into `bof3x.dll` behind
`Music_Decode`, and into a host tool (`tools/bgm/synth_render`, a CMake
target that builds with the system compiler) that renders a song from the
cache to a WAV. `tools/bgm/synth_check.py` runs it over every song and
scores the result against the renders. Mednafen (1.29 in Ubuntu's archive,
run under `xvfb` with `-soundrecord`, as a tool - nothing of it enters the
repo) can make more renders in the cloud from the owner's disc image and
BIOS, so the comparison does not wait on his machine; his 1.32.1 renders
stay the reference where both exist.

## 3. The cache format (the importer's output)

`ASSET_SOURCES.md` section 3's rule holds: the engine reads a format of ours,
never the disc's. The importer (`tools/seq.py`, hooked into `importer.py`
like `vag.py` and `xa.py`) writes:

```
base/bgm/NNN.DAT          one per song (0..165, 21 included, 166 absent): header, the bank's name,
                          the sub-song's events as a flat table (absolute tick, status, data),
                          with the loop start / end and the song's resolution and initial tempo
base/bgm/bank/NAME.DAT    one per EMI whose SEP a song uses (the 81 BGM files and BGMBAT00 for 165):
                          the programs and tones with the fields libsnd reads, the samples as the
                          SPU's ADPCM blocks with their flags, and each sample's start offset
```

- **Events translated, order kept.** Running status resolved, delta times
  made absolute; the event order within a tick is the file's, since libsnd
  processes them in that order and a note-on before a program change in the
  same tick sounds differently from the reverse.
- **Samples stay ADPCM.** The SPU decodes a block from its last two outputs,
  and keeps them across the loop jump: a sample looped mid-way does not
  decode to the same PCM on its second pass as on its first. Pre-decoding
  would be exact only for filter-0 blocks. 16 bytes to 28 samples is nothing
  at run time (`vag.decode` is the same routine, in Python).
- **Numbering is the PC's** (song N, `165` the battle bundles' sub 1, as the
  exe's `BGM\%03d` names them) so the engine's `Music_Play(track)` needs no
  table; song 21 gets a file the PC never had (section 6).
- **The 38 dropped bundles** (`BOSS*`, `BATTLE*`, `DEMO`) are copies of BGM
  files' SEPs but for sub 1 (`165`) - nothing else to import from them.
- **The PSP's `pPMS` / `PPHD`** are not read; a PSP-only install takes its
  music from a PSX source in the same install, as the plan's section 10
  recommends. The PSP's sample bodies are the PSX's (`bgm-comparison.md`
  8.3), so if a PSP-only music path is ever wanted, only the two readers are
  missing.
- Every file hashed into the manifest with its source (`psx-jp:seq:BGM019.EMI#2`).

The format is versioned in its header; `seq.py dump` prints a file back as
events for a diff against `inventory.py`'s parse of the disc (the
round-trip check, run in `importer.py check` with no game data: a synthetic
SEP).

## 4. The engine seam

`Music_Play(track, frames)` is ours and calls `Music_LoadFile(track)` then
`Music_Start(file, size, loops)`; `Music_Decode` fills the ring. The change:

1. **`Music_LoadFile`** looks for `<cache>/base/bgm/NNN.DAT` first (the
   cache root the loader's second prefix already knows - DIV-0086's seam,
   the same one `sound-import.md` section 7 wants for `base/snd/`), and
   only then `BGM\NNN.DAT` / `BGM\NNNN.DAT`. A cache file sets a source kind
   beside `Music_FileLoops` (once-only is the sequence's own property: no
   loop markers).
2. **`Music_Start`** opens the source: the MP3 decoder as today, or the
   sequencer on the song and its bank (the bank loaded once and kept while
   songs share it, as the PSX keeps the SEP resident).
3. **`Music_Decode`** asks the source for `size` bytes: `LoopDecode` / the
   original's loop for MP3, `Synth_Render` for a sequence - which advances
   the sequencer tick by tick (737.2 samples each, the fraction carried) and
   mixes the voices and the reverb into the buffer.
4. **`Music_Stop` / `Release`** tear the source down; `Music_SetVolume` stays
   the DirectSound volume for MP3 and becomes the sequence volume for the
   synth (section 6).
5. The pump, the notifications, the ring, the fades' state machine and
   DIV-0028 are untouched: the game sees `Music_Finished` and `Music_Track`
   exactly as before.

**The switch**: `BOF3X_MUSIC=seq` (the default when the cache has the song),
`mp3` (the PC's file even when the cache has the song), anything else fatal;
`music=` in the ini beside `opt=`. A song the cache lacks plays the MP3
whatever the switch, so a PC-only install is unchanged. Log one line per
start saying which source played.

**Self-tests** (`BOF3X_SHADOW=sound`, extended): the synth rendering a
synthetic song (a bank of one triangle-wave sample, four notes, a loop) to
a buffer in `0x12000`-byte calls and in odd sizes, every sample equal to a
single-call render - the tick carry and the buffer seams; the event reader
over a synthetic SEP against `inventory.py`'s parse. The real songs are the
host tool's business (section 2).

## 5. The ledger entry (to write when it lands)

- **Original**: the port plays a 128 kbit/s MP3 of a 2001 render of each
  song, looped file-end to file-start (DIV-0081 moved the loop for 48 of
  them inside the file), through DirectSound's buffer volume.
- **New**: with a disc among the importer's sources, the song is synthesised
  from the disc's sequence and bank by a model of the PlayStation's SPU and
  Sony's sequencer: the loop is the sequence's, the reverb the game's, the
  top octave present (the MP3s roll off above ~11 kHz,
  `bgm-comparison.md` 8.2). The MP3 path is untouched and is what plays when
  the cache lacks the song.
- **Rationale**: the owner's stance, quoted at the head; the measurement
  behind it.
- **Verification**: section 2's table, the self-tests, the owner's ear.
- **Reversibility**: `BOF3X_MUSIC=mp3`; no cache, no change.

Two further entries if the owner takes them: the fade curve (section 6) and
song 21.

## 6. Decisions the reading will raise (for the owner, section 8 collects them)

- **The fade.** The PC fades the DirectSound buffer's volume, which is
  logarithmic: `Music_SetVolume` maps 0..127 onto -100..0 dB linearly, so
  the first half of a fade-out is nearly inaudible as a change and the last
  tenth is a plunge. The PSX fades `SsSeqSetVol`, linear in amplitude. The
  synth can take either: the game's 0..127 as libsnd's volume (authentic),
  or converted to the PC's curve (consistent with the MP3 path's fades).
  Recommended: authentic, as a DIV with the MP3 path left as it is.
- **Song 21** has a sequence and no MP3; `Music_Play(21)` on the PC opens
  neither name and plays the previous track looping (D24). With the cache it
  plays. Whether the game ever asks for 21 is unmeasured; if it does, the
  cache restores a song the port lost - record it either way.
- **`166`** has no sequence (6.3): it stays MP3-only. Its source is still
  unknown; if the game asks for it with a disc-only install, silence, and
  the log says so.
- **Per-scene reverb.** If the game changes the reverb type or depth outside
  the title (the reading will say), the renders - all on the title - are the
  oracle for one setting only; the other settings are checked against the
  SPU model's own reverb code, which the title setting has proved.
- **The 38 bundles' sub 1 under the fights.** On the PSX, the battle fanfare
  (`165`) is sub 1 of the *fight's own* bundle (`BOSSnn` / `BATTLE*`), whose
  bank is the fight's; the PC plays one file. The bundles' SEPs are copies,
  so one import suffices unless their banks differ - checked by hash in the
  import.

## 7. What it opens, not in this step

- **The effects through the same model** (step 9b): the PC's banks flatten
  each tone to a WAV played flat - no ADSR, no pan, no priority, no reverb
  (`sound-import.md` 2.3); the port's converter wraps where the SPU clamps
  (9 clicks). With the SPU model in the engine, `Sound_PlayEffect` could key
  the disc's tones through `SsUtKeyOnV`'s semantics on voices 16..23 and
  share the reverb with the music - H5, live mixing, restored. A separate
  DIV; it needs `SE_Play`'s cue set-up read (the sibling's `SOUND_CUES.md`
  has most of it).
- **The 11 jingles** (`sound-import.md` 4) are XA, not sequences; the model
  does not help them.
- **The decoder swap** (`platform-layers-plan.md` 2.4) matters less: a player
  with a disc never runs the MP3 decoder; the cutover still needs one for
  the PC-only install.

## 8. The owner's calls

1. **A or B** (section 1). Recommended B.
2. **The fade curve** (section 6). Recommended authentic, ledgered.
3. **Song 21**: play it from the cache when asked (recommended), or keep the
   port's silence as a per-build quirk.
4. **Where the cache root comes from at run time**: DIV-0086's `BOF3X_OPT`
   names an `opt/` layer, not the cache; the music (and `base/snd/`) want
   the cache's `base/`. Proposed: `BOF3X_CACHE=<dir>` / `cache=` in the ini,
   the one root for `base/`, `loc/` and `opt/`, with `install` still the
   bridge for `DAT/`.
5. **Acceptance threshold** (section 2): 0.99 in clean windows is the
   proposal; the owner may want to hear a 0.97 before it is called a miss.

## 9. The work, in groups

| Group | What | Needs | Checked by |
|---|---|---|---|
| SPU | `src/audio/spu.{h,cpp}`: voices (ADPCM, Gaussian, ADSR, volume, noise), reverb, mix; no Windows, no game addresses; a host build | the public SPU description only; **can start now** | unit tests on synthetic samples (the ADSR rate table against the documented timings, the decoder against `vag.decode`'s integer form, a looped sample's second pass against a by-hand decode, the reverb's impulse response's structure); then the renders |
| SEQ | the reading: `SLPS_009.90`'s libsnd and the game's wrappers, into `docs/libsnd-reading.md` with every constant and formula cited by address; then `src/audio/seq.{h,cpp}`, the sequencer | the JP disc (the boot EXE is on it) | the renders, through the host tool |
| IMP | `tools/seq.py` (VAB and SEP parsed, the containers written, `dump`), the `importer.py` hook, the manifest rows, `check`'s synthetic round trip | the JP disc | `dump` against `inventory.py`; `importer.py check` |
| HOST | `tools/bgm/synth_render` (CMake, system compiler) and `synth_check.py` (align, score, table) | SPU + SEQ + IMP | the table over 165 songs |
| ENG | the seam (section 4), the switch, the self-tests, the ledger entry | all of the above | the `sound` and `'*'` self-tests (the owner's machine); the owner's ear |

SPU starts first; SEQ and IMP start when the disc is here; HOST joins them;
ENG last. Each group works in its own directory and files; `sound.cpp` is
ENG's alone.

## 10. What the session needs uploaded (scratch only, rule 1)

| Upload | For | Size |
|---|---|---|
| The JP PSX disc, `.cue` + `.bin` (zipped) | the 81 `BGM*.EMI` + `BGMBAT00.EMI`, `SLPS_009.90` (libsnd and the game's wrappers), the song table; Mednafen renders in the cloud | ~700 MB |
| `SCPH5500.BIN` (the sibling's `mednafen/firmware/`) | running Mednafen here for more renders and for the reverb / tick experiments (a patched disc, as `patch_disc.py` makes) | 512 KB |
| `analysis/bgm/renders/` for songs 000, 141 (the unpatched boot), 153, 003, 007, 011, 017, 025, 034, 089 - as FLAC (`ffmpeg -i X.wav -c:a flac X.flac`, about half) | the reference renders from the owner's 1.32.1: the same files the loop measurement was read against | ~300 MB |
| `analysis/bgm/*.json` (`inventory.json`, `loops.json`, `mp3_scan.json`, `compare_*.json`, `loop_proof.json`) | the pairing, the loop rows, the alignment numbers - small, and they save re-deriving | a few MB |

Not needed: the PC's `BGM/`, `DAT/`, `SND/`, `BOF3.exe` (the music seam is
ours; the fallback path is unchanged and already self-tested), the other
discs (every PSX disc's music bytes are JP's, `bgm-comparison.md` 8.3), the
PSPs.

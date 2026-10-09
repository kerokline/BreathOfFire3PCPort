# The engine seam: the cache's songs through the sequencer (DIV-0087)

**Status:** BUILT, NOT RUN (2026-10-08, group ENG of
[`sequenced-music-plan.md`](sequenced-music-plan.md) section 9). Compiled with
GCC 13's `i686-w64-mingw32-g++ -fsyntax-only -Wall -Wextra`. Not yet built
with llvm-mingw, run, or listened to (section 6).

## 1. What it does

A BGM track that the importer's cache holds, `<cache>\base\bgm\NNN.DAT` with
its bank `base\bgm\bank\NAME.DAT` ([`seq-format.md`](seq-format.md)), is
synthesised by `psx::MusicSynth` (`src/audio/`,
[`libsnd-reading.md`](libsnd-reading.md) section 8) and not decoded from the
PC's MP3. The MP3 path stays byte for byte as it was. It is what plays for
every track the cache lacks, for every track under `BOF3X_MUSIC=mp3`, and
for everything when there is no cache. The ledger entry is DIV-0087 in
[`DIVERGENCE.md`](DIVERGENCE.md).

## 2. The seam (`src/game/sound.cpp`, one guarded line or block each)

| Function | For a cache song |
|---|---|
| `Music_LoadFile` `0x587A20` | `music_seq::LoadFile(track)` first. When armed and `base\bgm\NNN.DAT` exists, it reads the song through the sound layer's own callees (`g.file_open` / `file_size` / `file_read` / `file_close`, `g.malloc` / `g.free`, the old `Music_File` freed as the original frees it) into a fresh `Music_File`. It also sets `Music_FileSize`, `Music_FileLoops` (the song's loop flag) and `Music_LoadedTrack`, and reads and parses the bank unless it is the one already parsed. The MP3 names are then never formatted. On the MP3 path, `music_seq::Forget(Music_File)` runs before the old file is freed. |
| `Music_Start` `0x5A6CC0` | The release and the copy into `Music_Data` happen as for an MP3, so the release gate `if (Music_Data)` still works. If `music_seq::IsSong(file, size)` is true, `Music_Decoder = nullptr`, `LoopBegin(-1)` (no measured-loop row can match) and `music_seq::Begin()` run. Begin puts the bank into the synth unless it already holds it, sets mono, calls `Play(song, 1, 1)` then `SetVolume(127, 127)`, marks the stream the synth's, and logs one line. The buffer, notifications and play then follow as before. |
| `Music_Decode` `0x5A6F30` | `if (music_seq::Active())` comes before the decoder test: `music_seq::Decode` renders `size / 4` frames in pieces of 1,024. Once `Ended()` holds after a piece (a once-only song's end of track has keyed off and every voice has released), the rest is zeros and `Music_Finished = 1`, as at an MP3's end of stream. |
| `Music_SetVolume` `0x5A6FB0` | After the `Music_Buffer` test, the synth's stream sets the buffer to `music_seq::Level(volume)`, computed in plain C++. The MP3 case keeps the x87 truncation path untouched (the fuzz compares it). |
| `Music_Stop` `0x5A7050` | `music_seq::Stop()` (the synth's `SsSepStop`) first, then as before. |
| `Music_Release` `0x5A70A0` | At its end, `music_seq::Release()`: the synth stops and the stream is no longer the synth's. The bank stays. |
| `Music_Pump` `0x5A7230` | The gate becomes `(!Music_Decoder && !music_seq::Active()) || !Music_Buffer`, the same as before for every MP3. |

**The source state** lives in `music_seq.cpp`. `song_file` / `song_size`
say "Music_File holds a cache song". That is the only thing `Music_Start`
looks at, so `Sound_LoadStream`'s streams (another buffer, `Music_Start(...,
0)`) and every MP3 never match it. The same holds for the measured-loop
table, which is skipped for a cache song. `active` says "the stream
`Music_Start` last began is the synth's", and only `Music_Decode`,
`Music_SetVolume`, `Music_Stop` and `Music_Pump` read it. Until `Arm()` sets
`armed` neither can be set, so the start-up fuzz (`sound_fuzz.cpp`) and every
other self-test run the MP3 path exactly. When neither MP3 name opens,
`Music_File` keeps the previous song, cache or MP3, as the original does
(D24).

**The volume (why the buffer, not the synth).** The ring renders 0.4 to
0.8 s ahead of the play cursor. A volume given to the synth would be heard
that late and in 0.42 s steps: about 0.84 s near silence at every song start,
and a stopping fade cut by its own `Music_Stop`. So the synth plays at 127
and the fade moves the buffer, by libsnd's law and not the PC's. The volume
is truncated, kept within 1..127, and gives
`round(4000 * log10(v / 127))` hundredths of a dB (amplitude `(v/127)^2`,
`libsnd-reading.md` 3.6). That is linear in volume units and quadratic in
amplitude, the PSX crescendo's shape, and acts at once. The only thing lost
is the per-voice register rounding of each step. The level difference stays:
the PC fades to 127, where the PSX title ends its crescendo at 97 (5.1).

**Mono.** Before each render, the synth's `SetMono` takes the options
screen's Sound byte `0x903A59` (row 3, Stereo 0 / Mono 1). The PC's driver
ignores this byte; the PSX calls `SsSetMono` from the same row.

## 3. The switch and the keys

| Key | Values | Read |
|---|---|---|
| `BOF3X_CACHE` / ini `cache=` | Unset: no cache. A directory: the cache root (a trailing `\` is dropped). | `music_seq::Arm()`. Fatal if it is not a directory or is longer than 42 characters (the file layer's 0x50-byte retry buffer in `File_Open`, kept from the original). A directory without `base\bgm` gives one log line and no cache. |
| `BOF3X_MUSIC` / ini `music=` | Unset or `seq`: the cache's song where it has one. `mp3`: the PC's file always. | `Arm()`. Anything else is fatal. |

The launcher sets each variable from the ini only when it is not already set
in the environment, as it does for `opt=` (`ConfigApplyEnvironment`). The
`Config` struct gained `cache` and `music` (`src/launcher/config.h`). There
is no dialog box. `Arm()` runs in `InjectAll` after `music_loops::Arm()`,
after every module's self-test, and logs one line. Each cache song started
logs one line, and each track the armed seam cannot find in the cache logs
one line as it falls back.

## 4. The self-test (`BOF3X_SHADOW=sound`)

`music_seq::SelfTest()` runs from `FadePerFrame_Inject` after DIV-0028's and
the measured loops' self-tests. It uses no game data: the bank (one program,
one tone, a looping triangle sample) and two songs (number 7 looping, number
8 once, four notes over 96 ticks at resolution 48 and 120 bpm) are built
byte by byte to `seq-format.md`. The file layer, `sprintf`, the heap, the
decoder opener and `Music_CreateBuffer` (which decodes its first half
through `g.music_decode`) are stood in for, and so is a DirectSound buffer
that has only `SetVolume`. The test checks:

1. Song 7 through `Music_LoadFile`, `Music_Start` and five 0x12000-byte
   `Music_Decode` calls, with no decoder opened. Every sample of the six
   halves must equal one `MusicSynth::Render` of the same length, and must
   not be silent.
2. `Music_SetVolume(0, 63.9, 127, 300)` sets the buffer to -8415, -1218, 0
   and 0.
3. Song 8 on the same bank, which is not read again. Its samples must match
   the Render up to the piece in which `Ended()` first holds, then zeros,
   with `Music_Finished` set. (Host run: the end falls at sample 44,032.)
4. Track 9 (not in the cache) and `BOF3X_MUSIC=mp3` must try `BGM\NNN.DAT`
   and `BGM\NNNN.DAT`.

Everything it touches is put back. The synth keeps the test's bank, but the
bank-name record is cleared so the game's first song loads its own.

## 5. Build

`CMakeLists.txt` adds `src/game/music_seq.cpp`, `src/audio/spu.cpp`,
`seq.cpp` and `song.cpp` to `bof3x`. `src/audio` is C++17-clean and builds
under the project's C++20.

## 6. Owed on the owner's machine

- ~~`cmake --preset i686 && cmake --build build` with llvm-mingw~~ - done
  2026-10-09, no warnings.
- ~~`BOF3X_SHADOW=sound` and `BOF3X_SHADOW='*'`~~ - done 2026-10-09, narrow and
  wide, 0 mismatches. The first `sound` run failed the once-only case: the
  self-test's oracle rendered song 8 on a fresh `MusicSynth`, while the game's
  synth plays it over song 7's release tails and reverb (the design: the SPU's
  state carries across songs as on the PlayStation), so the samples differed
  from frame 0 and `Ended()` held 1,024 frames later (45,056 against 44,032).
  The oracle is now one synth across both songs, as `g_s.synth` is; the engine
  did not change.
- The host suite (`tools/bgm/host`) ran in the cloud only: `spu_tests` and
  `seq_tests` check the aborts with `fork`, so they do not build on Windows
  (IDEAS I35).
- A listen: `importer.py` to build the cache, `BOF3X_CACHE=<cache>`, then the
  title and a field, a fight, a once-only track (one of the nine `N` songs),
  a fade-out, and a `Sound_LoadStream` jingle (the inn) between two cache
  songs. Then `BOF3X_MUSIC=mp3` for the A/B.
- The owner's calls: the level (127 against the PSX's 97 at the title),
  whether 42 characters is enough for the cache root (lifting it means reading
  the cache through Win32 rather than the game's file layer), and song 21
  (plan section 6).

## 7. For the other files

- **STATUS.md:** "DIV-0087 (2026-10-08): the cache's songs
  (`base\bgm\NNN.DAT`) play through `psx::MusicSynth` at the music seam
  (`music_seq.cpp`; `BOF3X_CACHE` / `cache=`, `BOF3X_MUSIC` / `music=`). The
  MP3 path is unchanged and is the fallback. Fades move the buffer by
  libsnd's square law. Compiled with GCC syntax checks only; not built with
  llvm-mingw, not run, not heard. [`music-seq-engine.md`](music-seq-engine.md)."
- **HANDOFF.md:** "Group ENG's seam (DIV-0087) needs the owner's llvm-mingw
  build, `BOF3X_SHADOW=sound` / `'*'`, and a listen with `BOF3X_CACHE` set
  (title, field, fight, a once-only `N` song, a fade-out, an inn jingle
  between cache songs, A/B with `BOF3X_MUSIC=mp3`). Owner's calls: level
  127 vs the PSX's 97; the 42-character cache-root limit set by
  `File_Open`'s 0x50-byte retry buffer. `music_seq::Arm` is wired in
  `inject_all.cpp` after `music_loops::Arm`."

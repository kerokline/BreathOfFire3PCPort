# The WAV dump: the game's audio mix to a file, and the ear backlog as WAV pairs

**Status:** MEASURED (2026-10-13, the away-day's agent WAV, branch `away/1013-wav`): the dump built and
checked on four recipes (two runs of `combat.txt` equal sample for sample); the listen set for the
random-encounter resume, the loop rows and the inn's jingle rendered under the main checkout's
`analysis/listen-1013/`. **Not heard by anyone** - the WAVs are for the owner's ear.

`BOF3X_WAV_DIR=<dir>` writes what the game hands DirectSound - music and effects summed - to
`<dir>/<n>_<label>.wav`, one file a run. It is the audio counterpart of `BOF3X_SHOT_DIR`
([`input-script.md`](input-script.md) 3): instrumentation, off unless set. `tools/wav_diff.py` says
where two such files differ, second by second, so a listen goes only there.

## 1. Where the audio reaches the device (the tap point)

There is no buffer of the final mix inside the process. Read from our own sources, which are byte-for-byte
reimplementations of Capcom's ([`sound.md`](sound.md) 1, [`sound-rest.md`](sound-rest.md)):

| What | Who makes the buffer | DirectSound buffer |
|---|---|---|
| each sound-effect voice | `SndBuf_CreateFromWave` `0x5A69E0` at a bank's load | a static secondary buffer per voice, the WAVE's own format, flags `0x180E2`; `SndBuf_Play` `0x5A6BB0` plays it from 0 at the cue's frequency |
| the music (an MP3 track, or DIV-0087's sequenced song) | `Music_CreateBuffer` `0x5A6E60` per track | one `0x24000`-byte ring, 44,100 Hz 16-bit stereo, flags `0x181E0` (`CTRLPOSITIONNOTIFY` among them), refilled half by half by `Music_Pump` `0x5A7230` as DirectSound's two position notifications fire; volume by `Music_SetVolume` `0x5A6FB0` |
| the SND stream (a jingle; the inn's `OYASUMI` goes through the music path instead, section 4c) | `SndStream_Play` `0x5A7140` | a static buffer |
| the primary buffer | `Snd_Init` `0x5A6830` | set to 22,050 Hz 8-bit stereo; nothing is written to it |

DirectSound - Windows - sums the secondary buffers. So the tap is the hand-over itself
(`src/hook/wav_dump.cpp`):

- The import slot `0x5C4014` (DSOUND.dll #1, `DirectSoundCreate`; `exe_host.cpp`'s `kSlots`) is
  pointed at a wrapper. Hosted, the loader has bound it before our `DllMain` (the launcher's remote
  `LoadLibraryW` runs after the process initialises); in the exe host `BindSlots` has. `WavDump_Start`
  checks the slot holds DSOUND's own export before it changes it, and `Fatal`s otherwise. The slot is
  outside `.data`, so the state hash never sees it ([`state-hash.md`](state-hash.md)).
- The wrapper hooks the device's vtable (`CreateSoundBuffer`, `DuplicateSoundBuffer`) and each secondary
  buffer's (`Release`, `Lock`, `Unlock`, `Play`, `Stop`, `SetCurrentPosition`, `SetVolume`, `SetPan`,
  `SetFrequency`). Every hook calls DirectSound first and changes the dump's model only on success, so
  the game and the device see what they saw without it. Capcom's code and ours reach DirectSound only
  through these vtables (`sound_callees.h`), so a `BOF3X_ORIGINAL` side is dumped the same way. One
  buffer vtable served every secondary buffer on this machine (`hooked the DirectSound buffer vtable
  6A6B1568 (1 so far)` in every run's log).
- A **static** buffer is modelled as DirectSound plays it: its bytes copied at `Unlock` (at the offsets
  `Lock` gave), a cursor, looping or not, frequency (linear interpolation), volume
  (10^(hundredths / 2000), -10000 silent) and pan.
- A **stream** (a buffer made with `CTRLPOSITIONNOTIFY`: the music's ring) is played from a queue of its
  bytes in the order the game wrote them. The pump writes when the real device's notifications fire, so
  after a frozen shot or a stall it writes two halves at once; a ring-shaped copy read at the frames'
  pace would then read halves already overwritten. The first cut dropped the queue back to a ring when it
  grew past one and a half, and two runs of `combat.txt` differed from 19.5 s on (`frame 1800: ... 24084
  dropped to catch up` in one log, none in the other). Played from the queue and never skipped, two runs
  are **equal sample for sample**: `runs/combat4` against `runs/combat5`, 3,862,609 frames, 0 differing
  samples; `wav_diff.py`: "no window differs". The queue's lead is logged (at most 141,243 samples,
  3.2 s, on `nueRandomEncounter.txt`, five frozen shots).

**The clock.** The model is mixed once per logic frame - `Frame_Counter` `0x937F94` - at
round(frames x 44,100 x period / 1000) samples from the run's first frame: the period is DIV-0047's
1001 / 30 ms (`BOF3X_FRAME_MS` when set) under our WinMain, Capcom's 33.334 ms (`0x5C4218`) under
`BOF3X_ORIGINAL=...Game_WinMain`. A frame is mixed when the latch of a later one runs (`WavDump_Tick` from
`DeviceLatch`, `HashedLatch`, or - Capcom's WinMain with no recipe, recorder or hash - a `WavLatch` on
the latch call `0x4FCDDE`) or when a hooked call arrives in a later frame, always before that call's
change. So a call lands on the frame it was made in, the same on every run of a recipe.

**The file.** 44,100 Hz, 16-bit, stereo PCM, the sum clipped. The rate is the music's own (the ring is
44,100 Hz), so the music passes sample for sample at full volume; the primary buffer's 22,050 Hz 8-bit
format is what `Snd_Init` asks for, and Windows' mixer since Vista is not bound by it - not measured here,
which is why the dump does not imitate it. The header is rewritten after every write (every 15 frames), so
a run ended by `TerminateProcess` - the runners' way - leaves a valid file short of at most half a second;
`DLL_PROCESS_DETACH` writes the rest. A status line every 1,800 frames: `wav frame N: S s written; B
buffers made (K streams), P effect plays; D samples the music's queue ran dry; the queue's lead at most L
samples`.

**What it is not.** Not the device's output after Windows resamples to the endpoint, and not the stale
ring halves and gaps a stalled or frozen game makes the real device play: it is the mix the game asked for,
frame by frame. Under `BOF3X_SPEED` the frames outrun the device and the music's queue runs dry (logged;
silence where it is dry). The effects' looping one-shots and every DirectSound call the game makes are
modelled; `Lock` from the write cursor, `SetCurrentPosition` on a stream and `DuplicateSoundBuffer` are
not, and `Fatal` if the game ever asks for them (it never has).

## 2. The switch

| Variable | Does |
|---|---|
| `BOF3X_WAV_DIR=<dir>` | the dump on; the file is `<dir>\<n>_<label>.wav`, `<n>` one more than the largest number a `.wav` there starts with (`001` in an empty directory) |
| `BOF3X_WAV_LABEL=<label>` | the label; default the recipe's name (`BOF3X_INPUT`), else `play`; characters outside `A-Z a-z 0-9 - _ .` become `_` |
| `input_run.py --wav <dir> [--wav-label L]` | sets both; the label defaults to the recipe's name, with `_orig` when `--original` is given |

**No ledger entry.** Off unless set, and with it set nothing the game computes, draws or plays changes
(the same verdict as `BOF3X_SHOT_EVERY`, [`abc-harness.md`](abc-harness.md) 3). The new `music TRACK
[FRAMES]` recipe step (section 4b) is instrumentation the same way as `poke`: it acts only when a recipe
says so.

**Checks** (scratch `check_wav.py`: the header through Python's `wave`, the duration against the
frames the log names, per-second RMS):

| Run | Frames | WAV | Against frames x 1001 / 30 ms | Per-second RMS |
|---|--:|--:|--:|---|
| `combat.txt` (`runs/combat5`) | 2,620 | 87.588 s | +0.19 % (frames after the recipe's end, before the kill) | median -17.6 dBFS; 1 of 88 s below -60; effects heard alone between the title music's stop at 6.2 s and the field's at 11.2 s (two identical bursts at 9.0 and 10.0 s, a menu sound) |
| `musicStopCheck.txt` (`runs/musicstop1`) | 3,975 | 132.632 s | -0.00 % | median -19.6 dBFS; 1 of 133 below -60; 77 effect plays by frame 3,600 |
| `nueRandomEncounter.txt`, ours | 12,443 | 414.914 s | -0.06 % | median -20.8 dBFS; 0 of 415 below -60 |
| `nueRandomEncounter.txt`, `BOF3X_ORIGINAL=AreaTrackFallback` | 12,443 | 415.42 s | +0.06 % | the same route frame for frame (4a) |
| `nueRandomEncounter.txt`, `*,-Game_Clock,-Game_WndProc` (hosted, Capcom's WinMain) | 11,735 | | period 33.334 ms | 0 samples dry; 341 effect plays by frame 10,800 |
| `loopRows.txt`, loops on / off | 10,061 | 335.34 / 335.84 s | | equal until the first join (4b) |
| `shop.txt`, PC / US-disc-only | 3,157 | 105.61 s each | +0.25 % | equal to 88.32 s (4c) |

The brief's "frames / 60" is not this game's pace: a logic frame is 1001 / 30 ms (DIV-0047), and the
durations above match that within the frames run after a recipe's end. 3,099 of 7.8 million samples
clip in `combat.txt`'s battle (the battle theme near full scale plus effects); DirectSound would have to
clip them too, or limit.

Self-test: `BOF3X_LANG=original BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW='*'` from an ini-less copy of the
branch's tip (the `music` step in), exit 0 after 1,593 s, all 1,076 `MISMATCHES` counts 0 (`inject: 10087
ours`); the first cut's run the same (exit 0, 1,846 s). The dump arms after the self-tests, so it cannot
reach them. `combat.txt` once more on the tip's build and the new `input_run.py`: equal to the earlier
run sample for sample (`wav_diff.py`: no window differs).

## 3. `tools/wav_diff.py`

`python tools/wav_diff.py A.wav B.wav [--out diff.tsv] [--threshold -50] [--window 1.0]`: per window the
levels of A, B and A - B (dBFS) and A's minus B's level; `differs` is 1 when A - B is above the threshold
and within 30 dB of the louder side. The summary names the first differing window and the length
difference. A one-frame shift of the same audio shows as `differs` with a level delta near 0 - "the same
music, moved".

## 4. The listen set (`<main>/analysis/listen-1013/`, game-derived, not in the tree)

One folder per question; each has a `README.txt` (what to listen for, at what second) and the
`wav_diff.py` TSV.

### 4a. The random-encounter resume (DIV-0092)

Recipe `tools/recipes/nueRandomEncounter.txt` (new): the owner's `cutsceneAndNue.txt`, then the party
paced left and right in area 23 until a random encounter, fought with circle (`seek circle 0x66C7E8 u16
== 2 max 400`), and a minute of the field. Two random fights come on this build, at frames 8,025 and
9,618.

| File | Build | What happens after each random fight |
|---|---|---|
| `ours_full.wav` | all ours (DIV-0092) | the field's track 27 resumes: `remember track 27 at sample 1908864` (f8025) / `resume track 27 at sample 1908864` (f8966); `remember ... 235008` (f9618) / `resume ... 235008` (f10638) |
| `capcom_rule_full.wav` | `BOF3X_ORIGINAL=AreaTrackFallback` (Capcom's rule in ours; DIV-0092's own switch) | silence |
| `all_capcom_full.wav` | `*,-Game_Clock,-Game_WndProc` | silence; the route parts from ours at frame 3,661 (below) |

`ours` and `capcom_rule` play **the same route frame for frame** (`randlog` equal on all 12,445
frames), so `wav_diff.py` lines them up: the first difference at 6 s (the load: DIV-0092's second case,
the area's song on a load), the event battle's end at 224 s, and each random fight's end at 299 s and
355 s. The resume's position comes from the real device's play cursor
(`Music_Buffer`'s `GetCurrentPosition`), so it differs between runs by a few thousand samples
(`235008` here, `237312` in the trial run): inherent to DIV-0092, not to the dump.

**A finding for the coordinator (not numbered):** with `--original '*,-Game_Clock,-Game_WndProc'` the
route's `Rand` count parts from ours at frame 3,661 (`rand 112` against `111`), inside the sunset
cutscene, and the random fights come elsewhere (one, ending at frame 9,375). One run only; the state
hash's pairs never played `cutsceneAndNue` (`abc-sweep-1012.md` 6). The first try with Capcom's WndProc
(`'*,-Game_Clock'`, `--no-front`) made 2,404 frames in ten minutes, the window's freeze
([`windowed-mode.md`](windowed-mode.md)), so an all-original side for a `--no-front` run wants
`-Game_WndProc`.

### 4b. The loop rows (DIV-0081)

No committed recipe plays `064`, `076`, `037`, `084`, `109` or `130`: every recipe but the `_ab` twins
and the attract ones, played at `--speed 8` under `BOF3X_MUSIC_LOG=1` (scratch `harvest.sh`, 42
recipes; two lost to the slot-0 lock, `stallion` ended in a crash - section 5), asked for tracks 0, 3..5, 7, 10, 12..16, 19, 27, 29, 33, 40..42,
46, 47, 49, 50, 54, 55, 77, 80, 85, 91..93, 126, 141, 151, 153..155, 165 and 166. The Jukebox plays
through the launcher's own `waveOut`, not the game's mix, so the dump cannot hear it. So the recipe
language has a new step, **`music TRACK [FRAMES]`**: the game's `Music_Play(TRACK, FRAMES)` (default 8,
the faerie village's music menu's) called through the original's entry at that frame, logged `input
music T (fade F frames) recipe frame R`. It is the menu's own call, made by a recipe; off unless a
recipe says so, like `poke`.

`tools/recipes/loopRows.txt` (new): the combat save, the party standing in the field, then `music 64`,
`music 76`, `music 37`, each long enough for two joins either way. Played twice from the same build,
`BOF3X_MUSIC=mp3`, unset (`music_loops track 064 looped at its measured points: sample 1438947 back to
23399`; 076 1438849 to 23301; 037 566960 to 734) and `BOF3X_MUSIC_LOOPS=0` (`music_loops off`). The two
files are equal sample for sample until each song's first join and differ from there to the next song:
`wav_diff.py` windows 48..150, 183..240, 253..335 s. Song starts 15.4, 150.5 and 240.6 s (frames 461,
4,511, 7,211); joins with the rows at 48.0 s (exactly 15.38 + 32.63 s, the row's end), 80.1, 112.2,
144.3 (064), 183.2, 215.2 (076), 253.5 and every 12.84 s after (037). The rewinds with the rows off,
found where 2 s of each song's opening recur (normalised correlation 0.90..0.98): 79.55 and 143.74 s
(064), 190.52 and 230.62 s (076), 285.23 and 330.14 s (037). `037` was the one of the four new rows
taken (the faerie village's music menu plays it too); `084`, `109`, `130` are a `music` block each in a
copy of the recipe.

### 4c. The inn's jingle on a PSX-only install (D255)

`shop.txt` (save `town`) on the player's PC install and on a US-disc-only install built for this
(`importer.py build --source "<main>/CDImage/Breath of Fire III (USA).cue" --lang en-US`, 571 s;
`install --base`; scratch `wav/d/`), both on the disc's sequenced music (`BOF3X_MUSIC=seq`), so only
what differs between the installs differs. The recipe saves come from `BOF3X_RECIPE_SAVES` (the main
checkout's `tools/recipe_saves`) - without it a disc-only game directory looks in the worktree's and
finds none.

The two files are **identical sample for sample to 88.32 s**: the music, the effects and the route the
same from two installs. At frame 2,647 (88.3 s) the PC opens `SND\OYASUMI.DAT` and plays it through the
music path (`Music_Start(..., 107416, 0)`, a once-only stream), faded out by 93.5 s, silence over the
black screen (94..98.5 s), the town again at 99 s (frame 3,000's capture lit). The disc-only install
logs `open ... FAILED SND\OYASUMI.DAT`; the town's song plays on through the inn, and the screen is still
black at frames 3,000 and 3,150 (`BOF3X_SHOT_EVERY=150` captures all zero) - D255's longer black, with the
town's song under it.

## 5. Not reached, and what was seen on the way

- **Not heard by anyone.** Every listen above is for the owner.
- **Loop rows `084`, `109`, `130`**: not rendered (the brief asked for one of the four new rows; `037` was
  taken). A copy of `loopRows.txt` with `music 84` etc. makes them.
- **A "restart from the top" side for 4a**: DIV-0092 has no switch for the resume alone, so the pair is
  resume (ours) against silence (Capcom's rule); the restart is the PlayStation's own, heard in mednafen.
- **The endpoint's own format** (`IAudioClient::GetMixFormat`) is not read; the dump's rate is the
  music's, by choice (section 1).
- **Found on the way, for the coordinator (not numbered):**
  - the all-Capcom side of `nueRandomEncounter.txt` parts from ours at frame 3,661 (`Rand` 112 against
    111; section 4a), one run;
  - `stallion.txt` (no `# save` header, so slot 0 as the owner's install held it) **crashed twice at
    `--speed 8`** (scratch `harvest.sh`, then `stallion.sh`): `exception 0xC0000005 at bof3x.dll+0xCA3F3`,
    reading 0, area `0x1D`, mode 2 - `llvm-symbolizer` on that build's DLL: `Area_ZoneAtIn`
    (`src/game/window_task.cpp:227`) under `Area_PickMusic` (`:194`) under `Field_ChangeArea` (`:137`).
    At `--speed 1` the same recipe from the same slot ran 10,310 frames into area 29 with no crash (timed
    out at six minutes). Not compared with Capcom's code; the save is not a recipe save, so the route is
    not the owner's as recorded;
  - a trap of the runner, met here: `input_run.py` reads the launcher directory's `bof3x.log` from the
    top, and a previous run's `recipe done` there ended the next run within seconds (`runs/combat3`, no
    WAV written) before the new game had truncated the log. `input_run.py` now deletes that log before
    it launches.
  - recipe saves for a disc-only game directory need `BOF3X_RECIPE_SAVES` (section 4c).

## 6. Files

- `src/hook/wav_dump.h`, `src/hook/wav_dump.cpp` (new); `src/hook/dllmain.cpp`, `src/hook/exe_host.cpp`
  (`WavDump_Start` before `InputScript_Start`, `WavDump_Stop` at detach); `src/hook/input_script.cpp`
  (the tick in `DeviceLatch` / `HashedLatch`, `WavLatch`, the `music` step); `CMakeLists.txt`.
- `tools/input_run.py` (`--wav`, `--wav-label`), `tools/wav_diff.py` (new).
- `tools/recipes/nueRandomEncounter.txt`, `tools/recipes/loopRows.txt` (new).

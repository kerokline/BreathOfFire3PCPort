# The quicksave census: the state outside the span

**Status:** MEASURED (2026-10-19, the quicksave round's group SC, branch
`quicksave/sc` off `quicksave/round` `f7886490`). The spec is
[`quicksave-round.md`](quicksave-round.md) section 2 "SC" and section 2a.
Every mutable static of `bof3x.dll`'s own code classed by
[`tools/quicksave_census.py`](../tools/quicksave_census.py) (3,341 objects in
621 object files, `check` passes); the heap read call site by call site; two
of the design's premises measured false. No `src/` file changed.

## 0. The answer in one paragraph

Section 0 of the round doc says that between two logic frames "every task of
the game is suspended on a stack that lives inside the image", and section 1
that "in the exe host our code sits at a fixed address". **Neither holds**
(section 2): the four task stacks are 16 KB slices of the main thread's own
stack below `WinMain`'s frame, outside `0x5C4000..0x987000`; and `bof3x.dll`
is linked `DYNAMIC_BASE` and was measured loading the same build at two
different bases from two directories. Both are fixable inside SN's design
(copy the slices as a second span; record and check the DLL's base, or link
it at a fixed one), but SN must take them before it is built. Past that the
census is small: of 3,341 statics, **19 are game state** - 8 for SP-A, 6 for
SP-G and 5 that belong to neither and are proposed here as one more
participant, **ST** (ours' statics: Rand's seed, the grown draw pool's free
ring, the relocated message pool, two button-map words); 26 are per-frame
(4 of them, the layering's last pass, want SP-G to invalidate them); 439
are configuration; 2,857 are tooling, 2,059 of those in `*_fuzz.cpp`.
**No heap block holds logic state without a source to rebuild it from**, so
I13's fixed-address arena does not enter SN (section 4). No thread but the
main one touches the span: the "stream thread" of the skip list's comments
does not exist in our code - `Music_Pump` fills `Music_Staging` from
`Sound_Tick` on the main thread (section 6).

## 1. How the sweep was done, so its coverage can be judged

- **Statics, from the objects, not from grep.** `tools/quicksave_census.py`
  runs `llvm-nm --defined-only` over every `.obj` of the `bof3x` target under
  `src/` (`build/CMakeFiles/bof3x.dir/src/**`: game 588, hook 19, audio 4,
  input 4, render 4, common 2 objects) and keeps every symbol of COFF type
  `b`/`B`/`d`/`D` - `.bss` and `.data`: namespace-scope objects,
  anonymous-namespace ones, class statics, function-local statics and their
  guard variables, whatever the source spelling. Read-only data (`r`) is not
  mutable and not listed; a static the optimiser folded into code holds
  nothing across frames and is rightly not seen. Names demangled by
  `llvm-cxxfilt`; a split aggregate (`g_steps (.0)`, `(.1)`) is one object.
  **3,341 objects.** Each is put in a class by an ordered rule list in the
  tool, and `check` fails on any object no rule names - a new static is
  classed by adding its row, never by widening a pattern.
- **What the objects do not cover.** `bof3x.dll` has 4,116 data symbols
  (`llvm-nm -n` of the DLL); 679 of them are not in our objects: SDL3 (the
  gamepad, `pad_sdl.cpp`), libc++ / libc++abi and the mingw runtime. They
  hold library state (SDL's device lists, the C++ runtime's statics, the
  heap's own bookkeeping) and no game value; they are counted, not classed.
  **Thread-local storage:** the DLL's `.tls` is 0x10 bytes, the toolchain's;
  no source of ours says `thread_local` / `__thread` / `__declspec(thread)`.
- **`src/launcher` in the game process: none.** The `bof3x` target compiles
  no `src/launcher` file (the sweep finds no object there); the launcher's
  copies of `src/audio` and `src/input` are its own process's. What the
  launcher puts into the game process are environment settings, read at
  start-up into the configuration rows below, and in the exe host nothing
  at run time (the hosted build's draw-pool block is the hosted build's).
- **The heap**: every `malloc` / `calloc` / `free` / `new` / `HeapAlloc` /
  `VirtualAlloc` in `src/game`, `src/audio`, `src/render`, `src/input`,
  `src/hook` outside the self-tests (a grep, then each site read) against
  [`crt-rest.md`](crt-rest.md) 2.6's list of ours' sites by name. Section 4.
- **The span's own words that name something outside it**: the skip list,
  the symbols of `symbols.toml` around each of its rows, and the state hash's
  pointer census ([`state-hash.md`](state-hash.md) 7). Section 5.
- **Threads**: every `CreateThread` / `CreateFiber` / `_beginthread` /
  `std::thread` in the DLL's sources, and the exe's imports. Section 6.
- **Measured** (2026-10-19, this build, headless, no game window): three
  `BOF3X_SELFTEST_ONLY=1 BOF3X_STACKWATCH=1` runs from two ini-less
  launcher copies (`C:\Users\kerok\q_sc\launcher`, `launcher2`), and the 60
  newest exe-host logs under the A/B/C harness roots. Section 2.
- **Not done**: the live check (a `BOF3X_STATEHASH` route against a second
  run). Two runs of one route measure what is nondeterministic between
  processes, which is the skip list's "noise" kind; RC's identity
  references for its six routes are that measurement, run beside this group,
  and a third game window on the machine lock would only have repeated one.
  Section 8 says what it would add.

## 2. Two premises of the design that do not hold

### 2.1 The task stacks are on the main thread's stack, not in the image

`Task_SetStackBase` (`0x5A9907`, ours in `task_sched.cpp`) is
`Task_StackTop = esp - 0x4000` with `esp` as it arrives from `Game_Init`'s
tail jump, and task `k`'s stack is the 0x4000 bytes below
`Task_StackTop - k * 0x4000` ([`task_sched.md`](task_sched.md) 1). That `esp`
is the main thread's: [`crt-startup.md`](crt-startup.md) 11 measured it, and
`src/audio/mp3.cpp`'s comment on the decoder's private stack is the scar of a
13 KB frame that overran one slice into the next (2026-10-10). So between
frames the suspended tasks' return addresses, saved registers and locals
live at `[Task_StackTop - 0x10000, Task_StackTop)` on the thread's stack -
**not inside `0x5C4000..0x987000`**. The skip list's row
`0x66C854 37292 # both Task_SchedulerEsp and the task stacks, to Area42_Rank`
names the wrong thing: those 37,292 bytes are `Task_SchedulerEsp`,
`Task_StackTop` (stack addresses), and from `0x66C858` to `0x675A00` data no
code of the exe host reads ([`exe-engine-own.md`](exe-engine-own.md) 4.1:
the MMX tables, Xaudio's and the C runtime's). What the span holds of the
scheduler is the four records (`Task_Records` `0x66C7D0`, 0x20 each) whose
`+4` words are saved `esp` values pointing into the slices.

**What it means for SN.** A copy of the span alone restores records whose
saved stack pointers name stacks that hold whatever the 300 frames since
left there: the first `Task_RunAll` after the restore `ret`s into garbage.
The fix is a second span: **the 64 KB of slices**, copied at the take and
written back at the restore, with `Task_StackTop` in the file's header.

- **The address is stable across processes in the exe host** (measured):
  three runs, two directories, the main thread's `StackBase` `0x001B0000`,
  reserve `0x100000`, `Engine_Main`'s `esp` `0x001AF90C` every time
  (`stack:` lines of the three logs, `scratch\selftest_*.log`). `bof3x.exe`
  has `DllCharacteristics 0` (no ASLR; `llvm-objdump -p`), and the game's
  `Task_StackTop` is a fixed function of the build's frames below that base.
  So the slices restore across processes of one build; the header's
  `Task_StackTop` is compared and a mismatch refused, as the build id is.
  Not measured: across a reboot, or on another Windows.
- **The restore must not run below `Task_StackTop + 0x4000`.** Between
  frames the main thread stands in `WinMain`'s loop, about 0x200 bytes below
  `Game_Init`'s arrival, so everything `Snapshot_Pending` calls has about
  0x3E00 bytes before it writes into slice 0's top - the slice it has just
  restored. DirectSound, Direct3D 11, the file layer and the C runtime
  called from there can go deeper than that. So the take and the restore
  (and every participant's `Save` / `Restore`) run on a stack of their own,
  as the decoder's frame does (`mp3.cpp`, `DecodeFrame`) and the renderer's
  does (`RunOnFiber`): **a design rule for SN**, with the slices written
  last, after every participant has run.
- The hosted build (`BOF3.exe` + `bof3x.dll`) has the same shape; the round
  doc already leaves it in-process only.

### 2.2 Our code does not sit at a fixed address in the exe host

`bof3x.exe` has no relocations, but our code is not in it: it imports
`bof3x.dll`, and the DLL is `DllCharacteristics 0x0140` (`DYNAMIC_BASE`,
`NX_COMPAT`), preferred base `0x10000000`, `SizeOfImage 0x6FCD000`
(`llvm-objdump -p build/bof3x.dll`). Measured on 2026-10-19 with this build:
`Msg_SystemPtr`'s replacement at `0x0FC9CE10` from `q_sc\launcher` twice and
at `0x0F78CE10` from `q_sc\launcher2` - the same file, `0x510000` apart. The
harness logs say the same of older builds (the exe host's
`inject ON Msg_SystemPtr ... -> ours` line, same low bits = same build):
on 2026-10-16 one build loaded at `0x5D216040`, `0x56826040`, `0x0F576040`
and `0x55F36040`; on 2026-10-18 another at `0x5C4C8960`, `0x0F948960` and
`0x0F368960`. One directory, one boot: the same base (the third run here);
another copy of the DLL, or a reboot: not.

**What depends on it.** Every return address on the task slices (each
suspended task returns into code of ours: `Task_Sleep`'s caller is ours);
the language overlay's pointer slots in the span (DIV-0038's
`Pause_LinesGame` / `Pause_LinesTitle`, DIV-0064's label tables,
the fishing lines, the battle banners: 73 such slots on the combat route,
[`state-hash.md`](state-hash.md) 7); `Imp_GetTickCount`'s slot
(`0x5C407C`, DIV-0022's `SessionTickCount`); the display's singleton
pointers (`Dd_DirectDraw`, `D3d_Device`: `render_shim.cpp`'s static
objects); and any pointer the game keeps from `Msg_SystemPtr` into the
relocated message pool (DIV-0007, a static of ours). In process, all of
these hold. Across processes, only if the DLL lands where it did.

**Two answers, for the owner** (section 9): link `bof3x.dll` for the exe
host without `DYNAMIC_BASE` (the loader then takes `0x10000000` unless that
range is taken, and the file can say so), and in any case write the DLL's
load address into the quicksave's header and refuse a snapshot whose
address differs, loudly, keeping the save half - the build-id rule extended
by one word. The refusal alone makes cross-process quick-loads work from one
launcher directory within a boot, which is how the owner plays (one
folder, `game\`); the fixed base makes them work after a reboot.

## 3. The census by class

| Class | Objects | What a restore does with it |
|---|--:|---|
| per-frame | 26 | nothing, but for 4 SP-G invalidates (layering's last pass) |
| configuration | 439 | nothing: the process's and the player's, kept as they are (118 callee tables, 44 language-overlay rows, 87 platform objects, the rest inject-time switches and operands) |
| game state | 19 | carried in a participant's record (below) |
| tooling | 2,857 | nothing (2,059 in `*_fuzz.cpp`, 128 in the harnesses, 670 instruments, log-once flags and self-test fixtures inside game files) |
| **all** | **3,341** | |

`python tools/quicksave_census.py summary` prints these; `table` prints
appendix A; `all` one TSV row per object.

### 3.1 Game state, by participant

**SP-A (audio)** - 8 objects, all in `src/game/sound.cpp`,
`music_seq.cpp`, `save_menu.cpp`:

| Object | What it is | Record / rebuild |
|---|---|---|
| `sound.cpp` `g_loop` | the measured loop's row, decode position and carried bytes (DIV-pending, `music_loops.h`) | position saved; rebuilt by `Music_Start` + `ResumeSkip` (decoding from the top, the loop table's own mechanism) |
| `sound.cpp` `g_ring` | DIV-0092's ring bookkeeping: samples written, each half's base, the track | rebuilt by `Music_Start`; the heard sample computed from it at the take |
| `sound.cpp` `g_resume` | DIV-0092's remembered / pending area-track position | saved as numbers |
| `sound.cpp` `sound::g_fade_frame`, `g_stop_unseen` | DIV-0028's fade, beside `Music_FadeCount` / `Music_FadeStep` / `Music_FadeStops` (span) - and DIV-0143's fade to a stop is this same state | saved |
| `sound.cpp` `sound::g_unducked` | DIV-0132's duck outstanding | saved |
| `music_seq.cpp` `g_s` | DIV-0087's song, bank and synth (heap: `new psx::Song`, `Bank`, `MusicSynth`), the active flag, the file names | rebuilt: `music_seq::LoadFile(track)` + `FastForward(sample)` - the path `ResumeSkip` already takes |
| `save_menu.cpp` `g_stream_missing` | DIV-0129: set while a stream's file is missing, and `Sound_StreamDone` answers 1 meanwhile - **logic-visible** (the inn and camp waits) | saved |

**SP-G (graphics)** - 6 objects game state, plus 4 per-frame to invalidate:

| Object | What it is | Record / rebuild |
|---|---|---|
| `map_layers.cpp` `g_file_sides` | DIV-0085: per map cell, the side faces as the area file loaded them (`SnapshotSides` at the area load) - the arena has been patched since, so not rebuildable from the span | copied, 64 KB |
| `widescreen.cpp` `g_split` | DIV-0041's faster slide: the 22 window records' Capcom x, drawn x and leg | copied (wide only) |
| `wide_rand.cpp` `g_seed` | DIV-0041's random split: the draw-only generator, stepping in play | copied (wide only; the picture) |
| `wide_rand.cpp` `g_noted`, `g_original`, `g_written` | the last inset `NoteInset` saw (`map_layers.cpp` `Inset`) | copied (wide only) |
| `layering.cpp` `g_runs`, `g_runs_now`, `g_runs_before`, `g_pass` *(per-frame)* | DIV-0071: one pass of history - a run is stamped `g_pass`, and the next pass reads the last pass's | not saved: SP-G moves `g_pass` on by 2 at the restore, so the first pass finds no run of the last and uses the proximity test (one frame; the picture only) |

Everything else SP-G owns is span or platform: the texture caches named in
the skip list (section 5), the CLUT rows' buffers (section 4),
`render_shim.cpp`'s surfaces and pipe state (configuration rows, marked
SP-G in appendix A: the caches in the span name them). `field_frame.cpp`'s
band (`g_band`, `g_band_count`, `g_band_added`) is per-frame: noted and put
back within one `Field_DrawFrame`. The map view's inset is
`MapView_Inset`, in the span. The frame grab (DIV-0125, `fb_grab.cpp`) and
SatPixie (`satpixie.cpp`) are tooling and platform; `g_flip` per-frame.

**ST (new: ours' statics)** - 5 objects that are neither audio nor
graphics. Proposed: one participant owned by SN, a byte copy of a named
list of statics of `bof3x.dll` plus rebuild hooks their modules register
(each module exposing its statics by an accessor, so SN names no other
module's internals):

| Object | What it is | Record / rebuild |
|---|---|---|
| `crt_rest.cpp` `crt_rest::g_seed` | **`Rand`'s seed** - MSVC's `holdrand`, ours since [`crt-rest.md`](crt-rest.md) 2.1 (`0x5B93D2`; 745 call sites, every one game code) | none: 4 bytes, copied. Without it every draw after a quick-load differs - the state hash would not see it until a draw changes `.data` ([`state-hash.md`](state-hash.md) 3) |
| `draw_pool.cpp` `DrawPool_Grow()::free_list` | DIV-0062's 2,048-entry free ring; `DrawItemPool_Top` (`0x9039D4`) and the items (`.pool`, `0x93F000`) are in the span, and map-view cells hold items across frames | none: 4 KB, copied with the span or two cells share an item |
| `msg_pool.cpp` `MsgPool_Relocate()::pool` | DIV-0007's relocated system-message pool (under a language overlay), filled from each area file's `0x4000` chunk | copied, 32 KB (the area's DAT is a source in principle, but which file last loaded is not recorded) |
| `input/sigma.cpp` `g_release_words` | DIV-0133/0134: whether the release's starting words or a save's stand - what `RecipeToPad` / `PadToRecipe` translate by | saved, 1 byte |
| `input/sigma.cpp` `g_table` | the unified button map, **derived** from the Config words in the span | rebuilt after the span: sigma's `Follow` (a hook, not a copy) |

QM's two quicksave tables are outside the span by design and are a listing,
not game state ([`quicksave-round.md`](quicksave-round.md) 3): no row.

### 3.2 Section 2a's items

- **`src/input/sigma.cpp`**: configuration but for two rows - the switches
  (`g_sigma_on`, `g_by_source`), the install's release and `g_armed` are
  fixed at `Arm`; `g_table` is derived from the span (ST rebuilds it);
  `g_release_words` changes in play (a new game's start, a save's load) and
  is ST's. So σ's map is game state in the sense the round doc asks: the
  Config screen and a save's load change it - but its source is in the span.
- **`src/game/shoulder_glyphs.cpp`**: configuration - the glyph sheet
  (`g_page`, `g_canvas`, `g_glyphs`, `g_pages`) is built once at arm from the
  install's font (DIV-0138); `g_on`, `g_page_art` switches; `g_rng`,
  `g_bad` the self-test's.
- **DIV-0143's fade to a stop**: the switch `g_keep_stopping_none`
  (`window_task.cpp`) is configuration; the fade it reads is
  `Music_FadeCount` / `Music_FadeStops` (span) and SP-A's
  `g_fade_frame` / `g_stop_unseen`.
- **`src/hook/gapwatch.cpp`**: tooling, off by default. Armed, it watches
  span pages with guard pages and debug registers; a restore's write over a
  watched page would fault into it. SN refuses F6 / F7 while
  `BOF3X_GAPWATCH` is armed (or pauses it, as the state hash's dump does,
  `g_pause_for_hash`).

### 3.3 The spec's examples, each answered

| Example | Row(s) | Class |
|---|---|---|
| DIV-0071's layering caches | `layering.cpp` `g_runs` .. `g_pass` | per-frame, SP-G invalidates |
| `field_frame.cpp` | `g_band*` | per-frame |
| `map_layers.cpp`'s inset | `MapView_Inset` (span); `g_file_sides` | span; game state SP-G |
| the language overlay's tables | `labels.cpp`, `battle_text.cpp`, `fishing_text.cpp`, `pause_text.cpp`, `config_text.cpp`, `text_advance.cpp`, `text_pairs.cpp` | configuration (pointers to them in the span: section 5) |
| DIV-0028's fade | span + `sound::g_fade_frame`, `g_stop_unseen` | game state SP-A |
| DIV-0041's random split | `wide_rand.cpp` `g_seed`, `g_noted`, `g_original`, `g_written` | game state SP-G |
| `BOF3X_SPEED`'s scripted speed | `win_main.cpp` `g_speed_scripted` (tooling), `g_speed` (F1, the player's: configuration), `save_menu.cpp` `g_stream_started` | tooling / configuration |
| the sequencer, `seq.cpp` | `music_seq.cpp` `g_s` (its synth on the heap) | game state SP-A |
| minimp3's decoder, `mp3.cpp` | the `Decoder` object (heap, section 4); `g_stack` the decoder's private stack (platform) | heap SP-A |
| the WAV dump's and band census's instruments | `wav_dump.cpp`, `band_census.cpp` | tooling |

## 4. The heap

Since [`crt-rest.md`](crt-rest.md) 2.6 every `malloc` the game reaches is the
toolchain's; in the exe host there is no Capcom code to reach the CRT's. The
blocks the game keeps across frames, and what holds each:

| Block | Allocated by | Held by (span unless said) | Contents | Rebuild source | Owner |
|---|---|---|---|---|---|
| a sound bank's sample data | `Snd_LoadBank` (`display_env.cpp`), from `LoadDatFile`'s kind-2 chunks and `Snd_LoadBankFile` | `Sound_Banks` + bank * 0x384 + 0x380 (`0x6BC928`), and each voice entry's sample pointer into it | wave data | the DAT chunk it came from - **not recorded anywhere** (the record keeps the chunk's first 0x380 bytes, not its file or size), so SP-A either records (file, layer, tag, size) per bank at load, or copies the block (its size known only if recorded) | SP-A |
| the BGM file image | `Music_LoadFile` (`sound.cpp`); `music_seq::LoadFile` for a cache song | `Music_File` `0x6BDE50`, `Music_FileSize` `0x6BDE48` | the track's file | `Music_LoadedTrack` (`0x6653BC`, span) | SP-A |
| the decoder's copy | `Music_Start` | `Music_Data` `0x7DE3D4` | a copy of the file (the decoder reads it) | `Music_Start(Music_File, size, loops)` | SP-A |
| the MP3 decoder | `mp3dec::Create` (`new Decoder`) | `Music_Decoder` `0x7DE3D0` | minimp3's state: bit reservoir, overlap, position | `Music_Start` + `ResumeSkip(sample)` (decode from the top; bit-exact, [`mp3-decoder.md`](mp3-decoder.md)) | SP-A |
| the last stream's file (jingles, voices) | `Sound_LoadStream` (`save_menu.cpp`) | `kStreamData` `0x6BDE44` | the stream file | the stream id is not kept (only `kStreamKind`): SP-A records it, or silences the stream (a one-shot cut by a restore is not a defect, round doc 3) | SP-A |
| the song, bank and synth | `music_seq.cpp` (`new`) | ST-side static `g_s` (not the span) | parsed song, VAB bank, synth voices | `LoadFile(track)` + `FastForward` | SP-A |
| the CLUT rows' converted pixels | `Gfx_ConvertRow` (`gfx_clut.cpp`), 0x1000 bytes a row on first use | `Gfx_ClutRows` `0x6C2A40` (512 x {generation, pixels}) | the VRAM shadow's rows in the display's format | the VRAM shadow (span): reconvert. The pointers are the process's: SP-G keeps the live ones (a fresh process has fewer rows allocated, or other addresses) | SP-G |
| the font glyph table | `LoadDatFile` kind 3 -> `Font_SetGlyphData` | `Font_GlyphData` `0x7CC35C` | the 24 x 24 font | once per launch (`FIRST.DAT`'s one kind-3 chunk, [`asset-loading-path.md`](asset-loading-path.md)): configuration - keep the live pointer | SN keep |
| the render shim's surfaces | `render_shim.cpp` (`HeapAlloc` on the process heap) | `Gfx_TexCache` `0x6C3F40`, `Font_TexCache` `0x7C9F50`, `D3d_CellTexCache` `0x7CAE38`, the `Dd_*` surfaces | converted texture pixels | the VRAM shadow: SP-G releases what the caches name and clears them | SP-G |
| text pairs, advances | `TextPairs_Apply`, `TextAdvance_Set` | statics of ours | language overlay tables | configuration | - |
| `LoadDatFile`'s file buffer, the cache reader's, `Snd_LoadBankFile`'s, `MapView`'s plane | `dat_load.cpp`, `dat_cache.cpp`, `save_menu.cpp`, `map_view.cpp` | locals | - | freed in the same call: nothing crosses a frame | - |
| the decoder's private stack | `mp3.cpp` (`VirtualAlloc`, 256 KB) | a static of ours | dead between calls | - | - |

**Headline: none of these holds logic state without a source.** The game's
logic data lives in `.data` / `.bss` (the arena at `0x803580` among it,
where `LoadDatFile` puts an area) and in `.pool`; the heap holds sound,
music and display caches, each rebuildable from the span plus one number,
and two whose number the span does not keep (which file a sound bank came
from; which stream id is loaded), for SP-A to record at load. So I13's
fixed-address arena does not become a step of SN.

## 5. Words in the span that point outside it, and the keep list

A copy carries these words; across processes (and for some in process) they
go stale. Each needs a fix-up after the span is written. **"Keep"** below
means: read the live word before the span is written, write it back after -
the pattern SN already applies to `Task_SchedulerEsp` and `Game_Hwnd`,
widened.

| Span word(s) | Points to | Fix-up | Who |
|---|---|---|---|
| `.rdata` `0x5C4000..0x5DA000`, in it the 14 bound import slots `0x5C4014..0x5C4174` | Windows' DLLs; `0x5C407C` our `SessionTickCount` (DIV-0022) | **leave `.rdata` out of the copy**: the exe host's start-up writes it whole (44 constants, the slots, and `c_dfDIKeyboard`'s header at `0x5C4948` with a pointer to the SDK's objects in our DLL - its start-up log's `rdata:` lines) and the game's code is not known to write it after; not measured in play (the state hash starts at `0x5DA000` too) - a dump of `.rdata` at two ticks would settle it | SN |
| `Task_Records`' `+4` (`0x66C7D4`, `..F4`, `..814`, `..834`), `Task_StackTop` `0x66C858` | the task slices (main thread's stack) | copied, valid with the slices (section 2.1); `Task_StackTop` checked | SN |
| `Task_SchedulerEsp` `0x66C854` | the loop's frame | keep | SN |
| `Game_Hwnd` `0x6BC620`, `Game_HInstance` `0x6BC624` | the window, the module | keep | SN |
| `Overlay_Text` `0x6BC62C`, `Overlay_Frames` `0x6BC630` | the overlay line showing at the take (a pointer: Capcom's `.rdata` or ours) | SN writes its "Quickload" line **after** the span | SN |
| `Fps_Shown` `0x6BC634`, `App_Active` `0x6BC63B`, `Cfg_Fullscreen` `0x65DA44`, `Cfg_RenderMode` `0x65DA48`, `Key_Table` `0x7DE7A8` (128) | the player's / the process's settings | keep | SN |
| `Game_Paused` `0x6BC63A` | the pause | the snapshot's own value comes back; QM's Load must leave the game unpaused whatever it was (F6 pressed during F9's pause) | SN / QM |
| `Sound_Channels` `0x6BC8C8` (23 words) | DirectSound buffers, heap | rebuilt | SP-A |
| `Sound_Banks` `0x6BC928` (6 x 0x384) | heap sample data, DirectSound buffers | rebuilt (section 4) | SP-A |
| `kStreamData` `0x6BDE44`, `Music_File` `0x6BDE50` | heap | rebuilt | SP-A |
| `Music_Events` `0x7DE3B4` (2 handles), `Snd_Device` `0x7DE3BC`, `Snd_Primary` `0x7DE3C0`, `Music_Buffer`, `SndStream_Buffer`, `Music_Notify`, `Music_Decoder`, `Music_Data` (to `0x7DE3D8`) | DirectSound objects, events, heap | device and primary kept; the rest released before and rebuilt after | SP-A |
| `Music_Staging` `0x7CC378` (73,728) | - (sample data) | refilled by the next `Music_Pump`s: may be left out of the copy | SP-A |
| `Gfx_WindowRect` .. `Dd_CellBackdrop` `0x6BE1D0` (2,096) | the window rectangle, the mode list, a surface | keep | SP-G |
| `Gfx_ClutRows` `0x6C2A40` (4,096) | heap rows | keep the pointers, reconvert | SP-G |
| `D3d_BackMaterialHandle` `0x6C3A40`, `D3d_ShadeModeCache`, `Gfx_RenderFlags`, the device description (1,280) | caches of the device's state | marked unknown (or pushed to `render_shim`'s `g_state`) - else a skipped state change | SP-G |
| `Gfx_TexCache` `0x6C3F40` (24,576), `Font_TexCache` `0x7C9F50` (128 x 20, two surface pointers each), `D3d_CellTexCache` `0x7CAE38` | `render_shim` surfaces | cleared, the shadow invalidated whole | SP-G |
| `D3d_DeviceDesc` `0x7CC238`, `Dd_DirectDraw` `0x7CC334`, `Dd_StageSurface` `0x7CC344`, `D3d_Device` `0x7CC350` | `render_shim`'s static objects (our DLL) and surfaces | keep | SP-G |
| `Font_GlyphData` `0x7CC35C` | heap (once a launch) | keep | SN |
| `File_Slots` `0x7DE3E4` | open files (the toolchain's `FILE*`) | none is open between frames (every reader closes in its call): SN asserts all empty at the take | SN |
| `DInput_Object` .. `Gpu_DispEnv` `0x7DE928` (952) | the input objects | keep | SN |
| the language overlay's pointer slots (DIV-0038 `0x66A418`, `0x66A448`; DIV-0064's `0x663970` and its copies, `0x66B12C`, ...; `0x653B98`, `0x66A088`, `0x669DE0`) | text in `bof3x.dll` | none needed in one build at one base (section 2.2); they are rewritten by the same inject in every process | SN's base check |
| the CRT's words `0x93C5A0`, `0x93C6B4`, `0x93D6E0..` | stack / heap addresses | none: written only by Capcom's start-up, which the exe host does not run; read by nobody | - |
| `.pool` `0x93F000..0x987000` (draw items) | draw items link by 24-bit addresses inside `.pool` | copied, with ST's free ring | SN / ST |

Our statics that hold the span's addresses (`draw_pool::g_items`,
`g_free`, `msg_pool`'s `g_pool`, the sound callee tables) are configuration
and fixed within a process; nothing of ours caches a heap pointer the span
also holds except what SP-A and SP-G rebuild.

**`Frame_Counter` goes back at a restore.** Statics of ours that remember
it: `sound::g_fade_frame` (SP-A saves it), the recipe player's
`g_last_frame` (an inequality test, so a jump back still counts as a new
frame), the state hash's and the stall watch's counters (their own ticks keep
running - what RC's check splits at; the stall watch counts beats). A grep
for `Frame_Counter` compared with `<` / `>` in `src/` finds none: ours
reads it as a parity, a mask or a log line.

## 6. Threads and stacks

| Thread / stack | Touches the span? | Around a restore |
|---|---|---|
| the main thread: `WinMain`'s loop, every task (slices of its stack), `Sound_Tick` / `Music_Pump`, the state hash | yes - the only one | the restore runs on it, between frames, on a stack of its own (2.1) |
| the render fiber (`render_d3d11.cpp` `RunOnFiber`, 1 MB) | no (reads the frame's packets inside a present) | nothing: a fiber of the main thread, idle between frames |
| the decoder's private stack (`mp3.cpp`, 256 KB) | no | nothing |
| the log writer (`log.cpp`), the crash reporter (`crash.cpp`), the stall watch (`stallwatch.cpp`, reads `Frame_Counter`) | no (the stall watch reads one word) | nothing; the stall watch sees the counter jump back and must not call it a stall (it watches beats, not order) |
| SDL3's own (the gamepad) | no | nothing |
| DirectSound's mixer (the OS) | no - it reads DirectSound's buffers | SP-A stops the music and sound buffers before the span is written |

**The music stream has no thread of its own.** The skip list's comments
("the stream decodes on its own thread") and the round doc's
`Music_Staging` line describe a thread nobody creates: `BOF3.exe` imports
no thread-creating function ([`crt-rest.md`](crt-rest.md) 2.1), and
`Music_Pump` (`0x5A7230`, ours) asks `MsgWaitForMultipleObjects` with a
zero timeout from `Sound_Tick`, on every spin of `WinMain`'s frame wait, and
writes `Music_Staging` there. What made `Music_Finished` and the staging
buffer differ between two runs is the play cursor's timing against the
wait loop, not a thread. So nothing has to be paused; SP-A stops the
buffers, restores, and the pump refills.

## 7. The skip list, row by row

Every row of [`tools/statehash_skip.txt`](../tools/statehash_skip.txt) (the
noise rows first, as the spec asks), with its row here:

| Skip row | Kind | Census row (section) |
|---|---|---|
| `0x6BC620 12` Game_Hwnd, Game_HInstance, Frame_Deadline | noise | SN keep (5); `Frame_Deadline` is the original's float, unwritten by ours (DIV-0047) |
| `0x6BC928 5400` Sound_Banks | noise | SP-A heap + DirectSound (4, 5) |
| `0x6BDE44 4` the pointer before Music_FileSize | noise | `kStreamData`, the last stream's file: SP-A heap (4) |
| `0x6BDE50 4` Music_File | noise | SP-A heap (4) |
| `0x7CC378 73728` Music_Staging | noise | SP-A, refilled; the "own thread" is the main thread (6) |
| `0x7DE3B4 40` Music_Events .. Music_Data | noise | SP-A (5) |
| `0x7DE3E0 4` Music_Finished | noise | SP-A: set by the pump from the play cursor's timing; restored with the stream's position |
| `0x93C5A0 4`, `0x93C6B4 4`, `0x93D6E0 12` | noise | the CRT's words, dead in the exe host (5) |
| `0x6BC8C8 92` Sound_Channels | noise | SP-A (5) |
| `0x66C7D4`, `..F4`, `..814`, `..834` (4 each) | ours | the task records' saved `esp`: valid with the slices (2.1) |
| `0x66C854 37292` | both | `Task_SchedulerEsp` (keep), `Task_StackTop` (checked); **not the task stacks** - those are on the thread's stack (2.1); the rest unread |
| `0x6BDE5C 4` Music_Volume | both | span; SP-A sets the buffer's volume from it |
| `0x6BE1D0 2096` | ours | SP-G keep (5) |
| `0x6C3940 256` the end of Gfx_ClutRows | both | SP-G keep + reconvert (4) |
| `0x6C3A40 1280` | both | SP-G: state caches marked unknown (5) |
| `0x6C3F40 24576` Gfx_TexCache | both | SP-G clear (5) |
| `0x7C9F58..0x7CA944`, 128 rows of 8 | both | `Font_TexCache`'s surfaces: SP-G clear (5) |
| `0x7CA958 136` D3d_Vertices | ours | per-frame (the quad the draw builds) |
| `0x7CAE38 5440` | both | SP-G clear the cell cache, keep the objects (5) |
| `0x7DE7A8 128` Key_Table | ours | SN keep (5) |
| `0x7DE928 952` | both | SN keep (5) |
| `0x7E09E0 2048` DrawItemPool_Free (Capcom's) | ours | unused under DIV-0062; copied harmlessly |
| `0x7E1C00 131072` Gfx_PacketPools | ours | per-frame (rebuilt by the next frame's draw) |
| `0x8022A0 2720` DrawLayers, Gfx_FrameNodes | ours | per-frame |
| `0x9037D0 4`, `0x9037F0 4`, `0x929EB0 4`, `0x929EB8 4` | ours | pointers into `.pool`, inside the span: copied |
| `0x9039D4 2` DrawItemPool_Top | ours | span, copied with ST's free ring |
| `0x905E80 147456` DrawItems (Capcom's) | ours | unused under DIV-0062 |
| `0x6BDE54 4`, `0x6BDE60 4` Music_FadeCount / FadeStep | ours (DIV-0028) | span, with SP-A's `g_fade_frame` / `g_stop_unseen` |
| `0x7DE46E`, `..476`, `..47E` (2 each) | ours (DIV-0023) | span |
| `0x66A418 8`, `0x66A448 8` Pause_Lines | ours (DIV-0038) | pointers into our DLL (2.2) |
| `0x93C35C`, `0x93C380 516`, `0x93C6A0 12`, `0x93D6C0 4` | ours | the CRT's start-up data, dead in the exe host |

Not in the skip list but in this census, because the hash sees them alike
in two runs while they are still process-owned: `Font_GlyphData`,
`Gfx_ClutRows` below `0x6C3940`, `Overlay_Text`, `Cfg_*`, `Fps_Shown`,
`App_Active` (section 5) - heap and settings that happened to be equal run
to run.

## 8. What is not settled

- **The live check** (section 1): two `BOF3X_STATEHASH` runs of a route,
  hashed with the skip list, would only re-find the noise rows above; what
  it cannot see is exactly this census's subject (our statics, the heap, the
  thread's stack). The useful live check is SN's in-process proof, and its
  first misses will be the ST rows if they are left out: **the state hash
  should hash ST's record too** (at least `Rand`'s seed, which otherwise
  shows only when a draw lands in `.data`) - a suggestion for SN / RC.
- The stack base across a reboot and on another Windows (2.1): one machine,
  one boot measured.
- The DLL's base after a reboot from one directory: not measured; Windows
  re-randomises per boot, so assumed to move.
- Configuration that the player changes in play (F1 speed, F8 window mode,
  the FPS line) survives a quick-load by design; the save half's settings
  (the game's own Config screen, in the span) are restored with the span -
  as a load restores them.

## 9. For the owner

1. **The DLL's base** (2.2): link `bof3x.dll` for the exe host at a fixed
   base (a build change: no game behaviour, no ledger entry), or accept
   cross-process quick-loads only from one launcher directory within one
   boot (the header check refuses the rest, loudly). The census's
   recommendation is both.
2. Nothing else here needs a decision: the slices (2.1) and ST are design
   work for SN, the heap's two unrecorded sources work for SP-A.

## Appendix A: every object that is not tooling

Generated by `python tools/quicksave_census.py table` at `f7886490` + this
branch; the tooling objects are counted per file in appendix B.

| Module | Object | Class | Participant | Rebuild source / note |
|---|---|---|---|---|
| `audio/mp3.cpp` | `have_simd()::g_have_simd` | configuration | - | switch / CPU probe |
| `audio/mp3.cpp` | `mp3dec::g_active` | configuration | - | switch / CPU probe |
| `audio/mp3.cpp` | `mp3dec::g_stack` | configuration | - | platform: the decoder's 256 KB private stack (VirtualAlloc), its contents dead between calls |
| `audio/seq.cpp` | `guard variable for psx::Pitches()::t` | configuration | - | a local static's once-flag |
| `audio/seq.cpp` | `psx::Pitches()::t` | configuration | - | pitch table, built once |
| `audio/song.cpp` | `psx::g_abort_hook` | configuration | - | abort hook |
| `audio/spu.cpp` | `psx::g_abort_hook` | configuration | - | abort hook |
| `game/area_backdrop.cpp` | `area_backdrop::g` | configuration | - | callee table, bound at inject |
| `game/area_entry.cpp` | `area_entry::g` | configuration | - | callee table, bound at inject |
| `game/area_w4a.cpp` | `AreaW4a_Inject()::index` | configuration | - | inject table |
| `game/battle_actions.cpp` | `battle_actions::g` | configuration | - | callee table, bound at inject |
| `game/battle_actions.cpp` | `battle_actions::kOriginals` | configuration | - | callee table, bound at inject |
| `game/battle_actor_copies.cpp` | `battle_actor_copies::g` | configuration | - | callee table, bound at inject |
| `game/battle_actor_copies.cpp` | `battle_actor_copies::kOriginals` | configuration | - | callee table, bound at inject |
| `game/battle_damage.cpp` | `battle_damage::g` | configuration | - | callee table, bound at inject |
| `game/battle_damage.cpp` | `battle_damage::kOriginals` | configuration | - | callee table, bound at inject |
| `game/battle_draw.cpp` | `battle_draw::g` | configuration | - | callee table, bound at inject |
| `game/battle_draw.cpp` | `battle_draw::kOriginals` | configuration | - | callee table, bound at inject |
| `game/battle_draw.cpp` | `g_list_title_centre` | configuration | - | a PatchBytes / Operand switch or value, set at inject after the self-tests (ini / launcher / ledgered DIV) |
| `game/battle_draw.cpp` | `g_title_site_draw` | configuration | - | a PatchBytes / Operand switch or value, set at inject after the self-tests (ini / launcher / ledgered DIV) |
| `game/battle_e7.cpp` | `g_equip_labels_row` | configuration | - | a PatchBytes / Operand switch or value, set at inject after the self-tests (ini / launcher / ledgered DIV) |
| `game/battle_flow.cpp` | `battle_flow::g` | configuration | - | callee table, bound at inject |
| `game/battle_flow.cpp` | `battle_flow::kOriginals` | configuration | - | callee table, bound at inject |
| `game/battle_fx_tasks.cpp` | `battle_fx_tasks::g` | configuration | - | callee table, bound at inject |
| `game/battle_fx_tasks.cpp` | `battle_fx_tasks::kOriginals` | configuration | - | callee table, bound at inject |
| `game/battle_items.cpp` | `battle_items::g` | configuration | - | callee table, bound at inject |
| `game/battle_items.cpp` | `battle_items::kOriginals` | configuration | - | callee table, bound at inject |
| `game/battle_menu_states.cpp` | `battle_menu_states::g` | configuration | - | callee table, bound at inject |
| `game/battle_menu_states.cpp` | `battle_menu_states::kOriginals` | configuration | - | callee table, bound at inject |
| `game/battle_misc.cpp` | `battle_misc::g` | configuration | - | callee table, bound at inject |
| `game/battle_misc.cpp` | `battle_misc::kOriginals` | configuration | - | callee table, bound at inject |
| `game/battle_obj_states.cpp` | `battle_obj_states::g` | configuration | - | callee table, bound at inject |
| `game/battle_obj_states.cpp` | `battle_obj_states::kOriginals` | configuration | - | callee table, bound at inject |
| `game/battle_odds.cpp` | `battle_odds::g` | configuration | - | callee table, bound at inject |
| `game/battle_odds.cpp` | `battle_odds::kOriginals` | configuration | - | callee table, bound at inject |
| `game/battle_phases.cpp` | `battle_phases::g` | configuration | - | callee table, bound at inject |
| `game/battle_phases.cpp` | `battle_phases::kOriginals` | configuration | - | callee table, bound at inject |
| `game/battle_result.cpp` | `battle_result::g` | configuration | - | callee table, bound at inject |
| `game/battle_result.cpp` | `battle_result::kOriginals` | configuration | - | callee table, bound at inject |
| `game/battle_result.cpp` | `g_exp_layout_us` | configuration | - | a PatchBytes / Operand switch or value, set at inject after the self-tests (ini / launcher / ledgered DIV) |
| `game/battle_setup.cpp` | `battle_setup::g` | configuration | - | callee table, bound at inject |
| `game/battle_setup.cpp` | `battle_setup::kOriginals` | configuration | - | callee table, bound at inject |
| `game/battle_sprites.cpp` | `battle_sprites::g` | configuration | - | callee table, bound at inject |
| `game/battle_sprites.cpp` | `battle_sprites::kOriginals` | configuration | - | callee table, bound at inject |
| `game/battle_text.cpp` | `g_messages` | configuration | - | language overlay text (DIV-0038/0064/...); the span's pointer slots name it (section 5) |
| `game/battle_text.cpp` | `g_room` | configuration | - | language overlay text (DIV-0038/0064/...); the span's pointer slots name it (section 5) |
| `game/battle_turn_steps.cpp` | `battle_turn_steps::g` | configuration | - | callee table, bound at inject |
| `game/battle_turn_steps.cpp` | `battle_turn_steps::kOriginals` | configuration | - | callee table, bound at inject |
| `game/battle_win_states.cpp` | `battle_win_states::g` | configuration | - | callee table, bound at inject |
| `game/battle_win_states.cpp` | `g_banner_centre` | configuration | - | a PatchBytes / Operand switch or value, set at inject after the self-tests (ini / launcher / ledgered DIV) |
| `game/battle_window_draw.cpp` | `battle_window_draw::g` | configuration | - | callee table, bound at inject |
| `game/battle_windows.cpp` | `battle_windows::g` | configuration | - | callee table, bound at inject |
| `game/battle_windows.cpp` | `battle_windows::kOriginals` | configuration | - | callee table, bound at inject |
| `game/char_stats.cpp` | `char_stats::g` | configuration | - | callee table, bound at inject |
| `game/cheats.cpp` | `g_exp` | configuration | - | a PatchBytes / Operand switch or value, set at inject after the self-tests (ini / launcher / ledgered DIV) |
| `game/cheats.cpp` | `g_pilfer_mask` | configuration | - | a PatchBytes / Operand switch or value, set at inject after the self-tests (ini / launcher / ledgered DIV) |
| `game/cheats.cpp` | `g_steal_mask` | configuration | - | a PatchBytes / Operand switch or value, set at inject after the self-tests (ini / launcher / ledgered DIV) |
| `game/cheats.cpp` | `g_zenny` | configuration | - | a PatchBytes / Operand switch or value, set at inject after the self-tests (ini / launcher / ledgered DIV) |
| `game/config_text.cpp` | `ConfigText_Apply(unsigned int, unsigned char const*, unsigned int)::applied` | configuration | - | DIV-0015 Config screen text and operands (language overlay) |
| `game/config_text.cpp` | `g_anchor_now` | configuration | - | DIV-0015 Config screen text and operands (language overlay) |
| `game/config_text.cpp` | `g_box_now` | configuration | - | DIV-0015 Config screen text and operands (language overlay) |
| `game/config_text.cpp` | `g_cell2_now` | configuration | - | DIV-0015 Config screen text and operands (language overlay) |
| `game/config_text.cpp` | `g_ctrl` | configuration | - | DIV-0015 Config screen text and operands (language overlay) |
| `game/config_text.cpp` | `g_edge_now` | configuration | - | DIV-0015 Config screen text and operands (language overlay) |
| `game/config_text.cpp` | `g_esp_now` | configuration | - | DIV-0015 Config screen text and operands (language overlay) |
| `game/config_text.cpp` | `g_frame_now` | configuration | - | DIV-0015 Config screen text and operands (language overlay) |
| `game/config_text.cpp` | `g_icons` | configuration | - | DIV-0015 Config screen text and operands (language overlay) |
| `game/config_text.cpp` | `g_label_w_now` | configuration | - | DIV-0015 Config screen text and operands (language overlay) |
| `game/config_text.cpp` | `g_labels` | configuration | - | DIV-0015 Config screen text and operands (language overlay) |
| `game/config_text.cpp` | `g_name_w_now` | configuration | - | DIV-0015 Config screen text and operands (language overlay) |
| `game/config_text.cpp` | `g_ops` | configuration | - | DIV-0015 Config screen text and operands (language overlay) |
| `game/config_text.cpp` | `g_option_w_now` | configuration | - | DIV-0015 Config screen text and operands (language overlay) |
| `game/crt_rest.cpp` | `crt_rest::RandSeedCell()::cell` | configuration | - | which seed cell (ours or the CRT's) |
| `game/crt_rest.cpp` | `crt_rest::g_seed` | game state | ST | none - Rand's seed (MSVC holdrand) is ours since crt-rest 2.1: 4 bytes, copied |
| `game/crt_rest.cpp` | `guard variable for crt_rest::RandSeedCell()::cell` | configuration | - | a local static's once-flag |
| `game/d3d_draw.cpp` | `d3d_draw::g` | configuration | - | callee table, bound at inject |
| `game/d3d_lines.cpp` | `d3d_lines::g_wide` | configuration | - | a PatchBytes / Operand switch or value, set at inject after the self-tests (ini / launcher / ledgered DIV) |
| `game/d3d_list.cpp` | `d3d_list::Operands()::d3d` | configuration | - | operand values |
| `game/d3d_list.cpp` | `d3d_list::Operands()::soft` | configuration | - | operand values |
| `game/d3d_list.cpp` | `d3d_list::g` | configuration | - | callee table, bound at inject |
| `game/d3d_list.cpp` | `d3d_list::kOriginals` | configuration | - | callee table, bound at inject |
| `game/d3d_rest.cpp` | `d3d_rest::g` | configuration | - | callee table, bound at inject |
| `game/d3d_rest.cpp` | `g_tile1_quad` | configuration | - | a PatchBytes / Operand switch or value, set at inject after the self-tests (ini / launcher / ledgered DIV) |
| `game/dat_cache.cpp` | `dat_cache::WalkShipped(char const*, void (*)(void*, dat_cache::Chunk const&, unsigned char*), void*)::base_path` | per-frame | - | path scratch inside one call |
| `game/dat_cache.cpp` | `dat_cache::WalkShipped(char const*, void (*)(void*, dat_cache::Chunk const&, unsigned char*), void*)::zh_path` | per-frame | - | path scratch inside one call |
| `game/dat_cache.cpp` | `dat_cache::g_c` | configuration | - | callee table, bound at inject |
| `game/dat_cache.cpp` | `dat_cache::kHash` | configuration | - | cache constants |
| `game/dat_cache.cpp` | `dat_cache::kMixWalk` | configuration | - | cache constants |
| `game/dat_load.cpp` | `WalkOverlay(char const*, char const*, bool, char const*, char const*)::cache` | per-frame | - | path scratch inside one call |
| `game/dat_load.cpp` | `g_area_block_loaded` | configuration | - | install / overlay / layer choice (BOF3X_LANG, BOF3X_OPT), fixed at start-up |
| `game/dat_load.cpp` | `g_install_has` | configuration | - | install / overlay / layer choice (BOF3X_LANG, BOF3X_OPT), fixed at start-up |
| `game/dat_load.cpp` | `g_io` | configuration | - | callee table, bound at inject |
| `game/dat_load.cpp` | `g_lang` | configuration | - | install / overlay / layer choice (BOF3X_LANG, BOF3X_OPT), fixed at start-up |
| `game/dat_load.cpp` | `g_lang_cached` | configuration | - | install / overlay / layer choice (BOF3X_LANG, BOF3X_OPT), fixed at start-up |
| `game/dat_load.cpp` | `g_opt` | configuration | - | install / overlay / layer choice (BOF3X_LANG, BOF3X_OPT), fixed at start-up |
| `game/dat_load.cpp` | `g_opt_banks` | configuration | - | install / overlay / layer choice (BOF3X_LANG, BOF3X_OPT), fixed at start-up |
| `game/dat_load.cpp` | `g_opt_cached` | configuration | - | install / overlay / layer choice (BOF3X_LANG, BOF3X_OPT), fixed at start-up |
| `game/dat_load.cpp` | `g_opt_count` | configuration | - | install / overlay / layer choice (BOF3X_LANG, BOF3X_OPT), fixed at start-up |
| `game/dat_load.cpp` | `g_take` | configuration | - | install / overlay / layer choice (BOF3X_LANG, BOF3X_OPT), fixed at start-up |
| `game/display_env.cpp` | `display_env::g` | configuration | - | callee table, bound at inject |
| `game/display_setup.cpp` | `DisplaySetup_Snap()::snap` | configuration | - | platform: backend up, scale k (DIV-0036) |
| `game/display_setup.cpp` | `g_backend_up` | configuration | - | platform: backend up, scale k (DIV-0036) |
| `game/display_setup.cpp` | `g_scale` | configuration | - | platform: backend up, scale k (DIV-0036) |
| `game/draw_pool.cpp` | `DrawPool_Grow()::bound_now` | configuration | - | DIV-0062 operands and the pool's place (.pool in the exe host) |
| `game/draw_pool.cpp` | `DrawPool_Grow()::free_list` | game state | ST | none - DIV-0062's 2,048-entry free ring; DrawItemPool_Top and the items (.pool) are in the span: 4 KB, copied with them |
| `game/draw_pool.cpp` | `DrawPool_Grow()::site_now` | configuration | - | DIV-0062 operands and the pool's place (.pool in the exe host) |
| `game/draw_pool.cpp` | `draw_pool::g_count` | configuration | - | DIV-0062 operands and the pool's place (.pool in the exe host) |
| `game/draw_pool.cpp` | `draw_pool::g_free` | configuration | - | DIV-0062 operands and the pool's place (.pool in the exe host) |
| `game/draw_pool.cpp` | `draw_pool::g_items` | configuration | - | DIV-0062 operands and the pool's place (.pool in the exe host) |
| `game/draw_pool.cpp` | `g_low_items` | configuration | - | DIV-0062 operands and the pool's place (.pool in the exe host) |
| `game/effect_1a.cpp` | `g_empty_line_none` | configuration | - | a PatchBytes / Operand switch or value, set at inject after the self-tests (ini / launcher / ledgered DIV) |
| `game/effect_1a.cpp` | `g_low_glyph_none` | configuration | - | a PatchBytes / Operand switch or value, set at inject after the self-tests (ini / launcher / ledgered DIV) |
| `game/effect_1e.cpp` | `g_fish_panel_duck` | configuration | - | a PatchBytes / Operand switch or value, set at inject after the self-tests (ini / launcher / ledgered DIV) |
| `game/effect_1e.cpp` | `g_shop_ask_row` | configuration | - | a PatchBytes / Operand switch or value, set at inject after the self-tests (ini / launcher / ledgered DIV) |
| `game/effect_1g.cpp` | `g_trade_leave_layout` | configuration | - | a PatchBytes / Operand switch or value, set at inject after the self-tests (ini / launcher / ledgered DIV) |
| `game/effect_gte.cpp` | `effect_gte::g_identity` | configuration | - | a PatchBytes / Operand switch or value, set at inject after the self-tests (ini / launcher / ledgered DIV) |
| `game/enemy_ai_ops.cpp` | `enemy_ai_ops::g` | configuration | - | callee table, bound at inject |
| `game/enemy_ai_ops.cpp` | `enemy_ai_ops::kOriginals` | configuration | - | callee table, bound at inject |
| `game/event_leader.cpp` | `event_leader::g` | configuration | - | callee table, bound at inject |
| `game/event_leader.cpp` | `event_leader::g_edge_no_area` | configuration | - | a PatchBytes / Operand switch or value, set at inject after the self-tests (ini / launcher / ledgered DIV) |
| `game/event_leader.cpp` | `event_leader::kOriginals` | configuration | - | callee table, bound at inject |
| `game/event_objs.cpp` | `event_objs::g` | configuration | - | callee table, bound at inject |
| `game/event_objs.cpp` | `event_objs::kOriginals` | configuration | - | callee table, bound at inject |
| `game/event_ops.cpp` | `event_ops::g` | configuration | - | callee table, bound at inject |
| `game/event_ops.cpp` | `event_ops::kOriginals` | configuration | - | callee table, bound at inject |
| `game/event_script.cpp` | `event_script::g` | configuration | - | callee table, bound at inject |
| `game/field_blocked.cpp` | `BlockingNibbles()::mask` | configuration | - | a table built on first use |
| `game/field_blocked.cpp` | `g` | configuration | - | callee table, bound at inject |
| `game/field_blocked.cpp` | `guard variable for BlockingNibbles()::mask` | configuration | - | a local static's once-flag |
| `game/field_e2.cpp` | `ConfirmLayout(unsigned char const*)::line` | per-frame | - | text scratch inside one call |
| `game/field_e2.cpp` | `g_trade_confirm_layout` | configuration | - | a PatchBytes / Operand switch or value, set at inject after the self-tests (ini / launcher / ledgered DIV) |
| `game/field_event.cpp` | `field_event::g` | configuration | - | callee table, bound at inject |
| `game/field_event.cpp` | `field_event::kOriginals` | configuration | - | callee table, bound at inject |
| `game/field_frame.cpp` | `g` | configuration | - | callee table, bound at inject |
| `game/field_frame.cpp` | `g_band` | per-frame | - | band sprites noted and put back within one Field_DrawFrame |
| `game/field_frame.cpp` | `g_band_added` | per-frame | - | band sprites noted and put back within one Field_DrawFrame |
| `game/field_frame.cpp` | `g_band_count` | per-frame | - | band sprites noted and put back within one Field_DrawFrame |
| `game/field_hidden.cpp` | `field_hidden::g` | configuration | - | callee table, bound at inject |
| `game/field_hidden.cpp` | `field_hidden::kOriginals` | configuration | - | callee table, bound at inject |
| `game/field_misc.cpp` | `field_misc::g` | configuration | - | callee table, bound at inject |
| `game/field_modes.cpp` | `field_modes::g` | configuration | - | callee table, bound at inject |
| `game/field_o.cpp` | `g_title_live` | configuration | - | a PatchBytes / Operand switch or value, set at inject after the self-tests (ini / launcher / ledgered DIV) |
| `game/field_o.cpp` | `g_wheel_name_centre` | configuration | - | a PatchBytes / Operand switch or value, set at inject after the self-tests (ini / launcher / ledgered DIV) |
| `game/field_objects.cpp` | `g` | configuration | - | callee table, bound at inject |
| `game/field_s.cpp` | `g_use_item_yes_no_hand` | configuration | - | a PatchBytes / Operand switch or value, set at inject after the self-tests (ini / launcher / ledgered DIV) |
| `game/fishing_text.cpp` | `g_disc_labels` | configuration | - | language overlay text (DIV-0038/0064/...); the span's pointer slots name it (section 5) |
| `game/fishing_text.cpp` | `g_disc_labels_armed` | configuration | - | language overlay text (DIV-0038/0064/...); the span's pointer slots name it (section 5) |
| `game/fishing_text.cpp` | `g_heads` | configuration | - | language overlay text (DIV-0038/0064/...); the span's pointer slots name it (section 5) |
| `game/fishing_text.cpp` | `g_image_how` | configuration | - | language overlay text (DIV-0038/0064/...); the span's pointer slots name it (section 5) |
| `game/fishing_text.cpp` | `g_image_release` | configuration | - | language overlay text (DIV-0038/0064/...); the span's pointer slots name it (section 5) |
| `game/fishing_text.cpp` | `g_label_sigma` | configuration | - | language overlay text (DIV-0038/0064/...); the span's pointer slots name it (section 5) |
| `game/fishing_text.cpp` | `g_lines` | configuration | - | language overlay text (DIV-0038/0064/...); the span's pointer slots name it (section 5) |
| `game/fishing_text.cpp` | `g_on` | configuration | - | language overlay text (DIV-0038/0064/...); the span's pointer slots name it (section 5) |
| `game/fishing_text.cpp` | `g_tabs` | configuration | - | language overlay text (DIV-0038/0064/...); the span's pointer slots name it (section 5) |
| `game/frame_callees.cpp` | `g` | configuration | - | callee table, bound at inject |
| `game/game_clock.cpp` | `g_base` | configuration | - | DIV-0022 wall clock (pacing only; no game logic reads it) - process-owned, kept |
| `game/game_clock.cpp` | `g_ours` | configuration | - | DIV-0022 wall clock (pacing only; no game logic reads it) - process-owned, kept |
| `game/game_clock.cpp` | `g_paused_at` | configuration | - | DIV-0022 wall clock (pacing only; no game logic reads it) - process-owned, kept |
| `game/game_clock.cpp` | `g_qpc_paused_at` | configuration | - | DIV-0022 wall clock (pacing only; no game logic reads it) - process-owned, kept |
| `game/game_clock.cpp` | `g_qpc_start` | configuration | - | DIV-0022 wall clock (pacing only; no game logic reads it) - process-owned, kept |
| `game/game_clock.cpp` | `g_qpf` | configuration | - | DIV-0022 wall clock (pacing only; no game logic reads it) - process-owned, kept |
| `game/game_clock.cpp` | `g_start` | configuration | - | DIV-0022 wall clock (pacing only; no game logic reads it) - process-owned, kept |
| `game/glyph_draw.cpp` | `glyph_draw::g` | configuration | - | callee table, bound at inject |
| `game/glyph_draw.cpp` | `glyph_draw::g_texel_inset` | configuration | - | a PatchBytes / Operand switch or value, set at inject after the self-tests (ini / launcher / ledgered DIV) |
| `game/inventory_ops.cpp` | `g_on_screen_lower_bounds` | configuration | - | a PatchBytes / Operand switch or value, set at inject after the self-tests (ini / launcher / ledgered DIV) |
| `game/inventory_ops.cpp` | `inventory_ops::g` | configuration | - | callee table, bound at inject |
| `game/inventory_ops.cpp` | `inventory_ops::kOriginals` | configuration | - | callee table, bound at inject |
| `game/item_use.cpp` | `item_use::g` | configuration | - | callee table, bound at inject |
| `game/item_use.cpp` | `item_use::kOriginals` | configuration | - | callee table, bound at inject |
| `game/kind2_object.cpp` | `g` | configuration | - | callee table, bound at inject |
| `game/labels.cpp` | `g_guest_name` | configuration | - | language overlay text (DIV-0038/0064/...); the span's pointer slots name it (section 5) |
| `game/labels.cpp` | `g_hilo_words` | configuration | - | language overlay text (DIV-0038/0064/...); the span's pointer slots name it (section 5) |
| `game/labels.cpp` | `g_identify_words` | configuration | - | language overlay text (DIV-0038/0064/...); the span's pointer slots name it (section 5) |
| `game/labels.cpp` | `g_item_types` | configuration | - | language overlay text (DIV-0038/0064/...); the span's pointer slots name it (section 5) |
| `game/labels.cpp` | `g_skill_types` | configuration | - | language overlay text (DIV-0038/0064/...); the span's pointer slots name it (section 5) |
| `game/labels.cpp` | `g_small_written` | configuration | - | language overlay text (DIV-0038/0064/...); the span's pointer slots name it (section 5) |
| `game/labels.cpp` | `g_sort_menus` | configuration | - | language overlay text (DIV-0038/0064/...); the span's pointer slots name it (section 5) |
| `game/labels.cpp` | `g_turn_words` | configuration | - | language overlay text (DIV-0038/0064/...); the span's pointer slots name it (section 5) |
| `game/labels.cpp` | `g_village_lists` | configuration | - | language overlay text (DIV-0038/0064/...); the span's pointer slots name it (section 5) |
| `game/labels.cpp` | `g_village_words` | configuration | - | language overlay text (DIV-0038/0064/...); the span's pointer slots name it (section 5) |
| `game/labels.cpp` | `g_written` | configuration | - | language overlay text (DIV-0038/0064/...); the span's pointer slots name it (section 5) |
| `game/layering.cpp` | `layering::g_ahead` | configuration | - | DIV-0071 mode and tuning (BOF3X_LAYERING*) |
| `game/layering.cpp` | `layering::g_cell_of` | per-frame | - | reset by Defer each pass |
| `game/layering.cpp` | `layering::g_in_run` | per-frame | - | reset by Defer each pass |
| `game/layering.cpp` | `layering::g_log` | configuration | - | BOF3X_LAYERING_LOG switch |
| `game/layering.cpp` | `layering::g_mode` | configuration | - | DIV-0071 mode and tuning (BOF3X_LAYERING*) |
| `game/layering.cpp` | `layering::g_moved` | per-frame | - | reset by Defer each pass |
| `game/layering.cpp` | `layering::g_moved_n` | per-frame | - | reset by Defer each pass |
| `game/layering.cpp` | `layering::g_pass` | per-frame | SP-G | one pass of history (runs stamped g_pass - 1): SP-G invalidates (g_pass += 2) so the first pass after a restore uses the proximity test |
| `game/layering.cpp` | `layering::g_pending` | per-frame | - | reset by Defer each pass |
| `game/layering.cpp` | `layering::g_pending_n` | per-frame | - | reset by Defer each pass |
| `game/layering.cpp` | `layering::g_rise` | configuration | - | DIV-0071 mode and tuning (BOF3X_LAYERING*) |
| `game/layering.cpp` | `layering::g_run` | per-frame | - | reset by Defer each pass |
| `game/layering.cpp` | `layering::g_run_boxes` | configuration | - | DIV-0071 mode and tuning (BOF3X_LAYERING*) |
| `game/layering.cpp` | `layering::g_runs` | per-frame | SP-G | one pass of history (runs stamped g_pass - 1): SP-G invalidates (g_pass += 2) so the first pass after a restore uses the proximity test |
| `game/layering.cpp` | `layering::g_runs_before` | per-frame | SP-G | one pass of history (runs stamped g_pass - 1): SP-G invalidates (g_pass += 2) so the first pass after a restore uses the proximity test |
| `game/layering.cpp` | `layering::g_runs_now` | per-frame | SP-G | one pass of history (runs stamped g_pass - 1): SP-G invalidates (g_pass += 2) so the first pass after a restore uses the proximity test |
| `game/layering.cpp` | `layering::g_why` | per-frame | - | reset by Defer each pass |
| `game/magic_fx_reached.cpp` | `magic_fx_reached::g` | configuration | - | callee table, bound at inject |
| `game/magic_fx_reached.cpp` | `magic_fx_reached::kOriginals` | configuration | - | callee table, bound at inject |
| `game/magic_s02.cpp` | `MagicS02_Inject()::index` | configuration | - | inject table |
| `game/magic_s20.cpp` | `MagicS20_Inject()::index` | configuration | - | inject table |
| `game/map_field_objects.cpp` | `map_field_objects::g` | configuration | - | callee table, bound at inject |
| `game/map_field_objects.cpp` | `map_field_objects::kOriginals` | configuration | - | callee table, bound at inject |
| `game/map_layers.cpp` | `g_file_sides` | game state | SP-G | DIV-0085: side faces as the area file loaded them - not rebuildable from the patched arena; copied, 64 KB |
| `game/map_layers.cpp` | `g_side_dup` | configuration | - | DIV-0085 switches |
| `game/map_layers.cpp` | `g_side_zero_release` | configuration | - | DIV-0085 switches |
| `game/map_layers.cpp` | `map_layers::SnapshotSides()::next_used` | per-frame | - | scratch of one SnapshotSides |
| `game/map_layers.cpp` | `map_layers::SnapshotSides()::used` | per-frame | - | scratch of one SnapshotSides |
| `game/map_layers.cpp` | `map_layers::g` | configuration | - | callee table, bound at inject |
| `game/map_scroll.cpp` | `map_scroll::g` | configuration | - | callee table, bound at inject |
| `game/member_sprites.cpp` | `member_sprites::g` | configuration | - | callee table, bound at inject |
| `game/member_sprites.cpp` | `member_sprites::kOriginals` | configuration | - | callee table, bound at inject |
| `game/menu_draw_helpers.cpp` | `g_shop_yes_no_line` | configuration | - | a PatchBytes / Operand switch or value, set at inject after the self-tests (ini / launcher / ledgered DIV) |
| `game/menu_draw_helpers.cpp` | `menu_draw_helpers::g` | configuration | - | callee table, bound at inject |
| `game/menu_draw_helpers.cpp` | `menu_draw_helpers::kOriginals` | configuration | - | callee table, bound at inject |
| `game/menu_frame.cpp` | `g_frame_draw` | configuration | - | a PatchBytes / Operand switch or value, set at inject after the self-tests (ini / launcher / ledgered DIV) |
| `game/menu_lists.cpp` | `g_title_centre` | configuration | - | a PatchBytes / Operand switch or value, set at inject after the self-tests (ini / launcher / ledgered DIV) |
| `game/menu_lists.cpp` | `menu_lists::g` | configuration | - | callee table, bound at inject |
| `game/menu_lists.cpp` | `menu_lists::kOriginals` | configuration | - | callee table, bound at inject |
| `game/menu_verbs.cpp` | `g_label_call` | configuration | - | a PatchBytes / Operand switch or value, set at inject after the self-tests (ini / launcher / ledgered DIV) |
| `game/menu_windows.cpp` | `menu_windows::g` | configuration | - | callee table, bound at inject |
| `game/menu_windows.cpp` | `menu_windows::g_yes_no_div` | configuration | - | a PatchBytes / Operand switch or value, set at inject after the self-tests (ini / launcher / ledgered DIV) |
| `game/menu_windows.cpp` | `menu_windows::kOriginals` | configuration | - | callee table, bound at inject |
| `game/mode_flow.cpp` | `mode_flow::g` | configuration | - | callee table, bound at inject |
| `game/mode_flow.cpp` | `mode_flow::g_area_track_fallback` | configuration | - | a PatchBytes / Operand switch or value, set at inject after the self-tests (ini / launcher / ledgered DIV) |
| `game/mode_flow.cpp` | `mode_flow::kOriginals` | configuration | - | callee table, bound at inject |
| `game/mode_states.cpp` | `mode_states::g` | configuration | - | callee table, bound at inject |
| `game/mode_states.cpp` | `mode_states::kOriginals` | configuration | - | callee table, bound at inject |
| `game/mode_tasks.cpp` | `g` | configuration | - | callee table, bound at inject |
| `game/move_cmds.cpp` | `move_cmds::g` | configuration | - | callee table, bound at inject |
| `game/move_groups.cpp` | `g` | configuration | - | callee table, bound at inject |
| `game/move_script.cpp` | `g` | configuration | - | callee table, bound at inject |
| `game/msg_pool.cpp` | `MsgPool_Relocate()::pool` | game state | ST | the area's DAT (the 0x4000 chunk) - copied, 32 KB; under a language overlay only (DIV-0007) |
| `game/msg_pool.cpp` | `g_pool` | configuration | - | where Msg_SystemPtr reads (arena or the relocated pool), set at start-up |
| `game/msgbox.cpp` | `msgbox::g` | configuration | - | callee table, bound at inject |
| `game/msgbox.cpp` | `msgbox::g_effect_space_skips` | configuration | - | a PatchBytes / Operand switch or value, set at inject after the self-tests (ini / launcher / ledgered DIV) |
| `game/music_loops.cpp` | `music_loops::g_on` | configuration | - | a PatchBytes / Operand switch or value, set at inject after the self-tests (ini / launcher / ledgered DIV) |
| `game/music_seq.cpp` | `music_seq::g_parsing` | per-frame | - | the file being parsed, null between calls |
| `game/music_seq.cpp` | `music_seq::g_s` | game state | SP-A | DIV-0087 song/bank/synth (heap): LoadFile(track) + FastForward(sample) |
| `game/object_kinds.cpp` | `g` | configuration | - | callee table, bound at inject |
| `game/pad_read.cpp` | `g_layout` | configuration | - | pad layout and map (ini / BOF3X_PAD) |
| `game/pad_read.cpp` | `g_pad_map` | configuration | - | pad layout and map (ini / BOF3X_PAD) |
| `game/pause_text.cpp` | `g_applied` | configuration | - | language overlay text (DIV-0038/0064/...); the span's pointer slots name it (section 5) |
| `game/pause_text.cpp` | `g_text` | configuration | - | language overlay text (DIV-0038/0064/...); the span's pointer slots name it (section 5) |
| `game/psp_code.cpp` | `psp_code::g_on` | configuration | - | a PatchBytes / Operand switch or value, set at inject after the self-tests (ini / launcher / ledgered DIV) |
| `game/psp_code.cpp` | `psp_code::g_psp_base` | configuration | - | a PatchBytes / Operand switch or value, set at inject after the self-tests (ini / launcher / ledgered DIV) |
| `game/psx_gte.cpp` | `PsxGte_Inject()::entries` | configuration | - | inject table |
| `game/psx_gte.cpp` | `guard variable for PsxGte_Inject()::entries` | configuration | - | a local static's once-flag |
| `game/psx_gte_float.cpp` | `PsxGteFloat_Inject()::entries` | configuration | - | inject table |
| `game/psx_gte_float.cpp` | `guard variable for PsxGteFloat_Inject()::entries` | configuration | - | a local static's once-flag |
| `game/psx_gte_matrix.cpp` | `PsxGteMatrix_Inject()::entries` | configuration | - | inject table |
| `game/psx_gte_matrix.cpp` | `guard variable for PsxGteMatrix_Inject()::entries` | configuration | - | a local static's once-flag |
| `game/psx_gte_transform.cpp` | `PsxGteTransform_Inject()::entries` | configuration | - | inject table |
| `game/psx_gte_transform.cpp` | `guard variable for PsxGteTransform_Inject()::entries` | configuration | - | a local static's once-flag |
| `game/psx_rest.cpp` | `psx_rest::g` | configuration | - | callee table, bound at inject |
| `game/rest_2c.cpp` | `g_summary_record0` | configuration | - | a PatchBytes / Operand switch or value, set at inject after the self-tests (ini / launcher / ledgered DIV) |
| `game/rest_2f.cpp` | `g_title_call` | configuration | - | a PatchBytes / Operand switch or value, set at inject after the self-tests (ini / launcher / ledgered DIV) |
| `game/rest_2h.cpp` | `rest_2h::g_master_bound` | configuration | - | a PatchBytes / Operand switch or value, set at inject after the self-tests (ini / launcher / ledgered DIV) |
| `game/rest_2h.cpp` | `rest_2h::g_master_title_live` | configuration | - | a PatchBytes / Operand switch or value, set at inject after the self-tests (ini / launcher / ledgered DIV) |
| `game/rest_2h.cpp` | `rest_2h::g_pupil_label_off` | configuration | - | a PatchBytes / Operand switch or value, set at inject after the self-tests (ini / launcher / ledgered DIV) |
| `game/rest_3e.cpp` | `dock::g_on` | configuration | - | a PatchBytes / Operand switch or value, set at inject after the self-tests (ini / launcher / ledgered DIV) |
| `game/rest_4d.cpp` | `g_entry_abandons` | configuration | - | a PatchBytes / Operand switch or value, set at inject after the self-tests (ini / launcher / ledgered DIV) |
| `game/save_menu.cpp` | `g_stream_missing` | game state | SP-A | DIV-0129: Sound_StreamDone answers 1 while set - logic-visible; saved |
| `game/save_menu.cpp` | `g_stream_started` | configuration | - | BOF3X_SPEED's hold only ("no game state", its comment); ST may clear it |
| `game/save_menu.cpp` | `save_menu::g` | configuration | - | callee table, bound at inject |
| `game/save_menu.cpp` | `save_menu::kOriginals` | configuration | - | callee table, bound at inject |
| `game/shell.cpp` | `g_cfg_own_table` | configuration | - | a PatchBytes / Operand switch or value, set at inject after the self-tests (ini / launcher / ledgered DIV) |
| `game/shell.cpp` | `g_cfg_table` | configuration | - | a PatchBytes / Operand switch or value, set at inject after the self-tests (ini / launcher / ledgered DIV) |
| `game/shell.cpp` | `shell::g` | configuration | - | callee table, bound at inject |
| `game/shell.cpp` | `shell::kOriginals` | configuration | - | callee table, bound at inject |
| `game/shop_states.cpp` | `shop_states::g` | configuration | - | callee table, bound at inject |
| `game/shop_states2.cpp` | `g_shop_yes_no_hand` | configuration | - | a PatchBytes / Operand switch or value, set at inject after the self-tests (ini / launcher / ledgered DIV) |
| `game/shop_states2.cpp` | `shop_states2::g` | configuration | - | callee table, bound at inject |
| `game/shoulder_glyphs.cpp` | `shoulder_glyphs::g_canvas` | configuration | - | wide-mode / glyph-sheet set-up, fixed at arm |
| `game/shoulder_glyphs.cpp` | `shoulder_glyphs::g_glyphs` | configuration | - | wide-mode / glyph-sheet set-up, fixed at arm |
| `game/shoulder_glyphs.cpp` | `shoulder_glyphs::g_on` | configuration | - | wide-mode / glyph-sheet set-up, fixed at arm |
| `game/shoulder_glyphs.cpp` | `shoulder_glyphs::g_page` | configuration | - | wide-mode / glyph-sheet set-up, fixed at arm |
| `game/shoulder_glyphs.cpp` | `shoulder_glyphs::g_page_art` | configuration | - | wide-mode / glyph-sheet set-up, fixed at arm |
| `game/shoulder_glyphs.cpp` | `shoulder_glyphs::g_pages` | configuration | - | wide-mode / glyph-sheet set-up, fixed at arm |
| `game/sound.cpp` | `ResumeSkip(unsigned int, int, int, unsigned int)::scratch` | per-frame | - | decode scratch inside one call |
| `game/sound.cpp` | `g_loop` | game state | SP-A | the measured loop's row and position: reopen + ResumeSkip |
| `game/sound.cpp` | `g_resume` | game state | SP-A | DIV-0092 remembered / pending area-track position: saved as numbers |
| `game/sound.cpp` | `g_ring` | game state | SP-A | DIV-0092 ring bookkeeping: rebuilt by Music_Start + ResumeSkip |
| `game/sound.cpp` | `guard variable for sound::MusicLogOn()::on` | configuration | - | a local static's once-flag |
| `game/sound.cpp` | `sound::MusicLogOn()::on` | configuration | - | switch / log flag |
| `game/sound.cpp` | `sound::g` | configuration | - | callee table, bound at inject |
| `game/sound.cpp` | `sound::g_fade_frame` | game state | SP-A | DIV-0028 fade state beside Music_FadeCount/Step (span): saved |
| `game/sound.cpp` | `sound::g_fade_per_frame` | configuration | - | switch / log flag |
| `game/sound.cpp` | `sound::g_stop_unseen` | game state | SP-A | DIV-0028 fade state beside Music_FadeCount/Step (span): saved |
| `game/sound.cpp` | `sound::g_unducked` | game state | SP-A | DIV-0132 duck outstanding: saved |
| `game/sound_rest.cpp` | `SoundRest_g` | configuration | - | callee table, bound at inject |
| `game/sprite_draw.cpp` | `g_sprite_capture` | configuration | - | a PatchBytes / Operand switch or value, set at inject after the self-tests (ini / launcher / ledgered DIV) |
| `game/sprite_pose.cpp` | `sprite_pose::g` | configuration | - | callee table, bound at inject |
| `game/sprite_screen.cpp` | `g` | configuration | - | callee table, bound at inject |
| `game/sprt_draw.cpp` | `sprt_draw::g` | configuration | - | callee table, bound at inject |
| `game/sprt_draw.cpp` | `sprt_draw::g_far` | configuration | - | wide-mode / glyph-sheet set-up, fixed at arm |
| `game/sprt_draw.cpp` | `sprt_draw::g_far_base` | configuration | - | wide-mode / glyph-sheet set-up, fixed at arm |
| `game/tex_cells.cpp` | `tex_cells::g` | configuration | - | callee table, bound at inject |
| `game/tex_page.cpp` | `tex_page::g` | configuration | - | callee table, bound at inject |
| `game/text_advance.cpp` | `g_advances` | configuration | - | DIV-0006 advance table (heap block of ours), from the overlay |
| `game/text_advance.cpp` | `g_count` | configuration | - | DIV-0006 advance table (heap block of ours), from the overlay |
| `game/text_advance.cpp` | `g_space` | configuration | - | DIV-0006 advance table (heap block of ours), from the overlay |
| `game/text_draw.cpp` | `g_bt` | configuration | - | a PatchBytes / Operand switch or value, set at inject after the self-tests (ini / launcher / ledgered DIV) |
| `game/text_draw.cpp` | `g_button_text` | configuration | - | a PatchBytes / Operand switch or value, set at inject after the self-tests (ini / launcher / ledgered DIV) |
| `game/text_draw.cpp` | `g_emit` | configuration | - | a PatchBytes / Operand switch or value, set at inject after the self-tests (ini / launcher / ledgered DIV) |
| `game/text_draw.cpp` | `g_glyph_count` | configuration | - | a PatchBytes / Operand switch or value, set at inject after the self-tests (ini / launcher / ledgered DIV) |
| `game/text_draw.cpp` | `g_glyph_limit` | configuration | - | a PatchBytes / Operand switch or value, set at inject after the self-tests (ini / launcher / ledgered DIV) |
| `game/text_immediate.cpp` | `g_draw_at` | configuration | - | a PatchBytes / Operand switch or value, set at inject after the self-tests (ini / launcher / ledgered DIV) |
| `game/text_pairs.cpp` | `g_count` | configuration | - | pair-code table (heap block of ours), from the overlay |
| `game/text_pairs.cpp` | `g_first` | configuration | - | pair-code table (heap block of ours), from the overlay |
| `game/text_pairs.cpp` | `g_pairs` | configuration | - | pair-code table (heap block of ours), from the overlay |
| `game/title_menu.cpp` | `g_now` | configuration | - | a PatchBytes / Operand switch or value, set at inject after the self-tests (ini / launcher / ledgered DIV) |
| `game/title_states.cpp` | `g` | configuration | - | callee table, bound at inject |
| `game/title_states.cpp` | `g_psp_title` | configuration | - | a PatchBytes / Operand switch or value, set at inject after the self-tests (ini / launcher / ledgered DIV) |
| `game/wide_rand.cpp` | `wide_rand::g_band` | per-frame | - | set and cleared around one DrawLayer_Open |
| `game/wide_rand.cpp` | `wide_rand::g_noted` | game state | SP-G | the last inset NoteInset saw; copied (wide only; picture) |
| `game/wide_rand.cpp` | `wide_rand::g_original` | game state | SP-G | the last inset NoteInset saw; copied (wide only; picture) |
| `game/wide_rand.cpp` | `wide_rand::g_seed` | game state | SP-G | DIV-0041 random split's draw-only generator: copied (wide only; picture) |
| `game/wide_rand.cpp` | `wide_rand::g_written` | game state | SP-G | the last inset NoteInset saw; copied (wide only; picture) |
| `game/wide_staging.cpp` | `wide_staging::g_case_on` | configuration | - | wide-mode / glyph-sheet set-up, fixed at arm |
| `game/wide_staging.cpp` | `wide_staging::g_columns` | configuration | - | wide-mode / glyph-sheet set-up, fixed at arm |
| `game/wide_staging.cpp` | `wide_staging::g_fast_index` | configuration | - | wide-mode / glyph-sheet set-up, fixed at arm |
| `game/wide_staging.cpp` | `wide_staging::g_hide` | configuration | - | wide-mode / glyph-sheet set-up, fixed at arm |
| `game/wide_staging.cpp` | `wide_staging::g_log_all` | configuration | - | wide-mode / glyph-sheet set-up, fixed at arm |
| `game/wide_staging.cpp` | `wide_staging::g_site_on` | configuration | - | wide-mode / glyph-sheet set-up, fixed at arm |
| `game/wide_staging.cpp` | `wide_staging::g_staging` | configuration | - | wide-mode / glyph-sheet set-up, fixed at arm |
| `game/widescreen.cpp` | `Widescreen_Columns()::columns` | configuration | - | wide-mode / glyph-sheet set-up, fixed at arm |
| `game/widescreen.cpp` | `Widescreen_SlideLogic(short)::control` | configuration | - | wide-mode / glyph-sheet set-up, fixed at arm |
| `game/widescreen.cpp` | `Widescreen_TerrainHi` | configuration | - | wide-mode / glyph-sheet set-up, fixed at arm |
| `game/widescreen.cpp` | `Widescreen_TerrainLo` | configuration | - | wide-mode / glyph-sheet set-up, fixed at arm |
| `game/widescreen.cpp` | `g_backdrop_pairs` | configuration | - | wide-mode / glyph-sheet set-up, fixed at arm |
| `game/widescreen.cpp` | `g_fills_armed` | configuration | - | wide-mode / glyph-sheet set-up, fixed at arm |
| `game/widescreen.cpp` | `g_inset_columns` | configuration | - | wide-mode / glyph-sheet set-up, fixed at arm |
| `game/widescreen.cpp` | `g_live` | configuration | - | wide-mode / glyph-sheet set-up, fixed at arm |
| `game/widescreen.cpp` | `g_narrow_hi` | configuration | - | wide-mode / glyph-sheet set-up, fixed at arm |
| `game/widescreen.cpp` | `g_narrow_lo` | configuration | - | wide-mode / glyph-sheet set-up, fixed at arm |
| `game/widescreen.cpp` | `g_sites` | configuration | - | wide-mode / glyph-sheet set-up, fixed at arm |
| `game/widescreen.cpp` | `g_slide_armed` | configuration | - | wide-mode / glyph-sheet set-up, fixed at arm |
| `game/widescreen.cpp` | `g_slide_switch` | configuration | - | wide-mode / glyph-sheet set-up, fixed at arm |
| `game/widescreen.cpp` | `g_slides` | configuration | - | wide-mode / glyph-sheet set-up, fixed at arm |
| `game/widescreen.cpp` | `g_split` | game state | SP-G | DIV-0041 faster slide: 22 window records' split; copied (wide only) |
| `game/widescreen.cpp` | `g_wide_hi` | configuration | - | wide-mode / glyph-sheet set-up, fixed at arm |
| `game/widescreen.cpp` | `g_wide_lo` | configuration | - | wide-mode / glyph-sheet set-up, fixed at arm |
| `game/widescreen.cpp` | `guard variable for Widescreen_SlideLogic(short)::control` | configuration | - | a local static's once-flag |
| `game/win_main.cpp` | `g_background` | configuration | - | window placement, focus, DIV-0047 period, DIV-0048 F1 speed - the player's / the process's, kept |
| `game/win_main.cpp` | `g_foreground` | configuration | - | window placement, focus, DIV-0047 period, DIV-0048 F1 speed - the player's / the process's, kept |
| `game/win_main.cpp` | `g_frame_ms` | configuration | - | window placement, focus, DIV-0047 period, DIV-0048 F1 speed - the player's / the process's, kept |
| `game/win_main.cpp` | `g_have_windowed_rect` | configuration | - | window placement, focus, DIV-0047 period, DIV-0048 F1 speed - the player's / the process's, kept |
| `game/win_main.cpp` | `g_loop_ours` | configuration | - | window placement, focus, DIV-0047 period, DIV-0048 F1 speed - the player's / the process's, kept |
| `game/win_main.cpp` | `g_speed` | configuration | - | window placement, focus, DIV-0047 period, DIV-0048 F1 speed - the player's / the process's, kept |
| `game/win_main.cpp` | `g_windowed_rect` | configuration | - | window placement, focus, DIV-0047 period, DIV-0048 F1 speed - the player's / the process's, kept |
| `game/window_kinds.cpp` | `window_kinds::g` | configuration | - | callee table, bound at inject |
| `game/window_kinds.cpp` | `window_kinds::kOriginals` | configuration | - | callee table, bound at inject |
| `game/window_task.cpp` | `g_keep_stopping_none` | configuration | - | a PatchBytes / Operand switch or value, set at inject after the self-tests (ini / launcher / ledgered DIV) |
| `game/window_task.cpp` | `window_task::g` | configuration | - | callee table, bound at inject |
| `game/window_task.cpp` | `window_task::kOriginals` | configuration | - | callee table, bound at inject |
| `game/world_map.cpp` | `KeepDimCluts()::written` | configuration | - | once a process (log) |
| `game/world_map.cpp` | `g_legend_on` | configuration | - | a PatchBytes / Operand switch or value, set at inject after the self-tests (ini / launcher / ledgered DIV) |
| `game/world_map.cpp` | `g_standin` | per-frame | - | set by Begin each frame |
| `game/world_map.cpp` | `world_map::g` | configuration | - | callee table, bound at inject |
| `game/world_map.cpp` | `world_map::kOriginals` | configuration | - | callee table, bound at inject |
| `game/worldmap_area.cpp` | `worldmap_area::g` | configuration | - | callee table, bound at inject |
| `game/worldmap_area.cpp` | `worldmap_area::kOriginals` | configuration | - | callee table, bound at inject |
| `game/yes_no_layout.cpp` | `g_line` | configuration | - | a PatchBytes / Operand switch or value, set at inject after the self-tests (ini / launcher / ledgered DIV) |
| `game/yes_no_layout.cpp` | `g_line_call` | configuration | - | a PatchBytes / Operand switch or value, set at inject after the self-tests (ini / launcher / ledgered DIV) |
| `game/yes_no_layout.cpp` | `g_master_hand_call` | configuration | - | a PatchBytes / Operand switch or value, set at inject after the self-tests (ini / launcher / ledgered DIV) |
| `game/yes_no_layout.cpp` | `g_master_line` | configuration | - | a PatchBytes / Operand switch or value, set at inject after the self-tests (ini / launcher / ledgered DIV) |
| `game/yes_no_layout.cpp` | `g_master_line_call` | configuration | - | a PatchBytes / Operand switch or value, set at inject after the self-tests (ini / launcher / ledgered DIV) |
| `game/yes_no_layout.cpp` | `g_master_stop` | configuration | - | a PatchBytes / Operand switch or value, set at inject after the self-tests (ini / launcher / ledgered DIV) |
| `game/yes_no_layout.cpp` | `g_name_inset` | configuration | - | a PatchBytes / Operand switch or value, set at inject after the self-tests (ini / launcher / ledgered DIV) |
| `game/yes_no_layout.cpp` | `g_stops` | configuration | - | a PatchBytes / Operand switch or value, set at inject after the self-tests (ini / launcher / ledgered DIV) |
| `game/yes_no_layout.cpp` | `g_stops_moved` | configuration | - | a PatchBytes / Operand switch or value, set at inject after the self-tests (ini / launcher / ledgered DIV) |
| `game/yes_no_layout.cpp` | `g_tail` | configuration | - | a PatchBytes / Operand switch or value, set at inject after the self-tests (ini / launcher / ledgered DIV) |
| `hook/detour.cpp` | `bof3::g_disabled` | configuration | - | inject / host tables, fixed at start-up |
| `hook/detour.cpp` | `bof3::g_enabled` | configuration | - | inject / host tables, fixed at start-up |
| `hook/detour.cpp` | `bof3::g_owned` | configuration | - | inject / host tables, fixed at start-up |
| `hook/detour.cpp` | `bof3::g_owned_on` | configuration | - | inject / host tables, fixed at start-up |
| `hook/exe_host.cpp` | `Engine_Main::entered` | configuration | - | inject / host tables, fixed at start-up |
| `hook/exe_host.cpp` | `bof3::CheckStub()::mine` | configuration | - | inject / host tables, fixed at start-up |
| `hook/exe_host.cpp` | `bof3::g_data` | configuration | - | inject / host tables, fixed at start-up |
| `hook/exe_host.cpp` | `bof3::g_exe` | configuration | - | inject / host tables, fixed at start-up |
| `hook/exe_host.cpp` | `bof3::g_rdata` | configuration | - | inject / host tables, fixed at start-up |
| `hook/exe_host.cpp` | `bof3::g_text` | configuration | - | inject / host tables, fixed at start-up |
| `hook/exe_icon.cpp` | `bof3::g_have_last` | configuration | - | platform: DIV-0095 icon, made once from the VRAM shadow |
| `hook/exe_icon.cpp` | `bof3::g_last` | configuration | - | platform: DIV-0095 icon, made once from the VRAM shadow |
| `hook/exe_icon.cpp` | `bof3::g_record` | configuration | - | platform: DIV-0095 icon, made once from the VRAM shadow |
| `hook/exe_icon.cpp` | `bof3::g_state` | configuration | - | platform: DIV-0095 icon, made once from the VRAM shadow |
| `hook/exe_image.cpp` | `bof3::g_disc_mapped` | configuration | - | inject / host tables, fixed at start-up |
| `hook/exe_image.cpp` | `bof3::g_mapped` | configuration | - | inject / host tables, fixed at start-up |
| `hook/exe_own.cpp` | `bof3::g_regions` | configuration | - | inject / host tables, fixed at start-up |
| `hook/host.cpp` | `bof3::g_host` | configuration | - | inject / host tables, fixed at start-up |
| `hook/host.cpp` | `bof3::g_known` | configuration | - | inject / host tables, fixed at start-up |
| `input/bindings.cpp` | `bof3x::input::DefaultKeys()::keys` | configuration | - | platform: the SDL gamepad handle, layout, default keys |
| `input/bindings.cpp` | `guard variable for bof3x::input::DefaultKeys()::keys` | configuration | - | a local static's once-flag |
| `input/pad_sdl.cpp` | `bof3x::input::g_layout` | configuration | - | platform: the SDL gamepad handle, layout, default keys |
| `input/pad_sdl.cpp` | `bof3x::input::g_nintendo` | configuration | - | platform: the SDL gamepad handle, layout, default keys |
| `input/pad_sdl.cpp` | `bof3x::input::g_pad` | configuration | - | platform: the SDL gamepad handle, layout, default keys |
| `input/pad_sdl.cpp` | `bof3x::input::g_pad_id` | configuration | - | platform: the SDL gamepad handle, layout, default keys |
| `input/pad_sdl.cpp` | `bof3x::input::g_started` | configuration | - | platform: the SDL gamepad handle, layout, default keys |
| `input/sigma.cpp` | `bof3x::input::sigma::g_armed` | configuration | - | DIV-0133/0134 switches and the install's release, fixed at Arm |
| `input/sigma.cpp` | `bof3x::input::sigma::g_by_source` | configuration | - | DIV-0133/0134 switches and the install's release, fixed at Arm |
| `input/sigma.cpp` | `bof3x::input::sigma::g_release` | configuration | - | DIV-0133/0134 switches and the install's release, fixed at Arm |
| `input/sigma.cpp` | `bof3x::input::sigma::g_release_words` | game state | ST | DIV-0133/0134: whose words stand (release start vs a save's): saved, 1 byte |
| `input/sigma.cpp` | `bof3x::input::sigma::g_sigma_on` | configuration | - | DIV-0133/0134 switches and the install's release, fixed at Arm |
| `input/sigma.cpp` | `bof3x::input::sigma::g_table` | game state | ST | derived from the Config words in the span: rebuilt by Follow after the span |
| `render/render_d3d11.cpp` | `render::g_blend_off` | configuration | - | platform: D3D11 device, targets, shaders, the render fiber - kept across a restore; textures live on render_shim surfaces |
| `render/render_d3d11.cpp` | `render::g_blends` | configuration | - | platform: D3D11 device, targets, shaders, the render fiber - kept across a restore; textures live on render_shim surfaces |
| `render/render_d3d11.cpp` | `render::g_cb_draw` | configuration | - | platform: D3D11 device, targets, shaders, the render fiber - kept across a restore; textures live on render_shim surfaces |
| `render/render_d3d11.cpp` | `render::g_cb_frame` | configuration | - | platform: D3D11 device, targets, shaders, the render fiber - kept across a restore; textures live on render_shim surfaces |
| `render/render_d3d11.cpp` | `render::g_ctx` | configuration | - | platform: D3D11 device, targets, shaders, the render fiber - kept across a restore; textures live on render_shim surfaces |
| `render/render_d3d11.cpp` | `render::g_device` | configuration | - | platform: D3D11 device, targets, shaders, the render fiber - kept across a restore; textures live on render_shim surfaces |
| `render/render_d3d11.cpp` | `render::g_frame_count` | configuration | - | platform: D3D11 device, targets, shaders, the render fiber - kept across a restore; textures live on render_shim surfaces |
| `render/render_d3d11.cpp` | `render::g_game_fiber` | configuration | - | platform: D3D11 device, targets, shaders, the render fiber - kept across a restore; textures live on render_shim surfaces |
| `render/render_d3d11.cpp` | `render::g_grab_slots` | configuration | - | platform: D3D11 device, targets, shaders, the render fiber - kept across a restore; textures live on render_shim surfaces |
| `render/render_d3d11.cpp` | `render::g_job` | configuration | - | platform: D3D11 device, targets, shaders, the render fiber - kept across a restore; textures live on render_shim surfaces |
| `render/render_d3d11.cpp` | `render::g_job_frame` | configuration | - | platform: D3D11 device, targets, shaders, the render fiber - kept across a restore; textures live on render_shim surfaces |
| `render/render_d3d11.cpp` | `render::g_job_ok` | configuration | - | platform: D3D11 device, targets, shaders, the render fiber - kept across a restore; textures live on render_shim surfaces |
| `render/render_d3d11.cpp` | `render::g_job_options` | configuration | - | platform: D3D11 device, targets, shaders, the render fiber - kept across a restore; textures live on render_shim surfaces |
| `render/render_d3d11.cpp` | `render::g_job_path` | configuration | - | platform: D3D11 device, targets, shaders, the render fiber - kept across a restore; textures live on render_shim surfaces |
| `render/render_d3d11.cpp` | `render::g_layout` | configuration | - | platform: D3D11 device, targets, shaders, the render fiber - kept across a restore; textures live on render_shim surfaces |
| `render/render_d3d11.cpp` | `render::g_look` | configuration | - | renderer options (ini) |
| `render/render_d3d11.cpp` | `render::g_main_thread` | configuration | - | platform: D3D11 device, targets, shaders, the render fiber - kept across a restore; textures live on render_shim surfaces |
| `render/render_d3d11.cpp` | `render::g_n_blends` | configuration | - | platform: D3D11 device, targets, shaders, the render fiber - kept across a restore; textures live on render_shim surfaces |
| `render/render_d3d11.cpp` | `render::g_opt` | configuration | - | renderer options (ini) |
| `render/render_d3d11.cpp` | `render::g_pad_x` | configuration | - | platform: D3D11 device, targets, shaders, the render fiber - kept across a restore; textures live on render_shim surfaces |
| `render/render_d3d11.cpp` | `render::g_pending_scale` | configuration | - | platform: D3D11 device, targets, shaders, the render fiber - kept across a restore; textures live on render_shim surfaces |
| `render/render_d3d11.cpp` | `render::g_pixel_offset` | configuration | - | renderer options (ini) |
| `render/render_d3d11.cpp` | `render::g_present_ps` | configuration | - | platform: D3D11 device, targets, shaders, the render fiber - kept across a restore; textures live on render_shim surfaces |
| `render/render_d3d11.cpp` | `render::g_present_vs` | configuration | - | platform: D3D11 device, targets, shaders, the render fiber - kept across a restore; textures live on render_shim surfaces |
| `render/render_d3d11.cpp` | `render::g_ps` | configuration | - | platform: D3D11 device, targets, shaders, the render fiber - kept across a restore; textures live on render_shim surfaces |
| `render/render_d3d11.cpp` | `render::g_raster` | configuration | - | platform: D3D11 device, targets, shaders, the render fiber - kept across a restore; textures live on render_shim surfaces |
| `render/render_d3d11.cpp` | `render::g_render_fiber` | configuration | - | platform: D3D11 device, targets, shaders, the render fiber - kept across a restore; textures live on render_shim surfaces |
| `render/render_d3d11.cpp` | `render::g_rescale_hook` | configuration | - | hook pointer |
| `render/render_d3d11.cpp` | `render::g_sampler_linear` | configuration | - | platform: D3D11 device, targets, shaders, the render fiber - kept across a restore; textures live on render_shim surfaces |
| `render/render_d3d11.cpp` | `render::g_sampler_point` | configuration | - | platform: D3D11 device, targets, shaders, the render fiber - kept across a restore; textures live on render_shim surfaces |
| `render/render_d3d11.cpp` | `render::g_sampler_present_linear` | configuration | - | platform: D3D11 device, targets, shaders, the render fiber - kept across a restore; textures live on render_shim surfaces |
| `render/render_d3d11.cpp` | `render::g_sampler_present_point` | configuration | - | platform: D3D11 device, targets, shaders, the render fiber - kept across a restore; textures live on render_shim surfaces |
| `render/render_d3d11.cpp` | `render::g_scale` | configuration | - | platform: D3D11 device, targets, shaders, the render fiber - kept across a restore; textures live on render_shim surfaces |
| `render/render_d3d11.cpp` | `render::g_scratch` | configuration | - | platform: D3D11 device, targets, shaders, the render fiber - kept across a restore; textures live on render_shim surfaces |
| `render/render_d3d11.cpp` | `render::g_scratch_bytes` | configuration | - | platform: D3D11 device, targets, shaders, the render fiber - kept across a restore; textures live on render_shim surfaces |
| `render/render_d3d11.cpp` | `render::g_swap` | configuration | - | platform: D3D11 device, targets, shaders, the render fiber - kept across a restore; textures live on render_shim surfaces |
| `render/render_d3d11.cpp` | `render::g_target` | configuration | - | platform: D3D11 device, targets, shaders, the render fiber - kept across a restore; textures live on render_shim surfaces |
| `render/render_d3d11.cpp` | `render::g_target_h` | configuration | - | platform: D3D11 device, targets, shaders, the render fiber - kept across a restore; textures live on render_shim surfaces |
| `render/render_d3d11.cpp` | `render::g_target_rtv` | configuration | - | platform: D3D11 device, targets, shaders, the render fiber - kept across a restore; textures live on render_shim surfaces |
| `render/render_d3d11.cpp` | `render::g_target_srv` | configuration | - | platform: D3D11 device, targets, shaders, the render fiber - kept across a restore; textures live on render_shim surfaces |
| `render/render_d3d11.cpp` | `render::g_target_w` | configuration | - | platform: D3D11 device, targets, shaders, the render fiber - kept across a restore; textures live on render_shim surfaces |
| `render/render_d3d11.cpp` | `render::g_vertex_capacity` | configuration | - | platform: D3D11 device, targets, shaders, the render fiber - kept across a restore; textures live on render_shim surfaces |
| `render/render_d3d11.cpp` | `render::g_vertices` | configuration | - | platform: D3D11 device, targets, shaders, the render fiber - kept across a restore; textures live on render_shim surfaces |
| `render/render_d3d11.cpp` | `render::g_vs` | configuration | - | platform: D3D11 device, targets, shaders, the render fiber - kept across a restore; textures live on render_shim surfaces |
| `render/render_d3d11.cpp` | `render::g_window_h` | configuration | - | platform: D3D11 device, targets, shaders, the render fiber - kept across a restore; textures live on render_shim surfaces |
| `render/render_d3d11.cpp` | `render::g_window_rtv` | configuration | - | platform: D3D11 device, targets, shaders, the render fiber - kept across a restore; textures live on render_shim surfaces |
| `render/render_d3d11.cpp` | `render::g_window_w` | configuration | - | platform: D3D11 device, targets, shaders, the render fiber - kept across a restore; textures live on render_shim surfaces |
| `render/render_shim.cpp` | `render::BuildTables()::built` | configuration | SP-G | platform: the DirectDraw/D3D-shaped singletons and pipe state; the span names their addresses (section 5) |
| `render/render_shim.cpp` | `render::g_clear_color` | configuration | SP-G | platform: the DirectDraw/D3D-shaped singletons and pipe state; the span names their addresses (section 5) |
| `render/render_shim.cpp` | `render::g_dd` | configuration | SP-G | platform: the DirectDraw/D3D-shaped singletons and pipe state; the span names their addresses (section 5) |
| `render/render_shim.cpp` | `render::g_dd_vtable` | configuration | SP-G | platform: the DirectDraw/D3D-shaped singletons and pipe state; the span names their addresses (section 5) |
| `render/render_shim.cpp` | `render::g_device` | configuration | SP-G | platform: the DirectDraw/D3D-shaped singletons and pipe state; the span names their addresses (section 5) |
| `render/render_shim.cpp` | `render::g_device_vtable` | configuration | SP-G | platform: the DirectDraw/D3D-shaped singletons and pipe state; the span names their addresses (section 5) |
| `render/render_shim.cpp` | `render::g_frame` | configuration | SP-G | platform: the DirectDraw/D3D-shaped singletons and pipe state; the span names their addresses (section 5) |
| `render/render_shim.cpp` | `render::g_material` | configuration | SP-G | platform: the DirectDraw/D3D-shaped singletons and pipe state; the span names their addresses (section 5) |
| `render/render_shim.cpp` | `render::g_material_vtable` | configuration | SP-G | platform: the DirectDraw/D3D-shaped singletons and pipe state; the span names their addresses (section 5) |
| `render/render_shim.cpp` | `render::g_n_surfaces` | configuration | SP-G | platform: every surface made (process heap); the span's texture caches name them - SP-G releases the caches' surfaces and clears the records |
| `render/render_shim.cpp` | `render::g_present` | configuration | SP-G | platform: the DirectDraw/D3D-shaped singletons and pipe state; the span names their addresses (section 5) |
| `render/render_shim.cpp` | `render::g_state` | configuration | SP-G | platform: the pipe state the device holds; the span D3d_ShadeModeCache / Gfx_RenderFlags / D3d_BackMaterialHandle cache it - SP-G pushes the restored caches to it or marks them unknown |
| `render/render_shim.cpp` | `render::g_surface_vtable` | configuration | SP-G | platform: the DirectDraw/D3D-shaped singletons and pipe state; the span names their addresses (section 5) |
| `render/render_shim.cpp` | `render::g_surfaces` | configuration | SP-G | platform: every surface made (process heap); the span's texture caches name them - SP-G releases the caches' surfaces and clears the records |
| `render/render_shim.cpp` | `render::g_texture_vtable` | configuration | SP-G | platform: the DirectDraw/D3D-shaped singletons and pipe state; the span names their addresses (section 5) |
| `render/render_shim.cpp` | `render::g_viewport` | configuration | SP-G | platform: the DirectDraw/D3D-shaped singletons and pipe state; the span names their addresses (section 5) |
| `render/render_shim.cpp` | `render::g_viewport_vtable` | configuration | SP-G | platform: the DirectDraw/D3D-shaped singletons and pipe state; the span names their addresses (section 5) |
| `render/satpixie.cpp` | `render::g_accumulate` | configuration | - | platform: DIV-0043 look's textures and shaders |
| `render/satpixie.cpp` | `render::g_blur_h` | configuration | - | platform: DIV-0043 look's textures and shaders |
| `render/satpixie.cpp` | `render::g_blur_v` | configuration | - | platform: DIV-0043 look's textures and shaders |
| `render/satpixie.cpp` | `render::g_cb` | configuration | - | platform: DIV-0043 look's textures and shaders |
| `render/satpixie.cpp` | `render::g_flip` | per-frame | - | blur ping-pong |
| `render/satpixie.cpp` | `render::g_h` | configuration | - | platform: DIV-0043 look's textures and shaders |
| `render/satpixie.cpp` | `render::g_linear` | configuration | - | platform: DIV-0043 look's textures and shaders |
| `render/satpixie.cpp` | `render::g_p` | configuration | - | platform: DIV-0043 look's textures and shaders |
| `render/satpixie.cpp` | `render::g_picture` | configuration | - | platform: DIV-0043 look's textures and shaders |
| `render/satpixie.cpp` | `render::g_rtv` | configuration | - | platform: DIV-0043 look's textures and shaders |
| `render/satpixie.cpp` | `render::g_srv` | configuration | - | platform: DIV-0043 look's textures and shaders |
| `render/satpixie.cpp` | `render::g_tex` | configuration | - | platform: DIV-0043 look's textures and shaders |
| `render/satpixie.cpp` | `render::g_vs` | configuration | - | platform: DIV-0043 look's textures and shaders |
| `render/satpixie.cpp` | `render::g_w` | configuration | - | platform: DIV-0043 look's textures and shaders |

## Appendix B: tooling, counted by file

| Module (tooling only) | Objects |
|---|--:|
| `audio/mp3.cpp` | 2 |
| `game/area_011_fuzz.cpp` | 1 |
| `game/area_backdrop_fuzz.cpp` | 11 |
| `game/area_entry_fuzz.cpp` | 9 |
| `game/area_harness.cpp` | 25 |
| `game/area_slope.cpp` | 2 |
| `game/area_w0a_fuzz.cpp` | 5 |
| `game/area_w0b_fuzz.cpp` | 15 |
| `game/area_w0c_fuzz.cpp` | 1 |
| `game/area_w1a_fuzz.cpp` | 4 |
| `game/area_w1b_fuzz.cpp` | 19 |
| `game/area_w1c_fuzz.cpp` | 5 |
| `game/area_w1d_fuzz.cpp` | 4 |
| `game/area_w1e_fuzz.cpp` | 14 |
| `game/area_w1f_fuzz.cpp` | 5 |
| `game/area_w2a_fuzz.cpp` | 3 |
| `game/area_w2b_fuzz.cpp` | 19 |
| `game/area_w2c_fuzz.cpp` | 5 |
| `game/area_w2d_fuzz.cpp` | 2 |
| `game/area_w2e_fuzz.cpp` | 10 |
| `game/area_w2f_fuzz.cpp` | 5 |
| `game/area_w3a_fuzz.cpp` | 18 |
| `game/area_w3b_fuzz.cpp` | 18 |
| `game/area_w3c_fuzz.cpp` | 10 |
| `game/area_w3d_fuzz.cpp` | 5 |
| `game/area_w3e_fuzz.cpp` | 4 |
| `game/area_w3f_fuzz.cpp` | 7 |
| `game/area_w3g_fuzz.cpp` | 16 |
| `game/area_w4a_fuzz.cpp` | 15 |
| `game/area_w4b_fuzz.cpp` | 5 |
| `game/area_w4c_fuzz.cpp` | 6 |
| `game/area_w4d_fuzz.cpp` | 5 |
| `game/area_w4e_fuzz.cpp` | 2 |
| `game/area_w4f_fuzz.cpp` | 2 |
| `game/band_census.cpp` | 11 |
| `game/battle_actions_fuzz.cpp` | 15 |
| `game/battle_actor_copies_fuzz.cpp` | 12 |
| `game/battle_damage_fuzz.cpp` | 14 |
| `game/battle_draw_fuzz.cpp` | 19 |
| `game/battle_e1_fuzz.cpp` | 3 |
| `game/battle_e2_fuzz.cpp` | 8 |
| `game/battle_e3_fuzz.cpp` | 1 |
| `game/battle_e4_fuzz.cpp` | 6 |
| `game/battle_e5_fuzz.cpp` | 6 |
| `game/battle_e6_fuzz.cpp` | 15 |
| `game/battle_e7_fuzz.cpp` | 3 |
| `game/battle_flow_fuzz.cpp` | 12 |
| `game/battle_fx_tasks_fuzz.cpp` | 9 |
| `game/battle_items_fuzz.cpp` | 20 |
| `game/battle_menu_states_fuzz.cpp` | 10 |
| `game/battle_misc_fuzz.cpp` | 18 |
| `game/battle_obj_states_fuzz.cpp` | 11 |
| `game/battle_odds_fuzz.cpp` | 10 |
| `game/battle_phases_fuzz.cpp` | 9 |
| `game/battle_result_fuzz.cpp` | 11 |
| `game/battle_setup_fuzz.cpp` | 9 |
| `game/battle_sprites_fuzz.cpp` | 11 |
| `game/battle_turn_steps_fuzz.cpp` | 14 |
| `game/battle_win_states_fuzz.cpp` | 13 |
| `game/battle_window_draw_fuzz.cpp` | 10 |
| `game/battle_windows_fuzz.cpp` | 14 |
| `game/boss_harness.cpp` | 38 |
| `game/boss_harness_eh.cpp` | 1 |
| `game/boss_sa_fuzz.cpp` | 2 |
| `game/boss_sb_fuzz.cpp` | 1 |
| `game/boss_sc_fuzz.cpp` | 1 |
| `game/boss_se_fuzz.cpp` | 2 |
| `game/boss_sf_fuzz.cpp` | 1 |
| `game/boss_sg_fuzz.cpp` | 20 |
| `game/boss_sh_fuzz.cpp` | 25 |
| `game/boss_si_fuzz.cpp` | 2 |
| `game/boss_sj_fuzz.cpp` | 1 |
| `game/boss_spawn_fuzz.cpp` | 1 |
| `game/char_stats_fuzz.cpp` | 15 |
| `game/crt_rest.cpp` | 3 |
| `game/crt_rest_fuzz.cpp` | 5 |
| `game/d3d_draw.cpp` | 1 |
| `game/d3d_draw_fuzz.cpp` | 10 |
| `game/d3d_fuzz.cpp` | 4 |
| `game/d3d_list.cpp` | 1 |
| `game/d3d_list_fuzz.cpp` | 13 |
| `game/d3d_rest_fuzz.cpp` | 21 |
| `game/dat_cache.cpp` | 4 |
| `game/dat_load.cpp` | 5 |
| `game/ddraw_fuzz.cpp` | 21 |
| `game/display_env_fuzz.cpp` | 18 |
| `game/draw_emit.cpp` | 15 |
| `game/draw_layers.cpp` | 7 |
| `game/draw_pass.cpp` | 13 |
| `game/draw_pool.cpp` | 8 |
| `game/effect_1a_fuzz.cpp` | 7 |
| `game/effect_1b.cpp` | 1 |
| `game/effect_1b_fuzz.cpp` | 6 |
| `game/effect_1c_fuzz.cpp` | 7 |
| `game/effect_1d_fuzz.cpp` | 4 |
| `game/effect_1e_fuzz.cpp` | 3 |
| `game/effect_1f_fuzz.cpp` | 7 |
| `game/effect_1g_fuzz.cpp` | 2 |
| `game/effect_2a_fuzz.cpp` | 4 |
| `game/effect_2b_fuzz.cpp` | 6 |
| `game/effect_2c_fuzz.cpp` | 4 |
| `game/effect_2d_fuzz.cpp` | 4 |
| `game/effect_2e_fuzz.cpp` | 3 |
| `game/effect_2f_fuzz.cpp` | 3 |
| `game/effect_2g_fuzz.cpp` | 4 |
| `game/effect_3a_fuzz.cpp` | 5 |
| `game/effect_3b_fuzz.cpp` | 5 |
| `game/effect_3c_fuzz.cpp` | 4 |
| `game/effect_3d_fuzz.cpp` | 4 |
| `game/effect_4a_fuzz.cpp` | 2 |
| `game/effect_4b_fuzz.cpp` | 4 |
| `game/effect_4c_fuzz.cpp` | 4 |
| `game/effect_4d_fuzz.cpp` | 3 |
| `game/effect_4e_fuzz.cpp` | 4 |
| `game/effect_4f_fuzz.cpp` | 4 |
| `game/effect_5a_fuzz.cpp` | 5 |
| `game/effect_5b_fuzz.cpp` | 3 |
| `game/effect_5c_fuzz.cpp` | 5 |
| `game/effect_5d_fuzz.cpp` | 4 |
| `game/effect_5e_fuzz.cpp` | 5 |
| `game/effect_5f_fuzz.cpp` | 5 |
| `game/effect_5g_fuzz.cpp` | 3 |
| `game/effect_6a_fuzz.cpp` | 3 |
| `game/effect_6b_fuzz.cpp` | 3 |
| `game/effect_6c_fuzz.cpp` | 5 |
| `game/effect_6d_fuzz.cpp` | 3 |
| `game/effect_gte_fuzz.cpp` | 3 |
| `game/enemy_ai_ops_fuzz.cpp` | 11 |
| `game/event_leader_fuzz.cpp` | 16 |
| `game/event_objs_fuzz.cpp` | 11 |
| `game/event_ops_fuzz.cpp` | 28 |
| `game/event_script_fuzz.cpp` | 19 |
| `game/field_blocked.cpp` | 12 |
| `game/field_c1_fuzz.cpp` | 7 |
| `game/field_c2_fuzz.cpp` | 7 |
| `game/field_e1_fuzz.cpp` | 2 |
| `game/field_e2_fuzz.cpp` | 13 |
| `game/field_event_fuzz.cpp` | 16 |
| `game/field_frame.cpp` | 14 |
| `game/field_hidden_fuzz.cpp` | 15 |
| `game/field_input.cpp` | 2 |
| `game/field_misc_fuzz.cpp` | 34 |
| `game/field_modes_fuzz.cpp` | 15 |
| `game/field_o_fuzz.cpp` | 7 |
| `game/field_objects.cpp` | 14 |
| `game/field_s_fuzz.cpp` | 10 |
| `game/file_io.cpp` | 3 |
| `game/fmv_play.cpp` | 2 |
| `game/frame_callees.cpp` | 15 |
| `game/gfx_clut.cpp` | 8 |
| `game/gfx_flush.cpp` | 8 |
| `game/gfx_frame.cpp` | 1 |
| `game/gfx_texcache.cpp` | 13 |
| `game/gfx_unpack.cpp` | 7 |
| `game/gfx_vram_ops.cpp` | 5 |
| `game/glyph_draw_fuzz.cpp` | 7 |
| `game/inventory_ops.cpp` | 1 |
| `game/inventory_ops_fuzz.cpp` | 15 |
| `game/item_use_fuzz.cpp` | 19 |
| `game/kind2_object.cpp` | 12 |
| `game/magic_c1_fuzz.cpp` | 3 |
| `game/magic_c2_fuzz.cpp` | 5 |
| `game/magic_c3_fuzz.cpp` | 3 |
| `game/magic_engine_fuzz.cpp` | 2 |
| `game/magic_fx_reached_fuzz.cpp` | 15 |
| `game/magic_harness.cpp` | 25 |
| `game/magic_lib_fuzz.cpp` | 6 |
| `game/magic_s01_fuzz.cpp` | 3 |
| `game/magic_s02_fuzz.cpp` | 5 |
| `game/magic_s03_fuzz.cpp` | 4 |
| `game/magic_s04_fuzz.cpp` | 4 |
| `game/magic_s06_fuzz.cpp` | 4 |
| `game/magic_s07_fuzz.cpp` | 3 |
| `game/magic_s08_fuzz.cpp` | 6 |
| `game/magic_s09_fuzz.cpp` | 4 |
| `game/magic_s10_fuzz.cpp` | 4 |
| `game/magic_s11_fuzz.cpp` | 6 |
| `game/magic_s12_fuzz.cpp` | 4 |
| `game/magic_s13_fuzz.cpp` | 4 |
| `game/magic_s14_fuzz.cpp` | 3 |
| `game/magic_s15_fuzz.cpp` | 9 |
| `game/magic_s16_fuzz.cpp` | 8 |
| `game/magic_s17_fuzz.cpp` | 11 |
| `game/magic_s18_fuzz.cpp` | 2 |
| `game/magic_s19_fuzz.cpp` | 3 |
| `game/magic_s20_fuzz.cpp` | 2 |
| `game/magic_s21_fuzz.cpp` | 5 |
| `game/magic_s22_fuzz.cpp` | 5 |
| `game/magic_s23_fuzz.cpp` | 26 |
| `game/magic_s24_fuzz.cpp` | 3 |
| `game/magic_s25_fuzz.cpp` | 3 |
| `game/magic_s26_fuzz.cpp` | 4 |
| `game/magic_s27_fuzz.cpp` | 4 |
| `game/magic_s28_fuzz.cpp` | 4 |
| `game/magic_s29_fuzz.cpp` | 2 |
| `game/magic_s30_fuzz.cpp` | 3 |
| `game/magic_s31_fuzz.cpp` | 5 |
| `game/magic_s32_fuzz.cpp` | 4 |
| `game/magic_s33_fuzz.cpp` | 4 |
| `game/magic_s34_fuzz.cpp` | 3 |
| `game/magic_s35_fuzz.cpp` | 3 |
| `game/magic_s36_fuzz.cpp` | 7 |
| `game/magic_s37_fuzz.cpp` | 5 |
| `game/magic_s38_fuzz.cpp` | 4 |
| `game/magic_steal_fuzz.cpp` | 1 |
| `game/map_cells.cpp` | 19 |
| `game/map_field_objects_fuzz.cpp` | 19 |
| `game/map_layers.cpp` | 2 |
| `game/map_layers_fuzz.cpp` | 12 |
| `game/map_scroll_fuzz.cpp` | 12 |
| `game/map_view.cpp` | 2 |
| `game/member_sprites_fuzz.cpp` | 11 |
| `game/menu_draw_helpers_fuzz.cpp` | 12 |
| `game/menu_lists_fuzz.cpp` | 12 |
| `game/menu_windows_fuzz.cpp` | 11 |
| `game/mode_flow_fuzz.cpp` | 23 |
| `game/mode_rest_fuzz.cpp` | 3 |
| `game/mode_states_fuzz.cpp` | 21 |
| `game/mode_tasks.cpp` | 14 |
| `game/move_cmds_fuzz.cpp` | 14 |
| `game/move_groups.cpp` | 15 |
| `game/move_script.cpp` | 16 |
| `game/msg_pool.cpp` | 2 |
| `game/msgbox.cpp` | 3 |
| `game/msgbox_fuzz.cpp` | 16 |
| `game/music_seq.cpp` | 9 |
| `game/object_kinds.cpp` | 16 |
| `game/prim.cpp` | 1 |
| `game/psx_gpu.cpp` | 6 |
| `game/psx_gte.cpp` | 5 |
| `game/psx_gte_float.cpp` | 8 |
| `game/psx_gte_matrix.cpp` | 5 |
| `game/psx_gte_transform.cpp` | 5 |
| `game/psx_rest_fuzz.cpp` | 29 |
| `game/rest_0a_fuzz.cpp` | 5 |
| `game/rest_1a_fuzz.cpp` | 3 |
| `game/rest_1b_fuzz.cpp` | 3 |
| `game/rest_1c_fuzz.cpp` | 3 |
| `game/rest_1d_fuzz.cpp` | 3 |
| `game/rest_1e_fuzz.cpp` | 3 |
| `game/rest_1f_fuzz.cpp` | 6 |
| `game/rest_1g_fuzz.cpp` | 6 |
| `game/rest_2a_fuzz.cpp` | 3 |
| `game/rest_2b_fuzz.cpp` | 5 |
| `game/rest_2c_fuzz.cpp` | 5 |
| `game/rest_2d_fuzz.cpp` | 4 |
| `game/rest_2e_fuzz.cpp` | 3 |
| `game/rest_2f_fuzz.cpp` | 11 |
| `game/rest_2g_fuzz.cpp` | 4 |
| `game/rest_2h_fuzz.cpp` | 25 |
| `game/rest_3a_fuzz.cpp` | 5 |
| `game/rest_3b_fuzz.cpp` | 3 |
| `game/rest_3c_fuzz.cpp` | 4 |
| `game/rest_3d_fuzz.cpp` | 3 |
| `game/rest_3e.cpp` | 2 |
| `game/rest_3e_fuzz.cpp` | 5 |
| `game/rest_3f_fuzz.cpp` | 4 |
| `game/rest_3g_fuzz.cpp` | 8 |
| `game/rest_4a_fuzz.cpp` | 8 |
| `game/rest_4b_fuzz.cpp` | 4 |
| `game/rest_4c_fuzz.cpp` | 4 |
| `game/rest_4d_fuzz.cpp` | 8 |
| `game/rest_4e_fuzz.cpp` | 5 |
| `game/rest_4f_fuzz.cpp` | 16 |
| `game/save_io.cpp` | 1 |
| `game/save_menu.cpp` | 2 |
| `game/save_menu_fuzz.cpp` | 23 |
| `game/scena_calls_fuzz.cpp` | 6 |
| `game/scena_sc0_fuzz.cpp` | 14 |
| `game/scena_sc11_fuzz.cpp` | 5 |
| `game/scena_sc12_fuzz.cpp` | 7 |
| `game/scena_sc13_fuzz.cpp` | 10 |
| `game/scena_sc15_fuzz.cpp` | 23 |
| `game/scena_sc1_fuzz.cpp` | 2 |
| `game/scena_sc2_fuzz.cpp` | 4 |
| `game/scena_sc3_fuzz.cpp` | 4 |
| `game/scena_sc5_fuzz.cpp` | 8 |
| `game/scena_sc6_fuzz.cpp` | 5 |
| `game/scena_sc7_fuzz.cpp` | 7 |
| `game/scena_sc9a_fuzz.cpp` | 6 |
| `game/scena_sc9b_fuzz.cpp` | 10 |
| `game/scena_se_fuzz.cpp` | 1 |
| `game/scena_sx2_fuzz.cpp` | 6 |
| `game/scena_sx_fuzz.cpp` | 2 |
| `game/scenario_harness.cpp` | 38 |
| `game/scenario_harness_ekh.cpp` | 1 |
| `game/shell_fuzz.cpp` | 23 |
| `game/shop_states2_fuzz.cpp` | 19 |
| `game/shop_states_fuzz.cpp` | 9 |
| `game/shoulder_glyphs.cpp` | 3 |
| `game/sound.cpp` | 11 |
| `game/sound_fuzz.cpp` | 26 |
| `game/sound_rest_fuzz.cpp` | 13 |
| `game/sprite_anim.cpp` | 2 |
| `game/sprite_clut.cpp` | 5 |
| `game/sprite_draw.cpp` | 19 |
| `game/sprite_find.cpp` | 19 |
| `game/sprite_order.cpp` | 4 |
| `game/sprite_pose_fuzz.cpp` | 14 |
| `game/sprite_records.cpp` | 6 |
| `game/sprite_screen.cpp` | 9 |
| `game/sprt_draw_fuzz.cpp` | 9 |
| `game/task_sched_fuzz.cpp` | 16 |
| `game/tex_cells_fuzz.cpp` | 29 |
| `game/tex_page.cpp` | 3 |
| `game/tex_page_fuzz.cpp` | 22 |
| `game/text_draw.cpp` | 12 |
| `game/text_immediate.cpp` | 5 |
| `game/title_states.cpp` | 13 |
| `game/wide_rand.cpp` | 12 |
| `game/wide_staging.cpp` | 16 |
| `game/widescreen.cpp` | 4 |
| `game/win_main.cpp` | 7 |
| `game/window_kinds_fuzz.cpp` | 11 |
| `game/window_task_fuzz.cpp` | 12 |
| `game/world_map.cpp` | 2 |
| `game/world_map_fuzz.cpp` | 10 |
| `game/worldmap_area_fuzz.cpp` | 23 |
| `hook/calltrace.cpp` | 30 |
| `hook/code_bytes.cpp` | 3 |
| `hook/crash.cpp` | 12 |
| `hook/draw_order.cpp` | 9 |
| `hook/gapwatch.cpp` | 31 |
| `hook/input_script.cpp` | 20 |
| `hook/log.cpp` | 9 |
| `hook/stackwatch.cpp` | 11 |
| `hook/stallwatch.cpp` | 7 |
| `hook/statehash.cpp` | 23 |
| `hook/wav_dump.cpp` | 25 |
| `input/button_tests.cpp` | 3 |
| `input/pad_sdl.cpp` | 1 |
| `input/sigma.cpp` | 1 |
| `render/fb_grab.cpp` | 3 |
| `render/render_shim.cpp` | 7 |

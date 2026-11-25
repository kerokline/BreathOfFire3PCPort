# State by name, group A1: the quicksave's format 2 and the load's three answers

**Status:** STABLE (2026-11-25, verified the same day on the Linux x86-64
build of `sbn/A1`; section 7 has the measurements). The
state-by-name round's group A1 ([`state-by-name-round.md`](state-by-name-round.md)
4, 4a), on the laptop. Built: `src/hook/snapshot.{h,cpp}`, the seam
`src/hook/task_yield.h`, the shared place `src/hook/span_words.h`. Not played
by the owner. Ledger: draft A below (section 8), numbered at the merge.

What a quicksave file now holds and what a load does with it. The round doc
has the design ([`state-by-name-round.md`](state-by-name-round.md) 3); this
document is the format as built, the owner's call 8 as read, and where the
build differs from the design and why.

## 1. Call 8: which settings change state a quicksave holds

The owner's call 8 (2026-11-25): read which settings change state that a
quicksave holds, and make the header carry a fingerprint of those that are
not safe to cross, so that a difference is a refusal with its reason and the
save offered, never a load that goes wrong. Where it cannot be told, the
setting is unsafe and the table says it is not measured.

"Safe to cross" here: a quicksave taken under one value and loaded under
another leaves every byte it writes meaning what the loading process's code
reads it as, and leaves no state of the setting's own (a static outside the
span and the records) at odds with the span. A setting that acts at an event
on ordinary game state (an enemy's EXP added to the battle total) is safe in
this sense: the state is as if the events before the take had run under the
other value, as after loading a save made under it.

The settings are what the game reads: the launcher turns `bof3x.ini` into
environment variables (`src/launcher/config.cpp:552`
`ConfigApplyEnvironment`), the preset included (a preset is a `BOF3X_OFF`
list and values; the game reads `BOF3X_PRESET` for its log only).

| Setting (what the game reads) | Read at | What it touches in a quicksave | Across values | In the fingerprint | Evidence |
|---|---|---|---|---|---|
| Language: the original text or a translation (`BOF3X_LANG`) | `msg_pool.cpp` (DIV-0007), `lang_layout.cpp` | `ST`'s record holds the moved system message pool (0 or 0x8000 bytes); a translation re-aims Capcom's pointer tables at text of ours | **unsafe**: refused today by `ST`'s check (`snapshot.cpp` `StCheck`) | yes, as its class: `lang=original` / `translation` | read; [`quicksave.md`](quicksave.md) 4b |
| One translation into another | as above | the take's pool and area text stay until the game reloads its own | let through (the owner, 2026-10-20) | no (the class is equal) | read, not measured ([`IDEAS.md`](IDEAS.md) I47) |
| Music source (`BOF3X_MUSIC`, the cache's layers) | `music_seq.cpp`, `sound_ours.cpp` `SnapCheck` | `SOUND`'s record names the take's music file by track and size | **unsafe**, refused by `SOUND`'s check before a byte is written | no: the finer check is there and self-tested | [`quicksave.md`](quicksave.md) 4c |
| Wide mode (`BOF3X_WIDE` 0..3, `BOF3X_WIDE_RING`) | `widescreen.cpp:356` | `GFX`'s record: the rim (`map_rim.cpp:275`), the slide split, `wide_rand`'s seed | **safe**: the rim is rebuilt across widths by design (the owner's answer 4, 2026-10-21) | no | **measured**: [`wide-ring.md`](wide-ring.md) 4.5, 16:9 to 4:3, 4:3 to 16:9, 21:9 to 16:9, no game page differs |
| Display scale k (`BOF3X_SCALE`, `BOF3X_SNAP`, the window) | `display_setup.cpp:88`; `d3d.cpp:916` `Rescale` | span words `D3d_ScaleY` / `X` `0x7C9F48`, `Gfx_ScreenRect[2..3]` `0x66B710`, `Display_BackPitch` `0x7DED5C`, written by `Rescale` at set-up and when the target is remade (and `SprtDraw_SetScale`'s static of ours) | **safe**: every one of those words is kept live by the graphics participant (`gfx_snapshot.cpp:59`, `:64`, `:70`, its own `kKept`), so a load leaves this process's scale | no | read |
| Desktop size and depth | `magic_080.cpp:697..700` (once, at start), `d3d.cpp:3570` | `Desktop_Bpp` / `Height` / `Width` `0x939A2C`: the window toggle's gate (`magic_080.cpp:537`) and the window's place (`game_window.cpp:128`, `:151`, `:329`); `0x6C9F40` and `Display_DesktopWidth` `0x7C9F44` | the machine's: across machines the take's desktop would stand until the next start | no: `0x939A2C`'s 12 bytes **join the keep list** (section 1a); the other two are kept by the graphics participant (`gfx_snapshot.cpp:63`, `:64`) | read |
| Code switches (`BOF3X_OFF`, the preset's list; `BOF3X_ORIGINAL`, the developer's) | `detour.cpp:61` | which of our functions and patches run: a switch can give a byte another meaning (DIV-0062 off puts the draw pool back in the span, `snapshot.cpp` `CurrentSpan`) | **unsafe**, not measured one by one | yes: `off=`, `original=`, each list sorted | read |
| Behaviours (`BOF3X_ON`, the mods' needs) | `behaviour.cpp:99` | the behaviours' state in the span and in `BEHAVIOURS`' record | **unsafe**: a take during a master's talk loaded with CampMasters off is a `Fatal` after the span is written (`field_7.cpp:807`) | yes: `on=` | read |
| Solo champion (`BOF3X_SOLO`) | `solo.cpp` | `SOLOCB`'s record and the party | **unsafe**, not measured | yes: `solo=` | read |
| Layers and mods (`BOF3X_OPT`) | `dat_load.cpp`, `exe_image.cpp` | an exe part lays `.data` and `.rdata` at start; the span brings back the take's `.data`, `.rdata` stays the loader's, later `DAT` reads are the loader's | **unsafe**, not measured | yes: `opt=` as given (the order is the layers') | read |
| The PSP's battle rules (`BOF3X_PSP`, else the cache's `base_policy`) | `psp_code.cpp:52`, `:145` | decisions at a duel's loss and a Myria item (`psp_code.cpp:126`, `:131`) | treated as unsafe: a behaviour switch, not measured | yes: `psp=` the resolved mask | read |
| Desert faerie entrance (`BOF3X_DESERT_FAERIE`, DIV-0204) | `event_script_ours.cpp:153` | an area's placement: the entrance stays in the span for the visit | treated as unsafe: a behaviour switch, not measured | yes: `desert-faerie=` | read |
| Sprite layering (`BOF3X_LAYERING`, the developer's) | `layering.cpp:613` | draw records in `.data` (the state-hash runs pin it, [`state-hash.md`](state-hash.md) 6) and `GFX`'s runs | treated as unsafe, not measured | yes: `layering=` | read |
| Cheats: EXP, zenny, steal, enemy info, no-fail, learn, faerie speed | `cheats.cpp:157..193` | none of their own: statics set once at start (`cheats.cpp:107..116`), each acting at an event on span state that means the same either way | **safe** in the sense above (faerie speed changes the village's count at once, as after a save's load) | no | read |
| Faster text (`BOF3X_TEXT_SPEED`) | `msgbox.cpp:778` | none of its own: the printer's pace chosen per step (`msg_box.cpp:831`) | safe | no | read |
| Unlearned skill in blue (`BOF3X_UNLEARNED_BLUE`) | `battle_action.cpp:2286` | a draw colour | safe | no | read |
| The picture: interpolation, filter, background, present, satpixie | `interp.cpp:184`, `d3d.cpp`, `magic_080.cpp`, `render_d3d11.cpp`, `satpixie.cpp` | the renderer's and the loop's own | safe | no | read at the variables' readers only |
| Keys, pad, function keys, the reading row's key | `d_input.cpp`, `pad_read.cpp`, `function_keys_game.cpp` | `Key_Table` is kept (`kKept`); the rest is the input's own | safe | no | read |
| Speed, the frame's length, the heap arena | `magic_080.cpp`, `crt_rest.cpp:373` | timing; the heap holds no logic state ([`quicksave-census.md`](quicksave-census.md) 4) | safe | no | read |
| The image's base (`BOF3X_IMAGEBASE`) | `host.cpp` | the span's place | refused already ("another span") | no | [`quicksave.md`](quicksave.md) 3 |
| The read watch (`BOF3X_GAPWATCH`) | `snapshot.cpp` `Snapshot_Pending` | its guard pages over the span | F6 and F7 refused while armed | no | read |
| Probes and self-test selectors (`*_ONLY`, `*_ROUNDS`, `*_TRACE`, `*LOG*`, `STATEHASH*`, `CALLTRACE*`, `TASKCENSUS`, `STPCENSUS`, `SHOT*`, `WAV*`) | their modules | they observe | safe | no | read at their names only |

The other developer's switches that change logic (`BOF3X_TILE1`,
`BOF3X_SIDE_DUP`, `BOF3X_SIDE_ZERO`, the `AR*_AREA` selectors and the like)
are not read for this. A run with one of them set is a developer's, and its
quicksaves are not promised across settings.

**What the header carries** (`snapshot.cpp` `Settings`): nine lines, one a
setting, in this order: `lang`, `off`, `original`, `on`, `solo`, `opt`,
`psp`, `desert-faerie`, `layering` (nine). The file holds the lines and
their FNV-1a; a load compares the fingerprint with this process's and, when
it differs, refuses as "other settings (the names)", the log naming each
line as the file has it and as here. The check is on every format-2 load,
exact ones too: two processes of one build under other settings refuse each
other.

**A finding for the owner.** A preset is a long `off=` list, so a quicksave
does not cross presets. That is what the evidence supports: no code switch
is measured as safe to cross.

**A first reading corrected (2026-11-25).** A first draft of this table
put the display scale in the fingerprint, from `Rescale` and `snapshot.cpp`'s
keep list alone. The graphics participant keeps those words itself
(`gfx_snapshot.cpp:58..70`), so the scale crosses; it is not in the
fingerprint.

### 1a. Words that are the process's but were in no keep list

Group A3's region table (2026-11-25) listed words that read as the
process's and that neither `kKept` held nor, as far as its table could
tell, anything else. Decided from the code:

| Word(s) | What writes them | Kept by | Decision |
|---|---|---|---|
| `DDraw_Primary`, `DDraw_BackBuffer`, `PsxRest_ZBuffer` `0x7CC338` (12) | the display's set-up: render surfaces of ours | the graphics participant (`gfx_snapshot.cpp:67`) | nothing to add |
| `PsxRest_Clipper`, `PsxRest_Direct3D` `0x7CC348` (8) | the set-up: objects of ours | the graphics participant (`:68`) | nothing to add |
| `D3d_Viewport`, `D3d_BackMaterial` `0x7CC354` (8) | the set-up | the graphics participant (`:69`) | nothing to add |
| `0x6C9F40` (the desktop's height, `Display_Setup`'s copy) | `d3d.cpp` set-up | the graphics participant (`:63`) | nothing to add |
| `Display_DesktopWidth` `0x7C9F44` (with `D3d_ScaleY` / `X`) | `d3d.cpp:3570`, `Rescale` | the graphics participant (`:64`) | nothing to add |
| `Desktop_Bpp`, `Desktop_Height`, `Desktop_Width` `0x939A2C` (12) | `magic_080.cpp:697..700`, once at start, from this machine's desktop | nobody | **added to `kKept`** as `{0x939A2C, 12, "Desktop_Bpp"}`: a load from another machine would leave the take's desktop in force for the window toggle's gate and the window's place until the next start |

For the region table (A3's `tools/region_classes.txt`): `0x939A2C 12`
becomes `process`; the rows the graphics participant keeps are the
participant's to class (they are written back by its `Restore`, not by the
core). `Task_StackTop` is **not** added to `kKept`: the portable restore
keeps it itself, as one of the scheduler's stack words (section 3), so its
row stays `stack`.

## 2. The file, format 2

`BISLPS1X.DAT` keeps its shape: the save half, the header, the span, the
pool, the arena, the records. Format 2 adds the header's extension and three
sections between the arena and the records. Little-endian.

| Part | Bytes | |
|---|---|---|
| the save half | `0x12B0` | as format 1 ([`quicksave.md`](quicksave.md) 4) |
| `Header` | `0xA0` | format 1's whole header, `format` = 2, `header_bytes` = `0xD0` |
| `HeaderExt` | `0x30` | 12 words: the state revision; portable (1 / 0); why not (a code, and the task's number `<< 8`); the settings' FNV-1a; the settings' bytes; the TASKS record's bytes (`0x70`); the WORDS entries; the code places' table base (`Bind_TableLo`, 0 hosted); the bound VAs' count and FNV-1a; `ImageBase()`; the writing build's pointer width |
| the span, the pool, the arena | as format 1 | the span with the named words written by their names (section 3) |
| `TASKS` | 4 x 28 | one entry a task: kind (0 free, 1 created and not yet run, 2 at a site, 3 nowhere named), name (the entry's VA, or the site's id), the locals' bytes, 16 bytes of locals |
| the settings | `settings_bytes` | section 1's lines |
| `WORDS` | 4 x `words` | one word a named word of `.data`: `(offset from 0x5DA000 / 4) << 3 \| kind` |
| the records | as format 1 | |

**The state revision** is 1. It is raised by a change that gives a byte of
saved state a new meaning, and the header's own table (`snapshot.cpp`
`kRevisions`) gives each revision a row naming the ledger entry that raised
it (revision 1: "-", the first). A file of another revision is refused, its
save offered (the owner's call 7 (a): no converter until one change is worth
its cost).

**Format 1** files (every quicksave before this round) are read as before:
exact in the build and process that wrote them; in any other build, refused
for their build with the save offered. A format-1 file of another build is
never portable.

## 3. Words written by what they name

At a take, before the copy, each aligned word of `.data` (`0x5DA000` to
`0x93E000`, the range the state hash reads and the tests were measured on)
is tested by the state hash's own tests, in the hash's order; a word that
passes is written in the file by its name and listed in `WORDS`; every other
byte is written as the process had it. The keep list and the scheduler's
stack words (`Task_StackTop`, each record's saved stack pointer) are never
named: they are the process's, and a portable load writes them fresh.

| Kind | The test (shared) | In the file | Back at a load |
|---|---|---|---|
| code place | `Bind_VaOf` (bind.h): a place `(top << 24) \| VA` under the table's top byte | the VA | `Code(VA)`: this process's place; refused when this build binds no function there |
| image pointer | `span_words::Fold`, the image's base when it is not `0x400000` | as at `0x400000` | at this process's base |
| image link | `span_words::Fold24` | the low 24 bits as at `0x400000`, the top byte kept | at this process's base |
| message slot | the 17 run-time slots (`span_words::RuntimeSlot`), a value inside our message pool | the offset in the pool | this process's pool plus the offset |
| text of ours | one of Capcom's slots (below) holding a pointer into a section of ours named by its bytes (`span_words::OurImage::Named`, content) | raw | exact: raw; portable: the process's own word, which must be text of ours |

**Shared, not copied.** The fold, the run-time slots and our module's
sections moved out of `statehash.cpp` into `src/hook/span_words.h`
unchanged; `statehash.cpp` uses them from there (its `ReadDllImage`,
`Named`, `Fold`, `Fold24`, `kMsgRing...`, `kExeLo`, `kExeHi`, `kImageExtent`
and `Section` are the header's now). What the hash computes did not change:
the same functions over the same values. The bound-word test was already
shared (`bind.h`). Shown in section 7: the state-hash runs are identical tick
for tick with the round base's. `statehash.cpp`'s `kBase` and `kEnd` take
their values from `span_words.h`'s `kDataLo` and `kDataHi` (the same
numbers).

**Capcom's slots for the quicksave** are listed once, before the injects, in
every run: `span_words::BeforeInjects`, called by `StateHash_BeforeInjects`
after its self-test and before its `BOF3X_STATEHASH` test - the one added
line in the hash's file that is not a move. They are the words whose value
then lay in `BOF3.exe`'s image at its place: the hash's census test without
its folds of another base.

**A premise that did not hold as written: the fold both ways across bases.**
The round doc has the image's pointers written "as at `0x400000`, and back to
this process's base". The fold is a test by value, and game data holds
values in any such range: 169 to 295 words of one dump in a range at
`0x70000000` ([`state-hash.md`](state-hash.md) 8, measured 3), and at
`0x400000` 7,358 of the census's 26,189 slots are numbers, not pointers
([`state-hash.md`](state-hash.md) 8, measured 1). The hash folds such a word
alike on both sides and loses nothing; a load across bases would turn each
of them into another number. So the fold is written both ways - a moved take
writes the image's pointers as at `0x400000` and a load at the same base
puts them back exactly - and a load at another base stays refused, as
"another span", as it was. Every released build has its image at `0x400000`,
where the fold is the identity and no word is listed.

**The same set of bound functions.** A code word whose VA the writing build
did not bind holds the trap, `Img(va)`, which reads as a number and is not
named. A build that binds that VA would hold a place there in a fresh start.
So a portable load wants the same bound VAs as the writing build: the header
carries their count and FNV-1a, and a difference is a refusal ("another
build (other functions bound)"). The round's builds change no `impl`
binding, so the round's proof is not touched by it.

## 4. The TASKS record and the seam with A2

`src/hook/task_yield.h` is the seam. What A2 must provide is written at its
top, and A2 changes only that header and its own files:

1. `Task_YieldAt(site, frames, locals, bytes)` writes the running task's
   entry, sleeps, and clears the entry on the way back.
2. `Task_Create`, `Task_Restart` and `Task_Exit` clear the slot's entry (a
   task dropped while it stood at a site must not leave its site behind).
3. A row in `kSites` per site: a permanent id, a name, the resume entry, the
   locals' size.
4. Each resume entry reads its locals from its entry, clears it, and goes on
   from the yield.

The entries (`task_yield::g_entries`, four) are a static of ours, not in the
span: no state hash moves.

At a take each task's place is found by `TaskPlace`: free (state 0);
**created and not yet run** when its state is not sleeping and its saved
stack pointer is its slice's first frame (`Task_StackTop - k * slice -
kFirstFrame`, task_switch's constants) - its place is the entry word there,
a code place, written as its VA; at a site when its entry holds one;
otherwise nowhere named, and the take is not portable ("a task stands at no
named place (task k)"). A created task whose entry is no place (a function
of ours by its address) is not portable either.

At a portable load (`PlaceTasks`), each task not free is started by
`Task_Create` itself on a fresh first frame under this process's
`Task_StackTop`, at `Code(VA)` or at the site's resume entry; then its
record's state is put back as the file has it (`Task_Create` makes it
runnable and leaves the sleep word), so a task that slept *n* frames wakes in
*n*. The entries become the file's. Nothing of the file's arena is read. At
an exact load the arena is written back and the entries are the file's.

**Today no yield writes an entry**, so a take is portable only when every
task is free or created and not yet run - the title's first frames, a start,
a restart. Every other take is "not portable" by the real test, which A2's
sites will turn true. The self-test builds both conditions (section 6).

## 5. The three answers

`Parse` decides, before a byte is written. In order: not a save file; no
snapshot; the format (1, 2, or another); truncated; the state revision; the
host; the sections' sizes; the settings (damaged, or other settings); the
span; the records (each participant's check); the span mapped. Then:

| The file | This process | Answer |
|---|---|---|
| the build (tag and DLL hash), the DLL's base, the arena's top, the code places' table, the image's base: all the take's | | **exact**: as format 1, the arena written back |
| any of those differs | the file is portable; this is `bof3x.exe` with its code places measured; the same bound VAs; every task's VA bound or site listed with its locals' size; every named word placed | **portable** |
| any of those differs | anything else | **refused**: "another build" or "another process", and why ("(not portable)", "(other functions bound)", "(a task at an unknown place)", "(a word this build cannot place)") |

A refusal **offers the save** when the save half is whole: the file holds
its `0x12B0` bytes and the block sums as `LoadMenu_Read` checks a save's (the
u16 at `+0x70` zeroed, the low word of the sum of `0x10B0` bytes;
`sound.cpp:1080`). Every refusal after "not a save file" can offer it.

**For the quick menu (group A4)**, `snapshot.h`:

- `Snapshot_LastLoad()`: the last load's answer - `Exact`, `Portable`,
  `RefusedSaveOffered` (with `why` and the log's `detail`) or `Refused` - read
  after `Snapshot_Pending` ran the load, as `Snapshot_LastLine` is.
- `Snapshot_ReadSaveHalf(slot, out, why)`: the slot's save half when it is
  whole, the bytes the title's load reads in place of a `BISLPS0?.DAT`.

F7 shows the reason as before ("Quickload refused: another build (not
portable)"); the log adds whether the save half is whole. The question and
the load of the save are A4's.

## 6. The self-test

`BOF3X_SELFTEST_ONLY=1` in the exe host, as before
([`quicksave.md`](quicksave.md) 7). Added (`snapshot.cpp` `SelfTestFormat2`,
and the span test made format 2):

- a take is format 2 at this build's revision, and its size is its parts;
- the build and process refusals, on a copy marked not portable, are
  "another build (not portable)" and "another process (not portable)", the
  save half offered; a save half that does not sum is not offered;
- **a format-1 file loads exactly** in its own build (the span but the keep
  list, the arena, the record) and is refused by another, its save offered;
- **a portable take**: task 0 created by `Task_Create` at `Boot_Task`'s code
  place (as `magic_080.cpp:756` creates it), the rest free, a string of ours
  in one of Capcom's slots; the take is portable, its TASKS record says task
  0 is new at `Boot_Task`'s VA, the WORDS list holds the code places and the
  text slot, and a code place reads as its VA in the file while the live span
  keeps the place;
- **stamped as another build, it restores portably**: the span as at the take
  but the keep list and the text slot, which keeps this process's text; task
  0 on a fresh first frame at the code place of its VA, runnable; nothing of
  the file's arena read; the record back;
- the same file in its own build restores **exactly**: the arena and the
  take's text slot;
- **not portable**: task 1 asleep inside a body at no site - the take is not
  portable (task 1), and another build refuses it with its save half kept and
  offered;
- a task at a site: portable, its site and locals in the TASKS record, and
  refused here because this build lists no such site;
- **another state revision** and **other settings** (the PSP rules' line
  changed) refused with their reasons and the save offered; settings that do
  not hash to their fingerprint refused as damaged;
- a code word naming a VA this build does not bind, and a build that binds
  other functions, refused;
- the other word kinds one word at a time: a message slot by its offset in
  the pool and at another pool; a pointer and a link into a moved image; at
  `0x400000` nothing folded; a text slot the file's at an exact load and the
  process's at a portable one, refused when the process's is not ours.

## 7. Measured (2026-11-25, the laptop, the Linux x86-64 ELF of `sbn/A1`)

The captures are outside the tree, in `~/bof3x-runs/sbn1125/A1/root/` on
the laptop (they hold game data).

- **The build** (`cmake --build build-linux-x86_64 -j 2`): clean, no warning
  in the files of this change.
- **The self-test** (`BOF3X_LAUNCHER=1 BOF3X_SELFTEST_ONLY=1`, the two cache
  variables, `SDL_VIDEO_DRIVER=offscreen SDL_AUDIO_DRIVER=dummy`): exit 0,
  10,402 log lines; `shadow      snapshot: self-test passed` with every case
  of section 6, the portable ones included (the code places at
  `0x7C401000..0x7C5C4000`, a measured top byte). The census line: 17,516
  aligned words of `.data` hold a pointer into `BOF3.exe`'s image before the
  injects (`selftest2.log`).
- **No state hash moved.** `bridgeWalk` (en-US) and `combat` as state-hash
  runs, `orig6`'s conditions ([`state-hash.md`](state-hash.md) 6:
  `--only-slot0`, `BOF3X_SCALE=2`, `BOF3X_OPT=none`, `BOF3X_LAYERING=0`, the
  skip list), on this build and on the round base's (`f6d53766`):
  `statehash.py diff` **identical on all 2,074 ticks** (`bridgeWalk`) and **on
  all 2,602 ticks** (`combat`). So the moved tests and the census call left
  the hash as it was (`sh/`).
- **The quicksave round's cross test, format 2 written and restored exactly**
  ([`quicksave-routes.md`](quicksave-routes.md) 3; on Linux the forms are
  played by `input_run.py`, the cross form with `--run-game none` in the live
  run's own game folder, which holds its `BISLPS10.DAT`; `qs/`):

  | Route | The take | `inproc` | `cross` |
  |---|---|---|---|
  | `combat` (F6 900, F7 1200) | 4,854,524 bytes, format 2, revision 1, 8,785 words by their names, not portable: task 0 stands at no named place | equal: the 300 ticks after the restore equal the 300 after the take | **equal: 1,680 ticks from the restore** (the loaded run at tick 1, `Frame_Counter` 901) |
  | `worldMapAndAreaTransition` (F6 595, F7 895) | 4,694,081 bytes, format 2, 8,780 words, not portable (task 0) | equal | **equal: 1,536 ticks from the restore** |

  Both takes are "not portable" by the real test, as section 4 says every
  take in play is until A2's sites exist: task 0 sleeps in `Field_Task`, a
  yield with no entry.
- **The no-data checks:** `tools/ledger_check.py` passes;
  `tools/reloc_census.py`, `tools/width_census.py` and
  `tools/data_census.py` rose with this change (the quicksave names VAs it
  compares and reads the task records and the arena raw), and their files in
  `analysis/` are regenerated as their checks direct; each `--check` passes
  after.

## 8. Ledger: draft A (an amendment of DIV-0145)

- **What the original did:** as DIV-0145 has it (F6 nothing; F7 the next
  renderer device).
- **What this does (amended):** a quicksave is written in format 2. A load
  answers one of three ways. In the build and process that wrote it, it
  restores to the frame, as before. In another build or process it restores
  when the file is portable - every task stood at a named place, and this
  build can place every word written by its name - with each task started
  again at its place. Otherwise it is refused with its reason on screen, and
  when its save half is whole the refusal says the save can be loaded (the
  menu's question is DIV-0147's amendment, draft B of group A4). A quicksave
  taken under other settings that are not safe to cross (the original text
  or a translation, the code switches and the preset's list, the behaviours,
  the champion, the layers and mods, the PSP's rules, the desert entrance,
  the layering) is a new refusal, "other settings", with
  the setting named and the save offered. A quicksave of another state
  revision is refused the same way. Every quicksave written before this
  change is format 1: it loads exactly in its own build and is refused, its
  save offered, in any other.
- **Why:** the state-by-name round (IDEAS I54; the owner's calls 1, 2, 7 and
  8 of 2026-11-24 and 2026-11-25): a quicksave that outlives its build, and a
  difference of settings that is a refusal, never a load that goes wrong.
- **Saved state:** changed (revision 1): the first revision.
- **Reversible?** The keys are ours. A build without this change refuses
  every format-2 file as "another format"; its save half is still a save.

## 9. What other groups need from this

- **A2:** the four items at the top of `task_yield.h` (section 4). Nothing
  in `snapshot.cpp` changes for a site.
- **A4:** `Snapshot_LastLoad` and `Snapshot_ReadSaveHalf` (section 5). The
  refusal's `why` is short enough for the menu.
- **A3:** the keep list (`kKept`) keeps its row form `{0xVA, bytes, "Name"}` and gains one row (below). The scheduler's stack words
  (`Task_StackTop`, the records' saved stack pointers) are a class of their
  own here, "stack": never named, written fresh by a portable load
  (`snapshot.cpp` `Unnamed`). `Desktop_Bpp` .. `Desktop_Width` `0x939A2C`
  (12) joins `kKept` and so the `process` class (section 1a).
  `Snapshot_Kept(count)` in `snapshot.h` exports the rows, as asked.
- **B1:** a participant whose record this build cannot read is refused by
  its check, before the span is written, today only for `ST`, `SOUND`,
  `SOLOCB` and `NEWGAME`; `GFX` and `BEHAVIOURS` check nothing in `Parse` and
  `Fatal` in their `Restore` on another version (`gfx_snapshot.cpp`
  `Restore`, `field_7.cpp:797`), and `BEHAVIOURS` on CampMasters
  (`field_7.cpp:807`), after the span is written. The settings' fingerprint
  catches the behaviour case first; the version cases are B1's (round doc
  3.6). `GFX` writes `render::PipeState` by `sizeof`, so its record is the
  width's.

## 10. Not checked

- A load at another display scale, run: that the scale crosses is from the
  read of `Rescale` and the graphics participant's keep list (section 1),
  not from a picture.
- The settings judged safe by a read of their readers (cheats, text speed,
  the picture, input) have not been crossed in a run.
- The two tails of the span that are ours (`0x93E000..0x93F000` and `.pool`):
  their layout is ours and could change between builds of one size; nothing
  checks it. A word of `.pool` is never named (the dumps measured `.data`
  only).
- A pointer of ours stored by the game outside the listed slots and the
  run-time slots: none was seen by the state hash's routes (73 slots and 17
  run-time words, [`state-hash.md`](state-hash.md) 7); one would restore as
  the other build's address.
- The hosted build: its takes are never portable, by design (round doc 8).
- The 32-bit build: not built here.

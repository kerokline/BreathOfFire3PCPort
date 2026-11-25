# State by name, the first round: a quicksave that outlives its build (IDEAS I54)

**Status: STAGED, 2026-11-24. Not launched; one probe built, nothing else.**
The owner chose I54 as the first of the larger changes after `v0.1.0-beta.2`
(2026-11-24, by chat) and asked for the investigation and this staging on the
laptop, the round to launch on the desktop. Section 2 is measured (the task
census ran here the same day, on the Linux build of `main` `65b7810f` plus the
probe); section 1 says what that changed in I54's tiers; sections 3 to 6 are a
plan; section 7 is the owner's calls (1 and 2 answered the same day, 3 to 5
open); section 9 lists what is not checked.

## 0. The shape in one paragraph

Today a quicksave loads in the build that wrote it and in no other
([`quicksave.md`](quicksave.md) 4). I54 proposed four tiers to end that, the
last of them the renovation of the mode loops. The measurement here says most
of the distance is much shorter than that. **The image is already
build-independent** but for a short list of words (2.2), because its layout
is Capcom's and its code words name functions by Capcom's addresses. What
ties a quicksave to its build is the four task stacks, and **on 16 routes
the tasks stand in a place that a few named words describe on 97.2 % of the
frames** (2.1), and on 99.9 % once the wait for a screen fade is such a place: the top of a task's own loop, where nothing is on its stack
but the loop. So the round is: (T0) a refused quicksave loads as the save it
starts with; (T2) at a take, each task says where it stands by a **yield
word**, and a take where every task stands at a known place is **portable**:
another build restores it by starting each task again at that place, with no
stack read; (T1a) the short list of build-named words is written by what
they name. A quicksave keeps its stacks too, so the same build still
restores any frame exactly as today. The acceptance test is a quicksave
taken by one build and loaded by another - 32-bit to 64-bit, Windows to
Linux - that plays on to the same state hashes as a run that never stopped.
Writing the image field by field (I54's tier 1 as written) is **not** needed
for this and is left to the wide image, I53, which is the change that does
need it (section 1).

## 1. I54's tiers, read again after the measurement

IDEAS I54 has the tiers: 0 the save half, 1 the image by name, 2 the frame
without the stacks, 3 the tasks as resumable data. Three things it assumed
do not hold as written, each measured below:

- **Tier 1 is not what makes a state survive a build.** I54 says the span
  written "field by field under `symbols.toml`'s names ... survives every
  build until the wide image". The span survives every build as bytes
  already, for the same reason: the layout is Capcom's, not the compiler's.
  What does not survive is counted in 2.2 and is small. Names buy three
  other things - a layout that may change (I53), a file that is checked
  value by value (I46), a smaller file (I45) - and only the first needs
  every field named. The named image covers 54 % of the span's bytes today
  and types 109 of its words as pointers (2.3), so a field-by-field writer
  now would write the other 46 % as unnamed runs and could not translate a
  pointer it cannot see. **This round does not build it.** It builds the
  part of tier 1 that persistence needs: the region table and the words
  written by what they name (3.4).
- **Tier 2 is not only a census.** I54 has tier 2 as the measurement that
  prices tier 3. The census is done (2.1), and its answer is that the
  common places need no state machine: three of the four task bodies yield
  from their own loop with no local alive across the yield. Giving those
  places a name and a way back in is a few dozen lines a body. So tier 2
  becomes a deliverable: portable quicksaves on the frames the census calls
  flat.
- **Tier 3 shrinks to a list.** What is left for "the tasks as resumable
  data" is the nested yields: a wait inside a mode's step, a script that
  sleeps. They are 2.8 % of the measured frames, in nine functions, and 2,151 of
  those 2,252 frames are one function, the wait for a fade (2.1).
  Each is its own small piece of work, taken in order of frames, and none
  is in this round except the ones section 4 names. A take on a nested
  frame is not portable; tier 0 is its floor.

The two ideas I54 keeps apart stay apart. (a) Save states across builds:
this round and the list after it. (b) A program whose memory is not handled
directly: I44's remaining stages, I53 and phase 5. They share the region
table (3.4), which is the first form of "every piece of state has a name
and a class".

## 2. Measured for this staging (2026-11-24)

### 2.1 Where the tasks stand: the task census

**The probe.** `BOF3X_TASKCENSUS=<file>` (`src/hook/task_census.{h,cpp}`,
built for this staging; one call in `WinMain`'s loop beside
`Snapshot_Pending`, `src/game/platform/magic_080.cpp`). At the quicksave
point of every frame, for each task record that is not free, it reads the
saved stack pointer (`Task_Records` `+4`), how many bytes of the slice lie
above it, and the return addresses into our code found there: the one the
scheduler's `ret` pops, which is `Task_Sleep`'s caller, exactly; then every
word above it that names an executable byte of ours right after a call
instruction. Words above a task's first frame are stale (a task has no
caller), so the chain past the first frames is a reading aid and the depth is
the measure. Rows aggregate by task and chain, and by the four tasks'
places together. It reads; it writes its own tables and file. No ledger
entry. `tools/task_census.py` names the addresses (`nm` on the ELF, which is
linked at a fixed address) and tallies.

**The runs.** The Linux build, x86-64, headless, `--direct`, `en-US`
(`tools/input_run.py`, as [`linux-round.md`](linux-round.md) runs a recipe),
one route after another. The files are in `~/bof3x-runs/tc1124/` on the
laptop, with the ELF they ran (`bof3x.elf`); the table of every site is
`analysis/task_census.tsv` there (`task_census.py --tsv`; local, not tracked).

| Route | Frames | Every live task flat | A task nested |
|---|--:|--:|--:|
| `bridgeWalk` | 2,040 | 2,005 (98.3 %) | 35 |
| `shop` | 3,120 | 2,906 (93.1 %) | 214 |
| `combat` | 2,580 | 2,564 (99.4 %) | 16 |
| `cutsceneAndNue` | 7,380 | 7,265 (98.4 %) | 115 |
| `worldMapAndAreaTransition` | 2,100 | 1,797 (85.6 %) | 303 |
| `caughFish` | 3,840 | 3,739 (97.4 %) | 101 |
| `whelpBoss` | 13,140 | 12,728 (96.9 %) | 412 |
| `dragonTransform` | 4,320 | 4,304 (99.6 %) | 16 |
| `masterAndManillo` | 5,460 | 5,226 (95.7 %) | 234 |
| `bossAndFlash` | 5,520 | 5,471 (99.1 %) | 49 |
| `menu_screens` | 1,740 | 1,715 (98.6 %) | 25 |
| `shisuMinigame` | 4,560 | 4,400 (96.5 %) | 160 |
| `attract_cycle` | 14,100 | 13,853 (98.2 %) | 247 |
| `newGameName` | 3,000 | 2,968 (98.9 %) | 32 |
| `field_menu` | 1,440 | 1,415 (98.3 %) | 25 |
| `fairyVillage` | 4,680 | 4,412 (94.3 %) | 268 |
| **all sixteen** | **79,020** | **76,768 (97.2 %)** | **2,252 (2.8 %)** |

"Flat" here is: every task that is not free stands in `Field_Task`,
`TitleTask_Run`, `Title_Task`, `Transition_FadeSub` or `Transition_FadeAdd`,
or is created and not yet run (`tools/task_census.py --flat` with those
names). The frame counts are the file's last write, at most 59 frames short
of the route's end. Fifteen runs ended `done`; `shisuMinigame` ends with the
recipe's `quit` and the harness reports that as the game's exit.

**What a task's place is, by the function that yields.** The depth is from
the slice's top to the saved stack pointer on x86-64, where a task created
and not yet run stands at `0x110` and a yield from the body's own frame at
about `0x1E8` (the body starts `0x28` under the top and a yield saves `0xE8`
bytes: `game/task_switch_x86_64.h`).

| Task | Yields in | Frames | Runs | Depth | |
|--:|---|--:|--:|---|---|
| 0 | `Field_Task` | 66,929 | 16 | `0x1E8` | flat |
| 0 | `TitleTask_Run` | 2,687 | 15 | `0x1E8` | flat |
| 0 | `Field_WaitTransition` | 2,151 | 16 | `0x3B8`, `0x3C8` | nested |
| 0 | `GameMode_Field` | 55 | 13 | `0x2C8` | nested |
| 0 | `Menu_WaitTransition` | 16 | 3 | `0x3B8`, `0x3C8` | nested |
| 0 | `GameMode8_Leave` | 16 | 1 | `0x2D8` | nested |
| 0 | `MenuModeEnter` | 6 | 1 | `0x2E8` | nested |
| 0 | `GameMode9_Leave` | 6 | 1 | `0x2D8` | nested |
| 0 | `GameMode3_Leave` | 2 | 1 | `0x2D8` | nested |
| 1 | `Title_Task` | 17,292 | 16 | `0x1E8` | flat |
| 2 | `Transition_FadeSub` | 3,390 | 16 | `0x208` | flat |
| 2 | `SwingIn` | 240 | 5 | `0x208` | a loop with its counters, as the fades |
| 2 | `SwingOut` | 186 | 4 | `0x208` | a loop with its level, as the fades |
| 2 | `Transition_FadeAdd` | 156 | 2 | `0x208` | flat |

Task 3 was free on every frame of every run.

**Read with the source:**

- **`Field_Task`** (`src/game/core/transition.cpp`; original `0x495800`),
  task 0 for a game: `for (;;) { the mode's handler; the play clock;
  Task_Sleep(1); }`. Nothing is alive across the yield. Its place is "the
  loop's top", and the game's place is `Game_Mode` and `Game_Step`, which
  are the task record's own words `+0x18` and `+0x1A` and in the span.
- **`TitleTask_Run`** (`src/game/core/sound.cpp`; original `0x588E70`), task
  0 for the title's menu: `for (;;) { Task_Sleep(1); the mode's entry;
  Field_RunTaskRecords; }`. Nothing alive across the yield.
- **`Title_Task`** (`src/game/core/title.cpp`; original `0x4621C0`), task 1:
  `for (;;) { Task_Sleep(1); Title_CheckStart; the state's entry; the
  backdrop; the logo; }`. Nothing alive across the yield; its state is the
  record's private words ([`mode-tasks.md`](mode-tasks.md) 1).
- **`Transition_FadeSub`** / **`FadeAdd`** (`transition.cpp`; originals
  `0x495620`, `0x4956A0`), task 2: a loop that draws a tile and sleeps a
  frame. Alive across the yield: the three arguments and one local, the
  fade's level (16 bits). The kind that chose them is the task's word
  `ModeFlow_TransitionKind`, in the span, and the arguments are constants
  of the kind; the level is the one word that is on the stack and nowhere
  else.
- **`Field_WaitTransition`** (`transition.cpp:1339`; original `0x4967F0`)
  and its twin `Menu_WaitTransition`: a wait called from inside a mode's
  step, which goes on with the rest of the step when the wait ends. Nested:
  the place is the caller's, part-way through a function.

- **`SwingOut`** and **`SwingIn`** (`transition.cpp:95`, `:117`; kinds 10,
  11, 17 and 18): task 2's other loops, the camera swing into and out of a
  place. The same shape as the fades: a loop, a frame's sleep, a level or a
  counter alive across it. Every frame a swing is running, task 0 is in
  `Field_WaitTransition`: with the swings counted flat the total does not
  move (2,252 nested frames still).
- **The rest of task 0's nested yields** (101 frames in all): a sleep
  inside `GameMode_Field` and inside the steps that enter and leave the
  menu modes 3, 8 and 9 - a frame or a few each time, around a load or a
  mode change.

**The answer.**

1. **A battle, a scene, a shop, a minigame, the menus, the world map and
   the title all run under one flat yield.** They are modes and steps of
   `Field_Task`'s loop (the `modes` column of the table of sites
   shows modes 1 to 9 under that one site), and where they are is in the
   span: `Game_Mode`, `Game_Step`, and each engine's own state words. I54
   expected "a scene script waiting on a frame, a battle turn mid-animation"
   to be deep in the call tree. On these routes they are not: the script
   and battle engines are state machines stepped once a frame, and no route
   showed a yield from inside one.
2. **Four task bodies and the fades cover 97.2 % of frames** with no state
   on a stack but one 16-bit level.
3. **One function is nearly all the rest.** `Field_WaitTransition` is 2,151
   of the 2,252 nested frames. Counting it and `Menu_WaitTransition` (and
   the swings) as places leaves **85 frames of 79,020 nested, 0.1 %**. Its
   callers were not counted here: the calls go through each group's callee
   table, so a grep of the source undercounts them (six direct calls in
   `transition.cpp`). That count is B2's first step.
4. **The frames differ by route in one way:** a route with many area
   changes spends more frames inside a fade's wait (`worldMapAndAreaTransition`
   14.4 %, `shop` 6.9 %).

### 2.2 What in a quicksave names its build

A quicksave is the span, the task arena and a record per participant
([`quicksave.md`](quicksave.md) 3). What each holds that another build of
ours cannot take back as bytes:

| What | Why it names the build | How many | What stands for it |
|---|---|--:|---|
| the task arena | return addresses into our code and the frames our compiler gave each function ([`quicksave-census.md`](quicksave-census.md) 2.1) | 4 slices, `0x4000` each on i686 and `0x8000` on x86-64 (`snapshot.cpp` `kSlice`) | the yield words (3.2) |
| the saved stack pointers | `Task_Records` `+4` of each record, into the arena; `Task_StackTop` | 5 words | written fresh by the restore |
| code words | in `bof3x.exe` a code word holds the function's **place**, `(top << 24) \| VA`: Capcom's address under a top byte the process chose at start-up from `0x7C, 0x70, 0x7E, 0x7B, 0x7D` (`src/hook/bind.cpp` `kTops`; [`state-hash.md`](state-hash.md) 8, "flags bit 2") | 8,673 load-time words with a function of ours, and every copy the game makes | the VA: the state hash folds exactly this, by value (`Bind_VaOf`) |
| text of ours in Capcom's pointer slots | a language overlay re-aims pointer tables at text in our module; its addresses move with every link ([`state-hash.md`](state-hash.md) 7) | 72 pointers on the measured build, five pages, and the 17 run-time slots of the battle's messages | written again by the same inject in every process: left as the process has them (a keep, as the hash's fold shows which) |
| the image's base | a pointer into the image is its VA plus the move, when the image is not at `0x400000` ([`relocatable-image-round.md`](relocatable-image-round.md)) | 18,831 load-time pointer words (`recipes/exe-pointers.tsv`) | the state hash's fold (`BOF3SH4` / `BOF3SH5`); 0 in every released build |
| the keep list | the process's own words inside the span: the window, the input and display objects, `Font_GlyphData` ([`quicksave.md`](quicksave.md) 3) | 17 ranges (`snapshot.cpp` `kKept`) | kept already |
| the participants' records | each has a version of its own; a version that differs is a `Fatal` today, because the build check came first (`gfx_snapshot.cpp` `kVersion` 3, `sound_ours.cpp` `kSnapVersion` 1, `field_7.cpp` `kBehavioursVersion` 4, `solo_battle.cpp` 3, `new_game.cpp` 1, `snapshot.cpp` `kStVersion` 1) | 6 participants | a refusal with a reason, or a reader of the older version (3.6) |
| our part of the span | `.pool` and the end of `.game` (`0x93E000..0x987000`) are laid out by us, not Capcom | 1 range | the header's extents, as today; a size that differs is a refusal |
| what a ledger entry made a byte mean | a divergence that gives a state byte a new meaning (the grown view ring's rim, GFX version 2) | by entry | a **state revision** in the header, raised by the entry that changes meaning (3.4) |

Not in the list, by measurement: **the heap**. No heap block holds logic
state without a source to rebuild it from ([`quicksave-census.md`](quicksave-census.md)
4); the participants rebuild sound, music and the display's caches. And the
span's **layout**: it is the same in the 32-bit and 64-bit builds and on
both systems by the narrow image's design ([`x64-build-round.md`](x64-build-round.md)
1), which the ten state-hash routes show tick for tick
([`hardening-1123.md`](hardening-1123.md): seven identical between the
widths, three differing on one noise page).

### 2.3 The named image, as it stands

From the generated header of this build (`build-linux-x86_64/gen/bof3/symbols.gen.h`,
`struct GameData`; a short script over its members and `static_assert`s):

- `GameData` has **2,971 members**; 2,940 lie in `.data` / `.bss`
  (`0x5DA000..0x93E000`) and cover **1,924,181 of 3,555,328 bytes, 54.1 %**.
- Between them are **855 unnamed runs, 1,630,375 bytes**: 1,347,237 bytes
  in runs over 4 KB, 258,471 in runs of 257 bytes to 4 KB, 24,667 in
  shorter ones. The largest: `0x813580`, `0xB2000` bytes (the arena past
  the message pools, where `LoadDatFile` puts an area and the party's
  sprite banks: [`arena-edits.md`](arena-edits.md)); `0x8CB5B4`, `0x18FCC`;
  `0x66C85C`, `0x91A4` (past `Task_StackTop`); `0x803580`, `0x8000` (the
  message pools).
- By type: 109 members are `Slot32<...>` (a pointer word), 15 are records,
  2,847 are scalars and arrays - 1,641 of them `std::uint32_t`, which is
  also what a pointer nobody has typed is. `analysis/typed_pointer_cells.tsv`
  lists the 33 `[[data]]` entries known to be such.
- The largest named members are derived or the display's: `Gfx_VramShadow`
  1 MB, `DrawItems` `0x24000`, `Gfx_PacketPools` `0x20000`,
  `Gfx_UnpackScratch` `0x1F020`, `Music_Staging` `0x12000`, `Gfx_TexCache`
  `0x6000`. The state hash's skip list (`tools/statehash_skip.txt`) already
  says which of these are the process's and which are redrawn.

So "the image by name" today means a table of regions, not of fields: about
3,800 runs, each with a name or an address and a class. That table is what
this round builds (3.4).

### 2.4 The refusal path and the save half

- A quicksave's first `0x12B0` bytes are a save file as `Save_BuildBlock`
  builds it (`snapshot.cpp` `GameSaveHalf`, `kSaveHalf`), and `Parse` keeps
  `save_half = true` for a file it refuses for its build: the self-test
  checks it (`snapshot.cpp:1043`, "with the build refused, the save half
  still parses").
- A refusal today writes nothing and shows its reason: `DoLoad`
  (`snapshot.cpp:768`), and in the F5 menu "REFUSED - the reason" under the
  slots (`quick_menu.cpp`; [`quicksave.md`](quicksave.md) QM.2).
- A save is loaded by the title's load menu only: `LoadMenu_Read` (original
  `0x5883C0`; `src/game/core/sound.cpp:1080`) reads the file whole into
  `Save_Staging`, sums and copies `0x10B0` bytes to the game block;
  `LoadMenu_Apply` (`0x5885D0`) applies it after 30 frames - the pad map,
  the party set, `Scenario_Load` - and `TitleFlow_EnterGame` (`0x588800`)
  restarts task 0 as `Field_Task` (`sound.cpp:1225..1240`). All of it runs
  **inside task 0**, with `Task_Sleep` waits.
- The game's own way to the title from anywhere is F9's second press:
  `Game_RestartFlag`, read by the loop, which sets the stack base again and
  creates the tasks afresh (`magic_080.cpp:774`).
- The load menu lists `BISLPS0?.DAT` only (`save_menu::ListFiles`, the
  quicksave round's narrowing of the original's `BISLPS??.DAT`), so a
  quicksave is not offered there.
- **The save half is an F12 save, with an F12 save's hazards** (the owner,
  2026-11-24). `GameSaveHalf` builds it as `Save_QuickWrite` does: anywhere,
  any time. The save block holds the area and the place in it, not where
  the party came from. In an encounter field a step onto the map's edge goes
  back to the area and position `Area_Enter` recorded on the way in
  (`AreaW2c_LastArea` `0x802290`, the position `0x7E091C` / `0x7E0920`);
  `GameMode_Start` makes that word `0xFFFF` for a loaded game, so the edge
  holds (DIV-0121, in place of the original's crash) and "the party may then
  have no way out of the field". A save loaded where the game never saves
  has crashed in one other measured place, repaired (DIV-0180, area 135).
  A quicksave restored exactly or as portable has none of this: the span
  holds the three words. Tier 0 is the road that would meet it.

## 3. The design

### 3.1 One file, two ways back

`BISLPS1X.DAT` keeps its shape: the save half, a header, the span, the
arena, the records. **Format 2** adds to the header a state revision and a
flag "portable", and adds one record, `TASKS` (3.2). A load then has three
answers in place of two:

| The file | The loading build | What happens |
|---|---|---|
| any format-2 file | the build and process layout that wrote it | **exact**: as today, the arena written back. Every frame restores |
| portable | another build that reads its state revision and every record | **portable**: the span and the records as today, the arena not read, each task started again at its yield word's place |
| not portable, or a revision or record this build cannot read | another build | **the save**: tier 0 (3.5), with the reason shown |

Format 1 files (every quicksave before this round) load exactly in their own
build and as their save elsewhere.

### 3.2 The yield word

A static of ours beside the task records, one entry a task: **a site id and
up to 16 bytes of the site's locals**. A yield that a resume entry can stand
for is written `Task_YieldAt(site, frames, locals)` in place of
`Task_Sleep(frames)`: it writes the entry, sleeps, and clears the entry on
the way back. A yield anywhere else leaves the entry clear. So at the
quicksave point task *k* is portable when it is free, or created and not yet
run (its place is its entry, a code word), or its entry holds a site.

- The entries are not in the span: Capcom's layout is not touched and no
  state hash moves. They are the `TASKS` record of a take.
- The site ids are a list in one header, permanent like the ledger's ids:
  a site that goes keeps its number unused.
- Each site has a **resume entry**: a task body of ours that goes on from
  the yield. For a loop that sleeps at its end (`Field_Task`) it is the
  loop. For one that sleeps at its top (`Title_Task`, `TitleTask_Run`) it is
  the rest of the pass, then the loop. For the fades it is the loop with
  the level taken from the entry's locals. The body is written once and
  entered two ways, so there is one text of the loop.
- **No behaviour changes.** The game's functions do what they did; the
  write and the clear are to memory of ours. The fuzz of each body compares
  Capcom's against ours through recorders and does not see a static of ours.
  The hosted build with Capcom's own task body (`BOF3X_ORIGINAL`) leaves the
  entry clear and is never portable, which is right.

### 3.3 The portable restore

After the file is checked: the span, the pool and the keep list as today;
then, in place of the arena:

1. `Task_StackTop` and `Task_SchedulerEsp` stay the process's (the keep
   list gains `Task_StackTop`).
2. For each task record that is not free: a fresh first frame on its slice,
   as `Task_Create` makes one, with the resume entry of its site (or, for a
   task not yet run, the function its entry word names). The record's state
   and sleep word stay the file's, so a task that slept *n* frames wakes in
   *n*.
3. The site's locals are handed to the resume entry.
4. The participants restore as today.

Nothing of the file's arena is read, so nothing of the writing build's code
addresses or frame shapes enters the process. That also answers I46 for a
portable file: its bytes are data the game's code reads, and its code words
are checked against the bind table, where an exact file's arena is return
addresses taken on trust.

### 3.4 The region table, and what is written by what it names

One generated table (from `symbols.toml`, the keep list and
`tools/statehash_skip.txt`, by `tools/gen_symbols.py` or beside it): every
run of the span with a name or an address, its length, and a **class**:

| Class | Meaning | In a portable load |
|---|---|---|
| state | the game's logic state | written |
| process | the process's own (the keep list) | kept |
| derived | rebuilt from state by a participant or by the next frames (the display's caches, `Music_Staging`) | as the participant says; candidates to leave out of the file later (I45) |
| stack | the scheduler's words | written fresh |
| unknown | an unnamed run nobody has classed | written, and counted |

It replaces the two hand-kept lists (`kKept` in `snapshot.cpp`, the skip
list's "process" rows) as their one source, and it is the thing I53 and I45
start from. The count of `unknown` bytes is a CI number that may only fall.

**Words written by what they name**, at a take and back at a load, using
the state hash's own tests so that the hash and the quicksave cannot
disagree:

- a code word (a place under any of the top bytes) as its VA, and back to
  this process's place: `Bind_VaOf` and `Code`;
- a pointer into the image as it would read at `0x400000`, and back to this
  process's base: the `BOF3SH4` fold, both ways;
- a slot holding a pointer to text of ours: kept, as the process has it.

**The state revision.** One number in the header, raised by a change that
gives a byte of state a new meaning, with a line in the header's own table
saying which entry of the ledger raised it. A file of an older revision
loads when a converter for each step exists, and as its save otherwise.

### 3.5 Tier 0: a refused quicksave loads as its save

When a load is refused for a reason of the file's build (another build,
another process, not portable here, a record this build cannot read) and the
save half is whole:

- **F5's menu** says so and asks: the reason, then "Load the save in this
  quicksave?" - Yes / No, in the game's own Yes / No. No leaves the game
  untouched, as today.
- **F7** does not ask a question (it has no menu); it shows the reason and
  that F5 offers the save.
- **Yes** takes the game's own road: the restart F9's second press takes,
  then the title's load of this file in place of a `BISLPS0?.DAT`, entered
  at `LoadMenu_Read` with the quicksave's save half as the bytes read, and on
  through `LoadMenu_Apply` and `TitleFlow_EnterGame` unchanged. The game's
  own code applies the save; ours chooses the file and skips the menu.

**The way back out.** A quicksave holds the whole span, so tier 0 has what
an F12 save lacks. After the save is applied and the area entered, the
loader puts back `AreaW2c_LastArea` and the two position words from the
quicksave's span (read by their addresses, which are Capcom's and the same
in every build), when the take stood in an area whose `Field_InputFlags`
has bit 1 and the word in the file is not `0xFFFF`. The map's edge then
leads where it led at the take, and DIV-0121's hold is not reached. F12's
own saves are as DIV-0121 leaves them; this is the quicksave's save half
only.

The other road - applying the save in place, at the quicksave point - means
running `LoadMenu_Apply`'s waits outside a task and clearing by hand what a
restart clears. It is not proposed.

### 3.6 The participants

Each of the six keeps its record and its version. What changes: a version
this build does not read is a **refusal from its check**, with a reason,
never a `Fatal` after the span is written; and each says whether its record
depends on the build's width or system (the `GFX` record writes a struct of
ours by `sizeof`). A record that does is written field by field at its next
version.

## 4. Groups

Two waves. The worktrees, the harness roots and the hand-back report are as
[`solo-character-round.md`](solo-character-round.md) 4 has them. Agents
number nothing in the ledger or `known-defects.md`.

### Wave A (four groups, in parallel; A1 merges first)

- **A1, the format** (`src/hook/snapshot.{h,cpp}`): format 2's header, the
  `TASKS` record's place, the three answers of 3.1 in `Parse` and `DoLoad`,
  the words written by what they name (3.4) with the hash's tests shared,
  not copied; the self-test widened: a file stamped as another build that is
  portable restores, one that is not is refused with its save half kept.
  Format 1 still loads exactly.
- **A2, the yield word and the first sites** (`src/game/core/task.cpp`,
  `task_switch*.h` untouched; `transition.cpp`, `title.cpp`, `sound.cpp`):
  `Task_YieldAt`, the site list, the resume entries of `Field_Task`,
  `TitleTask_Run`, `Title_Task`, `Transition_FadeSub` and `FadeAdd`, and the
  two swings (`SwingOut`, `SwingIn`: the same shape); the portable restore's
  task step (3.3). Checked by the census: the probe
  gains a column, "the yield word at this frame", and every frame the probe
  calls flat must carry a site.
- **A3, the region table** (`tools/`, `symbols.toml` for classes only): the
  generator, the table checked against `kKept` and the skip list at
  start-up as `kKept` is checked today, the `unknown` count in CI.
- **A4, tier 0** (`src/game/menu/quick_menu.cpp`, the title's load path):
  3.5, with a recipe that refuses a doctored build stamp and takes the
  save; the way back out of an encounter field, proved by `stallion.txt`'s
  walk to the edge (DIV-0121's verification) from a quicksave taken in area
  29 and loaded as its save: the party leaves to where it came from. And a
  read of what else a save half taken off a save point lands in: a take in
  a battle, in a scene, in a minigame, each loaded as its save once and
  written down.

### Wave B (after A merges; three groups)

- **B1, the participants** (their six files): 3.6.
- **B2, the next sites**: the nested yields in order of frames from 2.1 -
  `Field_WaitTransition` and `Menu_WaitTransition` first - each either
  given a site, when the caller's rest of the step can be a resume entry, or
  written down as left for the list after this round, with what its place
  would need.
- **B3, the proof** (`tools/quicksave_check.py`, recipes): section 5.

## 5. The proof

The quicksave round's test, across builds ([`quicksave-routes.md`](quicksave-routes.md)):
a live run takes a quicksave at a frame and plays on; a second process loads
it at its frame 0 and plays the rest of the route; the two hash sequences
are equal tick for tick (`quicksave_check.py cross`). Here the second
process is **another build**:

| Taken by | Loaded by | Shows |
|---|---|---|
| a build | the same build, as a portable load forced | the resume entries are the stacks' equal |
| the 32-bit Windows build | the 64-bit Windows build, and back | the width |
| the 64-bit Windows build | the Linux build, and back | the system |
| a build of the round's base | a build of its tip | a build changed by real work |

On the six quicksave routes at the frames they have, and on frames chosen
from the census: one in each flat site, and one on a nested frame, which
must be written as not portable and load as its save. Then the ten
state-hash routes with no quicksave taken, against `_m1116`'s readings, to
show the yield word moved nothing; the hosted shadow suite; the self-tests.

## 6. The ledger

DIV-0145's refusal rule changes: a quicksave from another build is restored
when it is portable, and otherwise offered as its save. An amendment of
DIV-0145 and DIV-0147 (the menu's question), drafted by A1 and A4, numbered
at the merge. The yield word and the region table change no behaviour and
have no entry.

## 7. The owner's calls

**Answered 2026-11-24 (the owner, by chat): calls 1 and 2, both as
proposed.** F6 takes the frame it is given and never waits; a take on a
nested frame is this build's only. Another build refuses its snapshot and
offers the save it starts with, after a confirmation. Calls 3 to 5 are open.

1. **A take on a nested frame - answered: (a).** (a) F6 takes the frame it is given and the
   file says "this build only" (proposed: a save state is of the frame the
   key was pressed on); (b) F6 waits for the next flat frame, up to a
   bound, and takes that one, so that nearly every quicksave is portable.
2. **Tier 0's question - answered: ask.** A refused quicksave: ask before loading its save
   (proposed), or load it at once with a line saying so.
3. **How the menu marks a quicksave that is not portable**, if at all: a
   mark on the slot, or nothing until a load is refused (proposed: nothing;
   the player of one build never meets it).
4. **Field by field waits for I53** (section 1). This round builds the
   region table and no per-field writer. Say if the per-field file is
   wanted sooner for its own sake (I45's size, I46's checks).
5. **The probe stays** as a developer's switch, like `BOF3X_STPCENSUS`
   (proposed), or goes after the round.

## 8. Not in this round

- The image field by field, and any change of the image's layout (I53).
- The nested yields past the ones B2 takes: scene scripts that sleep, the
  battle's waits. They are the list that follows, in order of frames.
- A quicksave across languages or music sources (I47; refused as today).
- A smaller file (I45): the region table makes it a short follow-up.
- The hosted build's quicksaves across builds: it runs Capcom's bodies by
  switch and is the developer's.
- The PlayStation side: nothing here is a disc's.

## 9. Not checked

- The census ran on the Linux x86-64 build only. The 32-bit build's depths
  differ (its slice is `0x4000`); the places should not, since they are the
  source's, but that is not run.
- The census's routes are recorded play. A frame no route reaches is not
  counted: the long scene scripts, most minigames and most boss fights are
  thin or absent in the sixteen.
- Whether every participant's record is the same bytes at both widths: read
  for `GFX` only (it writes a struct by `sizeof`), not compared.
- The `TitleFlow` entry at `LoadMenu_Read` for tier 0 is read from the
  source, not tried.
- What a save half taken in a battle, a scene or a minigame loads as. The
  owner's test of F12 in a battle (IDEAS I18): the save loads in the field
  where the party stood. A scene and a minigame are not tried; A4 tries
  them.
- That no code of the game reads a task's stack through a pointer kept in
  the span (a local's address stored in a record) at a flat site. The state
  hash's stack-address rows are the place to look; `Transition_DrawTile` is
  handed the level's address, but only for the call.
- The top byte of a place differing between two runs of one build: the
  design allows it; it has not been seen.

## 10. The brief, per group

Each brief names: the goal; the files it owns and must not touch; the docs
to read first (IDEAS I54, this doc, [`quicksave.md`](quicksave.md) 1 to 5,
[`quicksave-census.md`](quicksave-census.md) 2 and 5, [`task_sched.md`](task_sched.md)
1, [`mode-tasks.md`](mode-tasks.md), [`state-hash.md`](state-hash.md) 7 and
8, [`quicksave-routes.md`](quicksave-routes.md)); its worktree and harness
root; the checks that mean done; the hand-back report's shape. Rules 1, 2, 4
and 7 restated: no stub (a site with no resume entry is not a site), every
commit signed off, no game data in a doc or a commit. A usage cut resumes
with a message naming the next step; only the hand-back report means merge.

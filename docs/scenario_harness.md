# The scenario harness: one fuzz for every scenario group

**Status:** MEASURED (2026-09-28; every scenario group of round ten ran through it, 0 mismatches each, [`takeover-queue-round10.md`](takeover-queue-round10.md)) - built and proved on chapter 0
([`scena_sc0.md`](scena_sc0.md): 19 functions, 0 mismatches in 76,000
rounds, 105 of 105 controls refused). Round ten group SCH
([`takeover-queue-round10.md`](takeover-queue-round10.md) §1). **Widened for
round twelve's field groups** (2026-09-28, group FH, pin `430f34b`): field
mode, five shapes, the field regions and 174 field-standard callees, section
7 - every scenario shadow and `'*'` unchanged, the self-test
`scenario_harness_fh` 0 mismatches over 13 field functions with Capcom's on
both sides, 6 of 6 controls refused. **Widened for round thirteen's effect
groups** (2026-09-29, group EKH, pin `d19d803`): effect mode, the shape
`kEffect` and `Arg::kEffect`, twelve arguments, nine effect regions, 109
effect-standard callees and 10 louder re-listings, round twelve's masks folded,
section 8 - every shadow of `'*'` unchanged in its counts, the self-test
`scenario_harness_ekh` 0 mismatches over 8 rows of the cut with Capcom's on both
sides, 2 of 2 controls refused.

`src/game/scenario_harness.h` / `.cpp`: what every scenario group needs to
take a chapter's code, so that a group writes only its functions, a list
and its seeds. It is a copy of the spell harness
([`magic_harness.md`](magic_harness.md), `src/game/magic_harness.*`, not
edited) adapted to the scenario banks, **with its API one for one**: a group
file written for `magic_harness` with `magic_harness` -> `scenario_harness`
and `MH_` -> `SH_` substituted compiles against it. It keeps its own
recorder pool and its own active flag, so the spell groups' fuzzes and the
scenario groups' run side by side under `BOF3X_SHADOW='*'`. A later fold
may factor the spell, scenario and area harnesses into one; not this round.

## 1. What a bank function is, to the harness

The scenario engine reaches a chapter's code through three tables indexed
by the chapter byte `Cond_ByteFA` (`0x8034E0`), named this round in
`symbols.toml` ([`scenario-roots.md`](scenario-roots.md) §2 has the
readers):

| Table | Per chapter | Named |
|---|---|---|
| `Scenario_Hooks` `0x662C80` (20) | a five-slot vtable | `Scena00_Hooks` .. `Scena19_Hooks` (`0x660CD0` .. `0x662C58`) |
| `Scenario_CallATables` `0x660B84` (20) | call table A (`Scenario_CallA` `0x5341A0`) | `Scena00_CallA` .. `Scena19_CallA` (`0x65F664` ..) |
| `Scenario_CallBTables` `0x660BD4` (20) | call table B (`Scenario_CallB` `0x5341C0`, named, Capcom's) | `Scena01_CallB` .. `Scena19_CallB` (chapter 0 has none: its pointer is 0) |

The two call-table sets lie back to back from `0x65F664`, so each table
runs to the next one's pointer (`tools/scenario_rows.py` `read_roots`).
Their entries are not in any chapter's band: they are an engine-side block
of 99 starts at `0x519890..0x51AC50` (the tool's unit `CALLS`), in no
wave-one group.

A bank function is one of these call shapes, the new field `Clone::shape`
(default `kSlot`):

| `Shape` | Called by | Arguments | How the harness calls it |
|---|---|---|---|
| `kSlot` | vtable slot 0: `Field_ModeDispatch` `0x56D690` every field frame | none | ten random words (cdecl: unread) |
| `kState` | a chapter's frame or run dispatcher through its `.data` state / run table, or a direct call of the chapter's own | none | as `kSlot` |
| `kHook` | slot 2 `Scenario_StepHook`, 3 `Scenario_ArriveHook`, 4 `0x56D7A0` | `(x, z)`, 16.16 | `x`, `z` a cell `0..0x7F` and a random fraction; the answer's `al` compared (`ret_mask` `0xFF` unless the clone says otherwise) |
| `kEntry` | call table A or B through `Scenario_CallA` / `B`, the caller's arguments in place | 0..3 cdecl words | ten random words; the group's `args` sets what the entry reads |
| `kObject` | slot 1, `0x56D6D0` (`push esi`: the field object) | the object | one of the first four `Sprite_Objects` records unless `args` says |

Every shape reads the same state: the chapter bytes, the flag rows, the
field and party objects, the sprites - section 4.

## 2. How ours calls out

As the spell harness's, with `SH_`:

| Call | Write | In the game |
|---|---|---|
| a named callee, Capcom's or ours | `SH_CALL(Flags_Test)(row, 6)`, `SH_CALL(Field_ChangeArea)(0x18, x, z, 3)` | the name |
| an unnamed one, or one another group owns | `SH_AT(void (__cdecl*)(unsigned), 0x4410B0)(1)` (in the group's `_callees.h`) | that address |
| a handler out of a stack table | `scenario_harness::Phase(address)()` | the address |
| a handler out of a `.data` state / run table read in place | the table's word, called as it is (`Scena00_Frame`) | the table's entry; during the fuzz the recorder the harness swapped in |

A function of the group's own that another calls directly (`E8`) is called
by name, `SH_CALL(Scena00_Area02)()`, and listed in the group's callees as
`Answer::kPhase` - its recorder logs the chapter bytes it ran with.

## 3. What a group writes

Everything [`magic_harness.md`](magic_harness.md) §3 lists, under
`scenario_harness`:

1. **Ours**, `src/game/scena_<group>.cpp`, calling out as above, and a
   `_callees.h` for the raw addresses.
2. **The clone table**: `python tools/scenario_rows.py --unit <GROUP>
   --clones` (the brief's paths: `--exe`, `--analysis`, `--sibling`) prints
   it for the band's functions not yet ours (`--with-ours` for all): each
   function's extent (recursive descent, jump tables bounded at their `cmp`
   / `ja`, a two-level switch's byte table counted in), every `E8` / `E9`
   that leaves it, stack-table immediates, a note for each call / `jmp`
   through `.data` (list it as a `DataTable`), `REFUSED` lines for what a
   copy cannot carry, and for a root its chapter, slot and shape (the shape
   and a hook's `0xFF` are printed into the `Clone` line). Rename nothing:
   the names come from `symbols.toml` once the group names them.
3. **A `scenario_harness::Group`**, `_fuzz.cpp`: clones, callees, data
   tables, regions, `seed`, `disturb`, `rounds`, and the optional `settle`,
   `phase_span`, `args` - the same fields, defaults and meaning as the
   spell harness's - plus **`chapter`** (default 0), written into
   `Cond_ByteFA` every round before the seed, with the flag-row pointer
   `0x929ED0` set to `Cond_Flags + 8 * chapter` as `Scenario_Start` sets it.
   Because the `Group` is an aggregate, a group sets the new fields after
   the positional ones: `sh::Group g = {...12 fields...}; g.args = &Args;
   g.chapter = 1;`.
4. **One call**, `scenario_harness::Run(group)`, from the module's
   `_Inject()` under its `BOF3X_SHADOW` name, before the `BOF3_INJECT` lines.

A `.data` table the chapter indexes past its end: ours reads the word in
place and aborts with a message where the original would jump to what is
not code (the round-nine rule for tables); the seed keeps the index inside
the table. `scena_sc0.cpp`'s `CodeAt` is the pattern.

## 4. What the harness does

- **The recorders.** 256 stand-ins of its own (`Stub<I>`, ten argument
  slots), assigned at start-up: the group's callees, then the standard set,
  then a handler recorder per stack-table immediate and `.data` table entry.
  A callee's recorder logs its arguments masked to what the callee reads,
  disturbs, and answers by its kind (the spell harness's kinds, with its
  `kFlag` / `kBool` remix). A handler's recorder, and a `kPhase` callee's,
  logs the chapter bytes it ran with: state `0x8034E2`, run `0x8034E4`,
  step `0x8034E5` and chapter in one word, the timer `0x8034E6`, and
  `Sprite_Current`.
- **The standard callees** (`kStandard`, 70): the named part of the scenario
  walk's frontier (`analysis/scenario_roots.json` `frontier_functions`) less
  the draw primitives, each `SH_OURS` or `SH_THEIRS` as `symbols.toml` says
  on 2026-09-27 - messages (`Msg_OpenScript`, `Msg_OpenSystem`,
  `Text_DrawAt`); the area and party (`Field_ChangeArea`, `Party_DropIn`,
  `Party_Join`, `PartySet_Load` / `LoadFirst` / `LoadSecond`,
  `Char_HealHp` / `Ap`, `Inventory_Add` / `Count`, `Item_NamePtr`); the flags
  (`Flags_Test` a `kBool`, `Flags_Set`, `Flags_Clear`, `ScriptFlags_Set40` /
  `Clear40`, `ObjTrio_SetBit40`, `ObjTrio_ClearBit40`); the engine
  (`Scenario_CallA`, `Scenario_CallB`, `Transition_Start`,
  `ClutStrip_FadeTo` / `Restore`, `Field_LoadingFrame`, `Port_DroppedCall`);
  sprites, event objects, effects (`Sprite_EnsureAnimation` - its whole
  dword logged -, `Sprite_SetAnimation` / `At` / `Bank`,
  `Sprite_FaceDirection`, `EventObj_Face`, `EventObj_SetFlags`,
  `EventObj_Reset`, `EventOp_6x`, `Field_ObjectInHome`, `Effect_FindFree`
  and `Effect_SpawnAt` a byte `0xFF..0x13`); the map and camera
  (`MapView_SetElevation`, `Kind2_Place`, `AreaMap_Elevation`,
  `AreaMap_SetupEntries`, `AreaMap_ByteAt`, `AreaMap_SetByte`,
  `MoveCmd_TestFB`, `Field_ViewReset`); sound and music (`Sound_PlayEffect`,
  `Sound_PlayById`, `Sound_LoadStream`, `Sound_StreamDone`,
  `Sound_ResumeAll`, `Music_LoadFile`, `Music_Play`, `Music_FadeOutStop`,
  `Music_FadeOut`, `Music_FadeIn`); files and tasks (`LoadDatFile`,
  `File_LoadDone` a `kBool`, `Task_Sleep`, `Task_Create`, `Task_Exit`,
  `Task_Restart`, `Rand`); and by address the five unnamed functions chapter
  0 calls: `0x4410B0` (SE's: an event battle's set-up by index), `0x532ED0`
  (the party placed at x, z for event battle n), `0x57C6B0` (the camera
  turned toward an angle, `al` 1 while turning), `0x56FCA0` (the view shift),
  `0x56D6F0` (`Field_StatusBits |= 0x80`). A callee that changes hands (SE
  takes `EventObj_Face`, a later round `Rand`) fails at start-up with a line
  saying so and moves column.
- **The state** (22 standard regions, 9,996 bytes; then the group's):

  | Region | What |
  |---|---|
  | `0x8034E0`, 0x14 | the chapter bytes: `Cond_ByteFA`, `Field_StatusBits`, state `E2`, run `E4` (`MoveScript_Var7`), step `E5`, timer `E6`, `E8`, .., `Cond_ByteFD` `F1` |
  | `0x903F90`, 0x108 | `Cond_Flags` (8-byte rows; the story flags `0x904030`, the party lists `0x904061..`) |
  | `0x929EC0`, 0x14 | `Field_MemberCount`, `Camera_Angles`, `Cond_AngleFB`, the row pointer `0x929ED0` |
  | `0x66C810`, `0x66C7D8`, `0x904EFC` | the wait word `MoveScript_WaitWordDA`, `Field_Request`, `Game_AreaNumber` |
  | `0x903840`, 0x20 | `Camera_Distance`, the chapters' counters `0x903848..0x90384B`, the effect slot `0x903850` |
  | `0x9039A0`, 8; `0x903A04`, 0x10 | `Field_ScriptFlags`; the pending area change |
  | `0x937F88`, 0x14 | `Sprite_Current`, `Gfx_ClutStripDirty`, `Frame_Counter`, `0x937F98` |
  | `0x7E0918`; `0x7E0940`, 0xA4 | `Draw_PassFlags`; `Sprite_Kind2` |
  | `0x802D40`, 3 x 0x14C; `0x905D98` | `ObjTrio`, the party's field objects; `Field_State` |
  | `0x7DEE80`, 30 x 0xA4; `0x802000`, 4 x 0xA4 | `Sprite_Objects`, `Sprite_ObjectsExtra` |
  | `0x7E11E0`, 20 x 0x80 | `Effect_Objects` |
  | `0x905E60`, 0xC; `0x929F14`, 0x14; `0x92A0C0` | `Field_Kind2Z` / `X`, `MapView_Redraw`; `MapView_FocusX` / `Z`, elevation, column, row; `MapView_CornerPtr` |
  | `0x7E1BE8`, 8 | `Input_Held`, `Input_Previous`, `Input_Pressed` |
  | the harness's own, 0x100 | what `MapView_CornerPtr` points at |

  Each round: random bytes; the group's chapter into `Cond_ByteFA` and the
  row pointer to its row; `Sprite_Current` at one of the first four
  sprite records, `Field_State` at an `ObjTrio` record, `MapView_CornerPtr`
  into the harness's buffer; then the group's seed; then the arguments by
  shape and the group's `args`; theirs, then ours; every region and the log
  compared. 2,000 rounds per function by default.
- **The disturbance.** Two calls in three move one cell a chapter handler
  reads again after a call: the step `0x8034E5` or the run `0x8034E4`
  (below `phase_span` when the group sets it), the wait word (0 or not), a
  flag of the chapter's row, `Sprite_Current`, `Frame_Counter`, the counter
  byte `0x903848`, `Field_Request` (2 two times in three), the timer
  `0x8034E6`, a bit of `Field_ScriptFlags`, or (the group's `disturb`) a
  cell of the group's; then the group's `settle`.
- **The copies, the report**: as the spell harness's - `CloneOriginal` with
  every call re-aimed, jump tables moved into the copy, `.data` tables
  swapped for their recorders while the fuzz runs (up to 32 entries a table
  here); one line of totals, the coverage, the first twelve mismatching
  rounds and a `Fatal` (exit 3).

**Verified against chapter 0** (the table of
[`takeover-queue-scenario.md`](takeover-queue-scenario.md) §2, checked by
reading all 19 functions): every cell that table lists is read, and chapter
0 also reads and writes the camera words, the pass and script flags, the
pending change, the counters `0x90384A` / `B` and `0x903850`, the effect
records, sprite records 0..3 and extra 0..3, the kind-2 sprite, the view
focus and `Input_Pressed` - all now standard. What it touches that is its
own (a CLUT copy, 18 dwords at `0x904608`, an `Area_Descriptors` entry's
block) is in its group file. Chapter 0 does not reach `Scenario_Start`: its
load path is not covered by this proof (`Scena00_Start` waits on
`File_LoadDone`, a recorder).

## 5. What it cannot do

- **Tell a chapter's meaning.** The fuzz proves ours equals Capcom's on the
  state it builds; what a scene looks like is the recorded route's
  ([`takeover-queue-scenario.md`](takeover-queue-scenario.md) §5).
- **Reach what the seed does not.** Handlers wait on exact counter values
  (`0x903848` 0xB, `0x90384A` 0x71): a group seeds each value its steps
  compare with, or the step never runs.
- **See the arguments a `.data` table's handler is called with.** A handler
  recorder logs the chapter bytes, not arguments (a state handler has none,
  and its words would be each caller's frame). Chapter 0's object table
  passes two; its group stands a stand-in of the entry's own type in the
  table instead (`scena_sc0_fuzz.cpp`, `ObjectEntry`).
- **Copy a function whose conditional jump leaves its extent** (`REFUSED`).
  None in chapter 0.
- **Know a callee's arity or width** beyond the standard set's reading; a
  group re-lists a callee it needs recorded otherwise (its listing stands).
- **Run the real load.** `Task_Sleep`, `File_LoadDone`, `LoadDatFile` are
  recorders; `Scenario_Start(n)` has no harness path yet.


The round-ten notes in [`magic_harness.md`](magic_harness.md) §5 (an `args`
hook that writes memory is lost, a `kPhase` callee's `effect` never runs,
effects draw from `Noise()` only, an entry `jmp` refused by `CloneOriginal`,
a clone past 64 call sites copied by the fuzz file) hold here too: SX, SC12
and SC5 met them through this harness.

## 6. Shadow name and self-test

The harness has no shadow name of its own: each group's is its module's
(`scena_sc0` for the proof). `BOF3X_SHADOW='*'` runs every group of both
harnesses.

Chapter 0 through it (2026-09-27, `BOF3X_SELFTEST_ONLY=1
BOF3X_SHADOW=scena_sc0`, exit 0, this worktree): 76,000 rounds over 19
functions (4,000 each), **68,403** calls to the stand-ins, **0 mismatches**,
10,336 bytes of state (32 regions); **105 of 105 controls refused** by a
count ([`scena_sc0.md`](scena_sc0.md) §5). Every standard callee chapter 0 calls and
every handler its three tables hold is reached (coverage in
[`scena_sc0.md`](scena_sc0.md) §4). `BOF3X_SHADOW='*'`: exit 0.

**For the groups written against the contract (SE, SC1, SC3, SC11, SC12):**
nothing in a spell-style group file needs to change to compile. To use what
the scenario harness adds: set `g.chapter` (else 0 is written into
`Cond_ByteFA` every round); mark each root's `shape` in its `Clone` line
(the positional field after `calm`: `..., ours, 0xFF, false,
scenario_harness::Shape::kHook}`) - `kSlot` / `kState` need nothing, a
hook without `ret_mask` is compared on `al`, `kObject` gets a sprite record
and `kEntry` random words unless `args` says; the magic `at::` names
(`kTasks`, `kTarget`, ...) still compile but are no region here, and the
scenario's are `at::kChapter`, `kState`, `kRun`, `kStep`, `kTimer`,
`kByteFD`, `kCondFlags`, `kFlagRow`, `kWait`, `kRequest`, `kArea`,
`kCounter`, `kObjTrio`, `kSprites`, `kEffects`. `SH_PICK`, `SpriteRecord`,
`ObjectOf`, `FlagRow` are the seeding helpers.

## 7. Round twelve: the field engine's groups

Round twelve's seven field groups - FC1, FC2, FC3, FE1, FE2, FO, FS, 323
functions, the field rows of `analysis/round12_cut.tsv`
([`takeover-queue-field-battle.md`](takeover-queue-field-battle.md) §3) - run
through this harness in wave two. Group FH (2026-09-28, stage A of wave one,
pin `430f34b`) widened it for them. Everything below is **field mode**: a
group turns it on with `g.field = true`, or by giving any clone one of the
new shapes. Without it the harness is the scenario round's to the byte (the
proof, 7.8). FH's measurements are in the session scratchpad (`fh/`:
`frontier.py`, `field_rows.tsv`, `frontier.tsv`, `cross.tsv`, `shapes.tsv`),
game-derived and not committed; `tools/band_rows.py` (group RT) is the
clone-table tool the groups use.

### 7.1 What changed

| Where | Added | Default (today's behaviour) |
|---|---|---|
| `Shape` | `kSprite`, `kScript`, `kCursor`, `kCall`, `kMenu` (7.4) | the five shapes of section 1 unchanged |
| `Clone::pointers` | which arguments are pointers, `ArgAt(i, Arg::k...)` OR'ed | 0: every argument as the shape draws it |
| `Group::field` | field mode: the field regions, put-backs, disturbance, handler log | false (also on when a clone has a new shape) |
| `Group::sprite_span` | `Sprite_Current` +1..+4 drawn below it (kSprite) and kept below it by the disturbance | 0: random |
| `Group::menu_span` | the menu block's `0x929F01` / `0x929F02` likewise (kMenu) | 0: random |
| `Callee::guard` | a pointer argument dereferenced only where readable | false: as before |
| `kDerefString` (2026-09-29, the capture review of rounds 10..12) | a `deref` of 0xFF: the argument hashed as a string to its NUL (64 bytes at most, each byte `Readable`), not a fixed count; `Text_DrawFont8`, `Text_DrawFont12`, `Text_CharCount` and `Menu_DrawSkillRow`'s name take it (`TextRecord_Set`, which is given a length, keeps its 16) | a number: that many bytes, as before |
| `at::` | the field cells (7.3), `kWindows`, `kMessageCells`, `kTextRecords`, `kMapCells` | |
| helpers | `Script()`, `Cursor()`, `Scratch(i)`, `Packets()`, `Text()`, `InRegions()`, `InFieldRuns()`, `InChapterBank()` | |
| stand-ins | `kSlots` 512 (was 256), `kMaxRegions` 64 (was 48); the 174 field-standard callees (7.5) registered for every group **after** its handlers | a handler address stays a handler |
| disturbance | cases 10..13 move field cells (7.3) | no-ops outside field mode |
| a handler's recorder | its fourth word `Sprite_Current` +1..+3 and `0x929F01` | 0 outside field mode |

Three entries of `kStandard` are round-twelve functions - `Scenario_CallB`
(FE2), `EventOp_6x` and `ObjTrio_ClearBit40` (FO). They now name their
address and take the key from the name, so they register whichever side holds
the name: the owning group takes them without a harness edit.

### 7.2 The band: what each test accepts

The harness has one address test of its own and gains one:

- **`Register`** (unchanged): a callee listed as Capcom's (key equal to its
  address) must lie in `.text` `0x401000..0x5C3000`, and one listed as ours
  must not. Every field run lies inside `.text`, so it accepts every field
  function as a callee, Capcom's or ours; `FIELD_THEIRS` entries and the three
  above pass both ways.
- **`InFieldRuns` / `InChapterBank`** (new, field mode only): a clone whose
  base lies outside the field runs of plan section 1 - `0x461800`,
  `0x469D10..0x46D5ED`, `0x5172C0..0x5195F9`, `0x525390..0x526DB0`,
  `0x52D080..0x5372D8`, `0x56D240..0x5729F8`, `0x5738A0..0x57CD89`,
  `0x57FF80..0x5859F9`, `0x58C7A0`, `0x593960..0x594060` - and outside the
  chapter bank `0x537F20..0x56D5E0` is **named in the log**, not refused (a
  group may clone a shared tail outside its band).
- The chapter bank itself is `tools/scenario_rows.py`'s (`BANK_LO`,
  `BANK_HI`), a tool's, not the harness's; the field runs' clone tables are
  `tools/band_rows.py`'s.

### 7.3 The regions and the seeds

FH read every absolute address in the 323 (instruction operands and
immediates, capstone, each function to its extent) and counted the groups
touching each cell. **Standard in field mode** - three or more groups:

| Region | Groups | What, and what is put back each round |
|---|---|---|
| `Gfx_PacketNext` `0x7E0670` | FC1 FC2 FE1 FE2 FO | the packet cursor: into the harness's packet buffer (0x800) at `+0..0x1F0` |
| `0x905B70` + `0x38` | FC2 FC3 FE1 FE2 FS | `CameraTurn_Steps`, `Field_EdgeBits`, `Field_InputFlags`, `Field_ScriptFlags2` `0x905BA4`, `Field_InputHeld` |
| `0x929ED4` + `0x40` | FC1 FE2 FO FS | `MapView_BuildFlags`, the menu block `0x929F00..` (mode, state `+1`, step `+2`, timer `+4`, cursor bytes), `Field_Kind2Hold` `0x929F12` |
| `0x903A14` + `0x80` | FC1 FE1 FE2 FO FS | the window style byte `0x903A5A` (`Menu_DrawBox`'s colour), the records `0x903A70..`, `Field_ActorStates` |
| `0x904BA0` + `0x20` | FE1 FE2 FO FS | the text scratch the name copies and `sprintf` write |
| `0x904098` + `0xC8` | FE1 FE2 FS | the save block past `Cond_Flags`' `0x108`, to `0x904160` (`Party_Zenny` `0x904058` is already inside `Cond_Flags`) |
| `0x904560` + `0x1A0` | FE1 FE2 FO FS | the save block's bytes `0x904560..0x904700` |
| `AreaMap_Header` `0x8CB580` + `0x2000` | FC1 FC2 FE2 | the area block's first 8 KiB (as `area_harness`'s): width and height bytes below `0x20`, the offset word below `0x100` |
| `AreaMap_Bytes` `0x905D94` | (with the block) | into the block, `+0x800` |
| the harness's packet, text, script, cursor and scratch buffers | | random; the cursor into the script buffer at `+0..0x3F` |

Also put back in field mode: `MapView_Row` below `0x38` and `MapView_Column`
below `0x1C` (they sit in the standard `0x929F14` region; `MapView_Cells`'
readers wrap an index once, so a random word indexes far outside the table -
the self-test's `0x5728D0` faulted on it until this).

Already standard before round twelve, and read by the field runs: the chapter
bytes, `Cond_Flags` with the story flags and `Party_Zenny`, `Field_Request`,
`Game_AreaNumber` / `MoveScript_FAWord`, `0x903840..` (`Camera_Distance`, the
counters, `0x903850`), `Field_ScriptFlags`, `Sprite_Current` (194 of the 323
read it), `Frame_Counter`, `MoveScript_F3Divisor`, `ObjTrio` (the party's
working records, their HP included) and `Field_State` (34), `Sprite_Objects`,
`Effect_Objects`, `Field_Kind2X` / `Z`, the view focus, the pad.

**A group lists it** (one or two groups; `at::` names the ones FH met):
`WindowRecords` `0x803160` + 22 x `0x24` (FS: the shop's window cells
`0x803160..0x803478`, `at::kWindows`), `Text_Records` `0x904CE0` (FE1, FE2),
`MapView_Cells` `0x904F20` + `0xC40` (FE2), the message cells `0x7DEE20` +
`0x60` (`EventScript_FlagBank`, the message word, pen and line; FE1),
`0x903860..` (FE1), `0x9039A8..0x9039F8` (FE2), `0x6BC740..0x6BC8C8` (FO,
FS), `0x6BE080..` and `0x939850..` (FE2), `0x803480` `MoveScript_PartyRecords`
(FO), `0x7E0700` (FC3), `0x92C4C0` and `0x7E1200..` (FC2), `0x9048B0..` and
the save block's words past `0x904160` (FE1, FE2). **Not a region, on
purpose** (as `area_harness`'s): the image's tables - `Area_Descriptors`, the
`NameTable_*`, `Field_DirectionSteps`, `Field_MoveSpeeds`,
`MoveScript_EffectState`, `0x6696DC` / `0x6696E0` - which no field function
writes; they stay in place and are read for real.

**The field disturbance** (cases 10..13 of the 16, field mode only): a state
byte `+1..+4` of `Sprite_Current` (below `sprite_span` when set), the menu
state or step byte (below `menu_span`) or its timer, the packet cursor, a bit
of `Field_ScriptFlags2` or the pad's pressed word.

### 7.4 The shapes

FH classified the 323 by how they are reached (`pc_hidden.json`'s references,
the `E8` sites in `pc_funcs.json`'s callers) and what they read (a first
shape per function in `fh/shapes.tsv`; the group reads and decides):

| Shape | What | Of the 323 (FH's first pass) |
|---|---|--:|
| `kSprite` | an object's state handler, void, no arguments, run on `Sprite_Current`; the field code dispatches on its bytes `+1` (`Field_LeaderFrame` `0x660918`, `Field_MemberFrame` `0x65F960`), `+2`, `+3` (`0x525390` through `0x660140`), `+4` (`0x525CC0` through `0x66017C`), each `jmp [table + byte * 4]` unchecked - `sprite_span` keeps them inside | 133 |
| `kMenu` | a shop, save-point or field-menu state: void, no arguments, dispatched on the menu block's state `0x929F01` (`0x5837E0` through `0x6641BC`, `0x58C2C0` through `0x66739C`) or step `0x929F02` - `menu_span` keeps them inside | 33 (FS) |
| `kCursor` | an event-script condition: `EventScript_Conditions` `0x663B30`'s entries take `const unsigned char **position` and answer `al`; a[0] is `Cursor()`, the cursor cell pointing into the script buffer | 12 (FO) |
| `kScript` | an event-script op: `EventOp_3x`, `4x`, `6x`, `7x`, `Ax` take `const unsigned char *op`; a[0] is the cursor's op, `Script()` | 5 (FO) |
| `kCall` | a cdecl helper called by `E8`: 20 read more than three words (`MoveCmd_OpE9` seven), 31 answer in `eax` / `al` that a caller reads (set `ret_mask`); `pointers` makes argument *i* a sprite record, a scratch pointer (`Scratch(i)`, 0x40 bytes each) or the op | 77 |
| `kState` (unchanged) | a state handler of a `.data` table that does not read `Sprite_Current` (FE2's draw layers, the camera turn's states), or a direct no-argument call | 63 |

A function answering in `eax` is any shape with `ret_mask`; a hook `(x, z)`
(`kHook`) and a call-table entry (`kEntry`) keep their meanings. Of the 323,
202 are reached through a `.data` table - the group lists each table as a
`DataTable` (its length to the next table's start; `pc_xref.json` names the
dispatcher), and seeds or spans the byte it indexes by.

### 7.5 The standard callees

**The frontier**: every call and tail `jmp` out of the 323 that lands outside
them - 208 callees. 32 were in `kStandard` already; two are not callees but
code inside a cut row's extent (7.6); **174 are new**, `kField` in
`scenario_harness.cpp`, typed from `symbols.toml`'s `ret` / `params` (masks by
parameter type, `kFlag` for a byte answer), the 26 without a signature typed
by reading. `guard` is set on all of them: a pointer argument is hashed (16
bytes of a char or void pointer, 8 of short, 12 of long) where it is
readable, and logged as its value where not. The most called (sites in the
323): `Sound_PlayEffect` 86, `Sprite_EnsureAnimation` 52 (standard),
`Menu_DrawPiece` 48, `Text_DrawAt` 45 (standard), `AreaMap_ByteAt` 41
(standard), `Sprite_ScriptTick` 33, `AreaMap_Elevation` 33 (standard),
`Crt_sprintf` 27, `0x52CFE0` 25, `Sprite_UpdateScreenSlot` 24,
`MapView_SlopeAt` 24, `MapView_GroundAt` 23, `Text_DrawFont8` 20,
`Menu_DrawPieces` 19, `Menu_DrawBox` 18, `Effect_Release` 18. By family:

- **Field** (20): `Field_CellAhead`, `CellHasEvent`, `CellsBlock`, `DrawFrame`,
  `JumpCamera`, `JumpSetUp`, `JumpStart`, `LeaderPushObjects`, `LeaderStand`,
  `LeaderStepTick`, `MemberSprite`, `MemberTimers`, `MembersFrame`,
  `ObjectBlockedAhead`, `ObjectOpenDirection`, `ObjectRandomTurn`,
  `PartyLoad`, `RunTaskRecords`, `TileD0`, `WayBlocked`.
- **Menu** (19): `Menu_DrawBackdrop`, `DrawBlackScreen`, `DrawBorder`,
  `DrawBox`, `DrawCell8`, `DrawCursorBox`, `DrawHand`, `DrawIcon8`,
  `DrawItemIcon`, `DrawItemRow`, `DrawMemberStatus`, `DrawMoneyBox`,
  `DrawPiece`, `DrawPieces`, `DrawScrollBar`, `DrawSkillRow`, `DrawTitleBox`,
  `ListScroll`, `YesNo`.
- **Sprite** (16): `ApplyVelocity`, `ClearSteps`, `FindFree`, `InitFromEntry`,
  `LoadPalette`, `ObjectAt`, `QueueOverlay`, `ScriptTick`, `ScriptTickOnce`,
  `SetTint`, `ShadeFadeBegin`, `ShadeFadeStep`, `ShadeLower`, `UpdateScreen`,
  `UpdateScreenA`, `UpdateScreenSlot`.
- **Gte** (15) and **Gpu** (11): `Gte_MulMatrix0`, `PopMatrix`,
  `PrimDepthFlat4_10`, `PrimDepths3_10B`, `PushMatrix`, `RotMatrix`,
  `RotTrans`, `RotTransPers`, `RotTransPers3`, `RotTransPers4`,
  `SetRotMatrix`, `SetTransMatrix`, `StoreDepthF4`, `VectorNormal`,
  `VectorNormalS`; `Gpu_GetClut`, `GetTPage`, `SetDrawMode`, `SetLineF2`,
  `SetPolyFT4`, `SetPolyG3`, `SetPolyG4`, `SetSemiTrans`, `SetShadeTex`,
  `SetSprt`, `SetTile`; with `Gfx_CommitPrim`, `Gfx_ClutStripCopyRow`,
  `Prim_SetTexture`, `MapView_LinkPrimAt`.
- **The party, items and text**: `Party_ApplyRecord`, `Count`,
  `ExtraScreens`, `MemberAt`, `MoveMember`, `UpdateScreens`;
  `Char_AbilityList`, `ExpForLevel`, `LoseHp`, `RecalcStats`; `Skill_ApCost`,
  `CanUse`, `FlagIndex`; `Item_CanUse`, `HelpMessage`, `IconKind`;
  `KeyItem_Has`, `Inventory_Remove`, `AbilityList_Add`, `Equip_PreviewSet`,
  `Actor_EquipCount`, `Member_ClearState`, `PartyRecord_Clear`, `Zenny_Add`;
  `Text_CharCount`, `DrawFont12`, `DrawFont8`, `DrawSmall`, `TextRecord_Set`,
  `Msg_SystemPtr`, `SaveMenu_DrawSlots`, `Input_AutoRepeat`.
- **The map, areas, camera, movement**: `AreaMap_CellsNone`, `Frame`,
  `SetHeight`, `Slope`; `MapView_GroundAt`, `SlopeAt`; `Area_CellHook`,
  `LinkAt`, `RunPlacement`, `TestCondition`; `CameraTurn_Start`, `Step`,
  `End`; `MoveCmd_AttachOffset`, `Move`, `TestFC`; `MoveScript_ObjectKind`,
  `Step`, `TintFrame`; `Effect_Release`, `RunObjects`, `Spawn`;
  `Tint_Release`, `Math_Sin`, `Cos`, `EventOp_0x`, `Snd_LoadBankFile`,
  `Scena17_DrawLogo`, `Area104_LeaderRun`, `Area121_LeaderRun`.
- **Capcom's by name** (`FIELD_THEIRS`, hand-agnostic): `Crt_sprintf`,
  `MoveCmd_Move`, `Effect_Spawn`.
- **Unnamed, by address** (26, typed by reading): the draw helpers
  `0x52CFE0` (a sprite primitive at the packet cursor; answers it),
  `0x52CF60`, `0x468950`, `0x469750`, `0x46D5F0`, `0x5942C0`, `0x5947D0`,
  `0x594410`, `0x594AD0`; the camera matrices `0x494060`, `0x494110`,
  `0x4941E0`; the dispatchers `0x42D710`, `0x57DFF0` (by `0x929F00`),
  `0x586670` (by `0x9398CF`); `0x52CE60`, `0x52CED0`, `0x537500`,
  `0x5372E0`, `0x585A00`, `0x594700` (al), `0x594790` (al), `0x594D90`,
  `0x591AC0`, `0x58BD50` (swaps two bytes); `0x5B9550`, the CRT's `_ftol`, is
  `kThrough` - it pops `st(0)`, so no recorder can stand in for it. Of the
  26, twelve are round thirteen's area-overlay rows (world 0), twelve are
  part 2 or part 7 rows of the catalog, `0x586670` a part-6 row and
  `0x5B9550` the CRT's; entries by address stay valid when a later round
  takes them.

**Louder where the caller reads back**: the pointer answers land in the
harness's buffers - `Msg_SystemPtr`, `Char_AbilityList` and, in field mode,
`Item_NamePtr` (its callers read the name through it; a field re-listing of
the `kStandard` entry) answer into the text buffer, `Gpu_SetPolyG4` its
primitive, `Text_DrawSmall` its text, `0x52CFE0` the packet cursor;
`Gte_RotMatrix` / `MulMatrix0` fill their matrix and answer it, the other
`Gte_*` outputs and `MoveCmd_AttachOffset`'s are filled with noise (floats as
small whole numbers), `Crt_sprintf` writes up to seven letters;
`Gfx_CommitPrim` advances the packet cursor by its size; `Zenny_Add` moves
`Party_Zenny` (held at 9,999,999, al 0 then) and the tally `0x904138`;
`0x58BD50` swaps. An effect writes only inside the regions or the caller's
stack.

### 7.6 The cross-group edges

A callee that is one of the 323 is not standard: its owning group takes it,
and the others call it by address (`SH_AT`, listed in their callees) until it
merges. FH's pass over the 323's `E8` / `E9` sites (`fh/cross.tsv`):

| Caller group | Callee | Callee group | Callers |
|---|---|---|---|
| FC1 | `0x46BF80` | FC2 | `0x46BB50` |
| FC1 | `0x46D0E0` | FC2 | `0x46B7C0` |
| FC1 | `0x57AD10` `EventOp_6x` | FO | `0x46A600` (standard, hand-agnostic) |
| FC2 | `0x5307C0` | FE1 | `0x46D180` |
| FC2 | `0x5728D0` | FE2 | `0x46D180` |
| FC3 | `0x534C20` | FE2 | `0x526BA0` |
| FC3 | `0x535FC0`, `0x535FE0`, `0x536050`, `0x5360C0`, `0x536130`, `0x536170`, `0x536290`, `0x5362D0`, `0x5363C0`, `0x536440` | FE2 | ten of FC3's leader states `0x525CE0..0x525F90` |
| FC3 | `0x536F10` | FE2 | `0x5172C0` |
| FE1 | the same ten, and `0x5364D0`, `0x536550`, `0x5365D0` | FE2 | FE1's turn states `0x52F970..0x52FB50` |
| FE1 | `0x56D6B0` `Field_ObjectTrigger` | FE2 | `0x52F8F0` |
| FS | `0x574400` | FO | `0x581300` |

No field function calls a battle group's (BE1..BE7) and none calls back into
a group that calls it: the graph is acyclic. Callee first, a merge order that
never leaves a raw call to code already ours is **FE2, FO, FE1, FC3, FS,
FC2, FC1** (FE2 is called by four groups; FC1 calls three). Raw calls are
correct in any order - the address is the jmp to ours once the callee is
taken - so the order matters only to the rebinding step.

**Two starts that are not in the cut** but are reached from outside the
function whose extent holds them: `0x5254A0` (code after `0x5253E0`'s first
`ret`, inside FC3's `0x5253E0`, tail-jumped from `0x52548E` in it and from
`0x5256C7` in FC3's `0x5256A0`) and `0x536EC0` (inside FE2's `0x536E90`,
tail-jumped from `0x536E7D` in FE2's `0x536E70`). Each group copies its host
whole and lists the target as a `kPhase` callee of the other function, or
takes it as a function of its own; both are group-internal.

### 7.7 Worked examples

**A hidden state handler** (FC3's `0x525CC0`, hidden in `0x525390`, reached
through `0x660174`): it reads `Sprite_Current` `+4` and jumps through the
eight-entry table `0x66017C` - a `kSprite` with a `DataTable`, the span
keeping `+4` inside. Ours would read the byte and call the table's word in
place (the table swapped for recorders while the fuzz runs):

```cpp
// src/game/field_fc3.cpp
extern "C" void __cdecl Fc3_LeaderSubstate() {   // 0x525CC0
    const unsigned char k = static_cast<unsigned char*>(Sprite_Current)[4];
    reinterpret_cast<void (__cdecl*)()>(static_cast<std::uintptr_t>(
        move_script::Long(scenario_harness::Mem(0x66017C + 4u * k))))();
}
// src/game/field_fc3_fuzz.cpp
namespace sh = scenario_harness;
const sh::Clone kClones[] = {
    {"Fc3_LeaderSubstate", 0x525CC0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0,
     reinterpret_cast<const void*>(&::Fc3_LeaderSubstate), 0, false, sh::Shape::kSprite},
};
const sh::DataTable kTables[] = {{0x66017C, 8}};   // to 0x66019C, the next table
void Run() {
    sh::Group g = {"field_fc3", kClones, 1, nullptr, 0, kTables, 1, nullptr, 0, nullptr, nullptr, 0};
    g.sprite_span = 8;   // +1..+4 below 8: inside 0x66017C
    sh::Run(g);
}
```

**A cdecl helper** (FE2's `0x5343C0`, called by `E8` from `0x5341E0`): a byte
and a pointer to a dword it increments when no pair of `0x9046D0..` names a
record of `0x9048B0..` of kind 4 with that byte - a `kCall`, the pointer a
scratch, the records seeded so the match happens:

```cpp
const sh::Clone kClones[] = {
    {"Fe2_CountUnpaired", 0x5343C0, 0x55, nullptr, 0, nullptr, 0, nullptr, 0,
     reinterpret_cast<const void*>(&::Fe2_CountUnpaired), 0, false, sh::Shape::kCall,
     sh::ArgAt(1, sh::Arg::kScratch)},
};
const sh::Region kRegions[] = {{0x904700, 0x200}};   // to the records' end, 0x9048F8
void Seed(unsigned) {
    for (unsigned i = 0; i < 8; ++i)
        if (sh::Half()) sh::Mem(0x9048B0 + 8 * i)[0] = 4;
}
void Args(unsigned, std::uint32_t* a) { a[0] = sh::Mem(0x9048B1 + 8 * (sh::Next() % 8))[0]; }
void Run() {
    sh::Group g = {"field_fe2", kClones, 1, nullptr, 0, nullptr, 0, kRegions, 1, Seed, nullptr, 0};
    g.args = Args;
    sh::Run(g);   // field mode by the kCall shape
}
```

The names are placeholders (a group names its functions); the extents,
tables and seeds are the ones `scenario_harness_fh.cpp` runs.

### 7.8 The proof

**(a) Nothing moved.** `BOF3X_SHADOW='*'` headless at `430f34b` (this
worktree's build, `fh/star_base.log`) and at FH's tip (`fh/star_after.log`):
both exit 0, 949 lines of `0 MISMATCHES` after. **All 25 self-test lines
of the 17 scenario shadows** (`scena_sc0`, `se`, `sc1`, `sc2`, `sc3` two
chapters, `sc5`, `sc6`, `sc7` two, `sc9a`, `sc9b` two, `sc11`, `sc12`, `sc13`
two, `sc15` five, `sx`, `sx2`, `calls` - 4,833,000 rounds) **and their
coverage lines are byte-identical**: the same rounds, the same calls to the
stand-ins, 0 mismatches. `'*'` has 656 self-test lines against 655, the one
more `scenario_harness_fh`'s. Twelve lines of modules that do not use this
harness moved their call counts and nothing else, all still 0 mismatches:
`sound` (117,993 -> 117,996), `magic_fx_reached` (248,158 -> 248,160),
`magic_s16`, `magic_s17`, `magic_s34`, `magic_s35`, `area_w0b`, `area_w1b`,
`area_w1e`, `area_w2b` (both), `area_w3a` (each by under 0.5 %). That is the
trap [`HANDOFF.md`](HANDOFF.md) records ("a spell fuzz's call counts depend
on the build directory"): `magic_harness` and `area_harness` put pointers to
records in our DLL into game memory (`g_records`), and this change grew the
DLL (the 256 more recorders, the field buffers), so those pointers moved. Their
sources and harnesses are untouched. A second `'*'` on FH's build
(`fh/star_after2.log`) repeats every self-test and coverage line of the first byte for byte: the runs are deterministic, so what moved is the build, not chance.

**(b) The new shapes run.** `BOF3X_SHADOW=scenario_harness_fh`
(`src/game/scenario_harness_fh.cpp`, after every group's self-test in
`inject_all.cpp`): thirteen functions of the field runs, each a copy of the
original against the original in place, so any difference is the harness's -

| Shape | Functions (group) | Through |
|---|---|---|
| `kSprite` | `0x52F980` (FE1), `0x525CC0` (FC3) | `0x6609AC` (5), `0x66017C` (8); `sprite_span` 5 |
| `kMenu` | `0x5837E0`, `0x5811B0`, `0x5845E0` (FS) | `0x6641BC` (8); `menu_span` 8 |
| `kCursor` | `0x57C1A0`, `0x57C230` (FO; conditions 1 and 9) | the cursor cell; al |
| `kScript` | `0x56E020` (FE2) | `0x662E1C` (12) |
| `kCall` | `MoveCmd_OpE9` `0x57C8E0` (FO; seven words, a sprite record, al), `0x5343C0` (FE2; a scratch pointer), `0x534420` (FE2), `0x52D880` (FE1; al), `0x5728D0` (FE2; the area block through `AreaMap_Bytes`) | `0x663B84` (4) |

`scenario_harness_fh` alone: 26,000 rounds over 13 functions (2,000 each),
10,000 calls to the stand-ins (the table handlers of `kSprite`, `kMenu`,
`kScript` and `MoveCmd_OpE9`), **0 mismatches**; 26,852 bytes of state (39
regions); 281 stand-ins registered, 174 of them the field-standard set - every
one passing `Register`'s checks. Every entry of the five tables inside the
spans was reached (`0x6609AC`'s five, `0x66017C`'s first five by
`sprite_span`, `0x6641BC`'s eight, `0x663B84`'s four, `0x662E1C`'s twelve).
Under `'*'` it runs last of the scenario harness's users and draws another
stream (the harness's generator is shared), 0 mismatches there too.

`0x56E020` is handed an object record in the game, not an op: no leaf of
the 323 is handed an op (the five `EventOp_*` call out, and the original in
place would reach the real callee where the copy reaches a recorder). It
proves `kScript`'s mechanics - a[0] the cursor's op, read through, the buffer
compared. For the same reason the field-standard stand-ins are proved by
registration only (all 174 pass `Register`'s checks at start-up, and a group
calling one it did not list fails loudly); their effects run first in wave
two's fuzzes.

**Controls** (a one-off build with an environment switch standing another
original in as "ours", not committed): `kSprite` `0x52F980` against
`0x525CC0` refused in 2,000 of 2,000 rounds; `kMenu` `0x5811B0` against
`0x5845E0` 2,000; `kCursor` `0x57C1A0` against `0x57C230` 987; `kScript`
`0x56E020` against `0x534420` 2,000; `kCall` `0x5343C0` against `0x534420`
1,812; `kCall` `0x52D880` against `0x5845E0` 2,000 - **6 of 6 refused by a
count**; `0x5728D0` against itself passed.

### 7.9 Limits

- **The field-standard stand-ins' effects are unproved** until a wave-two
  group's fuzz runs them (7.8); their typing is `symbols.toml`'s, whose
  evidence a group re-reads for a callee it depends on.
- **Spans are per group, not per function**: a group whose tables differ in
  length seeds the byte per function (`seed`), as `scenario_harness_fh` does
  for `MoveCmd_OpE9`'s `+4`.
- **The area block is 8 KiB and bounded only by the header's dims**: a
  function indexing it by a sprite's coordinates, or walking its records from
  `AreaMap_CellBase` (`0x5728D0` past its cell test), wants its seed to bound
  the index, or a region of its own (as `area_harness` §5 says).
- **`MapView_Cells` is not standard** (one group): `0x5728D0`'s readers of it
  seed it (the self-test empties it).
- **The out-parameter fills trust the size** FH read from the signature (a
  `MATRIX`'s nine shorts, a `VECTOR`'s three longs, a float pair); a caller
  whose local is smaller would be written past. FH checked `0x494060`'s only;
  a group re-reads the callers it owns.
- **A pointer into the heap** (a loaded file) is logged as its value, not
  dereferenced: `Readable` knows the regions, the stack and the image only.
- **A subset of shadows draws other numbers than `'*'`**: the counts depend
  on which shadows ran before (the base build's subset run moved four scenario
  groups' call counts exactly as the new build's did), so before / after
  comparisons are `'*'` against `'*'`, as 7.8's.
- Everything in sections 5 and 6 still holds.

## 8. Round thirteen: the effect engine's groups

Round thirteen takes the effect-object engine: 1,695 functions of
`analysis/round13_cut.tsv` in 35 groups E1A..E6D and the stage-A group EGT
([`takeover-queue-round13.md`](takeover-queue-round13.md) sections 9 and 10).
Group EKH (2026-09-29, stage A, from the round branch's `d19d803`) gave this
harness what they need: **effect mode**, a fold-back of what FC1 and FC2 wrote
in their own seeds in round twelve. Everything below is off unless a group
sets `g.effect = true` or gives a clone the shape `kEffect`; without it the
harness is round twelve's to the draw (the proof, 8.8). EKH's measurements are
in the session scratchpad (`ekh/`: `analyse.py` and `shapes.tsv` a row per cut
function, `frontier2.py` / `frontier_typed.tsv` the callees, `typeraw.py` /
`typeraw.txt` the unnamed callees read, `cells.py` / `cells.txt` the cells,
`recarg.py` the record-handing callers), game-derived and not committed.

### 8.1 What changed

| Where | Added | Default (today's behaviour) |
|---|---|---|
| `Shape` | `kEffect` (8.3) | the ten shapes unchanged |
| `Arg` | `kEffect`: argument *i* one of the 20 `Effect_Objects` records; `ArgAt` now three bits an argument (the first ten) | `ArgAt` is the only way groups encode it, so every existing clone line means what it meant |
| `Clone` | `state_span`, `sub_span` (a function's own spans for `+1` / `+2`), `kind` (its own `+5`) | 0, 0, -1: the group's |
| `Group` | `effect`, `kinds` / `n_kinds`, `state_span`, `sub_span` | false, none, 0, 0 |
| `kArgs` | 12 (E3D's `0x486AB0` reads twelve words, `0x486B70` ten): the recorders take twelve, a clone is called with twelve; arguments 10 and 11 are **derived** from the first ten, not drawn, and a recorder's second extra log entry carries `r[10]`, `r[11]` (0 below eleven arguments) | the random stream, the scratch buffer (still ten slots of 0x40) and every log entry as before |
| stand-ins | `kSlots` 768 (was 512); `kEffectOverrides` (10) registered first in effect mode, `kEffectStd` (109) after `kField` | registered in effect mode only |
| `DataTable` | up to 128 entries a table (was 32: kind 0xF's table has 58) | |
| disturbance | case 4 moves `Sprite_Current` among the 20 effect records; case 10 moves `+1` or `+2` inside its span | effect mode only |
| regions | nine effect regions (8.4) | effect mode only |
| `at::` | `kEffectStride`, `kEffectCount`, `kKindHandlers`, `kKind18States`, and the effect regions' cells | |
| helpers | `EffectRecord(k)`, `InEffectRuns()` | |
| masks | round twelve's narrower masks folded into `kStandard` / `kField` (8.6) | a mask only narrows what is compared: no draw, no count moves |

### 8.2 The band: what each test accepts

- **`Register`** (unchanged): a callee listed as Capcom's must lie in `.text`
  `0x401000..0x5C3000`, one listed as ours outside it. Every effect run lies in
  `.text`, so it accepts every effect function as a callee either way;
  `kEffectOverrides`' five rows of the cut are listed by address (key = address),
  which keeps passing when their owners take them and a group still calls them
  raw.
- **`InEffectRuns`** (new): `0x462B00..0x470000`, `0x470000..0x4A0000`
  (`0x470000..0x4941E0` and its callees, as the brief draws it - it reaches past
  the spell band's first entry `0x498FE0`), `0x4FD2E0..0x517000`,
  `0x528CD0..0x52D080`, `0x594060..0x594D8A`. A field- or effect-mode clone whose
  base lies outside the field runs, the chapter bank **and** these is named in
  the log, not refused. So an effect group's clones are not named, but **E4A's
  `0x433640`** (a `hypothesis` row in the battle band) will be - expected. The
  test only names: nothing is refused by address.
- `StandIn` looks a key up in the two new tables as in `kStandard` / `kField`.

### 8.3 The shapes: `kEffect`, and the call shapes the cut has

**How the effect code is reached** (EKH's `analyse.py` over the 1,706
functions of the cut and the 15 its spans hold that no list has, EGT's four and
the three non-functions left out): 1,259 only by `.data` cells, 382 only by
calls, 28 by calls and jumps, 12 by a call and a cell, 11 only by `jmp` (shared
tails), 8 by a cell and a jump, a stack immediate or a push, 6 by a stack
immediate or nothing a sweep sees. **1,353 read `Sprite_Current`.**

**`kEffect`**, the fold of FC1 and FC2: every round in effect mode, after the
field put-backs and before the group's seed, all 20 records get `+5` one of
the clone's kind or the group's `kinds`, `+1` below the state span and `+2`
below the sub-state span (each when set), and `+0` 0 (free) a third of the
time, else in use; `Sprite_Current` is one of them, in use. For **every** clone
of an effect-mode group, `kCall` helpers too. The disturbance moves
`Sprite_Current` among the records only, and `+1` / `+2` of the current one only
inside their spans (never when a span is 0). What FC1 and FC2 agreed on and
this keeps: `+5` the kind and a state byte kept inside the table the code
indexes, because Capcom's dispatchers do not bound their index (FC1:
`sprite_span` 20; FC2: per function in its seed); `Effect_FindFree`'s answer a
byte `0xFF..0x13`. Where they differ: FC1 puts `Sprite_Current` on an effect
record half the time and seeds the first four sprite records alike, because
the round-twelve disturbance moves it onto sprites; FC2 leaves it on the four
sprite records altogether (its records are sprite-sized and it seeds all
four). **EKH chose FC1's record and neither's sprite**: an effect state runs
with `Sprite_Current` an `Effect_Objects` record (`Effect_RunObjects`,
`symbols.toml` evidence), so the record is always one and the disturbance never
leaves the pool. FC1's and FC2's own fuzz files are not rewritten onto the
shape (8.9).

**The spans are the table's own length, not the run of code pointers**: the
tables sit back to back - `0x65E068`'s run is eleven code pointers, but
`0x5011A0` dispatches through `0x65E080`, its entry 6, so `0x500930`'s table
has six. Measure each table to the next start any dispatcher indexes (the
`disp` column of `shapes.tsv` lists them), or to the first dword that is not
code (`0x65DAE8` has four, then bytes).

**The dispatchers** (`jmp [T + byte * 4]` with `T` in `.data`): 128 index by
`+1` (the kinds' `Effect_KindHandlers` entries, a state table each), 87 by
`+2` (kind 0x18's `EffectKind18_States` entries - that table is indexed by
`+1`, each entry a sub-kind's dispatcher by `+2` - and some kinds' own), 14 by
`+3`, 1 by `+4`, 4 by a byte not of the record; 18 more call through a table
(11 by `+1`). **A kind's dispatcher is taken whole with its table**: a
`kEffect` clone with its table a `DataTable` and `state_span` its length (the
harness swaps the entries for recorders on both sides, 8.8's `0x46F2B0`); a
sub-state dispatcher the same with `sub_span`.

**The call shapes beyond the state handler** (every row read for its stack
arguments, register inputs, answer and calls):

| Shape in the cut | Rows | How to fuzz it | New? |
|---|--:|---|---|
| a state handler, a kind's dispatcher, a kind-0x18 sub-state (void, no arguments, on `Sprite_Current`) | 1,454 take no stack word | `kEffect`, spans per clone | `kEffect` |
| a cdecl helper with stack words | 252 (1: 111, 2: 60, 3: 33, 4: 28, 5: 6, 6: 6, 8: 6, 10: 1, 12: 1) | `kCall`; `kArgs` is 12 now | twelve words |
| ... handed a record on the stack | about 34 by a scan of the callers (they push `Sprite_Current` or a register loaded from it) | `kCall` with `ArgAt(i, Arg::kEffect)` - or `Arg::kScratch` / `args` when the record is another pool's (8.8's `0x4857C0` is handed a shard record) | `Arg::kEffect` |
| a helper answering in `al` / `eax` that a caller reads | 59 | `ret_mask` (`0xFF`, or `0xFFFFFFFF` for `0x479970`'s angle) | no |
| a spawner: calls `Effect_FindFree` and fills the record it answers | 19 (23 sites, 8 groups) | `kEffect`; the effect-mode `Effect_FindFree` answers a free record (8.5) | louder stand-in |
| **the record in a register** | **none**: 85 rows looked like it and were MSVC's `push ecx` for `sub esp, 4`, two more `or al, 0xFF` / `sbb edx, edx` (read by hand) | - | no shape needed |

Three facts for the groups' seeds: some kinds keep **pointers in the record**
(`0x46F530` reads dwords `+0xC..+0x1C` as pointers and their byte `+6`: seed
them at records, or the original faults); some states **loop over a count**
(`0x482650` writes `ObjTrio` + 0x14C * i for i below `Field_MemberCount`: seed
it below 4); and **`EffectKind18_States`' count 160 is an upper bound**, so
`band_rows.py` names cells past its end `EffectKind18_States[n]` - `0x6542AC`,
"`[144]`", is entry 1 of kind 0x23's table `0x6542A8`.

### 8.4 The regions and the seeds

EKH read every absolute address the effect functions name (a memory operand
with no base register, indexed or not, and every immediate in the data range;
`cells.py`) and counted the groups. **Standard in effect mode** - three or more
groups:

| Region | Groups | What |
|---|--:|---|
| `0x9037A0` + `0x40` | 15 (all write) | `Prim_VertexScratch`, the quads' four vertices, and on to `0x9037E0` |
| `0x903800` + `0x40` | 9 | `Camera_ShiftX` / `Y`, `MapView_ScreenXY` `0x903820` (two floats) |
| `0x905E20` + `0x40` | 13 (9 write) | `Cond_ByteFE` .. `Camera_Matrix` `0x905E40` |
| `0x92BF80` + `0x644` | 15 | `EffectKind30_Shards` and the sparks after them, FC2's region (`0x485810` hands out its 0x18-byte records) |
| `0x903584` + `0x10` | 6 (read) | `Field_MenuButton` and the two after it |
| `0x803580` + `0xE8` | 4 | `MessagePools` (FE1: empty at start-up, so a wrong id cannot show without it) |
| `0x66C7E8` + 4 | 6 | `Game_Mode`, `Game_Step` |
| `0x939A00` + `0x30` | 3 | cells E1A, E1E and E3D read |
| `0x7DEE20` + `0x60` | 3 | round twelve's `kMessageCells`, which hold `MsgBoxState` `0x7DEE40..` |

Already standard and read by the effect code: the effect records themselves
(`0x7E11E0`, 20 x 0x80), `Sprite_Current`, `Frame_Counter`, `ObjTrio` (the
leader's record: `0x500D20` reads its point), `Field_Request` (20 groups),
`Camera_Distance` and the counters `0x903840..`, the chapter bytes, the flag
rows; field mode's packet cursor (`Gfx_CommitPrim`, 327 sites, is the most
called callee), the camera-turn cells, the menu block, the area block.

**A group lists it** (one or two groups): `0x800000` (six groups, indexed: a
table whose extent is not measured - a group reads it and lists it),
`DrawItems` `0x905E80` (three, indexed: `MapView_ItemHalfAt`'s half records;
the effect-mode stand-in answers into the harness's own buffer), `0x6BC644..`,
`0x6BC704..` (E1E, E6D), `0x6BE08C` and `0x93985C` (E1G), `0x9036E0`
`Sprite_DrawList` (E6C), `0x9039F4` (E3D, E5B), `DrawLayers` `0x802594` /
`0x802B34`, `0x80BCC0..0x80C184` (E5D, E5E, E6D), the `0x92C208..0x931174` run
of single cells (E1C..E4F, mostly E2E), the enemy records `0x93B9F2..` (E5C,
E6D), and the image's `.data` tables each group reads (its state tables,
swapped as `DataTable`s; `0x5C41B8..0x5C4254`, fifteen groups' float
constants, read-only and left in place like every table of the image).

### 8.5 The standard callees

**The frontier**: every call and tail `jmp` out of the effect functions that
lands outside the cut - **206 callees, 3,897 sites**. 29 are in `kStandard`, 60
in `kField`; of the 117 new, 109 are `kEffectStd` and 8 are second entries or
shared tails inside a group's own functions (`0x4FDCC0` in E5A's `0x4FDC80`,
`0x488B90` E4A, `0x473F10` E2B, `0x47E120` E2G, `0x48A480` and `0x48A560` E4B,
`0x506AB0` E5E, `0x485C60` E3C): the group lists each as a `kPhase` callee or
takes it. The most called: `Gfx_CommitPrim` 327, `Math_Sin` 269, `Math_Cos`
253, `Sound_PlayEffect` 232, `Gpu_SetSemiTrans` 202, `Gpu_SetDrawMode` 189,
`MapView_LinkPrimAt` 172, `Rand` 146, `AreaMap_Elevation` 142,
`Effect_Release` 135, `Gpu_GetTPage` 131, `_ftol` 106 (`kThrough`), `0x52CFE0`
100 (a cut row).

**`kEffectStd`** (109): the 64 ours typed from `symbols.toml` as FH typed
`kField` (the draw primitives' `prim` hashed, `Gte_*` outs not logged and
filled - `Gte_StoreDepthF` / `F3` as floats, `Gte_RotMatrixX` / `Y` / `Z` fill
and answer their matrix -, `FieldPanel_*` FE1's panels, `Member_SetState2_8`
writing what the real one writes, `Flags_Toggle` toggling, `Gpu_LinkPrim`
linking, `Effect_ReleaseAt` clearing, `MapView_ItemAt` answering 0 or a small
item, `MapView_ItemHalfAt` 0 or the harness's buffer, `WorldMap_RecordIndex`
0..11); and 45 of Capcom's unnamed ones, each read to its last instruction
(`typeraw.py`): the stack words and their widths, what is dereferenced (hashed
to the furthest byte read) and written through (not logged, filled to the
furthest byte), and whether a caller reads `al` (`kFlag`) or `eax`.

**`kEffectOverrides`** (10), louder where the caller reads back, registered
first in effect mode so they stand over `kStandard` / `kField`:

| Callee | Louder how |
|---|---|
| `Effect_FindFree` | a quarter of the time none (`0xFF`); else a **free** record (`+0` 0) looked for from a start the answer picks - not always the first, so a caller that finds its own record shows -, `0xFF` when none is free |
| `Effect_Release` | clears bytes 0..4 of `Sprite_Current`, as the real one: a state that reads `+1` again after it sees 0 |
| `0x52CFE0` (E1F, 100 sites) | read to its last instruction: `(id byte, slot, s16 x, s16 y)`, a sprite primitive of 0x1C from the 16-byte records at `0x660438` committed at the cursor, `eax` the primitive. Masks by width; the cursor moves 0x1C over noise; answers the primitive |
| `0x52CF60` (E1F, 31) | `(id byte, slot)`: a draw mode of 0xC from `0x660394`'s records, committed. The cursor moves 0xC |
| `0x469750` (E1B, 33) | `(x, y, w, h, colour)`, whole ints: a frame `0x469790(x, y, w, h)` and a fill `0x469960(x + 2, y + 2, w - 5, h - 5, colour)`. The cursor moves 0x80, **a size of the stand-in's own** (the originals' sum depends on the path) |
| `0x468AC0` (E1B, 19) | `(x, y, bits byte)`: three boxes `0x468BB0(x + 0x30 i, y, bit i)` and three `Text_DrawAt` lines by the byte. The cursor moves 0xC0, its own size |
| `0x503FA0` (E5D, 15) | `(variant)`, a whole word added to a table address: nothing when `Draw_PassFlags` has bit 2; else sixteen textured quads around `Sprite_Current`'s point (`AreaMap_Elevation`, `Gte_RotTransPers4`, `MapView_LinkPrimAt`). Fills `Prim_VertexScratch`' four vertices and `MapView_ScreenXY`, moves the cursor 16 x 0x54 |
| `Sprite_FindFree`, `Party_MemberAt`, `Gte_RotTransPers` | round twelve's behaviour folds (8.6) |
| `EffectGte_ProjectPoint`, `EffectGte_ProjectSize`, `Gte_VectorNormal` | wave two's fold (2026-10-03): no pointer logged by value - the callers hand locals, whose addresses differ between the copy and ours. The point hashed (12 bytes); the size's first word only (several callers leave the second as stack the original never wrote); the outs filled - three floats with fractions, two s16 small half the time, three longs |
| `Math_Cos` | never 0 or -1: E2B's spiral divides by it |
| `Sprite_UpdateScreen` | logs `Sprite_Current` and its 0x80 bytes: a draw on the wrong record shows |
| `MapView_LinkPrimAt` | moves the packet cursor by `size & 0xFF` two times in three, as the real one does for a row on the map; nothing filled |

Wave two's fold also changed three `kEffectStd` rows in place: `EffectSpark_FindFree`
(`0x47CF20`, 8 records of 0x1C at `EffectKind30_Shards`) and `0x47A130` (64 of
0x20 at `0x92D1DC`) answer the first record whose `+0` is 0, or null - null also
a quarter of the time, and always when the pool is outside the group's regions;
`0x4941B0` hashes 8 bytes at each of its three pointers and writes nothing (E2C's
reading; EKH's row wrote them). The square root `0x5A7A90` stays garbage: what
its callers need differs by caller (E2E's clamp), so a group re-lists it.

**The five cross-group rows are owned by their groups** (E1F, E1B, E5D) and not
taken here; the others call them raw (`SH_AT`) until they merge, and the rows
above serve those calls. **EGT's four** (`0x494060`, `0x494110`, `0x494180`,
`0x4941E0`) are EGT's: nothing here. `kField` holds three of them **by address**
(`"0x494060"`, `"0x494110"`, `"0x4941E0"`, FH's): a clone's `E8` to them still
reaches those rows once EGT merges, but ours calling them **by name** will not
(the key is the name's, not the address) - when EGT merges, those three rows
want `FIELD_OURS(name)` and `0x494180` a row, or every effect group lists them.

### 8.6 Round twelve's debts folded here

Round twelve's section 7 items 1 and 6, where they touch this harness:

- **The masks, in place** (38 rows of `kStandard` / `kField`), each the width
  the callee reads as a wave-two group found it: `Inventory_Add` / `Count`,
  `Party_Count`, `Skill_FlagIndex`, `Item_HelpMessage`, `AbilityList_Add`,
  `Field_MemberSprite`, `Area_LinkAt`, `0x594700` bytes; `Msg_SystemPtr`,
  `MapView_SetElevation`, `Area_TestCondition` words; `Sprite_EnsureAnimation`
  its byte (FE2, FC3: `cmp [+0x4B], al`); `AreaMap_SetByte`,
  `AreaMap_SetHeight` s16, s16, byte; `Menu_DrawBox`, `Menu_DrawCursorBox`
  four words and two bytes; `Menu_DrawMemberStatus`, `Menu_DrawMoneyBox`,
  `Menu_DrawSkillRow`, `Menu_DrawHand`, `Text_DrawFont8` / `12` (six bits of
  the colour), `Char_LoseHp`, `0x537500`, `Field_WayBlocked`,
  `MapView_SlopeAt`, `Gfx_CommitPrim` (two bytes); `Crt_sprintf` at three
  words; the stack pointers of `Gte_RotMatrix`, `Gte_RotTrans`,
  `Gte_MulMatrix0`, `Gte_RotTransPers`, `Gte_RotTransPers3` / `4`,
  `MoveCmd_AttachOffset` and `MoveCmd_Move`'s object no longer logged by value.
  **A mask never changes a draw or a count** (the recorders' stream is the log's
  length, not its contents), and a narrower one compares less, so no existing
  group can start to mismatch; 8.8 shows every count unchanged.
- **The behaviour, beside the rows** (effect mode only, `kEffectOverrides`):
  `Sprite_FindFree` answering 0..29 or `0xFF`, `Party_MemberAt` 0..2 or `0xFF`
  (FC1), `Gte_RotTransPers` filling its screen point as two floats (FE2). In
  place they would change what the wave-two groups that use the standard rows
  draw.
- **Folded 2026-10-01, in place** (the coordinator's pass, a cloud session;
  the i686 build verified it, the `'*'` run at the tip is the owner's):
  `Zenny_Add`'s tally test the right way round (`FxZennyAdd` adds to
  `0x904138` when the byte is 0, as `scena_sx.cpp` does; FE1);
  `Party_Count` answering 0..3 (FS: a random count ran the callers' loops
  past `ObjTrio`); `Menu_ListScroll` writing its out-bytes (`FxListScroll`,
  FS's form, the writes guarded); `AreaMap_Slope` and `MapView_SlopeAt`
  setting the "sloped" byte `0x903850` (`FxSloped`, FC1, FC3; inside the
  effect-slot region already); `MoveScript_Step` answering `0xFF..0x0F` and
  moving the context's flag byte (`FxStepFlag`, FC3); `Field_CellAhead`
  0..4 (FC3); `Item_IconKind` 2..7 (FO); `Gte_SetRotMatrix` hashing its
  nine shorts (18 bytes) and `Gte_SetTransMatrix` noting its translation
  `+0x14..+0x1F` instead of hashing the rotation it never reads
  (`FxSetTrans`, FC2 and FE2 agree); the SVECTOR arguments of
  `Gte_RotMatrix`, `_RotTrans`, `_RotTransPers` / `3` / `4` hashed as six
  bytes, not eight (the fourth short is the pad DIV-0023 found stale);
  `Gte_MulMatrix0`'s two matrices as 18 bytes, not 8;
  `Equip_PreviewSet` filling and noting the caller's marks and values
  (`FxPreviewSet`, FO: the row hashed the two buffers before anything had
  filled them). **The masks the first pass left**, now at what the callee
  reads: `Menu_DrawPiece`, `_DrawPieces`, `_DrawIcon8`, `_DrawScrollBar`,
  `_DrawCell8`, `_DrawItemRow`, `_DrawTitleBox`, `_DrawItemIcon`,
  `_DrawBorder`, `Text_DrawAt` (`kStandard`), `Text_DrawSmall`,
  `TextRecord_Set`, `Char_ExpForLevel`, `Char_AbilityList`, `Skill_CanUse`,
  `Skill_ApCost`, `Item_CanUse`, `KeyItem_Has`, `PartySet_Load`
  (`kStandard`), `0x46D5F0`, `UiSprite_Draw` (FO, FS, FE1, FC2). **Two
  regions for the field runs** (not effect mode, whose `kMenuButtons` and
  `kMessagePools` hold them already): `Field_ConfirmButtons` /
  `_CancelButtons` `0x90358C` (8) and `MessagePools`' first 0x200 offset
  words `0x803580` (0x400; FE1: empty at start-up, so a script-pool message
  drawn by id could not show). **The regions move every field group's
  draw** (the random fill is one stream over the regions), so FC1..FS's
  counts change at the next `'*'`; 0 mismatches is the bar, as for every
  cleanup. An effect's writes draw from `Noise()`, not the stream, so the
  louder forms move no count; a new mismatch under one is a finding.
- **Still not folded**: `Port_DroppedCall` at four words (FS's `0x581300`
  pushes four; every other caller one - a per-site count the row cannot
  hold, so FS's own listing stands) and the groups' reach effects
  (`AreaMap_ByteAt` / `_Elevation` answering what the callers compare,
  `MapView_GroundAt` at the height's boundaries, `Char_ExpForLevel`'s -1).

### 8.7 Worked examples

**A hidden state handler reached by a `.data` cell** - E1A's `0x462BC0`
(hidden in `0x462AC0`; the cell `0x653A44` is entry 0 of kind 1's state table,
whose dispatcher `0x462BA0` is `Effect_KindHandlers[1]`, the table two entries
to `0x653A4C`, kind 7's). It copies the dword `+0x54` of the
`Sprite_ObjectsExtra` record `+0x18` names into the effect record's `+0x54` and
sets `+1` to 1. Ours reads the index from the record the harness made current:

```cpp
// src/game/effect_1a.cpp
extern "C" void __cdecl Effect1A_Kind01Start() {   // 0x462BC0
    unsigned char* const s = static_cast<unsigned char*>(Sprite_Current);
    const std::uint32_t k = static_cast<std::uint32_t>(move_script::Long(s + 0x18));
    if (k >= 4) bof3::Fatal("Effect1A_Kind01Start: +0x18 is %u, past the four extra records", (unsigned)k);
    move_script::SetLong(s + 0x54, move_script::Long(scenario_harness::Mem(0x802054 + 0xA4 * k)));
    s[1] = 1;
}
// src/game/effect_1a_fuzz.cpp
namespace sh = scenario_harness;
const sh::Clone kClones[] = {
    {"Effect1A_Kind01Start", 0x462BC0, 0x23, nullptr, 0, nullptr, 0, nullptr, 0,
     reinterpret_cast<const void*>(&::Effect1A_Kind01Start), 0, false, sh::Shape::kEffect,
     0, 2, 0, 1},   // pointers, state_span 2 (0x653A44's two), sub_span, kind 1
};
void Seed(unsigned) {
    unsigned char* const s = static_cast<unsigned char*>(Sprite_Current);
    s[0x18] = static_cast<unsigned char>(sh::Next() % 4);   // the original reads 0x802054 + 0xA4 * it
    s[0x19] = s[0x1A] = s[0x1B] = 0;
}
void Run() {
    sh::Group g = {"effect_1a", kClones, 1, nullptr, 0, nullptr, 0, nullptr, 0, Seed, nullptr, 0};
    g.effect = true;
    sh::Run(g);
}
```

(A real index past the four is Capcom's read of whatever follows; ours aborts
there, the round-nine rule - the seed keeps it inside.)

**A kind-0x18 sub-state** - E5B's `0x500D20`, entry 3 of `0x65E068`, the
table of `0x500930` = `EffectKind18_States[15]` (so the record's `+5` is 0x18,
`+1` 15, `+2` 3; the table has six entries before `0x5011A0`'s at `0x65E080`).
Unless `Cond_ByteFD` is 0 and `Cond_ByteFE` equals `+0xA`, it compares the
leader's point (`ObjTrio +0x34` / `+0x38`) with the record's cell (`+0x36` /
`+0x3A`, the high words; `+8` chooses the axis first) and, within two cells on
both, sets `+2` to 4. No callee, no table:

```cpp
const sh::Clone kClones[] = {
    {"Effect5B_Kind18_15Near", 0x500D20, 0xD1, nullptr, 0, nullptr, 0, nullptr, 0,
     reinterpret_cast<const void*>(&::Effect5B_Kind18_15Near), 0, false, sh::Shape::kEffect,
     0, 16, 6, 0x18},   // +1 below 16 (EffectKind18_States' entries this group runs), +2 below 6
};
void Seed(unsigned) {
    unsigned char* const s = static_cast<unsigned char*>(Sprite_Current);
    s[1] = 15;
    if (sh::Half()) {   // the leader within, at and past two cells of the record
        const std::int32_t dx = static_cast<std::int32_t>(sh::Next() % 5) - 2;
        const std::int32_t dz = static_cast<std::int32_t>(sh::Next() % 5) - 2;
        move_script::SetLong(ObjTrio + 0x34, static_cast<std::int32_t>(((move_script::Word(s + 0x36) + dx) << 16) | (sh::Next() & 0xFFFF)));
        move_script::SetLong(ObjTrio + 0x38, static_cast<std::int32_t>(((move_script::Word(s + 0x3A) + dz) << 16) | (sh::Next() & 0xFFFF)));
    }
    if (sh::Next() % 3 == 0) sh::Mem(sh::at::kByteFD)[0] = 0;
    if (sh::Next() % 3 == 0) sh::Mem(sh::at::kCameraCells)[0] = s[0xA];   // Cond_ByteFE
}
void Run() {
    static const std::uint8_t kKinds[] = {0x18};
    sh::Group g = {"effect_5b", kClones, 1, nullptr, 0, nullptr, 0, nullptr, 0, Seed, nullptr, 0};
    g.effect = true;
    g.kinds = kKinds;
    g.n_kinds = 1;
    sh::Run(g);
}
```

The names are placeholders (a group names its functions); the extents, spans
and seeds are the ones `scenario_harness_ekh.cpp` runs (its seed writes the
leader's words byte by byte; the same values).

### 8.8 The proof

**(a) Nothing moved that uses this harness.** `BOF3X_SHADOW='*'` headless on
`d19d803` built in this worktree (`ekh/star_base.log`) and on EKH's build with
the effect mode and the mask fold (`ekh/star_after.log`): both exit 0,
`inject: 6891 ours, 0 left original`, **677 self-test lines before and 678
after**, the one more `scenario_harness_ekh`'s. Of 1,305 self-test, coverage
and mode lines, **1,276 are byte-identical**, among them every line of every
shadow that runs through `scenario_harness` - the 17 scenario shadows, the
seven field groups, `scenario_harness_fh`. What moved, all still 0
mismatches, only call counts: `sound` (117,994 -> 117,997), `magic_s16`,
`magic_s17`, `magic_s34`, `magic_s35`, `area_w0b`, `area_w1b`, `area_w1e`,
`area_w2b` (both), `area_w3a` and `battle_e7` (3,249,681 -> 3,249,684) - the
same modules as FH's 7.8 (plus `battle_e7`, which did not exist then), none of
which uses this harness, whose harnesses (`magic_harness`, `area_harness`,
`boss_harness`) and sources are untouched: the build-directory trap
([`HANDOFF.md`](HANDOFF.md)), their records' pointers into our DLL moving as
the DLL grew (256 more recorders, twelve arguments, the effect tables). Two
`field_s` lines changed their text only: the "lies outside" line now says
"and the effect runs", and one names our own `PartyForm_DrawReserve` at its
DLL address, which moved with the build.

**(b) The new shape runs.** `BOF3X_SHADOW=scenario_harness_ekh`
(`src/game/scenario_harness_ekh.cpp`, after every scenario and field group in
`inject_all.cpp`; every effect group's inject goes after it, since it copies
eight of their rows): eight rows of the cut, each a copy of the original
against Capcom's on the other side, so any difference is the harness's -

| Shape | Row (group) | Through |
|---|---|---|
| `kEffect`, a hidden state reached by a cell | `0x462BC0` (E1A) | kind 1, `state_span` 2; `+0x18` seeded below 4 |
| `kEffect`, a kind's dispatcher with its table | `0x46F2B0` (E1D) | `Effect_KindHandlers[0x21]`, `0x654284` (6), `state_span` 6 |
| `kEffect`, a kind-0x18 sub-state dispatcher | `0x4FD470` (E5A) | `EffectKind18_States[4]`, `0x65DAE8` (4), `sub_span` 4 |
| `kEffect`, a kind-0x18 sub-state | `0x500D20` (E5B) | 8.7's seeds |
| `kCall` answering `eax` | `0x479970` (E2E) | `ret_mask` `0xFFFFFFFF`, the angles seeded across `0x800` |
| `kCall` handed a record | `0x4857C0` (E3C) | `ArgAt(0, Arg::kEffect)` |
| `kEffect`, a spawner | `0x46F7D0` (E1D) | kind 0x23; `Effect_FindFree`'s effect-mode stand-in |
| `kEffect`, a release | `0x472770` (E2A) | `Effect_Release`'s effect-mode stand-in, by a tail `jmp` |

The first six call nothing (or only through their table, swapped on both
sides), so "ours" is the original in place. The spawner and the release call a
callee the original in place would reach for real, so their "ours" is a second
byte-copy of the original with that call re-aimed at a trampoline that asks
`StandIn` for the callee's stand-in - as ours does through `SH_CALL`; Capcom's
code on both sides still. Alone: **16,000 rounds over 8 functions, 6,973 calls
to the stand-ins, 0 mismatches**; 24,756 bytes of state in 45 regions; 365
stand-ins registered - the 174 field-standard, 109 effect-standard and 10
re-listings among them, every one passing `Register`'s checks. Every entry of
both tables was reached (`0x654284`'s six 312..353 times each, `0x65DAE8`'s
four about 500 each); `Effect_FindFree` 973 calls, `Effect_Release` 2,000.
Under `'*'` it runs last of this harness's users and draws another stream
(6,986 calls), 0 mismatches there too.

**(c) Controls** (`BOF3X_EKH_CONTROL`, in the committed file, off by default):
`1` stands in for `0x462BC0` a copy whose `mov byte [eax + 1], 1` writes 2 -
**refused in 2,000 of 2,000 rounds**, exit 3; `2` makes the spawner's copy
write the new record's kind 0x25 instead of 0x24 - **refused in 731 rounds**
(the rounds where the stand-in answered a free record and the spawn ran),
exit 3. **2 of 2 refused by a count.**

`tools/ledger_check.py`: 63 entries, 0 errors. No function is taken: no
`BOF3_INJECT`, no `impl`, no ledger entry needed (the harness changes what the
fuzz does, not what the game does).

### 8.9 Limits and debts

- **FC1's and FC2's fuzz files are not on the new shape.** FC1's
  `Sprite_Current = Eff(...)` and `sprite_span` 20, FC2's sprite-record seeds:
  both still run round twelve's field mode unchanged. Moving them onto
  `kEffect` changes what they draw; it is a debt for the round's cleanup, with a
  re-run of their controls.
- **The stand-ins' effects are proved by the self-test for `Effect_FindFree`
  and `Effect_Release` only**; the other rows by registration (every one passes
  `Register`'s checks at start-up in `scenario_harness_ekh`, 365 slots) and
  their reading. The first effect groups to call them will re-list some, as
  every wave-two group did.
- **Weaker than wanted**: the unnamed callees handed a large record
  (`0x4794D0` read to `+0x440`, `0x479B70` / `0x4799C0` to `+0xD20`) hash its
  first 16 bytes and fill nothing; `0x469750` and `0x468AC0` move the cursor by
  a size of their own; `0x503FA0`'s variant word is logged whole (it is added
  to a table address, so all of it matters).
- **Spans are per clone**: a function that indexes by `+3` or `+4` (15 rows)
  seeds the byte itself.
- **Records holding pointers** (kind 0x22's `+0xC..+0x1C`), **count loops**
  over pools and **indexes into the image's tables** by a record byte are the
  groups' seeds (8.3); a random record faults the original there.
- **`0x593950`** is the dispatcher of EKP's run `0x66A470` (by the byte
  `0x93985C`), which round thirteen's plan left open (its section 4): E1F
  reaches it by a tail `jmp`. It is not in the cut; `kEffectStd` lists it.
- **`0x5124C0`** (E6C) is a case of `0x512490`'s own switch, as the plan's
  section 4 said: not a function.
- Everything in sections 5, 6 and 7.9 still holds.

### 8.10 The round's end fold

2026-10-03, on the round branch's tip `8d3d064` (8,648 ours), the items
[`takeover-queue-round13.md`](takeover-queue-round13.md) sections 16 and 18
left this harness. Each row was read against the code (ours, and Capcom's
with `tools/pe_disasm.py`) before it changed.

| Row | Was | Now | What the code shows |
|---|---|---|---|
| `MapView_LinkPrimAt`, `kEffectOverrides` **and** `kField` | `{kAll, kAll, kAll, kAll}` | `{kAll, kAll, kU8, kU8}` | `0x572FA0`: x and z whole (each low word compared with 0, each high word summed), dy `movsx edx, byte [esp+0x14]` (`0x572FD6`), size `and edi, 0xFF` (`0x572FF6`); ours (`world_map.cpp`) reads the same. The `kField` row had the same whole-word dy; a mask only narrows, so it was folded with it |
| `0x4FEE70` (`kEffectStd`, Capcom's) | `kFlag` | `kByte` 1..8 with `FxPattern` (the rest of `eax` 0; a quarter of the time `Sprite_Current`'s `+2` when 1..8) | `lea eax, [edi + 1]` (`0x4FEE9F`): the three story flags as bits, plus 1, a whole `eax`; its caller `EffectKind18_09_Pattern` compares all of it (`cmp ecx, eax`, `0x4FEDE3`) and indexes two tables by its byte. `kFlag`'s garbage above `al` never compared equal - E5A's own `FxPattern`, folded |
| `EffectKind18Sub42_Draw` `0x509A70` (`kEffectStd`) | `{0, kAll}`, `deref {4}` (a pointer) | `{kAll, kU16}`, no `deref` | E5F's `(variant, height)`: the variant a whole word indexing the piece lists and lift words (`lea eax, [ebp + ebp*2]`, `0x509A83`; `[ebp*2 + 0x65E940]`), the height's low word alone reaching the vertex word it is subtracted into (`sub edx, ebp` / `mov [eax - 6], dx`, `0x509B62`) |

**`scenario_harness_ekh.cpp`'s clone sources**: all eight rows and both
callees are ours now, so each address literal became its symbol's constant,
the value unchanged - `bof3::addr::EffectKind01_Start` (`0x462BC0`),
`EffectKind21_Run` (`0x46F2B0`), `EffectKind18_04_Run` (`0x4FD470`),
`EffectKind18Sub0F_Open` (`0x500D20`), `EffectAngle_Mean` (`0x479970`),
`EffectKind73_SparkInit` (`0x4857C0`), `EffectKind23_SpawnRays`
(`0x46F7D0`), `EffectKind2D_End` (`0x472770`), and the copies' call targets
`Effect_FindFree` (`0x589810`) and `Effect_Release` (`0x589840`). Never
`&::Name`: the copies are Capcom's bytes, and the six "in place" are the
originals only because the self-test runs before every effect group's
inject - `inject_all.cpp` still has `ScenarioHarnessEkh_Inject` above
`Effect1F_Inject` and every `Effect*_Inject` (checked; its comment now says
so). `SpritePose_Inject` (line 346) detours the two callees before it, which
changes nothing: the copies' calls are re-aimed at the trampolines. The two
`DataTable`s stay literals (`0x654284`, `0x65DAE8`): data names are macros
casting the address, so `bof3::addr::` cannot spell them. The clones' log
names now carry the symbol. `scenario_harness.h`'s `kArgs` comment names
`EffectKind7D_FillF4`.

**`FX_RAW` rows**: no `kEffectStd` row's address has an `impl` in
`symbols.toml` (a script over all 42 at `8d3d064`) - `fold_names.py` had
taken the rest. The 42 that remain are Capcom's, none named:
`0x462F10`, `0x46E190`, `0x46F570`, `0x46F690`, `0x46FAE0`, `0x4790C0`,
`0x4790F0`, `0x479160`, `0x479260`, `0x4794D0`, `0x4796B0`, `0x4799C0`,
`0x479B70`, `0x479EE0`, `0x47A110`, `0x47A130`, `0x47A150`, `0x47A200`,
`0x4837B0`, `0x48ED80`, `0x491E30`, `0x4920F0`, `0x492260`, `0x4941B0`,
`0x4FEE70`, `0x5100B0`, `0x5101C0`, `0x5171E0`, `0x52B1B0`, `0x52B200`,
`0x52B2A0`, `0x52B2E0`, `0x52B330`, `0x52B370`, `0x52B6C0`, `0x586160`,
`0x593950`, `0x59E930`, `0x5A7570`, `0x5A7840`, `0x5A7A90`, `0x5A7C70`.
(`kField`'s eleven rows keyed by address, `0x58BD50` .. `0x591AC0`, are
Capcom's too.)

**The proof** (this worktree's i686 build): `BOF3X_SHADOW='*'` headless,
narrow and with `BOF3X_WIDE=1`: both exit 0, `inject: 8648 ours`, 716
self-test lines, **no non-zero MISMATCHES line**. No merged group changed its
verdict: the groups that call a changed row without re-listing it (E4F and
E4B among those that call `MapView_LinkPrimAt` through a standard row) stay
at 0; `0x4FEE70` and `0x509A70` are re-listed by their only callers
(E5A, E5F), so the standard rows serve groups to come.

**Controls** (each planted, built, its shadow run alone, restored and
rebuilt; scratch `fold/control.py`):

| Control | Shadow | Result |
|---|---|---|
| E4F's `EffectKindAB_DrawDrop` links with dy 1 instead of 0 | `effect_4f` | **refused**, 4,000 rounds (the log's entry, not a count) |
| ... with dy `0x100` (only bits above the byte) | `effect_4f` | 0 mismatches - equivalent, as the real one reads the byte; the old `kAll` row would have refused it |
| E5A's re-listing of `0x4FEE70` dropped (the standard row serving) | `effect_5a` | 0 mismatches |
| ... and `EffectKind18_09_Pattern` always taking the "changed" path | `effect_5a` | **refused**, 1,065 rounds (the rounds where the row answered the record's `+2`) |
| ... the same mutant against the old `kFlag` row | `effect_5a` | ours aborts (`+2` stored as a garbage byte, index 180 past `0x65DE62`'s nine): the old row could not serve this caller at all |
| E5F's re-listing of `EffectKind18Sub42_Draw` dropped | `effect_5f` | 0 mismatches |
| ... and `Draw42` passing the height + 1 | `effect_5f` | **refused**, 18,932 rounds over the six states |
| ... and the height `^ 0x10000` (above the low word) | `effect_5f` | 0 mismatches - equivalent, as the real one stores the low word |
| `EffectKindAF_DrawScreen`'s width `Widescreen_FillWidth() + 1` | `effect_4f` | **refused**, 4,000 rounds |
| `EffectKind89_DrawTint`'s width `Widescreen_FillWidth() + 1` | `effect_4b` | **refused**, 4,000 rounds |

The last two are the widened fills of DIV-0041's round's-end amendment
(E4B's `0x489D47`, E4F's `0x493308`; [`widescreen.md`](widescreen.md)
section 5): unarmed during every self-test, so the fuzz compares the
original's 320 x 240 and a wrong width is refused. `tools/ledger_check.py`:
72 entries, 0 errors.

### 8.11 Round fourteen's fold

2026-10-05, on `phase-3/round14-end` at `e3b98087` (10,009 ours), the
harness items of [`takeover-queue-round14.md`](takeover-queue-round14.md)
sections 9 to 13 (debts 2, 14 and 21). (Numbered 8.11, not 8.7 as the brief
had it: 8.7 to 8.10 were taken.) Each row was read against the code - ours,
and Capcom's with `tools/pe_disasm.py` - before it changed.

**Rows keyed by an address that is ours now, to the `_OURS` form** (the key
our function, the address the row's `address`; masks, answers, `deref` and
effects unchanged unless the next table says otherwise). A raw row served
only callers that call the address (a clone's re-aimed `E8`, ours' `SH_AT`);
ours calling the function by name found nothing and every group re-listed
it. In the `_OURS` form `StandIn` resolves both: by the key first, by the
address through the table (its second pass). The address stays in each
comment.

| Table | Rows | Whose |
|---|---|---|
| `kStandard` | `Field_StartEventBattle`, `Party_PlaceForBattle`, `Camera_EaseAngleFB`, `MapView_FillCells`, `Field_SetStatus80` (keyed by `bof3::addr::`, the address, since round ten; ours since then) | SE, SX |
| `kField` | `BattleExtra_Dispatch` `0x42D710` | R3A |
| `kField` | `EffectKind41_DrawNumber` `0x46D5F0` | R3E |
| `kField` | `Mode11_ListedSpriteScreens` `0x5372E0`, `Char_LoseAp` `0x537500` | R2A |
| `kField` | `Shisu_ModeDispatch` `0x57DFF0` | R2B |
| `kField` | `MasterTalk_Reset` `0x585A00`, `MasterTalk_Dispatch` `0x586670` | R2C |
| `kField` | `FieldMenu_SwapBytes` `0x58BD50`, `ItemTrade_TakeNeeds` `0x594D90`, `AbilityList_CountSet` `0x591AC0` | R2E, R2F |
| `kEffectStd` | the seventeen of R3E (`0x46E190`, `0x46F570`, `0x46F690`, `0x46FAE0`, `0x4790C0`, `0x4790F0`, `0x479160`, `0x479260`, `0x4794D0`, `0x4796B0`, `0x4799C0`, `0x479B70`, `0x479EE0`, `0x47A110`, `0x47A130`, `0x47A150`, `0x47A200`) - with `0x46D5F0` above, R3E's eighteen | R3E |
| `kEffectStd` | `0x4837B0`, `0x48ED80`, `0x491E30`, `0x492260`, `0x4920F0` | R3F |
| `kEffectStd` | `0x4941B0`, `0x4FEE70`, `0x5100B0`, `0x5101C0`, `0x5171E0` (R3G's other two are `boss_harness`'s, its 10.11) | R3G |
| `kEffectStd` | `0x52B1B0`, `0x52B200`, `0x52B2A0`, `0x52B2E0`, `0x52B330`, `0x52B370`, `0x52B6C0` (debt 2's seven) | R1G |
| `kEffectStd` | `EffectKind07_DrawSprite` `0x462F10` | R4F |
| `kEffectStd` | `Menu_DrawPanelBox` `0x586160` | R2C |

51 rows. What stays keyed by address is Capcom's: `0x5B9550` (`_ftol`,
`kThrough`), `0x5A7C70`, `0x5A7570`, `0x5A7840`, `0x5A7A90`, `0x59E930`,
`0x593950` (a script over every raw row against `symbols.toml`'s `impl`).

**Masks and answers, at what the function reads:**

| Row | Was | Now | What the code shows |
|---|---|---|---|
| `EffectKindA7_DrawGlow` `0x491E30` | `{kAll, kAll, kU8}` | `{kAll, kU16, kU8}` | the size is the low word: `short sz[2] = {size, size}` (`rest_3f.cpp`), `mov ax, word [esp + 0x60]` at `0x491E81` (R3F) |
| `EffectKind18Sub41_DrawPanels` `0x5100B0` | `{kAll, kAll}` | `{kU16, kU16}` | the lift multiplies a vertex byte into a word (`imul ecx, ebx` at `0x510123`, the product stored as a word), the texture is shifted up 16 (`(texture << 16) \| 0xBB009120`, `rest_3g.cpp`): neither upper half reaches anything (R3G) |
| `LeaderPanel_LeaveOnPress` `0x52B330` | `{kAll}`, garbage | `{kU16}`, `kByte` 0..1 | `and eax, edx; test ax, ax` (`0x52B33A`); `mov al, 1` (`0x52B351`) or `xor al, al` (`0x52B362`), the rest of eax what it was (R1G; E1E's own row has the same mask) |
| `Sprite_LoadPalette` (`kField`) | `{kAll, kAll}`, `deref {8, 0}` | `{kAll, kAll}`, no `deref`, `FxPalette` | the callee writes 32 words at `dst` and reads nothing there; the 8-byte hash of what `dst` held before the call passed R1A's C74 (a stride 0x20 for 0x40) wherever the palettes were alike. The pointer is logged by value, as R1A's own listing does, and `FxPalette` writes the 0x40 bytes with noise where they lie in the regions, so a group that keeps them compares the destination after the call. In field mode the palettes `0x80D380..` are in no standard region: there the value log is what compares |

**New `kField` rows** (registered for every group, after its own listing and
the handlers, so no group that lists them sees a difference):

- R1C's six field callees no standard row had - `Effect_SpawnAtCellHigh`,
  `Effect_SpawnAtCell` (the state's byte, x and z `movsx` words),
  `Field_GiveZenny` (whole: a byte product pushed whole), `AreaMap_ClearCell`
  (16 bits each), `Field_EffectAhead` (`kByte` `0xFF..0x13`: a record or
  none, all it answers - R1C's `FxEffectAhead` in the harness's own form) and
  `Sprite_TurnSense` (the target's byte, `static_cast<unsigned char>` in
  `inventory_ops.cpp`; R1A's and R1D's mask, where R1C's listing has the
  whole word). Each is listed by R1A..R1F with these masks. R0A's six party
  helpers (`PartyAction_TargetAhead` .. `_SideProbes`) are left to the
  groups that call them: only wave one's party-action groups do, and their
  answers differ by group (R1A's `FxRecord`, R1B's `FxKind30`, R1D's
  `FxMostlyNo`).
- `Gpu_SetLineF3`, `Gpu_SetSprt16` (debt 21's last clause): R4B found them
  in effect mode only. Field runs reach them - `rest_2d.cpp`,
  `rest_4b.cpp` and `rest_4e.cpp` call them in field-mode groups, each
  listing them itself - so `kField` has them now, the same rows as
  `kEffectStd`'s (the primitive hashed, `Gpu_SetLineF3` answering it), which
  they stand over in effect mode with no difference.

**Not folded**: R1C's `Sprite_FlashClut` mask (`kU8`; the `kEffectStd` row
had the word, which only an effect group calling it would see - narrowed to
`kU8` on 2026-10-06, `0x534DBE and ecx, 0xFF`, [`round-14-cleanup.md`](round-14-cleanup.md)
section 7; E5D is the effect group that reaches it); the rows
groups listed in their own `_fuzz.cpp` (left alone: each group's listing is
registered first and stands; `effect_1e_fuzz.cpp`'s `E_RAW` rows key on the
address and keep working).

**The proof** (this worktree's i686 build): `BOF3X_SHADOW='*'` headless on the fold
and, as the base, on `e3b98087`'s two harness files built in the same
worktree: all three runs (the fold narrow and with `BOF3X_WIDE=1`, the base
narrow) exit 0 with `self-test only: done` and `inject: 10009 ours, 0 left
original`, 1,049 `MISMATCHES` lines each, **every one 0**. Of the 517
self-test and coverage lines, base against fold narrow, 474 are
byte-identical. Of the other 43, most differ in the coverage text only: a
stand-in's log name is the symbol now where it was the address (`battle_e1`,
`battle_e5`, `rest_3b`, `rest_3c`, `effect_1a` .. `effect_6b`, `field_c3`,
`field_e2`, `field_s`; the same counts). **Counts moved** - calls to the
stand-ins, all still 0 mismatches - in `rest_2d` (563,368 -> 563,365),
`rest_2f` (1,843,914 -> 1,844,044), `rest_2g` (469,625 -> 470,273), and in
`magic_s16`, `_s17`, `_s34`, `_s35`, `area_w0b`, `area_w1e`, `area_w3a`,
whose harnesses (`magic_harness`, `area_harness`) this fold does not touch.
None of the three `rest_2*` groups reaches a changed row through the
standard set (each lists its own `Gpu_SetLineF3`, `0x586160`, `0x58BD50`;
`rest_2g` calls none of the rows), and their stand-ins answer pointers into
our DLL (`FxText`), so the moves read as the build-directory trap of 8.8 -
the DLL's layout moved with `FxPalette` and the new rows - not as the fold;
not proved further. The rows that change an answer (`0x52B330`'s `kByte`)
or write (`FxPalette`) moved no count: no merged group reaches them through
the standard set (E1E lists `0x52B330`; `Sprite_LoadPalette`'s callers'
palettes lie in no field region). `tools/ledger_check.py`: 75 entries, 0
errors. No function is taken, no ledger entry needed (the harness changes
what the fuzz compares, not what the game does).

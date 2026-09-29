# The scenario harness: one fuzz for every scenario group

**Status:** MEASURED (2026-09-28; every scenario group of round ten ran through it, 0 mismatches each, [`takeover-queue-round10.md`](takeover-queue-round10.md)) - built and proved on chapter 0
([`scena_sc0.md`](scena_sc0.md): 19 functions, 0 mismatches in 76,000
rounds, 105 of 105 controls refused). Round ten group SCH
([`takeover-queue-round10.md`](takeover-queue-round10.md) §1). **Widened for
round twelve's field groups** (2026-09-28, group FH, pin `430f34b`): field
mode, five shapes, the field regions and 174 field-standard callees, section
7 - every scenario shadow and `'*'` unchanged, the self-test
`scenario_harness_fh` 0 mismatches over 13 field functions with Capcom's on
both sides, 6 of 6 controls refused.

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

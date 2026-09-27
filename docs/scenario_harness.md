# The scenario harness: one fuzz for every scenario group

**Status:** IN PROGRESS (2026-09-27) - built and proved on chapter 0
([`scena_sc0.md`](scena_sc0.md): 19 functions, 0 mismatches in 76,000
rounds, 105 of 105 controls refused). Round ten group SCH
([`takeover-queue-round10.md`](takeover-queue-round10.md) §1).

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

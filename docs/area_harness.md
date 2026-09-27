# The area harness: one fuzz for every area group

**Status:** IN PROGRESS (2026-09-27) - built and proved on one area (area 11,
[`area_011.md`](area_011.md): 3 functions, 0 mismatches, @CONTROLS@) and on
the cell hook's reader `0x56E670` (section 8). Not yet used by a group that
needs a choice handler, a step or arrive hook, a mode-tail phase or a state
table of the area's own: the first group to use each shape is its first
test (the spell harness's section 7 said the same of its options).

Group ARH of round ten ([`takeover-queue-round10.md`](takeover-queue-round10.md)
§1). `src/game/area_harness.h` / `.cpp`: a copy of the spell round's
harness ([`magic_harness.md`](magic_harness.md)) adapted to the area
overlays of [`takeover-queue-areas.md`](takeover-queue-areas.md), so that an
area group writes only its functions, a list and its seeds.
`magic_harness.*` is not edited: the area harness has its own recorder pool,
its own `g_active`, its own standard set, and both run in one process
(`BOF3X_SHADOW='*'`).

## 1. What an area function is, to the harness

Every function of an area's block hangs from one of seven root tables
([`takeover-queue-areas.md`](takeover-queue-areas.md) §1.2) and runs in the
field frame: the leader's record and `Field_State`, `Sprite_Current` (the
object running a script, or the leader), the party records, the field
objects, the flags, the area block, the message word, and the area's own
`.data`. So one frame of that state is every input, and one recorder per
callee is every output. What the engine does around the call differs by
root; that is the clone's **shape** (`Clone::shape`, `area_harness::Shape`):

| `Shape` | Root | How the engine calls it | What the harness does |
|---|---|---|---|
| `kHandler` (default) | `Area_Descriptors[area] +0x3C [n]` | movement-script ops `03` / `DE`, `Sprite_Current` the running object; `void (void)` | calls it with three ignored words; logs `Sprite_Current` and `Sprite_Current[8]` after (op `DE`'s answer; the byte only when it lies in the regions) |
| `kChoice` | `+0x34 [id]` | `MsgBox_ChoiceCommit` / `MsgBox_MenuCommit`, id below `0x80`; `void (void)` | logs the message word `0x7DEE48` after (`0xFFFF`: no new message; [`item-use.md`](item-use.md) §5) |
| `kInit` | `+0x40` | `Area_Enter`, once per entry, after map and party placement; `void (void)` | nothing more |
| `kHook` | `Area_StepHook` / `Area_ArriveHook` handlers; `0x662F28`'s handlers through `Area_CellHook` | `(x, z)` in, `al` out | calls it with two cell words (a byte half the time, else any) unless the group's `args` says; compares the answer by `ret_mask`, 0 meaning `0xFF` |
| `kTail` | `0x662CE8` | `Field_ModeTailRun`, `jmp [eax*4 + 0x662CE8]` by the s8 `0x9039F3` | nothing more |
| `kState` | a table in the area's `.data` | the area's own frame function by a state byte | nothing more (the table is a `DataTable`) |
| `kCallee` | none | called directly by the area's own functions | nothing more; arguments by `args`, answer by `ret_mask` |

**A reading the plan had to correct.** The plan (§2) called the cell hook's
entries "a phase, void". `0x56E670` passes them its two arguments (the
cell's x and z, from `Field_LeaderTalkTest` through `0x56D7A0`) and returns
their `al` sign-extended: they are hooks, `kHook`, like the step and arrive
handlers.

## 2. How ours calls out

As the spell harness's, with `AH_` for `MH_`:

| Call | Write | In the game |
|---|---|---|
| a named callee, Capcom's or ours | `AH_CALL(Flags_Test)(bits, 0x13)` | the name |
| an unnamed one | `AH_AT(void (__cdecl*)(unsigned, unsigned, unsigned), 0x579F00)(x, z, 0)` | that address |
| a phase by its table's address | `area_harness::Phase(0x403570)()` | Capcom's, or the `jmp` Inject put there |
| a `.data` table read in place | read the table and call the pointer directly (no `AH_CALL`) | the fuzz has swapped the entries for recorders |

The check is `if (!area_harness::g_active)`, one byte, false outside the
fuzz. `area_harness::Hook` is `int (__cdecl*)(unsigned, unsigned)`.

## 3. What a group writes

Exactly what a spell group writes ([`magic_harness.md`](magic_harness.md)
§3, the same fields, defaults and semantics), with the names substituted:
`namespace ah = area_harness;`, `ah::Clone`, `ah::Callee`, `ah::Group`,
`ah::Run`, `AH_CALL`, `AH_AT`, `AH_PICK`. **Added fields, each with a
default** (a spell group's file compiles unchanged):

| Field | Default | What |
|---|---|---|
| `Clone::shape` | `Shape::kHandler` | the call shape (section 1), the eleventh positional field after `ret_mask`, `calm` |
| `Group::area` | `-1` (not written) | the area number, written into `Game_AreaNumber` every round after the random fill and **before** the seed (a seed may still set another, for a boundary) |
| `DataTable::stride` | 4 | bytes from one entry's pointer to the next (`0x662F28`'s pairs: 8, `at` the first handler cell) |
| `DataTable::nargs` | 0 | argument words the table's handlers take; a hook table's 2 are logged with each call |

Also added: `Object(k)` (field object `k % 30`), `Descriptor(area)` (the
image's `Area_Descriptors[area]`), `at::` constants of the field frame, and
the `Hook` type. Kept for compatibility, with area meanings: `TaskAt(k)` is
one of the first four field objects, `PartyOf(m)` party record `m % 3`,
`SpriteRecord(k)` one of the harness's two records; `EnemyOf` and the
battle `at::` constants still compute what `magic_harness` computes, but
none of them is in this harness's regions.

A group's module: `src/game/area_<nnn>.cpp`, `_fuzz.cpp`, `_callees.h`,
names `Area<NN>_<What>`; one `Run(group)` from `Area<nnn>_Inject()` under
the shadow name `area_<nnn>`, then one plain `BOF3_INJECT(Name);` line per
function. [`area_011.md`](area_011.md) and `area_011_fuzz.cpp` are the
worked example.

## 4. What the harness does

**The state regions** (20, 16,208 bytes with the harness's two records):

| Region | What |
|---|---|
| `0x7DEE40` + `0x40` | the message word `0x7DEE48`, the message box's pen and line cells |
| `Sprite_Objects` `0x7DEE80` + 30 x `0xA4` | the field objects |
| `Sprite_ObjectsExtra` `0x802000` + 4 x `0xA4` | four more |
| `ObjTrio` `0x802D40` + 3 x `0x14C` | the party records; record 0 the leader |
| `0x8034E0` + `0x14` | `Cond_ByteFA` (the chapter) .. `MoveScript_Var7` and their neighbours |
| `0x903840` + `0x40` | `Camera_Distance`, the pending area cells `0x90384C` / `0x903860`, the scratch words `0x903850..` |
| `0x9039A0` + `0x58` | `Field_ScriptFlags` .. the mode bytes `0x9039F2` / `0x9039F3` |
| `Cond_Flags` `0x903F90` + `0x1D0` | the condition rows, the story flags `0x904030`, the party lists `0x904062` / `0x904065`, to `0x904160` |
| `Game_AreaNumber` `0x904EFC` + 4 | |
| `0x905B80` + `0x30` | `Field_EdgeBits` .. `Field_ScriptFlags2` `0x905BA4` |
| `AreaMap_Bytes` / `Field_State` `0x905D94` + 8 | two pointers, put back (below) |
| `0x905E60` + `0x10` | `MapView_Redraw` `0x905E69` |
| `Field_MemberCount` `0x929EC0` | |
| `MapView_Elevation` `0x929F1C`, `MapView_ElevationOffset` `0x92BEE2` | the elevation |
| `0x937F80` + 4 | the pending area word `0x937F82` |
| `0x937F88` + `0x10` | `Sprite_Current`, `MoveScript_F3Divisor`, `Gfx_ClutStripDirty`, `Frame_Counter` |
| `Field_Request` `0x66C7D8` | |
| `AreaMap_Header` `0x8CB580` + `0x2000` | the area block: its header cells (`AreaMap_EntryBase` `+0x22` ..) and the first 8 KiB of entries |
| the harness's two records | `SpriteRecord` |

Against the plan's list (§2) each was checked against the code read: the
plan's `MoveScript_Var*` are the `0x8034E0` and `0x903840` rows (the
movement script's variables live in both, [`move-cmds.md`](move-cmds.md));
added by reading: `Camera_Distance` and `MapView_Redraw` (area 11's
handler 0), the party lists (its handler 1), the field objects' extra
records, the pending area word, `Field_ScriptFlags` / `Field_ScriptFlags2`
(which `event_ops`'s field fuzz found the field code reading). **Not a
region, on purpose:** `Area_Descriptors` and every table of the image (the
real descriptor and tables stay in place), and `Gfx_CurrentEnv` `0x937F84`
(a pointer nothing here writes). An area's own `.data` state is the
group's region.

**Each round:** random bytes over every region; then put back inside what
every area function dereferences - `Sprite_Current` a party record or one
of the first four field objects, `Field_State` a party record (record 0
half the time), `AreaMap_Bytes` into the area block (`+0x800`),
`Field_MemberCount` 1..3, the message word `0xFFFF` two rounds in three;
`Group::area` into `Game_AreaNumber`; the group's seed. Theirs, then ours
with the recorders routed; every region and the log compared. 2,000
rounds per function by default.

**The disturbance.** Two calls in three move one of: `Sprite_Current` (to
a party record or a field object), `Field_State`, a field of
`Sprite_Current` (`+0, 1, 2, 4, 8..0xC, 0x2E, 0x30, 0x34, 0x36, 0x38,
0x3A`; the phase bytes `+1` / `+2` kept below `phase_span` when set), any
byte of the leader's record, `Frame_Counter`, the message word,
`Field_Request`, a byte of the flags, `Field_MemberCount`, a byte of one of
the first four field objects, a byte of the camera / scratch cells, or
(the group's `disturb`) a cell of the group's; then the group's `settle`.
A write through `Sprite_Current` is made only when it lies in the regions.

**The recorders** answer as the spell harness's (`kGarbage`, `kByte`,
`kFlag`, `kBool`, `kRand`, `kPhase`, `kThrough`, `effect`, `custom`,
`deref`, `nargs`, `masks`). A handler's recorder logs `Sprite_Current`,
`Field_State` and the phase bytes (`0x100` for one outside the regions); a
hook's (`DataTable::nargs`) logs `Sprite_Current`, its arguments and the
phase bytes. After the call the clone's answer (`ret_mask`) and the shape's
reads (section 1) are logged.

**The standard callees** (`kStandard`, 91): the band's frontier as the
2026-09-26 walk found it, every function named `AreaMap_*`, `Field_*`,
`Flags_*`, `Gte_*`, `Gpu_*`, `MapView_*`, `Msg_Open*`, `Sprite_*`,
`Party_*`, `Inventory_*`, `Sound_*`, `Music_*`, `Text_Draw*` or `Rand` that
has a signature in `symbols.toml`, plus `Effect_Spawn`, `Math_Sin`,
`Math_Cos`. Each is `AH_OURS` or `AH_THEIRS` as `symbols.toml` says on
2026-09-27 (Capcom's today: `Rand`, `Sound_ResumeAll`, `Effect_Spawn`); a
callee that changes hands fails at start-up and moves column. Masks from
the parameter types; answers `kFlag` for a byte answer, `kBool` for
`Flags_Test` (all of eax is 0 or 1), garbage otherwise. The frontier's 81
unnamed functions and its other named ones (`WorldMapHud_*`, `MoveCmd_*`,
`EventOp_*`, ...) are the group's to list. A callee answering a pointer the
caller follows (`Text_DrawAt`, `Gpu_SetPolyG4`, `Gte_RotMatrix`, ...) wants
the group's own listing with an `effect`, and `Effect_Spawn`'s `kFlag`
never answers its "none", `0xFF` (area 11 lists it as `kByte 0xFE..0x02`).

**The report** is the spell harness's: a line of totals, coverage lines,
the first twelve differing rounds, then a `Fatal` (exit 3).

## 5. What it cannot do

Everything the spell harness cannot ([`magic_harness.md`](magic_harness.md)
§5), and, particular to areas:

- **See a pointer stored at run time and called later.** A root found by
  the walk only; the live walk per world is the check.
- **Stand in for a table that is not in the image.** The real descriptors
  are in place, so a clone reads the real handler arrays; a function that
  dispatches through `+0x3C` itself (an area calling another of its
  handlers through the descriptor) calls the real handler unless the group
  lists that array as a `DataTable`.
- **Hold more than 8 KiB of the area block.** A function that indexes
  `AreaMap_Header` or `AreaMap_Bytes` further wants its seed to keep the
  index inside, or a region of the group's.
- **Know what the flags mean.** A flag test's answer is the recorder's; a
  group whose function reads `Cond_Flags` directly seeds each bit it tests.

## 6. Shadow name and self-test

The harness has no shadow name of its own: each group's is its module's
(`area_011`, `area_cell_hook`). `BOF3X_SHADOW='*'` runs every group, the
spell groups through `magic_harness` and the area groups through this one.

Measured in this worktree (2026-09-27; counts depend on the build
directory): `area_011` 12,000 rounds, 10,656 calls, 0 mismatches, 16,208
bytes (20 regions); `area_cell_hook` 20,000 rounds, 11,619 calls, 0
mismatches. `BOF3X_SHADOW='*'`: @STAR@

## 7. For the area groups of the next wave

- **The shape enum:** `Shape::kHandler` (default), `kChoice`, `kInit`,
  `kHook`, `kTail`, `kState`, `kCallee` - the eleventh `Clone` field, after
  `ret_mask` and `calm`: `{"Area11_DimBackdrop", 0x4017C0, 0x79, kCalls,
  n, nullptr, 0, nullptr, 0, &ours, 0, false, ah::Shape::kInit}`.
- **`Group::area`** is not positional: set it after the aggregate
  (`ah::Group g{...}; g.area = 11; ah::Run(g);`), as area 11 does.
- **`DataTable{at, entries, stride, nargs}`**: an area's state table is
  `{at, n}`; a table of pairs or of hooks says its stride and arity.
  Entries up to 64 a table (the spell harness's 16).
- **`Effect_Spawn` answers `0xFF` for none**: list it `kByte` with a range
  through `0xFF` if a function tests that.
- The tool (`tools/area_rows.py`, group ART) prints the clone rows; each
  row still needs its shape by the table the root came from.

## 8. The cell hook's reader, `0x56E670` (`Area_CellHook`)

`src/game/area_cell_hook.cpp`, shadow `area_cell_hook`. Called only by
`0x56D7A0`, the chapter's `+0x10` cell hook, when the chapter's slot is null
or answers below 0; `Field_LeaderTalkTest` asks that about a `0x51` cell
with the found cell's x and z (`0x903850` / `0x903852`,
[`event-ops.md`](event-ops.md) §11). It walks the (area, handler) pairs at
`0x662F28` (group ART names the table and its layout) **while its cursor is
below `0x663008`** (`MapCell_Handlers`): **28 pairs**, not the 100 the plan
counted from the bytes after. Each pair's area is its first byte,
zero-extended, compared with the whole u16 `Game_AreaNumber`, so an area
number of `0x100` or more matches none. No match: `eax` 0. A match: the
pair's handler `(x, z)` and `movsx eax, al`, which `0x56D7A0` returns.

The 28 handlers are for areas `0x2C`, `0x6C`, `0x31`, `0x4D`, `0x56`,
`0x34`, `0x87`, `0x91`, `0xAF..0xB9` (eleven, one handler `0x4298D0`: the
world-map copies), `0x6D` (`0x4FEEB0`, outside the band), `0x70`, `0xA7`,
`0x8B`, `0x8C`, `0x94`, `0x8D`, `0x75`, `0x76`; no area byte repeats.

**Fuzz:** one clone (`kHook`, `ret_mask` all of eax), the table as a
`DataTable{0x662F2C, 28, 8, 2}`, 20,000 rounds; `Game_AreaNumber` a listed
area half the time, a listed byte with a high byte, a listed area plus or
minus one, any byte or any word. Coverage in this worktree: every handler
reached, 343..440 calls each and 4,995 for the eleven world-map areas'
shared one. 0 mismatches.

**Controls:** section 9.

## 9. Controls

@TABLE@

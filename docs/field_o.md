# Group FO: the field menu's panels, the movement and event-script ops of 0x5738A0..0x57CD89

**Status:** MEASURED (2026-09-29) - round twelve
([`takeover-queue-field-battle.md`](takeover-queue-field-battle.md) section
3, [`takeover-queue-round12.md`](takeover-queue-round12.md) section 8), wave
two, on the round branch's tip `61be26e`. **41 functions ours**
(`src/game/field_o.cpp`, shadow name `field_o`): the cut table's 44 rows for
FO (`analysis/round12_cut.tsv`) less six that are jump-table cases of
functions already ours (section 5), plus `0x576960` (flagged by
`tools/band_rows.py` as code no list has) and two starts no list and no tool
row had, `0x579CA0` `EventScript_SkipIf` and `0x57C3E0` `EventCond_Counter0`
(section 5). Each read to its last instruction with capstone and fuzzed
through the scenario harness's field mode
([`scenario_harness.md`](scenario_harness.md) section 7) without edits to it:
82,000 rounds, **0 mismatches**; 105 controls planted, 103 refused, the
two others equivalent with refused near variants (section 6). Fuzz-only except the
eight the whelp route and the one the dragon route enter (section 9).

| Part | Functions | Reached through |
|---|--:|---|
| The menu panels: stats, EXP, the icon wheel, a 16 x 8 tile, the equipment compare, the ability and item lists, the save slot | 8 | direct calls: the field menu's panel list `0x596980..0x59AA80` (Capcom's, no group's), FS's shop `0x581300`, ours `SaveMenu_DrawSlots` |
| Movement-script ops F9, 88, 87, E9, DB and the states of 88, 87, E9 | 13 | ours `MoveScript_GroupF` / `Group8` / `GroupE` / `GroupD`, the bosses' end moves, `Scena06_LeapAir`; `MoveCmd_Op88States` `0x663AFC`, `MoveCmd_Op87States` `0x663B04`, `MoveCmd_OpE9States` `0x663B84` (read in place) |
| Event-script placement ops 3x, 4x, 7x, 6x, Ax | 5 | ours `EventScript_Op`'s handler table; `EventOp_6x` also the scene placers and FC1's `0x46A600` |
| Event-script conditions 1, 3..7, 9, 11..16 | 13 | `EventScript_Conditions` `0x663B30` (read in place by ours `EventScript_If` / `IfNot` / `Switch`) |
| `EventScript_SkipIf`, `ObjTrio_ClearBit40` | 2 | ours `EventScript_SkipControl`; `MoveScript_Flow`, `Scena03_Scene5` |

The panels' names describe the code's drawing, not a play-tested screen:
which menu page shows the "icon wheel" (`0x573F70`, a turned triangle with up
to three icons) or the 16 x 8 tile (`0x574400`) was not traced; the owner's
walk of the menu ([`menu-screens.md`](menu-screens.md)) is the place to
settle it. Every name is a hypothesis (`symbols.toml` status `hypothesis`).

## 1. What each function does

Every function's comment in `field_o.cpp` is the full read; in short:

### 1.1 The menu panels (`x`, `y` words; `record` a `CharacterRecords` index)

- **`Menu_DrawStatsPanel` `0x5738A0` (x, y, record)**: a box, the four stat
  labels (`0x66A0F8`, 8 apart) and values - the record's words `+0x24`,
  `+0x26`, `+0x2A`, `+0x28` through `Crt_sprintf` / `Text_DrawFont8` - and,
  when the trait byte `+0x1F` is not `0xFF`, system message `0x111 + trait`
  into text record 0 and message `0x34`; the frame pieces.
- **`Menu_DrawExpPanel` `0x573BF0` (x, y, record, next)**: the record's EXP
  `+0xC` (next 0) or `Char_ExpForLevel(record, level + 1)` (next not 0; -1,
  past level 99, drawn as the text at `0x6639B8`), a label, the frame.
- **`Menu_DrawIconWheel` `0x573F70` (x, y, kind, lit, angle)**: the kind's
  name (28-byte entries at `0x6636B0`) through `Text_DrawSmall`; the triangle
  `0x6637C8` turned by `Math_Cos` / `Math_Sin` into `0x6BC860..` and drawn as
  a POLY_G3 shaded by each corner's z; one icon for kind 0, two for 1..3,
  three above, their spots (`0x6636C0`) turned into `0x6BC748..`, lit (a
  `Frame_Counter` pulse) by the lit byte's bits or grey `0x70`, bubble-sorted
  by z and drawn with `Menu_DrawCell8`; the frame pieces.
- **`Menu_DrawTile16` `0x574400` (x, y, u, dim)**: a draw mode (page `0x2F`)
  and one 16 x 8 SPRT (CLUT `0x7800`, v `0xD8`, u << 4) at (x, y) as floats,
  grey `0x80` or dimmed `0x10`, semi-transparent.
- **`Menu_DrawEquipCompare` `0x574EC0` (record, x, y, set, no_preview,
  panel)**: the record's name and four stats; with no_preview 0,
  `Equip_PreviewSet(record, set, marks, values)` and per stat an arrow cell
  (u `0x11` for mark 4, `0x12` for mark 1, else `0x13`) and the previewed value
  in the mark's colour; then the six equipment names (the name tables by
  `+0x12..+0x17`) with their icons, the panel's `+0xB` slot raised in colours 7
  / 2, its `+0xA` slot in 7 / 0; the frame.
- **`Menu_DrawAbilityPanel` `0x575F50` (panel)**: `Char_AbilityList`'s ten
  ids through `Menu_DrawSkillRow` (dimmed to colour 7 when `Skill_CanUse`
  refuses, colour 2 on row `+0xD`, rows `+0xC` / `+0xD` raised), the title
  `0x663984[+0xB]` centred, the arrows (panel `+9` bits 0 / 1, its high nibble
  a countdown stepped here), the frame.
- **`Menu_DrawItemPanel` `0x5763F0` (panel)**: the category (`+8`) from the
  filter `+9`; the category's 0x80 ids and counts that `Item_CanUse(4, ...)`
  passes (category 2 also by `Item_IconKind`) kept at `0x6BC760` / `0x6BC7E0`,
  the rest zeroed; `Menu_ListScroll`; the rows through `Menu_DrawItemRow` (the
  cursor's row raised), the title, "kept / 128", the frame and
  `Menu_DrawScrollBar`.
- **`Menu_DrawSaveSlot` `0x576960` (slot, x, y, summary)**: the slot box and
  number; with a summary the party's icons, the name (copied with four more
  bytes to `0x904BA0`), the level, the play time and `Menu_DrawExpBar`;
  without one the empty text; the frame. The name's x inset is DIV-0029's
  byte, read back (section 2).

### 1.2 The movement script's ops (Sprite_Current the object; Field_ActiveMember its context)

- **`MoveCmd_OpF9` `0x578FA0` (toward, shift)** - renamed from
  `MoveScript_WaitTest`, a call-site name: op F9 a b eases the object along
  its facing. The step is `0x6696DC`'s (x, z) pair by the facing times the
  speed `Field_MoveSpeeds[+0x84]`; on the op's first frame (acceleration 0) the
  target is `Field_DirectionSteps` * 2 * b past the position, and the
  velocity starts at 0 accelerating (a not 0) or at the step decelerating; a
  party object (`+6` `0x0A`) moves its member on a half-unit crossing and runs
  its party record's tilt counts; eax 0 at the target (taken exactly), else 1
  (MoveScript_GroupF repeats the op while it is not 0).
- **`MoveCmd_Op88` `0x5794D0` / `MoveCmd_Op87` `0x5795F0`**: jump by
  Sprite_Current `+4` through `0x663AFC` / `0x663B04`. **Op 88** state 0
  (`0x5794F0`) takes a tint record (`Sprite_SetTint(sprite, 0, 0, 0, 1)` into
  context `+0x9F`) at colour `0xC0`, state 1 (`0x579560`) steps it down by 4 to
  `0x80` and releases it (`Tint_Release`, `+0` bit 6). **Op 87** state 0
  (`0x579610`) starts at `0x80` (bit 6 cleared), state 1 (`0x579690`) steps up
  to `0xC0` (signed compare: `0x80..0xBF` only) and releases (bit 5 cleared,
  the colour 0). Every waiting frame decrements the context's wait `+0x8A`.
- **`MoveCmd_OpE9` `0x57C8E0` (object, a, b, c, d, e, f)**: its seven words
  handed on to `MoveCmd_OpE9States[+4]`, the jump: **Start** `0x57C920` (the
  steps `max(|a|, |b|)`, the frames `16 / speed`, the rise `c << 8`, the
  velocity `(a << 15) / (frames * steps)`; the kind-2 object instead aims
  `Field_Kind2X / Z`), **Arc** `0x57CA80` (velocity, rise and fall d, the
  ground from `AreaMap_Elevation`, the animation e at the top, `MoveCmd_OpDB`
  and state 3 after the last step), **Kind2** `0x57CBB0` (the kind-2 object's
  arc with `MapView_SetElevation`), **Fall** `0x57CCE0` (down to the ground).
  al: 0 when done, 1 while running (op E9 repeats while al is not 0).
- **`MoveCmd_OpDB` `0x57CD40`**: x and z each plus its velocity when that is
  above 0, then rounded down to the half unit.

### 1.3 The event script

- **`EventOp_3x` / `4x` / `7x` / `6x` / `Ax`** (`0x57A7C0`, `0x57A990`,
  `0x57AB50`, `0x57AD10`, `0x57AEC0`): placements of `Sprite_Objects[count]`
  (the count `0x903850`, below 30), each with its own operand layout
  (`field_o.cpp`); 3x / 4x / 7x as `EventOp_1x` with a bank, 6x a party
  member's sprite (`Field_MemberSprite(op[1], op[0xF])`), Ax as `EventOp_Bx`
  from the area descriptor's `+8` entry and, for an object of speed 0, the
  area block's cell(s) under it set to `0x10`.
- **The conditions** (`EventScript_Conditions` entries; each handed the
  script position, al the answer): 1 `EventCond_Area` (`Game_AreaNumber`), 3..6
  `EventCond_Counter0..3` (`0x903848..0x90384B`), 7 `EventCond_Run`
  (`MoveScript_Var7`), 9 `EventCond_Status1` (`Field_StatusBits` bit 0 or its
  complement), 11 `EventCond_LeaderId` (`ObjTrio +0x89`), 12
  `EventCond_StoryFlag` (`Flags_Test(0x904030, operand)`), 13
  `EventCond_KeyItem`, 14 `EventCond_ChapterAtMost` (`Cond_ByteFA <=`), 15
  `EventCond_RecordBit0` (`CharacterRecords[operand] +0xB` bit 0), 16
  `EventCond_Gene` (`Flags_Test(0x904650, operand)`, the gene bits:
  [`battle_e5.md`](battle_e5.md)). Entries 0, 2, 8 were ours already
  (`event_script.cpp`); 10 is null.
- **`EventScript_SkipIf` `0x579CA0`**: an F0 / F1 stepped over to past the FE
  of its depth (ops by `EventScript_OpLengths`, controls by
  `EventScript_SkipControl`, an FD stepped over).
- **`ObjTrio_ClearBit40` `0x57C7E0`**: bit 6 of the three party objects'
  first byte cleared.

### 1.4 The PSX twins

`analysis/pairs_propagated.json` pairs 28 of the 41 (`tools/band_rows.py`'s
twin column): six panels (not the ability and item panels), op F9, 6x, Ax,
twelve conditions, `ObjTrio_ClearBit40`, E9 and its four states, op DB; none
for `EventOp_3x` / `4x` / `7x`, ops 87 / 88 and their states,
`EventScript_SkipIf` and `0x57C3E0` (the first five and SkipIf carry older
`psx` fields from `psx_pair`). None has a name in the
sibling's `names/*.toml` or `symbols.toml` (searched 2026-09-29), so no name
was transferred; each twin is cited in its `symbols.toml` evidence as a
hypothesis. The existing `psx` fields (`EventOp_*`, `MoveCmd_*`,
`EventScript_SkipIf`, `ObjTrio_ClearBit40`) are kept.

## 2. Divergence

None: every function is a faithful replacement, no DIVERGENCE.md entry is
owed. `Menu_DrawSaveSlot` honours **DIV-0029** by reading the disp8 of `lea
eax, [ebp + 0x13]` at `0x576A46` back from the image (`0x576A48`: `0x13` as
shipped, `0x15` when `YesNoLayout_Inject` has patched it, any other value a
Fatal) - Menu_YesNo's pattern for DIV-0027 - so the patch keeps working with
the function ours (DIV-0029's entry notes it). The empty slot's text
(`0x576B07`, `lea edx, [ebp + 0x13]`) was never patched and stays at `+0x13`.
No other byte patch or cheat touches the band (`DIVERGENCE.md`, `cheats.cpp`
grepped for every address).

Where the original jumps through a table past its entries or divides by 0,
ours aborts with a `Fatal` naming the function (the round9 rule; nothing
reaches it): `MoveCmd_Op88` / `Op87` / `OpE9` past their tables (op 88's
bytes 2 and 3 reach op 87's states, as in the original), `MoveCmd_OpE9Start`
/ `Arc` / `Kind2` on a zero divisor (section 10).

`eax` on return: the conditions and E9's answer in `al`, `MoveCmd_OpF9` 0 or 1
in `eax`, `EventScript_SkipIf` the position; every other is `void`.

## 3. The arguments pushed with leftovers

Where Capcom pushes a byte or a word in a register whose upper bytes are a
callee's or the caller's leftovers (the menu panels' `mov cx, [esi + 6]; add
cx, 3; push ecx`, the conditions' `mov dl, [ecx]; push edx`, `EventOp_6x`'s
`Field_MemberSprite` pair, `EventOp_Ax`'s `AreaMap_SetByte` words), ours
passes the value and the fuzz re-lists the callee at the width its code
reads, each cited in `field_o_fuzz.cpp`: `Menu_DrawBox`, `Menu_DrawPiece(s)`,
`Menu_DrawIcon8`, `Text_DrawAt` / `DrawSmall` / `DrawFont8` / `DrawFont12`,
`Menu_DrawScrollBar` (as `battle_e7_fuzz.cpp` has them), `Menu_DrawCell8`,
`Menu_DrawBigIcon`, `Menu_DrawExpBar`, `Menu_DrawItemRow`,
`Menu_DrawSkillRow`, `Msg_SystemPtr`, `TextRecord_Set`, `Char_ExpForLevel`,
`Char_AbilityList`, `Skill_*`, `Item_CanUse` / `IconKind`,
`Field_MemberSprite`, `KeyItem_Has`, `AreaMap_SetByte`, `MapView_SetElevation`,
`Sprite_EnsureAnimation`. Every mask is what the callee (ours) reads: for the
harness fold, these are FH's field-standard rows at `kAll` where the callers
push leftovers.

## 4. The fuzz (`field_o_fuzz.cpp`)

One `Run` under `BOF3X_SHADOW=field_o`, `Group::field`, 2,000 rounds a
function (`BOF3X_FO_ROUNDS=n` raises it; `BOF3X_FO_ONLY=<name>` runs one
function alone, with a fault's EIP in the log). Shapes: the panels and
`MoveCmd_OpF9` `kCall` (a panel, set or summary a scratch pointer), op 88 /
87 and their states and `MoveCmd_OpDB` `kSprite`, `ObjTrio_ClearBit40`
`kState`, the event ops `kScript`, the conditions `kCursor` (al),
`EventScript_SkipIf` `kCall` with the script cursor's op, E9 and its states
`kCall` with the object the record's context `+0x80` (as the callers hand it:
`boss_sa_callees.h` `kFieldActors`).

**Stand-ins beyond the field-standard set:**

- `MoveCmd_OpE9States`: a typed recorder per entry written into `0x663B84`
  by the seed (a region, put back) - the seven words logged at the widths the
  states read (the handler recorder logs no arguments; `scena_sc0`'s
  `ObjectEntry` pattern). Op 88 / 87's four states are a `DataTable`
  (`0x663AFC`, 4: op 88's bytes 2, 3 reach op 87's pair).
- `Crt_sprintf` logs as many words as its format has conversions (callers
  push none, one or two past the format; the rest is their frame) and writes
  up to seven characters into the buffer.
- Louder where the caller reads back: `Equip_PreviewSet` fills the caller's
  four marks and four words; `Sprite_InitFromEntry` sets the sprite's `+0x54`
  (EventOp_Ax reads the entry's byte 2 through it); `MoveScript_ObjectKind`
  answers exactly 0..3 (the caller compares eax with 2); `Char_ExpForLevel`
  answers -1 a third of the time; `Item_IconKind` a kind 2..7;
  `EventScript_SkipControl` the byte after the control; `Msg_SystemPtr` and
  `Char_AbilityList` point into the text buffer; `Menu_ListScroll` is
  `kThrough` (ours, self-contained: it writes the top, offset, moving and
  state bytes the panel reads back).
- `MoveCmd_OpDB` `kPhase` (called by `MoveCmd_OpE9Arc`).

**Regions beyond field mode's:** the eight character records past the
standard `0x24` bytes (`0x903A94..0x903F90`), `0x6BC740` + `0x188` (the item
panel's lists, the wheel's spots and triangle), `MoveScript_PartyRecords`,
`Field_ActiveMember`, `0x663B84` + `0x10`, the inventory's lists
`0x904160` + `0x400`.

**Seeds:** `Field_ActiveMember` a sprite record, an extra record (a party
slot for op F9) or Sprite_Current; the count word 0..31 (30, 31 the early
return); the four records' `+0xA` 1..16 (E9 Start divides by it after a call)
and `+0x54` a readable pointer; per function the traits at `0xFF`, levels
0 / 1 / 98 / 99, panel bytes inside their tables (the ability title's `+0xB`
below 8, the item filter below 8, the scroll state `0x1n` / `0xFn`), some ids
0, the party icons `0xFF`, op F9's speed index below 6, a zero acceleration,
kind `0x0A` and a target within a unit, the colours around `0x80` / `0xC0`,
a synthetic script for SkipIf (F0 / F1, up to eight ops of their lengths,
controls F0..FC and FDs, then the FE; no FF), an area whose descriptor and
`+8` table are image data for Ax, each condition's operand equal to its cell
half the time, E9's speeds 1..5 (0, 6, 7 for Start's early answer), frames 0
or 1, steps 0, kind 6, e `0xFF`. **Disturbance** (the group's): the context
pointer, the count word (below 30), a colour byte.

**Measured** (this worktree, 2026-09-29): `BOF3X_SHADOW=field_o` exit 0, 82,000 rounds over
41 functions, 1,142,548 calls to the stand-ins, **0 mismatches**, 25,404 bytes
of state in 42 regions; 256 stand-ins (174 field-standard). Every stand-in
listed and every table entry was reached (the coverage line): E9's four
states about 500 each, op 88 / 87's four 500..1,500, `Tint_Release` 2,085,
`Sprite_EnsureAnimation` 262, `Party_MoveMember` 409, `AreaMap_SetByte`
1,545, `Menu_ListScroll` through. `BOF3X_SHADOW='*'`: exit 0, 672 self-test
lines, none with a mismatch, `inject: 6594 ours` (6,553 + 41).

## 5. What the cut and the tool said, settled

- **Six cut starts are not functions** - jump-table cases of functions ours
  already, each held by ours (read in `move_groups.cpp` / `move_script.cpp`),
  so no `[[func]]` entry and no inject: `0x577600` (MoveScript_GroupE's case
  EC: `+7 ^= 4`), `0x5776A0` (GroupE's EA: the tint, a type-0x0A sprite keeps
  its colour bytes), `0x577B50` (MoveScript_GroupF's FC: `MoveCmd_TestFC` and
  sound `0x103`), `0x578550` (MoveScript_GroupC's C9: `+0x2B`), `0x578790`
  (MoveScript_Group8's 85: the kind-2 object toward ObjTrio 0) and
  **`0x578A40`** (MoveScript_Group9's 90..95: `Effect_Spawn` into `+0xB`;
  reached through `0x578AD8`, Group9's table - the brief's list named five,
  this is the sixth).
- **`0x576960` is a function** (`Menu_DrawSaveSlot`, called by ours
  `SaveMenu_DrawSlots`), inside the catalog's `0x5763F0` extent (0x759 there;
  the code is 0x570 with its jump table).
- **Two starts in no list at all**, found by walking the band for code no
  extent covers: `0x57C3E0` (`EventScript_Conditions[3]`, between
  `MoveScript_Variable`'s code and its table's end) and `0x579CA0`
  (`EventScript_SkipIf`, named, never taken, no catalog row). The band's other
  uncovered bytes are `MoveScript_Counter*` (`0x57C460..0x57C4C0`, ours in
  `move_script.cpp`, no `entries_logic.txt` line: listed for the coordinator).
- **Extents**: the tool's, read from the code, stand; the cut's sizes differ
  by padding in 18 and by code in six (`0x5763F0`, `0x577600`, `0x5776A0`,
  `0x578550`, `0x578790`, `0x578A40`: each a host's extent or a case).

## 6. Controls

**105 planted, 103 refused** - 101 by a count, 2 by a hang (`EventScript_SkipIf`
57 and 59: an FD left unstepped or a control left unskipped loops for ever on
the seeded script; a hang proves less than a count) - and **2 not refused,
both equivalent with a refused near variant**:

- **12** (`Menu_DrawIconWheel`'s sort, `<=` to `<`): the two differ only when
  two icons' turned z are equal, which the random `Math_Cos` / `Math_Sin`
  answers practically never give; the near variant 104 (sorting on x) is
  refused in 1,260 rounds.
- **103** (`MoveCmd_OpDB`, `> 0` to `>= 0`): at 0 the add is of 0 - no input
  can tell them apart; the near variant 105 (`< 0`) is refused in every round.

Every function has at least one refused control (the conditions one each,
the panels four to seven). Method: `controls.py` in the session scratchpad
(`fo/`) plants a round of controls (at most one per function; a
shared-helper plant alone), rebuilds, runs each planted function alone
(`BOF3X_FO_ONLY`, 2,000 rounds), restores and rebuilds; 78 and 79 were re-run
alone (their plants met in one round). In this worktree:

| # | Function | Plant (the changed part) | Result |
|--:|---|---|---|
| 1 | `Menu_DrawStatsPanel` | `y + 3, 0x94, 0x2D, 0,` -> `y + 3, 0x94, 0x2E, 0,` | refused, 2000 mismatches |
| 2 | `Menu_DrawStatsPanel` | `>(Word(rec + 0x2A)));` -> `>(Word(rec + 0x28)));` | refused, 2000 mismatches |
| 3 | `Menu_DrawStatsPanel` | `if (trait != 0xFF) {` -> `if (trait != 0xFE) {` | refused, 508 mismatches |
| 4 | `Menu_DrawStatsPanel` | `ed b = 0; b < 0x10; ++` -> `ed b = 0; b < 0xF; ++` | refused, 2000 mismatches |
| 5 | `Menu_DrawExpPanel` | `0x1E + (want ? 0 : 6)` -> `0x1E + (want ? 6 : 0)` | refused, 2000 mismatches |
| 6 | `Menu_DrawExpPanel` | `har>(rec[0xA] + 1)` -> `har>(rec[0xA] + 2)` | refused, 1026 mismatches |
| 7 | `Menu_DrawExpPanel` | `if (v == -1) Sp` -> `if (v == -2) Sp` | refused, 311 mismatches |
| 8 | `Menu_DrawIconWheel` | `= 0 ? 1 : kb <= 3 ? 2` -> `= 0 ? 1 : kb <= 2 ? 2` | refused, 238 mismatches |
| 9 | `Menu_DrawIconWheel` | `` -> `` | refused, 2000 mismatches |
| 10 | `Menu_DrawIconWheel` | `SetLong(a + 8, Sb(static_cast<U>(x0)));` -> `SetLong(a + 8, x0);` | refused, 1234 mismatches |
| 11 | `Menu_DrawIconWheel` | `colour[j] = 0x70;` -> `colour[j] = 0x71;` | refused, 1592 mismatches |
| 12 | `Menu_DrawIconWheel` | `f (Long(a + 4) <= Lon` -> `f (Long(a + 4) < Lon` | **not refused**, 0 mismatches |
| 13 | `Menu_DrawIconWheel` | `gned char>(v / 24 + 0` -> `gned char>(v / 23 + 0` | refused, 2000 mismatches |
| 14 | `Menu_DrawIconWheel` | `(fc & 8 ? f` -> `(fc & 4 ? f` | refused, 824 mismatches |
| 15 | `Menu_DrawTile16` | `char>(dim) ? 0x10 :` -> `char>(dim) ? 0x80 :` | refused, 2000 mismatches |
| 16 | `Menu_DrawTile16` | `p[0x15] = 0xD8;` -> `p[0x15] = 0xD0;` | refused, 2000 mismatches |
| 17 | `Menu_DrawTile16` | `_cast<U>(x) & 0xFFFF)` -> `_cast<U>(x) & 0x7FFF)` | refused, 1017 mismatches |
| 18 | `Menu_DrawEquipCompare` | `4 ? 0x11 : m == 1 ? 0` -> `4 ? 0x11 : m == 2 ? 0` | refused, 939 mismatches |
| 19 | `Menu_DrawEquipCompare` | `== panel[0xB] ? 2 : 0;` -> `== panel[0xB] ? 0 : 2;` | refused, 1975 mismatches |
| 20 | `Menu_DrawEquipCompare` | `Armour + rec[0x14] *` -> `Armour + rec[0x15] *` | refused, 1985 mismatches |
| 21 | `Menu_DrawEquipCompare` | `ar>(no_preview) == 0)` -> `ar>(no_preview) != 0)` | refused, 2000 mismatches |
| 22 | `Menu_DrawEquipCompare` | `x78, bottom, 0x12, 1)` -> `x78, bottom, 0x11, 1)` | refused, 2000 mismatches |
| 23 | `Menu_DrawEquipCompare` | `st<int>(i) + 0x1C;` -> `st<int>(i) + 0x1D;` | refused, 1005 mismatches |
| 24 | `Menu_DrawAbilityPanel` | `our = dim ? 7 : 0;` -> `our = dim ? 7 : 1;` | refused, 1989 mismatches |
| 25 | `Menu_DrawAbilityPanel` | `f (b == panel[0xD]) c` -> `f (b == panel[0xC]) c` | refused, 1537 mismatches |
| 26 | `Menu_DrawAbilityPanel` | `if (colour != 7) SH` -> `if (colour != 2) SH` | refused, 1324 mismatches |
| 27 | `Menu_DrawAbilityPanel` | `har>(arrows - 0x10) :` -> `har>(arrows - 0x20) :` | refused, 1117 mismatches |
| 28 | `Menu_DrawAbilityPanel` | `turn x + 6 * (0xD - s` -> `turn x + 6 * (0xC - s` | refused, 2000 mismatches |
| 29 | `Menu_DrawAbilityPanel` | `` -> `` | refused, 2000 mismatches |
| 30 | `Menu_DrawItemPanel` | `case 4: case 5: pa` -> `case 4: pa` | refused, 247 mismatches |
| 31 | `Menu_DrawItemPanel` | `- 1u != kindv - 2u) c` -> `- 1u != kindv - 1u) c` | refused, 790 mismatches |
| 32 | `Menu_DrawItemPanel` | `istIds)[panel[0xB]];` -> `istIds)[panel[0xA]];` | refused, 928 mismatches |
| 33 | `Menu_DrawItemPanel` | `PanelY(panel) + Sb(offset) + 0` -> `PanelY(panel) + offset + 0` | refused, 485 mismatches |
| 34 | `Menu_DrawItemPanel` | `(panel[0xA] + j ==` -> `(panel[0xA] + j + 1 ==` | refused, 30 mismatches |
| 35 | `Menu_DrawItemPanel` | `gned>(total), 0x80u);` -> `gned>(total), 0x7Fu);` | refused, 2000 mismatches |
| 36 | `Menu_DrawItemPanel` | `s + kept), 0, 0x80u -` -> `s + kept), 0, 0x7Fu -` | refused, 1994 mismatches |
| 37 | `Menu_DrawSaveSlot` | `if (icon != 0xFF) SH` -> `if (icon != 0xFE) SH` | refused, 1313 mismatches |
| 38 | `Menu_DrawSaveSlot` | `5, summary + 0x16, 4)` -> `5, summary + 0x15, 4)` | refused, 1496 mismatches |
| 39 | `Menu_DrawSaveSlot` | `summary[0x14] ? 2 : 0` -> `summary[0x14] ? 1 : 0` | refused, 741 mismatches |
| 40 | `Menu_DrawSaveSlot` | `_DrawAt)(x + 0x13, y` -> `_DrawAt)(x + 0x15, y` | refused, 504 mismatches |
| 41 | `Menu_DrawSaveSlot` | `x12, 0, summary[8]` -> `x12, 0, summary[9]` | refused, 1492 mismatches |
| 42 | `MoveCmd_OpF9` | `(x > tx) : !(x < tx)` -> `(x > tx) : !(x <= tx)` | refused, 32 mismatches |
| 43 | `MoveCmd_OpF9` | `if (towa` -> `if (!towa` | refused, 703 mismatches |
| 44 | `MoveCmd_OpF9` | `ned s = shift & 31u;` -> `ned s = shift & 15u;` | refused, 304 mismatches |
| 45 | `MoveCmd_OpF9` | `_PartyRecords + Sb(static_cast<U>(s` -> `_PartyRecords + static_cast<unsigne` | refused, 708 mismatches |
| 46 | `MoveCmd_OpF9` | `) + p) ^ p) & 0x8000u` -> `) + p) ^ p) & 0x10000u` | refused, 199 mismatches |
| 47 | `MoveCmd_OpF9` | `ng(Sc() + 0x18, 0);` -> `ng(Sc() + 0x18, 1);` | refused, 356 mismatches |
| 48 | `MoveCmd_Op88` | `8States, Sc()[4], "M` -> `8States, Sc()[4] ^ 1u, "M` | refused, 2000 mismatches |
| 49 | `MoveCmd_Op88Start` | `Sc()[0x5F] = 0xC0;` -> `Sc()[0x5F] = 0xC1;` | refused, 2000 mismatches |
| 50 | `MoveCmd_Op88Start` | `Sc()[4] = 1; }` -> `Sc()[4] = 2; }` | refused, 2000 mismatches |
| 51 | `MoveCmd_Op88Fade` | `char>(Sc()[c] - 4)` -> `char>(Sc()[c] - 2)` | refused, 1476 mismatches |
| 52 | `MoveCmd_Op88Fade` | `Sc()[0] \|= 0x40;` -> `Sc()[0] \|= 0x20;` | refused, 767 mismatches |
| 53 | `MoveCmd_Op87` | `7States, Sc()[4], "M` -> `7States, Sc()[4] ^ 1u, "M` | refused, 2000 mismatches |
| 54 | `MoveCmd_Op87Start` | `Sc()[0] &= 0xBF;` -> `Sc()[0] &= 0xBE;` | refused, 1023 mismatches |
| 55 | `MoveCmd_Op87Fade` | `if (Sb(Sc()[c]) < Sb(0xC0)) Sc` -> `if (Sc()[c] < 0xC0) Sc` | refused, 692 mismatches |
| 56 | `MoveCmd_Op87Fade` | `Sc()[0] &= 0xDF;` -> `Sc()[0] &= 0xDE;` | refused, 520 mismatches |
| 57 | `EventScript_SkipIf` | `if (at[0] == 0xFD) ++` -> `if (at[0] == 0xFC) ++` | refused, hang (300 s, pid killed) |
| 58 | `EventScript_SkipIf` | `return at + 1; }` -> `return at; }` | refused, 2000 mismatches |
| 59 | `EventScript_SkipIf` | `if (b < 0xFD) at` -> `if (b < 0xFC) at` | refused, hang (300 s, pid killed) |
| 60 | `EventOp_3x` | `) + 0x70, op[0x11]);` -> `) + 0x70, op[0x10]);` | refused, 1312 mismatches |
| 61 | `EventOp_3x` | `n[0x83] = op[0x10];` -> `n[0x83] = op[0xF];` | refused, 1317 mismatches |
| 62 | `EventOp_4x` | `n[0xA0] = op[0x10];` -> `n[0xA0] = op[0xF];` | refused, 1317 mismatches |
| 63 | `EventOp_4x` | `Sc()[2] = op[0xC];` -> `Sc()[2] = op[0xB];` | refused, 1292 mismatches |
| 64 | `EventOp_7x` | `n[0x9E] = op[0x10];` -> `n[0x9E] = op[0xF];` | refused, 1317 mismatches |
| 65 | `EventOp_7x` | `Z(Coordinate(op[5], op[6]),` -> `Z(Coordinate(op[6], op[5]),` | refused, 1316 mismatches |
| 66 | `EventOp_6x` | `te)(op[1], op[0xF]);` -> `te)(op[1], op[0xE]);` | refused, 1316 mismatches |
| 67 | `EventOp_6x` | `etFlags)(op + 0xB);` -> `etFlags)(op + 0xC);` | refused, 1319 mismatches |
| 68 | `EventOp_Ax` | `Colour(0x80);` -> `Colour(0x81);` | refused, 1325 mismatches |
| 69 | `EventOp_Ax` | `(Count())[0x84] == 0)` -> `(Count())[0x84] != 0)` | refused, 1325 mismatches |
| 70 | `EventOp_Ax` | `trAt(sc + 0x54)[2];` -> `trAt(sc + 0x54)[3];` | refused, 508 mismatches |
| 71 | `EventOp_Ax` | `tyRecord_Clear)(4);` -> `tyRecord_Clear)(3);` | refused, 1325 mismatches |
| 72 | `EventOp_Ax` | `` -> `` | refused, 340 mismatches |
| 73 | `EventOp_3x` | `ount()) + 0x94, 0);` -> `ount()) + 0x94, 1);` | refused, 1319 mismatches |
| 74 | `EventOp_6x` | `if (Count() >= 30) re` -> `if (Count() >= 29) re` | refused, 32 mismatches |
| 75 | `EventCond_Area` | `Game_AreaNumber == **` -> `Game_AreaNumber != **` | refused, 2000 mismatches |
| 76 | `EventCond_Counter0` | `At(0x903848)[0] == **` -> `At(0x903848)[0] <= **` | refused, 490 mismatches |
| 77 | `EventCond_Counter1` | `At(0x903849)[0] == **` -> `At(0x903849)[0] >= **` | refused, 475 mismatches |
| 78 | `EventCond_Counter2` | `eturn At(0x90384A)[0]` -> `eturn At(0x90384B)[0]` | refused, 1003 mismatches (re-run alone) |
| 79 | `EventCond_Counter3` | `At(0x90384B)[0] == **` -> `At(0x90384B)[0] != **` | refused, 2000 mismatches (re-run alone) |
| 80 | `EventCond_Run` | `oveScript_Var7) == **` -> `oveScript_Var7) != **` | refused, 2000 mismatches |
| 81 | `EventCond_Status1` | `(**at == 1 ? b` -> `(**at == 2 ? b` | refused, 1012 mismatches |
| 82 | `EventCond_LeaderId` | `ObjTrio[0x89] ==` -> `ObjTrio[0x88] ==` | refused, 1005 mismatches |
| 83 | `EventCond_StoryFlag` | `Test)(At(0x904030), *` -> `Test)(At(0x904031), *` | refused, 2000 mismatches |
| 84 | `EventCond_KeyItem` | `eyItem_Has)(**at);` -> `eyItem_Has)(**at + 1);` | refused, 2000 mismatches |
| 85 | `EventCond_ChapterAtMost` | `>(Cond_ByteFA) <= **a` -> `>(Cond_ByteFA) < **a` | refused, 310 mismatches |
| 86 | `EventCond_RecordBit0` | `rd(**at)[0xB] & 1;` -> `rd(**at)[0xB] & 2;` | refused, 1053 mismatches |
| 87 | `EventCond_Gene` | `Test)(At(0x904650), *` -> `Test)(At(0x904651), *` | refused, 2000 mismatches |
| 88 | `ObjTrio_ClearBit40` | `io[0x298] &= 0xBF;` -> `io[0x298] &= 0xBE;` | refused, 983 mismatches |
| 89 | `MoveCmd_OpE9` | `OpE9"))(object, a, b, c,` -> `OpE9"))(object, b, a, c,` | refused, 1385 mismatches |
| 90 | `MoveCmd_OpE9Start` | `har>(ma < mb ? mb : ma);` -> `har>(ma < mb ? ma : mb);` | refused, 1056 mismatches |
| 91 | `MoveCmd_OpE9Start` | `short>(speed << 3);` -> `short>(speed << 2);` | refused, 353 mismatches |
| 92 | `MoveCmd_OpE9Start` | `` -> `` | refused, 805 mismatches |
| 93 | `MoveCmd_OpE9Start` | `ed == 0) return 0;` -> `ed == 0) return 1;` | refused, 508 mismatches |
| 94 | `MoveCmd_OpE9Arc` | `> 0) && !(rise < 0)` -> `> 0) && !(rise <= 0)` | refused, 150 mismatches |
| 95 | `MoveCmd_OpE9Arc` | `sc[0x2A] = f & 1;` -> `sc[0x2A] = f & 2;` | refused, 73 mismatches |
| 96 | `MoveCmd_OpE9Arc` | `Sc()[4] = 3;` -> `Sc()[4] = 2;` | refused, 264 mismatches |
| 97 | `MoveCmd_OpE9Arc` | `if (f & 2) {` -> `if (f & 1) {` | refused, 269 mismatches |
| 98 | `MoveCmd_OpE9Kind2` | `st<U>(ground)) < S16(Word(sc + 0x3E))) {         sc[0` -> `st<U>(ground)) <= S16(Word(sc + 0x3E))) {         sc[` | refused, 93 mismatches |
| 99 | `MoveCmd_OpE9Kind2` | `if (f & 2) Sc` -> `if (f & 1) Sc` | refused, 238 mismatches |
| 100 | `MoveCmd_OpE9Kind2` | `rite_Kind2 + 0x3E));` -> `rite_Kind2 + 0x3C));` | refused, 2000 mismatches |
| 101 | `MoveCmd_OpE9Fall` | `return 0;` -> `return 2;` | refused, 999 mismatches |
| 102 | `MoveCmd_OpDB` | `` -> `` | refused, 1015 mismatches |
| 103 | `MoveCmd_OpDB` | `ong(sc + 0x10) > 0)` -> `ong(sc + 0x10) >= 0)` | **not refused**, 0 mismatches |
| 104 | `Menu_DrawIconWheel` | `if (Long(a + 4) <= Long(a + 0xC)) c` -> `if (Long(a) <= Long(a + 8)) c` | refused, 1260 mismatches |
| 105 | `MoveCmd_OpDB` | `Long(sc + 0x10) > 0)` -> `Long(sc + 0x10) < 0)` | refused, 2000 mismatches |


## 7. Calls across groups

**Out of FO, raw:** none - every callee is ours already or Capcom's by name.

**Into FO from outside the group** (the rebinding pass's list):

| Function | Callers |
|---|---|
| `EventOp_6x` `0x57AD10` | FC1's `0x46A600` (two sites, raw in FC1's work: the coordinator rebinds); ours by name: `Area141_PlacePair*` / `PlaceOneAnimated` (area_w3e), `Scena08_SpawnPair` (scena_sc7), `Scena13_SpawnPairA / B` (scena_sc13), `Scena15`'s (scena_sc15), `EventScript_Op`'s table |
| `Menu_DrawTile16` `0x574400` | FS's `0x581300` (three sites, raw in FS's work); Capcom's `0x59AA80` (no group's) |
| `Menu_DrawSaveSlot` `0x576960` | ours `SaveMenu_DrawSlots` (through `save_menu_callees.h` `kSlotDraw`, rebound) |
| `MoveCmd_OpE9` `0x57C8E0` | ours `MoveScript_GroupE` (move_groups), `BossNue_EndMove`, `BossWeretigr_EndMove` (boss_sa, through `boss_harness`'s standard row, rebound) |
| `MoveCmd_OpDB` `0x57CD40` | ours `MoveScript_GroupD`, `Scena06_LeapAir` (scena_sc6's listing rebound) |
| `MoveCmd_Op87` / `Op88`, `MoveCmd_OpF9`, `ObjTrio_ClearBit40`, `EventScript_SkipIf`, the event ops | ours by name (`move_groups.cpp`, `move_script.cpp`, `event_script.cpp`, `scena_sc3`): no rebinding needed |
| the other panels | Capcom's field-menu code `0x596980..0x59AA80` (no group's) |
| the conditions, the op / E9 states | read in place from `.data` by ours: no rebinding |

`ScenarioHarnessFh_Inject`'s self-test copies three of FO's functions from
the image (`0x57C1A0`, `0x57C230`, `0x57C8E0`); `FieldO_Inject` is placed
after it in `inject_all.cpp`.

## 8. The rebinding

**Rebound** (the round-ten form, one line each):

- `save_menu_callees.h` `kSlotDraw` = `bof3::addr::Menu_DrawSaveSlot` (the
  same value, so `save_menu_fuzz.cpp`'s key stands; the header gains the
  generated include).
- **Four `_THEIRS` rows turned `_OURS`**, which a taken callee needs or the
  harness refuses it at start-up ("not Capcom's code"): `boss_harness.cpp`'s
  standard `MoveCmd_OpE9` row (the one line of a harness this group edits -
  without it every boss group's `Run` Fatals under `'*'`; **for the
  coordinator's fold**), `area_w3e_fuzz.cpp` and `scena_sc13_fuzz.cpp`'s
  `EventOp_6x`, `scena_sc6_fuzz.cpp`'s `MoveCmd_OpDB`.
- `MoveScript_WaitTest` renamed `MoveCmd_OpF9`: `move_script.cpp`'s callee
  table and `movement-script.md` follow.

**Left raw, on purpose:** the fuzz files' `CallSite` tables and switch keys
(`move_groups.cpp`'s `case 0x5794D0` / `0x5795F0` / `0x57C8E0` / `0x57CD40`
and its call-site lists, `event_script_fuzz.cpp`'s handler keys,
`boss_sa_fuzz.cpp`, `scena_sc3/sc6/sc7/sc13/sc15_fuzz.cpp`,
`area_w3e_fuzz.cpp`: the keys, the round-ten rule); `scenario_harness.cpp`'s
band `{0x5738A0, 0x57CD8A}` (a range, not a call) and its two hand-agnostic
rows (`EventOp_6x`, `ObjTrio_ClearBit40`, which register either way);
`scenario_harness_fh.cpp`'s three copies (the harness's self-test clones
Capcom's bytes and must stay raw); comments (`save_menu.cpp`,
`yes_no_layout.cpp`).

## 9. The live route

The two recorded routes' first-call traces (`analysis/calltrace/reach_whelp`,
`reach_dragon`, all original, 2026-09-29 at `979a567`):

| Route | FO functions entered (frame, caller) |
|---|---|
| `whelpBoss.txt` | `MoveCmd_OpF9` (2,230, GroupF `0x57798F`), `EventOp_3x` (9,146), `EventOp_7x` (283), `EventOp_6x` (2,574), `EventOp_Ax` (3,317; all four from `EventScript_Op`), `ObjTrio_ClearBit40` (12,709, `MoveScript_Flow`), `MoveCmd_OpE9` (3,205, GroupE), `MoveCmd_OpDB` (3,227, from inside `0x57CA80` - so `MoveCmd_OpE9Arc` ran, through the table) |
| `dragonTransform.txt` | `EventOp_3x` (333) |

Nine of 41 (eight by the whelp route, one of them also by the dragon's) -
the coordinator's frame-hash A/B covers exactly those. The trace armed the
catalog's starts, so the states read through tables (`0x57C920..`, op 87 /
88's, the conditions) show only through their callees. The menu panels, the
save slot, ops 87 / 88 and the conditions are fuzz-only: no route opens the
field menu, the save screen or a shop.

## 10. Latent defects (Capcom's, described, not fixed)

- **Unchecked state bytes**: op 88's `+4` of 2 or 3 runs op 87's states
  (visible: the colour ramps the other way), 4 and up jumps to data; op 87's
  past 1 and E9's past 3 jump to data. Ours aborts on data. Nothing shipped
  sets them so, as far as the three ops' own state writes go (each writes
  only its own states).
- **E9's divides**: Start's `(a << 15) / (frames * steps)` faults when the
  frames are 0 - a speed index whose `Field_MoveSpeeds` byte is above 16
  (read past the six: index 8 is `0x40`); Arc's and Kind2's next-step `16 /
  speed` faults on a speed of 0 (indexes 0, 6, 7). Start answers 0 at once for
  a speed of 0, so the second needs the state byte set some other way.
  Ours aborts on both.
- **The icon wheel's sort** swaps each coordinate through a byte register:
  the one moved down keeps its low byte sign-extended. The turned spots stay
  within about 48 units (the tables' values are at most 12 each), so no
  shipped kind shows it.
- **`EventScript_SkipIf`** never passes an FF (or an FD..FF control
  `EventScript_SkipControl` leaves in place): a malformed script hangs, as
  `EventScript_Run`'s controls do ([`event-script.md`](event-script.md)).
- **Tables indexed by bytes unchecked**: the ability title `0x663984[+0xB]`,
  the wheel's 28-byte tables by the kind, `CharacterRecords[operand]` in
  condition 15, `Area_Descriptors[Game_AreaNumber]` in op Ax, the item panel's
  rows past the kept list's 0x80 (read on into `.data`),
  `Field_MoveSpeeds[+0x84]` in F9 and E9.
- **DIV-0029's note on `ebx`**: the entry says the draw mode's texture window
  comes from the caller's `ebx`; the code pushes `ebx` at `0x5769C7` as a
  register save and the window argument is the `push 0` at `0x5769C8` (read
  2026-09-29). What cuts the glyph stays unexplained (the entry's own words).

## 11. For `analysis/calltrace/entries_logic.txt`

Appended to the main checkout's file (2026-09-29): 27 lines - the 41 but
the 14 already listed exactly (`005738A0`, `00573BF0`, `00573F70`,
`00574400`, `00574EC0`, `00575F50`, `00578FA0`, `0057A7C0`, `0057A990`,
`0057AB50`, `0057AD10`, `0057AEC0`, `0057C7E0`, `0057CD40`); smaller lines
for starts listed larger (`005763F0 570` under `759`, `005794D0 12` under
`114`, `005795F0 12` under `149`, `0057C8E0 39` under `455`), which the
consolidation's "duplicates keep the smaller" resolves. Not added (not FO's):
`0057C460`, `0057C480`, `0057C4A0` (`MoveScript_Counter*`, ours, unlisted).

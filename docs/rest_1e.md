# Group R1E: the field actions of party sets 13, 14, 15 and set 16's first two forms

**Status:** MEASURED (2026-10-04) - round fourteen
([`takeover-queue-round14.md`](takeover-queue-round14.md)), wave one, on the
round branch's tip `ba2c3c3`. **47 functions ours** (`src/game/rest_1e.cpp`,
declarations in `src/game/rest_1e.h`, the raw cells in `rest_1e_callees.h`,
shadow name `rest_1e`): the cut's 47 rows for R1E
(`analysis/round14_cut.tsv`), each read to its last instruction with capstone
and fuzzed through the scenario harness in field mode
([`scenario_harness.md`](scenario_harness.md) section 7), used unchanged:
188,000 rounds, **0 mismatches**. 76 controls planted one at a time, all
refused (section 6). 28 state tables named. No recorded route enters any of
the 47 (section 9): fuzz only.

The band is one thing: the field actions of four party sets, reached through
`Field_ActionBySet` / `Field_FormActions` (by the party set) and then through
per-set tables. Every start the cut lists is a function (each has its own
`ret`, none is a case or a shared tail); the band tool found no code the cut
does not list, and no start was dropped, merged or added. The cut's sizes are
the extents plus the nop padding to the next 16-byte boundary; the tool's
extents, read from the code, are the right ones (`band_rows.py --byte-tables`:
"0 extents differ from the cut by code (41 more by padding only)").

## 1. The shape of the band

`Field_ActionState` (ours, `0x52FB60`) calls `Field_ActionBySet[set]` and
`Field_FormActionState` (`0x52F4F0`) calls `Field_FormActions[set]` - the
set being `0x90412C & 0x7F`. For sets 13, 14 and 15 both entries are this
group's dispatchers, which jump through the set's forms table by the
sprite's u16 form word `+0x2C`; a form's dispatcher jumps through its states
table by the state byte `+2`; two forms (set 14's form 1, set 16's form 1)
jump through a two-entry modes table by `+2` and then a mode's states by `+3`.
Set 16's forms tables (`0x660030`, `0x66003C`) are read by R1F's `0x5243D0` /
`0x5243F0`; its forms 0 and 1 are here.

The pattern repeats per set, as the brief warned: the same code at two or
three addresses. A capstone diff of the copies (scratch `r1e/cmp.py`, the
jumps made relative) shows them **instruction for instruction the same** -
constants, item ids and sound ids included - except that each copy of the
resolve and strike states calls its own set's cell helper. Ours writes one body
per shape and three (or two) thin `extern "C"` entries.

| Shape | Copies | Bytes | What it is |
|---|---|--:|---|
| `PartyAction13_Form2Begin` | `0x522760`, `0x5230D0` (set 14 form 2), `0x523550` (set 15 form 1) | 0x1D4 | state 0: turn, then the rise ahead |
| `PartyAction13_Form2Resolve` | `0x522940`, `0x5232B0`, `0x523730` | 0xDB | state 1: countdown, then the cell pickup ahead |
| `PartyAction13_CellPickup` | `0x522A20`, `0x523390`, `0x523810` | 0x11F | the pickup on a cell, al 0 / 1 |
| `PartyAction14_StrikeBegin` | `0x522C00`, `0x523B40` (set 16) | 0x87 | mode 0 state 0 |
| `PartyAction14_Strike` | `0x522C90`, `0x523BD0` | 0x14C | mode 0 state 1 |
| `PartyAction14_StrikeCell` | `0x522E20`, `0x523D20` | 0x188 | the strike on a cell, al 0 / 1 |
| `PartyAction14_Mode1Begin` | `0x523050` | 0x35 | mode 1 state 0 |
| `PartyAction_ProbeBegin` | `0x5239F0` | 0xE7 | state 0 of seven tables (sets 1, 2, 5, 13, 16, ...) |
| `PartyAction_StrikeWait` | `0x522DE0` | 0x38 | state 4 of nine tables |
| `PartyAction_NoAction` | `0x5226D0` | 0xD | form 0 of the action tables of sets 5, 13, 14, 15 |
| dispatchers | 28 (below) | 0x12 / 0x13 | `jmp [table + index * 4]` |

Two of the shapes are copies of code already ours, by capstone diff:
**`Form2Resolve` is `PartyAction5_Form0Resolve` `0x51EAF0`** (`field_hidden`)
but for its callee, and **`CellPickup` is `Field_CellPickup` `0x51EBD0`** but
for one instruction - `mov cl, 0x14` where that has `mov cl, 0xA`: the zenny
bonus is twenty times, not ten. `Form2Begin` is `PartyAction5_Form0Begin`
`0x51E930`'s structure but its steep test differs (below).

## 2. What each function does

"The sprite" is `Sprite_Current`; "a step" a row of `Field_DirectionSteps`
(`0x6697B0`, read in place with the direction byte unmasked, as rest_0a.md
section 1 says every reader does); "the half turn" `(+8 - 1) / 2` as the
originals compute it (`dec eax; cdq; sub eax, edx; sar eax, 1` on the
zero-extended byte: direction 0 gives 0).

- **`PartyAction_NoAction` `0x5226D0`** - `Field_State +0x137 = 0`, the byte
  `Field_FormActionState` waits on and `PartyAction_Finish` clears: the form
  with nothing to do ends the action at once.
- **Dispatchers** - `mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx +
  2]` (or `+3`, or `mov ax, [ecx + 0x2C]`); `jmp [table + eax * 4]`. None bounds
  its index.
- **`PartyAction13_Form2Begin`** (`0x522760` / `0x5230D0` / `0x523550`) - an
  even `+8` is turned one eighth back (`& 7`); while `PartyAction_TargetAhead`
  (R0A) answers 0 it is turned two on, then two back (the sprite re-read after
  each call; an odd `+8` is kept, unmasked). Then the point one step ahead:
  `MapView_GroundAt` there, less the sprite's height word (a 16-bit
  subtraction); `MapView_SlopeAt` there, **its answer unused**, pushed `eax`
  whole - `al` the direction and the upper three bytes the sprite's own address
  (`mov eax, [Sprite_Current]; ...; mov al, [eax + 8]; push eax`). With
  `DamageScratch`'s flag set and that rise, signed, above 0x40:
  `Sprite_EnsureAnimation(half turn + 0x46)` and `+2` one on. Otherwise `+0x2B =
  1`, the side probes in directions 3 and 5 (`PartyAction_SideProbes`' code,
  inline), `Sound_PlayEffect(u16 +0x2C + 0x100)`, `Sprite_EnsureAnimation(half
  turn + 0x42)`, `+0xA = 5`. Then `+0xB = 0` and `+2` one on (so the steep path
  moves `+2` by two, to `PartyAction_Finish`). `PartyAction5_Form0Begin` tests
  the slope's own low word; this one the rise of the ground.
- **`PartyAction13_Form2Resolve`** (`0x522940` / ...) - `+0xA` counted down;
  at 0: the point two steps ahead; the object `Sprite_ObjectAt(point, 0)` finds
  gets bit 0 of its `+0x80` (0..0x1D a `Sprite_Objects` record, 0x1E..0x21
  `Sprite_ObjectsExtra`'s); the set's cell pickup on the point's cell, then -
  while it answers 0 - on the cell one on in x (x with a fraction), then one on
  in z (z with one); `+2` one on. `Sprite_ScriptTickOnce` every time.
- **`PartyAction13_CellPickup`** (`0x522A20` / ...) `(x, z)` -
  `AreaMap_ByteAt(x, z)`: **0xF2** - with `Effect_FindFree` not 0xFF,
  `Effect_SpawnAtCell(0, x, z)`; `Rand & 0xF` of 0xD..0xF gives 2 zenny (5 on
  0xF), times 20 when `Field_InputFlags & 6` and a second `Rand & 3` is 0,
  `Field_GiveZenny`, `Effect_SpawnAtCell(1, x, z)`; `+0xB = 1` (not when no
  object was free). **0xF8** - `Effect_SpawnAtCell(0, x, z)`; item 0x56 of
  category 0's name (`Item_NamePtr`, 16 bytes) into `Text_Records`;
  `Inventory_Add(0, 0x56, 1)`: taken, `Sound_PlayEffect(0x106)` and
  `Msg_OpenSystem(2)`, else `Msg_OpenSystem(3)`; `Field_Request = 2`; `+0xB =
  1`. Both clear the cell (`AreaMap_ClearCell`) and answer 1; any other byte 0.
- **`PartyAction14_StrikeBegin`** (`0x522C00` / `0x523B40`) - the turn as
  `Form2Begin`'s but while `PartyAction_BlockedAhead` answers 0;
  `PartyAction_SideProbes`; `Sprite_EnsureAnimation(half turn + 0x42)`; `+0xB =
  0`, `+0xA = 0xB`, `+3 = 1`.
- **`PartyAction14_Mode1Begin`** `0x523050` - `PartyAction_SideProbes`,
  `Sprite_EnsureAnimation(half turn + 0x42)`, `+0xA = 0xB`, `+3 = 1`.
- **`PartyAction14_Strike`** (`0x522C90` / `0x523BD0`) - with `+0xA` not 0,
  counted down; at 0: `Sound_PlayEffect(u16 +0x2C + 0x100)`; then
  `Field_EffectAhead`: an effect object (0..19) gets the sprite's direction in
  its `+8` and `+0xA = 1`, its index goes into the sprite's `+6`, sound 0x10B,
  and `+3` moves two on (to `PartyAction_StrikeWait` via state 3, R1C's
  `0x51F850`); none - the point two steps ahead, the object there flagged as in
  `Form2Resolve` and sound 0x10B, the strike cell on the point's cell and the
  cells one on across a fraction while it answers 0, `+3` one on.
  `Sprite_ScriptTickOnce` every time.
- **`PartyAction14_StrikeCell`** (`0x522E20` / `0x523D20`) `(x, z)` -
  `AreaMap_ByteAt(x, z)`: **0xF0, 0xF1, 0xF4** - `Effect_SpawnAtCellHigh(0, x,
  z)`, and state 4 too when `Rand & 7` is 6 or 7; sound 0x10B; 1. **0xF6,
  0xF7** - state 0, sound 0x10B, then `Rand & 0xF`: below 7 state 3, item 0x29's
  name into `Text_Records` and `Inventory_Add(0, 0x29, 1)` (taken: sound 0x106,
  message 2; else message 3), `+0xB = 2`; 7..0xB nothing more; 0xC..0xF state 2,
  `Sprite_FlashClut(0)`, `Char_LoseHp(1, Field_State +0x89)` (read after the
  flash), message 0xD9, `+0xB = 1`; then `Field_Request = 2`; 1. Any other byte
  0. (The cell codes are R0A's `PartyAction_BlockedAhead` set: what blocks the
  way is what the strike acts on.)
- **`PartyAction_StrikeWait`** `0x522DE0` - the effect object the sprite's
  `+6` names (zero-extended, unchecked) free (`+0` zero) or in its state 1, and
  `Field_Kind2Hold` 0: `+6 = 0` and `+3` two back.
- **`PartyAction_ProbeBegin`** `0x5239F0` - an even `+8` turned one eighth
  back (no probe); `+0x2B = 1` and the side probes (inline);
  `Sprite_EnsureAnimation((+8 >> 1) + 0x42)` - a byte (`shr cl, 1; add cl,
  0x42`), not the half turn; `+2` one on.

What the cell codes, the items and the effect states are in play is not read
here; the names say what the code does and which table reaches it. "Strike"
names the shape (a sound, an effect object or sprite ahead acted on, a cell
changed), not an action the owner has confirmed.

## 2a. Calling convention, arguments, answers

All 47 are `cdecl`. The 42 states and dispatchers take nothing and answer
nothing a caller reads (they are reached by `jmp` through `.data`, the
dispatcher's caller being `Field_ActionState` / `Field_FormActionState`, which
read nothing back). The five cell helpers take `(x, z)` and answer al 0 / 1:
every one of their 15 `E8` sites (all in this group) tests `al` first.

**The cells' upper halves.** `Form2Resolve` and `Strike` pass the cells as
dwords read 2 bytes into their own stored points (`[esp + 0x12]`, `[esp +
0x1A]`): the upper half of each is stack the function never wrote. Every callee
of the helpers reads the low word only (`AreaMap_ByteAt` sign-extends 16 bits;
`Effect_SpawnAtCell` and `Effect_SpawnAtCellHigh` `movsx` the words;
`AreaMap_ClearCell` per FE2, and field_hidden.md section 3 for the same
pattern), so ours passes `x >> 16` and the fuzz logs the helpers' arguments
masked to 16 bits.

**The helpers' scratch writes.** `CellPickup` keeps the zenny amount in the
low byte of its own z argument's slot, and `StrikeCell` stores the cell byte
there (and pushes that dword as `Effect_SpawnAtCellHigh`'s never-read fourth
argument). That slot is the caller's stack, pushed afresh for each call and
not read again; ours does not reproduce the store (no `.data` byte moves).

## 3. The state tables

28 tables, `[[data]]` in `symbols.toml` (`unsigned long`). **No reader bounds
its index**; each count is the run of code pointers to the next table, read by
hand (the band tool's counts run on across all of `0x65FEC4..0x660018`, as the
addendum warned). Mixed tables: many hold other groups' handlers (R1A..R1D,
R1F), named only by address in the evidence.

| Table | Count | Reader (by) | Entries (ours named) |
|---|--:|---|---|
| `PartyFormAction13_Form1States` `0x65FEC4` | 3 | `0x5226E0` (+2) | R1C's `0x520840`, `0x51FC80`, `0x52F5C0` |
| `PartyAction13_Form1States` `0x65FED0` | 3 | `0x522700` (+2) | `PartyAction_ProbeBegin`, R1B's `0x51DE20`, R1C's `0x520350` |
| `PartyFormAction13_Form2States` `0x65FEDC` | 3 | `0x522720` (+2) | `0x520E90`, `0x51FC80`, `0x52F5C0` |
| `PartyAction13_Form2States` `0x65FEE8` | 3 | `0x522740` (+2) | `_Form2Begin`, `_Form2Resolve`, `PartyAction_Finish` |
| `PartyFormAction13_Forms` `0x65FEF4` | 3 | `0x522B40` (u16 +0x2C) | R1D's `0x5226B0`, `PartyFormAction13_Form1`, `_Form2` |
| `PartyAction13_Forms` `0x65FF00` | 3 | `0x522B60` (u16 +0x2C) | `PartyAction_NoAction`, `PartyAction13_Form1`, `_Form2` |
| `PartyFormAction14_Form0States` `0x65FF0C` | 3 | `0x522B80` (+2) | `0x520840`, `0x51FC80`, `0x52F5C0` |
| `PartyFormAction14_Form1States` `0x65FF18` | 3 | `0x522BA0` (+2) | `0x520840`, `0x51FC80`, `0x437CA0` |
| `PartyAction14_Form1Modes` `0x65FF24` | 2 | `0x522BC0` (+2) | `PartyAction14_Form1Mode0`, `_Form1Mode1` |
| `PartyAction14_Mode0States` `0x65FF2C` | 5 | `0x522BE0` (+3) | `_StrikeBegin`, `_Strike`, R1A's `0x51D440`, R1C's `0x51F850`, `PartyAction_StrikeWait` |
| `PartyAction14_Mode1States` `0x65FF40` | 3 | `0x523030` (+3) | `_Mode1Begin`, R1F's `0x523F10`, R1A's `0x51D6D0` |
| `PartyFormAction14_Form2States` `0x65FF4C` | 3 | `0x523090` (+2) | `0x520E90`, `0x51FC80`, `0x52F5C0` |
| `PartyAction14_Form2States` `0x65FF58` | 3 | `0x5230B0` (+2) | `_Form2Begin`, `_Form2Resolve`, `PartyAction_Finish` |
| `PartyFormAction14_Forms` `0x65FF64` | 3 | `0x5234B0` (u16 +0x2C) | `PartyFormAction14_Form0..2` |
| `PartyAction14_Forms` `0x65FF70` | 3 | `0x5234D0` (u16 +0x2C) | `PartyAction_NoAction`, `PartyAction14_Form1`, `_Form2` |
| `PartyFormAction15_Form0States` `0x65FF7C` | 3 | `0x5234F0` (+2) | `0x520840`, `0x51FC80`, `0x52F5C0` |
| `PartyFormAction15_Form1States` `0x65FF88` | 3 | `0x523510` (+2) | `0x520E90`, `0x51FC80`, `0x52F5C0` |
| `PartyAction15_Form1States` `0x65FF94` | 3 | `0x523530` (+2) | `_Form1Begin`, `_Form1Resolve`, `PartyAction_Finish` |
| `PartyFormAction15_Form2States` `0x65FFA0` | 3 | `0x523930` (+2) | `0x520840`, R1A's `0x51C490`, `0x52F5C0` |
| `PartyAction15_Form2States` `0x65FFAC` | 2 | `0x523950` (+2) | R1F's `0x5252B0`, R1D's `0x521A20` |
| `PartyFormAction15_Forms` `0x65FFB4` | 3 | `0x523970` (u16 +0x2C) | `PartyFormAction15_Form0..2` |
| `PartyAction15_Forms` `0x65FFC0` | 3 | `0x523990` (u16 +0x2C) | `PartyAction_NoAction`, `PartyAction15_Form1`, `_Form2` |
| `PartyFormAction16_Form0States` `0x65FFCC` | 3 | `0x5239B0` (+2) | `0x520840`, `0x51FC80`, `0x52F5C0` |
| `PartyAction16_Form0States` `0x65FFD8` | 3 | `0x5239D0` (+2) | `PartyAction_ProbeBegin`, `0x51DE20`, `0x520350` |
| `PartyFormAction16_Form1States` `0x65FFE4` | 3 | `0x523AE0` (+2) | `0x520840`, `0x51FC80`, `0x437CA0` |
| `PartyAction16_Form1Modes` `0x65FFF0` | 2 | `0x523B00` (+2) | `PartyAction16_Form1Mode0`, `_Form1Mode1` |
| `PartyAction16_Mode0States` `0x65FFF8` | 5 | `0x523B20` (+3) | `_StrikeBegin`, `_Strike`, `0x51D440`, `0x51F850`, `PartyAction_StrikeWait` |
| `PartyAction16_Mode1States` `0x66000C` | 3 | `0x523EB0` (+3) | R1F's `0x523ED0`, `0x523F10`, `0x51D6D0` |

`PartyAction16_Mode1States`' end is the one not fixed by a following reader of
this group: the three dwords after it repeat a form-action states table's
pattern (`0x520E90`, `0x51FC80`, `0x52F5C0`), and set 14's mode-1 table holds
three. The cut's `unit_desc` (`table ... read by ...`) and `label` columns
(`Field_FormActions`, `Field_ActionBySet`, `PartyAction5_Forms`) were the
tables that reach a row, not the row; the names here replace them.

## 4. The fuzz (`rest_1e_fuzz.cpp`)

`scenario_harness::Run` in field mode, **4,000 rounds per function**: the 42
states and dispatchers as `Shape::kSprite` (on `Sprite_Current`, one of the
first four sprite records), the five cell helpers as `Shape::kCall` with
`ret_mask` 0xFF. The 28 tables are swapped for recorders; each dispatcher's
index is drawn below its own table's length (`kIndex`; the harness's
`sprite_span` is left 0). `BOF3X_R1E_ONLY=<name>` runs the clones whose name
contains it.

**The callees** (the group's listing, registered before the standard rows):

| Callee | Masks | Answers |
|---|---|---|
| the five cell helpers (internal) | 16 bits each | `kFlag` |
| `PartyAction_TargetAhead`, `_BlockedAhead` (R0A) | - | `kFlag` |
| `PartyAction_SideProbes` (R0A) | - | writes `+0x2B` 0 a third of the time, else 1 |
| `Effect_SpawnAtCellHigh` (R0A), `Effect_SpawnAtCell` | byte, 16, 16 | garbage |
| `Field_GiveZenny` | whole | garbage |
| `AreaMap_ClearCell` | whole, whole (the helpers pass the dwords they were given) | garbage |
| `AreaMap_ByteAt` | 16, 16 | al 0xF2 / 0xF8 / 0xF0 / 0xF1 / 0xF4 / 0xF6 / 0xF7 (weighted), neighbours, or any byte |
| `Sprite_ObjectAt` | whole x3 | al 0xFF half the time, else 0..0x21 |
| `Field_EffectAhead` | - | al 0xFF half the time, else 0..19 |
| `MapView_SlopeAt` | whole x3 (the Begin states push `eax` whole) | the scratch flag and a low word around 0x40 |
| `MapView_GroundAt` | whole x2 | a low word about the height and the height + 0x40 |
| `Sprite_FlashClut` | byte | garbage; `Field_State +0x138 \|= 8` (the real one's write) and `+0x89` moved half the time (section 6, C72) |
| `Rand` | - | half the time a low nibble at the helpers' boundaries |

The standard rows serve the rest (`Sound_PlayEffect`, `Msg_OpenSystem`,
`Inventory_Add`, `Item_NamePtr` into the text buffer,
`Sprite_EnsureAnimation`, `Sprite_ScriptTickOnce`, `Effect_FindFree`,
`Char_LoseHp`). Regions beyond field mode's: `Field_DirectionSteps` (0x40) and
`Text_Records`' first 16 bytes. 23,676 bytes, 40 regions.

**The seeds**: `Field_DirectionSteps` half the time in the exe's shape;
the sprite's direction 0..7 two times in three (else 8, 9, 15, 0x80, 0xFF or
random), its x / z a cell at 0, small, the s16 / u16 limits or random with a
fraction 0 (often), a half, a quarter, 1, 0xFFFF; its height around 0, the
limits and 0x7FC0 / 0xFFC0 (where + 0x40 crosses); `+0xA` 1 (often), 0, 2,
0xFF; `+0xB`, `+0x2B`, `+6` (0..19), `+0x2C`; the effect records' `+0` and
`+1`; `Field_Kind2Hold` 0 three times in five; `Field_InputFlags` 0, 2, 4, 6
or any. The helpers' arguments: a cell 0..0x7F mostly, else 0, 0x7FFF,
0x8000, 0xFFFF, under random upper halves. **The disturbance** (from its hash
only): the sprite's direction, x / z, height, `+0x2B`, `+0xA`, `+0x2C`; the
scratch flag; an effect record's `+0` / `+1`; `Field_Kind2Hold`; `Field_State
+0x89`; a dword of the steps table; `Field_InputFlags`.

**Result** (2026-10-04, this worktree, `BOF3X_SELFTEST_ONLY=1
BOF3X_SHADOW=rest_1e`, exit 0): 188,000 rounds over 47 functions, 325,097
calls to the stand-ins, **0 mismatches**; every table entry's recorder was
reached (1,300..12,000 each), the helpers 1,169..2,479, `Field_GiveZenny`
378, `Char_LoseHp` 456. **`BOF3X_SHADOW='*'`** after the rebinding
(2026-10-04, this worktree): exit 0, `inject: 8702 ours, 0 left original`,
1,020 self-test lines of 0 mismatches and no `MISMATCH` line (among them
`rest_0a`, `field_hidden` and `rest_1e`); the same with `BOF3X_WIDE=1`: exit
0, 1,020, 8,702 ours. Each passed on its first run.

**A trap paid for: the toolchain dropped the direction's upper bytes.** The
first run mismatched every round of the three Begin copies (12,000) on
`MapView_SlopeAt`'s third argument: ours pushed the direction byte alone.
The IR (`-emit-llvm`) keeps `(ptr & 0xFFFFFF00) | ptr[8]` (an `or disjoint`),
but llvm-mingw's i686 back end emitted `movzbl 8(%ebx), %ebx; push %ebx` -
the pointer bits lost. Reading the pointer again as a dword gave the same code.
Ours now passes the upper bits through an empty `asm volatile` (one line,
commented): 0 mismatches. Whether the callee could see the difference in play
is narrow (`AreaMap_Slope` reads the dword's upper bytes only for a direction
of 8 or more), but the clone compares it and so does ours now. Worth knowing
for any group that builds a register value from a pointer's bits.

## 5. Divergence, aborts, latent defects

**No divergence.** `DIVERGENCE.md`, `cheats.cpp` and `widescreen.cpp` name no
byte in `0x5226D0..0x523EC2` (grep, 2026-10-04); no full-frame fill is drawn
here. **Nothing needs a ledger entry**: no function reads memory it never
wrote into what is drawn or decided.

Where ours aborts (the project's rule; none reached by ordinary play as far
as the code shows):

- **A dispatcher's index past its table** (all 28): the originals jump
  through whatever dword follows - the next table's entries. Ours aborts with
  the table, the index and its length. In play the bytes are written by the
  states themselves (`+2` one or two on to the table's last entry,
  `PartyAction_Finish` ending the action) and the form word by the party set's
  set-up; nothing here moves them past.
- **`Sprite_ObjectAt`'s answer past 0x21** (`Form2Resolve`, `Strike`): the
  originals set bit 0 of the byte at `index * 0xA4 + 0x7DEF00` (signed: a
  negative byte reaches below `Sprite_Objects`). The real one answers 0..0x21
  or 0xFF (its evidence).
- **`Field_EffectAhead`'s answer past 19** (`Strike`, `movsx`), and **the
  sprite's `+6` past 19** (`StrikeWait`, zero-extended): an `Effect_Objects`
  record read or written unchecked. The first answers 0..19 or 0xFF; the second
  is written only by `Strike` from that answer.

Latent defects described, not fixed:

- **The direction is not masked** where it is odd (all the turns, every
  step): as rest_0a.md section 5 - read in place, reproduced, the seeds cover
  it.
- **`Form2Begin` hands `MapView_SlopeAt` its own address as the direction's
  upper bytes** and ignores the slope's answer; for a direction of 8 or more
  `AreaMap_Slope` reads those bytes. The call's only effect the state reads is
  `DamageScratch`'s flag. Reproduced.
- **`CellPickup`'s 0xF2 path with no free effect object** clears the cell and
  answers 1 without setting `+0xB` or giving anything: the pickup is lost.
  `Field_CellPickup` has the same path. Reproduced; whether play reaches it (20
  effect objects in use) is not established.

## 6. Controls

`r1e/controls.py` (scratch): each plant replaces a string that occurs exactly
once in `rest_1e.cpp`, rebuilds, runs the self-test on the clones it names
(`BOF3X_R1E_ONLY`; a shared body is planted once and run on one copy),
restores the file; one rebuild at the end. **76 planted, 76 refused.** The
count is the rounds that mismatched (of 4,000 a clone; "Strike" names three
clones, 12,000).

| # | Run on | Plant | Refused |
|---|---|---|--:|
| C01 | `PartyAction13_Form1` | a dispatcher's index counted from the table's end | 2,665 |
| C02 | `PartyAction13_ByForm` | the form word & 1 | 1,345 |
| C03 | `PartyAction14_Form1Mode0` | the +3 dispatchers read +2 | 3,226 |
| C04 | `PartyAction14_Form1Mode0` | set 14's mode-0 table for set 16's | 1,590 |
| C05 | `PartyAction13_ByForm` | set 13's forms table for set 14's | 2,680 |
| C06 | `PartyAction_NoAction` | +0x136 cleared, not +0x137 | 4,000 |
| C07 | `PartyAction_ProbeBegin` | the turn one on, not back | 1,941 |
| C08 | `PartyAction_ProbeBegin` | animation + 0x43 | 4,000 |
| C09 | `PartyAction_ProbeBegin` | +3 on, not +2 | 4,000 |
| C10 | `PartyAction_ProbeBegin` | steep from 0x40, not above it | 403 |
| C11 | `PartyAction_ProbeBegin` | direction 7 pushed for 5 | 4,000 |
| C12 | `PartyAction_ProbeBegin` | row 3 read for the second probe | 3,925 |
| C13 | `PartyAction_ProbeBegin` | cleared when level too | 115 |
| C14 | `PartyAction_ProbeBegin` | Sprite_Current not re-read after the ground call | 66 |
| C15 | `PartyAction_ProbeBegin` | +0x2B = 2 | 3,289 |
| C16 | `PartyAction13_Form2Begin` | the odd direction turned, the even kept | 4,000 |
| C17 | `PartyAction13_Form2Begin` | the second turn three on | 659 |
| C18 | `PartyAction13_Form2Begin` | the last turn masked & 0xF | 107 |
| C19 | `PartyAction13_Form2Begin` | the second probe's answer ignored | 426 |
| C20 | `PartyAction13_Form2Begin` | steep from 0x40 | 216 |
| C21 | `PartyAction13_Form2Begin` | the rise height - ground | 1,493 |
| C22 | `PartyAction13_Form2Begin` | the direction byte alone pushed | 4,000 |
| C23 | `PartyAction13_Form2Begin` | the steep animation + 0x45 | 818 |
| C24 | `PartyAction13_Form2Begin` | +0xA = 4 | 3,182 |
| C25 | `PartyAction13_Form2Begin` | +0xB = 1 | 4,000 |
| C26 | `PartyAction13_Form2Begin` | the steep path +2 once | 818 |
| C27 | `PartyAction13_Form2Begin` | the sound + 0x101 | 3,182 |
| C28 | `PartyAction13_Form2Begin` | the half turn by an arithmetic shift (direction 0 gives -1) | 12 |
| C29 | `PartyAction13_Form2Begin` | x two steps ahead | 3,318 |
| C30 | `PartyAction13_Form2Begin` | Sprite_Current not re-read after the ground call | 137 |
| C31 | `PartyAction13_Form2Resolve` | resolved at 1 | 2,294 |
| C32 | `PartyAction13_Form2Resolve` | bit 1 of a sprite's +0x80 | 593 |
| C33 | `PartyAction13_Form2Resolve` | +3 on, not +2 | 1,696 |
| C34 | `PartyAction13_Form2Resolve` | x's fraction tested & 0xFFFE | 42 |
| C35 | `PartyAction13_Form2Resolve` | the z probe one on in x too | 216 |
| C36 | `PartyAction13_Form2Resolve` | the x probe's answer ignored | 204 |
| C37 | `PartyAction13_Form2Resolve` | x stepped by the z row | 1,490 |
| C38 | `PartyAction13_Form2Resolve` | set 14's pickup called | 1,696 |
| C39 | `PartyAction13_CellPickup` | the bonus x 10 (Field_CellPickup's) | 32 |
| C40 | `PartyAction13_CellPickup` | zenny from 0xC | 43 |
| C41 | `PartyAction13_CellPickup` | 5 zenny from 0xE | 45 |
| C42 | `PartyAction13_CellPickup` | the input flags & 2 | 27 |
| C43 | `PartyAction13_CellPickup` | +0xB = 1 with no object free too | 18 |
| C44 | `PartyAction13_CellPickup` | two items added | 400 |
| C45 | `PartyAction13_CellPickup` | 12 bytes of the name copied | 400 |
| C46 | `PartyAction13_CellPickup` | Field_Request = 1 | 381 |
| C47 | `PartyAction13_CellPickup` | the cell cleared at (z, x) | 1,004 |
| C48 | `PartyAction13_CellPickup` | message 4 when not taken | 118 |
| C49 | `PartyAction14_StrikeBegin` | +0xA = 0xA | 4,000 |
| C50 | `PartyAction14_StrikeBegin` | +0xB not cleared | 3,067 |
| C51 | `PartyAction14_StrikeBegin` | the turn probed by TargetAhead | 1,941 |
| C52 | `PartyAction14_Mode1Begin` | +3 = 2 | 4,000 |
| C53 | `PartyAction14_Mode1Begin` | the probes after the animation | 4,000 |
| C54 | `PartyAction14_Strike` | the effect object's +0xA = 2 | 858 of 12,000 |
| C55 | `PartyAction14_Strike` | the direction into the object's +9 | 858 of 12,000 |
| C56 | `PartyAction14_Strike` | the index into +7 | 858 of 12,000 |
| C57 | `PartyAction14_Strike` | the effect path +3 once | 849 of 12,000 |
| C58 | `PartyAction14_Strike` | the strike sound with no object too | 418 of 12,000 |
| C59 | `PartyAction14_Strike` | set 16's strike cell called | 821 of 12,000 |
| C60 | `PartyAction14_Strike` | the sound + 0x200 | 1,679 of 12,000 |
| C61 | `PartyAction14_Strike` | a countdown of 1 skipped | 1,679 of 12,000 |
| C62 | `PartyAction_StrikeWait` | state 2 awaited | 1,080 |
| C63 | `PartyAction_StrikeWait` | Field_Kind2Hold not tested | 888 |
| C64 | `PartyAction_StrikeWait` | +3 one back | 1,353 |
| C65 | `PartyAction_StrikeWait` | free and in state 1 both wanted | 1,114 |
| C66 | `PartyAction14_StrikeCell` | 0xF5 for 0xF4 | 408 |
| C67 | `PartyAction14_StrikeCell` | the second spawn from 5 | 90 |
| C68 | `PartyAction14_StrikeCell` | the item below 8 | 58 |
| C69 | `PartyAction14_StrikeCell` | the hurt from 0xB | 59 |
| C70 | `PartyAction14_StrikeCell` | +0xB = 1 on the item | 337 |
| C71 | `PartyAction14_StrikeCell` | 2 HP lost | 227 |
| C72 | `PartyAction14_StrikeCell` | the member read before the flash | 108 |
| C73 | `PartyAction14_StrikeCell` | the item's effect state 1 | 337 |
| C74 | `PartyAction14_StrikeCell` | message 0xDA | 227 |
| C75 | `PartyAction14_StrikeCell` | 0xF8 for 0xF7 | 804 |
| C76 | `PartyAction14_StrikeCell` | Field_Request = 3 | 781 |
| C77 | `PartyAction13_Form2Begin` | the steep pose's direction held from before the ground and slope calls (case 0) | 64 |
| C78 | `PartyAction13_Form2Begin` | the slope's direction byte held from before the ground call (case 0) | 142 |
| C79 | `PartyAction_ProbeBegin` | the pose's direction held from before the side probes (case 0) | 339 |
| C80 | `PartyAction14_Strike` | the effect object's direction held from before the sound and the effect probe (case 0) | 89 of 12,000 |
| C81 | `PartyAction14_StrikeBegin` | the pose's direction held from before `PartyAction_SideProbes` (case 0) | 142 |
| C82 | `PartyAction_ProbeBegin` | the side probe's clear of +0x2B a read-modify-write (`&= 0xFE`; case 3) | 76 |
| C83 | `PartyAction_ProbeBegin` | the scratch flag tested again after the ground call (case 6) | 2 |
| C84 | `PartyAction14_StrikeCell` | the member read before the hurt's spawn and the flash (case 9) | 108 |

**The first run left C72 unrefused** (the strike cell's `Field_State +0x89`
read before `Sprite_FlashClut`, not after): the harness's disturbance moved
that byte about one call in three hundred. `Sprite_FlashClut`'s stand-in now
does what the real one does to `Field_State` (`+0x138 |= 8`) and, in the
disturbance's role, moves `+0x89` half the time; C72 was then refused (108)
and the group's run stayed at 0 mismatches. The thinnest of the rest (under
50): C28 (12: the half turn by an arithmetic shift differs only at direction 0
after the turns), C34, C39..C43 (the 0xF2 path behind `Effect_FindFree` and a
`Rand` nibble, the bonus behind two more conditions).

**Under the repaired disturbance, round fourteen's review item 1
(2026-10-05).** The group's `Disturb` switched on `h % 12`, and the harness
hands it only hashes that are not a multiple of 3, so its cases 0 (the
direction), 3 (`+0x2B`), 6 (the scratch flag) and 9 (`Field_State +0x89`)
never ran; `b9dfe34` draws the case from `sh::DisturbCase(h, 12)`. That is
also why the harness moved `+0x89` "about one call in three hundred" above:
only its own row did. Re-run at `451edeb` (`r1e/controls.py` copied, its
anchors unchanged - all 76 still occur once): **76 planted, 76 refused**;
the unplanted run 188,000 rounds, 0 mismatches. Ten counts moved by 1 to 34
(C07 1,924, C13 114, C15 3,255, C17 657, C18 106, C23 / C26 815, C24 / C27
3,185, C28 14), the rest equal. New controls C77..C84 above, one or more on
each cell a formerly dead case moves; **8 planted, 8 refused**. Each was run
again with its case switched off in the fuzz (scratch only), to see what
refuses it without the case: the direction's five (C77..C81) are still
refused (61, 136, 312, 81 of 12,000, 136) - mostly by the harness moving
`Sprite_Current` among the sprite records, so the held byte is another
sprite's; C82 still 70 (the same move; **no function of the group reads
`+0x2B`**, only writes it, so case 3 can test only that the clear is a plain
store, which C82 does); **C83 is refused by case 6 alone** (2 with it, 0
without: every other read of the flag directly follows `MapView_SlopeAt`,
whose stand-in writes it, so the case is otherwise noise here - and the 2 are
thin); C84 still 108 with case 9 off (`Sprite_FlashClut`'s stand-in moves
`+0x89` half the time), and with that stand-in's move off instead, case 9
alone refuses it once (1 of 4,000). The fuzz is unchanged.

## 7. Calls across groups

- **Out**: 23 `E8` sites into R0A (merged, called by name through
  `game/rest_0a.h`: `PartyAction_TargetAhead` 6, `_BlockedAhead` 4,
  `_SideProbes` 3, `Effect_SpawnAtCellHigh` 10). No call to a group of this
  round that is not merged; no raw-address call at all (`rest_1e_callees.h`
  holds cells and ids only).
- **Through the tables** (not calls ours makes): the dispatchers jump to
  R1A's `0x51C490`, `0x51D440`, `0x51D6D0`; R1B's `0x51DE20`; R1C's
  `0x51F850`, `0x51FC80`, `0x520350`, `0x520840`; R1D's `0x520E90`,
  `0x5226B0`, `0x521A20`; R1F's `0x523ED0`, `0x523F10`, `0x5252B0`; and ours
  `PartyAction_ScriptEnd` `0x52F5C0`, `BossOp_ScriptTick` `0x437CA0`,
  `PartyAction_Finish` `0x51DA30`. The fuzz swaps the cells for recorders; in
  the game each is whatever is injected at that address.
- **In**: no `E8` or `E9` from outside the group reaches any of the 47
  (`band_rows.py --byte-tables`). They are reached through `.data`: the
  sets' tables above (this group's), `Field_ActionBySet` / `Field_FormActions`
  entries 13..15 (ours' readers), and other groups' tables -
  `PartyAction_NoAction` from `PartyAction5_Forms` (field_hidden) and cells
  `0x65FCE8`, `0x65FE50`, `0x65FE5C`, `0x65FEA8`, `0x65FEB0`, `0x65FEB4`;
  `PartyAction_StrikeWait` from `0x65FAB8`, `0x65FB8C`, `0x65FBF0`, `0x65FC78`,
  `0x65FD84`, `0x65FE84`, `0x6600C8`; `PartyAction_ProbeBegin` from
  `0x65FA44`, `0x65FB18`, `0x65FC48`, `0x65FD10`, `0x660054`; set 16's forms
  tables `0x660030` / `0x66003C` (R1F's readers) hold `0x5239B0`, `0x523AE0`,
  `0x5239D0`, `0x523B00`. Nothing to rebind: a table cell holds the address,
  which `Inject` patches.

## 8. The rebinding

`band_rows.py --refs --group R1E`: one raw reference to one of the 47 in
`src/game` - `field_hidden.cpp`'s comment on `PartyAction5_ByForm` listing
`PartyAction5_Forms`' entries (`0x5226D0`), now `PartyAction_NoAction` (the one
line). No harness row (`scenario_harness*.cpp`, `boss_harness*.cpp`) and no
`_callees.h` names any of the 47 (grep), so none broke. `symbols.toml`'s
`PartyAction5_Forms` evidence keeps `0x5226D0` (a read of that table, as
written).

## 9. The live route

The catalog's reach columns (`analysis/remaining_catalog.tsv`: attract, shop,
worldmap, combat) and the cut's `reach` are empty for all 47. No first-call or
counted trace under `analysis/calltrace` names any of them (grep: only the
`entries*` lists). **Fuzz only.** Which character sets 13..16 are and when the
leader takes those actions is not read here; the coordinator's state hash
covers whatever a route reaches after the merge.

## 10. For `analysis/calltrace/entries_logic.txt`

Appended to the main checkout's file (2026-10-04): 42 lines, one per function
with the extent read here. Five entries already had a line with the same
address and were left (no duplicates): `0x522E20 188` (the same extent), and
the hosts `0x522A20 3F8`, `0x523390 47B`, `0x523810 50C`, `0x523D20 58B`,
whose extents cover the hidden starts after them - which now have lines of
their own.

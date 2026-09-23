# Stats and inventory: the stat recompute, the inventory, the item tables

**Status:** IN PROGRESS (2026-09-23) - twenty functions ours
(`src/game/char_stats.cpp`, shadow name `char_stats`), each fuzzed headless
against a copy of Capcom's with every call re-aimed at a recorder;
55 negative controls, 54 refused by a count and one by a fault (with a
counting twin). **Not yet through the live check** - the shop A/B
(`analysis/validate_shop.sh`) runs centrally after the merge (section 7).

Group W of the sixth parallel round
([`takeover-queue-round6.md`](takeover-queue-round6.md)): the eighteen
functions of `0x5903F0..0x591CAB` that the owner's shop route reaches, and
two next to them. They are the boot EXE's stat and inventory helpers on the
PSX too (`0x80164EC8..0x80167058`), in the same order. No divergence: every
function is a faithful replacement, and no `DIVERGENCE.md` entry is owed.
The helpers group P left Capcom's (`Char_RecalcStats`, `Inventory_Add`,
[`item-use.md`](item-use.md) section 4) are among them.

## 1. What is ours, and the extents

Bytes are each body to its last instruction and its jump tables (capstone,
2026-09-23) - the figure for `analysis/calltrace/entries_logic.txt`. PSX
twins from `analysis/pairs_propagated.json` and read side by side in
capstone MIPS from the boot EXE (`SLPS_009.90`) where the table says so.

| Function | Entry | Bytes | PSX twin | Does |
|---|---|--:|---|---|
| `Menu_DrawIcon` | `0x5903F0` | 0x1D2 | `0x80164EC8` (re-laid) | an icon quad, section 5 |
| `Menu_DrawHand` | `0x5905D0` | 0x8C | `0x801651B4`, read | the pointing hand, section 5 |
| `Char_RecalcStats` | `0x590660` | 0x1A0 | `0x80165434`, read | section 2 |
| `Char_ApplyTraits` | `0x590800` | 0x154 | `0x80165290`, read | section 2 |
| `Equip_PreviewSlot` | `0x590960` | 0x144 | none | section 3 |
| `Equip_PreviewSet` | `0x590AB0` | 0xF8 | none | section 3 (not on the queue) |
| `Inventory_Add` | `0x590BB0` | 0xDE | `0x80165AA4`, read | section 4 |
| `Stat_AddResist` | `0x590EE0` | 0x4A | `0x80165FE4`, read | section 2 (not on the queue) |
| `Stat_AddCap100` | `0x590F30` | 0x23 | `0x80166084`, read | section 2 |
| `Char_ApplyWeapon` | `0x590FC0` | 0x1C9 | `0x80166150`, read | section 2 |
| `Char_ApplyArmour` | `0x591190` | 0x2F7 | `0x801662CC`, read (head) | section 2 |
| `Char_ApplyAccessories` | `0x591490` | 0x1E8 | `0x8016651C`, read (head) | section 2 |
| `Item_NamePtr` | `0x591680` | 0x98 | `0x80166720` (the sibling's name) | section 4 |
| `Item_IconKind` | `0x591720` | 0x76 | `0x801667D4` | section 4 |
| `Item_EquipMask` | `0x5917A0` | 0x70 | `0x80166880` | section 4 |
| `KeyItem_Has` | `0x5918E0` | 0x1D | `0x80166AD0` | section 4 |
| `TextRecord_Set` | `0x591940` | 0x69 | `0x80166B9C` | section 4 |
| `Inventory_Count` | `0x5919B0` | 0xC5 | `0x80166C1C`, read (head) | section 4 |
| `Inventory_CountUsed` | `0x591A80` | 0x3C | none recorded | section 4 |
| `Item_Price` | `0x591C20` | 0x9C | `0x80167058` | section 4 |

**Two not on the queue, taken:** `Stat_AddResist` `0x590EE0` - all 60 of
its call sites (E8 scan) are in `Char_RecalcStats` and its passes, and the
shop route never reached it because no member there has a resistance
effect; and `Equip_PreviewSet` `0x590AB0`, `Equip_PreviewSlot`'s twin with
all six equipment bytes from an array (two call sites, `0x57502D`,
`0x59D7CF`, unreached). **No case labels posed as functions** in this list,
and no pointer-reached function turned up.

**For `entries_logic.txt`** - six sizes there are wrong (`pe_funcs.py` ran
on through the next function or kept padding): `00590960 149` is `144`,
`00590FC0 1CE` is `1C9`, `00591490 1E9` is `1E8`, `00591680 9D` is `98`,
`005917A0 FD` is `70`, `00591C20 185` is `9C` (six lines). The two taken
beyond the queue, `00590EE0 4A` and `00590AB0 F8`, are listed already with
the right sizes, as are the other twelve. Three real functions
are missing from the list, none ours: `0x591810` (0x88 bytes with its
table: `Item_EquipMask`'s sibling on byte `+0x11`, 18 call sites),
`0x591B60` (0x5D: the sibling's `Inventory_Remove` `0x80166F30`) and
`0x591CC0` (after `Item_Price`: adds a member to the party lists).

## 2. The stat recompute

A persistent character record is `CharacterRecords` `0x903A70` + `0xA4` x
id; every offset is the PSX record's + 4 ([`item-use.md`](item-use.md)
section 2). The fields read here: `+0x0B` bit 0 (counted by
`Inventory_Count`), the equipment bytes `+0x12` (weapon), `+0x13..+0x15`
(armour), `+0x16..+0x17` (accessories), HP `+0x18`, the halving bits
`+0x1D`, the max-HP scale `+0x1E`, the trait byte `+0x1F`; the effective
block `+0x20..+0x3F` - max HP `+0x20`, ATK `+0x24`, DEF `+0x26`, AGI `+0x28`,
INT `+0x2A`, a word `+0x2C`, the byte `+0x2E` (`Stat_AddCap99`), nine
resistance bytes `+0x2F..+0x37`, five percentage bytes `+0x38..+0x3C` - and
its base copy `+0x40..+0x5F`.

`Char_RecalcStats(record)` `0x590660`:

1. The 32 bytes at `+0x40` copied over `+0x20`.
2. The weapon, armour, accessory and trait passes, in that order.
3. Max HP `+0x20` plus `(scale * base max HP + 5) / -10` - the `imul
   0x99999999` idiom, a truncating division by -10; the PSX subtracts
   `/ 10`, the same number - and HP cut to it (unsigned).
4. For each bit of `+0x1D`, `Stat_AddResist(+0x2F, +0x30, +0x31, +0x32,
   +0x33, +0x35, +0x36, +0x37, 2)` - bit 5 is `+0x35`, and `+0x34` has no
   bit, on the PSX too. The bits are read again for each.
5. If the record is one of the eight persistent ones (the loop does not stop
   at the match), `Stat_AddCap100(+0x38 + j, roster bonus)` for each
   non-zero byte `+0x38 + j`, the bonus the byte `0x903640 + 5 * i + j` (PSX
   `0x80148668`). A preview's copy or a battle working copy gets none.

`Char_ApplyTraits` `0x590800`: the trait byte `+0x1F` (`0xFF`: none) picks
a pointer from `0x667548` (null: none) to (kind, amount) byte pairs ended by
kind `0xE`: kinds 0..8 `Stat_AddResist(+0x2F + kind)`, 9..13
`Stat_AddCap100(+0x2F + kind)`, a kind above 13 skipped. What the lists
mean is not read; the name is a guess (`hypothesis`).

`Char_ApplyWeapon` `0x590FC0`: ATK plus the weapon's byte `+0x16`, AGI less
its byte `+0x14` (both `Stat_AddClamped`, `NameTable_Weapons` stride 0x1C),
then by the weapon id - read once, before the calls:

| Weapon ids | Effect |
|---|---|
| `0x0D`, `0x43` | `Stat_AddCap100(+0x3A, 8)` |
| `0x15` | DEF, AGI, INT + 5 |
| `0x1A`, `0x44` | `Stat_AddResist(+0x37, 3)` |
| `0x20` | INT + 3 |
| `0x25` | INT + 5 |
| `0x27` | `Stat_AddResist(+0x37, 1)` |
| `0x28` | INT + 10, `Stat_AddResist(+0x37, 1)`, `(+0x36, 1)` |
| `0x34` | `Stat_AddCap100(+0x3C, 10)` |
| `0x41` | `Stat_AddCap100(+0x38, 5)`, `(+0x3A, 4)` |
| `0x4D` | `Stat_AddCap100(+0x3C, 30)` |

`Char_ApplyArmour` `0x591190`: the three armour ids copied first; for each,
DEF plus its byte `+0x14`, AGI less its byte `+0x13` (`NameTable_Armour`
stride 0x1A), then (resist = `Stat_AddResist`):

| Armour ids | Effect |
|---|---|
| `0x0C`, `0x2D`, `0x39` | resist `+0x2F` by 2 |
| `0x0F` | resist `+0x2F` by 0 (sets 7), `+0x30` by -1 |
| `0x11` | resist `+0x31` 2, `+0x32` 4, `+0x33` 2 |
| `0x13` | resist `+0x34` -3, then `+0x37`, `+0x36`, `+0x35` by 0 |
| `0x14`, `0x3B` | resist `+0x30` 2 |
| `0x16` | INT + 5 |
| `0x18`, `0x1D`, `0x2F` | resist `+0x37`, `+0x35`, `+0x36` by 2 |
| `0x19` | AGI + 5 |
| `0x1B` | resist `+0x35` by 0 |
| `0x1C`, `0x42` | ATK + 10 |
| `0x1E`, `0x31`, `0x41` | resist `+0x2F` by 0 |
| `0x20` | resist `+0x34` 1, then `+0x37`, `+0x35`, `+0x36` by 2 |
| `0x28` | the word `+0x2C` + 1 |
| `0x2C`, `0x3C` | ATK + 5 |
| `0x30` | resist `+0x37` by 0 |
| `0x3E` | resist `+0x36` 2 |
| `0x40` | resist `+0x35` 2, INT + 5 |

`Char_ApplyAccessories` `0x591490`: the two accessory ids copied first; for
each, AGI less its byte `+0x13` (`NameTable_Accessories` stride 0x18), then:
1 ATK + 10; 2 DEF + 5; 3 AGI + 10; 4 INT + 30; 5 `Stat_AddCap99(+0x2E, 10)`;
`0xA` resist `+0x30` by 0; `0xB` `+0x2F` by 0; `0xC` `+0x31` by 0; `0xD`
`+0x37` 1; `0xE` `+0x37` 3; `0xF` `+0x36` 1; `0x10` `+0x36` 3; `0x11`
`+0x35` 3; `0x12` `Stat_AddCap100(+0x38, 20)`; `0x13`
`Stat_AddCap100(+0x3C, 10)`; `0x17` every resistance `+0x2F..+0x37` by 2.

Which items those ids are is in the tables, not read here (game facts are
the owner's).

The two byte steps: `Stat_AddResist(level, step)` `0x590EE0` - the step's
low byte as s8; up: refused (0) at 6 or more, else add and cap at 6; down:
refused at 6 or more too, else add and floor at 0 as s8; 0: refused at 7 or
more, else set 7. `Stat_AddCap100(value, n)` `0x590F30` - refused at
exactly 100, else add the low byte, 0 if negative as s8, 100 if above. Both
answer 1 when they wrote. That the nine bytes are resistances with 7 a
special level is a hypothesis from the shape.

## 3. The equipment previews

`Equip_PreviewSlot(id, slot, item, marks, values)` `0x590960` (the
equipment screen, `0x5754FD`, 1,407 calls on the shop route): the
persistent record copied to the stack, equipment byte `+0x12 + slot` set to
the item (slot's low byte 0..5; above 5 nothing is set), `Char_RecalcStats`
on the copy - which gets no roster bonus, being no persistent record - then
for ATK, DEF, INT, AGI in that order (the menu's column order): `marks[k]`
0, `values[k]` the copy's word, `marks[k]` 3 if the record's is lower
(better), 2 if higher. `Equip_PreviewSet(id, set, marks, values)`
`0x590AB0` takes all six bytes from `set`.

## 4. The inventory and the item tables

The inventory is one id list and one count list of 128 bytes per category,
through two pointer tables indexed by the category's low byte without a
bound: ids `0x656B00` (`0x904154 + 0x80 * cat` for 0..3, then the key items
`0x904554`), counts `0x656B14` (`0x904354 + 0x80 * cat`, and 0 for
category 4). Key items are 32 bytes (`KeyItem_Has`, `Inventory_CountUsed`),
followed by the 128-byte list `0x590C90` fills (the sibling's
`AbilityList_Add`, `0x904574`).

- `Inventory_Add(category, item, count)` `0x590BB0`: 0 for an item or count
  of 0; `DamageScratch` (the PSX scratchpad's first byte, `0x903850`) 0;
  outside category 4, an existing stack takes the count - above 99 it is 99
  and the answer 0, else 1; otherwise the first slot with id 0 (or, outside
  category 4, count 0) takes the item and count, `DamageScratch` 1, answer
  1; no free slot, 0. **The category-4 branch is the PC's own**: the PSX
  `0x80165AA4` has none and treats every category alike. D34.
- `Inventory_Count(category, item, equipped)` `0x5919B0` answers a word:
  with `equipped`'s low byte 0, the count of the item's first stack (0 if
  none) - D35 for category 4; otherwise how many of the eight records
  with bit 0 of `+0x0B` wear it (category 1 the weapon, 2 the armour, 3 the
  accessories, any other none), category 3 also counting the byte
  `0x904130` once and adding the byte `0x90412F` when `0x90412E` is the
  item. What those three bytes are is not read.
- `Inventory_CountUsed(category)` `0x591A80`: the non-zero ids, 128 long
  below category 4, else 32.
- `KeyItem_Has(item)` `0x5918E0`: the item among the 32 key-item bytes.

The item tables (`NameTable_*`, symbols.toml), by category 1 weapons, 2
armour, 3 accessories, 4 key items, any other the consumables:
`Item_NamePtr` `0x591680` the record (its first 16 bytes the name);
`Item_Price` `0x591C20` the word at weapons `+0x18`, armour `+0x16`,
accessories `+0x14`, key items `+0x10`, consumables `+0x12` - the shop
screens call it 523 times on the route, hence the name (hypothesis; the
`NameTable_Consumables` note puts the price at `+0x14`, which nothing here
reads); `Item_IconKind` `0x591720` the low nibble of the equipment's
`+0x12` or the consumables' `+0x11` - **key items read the consumables'
table** (category 4 is not a case), as the original does; `Item_EquipMask`
`0x5917A0` the equipment's `+0x10`, every other category `0xFF`. Both
getters answer the whole of `eax` as the original leaves it: for an armour
the mask is loaded into `al` over the `id * 13` of the address arithmetic
(the fuzz caught ours answering the byte alone), and category 0's mask is
`0xFFFFFFFF`.

`TextRecord_Set(slot, length, text)` `0x591940`: up to 31 bytes (32 and up
cut to 31) copied a byte at a time into `Text_Records + 32 * slot`, then a
NUL. Ours keeps the byte loop, so a source overlapping the record from
below repeats as the original's does.

## 5. The two menu draws

`Menu_DrawHand(x, y, unused)` `0x5905D0`: `Gpu_SetDrawMode(page 0xF)`,
`Gfx_CommitPrim(0, 0xC)`, then at the new `Gfx_PacketNext` a SPRT at
`(x - 0x16, y + 2)` (the low words), 0x18 x 0xC from `(0xCC, 0x9C)`, CLUT
word `0x7802`, shade 0x80, `Gfx_CommitPrim(0, 0x1C)`. The PSX draws the same.

`Menu_DrawIcon(icon, x, y, w, h, shade)` `0x5903F0` (the status screen's
`0x573ADC`, the field menu's top bar `0x599EEB`, five more): the draw mode's
page 0xF below icon 12, else 0x1E; then a POLY_FT4 with corners `(x, y)`,
`(x + w, y)`, `(x, y + h)`, `(x + w, y + h)` as floats (x, y the low words,
w, h the low bytes), shade r = g = b the last argument's byte; icons 0..11
from `u = 20 * icon, v = 0x50`, 12..19 from `u = 20 * icon + 16` (a byte, so
icon 12 wraps to 4) at `v = 0x64`, both 16 x 16, any other the 15 x 15 cell
`(0xA8, 0xF0)`; page word 0xF / 0x1E; CLUT word `0x7800 |` a byte of a
21-entry table the function builds on its stack. The PSX twin draws its
icons from other cells with a 21-byte table too; the PC re-laid the texture.

**Icon 21 and up read past that table** (D36): `mov dl, [esp + ebx +
0x10]` with no bound. Ours reproduces what the original reads there (the
function is entered at the same `ESP`, the detour being a `jmp`), from
`__builtin_frame_address`: icon 24..27 the return address, 28..31 the first
argument's slot - which by then holds the float `x + w`, stored there as a
temporary - 32..47 the next four arguments, 48..51 the last argument's slot
(now `y + h`), 52 and up the caller's frame. Icons 21..23 read three stack
bytes the original never wrote; ours reads its own saved `EBP` there, so
those three are not reproduced (undefined in the original). Not kept: the
two temporaries the original leaves in its first and last argument slots -
none of the seven callers reads its argument area after the call (each
pops it, or, `0x443376`, reads a local above it). Whether any caller passes
an icon above 20 is not known: the top bar's come from the table
`0x6672AC`.

## 6. The fuzz, and the controls

`BOF3X_SHADOW=char_stats`, at start-up: twenty byte-copies
(`char_stats_fuzz.cpp`, `kClones`), every call out re-aimed at a recorder
(`bof3::CloneCall` with the callee each site was read to call), seven jump
tables relocated in the copies (`move_script::Relocate`; the two byte index
tables of the armour and weapon switches are read from the image, where
nothing moves them). The trait-list pointer table `0x667548` is swapped for
255 pointers into lists of our own (kinds 0..0xF, an `0xE` at the end, one
in eight null) and put back after. 2,000 rounds per function: random bytes
over the roster bonus and the eight records (`0x903640..0x903F90`), the
inventory lists, key items and extras (`0x904000..0x904600`), all 256
`Text_Records`, `Gfx_PacketNext` and a packet buffer, the previews' output;
then that function's boundaries seeded (records at the scale 0 / 1 / 9 / 10
/ 0xFF, base max HP 0 / 999 / 0xFFFF, HP either side of it, one halving
bit, zero percentages, the persistent records and stretches no record
starts at; every weapon and armour case id and its neighbours, accessories
0..0x18 and 0xFF; trait bytes 0xFF and 0..7, a list starting at its end;
slots 0..6 and 0xFF; an item in its list, counts 0 / 1 / 98 / 99 / 100 /
0x7F / 0x80 / 0xFF, full lists with one id or count 0; resistance levels
0..8 / 0x7F / 0x80 / 0xFA / 0xFF, percentages 99 / 100 / 101 / 0x80 / 0x9C,
steps 0..7 / 0x7F / 0x80 / 0xFD / 0xFF; categories 0..8 with stale bits
above; text lengths 0..2 / 30..33 / 255 and sources overlapping the record
from either side; icons 0..20 and 24..91, 11 / 12 / 19 / 20); theirs, then
from the same state ours, under x87 control word `0x027F`, both called
through one 16-argument call so that the frame an icon above 20 reads is
the same; the regions, the answer at the width the original defines and the
recorders' log compared.

The recorders are as loud as the real callees where the caller reads after
the call: the stat steps and the passes rewrite what `Char_RecalcStats`
reads after them (the scale, base and effective max HP, HP, the halving
bits, the percentages, the roster bonus) and the equipment bytes the passes
must not read twice; the recompute moves the four stats of the previews'
copy by a hash of the copy's bytes (and logs the hash, since the copy's
address differs between the passes); the commit moves `Gfx_PacketNext` by
the size three times in four; the primitive set-ups scribble over every
field their callers then write.

Result (2026-09-23):

    shadow      char_stats self-test: 40000 rounds over 20 functions (2000 each), 92366 calls to the stand-ins, 0 MISMATCHES; the roster bonus and records, the inventory lists and key items, Text_Records, the packet buffer, the answer and the stand-ins' log compared
    shadow      char_stats coverage: icons past the table 548; calls draw mode 4000, commit 8000, FT4 2000, SPRT 2000, clamped 21746, cap99 55, resist 29460, cap100 13105, passes 2000 / 2000 / 2000 / 2000, recomputes 4000; roster steps 6095, trait calls 18081; weapon ids 239; Inventory_Add 0 1469, 1 531 (new slot 426); resist 0 1450, 1 550; cap100 0 133, 1 1867; key item found 1386; counts 0 1211, other 789

`BOF3X_SHADOW='*'`: exit 0, every module's self-test passing, 603 ours.

**Fifty-five negative controls**, planted one at a time by a script (not
committed: apply, build, run `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=char_stats`,
restore), each build's output read. Fifty-four are refused by a count
(exit 3), each only in the functions it touches; one by a fault, with a
counting twin. None was a change that changes nothing.

| | Planted | Refused in (rounds of 2,000, by function) |
|---|---|---|
| W1 | DrawIcon: page threshold 0xB | 158 |
| W2 | DrawIcon: Gfx_PacketNext not read again after the commit | 1,451 |
| W3 | DrawIcon: icons 12..19 v1 0x73 | 628 |
| W4 | DrawIcon: past-table frame offset -0x10 | 301 |
| W5 | DrawIcon: bytes 28..31 from y + h | 84 |
| W6 | DrawIcon: h as a word | 1,992 |
| W7 | DrawHand: x - 0x15 | 2,000 |
| W8 | DrawHand: last commit slot 1 | 2,000 |
| W9 | Recalc: + 4 for the rounding | 337 |
| W10 | Recalc: HP cut compared signed | 892 |
| W11 | Recalc: bit 5 steps +0x34 | 691 |
| W12 | Recalc: roster bonus 5 * j + i | 1,308 |
| W13 | Recalc: armour before weapon | 2,000 |
| W14 | Recalc: halving bits read once | 267 |
| W15 | Recalc: scale and base read before the passes | 223 |
| W16 | Traits: kinds 0..7 to Stat_AddResist | 656 |
| W17 | Traits: kinds above 13 to Stat_AddCap100 | 668 |
| W18 | Traits: stops at a kind above 13 | 572 |
| W19 | Weapon: weight byte +0x15 | 1,436 |
| W20 | Weapon: 0x41 second step +0x3B | 63 |
| W21 | Weapon: id read again for the switch | 24 |
| W22 | Armour: ids read in the loop | 216 |
| W23 | Armour: 0x13 order | 142 |
| W24 | Armour: 0x0F second step +1 | 143 |
| W25 | Accessories: 0x17 to +0x36 only | 687 |
| W26 | Accessories: 5 through Stat_AddCap100 | 54 |
| W27 | Resist: down refused at 7 | 79 |
| W28 | Resist: step 0 refused above 7 only | 29 |
| W29 | Resist: up cap compared signed | 38 |
| W30 | Cap100: no floor | 886 |
| W31 | Cap100: refused at 100 and up | 1,217 |
| W32 | PreviewSlot: slot 5 refused | 170 |
| W33 | Preview: ATK DEF AGI INT | 3,828 over `Equip_PreviewSlot` 1,907 and `Equip_PreviewSet` 1,921 |
| W34 | Preview: marks swapped | 3,371 over `Equip_PreviewSlot` 1,676 and `Equip_PreviewSet` 1,695 |
| W35 | PreviewSet: set reversed | 2,000 |
| W36 | InvAdd: cap above 100 | 80 |
| W37 | InvAdd: scratch not cleared | 972 |
| W38 | InvAdd: count 0 not a free slot | 136 |
| W39 | InvAdd: stacks searched in category 4 too | a fault (access violation) - see below |
| W39b | InvAdd: stacks skipped in category 0 as in 4 | 179 |
| W40 | InvCount: flags bit 1 | 103 |
| W41 | InvCount: extra count as one | 32 |
| W42 | InvCount: armour two bytes | 34 |
| W43 | CountUsed: 128 up to category 4 | 230 |
| W44 | KeyHas: 31 bytes | 46 |
| W45 | NamePtr: key items stride 0x16 | 202 |
| W46 | IconKind: consumables +0x12 | 1,048 |
| W47 | EquipMask: 0xFF for every other category | 216 |
| W48 | Price: armour +0x18 | 167 |
| W49 | TextSet: 32 kept | 1,099 |
| W50 | TextSet: memmove | 422 |
| W51 | DrawIcon: shade 0x80 | 1,994 |
| W52 | EquipMask: armour eax cleared | 213 |
| W53 | InvCount: category 3 extras for every category | 182 |
| W54 | Recalc: roster for any record | 1,836 |

W39 is refused by a fault: in category 4 the stack search reads the null
count list (D35's pointer), exit `0xC0000005`, which proves less than
a count (Traps); W39b, the same condition moved to category 0, is its
counting twin. The thinnest by count are W21 (24 rounds: a stand-in must
move the weapon byte between the two stat calls and onto another case),
W28 (29: a level of exactly 7 against a step of 0), W41 / W42 (32 / 34),
W29 (38) and W44 (46); W5, W28, W36 and W42 were 10, 4, 10 and 7 before
their boundaries were seeded (the icon's float slot, the refusal edges
of `Stat_AddResist`, a stack summing to exactly 99..101, a worn item and
its wearer).

## 7. What the self-test does not reach, and what would

The shop route reaches eighteen of the twenty (`recipe_shop` call counts,
2026-09-23): the equipment screen runs `Equip_PreviewSlot` 1,407 times and
through it `Char_RecalcStats` and all four passes; the shop lists
`Item_NamePtr` 10,074, `Item_IconKind` 10,719, `Item_Price` 523;
`Inventory_Add` 3 (a purchase, a found item). **Not reached by it**:
`Stat_AddResist` (no member on the route has a resistance effect) and
`Equip_PreviewSet`; both are fuzz only. Within the reached ones the route
cannot say which weapon, armour and accessory cases ran - the batch's
memory checks compare the records.

What would reach the rest: `Stat_AddResist` any member wearing one of the
resistance items of section 2 (an equip change, or any recompute of a
member who wears one - `0x58884F` recomputes all eight records, eight calls
on the route; what that caller is was not read);
`Equip_PreviewSet` whatever calls `0x57502D` / `0x59D7CF` (unread - an
"optimise" or a whole-set screen by the shape). Icons above 20 and the
null count list of D35 only if a caller asks for them.

For the batch check: all twenty on the `--original` list, and each on the
trace list with the sizes in section 1.

## 8. Found on the way

- D34, D35, D36 ([`known-defects.md`](known-defects.md)),
  latent.
- `Item_Price`'s offsets contradict the `NameTable_Consumables` note's
  "u16 price" at `+0x14` (section 4); not re-measured here.
- The field menu's item handlers call `Char_RecalcStats` and `Inventory_Add`
  (group P); both are now ours, so `item_use`'s stand-ins stand for ours.

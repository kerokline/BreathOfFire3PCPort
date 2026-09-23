# The field menu's item effects, and the functions next to them

**Status:** IN PROGRESS (2026-09-22) - fifty-nine functions ours
(`src/game/item_use.cpp`, shadow name `item_use`), each fuzzed headless
against a copy of Capcom's with every call re-aimed at a recorder;
72 negative controls, 70 refused by a count, one by a fault (with a counting twin), one not refused because it changes nothing. **Not yet through the live batch check** - that runs
centrally after the merge (section 8). Nothing here is reached by the
attract sequence.

Group P of the fourth parallel round
([`takeover-queue-round4.md`](takeover-queue-round4.md)). The queue called
it "the title's hidden cluster": 33 pointer-reached functions after
`Title_LoadTask` that `pe_funcs.py` folded into `0x496AD0`'s 0xBAA bytes.
Read, they are **not title code at all** - they are the 33 handlers of the
field menu's item table, `ItemUse_Handlers` `0x658DB0`, and their PSX twins
are in `START.EMI`, the overlay that holds both the title screen and the
field menu (the sibling's `names/overlays.toml`: "Field main menu (status /
equip / items)"), which is presumably why the PC linker put them beside the
title's loader. The file is named for what the code is; the queue's file
name `title_tasks.cpp` was a guess from the address. No divergence: every
function is a faithful replacement, and no `DIVERGENCE.md` entry is owed.

## 1. The corrected extents in `0x496AD0`'s range

`pe_funcs.py` recorded one function `0x496AD0` of 0xBAA bytes
(`0x496AD0..0x49767A`). By disassembly (capstone, `ret` to `ret`, every jump
checked internal), 2026-09-22, the range holds 37 functions:

| Entry | Bytes | What |
|---|--:|---|
| `0x496AD0` | 0x88 | a camera step on the words `0x929EC8` / `0x929ECC` (and `0x7E0680` / `0x7E0684`), not read further; not ours, not this group's |
| `0x496B60` | 0x127 | `Boot_Task` (group K, [`mode-flow.md`](mode-flow.md)) |
| `0x496C90` | 0x2E | `Title_LoadTask` (group K) |
| `0x496CC0`..`0x497670` | below | the 33 `ItemUse_*` handlers, ours |
| `0x497680` | 0x44 | `ItemUse_Dispatch`, ours - `pe_funcs.py` had this one right |

For `analysis/calltrace/entries_logic.txt`: replace `00496AD0 BAA` with
`00496AD0 88`, and add every line of section 2's table (the file's format,
hex address and hex size). `entries_hidden.txt` already names all 33 handler
entries (by the padding rule), with the padded sizes.

## 2. What is ours

The character record the helpers work on is picked by two arguments: an
**id** (its low byte indexes, unchecked) and a **battle** flag (its low
byte): 0 picks the persistent record `CharacterRecords` `0x903A70` + `0xA4`
x id, non-zero the member's working copy `0x802DC0` + `0x14C` x id. Every
field offset is the PSX record's + 4 (the PC name field is 9 bytes, not 5,
[`save-interchange.md`](save-interchange.md) section 2): status word
`+0x10`, HP `+0x18`, AP `+0x1A`, max-HP scale byte `+0x1E`, max HP `+0x20`,
max AP `+0x22`, ATK / DEF / AGI / INT `+0x24..+0x2A`, their base copies
`+0x40..+0x4A` (the sibling's `BATTLE_RAM.md` record table, shifted).

Item names are the US disc's at each consumable id (the sibling's
`names/items.toml`, extracted from the disc; a table, not a memory of the
game), found through `ItemUse_HandlerIndex` `0x658E38`.

| Function | Entry | Bytes | PSX twin | Does |
|---|---|--:|---|---|
| `ItemUse_Dispatch` | `0x497680` | 0x44 | `0x801E8C1C` (START) | 2 for an item whose flags byte is 0, else `ItemUse_Handlers[ItemUse_HandlerIndex[item]](id, battle)` |
| `ItemUse_None` [0] | `0x496CC0` | 0x3 | `0x801E7E34` | 2 (45 of the 92 ids: every item with no field effect) |
| `ItemUse_HealHp20` [1] | `0x496CD0` | 0x1E | `0x801E7E3C` | HP + 20: Green Apple, Bread, Cheese, Pirana, Flying Fish |
| `ItemUse_HealHp40` [2] | `0x496CF0` | 0x1E | `0x801E7E70` | Healing Herb, Trout |
| `ItemUse_HealHp100` [3] | `0x496D10` | 0x1E | `0x801E7EA4` | Vitamin, Beef Jerky, Rakda Meat |
| `ItemUse_HealHpFull` [4] | `0x496D30` | 0x1E | `0x801E7ED8` | amount 0 = full: MultiVitamin |
| `ItemUse_PartyHealHp100` [5] | `0x496D50` | 0x7B | `0x801E7F0C` | every member: Vitamins |
| `ItemUse_HealAp20` [6] | `0x496DD0` | 0x1E | `0x801E7FA0` | Wisdom Seed, Black Porgy |
| `ItemUse_HealAp100` [7] | `0x496DF0` | 0x1E | `0x801E7FD4` | Wisdom Fruit |
| `ItemUse_HealHp5Cure` [8] | `0x496E10` | 0x88 | `0x801E8008` | HP + 5, then 30 % to clear `0xA0`: Croc Tear, Vinegar |
| `ItemUse_Cure80` [9] | `0x496EA0` | 0x21 | `0x801E8120` | clear `0x80`: Antidote |
| `ItemUse_Cure08` [10] | `0x496ED0` | 0x1E | `0x801E8154` | clear `8`: Eye Drops |
| `ItemUse_Cure100` [11] | `0x496EF0` | 0x21 | `0x801E8188` | clear `0x100`: THE MOCHI |
| `ItemUse_CureA0` [12] | `0x496F20` | 0x21 | `0x801E81BC` | clear `0xA0`: Panacea |
| `ItemUse_Revive` [13] | `0x496F50` | 0x58 | `0x801E81F0` | HP 1, clear `0x4000`: Ammonia (D21) |
| `ItemUse_PartyRestore` [14] | `0x496FB0` | 0x81 | `0x801E8278` | every member: clear `0xA0`, full HP: Moon Tears, Whale |
| `ItemUse_MaxHpUp` [15] | `0x497040` | 0x62 | `0x801E832C` | base and effective max HP + 1, cap 999: Life Shard |
| `ItemUse_MaxApUp` [16] | `0x4970B0` | 0x62 | `0x801E83C8` | Magic Shard |
| `ItemUse_AtkUp` [17] | `0x497120` | 0x62 | `0x801E8464` | Power Food |
| `ItemUse_DefUp` [18] | `0x497190` | 0x62 | `0x801E8500` | Protein |
| `ItemUse_AgiUp` [19] | `0x497200` | 0x62 | `0x801E859C` | Swallow Eye |
| `ItemUse_IntUp` [20] | `0x497270` | 0x62 | `0x801E8638` | Fish=head |
| `ItemUse_Stat2EUp` [21] | `0x4972E0` | 0x62 | `0x801E86D4` | the byte `+0x4E` / `+0x2E` + 1, cap 99: Moxa |
| `ItemUse_HpScaleUp` [22] | `0x497350` | 0x75 | `0x801E8770` | the max-HP scale + 1 (to 9) and a full heal: Mandrake |
| `ItemUse_HealHp1` [23] | `0x4973D0` | 0x1E | `0x801E882C` | Jellyfish, Man=o=War |
| `ItemUse_HealHp80` [25] | `0x4973F0` | 0x1E | `0x801E8894` | Bass, Sea Bass |
| `ItemUse_PartyHealHp80` [26] | `0x497410` | 0x7B | `0x801E88C8` | Black Bass |
| `ItemUse_PartyHealHp240` [27] | `0x497490` | 0x7E | `0x801E895C` | Barandy, Spearfish |
| `ItemUse_PartyCure80` [28] | `0x497510` | 0x7E | `0x801E89F0` | Blowfish |
| `ItemUse_HealAp40` [29] | `0x497590` | 0x1E | `0x801E8A84` | no consumable names entry 29 |
| `ItemUse_HealAp5` [24, 30] | `0x4975B0` | 0x1E | `0x801E8860`, `0x801E8AB8` | RainbowTrout, Sea Bream |
| `ItemUse_HealHp5` [31] | `0x4975D0` | 0x1E | `0x801E8AEC` | Shaly Seed, Berries, Horseradish |
| `ItemUse_FaerieTiara` [32] | `0x4975F0` | 0x7C | `0x801E8B20` | section 3 |
| `ItemUse_WaterJug` [33] | `0x497670` | 0xA | `0x801E8C08` | the byte `0x90405F` = `0xF0` |
| `Char_HealHp` | `0x590CE0` | 0x89 | `0x80165C70` (boot) | section 4 |
| `Char_HealAp` | `0x590D70` | 0x64 | `0x80165D3C` | the same on AP, without the status |
| `Stat_AddCap999` | `0x590DE0` | 0x27 | `0x80165E5C` | a word + n, cap 999, "changed" |
| `Stat_AddCap99` | `0x590E10` | 0x1D | `0x80165EA0` | a byte + n, cap 99, "changed" |
| `Stat_AddClamped` | `0x590E30` | 0x4F | `0x80165EE4` | a word + s16, clamped to 0..999, answers only a clamp |
| `Char_ClearStatus` | `0x590F60` | 0x56 | `0x801660DC` | clear the mask's status bits, "any were set" |
| `MsgBox_ChoiceCommit` | `0x4981C0` | 0x6D | `0x80151494` (boot) | section 5 |
| `MsgBox_MenuCommit` | `0x4983C0` | 0x4C | `0x80151838` | section 5 |
| `MsgBox_SysChoice80`..`84` | `0x498AD0`..`0x498B90` | 0x2E each | `0x80152E2C`..`0x80152F7C` | section 5 |
| `MsgBox_SysChoice85`..`8F` | `0x498BC0`..`0x498D00` | 0x17 each | `0x80152FCC`..`0x80153184` | section 5 |
| `Input_AutoRepeat` | `0x461EB0` | 0x48 | `0x8014E534` (boot) | section 6 |

Bytes are each body to its last instruction (capstone, 2026-09-22), the
figure for `entries_logic.txt`. PSX twins: the handlers by
`analysis/pairs_propagated.json` (table-anchored, gap and callers tiers) and
read side by side in capstone MIPS from the sibling's overlay capture
`b8cc1561` (START.EMI section 8); the helpers from the boot EXE; the commits
found by scanning the boot EXE for `sltiu 0x80` / `lw 0x34` / `jalr`, and the
sixteen system choices from the PSX's own copy of the table, `0x80149ACC`.
Every PSX twin does the same thing in the same order, with two exceptions
that change nothing: `ItemUse_FaerieTiara` stores three cells in another
order with no call between, and the PSX has entries 24 and 30 as two
functions of one body where the PC has one (the linker folded them).

**The answers.** A handler answers 0 (used), 2 (not usable: `ItemUse_None`,
or the dispatcher's own refusal), 3 (no effect) or 4 (used; the stat-ups
only). Its two callers in the field menu `0x58AAB0` (`0x58B039` for a
party-wide item - flag bit `0x10` - with id 0, and `0x58B9C3` for the member
under the cursor, `0x66972C[0x904062[cursor]]`), both with battle 0, play
sound `0x104` and take one from the stack on 0 or 4, and sound `0x107`
otherwise. Nothing checks the item against the member: the menu's gate is
`0x57D9A0`, which for a consumable wants bit 0 of its flags (and for the
Faerie Tiara, `0x536700` at the leader's position not `0xAE`).

Kept as Capcom's has them, each written in the code where it is kept:

- The stat-ups over `Stat_AddClamped` (ATK, DEF, AGI, INT) always answer 4 -
  `Stat_AddClamped` answers 0 unless it clamped - where the two over
  `Stat_AddCap999` answer 0 when either word moved. The menu treats 0 and 4
  alike, so an item on a stat at 999 is consumed with no effect.
- `ItemUse_HealHp5Cure` counts as used on a member with a bit of `0xA0`
  whether or not its 30 % roll (signed `Rand() % 100`) cured it.
- The party walks ask `Party_Count(0)` again after each member and compare
  bytes; the member's index is written into the id argument's own stack slot
  and that dword pushed, so its upper bytes are the caller's. Ours does the
  same (control C12).

## 3. The Faerie Tiara

`ItemUse_FaerieTiara` ignores its arguments. `Inventory_Add(0, 0x57, 1, 0)`
- the tiara gives itself back, so the menu's decrement leaves the stack as
it was; `Game_Step` + 1 (the field menu's sub-step, so the menu moves on);
the byte `0x929F01` (the menu state block, [`menu-screens.md`](menu-screens.md))
`0xA`; the byte `0x905B61` `0xB9`, less one for each story flag `0xFF`,
`0xFE`, .. `0xF6` that `Flags_Test(0x904030, f)` finds clear before the
first set one - `0xAF` when all ten are clear; then the leader's x and z
(`0x802D74`, `0x802D78`) and `Game_AreaNumber` copied to `0x904148`..`0x904151`.
What reads `0x905B61` and the copied position is not read here; by the
shape (a destination chosen by story progress, a return point kept) it is a
warp, but that is a hypothesis. A stack of 99 loses one per use (D23).

## 4. The helpers

`Char_HealHp(id, amount, battle)`: 0 when HP equals max HP. Else HP +=
the amount's low word (wrapping at 16 bits), HP = max when it is then above
max (unsigned) or the amount's low word is 0 (a full heal), and
`Char_ClearStatus(id, 0x2000, battle)` unless max / 4 is above HP - the
`0x2000` bit, set below a quarter of max HP, reads as "in danger"
(hypothesis, by the arithmetic only). Answers 1. `Char_HealAp` is the same
on AP without the status.

`Stat_AddClamped(stat, delta)`: the delta as s16. Up: 0 at 999, else add
and, if the sum is above 999, store 999 and answer 999 - old. Down: 0 at 0,
else add and, if the sum is negative as s16, store 0 and answer -old. Any
change that did not clamp answers **0** - on the PSX too, where the
sibling's note calls the answer "the applied delta". Only `ax` is defined;
none of its 38 call sites (E8 scan) reads above it, so ours answers a
`short`.

`Char_ClearStatus(id, mask, battle)`: if the status word shares a bit with
the mask's low word, clear them and answer 1, else 0.

**Left Capcom's, with recording stand-ins in the fuzz:**

- `Char_RecalcStats` `0x590660` (0x1A0 bytes, the sibling's
  `Char_RecalcStats`): five unowned callees below it (`0x590FC0`,
  `0x591190`, `0x591490`, `0x590800` and what they call) and 24 call sites
  outside this group - a group of its own.
- `Inventory_Add` `0x590BB0` (0xDE, the sibling's `Inventory_Add`): 80 call
  sites across shops, battle rewards and events; read (`symbols.toml`), not
  taken, since only one of its callers is here.
- `MsgBox_SystemChoice` `0x498A30`: section 5.
- `Rand` `0x5B93D2` is the CRT's (a stand-in in the fuzz: the start-up
  self-test runs before the CRT is up, [`HANDOFF.md`](HANDOFF.md) Traps).

`Party_Count` and `Flags_Test` are other groups' and ours already.

## 5. The message box's commits and the system choices

`MsgBox_ChoiceCommit` `0x4981C0` is `MsgBox_State4`'s entry 5 and
`MsgBox_MenuCommit` `0x4983C0` `MsgBox_State5`'s entry 3
([`msgbox.md`](msgbox.md) section 4, where group H left them). Both do the
choice list's action: for a choice id (`0x7DEE64`) below `0x80`,
`call [[Area_Descriptors[Game_AreaNumber] + 0x34] + 4 * id]` - each area has
its own table of choice handlers - and for `0x80` and above
`MsgBox_SystemChoice`. Then the choice commit sets window 1's state
(`0x803187`) to 3 and, if the message word `0x7DEE48` is not `0xFFFF`
(the handler opened another message), window 0's state 1, `0x7DEE58` 0 and
the next sub-state; else sub-state 7 and `0x7DEE58` 0. The menu commit sets
`0x7DEE58` 0, window 1's state 3 and the next sub-state. H could not clone
them because the copy's indirect call reaches the real table; the fuzz here
points `Area_Descriptors[0..3]` at descriptors of its own.

`MsgBox_SystemChoice` `0x498A30` builds a table of sixteen handlers on its
stack and calls `[esp + 4 * id - 0x200]` - **unbounded** (D22). It stays
Capcom's: ours could reproduce ids `0x80..0x8F` but not what an id of `0x90`
or more does (it calls a word of the stack above the table), and a Fatal or
a bounds check there would be a divergence. Its sixteen targets are ours:

- `0x80`: the message word from the pair at `0x658F08` by the cursor
  (`0x7DEE67`, read as s8), then cursor 0 sets the byte `0x929F0B` to 1,
  cursor 1 to 0.
- `0x81`..`0x84`: the message word from their pairs, and the byte
  `0x903848` (the movement script's variable 3,
  [`move-cmds.md`](move-cmds.md)) by the cursor: `0x1E` / `0x28`, `0x1E` /
  `0x32`, `0x3C` / `0x46`, `0x1E` / `0x32`.
- `0x85`..`0x8F`: the message word only.

A cursor outside 0..1 reads a neighbouring pair (the table is contiguous)
and sets no byte, as the original does.

## 6. The pad auto-repeat

`Input_AutoRepeat(pressed)` `0x461EB0` (104 call sites; group H's choice and
menu input among them): with the argument's low word non-zero, the countdown
`0x7E1BE0` = 12, the latch `0x7E01B8` = `Input_Held`, answer the dword at
`Input_Held`; else, if the latch shares a bit with `Input_Held` and the
countdown reaches 0 on its decrement, countdown 3, latch `Input_Held`, the
same dword; otherwise that dword with its low word 0. The upper half of the
answer is `Input_Previous` - the original loads the dword - and ours answers
it the same, since several callers keep the whole `eax`.

## 7. The fuzz, and the controls

`BOF3X_SHADOW=item_use`, at start-up: fifty-nine byte-copies
(`item_use_fuzz.cpp`, `kClones`), every call out re-aimed at a recorder
(`bof3::CloneCall` with the callee each site was read to call), none a jump
table. `ItemUse_Handlers`' 34 entries are swapped for 34 recorders and
`Area_Descriptors[0..3]` pointed at descriptors whose `+0x34` table holds 128
recorders; the item flags bytes and `ItemUse_HandlerIndex` are randomised
for all 256 ids a byte can name, and all of it is put back after. 2,000
rounds per function: random bytes over the eight persistent records, the
working copies and window records (`0x802D70`..`0x803820`), `MsgBoxState`,
the menu bytes, the story flags and party lists, the return point, the area,
`Game_Step`, the pad words; then that function's boundaries seeded (HP at,
one below, one above max and either side of max / 4, amounts 0, `0xFFFF`,
`0x10000`, words 998 / 999 / 1000 / `0xFFFF`, bytes 98 / 99 / 100, deltas
+1 / -1 / 0 / `0x10000` / `0x18000`, masks with bits above 16, the scale
byte 8 / 9 / 10, the story flags `0xF6`..`0xFF` one at a time, choice ids
`0x7F` / `0x80`, the message `0xFFFF`, cursors 0 / 1 / 2 / -1 / -128, the
repeat countdown 0 / 1 / 2 and pressed words whose low half is 0); theirs,
then from the same state ours; the regions, the answer at the width the
original defines, and the recorders' log compared.

The recorders are as loud as the real callees where the caller reads after
the call: a heal or a status clear rewrites the record it names (the cure
handler reads the status after its heal), `Char_RecalcStats`'s rewrites max
HP (the scale handler reads it again), `Party_Count`'s answers anew each time
with stale upper bytes half the time, `Rand`'s answers around the 30 %
boundary and negative values the CRT never gives (so signed and unsigned
division differ), `Stat_AddClamped`'s answers a word with a zero low byte a
quarter of the time, and every recorder may move one of the cells a caller
reads after a call (the message word, the sub-state, both window states,
`Game_Step`, `0x905B61`, `0x929F01`, the leader's position, the area, the
party lists, a record's status / HP / max).

Result (2026-09-22):

    shadow      item_use self-test: 118000 rounds over 59 functions (2000 each), 125800 calls to the
                stand-ins, 0 MISMATCHES
    shadow      item_use coverage: handler answers 0 42433, 2 2000, 3 18037, 4 3530, other 0;
                dispatched to 34 of 34 entries, refused 1018; area choices 122 of 128, system choice 1968;
                heals answered 1 1687 (status cleared 1542); clamped up 474, down 624; status cleared 1037;
                scale raised 593, filled 1178; repeat fired 1485; party counts asked 25289, rolls 1393,
                flag tests 8959, recomputes 593

`BOF3X_SHADOW='*'`: exit 0, every module's self-test passing.

**Seventy-two negative controls**, planted one at a time
by a script (not committed: apply, build, run
`BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=item_use`, restore), each build's output
read. Seventy are refused by a count (exit 3), each only in the functions it
touches:

| | Planted | Refused in (rounds of 2,000, by function) |
|---|---|---|
| P1 | Dispatch: flags byte at stride 21 | 977 |
| P2 | Dispatch: refused answer 3, not 2 | 1,013 |
| P3 | Dispatch: handler called (battle, id) | 960 |
| P4 | Dispatch: handler index masked to 5 bits | 59 |
| P5 | None: answers 3 | 2,000 |
| P6 | HealHp20: 0x15 HP | 2,000 |
| P7 | HealHpFull: amount 0xFFFF, not 0 | 2,000 |
| P8 | HealAp5: 4 AP | 2,000 |
| P9 | Cure08: mask 0x10 | 2,000 |
| P10 | UsedOr3: failure answers 1 | 17,215 over 22 functions |
| P11 | UsedOr3: bit 0 only | 8,146 over 22 functions |
| P12 | PartyWalk: id upper bytes zeroed | 4,062 over 5 functions |
| P13 | PartyWalk: Party_Count asked once | 8,055 over 5 functions |
| P14 | PartyWalk: count compared as an int | 4,082 over 5 functions |
| P15 | Member: party list skipped | 5,894 over 5 functions |
| P16 | PartyHealHp240: 0xF1 | 1,588 |
| P17 | PartyCure80: mask 0xA0 | 1,624 |
| P18 | PartyRestore: heal before the cure | 1,620 |
| P19 | PartyRestore: the cure not counted | 145 |
| P20 | HealHp5Cure: status read before the heal | 345 |
| P21 | HealHp5Cure: roll by unsigned division | 115 |
| P22 | HealHp5Cure: <= 30 | 202 |
| P23 | HealHp5Cure: used only when cured | 316 |
| P24 | HealHp5Cure: status bit 0x80 only | 499 |
| P25 | Revive: HP set after the status test | 1 |
| P26 | Revive: D21 fixed (HP only when revived) | 1,000 |
| P27 | MaxHpUp: effective before base | 2,000 |
| P28 | UsedOr4: answers 3 | 3,530 over 7 functions |
| P29 | ClampedUp: the whole word tested | 1,502 over 4 functions |
| P30 | IntUp: +0x4C | 2,000 |
| P31 | Stat2EUp: +0x2F | 2,000 |
| P32 | HpScaleUp: scale <= 9 | 208 |
| P33 | HpScaleUp: max read before the recompute | 442 |
| P34 | HpScaleUp: HP above max answers 0 | 507 |
| P35 | FaerieTiara: loop floor 0xF7 | 254 |
| P36 | FaerieTiara: count kept in a register | 324 |
| P37 | FaerieTiara: Game_Step before Inventory_Add | 93 |
| P38 | FaerieTiara: area stored as a byte | 1,992 |
| P39 | FaerieTiara: two given back | 2,000 |
| P40 | WaterJug: 0xF1 | 2,000 |
| P41 | HealHp: zero amount tested as a dword | 60 |
| P42 | HealHp: signed clamp compare | 583 |
| P43 | HealHp: quarter test < | 140 |
| P44 | HealHp: clears 0x4000 | 1,542 |
| P45 | HealHp: full HP test dropped | 313 |
| P46 | HealAp: no full heal on 0 | 130 |
| P47 | Cap999: cap test > 1000 | 30 |
| P48 | Cap999: 32-bit sum | 141 |
| P49 | Cap99: >= 99 refused | 1,165 |
| P50 | Cap99: sum as int | 389 |
| P51 | Clamped: unclamped up answers the delta | 132 |
| P52 | Clamped: delta as int32 | 789 |
| P53 | Clamped: down clamp at > 0 | 30 |
| P54 | Clamped: 999 not refused | **not refused** - see below |
| P55 | ClearStatus: low byte only | 246 |
| P56 | ClearStatus: bits toggled | 427 |
| P57 | ChoiceCommit: window 1 only with more | 929 |
| P58 | ChoiceCommit: message read before the commit | 54 |
| P59 | ChoiceCommit: done leaves 0x7DEE58 | 930 |
| P60 | CommitChoice: threshold 0x7F | 269 over 2 functions |
| P61 | CommitChoice: descriptor +0x38 | a fault (access violation) - see below |
| P62 | MenuCommit: sub-state read before the commit | 95 |
| P63 | SysMessage: cursor zero-extended | 12,237 over 16 functions |
| P64 | SysChoice80: cursor 1 does nothing | 210 |
| P65 | SysChoice83: values swapped | 451 |
| P66 | SysChoice8F: pair 14 | 1,137 |
| P67 | AutoRepeat: pressed as a dword | 373 |
| P68 | AutoRepeat: refire every 4 | 35 |
| P69 | AutoRepeat: no-fire answers 0 | 515 |
| P70 | AutoRepeat: latch kept on refire | 35 |
| P71 | CommitChoice: the neighbouring handler | 2,032 over 2 functions |
| P72 | AutoRepeat: first wait 11 | 1,450 |

Three need a word:

- **P54 is not refused, and cannot be**: `Stat_AddClamped`'s "0 at 999" test
  on the way up is redundant - from 999 any positive s16 delta sums to at
  most 33,766, which never wraps below 1000, so the clamp stores 999 and
  answers 999 - 999 = 0 anyway. A change that changes nothing (Traps); the
  test is kept because the original has it.
- **P25 is refused in one round of 2,000, and only by the stand-in**:
  `ItemUse_Revive` sets HP before calling `Char_ClearStatus`, which never
  reads or writes HP, so the order is unobservable against the real callee;
  the loud stand-in's occasional scribble on a record's HP is what told them
  apart. P26 - D21 "fixed" - is refused in 1,000 rounds.
- **P61 is refused by a fault** (the copy and ours call through `+0x38` of a
  descriptor whose bytes are `0xCC`: exit `0xC0000005`), which proves less
  than a count (Traps); P71, the neighbouring handler of the right table,
  is its counting twin: 2,032 rounds.

The thinnest by count are P47 / P53 (30 rounds: the 999 and 0 boundaries
exactly), P68 / P70 (35: a refire needs the countdown at exactly 1 and a
latched bit held) and P58 / P41 (54 / 60).

## 8. What the self-test does not reach, and what would

**Nothing here is reached by the attract sequence**: `hidden_b`, `all_a` and
`all_b` count no call to any of the fifty-nine, nor to `0x58AAB0` (the field
menu's item screen), `0x498A30` or the `0x5906xx` helpers (2026-09-22). The
fuzz cannot see what the callees really do, nor the item screen around them.
What would reach them in game:

- **The handlers, the dispatcher and the helpers:** using an item from the
  field menu. `tools/recipes/field_menu.txt` and `menu_screens.txt` open the
  menu on a loaded save; a recipe that opens Items, picks a healing item and
  a member, and confirms would run `ItemUse_Dispatch`, one handler and
  `Char_HealHp`; a party item (Vitamins) the party walk. The stat-ups, the
  Faerie Tiara and the Water Jug need those items in the save.
  `Char_HealHp`, `Char_ClearStatus` and `Stat_AddClamped` are also called
  from battle and the equipment pass (`Char_RecalcStats`), so a battle
  recipe (`battle_commands.txt`) or an equip change reaches them too.
- **The commits:** any choice list in dialogue - a yes / no question
  (`MsgBox_ChoiceCommit`) or a shop / master's menu (`MsgBox_MenuCommit`).
  The system choices `0x80..0x8F`: a choice list whose id is `0x80` or more,
  which scripts use is not measured.
- **`Input_AutoRepeat`:** holding a direction in any menu or choice list -
  every menu recipe presses once, which reaches only the first branch.

For the batch check: all fifty-nine on the `--original` list, and each on
the trace list with the sizes in section 2 (and `00496AD0 88`).

## 9. Found on the way

- D21, D22 and D23 ([`known-defects.md`](known-defects.md)), all latent and
  Capcom's on both platforms.
- `MoveScript_EffectState` `0x66972C` (a hypothesis name from group L's use
  of it) is read here as the party list's record map: `0x66972C[0x904062[i]]`
  is member i's record index, both in the menu and in the party walks, and
  its 24 bytes are `0..7` (measured). Not renamed here - one name, one owner.
- The sibling's note on `Stat_AddClamped` says it "returns the applied
  delta"; on both platforms it answers 0 for any change that did not clamp.

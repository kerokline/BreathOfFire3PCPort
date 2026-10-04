# Group R2E: the field menu's Items arrange steps and sorts, the Equipment and Ability screens

**Status:** MEASURED (2026-10-04) - round fourteen
([`takeover-queue-round14.md`](takeover-queue-round14.md)), wave two, on the
round branch's tip `fa583bc`. **49 functions ours** (`src/game/rest_2e.cpp`,
`rest_2e.h`, `rest_2e_callees.h`, shadow name `rest_2e`): the cut's 48 rows for
R2E (`analysis/round14_cut.tsv`) and the start in their span no list had
(`0x58CFC0`, band_rows' "code no list has"). Each read to its last instruction
with capstone and fuzzed through the scenario harness's **field** mode (used
unchanged): 294,000 rounds, **0 mismatches**; CONTROLS_SUMMARY (section 6).
Thirteen `.data` tables named. Fuzz only: no trace enters them (section 9).

**What the band is.** Three of the field menu's screens
([`menu-screens.md`](menu-screens.md) section 1: `FieldMenu_Run` jumps through
`FieldMenu_States` by the menu block's mode `0x929F00`; each screen's dispatcher
jumps by the state `0x929F01`, a few states by the step `0x929F02`):

- **The Items screen** (`FieldMenu_States[2]`): its dispatcher `0x58AAE0` and
  table `0x667328` are R2D's. Here: the three arrange steps R2D's `0x58B1C0`
  (state 5) jumps to through `FieldItems_ArrangeSteps`, states 6 (the discard
  prompt), 7 (a consumable used on a member) and 9 (a 32-entry list view), two
  window helpers R2D's states call, and `FieldItems_Sort` with its seven sorts.
- **The Equipment screen** (`FieldMenu_States[4]`): `FieldEquip_Run` and its
  states but FS's two choosers (`Equip_ChooseSlot`, `Equip_ChooseItem`, round
  twelve, [`field_s.md`](field_s.md)), the two "best equipment" previews and
  their apply, the preview helpers FS called by address.
- **The Ability screen** (`FieldMenu_States[3]`): `FieldAbility_Run`, its ten
  states, and the two step machines under states 7 (the arrange) and 9 (a list
  view).

The names say what the code does - which list, which window, which bytes - not
what the player sees; no gameplay fact is stated from memory. The menu block
`0x929F00` (mode, state `+1`, step `+2`, timer `+4`, member `+6`, target `+8`,
answer `+0xB`) and `WindowRecords` `0x803160` (22 of `0x24`) are the cells.

| Function | Entry | Bytes | Reached by | What |
|---|---|--:|---|---|
| `FieldItems_ArrangeCategory` | `0x58B1D0` | 0x15F | `FieldItems_ArrangeSteps[0]` | the category `0x80333E` turned through 0..3 by 0x8000 / 0x2000 (sound 0x101), the top from `0x9398B8`; confirm opens the sort window `0x803358`, step up; cancel state - 3 |
| `FieldItems_ArrangeHow` | `0x58B330` | 0x17B | [1] | the row `0x803363` over the category's `FieldItems_ArrangeRows` count; confirm: the row's sort id through `FieldItems_Sort`, or 0: the hand-arranged list (step up); cancel step down |
| `FieldItems_ArrangeMove` | `0x58B4B0` | 0x298 | [2] | the list scroll (section 1.1); confirm picks an entry (`0x803341`) or swaps the pick with the cursor's entry in the id and count lists |
| `FieldItems_DiscardConfirm` | `0x58B750` | 0x144 | Items state 6 (`0x667340`) | the item's name to text record 0; the yes / no hand by s8 `0x929F0B`, flipped by 0x2000 / 0x8000; yes clears the id and count; state - 3 |
| `FieldItems_UseOnMember` | `0x58B8A0` | 0x194 | Items state 7 (`0x667344`) | the member hand; confirm: `ItemUse_Dispatch(record, item, 0)` - al 0 or 4: the count down (at 0 the id cleared and the screen left), else a buzz |
| `FieldItems_ViewList32` | `0x58BA40` | 0x1E3 | Items state 9 (`0x66734C`) | the list scroll over 32 entries; cancel: state - 7, the category 0 |
| `FieldItems_InitWindows` | `0x58BC30` | 0x108 | E8 from R2D's `0x58AAF0` | the screen's window records set up |
| `FieldItems_Sort` | `0x58BD40` | 0x10 | E8 from `FieldItems_ArrangeHow` | jmp through `FieldItems_Sorts` by the argument's low byte (7) |
| `FieldMenu_SwapBytes` | `0x58BD50` | 0x13 | E8, 18 sites (section 8) | the two bytes exchanged |
| `FieldItemSort_Compact` | `0x58BD70` | 0x8C | `FieldItems_Sorts[0]`, E8 from the six | 127 passes moving the category's empty entries to the end (ids and counts) |
| `FieldItemSort_ConsumableFlag1` | `0x58BE00` | 0xA6 | [1] | Compact, then 127 bubble passes over the consumables `0x904154`: flag bit 0 (`NameTable_Consumables +0x10`) first |
| `FieldItemSort_ConsumableFlag2` | `0x58BEB0` | 0xA6 | [2] | the same on bit 1 |
| `FieldItemSort_WeaponsByPower` | `0x58BF60` | 0xA7 | [3] | weapons `0x9041D4` by `NameTable_Weapons +0x16`, highest first |
| `FieldItemSort_ArmourByPower` | `0x58C010` | 0x9C | [4] | armour `0x904254` by `NameTable_Armour +0x14`, highest first |
| `FieldItemSort_ByIconKind` | `0x58C0B0` | 0xEB | [5] | the category's list by `Item_IconKind`, ascending |
| `FieldItemSort_EquipableFirst` | `0x58C1A0` | 0xF7 | [6] (no row names it) | entries member s8 `0x929F06` can equip (`Item_EquipMask` bit) first |
| `FieldItems_CloseWindows` | `0x58C2A0` | 0x12 | E8 from R2D's `0x58B130` | three window records cleared |
| `FieldEquip_Run` | `0x58C2C0` | 0xE | `FieldMenu_States[4]` | jmp through `FieldEquip_States` by `0x929F01` (9) |
| `FieldEquip_Open` | `0x58C2D0` | 0x38 | `FieldEquip_States[0]` | `FieldEquip_InitWindows`, sound 0x102, cursor `0x6BDFAF` 0, timer 5, state up |
| `FieldMenu_CountdownState` | `0x58C310` | 0x23 | `FieldEquip_States[1]`, [4], `FieldAbility_States[1]` | the timer down, at 0 the state up |
| `FieldEquip_TopMenu` | `0x58C340` | 0x152 | [2] | four entries (`FieldEquip_TopHelp`), 0x8000 then 0x2000 both tested; confirm state up, cancel state + 5 |
| `FieldEquip_PickMember` | `0x58C4A0` | 0x2F7 | [3] | the member `0x803340`; cursor 1 / 2 preview the best equipment every frame; confirm: 0 the slot window (state up), 3 state + 5, 1 / 2 `FieldEquip_ApplyPreview` |
| `Equip_ChooseSlot`, `Equip_ChooseItem` | `0x58C7A0`, `0x58CAE0` | | [5], [6] | FS's (round twelve) |
| `FieldEquip_Close` | `0x58CD40` | 0x8D | [7] | the timer up to 4: the windows closed, mode 1, state 0 |
| `FieldEquip_RemoveSlot` | `0x58CDD0` | 0x1EC | [8] | the slot `0x80333E` 1..5 (a five-case jump table at `+0x1D8`); `FieldEquip_PreviewRemove`; confirm takes the item off (`Inventory_Add`, `Char_RecalcStats`, apply); cancel state - 5 |
| `FieldEquip_InitWindows` | `0x58CFC0` | 0xF4 | E8 from `FieldEquip_Open` | the screen's window records; `0x803354` = `0x6BDFA8` (the preview) |
| `FieldEquip_BestByPower` | `0x58D0C0` | 0x220 | E8 (`FieldEquip_PickMember`) | the preview: the weapon with the highest `+0x16` and per armour slot the armour of type 2 + slot with the highest `+0x14` the member can equip |
| `FieldEquip_BestByOrder` | `0x58D2E0` | 0x290 | E8 | the lowest `+0x14` / `+0x13` first, then the highest `+0x16` / `+0x14` (section 7) |
| `FieldEquip_ApplyPreview` | `0x58D570` | 0xC6 | E8 (PickMember, RemoveSlot, FS's `Equip_ChooseItem`) | each changed slot through `Inventory_Remove` / `Inventory_Add` by `FieldEquip_SlotCategories`; `Char_RecalcStats` |
| `FieldEquip_PreviewItem` | `0x58D640` | 0xBD | E8 (FS's `Equip_ChooseItem`) | `0x803341` = whether the chosen item lacks the member's bit; the preview with it in the slot |
| `FieldEquip_PreviewRemove` | `0x58D700` | 0xA2 | E8 (`FieldEquip_RemoveSlot`) | the preview without the slot's item; al 1 when there was one |
| `FieldEquip_CloseWindows` | `0x58D7B0` | 0xD | E8 (`FieldEquip_Close`) | two window records cleared |
| `FieldAbility_Run` | `0x58D7C0` | 0xE | `FieldMenu_States[3]` | jmp through `FieldAbility_States` by `0x929F01` (10) |
| `FieldAbility_Open` | `0x58D7D0` | 0x44 | `FieldAbility_States[0]` | R2F's `0x58ED40`, sound 0x102, timer 5, `0x929F06` = `0x905BA1`, cursor `0x6BDFB7` 0 |
| `FieldAbility_TopMenu` | `0x58D820` | 0x198 | [2] | four entries (`FieldAbility_TopHelp`); confirm 1: state + 5 (the arrange), 3: state + 7 (the view), 0 / 2: state up (2: type 3) |
| `FieldAbility_PickMember` | `0x58D9C0` | 0x27A | [3] | the member `0x8033AA` and (not cursor 2) the list type `0x8033AB`; confirm: the kept cursor from `0x9398C0`, state up |
| `FieldAbility_PickAbility` | `0x58DC40` | 0x42A | [4] | the cursor 0..9 over `Char_AbilityList`; confirm (cursor 0): `Skill_CanUse`, then flag 0x10 uses it on the member at once (R2D's `0x58A3C0`) or opens the target windows; other cursors: state + 2 |
| `FieldAbility_PickTarget` | `0x58E070` | 0x24C | [5] | the target `0x929F08`; confirm: R2D's `0x58A3C0(user, target, ability, 0)`, sound 0x109 on 1 / 5, else 0x107 |
| `FieldAbility_ShareConfirm` | `0x58E2C0` | 0x144 | [6] | yes: `AbilityList_Add(ability, 0, 1, 0)` and the entry cleared; state - 2 |
| `FieldAbility_ArrangeRun` | `0x58E410` | 0xE | [7] | jmp through `FieldAbility_ArrangeSteps` by `0x929F02` (3) |
| `FieldAbility_ArrangeMember` | `0x58E420` | 0x21E | ArrangeSteps[0] | member and type; confirm opens the sort window `0x8033E8` |
| `FieldAbility_ArrangeHow` | `0x58E640` | 0x19D | [1] | the row of `FieldAbility_ArrangeRows`; a sort id to R2F's `0x58EE40`, or 0: hand-arranged |
| `FieldAbility_ArrangeMove` | `0x58E7E0` | 0x1EC | [2] | pick and swap two entries of the list (`FieldMenu_SwapBytes`) |
| `FieldAbility_Close` | `0x58E9D0` | 0x8B | [8] | the timer down: R2F's `0x58F050`, the windows, `0x905BA1` = `0x929F06`, mode 1, state 0 |
| `FieldAbility_ViewRun` | `0x58EA60` | 0xE | [9] | jmp through `FieldAbility_ViewSteps` by `0x929F02` (5) |
| `FieldAbility_ViewOpen` | `0x58EA70` | 0x5A | ViewSteps[0] | R2F's `0x58F000`, the list's cursor and top from `0x939892` / `0x9398CC` |
| `FieldAbility_ViewWait` | `0x58EAD0` | 0x23 | [1] | the timer down, step up |
| `FieldAbility_ViewBrowse` | `0x58EB00` | 0x1BA | [2] | help `0x4183 + 0x8033D0`; the list scroll over 18 entries; cancel step up |
| `FieldAbility_ViewLeave` | `0x58ECC0` | 0x44 | [3] | the timer down: `0x8033A3` 2, sound 0x102, step up |
| `FieldAbility_ViewEnd` | `0x58ED10` | 0x30 | [4] | the timer down: step 0, state - 7 (back to the top menu) |

### 1.1 The list scroll

`FieldItems_ArrangeMove`, `FieldItems_ViewList32` and `FieldAbility_ViewBrowse`
share one shape (FS's `Equip_ChooseItem` and `SharedList_PickShared` have its
siblings): 0x1000 up a row (cursor above 0; the scroll cell 0xF0 when it goes
above the top), 0x4000 down below the last entry (0x10 at the top + 9), 4 a page
up (top 0: cursor 0; top below 9: both less the top; else both less 9), 8 a page
down (top at the last page: the cursor to the last entry; above the last page
less 9: both by the rest; else both + 9). Entries 0x80 / 0x20 / 0x12, the last
page's top 0x77 / 0x17 / 9, the scroll cell the word `0x803346` or the byte
`0x8033CC`. Ours is one helper (`Scroll`) with the three sets.

## 2. Divergence

None. `DIVERGENCE.md`, `cheats.cpp`, `widescreen.cpp`, `labels.cpp` and the
yes / no layout files name no address in `0x58B1D0..0x58ED3F` nor the tables
`0x667354..0x667443` (grep, 2026-10-04): no language overlay retargets a call
site here (the band draws nothing itself - every draw is in the window
handlers), and no full-frame fill (nothing for DIV-0041).

## 3. Arguments and answers

- `FieldItems_Sort(how)` reads `[esp+4] & 0xFF`; its entries read nothing.
  `FieldEquip_BestByPower` / `_BestByOrder(member)` read the low byte.
  `FieldMenu_SwapBytes(a, b)` two pointers. `FieldEquip_PreviewRemove` answers
  al (eax 0 / 1 whole; `ret_mask` 0xFF). Every other function is `void (void)`.
- **Leftovers above what callees read**, handed on as the callee reads them
  (the fuzz lists each with the mask it reads):
  `Menu_DrawBackdrop`'s kind is `al` over the previous `eax`; `Item_NamePtr`'s
  category (`FieldItems_DiscardConfirm`) is `al` over `Menu_DrawBackdrop`'s
  answer (`Item_NamePtr` reads the low byte); `Item_EquipMask`'s item is a
  byte over the caller's argument slot (`FieldEquip_BestByPower`) or an
  uninitialised local (`_BestByOrder`, `_PreviewItem`) - it reads `item &
  0xFF`; `Inventory_Remove` / `Inventory_Add` in `FieldEquip_ApplyPreview` and
  `FieldEquip_RemoveSlot` get bytes over register leftovers and a fourth
  pushed 0 (they read three bytes); `ItemUse_Dispatch`'s id is the record byte
  over the caller's `edx` (the handlers and `Char_HealHp` read `id & 0xFF`).
  **R2D's `0x58A3C0`** gets the user's record byte over `Skill_CanUse`'s or
  `Char_AbilityList`'s leftover `edx`, and passes its first argument whole to
  `Skill_ApCost` and the `0x6672EC` handlers: R2D's to read (section 8); the
  fuzz compares the low bytes. Its fourth argument (0, "not in battle") is the
  extra `push 0` left on the stack by the `Char_AbilityList` call before it.
- The window coordinates are 16-bit stores of 32-bit arithmetic whose upper
  halves are leftovers; ours computes the low 16 bits, all the stores keep.
- **Same-expression ordering**: every callee's answer is taken into a local
  before the next read, in the original's order (`Item_IconKind` asked for the
  second entry first; the id re-read after `Inventory_Remove`).

## 4. The fuzz (`rest_2e_fuzz.cpp`)

49 clones, 6,000 rounds each, field mode (`g.field`, `menu_span` 3): 31 `kMenu`
(the states and the four dispatchers), 18 `kCall` (the window helpers, the
sorts, the previews; `FieldMenu_SwapBytes` with two `Arg::kScratch` pointers;
`FieldEquip_PreviewRemove` `ret_mask` 0xFF). The five dispatch tables
(`FieldItems_Sorts`, `FieldEquip_States`, `FieldAbility_States`,
`FieldAbility_ArrangeSteps`, `FieldAbility_ViewSteps`) are `DataTable`s
swapped for recorders; `FieldEquip_RemoveSlot`'s jump table is moved into its
copy. `BOF3X_R2E_ONLY=<name>` runs the clones whose name contains it.

**The callees**: the group's own nine called by `E8` (keyed on ours), R2D's and
R2F's five by address, and four of ours re-listed (`Item_EquipMask`,
`ItemUse_Dispatch` - the standard set lacks both - and `Item_NamePtr`,
`Inventory_Remove` with byte masks, section 3). **Louder stand-ins:**
`FieldMenu_SwapBytes` exchanges the bytes (its callers bubble on and re-read the
lists); `FieldItemSort_Compact` moves an inventory entry from the log's noise
(its six callers sort what it leaves); `0x58A3C0` answers 0..6 so 1 and 5 come
up; `ItemUse_Dispatch` 0..5 (0 and 4 used).

**Regions** beyond field mode's: `WindowRecords`, `Field_ConfirmButtons` /
`Field_CancelButtons`, `CharacterRecords` past the style cells, the inventory's
lists `0x904160..0x904560`, the per-member bytes `0x939880 + 0x50`, the preview
and cursor bytes `0x6BDFA8 + 0x18`, `0x937F8C + 4`.

**The seed** (every round): the buttons (a confirm and a cancel bit, the pressed
word hitting either, both or neither, with 0x2000 / 0x8000 / 0xA000 / 0x1000 /
0x4000 half the time); the inventory mostly empty with a run of entries at a
list's front, ids inside the name tables; the party list from ids whose record
is one of the eight; the timer at 0..5 (the `== 0` / `== 4` tests); the member,
target and answer bytes at -1..3; the two top-menu cursors at 0..3 and 0xFF;
the category / slot `0x80333E` (0..3 for the Items functions, 0..6 and below
0x10 for the Equipment ones - the preview's slot index); the list's top at every
page boundary (0, 8, 9, 0xE, 0x17, 0x6E, 0x6F, 0x77) and the cursor at the top,
top + 8, top + 9, top - 1, 0x1F, 0x7F; the pick 0xFF half the time; the scroll
word 0 three times in five; the Ability cells (member, type 0..3 - it indexes
the `0x9398C0` block they write -, cursor 0..9 and 0xFF, pick, the view's top
and cursor at their boundaries, the sort row); per function the dispatcher's own
byte below its table, `FieldItems_ViewList32`'s category up to 4 (the key items,
whose count list is null and which it does not read), the Equipment previews
equal to the record's bytes half the time (`FieldEquip_ApplyPreview`'s skip).
`Args`: `FieldItems_Sort`'s index below 7, the previews' member 0..2.

**The disturbance** (from its hash only): the category, the cursor (0..2), the
pick, the top, the scroll word, the Ability member / type / cursor / pick /
row, the member and answer bytes, the two cursors, an inventory byte, a preview
byte, a party id (from the hash, one whose record is one of the eight).

**Result** (this worktree, `BOF3X_SELFTEST_ONLY=1 BOF3X_SHADOW=rest_2e`, exit
0): 294,000 rounds, 33,468,459 calls to the stand-ins, **0 mismatches**. Every
table entry reached (handler recorders 562..2,091 calls each; `Equip_ChooseSlot`
and `Equip_ChooseItem` among them through `FieldEquip_States`); the thinnest
callees `FieldItems_Sort` 1,686, `FieldEquip_BestByPower` 1,231, `0x58F050`
1,466 calls. The first run failed every round of the party list: the
disturbance had drawn a member id from the harness's `Next()` (round ten's rule:
from the hash only) - fixed before anything else.

**Under `'*'`**: STAR_RESULT

## 5. What the cut and the tool said, settled

- **One start no list had**: `0x58CFC0` (`FieldEquip_InitWindows`, E8 from
  `FieldEquip_Open`; after `0x58CDD0`'s jump table, whose cut size 740 ran over
  it). Taken.
- **Extents**: band_rows' read sizes are the code's; the cut's add the padding.
  The catalogue's host extents `0x58AAB0` (R2D's, over the Items states),
  `0x58BD70` 0x527, `0x58C2A0` 0x500 and `0x58D7B0` 0x1590 cover the hidden
  starts (section 11).
- **No case, no shared tail, nothing dropped.** Every hidden start is an entry
  by address (a table cell or an `E8`); the four one-instruction dispatchers are
  functions of their own (`FieldEquip_Run`, `FieldAbility_Run`,
  `_ArrangeRun`, `_ViewRun`).
- **The labels**: "Table FieldMenu_States" for `0x58C2C0` / `0x58D7C0` was the
  cell that reaches them (`FieldMenu_States[4]` / `[3]`), the tables they read
  are `FieldEquip_States` / `FieldAbility_States`; the six `hypothesis` rows
  (`0x58BD40`, `0x58BD50`, `0x58C2C0`, `0x58D0C0`, `0x58D2E0`, `0x58D700`) are
  the sort dispatcher, the swap, the dispatcher, the two previews and the
  remove preview - the menu's code, not "field core".
- **The tables**: `0x667328` (the Items states, 11) is R2D's dispatcher's and
  left to R2D; `0x667354` holds three entries, all R2E's, read by R2D's
  `0x58B1C0` - named here `FieldItems_ArrangeSteps` (R2D may name it too: for
  the coordinator). Counts by hand: `FieldItems_Sorts` 7 (`0x66739C` follows),
  `FieldEquip_States` 9 (data follows), `FieldAbility_States` 10, its arrange
  steps 3 (the 12-byte rows follow), view steps 5 (R2F's `0x667444` sort table
  follows - its three entries are R2F's sorts, read by R2F's `0x58EE40`).

## 6. Controls

`r2e/controls.py` (scratch): each plant replaces a string that occurs once in
`rest_2e.cpp`, rebuilds, runs the self-test on the clones whose name contains
the filter, restores and rebuilds. CONTROLS_SUMMARY.

CONTROLS_TABLE

## 7. Latent defects and ranges (Capcom's, described, not fixed)

- **The dispatchers are unbounded**: `FieldEquip_Run` and `FieldAbility_Run` by
  the state byte, `FieldAbility_ArrangeRun` and `_ViewRun` by the step byte,
  `FieldItems_Sort` by its argument - the original jumps through the dword after
  each table. Ours aborts. Every state and step every function here writes
  stays inside its table; not seen in play.
- **The inventory's category indexes `Inventory_IdLists` / `_CountLists`
  unbounded**, and category 4's count list is a null pointer: a sort or a swap
  on category 4 would write through it (a fault). `FieldItems_ArrangeCategory`
  keeps the category to 0..3 and `FieldItems_ViewList32` (the only reader that
  may see 4) reads the id list alone. Ours aborts on a category above 4 or on
  the null list.
- **`FieldEquip_BestByOrder`'s armour pass** breaks a full tie by comparing the
  candidate with the *weapon's* preview byte `0x6BDFA8`, not the slot's, and on
  taking a candidate keeps its `+0x13` byte as the best `+0x14` too: from then
  on the second key compares an armour's `+0x14` against a `+0x13` value.
  Reproduced; which armour the "order" preview picks therefore depends on the
  weapon chosen and on the order of the list. What the two previews are in play
  is the owner's to say.
- **`FieldEquip_RemoveSlot` takes off the slot read before this frame's turn**:
  the category and record byte are taken before the cursor moves, the preview
  (`FieldEquip_PreviewRemove`) after; a turn and a confirm in one frame remove
  the previous slot's item. Reproduced.
- **`FieldEquip_PreviewItem` writes `0x6BDFA8[slot]`** with the slot byte
  unbounded (0..5 while FS's `Equip_ChooseItem` runs).
- **`FieldEquip_PickMember`'s sound** compares the old member as signed against
  the new as unsigned: a cursor at 0x80..0xFF sounds unmoved. Harmless.
- **`FieldItemSort_EquipableFirst` is unreachable** from `FieldItems_ArrangeRows`
  (no row holds sort 6); kept, faithful.
- **Leftovers handed on** (section 3) are read as bytes by every callee that is
  ours; `0x58A3C0`'s first argument is R2D's to settle.

Nothing here needs a ledger entry: no original read of never-written memory
reaches a draw or a decision (the uninitialised upper bytes above are never
read by the callees that are ours).

## 8. Calls across groups

**Outbound, raw** (`rest_2e_callees.h`, the round's rebinding turns them into
names):

| Address | Owner | From | What |
|---|---|---|---|
| `0x58A3C0` | R2D | `FieldAbility_PickAbility`, `_PickTarget` | (user, target, ability, battle) -> al |
| `0x58ED40` | R2F | `FieldAbility_Open` | the Ability screen's windows |
| `0x58EE40` | R2F | `FieldAbility_ArrangeHow` | the ability sorts' dispatcher (`0x667444`) |
| `0x58F000` | R2F | `FieldAbility_ViewOpen` | the view's window |
| `0x58F050` | R2F | `FieldAbility_Close` | six windows cleared |

**Inbound from outside the group** (for the rebinding pass):

| Caller | Owner | Calls | How |
|---|---|---|---|
| `FieldMenu_Run` | ours | `FieldAbility_Run`, `FieldEquip_Run` | `FieldMenu_States[3]`, `[4]` |
| `0x58AAE0` | R2D | `FieldItems_DiscardConfirm`, `_UseOnMember`, `_ViewList32` | table `0x667328` [6], [7], [9] |
| `0x58B1C0` | R2D | the three arrange steps | `FieldItems_ArrangeSteps` |
| `0x58AAF0` | R2D | `FieldItems_InitWindows` | E8 |
| `0x58B130` | R2D | `FieldItems_CloseWindows` | E8 |
| `0x58EE50`, `0x58EEC0`, `0x58EF60`, `0x58F370`, `0x590020` (2) | R2F | `FieldMenu_SwapBytes` | E8 |
| `PartyForm_Swap` (2), `SharedList_Compact`, `_SortCostDown`, `_SortCostUp` | FS (ours) | `FieldMenu_SwapBytes` | `SH_AT(at::kSwapBytes)`, rebound |
| `Equip_ChooseItem` | FS (ours) | `FieldEquip_PreviewItem`, `FieldEquip_ApplyPreview` | `SH_AT(at::kEquipPreview / kEquipApply)`, rebound |

**Harness rows that list a function of this group**: `scenario_harness.cpp`'s
standard field row `"0x58BD50"` (`FxSwap`, two whole pointers, garbage answer)
matches the reading. It keys on the address, and FS's callers call by the
address (the constant keeps its value), so it stands; under `rest_2e` the
group's own row for the address is registered first.

## 9. The live route

The catalogue's reach columns are empty for all 49, and no file under
`analysis/calltrace` names any of them (grep of every `callcounts.tsv`,
2026-10-04; most were in no traced list). The owner's walk of the field menu
([`menu-screens.md`](menu-screens.md) section 1; `tools/recipes/menu_screens.txt`)
entered the Equipment screen's states 2, 3, 5 and 7 and the Ability screen's
exit (states 9, 8), so that route runs `FieldEquip_TopMenu`,
`FieldEquip_PickMember`, `FieldEquip_Close` and the Ability view's steps; none
of the Items arrange steps, the sorts or the Ability use / arrange states is on
a recorded route. Here: fuzz only. **The coordinator's check** is the state
hash on `menu_screens.txt` (and `shop.txt`, whose field-menu walk opens the
Items screen).

## 10. The rebinding

`band_rows.py --refs --group R2E` and `grep -rn -i` of the 49 over `src/game`:
22 raw references to 5 functions.

| File | Change |
|---|---|
| `field_s_callees.h` | `kSwapBytes` = `bof3::addr::FieldMenu_SwapBytes`, `kEquipPreview` = `bof3::addr::FieldEquip_PreviewItem`, `kEquipApply` = `bof3::addr::FieldEquip_ApplyPreview` (values unchanged; the header already includes `symbols.gen.h`) |

**Left raw, on purpose**: `field_s_fuzz.cpp`'s `CallSite` tables and its two
callee rows `"0x58D640"` / `"0x58D570"` (the disassembly's targets; the rows key
on the address); `scenario_harness.cpp`'s `"0x58BD50"` row (a harness: not this
group's to edit, section 8); comments in `field_s.cpp` and `menu_lists.cpp`
describing the round-twelve state. No file of another group of this round
refers to the 49.

## 11. For `analysis/calltrace/entries_logic.txt`

41 lines appended to the main checkout's file (2026-10-04): the 38 starts it
lacked and the smaller extents `0058BD70 8C`, `0058C2A0 12`, `0058D7B0 D`
beside the catalogue's host lines (`527`, `500`, `1590`, left for the round's
end). The other eight (`0058BC30`, `0058BD40`, `0058BD50`, `0058D0C0`,
`0058D2E0`, `0058D570`, `0058D640`, `0058D700`) were already listed with the
extents read here. R2D's host line for `0x58AAB0`, if it covers the Items
states, is R2D's.

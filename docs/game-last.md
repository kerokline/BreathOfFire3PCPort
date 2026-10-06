# Group TWO: the two Capcom functions left in game code

**Status:** MEASURED (2026-10-06) - the platform round's step 3, group TWO,
from `phase-3/platform-round-2` at `cc7611ab`. **2 functions ours**
(`src/game/game_last.cpp`, shadow name `game_last`): `Item_UseFlags`
`0x591810` and `ItemTrade_Dispatch` `0x593950`, the two the hidden-start scan
found in game code with no catalogue row
([`hidden-start-scan.md`](hidden-start-scan.md) section 4.1). Each read to its
last instruction with capstone and fuzzed through the scenario harness
([`scenario_harness.md`](scenario_harness.md), used unchanged): 8,000 rounds,
0 mismatches; **15 of 15 controls refused**. With them no start of Capcom's
own game code is left that any catalogue or scan names. Not live-checked
here: no trace ever armed either start (section 6); the coordinator's state
hash after the merge is the live check.

## 1. What each does

### 1.1 `Item_UseFlags` `0x591810` (PSX `0x80166918`)

`unsigned Item_UseFlags(unsigned category, unsigned item)`, cdecl, no calls,
no frame: `0x591810..0x591884` and its 4-entry jump table
`0x591888..0x591897` (0x88 bytes). By the category's low byte (`and eax,
0xFF; dec eax; cmp eax, 3; ja default; jmp [eax*4 + 0x591888]`), with `id`
the item's low byte:

| Category | Answer | The record |
|---|---|---|
| 1 | the byte at `0x657461 + 28 * id` | `NameTable_Weapons` `0x657450`, +0x11 |
| 2 | the byte at `0x657D79 + 26 * id` | `NameTable_Armour` `0x657D68`, +0x11 |
| 3 | the byte at `0x658461 + 24 * id` | `NameTable_Accessories` `0x658450`, +0x11 |
| 4 | 0 (`xor al, al` over `eax` 3: `eax` 0) | none |
| 0, 5..255 | the byte at `0x656B38 + 22 * id` | `NameTable_Consumables` `0x656B28`, +0x10 (the u16 flags' low byte) |

`Item_EquipMask` `0x5917A0` (`char_stats.cpp`) is the same shape on the
records' byte +0x10; this is the byte after it for equipment and the flags
word's low byte for consumables. **Only `al` is the answer its callers read**;
the rest of `eax` is what the address arithmetic left: for 1 and 3 the id
(below 0x100, so `eax` is the byte), for 2 `id * 13` (`lea edx, [eax +
eax*2]; lea eax, [eax + edx*4]`), for the default `id * 11` (`lea edx, [eax +
eax*4]; lea eax, [eax + edx*2]`). Ours returns the whole `eax` as the original
leaves it and the fuzz compares all 32 bits.

**Its callers.** A scan of every `E8` / `E9` in `.text` and every dword in
the image for `0x591810`: 18 `E8` sites, nothing else. Every one is in a
function of ours (owner by `symbols.toml`):

| Sites | Function | Ours in |
|---|---|---|
| `0x436857` | `EnemyOp_ReceiveAction` | `enemy_ai_ops.cpp` |
| `0x441BC5` | `BattleObj_HitReceive` | `battle_e3.cpp` |
| `0x448651`, `0x448701`, `0x4487E4`, `0x448924` | `BattleItemCmd_TargetKind`, `_TargetBegin`, `_PickEnemy`, `_PickMember` | `battle_menu_states.cpp` |
| `0x448BC1`, `0x448C5C`, `0x449402`, `0x4494C2`, `0x4495A8`, `0x4496E8`, `0x449932`, `0x4499D0` | `BattleItemCmd_SideBegin`, `_SidePick`, `_EquipUseKind`, `_EquipTargetBegin`, `_EquipPickEnemy`, `_EquipPickMember`, `_EquipSideBegin`, `_EquipSidePick` | `battle_e4.cpp` |
| `0x4532B0`, `0x45413B` | `Battle_ItemSuitsTarget`, `Battle_MemberAutoTarget` | `battle_sprites.cpp` |
| `0x57DA12`, `0x57DA27` | `Item_CanUse` | `menu_windows.cpp` |

None of ours called it by name (it had none); each called the address through
a constant or `Raw<>` (section 5).

### 1.2 `ItemTrade_Dispatch` `0x593950`

`void ItemTrade_Dispatch(void)`, 0xE bytes: `xor eax, eax; mov al, [0x93985C];
jmp [eax*4 + 0x66A470]` - a tail jump through `ItemTrade_States` by the byte
`0x93985C`, unchecked. The scan of the image finds one way in, the `E9` at
`0x52CF30` (`GameMode8_TradeStep`, ours, `effect_1f.cpp`), and no dword
holding the address.

**`ItemTrade_States`' count is 3** (`symbols.toml`'s `count = 3`, round
twelve's FE2), and this group re-established it from the writers: a scan of
`.text` for the address `0x93985C` finds the dispatcher's read and every
write of the byte -

| Site | Function | Write |
|---|---|---|
| `0x52B12B` | `LeaderPanel_S11Switch` (`rest_1g.cpp`) | 0 (the four trade bytes `0x93985C..0x93985F` cleared) |
| `0x5939DA` | `ItemTrade_OpenWait` (`field_e2.cpp`, a step of state 0) | 1 |
| `0x593BDB` | `ItemTrade_PickItem` (`field_e2.cpp`, a step of state 1) | + 1 (cancel: 1 to 2) |
| `0x594174`, `0x5941CD` | `ItemTrade_LeaveAsk` (`effect_1g.cpp`, a step of state 2) | - 1 (2 back to 1) |

So the byte takes 0, 1 and 2, and the table's three cells are
`ItemTrade_Open` `0x593960`, `ItemTrade_Run` `0x5939F0`, `ItemTrade_Leave`
`0x5940F0` (all ours). The fourth dword is `ItemTrade_OpenSteps[0]`
(`ItemTrade_OpenStart` `0x5939A0`): where the original would jump for 3. (A
write through a computed pointer would not show in an address scan; none of
ours writes the byte any other way.)

`eax` at the jump is the state; no state handler reads it (all three are
ours, compiled).

## 2. The PSX twins

`Item_UseFlags`: `0x80166918`, as [`battle_sprites.md`](battle_sprites.md)
section 4 placed it; the sibling's `names/functions.toml` reads a battle
item function's flags from it by the category and the id's low byte, the
same use. `ItemTrade_Dispatch`: not looked for. The trade states' twins are in the
overlay band `0x800F5000` and `ItemTrade_Open`'s (`0x800F5048`) is itself a
table-anchored hypothesis; nothing here settles the dispatcher's.

## 3. The fuzz (`game_last_fuzz.cpp`)

`BOF3X_SHADOW=game_last`, at start-up, through `scenario_harness::Run`. Two
byte copies; neither has an `E8` or `E9` leaving it, so no call is re-aimed.
`Item_UseFlags`' clone carries its jump table, moved into the copy
(`JumpTable {0x12, 0x78, 4}`: the `disp32` at +0x12, the table at +0x78).
`ItemTrade_Dispatch`'s one way out is its indirect jump: `ItemTrade_States`'
three cells are a `DataTable`, swapped for recording handlers while the fuzz
runs, which ours reads in place. Shapes: `Item_UseFlags` `kCall` with
`ret_mask 0xFFFFFFFF` (the whole `eax`); `ItemTrade_Dispatch` `kState`
(which puts the harness in field mode; the log's "lies outside the field
runs" lines for both are the harness naming bases outside its run list, not
a refusal). 4,000 rounds a function.

- **Regions** beyond the standard ones: the item tables from `0x656B38` to
  `0x659C4A` - the farthest byte item 255 of any category reaches
  (accessories, `0x658461 + 24 * 255`) - random every round, so a wrong
  table, stride or byte offset reads different noise; and the trade bytes
  `0x93985C..0x93985F`. Both are put back after the run (the harness's
  `Apply(saved)`).
- **Seeds**: `Item_UseFlags`' category from 0..6, 0x7F, 0x80, 0xFE, 0xFF or
  any byte; the item at 0, 1, each table's last record, its count and one
  past (consumables 92, weapons 83, armour 68, accessories 52: 51..53,
  67..69, 82..84, 91..93), 0x7F, 0x80, 0xFE, 0xFF or any byte - each word with
  random upper bytes half the time (the callers push whole registers).
  `ItemTrade_Dispatch`'s state byte 0, 1 or 2, each a third of the time.
  Past the count ours aborts by design, so a state of 3 or more is a control
  (15 below), not a seed.

Result (2026-10-06, final build):

    shadow      game_last self-test: 8000 rounds over 2 functions (4000 each), 4000 calls to the stand-ins, 0 MISMATCHES; 36162 bytes of state (40 regions) and the stand-ins' log compared
    shadow      game_last coverage (calls the originals made): phase 0x593960 1319, phase 0x5939F0 1316, phase 0x5940F0 1365

Every state reached as a handler by the original (about 1,330 each).

Under `BOF3X_SHADOW='*'` (every module's fuzz, one process), narrow and with
`BOF3X_WIDE=1`, 2026-10-06 at this group's build: both exit 0, `self-test
only: done`, `inject: 10067 ours, 0 left original by BOF3X_ORIGINAL`, 1,058
`MISMATCHES` lines each and every one `0 MISMATCHES`; `game_last` the same
8,000 rounds and 4,000 calls in each. (`battle_e3`, `battle_e4` and
`effect_1f` differ in their call counts from the module run of section 5 by
where the shared random stream stands when each starts, as every round has
seen.)

**What the fuzz cannot see**: anything a state handler does (each function
is tested alone against its copy); a caller's reading of the answer (each
reads `al`: ours returns the whole `eax` anyway).

## 4. Controls

`controls.py` in the group's scratch: each plant anchored on a unique string
of `game_last.cpp` (15: of the fuzz), the DLL rebuilt, the module's shadow
run, the file restored and rebuilt at the end. **15 of 15 refused**, every
one exit 3 (mismatching rounds of 4,000, first differing round of the
alternating 8,000):

| # | Function | Plant | Refused |
|--:|---|---|---|
| 1 | `Item_UseFlags` | weapons by stride 26 | 313 (6) |
| 2 | `Item_UseFlags` | armour without `id * 13` above the byte | 317 (8) |
| 3 | `Item_UseFlags` | accessories one byte early | 331 (16) |
| 4 | `Item_UseFlags` | category 4 answers 3 (no `xor al, al`) | 346 (4) |
| 5 | `Item_UseFlags` | the default's upper bytes `id * 13` | 1,886 (0) |
| 6 | `Item_UseFlags` | the category's low word for its byte | 694 (4) |
| 7 | `Item_UseFlags` | the item's nine bits for its byte | 903 (28) |
| 8 | `Item_UseFlags` | category 0 as category 4 | 324 (30) |
| 9 | `Item_UseFlags` | consumables +0x11 for +0x10 | 2,616 (0) |
| 10 | `Item_UseFlags` | category 3 read from the armour table | 331 (16) |
| 11 | `ItemTrade_Dispatch` | the state masked to one bit | 1,365 (5) |
| 12 | `ItemTrade_Dispatch` | states 1 and 2 swapped | 2,681 (5) |
| 13 | `ItemTrade_Dispatch` | state 0 runs nothing | 1,319 (1) |
| 14 | `ItemTrade_Dispatch` | the state read from `0x93985D` | ours' abort (a random byte of 193 read as the state) |
| 15 | the fuzz | the state seeded below 4, not 3 | ours' abort: "the trade state 0x93985C is 3, past the 3 entries of ItemTrade_States" |

15 is the abort's own control: past the table ours stops with its message
where the original would jump to `ItemTrade_OpenStart`.

## 5. The rebinding and the harness rows

`grep -rn "591810\|593950" src`, each reference read:

- **Rebound** (the round-ten form, the value unchanged - `bof3::addr::<Name>`):
  `battle_e3_callees.h` `kItemClass`, `battle_e4_callees.h` `kItemFlags`,
  `battle_menu_states_callees.h` `kItemFlags`, `enemy_ai_ops_callees.h`
  `kItemClass`, `menu_windows_callees.h` `kItemFlagsOf` (the header now
  includes `bof3/symbols.gen.h` itself; both its includers already did),
  `battle_sprites.cpp`'s `Raw<>(0x591810)` in `kOriginals` (=
  `Item_UseFlags`); `effect_1f_callees.h` `kTradeDispatch` (=
  `ItemTrade_Dispatch`). Every call through them still calls the address, as
  [`round-13-cleanup.md`](round-13-cleanup.md) 1.3's policy keeps them: in
  the game that is the jump Inject put there to ours.
- **The harness rows, to the `_OURS` form**: `scenario_harness.cpp`'s
  `kEffectStd` `FX_RAW(0x593950)` is `FX_OURS(ItemTrade_Dispatch)`;
  `boss_harness.cpp`'s `kEngineStandard` `"0x591810"` row is
  `BH_OURS(Item_UseFlags)`. Masks and answers unchanged (`{kU8, kU8}`,
  `kFlag`: it reads the two low bytes and answers in `al`). The callers by
  address still find them (`StandIn`'s second pass, by the row's address).
- **Left raw on purpose**: the fuzz files' `CallSite` targets
  (`battle_e3_fuzz.cpp`, `battle_e4_fuzz.cpp`, `battle_menu_states_fuzz.cpp`,
  `battle_sprites_fuzz.cpp`, `effect_1f_fuzz.cpp`), `battle_sprites_fuzz.cpp`'s
  mask row and `battle_menu_states_fuzz.cpp`'s `case 0x591810:` stand-in
  selector (the clone keys, [`round-14-cleanup.md`](round-14-cleanup.md)
  1.2); comments that name the address beside what it does (text, not
  references), three of them still saying "nobody's" (`battle_e3_callees.h`
  21, `battle_e4.cpp` 82, `battle_sprites_callees.h` 69) - left for whoever
  edits those files next, to keep this group's lines in files other agents
  touch to the ones it needs.

**The modules whose fuzz reaches the two rows or the rebound constants** -
`battle_e3`, `battle_e4` (the boss harness's row), `effect_1f` (the
`kEffectStd` row), `battle_menu_states`, `battle_sprites`, `enemy_ai_ops`,
`menu_windows` (the constants) - run in one process before and after, at
`cc7611ab` and at this group's build: **every count identical, 0 mismatches**
(`battle_e3` 90,000 and 204,000 rounds, 131,459 and 303,672 calls;
`battle_e4` 360,000, 716,039; `effect_1f` 120,000, 571,154;
`battle_menu_states` 15,000, 25,415; `battle_sprites` 93,600, 141,916;
`enemy_ai_ops` 22,000, 28,115; `menu_windows` 74,000, 3,646,663). The keys'
numeric values did not change, so nothing should move, and nothing did.

## 6. Live coverage

Neither start was ever armed, so no trace names them (`analysis/calltrace`
has no line for either). Their callers are the battle functions of section
1.1 and the field menu's use gate `Item_CanUse` `0x57D9A0`; the trade
screen's dispatcher is game mode 8's step (`GameMode8_TradeStep`). Which
routes reach which caller, and which opens the trade screen, is not read
here. The coordinator's state hash after the merge compares
`.data` frame for frame on every route.

## 7. Latent defects (Capcom's, described, not fixed)

- **`ItemTrade_Dispatch` unchecked**: past `ItemTrade_States`' three cells
  the original jumps through `ItemTrade_OpenSteps`' (state 3 runs
  `ItemTrade_OpenStart` as a state). Every writer keeps the byte in 0..2
  (section 1.2). Ours aborts with a message past the table, as `mode_rest`'s
  dispatchers and R3G's do.
- **`Item_UseFlags`' tables unbounded**: the item's low byte indexes each
  table with no check against its record count (92, 83, 68, 52); item 255
  of accessories reads `0x659C49`, past `Char_ExpTable`'s start. What ids
  the callers pass is not read here. Ours reads the same bytes.

**Needs a ledger entry: none.** Both are faithful replacements; the abort
past the table follows the project's rule for an index past a table (no
reachable state takes it).

## 8. For `analysis/calltrace/entries_logic.txt`

Not appended (the main checkout's file is the coordinator's). Two lines, the
read extents:

```
00591810 88
00593950 E
```

The host line `00593860 100` (`Sprite_ClutWord`, FE2's fix) covers
`0x593950..0x59395F`: cut to `00593860 F0` (its code and table end at
`0x593947`, then eight `nop`), so the dispatcher has its own line.
`005917A0 70` (`Item_EquipMask`) already ends at `0x59180F`.

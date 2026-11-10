# Save block range D: `+0x774..+0xC9F` (`0x904154..0x90467F`)

Status: reading pass, 2026-11-10. Source only (this worktree's `src/game`,
`symbols.toml`, `analysis/data_census.tsv`, docs). No executable, save file or
game data was opened. Block offset = address - `0x9039E0`.

Tags: **READ** = the code that writes or uses the byte was read here.
**CITED** = a note by someone else, not checked in code. **UNKNOWN** = no
access found in the source.

How the range was searched: `data_census.tsv` filtered on column 7 in
`0x904154..0x90467F` (every non-fuzz site followed up); a grep of `src/` for
every literal address in the range; every named data symbol in the range from
`symbols.toml` (`FieldE1_Rank`, `AreaW0b_MapMode`, `AreaW1c_Byte9045FB`,
`BattleMisc_PartyPages`, `Gene_Bits`, `ModeFlow_CountdownA`, `Rest3c_Byte660`)
and their `_addr + n` uses; `Cond_Flags` / `Cond_Flags_addr` with offsets that
reach the range (none found). `Save_BuildBlock` (`rest_2c.cpp:372`) copies
nothing into this range from outside the block, and the slot list reads only
`0x1C` bytes at `+0xCA0` (`save_menu.cpp:335`, `save_menu_callees.h:98`), so
no byte here is gathered at save time or scattered at load. Two whole-range
writers touch everything: the boot clear (`mode_flow.cpp:685`, `memset` of
`0x10B0` bytes at `0x9039E0` = the whole block to 0) and the plain load copy.

## 1. Byte table

| Offset | Address | Size | Meaning | Writers | Readers | Tag | Confidence |
|---|---|---|---|---|---|---|---|
| `+0x774` | `0x904154` | 512 | Item ids, 4 categories x 128: category 0 at `+0x774`, 1 at `+0x7F4`, 2 at `+0x874`, 3 at `+0x8F4`. 0 = empty slot | `Inventory_Add` (`char_stats.cpp:481`), `Inventory_Remove` (`scena_sx.cpp:486`, clears the id when its count reaches 0) | `Inventory_Count` (`char_stats.cpp:530`), `Inventory_CountUsed` (`char_stats.cpp:568`, 128 long below category 4), battle and field item lists, `Inventory_Holds38To4DAt99` (`field_e1.cpp:309`), accessory readers (`effect_1b.cpp:980..1105`, `effect_1f.cpp:92,255` with `kAccessoryIds` `0x9042D4` = `0x904154 + 3 * 0x80`) | READ | High. The base and stride come through the pointer table `Inventory_IdLists` `0x656B00` (`char_stats.cpp:72`; layout in `symbols.toml` evidence, line 293); code constants agree: `rest_2e.cpp:703-705` (`0x904154`, `0x9041D4`, `0x904254`), `effect_1f_callees.h:29`, `field_e1_callees.h:66` |
| `+0x974` | `0x904354` | 512 | Item counts, parallel to the ids, same category order. 1..99 | `Inventory_Add` (caps at `0x63`, `char_stats.cpp:495`), `Inventory_Remove` | `Inventory_Count` (the first stack of the id), `field_e1.cpp:309`, `effect_1b.cpp:988..1085` (`kItemCounts` `0x9044D4` = category 3) | READ | High (`Inventory_CountLists` `0x656B14`, `char_stats.cpp:73`; `field_e1_callees.h:67`) |
| `+0xB74` | `0x904554` | 32 | Key items, ids only, no counts. 0 = empty | `KeyItem_Add` (`scena_sx.cpp:469`, first 0 of 32), `KeyItem_Remove` (`scena_sx2.cpp:253`), `Inventory_Add(4, ..)` (`char_stats.cpp:505-511`, but 128 long: D34), `Inventory_Remove(4, ..)` (128 long), `Scena08_SwapKeyItem4` (`scena_sc7.cpp:1370`, 4 -> `0xE`), `Area39_SwapKeyItemE` (`area_w1a.cpp:260`, `0xE` -> 4) | `KeyItem_Has` (`char_stats.cpp:580`, 32), `Inventory_CountUsed(4)` (32) | READ | High |
| `+0xB94` | `0x904574` | 128 | The **shared ability list** (the spec's "skill notes"): 128 ability ids, each an index into `Ability_Records` (24-byte records at `0x65C4C8`). 0 = empty; gaps allowed | `AbilityList_Add(id, 0, 1, 0)` (`scena_sx.cpp:446`, first 0 of 128) called from: `SharedList_PickSlot` (`field_s.cpp:1490`, a member's slot sent to the list), `FieldAbility_ShareConfirm` (`rest_2e.cpp:1608`, the field ability menu's yes), `Scena02_StripMember` (`scena_sc2.cpp:2464`, all ten bytes of members 3 and 4 sent to the list). Swaps: `SharedList_PickShared` (`field_s.cpp:1541-1542`, entry <-> a member's record `+0x7E` slot), `SharedList_Arrange` (`field_s.cpp:1661-1664`, two entries). Sorts: `SharedList_Compact` (`field_s.cpp:1857`), `SharedList_SortCostDown` / `CostUp` (`field_s.cpp:1882,1889`, by `Ability_Records + 0x12`). Also D34's 33rd key item (below) | `SharedList_DrawList` (`field_s.cpp:1744`, "n/128"), `AbilityList_CountSet(.., which > 3, ..)` (`rest_2f.cpp:1087-1101`), `MasterWin_SkillKnown` (`rest_2h.cpp:576-583`: an id here counts as known) | READ | High |
| `+0xC14` | `0x9045F4` | 1 | `FieldE1_Rank`: the rank (0..12) of a points total that a field panel draws | `FieldPanel_DrawTotal` (`field_e1.cpp:220`) | `Area113_ChoiceAsk` (`area_w2f.cpp:1222`, >= 6), `Area113_Reward` (`area_w2f.cpp:1331`, 0xA / 0xB / 0xC give items and set event flags 0x96 / 0x97) | READ | High. The original also loads it as a dword masked with `0xFF` (`symbols.toml` evidence for `0x9045F4`) |
| `+0xC15` | `0x9045F5` | 5 | No access found (to `+0xC19`) | none | none (the masked dword load of `+0xC14` above covers `+0xC15..+0xC17` and discards them) | UNKNOWN | - |
| `+0xC1A` | `0x9045FA` | 1 | `AreaW0b_MapMode`: the world map's HUD mode, 0..2. 0: frame and region box shown; 2: frame held off (`Area16_FrameHold`, `WorldMap_FrameWait`); non-zero hides the region box (`BoxLeaves`) | `Field_LeaderCellEvent` (`event_ops.cpp:1234-1237`: pad `0x100` on the field cycles 0, 1, 2, 0) | `area_w0b.cpp:72,322,336,397` and the same machine in `area_w1b`, `area_w1e`, `area_w2b`, `area_w2e`, `area_w3a`, `area_w3b`, `area_w3g`, `area_w4a`, `rest_3a.cpp:119,136`, `world_map.cpp:155,169`, `worldmap_area.cpp:123,384,457` | READ | High |
| `+0xC1B` | `0x9045FB` | 1 | Settled (battle count up to `0x1E` while master 4 is met and not open) | `Field_AfterBattleTally` (`field_e2.cpp:203-204`), `Party_RestoreAll` (`save_menu.cpp:529`) | `area_w1c.cpp:646` | READ | Settled, re-confirmed |
| `+0xC1C` | `0x9045FC` | 9 | Battle list-window memory, 3 bytes per battle-party slot 0..2 (`+0xC1C`, `+0xC1F`, `+0xC22`): page, scroll, cursor | `Party_JoinReset` (`field_event.cpp:366-369`, all 9 to 0; called by `Party_Remove` `scena_sx.cpp:234` and, per `symbols.toml` line 5837, by `Party_Join`); `KeepPage` in `BattleItem_Browse` (`battle_actor_copies.cpp:161-165`, 371, 385); `BattleFx_RestoreParty` (`battle_e2.cpp:440-445`, the scroll and cursor bytes, `kPlaceCells` `0x9045FD`); `FieldMenu_ReconcileParty` (`menu_lists.cpp:411-430`: each triple moves with its character when the battle party `0x904065` changed in the menu, or becomes 0) | `ItemMenu_SetupForActor` (`battle_misc.cpp:347-352`: window record 16's page, `+0x10` and cursor) | READ | High |
| `+0xC25` | `0x904605` | 3 | `BattleMisc_PartyPages`: the party item list's saved page (category 0..3), top row and cursor | `BattleItemCmd` list state (`battle_menu_states.cpp:364,369-370` on cancel; `389-391` on a use) | `ItemMenu_SetupForParty` (`battle_misc.cpp:377-383`) | READ | High |
| `+0xC28` | `0x904608` | 24 | Dragon **history**: 6 rows of 4 bytes, newest first. Bytes 0..2: gene numbers (0..17), packed, `0xFF` after the last; byte 3: the form, `(group << 5) + (form & 0x1F)`; `0xFF` in byte 3 = empty row | `Scena00_Start` (`scena_sc0.cpp:138`, all 18 dwords to `0xFFFFFFFF` at the new game); `DragonForm_Transform` (`battle_e6.cpp:248-260`: rows 0..4 shifted to 1..5, the genes used into row 0, byte 3 the form) | `DragonCmd_SlotsCursor` / `SlotAffordable` / `SlotsConfirm` -> `TakeSlot` (`battle_e5.cpp:756-806,533`; reuse a row's genes); `DragonCmd_StoreSource` / `StoreCopy` (`battle_e5.cpp:1184-1224`); `GeneWin_DrawList` (`battle_e7.cpp:529-570`); `BattleFx_Win18Cursor*` (`battle_e2.cpp:691-696`, a gene byte as the cursor sprite's animation) | READ | High |
| `+0xC40` | `0x904620` | 48 | Dragon **stored list**: 12 rows, same 4-byte layout | `Scena00_Start` (as above); `DragonCmd_StoreCopy` (`battle_e5.cpp:1184-1188`: a history row copied over the chosen row, after an overwrite prompt when byte 3 is not `0xFF`) | `DragonCmd_Slots2*` / `Slot2Affordable` / `Slots2Confirm` (`battle_e5.cpp:1027-1068`), `DragonCmd_StoreCursor` (`battle_e5.cpp:1133`), `GeneWin_DrawList2` (`battle_e7.cpp:589-606`), `BattleFx_Win19Cursor*` (`battle_e2.cpp:707-712`) | READ | High |
| `+0xC70` | `0x904650` | 3 | Settled: dragon genes, 18 bits (`Gene_Bits`) | `Flags_Set` sites: `scena_sc3.cpp:305,1229`, `scena_sc7.cpp:1814-1815`, `FieldTail_FlagMessage` (`field_e2.cpp:1362`) | `DragonCmd_Open` (`battle_e5.cpp:625`), `MenuList_GeneWinDraw` (`rest_2h.cpp:257`), `EventCond_Gene` (`field_o.cpp:1317`), `Area143_ChoiceAsk52` (`area_w3f.cpp:297`) | READ | Settled, re-confirmed |
| `+0xC73` | `0x904653` | 1 | The **high byte of the `Gene_Bits` dword**. No writer sets a bit here | none found (every `Flags_Set` index read is 0..0x11; `FieldTail_FlagMessage`'s index is area data, unbounded in code) | read as part of the dword by `Area143_ChoiceAsk52` (`area_w3f.cpp:297`, `== 0x3FFFF`), `DragonCmd_Open` and `MenuList_GeneWinDraw` (both only look at bits 0..17) | READ | High |
| `+0xC74` | `0x904654` | 6 | Settled: masters met (`+0xC74`, 3) and masters open (`+0xC77`, 3), 17 bits each | `MasterTalk_IntroSay` / `IntroWait` (`rest_2c.cpp:735,746`), `Area98_ChoiceFlag9` (`area_w2d.cpp:332`), `scena_sc9b.cpp:952` | `MasterTalk_Begin` (`rest_2c.cpp:719`), `MasterWin_DrawList` (`rest_2h.cpp:477`), `Field_AfterBattleTally`, `Party_RestoreAll` | READ | Settled, re-confirmed: the highest bit any code tests is 16, so the rows end at `+0xC79` |
| `+0xC7A` | `0x90465A` | 2 | No access found | none | none | UNKNOWN | - |
| `+0xC7C` | `0x90465C` | 4 | `ModeFlow_CountdownA`: a countdown as hours, minutes, seconds, frames (the play clock's layout). Not all zero = ability `0x3E` refused in battle. Set after the effect whose end routine the source calls `Celerity_End` | `Celerity_End` (`magic_s12.cpp:838`, byte 0 = 5, so 5:00:00:00); `CountDown` from `Game_ClockTick` (`mode_flow.cpp:443-462,491`) | `Skill_CanUse` mode 2 (`battle_window_draw.cpp:486-489`) | READ | High |
| `+0xC80` | `0x904660` | 4 | `Rest3c_Byte660`: the same kind of countdown. Not all zero = ability `0x15` refused in battle. Set by the damage-doubling effect | `Effect_DoubleDamage` (`rest_3c.cpp:204`, byte 0 = 5); `CountDown` (`mode_flow.cpp:492`) | `Skill_CanUse` mode 2 (`battle_window_draw.cpp:482-485`) | READ | High |
| `+0xC84` | `0x904664` | 28 | No access found (to `+0xC9F`) | none | none | UNKNOWN | - |

## 2. The current spec rows (`save-editor-spec.md` section 7)

| Spec row | Verdict |
|---|---|
| `+0x774` 512 item ids, 4 x 128 | **Confirmed** in code (bounds and category order above). Category order: 0 consumables, 1 weapons, 2 armour, 3 accessories (`char_stats.cpp:588-589`, `rest_2e.cpp:703-705`, `effect_1f_callees.h:29`) |
| `+0x974` 512 item counts | **Confirmed**; counts cap at 99 |
| `+0xB74` 32 vital items | **Confirmed**: 32 bytes, ids only |
| `+0xB94` 128 "skill notes", CITED | **Confirmed in size, renamed**: the shared ability list, 128 ability ids. Now READ. "Skill notes" is not a name the code supports; "shared ability list" is the source's (`field_s.cpp:1166`) |
| `+0xC14` 92 unnamed; `+0xC28` eight rows, `+0xC48` 40 copies | **Corrected**: `+0xC14` rank, `+0xC15..C19` unknown, `+0xC1A` map HUD mode, `+0xC1B` settled, `+0xC1C..C27` battle window memory, `+0xC28..C6F` the two dragon lists. The real split is **6 history rows `+0xC28..C3F` and 12 stored rows `+0xC40..C6F`**, not 8 + 40 bytes; the measured shape is consistent with history in use and stored rows 0 and 1 in use on the samples, but that was not re-measured here |
| `+0xC70` 3 genes, CITED | **Confirmed** (READ): 18 bits; the field is the low 3 bytes of a dword |
| `+0xC73` 1 unnamed, UNKNOWN | **Corrected**: the dword's high byte, never written, read by the `== 0x3FFFF` test |
| `+0xC74`, `+0xC77` masters | **Confirmed** |
| `+0xC7A` 2 unnamed | **Still unknown**: no access in the source |
| `+0xC7C` 8 Celerity and Bonebreak timers, CITED | **Confirmed and corrected in detail** (READ): two separate 4-byte countdowns, `+0xC7C` gating ability `0x3E` and `+0xC80` gating ability `0x15`. "Celerity" matches the source's routine name; "Bonebreak" is not in the source (the code names the effect `Effect_DoubleDamage`), so that name stays CITED |
| `+0xC84` 28 unnamed | **Still unknown**: no access in the source |

## 3. What a save editor may edit

- **Items `+0x774..+0xB73`.** Edit as (id, count) pairs per category. Keep
  one stack per id in a category: `Inventory_Add` merges into the first match
  and `Inventory_Count` / `Inventory_Remove` use the first match only. Count 1..99
  with a non-zero id. An empty slot is id 0 with count 0; `Inventory_Add` also
  treats a count of 0 as free (`char_stats.cpp:512`). Item id ranges per
  category are table bounds not read here.
- **Key items `+0xB74..+0xB93`.** Ids only; gaps are fine (add takes the first
  0). Do not fill all 32: then `Inventory_Add(4, ..)` (`area_w0b.cpp:981`,
  `scena_sc2.cpp:1907`) writes into `+0xB94` (D34).
- **Shared ability list `+0xB94..+0xC13`.** Ability ids 1..227 (228 records,
  `symbols.toml` note on `Ability_Records`), gaps fine. The game moves ids
  between this list and members' record `+0x7E` lists and never copies them,
  so an editor should keep an id in one place; `MasterWin_SkillKnown` treats
  any copy here as known.
- **`+0xC14` rank**: 0..12 are the values the writer produces.
- **`+0xC1A` map mode**: 0..2. Other values act as "non-zero, not 2" until
  the next press resets them.
- **`+0xC1C..+0xC27` window memory**: safest 0 (what `Party_JoinReset` writes).
  The party page `+0xC25` must stay 0..3, top `+0xC26` at most `0x79` and cursor
  `+0xC27` at most `0x7F` (`battle_menu_states.cpp:232-241`, the list state's comment); a larger page
  would index the item pointer tables past their end.
- **Dragon lists `+0xC28..+0xC6F`**: a row is either all `0xFF` (empty) or
  genes packed from byte 0 (`0xFF` after the last) with byte 3 not `0xFF`. **Never
  0-fill**: the boot clear leaves 0, which reads as three gene-0 entries with
  form 0 in every row; only `Scena00_Start` turns them into `0xFF`. A new-game
  generator must write `0xFF` (spec 12.1 step 5 is right). Rows are not
  checked against `Gene_Bits` when reused (no test found in `TakeSlot`,
  `DragonCmd_SlotsConfirm`, `DragonCmd_Commit`), so an editor should only use
  genes the save holds. Byte 3 is display only (icon, label); the form is
  recomputed at each transformation.
- **Genes `+0xC70..+0xC73`**: edit bits 0..17 only. Bits 18..31, including
  all of `+0xC73`, must stay 0, or area 143's `== 0x3FFFF` test fails with all
  18 genes held.
- **Countdowns `+0xC7C..+0xC83`**: all zero = the ability is usable. Writing
  zeros is safe. Non-zero values must keep minutes and seconds below 60 and
  frames below 30.
- **Unknown bytes** `+0xC15..+0xC19`, `+0xC7A..+0xC7B`, `+0xC84..+0xC9F`:
  preserve.

Agreements between bytes: the count bytes go with the id bytes at the same
index; `+0xC1C..+0xC24` go with the battle party list at `0x904065`
(`FieldMenu_ReconcileParty`); `+0xC73` with the gene bits above. Nothing in this
range is a checksum input other than through the block sum.

## 4. Open questions and possible defects

1. **The `Game_ClockTick` note in `symbols.toml` (line 7601) looks wrong.** It
   says the two countdowns end "on 0:00:00 with the frames reloaded - so never
   all zero again". `CountDown` (`mode_flow.cpp:443-462`) returns at once on
   all-zero, and with in-range bytes the `hours == 0` minute-underflow branch
   can only be reached from all-zero. So the countdown does reach 0:00:00:00 and
   stops, and the ability becomes usable again. The branch is reached only with
   out-of-range bytes (a frame or second byte of `0x81` or more). Worth a check
   against the original's instructions; if the note is right, the abilities
   would stay locked forever after one use, which play would show.
2. **The countdown length.** Both writers set the hours byte to 5. On the
   play-clock scale (`Game_ClockTick` ticks frames to 30 a second) that is five
   hours. Whether `Game_ClockTick` runs every frame in battle and on the field
   was not read; the owner's play would settle the real duration.
3. **D34 reaches this range.** A 33rd key item lands in the shared ability
   list's first empty entry at `+0xB94..` and would then show as an ability
   id there. `Inventory_Remove(4, id)` also scans 128 bytes and could clear a
   matching ability id in that list when the key item is not held.
4. **`RowCost` with an empty gene byte 0.** `RowCost` (`battle_e7.cpp:128`) and
   `Affordable` (`battle_e5.cpp:522`) always read byte 0's cost, even when it
   is `0xFF`. `DragonForm_Transform` writes `0xFF` to bytes `count..2`, so a
   transformation with a gene count of 0 would make such a row. Not known to
   happen in play; an editor must not make one.
5. **`TakeSlot` (`battle_e5.cpp:533`) keeps each gene at its own index but
   counts only the non-`0xFF` ones.** A row with a gap (for example
   `g, 0xFF, g2`) gives a count of 2 with genes at indices 0 and 2, so the
   transformation reads a stale index 1. The game only writes packed rows.
6. **Who reads `+0xC15..+0xC19`, `+0xC7A..+0xC7B`, `+0xC84..+0xC9F`?** No
   code in the source. On the PSX the slot list reads the file from block
   `+0xC80` for `0x80` bytes (sibling `docs/BATTLE_RAM.md`, around its line 545),
   so the PSX may have used `+0xC84..+0xC9F` there; the PC reads only `+0xCA0`.
   Sample values were not checked here.
7. The `FieldAbility_ShareConfirm` path takes the ability from the current
   page of the field ability window (`rest_2e.cpp:1606`); which pages offer
   the share was not traced.

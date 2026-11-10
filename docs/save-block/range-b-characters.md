# Save block range B: the eight character records, +0x090..+0x5AF

**Status:** READ pass, 2026-11-10. Source and docs only (no executable, no
save file opened). Working file for the save-block map
([`BRIEF.md`](BRIEF.md)).

**Range:** block `+0x090..+0x5AF`, addresses `0x903A70..0x903F8F`: eight
records of stride `0xA4` (`records::CharRecord`, `src/game/records.h:606`;
symbols.toml `CharacterRecords`). Record *n* byte *off* is at

- block offset `+0x90 + n * 0xA4 + off`,
- address `0x903A70 + n * 0xA4 + off`,
- PlayStation record offset `off - 4` for `off >= 0x09`; PC `+0x00..+0x08`
  (the 9-byte name) is PSX `+0x00..+0x04` (5 bytes). PC `+0xA0..+0xA3` has
  no PSX twin; PSX `+0xA0..+0xA3` has no PC home
  ([`../save-interchange.md`](../save-interchange.md) lines 49-59).

Tags: **READ** - I read the code that writes or uses the byte; **CITED** -
someone's note, not checked in code; **UNKNOWN** - no code gives the byte a
meaning. "Bulk copy" means the byte is only moved as part of a whole record
or block (`memcpy` of `0xA4` or `0x20` bytes, the save itself).

Every record is copied whole, both ways, by these, so every byte below is
touched by them and they are not repeated in each row:

- `NewGame_InitCharacters` `0x437820` (`src/game/rest_3a.cpp:828`): records
  0..6 from `Char_DefaultRecords` `0x64B390 + n * 0xA4`, then the whelp's
  record `0x64B80C` over record `Char_WhelpSlot` (7).
- `Party_SwapMembers` (`area_entry.cpp:230`), `Party_HealJoined`
  (`scena_sx.cpp:218-219`), `Area192_RestoreCharacters` (`area_w4f.cpp:413-418`),
  `RestoreRecords` (`scena_sc13.cpp:2636-2641`), `Field_SwapExchange`
  (`event_leader.cpp:458`), `Battle_ReloadPartyRecords` `0x446600`
  (`battle_e4.cpp:405-410`): record -> a party member's working copy
  `ObjTrio + 0x14C * i + 0x80` (`records::PartyMember`, `records.h:191`).
  **No function copies a working copy back whole.** The only write-back is
  `Battle_WriteBackMember` (below, `+0x10`, `+0x18`, `+0x1A`, `+0x1C`).
- `Equip_PreviewSlot` / `Equip_PreviewSet` (`char_stats.cpp:450`, 461):
  record -> a stack copy, recomputed, compared; the record is not written.
- `Save_BuildBlock` (`rest_2c.cpp:372`) sums the block with the records in it
  and transforms none of them.

## 1. One record, byte by byte

Offsets are record 0's: block `+0x90 + off`, address `0x903A70 + off`. Add
`n * 0xA4` for record *n*.

| PC off | Block (rec 0) | Address (rec 0) | PSX off | Size | Field | Writers | Readers | Tag | Conf. |
|---|---|---|---|--:|---|---|---|---|---|
| `+0x00..+0x08` | `+0x090` | `0x903A70` | `+0x00..+0x04` | 9 | **Name**, glyph codes of the install's font, byte 8 zero on every sample (spec 9) | default records (NewGame); name entry `0x45F5A0` writes 8 bytes (CITED, [`save-interchange.md`](../save-interchange.md):106, disasm, not in `src/`); `BattleExtra_EquipOpen` copies 5 bytes `0x669CE8` over **record 7's** name and `BattleExtra_EquipLeave` puts `0x669CE0` back (`battle_e1.cpp:534-536`; DIV-0118, DIV-0020) | `MasterScreen_NameToText` / `MasterJoin_Step5Apply` copy 8 + byte 8 (`rest_2d.cpp:292`, 332-335); `Save_BuildBlock` copies 5 to the summary `+0xCA0` and 4 to `0x904696` (`rest_2c.cpp:391-393`); `Menu_DrawMemberStatus` draws 5 (`map_field_objects.cpp:475`) | READ | high |
| `+0x09` | `+0x099` | `0x903A79` | `+0x05` | 1 | **`char_id`**: the character the record holds (`PartyMember::char_id` +0x89 after the copy) | default records; `Scena01_Set934Party9` writes 9 into the record of `Character_IndexById[9]`, `Scena01_Party034` 0 into that of `[0]` (`scena_calls.cpp:90`, 97) | portraits `Menu_DrawItemIcon(+9)` (`map_field_objects.cpp:475`, `field_s.cpp:703`, `rest_2h.cpp:181`, `menu_windows.cpp:1276`, `rest_4e.cpp:391`); record 0's compared with 9 in eight areas' `Record4MarkCell` (`area_w0b.cpp:552` and siblings) and `Area33_Record04Start` (`area_w0c.cpp:495`); `PartyForm_Reload` (`field_s.cpp:553`) | READ | high |
| `+0x0A` | `+0x09A` | `0x903A7A` | `+0x06` | 1 | **Level** 1..99 | `Char_LevelUp` `0x498DE0` from the EXP (`scena_sx.cpp:107-138`) | `Char_LevelUpGain` (`battle_e1.cpp:885`), `BattleResult_LevelUpNotice` (830), `MasterJoin_Step5Apply` (copied to `+0x88`), `MasterGrant_Step1Member` (`rest_2d.cpp:468`), `NewGame_InitCharacters` (willpower, `rest_3a.cpp:832`), `Menu_DrawMemberStatus`; record 0's to the summary level (`rest_2c.cpp:397`, `0x903A7A`) | READ | high |
| `+0x0B` | `+0x09B` | `0x903A7B` | `+0x07` | 1 | **Flags.** Bit 0 *joined*: `Inventory_Count` counts only these wearers (`char_stats.cpp:547`), `Party_HealJoined`, `Area192_RestoreCharacters`, `RestoreRecords`, the community members' count (`rest_4e.cpp:435-457`), `TacticsMembers_Build` (`rest_2f.cpp:879`), script condition 15 (`field_o.cpp:1312`). Bit 1 *fixed in the party*: `PartyForm_Swap` refuses to move such a member (`field_s.cpp:767-768`), the reserve lists draw it dimmed (`field_s.cpp:798-800`, `rest_2h.cpp:177`), `Party_AddToLists` will not replace it (`scena_se.cpp:210`). Bit 2: cleared after a fight for a member left KO (`BattleEnd_Finish`, `battle_turn_steps.cpp:497`); **no setter found**. Bits 3..7: no access found | bit 0 set / cleared by scenario code (`scena_calls.cpp:87`, 120-122, 172, 358, 375, 676, 850; `scena_sc1.cpp:557`; `scena_sc5.cpp:456-508`, 1818-1820 for records 1, 5, 6; `scena_sc6.cpp:317-328`, 673-674); `Party_AddToLists` sets bits 0 and 1 (`scena_se.cpp:198-199`); bit 1 set / cleared in `Scena08_Scene11` (`scena_sc7.cpp:2491-2493`, 2569-2571), `scena_sc6.cpp:1540` | as left | READ | high (bits 0, 1); bit 2 partial |
| `+0x0C..+0x0F` | `+0x09C` | `0x903A7C` | `+0x08..+0x0B` | 4 | **EXP**, s32, compared signed | `BattleResult_AddExp` `0x4468B0` adds, capped at 9,999,999 (`rest_3b.cpp:144-159`) | `Char_LevelUp` (`scena_sx.cpp:112`), `Char_LevelUpGain` (`battle_e1.cpp:889`), `Menu_DrawMemberStatus` (EXP bar); record 0's to the summary EXP (`rest_2c.cpp:400`, `0x903A7C`) | READ | high |
| `+0x10..+0x11` | `+0x0A0` | `0x903A80` | `+0x0C..+0x0D` | 2 | **Status word** (u16). Bits seen in code: `0x4000` KO (`Encounter_PartyTurn` `inventory_ops.cpp:707`; `ItemUse_Revive` clears it, `item_use.cpp:196-198`); `0x2000` HP below a quarter of max - rewritten each time the status entry draws (`map_field_objects.cpp:490-491`), by `Field_FloorHurt` and cleared by `Char_HealHp` (`item_use.cpp:302`) ([`../spell-sweep.md`](../spell-sweep.md) 9.2); `0x80` and `0x20`: the two the status entry names (texts `0x66A0E8` / `0x66A0F0`, `map_field_objects.cpp:480-486`); `0x80` makes the field walk at pace 3 (`Field_LeaderSetPace`, `event_ops.cpp:456`), `0x20` stops the idle fidget (`Field_LeaderIdleTest`, `event_ops.cpp:668`); `0x08`, `0x100`, `0xA0` the cure masks of items 10, 11, 12 (`item_use.cpp:151-154`). Bit meanings by name are not in the source | `Battle_WriteBackMember` `0x446A80` keeps only **`0x60A0`** of the working copy and writes it here (`battle_e4.cpp:505-518`); `Char_ClearStatus` `0x590F60` (`item_use.cpp:366-372`); `Party_HealJoined`, `Area192_RestoreCharacters`, `RestoreRecords` write 0; `Party_RestoreAll` clears `0xA0` (full) and `0x2000`; `BattleEnd_Finish` clears `0x4000`; `Field_FloorHurt` ORs `0x80` / `0x20` / `0x2000` (`field_e2.cpp:289-297`); `Scena08_Scene11` ORs `0x6000` with HP 0 (`scena_sc7.cpp:2494-2495`) | as above; `MasterPanel_DrawMember` (`rest_2c.cpp:946-960`) | READ | high (which bits persist); names UNKNOWN |
| `+0x12` | `+0x0A2` | `0x903A82` | `+0x0E` | 1 | **Weapon** id (0 none: `GuestEquipped`, `scena_sc6.cpp:963`) | `Shop_Equip` `0x583210` (`save_menu.cpp:672`), `Equip_ChooseSlot` (`field_s.cpp:1983`), `BattleExtra_EquipCommit` for record 7 (`rest_3a.cpp:263-276`), `Scena06_GuestRecord` record 7 (`scena_sc6.cpp:261`); gifts into record 7 (`scena_sc6.cpp:1942`, `scena_sc7.cpp:1136`) | `Char_ApplyWeapon` (`char_stats.cpp:266`), `Inventory_Count`, `Actor_EquipCount` (`event_objs.cpp:445`) | READ | high |
| `+0x13..+0x15` | `+0x0A3` | `0x903A83` | `+0x0F..+0x11` | 3 | **Three armour** ids (which slot is which is not in the code) | as `+0x12` (`Shop_Equip`'s 5-entry table); record 7's `+0x15` by `Scena06_GuestRecord` (`scena_sc6.cpp:262`) | `Char_ApplyArmour` (copies the three first), `Inventory_Count` | READ | high |
| `+0x16..+0x17` | `+0x0A6` | `0x903A86` | `+0x12..+0x13` | 2 | **Two accessory** ids | as `+0x12` | `Char_ApplyAccessories`, `Inventory_Count`, `BattleResult_ZennyBonus` (working copy, `battle_e1.cpp:861`) | READ | high |
| `+0x18..+0x19` | `+0x0A8` | `0x903A88` | `+0x14..+0x15` | 2 | **HP** (u16) | `Char_RecalcStats` **cuts it to max HP** (`char_stats.cpp:226-227`); `Char_HealHp`; `Battle_WriteBackMember`; `Party_RestoreAll`, `Party_HealJoined`, `Area192_RestoreCharacters`, `RestoreRecords` set it to `+0x20`; `BattleEnd_Finish` 1 for a KO member; `Area189_DrainHp` (`area_w4e.cpp:820-829`); `ItemUse_Revive` 1; `Scena06_GuestRecord` record 7 | everywhere HP is shown or tested | READ | high |
| `+0x1A..+0x1B` | `+0x0AA` | `0x903A8A` | `+0x16..+0x17` | 2 | **AP** (u16). **Not cut by `Char_RecalcStats`** | `Char_HealAp` (`item_use.cpp:307`), `Battle_WriteBackMember`, the restore functions (= `+0x22`) | menus, battle | READ | high |
| `+0x1C` | `+0x0AC` | `0x903A8C` | `+0x18` | 1 | **Current willpower**: in battle a kill whose overkill is below it sets the member's flag bit 2 (`battle_damage.cpp:594`, 748); halved on a revive (`BattleObj_Revive`, `battle_e3.cpp:797`) | `NewGame_InitCharacters` (derived from the level, `rest_3a.cpp:837`); `Party_RestoreAll` full (`save_menu.cpp:517`), `Party_HealJoined` (`scena_sx.cpp:212`), `RestoreRecords` (`scena_sc13.cpp:2627`) = `+0x2E`; `Battle_WriteBackMember` (`battle_e4.cpp:513-516`); `BattleEnd_Finish` = 1 for a KO member (`battle_turn_steps.cpp:495`) | battle damage (working copy `+0x9C`) | READ | high |
| `+0x1D` | `+0x0AD` | `0x903A8D` | `+0x19` | 1 | **Halving bits**: each set bit steps one resistance byte by +2 in `Char_RecalcStats` - bits 0..4 `+0x2F..+0x33`, bit 5 `+0x35`, 6 `+0x36`, 7 `+0x37` (`+0x34` has none) (`char_stats.cpp:228-230`) | `Party_RestoreAll` clears it, twice (`save_menu.cpp:519`, 524). **No setter found in `src/`** | `Char_RecalcStats` | READ | high (reader); setter UNKNOWN |
| `+0x1E` | `+0x0AE` | `0x903A8E` | `+0x1A` | 1 | **Fatigue (max-HP scale)**: `Char_RecalcStats` takes `(scale * base max HP + 5) / 10` off max HP (`char_stats.cpp:224-225`); the max HP is drawn in colour 4 when it is not 0 (`map_field_objects.cpp:499`) | **+1 below 5** on a KO left after a fight (`BattleEnd_Finish`, `battle_turn_steps.cpp:496`) and by battle effect slot 106 (`Effect_ActorStepThenHpToOne`, `rest_3c.cpp:684`); **+1 below 9** by effect slot 118 (`rest_3d.cpp:496-497`), area 189's walk (`Area189_RaiseByte1E`, `area_w4e.cpp:809-815`, records 0..6) and item handler 22 (`ItemUse_HpScaleUp`, `item_use.cpp:241-258`); 0 by `Party_RestoreAll` full (the inn) and `Party_HealJoined`; chapter 13 sets record `[8]`'s to 9 and later 0 (`scena_sc13.cpp:1867`, 2012) | `Char_RecalcStats`; `Area191_Init` / `Area192_Init` read record 0's (`area_w4e.cpp:1079`, `area_w4f.cpp:372`), `Area191_TalkMessage` / `Area192_TalkMessage` the speaker's (`area_w4e.cpp:1048`, `area_w4f.cpp:337`) | READ | high |
| `+0x1F` | `+0x0AF` | `0x903A8F` | `+0x1B` | 1 | **Master**, `0xFF` none | `MasterJoin_Step5Apply` = the master (`rest_2d.cpp:334`), `MasterQuit_Step5Apply` `0xFF` (400) | `Char_ApplyTraits` picks the master's bonus list at `0x667548` (`char_stats.cpp:244-259`); `MasterScreen_PickMember`, `MasterJoin_Step7AllCheck`, `MasterQuit_Step7NoneLeftCheck`, `MasterGrant_Step1Member`, `FirstMember` (`rest_2c.cpp:770`); `Menu_DrawStatsPanel` prints system message `0x111 +` it (`field_o.cpp:164-168`) | READ | high |
| `+0x20..+0x21` | `+0x0B0` | `0x903A90` | `+0x1C..+0x1D` | 2 | **Max HP**, effective. **Derived** | `Char_RecalcStats` (base `+0x40` less the fatigue share); `ItemUse_MaxHpUp` beside the base (`item_use.cpp:206`); `BattleExtra_ApplyTally` record 7 (`battle_e1.cpp:264`) | every HP cap | READ | high |
| `+0x22..+0x23` | `+0x0B2` | `0x903A92` | `+0x1E..+0x1F` | 2 | **Max AP**, effective; equals `+0x42` after a recompute (no pass touches it) | `Char_RecalcStats` copy; `ItemUse_MaxApUp` (`item_use.cpp:211`) | AP caps | READ | high |
| `+0x24..+0x25` | `+0x0B4` | `0x903A94` | `+0x20..+0x21` | 2 | **ATK**, effective. Derived | copy + weapon power, armour, accessories (`Stat_AddClamped`, 0..999) | battle stat copy `0x939F80 + 4`; `Menu_DrawStatsPanel` | READ | high |
| `+0x26..+0x27` | `+0x0B6` | `0x903A96` | `+0x22..+0x23` | 2 | **DEF**, effective. Derived | copy + armour | stat copy `+6` (`kTargetDef` `0x939F86`) | READ | high |
| `+0x28..+0x29` | `+0x0B8` | `0x903A98` | `+0x24..+0x25` | 2 | **AGI**, effective. Derived | copy - every item's weight | battle order | READ | high |
| `+0x2A..+0x2B` | `+0x0BA` | `0x903A9A` | `+0x26..+0x27` | 2 | **INT**, effective. Derived | copy + items | stat copy `+0xA` | READ | high |
| `+0x2C..+0x2D` | `+0x0BC` | `0x903A9C` | `+0x28..+0x29` | 2 | A **word**, effective copy of `+0x4C`. Derived | copy; armour `0x28` adds 1 ([`../char-stats.md`](../char-stats.md) 2) | **no reader found** | READ (shape only) | low |
| `+0x2E` | `+0x0BE` | `0x903A9E` | `+0x2A` | 1 | **Willpower**, effective, 0..99 (`Stat_AddCap99`). Derived | copy; accessory 5 +10; `ItemUse_Stat2EUp` beside the base (`item_use.cpp:235`) | the restore functions copy it to `+0x1C`; battle working copy `+0xAE` | READ | high |
| `+0x2F..+0x33` | `+0x0BF` | `0x903A9F` | `+0x2B..+0x2F` | 5 | **Element classes** fire, ice, lightning, earth, wind (`Battle_ElementAffinity`, `battle_damage.cpp:644-655`; names from that comment). Level 0..6, a step of 0 sets 7 (`Stat_AddResist`). Derived | copy; armour, accessories, traits (kinds 0..4), halving bits 0..4 | battle damage scale | READ | high |
| `+0x34` | `+0x0C4` | `0x903AA4` | `+0x30` | 1 | **Holy class**. Derived | copy; armour `0x13`, `0x20`; traits kind 5 | `Battle_ScaleDamage` ([`../battle_damage.md`](../battle_damage.md) step 4) | READ | high |
| `+0x35..+0x37` | `+0x0C5` | `0x903AA5` | `+0x31..+0x33` | 3 | **Three status classes**: the status-roll lookups by `+0x35`, `+0x36`, `+0x37` (`ClassLookup`, `rest_3d.cpp:121-128`); `+0x36` also the wake roll (`Battle_WakeRoll`, `battle_e4.cpp:558`); `+0x37` at 6 or more blocks the pending flag (`Battle_RollPendingFlag`, `battle_sprites.cpp:144`, 168). Derived | copy; weapon, armour, accessories, traits kinds 6..8, halving bits 5..7 | as named | READ | high |
| `+0x38` | `+0x0C8` | `0x903AA8` | `+0x34` | 1 | **Surprise** %: a party member whose byte is not below a roll of 0..100 adds to the first-strike count (`Encounter_RollInitiative`, `inventory_ops.cpp:656`, read from the persistent record). Derived | copy; weapon `0x41`, accessory `0x12`, traits kind 9 (`Stat_AddCap100`, 0..100) | as named | READ | high |
| `+0x39` | `+0x0C9` | `0x903AA9` | `+0x35` | 1 | **Reprisal** %: `Battle_MemberReactRoll` answers 1 when it is above `Rand() % 100` (`battle_e4.cpp:497`, working copy `+0xB9`). Derived | copy; traits kind 10 | as named | READ | high |
| `+0x3A` | `+0x0CA` | `0x903AAA` | `+0x36` | 1 | **Critical** %: below it `BattleObj_SwingCue` gives cue 3 and the crit bit (`records.h:217`, `battle_obj_states.cpp:281`). Derived | copy; weapons `0x0D`, `0x43`, `0x41`; traits kind 11; in battle +30 under a condition (`battle_e6.cpp:877`, working copy) | as named | READ | high |
| `+0x3B` | `+0x0CB` | `0x903AAB` | `+0x37` | 1 | **Dodge** %: the stat copy's `0x939F9B`, a miss when it is not below `Rand() % 100` (`Battle_HitOrMissParty`, `battle_e4.cpp:332-333`; `BattleObj_HitReceive` +25 under a flag, `battle_e3.cpp:403-404`). Derived | copy; traits kind 12; +30 in battle (`battle_e6.cpp:876`) | as named | READ | high |
| `+0x3C` | `+0x0CC` | `0x903AAC` | `+0x38` | 1 | Fifth percentage; **accuracy** by the PSX notes. Derived | copy; weapons `0x34` +10, `0x4D` +30, accessory `0x13` +10, traits kind 13; a boss set-up writes the working copy's to 100 (`boss_se.cpp:584`) | **no reader found** | READ (writers); name CITED | medium |
| `+0x3D..+0x3F` | `+0x0CD` | `0x903AAD` | `+0x39..+0x3B` | 3 | Tail of the effective block | `Char_RecalcStats`'s 32-byte copy from `+0x5D` only | none | UNKNOWN | - |
| `+0x40..+0x41` | `+0x0D0` | `0x903AB0` | `+0x3C..+0x3D` | 2 | **Base max HP** (u16, 999 cap on the growth) | default records; `Char_LevelUp` (`Grow`, `scena_sx.cpp:127`); `ItemUse_MaxHpUp` (`Stat_AddCap999`); `BattleExtra_ApplyTally` record 7 (`battle_e1.cpp:263`) | `Char_RecalcStats` (copy, fatigue share); `DragonForm_ApplyRecipe` (`battle_e6.cpp:513`); `Area189_DrainHp` | READ | high |
| `+0x42..+0x43` | `+0x0D2` | `0x903AB2` | `+0x3E..+0x3F` | 2 | **Base max AP** | default; `Char_LevelUp`; `ItemUse_MaxApUp` | recompute | READ | high |
| `+0x44..+0x45` | `+0x0D4` | `0x903AB4` | `+0x40..+0x41` | 2 | **Base ATK** | default; `Char_LevelUp`; `ItemUse_AtkUp` (`item_use.cpp:226`); tally record 7 | recompute; `StatSumHit` (`rest_3b.cpp:674`) | READ | high |
| `+0x46..+0x47` | `+0x0D6` | `0x903AB6` | `+0x42..+0x43` | 2 | **Base DEF** | as ATK (`ItemUse_DefUp`) | recompute | READ | high |
| `+0x48..+0x49` | `+0x0D8` | `0x903AB8` | `+0x44..+0x45` | 2 | **Base AGI** | as ATK (`ItemUse_AgiUp`) | recompute | READ | high |
| `+0x4A..+0x4B` | `+0x0DA` | `0x903ABA` | `+0x46..+0x47` | 2 | **Base INT** | as ATK (`ItemUse_IntUp`) | recompute | READ | high |
| `+0x4C..+0x4D` | `+0x0DC` | `0x903ABC` | `+0x48..+0x49` | 2 | Base of the `+0x2C` word | default records only | `Char_RecalcStats` copy | READ (shape only) | low |
| `+0x4E` | `+0x0DE` | `0x903ABE` | `+0x4A` | 1 | **Base willpower**, 0..99 | `NewGame_InitCharacters`: `(level << 4) / 0x64B8B0[2n] + 0x64B8B1[2n]` for records 0..6, the same byte into `+0x1C` and `+0x2E` (`rest_3a.cpp:832-838`); `ItemUse_Stat2EUp` (`item_use.cpp:234`) | recompute | READ | high |
| `+0x4F..+0x57` | `+0x0DF` | `0x903ABF` | `+0x4B..+0x53` | 9 | **Base resistances**, the nine classes of `+0x2F..+0x37` | default records only (dragon forms write the working copy, `battle_e6.cpp:410-413`, not the record) | recompute | READ | high |
| `+0x58..+0x5C` | `+0x0E8` | `0x903AC8` | `+0x54..+0x58` | 5 | **Base percentages**: surprise, reprisal, critical, dodge, accuracy (as `+0x38..+0x3C`) | default records only | recompute; the roster bonus `0x903640 + 5n` is added to the effective byte only when that byte is not 0 (`char_stats.cpp:231-235`) | READ | high (four); `+0x5C` name CITED |
| `+0x5D..+0x5F` | `+0x0ED` | `0x903ACD` | `+0x59..+0x5B` | 3 | Tail of the base block | default records only | the recompute's copy to `+0x3D` | UNKNOWN | - |
| `+0x60..+0x69` | `+0x0F0` | `0x903AD0` | `+0x5C..+0x65` | 10 | **Ability list, type 0** | `AbilityList_Add` `0x590C90`, first 0 slot (`scena_sx.cpp:446-464`) through `AbilityList_ForType` by the ability's type byte `0x65C4D9 + 24 * id` `& 3` (`scena_sx2.cpp:271-279`); `Char_LevelUp` (row bytes +6, +7); `MasterGrant_Step1Member` | `Char_AbilityList` (`battle_window_draw.cpp:556-568`), `AbilityList_CountSet` (`rest_2f.cpp:1088-1105`) | READ | high |
| `+0x6A..+0x73` | `+0x0FA` | `0x903ADA` | `+0x66..+0x6F` | 10 | **Ability list, type 1** | as above | as above | READ | high |
| `+0x74..+0x7D` | `+0x104` | `0x903AE4` | `+0x70..+0x79` | 10 | **Ability list, type 2** | as above | as above | READ | high |
| `+0x7E..+0x87` | `+0x10E` | `0x903AEE` | `+0x7A..+0x83` | 10 | **Ability list, type 3** (the one `MasterWin_SkillKnown` searches, records 0..6 only, `rest_2h.cpp:576-583`; `Battle_MemberListFull` tests it full, `records.h:226`). **Record 7's first byte (`0x903F6A`) is also the shisu minigame's rank** (below) | as above; `Shisu_Result` writes 0..5 to record 7's `+0x7E` (`rest_2b.cpp:415`) | as above; `Area130_TailGiveItem` reads record 7's `+0x7E` as the prize pick (`area_w3c.cpp:379`) | READ | high |
| `+0x88` | `+0x118` | `0x903AF8` | `+0x84` | 1 | **Level at apprenticing** | `MasterJoin_Step5Apply` = `+0x0A` (`rest_2d.cpp:336`); `MasterQuit_Step5Apply` 0 (`rest_2d.cpp:401`) | `MasterGrant_Step1Member`: levels gained = `+0x0A - +0x88` **as a byte** (`rest_2d.cpp:468`) | READ | high |
| `+0x89..+0x8E` | `+0x119` | `0x903AF9` | `+0x85..+0x8A` | 6 | **Growth bias**, **s8 each**, for base max HP, max AP, ATK, DEF, AGI, INT in that order: each level gained adds `max(0, table gain + bias)`, the word capped at 999 (`Grow`, `scena_sx.cpp:87-93`, 127-132; `Char_LevelUpGain` the same, `battle_e1.cpp:902`) | `MasterJoin_Step5Apply` = the master's six bytes `0x664370 + 6 * master` (`rest_2d.cpp:337`); `MasterQuit_Step5Apply` 0 (`rest_2d.cpp:401`) | `Char_LevelUp`, `Char_LevelUpGain` | READ | high |
| `+0x8F..+0xA3` | `+0x11F` | `0x903AFF` | `+0x8B..+0x9F` | 21 | Record tail | bulk copies only | bulk copies only | UNKNOWN | - |

### 1.1 Which bytes are rebuilt on load, and from what

`TitleFlow_EnterGame` `0x588800` (`save_menu.cpp:1282-1297`), after every load
and after a new game, runs for **all eight records** (joined or not)
`NewGame_RollGrowth(i)` then `Char_RecalcStats(record i)`.

1. `NewGame_RollGrowth` writes the roster bonus `0x903640 + 5i` and then
   **clears all five bytes** (`save_menu.cpp:862-870`). That bonus is outside
   the block, and after a load it is always 0, so step 5 of the recompute
   adds nothing.
2. `Char_RecalcStats` (`char_stats.cpp:217-236`):
   - **`+0x20..+0x3F` (32 bytes) = `+0x40..+0x5F`**, then the weapon (`+0x12`),
     armour (`+0x13..+0x15`), accessory (`+0x16..+0x17`) and master
     (`+0x1F`) passes change `+0x24..+0x2E` and `+0x2F..+0x3C`.
   - `+0x20` (max HP) less `(+0x1E * +0x40 + 5) / 10`.
   - **`+0x18` (HP) cut to `+0x20`.**
   - Each set bit of `+0x1D` steps one byte of `+0x2F..+0x37` by 2.

So **derived**: `+0x20..+0x3F` entire, and HP when above the new max. **Not
derived** (kept as saved): everything else, including AP (`+0x1A`, not cut),
current willpower (`+0x1C`), status (`+0x10`; bit `0x2000` is rewritten on
the next status draw), and the base block.

## 2. The current spec table (section 8, PSX offsets) checked

| PSX row | Verdict |
|---|---|
| `+0x00` name 5 (PC 9) | **Confirmed.** 9 bytes on the PC; byte 8 copied as a ninth by `MasterJoin_Step5Apply` |
| `+0x05` `char_id` | **Confirmed** (PC `+0x09`). Scenario code rewrites it; `Character_IndexById` `0x66972C` (exe table, not in the save) maps ids to records |
| `+0x06` level | **Confirmed** (PC `+0x0A`) |
| `+0x07` unnamed, UNKNOWN | **Corrected:** flags (PC `+0x0B`) - bit 0 joined, bit 1 fixed in the party, bit 2 cleared after a KO fight |
| `+0x08` EXP | **Confirmed** (PC `+0x0C`): s32, capped 9,999,999 |
| `+0x0C` status bits, CITED | **Confirmed in code** (PC `+0x10`). Bits `0x4000`, `0x2000`, `0x80`, `0x20` persist past a fight (`0x60A0` mask) |
| `+0x0E` weapon, shield, helm, armor, two accessories | **Confirmed in shape**: weapon, three armour, two accessories. The code does not name the armour slots |
| `+0x14` current HP, AP | **Confirmed** (PC `+0x18`, `+0x1A`) |
| `+0x18` unnamed, UNKNOWN | **Corrected:** current willpower (PC `+0x1C`) |
| `+0x19` fatigue, CITED | **Corrected:** PSX `+0x19` (PC `+0x1D`) is the **halving bits**. Fatigue is PSX `+0x1A` |
| `+0x1A` unnamed, UNKNOWN | **Corrected:** **fatigue**, the max-HP scale 0..9 (PC `+0x1E`) |
| `+0x1B` master, CITED | **Confirmed** (PC `+0x1F`), `0xFF` none |
| `+0x1C` max HP, AP, current (D) | **Corrected:** effective max HP and max AP, both derived (PC `+0x20`, `+0x22`); no "current" word |
| `+0x20` 28 live stats (D) | **Confirmed derived**, and now itemised: ATK, DEF, AGI, INT words, the `+0x2C` word, willpower, nine classes, five percentages, three unused (PC `+0x24..+0x3F`) |
| `+0x3C` base max HP, AP, CITED | **Confirmed** (PC `+0x40`, `+0x42`) |
| `+0x40` base power, defence, agility, intelligence | **Confirmed** (PC `+0x44..+0x4B`) |
| `+0x48` unnamed 2, UNKNOWN | **Partly read:** the base of the effective `+0x2C` word (PC `+0x4C`); armour `0x28` raises the effective one; no reader, meaning UNKNOWN |
| `+0x4A` willpower, CITED | **Confirmed** (PC `+0x4E`): derived from the level at a new game |
| `+0x4B` resistances 9, CITED | **Confirmed** (PC `+0x4F..+0x57`): five elements, holy, three status classes |
| `+0x54` surprise, reprisal, critical, dodge, accuracy, CITED | **Confirmed for four** (PC `+0x58..+0x5B`, readers named above); accuracy (PC `+0x5C`) has writers but no reader found |
| `+0x59` 3 zero | **Confirmed:** no meaning; copied by the recompute only |
| `+0x5C` four ability lists (heal, assist, attack, skill) | **Confirmed in shape** (PC `+0x60`, `+0x6A`, `+0x74`, `+0x7E`, by the ability's type byte & 3). The four type names stay the sibling's; the code shows type 3 is the list the masters' skill check reads |
| `+0x84` apprenticing level, CITED | **Confirmed** (PC `+0x88`) |
| `+0x85` growth, signed | **Confirmed** (PC `+0x89..+0x8E`): a per-level bias per base stat, set by a master and cleared on leaving |
| `+0x8B` 25 unused | **Confirmed:** PC `+0x8F..+0xA3` (21 bytes) is moved only by whole-record copies |

## 3. What a save editor may edit, and what must agree

**Regenerate or leave alone:** `+0x20..+0x3F`. The game rebuilds them on
load, so an edit there does nothing. To change a stat, edit the base block
(`+0x40..+0x5F`) or the equipment.

**Safe to edit, with these limits:**

- Name: 9 bytes, byte 8 0, glyph codes of the target's font
  ([`../save-editor-spec.md`](../save-editor-spec.md) section 9).
- Base words `+0x40..+0x4B`: 0..999 (every writer caps there).
- Base willpower `+0x4E`: 0..99. Base classes `+0x4F..+0x57`: 0..6, or 7.
  Base percentages `+0x58..+0x5C`: 0..100.
- Equipment `+0x12..+0x17`: ids in the item tables; 0 for none. An editor
  that equips an item must remove it from the inventory itself - the game
  counts worn items apart (`Inventory_Count`), it does not move them.
- Fatigue `+0x1E`: 0..9.
- Ability lists: non-zero ids, each in the list its type byte names
  (`AbilityList_ForType`), packed from the front (`AbilityList_Add` takes the
  first 0).

**Values that must agree with another:**

- **Level and EXP.** `Char_LevelUp` derives the level from the EXP and the
  record's own 99 rows of `Char_ExpTable` (`rows = record index * 99`,
  `scena_sx.cpp:110`). If the EXP reaches a higher level than `+0x0A`, the
  next level-up grants every level in between, with stat gains and
  abilities. If the EXP is lower, nothing happens (no level goes down).
- **HP <= `+0x20`** - cut on load anyway. **AP <= `+0x22`** - **not** cut on
  load; the editor must keep it.
- **Current willpower `+0x1C`** is reset to `+0x2E` only at an inn; keep it at
  or below the effective willpower.
- **Status:** `0x4000` (KO) goes with HP 0; `0x2000` is derived (HP below a
  quarter of max) and is rewritten on the next status draw.
- **Master group:** `+0x1F`, `+0x88`, `+0x89..+0x8E` change together. With
  `+0x1F = 0xFF`, `+0x88..+0x8E` are 0. With a master, `+0x88 <= +0x0A`
  (the levels gained are a byte difference, so a larger `+0x88` wraps to 200
  and more and opens every reward), and `+0x89..+0x8E` should be that
  master's six bytes at `0x664370 + 6 * master` (from the install, not
  shipped).
- **Joined bit (`+0x0B` bit 0)** with the party lists (`+0x682`, `+0x685`):
  the heals and the equip count pass over a record without it. Bit 1 makes a
  member unmovable in the party screen.
- **`char_id` `+0x09`** is tied to the record's position by
  `Character_IndexById` in the exe (section 4); change it only to a value the
  game itself puts at that position.
- **Slot summary `+0xCA0`:** record 0's name (5 + 4 bytes), level and EXP.

## 4. What differs by record position

- **The EXP curve** is the record index's (`Char_ExpTable` rows by index,
  `scena_sx.cpp:110`; `Char_LevelUpGain` `battle_e1.cpp:882`), not the
  `char_id`'s.
- **New game:** records 0..6 get their willpower from the pair
  `0x64B8B0[2n]`, `[2n+1]` (`rest_3a.cpp:833-838`); record 7 is the whelp's
  record `0x64B80C`, with no derived willpower (`Char_WhelpSlot` = 7).
- **Record 0:** the save summary's name (DIV-0076), level and EXP
  (`rest_2c.cpp:391-400`); its `char_id` changes between 0, 7 and 9 by
  scenario code and eight areas test it for 9; areas 191 and 192 read its
  fatigue to pick their scene (`area_w4e.cpp:1079`, `area_w4f.cpp:372`);
  `RestoreRecords` heals records 1..7 only (`scena_sc13.cpp:2622`).
- **Record 7** is several things: the whelp at a new game; the chapter 6
  guest, whose HP, weapon, third armour and stats `Scena06_GuestRecord` sets
  directly (`scena_sc6.cpp:256-262`); the battle extra's guest, whose stats
  the tally raises (`battle_e1.cpp:250-266`), whose equipment
  `BattleExtra_EquipCommit` changes and whose name the equipment screen swaps
  (DIV-0118); a boss set-up copies its max HP, ATK, DEF into an enemy
  (`boss_se.cpp:104-111`). Its `+0x7E` holds the shisu rank, and the
  masters' skill check (records 0..6) and area 189's walk (records 0..6,
  `kCharWalked`) leave record 7 out.
- **Records 1, 5, 6:** chapter 5 sets and clears their joined bits directly
  (`scena_sc5.cpp:456-508`, 1818-1820).
- **Record 4:** chapter 8 run 11 sets it up and calls the recompute on it
  (`scena_sc7_callees.h:88`). `Scena08_Scene11` gives the record of member id
  4 (through `Character_IndexById`) HP 0 and status `0x6000`, and sets or
  clears bit 1 of it and of member 7's record (`scena_sc7.cpp:2489-2495`,
  2566-2571).

## 5. Open questions and possible defects

1. **Record 7's ability byte reused as a minigame rank** (possible original
   defect). `Shisu_Result` `0x57E160` (PSX `0x801D1018`) writes its rank 0..5
   to `0x903F6A`, record 7 `+0x7E`, the first slot of its type-3 list
   (`rest_2b.cpp:415`), and `Area130_TailGiveItem` reads it back as the prize
   (`area_w3c.cpp:379`). The masters' check skips record 7, which suggests
   the list is unused there. The rank survives into a save. An editor must
   not treat a non-zero byte there as an ability. Whether the PSX writes the
   same record byte is not checked here.
2. **The halving bits `+0x1D` have no setter in `src/`**; only
   `Party_RestoreAll` clears them. The setter is in code not yet ours, or
   there is none.
3. **`+0x0B` bit 2** is cleared after a KO fight; no setter found.
4. **Accuracy `+0x3C` / `+0x5C`** and the **`+0x2C` / `+0x4C` word** have
   writers and no reader in `src/`.
5. **Two fatigue caps**: a KO after a fight and effect slot 106 stop at 5;
   effect slot 118, area 189 and item handler 22 go to 9. This is the
   original's behaviour, as read. Whether it is intent is for the owner.
6. **Equipment used up in a fight** (`BattleObj_ReviveByEquip` zeroes the
   working copy's accessory or third armour, `battle_e3.cpp:840-849`) is not
   written back by `Battle_WriteBackMember`. `Battle_ReloadPartyRecords` then
   copies the record over the working copy. Is the item kept? Not traced.
7. **The roster bonus `0x903640`** (outside the block) is cleared on every
   load and new game. So `Char_RecalcStats`' step 5 adds nothing after a
   load. A bonus set later in play is lost at the next load. No other writer
   was found in `src/`.
8. **The meaning of status bits `0x80`, `0x20`, `0x08`, `0x100`** by name is
   not in the source (only the texts' addresses).

## Summary

Per record (164 bytes): **137 READ, 0 CITED, 27 UNKNOWN**; across the
eight records, 1,096 READ, 216 UNKNOWN. The UNKNOWN bytes are `+0x3D..+0x3F`,
`+0x5D..+0x5F` and `+0x8F..+0xA3`, which are moved only by whole-record or
whole-block copies. Two names on READ bytes stay CITED: accuracy
(`+0x3C`, `+0x5C`) and the four ability-list type names.

# The record types (`src/game/records.h`): typed-data round group SD

**Status:** DRAFT (verified 2026-10-19 against `origin/main` 19180e63; the
header compiles standalone, and no call site uses it yet)

Group SD of [`typed-data-round.md`](typed-data-round.md) section 2: one struct
per record family. Each family has `static_assert(sizeof == stride)` and one
`static_assert(offsetof == offset)` per named field. No `.cpp` changes and no
site is converted. Stage B does that.

## 1. The method

- **A field is named only where a cited reader names it.** The readers are a
  module doc, a `*_callees.h` constant with its width comment, a `symbols.toml`
  evidence line, or the reimplemented function itself. Each field's source is
  in the tables below and beside the field in the header. Everything else is
  `unsigned char pad_<offset>[n]`, with the reason for the gap beside it.
- **Widths come from the reader's own access**: `Word`, `Long`, `Byte`, a
  `u16` in a doc table, or a `movsx`.
  - Where readers take a dword and also its high word (the 16.16 positions
    `+0x34` / `+0x38` / `+0x3C`, whose whole parts are read at `+0x36` /
    `+0x3A` / `+0x3E`), the dword is the member and the comment names the
    high word.
  - The same applies where the field and battle code read one dword as two
    bytes (`PartyMember +0x134`). A union was not used, so that stage B
    decides that shape with the sites in front of it.
- **Addresses are `std::uint32_t`**, because the image is 32-bit. Nothing is
  packed: every named field is at its natural alignment. The header was
  checked both with and without `#pragma pack(1)`; all asserts hold either
  way, so the plain layout was kept.
- **Families split where the data split them.**
  - The battle enemy (`0x128`) is its sprite (`+0..+0x7F`) and then
    `EnemyWorkingRecords`. `EnemyData` (`0x8C`) is the area's record that
    `Battle_CopyEnemyData` copies in.
  - The battle task (`0x84`) is the 48 slots at `0x93A000`, not the
    scheduler's `0x20`-byte `Task_Records` ([`task_sched.md`](task_sched.md)).
  - The stride `0xA4` covers two layouts. One is the sprite object (this
    header). The other is the persistent character record `0x903A70 + id *
    0xA4` (`kCharStride` / `kRecordStride`), which is not covered here except
    as `PartyMember +0x80..+0x123`, the copy of it.
- The structs are flat on purpose. The sprite, the party member, the enemy,
  the effect object and the battle task all go through `Sprite_Current`, so
  their first `0x80` bytes are read by the same sprite functions. Their own
  readers still give some of those bytes different meanings (the battle
  task's `+0x60` is the popup's value, and the sprite's `+0x60` is the
  depth). A shared `SpriteHead` is a stage B or C decision, made against the
  sites.

## 2. Checked against the sources (2026-10-19)

Each layout was re-read against the doc or reader it cites before it was
written:

- **Window record**: every field is from [`window-task.md`](window-task.md)
  section 3's table, unchanged.
- **Save summary**: from `Menu_DrawSaveSlot` (`field_o.cpp`) and
  [`save-files.md`](save-files.md) section 2. [`save-interchange.md`](save-interchange.md)
  calls `+5` the leader's `char_id`, and `Menu_DrawSaveSlot` reads `+5..+7`
  as the three party icons. These agree: `party[0]` is the leader.
- **Directory entry**: from `Save_ListFiles` (`save_menu.cpp`). The name's
  0x14-byte extent is the gap up to the size field. The writer is unbounded
  (save-files.md's first defect).
- **Battle task**: from `battle_flow.cpp` (`BattleTask_Create` / `RunAll` /
  `FreeCurrent`) and `battle_fx_tasks.cpp` (the popup's five states). `+0x60`
  is read as a dword at the start and as a word by the draw. The dword is the
  member.
- **Party member**: from `battle_setup_callees.h`, `battle_damage_callees.h`
  (whose offsets are from `PartyWorkingRecords` `0x802D50`, the record
  `+0x10`; converted here), `area_entry_callees.h`, `area_backdrop.cpp`,
  `effect_3a_callees.h` and [`member-sprites.md`](member-sprites.md) section
  2.
  - A first census reported `+0x70` as a separate byte "raised". The member
    readers do test only its low byte (`member_sprites.cpp`), but
    [`sprite-pose.md`](sprite-pose.md) section 3 writes the sprite's `+0x70`
    as a dword. The member is a `u32` named `raised`.
  - The same census put `+0x136` / `+0x137` as bytes inside the battle's
    `u32` at `+0x134`. That is the overlap described in section 1.
- **Enemy and enemy data**: from `Battle_SetupEnemy` / `Battle_CopyEnemyData`
  (`battle_sprites.cpp`), `battle_flow.cpp`, `battle_setup_callees.h`,
  `battle_damage_callees.h` (offsets from `+0x80`) and `tools/enemy_ai.py`'s
  header.
  - The data's level `+0x18` is a `u16`, because `enemy_ai.py` and the copy
    both move a word. battle_damage's `kELevel` says "u8, also read as a u16".
  - Zenny and EXP at the data's `+0x14` / `+0x16` are named through the copy
    chain to the enemy's `+0x94` / `+0x96`, which the kill reads by those
    names (`battle_flow.cpp`).
- **Sprite object**: from [`sprite-draw-order.md`](sprite-draw-order.md)
  sections 2, 5, 6, 12 and 14, [`sprite-pose.md`](sprite-pose.md) section 3,
  [`movement-script.md`](movement-script.md) sections 1b and 1c,
  `sprite_screen.cpp` and `field_objects.cpp`.
- **Effect object**: from `symbols.toml`'s evidence lines for
  `Effect_Objects`, `Effect_FindFree`, `Effect_RunObjects`, `Effect_Spawn` and
  `0x5898D0`.

## 3. Unsettled

1. **`PartyMember +0x148`.** `area_entry_callees.h` (`kRecCharacterIndex`),
   `effect_3a_callees.h` and `member_sprites.cpp` read it as the character
   record's index. `battle_damage_callees.h` `kPWeaponKind` (working
   `+0x138`) calls the same byte "5: the weapon's element rolls apply".
   - It is named `char_index`, with the clash in its comment.
   - One of the two readings is wrong, or the byte is reused in battle. This
     wants a read of `Battle_CalcDamage`'s access.
2. **Two flag dwords, two namings.** For the party, `battle_setup` calls
   `+0x134` its flags, while `battle_damage` calls `+0x130` the flags and
   `+0x134` flags2. For the enemy, the pair is `+0x110` / `+0x114`. They are
   named `flags_130` / `flags_134` and `flags_110` / `flags_114` until stage B
   reads the bits.
3. **The party tail's field and battle reuse.**
   - `+0x124` is the script context, or the command.
   - `+0x128` is the walking speed (a byte), or the HP change (a word).
   - `+0x130` is the script pointer, or a flag dword.
   - `+0x134` is the standing counter and action bytes, or a flag dword.

   The battle names and widths are used, and the field use is in each
   comment. A union per overlap is stage B's call.
4. **`+0x144` / `+0x145`** (party; `+0x124` / `+0x125` on the enemy).
   `battle_setup` calls these the counters zeroed with statuses 0x40 / 0x80,
   and `battle_damage` calls them the attack and ability charges. Both are in
   the comment.
5. **The save summary's time bytes `+0xC` / `+0xD`.** They are the time
   format's two values. Which one is hours is not read.
6. **The sprite's `+0x64..+0x6F`.** These are rotation angles in their low
   words (sprite-draw-order 12), and also the member and slot row during a
   member set-up (sprite-pose 3). They are named `angle[3]`, with both uses in
   the comment.
7. **Bytes left as padding although some reader touches them.** Each has its
   own pad comment.
   - The sprite's `+1..+5` and `+7`.
   - `+0x9C` and `+0xA1` of the field object's context.
   - The effect's `+1..+4` and `+0xB`.
   - The battle task's `+0xB` (compared with 1, meaning not named).
   - The enemy's `+0xA8` / `+0xAC` (data dwords `+0x30` / `+0x34`) and its
     stat-block copies.
8. **`WindowRecord +0xA`.** [`window-task.md`](window-task.md) section 3
   reads it as the half-width for kinds 0..2.
   [`battle_sprites.md`](battle_sprites.md) section 3 has battle's kind-3
   enemy-name windows store the actor there. It is named `half_width`, with
   the kind-3 use in its comment.

## 4. What next

- The census (group CS) gives each record-by-offset site a family.
- Stage B converts the sites file by file to `records::T` member access.
  Every offset a site reads that is padding here becomes a named field
  then, with the site as its reader.
- `symbols.toml`'s tables take these as their `ctype` with a `count`:
  `Sprite_Objects` 30, `ObjTrio` 3, `Effect_Objects` 20, the enemies 8, the
  battle tasks 48, `WindowRecords` 22, the summaries and `Save_Directory` 16.

## 5. The fields and where each came from

The header's comments hold the same text. The section names are abbreviated:
"sprite-draw-order 14" is [`sprite-draw-order.md`](sprite-draw-order.md)
section 14, "battle_setup kPHp" is the constant in `battle_setup_callees.h`,
and so on.

### `SpriteObject` (stride `0xA4`): 46 named fields, 108 of 164 bytes

| off | type | field | source |
|---|---|---|---|
| `+0x00` | u8 | `flags` | bit 0 in use, 1 no image upload, 5 semi-transparent, 6 not drawn, 7 hidden / culled (sprite-draw-order 5, 14; sprite-pose 3) |
| `+0x06` | u8 | `type` | 0x0A a party object; 8 and up never turn (sprite-draw-order 5; sprite-pose 3) |
| `+0x08` | u8 | `direction` | facing, & 7 indexes Field_DirectionSteps (field_e2.cpp, area_w3e.cpp) |
| `+0x09` | u8 | `step_frames` | frames left in a step / a timed move; multiplies the step in Sprite_PointInReach (sprite-draw-order 6; movement-script 1b) |
| `+0x0C` | s32 | `step_x` | the step, 16.16 a frame (sprite-draw-order 6) |
| `+0x10` | s32 | `step_z` | (sprite-draw-order 6) |
| `+0x24` | u8 | `draw_flags` | bit 0 frames uploaded, 1 member set-up pending, 2 CLUT row 0x1F0, 6 a 3D object, 7 overlay list (sprite-pose 3; sprite-draw-order 5, 14; movement-script 1c) |
| `+0x25` | u8 | `tex_slot` | texture slot (sprite-pose 3) |
| `+0x26` | u8 | `tex_row` | the slot's y row (sprite-pose 3) |
| `+0x27` | u8 | `clut` | palette (sprite-draw-order 14 Sprite_ClutWord) |
| `+0x28` | u8 | `clut_mode` | palettes to a row; non-zero is "wide" (sprite-draw-order 14) |
| `+0x29` | u8 | `ot_slot` | the draw slot Gfx_CommitPrim takes (sprite-draw-order 14) |
| `+0x2A` | u8 | `pose` | pose / flip byte (sprite-pose 3; sprite-draw-order 14) |
| `+0x2B` | u8 | `key_class` | row of Sprite_KeyAdjust (sprite_screen.cpp 0x588F20) |
| `+0x2C` | u16 | `column` | frame-offset table column (sprite-pose 3) |
| `+0x2E` | s16 | `screen_x` | truncated screen point (movement-script 1c; sprite-draw-order 14) |
| `+0x30` | s16 | `screen_y` | as the line above |
| `+0x32` | u16 | `draw_key` | layer << 8 / sort byte (sprite-draw-order 2; movement-script 1c) |
| `+0x34` | s32 | `x` | position, 16.16; the readers also take its high word +0x36 as the cell (sprite_screen.cpp) |
| `+0x38` | s32 | `z` | position, 16.16; high word +0x3A likewise |
| `+0x3C` | s32 | `height` | the pass's third sort key (sprite-draw-order 2); its high word +0x3E is the s16 elevation Sprite_ObjectMatrix reads (sprite-draw-order 12) |
| `+0x40` | s32 | `scale_x` | 16.16 (sprite-draw-order 12, 14) |
| `+0x44` | s32 | `scale_y` | 16.16 (sprite-draw-order 14) |
| `+0x48` | u8 | `scale_mode` | 1: scale by depth; 0: 1.0 (sprite-draw-order 12, 14) |
| `+0x49` | u8 | `script_steps` | step count (sprite-draw-order 6 Sprite_ScriptStart) |
| `+0x4A` | u8 | `script_hold` | ticks left on the step (sprite-draw-order 5, 6) |
| `+0x4B` | u8 | `animation` | the current animation (sprite-pose 3) |
| `+0x4C` | u32 | `anim_data` | address: the animation data (sprite-pose 3) |
| `+0x50` | u32 | `script` | address: the byte script / a 3D object's quads (sprite-draw-order 5, 12) |
| `+0x54` | u32 | `frames` | address: the frame-word table (sprite-draw-order 5, 6) |
| `+0x58` | u16 | `script_pos` | (sprite-draw-order 5) |
| `+0x5A` | u16 | `frame_word` | (sprite-draw-order 5, 14) |
| `+0x5C` | u8 | `draw_mode` | into the mode word (sprite-draw-order 14) |
| `+0x5D` | u8[3] | `shade` | (sprite-draw-order 12, 14) |
| `+0x60` | s32 | `depth` | (movement-script 1c; sprite-draw-order 14) |
| `+0x64` | u32[3] | `angle` | the rotation angles in the low words (sprite-draw-order 12); +0x64 / +0x68 also the member / its row during a member set-up (sprite-pose 3) |
| `+0x70` | u32 | `reach` | dword (sprite-pose 3); the low byte is the reach Sprite_PointInReach reads (sprite-draw-order 6) |
| `+0x74` | u32 | `screen_xf` | the float screen point's bits (movement-script 1c; sprite_screen.cpp) |
| `+0x78` | u32 | `screen_yf` | as the line above |
| `+0x80` | u8 | `context` | the script context's first byte: flags (movement-script 1b; field_objects.cpp) |
| `+0x81` | u8 | `pose_wait` | (field_objects.cpp) |
| `+0x82` | u8 | `loop` | the context's loop counter (movement-script 1b) |
| `+0x83` | u8 | `area_script` | which of the area's scripts (movement-script 1b) |
| `+0x84` | u8 | `speed` | index into Field_MoveSpeeds (movement-script 1b) |
| `+0x87` | u8 | `move_steps` | steps left in a timed move (movement-script 1b) |
| `+0x8A` | u16 | `script_position` | the context's script position (movement-script 1b) |

Padding:

- +1..+5: the state bytes battle and effect records use; no field-object reader names them here
- +7: bit 7 skips the blocked test (field_objects.cpp); not named
- +0xA, +0xB: +0xB holds an Effect_Objects index for ops 9x (symbols.toml Effect_Spawn); +0xA unread
- +0x14..+0x23: dwords cleared by Battle_SetupEnemy and Sprite_ClearSteps; unnamed
- +0x7C..+0x7F: unread
- +0x85, +0x86: unread
- +0x88, +0x89: unread
- +0x8C..+0xA3: +0x9C (a u16 zeroed) and +0xA1 are written by field_objects.cpp, not named

### `PartyMember` (stride `0x14C`): 44 named fields, 87 of 332 bytes

| off | type | field | source |
|---|---|---|---|
| `+0x00` | u8 | `flags` | bit 0 present in battle (battle_setup_callees.h kPresent) |
| `+0x01` | u8[4] | `state` | the actor's state bytes +1..+4 (battle_setup_callees.h kState1..4) |
| `+0x05` | u8 | `member` | member index / palette slot (area_entry_callees.h kRecPalette; battle_damage kPCureId) |
| `+0x08` | u8 | `direction` | facing (member-sprites 2) |
| `+0x09` | u8 | `step_frames` | (member-sprites 2) |
| `+0x0C` | s32 | `step_x` | 16.16 a frame (member-sprites 2) |
| `+0x10` | s32 | `step_z` | as the line above |
| `+0x29` | u8 | `ot_slot` | the draw slot (area_backdrop.cpp kTrioSlot; member-sprites 2) |
| `+0x2C` | u16 | `column` | (area_entry_callees.h kRecColumn; sprite-pose 3 gives the u16) |
| `+0x34` | s32 | `x` | 16.16, +0x36 the cell (area_backdrop.cpp kTrioPosition; member-sprites 2) |
| `+0x38` | s32 | `z` | 16.16, +0x3A the cell |
| `+0x3C` | s32 | `height` | third dword of the 12 bytes at +0x34 (battle_setup_callees.h kPos; effect_3a_callees.h kLeaderPoint); its high word +0x3E the ground (member-sprites 2) |
| `+0x70` | u32 | `raised` | the member readers test its low byte, "raised" (member-sprites 2; member_sprites.cpp); sprite-pose 3 writes the sprite's +0x70 as a dword |
| `+0x89` | u8 | `char_id` | record +0x09 (battle_setup_callees.h kPCharId; area_entry_callees.h kRecMember) |
| `+0x8A` | u8 | `level` | record +0x0A (battle_damage_callees.h kPLevel, working +0x7A) |
| `+0x90` | u16 | `status` | record +0x10 (battle_setup kPStatus; battle_damage kPStatus) |
| `+0x92` | u8 | `weapon` | record +0x12 (battle_setup kPWeapon) |
| `+0x96` | u8[2] | `accessory` | record +0x16 / +0x17 (battle_setup kPAccessory) |
| `+0x98` | u16 | `hp` | record +0x18 (battle_setup kPHp; battle_damage kPHp) |
| `+0x9A` | u16 | `ap` | record +0x1A |
| `+0x9C` | u8 | `survive` | record +0x1C, set from +0xAE (battle_setup kPByte9C); the overkill a kill leaves the flag below (battle_damage kPSurvive) |
| `+0xA0` | u16 | `max_hp` | record +0x20 (battle_setup kPMaxHp) |
| `+0xA2` | u16 | `max_ap` | record +0x22 |
| `+0xA6` | u16 | `def` | record +0x26 (battle_damage kPDef) |
| `+0xA8` | u16 | `agi` | record +0x28 (battle_setup kPAgi; battle_damage kPAgi) |
| `+0xAE` | u8 | `byte_ae` | record +0x2E, copied to +0x9C (battle_setup kPByteAE) |
| `+0xAF` | u8[5] | `elements` | affinity classes, fire .. wind (battle_damage kPElements) |
| `+0xB4` | u8 | `holy` | holy affinity class (battle_damage kPHoly) |
| `+0x124` | u8 | `command` | battle: the command auto-battle gives (docs/battle_e4.md); field: the script context's first byte (area_entry_callees.h kRecContext) |
| `+0x125` | u8 | `queued_kind` | 5 an item, 4 an ability (battle_setup kPQueuedKind; battle_damage kPCommand) |
| `+0x126` | u16 | `queued_id` | category << 8 / id, or the ability id (battle_setup kPQueuedItem; battle_damage kPCommandId) |
| `+0x128` | u16 | `hp_change` | battle (battle_setup kPHpChange); field: its low byte the walking speed (member-sprites 2) |
| `+0x12A` | u16 | `ap_change` | battle (battle_setup kPApChange) |
| `+0x12C` | u8 | `shown` | status-shown bits, 0x11 / 2 with an effect (battle_setup kPEffect; battle_damage kPShown) |
| `+0x12D` | u8 | `counter` | the 0x40 / 0x20 statuses' counter (battle_setup kPCounter); battle_damage kPShown2 "cleared with a cured status" |
| `+0x12E` | u8 | `item_slot` | (battle_setup kPItemSlot) |
| `+0x130` | u32 | `flags_130` | battle: flag dword (battle_damage kPFlags; battle_sprites.cpp "the party's flags dword"); field: the script pointer (area_entry_callees.h kRecScript) |
| `+0x134` | u32 | `flags_134` | battle: flag dword (battle_setup kPFlags; battle_damage kPFlags2); field: byte +0x136 the standing counter, +0x137 the action (member-sprites 2) - one dword in battle, two bytes in the field, so the bytes are not separate members |
| `+0x138` | u8 | `field_flags` | field: bit 0, bit 3 CLUT pending (area_backdrop.cpp kTrioFlags; member-sprites 2) |
| `+0x142` | u8 | `count_4000` | (battle_setup kPCount4000) |
| `+0x143` | u8 | `count_800` | (battle_setup kPCount800) |
| `+0x144` | u8 | `count_40` | zeroed with flag 0x40 (battle_setup kPCount40); battle_damage kPCharge, the attack charge |
| `+0x145` | u8 | `count_80` | zeroed with flag 0x80 (battle_setup kPCount80); battle_damage kPCharge2, the ability charge |
| `+0x148` | u8 | `char_index` | the character record's index (area_entry_callees.h kRecCharacterIndex; effect_3a_callees.h kMemberFlag) - battle_damage_callees.h kPWeaponKind names the same byte "5: the weapon's element rolls apply": unsettled |

Padding:

- +6, +7: whom it follows / a run counter by member-sprites 2 and event_leader.md, no width stated
- +0xA, +0xB: +0xB an effect-object index (member-sprites 2), meaning not named
- +0x14..+0x28: unnamed sprite bytes
- +0x2A, +0x2B: the sprite's pose / key class; no member reader names them
- +0x2E..+0x33: the sprite's screen point and key; no member reader names them
- +0x40..+0x6F: sprite fields (SpriteObject); +0x5C..+0x5F written on entry (area-entry.md), not named here
- +0x74..+0x88: the sprite's float screen point; the character record's +0..+8 (name)
- +0x8B..+0x8F: character record +0x0B..+0x0F; +0x8B bit 0 "joined" per char-stats.md, no member reader addresses it
- +0x93..+0x95: armour (battle_setup kPArmour3 names only +0x95)
- +0x9D..+0x9F: unread
- +0xA4..+0xA5: record +0x24 (ATK per char-stats.md), not read through this record by a cited reader
- +0xAA..+0xAD: unread here
- +0xB5..+0x123: the rest of the character record copy (base stats at +0xC0 per battle_e4.md); not named here
- +0x12F: unread
- +0x139..+0x141: unread
- +0x146, +0x147: unread
- +0x149..+0x14B: unread

### `EffectObject` (stride `0x80`): 10 named fields, 27 of 128 bytes

| off | type | field | source |
|---|---|---|---|
| `+0x00` | u8 | `in_use` | non-zero in use (Effect_FindFree / Effect_RunObjects, symbols.toml) |
| `+0x05` | u8 | `kind` | indexes Effect_KindHandlers (symbols.toml Effect_RunObjects; Effect_Spawn sets 6) |
| `+0x06` | u8 | `param` | Effect_Spawn's first argument (symbols.toml Effect_Spawn) |
| `+0x0C` | s32 | `step_x` | dword, Effect_Spawn's movsx byte (symbols.toml) |
| `+0x10` | s32 | `step_z` | as the line above |
| `+0x2E` | s16 | `screen_x` | Effect_Spawn's word arguments (symbols.toml) |
| `+0x30` | s16 | `screen_y` | as the line above |
| `+0x34` | s32 | `x` | dword read by AreaMap_Elevation; its u16 halves written apart (symbols.toml, EventScript_Op's spawn 0x5898D0) |
| `+0x38` | s32 | `z` | as the line above |
| `+0x3C` | s32 | `height` | its high word +0x3E the u16 elevation written by the spawn 0x5898D0 (symbols.toml) |

Padding:

- +1..+4: 0x589870 zeroes bytes 0..4 (symbols.toml); not named for this family
- +7..+0xB: +0xB a spawn argument (0x5898D0), not named
- +0x14..+0x2D: unread by the pool's readers
- +0x32, +0x33: the sprite's draw key, not named for this family
- +0x40..+0x7F: the sprite fields, not named for this family

### `BattleEnemy` (stride `0x128`): 46 named fields, 105 of 296 bytes

| off | type | field | source |
|---|---|---|---|
| `+0x00` | u8 | `flags` | bit 0 present, 0x40 defeated (battle_setup kPresent; battle_flow.md) |
| `+0x01` | u8[4] | `state` | (battle_setup kState1..4) |
| `+0x05` | u8 | `actor` | slot + 3 (Battle_SetupEnemy, battle_sprites.cpp) |
| `+0x08` | u8 | `layout` | the layout ^ 2 (Battle_SetupEnemy); 2 / 3 pick the next animation (battle_flow.md) |
| `+0x29` | u8 | `ot_slot` | 4 (Battle_SetupEnemy) |
| `+0x2A` | u8 | `flip` | (battle_flow.md BattleEnemy_SetAnimation) |
| `+0x34` | s32 | `x` | (Battle_SetupEnemy; battle_setup kPos) |
| `+0x38` | s32 | `z` | as the line above |
| `+0x3C` | s32 | `height` | its high word +0x3E the ground (Battle_SetupEnemy) |
| `+0x4B` | u8 | `animation` | 0xFF at set-up (Battle_SetupEnemy) |
| `+0x80` | char[12] | `name` | from EnemyData +0 (battle_sprites.md 3) |
| `+0x8C` | u8 | `kind` | EnemyData +0xC; grouped by Battle_OpenEnemyNames (battle_sprites.cpp) |
| `+0x8D` | u8 | `family` | EnemyData +0xD (battle_damage kEFamily) |
| `+0x8E` | u8 | `odds_index` | EnemyData +0xE (Battle_CopyEnemyData) |
| `+0x8F` | u8 | `kind_flag` | Battle_EnemyKindFlag (Battle_SetupEnemy) |
| `+0x90` | u16 | `flags_90` | EnemyData +0x12 (Battle_CopyEnemyData); bit 1 read by BattleEnemy_Chance70 (battle_flow.cpp) |
| `+0x92` | u16 | `status` | (battle_setup kEStatus; battle_damage kEStatus); 0x4000 dead (battle_flow.cpp) |
| `+0x94` | u16 | `zenny` | (battle_flow.cpp, the kill) |
| `+0x96` | u16 | `exp` | (battle_flow.cpp, the kill) |
| `+0x98` | u16 | `level` | battle_damage kELevel: u8, also read as a u16 |
| `+0x9A` | u16 | `bank` | the animation bank, EnemyData +0x1A (Battle_SetupEnemy) |
| `+0x9C` | u8[8] | `abilities` | EnemyData +0x1C (Battle_CopyEnemyData) |
| `+0xA4` | u16 | `hp` | (battle_setup kEHp; battle_damage kEHp) |
| `+0xA6` | u16 | `ap` | (battle_damage kEAp) |
| `+0xB0` | u16 | `max_hp` | (battle_setup kEMaxHp) |
| `+0xB2` | u16 | `max_ap` | (battle_damage kEMaxAp) |
| `+0xB8` | u16 | `agi` | (battle_setup kEAgi) |
| `+0xBF` | u8[5] | `elements` | (battle_damage kEElements) |
| `+0xC4` | u8 | `holy` | (battle_damage kEHoly) |
| `+0xF0` | u8 | `ai_script` | the data id (battle_damage kEScript; Battle_CopyEnemyData) |
| `+0xF1` | u8 | `ai_rows_done` | a bit per AI row fired; DIV-0082 (battle_damage kERowsDone) |
| `+0xF2` | s8 | `popup_dx` | (battle_fx_tasks.cpp BattleFx_PopupStart) |
| `+0xF3` | s8 | `popup_dy` | as the line above |
| `+0xF4` | u32 | `event_hook` | address, called with 2 in an event battle (battle_flow.md) |
| `+0xFC` | u32 | `anim_table` | address of the animation bytes (battle_flow.cpp BattleEnemy_SetAnimation) |
| `+0x100` | u8 | `run_state` | indexes the state table 0x64B084 (battle_flow.md); EnemyData +0x88 |
| `+0x105` | u8 | `command` | (battle_damage kECommand) |
| `+0x108` | u16 | `hp_change` | (battle_setup kEHpChange) |
| `+0x10A` | u16 | `ap_change` | (battle_setup kEApChange) |
| `+0x10C` | u8 | `shown` | (battle_setup kEEffect; battle_damage kEShown) |
| `+0x10D` | u8 | `counter` | (battle_setup kECounter; battle_damage kEShown2) |
| `+0x110` | u32 | `flags_110` | bit 15, bits 0x70, the popup's 0x80 (battle_damage kEFlags; battle_flow.cpp; battle_fx_tasks.cpp) |
| `+0x114` | u32 | `flags_114` | bit 0x10 (battle_setup kEFlags; battle_damage kEFlags2) |
| `+0x122` | u8 | `count_4000` | (battle_setup kECount4000) |
| `+0x124` | u8 | `count_40` | (battle_setup kECount40); battle_damage kECharge |
| `+0x125` | u8 | `count_80` | (battle_setup kECount80); battle_damage kECharge2 |

Padding:

- +6, +7: sprite bytes, no enemy reader names them
- +9..+0x28: +0xC..+0x23 dwords cleared at set-up, +0x24 cleared; not named
- +0x2B..+0x33: unread here
- +0x40..+0x4A: sprite fields, not named here
- +0x4C..+0x7F: sprite fields, not named here
- +0xA8..+0xAF: EnemyData +0x30 / +0x34 dwords (Battle_CopyEnemyData), unnamed
- +0xB4..+0xB7: the stat block's copy at +0xB0.. (Battle_CopyEnemyData), unnamed
- +0xBA..+0xBE: likewise; stat names are hypotheses (tools/enemy_ai.py)
- +0xC5..+0xEF: copies of the stat block (+0xD0..), unnamed
- +0xF8..+0xFB: unread
- +0x101: EnemyData +0x89; +0x102..+0x104 unread
- +0x106, +0x107: unread
- +0x10E, +0x10F: unread
- +0x118..+0x121: +0x118..+0x11F zeroed at set-up, unnamed
- +0x123: zeroed at set-up, unnamed
- +0x126, +0x127: unread

### `EnemyData` (stride `0x8C`): 17 named fields, 120 of 140 bytes

| off | type | field | source |
|---|---|---|---|
| `+0x00` | char[12] | `name` | (enemy_ai.py; battle_sprites.md 3) |
| `+0x0C` | u8 | `kind` | to BattleEnemy +0x8C; Battle_EnemyKindFlag's argument |
| `+0x0D` | u8 | `family` | to BattleEnemy +0x8D |
| `+0x0E` | u8 | `odds_index` | the base action-odds index (enemy_ai.py) |
| `+0x12` | u16 | `flags` | (enemy_ai.py); to BattleEnemy +0x90 |
| `+0x14` | u16 | `zenny` | to BattleEnemy +0x94 |
| `+0x16` | u16 | `exp` | to BattleEnemy +0x96 |
| `+0x18` | u16 | `level` | (enemy_ai.py reads a u16) |
| `+0x1A` | u16 | `bank` | the animation bank (Battle_SetupEnemy) |
| `+0x1C` | u8[8] | `abilities` | one picked by Rand & 7 (enemy_ai.py) |
| `+0x24` | u16 | `hp` | (enemy_ai.py) |
| `+0x26` | u16 | `ap` | as the line above |
| `+0x28` | u16[4] | `stats` | names hypotheses (enemy_ai.py) |
| `+0x38` | u8[64] | `ai_rows` | four rows of 16 bytes (enemy_ai_data.md 1; enemy_ai.py) |
| `+0x7C` | u8[9] | `resist` | to BattleEnemy +0xDF.. and +0xBF.. (Battle_CopyEnemyData) |
| `+0x85` | u8 | `status` | to BattleEnemy +0x92 (Battle_CopyEnemyData) |
| `+0x88` | u8 | `run_state` | to BattleEnemy +0x100 (Battle_CopyEnemyData) |

Padding:

- +0xF..+0x11: unread
- +0x30..+0x37: two dwords copied to BattleEnemy +0xA8 / +0xAC, unnamed
- +0x78..+0x7B: unread
- +0x86, +0x87: unread
- +0x89: to BattleEnemy +0x101, unnamed; +0x8A, +0x8B unread

### `BattleTask` (stride `0x84`): 17 named fields, 42 of 132 bytes

| off | type | field | source |
|---|---|---|---|
| `+0x00` | u8 | `flags` | bit 0 taken; non-zero runs (battle_flow.cpp BattleTask_Create / RunAll) |
| `+0x01` | u8 | `state` | (battle_fx_tasks.md 1) |
| `+0x05` | u8 | `param` | the effect / row (BattleTask_Create; battle_fx_tasks.md 1) |
| `+0x06` | u8 | `kind` | 0..3 (BattleTask_RunAll) |
| `+0x07` | u8 | `mode` | (battle_fx_tasks.md 1, the popup's draw) |
| `+0x09` | u8 | `timer` | (battle_fx_tasks.cpp popup) |
| `+0x0A` | u8 | `digits` | the digit count (battle_fx_tasks.cpp) |
| `+0x0C` | s32 | `vel_x` | 16.16 (battle_fx_tasks.cpp PopupRise / Bounce) |
| `+0x10` | s32 | `vel_y` | as the line above |
| `+0x18` | s32 | `start_x` | (battle_fx_tasks.cpp PopupRise) |
| `+0x1C` | s32 | `start_y` | as the line above |
| `+0x27` | u8 | `clut` | (battle_fx_tasks.cpp DamagePopup) |
| `+0x32` | u16 | `digit_phase` | (battle_fx_tasks.cpp PopupStart) |
| `+0x34` | s32 | `x` | 16.16; +0x36 its whole part (battle_fx_tasks.cpp) |
| `+0x38` | s32 | `y` | 16.16; +0x3A its whole part |
| `+0x60` | s32 | `value` | the number shown (battle_fx_tasks.cpp; read as a word by the draw) |
| `+0x80` | u32 | `owner` | address of the owning actor (battle_flow_callees.h kTaskOwner) |

Padding:

- +2..+4: zeroed by BattleTask_FreeCurrent, unnamed
- +8: unread for the task itself
- +0xB: compared with 1 by PopupStart (y 9 higher); meaning not named
- +0x14..+0x17: unread
- +0x20..+0x26: unread
- +0x28..+0x31: unread here (+0x2C is the owner's, not the slot's)
- +0x3C..+0x5F: +0x48, +0x5D..+0x5F zeroed by BattleTask_FreeCurrent; unnamed
- +0x64..+0x7F: unread

### `WindowRecord` (stride `0x24`): 16 named fields, 24 of 36 bytes

| off | type | field | source |
|---|---|---|---|
| `+0x00` | u8 | `in_use` | as the line above |
| `+0x01` | u8 | `handler` | index into Field_RunTaskRecords' nine |
| `+0x02` | u8 | `kind` | 0..2 |
| `+0x03` | u8 | `state` | 0..4 |
| `+0x04` | u16 | `centre_x` | 12.4 |
| `+0x06` | u16 | `centre_y` | 12.4 |
| `+0x0A` | u8 | `half_width` | pixels; battle's kind-3 name windows put the actor here instead (battle_sprites.md 3) |
| `+0x0B` | u8 | `half_height` | as the line above |
| `+0x0D` | u8 | `flags` | bit 0 no frame, bit 1 the narrow box |
| `+0x0F` | u8 | `pass` | 0..2 |
| `+0x10` | u16 | `width` | 12.4 |
| `+0x12` | u16 | `height` | 12.4 |
| `+0x14` | u16 | `width_step` | as the line above |
| `+0x16` | u16 | `height_step` | as the line above |
| `+0x18` | u16 | `settled_x` | kind 1's settled centre |
| `+0x1A` | u16 | `settled_y` | as the line above |

Padding:

- +8, +9: not in window-task.md 3
- +0xC: not in window-task.md 3
- +0xE: not in window-task.md 3
- +0x1C..+0x23: not in window-task.md 3

### `SaveSummary` (stride `0x1C`): 8 named fields, 21 of 28 bytes

| off | type | field | source |
|---|---|---|---|
| `+0x00` | char[5] | `name` | five bytes (save-interchange.md; Menu_DrawSaveSlot) |
| `+0x05` | u8[3] | `party` | icons, 0xFF none; +5 the leader's char_id (save-interchange.md) |
| `+0x08` | u8 | `level` | (Menu_DrawSaveSlot) |
| `+0x0C` | u8[2] | `time` | the two values of the time format (Menu_DrawSaveSlot); which is hours is not read |
| `+0x10` | u32 | `exp` | Menu_DrawExpBar's argument |
| `+0x14` | u8 | `marked` | non-zero: frame flags 2 and piece 0x3D (Menu_DrawSaveSlot) |
| `+0x15` | u8 | `dir_index` | Save_Directory index, 0xFF empty (save-files.md 2) |
| `+0x16` | char[4] | `name_tail` | copied after the name (Menu_DrawSaveSlot) |

Padding:

- +9..+0xB: unread by these readers
- +0xE, +0xF: unread
- +0x1A, +0x1B: unread

### `SaveDirEntry` (stride `0x18`): 2 named fields, 24 of 24 bytes

| off | type | field | source |
|---|---|---|---|
| `+0x00` | char[20] | `name` | the file name, NUL-ended; the writer copies strlen + 1 unbounded, the extent here is the gap to +0x14 |
| `+0x14` | u32 | `size` | the file size |

Padding:

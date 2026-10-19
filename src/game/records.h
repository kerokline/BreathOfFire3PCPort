// The record families of the game's data image, typed (docs/typed-records.md;
// docs/typed-data-round.md group SD). Layout only: no call site uses these
// yet. A field is named where a cited reader names it; everything else is
// `pad_<offset>`, with the gap's reason beside it. Address fields are
// std::uint32_t: the image is 32-bit whatever the host. No packing pragma:
// every named field sits at its natural alignment, which the asserts check.
//
// Written from the field lists in docs/typed-records.md; edit this file directly.
#pragma once

#include <cstddef>
#include <cstdint>

namespace records {


// The field sprite object: Sprite_Objects 0x7DEE80 (30) and Sprite_ObjectsExtra
// 0x802000 (4), numbered 0..33 (docs/sprite-draw-order.md section 5). Every
// Sprite_* function reads it through Sprite_Current. +0x80.. is the field
// object's script context (docs/movement-script.md section 1b).
// Not this family: the character records 0x903A70 + id * 0xA4 share the
// stride (kCharStride / kRecordStride) but not the layout.
struct SpriteObject {
    std::uint8_t flags;  // +0x00 bit 0 in use, 1 no image upload, 5 semi-transparent, 6 not drawn, 7 hidden / culled (sprite-draw-order 5, 14; sprite-pose 3)
    unsigned char pad_01[0x5];  // +1..+5: the state bytes battle and effect records use; no field-object reader names them here
    std::uint8_t type;  // +0x06 0x0A a party object; 8 and up never turn (sprite-draw-order 5; sprite-pose 3)
    unsigned char pad_07[0x1];  // +7: bit 7 skips the blocked test (field_objects.cpp); not named
    std::uint8_t direction;  // +0x08 facing, & 7 indexes Field_DirectionSteps (field_e2.cpp, area_w3e.cpp)
    std::uint8_t step_frames;  // +0x09 frames left in a step / a timed move; multiplies the step in Sprite_PointInReach (sprite-draw-order 6; movement-script 1b)
    unsigned char pad_0A[0x2];  // +0xA, +0xB: +0xB holds an Effect_Objects index for ops 9x (symbols.toml Effect_Spawn); +0xA unread
    std::int32_t step_x;  // +0x0C the step, 16.16 a frame (sprite-draw-order 6)
    std::int32_t step_z;  // +0x10 (sprite-draw-order 6)
    unsigned char pad_14[0x10];  // +0x14..+0x23: dwords cleared by Battle_SetupEnemy and Sprite_ClearSteps; unnamed
    std::uint8_t draw_flags;  // +0x24 bit 0 frames uploaded, 1 member set-up pending, 2 CLUT row 0x1F0, 6 a 3D object, 7 overlay list (sprite-pose 3; sprite-draw-order 5, 14; movement-script 1c)
    std::uint8_t tex_slot;  // +0x25 texture slot (sprite-pose 3)
    std::uint8_t tex_row;  // +0x26 the slot's y row (sprite-pose 3)
    std::uint8_t clut;  // +0x27 palette (sprite-draw-order 14 Sprite_ClutWord)
    std::uint8_t clut_mode;  // +0x28 palettes to a row; non-zero is "wide" (sprite-draw-order 14)
    std::uint8_t ot_slot;  // +0x29 the draw slot Gfx_CommitPrim takes (sprite-draw-order 14)
    std::uint8_t pose;  // +0x2A pose / flip byte (sprite-pose 3; sprite-draw-order 14)
    std::uint8_t key_class;  // +0x2B row of Sprite_KeyAdjust (sprite_screen.cpp 0x588F20)
    std::uint16_t column;  // +0x2C frame-offset table column (sprite-pose 3)
    std::int16_t screen_x;  // +0x2E truncated screen point (movement-script 1c; sprite-draw-order 14)
    std::int16_t screen_y;  // +0x30
    std::uint16_t draw_key;  // +0x32 layer << 8 | sort byte (sprite-draw-order 2; movement-script 1c)
    std::int32_t x;  // +0x34 position, 16.16; the readers also take its high word +0x36 as the cell (sprite_screen.cpp)
    std::int32_t z;  // +0x38 position, 16.16; high word +0x3A likewise
    std::int32_t height;  // +0x3C the pass's third sort key (sprite-draw-order 2); its high word +0x3E is the s16 elevation Sprite_ObjectMatrix reads (sprite-draw-order 12)
    std::int32_t scale_x;  // +0x40 16.16 (sprite-draw-order 12, 14)
    std::int32_t scale_y;  // +0x44 16.16 (sprite-draw-order 14)
    std::uint8_t scale_mode;  // +0x48 1: scale by depth; 0: 1.0 (sprite-draw-order 12, 14)
    std::uint8_t script_steps;  // +0x49 step count (sprite-draw-order 6 Sprite_ScriptStart)
    std::uint8_t script_hold;  // +0x4A ticks left on the step (sprite-draw-order 5, 6)
    std::uint8_t animation;  // +0x4B the current animation (sprite-pose 3)
    std::uint32_t anim_data;  // +0x4C address: the animation data (sprite-pose 3)
    std::uint32_t script;  // +0x50 address: the byte script / a 3D object's quads (sprite-draw-order 5, 12)
    std::uint32_t frames;  // +0x54 address: the frame-word table (sprite-draw-order 5, 6)
    std::uint16_t script_pos;  // +0x58 (sprite-draw-order 5)
    std::uint16_t frame_word;  // +0x5A (sprite-draw-order 5, 14)
    std::uint8_t draw_mode;  // +0x5C into the mode word (sprite-draw-order 14)
    std::uint8_t shade[3];  // +0x5D (sprite-draw-order 12, 14)
    std::int32_t depth;  // +0x60 (movement-script 1c; sprite-draw-order 14)
    std::uint32_t angle[3];  // +0x64 the rotation angles in the low words (sprite-draw-order 12); +0x64 / +0x68 also the member / its row during a member set-up (sprite-pose 3)
    std::uint32_t reach;  // +0x70 dword (sprite-pose 3); the low byte is the reach Sprite_PointInReach reads (sprite-draw-order 6)
    std::uint32_t screen_xf;  // +0x74 the float screen point's bits (movement-script 1c; sprite_screen.cpp)
    std::uint32_t screen_yf;  // +0x78
    unsigned char pad_7C[0x4];  // +0x7C..+0x7F: unread
    std::uint8_t context;  // +0x80 the script context's first byte: flags (movement-script 1b; field_objects.cpp)
    std::uint8_t pose_wait;  // +0x81 (field_objects.cpp)
    std::uint8_t loop;  // +0x82 the context's loop counter (movement-script 1b)
    std::uint8_t area_script;  // +0x83 which of the area's scripts (movement-script 1b)
    std::uint8_t speed;  // +0x84 index into Field_MoveSpeeds (movement-script 1b)
    unsigned char pad_85[0x2];  // +0x85, +0x86: unread
    std::uint8_t move_steps;  // +0x87 steps left in a timed move (movement-script 1b)
    unsigned char pad_88[0x2];  // +0x88, +0x89: unread
    std::uint16_t script_position;  // +0x8A the context's script position (movement-script 1b)
    unsigned char pad_8C[0x18];  // +0x8C..+0xA3: +0x9C (a u16 zeroed) and +0xA1 are written by field_objects.cpp, not named
};

// A party member: ObjTrio 0x802D40, three records (record 0 the leader,
// Field_State). +0x00..+0x7F the member's sprite (the sprite object's
// offsets); +0x80..+0x123 a copy of the persistent character record
// 0x903A70 + id * 0xA4 (docs/area-entry.md Party_SwapMembers;
// docs/field-event.md); +0x124.. the member's own tail, which the field
// (as Field_State) and battle read differently: the battle names are used
// here and the field use is in each comment.
struct PartyMember {
    std::uint8_t flags;  // +0x00 bit 0 present in battle (battle_setup_callees.h kPresent)
    std::uint8_t state[4];  // +0x01 the actor's state bytes +1..+4 (battle_setup_callees.h kState1..4)
    std::uint8_t member;  // +0x05 member index / palette slot (area_entry_callees.h kRecPalette; battle_damage kPCureId)
    unsigned char pad_06[0x2];  // +6, +7: whom it follows / a run counter by member-sprites 2 and event_leader.md, no width stated
    std::uint8_t direction;  // +0x08 facing (member-sprites 2)
    std::uint8_t step_frames;  // +0x09 (member-sprites 2)
    unsigned char pad_0A[0x2];  // +0xA, +0xB: +0xB an effect-object index (member-sprites 2), meaning not named
    std::int32_t step_x;  // +0x0C 16.16 a frame (member-sprites 2)
    std::int32_t step_z;  // +0x10
    unsigned char pad_14[0x15];  // +0x14..+0x28: unnamed sprite bytes
    std::uint8_t ot_slot;  // +0x29 the draw slot (area_backdrop.cpp kTrioSlot; member-sprites 2)
    unsigned char pad_2A[0x2];  // +0x2A, +0x2B: the sprite's pose / key class; no member reader names them
    std::uint16_t column;  // +0x2C (area_entry_callees.h kRecColumn; sprite-pose 3 gives the u16)
    unsigned char pad_2E[0x6];  // +0x2E..+0x33: the sprite's screen point and key; no member reader names them
    std::int32_t x;  // +0x34 16.16, +0x36 the cell (area_backdrop.cpp kTrioPosition; member-sprites 2)
    std::int32_t z;  // +0x38 16.16, +0x3A the cell
    std::int32_t height;  // +0x3C third dword of the 12 bytes at +0x34 (battle_setup_callees.h kPos; effect_3a_callees.h kLeaderPoint); its high word +0x3E the ground (member-sprites 2)
    unsigned char pad_40[0x30];  // +0x40..+0x6F: sprite fields (SpriteObject); +0x5C..+0x5F written on entry (area-entry.md), not named here
    std::uint32_t raised;  // +0x70 the member readers test its low byte, "raised" (member-sprites 2; member_sprites.cpp); sprite-pose 3 writes the sprite's +0x70 as a dword
    unsigned char pad_74[0x15];  // +0x74..+0x88: the sprite's float screen point; the character record's +0..+8 (name)
    std::uint8_t char_id;  // +0x89 record +0x09 (battle_setup_callees.h kPCharId; area_entry_callees.h kRecMember)
    std::uint8_t level;  // +0x8A record +0x0A (battle_damage_callees.h kPLevel, working +0x7A)
    unsigned char pad_8B[0x5];  // +0x8B..+0x8F: character record +0x0B..+0x0F; +0x8B bit 0 "joined" per char-stats.md, no member reader addresses it
    std::uint16_t status;  // +0x90 record +0x10 (battle_setup kPStatus; battle_damage kPStatus)
    std::uint8_t weapon;  // +0x92 record +0x12 (battle_setup kPWeapon)
    unsigned char pad_93[0x3];  // +0x93..+0x95: armour (battle_setup kPArmour3 names only +0x95)
    std::uint8_t accessory[2];  // +0x96 record +0x16 / +0x17 (battle_setup kPAccessory)
    std::uint16_t hp;  // +0x98 record +0x18 (battle_setup kPHp; battle_damage kPHp)
    std::uint16_t ap;  // +0x9A record +0x1A
    std::uint8_t survive;  // +0x9C record +0x1C, set from +0xAE (battle_setup kPByte9C); the overkill a kill leaves the flag below (battle_damage kPSurvive)
    unsigned char pad_9D[0x3];  // +0x9D..+0x9F: unread
    std::uint16_t max_hp;  // +0xA0 record +0x20 (battle_setup kPMaxHp)
    std::uint16_t max_ap;  // +0xA2 record +0x22
    unsigned char pad_A4[0x2];  // +0xA4..+0xA5: record +0x24 (ATK per char-stats.md), not read through this record by a cited reader
    std::uint16_t def;  // +0xA6 record +0x26 (battle_damage kPDef)
    std::uint16_t agi;  // +0xA8 record +0x28 (battle_setup kPAgi; battle_damage kPAgi)
    unsigned char pad_AA[0x4];  // +0xAA..+0xAD: unread here
    std::uint8_t byte_ae;  // +0xAE record +0x2E, copied to +0x9C (battle_setup kPByteAE)
    std::uint8_t elements[5];  // +0xAF affinity classes, fire .. wind (battle_damage kPElements)
    std::uint8_t holy;  // +0xB4 holy affinity class (battle_damage kPHoly)
    unsigned char pad_B5[0x6F];  // +0xB5..+0x123: the rest of the character record copy (base stats at +0xC0 per battle_e4.md); not named here
    std::uint8_t command;  // +0x124 battle: the command auto-battle gives (docs/battle_e4.md); field: the script context's first byte (area_entry_callees.h kRecContext)
    std::uint8_t queued_kind;  // +0x125 5 an item, 4 an ability (battle_setup kPQueuedKind; battle_damage kPCommand)
    std::uint16_t queued_id;  // +0x126 category << 8 | id, or the ability id (battle_setup kPQueuedItem; battle_damage kPCommandId)
    std::uint16_t hp_change;  // +0x128 battle (battle_setup kPHpChange); field: its low byte the walking speed (member-sprites 2)
    std::uint16_t ap_change;  // +0x12A battle (battle_setup kPApChange)
    std::uint8_t shown;  // +0x12C status-shown bits, 0x11 / 2 with an effect (battle_setup kPEffect; battle_damage kPShown)
    std::uint8_t counter;  // +0x12D the 0x40 / 0x20 statuses' counter (battle_setup kPCounter); battle_damage kPShown2 "cleared with a cured status"
    std::uint8_t item_slot;  // +0x12E (battle_setup kPItemSlot)
    unsigned char pad_12F[0x1];  // +0x12F: unread
    std::uint32_t flags_130;  // +0x130 battle: flag dword (battle_damage kPFlags; battle_sprites.cpp "the party's flags dword"); field: the script pointer (area_entry_callees.h kRecScript)
    std::uint32_t flags_134;  // +0x134 battle: flag dword (battle_setup kPFlags; battle_damage kPFlags2); field: byte +0x136 the standing counter, +0x137 the action (member-sprites 2) - one dword in battle, two bytes in the field, so the bytes are not separate members
    std::uint8_t field_flags;  // +0x138 field: bit 0, bit 3 CLUT pending (area_backdrop.cpp kTrioFlags; member-sprites 2)
    unsigned char pad_139[0x9];  // +0x139..+0x141: unread
    std::uint8_t count_4000;  // +0x142 (battle_setup kPCount4000)
    std::uint8_t count_800;  // +0x143 (battle_setup kPCount800)
    std::uint8_t count_40;  // +0x144 zeroed with flag 0x40 (battle_setup kPCount40); battle_damage kPCharge, the attack charge
    std::uint8_t count_80;  // +0x145 zeroed with flag 0x80 (battle_setup kPCount80); battle_damage kPCharge2, the ability charge
    unsigned char pad_146[0x2];  // +0x146, +0x147: unread
    std::uint8_t char_index;  // +0x148 the character record's index (area_entry_callees.h kRecCharacterIndex; effect_3a_callees.h kMemberFlag) - battle_damage_callees.h kPWeaponKind names the same byte "5: the weapon's element rolls apply": unsettled
    unsigned char pad_149[0x3];  // +0x149..+0x14B: unread
};

// An effect object: Effect_Objects 0x7E11E0, 20 records (symbols.toml).
// Effect_RunObjects makes each in-use record Sprite_Current and calls
// Effect_KindHandlers[+5], so the sprite readers apply to it as well; only
// what the effect pool's own readers set is named here.
struct EffectObject {
    std::uint8_t in_use;  // +0x00 non-zero in use (Effect_FindFree / Effect_RunObjects, symbols.toml)
    unsigned char pad_01[0x4];  // +1..+4: 0x589870 zeroes bytes 0..4 (symbols.toml); not named for this family
    std::uint8_t kind;  // +0x05 indexes Effect_KindHandlers (symbols.toml Effect_RunObjects; Effect_Spawn sets 6)
    std::uint8_t param;  // +0x06 Effect_Spawn's first argument (symbols.toml Effect_Spawn)
    unsigned char pad_07[0x5];  // +7..+0xB: +0xB a spawn argument (0x5898D0), not named
    std::int32_t step_x;  // +0x0C dword, Effect_Spawn's movsx byte (symbols.toml)
    std::int32_t step_z;  // +0x10
    unsigned char pad_14[0x1A];  // +0x14..+0x2D: unread by the pool's readers
    std::int16_t screen_x;  // +0x2E Effect_Spawn's word arguments (symbols.toml)
    std::int16_t screen_y;  // +0x30
    unsigned char pad_32[0x2];  // +0x32, +0x33: the sprite's draw key, not named for this family
    std::int32_t x;  // +0x34 dword read by AreaMap_Elevation; its u16 halves written apart (symbols.toml, EventScript_Op's spawn 0x5898D0)
    std::int32_t z;  // +0x38
    std::int32_t height;  // +0x3C its high word +0x3E the u16 elevation written by the spawn 0x5898D0 (symbols.toml)
    unsigned char pad_40[0x40];  // +0x40..+0x7F: the sprite fields, not named for this family
};

// A battle enemy: 0x93B960 + (actor - 3) * 0x128, eight records. +0x00..+0x7F
// its sprite object, +0x80.. EnemyWorkingRecords 0x93B9E0 (battle_sprites.cpp,
// docs/battle_sprites.md 3), filled by Battle_CopyEnemyData from an
// EnemyData record. battle_damage_callees.h measures from +0x80.
struct BattleEnemy {
    std::uint8_t flags;  // +0x00 bit 0 present, 0x40 defeated (battle_setup kPresent; battle_flow.md)
    std::uint8_t state[4];  // +0x01 (battle_setup kState1..4)
    std::uint8_t actor;  // +0x05 slot + 3 (Battle_SetupEnemy, battle_sprites.cpp)
    unsigned char pad_06[0x2];  // +6, +7: sprite bytes, no enemy reader names them
    std::uint8_t layout;  // +0x08 the layout ^ 2 (Battle_SetupEnemy); 2 / 3 pick the next animation (battle_flow.md)
    unsigned char pad_09[0x20];  // +9..+0x28: +0xC..+0x23 dwords cleared at set-up, +0x24 cleared; not named
    std::uint8_t ot_slot;  // +0x29 4 (Battle_SetupEnemy)
    std::uint8_t flip;  // +0x2A (battle_flow.md BattleEnemy_SetAnimation)
    unsigned char pad_2B[0x9];  // +0x2B..+0x33: unread here
    std::int32_t x;  // +0x34 (Battle_SetupEnemy; battle_setup kPos)
    std::int32_t z;  // +0x38
    std::int32_t height;  // +0x3C its high word +0x3E the ground (Battle_SetupEnemy)
    unsigned char pad_40[0xB];  // +0x40..+0x4A: sprite fields, not named here
    std::uint8_t animation;  // +0x4B 0xFF at set-up (Battle_SetupEnemy)
    unsigned char pad_4C[0x34];  // +0x4C..+0x7F: sprite fields, not named here
    char name[0xc];  // +0x80 from EnemyData +0 (battle_sprites.md 3)
    std::uint8_t kind;  // +0x8C EnemyData +0xC; grouped by Battle_OpenEnemyNames (battle_sprites.cpp)
    std::uint8_t family;  // +0x8D EnemyData +0xD (battle_damage kEFamily)
    std::uint8_t odds_index;  // +0x8E EnemyData +0xE (Battle_CopyEnemyData)
    std::uint8_t kind_flag;  // +0x8F Battle_EnemyKindFlag (Battle_SetupEnemy)
    std::uint16_t flags_90;  // +0x90 EnemyData +0x12 (Battle_CopyEnemyData); bit 1 read by BattleEnemy_Chance70 (battle_flow.cpp)
    std::uint16_t status;  // +0x92 (battle_setup kEStatus; battle_damage kEStatus); 0x4000 dead (battle_flow.cpp)
    std::uint16_t zenny;  // +0x94 (battle_flow.cpp, the kill)
    std::uint16_t exp;  // +0x96 (battle_flow.cpp, the kill)
    std::uint16_t level;  // +0x98 battle_damage kELevel: u8, also read as a u16
    std::uint16_t bank;  // +0x9A the animation bank, EnemyData +0x1A (Battle_SetupEnemy)
    std::uint8_t abilities[8];  // +0x9C EnemyData +0x1C (Battle_CopyEnemyData)
    std::uint16_t hp;  // +0xA4 (battle_setup kEHp; battle_damage kEHp)
    std::uint16_t ap;  // +0xA6 (battle_damage kEAp)
    unsigned char pad_A8[0x8];  // +0xA8..+0xAF: EnemyData +0x30 / +0x34 dwords (Battle_CopyEnemyData), unnamed
    std::uint16_t max_hp;  // +0xB0 (battle_setup kEMaxHp)
    std::uint16_t max_ap;  // +0xB2 (battle_damage kEMaxAp)
    unsigned char pad_B4[0x4];  // +0xB4..+0xB7: the stat block's copy at +0xB0.. (Battle_CopyEnemyData), unnamed
    std::uint16_t agi;  // +0xB8 (battle_setup kEAgi)
    unsigned char pad_BA[0x5];  // +0xBA..+0xBE: likewise; stat names are hypotheses (tools/enemy_ai.py)
    std::uint8_t elements[5];  // +0xBF (battle_damage kEElements)
    std::uint8_t holy;  // +0xC4 (battle_damage kEHoly)
    unsigned char pad_C5[0x2B];  // +0xC5..+0xEF: copies of the stat block (+0xD0..), unnamed
    std::uint8_t ai_script;  // +0xF0 the data id (battle_damage kEScript; Battle_CopyEnemyData)
    std::uint8_t ai_rows_done;  // +0xF1 a bit per AI row fired; DIV-0082 (battle_damage kERowsDone)
    std::int8_t popup_dx;  // +0xF2 (battle_fx_tasks.cpp BattleFx_PopupStart)
    std::int8_t popup_dy;  // +0xF3
    std::uint32_t event_hook;  // +0xF4 address, called with 2 in an event battle (battle_flow.md)
    unsigned char pad_F8[0x4];  // +0xF8..+0xFB: unread
    std::uint32_t anim_table;  // +0xFC address of the animation bytes (battle_flow.cpp BattleEnemy_SetAnimation)
    std::uint8_t run_state;  // +0x100 indexes the state table 0x64B084 (battle_flow.md); EnemyData +0x88
    unsigned char pad_101[0x4];  // +0x101: EnemyData +0x89; +0x102..+0x104 unread
    std::uint8_t command;  // +0x105 (battle_damage kECommand)
    unsigned char pad_106[0x2];  // +0x106, +0x107: unread
    std::uint16_t hp_change;  // +0x108 (battle_setup kEHpChange)
    std::uint16_t ap_change;  // +0x10A (battle_setup kEApChange)
    std::uint8_t shown;  // +0x10C (battle_setup kEEffect; battle_damage kEShown)
    std::uint8_t counter;  // +0x10D (battle_setup kECounter; battle_damage kEShown2)
    unsigned char pad_10E[0x2];  // +0x10E, +0x10F: unread
    std::uint32_t flags_110;  // +0x110 bit 15, bits 0x70, the popup's 0x80 (battle_damage kEFlags; battle_flow.cpp; battle_fx_tasks.cpp)
    std::uint32_t flags_114;  // +0x114 bit 0x10 (battle_setup kEFlags; battle_damage kEFlags2)
    unsigned char pad_118[0xA];  // +0x118..+0x121: +0x118..+0x11F zeroed at set-up, unnamed
    std::uint8_t count_4000;  // +0x122 (battle_setup kECount4000)
    unsigned char pad_123[0x1];  // +0x123: zeroed at set-up, unnamed
    std::uint8_t count_40;  // +0x124 (battle_setup kECount40); battle_damage kECharge
    std::uint8_t count_80;  // +0x125 (battle_setup kECount80); battle_damage kECharge2
    unsigned char pad_126[0x2];  // +0x126, +0x127: unread
};

// An area's enemy data record: 0x8C55C8 + id * 0x8C, eight per area, loaded
// from the area's chunk 0xC2000 (docs/enemy_ai_data.md 1). The port widened
// the PSX's 0x88 record by its name field (docs/DAT_CONTAINER.md). Layout from
// Battle_CopyEnemyData (battle_sprites.cpp) and tools/enemy_ai.py's header.
struct EnemyData {
    char name[0xc];  // +0x00 (enemy_ai.py; battle_sprites.md 3)
    std::uint8_t kind;  // +0x0C to BattleEnemy +0x8C; Battle_EnemyKindFlag's argument
    std::uint8_t family;  // +0x0D to BattleEnemy +0x8D
    std::uint8_t odds_index;  // +0x0E the base action-odds index (enemy_ai.py)
    unsigned char pad_0F[0x3];  // +0xF..+0x11: unread
    std::uint16_t flags;  // +0x12 (enemy_ai.py); to BattleEnemy +0x90
    std::uint16_t zenny;  // +0x14 to BattleEnemy +0x94
    std::uint16_t exp;  // +0x16 to BattleEnemy +0x96
    std::uint16_t level;  // +0x18 (enemy_ai.py reads a u16)
    std::uint16_t bank;  // +0x1A the animation bank (Battle_SetupEnemy)
    std::uint8_t abilities[8];  // +0x1C one picked by Rand & 7 (enemy_ai.py)
    std::uint16_t hp;  // +0x24 (enemy_ai.py)
    std::uint16_t ap;  // +0x26
    std::uint16_t stats[4];  // +0x28 names hypotheses (enemy_ai.py)
    unsigned char pad_30[0x8];  // +0x30..+0x37: two dwords copied to BattleEnemy +0xA8 / +0xAC, unnamed
    std::uint8_t ai_rows[0x40];  // +0x38 four rows of 16 bytes (enemy_ai_data.md 1; enemy_ai.py)
    unsigned char pad_78[0x4];  // +0x78..+0x7B: unread
    std::uint8_t resist[9];  // +0x7C to BattleEnemy +0xDF.. and +0xBF.. (Battle_CopyEnemyData)
    std::uint8_t status;  // +0x85 to BattleEnemy +0x92 (Battle_CopyEnemyData)
    unsigned char pad_86[0x2];  // +0x86, +0x87: unread
    std::uint8_t run_state;  // +0x88 to BattleEnemy +0x100 (Battle_CopyEnemyData)
    unsigned char pad_89[0x3];  // +0x89: to BattleEnemy +0x101, unnamed; +0x8A, +0x8B unread
};

// A battle task: 0x93A000, 48 slots (battle_flow_callees.h kTasks). Not the
// scheduler's 0x20-byte Task_Records (docs/task_sched.md). BattleTask_RunAll
// makes the slot Sprite_Current, so its first 0x80 bytes are a sprite's;
// named here: what docs/battle_fx_tasks.md 1 and battle_flow.cpp read.
struct BattleTask {
    std::uint8_t flags;  // +0x00 bit 0 taken; non-zero runs (battle_flow.cpp BattleTask_Create / RunAll)
    std::uint8_t state;  // +0x01 (battle_fx_tasks.md 1)
    unsigned char pad_02[0x3];  // +2..+4: zeroed by BattleTask_FreeCurrent, unnamed
    std::uint8_t param;  // +0x05 the effect / row (BattleTask_Create; battle_fx_tasks.md 1)
    std::uint8_t kind;  // +0x06 0..3 (BattleTask_RunAll)
    std::uint8_t mode;  // +0x07 (battle_fx_tasks.md 1, the popup's draw)
    unsigned char pad_08[0x1];  // +8: unread for the task itself
    std::uint8_t timer;  // +0x09 (battle_fx_tasks.cpp popup)
    std::uint8_t digits;  // +0x0A the digit count (battle_fx_tasks.cpp)
    unsigned char pad_0B[0x1];  // +0xB: compared with 1 by PopupStart (y 9 higher); meaning not named
    std::int32_t vel_x;  // +0x0C 16.16 (battle_fx_tasks.cpp PopupRise / Bounce)
    std::int32_t vel_y;  // +0x10
    unsigned char pad_14[0x4];  // +0x14..+0x17: unread
    std::int32_t start_x;  // +0x18 (battle_fx_tasks.cpp PopupRise)
    std::int32_t start_y;  // +0x1C
    unsigned char pad_20[0x7];  // +0x20..+0x26: unread
    std::uint8_t clut;  // +0x27 (battle_fx_tasks.cpp DamagePopup)
    unsigned char pad_28[0xA];  // +0x28..+0x31: unread here (+0x2C is the owner's, not the slot's)
    std::uint16_t digit_phase;  // +0x32 (battle_fx_tasks.cpp PopupStart)
    std::int32_t x;  // +0x34 16.16; +0x36 its whole part (battle_fx_tasks.cpp)
    std::int32_t y;  // +0x38 16.16; +0x3A its whole part
    unsigned char pad_3C[0x24];  // +0x3C..+0x5F: +0x48, +0x5D..+0x5F zeroed by BattleTask_FreeCurrent; unnamed
    std::int32_t value;  // +0x60 the number shown (battle_fx_tasks.cpp; read as a word by the draw)
    unsigned char pad_64[0x1C];  // +0x64..+0x7F: unread
    std::uint32_t owner;  // +0x80 address of the owning actor (battle_flow_callees.h kTaskOwner)
};

// A window record: WindowRecords 0x803160, 22 records (docs/window-task.md 3,
// whose table every field below is from).
struct WindowRecord {
    std::uint8_t in_use;  // +0x00
    std::uint8_t handler;  // +0x01 index into Field_RunTaskRecords' nine
    std::uint8_t kind;  // +0x02 0..2
    std::uint8_t state;  // +0x03 0..4
    std::uint16_t centre_x;  // +0x04 12.4
    std::uint16_t centre_y;  // +0x06 12.4
    unsigned char pad_08[0x2];  // +8, +9: not in window-task.md 3
    std::uint8_t half_width;  // +0x0A pixels; battle's kind-3 name windows put the actor here instead (battle_sprites.md 3)
    std::uint8_t half_height;  // +0x0B
    unsigned char pad_0C[0x1];  // +0xC: not in window-task.md 3
    std::uint8_t flags;  // +0x0D bit 0 no frame, bit 1 the narrow box
    unsigned char pad_0E[0x1];  // +0xE: not in window-task.md 3
    std::uint8_t pass;  // +0x0F 0..2
    std::uint16_t width;  // +0x10 12.4
    std::uint16_t height;  // +0x12 12.4
    std::uint16_t width_step;  // +0x14
    std::uint16_t height_step;  // +0x16
    std::uint16_t settled_x;  // +0x18 kind 1's settled centre
    std::uint16_t settled_y;  // +0x1A
    unsigned char pad_1C[0x8];  // +0x1C..+0x23: not in window-task.md 3
};

// A save slot's summary: 0x905BC0, 16 records; file offset 0xCA0 of a save
// (docs/save-files.md 2, docs/save-interchange.md). Fields from
// Menu_DrawSaveSlot 0x576960 (field_o.cpp) and Save_ReadSummaries.
struct SaveSummary {
    char name[5];  // +0x00 five bytes (save-interchange.md; Menu_DrawSaveSlot)
    std::uint8_t party[3];  // +0x05 icons, 0xFF none; +5 the leader's char_id (save-interchange.md)
    std::uint8_t level;  // +0x08 (Menu_DrawSaveSlot)
    unsigned char pad_09[0x3];  // +9..+0xB: unread by these readers
    std::uint8_t time[2];  // +0x0C the two values of the time format (Menu_DrawSaveSlot); which is hours is not read
    unsigned char pad_0E[0x2];  // +0xE, +0xF: unread
    std::uint32_t exp;  // +0x10 Menu_DrawExpBar's argument
    std::uint8_t marked;  // +0x14 non-zero: frame flags 2 and piece 0x3D (Menu_DrawSaveSlot)
    std::uint8_t dir_index;  // +0x15 Save_Directory index, 0xFF empty (save-files.md 2)
    char name_tail[4];  // +0x16 copied after the name (Menu_DrawSaveSlot)
    unsigned char pad_1A[0x2];  // +0x1A, +0x1B: unread
};

// A Save_Directory entry: 0x929F40, 16 records (docs/save-files.md 2;
// Save_ListFiles, save_menu.cpp).
struct SaveDirEntry {
    char name[0x14];  // +0x00 the file name, NUL-ended; the writer copies strlen + 1 unbounded, the extent here is the gap to +0x14
    std::uint32_t size;  // +0x14 the file size
};

static_assert(offsetof(SpriteObject, flags) == 0x0, "SpriteObject::flags");
static_assert(offsetof(SpriteObject, type) == 0x6, "SpriteObject::type");
static_assert(offsetof(SpriteObject, direction) == 0x8, "SpriteObject::direction");
static_assert(offsetof(SpriteObject, step_frames) == 0x9, "SpriteObject::step_frames");
static_assert(offsetof(SpriteObject, step_x) == 0xC, "SpriteObject::step_x");
static_assert(offsetof(SpriteObject, step_z) == 0x10, "SpriteObject::step_z");
static_assert(offsetof(SpriteObject, draw_flags) == 0x24, "SpriteObject::draw_flags");
static_assert(offsetof(SpriteObject, tex_slot) == 0x25, "SpriteObject::tex_slot");
static_assert(offsetof(SpriteObject, tex_row) == 0x26, "SpriteObject::tex_row");
static_assert(offsetof(SpriteObject, clut) == 0x27, "SpriteObject::clut");
static_assert(offsetof(SpriteObject, clut_mode) == 0x28, "SpriteObject::clut_mode");
static_assert(offsetof(SpriteObject, ot_slot) == 0x29, "SpriteObject::ot_slot");
static_assert(offsetof(SpriteObject, pose) == 0x2A, "SpriteObject::pose");
static_assert(offsetof(SpriteObject, key_class) == 0x2B, "SpriteObject::key_class");
static_assert(offsetof(SpriteObject, column) == 0x2C, "SpriteObject::column");
static_assert(offsetof(SpriteObject, screen_x) == 0x2E, "SpriteObject::screen_x");
static_assert(offsetof(SpriteObject, screen_y) == 0x30, "SpriteObject::screen_y");
static_assert(offsetof(SpriteObject, draw_key) == 0x32, "SpriteObject::draw_key");
static_assert(offsetof(SpriteObject, x) == 0x34, "SpriteObject::x");
static_assert(offsetof(SpriteObject, z) == 0x38, "SpriteObject::z");
static_assert(offsetof(SpriteObject, height) == 0x3C, "SpriteObject::height");
static_assert(offsetof(SpriteObject, scale_x) == 0x40, "SpriteObject::scale_x");
static_assert(offsetof(SpriteObject, scale_y) == 0x44, "SpriteObject::scale_y");
static_assert(offsetof(SpriteObject, scale_mode) == 0x48, "SpriteObject::scale_mode");
static_assert(offsetof(SpriteObject, script_steps) == 0x49, "SpriteObject::script_steps");
static_assert(offsetof(SpriteObject, script_hold) == 0x4A, "SpriteObject::script_hold");
static_assert(offsetof(SpriteObject, animation) == 0x4B, "SpriteObject::animation");
static_assert(offsetof(SpriteObject, anim_data) == 0x4C, "SpriteObject::anim_data");
static_assert(offsetof(SpriteObject, script) == 0x50, "SpriteObject::script");
static_assert(offsetof(SpriteObject, frames) == 0x54, "SpriteObject::frames");
static_assert(offsetof(SpriteObject, script_pos) == 0x58, "SpriteObject::script_pos");
static_assert(offsetof(SpriteObject, frame_word) == 0x5A, "SpriteObject::frame_word");
static_assert(offsetof(SpriteObject, draw_mode) == 0x5C, "SpriteObject::draw_mode");
static_assert(offsetof(SpriteObject, shade) == 0x5D, "SpriteObject::shade");
static_assert(offsetof(SpriteObject, depth) == 0x60, "SpriteObject::depth");
static_assert(offsetof(SpriteObject, angle) == 0x64, "SpriteObject::angle");
static_assert(offsetof(SpriteObject, reach) == 0x70, "SpriteObject::reach");
static_assert(offsetof(SpriteObject, screen_xf) == 0x74, "SpriteObject::screen_xf");
static_assert(offsetof(SpriteObject, screen_yf) == 0x78, "SpriteObject::screen_yf");
static_assert(offsetof(SpriteObject, context) == 0x80, "SpriteObject::context");
static_assert(offsetof(SpriteObject, pose_wait) == 0x81, "SpriteObject::pose_wait");
static_assert(offsetof(SpriteObject, loop) == 0x82, "SpriteObject::loop");
static_assert(offsetof(SpriteObject, area_script) == 0x83, "SpriteObject::area_script");
static_assert(offsetof(SpriteObject, speed) == 0x84, "SpriteObject::speed");
static_assert(offsetof(SpriteObject, move_steps) == 0x87, "SpriteObject::move_steps");
static_assert(offsetof(SpriteObject, script_position) == 0x8A, "SpriteObject::script_position");
static_assert(sizeof(SpriteObject) == 0xA4, "SpriteObject stride");

static_assert(offsetof(PartyMember, flags) == 0x0, "PartyMember::flags");
static_assert(offsetof(PartyMember, state) == 0x1, "PartyMember::state");
static_assert(offsetof(PartyMember, member) == 0x5, "PartyMember::member");
static_assert(offsetof(PartyMember, direction) == 0x8, "PartyMember::direction");
static_assert(offsetof(PartyMember, step_frames) == 0x9, "PartyMember::step_frames");
static_assert(offsetof(PartyMember, step_x) == 0xC, "PartyMember::step_x");
static_assert(offsetof(PartyMember, step_z) == 0x10, "PartyMember::step_z");
static_assert(offsetof(PartyMember, ot_slot) == 0x29, "PartyMember::ot_slot");
static_assert(offsetof(PartyMember, column) == 0x2C, "PartyMember::column");
static_assert(offsetof(PartyMember, x) == 0x34, "PartyMember::x");
static_assert(offsetof(PartyMember, z) == 0x38, "PartyMember::z");
static_assert(offsetof(PartyMember, height) == 0x3C, "PartyMember::height");
static_assert(offsetof(PartyMember, raised) == 0x70, "PartyMember::raised");
static_assert(offsetof(PartyMember, char_id) == 0x89, "PartyMember::char_id");
static_assert(offsetof(PartyMember, level) == 0x8A, "PartyMember::level");
static_assert(offsetof(PartyMember, status) == 0x90, "PartyMember::status");
static_assert(offsetof(PartyMember, weapon) == 0x92, "PartyMember::weapon");
static_assert(offsetof(PartyMember, accessory) == 0x96, "PartyMember::accessory");
static_assert(offsetof(PartyMember, hp) == 0x98, "PartyMember::hp");
static_assert(offsetof(PartyMember, ap) == 0x9A, "PartyMember::ap");
static_assert(offsetof(PartyMember, survive) == 0x9C, "PartyMember::survive");
static_assert(offsetof(PartyMember, max_hp) == 0xA0, "PartyMember::max_hp");
static_assert(offsetof(PartyMember, max_ap) == 0xA2, "PartyMember::max_ap");
static_assert(offsetof(PartyMember, def) == 0xA6, "PartyMember::def");
static_assert(offsetof(PartyMember, agi) == 0xA8, "PartyMember::agi");
static_assert(offsetof(PartyMember, byte_ae) == 0xAE, "PartyMember::byte_ae");
static_assert(offsetof(PartyMember, elements) == 0xAF, "PartyMember::elements");
static_assert(offsetof(PartyMember, holy) == 0xB4, "PartyMember::holy");
static_assert(offsetof(PartyMember, command) == 0x124, "PartyMember::command");
static_assert(offsetof(PartyMember, queued_kind) == 0x125, "PartyMember::queued_kind");
static_assert(offsetof(PartyMember, queued_id) == 0x126, "PartyMember::queued_id");
static_assert(offsetof(PartyMember, hp_change) == 0x128, "PartyMember::hp_change");
static_assert(offsetof(PartyMember, ap_change) == 0x12A, "PartyMember::ap_change");
static_assert(offsetof(PartyMember, shown) == 0x12C, "PartyMember::shown");
static_assert(offsetof(PartyMember, counter) == 0x12D, "PartyMember::counter");
static_assert(offsetof(PartyMember, item_slot) == 0x12E, "PartyMember::item_slot");
static_assert(offsetof(PartyMember, flags_130) == 0x130, "PartyMember::flags_130");
static_assert(offsetof(PartyMember, flags_134) == 0x134, "PartyMember::flags_134");
static_assert(offsetof(PartyMember, field_flags) == 0x138, "PartyMember::field_flags");
static_assert(offsetof(PartyMember, count_4000) == 0x142, "PartyMember::count_4000");
static_assert(offsetof(PartyMember, count_800) == 0x143, "PartyMember::count_800");
static_assert(offsetof(PartyMember, count_40) == 0x144, "PartyMember::count_40");
static_assert(offsetof(PartyMember, count_80) == 0x145, "PartyMember::count_80");
static_assert(offsetof(PartyMember, char_index) == 0x148, "PartyMember::char_index");
static_assert(sizeof(PartyMember) == 0x14C, "PartyMember stride");

static_assert(offsetof(EffectObject, in_use) == 0x0, "EffectObject::in_use");
static_assert(offsetof(EffectObject, kind) == 0x5, "EffectObject::kind");
static_assert(offsetof(EffectObject, param) == 0x6, "EffectObject::param");
static_assert(offsetof(EffectObject, step_x) == 0xC, "EffectObject::step_x");
static_assert(offsetof(EffectObject, step_z) == 0x10, "EffectObject::step_z");
static_assert(offsetof(EffectObject, screen_x) == 0x2E, "EffectObject::screen_x");
static_assert(offsetof(EffectObject, screen_y) == 0x30, "EffectObject::screen_y");
static_assert(offsetof(EffectObject, x) == 0x34, "EffectObject::x");
static_assert(offsetof(EffectObject, z) == 0x38, "EffectObject::z");
static_assert(offsetof(EffectObject, height) == 0x3C, "EffectObject::height");
static_assert(sizeof(EffectObject) == 0x80, "EffectObject stride");

static_assert(offsetof(BattleEnemy, flags) == 0x0, "BattleEnemy::flags");
static_assert(offsetof(BattleEnemy, state) == 0x1, "BattleEnemy::state");
static_assert(offsetof(BattleEnemy, actor) == 0x5, "BattleEnemy::actor");
static_assert(offsetof(BattleEnemy, layout) == 0x8, "BattleEnemy::layout");
static_assert(offsetof(BattleEnemy, ot_slot) == 0x29, "BattleEnemy::ot_slot");
static_assert(offsetof(BattleEnemy, flip) == 0x2A, "BattleEnemy::flip");
static_assert(offsetof(BattleEnemy, x) == 0x34, "BattleEnemy::x");
static_assert(offsetof(BattleEnemy, z) == 0x38, "BattleEnemy::z");
static_assert(offsetof(BattleEnemy, height) == 0x3C, "BattleEnemy::height");
static_assert(offsetof(BattleEnemy, animation) == 0x4B, "BattleEnemy::animation");
static_assert(offsetof(BattleEnemy, name) == 0x80, "BattleEnemy::name");
static_assert(offsetof(BattleEnemy, kind) == 0x8C, "BattleEnemy::kind");
static_assert(offsetof(BattleEnemy, family) == 0x8D, "BattleEnemy::family");
static_assert(offsetof(BattleEnemy, odds_index) == 0x8E, "BattleEnemy::odds_index");
static_assert(offsetof(BattleEnemy, kind_flag) == 0x8F, "BattleEnemy::kind_flag");
static_assert(offsetof(BattleEnemy, flags_90) == 0x90, "BattleEnemy::flags_90");
static_assert(offsetof(BattleEnemy, status) == 0x92, "BattleEnemy::status");
static_assert(offsetof(BattleEnemy, zenny) == 0x94, "BattleEnemy::zenny");
static_assert(offsetof(BattleEnemy, exp) == 0x96, "BattleEnemy::exp");
static_assert(offsetof(BattleEnemy, level) == 0x98, "BattleEnemy::level");
static_assert(offsetof(BattleEnemy, bank) == 0x9A, "BattleEnemy::bank");
static_assert(offsetof(BattleEnemy, abilities) == 0x9C, "BattleEnemy::abilities");
static_assert(offsetof(BattleEnemy, hp) == 0xA4, "BattleEnemy::hp");
static_assert(offsetof(BattleEnemy, ap) == 0xA6, "BattleEnemy::ap");
static_assert(offsetof(BattleEnemy, max_hp) == 0xB0, "BattleEnemy::max_hp");
static_assert(offsetof(BattleEnemy, max_ap) == 0xB2, "BattleEnemy::max_ap");
static_assert(offsetof(BattleEnemy, agi) == 0xB8, "BattleEnemy::agi");
static_assert(offsetof(BattleEnemy, elements) == 0xBF, "BattleEnemy::elements");
static_assert(offsetof(BattleEnemy, holy) == 0xC4, "BattleEnemy::holy");
static_assert(offsetof(BattleEnemy, ai_script) == 0xF0, "BattleEnemy::ai_script");
static_assert(offsetof(BattleEnemy, ai_rows_done) == 0xF1, "BattleEnemy::ai_rows_done");
static_assert(offsetof(BattleEnemy, popup_dx) == 0xF2, "BattleEnemy::popup_dx");
static_assert(offsetof(BattleEnemy, popup_dy) == 0xF3, "BattleEnemy::popup_dy");
static_assert(offsetof(BattleEnemy, event_hook) == 0xF4, "BattleEnemy::event_hook");
static_assert(offsetof(BattleEnemy, anim_table) == 0xFC, "BattleEnemy::anim_table");
static_assert(offsetof(BattleEnemy, run_state) == 0x100, "BattleEnemy::run_state");
static_assert(offsetof(BattleEnemy, command) == 0x105, "BattleEnemy::command");
static_assert(offsetof(BattleEnemy, hp_change) == 0x108, "BattleEnemy::hp_change");
static_assert(offsetof(BattleEnemy, ap_change) == 0x10A, "BattleEnemy::ap_change");
static_assert(offsetof(BattleEnemy, shown) == 0x10C, "BattleEnemy::shown");
static_assert(offsetof(BattleEnemy, counter) == 0x10D, "BattleEnemy::counter");
static_assert(offsetof(BattleEnemy, flags_110) == 0x110, "BattleEnemy::flags_110");
static_assert(offsetof(BattleEnemy, flags_114) == 0x114, "BattleEnemy::flags_114");
static_assert(offsetof(BattleEnemy, count_4000) == 0x122, "BattleEnemy::count_4000");
static_assert(offsetof(BattleEnemy, count_40) == 0x124, "BattleEnemy::count_40");
static_assert(offsetof(BattleEnemy, count_80) == 0x125, "BattleEnemy::count_80");
static_assert(sizeof(BattleEnemy) == 0x128, "BattleEnemy stride");

static_assert(offsetof(EnemyData, name) == 0x0, "EnemyData::name");
static_assert(offsetof(EnemyData, kind) == 0xC, "EnemyData::kind");
static_assert(offsetof(EnemyData, family) == 0xD, "EnemyData::family");
static_assert(offsetof(EnemyData, odds_index) == 0xE, "EnemyData::odds_index");
static_assert(offsetof(EnemyData, flags) == 0x12, "EnemyData::flags");
static_assert(offsetof(EnemyData, zenny) == 0x14, "EnemyData::zenny");
static_assert(offsetof(EnemyData, exp) == 0x16, "EnemyData::exp");
static_assert(offsetof(EnemyData, level) == 0x18, "EnemyData::level");
static_assert(offsetof(EnemyData, bank) == 0x1A, "EnemyData::bank");
static_assert(offsetof(EnemyData, abilities) == 0x1C, "EnemyData::abilities");
static_assert(offsetof(EnemyData, hp) == 0x24, "EnemyData::hp");
static_assert(offsetof(EnemyData, ap) == 0x26, "EnemyData::ap");
static_assert(offsetof(EnemyData, stats) == 0x28, "EnemyData::stats");
static_assert(offsetof(EnemyData, ai_rows) == 0x38, "EnemyData::ai_rows");
static_assert(offsetof(EnemyData, resist) == 0x7C, "EnemyData::resist");
static_assert(offsetof(EnemyData, status) == 0x85, "EnemyData::status");
static_assert(offsetof(EnemyData, run_state) == 0x88, "EnemyData::run_state");
static_assert(sizeof(EnemyData) == 0x8C, "EnemyData stride");

static_assert(offsetof(BattleTask, flags) == 0x0, "BattleTask::flags");
static_assert(offsetof(BattleTask, state) == 0x1, "BattleTask::state");
static_assert(offsetof(BattleTask, param) == 0x5, "BattleTask::param");
static_assert(offsetof(BattleTask, kind) == 0x6, "BattleTask::kind");
static_assert(offsetof(BattleTask, mode) == 0x7, "BattleTask::mode");
static_assert(offsetof(BattleTask, timer) == 0x9, "BattleTask::timer");
static_assert(offsetof(BattleTask, digits) == 0xA, "BattleTask::digits");
static_assert(offsetof(BattleTask, vel_x) == 0xC, "BattleTask::vel_x");
static_assert(offsetof(BattleTask, vel_y) == 0x10, "BattleTask::vel_y");
static_assert(offsetof(BattleTask, start_x) == 0x18, "BattleTask::start_x");
static_assert(offsetof(BattleTask, start_y) == 0x1C, "BattleTask::start_y");
static_assert(offsetof(BattleTask, clut) == 0x27, "BattleTask::clut");
static_assert(offsetof(BattleTask, digit_phase) == 0x32, "BattleTask::digit_phase");
static_assert(offsetof(BattleTask, x) == 0x34, "BattleTask::x");
static_assert(offsetof(BattleTask, y) == 0x38, "BattleTask::y");
static_assert(offsetof(BattleTask, value) == 0x60, "BattleTask::value");
static_assert(offsetof(BattleTask, owner) == 0x80, "BattleTask::owner");
static_assert(sizeof(BattleTask) == 0x84, "BattleTask stride");

static_assert(offsetof(WindowRecord, in_use) == 0x0, "WindowRecord::in_use");
static_assert(offsetof(WindowRecord, handler) == 0x1, "WindowRecord::handler");
static_assert(offsetof(WindowRecord, kind) == 0x2, "WindowRecord::kind");
static_assert(offsetof(WindowRecord, state) == 0x3, "WindowRecord::state");
static_assert(offsetof(WindowRecord, centre_x) == 0x4, "WindowRecord::centre_x");
static_assert(offsetof(WindowRecord, centre_y) == 0x6, "WindowRecord::centre_y");
static_assert(offsetof(WindowRecord, half_width) == 0xA, "WindowRecord::half_width");
static_assert(offsetof(WindowRecord, half_height) == 0xB, "WindowRecord::half_height");
static_assert(offsetof(WindowRecord, flags) == 0xD, "WindowRecord::flags");
static_assert(offsetof(WindowRecord, pass) == 0xF, "WindowRecord::pass");
static_assert(offsetof(WindowRecord, width) == 0x10, "WindowRecord::width");
static_assert(offsetof(WindowRecord, height) == 0x12, "WindowRecord::height");
static_assert(offsetof(WindowRecord, width_step) == 0x14, "WindowRecord::width_step");
static_assert(offsetof(WindowRecord, height_step) == 0x16, "WindowRecord::height_step");
static_assert(offsetof(WindowRecord, settled_x) == 0x18, "WindowRecord::settled_x");
static_assert(offsetof(WindowRecord, settled_y) == 0x1A, "WindowRecord::settled_y");
static_assert(sizeof(WindowRecord) == 0x24, "WindowRecord stride");

static_assert(offsetof(SaveSummary, name) == 0x0, "SaveSummary::name");
static_assert(offsetof(SaveSummary, party) == 0x5, "SaveSummary::party");
static_assert(offsetof(SaveSummary, level) == 0x8, "SaveSummary::level");
static_assert(offsetof(SaveSummary, time) == 0xC, "SaveSummary::time");
static_assert(offsetof(SaveSummary, exp) == 0x10, "SaveSummary::exp");
static_assert(offsetof(SaveSummary, marked) == 0x14, "SaveSummary::marked");
static_assert(offsetof(SaveSummary, dir_index) == 0x15, "SaveSummary::dir_index");
static_assert(offsetof(SaveSummary, name_tail) == 0x16, "SaveSummary::name_tail");
static_assert(sizeof(SaveSummary) == 0x1C, "SaveSummary stride");

static_assert(offsetof(SaveDirEntry, name) == 0x0, "SaveDirEntry::name");
static_assert(offsetof(SaveDirEntry, size) == 0x14, "SaveDirEntry::size");
static_assert(sizeof(SaveDirEntry) == 0x18, "SaveDirEntry stride");

}  // namespace records

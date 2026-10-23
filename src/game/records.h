// The record families of the game's data image, typed (docs/typed-records.md;
// docs/typed-data-round.md groups SD and C1). A field is named where a cited
// reader names it; everything else is `pad_<offset>`, with the gap's reason
// beside it. Address fields are std::uint32_t: the image is 32-bit whatever
// the host. No packing pragma: every named field sits at its natural
// alignment, which the asserts check (the header holds under #pragma pack(1)
// as well).
//
// Since stage C1 the five families Sprite_Current can point at (the field
// sprite object, the party member, the effect object, the battle enemy, the
// battle task) derive from one SpriteHead, their first 0x80 bytes, so each
// head byte is named once (docs/typed-records.md section 6). Where the
// families give one head byte different meanings, the head holds an
// anonymous union of each family's name, the family in the comment: such a
// name is right only on its own family, and the compiler cannot see a wrong
// one. The views at the end (ScriptContext, ActionRecord, the window views,
// EffectWork) are per-meaning structs over bytes whose meaning changes with
// the handler, for rv::As where the meaning is known (record_views.h).
//
// Written from the field lists in docs/typed-records.md; edit this file directly.
#pragma once

#include <cstddef>
#include <cstdint>

#include "game/records_effect.h"
#include "game/records_gpu.h"

namespace records {

// The 0x80 bytes every record Sprite_Current can point at begins with: the
// field sprite objects, the party members (ObjTrio), the effect objects, the
// battle enemies and the battle tasks. Evidence that the layout is one:
// Battle_InitActorContexts 0x446FF0 copies an actor's first 0x80 bytes (a
// member's or an enemy's) into a battle task slot with rep movsd, and so do
// BossWeretigr 0x43D450 and the MAGIC017/018 images (the B1 magic audit;
// symbols.toml); the sprite engine's own functions (Sprite_*, the draw, the
// step) run on every family through Sprite_Current (sprite-draw-order.md 2,
// 5, 6, 14; sprite-pose.md 3; effect_1a.md 2.1; battle_fx_tasks.md 1). Bytes
// +0x02..+0x0B are each handler's own: the families' readers disagree there,
// and the unions below keep each family's name (docs/typed-records.md 6).
struct SpriteHead {
    std::uint8_t flags;  // +0x00 bit 0 in use / present / taken, 1 no image upload, 5 semi-transparent, 6 not drawn, 7 hidden / culled (sprite-draw-order 5, 14; sprite-pose 3); the party's bit 0 present in battle (battle_setup_callees.h kPresent), the enemy's 0x40 defeated (battle_flow.md); the effect pool's non-zero in use (Effect_FindFree / Effect_RunObjects, symbols.toml); the task slot's non-zero runs (battle_flow.cpp BattleTask_Create / RunAll)
    union {
        struct {
            std::uint8_t state;  // +0x01 the handler's state: a field object's Field_ObjectHandlers index (field-frame.md 2; movement-script.md F8), every effect kind dispatcher's jump index (effect_1a.md 2.1), the battle actor's first state byte (battle_setup_callees.h kState1), the task's state (battle_fx_tasks.md 1)
            std::uint8_t step;  // +0x02 the state's step: the effect kinds' sub-state tables by +2 (effect_*.md, e.g. EffectKind52_TrailSubStates), the actor's second state byte (kState2, battle_e2.cpp BattleEnemy_*); a field object's step byte to movement-script.md's F8 and event_ops.cpp's spawn (op[0xB]); zeroed with +3, +4 by BattleTask_FreeCurrent and by 0x589870 (symbols.toml)
            std::uint8_t state3;  // +0x03 the actor's third state byte (battle_setup_callees.h kState3); per effect kind a counter (effect_4e.md EffectKindA0 +3 a shade)
            std::uint8_t state4;  // +0x04 the actor's fourth state byte (kState4)
        };
        std::uint8_t states[4];  // +0x01 the four state bytes +1..+4 as an array (battle_setup_callees.h kState1..4, "the actor's state bytes"). The same bytes as state, step, state3, state4; spelled as the array where two branches change different state bytes and the original's code merges them through an index (typed-records.md 6: EnemyOp_CastStart, BattleObj_CastStart, Field_LeaderStand)
    };
    union {
        std::uint8_t actor;  // +0x05 party member, battle enemy: the actor index, a member's 0..2 (its palette slot in the field: area_entry_callees.h kRecPalette) and an enemy's slot + 3 (Battle_SetupEnemy, battle_sprites.cpp); battle_actions.cpp:353 reads the member's and the enemy's +5 as one actor
        std::uint8_t kind;  // +0x05 effect object, battle task: the handler's entry - Effect_KindHandlers[+5] (symbols.toml Effect_RunObjects; Effect_Spawn sets 6), and the entry +5 of the task runner's table (battle_fx_tasks.md 1: BattleFx_Dispatch's 19, BattleMagicFx_Dispatch's 110, Magic_Rows' row; BattleTask_Create's parameter)
    };
    union {
        std::uint8_t type;  // +0x06 field sprite object: 0x0A a party object; 8 and up never turn (sprite-draw-order 5; sprite-pose 3). On a party record +6 is whom it follows (member-sprites 2; event_leader.md), not named
        std::uint8_t param;  // +0x06 effect object: Effect_Spawn's first argument (symbols.toml Effect_Spawn); per kind a gap or a row (effect_1a.md 2.6, effect_3a.md 1.2)
        std::uint8_t runner;  // +0x06 battle task: which of BattleTask_RunAll's four runners, 0 BattleFx_Dispatch, 1 BattleMagicFx_Dispatch, 2 BattleMagicRow_Run (battle_fx_tasks.md 1 calls it the kind)
    };
    union {
        std::uint8_t flags2;  // +0x07 field sprite object: bit 3 not turned when talked to, bit 1 keeps its own facing rather than turning to the leader (field_frame.cpp:292-294; field-frame.md 2; shop_states.md ShopMode_End); bit 7 skips the blocked test (field_objects.cpp)
        std::uint8_t mode;  // +0x07 battle task: the damage popup's draw mode (battle_fx_tasks.md 1; battle_sprites.cpp:399-411, 558-559)
    };
    union {
        std::uint8_t direction;  // +0x08 field sprite object, party member: facing, & 7 indexes Field_DirectionSteps (field_e2.cpp, area_w3e.cpp; member-sprites 2); the battle banner's side byte of a member (battle_win_states.md 2)
        std::uint8_t layout;  // +0x08 battle enemy: the layout ^ 2 (Battle_SetupEnemy); 2 / 3 pick the next animation (battle_flow.md); the banner reads it as the side byte (battle_win_states.md 2), enemy_ai_ops.cpp as a mode
    };
    union {
        std::uint8_t step_frames;  // +0x09 field sprite object, party member: frames left in a step / a timed move; multiplies the step in Sprite_PointInReach (sprite-draw-order 6; movement-script 1b; member-sprites 2)
        std::uint8_t timer;  // +0x09 effect object, battle task: the kind's frame counter (effect_1a.md 2.3: EffectKind07_Start sets 0xF0, _FadeIn 0xFF); the popup's (battle_fx_tasks.cpp)
    };
    union {
        std::uint8_t digits;  // +0x0A battle task: the popup's digit count (battle_fx_tasks.cpp; battle_flow.cpp:483)
        std::uint8_t byte_0a;  // +0x0A every other family's: each handler's own - per effect kind a wait (effect_1a.md 2.6 kind 5's +0xA), the leader panel's hold after a full charge (area_w2e.cpp Area104_Kind5CFollow), effect 0's copy of a record's +0x10 (rest_1g.cpp LeaderPanel_EffectsStep); a field object's +0xA is unread. No one meaning: spelled raw (rv::Bytes(r)[0xa]), never by this name (C4 audit A F2; tools/typed_convert.py UNION_NAMES names +0xA for BattleTask only)
    };
    unsigned char pad_0B[0x1];  // +0xB: a field object's Effect_Objects index for ops 9x (symbols.toml Effect_Spawn); member-sprites 2's effect-object index; an effect's spawn argument (0x5898D0) or per kind a hold; compared with 1 by the popup's PopupStart (y 9 higher). One byte, four readings, none shared
    std::int32_t step_x;  // +0x0C the step, 16.16 a frame (sprite-draw-order 6; member-sprites 2); dword, Effect_Spawn's movsx byte (symbols.toml). Per effect kind and per magic task a word of its own (B1 magic audit N2; effect_1a.md 2.5): EffectWork's work_0c for those
    std::int32_t step_z;  // +0x10 the second step (sprite-draw-order 6); per kind and per task a word of its own, as +0xC
    std::int32_t step_y;  // +0x14 the third step, 16.16: Sprite_ClearSteps clears +0xC, +0x10, +0x14 (field_event.cpp); the field objects' rise added to the elevation (field_o.cpp MoveCmd_OpF9; area_w1b.cpp adds +0x14 * +9 to +0x3C; move_cmds.cpp; kind2_object.cpp); a magic task's scratch dword (B1 magic audit)
    std::int32_t work_18;  // +0x18 a handler's own dword: MoveCmd_OpF9's acceleration, added to +0xC each frame (field_o.cpp), and a handle from movement-script.md F8 and event_ops.cpp's spawn; a radius, a step or an angle offset in the magic tasks (B1 magic audit N3); kind 3's speed (effect_1a.md 2.5); kind 1's extra-record address (effect_1a.md 2.2). Was SpriteObject::accel_x, BattleTask::start_x
    std::int32_t work_1c;  // +0x1C likewise: MoveCmd_OpF9's second acceleration (field_o.cpp), F8's and the spawn's handle word, the magic tasks' scratch. Was SpriteObject::accel_z, BattleTask::start_y
    std::int32_t work_20;  // +0x20 likewise: cleared by Battle_SetupEnemy; field_c3.cpp's gravity added to +0x14; kind 5's speed (effect_1a.md 2.6); the magic tasks' scratch (92 rows)
    std::uint8_t draw_flags;  // +0x24 bit 0 frames uploaded, 1 member set-up pending, 2 CLUT row 0x1F0, 6 a 3D object, 7 overlay list (sprite-pose 3; sprite-draw-order 5, 14; movement-script 1c)
    std::uint8_t tex_slot;  // +0x25 texture slot (sprite-pose 3)
    std::uint8_t tex_row;  // +0x26 the slot's y row (sprite-pose 3)
    std::uint8_t clut;  // +0x27 palette (sprite-draw-order 14 Sprite_ClutWord; battle_fx_tasks.cpp DamagePopup)
    std::uint8_t clut_mode;  // +0x28 palettes to a row; non-zero is "wide" (sprite-draw-order 14)
    std::uint8_t ot_slot;  // +0x29 the draw slot Gfx_CommitPrim takes (sprite-draw-order 14; area_backdrop.cpp kTrioSlot; member-sprites 2); 4 for an enemy (Battle_SetupEnemy)
    std::uint8_t flip;  // +0x2A the draw's flip: non-zero ORs 0x400 into the mode word (sprite-draw-order 14), set from bit 7 by BattleEnemy_SetAnimation (battle_flow.md "the flip byte"); written as a pose byte's 0 / 1 by sprite-pose 3's readers. Was SpriteObject::pose
    std::uint8_t key_class;  // +0x2B row of Sprite_KeyAdjust (sprite_screen.cpp 0x588F20)
    std::uint16_t column;  // +0x2C frame-offset table column (sprite-pose 3; area_entry_callees.h kRecColumn); dispatched as "the form word" by rest_1b.cpp ByForm; on a task the copied owner's (battle_fx_tasks.md: indexed by the owner's +0x2C)
    std::int16_t screen_x;  // +0x2E the truncated screen point (movement-script 1c; sprite-draw-order 14; battle_actions.cpp SetMemberBlock / SetEnemyBlock; area_011.cpp); Effect_Spawn's word arguments (symbols.toml). Per effect kind a word of its own (EffectWork's work_2e)
    std::int16_t screen_y;  // +0x30 as +0x2E; EffectWork's work_30 per kind
    union {
        std::uint16_t draw_key;  // +0x32 layer << 8 | sort byte (sprite-draw-order 2; movement-script 1c; battle_actions.cpp SetMemberBlock / SetEnemyBlock)
        std::uint16_t digit_phase;  // +0x32 battle task, the damage popup: the digit phase (battle_fx_tasks.cpp PopupStart; battle_flow.cpp:477-485), started from the owner's draw key (battle_sprites.cpp:564)
    };
    union {
        std::int32_t x;  // +0x34 position, 16.16 (sprite_screen.cpp; area_backdrop.cpp kTrioPosition; Battle_SetupEnemy; battle_setup kPos; battle_fx_tasks.cpp); a dword read by AreaMap_Elevation (symbols.toml)
        struct {
            std::uint16_t x_frac;  // +0x34 x's fraction, read alone as a word (B1 width rows, 30 sites with +0x38)
            std::uint16_t x_whole;  // +0x36 x's whole part, the cell: read alone as a u16 (sprite_screen.cpp; member-sprites 2 "+0x36 the cell"; the spawn 0x5898D0 writes the halves apart, symbols.toml)
        };
    };
    union {
        std::int32_t z;  // +0x38 position, 16.16 (the same readers); a battle task's world z (B1 magic audit N1: all 331 uses; was BattleTask::y)
        struct {
            std::uint16_t z_frac;  // +0x38 z's fraction
            std::uint16_t z_whole;  // +0x3A z's whole part, the cell (member-sprites 2)
        };
    };
    union {
        std::int32_t height;  // +0x3C the pass's third sort key (sprite-draw-order 2); the third dword of the 12-byte point at +0x34 (battle_setup_callees.h kPos; effect_3a_callees.h kLeaderPoint)
        struct {
            std::uint16_t height_frac;  // +0x3C the height's fraction
            std::int16_t height_whole;  // +0x3E the height's whole part, s16: the elevation Sprite_ObjectMatrix 0x4FBD10 reads as s16 (sprite-draw-order 12; u16 until the records audit, 2026-10-23), the ground (member-sprites 2; Battle_SetupEnemy), the u16 the spawn 0x5898D0 writes (symbols.toml)
        };
    };
    std::int32_t scale_x;  // +0x40 16.16 (sprite-draw-order 12, 14)
    std::int32_t scale_y;  // +0x44 16.16 (sprite-draw-order 14)
    std::uint8_t scale_mode;  // +0x48 1: scale by depth; 0: 1.0 (sprite-draw-order 12, 14)
    std::uint8_t script_steps;  // +0x49 step count (sprite-draw-order 6 Sprite_ScriptStart)
    std::uint8_t script_hold;  // +0x4A ticks left on the step (sprite-draw-order 5, 6)
    std::uint8_t animation;  // +0x4B the current animation (sprite-pose 3); 0xFF on a member swap (area_entry.cpp:282) and at an enemy's set-up (Battle_SetupEnemy); saved to the member's +0x12F on a hit (battle_e3.cpp:380)
    std::uint32_t anim_data;  // +0x4C address: the animation data (sprite-pose 3)
    std::uint32_t script;  // +0x50 address: the byte script / a 3D object's quads (sprite-draw-order 5, 12)
    std::uint32_t frames;  // +0x54 address: the frame-word table (sprite-draw-order 5, 6)
    std::uint16_t script_pos;  // +0x58 (sprite-draw-order 5); saved to the member's +0x140 on a hit (battle_e3.cpp:381); an enemy's pose word (boss_sc_callees.h kEnemyPose)
    std::uint16_t frame_word;  // +0x5A (sprite-draw-order 5, 14)
    std::uint8_t draw_mode;  // +0x5C into the mode word (sprite-draw-order 14); written on a member's entry (area_entry.cpp; area-entry.md)
    std::uint8_t shade[3];  // +0x5D (sprite-draw-order 12, 14; effect_1a.md 2.3: EffectKind07 zeroes them and fades them up)
    union {
        std::int32_t depth;  // +0x60 the projection's depth (movement-script 1c; sprite-draw-order 14; effect_1a.md 2.2)
        std::int32_t value;  // +0x60 battle task, the damage popup: the number shown (battle_fx_tasks.cpp; read as a word by the draw)
    };
    std::uint32_t angle[3];  // +0x64 the rotation angles in the low words (sprite-draw-order 12); +0x64 / +0x68 also the member / its row during a member set-up (sprite-pose 3); MoveCmd_OpF9 runs a party object's tilt counts into them (field_o.cpp); effect kind 0x13 (the camera turn): the three target Camera_Angles, dwords (CameraTurn_Start, frame-callees.md; area_w3f.cpp Kind13)
    std::uint32_t reach;  // +0x70 dword (sprite-pose 3: its low byte ORed from the bank's first byte); the low byte the object's reach: Sprite_PointInReach's margin (sprite-draw-order 6), Party_MemberAt's (area_w1c.md Area51_MemberAtObject), blocked_at's and AreaMap_CellsNone's size (member_sprites.cpp; field_c3.cpp), the jump's scale (field_c3.md); member-sprites 2 calls a member's non-zero byte "raised" (Field_CellAhead splits on it). Was PartyMember::raised
    std::uint32_t screen_xf;  // +0x74 the float screen point's bits (movement-script 1c; sprite_screen.cpp; effect_4d.md EffectKind98_DrawBurst)
    std::uint32_t screen_yf;  // +0x78
    std::uint32_t depth_f;  // +0x7C the float projected depth's bits (rest_4f.cpp:1215 F(s + 0x7C); effect_4d.md EffectKind98_DrawBurst)
};

// The field sprite object: Sprite_Objects 0x7DEE80 (30) and Sprite_ObjectsExtra
// 0x802000 (4), numbered 0..33 (docs/sprite-draw-order.md section 5). Every
// Sprite_* function reads it through Sprite_Current. +0x80.. is the field
// object's script context (docs/movement-script.md section 1b; the
// ScriptContext view below).
// Not this family: the character records 0x903A70 + id * 0xA4 share the
// stride (kCharStride / kRecordStride) but not the layout (CharRecord below).
//
// SpriteObjectBody is its first 0xA0 bytes, all but the area-object handler
// byte and the three after it: the extent of Sprite_Kind2 0x7E0940, the kind-2
// object, whose +0xA0..+0xA3 are DrawItemPool_Free 0x7E09E0 (C4 audit A F5;
// symbols.toml Sprite_Kind2), so Sprite_Kind2.handler cannot be spelled.
struct SpriteObjectBody : SpriteHead {
    std::uint8_t context;  // +0x80 the script context's first byte: flags (movement-script 1b; field_objects.cpp)
    std::uint8_t pose_wait;  // +0x81 (field_objects.cpp)
    std::uint8_t loop;  // +0x82 the context's loop counter (movement-script 1b)
    std::uint8_t area_script;  // +0x83 which of the area's scripts (movement-script 1b)
    std::uint8_t speed;  // +0x84 index into Field_MoveSpeeds (movement-script 1b)
    std::uint8_t saved_facing;  // +0x85 the facing kept while talked to, | 1 for a state-0x0A object (field_frame.cpp:293,300); put back to +8 (mode_states.cpp:250; shop_states.md ShopMode_End; field-frame.md 2)
    std::uint8_t trigger;  // +0x86 the object-trigger id: Field_ObjectTriggers by object[+0x86] (area-rows.md)
    std::uint8_t move_steps;  // +0x87 steps left in a timed move (movement-script 1b)
    std::uint16_t trigger_word;  // +0x88 written op[0xD] << 8 | op[0xE] by the spawn (event_ops.cpp:201); 0xFFFF none (event_ops.cpp:986)
    std::uint16_t script_position;  // +0x8A the context's script position (movement-script 1b)
    std::int32_t target_x;  // +0x8C MoveCmd_OpF9's target on Field_ActiveMember, 16.16 (field_o.cpp); the placement op's home (wide_staging.cpp:572); zeroed by event_script.cpp:494
    std::int32_t target_z;  // +0x90 (the same readers)
    std::uint32_t best_score;  // +0x94 Field_ObjectBestDirection's running score, started 0 or 0xFFFFFF (field_c3.md); zeroed by field_o.cpp:1229
    std::uint16_t half_w;  // +0x98 the object's half-widths, |d| >> 16 against them (field_c3.md Field_ObjectApproachDirection); op[3] (event_ops.cpp:199; event-ops.md)
    std::uint16_t half_d;  // +0x9A op[4] (event_ops.cpp:200)
    std::uint16_t counter;  // +0x9C counted up for every object in reach of the point ahead (event-ops.md); a u16 zeroed by field_objects.cpp
    std::uint8_t tag;  // +0x9E the boss set-ups' tag (boss_spawn.h; field_o.cpp:1157 op[0x10]; psp-battle-changes.md)
    unsigned char pad_9F[0x1];  // +0x9F: area_w0c.md Area32_TintStart puts a slot in the active member's +0x9F; one reader, meaning not settled
};

struct SpriteObject : SpriteObjectBody {
    std::uint8_t handler;  // +0xA0 & 0x7F the area object handler, 0x7F none (rest_2a.md Area_ObjectHandler); 0x7F at a spawn (event_ops.cpp:204,259; field_o.cpp:1158)
    unsigned char pad_A1[0x3];  // +0xA1..+0xA3: +0xA1 written by field_objects.cpp, not named
};

// A party member: ObjTrio 0x802D40, three records (record 0 the leader,
// Field_State). +0x00..+0x7F the member's sprite (SpriteHead);
// +0x80..+0x123 a copy of the persistent character record 0x903A70 + id *
// 0xA4 (CharRecord below; docs/area-entry.md Party_SwapMembers;
// docs/field-event.md); +0x124.. the member's own tail, which the field (as
// Field_State) and battle read differently: the battle names are used here,
// the field use is in each comment, and the two views of the tail are
// ActionRecord (battle) and MemberFieldTail (field) below.
struct PartyMember : SpriteHead {
    unsigned char pad_80[0x9];  // +0x80..+0x88: the character record's name (+0..+8, CharRecord::name)
    std::uint8_t char_id;  // +0x89 record +0x09 (battle_setup_callees.h kPCharId; area_entry_callees.h kRecMember)
    std::uint8_t level;  // +0x8A record +0x0A (battle_damage_callees.h kPLevel, working +0x7A)
    unsigned char pad_8B[0x5];  // +0x8B..+0x8F: character record +0x0B..+0x0F; +0x8B bit 0 "joined" per char-stats.md, no member reader addresses it; +0x8C the experience (CharRecord::exp)
    std::uint16_t status;  // +0x90 record +0x10 (battle_setup kPStatus; battle_damage kPStatus)
    std::uint8_t weapon;  // +0x92 record +0x12 (battle_setup kPWeapon)
    std::uint8_t armour[3];  // +0x93 record +0x13..+0x15, CharRecord::armour (char-stats.md 2 Char_ApplyArmour); +0x95 tested for 0x43 by BattleParty_AllDown and cleared before BattleParty_RecalcStats (battle_e3.cpp:780, 845; battle_setup kPArmour3)
    std::uint8_t accessory[2];  // +0x96 record +0x16 / +0x17 (battle_setup kPAccessory)
    std::uint16_t hp;  // +0x98 record +0x18 (battle_setup kPHp; battle_damage kPHp)
    std::uint16_t ap;  // +0x9A record +0x1A
    std::uint8_t survive;  // +0x9C record +0x1C, set from +0xAE (battle_setup kPByte9C); the overkill a kill leaves the flag below (battle_damage kPSurvive)
    unsigned char pad_9D[0x1];  // +0x9D: record +0x1D (CharRecord::halving); no member reader names it
    std::uint8_t hp_scale;  // +0x9E record +0x1E, CharRecord::hp_scale: BattleForm_ApplyStats multiplies the form's HP word by it (battle_e5.cpp:403; char-stats.md 2)
    unsigned char pad_9F[0x1];  // +0x9F: record +0x1F (CharRecord::master); no member reader names it
    std::uint16_t max_hp;  // +0xA0 record +0x20 (battle_setup kPMaxHp)
    std::uint16_t max_ap;  // +0xA2 record +0x22
    std::uint16_t atk;  // +0xA4 record +0x24 (char-stats.md 2; rest_3b.cpp ActorWord(actor, 0xA4, 0xB4); battle_e5.cpp:408; the stat copy's +4, battle_damage kAttackerAtk)
    std::uint16_t def;  // +0xA6 record +0x26 (battle_damage kPDef)
    std::uint16_t agi;  // +0xA8 record +0x28 (battle_setup kPAgi; battle_damage kPAgi)
    std::uint16_t intel;  // +0xAA record +0x2A (char-stats.md 2; the stat copy's +0xA, battle_damage kAttackerInt / kTargetInt)
    unsigned char pad_AC[0x2];  // +0xAC, +0xAD: record +0x2C, +0x2D; not named
    std::uint8_t byte_ae;  // +0xAE record +0x2E, copied to +0x9C (battle_setup kPByteAE)
    std::uint8_t elements[5];  // +0xAF affinity classes, fire .. wind (battle_damage kPElements)
    std::uint8_t holy;  // +0xB4 holy affinity class (battle_damage kPHoly)
    unsigned char pad_B5[0x5];  // +0xB5..+0xB9: the rest of the character record's effective block: +0xB5..+0xB7 the rest of the nine resistance bytes, +0xB8 / +0xB9 the first two of the five percentage bytes +0xB8..+0xBC (char-stats.md 2); not named here
    std::uint8_t crit_chance;  // +0xBA record +0x3A, a percentage: Rand() % 100 below it gives the swing cue 3 and sets 0x904AA8 bit 7 (BattleObj_SwingCue, BattleObj_SpecialCue: battle_obj_states.cpp:290, battle_e3.cpp:1075), the bit Battle_CalcDamage doubles the damage by (battle_damage.cpp:546); raised by 0x1E and capped at 100 with +0xBB (battle_e6.cpp:877)
    unsigned char pad_BB[0x5];  // +0xBB..+0xBF: the last three percentage bytes (+0xBB raised with +0xBA, battle_e6.cpp:876) and two more of the block; not named here
    std::uint16_t base_max_hp;  // +0xC0 record +0x40, the base copy of +0xA0 (char-stats.md 2; item-use.md 2; battle_e5.cpp:405-425)
    std::uint16_t base_max_ap;  // +0xC2 record +0x42
    std::uint16_t base_atk;  // +0xC4 record +0x44
    std::uint16_t base_def;  // +0xC6 record +0x46
    std::uint16_t base_agi;  // +0xC8 record +0x48
    std::uint16_t base_intel;  // +0xCA record +0x4A
    unsigned char pad_CC[0x32];  // +0xCC..+0xFD: the rest of the base block (record +0x4C..+0x5F) and record +0x60..+0x7D (CharRecord); not named here
    std::uint8_t learned[10];  // +0xFE record +0x7E..+0x87, a ten-entry list: Battle_MemberListFull 0x453AC0 answers 1 when no entry is 0, and Battle_MemberRollByAction 0x453910 refuses the roll on an enemy's action then (battle_e6.cpp:902, 927; PSX 0x800A77E4)
    unsigned char pad_108[0x1C];  // +0x108..+0x123: record +0x88..+0xA3 (CharRecord); not named here
    std::uint8_t command;  // +0x124 battle: the command auto-battle gives (docs/battle_e4.md); field: the script context's first byte (area_entry_callees.h kRecContext). ActionRecord::command / MemberFieldTail::context
    std::uint8_t command_kind;  // +0x125 ActionRecord::command_kind (was queued_kind; C3 naming): 5 an item, 4 an ability (battle_setup kPQueuedKind; battle_damage kPCommand)
    std::uint16_t queued_id;  // +0x126 category << 8 | id, or the ability id (battle_setup kPQueuedItem; battle_damage kPCommandId)
    std::int16_t hp_change;  // +0x128 battle: the result record's +4, read signed (battle_setup kPHpChange; battle_e3.cpp:486,499,898 SWord); field: its low byte the walking speed (member-sprites 2)
    std::int16_t ap_change;  // +0x12A battle: the result record's +6, read signed (battle_setup kPApChange; battle_e3.cpp:499)
    std::uint8_t shown;  // +0x12C status-shown bits, 0x11 / 2 with an effect (battle_setup kPEffect; battle_damage kPShown)
    std::uint8_t counter;  // +0x12D the 0x40 / 0x20 statuses' counter (battle_setup kPCounter); battle_damage kPShown2 "cleared with a cured status"
    std::uint8_t item_slot;  // +0x12E (battle_setup kPItemSlot)
    std::uint8_t saved_animation;  // +0x12F +0x4B saved by BattleObj_HitEnter (battle_e3.cpp:380)
    std::uint32_t flags_130;  // +0x130 battle: flag dword (battle_damage kPFlags; battle_sprites.cpp "the party's flags dword"); field: the script pointer (area_entry_callees.h kRecScript)
    std::uint32_t flags_134;  // +0x134 battle: flag dword (battle_setup kPFlags; battle_damage kPFlags2); field: byte +0x136 the standing counter, +0x137 the action (member-sprites 2) - one dword in battle, two bytes in the field, so the bytes are not separate members (MemberFieldTail has them)
    std::uint8_t field_flags;  // +0x138 field: bit 0, bit 3 CLUT pending (area_backdrop.cpp kTrioFlags; member-sprites 2)
    std::uint8_t field_object;  // +0x139 field: a Sprite_Objects index (field_e1.cpp:478); battle zeroes +0x138..+0x13F (battle_e3.cpp:691; battle_e6.cpp:217-218; battle_phases.cpp:222-223)
    unsigned char pad_13A[0x6];  // +0x13A..+0x13F: zeroed with +0x138 in battle; not named
    std::uint16_t saved_script_pos;  // +0x140 the sprite's +0x58 saved by BattleObj_HitEnter (battle_e3.cpp:381)
    std::uint8_t count_4000;  // +0x142 (battle_setup kPCount4000)
    std::uint8_t count_800;  // +0x143 (battle_setup kPCount800)
    std::uint8_t count_40;  // +0x144 zeroed with flag 0x40 (battle_setup kPCount40); battle_damage kPCharge, the attack charge
    std::uint8_t count_80;  // +0x145 zeroed with flag 0x80 (battle_setup kPCount80); battle_damage kPCharge2, the ability charge
    unsigned char pad_146[0x2];  // +0x146, +0x147: unread
    std::uint8_t char_index;  // +0x148 the character record's index, in the field and in battle (area_entry_callees.h kRecCharacterIndex; effect_3a_callees.h kMemberFlag; battle_e3.cpp:684,723; battle_e4.cpp:126; battle_e6.cpp:318). battle_damage_callees.h kPCharIndex (0x138 from 0x802D50; kPWeaponKind until stage B1) is this byte: Battle_CalcDamage's rule is "character index 5" (docs/typed-records.md 3)
    std::uint8_t tint;  // +0x149 field: the running script's tint record, MoveScript_TintRecords by it (area_w1a.cpp:134)
    unsigned char pad_14A[0x2];  // +0x14A, +0x14B: unread
};

// An effect object: Effect_Objects 0x7E11E0, 20 records (symbols.toml).
// Effect_RunObjects makes each in-use record Sprite_Current and calls
// Effect_KindHandlers[+5], so it is a sprite head and nothing more: its
// names are SpriteHead's (the effect's own at +5, +6, +9 in the head's
// unions). The kinds reuse +0xC, +0x10, +0x2E, +0x30 as words of their own:
// the EffectWork view below.
struct EffectObject : SpriteHead {};

// A battle enemy: 0x93B960 + (actor - 3) * 0x128, eight records. +0x00..+0x7F
// its sprite (SpriteHead), +0x80.. EnemyWorkingRecords 0x93B9E0
// (battle_sprites.cpp, docs/battle_sprites.md 3), filled by
// Battle_CopyEnemyData from an EnemyData record. battle_damage_callees.h
// measures from +0x80. Its +0x8C `kind` hides the head's +5 `kind` (an
// effect's and a task's): on an enemy, ->kind is +0x8C, as before C1.
struct BattleEnemy : SpriteHead {
    char name[0xc];  // +0x80 from EnemyData +0 (battle_sprites.md 3)
    std::uint8_t enemy_kind;  // +0x8C EnemyData::kind (+0xC); grouped by Battle_OpenEnemyNames (battle_sprites.cpp). Was kind, which hid the head's kind at +5 (C3 naming)
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
    std::uint16_t drop0_item;  // +0xA8 drop 0's item, EnemyData +0x30 (Battle_CopyEnemyData): Battle_RollDrops 0x437580 rolls it, SkillSteal_Roll 0x4F5140 steals it, Identify_DrawEnemy hands it to Identify_DrawItem (magic_s12.cpp:448). Was item_a8 (C3 naming)
    std::uint8_t drop0_row;  // +0xAA drop 0's chance row: Battle_RollDrops' {0, 0, 1, 3, 7, 0x1F, 0x7F, 0xFF}[+0xAA] (battle_flow.cpp; 0x4375CA esi 0xA8 + 2), SkillSteal_RateTable[+0xAA] for the steal (magic_steal.cpp; magic_steal.md); set by AI op 5 (battle_e5.cpp:216). Was steal_row (records audit N2)
    unsigned char pad_AB[0x1];  // +0xAB: the rest of the EnemyData +0x30 dword; its marker bit raises +0xAA once (rest_3c.md Effect_EnemyRaiseAAAE); not named
    std::uint16_t drop1_item;  // +0xAC drop 1's item, EnemyData +0x34: Battle_RollDrops' second, Identify_DrawEnemy's second item word (magic_s12.cpp:450). Was item_ac (C3 naming)
    std::uint8_t drop1_row;  // +0xAE drop 1's chance row: Battle_RollDrops' class byte for +0xAC (0x4375CA, the second pass esi 0xAC + 2); set by AI op 5 (battle_e5.cpp:217); raised with +0xAA once by the marker bit of +0xAF (rest_3c.md Effect_EnemyRaiseAAAE) (records audit N2)
    unsigned char pad_AF[0x1];  // +0xAF: the rest of the EnemyData +0x34 dword; its marker bit raises +0xAE once (rest_3c.md Effect_EnemyRaiseAAAE); not named
    std::uint16_t max_hp;  // +0xB0 (battle_setup kEMaxHp)
    std::uint16_t max_ap;  // +0xB2 (battle_damage kEMaxAp)
    std::uint16_t atk;  // +0xB4 the stat copy's +4 (battle_damage kAttackerAtk; rest_3b.cpp ActorWord(actor, 0xA4, 0xB4))
    std::uint16_t def;  // +0xB6 the stat copy's +6 (battle_damage kTargetDef; rest_3b.cpp:464; rest_3b.md)
    std::uint16_t agi;  // +0xB8 (battle_setup kEAgi)
    std::uint16_t intel;  // +0xBA the stat copy's +0xA (battle_damage kAttackerInt / kTargetInt); BattleEnemy_PickAction tests it <= 1 (battle_e2.md), Battle_AutoTargetCheck > 1 (rest_4a kEnemyIntel, named kEnemyHp until 2026-10-20: IDEAS I49). magic_steal.md R22 calls +0xBA the speed, but SkillSteal_Roll reads +0xB8 (magic_steal.cpp): a doc conflict
    unsigned char pad_BC[0x3];  // +0xBC..+0xBE: the stat copy's +0xC..+0xE; not named
    std::uint8_t elements[5];  // +0xBF (battle_damage kEElements)
    std::uint8_t holy;  // +0xC4 (battle_damage kEHoly)
    unsigned char pad_C5[0xB];  // +0xC5..+0xCF: the rest of the stat block copy (+0xC5..+0xC7 the last resistance bytes, Battle_CopyEnemyData; +0xC8.. the percentage bytes as the character record's +0x38..); not named
    std::uint16_t base_max_hp;  // +0xD0 the stat block's base copy, +0x20 after the working one as the character record's +0x40 after +0x20 (char-stats.md 2): EnemyAI_ScaleStat caps HP +0xA4 at it (battle_e5.md)
    std::uint16_t base_max_ap;  // +0xD2 EnemyAI_ScaleStat caps AP +0xA6 at it (battle_e5.md)
    std::uint16_t base_atk;  // +0xD4 scaled and capped at 999 by EnemyAI_ScaleStat (battle_e5.md); the base copy of +0xB4
    std::uint16_t base_def;  // +0xD6 likewise, of +0xB6
    std::uint16_t base_agi;  // +0xD8 likewise, of +0xB8
    std::uint16_t base_intel;  // +0xDA likewise, of +0xBA
    unsigned char pad_DC[0x14];  // +0xDC..+0xEF: the rest of the base copy (+0xDF.. the resistance bytes, Battle_CopyEnemyData); not named
    std::uint8_t data_id;  // +0xF0 the id's low byte: indexes EnemyData (battle_e3_callees.h kEnemyData8B; battle_e2.md) and the AI scripts (battle_damage kEScript; Battle_CopyEnemyData)
    std::uint8_t ai_rows_done;  // +0xF1 a bit per AI row fired; DIV-0082 (battle_damage kERowsDone)
    std::int8_t screen_dx;  // +0xF2 added to the screen point by the popup and the action block (battle_fx_tasks.cpp BattleFx_PopupStart; battle_actions.cpp SetEnemyBlock); from EnemyData +0x78..+0x7B (Battle_SetEnemyOffset)
    std::int8_t screen_dy;  // +0xF3
    std::uint32_t event_hook;  // +0xF4 address, called with 2 in an event battle (battle_flow.md)
    std::uint32_t cue_table;  // +0xF8 address of the sound cues (battle_items.cpp:724 Long(r + 0xF8); enemy_ai_ops.md; battle_e2.md; boss_sb.md)
    std::uint32_t anim_table;  // +0xFC address of the animation bytes (battle_flow.cpp BattleEnemy_SetAnimation)
    std::uint8_t run_state;  // +0x100 indexes the state table 0x64B084 (battle_flow.md); EnemyData +0x88
    std::uint8_t hit_class;  // +0x101 EnemyData +0x89; the hit-sound class (battle_sprites.cpp:534 Battle_PlayHitSound)
    unsigned char pad_102[0x2];  // +0x102, +0x103: unread
    std::uint8_t command;  // +0x104 ActionRecord::command, the action record's first byte (was result_head; C3 naming, the party's +0x124 is command): 0x904B60 points here (battle_actions.cpp SetEnemyBlock; battle_actions.md; enemy_ai_ops.md); the party's is +0x124
    std::uint8_t command_kind;  // +0x105 ActionRecord::command_kind: 5 an item, 4 an ability (battle_damage kECommand). Was command (C3 naming: the party's +0x125 and ActionRecord +1 say command_kind)
    std::uint16_t ability;  // +0x106 the ability id, one of +0x9C by Rand() & 7 (battle_e2.cpp:828,866; battle_e2.md BattleEnemy_PickAction)
    std::int16_t hp_change;  // +0x108 the result record's +4, signed (battle_setup kEHpChange; battle_e2.cpp:1133; boss_sd.cpp:404; boss_sa.cpp:394; battle_e2.md "the s16 +0x108")
    std::int16_t ap_change;  // +0x10A the result record's +6, signed like +0x108 (battle_setup kEApChange)
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
    std::uint16_t drop0_item;  // +0x30 to BattleEnemy::drop0_item (+0xA8), copied with +0x32 / +0x33 as one dword (Battle_CopyEnemyData)
    std::uint8_t drop0_row;  // +0x32 to BattleEnemy::drop0_row (+0xAA)
    unsigned char pad_33[0x1];  // +0x33: to BattleEnemy +0xAB with the dword; not named
    std::uint16_t drop1_item;  // +0x34 to BattleEnemy::drop1_item (+0xAC), likewise
    std::uint8_t drop1_row;  // +0x36 to BattleEnemy::drop1_row (+0xAE)
    unsigned char pad_37[0x1];  // +0x37: to BattleEnemy +0xAF with the dword; not named
    std::uint8_t ai_rows[0x40];  // +0x38 four rows of 16 bytes (enemy_ai_data.md 1; enemy_ai.py)
    std::int8_t place[4];  // +0x78 x / z offsets by side, copied or negated to BattleEnemy +0xF2 / +0xF3 (Battle_SetEnemyOffset, battle_sprites.cpp:997-1007)
    std::uint8_t resist[9];  // +0x7C to BattleEnemy +0xDF.. and +0xBF.. (Battle_CopyEnemyData)
    std::uint8_t status;  // +0x85 to BattleEnemy +0x92 (Battle_CopyEnemyData)
    std::uint8_t size;  // +0x86 the on-screen test's size (field_hidden.cpp:525; field_hidden_callees.h kKinds)
    std::uint8_t margin;  // +0x87 (field_hidden.cpp:525; field_hidden_callees.h kKinds); battle_e2.md BattleFx_PlaceOverOwner uses it as the y
    std::uint8_t run_state;  // +0x88 to BattleEnemy +0x100 (Battle_CopyEnemyData)
    std::uint8_t hit_class;  // +0x89 to BattleEnemy +0x101, the hit-sound class (battle_sprites.cpp:534)
    std::uint8_t fx_size;  // +0x8A (battle_items_callees.h kEnemyFxSize = 0x8C55C8 + 0x8A; boss_sj_callees.h; boss_si_callees.h names the same byte kEnemyDataCount)
    std::uint8_t cast_fx;  // +0x8B (battle_e3_callees.h kEnemyData8B, by the enemy's +0xF0; battle_e3.md EnemyOp_CastStart)
};


// A battle task: 0x93A000, 48 slots (battle_flow_callees.h kTasks), and the
// 0x84-byte overlay pools the magic code runs the same way (B1 magic audit).
// Not the scheduler's 0x20-byte Task_Records (docs/task_sched.md).
// BattleTask_RunAll makes the slot Sprite_Current, and the makers copy an
// actor's first 0x80 bytes into it (Battle_InitActorContexts 0x446FF0), so
// +0x00..+0x7F are a sprite head: the popup's own bytes are the head's
// task names (kind +5, runner +6, mode +7, timer +9, digits +0xA,
// digit_phase +0x32, value +0x60), and the magic tasks use +0xC..+0x23 as
// scratch (step_x, step_z, step_y, work_18, work_1c, work_20).
struct BattleTask : SpriteHead {
    std::uint32_t owner;  // +0x80 address of the owning actor (battle_flow_callees.h kTaskOwner); BattleTask_RunAll copies it to 0x93B940 (B1 magic audit)
};

// A window record: WindowRecords 0x803160, 22 records (docs/window-task.md
// 3). Bytes +0..+7 and +0xF mean the same to every handler; +8..+0x23 are
// each handler's own. The names below past +7 are the field windows'
// (window-task.md 3, Field_RunTaskRecords' handlers); BattleWindow and
// ItemListWindow are the battle handler's and the item list's views of the
// same record.
struct WindowRecord {
    std::uint8_t in_use;  // +0x00 Window_Alloc sets 1, Window_FreeCurrent clears it (window-task.md 3)
    std::uint8_t handler;  // +0x01 index into Field_RunTaskRecords' nine
    std::uint8_t kind;  // +0x02 the window kind, per handler: 0..2 for the field windows (window-task.md 3, 4), 0..7 for the battle windows (battle_win_states.md 2; written 7 battle_phases.cpp:286, 6 battle_setup.cpp:121, 4 battle_sprites.cpp:970 and battle_result.cpp:148, 5 battle_menu_states.cpp:348), and by the menus 5 (rest_2d.cpp:810) and 0xB (rest_2f.cpp:843); the fuzz seeds 0..7 (battle_win_states_fuzz.cpp)
    std::uint8_t state;  // +0x03 0..4
    std::uint16_t x;  // +0x04 the box's left edge: Window_Grow moves it back half the width step while the width grows, which keeps the box centred only on a left edge (window_task.cpp Window_Grow, HalfStep); 12.4 in the field windows (kind 1 writes << 4, rest_2f.cpp), whole pixels in the menus (Menu_DrawBox at +4 + 3, text at +4 + 7, menu_windows.cpp; MenuSlide_RightTo80 clamps it at 0x50, rest_2f.cpp) and the battle windows (battle_win_states.md 2; menu-windows.md 2 "x, y (s16)"). Was centre_x: window-task.md 3 said "centre" until the C3 naming pass (B1 audit rbs section 4)
    std::uint16_t y;  // +0x06 the top edge, as +4. Was centre_y
    unsigned char pad_08[0x2];  // +8, +9: not in window-task.md 3; the battle and list handlers' own (BattleWindow, ItemListWindow)
    std::uint8_t param_a;  // +0x0A per handler: the half-width in pixels for the field windows (window-task.md 3); the actor slot for battle handler 3 (battle_win_states.md 2; battle_sprites.md 3); the item category for the list windows, the greyed / price percent in the buy list (menu-windows.md 2). The per-handler views are BattleWindow and ItemListWindow
    std::uint8_t half_height;  // +0x0B
    unsigned char pad_0C[0x1];  // +0xC: not in window-task.md 3
    std::uint8_t flags;  // +0x0D bit 0 no frame, bit 1 the narrow box
    unsigned char pad_0E[0x1];  // +0xE: not in window-task.md 3
    std::uint8_t pass;  // +0x0F 0..2
    std::uint16_t width;  // +0x10 12.4
    std::uint16_t height;  // +0x12 12.4
    std::uint16_t width_step;  // +0x14
    std::uint16_t height_step;  // +0x16
    std::uint16_t settled_x;  // +0x18 kind 1's settled place (Window_Grow snaps +4 to it)
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
    std::uint8_t style;  // +0x09 the window style byte 0x903A5A, written by Save_BuildBlock (rest_2c.cpp:397)
    std::uint8_t style2;  // +0x0A 0x903A5B (rest_2c.cpp:398)
    unsigned char pad_0B[0x1];  // +0xB: unwritten by Save_BuildBlock, unread
    std::uint8_t hours;  // +0x0C the play clock 0x9040C8..CB copied whole (rest_2c.cpp:396); drawn first (field_o.cpp Menu_DrawSaveSlot)
    std::uint8_t minutes;  // +0x0D drawn second (Menu_DrawSaveSlot)
    std::uint8_t seconds;  // +0x0E (map_field_objects_callees.h: hours, minutes, seconds, frames)
    std::uint8_t frames;  // +0x0F 30 a second
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

// ---------------------------------------------------------------------------
// Views: one record's bytes as one handler reads them, for rv::As where the
// meaning is known (typed-data-stage-c.md section 3 item 3). Each is a cast
// target and nothing else; the record keeps its family struct above.

// The movement-script context: SpriteObject +0x80 for a field object, and
// PartyMember +0x124 for a party member in the field (area_entry.cpp hands
// record + kRecContext 0x124 to ScriptContext_Reset; area_entry_callees.h).
// MoveScript_Step and its group handlers take this pointer as `object`
// (move_script.cpp; movement-script.md 1b). Only the bytes the context's own
// readers settle: ScriptContext_Reset 0x5322B0 (field_event.cpp) writes +0,
// +1, +2, +3, +5, +7, +0xA and speed 3 at +4. Past +0xB the sprite (a
// target point, SpriteObject +0x8C) and the member (the script pointer,
// MemberFieldTail +0xC) differ.
struct ScriptContext {
    std::uint8_t flags;  // +0x00 bits 0x40 / 0x80 set and cleared by the step's ops (move_script.cpp 188-299); SpriteObject::context
    std::uint8_t pose_wait;  // +0x01 op 0x5x stores op & 0xF (MoveScript_Step); SpriteObject::pose_wait
    std::uint8_t loop;  // +0x02 the loop counter of op 02 (move_script.cpp:123-130; movement-script.md 1b)
    std::uint8_t area_script;  // +0x03 which of the area's scripts (movement-script.md 1b)
    std::uint8_t speed;  // +0x04 index into Field_MoveSpeeds; 3 after a reset (ScriptContext_Reset; movement-script.md 1b); the member's walking speed (member-sprites 2: PartyMember +0x128's low byte)
    std::uint8_t saved_facing;  // +0x05 cleared by ScriptContext_Reset; the facing kept while talked to (SpriteObject +0x85, field_frame.cpp:293,300)
    unsigned char pad_06[0x1];  // +6: a field object's trigger id (SpriteObject +0x86); not the context's reset
    std::uint8_t move_steps;  // +0x07 steps left in a timed move: op 1x-4x stores op & 7 (MoveScript_Step; movement-script.md 1b)
    unsigned char pad_08[0x2];  // +8, +9: a field object's trigger word (SpriteObject +0x88); not the context's
    std::uint16_t position;  // +0x0A the script position: move_script_bytes.h Pos / SetPos, every store 16 bits
};

// A party member's tail +0x124..+0x13F as the field reads it (Field_State,
// ObjTrio in the field): the script context, then the member's own bytes.
// Cast at the record's +0x124. The battle reads the same bytes as an
// ActionRecord.
struct MemberFieldTail {
    ScriptContext context;  // +0x00 (+0x124) area_entry_callees.h kRecContext; ScriptContext_Reset
    std::uint32_t script;  // +0x0C (+0x130) the script pointer: area_entry_callees.h kRecScript, set from the descriptor's +0x18 list (area_entry.cpp:210); the interpreter's third caller's "pointer at struct +0x130" (movement-script.md 3)
    unsigned char pad_10[0x2];  // +0x10, +0x11 (+0x134, +0x135): no field reader named
    std::uint8_t standing;  // +0x12 (+0x136) the standing counter (member-sprites 2)
    std::uint8_t action;  // +0x13 (+0x137) the action (member-sprites 2)
    std::uint8_t field_flags;  // +0x14 (+0x138) bit 0, bit 3 CLUT pending (area_backdrop.cpp kTrioFlags; member-sprites 2); PartyMember::field_flags
    std::uint8_t field_object;  // +0x15 (+0x139) a Sprite_Objects index (field_e1.cpp:478); PartyMember::field_object
    unsigned char pad_16[0x6];  // +0x16..+0x1B (+0x13A..+0x13F): no field reader named
};

// The battle's action / result record: a member's +0x124, an enemy's +0x104
// (battle_actions.md: the action blocks' +0xC points here; 0x904B60 is
// pointed at it by EnemyOp_ReceiveAction, enemy_ai_ops.md, and by the
// member's handlers). The same 0x1C bytes on both sides, by the pairs of
// constants that name them: battle_setup_callees.h kPHpChange / kEHpChange
// (+4), kPApChange / kEApChange (+6), kPEffect / kEEffect (+8), kPCounter /
// kECounter (+9), kPFlags / kEFlags (+0x10); battle_damage_callees.h
// kPCommand / kECommand (+1), kPFlags / kEFlags (+0xC).
struct ActionRecord {
    std::uint8_t command;  // +0x00 the command: auto-battle gives 3 (battle_e4.md AutoBattle_FillCommands); PartyMember::command, BattleEnemy::command
    std::uint8_t command_kind;  // +0x01 5 an item, 4 an ability (battle_setup kPQueuedKind; battle_damage kPCommand / kECommand); PartyMember::command_kind, BattleEnemy::command_kind
    std::uint16_t action_id;  // +0x02 the action id (battle_actions.md): category << 8 | id or the ability id (battle_setup kPQueuedItem; battle_damage kPCommandId); the enemy's ability (battle_e2.cpp:828,866). PartyMember::queued_id, BattleEnemy::ability
    std::int16_t hp_change;  // +0x04 read signed (battle_setup kPHpChange / kEHpChange; battle_e3.cpp:486; battle_e2.cpp:1133)
    std::int16_t ap_change;  // +0x06 (kPApChange / kEApChange)
    std::uint8_t shown;  // +0x08 status-shown bits (battle_setup kPEffect / kEEffect; battle_damage kPShown / kEShown)
    std::uint8_t counter;  // +0x09 (battle_setup kPCounter / kECounter; battle_damage kPShown2 / kEShown2)
    std::uint8_t item_slot;  // +0x0A the member's (battle_setup kPItemSlot); unread on an enemy
    std::uint8_t saved_animation;  // +0x0B the member's +0x4B saved by BattleObj_HitEnter (battle_e3.cpp:380); unread on an enemy
    std::uint32_t flags;  // +0x0C (battle_damage kPFlags / kEFlags; battle_flow.cpp; battle_fx_tasks.cpp); PartyMember::flags_130, BattleEnemy::flags_110
    std::uint32_t flags2;  // +0x10 bit 0x10 the gate, 0x40 / 0x80 the charges (battle_setup kPFlags / kEFlags; battle_damage kPFlags2 / kEFlags2; typed-records.md 3 item 2); PartyMember::flags_134, BattleEnemy::flags_114
    unsigned char pad_14[0x8];  // +0x14..+0x1B: zeroed as two dwords in battle (the member's +0x138 / +0x13C, battle_phases.cpp:222-223, battle_e6.cpp:217-218; the enemy's +0x118..+0x11F at set-up, battle_sprites.cpp); no reader names them
};

// The battle windows' view of a window record (handler 3,
// battle_win_states.md 2, whose table every field below is from;
// Window_DispatchKind dispatches the gauges on +8 / +9).
struct BattleWindow {
    std::uint8_t in_use;  // +0x00 as WindowRecord
    std::uint8_t handler;  // +0x01 3
    std::uint8_t kind;  // +0x02 0..7
    std::uint8_t state;  // +0x03
    std::uint16_t x;  // +0x04 handed to the draws as they are, not 12.4
    std::uint16_t y;  // +0x06
    std::uint8_t hp_gauge;  // +0x08 the HP gauge's kind (Window_DispatchKind)
    std::uint8_t ap_gauge;  // +0x09 the AP gauge's kind
    std::uint8_t slot;  // +0x0A party member 0..2, enemy 3..8, or (kind 3) a banner-pool record
    std::uint8_t hp_length;  // +0x0B the HP gauge's length, 0..55
    std::uint8_t ap_length;  // +0x0C the AP gauge's length
    std::uint8_t out_wait;  // +0x0D cleared on set-up; the out test waits on it being 0
    unsigned char pad_0E[0x1];  // +0xE: cleared on set-up, unread
    std::uint8_t pass;  // +0x0F Window_FlagAndAdvance sets 1, the target states clear it
    std::uint16_t home_x;  // +0x10 the home place Window_RestoreAndBack puts back
    std::uint16_t home_y;  // +0x12
    std::uint16_t hp_shown;  // +0x14 HP as shown
    std::uint16_t ap_shown;  // +0x16 AP as shown
    unsigned char pad_18[0x4];  // +0x18..+0x1B: handed to the gauge step (group CM's handlers); not read by the battle window states
    std::uint16_t hp_top_shown;  // +0x1C HP top as shown
    std::uint16_t ap_top_shown;  // +0x1E AP top as shown
    unsigned char pad_20[0x4];  // +0x20..+0x23: unread
};

// The item list's view of a window record (Menu_DrawItemList 0x5759C0,
// menu-windows.md 2's first column). The buy and sell lists read +0xA..+0xD
// differently (the greyed / price percent, the selected entry, the
// quantity; menu-windows.md 2): those stay WindowRecord bytes, and the shop
// list's dword +0x20 is named here because only the buy list reads it.
struct ItemListWindow {
    std::uint8_t in_use;  // +0x00 as WindowRecord
    std::uint8_t handler;  // +0x01
    std::uint8_t kind;  // +0x02
    std::uint8_t state;  // +0x03
    std::uint16_t x;  // +0x04 whole pixels (menu-windows.md 2 "x, y (s16)")
    std::uint16_t y;  // +0x06
    std::uint8_t use_mode;  // +0x08 the Item_CanUse mode
    std::uint8_t greyed;  // +0x09
    std::uint8_t category;  // +0x0A 0..4
    std::uint8_t top;  // +0x0B the top index
    std::uint8_t marked;  // +0x0C a marked index
    std::uint8_t cursor;  // +0x0D
    unsigned char pad_0E[0x1];  // +0xE: unread by the list
    std::uint8_t pass;  // +0x0F as WindowRecord
    std::uint16_t arrows;  // +0x10 arrow bits 1, 0; a countdown in the high nibble
    std::uint16_t scroll;  // +0x12 the scroll state (Menu_ListScroll)
    unsigned char pad_14[0xC];  // +0x14..+0x1F: unread by the list
    std::uint32_t shop_list;  // +0x20 the buy list's: the shop list (a count, then (category, id) pairs)
};

// An effect object as its kind reads it (effect_1a.md 2.2..2.6; the B1
// effect group's union rows: 760 sites in 59 kinds): the kinds reuse the
// sprite's steps +0xC / +0x10 and screen point +0x2E / +0x30 as words of
// their own - a level and its acceleration (kind 3), an extra record's
// address (kind 0x10), a target height (kind 5), a count and a frame
// (kinds 0x45, 0x68). Neutral names, cast where the kind uses them so; the
// rest through EffectObject.
struct EffectWork {
    unsigned char pad_00[0xC];  // +0x00..+0x0B: EffectObject's
    std::uint32_t work_0c;  // +0x0C a dword of the kind's own (UL / SetUL in 268 of the 358 union rows at +0xC / +0x10, the rest Long / SetLong; kind 3's level, effect_1a.md 2.5; kind 0x10's extra record, 2.2)
    std::uint32_t work_10;  // +0x10 likewise (kind 3's acceleration, kind 5's target height, effect_1a.md 2.5, 2.6)
    unsigned char pad_14[0x1A];  // +0x14..+0x2D: EffectObject's
    std::uint16_t work_2e;  // +0x2E a word of the kind's own, read as u16 (Word / W) in 367 of the 402 union rows at +0x2E / +0x30, as s16 (SW) in 35 (a count: effect_2d.cpp kind 0x45; effect_3a.md 1.5 kind 0x68's count)
    std::uint16_t work_30;  // +0x30 likewise (kind 0x68's frame, effect_3a.md 1.5; kind 0x45's, effect_2d.cpp)
    std::uint16_t work_32;  // +0x32 likewise, where the kind does not use +0x32 as a draw key (the C2 audit, ea F1 and doubt 1, frs F4: kind 0x2B's count, 0x43's countdown, 0x45's amplitude, 0x51's and 0x98's radius, 0xAE's ellipse word and angle, 0x78's saved turn of object 2; kind 18's subs 0x0F and 0x2B a height, 0x17 a counter and a height factor, 0x24 an angle, 0x25, 0x26 and 0x39 a lid index). Kind 18's sub 0x44 draws by it as a key (Sub44_Follow stores 0x3200) and keeps draw_key
    unsigned char pad_34[0x8];  // +0x34..+0x3B: EffectObject's
    std::uint16_t work_3c;  // +0x3C a word of the kind's own where the kind does not keep a height there: the choice menu's (effect 6) page word (rest_1g.cpp kEff6Page, ChoiceMenu_DataPage / _RulePage; C4 audit A F3)
    std::uint16_t work_3e;  // +0x3E likewise: the choice menu's shown page (rest_1g.cpp kEff6Shown)
    unsigned char pad_40[0x40];  // +0x40..+0x7F: EffectObject's
};

// ---------------------------------------------------------------------------
// Families records.h lacked before C1 (typed-data-stage-c.md section 3 item
// 5). Each is named only as far as its readers agree.

// A persistent character record: CharacterRecords 0x903A70 + id * 0xA4,
// eight (symbols.toml [[block]] CharacterRecords; save-interchange.md 2:
// the PSX record's offsets + 4 past the name, which the port widened from 5
// bytes to 9). PartyMember +0x80..+0x123 is a copy of one (item-use.md 2: the
// working copy 0x802DC0 + 0x14C * id). Not the sprite object's layout,
// though the stride is the same.
struct CharStatBlock {
    std::uint16_t max_hp;  // +0x00 (record +0x20 / +0x40; item-use.md 2)
    std::uint16_t max_ap;  // +0x02
    std::uint16_t atk;  // +0x04 (char-stats.md 2: Char_ApplyWeapon adds the weapon's +0x16)
    std::uint16_t def;  // +0x06
    std::uint16_t agi;  // +0x08
    std::uint16_t intel;  // +0x0A
    unsigned char pad_0C[0x2];  // +0xC, +0xD: "a word" (char-stats.md 2; armour 0x28 adds 1); meaning not named
    std::uint8_t byte_0e;  // +0x0E Stat_AddCap99's byte (char-stats.md 2; ItemUse_Stat2EUp, item-use.md 2); PartyMember::byte_ae is the working copy's
    std::uint8_t resist[9];  // +0x0F nine resistance bytes, Stat_AddResist's (char-stats.md 2); PartyMember::elements / holy are the first six of the working copy's
    std::uint8_t percent[5];  // +0x18 five percentage bytes, Stat_AddCap100's (char-stats.md 2)
    unsigned char pad_1D[0x3];  // +0x1D..+0x1F: copied with the block (Char_RecalcStats copies 32 bytes), unread
};

struct CharRecord {
    char name[9];  // +0x00 the name, nine bytes on the PC (save-interchange.md 2; 0x45F5A0 writes 8 of them)
    std::uint8_t char_id;  // +0x09 (save-interchange.md 2; PartyMember +0x89, battle_setup_callees.h kPCharId)
    std::uint8_t level;  // +0x0A (save-interchange.md 2; PartyMember +0x8A)
    std::uint8_t flags;  // +0x0B bit 0 joined: counted by Inventory_Count (char-stats.md 2) and by the name entry 0x45F020 (save-interchange.md 2)
    std::int32_t exp;  // +0x0C the experience: Char_LevelUp, Char_LevelUpGain, BattleResult_AddExp (cap 9999999), Menu_DrawMemberStatus read it as a dword
    std::uint16_t status;  // +0x10 the status word (item-use.md 2; PartyMember +0x90)
    std::uint8_t weapon;  // +0x12 (char-stats.md 2; PartyMember +0x92)
    std::uint8_t armour[3];  // +0x13 (char-stats.md 2: Char_ApplyArmour copies the three)
    std::uint8_t accessory[2];  // +0x16 (char-stats.md 2; PartyMember +0x96)
    std::uint16_t hp;  // +0x18 (item-use.md 2; char-stats.md 2)
    std::uint16_t ap;  // +0x1A (item-use.md 2)
    std::uint8_t survive;  // +0x1C the working copy's +0x9C (battle_setup kPByte9C; battle_damage kPSurvive)
    std::uint8_t halving;  // +0x1D the halving bits, a Stat_AddResist pass per bit (char-stats.md 2 Char_RecalcStats step 4)
    std::uint8_t hp_scale;  // +0x1E the max-HP scale (char-stats.md 2; item-use.md 2 ItemUse_HpScaleUp, to 9)
    std::uint8_t master;  // +0x1F the master, 0xFF none (rest_2d.md MasterJoin_Step5Apply / MasterQuit_Step5Apply; Char_ApplyTraits applies its list)
    CharStatBlock stats;  // +0x20 the effective block (char-stats.md 2)
    CharStatBlock base;  // +0x40 its base copy, copied over +0x20 first by Char_RecalcStats (char-stats.md 2)
    unsigned char pad_60[0x44];  // +0x60..+0xA3: not read by the records' readers here; save-interchange.md 2's late three-byte field is at +0x74 (carried verbatim, not understood)
};

// An encounter slot: 0x939F00 + r * 12, eight (battle_sprites_callees.h
// kEncounter; field_hidden_callees.h / inventory_ops_callees.h kEnemies,
// PSX 0x801463D8; inventory_ops.md). Battle_InitEnemies 0x4942C0 sets up
// each used one as Battle_SetupEnemy(n, +1, +4, +8) (battle_sprites.cpp);
// Encounter_FillSlots writes +0 / +1 (field_hidden.cpp).
struct EncounterSlot {
    std::uint8_t active;  // +0x00 used / placed (Encounter_FillSlots; Battle_InitEnemies; field_hidden.cpp's on-screen test clears it)
    std::uint8_t kind;  // +0x01 the enemy data id (Battle_SetupEnemy's second argument; battle_sprites.md 3), 0xFF none (Encounter_FillSlots)
    unsigned char pad_02[0x2];  // +2, +3: unread
    std::int32_t x;  // +0x04 the place, Battle_SetupEnemy's x (Long, battle_sprites.cpp:759; field_hidden.cpp:530)
    std::int32_t z;  // +0x08 its z
};

// A community record: 0x9046D0 + 8k, sixty (rest_4a.md, rest_4b.md,
// rest_4d.md, rest_4e.md, field_e2.md; the five-byte names beside them at
// 0x9048F0, save-interchange.md 2). Saved at block +0xCF0.
struct CommunityRecord {
    std::uint8_t in_use;  // +0x00 non-zero in use (every reader; CommuName_CountSlots, rest_4d.md)
    std::uint8_t slot;  // +0x01 the building it is placed in, 1..8 (rest_4a.cpp Rec(i)[1] == building + 1; field_e2_callees.h "the record it names (1-based)"; rest_4b.md "the slot"), or a loose kind 0, 9, 0xA, 0xB (rest_4a.md)
    unsigned char pad_02[0x2];  // +2, +3: the readers disagree - "states" (rest_4a.md; rest_4a.cpp:1046 reads +2 as a state), "marks, +3 two nibbles" (rest_4b.md), "an item and its tab" (rest_4e.md)
    std::uint32_t stamp;  // +0x04 a stamp of the battle count 0x904134 (rest_4a.md; rest_4a.cpp:1037 age = Game_BattleCount - +4; rest_4b.md); "a dword" (rest_4e.md)
};

// A community building: 0x9048B0 + 8b, eight (rest_4a.md "buildings";
// rest_4b.md "slots", indexed 1..8 from 0x9048A8; field_e2.md "records").
struct CommunityBuilding {
    std::uint8_t kind;  // +0x00 the kind, 0 empty, else 4 + a pick (rest_4a.md; rest_4b.md; field_e2.cpp:135,149 compares it with 4 and with a kind)
    unsigned char pad_01[0x7];  // +1..+7: the readers disagree - +1 a level (rest_4a.cpp:580), the lit flag (rest_4a.cpp:463), the key (field_e2_callees.h); +3 a level (rest_4a.md) or "counted" (field_e2.cpp:150); +4 a stamp (rest_4a.md, rest_4b.md) or a tally Records_CountUnpairedOfKind counts up (field_e2.cpp:153)
};

// The PSX scratchpad 0x1F800000, relocated by the port to 0x903850
// (symbols.toml [[block]] SharedScratch; its note: "only the one word is
// confirmed"). The magic code (1,546 sites, through each file's SB / SW /
// SetSW / SD / SetSD) and Battle_ScaleDamage use it as the PSX compiler's
// spill area: each function its own locals, the same offset a word in one
// spell and a byte or a dword in the next. So no offset has a name; the
// struct gives each width a typed view, named by offset, with what its
// readers settle: words at even offsets +0..+0x1A, dwords at
// +0..+0x18, bytes at +0..+0x1B (a word's low byte read alone, SB). The
// extent is the readers' (+0x18's dword); Gfx_BufferBlocks 0x903880 bounds it
// at 0x30. Scratch_Swap 0x90385C (symbols.toml) is the dword at +0xC, and
// the move-script counters at 0x903848 sit below it (move_script.cpp).
struct SharedScratch {
    union {
        struct {
            std::uint16_t w_00, w_02, w_04, w_06, w_08, w_0a, w_0c, w_0e,
                w_10, w_12, w_14, w_16, w_18, w_1a;  // +0x00.. the SW / SetSW words (u16: SW reads Word)
        };
        struct {
            std::int32_t d_00, d_04, d_08, d_0c, d_10, d_14, d_18;  // +0x00.. the SD / SetSD dwords (s32: SD reads Long)
        };
        struct {
            std::uint8_t b_00, b_01, b_02, b_03, b_04, b_05, b_06, b_07, b_08, b_09, b_0a, b_0b, b_0c, b_0d,
                b_0e, b_0f, b_10, b_11, b_12, b_13, b_14, b_15, b_16, b_17, b_18, b_19, b_1a, b_1b;  // +0x00.. the SB bytes
        };
    };
};

static_assert(offsetof(SpriteHead, flags) == 0x0, "SpriteHead::flags");
static_assert(offsetof(SpriteHead, state) == 0x1, "SpriteHead::state");
static_assert(offsetof(SpriteHead, step) == 0x2, "SpriteHead::step");
static_assert(offsetof(SpriteHead, state3) == 0x3, "SpriteHead::state3");
static_assert(offsetof(SpriteHead, state4) == 0x4, "SpriteHead::state4");
static_assert(offsetof(SpriteHead, states) == 0x1, "SpriteHead::states");
static_assert(offsetof(SpriteHead, actor) == 0x5, "SpriteHead::actor");
static_assert(offsetof(SpriteHead, kind) == 0x5, "SpriteHead::kind");
static_assert(offsetof(SpriteHead, type) == 0x6, "SpriteHead::type");
static_assert(offsetof(SpriteHead, param) == 0x6, "SpriteHead::param");
static_assert(offsetof(SpriteHead, runner) == 0x6, "SpriteHead::runner");
static_assert(offsetof(SpriteHead, flags2) == 0x7, "SpriteHead::flags2");
static_assert(offsetof(SpriteHead, mode) == 0x7, "SpriteHead::mode");
static_assert(offsetof(SpriteHead, direction) == 0x8, "SpriteHead::direction");
static_assert(offsetof(SpriteHead, layout) == 0x8, "SpriteHead::layout");
static_assert(offsetof(SpriteHead, step_frames) == 0x9, "SpriteHead::step_frames");
static_assert(offsetof(SpriteHead, timer) == 0x9, "SpriteHead::timer");
static_assert(offsetof(SpriteHead, digits) == 0xA, "SpriteHead::digits");
static_assert(offsetof(SpriteHead, byte_0a) == 0xA, "SpriteHead::byte_0a");
static_assert(offsetof(SpriteHead, step_x) == 0xC, "SpriteHead::step_x");
static_assert(offsetof(SpriteHead, step_z) == 0x10, "SpriteHead::step_z");
static_assert(offsetof(SpriteHead, step_y) == 0x14, "SpriteHead::step_y");
static_assert(offsetof(SpriteHead, work_18) == 0x18, "SpriteHead::work_18");
static_assert(offsetof(SpriteHead, work_1c) == 0x1C, "SpriteHead::work_1c");
static_assert(offsetof(SpriteHead, work_20) == 0x20, "SpriteHead::work_20");
static_assert(offsetof(SpriteHead, draw_flags) == 0x24, "SpriteHead::draw_flags");
static_assert(offsetof(SpriteHead, tex_slot) == 0x25, "SpriteHead::tex_slot");
static_assert(offsetof(SpriteHead, tex_row) == 0x26, "SpriteHead::tex_row");
static_assert(offsetof(SpriteHead, clut) == 0x27, "SpriteHead::clut");
static_assert(offsetof(SpriteHead, clut_mode) == 0x28, "SpriteHead::clut_mode");
static_assert(offsetof(SpriteHead, ot_slot) == 0x29, "SpriteHead::ot_slot");
static_assert(offsetof(SpriteHead, flip) == 0x2A, "SpriteHead::flip");
static_assert(offsetof(SpriteHead, key_class) == 0x2B, "SpriteHead::key_class");
static_assert(offsetof(SpriteHead, column) == 0x2C, "SpriteHead::column");
static_assert(offsetof(SpriteHead, screen_x) == 0x2E, "SpriteHead::screen_x");
static_assert(offsetof(SpriteHead, screen_y) == 0x30, "SpriteHead::screen_y");
static_assert(offsetof(SpriteHead, draw_key) == 0x32, "SpriteHead::draw_key");
static_assert(offsetof(SpriteHead, digit_phase) == 0x32, "SpriteHead::digit_phase");
static_assert(offsetof(SpriteHead, x) == 0x34, "SpriteHead::x");
static_assert(offsetof(SpriteHead, x_frac) == 0x34, "SpriteHead::x_frac");
static_assert(offsetof(SpriteHead, x_whole) == 0x36, "SpriteHead::x_whole");
static_assert(offsetof(SpriteHead, z) == 0x38, "SpriteHead::z");
static_assert(offsetof(SpriteHead, z_frac) == 0x38, "SpriteHead::z_frac");
static_assert(offsetof(SpriteHead, z_whole) == 0x3A, "SpriteHead::z_whole");
static_assert(offsetof(SpriteHead, height) == 0x3C, "SpriteHead::height");
static_assert(offsetof(SpriteHead, height_frac) == 0x3C, "SpriteHead::height_frac");
static_assert(offsetof(SpriteHead, height_whole) == 0x3E, "SpriteHead::height_whole");
static_assert(offsetof(SpriteHead, scale_x) == 0x40, "SpriteHead::scale_x");
static_assert(offsetof(SpriteHead, scale_y) == 0x44, "SpriteHead::scale_y");
static_assert(offsetof(SpriteHead, scale_mode) == 0x48, "SpriteHead::scale_mode");
static_assert(offsetof(SpriteHead, script_steps) == 0x49, "SpriteHead::script_steps");
static_assert(offsetof(SpriteHead, script_hold) == 0x4A, "SpriteHead::script_hold");
static_assert(offsetof(SpriteHead, animation) == 0x4B, "SpriteHead::animation");
static_assert(offsetof(SpriteHead, anim_data) == 0x4C, "SpriteHead::anim_data");
static_assert(offsetof(SpriteHead, script) == 0x50, "SpriteHead::script");
static_assert(offsetof(SpriteHead, frames) == 0x54, "SpriteHead::frames");
static_assert(offsetof(SpriteHead, script_pos) == 0x58, "SpriteHead::script_pos");
static_assert(offsetof(SpriteHead, frame_word) == 0x5A, "SpriteHead::frame_word");
static_assert(offsetof(SpriteHead, draw_mode) == 0x5C, "SpriteHead::draw_mode");
static_assert(offsetof(SpriteHead, shade) == 0x5D, "SpriteHead::shade");
static_assert(offsetof(SpriteHead, depth) == 0x60, "SpriteHead::depth");
static_assert(offsetof(SpriteHead, value) == 0x60, "SpriteHead::value");
static_assert(offsetof(SpriteHead, angle) == 0x64, "SpriteHead::angle");
static_assert(offsetof(SpriteHead, reach) == 0x70, "SpriteHead::reach");
static_assert(offsetof(SpriteHead, screen_xf) == 0x74, "SpriteHead::screen_xf");
static_assert(offsetof(SpriteHead, screen_yf) == 0x78, "SpriteHead::screen_yf");
static_assert(offsetof(SpriteHead, depth_f) == 0x7C, "SpriteHead::depth_f");
static_assert(sizeof(SpriteHead) == 0x80, "SpriteHead stride");

// The families derive from SpriteHead, which makes them non-standard-layout:
// offsetof on them is conditionally-supported, and clang and GCC warn
// (-Winvalid-offsetof). Both lay a single non-virtual base at offset 0 and
// the derived members after it, which is what these asserts check.
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif

static_assert(offsetof(SpriteObjectBody, context) == 0x80, "SpriteObjectBody::context");
static_assert(offsetof(SpriteObjectBody, pose_wait) == 0x81, "SpriteObjectBody::pose_wait");
static_assert(offsetof(SpriteObjectBody, loop) == 0x82, "SpriteObjectBody::loop");
static_assert(offsetof(SpriteObjectBody, area_script) == 0x83, "SpriteObjectBody::area_script");
static_assert(offsetof(SpriteObjectBody, speed) == 0x84, "SpriteObjectBody::speed");
static_assert(offsetof(SpriteObjectBody, saved_facing) == 0x85, "SpriteObjectBody::saved_facing");
static_assert(offsetof(SpriteObjectBody, trigger) == 0x86, "SpriteObjectBody::trigger");
static_assert(offsetof(SpriteObjectBody, move_steps) == 0x87, "SpriteObjectBody::move_steps");
static_assert(offsetof(SpriteObjectBody, trigger_word) == 0x88, "SpriteObjectBody::trigger_word");
static_assert(offsetof(SpriteObjectBody, script_position) == 0x8A, "SpriteObjectBody::script_position");
static_assert(offsetof(SpriteObjectBody, target_x) == 0x8C, "SpriteObjectBody::target_x");
static_assert(offsetof(SpriteObjectBody, target_z) == 0x90, "SpriteObjectBody::target_z");
static_assert(offsetof(SpriteObjectBody, best_score) == 0x94, "SpriteObjectBody::best_score");
static_assert(offsetof(SpriteObjectBody, half_w) == 0x98, "SpriteObjectBody::half_w");
static_assert(offsetof(SpriteObjectBody, half_d) == 0x9A, "SpriteObjectBody::half_d");
static_assert(offsetof(SpriteObjectBody, counter) == 0x9C, "SpriteObjectBody::counter");
static_assert(offsetof(SpriteObjectBody, tag) == 0x9E, "SpriteObjectBody::tag");
static_assert(sizeof(SpriteObjectBody) == 0xA0, "SpriteObjectBody: Sprite_Kind2's extent");
static_assert(offsetof(SpriteObject, handler) == 0xA0, "SpriteObject::handler");
static_assert(sizeof(SpriteObject) == 0xA4, "SpriteObject stride");

static_assert(offsetof(PartyMember, char_id) == 0x89, "PartyMember::char_id");
static_assert(offsetof(PartyMember, level) == 0x8A, "PartyMember::level");
static_assert(offsetof(PartyMember, status) == 0x90, "PartyMember::status");
static_assert(offsetof(PartyMember, weapon) == 0x92, "PartyMember::weapon");
static_assert(offsetof(PartyMember, armour) == 0x93, "PartyMember::armour");
static_assert(offsetof(PartyMember, accessory) == 0x96, "PartyMember::accessory");
static_assert(offsetof(PartyMember, hp) == 0x98, "PartyMember::hp");
static_assert(offsetof(PartyMember, ap) == 0x9A, "PartyMember::ap");
static_assert(offsetof(PartyMember, survive) == 0x9C, "PartyMember::survive");
static_assert(offsetof(PartyMember, hp_scale) == 0x9E, "PartyMember::hp_scale");
static_assert(offsetof(PartyMember, max_hp) == 0xA0, "PartyMember::max_hp");
static_assert(offsetof(PartyMember, max_ap) == 0xA2, "PartyMember::max_ap");
static_assert(offsetof(PartyMember, atk) == 0xA4, "PartyMember::atk");
static_assert(offsetof(PartyMember, def) == 0xA6, "PartyMember::def");
static_assert(offsetof(PartyMember, agi) == 0xA8, "PartyMember::agi");
static_assert(offsetof(PartyMember, intel) == 0xAA, "PartyMember::intel");
static_assert(offsetof(PartyMember, byte_ae) == 0xAE, "PartyMember::byte_ae");
static_assert(offsetof(PartyMember, elements) == 0xAF, "PartyMember::elements");
static_assert(offsetof(PartyMember, holy) == 0xB4, "PartyMember::holy");
static_assert(offsetof(PartyMember, crit_chance) == 0xBA, "PartyMember::crit_chance");
static_assert(offsetof(PartyMember, base_max_hp) == 0xC0, "PartyMember::base_max_hp");
static_assert(offsetof(PartyMember, base_max_ap) == 0xC2, "PartyMember::base_max_ap");
static_assert(offsetof(PartyMember, base_atk) == 0xC4, "PartyMember::base_atk");
static_assert(offsetof(PartyMember, base_def) == 0xC6, "PartyMember::base_def");
static_assert(offsetof(PartyMember, base_agi) == 0xC8, "PartyMember::base_agi");
static_assert(offsetof(PartyMember, base_intel) == 0xCA, "PartyMember::base_intel");
static_assert(offsetof(PartyMember, learned) == 0xFE, "PartyMember::learned");
static_assert(offsetof(PartyMember, command) == 0x124, "PartyMember::command");
static_assert(offsetof(PartyMember, command_kind) == 0x125, "PartyMember::command_kind");
static_assert(offsetof(PartyMember, queued_id) == 0x126, "PartyMember::queued_id");
static_assert(offsetof(PartyMember, hp_change) == 0x128, "PartyMember::hp_change");
static_assert(offsetof(PartyMember, ap_change) == 0x12A, "PartyMember::ap_change");
static_assert(offsetof(PartyMember, shown) == 0x12C, "PartyMember::shown");
static_assert(offsetof(PartyMember, counter) == 0x12D, "PartyMember::counter");
static_assert(offsetof(PartyMember, item_slot) == 0x12E, "PartyMember::item_slot");
static_assert(offsetof(PartyMember, saved_animation) == 0x12F, "PartyMember::saved_animation");
static_assert(offsetof(PartyMember, flags_130) == 0x130, "PartyMember::flags_130");
static_assert(offsetof(PartyMember, flags_134) == 0x134, "PartyMember::flags_134");
static_assert(offsetof(PartyMember, field_flags) == 0x138, "PartyMember::field_flags");
static_assert(offsetof(PartyMember, field_object) == 0x139, "PartyMember::field_object");
static_assert(offsetof(PartyMember, saved_script_pos) == 0x140, "PartyMember::saved_script_pos");
static_assert(offsetof(PartyMember, count_4000) == 0x142, "PartyMember::count_4000");
static_assert(offsetof(PartyMember, count_800) == 0x143, "PartyMember::count_800");
static_assert(offsetof(PartyMember, count_40) == 0x144, "PartyMember::count_40");
static_assert(offsetof(PartyMember, count_80) == 0x145, "PartyMember::count_80");
static_assert(offsetof(PartyMember, char_index) == 0x148, "PartyMember::char_index");
static_assert(offsetof(PartyMember, tint) == 0x149, "PartyMember::tint");
static_assert(sizeof(PartyMember) == 0x14C, "PartyMember stride");

static_assert(sizeof(EffectObject) == 0x80, "EffectObject stride");

static_assert(offsetof(BattleEnemy, name) == 0x80, "BattleEnemy::name");
static_assert(offsetof(BattleEnemy, enemy_kind) == 0x8C, "BattleEnemy::enemy_kind");
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
static_assert(offsetof(BattleEnemy, drop0_item) == 0xA8, "BattleEnemy::drop0_item");
static_assert(offsetof(BattleEnemy, drop0_row) == 0xAA, "BattleEnemy::drop0_row");
static_assert(offsetof(BattleEnemy, drop1_item) == 0xAC, "BattleEnemy::drop1_item");
static_assert(offsetof(BattleEnemy, drop1_row) == 0xAE, "BattleEnemy::drop1_row");
static_assert(offsetof(BattleEnemy, max_hp) == 0xB0, "BattleEnemy::max_hp");
static_assert(offsetof(BattleEnemy, max_ap) == 0xB2, "BattleEnemy::max_ap");
static_assert(offsetof(BattleEnemy, atk) == 0xB4, "BattleEnemy::atk");
static_assert(offsetof(BattleEnemy, def) == 0xB6, "BattleEnemy::def");
static_assert(offsetof(BattleEnemy, agi) == 0xB8, "BattleEnemy::agi");
static_assert(offsetof(BattleEnemy, intel) == 0xBA, "BattleEnemy::intel");
static_assert(offsetof(BattleEnemy, elements) == 0xBF, "BattleEnemy::elements");
static_assert(offsetof(BattleEnemy, holy) == 0xC4, "BattleEnemy::holy");
static_assert(offsetof(BattleEnemy, base_max_hp) == 0xD0, "BattleEnemy::base_max_hp");
static_assert(offsetof(BattleEnemy, base_max_ap) == 0xD2, "BattleEnemy::base_max_ap");
static_assert(offsetof(BattleEnemy, base_atk) == 0xD4, "BattleEnemy::base_atk");
static_assert(offsetof(BattleEnemy, base_def) == 0xD6, "BattleEnemy::base_def");
static_assert(offsetof(BattleEnemy, base_agi) == 0xD8, "BattleEnemy::base_agi");
static_assert(offsetof(BattleEnemy, base_intel) == 0xDA, "BattleEnemy::base_intel");
static_assert(offsetof(BattleEnemy, data_id) == 0xF0, "BattleEnemy::data_id");
static_assert(offsetof(BattleEnemy, ai_rows_done) == 0xF1, "BattleEnemy::ai_rows_done");
static_assert(offsetof(BattleEnemy, screen_dx) == 0xF2, "BattleEnemy::screen_dx");
static_assert(offsetof(BattleEnemy, screen_dy) == 0xF3, "BattleEnemy::screen_dy");
static_assert(offsetof(BattleEnemy, event_hook) == 0xF4, "BattleEnemy::event_hook");
static_assert(offsetof(BattleEnemy, cue_table) == 0xF8, "BattleEnemy::cue_table");
static_assert(offsetof(BattleEnemy, anim_table) == 0xFC, "BattleEnemy::anim_table");
static_assert(offsetof(BattleEnemy, run_state) == 0x100, "BattleEnemy::run_state");
static_assert(offsetof(BattleEnemy, hit_class) == 0x101, "BattleEnemy::hit_class");
static_assert(offsetof(BattleEnemy, command) == 0x104, "BattleEnemy::command");
static_assert(offsetof(BattleEnemy, command_kind) == 0x105, "BattleEnemy::command_kind");
static_assert(offsetof(BattleEnemy, ability) == 0x106, "BattleEnemy::ability");
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
static_assert(offsetof(EnemyData, drop0_item) == 0x30, "EnemyData::drop0_item");
static_assert(offsetof(EnemyData, drop0_row) == 0x32, "EnemyData::drop0_row");
static_assert(offsetof(EnemyData, drop1_item) == 0x34, "EnemyData::drop1_item");
static_assert(offsetof(EnemyData, drop1_row) == 0x36, "EnemyData::drop1_row");
static_assert(offsetof(EnemyData, abilities) == 0x1C, "EnemyData::abilities");
static_assert(offsetof(EnemyData, hp) == 0x24, "EnemyData::hp");
static_assert(offsetof(EnemyData, ap) == 0x26, "EnemyData::ap");
static_assert(offsetof(EnemyData, stats) == 0x28, "EnemyData::stats");
static_assert(offsetof(EnemyData, ai_rows) == 0x38, "EnemyData::ai_rows");
static_assert(offsetof(EnemyData, place) == 0x78, "EnemyData::place");
static_assert(offsetof(EnemyData, resist) == 0x7C, "EnemyData::resist");
static_assert(offsetof(EnemyData, status) == 0x85, "EnemyData::status");
static_assert(offsetof(EnemyData, size) == 0x86, "EnemyData::size");
static_assert(offsetof(EnemyData, margin) == 0x87, "EnemyData::margin");
static_assert(offsetof(EnemyData, run_state) == 0x88, "EnemyData::run_state");
static_assert(offsetof(EnemyData, hit_class) == 0x89, "EnemyData::hit_class");
static_assert(offsetof(EnemyData, fx_size) == 0x8A, "EnemyData::fx_size");
static_assert(offsetof(EnemyData, cast_fx) == 0x8B, "EnemyData::cast_fx");
static_assert(sizeof(EnemyData) == 0x8C, "EnemyData stride");

static_assert(offsetof(BattleTask, owner) == 0x80, "BattleTask::owner");
static_assert(sizeof(BattleTask) == 0x84, "BattleTask stride");

static_assert(offsetof(WindowRecord, in_use) == 0x0, "WindowRecord::in_use");
static_assert(offsetof(WindowRecord, handler) == 0x1, "WindowRecord::handler");
static_assert(offsetof(WindowRecord, kind) == 0x2, "WindowRecord::kind");
static_assert(offsetof(WindowRecord, state) == 0x3, "WindowRecord::state");
static_assert(offsetof(WindowRecord, x) == 0x4, "WindowRecord::x");
static_assert(offsetof(WindowRecord, y) == 0x6, "WindowRecord::y");
static_assert(offsetof(WindowRecord, param_a) == 0xA, "WindowRecord::param_a");
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
static_assert(offsetof(SaveSummary, style) == 0x9, "SaveSummary::style");
static_assert(offsetof(SaveSummary, style2) == 0xA, "SaveSummary::style2");
static_assert(offsetof(SaveSummary, hours) == 0xC, "SaveSummary::hours");
static_assert(offsetof(SaveSummary, minutes) == 0xD, "SaveSummary::minutes");
static_assert(offsetof(SaveSummary, seconds) == 0xE, "SaveSummary::seconds");
static_assert(offsetof(SaveSummary, frames) == 0xF, "SaveSummary::frames");
static_assert(offsetof(SaveSummary, exp) == 0x10, "SaveSummary::exp");
static_assert(offsetof(SaveSummary, marked) == 0x14, "SaveSummary::marked");
static_assert(offsetof(SaveSummary, dir_index) == 0x15, "SaveSummary::dir_index");
static_assert(offsetof(SaveSummary, name_tail) == 0x16, "SaveSummary::name_tail");
static_assert(sizeof(SaveSummary) == 0x1C, "SaveSummary stride");

static_assert(offsetof(SaveDirEntry, name) == 0x0, "SaveDirEntry::name");
static_assert(offsetof(SaveDirEntry, size) == 0x14, "SaveDirEntry::size");
static_assert(sizeof(SaveDirEntry) == 0x18, "SaveDirEntry stride");

static_assert(offsetof(ScriptContext, flags) == 0x0, "ScriptContext::flags");
static_assert(offsetof(ScriptContext, pose_wait) == 0x1, "ScriptContext::pose_wait");
static_assert(offsetof(ScriptContext, loop) == 0x2, "ScriptContext::loop");
static_assert(offsetof(ScriptContext, area_script) == 0x3, "ScriptContext::area_script");
static_assert(offsetof(ScriptContext, speed) == 0x4, "ScriptContext::speed");
static_assert(offsetof(ScriptContext, saved_facing) == 0x5, "ScriptContext::saved_facing");
static_assert(offsetof(ScriptContext, move_steps) == 0x7, "ScriptContext::move_steps");
static_assert(offsetof(ScriptContext, position) == 0xA, "ScriptContext::position");
static_assert(sizeof(ScriptContext) == 0xC, "ScriptContext stride");

static_assert(offsetof(MemberFieldTail, context) == 0x0, "MemberFieldTail::context");
static_assert(offsetof(MemberFieldTail, script) == 0xC, "MemberFieldTail::script");
static_assert(offsetof(MemberFieldTail, standing) == 0x12, "MemberFieldTail::standing");
static_assert(offsetof(MemberFieldTail, action) == 0x13, "MemberFieldTail::action");
static_assert(offsetof(MemberFieldTail, field_flags) == 0x14, "MemberFieldTail::field_flags");
static_assert(offsetof(MemberFieldTail, field_object) == 0x15, "MemberFieldTail::field_object");
static_assert(sizeof(MemberFieldTail) == 0x1C, "MemberFieldTail stride");

static_assert(offsetof(ActionRecord, command) == 0x0, "ActionRecord::command");
static_assert(offsetof(ActionRecord, command_kind) == 0x1, "ActionRecord::command_kind");
static_assert(offsetof(ActionRecord, action_id) == 0x2, "ActionRecord::action_id");
static_assert(offsetof(ActionRecord, hp_change) == 0x4, "ActionRecord::hp_change");
static_assert(offsetof(ActionRecord, ap_change) == 0x6, "ActionRecord::ap_change");
static_assert(offsetof(ActionRecord, shown) == 0x8, "ActionRecord::shown");
static_assert(offsetof(ActionRecord, counter) == 0x9, "ActionRecord::counter");
static_assert(offsetof(ActionRecord, item_slot) == 0xA, "ActionRecord::item_slot");
static_assert(offsetof(ActionRecord, saved_animation) == 0xB, "ActionRecord::saved_animation");
static_assert(offsetof(ActionRecord, flags) == 0xC, "ActionRecord::flags");
static_assert(offsetof(ActionRecord, flags2) == 0x10, "ActionRecord::flags2");
static_assert(sizeof(ActionRecord) == 0x1C, "ActionRecord stride");

static_assert(offsetof(BattleWindow, in_use) == 0x0, "BattleWindow::in_use");
static_assert(offsetof(BattleWindow, handler) == 0x1, "BattleWindow::handler");
static_assert(offsetof(BattleWindow, kind) == 0x2, "BattleWindow::kind");
static_assert(offsetof(BattleWindow, state) == 0x3, "BattleWindow::state");
static_assert(offsetof(BattleWindow, x) == 0x4, "BattleWindow::x");
static_assert(offsetof(BattleWindow, y) == 0x6, "BattleWindow::y");
static_assert(offsetof(BattleWindow, hp_gauge) == 0x8, "BattleWindow::hp_gauge");
static_assert(offsetof(BattleWindow, ap_gauge) == 0x9, "BattleWindow::ap_gauge");
static_assert(offsetof(BattleWindow, slot) == 0xA, "BattleWindow::slot");
static_assert(offsetof(BattleWindow, hp_length) == 0xB, "BattleWindow::hp_length");
static_assert(offsetof(BattleWindow, ap_length) == 0xC, "BattleWindow::ap_length");
static_assert(offsetof(BattleWindow, out_wait) == 0xD, "BattleWindow::out_wait");
static_assert(offsetof(BattleWindow, pass) == 0xF, "BattleWindow::pass");
static_assert(offsetof(BattleWindow, home_x) == 0x10, "BattleWindow::home_x");
static_assert(offsetof(BattleWindow, home_y) == 0x12, "BattleWindow::home_y");
static_assert(offsetof(BattleWindow, hp_shown) == 0x14, "BattleWindow::hp_shown");
static_assert(offsetof(BattleWindow, ap_shown) == 0x16, "BattleWindow::ap_shown");
static_assert(offsetof(BattleWindow, hp_top_shown) == 0x1C, "BattleWindow::hp_top_shown");
static_assert(offsetof(BattleWindow, ap_top_shown) == 0x1E, "BattleWindow::ap_top_shown");
static_assert(sizeof(BattleWindow) == 0x24, "BattleWindow stride");

static_assert(offsetof(ItemListWindow, in_use) == 0x0, "ItemListWindow::in_use");
static_assert(offsetof(ItemListWindow, handler) == 0x1, "ItemListWindow::handler");
static_assert(offsetof(ItemListWindow, kind) == 0x2, "ItemListWindow::kind");
static_assert(offsetof(ItemListWindow, state) == 0x3, "ItemListWindow::state");
static_assert(offsetof(ItemListWindow, x) == 0x4, "ItemListWindow::x");
static_assert(offsetof(ItemListWindow, y) == 0x6, "ItemListWindow::y");
static_assert(offsetof(ItemListWindow, use_mode) == 0x8, "ItemListWindow::use_mode");
static_assert(offsetof(ItemListWindow, greyed) == 0x9, "ItemListWindow::greyed");
static_assert(offsetof(ItemListWindow, category) == 0xA, "ItemListWindow::category");
static_assert(offsetof(ItemListWindow, top) == 0xB, "ItemListWindow::top");
static_assert(offsetof(ItemListWindow, marked) == 0xC, "ItemListWindow::marked");
static_assert(offsetof(ItemListWindow, cursor) == 0xD, "ItemListWindow::cursor");
static_assert(offsetof(ItemListWindow, pass) == 0xF, "ItemListWindow::pass");
static_assert(offsetof(ItemListWindow, arrows) == 0x10, "ItemListWindow::arrows");
static_assert(offsetof(ItemListWindow, scroll) == 0x12, "ItemListWindow::scroll");
static_assert(offsetof(ItemListWindow, shop_list) == 0x20, "ItemListWindow::shop_list");
static_assert(sizeof(ItemListWindow) == 0x24, "ItemListWindow stride");

static_assert(offsetof(EffectWork, work_0c) == 0xC, "EffectWork::work_0c");
static_assert(offsetof(EffectWork, work_10) == 0x10, "EffectWork::work_10");
static_assert(offsetof(EffectWork, work_2e) == 0x2E, "EffectWork::work_2e");
static_assert(offsetof(EffectWork, work_30) == 0x30, "EffectWork::work_30");
static_assert(offsetof(EffectWork, work_32) == 0x32, "EffectWork::work_32");
static_assert(offsetof(EffectWork, work_3c) == 0x3C, "EffectWork::work_3c");
static_assert(offsetof(EffectWork, work_3e) == 0x3E, "EffectWork::work_3e");
static_assert(sizeof(EffectWork) == 0x80, "EffectWork stride");

static_assert(offsetof(CharStatBlock, max_hp) == 0x0, "CharStatBlock::max_hp");
static_assert(offsetof(CharStatBlock, max_ap) == 0x2, "CharStatBlock::max_ap");
static_assert(offsetof(CharStatBlock, atk) == 0x4, "CharStatBlock::atk");
static_assert(offsetof(CharStatBlock, def) == 0x6, "CharStatBlock::def");
static_assert(offsetof(CharStatBlock, agi) == 0x8, "CharStatBlock::agi");
static_assert(offsetof(CharStatBlock, intel) == 0xA, "CharStatBlock::intel");
static_assert(offsetof(CharStatBlock, byte_0e) == 0xE, "CharStatBlock::byte_0e");
static_assert(offsetof(CharStatBlock, resist) == 0xF, "CharStatBlock::resist");
static_assert(offsetof(CharStatBlock, percent) == 0x18, "CharStatBlock::percent");
static_assert(sizeof(CharStatBlock) == 0x20, "CharStatBlock stride");

static_assert(offsetof(CharRecord, name) == 0x0, "CharRecord::name");
static_assert(offsetof(CharRecord, char_id) == 0x9, "CharRecord::char_id");
static_assert(offsetof(CharRecord, level) == 0xA, "CharRecord::level");
static_assert(offsetof(CharRecord, flags) == 0xB, "CharRecord::flags");
static_assert(offsetof(CharRecord, exp) == 0xC, "CharRecord::exp");
static_assert(offsetof(CharRecord, status) == 0x10, "CharRecord::status");
static_assert(offsetof(CharRecord, weapon) == 0x12, "CharRecord::weapon");
static_assert(offsetof(CharRecord, armour) == 0x13, "CharRecord::armour");
static_assert(offsetof(CharRecord, accessory) == 0x16, "CharRecord::accessory");
static_assert(offsetof(CharRecord, hp) == 0x18, "CharRecord::hp");
static_assert(offsetof(CharRecord, ap) == 0x1A, "CharRecord::ap");
static_assert(offsetof(CharRecord, survive) == 0x1C, "CharRecord::survive");
static_assert(offsetof(CharRecord, halving) == 0x1D, "CharRecord::halving");
static_assert(offsetof(CharRecord, hp_scale) == 0x1E, "CharRecord::hp_scale");
static_assert(offsetof(CharRecord, master) == 0x1F, "CharRecord::master");
static_assert(offsetof(CharRecord, stats) == 0x20, "CharRecord::stats");
static_assert(offsetof(CharRecord, base) == 0x40, "CharRecord::base");
static_assert(sizeof(CharRecord) == 0xA4, "CharRecord stride");

static_assert(offsetof(EncounterSlot, active) == 0x0, "EncounterSlot::active");
static_assert(offsetof(EncounterSlot, kind) == 0x1, "EncounterSlot::kind");
static_assert(offsetof(EncounterSlot, x) == 0x4, "EncounterSlot::x");
static_assert(offsetof(EncounterSlot, z) == 0x8, "EncounterSlot::z");
static_assert(sizeof(EncounterSlot) == 0xC, "EncounterSlot stride");

static_assert(offsetof(CommunityRecord, in_use) == 0x0, "CommunityRecord::in_use");
static_assert(offsetof(CommunityRecord, slot) == 0x1, "CommunityRecord::slot");
static_assert(offsetof(CommunityRecord, stamp) == 0x4, "CommunityRecord::stamp");
static_assert(sizeof(CommunityRecord) == 8, "CommunityRecord stride");

static_assert(offsetof(CommunityBuilding, kind) == 0x0, "CommunityBuilding::kind");
static_assert(sizeof(CommunityBuilding) == 8, "CommunityBuilding stride");

static_assert(offsetof(SharedScratch, w_00) == 0x0, "SharedScratch::w_00");
static_assert(offsetof(SharedScratch, w_02) == 0x2, "SharedScratch::w_02");
static_assert(offsetof(SharedScratch, w_04) == 0x4, "SharedScratch::w_04");
static_assert(offsetof(SharedScratch, w_06) == 0x6, "SharedScratch::w_06");
static_assert(offsetof(SharedScratch, w_08) == 0x8, "SharedScratch::w_08");
static_assert(offsetof(SharedScratch, w_0a) == 0xA, "SharedScratch::w_0a");
static_assert(offsetof(SharedScratch, w_0c) == 0xC, "SharedScratch::w_0c");
static_assert(offsetof(SharedScratch, w_0e) == 0xE, "SharedScratch::w_0e");
static_assert(offsetof(SharedScratch, w_10) == 0x10, "SharedScratch::w_10");
static_assert(offsetof(SharedScratch, w_12) == 0x12, "SharedScratch::w_12");
static_assert(offsetof(SharedScratch, w_14) == 0x14, "SharedScratch::w_14");
static_assert(offsetof(SharedScratch, w_16) == 0x16, "SharedScratch::w_16");
static_assert(offsetof(SharedScratch, w_18) == 0x18, "SharedScratch::w_18");
static_assert(offsetof(SharedScratch, w_1a) == 0x1A, "SharedScratch::w_1a");
static_assert(offsetof(SharedScratch, d_00) == 0x0, "SharedScratch::d_00");
static_assert(offsetof(SharedScratch, d_04) == 0x4, "SharedScratch::d_04");
static_assert(offsetof(SharedScratch, d_08) == 0x8, "SharedScratch::d_08");
static_assert(offsetof(SharedScratch, d_0c) == 0xC, "SharedScratch::d_0c");
static_assert(offsetof(SharedScratch, d_10) == 0x10, "SharedScratch::d_10");
static_assert(offsetof(SharedScratch, d_14) == 0x14, "SharedScratch::d_14");
static_assert(offsetof(SharedScratch, d_18) == 0x18, "SharedScratch::d_18");
static_assert(offsetof(SharedScratch, b_00) == 0x0, "SharedScratch::b_00");
static_assert(offsetof(SharedScratch, b_01) == 0x1, "SharedScratch::b_01");
static_assert(offsetof(SharedScratch, b_02) == 0x2, "SharedScratch::b_02");
static_assert(offsetof(SharedScratch, b_03) == 0x3, "SharedScratch::b_03");
static_assert(offsetof(SharedScratch, b_04) == 0x4, "SharedScratch::b_04");
static_assert(offsetof(SharedScratch, b_05) == 0x5, "SharedScratch::b_05");
static_assert(offsetof(SharedScratch, b_06) == 0x6, "SharedScratch::b_06");
static_assert(offsetof(SharedScratch, b_07) == 0x7, "SharedScratch::b_07");
static_assert(offsetof(SharedScratch, b_08) == 0x8, "SharedScratch::b_08");
static_assert(offsetof(SharedScratch, b_09) == 0x9, "SharedScratch::b_09");
static_assert(offsetof(SharedScratch, b_0a) == 0xA, "SharedScratch::b_0a");
static_assert(offsetof(SharedScratch, b_0b) == 0xB, "SharedScratch::b_0b");
static_assert(offsetof(SharedScratch, b_0c) == 0xC, "SharedScratch::b_0c");
static_assert(offsetof(SharedScratch, b_0d) == 0xD, "SharedScratch::b_0d");
static_assert(offsetof(SharedScratch, b_0e) == 0xE, "SharedScratch::b_0e");
static_assert(offsetof(SharedScratch, b_0f) == 0xF, "SharedScratch::b_0f");
static_assert(offsetof(SharedScratch, b_10) == 0x10, "SharedScratch::b_10");
static_assert(offsetof(SharedScratch, b_11) == 0x11, "SharedScratch::b_11");
static_assert(offsetof(SharedScratch, b_12) == 0x12, "SharedScratch::b_12");
static_assert(offsetof(SharedScratch, b_13) == 0x13, "SharedScratch::b_13");
static_assert(offsetof(SharedScratch, b_14) == 0x14, "SharedScratch::b_14");
static_assert(offsetof(SharedScratch, b_15) == 0x15, "SharedScratch::b_15");
static_assert(offsetof(SharedScratch, b_16) == 0x16, "SharedScratch::b_16");
static_assert(offsetof(SharedScratch, b_17) == 0x17, "SharedScratch::b_17");
static_assert(offsetof(SharedScratch, b_18) == 0x18, "SharedScratch::b_18");
static_assert(offsetof(SharedScratch, b_19) == 0x19, "SharedScratch::b_19");
static_assert(offsetof(SharedScratch, b_1a) == 0x1A, "SharedScratch::b_1a");
static_assert(offsetof(SharedScratch, b_1b) == 0x1B, "SharedScratch::b_1b");
static_assert(sizeof(SharedScratch) == 0x1C, "SharedScratch stride");

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

}  // namespace records

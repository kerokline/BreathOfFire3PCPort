// Internal to magic_fx_reached.cpp and magic_fx_reached_fuzz.cpp: the
// addresses the magic effects the combat route casts touch that have no name
// in symbols.toml, and every call they make - through pointers, so that the
// start-up fuzz can stand recording functions in for them, for the
// originals' copies and for ours alike. docs/magic_fx_reached.md.
//
// Calls into other groups' functions of the eighth round, or into anything
// unnamed, go through the raw addresses below and are never bound here:
//   0x4ED5C0  the steal double's phase 0: its effect size (group CK's)
//   0x4AEE90  the steal double's phase 3: a jmp to BattleTask_FreeCurrent (CK's)
//   0x4B1E70  the sparkles' phase 1: the caster's tint released and set (CK's)
//   0x4B1ED0  the sparkles' phase 2: a tint record brightened (CK's)
//   0x4EE8A0  the sparkles' phase 3: wait for the sparkles to thin out (CK's)
//   0x4F7350  the sparkles' phase 5: the end, when the last one is gone (CK's)
//   0x4B58F0  an item's name into Text_Records, by (index, category) (unnamed,
//             in no group: reached only on a successful steal)
//
// The dispatches that are not calls to a named function:
//   - five stack tables (mov [esp + 4k], imm32; call [esp + 4 * index]), ours
//     in Callees below: the index is not checked by the original - past the
//     table it calls through its own stack - and ours aborts there;
//   - three .data tables read in place, as the original does, the index
//     unchecked: FxRing_Phases 0x65B5B8 (by +1), StealClone_Types 0x65AC28
//     (by +1), FxDim_Phases 0x65C3A0 (by +1). The fuzz swaps their entries.
#pragma once

#include <cstdint>

namespace magic_fx_reached {

namespace at {

// The battle task slots (BattleTask_Create's): 48 of 0x84 bytes; Sprite_Current
// is the slot being run, 0x93B940 its +0x80 (the task's owner).
constexpr std::uint32_t kTasks = 0x93A000;
constexpr std::uint32_t kTaskStride = 0x84;
constexpr unsigned kTaskCount = 48;
constexpr std::uint32_t kOwner = 0x93B940;          // unsigned char *, BattleTask_RunAll's copy of +0x80

// The battle state bytes.
constexpr std::uint32_t kFlags = 0x904AA8;          // u8: bit 2 "the effect is done", set by the effects' ends
constexpr std::uint32_t kActor = 0x904B34;          // u8: the acting actor, 0..2 a party member
constexpr std::uint32_t kTarget = 0x904B44;         // u8: the target, 3.. an enemy
constexpr std::uint32_t kSource = 0x904B4C;         // unsigned char *: the sprite the effect starts from
constexpr std::uint32_t kMessageUp = 0x939F60;      // u8: the battle message window is up (0x597D90 sets it)

// The party (ObjTrio, stride 0x14C) and the enemies (stride 0x128, index - 3).
constexpr std::uint32_t kParty = 0x802D40;
constexpr std::uint32_t kPartyStride = 0x14C;
constexpr std::uint32_t kEnemies = 0x93B960;
constexpr std::uint32_t kEnemyStride = 0x128;
constexpr unsigned kStealItem = 0xA8;               // u16 on the enemy: category << 8 | index, 0 none
constexpr unsigned kStealRate = 0xAA;               // u8 on the enemy: row of Steal_RateTable
constexpr unsigned kEnemySpeed = 0xB8;              // u16 on the enemy, against the thief's +0xA8
constexpr unsigned kThiefSpeed = 0xA8;              // u16 on the party record

// .data tables.
constexpr std::uint32_t kStealRates = 0x65AC20;     // Steal_RateTable: s8 by the enemy's +0xAA
constexpr std::uint32_t kCloneTypes = 0x65AC28;     // StealClone_Types: 2 entries
constexpr std::uint32_t kRingPhases = 0x65B5B8;     // FxRing_Phases: 4 entries
constexpr std::uint32_t kDimPhases = 0x65C3A0;      // FxDim_Phases: 4 entries, then 0
constexpr std::uint32_t kSparkleCounts = 0x65AE10;  // Sparkle_CountByKind: u8
constexpr std::uint32_t kSparkleSteps = 0x65AE18;   // Sparkle_DelayByKind: u8

// The sparkle pool (battle_items_callees.h has its fields).
constexpr std::uint32_t kSparklePool = 0x684790;
constexpr std::uint32_t kSparkleStride = 0x2C;
constexpr unsigned kSparkles = 0x80;
constexpr std::uint32_t kSparkleCurrent = 0x685D90;

// The CLUT FxDiscFan_Start marks semi-transparent: strip row 26 (VRAM row
// 506) cells 1..15 from the buffer 0x4000 below, cell 0 cleared.
constexpr std::uint32_t kClutRow = 0x812980;        // Gfx_ClutStrip + 0x3400
constexpr std::uint32_t kClutSource = 0x80E980;     // kClutRow - 0x4000

// MoveScript_TintRecords: 12 bytes each; Sparkle_End dims bytes +2..+4.
constexpr std::uint32_t kTints = 0x7E0700;

}  // namespace at

// Raw addresses of callees this group does not own (see the top).
constexpr std::uint32_t kCloneSize = 0x4ED5C0;
constexpr std::uint32_t kCloneFree = 0x4AEE90;
constexpr std::uint32_t kSparkleTint = 0x4B1E70;
constexpr std::uint32_t kSparkleBrighten = 0x4B1ED0;
constexpr std::uint32_t kSparkleThin = 0x4EE8A0;
constexpr std::uint32_t kSparkleDone = 0x4F7350;
constexpr std::uint32_t kItemName = 0x4B58F0;

using Handler = void (__cdecl*)();

struct Callees {
    // The five stack tables, in the originals' order.
    Handler disc_fan[3];       // 0x4C4FC0's: FxDiscFan_Start, FxDiscFan_Grow, FxDiscFan_Fade
    Handler steal[4];          // 0x4B54B0's: Steal_Start, Steal_Wait, Steal_Report, MagicFx_EndWhenIdle
    Handler clone[4];          // 0x4B5830's: 0x4ED5C0, StealClone_Run, StealClone_Finish, 0x4AEE90
    Handler sparkle_task[6];   // 0x4B8D70's: Sparkle_Spawn, 0x4B1E70, 0x4B1ED0, 0x4EE8A0, Sparkle_End, 0x4F7350
    Handler sparkle[3];        // 0x4B9000's: Sparkle_Launch, Sparkle_Rise, Sparkle_Fade
    // Calls.
    void (__cdecl* update_screen_xy)();                              // BattleActor_UpdateScreenXY (ours)
    void (__cdecl* draw_disc)();                                     // MagicFx_DrawDisc (ours)
    void (__cdecl* push_matrix)();                                   // MagicFx_PushActorMatrix (ours)
    void (__cdecl* draw_fan)();                                      // MagicFx_DrawFan (ours)
    void (__cdecl* pop_matrix)();                                    // Gte_PopMatrix (ours)
    void (__cdecl* draw_ring)();                                     // MagicFx_DrawRing (ours)
    unsigned char (__cdecl* task_create)(unsigned, unsigned);        // BattleTask_Create (ours)
    void (__cdecl* free_current)();                                  // BattleTask_FreeCurrent (ours)
    void (__cdecl* set_target_flags)(unsigned, unsigned);            // Battle_SetTargetFlags (ours)
    void (__cdecl* set_target_flag40)(unsigned);                     // Battle_SetTargetFlag40 (ours)
    void (__cdecl* play_by_id)(unsigned short);                      // Sound_PlayById (ours)
    void (__cdecl* play_effect)(unsigned short);                     // Sound_PlayEffect (ours)
    void (__cdecl* set_animation)(unsigned, unsigned);               // BattleActor_SetAnimation (ours)
    int (__cdecl* rand)();                                           // Rand, the CRT's
    unsigned char (__cdecl* inventory_add)(unsigned, unsigned, unsigned);   // Inventory_Add (ours)
    void (__cdecl* item_name)(unsigned, unsigned);                   // 0x4B58F0
    const unsigned char* (__cdecl* msg_system)(unsigned);            // Msg_SystemPtr (ours)
    unsigned long (__cdecl* queue_push)(unsigned, unsigned, unsigned long);   // BattleQueue_Push (ours)
    unsigned char (__cdecl* script_tick_once)();                     // Sprite_ScriptTickOnce (ours)
    void (__cdecl* play_sound)(unsigned, unsigned);                  // BattleActor_PlaySound (ours)
    void (__cdecl* update_screen)();                                 // Sprite_UpdateScreen (ours)
    void (__cdecl* sparkle_dispatch)();                              // Sparkle_Dispatch (ours)
    unsigned char (__cdecl* sparkle_alloc)();                        // Sparkle_Alloc (ours)
    void (__cdecl* release_tint)(unsigned char*);                    // Sprite_ReleaseTint (ours)
    void (__cdecl* flash)(unsigned);                                 // BattleActor_Flash (ours)
    void (__cdecl* sparkle_disc)();                                  // Sparkle_DrawDisc (ours)
    void (__cdecl* rays_g2)(unsigned, unsigned);                     // Sparkle_DrawRaysG2 (ours)
    void (__cdecl* rays_g3)(unsigned, unsigned);                     // Sparkle_DrawRaysG3 (ours)
    void (__cdecl* tint_clut)(int);                                  // AreaMap_TintClut (ours)
};

extern const Callees kOriginals;
extern Callees g;

// BOF3X_SHADOW=magic_fx_reached: the start-up fuzz, magic_fx_reached_fuzz.cpp.
// Clones every original before MagicFxReached_Inject patches it.
void SelfTest();

}  // namespace magic_fx_reached

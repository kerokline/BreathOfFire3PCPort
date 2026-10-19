// Internal to battle_actions.cpp and battle_actions_fuzz.cpp: the battle's
// globals the action phases touch, and every call they make - through
// pointers, so that the start-up fuzz can stand recording functions in for
// them, for ours and for the originals' copies alike. docs/battle_actions.md.
//
// The seven step tables are not calls to a named function: each stub reads its
// step byte and calls (or tail-jumps) through a table of code pointers in
// .data (symbols.toml names them, BattleAction_Steps and the rest). Ours reads
// the same table afresh, so the fuzz swaps the table's entries for recorders
// in memory, as battle_flow's does for 0x64B084. The event hook 0x904B6C is
// called the same way.
//
// Calls into other groups' functions and into unnamed ones go through raw
// addresses here and are never bound (docs/takeover-queue-round8.md, the rule

// for calls across groups): 0x435AB0, in no group. 0x42F9D0 is round twelve
// group BE1's BattleAction_PickRandomAbility (docs/battle_e1.md): its constant
// names it, the value unchanged, so the fuzz's keys stand.
// for calls across groups): 0x435AB0 and 0x42F9D0, both in no group then.
// 0x435AB0 is round twelve group BE2's BattleEnemy_PickAction since 2026-09-29:
// its constant names it, the value unchanged (the fuzz keys on it).
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"

namespace battle_actions {

namespace at {

// The battle's phase bytes: 0x904AA0 the phase (Battle_PhaseDispatch's
// index, 3 is the actions), 0x904AA1 the action step (BattleAction_Steps),
// 0x904AA2 and 0x904AA3 the sub-steps below it.
// kStep: now BattleActions_Step (symbols.toml).
// kSub: now BattleE4_Step2 (symbols.toml).
// kSub2: now BattleE4_Step3 (symbols.toml).
constexpr std::uint32_t kPhase = 0x904AA0;
// kCounter: now BossSb_Countdown (symbols.toml). u8, zeroed as an action starts its effect
constexpr std::uint32_t kRoundFlags = 0x904AA8;    // u16 (0x904AA9 its high byte)
constexpr std::uint32_t kEventBattle = 0x904AAA;   // u8: non-zero, the hook 0x904B6C is called (battle_flow.md)
constexpr std::uint32_t kQueue = 0x904ACC;         // u8 per slot: the actors in turn order, 0xFF a spent slot
// kQueueAt: now BossSe_RoundSlot (symbols.toml).
constexpr std::uint32_t kQueueEnd = 0x904AE3;  // u8 each
constexpr std::uint32_t kBattleEnd = 0x904AE8;     // u8
// The acting actor's block and the target's, 0x10 bytes each and the same
// layout: +0 the actor (0..2 a member, 3..10 an enemy; the target byte may be
// 0x40 / 0x80 / 0xC0, a side), +1 the command kind (the actor's), +2 / +4 /
// +6 the screen position words, +8 the actor object, +0xC its action record
// (a member's +0x124, an enemy's +0x104; its u16 at +2 the action id).
constexpr std::uint32_t kActor = 0x904B34, kTarget = 0x904B44;
constexpr unsigned kBKind = 1, kBX = 2, kBY = 4, kBZ = 6, kBObject = 8, kBAction = 0xC;
constexpr std::uint32_t kEventHook = 0x904B6C;     // void (*)(int)
constexpr std::uint32_t kApCost = 0x904B78;        // u8: the AP cost when the action is 0x97
constexpr std::uint32_t kAbility = 0x904B80;       // u16: the action id of the action in hand
// kPending: now BattleActions_Pending (symbols.toml). u16: a bit per actor with an effect to show
constexpr std::uint32_t kCostShown = 0x904B88;     // u8: the AP cost Skill_ApCost answered
// kAbilityShown: now BattleE3_SoundSet (symbols.toml). u8: the ability's low byte
constexpr std::uint32_t kText0 = 0x904CE0, kText1 = 0x904D00;   // Text_Records[0] and [1], 32 bytes each
constexpr std::uint32_t kNameBuf = 0x904EA0;       // the banner's 16-byte name copy
constexpr std::uint32_t kBannerShown = 0x8031F3;   // u8, set to 1 with a banner
constexpr std::uint32_t kStats = 0x939FE0;         // 32 bytes: the actor's effective stat block, copied per action
// kMsgBusy: now BattleE1_MessageUp (symbols.toml). u8: non-zero, the enemy message step waits
constexpr std::uint32_t kMsgCount = 0x93C2A2;      // u8: messages left in 0x939FBC..
constexpr std::uint32_t kMsgList = 0x939FBC;       // 4 bytes per message (1-based): +0 the enemy, +2 the u16 message id
// kCancelButtons: now Field_CancelButtons (symbols.toml). u16, Field_CancelButtons
// kInputHeld: now Input_Held (symbols.toml). u16, Input_Held
// The actor objects: 0..2 ObjTrio 0x802D40 stride 0x14C, 3.. the enemies
// 0x93B960 stride 0x128 (actor - 3). Their enemy name records are +0x80.
// kParty: now ObjTrio (symbols.toml).
constexpr std::uint32_t kPartyStride = 0x14C;
constexpr std::uint32_t kEnemy = 0x93B960, kEnemyStride = 0x128;
// Constant tables in .data.
// kStandOffsets: now BattleWin_MemberTargetOffsets (symbols.toml). s8 x / y pairs by (object +8) + (char id +0x89) * 4
constexpr std::uint32_t kAbilityRecords = 0x65C4C8;// 24 bytes per action: +0 the name, +0x10 flags, +0x14 u16, +0x15 flags
constexpr std::uint32_t kItemRows = 0x64B274;      // 4 pointers to u8 tables (battle_flow.md)
constexpr std::uint32_t kAbilityRows = 0x64C1D0;   // u8 by action id
// kMagicFiles: now Magic_Rows (symbols.toml). 8-byte rows, the u16 DAT file first
// kTextMiss: now BattleActions_TextMiss (symbols.toml).
// kTextCounter: now BattleActions_TextCounter (symbols.toml). const char* each

// The seven step tables (symbols.toml [[data]]), their entry counts.
// kSteps: now BattleAction_Steps (symbols.toml). 5, by 0x904AA1: 0x42F250 0x42F500 0x42FC50 0x42FDD0 0x430010
// kBeginSteps: now BattleAction_BeginSteps (symbols.toml). 2, by 0x904AA2: 0x42F260 0x42F4C0
// kKindSteps: now BattleAction_KindSteps (symbols.toml). 6, by 0x904AA2 (the kind): 0x42F510 0x42F5B0 0x42F510 0x42F5E0 (BattleAction_Kind3Dispatch) 0x42F670 0x42FAF0
// kAbilitySteps: now BattleAction_AbilitySteps (symbols.toml). 3, by 0x904AA3: 0x42F680 0x42F880 0x42FAB0
// kItemSteps: now BattleAction_ItemSteps (symbols.toml). 3, by 0x904AA3: 0x42FB00 0x42FBD0 0x42FC10
// kEffectSteps: now BattleAction_EffectSteps (symbols.toml). 3, by 0x904AA2: 0x42FC60 0x42FD20 0x42FD90
// kAfterSteps: now BattleAction_AfterSteps (symbols.toml). 3, by 0x904AA2: 0x42FDE0 0x42FE20 (BattleAction_AbilityNotice) 0x42FF70

}  // namespace at

// Raw addresses of the callees no group owns (the rule above).
constexpr std::uint32_t kEnemyPickAction = bof3::addr::BattleEnemy_PickAction;   // 0x435AB0 (enemy): Rand, then 0x904B35 = a 2-bit kind from 0x65563C
constexpr std::uint32_t kPickTarget = bof3::addr::BattleAction_PickRandomAbility;   // 0x42F9D0, () -> al: the random action of 0x24 / 0x25 / 0x8C and its target

// Every callee that answers in al is typed unsigned char, as symbols.gen.h has
// the named ones: the originals test only al.
struct Callees {
    void (__cdecl* member_auto_target)(unsigned);                 // Battle_MemberAutoTarget 0x453FA0
    void (__cdecl* enemy_pick_action)(unsigned);                  // BattleEnemy_PickAction 0x435AB0 (BE2's)
    void (__cdecl* clear_acting_flags)();                         // Battle_ClearActingFlags 0x4301B0
    void (__cdecl* open_msg_window)();                            // Battle_OpenMsgWindow 0x444310
    const unsigned char* (__cdecl* msg)(unsigned);                // Msg_SystemPtr 0x497740
    unsigned long (__cdecl* banner_add)(unsigned, unsigned, unsigned, unsigned, const char*);   // BattleBanner_Add 0x44A650
    unsigned long (__cdecl* set_actor_bit)(unsigned);             // Battle_SetActorBit 0x446FB0
    unsigned char (__cdecl* action_suits_target)();               // Battle_ActionSuitsTarget 0x453210
    void (__cdecl* remove_from_turn_order)(unsigned);             // Battle_RemoveFromTurnOrder 0x446650
    char* (__cdecl* str_copy_n)(char*, const char*, unsigned);    // Str_CopyN 0x5171A0
    unsigned char (__cdecl* skill_ap_cost)(unsigned, unsigned, unsigned);   // Skill_ApCost 0x591DB0
    unsigned char (__cdecl* pick_flag8_member)();                 // Battle_PickFlag8Member 0x4537A0
    unsigned char (__cdecl* pick_target)();                       // BattleAction_PickRandomAbility 0x42F9D0
    void (__cdecl* load_for_ability)(unsigned);                   // Magic_LoadForAbility 0x4379D0
    int (__cdecl* file_load_done)();                              // File_LoadDone 0x454810
    void (__cdecl* start_ability_magic)(unsigned, unsigned);      // Battle_StartAbilityMagic 0x437930
    unsigned char (__cdecl* item_suits_target)();                 // Battle_ItemSuitsTarget 0x4532A0
    void (__cdecl* return_queued_item)(unsigned);                 // Battle_ReturnQueuedItem 0x446EA0
    unsigned char* (__cdecl* item_name)(unsigned, unsigned);      // Item_NamePtr 0x591680
    void (__cdecl* load_for_item)(unsigned);                      // Magic_LoadForItem 0x4377D0
    void (__cdecl* start_item_magic)(unsigned, unsigned);         // Battle_StartItemMagic 0x437780
    unsigned char (__cdecl* any_flag_f0)();                       // Battle_AnyFlagF0 0x453190
    void (__cdecl* load_dat)(int);                                // LoadDatFile 0x454590
    void (__cdecl* clut_copy_row)(unsigned);                      // Gfx_ClutStripCopyRow 0x4549F0
    unsigned char (__cdecl* settle_flag8)();                      // Battle_SettleFlag8 0x453B10
    unsigned long (__cdecl* queue_push)(unsigned, unsigned, unsigned long);   // BattleQueue_Push 0x44A880
};

extern const Callees kOriginals;
extern Callees g;

// BOF3X_SHADOW=battle_actions: the start-up fuzz, battle_actions_fuzz.cpp.
// Clones every original before BattleActions_Inject patches it.
void SelfTest();

}  // namespace battle_actions

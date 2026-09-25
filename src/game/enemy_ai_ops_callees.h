// Internal to enemy_ai_ops.cpp and enemy_ai_ops_fuzz.cpp: every call the enemy
// AI script ops make, through pointers, so that the start-up fuzz can stand
// recording functions in for them - for the originals' copies and for ours
// alike. docs/enemy_ai_ops.md.
//
// Three kinds of call are not calls to a named function:
//   - the seven dispatchers jump through .data tables (EnemyOp_Steps
//     0x64B1A0 and the six after it) by one of Sprite_Current's step bytes
//     +1 / +2 / +3; ours read the same tables, and the fuzz swaps their
//     entries for recorders;
//   - EnemyOp_Wait 0x4363E0 calls through EnemyOp_WaitSubs 0x64B1EC by +2;
//   - EnemyOp_ReceiveAction 0x436740 calls the enemy object's +0xF4 hook
//     with 1 in an event battle; the fuzz points +0xF4 at a recorder.
// Addresses of other groups' functions and of functions nobody owns yet (the
// round's cross-group rule, docs/takeover-queue-round8.md) are raw here and
// never bound by name.
#pragma once

#include <cstdint>

namespace enemy_ai_ops {

namespace at {

// The enemy objects (PC 0x93B960 + 0x128 n, PSX 0x801EB5A0 + 0x118 n; the
// working record, PSX 0x801EB620, is +0x80 of each - sibling BATTLE_RAM.md
// "Enemy working records"). Sprite_Current and 0x939AD8 are both the enemy
// being run while BattleEnemy_RunAll dispatches its state.
constexpr std::uint32_t kEnemies = 0x93B960;
constexpr std::uint32_t kEnemySize = 0x128;
constexpr unsigned kEnemyCount = 8;
constexpr std::uint32_t kEnemyCurrent = 0x939AD8;  // unsigned char*: the enemy being run (PSX 0x801EB458)
// The 48 battle-task slots, 0x84 bytes each (docs/battle_flow.md). EnemyOp_Wait
// indexes them by the actor number Sprite_Current +5: byte +9 its bob counter,
// dwords +0x34 / +0x38 the base the bob is added to.
constexpr std::uint32_t kTasks = 0x93A000;
constexpr std::uint32_t kTaskSize = 0x84;
// The 32 tint records, 12 bytes each (MoveScript_TintRecords; +2..+4 r g b).
constexpr std::uint32_t kTints = 0x7E0700;
constexpr std::uint32_t kTintSize = 12;
// The action being resolved (PSX 0x80146390.., sibling BATTLE_RAM.md
// "Commands, Auto, Run and the enemy AI").
constexpr std::uint32_t kStatBlock = 0x939F80;     // 32 bytes: the target's +0xB0.. copied in
constexpr std::uint32_t kCommandSource = 0x939FA0; // pointer: byte +0 the actor the command is aimed at
constexpr std::uint32_t kRoundFlags = 0x904AA8;    // u16 (PSX 0x801462E4)
constexpr std::uint32_t kEventBattle = 0x904AAA;   // u8 (docs/battle_flow.md: "event battle" is a guess)
constexpr std::uint32_t kTargeting = 0x904AAF;     // u8: non-zero while a target is being picked (by use here)
constexpr std::uint32_t kPulse = 0x904AC8;         // u8: the menu cursor's pulse (Battle_PulseStep)
constexpr std::uint32_t kActor = 0x904B34;         // u8: the acting actor
constexpr std::uint32_t kActionKind = 0x904B35;    // u8: 1 attack, 4 ability, 5 item (sibling's command bytes)
constexpr std::uint32_t kTarget = 0x904B44;        // u8: the target actor, or 0x40 / 0x80 / 0xC0 a side
constexpr std::uint32_t kResultTarget = 0x904B54;  // u8 (PSX 0x80146390)
constexpr std::uint32_t kResultSprite = 0x904B5C;  // pointer: the target's sprite
constexpr std::uint32_t kResult = 0x904B60;        // pointer: the result record, +4 HP and +6 AP delta, +8 flags
constexpr std::uint32_t kHookA = 0x904B64;         // code pointers stored by EnemyOp_Begin
constexpr std::uint32_t kHookB = 0x904B68;
constexpr std::uint32_t kMagicId = 0x904B80;       // u16: the item or ability (low byte id, high byte category)
constexpr std::uint32_t kStatusAdd = 0x904B98;     // u16: status bits ORed into the target after the effect
constexpr std::uint32_t kStatusAdd2 = 0x904B9A;    // u16: zeroed with it
// The 24-byte ability records (0x65C4D0 + 24 id): the ops test byte +8 bits
// 2 and 4 and byte +0xD bit 3 (what each does here: docs/enemy_ai_ops.md).
constexpr std::uint32_t kAbilityFlags8 = 0x65C4D8;
constexpr std::uint32_t kAbilityFlagsD = 0x65C4DD;
// Constants the ops store.
constexpr std::uint32_t kAnimTable = 0x64B078;     // the enemy's animation bytes 0..7
constexpr std::uint32_t kHookAValue = 0x437720;    // group CE's
constexpr std::uint32_t kHookBValue = 0x437750;    // group CE's

// The op tables in .data (symbols.toml [[data]] EnemyOp_*): each a run of
// code pointers indexed by one of Sprite_Current's step bytes.
constexpr std::uint32_t kSteps = 0x64B1A0;         // by +1, 12 entries (a null after them)
constexpr std::uint32_t kEnterSubs = 0x64B1D4;     // by +2, 2
constexpr std::uint32_t kScaleInSubs = 0x64B1DC;   // by +3, 2
constexpr std::uint32_t kWaitSubs = 0x64B1EC;      // by +2, 2 (called, not jumped to)
constexpr std::uint32_t kActSubs = 0x64B1FC;       // by +2, 6
constexpr std::uint32_t kHitSubs = 0x64B214;       // by +3, 3
constexpr std::uint32_t kDeathSubs = 0x64B234;     // by +3, 4

}  // namespace at

// Callees with no name in symbols.gen.h, or another group's: by address.
constexpr std::uint32_t kItemClass = 0x591810;     // nobody's; (category, id) -> a class byte (docs/battle_sprites.md)
constexpr std::uint32_t kApPopup = 0x453EB0;       // nobody's; Battle_SetDamagePopup's AP twin (reads a word and a byte)
constexpr std::uint32_t kPlayCue = 0x437450;       // nobody's; Sound_PlayEffect(id) unless id is 0xFFFF

using Handler = void (__cdecl*)();

struct Callees {
    // ours (battle_flow.cpp)
    void (__cdecl* set_animation)(unsigned);
    unsigned char (__cdecl* script_tick)();
    unsigned char (__cdecl* script_tick_once)();
    unsigned char (__cdecl* chance70)();
    void (__cdecl* defeated)();
    // other modules' and Capcom's
    unsigned char (__cdecl* sprite_tick_once)();
    unsigned long (__cdecl* status_tint)(unsigned);
    unsigned char (__cdecl* set_tint)(unsigned char*, unsigned char, unsigned char, unsigned char, unsigned char);
    void (__cdecl* tint_release)(unsigned char);
    void (__cdecl* release_tint)(unsigned char*);
    short (__cdecl* apply_damage)(unsigned, unsigned);
    void (__cdecl* effect_apply)();
    unsigned (__cdecl* item_class)(unsigned, unsigned);
    void (__cdecl* damage_popup)(unsigned, unsigned);
    void (__cdecl* ap_popup)(unsigned, unsigned);
    unsigned long (__cdecl* turn_check)();
    void (__cdecl* play_cue)(unsigned);
    void (__cdecl* play_effect)(unsigned short);
    void (__cdecl* hit_sound)();
    void (__cdecl* hit_popup)();
    unsigned long (__cdecl* clear_actor_bit)(unsigned);
};

extern const Callees kOriginals;
extern Callees g;

// The start-up fuzz (enemy_ai_ops_fuzz.cpp): clones every function of this
// file with each call out re-aimed at a recording stand-in and the seven op
// tables' entries swapped for recorders, runs ours against the clones from
// the same random state, and ends the process through bof3::Fatal on any
// difference.
void SelfTest();

}  // namespace enemy_ai_ops

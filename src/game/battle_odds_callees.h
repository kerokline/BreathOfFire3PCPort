// Internal to battle_odds.cpp and battle_odds_fuzz.cpp: the addresses group
// CK's functions touch that have no name in symbols.toml, and every call they
// make - through pointers, so that the start-up fuzz can stand recording
// functions in for them, for the originals' copies and for ours alike. Every
// callee is already ours in another module (named in symbols.toml); none of
// this round's other groups' addresses is called. docs/battle_odds.md.
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"

namespace battle_odds {

using U = std::uint32_t;

// --- Addresses without a name --------------------------------------------------
// The banner pool's current entry and the mask of kinds seen this frame
// (battle_misc_callees.h has the pool's layout).
constexpr U kBannerCurrent = 0x93B8C0;
constexpr U kBannerKinds = 0x904AE9;
constexpr U kBanners = 0x93B8E0;
// Window records (WindowRecords 0x803160, 0x24 bytes each) the two banner
// handlers write: record 0's +3 and +0xA, record 4's +3 and +0xA.
constexpr U kWin0State = 0x803163;
constexpr U kWin0Index = 0x80316A;
constexpr U kWin4State = 0x8031F3;
constexpr U kWin4Index = 0x8031FA;
// Effect_ApplyResult's operands (battle_damage_callees.h): the result record's
// pointer, the target byte, the acting dword (its low byte the actor).
constexpr U kResult = 0x904B60;
constexpr U kTarget = 0x904B54;
constexpr U kActing = 0x904B34;
// The acting actor's object, stored by the action phase (0x42F400: mov
// [0x904B4C], ecx, the object whose +8 and +0x2E it reads next).
constexpr U kActorObject = 0x904B4C;
// The target byte Battle_SetTargetFlag40 is handed (battle_sprites_callees.h
// kTarget) and the round's flags, whose bit 2 the finish state sets.
constexpr U kFlagTarget = 0x904B44;
constexpr U kRoundFlags = 0x904AA8;
// Effect_ApplyResult's 130 handlers (Effect_Handlers): the two of this group
// are slots 8 and 31.
constexpr U kHandlerTable = 0x64E73C;

// The named data, by the address its symbols.gen.h macro names.
namespace at {
inline U Of(const volatile void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
inline U Sprite_CurrentAt() { return Of(&Sprite_Current); }
inline U TintRecordsAt() { return Of(MoveScript_TintRecords); }   // 0x7E0700: 32 of 12 bytes (+2..+4 r g b)
}  // namespace at

// Every callee returns eax whole (U), since the group's functions pass a
// callee's eax on to their callers.
struct Callees {
    U (__cdecl* none_of_kind)(U);          // BattleBanner_NoneOfKind 0x44A830 (battle_misc)
    U (__cdecl* calc_damage)(U, U, U);     // Battle_CalcDamage 0x445CF0 (battle_damage)
    U (__cdecl* free_task)();              // BattleTask_FreeCurrent 0x4351F0 (battle_flow)
    U (__cdecl* release_tint)(U);          // Sprite_ReleaseTint 0x454DC0 (field_event)
    U (__cdecl* set_tint)(U, U, U, U, U);  // Sprite_SetTint 0x454CC0 (battle_sprites)
    U (__cdecl* fx_size)();                // BattleActor_FxSize 0x4FC1F0 (battle_items)
    U (__cdecl* set_flag40)(U);            // Battle_SetTargetFlag40 0x4530D0 (battle_sprites)
};
extern const Callees kOriginals;
extern Callees g;

// BOF3X_SHADOW=battle_odds: the start-up fuzz, battle_odds_fuzz.cpp. Clones
// every original before BattleOdds_Inject patches it.
void SelfTest();

}  // namespace battle_odds

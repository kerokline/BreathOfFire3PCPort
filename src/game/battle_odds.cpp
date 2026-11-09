// Group CK of the eighth takeover round (docs/battle_odds.md): ten small
// functions of the battle, every one pointer-reached, each read to its last
// instruction with capstone against bof3/BOF3.exe on 2026-09-25, and every
// site that holds its address read for what the caller does with the call.
// Faithful: no divergence. The start-up fuzz is battle_odds_fuzz.cpp.
//
//   BattleBanner_TickKind1   0x44A740..0x44A7AC (0x6D)  BattleBanner_Dispatch's table, kind 1
//   BattleBanner_TickKind2   0x44A7B0..0x44A807 (0x58)  kind 2 (PSX 0x801DE71C, docs/battle_misc.md 1.1)
//   Effect_Heal20            0x44C140..0x44C14B (0xC)   Effect_Handlers slot 8
//   Effect_HalfAttack        0x44C990..0x44C9B7 (0x28)  Effect_Handlers slot 31
//   BattleFx_FreeTask        0x4AEE90..0x4AEE94 (0x5)   a tail jmp; 31 stack tables (PSX 0x800C2120)
//   BattleFx_TintActor       0x4B1E70..0x4B1EC3 (0x54)  7 stack tables
//   BattleFx_Brighten        0x4B1ED0..0x4B1F34 (0x65)  11 stack tables
//   BattleFx_SetSize         0x4ED5C0..0x4ED5D6 (0x17)  7 stack tables
//   BattleFx_WaitStep4       0x4EE8A0..0x4EE8B7 (0x18)  5 stack tables
//   BattleFx_Finish          0x4F7350..0x4F7377 (0x28)  45 stack tables
//
// Every one is called with no argument - `call [esp + reg*4 + k]` from a
// table of immediates its host builds on its stack, or `call [0x64E73C +
// 4 * i]` from Effect_ApplyResult - and several hosts return the handler's
// eax as their own (`call; add esp, n; ret`), so ours return eax whole.
#include "game/battle_odds.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/battle_odds_callees.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace battle_odds {

namespace {
template <typename T, typename F> T As(F f) { return reinterpret_cast<T>(reinterpret_cast<void*>(f)); }
}  // namespace

const Callees kOriginals = {
    As<U (__cdecl*)(U)>(BattleBanner_NoneOfKind),
    As<U (__cdecl*)(U, U, U)>(Battle_CalcDamage),
    As<U (__cdecl*)()>(BattleTask_FreeCurrent),
    As<U (__cdecl*)(U)>(Sprite_ReleaseTint),
    As<U (__cdecl*)(U, U, U, U, U)>(Sprite_SetTint),
    As<U (__cdecl*)()>(BattleActor_FxSize),
    As<U (__cdecl*)(U)>(Battle_SetTargetFlag40),
};
Callees g = kOriginals;

}  // namespace battle_odds

namespace {

using namespace battle_odds;

unsigned char* AtBattleOdds(battle_odds::U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }
U Long(const unsigned char* p) {
    U v;
    std::memcpy(&v, p, sizeof v);
    return v;
}

U SpriteCurrent() { return Long(AtBattleOdds(battle_odds::at::Sprite_CurrentAt())); }

}  // namespace

// --- the banner pool's handlers ----------------------------------------------------

// --- two effect handlers -------------------------------------------------------------

// --- the battle effect objects' states -------------------------------------------------
// Each runs on Sprite_Current, an effect object: +1 its state (the host's
// table index), +2 a second counter, +9 a countdown, +0xA a tint record,
// +0xB a byte the waits test.

// original 0x4F7350: once +0xB is 0 - Battle_SetTargetFlag40(the byte
// kFlagTarget; pushed in ecx, whose upper bits are the entry's, and the callee
// reads the low byte), kRoundFlags |= 4, and a tail jump to
// BattleTask_FreeCurrent, whose eax is returned. Otherwise eax Sprite_Current.
extern "C" unsigned long __cdecl BattleFx_Finish(void) {
    const U s = SpriteCurrent();
    if (AtBattleOdds(s + 0xB)[0] != 0) return s;
    g.set_flag40(AtBattleOdds(::bof3::Img(kFlagTarget.v))[0]);
    AtBattleOdds(::bof3::Img(kRoundFlags.v))[0] = static_cast<unsigned char>(AtBattleOdds(::bof3::Img(kRoundFlags.v))[0] | 4);
    return g.free_task();
}

void BattleOdds_Inject() {
    if (bof3::WantsShadow("battle_odds")) battle_odds::SelfTest();
    BOF3_INJECT(BattleFx_Finish);
}

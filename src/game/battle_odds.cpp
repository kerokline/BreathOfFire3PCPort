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
battle_odds::U WordBattleOdds(const unsigned char* p) {
    std::uint16_t v;
    std::memcpy(&v, p, sizeof v);
    return v;
}
U Long(const unsigned char* p) {
    U v;
    std::memcpy(&v, p, sizeof v);
    return v;
}
void PutWord(unsigned char* p, U v) {
    const auto w = static_cast<std::uint16_t>(v);
    std::memcpy(p, &w, sizeof w);
}

battle_odds::U BannerCurrent() { return BattleMisc_BannerCurrent; }
U SpriteCurrent() { return Long(AtBattleOdds(battle_odds::at::Sprite_CurrentAt())); }

}  // namespace

// --- the banner pool's handlers ----------------------------------------------------

// original 0x44A740 (BattleBanner_Dispatch's kind 1; the name banner
// BattleBanner_ShowName sets): the entry kBannerCurrent points at. A timer
// (word +8) of 0xFF runs forever. Otherwise the kind byte is or'ed into the
// kinds seen, the timer counted down, and while it has not reached 0 - both
// read through kBannerCurrent again - window record 4's +0xA gets
// SharedScratch (the entry's index, which the dispatch stored). At 0: the
// entry's active byte 0; its kind taken back out of the kinds seen (the
// pointer read again, the kinds byte before the kind byte); then
// BattleBanner_NoneOfKind(1), and when no kind-1 entry is left (al set)
// window record 4's +3 = 0. eax is the entry pointer, or NoneOfKind's eax.
extern "C" unsigned long __cdecl BattleBanner_TickKind1(void) {
    unsigned char* e = AtBattleOdds(BannerCurrent());
    if (WordBattleOdds(e + 8) != 0xFF) {
        BattleMisc_BannerKinds = static_cast<unsigned char>(BattleMisc_BannerKinds | e[1]);
        PutWord(e + 8, WordBattleOdds(e + 8) - 1);
        e = AtBattleOdds(BannerCurrent());
        if (WordBattleOdds(e + 8) == 0) {
            e[0] = 0;
            const battle_odds::U entry = BannerCurrent();
            const unsigned char kinds = BattleMisc_BannerKinds;
            const unsigned char kind = AtBattleOdds(entry + 1)[0];
            BattleMisc_BannerKinds = static_cast<unsigned char>(kinds & static_cast<unsigned char>(~kind));
            const battle_odds::U eax = battle_odds::g.none_of_kind(1);
            if (eax & 0xFF) WindowRecords[4].state = 0;
            return eax;
        }
    }
    Window4_Index = SharedScratch.b_00;
    return Low32(e);
}

// original 0x44A7B0 (kind 2; PSX 0x801DE71C): as kind 1 with three
// differences. The kind is or'ed into the kinds seen before the timer is
// tested, even for 0xFF; it is never taken back out; and the test is
// NoneOfKind(4), after which window record 0's +3 = 2. While the timer runs,
// record 0's +0xA gets SharedScratch. The word is decremented through a
// register (only its low 16 bits stored) and the pointer read again only then;
// at 0xFF the entry the first read found is tested, which is never 0.
extern "C" unsigned long __cdecl BattleBanner_TickKind2(void) {
    unsigned char* e = AtBattleOdds(BannerCurrent());
    const unsigned char kinds = BattleMisc_BannerKinds;
    BattleMisc_BannerKinds = static_cast<unsigned char>(kinds | e[1]);
    const battle_odds::U timer = WordBattleOdds(e + 8);
    if (timer != 0xFF) {
        PutWord(e + 8, timer - 1);
        e = AtBattleOdds(BannerCurrent());
    }
    if (WordBattleOdds(e + 8) == 0) {
        e[0] = 0;
        const battle_odds::U eax = battle_odds::g.none_of_kind(4);
        if (eax & 0xFF) WindowRecords[0].state = 2;
        return eax;
    }
    Window0_ParamA = SharedScratch.b_00;
    return Low32(e);
}

// --- two effect handlers -------------------------------------------------------------

// original 0x44C140 (Effect_Handlers slot 8): the result record's HP delta
// (word +4 of *kResult) = -20, a heal of 20 (positive is damage,
// Effect_ApplyResult). eax the record pointer.
extern "C" unsigned long __cdecl Effect_Heal20(void) {
    const battle_odds::U result = BattleDamage_Result;
    rv::Store<battle_odds::U>(rv::As<records::ActionRecord>(AtBattleOdds(result))->hp_change, 0xFFEC);  // the result record (records::ActionRecord)
    return result;
}

// original 0x44C990 (Effect_Handlers slot 31): the HP delta half an attack -
// Battle_CalcDamage(actor, target, 0xFFFF), actor the byte kActing, target
// the byte kTarget, the weapon's element; its low word shifted right by one,
// arithmetically (sar ax, 1). The two bytes are pushed in registers whose
// upper bits are the entry's (mov al / mov cl); Battle_CalcDamage reads their
// low bytes only. eax is Battle_CalcDamage's with the low word halved; the
// result pointer is read after the call.
extern "C" unsigned long __cdecl Effect_HalfAttack(void) {
    const battle_odds::U target = AtBattleOdds(::bof3::Img(kTarget.v))[0];
    const battle_odds::U actor = AtBattleOdds(::bof3::Img(kActing.v))[0];
    const battle_odds::U amount = battle_odds::g.calc_damage(actor, target, 0xFFFF);
    const battle_odds::U half = static_cast<std::uint16_t>(static_cast<std::int16_t>(amount) >> 1);
    rv::Store<battle_odds::U>(rv::As<records::ActionRecord>(AtBattleOdds(BattleDamage_Result))->hp_change, half);
    return (amount & 0xFFFF0000u) | half;
}

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
    BOF3_INJECT(BattleBanner_TickKind1);
    BOF3_INJECT(BattleBanner_TickKind2);
    BOF3_INJECT(Effect_Heal20);
    BOF3_INJECT(Effect_HalfAttack);
    BOF3_INJECT(BattleFx_Finish);
}

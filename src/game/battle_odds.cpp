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
//   BattleFx_Finish          0x4F7350..0x4F7377 (0x28)  47 stack tables
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

unsigned char* At(U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }
U Word(const unsigned char* p) {
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

U BannerCurrent() { return Long(At(kBannerCurrent)); }
U SpriteCurrent() { return Long(At(at::Sprite_CurrentAt())); }
unsigned char* Sprite() { return At(SpriteCurrent()); }

}  // namespace

// --- the banner pool's handlers ----------------------------------------------------

// original 0x44A740 (BattleBanner_Dispatch's kind 1; the name banner
// BattleBanner_ShowName sets): the entry kBannerCurrent points at. A timer
// (word +8) of 0xFF runs forever. Otherwise the kind byte is or'ed into the
// kinds seen, the timer counted down, and while it has not reached 0 - both
// read through kBannerCurrent again - window record 4's +0xA gets
// DamageScratch (the entry's index, which the dispatch stored). At 0: the
// entry's active byte 0; its kind taken back out of the kinds seen (the
// pointer read again, the kinds byte before the kind byte); then
// BattleBanner_NoneOfKind(1), and when no kind-1 entry is left (al set)
// window record 4's +3 = 0. eax is the entry pointer, or NoneOfKind's eax.
extern "C" unsigned long __cdecl BattleBanner_TickKind1(void) {
    unsigned char* e = At(BannerCurrent());
    if (Word(e + 8) != 0xFF) {
        At(kBannerKinds)[0] = static_cast<unsigned char>(At(kBannerKinds)[0] | e[1]);
        PutWord(e + 8, Word(e + 8) - 1);
        e = At(BannerCurrent());
        if (Word(e + 8) == 0) {
            e[0] = 0;
            const U entry = BannerCurrent();
            const unsigned char kinds = At(kBannerKinds)[0];
            const unsigned char kind = At(entry + 1)[0];
            At(kBannerKinds)[0] = static_cast<unsigned char>(kinds & static_cast<unsigned char>(~kind));
            const U eax = g.none_of_kind(1);
            if (eax & 0xFF) At(kWin4State)[0] = 0;
            return eax;
        }
    }
    At(kWin4Index)[0] = At(bof3::addr::DamageScratch)[0];
    return static_cast<U>(reinterpret_cast<std::uintptr_t>(e));
}

// original 0x44A7B0 (kind 2; PSX 0x801DE71C): as kind 1 with three
// differences. The kind is or'ed into the kinds seen before the timer is
// tested, even for 0xFF; it is never taken back out; and the test is
// NoneOfKind(4), after which window record 0's +3 = 2. While the timer runs,
// record 0's +0xA gets DamageScratch. The word is decremented through a
// register (only its low 16 bits stored) and the pointer read again only then;
// at 0xFF the entry the first read found is tested, which is never 0.
extern "C" unsigned long __cdecl BattleBanner_TickKind2(void) {
    unsigned char* e = At(BannerCurrent());
    const unsigned char kinds = At(kBannerKinds)[0];
    At(kBannerKinds)[0] = static_cast<unsigned char>(kinds | e[1]);
    const U timer = Word(e + 8);
    if (timer != 0xFF) {
        PutWord(e + 8, timer - 1);
        e = At(BannerCurrent());
    }
    if (Word(e + 8) == 0) {
        e[0] = 0;
        const U eax = g.none_of_kind(4);
        if (eax & 0xFF) At(kWin0State)[0] = 2;
        return eax;
    }
    At(kWin0Index)[0] = At(bof3::addr::DamageScratch)[0];
    return static_cast<U>(reinterpret_cast<std::uintptr_t>(e));
}

// --- two effect handlers -------------------------------------------------------------

// original 0x44C140 (Effect_Handlers slot 8): the result record's HP delta
// (word +4 of *kResult) = -20, a heal of 20 (positive is damage,
// Effect_ApplyResult). eax the record pointer.
extern "C" unsigned long __cdecl Effect_Heal20(void) {
    const U result = Long(At(kResult));
    PutWord(At(result + 4), 0xFFEC);
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
    const U target = At(kTarget)[0];
    const U actor = At(kActing)[0];
    const U amount = g.calc_damage(actor, target, 0xFFFF);
    const U half = static_cast<std::uint16_t>(static_cast<std::int16_t>(amount) >> 1);
    PutWord(At(Long(At(kResult)) + 4), half);
    return (amount & 0xFFFF0000u) | half;
}

// --- the battle effect objects' states -------------------------------------------------
// Each runs on Sprite_Current, an effect object: +1 its state (the host's
// table index), +2 a second counter, +9 a countdown, +0xA a tint record,
// +0xB a byte the waits test.

// original 0x4AEE90 (PSX 0x800C2120): `jmp BattleTask_FreeCurrent` - the state
// that ends an effect, the running battle task's slot freed. 31 host tables in
// .text and one .data slot (0x64D474) hold it. eax the callee's.
extern "C" unsigned long __cdecl BattleFx_FreeTask(void) { return g.free_task(); }

// original 0x4B1E70: the countdown +9 stepped (a byte); at 0 the acting
// actor's object (kActorObject, read before the countdown) loses its tints
// (Sprite_ReleaseTint) and gets a new one, Sprite_SetTint(object, 0, 0, 0, 1),
// whose index (al) becomes +0xA; then +9 = 8 and the next state. Sprite_Current
// is read again for each store. eax is Sprite_Current as last read.
extern "C" unsigned long __cdecl BattleFx_TintActor(void) {
    const U object = Long(At(kActorObject));
    unsigned char* s = Sprite();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    s = Sprite();
    if (s[9] != 0) return static_cast<U>(reinterpret_cast<std::uintptr_t>(s));
    g.release_tint(object);
    const U index = g.set_tint(object, 0, 0, 0, 1);
    Sprite()[0xA] = static_cast<unsigned char>(index);
    Sprite()[9] = 8;
    s = Sprite();
    s[1] = static_cast<unsigned char>(s[1] + 1);
    return static_cast<U>(reinterpret_cast<std::uintptr_t>(s));
}

// original 0x4B1ED0: the red, green and blue bytes (+2, +3, +4) of tint record
// +0xA of MoveScript_TintRecords (12 bytes each, the index read again for
// each; no bound) stepped up by one; then the countdown +9 down, and at 0 the
// next state. Sprite_Current is read once for all of that and once more for
// the test. eax Sprite_Current.
extern "C" unsigned long __cdecl BattleFx_Brighten(void) {
    unsigned char* const s = Sprite();
    for (U k = 2; k <= 4; ++k) {
        unsigned char* const c = At(at::TintRecordsAt() + s[0xA] * 12u + k);
        c[0] = static_cast<unsigned char>(c[0] + 1);
    }
    s[9] = static_cast<unsigned char>(s[9] - 1);
    unsigned char* const t = Sprite();
    if (t[9] == 0) t[1] = static_cast<unsigned char>(t[1] + 1);
    return static_cast<U>(reinterpret_cast<std::uintptr_t>(t));
}

// original 0x4ED5C0: +9 = BattleActor_FxSize() (al; Sprite_Current read after
// the call), then +2 stepped (read again). eax Sprite_Current.
extern "C" unsigned long __cdecl BattleFx_SetSize(void) {
    const U size = g.fx_size();
    Sprite()[9] = static_cast<unsigned char>(size);
    unsigned char* const s = Sprite();
    s[2] = static_cast<unsigned char>(s[2] + 1);
    return static_cast<U>(reinterpret_cast<std::uintptr_t>(s));
}

// original 0x4EE8A0: once +0xB is 4 or below (unsigned), +9 = 8 and the next
// state. eax Sprite_Current.
extern "C" unsigned long __cdecl BattleFx_WaitStep4(void) {
    unsigned char* s = Sprite();
    if (s[0xB] > 4) return static_cast<U>(reinterpret_cast<std::uintptr_t>(s));
    s[9] = 8;
    s = Sprite();
    s[1] = static_cast<unsigned char>(s[1] + 1);
    return static_cast<U>(reinterpret_cast<std::uintptr_t>(s));
}

// original 0x4F7350: once +0xB is 0 - Battle_SetTargetFlag40(the byte
// kFlagTarget; pushed in ecx, whose upper bits are the entry's, and the callee
// reads the low byte), kRoundFlags |= 4, and a tail jump to
// BattleTask_FreeCurrent, whose eax is returned. Otherwise eax Sprite_Current.
extern "C" unsigned long __cdecl BattleFx_Finish(void) {
    const U s = SpriteCurrent();
    if (At(s + 0xB)[0] != 0) return s;
    g.set_flag40(At(kFlagTarget)[0]);
    At(kRoundFlags)[0] = static_cast<unsigned char>(At(kRoundFlags)[0] | 4);
    return g.free_task();
}

void BattleOdds_Inject() {
    if (bof3::WantsShadow("battle_odds")) battle_odds::SelfTest();
    BOF3_INJECT(BattleBanner_TickKind1);
    BOF3_INJECT(BattleBanner_TickKind2);
    BOF3_INJECT(Effect_Heal20);
    BOF3_INJECT(Effect_HalfAttack);
    BOF3_INJECT(BattleFx_FreeTask);
    BOF3_INJECT(BattleFx_TintActor);
    BOF3_INJECT(BattleFx_Brighten);
    BOF3_INJECT(BattleFx_SetSize);
    BOF3_INJECT(BattleFx_WaitStep4);
    BOF3_INJECT(BattleFx_Finish);
}

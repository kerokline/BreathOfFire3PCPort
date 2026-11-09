// Group BF of the seventh takeover round (docs/battle_misc.md): twenty-seven
// small battle helpers, each read to its last instruction with capstone against
// bof3/BOF3.exe and, where one is paired, against its PSX twin in BATTLE.EMI
// (the sibling's overlay captures). Faithful: no divergence. The start-up fuzz
// is battle_misc_fuzz.cpp.
//
//   Battle_WrapIndex            0x4469F0..0x446A09 (0x1A)   -
//   Battle_PulseStep            0x446A10..0x446A43 (0x34)   PSX 0x801DD7C8
//   Battle_PlayActorCue         0x446A50..0x446A7B (0x2C)   PSX 0x801DD820 Battle_PlayActorCue
//   Battle_StatusTint           0x446BB0..0x446BCD (0x1E)   PSX 0x801DDA7C
//   Battle_InitActorContext     0x446BD0..0x446C23 (0x54)   PSX 0x801DDAB8
//   Battle_InitActorContexts    0x446C30..0x446CA2 (0x73)   PSX 0x801DDB44
//   Battle_LoadSoundByKey       0x446E40..0x446E98 (0x59)   -
//   Battle_SetActorBit          0x446FB0..0x446FC2 (0x13)   PSX 0x801DE158
//   Battle_ClearActorBit        0x446FD0..0x446FE4 (0x15)   PSX 0x801DE178 Battle_ClearActorBit
//   Battle_SpawnActorCopies     0x446FF0..0x44710C (0x11D)  PSX 0x801DE19C
//   ItemMenu_CanUseSelected     0x447840..0x44787B (0x3C)   -
//   ItemMenu_SetupForActor      0x447E60..0x447F33 (0xD4)   -
//   Battle_ReturnTrue           0x449E00..0x449E02 (0x3)    PSX 0x80097F44
//   ItemMenu_SetupForParty      0x449E10..0x449E8A (0x7B)   PSX 0x80097F4C
//   ItemMenu_FreeWindows        0x449FE0..0x449FFF (0x20)   PSX 0x800981D4
//   BattleBanner_Dispatch       0x44A5C0..0x44A647 (0x88)   PSX 0x801DE404 BattleBanner_Dispatch
//   BattleBanner_Add            0x44A650..0x44A6D8 (0x89)   PSX 0x801DE528
//   BattleBanner_Set            0x44A6E0..0x44A73A (0x5B)   PSX 0x801DE5D4
//   BattleBanner_ClearAll       0x44A810..0x44A82A (0x1B)   PSX 0x801DE7CC
//   BattleBanner_NoneOfKind     0x44A830..0x44A871 (0x42)   PSX 0x801DE820 BattleBanner_NoneOfKind
//   BattleQueue_Push            0x44A880..0x44A8B6 (0x37)   PSX 0x801DE888
//   BattleQueue_Pending         0x44A8C0..0x44A8D0 (0x11)   PSX 0x801DE8F4
//   BattleBanner_SetMessage     0x44A8E0..0x44A90A (0x2B)   PSX 0x801DE914
//   BattleBanner_ShowName       0x44A990..0x44A9FE (0x6F)   PSX 0x801DEA20
//   EnemyAI_TurnCheck           0x44AAD0..0x44AE12 (0x343) + tables 0x44AE14 (20 dwords), 0x44AE64 (0x26 bytes)
//                                                           PSX 0x80098BB0 EnemyAI_TurnCheck
//   Str_CopyN                   0x5171A0..0x5171D3 (0x34)   PSX 0x8015030C
//   Sprite_UpdateObjectScreens  0x517440..0x517483 (0x44)   PSX 0x801A1624 (callers tier)
#include "game/battle_misc.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "common/asm_name.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/battle_misc_callees.h"
#include "game/battle_text.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace battle_misc {

namespace {
template <typename T> T* Fn(U address) { return reinterpret_cast<T*>(static_cast<std::uintptr_t>(address)); }
template <typename T, typename F> T As(F f) { return reinterpret_cast<T>(reinterpret_cast<void*>(f)); }
using F0 = U __cdecl();
}  // namespace

const Callees kOriginals = {
    Fn<U __cdecl(U, U)>(::bof3::Code(bof3::addr::BattleTask_Create)),
    Fn<U __cdecl(U)>(::bof3::Code(bof3::addr::Battle_ActorIsOut)),
    Fn<U __cdecl(U, U, U, U, U)>(::bof3::Code(bof3::addr::Sprite_SetTint)),
    As<U (__cdecl*)(U)>(Sound_PlayEffect),
    As<U (__cdecl*)(U)>(Sound_LoadStream),
    Fn<U __cdecl(U, U, U)>(::bof3::Code(bof3::addr::Char_AbilityList)),
    Fn<U __cdecl(U, U, U)>(::bof3::Code(bof3::addr::Skill_CanUse)),
    As<U (__cdecl*)()>(Window_FreeCurrent),
    Fn<U __cdecl(U)>(::bof3::Code(bof3::addr::EnemyAI_CondElement)),
    Fn<U __cdecl(U, U)>(::bof3::Code(bof3::addr::EnemyAI_RowDone)),
    Fn<U __cdecl(U, U)>(::bof3::Code(bof3::addr::EnemyAI_ApplyAction)),
    Fn<U __cdecl(U, U, U)>(::bof3::Code(bof3::addr::EnemyAI_SetRowDone)),
    Fn<F0>(::bof3::Code(bof3::addr::EnemyAI_DedupMessages)),
    As<U (__cdecl*)()>(Sprite_UpdateScreenA),
    Fn<F0>(::bof3::Code(bof3::addr::Sprite_UpdateScreenSlot)),
    As<U (__cdecl*)()>(Battle_InitActorContext),
    As<U (__cdecl*)(U, U, U)>(Str_CopyN),
    As<U (__cdecl*)(U, U, U, U, U, U)>(BattleBanner_Set),
    {Fn<F0>(::bof3::Code(kRetOnly.v)), Fn<F0>(::bof3::Code(kBannerKind1.v)), Fn<F0>(::bof3::Code(kBannerKind2.v)), Fn<F0>(::bof3::Code(kRetOnly.v)), Fn<F0>(::bof3::Code(kRetOnly.v))},
};
Callees g = kOriginals;

}  // namespace battle_misc

namespace {

using namespace battle_misc;

unsigned char* AtBattleMisc(battle_misc::U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }
battle_misc::U WordBattleMisc(const unsigned char* p) {
    std::uint16_t v;
    std::memcpy(&v, p, sizeof v);
    return v;
}
battle_misc::U LongBattleMisc(const unsigned char* p) {
    battle_misc::U v;
    std::memcpy(&v, p, sizeof v);
    return v;
}
void PutWord(unsigned char* p, U v) {
    const auto w = static_cast<std::uint16_t>(v);
    std::memcpy(p, &w, sizeof w);
}
void PutLong(unsigned char* p, battle_misc::U v) { std::memcpy(p, &v, sizeof v); }
battle_misc::U Addr(const void* p) { return Low32(p); }

battle_misc::U SpriteCurrent() { return LongBattleMisc(AtBattleMisc(at::Sprite_CurrentAt())); }
void SetSpriteCurrent(battle_misc::U v) { PutLong(AtBattleMisc(battle_misc::at::Sprite_CurrentAt()), v); }

}  // namespace

// --- small arithmetic --------------------------------------------------------------

// original 0x446BB0 (PSX 0x801DDA7C): when bit 7 of the status argument is set,
// Sprite_SetTint(Sprite_Current, -6, -10, 0, 0) and its eax returned; otherwise
// nothing at all - eax is the caller's own. Two of the twelve callers
// (0x44F40E, 0x44F446, in Battle_InflictStatus) return eax on, so the no-tint
// path is a bare ret here too. A pair by width (docs/x64-asm-policy.md 8
// unit 6): Battle_InflictStatus is ours (rest_3d.cpp) and calls this as a
// statement, as every other caller does, so no C++ reads the no-tint eax -
// but this file's fuzz does (battle_misc_fuzz.cpp FuzzTint seeds eax and
// compares the result: a plain `return 0` failed 1494 of 3000 rounds), and
// the x86_64 half is the same three instructions on the argument register.
extern "C" unsigned long __cdecl BattleMisc_StatusTintBody(void) {
    return g.set_tint(SpriteCurrent(), 0xFFFFFFFAu, 0xFFFFFFF6u, 0, 0);
}

// --- the actor contexts -------------------------------------------------------------

// --- sound, bits ------------------------------------------------------------------------

// --- the item window --------------------------------------------------------------------

// --- the banner pool and the message queue -----------------------------------------------

// --- the enemy AI's per-turn rows -------------------------------------------------------

// --- a string copy and the objects' screen pass ----------------------------------------------

void BattleMisc_Inject() {
    if (bof3::WantsShadow("battle_misc")) battle_misc::SelfTest();
}

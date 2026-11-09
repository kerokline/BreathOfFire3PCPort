// The magic effects the recorded fight casts that only a pointer reaches:
// the PSX BMAGIC overlays' code compiled into the exe, run as battle tasks
// (BattleTask_RunAll's slots; Sprite_Current is the slot, 0x93B940 its
// owner). docs/magic_fx_reached.md.
//
//   - The disc-and-fan effect (kind-2 row 21, 0x4C4FC0): its start 0x4C5020,
//     the grow and fade 0x4AD130 / 0x4AD160 it shares with a twin overlay,
//     and its six ring tasks (kind 1, parameter 0x16, 0x4C5110) whose three
//     phases 0x4AD200 / 0x4AD270 / 0x4AD2B0 are FxRing_Phases 0x65B5B8's.
//   - The steal (kind-2 row 69, 0x4B54B0): the start 0x4B54F0 that rolls it,
//     the wait 0x4B5770, the report 0x4B57C0, the shared end 0x4F52D0; and the
//     thief's double (kind 1, parameter 0x45, 0x4B5810 through
//     StealClone_Types 0x65AC28 to 0x4B5830) with its phases 0x4B5880 /
//     0x4B58C0.
//   - The Healing Herb (kind-2 row 43, 0x4B8D70, MAGIC070.EMI on the PSX):
//     the task that walks the sparkle pool, its spawn 0x4B8E00 and end
//     0x4B8F50, and the sparkle's type-0 update 0x4B9000.
//   - The backdrop dim (kind 1, parameter 0x43, 0x4FAFF0 through FxDim_Phases
//     0x65C3A0) and its phases 0x4FB010 / 0x4FB050 / 0x4FB070.
//
// Every call goes through magic_fx_reached::g (magic_fx_reached_callees.h),
// so that the start-up fuzz can stand recorders in for them - for ours and
// for the originals' copies alike. No divergence: each is a faithful
// replacement, except that an index past one of the five stack tables aborts
// where the original would call through its own stack (the doc, section 3).
#include "game/magic_fx_reached.h"

#include "game/cheats.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_fx_reached_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace magic_fx_reached {

using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

namespace {
template <typename T> T Fn(std::uint32_t address) { return reinterpret_cast<T>(static_cast<std::uintptr_t>(address)); }
template <typename T, typename F> T As(F* f) { return reinterpret_cast<T>(reinterpret_cast<void*>(f)); }
}  // namespace

const Callees kOriginals = {
    {FxDiscFan_Start, FxDiscFan_Grow, FxDiscFan_Fade},
    {Steal_Start, Steal_Wait, Steal_Report, MagicFx_EndWhenIdle},
    {Fn<Handler>(::bof3::Code(kCloneSize)), StealClone_Run, StealClone_Finish, Fn<Handler>(::bof3::Code(kCloneFree))},
    {Sparkle_Spawn, Fn<Handler>(::bof3::Code(kSparkleTint)), Fn<Handler>(::bof3::Code(kSparkleBrighten)), Fn<Handler>(::bof3::Code(kSparkleThin)), Sparkle_End,
     Fn<Handler>(::bof3::Code(kSparkleDone))},
    {Sparkle_Launch, Sparkle_Rise, Sparkle_Fade},
    BattleActor_UpdateScreenXY,
    MagicFx_DrawDisc,
    MagicFx_PushActorMatrix,
    MagicFx_DrawFan,
    Gte_PopMatrix,
    MagicFx_DrawRing,
    BattleTask_Create,
    BattleTask_FreeCurrent,
    Battle_SetTargetFlags,
    Battle_SetTargetFlag40,
    Sound_PlayById,
    Sound_PlayEffect,
    BattleActor_SetAnimation,
    Rand,
    Inventory_Add,
    Fn<void (__cdecl*)(unsigned, unsigned)>(::bof3::Code(kItemName)),
    Msg_SystemPtr,
    BattleQueue_Push,
    Sprite_ScriptTickOnce,
    BattleActor_PlaySound,
    Sprite_UpdateScreen,
    Sparkle_Dispatch,
    Sparkle_Alloc,
    Sprite_ReleaseTint,
    BattleActor_Flash,
    Sparkle_DrawDisc,
    Sparkle_DrawRaysG2,
    Sparkle_DrawRaysG3,
    AreaMap_TintClut,
};
Callees g = kOriginals;

}  // namespace magic_fx_reached

using namespace magic_fx_reached;

namespace {

unsigned char* Ptr(std::uint32_t cell) {
    return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(magic_fx_reached::Long(magic_fx_reached::At(cell)))));
}
std::int32_t PtrValue(const unsigned char* p) {
    return static_cast<std::int32_t>(Low32(p));
}
// A task slot by its index, unchecked as the originals index it.
unsigned char* Task(unsigned index) { return magic_fx_reached::At(::bof3::Img(BattleTask_Slots_addr) + index * magic_fx_reached::at::kTaskStride); }
unsigned char Byte(std::uint32_t address) { return magic_fx_reached::At(address)[0]; }
// A .data dispatch table's entry, read as the original reads it.
Handler Entry(std::uint32_t table, unsigned index) {
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(magic_fx_reached::Long(magic_fx_reached::At(table + 4 * index)))));
}
// Sprite_Current's position copied from `from`: the actor byte +8 and the
// three dwords +0x34 / +0x38 / +0x3C, Sprite_Current read again for each.
void TakePosition(const unsigned char* from) {
    Sprite_Current[8] = from[8];
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(from)->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(from)->z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(from)->height);
}
void Bump(unsigned char& b, int by) { b = static_cast<unsigned char>(b + by); }

}  // namespace

#define MFX_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// The disc-and-fan effect and its rings

// ===========================================================================
// The steal

// original 0x4F52D0 (the last phase of the steal and of the effect at
// 0x4F50B0): once the message window is down, 0x904AA8 bit 2 and the task
// freed (a tail jmp in the original).
MFX_EXPORT void __cdecl MagicFx_EndWhenIdle(void) {
    if (BattleE1_MessageUp != 0) return;
    At(::bof3::Img(at::kFlags))[0] = static_cast<unsigned char>(At(::bof3::Img(at::kFlags))[0] | 4);
    g.free_current();
}

// ===========================================================================
// The Healing Herb's task and the sparkle update (MAGIC070.EMI)

// ===========================================================================
// The backdrop dim

// original 0x4FAFF0: the dim task (kind 1, parameter 0x43): a jmp through
// FxDim_Phases 0x65C3A0 by +1, read in place, unchecked (entry 0 is
// 0x4332E0, group CE's; the fifth word is 0).
MFX_EXPORT void __cdecl FxDim_Dispatch(void) { Entry(::bof3::Img(FxDim_Phases_addr), Sprite_Current[1])(); }

// original 0x4FB010: +9 down; at -6 the phase on; the map's CLUTs tinted by
// +9 (signed).
MFX_EXPORT void __cdecl FxDim_Down(void) {
    Bump(Sprite_Current[9], -1);
    unsigned char* const sc = Sprite_Current;
    if (sc[9] == 0xFA) Bump(sc[1], 1);
    magic_fx_reached::g.tint_clut(static_cast<signed char>(Sprite_Current[9]));
}

// original 0x4FB050: held until 0x904AA8 bit 2 (the effect is done), then
// the phase on.
MFX_EXPORT void __cdecl FxDim_Hold(void) {
    if (magic_fx_reached::At(::bof3::Img(at::kFlags))[0] & 4) Bump(Sprite_Current[1], 1);
}

// original 0x4FB070: +9 up; at 0 the task freed; the CLUTs tinted by +9
// (signed), read after the free.
MFX_EXPORT void __cdecl FxDim_Up(void) {
    Bump(Sprite_Current[9], 1);
    if (Sprite_Current[9] == 0) magic_fx_reached::g.free_current();
    magic_fx_reached::g.tint_clut(static_cast<signed char>(Sprite_Current[9]));
}

#undef MFX_EXPORT

// ===========================================================================

void MagicFxReached_Inject() {
    if (bof3::WantsShadow("magic_fx_reached")) magic_fx_reached::SelfTest();
    BOF3_INJECT(MagicFx_EndWhenIdle);
    BOF3_INJECT(FxDim_Dispatch);
    BOF3_INJECT(FxDim_Down);
    BOF3_INJECT(FxDim_Hold);
    BOF3_INJECT(FxDim_Up);
}

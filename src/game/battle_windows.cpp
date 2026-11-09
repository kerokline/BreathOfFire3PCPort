// The battle windows (the seventh round's group BC). docs/battle_windows.md.
//
// Twenty-two functions the owner's combat route reaches: the party row's
// status bars, the command cross and its icons, the command label banner,
// the three plain boxes, the enemy and member target banners - the handlers
// the window task 0x596A90 calls - with the window task's small helpers and
// the battle party's per-frame state step (0x441100 .. 0x4412B0). Each was
// read to its last instruction (2026-09-23) against its BATTLE.EMI twin where
// analysis/pairs_propagated.json has one (docs/battle_windows.md, section 1).
//
// Every call goes through battle_windows::g (battle_windows_callees.h), so
// that the start-up fuzz can stand recorders in for them - for ours and for
// the originals' copies alike. Where the original reads memory after a call,
// ours reads it after the same call.
//
// Upper halves. The originals push many values built with 8- and 16-bit
// operations, whose upper bits are whatever the register or the stack slot
// held; every callee here reads only the low 16 bits of a coordinate and the
// low byte of a colour, size or flag (each callee's read is in the fuzz's
// stand-ins), so ours passes the value with its upper bits as C++ computes
// them. The originals also park values in their own argument slots (the
// status bars' x in the first, a colour byte in the second); unobservable to
// a caller, ours uses locals.
#include "game/battle_windows.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/battle_windows_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/text_pairs.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace battle_windows {

using move_script::At;

namespace {
template <typename T> T Fn(std::uint32_t address) { return reinterpret_cast<T>(static_cast<std::uintptr_t>(address)); }
}  // namespace

const Callees kOriginals = {
    Gpu_GetTPage,
    Gpu_SetDrawMode,
    Gfx_CommitPrim,
    Gpu_SetPolyFT4,
    Text_DrawAt,
    Text_DrawFont8,
    Crt_sprintf,
    Menu_DrawIcon,
    Sprite_UpdateScreen,
    reinterpret_cast<unsigned (__cdecl*)()>(reinterpret_cast<void*>(&Sprite_ScriptTick)),
    reinterpret_cast<unsigned (__cdecl*)()>(reinterpret_cast<void*>(&Sprite_ScriptTickOnce)),
    reinterpret_cast<unsigned (__cdecl*)(unsigned)>(reinterpret_cast<void*>(&Sprite_EnsureAnimation)),
    Fn<unsigned (__cdecl*)(unsigned, std::uint32_t, unsigned)>(::bof3::Code(kPoseFrom.v)),
    Fn<void (__cdecl*)(int, int, unsigned, unsigned)>(::bof3::Code(kDrawValue.v)),
    Fn<void (__cdecl*)(int, int, unsigned, unsigned)>(::bof3::Code(kDrawEdge.v)),
    Fn<void (__cdecl*)(int, int, unsigned, unsigned)>(::bof3::Code(kDrawTile.v)),
    Fn<void (__cdecl*)(int, int, unsigned, unsigned, unsigned)>(::bof3::Code(kDrawTileRgb.v)),
    Fn<void (__cdecl*)(int, int, unsigned, unsigned, unsigned)>(::bof3::Code(kDrawBar.v)),
    Fn<void (__cdecl*)(int, int, unsigned)>(::bof3::Code(kDrawDigit.v)),
    Fn<void (__cdecl*)(int, int, int, int, unsigned, unsigned, unsigned)>(::bof3::Code(kLinePlain.v)),
    Fn<void (__cdecl*)(int, int, int, int, unsigned, unsigned, unsigned)>(::bof3::Code(kLineSemi0.v)),
    Fn<void (__cdecl*)(int, int, int, int, unsigned, unsigned, unsigned)>(::bof3::Code(kLineSemi1.v)),
    Fn<unsigned (__cdecl*)(unsigned)>(::bof3::Code(kEnemyNameShown.v)),
    Fn<const unsigned char* (__cdecl*)(int, int, unsigned, unsigned, const unsigned char*)>(::bof3::Code(kTinyFont.v)),
    reinterpret_cast<const std::uint32_t*>(static_cast<std::uintptr_t>(::bof3::Img(BattleObj_StateTable_addr))),
    {at::kKindHandlers[0], at::kKindHandlers[1], at::kKindHandlers[2]},
    BattleObj_RunState,
    BattleWin_DrawCommandIcon,
    BattleWin_DrawMessageBox,
};
Callees g = kOriginals;

}  // namespace battle_windows

using namespace battle_windows;

namespace {

using move_script::Long;
using move_script::SetWord;

unsigned char* PtrBattleWindows(std::uint32_t at) { return battle_windows::At(static_cast<std::uint32_t>(move_script::Long(battle_windows::At(at)))); }
unsigned char* CurrentBattleWindows() { return PtrBattleWindows(::bof3::Img(battle_windows::at::kCurrent.v)); }

// A party member's battle object (ObjTrio, stride 0x14C) and its words.
unsigned char* MemberBattleWindows(unsigned i) { return battle_windows::At(::bof3::Img(ObjTrio_addr) + battle_windows::at::kObjectStride * i); }

}  // namespace

// ===========================================================================
// The battle party's per-frame state step (the battle frame Battle_Frame 0x42E370 calls
// the first two; the state handlers of 0x64DFE0 the rest).

// ===========================================================================
// The window task's handlers

// ===========================================================================
// The window task's small helpers

// ===========================================================================

void BattleWindows_Inject() {
    g = kOriginals;
    if (bof3::WantsShadow("battle_windows")) SelfTest();

}

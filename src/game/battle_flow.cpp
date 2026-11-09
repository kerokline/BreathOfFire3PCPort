// The battle task and the turn flow: the phase dispatch the battle's frame
// calls (0x42E400); the 48 battle-task slots - run, create, free, clear
// (0x435110..0x435295) - and two of their kinds' helpers (the rolling digits
// 0x432F10, the magic starters 0x437780 / 0x437930); the eight enemy objects'
// state dispatch and screen update (0x435830, 0x4358A0) and their animation
// helpers (0x4358D0, 0x436090, 0x4360C0, 0x436B50); an enemy's defeat and its
// drops (0x437470, 0x437580); the magic file loaders (0x4377D0, 0x4379D0);
// the number and label draws (0x444480, 0x4445A0); and the "actor is out"
// test (0x4456C0). docs/battle_flow.md.
//
// Every call goes through battle_flow::g (battle_flow_callees.h), so that the
// start-up fuzz can stand recorders in for them - for ours and for the
// originals' copies alike. Everything here is a faithful replacement.
#include "game/battle_flow.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/battle_flow_callees.h"
#include "game/cheats.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace battle_flow {

using move_script::At;
using move_script::Long;
using move_script::SetWord;
using move_script::Word;

namespace {
template <typename T> T Fn(std::uint32_t address) { return reinterpret_cast<T>(static_cast<std::uintptr_t>(address)); }
// BattleBossFx_Dispatch is still Capcom's, so symbols.gen.h also defines its
// name as a pointer macro, which would expand inside bof3::addr:: - the
// constant is read with the macro set aside. The entry then stays the
// original's address once the function is ours too (the macro gone, the bare
// name would bind to our implementation, past BOF3X_ORIGINAL).
#pragma push_macro("BattleBossFx_Dispatch")
#undef BattleBossFx_Dispatch
constexpr ::bof3::image::CodeVa kBossFxDispatch{bof3::addr::BattleBossFx_Dispatch};
#pragma pop_macro("BattleBossFx_Dispatch")
}  // namespace

const Callees kOriginals = {
    // Rebound 2026-09-28 (round eleven's cleanup, docs/round-11-cleanup.md item 2): the
    // same values by name, every entry through bof3::addr (kBossFxDispatch above).
    {Fn<Handler>(::bof3::Code(bof3::addr::BattleStart_Dispatch)), Fn<Handler>(::bof3::Code(bof3::addr::BattleInput_Dispatch)),
     Fn<Handler>(::bof3::Code(bof3::addr::BattleCommit_Dispatch)), Fn<Handler>(::bof3::Code(bof3::addr::Battle_ActionPhase)),
     Fn<Handler>(::bof3::Code(bof3::addr::BattleRoundEnd_Step)), Fn<Handler>(::bof3::Code(bof3::addr::BattleEnd_Step))},
    {Fn<Handler>(::bof3::Code(bof3::addr::BattleFx_Dispatch)), Fn<Handler>(::bof3::Code(bof3::addr::BattleMagicFx_Dispatch)),
     Fn<Handler>(::bof3::Code(bof3::addr::BattleMagicRow_Run)), Fn<Handler>(::bof3::Code(kBossFxDispatch.v))},
    BattleTask_Create, Battle_RollDrops, Battle_DrawNumber,
    Sprite_UpdateScreen, Sprite_EnsureAnimation, Sprite_ScriptTick, Sprite_ScriptTickOnce, Rand,
    Fn<void (__cdecl*)(unsigned)>(::bof3::Code(kRemoveFromTurnOrder.v)), Fn<void (__cdecl*)(unsigned)>(::bof3::Code(kSetFlagBit.v)),
    Sprite_ReleaseTint, Fn<void (__cdecl*)(unsigned)>(::bof3::Code(kClearTurnBit.v)), LoadDatFile, Crt_sprintf,
    Gpu_GetTPage, Gpu_SetDrawMode, Gfx_CommitPrim, Gpu_GetClut, Gpu_SetSprt,
};
Callees g = kOriginals;

}  // namespace battle_flow

using namespace battle_flow;

namespace {

// A pointer the original keeps in a dword of .data / .bss, read afresh.
unsigned char* PtrAtBattleFlow(std::uint32_t address) {
    return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(battle_flow::Long(battle_flow::At(address)))));
}
unsigned char* CurrentTask() { return PtrAtBattleFlow(::bof3::Img(BattleFlow_TaskCurrent_addr)); }
unsigned char* Task(unsigned index) { return battle_flow::At(::bof3::Img(BattleTask_Slots_addr) + index * battle_flow::at::kTaskSize); }
unsigned char* EnemyObject(unsigned index) { return battle_flow::At(::bof3::Img(Battle_Enemies_addr) + index * battle_flow::at::kEnemySize); }

// The magic file of a row: the u16 at the head of its 8-byte record; 0xFFFF
// is none. A file asked for sets bit 2 of 0x904AA9 (the round flags' 0x400).
void LoadMagicRow(unsigned row) {
    const unsigned file = battle_flow::Word(battle_flow::At(::bof3::Img(Magic_Rows_addr) + row * 8));
    if (file == 0xFFFF) return;
    battle_flow::g.load_dat(static_cast<int>(file));
    battle_flow::At(::bof3::Img(battle_flow::at::kLoadFlags.v))[0] = static_cast<unsigned char>(battle_flow::At(::bof3::Img(battle_flow::at::kLoadFlags.v))[0] | 4);
}

// The event hook 0x904B6C and the enemies' +0xF4 hooks.
using HookFn = void (__cdecl*)(int);

}  // namespace

// ===========================================================================
// The battle's frame and the task slots
// ===========================================================================

// ===========================================================================
// The enemy objects
// ===========================================================================

// ===========================================================================
// A kill
// ===========================================================================

// ===========================================================================
// Magic: the task and the file
// ===========================================================================

// ===========================================================================
// Digits and labels
// ===========================================================================

// ===========================================================================

// ===========================================================================

void BattleFlow_Inject() {
    if (bof3::WantsShadow("battle_flow")) battle_flow::SelfTest();
}

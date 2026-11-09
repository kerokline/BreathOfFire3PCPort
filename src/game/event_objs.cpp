// The event script's object ops - see docs/event-objs.md for each function,
// its PSX twin (GAME.EMI section 0) and the fuzz (event_objs_fuzz.cpp).
//
// "SC" is Sprite_Current, read afresh at every use where the original reads
// [0x937F88] again (a callee may move it), and held where the original holds
// it in a register. Every callee is reached through event_objs::g, so that
// the start-up fuzz can stand recorders in for them.
#include "game/event_objs.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/event_objs_callees.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"

namespace event_objs {

namespace {
template <typename F>
F Fn(std::uint32_t address) { return reinterpret_cast<F>(static_cast<std::uintptr_t>(address)); }
}  // namespace

const Callees kOriginals = {
    Field_JumpSetUp,
    Field_JumpCamera,
    Field_JumpStart,
    Fn<long (__cdecl*)(long, long, unsigned)>(::bof3::Code(kSlopeAt.v)),
    MapView_GroundAt,
    Sprite_ShadeRaise,
    Sprite_LoadPalette,
    AreaMap_CellsAll,
    AreaMap_CellsAll4,
    Fn<unsigned char (__cdecl*)(long, long, unsigned, unsigned)>(::bof3::Code(kCellsAllWide.v)),
    AreaMap_CellsNone,
    AreaMap_CellsNone4,
    Fn<unsigned char (__cdecl*)(long, long, unsigned, unsigned)>(::bof3::Code(kCellsNoneWide.v)),
    AreaMap_ByteAt,
    Fn<void (__cdecl*)(unsigned)>(::bof3::Code(kFloorHurt.v)),
    Fn<void (__cdecl*)(unsigned)>(::bof3::Code(kFlash.v)),
    Party_Count,
    Fn<unsigned (__cdecl*)(unsigned, unsigned)>(::bof3::Code(kHpLose.v)),
    Fn<void (__cdecl*)(unsigned, unsigned)>(::bof3::Code(kHpGain.v)),
    Effect_FindFree,
    Field_TileTurn,
    Fn<unsigned char (__cdecl*)(long, long, unsigned, unsigned)>(::bof3::Code(kTurnProbe.v)),
    Actor_EquipCount,
};
Callees g = kOriginals;

}  // namespace event_objs

namespace {

using namespace event_objs;

}  // namespace

// --- the shade fade -----------------------------------------------------------

// --- the jump -------------------------------------------------------------------

// --- the tiles under the leader -------------------------------------------------

// --- the cells ----------------------------------------------------------------

// --- the actor timers and the equipment --------------------------------------

void EventObjs_Inject() {
    if (bof3::WantsShadow("event_objs")) event_objs::SelfTest();
}

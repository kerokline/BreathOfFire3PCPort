// Moved here by tools/move_funcs.py from area_w0a.cpp (docs/source-layout-round.md).
#include "game/area/area_15.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w0a_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w0a::at;

}  // namespace

namespace {

using area_harness::Mem;

using move_script::Long;

using move_script::Word;

// The script position of MoveScript_Object (u16 +0xA), moved as the
// original's 16-bit add.
void MoveScriptPos(unsigned delta) {
    unsigned char* const object = MoveScript_Object;
    rv::Store<unsigned>(rv::As<records::ScriptContext>(object)->position, rv::As<records::ScriptContext>(object)->position + delta);
}

// Area 15's state dispatch: jmp [table + Sprite_Current[4] * 4], unchecked
// in the original; two entries, so ours aborts past them.
void RunState(std::uint32_t table, const char* who) {
    const unsigned state = Sprite_Current[4];
    if (state >= at::kArea15States)
        bof3::Fatal("%s: Sprite_Current[4] is %u, past the two-entry state table 0x%X (the original jumps into data)", who,
                    state, (unsigned)table);
    reinterpret_cast<area_harness::Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(table + 4 * state)))))();
}

// Area 15's first state of each table: the pose pair by frame parity, a
// countdown of Rand & 7, state 1, the script op repeated.
void Area15Pose(std::uint32_t poses) {
    const unsigned parity = (Frame_Counter >> 1) & 1u;
    const int r = AH_CALL(Rand)();
    Sprite_Current[0xA] = static_cast<unsigned char>(r & 7);
    AH_CALL(Sprite_EnsureAnimation)(Mem(poses + parity * 2)[0]);
    Sprite_Current[0x2A] = Mem(poses + parity * 2 + 1)[0];
    Sprite_Current[4] = 1;
    MoveScriptPos(0xFFFE);
}

}  // namespace

// original 0x4019B0 (area 15's init, the descriptor's +0x40; PSX 0x801F2C04):
// the zone byte (+4) of the area's zone records 1 and 2 (the list at
// [0x668D80 + Game_AreaNumber * 4], read for each) is 2 with bit 0 of
// Field_StatusBits (MoveScript_Variable's variable 9), else 0.
extern "C" void __cdecl Area15_SetZones(void) {
    const unsigned char zone = (Field_StatusBits & 1) ? 2 : 0;
    auto list = [] {
        return Mem(static_cast<std::uint32_t>(Long(Mem(::bof3::Img(Area_StateRecords_addr) + static_cast<std::uint32_t>(Game_AreaNumber) * 4u))));
    };
    list()[0xC] = zone;
    list()[0x14] = zone;
}

// original 0x401A00 (area 15's handler 0; PSX 0x801F2CF8): Draw_PassFlags 0x1F.
extern "C" void __cdecl Area15_PassFlags1F(void) { Draw_PassFlags = 0x1F; }

// original 0x401A10 / 0x401AF0 (area 15's handlers 4 / 5; PSX 0x801F2D78 /
// 0x801F2EE8): jmp [Area15_StatesA / B + Sprite_Current[4] * 4].
extern "C" void __cdecl Area15_RunStatesA(void) { RunState(::bof3::Img(Area15_StatesA_addr), "Area15_RunStatesA"); }

// original 0x401A30 / 0x401B10 (Area15_StatesA / B entry 0): the pose of
// Area15_PosesA / B by bit 1 of Frame_Counter (read before Rand), a
// countdown Sprite_Current[0xA] = Rand & 7, state 1, the script position 2
// back. Sprite_Current read after each call, MoveScript_Object at the end.
extern "C" void __cdecl Area15_StateA0(void) { Area15Pose(::bof3::Img(Area15_PosesA_addr)); }

// original 0x401AA0 (entry 1 of both state tables): the countdown
// Sprite_Current[0xA] one less; at 0 before it, state 0. Then with the
// scene's timer 0x8034E6 at 0, state 0 and the countdown 0; else the script
// position 2 back.
extern "C" void __cdecl Area15_StateCount(void) {
    const unsigned char count = Sprite_Current[0xA];
    Sprite_Current[0xA] = static_cast<unsigned char>(count - 1);
    if (count == 0) Sprite_Current[4] = 0;
    if (Word(Mem(::bof3::Img(MoveScript_PartyRecords_addr) + 0x66)) == 0) {
        Sprite_Current[4] = 0;
        Sprite_Current[0xA] = 0;
        return;
    }
    MoveScriptPos(0xFFFE);
}

extern "C" void __cdecl Area15_RunStatesB(void) { RunState(::bof3::Img(Area15_StatesB_addr), "Area15_RunStatesB"); }

extern "C" void __cdecl Area15_StateB0(void) { Area15Pose(::bof3::Img(Area15_PosesB_addr)); }

void Area15_Inject() {
    BOF3_INJECT(Area15_SetZones);
    BOF3_INJECT(Area15_PassFlags1F);
    BOF3_INJECT(Area15_RunStatesA);
    BOF3_INJECT(Area15_StateA0);
    BOF3_INJECT(Area15_StateCount);
    BOF3_INJECT(Area15_RunStatesB);
    BOF3_INJECT(Area15_StateB0);
}

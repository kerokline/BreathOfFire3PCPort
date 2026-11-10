// Moved here by tools/move_funcs.py from area_w1f.cpp (docs/source-layout-round.md).
#include "game/area/area_69.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w1f_callees.h"
#include "game/cheats.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace {

namespace at = area_w1f::at;

}  // namespace

namespace {

using area_harness::Mem;

using move_script::Long;

using move_script::SetWord;

using move_script::Word;

unsigned char& B(std::uint32_t address) { return *Mem(address); }

void AddWord(unsigned char* p, unsigned v) { SetWord(p, Word(p) + v); }

// A two-entry .data state table read in place by the running object's +4 and
// jumped through (area 68's handlers 2 and 3, 69's 0, 75's 7 and 11). The
// original indexes it unchecked: a state of 2 or more jumps through the bytes
// after the table - ours aborts there.
void RunState(const char* who, std::uint32_t table) {
    const unsigned state = Sprite_Current[4];
    if (state >= 2)
        bof3::Fatal("%s: the running object's state %u is past its 2-entry table 0x%X (the original jumps through the "
                    "bytes after it)",
                    who, state, static_cast<unsigned>(table));
    reinterpret_cast<area_harness::Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(table + state * 4)))))();
}

// The glides' state 0 (0x40D090, 0x40D380): the running object's count +0xA =
// `count`, its state 1, and the leader's script word +0x12E (through
// Field_State) back by 2.
void GlideBegin(unsigned char count) {
    Sprite_Current[0xA] = count;
    Sprite_Current[4] = 1;
    AddWord(Field_State + 0x12E, 0xFFFE);
}

// The glides' state 1 (0x40D0C0, 0x40D150, 0x40D3B0): with the count +0xA at 0
// the state goes back to 0; else x +0x34 moves by steps[count & 0xF] << 11 and
// z +0x38 by -steps[count & 0xF] << 11 (signed bytes), the count goes down by
// one and the script word +0x12E back by 2.
void GlideStep(std::uint32_t steps) {
    unsigned char* const cur = Sprite_Current;
    auto* const tCur = rv::As<records::SpriteObject>(cur);
    const unsigned char count = cur[0xA];
    if (count == 0) {
        cur[4] = 0;
        return;
    }
    const auto dx = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<signed char>(B(steps + (count & 0xF)))));
    tCur->x = static_cast<std::int32_t>(static_cast<std::uint32_t>(tCur->x) + (dx << 11));
    const auto dz = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<signed char>(B(steps + (cur[0xA] & 0xF)))));
    tCur->z = static_cast<std::int32_t>(static_cast<std::uint32_t>(tCur->z) + ((0u - dz) << 11));
    cur[0xA] = static_cast<unsigned char>(cur[0xA] - 1);
    AddWord(Field_State + 0x12E, 0xFFFE);
}

}  // namespace

// original 0x40D360 (Area69_Handlers[0]; PSX 0x801F2C04): jmp through
// Area69_GlideStates by the running object's +4.
extern "C" void __cdecl Area69_GlideRun(void) { RunState("Area69_GlideRun", ::bof3::Img(Area69_GlideStates_addr)); }

// original 0x40D380 (Area69_GlideStates[0]; also area 79's state table
// 0x60EC80 entry 0): GlideBegin(0x40).
extern "C" void __cdecl Area69_GlideBegin40(void) { GlideBegin(0x40); }

// original 0x40D3B0 (Area69_GlideStates[1]): GlideStep over Area69_GlideSteps.
extern "C" void __cdecl Area69_GlideStep(void) { GlideStep(::bof3::Img(Area69_GlideSteps_addr)); }

void Area69_Inject() {
    BOF3_INJECT(Area69_GlideRun);
    BOF3_INJECT(Area69_GlideBegin40);
    BOF3_INJECT(Area69_GlideStep);
}

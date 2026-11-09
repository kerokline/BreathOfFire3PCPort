// Moved here by tools/move_funcs.py from area_w3e.cpp (docs/source-layout-round.md).
#include "game/area/area_139.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w3e.h"
#include "game/area_w3e_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w3e::at;

}  // namespace

namespace {

using area_harness::Mem;

using U = std::uint32_t;

unsigned char& B(U address) { return *Mem(address); }

unsigned char* StoryFlags() { return Mem(::bof3::Img(Cond_Flags_addr) + 0xa0); }

// The first of `count` entries of `stride` bytes at `table` whose x byte is
// x's low byte, whose z byte is z's, and whose direction (the third byte's low
// nibble) is the leader's direction byte (the whole byte: 0x10 or more matches
// none); `count` when none - the search the three cell hooks open with.
unsigned FindCell(U table, unsigned count, unsigned stride, long x, long z) {
    const unsigned char pose = B(::bof3::Img(ObjTrio_addr) + 0x8);
    for (unsigned i = 0; i < count; ++i) {
        const unsigned char* const e = Mem(table + i * stride);
        if (e[0] == static_cast<unsigned char>(x) && e[1] == static_cast<unsigned char>(z) && (e[2] & 0xF) == pose) return i;
    }
    return count;
}

}  // namespace

// original 0x41F510 (area 139 +0x3C[0]; PSX 0x801F42F4): the active member's
// +0x80 bit 0 cleared; the leader's +0x89 at 5: story flag 0x4D,
// MoveCmd_TestFB(4, 0xD), Sprite_Current's +0 = 0 (read after the calls).
extern "C" void __cdecl Area139_FlagIfLeader89Is5(void) {
    unsigned char* const member = Field_ActiveMember;
    member[0x80] = static_cast<unsigned char>(member[0x80] & 0xFE);
    if (B(::bof3::Img(ObjTrio_addr) + 0x89) != 5) return;
    AH_CALL(Flags_Set)(StoryFlags(), 0x4D);
    AH_CALL(MoveCmd_TestFB)(4, 0xD);
    Sprite_Current[0] = 0;
}

// original 0x41F550 (Area_CellHooks, area 0x8B): FindCell over
// Area139_CellEntries; a match: Flags_Toggle(story, its flag byte),
// Sound_PlayEffect(0x204), al 1; none: al 0.
extern "C" unsigned char __cdecl Area139_CellHook(long x, long z) {
    const unsigned i = FindCell(::bof3::Img(Area139_CellEntries_addr), at::kArea139CellCount, 4, x, z);
    if (i == at::kArea139CellCount) return 0;
    AH_CALL(Flags_Toggle)(StoryFlags(), B(::bof3::Img(Area139_CellEntries_addr) + i * 4 + 3));
    AH_CALL(Sound_PlayEffect)(0x204);
    return 1;
}

void Area139_Inject() {
    BOF3_INJECT(Area139_FlagIfLeader89Is5);
    BOF3_INJECT(Area139_CellHook);
}

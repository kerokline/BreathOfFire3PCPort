// Moved here by tools/move_funcs.py from area_w2a.cpp (docs/source-layout-round.md).
#include "game/area/area_84.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w2a.h"
#include "game/area_w2a_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w2a::at;

}  // namespace

namespace {

using area_harness::Mem;

unsigned char& B(std::uint32_t address) { return *Mem(address); }

}  // namespace

// original 0x40F6E0 (Area84_Handlers[0]; PSX 0x801F2C04): the active
// member's +0x80 bit 0 cleared; with the leader's byte +0x89 at 5, story flag
// 0x31 set, MoveCmd_TestFB(9, 0x10) (its answer unread) and the running
// object's byte +0 cleared.
extern "C" void __cdecl Area84_SetFlag31At5(void) {
    unsigned char* const member = Field_ActiveMember;
    member[0x80] = static_cast<unsigned char>(member[0x80] & 0xFE);
    if (B(::bof3::Img(ObjTrio_addr) + 0x89) != 5) return;
    AH_CALL(Flags_Set)(Mem(::bof3::Img(Cond_Flags_addr) + 0xa0), 0x31);
    AH_CALL(MoveCmd_TestFB)(9, 0x10);
    Sprite_Current[0] = 0;
}

void Area84_Inject() {
    BOF3_INJECT(Area84_SetFlag31At5);
}

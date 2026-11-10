// Moved here by tools/move_funcs.py from area_w3c.cpp (docs/source-layout-round.md).
#include "game/area/area_127.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w3c.h"
#include "game/area_w3c_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w3c::at;

}  // namespace

namespace {

using area_harness::Mem;

using move_script::Long;

using U = std::uint32_t;

unsigned char& B(U address) { return *Mem(address); }

// The dword at `cell` as a pointer.
unsigned char* Ptr(U cell) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(cell))))); }

unsigned char* StoryFlags() { return Mem(::bof3::Img(Cond_Flags_addr) + 0xa0); }

}  // namespace

// original 0x41CA30 (+0x3C[0], PSX 0x801F2C04): Field_ActiveMember's byte
// +0x80 bit 0 cleared; the leader's byte +0x89 at 5: story flag 0x32,
// MoveCmd_TestFB(0x4E, 0x33), then Sprite_Current (read after the calls) +0 =
// 0.
extern "C" void __cdecl Area127_ClearActive80(void) {
    unsigned char* const member = Ptr(::bof3::Img(Field_ActiveMember_addr));
    member[0x80] = static_cast<unsigned char>(member[0x80] & 0xFE);
    if (B(::bof3::Img(ObjTrio_addr) + 0x89) != 5) return;
    AH_CALL(Flags_Set)(StoryFlags(), 0x32);
    AH_CALL(MoveCmd_TestFB)(0x4E, 0x33);
    Sprite_Current[0] = 0;
}

void Area127_Inject() {
    BOF3_INJECT(Area127_ClearActive80);
}

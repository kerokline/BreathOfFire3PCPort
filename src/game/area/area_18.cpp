// Moved here by tools/move_funcs.py from area_w0b.cpp (docs/source-layout-round.md).
#include "game/area/area_18.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w0b.h"
#include "game/area_w0b_callees.h"
#include "game/event_script.h"   // DIV-0191: g_mountain_passes
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/world_map.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w0b::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

unsigned char* Pointer(U cell) { return At(static_cast<U>(Long(At(cell)))); }

}  // namespace

// original 0x402CA0 (area 18's init, the descriptor 0x5E6688's +0x40; PSX
// 0x801F2C04): the byte +4 of the record 0x668D80[Game_AreaNumber] points at
// is 0 when Cond_ByteFA is 8, else 2.
extern "C" void __cdecl Area18_SetRecordByte(void) {
    const unsigned value = Cond_ByteFA == 8 ? 0u : 2u;
    Pointer(::bof3::Img(at::kAreaRecords.v) + Game_AreaNumber * 4u)[4] = static_cast<unsigned char>(value);
}

// original 0x402CE0 (area 18's handler 0, Area18_Handlers 0x5E6680; PSX
// 0x801F2C68): unless the byte +0x89 of Field_State's record is 2, the word
// +0xA of MoveScript_Object (its script position) + 9.
extern "C" void __cdecl Area18_SkipScript(void) {
    if (Field_State[0x89] == 2) return;
    unsigned char* const object = MoveScript_Object;
    rv::Store<unsigned>(rv::As<records::ScriptContext>(object)->position, rv::As<records::ScriptContext>(object)->position + 9u);
}

// original 0x402D00 (area 18's handler 1 and area 19's handler 6: one body;
// PSX 0x801F2CA8): eight map cells to 0 through AreaMap_SetByte - x 0x1F and
// 0x20 of rows 0x4C, 0x4D, 3 and 2, in that order.
extern "C" void __cdecl Area18_ClearCells(void) {
    static constexpr unsigned char kRows[] = {0x4C, 0x4D, 3, 2};
    for (const unsigned char z : kRows) {
        AH_CALL(AreaMap_SetByte)(0x1F, z, 0);
        AH_CALL(AreaMap_SetByte)(0x20, z, 0);
    }
}

void Area18_Inject() {
    BOF3_INJECT(Area18_SetRecordByte);
    BOF3_INJECT(Area18_SkipScript);
    BOF3_INJECT(Area18_ClearCells);
}

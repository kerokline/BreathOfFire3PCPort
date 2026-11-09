// Moved here by tools/move_funcs.py from area_w0a.cpp (docs/source-layout-round.md).
#include "game/area/area_12.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w0a.h"
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

// The script position of MoveScript_Object (u16 +0xA), moved as the
// original's 16-bit add.
void MoveScriptPos(unsigned delta) {
    unsigned char* const object = MoveScript_Object;
    rv::Store<unsigned>(rv::As<records::ScriptContext>(object)->position, rv::As<records::ScriptContext>(object)->position + delta);
}

}  // namespace

// original 0x401840 (area 12's handler 0; PSX 0x801F2C04): until flag 0xE of
// the bank at [0x929ED0] is set, the script position 2 back (the op runs
// again), MoveScript_Object read after the call.
extern "C" void __cdecl Area12_WaitFlagE(void) {
    const auto bank = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(::bof3::Img(Chapter_FlagRow_addr))))));
    if (AH_CALL(Flags_Test)(bank, 0xE)) return;
    MoveScriptPos(0xFFFE);
}

void Area12_Inject() {
    BOF3_INJECT(Area12_WaitFlagE);
}

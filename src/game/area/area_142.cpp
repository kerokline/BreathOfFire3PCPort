// Moved here by tools/move_funcs.py from area_w3e.cpp (docs/source-layout-round.md).
#include "game/area/area_142.h"

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

using area_harness::Handler;

using area_harness::Mem;

using move_script::Long;

using move_script::SetWord;

using move_script::Word;

using U = std::uint32_t;

unsigned char& B(U address) { return *Mem(address); }

void AddWord(unsigned char* p, int v) { SetWord(p, static_cast<unsigned>(Word(p) + v)); }

// The movement script's position (MoveScript_Object's word +0xA) moved by `v`.
void ScriptStep(int v) { AddWord(MoveScript_Object + 0xA, v); }

// A state handler read from an area's .data table in place, as the
// originals' `jmp [index * 4 + table]`: the index is not checked there. Ours
// aborts past the table (what follows is data or the next table).
Handler StateEntry(const char* who, U table, unsigned count, unsigned index) {
    if (index >= count)
        bof3::Fatal("%s: state %u is past its %u-entry table 0x%X", who, index, count, static_cast<unsigned>(table));
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(table + index * 4u)))));
}

}  // namespace

// original 0x420760 (area 142 +0x3C[0]; PSX 0x801F3414): Area142_States by
// Sprite_Current[4].
extern "C" void __cdecl Area142_RunWait(void) {
    StateEntry("Area142_RunWait", ::bof3::Img(Area142_States_addr), at::kArea142StateCount, Sprite_Current[4])();
}

// original 0x420780 (Area142_States[0]): counter 0 at 1: nothing. Else
// Field_Request 3: +0 bit 0x40, +4 = 1; the script position - 2.
extern "C" void __cdecl Area142_WaitRequest3(void) {
    if (B(::bof3::Img(at::kCounter0.v)) == 1) return;
    if (Field_Request == 3) {
        Sprite_Current[0] = static_cast<unsigned char>(Sprite_Current[0] | 0x40);
        Sprite_Current[4] = 1;
    }
    ScriptStep(-2);
}

// original 0x4207C0 (Area142_States[1]): Field_Request not 3: +0 bit 0x40
// cleared, Sprite_SetAnimation(+8), +4 = 0 (the pointer read again after the
// call). The script position - 2.
extern "C" void __cdecl Area142_WaitRequestEnd(void) {
    if (Field_Request != 3) {
        Sprite_Current[0] = static_cast<unsigned char>(Sprite_Current[0] & 0xBF);
        AH_CALL(Sprite_SetAnimation)(Sprite_Current[8]);
        Sprite_Current[4] = 0;
    }
    ScriptStep(-2);
}

void Area142_Inject() {
    BOF3_INJECT(Area142_RunWait);
    BOF3_INJECT(Area142_WaitRequest3);
    BOF3_INJECT(Area142_WaitRequestEnd);
}

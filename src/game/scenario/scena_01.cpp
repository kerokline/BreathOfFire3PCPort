// Moved here by tools/move_funcs.py from scena_calls.cpp (docs/source-layout-round.md).
#include "game/scenario/scena_01.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "common/asm_name.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/scena_calls.h"
#include "game/scena_calls_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = scena_calls::at;

}  // namespace

namespace {

using move_script::At;

void Set(unsigned a, unsigned b, unsigned c) { SH_CALL(PartySet_Load)(a, b, c, 0); }

void Join(unsigned id) { SH_CALL(Party_Join)(id); }

void Leave(unsigned id) { SH_AT(void (__cdecl*)(unsigned), at::kLeave.v)(id); }

void Palettes() { SH_AT(void (__cdecl*)(), at::kPalettes.v)(); }

// The character record of Character_IndexById[k] (CharacterRecords, 0xA4
// each; the byte found unchecked), the index read at the call.
unsigned char* RecordOf(unsigned k) {
    return At(::bof3::Img(CharacterRecords_addr) + Character_IndexById[k] * at::kRecordStride);
}

}  // namespace

// original 0x5198F0 (0x49 bytes; 1 A[0]): the record of Character_IndexById[10]
// (Char_WhelpSlot) loses +0xB bit 0 before the load; the record of
// Character_IndexById[9], read after it, gets +9 = 9.
extern "C" void __cdecl Scena01_Set934Party9(void) {
    unsigned char* const out = RecordOf(10);
    out[0xB] = static_cast<unsigned char>(out[0xB] & 0xFE);
    Set(9, 3, 4);
    Field_MemberCount = 0;
    RecordOf(9)[9] = 9;
    Join(9);
}

// original 0x519940 (0x42 bytes; 1 A[1]): the record of Character_IndexById[0]
// gets +9 = 0 before the load.
extern "C" void __cdecl Scena01_Party034(void) {
    RecordOf(0)[9] = 0;
    Set(0, 3, 4);
    Field_MemberCount = 0;
    Join(0);
    Join(3);
    Join(4);
}

// original 0x519990 (0x9 bytes; 1 A[2], 2 A[0]): Party_Join(4).
extern "C" void __cdecl ScenaCall_Join4(void) {
    Join(4);
}

// original 0x5199A0 (0xF bytes; 1 B[0], 2 B[0], 9 B[0], 11 B[0], 12 B[0]): 0x534030(4); 0x533E00.
extern "C" void __cdecl ScenaCall_Leave4(void) {
    Leave(4);
    Palettes();
}

void Scena01_Inject() {
    BOF3_INJECT(Scena01_Set934Party9);
    BOF3_INJECT(Scena01_Party034);
    BOF3_INJECT(ScenaCall_Join4);
    BOF3_INJECT(ScenaCall_Leave4);
}

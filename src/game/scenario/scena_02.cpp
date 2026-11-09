// Moved here by tools/move_funcs.py from scena_calls.cpp (docs/source-layout-round.md).
#include "game/scenario/scena_02.h"

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

void Leave(unsigned id) { SH_AT(void (__cdecl*)(unsigned), at::kLeave.v)(id); }

void Palettes() { SH_AT(void (__cdecl*)(), at::kPalettes.v)(); }

// The character record of Character_IndexById[k] (CharacterRecords, 0xA4
// each; the byte found unchecked), the index read at the call.
unsigned char* RecordOf(unsigned k) {
    return At(::bof3::Img(CharacterRecords_addr) + Character_IndexById[k] * at::kRecordStride);
}

}  // namespace

// original 0x5199B0 (0x49 bytes; 2 B[1]): the records of Character_IndexById[4]
// and [3] lose +0xB bit 0, then members 4 and 3 leave; tail 0x533E00.
extern "C" void __cdecl Scena02_Leave43(void) {
    unsigned char* const a = RecordOf(4);
    a[0xB] = static_cast<unsigned char>(a[0xB] & 0xFE);
    unsigned char* const b = RecordOf(3);
    b[0xB] = static_cast<unsigned char>(b[0xB] & 0xFE);
    Leave(4);
    Leave(3);
    Palettes();
}

void Scena02_Inject() {
    BOF3_INJECT(Scena02_Leave43);
}

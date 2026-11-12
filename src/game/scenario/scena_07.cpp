// Moved here by tools/move_funcs.py from scena_calls.cpp (docs/source-layout-round.md).
#include "game/scenario/scena_07.h"

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

unsigned char MemberId(unsigned i) { return At(::bof3::Img(ObjTrio_addr) + 0x89 + i * at::kMemberStride)[0]; }

// The character record of Character_IndexById[k] (CharacterRecords, 0xA4
// each; the byte found unchecked), the index read at the call.
unsigned char* RecordOf(unsigned k) {
    return At(::bof3::Img(CharacterRecords_addr) + Character_IndexById[k] * at::kRecordStride);
}

}  // namespace

// original 0x519DE0 (0x2D bytes; 7 A[1]): PartySet_Load(0, 1, 2, 0); Field_MemberCount 0; Party_Join(0); Party_Join(1); Party_Join(2).
extern "C" void __cdecl Scena07_Party012(void) {
    Set(0, 1, 2);
    Field_MemberCount = 0;
    Join(0);
    Join(1);
    Join(2);
}

// original 0x519E10 (0xD bytes; 7 B[0]): the third field member leaves (its
// +0x89, 0x803061).
extern "C" void __cdecl Scena07_LeaveThird(void) { Leave(MemberId(2)); }

// original 0x519E20 (0x78 bytes; 7 B[1]): member 2 leaves; then the records of
// Character_IndexById[1], [5], [6] and [2] lose +0xB bit 0, each index read
// after the call. No palettes.
extern "C" void __cdecl Scena07_Leave2Out1562(void) {
    Leave(2);
    static const unsigned kOut[] = {1, 5, 6, 2};
    for (unsigned k : kOut) {
        unsigned char* const r = RecordOf(k);
        r[0xB] = static_cast<unsigned char>(r[0xB] & 0xFE);
    }
}

void Scena07_Inject() {
    BOF3_INJECT(Scena07_Party012);
    BOF3_INJECT(Scena07_LeaveThird);
    BOF3_INJECT(Scena07_Leave2Out1562);
}

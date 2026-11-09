// Moved here by tools/move_funcs.py from scena_calls.cpp (docs/source-layout-round.md).
#include "game/scenario/scena_10_2.h"

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

unsigned char MemberId(unsigned i) { return At(::bof3::Img(ObjTrio_addr) + 0x89 + i * at::kMemberStride)[0]; }

// The character record of Character_IndexById[k] (CharacterRecords, 0xA4
// each; the byte found unchecked), the index read at the call.
unsigned char* RecordOf(unsigned k) {
    return At(::bof3::Img(CharacterRecords_addr) + Character_IndexById[k] * at::kRecordStride);
}

}  // namespace

// original 0x51A430 (0x55 bytes; 10 A[4]): the saved party (0x903A10..12)
// loaded, and each of the three that is not 7 joins. No count reset, no
// palettes.
//
// As the original has it: the three are passed as dwords read across a
// 6-byte local (their upper bytes are the neighbours and stale stack), and
// the joins' ids are al over the last call's eax; the callees read the byte.
extern "C" void __cdecl Scena10_RestoreSaved(void) {
    const unsigned char* const saved = At(::bof3::Img(at::kSaved));
    const unsigned char s[3] = {saved[0], saved[1], saved[2]};
    Set(s[0], s[1], s[2]);
    for (unsigned char id : s)
        if (id != 7) Join(id);
}

// original 0x51A490 (0x24 bytes; 10 B[0]): the record of
// Character_IndexById[5] loses +0xB bit 0; member 5 leaves; tail 0x533E00.
extern "C" void __cdecl Scena10_Leave5(void) {
    unsigned char* const r = RecordOf(5);
    r[0xB] = static_cast<unsigned char>(r[0xB] & 0xFE);
    Leave(5);
    Palettes();
}

// original 0x51A4C0 (0x43 bytes; 10 B[2]): the three field members' ids copied
// first; each that is not 7 leaves; 0x533E00.
extern "C" void __cdecl Scena10_LeaveAll(void) {
    const unsigned char ids[3] = {MemberId(0), MemberId(1), MemberId(2)};
    for (unsigned char id : ids)
        if (id != 7) Leave(id);
    Palettes();
}

void Scena102_Inject() {
    BOF3_INJECT(Scena10_RestoreSaved);
    BOF3_INJECT(Scena10_Leave5);
    BOF3_INJECT(Scena10_LeaveAll);
}

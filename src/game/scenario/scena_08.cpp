// Moved here by tools/move_funcs.py from scena_calls.cpp, scena_se.cpp (docs/source-layout-round.md).
#include "game/scenario/scena_08.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "common/asm_name.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scena_calls.h"
#include "game/scena_calls_callees.h"
#include "game/scena_se_callees.h"
#include "game/scenario/scena_08_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = scena_08::at;

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

// original 0x519EA0 (0x23 bytes; 8 A[0]): PartySet_Load(7, 2, 0xA, 0); Field_MemberCount 0; Party_Join(0xA); 0x533E00.
extern "C" void __cdecl Scena08_Set72APartyA(void) {
    Set(7, 2, 0xA);
    Field_MemberCount = 0;
    Join(0xA);
    Palettes();
}

// original 0x519ED0 (0x25 bytes; 8 A[1]): the party emptied, the record of
// Character_IndexById[10] (Char_WhelpSlot) loses +0xB bit 0, member 7 joins.
extern "C" void __cdecl Scena08_Party7(void) {
    unsigned char* const r = RecordOf(10);
    Field_MemberCount = 0;
    r[0xB] = static_cast<unsigned char>(r[0xB] & 0xFE);
    Join(7);
}

// original 0x519F00 (0x2A bytes; 8 A[3]): PartySet_Load(7, 2, 4, 0); Field_MemberCount 0; Party_Join(7); Party_Join(2); 0x533E00.
extern "C" void __cdecl Scena08_Set724Party72(void) {
    Set(7, 2, 4);
    Field_MemberCount = 0;
    Join(7);
    Join(2);
    Palettes();
}

// original 0x519F30 (0x31 bytes; 8 A[4], 14 A[7]): PartySet_Load(7, 2, 8, 0); Field_MemberCount 0; Party_Join(7); Party_Join(2); Party_Join(8); 0x533E00.
extern "C" void __cdecl ScenaCall_Party728(void) {
    Set(7, 2, 8);
    Field_MemberCount = 0;
    Join(7);
    Join(2);
    Join(8);
    Palettes();
}

// original 0x519F70: chapter 8's call table A entry 5, and the tail of entry
// 6 (0x519FA0, which loads a party set first and jumps here). The member
// count to 0; Party_Join(7), (8), (4), their answers unread; then 0x533E00,
// the members' palettes reloaded - a tail jmp in the original, whose eax no
// caller reads (Scenario_CallA is void).
extern "C" void __cdecl Scena08_PartyJoin784(void) {
    Field_MemberCount = 0;
    SH_CALL(Party_Join)(7);
    SH_CALL(Party_Join)(8);
    SH_CALL(Party_Join)(4);
    SH_AT(void (__cdecl*)(), at::kPartyPalettes.v)();
}

// original 0x519FA0 (0x15 bytes; 8 A[6]): PartySet_Load(7, 8, 4, 0); tail Scena08_PartyJoin784 (SE).
extern "C" void __cdecl Scena08_SetParty784(void) {
    Set(7, 8, 4);
    SH_CALL(Scena08_PartyJoin784)();
}

// original 0x519FC0 (0x9 bytes; 8 B[0]): 0x534030(2).
extern "C" void __cdecl Scena08_Leave2(void) {
    Leave(2);
}

void Scena08_Inject() {
    BOF3_INJECT(Scena08_Set72APartyA);
    BOF3_INJECT(Scena08_Party7);
    BOF3_INJECT(Scena08_Set724Party72);
    BOF3_INJECT(ScenaCall_Party728);
    BOF3_INJECT(Scena08_PartyJoin784);
    BOF3_INJECT(Scena08_SetParty784);
    BOF3_INJECT(Scena08_Leave2);
}

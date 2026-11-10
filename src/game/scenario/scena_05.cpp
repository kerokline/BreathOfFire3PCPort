// Moved here by tools/move_funcs.py from scena_calls.cpp (docs/source-layout-round.md).
#include "game/scenario/scena_05.h"

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

void Reload() { SH_CALL(Field_PartyLoad)(0); }

void Join(unsigned id) { SH_CALL(Party_Join)(id); }

void Leave(unsigned id) { SH_AT(void (__cdecl*)(unsigned), at::kLeave.v)(id); }

void Palettes() { SH_AT(void (__cdecl*)(), at::kPalettes.v)(); }

// The character record of Character_IndexById[k] (CharacterRecords, 0xA4
// each; the byte found unchecked), the index read at the call.
unsigned char* RecordOf(unsigned k) {
    return At(::bof3::Img(CharacterRecords_addr) + Character_IndexById[k] * at::kRecordStride);
}

}  // namespace

// original 0x519AE0 (0x35 bytes; 5 A[0]): the record of Character_IndexById[6]
// gains +0xB bit 0; the field members reloaded from the first list; 5, 1 and 6
// join.
extern "C" void __cdecl Scena05_ReloadJoin516(void) {
    unsigned char* const r = RecordOf(6);
    r[0xB] = static_cast<unsigned char>(r[0xB] | 1);
    Reload();
    Join(5);
    Join(1);
    Join(6);
}

// original 0x519B20 (0x9 bytes; 5 A[4]): Party_Join(0).
extern "C" void __cdecl Scena05_Join0(void) {
    Join(0);
}

// original 0x519B30 (0x1F bytes; 5 A[5]): PartySet_Load(0, 1, 5, 0); Field_PartyLoad(0); Party_Join(5).
extern "C" void __cdecl Scena05_Set015ReloadJoin5(void) {
    Set(0, 1, 5);
    Reload();
    Join(5);
}

// original 0x519B50 (0x1F bytes; 5 A[6]): PartySet_Load(0, 1, 5, 0); Field_PartyLoad(0); Party_Join(1).
extern "C" void __cdecl Scena05_Set015ReloadJoin1(void) {
    Set(0, 1, 5);
    Reload();
    Join(1);
}

// original 0x519B70 (0x1F bytes; 5 A[7]): PartySet_Load(0, 1, 6, 0); Field_PartyLoad(0); Party_Join(6).
extern "C" void __cdecl Scena05_Set016ReloadJoin6(void) {
    Set(0, 1, 6);
    Reload();
    Join(6);
}

// original 0x519B90 (0x9 bytes; 5 A[8], 8 A[2]): Party_Join(2).
extern "C" void __cdecl ScenaCall_Join2(void) {
    Join(2);
}

// original 0x519BA0 (0x16 bytes; 5 B[0]): 0x534030(5); 0x534030(1); 0x533E00.
extern "C" void __cdecl Scena05_Leave51(void) {
    Leave(5);
    Leave(1);
    Palettes();
}

// original 0x519BC0 (0xF bytes; 5 B[2], 6 B[6], 9 B[7], 10 B[1], 11 B[3], 12 B[3]): 0x534030(5); 0x533E00.
extern "C" void __cdecl ScenaCall_Leave5(void) {
    Leave(5);
    Palettes();
}

// original 0x519BD0 (0xF bytes; 5 B[3], 6 B[4], 9 B[8], 11 B[2], 12 B[2]): 0x534030(6); 0x533E00.
extern "C" void __cdecl ScenaCall_Leave6(void) {
    Leave(6);
    Palettes();
}

// original 0x519BE0 (0xF bytes; 5 B[4]): 0x534030(0); 0x533E00.
extern "C" void __cdecl Scena05_Leave0(void) {
    Leave(0);
    Palettes();
}

void Scena05_Inject() {
    BOF3_INJECT(Scena05_ReloadJoin516);
    BOF3_INJECT(Scena05_Join0);
    BOF3_INJECT(Scena05_Set015ReloadJoin5);
    BOF3_INJECT(Scena05_Set015ReloadJoin1);
    BOF3_INJECT(Scena05_Set016ReloadJoin6);
    BOF3_INJECT(ScenaCall_Join2);
    BOF3_INJECT(Scena05_Leave51);
    BOF3_INJECT(ScenaCall_Leave5);
    BOF3_INJECT(ScenaCall_Leave6);
    BOF3_INJECT(Scena05_Leave0);
}

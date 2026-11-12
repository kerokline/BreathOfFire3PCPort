// Moved here by tools/move_funcs.py from scena_calls.cpp (docs/source-layout-round.md).
#include "game/scenario/scena_14.h"

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
#include "game/solo/solo_scenes.h"  // DIV-0228: the scene guards

namespace {

namespace at = scena_calls::at;

}  // namespace

namespace {

using move_script::At;

void Set(unsigned a, unsigned b, unsigned c) { SH_CALL(PartySet_Load)(a, b, c, 0); }

void Join(unsigned id) { SH_CALL(Party_Join)(id); }

void Leave(unsigned id) { SH_AT(void (__cdecl*)(unsigned), at::kLeave.v)(id); }

void Palettes() { SH_AT(void (__cdecl*)(), at::kPalettes.v)(); }

// Field member i (ObjTrio + 0x14C i, unchecked) and its character id +0x89.
unsigned char* Member(unsigned i) { return rv::Bytes(&ObjTrio[i]); }

unsigned char MemberId(unsigned i) { return At(::bof3::Img(ObjTrio_addr) + 0x89 + i * at::kMemberStride)[0]; }

// The character record of Character_IndexById[k] (CharacterRecords, 0xA4
// each; the byte found unchecked), the index read at the call.
unsigned char* RecordOf(unsigned k) {
    return At(::bof3::Img(CharacterRecords_addr) + Character_IndexById[k] * at::kRecordStride);
}

}  // namespace

// original 0x51A780 (0x2D bytes; 14 A[0]): PartySet_Load(7, 8, 4, 0); Field_MemberCount 0; Party_Join(7); Party_Join(4); Party_Join(8).
extern "C" void __cdecl Scena14_Party748(void) {
    Set(7, 8, 4);
    Field_MemberCount = 0;
    Join(7);
    Join(4);
    Join(8);
}

// original 0x51A7B0 (0x2D bytes; 14 A[1]): PartySet_Load(7, 4, 6, 0); Field_MemberCount 0; Party_Join(7); Party_Join(4); Party_Join(6).
extern "C" void __cdecl Scena14_Party746(void) {
    Set(7, 4, 6);
    Field_MemberCount = 0;
    Join(7);
    Join(4);
    Join(6);
}

// original 0x51A7E0 (0x2D bytes; 14 A[2]): PartySet_Load(7, 5, 6, 0); Field_MemberCount 0; Party_Join(7); Party_Join(5); Party_Join(6).
extern "C" void __cdecl Scena14_Party756(void) {
    Set(7, 5, 6);
    Field_MemberCount = 0;
    Join(7);
    Join(5);
    Join(6);
}

// original 0x51A810 (0x71 bytes; 14 A[3]): the three field members' ids copied,
// the last that is neither 7 nor 4 saved in 0x903A10; each sent away; the set
// (0xA, 0xFF, 0xFF) and member 0xA alone; 0x533E00.
extern "C" void __cdecl Scena14_SaveLeaveAllPartyA(void) {
    unsigned char ids[3];
    // DIV-0228 SoloCharacter: a walker slot past the count is no member
    // (0xFF), and with no member neither 7 nor 4 the saved byte is 0xFF
    // rather than an earlier save's (solo_scenes.h). Off: the original.
    const bool solo_scene = solo::Scene_Active();
    if (solo_scene) At(::bof3::Img(at::kSaved))[0] = 0xFF;
    for (unsigned i = 0; i < 3; ++i) {
        const unsigned char id = solo::Scene_MemberId(i, MemberId(i));
        if (id != 7 && id != 4 && !(solo_scene && id == 0xFF)) At(::bof3::Img(at::kSaved))[0] = id;
        ids[i] = id;
    }
    for (unsigned char id : ids) Leave(id);
    Set(0xA, 0xFF, 0xFF);
    Field_MemberCount = 0;
    Join(0xA);
    Palettes();
}

// original 0x51A890 (0x46 bytes; 14 A[4]): the record of
// Character_IndexById[10] (Char_WhelpSlot) loses +0xB bit 0 before the
// load; the party (7, 4, 8); tail 0x533E00.
extern "C" void __cdecl Scena14_Out10Party748(void) {
    unsigned char* const r = RecordOf(10);
    r[0xB] = static_cast<unsigned char>(r[0xB] & 0xFE);
    Set(7, 4, 8);
    Field_MemberCount = 0;
    Join(7);
    Join(4);
    Join(8);
    Palettes();
}

// original 0x51A8E0 (0x47 bytes; 14 A[5]): the party (0, 1, 5); 0x533E00; then
// the second party list set to 7, 8, 5.
extern "C" void __cdecl Scena14_Party015Lists785(void) {
    Set(0, 1, 5);
    Field_MemberCount = 0;
    Join(0);
    Join(1);
    Join(5);
    Palettes();
    unsigned char* const lists = At(::bof3::Img(Cond_Flags_addr) + 0xd2);
    lists[3] = 7;
    lists[4] = 8;
    lists[5] = 5;
}

// original 0x51A930 (0x31 bytes; 14 A[6]): PartySet_Load(7, 2, 6, 0); Field_MemberCount 0; Party_Join(7); Party_Join(2); Party_Join(6); 0x533E00.
extern "C" void __cdecl Scena14_Party726(void) {
    Set(7, 2, 6);
    Field_MemberCount = 0;
    Join(7);
    Join(2);
    Join(6);
    Palettes();
}

// original 0x51A970 (0x3A bytes; 14 A[8]): the set (7, 4, first saved id) and
// members 7, 4 and the first saved id, read again after the two joins; tail
// 0x533E00.
extern "C" void __cdecl Scena14_Party74First(void) {
    Set(7, 4, At(::bof3::Img(at::kSaved))[0]);
    Field_MemberCount = 0;
    Join(7);
    Join(4);
    Join(At(::bof3::Img(at::kSaved))[0]);
    Palettes();
}

// original 0x51A9B0 (0x5D bytes; 14 A[9]): the three field members' ids saved
// in 0x903A10..12, each sent away (read from there after the last call, 7
// too), the set (7, 8, 4) and member 7; 0x533E00.
extern "C" void __cdecl Scena14_SaveLeaveAllParty7(void) {
    unsigned char* const saved = At(::bof3::Img(at::kSaved));
    // DIV-0228 SoloCharacter: a walker slot past the count saves 0xFF, not the
    // off record's stale +0x89 (solo_scenes.h). Off: the original.
    for (unsigned i = 0; i < 3; ++i) saved[i] = solo::Scene_MemberId(i, MemberId(i));
    for (unsigned i = 0; i < 3; ++i) Leave(saved[i]);
    Set(7, 8, 4);
    Field_MemberCount = 0;
    Join(7);
    Palettes();
}

// original 0x51AA10 (0x23 bytes; 14 A[10]): PartySet_Load(7, 2, 6, 0); Field_MemberCount 0; Party_Join(7); 0x533E00.
extern "C" void __cdecl Scena14_Set726Party7(void) {
    Set(7, 2, 6);
    Field_MemberCount = 0;
    Join(7);
    Palettes();
}

// original 0x51AA40 (0x31 bytes; 14 A[11], 15 A[3]): PartySet_Load(7, 8, 4, 0); Field_MemberCount 0; Party_Join(7); Party_Join(8); Party_Join(4); 0x533E00.
extern "C" void __cdecl ScenaCall_Party784(void) {
    Set(7, 8, 4);
    Field_MemberCount = 0;
    Join(7);
    Join(8);
    Join(4);
    Palettes();
}

// original 0x51AA80 (0x68 bytes; 14 A[12]): the first saved id that is neither
// 7 nor 4 (the third if all are) with 7 and 4; 0x533E00.
//
// As the original has it: the id is a byte in a stack dword whose upper bytes
// are stale; the callees read the byte.
extern "C" void __cdecl Scena14_Party74Other(void) {
    const unsigned char* const saved = At(::bof3::Img(at::kSaved));
    unsigned char k;
    unsigned i = 0;
    for (;;) {
        k = saved[i];
        // DIV-0228 SoloCharacter: a 0xFF a folded save left is passed over as
        // 7 and 4 are (solo_scenes.h); off, the original's test.
        if (k != 7 && k != 4 && !solo::Scene_SavedNone(k)) break;
        if (++i >= 3) break;
    }
    Set(7, 4, k);
    Field_MemberCount = 0;
    Join(7);
    Join(4);
    Join(k);
    Palettes();
}

// original 0x51AAF0 (0x53 bytes; 14 B[0]): Field_MemberCount recounted - the
// field members whose +0 has bit 0 - with the three ids copied; each id that is
// not 7 leaves. No palettes.
extern "C" void __cdecl Scena14_RecountLeaveAll(void) {
    unsigned char n = 0;
    unsigned char ids[3];
    for (unsigned i = 0; i < 3; ++i) {
        if (Member(i)[0] & 1) ++n;
        ids[i] = MemberId(i);
    }
    Field_MemberCount = n;
    for (unsigned char id : ids)
        if (id != 7) Leave(id);
}

void Scena14_Inject() {
    BOF3_INJECT(Scena14_Party748);
    BOF3_INJECT(Scena14_Party746);
    BOF3_INJECT(Scena14_Party756);
    BOF3_INJECT(Scena14_SaveLeaveAllPartyA);
    BOF3_INJECT(Scena14_Out10Party748);
    BOF3_INJECT(Scena14_Party015Lists785);
    BOF3_INJECT(Scena14_Party726);
    BOF3_INJECT(Scena14_Party74First);
    BOF3_INJECT(Scena14_SaveLeaveAllParty7);
    BOF3_INJECT(Scena14_Set726Party7);
    BOF3_INJECT(ScenaCall_Party784);
    BOF3_INJECT(Scena14_Party74Other);
    BOF3_INJECT(Scena14_RecountLeaveAll);
}

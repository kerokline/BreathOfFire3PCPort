// Moved here by tools/move_funcs.py from scena_calls.cpp (docs/source-layout-round.md).
#include "game/scenario/scena_13.h"

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

}  // namespace

// original 0x51A5A0 (0x2D bytes; 13 A[0]): PartySet_Load(7, 5, 6, 0); Field_MemberCount 0; Party_Join(7); Party_Join(6); Party_Join(5).
extern "C" void __cdecl Scena13_Party765(void) {
    Set(7, 5, 6);
    Field_MemberCount = 0;
    Join(7);
    Join(6);
    Join(5);
}

// original 0x51A5D0 (0x60 bytes; 13 A[1]): both party lists saved (0x903A10..12
// the first, 0x903A13..15 the second), the three saved members sent away (each
// id read from 0x903A10 after the last call), the set (7, 4, 8) and member 7;
// 0x533E00.
extern "C" void __cdecl Scena13_SaveParty7(void) {
    unsigned char* const saved = At(::bof3::Img(at::kSaved));
    const unsigned char* const lists = At(::bof3::Img(Cond_Flags_addr) + 0xd2);
    for (unsigned i = 0; i < 3; ++i) {
        saved[i] = lists[i];
        saved[3 + i] = lists[3 + i];
    }
    for (unsigned i = 0; i < 3; ++i) Leave(saved[i]);
    Set(7, 4, 8);
    Field_MemberCount = 0;
    Join(7);
    Palettes();
}

// original 0x51A630 (0x1F bytes; 13 A[2]): PartySet_Load(7, 5, 8, 0); Field_MemberCount 0; Party_Join(7).
extern "C" void __cdecl Scena13_Set758Party7(void) {
    Set(7, 5, 8);
    Field_MemberCount = 0;
    Join(7);
}

// original 0x51A650 (0x6A bytes; 13 A[3]): the saved party loaded and joined
// in order, then the second list put back from 0x903A13..15 (read after the
// calls).
//
// As the original has it: the three ids are dwords read across a 6-byte
// local; the callees read the byte.
extern "C" void __cdecl Scena13_RestoreSaved(void) {
    unsigned char* const saved = At(::bof3::Img(at::kSaved));
    const unsigned char s[3] = {saved[0], saved[1], saved[2]};
    Set(s[0], s[1], s[2]);
    Field_MemberCount = 0;
    Join(s[0]);
    Join(s[1]);
    Join(s[2]);
    unsigned char* const lists = At(::bof3::Img(Cond_Flags_addr) + 0xd2);
    for (unsigned i = 0; i < 3; ++i) lists[3 + i] = saved[3 + i];
}

// original 0x51A6C0 (0x31 bytes; 13 A[4]): PartySet_Load(7, 5, 8, 0); Field_MemberCount 0; Party_Join(7); Party_Join(5); Party_Join(8); 0x533E00.
extern "C" void __cdecl Scena13_Party758(void) {
    Set(7, 5, 8);
    Field_MemberCount = 0;
    Join(7);
    Join(5);
    Join(8);
    Palettes();
}

// original 0x51A700 (0x5D bytes; 13 A[5]): the three field members' ids copied,
// each sent away (7 too), the set (7, 4, 2) and member 7; 0x533E00.
extern "C" void __cdecl Scena13_LeaveAllParty7(void) {
    const unsigned char ids[3] = {MemberId(0), MemberId(1), MemberId(2)};
    for (unsigned char id : ids) Leave(id);
    Set(7, 4, 2);
    Field_MemberCount = 0;
    Join(7);
    Palettes();
}

// original 0x51A760 (0xF bytes; 13 A[6]): Party_Join(4); 0x533E00.
extern "C" void __cdecl Scena13_Join4(void) {
    Join(4);
    Palettes();
}

// original 0x51A770 (0xF bytes; 13 A[7]): Party_Join(2); 0x533E00.
extern "C" void __cdecl Scena13_Join2(void) {
    Join(2);
    Palettes();
}

void Scena13_Inject() {
    BOF3_INJECT(Scena13_Party765);
    BOF3_INJECT(Scena13_SaveParty7);
    BOF3_INJECT(Scena13_Set758Party7);
    BOF3_INJECT(Scena13_RestoreSaved);
    BOF3_INJECT(Scena13_Party758);
    BOF3_INJECT(Scena13_LeaveAllParty7);
    BOF3_INJECT(Scena13_Join4);
    BOF3_INJECT(Scena13_Join2);
}

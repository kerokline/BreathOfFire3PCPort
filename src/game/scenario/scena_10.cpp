// Moved here by tools/move_funcs.py from scena_calls.cpp (docs/source-layout-round.md).
#include "game/scenario/scena_10.h"

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

}  // namespace

// original 0x51A370 (0x2D bytes; 10 A[0]): PartySet_Load(7, 2, 8, 0); Field_MemberCount 0; Party_Join(7); Party_Join(2); Party_Join(8).
extern "C" void __cdecl Scena10_Party728(void) {
    Set(7, 2, 8);
    Field_MemberCount = 0;
    Join(7);
    Join(2);
    Join(8);
}

// original 0x51A3A0 (0x6A bytes; 10 A[1]): every field member sent away, then
// the set (7, 5, 8) and members 7 and 5.
//
// As the original has it: the count is read again after every call, and
// 0x534030 lowers it and moves the later members down one, so the loop's
// index passes over every second member - with three members, the first and
// the third leave and the second stays in the lists (latent;
// docs/scena_calls.md section 6).
extern "C" void __cdecl Scena10_Party75(void) {
    if (Field_MemberCount != 0) {
        unsigned char i = 0;
        do {
            Leave(MemberId(i));
            ++i;
        } while (i < Field_MemberCount);
    }
    Set(7, 5, 8);
    Field_MemberCount = 0;
    Join(7);
    Join(5);
}

// original 0x51A410 (0x9 bytes; 10 A[2]): Party_Join(8).
extern "C" void __cdecl Scena10_Join8(void) {
    Join(8);
}

void Scena10_Inject() {
    BOF3_INJECT(Scena10_Party728);
    BOF3_INJECT(Scena10_Party75);
    BOF3_INJECT(Scena10_Join8);
}

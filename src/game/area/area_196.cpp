// Moved here by tools/move_funcs.py from area_w4f.cpp (docs/source-layout-round.md).
#include "game/area/area_196.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w4f_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w4f::at;

}  // namespace

namespace {

using area_harness::Mem;

using move_script::Word;

using U = std::uint32_t;

unsigned char& B(U address) { return *Mem(address); }

// The member search of areas 196 and 197's handlers (nine bodies, one a
// handler, each over its own two tables; AR3F's Area143_MessageByMember is
// the same code): for each of the four keys in turn, each of
// Field_MemberCount's party records (the count read once, compared
// unsigned): the first record whose +0x89 is the key opens the key's message
// (Msg_OpenScript) and sets Field_Request 2. None: nothing. The records are
// not bounded by three (the count is).
void MessageByMember(U keys) {
    const U messages = keys + at::kKeysToMessages;
    const unsigned char count = Field_MemberCount;
    for (unsigned i = 0; i < 4; ++i) {
        const unsigned char key = B(keys + i);
        for (unsigned char j = 0; j < count; ++j) {
            if (B(::bof3::Img(ObjTrio_addr) + 0x89 + j * at::kPartyStride) != key) continue;
            AH_CALL(Msg_OpenScript)(Word(Mem(messages + i * 2)));
            Field_Request = 2;
            return;
        }
    }
}

}  // namespace

// original 0x42C780 (area 196 +0x3C[0]; PSX 0x801F3F90): MessageByMember over
// Area196_Keys0.
extern "C" void __cdecl Area196_MessageByMember0(void) { MessageByMember(::bof3::Img(Area196_Keys0_addr)); }

// original 0x42C810 (area 196 +0x3C[1]; PSX 0x801F4054): over Area196_Keys1.
extern "C" void __cdecl Area196_MessageByMember1(void) { MessageByMember(::bof3::Img(Area196_Keys1_addr)); }

// original 0x42C8A0 (area 196 +0x3C[2], PSX 0x801F4118; also area 148
// +0x34[2] = +0x3C[0], area 167 +0x34[14] = +0x3C[9], area 173 +0x3C[0]):
// Cond_ByteFE 1.
extern "C" void __cdecl Area196_SetCondFE(void) { Cond_ByteFE = 1; }

void Area196_Inject() {
    BOF3_INJECT(Area196_MessageByMember0);
    BOF3_INJECT(Area196_MessageByMember1);
    BOF3_INJECT(Area196_SetCondFE);
}

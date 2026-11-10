// The chapter call tables' entries - round ten, group CALLS, taken with the
// scenario harness (scenario_harness.h). docs/scena_calls.md has each
// function, which chapters' tables hold it, and at which index.
//
// Scenario_CallA 0x5341A0 (ours, field_modes.cpp) and Scenario_CallB 0x5341C0
// (Capcom's) jump to entry n & 0xFF of the current chapter's table A or B
// (Scena<NN>_CallA / _CallB), the caller's arguments in place and ecx = the
// index. The 98 entries here read no argument: each is one party change,
// made of five callees -
//
//   Set(a, b, c)    PartySet_Load(a, b, c, 0): the party set's files for those
//                   three members, a frame at a time until loaded (ours)
//   Reload()        Field_PartyLoad(0): the field members rebuilt from the
//                   first party list (ours)
//   Join(id)        Party_Join(id) (ours)
//   Leave(id)       0x534030(id): the member leaves (nobody's)
//   Palettes()      0x533E00: the members' palettes reloaded (nobody's)
//
// - with Field_MemberCount cleared before a party is joined anew, and some
// with a character record's +0xB bit 0 ("in the party") or +9 set by hand,
// the party lists emptied or rewritten, or a party saved to and restored
// from 0x903A10..0x903A15. Names: Scena<NN>_ where one chapter's tables hold
// the entry, ScenaCall_ where several do; Party<ids> = the count cleared and
// those joined (after a set of the same three), Set<abc> a set, Reload,
// Join<ids>, Leave<ids>; X is 0xFF.
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies. No
// divergence: each is a faithful replacement. Two entries read their
// caller's ecx and can overwrite their return address with a member count
// above 4 (0x51A300, 0x51AB50): ours takes ecx through a naked entry and
// aborts where the original would return to what it wrote
// (docs/scena_calls.md section 6).
#include "game/scena_calls.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "common/asm_name.h"
#include "game/record_views.h"
#include "game/move_script_bytes.h"
#include "game/scena_calls_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = scena_calls::at;
using move_script::At;

void Set(unsigned a, unsigned b, unsigned c) { SH_CALL(PartySet_Load)(a, b, c, 0); }
void Reload() { SH_CALL(Field_PartyLoad)(0); }
void Join(unsigned id) { SH_CALL(Party_Join)(id); }
void Leave(unsigned id) { SH_AT(void (__cdecl*)(unsigned), at::kLeave.v)(id); }
void Palettes() { SH_AT(void (__cdecl*)(), at::kPalettes.v)(); }

unsigned char MemberId(unsigned i) { return At(::bof3::Img(ObjTrio_addr) + 0x89 + i * at::kMemberStride)[0]; }

// The character record of Character_IndexById[k] (CharacterRecords, 0xA4
// each; the byte found unchecked), the index read at the call.
unsigned char* RecordOf(unsigned k) {
    return At(::bof3::Img(CharacterRecords_addr) + Character_IndexById[k] * at::kRecordStride);
}

// Both 3-byte party lists 0x904062..0x904067 to 0xFF.
void EmptyLists() {
    unsigned char* const lists = At(::bof3::Img(Cond_Flags_addr) + 0xd2);
    for (unsigned i = 0; i < 6; ++i) lists[i] = 0xFF;
}

}  // namespace

// The two entries that take the caller's ecx: their bodies, called by the naked
// entries below with ecx pushed.
extern "C" void __cdecl scena_calls_SaveAndLeave(std::uint32_t ecx);
extern "C" void __cdecl scena_calls_LeaveAllParty7(std::uint32_t ecx);

// original 0x51A300 (0x53 bytes; 9 B[6], 10 B[3]): the field members' ids saved
// (0x903A10..) and each of the first three that is not 7 sent away; 0x533E00.
// The entry takes the caller's ecx (ScenaCall_SaveAndLeave, below).
//
// As the original has it: the ids are gathered in the caller's ecx pushed as a
// local, one byte a member below Field_MemberCount, and the first three bytes
// are then read whatever the count - so with fewer than three members the rest
// are ecx's bytes, the entry's index (Scenario_CallA / B leave n & 0xFF in ecx:
// 0x534030 is called with 0 for the missing ones). A count of 5 or more writes
// past the local over the return address (and the caller's frame): the
// original returns to what it wrote. Ours does everything the original does
// before that return and then aborts.
extern "C" void __cdecl scena_calls_SaveAndLeave(std::uint32_t ecx) {
    unsigned char local[4];
    std::memcpy(local, &ecx, sizeof local);
    unsigned char* const saved = At(::bof3::Img(at::kSaved));
    const unsigned n = Field_MemberCount;
    for (unsigned i = 0; i < n; ++i) {
        const unsigned char id = MemberId(i);
        saved[i] = id;
        if (i < sizeof local) local[i] = id;
    }
    for (unsigned i = 0; i < 3; ++i)
        if (local[i] != 7) Leave(local[i]);
    Palettes();
    if (n > sizeof local)
        bof3::Fatal("ScenaCall_SaveAndLeave (0x51A300): Field_MemberCount %u wrote %u ids past its 4-byte local, over the "
                    "return address",
                    n, n - static_cast<unsigned>(sizeof local));
}

// original 0x51AB50 (0x68 bytes; 15 A[0]): the field members' ids gathered, each
// of the first three sent away (7 too), the set (7, 4, 2) and member 7;
// 0x533E00. The entry takes the caller's ecx (Scena15_LeaveAllParty7, below).
//
// As the original has it: as 0x51A300 - the ids gathered in the caller's ecx
// pushed as a local, one byte a member below Field_MemberCount, the first three
// bytes read whatever the count (the entry's index 0 for the missing ones), and
// a count of 5 or more writes over the return address: ours does the rest and
// then aborts.
extern "C" void __cdecl scena_calls_LeaveAllParty7(std::uint32_t ecx) {
    unsigned char local[4];
    std::memcpy(local, &ecx, sizeof local);
    const unsigned n = Field_MemberCount;
    for (unsigned i = 0; i < n; ++i) {
        const unsigned char id = MemberId(i);
        if (i < sizeof local) local[i] = id;
    }
    for (unsigned i = 0; i < 3; ++i) Leave(local[i]);
    Set(7, 4, 2);
    Field_MemberCount = 0;
    Join(7);
    Palettes();
    if (n > sizeof local)
        bof3::Fatal("Scena15_LeaveAllParty7 (0x51AB50): Field_MemberCount %u wrote %u ids past its 4-byte local, over the "
                    "return address",
                    n, n - static_cast<unsigned>(sizeof local));
}

void ScenaCalls_Inject() {
    if (bof3::WantsShadow("scena_calls")) scena_calls::SelfTest();
}

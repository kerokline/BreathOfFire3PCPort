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
void Leave(unsigned id) { SH_AT(void (__cdecl*)(unsigned), at::kLeave)(id); }
void Palettes() { SH_AT(void (__cdecl*)(), at::kPalettes)(); }

// Field member i (ObjTrio + 0x14C i, unchecked) and its character id +0x89.
unsigned char* Member(unsigned i) { return ObjTrio + i * at::kMemberStride; }
unsigned char MemberId(unsigned i) { return At(at::kMemberIds + i * at::kMemberStride)[0]; }

// The character record of MoveScript_EffectState[k] (CharacterRecords, 0xA4
// each; the byte found unchecked), the index read at the call.
unsigned char* RecordOf(unsigned k) {
    return At(bof3::addr::CharacterRecords + MoveScript_EffectState[k] * at::kRecordStride);
}

// Both 3-byte party lists 0x904062..0x904067 to 0xFF.
void EmptyLists() {
    unsigned char* const lists = At(at::kPartyLists);
    for (unsigned i = 0; i < 6; ++i) lists[i] = 0xFF;
}

}  // namespace

// The two entries that take the caller's ecx: their bodies, called by the naked
// entries below with ecx pushed.
extern "C" void __cdecl scena_calls_SaveAndLeave(std::uint32_t ecx);
extern "C" void __cdecl scena_calls_LeaveAllParty7(std::uint32_t ecx);

// original 0x5198F0 (0x49 bytes; 1 A[0]): the record of MoveScript_EffectState[10]
// (Char_WhelpSlot) loses +0xB bit 0 before the load; the record of
// MoveScript_EffectState[9], read after it, gets +9 = 9.
extern "C" void __cdecl Scena01_Set934Party9(void) {
    unsigned char* const out = RecordOf(10);
    out[0xB] = static_cast<unsigned char>(out[0xB] & 0xFE);
    Set(9, 3, 4);
    Field_MemberCount = 0;
    RecordOf(9)[9] = 9;
    Join(9);
}

// original 0x519940 (0x42 bytes; 1 A[1]): the record of MoveScript_EffectState[0]
// gets +9 = 0 before the load.
extern "C" void __cdecl Scena01_Party034(void) {
    RecordOf(0)[9] = 0;
    Set(0, 3, 4);
    Field_MemberCount = 0;
    Join(0);
    Join(3);
    Join(4);
}

// original 0x519990 (0x9 bytes; 1 A[2], 2 A[0]): Party_Join(4).
extern "C" void __cdecl ScenaCall_Join4(void) {
    Join(4);
}

// original 0x5199A0 (0xF bytes; 1 B[0], 2 B[0], 9 B[0], 11 B[0], 12 B[0]): 0x534030(4); 0x533E00.
extern "C" void __cdecl ScenaCall_Leave4(void) {
    Leave(4);
    Palettes();
}

// original 0x5199B0 (0x49 bytes; 2 B[1]): the records of MoveScript_EffectState[4]
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

// original 0x519A00 (0x3F bytes; 3 A[0]): after the load both party lists are
// emptied (0xFF), then member 0 joins alone.
extern "C" void __cdecl Scena03_Set012Party0(void) {
    Set(0, 1, 2);
    EmptyLists();
    Field_MemberCount = 0;
    Join(0);
}

// original 0x519A40 (0x9 bytes; 3 A[1], 5 A[1]): Party_Join(1).
extern "C" void __cdecl ScenaCall_Join1(void) {
    Join(1);
}

// original 0x519A50 (0xF bytes; 3 B[0], 5 B[1]): 0x534030(1); 0x533E00.
extern "C" void __cdecl ScenaCall_Leave1(void) {
    Leave(1);
    Palettes();
}

// original 0x519A60 (0x46 bytes; 4 A[0]): as 0x519A00 with the set (0, 1, 5)
// and members 0 and 1.
extern "C" void __cdecl Scena04_Set015Party01(void) {
    Set(0, 1, 5);
    EmptyLists();
    Field_MemberCount = 0;
    Join(0);
    Join(1);
}

// original 0x519AB0 (0x2D bytes; 4 A[1], 7 A[0]): PartySet_Load(0, 1, 5, 0); Field_MemberCount 0; Party_Join(0); Party_Join(1); Party_Join(5).
extern "C" void __cdecl ScenaCall_Party015(void) {
    Set(0, 1, 5);
    Field_MemberCount = 0;
    Join(0);
    Join(1);
    Join(5);
}

// original 0x519AE0 (0x35 bytes; 5 A[0]): the record of MoveScript_EffectState[6]
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

// original 0x519BF0 (0x2A bytes; 6 A[0]): PartySet_Load(0, 1, 5, 0); Field_PartyLoad(0); Party_Join(0); Party_Join(5); 0x533E00.
extern "C" void __cdecl Scena06_Set015ReloadJoin05(void) {
    Set(0, 1, 5);
    Reload();
    Join(0);
    Join(5);
    Palettes();
}

// original 0x519C20 (0x23 bytes; 6 A[1]): PartySet_Load(0, 1, 5, 0); Field_PartyLoad(0); Party_Join(5); 0x533E00.
extern "C" void __cdecl Scena06_Set015ReloadJoin5(void) {
    Set(0, 1, 5);
    Reload();
    Join(5);
    Palettes();
}

// original 0x519C50 (0x23 bytes; 6 A[2]): PartySet_Load(0, 1, 5, 0); Field_PartyLoad(0); Party_Join(1); 0x533E00.
extern "C" void __cdecl Scena06_Set015ReloadJoin1(void) {
    Set(0, 1, 5);
    Reload();
    Join(1);
    Palettes();
}

// original 0x519C80 (0x2A bytes; 6 A[3]): PartySet_Load(0, 1, 5, 0); Field_PartyLoad(0); Party_Join(1); Party_Join(5); 0x533E00.
extern "C" void __cdecl Scena06_Set015ReloadJoin15(void) {
    Set(0, 1, 5);
    Reload();
    Join(1);
    Join(5);
    Palettes();
}

// original 0x519CB0 (0x9 bytes; 5 A[3], 6 A[5]): Party_Join(6).
extern "C" void __cdecl ScenaCall_Join6(void) {
    Join(6);
}

// original 0x519CC0 (0x1F bytes; 5 A[9], 5 A[10], 6 A[6], 6 A[8]): PartySet_Load(0, 1, 2, 0); Field_PartyLoad(0); Party_Join(1).
extern "C" void __cdecl ScenaCall_Set012ReloadJoin1(void) {
    Set(0, 1, 2);
    Reload();
    Join(1);
}

// original 0x519CE0 (0x1F bytes; 6 A[7]): PartySet_Load(0, 1, 2, 0); Field_PartyLoad(0); Party_Join(2).
extern "C" void __cdecl Scena06_Set012ReloadJoin2(void) {
    Set(0, 1, 2);
    Reload();
    Join(2);
}

// original 0x519D00 (0x26 bytes; 6 A[9]): PartySet_Load(0, 1, 2, 0); Field_PartyLoad(0); Party_Join(1); Party_Join(2).
extern "C" void __cdecl Scena06_Set012ReloadJoin12(void) {
    Set(0, 1, 2);
    Reload();
    Join(1);
    Join(2);
}

// original 0x519D30 (0x16 bytes; 6 B[0]): 0x534030(0); 0x534030(2); 0x533E00.
extern "C" void __cdecl Scena06_Leave02(void) {
    Leave(0);
    Leave(2);
    Palettes();
}

// original 0x519D50 (0x16 bytes; 6 B[1]): 0x534030(0); 0x534030(5); 0x533E00.
extern "C" void __cdecl Scena06_Leave05(void) {
    Leave(0);
    Leave(5);
    Palettes();
}

// original 0x519D70 (0x16 bytes; 6 B[2]): 0x534030(0); 0x534030(6); 0x533E00.
extern "C" void __cdecl Scena06_Leave06(void) {
    Leave(0);
    Leave(6);
    Palettes();
}

// original 0x519D90 (0xF bytes; 6 B[3], 9 B[2], 11 B[1], 12 B[1]): 0x534030(2); 0x533E00.
extern "C" void __cdecl ScenaCall_Leave2(void) {
    Leave(2);
    Palettes();
}

// original 0x519DA0 (0x16 bytes; 6 B[5]): 0x534030(6); 0x534030(2); 0x533E00.
extern "C" void __cdecl Scena06_Leave62(void) {
    Leave(6);
    Leave(2);
    Palettes();
}

// original 0x519DC0 (0x16 bytes; 6 B[7]): 0x534030(5); 0x534030(6); 0x533E00.
extern "C" void __cdecl Scena06_Leave56(void) {
    Leave(5);
    Leave(6);
    Palettes();
}

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
// MoveScript_EffectState[1], [5], [6] and [2] lose +0xB bit 0, each index read
// after the call. No palettes.
extern "C" void __cdecl Scena07_Leave2Out1562(void) {
    Leave(2);
    static const unsigned kOut[] = {1, 5, 6, 2};
    for (unsigned k : kOut) {
        unsigned char* const r = RecordOf(k);
        r[0xB] = static_cast<unsigned char>(r[0xB] & 0xFE);
    }
}

// original 0x519EA0 (0x23 bytes; 8 A[0]): PartySet_Load(7, 2, 0xA, 0); Field_MemberCount 0; Party_Join(0xA); 0x533E00.
extern "C" void __cdecl Scena08_Set72APartyA(void) {
    Set(7, 2, 0xA);
    Field_MemberCount = 0;
    Join(0xA);
    Palettes();
}

// original 0x519ED0 (0x25 bytes; 8 A[1]): the party emptied, the record of
// MoveScript_EffectState[10] (Char_WhelpSlot) loses +0xB bit 0, member 7 joins.
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

// original 0x519FA0 (0x15 bytes; 8 A[6]): PartySet_Load(7, 8, 4, 0); tail Scena08_PartyJoin784 (SE).
extern "C" void __cdecl Scena08_SetParty784(void) {
    Set(7, 8, 4);
    SH_CALL(Scena08_PartyJoin784)();
}

// original 0x519FC0 (0x9 bytes; 8 B[0]): 0x534030(2).
extern "C" void __cdecl Scena08_Leave2(void) {
    Leave(2);
}

// original 0x519FD0 (0x23 bytes; 9 A[0]): PartySet_Load(7, 8, 2, 0); Field_PartyLoad(0); Party_Join(8); 0x533E00.
extern "C" void __cdecl Scena09_Set782ReloadJoin8(void) {
    Set(7, 8, 2);
    Reload();
    Join(8);
    Palettes();
}

// original 0x51A000 (0x23 bytes; 9 A[1]): PartySet_Load(7, 8, 2, 0); Field_PartyLoad(0); Party_Join(2); 0x533E00.
extern "C" void __cdecl Scena09_Set782ReloadJoin2(void) {
    Set(7, 8, 2);
    Reload();
    Join(2);
    Palettes();
}

// original 0x51A030 (0x2A bytes; 9 A[2], 11 A[0], 11 A[3], 12 A[0], 12 A[3]): PartySet_Load(7, 8, 5, 0); Field_PartyLoad(0); Party_Join(8); Party_Join(5); 0x533E00.
extern "C" void __cdecl ScenaCall_Set785ReloadJoin85(void) {
    Set(7, 8, 5);
    Reload();
    Join(8);
    Join(5);
    Palettes();
}

// original 0x51A060 (0x23 bytes; 9 A[3], 11 A[2], 11 A[5], 12 A[2], 12 A[5]): PartySet_Load(7, 8, 5, 0); Field_PartyLoad(0); Party_Join(5); 0x533E00.
extern "C" void __cdecl ScenaCall_Set785ReloadJoin5(void) {
    Set(7, 8, 5);
    Reload();
    Join(5);
    Palettes();
}

// original 0x51A090 (0x23 bytes; 9 A[4], 11 A[1], 12 A[1]): PartySet_Load(7, 8, 5, 0); Field_PartyLoad(0); Party_Join(8); 0x533E00.
extern "C" void __cdecl ScenaCall_Set785ReloadJoin8(void) {
    Set(7, 8, 5);
    Reload();
    Join(8);
    Palettes();
}

// original 0x51A0C0 (0x16 bytes; 9 A[5]): Field_PartyLoad(0); Party_Join(6); 0x533E00.
extern "C" void __cdecl Scena09_ReloadJoin6(void) {
    Reload();
    Join(6);
    Palettes();
}

// original 0x51A0E0 (0x2A bytes; 9 A[6], 11 A[6], 12 A[6]): PartySet_Load(7, 6, 5, 0); Field_PartyLoad(0); Party_Join(5); Party_Join(6); 0x533E00.
extern "C" void __cdecl ScenaCall_Set765ReloadJoin56(void) {
    Set(7, 6, 5);
    Reload();
    Join(5);
    Join(6);
    Palettes();
}

// original 0x51A110 (0x23 bytes; 9 A[7], 11 A[7], 12 A[7]): PartySet_Load(7, 6, 5, 0); Field_PartyLoad(0); Party_Join(6); 0x533E00.
extern "C" void __cdecl ScenaCall_Set765ReloadJoin6(void) {
    Set(7, 6, 5);
    Reload();
    Join(6);
    Palettes();
}

// original 0x51A140 (0x23 bytes; 9 A[8]): PartySet_Load(7, 6, 5, 0); Field_PartyLoad(0); Party_Join(5); 0x533E00.
extern "C" void __cdecl Scena09_Set765ReloadJoin5(void) {
    Set(7, 6, 5);
    Reload();
    Join(5);
    Palettes();
}

// original 0x51A170 (0x5C bytes; 9 A[9], 10 A[5]): the saved member - the last
// of the three saved ids that is neither 7 nor 2, else 8 - with 7 and 2; the
// three saved ids cleared on the way.
//
// As the original has it: the id is a byte in a pushed dword whose upper three
// bytes are the caller's ecx; PartySet_Load and Party_Join read only the byte.
extern "C" void __cdecl ScenaCall_Party72Saved(void) {
    unsigned char* const saved = At(at::kSaved);
    unsigned char k = 8;
    for (unsigned i = 0; i < 3; ++i) {
        const unsigned char b = saved[i];
        if (b != 7 && b != 2) k = b;
        saved[i] = 0;
    }
    Set(7, 2, k);
    Field_MemberCount = 0;
    Join(7);
    Join(2);
    Join(k);
    Palettes();
}

// original 0x51A1D0 (0x23 bytes; 9 A[10]): PartySet_Load(7, 4, 8, 0); Field_PartyLoad(0); Party_Join(8); 0x533E00.
extern "C" void __cdecl Scena09_Set748ReloadJoin8(void) {
    Set(7, 4, 8);
    Reload();
    Join(8);
    Palettes();
}

// original 0x51A200 (0x2A bytes; 9 A[11]): PartySet_Load(7, 4, 8, 0); Field_PartyLoad(0); Party_Join(4); Party_Join(8); 0x533E00.
extern "C" void __cdecl Scena09_Set748ReloadJoin48(void) {
    Set(7, 4, 8);
    Reload();
    Join(4);
    Join(8);
    Palettes();
}

// original 0x51A230 (0x23 bytes; 9 A[12]): PartySet_Load(7, 4, 8, 0); Field_PartyLoad(0); Party_Join(4); 0x533E00.
extern "C" void __cdecl Scena09_Set748ReloadJoin4(void) {
    Set(7, 4, 8);
    Reload();
    Join(4);
    Palettes();
}

// original 0x51A260 (0x23 bytes; 9 A[13]): PartySet_Load(7, 4, 8, 0); Field_PartyLoad(0); Party_Join(7); 0x533E00.
extern "C" void __cdecl Scena09_Set748ReloadJoin7(void) {
    Set(7, 4, 8);
    Reload();
    Join(7);
    Palettes();
}

// original 0x51A290 (0x16 bytes; 9 B[1]): 0x534030(4); 0x534030(2); 0x533E00.
extern "C" void __cdecl Scena09_Leave42(void) {
    Leave(4);
    Leave(2);
    Palettes();
}

// original 0x51A2B0 (0x16 bytes; 9 B[3]): 0x534030(2); 0x534030(8); 0x533E00.
extern "C" void __cdecl Scena09_Leave28(void) {
    Leave(2);
    Leave(8);
    Palettes();
}

// original 0x51A2D0 (0x16 bytes; 9 B[4]): 0x534030(4); 0x534030(8); 0x533E00.
extern "C" void __cdecl Scena09_Leave48(void) {
    Leave(4);
    Leave(8);
    Palettes();
}

// original 0x51A2F0 (0xF bytes; 9 B[5], 11 B[4], 12 B[4]): 0x534030(8); 0x533E00.
extern "C" void __cdecl ScenaCall_Leave8(void) {
    Leave(8);
    Palettes();
}

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
    unsigned char* const saved = At(at::kSaved);
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
extern "C" __attribute__((naked)) void __cdecl ScenaCall_SaveAndLeave(void) {
    asm("pushl %ecx\n\t"
        "call _scena_calls_SaveAndLeave\n\t"
        "addl $4, %esp\n\t"
        "ret");
}

// original 0x51A360 (0xF bytes; 9 B[9]): 0x534030(7); 0x533E00.
extern "C" void __cdecl Scena09_Leave7(void) {
    Leave(7);
    Palettes();
}

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

// original 0x51A420 (0x9 bytes; 5 A[2], 6 A[4], 10 A[3]): Party_Join(5).
extern "C" void __cdecl ScenaCall_Join5(void) {
    Join(5);
}

// original 0x51A430 (0x55 bytes; 10 A[4]): the saved party (0x903A10..12)
// loaded, and each of the three that is not 7 joins. No count reset, no
// palettes.
//
// As the original has it: the three are passed as dwords read across a
// 6-byte local (their upper bytes are the neighbours and stale stack), and
// the joins' ids are al over the last call's eax; the callees read the byte.
extern "C" void __cdecl Scena10_RestoreSaved(void) {
    const unsigned char* const saved = At(at::kSaved);
    const unsigned char s[3] = {saved[0], saved[1], saved[2]};
    Set(s[0], s[1], s[2]);
    for (unsigned char id : s)
        if (id != 7) Join(id);
}

// original 0x51A490 (0x24 bytes; 10 B[0]): the record of
// MoveScript_EffectState[5] loses +0xB bit 0; member 5 leaves; tail 0x533E00.
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

// original 0x51A510 (0x23 bytes; 11 A[4], 12 A[4]): PartySet_Load(7, 2, 5, 0); Field_PartyLoad(0); Party_Join(5); 0x533E00.
extern "C" void __cdecl ScenaCall_Set725ReloadJoin5(void) {
    Set(7, 2, 5);
    Reload();
    Join(5);
    Palettes();
}

// original 0x51A540 (0x2A bytes; 11 A[8], 12 A[8]): PartySet_Load(7, 4, 2, 0); Field_PartyLoad(0); Party_Join(4); Party_Join(2); 0x533E00.
extern "C" void __cdecl ScenaCall_Set742ReloadJoin42(void) {
    Set(7, 4, 2);
    Reload();
    Join(4);
    Join(2);
    Palettes();
}

// original 0x51A570 (0x2A bytes; 12 A[9]): PartySet_Load(7, 5, 2, 0); Field_PartyLoad(0); Party_Join(2); Party_Join(5); 0x533E00.
extern "C" void __cdecl Scena12_Set752ReloadJoin25(void) {
    Set(7, 5, 2);
    Reload();
    Join(2);
    Join(5);
    Palettes();
}

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
    unsigned char* const saved = At(at::kSaved);
    const unsigned char* const lists = At(at::kPartyLists);
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
    unsigned char* const saved = At(at::kSaved);
    const unsigned char s[3] = {saved[0], saved[1], saved[2]};
    Set(s[0], s[1], s[2]);
    Field_MemberCount = 0;
    Join(s[0]);
    Join(s[1]);
    Join(s[2]);
    unsigned char* const lists = At(at::kPartyLists);
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
    for (unsigned i = 0; i < 3; ++i) {
        const unsigned char id = MemberId(i);
        if (id != 7 && id != 4) At(at::kSaved)[0] = id;
        ids[i] = id;
    }
    for (unsigned char id : ids) Leave(id);
    Set(0xA, 0xFF, 0xFF);
    Field_MemberCount = 0;
    Join(0xA);
    Palettes();
}

// original 0x51A890 (0x46 bytes; 14 A[4]): the record of
// MoveScript_EffectState[10] (Char_WhelpSlot) loses +0xB bit 0 before the
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
    unsigned char* const lists = At(at::kPartyLists);
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
    Set(7, 4, At(at::kSaved)[0]);
    Field_MemberCount = 0;
    Join(7);
    Join(4);
    Join(At(at::kSaved)[0]);
    Palettes();
}

// original 0x51A9B0 (0x5D bytes; 14 A[9]): the three field members' ids saved
// in 0x903A10..12, each sent away (read from there after the last call, 7
// too), the set (7, 8, 4) and member 7; 0x533E00.
extern "C" void __cdecl Scena14_SaveLeaveAllParty7(void) {
    unsigned char* const saved = At(at::kSaved);
    for (unsigned i = 0; i < 3; ++i) saved[i] = MemberId(i);
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
    const unsigned char* const saved = At(at::kSaved);
    unsigned char k;
    unsigned i = 0;
    for (;;) {
        k = saved[i];
        if (k != 7 && k != 4) break;
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
extern "C" __attribute__((naked)) void __cdecl Scena15_LeaveAllParty7(void) {
    asm("pushl %ecx\n\t"
        "call _scena_calls_LeaveAllParty7\n\t"
        "addl $4, %esp\n\t"
        "ret");
}

// original 0x51ABC0 (0x16 bytes; 15 A[1]): Party_Join(4); Party_Join(2); 0x533E00.
extern "C" void __cdecl Scena15_Join42(void) {
    Join(4);
    Join(2);
    Palettes();
}

// original 0x51ABE0 (0x31 bytes; 15 A[2]): PartySet_Load(7, 8, 2, 0); Field_MemberCount 0; Party_Join(7); Party_Join(8); Party_Join(2); 0x533E00.
extern "C" void __cdecl Scena15_Party782(void) {
    Set(7, 8, 2);
    Field_MemberCount = 0;
    Join(7);
    Join(8);
    Join(2);
    Palettes();
}

// original 0x51AC20 (0x2D bytes; 17 A[0], 18 A[0], 19 A[0]): PartySet_Load(0, 3, 4, 0); Field_MemberCount 0; Party_Join(0); Party_Join(3); Party_Join(4).
extern "C" void __cdecl ScenaCall_Party034(void) {
    Set(0, 3, 4);
    Field_MemberCount = 0;
    Join(0);
    Join(3);
    Join(4);
}

void ScenaCalls_Inject() {
    if (bof3::WantsShadow("scena_calls")) scena_calls::SelfTest();
    BOF3_INJECT(Scena01_Set934Party9);
    BOF3_INJECT(Scena01_Party034);
    BOF3_INJECT(ScenaCall_Join4);
    BOF3_INJECT(ScenaCall_Leave4);
    BOF3_INJECT(Scena02_Leave43);
    BOF3_INJECT(Scena03_Set012Party0);
    BOF3_INJECT(ScenaCall_Join1);
    BOF3_INJECT(ScenaCall_Leave1);
    BOF3_INJECT(Scena04_Set015Party01);
    BOF3_INJECT(ScenaCall_Party015);
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
    BOF3_INJECT(Scena06_Set015ReloadJoin05);
    BOF3_INJECT(Scena06_Set015ReloadJoin5);
    BOF3_INJECT(Scena06_Set015ReloadJoin1);
    BOF3_INJECT(Scena06_Set015ReloadJoin15);
    BOF3_INJECT(ScenaCall_Join6);
    BOF3_INJECT(ScenaCall_Set012ReloadJoin1);
    BOF3_INJECT(Scena06_Set012ReloadJoin2);
    BOF3_INJECT(Scena06_Set012ReloadJoin12);
    BOF3_INJECT(Scena06_Leave02);
    BOF3_INJECT(Scena06_Leave05);
    BOF3_INJECT(Scena06_Leave06);
    BOF3_INJECT(ScenaCall_Leave2);
    BOF3_INJECT(Scena06_Leave62);
    BOF3_INJECT(Scena06_Leave56);
    BOF3_INJECT(Scena07_Party012);
    BOF3_INJECT(Scena07_LeaveThird);
    BOF3_INJECT(Scena07_Leave2Out1562);
    BOF3_INJECT(Scena08_Set72APartyA);
    BOF3_INJECT(Scena08_Party7);
    BOF3_INJECT(Scena08_Set724Party72);
    BOF3_INJECT(ScenaCall_Party728);
    BOF3_INJECT(Scena08_SetParty784);
    BOF3_INJECT(Scena08_Leave2);
    BOF3_INJECT(Scena09_Set782ReloadJoin8);
    BOF3_INJECT(Scena09_Set782ReloadJoin2);
    BOF3_INJECT(ScenaCall_Set785ReloadJoin85);
    BOF3_INJECT(ScenaCall_Set785ReloadJoin5);
    BOF3_INJECT(ScenaCall_Set785ReloadJoin8);
    BOF3_INJECT(Scena09_ReloadJoin6);
    BOF3_INJECT(ScenaCall_Set765ReloadJoin56);
    BOF3_INJECT(ScenaCall_Set765ReloadJoin6);
    BOF3_INJECT(Scena09_Set765ReloadJoin5);
    BOF3_INJECT(ScenaCall_Party72Saved);
    BOF3_INJECT(Scena09_Set748ReloadJoin8);
    BOF3_INJECT(Scena09_Set748ReloadJoin48);
    BOF3_INJECT(Scena09_Set748ReloadJoin4);
    BOF3_INJECT(Scena09_Set748ReloadJoin7);
    BOF3_INJECT(Scena09_Leave42);
    BOF3_INJECT(Scena09_Leave28);
    BOF3_INJECT(Scena09_Leave48);
    BOF3_INJECT(ScenaCall_Leave8);
    BOF3_INJECT(ScenaCall_SaveAndLeave);
    BOF3_INJECT(Scena09_Leave7);
    BOF3_INJECT(Scena10_Party728);
    BOF3_INJECT(Scena10_Party75);
    BOF3_INJECT(Scena10_Join8);
    BOF3_INJECT(ScenaCall_Join5);
    BOF3_INJECT(Scena10_RestoreSaved);
    BOF3_INJECT(Scena10_Leave5);
    BOF3_INJECT(Scena10_LeaveAll);
    BOF3_INJECT(ScenaCall_Set725ReloadJoin5);
    BOF3_INJECT(ScenaCall_Set742ReloadJoin42);
    BOF3_INJECT(Scena12_Set752ReloadJoin25);
    BOF3_INJECT(Scena13_Party765);
    BOF3_INJECT(Scena13_SaveParty7);
    BOF3_INJECT(Scena13_Set758Party7);
    BOF3_INJECT(Scena13_RestoreSaved);
    BOF3_INJECT(Scena13_Party758);
    BOF3_INJECT(Scena13_LeaveAllParty7);
    BOF3_INJECT(Scena13_Join4);
    BOF3_INJECT(Scena13_Join2);
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
    BOF3_INJECT(Scena15_LeaveAllParty7);
    BOF3_INJECT(Scena15_Join42);
    BOF3_INJECT(Scena15_Party782);
    BOF3_INJECT(ScenaCall_Party034);
}

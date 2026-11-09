// World 4's areas 152..155, 166 and 167: the code of the PSX's
// BIN/WORLD04/AREA152..167.EMI compiled into the exe at 0x4249D0..0x42655C -
// 48 functions, each read to its last instruction with capstone (2026-09-28)
// and taken through the area harness (area_harness.h). Areas 156..165 have no
// code in the band. Round ten group AR4A; docs/area_w4a.md has the areas one
// section each.
//
// Area 152 is the world map's eleventh and last copy (WorldMap_Records record
// 10): area 87's code (docs/area_w2b.md section 4, itself area 45's)
// instruction for instruction over area 152's tables, but for the plate's
// animation bank (0x1D1) and a field hook of its own - area 121's short shape
// (no cell test, no name sets) opening the message of the place's row by the
// chapter byte, as area 87's does on a place cell. Its init clears six story
// flags; its block also holds 0x4253C0, the record +8 effect's state 0 that
// all ten world maps' record-8 tables name (every 1024 frames, up to three
// kind-0x16 effects at the leader's ground). Areas 153 and 154: one choice
// each (a byte pair by the answer into the focus object). Area 155: a choice
// that gives an item, and the choice seven areas share (arm tail kind 10).
// Area 166: a pair choice that also arms tail kind 10. Area 167: four choices
// that arm its tail kind 32, eleven handlers (heights and flags 0x48..0x4C by
// Cond_ByteFD), the tail kind itself (party drop-ins, area changes, two
// countdowns), an arrive hook, a one-switch cell hook, an init.
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. The
// dispatchers through area 152's .data state tables abort past the table,
// where the original would jump through whatever the next dwords hold, and
// 0x4253C0 aborts on an effect slot past Effect_Objects' twenty (the owner's
// rule for an unchecked index, round9 doc section 6; no route reaches any).
// The out-of-table reads that land in mapped memory are reproduced as the
// original makes them (docs/area_w4a.md section 8). Every call goes through
// the harness (AH_CALL / AH_AT), so the start-up fuzz can stand recorders in
// for the callees.
#include "game/area_w4a.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w4a_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/world_map.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "hook/code_bytes.h"

namespace {

namespace at = area_w4a::at;
using U = std::uint32_t;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

unsigned char& B(U address) { return move_script::At(address)[0]; }
unsigned char* Bank() { return At(::bof3::Img(Cond_Flags_addr) + 0xa0); }
void Clear(unsigned index) { AH_CALL(Flags_Clear)(Bank(), index); }
void SetMessage(unsigned v) { SetWord(At(::bof3::Img(at::kMessage.v)), v); }
std::int32_t Answer() { return AreaW0c_ChoiceAnswer; }
unsigned char* Focus() { return reinterpret_cast<unsigned char*>(AreaW1a_FocusObject); }
void ArmTail(unsigned kind, unsigned state) {
    AreaW0a_TailKind = static_cast<unsigned char>(kind);
    AreaW0c_TailState = static_cast<unsigned char>(state);
}

}  // namespace

// ===========================================================================
// Area 152: its init, and the world map's eleventh copy (WorldMap_Records
// record 10; area 87's code, docs/area_w2b.md section 4; area 45's in
// docs/area_w1b.md section 5; area 33's in docs/worldmap_area.md sections 1..4)
// ===========================================================================

// ===========================================================================
// Areas 153 and 154: one choice each (their inits are 0x437CC0, a bare ret)
// ===========================================================================

namespace {
// The focus object's dwords +0x18 / +0x1C = the byte pair pairs[s8 answer]
// (zero-extended; the pointer and the answer read again for the second;
// unchecked), the message 0xFFFF between the two reads of the first.
void FocusPair(U pairs) {
    unsigned char* focus = Focus();
    const unsigned first = B(pairs + static_cast<U>(Answer()) * 2u);
    SetMessage(0xFFFF);
    SetLong(focus + 0x18, static_cast<std::int32_t>(first));
    focus = Focus();
    SetLong(focus + 0x1C, B(pairs + static_cast<U>(Answer()) * 2u + 1));
}
}  // namespace

// ===========================================================================
// Area 155 (descriptor 0x638C80: a choice table only)
// ===========================================================================

// ===========================================================================
// Area 166 (descriptor 0x63A758: a choice table only; choice 0 is
// Area155_ChoiceArmTail10)
// ===========================================================================

// ===========================================================================
// Area 167 (descriptor 0x63C550: choices 0x63C510, running on into the
// handlers 0x63C524 - choices 5..15 are handlers 0..10)
// ===========================================================================

// original 0x4260F0 (Field_ModeTailKinds[32] 0x662D68; armed by area 167's
// choices, arrive hook, cell hook and init): a switch on the s8 0x9039F4 -
// above 0x20 (unsigned) nothing - through the byte table 0x426440 in the code
// (33 cases, 0..0xD; ours holds it as the switch it was compiled from,
// Tail32Case, docs/operand-store.md) and a jump table of fourteen:
//   0: Party_DropIn(0), state 1.
//   1: variable 6 0x18: kind, state 0; flag 0x46 set; Field_ChangeArea(0x95,
//      0x80000, 0xC0000, 0x81).
//   5: Sound_PlayEffect(0x204), state 6, the countdown 0x9039F6 = 0xA.
//   6: the countdown - 1; at 0 ScriptFlags_Clear40, kind and state 0.
//   0xA: variable 6 0x64: Party_DropIn(5), state 0xB.
//   0xB: variable 6 0x74: Field_ChangeArea(0xA7, 0x130000, 0x9A0000, 0x87)
//      with Cond_ByteFD 2, else (0xA7, 0xA0000, 0x1E0000, 0x86); flag 0x4B
//      toggled, 0x4C set, state 0xC.
//   0xC: variable 6 0: Sound_PlayEffect(0x206) with Cond_ByteFD 0; variable 5
//      = 0, kind and state 0.
//   0x14: variable 6 0x44: Party_DropIn(2), state 0x18.
//   0x15: Party_DropIn(8), state 0x18.
//   0x18: variable 6 0x54: by variable 5 (read once) - 0: variable 6 = 0,
//      ScriptFlags_Clear40, Field_ChangeArea(0xA7, 0x1A8000, 0x560000, 5),
//      flags 0x48 and 0x49 cleared; 1: (0xA7, 0x200000, 0xF0000, 0x83), 0x48
//      set, 0x49 cleared; 2: (0xA7, 0x290000, 0x8B0000, 0x84), 0x48 cleared,
//      0x49 set. Then Sound_PlayEffect(0x205) with Cond_ByteFD 0, flag 0x4A
//      set, state 0xC.
//   0x1E: state 0x1F.
//   0x1F: Transition_Busy 0: Cond_ByteFE = 0x20, the countdown 0x1E,
//      state 0x20.
//   0x20: the countdown - 1; at 0 ScriptFlags_Clear40, flag 0x90 set,
//      Field_ChangeArea(0xA8, 0x150000, 0x40000, 7), Field_ScriptFlags2 bit 6
//      set (a byte or), kind and state 0.
// The other states (2..4, 7..9, 0xD..0x13, 0x16, 0x17, 0x19..0x1D) do nothing.
namespace {
// Tail kind 32's case by state: the switch the compiler turned into the
// 33-byte index at at::kTail32Index - this function's logic, ours since the
// function is (the brief of the disc-only round, docs/operand-store.md). 13
// is the cases that do nothing. AreaW4a_Inject registers the bytes this
// gives against the index for the hosted check.
unsigned Tail32Case(std::int32_t state) {
    switch (state) {
    case 0x00: return 0;
    case 0x01: return 1;
    case 0x05: return 2;
    case 0x06: return 3;
    case 0x0A: return 4;
    case 0x0B: return 5;
    case 0x0C: return 6;
    case 0x14: return 7;
    case 0x15: return 8;
    case 0x18: return 9;
    case 0x1E: return 10;
    case 0x1F: return 11;
    case 0x20: return 12;
    default: return 13;
    }
}
}  // namespace

// ===========================================================================

void AreaW4a_Inject() {
    {
        static std::uint8_t index[0x21];   // Tail32Case as the compiler's byte index, for the hosted check
        for (std::int32_t s = 0; s < 0x21; ++s) index[s] = static_cast<std::uint8_t>(Tail32Case(s));
        bof3::Operand("Area167_Tail32 case index", at::kTail32Index, index, sizeof index);
    }
    if (bof3::WantsShadow("area_w4a")) area_w4a::SelfTest();
}

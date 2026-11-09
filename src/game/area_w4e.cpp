// World 4's areas 188..191: the PSX's BIN/WORLD04/AREA188..191.EMI compiled
// into the exe at 0x42A320..0x42BD60 (Area_Descriptors entries 188..191; 190
// has no code). Round ten, group AR4E: the band's 51 functions, none ours
// before, each read to its last instruction with capstone (2026-09-28) and
// taken through the area harness (area_harness.h). docs/area_w4e.md.
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. The
// dispatchers through area 189's five leader states and area 191's three
// scale states abort past their tables (what follows is data, which the
// original would jump into), and area 189's step divides by a byte the
// original never lets be 0 but does not check (ours aborts there); every other
// unchecked read stays inside mapped memory and is kept (docs/area_w4e.md
// section 6). Every call goes through the harness (AH_CALL / AH_AT), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies;
// the group's own callees are called the same way, so each function is fuzzed
// alone.
#include "game/area_w4e.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w4e_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w4e::at;
using area_harness::Handler;
using area_harness::Mem;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using U = std::uint32_t;

unsigned char& B(U address) { return *Mem(address); }
void AddWord(unsigned char* p, int v) { SetWord(p, static_cast<unsigned>(Word(p) + v)); }
unsigned char* Ptr(U cell) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(cell))))); }
unsigned char* Cur() { return Sprite_Current; }

void SetMessage(unsigned id) { SetWord(Mem(::bof3::Img(at::kMessage.v)), id); }
unsigned char Answer() { return B(::bof3::Img(AreaW0c_ChoiceAnswer_addr)); }
unsigned char* StoryFlags() { return Mem(::bof3::Img(Cond_Flags_addr) + 0xa0); }
unsigned char* Row14() { return Mem(::bof3::Img(Cond_Flags_addr) + 0x70); }
bool Test(const unsigned char* bits, unsigned index) { return AH_CALL(Flags_Test)(bits, index) != 0; }
// The movement script's position (MoveScript_Object's word +0xA) back by 2.
void ScriptBack2() { AddWord(MoveScript_Object + 0xA, -2); }

// A state handler read from an area's .data table in place, as the
// originals' `jmp [index * 4 + table]`: the index is not checked there. Ours
// aborts past the table (what follows is data).
Handler StateEntry(const char* who, U table, unsigned count, unsigned index) {
    if (index >= count) bof3::Fatal("%s: state %u is past its %u-entry table 0x%X", who, index, count, static_cast<unsigned>(table));
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(table + index * 4u)))));
}

// The tail armed from a choice: ScriptFlags_Set40, then kind and state (the
// sub-kind left as it is).
void ArmTail(unsigned char kind, unsigned char state) {
    AH_CALL(ScriptFlags_Set40)();
    B(::bof3::Img(AreaW0a_TailKind_addr)) = kind;
    B(::bof3::Img(AreaW0c_TailState_addr)) = state;
}

// Areas 188 and 191's choice that stores a byte pair at the focus object
// (0x42A3E0's first half, 0x42B570): the answer read as s8 indexes the pairs
// (unchecked: a negative answer reads before the table, in .data); +0x18 the
// first byte, +0x1C the second, both dwords; the focus pointer and the answer
// read again between; the message word 0xFFFF.
void FocusPair(U pairs) {
    const auto a = static_cast<signed char>(Answer());
    unsigned char* focus = Ptr(::bof3::Img(AreaW1a_FocusObject_addr));
    const unsigned char first = B(pairs + static_cast<U>(a * 2));
    SetMessage(0xFFFF);
    SetLong(focus + 0x18, first);
    focus = Ptr(::bof3::Img(AreaW1a_FocusObject_addr));
    const auto a2 = static_cast<signed char>(Answer());
    SetLong(focus + 0x1C, B(pairs + 1 + static_cast<U>(a2 * 2)));
}

}  // namespace

// ===========================================================================
// Area 188 (descriptor 0x6474E0; PSX 0x801F6848)
// ===========================================================================

// ===========================================================================
// Area 189 (descriptor 0x647558; PSX 0x801F62AC)
// ===========================================================================

// ===========================================================================
// Area 191 (descriptor 0x647BC8; PSX 0x801F418C)
// ===========================================================================

void AreaW4e_Inject() {
    if (bof3::WantsShadow("area_w4e")) area_w4e::SelfTest();
}

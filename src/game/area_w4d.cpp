// World 4's areas 175..187: the PSX's BIN/WORLD04/AREA175..187.EMI compiled
// into the exe at 0x4292C0..0x42A320 (Area_Descriptors entries 175..187).
// Round ten, group AR4D: the band's 49 functions, none ours before, each read
// to its last instruction with capstone (2026-09-28) and taken through the
// area harness (area_harness.h). docs/area_w4d.md.
//
// Areas 175..185 are one body of code eleven times over: their descriptors
// name the same init, the same two handlers and the same choice table but for
// entry 27, and Area_StepHook / Area_CellHooks name one step hook and one cell
// hook for all eleven. Each area's choice 27 is its own copy, byte for byte
// the same but for its jump table's address; ours is one body behind eleven
// names. The shared bodies are named for the area whose block holds them (the
// tool's unit).
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. Reads
// by an unchecked index into the areas' .data tables are kept where they stay
// in .data; the dispatcher through area 175's glide states aborts past its two
// entries, and area 175's script message aborts on an area number past
// Area_Descriptors (docs/area_w4d.md section 6). Every call goes through the
// harness (AH_CALL / AH_AT), so the start-up fuzz can stand recorders in for
// ours as for the originals' copies; the group's own callee (area 186's start)
// is called the same way, so each function is fuzzed alone.
#include "game/area_w4d.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w4d_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w4d::at;
using area_harness::Mem;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using U = std::uint32_t;

unsigned char& B(U address) { return *Mem(address); }
void AddWord(unsigned char* p, int v) { SetWord(p, static_cast<unsigned>(Word(p) + v)); }
void AddLong(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(static_cast<U>(Long(p)) + v)); }
unsigned char* Ptr(U cell) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(cell))))); }

void SetMessage(unsigned id) { SetWord(Mem(::bof3::Img(at::kMessage.v)), id); }
unsigned char Answer() { return B(::bof3::Img(AreaW0c_ChoiceAnswer_addr)); }
unsigned char* StoryFlags() { return Mem(::bof3::Img(Cond_Flags_addr) + 0xa0); }
// The focus object's trigger id cleared (0xFF): the focus read before the
// message store, as the originals do.
void DropFocusTrigger(unsigned message) {
    unsigned char* const focus = Ptr(::bof3::Img(AreaW1a_FocusObject_addr));
    SetMessage(message);
    focus[0x86] = 0xFF;
}

// Areas 175..185's choice 27 (eleven copies): the message by the answer read
// signed - 0xFA, 0xFB, 0xFC, 0xFD for 0..3, 0xF9 for 4; anything else (a
// negative answer is above 4 unsigned) leaves the message word alone.
void ChoiceMessageByAnswer() {
    static const std::uint16_t kMessages[5] = {0xFA, 0xFB, 0xFC, 0xFD, 0xF9};
    const auto answer = static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(Answer())));
    if (answer > 4) return;
    SetMessage(kMessages[answer]);
}

// Areas 175 and 184's Zenny question (choices 10 and 14): the answer and
// Party_Zenny at 500 or more (unsigned) pick a message and a byte.
bool HasZenny500() { return static_cast<U>(Long(Mem(::bof3::Img(Party_Zenny_addr)))) >= 0x1F4; }

// Field_ChangeArea to the return point (its area word, x, z) with flags 4.
void ChangeToReturnPoint() {
    AH_CALL(Field_ChangeArea)(Word(Mem(::bof3::Img(AreaW4d_ReturnArea_addr))), Long(Mem(::bof3::Img(AreaW2d_ReturnPoint_addr))), Long(Mem(::bof3::Img(AreaW4d_ReturnZ_addr))), 4);
}

}  // namespace

// ===========================================================================
// Areas 175..185's shared body (descriptors 0x642450 .. 0x644628; PSX
// 0x801F531C for 175, 0x801F376C .. 0x801F3A44 for 176..185)
// ===========================================================================

// ===========================================================================
// Area 175's own (descriptor 0x642450; PSX 0x801F531C): handlers 2..7
// (choices 31..36), its glide states, object trigger 64
// ===========================================================================

// ===========================================================================
// Area 186 (descriptor 0x645228; PSX 0x801F3B34): the init, its start, tail
// kind 39
// ===========================================================================

// ===========================================================================
// Area 187 (descriptor 0x645A70; no PSX pairing): choice 0 (choice 1 is
// Area130_ChoiceTailState2, AR3C's, read in place), object trigger 14, tail
// kind 29
// ===========================================================================

void AreaW4d_Inject() {
    if (bof3::WantsShadow("area_w4d")) area_w4d::SelfTest();
}

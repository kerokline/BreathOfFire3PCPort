// World 2's areas 108 and 110..113: the PSX's BIN/WORLD02/AREA108 and
// AREA110..113.EMI compiled into the exe at 0x4168E0..0x418BE0
// (Area_Descriptors entries 108..113; 109 has no code). Round ten, group AR2F:
// the band's 53 functions, none ours before, each read to its last
// instruction with capstone (2026-09-28) and taken through the area harness
// (area_harness.h). docs/area_w2f.md.
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. Reads
// by an unchecked index that stay inside the image's .data are kept (area
// 108's place tables and Character_IndexById by a byte, area 111's grid by
// the leader's cell, the extra objects by a byte); the dispatchers through
// the areas' state tables abort past the table (what follows is data, which
// the original would jump into), area 111's handler 8 aborts where its dword
// index would read outside the image, and its handler 5 aborts where its
// final write would land outside the 28-byte grid (docs/area_w2f.md section
// 6). Every call goes through the harness (AH_CALL / AH_AT), so the start-up
// fuzz can stand recorders in for ours as for the originals' copies; the
// callees of the group's own are called the same way, so each function is
// fuzzed alone.
#include "game/area_w2f.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w2f_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w2f::at;
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
// Sprite_ObjectsExtra record k (0xA4 bytes; k not checked, as the originals).
unsigned char* Extra(unsigned k) { return Mem(::bof3::Img(Sprite_ObjectsExtra_addr) + k * at::kExtraStride); }
// Party record m (ObjTrio, 0x14C bytes; m not checked).
unsigned char* Record(unsigned m) { return Mem(::bof3::Img(ObjTrio_addr) + m * at::kPartyStride); }

void SetMessage(unsigned id) { SetWord(Mem(::bof3::Img(at::kMessage.v)), id); }
unsigned char* StoryFlags() { return Mem(::bof3::Img(Cond_Flags_addr) + 0xa0); }
bool Test(unsigned index) { return AH_CALL(Flags_Test)(StoryFlags(), index) != 0; }
void Set(unsigned index) { AH_CALL(Flags_Set)(StoryFlags(), index); }
void Clear(unsigned index) { AH_CALL(Flags_Clear)(StoryFlags(), index); }
// The tail's word timer counted down (a 16-bit dec): the new value.
unsigned DecTimer() {
    const unsigned v = (Word(Mem(::bof3::Img(at::kTailTimer.v))) - 1u) & 0xFFFFu;
    SetWord(Mem(::bof3::Img(at::kTailTimer.v)), v);
    return v;
}
// The movement script's position (MoveScript_Object's word +0xA) moved by v.
void ScriptStep(int v) { AddWord(MoveScript_Object + 0xA, v); }

// A state handler read from an area's .data table in place, as the
// originals' `jmp [index * 4 + table]`: the index is not checked there. Ours
// aborts past the table (what follows is data).
Handler StateEntry(const char* who, U table, unsigned count, unsigned index) {
    if (index >= count)
        bof3::Fatal("%s: state %u is past its %u-entry table 0x%X", who, index, count, static_cast<unsigned>(table));
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(table + index * 4u)))));
}

// The map setters as the originals pass them: x and z words, a value byte.
void SetByte(unsigned x, unsigned z, unsigned value) { AH_CALL(AreaMap_SetByte)(x, z, value); }

// ---- area 111's grid ----

// ---- area 108 ----

// ---- area 110 ----

}  // namespace

// ===========================================================================
// Area 108 (descriptor 0x61E598): nineteen choices, sixteen handlers (the
// handlers are choices 3..18 but 2), a cell hook, tail kind 12, and a helper.
// Choice 2 is area 22's Area22_ArmTailOnYes, choice 15 = handler 12 area 67's
// Area67_SetBit24, choices 16, 17 = handlers 13, 14 area 57's
// Area57_StopMusic / Area57_ResumeSound; +0x40 is the shared ret 0x437CC0.
// ===========================================================================

// ===========================================================================
// Area 110 (descriptor 0x61E808): its init.
// ===========================================================================

// ===========================================================================
// Area 111 (descriptor 0x61FB38): eleven choices (choices 2..10 are handlers
// 0..8), tail kind 17, the arrive hook, the init, two helpers. The handlers
// work a 7 x 7 grid of nibbles (Area111_Grid) over the map cells from x 0xB,
// z 0xB1, two map cells a grid cell.
// ===========================================================================

// ===========================================================================
// Area 112 (descriptor 0x61FF30): three choices (handlers 3..5), handlers 0
// and 2 (handler 1 is 0x421FB0, another block's), the step and cell hooks,
// the cell hook's helper, and effect kind 0xB9's handler with its ring state
// (its state 0 is area 59's Area59_EffectGround).
// ===========================================================================

// ===========================================================================
// Area 113 (descriptor 0x620010): five choices (choices 1, 2 are 0x420850 /
// 0x420870, another block's; 3, 4 area 61's Area61_ChoiceMark4), object
// triggers 27, 39 and 63, tail kind 62 and its reward helper. No handlers.
// ===========================================================================

void AreaW2f_Inject() {
    if (bof3::WantsShadow("area_w2f")) area_w2f::SelfTest();
}

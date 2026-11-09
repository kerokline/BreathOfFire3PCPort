// World 2's areas 95..100 and 103: the PSX's BIN/WORLD02/AREA095..100 and
// AREA103.EMI compiled into the exe at 0x4135B0..0x4146C0 (Area_Descriptors
// entries 95..103; 101 and 102 have no code here). Round ten, group AR2D: the
// band's 53 functions, none ours before, each read to its last instruction
// with capstone (2026-09-28) and taken through the area harness
// (area_harness.h). docs/area_w2d.md.
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. Reads
// by an unchecked index into the areas' .data tables are kept where they stay
// in .data; the dispatchers through area 99's and area 100's state tables
// abort past the table (what follows is data, which the original would jump
// into), and the two divides by a byte that can be 0 abort where the original
// faults (docs/area_w2d.md section 6). Every call goes through the harness
// (AH_CALL / AH_AT), so the start-up fuzz can stand recorders in for ours as
// for the originals' copies; the callees of the group's own are called the
// same way, so each function is fuzzed alone.
#include "game/area_w2d.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w2d_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w2d::at;
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
// Effect_Objects record `slot` (0x80 bytes; the slot is not checked, as the
// originals' `shl 7`).
unsigned char* EffectAt(unsigned slot) { return rv::Bytes(&Effect_Objects[slot]); }
// The high word of a 16.16 position, as the hooks read it (a word at +2).
std::uint16_t High(long v) { return static_cast<std::uint16_t>(static_cast<U>(v) >> 16); }

void SetMessage(unsigned id) { SetWord(Mem(::bof3::Img(at::kMessage.v)), id); }
unsigned char* StoryFlags() { return Mem(::bof3::Img(Cond_Flags_addr) + 0xa0); }
// The chapter's flag row (a pointer the chapters keep at 0x929ED0).
unsigned char* FlagRow() { return Ptr(::bof3::Img(Chapter_FlagRow_addr)); }

// A state handler read from an area's .data table in place, as the
// originals' `jmp [index * 4 + table]`: the index is not checked there. Ours
// aborts past the table (what follows is data).
Handler StateEntry(const char* who, U table, unsigned count, unsigned index) {
    if (index >= count)
        bof3::Fatal("%s: state %u is past its %u-entry table 0x%X", who, index, count, static_cast<unsigned>(table));
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(table + index * 4u)))));
}

// The map byte setter, as the originals pass it: x and z words, a value byte.
void SetByte(unsigned x, unsigned z, unsigned value) { AH_CALL(AreaMap_SetByte)(x, z, value); }

// The movement script's position (MoveScript_Object's word +0xA) moved by `v`.
void ScriptStep(int v) { AddWord(MoveScript_Object + 0xA, v); }
// Field_ActiveMember's word +0x8A less 2.
void MemberStep() { AddWord(Field_ActiveMember + 0x8A, -2); }

// The tail armed: ScriptFlags_Set40, kind `kind`, state 0.
void ArmTail(unsigned char kind) {
    AH_CALL(ScriptFlags_Set40)();
    B(::bof3::Img(AreaW0a_TailKind_addr)) = kind;
    B(::bof3::Img(AreaW0c_TailState_addr)) = 0;
}
// The tail disarmed: ScriptFlags_Clear40, kind and state 0.
void Disarm() {
    AH_CALL(ScriptFlags_Clear40)();
    B(::bof3::Img(AreaW0a_TailKind_addr)) = 0;
    B(::bof3::Img(AreaW0c_TailState_addr)) = 0;
}

// Areas 95 and 96's handler 4 (0x4135B0, 0x413860, byte for byte the same):
// the running object at z word 0x3A and x word 0x40 turns to 3, at x 0x42 to
// 7, and runs Area95_TurnAndMove (the originals' tail jmp); anywhere else
// the script position + 2.
void TurnAtCell() {
    unsigned char* const cur = Sprite_Current;
    if (rv::As<records::SpriteObject>(cur)->z_whole == 0x3A) {
        const auto d = static_cast<std::uint16_t>(rv::As<records::SpriteObject>(cur)->x_whole - 0x40);
        if (d == 0 || d == 2) {
            cur[8] = d == 0 ? 3 : 7;
            AH_CALL(Area95_TurnAndMove)();
            return;
        }
    }
    ScriptStep(2);
}

}  // namespace

// ===========================================================================
// Areas 95 and 96 (descriptors 0x617588, 0x617D48): eight handlers each, the
// same bodies but handler 4 (two identical copies, one in each block).
// ===========================================================================

// ===========================================================================
// Area 97 (descriptor 0x617E58: no tables): a step hook arming tail kind 13,
// that tail, and the world-map field hook for "no world map".
// ===========================================================================

// ===========================================================================
// Area 98 (descriptor 0x6183B0): six choices (four its own), seven handlers
// (handlers 1..6 are choices 0..5), the init, tail kind 45, object trigger 44,
// and two camera handlers areas 39, 41 and 99 name.
// ===========================================================================

// ===========================================================================
// Area 99 (descriptor 0x619BF8): six handlers (0 and 1 area 98's block's),
// two two-state machines in its .data, the init.
// ===========================================================================

// ===========================================================================
// Area 100 (descriptor 0x61AAF0): one choice (handler 8), nine handlers (1
// and 5 other blocks'), the init, a step hook, tail kind 28, object triggers
// 13, 16 and 30, effect kind 0xB7's handler and its draw state.
// ===========================================================================

// ===========================================================================
// Area 103 (descriptor 0x61B4A0): six handlers and their helper, object
// trigger 35.
// ===========================================================================

void AreaW2d_Inject() {
    if (bof3::WantsShadow("area_w2d")) area_w2d::SelfTest();
}

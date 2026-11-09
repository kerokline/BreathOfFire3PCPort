// Group FC3 of round twelve (wave two, the field side): 63 functions of the
// field engine's resident code, 0x5172C0..0x5195F9 and 0x525390..0x526DAF -
// analysis/round12_cut.tsv's 57 rows for FC3, 0x5254A0 (docs/scenario_harness.md
// section 7.6) and five dispatchers no list had (FieldCore_State2Steps entries
// 4..8, docs/field_c3.md section 1) - each read to its last instruction with
// capstone (2026-09-29) and taken through the scenario harness's field mode
// (scenario_harness.h, Group::field). docs/field_c3.md has them one row each.
//
//   object steering        op E7, the flee / chase directions and the best
//                          of four turns, the fade cases of Field_ObjectFadeOut
//                          / FadeIn
//   two mode frames        mode 11's frame, mode 8's steps 5 and 8
//   the field core         FieldCore_State2Steps' sub-states 0 (the move
//                          script's walk), 1 (attached to an extra object), 3
//                          (the hop), 4 (the vertical moves), 5 (the jump out
//                          and in), 6 (the 0xD0 cells), 7 (the fall), 8 (the
//                          recoil), with every step of theirs
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. The
// dispatchers abort past their tables (the original jumps through the dword
// after, another table's entry), and the hop's frame count aborts on a speed
// of 0 (the original's idiv faults) - the owner's rule for an unchecked index
// and a divide, round9 doc section 6; docs/field_c3.md section 6. Every call
// goes through the harness (SH_CALL / SH_AT), so the start-up fuzz can stand
// recorders in for the callees; the callees of other groups of this wave are
// raw addresses in field_c3_callees.h.
//
// Sprite_Current, Field_State and Field_ActiveMember are re-read after every
// call, as the originals re-read them, and held where the original holds them
// in a register. A value the original pushes with a register's leftover upper
// bytes goes out as the byte or word its callee reads (each callee read,
// docs/field_c3.md section 3).
#include "game/field_c3.h"

#include <cstdint>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/field_c3_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = field_c3::at;
using U = std::uint32_t;
// move_script::At is named in full here (step B0, analysis/tu_renames.tsv)
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

unsigned char* S() { return Sprite_Current; }

U ULong(const unsigned char* p) { return static_cast<U>(move_script::Long(p)); }
void SetULong(unsigned char* p, U v) { move_script::SetLong(p, static_cast<std::int32_t>(v)); }
short SWord(const unsigned char* p) { return static_cast<short>(move_script::Word(p)); }
template <typename T> U AddressOf(T* p) { return Low32(p); }

U DirStepX(unsigned d) { return ULong(move_script::At(AddressOf(Field_DirectionSteps) + d * 8)); }
U DirStepZ(unsigned d) { return ULong(move_script::At(AddressOf(Field_DirectionSteps) + 4 + d * 8)); }

}  // namespace

// ===========================================================================
// The mode frames
// ===========================================================================

// ===========================================================================
// Object steering
// ===========================================================================

// The fade cases (Field_ObjectFadeOutSteps / Field_ObjectFadeInSteps). Ours'
// Field_ObjectFadeOut / FadeIn (object_kinds.cpp) run the same code inline;
// these are the starts the tables name. The tint record is the one
// Sprite_SetTint's result names, kept in Field_ActiveMember +0x9F.
// ===========================================================================
// The field core, state 2: sub-state 0, the move script's walk
// ===========================================================================

// ===========================================================================
// Sub-state 1: attached to an extra object
// ===========================================================================

// ===========================================================================
// Sub-state 3: the hop
// ===========================================================================

// ===========================================================================
// Sub-state 4: the vertical moves
// ===========================================================================

// ===========================================================================
// Sub-state 5: the jump out and back in
// ===========================================================================

// ===========================================================================
// Sub-state 6: the 0xD0 cells
// ===========================================================================

// ===========================================================================
// Sub-state 7: the fall
// ===========================================================================

// ===========================================================================
// Sub-state 8: the recoil
// ===========================================================================

void FieldC3_Inject() {
    if (bof3::WantsShadow("field_c3")) field_c3::SelfTest();
}

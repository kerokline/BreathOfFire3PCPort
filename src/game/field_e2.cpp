// Round twelve group FE2 (docs/field_e2.md): the field engine's resident code
// in 0x5341C0..0x5372D8, 0x56D240..0x5729F8 and 0x593960..0x594060 that no
// earlier group took - 51 functions:
//
//   the event script's rest   Scenario_CallB, Field_AfterBattleTally and its
//                             two record counts, Field_FaceMode11Object,
//                             Field_FloorHurt, the wide cell tests,
//                             Field_ObjectTrigger and the two hooks under it
//   the leader's hop helpers  Leader_* (called by FC3's and FE1's states)
//   the mode-11 object        Mode11_Object* (the object at 0x905DA0 that
//                             Game_Mode 11 moves) and its draw
//   the field tail            FieldTail_* (seven Field_ModeTailKinds slots)
//   draw layers               three MapCell_Handlers kinds, AreaMap_ClearCell
//   the trade screen          ItemTrade_* (0x66A47C / 0x66A484's states)
//
// Every call goes through the scenario harness (SH_CALL / SH_AT, a table's
// word read in place), so the start-up fuzz (field_e2_fuzz.cpp) stands
// recorders in for ours as for the originals' copies. Faithful: the one
// difference is DIV-0023's, extended - the fourth word of the SVECTORs the
// draw functions build on their stacks is 0 here where the originals leave
// stale stack (docs/DIVERGENCE.md DIV-0023). Where an original indexes past
// its table, divides by 0 or walks a record list past its end, ours aborts
// with a message (docs/field_e2.md section 6).
#include "game/field_e2.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "common/asm_name.h"
#include "game/field_e2_callees.h"
#include "game/lang_layout.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "game/text_advance.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = field_e2::at;
// move_script::At: named in full below (source-layout B0, a clash on At)
// move_script::Long: named in full below (source-layout B0, a clash on Long)
using move_script::SetLong;
// move_script::SetWord: named in full below (source-layout B0, a clash on SetWord)
// move_script::Word: named in full below (source-layout B0, a clash on Word)
using U = std::uint32_t;

unsigned char& BFieldE2(U a) { return *move_script::At(a); }
U LFieldE2(U a) { return static_cast<U>(move_script::Long(move_script::At(a))); }
void SetL(U a, U v) { move_script::SetLong(move_script::At(a), static_cast<std::int32_t>(v)); }
std::uint16_t WFieldE2(U a) { return move_script::Word(move_script::At(a)); }
void SetW(U a, unsigned v) { move_script::SetWord(move_script::At(a), v); }
U Key(const void* p) { return Low32(p); }

// The record views (game/record_views.h): the leader helpers run on the leader,
// an ObjTrio record, and Field_State is one.
U LongAt(const unsigned char* p) { return static_cast<U>(move_script::Long(p)); }
int S8FieldE2(unsigned v) { return static_cast<signed char>(v); }
int S16FieldE2(unsigned v) { return static_cast<short>(v); }

using Handler = void (__cdecl*)();

// A .data table's entry read in place (swapped for recorders while the fuzz
// runs); an entry that is not code - an index past the table - aborts, where
// the original jumps there (scena_sc0.cpp's CodeAt).
U CodeAtFieldE2(U table, U index, const char* who) {
    const U cell = table + 4u * index;
    const U entry = LFieldE2(cell);
    // A code word: hosted a VA in BOF3.exe's .text, in bof3x.exe a function of ours
    // the bind table names (common/image_base.h IsCodeWord); not the trap, not data.
    if (!scenario_harness::g_active && !::bof3::IsCodeWord(entry))
        bof3::Fatal("%s: index %u reads 0x%X at 0x%X (VA 0x%X), not code - past its table (the original jumps there)", who,
                    (unsigned)index, (unsigned)entry, (unsigned)cell, (unsigned)::bof3::VaOf(cell));
    return entry;
}

}  // namespace

// ============================================================================
// The event script's rest
// ============================================================================

// The map's cell byte at (x, z), whole words (AreaMap_ByteAt reads them as
// s16s), each and 0xF0 when `mask` (a byte) is set.
// ============================================================================
// The leader's hop and step helpers (FC3's leader states 0x525CE0..0x525F90
// and FE1's 0x52F970..0x52FB50 call them; PSX twins 0x801C5D14..0x801C66DC,
// pairs_propagated "call", each a hypothesis). Each works on Sprite_Current
// and reads it again after every call, as the originals do.
// ============================================================================

// The hop's set-up, 0x535FE0 and 0x5363C0 alike: Field_State +0x128 = +0x70
// + 2, Field_JumpSetUp; +0x14 = 0; +9 = (+0x70 + 1) * +9 * 2 and +0xA = +9 / 2
// (bytes); Sprite_EnsureAnimation(+8 + 8), Sprite_ApplyVelocity.
// ============================================================================
// The party-set error screen
// ============================================================================

// ============================================================================
// The mode-11 object (0x905DA0; Game_Mode 11, which Field_Request 9 enters,
// runs Mode11_ObjectFrame then 0x5172C0 each frame: docs/mode-tasks.md)
// ============================================================================

// ============================================================================
// The object triggers and the cell hook
// ============================================================================

// ============================================================================
// The field tail: seven of Field_ModeTailKinds' slots (Field_ModeTailRun
// jumps through them by the s8 0x9039F3 each field frame). Each is a state
// machine on the s8 0x9039F4 with an argument byte 0x9039F5; the area code
// arms them (kind, state 0, argument). A state out of range does nothing.
// No PSX twin paired for any.
// ============================================================================

// original 0x56DE50 (slot 44): 0 - unless a message is up: bit (argument) of
// 0x904650 set, the system message 0x4172 + argument copied (8 bytes, the
// CRT's copy) into Text_Records, Msg_OpenSystem(0xFA), Field_Request 2,
// Sound_StopMusic, Sound_LoadStream(9), state 1; 1 - once Sound_StreamDone
// and no message is up, Sound_ResumeAll, the byte 0x903804 points at 0,
// ScriptFlags_Clear40, the tail cleared. Bit (argument) of 0x904650 is the
// dragon gene's (Gene_Bits) and message 0xFA names it.
//
// DIVERGENCE DIV-0166 (known-defects D351): the copy (`push 8` ... `call
// 0x5B9450` at 0x56DED1..0x56DEE2) is strncpy, so a name of 8 bytes or more
// leaves Text_Records' first record unterminated and the message prints on
// into whatever an earlier copy left at +8. The US disc (GAME.EMI#0
// 0x801A83DC; a `sb $zero` to the buffer + 8 after the copy) terminates it;
// the JP (0x801A93E4) does not. With g_name_nul (1 through
// PatchBytes("GeneNameNul") in FieldE2_Inject, after the self-test) the byte
// at +8 is 0 after the copy, as on the US disc.
namespace field_e2 { std::uint8_t g_name_nul = 0; }

// ============================================================================
// Draw layers: three MapCell_Handlers kinds (DrawLayer_Open calls each with
// the cell's record, its byte 1 and byte 0), and a map cell cleared
// ============================================================================

// ============================================================================
// The trade screen (ItemTrade_States 0x66A470, dispatched by 0x593950 on
// 0x93985C - nobody's): a list of entries (0x66AD10's row 0x905B88, ten
// record numbers) of the 8-byte records at 0x66AB58 (+0 the item, +1 its
// category, +2..+4 three ingredients, +5..+7 their amounts); a count to
// choose; a yes / no. The state handlers are hidden starts in
// Sprite_ClutWord's extent. PSX twins 0x800F5048..0x800F5828 (table-
// anchored and callers, each a hypothesis).
// ============================================================================

// DIVERGENCE DIV-0027 (amended 2026-10-03, after the live check): the per-item
// "Is <item> OK?" answers. Capcom draws the two answer words, one space apart,
// at x 0xDE and the hand at 0xDC + 36 * the answer - stops fitted to the Chinese
// words: under the English overlay the hand on No covers Yes (the owner's
// masterAndManillo.txt, frames 2700 and 3030). With the flag on (FieldE2_Inject,
// a Latin overlay, after the self-test) the layout is the load / save screen's,
// as the leave prompt's is (effect_1g.cpp, yes_no_layout.cpp): the first answer
// three spaces further left, the second where it was, the hand two units left
// of each.
unsigned char g_trade_confirm_layout = 0;

// ============================================================================

void FieldE2_Inject() {
    if (bof3::WantsShadow("field_e2")) field_e2::SelfTest();
    {
        // DIV-0166: after the self-test, which compared Capcom's unterminated copy.
        const std::uint8_t was = 0, is = 1;
        bof3::PatchBytes("GeneNameNul", Low32(&field_e2::g_name_nul), &was, &is, 1);
        bof3::Log("DIV-0166    message 0xFA's gene name is terminated after its 8-byte copy (on unless the line above says OFF)");
    }
    // DIVERGENCE DIV-0027 (amended 2026-10-03): after the self-test, which
    // compares the original's prompt; a Latin overlay only (DIV-0056).
    if (Lang_Latin()) {
        static const std::uint8_t was = 0, is = 1;
        bof3::PatchBytes("TradeConfirmLayout", Low32(&g_trade_confirm_layout),
                         &was, &is, 1);
        bof3::Log("DIV-0027    Manillo's \"Is <item> OK?\": answers re-spaced, the hand two units left of each");
    }
}

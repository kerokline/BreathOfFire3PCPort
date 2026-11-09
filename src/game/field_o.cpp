// Group FO of round twelve, wave two - the field engine's resident code in
// 0x5738A0..0x57CD89, taken with the scenario harness's field mode
// (scenario_harness.h). docs/field_o.md has each function, its callers, the
// fuzz and the controls.
//
//   Menu_DrawStatsPanel     0x5738A0  a record's four stats and its trait line
//   Menu_DrawExpPanel       0x573BF0  a record's EXP, or the EXP its next level wants
//   Menu_DrawIconWheel      0x573F70  a kind's name, a turned triangle and up to three icons
//   Menu_DrawTile16         0x574400  one 16 x 8 sprite of the menu page, dimmed or not
//   Menu_DrawEquipCompare   0x574EC0  a record's name, stats, the compare arrows and six items
//   Menu_DrawAbilityPanel   0x575F50  a member's ten abilities with their AP costs
//   Menu_DrawItemPanel      0x5763F0  a category's usable items, kept in 0x6BC760, scrolled
//   Menu_DrawSaveSlot       0x576960  a save slot's number, party icons, name, level and time
//   MoveCmd_OpF9            0x578FA0  op F9: the object eased a step along its facing
//   MoveCmd_Op88            0x5794D0  op 88 by Sprite_Current +4 through 0x663AFC
//   MoveCmd_Op88Start       0x5794F0  its state 0: a tint from 0xC0
//   MoveCmd_Op88Fade        0x579560  its state 1: the tint down to 0x80, then released
//   MoveCmd_Op87            0x5795F0  op 87 by Sprite_Current +4 through 0x663B04
//   MoveCmd_Op87Start       0x579610  its state 0: a tint from 0x80
//   MoveCmd_Op87Fade        0x579690  its state 1: the tint up to 0xC0, then released
//   EventScript_SkipIf      0x579CA0  an F0 / F1 stepped over to past its FE
//   EventOp_3x .. _Ax       0x57A7C0..0x57AEC0  the placement ops 3x, 4x, 7x, 6x, Ax
//   EventCond_*             0x57C1A0..0x57C440  event-script conditions 1, 3..9, 11..16
//   ObjTrio_ClearBit40      0x57C7E0  bit 6 of the three party objects' first byte cleared
//   MoveCmd_OpE9            0x57C8E0  op E9 by Sprite_Current +4 through 0x663B84
//   MoveCmd_OpE9Start/Arc/Kind2/Fall  0x57C920, 0x57CA80, 0x57CBB0, 0x57CCE0  its four states
//   MoveCmd_OpDB            0x57CD40  the object's x and z to the half unit
//
// Every call goes through the harness (SH_CALL), so the start-up fuzz stands
// recorders in for ours as for the originals' copies. No divergence: each is
// a faithful replacement (Menu_DrawSaveSlot reads DIV-0029's byte as
// yes_no_layout.cpp holds it, as Menu_YesNo reads DIV-0027's) - save for
// the empty save slot's text, drawn blank under SaveSlotBlank (the exe-gap
// round), and DIV-0059's and DIV-0084's switches below. The unchecked indexes the originals make are
// reproduced; where the original would jump to what is not code or divide by
// zero, ours aborts with a message (docs/field_o.md section 6).
#include "game/field_o.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/field_o_callees.h"
#include "game/labels.h"
#include "game/lang_layout.h"
#include "game/list_title.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "game/yes_no_layout.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = field_o::at;
constexpr auto& AtFieldO = move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using U = std::uint32_t;

unsigned char* Ptr(U a) { return AtFieldO(a); }

unsigned char* Record(unsigned id) { return AtFieldO(::bof3::Img(CharacterRecords_addr) + (id & 0xFF) * field_o::at::kRecordStride); }
unsigned char* Sc() { return Sprite_Current; }
unsigned char* Fam() { return Field_ActiveMember; }

// A signed 16-bit word, as `movsx` / a 16-bit compare reads it.
std::int32_t S16(unsigned v) { return static_cast<std::int16_t>(static_cast<std::uint16_t>(v)); }
std::int32_t Sb(unsigned v) { return static_cast<signed char>(static_cast<unsigned char>(v)); }

// A handler read in place from a .data table: an entry outside .text is not
// code - the original jumps there; ours aborts (round nine's rule). While the
// fuzz runs, the table holds the harness's recorders, which live outside .text.
U CodeAt(U table, unsigned index, const char* who) {
    const U cell = table + 4u * index;
    const U entry = static_cast<U>(Long(AtFieldO(cell)));
    // A code word: hosted a VA in BOF3.exe's .text, in bof3x.exe a function of ours
    // the bind table names (common/image_base.h IsCodeWord); not the trap, not data.
    if (!scenario_harness::g_active && !::bof3::IsCodeWord(entry))
        bof3::Fatal("%s: index %u reads 0x%X at 0x%X (VA 0x%X), not code - past its table (the original jumps there)", who,
                    index, (unsigned)entry, (unsigned)cell, (unsigned)::bof3::VaOf(cell));
    return entry;
}

}  // namespace

// =============================================================================
// The menu panels
// =============================================================================

// DIVERGENCE DIV-0084: the icon wheel's formation name centred on x + 0x27,
// as the US release draws it (its small-text draw takes the middle), where
// the port starts its four-glyph names at x + 0x16. 0 until FieldO_Inject
// patches it to 1 under a Latin overlay, after the fuzz, under the name
// FormationNameCentre.
namespace field_o { unsigned char g_wheel_name_centre = 0; }

namespace {

// A title of a table of pointers, centred on 13 six-unit columns - or, under
// DIVERGENCE DIV-0059, on the width its pen covers.
// g_title_live is set by FieldO_Inject after the fuzz, which runs later than
// BattleDraw_Inject's patch and compares with the original's arithmetic.
}  namespace field_o { bool g_title_live = false; }  // original 0x576960: a save or load slot (SaveMenu_DrawSlots 0x588D6B; the
// save header's summary, or 0 for an empty slot). Box (x + 8, y) 0xC8 x 0x30,
// the slot's number (0x6639D4) at (x + 9, y + 5); with a summary: a draw mode,
// the party's three icons (+5..+7, 0xFF none) through Menu_DrawBigIcon at
// x + 0x56, 41 apart, the name - +0..+4 and the four bytes +0x16 copied to
// 0x904BA0 - through Text_DrawAt (five characters) at (x + the inset, y + 2),
// the level +8 (0x64D3EC) at (x + 0x1F, y + 0x18), the time +0xC / +0xD
// (0x654830) and two colons (0x6639D0) at x + 0x27, Menu_DrawExpBar(x + 0x14,
// y + 0x12, 0, level, +0x10); without one the text 0x669CCC at (x + 0x13,
// y + 2). Then the frame pieces 0x663584 (flags 2 when +0x14 is set, else 0)
// and, with +0x14 set, piece 0x3D at (x + 0x32, y + 0x16).
// The inset is the disp8 at 0x576A48 - 0x13 as Capcom shipped it, 0x15 under
// DIV-0029 (YesNoLayout_Inject patches those bytes) - as yes_no_layout.cpp
// holds it (YesNoLayout_SaveNameInset; docs/operand-store.md).
// DIVERGENCE (exe-gap round, group AU; the coordinator numbers it): an empty
// slot draws no text, as the US PlayStation release draws it and as a
// disc-built image already did (0x669CCC is a gap there, zero). 0 until
// FieldO_Inject patches it to 1, after the fuzz, under the name SaveSlotBlank
// (BOF3X_ORIGINAL=SaveSlotBlank, or BOF3X_OFF, draws the port's string).

namespace field_o { unsigned char g_empty_slot_blank = 0; }

// =============================================================================
// The movement script's ops
// =============================================================================

// =============================================================================
// The event script
// =============================================================================

// --- the conditions (EventScript_Conditions entries; each handed the script
//     position, its operand at **at, al the answer) ---

void FieldO_Inject() {
    if (bof3::WantsShadow("field_o")) field_o::SelfTest();
    field_o::g_title_live = true;   // DIV-0059, from here on
    // DIVERGENCE DIV-0084: a Latin language overlay only, as DIV-0059's switch.
    if (Lang_Latin()) {
        static const std::uint8_t was = 0, is = 1;
        bof3::PatchBytes("FormationNameCentre",
                         Low32(&field_o::g_wheel_name_centre),
                         &was, &is, 1);
        bof3::Log("DIV-0084    formation names centred: %s", field_o::g_wheel_name_centre ? "on" : "off");
    }
    // An empty save slot drawn blank on every install and language (the
    // owner's call of 2026-10-31, docs/exe-gap-round.md 7).
    {
        static const std::uint8_t was = 0, is = 1;
        bof3::PatchBytes("SaveSlotBlank", Low32(&field_o::g_empty_slot_blank),
                         &was, &is, 1);
        bof3::Log("SaveSlotBlank  an empty save slot draws %s", field_o::g_empty_slot_blank ? "no text" : "the port's text (0x669CCC)");
    }
}

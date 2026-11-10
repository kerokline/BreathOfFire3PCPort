// The masters' screen and three field-menu screens - round fourteen, wave two,
// group R2D: the 51 functions 0x5869A0..0x58B1CD of the cut
// (analysis/round14_cut.tsv), each read with capstone to its last instruction
// (docs/rest_2d.md section 1).
//
// Every call out goes through the scenario harness (SH_CALL / SH_AT), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies;
// a table's handler is called by the address the table holds. No divergence:
// each is a faithful replacement. The one patch inside the band is DIV-0027's
// (yes_no_layout.cpp): two call sites of the masters' yes / no prompt
// (0x586E78, 0x586E97) re-aimed under a Latin overlay; MasterScreen_AskYesNo
// reads where each site reaches now and calls that, so the patch, its
// BOF3X_ORIGINAL switch and the language are honoured as they were in Capcom's
// body. Where the original jumps through a table past its own count, or
// writes past a record array by a byte no path in play makes that large, ours
// aborts with a message (section 5 of the doc); reads past them are made in
// place, as the original makes them.
#include "game/rest_2d.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_2d_callees.h"
#include "game/scenario_harness.h"
#include "game/yes_no_layout.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace {

namespace at = rest_2d::at;
using U = std::uint32_t;
// move_script::At is named in full here (step B0, analysis/tu_renames.tsv)
using move_script::Long;
using move_script::SetWord;
using move_script::Word;

unsigned char& BRest2d(U a) { return move_script::At(a)[0]; }

// A state table's handler, jumped to as the original's `jmp [table + 4 *
// index]` does: the index must lie inside the table's own count (its reader's
// reach, docs/rest_2d.md section 3); past it the original jumps through what
// follows the table, which no handler of the screen writes - ours aborts. The
// entry is called as the cell holds it: Capcom's address in the game (Inject's
// jmp to ours where it is ours), a recorder while the fuzz runs.
using Handler = void (__cdecl*)();
void Jump(U table, unsigned count, unsigned index, const char* who) {
    if (index >= count)
        bof3::Fatal("%s: step %u is past its table's %u entries at 0x%X; the original jumps through 0x%X", who, index, count,
                    static_cast<unsigned>(::bof3::VaOf(table)), static_cast<unsigned>(::bof3::VaOf(table + 4 * index)));
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<U>(Long(move_script::At(table + 4 * index)))))();
}

// A record the original writes by an index it never bounds. Past the array it
// writes into the next block; no path in play gives such an index (section 5),
// so ours aborts there.
U RecordChecked(U base, U stride, unsigned count, unsigned index, const char* array, const char* who) {
    if (index >= count)
        bof3::Fatal("%s: %s index %u is past its %u records; the original writes at 0x%X", who, array, index, count,
                    static_cast<unsigned>(::bof3::VaOf(base + index * stride)));
    return base + index * stride;
}
U Window(unsigned index, const char* who) {
    return RecordChecked(::bof3::Img(WindowRecords_addr), rest_2d::at::kWindowStride, rest_2d::at::kWindowCount, index, "WindowRecords", who);
}

// --- callees by address (R2C's and R2E's, ours; named in rest_2d_callees.h) ----------------
void MemberPanel(U x, U y, U member, U row) {
    SH_AT(void (__cdecl*)(U, U, U, U), rest_2d::at::kMemberPanel.v)(x, y, member, row);
}
void MemberLabel(U x, U y, U member) { SH_AT(void (__cdecl*)(U, U, U), rest_2d::at::kMemberLabel.v)(x, y, member); }
void PromptBox(U x, U y, U w, U h, U style) { SH_AT(void (__cdecl*)(U, U, U, U, U), rest_2d::at::kPromptBox.v)(x, y, w, h, style); }

// The master's message: word [0x6644E8 + 2 * master] + k (the master byte
// unbounded: the table is read in place).
unsigned short MasterMessageRest2d(unsigned k) {
    return static_cast<unsigned short>(Word(move_script::At(::bof3::Img(rest_2d::at::kMasterMessages.v) + 2u * AreaW0c_TailSub)) + k);
}
// Field_Members' byte of the party slot `slot` (ObjTrio + 0x148, 0x14C apart).
unsigned char MemberAt(unsigned slot) { return (rv::Bytes(ObjTrio) + 0x148)[slot * rest_2d::at::kObjStride]; }
// The member under the cursor: a CharacterRecords index.
unsigned char CursorMember() { return MemberAt(Rest2d_Cursor); }
U RecordOf(unsigned member) { return ::bof3::Img(CharacterRecords_addr) + member * rest_2d::at::kRecordStride; }

// --- the masters' screen's shared draw -------------------------------------------------------

// The pick's and the yes / no prompt's frame: each member's panel and label,
// the cursor's frame over the cursor's member (blinking when `blink`), the
// prompt's box, and the line - message `message` of MessagePools.
void DrawMasterScreen(U message, unsigned blink, const unsigned char* (*line)(U)) {
    for (unsigned i = 0; i < Field_MemberCount; ++i) {
        const U y = 0x3E + 54 * i;
        MemberPanel(0x11, y, MemberAt(i), i);
        MemberLabel(0x89, y, MemberAt(i));
    }
    SH_CALL(MasterScreen_DrawCursorFrame)(0x11, 0x3E + 54u * Rest2d_Cursor, 0x110, 0x34, blink, 6);
    PromptBox(0x14, 0x10, 0x118, 0x13, static_cast<unsigned char>(Config_WindowColour));
    line(message);
}

const unsigned char* MessageText(U message) {
    const unsigned word = Word(move_script::At(::bof3::Img(rest_2d::at::kMessagePools.v) + 2 * (message & 0xFFFF)));
    return move_script::At(::bof3::Img(rest_2d::at::kMessagePools.v) + word);
}

}  // namespace

// ============================================================================================
// The masters' screen
// ============================================================================================

// --- state 3: a member made the master's apprentice (R2C's step table 0x664548) ---------------

// --- state 4: an apprentice released (MasterQuit_Steps) -----------------------------------------

// --- state 5: what the levels gained under the master grant (MasterGrant_Steps) ----------------

// ============================================================================================
// The field menu
// ============================================================================================

// --- the field abilities (FieldAbility_Effects, called by FieldAbility_Use) ---------------------

// --- the Status screen: FieldMenu_States[6] ----------------------------------------------------

// --- the Items screen: FieldMenu_States[2] -------------------------------------------------------

// ============================================================================================

void Rest2D_Inject() {
    if (bof3::WantsShadow("rest_2d")) rest_2d::SelfTest();
}

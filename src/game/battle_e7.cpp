// Round twelve group BE7 (docs/takeover-queue-field-battle.md section 3): the
// battle windows of 0x597FC0..0x59DB61, the 31 rows of analysis/round12_cut.tsv
// group BE7, each read to its last instruction with capstone (2026-09-29) and
// taken through the boss harness as an engine group (boss_harness.h,
// docs/boss_harness.md section 10). docs/battle_e7.md has every function one
// row each; names are from the code and its callers (no PSX twin of these is
// named in the sibling's corpus).
//
//   - the result screen (Window_Handler4Kinds' kinds 0 and 1): the level-up
//     window (six stat gains and two abilities learned), the drops window
//     (two items a row), one item's line, and the translucent frame both
//     draw - also BattleResultWin_DrawExp's frame;
//   - the gene windows (the window-kind handler 0x598890's kinds 1..4 and the
//     grid's draw its kind 0 calls): the grid window's boxes and frame, a
//     three-choice box, two list windows (six and twelve rows) with their
//     frame, a form icon and a flashing cursor box - each kind a state
//     dispatcher by the record's +3 over an open state and slides;
//   - the battle menu's equipment window 0x59D640 (Window_Handler8Kinds'
//     kind 3): a member's name, four stats, the preview of a set, the six
//     equipped items.
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. The
// four stack-table dispatchers abort past their tables where the original
// calls through whatever the stack holds beyond them, and the three open
// states abort on a task slot past the pool where the original writes past it
// (the owner's rule for an unchecked index, round9 doc section 6; both are
// described in docs/battle_e7.md section 7, never fixed). One divergence has
// patch sites inside these bodies and survives in ours: DIV-0041
// (widescreen.cpp, the faster slide) at the three list slide-outs; ours reads each
// bound as widescreen.cpp holds it (docs/operand-store.md). Every call goes
// through the harness (BH_CALL / BH_AT), so the start-up fuzz can stand
// recorders in for the callees.
#include "game/battle_e7.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/battle_e7_callees.h"
#include "game/boss_harness.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = battle_e7::at;
using U = std::uint32_t;
using move_script::SetWord;
// move_script::Word is named in full here (step B0, analysis/tu_renames.tsv)

unsigned char& BBattleE7(U address) { return move_script::At(address)[0]; }
unsigned W(const unsigned char* p) { return move_script::Word(p); }
U Addr(const void* p) { return Low32(p); }
const char* Str(U address) { return reinterpret_cast<const char*>(move_script::At(address)); }
char* Buf(U address) { return reinterpret_cast<char*>(move_script::At(address)); }
const unsigned char* Text(U address) { return move_script::At(address); }

}  // namespace

// ===========================================================================
// The result screen (Window_Handler4Kinds 0x597F60: kind 0 through 0x597FA0,
// kind 1)
// ===========================================================================

// ===========================================================================
// The gene windows (the window-kind handler 0x598890: kind 0's grid through
// 0x5989B0 / 0x5989F0, kinds 1..4 the stack dispatchers below)
// ===========================================================================

// original 0x599A00 (PSX 0x800AEE34): the cursor box at (x, y), w by h (words): a LINE_F3
// (x, y), (x, y), (x, y + h - 1) and a LINE_F4 (x, y), (x + w - 1, y), (x + w
// - 1, y + h - 1), (x, y + h - 1), both in one colour - the brightness 0xFF,
// or with `flash` a pulse of Frame_Counter (bit 3 set: ((c & 6) << 5) +
// 0x3F; else ((~c & 6) << 5) + 0x3F) - in the channels `sides` bits 0..2 name
// (bit 0 the prim's +6, bit 1 its +5, bit 2 its +4), 0 in the others.
//
// As the original has it: x, y, w and h are masked with 0xFFFF before `fild`
// (0x599A6E..0x599A81), so a coordinate below 0 lands at 65,536 less it. The
// PlayStation (US 0x800B1F04, JP 0x800AEF04: `sh` of x, y and y + h - 1 into
// the LINE's vertex halfwords at 0x800AEEF4..0x800AEF08) keeps them signed.
//
// DIVERGENCE DIV-0161: 1 through PatchBytes("BattleDrawSigned") in
// BattleE7_Inject, after the self-test. Each vertex is then the PlayStation's
// signed halfword - x, y, x + w - 1 and y + h - 1 each cut to 16 bits and
// sign-extended - so a box partly off the left or top edge clips (D336).
namespace battle_e7 { std::uint8_t g_cursor_box_signed = 0; }

// ===========================================================================
// The battle menu's equipment window (Window_Handler8Kinds kind 3, 0x59CC10)
// ===========================================================================

// original 0x59D640: (member, x, y, set, flags, record) - the window at (x +
// 4, y + 4) 0x78 by 0xA3; the member's name (CharacterRecords + 0xA4 member,
// 5 characters at most) centred on x + 0x3E; the four stat labels 0x66A0F8..
// at (x + 5, y + 0x27 + 13 k); Char_RecalcStats, then the four stats +0x24,
// +0x26, +0x2A, +0x28 (by 0x64E324, 8-pixel font, at (x + 0x31, y + 0x1C + 13
// k)). Unless flags bit 0: Equip_PreviewSet(member, set, marks, values) into
// the stack, and per stat a bar 0x59DB70(x + 0x51, y + 0x1C + 13 k, 0x11 for
// mark 4, 0x12 for 1, else 0x13, 0x1C, CLUT (0, 0x1E0), 0x80) and the value
// in the mark's colour at (x + 0x59, same y). Then the six equipped items
// (+0x12..+0x17, categories 0x66B5BC): the record's +0x10 = 0 first; row r
// in colour 2 when it is the record's +0xB; with the record's +0xD bit 1, an
// item Item_CanUse(2, the member 0x66972C[0x904065[s8 0x929F06]], category,
// item) refuses is drawn dim in colour 7, and for one it allows on row +0xA
// the record's +0x10 = category << 8 | item, and it is first drawn dim in
// colour 7; the row +0xA / +0xB raised by 2 pixels; then its icon
// (0x66B5B4) and name. Last the pieces 0x66B508 and the frame's edges.
//
// DIVERGENCE DIV-0065: the labels' row. The original draws label k at y +
// 0x27 + 13 k and the values at y + 0x1C + 13 k, so each label sits two
// pixels above the next stat's value - the first value has no label and the
// fourth label is drawn under the frame, off the box. The field's member
// panel (0x5738A0: the label at y + 8, the value at y + 0xA) has the label
// two pixels above its own value; 0x27 is that relation one 13-pixel row
// down, 0x1A + 0xD. Ours draws the labels at y + 0x1A + 13 k once the flag
// is on; BattleE7_Inject sets it after the self-test, which compares the
// original's rows.
unsigned char g_equip_labels_row = 0;

// ============================================================================

void BattleE7_Inject() {
    if (bof3::WantsShadow("battle_e7")) battle_e7::SelfTest();
    // DIVERGENCE DIV-0065: after the self-test, which compares the original's rows.
    {
        static const std::uint8_t was = 0, is = 1;
        bof3::PatchBytes("BattleEquipLabelsRow", Low32(&g_equip_labels_row),
                         &was, &is, 1);
        bof3::Log("DIV-0065    the battle equip window's stat labels beside their values");
    }
    {
        // DIV-0161: after the self-test, which compared Capcom's unsigned reading.
        static const std::uint8_t was = 0, is = 1;
        bof3::PatchBytes("BattleDrawSigned", Low32(&battle_e7::g_cursor_box_signed),
                         &was, &is, 1);
        bof3::Log("DIV-0161    the gene list's cursor box takes its corners signed, as the PlayStation (on unless the line above says OFF)");
    }
}

// DIVERGENCE DIV-0015: the in-game Config screen's text from a language
// overlay.
//
// The Config screen's strings are not in any DAT. They are in BOF3.exe itself
// (docs/config-screen.md), in three shapes, and each needs a different key:
//
//  - Six row labels, as pointers built into the code of the row draw
//    0x461800 (`mov dword ptr [esp + n], 0x669F0C` and five more). The Chinese
//    strings have 4 to 8 bytes of room where "Background" needs 11, so these
//    are RE-POINTED at buffers of ours rather than written over - six operand
//    patches, each validated against the address it must hold.
//  - Seventeen option strings, in 16-byte records at 0x6536F8: a count, a
//    signed x, then the string. Written in place; the donor's own count and x
//    come with the string, because they are what puts an option where the
//    PlayStation release puts it.
//  - Six controller-panel names behind the pointer table 0x66A368, whose six
//    entries point into eight-byte slots at 0x66A338. Re-pointed too: a UI
//    string is two bytes a character, so "Change" needs 13.
//
// NOT patched: the screen's own centring and right-alignment arithmetic. This
// screen draws through 0x516E70, whose quad is 8 units where the ordinary draw
// uses 12 - 16 screen pixels a character against 24, measured off the owner's
// screenshot (docs/config-screen.md section 4). That quad shows the WHOLE
// 24 x 24 cell scaled to 16 x 16 (the emitter 0x516D50 fixes the texture
// extent at 0xC units), so every string here names the donor's 8 x 8 UI cells
// (glyph 0xA00 up, DIV-0016), which the overlay stores tripled to fill the
// cell and which therefore arrive as the PlayStation's doubled 8 x 8 - and
// names them two bytes at a time even where a one-byte slot exists, which
// keeps a string's byte length exactly twice its character count, exactly
// what the original's `4 * len` and `4 * count` assume.
//
// Three earlier builds got this wrong, and the wrongs are worth knowing. The
// first used single-byte codes and patched three width computations to hide
// the difference. The second used the 8 x 8 set doubled into a corner of its
// cell, and the letters came out two thirds of the disc's size. The third took
// that for the wrong font and used the 8 x 12 cells: right height, two thirds
// of the width. The width arithmetic is the original's.
#include "game/config_text.h"

#include <windows.h>

#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/lang_layout.h"
#include "game/scenario_harness.h"
#include "hook/code_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

constexpr std::uint32_t kLabels = 6;
constexpr std::uint32_t kRecords = 17;
constexpr std::uint32_t kCtrl = 6;

// The record table at 0x6536F8: 16 bytes each, [0] count, [1] signed x,
// [2..] the string. Read 2026-09-20; the row -> first record and row -> count
// tables at 0x653808 / 0x653810 are byte-identical to the PlayStation's.
constexpr std::uint32_t kRecordBase = 0x6536F8;
constexpr std::uint32_t kRecordStride = 16;
constexpr std::uint32_t kRecordRoom = kRecordStride - 2;   // string + its NUL

// The controller panel's six names. The pointer table at 0x66A368 is data, so
// unlike the labels these are re-aimed by writing the table itself.
constexpr std::uint32_t kCtrlPointers = 0x66A368;
constexpr std::uint32_t kCtrlExpected[kCtrl] = {
    0x66A338, 0x66A340, 0x66A348, 0x66A350, 0x66A358, 0x66A360,
};
constexpr std::uint32_t kCtrlRoom = 32;
char g_ctrl[kCtrl][kCtrlRoom];

// Our own storage for the six labels. Static, so the addresses patched into
// the code stay good for the life of the process.
constexpr std::uint32_t kLabelRoom = 32;
char g_labels[kLabels][kLabelRoom];

// The six `mov dword ptr [esp + n], <label>` operands in the row draw
// 0x461800 (Config_DrawRowLabel, ours since round twelve: field_c1.cpp reads
// these operands, the anchors and the large call below from the original's
// code at every call, so the patches hold for ours), and the address each must
// hold before we touch it.
struct LabelSite {
    std::uint32_t at;
    std::uint32_t expected;
};
constexpr LabelSite kLabelSites[kLabels] = {
    {0x461832, 0x669F0C}, {0x46183A, 0x669F18}, {0x461842, 0x669F24},
    {0x46184A, 0x669F2C}, {0x461852, 0x669F34}, {0x46185A, 0x669F3C},
};

// The values of ours (ConfigText_Operands), the original's until a patch goes
// in; and, for the hosted check, the bytes of each multi-byte patch as they
// stand (the original's, then the replacement).
ConfigTextOperands g_ops = {
    {0x669F0C, 0x669F18, 0x669F24, 0x669F2C, 0x669F34, 0x669F3C},
    0x3A, false, false, 0x516B30, 0x516B30, false, 0x20, 0x68, true, 0xC,
};
std::uint8_t g_anchor_now = 0x3A;
std::uint8_t g_label_w_now[5] = {0x8D, 0x0C, 0x49, 0xD1, 0xE1};
std::uint8_t g_option_w_now[3] = {0x8D, 0x04, 0x40};
std::uint8_t g_name_w_now[3] = {0x8D, 0x04, 0x49};
std::uint8_t g_edge_now[3] = {0x83, 0xC1, 0x20};
std::uint8_t g_box_now[2] = {0x6A, 0x68};
std::uint8_t g_cell2_now[6] = {0x8B, 0x35, 0x70, 0x06, 0x7E, 0x00};
std::uint8_t g_esp_now[3] = {0x83, 0xC4, 0x44};
std::uint8_t g_frame_now[3] = {0x6A, 0x0C, 0x32};

// PatchBytes, the bytes kept in `now` when they go in.
bool Patch(const char* name, std::uint32_t at, const std::uint8_t* was, const std::uint8_t* is, std::uint32_t n,
           std::uint8_t* now) {
    if (!bof3::PatchBytes(name, at, was, is, n)) return false;
    std::memcpy(now, is, n);
    return true;
}

// Reads one NUL-terminated string out of the payload. Returns nullptr when the
// payload ends first, which the caller treats as a malformed chunk.
const char* TakeString(const std::uint8_t*& p, const std::uint8_t* end) {
    const auto* start = reinterpret_cast<const char*>(p);
    while (p < end && *p) ++p;
    if (p >= end) return nullptr;
    ++p;  // the NUL
    return start;
}

// DIVERGENCE DIV-0017: the selected row, in the dialogue font at its own
// advance.
//
// The row under the cursor is drawn large: 0x461800 and Config_DrawRowOptions each have a
// branch that goes through Text_DrawAt and its 12-unit quad instead of the
// 8-unit draw. In Chinese that is the same glyph at full size. On the
// PlayStation it is a different FONT - the 8 x 12 dialogue cells, still on an
// 8 advance (owner's screenshots, 2026-09-21: "Background" large spans the
// same ten columns it does small). Our strings name the 8 x 8 UI cells, which
// at full size are a tripled 8 x 8 on an advance of 8: crowded, and - because
// the branch reckons 12 units a character where the pen moves 8 - thrown left
// by four units a character.
//
// So this stands in for Text_DrawAt at those two call sites only (and, since
// DIV-0026, at the controller panel's one, 0x461B43), and redraws
// the string with each UI glyph swapped for the dialogue glyph of the same
// character. The two sets are in the same code order, 100 cells each
// (tools/loc_build.py), so the swap is a constant offset. The blank after the
// UI set stays: it is the space, and its advance is 8 in either draw.
constexpr std::uint32_t kUiGlyphs = 0xA00, kDialogueGlyphs = 0x993, kSetCells = 100;
// The French and German discs' accented cells (codes 0x94 up) are a second
// pair of blocks, laid out the same way: UI at 0xAA0, dialogue at 0xA80, up
// to 24 cells (loc_build.py's EXT_SMALL_AT, EXT_AT, EXT_MAX). An English
// overlay has no glyphs there, and its strings never name them.
constexpr std::uint32_t kUiExtGlyphs = 0xAA0, kDialogueExtGlyphs = 0xA80, kExtCells = 24;
// DIV-0051: the controller panel's icons - six glyphs loc_build.py puts
// after the UI set's blank (its ICONS_AT): circle, cross, triangle, square
// from the disc's 12 x 12 set, L1 and R1 composed of the dialogue capital
// and the small serifed 1, each doubled to fill the 24 x 24 glyph, so the
// 24 px quad shows the icon at the screen's 2x with its own margins.
constexpr std::uint32_t kIconCircle = kUiGlyphs + kSetCells + 1, kIconCross = kIconCircle + 1,
                        kIconTriangle = kIconCircle + 2, kIconSquare = kIconCircle + 3,
                        kIconL1 = kIconCircle + 4, kIconR1 = kIconCircle + 5;
// The quad's x from the call's x (panel x + 0x5D). Measured off the captures
// (analysis/shots/ctrl_icons, 2026-09-24): the row box's divider sits at
// row x + 0x44 whatever the box's width, and the box ends at row x + width.
// The 24 px quad at x - 0x14 runs from row x + 0x44 to 0x5C - the shapes'
// own margins keep their ink a pixel or two inside - and the box, 0x60 wide,
// ends four past it.
constexpr int kIconX = -0x0C;   // the owner, 2026-09-24: half a glyph right of -0x14

constexpr std::uint32_t kTextDrawAt = 0x516B30;   // Text_DrawAt: the sites' target in Capcom's bytes, a patch fact (raw)
constexpr std::uint32_t kBigLabelCall = 0x46189F, kBigOptionCall = 0x4619F9;
constexpr std::uint32_t kCtrlNameCall = 0x461B43;   // Config_DrawControllerRow's one draw (DIV-0026)

// DIV-0051's icon path is on: set once the cell's Inject went in, which is
// under a Latin overlay only (ConfigText_Inject), the overlay whose font
// loc_build.py built the icon glyphs into. Without it the glyphs are past the
// end of the Chinese font, and Text_DrawString stops on Capcom's glyph trap
// (0xC0000096, ecx 0xA65: known-defects D312) - so the cell draws what the
// original draws.
bool g_icons = false;

// The controller panel's cell, original 0x461C00 (docs/controls.md section 2;
// disasm 2026-10-17, 0x461C00..0x461E07). cdecl (x, y, word). The original
// tests the word's bits in the order circle 0x20, cross 0x40, triangle 0x10,
// square 0x80, L1 0x04, R1 0x08 and, for the first it finds, draws
// Text_DrawAt(x + 4, y, colour, 3, 0x66A2FC[i]) - the port's glyph, the
// pointer read in place: colours 2, 1, 6, 5, 0, 0 - then Text_DrawAt(x - 0x10,
// y, 0, count, letter): the default key table's letter, hard-coded at
// 0x65382C, 0x653828, 0x653824, 0x653820, 0x65381C (count 4) and 0x653818
// (known-defects D58). With none of those, select 0x100 (`test ah, 1`) draws
// the three cells of a SELECT label: Gpu_SetDrawMode(Gfx_PacketNext, 0, 0,
// 0xF, 0), Gfx_CommitPrim(1, 0xC), then for u = 6, 7, 8 at x + 1, x + 9,
// x + 0x11: Menu_DrawCell8(x', y + 3, u, 2, Gpu_GetClut(0, 0x1E0), 0x80) - the
// shade the 0x80 the clut call's caller left on the stack. Nothing else.
//
// Under DIV-0051 ours draws one thing, the PlayStation's icon, in that colour,
// where the letter was: the PlayStation screen has one column. Count 3 as
// the original passes it. L2 and R2 have no icon on either side, as before,
// and select none under DIV-0051 (the rebinding's `and ecx, 0xFC` never gives
// a line the bit; a save could).
extern "C" void __cdecl Config_DrawControllerCell(int x, int y, unsigned int word) {
    if (g_icons) {
        struct Icon { unsigned bit; std::uint32_t glyph; int color; };
        static const Icon kIcons[] = {
            {0x20, kIconCircle, 2}, {0x40, kIconCross, 1},  {0x10, kIconTriangle, 6},
            {0x80, kIconSquare, 5}, {0x04, kIconL1, 0},     {0x08, kIconR1, 0},
        };
        for (const Icon& i : kIcons) {
            if (!(word & i.bit)) continue;
            const unsigned char code[3] = {static_cast<unsigned char>(0x80 | (i.glyph >> 8)),
                                           static_cast<unsigned char>(i.glyph), 0};
            SH_CALL(Text_DrawAt)(x + kIconX, y, i.color, 3, code);
            return;
        }
        return;
    }
    struct Cell { unsigned bit; int color; std::uint32_t letter; int letter_count; };
    static const Cell kCells[] = {
        {0x20, 2, 0x65382C, 3}, {0x40, 1, 0x653828, 3}, {0x10, 6, 0x653824, 3},
        {0x80, 5, 0x653820, 3}, {0x04, 0, 0x65381C, 4}, {0x08, 0, 0x653818, 3},
    };
    constexpr std::uint32_t kGlyphPointers = 0x66A2FC;   // the six one-byte glyph strings 0x66A2E0..
    for (unsigned i = 0; i < sizeof kCells / sizeof kCells[0]; ++i) {
        const Cell& c = kCells[i];
        if (!(word & c.bit)) continue;
        const auto* const glyph = reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(
            *reinterpret_cast<const std::uint32_t*>(static_cast<std::uintptr_t>(kGlyphPointers + 4 * i))));
        SH_CALL(Text_DrawAt)(x + 4, y, c.color, 3, glyph);
        SH_CALL(Text_DrawAt)(x - 0x10, y, 0, c.letter_count,
                             reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(c.letter)));
        return;
    }
    if (!(word & 0x100)) return;
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0xF, 0);
    SH_CALL(Gfx_CommitPrim)(1, 0xC);
    for (unsigned u = 6; u <= 8; ++u) {
        const unsigned clut = SH_CALL(Gpu_GetClut)(0, 0x1E0);
        SH_CALL(Menu_DrawCell8)(static_cast<unsigned>(x) + 1 + 8 * (u - 6), static_cast<unsigned>(y) + 3, u, 2, clut,
                                0x80);
    }
}

extern "C" const unsigned char* __cdecl ConfigText_DrawSelected(int x, int y, int color, int count,
                                                                const unsigned char* text) {
    unsigned char big[64];
    std::size_t n = 0;
    while (text[n]) {
        if (n + 3 > sizeof big) bof3::Fatal("config: a selected-row string is over %u bytes",
                                            (unsigned)sizeof big - 1);
        if (text[n] & 0x80) {
            if (!text[n + 1]) bof3::Fatal("config: a selected-row string ends inside a code");
            std::uint32_t g = (static_cast<std::uint32_t>(text[n] & 0x7F) << 8) | text[n + 1];
            if (g >= kUiGlyphs && g < kUiGlyphs + kSetCells) g = g - kUiGlyphs + kDialogueGlyphs;
            else if (g >= kUiExtGlyphs && g < kUiExtGlyphs + kExtCells) g = g - kUiExtGlyphs + kDialogueExtGlyphs;
            big[n] = static_cast<unsigned char>(0x80 | (g >> 8));
            big[n + 1] = static_cast<unsigned char>(g);
            n += 2;
        } else {
            big[n] = text[n];
            ++n;
        }
    }
    big[n] = 0;
    // The original returns a pointer past the string it was given; neither
    // call site reads it, but keep it a pointer into THEIR string, not ours.
    Text_DrawAt(x, y, color, count, big);
    return text + n + 1;
}

}  // namespace

void ConfigText_Apply(std::uint32_t tag, const std::uint8_t* payload, std::uint32_t size) {
    if (tag != 0) bof3::Fatal("config chunk tag is 0x%X, expected 0", (unsigned)tag);
    // FIRST.DAT's overlay is walked again when the game returns to the title
    // (F9 twice in play, the owner 2026-09-27) and its text is the same; the
    // controller pointers and the label sites below already name ours by
    // then, and checking them against the original's values stopped the
    // game ("controller pointer 0 holds ..."). As PauseText_Apply has it.
    static bool applied = false;
    if (applied) return;
    applied = true;
    const std::uint8_t* p = payload;
    const std::uint8_t* const end = payload + size;

    if (p >= end || *p++ != kLabels)
        bof3::Fatal("config chunk: label count is not %u", (unsigned)kLabels);
    for (std::uint32_t i = 0; i < kLabels; ++i) {
        const char* s = TakeString(p, end);
        if (!s) bof3::Fatal("config chunk: ran out inside label %u", (unsigned)i);
        if (std::strlen(s) + 1 > kLabelRoom)
            bof3::Fatal("config chunk: label %u is %u bytes, room is %u", (unsigned)i,
                        (unsigned)std::strlen(s) + 1, (unsigned)kLabelRoom);
        std::memcpy(g_labels[i], s, std::strlen(s) + 1);
    }

    if (p >= end || *p++ != kRecords)
        bof3::Fatal("config chunk: record count is not %u", (unsigned)kRecords);
    for (std::uint32_t i = 0; i < kRecords; ++i) {
        if (p + 2 > end) bof3::Fatal("config chunk: ran out at record %u", (unsigned)i);
        const std::uint8_t count = *p++;
        const std::uint8_t x = *p++;
        const char* s = TakeString(p, end);
        if (!s) bof3::Fatal("config chunk: ran out inside record %u", (unsigned)i);
        if (std::strlen(s) + 1 > kRecordRoom)
            bof3::Fatal("config chunk: option %u is %u bytes, the record holds %u", (unsigned)i,
                        (unsigned)std::strlen(s) + 1, (unsigned)kRecordRoom);
        auto* rec = reinterpret_cast<std::uint8_t*>(
            static_cast<std::uintptr_t>(kRecordBase + i * kRecordStride));
        rec[0] = count;
        rec[1] = x;
        std::memset(rec + 2, 0, kRecordRoom);
        std::memcpy(rec + 2, s, std::strlen(s));
    }

    if (p >= end || *p++ != kCtrl)
        bof3::Fatal("config chunk: controller count is not %u", (unsigned)kCtrl);
    for (std::uint32_t i = 0; i < kCtrl; ++i) {
        const char* s = TakeString(p, end);
        if (!s) bof3::Fatal("config chunk: ran out inside controller name %u", (unsigned)i);
        if (std::strlen(s) + 1 > kCtrlRoom)
            bof3::Fatal("config chunk: controller name %u is %u bytes, room is %u", (unsigned)i,
                        (unsigned)std::strlen(s) + 1, (unsigned)kCtrlRoom);
        std::memcpy(g_ctrl[i], s, std::strlen(s) + 1);
        auto* entry = reinterpret_cast<std::uint32_t*>(
            static_cast<std::uintptr_t>(kCtrlPointers + i * sizeof(std::uint32_t)));
        if (*entry != kCtrlExpected[i])
            bof3::Fatal("config: controller pointer %u holds 0x%08X, expected 0x%08X", (unsigned)i,
                        (unsigned)*entry, (unsigned)kCtrlExpected[i]);
        *entry = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(g_ctrl[i]));
    }

    if (p != end)
        bof3::Fatal("config chunk: %u bytes left over", (unsigned)(end - p));

    // The labels are ours now, so point the code at them. Done here rather
    // than in Inject because the strings must exist first; the chunk arrives
    // with FIRST.DAT, long before the Config screen can be opened.
    for (std::uint32_t i = 0; i < kLabels; ++i) {
        std::uint8_t expected[4], replacement[4];
        std::memcpy(expected, &kLabelSites[i].expected, 4);
        const auto ours = static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(g_labels[i]));
        std::memcpy(replacement, &ours, 4);
        if (bof3::PatchBytes("ConfigText", kLabelSites[i].at, expected, replacement, 4)) g_ops.label[i] = ours;
    }
    bof3::Operands_Check("DIV-0015's Config labels");
    bof3::Log("DIV-0015: Config screen - %u labels, %u options, %u controller names",
              (unsigned)kLabels, (unsigned)kRecords, (unsigned)kCtrl);
}

const ConfigTextOperands& ConfigText_Operands() { return g_ops; }

void ConfigText_Inject() {
    // The values of ours against the operands they stand for (code_bytes.h),
    // whether or not the patches below go in.
    for (std::uint32_t i = 0; i < kLabels; ++i) bof3::Operand("DIV-0015 Config label", kLabelSites[i].at, &g_ops.label[i], 4);
    bof3::Operand("DIV-0017 Config label anchor", 0x46189D, &g_anchor_now, 1);
    bof3::Operand("DIV-0017 Config label anchor", 0x4618ED, &g_anchor_now, 1);
    bof3::Operand("DIV-0017 Config label width", 0x461894, g_label_w_now, sizeof g_label_w_now);
    bof3::Operand("DIV-0017 Config option width", 0x4619E1, g_option_w_now, sizeof g_option_w_now);
    bof3::OperandCall("DIV-0017 Config selected draw", kBigLabelCall, &g_ops.big_draw);
    bof3::OperandCall("DIV-0017 Config selected draw", kBigOptionCall, &g_ops.big_draw);
    bof3::OperandCall("DIV-0026 controller name draw", kCtrlNameCall, &g_ops.name_draw);
    bof3::Operand("DIV-0026 controller name width", 0x461B36, g_name_w_now, sizeof g_name_w_now);
    bof3::Operand("DIV-0026 controller name edge", 0x461B3F, g_edge_now, sizeof g_edge_now);
    bof3::Operand("DIV-0051 controller box width", 0x461B06, g_box_now, sizeof g_box_now);
    bof3::Operand("DIV-0051 controller second cell", 0x461BAC, g_cell2_now, sizeof g_cell2_now);
    bof3::Operand("DIV-0051 controller second cell", 0x461BE9, g_esp_now, sizeof g_esp_now);
    bof3::Operand("DIV-0051 controller frame", 0x461A61, g_frame_now, sizeof g_frame_now);

    // Layout only, and only under a language overlay: with the shipped Chinese
    // text every number below is right as it stands.
    // Lang_Latin: set, not "original", not full-width (DIV-0056). Until
    // 2026-09-29 this tested "set" and "not full-width" alone, so
    // BOF3X_LANG=original laid the Chinese screen out for Latin text, and
    // group FC1's fuzz found the row draw's call re-aimed (known-defects D174).
    if (!Lang_Latin()) return;

    // --- the label column, right-aligned -----------------------------------
    // Both branches of the row draw 0x461800 place a label at
    // `x_arg + 0x3A - width`, so its RIGHT edge is a constant: measured off the
    // owner's screenshot of the Chinese screen at x = 168 of 640, with the
    // separator bar at 292. Four Chinese characters fit left of that; ten Latin
    // ones do not, and "Controller" ran off the panel.
    //
    // 0x5F puts it at 205, which the owner judged right in game on 2026-09-20 -
    // the second try. The first, 0x7F, was set from a reading of the disc
    // screenshot that put the edge near 251; at that anchor the labels ran into
    // the separator bar at 292, so the estimate was wrong and the in-game look
    // is what settled it.
    static const std::uint8_t anchor_was[] = {0x3A};
    static const std::uint8_t anchor_is[] = {0x5F};
    // One name for every patch of this block: all go in, or none
    // (BOF3X_ORIGINAL=ConfigText), so one answer sets every value.
    const bool on = bof3::PatchBytes("ConfigText", 0x46189D, anchor_was, anchor_is, 1);   // Config_DrawRowLabel 0x461800 big path
    bof3::PatchBytes("ConfigText", 0x4618ED, anchor_was, anchor_is, 1);   // Config_DrawRowLabel 0x461800 small path

    // --- the selected row (DIV-0017) ----------------------------------------
    // The glyph swap, and the branch's width reckoned at the 8 units a
    // character the dialogue font advances (DIV-0006) instead of 12:
    //   label  0x461894  lea ecx,[ecx+ecx*2] / shl ecx,1   len * 6
    //                 -> shl ecx,2 / nop / nop             len * 4
    //   option 0x4619E1  lea eax,[eax+eax*2]  (then shl 2)  count * 12
    //                 -> shl eax,1 / nop      (then shl 2)  count * 8
    // which makes each the same number its small branch already computes, so a
    // row does not move when the cursor lands on it - as on the disc.
    bof3::RetargetCall("ConfigText", kBigLabelCall, kTextDrawAt,
                       reinterpret_cast<void*>(&ConfigText_DrawSelected));
    bof3::RetargetCall("ConfigText", kBigOptionCall, kTextDrawAt,
                       reinterpret_cast<void*>(&ConfigText_DrawSelected));
    static const std::uint8_t label_was[] = {0x8D, 0x0C, 0x49, 0xD1, 0xE1};
    static const std::uint8_t label_is[] = {0xC1, 0xE1, 0x02, 0x90, 0x90};
    Patch("ConfigText", 0x461894, label_was, label_is, 5, g_label_w_now);
    static const std::uint8_t option_was[] = {0x8D, 0x04, 0x40};
    static const std::uint8_t option_is[] = {0xD1, 0xE0, 0x90};
    Patch("ConfigText", 0x4619E1, option_was, option_is, 3, g_option_w_now);
    if (on) {
        g_anchor_now = anchor_is[0];
        g_ops.anchor = anchor_is[0];
        g_ops.label_x4 = true;
        g_ops.option_x8 = true;
        g_ops.big_draw = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&ConfigText_DrawSelected));
    }

    // --- the controller panel (DIV-0026) ------------------------------------
    // Config_DrawControllerRow draws each of the six names through the large Text_DrawAt at
    // x = row x + 0x20 - width, the width reckoned `len * 6` - 12 units a
    // Chinese character of two bytes:
    //   0x461B36  lea eax,[ecx+ecx*2]   (then mov ecx,ebp / shl eax,1)
    // Ours are two bytes a character too but advance 8, so each name began 4
    // units a character left of its right edge, and in the large quad the
    // tripled 8 x 8 cells crowded (the owner's screenshot, 2026-09-22). The
    // same pair as the selected row: the SIB byte made `[ecx+ecx]`, so the
    // `shl eax,1` after it gives len * 4, and the call re-aimed at the glyph
    // swap. The right edge, row x + 0x20, is the original's (the doc says why
    // it is not enough: docs/config-screen.md section 9). Not under
    // BOF3X_LANG=original, which DatLoad_Inject reads as no overlay: the
    // Chinese names advance 12 and want the original's width - since
    // 2026-09-29 the Lang_Latin test at the top makes that exception for
    // every patch here (docs/glyph-draw.md section 8 had it for this one).
    {
        // One name again: all of the controller panel's go in, or none.
        const bool ctrl_on = bof3::RetargetCall("ConfigController", kCtrlNameCall, kTextDrawAt,
                                                reinterpret_cast<void*>(&ConfigText_DrawSelected));
        static const std::uint8_t ctrl_was[] = {0x8D, 0x04, 0x49};
        static const std::uint8_t ctrl_is[] = {0x8D, 0x04, 0x09};
        Patch("ConfigController", 0x461B36, ctrl_was, ctrl_is, 3, g_name_w_now);

        // The owner's second look, 2026-09-23 (in game, English): "Change"
        // and "Action" still began left of the panel's frame, and the rows ran
        // past its right side. The names' right edge from row x + 0x20 to
        // x + 0x36 (`add ecx, 0x20` at 0x461B3F): 22 units in, "Change" inside
        // the frame with a margin, every name still left of the separator at
        // x + 0x3F. The frame (DIV-0011's Menu_DrawFrame through the call at
        // 0x461A84) from 0xC cells to 0xF (`push 0xC` at 0x461A61): 0xE, the
        // first try, still left the rows' dark boxes poking past its right side
        // in game, so one cell more (owner, 2026-09-23).
        static const std::uint8_t edge_was[] = {0x83, 0xC1, 0x20};
        static const std::uint8_t edge_is[] = {0x83, 0xC1, 0x36};
        Patch("ConfigController", 0x461B3F, edge_was, edge_is, 3, g_edge_now);
        // DIV-0051 (2026-09-24, the owner's choice: the PlayStation's one
        // column): the frame from DIV-0026's 0xF cells to 0xD (below), the
        // row's dark box from
        // 0x68 wide to 0x60 (`push 0x68` at 0x461B06, Menu_DrawRowBox's
        // width), the second cell's set-up skipped - 0x461BAC..0x461BE8,
        // the `mov esi, [0x7E0670]` at its start made a jump to the
        // `add esp, 0x44` at 0x461BE9, which loses that block's three
        // pushes (push esi for 0x5A7650, push 0x20 / push 1 for 0x461E50)
        // and becomes `add esp, 0x38` - and the cell's contents ours.
        static const std::uint8_t box_was[] = {0x6A, 0x68};
        static const std::uint8_t box_is[] = {0x6A, 0x58};   // the owner: half a glyph shorter than 0x60
        Patch("ConfigController", 0x461B06, box_was, box_is, 2, g_box_now);
        static const std::uint8_t cell2_was[] = {0x8B, 0x35, 0x70, 0x06, 0x7E, 0x00};
        static const std::uint8_t cell2_is[] = {0xEB, 0x3B, 0x90, 0x90, 0x90, 0x90};
        Patch("ConfigController", 0x461BAC, cell2_was, cell2_is, 6, g_cell2_now);
        static const std::uint8_t esp_was[] = {0x83, 0xC4, 0x44};
        static const std::uint8_t esp_is[] = {0x83, 0xC4, 0x38};
        Patch("ConfigController", 0x461BE9, esp_was, esp_is, 3, g_esp_now);
        // The frame from DIV-0026's 0xF cells to 0xD: 0x68 px from the panel's
        // x, three past the row box's end at panel x + 0x65.
        static const std::uint8_t frame_was[] = {0x6A, 0x0C, 0x32};
        static const std::uint8_t frame_is[] = {0x6A, 0x0D, 0x32};   // the owner: a cell past the box, as at the left
        Patch("ConfigController", 0x461A61, frame_was, frame_is, 3, g_frame_now);
        if (ctrl_on) {
            g_ops.name_draw = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&ConfigText_DrawSelected));
            g_ops.name_x4 = true;
            g_ops.name_edge = edge_is[2];
            g_ops.box_width = box_is[1];
            g_ops.second_cell = false;
            g_ops.frame_w = frame_is[1];
        }
        bof3::Inject("Config_DrawControllerCell", bof3::addr::Config_DrawControllerCell,
                     reinterpret_cast<void*>(&Config_DrawControllerCell));
        // Ours is called directly by our Config_DrawControllerPanel
        // (rest_4f.cpp, since round 14's R4F) whether or not this Inject went
        // in, so the icons follow the Inject, not the call: OFF under
        // BOF3X_ORIGINAL (the jmp on ours reaches Capcom's), and never without
        // a Latin overlay, whose return above leaves the original path (D312).
        g_icons = bof3::IsEnabled(bof3::addr::Config_DrawControllerCell);
    }

    // The rows' y is the original's. An earlier build lowered every string on
    // this screen by two (five displacements in 0x461800, Config_DrawRowOptions and
    // Config_DrawControllerRow), tuned against cells that sat in the top of their 24 x 24 slot.
    // A tripled 8 x 8 cell fills the slot, and with it the owner saw the text
    // two pixels low - sitting on the row's floor - so the drop is gone
    // (2026-09-20).
}

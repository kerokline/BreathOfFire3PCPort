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
// 0x461800, and the address each must hold before we touch it.
struct LabelSite {
    std::uint32_t at;
    std::uint32_t expected;
};
constexpr LabelSite kLabelSites[kLabels] = {
    {0x461832, 0x669F0C}, {0x46183A, 0x669F18}, {0x461842, 0x669F24},
    {0x46184A, 0x669F2C}, {0x461852, 0x669F34}, {0x46185A, 0x669F3C},
};

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
// The row under the cursor is drawn large: 0x461800 and 0x461970 each have a
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
constexpr std::uint32_t kTextDrawAt = 0x516B30;   // its name is a macro here
constexpr std::uint32_t kBigLabelCall = 0x46189F, kBigOptionCall = 0x4619F9;
constexpr std::uint32_t kCtrlNameCall = 0x461B43;   // 0x461AF0's one draw (DIV-0026)

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
        bof3::PatchBytes("ConfigText", kLabelSites[i].at, expected, replacement, 4);
    }
    bof3::Log("DIV-0015: Config screen - %u labels, %u options, %u controller names",
              (unsigned)kLabels, (unsigned)kRecords, (unsigned)kCtrl);
}

void ConfigText_Inject() {
    // Layout only, and only under a language overlay: with the shipped Chinese
    // text every number below is right as it stands.
    char lang[16];
    if (GetEnvironmentVariableA("BOF3X_LANG", lang, sizeof lang) == 0) return;

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
    bof3::PatchBytes("ConfigText", 0x46189D, anchor_was, anchor_is, 1);   // 0x461800 big path
    bof3::PatchBytes("ConfigText", 0x4618ED, anchor_was, anchor_is, 1);   // 0x461800 small path

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
    bof3::PatchBytes("ConfigText", 0x461894, label_was, label_is, 5);
    static const std::uint8_t option_was[] = {0x8D, 0x04, 0x40};
    static const std::uint8_t option_is[] = {0xD1, 0xE0, 0x90};
    bof3::PatchBytes("ConfigText", 0x4619E1, option_was, option_is, 3);

    // --- the controller panel (DIV-0026) ------------------------------------
    // 0x461AF0 draws each of the six names through the large Text_DrawAt at
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
    // Chinese names advance 12 and want the original's width. (The patches
    // above this one do not make that exception; docs/glyph-draw.md section 8.)
    if (std::strcmp(lang, "original") != 0) {
        bof3::RetargetCall("ConfigController", kCtrlNameCall, kTextDrawAt,
                           reinterpret_cast<void*>(&ConfigText_DrawSelected));
        static const std::uint8_t ctrl_was[] = {0x8D, 0x04, 0x49};
        static const std::uint8_t ctrl_is[] = {0x8D, 0x04, 0x09};
        bof3::PatchBytes("ConfigController", 0x461B36, ctrl_was, ctrl_is, 3);
    }

    // The rows' y is the original's. An earlier build lowered every string on
    // this screen by two (five displacements in 0x461800, 0x461970 and
    // 0x461AF0), tuned against cells that sat in the top of their 24 x 24 slot.
    // A tripled 8 x 8 cell fills the slot, and with it the owner saw the text
    // two pixels low - sitting on the row's floor - so the drop is gone
    // (2026-09-20).
}

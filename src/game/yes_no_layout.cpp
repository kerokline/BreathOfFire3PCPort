// DIVERGENCE DIV-0027: the Yes / No chooser's layout for Latin text.
//
// Menu_YesNo 0x5747D0 (read to its last ret, docs/glyph-draw.md section 7) is
// the chooser under the save screen's "OK to overwrite?" and three more
// prompts. It draws one line, system message 0xF, through Text_DrawAt at
// x 0x1C, and the pointing hand through Menu_DrawHand at
// x = 0xFE - 36 * selection (1 is Yes, 0 is No). The words' places are the
// line's own spaces: the Chinese line is 16 spaces, Yes, 2 spaces, No, at 12
// units a character, so its words start at 220 and 256 - and the hand's
// stops, 218 and 254 (its visible tip is one unit left of x), were fitted to
// exactly that. The English line the overlay carries is 27 spaces, Yes,
// 1 space, No at 8 units: its words start at 244 and 276, and the hand
// stops 25 units short of each (the owner's screenshots, 2026-09-22).
//
// The owner's layout (docs/dialogue-localisation.md section 6 item 8): the
// left hand and "No" stay; "Yes" moves left to start a few units right of
// the left hand's tip; the right hand moves right to end a few units before
// "No". So:
//
//   - the line: three spaces moved from its lead to between the words - Yes
//     at 28 + 8 * 24 = 220, three units right of the left tip at 217; No
//     still at 220 + 24 + 8 * 4 = 276;
//   - the hand: x = 0x112 - 56 * selection - Yes 218 as before, No 274, its
//     tip at 273, three units before No:
//       0x5747F0  mov ecx, 0xFE          -> mov ecx, 0x112
//       0x5747F7  lea eax, [eax+eax*8]   -> imul eax, eax, 56
//       0x5747FC  shl eax, 2             -> three nops
//     The selection is `movsx ax`: eax's high half is whatever Text_DrawAt
//     returned, and only the low 16 bits of the product are right - as in the
//     original, whose callee keeps only the low 16 (`and ecx, 0xFFFF` at
//     0x590601).
//
// Under a language overlay only (BOF3X_LANG set, and not "original"): the
// Chinese line is what the stops were made for. All four prompts that use
// the chooser get the new layout, since it is one function.
#include "game/yes_no_layout.h"

#include <windows.h>

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

constexpr std::uint32_t kLineCall = 0x5747D2;       // call Msg_SystemPtr(0xF)
constexpr std::uint32_t kMsgSystemPtr = 0x497740;   // its name is our prototype here
constexpr unsigned kMoved = 3;                      // spaces moved from the lead to the gap

unsigned char g_line[96];

// A word: a run of glyph codes - a byte above 0x20, two bytes where the first
// has bit 7. Returns the index after it.
unsigned SkipWord(const unsigned char* s, unsigned i) {
    while (s[i] > 0x20) {
        if (s[i] & 0x80) {
            if (!s[i + 1]) return i + 1;   // a code cut short: the shape check refuses it
            i += 2;
        } else {
            ++i;
        }
    }
    return i;
}

const unsigned char* Respace(const unsigned char* s, unsigned id) {
    unsigned lead = 0;
    while (s[lead] == 0x20) ++lead;
    const unsigned first_end = SkipWord(s, lead);
    unsigned second = first_end;
    while (s[second] == 0x20) ++second;
    const unsigned end = SkipWord(s, second);
    if (lead < kMoved || first_end == lead || second == first_end || end == second || s[end] != 0)
        bof3::Fatal("DIV-0027: system message 0x%X is not spaces, a word, spaces, a word "
                    "(lead %u, words at %u..%u and %u..%u, then byte 0x%02X)",
                    id, lead, lead, first_end, second, end, s[end]);
    if (end + 1 > sizeof g_line) bof3::Fatal("DIV-0027: system message 0x%X is %u bytes", id, end);
    unsigned n = 0;
    for (unsigned k = 0; k < lead - kMoved; ++k) g_line[n++] = 0x20;
    for (unsigned k = lead; k < first_end; ++k) g_line[n++] = s[k];
    for (unsigned k = 0; k < second - first_end + kMoved; ++k) g_line[n++] = 0x20;
    for (unsigned k = second; k < end; ++k) g_line[n++] = s[k];
    g_line[n] = 0;
    return g_line;
}

extern "C" const unsigned char* __cdecl YesNo_Line(unsigned id) { return Respace(Msg_SystemPtr(id), id); }

// BOF3X_SHADOW=yes_no_layout: the re-spacing on the two lines it will meet,
// built here - the English line's shape and the Chinese line's (two-byte
// codes) - against the lines it must make.
void SelfTest() {
    // Each line: lead spaces, first word's bytes, gap spaces, second word's
    // bytes. The Chinese words are two-byte codes (the shipped line's shape:
    // one code, then two), written as bytes.
    struct Case { unsigned lead; unsigned char w1[4]; unsigned gap; unsigned char w2[6]; };
    static const Case kCases[] = {
        {27, {'Y', 'e', 's', 0}, 1, {'N', 'o', 0}},
        {16, {0x86, 0x13, 0}, 2, {0x80, 0xCC, 0x86, 0x13, 0}},
    };
    auto build = [](unsigned char* out, unsigned lead, const unsigned char* w1, unsigned gap, const unsigned char* w2) {
        unsigned n = 0;
        for (unsigned k = 0; k < lead; ++k) out[n++] = 0x20;
        for (unsigned k = 0; w1[k]; ++k) out[n++] = w1[k];
        for (unsigned k = 0; k < gap; ++k) out[n++] = 0x20;
        for (unsigned k = 0; w2[k]; ++k) out[n++] = w2[k];
        out[n] = 0;
    };
    for (const Case& c : kCases) {
        unsigned char in[64], want[64];
        build(in, c.lead, c.w1, c.gap, c.w2);
        build(want, c.lead - kMoved, c.w1, c.gap + kMoved, c.w2);
        const unsigned char* got = Respace(in, 0xF);
        if (std::strcmp(reinterpret_cast<const char*>(got), reinterpret_cast<const char*>(want)) != 0)
            bof3::Fatal("DIV-0027 self-test: a line of lead %u and gap %u was not re-spaced to %u and %u", c.lead,
                        c.gap, c.lead - kMoved, c.gap + kMoved);
    }
    bof3::Log("shadow      yes_no_layout self-test: %u lines re-spaced as wanted",
              (unsigned)(sizeof kCases / sizeof kCases[0]));
}

}  // namespace

void YesNoLayout_Inject() {
    char lang[16];
    const DWORD n = GetEnvironmentVariableA("BOF3X_LANG", lang, sizeof lang);
    if (bof3::WantsShadow("yes_no_layout")) SelfTest();
    if (n == 0 || n >= sizeof lang || std::strcmp(lang, "original") == 0) return;

    bof3::RetargetCall("YesNoLayout", kLineCall, kMsgSystemPtr, reinterpret_cast<void*>(&YesNo_Line));
    static const std::uint8_t stop_was[] = {0xB9, 0xFE, 0x00, 0x00, 0x00};
    static const std::uint8_t stop_is[] = {0xB9, 0x12, 0x01, 0x00, 0x00};
    bof3::PatchBytes("YesNoLayout", 0x5747F0, stop_was, stop_is, 5);
    static const std::uint8_t step_was[] = {0x8D, 0x04, 0xC0};
    static const std::uint8_t step_is[] = {0x6B, 0xC0, 0x38};
    bof3::PatchBytes("YesNoLayout", 0x5747F7, step_was, step_is, 3);
    static const std::uint8_t shift_was[] = {0xC1, 0xE0, 0x02};
    static const std::uint8_t shift_is[] = {0x90, 0x90, 0x90};
    bof3::PatchBytes("YesNoLayout", 0x5747FC, shift_was, shift_is, 3);
    bof3::Log("DIV-0027    Yes / No: %u spaces into the gap, hand stops 218 and 274 (on unless the lines above say OFF)", kMoved);

    // DIV-0029: the save / load slot's name two units further in. The slot
    // panel 0x576960 draws the name - five bytes of the save header copied to
    // 0x904BA0 - through Text_DrawAt at x + 0x13 (`lea eax, [ebp + 0x13]` at
    // 0x576A46; the one call site, found live 2026-09-22 by a probe on
    // Text_DrawAt). On screen the glyph's first two pixel columns are lost
    // (at x 135 of 640 in slot 1): a Chinese glyph's are blank, but the Latin
    // cells start at column 0, so an R loses its stem (owner's screenshot).
    // What cuts them is not established (the draw mode the panel sends first
    // takes its texture window from the caller's ebx). 0x15 puts the whole
    // glyph past the cut with a pixel to spare; five Latin letters still end
    // well inside the name box.
    static const std::uint8_t name_was[] = {0x8D, 0x45, 0x13};
    static const std::uint8_t name_is[] = {0x8D, 0x45, 0x15};
    bof3::PatchBytes("SaveNameInset", 0x576A46, name_was, name_is, 3);
    bof3::Log("DIV-0029    save slot names at x + 0x15 (on unless the line above says OFF)");
}

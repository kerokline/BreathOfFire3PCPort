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
// Under a Latin language overlay only (BOF3X_LANG set, not "original", and
// not full-width - DIV-0056): the Chinese line is what the stops were made
// for. All four prompts that use the chooser get the new layout, since it is
// one function. YesNoLayout_Inject also applies DIV-0029 (below).
//
// Amended 2026-10-03 (group YN, docs/yes-no-prompts.md): two prompts outside
// Menu_YesNo that carry their answers on the question's own line get the
// same spacing - YesNoLayout_Tail - the master's here (MasterScreen_AskYesNo 0x586D20,
// two calls re-aimed) and Manillo's in effect_1g.cpp (ours).
#include "game/yes_no_layout.h"

#include <windows.h>

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/lang_layout.h"
#include "game/text_advance.h"
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

// `s` (system message `id`) with kMoved spaces moved from its lead into the
// gap between its two words, in g_line - same length, so it fits whenever
// `s` does. Aborts on any other shape.
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

// ---------------------------------------------------------------------------
// DIV-0027, amended 2026-10-03 (group YN): prompts that carry their own
// answers at the end of the question's line - the master's "Is this OK?"
// (MasterScreen_AskYesNo 0x586D20) and Manillo's "Will that be all?" (ours,
// effect_1g.cpp's LeavePrompt). Each draws one line - the question, spaces,
// the two answers one space apart, ending at the line's 33rd character - and
// puts the hand at a Chinese-fitted base + 36 * the answer. The load / save
// screen's spacing, which the owner asked for (2026-10-03): the first answer
// three spaces further left, the hand two units left of each answer (its
// visible tip three units before the word), the second answer where it was.

unsigned char g_tail[128];

// A character's byte length: two where the first byte has bit 7 (a second
// byte may be anything, 0x20 included), else one.
unsigned CharLen(const unsigned char* s, unsigned i) { return (s[i] & 0x80) ? 2u : 1u; }

}  // namespace

YesNoTail YesNoLayout_Tail(const unsigned char* s, int x, const char* who) {
    // The characters' byte offsets, refusing a control byte (the width below
    // would not know it) and a two-byte code cut short.
    unsigned starts[128];
    unsigned n = 0, i = 0;
    while (s[i]) {
        if (n == sizeof starts / sizeof starts[0]) bof3::Fatal("DIV-0027: %s's line is over %u characters", who, n);
        if (s[i] < 0x20 || ((s[i] & 0x80) && !s[i + 1]))
            bof3::Fatal("DIV-0027: %s's line has byte 0x%02X at %u - not a character this layout can measure", who,
                        s[i], i);
        starts[n++] = i;
        i += CharLen(s, i);
    }
    const unsigned len = i;
    auto space = [&](unsigned k) { return s[starts[k]] == 0x20; };
    // From the end: the second answer, its gap, the first answer, its gap.
    unsigned k = n;
    const unsigned w2_end = k;
    while (k > 0 && !space(k - 1)) --k;
    const unsigned w2 = k;
    while (k > 0 && space(k - 1)) --k;
    const unsigned g2 = k;
    while (k > 0 && !space(k - 1)) --k;
    const unsigned w1 = k;
    while (k > 0 && space(k - 1)) --k;
    const unsigned g1 = k;
    if (w2 == w2_end || g2 == w2 || w1 == g2 || g1 == 0 || w1 - g1 < kMoved + 1)
        bof3::Fatal("DIV-0027: %s's line is not a question, spaces, a word, spaces, a word with %u spaces to spare "
                    "(%u characters: question to %u, answers at %u and %u)",
                    who, kMoved + 1, n, g1, w1, w2);
    if (len + 1 > sizeof g_tail) bof3::Fatal("DIV-0027: %s's line is %u bytes", who, len);
    // The same bytes with kMoved spaces moved from the first gap to the second.
    unsigned out = 0;
    auto copy = [&](unsigned from, unsigned to) {   // characters [from, to)
        const unsigned a = starts[from], b = to < n ? starts[to] : len;
        std::memcpy(g_tail + out, s + a, b - a);
        out += b - a;
    };
    copy(0, w1 - kMoved);
    copy(w1, g2);
    for (unsigned m = 0; m < kMoved; ++m) g_tail[out++] = 0x20;
    copy(g2, n);
    g_tail[out] = 0;
    // The pen's place at each answer, the advances summed as the draw sums them.
    int pen = x, stop[2] = {0, 0};
    unsigned at = 0, word = 0;
    while (g_tail[at]) {
        if (word < 2 && at == (word == 0 ? starts[w1] - kMoved : starts[w2])) stop[word++] = pen - 2;
        pen += TextAdvance_Of(g_tail + at);
        at += CharLen(g_tail, at);
    }
    if (word != 2) bof3::Fatal("DIV-0027: %s's re-spaced line lost an answer", who);
    return {g_tail, {stop[0], stop[1]}};
}

namespace {

// The master's prompt, MasterScreen_AskYesNo 0x586D20 (docs/yes-no-prompts.md): its line
// Text_DrawAt(0x1B, 0x13, 0, 0xFF, MessagePools + word) at 0x586E78 and its
// hand Menu_DrawHand(0xCF + 36 * the byte 0x9398D2, 0x15, 0) at 0x586E97 -
// 0 is Yes. Both calls re-aimed here; the line's stops kept for the hand.
// Ours since round fourteen (R2D, rest_2d.cpp): it calls where these two sites
// reach, so the re-aim below acts on it as it did on the original body.
constexpr std::uint32_t kMasterLineCall = 0x586E78;
constexpr std::uint32_t kMasterHandCall = 0x586E97;
constexpr std::uint32_t kMenuDrawHand = 0x5905D0;
constexpr std::uint32_t kTextDrawAt = 0x516B30;
constexpr unsigned kMasterHandBase = 0xCF, kMasterHandStep = 36;

int g_master_stop[2];
bool g_master_line = false;

extern "C" const unsigned char* __cdecl MasterAsk_Line(int x, int y, int color, int count, const unsigned char* s) {
    const YesNoTail t = YesNoLayout_Tail(s, x, "the master's prompt");
    g_master_stop[0] = t.stop[0];
    g_master_stop[1] = t.stop[1];
    g_master_line = true;
    const unsigned char* end = Text_DrawAt(x, y, color, count, t.line);
    return s + (end - t.line);   // same length: where the original's return would point
}

extern "C" void __cdecl MasterAsk_Hand(int x, int y, int unused) {
    // Capcom's x is 0xCF + 36 * the answer byte; Menu_DrawHand keeps its low 16 bits.
    const unsigned v = static_cast<unsigned>(x) & 0xFFFF;
    if (!g_master_line || v < kMasterHandBase || (v - kMasterHandBase) % kMasterHandStep)
        bof3::Fatal("DIV-0027: the master's hand at x 0x%X (%s) is not 0x%X + %u * an answer after its line", v,
                    g_master_line ? "line drawn" : "no line", kMasterHandBase, kMasterHandStep);
    g_master_line = false;
    const int answer = static_cast<int>((v - kMasterHandBase) / kMasterHandStep);
    Menu_DrawHand(g_master_stop[0] + answer * (g_master_stop[1] - g_master_stop[0]), y, unused);
}

}  // namespace

const unsigned char* YesNoLayout_SystemLine(const unsigned char* line) { return Respace(line, 0xF); }

int YesNoLayout_ShopHandX(int line_x, int answer) {
    const unsigned char* const line = Respace(Msg_SystemPtr(0xF), 0xF);
    // Respace has checked the shape: spaces, a word, spaces, a word.
    int pen = line_x, word = 0, stops[2] = {0, 0};
    bool in_word = false;
    for (unsigned i = 0; line[i]; i += (line[i] & 0x80) ? 2u : 1u) {
        const bool space = line[i] == 0x20;
        if (!space && !in_word && word < 2) stops[word++] = pen - 2;
        in_word = !space;
        pen += TextAdvance_Of(line + i);
    }
    if (word != 2) bof3::Fatal("DIV-0027: system message 0xF re-spaced has %d words", word);
    return stops[1] - answer * (stops[1] - stops[0]);
}

namespace {

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
    // The amendment's prompts: question, gap, answer, gap, answer - the
    // shapes the en / fr / de overlays carry (a question with spaces of its
    // own, a two-byte code in it, answers of 2..4 letters), checked against
    // the line they must make and the pen's place at each answer, summed
    // here character by character.
    struct TailCase { const char* question; unsigned gap1; const char* a1; unsigned gap2; const char* a2; };
    static const TailCase kTails[] = {
        {"Will that be all?", 10, "Yes", 1, "No"},
        {"Is this OK?", 16, "Yes", 1, "No"},
        {"Ist das okay?", 13, "Ja", 1, "Nein"},
        {"Est-ce que \x8a\x89" "a va?", 9, "Oui", 1, "Non"},
    };
    auto tail = [](unsigned char* out, const TailCase& c, unsigned gap1, unsigned gap2, unsigned* a1_at, unsigned* a2_at) {
        unsigned n = 0;
        for (const char* p = c.question; *p; ++p) out[n++] = static_cast<unsigned char>(*p);
        for (unsigned k = 0; k < gap1; ++k) out[n++] = 0x20;
        *a1_at = n;
        for (const char* p = c.a1; *p; ++p) out[n++] = static_cast<unsigned char>(*p);
        for (unsigned k = 0; k < gap2; ++k) out[n++] = 0x20;
        *a2_at = n;
        for (const char* p = c.a2; *p; ++p) out[n++] = static_cast<unsigned char>(*p);
        out[n] = 0;
    };
    for (const TailCase& c : kTails) {
        unsigned char in[96], want[96];
        unsigned a1, a2, w1, w2;
        tail(in, c, c.gap1, c.gap2, &a1, &a2);
        tail(want, c, c.gap1 - kMoved, c.gap2 + kMoved, &w1, &w2);
        const int x = 0x18;
        const YesNoTail got = YesNoLayout_Tail(in, x, "the self-test");
        int pen = x, at1 = 0, at2 = 0;
        for (unsigned i = 0; want[i]; i += (want[i] & 0x80) ? 2 : 1) {
            if (i == w1) at1 = pen;
            if (i == w2) at2 = pen;
            pen += TextAdvance_Of(want + i);
        }
        if (std::strcmp(reinterpret_cast<const char*>(got.line), reinterpret_cast<const char*>(want)) != 0 ||
            got.stop[0] != at1 - 2 || got.stop[1] != at2 - 2)
            bof3::Fatal("DIV-0027 self-test: \"%s\" re-spaced wrong or stopped at %d / %d, not %d / %d", c.question,
                        got.stop[0], got.stop[1], at1 - 2, at2 - 2);
    }
    bof3::Log("shadow      yes_no_layout self-test: %u lines re-spaced as wanted, %u prompts' tails and stops",
              (unsigned)(sizeof kCases / sizeof kCases[0]), (unsigned)(sizeof kTails / sizeof kTails[0]));
}

}  // namespace

void YesNoLayout_Inject() {
    if (bof3::WantsShadow("yes_no_layout")) SelfTest();
    if (!Lang_Latin()) return;   // DIV-0056: the original's stops were fitted to full-width words

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

    // DIV-0027, amended 2026-10-03: the master's "Is this OK?" (0x586D20).
    bof3::RetargetCall("MasterAskLayout", kMasterLineCall, kTextDrawAt, reinterpret_cast<void*>(&MasterAsk_Line));
    bof3::RetargetCall("MasterAskLayout", kMasterHandCall, kMenuDrawHand, reinterpret_cast<void*>(&MasterAsk_Hand));
    bof3::Log("DIV-0027    the master's prompt: its answers re-spaced, the hand two units left of each (on unless the "
              "lines above say OFF)");

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

// Menu_YesNo 0x5747D0 is ours since the sixth round (src/game/menu_windows.cpp):
// it asks here what YesNoLayout_Inject put into Capcom's body - read back from
// the bytes themselves, so it is whatever went in, BOF3X_ORIGINAL=YesNoLayout
// and the language included.
YesNoLayout_LineFn YesNoLayout_ActiveLine() {
    const auto* site = reinterpret_cast<const std::uint8_t*>(static_cast<std::uintptr_t>(kLineCall));
    std::int32_t rel;
    std::memcpy(&rel, site + 1, sizeof rel);
    const std::uint32_t target = kLineCall + 5 + static_cast<std::uint32_t>(rel);
    const auto ours = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&YesNo_Line));
    if (site[0] != 0xE8 || (target != kMsgSystemPtr && target != ours))
        bof3::Fatal("YesNoLayout: 0x%08X is not a call to Msg_SystemPtr or to ours", (unsigned)kLineCall);
    return target == ours ? &YesNo_Line : nullptr;
}

// The 15 bytes from 0x5747F0 to the shift: the three patched instructions
// with the two pushes between them (6A 00, 6A 18) that PatchBytes leaves.
bool YesNoLayout_StopsMoved() {
    const auto* at = reinterpret_cast<const std::uint8_t*>(static_cast<std::uintptr_t>(0x5747F0));
    static const std::uint8_t moved[] = {0xB9, 0x12, 0x01, 0x00, 0x00, 0x6A, 0x00, 0x6B, 0xC0, 0x38, 0x6A, 0x18, 0x90, 0x90, 0x90};
    static const std::uint8_t original[] = {0xB9, 0xFE, 0x00, 0x00, 0x00, 0x6A, 0x00, 0x8D, 0x04, 0xC0, 0x6A, 0x18, 0xC1, 0xE0, 0x02};
    if (std::memcmp(at, moved, sizeof moved) == 0) return true;
    if (std::memcmp(at, original, sizeof original) == 0) return false;
    bof3::Fatal("YesNoLayout: the hand's stops at 0x5747F0 are neither the original's nor DIV-0027's");
    return false;
}

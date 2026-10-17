// DIVERGENCE DIV-0069: the fishing minigame's text from a language overlay.
//
// The fishing spot's two pieces of Chinese that every overlay left as shipped
// (read 2026-10-03, docs/fishing-text.md):
//
//   1  the thirteen lines effect kind 0xF types across the top window ("set rod
//      and lure", "quit fishing", the cast's and the lost catch's), 8-byte
//      records at 0x653B98: a pointer, a label byte (+4: which button label
//      follows the line, 0xFF none) and a pause byte (+5). Only the records'
//      pointers reach the strings (0x669FC0..0x66A06D; pe_xref: the kind's
//      states 0x466260.. and 0x4661D0 / 0x466210, all ours in effect_1a.cpp),
//      so the strings go into buffers of ours and the pointers are re-aimed;
//   2  the three tab labels over the equip menu, three 8-byte slots at
//      0x66A070 behind the pointer table 0x66A088, which only
//      EffectKind0F_DrawToggles (0x468AC0, ours in effect_1b.cpp) reads: the
//      same, three pointers re-aimed.
//
// The strings are the disc's own (tools/loc_build.py, chunk kind 16): the
// fishing module every fishing area carries has the same records - 12 bytes,
// a count added - before a row table the PC still has byte for byte.
//
// And the layout Latin text wants, armed after every self-test under a Latin
// overlay only (the US module's own constants say what: its typing step is 4
// frames at two units a frame, 8 units a letter, where the PC's is 6 for a
// 12-unit glyph - effect_1a.cpp's LineStart / LineType / LineNext):
//
//   - each character is typed after half its own advance in frames, so it
//     lands where the flip cursor ends, at the window's right edge;
//   - the flip cursor is the character's advance wide;
//   - a line's button label follows the line's real pen width, not 12 a
//     character, and is DIV-0051's PlayStation icon for the button the port's
//     own label names (its circled numerals 1, 2, 3 are circle, cross and
//     triangle, entries 0..2 of 0x66A2FC, the table the Config screen's
//     0x461C00 reads with the same colours 2, 1, 6 the lines' 0x653C00 has) -
//     the overlay paints the single-byte slots of those numerals with
//     letters, which is what showed as "b", "c", "d";
//   - the tabs are centred in their 0x28-wide boxes by their real width, to
//     the NUL (the original's counts 2, 3, 2 and x 0xA, 0x35, 0x6A fit two
//     12-unit glyphs);
//   - an accessory's name is drawn to 12 characters where the original
//     stopped at 8 (the item lists, the equipped panel, the cast's panel).
//
// Ledger draft D (the button round's IC-F, amends DIV-0069; docs/fishing-text.md
// 8), armed after sigma::Arm with or without an overlay: a label names the
// button its line's test is on now - Sigma of the line byte's button, word 7's
// for the cast's line (text 0) - drawn as that button's DIV-0051 icon under a
// Latin overlay, else as the port's own stand-in for it (0x66A2FC's entry), in
// that button's pen. At the identity Config every label is the original's.
#include "game/fishing_text.h"

#include <algorithm>
#include <cstring>
#include <string>

#include "game/lang_layout.h"
#include "game/text_advance.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace {

constexpr std::uint32_t kLineRecords = 0x653B98;   // 13 records of 8: pointer, label byte, pause byte
constexpr std::uint32_t kLineCount = 13;
constexpr std::uint32_t kLineRoom = 64;            // tools/loc_build.py FISH_LINE_ROOM
// The records' shipped pointers (read from the image 2026-10-03; 1 and 12
// share one string).
constexpr std::uint32_t kLineShipped[kLineCount] = {0x669FC0, 0x669FC8, 0x669FDC, 0x669FE8, 0x669FF0,
                                                    0x66A004, 0x66A010, 0x66A020, 0x66A030, 0x66A040,
                                                    0x66A058, 0x66A06C, 0x669FC8};
constexpr std::uint32_t kTabTable = 0x66A088;      // 3 pointers, read by 0x468AC0 only
constexpr std::uint32_t kTabCount = 3;
constexpr std::uint32_t kTabRoom = 16;             // tools/loc_build.py FISH_TAB_ROOM
constexpr std::uint32_t kTabShipped[kTabCount] = {0x66A070, 0x66A078, 0x66A080};

// DIV-0051's icons: loc_build.py's ICONS_AT, after the 8 x 8 set (0xA00, 100
// cells) and its blank - circle, cross, triangle, square, L1, R1, as
// config_text.cpp's kIconCircle .. kIconR1.
constexpr std::uint32_t kIconCircle = 0xA00 + 100 + 1;
constexpr unsigned kEntries = 6;
#define FISH_ICON(k)                                                               \
    {static_cast<unsigned char>(0x80 | ((kIconCircle + (k)) >> 8)), static_cast<unsigned char>(kIconCircle + (k)), 0}
constexpr unsigned char kLabelCodes[kEntries][3] = {FISH_ICON(0), FISH_ICON(1), FISH_ICON(2),
                                                    FISH_ICON(3), FISH_ICON(4), FISH_ICON(5)};
#undef FISH_ICON

// Ledger draft D: the label through sigma. The six entries of 0x66A2FC in the
// order the Config cell 0x461C00 reads them (its loads 0x66A2FC..0x66A310 under
// its tests of 0x20, 0x40, 0x10, 0x80, 0x04, 0x08), and each button's pen as
// that cell pushes it (2, 1, 6, 5, 0, 0; read 2026-10-17). The first three are
// 0x653C00's bytes, which the self-test checks against the image.
constexpr std::uint16_t kEntryBit[kEntries] = {0x20, 0x40, 0x10, 0x80, 0x04, 0x08};
constexpr unsigned kEntryPen[kEntries] = {2, 1, 6, 5, 0, 0};
constexpr std::uint32_t kLinePens = 0x653C00;      // bytes by a line byte: 2, 1, 6
constexpr std::uint32_t kConfirmWord = 0x90358E;   // Field_ConfirmButtons, word 7, u16
// Text 0 is the cast's line: row 0 (0x653C04, the stage-1 row with both items
// set) column 0, line byte 0. Stage 1's 0x20 (LeaderPanel_S1Buttons 0x528A90)
// leads to stage 2, whose LeaderPanel_S2Wait 0x528FA0 casts on word 7. Every
// other labelled text names a constant test: rows 0 and 6 stage 1's 0x20 /
// 0x40 / 0x10, rows 1 and 2 stage 3's and 4's 0x20 / 0x40.
constexpr unsigned kCastText = 0;

constexpr unsigned kNameCount = 8, kNameCountLatin = 12;   // the PC's count; the US name field

char g_lines[kLineCount][kLineRoom];
char g_tabs[kTabCount][kTabRoom];
bool g_on = false;
// PatchBytes "FishingLabelSigma" (draft D), set by FishingText_Arm.
unsigned char g_label_sigma = 0;

unsigned EntryOf(std::uint16_t bit) {
    for (unsigned e = 0; e < kEntries; ++e)
        if (kEntryBit[e] == bit) return e;
    bof3::Fatal("fishing label: button 0x%04X is none of the six (circle, cross, triangle, square, L1, R1)",
                (unsigned)bit);
}

// The first of the six in a Config word, in the Config cell's order (L2 / R2,
// which word 7 carries beside Speak's button, have no label).
std::uint16_t FirstButton(std::uint16_t word) {
    for (unsigned e = 0; e < kEntries; ++e)
        if (word & kEntryBit[e]) return kEntryBit[e];
    bof3::Fatal("fishing label: word 7 (Field_ConfirmButtons 0x90358E) is 0x%04X, on none of the six buttons - "
                "a hand-edited save; hold Start while it loads to reset the buttons, or BOF3X_ORIGINAL=FishingLabelSigma",
                (unsigned)word);
}

// The label of text `text` with line byte `line` (0..2) through a sigma
// (`sigma`: Japan's bit -> the bit now) and word 7.
template <typename SigmaFn>
FishingLabel Resolve(unsigned text, unsigned line, SigmaFn sigma, std::uint16_t word7) {
    if (line >= 3) bof3::Fatal("fishing label: line byte %u, past the three buttons", line);
    const std::uint16_t bit = text == kCastText ? FirstButton(word7) : sigma(kEntryBit[line]);
    const unsigned e = EntryOf(bit);
    return {e, kEntryPen[e]};
}

std::uint8_t Byte(std::uint32_t at) { return *reinterpret_cast<const std::uint8_t*>(static_cast<std::uintptr_t>(at)); }
std::uint16_t Half(std::uint32_t at) { return *reinterpret_cast<const std::uint16_t*>(static_cast<std::uintptr_t>(at)); }

std::uint32_t Addr(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
std::uint32_t* Entry(std::uint32_t at) { return reinterpret_cast<std::uint32_t*>(static_cast<std::uintptr_t>(at)); }

// Copies `count` strings into `buffers` (each `room` long) and re-aims the
// pointer at `at + stride * i` from `shipped[i]` (or our buffer) at buffer i.
void Repoint(const char* what, const std::uint8_t* payload, std::uint32_t size, std::uint32_t count,
             char* buffers, std::uint32_t room, std::uint32_t at, std::uint32_t stride, const std::uint32_t* shipped) {
    const std::uint8_t* p = payload;
    const std::uint8_t* const end = payload + size;
    if (p >= end || *p++ != count) bof3::Fatal("fishing %s chunk: count is not %u", what, (unsigned)count);
    std::uint32_t written = 0;
    for (std::uint32_t i = 0; i < count; ++i) {
        const auto* s = reinterpret_cast<const char*>(p);
        while (p < end && *p) ++p;
        if (p >= end) bof3::Fatal("fishing %s chunk: ran out inside string %u", what, (unsigned)i);
        ++p;
        const std::size_t len = std::strlen(s);
        if (len + 1 > room)
            bof3::Fatal("fishing %s chunk: string %u is %u bytes, the buffer holds %u", what, (unsigned)i,
                        (unsigned)len + 1, (unsigned)room);
        char* const buffer = buffers + i * room;
        std::uint32_t* const entry = Entry(at + stride * i);
        // The entry names the shipped string, or our buffer when the title's
        // FIRST.DAT is loaded a second time (after a game over, DIV-0064's
        // lesson of 2026-09-30).
        if (*entry != shipped[i] && *entry != Addr(buffer))
            bof3::Fatal("fishing %s: 0x%08X holds 0x%08X, expected 0x%08X", what, (unsigned)(at + stride * i),
                        (unsigned)*entry, (unsigned)shipped[i]);
        if (len == 0) continue;   // the builder had nothing for it: as shipped
        std::memset(buffer, 0, room);
        std::memcpy(buffer, s, len);
        *entry = Addr(buffer);
        ++written;
    }
    if (p != end) bof3::Fatal("fishing %s chunk: %u bytes left over", what, (unsigned)(end - p));
    bof3::Log("DIV-0069: %u of %u fishing %s", (unsigned)written, (unsigned)count, what);
}


// BOF3X_SHADOW=fishing_text: the label through stand-in Config words. Pure
// but for the image's records and pens; a Fatal on any mismatch.
void SelfTest() {
    namespace sg = bof3x::input::sigma;
    unsigned cases = 0, bad = 0;
    auto check = [&](bool ok, const char* what, unsigned a, unsigned b) {
        ++cases;
        if (ok) return;
        ++bad;
        bof3::Log("shadow      fishing_text MISMATCH %s (%u, %u)", what, a, b);
    };
    // The image: 0x653C00's pens are the Config cell's for circle, cross,
    // triangle, and text 0 (the cast's) is labelled circle.
    for (unsigned line = 0; line < 3; ++line)
        check(Byte(kLinePens + line) == kEntryPen[line], "0x653C00's pen", line, Byte(kLinePens + line));
    unsigned char label[kLineCount];
    unsigned labelled = 0;
    for (unsigned i = 0; i < kLineCount; ++i) {
        label[i] = Byte(kLineRecords + 8 * i + 4);
        check(label[i] < 3 || label[i] == 0xFF, "a line byte", i, label[i]);
        if (label[i] != 0xFF) ++labelled;
    }
    check(label[kCastText] == 0, "the cast's line byte", kCastText, label[kCastText]);
    // One Config: every labelled text against `want` (entry by line byte 0, 1,
    // 2 and the cast's), when given, and always against the rule: the
    // non-cast lines Sigma(Japan's bit), the cast's word 7's button, which is
    // also Sigma(circle) once word 7 is Speak's word; the pen by the shape.
    auto config = [&](const std::uint16_t words[9], const unsigned* want, const char* what) {
        std::string why;
        const sg::Table t = sg::Build(words, why);
        check(t.valid, what, 0, 0);
        if (!t.valid) return;
        auto sigma = [&](std::uint16_t b) { return sg::Apply(t, b); };
        for (unsigned i = 0; i < kLineCount; ++i) {
            if (label[i] == 0xFF) continue;
            const FishingLabel l = Resolve(i, label[i], sigma, words[7]);
            const std::uint16_t rule = sg::Apply(t, kEntryBit[label[i]]);
            check(kEntryBit[l.entry] == rule, what, i, l.entry);
            check(l.pen == kEntryPen[l.entry], what, i, l.pen);
            if (want) check(l.entry == want[i == kCastText ? 3 : label[i]], what, i, l.entry);
        }
    };
    // Each release's defaults. Japan's: the original's draw, entry = the line
    // byte, pen = 0x653C00's. The West's (P) and PSP-EU's (S) by name.
    static const unsigned kJapan[4] = {0, 1, 2, 0};   // circle, cross, triangle; the cast circle
    static const unsigned kWest[4] = {1, 2, 3, 1};    // cross, triangle, square; the cast cross
    static const unsigned kPspEu[4] = {1, 0, 3, 1};   // cross, circle, square; the cast cross
    std::uint16_t words[9];
    sg::DefaultWords(sg::Release::kJapan, false, words);
    config(words, kJapan, "Japan's defaults");
    for (unsigned i = 0; i < kLineCount; ++i)
        if (label[i] != 0xFF) {
            const FishingLabel l = Resolve(i, label[i], [](std::uint16_t b) { return b; }, words[7]);
            check(l.entry == label[i] && l.pen == Byte(kLinePens + label[i]), "the identity is the original's", i,
                  l.entry);
            check(std::memcmp(kLabelCodes[l.entry], kLabelCodes[label[i]], 3) == 0, "the identity's icon", i, l.entry);
        }
    sg::DefaultWords(sg::Release::kWest, false, words);
    config(words, kWest, "the West's defaults");
    sg::DefaultWords(sg::Release::kPspEu, false, words);
    config(words, kPspEu, "PSP-EU's defaults");
    // The round's stand-in swap from Japan's: Speak <-> Action, View <->
    // Change, then words 7 / 8 as Speak's / Action's (draft A).
    static const unsigned kSwap[4] = {1, 0, 2, 1};    // cross, circle, triangle; the cast cross
    sg::DefaultWords(sg::Release::kJapan, false, words);
    words[0] = static_cast<std::uint16_t>(0x40 | sg::kL2R2);
    words[6] = 0x20;
    words[3] = 0x04;
    words[4] = 0x08;
    words[7] = words[0];
    words[8] = words[6];
    config(words, kSwap, "Speak <-> Action, View <-> Change");
    // Every Config the rebind can make: the six lines over the six bits.
    std::uint16_t perm[sg::kLines] = {0x04, 0x08, 0x10, 0x20, 0x40, 0x80};
    unsigned configs = 0;
    do {
        sg::DefaultWords(sg::Release::kJapan, false, words);
        for (int k = 0; k < sg::kLines; ++k) {
            const unsigned w = sg::kLineWord[k];
            words[w] = static_cast<std::uint16_t>(perm[k] | (w == 0 ? sg::kL2R2 : 0));
        }
        words[7] = words[0];
        words[8] = words[6];
        config(words, nullptr, "an assignment");
        ++configs;
    } while (std::next_permutation(perm, perm + sg::kLines));
    check(configs == 720, "the assignments", configs, 720);
    bof3::Log("shadow      fishing_text self-test: the label through sigma over %u labelled lines, each release's "
              "defaults, the stand-in swap and %u assignments (%u checks); %u MISMATCHES",
              labelled, configs, cases, bad);
    if (bad) bof3::Fatal("fishing_text: %u mismatches in the label through sigma (docs/fishing-text.md 8)", bad);
}

}  // namespace

void FishingText_Apply(std::uint32_t tag, const std::uint8_t* payload, std::uint32_t size) {
    if (tag == 1)
        Repoint("lines", payload, size, kLineCount, &g_lines[0][0], kLineRoom, kLineRecords, 8, kLineShipped);
    else if (tag == 2)
        Repoint("tabs", payload, size, kTabCount, &g_tabs[0][0], kTabRoom, kTabTable, 4, kTabShipped);
    else
        bof3::Fatal("fishing chunk tag is 0x%X: 1 the lines, 2 the tabs", (unsigned)tag);
}

void FishingText_Arm() {
    g_on = Lang_Latin();
    if (g_on) bof3::Log("DIV-0069: the fishing text's Latin layout armed");
    // Draft D: after sigma::Arm, which InjectAll calls first.
    if (bof3::WantsShadow("fishing_text")) SelfTest();
    static const std::uint8_t was = 0, is = 1;
    bof3::PatchBytes("FishingLabelSigma", static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&g_label_sigma)),
                     &was, &is, 1);
    bof3::Log("FishingLabelSigma the fishing banner's labels through sigma: %s",
              g_label_sigma ? "on" : "OFF (BOF3X_ORIGINAL=FishingLabelSigma)");
}

bool FishingText_On() { return g_on; }

unsigned FishingText_Width(const unsigned char* text, unsigned chars) {
    unsigned width = 0;
    for (unsigned n = 0; n < chars && *text; ++n) {
        width += static_cast<unsigned>(TextAdvance_Of(text));
        text += (*text & 0x80) ? 2 : 1;
    }
    return width;
}

unsigned FishingText_HalfAdvance(const unsigned char* text) {
    const unsigned half = static_cast<unsigned>(TextAdvance_Of(text)) / 2;
    return half != 0 ? half : 1;
}

const unsigned char* FishingText_Label(unsigned entry) {
    if (entry >= kEntries) bof3::Fatal("FishingText_Label: entry %u, past the six buttons", entry);
    return kLabelCodes[entry];
}

FishingLabel FishingText_LabelOf(unsigned text, unsigned line) {
    if (g_label_sigma == 0) return {line, Byte(kLinePens + line)};   // the original's reads
    return Resolve(text, line, [](std::uint16_t b) { return bof3x::input::sigma::Sigma(b); }, Half(kConfirmWord));
}

unsigned FishingText_NameCount() { return g_on ? kNameCountLatin : kNameCount; }

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
#include "game/fishing_text.h"

#include <cstring>

#include "game/lang_layout.h"
#include "game/text_advance.h"
#include "hook/log.h"

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
// cells) and its blank - circle, cross, triangle first, as config_text.cpp's
// kIconCircle .. kIconTriangle.
constexpr std::uint32_t kIconCircle = 0xA00 + 100 + 1;
constexpr unsigned char kLabelCodes[3][3] = {
    {static_cast<unsigned char>(0x80 | (kIconCircle >> 8)), static_cast<unsigned char>(kIconCircle), 0},
    {static_cast<unsigned char>(0x80 | ((kIconCircle + 1) >> 8)), static_cast<unsigned char>(kIconCircle + 1), 0},
    {static_cast<unsigned char>(0x80 | ((kIconCircle + 2) >> 8)), static_cast<unsigned char>(kIconCircle + 2), 0},
};

constexpr unsigned kNameCount = 8, kNameCountLatin = 12;   // the PC's count; the US name field

char g_lines[kLineCount][kLineRoom];
char g_tabs[kTabCount][kTabRoom];
bool g_on = false;

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

const unsigned char* FishingText_Label(unsigned line) {
    if (line >= 3) bof3::Fatal("FishingText_Label: label byte %u, past the three buttons", line);
    return kLabelCodes[line];
}

unsigned FishingText_NameCount() { return g_on ? kNameCountLatin : kNameCount; }

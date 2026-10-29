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
//      same, three pointers re-aimed;
//   3  (D286, 2026-10-18, docs/install-gap-labels.md) the four panel headings
//      ROD, LURE, EQUIP, GUIDE - fixed fields at 0x653EA0 / A4 / A8 / AD that
//      EffectKind0F_DrawItemsB / _DrawItemsA / _DrawEquipped (ours) draw with
//      the counts 3, 4, 5, 5. The French and German modules lay their words
//      out at other widths (Canne, the German RUTE ...), so a disc-built
//      image has no bytes there; the builder reads each word off the
//      module's draws and our draws, once armed, centre it where the PC's
//      centre is. Kept in buffers of ours; nothing in .data is re-aimed.
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
// DIV-0136 (the button round's IC-F, amends DIV-0069; docs/fishing-text.md
// 8), armed after sigma::Arm with or without an overlay: a label names the
// button its line's test is on now - Sigma of the line byte's button, word 7's
// for the cast's line (text 0) - drawn as that button's DIV-0051 icon under a
// Latin overlay, else as the port's own stand-in for it (0x66A2FC's entry), in
// that button's pen. At the identity Config every label is the original's.
//
// D313 (2026-10-17, docs/fishing-text.md 8.8): a disc-built image holds the
// line bytes and the pens its disc's fishing module has (tools/psp_exe.py
// fishrec / fishpen). The line byte is the same on every release; the label
// entry it indexes is the release's own button for the line, and the pen that
// button's. So a label is read in the terms of the release the image was
// built from: its button is M_R of the line byte's Japanese one, and through
// sigma it is SigmaFrom(R, that).
#include "game/fishing_text.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>
#include "common/image_base.h"

#include "game/lang_layout.h"
#include "game/text_advance.h"
#include "hook/detour.h"
#include "hook/exe_image.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace {

constexpr ::bof3::image::Va kLineRecords{0x653B98};   // 13 records of 8: pointer, label byte, pause byte
constexpr std::uint32_t kLineCount = 13;
constexpr std::uint32_t kLineRoom = 64;            // tools/loc_build.py FISH_LINE_ROOM
// The records' shipped pointers (read from the image 2026-10-03; 1 and 12
// share one string).
constexpr std::uint32_t kLineShipped[kLineCount] = {0x669FC0, 0x669FC8, 0x669FDC, 0x669FE8, 0x669FF0,
                                                    0x66A004, 0x66A010, 0x66A020, 0x66A030, 0x66A040,
                                                    0x66A058, 0x66A06C, 0x669FC8};
constexpr ::bof3::image::Va kTabTable{0x66A088};      // 3 pointers, read by 0x468AC0 only
constexpr std::uint32_t kTabCount = 3;
constexpr std::uint32_t kTabRoom = 16;             // tools/loc_build.py FISH_TAB_ROOM
constexpr std::uint32_t kTabShipped[kTabCount] = {0x66A070, 0x66A078, 0x66A080};
constexpr std::uint32_t kHeadCount = 4;            // tools/loc_build.py FISH_HEAD_COUNT
constexpr std::uint32_t kHeadRoom = 16;            // tools/loc_build.py FISH_HEAD_ROOM

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

// DIV-0136: the label through sigma. The six entries of 0x66A2FC in the
// order the Config cell 0x461C00 reads them (its loads 0x66A2FC..0x66A310 under
// its tests of 0x20, 0x40, 0x10, 0x80, 0x04, 0x08), and each button's pen as
// that cell pushes it (2, 1, 6, 5, 0, 0; read 2026-10-17). The first three are
// 0x653C00's bytes, which the self-test checks against the image.
constexpr std::uint16_t kEntryBit[kEntries] = {0x20, 0x40, 0x10, 0x80, 0x04, 0x08};
constexpr unsigned kEntryPen[kEntries] = {2, 1, 6, 5, 0, 0};
constexpr ::bof3::image::Va kLinePens{0x653C00};      // bytes by a line byte: 2, 1, 6
constexpr ::bof3::image::Va kConfirmWord{0x90358E};   // Field_ConfirmButtons, word 7, u16
// Text 0 is the cast's line: row 0 (0x653C04, the stage-1 row with both items
// set) column 0, line byte 0. Stage 1's 0x20 (LeaderPanel_S1Buttons 0x528A90)
// leads to stage 2, whose LeaderPanel_S2Wait 0x528FA0 casts on word 7. Every
// other labelled text names a constant test: rows 0 and 6 stage 1's 0x20 /
// 0x40 / 0x10, rows 1 and 2 stage 3's and 4's 0x20 / 0x40.
constexpr unsigned kCastText = 0;

constexpr unsigned kNameCount = 8, kNameCountLatin = 12;   // the PC's count; the US name field

namespace sg = bof3x::input::sigma;

char g_lines[kLineCount][kLineRoom];
char g_tabs[kTabCount][kTabRoom];
char g_heads[kHeadCount][kHeadRoom];
// D279 (2026-10-18): tag 4, the disc's own three button labels (a Japanese
// disc's; tools/loc_build.py convert_fishing_ja), one character each by the
// label byte's entry (circle, cross, triangle), read by Label() in place of
// 0x66A2FC's string once armed (FishingText_Arm, so effect_1a's fuzz compares
// Capcom's reads).
constexpr unsigned kDiscLabelCount = 3;            // tools/loc_build.py FISH_LABEL_COUNT
constexpr unsigned kDiscLabelRoom = 8;             // tools/loc_build.py FISH_LABEL_ROOM
char g_disc_labels[kDiscLabelCount][kDiscLabelRoom];
bool g_disc_labels_armed = false;
bool g_on = false;
// PatchBytes "FishingLabelSigma" (DIV-0136), set by FishingText_Arm.
unsigned char g_label_sigma = 0;
// The release whose terms the running image's line pens are in
// (FishingText_Arm): the build of the disc-built image the exe host mapped;
// Japan's for BOF3.exe's own .data (hosted, or a pc-zh image, which is
// byte-identical to it).
sg::Release g_image_release = sg::Release::kJapan;
std::string g_image_how = "before FishingText_Arm";

// The running .data's build: ExeImage_MappedBuild (BOF3X_EXEIMAGE's data.toml)
// or, with none mapped, the file's. A build whose release is not known: a
// Fatal (the terms cannot be guessed).
sg::Release ImageRelease(std::string& how) {
    const char* const build = bof3::ExeImage_MappedBuild();
    if (build == nullptr) {
        how = "the running .data is BOF3.exe's";
        return sg::Release::kJapan;
    }
    std::string error;
    const sg::Release r = sg::ReleaseOfSources(std::vector<std::string>{build}, error);
    if (!error.empty()) bof3::Fatal("fishing labels: the mapped image's build: %s", error.c_str());
    how = std::string("the running .data is the ") + build + "-built image";
    return r;
}

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

// The label of text `text` with line byte `line` (0..2) on an image of
// release `r`, through a sigma (`sigma_from`: release r's bit -> the bit now,
// SigmaFrom(r, ..)) and word 7. `pens` is what the image holds at 0x653C00 by
// line byte. The line byte names the line's label entry, the same byte on
// every release; the entry is the release's own button for the line, M_R of
// Japan's (0 circle, 1 cross, 2 triangle): on a Western disc cross, triangle,
// square, on PSP-EU cross, circle, square (each disc's entries, 2026-10-17).
// The owner's rule of 2026-10-17: a label whose button sigma leaves where the
// release had it draws exactly as that release's own draw reads it, that
// button's entry in the image's pen - on the PC 2, 1, 6, from a disc its
// module's pens. Only a button that moved takes the drawn shape's own pen.
template <typename SigmaFromFn>
FishingLabel Resolve(unsigned text, unsigned line, sg::Release r, SigmaFromFn sigma_from, std::uint16_t word7,
                     const std::uint8_t pens[3]) {
    if (line >= 3) bof3::Fatal("fishing label: line byte %u, past the three buttons", line);
    const std::uint16_t own = sg::ReleaseMap(r, kEntryBit[line]);
    const std::uint16_t bit = text == kCastText ? FirstButton(word7) : sigma_from(own);
    if (bit == own) return {EntryOf(own), pens[line]};
    const unsigned e = EntryOf(bit);
    return {e, kEntryPen[e]};
}

// The pen release r's own label entry for line byte `line` has: its button's
// shape's pen. Every disc measured holds exactly this (fishing-text.md 8.8),
// so the self-test checks the image against it.
unsigned OwnPen(sg::Release r, unsigned line) { return kEntryPen[EntryOf(sg::ReleaseMap(r, kEntryBit[line]))]; }

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
        if (*entry != ::bof3::Img(shipped[i]) && *entry != Addr(buffer))   // a pointer word: the place
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
    unsigned cases = 0, bad = 0;
    auto check = [&](bool ok, const char* what, unsigned a, unsigned b) {
        ++cases;
        if (ok) return;
        ++bad;
        bof3::Log("shadow      fishing_text MISMATCH %s (%u, %u)", what, a, b);
    };
    // Four images: this one, in its release's terms (g_image_release), and
    // one of each release's as tools/psp_exe.py's fishrec / fishpen write it
    // from that release's disc (D313; docs/fishing-text.md 8.8): this image's
    // line bytes, which every disc holds as the PC does, and each line pen
    // the pen of the release's own button for it (Japan's 2, 1, 6, the
    // West's 1, 6, 5, PSP-EU's 1, 2, 5 on the discs).
    struct Image {
        const char* name;
        sg::Release release;
        unsigned char label[kLineCount];
        std::uint8_t pens[3];
    };
    Image images[4] = {{"this image", g_image_release, {}, {}},
                       {"a Japanese disc's image", sg::Release::kJapan, {}, {}},
                       {"a Western disc's image", sg::Release::kWest, {}, {}},
                       {"PSP-EU's image", sg::Release::kPspEu, {}, {}}};
    for (unsigned i = 0; i < kLineCount; ++i) {
        const unsigned char line = Byte(::bof3::Img(kLineRecords.v) + 8 * i + 4);
        const unsigned char pause = Byte(::bof3::Img(kLineRecords.v) + 8 * i + 5);
        for (Image& im : images) im.label[i] = line;
        check(line < 3 || line == 0xFF, "a line byte", i, line);
        // D313: a pause of 0 is what a disc-built image without the records
        // held - LineNext 0x466460 takes it to 255 before it tests it, so the
        // line waited 256 frames for the next.
        check(pause != 0, "a line's pause byte is not 0 (D313: an image without the disc's records)", i, pause);
    }
    for (unsigned line = 0; line < 3; ++line) {
        images[0].pens[line] = Byte(::bof3::Img(kLinePens.v) + line);
        for (unsigned k = 1; k < 4; ++k) images[k].pens[line] = static_cast<std::uint8_t>(OwnPen(images[k].release, line));
        // This image's pens are its release's own buttons' (D313: a disc
        // image without the module's pens held 0, white).
        check(images[0].pens[line] == OwnPen(g_image_release, line), "this image's pen in its release's terms", line,
              images[0].pens[line]);
    }
    // Text 0 (the cast's) is the first entry's.
    check(images[0].label[kCastText] == 0, "the cast's line byte", kCastText, images[0].label[kCastText]);
    unsigned labelled = 0;
    for (unsigned i = 0; i < kLineCount; ++i)
        if (images[0].label[i] != 0xFF) ++labelled;
    // One Config on one image: every labelled text against `want` (the entry
    // by line byte 0, 1, 2 and the cast's), when given, and always against
    // the rule: the button is SigmaFrom(R, the release's button), which is
    // Sigma(Japan's), or word 7's for the cast; a button left where the
    // release had it draws as that release does (its entry, the image's pen),
    // a moved one takes its shape's pen. On an image whose pens are its
    // release's, every label is in its own shape's pen: no circle in cross's
    // blue.
    auto config = [&](const Image& im, const std::uint16_t words[9], const unsigned* want, const char* what) {
        std::string why;
        const sg::Table t = sg::Build(words, why);
        check(t.valid, what, 0, 0);
        if (!t.valid) return;
        auto sigma_from = [&](std::uint16_t b) { return sg::Apply(t, sg::ReleaseMapInverse(im.release, b)); };
        const bool own_pens = OwnPen(im.release, 0) == im.pens[0] && OwnPen(im.release, 1) == im.pens[1] &&
                              OwnPen(im.release, 2) == im.pens[2];
        for (unsigned i = 0; i < kLineCount; ++i) {
            const unsigned line = im.label[i];
            if (line == 0xFF) continue;
            const FishingLabel l = Resolve(i, line, im.release, sigma_from, words[7], im.pens);
            const std::uint16_t own = sg::ReleaseMap(im.release, kEntryBit[line]);
            const std::uint16_t rule = i == kCastText ? FirstButton(words[7]) : sg::Apply(t, kEntryBit[line]);
            check(kEntryBit[l.entry] == rule, what, i, l.entry);
            if (i != kCastText) check(sigma_from(own) == rule, what, i, rule);
            if (rule == own)
                check(l.entry == EntryOf(own) && l.pen == im.pens[line] &&
                          std::memcmp(kLabelCodes[l.entry], kLabelCodes[EntryOf(own)], 3) == 0,
                      what, i, l.pen);
            else
                check(l.pen == kEntryPen[l.entry], what, i, l.pen);
            if (own_pens) check(l.pen == kEntryPen[l.entry], what, i, l.pen);
            if (want) check(l.entry == want[i == kCastText ? 3 : line], what, i, l.entry);
        }
    };
    // By name: entries for line bytes 0, 1, 2 and the cast's - the same on
    // every image, for the button is the Config's; only the pen of an unmoved
    // one is the image's.
    static const unsigned kJapan[4] = {0, 1, 2, 0};   // circle, cross, triangle; the cast circle
    static const unsigned kWest[4] = {1, 2, 3, 1};    // cross, triangle, square; the cast cross
    static const unsigned kPspEu[4] = {1, 0, 3, 1};   // cross, circle, square; the cast cross
    static const unsigned kSwap[4] = {1, 0, 2, 1};    // cross, circle, triangle; the cast cross
    unsigned configs = 0;
    for (const Image& im : images) {
        std::uint16_t words[9];
        // Japan's words (the PC's start; and every route's save, the camping
        // save, on every install kind). On Japan's image sigma is the
        // identity there and every label is the original's; on a Western or
        // PSP-EU image the labels name Japan's buttons, moved from the
        // release's, in their shapes' pens.
        sg::DefaultWords(sg::Release::kJapan, false, words);
        config(im, words, kJapan, "Japan's words");
        // A disc install's own New Game words (DIV-0134): at its release's
        // defaults every label is that release's own draw - its button, the
        // image's pen - which is the disc's (the US module's LineNext reads
        // its entry 0x801E1F14 + 3 * line and the pen at + 2).
        sg::DefaultWords(im.release, false, words);
        for (unsigned i = 0; i < kLineCount; ++i)
            if (im.label[i] != 0xFF) {
                const unsigned own = EntryOf(sg::ReleaseMap(im.release, kEntryBit[im.label[i]]));
                const FishingLabel l =
                    Resolve(i, im.label[i], im.release, [](std::uint16_t b) { return b; }, words[7], im.pens);
                check(l.entry == own && l.pen == im.pens[im.label[i]], "the release's defaults are its own draw", i,
                      l.pen);
            }
        sg::DefaultWords(sg::Release::kWest, false, words);
        config(im, words, kWest, "the West's defaults");
        sg::DefaultWords(sg::Release::kPspEu, false, words);
        config(im, words, kPspEu, "PSP-EU's defaults");
        // The round's stand-in swap from Japan's: Speak <-> Action, View <->
        // Change, words 7 / 8 as Speak's / Action's (DIV-0133). On Japan's
        // image circle and cross move (their shapes' pens 1 and 2) and
        // triangle stays, as drawn.
        sg::DefaultWords(sg::Release::kJapan, false, words);
        words[0] = static_cast<std::uint16_t>(0x40 | sg::kL2R2);
        words[6] = 0x20;
        words[3] = 0x04;
        words[4] = 0x08;
        words[7] = words[0];
        words[8] = words[6];
        config(im, words, kSwap, "Speak <-> Action, View <-> Change");
        // Every Config the rebind can make: the six lines over the six bits.
        std::uint16_t perm[sg::kLines] = {0x04, 0x08, 0x10, 0x20, 0x40, 0x80};
        do {
            sg::DefaultWords(sg::Release::kJapan, false, words);
            for (int k = 0; k < sg::kLines; ++k) {
                const unsigned w = sg::kLineWord[k];
                words[w] = static_cast<std::uint16_t>(perm[k] | (w == 0 ? sg::kL2R2 : 0));
            }
            words[7] = words[0];
            words[8] = words[6];
            config(im, words, nullptr, "an assignment");
            ++configs;
        } while (std::next_permutation(perm, perm + sg::kLines));
    }
    check(configs == 2880, "the assignments", configs, 2880);
    bof3::Log("shadow      fishing_text self-test: the label through sigma over %u labelled lines on four images "
              "(this one, in %s terms, pens %u %u %u; Japan's, the West's and PSP-EU's by their own pens), Japan's "
              "words, each release's defaults as its own draw, the stand-in swap and %u assignments (%u checks); "
              "%u MISMATCHES",
              labelled, sg::ReleaseName(g_image_release), images[0].pens[0], images[0].pens[1], images[0].pens[2],
              configs, cases, bad);
}

}  // namespace

// Tag 3: four NUL-ended strings into g_heads (an empty one keeps the shipped
// field). No pointer in the image names the buffers, so nothing is re-aimed.
void ApplyHeadings(const std::uint8_t* payload, std::uint32_t size) {
    const std::uint8_t* p = payload;
    const std::uint8_t* const end = payload + size;
    if (p >= end || *p++ != kHeadCount) bof3::Fatal("fishing headings chunk: count is not %u", (unsigned)kHeadCount);
    std::uint32_t written = 0;
    for (std::uint32_t i = 0; i < kHeadCount; ++i) {
        const auto* s = reinterpret_cast<const char*>(p);
        while (p < end && *p) ++p;
        if (p >= end) bof3::Fatal("fishing headings chunk: ran out inside string %u", (unsigned)i);
        ++p;
        const std::size_t len = std::strlen(s);
        if (len + 1 > kHeadRoom)
            bof3::Fatal("fishing headings chunk: string %u is %u bytes, the buffer holds %u", (unsigned)i,
                        (unsigned)len + 1, (unsigned)kHeadRoom);
        std::memset(g_heads[i], 0, kHeadRoom);
        std::memcpy(g_heads[i], s, len);
        if (len != 0) ++written;
    }
    if (p != end) bof3::Fatal("fishing headings chunk: %u bytes left over", (unsigned)(end - p));
    bof3::Log("DIV-0069: %u of %u fishing headings (D286)", (unsigned)written, (unsigned)kHeadCount);
}

// Tag 4 (D279): three NUL-ended one-character strings into g_disc_labels (an
// empty one keeps 0x66A2FC's string). Nothing in the image is re-aimed: the
// Config cell 0x461C00 reads the same table and is D280's, not this.
void ApplyDiscLabels(const std::uint8_t* payload, std::uint32_t size) {
    const std::uint8_t* p = payload;
    const std::uint8_t* const end = payload + size;
    if (p >= end || *p++ != kDiscLabelCount)
        bof3::Fatal("fishing labels chunk: count is not %u", (unsigned)kDiscLabelCount);
    std::uint32_t written = 0;
    for (std::uint32_t i = 0; i < kDiscLabelCount; ++i) {
        const auto* s = reinterpret_cast<const char*>(p);
        while (p < end && *p) ++p;
        if (p >= end) bof3::Fatal("fishing labels chunk: ran out inside string %u", (unsigned)i);
        ++p;
        const std::size_t len = std::strlen(s);
        if (len + 1 > kDiscLabelRoom)
            bof3::Fatal("fishing labels chunk: string %u is %u bytes, the buffer holds %u", (unsigned)i,
                        (unsigned)len + 1, (unsigned)kDiscLabelRoom);
        const std::size_t first = (static_cast<unsigned char>(s[0]) & 0x80) != 0 ? 2 : 1;
        if (len != 0 && len != first)
            bof3::Fatal("fishing labels chunk: string %u is %u bytes, not one character", (unsigned)i, (unsigned)len);
        std::memset(g_disc_labels[i], 0, kDiscLabelRoom);
        std::memcpy(g_disc_labels[i], s, len);
        if (len != 0) ++written;
    }
    if (p != end) bof3::Fatal("fishing labels chunk: %u bytes left over", (unsigned)(end - p));
    bof3::Log("DIV-0069: %u of %u fishing labels (D279)", (unsigned)written, (unsigned)kDiscLabelCount);
}

void FishingText_Apply(std::uint32_t tag, const std::uint8_t* payload, std::uint32_t size) {
    if (tag == 1)
        Repoint("lines", payload, size, kLineCount, &g_lines[0][0], kLineRoom, ::bof3::Img(kLineRecords.v), 8, kLineShipped);
    else if (tag == 2)
        Repoint("tabs", payload, size, kTabCount, &g_tabs[0][0], kTabRoom, ::bof3::Img(kTabTable.v), 4, kTabShipped);
    else if (tag == 3)
        ApplyHeadings(payload, size);
    else if (tag == 4)
        ApplyDiscLabels(payload, size);
    else
        bof3::Fatal("fishing chunk tag is 0x%X: 1 the lines, 2 the tabs, 3 the headings, 4 the labels", (unsigned)tag);
}

const unsigned char* FishingText_DiscLabel(unsigned entry) {
    if (entry >= kEntries) bof3::Fatal("FishingText_DiscLabel: entry %u, past the six buttons", entry);
    if (!g_disc_labels_armed || entry >= kDiscLabelCount || g_disc_labels[entry][0] == 0) return nullptr;
    return reinterpret_cast<const unsigned char*>(g_disc_labels[entry]);
}

const unsigned char* FishingText_Heading(unsigned i) {
    if (i >= kHeadCount) bof3::Fatal("FishingText_Heading: heading %u, past the four", i);
    if (!g_on || g_heads[i][0] == 0) return nullptr;
    return reinterpret_cast<const unsigned char*>(g_heads[i]);
}

void FishingText_Arm() {
    g_on = Lang_Latin();
    if (g_on) bof3::Log("DIV-0069: the fishing text's Latin layout armed");
    // D279: the disc's labels (tag 4, a Japanese layer's) from here on.
    g_disc_labels_armed = true;
    // D313: the terms of the image's line pens, before the self-test reads them.
    g_image_release = ImageRelease(g_image_how);
    // DIV-0136: after sigma::Arm, which InjectAll calls first.
    if (bof3::WantsShadow("fishing_text")) SelfTest();
    static const std::uint8_t was = 0, is = 1;
    bof3::PatchBytes("FishingLabelSigma", static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&g_label_sigma)),
                     &was, &is, 1);
    bof3::Log("FishingLabelSigma the fishing banner's labels through sigma: %s; labels in %s terms (%s)",
              g_label_sigma ? "on" : "OFF (BOF3X_ORIGINAL=FishingLabelSigma)", sg::ReleaseName(g_image_release),
              g_image_how.c_str());
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
    if (g_label_sigma == 0) return {line, Byte(::bof3::Img(kLinePens.v) + line)};   // the original's reads
    const std::uint8_t pens[3] = {Byte(::bof3::Img(kLinePens.v)), Byte(::bof3::Img(kLinePens.v) + 1), Byte(::bof3::Img(kLinePens.v) + 2)};
    const sg::Release r = g_image_release;
    return Resolve(text, line, r, [r](std::uint16_t b) { return sg::SigmaFrom(r, b); }, Half(::bof3::Img(kConfirmWord.v)), pens);
}

unsigned FishingText_NameCount() { return g_on ? kNameCountLatin : kNameCount; }

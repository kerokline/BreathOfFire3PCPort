#include "game/text_draw.h"

#include <windows.h>

#include <cstdint>
#include <cstring>
#include <string>

#include "bof3/symbols.gen.h"
#include "game/button_text.h"
#include "game/lang_layout.h"
#include "game/msgbox.h"
#include "game/text_advance.h"
#include "game/text_pairs.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace {

// 0x516C94: cmp cx, 0xA00 / jbe. Not constant any more - DIV-0016, see
// text_draw.h. Starts at the original's value, so a run with no overlay
// never sees a different number.
constexpr unsigned kOriginalGlyphLimit = 0xA00;
unsigned g_glyph_limit = kOriginalGlyphLimit;
unsigned g_glyph_count = 0;   // the loaded table's glyphs, as TextDraw_SetGlyphCount was last told
constexpr int kCell = 12;
constexpr int kRecordBytes = 0x20;

// Text_EmitGlyph is not ours, and cannot run in a start-up test (it commits a
// primitive), so it is called through a pointer the fuzz can aim at a
// recorder - for the original's copy and for ours alike.
using EmitFn = void (__cdecl*)(int, int, int, int, int, int, int);
EmitFn g_emit = Text_EmitGlyph;

const std::uint8_t* ClipPairs() { return reinterpret_cast<const std::uint8_t*>(bof3::addr::Text_ClipPairs); }
const std::uint8_t* Records() { return reinterpret_cast<const std::uint8_t*>(bof3::addr::Text_Records); }

// The original's answer to a glyph index above 0xA00: `mov dx, 0x1000` /
// `in al, dx` (0x516C9B), a privileged instruction, which in user mode is a
// fault. Ours is the same instruction, so the crash reporter sees what it
// would have seen. Should the fault ever be resumed, the original goes on to
// draw the glyph; so does the caller of this.
void GlyphTrap() { __asm__ volatile("mov $0x1000, %%dx\n\tin %%dx, %%al" : : : "eax", "edx"); }

unsigned ClutOf(unsigned color) { return 0x7800u | (color & 0xFu); }

// --- DT-2 (DIV-0140): the dialogue's button mentions, live -----------------
// The decision is button_text.h's; this is what it reads from the running game.
// Armed by ButtonText_Arm after every module's self-test (so every fuzz of a
// text reader compares Capcom's draws), as its own switch: under
// BOF3X_ORIGINAL=ButtonText both readers draw every mention as its text has it.
std::uint8_t g_button_text = 0;
button_text::Context g_bt;
// The area text block: MessagePools, the pool every dialogue line and the
// fishing list are drawn from (Msg_OpenScript, EffectKind0F_DrawMessageList's
// Pool), at most 0x8000 bytes (tools/loc_build.py TEXT_ROOM: the CLUT strip
// as loaded starts there). A string outside it is never a mention.
constexpr std::uint32_t kPool = 0x803580, kPoolRoom = 0x8000;
// The areas whose dialogue names a button (docs/button-dialogue.md 3.1): the
// wood chop, the fishing help (three), the well, the rope pull, the antenna,
// the ram, the crane. Not area 1 (a place name, section 4.3), not area 78
// (a word of crosses), not the faerie village (a times sign).
constexpr unsigned short kButtonAreas[] = {8, 30, 68, 75, 89, 111, 121, 129, 135};
// 0x461C00's six strings, circle, cross, triangle, square, L1, R1: the
// Config cell's stand-ins, the set the colour cue's swap draws from.
constexpr std::uint32_t kStandIns = 0x66A2FC;

bool InPool(const std::uint8_t* p) {
    const auto a = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p));
    return a >= kPool && a < kPool + kPoolRoom;
}

bool InButtonArea() {
    const unsigned short area = Game_AreaNumber;
    for (unsigned short a : kButtonAreas)
        if (a == area) return true;
    return false;
}

std::uint16_t LiveSigma(std::uint16_t bit) { return bof3x::input::sigma::Sigma(bit); }

[[noreturn]] void Refuse(const char* why, const std::uint8_t* p) {
    bof3::Fatal("ButtonText (docs/button-dialogue.md 4): %s, at 0x%08X in area %u (bytes %02X %02X %02X %02X %02X %02X "
                "%02X); BOF3X_ORIGINAL=ButtonText draws the text as it is",
                why, (unsigned)reinterpret_cast<std::uintptr_t>(p), (unsigned)Game_AreaNumber, p[0], p[1], p[2], p[3],
                p[4], p[5], p[6]);
}

// --- BOF3X_TEXTLOG: which string is drawn from where ----------------------------
// Diagnostic only, off unless the variable is set: each distinct (caller,
// string address) pair is logged once, with the string's first bytes - the
// way to find where a piece of on-screen text lives: the string's address
// leads to its table by a search for pointers to it (docs/HANDOFF.md, the
// battle labels). The caller logged is our own return address, so anything
// drawn through Text_DrawAt shows 0x516B61. Reading Text_DrawAt's caller off
// the stack, three slots above our first argument, was tried on 2026-09-21
// and came back wrong as built; unfixed, so not claimed.
bool g_textlog = false;
constexpr unsigned kTextLogSlots = 4096;
std::uint64_t g_textlog_seen[kTextLogSlots];
unsigned g_textlog_count = 0;

void TextLog(std::uint32_t caller, const unsigned char* text) {
    const std::uint64_t key = (static_cast<std::uint64_t>(caller) << 32) | reinterpret_cast<std::uintptr_t>(text);
    unsigned h = static_cast<unsigned>((key * 0x9E3779B97F4A7C15ull) >> 52) % kTextLogSlots;
    for (unsigned probe = 0; probe < kTextLogSlots; ++probe, h = (h + 1) % kTextLogSlots) {
        if (g_textlog_seen[h] == key) return;
        if (g_textlog_seen[h] == 0) {
            if (g_textlog_count + 1 >= kTextLogSlots) return;   // full: stop logging, keep drawing
            g_textlog_seen[h] = key;
            ++g_textlog_count;
            char hex[3 * 16 + 1] = {};
            for (unsigned i = 0; i < 16 && text[i]; ++i) {
                static const char digits[] = "0123456789ABCDEF";
                hex[i * 3] = digits[text[i] >> 4];
                hex[i * 3 + 1] = digits[text[i] & 0xF];
                hex[i * 3 + 2] = ' ';
            }
            bof3::Log("textlog     caller 0x%08X text 0x%08X frame %u: %s", (unsigned)caller,
                      (unsigned)reinterpret_cast<std::uintptr_t>(text), (unsigned)Frame_Counter, hex);
            return;
        }
    }
}

// --- BOF3X_SHADOW=text_draw ----------------------------------------------------
// The clone: 0x516B70..0x516D50, body, jump table and byte table. The jump
// table holds absolute addresses, so the copy's seven entries are moved into
// the copy and the `jmp [edx*4 + table]` at +0x76 re-aimed at them; the byte
// table is only read and stays where it is. The recursive call at +0xF1 is
// relative and inside the range: the copy calls the copy. The call of
// Text_EmitGlyph at +0x166 goes to the recorder.
constexpr std::uint32_t kBody = 0x516B70, kSize = 0x1E0;
constexpr std::uint32_t kJmpDisp = 0x79, kJumpTable = 0x198, kJumpEntries = 7, kEmitCall = 0x166;

struct Emit {
    std::uint16_t x, y, clut, glyph;
    std::uint8_t w, h, u, v;
};
constexpr unsigned kLog = 512;
Emit g_log[kLog];
unsigned g_log_n;

void __cdecl RecordEmit(int x, int y, int w, int h, int u, int v, int clut) {
    std::uint16_t glyph;
    std::memcpy(&glyph, Gfx_PacketNext + 0x16, sizeof glyph);
    if (g_log_n < kLog)
        g_log[g_log_n] = {static_cast<std::uint16_t>(x), static_cast<std::uint16_t>(y),
                          static_cast<std::uint16_t>(clut), glyph,
                          static_cast<std::uint8_t>(w), static_cast<std::uint8_t>(h),
                          static_cast<std::uint8_t>(u), static_cast<std::uint8_t>(v)};
    ++g_log_n;
}

using DrawFn = const unsigned char* (__cdecl*)(unsigned, unsigned, const unsigned char*);

void SelfTest(void* clone) {
    auto* code = static_cast<std::uint8_t*>(clone);
    const std::uint32_t moved = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(code)) - kBody;
    for (std::uint32_t i = 0; i < kJumpEntries; ++i) {
        std::uint32_t target;
        std::memcpy(&target, code + kJumpTable + 4 * i, sizeof target);
        if (target < kBody || target >= kBody + kSize) bof3::Fatal("Text_DrawString: jump table entry %u is 0x%X", (unsigned)i, (unsigned)target);
        target += moved;
        std::memcpy(code + kJumpTable + 4 * i, &target, sizeof target);
    }
    std::uint32_t disp;
    std::memcpy(&disp, code + kJmpDisp, sizeof disp);
    if (disp != kBody + kJumpTable) bof3::Fatal("Text_DrawString: no jump table operand at +0x%X", (unsigned)kJmpDisp);
    disp += moved;
    std::memcpy(code + kJmpDisp, &disp, sizeof disp);
    const auto theirs = reinterpret_cast<DrawFn>(clone);

    // The state both sides touch: three pen words, the primitive under
    // Gfx_PacketNext (one word of it), and eight of the 0x07 records.
    constexpr int kRecords = 8;
    static std::uint8_t saved_records[kRecords * kRecordBytes], prim[0x40];
    auto* records = const_cast<std::uint8_t*>(Records());
    std::memcpy(saved_records, records, sizeof saved_records);
    const short saved_pen[3] = {Text_PenX, Text_PenY, Text_LineX};
    unsigned char* const saved_packet = Gfx_PacketNext;
    Gfx_PacketNext = prim;
    g_emit = &RecordEmit;

    std::uint32_t rng = 0x27D4EB2Fu;
    auto next = [&rng] { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; };
    // One character that does not trap: a control, a space, a single byte from
    // 0x26 up, or a two-byte code up to 0xA00 - the last seeded with the limit
    // itself and the bytes either side of each range.
    auto put_char = [&](std::uint8_t* out, bool allow_record) -> int {
        const unsigned r = next() % 16;
        if (r == 0) { out[0] = 0x01; return 1; }
        if (r == 1) { out[0] = 0x05; out[1] = static_cast<std::uint8_t>(next()); if (!out[1]) out[1] = 0x3A; return 2; }
        if (r == 2) { out[0] = 0x06; return 1; }
        if (r == 3 && allow_record) { out[0] = 0x07; out[1] = static_cast<std::uint8_t>(next() % kRecords); if (!out[1]) out[1] = 1; return 2; }
        if (r == 4) { out[0] = 0x20; return 1; }
        if (r < 8) {
            static const unsigned kSeeds[] = {0x000, 0x001, 0x0FF, 0x100, 0x992, 0x993, 0x9FF, 0xA00};
            unsigned g = (next() & 1) ? kSeeds[next() % 8] : next() % (g_glyph_limit + 1);
            // A zero second byte is left in: the glyph is drawn and the loop's
            // look-ahead then ends the string, the same on both sides. (Forcing
            // it non-zero once turned the seed 0xA00 into 0xA01 - the trap.)
            out[0] = static_cast<std::uint8_t>(0x80 | (g >> 8));
            out[1] = static_cast<std::uint8_t>(g);
            return 2;
        }
        static const std::uint8_t kEdges[] = {0x26, 0x27, 0x7E, 0x7F};
        out[0] = (next() & 3) == 0 ? kEdges[next() % 4] : static_cast<std::uint8_t>(0x26 + next() % (0x80 - 0x26));
        return 1;
    };

    unsigned bad = 0, emits = 0;
    const unsigned rounds = 6000;
    for (unsigned n = 0; n < rounds; ++n) {
        for (int r = 0; r < kRecords; ++r) {
            std::uint8_t* rec = records + r * kRecordBytes;
            std::memset(rec, 0, kRecordBytes);
            const int chars = static_cast<int>(next() % 12);
            int at = 0;
            for (int c = 0; c < chars && at < kRecordBytes - 3; ++c) at += put_char(rec + at, false);
        }
        std::uint8_t text[96] = {};
        const int chars = static_cast<int>(next() % 40);
        int at = 0;
        for (int c = 0; c < chars && at < 90; ++c) at += put_char(text + at, true);
        const unsigned color = next();                        // the low byte is read
        const unsigned count = (next() & ~0xFFu) | (next() % 48);  // a byte; 0 included
        const short pen[3] = {static_cast<short>(next()), static_cast<short>(next()), static_cast<short>(next())};

        struct Out { const unsigned char* ret; short pen[3]; unsigned n; Emit log[kLog]; };
        static Out a, b;
        auto run = [&](DrawFn fn, Out& out) {
            Text_PenX = pen[0]; Text_PenY = pen[1]; Text_LineX = pen[2];
            std::memset(prim, 0, sizeof prim);
            g_log_n = 0;
            std::memset(g_log, 0, sizeof g_log);
            out.ret = fn(color, count, text);
            out.pen[0] = Text_PenX; out.pen[1] = Text_PenY; out.pen[2] = Text_LineX;
            out.n = g_log_n;
            std::memcpy(out.log, g_log, sizeof g_log);
        };
        run(theirs, a);
        run(reinterpret_cast<DrawFn>(reinterpret_cast<void*>(&Text_DrawString)), b);
        emits += a.n;
        if ((a.ret != b.ret || std::memcmp(a.pen, b.pen, sizeof a.pen) != 0 || a.n != b.n ||
             std::memcmp(a.log, b.log, sizeof a.log) != 0) && ++bad <= 8)
            bof3::Log("shadow      Text_DrawString self-test MISMATCH round %u: ret %+d, glyphs %u / %u, pen %d,%d / %d,%d",
                      n, (int)(b.ret - a.ret), a.n, b.n, a.pen[0], a.pen[1], b.pen[0], b.pen[1]);
    }

    g_emit = Text_EmitGlyph;
    Gfx_PacketNext = saved_packet;
    Text_PenX = saved_pen[0]; Text_PenY = saved_pen[1]; Text_LineX = saved_pen[2];
    std::memcpy(records, saved_records, sizeof saved_records);
    bof3::Log("shadow      Text_DrawString self-test: %u strings, %u glyphs through the stand-in, %u MISMATCHES; "
              "the return, the three pen words and every glyph's arguments and index compared", rounds, emits, bad);
    if (bad) bof3::Fatal("Text_DrawString differs from the original in %u of %u rounds", bad, rounds);
}

}  // namespace

bool ButtonText_Code(const std::uint8_t* p, button_text::Icon& out) {
    if (!g_button_text || !InPool(p)) return false;
    const char* why = nullptr;
    if (button_text::ResolveCode(g_bt, p, out, why)) return true;
    if (why) Refuse(why, p);
    return false;
}

bool ButtonText_Cue(const std::uint8_t* p, button_text::Icon& out) {
    if (!g_button_text || g_bt.layer != button_text::Layer::kNone || !InPool(p) || !InButtonArea()) return false;
    const char* why = nullptr;
    if (button_text::ResolveCue(g_bt, p, out, why)) return true;
    if (why) Refuse(why, p);
    return false;
}

// original 0x516B70. Draws `text` at the pen (Text_PenX, Text_PenY) until a
// NUL or until `count` characters - a byte - have gone by.
//
//   0x01        newline: pen y += 12, pen x = Text_LineX
//   0x05 nn     remember the colour byte, take nn
//   0x06        take the remembered colour byte back (0 if none was)
//   0x07 nn     draw the record Text_Records + nn * 32, up to 32 characters,
//               by recursion, in the current colour byte
//   0x20        advance, draw nothing
//   >= 0x80     two bytes: glyph ((b0 & 0x7F) << 8) + b1
//   the rest    glyph b - 0x26 as 16 bits - so 0x02..0x04, 0x08..0x1F and
//               0x21..0x25 wrap past 0xA00 and trap, as any glyph above it
//
// As the original has it:
//   - (u, v), the clip pair picked by the colour byte's high nibble, is read
//     once on entry; 0x05 and 0x06 change the CLUT and not the clip;
//   - the remembered colour is one byte per call, not a stack, and a record
//     drawn by 0x07 starts with none of its own;
//   - a control's argument counts as a character of its own against `count`
//     only in that the loop looks at the byte after it: every pass, glyph or
//     control, costs one;
//   - the loop ends on the byte AFTER the one it handled being NUL, so a
//     two-byte code or an argument whose last byte is 0 ends the string, and
//     the first byte is the only one tested before anything is drawn;
//   - the return is one past the byte the loop stopped at: past the NUL, or,
//     when `count` ran out, past the first character it did not draw.
//
// DIVERGENCE DIV-0006: the pen advances by the glyph's entry in the overlay
// font's advance table where the original adds 12. With no table loaded -
// every shipped file - TextAdvance_Of is 12 and this is the original.
//
// DIVERGENCE (DIV-0140, the button unification round's DT-2;
// docs/button-dialogue.md 4): a button mention in the area text block - a
// layer's button code, or the PC's colour cue with no layer - is drawn as the
// button the Config has put its role on (ButtonText_Code / _Cue, button_text.h):
// the `05 cc` takes that button's colour and the icon its glyph, the pen
// advancing by the drawn glyph's width, so the rest of the line re-flows as
// the layer's widths do. Nothing else changes: the same bytes are walked,
// every pass costs what it cost, the return is the same. When the drawn
// button is the one the text shows, and under BOF3X_ORIGINAL=ButtonText, the
// draw is the one above.
extern "C" const unsigned char* __cdecl Text_DrawString(unsigned color_arg, unsigned count_arg,
                                                        const unsigned char* text) {
    if (g_textlog) {
        TextLog(static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(__builtin_return_address(0))), text);
    }
    unsigned color = color_arg;                     // only the low byte is ever read
    auto count = static_cast<std::uint8_t>(count_arg);
    unsigned clut = ClutOf(color);
    const std::uint8_t* pair = ClipPairs() + ((color & 0xFF) >> 4) * 2;
    const int u = pair[0], v = pair[1];
    std::uint8_t remembered = 0;
    // DIV-0140: 1 after a button code that resolved (its `05 cc` next), 2 once
    // the drawn colour is taken (the glyph next); `icon` is what to draw.
    int button = 0;
    button_text::Icon icon;

    if (text[0] == 0) return text + 1;
    do {
        if (count == 0) break;
        const std::uint8_t c = text[0];
        bool glyph = c > 0x20;
        if (!glyph) {
            switch (c) {
            case 0x00:
                break;
            case 0x01:
                Text_PenY = static_cast<short>(Text_PenY + kCell);
                Text_PenX = Text_LineX;
                break;
            case 0x05: {
                const std::uint8_t* const control = text;
                ++text;
                remembered = static_cast<std::uint8_t>(color);
                color = (color & ~0xFFu) | text[0];
                clut = ClutOf(color);
                // DIV-0140: a button code's own colour stands as the text has
                // it (06 takes it back at once); the `05 cc` after it, or a
                // colour cue, takes the drawn button's colour instead.
                if (button == 1 || (button == 0 && ButtonText_Cue(control, icon))) {
                    color = (color & ~0xFFu) | icon.colour;
                    clut = ClutOf(color);
                    button = 2;
                } else if (button == 0 && ButtonText_Code(control, icon)) {
                    button = 1;
                }
                break;
            }
            case 0x06:
                color = (color & ~0xFFu) | remembered;
                clut = ClutOf(color);
                break;
            case 0x07:
                ++text;
                Text_DrawString(color, kRecordBytes, Records() + text[0] * kRecordBytes);
                break;
            case 0x20:
                Text_PenX = static_cast<short>(Text_PenX + TextAdvance_Of(text));  // DIV-0006
                break;
            default:
                glyph = true;
                break;
            }
        }
        if (glyph) {
            // DIV-0140: the drawn button's glyph in the fallback's place; the
            // walk still steps over the fallback's own bytes.
            const std::uint8_t* const first = button == 2 ? icon.glyph : text;
            button = 0;
            std::uint16_t index;
            if (first[0] & 0x80) {
                index = static_cast<std::uint16_t>(((first[0] & 0x7F) << 8) + first[1]);
            } else {
                index = static_cast<std::uint16_t>(first[0] - 0x26);
            }
            if (c & 0x80) ++text;
            if (index > g_glyph_limit) GlyphTrap();
            std::uint32_t pa, pb;
            if (TextPair_Of(index, &pa, &pb)) {
                // DIV-0057: a pair code is its two glyphs, the second one the
                // first's advance on. It counts as two characters, but draws
                // whole even when one is left (the message box asks for 1).
                const auto a = static_cast<std::uint16_t>(pa), b = static_cast<std::uint16_t>(pb);
                if (a > g_glyph_limit || b > g_glyph_limit) GlyphTrap();
                std::memcpy(Gfx_PacketNext + 0x16, &a, sizeof a);
                g_emit(static_cast<std::uint16_t>(Text_PenX), static_cast<std::uint16_t>(Text_PenY), kCell,
                       kCell - v, u, v, static_cast<int>(clut));
                std::memcpy(Gfx_PacketNext + 0x16, &b, sizeof b);
                g_emit(static_cast<std::uint16_t>(Text_PenX + TextAdvance_OfGlyph(a)),
                       static_cast<std::uint16_t>(Text_PenY), kCell, kCell - v, u, v, static_cast<int>(clut));
                if (count > 1) --count;
            } else {
                std::memcpy(Gfx_PacketNext + 0x16, &index, sizeof index);
                g_emit(static_cast<std::uint16_t>(Text_PenX), static_cast<std::uint16_t>(Text_PenY), kCell,
                       kCell - v, u, v, static_cast<int>(clut));
            }
            Text_PenX = static_cast<short>(Text_PenX + TextAdvance_Of(first));  // DIV-0006
        }
        ++text;
        --count;
    } while (text[0] != 0);
    return text + 1;
}

void TextDraw_SetGlyphCount(unsigned glyphs) {
    g_glyph_count = glyphs;
    g_glyph_limit = glyphs > kOriginalGlyphLimit + 1 ? glyphs - 1 : kOriginalGlyphLimit;
}

unsigned TextDraw_GlyphCount() { return g_glyph_count; }

void TextDraw_Inject() {
    if (bof3::WantsShadow("text_draw")) {
        const bof3::CloneCall calls[] = {{kEmitCall, reinterpret_cast<const void*>(&RecordEmit)}};
        SelfTest(bof3::CloneOriginal("Text_DrawString", bof3::addr::Text_DrawString, kSize, calls, 1));
    }
    BOF3_INJECT(Text_DrawString);
    char flag[8];
    g_textlog = GetEnvironmentVariableA("BOF3X_TEXTLOG", flag, sizeof flag) != 0;
    if (g_textlog) bof3::Log("textlog     on: each (caller, string) pair drawn is logged once");
}

// --- DT-2 (DIV-0140): the self-test and the switch ---------------------------------

namespace {

namespace sg = bof3x::input::sigma;

sg::Table g_test_table;
std::uint16_t TestSigma(std::uint16_t bit) { return sg::Apply(g_test_table, bit); }

// The stand-in Configs: a release's defaults, then swap 1 (Speak <-> Action,
// View <-> Change: the round's swapped run) or swap 2 (Speak <-> View: a face
// role put on a shoulder).
void TestWords(sg::Release r, int swap, std::uint16_t w[9]) {
    sg::DefaultWords(r, false, w);
    const auto keep = static_cast<std::uint16_t>(w[0] & sg::kL2R2);
    const auto speak = static_cast<std::uint16_t>(w[0] & ~sg::kL2R2);
    if (swap == 1) {
        w[0] = static_cast<std::uint16_t>(keep | w[6]);
        w[6] = speak;
        const std::uint16_t view = w[3];
        w[3] = w[4];
        w[4] = view;
    } else if (swap == 2) {
        w[0] = static_cast<std::uint16_t>(keep | w[3]);
        w[3] = speak;
    }
}

// The expected drawn button for Japan's bit, from the words themselves: the
// line whose Japan default the bit is, and that line's word now.
std::uint16_t Expected(const std::uint16_t w[9], std::uint16_t bit) {
    static const struct { std::uint16_t bit; int word; } kLine[] = {
        {0x20, 0}, {0x80, 1}, {0x40, 6}, {0x10, 2}, {0x08, 3}, {0x04, 4}};
    for (const auto& l : kLine)
        if (l.bit == bit) return static_cast<std::uint16_t>(w[l.word] & ~sg::kL2R2);
    bof3::Fatal("ButtonText self-test: no line for bit 0x%X", bit);
}

struct Draw {
    unsigned x, clut, glyph;
    bool operator==(const Draw& o) const { return x == o.x && clut == o.clut && glyph == o.glyph; }
};

unsigned GlyphIndex(const std::uint8_t* g) {
    return g[0] & 0x80 ? ((g[0] & 0x7Fu) << 8) | g[1] : static_cast<unsigned>(g[0] - 0x26) & 0xFFFFu;
}

// One message through Text_DrawString (count 0xFF, colour 0) and through
// MsgBox_Step (every glyph), from the tail of the area text block, the
// switch as given. Returns the glyphs drawn.
unsigned DrawBoth(const std::uint8_t* msg, std::size_t len, bool on, Draw* ds, Draw* mb) {
    std::uint8_t* const at = reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(kPool + kPoolRoom - 0x40));
    std::memcpy(at, msg, len);
    g_button_text = on ? 1 : 0;
    Text_PenX = 0x10;
    Text_PenY = 0x20;
    Text_LineX = 0x10;
    g_log_n = 0;
    Text_DrawString(0, 0xFF, at);
    const unsigned n = g_log_n;
    for (unsigned i = 0; i < n && i < 8; ++i) ds[i] = {g_log[i].x, g_log[i].clut, g_log[i].glyph};
    MsgBoxDraw rec[8];
    const unsigned m = MsgBox_StepRecord(at, n, rec, 8);
    if (m != n) bof3::Fatal("ButtonText self-test: the message box drew %u glyphs, the string draw %u", m, n);
    for (unsigned i = 0; i < m && i < 8; ++i)
        mb[i] = {static_cast<unsigned>(rec[i].x), static_cast<unsigned>(rec[i].color), GlyphIndex(rec[i].glyph)};
    g_button_text = 0;
    return n;
}

bool SameDraws(const Draw* a, const Draw* b, unsigned n) {
    for (unsigned i = 0; i < n; ++i)
        if (!(a[i] == b[i])) return false;
    return true;
}

void ButtonTextSelfTest() {
    // Saved: the switch, the context, the pens, the packet pointer, the area,
    // and the tail of the area text block the messages are written into.
    const std::uint8_t saved_on = g_button_text;
    const button_text::Context saved_ctx = g_bt;
    const short saved_pen[3] = {Text_PenX, Text_PenY, Text_LineX};
    unsigned char* const saved_packet = Gfx_PacketNext;
    const unsigned short saved_area = Game_AreaNumber;
    static std::uint8_t saved_pool[0x40], prim[0x40];
    std::uint8_t* const tail = reinterpret_cast<std::uint8_t*>(static_cast<std::uintptr_t>(kPool + kPoolRoom - 0x40));
    std::memcpy(saved_pool, tail, sizeof saved_pool);
    Gfx_PacketNext = prim;
    g_emit = &RecordEmit;
    // A layer's table holds DIV-0051's L1 / R1 at 0xA69 / 0xA6A; the shipped
    // one, loaded at a self-test, stops at 0x992 and the guard would trap.
    const unsigned saved_limit = g_glyph_limit;
    g_glyph_limit = 0xFFF;
    Game_AreaNumber = 30;
    g_bt.sigma = &TestSigma;

    using button_text::Layer;
    struct Family { const char* name; Layer layer; sg::Release text; bool cue; bool port; };
    static const Family kFamilies[] = {
        {"the West's icons in a Latin layer", Layer::kLatin, sg::Release::kWest, false, false},
        {"PSP-EU's icons in a Latin layer", Layer::kLatin, sg::Release::kPspEu, false, false},
        {"JP's icons in ja-JP", Layer::kJapanese, sg::Release::kJapan, false, false},
        {"a kept PC message in a Latin layer", Layer::kLatin, sg::Release::kJapan, false, true},
        {"the PC's colour cue, no layer", Layer::kNone, sg::Release::kJapan, true, true},
    };
    static const sg::Release kConfigs[] = {sg::Release::kJapan, sg::Release::kWest, sg::Release::kPspEu};
    static const std::uint16_t kFaces[] = {0x20, 0x40, 0x10, 0x80};
    unsigned cases = 0, same = 0, swapped = 0, bad = 0;
    for (const Family& f : kFamilies) {
        g_bt.layer = f.layer;
        for (const sg::Release config : kConfigs) {
            for (int swap = 0; swap < 3; ++swap) {
                std::uint16_t w[9];
                TestWords(config, swap, w);
                std::string why;
                g_test_table = sg::Build(w, why);
                if (!g_test_table.valid) bof3::Fatal("ButtonText self-test: stand-in Config refused: %s", why.c_str());
                for (const std::uint16_t bit : kFaces) {
                    // What the text shows: its release's button for Japan's
                    // bit, in that button's colour, by its family's glyph.
                    const int shown = button_text::ShapeOf(sg::ReleaseMap(f.text, bit));
                    std::uint8_t msg[16];
                    std::size_t n = 0;
                    msg[n++] = 'A';
                    if (!f.cue) {
                        msg[n++] = 0x05;
                        msg[n++] = static_cast<std::uint8_t>(0xB0 + __builtin_ctz(bit));
                        msg[n++] = 0x06;
                    }
                    msg[n++] = 0x05;
                    msg[n++] = button_text::kShapeColour[shown];
                    if (f.port) {
                        msg[n++] = g_bt.port[shown];
                    } else {
                        const unsigned g = (f.layer == Layer::kLatin ? button_text::kLatinIcons
                                                                     : button_text::kJapaneseIcons) +
                                           static_cast<unsigned>(shown);
                        msg[n++] = static_cast<std::uint8_t>(0x80 | (g >> 8));
                        msg[n++] = static_cast<std::uint8_t>(g);
                    }
                    msg[n++] = 0x06;
                    msg[n++] = 'A';
                    msg[n++] = 0;
                    Draw off_ds[8], off_mb[8], on_ds[8], on_mb[8];
                    const unsigned k = DrawBoth(msg, n, false, off_ds, off_mb);
                    const unsigned k2 = DrawBoth(msg, n, true, on_ds, on_mb);
                    ++cases;
                    const int drawn = button_text::ShapeOf(Expected(w, bit));
                    bool ok = k == 3 && k2 == 3 && drawn >= 0;
                    if (ok && drawn == shown) {
                        ok = SameDraws(on_ds, off_ds, 3) && SameDraws(on_mb, off_mb, 3);
                        same += ok;
                    } else if (ok) {
                        // The icon (the second glyph) is the drawn button's, in
                        // its colour, at the same pen; the rest as before.
                        button_text::Icon want;
                        button_text::GlyphOf(g_bt, f.cue ? Layer::kNone : f.layer, drawn, want);
                        const unsigned glyph = GlyphIndex(want.glyph);
                        const unsigned colour = button_text::kShapeColour[drawn];
                        ok = on_ds[0] == off_ds[0] && on_mb[0] == off_mb[0] && on_ds[2] == off_ds[2] &&
                             on_mb[2] == off_mb[2] && on_ds[1].glyph == glyph && on_ds[1].clut == ClutOf(colour) &&
                             on_ds[1].x == off_ds[1].x && on_mb[1].glyph == glyph && on_mb[1].clut == colour &&
                             on_mb[1].x == off_mb[1].x && off_ds[1].glyph != glyph;
                        swapped += ok;
                    }
                    if (!ok && ++bad <= 8)
                        bof3::Log("shadow      ButtonText self-test MISMATCH: %s, Config %s swap %d, Japan's 0x%02X shown %d "
                                  "drawn %d",
                                  f.name, sg::ReleaseName(config), swap, bit, shown, drawn);
                }
            }
        }
    }
    // Refused, not guessed (rule 4): a code naming a shoulder, a code before
    // a glyph that is no icon, a code before another layer's icon.
    unsigned refused = 0;
    {
        g_bt.layer = Layer::kLatin;
        button_text::Icon icon;
        const char* why = nullptr;
        static const std::uint8_t shoulder[] = {0x05, 0xB2, 0x06, 0x05, 0x01, 0x89, 0xE4, 0};
        static const std::uint8_t letter[] = {0x05, 0xB5, 0x06, 0x05, 0x01, 'A', 0};
        static const std::uint8_t japanese[] = {0x05, 0xB5, 0x06, 0x05, 0x01, 0x89, 0xF7, 0};
        for (const std::uint8_t* m : {shoulder, letter, japanese}) {
            why = nullptr;
            if (button_text::ResolveCode(g_bt, m, icon, why) || !why) {
                ++bad;
                bof3::Log("shadow      ButtonText self-test: a code that cannot be drawn was not refused");
            } else {
                ++refused;
            }
        }
        // And not a mention at all: a colour outside the codes' range, a code
        // cut short.
        static const std::uint8_t colour[] = {0x05, 0x02, 0x89, 0xE3, 0};
        static const std::uint8_t cut[] = {0x05, 0xB5, 0x06, 0};
        for (const std::uint8_t* m : {colour, cut}) {
            why = nullptr;
            if (button_text::ResolveCode(g_bt, m, icon, why) || why) ++bad;
        }
        // The cue outside the button areas: drawn as it is, swapped Config or not.
        g_bt.layer = Layer::kNone;
        std::uint16_t w[9];
        TestWords(sg::Release::kJapan, 1, w);
        std::string why2;
        g_test_table = sg::Build(w, why2);
        static const std::uint8_t cue[] = {'A', 0x05, 0x02, 'b', 0x06, 'A', 0};
        Draw off_ds[8], off_mb[8], on_ds[8], on_mb[8];
        for (const unsigned short area : {78, 175, 1}) {
            Game_AreaNumber = area;
            DrawBoth(cue, sizeof cue, false, off_ds, off_mb);
            DrawBoth(cue, sizeof cue, true, on_ds, on_mb);
            if (!SameDraws(on_ds, off_ds, 3) || !SameDraws(on_mb, off_mb, 3)) {
                ++bad;
                bof3::Log("shadow      ButtonText self-test: the cue was swapped in area %u", (unsigned)area);
            }
        }
    }

    g_bt = saved_ctx;
    g_button_text = saved_on;
    Game_AreaNumber = saved_area;
    std::memcpy(tail, saved_pool, sizeof saved_pool);
    g_emit = Text_EmitGlyph;
    g_glyph_limit = saved_limit;
    Gfx_PacketNext = saved_packet;
    Text_PenX = saved_pen[0];
    Text_PenY = saved_pen[1];
    Text_LineX = saved_pen[2];
    bof3::Log("shadow      ButtonText self-test: %u mentions (5 families x 3 Configs x 3 stand-in swaps x 4 buttons) "
              "through Text_DrawString and MsgBox_Step: %u drawn as the text has them, %u as the drawn button, "
              "%u refused codes, the cue left in areas 78 / 175 / 1; %u MISMATCHES",
              cases, same, swapped, refused, bad);
    if (bad) bof3::Fatal("ButtonText self-test: %u mismatches", bad);
}

}  // namespace

void ButtonText_Arm() {
    g_bt.layer = Lang_Latin()      ? button_text::Layer::kLatin
                 : Lang_Japanese() ? button_text::Layer::kJapanese
                                   : button_text::Layer::kNone;
    // The Config cell's stand-ins, 0x461C00's six strings through 0x66A2FC:
    // one byte each, one the census knows. The PC's own image has them; a
    // disc-built image does not (the RC validation, 2026-10-17: psx-us and
    // psp-eu). They are needed with no layer (the colour cue), so there their
    // absence is fatal; under a layer only ja-JP's L1 / R1 would draw one,
    // and that draws the blank cell docs/button-dialogue.md 4 accepts.
    bool port = true;
    for (int i = 0; i < 6 && port; ++i) {
        const auto p = reinterpret_cast<const std::uint32_t*>(static_cast<std::uintptr_t>(kStandIns))[i];
        const auto* s = reinterpret_cast<const std::uint8_t*>(static_cast<std::uintptr_t>(p));
        port = p >= 0x400000u && p < 0x700000u && button_text::IsPortStandIn(s[0]) && s[1] == 0;
        if (!port && g_bt.layer == button_text::Layer::kNone)
            bof3::Fatal("ButtonText: 0x461C00's stand-in %d (0x%08X) is not a one-byte stand-in, and with no "
                        "language layer the PC's colour cue needs it",
                        i, (unsigned)p);
    }
    for (int i = 0; i < 6; ++i)
        g_bt.port[i] = port ? reinterpret_cast<const std::uint8_t* const*>(static_cast<std::uintptr_t>(kStandIns))[i][0]
                            : 0;
    if (!port) bof3::Log("ButtonText  this image holds no port stand-ins at 0x66A2FC (a disc-built image): a layer's "
                         "codes only");
    g_bt.sigma = &LiveSigma;
    if (bof3::WantsShadow("button_text")) ButtonTextSelfTest();
    static const std::uint8_t was = 0, is = 1;
    const bool on = bof3::PatchBytes(
        "ButtonText", static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&g_button_text)), &was, &is, 1);
    static const char* const kLayer[] = {"no layer: the PC's colour cue", "a Latin layer's codes", "ja-JP's codes"};
    bof3::Log("ButtonText  the dialogue's button mentions drawn as Sigma's button (DIV-0140): %s, %s",
              on ? "on" : "OFF (BOF3X_ORIGINAL=ButtonText)", kLayer[static_cast<int>(g_bt.layer)]);
}

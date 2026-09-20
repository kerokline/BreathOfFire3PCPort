#include "game/text_draw.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/text_advance.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

constexpr unsigned kGlyphLimit = 0xA00;  // 0x516C94: cmp cx, 0xA00 / jbe
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
            unsigned g = (next() & 1) ? kSeeds[next() % 8] : next() % (kGlyphLimit + 1);
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
extern "C" const unsigned char* __cdecl Text_DrawString(unsigned color_arg, unsigned count_arg,
                                                        const unsigned char* text) {
    unsigned color = color_arg;                     // only the low byte is ever read
    auto count = static_cast<std::uint8_t>(count_arg);
    unsigned clut = ClutOf(color);
    const std::uint8_t* pair = ClipPairs() + ((color & 0xFF) >> 4) * 2;
    const int u = pair[0], v = pair[1];
    std::uint8_t remembered = 0;

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
            case 0x05:
                ++text;
                remembered = static_cast<std::uint8_t>(color);
                color = (color & ~0xFFu) | text[0];
                clut = ClutOf(color);
                break;
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
            const std::uint8_t* const first = text;
            std::uint16_t index;
            if (c & 0x80) {
                index = static_cast<std::uint16_t>(((c & 0x7F) << 8) + text[1]);
                ++text;
            } else {
                index = static_cast<std::uint16_t>(c - 0x26);
            }
            if (index > kGlyphLimit) GlyphTrap();
            std::memcpy(Gfx_PacketNext + 0x16, &index, sizeof index);
            g_emit(static_cast<std::uint16_t>(Text_PenX), static_cast<std::uint16_t>(Text_PenY), kCell,
                   kCell - v, u, v, static_cast<int>(clut));
            Text_PenX = static_cast<short>(Text_PenX + TextAdvance_Of(first));  // DIV-0006
        }
        ++text;
        --count;
    } while (text[0] != 0);
    return text + 1;
}

void TextDraw_Inject() {
    if (bof3::WantsShadow("text_draw")) {
        const bof3::CloneCall calls[] = {{kEmitCall, reinterpret_cast<const void*>(&RecordEmit)}};
        SelfTest(bof3::CloneOriginal("Text_DrawString", bof3::addr::Text_DrawString, kSize, calls, 1));
    }
    BOF3_INJECT(Text_DrawString);
}

#include "game/text_immediate.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/text_advance.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

constexpr std::uint32_t kCharacterRecord = 0xA4;
constexpr std::uint32_t kTextRecord = 0x20;

// Text_DrawAt is not ours and ends in a committed primitive, so it is called
// through a pointer the fuzz can aim at a recorder - for the original's copy
// and for ours alike.
using DrawAtFn = const unsigned char* (__cdecl*)(int, int, int, int, const unsigned char*);
}  namespace text_immediate { DrawAtFn g_draw_at = Text_DrawAt; }
namespace {
const std::uint8_t* AtTextImmediate(std::uint32_t address) {
    return reinterpret_cast<const std::uint8_t*>(static_cast<std::uintptr_t>(address));
}

// --- BOF3X_SHADOW=text_immediate ------------------------------------------------
// The clone: 0x5961C0..0x596324, body and its ten-entry jump table, which
// holds absolute addresses: the copy's entries are moved into the copy and
// the `jmp [eax*4 + table]` at +0x2A re-aimed. The one call, Text_DrawAt at
// +0x100, goes to the recorder.
constexpr ::bof3::image::CodeVa kBody{0x5961C0};
constexpr std::uint32_t kSize = 0x164;
constexpr std::uint32_t kJmpDisp = 0x2D, kJumpTable = 0x13C, kJumpEntries = 10, kDrawCall = 0x100;

struct Draw {
    std::int16_t x, y;
    std::uint8_t colour, count;
    const unsigned char* text;
};
constexpr unsigned kLog = 256;
Draw g_log[kLog];
unsigned g_log_n;

const unsigned char* __cdecl RecordDraw(int x, int y, int colour, int count, const unsigned char* text) {
    if (g_log_n < kLog)
        g_log[g_log_n] = {static_cast<std::int16_t>(x), static_cast<std::int16_t>(y),
                          static_cast<std::uint8_t>(colour), static_cast<std::uint8_t>(count), text};
    ++g_log_n;
    return text + 1;  // nothing reads it
}

using ImmediateFn = const unsigned char* (__cdecl*)(int, int, const unsigned char*);

void SelfTest(void* clone) {
    auto* code = static_cast<std::uint8_t*>(clone);
    const std::uint32_t moved = Low32(code) - kBody.v;
    for (std::uint32_t i = 0; i < kJumpEntries; ++i) {
        std::uint32_t target;
        std::memcpy(&target, code + kJumpTable + 4 * i, sizeof target);
        if (target < kBody.v || target >= kBody.v + kSize)
            bof3::Fatal("Text_DrawImmediate: jump table entry %u is 0x%X", (unsigned)i, (unsigned)target);
        target += moved;
        std::memcpy(code + kJumpTable + 4 * i, &target, sizeof target);
    }
    std::uint32_t disp;
    std::memcpy(&disp, code + kJmpDisp, sizeof disp);
    if (disp != kBody.v + kJumpTable) bof3::Fatal("Text_DrawImmediate: no jump table operand at +0x%X", (unsigned)kJmpDisp);
    disp += moved;
    std::memcpy(code + kJmpDisp, &disp, sizeof disp);
    const auto theirs = reinterpret_cast<ImmediateFn>(clone);

    // What the substitutions read: the current character's name, four
    // character records' names, four 32-byte records, and the head of the
    // script block - the dword at +4 and a table behind it. Saved, filled
    // with strings, restored.
    struct Region { std::uint32_t address, size; std::uint8_t saved[0x400]; };
    static Region regions[] = {
        {::bof3::Img(bof3::addr::Text_CurrentName), 16, {}},
        {::bof3::Img(CharacterRecords_addr), 4 * kCharacterRecord, {}},
        {::bof3::Img(bof3::addr::Text_Records), 4 * kTextRecord, {}},
        {::bof3::Img(bof3::addr::MessagePools), 0x400, {}},
    };
    for (Region& r : regions) std::memcpy(r.saved, AtTextImmediate(r.address), r.size);
    auto* const script = const_cast<std::uint8_t*>(AtTextImmediate(::bof3::Img(bof3::addr::MessagePools)));

    std::uint32_t rng = 0x165667B1u;
    auto next = [&rng] { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; };
    // A run of characters with no substitution in it: glyph bytes of either
    // width, colour controls, newlines, the skipped 0x09 and the 0x02 that
    // falls into the glyph path. Substitutions do not nest in the test: the
    // original keeps ONE resume pointer, and a record that names itself
    // would never end.
    auto fill = [&](std::uint8_t* out, int room, int chars) {
        int at = 0;
        for (int c = 0; c < chars && at < room - 3; ++c) {
            const unsigned r = next() % 12;
            if (r == 0) out[at++] = 0x01;
            else if (r == 1) { out[at++] = 0x05; out[at++] = static_cast<std::uint8_t>(next() | 1); }
            else if (r == 2) out[at++] = 0x06;
            else if (r == 3) out[at++] = (next() & 1) ? 0x09 : 0x02;
            else if (r < 7) { out[at++] = static_cast<std::uint8_t>(0x80 | next()); out[at++] = static_cast<std::uint8_t>(next() | 1); }
            else out[at++] = static_cast<std::uint8_t>(0x0A + next() % 0x76);
        }
        out[at] = 0;
    };

    text_immediate::g_draw_at = &RecordDraw;
    unsigned bad = 0, draws = 0;
    const unsigned rounds = 6000;
    for (unsigned n = 0; n < rounds; ++n) {
        // Names and records of every length around their counts (9, 9, 0x20,
        // 0x10), so that both ways out of a substitution are taken: the NUL
        // and the count.
        fill(const_cast<std::uint8_t*>(AtTextImmediate(::bof3::Img(bof3::addr::Text_CurrentName))), 16, static_cast<int>(next() % 14));
        for (int i = 0; i < 4; ++i) {
            fill(const_cast<std::uint8_t*>(AtTextImmediate(::bof3::Img(CharacterRecords_addr))) + i * kCharacterRecord, 16, static_cast<int>(next() % 14));
            fill(const_cast<std::uint8_t*>(AtTextImmediate(::bof3::Img(bof3::addr::Text_Records))) + i * kTextRecord, kTextRecord, static_cast<int>(next() % 40));
        }
        const std::uint32_t table = 0x10 + (next() % 8) * 2;
        std::memcpy(script + 4, &table, sizeof table);
        for (int i = 0; i < 4; ++i) {
            const std::uint16_t off = static_cast<std::uint16_t>(0x40 + i * 0x40);
            std::memcpy(script + table + 2 * i, &off, sizeof off);
            fill(script + table + off, 0x40, static_cast<int>(next() % 24));
        }
        std::uint8_t text[160];
        int at = 0;
        const int chars = static_cast<int>(next() % 30);
        for (int c = 0; c < chars && at < 150; ++c) {
            const unsigned r = next() % 10;
            if (r == 0) text[at++] = 0x03;
            else if (r == 1) { text[at++] = 0x04; text[at++] = static_cast<std::uint8_t>(next() % 4); }
            else if (r == 2) { text[at++] = 0x07; text[at++] = static_cast<std::uint8_t>(next() % 4); }
            else if (r == 3) { text[at++] = 0x08; text[at++] = static_cast<std::uint8_t>(next() % 4); }
            else { std::uint8_t one[8]; fill(one, 8, 1); for (int k = 0; one[k]; ++k) text[at++] = one[k]; }
        }
        text[at] = 0;
        const int x = static_cast<std::int16_t>(next()), y = static_cast<std::int16_t>(next());

        struct Out { const unsigned char* ret; unsigned n; Draw log[kLog]; };
        static Out a, b;
        auto run = [&](ImmediateFn fn, Out& out) {
            g_log_n = 0;
            std::memset(g_log, 0, sizeof g_log);
            out.ret = fn(x, y, text);
            out.n = g_log_n;
            std::memcpy(out.log, g_log, sizeof g_log);
        };
        run(theirs, a);
        run(reinterpret_cast<ImmediateFn>(reinterpret_cast<void*>(&Text_DrawImmediate)), b);
        draws += a.n;
        if ((a.ret != b.ret || a.n != b.n || std::memcmp(a.log, b.log, sizeof a.log) != 0) && ++bad <= 8)
            bof3::Log("shadow      Text_DrawImmediate self-test MISMATCH round %u: ret %+d, draws %u / %u", n,
                      (int)(b.ret - a.ret), a.n, b.n);
    }
    text_immediate::g_draw_at = Text_DrawAt;
    for (Region& r : regions) std::memcpy(const_cast<std::uint8_t*>(AtTextImmediate(r.address)), r.saved, r.size);
    bof3::Log("shadow      Text_DrawImmediate self-test: %u strings, %u characters through the stand-in, %u MISMATCHES; "
              "the return and every draw's x, y, colour, count and text pointer compared", rounds, draws, bad);
    if (bad) bof3::Fatal("Text_DrawImmediate differs from the original in %u of %u rounds", bad, rounds);
}

}  // namespace

void TextImmediate_Inject() {
    if (bof3::WantsShadow("text_immediate")) {
        const bof3::CloneCall calls[] = {{kDrawCall, reinterpret_cast<const void*>(&RecordDraw)}};
        SelfTest(bof3::CloneOriginal("Text_DrawImmediate", bof3::addr::Text_DrawImmediate, kSize, calls, 1));
    }
}

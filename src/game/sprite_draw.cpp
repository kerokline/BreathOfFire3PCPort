#include "game/sprite_draw.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "hook/detour.h"
#include "hook/log.h"

// A sprite object as the draw-order pass hands it over (Sprite_Current): not a
// POLY_FT4 but one primitive of the port's own, GPU code 0x84, 0x20 bytes -
// position, scale, shade, CLUT and a run of 8-byte cell records in a side
// table, one per piece of the sprite's frame. The Direct3D end draws it from
// those (docs/sprite-draw-order.md section 14).

namespace {

template <class T> T Get(const unsigned char* p, unsigned offset) {
    T v;
    std::memcpy(&v, p + offset, sizeof v);
    return v;
}
template <class T> void Put(unsigned char* p, unsigned offset, T v) { std::memcpy(p + offset, &v, sizeof v); }

constexpr unsigned kPrimBytes = 0x20, kCellBytes = 8, kPartBytes = 5;

void Draw();

}  // namespace

// original 0x5A6780: the cell table is empty again. Once a logic frame.
extern "C" void __cdecl SpriteCell_Reset() { SpriteCell_Count = 0; }

// original 0x5A6790: one cell record, at SpriteCell_Table[SpriteCell_Count],
// and its index. Byte +3 packs the size as ((h & 0xF8) << 1 | w >> 3) in a
// byte - so a height of 128 or more loses its top bit, as the original has it.
// No bound on the count, as the original has it; SpriteCell_Reset is the only
// thing that lowers it.
extern "C" unsigned long __cdecl SpriteCell_Add(unsigned x, unsigned y, unsigned flags, unsigned u, unsigned v,
                                                unsigned w, unsigned h) {
    const unsigned long index = SpriteCell_Count;
    unsigned char* cell = SpriteCell_Table + index * kCellBytes;
    Put<std::uint16_t>(cell, 0, static_cast<std::uint16_t>(x));
    cell[2] = static_cast<unsigned char>(y);
    cell[3] = static_cast<unsigned char>(((h & 0xF8) << 1) | ((w & 0xFF) >> 3));
    Put<std::uint16_t>(cell, 4, static_cast<std::uint16_t>(flags));
    cell[6] = static_cast<unsigned char>(u);
    cell[7] = static_cast<unsigned char>(v);
    SpriteCell_Count = index + 1;
    return index;
}

// original 0x593860: the current object's CLUT word, to *out. Byte +0x27 is
// its palette and byte +0x28 how the palettes share the CLUT rows: 2^s to a
// row, s = 4, 0, 3, 2 for +0x28 = 0..3 and 1 for anything above - so column
// (palette mod 2^s) * 256 / 2^s texels, row 0x1E0 + palette / 2^s, or 0x1F0
// + with bit 2 of +0x24. (The original is a four-way jump table.)
extern "C" void __cdecl Sprite_ClutWord(unsigned short* out) {
    const unsigned char* object = Sprite_Current;
    static const unsigned char kShift[4] = {4, 0, 3, 2};
    const unsigned row = ((object[0x24] & 4u) | 0x78u) << 2;
    const unsigned shift = object[0x28] < 4 ? kShift[object[0x28]] : 1u;
    const unsigned palette = object[0x27];
    const unsigned column = ((palette & ((1u << shift) - 1)) << (8 - shift)) >> 4;
    *out = static_cast<unsigned short>(((row + (palette >> shift)) << 6) | (column & 0x3F));
}

// original 0x5935B0: the current object, drawn. In order:
//   - bit 6 of byte 0 set: nothing;
//   - bit 7 of +0x24 clear and +0x48 == 1: the scale at +0x40 and +0x44 is
//     0x4650000 / the dword at +0x60 with the low byte cleared - 1125 over a
//     depth, in 16.16 - and a depth of 0 stores 0 to both and draws nothing;
//   - the s16 position at +0x2E / +0x30 outside -0x40..0x180 / -0x40..0x130:
//     bit 7 of byte 0 set and nothing drawn; inside, bit 7 cleared;
//   - the frame is the count byte at +0x54's data + the u16 at +0x5A, then
//     that many 5-byte pieces; a count of 0 draws nothing;
//   - at Gfx_PacketNext: code 0x84, the CLUT word at +0x1C, one SpriteCell_Add
//     per piece (the first index at +0x18, the count at +0x1A), a mode word
//     at +0x1E, the dwords +0x74 / +0x78 at +8 / +0xC, the scale as floats at
//     +0x10 / +0x14 (1.0 when +0x48 is 0), the shade +0x5D..+0x5F each plus
//     0x80, semi-transparency from bit 5 of byte 0; then Gfx_CommitPrim into
//     ordering-table slot +0x29, 0x20 bytes.
// The scale goes through x87 in the original as an integer times 2^-16: the
// product is exact at any precision, so one rounding to float either way.
namespace {
void Draw() {
    unsigned char* object = Sprite_Current;
    if (object[0] & 0x40) return;
    if (!(object[0x24] & 0x80) && object[0x48] == 1) {
        const long depth = Get<long>(object, 0x60);
        if (depth == 0) {
            Put<long>(object, 0x40, 0);
            Put<long>(object, 0x44, 0);
            return;
        }
        const long scale = (0x4650000L / depth) & ~0xFFL;
        Put<long>(object, 0x40, scale);
        Put<long>(object, 0x44, scale);
    }
    const short x = Get<short>(object, 0x2E), y = Get<short>(object, 0x30);
    if (x > 0x180 || x < -0x40 || y > 0x130 || y < -0x40) {
        object[0] |= 0x80;
        return;
    }
    object[0] &= 0x7F;
    const unsigned char* frame = Get<const unsigned char*>(object, 0x54) + Get<std::uint16_t>(object, 0x5A);
    const unsigned count = frame[0];
    if (count == 0) return;

    unsigned char* const prim = Gfx_PacketNext;
    Gpu_SetCode84(prim);
    Sprite_ClutWord(reinterpret_cast<unsigned short*>(prim + 0x1C));
    float scale_x = 1.0f, scale_y = 1.0f;
    if (object[0x48] != 0) {
        scale_x = static_cast<float>(static_cast<double>(Get<long>(object, 0x40)) * (1.0 / 65536));
        scale_y = static_cast<float>(static_cast<double>(Get<long>(object, 0x44)) * (1.0 / 65536));
    }
    const unsigned wide = object[0x28] != 0 ? 1u : 0u;
    const unsigned char* piece = frame + 1;
    for (unsigned i = 0; i < count; ++i, piece += kPartBytes) {
        const unsigned size = (piece[0] & 0xFu) * 2;
        const unsigned flags = ((((piece[0] & 0x80u) | (wide << 5)) << 2) | object[0x25]);
        const unsigned long index = SpriteCell_Add(
            static_cast<unsigned>(static_cast<signed char>(piece[1])),
            static_cast<unsigned>(static_cast<signed char>(piece[2])), flags, piece[3],
            static_cast<unsigned>(object[0x26]) + piece[4], SpriteCell_Sizes[size], SpriteCell_Sizes[size + 1]);
        if (i == 0) Put<std::uint16_t>(prim, 0x18, static_cast<std::uint16_t>(index));
    }
    Put<std::uint16_t>(prim, 0x1A, static_cast<std::uint16_t>(count));
    unsigned mode = ((object[0x5C] | (wide << 2)) << 5) | object[0x25];
    if (object[0x2A] != 0) mode |= 0x400;
    Put<std::uint16_t>(prim, 0x1E, static_cast<std::uint16_t>(mode));
    Put<std::uint32_t>(prim, 8, Get<std::uint32_t>(object, 0x74));
    Put<std::uint32_t>(prim, 0xC, Get<std::uint32_t>(object, 0x78));
    Put<float>(prim, 0x10, scale_x);
    Put<float>(prim, 0x14, scale_y);
    prim[4] = static_cast<unsigned char>(object[0x5D] + 0x80);
    prim[5] = static_cast<unsigned char>(object[0x5E] + 0x80);
    prim[6] = static_cast<unsigned char>(object[0x5F] + 0x80);
    Gpu_SetSemiTrans(prim, (object[0] >> 5) & 1u);
    Gfx_CommitPrim(object[0x29], kPrimBytes);
}

// --- BOF3X_SHADOW=sprite_draw, live ----------------------------------------------
// After the start-up fuzz, every call the game makes runs the clone first, then
// puts back everything it wrote and runs ours, and compares the two: the object,
// the primitive at the packet pointer, the pointer itself, the cell records
// from the count on and the count, and the ordering-table slot the primitive
// joins with the tail it links from. Ours is what the game keeps.

using DrawFn = void (__cdecl*)();
DrawFn g_draw_clone = nullptr;
struct {
    unsigned calls, drawn, cells, mismatches;
} g_counts;

struct LiveState {
    unsigned char object[0x80];
    unsigned char prim[kPrimBytes];
    unsigned char* packet_next;
    unsigned long count;
    unsigned char cells[256 * kCellBytes];
    unsigned long slot_word, tail_word;
};

struct LivePlace {
    unsigned char* object;
    unsigned char* prim;
    unsigned char* cells;
    unsigned long* slot;
    unsigned long* tail;
};

void LiveCapture(LiveState& s, const LivePlace& at) {
    std::memcpy(s.object, at.object, sizeof s.object);
    std::memcpy(s.prim, at.prim, sizeof s.prim);
    s.packet_next = Gfx_PacketNext;
    s.count = SpriteCell_Count;
    std::memcpy(s.cells, at.cells, sizeof s.cells);
    s.slot_word = *at.slot;
    s.tail_word = at.tail ? *at.tail : 0;
}
void LiveApply(const LiveState& s, const LivePlace& at) {
    std::memcpy(at.object, s.object, sizeof s.object);
    std::memcpy(at.prim, s.prim, sizeof s.prim);
    Gfx_PacketNext = s.packet_next;
    SpriteCell_Count = s.count;
    std::memcpy(at.cells, s.cells, sizeof s.cells);
    *at.slot = s.slot_word;
    if (at.tail) *at.tail = s.tail_word;
}

void LiveDraw() {
    static LiveState input, theirs, ours;
    unsigned char* object = Sprite_Current;
    LivePlace at;
    at.object = object;
    at.prim = Gfx_PacketNext;
    at.cells = SpriteCell_Table + SpriteCell_Count * kCellBytes;
    at.slot = reinterpret_cast<unsigned long*>(&Gfx_OtPointers[object[0x29]]);
    at.tail = *at.slot ? reinterpret_cast<unsigned long*>(*at.slot) : nullptr;
    LiveCapture(input, at);
    g_draw_clone();
    LiveCapture(theirs, at);
    LiveApply(input, at);
    Draw();
    LiveCapture(ours, at);
    ++g_counts.calls;
    if (ours.count != input.count) {
        ++g_counts.drawn;
        g_counts.cells += ours.count - input.count;
    }
    if (std::memcmp(&theirs, &ours, sizeof ours) != 0 && ++g_counts.mismatches <= 8) {
        const auto* a = reinterpret_cast<const unsigned char*>(&theirs);
        const auto* b = reinterpret_cast<const unsigned char*>(&ours);
        unsigned first = 0;
        while (a[first] == b[first]) ++first;
        bof3::Log("shadow      Sprite_Draw live MISMATCH: call %u, first difference at state +0x%X (%02X / %02X)",
                  g_counts.calls, first, a[first], b[first]);
    }
    if (g_counts.calls == 1 || g_counts.calls % 1024 == 0)
        bof3::Log("shadow      Sprite_Draw live: %u calls, %u drawn with %u cells, %u MISMATCHES", g_counts.calls,
                  g_counts.drawn, g_counts.cells, g_counts.mismatches);
}

}  // namespace

extern "C" void __cdecl Sprite_Draw() {
    if (g_draw_clone) LiveDraw();
    else Draw();
}

namespace {

// --- BOF3X_SHADOW=sprite_draw: a differential fuzz, once at start-up -----------
// Byte-copies of the four; Sprite_Draw's clone calls the other two clones and,
// where it calls Gpu_SetCode84, Gpu_SetSemiTrans and Gfx_CommitPrim, Capcom's
// own - this module injects before theirs. One round: a random object and
// frame, the position biased to the cull's edges and the depth to 0, one and
// small values; the packet pointer inside the real pool, often at the room
// test's limit; the ordering-table pointers aimed at a scratch row of tails;
// the cell table's count small and a window of it random. Theirs, then ours
// from the same state; everything compared. One round in sixteen is
// SpriteCell_Reset instead, and a second loop drives Sprite_ClutWord alone.

constexpr unsigned short kGameControlWord = 0x027F;   // measured: psx_gte_float.cpp
unsigned short GetControlWord() {
    unsigned short cw;
    __asm__("fnstcw %0" : "=m"(cw));
    return cw;
}
void SetControlWord(unsigned short cw) { __asm__("fldcw %0" : : "m"(cw)); }

constexpr unsigned kMaxCount = 0x200;                                  // SpriteCell_Count at most, going in
constexpr unsigned kCellWindow = (kMaxCount + 255) * kCellBytes;       // what one call can reach
constexpr unsigned kFrameBytes = 0x100 + 1 + 255 * kPartBytes;
constexpr unsigned kWindow = 0x40;

unsigned long g_tails[8];

struct State {
    alignas(4) unsigned char object[0x80];
    alignas(4) unsigned char frame[kFrameBytes];
    unsigned char cells[kCellWindow];
    unsigned long cell_count;
    unsigned char* packet_next;
    unsigned long* ot[8];
    unsigned long tails[8];
    unsigned char buffer_index;
    unsigned char* current;
    unsigned char window[kWindow];   // the pool, from packet_next as it was on the way in
};

State g_live;   // the object and frame the calls see: Sprite_Current points in here

void Capture(State& s, const unsigned char* window_at) {
    std::memcpy(s.object, g_live.object, sizeof s.object);
    std::memcpy(s.frame, g_live.frame, sizeof s.frame);
    std::memcpy(s.cells, SpriteCell_Table, kCellWindow);
    s.cell_count = SpriteCell_Count;
    s.packet_next = Gfx_PacketNext;
    std::memcpy(s.ot, Gfx_OtPointers, sizeof s.ot);
    std::memcpy(s.tails, g_tails, sizeof s.tails);
    s.buffer_index = Gfx_BufferIndex;
    s.current = Sprite_Current;
    if (window_at) std::memcpy(s.window, window_at, kWindow);
}
void Apply(const State& s, unsigned char* window_at) {
    std::memcpy(g_live.object, s.object, sizeof s.object);
    std::memcpy(g_live.frame, s.frame, sizeof s.frame);
    std::memcpy(SpriteCell_Table, s.cells, kCellWindow);
    SpriteCell_Count = s.cell_count;
    Gfx_PacketNext = s.packet_next;
    std::memcpy(Gfx_OtPointers, s.ot, sizeof s.ot);
    std::memcpy(g_tails, s.tails, sizeof s.tails);
    Gfx_BufferIndex = s.buffer_index;
    Sprite_Current = s.current;
    if (window_at) std::memcpy(window_at, s.window, kWindow);
}

using ClutFn = void (__cdecl*)(unsigned short*);
using AddFn = unsigned long (__cdecl*)(unsigned, unsigned, unsigned, unsigned, unsigned, unsigned, unsigned);

void SelfTest(DrawFn theirs, ClutFn their_clut, DrawFn their_reset, AddFn their_add) {
    constexpr unsigned kRounds = 24000;
    static State saved, input, their_out, our_out;
    Capture(saved, nullptr);
    static unsigned char saved_pool[2 * 0x10000];
    std::memcpy(saved_pool, Gfx_PacketPools, sizeof saved_pool);
    const unsigned short saved_word = GetControlWord();

    std::uint32_t rng = 0x5935B021u;
    auto next = [&rng] { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; };
    auto edge = [&next](int low, int high) -> short {
        switch (next() % 4) {
            case 0: return static_cast<short>(low - 1 + static_cast<int>(next() % 3));
            case 1: return static_cast<short>(high - 1 + static_cast<int>(next() % 3));
            case 2: return static_cast<short>(next());
            default: return static_cast<short>(low + static_cast<int>(next() % static_cast<unsigned>(high - low + 1)));
        }
    };
    unsigned bad = 0, hidden = 0, zero_depth = 0, scaled = 0, culled = 0, empty = 0, drawn = 0, cells = 0, full = 0,
             resets = 0;
    for (unsigned round = 0; round < kRounds; ++round) {
        auto* bytes = reinterpret_cast<unsigned char*>(&input);
        for (unsigned i = 0; i < sizeof input; ++i) bytes[i] = static_cast<unsigned char>(next());
        unsigned char* object = input.object;
        // Mostly drawable: bit 6 of byte 0 clear three rounds in four.
        if (next() % 4) object[0] &= ~0x40;
        if (next() % 2) object[0x48] = 1;
        else if (next() % 2) object[0x48] = 0;
        static const long kDepth[] = {0, 1, -1, 0x4650000, 0x4650001, 0x7FFFFFFF, static_cast<long>(0x80000000u)};
        const long depth = next() % 3 == 0 ? kDepth[next() % 7] : static_cast<long>(next() % 0x4000) - 0x800;
        Put<long>(object, 0x60, depth);
        Put<short>(object, 0x2E, edge(-0x40, 0x180));
        Put<short>(object, 0x30, edge(-0x40, 0x130));
        const unsigned offset = next() % 0x100;
        Put<std::uint16_t>(object, 0x5A, static_cast<std::uint16_t>(offset));
        Put<const unsigned char*>(object, 0x54, g_live.frame);
        input.frame[offset] = static_cast<unsigned char>(next() % 4 == 0 ? next() : next() % 8);
        object[0x29] = static_cast<unsigned char>(next() % 8);   // the ninth slot is somebody else's memory
        input.cell_count = next() % kMaxCount;
        input.current = g_live.object;
        input.buffer_index = static_cast<unsigned char>(next() % 2);
        for (unsigned k = 0; k < 8; ++k) input.ot[k] = &g_tails[next() % 8];
        const std::uint32_t pool = next() % 2 ? 0x10000u : 0u;
        std::uint32_t at = next() % (0x10000u - kWindow - 0x200u);
        if (next() % 4 == 0) at = 0x10000u - 0x54u - 0x40u + next() % 0x40u;
        input.packet_next = Gfx_PacketPools + pool + at;

        const bool reset = next() % 16 == 0;
        if (reset) ++resets;
        else if (object[0] & 0x40) ++hidden;
        else if (!(object[0x24] & 0x80) && object[0x48] == 1 && depth == 0) ++zero_depth;
        else {
            const short x = Get<short>(object, 0x2E), y = Get<short>(object, 0x30);
            if (x > 0x180 || x < -0x40 || y > 0x130 || y < -0x40) ++culled;
            else if (input.frame[offset] == 0) ++empty;
            else {
                ++drawn;
                cells += input.frame[offset];
                if (object[0x48] != 0) ++scaled;
                const std::uint32_t limit = (static_cast<std::uint32_t>(input.buffer_index) << 16) + 0x7F1BACu;
                if (limit <= static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(input.packet_next)) + kPrimBytes)
                    ++full;
            }
        }

        unsigned char* const window_at = input.packet_next;
        for (int pass = 0; pass < 2; ++pass) {
            Apply(input, window_at);
            if (pass == 0) SetControlWord(kGameControlWord);
            if (reset) (pass ? DrawFn(&SpriteCell_Reset) : their_reset)();
            else (pass ? DrawFn(&Sprite_Draw) : theirs)();
            if (pass == 0) SetControlWord(saved_word);
            Capture(pass ? our_out : their_out, window_at);
        }
        if (std::memcmp(&their_out, &our_out, sizeof their_out) != 0 && ++bad <= 8) {
            const auto* a = reinterpret_cast<const unsigned char*>(&their_out);
            const auto* b = reinterpret_cast<const unsigned char*>(&our_out);
            unsigned first = 0;
            while (a[first] == b[first]) ++first;
            bof3::Log("shadow      sprite_draw self-test MISMATCH: round %u, %s, count %u, first difference at state "
                      "+0x%X (%02X / %02X)", round, reset ? "reset" : "draw", input.frame[offset], first, a[first],
                      b[first]);
        }
    }

    // Sprite_ClutWord alone: all three bytes it reads, random, and the rest.
    unsigned clut_bad = 0;
    constexpr unsigned kClutRounds = 65536;
    for (unsigned round = 0; round < kClutRounds; ++round) {
        for (auto& b : g_live.object) b = static_cast<unsigned char>(next());
        g_live.object[0x28] = static_cast<unsigned char>(round % 8 < 6 ? round % 8 : next());
        Sprite_Current = g_live.object;
        unsigned short theirs_word = static_cast<unsigned short>(next()), ours_word = theirs_word;
        their_clut(&theirs_word);
        Sprite_ClutWord(&ours_word);
        if (theirs_word != ours_word && ++clut_bad <= 8)
            bof3::Log("shadow      Sprite_ClutWord self-test MISMATCH: +0x24 %02X +0x27 %02X +0x28 %02X: %04X / %04X",
                      g_live.object[0x24], g_live.object[0x27], g_live.object[0x28], theirs_word, ours_word);
    }

    // SpriteCell_Add alone: every argument random, whole dwords - Sprite_Draw
    // only ever hands it sizes of 8..32, which cannot show the packing.
    unsigned add_bad = 0;
    constexpr unsigned kAddRounds = 65536;
    for (unsigned round = 0; round < kAddRounds; ++round) {
        unsigned args[7];
        for (auto& a : args) a = next();
        const unsigned long count = next() % kMaxCount;
        unsigned long result[2];
        unsigned char cells[2][kCellWindow];
        unsigned long counts[2];
        for (auto& b : input.cells) b = static_cast<unsigned char>(next());
        for (int pass = 0; pass < 2; ++pass) {
            std::memcpy(SpriteCell_Table, input.cells, kCellWindow);
            SpriteCell_Count = count;
            result[pass] = (pass ? AddFn(&SpriteCell_Add) : their_add)(args[0], args[1], args[2], args[3], args[4],
                                                                         args[5], args[6]);
            std::memcpy(cells[pass], SpriteCell_Table, kCellWindow);
            counts[pass] = SpriteCell_Count;
        }
        if ((result[0] != result[1] || counts[0] != counts[1] || std::memcmp(cells[0], cells[1], kCellWindow) != 0) &&
            ++add_bad <= 8)
            bof3::Log("shadow      SpriteCell_Add self-test MISMATCH: round %u, count %lu", round, count);
    }

    Apply(saved, nullptr);
    std::memcpy(Gfx_PacketPools, saved_pool, sizeof saved_pool);
    SetControlWord(saved_word);
    bof3::Log("shadow      sprite_draw self-test: %u rounds (%u hidden, %u at depth 0, %u culled, %u empty frames, %u "
              "drawn with %u cells - %u scaled, %u with no room in the pool - and %u resets), %u MISMATCHES; the "
              "object, its frame, a window of the cell table and its count, the packet pointer and 0x%X bytes at it, "
              "the ordering-table pointers and their tails compared. Sprite_ClutWord alone: %u rounds, %u MISMATCHES; "
              "SpriteCell_Add alone: %u rounds, %u MISMATCHES",
              kRounds, hidden, zero_depth, culled, empty, drawn, cells, scaled, full, resets, bad, kWindow, kClutRounds,
              clut_bad, kAddRounds, add_bad);
    if (bad || clut_bad || add_bad)
        bof3::Fatal("sprite_draw differs from the originals: %u of %u rounds, %u of %u ClutWord rounds, %u of %u Add "
                    "rounds", bad, kRounds, clut_bad, kClutRounds, add_bad, kAddRounds);
}

}  // namespace

void SpriteDraw_Inject() {
    // Before PsxGpu_Inject and DrawEmit_Inject: the clone's calls to
    // Gpu_SetCode84, Gpu_SetSemiTrans and Gfx_CommitPrim go where the
    // original's went, which must still be Capcom's code while the fuzz runs.
    if (bof3::WantsShadow("sprite_draw")) {
        // Sizes and call offsets: capstone 2026-09-21. 0x593860's jump table
        // at 0x593938 holds absolute addresses into the original's body, so
        // the clone's cases run there - Capcom's bytes either way, since the
        // fuzz runs before anything is patched. No other jump leaves any.
        void* reset = bof3::CloneOriginal("SpriteCell_Reset", bof3::addr::SpriteCell_Reset, 0xB);
        void* add = bof3::CloneOriginal("SpriteCell_Add", bof3::addr::SpriteCell_Add, 0x61);
        void* clut = bof3::CloneOriginal("Sprite_ClutWord", bof3::addr::Sprite_ClutWord, 0xD5);
        const bof3::CloneCall calls[] = {{0xBA, nullptr}, {0xC3, clut}, {0x1B3, add}, {0x281, nullptr}, {0x292, nullptr}};
        void* draw = bof3::CloneOriginal("Sprite_Draw", bof3::addr::Sprite_Draw, 0x2AB, calls, 5);
        g_draw_clone = nullptr;
        SelfTest(reinterpret_cast<DrawFn>(draw), reinterpret_cast<ClutFn>(clut), reinterpret_cast<DrawFn>(reset),
                 reinterpret_cast<AddFn>(add));
        // From here on Sprite_Draw compares every live call against the clone.
        g_draw_clone = reinterpret_cast<DrawFn>(draw);
    }
    BOF3_INJECT(SpriteCell_Reset);
    BOF3_INJECT(SpriteCell_Add);
    BOF3_INJECT(Sprite_ClutWord);
    BOF3_INJECT(Sprite_Draw);
}

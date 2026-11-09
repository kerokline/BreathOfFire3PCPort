#include "game/map_cells.h"

#include <cstdint>
#include <cstring>
#include <type_traits>

#include "bof3/symbols.gen.h"
#include "game/crt_rest.h"
#include "game/wide_rand.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/draw_order.h"
#include "hook/log.h"

// Two of the map-cell handlers DrawLayer_Open hands a cell's records to, and
// the two functions under them that were still Capcom's: the condition test a
// record carries, and the ground's elevation at a cell
// (docs/sprite-draw-order.md section 16). Everything else these call is ours.

namespace {

template <class T> T Get(const unsigned char* p, unsigned offset) {
    // A pointer-typed field is a 32-bit word of the image on every width.
    if constexpr (std::is_pointer_v<T>) {
        std::uint32_t w;
        std::memcpy(&w, p + offset, sizeof w);
        return reinterpret_cast<T>(static_cast<std::uintptr_t>(w));
    } else {
        T v;
        std::memcpy(&v, p + offset, sizeof v);
        return v;
    }
}

constexpr unsigned kPrimBytes = 0x48;   // a POLY_FT4 as the port lays it out

// Where MapCell_DrawRising gets its random numbers: Capcom's Rand, except
// during the start-up fuzz, which runs before the exe's C runtime is set up
// (the launcher loads us into a suspended process) and so cannot call it.
int (__cdecl* g_rand)(void) = Rand;

}  // namespace

int map_cells::RisingRand() { return wide_rand::Band() ? wide_rand::DrawOnly() : g_rand(); }
map_cells::RandFn map_cells::SwapRand(RandFn rand) {
    const RandFn was = g_rand;
    g_rand = rand;
    return was;
}

namespace {

// --- BOF3X_SHADOW=map_cells: differential fuzzes, once at start-up -----------
// Byte-copies of the four, their calls left where the originals called (the
// handlers' calls to these two re-aimed at their clones): this module injects
// BEFORE every module that owns one of their callees, so at fuzz time theirs
// runs Capcom's whole call tree and ours runs ours - except Rand, which
// needs the C runtime the exe has not started yet: both sides get RandStandIn,
// the same generator on a seed of our own, and the seed is part of the state.

constexpr unsigned short kGameControlWord = 0x027F;   // measured: psx_gte_float.cpp
unsigned short GetControlWord() {
    unsigned short cw;
    __asm__("fnstcw %0" : "=m"(cw));
    return cw;
}
void SetControlWord(unsigned short cw) { __asm__("fldcw %0" : : "m"(cw)); }

std::uint32_t g_rng;
std::uint32_t Next() {
    g_rng ^= g_rng << 13;
    g_rng ^= g_rng >> 17;
    g_rng ^= g_rng << 5;
    return g_rng;
}

// The condition globals, all of them, and a code aimed at the kinds.
struct Conditions {
    unsigned char flags[0x108];
    unsigned char ff, fe, fd, request;
    signed char fa;
    unsigned short mode;
    unsigned long angle;
};
void ReadConditions(Conditions& c) {
    std::memcpy(c.flags, Cond_Flags, sizeof c.flags);
    c.ff = Cond_ByteFF; c.fe = Cond_ByteFE; c.fd = Cond_ByteFD; c.request = Field_Request;
    c.fa = Cond_ByteFA; c.mode = Game_Mode; c.angle = Cond_AngleFB;
}
void WriteConditions(const Conditions& c) {
    std::memcpy(Cond_Flags, c.flags, sizeof c.flags);
    Cond_ByteFF = c.ff; Cond_ByteFE = c.fe; Cond_ByteFD = c.fd; Field_Request = c.request;
    Cond_ByteFA = c.fa; Game_Mode = c.mode; Cond_AngleFB = c.angle;
}
void RandomConditions() {
    Conditions c;
    for (unsigned char& b : c.flags) b = static_cast<unsigned char>(Next());
    c.ff = static_cast<unsigned char>(Next());
    c.fe = static_cast<unsigned char>(Next());
    c.fd = static_cast<unsigned char>(Next());
    c.fa = static_cast<signed char>(Next() % 2 ? Next() % 0x100 : Next() % 4 + 0x7E);
    c.mode = static_cast<unsigned short>(Next() % 4 == 0 ? 7 : Next() % 16);
    c.request = static_cast<unsigned char>(Next() % 4 == 0 ? 4 : Next() % 8);
    // The angle: a third of the time a step from either end of the true half.
    static const unsigned long kEdge[] = {0x1FF, 0x200, 0x201, 0x9FF, 0xA00, 0xA01};
    c.angle = Next() % 3 == 0 ? (Next() & ~0xFFFul) | kEdge[Next() % 6] : Next();
    WriteConditions(c);
}
unsigned long RandomCode() {
    static const unsigned kSpecial[] = {0x40, 0xFF, 0xFE, 0xFD, 0xFC, 0xFB, 0xFA};
    static const unsigned kEdge[] = {0x1F, 0x20, 0x3F, 0x41, 0xF9, 0x00};
    unsigned kind;
    switch (Next() % 4) {
        case 0: kind = kSpecial[Next() % 7]; break;
        case 1: kind = Next() % 0x40; break;
        case 2: kind = kEdge[Next() % 6]; break;
        default: kind = Next() & 0xFF; break;
    }
    unsigned operand = Next() & 0xFF;
    if (Next() % 2 == 0) {
        switch (kind) {
            case 0xFE: operand = Cond_ByteFE; break;
            case 0xFD: operand = Cond_ByteFD; break;
            case 0xFA: operand = static_cast<unsigned char>(Cond_ByteFA); break;
            default: break;
        }
    }
    return (Next() & 0xFFFF0000ul) | (kind << 8) | operand;   // stale bits above, as the callers push
}

using TestFn = unsigned char (__cdecl*)(unsigned long);
using ElevationFn = long (__cdecl*)(long, long);
using HandlerFn = void (__cdecl*)(const unsigned char*, unsigned, unsigned);

void SelfTestCondition(TestFn theirs) {
    constexpr unsigned kRounds = 65536;
    Conditions saved;
    ReadConditions(saved);
    g_rng = 0x56FF0001u;
    unsigned bad = 0, special = 0, flags = 0, other = 0, wide = 0;
    for (unsigned round = 0; round < kRounds; ++round) {
        if (round % 16 == 0) RandomConditions();
        const unsigned long code = RandomCode();
        const unsigned kind = (code >> 8) & 0xFF;
        if (kind >= 0xFA) ++special;
        else if (kind < 0x40) ++flags;
        else ++other;
        const unsigned char their = theirs(code), our = Area_TestCondition(code);
        if (their > 1) ++wide;
        if (their != our && ++bad <= 8)
            bof3::Log("shadow      map_cells Area_TestCondition MISMATCH: round %u, code %08lX, %02X / %02X", round, code,
                      their, our);
    }
    WriteConditions(saved);
    bof3::Log("shadow      map_cells Area_TestCondition self-test: %u rounds (%u kinds FA..FF, %u flag kinds, %u "
              "others; %u answers above 1), %u MISMATCHES; al compared",
              kRounds, special, flags, other, wide, bad);
    if (bad) bof3::Fatal("Area_TestCondition differs from the original in %u of %u self-test rounds", bad, kRounds);
}

// The area block's first 0x2000 bytes stand in for an area: a grid of up to
// 40 x 40 cells, its heights and its corners all inside them.
constexpr unsigned kAreaWindow = 0x2000;

void RandomArea(unsigned max_side) {
    for (unsigned i = 0; i < kAreaWindow; ++i) AreaMap_Header[i] = static_cast<unsigned char>(Next());
    const unsigned width = Next() % 16 == 0 ? 0 : 1 + Next() % max_side;
    const unsigned depth = Next() % 16 == 0 ? 0 : 1 + Next() % max_side;
    AreaMap_Header[0] = static_cast<unsigned char>(width);
    AreaMap_Header[1] = static_cast<unsigned char>(depth);
    AreaMap_HeightBase = static_cast<unsigned short>(Next() % 0x200);
    static const unsigned char kScale[] = {0, 1, 2, 0x7F, 0x80, 0xFF};
    MapView_HeightScale = static_cast<unsigned char>(Next() % 2 ? kScale[Next() % 6] : Next());
}
long RandomCoordinate(unsigned side) {
    long whole;
    if (Next() % 8 == 0) whole = static_cast<long>(static_cast<short>(Next()));
    else whole = static_cast<long>(Next() % (side + 4)) - 2;
    return static_cast<long>((static_cast<std::uint32_t>(whole) << 16) | (Next() & 0xFFFFu));
}

void SelfTestElevation(ElevationFn theirs) {
    constexpr unsigned kRounds = 131072;
    static unsigned char saved[kAreaWindow];
    std::memcpy(saved, AreaMap_Header, kAreaWindow);
    const unsigned char saved_scale = MapView_HeightScale;
    g_rng = 0x5720C001u;
    unsigned bad = 0, outside = 0, quadrant[4] = {0, 0, 0, 0};
    for (unsigned round = 0; round < kRounds; ++round) {
        if (round % 256 == 0) RandomArea(40);
        const long x = RandomCoordinate(AreaMap_Header[0]), y = RandomCoordinate(AreaMap_Header[1]);
        const long their = theirs(x, y), our = AreaMap_Elevation(x, y);
        const auto cx = static_cast<short>(static_cast<std::uint32_t>(x) >> 16);
        const auto cy = static_cast<short>(static_cast<std::uint32_t>(y) >> 16);
        if (cx < 0 || cy < 0 || cx >= AreaMap_Header[0] || cy >= AreaMap_Header[1]) ++outside;
        else ++quadrant[((x >> 15) & 1) | ((y >> 14) & 2)];
        if (their != our && ++bad <= 8)
            bof3::Log("shadow      map_cells AreaMap_Elevation MISMATCH: round %u, (%08lX, %08lX), %08lX / %08lX", round,
                      x, y, their, our);
    }
    std::memcpy(AreaMap_Header, saved, kAreaWindow);
    MapView_HeightScale = saved_scale;
    bof3::Log("shadow      map_cells AreaMap_Elevation self-test: %u rounds (%u outside the grid; inside, quadrants "
              "%u / %u / %u / %u), %u MISMATCHES; eax compared",
              kRounds, outside, quadrant[0], quadrant[1], quadrant[2], quadrant[3], bad);
    if (bad) bof3::Fatal("AreaMap_Elevation differs from the original in %u of %u self-test rounds", bad, kRounds);
}

// --- the two handlers: everything they touch is state ------------------------

unsigned long g_tails[8];
std::uint32_t g_seed;

// MSVC's rand(), as Rand 0x5B93D2 computes it, on g_seed.
int __cdecl RandStandIn(void) {
    g_seed = g_seed * 0x343FDu + 0x269EC3u;
    return static_cast<int>((g_seed >> 16) & 0x7FFFu);
}
alignas(4) unsigned char g_record[4 + 6 * 0x14];

struct Region { unsigned char* at; unsigned size; };
constexpr unsigned kRegions = 16, kStateBytes = 0x3000, kPacketWindow = 0x300;

void Capture(const Region* r, unsigned char* out) {
    for (unsigned i = 0; i < kRegions; out += r[i].size, ++i) std::memcpy(out, r[i].at, r[i].size);
}
void Apply(const Region* r, const unsigned char* in) {
    for (unsigned i = 0; i < kRegions; in += r[i].size, ++i) std::memcpy(r[i].at, in, r[i].size);
}

// Half the rounds a scene rather than noise: a near-identity rotation, a
// translation in front, a projection and offsets of a screen's size, so that
// points land around the cull's bounds.
void RandomScene() {
    auto small = [](unsigned span) { return static_cast<long>(Next() % (2 * span + 1)) - static_cast<long>(span); };
    auto* rotation = reinterpret_cast<short*>(Gte_Matrix);
    for (unsigned i = 0; i < 9; ++i) rotation[i] = static_cast<short>((i % 4 == 0 ? 0x1000 : 0) + small(0x200));
    Gte_Matrix[5] = static_cast<unsigned long>(small(0x400));
    Gte_Matrix[6] = static_cast<unsigned long>(small(0x400));
    Gte_Matrix[7] = static_cast<unsigned long>(0x400 + Next() % 0x4000);
    Gte_ProjDistance = 0x100 + static_cast<long>(Next() % 0x300);
    Gte_OffsetX = 160 + small(40);
    Gte_OffsetY = 120 + small(40);
    Gte_NearZ = small(0x40);
}
long RandomNumber() {
    static const long kEdge[] = {0, 1, -1, 2, 0x7FFFFFFF, static_cast<long>(0x80000000u), 0x1000, 500, 1000};
    switch (Next() % 8) {
        case 0: return static_cast<long>(Next());
        case 1: return kEdge[Next() % 9];
        case 2: return static_cast<long>(Next() % 0x2000000u) - 0x1000000;
        default: return static_cast<long>(Next() % 0x4000u) - 0x1000;
    }
}

void SelfTestHandlers(HandlerFn their_quads, HandlerFn their_rising) {
    constexpr unsigned kRounds = 24000;
    static unsigned char saved[kStateBytes], input[kStateBytes], their_out[kStateBytes], our_out[kStateBytes];
    auto* gte = reinterpret_cast<unsigned char*>(&Gte_RampFar);   // 0x7DE428 .. Gte_Depth, 0x7DE7A8
    const unsigned gte_size = static_cast<unsigned>(reinterpret_cast<unsigned char*>(&Gte_Depth) + 4 - gte);
    auto* pools = Gfx_PacketPools;
    const unsigned short saved_word = GetControlWord();
    const unsigned pad_at = static_cast<unsigned>(reinterpret_cast<unsigned char*>(Gte_Vertices) - gte);

    g_rng = 0x57002001u;
    unsigned bad = 0, rising = 0, refused = 0, walked = 0, drawn = 0, dropped = 0, scenes = 0, padding = 0;
    for (unsigned round = 0; round < kRounds; ++round) {
        const bool is_rising = round % 3 == 2;
        const unsigned buffer = Next() % 2, pool = Next() % 2;
        unsigned offset = Next() % (0x10000u - 0x54u - 8u * kPrimBytes);
        if (Next() % 6 == 0) offset = 0x10000u - 0x54u - (1 + Next() % 8) * kPrimBytes + (Next() % 3) - 1;
        unsigned char* packet = pools + pool * 0x10000u + offset;
        const unsigned window = static_cast<unsigned>(pools + 0x20000u - packet) < kPacketWindow
                                    ? static_cast<unsigned>(pools + 0x20000u - packet) : kPacketWindow;
        const Region regions[kRegions] = {
            {gte, gte_size},
            {reinterpret_cast<unsigned char*>(&Gfx_PacketNext), 4},
            {reinterpret_cast<unsigned char*>(&Gfx_BufferIndex), 1},
            {reinterpret_cast<unsigned char*>(Gfx_OtPointers), 8 * 4},
            {reinterpret_cast<unsigned char*>(g_tails), sizeof g_tails},
            {reinterpret_cast<unsigned char*>(&Draw_OtSlot), 1},
            {reinterpret_cast<unsigned char*>(Prim_VertexScratch), 0x20},
            {reinterpret_cast<unsigned char*>(&Frame_Counter), 4},
            {reinterpret_cast<unsigned char*>(&g_seed), 4},
            {AreaMap_Header, 0x800},
            {reinterpret_cast<unsigned char*>(&MapView_HeightScale), 1},
            {g_record, sizeof g_record},
            {Cond_Flags, 0x108},
            {reinterpret_cast<unsigned char*>(&Game_Mode), 2},
            {reinterpret_cast<unsigned char*>(&Cond_AngleFB), 4},
            {packet, window},
        };
        // The five one-byte condition globals are not regions: nothing here
        // writes them, and RandomConditions sets them each round.
        if (round == 0) Capture(regions, saved);
        static unsigned char window_saved[kPacketWindow];
        std::memcpy(window_saved, packet, window);
        static Conditions conditions_saved;
        if (round == 0) ReadConditions(conditions_saved);
        unsigned total = 0;
        for (const Region& r : regions) total += r.size;
        if (total > kStateBytes) bof3::Fatal("map_cells self-test: state of 0x%X bytes", total);

        Capture(regions, input);
        for (unsigned i = 0; i < total; ++i) input[i] = static_cast<unsigned char>(Next());
        Apply(regions, input);
        RandomConditions();
        Gfx_PacketNext = packet;
        Gfx_BufferIndex = static_cast<unsigned char>(buffer);
        for (unsigned k = 0; k < 8; ++k) Gfx_OtPointers[k] = &g_tails[Next() % 8];
        Draw_OtSlot = static_cast<unsigned char>(Next() % 8);
        Gte_NearZ = RandomNumber(); Gte_ProjDistance = RandomNumber();
        Gte_OffsetX = RandomNumber(); Gte_OffsetY = RandomNumber();
        Gte_RampNear = RandomNumber(); Gte_RampFar = RandomNumber();
        Gte_RampNearScale = RandomNumber(); Gte_RampFarScale = RandomNumber();
        const bool scene = Next() % 2 == 0;
        if (scene) {
            ++scenes;
            RandomScene();
        }
        // A small grid inside the window, as the elevation fuzz has it.
        AreaMap_Header[0] = static_cast<unsigned char>(1 + Next() % 16);
        AreaMap_Header[1] = static_cast<unsigned char>(1 + Next() % 16);
        AreaMap_HeightBase = static_cast<unsigned short>(0x10 + Next() % 0x100);
        unsigned b1, b0;
        if (scene) {
            b1 = 0x80 + Next() % 5 - 2;
            b0 = 0x80 + Next() % 5 - 2;
        } else if (Next() % 2) {
            b1 = 1 + Next() % 17;   // (b1 - 1, b0): inside the grid, mostly
            b0 = Next() % 17;
        } else {
            b1 = Next() % 8 ? Next() & 0xFF : Next();
            b0 = Next() % 8 ? Next() & 0xFF : Next();
        }
        if (!is_rising) {
            // The record: a condition, a length of 1 + 5n, n subrecords.
            const unsigned n = Next() % 16 == 0 ? 0 : 1 + Next() % 6;
            g_record[2] = static_cast<unsigned char>(1 + 5 * n);
            const unsigned long code = Next() % 2 ? 0x4001ul | (Next() & 0xFFFF0000ul) : RandomCode();
            std::memcpy(g_record, &code, 2);
            if (scene)
                for (unsigned k = 0; k < n * 5; ++k) {
                    if (k % 5 == 4) continue;   // the texture word stays random
                    unsigned char* dword = g_record + 4 + 4 * k;
                    const auto z = static_cast<short>(static_cast<int>(Next() % 0x801) - 0x400);
                    std::memcpy(dword, &z, 2);
                    dword[2] = static_cast<unsigned char>(Next() % 0x81 - 0x40);
                    dword[3] = static_cast<unsigned char>(Next() % 0x81 - 0x40);
                }
        }
        Capture(regions, input);

        const HandlerFn ours = is_rising ? MapCell_DrawRising : MapCell_DrawQuads;
        for (int pass = 0; pass < 2; ++pass) {
            Apply(regions, input);
            if (pass == 0) SetControlWord(kGameControlWord);
            (pass ? ours : (is_rising ? their_rising : their_quads))(g_record, b1, b0);
            if (pass == 0) SetControlWord(saved_word);
            Capture(regions, pass ? our_out : their_out);
        }
        // DIV-0023: the top halves of Gte_Vertices[1], [3], [5] are stale
        // stack from the original and 0 from ours. Where they differ, ours
        // must be 0; theirs is then taken there, counted apart.
        for (unsigned slot = 1; slot < 6; slot += 2) {
            unsigned char* our_pad = our_out + pad_at + 4 * slot + 2;
            const unsigned char* their_pad = their_out + pad_at + 4 * slot + 2;
            if (std::memcmp(our_pad, their_pad, 2) == 0) continue;
            if (our_pad[0] != 0 || our_pad[1] != 0) continue;   // left to differ: a mismatch below
            ++padding;
            std::memcpy(our_pad, their_pad, 2);
        }
        if (is_rising) {
            ++rising;
        } else if (Area_TestCondition(Get<std::uint16_t>(g_record, 0)) == 0 || g_record[2] == 1) {
            ++refused;
        } else {
            ++walked;
            const unsigned advanced = static_cast<unsigned>(Get<unsigned char*>(their_out, gte_size) - packet);
            const unsigned n = (g_record[2] - 1) / 5, kept = advanced / kPrimBytes;
            drawn += kept;
            dropped += n - kept;
        }
        if (std::memcmp(their_out, our_out, total) != 0 && ++bad <= 8) {
            unsigned at = 0;
            while (at < total && their_out[at] == our_out[at]) ++at;
            unsigned region = 0, base = 0;
            while (region < kRegions && at >= base + regions[region].size) base += regions[region++].size;
            bof3::Log("shadow      map_cells %s MISMATCH: round %u, b1 %X, b0 %X, first difference in region %u at +0x%X "
                      "(%02X / %02X)", is_rising ? "MapCell_DrawRising" : "MapCell_DrawQuads", round, b1, b0, region,
                      at - base, their_out[at], our_out[at]);
        }
        std::memcpy(packet, window_saved, window);
        if (round == kRounds - 1) WriteConditions(conditions_saved);
    }
    const Region fixed[kRegions] = {
        {gte, gte_size},
        {reinterpret_cast<unsigned char*>(&Gfx_PacketNext), 4},
        {reinterpret_cast<unsigned char*>(&Gfx_BufferIndex), 1},
        {reinterpret_cast<unsigned char*>(Gfx_OtPointers), 8 * 4},
        {reinterpret_cast<unsigned char*>(g_tails), sizeof g_tails},
        {reinterpret_cast<unsigned char*>(&Draw_OtSlot), 1},
        {reinterpret_cast<unsigned char*>(Prim_VertexScratch), 0x20},
        {reinterpret_cast<unsigned char*>(&Frame_Counter), 4},
        {reinterpret_cast<unsigned char*>(&g_seed), 4},
        {AreaMap_Header, 0x800},
        {reinterpret_cast<unsigned char*>(&MapView_HeightScale), 1},
        {g_record, sizeof g_record},
        {Cond_Flags, 0x108},
        {reinterpret_cast<unsigned char*>(&Game_Mode), 2},
        {reinterpret_cast<unsigned char*>(&Cond_AngleFB), 4},
        {pools, 0},
    };
    Apply(fixed, saved);
    SetControlWord(saved_word);
    bof3::Log("shadow      map_cells handlers self-test: %u rounds under control word %04X (MapCell_DrawRising %u; "
              "MapCell_DrawQuads %u refused, %u walked - %u quads committed, %u culled or without room; %u "
              "scenes), %u MISMATCHES; the GTE's globals, the packet window and pointer, the ordering "
              "table and its tails, the vertex scratch, the frame counter, the stand-in Rand's seed, the area window, the record and "
              "the conditions compared; a vertex pad word differed %u times (DIV-0023, not counted)",
              kRounds, kGameControlWord, rising, refused, walked, drawn, dropped, scenes, bad, padding);
    if (bad) bof3::Fatal("the map-cell handlers differ from the originals in %u of %u self-test rounds", bad, kRounds);
}

}  // namespace

namespace {

// --- BOF3X_SHADOW=map_cells, live ----------------------------------------------
// After the start-up fuzz, every call the game makes runs the clone first, then
// puts back everything it wrote and runs ours, and compares the two: the GTE's
// globals, the vertex scratch, the packet pointer and a window of the pool
// after it, the ordering-table pointers and the tail words they pointed at, and
// Rand's seed (crt_rest::RandSeedCell: ours, or with Rand left original the C
// runtime's at Crt_GetPtd() + 0x14 - there by the time the game draws). Ours
// is what the game keeps. DIV-0023's pad words are taken as
// the fuzz takes them.
}  // namespace
// DIV-0041's random split: a band cell's rising squares are checked against a clone whose Rand
// call is aimed at the draw-only generator, as ours draws them; both seeds are
// in the compared state, so either side touching the wrong one is a mismatch.
namespace map_cells { HandlerFn g_quads_live = nullptr, g_rising_live = nullptr, g_rising_band_live = nullptr; }  namespace {
constexpr unsigned kLiveGte = 0x380, kLiveWindow = 0x1000;

unsigned char* LiveGte() { return reinterpret_cast<unsigned char*>(&Gte_RampFar); }
unsigned LiveGteSize() {
    return static_cast<unsigned>(reinterpret_cast<unsigned char*>(&Gte_Depth) + 4 - LiveGte());
}

}  // namespace

void MapCells_Inject() {
    // Called BEFORE every module that owns a callee of these four: the
    // clones' calls go where the originals' went, which must still be
    // Capcom's code while the fuzz runs. Offsets of every call: capstone over
    // each function, 2026-09-21. No jump leaves any of them.
    if (bof3::WantsShadow("map_cells")) {
        static const bof3::CloneCall kTestCalls[] = {{0xE7, nullptr}, {0x106, nullptr}};   // Flags_Test twice
        const void* test =
            bof3::CloneOriginal("Area_TestCondition", bof3::addr::Area_TestCondition, 0x118, kTestCalls, 2);
        const void* elevation = bof3::CloneOriginal("AreaMap_Elevation", bof3::addr::AreaMap_Elevation, 0x20B);
        SelfTestCondition(reinterpret_cast<TestFn>(const_cast<void*>(test)));
        SelfTestElevation(reinterpret_cast<ElevationFn>(const_cast<void*>(elevation)));
        // 0xE -> Area_TestCondition, to its clone; then Gte_LoadVertex, Gte_Rtps,
        // Gte_StoreScreenXY, Gpu_SetPolyFT4, Gpu_SetShadeTex, Gte_LoadVertices3,
        // Prim_SetTexture, Gte_Rtpt, Gfx_CommitPrim, Gte_StoreScreenXY3,
        // Gte_PrimDepths4_10.
        const bof3::CloneCall quad_calls[] = {{0xE, test},     {0x94, nullptr},  {0x99, nullptr},  {0xA8, nullptr},
                                              {0xFF, nullptr}, {0x107, nullptr}, {0x15C, nullptr}, {0x168, nullptr},
                                              {0x170, nullptr}, {0x1A2, nullptr}, {0x1B6, nullptr}, {0x1BC, nullptr}};
        const void* quads = bof3::CloneOriginal("MapCell_DrawQuads", bof3::addr::MapCell_DrawQuads, 0x1EF, quad_calls,
                                                sizeof quad_calls / sizeof quad_calls[0]);
        // 0x20 -> AreaMap_Elevation, to its clone; 0x93 -> Rand, to the stand-in;
        // then Gpu_SetPolyFT4, Gpu_SetShadeTex, Gte_RotTransPers4,
        // Gte_StoreDepthF4, Prim_SetTexture, Gfx_CommitPrim.
        const bof3::CloneCall rising_calls[] = {{0x20, elevation}, {0x86, nullptr},  {0x8E, nullptr},
                                                {0x93, reinterpret_cast<const void*>(&RandStandIn)},
                                                {0x184, nullptr},  {0x199, nullptr}, {0x1C2, nullptr}, {0x1D0, nullptr}};
        const void* rising = bof3::CloneOriginal("MapCell_DrawRising", bof3::addr::MapCell_DrawRising, 0x20F,
                                                 rising_calls, sizeof rising_calls / sizeof rising_calls[0]);
        g_rand = RandStandIn;
        SelfTestHandlers(reinterpret_cast<HandlerFn>(const_cast<void*>(quads)),
                         reinterpret_cast<HandlerFn>(const_cast<void*>(rising)));
        g_rand = Rand;
        // Live, from here on: every call in game runs a clone first. The
        // quads' clone is the fuzz's; the rising squares' needs Capcom's Rand,
        // which by then has its C runtime.
        if (LiveGteSize() > kLiveGte) bof3::Fatal("map_cells: the GTE's globals are 0x%X bytes", LiveGteSize());
        map_cells::g_quads_live = reinterpret_cast<HandlerFn>(const_cast<void*>(quads));
        const bof3::CloneCall live_rising_calls[] = {{0x20, elevation}, {0x86, nullptr},  {0x8E, nullptr},  {0x93, nullptr},
                                                     {0x184, nullptr},  {0x199, nullptr}, {0x1C2, nullptr}, {0x1D0, nullptr}};
        map_cells::g_rising_live = reinterpret_cast<HandlerFn>(const_cast<void*>(bof3::CloneOriginal(
            "MapCell_DrawRising", bof3::addr::MapCell_DrawRising, 0x20F, live_rising_calls,
            sizeof live_rising_calls / sizeof live_rising_calls[0])));
        const bof3::CloneCall band_rising_calls[] = {{0x20, elevation}, {0x86, nullptr},  {0x8E, nullptr},
                                                     {0x93, reinterpret_cast<const void*>(&wide_rand::DrawOnly)},
                                                     {0x184, nullptr},  {0x199, nullptr}, {0x1C2, nullptr}, {0x1D0, nullptr}};
        map_cells::g_rising_band_live = reinterpret_cast<HandlerFn>(const_cast<void*>(bof3::CloneOriginal(
            "MapCell_DrawRising", bof3::addr::MapCell_DrawRising, 0x20F, band_rising_calls,
            sizeof band_rising_calls / sizeof band_rising_calls[0])));
    }
}

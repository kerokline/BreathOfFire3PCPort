#include "game/map_cells.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "hook/detour.h"
#include "hook/log.h"

// Two of the map-cell handlers DrawLayer_Open hands a cell's records to, and
// the two functions under them that were still Capcom's: the condition test a
// record carries, and the ground's elevation at a cell
// (docs/sprite-draw-order.md section 16). Everything else these call is ours.

namespace {

template <class T> T Get(const unsigned char* p, unsigned offset) {
    T v;
    std::memcpy(&v, p + offset, sizeof v);
    return v;
}

constexpr unsigned kPrimBytes = 0x48;   // a POLY_FT4 as the port lays it out

int S8(unsigned v) { return static_cast<signed char>(v); }

// Where MapCell_DrawRising gets its random numbers: Capcom's Rand, except
// during the start-up fuzz, which runs before the exe's C runtime is set up
// (the launcher loads us into a suspended process) and so cannot call it.
int (__cdecl* g_rand)(void) = Rand;

// A vertex dword of MapCell_DrawQuads: the s8 at byte 3 and the s8 at byte 2,
// each doubled, about the cell's origin; the low word is the third coordinate.
// The sums are the original's 32-bit ones, stored as words.
//
// DIV-0023: the fourth word is 0, as DIV-0021 zeros the matrix product's
// padding (the owner's call). The original builds these on its stack and
// never writes that word, so Gte_LoadVertex and Gte_LoadVertices3 carry stale
// stack into the top halves of Gte_Vertices[1], [3] and [5], which no
// instruction in the image reads.
void QuadVertex(short* v, std::uint32_t dword, std::uint32_t x0, std::uint32_t y0) {
    v[0] = static_cast<short>(x0 + 2u * static_cast<std::uint32_t>(S8(dword >> 24)));
    v[1] = static_cast<short>(y0 + 2u * static_cast<std::uint32_t>(S8(dword >> 16)));
    v[2] = static_cast<short>(dword);
    v[3] = 0;
}

}  // namespace

// original 0x56FF00: a record's condition. The high byte of the u16 `code`
// is the kind, the low byte its operand:
//   0x00..0x1F   bit (operand) of Cond_Flags + 8 * kind
//   0x20..0x3F   the same bit of row kind & 0x1F, complemented
//   0xFF         (code & 1) XOR Cond_ByteFF - the whole byte, not 0 or 1
//   0xFE / 0xFD  operand == / != Cond_ByteFE / Cond_ByteFD
//   0xFC         (Game_Mode == 7 or Field_Request == 4) XOR (code & 1)
//   0xFB         ((Cond_AngleFB - 0x200) & 0xFFF) <= 0x800, XOR (code & 1)
//   0xFA         operand == Cond_ByteFA sign-extended, so never at 0x80 and up
//   anything else, 0x40 included, code & 1.
// Only the low 16 bits are read: the callers push ax with stale bits above.
// The value is al; two callers widen it with movsx, so all eight bits count.
extern "C" unsigned char __cdecl Area_TestCondition(unsigned long code) {
    const unsigned kind = code & 0xFF00u, operand = code & 0xFFu, low = code & 1u;
    switch (kind) {
        case 0x4000: return static_cast<unsigned char>(low);
        case 0xFF00: return static_cast<unsigned char>(low ^ Cond_ByteFF);
        case 0xFE00: return operand == Cond_ByteFE;
        case 0xFD00: return operand != Cond_ByteFD;
        case 0xFC00: return static_cast<unsigned char>(((Game_Mode == 7) | (Field_Request == 4)) ^ low);
        case 0xFB00: return static_cast<unsigned char>((((Cond_AngleFB - 0x200u) & 0xFFFu) <= 0x800u) ^ low);
        case 0xFA00: return static_cast<int>(operand) == static_cast<int>(Cond_ByteFA);
        default: break;
    }
    if (kind < 0x2000) return Flags_Test(Cond_Flags + 8u * (kind >> 8), code);
    if (kind < 0x4000) return Flags_Test(Cond_Flags + 8u * ((code >> 8) & 0x1Fu), code) == 0;
    return static_cast<unsigned char>(low);
}

// original 0x5720C0: the ground's elevation at (x, y), both 16.16. The cell
// is the high words' in a grid AreaMap_Header bytes 0 by 1; outside it the
// answer is 0 in the low half and the header's own top half above - kept, for
// a caller that reads eax. Inside, with h(dx, dy) a neighbour's height byte
// (at AreaMap_Header + AreaMap_HeightBase * 4, one per cell) times
// MapView_HeightScale and c[k] byte k of the cell's AreaMap_Corners dword
// onward, by quadrant - bit 15 of x, bit 15 of y:
//   neither   (c[1] + c[2] + 2 * h) << 4, in full precision
//   x only    max(c[1] + c[3] + 2h, c[4] + c[6] + 2h(1, 0)) << 4
//   y only    max(c[2] + c[3] + 2h, c[4w] + c[4w + 1] + 2h(0, 1)) << 4
//   both      the largest of c[3] + h, c[6] + h(1, 0), c[4w + 4] + h(1, 1),
//             c[4w + 1] + h(0, 1), << 5
// Kept as the original has it: past the first quadrant each height is its
// multiplication's low byte, and each corner-plus-height a byte sum, before
// they are widened as signed.
extern "C" long __cdecl AreaMap_Elevation(long x, long y) {
    std::uint32_t header;
    std::memcpy(&header, AreaMap_Header, 4);
    const auto cx = static_cast<short>(static_cast<std::uint32_t>(x) >> 16);
    const auto cy = static_cast<short>(static_cast<std::uint32_t>(y) >> 16);
    const unsigned width = header & 0xFFu;
    if (cy >= static_cast<short>((header >> 8) & 0xFFu) || cx >= static_cast<short>(width) || cy < 0 || cx < 0)
        return static_cast<long>(header & 0xFFFF0000u);
    const int cell = cx + cy * static_cast<int>(width);
    const unsigned char* h = AreaMap_Header + static_cast<unsigned>(AreaMap_HeightBase) * 4u + cell;
    const unsigned char* c = reinterpret_cast<const unsigned char*>(&AreaMap_Corners) + cell * 4;
    const unsigned scale = MapView_HeightScale;
    const bool half_x = (x & 0x8000) != 0, half_y = (y & 0x8000) != 0;
    if (!half_x && !half_y) return (S8(c[2]) + S8(h[0]) * static_cast<int>(scale) * 2 + S8(c[1])) * 16;
    // imul r/m8: only the product's low byte is kept.
    auto height = [&](unsigned at) { return static_cast<unsigned char>(h[at] * scale); };
    if (half_x != half_y) {
        const unsigned char here = height(0);
        unsigned char a, b, d, e;
        if (half_x) {
            a = static_cast<unsigned char>(c[1] + here);
            b = static_cast<unsigned char>(c[3] + here);
            const unsigned char next = height(1);
            d = static_cast<unsigned char>(c[4] + next);
            e = static_cast<unsigned char>(c[6] + next);
        } else {
            a = static_cast<unsigned char>(c[2] + here);
            b = static_cast<unsigned char>(c[3] + here);
            const unsigned char next = height(width);
            d = static_cast<unsigned char>(c[4 * width] + next);
            e = static_cast<unsigned char>(c[4 * width + 1] + next);
        }
        const int second = S8(e) + S8(d), first = S8(b) + S8(a);
        return (first > second ? first : second) * 16;
    }
    const int p0 = S8(static_cast<unsigned char>(c[3] + height(0)));
    const int p1 = S8(static_cast<unsigned char>(c[6] + height(1)));
    const int p2 = S8(static_cast<unsigned char>(c[4 * width + 4] + height(width + 1)));
    const int p3 = S8(static_cast<unsigned char>(c[4 * width + 1] + height(width)));
    if (p0 >= p3 && p0 >= p2 && p0 >= p1) return p0 * 32;
    if (p1 >= p3 && p1 >= p2) return p1 * 32;
    return (p2 < p3 ? p3 : p2) * 32;
}

// original 0x570020: a map cell's textured quads. Unless the record's
// condition (its low word) says no, or its byte +2 is 1, walks five-dword
// subrecords from +4 until a count that starts at 1 and adds 5 each equals
// byte +2 again: four vertices, then a Prim_SetTexture word. The cell's origin
// is ((b1 - 0x80) * 128, (b0 - 0x80) * 128). The first vertex alone goes
// through Gte_Rtps and to the primitive at Gfx_PacketNext; the quad is drawn
// only if that point is inside -100 < x < 420, -150 < y < 300 - a POLY_FT4
// committed to slot 4, or 7 with bit 30 of the word, when bit 14 is set, else
// to Draw_OtSlot, or 6 with bit 30. 12,183 calls a whole attract cycle.
//
// Kept as the original has it: a culled quad still leaves its first screen
// point at +8 of the packet; a byte +2 that is not 1 + 5n never stops the
// walk (read, not run: the fuzz only builds lengths of 1 + 5n).
static void DrawQuads(const unsigned char* record, unsigned b1, unsigned b0) {
    if (Area_TestCondition(Get<std::uint16_t>(record, 0)) == 0) return;
    const std::uint32_t x0 = (b1 - 0x80u) << 7, y0 = (b0 - 0x80u) << 7;
    if (record[2] == 1) return;
    unsigned count = 1;
    for (const unsigned char* sub = record + 4;; sub += 0x14) {
        alignas(4) short first[4];
        QuadVertex(first, Get<std::uint32_t>(sub, 0), x0, y0);
        Gte_LoadVertex(reinterpret_cast<const unsigned long*>(first));
        Gte_Rtps();
        unsigned char* const prim = Gfx_PacketNext;
        Gte_StoreScreenXY(reinterpret_cast<unsigned long*>(prim + 8));
        const float sx = Get<float>(prim, 8), sy = Get<float>(prim, 0xC);
        if (sx > -100.0f && sx < 420.0f && sy > -150.0f && sy < 300.0f) {
            Gpu_SetPolyFT4(prim);
            Gpu_SetShadeTex(prim, 0);
            alignas(4) short rest[12];
            for (unsigned k = 0; k < 3; ++k) QuadVertex(rest + 4 * k, Get<std::uint32_t>(sub, 4 + 4 * k), x0, y0);
            Gte_LoadVertices3(reinterpret_cast<const unsigned long*>(rest));
            const std::uint32_t texture = Get<std::uint32_t>(sub, 0x10);
            Prim_SetTexture(texture, prim, 1);
            Gte_Rtpt();
            unsigned slot;
            if (texture & 0x4000u) slot = texture & 0x40000000u ? 7u : 4u;
            else slot = texture & 0x40000000u ? 6u : Draw_OtSlot;
            Gfx_CommitPrim(slot, kPrimBytes);
            Gte_StoreScreenXY3(reinterpret_cast<unsigned long*>(prim + 0x18), reinterpret_cast<unsigned long*>(prim + 0x28),
                               reinterpret_cast<unsigned long*>(prim + 0x38));
            Gte_PrimDepths4_10(prim);
        }
        count += 5;
        if (count == record[2]) return;
    }
}

// original 0x570660: eight flat squares stacked on a map cell, redrawn each
// frame with one Rand each. The record is not read. For square i = 0..7, with
// f = Frame_Counter & 7:
//   half-side   0x20 + 0x20 * i + 4f + (Rand() & 3)
//   centre      ((b1 - 0x81) * 128 + s, (b0 - 0x80) * 128 - s), where the
//               sway s = |16 - ((Frame_Counter >> 8) & 0x1F)| * (f + 8i) / 10
//   third       -(AreaMap_Elevation(b1 - 1, b0) / 2) - 8 * (f + 8i)
//   texture     0xBA009124, shade ((0x3F - 8i - f) / 4) in bits 19..22
// The corners go through Prim_VertexScratch, whose fourth words are left as
// they were, into Gte_RotTransPers4; each square is committed to
// Draw_OtSlot. 5,098 calls a whole attract cycle.
//
// Kept as the original has it: the arithmetic is 32-bit on whatever b1 and b0
// hold and each coordinate is stored as its low word; the elevation is halved
// as an s16, so the header's top half that an off-grid answer carries is lost.
static void DrawRising(const unsigned char* record, unsigned b1, unsigned b0) {
    static_cast<void>(record);
    const long elevation = AreaMap_Elevation(static_cast<long>((b1 << 16) - 0x10000u), static_cast<long>(b0 << 16));
    const int base = -(static_cast<short>(elevation) / 2);
    const unsigned phase = Frame_Counter & 7u;
    const std::uint32_t x0 = (b1 - 0x81u) << 7, y0 = (b0 - 0x80u) << 7;
    short* const v = Prim_VertexScratch;
    for (unsigned i = 0; i < 8; ++i) {
        unsigned char* const prim = Gfx_PacketNext;
        Gpu_SetPolyFT4(prim);
        Gpu_SetShadeTex(prim, 0);
        const std::uint32_t half = (static_cast<std::uint32_t>(g_rand()) & 3u) + 4u * phase + 0x20u + 0x20u * i;
        int swing = 16 - static_cast<int>((Frame_Counter >> 8) & 0x1Fu);
        if (swing < 0) swing = -swing;
        const auto sway = static_cast<std::uint32_t>(swing * static_cast<int>(phase + 8u * i) / 10);
        const auto left = static_cast<short>(x0 + sway - half), right = static_cast<short>(x0 + sway + half);
        const auto top = static_cast<short>(y0 - sway - half), bottom = static_cast<short>(y0 - sway + half);
        const auto depth = static_cast<short>(base - 8 * static_cast<int>(phase + 8u * i));
        v[0] = left;   v[1] = top;     v[2] = depth;
        v[4] = right;  v[5] = top;     v[6] = depth;
        v[8] = left;   v[9] = bottom;  v[10] = depth;
        v[12] = right; v[13] = bottom; v[14] = depth;
        long unused;
        Gte_RotTransPers4(v + 4, v + 8, v + 12, v, reinterpret_cast<float*>(prim + 0x18), reinterpret_cast<float*>(prim + 0x28),
                          reinterpret_cast<float*>(prim + 0x38), reinterpret_cast<float*>(prim + 8), &unused);
        Gte_StoreDepthF4(reinterpret_cast<float*>(prim + 0x20), reinterpret_cast<float*>(prim + 0x30),
                         reinterpret_cast<float*>(prim + 0x40), reinterpret_cast<float*>(prim + 0x10));
        const unsigned shade = static_cast<unsigned>(static_cast<int>(0x3F - 8 * i - phase) / 4);
        Prim_SetTexture((shade << 19) | 0xBA009124u, prim, 1);
        Gfx_CommitPrim(Draw_OtSlot, kPrimBytes);
    }
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
// Rand's seed (the C runtime's, at Crt_GetPtd() + 0x14 - there by the time the
// game draws). Ours is what the game keeps. DIV-0023's pad words are taken as
// the fuzz takes them.

HandlerFn g_quads_live = nullptr, g_rising_live = nullptr;
constexpr unsigned kLiveGte = 0x380, kLiveWindow = 0x1000;

struct LiveState {
    unsigned char gte[kLiveGte];
    unsigned char scratch[0x20];
    unsigned char* packet_next;
    unsigned long* ot[8];
    unsigned long tails[8];
    std::uint32_t seed;
    unsigned char window[kLiveWindow];
};
struct LiveCounts {
    unsigned calls, mismatches, padding, committed;
} g_live[2];

unsigned char* LiveGte() { return reinterpret_cast<unsigned char*>(&Gte_RampFar); }
unsigned LiveGteSize() {
    return static_cast<unsigned>(reinterpret_cast<unsigned char*>(&Gte_Depth) + 4 - LiveGte());
}

// The tails are read through `tails_at` - the input's pointers - on every
// capture, so that each side's links are seen where they were made.
void LiveCapture(LiveState& s, unsigned char* window_at, unsigned window, std::uint32_t* seed,
                 unsigned long* const* tails_at) {
    std::memset(&s, 0, sizeof s);
    std::memcpy(s.gte, LiveGte(), LiveGteSize());
    std::memcpy(s.scratch, Prim_VertexScratch, sizeof s.scratch);
    s.packet_next = Gfx_PacketNext;
    for (unsigned k = 0; k < 8; ++k) {
        s.ot[k] = Gfx_OtPointers[k];
        s.tails[k] = tails_at[k] ? *tails_at[k] : 0;
    }
    s.seed = *seed;
    std::memcpy(s.window, window_at, window);
}
void LiveApply(const LiveState& s, unsigned char* window_at, unsigned window, std::uint32_t* seed) {
    std::memcpy(LiveGte(), s.gte, LiveGteSize());
    std::memcpy(Prim_VertexScratch, s.scratch, sizeof s.scratch);
    Gfx_PacketNext = s.packet_next;
    for (unsigned k = 0; k < 8; ++k) Gfx_OtPointers[k] = s.ot[k];
    for (unsigned k = 0; k < 8; ++k)
        if (s.ot[k]) *s.ot[k] = s.tails[k];
    *seed = s.seed;
    std::memcpy(window_at, s.window, window);
}

void Live(unsigned which, HandlerFn theirs, void (*ours)(const unsigned char*, unsigned, unsigned),
          const unsigned char* record, unsigned b1, unsigned b0) {
    static LiveState input, their_out, our_out;
    static const char* const kName[2] = {"MapCell_DrawQuads", "MapCell_DrawRising"};
    unsigned char* const window_at = Gfx_PacketNext;
    const auto room = static_cast<unsigned>(Gfx_PacketPools + 0x20000 - window_at);
    const unsigned window = room < kLiveWindow ? room : kLiveWindow;
    auto* seed = reinterpret_cast<std::uint32_t*>(Crt_GetPtd() + 0x14);
    unsigned long* tails_at[8];
    for (unsigned k = 0; k < 8; ++k) tails_at[k] = Gfx_OtPointers[k];

    LiveCapture(input, window_at, window, seed, tails_at);
    theirs(record, b1, b0);
    LiveCapture(their_out, window_at, window, seed, tails_at);
    LiveApply(input, window_at, window, seed);
    ours(record, b1, b0);
    LiveCapture(our_out, window_at, window, seed, tails_at);

    LiveCounts& c = g_live[which];
    ++c.calls;
    c.committed += static_cast<unsigned>(our_out.packet_next - input.packet_next) / kPrimBytes;
    const unsigned pad_at = static_cast<unsigned>(reinterpret_cast<unsigned char*>(Gte_Vertices) - LiveGte());
    for (unsigned slot = 1; slot < 6; slot += 2) {
        unsigned char* our_pad = our_out.gte + pad_at + 4 * slot + 2;
        const unsigned char* their_pad = their_out.gte + pad_at + 4 * slot + 2;
        if (std::memcmp(our_pad, their_pad, 2) == 0 || our_pad[0] != 0 || our_pad[1] != 0) continue;
        ++c.padding;
        std::memcpy(our_pad, their_pad, 2);
    }
    if (std::memcmp(&their_out, &our_out, sizeof our_out) != 0 && ++c.mismatches <= 8) {
        const auto* a = reinterpret_cast<const unsigned char*>(&their_out);
        const auto* b = reinterpret_cast<const unsigned char*>(&our_out);
        unsigned first = 0;
        while (a[first] == b[first]) ++first;
        bof3::Log("shadow      %s live MISMATCH: call %u, b1 %X, b0 %X, first difference at state +0x%X (%02X / %02X)",
                  kName[which], c.calls, b1, b0, first, a[first], b[first]);
    }
    if (c.calls == 1 || c.calls % 1024 == 0)
        bof3::Log("shadow      %s live: %u calls, %u primitives committed, %u MISMATCHES; a vertex pad word "
                  "differed %u times (DIV-0023, not counted)",
                  kName[which], c.calls, c.committed, c.mismatches, c.padding);
}

}  // namespace

extern "C" void __cdecl MapCell_DrawQuads(const unsigned char* record, unsigned b1, unsigned b0) {
    if (g_quads_live) Live(0, g_quads_live, DrawQuads, record, b1, b0);
    else DrawQuads(record, b1, b0);
}
extern "C" void __cdecl MapCell_DrawRising(const unsigned char* record, unsigned b1, unsigned b0) {
    if (g_rising_live) Live(1, g_rising_live, DrawRising, record, b1, b0);
    else DrawRising(record, b1, b0);
}

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
        g_quads_live = reinterpret_cast<HandlerFn>(const_cast<void*>(quads));
        const bof3::CloneCall live_rising_calls[] = {{0x20, elevation}, {0x86, nullptr},  {0x8E, nullptr},  {0x93, nullptr},
                                                     {0x184, nullptr},  {0x199, nullptr}, {0x1C2, nullptr}, {0x1D0, nullptr}};
        g_rising_live = reinterpret_cast<HandlerFn>(const_cast<void*>(bof3::CloneOriginal(
            "MapCell_DrawRising", bof3::addr::MapCell_DrawRising, 0x20F, live_rising_calls,
            sizeof live_rising_calls / sizeof live_rising_calls[0])));
    }
    BOF3_INJECT(Area_TestCondition);
    BOF3_INJECT(AreaMap_Elevation);
    BOF3_INJECT(MapCell_DrawQuads);
    BOF3_INJECT(MapCell_DrawRising);
}

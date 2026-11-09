// Moved here by tools/move_funcs.py from effect_2b.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_3b.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_2b.h"
#include "game/effect_2b_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_2b::at;

}  // namespace

namespace {

using move_script::At;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

using U = std::uint32_t;

using Handler = void (__cdecl*)();

unsigned char* Cur() { return Sprite_Current; }

U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }

void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }

U Key(const void* p) { return Low32(p); }

U AddressOf(const void* p) { return Key(p); }

void StoreFloat(unsigned char* at, double v) {
    const auto f = static_cast<float>(v);
    std::memcpy(at, &f, sizeof f);
}

// imul of two dwords (the product's low 32 bits), then sar: how every point
// here is placed round its centre (>> 12 for the fixed-point trig, >> 4 for
// kind 0x3B's spirals).
std::int32_t MulSar(int t, U v, unsigned shift) { return static_cast<std::int32_t>(static_cast<U>(t) * v) >> shift; }

// jmp / call [table + 4 * index]: the table's `entries` handlers, read in place
// (the fuzz swaps the cells for recorders); a Fatal past them, where the
// original goes through the dword after - the next kind's table or data.
Handler Entry(U table, unsigned index, unsigned entries, const char* who) {
    if (index >= entries)
        bof3::Fatal("%s: index %u past the %u entries of the table at 0x%X (the original jumps through 0x%X)", who,
                    index, entries, (unsigned)table, (unsigned)(table + 4u * index));
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(At(table + 4u * index))));
}

// Gpu_GetTPage(0, abr, x, y) and a draw mode of it at Gfx_PacketNext (dfe 0,
// dtd `dtd`); the fifth zero the original pushed for Gpu_GetTPage and left on
// the stack is the mode's texture window.
void Mode(unsigned abr, int x, int y, int dtd) {
    const unsigned tpage = SH_CALL(Gpu_GetTPage)(0, abr, x, y) & 0xFFFFu;
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, dtd, tpage, 0);
}

// A spiral's step: Sin(0x40) * the length so far / Cos(0x80), the originals'
// cdq / idiv (called afresh for each of the four uses). A divisor of 0, or the
// one quotient idiv cannot hold, is the original's divide fault: ours aborts.
std::int32_t Spread(U length) {
    const int s = SH_CALL(Math_Sin)(0x40);
    const auto num = static_cast<std::int32_t>(static_cast<U>(s) * length);
    const int c = SH_CALL(Math_Cos)(0x80);
    if (c == 0) bof3::Fatal("EffectKind3B_DrawSpiral: Math_Cos(0x80) answered 0 - the original's idiv faults");
    if (num == INT32_MIN && c == -1)
        bof3::Fatal("EffectKind3B_DrawSpiral: 0x80000000 / -1 - the original's idiv faults");
    return num / c;
}

// Kinds 0x3B's and 0x3D's glow: the map camera loaded, `point` projected,
// `size` scaled at its depth (EffectGte_ProjectSize: in = v, out = v + 2 - the
// original's second in word is its stack's leftover, ours 0; out's second word
// is not read); the radius that plus (Frame_Counter & 1) << `parity`, 16 bits;
// 32 semi-transparent G3 triangles fanned round the screen point at steps of
// 0x80 (the first angle whole, the second & 0xFFF), the centre `centre`'s
// shade and the two rim points `rim`'s, the depth at every vertex; each
// committed at slot 1 (0x34 bytes).
void GlowFan(const long* point, U size, U centre, U rim, unsigned parity) {
    SH_CALL(EffectGte_LoadMapCamera)();
    float p[3];
    SH_CALL(EffectGte_ProjectPoint)(point, p);
    short v[4] = {static_cast<short>(size), 0, 0, 0};
    SH_CALL(EffectGte_ProjectSize)(point, v, v + 2);
    const auto radius = static_cast<int>(static_cast<short>((Frame_Counter & 1u) << parity) + v[2]);
    const U r = static_cast<U>(static_cast<int>(static_cast<short>(radius)));
    const auto rim_shade = static_cast<unsigned char>(rim);
    U a1 = 0, next = 0;
    do {
        unsigned char* const prim = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG3)(prim);
        SH_CALL(Gpu_SetSemiTrans)(prim, 1);
        std::memcpy(prim + 8, &p[0], 4);
        next += 0x80;
        const U a2 = next & 0xFFFu;
        std::memcpy(prim + 0xC, &p[1], 4);
        StoreFloat(prim + 0x18, static_cast<double>(MulSar(SH_CALL(Math_Cos)(static_cast<int>(a1)), r, 12)) + static_cast<double>(p[0]));
        StoreFloat(prim + 0x1C, static_cast<double>(MulSar(SH_CALL(Math_Sin)(static_cast<int>(a1)), r, 12)) + static_cast<double>(p[1]));
        StoreFloat(prim + 0x28, static_cast<double>(MulSar(SH_CALL(Math_Cos)(static_cast<int>(a2)), r, 12)) + static_cast<double>(p[0]));
        StoreFloat(prim + 0x2C, static_cast<double>(MulSar(SH_CALL(Math_Sin)(static_cast<int>(a2)), r, 12)) + static_cast<double>(p[1]));
        std::memcpy(prim + 0x30, &p[2], 4);
        std::memcpy(prim + 0x20, &p[2], 4);
        std::memcpy(prim + 0x10, &p[2], 4);
        prim[4] = prim[5] = prim[6] = static_cast<unsigned char>(centre);
        prim[0x14] = prim[0x15] = prim[0x16] = rim_shade;
        prim[0x24] = prim[0x25] = prim[0x26] = rim_shade;
        SH_CALL(Gfx_CommitPrim)(1, 0x34);
        a1 += 0x80;
    } while ((next & 0xFFFFu) < 0x1000u);
}

}  // namespace

// The glow's draw mode (the page at (0x380, 0x100), abr 1, dtd 1) committed at
// slot 1, and the glow at a copy of the record's point, its size `byte` << 2,
// grey 0x80 at the centre and black at the rim.
static void Kind3BGlow(unsigned offset) {
    Mode(1, 0x380, 0x100, 1);
    SH_CALL(Gfx_CommitPrim)(1, 0xC);
    const unsigned char* const s = Cur();
    auto* const so = rv::AsConst<records::EffectObject>(s);
    const long point[3] = {so->x, so->z, so->height};
    SH_CALL(EffectKind3B_DrawGlow)(point, static_cast<U>(s[offset]) << 2, 0x80, 0);
}

// original 0x4741E0 (Effect_KindHandlers[0x3B], hidden in 0x474070's recorded
// extent): a tail jump through EffectKind3B_States 0x654448 by +1.
extern "C" void __cdecl EffectKind3B_Run(void) {
    Entry(AddressOf(EffectKind3B_States), Cur()[1], 4, "EffectKind3B_Run")();
}

// original 0x474200 (EffectKind3B_States[0]): the three spirals set up
// (EffectKind3B_InitSpirals); +9 0x80, +0xA 0, +1 up; Sound_PlayEffect(0x200)
// and (0x20A).
extern "C" void __cdecl EffectKind3B_Start(void) {
    SH_CALL(EffectKind3B_InitSpirals)();
    Cur()[9] = 0x80;
    Cur()[0xA] = 0;
    unsigned char* const s = Cur();
    s[1] = static_cast<unsigned char>(s[1] + 1);
    SH_CALL(Sound_PlayEffect)(0x200);
    SH_CALL(Sound_PlayEffect)(0x20A);
}

// original 0x474240 (EffectKind3B_States[1]): the glow at +0xA's size; the map
// camera loaded; each spiral drawn (EffectKind3B_DrawSpiral), its shade word
// +0x14 up 4 and the record's +0xA up 1 - past 0x20, +0xA 0x20 and the shade
// 0x80 -, its angle +0x10 up 0x40, radius +0x12 down 0x18, height +8 down 7.0;
// +9 down 1 (the record last read in the loop) - at 0, +9 0x20 and +1 up.
extern "C" void __cdecl EffectKind3B_Rise(void) {
    Kind3BGlow(0xA);
    SH_CALL(EffectGte_LoadMapCamera)();
    unsigned char* s = nullptr;
    for (U i = 0; i < at::kSpiralCount; ++i) {
        unsigned char* const r = At(::bof3::Img(EffectKind30_Shards_addr) + at::kSpiralStride * i);
        SH_CALL(EffectKind3B_DrawSpiral)(r);
        s = Cur();
        SetWord(r + 0x14, Word(r + 0x14) + 4u);
        s[0xA] = static_cast<unsigned char>(s[0xA] + 1);
        s = Cur();
        if (s[0xA] > 0x20) {
            s[0xA] = 0x20;
            s = Cur();
            SetWord(r + 0x14, 0x80);
        }
        const U height = UL(r + 8);
        SetWord(r + 0x10, Word(r + 0x10) + 0x40u);
        SetWord(r + 0x12, Word(r + 0x12) - 0x18u);
        SetUL(r + 8, height + 0xFFF90000u);
    }
    s[9] = static_cast<unsigned char>(s[9] - 1);
    s = Cur();
    if (s[9] == 0) {
        s[9] = 0x20;
        s = Cur();
        s[1] = static_cast<unsigned char>(s[1] + 1);
    }
}

// original 0x474340 (EffectKind3B_States[2]): the glow at +9's size; the map
// camera loaded; each spiral drawn, its angle up 0x40 and shade down 8; +9
// down 1 - at 0, +1 up (Effect_StateRelease).
extern "C" void __cdecl EffectKind3B_Fade(void) {
    Kind3BGlow(9);
    SH_CALL(EffectGte_LoadMapCamera)();
    for (U i = 0; i < at::kSpiralCount; ++i) {
        unsigned char* const r = At(::bof3::Img(EffectKind30_Shards_addr) + at::kSpiralStride * i);
        SH_CALL(EffectKind3B_DrawSpiral)(r);
        SetWord(r + 0x10, Word(r + 0x10) + 0x40u);
        SetWord(r + 0x14, Word(r + 0x14) - 8u);
    }
    unsigned char* s = Cur();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    s = Cur();
    if (s[9] == 0) s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x474410: a spiral of 32 semi-transparent G4 quads round the
// record's centre (+0, +4; the height +8). The first point at the angle +0x10
// and radius +0x12 (s16 each; (trig * radius) >> 4) projected into both edges;
// then for each quad the angle down 0x20 and the radius up 0x18 (16 bits), the
// next point, the direction from the last point (z 0) normalised
// (Gte_VectorNormal, in place) and its length (0x5A7A90 of the answer) added
// to a running length L; the two edge points the next point plus and minus
// its perpendicular times Sin(0x40) * L / Cos(0x80) >> 12, projected; the
// quad from the last edges to the new, its first two corners the shade word
// +0x14 clamped to 0..0xFF and its last two that less 4 (the shade kept down 4
// a quad), committed at slot 1 (0x44 bytes).
extern "C" void __cdecl EffectKind3B_DrawSpiral(unsigned char* spiral) {
    U length = 0;
    U angle = Word(spiral + 0x10);
    U radius = Word(spiral + 0x12);
    U shade = Word(spiral + 0x14);
    long q[3];
    float v[3], w[3];
    int c = SH_CALL(Math_Cos)(static_cast<short>(angle));
    q[0] = static_cast<long>(static_cast<U>(MulSar(c, static_cast<U>(static_cast<int>(static_cast<short>(radius))), 4)) +
                             UL(spiral + 0));
    int n = SH_CALL(Math_Sin)(static_cast<short>(angle));
    q[1] = static_cast<long>(static_cast<U>(MulSar(n, static_cast<U>(static_cast<int>(static_cast<short>(radius))), 4)) +
                             UL(spiral + 4));
    q[2] = static_cast<long>(UL(spiral + 8));
    SH_CALL(EffectGte_ProjectPoint)(q, v);
    std::memcpy(w, v, sizeof w);
    for (unsigned k = 0; k < 32; ++k) {
        unsigned char* const prim = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG4)(prim);
        SH_CALL(Gpu_SetSemiTrans)(prim, 1);
        std::memcpy(prim + 8, v, 12);
        std::memcpy(prim + 0x18, w, 12);
        if (static_cast<short>(shade) < 0)
            shade = 0;
        else if (static_cast<short>(shade) > 0xFF)
            shade = 0xFF;
        prim[4] = prim[5] = prim[6] = static_cast<unsigned char>(shade);
        prim[0x14] = prim[0x15] = prim[0x16] = static_cast<unsigned char>(shade);
        angle -= 0x20;
        radius += 0x18;
        const U last_x = static_cast<U>(q[0]), last_z = static_cast<U>(q[1]);
        shade -= 4;
        const U rr = static_cast<U>(static_cast<int>(static_cast<short>(radius)));
        c = SH_CALL(Math_Cos)(static_cast<short>(angle));
        q[0] = static_cast<long>(static_cast<U>(MulSar(c, rr, 4)) + UL(spiral + 0));
        n = SH_CALL(Math_Sin)(static_cast<short>(angle));
        q[1] = static_cast<long>(static_cast<U>(MulSar(n, rr, 4)) + UL(spiral + 4));
        q[2] = static_cast<long>(UL(spiral + 8));
        long d[3] = {static_cast<long>(static_cast<U>(q[0]) - last_x), static_cast<long>(static_cast<U>(q[1]) - last_z), 0};
        const long squared = SH_CALL(Gte_VectorNormal)(d, d);
        length += SH_AT(U (__cdecl*)(long), at::kSqrt.v)(squared);
        const long perp[3] = {static_cast<long>(0u - static_cast<U>(d[1])), d[0], 0};
        long edge[3];
        edge[0] = static_cast<long>(static_cast<U>(q[0]) + static_cast<U>(MulSar(Spread(length), static_cast<U>(perp[0]), 12)));
        edge[1] = static_cast<long>(static_cast<U>(q[1]) + static_cast<U>(MulSar(Spread(length), static_cast<U>(perp[1]), 12)));
        edge[2] = q[2];
        SH_CALL(EffectGte_ProjectPoint)(edge, v);
        edge[0] = static_cast<long>(static_cast<U>(q[0]) - static_cast<U>(MulSar(Spread(length), static_cast<U>(perp[0]), 12)));
        edge[1] = static_cast<long>(static_cast<U>(q[1]) - static_cast<U>(MulSar(Spread(length), static_cast<U>(perp[1]), 12)));
        edge[2] = q[2];
        SH_CALL(EffectGte_ProjectPoint)(edge, w);
        std::memcpy(prim + 0x28, v, 12);
        std::memcpy(prim + 0x38, w, 12);
        if (static_cast<short>(shade) < 0)
            shade = 0;
        else if (static_cast<short>(shade) > 0xFF)
            shade = 0xFF;
        prim[0x24] = prim[0x25] = prim[0x26] = static_cast<unsigned char>(shade);
        prim[0x34] = prim[0x35] = prim[0x36] = static_cast<unsigned char>(shade);
        SH_CALL(Gfx_CommitPrim)(1, 0x44);
    }
}

// original 0x474770: the three spirals round the record's point (read once):
// centre +0x34 / +0x38, height +0x3C + 0x380 (<< 16), radius word 0xC00,
// shade 0, angle 0x1000 * i / 3.
extern "C" void __cdecl EffectKind3B_InitSpirals(void) {
    const unsigned char* const s = Cur();
    auto* const so = rv::AsConst<records::EffectObject>(s);
    for (U i = 0; i < at::kSpiralCount; ++i) {
        unsigned char* const r = At(::bof3::Img(EffectKind30_Shards_addr) + at::kSpiralStride * i);
        SetUL(r + 0, static_cast<std::uint32_t>(so->x));
        SetUL(r + 4, static_cast<std::uint32_t>(so->z));
        SetWord(r + 0x12, 0xC00);
        SetUL(r + 8, static_cast<std::uint32_t>(so->height) + 0x3800000u);
        SetWord(r + 0x14, 0);
        SetWord(r + 0x10, 0x1000u * i / 3u);
    }
}

// original 0x4747D0: the glow (GlowFan): `point` projected, `size` (its low
// word) scaled at its depth, the radius that plus the frame's parity; `centre`
// and `rim` the shades (their low bytes).
extern "C" void __cdecl EffectKind3B_DrawGlow(const long* point, unsigned size, unsigned centre, unsigned rim) {
    GlowFan(point, size, centre, rim, 0);
}

void EffectKind3b_Inject() {
    BOF3_INJECT(EffectKind3B_Run);
    BOF3_INJECT(EffectKind3B_Start);
    BOF3_INJECT(EffectKind3B_Rise);
    BOF3_INJECT(EffectKind3B_Fade);
    BOF3_INJECT(EffectKind3B_DrawSpiral);
    BOF3_INJECT(EffectKind3B_InitSpirals);
    BOF3_INJECT(EffectKind3B_DrawGlow);
}

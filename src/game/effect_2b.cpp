// Round thirteen group E2B (docs/effect_2b.md): analysis/round13_cut.tsv's 51
// rows for E2B and the one start inside them that no list has (0x473F10, kind
// 0x38's tile, the tail of 0x473EC0 that 0x473E30 and 0x473E90 jump to), each
// read to its last instruction with capstone (2026-09-29) and fuzzed through
// the scenario harness's effect mode (scenario_harness.h,
// docs/scenario_harness.md section 8).
//
//   EffectKind2F_Run .. _Spin          0x4731A0 .. 0x473240  Effect_KindHandlers[0x2F], EffectKind2F_States 0, 1
//   EffectKind2F_StepTrail .. _InitTrail  0x4732D0 .. 0x473460  a trail's head moved, drawn, set up (E8)
//   EffectKind33_Run .. _Grow          0x4734F0 .. 0x473550  Effect_KindHandlers[0x33], EffectKind33_States 0, 1
//   EffectKind33_DrawDisc, _DrawQuarter  0x4735B0, 0x473600  64 flat triangles round a point (E8)
//   EffectKind35_Run .. _FreeModel     0x4737A0 .. 0x473800  Effect_KindHandlers[0x35], EffectKind35_States 0..2
//   EffectKind35_ShardsInit .. _TurnPiece  0x473810 .. 0x473CE0  kind 0x1E's shards and pieces, sixteen and 55
//   EffectKind38_Run .. _DrawTint      0x473DD0 .. 0x473F10  Effect_KindHandlers[0x38], EffectKind38_States 0..3, the tile
//   EffectKind39_Run .. _DrawDisc      0x473FA0 .. 0x474070  Effect_KindHandlers[0x39], EffectKind39_States 0..2, a fan
//   EffectKind3B_Run .. _DrawGlow      0x4741E0 .. 0x4747D0  Effect_KindHandlers[0x3B], EffectKind3B_States 0..2, spirals
//   EffectKind3D_Run .. _DrawRing      0x474940 .. 0x474D20  Effect_KindHandlers[0x3D], EffectKind3D_States 0..3, rings
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies. Sprite_Current
// is read again wherever the original reads [0x937F88] again after a call, and
// kept where it keeps it in a register. No divergence: each is a faithful
// replacement. Where the original would jump through a state table past its
// code or divide by zero, ours aborts with a message (docs/effect_2b.md
// section 7).
#include "game/effect_2b.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_2b_callees.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_2b::at;
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
short SW(const unsigned char* p) { return static_cast<short>(Word(p)); }
U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
U AddressOf(const void* p) { return Key(p); }
void StoreFloat(unsigned char* at, double v) {
    const auto f = static_cast<float>(v);
    std::memcpy(at, &f, sizeof f);
}
void Copy12(unsigned char* to, const void* from) { std::memcpy(to, from, 12); }

// imul of two dwords (the product's low 32 bits), then sar: how every point
// here is placed round its centre (>> 12 for the fixed-point trig, >> 4 for
// kind 0x3B's spirals).
std::int32_t MulSar(int t, U v, unsigned shift) { return static_cast<std::int32_t>(static_cast<U>(t) * v) >> shift; }

// Kind 0x35's shard cursor, EffectKind35_ShardCursor 0x676108.
unsigned char* Cursor() { return EffectKind35_ShardCursor; }
void SetCursor(U v) { EffectKind35_ShardCursor = At(v); }

// libgte's MATRIX: nine s16 of rotation, two bytes of padding, a translation
// of three s32.
struct Matrix {
    short m[9];
    short pad;
    long t[3];
};
static_assert(sizeof(Matrix) == 0x20, "a MATRIX is 32 bytes");

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

// The byte a G2 line's end and a spiral's quad take: a word clamped to 0..0xFF
// as the originals clamp it (a 16-bit compare).
unsigned char ClampUnsigned(U v) { return static_cast<unsigned char>((v & 0xFFFFu) > 0xFFu ? 0xFFu : v); }

// Kind 0x2F's head: the point at the record's angle word (zero-extended) and
// radius +0x10 round its centre (+0, +4), the height +8 << 8; the map camera
// loaded; projected into the first of the trail's points (+0x18).
void TrailHead(unsigned char* r) {
    long point[3];
    const int c = SH_CALL(Math_Cos)(static_cast<int>(Word(r + 0x14)));
    point[0] = static_cast<long>(static_cast<U>(MulSar(c, UL(r + 0x10), 12)) + UL(r + 0));
    const int n = SH_CALL(Math_Sin)(static_cast<int>(Word(r + 0x14)));
    point[1] = static_cast<long>(static_cast<U>(MulSar(n, UL(r + 0x10), 12)) + UL(r + 4));
    point[2] = static_cast<long>(UL(r + 8) << 8);
    SH_CALL(EffectGte_LoadMapCamera)();
    SH_CALL(EffectGte_ProjectPoint)(point, reinterpret_cast<float*>(r + 0x18));
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

// ===========================================================================
// Kind 0x2F: Effect_KindHandlers[0x2F], EffectKind2F_States by +1
// ===========================================================================

// original 0x4731A0 (Effect_KindHandlers[0x2F], hidden in E2A's 0x473100's
// recorded extent): a tail jump through EffectKind2F_States 0x6543D8 by +1.
extern "C" void __cdecl EffectKind2F_Run(void) {
    Entry(AddressOf(EffectKind2F_States), Cur()[1], 2, "EffectKind2F_Run")();
}

// original 0x4731C0 (EffectKind2F_States[0]): the three trail records at
// EffectKind30_Shards (0x198 apart): centre (0x20000, 0xC0000), height
// 0xB0000, radius 0x20000, the angle word 0x1000 * i / 3; each trail set up
// (EffectKind2F_InitTrail); Sound_PlayEffect(0x206); +9 0x40, +1 up.
extern "C" void __cdecl EffectKind2F_Start(void) {
    for (U i = 0; i < at::kTrailCount; ++i) {
        unsigned char* const r = At(at::kTrails + at::kTrailStride * i);
        SetUL(r + 0, 0x20000);
        SetUL(r + 4, 0xC0000);
        SetUL(r + 8, 0xB0000);
        SetUL(r + 0x10, 0x20000);
        SetWord(r + 0x14, 0x1000u * i / 3u);
        SH_CALL(EffectKind2F_InitTrail)(r);
    }
    SH_CALL(Sound_PlayEffect)(0x206);
    Cur()[9] = 0x40;
    unsigned char* const s = Cur();
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x473240 (EffectKind2F_States[1]): each trail four times: the
// height down 0x400, the radius down 0x200, the angle word up 0x40, the head
// moved (EffectKind2F_StepTrail); then drawn (EffectKind2F_DrawTrail). +9 down
// 1 - at 0, Effect_Release.
extern "C" void __cdecl EffectKind2F_Spin(void) {
    for (U i = 0; i < at::kTrailCount; ++i) {
        unsigned char* const r = At(at::kTrails + at::kTrailStride * i);
        for (unsigned k = 0; k < 4; ++k) {
            const U height = UL(r + 8), radius = UL(r + 0x10);
            SetWord(r + 0x14, Word(r + 0x14) + 0x40u);
            SetUL(r + 8, height - 0x400u);
            SetUL(r + 0x10, radius - 0x200u);
            SH_CALL(EffectKind2F_StepTrail)(r);
        }
        SH_CALL(EffectKind2F_DrawTrail)(r);
    }
    unsigned char* const s = Cur();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    if (Cur()[9] == 0) SH_CALL(Effect_Release)();
}

// original 0x4732D0: a trail's 32 points (three floats from +0x18) moved down
// one, from the last (dword by dword); the head projected into the first
// (TrailHead).
extern "C" void __cdecl EffectKind2F_StepTrail(unsigned char* trail) {
    for (U k = 31; k >= 1; --k) {
        unsigned char* const to = trail + 0x18 + 0xC * k;
        const unsigned char* const from = to - 0xC;
        SetUL(to, UL(from));
        SetUL(to + 4, UL(from + 4));
        SetUL(to + 8, UL(from + 8));
    }
    TrailHead(trail);
}

// original 0x473360: a draw mode (the page at (0x380, 0x100), abr 1, dtd 0)
// committed at slot 1; then 31 semi-transparent G2 lines at Gfx_PacketNext,
// point j to point j + 1 (floats as they are), red from 0xFF (the first end
// (0x1F00 - 0x100 j + 0x100) / 32, the second (0x1F00 - 0x100 j) / 32, each
// clamped to 0xFF), green and blue 0; each committed at slot 1 (0x24 bytes).
extern "C" void __cdecl EffectKind2F_DrawTrail(unsigned char* trail) {
    Mode(1, 0x380, 0x100, 0);
    SH_CALL(Gfx_CommitPrim)(1, 0xC);
    std::int32_t shade = 0x1F00;
    for (U j = 0; j < 31; ++j) {
        unsigned char* const prim = Gfx_PacketNext;
        SH_CALL(Gpu_SetLineG2)(prim);
        SH_CALL(Gpu_SetSemiTrans)(prim, 1);
        const unsigned char* const e = trail + 0x18 + 0xC * j;
        Copy12(prim + 8, e);
        Copy12(prim + 0x18, e + 0xC);
        prim[4] = ClampUnsigned(static_cast<U>((shade + 0x100) / 32));
        prim[5] = 0;
        prim[6] = 0;
        prim[0x14] = ClampUnsigned(static_cast<U>(shade / 32));
        prim[0x15] = 0;
        prim[0x16] = 0;
        SH_CALL(Gfx_CommitPrim)(1, 0x24);
        shade -= 0x100;
    }
}

// original 0x473460: the head projected into the first point (TrailHead), then
// copied into the other 31.
extern "C" void __cdecl EffectKind2F_InitTrail(unsigned char* trail) {
    TrailHead(trail);
    for (U k = 1; k < 32; ++k) {
        unsigned char* const to = trail + 0x18 + 0xC * k;
        SetUL(to, UL(trail + 0x18));
        SetUL(to + 4, UL(trail + 0x1C));
        SetUL(to + 8, UL(trail + 0x20));
    }
}

// ===========================================================================
// Kind 0x33: Effect_KindHandlers[0x33], EffectKind33_States by +1
// ===========================================================================

// original 0x4734F0 (Effect_KindHandlers[0x33], hidden in 0x473460's recorded
// extent): a tail jump through EffectKind33_States 0x6543E0 by +1.
extern "C" void __cdecl EffectKind33_Run(void) {
    Entry(AddressOf(EffectKind33_States), Cur()[1], 4, "EffectKind33_Run")();
}

// original 0x473510 (EffectKind33_States[0]): the disc's centre +0xC..+0x14
// the record's point +0x34..+0x3C, its radius +0x1C 0; +9 0xA, +1 up.
extern "C" void __cdecl EffectKind33_Start(void) {
    unsigned char* const s = Cur();
    SetUL(s + 0xC, UL(Cur() + 0x34));
    SetUL(s + 0x10, UL(Cur() + 0x38));
    const U height = UL(Cur() + 0x3C);
    SetUL(s + 0x1C, 0);
    SetUL(s + 0x14, height);
    Cur()[9] = 0xA;
    unsigned char* const t = Cur();
    t[1] = static_cast<unsigned char>(t[1] + 1);
}

// original 0x473550 (EffectKind33_States[1]): the disc drawn
// (EffectKind33_DrawDisc(+0xC, +0x1C)), its radius +0x1C up 0x1199 (the record
// read before the call); +9 down 1 - at 0, Sound_PlayEffect(0x20D) and +1 up.
extern "C" void __cdecl EffectKind33_Grow(void) {
    unsigned char* const s = Cur();
    SH_CALL(EffectKind33_DrawDisc)(reinterpret_cast<const long*>(s + 0xC), static_cast<long>(UL(s + 0x1C)));
    SetUL(s + 0x1C, UL(s + 0x1C) + 0x1199u);
    unsigned char* t = Cur();
    t[9] = static_cast<unsigned char>(t[9] - 1);
    if (Cur()[9] != 0) return;
    SH_CALL(Sound_PlayEffect)(0x20D);
    t = Cur();
    t[1] = static_cast<unsigned char>(t[1] + 1);
}

// original 0x4735B0: the map camera loaded; four quarters of the disc
// (EffectKind33_DrawQuarter) at the angles 0x400 i, each linked a step off the
// centre by EffectKind33_SignX[i] / _SignZ[i].
extern "C" void __cdecl EffectKind33_DrawDisc(const long* centre, long radius) {
    SH_CALL(EffectGte_LoadMapCamera)();
    for (U i = 0; i < 4; ++i) {
        const auto dz = static_cast<long>(UL(At(AddressOf(EffectKind33_SignZ) + 4 * i)));
        const auto dx = static_cast<long>(UL(At(AddressOf(EffectKind33_SignX) + 4 * i)));
        SH_CALL(EffectKind33_DrawQuarter)(centre, radius, i << 10, dx, dz);
    }
}

// original 0x473600: the centre projected; the point at `angle` (its low 16
// bits) and `radius` round it (the height the centre's, read once) projected;
// then four times: a draw mode (the page at (0x380, 0x100), abr 2, dtd 0)
// linked at the centre's x + dx, z + dz (MapView_LinkPrimAt(.., 0, 0x38)); a
// semi-transparent POLY_F3 (0x5A7570) at Gfx_PacketNext (read after the link)
// from the centre's projection to the last point's and the point 0x100 further
// round (the angle kept whole in its argument's slot), grey 0x80; linked the
// same.
extern "C" void __cdecl EffectKind33_DrawQuarter(const long* centre, long radius, unsigned angle, long dx, long dz) {
    float mid[3], edge[3];
    SH_CALL(EffectGte_ProjectPoint)(centre, mid);
    const U r = static_cast<U>(radius);
    long q[3];
    U a = angle & 0xFFFFu;
    const int c = SH_CALL(Math_Cos)(static_cast<int>(a));
    q[0] = static_cast<long>(static_cast<U>(MulSar(c, r, 12)) + static_cast<U>(centre[0]));
    const int n = SH_CALL(Math_Sin)(static_cast<int>(a));
    q[1] = static_cast<long>(static_cast<U>(MulSar(n, r, 12)) + static_cast<U>(centre[1]));
    q[2] = centre[2];
    SH_CALL(EffectGte_ProjectPoint)(q, edge);
    U slot = angle;
    for (unsigned k = 0; k < 4; ++k) {
        Mode(2, 0x380, 0x100, 0);
        const auto x = static_cast<unsigned long>(static_cast<U>(centre[0]) + static_cast<U>(dx));
        const auto z = static_cast<unsigned long>(static_cast<U>(centre[1]) + static_cast<U>(dz));
        SH_CALL(MapView_LinkPrimAt)(x, z, 0, 0x38);
        unsigned char* const prim = Gfx_PacketNext;
        SH_AT(void (__cdecl*)(unsigned char*), at::kSetPolyF3)(prim);
        SH_CALL(Gpu_SetSemiTrans)(prim, 1);
        std::memcpy(prim + 8, mid, 12);
        std::memcpy(prim + 0x14, edge, 12);
        slot += 0x100;
        a = slot & 0xFFFFu;
        const int c2 = SH_CALL(Math_Cos)(static_cast<int>(a));
        q[0] = static_cast<long>(static_cast<U>(MulSar(c2, r, 12)) + static_cast<U>(centre[0]));
        const int n2 = SH_CALL(Math_Sin)(static_cast<int>(a));
        q[1] = static_cast<long>(static_cast<U>(MulSar(n2, r, 12)) + static_cast<U>(centre[1]));
        SH_CALL(EffectGte_ProjectPoint)(q, edge);
        std::memcpy(prim + 0x20, edge, 12);
        prim[4] = prim[5] = prim[6] = 0x80;
        SH_CALL(MapView_LinkPrimAt)(x, z, 0, 0x38);
    }
}

// ===========================================================================
// Kind 0x35: Effect_KindHandlers[0x35], EffectKind35_States by +1 - kind
// 0x1E's shards and pieces (effect_1c.cpp) on Sprite_ObjectsExtra[1]'s model
// ===========================================================================

// original 0x4737A0 (Effect_KindHandlers[0x35], hidden in 0x473600's recorded
// extent): a tail jump through EffectKind35_States 0x654410 by +1.
extern "C" void __cdecl EffectKind35_Run(void) {
    Entry(AddressOf(EffectKind35_States), Cur()[1], 3, "EffectKind35_Run")();
}

// original 0x4737C0 (EffectKind35_States[0]): the sixteen shards set up, the
// model split; +1 up.
extern "C" void __cdecl EffectKind35_Start(void) {
    SH_CALL(EffectKind35_ShardsInit)();
    SH_CALL(EffectKind35_SplitModel)();
    unsigned char* const s = Cur();
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x4737E0 (EffectKind35_States[1]): the pieces stepped, the shards
// stepped and drawn; none in use, +1 up.
extern "C" void __cdecl EffectKind35_Burst(void) {
    SH_CALL(EffectKind35_StepPieces)();
    if (SH_CALL(EffectKind35_ShardsDraw)()) return;
    unsigned char* const s = Cur();
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x473800 (EffectKind35_States[2]): Sprite_ObjectsExtra[1]'s +0 = 0
// (the model's record freed); Effect_Release (a tail jump).
extern "C" void __cdecl EffectKind35_FreeModel(void) {
    At(at::kExtra1Use)[0] = 0;
    SH_CALL(Effect_Release)();
}

// original 0x473810: EffectKind1E_ShardsInit with sixteen: the cursor 0x676108
// set to EffectKind30_Shards, then sixteen times EffectKind1E_ShardInit(the
// cursor) (E1C's, ours) and the cursor up 0x28 (read again after the call).
extern "C" void __cdecl EffectKind35_ShardsInit(void) {
    SetCursor(at::kShards35);
    for (U i = 0; i < at::kShard35Count; ++i) {
        SH_CALL(EffectKind1E_ShardInit)(Cursor());
        SetCursor(Key(Cursor()) + 0x28);
    }
}

// original 0x473850: EffectKind1E_ShardsDraw with sixteen: the map camera
// loaded; the cursor walked over the shards: each in use stepped by
// EffectKind35_ShardStates[+1] (a call, the handler reading the cursor) and
// drawn (EffectKind35_ShardQuad of the cursor, read again after each call); al
// 1 when any was in use.
extern "C" unsigned char __cdecl EffectKind35_ShardsDraw(void) {
    SH_CALL(EffectGte_LoadMapCamera)();
    U c = at::kShards35;
    SetCursor(c);
    unsigned char any = 0;
    for (U i = 0; i < at::kShard35Count; ++i) {
        if (At(c)[0]) {
            Entry(AddressOf(EffectKind35_ShardStates), At(c)[1], 2, "EffectKind35_ShardsDraw")();
            SH_CALL(EffectKind35_ShardQuad)(Cursor());
            c = Key(Cursor());
            any = 1;
        }
        c += 0x28;
        SetCursor(c);
    }
    return any;
}

// original 0x4738A0: EffectKind1E_ShardQuad with the page's abr 2: a shard's
// POLY_FT4 at Gfx_PacketNext (read once), semi-transparent; the point (+4)
// projected, the size (+0x24, s16, both axes) scaled at its depth in place;
// the corners x - (w >> 1) and + w, y - (h >> 1) and + h at the x87's 53 bits,
// rounded once to a float; the depth at +0x10 +0x20 +0x30 +0x40; u 0xE0 /
// 0xFF, v 0x30 / 0x4F; the CLUT Gpu_GetClut(0xA0, 0x1E3), the page
// Gpu_GetTPage(0, 2, 0x2C0, 0x100); the shade +3; MapView_LinkPrimAt(x, z of
// the point, 2, 0x48).
extern "C" void __cdecl EffectKind35_ShardQuad(unsigned char* shard) {
    unsigned char* const prim = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyFT4)(prim);
    SH_CALL(Gpu_SetSemiTrans)(prim, 1);
    const long* const point = reinterpret_cast<const long*>(shard + 4);
    float screen[3];
    SH_CALL(EffectGte_ProjectPoint)(point, screen);
    short wh[2] = {SW(shard + 0x24), SW(shard + 0x24)};
    SH_CALL(EffectGte_ProjectSize)(point, wh, wh);
    const int w = wh[0], h = wh[1];
    const int hw = static_cast<short>(wh[0]) >> 1, hh = static_cast<short>(wh[1]) >> 1;
    const double left = static_cast<double>(screen[0]) - static_cast<double>(hw);
    const double top = static_cast<double>(screen[1]) - static_cast<double>(hh);
    StoreFloat(prim + 0x08, left);
    StoreFloat(prim + 0x0C, top);
    StoreFloat(prim + 0x18, left + static_cast<double>(w));
    StoreFloat(prim + 0x1C, top);
    StoreFloat(prim + 0x28, left);
    StoreFloat(prim + 0x2C, top + static_cast<double>(h));
    StoreFloat(prim + 0x38, left + static_cast<double>(w));
    prim[0x15] = 0x30;
    prim[0x25] = 0x30;
    prim[0x14] = 0xE0;
    prim[0x24] = 0xFF;
    prim[0x34] = 0xE0;
    prim[0x35] = 0x4F;
    prim[0x44] = 0xFF;
    prim[0x45] = 0x4F;
    StoreFloat(prim + 0x3C, top + static_cast<double>(h));
    std::memcpy(prim + 0x40, &screen[2], 4);
    std::memcpy(prim + 0x30, &screen[2], 4);
    std::memcpy(prim + 0x20, &screen[2], 4);
    std::memcpy(prim + 0x10, &screen[2], 4);
    SetWord(prim + 0x16, SH_CALL(Gpu_GetClut)(0xA0, 0x1E3));
    SetWord(prim + 0x26, SH_CALL(Gpu_GetTPage)(0, 2, 0x2C0, 0x100));
    prim[4] = shard[3];
    prim[5] = shard[3];
    prim[6] = shard[3];
    SH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(Long(shard + 4)), static_cast<unsigned long>(Long(shard + 8)),
                                2, 0x48);
}

// original 0x473A90 (EffectKind35_ShardStates[0]): EffectKind1E_ShardFly on
// the cursor 0x676108: the shard moved by twice its direction, its size +0x24
// up 0x80, its count +2 down - at 0, +2 0x40 and its state +1 up. The cursor
// read again for every access.
extern "C" void __cdecl EffectKind35_ShardFly(void) {
    unsigned char* r = Cursor();
    SetUL(r + 4, UL(r + 4) + (UL(r + 0x14) << 1));
    r = Cursor();
    SetUL(r + 8, UL(r + 8) + (UL(r + 0x18) << 1));
    r = Cursor();
    SetUL(r + 0xC, UL(r + 0xC) + (UL(r + 0x1C) << 1));
    r = Cursor();
    SetWord(r + 0x24, Word(r + 0x24) + 0x80u);
    r = Cursor();
    r[2] = static_cast<unsigned char>(r[2] - 1);
    r = Cursor();
    if (r[2] == 0) {
        r[2] = 0x40;
        r = Cursor();
        r[1] = static_cast<unsigned char>(r[1] + 1);
    }
}

// original 0x473B00 (EffectKind35_ShardStates[1]): EffectKind1E_ShardFade on
// the cursor 0x676108: moved by its direction, its shade +3 and count +2 down
// 1 - the count at 0, the shard freed (+0 = 0).
extern "C" void __cdecl EffectKind35_ShardFade(void) {
    unsigned char* r = Cursor();
    SetUL(r + 4, UL(r + 4) + UL(r + 0x14));
    r = Cursor();
    SetUL(r + 8, UL(r + 8) + UL(r + 0x18));
    r = Cursor();
    SetUL(r + 0xC, UL(r + 0xC) + UL(r + 0x1C));
    r = Cursor();
    r[3] = static_cast<unsigned char>(r[3] - 1);
    r = Cursor();
    r[2] = static_cast<unsigned char>(r[2] - 1);
    r = Cursor();
    if (r[2] == 0) r[0] = 0;
}

// original 0x473B60: EffectKind1E_SplitModel with 55 faces: the model
// (Sprite_ObjectsExtra[1] +0x50, read once) split: each face's 0x28 bytes
// copied (dword by dword, forward) to 0x92C728 + 0x28 i; its centre - the four
// vertices' (+2, +8, +0xE, +0x14: x, y, z s16 each) sums >> 2, 16 bits each -
// at the piece record 0x92C200 + 0x18 i; the centre as three longs normalised
// (Gte_VectorNormalS) into +8..+0xD and each s16 there >> 7 (kind 0x1E's >> 8);
// +0x10..+0x15 0; then the copy's twelve vertex words made relative to the
// centre.
extern "C" void __cdecl EffectKind35_SplitModel(void) {
    U src = UL(At(at::kExtra1Model));
    for (U i = 0; i < at::kPiece35Count; ++i) {
        unsigned char* const d = At(at::kCopies35 + 0x28 * i);
        unsigned char* const r = At(at::kPieces35 + 0x18 * i);
        for (U j = 0; j < 0x28; j += 4) SetUL(d + j, UL(At(src + j)));
        SetWord(r + 0, static_cast<U>((SW(d + 8) + SW(d + 0x14) + SW(d + 0xE) + SW(d + 2)) >> 2));
        SetWord(r + 2, static_cast<U>((SW(d + 4) + SW(d + 0x16) + SW(d + 0xA) + SW(d + 0x10)) >> 2));
        const int z = (SW(d + 0x18) + SW(d + 6) + SW(d + 0x12) + SW(d + 0xC)) >> 2;
        SetWord(r + 4, static_cast<U>(z));
        const long centre[3] = {SW(r + 0), SW(r + 2), static_cast<short>(z)};
        SH_CALL(Gte_VectorNormalS)(centre, reinterpret_cast<short*>(r + 8));
        for (U k = 8; k <= 0xC; k += 2) SetWord(r + k, static_cast<U>(SW(r + k) >> 7));
        SetWord(r + 0x10, 0);
        SetWord(r + 0x12, 0);
        SetWord(r + 0x14, 0);
        for (U v = 2; v <= 0x14; v += 6)
            for (U k = 0; k < 3; ++k) SetWord(d + v + 2 * k, Word(d + v + 2 * k) - Word(r + 2 * k));
        src += 0x28;
    }
}

// original 0x473C80: each of the 55 pieces stepped - its z velocity +0xC up 1
// (kind 0x1E's by the frame's parity), its centre x, z, y by its velocity
// (+8, +0xC, +0xA) - and turned (EffectKind35_TurnPiece(the copy, the model's
// face, the piece)); the model's faces from Sprite_ObjectsExtra[1] +0x50 read
// once.
extern "C" void __cdecl EffectKind35_StepPieces(void) {
    const U model = UL(At(at::kExtra1Model));
    for (U i = 0; i < at::kPiece35Count; ++i) {
        unsigned char* const r = At(at::kPieces35 + 0x18 * i);
        const U vx = Word(r + 8);
        SetWord(r + 0xC, Word(r + 0xC) + 1u);
        SetWord(r + 0, Word(r + 0) + vx);
        const U vz = Word(r + 0xC), vy = Word(r + 0xA);
        SetWord(r + 4, Word(r + 4) + vz);
        SetWord(r + 2, Word(r + 2) + vy);
        SH_CALL(EffectKind35_TurnPiece)(At(at::kCopies35 + 0x28 * i), At(model + 0x28 * i), r);
    }
}

// original 0x473CE0: EffectKind1E_TurnPiece as a loop: a piece's angles
// +0x10..+0x14 each Rand & 0xFC0; a rotation of them (Gte_RotMatrix), no
// translation, loaded (Gte_SetTransMatrix, Gte_SetRotMatrix); each of the
// copy's four relative vertices (+2, +8, +0xE, +0x14) turned (Gte_RotTrans)
// and written to the same place of the face plus the piece's centre (16 bits;
// the centre read after each call). The original hands Gte_RotTrans its third
// argument's slot as the flag; ours has no flag argument.
extern "C" void __cdecl EffectKind35_TurnPiece(const unsigned char* copy, unsigned char* face, unsigned char* piece) {
    SetWord(piece + 0x10, static_cast<U>(SH_CALL(Rand)()) & 0xFC0u);
    SetWord(piece + 0x12, static_cast<U>(SH_CALL(Rand)()) & 0xFC0u);
    SetWord(piece + 0x14, static_cast<U>(SH_CALL(Rand)()) & 0xFC0u);
    Matrix m;
    SH_CALL(Gte_RotMatrix)(reinterpret_cast<const short*>(piece + 0x10), m.m);
    m.t[0] = m.t[1] = m.t[2] = 0;
    SH_CALL(Gte_SetTransMatrix)(reinterpret_cast<const unsigned long*>(&m));
    SH_CALL(Gte_SetRotMatrix)(reinterpret_cast<const unsigned long*>(&m));
    for (U v = 2; v <= 0x14; v += 6) {
        short vector[4];
        vector[0] = SW(copy + v);
        vector[1] = SW(copy + v + 2);
        vector[2] = SW(copy + v + 4);
        long out[3];
        SH_CALL(Gte_RotTrans)(vector, out);
        SetWord(face + v, Word(piece + 0) + static_cast<U>(out[0]));
        SetWord(face + v + 2, static_cast<U>(out[1]) + Word(piece + 2));
        SetWord(face + v + 4, static_cast<U>(out[2]) + Word(piece + 4));
    }
}

// ===========================================================================
// Kind 0x38: Effect_KindHandlers[0x38], EffectKind38_States by +1
// ===========================================================================

// original 0x473DD0 (Effect_KindHandlers[0x38], hidden in 0x473CE0's recorded
// extent): a tail jump through EffectKind38_States 0x654424 by +1.
extern "C" void __cdecl EffectKind38_Run(void) {
    Entry(AddressOf(EffectKind38_States), Cur()[1], 5, "EffectKind38_Run")();
}

// original 0x473DF0 (EffectKind38_States[0]): +9 0x10, +1 1, the tile's
// colour +0x5D..+0x5F 0.
extern "C" void __cdecl EffectKind38_Start(void) {
    Cur()[9] = 0x10;
    Cur()[1] = 1;
    Cur()[0x5F] = 0;
    Cur()[0x5E] = 0;
    Cur()[0x5D] = 0;
}

// original 0x473F10 (the tail of 0x473EC0, which 0x473E30 and 0x473E90 jump
// to; no list had it): a draw mode (page word 0x4F, dtd 1) committed at slot
// 2; a semi-transparent TILE at Gfx_PacketNext (read after the commit),
// untextured, at (0, 0) and 320.0 x 240.0 (floats), coloured +0x5D, +0x5E,
// +0x5F (Sprite_Current read for each); committed at slot 2 (0x1C bytes).
extern "C" void __cdecl EffectKind38_DrawTint(void) {
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x4F, 0);
    SH_CALL(Gfx_CommitPrim)(2, 0xC);
    unsigned char* const prim = Gfx_PacketNext;
    SH_CALL(Gpu_SetTile)(prim);
    prim[4] = Cur()[0x5D];
    prim[5] = Cur()[0x5E];
    const unsigned char blue = Cur()[0x5F];
    SetUL(prim + 8, std::bit_cast<std::uint32_t>(Widescreen_FillX()));   // DIV-0041: (-53, 0) 426 wide under the wide picture
    prim[6] = blue;
    SetUL(prim + 0xC, 0);
    SetUL(prim + 0x14, std::bit_cast<std::uint32_t>(Widescreen_FillWidth()));   // 0x43A00000, 320.0f narrow
    SetUL(prim + 0x18, 0x43700000u);   // 240.0f
    SH_CALL(Gpu_SetSemiTrans)(prim, 1);
    SH_CALL(Gpu_SetShadeTex)(prim, 0);
    SH_CALL(Gfx_CommitPrim)(2, 0x1C);
}

// The tile's three colour bytes stepped by `by` (the record read for each).
static void TintStep(unsigned char* s, unsigned char by) {
    s[0x5D] = static_cast<unsigned char>(s[0x5D] + by);
    s = Cur();
    s[0x5E] = static_cast<unsigned char>(s[0x5E] + by);
    s = Cur();
    s[0x5F] = static_cast<unsigned char>(s[0x5F] + by);
}

// original 0x473E30 (EffectKind38_States[1]): +9 down 1 - not yet 0, the tile
// brighter by 6; at 0, message 0x1F opened (Msg_OpenScript), Field_Request 2,
// +1 2. The tile drawn (a tail jump to EffectKind38_DrawTint).
extern "C" void __cdecl EffectKind38_FadeIn(void) {
    unsigned char* s = Cur();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    s = Cur();
    if (s[9] != 0) {
        TintStep(s, 6);
    } else {
        SH_CALL(Msg_OpenScript)(0x1F);
        unsigned char* const t = Cur();
        Field_Request = 2;
        t[1] = 2;
    }
    SH_CALL(EffectKind38_DrawTint)();
}

// original 0x473E90 (EffectKind38_States[2]): Field_Request no longer 2 (the
// message closed), +9 0x10 and +1 3. The tile drawn (a tail jump).
extern "C" void __cdecl EffectKind38_WaitMessage(void) {
    if (Field_Request != 2) {
        Cur()[9] = 0x10;
        Cur()[1] = 3;
    }
    SH_CALL(EffectKind38_DrawTint)();
}

// original 0x473EC0 (EffectKind38_States[3]): +9 down 1 - not yet 0, the tile
// dimmer by 6; at 0, +1 4 (Effect_StateRelease). The tile drawn (0x473F10 is
// this function's own tail: run here in place, not through the recorder).
extern "C" void __cdecl EffectKind38_FadeOut(void) {
    unsigned char* s = Cur();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    s = Cur();
    if (s[9] != 0)
        TintStep(s, 0xFA);
    else
        s[1] = 4;
    EffectKind38_DrawTint();
}

// ===========================================================================
// Kind 0x39: Effect_KindHandlers[0x39], EffectKind39_States by +1
// ===========================================================================

// original 0x473FA0 (Effect_KindHandlers[0x39], hidden in 0x473CE0's recorded
// extent): a tail jump through EffectKind39_States 0x654438 by +1.
extern "C" void __cdecl EffectKind39_Run(void) {
    Entry(AddressOf(EffectKind39_States), Cur()[1], 4, "EffectKind39_Run")();
}

// original 0x473FC0 (EffectKind39_States[0]): +9 0, +1 up;
// Sound_PlayEffect(0x20E).
extern "C" void __cdecl EffectKind39_Start(void) {
    Cur()[9] = 0;
    unsigned char* const s = Cur();
    s[1] = static_cast<unsigned char>(s[1] + 1);
    SH_CALL(Sound_PlayEffect)(0x20E);
}

// The fan at the record's screen point (+0x2E, +0x30) with the radius +9 (each
// pushed with the upper half the register held: only the words are read).
static void Kind39Disc() {
    const unsigned char* const s = Cur();
    SH_CALL(EffectKind39_DrawDisc)(Word(s + 0x2E), Word(s + 0x30), s[9]);
}

// original 0x473FE0 (EffectKind39_States[1]): the fan drawn; +9 up 1 - past
// 0x1E, +9 0x1E and +1 up.
extern "C" void __cdecl EffectKind39_Grow(void) {
    Kind39Disc();
    unsigned char* s = Cur();
    s[9] = static_cast<unsigned char>(s[9] + 1);
    s = Cur();
    if (s[9] > 0x1E) {
        s[9] = 0x1E;
        s = Cur();
        s[1] = static_cast<unsigned char>(s[1] + 1);
    }
}

// original 0x474030 (EffectKind39_States[2]): the fan drawn; +9 down 1 - at
// 0, +1 up (Effect_StateRelease).
extern "C" void __cdecl EffectKind39_Shrink(void) {
    Kind39Disc();
    unsigned char* s = Cur();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    s = Cur();
    if (s[9] == 0) s[1] = static_cast<unsigned char>(s[1] + 1);
}

// A screen fan's rim point: (trig * radius >> 12) + the centre as a dword, then
// a float (fild of it).
static float Rim(int trig, U radius, int centre) {
    return static_cast<float>(static_cast<std::int32_t>(static_cast<U>(MulSar(trig, radius, 12)) + static_cast<U>(centre)));
}

// original 0x474070: a draw mode (the page at (0x3C0, 0), abr 1, dtd 1)
// committed at slot 1; 32 semi-transparent G3 triangles at Gfx_PacketNext
// fanned round the screen point (x, y) (s16 each, the centre as floats), the
// rim points at `radius` (s16) and the angles 0x80 k (whole) and 0x80 (k + 1)
// (its low 16 bits), each (trig * radius >> 12) + the centre as an int then a
// float; the centre grey 0x80, the rim black; no depth written; each committed
// at slot 1 (0x34 bytes).
extern "C" void __cdecl EffectKind39_DrawDisc(int x, int y, int radius) {
    Mode(1, 0x3C0, 0, 1);
    SH_CALL(Gfx_CommitPrim)(1, 0xC);
    const int cx = static_cast<short>(x), cy = static_cast<short>(y);
    const U r = static_cast<U>(static_cast<int>(static_cast<short>(radius)));
    const auto fx = static_cast<float>(cx);
    const auto fy = static_cast<float>(cy);
    U a1 = 0, next = 0x80;
    do {
        unsigned char* const prim = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG3)(prim);
        SH_CALL(Gpu_SetSemiTrans)(prim, 1);
        std::memcpy(prim + 8, &fx, 4);
        std::memcpy(prim + 0xC, &fy, 4);
        StoreFloat(prim + 0x18, Rim(SH_CALL(Math_Cos)(static_cast<int>(a1)), r, cx));
        StoreFloat(prim + 0x1C, Rim(SH_CALL(Math_Sin)(static_cast<int>(a1)), r, cy));
        const U a2 = next & 0xFFFFu;
        StoreFloat(prim + 0x28, Rim(SH_CALL(Math_Cos)(static_cast<int>(a2)), r, cx));
        StoreFloat(prim + 0x2C, Rim(SH_CALL(Math_Sin)(static_cast<int>(a2)), r, cy));
        prim[4] = prim[5] = prim[6] = 0x80;
        prim[0x14] = prim[0x15] = prim[0x16] = 0;
        prim[0x24] = prim[0x25] = prim[0x26] = 0;
        SH_CALL(Gfx_CommitPrim)(1, 0x34);
        next += 0x80;
        a1 += 0x80;
    } while (((next - 0x80u) & 0xFFFFu) < 0x1000u);
}

// ===========================================================================
// Kind 0x3B: Effect_KindHandlers[0x3B], EffectKind3B_States by +1
// ===========================================================================

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

// The glow's draw mode (the page at (0x380, 0x100), abr 1, dtd 1) committed at
// slot 1, and the glow at a copy of the record's point, its size `byte` << 2,
// grey 0x80 at the centre and black at the rim.
static void Kind3BGlow(unsigned offset) {
    Mode(1, 0x380, 0x100, 1);
    SH_CALL(Gfx_CommitPrim)(1, 0xC);
    const unsigned char* const s = Cur();
    const long point[3] = {Long(s + 0x34), Long(s + 0x38), Long(s + 0x3C)};
    SH_CALL(EffectKind3B_DrawGlow)(point, static_cast<U>(s[offset]) << 2, 0x80, 0);
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
        unsigned char* const r = At(at::kSpirals + at::kSpiralStride * i);
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
        unsigned char* const r = At(at::kSpirals + at::kSpiralStride * i);
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
        length += SH_AT(U (__cdecl*)(long), at::kSqrt)(squared);
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
    for (U i = 0; i < at::kSpiralCount; ++i) {
        unsigned char* const r = At(at::kSpirals + at::kSpiralStride * i);
        SetUL(r + 0, UL(s + 0x34));
        SetUL(r + 4, UL(s + 0x38));
        SetWord(r + 0x12, 0xC00);
        SetUL(r + 8, UL(s + 0x3C) + 0x3800000u);
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

// ===========================================================================
// Kind 0x3D: Effect_KindHandlers[0x3D], EffectKind3D_States by +1
// ===========================================================================

// original 0x474940 (Effect_KindHandlers[0x3D], hidden in 0x4747D0's recorded
// extent): a tail jump through EffectKind3D_States 0x654458 by +1.
extern "C" void __cdecl EffectKind3D_Run(void) {
    Entry(AddressOf(EffectKind3D_States), Cur()[1], 4, "EffectKind3D_Run")();
}

// original 0x474960 (EffectKind3D_States[0]): the timer +0xC (a dword) 0x3C,
// +1 up.
extern "C" void __cdecl EffectKind3D_Start(void) {
    SetUL(Cur() + 0xC, 0x3C);
    unsigned char* const s = Cur();
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x474980 (EffectKind3D_States[1]): the timer down 1 - at 0, the
// timer 0x230, the rings cleared (EffectKind3D_RingsClear), +1 up.
extern "C" void __cdecl EffectKind3D_Wait(void) {
    unsigned char* s = Cur();
    SetUL(s + 0xC, UL(s + 0xC) - 1);
    s = Cur();
    if (UL(s + 0xC) != 0) return;
    SetUL(s + 0xC, 0x230);
    SH_CALL(EffectKind3D_RingsClear)();
    s = Cur();
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x4749B0 (EffectKind3D_States[2]): the glow at the leader
// (EffectKind3D_DrawGlow(0x80)), the rings stepped, a ring started every
// eighth frame (Frame_Counter & 7 == 0); the timer down 1 - at 0, the timer
// 0x10 and +1 up.
extern "C" void __cdecl EffectKind3D_Glow(void) {
    SH_CALL(EffectKind3D_DrawGlow)(0x80);
    SH_CALL(EffectKind3D_RingsStep)();
    if ((Frame_Counter & 7u) == 0) SH_CALL(EffectKind3D_RingSpawn)();
    unsigned char* s = Cur();
    SetUL(s + 0xC, UL(s + 0xC) - 1);
    s = Cur();
    if (UL(s + 0xC) != 0) return;
    SetUL(s + 0xC, 0x10);
    s = Cur();
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x474A00 (EffectKind3D_States[3]): the timer down 1 while above 0
// (signed); the glow at the timer's low word << 3; the rings stepped - none
// in use, Effect_Release (a tail jump).
extern "C" void __cdecl EffectKind3D_Fade(void) {
    unsigned char* s = Cur();
    const auto timer = static_cast<std::int32_t>(UL(s + 0xC));
    if (timer > 0) {
        SetUL(s + 0xC, static_cast<U>(timer - 1));
        s = Cur();
    }
    SH_CALL(EffectKind3D_DrawGlow)((Word(s + 0xC) << 3) & 0xFFFFu);
    if (!SH_CALL(EffectKind3D_RingsStep)()) SH_CALL(Effect_Release)();
}

// original 0x474A40: the glow's draw mode (the page at (0x380, 0x100), abr 1,
// dtd 1) committed at slot 1; the glow (EffectKind3D_DrawFan) at a copy of
// the leader's point (ObjTrio +0x34, +0x38, +0x3C + 0x80 << 16), `size` (its
// low word; the whole dword passed on), grey 0x60 at the centre, black at the
// rim.
extern "C" void __cdecl EffectKind3D_DrawGlow(unsigned size) {
    Mode(1, 0x380, 0x100, 1);
    SH_CALL(Gfx_CommitPrim)(1, 0xC);
    const unsigned char* const leader = At(at::kLeaderPoint);
    const long point[3] = {Long(leader), Long(leader + 4), static_cast<long>(UL(leader + 8) + 0x800000u)};
    SH_CALL(EffectKind3D_DrawFan)(point, size, 0x60, 0);
}

// original 0x474AC0: EffectKind3B_DrawGlow with the frame's parity doubled in
// the radius (GlowFan).
extern "C" void __cdecl EffectKind3D_DrawFan(const long* point, unsigned size, unsigned centre, unsigned rim) {
    GlowFan(point, size, centre, rim, 1);
}

// original 0x474C30: the sixteen rings' in-use bytes (+0x14) cleared.
extern "C" void __cdecl EffectKind3D_RingsClear(void) {
    for (U i = 0; i < at::kRingCount; ++i) At(at::kRings + at::kRingStride * i + 0x14)[0] = 0;
}

// original 0x474C50: the first free ring of the sixteen (+0x14 0) in use, its
// state +0x15 0; none free, nothing.
extern "C" void __cdecl EffectKind3D_RingSpawn(void) {
    for (U i = 0; i < at::kRingCount; ++i) {
        unsigned char* const r = At(at::kRings + at::kRingStride * i);
        if (r[0x14]) continue;
        r[0x14] = 1;
        r[0x15] = 0;
        return;
    }
}

// original 0x474C80: each ring in use by its state +0x15 - 0: width +0x10 0,
// half-thickness +0x12 0x10, shade +0x17 0x20, count +0x16 0x20, state up; 1:
// the point the leader's (ObjTrio +0x34, +0x38, +0x3C + 0x80 << 16), the width
// up 8, the shade and count down 1 - the count at 0, the ring freed - and
// drawn (EffectKind3D_DrawRing); any other, nothing. al 1 when any was in use.
extern "C" unsigned char __cdecl EffectKind3D_RingsStep(void) {
    unsigned char any = 0;
    for (U i = 0; i < at::kRingCount; ++i) {
        unsigned char* const r = At(at::kRings + at::kRingStride * i);
        if (!r[0x14]) continue;
        any = 1;
        if (r[0x15] == 0) {
            const auto state = static_cast<unsigned char>(r[0x15] + 1);
            SetWord(r + 0x10, 0);
            SetWord(r + 0x12, 0x10);
            r[0x17] = 0x20;
            r[0x15] = state;
            r[0x16] = 0x20;
        } else if (r[0x15] == 1) {
            const unsigned char* const leader = At(at::kLeaderPoint);
            const U x = UL(leader), z = UL(leader + 4);
            SetWord(r + 0x10, Word(r + 0x10) + 8u);
            SetUL(r + 4, z);
            const auto shade = static_cast<unsigned char>(r[0x17] - 1);
            SetUL(r + 0, x);
            SetUL(r + 8, UL(leader + 8) + 0x800000u);
            const auto count = static_cast<unsigned char>(r[0x16] - 1);
            r[0x17] = shade;
            r[0x16] = count;
            if (count == 0) r[0x14] = 0;
            SH_CALL(EffectKind3D_DrawRing)(r);
        }
    }
    return any;
}

// original 0x474D20 (PSX twin 0x801F2F60 by callers): a ring (its point +0..+8)
// projected; twice - the half-thickness +0x12 added, then taken off -: the
// width +0x10 and the width plus or minus the half-thickness (16 bits) scaled
// at the point's depth (EffectGte_ProjectSize: in {w +- h, w}, out the
// original's argument slot); 32 semi-transparent G4 quads round the screen
// point at steps of 0x80 (the first angle whole, the second & 0xFFF), the
// outer two corners at w +- h black, the inner two at w the shade +0x17, the
// depth at every corner; each committed at slot 1 (0x44 bytes).
extern "C" void __cdecl EffectKind3D_DrawRing(unsigned char* ring) {
    const long* const point = reinterpret_cast<const long*>(ring);
    float p[3];
    SH_CALL(EffectGte_ProjectPoint)(point, p);
    for (unsigned pass = 0; pass < 2; ++pass) {
        int h = SW(ring + 0x12);
        if (pass != 0) h = -h;
        const short w = SW(ring + 0x10);
        short buf[6] = {static_cast<short>(h + w), w, 0, 0, 0, 0};   // in = buf, out = buf + 4
        SH_CALL(EffectGte_ProjectSize)(point, buf, buf + 4);
        const U outer = static_cast<U>(static_cast<int>(buf[4]));
        const U inner = static_cast<U>(static_cast<int>(buf[5]));
        U a1 = 0, next = 0;
        do {
            unsigned char* const prim = Gfx_PacketNext;
            SH_CALL(Gpu_SetPolyG4)(prim);
            SH_CALL(Gpu_SetSemiTrans)(prim, 1);
            next += 0x80;
            const U a2 = next & 0xFFFu;
            StoreFloat(prim + 0x08, static_cast<double>(MulSar(SH_CALL(Math_Cos)(static_cast<int>(a1)), outer, 12)) + static_cast<double>(p[0]));
            StoreFloat(prim + 0x0C, static_cast<double>(MulSar(SH_CALL(Math_Sin)(static_cast<int>(a1)), outer, 12)) + static_cast<double>(p[1]));
            StoreFloat(prim + 0x18, static_cast<double>(MulSar(SH_CALL(Math_Cos)(static_cast<int>(a2)), outer, 12)) + static_cast<double>(p[0]));
            StoreFloat(prim + 0x1C, static_cast<double>(MulSar(SH_CALL(Math_Sin)(static_cast<int>(a2)), outer, 12)) + static_cast<double>(p[1]));
            StoreFloat(prim + 0x28, static_cast<double>(MulSar(SH_CALL(Math_Cos)(static_cast<int>(a1)), inner, 12)) + static_cast<double>(p[0]));
            StoreFloat(prim + 0x2C, static_cast<double>(MulSar(SH_CALL(Math_Sin)(static_cast<int>(a1)), inner, 12)) + static_cast<double>(p[1]));
            StoreFloat(prim + 0x38, static_cast<double>(MulSar(SH_CALL(Math_Cos)(static_cast<int>(a2)), inner, 12)) + static_cast<double>(p[0]));
            StoreFloat(prim + 0x3C, static_cast<double>(MulSar(SH_CALL(Math_Sin)(static_cast<int>(a2)), inner, 12)) + static_cast<double>(p[1]));
            std::memcpy(prim + 0x40, &p[2], 4);
            std::memcpy(prim + 0x30, &p[2], 4);
            std::memcpy(prim + 0x20, &p[2], 4);
            std::memcpy(prim + 0x10, &p[2], 4);
            prim[4] = prim[5] = prim[6] = 0;
            prim[0x14] = prim[0x15] = prim[0x16] = 0;
            prim[0x24] = prim[0x25] = prim[0x26] = ring[0x17];
            prim[0x34] = prim[0x35] = prim[0x36] = ring[0x17];
            SH_CALL(Gfx_CommitPrim)(1, 0x44);
            a1 += 0x80;
        } while ((next & 0xFFFFu) < 0x1000u);
    }
}

void Effect2B_Inject() {
    if (bof3::WantsShadow("effect_2b")) effect_2b::SelfTest();
    BOF3_INJECT(EffectKind2F_Run);
    BOF3_INJECT(EffectKind2F_Start);
    BOF3_INJECT(EffectKind2F_Spin);
    BOF3_INJECT(EffectKind2F_StepTrail);
    BOF3_INJECT(EffectKind2F_DrawTrail);
    BOF3_INJECT(EffectKind2F_InitTrail);
    BOF3_INJECT(EffectKind33_Run);
    BOF3_INJECT(EffectKind33_Start);
    BOF3_INJECT(EffectKind33_Grow);
    BOF3_INJECT(EffectKind33_DrawDisc);
    BOF3_INJECT(EffectKind33_DrawQuarter);
    BOF3_INJECT(EffectKind35_Run);
    BOF3_INJECT(EffectKind35_Start);
    BOF3_INJECT(EffectKind35_Burst);
    BOF3_INJECT(EffectKind35_FreeModel);
    BOF3_INJECT(EffectKind35_ShardsInit);
    BOF3_INJECT(EffectKind35_ShardsDraw);
    BOF3_INJECT(EffectKind35_ShardQuad);
    BOF3_INJECT(EffectKind35_ShardFly);
    BOF3_INJECT(EffectKind35_ShardFade);
    BOF3_INJECT(EffectKind35_SplitModel);
    BOF3_INJECT(EffectKind35_StepPieces);
    BOF3_INJECT(EffectKind35_TurnPiece);
    BOF3_INJECT(EffectKind38_Run);
    BOF3_INJECT(EffectKind38_Start);
    BOF3_INJECT(EffectKind38_FadeIn);
    BOF3_INJECT(EffectKind38_WaitMessage);
    BOF3_INJECT(EffectKind38_FadeOut);
    BOF3_INJECT(EffectKind38_DrawTint);
    BOF3_INJECT(EffectKind39_Run);
    BOF3_INJECT(EffectKind39_Start);
    BOF3_INJECT(EffectKind39_Grow);
    BOF3_INJECT(EffectKind39_Shrink);
    BOF3_INJECT(EffectKind39_DrawDisc);
    BOF3_INJECT(EffectKind3B_Run);
    BOF3_INJECT(EffectKind3B_Start);
    BOF3_INJECT(EffectKind3B_Rise);
    BOF3_INJECT(EffectKind3B_Fade);
    BOF3_INJECT(EffectKind3B_DrawSpiral);
    BOF3_INJECT(EffectKind3B_InitSpirals);
    BOF3_INJECT(EffectKind3B_DrawGlow);
    BOF3_INJECT(EffectKind3D_Run);
    BOF3_INJECT(EffectKind3D_Start);
    BOF3_INJECT(EffectKind3D_Wait);
    BOF3_INJECT(EffectKind3D_Glow);
    BOF3_INJECT(EffectKind3D_Fade);
    BOF3_INJECT(EffectKind3D_DrawGlow);
    BOF3_INJECT(EffectKind3D_DrawFan);
    BOF3_INJECT(EffectKind3D_RingsClear);
    BOF3_INJECT(EffectKind3D_RingSpawn);
    BOF3_INJECT(EffectKind3D_RingsStep);
    BOF3_INJECT(EffectKind3D_DrawRing);
}

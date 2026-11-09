// Moved here by tools/move_funcs.py from effect_2a.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_29.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_2a.h"
#include "game/effect_2a_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_2a::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

using move_script::SetLong;

unsigned char* S() { return Sprite_Current; }

U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }

void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }

U AddressOf(const void* p) { return Low32(p); }

// sar: an arithmetic right shift of the 32 bits as the original holds them.
U Sar(U v, unsigned n) { return static_cast<U>(static_cast<std::int32_t>(v) >> n); }

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 1]; jmp (or call)
// [table + eax * 4]: the table's `entries` handlers read in place (the fuzz
// swaps the cells for recorders); a Fatal past them, where the original jumps
// through the dword after - the next kind's table or data.
void Dispatch(const char* who, U table, unsigned entries) {
    const unsigned state = Sprite_Current[1];
    if (state >= entries)
        bof3::Fatal("%s: state byte +1 is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/effect_2a.md section 2)",
                    who, state, entries, (unsigned)table);
    reinterpret_cast<scenario_harness::Handler>(static_cast<std::uintptr_t>(UL(At(table + 4 * state))))();
}

// +1 up, Sprite_Current read afresh.
void NextState() {
    unsigned char* const s = S();
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// +9 down one (Sprite_Current read for each access, as the originals); at 0,
// +9 = `reload` and +1 up - or, when `reload` is negative, +1 up with +9 left 0.
void CountDown(int reload) {
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    s = S();
    if (s[9] != 0) return;
    if (reload >= 0) {
        s[9] = static_cast<unsigned char>(reload);
        s = S();
    }
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// A draw mode for the page (x, y) with transparency `abr`, and `dtd` its
// dither flag: Gpu_GetTPage(0, abr, x, y), Gpu_SetDrawMode(the packet cursor,
// 0, dtd, the page's low word, 0).
void DrawMode(unsigned abr, int x, int y, int dtd) {
    const unsigned tpage = SH_CALL(Gpu_GetTPage)(0, abr, x, y);
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, dtd, tpage & 0xFFFFu, 0);
}

}  // namespace

// original 0x4709E0: jmp [EffectKind29_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind29_Run(void) { Dispatch("EffectKind29_Run", AddressOf(EffectKind29_States), EffectKind29_States_count); }

// original 0x470A00 (state 0): two boxes from the record + 0xC (Sprite_Current
// read once): box A's +0xC / +0x10 / +0x14 = (0x568000, 0x128000, 0x1000000),
// its half sizes +0x1C = 0 and +0x20 = 0x2200000; box B's +0x24 / +0x28 / +0x2C
// the same point, +0x34 = 0x4000 and +0x38 = 0x2200000 (+0x18 and +0x30 not
// written). EffectKind29_SetSegments; +9 = 8, +1 up.
extern "C" void __cdecl EffectKind29_Start(void) {
    unsigned char* const q = S() + 0xC;
    SetUL(q, 0x568000);
    SetUL(q + 4, 0x128000);
    SetUL(q + 8, 0x1000000);
    SetUL(q + 0x10, 0);
    SetUL(q + 0x14, 0x2200000);
    unsigned char* const r = q + 0x18;
    SetUL(r, 0x568000);
    SetUL(r + 4, 0x128000);
    SetUL(r + 8, 0x1000000);
    SetUL(r + 0x10, 0x4000);
    SetUL(r + 0x14, 0x2200000);
    SH_CALL(EffectKind29_SetSegments)();
    S()[9] = 8;
    NextState();
}

// original 0x470A70 (state 1): box B moved: x +0x24 + 0x10000, half height +0x38
// + 0x220000, half depth +0x34 + 0x2000 (all read before any is written). The
// segments drawn to t = 0x10 - 2 * +9 (a byte; the pushed dword's upper bytes
// the new +0x34's), shade 0xFF; the boxes drawn (the record + 0xC taken on
// entry); +9 down, at 0 +9 = 0x40 and +1 up.
extern "C" void __cdecl EffectKind29_Extend(void) {
    unsigned char* const s = S();
    auto* const so = rv::As<records::EffectObject>(s);
    unsigned char* const q = s + 0xC;
    const U x = UL(s + 0x24) + 0x10000u;
    const U h = static_cast<std::uint32_t>(so->z) + 0x220000u;
    const U d = UL(q + 0x28) + 0x2000u;
    SetUL(q + 0x18, x);
    SetUL(q + 0x2C, h);
    SetUL(q + 0x28, d);
    const U t = (d & 0xFFFFFF00u) | static_cast<unsigned char>(0x10 - 2 * S()[9]);
    SH_CALL(EffectKind29_DrawSegments)(t, 0xFF);
    SH_CALL(EffectKind29_DrawBoxes)(q);
    CountDown(0x40);
}

// original 0x470AF0 (state 2): the boxes drawn, the segments whole (t 0x10,
// shade 0xFF); +9 down, at 0 +9 = 4 and +1 up.
extern "C" void __cdecl EffectKind29_Hold(void) {
    SH_CALL(EffectKind29_DrawBoxes)(S() + 0xC);
    SH_CALL(EffectKind29_DrawSegments)(0x10, 0xFF);
    CountDown(4);
}

// original 0x470B40 (state 3): box A's half depth +0x1C + 0x4000 and box B's
// +0x34 + 0x10000; the boxes drawn; the segments whole, shaded 0xFF at +9 = 8,
// else +9 << 5 (its low byte); +9 down, at 0 +9 = 0x20 and +1 up.
extern "C" void __cdecl EffectKind29_Fade(void) {
    unsigned char* const q = S() + 0xC;
    const U a = UL(q + 0x10) + 0x4000u;
    const U b = UL(q + 0x28) + 0x10000u;
    SetUL(q + 0x10, a);
    SetUL(q + 0x28, b);
    SH_CALL(EffectKind29_DrawBoxes)(q);
    const unsigned count = S()[9];
    SH_CALL(EffectKind29_DrawSegments)(0x10, count == 8 ? 0xFFu : count << 5);
    CountDown(0x20);
}

// original 0x470BC0 (state 4): the boxes drawn; +9 down, at 0 +1 up.
extern "C" void __cdecl EffectKind29_Linger(void) {
    SH_CALL(EffectKind29_DrawBoxes)(S() + 0xC);
    CountDown(-1);
}

// original 0x470BF0 (0x18A bytes): a draw mode (0x3C0, 0, transparency 1)
// committed 0xC in slot 1; the eight corners into 0x676080 (0x10 each): box A
// (boxes +0..+0x14: a point x, z, height and the half sizes +0x10 z, +0x14
// height) at (x, z + dz, h + dh), (x, z - dz, h + dh), (x, z + dz, h), (x, z -
// dz, h); box B (+0x18..+0x2C) likewise; EffectGte_LoadMapCamera; the five faces
// EffectKind29_DrawFace (0, 1, 2, 3 | 1, 1), (0, 1, 4, 5 | 1, 0), (2, 0, 6, 4 |
// 1, 0), (1, 3, 5, 7 | 1, 0), (2, 3, 6, 7 | 1, 0).
extern "C" void __cdecl EffectKind29_DrawBoxes(const unsigned char* boxes) {
    DrawMode(1, 0x3C0, 0, 1);
    SH_CALL(Gfx_CommitPrim)(1, 0xC);
    for (unsigned b = 0; b < 2; ++b) {
        const unsigned char* const box = boxes + 0x18 * b;
        const U x = UL(box), z = UL(box + 4), h = UL(box + 8), dz = UL(box + 0x10), dh = UL(box + 0x14);
        const U corners[4][3] = {{x, z + dz, h + dh}, {x, z - dz, h + dh}, {x, z + dz, h}, {x, z - dz, h}};
        for (unsigned c = 0; c < 4; ++c)
            for (unsigned k = 0; k < 3; ++k) SetUL(At(::bof3::Img(at::kBoxPoints.v) + 0x10 * (4 * b + c) + 4 * k), corners[c][k]);
    }
    SH_CALL(EffectGte_LoadMapCamera)();
    SH_CALL(EffectKind29_DrawFace)(0, 1, 2, 3, 1, 1);
    SH_CALL(EffectKind29_DrawFace)(0, 1, 4, 5, 1, 0);
    SH_CALL(EffectKind29_DrawFace)(2, 0, 6, 4, 1, 0);
    SH_CALL(EffectKind29_DrawFace)(1, 3, 5, 7, 1, 0);
    SH_CALL(EffectKind29_DrawFace)(2, 3, 6, 7, 1, 0);
}

// original 0x470D80 (0xE2 bytes): a semi-transparent Gouraud quad at the packet
// cursor, its vertices EffectGte_ProjectPoint of the corners 0x676080 + 0x10 *
// (each index's low byte); shaded ((shade << 5) + the frame's low bit) << 2 (byte
// arithmetic) for the first two vertices from shade0, the last two from shade1,
// blue at half; committed 0x44 in slot 1. An index past the eight corners reads
// past the table: ours aborts there.
extern "C" void __cdecl EffectKind29_DrawFace(unsigned i0, unsigned i1, unsigned i2, unsigned i3, unsigned shade0,
                                              unsigned shade1) {
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyG4)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, 1);
    const unsigned index[4] = {i0 & 0xFFu, i1 & 0xFFu, i2 & 0xFFu, i3 & 0xFFu};
    for (unsigned k = 0; k < 4; ++k) {
        if (index[k] >= at::kBoxPointCount)
            bof3::Fatal("EffectKind29_DrawFace: corner %u is %u, past the eight at 0x676080 (docs/effect_2a.md section 2)",
                        k, index[k]);
        SH_CALL(EffectGte_ProjectPoint)(reinterpret_cast<const long*>(At(::bof3::Img(at::kBoxPoints.v) + 0x10 * index[k])),
                                        reinterpret_cast<float*>(p + 8 + 0x10 * k));
    }
    const auto bit = static_cast<unsigned char>(Frame_Counter & 1u);
    const auto first = static_cast<unsigned char>(static_cast<unsigned char>(static_cast<unsigned char>(shade0 << 5) + bit) << 2);
    const auto second = static_cast<unsigned char>(static_cast<unsigned char>(static_cast<unsigned char>(shade1 << 5) + bit) << 2);
    p[4] = p[5] = first;
    p[6] = static_cast<unsigned char>(first >> 1);
    p[0x14] = p[0x15] = first;
    p[0x16] = static_cast<unsigned char>(first >> 1);
    p[0x24] = p[0x25] = second;
    p[0x26] = static_cast<unsigned char>(second >> 1);
    p[0x34] = p[0x35] = second;
    p[0x36] = static_cast<unsigned char>(second >> 1);
    SH_CALL(Gfx_CommitPrim)(1, 0x44);
}

// original 0x470E70 (0xFA bytes): the five segments of 0x675FE0 set - thirty
// dword stores of constants, each segment's two points (the dwords +0xC and
// +0x1C left as they are).
extern "C" void __cdecl EffectKind29_SetSegments(void) {
    struct Cell { U at, value; };
    static const Cell kCells[] = {
        {::bof3::Img(0x675FE4), 0x118000},  {::bof3::Img(0x676004), 0x118000},  {::bof3::Img(0x675FE0), 0x568000},  {::bof3::Img(0x676000), 0x568000},
        {::bof3::Img(0x676008), 0x2400000}, {::bof3::Img(0x676018), 0x2400000}, {::bof3::Img(0x676020), 0x568000},  {::bof3::Img(0x676040), 0x568000},
        {::bof3::Img(0x676060), 0x568000},  {::bof3::Img(0x675FE8), 0x3000000}, {::bof3::Img(0x675FF0), 0x5E8000},  {::bof3::Img(0x675FF4), 0xF0000},
        {::bof3::Img(0x675FF8), 0x4000000}, {::bof3::Img(0x676010), 0x5E8000},  {::bof3::Img(0x676014), 0xF0000},   {::bof3::Img(0x676024), 0x138000},
        {::bof3::Img(0x676028), 0x3000000}, {::bof3::Img(0x676030), 0x5E8000},  {::bof3::Img(0x676034), 0x158000},  {::bof3::Img(0x676038), 0x4000000},
        {::bof3::Img(0x676044), 0x138000},  {::bof3::Img(0x676048), 0x2800000}, {::bof3::Img(0x676050), 0x5E8000},  {::bof3::Img(0x676054), 0x160000},
        {::bof3::Img(0x676058), 0x2C00000}, {::bof3::Img(0x676064), 0x138000},  {::bof3::Img(0x676068), 0x1200000}, {::bof3::Img(0x676070), 0x5E8000},
        {::bof3::Img(0x676074), 0x160000},  {::bof3::Img(0x676078), 0x1200000},
    };
    for (const Cell& c : kCells) SetUL(At(c.at), c.value);
}

// original 0x470F70 (0x126 bytes): a draw mode (0x3C0, 0, transparency 1)
// committed 0xC in slot 1, EffectGte_LoadMapCamera; for each of the five
// segments (A at +0, B at +0x10), a semi-transparent Gouraud line at the packet
// cursor from A's screen point to that of A + ((B - A) * t sar 4) (each
// coordinate, 32 bits), shaded ((0x20 + the frame's bit) * shade * 4) >> 8 at A
// and (the frame's bit * 4 * shade) >> 8 at the far end, blue at half;
// committed 0x24 in slot 1. t and shade are read as bytes.
extern "C" void __cdecl EffectKind29_DrawSegments(unsigned t, unsigned shade) {
    DrawMode(1, 0x3C0, 0, 1);
    SH_CALL(Gfx_CommitPrim)(1, 0xC);
    SH_CALL(EffectGte_LoadMapCamera)();
    const U tt = t & 0xFFu, level = shade & 0xFFu;
    for (unsigned k = 0; k < at::kSegmentCount; ++k) {
        const unsigned char* const a = At(::bof3::Img(Effect2a_Segments_addr) + at::kSegmentStride * k);
        const unsigned char* const b = a + 0x10;
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetLineG2)(p);
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        long point[3];
        for (unsigned i = 0; i < 3; ++i)
            point[i] = static_cast<long>(Sar((UL(b + 4 * i) - UL(a + 4 * i)) * tt, 4) + UL(a + 4 * i));
        SH_CALL(EffectGte_ProjectPoint)(reinterpret_cast<const long*>(a), reinterpret_cast<float*>(p + 8));
        SH_CALL(EffectGte_ProjectPoint)(point, reinterpret_cast<float*>(p + 0x18));
        const U bit = Frame_Counter & 1u;
        const auto near = static_cast<unsigned char>((((bit + 0x20u) * level) << 2) >> 8);
        const auto far = static_cast<unsigned char>(((bit << 2) * level) >> 8);
        p[4] = p[5] = near;
        p[0x14] = p[0x15] = far;
        p[6] = static_cast<unsigned char>(near >> 1);
        p[0x16] = static_cast<unsigned char>(far >> 1);
        SH_CALL(Gfx_CommitPrim)(1, 0x24);
    }
}

void EffectKind29_Inject() {
    BOF3_INJECT(EffectKind29_Run);
    BOF3_INJECT(EffectKind29_Start);
    BOF3_INJECT(EffectKind29_Extend);
    BOF3_INJECT(EffectKind29_Hold);
    BOF3_INJECT(EffectKind29_Fade);
    BOF3_INJECT(EffectKind29_Linger);
    BOF3_INJECT(EffectKind29_DrawBoxes);
    BOF3_INJECT(EffectKind29_DrawFace);
    BOF3_INJECT(EffectKind29_SetSegments);
    BOF3_INJECT(EffectKind29_DrawSegments);
}

// Round thirteen group E5D (docs/effect_5d.md): the 49 functions of
// analysis/round13_cut.tsv's group E5D and three starts the cut does not list
// (sub-kind 0x17's texture scroll 0x503E50, code no list has that E5C's states
// tail-jump to, and the dispatchers of sub-kinds 0x1C 0x505100 and 0x1D
// 0x505540, EffectKind18_States' entries 28 and 29, catalog rows of another
// part that no group of the round holds), each read with capstone to its last
// instruction. Effect_RunObjects (ours) makes each live record of
// Effect_Objects (20 of 0x80 bytes) Sprite_Current and calls
// Effect_KindHandlers[+5]; kind 0x18 jumps through EffectKind18_States by +1
// to a sub-kind, whose dispatcher here jumps through its own table by +2 (none
// bounded by a compare). What each sub-kind is, as far as the code says:
//
//   0x17 (E5C's)  helpers only: a 4 x 4 patch of textured quads laid on the
//                 ground's elevation, a VRAM frame copy, a 16 x 32 texture
//                 scrolled by the record's point, and the closing state
//   0x18          a column of eight textured quads and a ring of 64 dots that
//                 grow, hold and shrink round the record's point, then release
//   0x19          a fan of sixteen shaded quads turning in the camera's frame,
//                 placed from a two-entry table, drawn when the camera is near
//   0x1B          a timer: two MoveCmd_TestFB calls, then story flag 0x1C
//                 cleared and the record released
//   0x1C, 0x1D    a column of eight shrinking squares rising from a map cell
//                 (0x1D's leaning along its direction), the cell's map byte
//                 set and cleared; 0x1D hurts a party member standing on the
//                 two cells ahead (Field_FloorHurt)
//   0x14, 0x1E    a CLUT strip dimmed once area flag 0x3E is set; then a
//                 checkerboard of shaded squares over one of two map
//                 rectangles, two effect records (kinds 0x6E, 0x6D) spawned
//                 when a flag of the rectangle's is set, fading
//   0x21          a screen-centred spiral of a POLY_G3 and two POLY_G4s
//   0x22          a platform of up to 25 textured pieces over one of four map
//                 rectangles that sinks and rises with a story flag, writing
//                 the map's bytes, corner dwords and heights as it moves
//   0x43          a textured quad that glows while the counter 0x903848 is
//                 0xE, then waits for 0x1C and releases
//   0x52          sixteen rings of sixteen dots round the record's point,
//                 brightening, when the camera is within 20 cells
//   0x68          a field of 256 dots in eight arcs, when the camera is near
//                 (0x2D, 0x23)
//
// Every call goes through the harness (SH_CALL), so the start-up fuzz can
// stand recorders in for ours as for the originals' copies. No divergence:
// each is a faithful replacement. Where the original jumps through a state
// table past its end, indexes one of the image's tables past it by a record
// byte, or divides by a count that can be 0, ours aborts with a message
// (docs/effect_5d.md section 7).
#include "game/effect_5d.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_5d_callees.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_5d::at;
using U = std::uint32_t;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

unsigned char* S() { return Sprite_Current; }
U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }
U UL(U a) { return UL(At(a)); }
void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }
U W(const unsigned char* p) { return Word(p); }
U W(U a) { return Word(At(a)); }
unsigned char& B(U a) { return *At(a); }
std::int32_t I(U v) { return static_cast<std::int32_t>(v); }
std::int32_t S16(U v) { return static_cast<std::int16_t>(v & 0xFFFFu); }
std::int32_t S8(unsigned char v) { return static_cast<signed char>(v); }
U Abs(std::int32_t v) { return static_cast<U>(v < 0 ? -v : v); }   // cdq; xor; sub (small values here)
U AddressOf(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
U FC() { return Frame_Counter; }
float Fl(U a) {
    float f;
    std::memcpy(&f, At(a), sizeof f);
    return f;
}
long double SX() { return Fl(AddressOf(MapView_ScreenXY)); }   // fld / fadd dword [0x903820]
long double SY() { return Fl(at::kScreenY); }
void StoreFloat(unsigned char* p, long double v) {
    const float f = static_cast<float>(v);   // fstp dword
    std::memcpy(p, &f, sizeof f);
}
// The CRT's _ftol 0x5B9550 on the value x87 holds: truncation through a 64-bit
// fistp, whose NaN and out-of-range answer is the integer indefinite
// 0x8000000000000000; the callers keep eax's low word.
U Ftol(long double v) {
    if (!(v > -9.2233720368547758e18L && v < 9.2233720368547758e18L)) return 0;
    return static_cast<U>(static_cast<std::uint64_t>(static_cast<std::int64_t>(v)));
}

unsigned char* Vertex() { return reinterpret_cast<unsigned char*>(Prim_VertexScratch); }
void SetV(unsigned offset, U v) { SetWord(Vertex() + offset, v); }
const short* Vx(unsigned i) { return reinterpret_cast<const short*>(Vertex() + 8 * i); }
float* Fp(unsigned char* p) { return reinterpret_cast<float*>(p); }
unsigned long* ScreenXY() { return reinterpret_cast<unsigned long*>(MapView_ScreenXY); }

using Handler = scenario_harness::Handler;

// A sub-kind's state table: its address and its own length (symbols.toml [[data]]).
#define E5D_TABLE(name) AddressOf(name), name##_count

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 2]; jmp (or call)
// [table + eax * 4]: the table's `entries` handlers read in place (the fuzz
// swaps the cells for recorders); a Fatal past them, where the original jumps
// through the dword after - the next table or data.
void Dispatch(const char* who, U table, unsigned entries) {
    const unsigned state = Sprite_Current[2];
    if (state >= entries)
        bof3::Fatal("%s: sub-state byte +2 is %u, past the %u entries of 0x%X - the original jumps through the dword "
                    "after (docs/effect_5d.md section 7)",
                    who, state, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(table + 4 * state)))();
}

// An index into one of the image's tables by a record word or byte, checked:
// past it the original reads the data after (another table or code pointers).
U Index(const char* who, std::int32_t index, unsigned count, U table) {
    if (index < 0 || static_cast<unsigned>(index) >= count)
        bof3::Fatal("%s: index %d, past the %u entries of the table at 0x%X - the original reads the data after "
                    "(docs/effect_5d.md section 7)",
                    who, (int)index, count, (unsigned)table);
    return static_cast<U>(index);
}
// The record's x cell word (+0x36, signed), the index the 0x14 / 0x1E and 0x22
// sub-kinds' tables take.
U Place(const char* who, unsigned count, U table) { return Index(who, S16(W(S() + 0x36)), count, table); }

// An effect record by Effect_FindFree's answer (not 0xFF).
unsigned char* Record(const char* who, U index) {
    if (index >= at::kEffectCount)
        bof3::Fatal("%s: Effect_FindFree answered %u, past the 20 records - the original writes past Effect_Objects "
                    "(docs/effect_5d.md section 7)",
                    who, (unsigned)index);
    return Effect_Objects + at::kEffectStride * index;
}

// The x87's quad corners about a screen point: the first float less the half
// size, the second that plus the whole size (no rounding between, as the
// original keeps both on the FPU stack).
long double Less(long double centre, long double half) { return centre - half; }

}  // namespace

// ===========================================================================
// Sub-kind 0x17's helpers (EffectKind18_States[23], E5C's dispatcher 0x503660
// and table 0x65E1A8; its states call these raw until this group merges)
// ===========================================================================

// original 0x503DE0 (0x65E1A8's entry 15, hidden in E5C's 0x5032C0): q = +9 / 5;
// at 4, MoveCmd_TestFC(0x43, 0x1A) (al unread) and Effect_Release; else on
// every fifth frame EffectKind18Sub17_CopyFrame(7 - q); +9 up one.
extern "C" void __cdecl EffectKind18Sub17_Close(void) {
    unsigned char* s = S();
    const U n = s[9];
    const U q = n / 5;
    if (q == 4) {
        SH_CALL(MoveCmd_TestFC)(0x43, 0x1A);
        SH_CALL(Effect_Release)();
        return;
    }
    if (n % 5 == 0) {
        SH_CALL(EffectKind18Sub17_CopyFrame)(7u - q);
        s = S();
    }
    s[9] = static_cast<unsigned char>(s[9] + 1);
}

// original 0x503E50 (code no list has, after 0x503DE0; E5C's five states
// 0x5036D0..0x5038A0 tail-jump to it - its own frame and ret): a 16 x 32 block
// of VRAM at (0x260, 0x100) scrolled by the record's point - u = bits 13..16 of
// +0x34 with bit 16 flipped, v = bits 12..16 of +0x38 - as four
// Gpu_SetDrawMove copies from (0x170, 0x1A0), each committed to slot 6 (0x18).
extern "C" void __cdecl EffectKind18Sub17_ScrollTexture(void) {
    unsigned char* const s = S();
    const U u = (static_cast<U>(I(UL(s + 0x34) ^ 0xFFFF1FFFu) >> 13)) & 0xF;
    const U v = static_cast<U>(I(UL(s + 0x38)) >> 12) & 0x1F;
    unsigned char rect[8];
    auto copy = [&rect](U x, U y, U w, U h, U to_x, U to_y) {
        unsigned char* const p = Gfx_PacketNext;
        SetWord(rect, x);
        SetWord(rect + 2, y);
        SetWord(rect + 4, w);
        SetWord(rect + 6, h);
        SH_CALL(Gpu_SetDrawMove)(p, rect, to_x, to_y);
        SH_CALL(Gfx_CommitPrim)(6, 0x18);
    };
    copy(0x170, 0x1A0, u, v, 0x270 - u, 0x120 - v);
    copy(u + 0x170, 0x1A0, 0x10 - u, v, 0x260, 0x120 - v);
    copy(0x170, v + 0x1A0, u, 0x20 - v, 0x270 - u, 0x100);
    copy(u + 0x170, v + 0x1A0, 0x10 - u, 0x20 - v, 0x260, 0x100);
}

// original 0x503FA0 (cdecl; E5C's states call it 15 times with 0, 1 or 2):
// with Draw_PassFlags bit 2 set (nothing otherwise), MapView_ScreenXY the
// record's point less 0x4000 cells (>> 9), and sixteen textured quads in a
// 4 x 4 grid of 128-unit cells about it, each corner on the ground
// (AreaMap_Elevation, halved) less the record's +0x32 scaled by a height byte
// of the quad's record in 0x65E1E8; its flag byte picks the corner order and
// the depth (Gte_StoreDepthF4 or Gte_PrimDepths4_10); the texture its flag,
// the variant's byte and page 0x2180.. (variant 0) or 0x2580..; a draw mode
// and the quad linked on the record's row (dy (i & 3) + (i >> 2) - 1).
extern "C" void __cdecl EffectKind18Sub17_DrawPatch(U variant) {
    if ((Draw_PassFlags & 4) == 0) return;
    unsigned char* s = S();
    const std::int32_t x0 = (I(UL(s + 0x34)) >> 9) - 0x4000;
    StoreFloat(reinterpret_cast<unsigned char*>(MapView_ScreenXY), static_cast<long double>(x0));
    const std::int32_t z0 = (I(UL(s + 0x38)) >> 9) - 0x4000;
    StoreFloat(At(at::kScreenY), static_cast<long double>(z0));
    if (variant >= at::kPatchVariants)
        bof3::Fatal("EffectKind18Sub17_DrawPatch: variant %u, past the three bytes of each 0x65E1E8 record - the original "
                    "reads the next record's (docs/effect_5d.md section 7)",
                    (unsigned)variant);
    const U page = variant == 0 ? 0x21800000u : 0x25800000u;
    const long double half = Fl(at::kHalfCell);
    for (U i = 0; i < at::kPatchCount; ++i) {
        const U rec = at::kPatch + at::kPatchStride * i;
        SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x95, 0);
        const U row = i >> 2;
        const U dy = ((i & 3) + row - 1) & 0xFF;
        s = S();
        SH_CALL(MapView_LinkPrimAt)(UL(s + 0x34), UL(s + 0x38), static_cast<int>(dy), 0xC);
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyFT4)(p);
        SH_CALL(Gpu_SetShadeTex)(p, 0);
        const long double fx = static_cast<float>(static_cast<std::int32_t>((i & 3) << 7));
        const long double fy = static_cast<float>(static_cast<std::int32_t>(row << 7));
        // each corner: x and y about the cell's centre, z the ground less the height byte's share
        auto corner = [&](unsigned k, long double sx, long double sy, U height_byte) {
            const U x = Ftol(sx) & 0xFFFF;
            SetV(8 * k, x);
            const U y = Ftol(sy) & 0xFFFF;
            SetV(8 * k + 2, y);
            const U e = static_cast<U>(SH_CALL(AreaMap_Elevation)(static_cast<long>((static_cast<U>(S16(x) + 0x4000)) << 9),
                                                                  static_cast<long>((static_cast<U>(S16(y) + 0x4000)) << 9)));
            const std::int32_t h = S16(W(S() + 0x32));
            const std::int32_t lift = I(B(height_byte) * static_cast<U>(h)) / 128;
            SetV(8 * k + 4, static_cast<U>(-(S16(e) / 2) - lift));
        };
        corner(0, fx + SX() - half, fy + SY() - half, rec);
        corner(1, fx + SX() + half, fy + SY() - half, rec + 1);
        corner(2, fx + SX() - half, fy + SY() + half, rec + 2);
        corner(3, fx + SX() + half, fy + SY() + half, rec + 3);
        const unsigned char flag = B(rec + 4);
        long depth[2];
        if (flag != 0) {
            SH_CALL(Gte_RotTransPers4)(Vx(0), Vx(1), Vx(2), Vx(3), Fp(p + 0x18), Fp(p + 8), Fp(p + 0x38), Fp(p + 0x28), depth);
            SH_CALL(Gte_StoreDepthF4)(Fp(p + 0x20), Fp(p + 0x10), Fp(p + 0x40), Fp(p + 0x30));
        } else {
            SH_CALL(Gte_RotTransPers4)(Vx(0), Vx(1), Vx(2), Vx(3), Fp(p + 8), Fp(p + 0x18), Fp(p + 0x28), Fp(p + 0x38), depth);
            SH_CALL(Gte_PrimDepths4_10)(p);
        }
        SH_CALL(Prim_SetTexture)((static_cast<U>(B(rec + 4)) << 16) | B(rec + 5 + variant) | page, p, 1);
        s = S();
        SH_CALL(MapView_LinkPrimAt)(UL(s + 0x34), UL(s + 0x38), static_cast<int>(dy), 0x48);
    }
}

// original 0x5043B0 (cdecl; 0x503DE0 and E5C's states): a 32 x 64 block of VRAM
// copied from (0x240, 0x100) + frame `frame`'s (u, v) byte pair (0x65E268, ten)
// to ((+0xB + 12) * 48, 0x100), committed to slot 6 (0x18). The whole word is
// the index: past the ten (or below 0) the original reads the data after.
extern "C" void __cdecl EffectKind18Sub17_CopyFrame(U frame) {
    unsigned char rect[8];
    SetWord(rect + 4, 0x20);
    SetWord(rect + 6, 0x40);
    const U f = Index("EffectKind18Sub17_CopyFrame", I(frame), at::kFrameCount, at::kFrames);
    SetWord(rect, B(at::kFrames + 2 * f) + 0x240u);
    SetWord(rect + 2, B(at::kFrames + 2 * f + 1) + 0x100u);
    const U to_x = (S()[0xB] + 0xCu) * 0x30u;
    SH_CALL(Gpu_SetDrawMove)(Gfx_PacketNext, rect, to_x, 0x100);
    SH_CALL(Gfx_CommitPrim)(6, 0x18);
}

// ===========================================================================
// Sub-kind 0x18: EffectKind18_States[24] (0x6540CC), EffectKind18Sub18_States
// (four) by +2
// ===========================================================================

// original 0x504430 (EffectKind18_States[24], hidden in 0x5043B0):
// jmp [EffectKind18Sub18_States + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub18_Run(void) {
    Dispatch("EffectKind18Sub18_Run", E5D_TABLE(EffectKind18Sub18_States));
}

// original 0x504450 (sub-state 0): the column at +9 (0x80), the ring at +9; +9
// up one; past 7, +2 up.
extern "C" void __cdecl EffectKind18Sub18_Grow(void) {
    SH_CALL(EffectKind18Sub18_DrawColumn)(S()[9], 0x80);
    SH_CALL(EffectKind18Sub18_DrawRing)(S()[9]);
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] + 1);
    s = S();
    if (s[9] > 7) s[2] = static_cast<unsigned char>(s[2] + 1);
}

// original 0x5044A0 (sub-state 1): the column at 7; below 0x10 the ring at +9;
// +9 up one; past 0x1F, +9 0 and +2 up.
extern "C" void __cdecl EffectKind18Sub18_Hold(void) {
    SH_CALL(EffectKind18Sub18_DrawColumn)(7, 0x80);
    unsigned char* s = S();
    const unsigned char n = s[9];
    if (n < 0x10) {
        SH_CALL(EffectKind18Sub18_DrawRing)(n);
        s = S();
    }
    s[9] = static_cast<unsigned char>(s[9] + 1);
    s = S();
    if (s[9] > 0x1F) {
        s[9] = 0;
        S()[2] = static_cast<unsigned char>(S()[2] + 1);
    }
}

// original 0x5044F0 (sub-state 2): t = 0x1F - +9: the column at (t / 4, 4 t);
// +9 up one; past 0x18, +9 0 and +2 up.
extern "C" void __cdecl EffectKind18Sub18_Shrink(void) {
    const std::int32_t t = 0x1F - static_cast<std::int32_t>(S()[9]);
    SH_CALL(EffectKind18Sub18_DrawColumn)(static_cast<U>(t / 4), static_cast<U>(t) * 4);
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] + 1);
    s = S();
    if (s[9] > 0x18) {
        s[9] = 0;
        S()[2] = static_cast<unsigned char>(S()[2] + 1);
    }
}

// original 0x504550 (sub-state 3): +9 up one; past 0x1C a tail jump to
// Effect_Release.
extern "C" void __cdecl EffectKind18Sub18_End(void) {
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] + 1);
    s = S();
    if (s[9] > 0x1C) SH_CALL(Effect_Release)();
}

// original 0x504570 (cdecl): eight textured quads stacked over the record's
// cell (x, z = (cell - 0x80) << 7, y half the ground's height up), each level
// 8 `grow` higher and 2 `grow` wider than the last (the start 8 + grow
// (Frame_Counter & 1) wider still), texture 0xBA009124 | (level) << 19, linked
// on the record's row (dy 2). The second argument is read only to compute a
// value the code stores back into its own stack slot and reads nowhere else:
// it changes nothing (docs/effect_5d.md section 1.2).
extern "C" void __cdecl EffectKind18Sub18_DrawColumn(U grow, U unused) {
    (void)unused;
    unsigned char* s = S();
    const U x = static_cast<U>(S16(W(s + 0x36)) - 0x80) << 7;
    const U z = static_cast<U>(S16(W(s + 0x3A)) - 0x80) << 7;
    const U e = static_cast<U>(SH_CALL(AreaMap_Elevation)(static_cast<long>(UL(s + 0x34)), static_cast<long>(UL(s + 0x38))));
    const U y = static_cast<U>(-(S16(e) / 2));
    const U odd = (FC() & 1) << 2;
    U b = static_cast<U>(I(odd * grow) / 4) + 8;
    U level = 0, shade = 0x3F;
    std::int32_t c = 0x1F80000;
    do {
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyFT4)(p);
        SH_CALL(Gpu_SetShadeTex)(p, 0);
        SetV(0, x - b);
        SetV(2, z - b);
        SetV(4, y + (((2 - level) << 3) - odd) * grow);
        const U low = y - (odd + level * 8) * grow;
        SetV(8, x + b);
        SetV(0xA, z - b);
        SetV(0xC, low);
        SetV(0x10, x - b);
        SetV(0x12, z + b);
        SetV(0x14, low);
        SetV(0x18, x + b);
        SetV(0x1A, z + b);
        SetV(0x1C, y - (odd + level * 8 + 0x10) * grow);
        long depth[2];
        SH_CALL(Gte_RotTransPers4)(Vx(1), Vx(2), Vx(3), Vx(0), Fp(p + 0x18), Fp(p + 0x28), Fp(p + 0x38), Fp(p + 8), depth);
        SH_CALL(Gte_StoreDepthF4)(Fp(p + 0x20), Fp(p + 0x30), Fp(p + 0x40), Fp(p + 0x10));
        SH_CALL(Prim_SetTexture)((static_cast<U>(I(shade - odd) / 4) << 19) | 0xBA009124u, p, 1);
        s = S();
        SH_CALL(MapView_LinkPrimAt)(UL(s + 0x34), UL(s + 0x38), 2, 0x48);
        b += grow * 2;
        ++level;
        c -= 0x400000;
        shade -= 8;
    } while (c > static_cast<std::int32_t>(0xFFF80000u));
}

// original 0x5047A0 (cdecl): MapView_ScreenXY the record's point (its cell,
// half the ground's height up) projected; a draw mode (0xB5) linked on the
// record's row (dy 1); then 64 semi-transparent dots on a ring of radius
// 4 `radius` (x by Math_Sin >> 14, y by Math_Cos >> 15, 8 down), coloured
// (c, c on odd frames, (c >> 1) on odd frames) with c = ((4 radius) ^ 0x3F)
// << 1 (bytes), each linked on the record's row (dy 2).
extern "C" void __cdecl EffectKind18Sub18_DrawRing(U radius) {
    unsigned char* s = S();
    const U x = static_cast<U>(S16(W(s + 0x36)) - 0x80) << 7;
    const U z = static_cast<U>(S16(W(s + 0x3A)) - 0x80) << 7;
    const U e = static_cast<U>(SH_CALL(AreaMap_Elevation)(static_cast<long>(UL(s + 0x34)), static_cast<long>(UL(s + 0x38))));
    SetV(4, static_cast<U>(-(S16(e) / 2)));
    SetV(0, x);
    SetV(2, z);
    long depth[2];
    SH_CALL(Gte_RotTransPers)(Vx(0), ScreenXY(), depth);
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0xB5, 0);
    s = S();
    SH_CALL(MapView_LinkPrimAt)(UL(s + 0x34), UL(s + 0x38), 1, 0xC);
    const U r = radius << 2;
    const unsigned char blue = static_cast<unsigned char>((r & 0xFF) ^ 0x3F);
    const unsigned char red = static_cast<unsigned char>(blue << 1);
    for (U j = 0; j < 0x40; ++j) {
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetTile1)(p);
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        SH_CALL(Gte_StoreDepthF)(Fp(p + 0x10));
        const U angle = j << 7;
        const U sn = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(angle)));
        const std::int32_t dx = I(sn * r) >> 14;
        StoreFloat(p + 8, static_cast<long double>(dx) + SX());
        const U cs = static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(angle)));
        const std::int32_t dy = I(cs * r) >> 15;
        p[4] = red;
        StoreFloat(p + 0xC, static_cast<long double>(dy) + SY() + Fl(at::kRingDrop));
        p[5] = static_cast<unsigned char>((FC() & 1) * red);
        p[6] = static_cast<unsigned char>((FC() & 1) * blue);
        s = S();
        SH_CALL(MapView_LinkPrimAt)(UL(s + 0x34), UL(s + 0x38), 2, 0x14);
    }
}

// ===========================================================================
// Sub-kind 0x19: EffectKind18_States[25] (0x6540D0), one state
// ===========================================================================

namespace {

// 0x5049D0 (jumped to by 0x504900 only; its own frame, the function's tail):
// in the camera's frame turned about z by 0x40 - |0x80 - 2 (Frame_Counter &
// 0x7F)| and moved to the record's point (Gte_RotTrans), sixteen POLY_GT4
// strips side by side (x -12 i .. -12 (i + 1)), each edge waved by Math_Sin of
// the frame, shaded by Math_Cos of it; between two draw modes (slot 4), the
// GTE matrix pushed and popped.
void Sub19Draw() {
    SH_CALL(Gte_PushMatrix)();
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x95, 0);
    SH_CALL(Gfx_CommitPrim)(4, 0xC);
    const std::int32_t turn = 0x40 - static_cast<std::int32_t>(Abs(0x80 - static_cast<std::int32_t>((FC() & 0x7F) << 1)));
    alignas(4) unsigned char angles[8] = {};
    alignas(4) unsigned char point[8] = {};
    alignas(4) unsigned char matrix[0x20] = {};
    SetWord(angles + 4, static_cast<U>(turn));
    unsigned char* const s = S();
    SetWord(point, W(s + 0x34));
    SetWord(point + 2, W(s + 0x38));
    SetWord(point + 4, W(s + 0x3C));
    SH_CALL(Gte_RotTrans)(reinterpret_cast<const short*>(point), reinterpret_cast<long*>(matrix + 0x14));
    SH_CALL(Gte_RotMatrix)(reinterpret_cast<const short*>(angles), reinterpret_cast<short*>(matrix));
    SH_CALL(Gte_MulMatrix0)(Camera_Matrix, reinterpret_cast<const short*>(matrix),
                            reinterpret_cast<short*>(matrix));
    SH_CALL(Gte_SetRotMatrix)(reinterpret_cast<const unsigned long*>(matrix));
    SH_CALL(Gte_SetTransMatrix)(reinterpret_cast<const unsigned long*>(matrix));
    auto shade = [](U angle, U times) {
        const U c = static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(angle)));
        return static_cast<unsigned char>((I(c * times) >> 11) + 0x80);
    };
    for (U i = 0; i < 0x10; ++i) {
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyGT4)(p);
        SH_CALL(Gpu_SetShadeTex)(p, 0);
        const unsigned char u0 = static_cast<unsigned char>(0x20 - 2 * i), u1 = static_cast<unsigned char>(0x1E - 2 * i);
        SetWord(p + 0x2A, 0x1B);
        SetWord(p + 0x16, 0x78CF);
        p[0x14] = u0;
        p[0x15] = 0xC0;
        p[0x28] = u1;
        p[0x29] = 0xC0;
        p[0x3C] = u0;
        p[0x3D] = 0xD8;
        p[0x50] = u1;
        p[0x51] = 0xD8;
        const U before = i != 0 ? i - 1 : 0;
        const unsigned char c0 = shade(((before + FC()) << 9) & 0xFFF, before);
        p[6] = c0;
        p[5] = c0;
        p[4] = c0;
        const unsigned char c1 = shade(((FC() + i) << 9) & 0xFFF, i);
        p[0x2E] = c1;
        p[0x2D] = c1;
        p[0x2C] = c1;
        p[0x1A] = c1;
        p[0x19] = c1;
        p[0x18] = c1;
        const U after = i + 1;
        const unsigned char c2 = shade(((FC() + i + 1) << 9) & 0xFFF, after);
        p[0x42] = c2;
        p[0x41] = c2;
        p[0x40] = c2;
        const U near_x = static_cast<U>(I(0u - ((3 * i) << 6)) / 16);
        SetV(0x10, near_x);
        SetV(0, near_x);
        const U far_x = static_cast<U>(I(((0xFFFFFFFFu - i) * 3) << 6) / 16);
        SetV(0x18, far_x);
        SetV(8, far_x);
        auto wave = [](U angle, U times) {
            const U v = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(angle)));
            return static_cast<U>(I(v * times) >> 13);
        };
        SetV(2, wave(((FC() - before) << 9) & 0xFFF, before));
        const U mid = wave(((FC() - i) << 9) & 0xFFF, i);
        SetV(0x12, mid);
        SetV(0xA, mid);
        SetV(0x1A, wave(((FC() - i - 1) << 9) & 0xFFF, after));
        SetV(0xC, 0xFF70);
        SetV(4, 0xFF70);
        SetV(0x1C, 0);
        SetV(0x14, 0);
        long depth[2];
        SH_CALL(Gte_RotTransPers4)(Vx(0), Vx(1), Vx(2), Vx(3), Fp(p + 8), Fp(p + 0x1C), Fp(p + 0x30), Fp(p + 0x44), depth);
        SH_CALL(Gte_PrimDepths4_14)(p);
        SH_CALL(Gfx_CommitPrim)(4, 0x54);
    }
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x95, 0);
    SH_CALL(Gfx_CommitPrim)(4, 0xC);
    SH_CALL(Gte_PopMatrix)();
}

}  // namespace

// original 0x504900 (EffectKind18_States[25], hidden in 0x5047A0): with +2 0,
// +0xB the x cell's low byte (+0x36) and the point placed from the tables
// 0x65E28C / 0x65E290 / 0x65E294 by it (x (t << 7) - 0x3FC8, z (t - 0x80) << 7,
// the height a word) and +2 up. Then, with Draw_PassFlags bit 2 set and the
// camera's cell within 10 of the place's (less 6, both axes), the fan
// (0x5049D0). The tables hold two places; +0xB past them reads the data after.
extern "C" void __cdecl EffectKind18Sub19_Run(void) {
    static const char kWho[] = "EffectKind18Sub19_Run";
    unsigned char* s = S();
    if (s[2] == 0) {
        s[0xB] = s[0x36];
        s = S();
        U k = Index(kWho, s[0xB], at::kSub19Count, at::kSub19X);
        SetUL(s + 0x34, (static_cast<U>(B(at::kSub19X + k)) << 7) - 0x3FC8u);
        s = S();
        k = Index(kWho, s[0xB], at::kSub19Count, at::kSub19Z);
        SetUL(s + 0x38, (static_cast<U>(B(at::kSub19Z + k)) - 0x80u) << 7);
        s = S();
        k = Index(kWho, s[0xB], at::kSub19Count, at::kSub19Height);
        SetUL(s + 0x3C, static_cast<U>(S16(W(at::kSub19Height + 2 * k))));
        s = S();
        s[2] = static_cast<unsigned char>(s[2] + 1);
        s = S();
    }
    if ((Draw_PassFlags & 4) == 0) return;
    const U k = Index(kWho, s[0xB], at::kSub19Count, at::kSub19X);
    if (Abs(S16(W(at::kCameraX)) - static_cast<std::int32_t>(B(at::kSub19X + k)) + 6) > 10) return;
    if (Abs(S16(W(at::kCameraZ)) - static_cast<std::int32_t>(B(at::kSub19Z + k)) + 6) > 10) return;
    Sub19Draw();
}

// ===========================================================================
// Sub-kind 0x68: EffectKind18_States[104] (0x65420C), one state
// ===========================================================================

// original 0x504CE0 (EffectKind18_States[104], hidden in 0x5047A0): with
// Draw_PassFlags bit 2 set and the camera within 10 cells of (0x2D, 0x23):
// MapView_ScreenXY (-10624.0, -11904.0); then eight arcs (angle a / 8 - 0x100,
// a = 0..0x7000 by 0x1000) of 32 semi-transparent dots each, at distance
// ((Frame_Counter & 0xF) + 16 k) * 5 / 65536 of Math_Sin / Math_Cos (unsigned)
// from it, z -0x140 less Math_Sin(5 that) >> 5, through the GTE's own
// projection (Gte_LoadVertex, Gte_Rtps), shaded by k and the frame, each
// linked on the record's row (dy 1); a draw mode (0xB5) before every fourth arc.
extern "C" void __cdecl EffectKind18Sub68_Draw(void) {
    if ((Draw_PassFlags & 4) == 0) return;
    if (Abs(S16(W(at::kCameraX)) - 0x2D) > 10) return;
    if (Abs(S16(W(at::kCameraZ)) - 0x23) > 10) return;
    SetUL(reinterpret_cast<unsigned char*>(MapView_ScreenXY), 0xC6260000u);
    SetUL(At(at::kScreenY), 0xC63A0000u);
    U arc = 0;
    for (U a = 0; a < 0x8000; a += 0x1000, ++arc) {
        const U first = (arc & 0xFFFFFFF8u) == 0 ? 1 : 0;
        if ((arc & 3) == 0) {
            SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0xB5, 0);
            unsigned char* const s = S();
            SH_CALL(MapView_LinkPrimAt)(UL(s + 0x34), UL(s + 0x38), static_cast<int>(first), 0xC);
        }
        const U angle = static_cast<U>(I(a) / 8) + 0xFFFFFF00u;
        U k = 0;
        for (std::int32_t step = 0; step > -0x100; step -= 8, k += 0x10) {
            unsigned char* p = Gfx_PacketNext;
            SH_CALL(Gpu_SetTile1)(p);
            SH_CALL(Gpu_SetSemiTrans)(p, 1);
            const U blue = (static_cast<U>(step) - (FC() & 7) + 0x17F) >> 1;
            p[6] = static_cast<unsigned char>(blue);
            const unsigned char other = static_cast<unsigned char>(I(blue) / 4);
            p[4] = other;
            p[5] = other;
            const U sn = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(angle)));
            const U frame = FC();
            const U reach = (frame & 0xF) + k;
            const U dx = (sn * reach * 5) >> 16;
            SetV(0, Ftol(static_cast<long double>(dx) + SX()) & 0xFFFF);
            const U cs = static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(angle)));
            const U dz = (cs * reach * 5) >> 16;
            SetV(2, Ftol(static_cast<long double>(dz) + SY()) & 0xFFFF);
            const U lift = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(reach * 5)));
            SetV(4, 0xFFFFFEC0u - static_cast<U>(I(lift) >> 5));
            SH_CALL(Gte_LoadVertex)(reinterpret_cast<const unsigned long*>(Vertex()));
            SH_CALL(Gte_Rtps)();
            SH_CALL(Gte_StoreScreenXY)(reinterpret_cast<unsigned long*>(p + 8));
            p += 0x10;
            SH_CALL(Gte_StoreDepthF)(Fp(p));
            unsigned char* const s = S();
            SH_CALL(MapView_LinkPrimAt)(UL(s + 0x34), UL(s + 0x38), static_cast<int>(first), 0x14);
        }
    }
}

// ===========================================================================
// Sub-kind 0x1B: EffectKind18_States[27] (0x6540D8), one state
// ===========================================================================

// original 0x504F00 (EffectKind18_States[27], hidden in 0x5047A0): +9 up one;
// at 1, MoveCmd_TestFB(cell x + 1, cell z); at 5, MoveCmd_TestFB(cell x, cell
// z) (their al unread); at 0xF, story flag 0x1C cleared and a tail jump to
// Effect_Release.
extern "C" void __cdecl EffectKind18Sub1B_Run(void) {
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] + 1);
    s = S();
    if (s[9] == 1) {
        SH_CALL(MoveCmd_TestFB)(static_cast<short>(W(s + 0x36) + 1), static_cast<short>(W(s + 0x3A)));
        s = S();
    }
    if (s[9] == 5) {
        SH_CALL(MoveCmd_TestFB)(static_cast<short>(W(s + 0x36)), static_cast<short>(W(s + 0x3A)));
        s = S();
    }
    if (s[9] == 0xF) {
        SH_CALL(Flags_Clear)(At(at::kStoryFlags), 0x1C);
        SH_CALL(Effect_Release)();
    }
}

// ===========================================================================
// Sub-kind 0x43: EffectKind18_States[67] (0x654178), EffectKind18Sub43_States
// (nine by +2; entries 4..8 are sub-kind 0x1C's table)
// ===========================================================================

// original 0x504F70 (EffectKind18_States[67], hidden in 0x5047A0):
// jmp [EffectKind18Sub43_States + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub43_Run(void) {
    Dispatch("EffectKind18Sub43_Run", E5D_TABLE(EffectKind18Sub43_States));
}

// original 0x504F90 (sub-state 1): while the counter 0x903848 is 0xE, the quad
// at (+9, +9); +9 up one; past 0x6C, Cond_ByteFE 1, the counter 0xF and +2 up.
extern "C" void __cdecl EffectKind18Sub43_Glow(void) {
    if (B(at::kCounter) != 0xE) return;
    const unsigned char n = S()[9];
    SH_CALL(EffectKind18Sub43_DrawQuad)(n, n);
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] + 1);
    s = S();
    if (s[9] > 0x6C) {
        Cond_ByteFE = 1;
        B(at::kCounter) = 0xF;
        s[2] = static_cast<unsigned char>(s[2] + 1);
    }
}

// original 0x504FE0 (sub-state 3): the counter at 0x1C: Cond_ByteFE 0 and a tail
// jump to Effect_Release.
extern "C" void __cdecl EffectKind18Sub43_End(void) {
    if (B(at::kCounter) != 0x1C) return;
    Cond_ByteFE = 0;
    SH_CALL(Effect_Release)();
}

// original 0x505000 (cdecl): one textured quad (x (-0xD40 - g) * 4 .. 4 g -
// 0x3500, y (-0x7C0 - g) * 4 .. 4 g - 0x1F00, z -0x80, low words), depths
// Gte_PrimDepths4_10, texture 0x25100100 + ((shade & 0xF8) << 16), committed to
// slot 5 (0x48).
extern "C" void __cdecl EffectKind18Sub43_DrawQuad(U grow, U shade) {
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyFT4)(p);
    SH_CALL(Gpu_SetShadeTex)(p, 0);
    const U x0 = (0xFFFFF2C0u - grow) << 2, x1 = grow * 4 - 0x3500;
    const U y0 = (0xFFFFF840u - grow) << 2, y1 = grow * 4 - 0x1F00;
    SetV(0x10, x0);
    SetV(0, x0);
    SetV(0x18, x1);
    SetV(8, x1);
    SetV(0xA, y0);
    SetV(2, y0);
    SetV(0x1A, y1);
    SetV(0x12, y1);
    SetV(0x1C, 0xFF80);
    SetV(0x14, 0xFF80);
    SetV(0xC, 0xFF80);
    SetV(4, 0xFF80);
    long depth[2];
    SH_CALL(Gte_RotTransPers4)(Vx(0), Vx(1), Vx(2), Vx(3), Fp(p + 8), Fp(p + 0x18), Fp(p + 0x28), Fp(p + 0x38), depth);
    SH_CALL(Gte_PrimDepths4_10)(p);
    SH_CALL(Prim_SetTexture)(((shade & 0xF8) << 16) + 0x25100100u, p, 1);
    SH_CALL(Gfx_CommitPrim)(5, 0x48);
}

// ===========================================================================
// Sub-kinds 0x1C and 0x1D: EffectKind18_States[28] / [29] (0x6540DC /
// 0x6540E0), EffectKind18Sub1C_States / EffectKind18Sub1D_States (five each)
// ===========================================================================

// original 0x505100 (EffectKind18_States[28], hidden in 0x505000; a catalog row
// of part 2 that no group of the round holds): jmp [EffectKind18Sub1C_States +
// +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub1C_Run(void) {
    Dispatch("EffectKind18Sub1C_Run", E5D_TABLE(EffectKind18Sub1C_States));
}

// original 0x505120 (sub-state 0; sub-kind 0x43's 4): +0x3E the ground's height
// at the point; +0xA the wait 0x65E2DC gives the cell's parity (bit 0 of z's
// and x's cell), +9 (Rand & 0x2F) + it; the cell's map byte 0; +2 up.
extern "C" void __cdecl EffectKind18Sub1C_Start(void) {
    unsigned char* s = S();
    const U e = static_cast<U>(SH_CALL(AreaMap_Elevation)(static_cast<long>(UL(s + 0x34)), static_cast<long>(UL(s + 0x38))));
    SetWord(S() + 0x3E, e);
    s = S();
    s[0xA] = B(at::kSub1CWaits + (((s[0x3A] & 1u) << 1) | (s[0x36] & 1u)));
    const U r = static_cast<U>(SH_CALL(Rand)());
    s = S();
    s[9] = static_cast<unsigned char>((r & 0x2F) + s[0xA]);
    s = S();
    SH_CALL(AreaMap_SetByte)(W(s + 0x36), W(s + 0x3A), 0);
    s = S();
    s[2] = static_cast<unsigned char>(s[2] + 1);
}

namespace {

// Sub-kinds 0x1C / 0x1D's sub-state 1: Field_Request 0: +9 down one; at 0, +2 up
// and, the record on screen, Sound_PlayEffect(0x200).
void WaitThenSound() {
    if (Field_Request != 0) return;
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    s = S();
    if (s[9] != 0) return;
    s[2] = static_cast<unsigned char>(s[2] + 1);
    if (SH_CALL(EffectKind18Sub1C_OnScreen)() != 0) SH_CALL(Sound_PlayEffect)(0x200);
}

}  // namespace

// original 0x5051A0 (sub-state 1): WaitThenSound.
extern "C" void __cdecl EffectKind18Sub1C_Wait(void) { WaitThenSound(); }

// original 0x5051E0 (sub-state 2): the column at +9; +9 up one; past 7 the
// cell's map byte 0x8A, +9 0, +2 up.
extern "C" void __cdecl EffectKind18Sub1C_Rise(void) {
    SH_CALL(EffectKind18Sub1C_Draw)(S()[9]);
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] + 1);
    s = S();
    if (s[9] > 7) {
        SH_CALL(AreaMap_SetByte)(W(s + 0x36), W(s + 0x3A), 0x8A);
        S()[9] = 0;
        S()[2] = static_cast<unsigned char>(S()[2] + 1);
    }
}

// original 0x505240 (sub-state 3): the column at 7; +9 up one; past 0x1E the
// cell's map byte 0, +9 0x1F, +2 up.
extern "C" void __cdecl EffectKind18Sub1C_Hold(void) {
    SH_CALL(EffectKind18Sub1C_Draw)(7);
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] + 1);
    s = S();
    if (s[9] > 0x1E) {
        SH_CALL(AreaMap_SetByte)(W(s + 0x36), W(s + 0x3A), 0);
        S()[9] = 0x1F;
        S()[2] = static_cast<unsigned char>(S()[2] + 1);
    }
}

// original 0x505290 (sub-state 4): the column at +9 >> 2; +9 down one; below 8,
// +9 the wait +0xA and +2 1.
extern "C" void __cdecl EffectKind18Sub1C_Fall(void) {
    SH_CALL(EffectKind18Sub1C_Draw)(S()[9] >> 2);
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    s = S();
    if (s[9] < 8) {
        s[9] = s[0xA];
        S()[2] = 1;
    }
}

namespace {

// Sub-kinds 0x1C / 0x1D's column (0x5052D0 / 0x5057D0, cdecl `rise`): eight
// semi-transparent squares from the record's cell, each a screen point
// (Gte_RotTransPers) with Gte_PrimDepthFlat4_10, half size (level or 4 past the
// fourth) * rise / 2 + (Frame_Counter & 1) * 4 rise / 16 + 4, texture
// 0xBA009124 | ((0x1F80000 - level 0x400000 - (Frame_Counter & 1) << 21) / 4 /
// (8 - rise)) & 0xF80000; the vertex `place` sets. The original divides by
// 8 - rise unchecked: 8 faults it, ours aborts.
template <typename Place>
void Column(const char* who, U rise, U row, Place place) {
    unsigned char* s = S();
    const U frame = FC();
    const U x0 = static_cast<U>(S16(W(s + 0x36)) - 0x80) << 7;
    const U z0 = static_cast<U>(S16(W(s + 0x3A)) - 0x80) << 7;
    const U y0 = static_cast<U>(-(S16(W(s + 0x3E)) / 2));
    const U odd = (frame & 1) << 2;
    const U spread = static_cast<U>(I(odd * rise) / 16);
    const std::int32_t divisor = static_cast<std::int32_t>(8 - rise);
    const U page = odd << 19;
    U level = 0;
    std::int32_t c = 0x1F80000;
    do {
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyFT4)(p);
        SH_CALL(Gpu_SetShadeTex)(p, 0);
        place(level, x0, z0, y0, odd);
        long depth[2];
        SH_CALL(Gte_RotTransPers)(Vx(0), ScreenXY(), depth);
        SH_CALL(Gte_PrimDepthFlat4_10)(p);
        const U m = c < 0x1380000 ? 4 : level;
        const U half = static_cast<U>(I(m * rise) / 2) + spread + 4;
        if (divisor == 0)
            bof3::Fatal("%s: the column's divisor 8 - rise is 0 (rise 8) - the original divides by it unchecked and faults "
                        "(docs/effect_5d.md section 7)",
                        who);
        const std::int32_t step = I(static_cast<U>(c) - page) / 4;
        const U texture = static_cast<U>(step / divisor);
        const long double h = static_cast<long double>(I(half));
        const long double sx = SX();
        StoreFloat(p + 0x28, sx - h);
        StoreFloat(p + 8, sx - h);
        StoreFloat(p + 0x38, h + SX());
        StoreFloat(p + 0x18, h + SX());
        const long double sy = SY();
        StoreFloat(p + 0x1C, sy - h);
        StoreFloat(p + 0xC, sy - h);
        StoreFloat(p + 0x3C, h + SY());
        StoreFloat(p + 0x2C, h + SY());
        SH_CALL(Prim_SetTexture)((texture & 0xF80000) | 0xBA009124u, p, 1);
        if (row == 3) p[6] = 0;
        s = S();
        SH_CALL(MapView_LinkPrimAt)(UL(s + 0x34), UL(s + 0x38), static_cast<int>(row), 0x48);
        c -= 0x400000;
        ++level;
    } while (c > static_cast<std::int32_t>(0xFFF80000u));
}

}  // namespace

// original 0x5052D0 (cdecl): Column - the square's point the record's cell, y
// rising (odd + 8 level) * rise above half the stored height; linked on the
// record's row (dy 1).
extern "C" void __cdecl EffectKind18Sub1C_Draw(U rise) {
    Column("EffectKind18Sub1C_Draw", rise, 1, [rise](U level, U x0, U z0, U y0, U odd) {
        SetV(0, x0);
        SetV(2, z0);
        SetV(4, y0 - (odd + level * 8) * rise);
    });
}

// original 0x505480 (sub-kinds 0x1C / 0x1D's sub-state 1): the record's cell at
// half its stored height projected (Gte_RotTransPers to MapView_ScreenXY);
// eax 1 when both screen floats lie in [-20, 340] (0x5C422C, 0x5C4228; x87
// compares, a NaN outside), else 0. DIV-0041's "unnamed 0x5054E3" cull - left
// at 320's bounds as that entry says.
extern "C" U __cdecl EffectKind18Sub1C_OnScreen(void) {
    unsigned char* const s = S();
    SetV(4, static_cast<U>(-(S16(W(s + 0x3E)) / 2)));
    SetV(2, static_cast<U>(S16(W(s + 0x3A)) - 0x80) << 7);
    SetV(0, (W(s + 0x36) - 0x80) << 7);
    long depth[2];
    SH_CALL(Gte_RotTransPers)(Vx(0), ScreenXY(), depth);
    const float low = Fl(at::kScreenLow), high = Fl(at::kScreenHigh);
    const float sx = Fl(AddressOf(MapView_ScreenXY)), sy = Fl(at::kScreenY);
    if (!(sx >= low)) return 0;
    if (sx > high) return 0;
    if (!(sy >= low)) return 0;
    if (sy > high) return 0;
    return 1;
}

// original 0x505540 (EffectKind18_States[29], hidden in 0x505480; a catalog row
// of part 2 that no group of the round holds): jmp [EffectKind18Sub1D_States +
// +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub1D_Run(void) {
    Dispatch("EffectKind18Sub1D_Run", E5D_TABLE(EffectKind18Sub1D_States));
}

// original 0x505560 (sub-state 0): +0xA the wait 0x65E2F4 gives the cell's
// parity, +9 it; +0x3E the ground's height at the point; the direction (+0xC,
// +0x10) (0, 1) when the ground half a cell along (+x, -z) is higher, else
// (1, 0); +2 up.
extern "C" void __cdecl EffectKind18Sub1D_Start(void) {
    unsigned char* s = S();
    s[0xA] = B(at::kSub1DWaits + (((s[0x3A] & 1u) << 1) | (s[0x36] & 1u)));
    s = S();
    s[9] = s[0xA];
    s = S();
    const U e = static_cast<U>(SH_CALL(AreaMap_Elevation)(static_cast<long>(UL(s + 0x34)), static_cast<long>(UL(s + 0x38))));
    SetWord(S() + 0x3E, e);
    s = S();
    const U e2 = static_cast<U>(
        SH_CALL(AreaMap_Elevation)(static_cast<long>(UL(s + 0x34) + 0x8000), static_cast<long>(UL(s + 0x38) - 0x8000)));
    s = S();
    if (S16(e2) > S16(W(s + 0x3E))) {
        SetUL(s + 0xC, 0);
        SetUL(S() + 0x10, 1);
    } else {
        SetUL(s + 0xC, 1);
        SetUL(S() + 0x10, 0);
    }
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
}

// original 0x505610 (sub-state 1): WaitThenSound (0x5051A0's twin).
extern "C" void __cdecl EffectKind18Sub1D_Wait(void) { WaitThenSound(); }

// original 0x505650 (sub-state 2): the column at +9; +9 up one; past 7, +9 0 and
// +2 up.
extern "C" void __cdecl EffectKind18Sub1D_Rise(void) {
    SH_CALL(EffectKind18Sub1D_Draw)(S()[9]);
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] + 1);
    s = S();
    if (s[9] > 7) {
        s[9] = 0;
        S()[2] = static_cast<unsigned char>(S()[2] + 1);
    }
}

// original 0x505690 (sub-state 3): for the cells one and two along the direction
// (+0xC, +0x10) from the point: Party_MemberAt(x, z, 0); a member there
// (al not negative): Field_State and Sprite_Current that member's ObjTrio
// record; Actor_EquipCount(its +0x148, 3, 8) answering 0: unless bit 5 of its
// Field_ActorStates byte, Sprite_FlashClut(2); Field_FloorHurt(5); then
// Sprite_Current put back (Field_State left). Then the column at 7; +9 up one;
// past 0x1E, +9 0x1F and +2 up.
extern "C" void __cdecl EffectKind18Sub1D_Hurt(void) {
    static const char kWho[] = "EffectKind18Sub1D_Hurt";
    unsigned char* s = S();
    for (U step = 1; step < 3; ++step) {
        const U x = ((UL(s + 0xC) * step) << 16) + UL(s + 0x34);
        const U z = ((UL(s + 0x10) * step) << 16) + UL(s + 0x38);
        const unsigned char who = SH_CALL(Party_MemberAt)(static_cast<long>(x), static_cast<long>(z), 0);
        if (S8(who) < 0) {
            s = S();
            continue;
        }
        const U member = Index(kWho, S8(who), at::kMemberCount, AddressOf(ObjTrio));
        unsigned char* const saved = S();
        unsigned char* const object = ObjTrio + at::kMemberStride * member;
        Field_State = object;
        Sprite_Current = object;
        if (SH_CALL(Actor_EquipCount)(object[0x148], 3, 8) == 0) {
            if ((Field_ActorStates[Field_State[0x148] * at::kActorStride] & 0x20) == 0) SH_CALL(Sprite_FlashClut)(2);
            SH_CALL(Field_FloorHurt)(5);
        }
        Sprite_Current = saved;
        s = saved;
    }
    SH_CALL(EffectKind18Sub1D_Draw)(7);
    s = S();
    s[9] = static_cast<unsigned char>(s[9] + 1);
    s = S();
    if (s[9] > 0x1E) {
        s[9] = 0x1F;
        S()[2] = static_cast<unsigned char>(S()[2] + 1);
    }
}

// original 0x505790 (sub-state 4): 0x505290's twin with sub-kind 0x1D's column.
extern "C" void __cdecl EffectKind18Sub1D_Fall(void) {
    SH_CALL(EffectKind18Sub1D_Draw)(S()[9] >> 2);
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    s = S();
    if (s[9] < 8) {
        s[9] = s[0xA];
        S()[2] = 1;
    }
}

// original 0x5057D0 (cdecl): Column - the square's point leaning along the
// direction (+0xC, +0x10, low words) by (odd + 8 level) * rise, y half the
// stored height; blue 0, linked on the record's row (dy 3).
extern "C" void __cdecl EffectKind18Sub1D_Draw(U rise) {
    Column("EffectKind18Sub1D_Draw", rise, 3, [rise](U level, U x0, U z0, U y0, U odd) {
        const U lean = odd + level * 8;
        unsigned char* const s = S();
        SetV(0, ((W(s + 0xC) * lean) & 0xFFFF) * rise + x0);
        SetV(2, ((W(s + 0x10) * lean) & 0xFFFF) * rise + z0);
        SetV(4, y0);
    });
}

// ===========================================================================
// Sub-kinds 0x14 and 0x1E: EffectKind18_States[20] / [30] (0x6540BC /
// 0x6540E4), EffectKind18Sub14_States (five; its entries 2..4 are 0x1E's)
// ===========================================================================

// original 0x5059A0 (EffectKind18_States[20], hidden in 0x5057D0):
// jmp [EffectKind18Sub14_States + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub14_Run(void) {
    Dispatch("EffectKind18Sub14_Run", E5D_TABLE(EffectKind18Sub14_States));
}

// original 0x5059C0 (sub-state 0): area flag 0x3E (0x903FD8) set: the strip at
// half (0x40) and Effect_Release; +2 up either way.
extern "C" void __cdecl EffectKind18Sub14_Start(void) {
    if (SH_CALL(Flags_Test)(At(at::kAreaFlags), 0x3E)) {
        SH_CALL(EffectKind18Sub14_ScaleClut)(0x40);
        SH_CALL(Effect_Release)();
    }
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
}

// original 0x5059F0 (sub-state 1): once area flag 0x3E is set: +9 up one, the
// strip at 0x80 - (+9 >> 1); at 0x80 or past, a tail jump to Effect_Release.
extern "C" void __cdecl EffectKind18Sub14_Dim(void) {
    if (!SH_CALL(Flags_Test)(At(at::kAreaFlags), 0x3E)) return;
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] + 1);
    SH_CALL(EffectKind18Sub14_ScaleClut)(0x80u - (S()[9] >> 1));
    if (S()[9] >= 0x80) SH_CALL(Effect_Release)();
}

// original 0x505A40 (cdecl): the sixteen 15-bit colours at 0x80BCC0 written to
// 0x80FCC0 with red and blue scaled by scale / 128 (green and bit 15 kept), and
// Gfx_ClutStripDirty set.
extern "C" void __cdecl EffectKind18Sub14_ScaleClut(U scale) {
    for (U k = 0; k < at::kClutStripWords; ++k) {
        const U w = W(at::kClutStrip + 2 * k);
        const U blue = static_cast<U>(I(((w >> 10) & 0x1F) * scale) / 128);
        const U red = static_cast<U>(I((w & 0x1F) * scale) / 128);
        SetWord(At(at::kClutStripOut + 2 * k), (blue << 10) | red | (w & 0x83E0));
    }
    Gfx_ClutStripDirty = 1;
}

// original 0x505AB0 (EffectKind18_States[30], hidden in 0x505A40):
// jmp [EffectKind18Sub1E_States + +2 * 4], unbounded (0x65E300, sub-kind 0x14's
// table from its entry 2).
extern "C" void __cdecl EffectKind18Sub1E_Run(void) {
    Dispatch("EffectKind18Sub1E_Run", E5D_TABLE(EffectKind18Sub1E_States));
}

// original 0x505AD0 (sub-kind 0x14's sub-state 3, 0x1E's 1): the place's flag
// (0x65E31C by +0x36) set in 0x903FD8: Sound_PlayEffect(0x20A); a free record
// made kind 0x6E at the place's point (0x65E30C; height 0x7800000), another
// made kind 0x6D with +6 the place; +9 0, +2 up. The tiles at full shade either
// way.
extern "C" void __cdecl EffectKind18Sub14_WaitFlag(void) {
    static const char kWho[] = "EffectKind18Sub14_WaitFlag";
    const U place = Place(kWho, at::kSub14Count, at::kSub14Flags);
    if (SH_CALL(Flags_Test)(At(at::kAreaFlags), B(at::kSub14Flags + place))) {
        SH_CALL(Sound_PlayEffect)(0x20A);
        const U first = SH_CALL(Effect_FindFree)() & 0xFFu;
        if (first != 0xFF) {
            unsigned char* const s = S();
            unsigned char* const e = Record(kWho, first);
            e[0] = 1;
            e[5] = 0x6E;
            U k = Index(kWho, S16(W(s + 0x36)), at::kSub14Count, at::kSub14Spawn);
            SetUL(e + 0x34, UL(at::kSub14Spawn + 8 * k));
            k = Index(kWho, S16(W(s + 0x36)), at::kSub14Count, at::kSub14Spawn);
            SetUL(e + 0x38, UL(at::kSub14Spawn + 4 + 8 * k));
            SetUL(e + 0x3C, 0x7800000);
        }
        const U second = SH_CALL(Effect_FindFree)() & 0xFFu;
        unsigned char* const s = S();
        if (second != 0xFF) {
            unsigned char* const e = Record(kWho, second);
            e[0] = 1;
            e[5] = 0x6D;
            e[6] = s[0x36];
        }
        s[9] = 0;
        S()[2] = static_cast<unsigned char>(S()[2] + 1);
    }
    SH_CALL(EffectKind18Sub14_DrawTiles)(0x80, 0);
}

// original 0x505BB0 (sub-kind 0x14's sub-state 4, 0x1E's 2): +9 up one on even
// frames; past 0x7F Effect_Release (a call: the tiles still drawn); the tiles
// at 0x80 - +9.
extern "C" void __cdecl EffectKind18Sub14_Fade(void) {
    if ((FC() & 1) == 0) S()[9] = static_cast<unsigned char>(S()[9] + 1);
    if (S()[9] > 0x7F) SH_CALL(Effect_Release)();
    SH_CALL(EffectKind18Sub14_DrawTiles)(0x80u - S()[9], 0);
}

// original 0x505BF0 (cdecl; the second argument unread): over the place's
// rectangle (0x65E320 by +0x36: columns from..to, rows from..to), every cell
// whose column + row is odd a semi-transparent textured square on the ground
// plane (z -0x340) projected (Gte_RotTransPers), half size |3 - ((Frame_Counter
// / 3 + column + row) & 7)| + 0x18, shaded (shade / 2, shade, shade / 2),
// linked on the cell's row (dy 1).
extern "C" void __cdecl EffectKind18Sub14_DrawTiles(U shade, U unused) {
    (void)unused;
    static const char kWho[] = "EffectKind18Sub14_DrawTiles";
    U place = Place(kWho, at::kSub14Count, at::kSub14Rect);
    U row = B(at::kSub14Rect + 4 * place + 1);
    if (row >= B(at::kSub14Rect + 4 * place + 3)) return;
    do {
        U column = B(at::kSub14Rect + 4 * place);
        if (column < B(at::kSub14Rect + 4 * place + 2)) {
            U parity = column + row;
            do {
                if (parity & 1) {
                    SetV(0, (column << 7) - 0x3FC0);
                    SetV(2, (row << 7) - 0x3FC0);
                    SetV(4, 0xFCC0);
                    long depth[2];
                    SH_CALL(Gte_RotTransPers)(Vx(0), ScreenXY(), depth);
                    const U swing = ((FC() / 3) + column + row) & 7;
                    const U half = Abs(3 - static_cast<std::int32_t>(swing)) + 0x18;
                    unsigned char* const p = Gfx_PacketNext;
                    SH_CALL(Gpu_SetPolyFT4)(p);
                    SH_CALL(Gpu_SetShadeTex)(p, 0);
                    const long double h = static_cast<long double>(I(half));
                    const long double whole = static_cast<long double>(I(half * 2));
                    StoreFloat(p + 8, Less(SX(), h));
                    StoreFloat(p + 0xC, Less(SY(), h));
                    StoreFloat(p + 0x18, Less(SX(), h) + whole);
                    StoreFloat(p + 0x1C, Less(SY(), h));
                    StoreFloat(p + 0x28, Less(SX(), h));
                    StoreFloat(p + 0x2C, Less(SY(), h) + whole);
                    StoreFloat(p + 0x38, Less(SX(), h) + whole);
                    StoreFloat(p + 0x3C, Less(SY(), h) + whole);
                    SH_CALL(Gte_PrimDepthFlat4_10)(p);
                    SH_CALL(Gpu_SetSemiTrans)(p, 1);
                    const unsigned char half_shade = static_cast<unsigned char>(I(shade << 6) / 128);
                    const unsigned char full_shade = static_cast<unsigned char>(I(shade << 7) / 128);
                    p[0x14] = 0xE0;
                    p[0x15] = 0x30;
                    p[0x24] = 0xFF;
                    p[0x25] = 0x30;
                    p[0x34] = 0xE0;
                    p[0x35] = 0x4F;
                    p[0x44] = 0xFF;
                    p[0x45] = 0x4F;
                    SetWord(p + 0x26, 0x3B);
                    SetWord(p + 0x16, 0x78C5);
                    p[4] = half_shade;
                    p[5] = full_shade;
                    p[6] = half_shade;
                    SH_CALL(MapView_LinkPrimAt)(column << 16, row << 16, 1, 0x48);
                }
                ++parity;
                ++column;
                place = Place(kWho, at::kSub14Count, at::kSub14Rect);
            } while (column < B(at::kSub14Rect + 4 * place + 2));
        }
        place = Place(kWho, at::kSub14Count, at::kSub14Rect);
        ++row;
    } while (row < B(at::kSub14Rect + 4 * place + 3));
}

// ===========================================================================
// Sub-kind 0x21: EffectKind18_States[33] (0x6540F0), EffectKind18Sub21_States
// (two, called) by +2
// ===========================================================================

// original 0x505E20 (EffectKind18_States[33], hidden in 0x505BF0): call
// [EffectKind18Sub21_States + +2 * 4] (unbounded), then the spiral at +9.
extern "C" void __cdecl EffectKind18Sub21_Run(void) {
    Dispatch("EffectKind18Sub21_Run", E5D_TABLE(EffectKind18Sub21_States));
    SH_CALL(EffectKind18Sub21_DrawSpiral)(S()[9]);
}

// original 0x505E50 (sub-state 1): +9 up two.
extern "C" void __cdecl EffectKind18Sub21_Step(void) { S()[9] = static_cast<unsigned char>(S()[9] + 2); }

// original 0x505E60 (cdecl): with Draw_PassFlags bit 2 set: a draw mode, then
// sixteen steps of a spiral about the screen's centre (160, 120) - a POLY_G3
// from the centre and two POLY_G4s outward, their corners Math_Cos / Math_Sin
// of the step's angle (from 0x200 by 0x100) at three radii (x / 16, / 8, / 4
// over 3; y half that) - grey |0x80 - n| / 2 inside, the rim's red and blue
// from Math_Cos of n's phases; committed to slot 7; a draw mode after. The
// centre is the 320 x 240 screen's (DIV-0041 not applied here).
extern "C" void __cdecl EffectKind18Sub21_DrawSpiral(U n) {
    if ((Draw_PassFlags & 4) == 0) return;
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x95, 0);
    SH_CALL(Gfx_CommitPrim)(7, 0xC);
    const unsigned char grey = static_cast<unsigned char>(Abs(static_cast<std::int32_t>(0x80 - n)) >> 1);
    U angle = 0x200, phase = n << 4;
    const U lag = n << 4;
    auto x_at = [](U a, unsigned shift) {
        const std::int32_t c = static_cast<std::int32_t>(SH_CALL(Math_Cos)(static_cast<int>(a)));
        return static_cast<long double>((c >> shift) / 3 + 0xA0);
    };
    auto y_at = [](U a, unsigned shift) {
        const std::int32_t c = static_cast<std::int32_t>(SH_CALL(Math_Sin)(static_cast<int>(a)));
        return static_cast<long double>((c >> shift) / 3 + 0x78);
    };
    auto rim = [](U a) {
        const std::int32_t c = static_cast<std::int32_t>(SH_CALL(Math_Cos)(static_cast<int>(a)));
        return static_cast<unsigned char>(((c >> 6) + 0x80) >> 1);
    };
    for (U k = 0x900; k < 0x1900; k += 0x100) {
        unsigned char* const g3 = Gfx_PacketNext;
        unsigned char* const g4 = g3 + 0x34;
        unsigned char* const g5 = g3 + 0x78;
        SH_CALL(Gpu_SetPolyG3)(g3);
        SH_CALL(Gpu_SetPolyG4)(g4);
        SH_CALL(Gpu_SetPolyG4)(g5);
        SetUL(g3 + 8, 0x43200000u);
        SetUL(g3 + 0xC, 0x42F00000u);
        long double v = x_at(angle - 0x200, 4);
        StoreFloat(g4 + 8, v);
        StoreFloat(g3 + 0x18, v);
        v = y_at(angle - 0x200, 5);
        StoreFloat(g4 + 0xC, v);
        StoreFloat(g3 + 0x1C, v);
        v = x_at(angle - 0x100, 4);
        StoreFloat(g4 + 0x18, v);
        StoreFloat(g3 + 0x28, v);
        v = y_at(angle - 0x100, 5);
        StoreFloat(g4 + 0x1C, v);
        StoreFloat(g3 + 0x2C, v);
        v = x_at(angle - 0x100, 3);
        StoreFloat(g5 + 8, v);
        StoreFloat(g4 + 0x28, v);
        v = y_at(angle - 0x100, 4);
        StoreFloat(g5 + 0xC, v);
        StoreFloat(g4 + 0x2C, v);
        v = x_at(angle, 3);
        StoreFloat(g5 + 0x18, v);
        StoreFloat(g4 + 0x38, v);
        v = y_at(angle, 4);
        StoreFloat(g5 + 0x1C, v);
        StoreFloat(g4 + 0x3C, v);
        StoreFloat(g5 + 0x28, x_at(angle, 2));
        StoreFloat(g5 + 0x2C, y_at(angle, 3));
        angle += 0x100;
        StoreFloat(g5 + 0x38, x_at(angle, 2));
        StoreFloat(g5 + 0x3C, y_at(angle, 3));
        g3[6] = grey;
        g3[5] = grey;
        g3[4] = grey;
        g5[0x36] = grey;
        g5[0x35] = grey;
        g5[0x34] = grey;
        g5[0x26] = grey;
        g5[0x25] = grey;
        g5[0x24] = grey;
        unsigned char c = rim(phase);
        g5[6] = c;
        g4[0x26] = c;
        g4[6] = c;
        g3[0x16] = c;
        c = rim(phase + 0x100);
        g5[0x16] = c;
        g4[0x36] = c;
        g4[0x16] = c;
        g3[0x26] = c;
        c = rim(k - lag - 0x100);
        g5[4] = c;
        g4[0x24] = c;
        g4[4] = c;
        g3[0x14] = c;
        c = rim(k - lag);
        g5[0x14] = c;
        g4[0x34] = c;
        g4[0x14] = c;
        g3[0x24] = c;
        g3[0x25] = 0;
        g3[0x15] = 0;
        g4[0x15] = 0;
        g4[5] = 0;
        g4[0x35] = 0;
        g4[0x25] = 0;
        g5[0x15] = 0;
        g5[5] = 0;
        SH_CALL(Gfx_CommitPrim)(7, 0x34);
        SH_CALL(Gfx_CommitPrim)(7, 0x44);
        SH_CALL(Gfx_CommitPrim)(7, 0x44);
        phase += 0x100;
    }
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x95, 0);
    SH_CALL(Gfx_CommitPrim)(7, 0xC);
}

// ===========================================================================
// Sub-kind 0x52: EffectKind18_States[82] (0x6541B4), one state
// ===========================================================================

// original 0x506290 (EffectKind18_States[82], hidden in 0x505E60): the camera's
// cell within 20 (|dx| + |dz|) of the record's: a draw mode (0xB5) linked on
// the record's row; +9 up two; sixteen rings (i) of sixteen semi-transparent
// dots about the record's point, turning by (Frame_Counter & 0xF) << 4 one way
// or the other (0x65E330 by the ring's parity), radius 3 (Math_Sin / Math_Cos
// >> 6), grey 0xFF - (+9 - 16 i), height (-0xC0 - (+9 - 16 i)) * 2, each
// projected (Gte_RotTransPers to the dot) and linked (dy 1); a draw mode after.
extern "C" void __cdecl EffectKind18Sub52_Draw(void) {
    unsigned char* s = S();
    const U near = Abs(S16(W(at::kCameraZ)) - S16(W(s + 0x3A))) + Abs(S16(W(at::kCameraX)) - S16(W(s + 0x36)));
    if (I(near) > 0x14) return;
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0xB5, 0);
    s = S();
    SH_CALL(MapView_LinkPrimAt)(UL(s + 0x34), UL(s + 0x38), 1, 0xC);
    s = S();
    s[9] = static_cast<unsigned char>(s[9] + 2);
    for (U ring = 0; ring < 0x10; ++ring) {
        s = S();
        const U grey = 0xFFu - ((s[9] - ring * 0x10) & 0xFF);
        const std::int32_t sign = S8(B(at::kSub52Signs + (ring & 1)));
        for (U turn = 0; turn < 0x1000; turn += 0x100) {
            unsigned char* p = Gfx_PacketNext;
            SH_CALL(Gpu_SetTile1)(p);
            SH_CALL(Gpu_SetSemiTrans)(p, 1);
            p[4] = static_cast<unsigned char>(grey);
            p[5] = static_cast<unsigned char>(grey);
            p[6] = static_cast<unsigned char>(grey);
            const U sn = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(((FC() & 0xF) << 4) * static_cast<U>(sign) + turn)));
            s = S();
            SetV(0, static_cast<U>((I(sn) >> 6) * 3 + (I(UL(s + 0x34)) >> 9) - 0x4000));
            const U cs = static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(((FC() & 0xF) << 4) * static_cast<U>(sign) + turn)));
            s = S();
            SetV(2, static_cast<U>((I(cs) >> 6) * 3 + (I(UL(s + 0x38)) >> 9) - 0x4000));
            const U fade = (s[9] - static_cast<unsigned char>(ring * 0x10)) & 0xFF;
            SetV(4, (0xFFFFFF40u - fade) << 1);
            long depth[2];
            SH_CALL(Gte_RotTransPers)(Vx(0), reinterpret_cast<unsigned long*>(p + 8), depth);
            p += 0x10;
            SH_CALL(Gte_StoreDepthF)(Fp(p));
            s = S();
            SH_CALL(MapView_LinkPrimAt)(UL(s + 0x34), UL(s + 0x38), 1, 0x14);
        }
    }
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0xB5, 0);
    s = S();
    SH_CALL(MapView_LinkPrimAt)(UL(s + 0x34), UL(s + 0x38), 1, 0xC);
}

// ===========================================================================
// Sub-kind 0x22: EffectKind18_States[34] (0x6540F4), EffectKind18Sub22_States
// (five) by +2
// ===========================================================================

// original 0x5064A0 (EffectKind18_States[34], hidden in 0x505E60):
// jmp [EffectKind18Sub22_States + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub22_Run(void) {
    Dispatch("EffectKind18Sub22_Run", E5D_TABLE(EffectKind18Sub22_States));
}

namespace {

constexpr char kSub22[] = "EffectKind18Sub22";

U Sub22Place() { return Place(kSub22, at::kSub22Count, at::kSub22Rect); }
U Rect(U place, unsigned k) { return B(at::kSub22Rect + 4 * place + k); }

// The record's story flag (+0x34's low byte, which sub-state 0 sets) tested.
bool Sub22Flag() { return SH_CALL(Flags_Test)(At(at::kStoryFlags), S()[0x34]) != 0; }

// Sub-states 2 and 4's turn: +9 0, +2 `next` (or up one), +0x3A the step;
// the map written; sounds 0x202 and 0x201.
void Sub22Turn() {
    SH_CALL(EffectKind18Sub22_SetMap)();
    SH_CALL(Sound_PlayEffect)(0x202);
    SH_CALL(Sound_PlayEffect)(0x201);
}

}  // namespace

// original 0x5064C0 (sub-state 0): +0x34's low word the place's story flag
// (0x65E348 by +0x36); set: +0x3E the place's height (0x65E34C) less 0x80 and
// +2 2; clear: +0x3E the height and +2 4. The map written, then the draw (a
// tail jump).
extern "C" void __cdecl EffectKind18Sub22_Start(void) {
    U place = Place(kSub22, at::kSub22Count, at::kSub22Flags);
    SetWord(S() + 0x34, B(at::kSub22Flags + place));
    const bool set = Sub22Flag();
    unsigned char* const s = S();
    place = Place(kSub22, at::kSub22Count, at::kSub22Heights);
    if (set) {
        SetWord(s + 0x3E, W(at::kSub22Heights + 2 * place) - 0x80);
        S()[2] = 2;
    } else {
        SetWord(s + 0x3E, W(at::kSub22Heights + 2 * place));
        S()[2] = 4;
    }
    SH_CALL(EffectKind18Sub22_SetMap)();
    SH_CALL(EffectKind18Sub22_Draw)();
}

// original 0x506540 (sub-state 4): the flag set: +9 0, +2 1, +0x3A -8, the map
// written, sounds 0x202 and 0x201. The draw (a tail jump).
extern "C" void __cdecl EffectKind18Sub22_WaitSet(void) {
    if (Sub22Flag()) {
        S()[9] = 0;
        S()[2] = 1;
        SetWord(S() + 0x3A, 0xFFF8);
        Sub22Turn();
    }
    SH_CALL(EffectKind18Sub22_Draw)();
}

// original 0x5065A0 (sub-states 1 and 3): +0x3E plus +0x3A (words); +9 up one;
// at 0x10 or past, +2 up and the map written. The draw (a tail jump).
extern "C" void __cdecl EffectKind18Sub22_Move(void) {
    unsigned char* s = S();
    SetWord(s + 0x3E, W(s + 0x3E) + W(s + 0x3A));
    s = S();
    s[9] = static_cast<unsigned char>(s[9] + 1);
    s = S();
    if (s[9] >= 0x10) {
        s[2] = static_cast<unsigned char>(s[2] + 1);
        SH_CALL(EffectKind18Sub22_SetMap)();
    }
    SH_CALL(EffectKind18Sub22_Draw)();
}

// original 0x5065E0 (sub-state 2): the flag clear: +9 0 (the al Flags_Test
// answered), +2 up, +0x3A 8, the map written, sounds 0x202 and 0x201. The draw
// (a tail jump).
extern "C" void __cdecl EffectKind18Sub22_WaitClear(void) {
    if (!Sub22Flag()) {
        S()[9] = 0;
        S()[2] = static_cast<unsigned char>(S()[2] + 1);
        SetWord(S() + 0x3A, 8);
        Sub22Turn();
    }
    SH_CALL(EffectKind18Sub22_Draw)();
}

// original 0x506640: the place's map cells (0x65E354 by +0x36: x from..to, z
// from..to) written - for place 0 their AreaMap_Bytes byte 0x11 (0 at
// sub-state 2), for the others their AreaMap_Corners dword four times a byte of
// the flag's row of 0x65E364 (a running count, eleven a row); then every
// cell's height byte (AreaMap_Header + AreaMap_HeightBase * 4 + the cell) to
// (-ground - 2 +0x3E) / 32. Indexes into the area block unchecked, as the
// original's: the rectangles are the area's own cells.
extern "C" void __cdecl EffectKind18Sub22_SetMap(void) {
    const U flag = SH_CALL(Flags_Test)(At(at::kStoryFlags), S()[0x34]) & 0xFFu;
    U count = 0;
    if (W(S() + 0x36) == 0) {
        U off = 0;
        U x = B(at::kSub22Rect);
        if (x < B(at::kSub22Rect + 2)) {
            do {
                U z = B(at::kSub22Rect + 1 + off);
                if (z < B(at::kSub22Rect + 3 + off)) {
                    do {
                        const unsigned char value = S()[2] != 2 ? 0x11 : 0;
                        const U width = AreaMap_Header[0];
                        const_cast<unsigned char*>(AreaMap_Bytes)[width * z + x] = value;
                        ++z;
                    } while (z < Rect(Sub22Place(), 3));
                }
                off = 4 * Sub22Place();
                ++x;
            } while (x < B(at::kSub22Rect + 2 + off));
        }
    } else {
        U place = Sub22Place();
        U x = Rect(place, 0);
        if (x < Rect(place, 2)) {
            do {
                U z = Rect(place, 1);
                if (z < Rect(place, 3)) {
                    const U row = at::kSub22Corners + at::kSub22CornerRow * flag;
                    do {
                        if (count >= at::kSub22CornerRow || flag > 1)
                            bof3::Fatal("EffectKind18Sub22_SetMap: corner byte %u of row %u, past the two rows of eleven at "
                                        "0x65E364 - the original reads the data after (docs/effect_5d.md section 7)",
                                        (unsigned)count, (unsigned)flag);
                        const U c = B(row + count);
                        const U width = AreaMap_Header[0];
                        SetUL(At(AddressOf(&AreaMap_Corners) + 4 * (width * z + x)), c * 0x01010101u);
                        ++count;
                        ++z;
                    } while (z < Rect(Sub22Place(), 3));
                }
                place = Sub22Place();
                ++x;
            } while (x < Rect(place, 2));
        }
    }
    U place = Sub22Place();
    U x = Rect(place, 0);
    if (x >= Rect(place, 2)) return;
    do {
        U z = Rect(place, 1);
        if (z < Rect(place, 3)) {
            do {
                const U e = static_cast<U>(SH_CALL(AreaMap_Elevation)(static_cast<long>(x << 16), static_cast<long>(z << 16)));
                unsigned char* const s = S();
                const std::int32_t v = -S16(e) - S16(W(s + 0x3E)) * 2;
                const U base = AreaMap_HeightBase;
                const U width = AreaMap_Header[0];
                B(AddressOf(AreaMap_Header) + base * 4 + x + width * z) = static_cast<unsigned char>(v / 32);
                ++z;
            } while (z < Rect(Sub22Place(), 3));
        }
        place = Sub22Place();
        ++x;
    } while (x < Rect(place, 2));
}

// original 0x506860 (sub-states 0..4 tail-jump to it; inside 0x506640's span,
// its own frame and ret): the place's pieces (0x65E38C by +0x36: from..to of
// the 89 records at 0x65E3D0) - each a textured quad whose four corners are
// its shape's (0x65E394, by the piece's byte 1) offsets from the piece's cell
// (bytes 3, 2), height the offset over +0x3E; Gte_RotTransPers4,
// Gte_PrimDepths4_10, the piece's texture dword; linked on the piece's cell
// with the row offset 0x65E37C gives its kind (byte 0) at sub-state 2 or not -
// or, negative, committed to slot 3.
extern "C" void __cdecl EffectKind18Sub22_Draw(void) {
    static const char kWho[] = "EffectKind18Sub22_Draw";
    U place = Place(kWho, at::kSub22Count, at::kSub22Pieces);
    U k = B(at::kSub22Pieces + 2 * place);
    if (k >= B(at::kSub22Pieces + 2 * place + 1)) return;
    do {
        const U piece = at::kSub22Piece + 8 * Index(kWho, I(k), at::kSub22PieceCount, at::kSub22Piece);
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyFT4)(p);
        SH_CALL(Gpu_SetShadeTex)(p, 0);
        U d = UL(piece);
        const U x = ((d >> 24) << 7) - 0x3FC0;
        const U z = (((d >> 16) & 0xFF) << 7) - 0x3FC0;
        const U shape = at::kSub22Shapes + 12 * Index(kWho, (d >> 8) & 0xFF, at::kSub22ShapeCount, at::kSub22Shapes);
        for (unsigned v = 0; v < 4; ++v) {
            const U o = shape + 3 * v;
            SetV(8 * v, x - (static_cast<U>(S8(B(o))) << 7));
            SetV(8 * v + 2, z - (static_cast<U>(S8(B(o + 1))) << 7));
            SetV(8 * v + 4, (static_cast<U>(S8(B(o + 2))) << 7) + W(S() + 0x3E));
        }
        long depth[2];
        SH_CALL(Gte_RotTransPers4)(Vx(0), Vx(1), Vx(2), Vx(3), Fp(p + 8), Fp(p + 0x18), Fp(p + 0x28), Fp(p + 0x38), depth);
        SH_CALL(Gte_PrimDepths4_10)(p);
        SH_CALL(Prim_SetTexture)(UL(piece + 4), p, 1);
        d = UL(piece);
        unsigned char* s = S();
        const U kind = Index(kWho, d & 0xFF, at::kSub22RowKinds, at::kSub22Rows);
        const std::int32_t row = S8(B(at::kSub22Rows + (s[2] == 2 ? 1 : 0) + 2 * kind));
        if (row < 0)
            SH_CALL(Gfx_CommitPrim)(3, 0x48);
        else
            SH_CALL(MapView_LinkPrimAt)((d >> 8) & 0xFF0000, d & 0xFF0000, row, 0x48);
        s = S();
        ++k;
        place = Place(kWho, at::kSub22Count, at::kSub22Pieces);
    } while (k < B(at::kSub22Pieces + 2 * place + 1));
}

void Effect5D_Inject() {
    if (bof3::WantsShadow("effect_5d")) effect_5d::SelfTest();
    BOF3_INJECT(EffectKind18Sub17_Close);
    BOF3_INJECT(EffectKind18Sub17_ScrollTexture);
    BOF3_INJECT(EffectKind18Sub17_DrawPatch);
    BOF3_INJECT(EffectKind18Sub17_CopyFrame);
    BOF3_INJECT(EffectKind18Sub18_Run);
    BOF3_INJECT(EffectKind18Sub18_Grow);
    BOF3_INJECT(EffectKind18Sub18_Hold);
    BOF3_INJECT(EffectKind18Sub18_Shrink);
    BOF3_INJECT(EffectKind18Sub18_End);
    BOF3_INJECT(EffectKind18Sub18_DrawColumn);
    BOF3_INJECT(EffectKind18Sub18_DrawRing);
    BOF3_INJECT(EffectKind18Sub19_Run);
    BOF3_INJECT(EffectKind18Sub68_Draw);
    BOF3_INJECT(EffectKind18Sub1B_Run);
    BOF3_INJECT(EffectKind18Sub43_Run);
    BOF3_INJECT(EffectKind18Sub43_Glow);
    BOF3_INJECT(EffectKind18Sub43_End);
    BOF3_INJECT(EffectKind18Sub43_DrawQuad);
    BOF3_INJECT(EffectKind18Sub1C_Run);
    BOF3_INJECT(EffectKind18Sub1C_Start);
    BOF3_INJECT(EffectKind18Sub1C_Wait);
    BOF3_INJECT(EffectKind18Sub1C_Rise);
    BOF3_INJECT(EffectKind18Sub1C_Hold);
    BOF3_INJECT(EffectKind18Sub1C_Fall);
    BOF3_INJECT(EffectKind18Sub1C_Draw);
    BOF3_INJECT(EffectKind18Sub1C_OnScreen);
    BOF3_INJECT(EffectKind18Sub1D_Run);
    BOF3_INJECT(EffectKind18Sub1D_Start);
    BOF3_INJECT(EffectKind18Sub1D_Wait);
    BOF3_INJECT(EffectKind18Sub1D_Rise);
    BOF3_INJECT(EffectKind18Sub1D_Hurt);
    BOF3_INJECT(EffectKind18Sub1D_Fall);
    BOF3_INJECT(EffectKind18Sub1D_Draw);
    BOF3_INJECT(EffectKind18Sub14_Run);
    BOF3_INJECT(EffectKind18Sub14_Start);
    BOF3_INJECT(EffectKind18Sub14_Dim);
    BOF3_INJECT(EffectKind18Sub14_ScaleClut);
    BOF3_INJECT(EffectKind18Sub1E_Run);
    BOF3_INJECT(EffectKind18Sub14_WaitFlag);
    BOF3_INJECT(EffectKind18Sub14_Fade);
    BOF3_INJECT(EffectKind18Sub14_DrawTiles);
    BOF3_INJECT(EffectKind18Sub21_Run);
    BOF3_INJECT(EffectKind18Sub21_Step);
    BOF3_INJECT(EffectKind18Sub21_DrawSpiral);
    BOF3_INJECT(EffectKind18Sub52_Draw);
    BOF3_INJECT(EffectKind18Sub22_Run);
    BOF3_INJECT(EffectKind18Sub22_Start);
    BOF3_INJECT(EffectKind18Sub22_WaitSet);
    BOF3_INJECT(EffectKind18Sub22_Move);
    BOF3_INJECT(EffectKind18Sub22_WaitClear);
    BOF3_INJECT(EffectKind18Sub22_SetMap);
    BOF3_INJECT(EffectKind18Sub22_Draw);
}

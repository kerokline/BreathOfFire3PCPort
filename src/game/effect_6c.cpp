// Round thirteen group E6C (docs/effect_6c.md): the cut's 48 rows of group E6C,
// 0x510C90..0x51426B, less one start that is a case of its host's switch
// (0x5124C0, inside EffectKind18Sub51_Wait), plus three its band holds that no
// group's list did - sub-kind 0x51's third state 0x512510 and the dispatchers
// of sub-kinds 0x59 and 0x5B (0x512B20, 0x513CD0): 50 functions, each read with
// capstone to its last instruction (2026-10-03). Effect_RunObjects (ours) makes
// each live record of Effect_Objects (20 of 0x80 bytes) Sprite_Current and
// calls Effect_KindHandlers[+5]; kind 0x18's EffectKind18_Run jumps through
// EffectKind18_States by +1 (the sub-kind). What each is, as far as the code
// says (what the game shows is the owner's to say):
//
//   sub-kind 0x44   (E6B's dispatcher 0x510C20, whose state 1 calls the first
//                   and jumps to the second) the record kept on the leader's
//                   animation and projected at Field_Kind2's point, put on the
//                   sprite list; on Cond_ByteFE 1 its colour cells set, on 2 a
//                   red additive fill over the frame - DIV-0041's (below). Then
//                   a screen-space sky: bands and two rows of three textured
//                   quads tinted by area 189's frame word (0x90405C, 0..0x3BF:
//                   Gfx_ClutAdjust by its quarter), stars, two glows placed by
//                   the camera's angle and distance from two map points
//   sub-kind 0x45   a ring of 32 textured quads round a fixed point, opened
//                   outward 8 a frame; then eight rings spreading one after
//                   another; at the end Cond_ByteFE 2 and the record released
//   sub-kind 0x51   a rectangle of map cells (one of five, by +0x36) whose
//                   records' bits 19..23 are cleared, then - once a flag or
//                   Cond_ByteFE says - stepped up to 0x10 and the record released
//   sub-kind 0x53   sixteen rings of spinning dots near the record while the
//                   camera's point is within 20 cells of it
//   sub-kind 0x55   a field of 66 textured squares over cells (3..13, 0x71..
//                   0x76) on a checkerboard, shading out once flag 0xE is set
//   sub-kind 0x59   a sixteen-sided ring of two-quad columns and glows, its
//                   shades faded in a cascade through twelve .data bytes; a
//                   texture-page move (DR_MOVE) each frame
//   sub-kind 0x5A   map cells rewritten in a sweep, then two texture moves
//                   cycling through four rows
//   sub-kind 0x5B   three groups of the 49 textured pieces of 0x65EF98 slid
//                   apart and lifted, story flag 0x81 set at the end
//   sub-kind 0x66   two layers of a 2 x 2 grid of 256-pixel textured quads,
//                   scrolled and faded by +9
//   0x511C10        the map's height at (x, z) from the corner table - called
//                   by area 189's code (ours), not effect code
//
// Every call goes through the harness (SH_CALL), so the start-up fuzz can stand
// recorders in for ours as for the originals' copies. Sprite_Current is read
// again wherever the original reads [0x937F88] again after a call. The draw-item
// array is read through draw_pool::Items() (DIV-0062: the original's immediates
// in this band are re-aimed at inject). Sub-kind 0x44's red fill is DIV-0041's
// (section 3c): its tile is (Widescreen_FillX(), 0) Widescreen_FillWidth() x
// 240, the original's (0, 0) 320 x 240 until Widescreen_ArmFills has run and
// whenever the picture is narrow. Otherwise no divergence: each is a faithful
// replacement. Where the original jumps through a state table past its end,
// indexes a table of the image past its room, divides by zero or indexes past
// the draw-item pool, ours aborts with a message (docs/effect_6c.md section 7).
//
// x87: the game runs its FPU at 53 bits; every fadd / fsub / fmul here is
// written in double (the same rounding) and stored to a float once, as the
// original's fstp does; a float the original only loads and stores goes
// through CopyQuiet (x87 turns a signalling NaN quiet on the load).
#include "game/effect_6c.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/draw_pool.h"
#include "game/effect_6c_callees.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_6c::at;
using U = std::uint32_t;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using Handler = scenario_harness::Handler;

unsigned char* S() { return Sprite_Current; }
U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }
U UL(U a) { return UL(At(a)); }
void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }
void SetUL(U a, U v) { SetUL(At(a), v); }
std::int32_t S16(const unsigned char* p) { return static_cast<std::int16_t>(Word(p)); }
std::int32_t S16(U a) { return S16(At(a)); }
std::int32_t S8(U a) { return static_cast<signed char>(*At(a)); }
std::int32_t I(U v) { return static_cast<std::int32_t>(v); }
U AddressOf(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
// sar: an arithmetic right shift of the 32 bits as the original holds them.
U Sar(U v, unsigned n) { return static_cast<U>(static_cast<std::int32_t>(v) >> n); }
// cdq; and edx, 2^n - 1; add eax, edx; sar eax, n: a signed divide by 2^n,
// toward zero.
U DivP2(U v, unsigned n) { return Sar(v + (I(v) < 0 ? (1u << n) - 1u : 0u), n); }
// cdq; xor eax, edx; sub eax, edx.
U Abs(U v) { return I(v) < 0 ? 0u - v : v; }
// xor ecx, ecx; cmp eax, 0; setl cl; dec ecx; and ecx, eax: below zero, 0.
U FloorZero(U v) { return I(v) < 0 ? 0u : v; }

float F(const unsigned char* p) {
    float f;
    std::memcpy(&f, p, sizeof f);
    return f;
}
float F(U a) { return F(At(a)); }
double D(U a) { return static_cast<double>(F(a)); }
void SetF(unsigned char* p, float f) { std::memcpy(p, &f, sizeof f); }
void SetD(unsigned char* p, double d) { SetF(p, static_cast<float>(d)); }
void SetD(U a, double d) { SetD(At(a), d); }
// fild dword: a whole number, exact in double.
double Fi(U v) { return static_cast<double>(I(v)); }
// fld dword / fstp dword: the bits kept, but a signalling NaN made quiet.
void CopyQuiet(unsigned char* to, const unsigned char* from) {
    U bits = UL(from);
    if ((bits & 0x7F800000u) == 0x7F800000u && (bits & 0x007FFFFFu) != 0) bits |= 0x00400000u;
    SetUL(to, bits);
}
// The CRT's _ftol 0x5B9550 on the value x87 holds: truncation through a 64-bit
// fistp, whose NaN and out-of-range answer is the integer indefinite
// 0x8000000000000000; the callers keep eax's low word.
U Ftol(double v) {
    if (!(v > -9.2233720368547758e18 && v < 9.2233720368547758e18)) return 0;
    return static_cast<U>(static_cast<std::uint64_t>(static_cast<std::int64_t>(v)));
}

unsigned char* Vertices() { return reinterpret_cast<unsigned char*>(Prim_VertexScratch); }
void SetV(unsigned offset, U v) { SetWord(Vertices() + offset, v); }
const short* Vx(unsigned i) { return reinterpret_cast<const short*>(Vertices() + 8 * i); }
float* Fp(unsigned char* p) { return reinterpret_cast<float*>(p); }
U VertexAddress(unsigned offset) { return AddressOf(Vertices()) + offset; }

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 2]; jmp [table + eax
// * 4]: the table's `entries` handlers read in place (the fuzz swaps the cells
// for recorders); a Fatal past them, where the original jumps through the dword
// after - another table or data.
void Dispatch(const char* who, U table, unsigned entries) {
    const unsigned sub = Sprite_Current[2];
    if (sub >= entries)
        bof3::Fatal("%s: sub-state byte +2 is %u, past the %u entries of 0x%X - the original jumps through the dword "
                    "after (docs/effect_6c.md section 7)",
                    who, sub, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(At(table + 4 * sub))))();
}
#define E6C_TABLE(name) AddressOf(name), name##_count

// A draw item by index: the pool's (DIV-0062), a Fatal past its count where the
// original reads past the array.
unsigned char* Item(const char* who, U item) {
    if (item >= draw_pool::Count())
        bof3::Fatal("%s: draw item %u, past the pool's %u - the original reads past DrawItems (docs/effect_6c.md "
                    "section 7)",
                    who, (unsigned)item, draw_pool::Count());
    return draw_pool::Items() + item * at::kDrawItemStride;
}

// The dword record of the map cell (x, z) at `run` records into its run, as
// the original indexes it: the cell's word at AreaMap_Header + (x + width * z
// + 2 * base) * 2, plus base, plus (height * width + 1) / 2 - every byte and
// word re-read where the original re-reads them.
unsigned char* CellRecord(U x, U z, U run) {
    const U header = UL(at::kAreaHeader);
    const U width = header & 0xFF;
    const U base = Word(At(at::kAreaCellBase));
    const U word = Word(At(at::kAreaHeader + 2 * (x + width * z + 2 * base)));
    const U height = (header >> 8) & 0xFF;
    const U half = (height * width + 1) / 2;   // cdq; sub eax, edx; sar 1 - never negative here
    return At(at::kAreaHeader + 4 * (base + word + half + run));
}

}  // namespace

// ===========================================================================
// 0x511C10: the map's height at a point (area 189's, not effect code)
// ===========================================================================

// original 0x511C10 (called by Area189_LeaderStart and Area189_StepBegin): the
// cell (x + 0x8000) >> 16, (z + 0x8000) >> 16 (bytes); its 16 x 16 block's byte
// from kBlockMap (by the high nibbles), whose nibbles with the cell's low ones
// index AreaMap_Corners; the corner dword's four s8 heights c0..c3 interpolated
// by the point's fraction about the cell's centre ((x - 0x8000) & 0xFFFF): the
// lower triangle (fx + fz <= 0x10000) from c0, toward c1 along x and c2 along
// z; the upper from c3. Each product divided by 0x10000 toward zero. Answers
// the height << 5 in eax (the callers keep ax).
extern "C" long __cdecl AreaMap_CornerHeight(long x, long z) {
    const U ux = static_cast<U>(x), uz = static_cast<U>(z);
    const U xi = Sar(ux + 0x8000u, 16) & 0xFF, zi = Sar(uz + 0x8000u, 16) & 0xFF;
    const U block = At(at::kBlockMap)[((xi >> 4) & 0xF) | (zi & 0xF0)];
    const U index = ((block & 0xF0) | (zi & 0xF)) * 0x60 + (((block & 0xF) << 4) | (xi & 0xF));
    const U corners = UL(at::kAreaCorners + 4 * index);
    const std::int32_t c0 = static_cast<signed char>(corners & 0xFF);
    const std::int32_t c1 = static_cast<signed char>((corners >> 8) & 0xFF);
    const std::int32_t c2 = static_cast<signed char>((corners >> 16) & 0xFF);
    const std::int32_t c3 = static_cast<signed char>(corners >> 24);
    const U fz = (uz - 0x8000u) & 0xFFFF, fx = (ux - 0x8000u) & 0xFFFF;
    U h;
    if (I(fz + fx) <= 0x10000) {
        const U along_z = DivP2(static_cast<U>(c2 - c0) * fz, 16);
        const U along_x = DivP2(static_cast<U>(c1 - c0) * fx, 16);
        h = along_x + static_cast<U>(c0) + along_z;
    } else {
        const U from_x = DivP2(static_cast<U>(c3 - c1) * (0x10000u - fz), 16);
        const U from_z = DivP2(static_cast<U>(c3 - c2) * (0x10000u - fx), 16);
        h = static_cast<U>(c3) - from_z - from_x;
    }
    return static_cast<long>(h << 5);
}

// ===========================================================================
// Sub-kind 0x44 (E6B's dispatcher 0x510C20; its state 1, 0x510C80, calls the
// first and tail-jumps to the second): the record on the leader, and the sky
// ===========================================================================

// original 0x511BB0 (called by every draw of sub-kind 0x44): Gfx_CommitPrim's
// test against another list - when (Gfx_BufferIndex << 16) + 0x7F1BAC is above
// the cursor + (size & 0xFF), Gpu_LinkPrim(the last pointer at 0x802B34 + 8 *
// Gfx_BufferIndex, cursor), that pointer = the cursor (read again), the cursor
// up the size; else nothing. The buffer byte is not checked, as
// Gfx_CommitPrim's slot is not.
extern "C" void __cdecl EffectKind18Sub44_Commit(unsigned size) {
    unsigned char* const next = Gfx_PacketNext;
    const unsigned buffer = Gfx_BufferIndex;
    const U bytes = size & 0xFFu;
    const U limit = (static_cast<U>(buffer) << 16) + at::kPoolLimit;
    if (limit <= AddressOf(next) + bytes) return;
    SH_CALL(Gpu_LinkPrim)(reinterpret_cast<unsigned long*>(static_cast<std::uintptr_t>(UL(at::kLayerTails + 8 * buffer))),
                          AddressOf(next));
    unsigned char* const now = Gfx_PacketNext;
    SetUL(at::kLayerTails + 8u * Gfx_BufferIndex, AddressOf(now));
    Gfx_PacketNext = now + bytes;
}

// original 0x510C90 (called by E6B's 0x510C80, sub-kind 0x44's state 1): the
// record kept on the leader's animation (Sprite_EnsureAnimation when +0x4B
// differs, then +0x4B = the leader's, read again); the leader in state 3 runs
// Sprite_ScriptTick. Cond_ByteFE 1: the colour cells +0x5D..+0x5F = 0x7F, 0x80,
// 0x80 and Cond_ByteFE 0; else 0, 0, 0. +0x32 = 0x3200; the vertex
// (((Field_Kind2X - 1) & 0xFFFFFF) + 1 >> 9) - 0x4000, the same of Field_Kind2Z,
// -(the leader's +0x3E / 2) projected into MapView_ScreenXY; +0x74 160.0f, +0x2E
// 0xA0; the leader's +0x14 set: +0x78 = the screen y (a copy), +0x30 and the
// leader's +0x30 = it truncated; else +0x78 = the leader's +0x30 and +0x30 =
// it. The record put on Sprite_DrawList while it has room (40). Cond_ByteFE 2:
// a red additive tile over the frame - DIV-0041's fill - and Cond_ByteFE 0.
extern "C" void __cdecl EffectKind18Sub44_Follow(void) {
    if (S()[0x4B] != *At(at::kLeaderAnimation)) {
        SH_CALL(Sprite_EnsureAnimation)(*At(at::kLeaderAnimation));
        S()[0x4B] = *At(at::kLeaderAnimation);
    }
    if (*At(at::kLeaderState) == 3) SH_CALL(Sprite_ScriptTick)();
    if (Cond_ByteFE == 1) {
        S()[0x5D] = 0x7F;
        S()[0x5E] = 0x80;
        S()[0x5F] = 0x80;
        Cond_ByteFE = 0;
    } else {
        S()[0x5D] = 0;
        S()[0x5E] = 0;
        S()[0x5F] = 0;
    }
    SetWord(S() + 0x32, 0x3200);
    SetV(0, Sar(((static_cast<U>(Field_Kind2X) - 1u) & 0xFFFFFFu) + 1u, 9) - 0x4000u);
    SetV(4, 0u - DivP2(static_cast<U>(S16(at::kLeaderLift)), 1));
    SetV(2, Sar(((static_cast<U>(Field_Kind2Z) - 1u) & 0xFFFFFFu) + 1u, 9) - 0x4000u);
    long p;
    SH_CALL(Gte_RotTransPers)(Vx(0), reinterpret_cast<unsigned long*>(MapView_ScreenXY), &p);
    SetUL(S() + 0x74, 0x43200000u);   // 160.0f
    SetWord(S() + 0x2E, 0xA0);
    if (UL(at::kLeaderMoving) != 0) {
        CopyQuiet(S() + 0x78, At(at::kScreenY));
        SetWord(S() + 0x30, Ftol(D(at::kScreenY)));
        SetWord(At(at::kLeaderDepth), Ftol(D(at::kScreenY)));
    } else {
        SetD(S() + 0x78, Fi(static_cast<U>(S16(at::kLeaderDepth))));
        unsigned char* const s = S();
        SetWord(s + 0x30, Ftol(static_cast<double>(F(s + 0x78))));
    }
    const unsigned count = Sprite_DrawListCount;
    if (count < 0x28) {
        SetUL(at::kDrawList + 4 * count, AddressOf(S()));
        Sprite_DrawListCount = static_cast<unsigned char>(count + 1);
    }
    if (Cond_ByteFE == 2) {
        SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0xB5, 0);
        SH_CALL(Gfx_CommitPrim)(3, 0xC);
        unsigned char* const prim = Gfx_PacketNext;
        SH_CALL(Gpu_SetTile)(prim);
        SH_CALL(Gpu_SetSemiTrans)(prim, 1);
        SetUL(prim + 8, std::bit_cast<U>(Widescreen_FillX()));       // DIV-0041: (-53, 0) 426 wide under the wide picture
        SetUL(prim + 0xC, 0);
        SetUL(prim + 0x14, std::bit_cast<U>(Widescreen_FillWidth()));  // 0x43A00000, 320.0f narrow
        SetUL(prim + 0x18, 0x43700000u);                               // 240.0f
        prim[4] = 0xFF;
        prim[5] = 0;
        prim[6] = 0;
        SH_CALL(Gte_StoreDepthF)(Fp(prim + 0x10));
        SH_CALL(Gfx_CommitPrim)(3, 0x1C);
        Cond_ByteFE = 0;
    }
}

namespace {

// (angle * 10) >> 5 (lea; shl 1; sar 5): the sky's screen x of an angle.
U AngleX(U angle) { return Sar(angle * 10u, 5); }

// An (r, g, b) from the vertex words of row `row` (0x9037A0 + 8 * row, + 2,
// + 4): their low bytes.
void RowColour(unsigned char* to, unsigned row) {
    const unsigned char* const v = Vertices() + 8 * row;
    to[0] = v[0];
    to[1] = v[2];
    to[2] = v[4];
}

// A rect of four words allocated at the cursor (the cursor up 8), as the sky
// and sub-kind 0x66 do before a draw mode that names it.
unsigned char* Rect(U x, U y, U w, U h) {
    unsigned char* const r = Gfx_PacketNext;
    Gfx_PacketNext = r + 8;
    SetWord(r, x);
    SetWord(r + 2, y);
    SetWord(r + 6, h);
    SetWord(r + 4, w);
    return r;
}

// A star's shade: (Rand() & 0x7F) * level / 128, toward zero.
unsigned char Twinkle(U level) {
    const U r = static_cast<U>(SH_CALL(Rand)());
    return static_cast<unsigned char>(DivP2((r & 0x7F) * level, 7));
}

}  // namespace

// original 0x511D50 (called by EffectKind18Sub44_DrawStars after each star):
// four TILE_1s about the star `tile` at the s8 offsets kTwinkle, each its
// colour >> 2 (the star's prim read after each new tile is set up), committed
// (0x14). The original clears ax on return; no caller reads it.
extern "C" void __cdecl EffectKind18Sub44_DrawTwinkle(unsigned char* tile) {
    for (unsigned i = 0; i < 4; ++i) {
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetTile1)(p);
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        SetD(p + 8, Fi(static_cast<U>(S8(at::kTwinkle + 2 * i))) + static_cast<double>(F(tile + 8)));
        SetD(p + 0xC, Fi(static_cast<U>(S8(at::kTwinkle + 2 * i + 1))) + static_cast<double>(F(tile + 0xC)));
        p[4] = static_cast<unsigned char>(tile[4] >> 2);
        p[5] = static_cast<unsigned char>(tile[5] >> 2);
        p[6] = static_cast<unsigned char>(tile[6] >> 2);
        SH_CALL(EffectKind18Sub44_Commit)(0x14);
    }
}

// original 0x511740 (called by EffectKind18Sub44_Draw when the vertex word
// 0x9037A8 is not 0, that word its level): a draw mode (page 0xB5, dtd); up to
// six TILE_1 stars, each of the first three twinkled
// (EffectKind18Sub44_DrawTwinkle): one at the camera's angle + 0x200, y 45; one
// at + 0x400 and y Field_Kind2Z / 85 - 1; one at y |0x1400 - z|^2 / 98304 +
// (x past 0xF00) (x - 0xF00) / 25 + 8; three of a size from the camera's
// distance to (0x1480, 0x1380), placed by Math_Ratan2 of a clamped point - each
// of the last five only while its y (size) is inside [0, 90) ([.., 89]). The
// shades from Rand and the level. The y is stored into the cursor's prim
// before the test, drawn or not.
extern "C" void __cdecl EffectKind18Sub44_DrawStars(int level_word) {
    const U level = static_cast<U>(level_word);
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0xB5, 0);
    SH_CALL(EffectKind18Sub44_Commit)(0xC);
    const unsigned char shade = static_cast<unsigned char>(DivP2(level * 0xFFu, 7));
    {
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetTile1)(p);
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        SetUL(p + 0xC, 0x42340000u);   // 45.0f
        SetD(p + 8, Fi(AngleX((Cond_AngleFB + 0x200u) & 0xFFF)));
        p[4] = shade;
        p[5] = Twinkle(level);
        p[6] = Twinkle(level);
        SH_CALL(EffectKind18Sub44_Commit)(0x14);
        SH_CALL(EffectKind18Sub44_DrawTwinkle)(p);
    }
    {
        unsigned char* const p = Gfx_PacketNext;
        const float y = static_cast<float>(Fi(static_cast<U>(S16(at::kKind2ZHigh) / 85 - 1)));
        SetF(p + 0xC, y);
        if (!(y < F(at::kZero)) && y < F(at::kNinety)) {
            SH_CALL(Gpu_SetTile1)(p);
            SH_CALL(Gpu_SetSemiTrans)(p, 1);
            p[4] = shade;
            SetD(p + 8, Fi(AngleX((Cond_AngleFB + 0x400u) & 0xFFF)));
            p[5] = Twinkle(level);
            p[6] = Twinkle(level);
            SH_CALL(EffectKind18Sub44_Commit)(0x14);
            SH_CALL(EffectKind18Sub44_DrawTwinkle)(p);
        }
    }
    {
        unsigned char* const p = Gfx_PacketNext;
        const U dz = Abs(0x1400u - static_cast<U>(S16(at::kKind2ZHigh)));
        const U x_word = Word(At(at::kKind2XHigh));
        const U past = static_cast<std::int16_t>(x_word & 0xFF00u) >= 0xF00 ? 1u : 0u;
        const std::int32_t step = (static_cast<std::int16_t>(x_word) - 0xF00) / 25;
        const U y = static_cast<U>(I(dz * dz) / 98304) + past * static_cast<U>(step) + 8u;
        const float fy = static_cast<float>(Fi(y));
        SetF(p + 0xC, fy);
        if (!(fy < F(at::kZero)) && fy < F(at::kNinety)) {
            SH_CALL(Gpu_SetTile1)(p);
            SH_CALL(Gpu_SetSemiTrans)(p, 1);
            const U x = Sar((static_cast<U>(static_cast<std::int16_t>(Cond_AngleFB & 0xFFFF)) - 0x200u) * 10u, 5) +
                        static_cast<U>((S16(at::kKind2ZHigh) - 0x1480) / 17);
            SetD(p + 8, Fi(x));
            for (unsigned c = 4; c < 7; ++c) {
                const U r = static_cast<U>(SH_CALL(Rand)());
                p[c] = static_cast<unsigned char>(DivP2(((r - 0x80u) & 0xFF) * level, 7));
            }
            SH_CALL(EffectKind18Sub44_Commit)(0x14);
            SH_CALL(EffectKind18Sub44_DrawTwinkle)(p);
        }
    }
    const std::int32_t x = S16(at::kKind2XHigh), z = S16(at::kKind2ZHigh);
    U ax = Abs(static_cast<U>(x - 0x1480)), az = Abs(static_cast<U>(z - 0x1380));
    if (I(ax) > 0x900) ax = 0x900;
    if (I(az) > 0x400) az = 0x400;
    U size = static_cast<U>(I(0xD00u - az - ax) / 43 + 0x14);
    if (I(size) < 0x28) size = 0x28;
    const std::int32_t cx = static_cast<std::int16_t>(Word(At(at::kKind2XHigh))) < 0xB00 ? 0xB00 : (x > 0x1E00 ? 0x1E00 : x);
    const std::int32_t cz = static_cast<std::int16_t>(Word(At(at::kKind2ZHigh))) < 0xF00 ? 0xF00 : (z > 0x1800 ? 0x1800 : z);
    for (U back = 0; I(back) < 0x1E; back += 0xA, size += 8) {
        unsigned char* const p = Gfx_PacketNext;
        const float fs = static_cast<float>(Fi(size));
        SetF(p + 0xC, fs);
        if (fs > F(at::kEightyNine)) continue;
        SH_CALL(Gpu_SetTile1)(p);
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        const U r = static_cast<U>(SH_CALL(Math_Ratan2)(static_cast<float>(cz - 0x1380), static_cast<float>(cx - 0x1480)));
        SetD(p + 8, Fi(AngleX((r + Cond_AngleFB + 0x633u) & 0xFFF) - back));
        p[4] = Twinkle(level);
        p[5] = Twinkle(level);
        p[6] = shade;
        SH_CALL(EffectKind18Sub44_Commit)(0x14);
    }
}

// original 0x5115A0 (called twice by EffectKind18Sub44_Draw): a glow at screen
// x: a textured quad (x .. x + 0x4E, 72 .. 102) shaded `level` (page 0xD7,
// CLUT 0x7A00), then two more over it (page 0xB7) shaded by the vertex words
// of rows 0 and 1 times level / 128 (their u stepped 0x50, CLUT 0x1E6 + row),
// each committed (0x48).
extern "C" void __cdecl EffectKind18Sub44_DrawGlow(int level_word, int x_word) {
    const U level = static_cast<U>(level_word), x = static_cast<U>(x_word);
    const float left = static_cast<float>(Fi(x)), right = static_cast<float>(Fi(x + 0x4Eu));
    auto corners = [&](unsigned char* p) {
        SetF(p + 8, left);
        SetUL(p + 0xC, 0x42900000u);   // 72.0f
        SetF(p + 0x18, right);
        SetUL(p + 0x1C, 0x42900000u);
        SetF(p + 0x28, left);
        SetUL(p + 0x2C, 0x42CC0000u);  // 102.0f
        SetF(p + 0x38, right);
        SetUL(p + 0x3C, 0x42CC0000u);
    };
    {
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyFT4)(p);
        SH_CALL(Gpu_SetShadeTex)(p, 0);
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        corners(p);
        p[4] = p[5] = p[6] = static_cast<unsigned char>(level);
        SetWord(p + 0x26, 0xD7);
        SetWord(p + 0x16, 0x7A00);
        p[0x14] = 0;
        p[0x15] = 0xC0;
        p[0x24] = 0x4F;
        p[0x25] = 0xC0;
        p[0x34] = 0;
        p[0x35] = 0xDF;
        p[0x44] = 0x4F;
        p[0x45] = 0xDF;
        SH_CALL(EffectKind18Sub44_Commit)(0x48);
    }
    for (U row = 0; row < 2; ++row) {
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyFT4)(p);
        SH_CALL(Gpu_SetShadeTex)(p, 0);
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        corners(p);
        for (unsigned c = 0; c < 3; ++c)
            p[4 + c] = static_cast<unsigned char>(DivP2(static_cast<U>(S16(VertexAddress(8 * row + 2 * c))) * level, 7));
        SetWord(p + 0x26, 0xB7);
        p[0x15] = 0xC0;
        p[0x25] = 0xC0;
        SetWord(p + 0x16, (row + 0x1E6) << 6);
        const unsigned char u = static_cast<unsigned char>(row * 0x50);
        p[0x14] = u;
        p[0x24] = static_cast<unsigned char>(u + 0x4F);
        p[0x34] = u;
        p[0x35] = 0xDF;
        p[0x44] = static_cast<unsigned char>(u + 0x4F);
        p[0x45] = 0xDF;
        SH_CALL(EffectKind18Sub44_Commit)(0x48);
    }
}

// original 0x510EB0 (E6B's 0x510C80 tail-jumps to it): sub-kind 0x44's sky,
// in screen space. A draw mode with a texture window (0x20, 0, 0x20, 0x20) and
// a textured band (0, 90)..(320, 122) at slot 7; a draw mode with the window
// (0, 0, 0x100, 0x100) at slot 7; a draw mode (page 0xD5) and a white tile (0,
// 50) 320 x 40 through EffectKind18Sub44_Commit. By area 189's frame word
// (0x90405C) the vertex words 0x9037A0.. are two colour rows and the CLUT rows
// 6.. are tinted (Gfx_ClutAdjust): below 0x1B0 rows (0, 0, 0) / (0x80, ..);
// below 0x1E0 a ramp (n = (w - 0x1B0) / 3); below 0x390 (0x80, ..) / (0, ..);
// else n = (w - 0x390) / 3 into the sixteen tints of kSkyTints. Two rows of
// three textured quads (x stepping 0x100 from the camera's angle, y 0..90)
// coloured by the rows; the stars (the word 0x9037A8 their level); two layers
// (pages 0xB5, 0xD5) of four Gouraud bands between kSkyXs, from y 90 down to
// kSkyYs, coloured by the rows; and the two glows: one by the camera's distance
// from (0x1780, 0x1380), one - flag 4 of 0x904000 set - by its distance from
// (0x1780, 0xE00), placed by Math_Ratan2. The bands are laid out from the
// cursor as it was after their draw mode (0x44 apart), as the original does.
extern "C" void __cdecl EffectKind18Sub44_Draw(void) {
    {
        unsigned char* const window = Rect(0x20, 0, 0x20, 0x20);
        SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x95, AddressOf(window));
        SH_CALL(Gfx_CommitPrim)(7, 0xC);
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyFT4)(p);
        SH_CALL(Gpu_SetShadeTex)(p, 1);
        SetUL(p + 0xC, 0x42B40000u);   // 90.0f
        SetUL(p + 0x1C, 0x42B40000u);
        SetUL(p + 0x2C, 0x42F40000u);  // 122.0f
        SetUL(p + 0x3C, 0x42F40000u);
        SetUL(p + 8, 0);
        SetUL(p + 0x18, 0x43A00000u);  // 320.0f
        SetUL(p + 0x28, 0);
        SetUL(p + 0x38, 0x43A00000u);
        p[0x14] = 0;
        p[0x15] = 0;
        p[0x24] = 0xFF;
        p[0x25] = 0;
        p[0x34] = 0;
        p[0x35] = 0x40;
        p[0x44] = 0xFF;
        p[0x45] = 0x40;
        SetWord(p + 0x26, 0x95);
        SetWord(p + 0x16, 0x7900);
        SH_CALL(Gfx_CommitPrim)(7, 0x48);
    }
    {
        unsigned char* const window = Rect(0, 0, 0x100, 0x100);
        SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x95, AddressOf(window));
        SH_CALL(Gfx_CommitPrim)(7, 0xC);
        SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0xD5, 0);
        SH_CALL(EffectKind18Sub44_Commit)(0xC);
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetTile)(p);
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        SetUL(p + 8, 0);
        SetUL(p + 0xC, 0x42480000u);   // 50.0f
        SetUL(p + 0x14, 0x43A00000u);  // 320.0f
        SetUL(p + 0x18, 0x42200000u);  // 40.0f
        p[4] = p[5] = p[6] = 0xFF;
        SH_CALL(EffectKind18Sub44_Commit)(0x1C);
    }
    const U clock = Word(At(at::kWalkClock));
    if (clock < 0x1B0) {
        SetV(0xC, 0x80);
        SetV(0xA, 0x80);
        SetV(8, 0x80);
        SetV(4, 0);
        SetV(2, 0);
        SetV(0, 0);
    } else if (clock < 0x1E0) {
        const U n = (clock - 0x1B0) / 3;
        SetV(4, n * 8);
        SetV(2, n * 8);
        SetV(0, n * 8);
        SetV(0xC, (0x10 - n) << 3);
        SetV(0xA, (0x10 - n) << 3);
        SetV(8, (0x10 - n) << 3);
        SH_CALL(Gfx_ClutAdjust)(0, 6, I(n * 4) / 10 - 6, I(n) / 8 - 2, 0);
    } else if (clock < 0x390) {
        SetV(4, 0x80);
        SetV(2, 0x80);
        SetV(0, 0x80);
        SetV(0xC, 0);
        SetV(0xA, 0);
        SetV(8, 0);
    } else {
        const U n = (clock - 0x390) / 3;
        if (n >= at::kSkyTintSteps)
            bof3::Fatal("EffectKind18Sub44_Draw: the frame word 0x%X is step %u, past the %u tints of 0x%X - the "
                        "original reads on into the twinkle offsets (docs/effect_6c.md section 7)",
                        (unsigned)clock, (unsigned)n, at::kSkyTintSteps, (unsigned)at::kSkyTints);
        const U past = FloorZero(n - 6);
        SetV(0, 0x80u - past * 12u);
        SetV(4, (0x10 - n) << 3);
        SetV(2, (0x10 - n) << 3);
        SetV(0xC, n * 8);
        SetV(0xA, n * 8);
        SetV(8, n * 8);
        SH_CALL(Gfx_ClutAdjust)(0, 6, S8(at::kSkyTints + 3 * n), S8(at::kSkyTints + 3 * n + 1),
                                S8(at::kSkyTints + 3 * n + 2));
    }
    for (U row = 0; row < 2; ++row) {
        const U clut = (row + 0x1E6) << 6;
        const unsigned char v0 = static_cast<unsigned char>(row * 0x60), v1 = static_cast<unsigned char>(v0 + 0x5A);
        for (U column = 0; I(column) < 0x300; column += 0x100) {
            unsigned char* const p = Gfx_PacketNext;
            SH_CALL(Gpu_SetPolyFT4)(p);
            SH_CALL(Gpu_SetShadeTex)(p, 0);
            SH_CALL(Gpu_SetSemiTrans)(p, 1);
            SetUL(p + 0xC, 0);
            SetD(p + 8, Fi((AngleX(Cond_AngleFB) & 0xFF) + column - 0x100u));
            SetUL(p + 0x1C, 0);
            SetD(p + 0x18, Fi((AngleX(Cond_AngleFB) & 0xFF) + column));
            SetUL(p + 0x2C, 0x42B40000u);   // 90.0f
            SetD(p + 0x28, Fi((AngleX(Cond_AngleFB) & 0xFF) + column - 0x100u));
            SetUL(p + 0x3C, 0x42B40000u);
            SetD(p + 0x38, Fi((AngleX(Cond_AngleFB) & 0xFF) + column));
            RowColour(p + 4, row);
            SetWord(p + 0x16, clut);
            p[0x15] = v0;
            p[0x25] = v0;
            SetWord(p + 0x26, 0xB7);
            p[0x14] = 0;
            p[0x24] = 0xFF;
            p[0x34] = 0;
            p[0x35] = v1;
            p[0x44] = 0xFF;
            p[0x45] = v1;
            SH_CALL(EffectKind18Sub44_Commit)(0x48);
        }
    }
    const U stars = Word(Vertices() + 8);
    if (stars != 0) SH_CALL(EffectKind18Sub44_DrawStars)(static_cast<std::int16_t>(stars));
    for (U layer = 0x20, row = 0; I(layer) < 0x60; layer += 0x20, ++row) {
        SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, (layer & 0x60) | 0x95, 0);
        SH_CALL(EffectKind18Sub44_Commit)(0xC);
        unsigned char* const first = Gfx_PacketNext;
        for (U m = 0; m < 4; ++m) {
            unsigned char* const p = first + 0x44 * m;
            SH_CALL(Gpu_SetPolyG4)(p);
            SH_CALL(Gpu_SetSemiTrans)(p, 1);
            SetD(p + 8, Fi(Word(At(at::kSkyXs + 2 * m))));
            SetUL(p + 0xC, 0x42B40000u);   // 90.0f
            SetD(p + 0x18, Fi(Word(At(at::kSkyXs + 2 * m + 2))));
            SetUL(p + 0x1C, 0x42B40000u);
            SetD(p + 0x28, Fi(Word(At(at::kSkyXs + 2 * m))));
            SetD(p + 0x2C, Fi(*At(at::kSkyYs + m)));
            SetD(p + 0x38, Fi(Word(At(at::kSkyXs + 2 * m + 2))));
            SetD(p + 0x3C, Fi(*At(at::kSkyYs + m + 1)));
            RowColour(p + 4, row);
            RowColour(p + 0x14, row);
            p[0x24] = p[0x25] = p[0x26] = 0;
            p[0x34] = p[0x35] = p[0x36] = 0;
            SH_CALL(EffectKind18Sub44_Commit)(0x44);
        }
    }
    {
        const U near = Abs(static_cast<U>(S16(at::kKind2XHigh) - 0x1780)) + Abs(static_cast<U>(S16(at::kKind2ZHigh) - 0x1380));
        U level = 0xC0u - static_cast<U>(I(near) / 3);
        if (I(level) > 0x80) level = 0x80;
        level = FloorZero(level);
        if (level != 0)
            SH_CALL(EffectKind18Sub44_DrawGlow)(I(level), I(AngleX(((Cond_AngleFB - 0x800u) & 0xFFF) - 0x400u) - 0x28u));
    }
    if (SH_CALL(Flags_Test)(At(at::kFlagRow), 4) == 0) return;
    const std::int32_t x = S16(at::kKind2XHigh), z = S16(at::kKind2ZHigh);
    const U dx = static_cast<U>(x - 0x1780), dz = static_cast<U>(z - 0xE00);
    U level = 0x100u - DivP2(Abs(dz) + Abs(dx), 2);
    if (I(level) > 0x80) level = 0x80;
    level = FloorZero(level);
    if (level == 0) return;
    const U r = static_cast<U>(SH_CALL(Math_Ratan2)(static_cast<float>(I(dx)), static_cast<float>(-z)));
    SH_CALL(EffectKind18Sub44_DrawGlow)(I(level), I(AngleX(((r + Cond_AngleFB - 0x400u) & 0xFFF) - 0x200u) - 0x28u));
}

// ===========================================================================
// Sub-kind 0x45: EffectKind18_States[0x45] (0x654180), EffectKind18Sub45_States
// (three) by +2
// ===========================================================================

// original 0x512040 (called by sub-kind 0x45's two drawing states): a ring of
// 32 textured quads round the fixed point (-14912, -12096, -960) - written into
// MapView_ScreenXY and the cell after it first - between the radius `outer`
// (vertices 0, 1) and `inner` (2, 3), each quad 0x1000 of angle (/32 for
// Math_Sin / Math_Cos, the products >> 13); projected, semi-transparency 0, a
// page by the half (((half << 7) + 0x140) >> 6 & 0xF | 0x90), grey 0x50, the u
// and v the same angles' products >> 16 plus the half's pair of kRingUv, linked
// at the record's point (dy 0, 0x48).
extern "C" void __cdecl EffectKind18Sub45_DrawRing(int outer_word, int inner_word, int half_word) {
    const U outer = static_cast<U>(outer_word), inner = static_cast<U>(inner_word), half = static_cast<U>(half_word);
    SetUL(AddressOf(MapView_ScreenXY), 0xC6690000u);   // -14912.0f
    SetUL(at::kScreenY, 0xC63D0000u);                   // -12096.0f
    SetUL(at::kScreenZ, 0xC4700000u);                   // -960.0f
    if (half >= at::kRingHalves)
        bof3::Fatal("EffectKind18Sub45_DrawRing: the half %u, past the %u pairs of 0x%X - the original reads sub-kind "
                    "0x51's rectangles (docs/effect_6c.md section 7)",
                    (unsigned)half, at::kRingHalves, (unsigned)at::kRingUv);
    for (U angle = 0; I(angle) < 0x20000; angle += 0x1000) {
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyFT4)(p);
        SH_CALL(Gpu_SetShadeTex)(p, 0);
        for (unsigned v = 0; v < 4; ++v) {
            const U t = angle + ((v & 1) ? 0x1000u : 0u);
            const U radius = v < 2 ? outer : inner;
            SetV(8 * v, Ftol(D(AddressOf(MapView_ScreenXY))));
            const U q = DivP2(t, 5);
            const U sine = Sar(static_cast<U>(SH_CALL(Math_Sin)(I(q))) * radius, 13);
            SetV(8 * v + 2, Ftol(D(at::kScreenY) - Fi(sine)));
            const U cosine = Sar(static_cast<U>(SH_CALL(Math_Cos)(I(q))) * radius, 13);
            SetV(8 * v + 4, Ftol(Fi(cosine) + D(at::kScreenZ)));
        }
        long depth;
        SH_CALL(Gte_RotTransPers4)(Vx(0), Vx(1), Vx(2), Vx(3), Fp(p + 8), Fp(p + 0x18), Fp(p + 0x28), Fp(p + 0x38), &depth);
        SH_CALL(Gte_PrimDepths4_10)(p);
        SH_CALL(Gpu_SetSemiTrans)(p, 0);
        p[6] = 0x50;
        p[5] = 0x50;
        p[4] = 0x50;
        SetWord(p + 0x26, (Sar((half << 7) + 0x140u, 6) & 0xF) | 0x90u);
        SetWord(p + 0x16, 0x7940);
        const U q0 = DivP2(angle, 5), q1 = DivP2(angle + 0x1000u, 5);
        const unsigned char* const uv = At(at::kRingUv + 2 * half);
        struct Corner { unsigned at; U q; U radius; };
        const Corner corners[] = {{0x14, q0, outer}, {0x24, q1, outer}, {0x34, q0, inner}, {0x44, q1, inner}};
        for (const Corner& c : corners) {
            const U sine = static_cast<U>(SH_CALL(Math_Sin)(I(c.q)));
            p[c.at] = static_cast<unsigned char>(Sar(sine * c.radius, 16) + uv[0]);
            const U cosine = static_cast<U>(SH_CALL(Math_Cos)(I(c.q)));
            p[c.at + 1] = static_cast<unsigned char>(Sar(cosine * c.radius, 16) + uv[1]);
        }
        unsigned char* const s = S();
        SH_CALL(MapView_LinkPrimAt)(UL(s + 0x34), UL(s + 0x38), 0, 0x48);
    }
}

// original 0x511DD0 (EffectKind18_States[0x45], hidden in 0x511D50): jmp
// [EffectKind18Sub45_States + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub45_Run(void) {
    Dispatch("EffectKind18Sub45_Run", E6C_TABLE(EffectKind18Sub45_States));
}

// original 0x511DF0 (sub-state 0): once Cond_ByteFE is 1, +0xB 0, the radius
// +0x3C 0, +2 up. Nothing drawn.
extern "C" void __cdecl EffectKind18Sub45_Wait(void) {
    if (Cond_ByteFE != 1) return;
    S()[0xB] = 0;
    SetUL(S() + 0x3C, 0);
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
}

namespace {
// v - back, at least 0 and (where `cap`) at most 0x100 past 0xFF.
U Ring(U v, U back, bool cap) {
    const U r = v - back;
    if (I(r) < 0) return 0;
    if (cap && I(r) > 0xFF) return 0x100;
    return r;
}
}  // namespace

// original 0x511E20 (sub-state 1): the radius +0x3C up 8; one ring from it (at
// most 0x100) in to it - 0x20 (at least 0), the half 1; once the inner edge is
// past 0xFF the radius 0 and +2 up.
extern "C" void __cdecl EffectKind18Sub45_Open(void) {
    SetUL(S() + 0x3C, UL(S() + 0x3C) + 8u);
    const U v = UL(S() + 0x3C);
    const U outer = I(v) > 0xFF ? 0x100u : v;
    const U inner = Ring(v, 0x20, false);
    SH_CALL(EffectKind18Sub45_DrawRing)(I(outer), I(inner), 1);
    if (I(inner) <= 0xFF) return;
    SetUL(S() + 0x3C, 0);
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
}

// original 0x511E80 (sub-state 2): the radius up 8; eight rings, the radii v,
// v - 0x20, .. v - 0x100 each kept to 0..0x100, the halves 1, 0, 1, ...; once
// the first is past 0xFF (and +0xB is 0) MoveCmd_TestFB(0xB, 0x22) and +0xB 1;
// once the last is, Cond_ByteFE 2 and the record released.
extern "C" void __cdecl EffectKind18Sub45_Spread(void) {
    SetUL(S() + 0x3C, UL(S() + 0x3C) + 8u);
    const U v = UL(S() + 0x3C);
    U r[9];
    r[0] = I(v) > 0xFF ? 0x100u : v;
    for (unsigned k = 1; k < 9; ++k) r[k] = Ring(v, 0x20 * k, true);
    for (unsigned k = 0; k < 8; ++k) SH_CALL(EffectKind18Sub45_DrawRing)(I(r[k]), I(r[k + 1]), (k & 1) ? 0 : 1);
    if (I(r[0]) > 0xFF && S()[0xB] == 0) {
        SH_CALL(MoveCmd_TestFB)(0xB, 0x22);
        S()[0xB] = 1;
    }
    if (I(r[8]) > 0xFF) {
        Cond_ByteFE = 2;
        SH_CALL(Effect_Release)();
    }
}

// ===========================================================================
// Sub-kind 0x51: EffectKind18_States[0x51] (0x6541B0), EffectKind18Sub51_States
// (three) by +2
// ===========================================================================

namespace {
// The variant +0x36 (s16) of the record now current, checked against the five
// rectangles of kRect51.
U Variant51(const char* who) {
    const std::int32_t v = S16(S() + 0x36);
    if (v < 0 || v >= static_cast<std::int32_t>(at::kRect51Variants))
        bof3::Fatal("%s: the variant +0x36 is %d, past the %u rectangles of 0x%X - the original reads on into "
                    "EffectKind18Sub51_States (docs/effect_6c.md section 7)",
                    who, (int)v, at::kRect51Variants, (unsigned)at::kRect51);
    return static_cast<U>(v);
}
U Rect51(const char* who, unsigned field) { return At(at::kRect51 + 4 * Variant51(who) + field)[0]; }

// Each cell of the variant's rectangle, z rows outer, x inner, every bound read
// again from the record current at that point (as the original re-reads +0x36):
// `cell(x, z)`.
template <typename F> void ForCells(const char* who, F cell) {
    for (U z = Rect51(who, 1); I(z) <= I(Rect51(who, 3)); ++z)
        for (U x = Rect51(who, 0); I(x) <= I(Rect51(who, 2)); ++x) cell(x, z);
}
}  // namespace

// original 0x512350 (EffectKind18_States[0x51], hidden in 0x512040): jmp
// [EffectKind18Sub51_States + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub51_Run(void) {
    Dispatch("EffectKind18Sub51_Run", E6C_TABLE(EffectKind18Sub51_States));
}

// original 0x512370 (sub-state 0): +9 0; each cell of the variant's rectangle:
// its first record's bits 19..23 cleared and, where MapView_ItemAt answers a
// draw item, Prim_SetTexture(the record, the item, 2) - DIV-0062's site
// 0x51242A; +2 up.
extern "C" void __cdecl EffectKind18Sub51_Place(void) {
    S()[9] = 0;
    ForCells("EffectKind18Sub51_Place", [](U x, U z) {
        unsigned char* const record = CellRecord(x, z, 0);
        SetUL(record, UL(record) & 0xFF07FFFFu);
        const U item = static_cast<U>(SH_CALL(MapView_ItemAt)(I(x), I(z)));
        if (item != 0) SH_CALL(Prim_SetTexture)(UL(record), Item("EffectKind18Sub51_Place", item), 2);
    });
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
}

// original 0x512490 (sub-state 1; its switch on the variant +0x36, 0..4, has
// five cases, the fourth the cut's start 0x5124C0): +2 up once - variant 0
// flag 0x14 of 0x904000, 1 Cond_ByteFE 1, 2..4 story flags 0x7A..0x7C - is set.
// A variant past 4 does nothing.
extern "C" void __cdecl EffectKind18Sub51_Wait(void) {
    unsigned char* const s = S();
    const U v = static_cast<U>(S16(s + 0x36));
    if (v > 4) return;
    if (v == 1) {
        if (Cond_ByteFE == 1) s[2] = static_cast<unsigned char>(s[2] + 1);
        return;
    }
    const bool set = v == 0 ? SH_CALL(Flags_Test)(At(at::kFlagRow), 0x14) != 0
                            : SH_CALL(Flags_Test)(At(at::kStoryFlags), 0x7A + (v - 2)) != 0;
    if (set) S()[2] = static_cast<unsigned char>(S()[2] + 1);
}

// original 0x512510 (sub-state 2; a start no list had, reached by
// EffectKind18Sub51_States[2]): +9 up; each cell of the rectangle: its first
// record's bits 19..23 = min(+9, 0x10) and, where MapView_ItemAt answers an
// item, Prim_SetTexture(the record, the item's half for this buffer, 1) -
// DIV-0062's site 0x512607; once +9 is past 0x11 the record released.
extern "C" void __cdecl EffectKind18Sub51_Fade(void) {
    S()[9] = static_cast<unsigned char>(S()[9] + 1);
    const U step = S()[9] > 0x10 ? 0x10u : S()[9];
    ForCells("EffectKind18Sub51_Fade", [step](U x, U z) {
        unsigned char* const record = CellRecord(x, z, 0);
        SetUL(record, (UL(record) & 0xFF07FFFFu) | (step << 19));
        const U item = static_cast<U>(SH_CALL(MapView_ItemAt)(I(x), I(z)));
        if (item == 0) return;
        const U half = Gfx_BufferIndex + item * 2;
        if (half >= 2 * draw_pool::Count())
            bof3::Fatal("EffectKind18Sub51_Fade: the item half %u, past the pool's %u items - the original reads past "
                        "DrawItems (docs/effect_6c.md section 7)",
                        (unsigned)half, draw_pool::Count());
        SH_CALL(Prim_SetTexture)(UL(record), draw_pool::Items() + half * at::kItemHalf, 1);
    });
    if (S()[9] > 0x11) SH_CALL(Effect_Release)();
}

// ===========================================================================
// Sub-kind 0x53: EffectKind18_States[0x53] (0x6541B8) - one state
// ===========================================================================

// original 0x512660 (hidden in 0x512040): on +2 = 0 +0x3C = the ground at the
// record's point (AreaMap_Elevation, an s16) and +2 up. Each frame, while the
// camera's cell is within 20 (|dx| + |dz|) of the record's (+0x36, +0x3A): an
// additive draw mode linked at the record (dy 1); +9 up 2; sixteen rings k of
// sixteen TILE_1 dots, shaded 0xFF - ((+9 - 0x10 k) & 0xFF), each at angle
// ((Frame_Counter & 0xF) << 4) * kSpin53[k & 1] + 0x100 i round the record's
// point (3 * (sin >> 6) about (+0x34 >> 9) - 0x4000), lifted -(+0x3C / 2) -
// 2 ((+9 - 0x10 k) & 0xFF); projected, linked at the record (dy 1, 0x14); a
// draw mode closing it.
extern "C" void __cdecl EffectKind18Sub53_Run(void) {
    if (S()[2] == 0) {
        const unsigned char* const s = S();
        const long h = SH_CALL(AreaMap_Elevation)(Long(s + 0x34), Long(s + 0x38));
        SetUL(S() + 0x3C, static_cast<U>(static_cast<std::int16_t>(static_cast<U>(h) & 0xFFFF)));
        S()[2] = static_cast<unsigned char>(S()[2] + 1);
    }
    {
        const unsigned char* const s = S();
        const U near = Abs(static_cast<U>(S16(at::kKind2ZHigh) - S16(s + 0x3A))) + Abs(static_cast<U>(S16(at::kKind2XHigh) - S16(s + 0x36)));
        if (I(near) > 0x14) return;
    }
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0xB5, 0);
    {
        unsigned char* const s = S();
        SH_CALL(MapView_LinkPrimAt)(UL(s + 0x34), UL(s + 0x38), 1, 0xC);
    }
    S()[9] = static_cast<unsigned char>(S()[9] + 2);
    for (U k = 0, ring = 0; I(ring) < 0x100; ++k, ring += 0x10) {
        const unsigned char shade = static_cast<unsigned char>(0xFFu - ((S()[9] - ring) & 0xFF));
        const std::int32_t spin = S8(at::kSpin53 + (k & 1));
        for (U angle = 0; I(angle) < 0x1000; angle += 0x100) {
            unsigned char* const p = Gfx_PacketNext;
            SH_CALL(Gpu_SetTile1)(p);
            SH_CALL(Gpu_SetSemiTrans)(p, 1);
            p[4] = p[5] = p[6] = shade;
            const U sine = static_cast<U>(SH_CALL(Math_Sin)(I(((Frame_Counter & 0xF) << 4) * static_cast<U>(spin) + angle)));
            SetV(0, Sar(sine, 6) * 3 + Sar(UL(S() + 0x34), 9) - 0x4000u);
            const U cosine = static_cast<U>(SH_CALL(Math_Cos)(I(((Frame_Counter & 0xF) << 4) * static_cast<U>(spin) + angle)));
            {
                const unsigned char* const s = S();
                SetV(2, Sar(cosine, 6) * 3 + Sar(UL(s + 0x38), 9) - 0x4000u);
                const U lift = (static_cast<U>(s[9]) - (k << 4)) & 0xFF;
                SetV(4, (0u - DivP2(UL(s + 0x3C), 1)) - (lift << 1));
            }
            long a;
            SH_CALL(Gte_RotTransPers)(Vx(0), reinterpret_cast<unsigned long*>(p + 8), &a);
            SH_CALL(Gte_StoreDepthF)(Fp(p + 0x10));
            unsigned char* const s = S();
            SH_CALL(MapView_LinkPrimAt)(UL(s + 0x34), UL(s + 0x38), 1, 0x14);
        }
    }
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0xB5, 0);
    unsigned char* const s = S();
    SH_CALL(MapView_LinkPrimAt)(UL(s + 0x34), UL(s + 0x38), 1, 0xC);
}

// ===========================================================================
// Sub-kind 0x55: EffectKind18_States[0x55] (0x6541C0), EffectKind18Sub55_States
// (three: WeretigerFx_Next, then these two) by +2
// ===========================================================================

// original 0x512960 (called by sub-kind 0x55's two states): over the cells x
// 3..13, z 0x71..0x76 where x + z is even, a textured square: the cell's point
// ((x - 0x80) << 7, ((z - 0x80) << 6 - lift) << 1, (z - 0x77) << 4) projected
// into MapView_ScreenXY, half a side of |3 - (Frame_Counter / 3 + x + z & 7)|
// + 0x18; shaded (shade / 2, shade, shade / 2) (each << 6 or << 7, / 128
// toward zero); page 0x3B, CLUT 0x78CA; linked at (x << 16, z << 16) (dy 1,
// 0x48).
extern "C" void __cdecl EffectKind18Sub55_Draw(int shade_word, int lift_word) {
    const U shade = static_cast<U>(shade_word), lift = static_cast<U>(lift_word);
    for (U z = 0x71; I(z) < 0x77; ++z) {
        for (U x = 3; I(x) < 0xE; ++x) {
            if (((x + z) & 1) != 0) continue;
            SetV(0, (x - 0x80u) << 7);
            SetV(2, (((z - 0x80u) << 6) - lift) << 1);
            SetV(4, (z - 0x77u) << 4);
            long a;
            SH_CALL(Gte_RotTransPers)(Vx(0), reinterpret_cast<unsigned long*>(MapView_ScreenXY), &a);
            const U half = Abs(3u - (((Frame_Counter / 3) + x + z) & 7)) + 0x18;
            unsigned char* const p = Gfx_PacketNext;
            SH_CALL(Gpu_SetPolyFT4)(p);
            SH_CALL(Gpu_SetShadeTex)(p, 0);
            SH_CALL(Gte_PrimDepthFlat4_10)(p);
            const double h = Fi(half), side = Fi(half * 2);
            const double sx = D(AddressOf(MapView_ScreenXY)), sy = D(at::kScreenY);
            SetD(p + 8, sx - h);
            SetD(p + 0xC, sy - h);
            SetD(p + 0x18, (sx - h) + side);
            SetD(p + 0x1C, sy - h);
            SetD(p + 0x28, sx - h);
            SetD(p + 0x2C, (sy - h) + side);
            SetD(p + 0x38, (sx - h) + side);
            SetD(p + 0x3C, (sy - h) + side);
            SH_CALL(Gpu_SetSemiTrans)(p, 1);
            p[0x14] = 0xE0;
            p[0x15] = 0x30;
            const unsigned char outer = static_cast<unsigned char>(DivP2(shade << 6, 7));
            p[4] = outer;
            p[6] = outer;
            p[0x24] = 0xFF;
            p[0x25] = 0x30;
            p[0x34] = 0xE0;
            p[0x35] = 0x4F;
            p[0x44] = 0xFF;
            p[0x45] = 0x4F;
            SetWord(p + 0x26, 0x3B);
            SetWord(p + 0x16, 0x78CA);
            p[5] = static_cast<unsigned char>(DivP2(shade << 7, 7));
            SH_CALL(MapView_LinkPrimAt)(x << 16, z << 16, 1, 0x48);
        }
    }
}

// original 0x5128B0 (EffectKind18_States[0x55], hidden in 0x512040): jmp
// [EffectKind18Sub55_States + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub55_Run(void) {
    Dispatch("EffectKind18Sub55_Run", E6C_TABLE(EffectKind18Sub55_States));
}

// original 0x5128D0 (sub-state 1): once flag 0xE of 0x904000 is set, +9 0 and
// +2 up. The squares drawn (0x80, 0).
extern "C" void __cdecl EffectKind18Sub55_Wait(void) {
    if (SH_CALL(Flags_Test)(At(at::kFlagRow), 0xE) != 0) {
        S()[9] = 0;
        S()[2] = static_cast<unsigned char>(S()[2] + 1);
    }
    SH_CALL(EffectKind18Sub55_Draw)(0x80, 0);
}

// original 0x512910 (sub-state 2): +9 up; past 0xF the record released; the
// squares drawn ((0x10 - +9) * 8, +9 * 8), +9 read again after the release.
extern "C" void __cdecl EffectKind18Sub55_Fade(void) {
    S()[9] = static_cast<unsigned char>(S()[9] + 1);
    if (S()[9] > 0xF) SH_CALL(Effect_Release)();
    const U n = S()[9];
    SH_CALL(EffectKind18Sub55_Draw)(I((0x10u - n) << 3), I(n * 8));
}

// ===========================================================================
// Sub-kind 0x59: EffectKind18_States[0x59] (0x6541D0), EffectKind18Sub59_States
// (six) by +2
// ===========================================================================

// original 0x513410 (sub-kind 0x59's draws end with it, a tail jump from
// three states and a call from the fourth): a texture-page move (DR_MOVE) of
// the rect (0x1A8, 0x140 - (+0xA >> 1), 0x10, 0x20) to (0x198, 0x120),
// committed at slot 2 (0x18). The rect is a local the original builds on its
// stack.
extern "C" void __cdecl EffectKind18Sub59_Move(void) {
    unsigned char rect[8];
    SetWord(rect, 0x1A8);
    SetWord(rect + 2, 0x140u - (S()[0xA] >> 1));
    SetWord(rect + 4, 0x10);
    SetWord(rect + 6, 0x20);
    SH_CALL(Gpu_SetDrawMove)(Gfx_PacketNext, rect, 0x198, 0x120);
    SH_CALL(Gfx_CommitPrim)(2, 0x18);
}

// original 0x512D30 (called by sub-kind 0x59's six states): the record's point
// ((+0x34 >> 9) - 0x4000, (+0x38 >> 9) - 0x4000) into MapView_ScreenXY as
// floats; sixteen columns i (angle 0x1000 i, /16 for Math_Sin / Math_Cos) of
// eight rings k: two shaded quads (POLY_GT4, pages 0xD5 and 0xB5) between the
// radii kRadii59[7 - k] and [8 - k] (the products >> 8), lifted by kHeights59,
// shaded by the twelve .data bytes kShades59 (the fade's), projected once and
// the second given the first's screen points; each linked at the record (dy
// kRows59[i], 0x54). Then two glows a column: a point on the outer ring
// projected into 0x903838, a square of half side (|3 - (i / 2 + Frame_Counter
// & 7)| + 0x10) * 4500 / (Camera_Distance + 0x1194) (+ 8 the second), shaded
// (glow, glow, glow / 2), linked (dy kRows59[i] + 1, 0x48).
extern "C" void __cdecl EffectKind18Sub59_DrawRing(int glow_word) {
    const U glow = static_cast<U>(glow_word);
    {
        const unsigned char* const s = S();
        SetD(AddressOf(MapView_ScreenXY), Fi(Sar(UL(s + 0x34), 9) - 0x4000u));
        SetD(at::kScreenY, Fi(Sar(UL(s + 0x38), 9) - 0x4000u));
    }
    for (U i = 0, angle = 0; I(angle) < 0x10000; ++i, angle += 0x1000) {
        const U b0 = DivP2(angle, 4), b1 = DivP2(angle + 0x1000u, 4);
        const U row = *At(at::kRows59 + i);
        for (std::int32_t k = 0; k + 7 >= 0; --k) {
            const U ki = static_cast<U>(k);
            unsigned char* const p = Gfx_PacketNext;
            SH_CALL(Gpu_SetPolyGT4)(p);
            SH_CALL(Gpu_SetSemiTrans)(p, 1);
            struct Corner { unsigned at; U b; U radius; U height; };
            const Corner corners[] = {
                {0, b0, at::kRadii59 + 7 + ki, at::kHeights59 + 7 + ki},
                {8, b1, at::kRadii59 + 7 + ki, at::kHeights59 + 7 + ki},
                {0x10, b0, at::kRadii59 + 8 + ki, at::kHeights59 + 8 + ki},
                {0x18, b1, at::kRadii59 + 8 + ki, at::kHeights59 + 8 + ki},
            };
            for (const Corner& c : corners) {
                const U sine = static_cast<U>(SH_CALL(Math_Sin)(I(c.b)));
                SetV(c.at, Ftol(Fi(Sar(sine, 8) * *At(c.radius)) + D(AddressOf(MapView_ScreenXY))));
                const U cosine = static_cast<U>(SH_CALL(Math_Cos)(I(c.b)));
                SetV(c.at + 2, Ftol(Fi(Sar(cosine, 8) * *At(c.radius)) + D(at::kScreenY)));
                SetV(c.at + 4, 0u - (static_cast<U>(*At(c.height)) << 5));
            }
            long depth;
            SH_CALL(Gte_RotTransPers4)(Vx(0), Vx(1), Vx(2), Vx(3), Fp(p + 8), Fp(p + 0x1C), Fp(p + 0x30), Fp(p + 0x44), &depth);
            SH_CALL(Gte_PrimDepths4_14)(p);
            auto dress = [&](unsigned char* q, U page, U clut) {
                q[0x14] = 0xB0;
                q[0x15] = 0x20;
                q[0x28] = 0xCF;
                q[0x29] = 0x20;
                q[0x3C] = 0xB0;
                q[0x3D] = 0x3F;
                q[0x50] = 0xCF;
                q[0x51] = 0x3F;
                SetWord(q + 0x2A, page);
                SetWord(q + 0x16, clut);
                const unsigned char near = *At(at::kShades59 + 7 + ki), far = *At(at::kShades59 + 8 + ki);
                q[4] = q[5] = q[6] = near;
                q[0x18] = q[0x19] = q[0x1A] = near;
                q[0x2C] = q[0x2D] = q[0x2E] = far;
                q[0x40] = q[0x41] = q[0x42] = far;
            };
            dress(p, 0xD5, 0x7940);
            {
                unsigned char* const s = S();
                SH_CALL(MapView_LinkPrimAt)(UL(s + 0x34), UL(s + 0x38), I(row), 0x54);
            }
            unsigned char* const q = Gfx_PacketNext;
            SH_CALL(Gpu_SetPolyGT4)(q);
            SH_CALL(Gpu_SetShadeTex)(q, 0);
            SH_CALL(Gpu_SetSemiTrans)(q, 1);
            static const unsigned kPoints[] = {8, 0x1C, 0x30, 0x44};
            for (const unsigned point : kPoints) std::memmove(q + point, p + point, 12);
            dress(q, 0xB5, 0x7900);
            unsigned char* const s = S();
            SH_CALL(MapView_LinkPrimAt)(UL(s + 0x34), UL(s + 0x38), I(row), 0x54);
        }
        const U half = DivP2(i, 1);
        const unsigned char bright = static_cast<unsigned char>(DivP2(glow << 7, 7));
        const unsigned char blue = static_cast<unsigned char>(DivP2(glow << 6, 7));
        U e = b0;
        for (U back = 0; I(back) < 0x10; back += 8, e += 0x80) {
            const U sine = static_cast<U>(SH_CALL(Math_Sin)(I(e)));
            SetV(0x10, Ftol(Fi(Sar(sine, 8) * *At(at::kRadii59 + 8)) + D(AddressOf(MapView_ScreenXY))));
            const U cosine = static_cast<U>(SH_CALL(Math_Cos)(I(e)));
            SetV(0x12, Ftol(Fi(Sar(cosine, 8) * *At(at::kRadii59 + 8)) + D(at::kScreenY)));
            SetV(0x14, 0u - (static_cast<U>(*At(at::kHeights59 + 8)) << 5));
            long a;
            SH_CALL(Gte_RotTransPers)(Vx(2), reinterpret_cast<unsigned long*>(At(at::kGlowXY)), &a);
            const std::int32_t divisor = static_cast<std::int16_t>(Camera_Distance) + 0x1194;
            if (divisor == 0)
                bof3::Fatal("EffectKind18Sub59_DrawRing: Camera_Distance is -0x1194, the glow's divisor 0 - the original "
                            "divides by zero (docs/effect_6c.md section 7)");
            const U size = static_cast<U>(I((Abs(3u - ((half + Frame_Counter) & 7)) + 0x10) * 4500u) / divisor) + back;
            unsigned char* const p = Gfx_PacketNext;
            SH_CALL(Gpu_SetPolyFT4)(p);
            SH_CALL(Gpu_SetShadeTex)(p, 0);
            SH_CALL(Gte_PrimDepthFlat4_10)(p);
            const double h = Fi(size), side = Fi(size * 2);
            SetD(p + 8, D(at::kGlowXY) - h);
            SetD(p + 0xC, D(at::kGlowY) - h);
            SetD(p + 0x18, (D(at::kGlowXY) - h) + side);
            SetD(p + 0x1C, D(at::kGlowY) - h);
            SetD(p + 0x28, D(at::kGlowXY) - h);
            SetD(p + 0x2C, (D(at::kGlowY) - h) + side);
            SetD(p + 0x38, (D(at::kGlowXY) - h) + side);
            SetD(p + 0x3C, (D(at::kGlowY) - h) + side);
            SH_CALL(Gpu_SetSemiTrans)(p, 1);
            p[4] = bright;
            p[5] = bright;
            p[0x14] = 0xE0;
            p[0x15] = 0x30;
            p[0x24] = 0xFF;
            p[0x25] = 0x30;
            p[0x34] = 0xE0;
            p[0x35] = 0x4F;
            p[0x44] = 0xFF;
            p[0x45] = 0x4F;
            SetWord(p + 0x26, 0x3B);
            SetWord(p + 0x16, 0x78CA);
            p[6] = blue;
            unsigned char* const s = S();
            SH_CALL(MapView_LinkPrimAt)(UL(s + 0x34), UL(s + 0x38), I((row + 1) & 0xFF), 0x48);
        }
    }
}

// original 0x512B20 (EffectKind18_States[0x59], hidden in 0x512960; the
// catalog's Table EffectKind18_States row, in no group's list): jmp
// [EffectKind18Sub59_States + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub59_Run(void) {
    Dispatch("EffectKind18Sub59_Run", E6C_TABLE(EffectKind18Sub59_States));
}

namespace {
// +0xA up `by`, then & 0x3F (two reads of the record, as the original).
void Scroll59(unsigned by) {
    S()[0xA] = static_cast<unsigned char>(S()[0xA] + by);
    S()[0xA] &= 0x3F;
}
}  // namespace

// original 0x512B40 (sub-state 0): +9 0, +0xA 0, +2 up; the ring drawn dark;
// the texture move.
extern "C" void __cdecl EffectKind18Sub59_Start(void) {
    S()[9] = 0;
    S()[0xA] = 0;
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
    SH_CALL(EffectKind18Sub59_DrawRing)(0);
    SH_CALL(EffectKind18Sub59_Move)();
}

// original 0x512B70 (sub-state 1): once Cond_ByteFE is 1, +2 up; the ring
// drawn dark (no move).
extern "C" void __cdecl EffectKind18Sub59_Wait(void) {
    if (Cond_ByteFE == 1) S()[2] = static_cast<unsigned char>(S()[2] + 1);
    SH_CALL(EffectKind18Sub59_DrawRing)(0);
}

// original 0x512B90 (sub-state 2): once Cond_ByteFE is 2, +2 up; +9 up to 0x20;
// the ring drawn with the glow 0x20 - +9; +0xA up 1; the move.
extern "C" void __cdecl EffectKind18Sub59_Rise(void) {
    if (Cond_ByteFE == 2) S()[2] = static_cast<unsigned char>(S()[2] + 1);
    if (S()[9] < 0x20) S()[9] = static_cast<unsigned char>(S()[9] + 1);
    SH_CALL(EffectKind18Sub59_DrawRing)(I(0x20u - S()[9]));
    Scroll59(1);
    SH_CALL(EffectKind18Sub59_Move)();
}

// original 0x512BF0 (sub-state 3): once Cond_ByteFE is 3, +9 0 and +2 up; the
// ring dark; +0xA up 2; the move.
extern "C" void __cdecl EffectKind18Sub59_Hold(void) {
    if (Cond_ByteFE == 3) {
        S()[9] = 0;
        S()[2] = static_cast<unsigned char>(S()[2] + 1);
    }
    SH_CALL(EffectKind18Sub59_DrawRing)(0);
    Scroll59(2);
    SH_CALL(EffectKind18Sub59_Move)();
}

// original 0x512C30 (sub-state 4): +9 up; the fade of kShades59 (.data, never
// restored): the first of its first nine bytes that is not 0 down 2, and while
// each so stepped is at most 0x60 the next down 2 too (four at most); the ring
// with the glow min(+9, 0x60); +0xA up 4; the move; once the ninth byte
// (0x65EF14) is 0, +9 0x60 and +2 up.
extern "C" void __cdecl EffectKind18Sub59_Fade(void) {
    S()[9] = static_cast<unsigned char>(S()[9] + 1);
    unsigned char* const shades = At(at::kShades59);
    for (unsigned e = 0; e < 9; ++e) {
        if (shades[e] == 0) continue;
        for (unsigned n = 0; n < 4; ++n) {
            shades[e + n] = static_cast<unsigned char>(shades[e + n] - 2);
            if (n == 3 || shades[e + n] > 0x60) break;
        }
        break;
    }
    const U glow = S()[9] > 0x60 ? 0x60u : S()[9];
    SH_CALL(EffectKind18Sub59_DrawRing)(I(glow));
    Scroll59(4);
    SH_CALL(EffectKind18Sub59_Move)();
    if (*At(at::kShades59 + 8) != 0) return;
    S()[9] = 0x60;
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
}

// original 0x512CF0 (sub-state 5): +9 down 4; at 0 the record released; the
// ring with the glow +9 (read again).
extern "C" void __cdecl EffectKind18Sub59_Close(void) {
    S()[9] = static_cast<unsigned char>(S()[9] - 4);
    if (S()[9] == 0) SH_CALL(Effect_Release)();
    SH_CALL(EffectKind18Sub59_DrawRing)(I(static_cast<U>(S()[9])));
}

// ===========================================================================
// Sub-kind 0x66: EffectKind18_States[0x66] (0x654204) - one state
// ===========================================================================

// original 0x513470 (hidden in 0x513410): nothing while Cond_ByteFE is 0.
// Else two layers, each a draw mode with a texture window (0, 0, 0x40, 0x40;
// page 0x97, dtd) at slot 4, a 2 x 2 grid of 256-pixel textured quads (page
// 0x7B, CLUT 0x78C0) at (-d - 0x80, d - 0x80), and a draw mode with the window
// (0, 0, 0x100, 0x100): layer 0 offset d = -(+9 & 0x3F), shaded (0x80 - |0x80
// - +9|) / 2; layer 1 d = -((+9 & 0x1F) * 2), shaded the same unhalved past
// 0x40 of 0x80, else ((Math_Cos((+9 - 0x40) << 5) >> 7) + 0x60) / 2. +9 up 4;
// at 0 the record released.
extern "C" void __cdecl EffectKind18Sub66_Run(void) {
    if (Cond_ByteFE == 0) return;
    const U n = S()[9];
    const U distance = Abs(0x80u - n);
    const U edge = 0x80u - distance;
    U shades[2];
    shades[0] = Sar(edge, 1);
    if (I(distance) > 0x40) {
        shades[1] = edge;
    } else {
        const U cosine = static_cast<U>(SH_CALL(Math_Cos)(I((n - 0x40u) << 5)));
        shades[1] = Sar(Sar(cosine, 7) + 0x60u, 1);
    }
    const U m = S()[9];
    const U offsets[2] = {0u - (m & 0x3F), 0u - ((m & 0x1F) << 1)};
    for (unsigned layer = 0; layer < 2; ++layer) {
        const U d = offsets[layer];
        const unsigned char shade = static_cast<unsigned char>(shades[layer]);
        unsigned char* const window = Rect(0, 0, 0x40, 0x40);
        SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x97, AddressOf(window));
        SH_CALL(Gfx_CommitPrim)(4, 0xC);
        const U xs[3] = {0u - d - 0x80u, 0u - d + 0x80u, 0u - d + 0x180u};
        const U ys[3] = {d - 0x80u, d + 0x80u, d + 0x180u};
        for (unsigned tile = 0; tile < 4; ++tile) {
            const unsigned cx = tile & 1, cy = tile >> 1;
            unsigned char* const p = Gfx_PacketNext;
            SH_CALL(Gpu_SetPolyFT4)(p);
            SH_CALL(Gpu_SetShadeTex)(p, 0);
            SH_CALL(Gpu_SetSemiTrans)(p, 1);
            SetD(p + 8, Fi(xs[cx]));
            SetD(p + 0xC, Fi(ys[cy]));
            SetD(p + 0x18, Fi(xs[cx + 1]));
            SetD(p + 0x1C, Fi(ys[cy]));
            SetD(p + 0x28, Fi(xs[cx]));
            SetD(p + 0x2C, Fi(ys[cy + 1]));
            SetD(p + 0x38, Fi(xs[cx + 1]));
            SetD(p + 0x3C, Fi(ys[cy + 1]));
            p[0x14] = 0;
            p[0x15] = 0;
            p[0x24] = 0xFF;
            p[0x25] = 0;
            p[0x34] = 0;
            p[0x35] = 0xFF;
            p[0x44] = 0xFF;
            p[0x45] = 0xFF;
            p[4] = p[5] = p[6] = shade;
            SetWord(p + 0x26, 0x7B);
            SetWord(p + 0x16, 0x78C0);
            SH_CALL(Gfx_CommitPrim)(4, 0x48);
        }
        unsigned char* const whole = Rect(0, 0, 0x100, 0x100);
        SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x97, AddressOf(whole));
        SH_CALL(Gfx_CommitPrim)(4, 0xC);
    }
    S()[9] = static_cast<unsigned char>(S()[9] + 4);
    if (S()[9] == 0) SH_CALL(Effect_Release)();
}

// ===========================================================================
// Sub-kind 0x5A: EffectKind18_States[0x5A] (0x6541D4), EffectKind18Sub5A_States
// (six) by +2
// ===========================================================================

// original 0x513C70 (called by sub-kind 0x5A's three moving states): a
// texture-page move of the rect (0x240, x + 0x100, 0x58, 0x30) to (0x240, y +
// 0x100), committed at slot 2 (0x18). The rect is a local the original builds
// on its stack.
extern "C" void __cdecl EffectKind18Sub5A_Move(int x_word, int y_word) {
    unsigned char rect[8];
    SetWord(rect, 0x240);
    SetWord(rect + 2, static_cast<U>(x_word) + 0x100u);
    SetWord(rect + 4, 0x58);
    SetWord(rect + 6, 0x30);
    SH_CALL(Gpu_SetDrawMove)(Gfx_PacketNext, rect, 0x240, static_cast<U>(y_word) + 0x100u);
    SH_CALL(Gfx_CommitPrim)(2, 0x18);
}

// original 0x513AD0 (called by EffectKind18Sub5A_Animate): map row z = step +
// 0x47, the bytes of two six-byte stack tables picked by step % 3 + 3 * bank:
// the second record of cell (0xD, z) gets the first's byte (5, 4, 3 / 0xB, 0xA,
// 9) and, where MapView_ItemAt answers an item, Prim_SetTexture(it, the item
// its link word names, 2) - DIV-0062's sites 0x513B9B / 0x513BAC; then the
// first record of cells (0xE..0x15, z) gets (x - 0xB) | the second's byte (0x30,
// 0x40, 0x50 / 0xC0, 0xD0, 0xE0) and its item textured (2) - the site 0x513C41.
extern "C" void __cdecl EffectKind18Sub5A_SetTiles(int step_word, int bank_word) {
    static const unsigned char kFirst[6] = {5, 4, 3, 0xB, 0xA, 9};
    static const unsigned char kSecond[6] = {0x30, 0x40, 0x50, 0xC0, 0xD0, 0xE0};
    const U step = static_cast<U>(step_word), bank = static_cast<U>(bank_word);
    const U z = step + 0x47;
    const U pick = static_cast<U>(I(step) % 3) + bank * 3;
    if (pick >= 6)
        bof3::Fatal("EffectKind18Sub5A_SetTiles: (step %d, bank %d) picks %d, past the six bytes of its stack tables - "
                    "the original reads its own frame (docs/effect_6c.md section 7)",
                    I(step), I(bank), I(pick));
    {
        unsigned char* const record = CellRecord(0xD, z, 1);
        SetUL(record, (UL(record) & 0xFFFFFF00u) | kFirst[pick]);
        const U item = static_cast<U>(SH_CALL(MapView_ItemAt)(0xD, I(z)));
        if (item != 0) {
            const U link = Word(Item("EffectKind18Sub5A_SetTiles", item) + at::kItemLink) & 0xFFF;
            SH_CALL(Prim_SetTexture)(UL(record), Item("EffectKind18Sub5A_SetTiles", link), 2);
        }
    }
    const U mark = kSecond[pick];
    for (U e = 0; I(e) < 8; ++e) {
        unsigned char* const record = CellRecord(e + 0xE, z, 0);
        SetUL(record, (UL(record) & 0xFFFFFF00u) | (e + 3) | mark);
        const U item = static_cast<U>(SH_CALL(MapView_ItemAt)(I(e + 0xE), I(z)));
        if (item != 0) SH_CALL(Prim_SetTexture)(UL(record), Item("EffectKind18Sub5A_SetTiles", item), 2);
    }
}

// original 0x513860 (EffectKind18_States[0x5A], hidden in 0x513410): jmp
// [EffectKind18Sub5A_States + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub5A_Run(void) {
    Dispatch("EffectKind18Sub5A_Run", E6C_TABLE(EffectKind18Sub5A_States));
}

// original 0x513880 (sub-state 0): +9 0; story flag 0x80 set: +2 = 5 (the
// second cycle); else +2 up.
extern "C" void __cdecl EffectKind18Sub5A_Start(void) {
    S()[9] = 0;
    if (SH_CALL(Flags_Test)(At(at::kStoryFlags), 0x80) != 0)
        S()[2] = 5;
    else
        S()[2] = static_cast<unsigned char>(S()[2] + 1);
}

// original 0x5138C0 (sub-state 1): once Cond_ByteFE is 2, +2 up.
extern "C" void __cdecl EffectKind18Sub5A_Wait(void) {
    if (Cond_ByteFE == 2) S()[2] = static_cast<unsigned char>(S()[2] + 1);
}

// original 0x5138E0 (sub-state 2): the map rows swept by +9
// (EffectKind18Sub5A_SetTiles(+9 >> 3, +9 >> 2 & 1)); +9 up; at 0xD0 +2 up.
extern "C" void __cdecl EffectKind18Sub5A_Animate(void) {
    const U n = S()[9];
    SH_CALL(EffectKind18Sub5A_SetTiles)(I(n >> 3), I((n >> 2) & 1));
    S()[9] = static_cast<unsigned char>(S()[9] + 1);
    if (S()[9] >= 0xD0) S()[2] = static_cast<unsigned char>(S()[2] + 1);
}

// original 0x513920 (sub-state 3): story flag 0x80 set; +9 0; the move (0xC0,
// 0); the first records of cells (0x11, 0x61) and (0x12, 0x61) given 0xE6 and
// 0xE7 and their items textured (2) - DIV-0062's site 0x5139C2; +2 up.
extern "C" void __cdecl EffectKind18Sub5A_Set(void) {
    SH_CALL(Flags_Set)(At(at::kStoryFlags), 0x80);
    S()[9] = 0;
    SH_CALL(EffectKind18Sub5A_Move)(0xC0, 0);
    for (U e = 0; I(e) < 2; ++e) {
        unsigned char* const record = CellRecord(e + 0x11, 0x61, 0);
        SetUL(record, (UL(record) & 0xFFFFFF00u) | (e + 0xE6));
        const U item = static_cast<U>(SH_CALL(MapView_ItemAt)(I(e + 0x11), 0x61));
        if (item != 0) SH_CALL(Prim_SetTexture)(UL(record), Item("EffectKind18Sub5A_Set", item), 2);
    }
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
}

namespace {
// Sub-kind 0x5A's cycles: +9 up; every sixth frame +9 wraps to 0 at 0x18 and
// the move (rows[+9 / 6], y).
void Cycle5A(U rows, int y) {
    S()[9] = static_cast<unsigned char>(S()[9] + 1);
    unsigned char* const s = S();
    const U n = s[9];
    if (n % 6 != 0) return;
    s[9] = static_cast<unsigned char>(n < 0x18 ? n : 0);
    SH_CALL(EffectKind18Sub5A_Move)(*At(rows + S()[9] / 6), y);
}
}  // namespace

// original 0x5139F0 (sub-state 4): the cycle through kSlide5A, y 0xC0.
extern "C" void __cdecl EffectKind18Sub5A_Cycle(void) { Cycle5A(at::kSlide5A, 0xC0); }

// original 0x513A60 (sub-state 5): the cycle through kSlide5B, y 0.
extern "C" void __cdecl EffectKind18Sub5A_CycleOpen(void) { Cycle5A(at::kSlide5B, 0); }

// ===========================================================================
// Sub-kind 0x5B: EffectKind18_States[0x5B] (0x6541D8), EffectKind18Sub5B_States
// (eight) by +2
// ===========================================================================

// original 0x5140C0 (called three times by each of sub-kind 0x5B's drawing
// states): the pieces kParts5B[part] (first, end) of kPieces5B: each a POLY_FT4
// at its cell (bytes 0, 1: ((c << 7) - 0x4040) into MapView_ScreenXY as
// floats), its four corners (bytes 2..13: x << 6 + the offset 0x903828 + the
// screen x, z << 6 + the screen y, (6 - y) << 6 + the lift 0x90382C, each
// truncated), projected, textured by its dword +0x10 (1), linked at ((x << 16)
// + 0x903828 * kPieceScale, z << 16) (dy 0, 0x48).
extern "C" void __cdecl EffectKind18Sub5B_DrawPart(int part_word) {
    const U part = static_cast<U>(part_word);
    if (part >= at::kParts5BCount)
        bof3::Fatal("EffectKind18Sub5B_DrawPart: part %u, past the %u ranges of 0x%X - the original reads on "
                    "(docs/effect_6c.md section 7)",
                    (unsigned)part, at::kParts5BCount, (unsigned)at::kParts5B);
    for (U j = *At(at::kParts5B + 2 * part); I(j) < I(*At(at::kParts5B + 2 * part + 1)); ++j) {
        const unsigned char* const r = At(at::kPieces5B + at::kPieceStride * j);
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyFT4)(p);
        SH_CALL(Gpu_SetShadeTex)(p, 0);
        SetD(AddressOf(MapView_ScreenXY), Fi((static_cast<U>(r[0]) << 7) - 0x4040u));
        SetD(at::kScreenY, Fi((static_cast<U>(r[1]) << 7) - 0x4040u));
        for (unsigned v = 0; v < 4; ++v) {
            const unsigned char* const c = r + 2 + 3 * v;
            SetV(8 * v, Ftol((Fi(static_cast<U>(c[0]) << 6) + D(at::kOffsetX)) + D(AddressOf(MapView_ScreenXY))));
            SetV(8 * v + 2, Ftol(Fi(static_cast<U>(c[1]) << 6) + D(at::kScreenY)));
            SetV(8 * v + 4, Ftol(Fi((6u - c[2]) << 6) + D(at::kOffsetY)));
        }
        long depth;
        SH_CALL(Gte_RotTransPers4)(Vx(0), Vx(1), Vx(2), Vx(3), Fp(p + 8), Fp(p + 0x18), Fp(p + 0x28), Fp(p + 0x38), &depth);
        SH_CALL(Gte_PrimDepths4_10)(p);
        SH_CALL(Prim_SetTexture)(UL(r + 0x10), p, 1);
        const U x = Ftol(Fi(static_cast<U>(r[0]) << 16) + D(at::kOffsetX) * D(at::kPieceScale));
        SH_CALL(MapView_LinkPrimAt)(x, static_cast<U>(r[1]) << 16, 0, 0x48);
    }
}

namespace {
// The three parts, the offsets set before each as the states set them.
void SetOffset(U x_bits, U y_bits) {
    SetUL(at::kOffsetX, x_bits);
    SetUL(at::kOffsetY, y_bits);
}
void Part(int part) { SH_CALL(EffectKind18Sub5B_DrawPart)(part); }
// float(-(+9 << shift)) / float(+9 << shift) as fild / fstp store them.
void OffsetX(bool negative, unsigned shift) {
    const U v = static_cast<U>(S()[9]) << shift;
    SetD(at::kOffsetX, Fi(negative ? 0u - v : v));
}
}  // namespace

// original 0x513CD0 (EffectKind18_States[0x5B], hidden in 0x513C70; the
// catalog's Table EffectKind18_States row, in no group's list): jmp
// [EffectKind18Sub5B_States + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub5B_Run(void) {
    Dispatch("EffectKind18Sub5B_Run", E6C_TABLE(EffectKind18Sub5B_States));
}

// original 0x513CF0 (sub-state 0): story flag 0x81 set: +2 = 6 (opened
// already); else Cond_ByteFE 3 and +2 up.
extern "C" void __cdecl EffectKind18Sub5B_Start(void) {
    if (SH_CALL(Flags_Test)(At(at::kStoryFlags), 0x81) != 0) {
        S()[2] = 6;
        return;
    }
    unsigned char* const s = S();
    Cond_ByteFE = 3;
    s[2] = static_cast<unsigned char>(s[2] + 1);
}

// original 0x513D20 (sub-state 1): once Cond_ByteFE is 2: the offsets 0, the
// three parts drawn, sound 0x205, +2 up.
extern "C" void __cdecl EffectKind18Sub5B_Wait(void) {
    if (Cond_ByteFE != 2) return;
    SetOffset(0, 0);
    Part(0);
    Part(1);
    Part(2);
    SH_CALL(Sound_PlayEffect)(static_cast<unsigned short>(at::kSoundOpen5B));
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
}

namespace {
// Sub-states 2 and 4: the lift 0; part 0 at -(+9 << 3), part 1 at +9 << 3 (read
// again), part 2 at 0; +9 up; past `end` +9 0 and +2 up.
void Slide5B(unsigned end) {
    SetUL(at::kOffsetY, 0);
    OffsetX(true, 3);
    Part(0);
    OffsetX(false, 3);
    Part(1);
    SetUL(at::kOffsetX, 0);
    Part(2);
    S()[9] = static_cast<unsigned char>(S()[9] + 1);
    if (S()[9] <= end) return;
    S()[9] = 0;
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
}
}  // namespace

// original 0x513D70 (sub-state 2): the slide, +9 past 8 ends it.
extern "C" void __cdecl EffectKind18Sub5B_SlideOut(void) { Slide5B(8); }

// original 0x513E00 (sub-state 3): parts 0 and 1 at -64 / 64, 2 at 0, the lift
// 0; +9 up; past 0x1E sound 0x206, +9 8, +2 up.
extern "C" void __cdecl EffectKind18Sub5B_Hold(void) {
    SetOffset(0xC2800000u, 0);   // -64.0f
    Part(0);
    SetUL(at::kOffsetX, 0x42800000u);   // 64.0f
    Part(1);
    SetUL(at::kOffsetX, 0);
    Part(2);
    S()[9] = static_cast<unsigned char>(S()[9] + 1);
    if (S()[9] <= 0x1E) return;
    SH_CALL(Sound_PlayEffect)(static_cast<unsigned short>(at::kSoundStep5B));
    S()[9] = 8;
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
}

// original 0x513E80 (sub-state 4): the slide, +9 past 0x30 ends it.
extern "C" void __cdecl EffectKind18Sub5B_SlideWide(void) { Slide5B(0x30); }

// original 0x513F10 (sub-state 5): parts 0 and 1 at -384 / 384, part 2 at 0
// lifted +9 << 3; +9 up; past 0x30 story flag 0x81 set, MoveCmd_TestFB(0x30,
// 0x71), the record released.
extern "C" void __cdecl EffectKind18Sub5B_Lift(void) {
    SetOffset(0xC3C00000u, 0);   // -384.0f
    Part(0);
    SetUL(at::kOffsetX, 0x43C00000u);   // 384.0f
    Part(1);
    SetUL(at::kOffsetX, 0);
    SetD(at::kOffsetY, Fi(static_cast<U>(S()[9]) << 3));
    Part(2);
    S()[9] = static_cast<unsigned char>(S()[9] + 1);
    if (S()[9] <= 0x30) return;
    SH_CALL(Flags_Set)(At(at::kStoryFlags), 0x81);
    SH_CALL(MoveCmd_TestFB)(0x30, 0x71);
    SH_CALL(Effect_Release)();
}

// original 0x513FB0 (sub-state 6): once Cond_ByteFE is 2: story flag 0x81
// cleared, sound 0x206, the parts at -384 / 384 / 0 lifted 384, +9 0x60, +2 up.
extern "C" void __cdecl EffectKind18Sub5B_Reopen(void) {
    if (Cond_ByteFE != 2) return;
    SH_CALL(Flags_Clear)(At(at::kStoryFlags), 0x81);
    SH_CALL(Sound_PlayEffect)(static_cast<unsigned short>(at::kSoundStep5B));
    SetOffset(0xC3C00000u, 0);   // -384.0f
    Part(0);
    SetUL(at::kOffsetX, 0x43C00000u);   // 384.0f
    Part(1);
    SetOffset(0, 0x43C00000u);
    Part(2);
    S()[9] = 0x60;
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
}

// original 0x514030 (sub-state 7): part 0 at -(+9 << 2), part 1 at +9 << 2,
// part 2 at 0 lifted 384; +9 down to 0.
extern "C" void __cdecl EffectKind18Sub5B_Close(void) {
    SetUL(at::kOffsetY, 0);
    OffsetX(true, 2);
    Part(0);
    OffsetX(false, 2);
    Part(1);
    SetOffset(0, 0x43C00000u);
    Part(2);
    unsigned char* const s = S();
    if (s[9] != 0) s[9] = static_cast<unsigned char>(s[9] - 1);
}

void Effect6C_Inject() {
    if (bof3::WantsShadow("effect_6c")) effect_6c::SelfTest();
    BOF3_INJECT(EffectKind18Sub44_Follow);
    BOF3_INJECT(EffectKind18Sub44_Draw);
    BOF3_INJECT(EffectKind18Sub44_DrawGlow);
    BOF3_INJECT(EffectKind18Sub44_DrawStars);
    BOF3_INJECT(EffectKind18Sub44_Commit);
    BOF3_INJECT(AreaMap_CornerHeight);
    BOF3_INJECT(EffectKind18Sub44_DrawTwinkle);
    BOF3_INJECT(EffectKind18Sub45_Run);
    BOF3_INJECT(EffectKind18Sub45_Wait);
    BOF3_INJECT(EffectKind18Sub45_Open);
    BOF3_INJECT(EffectKind18Sub45_Spread);
    BOF3_INJECT(EffectKind18Sub45_DrawRing);
    BOF3_INJECT(EffectKind18Sub51_Run);
    BOF3_INJECT(EffectKind18Sub51_Place);
    BOF3_INJECT(EffectKind18Sub51_Wait);
    BOF3_INJECT(EffectKind18Sub51_Fade);
    BOF3_INJECT(EffectKind18Sub53_Run);
    BOF3_INJECT(EffectKind18Sub55_Run);
    BOF3_INJECT(EffectKind18Sub55_Wait);
    BOF3_INJECT(EffectKind18Sub55_Fade);
    BOF3_INJECT(EffectKind18Sub55_Draw);
    BOF3_INJECT(EffectKind18Sub59_Run);
    BOF3_INJECT(EffectKind18Sub59_Start);
    BOF3_INJECT(EffectKind18Sub59_Wait);
    BOF3_INJECT(EffectKind18Sub59_Rise);
    BOF3_INJECT(EffectKind18Sub59_Hold);
    BOF3_INJECT(EffectKind18Sub59_Fade);
    BOF3_INJECT(EffectKind18Sub59_Close);
    BOF3_INJECT(EffectKind18Sub59_DrawRing);
    BOF3_INJECT(EffectKind18Sub59_Move);
    BOF3_INJECT(EffectKind18Sub66_Run);
    BOF3_INJECT(EffectKind18Sub5A_Run);
    BOF3_INJECT(EffectKind18Sub5A_Start);
    BOF3_INJECT(EffectKind18Sub5A_Wait);
    BOF3_INJECT(EffectKind18Sub5A_Animate);
    BOF3_INJECT(EffectKind18Sub5A_Set);
    BOF3_INJECT(EffectKind18Sub5A_Cycle);
    BOF3_INJECT(EffectKind18Sub5A_CycleOpen);
    BOF3_INJECT(EffectKind18Sub5A_SetTiles);
    BOF3_INJECT(EffectKind18Sub5A_Move);
    BOF3_INJECT(EffectKind18Sub5B_Run);
    BOF3_INJECT(EffectKind18Sub5B_Start);
    BOF3_INJECT(EffectKind18Sub5B_Wait);
    BOF3_INJECT(EffectKind18Sub5B_SlideOut);
    BOF3_INJECT(EffectKind18Sub5B_Hold);
    BOF3_INJECT(EffectKind18Sub5B_SlideWide);
    BOF3_INJECT(EffectKind18Sub5B_Lift);
    BOF3_INJECT(EffectKind18Sub5B_Reopen);
    BOF3_INJECT(EffectKind18Sub5B_Close);
    BOF3_INJECT(EffectKind18Sub5B_DrawPart);
}

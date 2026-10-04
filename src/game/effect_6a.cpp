// Round thirteen group E6A (docs/effect_6a.md): the 48 functions of
// analysis/round13_cut.tsv's group E6A, 0x50C0D0..0x50E3FA, each read with
// capstone to its last instruction (2026-10-03). Effect_RunObjects (ours)
// makes each live record of Effect_Objects (20 of 0x80 bytes) Sprite_Current
// and calls Effect_KindHandlers[+5]; kind 0x18's EffectKind18_Run jumps through
// EffectKind18_States by +1 (the sub-kind, EffectKind18_Start copies it from
// +0xB). Seven sub-kinds of the band dispatch again by +2 through a table of
// their own (none bounded by a compare), five states each, the same shape as
// E5G's sub-kind 0x2C: place (0), wait for the leader at the cell's front (1),
// slide open (2), wait for the leader two cells off (3), slide shut (4).
//
//   sub-kind 0x2D   one textured quad at a variant's cell and height (+0x3E),
//                   lying across x or z (bit 0 of the spawn's z cell chooses),
//                   slid by a sign bit 1 of the z cell chooses; flat, linked
//                   into the map's depth order at the record (dy -2)
//   sub-kind 0x3E   the same states with the front one cell further back
//                   (the centre at cell - 2) and nothing drawn
//   sub-kinds 0x2E, 0x30, 0x31   one quad on the ground (four AreaMap_Elevation
//                   reads), slid toward -0x100; three draws that differ only
//                   in the texture word
//   sub-kinds 0x2F, 0x32   the same slid toward +0x100, sharing a draw
//                   (EffectKind18Sub2F_Draw, which E6B's sub-kinds call too)
//   sub-kind 0x4A   its state 1 (E5G's table): once a bit of +0xB is set in
//                   Cond_ByteFE (and Field_Request is not 2) the map byte 0xA1
//                   at the cell and the next along z, sound 0x200, +2 up; the
//                   draw is E5G's EffectKind18Sub2C_Draw
//
// Every call goes through the harness (SH_CALL), so the start-up fuzz can stand
// recorders in for ours as for the originals' copies. Sprite_Current is read
// again wherever the original reads [0x937F88] again after a call. No
// divergence: each is a faithful replacement. Where the original jumps
// through a sub-state table past its end or indexes a variant or sign table
// past its room, ours aborts with a message (docs/effect_6a.md section 7).
#include "game/effect_6a.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/effect_6a_callees.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_6a::at;
using U = std::uint32_t;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using Handler = scenario_harness::Handler;

unsigned char* S() { return Sprite_Current; }
U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }
std::int32_t S16(const unsigned char* p) { return static_cast<std::int16_t>(Word(p)); }
U AddressOf(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
unsigned char* Vertices() { return reinterpret_cast<unsigned char*>(Prim_VertexScratch); }
const short* Vertex(unsigned i) { return reinterpret_cast<const short*>(Vertices() + 8 * i); }

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 2]; jmp [table + eax
// * 4]: the table's `entries` handlers read in place (the fuzz swaps the cells
// for recorders); a Fatal past them, where the original jumps through the dword
// after - data or the next table.
void Dispatch(const char* who, U table, unsigned entries) {
    const unsigned sub = Sprite_Current[2];
    if (sub >= entries)
        bof3::Fatal("%s: sub-state byte +2 is %u, past the %u entries of 0x%X - the original jumps through the dword "
                    "after (docs/effect_6a.md section 7)",
                    who, sub, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(At(table + 4 * sub))))();
}

// The variant the spawn's x cell (+0x36, movsx) names, checked against the
// room its tables have.
U Variant(const char* who, const unsigned char* s, unsigned room) {
    const std::int32_t v = S16(s + 0x36);
    if (v < 0 || v >= static_cast<std::int32_t>(room))
        bof3::Fatal("%s: the variant +0x36 is %d, past the %u its tables have room for - the original reads on into "
                    "the next table (docs/effect_6a.md section 7)",
                    who, (int)v, room);
    return static_cast<U>(v);
}

// cdq; xor eax, edx; sub eax, edx; cmp eax, n; jg: |d| as the original
// computes it - 0x80000000 stays itself, negative, and so counts as near.
std::int32_t Abs(U d) {
    const U m = static_cast<std::int32_t>(d) < 0 ? 0xFFFFFFFFu : 0u;
    return static_cast<std::int32_t>((d ^ m) - m);
}
// A point against a cell's centre on one axis: point - (cell << 16 | 0x8000)
// (the caller's cell already moved by the sub-kind's delta: inc, or sub 2).
U FromCentre(U point, std::int32_t cell) { return point - ((static_cast<U>(cell) << 16) | 0x8000u); }
// ... and against its edge: point - (cell << 16).
U FromEdge(U point, std::int32_t cell) { return point - (static_cast<U>(cell) << 16); }
// |a| within `first`, then |b| or |b - 0x10000| within `second` (the cell or
// the one past it).
bool Within(U a, std::int32_t first, U b, std::int32_t second) {
    if (Abs(a) > first) return false;
    if (Abs(b) <= second) return true;
    return Abs(b - 0x10000u) <= second;
}
U LeaderX() { return UL(At(at::kLeaderX)); }
U LeaderZ() { return UL(At(at::kLeaderZ)); }
// The leader at the record's cell: with +8 set across z first (the centre of
// cell z + delta), then x against the edge; without, x first, then z.
bool LeaderAt(const unsigned char* s, std::int32_t delta, std::int32_t first, std::int32_t second) {
    const std::int32_t cx = S16(s + 0x36), cz = S16(s + 0x3A);
    if (s[8] != 0) return Within(FromCentre(LeaderZ(), cz + delta), first, FromEdge(LeaderX(), cx), second);
    return Within(FromCentre(LeaderX(), cx + delta), first, FromEdge(LeaderZ(), cz), second);
}

// The front's delta: the centre at cell + 1 (every sub-kind but 0x3E), cell -
// 2 (0x3E).
constexpr std::int32_t kFront = 1, kFront3E = -2;

// --- the states' bodies --------------------------------------------------------------

// Sub-state 0 of 0x3E and 0x2E..0x32: +8 = whether the spawn's z cell is 0,
// the variant's cell into +0x36 / +0x3A, the slide 0, +2 up; the leader
// already at the front: the slide `open` and +2 = 3. Answers whether it was.
bool PlaceCells(const char* who, U cells, unsigned room, std::int32_t delta, U open) {
    unsigned char* const s = S();
    s[8] = Word(s + 0x3A) == 0 ? 1 : 0;
    const U v = Variant(who, s, room);
    SetWord(s + 0x36, At(cells + 2 * v)[0]);
    SetWord(s + 0x3A, At(cells + 2 * v + 1)[0]);
    SetWord(s + 0x30, 0);
    s[2] = static_cast<unsigned char>(s[2] + 1);
    if (!LeaderAt(s, delta, 0x8000, 0x10000)) return false;
    SetWord(s + 0x30, open);
    s[2] = 3;
    return true;
}
// Sub-state 1: the leader at the front: sound 0x200 unless a message is up,
// +2 up (Sprite_Current read after the sound).
void WaitNear(std::int32_t delta) {
    unsigned char* s = S();
    if (!LeaderAt(s, delta, 0x8000, 0x10000)) return;
    if (Field_Request == 0) {
        SH_CALL(Sound_PlayEffect)(at::kSoundOpen);
        s = S();
    }
    s[2] = static_cast<unsigned char>(s[2] + 1);
}
// Sub-state 2: the slide 0x20 toward +0x100 (`up`) or -0x100; there (signed
// words), +2 up.
void Open(bool up) {
    unsigned char* const s = S();
    SetWord(s + 0x30, Word(s + 0x30) + (up ? 0x20u : 0xFFE0u));
    const std::int32_t slide = S16(S() + 0x30);
    if (up ? slide >= 0x100 : slide <= -0x100) S()[2] = static_cast<unsigned char>(S()[2] + 1);
}
// Sub-state 3: once the leader is two cells off (the front test at 0x20000
// both ways fails), +2 up.
void WaitFar(std::int32_t delta) {
    unsigned char* const s = S();
    if (!LeaderAt(s, delta, 0x20000, 0x20000)) s[2] = static_cast<unsigned char>(s[2] + 1);
}
// Sub-state 4: the slide 0x20 back toward 0; at 0 or past it, sound 0x201
// unless a message is up, +2 = 1 (read after the sound).
void Close(bool up) {
    unsigned char* const s = S();
    SetWord(s + 0x30, Word(s + 0x30) + (up ? 0xFFE0u : 0x20u));
    const std::int32_t slide = S16(S() + 0x30);
    if (up ? slide > 0 : slide < 0) return;
    if (Field_Request == 0) SH_CALL(Sound_PlayEffect)(at::kSoundShut);
    S()[2] = 1;
}

// --- the draws' parts -------------------------------------------------------------------

// The quad's four corners projected into the POLY_FT4 at p, its depths set.
// The original also pushes a tenth pointer (a flag local) the projection
// does not read.
void Project(unsigned char* p) {
    long depth;
    SH_CALL(Gte_RotTransPers4)(Vertex(0), Vertex(1), Vertex(2), Vertex(3), reinterpret_cast<float*>(p + 8),
                               reinterpret_cast<float*>(p + 0x18), reinterpret_cast<float*>(p + 0x28),
                               reinterpret_cast<float*>(p + 0x38), &depth);
    SH_CALL(Gte_PrimDepths4_10)(p);
}
// A vertex's ground point: (s16 + 0x4000) << 9, the 16.16 the map reads.
long Ground(const unsigned char* word) { return static_cast<long>(static_cast<U>(S16(word) + 0x4000) << 9); }
// base - ground / 2 (movsx ax; cdq; sub; sar 1: toward zero), the low word.
U Above(U base, long height) {
    const std::int32_t half = static_cast<std::int16_t>(static_cast<U>(height) & 0xFFFFu) / 2;
    return base - static_cast<U>(half);
}

// The draw sub-kinds 0x2E..0x32 share but for the texture word: the draw mode
// (page 0x95) committed at slot 6 (0xC); one POLY_FT4. With +8 the quad lies
// across z - its y (+0x3A << 7) - 0x3FC0 - and its x (+0x36 << 7) minus the
// slide +0x30, - 0x3F40 / - 0x4040; without, across x - x (+0x36 << 7) -
// 0x3FC0 - and its y (+0x3A << 7) plus the slide, - 0x3F40 / - 0x4040. Every
// word the low sixteen bits. The heights from the ground: corner 0 0x40 above
// half vertex 0's ground and corner 2 0x180, corners 1 and 3 likewise at
// vertex 1's (four AreaMap_Elevation calls, two pairs with the same
// arguments; vertex 1's point read after the second). Projected; then the
// sub-kind's texture word (from Sprite_Current, read after the projection),
// Prim_SetTexture(word, p, 1), committed at slot 6 (0x48).
void GroundDraw(U (*texture)(const unsigned char* s)) {
    unsigned char* const v = Vertices();
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x95, 0);
    SH_CALL(Gfx_CommitPrim)(6, 0xC);
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyFT4)(p);
    SH_CALL(Gpu_SetShadeTex)(p, 0);
    {
        const unsigned char* const s = S();
        const U slide = Word(s + 0x30);
        if (s[8] != 0) {
            const U y = (static_cast<U>(Word(s + 0x3A)) << 7) - 0x3FC0u;
            SetWord(v + 0x1A, y);
            SetWord(v + 0x12, y);
            SetWord(v + 0xA, y);
            SetWord(v + 2, y);
            const U x = (static_cast<U>(Word(s + 0x36)) << 7) - slide;
            SetWord(v + 0x10, x - 0x3F40u);
            SetWord(v, x - 0x3F40u);
            SetWord(v + 0x18, x - 0x4040u);
            SetWord(v + 8, x - 0x4040u);
        } else {
            const U x = (static_cast<U>(Word(s + 0x36)) << 7) - 0x3FC0u;
            SetWord(v + 0x18, x);
            SetWord(v + 0x10, x);
            SetWord(v + 8, x);
            SetWord(v, x);
            const U y = (static_cast<U>(Word(s + 0x3A)) << 7) + slide;
            SetWord(v + 0x12, y - 0x3F40u);
            SetWord(v + 2, y - 0x3F40u);
            SetWord(v + 0x1A, y - 0x4040u);
            SetWord(v + 0xA, y - 0x4040u);
        }
    }
    const long x0 = Ground(v), y0 = Ground(v + 2);
    long h = SH_CALL(AreaMap_Elevation)(x0, y0);
    SetWord(v + 4, Above(0x40, h));
    h = SH_CALL(AreaMap_Elevation)(x0, y0);
    const long x1 = Ground(v + 8), y1 = Ground(v + 0xA);
    SetWord(v + 0x14, Above(0x180, h));
    h = SH_CALL(AreaMap_Elevation)(x1, y1);
    SetWord(v + 0xC, Above(0x40, h));
    h = SH_CALL(AreaMap_Elevation)(x1, y1);
    SetWord(v + 0x1C, Above(0x180, h));
    Project(p);
    const U word = texture(S());
    SH_CALL(Prim_SetTexture)(word, p, 1);
    SH_CALL(Gfx_CommitPrim)(6, 0x48);
}

}  // namespace

// ===========================================================================
// Sub-kind 0x4A's state 1 (EffectKind18Sub4A_States[1], E5G's table)
// ===========================================================================

// original 0x50C0D0 (hidden in E5G's 0x50BDC0): when +0xB has a bit set in
// Cond_ByteFE and Field_Request is not 2: AreaMap_SetByte(+0x36, +0x3A, 0xA1)
// and (+0x36, +0x3A + 1, 0xA1) (Sprite_Current read again for the second),
// sound 0x200 unless a message is up (Field_Request read again), +2 up. A tail
// jump to sub-kind 0x2C's draw either way.
extern "C" void __cdecl EffectKind18Sub4A_WaitCond(void) {
    {
        unsigned char* s = S();
        if ((s[0xB] & Cond_ByteFE) != 0 && Field_Request != 2) {
            SH_CALL(AreaMap_SetByte)(Word(s + 0x36), Word(s + 0x3A), at::kMapOpen);
            s = S();
            SH_CALL(AreaMap_SetByte)(Word(s + 0x36), (Word(s + 0x3A) + 1u) & 0xFFFFu, at::kMapOpen);
            if (Field_Request == 0) SH_CALL(Sound_PlayEffect)(at::kSoundOpen);
            S()[2] = static_cast<unsigned char>(S()[2] + 1);
        }
    }
    SH_CALL(EffectKind18Sub2C_Draw)();
}

// ===========================================================================
// Sub-kind 0x2D: EffectKind18_States[0x2D] (0x654120), EffectKind18Sub2D_States
// (five) by +2
// ===========================================================================

// original 0x50C4B0 (sub-kind 0x2D's draw; called or tail-jumped to by its
// five states): a draw mode (page 0x95) linked at the record's point with dy
// -2 (0xC); one POLY_FT4: with +8 across z - y (+0x3A << 7) - 0x3FC0, x (+0x36
// << 7) minus the slide times the sign kSub2DSides[+0xA], - 0x3F40 / - 0x4040
// - else across x - x (+0x36 << 7) - 0x3FC0, y (+0x3A << 7) plus the signed
// slide, - 0x3F40 / - 0x4040; flat: corners 0 and 1 at +0x3E - 0x140, 2 and 3
// at +0x3E. Projected; the texture ((+8 == 0) ^ +0xA) << 16 | +8 << 21 |
// 0x1500117, linked likewise (0x48).
extern "C" void __cdecl EffectKind18Sub2D_Draw(void) {
    unsigned char* const v = Vertices();
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x95, 0);
    {
        const unsigned char* const s = S();
        SH_CALL(MapView_LinkPrimAt)(UL(s + 0x34), UL(s + 0x38), -2, 0xC);
    }
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyFT4)(p);
    SH_CALL(Gpu_SetShadeTex)(p, 0);
    {
        const unsigned char* const s = S();
        const unsigned side = s[0xA];
        if (side >= at::kSidesRoom)
            bof3::Fatal("EffectKind18Sub2D_Draw: +0xA is %u, past the %u signs of 0x%X - the original reads on into "
                        "EffectKind18Sub3E_States (docs/effect_6a.md section 7)",
                        side, at::kSidesRoom, (unsigned)at::kSub2DSides);
        const U sign = static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(At(at::kSub2DSides)[side])));
        const U slide = sign * Word(s + 0x30);
        if (s[8] != 0) {
            const U y = (static_cast<U>(Word(s + 0x3A)) << 7) - 0x3FC0u;
            SetWord(v + 0x1A, y);
            SetWord(v + 0x12, y);
            SetWord(v + 0xA, y);
            SetWord(v + 2, y);
            const U x = (static_cast<U>(Word(s + 0x36)) << 7) - slide;
            SetWord(v + 0x10, x - 0x3F40u);
            SetWord(v, x - 0x3F40u);
            SetWord(v + 0x18, x - 0x4040u);
            SetWord(v + 8, x - 0x4040u);
        } else {
            const U x = (static_cast<U>(Word(s + 0x36)) << 7) - 0x3FC0u;
            SetWord(v + 0x18, x);
            SetWord(v + 0x10, x);
            SetWord(v + 8, x);
            SetWord(v, x);
            const U y = slide + (static_cast<U>(Word(s + 0x3A)) << 7);
            SetWord(v + 0x12, y - 0x3F40u);
            SetWord(v + 2, y - 0x3F40u);
            SetWord(v + 0x1A, y - 0x4040u);
            SetWord(v + 0xA, y - 0x4040u);
        }
        SetWord(v + 0xC, Word(s + 0x3E) - 0x140u);
        SetWord(v + 4, Word(s + 0x3E) - 0x140u);
        SetWord(v + 0x1C, Word(s + 0x3E));
        SetWord(v + 0x14, Word(s + 0x3E));
    }
    Project(p);
    {
        const unsigned char* const r = S();
        const U flip = (r[8] == 0 ? 1u : 0u) ^ r[0xA];
        SH_CALL(Prim_SetTexture)((flip << 16) | (static_cast<U>(r[8]) << 21) | 0x1500117u, p, 1);
    }
    const unsigned char* const r = S();
    SH_CALL(MapView_LinkPrimAt)(UL(r + 0x34), UL(r + 0x38), -2, 0x48);
}

// original 0x50C140 (hidden in E5G's 0x50BDC0): jmp [EffectKind18Sub2D_States
// + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub2D_Run(void) {
    Dispatch("EffectKind18Sub2D_Run", AddressOf(EffectKind18Sub2D_States), EffectKind18Sub2D_States_count);
}

// original 0x50C160 (sub-state 0): +0xA = bit 1 of the spawn's z cell's low
// byte (sar; and 1) and +8 = bit 0 clear; the variant's cell (kSub2DCells)
// into +0x36 / +0x3A and its height (kSub2DHeights) into +0x3E; the slide 0,
// +2 up; the leader already at the cell's front (the axis +8 names first): the
// slide 0x100 and +2 = 3. The draw.
extern "C" void __cdecl EffectKind18Sub2D_Place(void) {
    unsigned char* const s = S();
    const unsigned char z = s[0x3A];
    const U v = Variant("EffectKind18Sub2D_Place", s, at::kSub2DVariants);
    s[0xA] = static_cast<unsigned char>((static_cast<signed char>(z) >> 1) & 1);
    s[8] = static_cast<unsigned char>(~z & 1);
    SetWord(s + 0x36, At(at::kSub2DCells + 2 * v)[0]);
    SetWord(s + 0x3A, At(at::kSub2DCells + 2 * v + 1)[0]);
    SetWord(s + 0x3E, Word(At(at::kSub2DHeights + 2 * v)));
    SetWord(s + 0x30, 0);
    s[2] = static_cast<unsigned char>(s[2] + 1);
    if (LeaderAt(s, kFront, 0x8000, 0x10000)) {
        SetWord(s + 0x30, 0x100);
        s[2] = 3;
    }
    SH_CALL(EffectKind18Sub2D_Draw)();
}

// original 0x50C2A0 (sub-state 1): the leader at the front: sound 0x200, +2
// up. The draw.
extern "C" void __cdecl EffectKind18Sub2D_WaitNear(void) {
    WaitNear(kFront);
    SH_CALL(EffectKind18Sub2D_Draw)();
}

// original 0x50C380 (sub-state 2): the slide up 0x20; at 0x100 +2 up. A tail
// jump to the draw.
extern "C" void __cdecl EffectKind18Sub2D_Open(void) {
    Open(true);
    SH_CALL(EffectKind18Sub2D_Draw)();
}

// original 0x50C3A0 (sub-state 3): the leader two cells off: +2 up. The draw.
extern "C" void __cdecl EffectKind18Sub2D_WaitFar(void) {
    WaitFar(kFront);
    SH_CALL(EffectKind18Sub2D_Draw)();
}

// original 0x50C470 (sub-state 4): the slide down 0x20; at 0 or below sound
// 0x201 and +2 = 1. A tail jump to the draw.
extern "C" void __cdecl EffectKind18Sub2D_Close(void) {
    Close(true);
    SH_CALL(EffectKind18Sub2D_Draw)();
}

// ===========================================================================
// Sub-kind 0x3E: EffectKind18_States[0x3E] (0x654164), EffectKind18Sub3E_States
// (five) by +2 - nothing drawn
// ===========================================================================

// original 0x50C6E0 (hidden in 0x50C4B0): jmp [EffectKind18Sub3E_States + +2 *
// 4], unbounded.
extern "C" void __cdecl EffectKind18Sub3E_Run(void) {
    Dispatch("EffectKind18Sub3E_Run", AddressOf(EffectKind18Sub3E_States), EffectKind18Sub3E_States_count);
}

// original 0x50C700 (sub-state 0): PlaceCells (kSub3ECells), the front's
// centre at cell - 2; the leader there: the slide 0x100, +2 = 3.
extern "C" void __cdecl EffectKind18Sub3E_Place(void) {
    PlaceCells("EffectKind18Sub3E_Place", at::kSub3ECells, at::kSub3EVariants, kFront3E, 0x100);
}

// original 0x50C830 (sub-state 1): the leader at the front (cell - 2): sound
// 0x200, +2 up.
extern "C" void __cdecl EffectKind18Sub3E_WaitNear(void) { WaitNear(kFront3E); }

// original 0x50C910 (sub-state 2): the slide up 0x20; at 0x100 +2 up.
extern "C" void __cdecl EffectKind18Sub3E_Open(void) { Open(true); }

// original 0x50C930 (sub-state 3): the leader two cells off (cell - 2): +2 up.
// Not WaitFar: the first test (against the centre, on the axis +8 names) is
// at 0x8000, not 0x20000 (cmp eax, 0x8000 at 0x50C959 and 0x50C9B6; every
// other sub-kind's far test has 0x20000 there) - the leader need only leave
// the front's half-width across that axis (section 7).
extern "C" void __cdecl EffectKind18Sub3E_WaitFar(void) {
    unsigned char* const s = S();
    if (!LeaderAt(s, kFront3E, 0x8000, 0x20000)) s[2] = static_cast<unsigned char>(s[2] + 1);
}

// original 0x50CA00 (sub-state 4): the slide down 0x20; at 0 or below sound
// 0x201 and +2 = 1.
extern "C" void __cdecl EffectKind18Sub3E_Close(void) { Close(true); }

// ===========================================================================
// Sub-kinds 0x2E..0x32: the ground draws
// ===========================================================================

// original 0x50CD90 (sub-kind 0x2E's): GroundDraw, the texture (+8 * 0x21) <<
// 16 | 0x150010F (shl 5; add; shl 16: +8 at bits 16 and 21).
extern "C" void __cdecl EffectKind18Sub2E_Draw(void) {
    GroundDraw([](const unsigned char* s) { return ((static_cast<U>(s[8]) * 0x21u) << 16) | 0x150010Fu; });
}

// original 0x50D680 (sub-kind 0x30's): GroundDraw, the texture (+8 == 0) << 16
// | +8 << 21 | 0x150010F.
extern "C" void __cdecl EffectKind18Sub30_Draw(void) {
    GroundDraw([](const unsigned char* s) {
        return (static_cast<U>(s[8] == 0 ? 1 : 0) << 16) | (static_cast<U>(s[8]) << 21) | 0x150010Fu;
    });
}

// original 0x50DC20 (sub-kind 0x31's): GroundDraw, the texture ((+8 == 0) <
// 0x10) | +8 << 21 | 0x150010F - the compare (sete; cmp ecx, 0x10; setl) is
// always true and its bit 0 already set, so +8 << 21 | 0x150010F (section 7).
extern "C" void __cdecl EffectKind18Sub31_Draw(void) {
    GroundDraw([](const unsigned char* s) {
        const U flag = s[8] == 0 ? 1u : 0u;
        return (flag < 0x10u ? 1u : 0u) | (static_cast<U>(s[8]) << 21) | 0x150010Fu;
    });
}

// original 0x50E1C0 (sub-kinds 0x2F's and 0x32's, and E6B's 0x33 / 0x34 call
// it): GroundDraw, the texture +8 << 21 | 0x150010F.
extern "C" void __cdecl EffectKind18Sub2F_Draw(void) {
    GroundDraw([](const unsigned char* s) { return (static_cast<U>(s[8]) << 21) | 0x150010Fu; });
}

// ===========================================================================
// Sub-kinds 0x2E, 0x30, 0x31 (the slide toward -0x100) and 0x2F, 0x32 (toward
// +0x100): EffectKind18_States[0x2E..0x32] (0x654124..0x654134), each its own
// table of five by +2
// ===========================================================================

// original 0x50CA40 (hidden in 0x50C4B0): jmp [EffectKind18Sub2E_States + +2 *
// 4], unbounded.
extern "C" void __cdecl EffectKind18Sub2E_Run(void) {
    Dispatch("EffectKind18Sub2E_Run", AddressOf(EffectKind18Sub2E_States), EffectKind18Sub2E_States_count);
}
// original 0x50CA60 (sub-state 0): PlaceCells (kSub2ECells); the leader at the
// front: the slide -0x100, +2 = 3. The draw.
extern "C" void __cdecl EffectKind18Sub2E_Place(void) {
    PlaceCells("EffectKind18Sub2E_Place", at::kSub2ECells, at::kSub2EVariants, kFront, 0xFF00);
    SH_CALL(EffectKind18Sub2E_Draw)();
}
// original 0x50CB80 (sub-state 1): WaitNear. The draw.
extern "C" void __cdecl EffectKind18Sub2E_WaitNear(void) {
    WaitNear(kFront);
    SH_CALL(EffectKind18Sub2E_Draw)();
}
// original 0x50CC60 (sub-state 2): the slide down 0x20; at -0x100 +2 up. A
// tail jump to the draw.
extern "C" void __cdecl EffectKind18Sub2E_Open(void) {
    Open(false);
    SH_CALL(EffectKind18Sub2E_Draw)();
}
// original 0x50CC80 (sub-state 3): WaitFar. The draw.
extern "C" void __cdecl EffectKind18Sub2E_WaitFar(void) {
    WaitFar(kFront);
    SH_CALL(EffectKind18Sub2E_Draw)();
}
// original 0x50CD50 (sub-state 4): the slide up 0x20; at 0 or above sound 0x201
// and +2 = 1. A tail jump to the draw.
extern "C" void __cdecl EffectKind18Sub2E_Close(void) {
    Close(false);
    SH_CALL(EffectKind18Sub2E_Draw)();
}

// original 0x50CFE0 (hidden in 0x50CD90): jmp [EffectKind18Sub2F_States + +2 *
// 4], unbounded.
extern "C" void __cdecl EffectKind18Sub2F_Run(void) {
    Dispatch("EffectKind18Sub2F_Run", AddressOf(EffectKind18Sub2F_States), EffectKind18Sub2F_States_count);
}
// original 0x50D000 (sub-state 0): PlaceCells (kSub2FCells); the leader at the
// front: the slide 0x100, +2 = 3. The draw.
extern "C" void __cdecl EffectKind18Sub2F_Place(void) {
    PlaceCells("EffectKind18Sub2F_Place", at::kSub2FCells, at::kSmallVariants, kFront, 0x100);
    SH_CALL(EffectKind18Sub2F_Draw)();
}
// original 0x50D120 (sub-state 1): WaitNear. The draw.
extern "C" void __cdecl EffectKind18Sub2F_WaitNear(void) {
    WaitNear(kFront);
    SH_CALL(EffectKind18Sub2F_Draw)();
}
// original 0x50D200 (sub-state 2): the slide up 0x20; at 0x100 +2 up. A tail
// jump to the draw.
extern "C" void __cdecl EffectKind18Sub2F_Open(void) {
    Open(true);
    SH_CALL(EffectKind18Sub2F_Draw)();
}
// original 0x50D220 (sub-state 3): WaitFar. The draw.
extern "C" void __cdecl EffectKind18Sub2F_WaitFar(void) {
    WaitFar(kFront);
    SH_CALL(EffectKind18Sub2F_Draw)();
}
// original 0x50D2F0 (sub-state 4): the slide down 0x20; at 0 or below sound
// 0x201 and +2 = 1. A tail jump to the draw.
extern "C" void __cdecl EffectKind18Sub2F_Close(void) {
    Close(true);
    SH_CALL(EffectKind18Sub2F_Draw)();
}

// original 0x50D330 (hidden in 0x50CD90): jmp [EffectKind18Sub30_States + +2 *
// 4], unbounded.
extern "C" void __cdecl EffectKind18Sub30_Run(void) {
    Dispatch("EffectKind18Sub30_Run", AddressOf(EffectKind18Sub30_States), EffectKind18Sub30_States_count);
}
// original 0x50D350 (sub-state 0): PlaceCells (kSub30Cells); the leader at the
// front: the slide -0x100, +2 = 3. The draw.
extern "C" void __cdecl EffectKind18Sub30_Place(void) {
    PlaceCells("EffectKind18Sub30_Place", at::kSub30Cells, at::kSmallVariants, kFront, 0xFF00);
    SH_CALL(EffectKind18Sub30_Draw)();
}
// original 0x50D470 (sub-state 1): WaitNear. The draw.
extern "C" void __cdecl EffectKind18Sub30_WaitNear(void) {
    WaitNear(kFront);
    SH_CALL(EffectKind18Sub30_Draw)();
}
// original 0x50D550 (sub-state 2): the slide down 0x20; at -0x100 +2 up. A
// tail jump to the draw.
extern "C" void __cdecl EffectKind18Sub30_Open(void) {
    Open(false);
    SH_CALL(EffectKind18Sub30_Draw)();
}
// original 0x50D570 (sub-state 3): WaitFar. The draw.
extern "C" void __cdecl EffectKind18Sub30_WaitFar(void) {
    WaitFar(kFront);
    SH_CALL(EffectKind18Sub30_Draw)();
}
// original 0x50D640 (sub-state 4): the slide up 0x20; at 0 or above sound 0x201
// and +2 = 1. A tail jump to the draw.
extern "C" void __cdecl EffectKind18Sub30_Close(void) {
    Close(false);
    SH_CALL(EffectKind18Sub30_Draw)();
}

// original 0x50D8D0 (hidden in 0x50D680): jmp [EffectKind18Sub31_States + +2 *
// 4], unbounded.
extern "C" void __cdecl EffectKind18Sub31_Run(void) {
    Dispatch("EffectKind18Sub31_Run", AddressOf(EffectKind18Sub31_States), EffectKind18Sub31_States_count);
}
// original 0x50D8F0 (sub-state 0): PlaceCells (kSub31Cells); the leader at the
// front: the slide -0x100, +2 = 3. The draw.
extern "C" void __cdecl EffectKind18Sub31_Place(void) {
    PlaceCells("EffectKind18Sub31_Place", at::kSub31Cells, at::kSmallVariants, kFront, 0xFF00);
    SH_CALL(EffectKind18Sub31_Draw)();
}
// original 0x50DA10 (sub-state 1): WaitNear. The draw.
extern "C" void __cdecl EffectKind18Sub31_WaitNear(void) {
    WaitNear(kFront);
    SH_CALL(EffectKind18Sub31_Draw)();
}
// original 0x50DAF0 (sub-state 2): the slide down 0x20; at -0x100 +2 up. A
// tail jump to the draw.
extern "C" void __cdecl EffectKind18Sub31_Open(void) {
    Open(false);
    SH_CALL(EffectKind18Sub31_Draw)();
}
// original 0x50DB10 (sub-state 3): WaitFar. The draw.
extern "C" void __cdecl EffectKind18Sub31_WaitFar(void) {
    WaitFar(kFront);
    SH_CALL(EffectKind18Sub31_Draw)();
}
// original 0x50DBE0 (sub-state 4): the slide up 0x20; at 0 or above sound 0x201
// and +2 = 1. A tail jump to the draw.
extern "C" void __cdecl EffectKind18Sub31_Close(void) {
    Close(false);
    SH_CALL(EffectKind18Sub31_Draw)();
}

// original 0x50DE70 (hidden in 0x50DC20): jmp [EffectKind18Sub32_States + +2 *
// 4], unbounded.
extern "C" void __cdecl EffectKind18Sub32_Run(void) {
    Dispatch("EffectKind18Sub32_Run", AddressOf(EffectKind18Sub32_States), EffectKind18Sub32_States_count);
}
// original 0x50DE90 (sub-state 0): PlaceCells (kSub32Cells); the leader at the
// front: the slide 0x100, +2 = 3. The draw (0x2F's).
extern "C" void __cdecl EffectKind18Sub32_Place(void) {
    PlaceCells("EffectKind18Sub32_Place", at::kSub32Cells, at::kSmallVariants, kFront, 0x100);
    SH_CALL(EffectKind18Sub2F_Draw)();
}
// original 0x50DFB0 (sub-state 1): WaitNear. The draw.
extern "C" void __cdecl EffectKind18Sub32_WaitNear(void) {
    WaitNear(kFront);
    SH_CALL(EffectKind18Sub2F_Draw)();
}
// original 0x50E090 (sub-state 2): the slide up 0x20; at 0x100 +2 up. A tail
// jump to the draw.
extern "C" void __cdecl EffectKind18Sub32_Open(void) {
    Open(true);
    SH_CALL(EffectKind18Sub2F_Draw)();
}
// original 0x50E0B0 (sub-state 3): WaitFar. The draw.
extern "C" void __cdecl EffectKind18Sub32_WaitFar(void) {
    WaitFar(kFront);
    SH_CALL(EffectKind18Sub2F_Draw)();
}
// original 0x50E180 (sub-state 4): the slide down 0x20; at 0 or below sound
// 0x201 and +2 = 1. A tail jump to the draw.
extern "C" void __cdecl EffectKind18Sub32_Close(void) {
    Close(true);
    SH_CALL(EffectKind18Sub2F_Draw)();
}

void Effect6A_Inject() {
    if (bof3::WantsShadow("effect_6a")) effect_6a::SelfTest();
    BOF3_INJECT(EffectKind18Sub4A_WaitCond);
    BOF3_INJECT(EffectKind18Sub2D_Run);
    BOF3_INJECT(EffectKind18Sub2D_Place);
    BOF3_INJECT(EffectKind18Sub2D_WaitNear);
    BOF3_INJECT(EffectKind18Sub2D_Open);
    BOF3_INJECT(EffectKind18Sub2D_WaitFar);
    BOF3_INJECT(EffectKind18Sub2D_Close);
    BOF3_INJECT(EffectKind18Sub2D_Draw);
    BOF3_INJECT(EffectKind18Sub3E_Run);
    BOF3_INJECT(EffectKind18Sub3E_Place);
    BOF3_INJECT(EffectKind18Sub3E_WaitNear);
    BOF3_INJECT(EffectKind18Sub3E_Open);
    BOF3_INJECT(EffectKind18Sub3E_WaitFar);
    BOF3_INJECT(EffectKind18Sub3E_Close);
    BOF3_INJECT(EffectKind18Sub2E_Run);
    BOF3_INJECT(EffectKind18Sub2E_Place);
    BOF3_INJECT(EffectKind18Sub2E_WaitNear);
    BOF3_INJECT(EffectKind18Sub2E_Open);
    BOF3_INJECT(EffectKind18Sub2E_WaitFar);
    BOF3_INJECT(EffectKind18Sub2E_Close);
    BOF3_INJECT(EffectKind18Sub2E_Draw);
    BOF3_INJECT(EffectKind18Sub2F_Run);
    BOF3_INJECT(EffectKind18Sub2F_Place);
    BOF3_INJECT(EffectKind18Sub2F_WaitNear);
    BOF3_INJECT(EffectKind18Sub2F_Open);
    BOF3_INJECT(EffectKind18Sub2F_WaitFar);
    BOF3_INJECT(EffectKind18Sub2F_Close);
    BOF3_INJECT(EffectKind18Sub30_Run);
    BOF3_INJECT(EffectKind18Sub30_Place);
    BOF3_INJECT(EffectKind18Sub30_WaitNear);
    BOF3_INJECT(EffectKind18Sub30_Open);
    BOF3_INJECT(EffectKind18Sub30_WaitFar);
    BOF3_INJECT(EffectKind18Sub30_Close);
    BOF3_INJECT(EffectKind18Sub30_Draw);
    BOF3_INJECT(EffectKind18Sub31_Run);
    BOF3_INJECT(EffectKind18Sub31_Place);
    BOF3_INJECT(EffectKind18Sub31_WaitNear);
    BOF3_INJECT(EffectKind18Sub31_Open);
    BOF3_INJECT(EffectKind18Sub31_WaitFar);
    BOF3_INJECT(EffectKind18Sub31_Close);
    BOF3_INJECT(EffectKind18Sub31_Draw);
    BOF3_INJECT(EffectKind18Sub32_Run);
    BOF3_INJECT(EffectKind18Sub32_Place);
    BOF3_INJECT(EffectKind18Sub32_WaitNear);
    BOF3_INJECT(EffectKind18Sub32_Open);
    BOF3_INJECT(EffectKind18Sub32_WaitFar);
    BOF3_INJECT(EffectKind18Sub32_Close);
    BOF3_INJECT(EffectKind18Sub2F_Draw);
}

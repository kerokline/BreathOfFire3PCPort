// Round thirteen group E6D (docs/effect_6d.md): the 51 functions of
// analysis/round13_cut.tsv's group E6D (50 rows) and the one start its band
// holds that no list has (0x5166C0), 0x514270..0x516B2E, each read with
// capstone to its last instruction (2026-10-03). Effect_RunObjects (ours)
// makes each live record of Effect_Objects (20 of 0x80 bytes) Sprite_Current
// and calls Effect_KindHandlers[+5]; kind 0x18's EffectKind18_Run jumps through
// EffectKind18_States by +1 (the sub-kind, EffectKind18_Start copies it from
// +0xB). Five of the band's sub-kinds dispatch again through a table of their
// own (by +2, or by Cond_ByteFE for 0x61; none bounded by a compare); two are
// one state. What each is, as far as the code says:
//
//   sub-kind 0x5C   two panels of three textured quads each (a vertex table),
//                   placed at a cell a variant names; a story flag set while
//                   they are shut and cleared as they open: they slide apart
//                   when the leader stands at the cell (or Cond_ByteFE is 1),
//                   wait, and slide back once the leader is away (or
//                   Cond_ByteFE is 1); drawn only while not waiting shut
//   sub-kind 0x5E   two panels of one quad, the ground under each corner,
//                   sliding along x when the leader stands at the cell
//   sub-kind 0x5D   two panels at a variant's cell lying across x or z (the
//                   spawn's z cell 0 chooses), a height of their own, flat
//   sub-kind 0x61   by Cond_ByteFE: a screen overlay of textured pieces raised
//                   or slid, a glow of a disc and four rings, the followers'
//                   tracks marked on the map behind them, a scrolling mist
//   sub-kind 0x62   the map's corner heights rippled around Field_Kind2's cell
//   sub-kind 0x63   six projected Gouraud quads turning with Cond_AngleFB
//   sub-kind 0x64   a crossed pair of textured quads at a variant's cell that
//                   follows the state of an enemy record (+0xC) in battle
//   sub-kind 0x65   CLUT rows 4 and 5 faded in from black
//
// Every call goes through the harness (SH_CALL), so the start-up fuzz can stand
// recorders in for ours as for the originals' copies. Sprite_Current is read
// again wherever the original reads [0x937F88] again after a call. No
// divergence: each is a faithful replacement. Where the original jumps through
// a table past its end or indexes a variant table past its room, ours aborts
// with a message (docs/effect_6d.md section 7).
#include "game/effect_6d.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_6d_callees.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_6d::at;
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
std::int32_t S16(const unsigned char* p) { return static_cast<std::int16_t>(Word(p)); }
std::int32_t S16(U a) { return S16(At(a)); }
std::int32_t I(U v) { return static_cast<std::int32_t>(v); }
U S8U(unsigned char b) { return static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(b))); }
U AddressOf(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
unsigned char* Vertices() { return reinterpret_cast<unsigned char*>(Prim_VertexScratch); }
const short* Vertex(unsigned i) { return reinterpret_cast<const short*>(Vertices() + 8 * i); }
// cdq; and edx, 0xFFF; add; sar 12 - a signed divide by 4096, toward zero.
std::int32_t Div4096(U v) { return I(v) / 4096; }
// The float bits of an int (fild; fstp dword).
U FloatBits(std::int32_t v) {
    const float f = static_cast<float>(v);
    U b;
    std::memcpy(&b, &f, sizeof b);
    return b;
}
long double Fl(U a) {
    float f;
    std::memcpy(&f, At(a), sizeof f);
    return f;
}
// The CRT's _ftol 0x5B9550 on the value x87 holds: truncation through a 64-bit
// fistp, whose NaN and out-of-range answer is the integer indefinite
// 0x8000000000000000; the callers keep eax's low word.
U Ftol(long double v) {
    if (!(v > -9.2233720368547758e18L && v < 9.2233720368547758e18L)) return 0;
    return static_cast<U>(static_cast<std::uint64_t>(static_cast<std::int64_t>(v)));
}

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 2]; jmp [table + eax
// * 4]: the table's `entries` handlers read in place (the fuzz swaps the cells
// for recorders); a Fatal past them, where the original jumps through the dword
// after - another table or data.
void Dispatch(const char* who, U table, unsigned entries) {
    const unsigned sub = Sprite_Current[2];
    if (sub >= entries)
        bof3::Fatal("%s: sub-state byte +2 is %u, past the %u entries of 0x%X - the original jumps through the dword "
                    "after (docs/effect_6d.md section 7)",
                    who, sub, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(table + 4 * sub)))();
}

// The variant the spawn's x cell (+0x36, movsx) names, checked against the
// room its tables have.
U Variant(const char* who, const unsigned char* s, unsigned room) {
    const std::int32_t v = S16(s + 0x36);
    if (v < 0 || v >= static_cast<std::int32_t>(room))
        bof3::Fatal("%s: the variant +0x36 is %d, past the %u its tables have room for - the original reads on into "
                    "the next table (docs/effect_6d.md section 7)",
                    who, (int)v, room);
    return static_cast<U>(v);
}

// cdq; xor eax, edx; sub eax, edx: |d| as the original computes it -
// 0x80000000 stays itself, negative.
std::int32_t Abs(U d) {
    const U m = I(d) < 0 ? 0xFFFFFFFFu : 0u;
    return I((d ^ m) - m);
}
// A point against a cell's centre on one axis: point - ((cell + 1) << 16 |
// 0x8000) (inc; shl 16; or dh, 0x80).
U FromCentre(U point, std::int32_t cell) { return point - ((static_cast<U>(cell + 1) << 16) | 0x8000u); }
// ... and against its edge: point - (cell << 16).
U FromEdge(U point, std::int32_t cell) { return point - (static_cast<U>(cell) << 16); }
// |a| within `first`, then |b| or |b - 0x10000| within `second` (the cell or
// the one past it).
bool Within(U a, std::int32_t first, U b, std::int32_t second) {
    if (Abs(a) > first) return false;
    if (Abs(b) <= second) return true;
    return Abs(b - 0x10000u) <= second;
}
U LeaderX() { return UL(at::kLeaderX); }
U LeaderZ() { return UL(at::kLeaderZ); }
// The leader at the record's cell, z across the centre first (sub-kind 0x5E,
// and 0x5D with +8 set), or x first (0x5D without).
bool LeaderAt(const unsigned char* s, bool z_first, std::int32_t first, std::int32_t second) {
    const std::int32_t cx = S16(s + 0x36), cz = S16(s + 0x3A);
    if (z_first) return Within(FromCentre(LeaderZ(), cz), first, FromEdge(LeaderX(), cx), second);
    return Within(FromCentre(LeaderX(), cx), first, FromEdge(LeaderZ(), cz), second);
}

// A vertex's ground point: (s16 + 0x4000) << 9, the 16.16 the map reads.
long Ground(const unsigned char* word) { return static_cast<long>(static_cast<U>(S16(word) + 0x4000) << 9); }
// movsx eax, ax; cdq; sub eax, edx; sar 1: half a height, toward zero.
U Half(long height) { return static_cast<U>(static_cast<std::int16_t>(static_cast<U>(height) & 0xFFFFu) / 2); }

// The quad's four corners projected into the POLY_FT4 / G4 at p, its depths
// set. The original also pushes a tenth pointer (a flag local) the projection
// does not read.
void Project(unsigned char* p) {
    long depth;
    SH_CALL(Gte_RotTransPers4)(Vertex(0), Vertex(1), Vertex(2), Vertex(3), reinterpret_cast<float*>(p + 8),
                               reinterpret_cast<float*>(p + 0x18), reinterpret_cast<float*>(p + 0x28),
                               reinterpret_cast<float*>(p + 0x38), &depth);
    SH_CALL(Gte_PrimDepths4_10)(p);
}
// The draw mode the panels put first: Gpu_SetDrawMode(prim, 0, 0, 0x95, 0).
void PanelMode() { SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x95, 0); }
void Link(int dy, unsigned size) {
    unsigned char* const s = S();
    SH_CALL(MapView_LinkPrimAt)(UL(s + 0x34), UL(s + 0x38), dy, size);
}
// A texture window of 8 bytes taken from the packet cursor (mov ecx,
// [Gfx_PacketNext]; add 8): x, y 0 and w, h - its address the draw mode's
// last argument.
U Window(U x, U wh) {
    unsigned char* const r = Gfx_PacketNext;
    Gfx_PacketNext = r + 8;
    SetWord(r, x);
    SetWord(r + 2, 0);
    SetWord(r + 6, wh);
    SetWord(r + 4, wh);
    return AddressOf(r);
}
// A slide of +0x30 by `by`; answers +0x30 (signed, read again).
std::int32_t Slide(U by) {
    unsigned char* const s = S();
    SetWord(s + 0x30, Word(s + 0x30) + by);
    return S16(S() + 0x30);
}
// Closed: the sound unless a message is up (Field_Request), +2 = 1 (read after
// the sound).
void Shut() {
    if (Field_Request == 0) SH_CALL(Sound_PlayEffect)(static_cast<unsigned short>(at::kSoundShut));
    S()[2] = 1;
}
void StepUp() { S()[2] = static_cast<unsigned char>(S()[2] + 1); }

}  // namespace

// ===========================================================================
// Sub-kind 0x5C: EffectKind18_States[0x5C] (0x6541DC), EffectKind18Sub5C_States
// (five) by +2
// ===========================================================================

// original 0x5144F0 (cdecl (side, dy); sub-kind 0x5C's draw, twice a state):
// x (+0x36 << 7) - the slide +0x30 times the side's sign (0x65F390) + side *
// 0xC0 - 0x4040 and y (+0x3A << 7) - 0x4040 as floats into MapView_ScreenXY;
// three textured quads, each: a draw mode (page 0x95) linked at the record's
// point with dy (0xC), its four corners the vertex table's (x, y) plus the two
// floats through _ftol and its z, projected, the texture the side's and the
// variant's (+0xB) of 0x65F3DC, linked with dy (0x48).
extern "C" void __cdecl EffectKind18Sub5C_DrawPanel(unsigned side, int dy) {
    {
        unsigned char* const s = S();
        if (side >= at::kSub5CSidesUsed)
            bof3::Fatal("EffectKind18Sub5C_DrawPanel: side %u, past the two its tables hold - the original reads on into "
                        "the next variant's textures (docs/effect_6d.md section 7)",
                        side);
        const U sign = S8U(At(at::kSub5CSides)[side]);
        const U x = (static_cast<U>(S16(s + 0x36)) << 7) - sign * static_cast<U>(S16(s + 0x30)) + side * 0xC0u - 0x4040u;
        SetUL(At(AddressOf(MapView_ScreenXY)), FloatBits(I(x)));
        const U y = (static_cast<U>(S16(s + 0x3A)) << 7) - 0x4040u;
        SetUL(At(AddressOf(MapView_ScreenXY) + 4), FloatBits(I(y)));
    }
    for (U i = 0; i < at::kSub5CQuads; ++i) {
        PanelMode();
        Link(dy, 0xC);
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyFT4)(p);
        SH_CALL(Gpu_SetShadeTex)(p, 0);
        for (U v = 0; v < 4; ++v) {
            const unsigned char* const t = At(at::kSub5CVertices + 6 * (4 * i + v));
            unsigned char* const out = Vertices() + 8 * v;
            SetWord(out, Ftol(static_cast<long double>(S16(t)) + Fl(AddressOf(MapView_ScreenXY))));
            SetWord(out + 2, Ftol(static_cast<long double>(S16(t + 2)) + Fl(AddressOf(MapView_ScreenXY) + 4)));
            SetWord(out + 4, Word(t + 4));
        }
        Project(p);
        const U variant = S()[0xB];
        if (variant >= at::kSub5CVariants)
            bof3::Fatal("EffectKind18Sub5C_DrawPanel: +0xB is %u, past the two variants of 0x%X - the original reads on "
                        "into EffectKind18Sub5E_States (docs/effect_6d.md section 7)",
                        (unsigned)variant, (unsigned)at::kSub5CTextures);
        SH_CALL(Prim_SetTexture)(UL(at::kSub5CTextures + 4 * (3 * side + 6 * variant + i)), p, 1);
        Link(dy, 0x48);
    }
}

// original 0x514270 (hidden in E6C's 0x5140C0): jmp [EffectKind18Sub5C_States +
// +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub5C_Run(void) {
    Dispatch("EffectKind18Sub5C_Run", AddressOf(EffectKind18Sub5C_States), EffectKind18Sub5C_States_count);
}

// The pair as every state but the first draws it: side 0 at `dy0`, side 1 at 1.
static void Sub5CPair(int dy0) {
    SH_CALL(EffectKind18Sub5C_DrawPanel)(0, dy0);
    SH_CALL(EffectKind18Sub5C_DrawPanel)(1, 1);
}
// The opening states' dy for side 0: -1, or -2 once +0x30 reaches 0x80.
static int Sub5CDy() { return S16(S() + 0x30) >= 0x80 ? -2 : -1; }

// original 0x514290 (sub-state 0): the variant v the spawn's x cell names: +0xB
// = v, the cell (0x65F388) into +0x36 / +0x3A, +8 the variant's story flag
// (0x65F38C), set (Flags_Set(Cond_Flags + 0xA0, +8)); +0x30 0, +2 up. The pair.
extern "C" void __cdecl EffectKind18Sub5C_Place(void) {
    unsigned char* const s = S();
    const U v = Variant("EffectKind18Sub5C_Place", s, at::kSub5CVariants);
    s[0xB] = static_cast<unsigned char>(v);
    SetWord(s + 0x36, At(at::kSub5CCells + 2 * v)[0]);
    SetWord(s + 0x3A, At(at::kSub5CCells + 2 * v + 1)[0]);
    s[8] = At(at::kSub5CFlags + v)[0];
    SH_CALL(Flags_Set)(At(at::kStoryFlags), s[8]);
    SetWord(S() + 0x30, 0);
    StepUp();
    Sub5CPair(1);
}

// original 0x514310 (sub-state 1): Cond_ByteFE 1 is taken (0) and opens; or the
// leader within half a cell of the cell's centre along z and within two cells
// of the cell past it along x. Opening: the flag +8 cleared
// (Flags_Clear(Cond_Flags + 0xA0, +8)), sound 0x200 unless a message is up,
// +2 up (read after the calls), the pair. Otherwise nothing, not even drawn.
extern "C" void __cdecl EffectKind18Sub5C_WaitNear(void) {
    bool forced = false;
    if (Cond_ByteFE == 1) {
        forced = true;
        Cond_ByteFE = 0;
    }
    unsigned char* const s = S();
    const bool near = Abs(FromCentre(LeaderZ(), S16(s + 0x3A))) <= 0x8000 &&
                      Abs(FromEdge(LeaderX(), S16(s + 0x36)) - 0x10000u) <= 0x20000;
    if (!near && !forced) return;
    SH_CALL(Flags_Clear)(At(at::kStoryFlags), s[8]);
    if (Field_Request == 0) SH_CALL(Sound_PlayEffect)(static_cast<unsigned short>(at::kSoundOpen));
    StepUp();
    Sub5CPair(1);
}

// original 0x5143C0 (sub-state 2): the slide up 0x10; at 0xC0 +2 up. The pair,
// side 0 at -1 (-2 from 0x80).
extern "C" void __cdecl EffectKind18Sub5C_Open(void) {
    if (Slide(0x10) >= 0xC0) StepUp();
    Sub5CPair(Sub5CDy());
}

// original 0x514410 (sub-state 3): Cond_ByteFE 1 is taken (0) and +2 goes up;
// then, the leader more than four cells off the cell's edge along z or three
// off the cell past it along x, +2 up again. Nothing drawn.
extern "C" void __cdecl EffectKind18Sub5C_WaitFar(void) {
    if (Cond_ByteFE == 1) {
        Cond_ByteFE = 0;
        StepUp();
    }
    unsigned char* const s = S();
    if (Abs(FromEdge(LeaderZ(), S16(s + 0x3A))) > 0x40000 ||
        Abs(FromEdge(LeaderX(), S16(s + 0x36)) - 0x10000u) > 0x30000)
        s[2] = static_cast<unsigned char>(s[2] + 1);
}

// original 0x514470 (sub-state 4): the slide down 0x10; at 0 or below the flag
// +8 set again, sound 0x201 unless a message is up, +2 = 1. The pair, side 0
// at -1 (-2 from 0x80).
extern "C" void __cdecl EffectKind18Sub5C_Close(void) {
    if (Slide(0xFFF0u) <= 0) {
        SH_CALL(Flags_Set)(At(at::kStoryFlags), S()[8]);
        Shut();
    }
    Sub5CPair(Sub5CDy());
}

// ===========================================================================
// Sub-kind 0x5E: EffectKind18_States[0x5E] (0x6541E4), EffectKind18Sub5E_States
// (five) by +2
// ===========================================================================

// original 0x514880 (sub-kind 0x5E's draw: called by its states 0, 1, 3, tail-
// jumped to by 2 and 4): the corners' y (+0x3A << 7) - 0x3FC0; a draw mode
// (page 0x95) committed at slot 6 (0xC); two textured quads, i 0 and 1: x
// (+0x36 << 7) - 0x3F40 / - 0x4040 plus the slide +0x30 times the side's sign
// (0x65F420); each corner's height from the ground under it (the top two
// -ground / 2, the bottom two 0x180 below), projected, the texture (0x11D - i)
// | 0x1790000, committed at slot 6 (0x48).
extern "C" void __cdecl EffectKind18Sub5E_Draw(void) {
    unsigned char* const v = Vertices();
    unsigned char* const mode = Gfx_PacketNext;
    const U y = (static_cast<U>(Word(S() + 0x3A)) << 7) - 0x3FC0u;
    SetWord(v + 0x1A, y);
    SetWord(v + 0x12, y);
    SetWord(v + 0xA, y);
    SetWord(v + 2, y);
    SH_CALL(Gpu_SetDrawMode)(mode, 0, 0, 0x95, 0);
    SH_CALL(Gfx_CommitPrim)(6, 0xC);
    for (U i = 0; i < 2; ++i) {
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyFT4)(p);
        SH_CALL(Gpu_SetShadeTex)(p, 0);
        unsigned char* const s = S();
        const U sign = S8U(At(at::kSub5ESides)[i]);
        const long y0 = Ground(v + 2);
        const U slid = Word(s + 0x30) * sign + (static_cast<U>(Word(s + 0x36)) << 7);
        SetWord(v + 0x10, slid - 0x3F40u);
        SetWord(v, slid - 0x3F40u);
        SetWord(v + 0x18, slid - 0x4040u);
        SetWord(v + 8, slid - 0x4040u);
        const long x0 = Ground(v);
        long h = SH_CALL(AreaMap_Elevation)(x0, y0);
        SetWord(v + 4, 0u - Half(h));
        h = SH_CALL(AreaMap_Elevation)(x0, y0);
        SetWord(v + 0x14, 0x180u - Half(h));
        const long y1 = Ground(v + 0xA), x1 = Ground(v + 8);
        h = SH_CALL(AreaMap_Elevation)(x1, y1);
        SetWord(v + 0xC, 0u - Half(h));
        h = SH_CALL(AreaMap_Elevation)(x1, y1);
        SetWord(v + 0x1C, 0x180u - Half(h));
        Project(p);
        SH_CALL(Prim_SetTexture)((0x11Du - i) | 0x1790000u, p, 1);
        SH_CALL(Gfx_CommitPrim)(6, 0x48);
    }
}

// original 0x514690 (hidden in 0x5144F0): jmp [EffectKind18Sub5E_States + +2 *
// 4], unbounded.
extern "C" void __cdecl EffectKind18Sub5E_Run(void) {
    Dispatch("EffectKind18Sub5E_Run", AddressOf(EffectKind18Sub5E_States), EffectKind18Sub5E_States_count);
}

// original 0x5146B0 (sub-state 0): +0x3E 0xFF80, +0x30 0, +2 up; the leader
// already at the cell (within half a cell of its centre along z, on it or the
// next along x): +0x30 0xC0, +2 = 3. The draw.
extern "C" void __cdecl EffectKind18Sub5E_Start(void) {
    SetWord(S() + 0x3E, 0xFF80u);
    SetWord(S() + 0x30, 0);
    StepUp();
    unsigned char* const s = S();
    if (LeaderAt(s, true, 0x8000, 0x10000)) {
        SetWord(s + 0x30, 0xC0);
        S()[2] = 3;
    }
    SH_CALL(EffectKind18Sub5E_Draw)();
}

// original 0x514740 (sub-state 1): the leader at the cell: sound 0x200 unless a
// message is up, +2 up (read after the sound). The draw.
extern "C" void __cdecl EffectKind18Sub5E_WaitNear(void) {
    unsigned char* s = S();
    if (LeaderAt(s, true, 0x8000, 0x10000)) {
        if (Field_Request == 0) {
            SH_CALL(Sound_PlayEffect)(static_cast<unsigned short>(at::kSoundOpen));
            s = S();
        }
        s[2] = static_cast<unsigned char>(s[2] + 1);
    }
    SH_CALL(EffectKind18Sub5E_Draw)();
}

// original 0x5147C0 (sub-state 2): the slide up 0x10; at 0xC0 +2 up. A tail jump
// to the draw.
extern "C" void __cdecl EffectKind18Sub5E_Open(void) {
    if (Slide(0x10) >= 0xC0) StepUp();
    SH_CALL(EffectKind18Sub5E_Draw)();
}

// original 0x5147E0 (sub-state 3): the leader two cells off (the same test at
// 0x20000 both ways fails): +2 up. The draw.
extern "C" void __cdecl EffectKind18Sub5E_WaitFar(void) {
    unsigned char* const s = S();
    if (!LeaderAt(s, true, 0x20000, 0x20000)) s[2] = static_cast<unsigned char>(s[2] + 1);
    SH_CALL(EffectKind18Sub5E_Draw)();
}

// original 0x514840 (sub-state 4): the slide down 0x10; at 0 or below sound
// 0x201 unless a message is up and +2 = 1. A tail jump to the draw.
extern "C" void __cdecl EffectKind18Sub5E_Close(void) {
    if (Slide(0xFFF0u) <= 0) Shut();
    SH_CALL(EffectKind18Sub5E_Draw)();
}

// ===========================================================================
// Sub-kind 0x5D: EffectKind18_States[0x5D] (0x6541E0), EffectKind18Sub5D_States
// (five) by +2
// ===========================================================================

// original 0x514DF0 (sub-kind 0x5D's draw: called by its states 0, 1, 3, tail-
// jumped to by 2 and 4): with +8 the corners' y (+0x3A << 7) - 0x3FC0 and the
// panels slide along x, else x (+0x36 << 7) - 0x3FC0 and they slide along z;
// the top corners at +0x3E - +0x2E, the bottom ones at +0x3E. Two sides: a
// draw mode (page 0x95) linked at the record's point with dy -1 (0xC), the
// corners slid by +0x30 times the side's sign (0x65F44C), projected, the
// texture (+0x20 + side) | (+8 << 21), linked likewise (0x48).
extern "C" void __cdecl EffectKind18Sub5D_Draw(void) {
    unsigned char* const v = Vertices();
    {
        unsigned char* const s = S();
        if (s[8] != 0) {
            const U y = (static_cast<U>(Word(s + 0x3A)) << 7) - 0x3FC0u;
            SetWord(v + 0x1A, y);
            SetWord(v + 0x12, y);
            SetWord(v + 0xA, y);
            SetWord(v + 2, y);
        } else {
            const U x = (static_cast<U>(Word(s + 0x36)) << 7) - 0x3FC0u;
            SetWord(v + 0x18, x);
            SetWord(v + 0x10, x);
            SetWord(v + 8, x);
            SetWord(v, x);
        }
        const U top = Word(s + 0x3E) - Word(s + 0x2E);
        SetWord(v + 0xC, top);
        SetWord(v + 4, top);
        SetWord(v + 0x1C, Word(s + 0x3E));
        SetWord(v + 0x14, Word(s + 0x3E));
    }
    for (U side = 0; side < 2; ++side) {
        PanelMode();
        Link(-1, 0xC);
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyFT4)(p);
        SH_CALL(Gpu_SetShadeTex)(p, 0);
        unsigned char* const s = S();
        const U sign = S8U(At(at::kSub5DSides)[side]);
        const U slide = Word(s + 0x30) * sign;
        if (s[8] != 0) {
            const U x = (side + Word(s + 0x36)) << 7;
            SetWord(v + 0x10, x - slide - 0x4040u);
            SetWord(v, x - slide - 0x4040u);
            SetWord(v + 0x18, x - slide - 0x3FC0u);
            SetWord(v + 8, x - slide - 0x3FC0u);
        } else {
            const U y = (Word(s + 0x3A) - side) << 7;
            SetWord(v + 0x12, y + slide - 0x3F40u);
            SetWord(v + 2, y + slide - 0x3F40u);
            SetWord(v + 0x1A, y + slide - 0x3FC0u);
            SetWord(v + 0xA, y + slide - 0x3FC0u);
        }
        Project(p);
        {
            unsigned char* const r = S();
            SH_CALL(Prim_SetTexture)((UL(r + 0x20) + side) | (static_cast<U>(r[8]) << 21), p, 1);
        }
        Link(-1, 0x48);
    }
}

// original 0x514A70 (hidden in 0x514880): jmp [EffectKind18Sub5D_States + +2 *
// 4], unbounded.
extern "C" void __cdecl EffectKind18Sub5D_Run(void) {
    Dispatch("EffectKind18Sub5D_Run", AddressOf(EffectKind18Sub5D_States), EffectKind18Sub5D_States_count);
}

// original 0x514A90 (sub-state 0): +8 = whether the spawn's z cell is 0; the
// variant the x cell names: the cell (0x65F448), +0x3E (0x65F438), +0x20
// (0x65F440), +0x2E (0x65F43C); +0x30 0, +2 up; the leader already at the
// cell (the axis +8 names first): +0x30 0x80, +2 = 3. The draw.
extern "C" void __cdecl EffectKind18Sub5D_Place(void) {
    unsigned char* const s = S();
    s[8] = Word(s + 0x3A) == 0 ? 1 : 0;
    const U v = Variant("EffectKind18Sub5D_Place", s, at::kSub5DVariants);
    SetWord(s + 0x36, At(at::kSub5DCells + 2 * v)[0]);
    SetWord(s + 0x3A, At(at::kSub5DCells + 2 * v + 1)[0]);
    SetWord(s + 0x3E, Word(At(at::kSub5DLifts + 2 * v)));
    SetUL(s + 0x20, UL(at::kSub5DTextures + 4 * v));
    SetWord(s + 0x2E, Word(At(at::kSub5DHeights + 2 * v)));
    SetWord(s + 0x30, 0);
    s[2] = static_cast<unsigned char>(s[2] + 1);
    if (LeaderAt(s, s[8] != 0, 0x8000, 0x10000)) {
        SetWord(s + 0x30, 0x80);
        s[2] = 3;
    }
    SH_CALL(EffectKind18Sub5D_Draw)();
}

// original 0x514BE0 (sub-state 1): the leader at the cell (the axis +8 names
// first): sound 0x200 unless a message is up, +2 up (read after the sound).
// The draw.
extern "C" void __cdecl EffectKind18Sub5D_WaitNear(void) {
    unsigned char* s = S();
    if (LeaderAt(s, s[8] != 0, 0x8000, 0x10000)) {
        if (Field_Request == 0) {
            SH_CALL(Sound_PlayEffect)(static_cast<unsigned short>(at::kSoundOpen));
            s = S();
        }
        s[2] = static_cast<unsigned char>(s[2] + 1);
    }
    SH_CALL(EffectKind18Sub5D_Draw)();
}

// original 0x514CC0 (sub-state 2): the slide up 0x10; at 0x80 +2 up. A tail jump
// to the draw.
extern "C" void __cdecl EffectKind18Sub5D_Open(void) {
    if (Slide(0x10) >= 0x80) StepUp();
    SH_CALL(EffectKind18Sub5D_Draw)();
}

// original 0x514CE0 (sub-state 3): the leader two cells off: +2 up. The draw.
extern "C" void __cdecl EffectKind18Sub5D_WaitFar(void) {
    unsigned char* const s = S();
    if (!LeaderAt(s, s[8] != 0, 0x20000, 0x20000)) s[2] = static_cast<unsigned char>(s[2] + 1);
    SH_CALL(EffectKind18Sub5D_Draw)();
}

// original 0x514DB0 (sub-state 4): the slide down 0x10; at 0 or below sound
// 0x201 unless a message is up and +2 = 1. A tail jump to the draw.
extern "C" void __cdecl EffectKind18Sub5D_Close(void) {
    if (Slide(0xFFF0u) <= 0) Shut();
    SH_CALL(EffectKind18Sub5D_Draw)();
}

// ===========================================================================
// Sub-kind 0x61: EffectKind18_States[0x61] (0x6541F0), its steps
// EffectKind18Sub61_Steps (seven) by Cond_ByteFE
// ===========================================================================

// original 0x5151A0 (cdecl (y); the overlay, called by steps 1, 2 and 3):
// nothing unless Draw_PassFlags has bit 2. A draw mode (page 0x99, the window
// (16, 0) 16 x 16) committed at slot 7 (0xC); two textured quads, (0, 0)..(256,
// 256) and (256, 0)..(320, 256), CLUT 0x79C0, page 0x99, committed (0x48); a
// draw mode (page 0x99, the window 256 x 256) (0xC); the eight pieces of
// 0x65F470 at their (x, y + y) .. (x + w, y + h + y), their (u, v) .. (u + du,
// v + dv), their page byte's page and CLUT, committed (0x48).
extern "C" void __cdecl EffectKind18Sub61_DrawOverlay(int y) {
    if ((Draw_PassFlags & 4) == 0) return;
    {
        const U window = Window(0x10, 0x10);
        SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x99, window);
    }
    SH_CALL(Gfx_CommitPrim)(7, 0xC);
    const U f256 = 0x43800000u, f320 = 0x43A00000u;
    {
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyFT4)(p);
        SH_CALL(Gpu_SetShadeTex)(p, 1);
        SetUL(p + 8, 0);
        SetUL(p + 0xC, 0);
        SetUL(p + 0x18, f256);
        SetUL(p + 0x1C, 0);
        SetUL(p + 0x28, 0);
        SetUL(p + 0x2C, f256);
        SetUL(p + 0x38, f256);
        SetUL(p + 0x3C, f256);
        p[0x14] = 0;
        p[0x15] = 0;
        p[0x24] = 0xFF;
        p[0x25] = 0;
        p[0x34] = 0;
        p[0x35] = 0xFF;
        p[0x44] = 0xFF;
        p[0x45] = 0xFF;
        SetWord(p + 0x26, 0x99);
        SetWord(p + 0x16, 0x79C0);
        SH_CALL(Gfx_CommitPrim)(7, 0x48);
    }
    {
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyFT4)(p);
        SH_CALL(Gpu_SetShadeTex)(p, 1);
        SetUL(p + 0x18, f320);
        SetUL(p + 0x38, f320);
        SetUL(p + 8, f256);
        SetUL(p + 0xC, 0);
        SetUL(p + 0x1C, 0);
        SetUL(p + 0x28, f256);
        SetUL(p + 0x2C, f256);
        SetUL(p + 0x3C, f256);
        p[0x14] = 0;
        p[0x15] = 0;
        p[0x24] = 0x40;
        p[0x25] = 0;
        p[0x34] = 0;
        p[0x35] = 0xFF;
        p[0x44] = 0x40;
        p[0x45] = 0xFF;
        SetWord(p + 0x26, 0x99);
        SetWord(p + 0x16, 0x79C0);
        SH_CALL(Gfx_CommitPrim)(7, 0x48);
    }
    {
        const U window = Window(0, 0x100);
        SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x99, window);
    }
    SH_CALL(Gfx_CommitPrim)(7, 0xC);
    const U dy = static_cast<U>(y);
    for (U k = 0; k < at::kOverlayCount; ++k) {
        const unsigned char* const e = At(at::kOverlayPieces + at::kOverlayStride * k);
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyFT4)(p);
        SH_CALL(Gpu_SetShadeTex)(p, 1);
        const U x = static_cast<U>(S16(e)), top = static_cast<U>(S16(e + 2)) + dy;
        const U w = static_cast<U>(S16(e + 4)), h = static_cast<U>(S16(e + 6));
        const U bottom = static_cast<U>(S16(e + 2)) + (dy + h);
        SetUL(p + 8, FloatBits(I(x)));
        SetUL(p + 0xC, FloatBits(I(top)));
        SetUL(p + 0x18, FloatBits(I(x + w)));
        SetUL(p + 0x1C, FloatBits(I(top)));
        SetUL(p + 0x28, FloatBits(I(x)));
        SetUL(p + 0x2C, FloatBits(I(bottom)));
        SetUL(p + 0x38, FloatBits(I(x + w)));
        SetUL(p + 0x3C, FloatBits(I(bottom)));
        p[0x14] = e[9];
        p[0x15] = e[0xA];
        p[0x24] = static_cast<unsigned char>(e[0xB] + e[9]);
        p[0x25] = e[0xA];
        p[0x34] = e[9];
        p[0x35] = static_cast<unsigned char>(e[0xC] + e[0xA]);
        p[0x44] = static_cast<unsigned char>(e[0xB] + e[9]);
        p[0x45] = static_cast<unsigned char>(e[0xC] + e[0xA]);
        SetWord(p + 0x26, ((((static_cast<U>(e[8]) << 7) + 0x140u) >> 6) & 0xFu) | 0x90u);
        SetWord(p + 0x16, (static_cast<U>(e[8]) + 0x1E5u) << 6);
        SH_CALL(Gfx_CommitPrim)(7, 0x48);
    }
}

// original 0x515CC0 (cdecl (x, y, radius, shade); called by the glow): 64
// semi-transparent POLY_G3s about (x, y), each from the centre (shade, grey) to
// two points of the rim at radius (black), angles k * 0x40 and (k + 1) * 0x40
// (Math_Sin for x, Math_Cos for y, * radius / 4096), committed at slot 4
// (0x34).
extern "C" void __cdecl EffectKind18Sub61_DrawDisc(int x, int y, int radius, unsigned shade) {
    const U fx = FloatBits(x), fy = FloatBits(y);
    const U r = static_cast<U>(radius);
    for (U a = 0; a < 0x40000u; a += 0x1000u) {
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG3)(p);
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        SetUL(p + 8, fx);
        SetUL(p + 0xC, fy);
        const int a0 = static_cast<int>(a >> 6);
        U t = static_cast<U>(SH_CALL(Math_Sin)(a0));
        SetUL(p + 0x18, FloatBits(Div4096(t * r) + x));
        t = static_cast<U>(SH_CALL(Math_Cos)(a0));
        SetUL(p + 0x1C, FloatBits(Div4096(t * r) + y));
        const int a1 = static_cast<int>((a + 0x1000u) >> 6);
        t = static_cast<U>(SH_CALL(Math_Sin)(a1));
        SetUL(p + 0x28, FloatBits(Div4096(t * r) + x));
        t = static_cast<U>(SH_CALL(Math_Cos)(a1));
        const std::int32_t y2 = Div4096(t * r) + y;
        p[0x14] = 0;
        p[0x15] = 0;
        p[0x16] = 0;
        p[0x24] = 0;
        p[4] = static_cast<unsigned char>(shade);
        p[5] = static_cast<unsigned char>(shade);
        p[6] = static_cast<unsigned char>(shade);
        p[0x25] = 0;
        SetUL(p + 0x2C, FloatBits(y2));
        p[0x26] = 0;
        SH_CALL(Gfx_CommitPrim)(4, 0x34);
    }
}

// original 0x515950 (cdecl (x, y, radius, shade); called four times by the
// glow): 64 steps about (x, y), angles k * 0x40 and (k + 1) * 0x40, each two
// semi-transparent POLY_G4s: radius + 2 (black) in to radius (shade), and
// radius (shade) in to radius - 8 (black); committed at slot 4 (0x44).
extern "C" void __cdecl EffectKind18Sub61_DrawRing(int x, int y, int radius, unsigned shade) {
    const unsigned char c = static_cast<unsigned char>(shade);
    const U r = static_cast<U>(radius), outer = r + 2, inner = r - 8;
    // a corner: Math_Sin(angle) * reach / 4096 + x, then Math_Cos for y
    auto corner = [&](unsigned char* p, int angle, U reach) {
        U t = static_cast<U>(SH_CALL(Math_Sin)(angle));
        SetUL(p, FloatBits(Div4096(t * reach) + x));
        t = static_cast<U>(SH_CALL(Math_Cos)(angle));
        SetUL(p + 4, FloatBits(Div4096(t * reach) + y));
    };
    auto colours = [](unsigned char* p, unsigned char first, unsigned char second) {
        for (U k = 0; k < 3; ++k) {
            p[4 + k] = first;
            p[0x14 + k] = first;
            p[0x24 + k] = second;
            p[0x34 + k] = second;
        }
    };
    for (U a = 0; a < 0x40000u; a += 0x1000u) {
        const int a0 = static_cast<int>(a >> 6), a1 = static_cast<int>((a + 0x1000u) >> 6);
        {
            unsigned char* const p = Gfx_PacketNext;
            SH_CALL(Gpu_SetPolyG4)(p);
            SH_CALL(Gpu_SetSemiTrans)(p, 1);
            corner(p + 8, a0, outer);
            corner(p + 0x18, a1, outer);
            corner(p + 0x28, a0, r);
            corner(p + 0x38, a1, r);
            colours(p, 0, c);
            SH_CALL(Gfx_CommitPrim)(4, 0x44);
        }
        {
            unsigned char* const p = Gfx_PacketNext;
            SH_CALL(Gpu_SetPolyG4)(p);
            SH_CALL(Gpu_SetSemiTrans)(p, 1);
            corner(p + 8, a0, r);
            corner(p + 0x18, a1, r);
            corner(p + 0x28, a0, inner);
            corner(p + 0x38, a1, inner);
            colours(p, c, 0);
            SH_CALL(Gfx_CommitPrim)(4, 0x44);
        }
    }
}

// original 0x5156B0 (the glow; called by step 3 while +0x38 is above 0x80): a
// draw mode (page 0xB5, additive) committed at slot 4 (0xC); +9 up to 0xFF;
// past 0x14 by t, a disc at (0x82, +9 + 10) of radius 20t + 0x20 (at most 500)
// and shade 12t + 0x80 (at most 0xFF); then four rings of width 0x40, 0x20, 8
// and 0x10 (shade 12 * +9, at most 0x30) about (0x82 + Math_Sin(0x100) * (0x1E
// - +9) * m / 4096, +9 + 10 + Math_Cos(0x100) * (0x1E - +9) * m / 4096) for m 9,
// 8, 6 and 4, +9 read again for each.
extern "C" void __cdecl EffectKind18Sub61_DrawGlow(void) {
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0xB5, 0);
    SH_CALL(Gfx_CommitPrim)(4, 0xC);
    if (S()[9] < 0xFF) S()[9] = static_cast<unsigned char>(S()[9] + 1);
    {
        const U b = S()[9];
        const U t = b > 0x14 ? b - 0x14 : 0;
        U radius = t * 20 + 0x20;
        if (I(radius) > 0x1F4) radius = 0x1F4;
        U shade = t * 12 + 0x80;
        if (I(shade) > 0xFF) shade = 0xFF;
        SH_CALL(EffectKind18Sub61_DrawDisc)(0x82, I(b + 0xA), I(radius), shade);
    }
    U width = static_cast<U>(S()[9]) * 12;
    if (I(width) > 0x30) width = 0x30;
    struct Ring { U times; int reach; };
    static constexpr Ring kRings[] = {{9, 0x40}, {8, 0x20}, {6, 8}, {4, 0x10}};
    for (const Ring& ring : kRings) {
        const U b = S()[9];
        const U c = static_cast<U>(SH_CALL(Math_Cos)(0x100));
        const std::int32_t ry = Div4096(c * (0x1Eu - b) * ring.times) + I(b) + 0xA;
        const U sn = static_cast<U>(SH_CALL(Math_Sin)(0x100));
        const U b2 = S()[9];
        const std::int32_t rx = Div4096(sn * (0x1Eu - b2) * ring.times) + 0x82;
        SH_CALL(EffectKind18Sub61_DrawRing)(rx, ry, ring.reach, width);
    }
}

// original 0x515E00 (the mist; a tail jump from EffectKind18Sub61_Run): nothing
// while +2 is 0. Else two layers, each a draw mode (page 0x97, dtd, the window
// 64 x 64) committed at slot 4 (0xC), two semi-transparent textured quads side
// by side 256 wide and 256 high from (-o, o - 0x20), CLUT 0x78C0, page 0x7B,
// committed (0x48), and a draw mode (page 0x97, the window 256 x 256) (0xC):
// the first layer's offset +2 & 0x3F and shade (0x80 - |0x80 - +2|) / 2, the
// second's (+2 & 0x1F) * 2 and shade 0x80 - |0x80 - +2| while that is under
// 0x40 off 0x80, else (Math_Cos((+2 - 0x40) << 5) / 128 + 0x60) / 2. Then +2 up
// 4.
extern "C" void __cdecl EffectKind18Sub61_DrawMist(void) {
    const U c = S()[2];
    if (c == 0) return;
    const std::int32_t off = Abs(0x80u - c);
    const std::int32_t e = 0x80 - off;
    U shades[2];
    shades[0] = static_cast<U>(e >> 1);
    if (off > 0x40) {
        shades[1] = static_cast<U>(e);
    } else {
        const std::int32_t t = SH_CALL(Math_Cos)(static_cast<int>((c - 0x40u) << 5));
        shades[1] = static_cast<U>(((t >> 7) + 0x60) >> 1);
    }
    U offsets[2];
    {
        const U c2 = S()[2];
        offsets[0] = c2 & 0x3F;
        offsets[1] = (c2 & 0x1F) << 1;
    }
    auto quad = [](unsigned char* p, U x0, U x1, U top, U bottom, unsigned char shade) {
        SetUL(p + 8, x0);
        SetUL(p + 0xC, top);
        SetUL(p + 0x18, x1);
        SetUL(p + 0x1C, top);
        SetUL(p + 0x28, x0);
        SetUL(p + 0x2C, bottom);
        SetUL(p + 0x38, x1);
        SetUL(p + 0x3C, bottom);
        p[0x14] = 0;
        p[0x15] = 0;
        p[0x24] = 0xFF;
        p[0x25] = 0;
        p[0x34] = 0;
        p[0x35] = 0xFF;
        p[0x44] = 0xFF;
        p[0x45] = 0xFF;
        p[4] = shade;
        p[5] = shade;
        p[6] = shade;
        SetWord(p + 0x26, 0x7B);
        SetWord(p + 0x16, 0x78C0);
    };
    for (U layer = 0; layer < 2; ++layer) {
        const std::int32_t o = I(offsets[layer]);
        const unsigned char shade = static_cast<unsigned char>(shades[layer]);
        {
            const U window = Window(0, 0x40);
            SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x97, window);
        }
        SH_CALL(Gfx_CommitPrim)(4, 0xC);
        const U top = FloatBits(o - 0x20), bottom = FloatBits(o + 0xE0);
        const U left = FloatBits(-o), middle = FloatBits(0x100 - o), right = FloatBits(0x200 - o);
        {
            unsigned char* const p = Gfx_PacketNext;
            SH_CALL(Gpu_SetPolyFT4)(p);
            SH_CALL(Gpu_SetShadeTex)(p, 0);
            SH_CALL(Gpu_SetSemiTrans)(p, 1);
            quad(p, left, middle, top, bottom, shade);
            SH_CALL(Gfx_CommitPrim)(4, 0x48);
        }
        {
            unsigned char* const p = Gfx_PacketNext;
            SH_CALL(Gpu_SetPolyFT4)(p);
            SH_CALL(Gpu_SetShadeTex)(p, 0);
            SH_CALL(Gpu_SetSemiTrans)(p, 1);
            quad(p, middle, right, top, bottom, shade);
            SH_CALL(Gfx_CommitPrim)(4, 0x48);
        }
        {
            const U window = Window(0, 0x100);
            SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x97, window);
        }
        SH_CALL(Gfx_CommitPrim)(4, 0xC);
    }
    S()[2] = static_cast<unsigned char>(S()[2] + 4);
}

// original 0x515440 (the tracks; called by EffectKind18Sub61_Run every frame):
// for followers 1 and 2 (Sprite_Objects[1], [2]): when the last mark's cell x
// (the follower's ring head, 0x6BC704) is past the follower's (x + 0x4000) >>
// 16, a new mark at the next of 32 entries - (x cell, the z cell byte +0x3A,
// the parity flipped). Then every mark of the ring: the map item half at its
// cell, a POLY_FT4 copying the half's four corners, the texture the follower's
// first byte (0x65F4E0) + the parity | 0x25808000, linked at the cell; and the
// half one z step on (0x65F4E4), the second byte, linked there.
extern "C" void __cdecl EffectKind18Sub61_DrawTracks(void) {
    auto copy = [](unsigned char* p, const unsigned char* item) {
        for (U corner = 8; corner < 0x48; corner += 0x10) std::memcpy(p + corner, item + corner, 12);
    };
    for (U r = 0; r < at::kFollowerCount; ++r) {
        const unsigned char* const follower = At(at::kFollowers + at::kFollowerStride * r);
        const std::int32_t cell = I(UL(follower) + 0x4000u) >> 16;
        unsigned char* const head = At(at::kTrackHeads + r);
        const U h = *head;
        if (h >= at::kTrackEntries)
            bof3::Fatal("EffectKind18Sub61_DrawTracks: follower %u's head is %u, past the ring's %u entries - the "
                        "original reads and writes past it (docs/effect_6d.md section 7)",
                        (unsigned)r, (unsigned)h, at::kTrackEntries);
        const unsigned char* const last = At(at::kTrackRing + at::kTrackStride * h + 3 * r);
        if (static_cast<std::int32_t>(last[0]) > cell) {
            const U next = (h + 1) & 0x1F;
            *head = static_cast<unsigned char>(next);
            unsigned char* const mark = At(at::kTrackRing + at::kTrackStride * next + 3 * r);
            mark[0] = static_cast<unsigned char>(cell);
            mark[1] = follower[6];
            mark[2] = static_cast<unsigned char>((last[2] - 1) & 1);
        }
        for (U k = 0; k < at::kTrackEntries; ++k) {
            const unsigned char* const t = At(at::kTrackRing + 3 * r + at::kTrackStride * k);
            const unsigned char* item = SH_CALL(MapView_ItemHalfAt)(t[0], t[1]);
            if (item == nullptr) continue;
            {
                unsigned char* const p = Gfx_PacketNext;
                SH_CALL(Gpu_SetPolyFT4)(p);
                SH_CALL(Gpu_SetShadeTex)(p, 1);
                copy(p, item);
                SH_CALL(Prim_SetTexture)(At(at::kTrackTextures + 2 * r)[0] + t[2] + 0x25808000u, p, 1);
                SH_CALL(MapView_LinkPrimAt)(static_cast<U>(t[0]) << 16, static_cast<U>(t[1]) << 16, 0, 0x48);
            }
            const U step = S8U(At(at::kTrackSteps + r)[0]);
            item = SH_CALL(MapView_ItemHalfAt)(t[0], static_cast<long>(t[1] + step));
            if (item == nullptr) continue;
            unsigned char* const p = Gfx_PacketNext;
            SH_CALL(Gpu_SetPolyFT4)(p);
            SH_CALL(Gpu_SetShadeTex)(p, 1);
            copy(p, item);
            SH_CALL(Prim_SetTexture)(At(at::kTrackTextures + 2 * r + 1)[0] + t[2] + 0x25808000u, p, 1);
            const U step2 = S8U(At(at::kTrackSteps + r)[0]);
            SH_CALL(MapView_LinkPrimAt)(static_cast<U>(t[0]) << 16, (t[1] + step2) << 16, 0, 0x48);
        }
    }
}

// original 0x515000 (hidden in 0x514DF0): call [EffectKind18Sub61_Steps +
// Cond_ByteFE * 4], unbounded; then the tracks, and a tail jump to the mist.
extern "C" void __cdecl EffectKind18Sub61_Run(void) {
    const unsigned step = Cond_ByteFE;
    if (step >= EffectKind18Sub61_Steps_count)
        bof3::Fatal("EffectKind18Sub61_Run: Cond_ByteFE is %u, past the %u steps of 0x%X - the original calls through "
                    "the dword after (docs/effect_6d.md section 7)",
                    step, (unsigned)EffectKind18Sub61_Steps_count, (unsigned)AddressOf(EffectKind18Sub61_Steps));
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(AddressOf(EffectKind18Sub61_Steps) + 4 * step)))();
    SH_CALL(EffectKind18Sub61_DrawTracks)();
    SH_CALL(EffectKind18Sub61_DrawMist)();
}

// original 0x515020 (step 0): MapView_FocusX 0x16FF: +2 = 4 (the mist starts).
extern "C" void __cdecl EffectKind18Sub61_WaitFocus(void) {
    if (MapView_FocusX == 0x16FF) S()[2] = 4;
}

// original 0x515040 (step 1): +0x34 down 1 every fourth frame (Frame_Counter &
// 3 = 0); the overlay at y +0x34.
extern "C" void __cdecl EffectKind18Sub61_Slide(void) {
    const U by = (Frame_Counter & 3) == 0 ? 1u : 0u;
    unsigned char* const s = S();
    SetUL(s + 0x34, UL(s + 0x34) - by);
    SH_CALL(EffectKind18Sub61_DrawOverlay)(I(UL(S() + 0x34)));
}

// original 0x515070 (step 2): the overlay at y 0.
extern "C" void __cdecl EffectKind18Sub61_Hold(void) { SH_CALL(EffectKind18Sub61_DrawOverlay)(0); }

// original 0x515080 (step 3): by +0x38 (signed): above 0x100 the glow and the
// overlay at +0x38; above 0x80 the glow, +0x38 up 2, the overlay; above 0x60
// +0x38 up 2, the overlay; else up 1, the overlay.
extern "C" void __cdecl EffectKind18Sub61_Rise(void) {
    unsigned char* const s = S();
    const U v = UL(s + 0x38);
    if (I(v) > 0x100) {
        SH_CALL(EffectKind18Sub61_DrawGlow)();
    } else if (I(v) > 0x80) {
        SH_CALL(EffectKind18Sub61_DrawGlow)();
        SetUL(S() + 0x38, UL(S() + 0x38) + 2);
    } else if (I(v) > 0x60) {
        SetUL(s + 0x38, v + 2);
    } else {
        SetUL(s + 0x38, v + 1);
    }
    SH_CALL(EffectKind18Sub61_DrawOverlay)(I(UL(S() + 0x38)));
}

// original 0x515100 (step 4): +0x38 0, +0x34 0x54, +0x3C 0; the ring's first
// entry (0x7E, 0x14, 1) and (0x54, 0x13, 0); Cond_ByteFE 0.
extern "C" void __cdecl EffectKind18Sub61_Reset(void) {
    SetUL(S() + 0x38, 0);
    SetUL(S() + 0x34, 0x54);
    SetUL(S() + 0x3C, 0);
    unsigned char* const ring = At(at::kTrackRing);
    ring[0] = 0x7E;
    ring[1] = 0x14;
    ring[2] = 1;
    ring[3] = 0x54;
    ring[4] = 0x13;
    ring[5] = 0;
    Cond_ByteFE = 0;
}

// original 0x515160 (step 6, and step 5's call): both heads 0, every mark's
// cell x 0xFF (none drawn), the first entry's parities 1 and 0; Cond_ByteFE 0.
extern "C" void __cdecl EffectKind18Sub61_ClearTracks(void) {
    SetWord(At(at::kTrackHeads), 0);
    for (U r = 0; r < 2; ++r)
        for (U k = 0; k < at::kTrackEntries; ++k) At(at::kTrackRing + 3 * r + at::kTrackStride * k)[0] = 0xFF;
    At(at::kTrackRing)[2] = 1;
    At(at::kTrackRing)[5] = 0;
    Cond_ByteFE = 0;
}

// original 0x515150 (step 5): the tracks cleared, then Cond_ByteFE 1.
extern "C" void __cdecl EffectKind18Sub61_Rearm(void) {
    SH_CALL(EffectKind18Sub61_ClearTracks)();
    Cond_ByteFE = 1;
}

// ===========================================================================
// Sub-kinds 0x62 and 0x63: one state each
// ===========================================================================

// original 0x516090 (EffectKind18_States[0x62], 0x6541F4; hidden in 0x515CC0):
// Camera_Distance 0x5DC; for 35 columns j from Field_Kind2X's cell - 0x14 and
// 35 rows from Field_Kind2Z's cell - 0x14 inside the map (AreaMap_Header's
// width and height), the corner bytes 0 and 2 of each cell up by Math_Sin's
// step at ((Frame_Counter + 3j) << 7) less the one 0x80 before (each >> 10),
// bytes 1 and 3 by the step at + 3 less + 2. Then Sprite_ObjectsExtra[0]'s
// height from the ground at its point, its +0x14 0, MapView_Redraw 2.
extern "C" void __cdecl EffectKind18Sub62_Ripple(void) {
    const std::int32_t row0 = S16(at::kKind2ZCell) - 0x14;
    const std::int32_t col0 = S16(at::kKind2XCell) - 0x14;
    Camera_Distance = 0x5DC;
    for (U j = 0; j < 35; ++j) {
        const U phase = 3 * j;
        const std::int32_t s1 = SH_CALL(Math_Sin)(static_cast<int>((((Frame_Counter + phase) << 7) - 0x80u) & 0xFFFu));
        const std::int32_t s2 = SH_CALL(Math_Sin)(static_cast<int>(((Frame_Counter + phase) << 7) & 0xFFFu));
        const U up02 = static_cast<U>((s2 >> 10) - (s1 >> 10));
        const std::int32_t s3 = SH_CALL(Math_Sin)(static_cast<int>(((Frame_Counter + phase + 3) << 7) & 0xFFFu));
        const std::int32_t s4 = SH_CALL(Math_Sin)(static_cast<int>(((Frame_Counter + phase + 2) << 7) & 0xFFFu));
        const U up13 = static_cast<U>((s3 >> 10) - (s4 >> 10));
        std::int32_t row = row0;
        for (U n = 0; n < 35; ++n, ++row) {
            if (row < 0) continue;
            if (row >= static_cast<std::int32_t>(AreaMap_Header[1])) continue;
            const std::int32_t col = static_cast<std::int32_t>(j) + col0;
            if (col < 0) continue;
            if (col >= static_cast<std::int32_t>(AreaMap_Header[0])) continue;
            unsigned char* const corners = reinterpret_cast<unsigned char*>(&AreaMap_Corners);
            const U at0 = 4 * static_cast<U>(row * static_cast<std::int32_t>(AreaMap_Header[0]) + col);
            corners[at0] = static_cast<unsigned char>(corners[at0] + up02);
            const U at2 = 4 * static_cast<U>(row * static_cast<std::int32_t>(AreaMap_Header[0]) + col) + 2;
            corners[at2] = static_cast<unsigned char>(corners[at2] + up02);
            const U at1 = 4 * static_cast<U>(row * static_cast<std::int32_t>(AreaMap_Header[0]) + col) + 1;
            corners[at1] = static_cast<unsigned char>(corners[at1] + up13);
            const U at3 = 4 * static_cast<U>(row * static_cast<std::int32_t>(AreaMap_Header[0]) + col) + 3;
            corners[at3] = static_cast<unsigned char>(corners[at3] + up13);
        }
    }
    const long h = SH_CALL(AreaMap_Elevation)(Long(At(at::kExtra0Point)), Long(At(at::kExtra0Point + 4)));
    SetWord(At(at::kExtra0Height), static_cast<U>(h));
    SetUL(At(at::kExtra0Word14), 0);
    MapView_Redraw = 2;
}

// original 0x516280 (EffectKind18_States[0x63], 0x6541F8; hidden in 0x515CC0):
// Cond_ByteFE 1; a draw mode (page 0xB5, additive) committed at slot 4 (0xC);
// six semi-transparent POLY_G4s, quad n from the points k and k + 1 of
// 0x65F4E8 (k the n-th byte of 0x65F554) to the same two turned by
// Cond_AngleFB + 0x200 (x by Math_Sin, y by Math_Cos, times each point's reach
// of 0x65F530 / 4096, z the reach table's), projected, depths by
// Gte_PrimDepths4_10B, the two near corners the points' shades (grey), the far
// ones black, committed (0x44); a draw mode (page 0x95) (0xC).
extern "C" void __cdecl EffectKind18Sub63_Glow(void) {
    Cond_ByteFE = 1;
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0xB5, 0);
    SH_CALL(Gfx_CommitPrim)(4, 0xC);
    unsigned char* const v = Vertices();
    for (U n = 0; n < at::kGlowQuads; ++n) {
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG4)(p);
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        {
            const U k = At(at::kGlowOrder + n)[0];
            for (U i = 0; i < 2; ++i) {
                const unsigned char* const point = At(at::kGlowPoints + 8 * (k + i));
                SetWord(v + 8 * i, Word(point));
                SetWord(v + 8 * i + 2, Word(point + 2));
                SetWord(v + 8 * i + 4, Word(point + 4));
            }
        }
        for (U i = 0; i < 2; ++i) {
            unsigned char* const out = v + 0x10 + 8 * i;
            const U k = At(at::kGlowOrder + n)[0] + i;
            const U sn = static_cast<U>(SH_CALL(Math_Sin)(S16(AddressOf(&Cond_AngleFB)) + 0x200));
            const U x = static_cast<U>(Div4096(sn * static_cast<U>(S16(at::kGlowReach + 4 * k)))) + Word(At(at::kGlowPoints + 8 * k));
            SetWord(out, x);
            const U k2 = At(at::kGlowOrder + n)[0] + i;
            const U cs = static_cast<U>(SH_CALL(Math_Cos)(S16(AddressOf(&Cond_AngleFB)) + 0x200));
            const U y = static_cast<U>(Div4096(cs * static_cast<U>(S16(at::kGlowReach + 4 * k2)))) + Word(At(at::kGlowPoints + 8 * k2 + 2));
            SetWord(out + 2, y);
            SetWord(out + 4, Word(At(at::kGlowReach + 4 * k2 + 2)));
        }
        long depth;
        SH_CALL(Gte_RotTransPers4)(Vertex(0), Vertex(1), Vertex(2), Vertex(3), reinterpret_cast<float*>(p + 8),
                                   reinterpret_cast<float*>(p + 0x18), reinterpret_cast<float*>(p + 0x28),
                                   reinterpret_cast<float*>(p + 0x38), &depth);
        SH_CALL(Gte_PrimDepths4_10B)(p);
        const unsigned char near0 = At(at::kGlowPoints + 8 * At(at::kGlowOrder + n)[0] + 6)[0];
        p[6] = near0;
        p[5] = near0;
        p[4] = near0;
        const unsigned char near1 = At(at::kGlowPoints + 8 * At(at::kGlowOrder + n)[0] + 0xE)[0];
        p[0x36] = 0;
        p[0x16] = near1;
        p[0x15] = near1;
        p[0x14] = near1;
        p[0x26] = 0;
        p[0x35] = 0;
        p[0x25] = 0;
        p[0x34] = 0;
        p[0x24] = 0;
        SH_CALL(Gfx_CommitPrim)(4, 0x44);
    }
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x95, 0);
    SH_CALL(Gfx_CommitPrim)(4, 0xC);
}

// ===========================================================================
// Sub-kind 0x64: EffectKind18_States[0x64] (0x6541FC), EffectKind18Sub64_States
// (seven) by +2
// ===========================================================================

// original 0x5167C0 (cdecl (scale, width, step, lift); every state's draw): +0xA
// up by the step byte, less 12 past 11; frames f0 = +0xA / 2 and f1 = f0 + 2
// (f0 - 4 past 5), each + ((scale << 16) | 0xBF009111) a texture; two crossed
// textured quads at the record's cell ((+0x36 << 7) - 0x3FF0, (+0x3A << 7) -
// 0x3FF0): i 0 across z, i 1 across x, width either side, the top lift above
// the near edge; the top corners -(2 width + 0xC0) - ground / 2, the bottom
// -0xC0 - ground / 2 (the ground at the record's point, twice). Each after a
// draw mode (page 0x95) linked at the point with dy 1 (0xC); projected, the
// frame's texture, linked (0x48).
extern "C" void __cdecl EffectKind18Sub64_Draw(unsigned scale, int width, unsigned step, int lift) {
    {
        unsigned char* const s = S();
        s[0xA] = static_cast<unsigned char>(s[0xA] + static_cast<unsigned char>(step));
    }
    {
        unsigned char* const s = S();
        U f = s[0xA];
        if (f > 0xB) f -= 0xC;
        s[0xA] = static_cast<unsigned char>(f);
    }
    unsigned char* const s = S();
    const U f0 = static_cast<U>(s[0xA]) >> 1;
    const U f1 = f0 + 2 > 5 ? f0 - 4 : f0 + 2;
    const U base = (scale << 16) | 0xBF009111u;
    const U textures[2] = {f0 + base, f1 + base};
    const U x = (static_cast<U>(S16(s + 0x36)) << 7) - 0x3FF0u;
    const U z = (static_cast<U>(S16(s + 0x3A)) << 7) - 0x3FF0u;
    const U w = static_cast<U>(width), l = static_cast<U>(lift);
    unsigned char* const v = Vertices();
    for (U i = 0; i < 2; ++i) {
        PanelMode();
        Link(1, 0xC);
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyFT4)(p);
        SH_CALL(Gpu_SetShadeTex)(p, 0);
        const U across = w * i;
        SetWord(v + 0x10, across + x);
        SetWord(v, across + x);
        SetWord(v + 0x18, x - across);
        SetWord(v + 8, x - across);
        const U along = i == 0 ? w : 0u;
        SetWord(v + 2, along - l + z);
        SetWord(v + 0x12, along + z);
        SetWord(v + 0x1A, z - along);
        SetWord(v + 0xA, z - along - l);
        {
            unsigned char* const r = S();
            const long h = SH_CALL(AreaMap_Elevation)(Long(r + 0x34), Long(r + 0x38));
            const U top = (0u - (2 * w + 0xC0u)) - Half(h);
            SetWord(v + 0xC, top);
            SetWord(v + 4, top);
        }
        {
            unsigned char* const r = S();
            const long h = SH_CALL(AreaMap_Elevation)(Long(r + 0x34), Long(r + 0x38));
            const U bottom = 0xFFFFFF40u - Half(h);
            SetWord(v + 0x1C, bottom);
            SetWord(v + 0x14, bottom);
        }
        Project(p);
        SH_CALL(Prim_SetTexture)(textures[i], p, 1);
        Link(1, 0x48);
    }
}

// original 0x516480 (hidden in 0x515CC0): jmp [EffectKind18Sub64_States + +2 *
// 4], unbounded.
extern "C" void __cdecl EffectKind18Sub64_Run(void) {
    Dispatch("EffectKind18Sub64_Run", AddressOf(EffectKind18Sub64_States), EffectKind18Sub64_States_count);
}

namespace {
// The enemy record +0xC holds (Place stores it): its bytes +1 and +2, read
// again for every test.
const unsigned char* Watched() { return reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(UL(S() + 0xC))); }
bool WatchedIs(unsigned char a, unsigned char b) {
    const unsigned char* const r = Watched();
    return r[1] == a && r[2] == b;
}
void Sub64Plain() { SH_CALL(EffectKind18Sub64_Draw)(0x80, 0x40, 1, 0); }
}  // namespace

// original 0x5164A0 (sub-state 0): the variant v the spawn's x cell names: its
// cell (0x65F578) << 16 into +0x34 / +0x38, +0xC the enemy record v + 1
// (0x93BA88 + 0x128 v), +9 and +0xA 0, +2 up. The draw (0x80, 0x40, 1, 0).
extern "C" void __cdecl EffectKind18Sub64_Place(void) {
    unsigned char* const s = S();
    const U v = Variant("EffectKind18Sub64_Place", s, at::kSub64Variants);
    SetUL(s + 0x34, static_cast<U>(At(at::kSub64Cells + 2 * v)[0]) << 16);
    SetUL(s + 0x38, static_cast<U>(At(at::kSub64Cells + 2 * v + 1)[0]) << 16);
    SetUL(s + 0xC, at::kEnemyRecords + at::kEnemyStride * v);
    s[9] = 0;
    s[0xA] = 0;
    s[2] = static_cast<unsigned char>(s[2] + 1);
    Sub64Plain();
}

// original 0x516520 (sub-state 1): Game_Mode and Game_Step both 5: +2 up. The
// draw.
extern "C" void __cdecl EffectKind18Sub64_WaitBattle(void) {
    if (Game_Mode == 5 && Game_Step == 5) StepUp();
    Sub64Plain();
}

// original 0x516560 (sub-state 2): Game_Mode 5 and Game_Step not: +2 down. Then
// by the watched record's bytes +1 / +2: (8, 2) +2 = 4; (6, 1) and (6, 2) 6;
// (6, 3) 5; (6, 4) 3 - each test after the last's write. (3, 1): the draw at
// scale ((0x904AC8 * 6 + 0x3F) & 0xF8); else the plain draw.
extern "C" void __cdecl EffectKind18Sub64_Watch(void) {
    if (Game_Mode == 5 && Game_Step != 5) S()[2] = static_cast<unsigned char>(S()[2] - 1);
    if (WatchedIs(8, 2)) S()[2] = 4;
    if (WatchedIs(6, 1)) S()[2] = 6;
    if (WatchedIs(6, 2)) S()[2] = 6;
    if (WatchedIs(6, 3)) S()[2] = 5;
    if (WatchedIs(6, 4)) S()[2] = 3;
    if (WatchedIs(3, 1)) {
        const U scale = (UL(at::kBattleWord) * 6 + 0x3Fu) & 0xF8u;
        SH_CALL(EffectKind18Sub64_Draw)(scale, 0x40, 1, 0);
        return;
    }
    Sub64Plain();
}

// original 0x516640 (sub-state 3): +9 up 2; the draw at (2a, a, 1, 0), a = 0x40 -
// +9; past 0x3F a tail jump to Effect_Release.
extern "C" void __cdecl EffectKind18Sub64_Fade(void) {
    S()[9] = static_cast<unsigned char>(S()[9] + 2);
    const U a = 0x40u - S()[9];
    SH_CALL(EffectKind18Sub64_Draw)(a + a, I(a), 1, 0);
    if (S()[9] > 0x3F) SH_CALL(Effect_Release)();
}

// original 0x516690 (sub-state 4): unless the watched record is (8, 2), +2 = 2.
// The draw at (0xE0, 0x70, 2, 0).
extern "C" void __cdecl EffectKind18Sub64_Hold(void) {
    unsigned char* const s = S();
    if (!WatchedIs(8, 2)) s[2] = 2;
    SH_CALL(EffectKind18Sub64_Draw)(0xE0, 0x70, 2, 0);
}

// original 0x5166C0 (sub-state 5, and sub-state 6's tail jump; no list held it):
// +9 up; e = 8 - |8 - +9|; past 0xF +9 0 and +2 = 2. The draw lifted by -e <<
// 4.
extern "C" void __cdecl EffectKind18Sub64_Pulse(void) {
    S()[9] = static_cast<unsigned char>(S()[9] + 1);
    unsigned char* const s = S();
    const U c = s[9];
    const std::int32_t e = 8 - Abs(8u - c);
    if (c > 0xF) {
        s[9] = 0;
        S()[2] = 2;
    }
    SH_CALL(EffectKind18Sub64_Draw)(0x80, 0x40, 1, I(static_cast<U>(-e) << 4));
}

// original 0x516730 (sub-state 6): the watched record (6, 3): +9 0, +2 = 5 and a
// tail jump to the pulse. Else +9 up 8; the draw at (|0x40 - +9| + 0x40, |0x20 -
// +9 / 2| + 0x20, 1, 0), and past 0x7F +9 0 and +2 = 2 first.
extern "C" void __cdecl EffectKind18Sub64_Swell(void) {
    if (WatchedIs(6, 3)) {
        S()[9] = 0;
        S()[2] = 5;
        SH_CALL(EffectKind18Sub64_Pulse)();
        return;
    }
    S()[9] = static_cast<unsigned char>(S()[9] + 8);
    unsigned char* const s = S();
    const U b = s[9];
    const std::int32_t scale = Abs(0x40u - b) + 0x40;
    const std::int32_t width = Abs(0x20u - (b >> 1)) + 0x20;
    if (b > 0x7F) {
        s[9] = 0;
        S()[2] = 2;
    }
    SH_CALL(EffectKind18Sub64_Draw)(static_cast<U>(scale), width, 1, 0);
}

// ===========================================================================
// Sub-kind 0x65: EffectKind18_States[0x65] (0x654200), EffectKind18Sub65_States
// (three) by +2
// ===========================================================================

// original 0x516A90 (cdecl (level); called by sub-kind 0x65's states 0 and 2):
// each of CLUT rows 4 and 5's 0x200 colours as loaded (Gfx_ClutStripSource +
// 0x800) with its three 5-bit channels times level / 128 (signed, toward zero)
// packed back unclamped (a channel past 31 runs into the next); 0 from a colour
// that was not becomes 0x8000; written to the live strip (Gfx_ClutStrip +
// 0x800). Gfx_ClutStripDirty 1.
extern "C" void __cdecl EffectKind18Sub65_ShadeCluts(int level) {
    const U l = static_cast<U>(level);
    for (U k = 0; k < at::kClutWords; ++k) {
        const U colour = Word(At(at::kClutRows + 2 * k));
        U out = static_cast<U>(I(((colour >> 10) & 0x1F) * l) / 128);
        out = (out << 5) | static_cast<U>(I(((colour >> 5) & 0x1F) * l) / 128);
        out = (out << 5) | static_cast<U>(I((colour & 0x1F) * l) / 128);
        if (out == 0 && colour != 0) out = 0x8000;
        SetWord(At(at::kClutRowsLive + 2 * k), out);
    }
    Gfx_ClutStripDirty = 1;
}

// original 0x5169F0 (hidden in 0x5167C0): jmp [EffectKind18Sub65_States + +2 *
// 4], unbounded.
extern "C" void __cdecl EffectKind18Sub65_Run(void) {
    Dispatch("EffectKind18Sub65_Run", AddressOf(EffectKind18Sub65_States), EffectKind18Sub65_States_count);
}

// original 0x516A10 (sub-state 0): the rows black (level 0), +2 up.
extern "C" void __cdecl EffectKind18Sub65_Start(void) {
    SH_CALL(EffectKind18Sub65_ShadeCluts)(0);
    StepUp();
}

// original 0x516A30 (sub-state 1): the counter byte 0x903848 0xD: +2 up.
extern "C" void __cdecl EffectKind18Sub65_Wait(void) {
    if (At(at::kCounter48)[0] == 0xD) StepUp();
}

// original 0x516A50 (sub-state 2): +9 up; the rows at level +9; from 0x80 a tail
// jump to Effect_Release.
extern "C" void __cdecl EffectKind18Sub65_FadeIn(void) {
    S()[9] = static_cast<unsigned char>(S()[9] + 1);
    SH_CALL(EffectKind18Sub65_ShadeCluts)(S()[9]);
    if (S()[9] >= 0x80) SH_CALL(Effect_Release)();
}

void Effect6D_Inject() {
    if (bof3::WantsShadow("effect_6d")) effect_6d::SelfTest();
    BOF3_INJECT(EffectKind18Sub5C_Run);
    BOF3_INJECT(EffectKind18Sub5C_Place);
    BOF3_INJECT(EffectKind18Sub5C_WaitNear);
    BOF3_INJECT(EffectKind18Sub5C_Open);
    BOF3_INJECT(EffectKind18Sub5C_WaitFar);
    BOF3_INJECT(EffectKind18Sub5C_Close);
    BOF3_INJECT(EffectKind18Sub5C_DrawPanel);
    BOF3_INJECT(EffectKind18Sub5E_Run);
    BOF3_INJECT(EffectKind18Sub5E_Start);
    BOF3_INJECT(EffectKind18Sub5E_WaitNear);
    BOF3_INJECT(EffectKind18Sub5E_Open);
    BOF3_INJECT(EffectKind18Sub5E_WaitFar);
    BOF3_INJECT(EffectKind18Sub5E_Close);
    BOF3_INJECT(EffectKind18Sub5E_Draw);
    BOF3_INJECT(EffectKind18Sub5D_Run);
    BOF3_INJECT(EffectKind18Sub5D_Place);
    BOF3_INJECT(EffectKind18Sub5D_WaitNear);
    BOF3_INJECT(EffectKind18Sub5D_Open);
    BOF3_INJECT(EffectKind18Sub5D_WaitFar);
    BOF3_INJECT(EffectKind18Sub5D_Close);
    BOF3_INJECT(EffectKind18Sub5D_Draw);
    BOF3_INJECT(EffectKind18Sub61_Run);
    BOF3_INJECT(EffectKind18Sub61_WaitFocus);
    BOF3_INJECT(EffectKind18Sub61_Slide);
    BOF3_INJECT(EffectKind18Sub61_Hold);
    BOF3_INJECT(EffectKind18Sub61_Rise);
    BOF3_INJECT(EffectKind18Sub61_Reset);
    BOF3_INJECT(EffectKind18Sub61_Rearm);
    BOF3_INJECT(EffectKind18Sub61_ClearTracks);
    BOF3_INJECT(EffectKind18Sub61_DrawOverlay);
    BOF3_INJECT(EffectKind18Sub61_DrawTracks);
    BOF3_INJECT(EffectKind18Sub61_DrawGlow);
    BOF3_INJECT(EffectKind18Sub61_DrawRing);
    BOF3_INJECT(EffectKind18Sub61_DrawDisc);
    BOF3_INJECT(EffectKind18Sub61_DrawMist);
    BOF3_INJECT(EffectKind18Sub62_Ripple);
    BOF3_INJECT(EffectKind18Sub63_Glow);
    BOF3_INJECT(EffectKind18Sub64_Run);
    BOF3_INJECT(EffectKind18Sub64_Place);
    BOF3_INJECT(EffectKind18Sub64_WaitBattle);
    BOF3_INJECT(EffectKind18Sub64_Watch);
    BOF3_INJECT(EffectKind18Sub64_Fade);
    BOF3_INJECT(EffectKind18Sub64_Hold);
    BOF3_INJECT(EffectKind18Sub64_Pulse);
    BOF3_INJECT(EffectKind18Sub64_Swell);
    BOF3_INJECT(EffectKind18Sub64_Draw);
    BOF3_INJECT(EffectKind18Sub65_Run);
    BOF3_INJECT(EffectKind18Sub65_Start);
    BOF3_INJECT(EffectKind18Sub65_Wait);
    BOF3_INJECT(EffectKind18Sub65_FadeIn);
    BOF3_INJECT(EffectKind18Sub65_ShadeCluts);
}

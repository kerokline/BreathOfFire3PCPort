// Round thirteen group E5G (docs/effect_5g.md): the 24 functions of
// analysis/round13_cut.tsv's group E5G, 0x50AF90..0x50C0CA, each read with
// capstone to its last instruction (2026-10-03). Effect_RunObjects (ours)
// makes each live record of Effect_Objects (20 of 0x80 bytes) Sprite_Current
// and calls Effect_KindHandlers[+5]; kind 0x18's EffectKind18_Run jumps through
// EffectKind18_States by +1 (the sub-kind, EffectKind18_Start copies it from
// +0xB). Four of the band's sub-kinds dispatch again by +2 through a table of
// their own (none bounded by a compare); the fifth is one state. What each is,
// as far as the code says:
//
//   sub-kind 0x2B   two textured panels (one quad each, mirrored) at a cell a
//                   variant table picks by the spawn's x cell: when the leader
//                   stands at the cell's front they slide apart over twelve
//                   frames (sound 0x200), hide while the leader stays, slide
//                   back once the leader is two cells away (sound 0x201)
//   sub-kind 0x2C   the same with panels sliding along x or z (the spawn's z
//                   cell 0 chooses), drawn always, linked into the map's depth
//                   order at the record; eight frames each way
//   sub-kind 0x36   a semi-transparent blue fill over the frame (additive), its
//                   blue stepped through four levels every eight frames - one of
//                   DIV-0041's full-frame fills (below)
//   sub-kind 0x3A   two panels at the record's cell that slide apart while any
//                   party member stands at them (sound 0x202) and back once none
//                   does (sound 0x203)
//   sub-kind 0x4A   sub-kind 0x2C's panels placed with two map bytes written
//                   under them (AreaMap_SetByte); its state 1 is E6A's
//
// Every call goes through the harness (SH_CALL), so the start-up fuzz can stand
// recorders in for ours as for the originals' copies. Sprite_Current is read
// again wherever the original reads [0x937F88] again after a call. Sub-kind
// 0x36's fill is DIV-0041's (section 3c): its corners are (Widescreen_FillX(),
// 0) .. (320 + Widescreen_Fill(), 240), which is the original's (0, 0) ..
// (320, 240) until Widescreen_ArmFills has run and whenever the picture is
// narrow. Otherwise no divergence: each is a faithful replacement. Where the
// original jumps through a sub-state table past its end or indexes a variant
// or step table past its room, ours aborts with a message (docs/effect_5g.md
// section 6).
#include "game/effect_5g.h"

#include <bit>
#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/effect_5g_callees.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_5g::at;
using U = std::uint32_t;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using Handler = scenario_harness::Handler;

unsigned char* S() { return Sprite_Current; }
U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }
void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }
std::int32_t S16(const unsigned char* p) { return static_cast<std::int16_t>(Word(p)); }
U AddressOf(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
unsigned char* Vertices() { return reinterpret_cast<unsigned char*>(Prim_VertexScratch); }
const short* Vertex(unsigned i) { return reinterpret_cast<const short*>(Vertices() + 8 * i); }

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 2]; jmp [table + eax
// * 4]: the table's `entries` handlers read in place (the fuzz swaps the cells
// for recorders); a Fatal past them, where the original jumps through the dword
// after - another table or data.
void Dispatch(const char* who, U table, unsigned entries) {
    const unsigned sub = Sprite_Current[2];
    if (sub >= entries)
        bof3::Fatal("%s: sub-state byte +2 is %u, past the %u entries of 0x%X - the original jumps through the dword "
                    "after (docs/effect_5g.md section 6)",
                    who, sub, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(At(table + 4 * sub))))();
}

// The variant the spawn's x cell (+0x36, movsx) names, checked against the
// room its tables have.
U Variant(const char* who, const unsigned char* s, unsigned room) {
    const std::int32_t v = S16(s + 0x36);
    if (v < 0 || v >= static_cast<std::int32_t>(room))
        bof3::Fatal("%s: the variant +0x36 is %d, past the %u its tables have room for - the original reads on into "
                    "the next table (docs/effect_5g.md section 6)",
                    who, (int)v, room);
    return static_cast<U>(v);
}

// cdq; xor eax, edx; sub eax, edx; cmp eax, n; jg: |d| as the original
// computes it - 0x80000000 stays itself, negative, and so counts as near.
std::int32_t Abs(U d) {
    const U m = static_cast<std::int32_t>(d) < 0 ? 0xFFFFFFFFu : 0u;
    return static_cast<std::int32_t>((d ^ m) - m);
}
// A point against a cell's centre on one axis: point - ((cell + 1) << 16 |
// 0x8000) (inc; shl 16; or dh, 0x80).
U FromCentre(U point, std::int32_t cell) { return point - ((static_cast<U>(cell + 1) << 16) | 0x8000u); }
// ... and against its edge: point - (cell << 16).
U FromEdge(U point, std::int32_t cell) { return point - (static_cast<U>(cell) << 16); }
// The tests every waiting state makes: |a| within `first`, then |b| or |b -
// 0x10000| within `second` (the cell or the one past it).
bool Within(U a, std::int32_t first, U b, std::int32_t second) {
    if (Abs(a) > first) return false;
    if (Abs(b) <= second) return true;
    return Abs(b - 0x10000u) <= second;
}
U LeaderX() { return UL(At(at::kLeaderX)); }
U LeaderZ() { return UL(At(at::kLeaderZ)); }
// The leader at the record's cell: across the x axis first (the centre), then
// z - or, for +8 set (sub-kinds 0x2C / 0x4A), z first, then x.
bool LeaderAt(const unsigned char* s, bool z_first, std::int32_t first, std::int32_t second) {
    const std::int32_t cx = S16(s + 0x36), cz = S16(s + 0x3A);
    if (z_first) return Within(FromCentre(LeaderZ(), cz), first, FromEdge(LeaderX(), cx), second);
    return Within(FromCentre(LeaderX(), cx), first, FromEdge(LeaderZ(), cz), second);
}
// Any of the three party records in use (+0 set) at the record's cell: z
// against the edge within `first`, then x (sub-kind 0x3A). Every record is
// tested; none ends the loop early.
bool PartyAt(const unsigned char* s, std::int32_t first, std::int32_t second) {
    bool any = false;
    for (unsigned m = 0; m < at::kTrioCount; ++m) {
        const unsigned char* const r = ObjTrio + at::kTrioStride * m;
        if (r[0] == 0) continue;
        if (Within(FromEdge(UL(r + 0x38), S16(s + 0x3A)), first, FromEdge(UL(r + 0x34), S16(s + 0x36)), second))
            any = true;
    }
    return any;
}

// A vertex's ground point: (s16 + 0x4000) << 9, the 16.16 the map reads.
long Ground(const unsigned char* word) { return static_cast<long>(static_cast<U>(S16(word) + 0x4000) << 9); }
// -(height / 2) - +0x3E, the low word (movsx; cdq; sub; sar 1; neg; sub ax).
U Lift(long height, const unsigned char* s) {
    const std::int32_t half = static_cast<std::int16_t>(static_cast<U>(height) & 0xFFFFu) / 2;
    return 0u - static_cast<U>(half) - Word(s + 0x3E);
}
// The four corners' heights from the ground under them, as every panel draw
// lifts them: corners 0 and 2 at vertex 0's point (two reads, the top 0x180
// above), corners 1 and 3 at vertex 1's, Sprite_Current read after each call.
void Grounded() {
    unsigned char* const v = Vertices();
    const long x0 = Ground(v), y0 = Ground(v + 2);
    long h = SH_CALL(AreaMap_Elevation)(x0, y0);
    SetWord(v + 4, Lift(h, S()) - 0x180u);
    h = SH_CALL(AreaMap_Elevation)(x0, y0);
    const long x1 = Ground(v + 8), y1 = Ground(v + 0xA);
    SetWord(v + 0x14, Lift(h, S()));
    h = SH_CALL(AreaMap_Elevation)(x1, y1);
    SetWord(v + 0xC, Lift(h, S()) - 0x180u);
    h = SH_CALL(AreaMap_Elevation)(x1, y1);
    SetWord(v + 0x1C, Lift(h, S()));
}
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
// The draw mode the panels put first: Gpu_SetDrawMode(prim, 0, 0, 0x95, 0).
void PanelMode(unsigned char* prim) { SH_CALL(Gpu_SetDrawMode)(prim, 0, 0, 0x95, 0); }

// The variant's cell, height and texture as sub-kinds 0x2C and 0x4A place it
// (0x50B540 / 0x50BFF0 alike): +8 = whether the spawn's z cell is 0, the cell
// from the table, the lift at the cell's ground into +0x3E, the texture into
// +0x20, the slide +0x30 0. Answers the variant.
U Place2C(const char* who) {
    unsigned char* s = S();
    s[8] = Word(s + 0x3A) == 0 ? 1 : 0;
    const U v = Variant(who, s, at::kSub2CVariants);
    SetWord(s + 0x36, At(at::kSub2CCells + 2 * v)[0]);
    SetWord(s + 0x3A, At(at::kSub2CCells + 2 * v + 1)[0]);
    const long h = SH_CALL(AreaMap_Elevation)(Long(s + 0x34), Long(s + 0x38));
    const std::int32_t half = static_cast<std::int16_t>(static_cast<U>(h) & 0xFFFFu) / 2;
    s = S();
    SetWord(s + 0x3E, (0u - static_cast<U>(half)) - Word(At(at::kSub2CHeights + 2 * v)));
    SetUL(S() + 0x20, UL(At(at::kSub2CTextures + 4 * v)));
    SetWord(S() + 0x30, 0);
    return v;
}

// A slide of +0x30 by `by`; answers whether it has reached `end` (>= when
// opening, <= 0 when closing; signed words).
bool Opened(std::int32_t end) {
    unsigned char* const s = S();
    SetWord(s + 0x30, Word(s + 0x30) + 0x10u);
    return S16(S() + 0x30) >= end;
}
bool Closed() {
    unsigned char* const s = S();
    SetWord(s + 0x30, Word(s + 0x30) - 0x10u);
    return S16(S() + 0x30) <= 0;
}
// Closed: the sound unless a message is up (Field_Request), +2 = 1 (read after
// the sound).
void Shut(unsigned sound) {
    if (Field_Request == 0) SH_CALL(Sound_PlayEffect)(static_cast<unsigned short>(sound));
    S()[2] = 1;
}

}  // namespace

// ===========================================================================
// The panel draws
// ===========================================================================

// original 0x50B220 (sub-kind 0x2B's draw; called by its five states but the
// fourth): the draw mode (page 0x95) committed at slot 6 (0xC); two textured
// quads, i = 0 and 1, one cell (0x100) wide at x (+0x36 << 7) - 0x3FC0 (the
// four corners alike), y (+0x3A << 7) - 0x3F40 / - 0x4040 plus the slide +0x30
// times the side's sign (kSub2BSides), every word the low sixteen bits. With
// `follow` the corners' heights come from the ground (Grounded); without, flat
// at +0x32 (the top 0x180 above). Projected, the texture +0xC | (0x130 - i) |
// 0x1690000, committed at slot 6 (0x48).
extern "C" void __cdecl EffectKind18Sub2B_Draw(int follow) {
    unsigned char* const v = Vertices();
    unsigned char* const mode = Gfx_PacketNext;
    const U x = (static_cast<U>(Word(S() + 0x36)) << 7) - 0x3FC0u;
    SetWord(v + 0x18, x);
    SetWord(v + 0x10, x);
    SetWord(v + 8, x);
    SetWord(v, x);
    PanelMode(mode);
    SH_CALL(Gfx_CommitPrim)(6, 0xC);
    U texture = 0x130;
    for (unsigned i = 0; i < 2; ++i, --texture) {
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyFT4)(p);
        SH_CALL(Gpu_SetShadeTex)(p, 0);
        unsigned char* const s = S();
        const U sign = static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(At(at::kSub2BSides)[i])));
        const U slide = Word(s + 0x30) * sign;
        const U z = static_cast<U>(Word(s + 0x3A)) << 7;
        SetWord(v + 0x12, slide + z - 0x3F40u);
        SetWord(v + 2, slide + z - 0x3F40u);
        SetWord(v + 0x1A, slide + z - 0x4040u);
        SetWord(v + 0xA, slide + z - 0x4040u);
        if (follow != 0) {
            Grounded();
        } else {
            SetWord(v + 0xC, Word(s + 0x32) - 0x180u);
            SetWord(v + 4, Word(s + 0x32) - 0x180u);
            SetWord(v + 0x1C, Word(s + 0x32));
            SetWord(v + 0x14, Word(s + 0x32));
        }
        Project(p);
        SH_CALL(Prim_SetTexture)(UL(S() + 0xC) | texture | 0x1690000u, p, 1);
        SH_CALL(Gfx_CommitPrim)(6, 0x48);
    }
}

// original 0x50B8B0 (sub-kinds 0x2C's and 0x4A's draw: called by 0x2C's states
// and 0x4A's first, a tail jump from 0x2C's third and E6A's 0x50C0D0, and
// EffectKind18Sub4A_States' fourth entry itself): with +8 the panels lie across
// z - their y (+0x3A << 7) - 0x3FC0 - and slide along x; without, across x and
// slide along z. Two quads, side 0 and 1: a draw mode (page 0x95) linked at
// the record's point (+0x34, +0x38) with dy -1 (0xC); the corners slid by
// +0x30 times the side's sign (kSub2CSides), their heights from the ground;
// projected, the texture (+0x20 + side) | (+8 << 21), linked likewise (0x48).
extern "C" void __cdecl EffectKind18Sub2C_Draw(void) {
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
        SetWord(v + 0xC, Word(s + 0x3E) - 0x180u);
        SetWord(v + 4, Word(s + 0x3E) - 0x180u);
        SetWord(v + 0x1C, Word(s + 0x3E));
        SetWord(v + 0x14, Word(s + 0x3E));
    }
    for (U side = 0; side < 2; ++side) {
        PanelMode(Gfx_PacketNext);
        {
            unsigned char* const s = S();
            SH_CALL(MapView_LinkPrimAt)(UL(s + 0x34), UL(s + 0x38), -1, 0xC);
        }
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyFT4)(p);
        SH_CALL(Gpu_SetShadeTex)(p, 0);
        unsigned char* const s = S();
        const U sign = static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(At(at::kSub2CSides)[side])));
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
        Grounded();
        Project(p);
        {
            unsigned char* const r = S();
            SH_CALL(Prim_SetTexture)((UL(r + 0x20) + side) | (static_cast<U>(r[8]) << 21), p, 1);
        }
        unsigned char* const r = S();
        SH_CALL(MapView_LinkPrimAt)(UL(r + 0x34), UL(r + 0x38), -1, 0x48);
    }
}

// original 0x50BDC0 (sub-kind 0x3A's draw, called twice by each of its states:
// side 0 and 1): one quad across x at side's column - x ((side + +0x36) << 7)
// minus the slide +0x30 times kSub3ASides[side], - 0x4040 / - 0x3FC0; y ((+0x3A
// - 0x80) << 7) - after a draw mode (page 0x95) linked at the record's point
// with `dy` (0xC); the heights from the ground; projected, the texture +0x20 +
// side, linked with `dy` (0x48).
extern "C" void __cdecl EffectKind18Sub3A_DrawPanel(unsigned side, int dy) {
    unsigned char* const v = Vertices();
    {
        unsigned char* const s = S();
        if (side >= at::kSidesRoom)
            bof3::Fatal("EffectKind18Sub3A_DrawPanel: side %u, past the %u bytes of 0x%X - the original reads on into "
                        "EffectKind18Sub4A_States (docs/effect_5g.md section 6)",
                        side, at::kSidesRoom, (unsigned)at::kSub3ASides);
        const U sign = static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(At(at::kSub3ASides)[side])));
        const U slide = sign * Word(s + 0x30);
        const U x = (side + Word(s + 0x36)) << 7;
        SetWord(v + 0x10, x - slide - 0x4040u);
        SetWord(v, x - slide - 0x4040u);
        SetWord(v + 0x18, x - slide - 0x3FC0u);
        SetWord(v + 8, x - slide - 0x3FC0u);
        const U y = (Word(s + 0x3A) - 0x80u) << 7;
        SetWord(v + 0x1A, y);
        SetWord(v + 0x12, y);
        SetWord(v + 0xA, y);
        SetWord(v + 2, y);
    }
    PanelMode(Gfx_PacketNext);
    {
        unsigned char* const s = S();
        SH_CALL(MapView_LinkPrimAt)(UL(s + 0x34), UL(s + 0x38), dy, 0xC);
    }
    Grounded();
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyFT4)(p);
    SH_CALL(Gpu_SetShadeTex)(p, 0);
    Project(p);
    SH_CALL(Prim_SetTexture)(UL(S() + 0x20) + side, p, 1);
    unsigned char* const s = S();
    SH_CALL(MapView_LinkPrimAt)(UL(s + 0x34), UL(s + 0x38), dy, 0x48);
}

// ===========================================================================
// Sub-kind 0x2B: EffectKind18_States[0x2B] (0x654118), EffectKind18Sub2B_States
// (five) by +2
// ===========================================================================

// original 0x50AF90 (hidden in E5F's 0x50AD70): jmp [EffectKind18Sub2B_States +
// +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub2B_Run(void) {
    Dispatch("EffectKind18Sub2B_Run", AddressOf(EffectKind18Sub2B_States), EffectKind18Sub2B_States_count);
}

// original 0x50AFB0 (sub-state 0): the variant the spawn's x cell names: the
// cell into +0x36 / +0x3A, the lift -height - ground / 2 into +0x3E (the ground
// at the record's point, read after the cell is written), the height into
// +0x32, the texture byte << 24 into +0xC, the slide 0, +2 up. The leader
// already at the cell's front: the slide 0xC0 and +2 = 3. Then the draw on the
// ground.
extern "C" void __cdecl EffectKind18Sub2B_Place(void) {
    unsigned char* s = S();
    const U v = Variant("EffectKind18Sub2B_Place", s, at::kSub2BVariants);
    SetWord(s + 0x36, At(at::kSub2BCells + 2 * v)[0]);
    SetWord(s + 0x3A, At(at::kSub2BCells + 2 * v + 1)[0]);
    const long h = SH_CALL(AreaMap_Elevation)(Long(s + 0x34), Long(s + 0x38));
    const U height = Word(At(at::kSub2BHeights + 2 * v));
    const std::int32_t half = static_cast<std::int16_t>(static_cast<U>(h) & 0xFFFFu) / 2;
    s = S();
    SetWord(s + 0x3E, (0u - height) - static_cast<U>(half));
    SetWord(s + 0x32, height);
    SetUL(s + 0xC, static_cast<U>(At(at::kSub2BTextures + v)[0]) << 24);
    SetWord(s + 0x30, 0);
    s[2] = static_cast<unsigned char>(s[2] + 1);
    if (LeaderAt(s, false, 0x8000, 0x10000)) {
        SetWord(s + 0x30, 0xC0);
        s[2] = 3;
    }
    SH_CALL(EffectKind18Sub2B_Draw)(1);
}

// original 0x50B0C0 (sub-state 1): the leader at the cell's front (within half
// a cell of the centre across x, on the cell or the next along z): sound 0x200
// unless a message is up, +2 up (Sprite_Current read after the sound). The draw
// on the ground.
extern "C" void __cdecl EffectKind18Sub2B_WaitNear(void) {
    unsigned char* s = S();
    if (LeaderAt(s, false, 0x8000, 0x10000)) {
        if (Field_Request == 0) {
            SH_CALL(Sound_PlayEffect)(at::kSoundOpen);
            s = S();
        }
        s[2] = static_cast<unsigned char>(s[2] + 1);
    }
    SH_CALL(EffectKind18Sub2B_Draw)(1);
}

// original 0x50B150 (sub-state 2): the slide up 0x10; at 0xC0 +2 up. The draw
// flat.
extern "C" void __cdecl EffectKind18Sub2B_Open(void) {
    if (Opened(0xC0)) S()[2] = static_cast<unsigned char>(S()[2] + 1);
    SH_CALL(EffectKind18Sub2B_Draw)(0);
}

// original 0x50B180 (sub-state 3): once the leader is two cells away (the same
// test at 0x20000 both ways fails), +2 up. Nothing drawn.
extern "C" void __cdecl EffectKind18Sub2B_WaitFar(void) {
    unsigned char* const s = S();
    if (!LeaderAt(s, false, 0x20000, 0x20000)) s[2] = static_cast<unsigned char>(s[2] + 1);
}

// original 0x50B1E0 (sub-state 4): the slide down 0x10; at 0 or below sound
// 0x201 unless a message is up and +2 = 1. The draw flat.
extern "C" void __cdecl EffectKind18Sub2B_Close(void) {
    if (Closed()) Shut(at::kSoundShut);
    SH_CALL(EffectKind18Sub2B_Draw)(0);
}

// ===========================================================================
// Sub-kind 0x36: EffectKind18_States[0x36] (0x654144) - one state
// ===========================================================================

// original 0x50B480 (hidden in 0x50B220): a draw mode (page 0xB5, additive)
// committed at slot 3 (0xC); a semi-transparent POLY_F4 over the frame -
// DIV-0041's fill, (Widescreen_FillX(), 0)..(320 + Widescreen_Fill(), 240), the
// original's (0, 0)..(320, 240) narrow or unarmed - coloured (0, 0x30,
// kSub36Blues[+2] << 3), committed at slot 3 (0x38); every eighth frame
// (Frame_Counter & 7, read after the commit) +2 = (+2 + 1) & 3.
extern "C" void __cdecl EffectKind18Sub36_Pulse(void) {
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0xB5, 0);
    SH_CALL(Gfx_CommitPrim)(3, 0xC);
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyF4)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, 1);
    const U left = std::bit_cast<U>(Widescreen_FillX());                                 // DIV-0041: -53 wide, 0 narrow
    const U right = std::bit_cast<U>(320.0f + static_cast<float>(Widescreen_Fill()));    // 0x43A00000, 320.0f narrow
    SetUL(p + 8, left);
    SetUL(p + 0xC, 0);
    SetUL(p + 0x14, right);
    SetUL(p + 0x18, 0);
    SetUL(p + 0x20, left);
    SetUL(p + 0x24, 0x43700000u);   // 240.0f
    SetUL(p + 0x2C, right);
    SetUL(p + 0x30, 0x43700000u);
    p[4] = 0;
    p[5] = 0x30;
    const unsigned step = S()[2];
    if (step >= at::kSub36Steps)
        bof3::Fatal("EffectKind18Sub36_Pulse: +2 is %u, past the %u steps of 0x%X - the original reads on into "
                    "sub-kind 0x2C's heights (docs/effect_5g.md section 6)",
                    step, at::kSub36Steps, (unsigned)at::kSub36Blues);
    p[6] = static_cast<unsigned char>(At(at::kSub36Blues)[step] << 3);
    SH_CALL(Gfx_CommitPrim)(3, 0x38);
    if ((Frame_Counter & 7) == 0) {
        unsigned char* const s = S();
        s[2] = static_cast<unsigned char>((s[2] + 1) & 3);
    }
}

// ===========================================================================
// Sub-kind 0x2C: EffectKind18_States[0x2C] (0x65411C), EffectKind18Sub2C_States
// (five) by +2
// ===========================================================================

// original 0x50B520 (hidden in 0x50B220): jmp [EffectKind18Sub2C_States + +2 *
// 4], unbounded.
extern "C" void __cdecl EffectKind18Sub2C_Run(void) {
    Dispatch("EffectKind18Sub2C_Run", AddressOf(EffectKind18Sub2C_States), EffectKind18Sub2C_States_count);
}

// original 0x50B540 (sub-state 0): Place2C, +2 up; the leader already at the
// cell's front (the axis +8 names first): the slide 0x80 and +2 = 3. The draw.
extern "C" void __cdecl EffectKind18Sub2C_Place(void) {
    Place2C("EffectKind18Sub2C_Place");
    unsigned char* const s = S();
    s[2] = static_cast<unsigned char>(s[2] + 1);
    if (LeaderAt(s, s[8] != 0, 0x8000, 0x10000)) {
        SetWord(s + 0x30, 0x80);
        s[2] = 3;
    }
    SH_CALL(EffectKind18Sub2C_Draw)();
}

// original 0x50B6A0 (sub-state 1): the leader at the cell's front: sound 0x200
// unless a message is up, +2 up (read after the sound). The draw.
extern "C" void __cdecl EffectKind18Sub2C_WaitNear(void) {
    unsigned char* s = S();
    if (LeaderAt(s, s[8] != 0, 0x8000, 0x10000)) {
        if (Field_Request == 0) {
            SH_CALL(Sound_PlayEffect)(at::kSoundOpen);
            s = S();
        }
        s[2] = static_cast<unsigned char>(s[2] + 1);
    }
    SH_CALL(EffectKind18Sub2C_Draw)();
}

// original 0x50B780 (sub-state 2, and EffectKind18Sub4A_States' third): the
// slide up 0x10; at 0x80 +2 up. A tail jump to the draw.
extern "C" void __cdecl EffectKind18Sub2C_Open(void) {
    if (Opened(0x80)) S()[2] = static_cast<unsigned char>(S()[2] + 1);
    SH_CALL(EffectKind18Sub2C_Draw)();
}

// original 0x50B7A0 (sub-state 3): once the leader is two cells away, +2 up.
// The draw.
extern "C" void __cdecl EffectKind18Sub2C_WaitFar(void) {
    unsigned char* const s = S();
    if (!LeaderAt(s, s[8] != 0, 0x20000, 0x20000)) s[2] = static_cast<unsigned char>(s[2] + 1);
    SH_CALL(EffectKind18Sub2C_Draw)();
}

// original 0x50B870 (sub-state 4): the slide down 0x10; at 0 or below sound
// 0x201 unless a message is up and +2 = 1. A tail jump to the draw.
extern "C" void __cdecl EffectKind18Sub2C_Close(void) {
    if (Closed()) Shut(at::kSoundShut);
    SH_CALL(EffectKind18Sub2C_Draw)();
}

// ===========================================================================
// Sub-kind 0x3A: EffectKind18_States[0x3A] (0x654154), EffectKind18Sub3A_States
// (five) by +2
// ===========================================================================

// original 0x50BB90 (hidden in 0x50B8B0): jmp [EffectKind18Sub3A_States + +2 *
// 4], unbounded.
extern "C" void __cdecl EffectKind18Sub3A_Run(void) {
    Dispatch("EffectKind18Sub3A_Run", AddressOf(EffectKind18Sub3A_States), EffectKind18Sub3A_States_count);
}

// The pair as the closed and opening states draw it: side 0 at dy 1, side 1 at
// dy 2; as the open ones: dy 0 and 3.
void Sub3APair(int dy0, int dy1) {
    SH_CALL(EffectKind18Sub3A_DrawPanel)(0, dy0);
    SH_CALL(EffectKind18Sub3A_DrawPanel)(1, dy1);
}

// original 0x50BBB0 (sub-state 0): +8 = 1, the lift +0x3E 0, the texture
// 0x28709126 into +0x20, the slide 0, +2 up. The pair at dy 1 and 2.
extern "C" void __cdecl EffectKind18Sub3A_Start(void) {
    S()[8] = 1;
    SetWord(S() + 0x3E, 0);
    SetUL(S() + 0x20, 0x28709126u);
    SetWord(S() + 0x30, 0);
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
    Sub3APair(1, 2);
}

// original 0x50BC00 (sub-state 1): any party member in use within 1.5 cells of
// the cell's edge along z and on the cell or the next along x: sound 0x202
// unless a message is up, +2 up (read after the sound). The pair at dy 1, 2.
extern "C" void __cdecl EffectKind18Sub3A_WaitParty(void) {
    unsigned char* s = S();
    if (PartyAt(s, 0x18000, 0x10000)) {
        if (Field_Request == 0) {
            SH_CALL(Sound_PlayEffect)(at::kSoundOpen3A);
            s = S();
        }
        s[2] = static_cast<unsigned char>(s[2] + 1);
    }
    Sub3APair(1, 2);
}

// original 0x50BCB0 (sub-state 2): the slide up 0x10; at 0x80 +2 up. The pair
// at dy 0 and 3.
extern "C" void __cdecl EffectKind18Sub3A_Open(void) {
    if (Opened(0x80)) S()[2] = static_cast<unsigned char>(S()[2] + 1);
    Sub3APair(0, 3);
}

// original 0x50BCE0 (sub-state 3): no party member within 2.5 cells along z and
// two along x: +2 up. The pair at dy 0 and 3.
extern "C" void __cdecl EffectKind18Sub3A_WaitClear(void) {
    unsigned char* const s = S();
    if (!PartyAt(s, 0x28000, 0x20000)) s[2] = static_cast<unsigned char>(s[2] + 1);
    Sub3APair(0, 3);
}

// original 0x50BD70 (sub-state 4): the slide down 0x10; at 0 or below sound
// 0x203 unless a message is up and +2 = 1. The pair at dy 1 and 2.
extern "C" void __cdecl EffectKind18Sub3A_Close(void) {
    if (Closed()) Shut(at::kSoundShut3A);
    Sub3APair(1, 2);
}

// ===========================================================================
// Sub-kind 0x4A: EffectKind18_States[0x4A] (0x654194), EffectKind18Sub4A_States
// (four) by +2: this state, E6A's 0x50C0D0, EffectKind18Sub2C_Open, the draw
// ===========================================================================

// original 0x50BFD0 (hidden in 0x50BDC0): jmp [EffectKind18Sub4A_States + +2 *
// 4], unbounded.
extern "C" void __cdecl EffectKind18Sub4A_Run(void) {
    Dispatch("EffectKind18Sub4A_Run", AddressOf(EffectKind18Sub4A_States), EffectKind18Sub4A_States_count);
}

// original 0x50BFF0 (sub-state 0): Place2C; +0xB the variant's byte of
// kSub4ASubKinds; the variant's map byte written at the cell and the cell past
// it along z (AreaMap_SetByte(x, z, b), AreaMap_SetByte(x, z + 1, b), each
// read afresh), +2 up. The draw.
extern "C" void __cdecl EffectKind18Sub4A_Place(void) {
    const U v = Place2C("EffectKind18Sub4A_Place");
    S()[0xB] = At(at::kSub4ASubKinds + 4 * v)[0];
    const unsigned char* const mark = At(at::kSub4AMarks + 4 * v);
    {
        unsigned char* const s = S();
        SH_CALL(AreaMap_SetByte)(Word(s + 0x36), Word(s + 0x3A), mark[0]);
    }
    {
        unsigned char* const s = S();
        SH_CALL(AreaMap_SetByte)(Word(s + 0x36), (Word(s + 0x3A) + 1u) & 0xFFFFu, mark[0]);
    }
    unsigned char* const s = S();
    s[2] = static_cast<unsigned char>(s[2] + 1);
    SH_CALL(EffectKind18Sub2C_Draw)();
}

void Effect5G_Inject() {
    if (bof3::WantsShadow("effect_5g")) effect_5g::SelfTest();
    BOF3_INJECT(EffectKind18Sub2B_Run);
    BOF3_INJECT(EffectKind18Sub2B_Place);
    BOF3_INJECT(EffectKind18Sub2B_WaitNear);
    BOF3_INJECT(EffectKind18Sub2B_Open);
    BOF3_INJECT(EffectKind18Sub2B_WaitFar);
    BOF3_INJECT(EffectKind18Sub2B_Close);
    BOF3_INJECT(EffectKind18Sub2B_Draw);
    BOF3_INJECT(EffectKind18Sub36_Pulse);
    BOF3_INJECT(EffectKind18Sub2C_Run);
    BOF3_INJECT(EffectKind18Sub2C_Place);
    BOF3_INJECT(EffectKind18Sub2C_WaitNear);
    BOF3_INJECT(EffectKind18Sub2C_Open);
    BOF3_INJECT(EffectKind18Sub2C_WaitFar);
    BOF3_INJECT(EffectKind18Sub2C_Close);
    BOF3_INJECT(EffectKind18Sub2C_Draw);
    BOF3_INJECT(EffectKind18Sub3A_Run);
    BOF3_INJECT(EffectKind18Sub3A_Start);
    BOF3_INJECT(EffectKind18Sub3A_WaitParty);
    BOF3_INJECT(EffectKind18Sub3A_Open);
    BOF3_INJECT(EffectKind18Sub3A_WaitClear);
    BOF3_INJECT(EffectKind18Sub3A_Close);
    BOF3_INJECT(EffectKind18Sub3A_DrawPanel);
    BOF3_INJECT(EffectKind18Sub4A_Run);
    BOF3_INJECT(EffectKind18Sub4A_Place);
}

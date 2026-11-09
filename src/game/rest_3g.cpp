// Round fourteen group R3G (docs/rest_3g.md): the 32 functions of
// analysis/round14_cut.tsv's group R3G, 0x4925C0..0x5171FB, each read with
// capstone to its last instruction (2026-10-04). A mixed band - what round
// thirteen's effect groups left and the catalog's top-level rows between them:
//
//   kinds 0xAC, 0xAD, 0xAE, 0xBA   effect states the effect groups' tables
//                   point at (E4F's EffectKindAC/AD/AE/BA_States)
//   Screen_TriangleWinding   the z of a screen triangle's cross product
//   Battle_PlaceBossActors / _PlaceBossActor, BattleEnemy_ClearStates
//                   the battle's boss actors copied into the enemy records
//   GameMode8..11_*  four game modes' step dispatchers and steps
//   Quake_VertexLift  a BMAGIC cell vertex's height from Quake's lift table
//   Area109_SwitchHook / _SwitchPattern   area 109's switch cell
//   EffectKind18Sub41_DrawPanels / _DrawRings   sub-kind 0x41's two draws
//   AreaMap_FrameAreaBD, AreaMapBD_BuildView, AreaMapBD_CellTexture
//                   area 0xBD's view, built from a 16 x 16 block map
//   EffectKind0F_CharCount   a string's characters (a byte above 0x7F takes two)
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies. Sprite_Current
// is read again wherever the original reads [0x937F88] again after a call; a
// memory cell the original reads after a call is read after it here, each
// call's answer into a local first. No divergence: each is a faithful
// replacement. Where the original jumps through a step table past its end or
// indexes past a table, ours aborts with a message (docs/rest_3g.md section 7).
// AreaMapBD_BuildView reads its draw-item array, its item bound and its four
// cull bounds as DIV-0062 (draw_pool.h) and DIV-0041 (widescreen.h) hold the
// operands they patch, as the original reads the operands
// (docs/operand-store.md).
#include "game/rest_3g.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_3g_callees.h"
#include "game/cheats.h"
#include "game/map_rim.h"
#include "game/scenario_harness.h"
#include "game/draw_pool.h"
#include "game/psp_code.h"
#include "game/widescreen.h"
#include "hook/code_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = rest_3g::at;
using U = std::uint32_t;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

unsigned char* S() { return Sprite_Current; }
U ULRest3g(const unsigned char* p) { return static_cast<U>(move_script::Long(p)); }
U ULRest3g(U a) { return ULRest3g(move_script::At(a)); }
void SetULRest3g(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }
void SetULRest3g(U a, U v) { SetULRest3g(At(a), v); }
std::int32_t I(U v) { return static_cast<std::int32_t>(v); }
std::int32_t S8Rest3g(unsigned char b) { return static_cast<signed char>(b); }
U AddressOf(const void* p) { return Low32(p); }
unsigned char* Vertices() { return reinterpret_cast<unsigned char*>(Prim_VertexScratch); }
// sar on the 32 bits.
U SarRest3g(U v, unsigned n) { return static_cast<U>(I(v) >> n); }
float Fl(U a) {
    float f;
    std::memcpy(&f, At(a), sizeof f);
    return f;
}
// The CRT's _ftol 0x5B9550 on the value x87 holds: truncation through a 64-bit
// fistp, whose NaN and out-of-range answer is the integer indefinite
// 0x8000000000000000 (its low dword 0); the callers keep eax or its low word.
U FtolRest3g(long double v) {
    if (!(v > -9.2233720368547758e18L && v < 9.2233720368547758e18L)) return 0;
    return static_cast<U>(static_cast<std::uint64_t>(static_cast<std::int64_t>(v)));
}
// fld dword [from]; fstp dword [to]: the copy through the FPU, which quietens a
// signalling NaN where a mov would not.
void FpuCopy(void* to, const void* from) {
    float f;
    std::memcpy(&f, from, sizeof f);
    volatile long double through = f;
    const float back = static_cast<float>(through);
    std::memcpy(to, &back, sizeof back);
}

}  // namespace

// ===========================================================================
// Kind 0xAC: EffectKindAC_States (E4F's), three states - a fade in and out
// ===========================================================================

// ===========================================================================
// Kind 0xAD: EffectKindAD_States (E4F's), states 0 and 1 - a ring rising at
// the third party member
// ===========================================================================

// ===========================================================================
// Kind 0xAE: EffectKindAE_States (E4F's), state 0
// ===========================================================================

// ===========================================================================
// Kind 0xBA: EffectKindBA_States (E4F's), state 1 - the line from extra
// sprite 1's ring
// ===========================================================================

// ===========================================================================
// A screen triangle's winding
// ===========================================================================

// ===========================================================================
// The boss actors at a boss encounter
// ===========================================================================

// ===========================================================================
// Game modes 8..11 (GameMode_Handlers 8..11) and their steps
// ===========================================================================

// ===========================================================================
// Quake's vertex lift (MapCell_DrawTexQuads, _DrawShadedQuads, _DrawSpinQuads)
// ===========================================================================

// ===========================================================================
// Area 109's switch (Area_CellHooks' entry for area 0x6D)
// ===========================================================================

// original 0x4FEE70 (cdecl; EffectKind18_09_Pattern twice, Area109_SwitchHook):
// the three story flags 0x65DE60 lists as bits 0..2, plus 1 - eax 1..8.
extern "C" int __cdecl Area109_SwitchPattern(void) {
    U bits = 0;
    for (unsigned i = 0; i < 3; ++i) {
        const unsigned char set = SH_CALL(Flags_Test)(At(::bof3::Img(Cond_Flags_addr) + 0xa0), At(::bof3::Img(at::kPatternFlags.v) + i)[0]);
        if (set != 0) bits |= 1u << i;
    }
    return static_cast<int>(bits + 1);
}

// original 0x4FEEB0 (Area_CellHook's hook for area 0x6D, (x, z) cell bytes,
// al): at cell (0x1C, 6) with the leader facing 3 and story flag 0x1C clear, the
// pattern (Area109_SwitchPattern) mod 6 (idiv) set as the three flags anew -
// each cleared, then set by its bit; Effect_HoldFlag1C(0xF); sounds 0x206,
// 0x202; al 1. Otherwise al 0 (the rest of eax the caller's).
extern "C" unsigned char __cdecl Area109_SwitchHook(long x, long z) {
    if ((static_cast<U>(x) & 0xFF) != 0x1C || (static_cast<U>(z) & 0xFF) != 6 || ObjTrio[0].direction != 3) return 0;
    if (SH_CALL(Flags_Test)(At(::bof3::Img(Cond_Flags_addr) + 0xa0), 0x1C) != 0) return 0;
    const int pattern = SH_CALL(Area109_SwitchPattern)();
    int bits = pattern % 6;
    for (unsigned i = 0; i < 3; ++i) {
        SH_CALL(Flags_Clear)(At(::bof3::Img(Cond_Flags_addr) + 0xa0), At(::bof3::Img(at::kPatternFlags.v) + i)[0]);
        if (bits & 1) SH_CALL(Flags_Set)(At(::bof3::Img(Cond_Flags_addr) + 0xa0), At(::bof3::Img(at::kPatternFlags.v) + i)[0]);
        bits >>= 1;
    }
    SH_CALL(Effect_HoldFlag1C)(0xF);
    SH_CALL(Sound_PlayEffect)(0x206);
    SH_CALL(Sound_PlayEffect)(0x202);
    return 1;
}

// ===========================================================================
// Kind 0x18 sub-kind 0x41's two draws (EffectKind18Sub41_Hold's)
// ===========================================================================

// original 0x5100B0 (cdecl): a draw mode (page 0x95) committed at slot 5
// (0xC); two textured quads from the eight three-byte vertices at 0x65ED30 -
// x = b0 << 7 - 0x37C0, z = b1 << 7 - 0x2DC0, y = -0x3F8 - b2 * lift (each a
// word) - projected (Gte_RotTransPers4, Gte_PrimDepths4_10), the texture word
// (texture << 16) | 0xBB009120, committed at slot 5 (0x48).
extern "C" void __cdecl EffectKind18Sub41_DrawPanels(unsigned lift, unsigned texture) {
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x95, 0);
    SH_CALL(Gfx_CommitPrim)(5, 0xC);
    for (U from = ::bof3::Img(at::kPanelVertices.v); from < ::bof3::Img(at::kPanelVerticesEnd.v); from += 12) {
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyFT4)(p);
        SH_CALL(Gpu_SetShadeTex)(p, 0);
        unsigned char* const v = Vertices();
        for (unsigned k = 0; k < 4; ++k) {
            const unsigned char* const b = At(from + 3 * k);
            rv::As<records::SVector>(v)[k].x = (static_cast<U>(b[0]) << 7) - 0x37C0u;
            rv::As<records::SVector>(v)[k].y = (static_cast<U>(b[1]) << 7) - 0x2DC0u;
            rv::As<records::SVector>(v)[k].z = 0xFFFFFC08u - static_cast<U>(b[2]) * lift;
        }
        long depth;
        const short* const vs = reinterpret_cast<short*>(Prim_VertexScratch);
        SH_CALL(Gte_RotTransPers4)(vs, vs + 4, vs + 8, vs + 12, reinterpret_cast<float*>(p + 8),
                                   reinterpret_cast<float*>(p + 0x18), reinterpret_cast<float*>(p + 0x28),
                                   reinterpret_cast<float*>(p + 0x38), &depth);
        SH_CALL(Gte_PrimDepths4_10)(p);
        SH_CALL(Prim_SetTexture)((texture << 16) | 0xBB009120u, p, 1);
        SH_CALL(Gfx_CommitPrim)(5, 0x48);
    }
}

// original 0x5101C0 (cdecl, with +0xA): a draw mode (dtd, page 0xB5) committed
// at slot 5 (0xC); the vertex word z 0xFC08 (once). Three rings, from r = the
// argument and c = (0x28 - it) * 5, each next with r - 0x10 and c + 0x10: r
// below 0 taken as 0, c clamped to 0..0xFF; R = (r << 12) sar 10. Each: a
// semi-transparent LINE_F2 coloured (c / 2, c / 2, c) from (0xC840, R -
// 0x2CC0) to (0xC9C0, R - 0x2CC0); 16 chords of the circle of radius r about
// (-13888, -11456) - each end (sin * r sar 10 + x, cos * r sar 10 + y) through
// _ftol, coloured c / 2 - at angles 0..0x400 by 0x40; a line from (R - 0x3640,
// 0xD240) to (R - 0x3640, 0xD340); 16 chords about (-13888, -11712) at
// 0x400..0x800. Each line projected end by end (Gte_RotTransPers, the depth
// stored) and committed at slot 5 (0x20). The draw mode again (no dtd) to end.
extern "C" void __cdecl EffectKind18Sub41_DrawRings(int size) {
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0xB5, 0);
    SH_CALL(Gfx_CommitPrim)(5, 0xC);
    unsigned char* const v = Vertices();
    const short* const v0 = reinterpret_cast<short*>(Prim_VertexScratch);
    rv::As<records::SVector>(v)->z = 0xFC08;
    U radius_arg = static_cast<U>(size);
    U colour = (0x28u - static_cast<U>(size)) * 5u;
    long p_out;
    // A line's two ends projected into p + 8 / p + 0x14, each depth after it.
    auto end = [&](unsigned char* p, unsigned at_xy, unsigned at_depth) {
        SH_CALL(Gte_RotTransPers)(v0, reinterpret_cast<unsigned long*>(p + at_xy), &p_out);
        SH_CALL(Gte_StoreDepthF)(reinterpret_cast<float*>(p + at_depth));
    };
    for (unsigned ring = 3; ring != 0; --ring) {
        const U r = I(radius_arg) < 0 ? 0u : radius_arg;
        U c = colour;
        if (I(c) > 0xFF) c = 0xFF;
        const U c8 = I(c) < 0 ? 0u : c;
        const unsigned char half = static_cast<unsigned char>(c8 >> 1);
        const U big = SarRest3g(r << 12, 10);
        // the straight line across
        {
            unsigned char* const p = Gfx_PacketNext;
            SH_CALL(Gpu_SetLineF2)(p);
            SH_CALL(Gpu_SetSemiTrans)(p, 1);
            p[6] = static_cast<unsigned char>(c8);
            p[4] = half;
            p[5] = half;
            rv::As<records::SVector>(v)->x = 0xC840;
            SetWord(v + 2, big - 0x2CC0u);
            end(p, 8, 0x10);
            rv::As<records::SVector>(v)->x = 0xC9C0;
            rv::As<records::SVector>(v)->y = big - 0x2CC0u;
            end(p, 0x14, 0x1C);
            SH_CALL(Gfx_CommitPrim)(5, 0x20);
        }
        // a circle's 16 chords about the centre MapView_ScreenXY holds
        auto chords = [&](U from, U to) {
            for (U angle = from; angle < to;) {
                unsigned char* const p = Gfx_PacketNext;
                SH_CALL(Gpu_SetLineF2)(p);
                SH_CALL(Gpu_SetSemiTrans)(p, 1);
                p[4] = half;
                p[5] = half;
                p[6] = half;
                for (unsigned k = 0; k < 2; ++k) {
                    if (k == 1) angle += 0x40;
                    const int s = SH_CALL(Math_Sin)(static_cast<int>(angle));
                    const U dx = SarRest3g(static_cast<U>(s) * r, 10);
                    SetWord(v, FtolRest3g(static_cast<long double>(I(dx)) + static_cast<long double>(Fl(AddressOf(MapView_ScreenXY)))));
                    const int co = SH_CALL(Math_Cos)(static_cast<int>(angle));
                    const U dy = SarRest3g(static_cast<U>(co) * r, 10);
                    SetWord(v + 2,
                            FtolRest3g(static_cast<long double>(I(dy)) + static_cast<long double>(Fl(AddressOf(MapView_ScreenXY) + 4))));
                    end(p, k == 0 ? 8 : 0x14, k == 0 ? 0x10 : 0x1C);
                }
                SH_CALL(Gfx_CommitPrim)(5, 0x20);
            }
        };
        SetULRest3g(AddressOf(MapView_ScreenXY), 0xC6590000u);       // -13888.0
        SetULRest3g(AddressOf(MapView_ScreenXY) + 4, 0xC6330000u);   // -11456.0
        chords(0, 0x400);
        // the straight line down
        {
            unsigned char* const p = Gfx_PacketNext;
            SH_CALL(Gpu_SetLineF2)(p);
            SH_CALL(Gpu_SetSemiTrans)(p, 1);
            p[6] = static_cast<unsigned char>(c8);
            p[4] = half;
            p[5] = half;
            SetWord(v, big - 0x3640u);
            rv::As<records::SVector>(v)->y = 0xD240;
            end(p, 8, 0x10);
            rv::As<records::SVector>(v)->x = big - 0x3640u;
            rv::As<records::SVector>(v)->y = 0xD340;
            end(p, 0x14, 0x1C);
            SH_CALL(Gfx_CommitPrim)(5, 0x20);
        }
        SetULRest3g(AddressOf(MapView_ScreenXY), 0xC6590000u);       // -13888.0
        SetULRest3g(AddressOf(MapView_ScreenXY) + 4, 0xC6370000u);   // -11712.0
        chords(0x400, 0x800);
        radius_arg -= 0x10;
        colour += 0x10;
    }
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0xB5, 0);
    SH_CALL(Gfx_CommitPrim)(5, 0xC);
}

// ===========================================================================
// Area 0xBD's view
// ===========================================================================

// original 0x510630 (AreaMap_Frame's whole work in area 0xBD): MapView_FocusX
// / Z = (0x800000 - Field_Kind2X / Z) sar 8; with any of the three angle words
// moved from Camera_AnglesDrawn, MapView_Redraw 3 and both dwords copied;
// Gte_RotMatrix(Camera_Angles, Camera_Matrix); the vector (((FocusX >> 1) -
// 0x4000) & 0x7FFF) - 0x4000, the same of FocusZ, MapView_Elevation >> 1 (each
// a word, read after the matrix) through Gte_ApplyMatrix, plus Camera_ShiftX,
// Camera_ShiftY and Camera_Distance + 0x1194, as the translation; the matrix
// set; with MapView_Redraw, AreaMapBD_BuildView and MapView_Redraw down.
extern "C" void __cdecl AreaMap_FrameAreaBD(void) {
    MapView_FocusX = static_cast<long>(SarRest3g(0x800000u - static_cast<U>(Field_Kind2X), 8));
    MapView_FocusZ = static_cast<long>(SarRest3g(0x800000u - static_cast<U>(Field_Kind2Z), 8));
    {
        unsigned char* const angles = reinterpret_cast<unsigned char*>(Camera_Angles);
        unsigned char* const drawn = reinterpret_cast<unsigned char*>(Camera_AnglesDrawn);
        const U first = ULRest3g(angles), second = ULRest3g(angles + 4);
        if (Word(angles) != Word(drawn) || Word(angles + 2) != Word(drawn + 2) || (second & 0xFFFFu) != Word(drawn + 4)) {
            MapView_Redraw = 3;
            SetULRest3g(drawn, first);
            SetULRest3g(drawn + 4, second);
        }
    }
    SH_CALL(Gte_RotMatrix)(Camera_Angles, Camera_Matrix);
    short vector[3];
    vector[0] = static_cast<short>((((static_cast<U>(MapView_FocusX) >> 1) - 0x4000u) & 0x7FFFu) - 0x4000u);
    vector[1] = static_cast<short>((((static_cast<U>(MapView_FocusZ) >> 1) - 0x4000u) & 0x7FFFu) - 0x4000u);
    vector[2] = static_cast<short>(static_cast<U>(MapView_Elevation) >> 1);
    long moved[3];
    SH_CALL(Gte_ApplyMatrix)(Camera_Matrix, vector, moved);
    unsigned char* const matrix = reinterpret_cast<unsigned char*>(Camera_Matrix);
    SetULRest3g(matrix + 0x14, static_cast<U>(static_cast<int>(Camera_ShiftX)) + static_cast<U>(moved[0]));
    SetULRest3g(matrix + 0x18, static_cast<U>(static_cast<int>(Camera_ShiftY)) + static_cast<U>(moved[1]));
    SetULRest3g(matrix + 0x1C, static_cast<U>(static_cast<int>(Camera_Distance)) + static_cast<U>(moved[2]) + 0x1194u);
    SH_CALL(Gte_SetRotMatrix)(reinterpret_cast<const unsigned long*>(Camera_Matrix));
    SH_CALL(Gte_SetTransMatrix)(reinterpret_cast<const unsigned long*>(Camera_Matrix));
    if (MapView_Redraw == 0) return;
    SH_CALL(AreaMapBD_BuildView)();
    MapView_Redraw = static_cast<unsigned char>(MapView_Redraw - 1);
}

namespace {
// The two y bounds' operands, which nothing patches: the .rdata floats 120.0
// and 121.0 they name - the function's own, ours since the function is.
constexpr U kWideYFloat = 0x5C4244, kNarrowYFloat = 0x5C4238;
// The value of the float an `fcomp dword ptr [mem]` operand names: the two
// above, or DIV-0041's four as widescreen.cpp holds them. Values of ours, never
// a read of the .rdata place: our own exe does not carry .rdata that only our
// functions name, and the six floats read 0 there - the desert's floor culled
// whole, its cells kept only when y > 0 at the wide picture (issue 185).
float Bound(U operand_at) {
    if (operand_at == at::kWideYAt.v) return 120.0f;     // AreaMapBD_WideYBound
    if (operand_at == at::kNarrowYAt.v) return 121.0f;   // AreaMapBD_NarrowYBound
    return Widescreen_RangeValue(operand_at);
}
}  // namespace

// original 0x510780 (AreaMap_FrameAreaBD's, with MapView_Redraw): area 0xBD's
// cells drawn. DrawTable_Count 0; this buffer's lists 0 and 1 of the 0x38
// DrawLayers emptied; the camera's octant ((Cond_AngleFB + 0x100) sar 9) & 7
// picks a record of six s8 at 0x65ED48 (x0, dx per column, dx per row, z0, dz
// per column, dz per row); the first cell x0 - (byte (FocusX >> 8) - 0x80) +
// 0x100 and z0 likewise (words 0x903850 / 0x903852 and MapView_Origin); an odd
// octant walks 0x38 rows of 0x1C cells, an even one 0x2C of 0x28 (dwords
// 0x903854 / 0x903858); DrawItemPool_Top 0. Each cell, while the item index is
// below its bound (0x400): x = row * dxr / 2 + col * dxc + x0, z = (row + 1) *
// dzr / 2 + col * dzc + z0 (the halves toward 0); the block map 0x65ED78
// [((x sar 4) & 0xF) | (z & 0xF0)] gives the map cell (row nibble << 4 | z &
// 0xF) * 0x60 + (column nibble << 4 | x & 0xF); its first corner projected; kept
// when its screen y is above 120 and x inside (-200, 520), or y below 121 (or
// unordered) and x inside (-50, 370); a kept cell takes the next draw item
// (DrawItemPool_Top up), its texture (AreaMapBD_CellTexture), its first screen
// point (copied through the FPU), its three other corners projected, linked
// into list 0 of layer 0x37 - row, and its depths.
extern "C" void __cdecl AreaMapBD_BuildView(void) {
    map_rim::NoteOwnView(true);   // DIV-0041 (wide-ring): this view is Capcom's 28 x 56 (issue 185)
    DrawTable_Count = 0;
    {
        unsigned char* layer = At(AddressOf(DrawLayers) + Gfx_BufferIndex * 8u);
        for (unsigned n = 0; n < 0x38; ++n, layer += 0x30) {
            unsigned char* list = layer;
            for (unsigned k = 0; k < 2; ++k, list += 0x10) {
                SetULRest3g(list, 0);
                SetULRest3g(list + 4, AddressOf(list));
            }
        }
    }
    const U octant = SarRest3g(static_cast<U>(Cond_AngleFB) + 0x100u, 9) & 7u;
    const U fx = ((static_cast<U>(MapView_FocusX) >> 8) - 0x80u) & 0xFFu;
    const U fz = ((static_cast<U>(MapView_FocusZ) >> 8) - 0x80u) & 0xFFu;
    const unsigned char* const rec = At(::bof3::Img(at::kOctants.v) + octant * 6);
    const U even = (~octant) & 1u;   // not eax; and eax, 1
    const U x0 = static_cast<U>(S8Rest3g(rec[0])) - fx + 0x100u;
    const U z0 = static_cast<U>(S8Rest3g(rec[3])) - fz + 0x100u;
    const U rows = 0x38u - even * 12u, columns = even * 12u + 0x1Cu;
    at::kOriginX = x0;
    MapView_Origin[0] = static_cast<short>(x0);
    at::kOriginZ = z0;
    MapView_Origin[1] = static_cast<short>(z0);
    at::kRows = rows;
    at::kColumns = columns;
    DrawItemPool_Top = 0;
    U row_bound = rows;
    for (U row = 0; I(row) < I(row_bound); ++row) {
        U col_bound = row == 0 ? columns : static_cast<std::uint32_t>(at::kColumns);
        for (U col = 0; I(col) < I(col_bound); ++col, col_bound = static_cast<std::uint32_t>(at::kColumns)) {
            if (DrawItemPool_Top >= draw_pool::Count()) continue;   // at::kTopBoundAt: 0x400, DIV-0062's 0x800
            const U a = static_cast<U>(S8Rest3g(rec[2])) * row;
            const U x = static_cast<U>(I(a) / 2) + static_cast<U>(S8Rest3g(rec[1])) * col + static_cast<std::uint32_t>(static_cast<std::int16_t>(at::kOriginX));
            const U b = static_cast<U>(S8Rest3g(rec[5])) * (row + 1u);
            const U z = static_cast<U>(I(b) / 2) + static_cast<U>(S8Rest3g(rec[4])) * col + static_cast<std::uint32_t>(static_cast<std::int16_t>(at::kOriginZ));
            const unsigned char block = At(::bof3::Img(at::kBlocks.v) + ((SarRest3g(x, 4) & 0xFu) | (z & 0xF0u)))[0];
            const U map_row = (block & 0xF0u) | (z & 0xFu);
            const U map_col = ((block & 0xFu) << 4) | (x & 0xFu);
            const U cell = map_row * at::kMapWidth + map_col;
            unsigned char* const v = Vertices();
            rv::As<records::SVector>(v)->y = (z << 7) - 0x4040u;
            rv::As<records::SVector>(v)->x = (x << 7) - 0x4040u;
            unsigned char* corner = reinterpret_cast<unsigned char*>(&AreaMap_Corners) + cell * 4;
            MapView_CornerPtr = corner;
            rv::As<records::SVector>(v)->z = (0u - static_cast<U>(S8Rest3g(corner[0]))) << 4;
            SH_CALL(Gte_LoadVertex)(reinterpret_cast<const unsigned long*>(Prim_VertexScratch));
            SH_CALL(Gte_Rtps)();
            SH_CALL(Gte_StoreScreenXY)(reinterpret_cast<unsigned long*>(MapView_ScreenXY));
            // x87 fcomp against the operands' floats: an unordered compare sets
            // C0 and C3 both.
            const float sx = Fl(AddressOf(MapView_ScreenXY)), sy = Fl(AddressOf(MapView_ScreenXY) + 4);
            bool keep = sy > Bound(at::kWideYAt.v) && sx > Bound(at::kWideLoAt.v) && sx < Bound(at::kWideHiAt.v);
            if (!keep) {
                const bool y_ok = !(sy >= Bound(at::kNarrowYAt.v));
                keep = y_ok && sx > Bound(at::kNarrowLoAt.v) && !(sx >= Bound(at::kNarrowHiAt.v));
            }
            if (!keep) continue;

            const unsigned top = DrawItemPool_Top;
            unsigned char* const item = draw_pool::Items() + top * 0x90u;   // at::kItemsAt: DIV-0062's array once grown
            DrawItemPool_Top = static_cast<unsigned short>(top + 1);
            {
                const U offset = Word(AreaMap_Header + 2);
                unsigned char* const quad = item + Gfx_BufferIndex * 0x48u;
                const U tile = Word(AreaMap_Header + (cell + offset * 2u) * 2u);
                const U header = ULRest3g(AreaMap_Header);
                const U product = ((header >> 8) & 0xFFu) * (header & 0xFFu) + 1u;
                const U index = static_cast<U>(I(product) / 2) + offset + tile;
                SH_CALL(AreaMapBD_CellTexture)(static_cast<unsigned long>(ULRest3g(AreaMap_Header + index * 4u)), quad);
            }
            FpuCopy(item + Gfx_BufferIndex * 0x48u + 8, MapView_ScreenXY);
            FpuCopy(item + Gfx_BufferIndex * 0x48u + 0xC, MapView_ScreenXY + 1);
            {
                const U xy = ULRest3g(v);
                const U y = static_cast<std::uint16_t>(rv::As<records::SVector>(v)->y);
                corner = MapView_CornerPtr;
                const U x1 = xy + 0x80u;
                rv::As<records::SVector>(v)[1].x = x1;
                rv::As<records::SVector>(v)[1].y = y;
                rv::As<records::SVector>(v)[1].z = (0u - static_cast<U>(S8Rest3g(static_cast<unsigned char>(ULRest3g(corner) >> 8))) ) << 4;
                const U y1 = y + 0x80u;
                rv::As<records::SVector>(v)[2].x = static_cast<std::uint16_t>(rv::As<records::SVector>(v)->x);
                rv::As<records::SVector>(v)[2].y = y1;
                rv::As<records::SVector>(v)[3].x = x1;
                rv::As<records::SVector>(v)[2].z = (0u - static_cast<U>(S8Rest3g(static_cast<unsigned char>(ULRest3g(corner) >> 16))) ) << 4;
                rv::As<records::SVector>(v)[3].y = y1;
                rv::As<records::SVector>(v)[3].z = (0u - static_cast<U>(S8Rest3g(static_cast<unsigned char>(ULRest3g(corner) >> 24))) ) << 4;
            }
            SH_CALL(Gte_LoadVertices3)(reinterpret_cast<const unsigned long*>(v + 8));
            SH_CALL(Gte_Rtpt)();
            {
                const U last = ::bof3::Img(at::kLayerLast.v) + (Gfx_BufferIndex - row * 6u) * 8u;
                unsigned char* const quad = item + Gfx_BufferIndex * 0x48u;
                SH_CALL(Gpu_LinkPrim)(reinterpret_cast<unsigned long*>(static_cast<std::uintptr_t>(ULRest3g(last))), AddressOf(quad));
            }
            SetULRest3g(::bof3::Img(at::kLayerLast.v) + (Gfx_BufferIndex - row * 6u) * 8u, AddressOf(item + Gfx_BufferIndex * 0x48u));
            {
                unsigned char* const quad = item + Gfx_BufferIndex * 0x48u;
                SH_CALL(Gte_StoreScreenXY3)(reinterpret_cast<unsigned long*>(quad + 0x18), reinterpret_cast<unsigned long*>(quad + 0x28),
                                            reinterpret_cast<unsigned long*>(quad + 0x38));
            }
            SH_CALL(Gte_PrimDepths4_10)(item + Gfx_BufferIndex * 0x48u);
        }
        row_bound = static_cast<std::uint32_t>(at::kRows);
    }
}

// original 0x510BB0 (cdecl, by AreaMapBD_BuildView): the quad not semi-
// transparent; its colour 0x80 x 3; its CLUT word ((texture sar 24) & 0xF) +
// 0x1E3 << 6, its page word 0x95; the 16 x 16 tile of the texture's low byte -
// u (texture & 0xF) << 4 to + 0xF, v texture & 0xF0 to + 0xF.
extern "C" void __cdecl AreaMapBD_CellTexture(unsigned long texture, unsigned char* quad) {
    SH_CALL(Gpu_SetSemiTrans)(quad, 0);
    quad[6] = 0x80;
    quad[5] = 0x80;
    quad[4] = 0x80;
    const U t = static_cast<U>(texture);
    SetWord(quad + 0x26, 0x95);
    SetWord(quad + 0x16, ((SarRest3g(t, 24) & 0xFu) + 0x1E3u) << 6);
    const unsigned char v0 = static_cast<unsigned char>(t & 0xF0u);
    const unsigned char u0 = static_cast<unsigned char>((t & 0xFu) << 4);
    quad[0x15] = v0;
    quad[0x25] = v0;
    quad[0x14] = u0;
    quad[0x24] = static_cast<unsigned char>(u0 + 0xF);
    quad[0x34] = u0;
    quad[0x35] = static_cast<unsigned char>(v0 + 0xF);
    quad[0x44] = static_cast<unsigned char>(u0 + 0xF);
    quad[0x45] = static_cast<unsigned char>(v0 + 0xF);
}

// ===========================================================================
// Kind 0xF's character count
// ===========================================================================

// original 0x5171E0 (cdecl; EffectKind0F_Title, _TitleClose, _LineType): the
// characters of a string to its NUL, a byte with bit 7 taking the next byte
// with it (that byte not tested for the NUL); eax.
extern "C" int __cdecl EffectKind0F_CharCount(const unsigned char* text) {
    int count = 0;
    unsigned char c = *text++;
    if (c == 0) return 0;
    do {
        if (c & 0x80) ++text;
        c = *text;
        ++count;
        ++text;
    } while (c != 0);
    return count;
}

void Rest3G_Inject() {
    bof3::OperandExpect("AreaMapBD_BuildView y bound", at::kWideYAt.v, &kWideYFloat, 4);
    bof3::OperandExpect("AreaMapBD_BuildView y bound", at::kNarrowYAt.v, &kNarrowYFloat, 4);
    if (bof3::WantsShadow("rest_3g")) rest_3g::SelfTest();
    BOF3_INJECT(Area109_SwitchPattern);
    BOF3_INJECT(Area109_SwitchHook);
    BOF3_INJECT(EffectKind18Sub41_DrawPanels);
    BOF3_INJECT(EffectKind18Sub41_DrawRings);
    BOF3_INJECT(AreaMap_FrameAreaBD);
    BOF3_INJECT(AreaMapBD_BuildView);
    BOF3_INJECT(AreaMapBD_CellTexture);
    BOF3_INJECT(EffectKind0F_CharCount);
    // DIV-0187 writes a boss's +0x8F here; Capcom's Battle_PlaceBossActor
    // would clear it and the cheat would miss every boss.
    if (Cheats_EnemyInfoShown() && !bof3::IsEnabled(bof3::addr::Battle_PlaceBossActor))
        bof3::Fatal("BOF3X_ENEMY_INFO=1 needs Battle_PlaceBossActor ours, and it is disabled");
}

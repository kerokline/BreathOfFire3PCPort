// Moved here by tools/move_funcs.py from effect_5b.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_18_sub_0e.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_5b.h"
#include "game/effect_5b_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_5b::at;

}  // namespace

namespace {

using U = std::uint32_t;

using I = std::int32_t;

using move_script::At;

using move_script::Long;

using move_script::SetWord;

using move_script::Word;

unsigned char* S() { return Sprite_Current; }

U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }

unsigned char& B(U a) { return *At(a); }

U AddressOf(const void* p) { return Low32(p); }

using Handler = scenario_harness::Handler;

// Prim_VertexScratch's four SVECTORs (0x9037A0, A8, B0, B8): word k of vertex v.
// kScratch: now Prim_VertexScratch (symbols.toml).
U VW(unsigned v, unsigned k) { return Word(At(::bof3::Img(Prim_VertexScratch_addr) + 8 * v + 2 * k)); }

I VS(unsigned v, unsigned k) { return static_cast<std::int16_t>(VW(v, k)); }

void SetV(unsigned v, unsigned k, U value) { SetWord(At(::bof3::Img(Prim_VertexScratch_addr) + 8 * v + 2 * k), value); }

const short* VertexEffect5b(unsigned v) { return reinterpret_cast<const short*>(At(::bof3::Img(Prim_VertexScratch_addr) + 8 * v)); }

// cdq; xor eax, edx; sub eax, edx (INT_MIN stays itself)
I Abs(I v) {
    const U sign = static_cast<U>(v >> 31);
    return static_cast<I>((static_cast<U>(v) ^ sign) - sign);
}

// movsx eax, ax; cdq; sub eax, edx; sar eax, 1: an elevation's low word halved
// toward zero
I Half(long e) {
    const I w = static_cast<std::int16_t>(static_cast<U>(e));
    return (w - (w >> 31)) >> 1;
}

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 2]; jmp [table + eax * 4]:
// the table's `entries` handlers read in place (the fuzz swaps the cells for
// recorders); a Fatal past them, where the original jumps through the dword
// after - the next table, a data dword or a null.
void DispatchEffect5b(const char* who, U table, unsigned entries) {
    const unsigned sub = Sprite_Current[2];
    if (sub >= entries)
        bof3::Fatal("%s: sub-state byte +2 is %u, past the %u entries of 0x%X - the original jumps through the dword "
                    "after (docs/effect_5b.md section 7)",
                    who, sub, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(At(table + 4 * sub))))();
}

// One of the image's byte arrays by an index the record supplies (unchecked by
// the original, which reads the next array or table past its extent).
unsigned Byte(const char* who, U array, unsigned extent, U index) {
    if (index >= extent)
        bof3::Fatal("%s: index %d past the %u bytes of 0x%X - the original reads what follows "
                    "(docs/effect_5b.md section 7)",
                    who, static_cast<int>(index), extent, (unsigned)::bof3::VaOf(array));
    return B(array + index);
}

unsigned char* Prim() { return Gfx_PacketNext; }

unsigned char* Row9() { return At(::bof3::Img(Cond_Flags_addr) + 0x48); }

// The four-vertex projection every quad here makes of Prim_VertexScratch into a
// primitive whose vertices are `stride` apart from +8 (0x10 textured, 0xC flat).
void Project4Effect5b(unsigned char* prim, unsigned stride) {
    long p;
    SH_CALL(Gte_RotTransPers4)(VertexEffect5b(0), VertexEffect5b(1), VertexEffect5b(2), VertexEffect5b(3), reinterpret_cast<float*>(prim + 8),
                               reinterpret_cast<float*>(prim + 8 + stride), reinterpret_cast<float*>(prim + 8 + 2 * stride),
                               reinterpret_cast<float*>(prim + 8 + 3 * stride), &p);
}

// AreaMap_Bytes + z * width + x (width the area header's byte 0), as the
// original's 32-bit sums: no bound, as Capcom's.
unsigned char* Cell(I x, I z) {
    const U width = AreaMap_Header[0];
    return const_cast<unsigned char*>(AreaMap_Bytes.Native()) + (static_cast<U>(z) * width + static_cast<U>(x));
}

// Sub-kind 0x0E's fixed gate: the leader within `x` of 0x28000 and within `z`
// of 0x150000 or 0x160000.
bool Sub0ENear(I x, I z) {
    const U lx = static_cast<std::uint32_t>(ObjTrio[0].x), lz = static_cast<std::uint32_t>(ObjTrio[0].z);
    if (Abs(static_cast<I>(lx - 0x28000u)) > x) return false;
    return Abs(static_cast<I>(lz - 0x150000u)) <= z || Abs(static_cast<I>(lz - 0x160000u)) <= z;
}

// Sub-kind 0x0E's two cells (1, 21) and (1, 22): AreaMap_Bytes + 21w + 1 and + 22w + 1.
void Sub0ECells(unsigned char value) {
    *Cell(1, 21) = value;
    *Cell(1, 22) = value;
}

unsigned Sub13(const char* who, U array) { return Byte(who, array, at::kSub13Count, S()[0xB]); }

}  // namespace

// original 0x4FFE40 (EffectKind18_States[14], hidden in 0x4FFDD0): jmp
// [EffectKind18Sub0E_States + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub0E_Run(void) {
    DispatchEffect5b("EffectKind18Sub0E_Run", AddressOf(EffectKind18Sub0E_States), EffectKind18Sub0E_States_count);
}

// original 0x4FFE60 (state 0): the lowering +0x30 0; row 9's flag 0xC clear: the
// gate's cells 0x50 and +2 5; set: the cells 0xA1, +2 up and, with the leader
// at the gate, +0x30 0xFF00 and +2 3. The gate drawn.
extern "C" void __cdecl EffectKind18Sub0E_Start(void) {
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_30, 0);
    if (!SH_CALL(Flags_Test)(Row9(), 0xC)) {
        Sub0ECells(0x50);
        S()[2] = 5;
        SH_CALL(EffectKind18Sub0E_DrawGate)();
        return;
    }
    Sub0ECells(0xA1);
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
    if (Sub0ENear(0x8000, 0x10000)) {
        rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_30, 0xFF00);
        S()[2] = 3;
    }
    SH_CALL(EffectKind18Sub0E_DrawGate)();
}

// original 0x4FFF80 (state 1; EffectKind18Sub0E_Raise calls it): Cond_ByteFE not
// 0, or the leader at the gate: sound 0x200 (Field_Request 0 only) and +2 2 -
// each test on its own. The gate drawn.
extern "C" void __cdecl EffectKind18Sub0E_Wait(void) {
    if (Cond_ByteFE != 0) {
        if (Field_Request == 0) SH_CALL(Sound_PlayEffect)(0x200);
        S()[2] = 2;
    }
    if (Sub0ENear(0x8000, 0x10000)) {
        if (Field_Request == 0) SH_CALL(Sound_PlayEffect)(0x200);
        S()[2] = 2;
    }
    SH_CALL(EffectKind18Sub0E_DrawGate)();
}

// original 0x500020 (state 2): +0x30 down 0x20; at -0x100 or below (s16): +2 up.
// A tail jump to the draw.
extern "C" void __cdecl EffectKind18Sub0E_Lower(void) {
    unsigned char* const s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_30, rv::As<records::EffectWork>(s)->work_30 - 0x20u);
    if (static_cast<std::int32_t>(static_cast<std::int16_t>(rv::As<records::EffectWork>(S())->work_30)) <= -0x100) S()[2] = static_cast<unsigned char>(S()[2] + 1);
    SH_CALL(EffectKind18Sub0E_DrawGate)();
}

// original 0x500040 (state 3): the leader away from the gate (0x18000 either
// way): +2 4; Cond_ByteFE not 0: +2 3. A tail jump to the draw.
extern "C" void __cdecl EffectKind18Sub0E_Open(void) {
    if (!Sub0ENear(0x18000, 0x18000)) S()[2] = 4;
    if (Cond_ByteFE != 0) S()[2] = 3;
    SH_CALL(EffectKind18Sub0E_DrawGate)();
}

// original 0x5000B0 (state 4): +0x30 up 0x20; still below 0 (s16): the draw.
// Else EffectKind18Sub0E_Wait (which draws); +2 left at 2 by it: done; else +2
// 1, sound 0x201 (Field_Request 0 only) and the draw again.
extern "C" void __cdecl EffectKind18Sub0E_Raise(void) {
    unsigned char* const s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_30, rv::As<records::EffectWork>(s)->work_30 + 0x20u);
    if (static_cast<std::int32_t>(static_cast<std::int16_t>(rv::As<records::EffectWork>(S())->work_30)) < 0) {
        SH_CALL(EffectKind18Sub0E_DrawGate)();
        return;
    }
    SH_CALL(EffectKind18Sub0E_Wait)();
    unsigned char* const r = S();
    if (r[2] == 2) return;
    r[2] = 1;
    if (Field_Request == 0) SH_CALL(Sound_PlayEffect)(0x201);
    SH_CALL(EffectKind18Sub0E_DrawGate)();
}

// original 0x500100 (state 5): row 9's flag 0xC set: the cells 0xA1 and +2 1. A
// tail jump to the draw.
extern "C" void __cdecl EffectKind18Sub0E_Closed(void) {
    if (SH_CALL(Flags_Test)(Row9(), 0xC)) {
        Sub0ECells(0xA1);
        S()[2] = 1;
    }
    SH_CALL(EffectKind18Sub0E_DrawGate)();
}

// original 0x500160: under Draw_PassFlags bit 2, a draw mode (tpage 0x95)
// committed to slot 6, then a POLY_FT4 standing at x -0x3F40 from z +0x30 -
// 0x34C0 to +0x30 - 0x35C0, its foot 0x40 and its head 0x180 above half the
// ground's elevation at each end (AreaMap_Elevation), Gte_RotTransPers4,
// Gte_PrimDepths4_10, Prim_SetTexture(0x25500123, prim, 1), slot 6 (0x48).
extern "C" void __cdecl EffectKind18Sub0E_DrawGate(void) {
    if ((Draw_PassFlags & 4) == 0) return;
    SH_CALL(Gpu_SetDrawMode)(Prim(), 0, 0, 0x95, 0);
    SH_CALL(Gfx_CommitPrim)(6, 0xC);
    unsigned char* const prim = Prim();
    SH_CALL(Gpu_SetPolyFT4)(prim);
    SH_CALL(Gpu_SetShadeTex)(prim, 0);
    const unsigned char* const s = S();
    SetV(3, 0, 0xC0C0);
    SetV(2, 0, 0xC0C0);
    SetV(1, 0, 0xC0C0);
    SetV(0, 0, 0xC0C0);
    const U near = Word(s + 0x30) - 0x34C0u;
    SetV(2, 1, near);
    SetV(0, 1, near);
    const U z0 = static_cast<U>(static_cast<std::int16_t>(near) + 0x4000) << 9;
    const U far = Word(s + 0x30) - 0x35C0u;
    SetV(3, 1, far);
    SetV(1, 1, far);
    const long e1 = SH_CALL(AreaMap_Elevation)(0x18000, static_cast<long>(z0));
    SetV(0, 2, static_cast<U>(0x40 - Half(e1)));
    const long e2 = SH_CALL(AreaMap_Elevation)(0x18000, static_cast<long>(z0));
    const U z1 = static_cast<U>(VS(1, 1) + 0x4000) << 9;
    const U x1 = static_cast<U>(VS(1, 0) + 0x4000) << 9;
    SetV(2, 2, static_cast<U>(0x180 - Half(e2)));
    const long e3 = SH_CALL(AreaMap_Elevation)(static_cast<long>(x1), static_cast<long>(z1));
    SetV(1, 2, static_cast<U>(0x40 - Half(e3)));
    const long e4 = SH_CALL(AreaMap_Elevation)(static_cast<long>(x1), static_cast<long>(z1));
    SetV(3, 2, static_cast<U>(0x180 - Half(e4)));
    Project4Effect5b(prim, 0x10);
    SH_CALL(Gte_PrimDepths4_10)(prim);
    SH_CALL(Prim_SetTexture)(0x25500123, prim, 1);
    SH_CALL(Gfx_CommitPrim)(6, 0x48);
}

// original 0x500300 (EffectKind18_States[19], hidden in 0x500160): jmp
// [EffectKind18Sub13_States + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub13_Run(void) {
    DispatchEffect5b("EffectKind18Sub13_Run", AddressOf(EffectKind18Sub13_States), EffectKind18Sub13_States_count);
}

// original 0x500320 (state 0): +0xB = the byte +0x36 (the variant); the point
// +0x34 / +0x38 the variant's cell (_Cells, << 16); its row-9 flag (_Flags)
// set: +2 up and a jump into state 1; clear: +2 3 and a tail jump to state 3.
extern "C" void __cdecl EffectKind18Sub13_Start(void) {
    const char* const who = "EffectKind18Sub13_Start";
    unsigned char* s = S();
    s[0xB] = s[0x36];
    s = S();
    rv::As<records::EffectObject>(s)->x = static_cast<U>(Byte(who, ::bof3::Img(at::kSub13Cells.v), 2 * at::kSub13Count, 2u * s[0xB])) << 16;
    s = S();
    rv::As<records::EffectObject>(s)->z = static_cast<U>(B(::bof3::Img(at::kSub13Cells.v) + 1 + 2u * s[0xB])) << 16;
    if (SH_CALL(Flags_Test)(Row9(), Sub13(who, ::bof3::Img(at::kSub13Flags.v)))) {
        S()[2] = static_cast<unsigned char>(S()[2] + 1);
        SH_CALL(EffectKind18Sub13_Shown)();
        return;
    }
    S()[2] = 3;
    SH_CALL(EffectKind18Sub13_Hidden)();
}

// original 0x5003A0 (state 1; 0x500320 jumps here): the variant's flag clear: +9
// 9 and +2 up. The panel with face +0xB + 4 * (row 9's _Faces flag) + 1 at full
// height 0x40, and a tail jump to the line.
extern "C" void __cdecl EffectKind18Sub13_Shown(void) {
    const char* const who = "EffectKind18Sub13_Shown";
    if (!SH_CALL(Flags_Test)(Row9(), Sub13(who, ::bof3::Img(at::kSub13Flags.v)))) {
        S()[9] = 9;
        S()[2] = static_cast<unsigned char>(S()[2] + 1);
    }
    const U face = SH_CALL(Flags_Test)(Row9(), Sub13(who, ::bof3::Img(at::kSub13Faces.v))) & 0xFFu;
    SH_CALL(EffectKind18Sub13_DrawPanel)(S()[0xB] + face * 4 + 1, 0x40);
    SH_CALL(EffectKind18Sub13_DrawLine)();
}

// original 0x500420 (state 2): +9 down one, at 0: +2 up. The bare panel (face 0,
// 0x40), the face panel at +9 * 8, and a tail jump to the line.
extern "C" void __cdecl EffectKind18Sub13_Fall(void) {
    const char* const who = "EffectKind18Sub13_Fall";
    unsigned char* const s = S();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    if (S()[9] == 0) S()[2] = static_cast<unsigned char>(S()[2] + 1);
    SH_CALL(EffectKind18Sub13_DrawPanel)(0, 0x40);
    const int height = static_cast<int>(static_cast<unsigned>(S()[9]) << 3);
    const U face = SH_CALL(Flags_Test)(Row9(), Sub13(who, ::bof3::Img(at::kSub13Faces.v))) & 0xFFu;
    SH_CALL(EffectKind18Sub13_DrawPanel)(S()[0xB] + face * 4 + 1, height);
    SH_CALL(EffectKind18Sub13_DrawLine)();
}

// original 0x5004A0 (state 3; 0x500320 jumps here): the variant's flag set: +9 9
// and +2 up. The bare panel (face 0, 0x40).
extern "C" void __cdecl EffectKind18Sub13_Hidden(void) {
    if (SH_CALL(Flags_Test)(Row9(), Sub13("EffectKind18Sub13_Hidden", ::bof3::Img(at::kSub13Flags.v)))) {
        S()[9] = 9;
        S()[2] = static_cast<unsigned char>(S()[2] + 1);
    }
    SH_CALL(EffectKind18Sub13_DrawPanel)(0, 0x40);
}

// original 0x5004F0 (state 4): +9 down one, at 0: +2 1. A draw mode (tpage 0xB5)
// linked at the point; the face panel at 0x40; a semi-transparent POLY_F4 over
// the panel's vertices, grey +9 * 0x1E (Gte_RotTransPers4, Gte_PrimDepths4_0C),
// linked (0x38); then the line.
extern "C" void __cdecl EffectKind18Sub13_Rise(void) {
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    if (S()[9] == 0) S()[2] = 1;
    SH_CALL(Gpu_SetDrawMode)(Prim(), 0, 0, 0xB5, 0);
    s = S();
    SH_CALL(MapView_LinkPrimAt)(static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->x), static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->z), 1, 0xC);
    const U face = SH_CALL(Flags_Test)(Row9(), Sub13("EffectKind18Sub13_Rise", ::bof3::Img(at::kSub13Faces.v))) & 0xFFu;
    SH_CALL(EffectKind18Sub13_DrawPanel)(S()[0xB] + face * 4 + 1, 0x40);
    unsigned char* const prim = Prim();
    SH_CALL(Gpu_SetPolyF4)(prim);
    SH_CALL(Gpu_SetSemiTrans)(prim, 1);
    const auto grey = static_cast<unsigned char>(S()[9] * 0x1E);
    prim[6] = grey;
    prim[5] = grey;
    prim[4] = grey;
    Project4Effect5b(prim, 0xC);
    SH_CALL(Gte_PrimDepths4_0C)(prim);
    s = S();
    SH_CALL(MapView_LinkPrimAt)(static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->x), static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->z), 1, 0x38);
    SH_CALL(EffectKind18Sub13_DrawLine)();
}

// original 0x500610 (texture, height): a POLY_FT4 standing at x +0x36 * 0x80 -
// 0x3FC0 between z +0x3A * 0x80 + height - 0x4008 and - height - 0x3FF8; its
// head 0x48 - height and its foot height + 0x40 off half the ground's
// elevation at each end; Gte_RotTransPers4, Gte_PrimDepths4_10,
// Prim_SetTexture(texture + 0x275000F3, prim, 1), linked at the point (0x48).
extern "C" void __cdecl EffectKind18Sub13_DrawPanel(unsigned long texture, int height) {
    const unsigned char* const s = S();
    const U h = static_cast<U>(height);
    const U x = (rv::As<records::EffectObject>(s)->x_whole << 7) - 0x3FC0u;
    SetV(3, 0, x);
    SetV(2, 0, x);
    SetV(1, 0, x);
    SetV(0, 0, x);
    const U zn = (rv::As<records::EffectObject>(s)->z_whole << 7) + h - 0x4008u;
    const U xi = static_cast<U>(static_cast<std::int16_t>(x) + 0x4000) << 9;
    const U zi = static_cast<U>(static_cast<std::int16_t>(zn) + 0x4000) << 9;
    SetV(2, 1, zn);
    SetV(0, 1, zn);
    const U zf = (rv::As<records::EffectObject>(s)->z_whole << 7) - h - 0x3FF8u;
    SetV(3, 1, zf);
    SetV(1, 1, zf);
    const long e1 = SH_CALL(AreaMap_Elevation)(static_cast<long>(xi), static_cast<long>(zi));
    SetV(0, 2, 0x48u - static_cast<U>(Half(e1)) - h);
    const long e2 = SH_CALL(AreaMap_Elevation)(static_cast<long>(xi), static_cast<long>(zi));
    const U z1 = static_cast<U>(VS(1, 1) + 0x4000) << 9;
    const U x1 = static_cast<U>(VS(1, 0) + 0x4000) << 9;
    SetV(2, 2, h - static_cast<U>(Half(e2)) + 0x40u);
    const long e3 = SH_CALL(AreaMap_Elevation)(static_cast<long>(x1), static_cast<long>(z1));
    SetV(1, 2, 0x48u - static_cast<U>(Half(e3)) - h);
    const long e4 = SH_CALL(AreaMap_Elevation)(static_cast<long>(x1), static_cast<long>(z1));
    SetV(3, 2, h - static_cast<U>(Half(e4)) + 0x40u);
    unsigned char* const prim = Prim();
    SH_CALL(Gpu_SetPolyFT4)(prim);
    SH_CALL(Gpu_SetShadeTex)(prim, 0);
    Project4Effect5b(prim, 0x10);
    SH_CALL(Gte_PrimDepths4_10)(prim);
    SH_CALL(Prim_SetTexture)(static_cast<U>(texture) + 0x275000F3u, prim, 1);
    const unsigned char* const r = S();
    auto* const ro = rv::AsConst<records::EffectObject>(r);
    SH_CALL(MapView_LinkPrimAt)(static_cast<std::uint32_t>(ro->x), static_cast<std::uint32_t>(ro->z), 1, 0x48);
}

// original 0x5007B0: a draw mode (tpage 0x95, dtd 1) linked at the point; a
// semi-transparent LINE_F2, grey 0x80 on odd frames, from Prim_VertexScratch's
// first two vertices lifted to 2 * (Frame_Counter % 48) + 4 above half the
// ground's elevation (Gte_RotTransPers3 with the third into MapView_ScreenXY,
// Gte_StoreDepthF3), linked (0x20); a closing draw mode (dtd 0) linked.
extern "C" void __cdecl EffectKind18Sub13_DrawLine(void) {
    SH_CALL(Gpu_SetDrawMode)(Prim(), 0, 1, 0x95, 0);
    const unsigned char* s = S();
    SH_CALL(MapView_LinkPrimAt)(static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->x), static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->z), 1, 0xC);
    unsigned char* const prim = Prim();
    SH_CALL(Gpu_SetLineF2)(prim);
    SH_CALL(Gpu_SetSemiTrans)(prim, 1);
    const auto grey = static_cast<unsigned char>((Frame_Counter & 1) << 7);
    prim[6] = grey;
    prim[5] = grey;
    prim[4] = grey;
    const U z0 = static_cast<U>(VS(0, 1) + 0x4000) << 9;
    const U x0 = static_cast<U>(VS(0, 0) + 0x4000) << 9;
    const long e1 = SH_CALL(AreaMap_Elevation)(static_cast<long>(x0), static_cast<long>(z0));
    const I h1 = Half(e1);
    const U lift1 = (Frame_Counter % 0x30u) * 2 + 4;
    const U x1 = static_cast<U>(VS(1, 0) + 0x4000) << 9;
    SetV(0, 2, lift1 - static_cast<U>(h1));
    const U z1 = static_cast<U>(VS(1, 1) + 0x4000) << 9;
    const long e2 = SH_CALL(AreaMap_Elevation)(static_cast<long>(x1), static_cast<long>(z1));
    const I h2 = Half(e2);
    const U lift2 = (Frame_Counter % 0x30u) * 2 + 4;
    SetV(1, 2, lift2 - static_cast<U>(h2));
    long p;
    SH_CALL(Gte_RotTransPers3)(VertexEffect5b(0), VertexEffect5b(1), VertexEffect5b(2), reinterpret_cast<float*>(prim + 8),
                               reinterpret_cast<float*>(prim + 0x14), MapView_ScreenXY, &p);
    float depth;
    SH_CALL(Gte_StoreDepthF3)(reinterpret_cast<float*>(prim + 0x10), reinterpret_cast<float*>(prim + 0x1C), &depth);
    s = S();
    SH_CALL(MapView_LinkPrimAt)(static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->x), static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->z), 1, 0x20);
    SH_CALL(Gpu_SetDrawMode)(Prim(), 0, 0, 0x95, 0);
    s = S();
    SH_CALL(MapView_LinkPrimAt)(static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->x), static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->z), 1, 0xC);
}

void EffectKind18Sub0e_Inject() {
    BOF3_INJECT(EffectKind18Sub0E_Run);
    BOF3_INJECT(EffectKind18Sub0E_Start);
    BOF3_INJECT(EffectKind18Sub0E_Wait);
    BOF3_INJECT(EffectKind18Sub0E_Lower);
    BOF3_INJECT(EffectKind18Sub0E_Open);
    BOF3_INJECT(EffectKind18Sub0E_Raise);
    BOF3_INJECT(EffectKind18Sub0E_Closed);
    BOF3_INJECT(EffectKind18Sub0E_DrawGate);
    BOF3_INJECT(EffectKind18Sub13_Run);
    BOF3_INJECT(EffectKind18Sub13_Start);
    BOF3_INJECT(EffectKind18Sub13_Shown);
    BOF3_INJECT(EffectKind18Sub13_Fall);
    BOF3_INJECT(EffectKind18Sub13_Hidden);
    BOF3_INJECT(EffectKind18Sub13_Rise);
    BOF3_INJECT(EffectKind18Sub13_DrawPanel);
    BOF3_INJECT(EffectKind18Sub13_DrawLine);
}

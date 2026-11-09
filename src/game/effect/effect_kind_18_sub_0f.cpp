// Moved here by tools/move_funcs.py from effect_5b.cpp, effect_5c.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_18_sub_0f.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect/effect_kind_18_sub_0f_callees.h"
#include "game/effect_5b.h"
#include "game/effect_5b_callees.h"
#include "game/effect_5c.h"
#include "game/effect_5c_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/draw_order.h"
#include "hook/log.h"

namespace {

namespace at = effect_kind_18_sub_0f::at;

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

// movsx word
I SW(const unsigned char* p) { return static_cast<std::int16_t>(Word(p)); }

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

// The record's cell words (+0x36 x, +0x3A z) as 16.16 against the leader's point:
// with +8 set the gate lies along x, so the leader's z against the centre of
// the row after the gate's (within `across`) and its x against the gate's
// first or second cell (within `along`); with +8 clear the axes swap.
bool GateNearXZ(I x, I z, bool along_x, I across, I along) {
    if (along_x) {
        const U row = (static_cast<U>(z + 1) << 16) | 0x8000u;
        if (Abs(static_cast<I>(static_cast<std::uint32_t>(ObjTrio[0].z) - row)) > across) return false;
        const I d = static_cast<I>(static_cast<std::uint32_t>(ObjTrio[0].x) - (static_cast<U>(x) << 16));
        return Abs(d) <= along || Abs(static_cast<I>(static_cast<U>(d) - 0x10000u)) <= along;
    }
    const U column = (static_cast<U>(x + 1) << 16) | 0x8000u;
    if (Abs(static_cast<I>(static_cast<std::uint32_t>(ObjTrio[0].x) - column)) > across) return false;
    const I d = static_cast<I>(static_cast<std::uint32_t>(ObjTrio[0].z) - (static_cast<U>(z) << 16));
    return Abs(d) <= along || Abs(static_cast<I>(static_cast<U>(d) - 0x10000u)) <= along;
}

bool GateNear(const unsigned char* s, I across, I along) {
    return GateNearXZ(SW(s + 0x36), SW(s + 0x3A), s[8] != 0, across, along);
}

// The gate's two cells: (x, z) and, with +8 set, (x + 1, z), else (x, z + 1)
// (the original adds the byte +8 to x and +8 == 0 to z).
void GateCells(unsigned char value) {
    unsigned char* s = S();
    *Cell(static_cast<std::int32_t>(static_cast<std::int16_t>(rv::As<records::EffectObject>(s)->x_whole)), static_cast<std::int32_t>(static_cast<std::int16_t>(rv::As<records::EffectObject>(s)->z_whole))) = value;
    s = S();
    const unsigned b8 = s[8];
    *Cell(static_cast<std::int32_t>(static_cast<std::int16_t>(rv::As<records::EffectObject>(s)->x_whole)) + static_cast<I>(b8), static_cast<std::int32_t>(static_cast<std::int16_t>(rv::As<records::EffectObject>(s)->z_whole)) + (b8 == 0 ? 1 : 0)) = value;
}

// Sub-kind 0x4F's two cells: (x, z) and (x, z + 1).
void Gate4FCells(unsigned char value) {
    unsigned char* s = S();
    *Cell(static_cast<std::int32_t>(static_cast<std::int16_t>(rv::As<records::EffectObject>(s)->x_whole)), static_cast<std::int32_t>(static_cast<std::int16_t>(rv::As<records::EffectObject>(s)->z_whole))) = value;
    s = S();
    *Cell(static_cast<std::int32_t>(static_cast<std::int16_t>(rv::As<records::EffectObject>(s)->x_whole)), static_cast<std::int32_t>(static_cast<std::int16_t>(rv::As<records::EffectObject>(s)->z_whole)) + 1) = value;
}

void Gate0FPrepare(const char* who, bool sub4F);

// _Start's set-up common to sub-kinds 0x0F and 0x4F, from the variant +0x36 (v)
// and the flag index +0x3A: +0xA (0x0F: the byte +0x3A + 1; 0x4F: 8 - the byte
// v), +0xB = _Flags[+0x3A], the cell words from _Cells[v], +8 = (v == 0), +0x3E
// = -_Heights[v] - half the ground's elevation at the point, +0x32 =
// _Heights[v], +0x2E = _Textures[v], +0x30 = 0.
void Gate0FPrepare(const char* who, bool sub4F) {
    unsigned char* s = S();
    const I v = SW(s + 0x36);
    if (sub4F) s[0xA] = static_cast<unsigned char>(8 - static_cast<unsigned char>(v));
    else s[0xA] = static_cast<unsigned char>(s[0x3A] + 1);
    s = S();
    s[0xB] = static_cast<unsigned char>(Byte(who, ::bof3::Img(at::kGateFlags.v), at::kGateFlagCount, static_cast<U>(SW(s + 0x3A))));
    const unsigned cell_x = Byte(who, ::bof3::Img(at::kGateCells.v), 2 * at::kGateCount, 2u * static_cast<U>(v));
    SetWord(S() + 0x36, cell_x);
    SetWord(S() + 0x3A, B(::bof3::Img(at::kGateCells.v) + 1 + 2u * static_cast<U>(v)));
    S()[8] = v == 0 ? 1 : 0;
    s = S();
    const long e = SH_CALL(AreaMap_Elevation)(Long(s + 0x34), Long(s + 0x38));
    const U height = Word(At(::bof3::Img(at::kGateHeights.v) + 2u * static_cast<U>(v)));
    SetWord(S() + 0x3E, (0u - height) - static_cast<U>(Half(e)));
    SetWord(S() + 0x32, height);
    SetWord(S() + 0x2E, Word(At(::bof3::Img(at::kGateTextures.v) + 2u * static_cast<U>(v))));
    SetWord(S() + 0x30, 0);
}

std::int32_t SWEffect5c(U v) { return static_cast<std::int16_t>(v & 0xFFFFu); }

// movsx eax, ax; cdq; sub eax, edx; sar eax, 1: a height word halved toward 0.
U HalfOf(long h) { return static_cast<U>(SWEffect5c(static_cast<U>(h)) / 2); }

// Prim_VertexScratch 0x9037A0: four SVECTORs of 8 bytes (a0, a8, b0, b8).
unsigned char* VSEffect5c(unsigned off) { return reinterpret_cast<unsigned char*>(Prim_VertexScratch) + off; }

const short* VX(unsigned off) { return reinterpret_cast<const short*>(VSEffect5c(off)); }

float* F(unsigned char* p) { return reinterpret_cast<float*>(p); }

// (x << 9) of a vertex word sign-extended and lifted by 0x4000: the 16.16 cell
// AreaMap_Elevation is asked for.
U CellOf(U word) { return static_cast<U>(SWEffect5c(word) + 0x4000) << 9; }

U VertexCell(unsigned off) { return CellOf(Word(VSEffect5c(off))); }

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 2]; jmp [table +
// eax * 4]: the table's `entries` handlers read in place (the fuzz swaps the
// cells for recorders); a Fatal past them, where the original jumps through the
// dword after - the next table or data.
void DispatchEffect5c(const char* who, U table, unsigned entries) {
    const unsigned state = Sprite_Current[2];
    if (state >= entries)
        bof3::Fatal("%s: sub-state byte +2 is %u, past the %u entries of 0x%X - the original jumps through the dword "
                    "after (docs/effect_5c.md section 6)",
                    who, state, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(At(table + 4 * state))))();
}

// --- sub-kind 0x50's small tables (the variant and the flag index) -----------------------
unsigned Sub50Variant(const char* who, unsigned v) {
    if (v >= at::kSub50Variants)
        bof3::Fatal("%s: variant %u, past the %u cells of 0x%X - the original reads the bytes after "
                    "(docs/effect_5c.md section 6)",
                    who, v, at::kSub50Variants, (unsigned)AddressOf(EffectKind18Sub50_Cells));
    return v;
}

unsigned char Sub50Byte(const char* who, U table, unsigned index) {
    if (index >= at::kSub50FlagCount)
        bof3::Fatal("%s: +0xB is %u, past the %u flag bytes of 0x%X - the original reads the bytes after "
                    "(docs/effect_5c.md section 6)",
                    who, index, at::kSub50FlagCount, (unsigned)::bof3::VaOf(table));
    return At(table + index)[0];
}

// The four vertices in Prim_VertexScratch projected into a quad's four screen
// points at the given offsets (the depth out a local nothing reads).
void Project4Effect5c(unsigned char* prim, unsigned o0, unsigned o1, unsigned o2, unsigned o3) {
    long depth = 0;
    SH_CALL(Gte_RotTransPers4)(VX(0), VX(8), VX(0x10), VX(0x18), F(prim + o0), F(prim + o1), F(prim + o2), F(prim + o3),
                               &depth);
}

// MapView_LinkPrimAt at Sprite_Current's point (+0x34, +0x38; read afresh), row
// offset 1.
void LinkAtRecord(unsigned size) {
    unsigned char* const s = S();
    auto* const so = rv::As<records::EffectObject>(s);
    SH_CALL(MapView_LinkPrimAt)(static_cast<std::uint32_t>(so->x), static_cast<std::uint32_t>(so->z), 1, size);
}

}  // namespace

// original 0x500930 (EffectKind18_States[15], hidden in 0x5007B0): jmp
// [EffectKind18Sub0F_States + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub0F_Run(void) {
    DispatchEffect5b("EffectKind18Sub0F_Run", AddressOf(EffectKind18Sub0F_States), EffectKind18Sub0F_States_count);
}

// original 0x500950 (state 0): the set-up (Gate0FPrepare); row 9's flag +0xB
// clear: the gate's cells 0x52 and +2 5; set: the cells 0xA1, +2 up, and with
// the leader at the gate +0x30 0xFF00 and +2 3. The gate drawn flat (0).
extern "C" void __cdecl EffectKind18Sub0F_Start(void) {
    Gate0FPrepare("EffectKind18Sub0F_Start", false);
    if (!SH_CALL(Flags_Test)(Row9(), S()[0xB])) {
        GateCells(0x52);
        S()[2] = 5;
        SH_CALL(EffectKind18Sub0F_DrawGate)(0);
        return;
    }
    GateCells(0xA1);
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
    unsigned char* const s = S();
    if (GateNear(s, 0x8000, 0x10000)) {
        rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_30, 0xFF00);
        S()[2] = 3;
    }
    SH_CALL(EffectKind18Sub0F_DrawGate)(0);
}

// original 0x500BD0 (state 1; EffectKind18Sub0F_Raise calls it): Cond_ByteFD 0
// and Cond_ByteFE the record's +0xA, or the leader at the gate: sound 0x200
// (Field_Request 0 only) and +2 2, each on its own. The gate drawn on the ground.
extern "C" void __cdecl EffectKind18Sub0F_Wait(void) {
    if (Cond_ByteFD == 0 && Cond_ByteFE == S()[0xA]) {
        if (Field_Request == 0) SH_CALL(Sound_PlayEffect)(0x200);
        S()[2] = 2;
    }
    if (GateNear(S(), 0x8000, 0x10000)) {
        if (Field_Request == 0) SH_CALL(Sound_PlayEffect)(0x200);
        S()[2] = 2;
    }
    SH_CALL(EffectKind18Sub0F_DrawGate)(1);
}

// original 0x500CF0 (state 2 of both tables): +0x30 down 0x20; at -0x100 or below
// (s16): +2 up. The gate drawn flat.
extern "C" void __cdecl EffectKind18Sub0F_Lower(void) {
    unsigned char* const s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_30, rv::As<records::EffectWork>(s)->work_30 - 0x20u);
    if (static_cast<std::int32_t>(static_cast<std::int16_t>(rv::As<records::EffectWork>(S())->work_30)) <= -0x100) S()[2] = static_cast<unsigned char>(S()[2] + 1);
    SH_CALL(EffectKind18Sub0F_DrawGate)(0);
}

// original 0x500D20 (state 3): unless Cond_ByteFD is 0 and Cond_ByteFE the
// record's +0xA, the leader away from the gate (two cells): +2 4. Nothing drawn.
extern "C" void __cdecl EffectKind18Sub0F_Open(void) {
    unsigned char* const s = S();
    if (Cond_ByteFD == 0 && Cond_ByteFE == s[0xA]) return;
    if (!GateNear(s, 0x20000, 0x20000)) s[2] = 4;
}

// original 0x500E00 (state 4 of both tables): +0x30 up 0x20; still below 0 (s16):
// drawn flat. Else EffectKind18Sub0F_Wait (which draws); +2 left at 2 by it:
// done; else sound 0x201 (Field_Request 0 only), +2 1 and drawn flat.
extern "C" void __cdecl EffectKind18Sub0F_Raise(void) {
    unsigned char* const s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_30, rv::As<records::EffectWork>(s)->work_30 + 0x20u);
    if (static_cast<std::int32_t>(static_cast<std::int16_t>(rv::As<records::EffectWork>(S())->work_30)) < 0) {
        SH_CALL(EffectKind18Sub0F_DrawGate)(0);
        return;
    }
    SH_CALL(EffectKind18Sub0F_Wait)();
    if (S()[2] == 2) return;
    if (Field_Request == 0) SH_CALL(Sound_PlayEffect)(0x201);
    S()[2] = 1;
    SH_CALL(EffectKind18Sub0F_DrawGate)(0);
}

// original 0x500E50 (state 5): row 9's flag +0xB set: the gate's cells 0xA1 and
// +2 1. The gate drawn on the ground.
extern "C" void __cdecl EffectKind18Sub0F_Closed(void) {
    if (SH_CALL(Flags_Test)(Row9(), S()[0xB])) {
        GateCells(0xA1);
        S()[2] = 1;
    }
    SH_CALL(EffectKind18Sub0F_DrawGate)(1);
}

// original 0x500EF0 (ground): under Draw_PassFlags bit 2, a draw mode (tpage
// 0x95) committed to slot 6; a POLY_FT4 standing over the gate's cell - along x
// with +8 set (x +0x36 * 0x80 - +0x30 - 0x3F40 to 0x100 less, z +0x3A * 0x80 -
// 0x3FC0), along z without (x +0x36 * 0x80 - 0x3FC0, z +0x3A * 0x80 + +0x30 -
// 0x3F40 to 0x100 less) - its feet at -(half the elevation at each end) -
// +0x3E with ground, else +0x32, its heads 0x140 above; Gte_RotTransPers4,
// Gte_PrimDepths4_10, Prim_SetTexture(+8 * 41 << 16 | (s16) +0x2E | 0x26500000,
// prim, 1), slot 6 (0x48).
extern "C" void __cdecl EffectKind18Sub0F_DrawGate(int ground) {
    if ((Draw_PassFlags & 4) == 0) return;
    SH_CALL(Gpu_SetDrawMode)(Prim(), 0, 0, 0x95, 0);
    SH_CALL(Gfx_CommitPrim)(6, 0xC);
    unsigned char* const prim = Prim();
    SH_CALL(Gpu_SetPolyFT4)(prim);
    SH_CALL(Gpu_SetShadeTex)(prim, 0);
    const unsigned char* const s = S();
    U vx, vz;
    if (s[8] != 0) {
        vz = (rv::As<records::EffectObject>(s)->z_whole << 7) - 0x3FC0u;
        SetV(3, 1, vz);
        SetV(2, 1, vz);
        SetV(1, 1, vz);
        SetV(0, 1, vz);
        vx = (rv::As<records::EffectObject>(s)->x_whole << 7) - rv::As<records::EffectWork>(s)->work_30 - 0x3F40u;
        SetV(2, 0, vx);
        SetV(0, 0, vx);
        const U x1 = (rv::As<records::EffectObject>(s)->x_whole << 7) - rv::As<records::EffectWork>(s)->work_30 - 0x4040u;
        SetV(3, 0, x1);
        SetV(1, 0, x1);
    } else {
        vx = (rv::As<records::EffectObject>(s)->x_whole << 7) - 0x3FC0u;
        SetV(3, 0, vx);
        SetV(2, 0, vx);
        SetV(1, 0, vx);
        SetV(0, 0, vx);
        vz = (rv::As<records::EffectObject>(s)->z_whole << 7) + rv::As<records::EffectWork>(s)->work_30 - 0x3F40u;
        SetV(2, 1, vz);
        SetV(0, 1, vz);
        const U z1 = (rv::As<records::EffectObject>(s)->z_whole << 7) + rv::As<records::EffectWork>(s)->work_30 - 0x4040u;
        SetV(3, 1, z1);
        SetV(1, 1, z1);
    }
    if (ground != 0) {
        const U z0 = static_cast<U>(static_cast<std::int16_t>(vz) + 0x4000) << 9;
        const U x0 = static_cast<U>(static_cast<std::int16_t>(vx) + 0x4000) << 9;
        const long e1 = SH_CALL(AreaMap_Elevation)(static_cast<long>(x0), static_cast<long>(z0));
        SetV(0, 2, (0u - static_cast<U>(Half(e1))) - rv::As<records::EffectObject>(S())->height_whole - 0x140u);
        const long e2 = SH_CALL(AreaMap_Elevation)(static_cast<long>(x0), static_cast<long>(z0));
        const U z1 = static_cast<U>(VS(1, 1) + 0x4000) << 9;
        const U x1 = static_cast<U>(VS(1, 0) + 0x4000) << 9;
        SetV(2, 2, (0u - static_cast<U>(Half(e2))) - rv::As<records::EffectObject>(S())->height_whole);
        const long e3 = SH_CALL(AreaMap_Elevation)(static_cast<long>(x1), static_cast<long>(z1));
        SetV(1, 2, (0u - static_cast<U>(Half(e3))) - rv::As<records::EffectObject>(S())->height_whole - 0x140u);
        const long e4 = SH_CALL(AreaMap_Elevation)(static_cast<long>(x1), static_cast<long>(z1));
        SetV(3, 2, (0u - static_cast<U>(Half(e4))) - rv::As<records::EffectObject>(S())->height_whole);
    } else {
        const U head = rv::As<records::EffectWork>(s)->work_32 - 0x140u;
        SetV(1, 2, head);
        SetV(0, 2, head);
        SetV(3, 2, rv::As<records::EffectWork>(s)->work_32);
        SetV(2, 2, rv::As<records::EffectWork>(s)->work_32);
    }
    Project4Effect5b(prim, 0x10);
    SH_CALL(Gte_PrimDepths4_10)(prim);
    const unsigned char* const r = S();
    const U texture = ((static_cast<U>(r[8]) * 41u) << 16 | static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(rv::As<records::EffectWork>(r)->work_2e)))) | 0x26500000u;
    SH_CALL(Prim_SetTexture)(texture, prim, 1);
    SH_CALL(Gfx_CommitPrim)(6, 0x48);
}

// original 0x5011A0 (EffectKind18_States[79], hidden in 0x500EF0): jmp
// [EffectKind18Sub4F_States + +2 * 4], unbounded. Its entries 2 and 4 are
// sub-kind 0x0F's _Lower and _Raise.
extern "C" void __cdecl EffectKind18Sub4F_Run(void) {
    DispatchEffect5b("EffectKind18Sub4F_Run", AddressOf(EffectKind18Sub4F_States), EffectKind18Sub4F_States_count);
}

// original 0x5011C0 (state 0): the set-up (+0xA = 8 - the variant); the gate's
// cells (x, z) and (x, z + 1) 0x52; +2 up; with the leader at the gate (the +8
// clear test whatever +8 holds) the cells 0xA1, +0x30 0xFF00 and +2 3. Drawn
// flat.
extern "C" void __cdecl EffectKind18Sub4F_Start(void) {
    Gate0FPrepare("EffectKind18Sub4F_Start", true);
    Gate4FCells(0x52);
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
    const unsigned char* const s = S();
    const I x = static_cast<std::int32_t>(static_cast<std::int16_t>(rv::As<records::EffectObject>(s)->x_whole)), z = static_cast<std::int32_t>(static_cast<std::int16_t>(rv::As<records::EffectObject>(s)->z_whole));
    if (GateNearXZ(x, z, false, 0x8000, 0x10000)) {
        *Cell(x, z) = 0xA1;
        const unsigned char* const r = S();
        *Cell(static_cast<std::int32_t>(static_cast<std::int16_t>(rv::As<records::EffectObject>(r)->x_whole)), static_cast<std::int32_t>(static_cast<std::int16_t>(rv::As<records::EffectObject>(r)->z_whole)) + 1) = 0xA1;
        rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_30, 0xFF00);
        S()[2] = 3;
    }
    SH_CALL(EffectKind18Sub0F_DrawGate)(0);
}

// original 0x5013A0 (state 1): Cond_ByteFE the record's +0xA: sound 0x200
// (Field_Request 0 only), the cells 0xA1 and +2 2. Drawn on the ground.
extern "C" void __cdecl EffectKind18Sub4F_Wait(void) {
    if (Cond_ByteFE == S()[0xA]) {
        if (Field_Request == 0) SH_CALL(Sound_PlayEffect)(0x200);
        Gate4FCells(0xA1);
        S()[2] = 2;
    }
    SH_CALL(EffectKind18Sub0F_DrawGate)(1);
}

// original 0x501430 (state 3): unless Cond_ByteFE is the record's +0xA, the leader
// away from the gate (two cells, the +8 clear test): the cells 0x52 and +2 4.
// Drawn flat.
extern "C" void __cdecl EffectKind18Sub4F_Open(void) {
    const unsigned char* const s = S();
    if (Cond_ByteFE != s[0xA]) {
        const I x = static_cast<std::int32_t>(static_cast<std::int16_t>(rv::As<records::EffectObject>(s)->x_whole)), z = static_cast<std::int32_t>(static_cast<std::int16_t>(rv::As<records::EffectObject>(s)->z_whole));
        if (!GateNearXZ(x, z, false, 0x20000, 0x20000)) {
            *Cell(x, z) = 0x52;
            const unsigned char* const r = S();
            *Cell(static_cast<std::int32_t>(static_cast<std::int16_t>(rv::As<records::EffectObject>(r)->x_whole)), static_cast<std::int32_t>(static_cast<std::int16_t>(rv::As<records::EffectObject>(r)->z_whole)) + 1) = 0x52;
            S()[2] = 4;
        }
    }
    SH_CALL(EffectKind18Sub0F_DrawGate)(0);
}

// original 0x501500 (EffectKind18_States[80], hidden in E5B's 0x500EF0):
// jmp [EffectKind18Sub50_States + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub50_Run(void) {
    DispatchEffect5c("EffectKind18Sub50_Run", AddressOf(EffectKind18Sub50_States), EffectKind18Sub50_States_count);
}

// original 0x501520 (sub-state 0): the variant +0xB = the x byte +0x36; the
// point (+0x34, +0x38) the variant's cell (0x65E0AC, x and z bytes << 16);
// +0x3E = -(the ground there / 2) - the variant's height word (0x65E0B0);
// +0xB up one; row-9 flag 0x65E094[+0xB] set: +2 up and a tail jump to
// sub-state 1, clear: +2 = 3 and a tail jump to sub-state 3.
extern "C" void __cdecl EffectKind18Sub50_Start(void) {
    static const char kWho[] = "EffectKind18Sub50_Start";
    unsigned char* s = S();
    s[0xB] = s[0x36];
    unsigned v = Sub50Variant(kWho, s[0xB]);
    rv::As<records::EffectObject>(s)->x = static_cast<U>(At(AddressOf(EffectKind18Sub50_Cells) + 2 * v)[0]) << 16;
    v = Sub50Variant(kWho, s[0xB]);
    rv::As<records::EffectObject>(s)->z = static_cast<U>(At(AddressOf(EffectKind18Sub50_Cells) + 2 * v + 1)[0]) << 16;
    const long ground = SH_CALL(AreaMap_Elevation)(rv::As<records::EffectObject>(s)->x, rv::As<records::EffectObject>(s)->z);
    s = S();
    v = Sub50Variant(kWho, s[0xB]);
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->height_whole, (0u - HalfOf(ground)) - Word(At(AddressOf(EffectKind18Sub50_Heights) + 2 * v)));
    s[0xB] = static_cast<unsigned char>(s[0xB] + 1);
    const unsigned char flag = Sub50Byte(kWho, AddressOf(EffectKind18Sub50_Flags), s[0xB]);
    const unsigned char set = SH_CALL(Flags_Test)(At(::bof3::Img(Cond_Flags_addr) + 0x48), flag);
    s = S();
    if (set) {
        s[2] = static_cast<unsigned char>(s[2] + 1);
        SH_CALL(EffectKind18Sub50_Shine)();
        return;
    }
    s[2] = 3;
    SH_CALL(EffectKind18Sub50_WaitSet)();
}

// original 0x5015E0 (sub-state 1; also the tail of sub-state 0): row-9 flag
// 0x65E094[+0xB] clear: +9 = 9, +2 up. Then the wall, its texture 1 + +0xB +
// 4 * row-9 flag 0x65E034[+0xB], height 0x40, and a tail jump to the line.
extern "C" void __cdecl EffectKind18Sub50_Shine(void) {
    static const char kWho[] = "EffectKind18Sub50_Shine";
    unsigned char* s = S();
    if (!SH_CALL(Flags_Test)(At(::bof3::Img(Cond_Flags_addr) + 0x48), Sub50Byte(kWho, AddressOf(EffectKind18Sub50_Flags), s[0xB]))) {
        s = S();
        s[9] = 9;
        s[2] = static_cast<unsigned char>(s[2] + 1);
    }
    s = S();
    const U light = SH_CALL(Flags_Test)(At(::bof3::Img(Cond_Flags_addr) + 0x48), Sub50Byte(kWho, ::bof3::Img(at::kSub50Lights.v), s[0xB])) & 0xFFu;
    s = S();
    SH_CALL(EffectKind18Sub50_DrawWall)(s[0xB] + light * 4u + 1u, 0x40);
    SH_CALL(EffectKind18Sub50_DrawLine)();
}

// original 0x501660 (sub-state 2): +9 down; at 0 +2 up. The bare wall
// (texture 0, height 0x40), then the lit one at height +9 * 8; a tail jump to
// the line.
extern "C" void __cdecl EffectKind18Sub50_Sink(void) {
    static const char kWho[] = "EffectKind18Sub50_Sink";
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    if (s[9] == 0) s[2] = static_cast<unsigned char>(s[2] + 1);
    SH_CALL(EffectKind18Sub50_DrawWall)(0, 0x40);
    s = S();
    const U height = static_cast<U>(s[9]) << 3;
    const unsigned char flag = Sub50Byte(kWho, ::bof3::Img(at::kSub50Lights.v), s[0xB]);
    const U light = SH_CALL(Flags_Test)(At(::bof3::Img(Cond_Flags_addr) + 0x48), flag) & 0xFFu;
    s = S();
    SH_CALL(EffectKind18Sub50_DrawWall)(s[0xB] + light * 4u + 1u, height);
    SH_CALL(EffectKind18Sub50_DrawLine)();
}

// original 0x5016E0 (sub-state 3; also the tail of sub-state 0; a start no list
// of the cut holds): row-9 flag 0x65E094[+0xB] set: +9 = 9, +2 up. The bare
// wall.
extern "C" void __cdecl EffectKind18Sub50_WaitSet(void) {
    unsigned char* s = S();
    if (SH_CALL(Flags_Test)(At(::bof3::Img(Cond_Flags_addr) + 0x48), Sub50Byte("EffectKind18Sub50_WaitSet", AddressOf(EffectKind18Sub50_Flags), s[0xB]))) {
        s = S();
        s[9] = 9;
        s[2] = static_cast<unsigned char>(s[2] + 1);
    }
    SH_CALL(EffectKind18Sub50_DrawWall)(0, 0x40);
}

// original 0x501730 (sub-state 4): +9 down; at 0 +2 = 1. A draw mode (page
// 0xB5) linked at the point (0xC), the lit wall, then a grey semi-transparent
// POLY_F4 (shade +9 * 0x1E) over the wall's vertices, linked (0x38); a call to
// the line.
extern "C" void __cdecl EffectKind18Sub50_Flash(void) {
    static const char kWho[] = "EffectKind18Sub50_Flash";
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    if (s[9] == 0) s[2] = 1;
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0xB5, 0);
    LinkAtRecord(0xC);
    s = S();
    const unsigned char flag = Sub50Byte(kWho, ::bof3::Img(at::kSub50Lights.v), s[0xB]);
    const U light = SH_CALL(Flags_Test)(At(::bof3::Img(Cond_Flags_addr) + 0x48), flag) & 0xFFu;
    s = S();
    SH_CALL(EffectKind18Sub50_DrawWall)(s[0xB] + light * 4u + 1u, 0x40);
    unsigned char* const prim = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyF4)(prim);
    SH_CALL(Gpu_SetSemiTrans)(prim, 1);
    const unsigned char shade = static_cast<unsigned char>(S()[9] * 0x1E);
    prim[6] = shade;
    prim[5] = shade;
    prim[4] = shade;
    Project4Effect5c(prim, 8, 0x14, 0x20, 0x2C);
    SH_CALL(Gte_PrimDepths4_0C)(prim);
    LinkAtRecord(0x38);
    SH_CALL(EffectKind18Sub50_DrawLine)();
}

// original 0x501850: the wall - x the record's x cell (+0x36 << 7 - 0x3FC0),
// z from +0x3A << 7 - 0x4008 + height to - 0x3FF8 - height, each end's bottom
// and top 8 - height and + height off -(the ground / 2) - +0x3E - into
// Prim_VertexScratch; a textured POLY_FT4 (texture + 0x285000F3), linked at the
// point (0x48).
extern "C" void __cdecl EffectKind18Sub50_DrawWall(unsigned texture, unsigned height) {
    unsigned char* const s = S();
    const U b = height;
    const U x = (static_cast<U>(rv::As<records::EffectObject>(s)->x_whole) << 7) - 0x3FC0u;
    SetWord(VSEffect5c(0x18), x);
    SetWord(VSEffect5c(0x10), x);
    SetWord(VSEffect5c(8), x);
    SetWord(VSEffect5c(0), x);
    const U z0 = (static_cast<U>(rv::As<records::EffectObject>(s)->z_whole) << 7) + b - 0x4008u;
    const U ex = CellOf(x);
    const U ez = CellOf(z0);
    SetWord(VSEffect5c(0x12), z0);
    SetWord(VSEffect5c(2), z0);
    const U z1 = (static_cast<U>(rv::As<records::EffectObject>(s)->z_whole) << 7) - b - 0x3FF8u;
    SetWord(VSEffect5c(0x1A), z1);
    SetWord(VSEffect5c(0xA), z1);
    long ground = SH_CALL(AreaMap_Elevation)(static_cast<long>(ex), static_cast<long>(ez));
    SetWord(VSEffect5c(4), (0u - HalfOf(ground)) - rv::As<records::EffectObject>(S())->height_whole - b + 8u);
    ground = SH_CALL(AreaMap_Elevation)(static_cast<long>(ex), static_cast<long>(ez));
    const U ez2 = VertexCell(0xA);
    const U ex2 = VertexCell(8);
    SetWord(VSEffect5c(0x14), (0u - HalfOf(ground)) - rv::As<records::EffectObject>(S())->height_whole + b);
    ground = SH_CALL(AreaMap_Elevation)(static_cast<long>(ex2), static_cast<long>(ez2));
    SetWord(VSEffect5c(0xC), (0u - HalfOf(ground)) - rv::As<records::EffectObject>(S())->height_whole - b + 8u);
    ground = SH_CALL(AreaMap_Elevation)(static_cast<long>(ex2), static_cast<long>(ez2));
    SetWord(VSEffect5c(0x1C), (0u - HalfOf(ground)) - rv::As<records::EffectObject>(S())->height_whole + b);
    unsigned char* const prim = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyFT4)(prim);
    SH_CALL(Gpu_SetShadeTex)(prim, 0);
    Project4Effect5c(prim, 8, 0x18, 0x28, 0x38);
    SH_CALL(Gte_PrimDepths4_10)(prim);
    SH_CALL(Prim_SetTexture)(texture + at::kTexture50, prim, 1);
    LinkAtRecord(0x48);
}

// original 0x501A10: a draw mode (page 0x95, dtd) linked at the point (0xC); a
// semi-transparent LINE_F2 (shade 0 or 0x80 by Frame_Counter's bit 0) from
// vertex a0 to a8, each end 2 * (Frame_Counter % 48) - 0x3C - +0x3E - the
// ground / 2 high, projected with b0 (Gte_RotTransPers3, the third point into
// MapView_ScreenXY), linked (0x20); a draw mode (page 0x95) linked (0xC).
extern "C" void __cdecl EffectKind18Sub50_DrawLine(void) {
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x95, 0);
    LinkAtRecord(0xC);
    unsigned char* const prim = Gfx_PacketNext;
    SH_CALL(Gpu_SetLineF2)(prim);
    SH_CALL(Gpu_SetSemiTrans)(prim, 1);
    const unsigned char shade = static_cast<unsigned char>((Frame_Counter & 1u) << 7);
    prim[6] = shade;
    prim[5] = shade;
    prim[4] = shade;
    long ground = SH_CALL(AreaMap_Elevation)(static_cast<long>(VertexCell(0)), static_cast<long>(VertexCell(2)));
    U rise = 2u * (Frame_Counter % 0x30u) - 0x3Cu;
    const U ex2 = VertexCell(8);
    const U ez2 = VertexCell(0xA);
    SetWord(VSEffect5c(4), rise - rv::As<records::EffectObject>(S())->height_whole - HalfOf(ground));
    ground = SH_CALL(AreaMap_Elevation)(static_cast<long>(ex2), static_cast<long>(ez2));
    rise = 2u * (Frame_Counter % 0x30u) - 0x3Cu;
    SetWord(VSEffect5c(0xC), rise - rv::As<records::EffectObject>(S())->height_whole - HalfOf(ground));
    long depth = 0;
    SH_CALL(Gte_RotTransPers3)(VX(0), VX(8), VX(0x10), F(prim + 8), F(prim + 0x14), MapView_ScreenXY, &depth);
    float third = 0;
    SH_CALL(Gte_StoreDepthF3)(F(prim + 0x10), F(prim + 0x1C), &third);
    LinkAtRecord(0x20);
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x95, 0);
    LinkAtRecord(0xC);
}

void EffectKind18Sub0f_Inject() {
    BOF3_INJECT(EffectKind18Sub0F_Run);
    BOF3_INJECT(EffectKind18Sub0F_Start);
    BOF3_INJECT(EffectKind18Sub0F_Wait);
    BOF3_INJECT(EffectKind18Sub0F_Lower);
    BOF3_INJECT(EffectKind18Sub0F_Open);
    BOF3_INJECT(EffectKind18Sub0F_Raise);
    BOF3_INJECT(EffectKind18Sub0F_Closed);
    BOF3_INJECT(EffectKind18Sub0F_DrawGate);
    BOF3_INJECT(EffectKind18Sub4F_Run);
    BOF3_INJECT(EffectKind18Sub4F_Start);
    BOF3_INJECT(EffectKind18Sub4F_Wait);
    BOF3_INJECT(EffectKind18Sub4F_Open);
    BOF3_INJECT(EffectKind18Sub50_Run);
    BOF3_INJECT(EffectKind18Sub50_Start);
    BOF3_INJECT(EffectKind18Sub50_Shine);
    BOF3_INJECT(EffectKind18Sub50_Sink);
    BOF3_INJECT(EffectKind18Sub50_WaitSet);
    BOF3_INJECT(EffectKind18Sub50_Flash);
    BOF3_INJECT(EffectKind18Sub50_DrawWall);
    BOF3_INJECT(EffectKind18Sub50_DrawLine);
}

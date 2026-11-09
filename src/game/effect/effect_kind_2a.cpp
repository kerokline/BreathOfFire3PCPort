// Moved here by tools/move_funcs.py from effect_2a.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_2a.h"

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

unsigned char* S() { return Sprite_Current; }

U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }

std::int32_t S32(U v) { return static_cast<std::int32_t>(v); }

U AddressOf(const void* p) { return Low32(p); }

// sar: an arithmetic right shift of the 32 bits as the original holds them.
U Sar(U v, unsigned n) { return static_cast<U>(static_cast<std::int32_t>(v) >> n); }

float F(const unsigned char* p) {
    float f;
    std::memcpy(&f, p, sizeof f);
    return f;
}

double D(const unsigned char* p) { return static_cast<double>(F(p)); }

void SetF(unsigned char* p, float f) { std::memcpy(p, &f, sizeof f); }

// The CRT's _ftol 0x5B9550 on the value x87 holds: truncation through a 64-bit
// fistp, whose NaN and out-of-range answer is the integer indefinite
// 0x8000000000000000; the callers keep eax, its low 32 bits.
U Ftol(double v) {
    if (!(v > -9.2233720368547758e18 && v < 9.2233720368547758e18)) return 0;
    return static_cast<U>(static_cast<std::uint64_t>(static_cast<std::int64_t>(v)));
}

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

// A draw mode for the page (x, y) with transparency `abr`, and `dtd` its
// dither flag: Gpu_GetTPage(0, abr, x, y), Gpu_SetDrawMode(the packet cursor,
// 0, dtd, the page's low word, 0).
void DrawMode(unsigned abr, int x, int y, int dtd) {
    const unsigned tpage = SH_CALL(Gpu_GetTPage)(0, abr, x, y);
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, dtd, tpage & 0xFFFFu, 0);
}

void Shade3(unsigned char* v, unsigned char value) { v[0] = v[1] = v[2] = value; }

using EndFn = void (__cdecl*)(unsigned, unsigned, unsigned, unsigned);

using SidesFn = void (__cdecl*)(unsigned, unsigned, unsigned, unsigned, unsigned, unsigned, unsigned, unsigned);

// The colour of Sprite_Current's +6 (Sprite_Current read for each byte): the
// three bytes at table + 3 * +6, read in place - a +6 past the table reads the
// bytes after it, which the original does too (image data, no fault).
void BeamColour(unsigned char* v, U table) {
    for (unsigned k = 0; k < 3; ++k) v[k] = At(table + 3u * S()[6] + k)[0];
}

// original 0x470470 / 0x4713C0: see EffectKind28_DrawBeam.
void DrawBeam(const long* a, const long* b, U colours, EndFn end, SidesFn sides) {
    DrawMode(1, 0x3C0, 0, 1);
    SH_CALL(Gfx_CommitPrim)(3, 0xC);
    SH_CALL(EffectGte_LoadMapCamera)();
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetLineF2)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, 0);
    SH_CALL(EffectGte_ProjectPoint)(a, reinterpret_cast<float*>(p + 8));
    SH_CALL(EffectGte_ProjectPoint)(b, reinterpret_cast<float*>(p + 0x14));
    BeamColour(p + 4, colours);
    SH_CALL(Gfx_CommitPrim)(3, 0x20);
    const U dx = Ftol(D(p + 0x14) - D(p + 8));
    const U dy = Ftol(D(p + 0x18) - D(p + 0xC));
    const U angle = static_cast<U>(
        SH_CALL(Math_Ratan2)(static_cast<float>(S32(dy)), static_cast<float>(S32(dx))));
    // the original's size: two words 0x40, 0 on its stack, read again for b
    short size[2] = {0x40, 0};
    short out[2] = {0, 0};
    SH_CALL(EffectGte_ProjectSize)(a, size, out);
    // the dword of both out words, plus the frame's low bit (the callees read its low word)
    U r1;
    std::memcpy(&r1, out, sizeof r1);
    r1 += Frame_Counter & 1u;
    const U a1 = angle + 0x400u;
    {
        const U ya = Ftol(D(p + 0xC));
        const U xa = Ftol(D(p + 8));
        SH_CALL(end)(xa, ya, r1, a1);
    }
    SH_CALL(EffectGte_ProjectSize)(b, size, out);
    U r2;
    std::memcpy(&r2, out, sizeof r2);
    r2 += Frame_Counter & 1u;
    const U a2 = angle + 0xC00u;
    {
        const U yb = Ftol(D(p + 0x18));
        const U xb = Ftol(D(p + 0x14));
        SH_CALL(end)(xb, yb, r2, a2);
    }
    const U yb = Ftol(D(p + 0x18));
    const U xb = Ftol(D(p + 0x14));
    const U ya = Ftol(D(p + 0xC));
    const U xa = Ftol(D(p + 8));
    SH_CALL(sides)(xa, ya, r1, a1, xb, yb, r2, a2);
}

// original 0x470640 / 0x471590: see EffectKind28_DrawEnd.
void DrawEnd(U colours, unsigned x, unsigned y, unsigned radius, unsigned angle) {
    const U cx = static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(x)));
    const U cy = static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(y)));
    const U r = static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(radius)));
    const float fx = static_cast<float>(S32(cx));
    const float fy = static_cast<float>(S32(cy));
    U slot = angle;             // the original's argument slot: the angle whole, stepped by 0x100
    U e = angle & 0xFFFFu;      // and its low word, what Math_* are handed
    for (unsigned n = 0; n < 8; ++n) {
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG3)(p);
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        SetF(p + 8, fx);
        SetF(p + 0xC, fy);
        SetF(p + 0x18, static_cast<float>(S32(Sar(static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(e))) * r, 12) + cx)));
        SetF(p + 0x1C, static_cast<float>(S32(Sar(static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(e))) * r, 12) + cy)));
        slot += 0x100;
        e = slot & 0xFFFFu;
        SetF(p + 0x28, static_cast<float>(S32(Sar(static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(e))) * r, 12) + cx)));
        SetF(p + 0x2C, static_cast<float>(S32(Sar(static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(e))) * r, 12) + cy)));
        BeamColour(p + 4, colours);
        Shade3(p + 0x14, 0);
        Shade3(p + 0x24, 0);
        SH_CALL(Gfx_CommitPrim)(3, 0x34);
    }
}

// original 0x4707A0 / 0x4716F0: see EffectKind28_DrawSides.
void DrawSides(U colours, unsigned x0, unsigned y0, unsigned r0, unsigned a0, unsigned x1, unsigned y1, unsigned r1,
               unsigned a1) {
    auto s16 = [](unsigned v) { return static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(v))); };
    auto Cos = [](U e) { return static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(e))); };
    auto Sin = [](U e) { return static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(e))); };
    auto Round = [](U v) { return static_cast<float>(S32(v)); };
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyG4)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, 1);
    const U X0 = s16(x0), Y0 = s16(y0), X1 = s16(x1), Y1 = s16(y1), R0 = s16(r0), R1 = s16(r1);
    SetF(p + 8, Round(X0));
    SetF(p + 0xC, Round(Y0));
    SetF(p + 0x18, Round(X1));
    SetF(p + 0x1C, Round(Y1));
    const U e0 = a0 & 0xFFFFu;
    SetF(p + 0x28, Round(Sar(Cos(e0) * R0, 12) + X0));
    SetF(p + 0x2C, Round(Sar(Sin(e0) * R0, 12) + Y0));
    const U e1 = a1 & 0xFFFFu;
    const U e1b = e1 + 0x800u;   // not masked again
    SetF(p + 0x38, Round(Sar(Cos(e1b) * R1, 12) + X1));
    SetF(p + 0x3C, Round(Sar(Sin(e1b) * R1, 12) + Y1));
    BeamColour(p + 4, colours);
    BeamColour(p + 0x14, colours);
    Shade3(p + 0x24, 0);
    Shade3(p + 0x34, 0);
    SH_CALL(Gfx_CommitPrim)(3, 0x44);
    // the second quad right after the first, a copy of it (not the cursor read again)
    unsigned char* const q = p + 0x44;
    std::memcpy(q, p, 0x44);
    const U e0b = e0 + 0x800u;   // not masked again
    SetF(q + 0x28, Round(Sar(Cos(e0b) * R0, 12) + X0));
    SetF(q + 0x2C, Round(Sar(Sin(e0b) * R0, 12) + Y0));
    SetF(q + 0x38, Round(Sar(Cos(e1) * R1, 12) + X1));
    SetF(q + 0x3C, Round(Sar(Sin(e1) * R1, 12) + Y1));
    SH_CALL(Gfx_CommitPrim)(3, 0x44);
}

// original 0x4703F0: see EffectKind28_PushParty.
bool PushBlocked() { return ((Field_ScriptFlags2 >> 8) & 0x14u) != 0 || Field_Request != 0; }

unsigned char* Member(const char* who, unsigned m) {
    if (m >= at::kMembers)
        bof3::Fatal("%s: Party_MemberAt answered %u - the original indexes ObjTrio past its three records "
                    "(docs/effect_2a.md section 2)",
                    who, m);
    return At(::bof3::Img(ObjTrio_addr) + at::kObjStride * m);
}

// Kind 0x2A's row: the ground under +0xC / +0x10, both points' heights 0x100
// above it, the beam drawn and the row pushed.
void Row2A() {
    const unsigned char* const s = S();
    const long ground = SH_CALL(AreaMap_Elevation)(static_cast<std::int32_t>(rv::As<records::EffectWork>(s)->work_0c), static_cast<std::int32_t>(rv::As<records::EffectWork>(s)->work_10));
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->work_20, (static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(ground))) + 0x100u) << 16);
    unsigned char* const t = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(t)->step_y, static_cast<std::uint32_t>(rv::As<records::EffectObject>(t)->work_20));
    const unsigned char* const u = S();
    SH_CALL(EffectKind2A_DrawBeam)(reinterpret_cast<const long*>(u + 0xC), reinterpret_cast<const long*>(u + 0x18));
    SH_CALL(EffectKind2A_PushParty)();
}

void RowPoints2A(U z) {
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_0c, 0x608000);
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->work_18, 0x6B8000);
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->work_1c, z);
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_10, z);
}

}  // namespace

// original 0x4710A0: jmp [EffectKind2A_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind2A_Run(void) { Dispatch("EffectKind2A_Run", AddressOf(EffectKind2A_States), EffectKind2A_States_count); }

// original 0x4710C0 (state 0): +1 = 2 while story flag 0xD is clear, 1 once it
// is set; the dword +0x3C = 0x1000000 either way.
extern "C" void __cdecl EffectKind2A_Start(void) {
    const unsigned char set = SH_CALL(Flags_Test)(At(::bof3::Img(Cond_Flags_addr) + 0xa0), 0xD);
    S()[1] = set == 0 ? 2 : 1;
    rv::Current<records::EffectObject>()->height = 0x1000000;
}

// original 0x471110 (state 1): +6 = 1; the beam from (0x608000, 0x118000) to
// (0x6B8000, 0x118000) at 0x100 above the ground under the first, drawn, the row
// of +6 pushed; while story flag 0xD is clear, +1 = 2.
extern "C" void __cdecl EffectKind2A_Beam(void) {
    S()[6] = 1;
    RowPoints2A(0x118000);
    Row2A();
    if (SH_CALL(Flags_Test)(At(::bof3::Img(Cond_Flags_addr) + 0xa0), 0xD) == 0) S()[1] = 2;
}

// original 0x4711B0 (state 2): the same beam at z 0xD8000 (+6 = 0) and at
// 0x158000 (+6 = 2), each drawn and its row pushed; once story flag 0xD is set,
// +1 = 1.
extern "C" void __cdecl EffectKind2A_Beams(void) {
    S()[6] = 0;
    RowPoints2A(0xD8000);
    Row2A();
    S()[6] = 2;
    RowPoints2A(0x158000);
    Row2A();
    if (SH_CALL(Flags_Test)(At(::bof3::Img(Cond_Flags_addr) + 0xa0), 0xD) != 0) S()[1] = 1;
}

// original 0x4712E0: for the ten x cells 0x61..0x6A (each + 0x8000, the cell's
// middle): Sprite_Current's +0x34 = it, +0x38 = the z cell of +6's row
// (0x654344 + 2 * +6, << 16); Party_MemberAt(+0x34, +0x38, 0); a member there,
// with Field_ScriptFlags2's bits 0x1400 clear and Field_Request 0: its ObjTrio
// record's +8 = 5 when the row lies past the member's z (+0x38, signed), else
// 1; and when the record's +1 is 1, Member_SetState2_8(the member, the row's
// second byte).
extern "C" void __cdecl EffectKind2A_PushParty(void) {
    U x = 0x610000;
    for (unsigned i = 0; i < 10; ++i, x += 0x10000) {
        rv::Current<records::EffectObject>()->x = x | 0x8000u;
        unsigned char* const s = S();
        auto* const so = rv::As<records::EffectObject>(s);
        so->z = static_cast<U>(At(::bof3::Img(at::kPushRows2A.v) + 2u * s[6])[0]) << 16;
        const unsigned char* const t = S();
        auto* const to = rv::AsConst<records::EffectObject>(t);
        const unsigned char m = SH_CALL(Party_MemberAt)(to->x, to->z, 0);
        if (m == 0xFF || PushBlocked()) continue;
        unsigned char* const member = Member("EffectKind2A_PushParty", m);
        auto* const membero = rv::As<records::PartyMember>(member);
        const std::int32_t past = static_cast<std::int32_t>(static_cast<std::uint32_t>(rv::Current<records::EffectObject>()->z) - static_cast<std::uint32_t>(membero->z));
        member[8] = past > 0 ? 5 : 1;
        if (member[1] != 1) continue;
        SH_CALL(Member_SetState2_8)(m, At(::bof3::Img(at::kPushRows2A.v) + 2u * S()[6] + 1)[0]);
    }
}

// original 0x4713C0 (0x1CC bytes): EffectKind28_DrawBeam's copy - its colours
// 0x654328, its end and side draws EffectKind2A_DrawEnd / _DrawSides.
extern "C" void __cdecl EffectKind2A_DrawBeam(const long* a, const long* b) {
    DrawBeam(a, b, ::bof3::Img(at::kBeamColours2A.v), &EffectKind2A_DrawEnd, &EffectKind2A_DrawSides);
}

// original 0x471590 (0x160 bytes): EffectKind28_DrawEnd's copy (colours 0x654328).
extern "C" void __cdecl EffectKind2A_DrawEnd(unsigned x, unsigned y, unsigned radius, unsigned angle) {
    DrawEnd(::bof3::Img(at::kBeamColours2A.v), x, y, radius, angle);
}

// original 0x4716F0 (0x234 bytes): EffectKind28_DrawSides' copy (colours 0x654328).
extern "C" void __cdecl EffectKind2A_DrawSides(unsigned x0, unsigned y0, unsigned r0, unsigned a0, unsigned x1,
                                               unsigned y1, unsigned r1, unsigned a1) {
    DrawSides(::bof3::Img(at::kBeamColours2A.v), x0, y0, r0, a0, x1, y1, r1, a1);
}

void EffectKind2a_Inject() {
    BOF3_INJECT(EffectKind2A_Run);
    BOF3_INJECT(EffectKind2A_Start);
    BOF3_INJECT(EffectKind2A_Beam);
    BOF3_INJECT(EffectKind2A_Beams);
    BOF3_INJECT(EffectKind2A_PushParty);
    BOF3_INJECT(EffectKind2A_DrawBeam);
    BOF3_INJECT(EffectKind2A_DrawEnd);
    BOF3_INJECT(EffectKind2A_DrawSides);
}

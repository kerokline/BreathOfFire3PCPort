// Moved here by tools/move_funcs.py from effect_2a.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_28.h"

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

}  // namespace

// original 0x470300 (0x12 bytes): jmp [EffectKind28_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind28_Run(void) { Dispatch("EffectKind28_Run", AddressOf(EffectKind28_States), EffectKind28_States_count); }

// original 0x470320 (state 0): the ground under the point +0xC / +0x10
// (AreaMap_Elevation(x, z)); both points' heights +0x14 / +0x20 = (its low word,
// signed, + 0x100) << 16, the word +0x3E its low word, the dword +0x70 0 (each on
// Sprite_Current as read after the call). Then +1 = 1 while story flag 0xB is
// clear, 2 once it is set.
extern "C" void __cdecl EffectKind28_Start(void) {
    const unsigned char* const s = S();
    const long ground = SH_CALL(AreaMap_Elevation)(static_cast<std::int32_t>(rv::As<records::EffectWork>(s)->work_0c), static_cast<std::int32_t>(rv::As<records::EffectWork>(s)->work_10));
    const U height = (static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(ground))) + 0x100u) << 16;
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->step_y, height);
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->work_20, height);
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->height_whole, static_cast<U>(ground));
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->reach, 0);
    const unsigned char set = SH_CALL(Flags_Test)(At(::bof3::Img(Cond_Flags_addr) + 0xa0), 0xB);
    S()[1] = set == 0 ? 1 : 2;
}

// original 0x470390 (state 1): once story flag 0xB is set, +1 = 0 (back to the
// start).
extern "C" void __cdecl EffectKind28_WaitFlag(void) {
    if (SH_CALL(Flags_Test)(At(::bof3::Img(Cond_Flags_addr) + 0xa0), 0xB) != 0) S()[1] = 0;
}

// original 0x4703B0 (state 2): the beam from +0xC to +0x18 drawn, the row
// pushed; while story flag 0xB is clear, +1 = 0 (the flag's answer, 0).
extern "C" void __cdecl EffectKind28_Beam(void) {
    const unsigned char* const s = S();
    SH_CALL(EffectKind28_DrawBeam)(reinterpret_cast<const long*>(s + 0xC), reinterpret_cast<const long*>(s + 0x18));
    SH_CALL(EffectKind28_PushParty)();
    if (SH_CALL(Flags_Test)(At(::bof3::Img(Cond_Flags_addr) + 0xa0), 0xB) == 0) S()[1] = 0;
}

// original 0x4703F0: for each of the eight x cells of the byte table 0x654308:
// Sprite_Current's +0x34 = the cell << 16, +0x38 = 0x100000 (the cell z 0x10);
// Party_MemberAt(+0x34, +0x38, 0); a member there, with Field_ScriptFlags2's
// bits 0x1400 clear and Field_Request 0: Member_SetState2_8(the member, 2).
// The record's point is left at the last cell.
extern "C" void __cdecl EffectKind28_PushParty(void) {
    for (unsigned i = 0; i < 8; ++i) {
        rv::Store<U>(rv::Current<records::EffectObject>()->x, static_cast<U>(At(::bof3::Img(at::kPushCells28.v))[i]) << 16);
        rv::Current<records::EffectObject>()->z = 0x100000;
        const unsigned char* const s = S();
        auto* const so = rv::AsConst<records::EffectObject>(s);
        const unsigned char m = SH_CALL(Party_MemberAt)(so->x, so->z, 0);
        if (m == 0xFF || PushBlocked()) continue;
        SH_CALL(Member_SetState2_8)(m, 2);
    }
}

// original 0x470470 (0x1CC bytes): the beam from `a` to `b`. A draw mode (page
// 0x3C0, 0, transparency 1) committed in slot 3 and EffectGte_LoadMapCamera; a
// flat line at the packet cursor (Gpu_SetLineF2, opaque) from a's screen point to
// b's (EffectGte_ProjectPoint each, straight into the line), coloured by +6
// (EffectKind28's three-byte colours 0x6542EC), committed 0x20 in slot 3. Then
// the angle Math_Ratan2(dy, dx) of the screen difference (each through _ftol and
// back to a float); at each end, its size EffectGte_ProjectSize(end, {0x40, 0})
// plus the frame's low bit, a half-fan EffectKind28_DrawEnd(x, y, size, angle +
// 0x400 at a, + 0xC00 at b), the screen words through _ftol; then
// EffectKind28_DrawSides between the two ends.
extern "C" void __cdecl EffectKind28_DrawBeam(const long* a, const long* b) {
    DrawBeam(a, b, ::bof3::Img(at::kBeamColours28.v), &EffectKind28_DrawEnd, &EffectKind28_DrawSides);
}

// original 0x470640 (0x160 bytes): eight semi-transparent Gouraud triangles at
// the packet cursor (read afresh), each committed 0x34 in slot 3: the centre (x,
// y) as floats, coloured by +6 (0x6542EC), and two rim points (x + (cos(u) *
// radius sar 12), y + (sin(u) * radius sar 12)) shaded 0, for u = the angle's
// low word and the next 0x100 on (the angle stepped whole, masked for each
// call): half a circle. x, y, radius are read as s16.
extern "C" void __cdecl EffectKind28_DrawEnd(unsigned x, unsigned y, unsigned radius, unsigned angle) {
    DrawEnd(::bof3::Img(at::kBeamColours28.v), x, y, radius, angle);
}

// original 0x4707A0 (0x234 bytes): at the packet cursor a semi-transparent
// Gouraud quad from (x0, y0), (x1, y1) (coloured by +6, 0x6542EC) to the circle
// points (x0 + cos(a0) r0 sar 12, y0 + sin(a0) r0 sar 12) and (x1, y1) likewise
// at a1 + 0x800 (shaded 0), committed 0x44 in slot 3; then a copy of it right
// after (p + 0x44, not the cursor) with its far points at a0 + 0x800 and a1,
// committed 0x44. Every argument read as its low word (x, y, r signed); the
// angles' + 0x800 not masked again.
extern "C" void __cdecl EffectKind28_DrawSides(unsigned x0, unsigned y0, unsigned r0, unsigned a0, unsigned x1,
                                               unsigned y1, unsigned r1, unsigned a1) {
    DrawSides(::bof3::Img(at::kBeamColours28.v), x0, y0, r0, a0, x1, y1, r1, a1);
}

void EffectKind28_Inject() {
    BOF3_INJECT(EffectKind28_Run);
    BOF3_INJECT(EffectKind28_Start);
    BOF3_INJECT(EffectKind28_WaitFlag);
    BOF3_INJECT(EffectKind28_Beam);
    BOF3_INJECT(EffectKind28_PushParty);
    BOF3_INJECT(EffectKind28_DrawBeam);
    BOF3_INJECT(EffectKind28_DrawEnd);
    BOF3_INJECT(EffectKind28_DrawSides);
}

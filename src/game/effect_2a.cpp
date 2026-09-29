// Effect kinds 0x28..0x2E - round thirteen, wave two, group E2A: the 64
// functions of the cut (analysis/round13_cut.tsv) in 0x470300..0x473192 and
// the kind-0x2B ring draw 0x471A60 no list held, each read with capstone to
// its last instruction (docs/effect_2a.md section 1). Effect_RunObjects (ours)
// makes each live record of Effect_Objects (20 of 0x80 bytes) Sprite_Current
// and calls Effect_KindHandlers[+5]; each kind here is a dispatcher by +1
// through its state table (none bounded by a compare) and the states it names:
//
//   kind 0x28  a beam between two ground points (+0xC, +0x18; area 42's init
//              spawns it with +6 = 2), drawn while story flag 0xB is clear, a
//              party member on its row of eight cells set to state 2;
//   kind 0x29  two boxes of five faces and five segments growing, then fading;
//   kind 0x2A  kind 0x28's beam (its own copy of the draw) at one row or three,
//              by story flag 0xD, pushing a member off each row;
//   kind 0x2B  a ring on the ground that rises, throws specks until the
//              counter byte reaches 0x29, settles and shrinks (chapter 9's run
//              11 spawns it);
//   kind 0x2C  sparks thrown from Sprite_ObjectsExtra[0] toward eight ground
//              points, one wave every 16 frames for 0x200 frames;
//   kind 0x2D  rays, a disc, rings and a curtain at the screen point between
//              Sprite_Objects[0] and [1], and drops (area 67's handler 2
//              spawns it) - a message opened at its start, the counter byte
//              set 0x31 and 0x32 as it closes;
//   kind 0x2E  sparks from Sprite_Objects[1]: one every eight frames until the
//              counter byte is 3, then a burst of sixteen each frame.
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies. No divergence:
// each is a faithful replacement. Where the original jumps through a state
// table past its end, indexes a table of its own past its end, or divides by
// a word the harness could zero between the test and the divide, ours aborts
// with a message (docs/effect_2a.md section 2).
//
// x87: the game runs its FPU at 53 bits (psx_gte_float.cpp); every fadd / fsub /
// fmul here is written in double (SSE2, the same rounding) and stored to a float
// once, as the original's fst does. A float the original only loads and stores
// (fld / fstp) goes through Quiet(): x87 turns a signalling NaN quiet on the load.
#include "game/effect_2a.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_2a_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_2a::at;
using U = std::uint32_t;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

unsigned char* S() { return Sprite_Current; }
U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }
void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }
std::int32_t SW(const unsigned char* p) { return static_cast<std::int16_t>(Word(p)); }
std::int32_t S32(U v) { return static_cast<std::int32_t>(v); }
void AddWord(unsigned char* p, U d) { SetWord(p, Word(p) + d); }
U AddressOf(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
// sar: an arithmetic right shift of the 32 bits as the original holds them.
U Sar(U v, unsigned n) { return static_cast<U>(static_cast<std::int32_t>(v) >> n); }
// cdq; and edx, 7; add eax, edx; sar eax, 3: a signed divide by 8, toward zero.
U Div8(U v) { return Sar(v + (static_cast<std::int32_t>(v) < 0 ? 7u : 0u), 3); }
float F(const unsigned char* p) {
    float f;
    std::memcpy(&f, p, sizeof f);
    return f;
}
double D(const unsigned char* p) { return static_cast<double>(F(p)); }
void SetF(unsigned char* p, float f) { std::memcpy(p, &f, sizeof f); }
void SetD(unsigned char* p, double d) { SetF(p, static_cast<float>(d)); }
// fld dword / fstp dword: the bits kept, but a signalling NaN made quiet.
void CopyQuiet(unsigned char* to, const unsigned char* from) {
    U bits = UL(from);
    if ((bits & 0x7F800000u) == 0x7F800000u && (bits & 0x007FFFFFu) != 0) bits |= 0x00400000u;
    SetUL(to, bits);
}
// fild dword of a whole number: (double) of it, exact.
double I(U v) { return static_cast<double>(static_cast<std::int32_t>(v)); }

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

// +1 up, Sprite_Current read afresh.
void NextState() {
    unsigned char* const s = S();
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// +9 down one (Sprite_Current read for each access, as the originals); at 0,
// +9 = `reload` and +1 up - or, when `reload` is negative, +1 up with +9 left 0.
void CountDown(int reload) {
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    s = S();
    if (s[9] != 0) return;
    if (reload >= 0) {
        s[9] = static_cast<unsigned char>(reload);
        s = S();
    }
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// A draw mode for the page (x, y) with transparency `abr`, and `dtd` its
// dither flag: Gpu_GetTPage(0, abr, x, y), Gpu_SetDrawMode(the packet cursor,
// 0, dtd, the page's low word, 0).
void DrawMode(unsigned abr, int x, int y, int dtd) {
    const unsigned tpage = SH_CALL(Gpu_GetTPage)(0, abr, x, y);
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, dtd, tpage & 0xFFFFu, 0);
}

void Shade3(unsigned char* v, unsigned char value) { v[0] = v[1] = v[2] = value; }

// ===========================================================================
// Kinds 0x28 and 0x2A's beam: one draw, two copies in the image (0x470470 /
// 0x4713C0 and their callees differ only in the colour table and which copy
// they call)
// ===========================================================================

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
    return At(at::kObjTrio + at::kObjStride * m);
}

// Kind 0x2A's row: the ground under +0xC / +0x10, both points' heights 0x100
// above it, the beam drawn and the row pushed.
void Row2A() {
    const unsigned char* const s = S();
    const long ground = SH_CALL(AreaMap_Elevation)(Long(s + 0xC), Long(s + 0x10));
    SetUL(S() + 0x20, (static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(ground))) + 0x100u) << 16);
    unsigned char* const t = S();
    SetUL(t + 0x14, UL(t + 0x20));
    const unsigned char* const u = S();
    SH_CALL(EffectKind2A_DrawBeam)(reinterpret_cast<const long*>(u + 0xC), reinterpret_cast<const long*>(u + 0x18));
    SH_CALL(EffectKind2A_PushParty)();
}

void RowPoints2A(U z) {
    SetUL(S() + 0xC, 0x608000);
    SetUL(S() + 0x18, 0x6B8000);
    SetUL(S() + 0x1C, z);
    SetUL(S() + 0x10, z);
}

// ===========================================================================
// Kind 0x2D's draws: the link cell and the record's screen point
// ===========================================================================

U LinkX() { return UL(At(at::kLinkX)); }
U LinkZ() { return UL(At(at::kLinkZ)); }
// A draw mode for the page (0x380, 0x100) linked at kind 0x2D's cell.
void LinkedMode(unsigned abr, int dtd) {
    DrawMode(abr, 0x380, 0x100, dtd);
    SH_CALL(MapView_LinkPrimAt)(LinkX(), LinkZ(), 4, 0xC);
}

// The pool's records.
unsigned char* Pool(U stride, unsigned i) { return At(at::kPool + stride * i); }

}  // namespace

// ===========================================================================
// Kind 0x28: Effect_KindHandlers[0x28] (0x6553F0), EffectKind28_States (four,
// the last FC1's Effect_StateRelease)
// ===========================================================================

// original 0x470300 (0x12 bytes): jmp [EffectKind28_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind28_Run(void) { Dispatch("EffectKind28_Run", AddressOf(EffectKind28_States), EffectKind28_States_count); }

// original 0x470320 (state 0): the ground under the point +0xC / +0x10
// (AreaMap_Elevation(x, z)); both points' heights +0x14 / +0x20 = (its low word,
// signed, + 0x100) << 16, the word +0x3E its low word, the dword +0x70 0 (each on
// Sprite_Current as read after the call). Then +1 = 1 while story flag 0xB is
// clear, 2 once it is set.
extern "C" void __cdecl EffectKind28_Start(void) {
    const unsigned char* const s = S();
    const long ground = SH_CALL(AreaMap_Elevation)(Long(s + 0xC), Long(s + 0x10));
    const U height = (static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(ground))) + 0x100u) << 16;
    SetUL(S() + 0x14, height);
    SetUL(S() + 0x20, height);
    SetWord(S() + 0x3E, static_cast<U>(ground));
    SetUL(S() + 0x70, 0);
    const unsigned char set = SH_CALL(Flags_Test)(At(at::kStoryFlags), 0xB);
    S()[1] = set == 0 ? 1 : 2;
}

// original 0x470390 (state 1): once story flag 0xB is set, +1 = 0 (back to the
// start).
extern "C" void __cdecl EffectKind28_WaitFlag(void) {
    if (SH_CALL(Flags_Test)(At(at::kStoryFlags), 0xB) != 0) S()[1] = 0;
}

// original 0x4703B0 (state 2): the beam from +0xC to +0x18 drawn, the row
// pushed; while story flag 0xB is clear, +1 = 0 (the flag's answer, 0).
extern "C" void __cdecl EffectKind28_Beam(void) {
    const unsigned char* const s = S();
    SH_CALL(EffectKind28_DrawBeam)(reinterpret_cast<const long*>(s + 0xC), reinterpret_cast<const long*>(s + 0x18));
    SH_CALL(EffectKind28_PushParty)();
    if (SH_CALL(Flags_Test)(At(at::kStoryFlags), 0xB) == 0) S()[1] = 0;
}

// original 0x4703F0: for each of the eight x cells of the byte table 0x654308:
// Sprite_Current's +0x34 = the cell << 16, +0x38 = 0x100000 (the cell z 0x10);
// Party_MemberAt(+0x34, +0x38, 0); a member there, with Field_ScriptFlags2's
// bits 0x1400 clear and Field_Request 0: Member_SetState2_8(the member, 2).
// The record's point is left at the last cell.
extern "C" void __cdecl EffectKind28_PushParty(void) {
    for (unsigned i = 0; i < 8; ++i) {
        SetUL(S() + 0x34, static_cast<U>(At(at::kPushCells28)[i]) << 16);
        SetUL(S() + 0x38, 0x100000);
        const unsigned char* const s = S();
        const unsigned char m = SH_CALL(Party_MemberAt)(Long(s + 0x34), Long(s + 0x38), 0);
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
    DrawBeam(a, b, at::kBeamColours28, &EffectKind28_DrawEnd, &EffectKind28_DrawSides);
}

// original 0x470640 (0x160 bytes): eight semi-transparent Gouraud triangles at
// the packet cursor (read afresh), each committed 0x34 in slot 3: the centre (x,
// y) as floats, coloured by +6 (0x6542EC), and two rim points (x + (cos(u) *
// radius sar 12), y + (sin(u) * radius sar 12)) shaded 0, for u = the angle's
// low word and the next 0x100 on (the angle stepped whole, masked for each
// call): half a circle. x, y, radius are read as s16.
extern "C" void __cdecl EffectKind28_DrawEnd(unsigned x, unsigned y, unsigned radius, unsigned angle) {
    DrawEnd(at::kBeamColours28, x, y, radius, angle);
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
    DrawSides(at::kBeamColours28, x0, y0, r0, a0, x1, y1, r1, a1);
}

// ===========================================================================
// Kind 0x29: Effect_KindHandlers[0x29] (0x6553F4), EffectKind29_States (six,
// the last Effect_StateRelease)
// ===========================================================================

// original 0x4709E0: jmp [EffectKind29_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind29_Run(void) { Dispatch("EffectKind29_Run", AddressOf(EffectKind29_States), EffectKind29_States_count); }

// original 0x470A00 (state 0): two boxes from the record + 0xC (Sprite_Current
// read once): box A's +0xC / +0x10 / +0x14 = (0x568000, 0x128000, 0x1000000),
// its half sizes +0x1C = 0 and +0x20 = 0x2200000; box B's +0x24 / +0x28 / +0x2C
// the same point, +0x34 = 0x4000 and +0x38 = 0x2200000 (+0x18 and +0x30 not
// written). EffectKind29_SetSegments; +9 = 8, +1 up.
extern "C" void __cdecl EffectKind29_Start(void) {
    unsigned char* const q = S() + 0xC;
    SetUL(q, 0x568000);
    SetUL(q + 4, 0x128000);
    SetUL(q + 8, 0x1000000);
    SetUL(q + 0x10, 0);
    SetUL(q + 0x14, 0x2200000);
    unsigned char* const r = q + 0x18;
    SetUL(r, 0x568000);
    SetUL(r + 4, 0x128000);
    SetUL(r + 8, 0x1000000);
    SetUL(r + 0x10, 0x4000);
    SetUL(r + 0x14, 0x2200000);
    SH_CALL(EffectKind29_SetSegments)();
    S()[9] = 8;
    NextState();
}

// original 0x470A70 (state 1): box B moved: x +0x24 + 0x10000, half height +0x38
// + 0x220000, half depth +0x34 + 0x2000 (all read before any is written). The
// segments drawn to t = 0x10 - 2 * +9 (a byte; the pushed dword's upper bytes
// the new +0x34's), shade 0xFF; the boxes drawn (the record + 0xC taken on
// entry); +9 down, at 0 +9 = 0x40 and +1 up.
extern "C" void __cdecl EffectKind29_Extend(void) {
    unsigned char* const s = S();
    unsigned char* const q = s + 0xC;
    const U x = UL(s + 0x24) + 0x10000u;
    const U h = UL(s + 0x38) + 0x220000u;
    const U d = UL(q + 0x28) + 0x2000u;
    SetUL(q + 0x18, x);
    SetUL(q + 0x2C, h);
    SetUL(q + 0x28, d);
    const U t = (d & 0xFFFFFF00u) | static_cast<unsigned char>(0x10 - 2 * S()[9]);
    SH_CALL(EffectKind29_DrawSegments)(t, 0xFF);
    SH_CALL(EffectKind29_DrawBoxes)(q);
    CountDown(0x40);
}

// original 0x470AF0 (state 2): the boxes drawn, the segments whole (t 0x10,
// shade 0xFF); +9 down, at 0 +9 = 4 and +1 up.
extern "C" void __cdecl EffectKind29_Hold(void) {
    SH_CALL(EffectKind29_DrawBoxes)(S() + 0xC);
    SH_CALL(EffectKind29_DrawSegments)(0x10, 0xFF);
    CountDown(4);
}

// original 0x470B40 (state 3): box A's half depth +0x1C + 0x4000 and box B's
// +0x34 + 0x10000; the boxes drawn; the segments whole, shaded 0xFF at +9 = 8,
// else +9 << 5 (its low byte); +9 down, at 0 +9 = 0x20 and +1 up.
extern "C" void __cdecl EffectKind29_Fade(void) {
    unsigned char* const q = S() + 0xC;
    const U a = UL(q + 0x10) + 0x4000u;
    const U b = UL(q + 0x28) + 0x10000u;
    SetUL(q + 0x10, a);
    SetUL(q + 0x28, b);
    SH_CALL(EffectKind29_DrawBoxes)(q);
    const unsigned count = S()[9];
    SH_CALL(EffectKind29_DrawSegments)(0x10, count == 8 ? 0xFFu : count << 5);
    CountDown(0x20);
}

// original 0x470BC0 (state 4): the boxes drawn; +9 down, at 0 +1 up.
extern "C" void __cdecl EffectKind29_Linger(void) {
    SH_CALL(EffectKind29_DrawBoxes)(S() + 0xC);
    CountDown(-1);
}

// original 0x470BF0 (0x18A bytes): a draw mode (0x3C0, 0, transparency 1)
// committed 0xC in slot 1; the eight corners into 0x676080 (0x10 each): box A
// (boxes +0..+0x14: a point x, z, height and the half sizes +0x10 z, +0x14
// height) at (x, z + dz, h + dh), (x, z - dz, h + dh), (x, z + dz, h), (x, z -
// dz, h); box B (+0x18..+0x2C) likewise; EffectGte_LoadMapCamera; the five faces
// EffectKind29_DrawFace (0, 1, 2, 3 | 1, 1), (0, 1, 4, 5 | 1, 0), (2, 0, 6, 4 |
// 1, 0), (1, 3, 5, 7 | 1, 0), (2, 3, 6, 7 | 1, 0).
extern "C" void __cdecl EffectKind29_DrawBoxes(const unsigned char* boxes) {
    DrawMode(1, 0x3C0, 0, 1);
    SH_CALL(Gfx_CommitPrim)(1, 0xC);
    for (unsigned b = 0; b < 2; ++b) {
        const unsigned char* const box = boxes + 0x18 * b;
        const U x = UL(box), z = UL(box + 4), h = UL(box + 8), dz = UL(box + 0x10), dh = UL(box + 0x14);
        const U corners[4][3] = {{x, z + dz, h + dh}, {x, z - dz, h + dh}, {x, z + dz, h}, {x, z - dz, h}};
        for (unsigned c = 0; c < 4; ++c)
            for (unsigned k = 0; k < 3; ++k) SetUL(At(at::kBoxPoints + 0x10 * (4 * b + c) + 4 * k), corners[c][k]);
    }
    SH_CALL(EffectGte_LoadMapCamera)();
    SH_CALL(EffectKind29_DrawFace)(0, 1, 2, 3, 1, 1);
    SH_CALL(EffectKind29_DrawFace)(0, 1, 4, 5, 1, 0);
    SH_CALL(EffectKind29_DrawFace)(2, 0, 6, 4, 1, 0);
    SH_CALL(EffectKind29_DrawFace)(1, 3, 5, 7, 1, 0);
    SH_CALL(EffectKind29_DrawFace)(2, 3, 6, 7, 1, 0);
}

// original 0x470D80 (0xE2 bytes): a semi-transparent Gouraud quad at the packet
// cursor, its vertices EffectGte_ProjectPoint of the corners 0x676080 + 0x10 *
// (each index's low byte); shaded ((shade << 5) + the frame's low bit) << 2 (byte
// arithmetic) for the first two vertices from shade0, the last two from shade1,
// blue at half; committed 0x44 in slot 1. An index past the eight corners reads
// past the table: ours aborts there.
extern "C" void __cdecl EffectKind29_DrawFace(unsigned i0, unsigned i1, unsigned i2, unsigned i3, unsigned shade0,
                                              unsigned shade1) {
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyG4)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, 1);
    const unsigned index[4] = {i0 & 0xFFu, i1 & 0xFFu, i2 & 0xFFu, i3 & 0xFFu};
    for (unsigned k = 0; k < 4; ++k) {
        if (index[k] >= at::kBoxPointCount)
            bof3::Fatal("EffectKind29_DrawFace: corner %u is %u, past the eight at 0x676080 (docs/effect_2a.md section 2)",
                        k, index[k]);
        SH_CALL(EffectGte_ProjectPoint)(reinterpret_cast<const long*>(At(at::kBoxPoints + 0x10 * index[k])),
                                        reinterpret_cast<float*>(p + 8 + 0x10 * k));
    }
    const auto bit = static_cast<unsigned char>(Frame_Counter & 1u);
    const auto first = static_cast<unsigned char>(static_cast<unsigned char>(static_cast<unsigned char>(shade0 << 5) + bit) << 2);
    const auto second = static_cast<unsigned char>(static_cast<unsigned char>(static_cast<unsigned char>(shade1 << 5) + bit) << 2);
    p[4] = p[5] = first;
    p[6] = static_cast<unsigned char>(first >> 1);
    p[0x14] = p[0x15] = first;
    p[0x16] = static_cast<unsigned char>(first >> 1);
    p[0x24] = p[0x25] = second;
    p[0x26] = static_cast<unsigned char>(second >> 1);
    p[0x34] = p[0x35] = second;
    p[0x36] = static_cast<unsigned char>(second >> 1);
    SH_CALL(Gfx_CommitPrim)(1, 0x44);
}

// original 0x470E70 (0xFA bytes): the five segments of 0x675FE0 set - thirty
// dword stores of constants, each segment's two points (the dwords +0xC and
// +0x1C left as they are).
extern "C" void __cdecl EffectKind29_SetSegments(void) {
    struct Cell { U at, value; };
    static constexpr Cell kCells[] = {
        {0x675FE4, 0x118000},  {0x676004, 0x118000},  {0x675FE0, 0x568000},  {0x676000, 0x568000},
        {0x676008, 0x2400000}, {0x676018, 0x2400000}, {0x676020, 0x568000},  {0x676040, 0x568000},
        {0x676060, 0x568000},  {0x675FE8, 0x3000000}, {0x675FF0, 0x5E8000},  {0x675FF4, 0xF0000},
        {0x675FF8, 0x4000000}, {0x676010, 0x5E8000},  {0x676014, 0xF0000},   {0x676024, 0x138000},
        {0x676028, 0x3000000}, {0x676030, 0x5E8000},  {0x676034, 0x158000},  {0x676038, 0x4000000},
        {0x676044, 0x138000},  {0x676048, 0x2800000}, {0x676050, 0x5E8000},  {0x676054, 0x160000},
        {0x676058, 0x2C00000}, {0x676064, 0x138000},  {0x676068, 0x1200000}, {0x676070, 0x5E8000},
        {0x676074, 0x160000},  {0x676078, 0x1200000},
    };
    for (const Cell& c : kCells) SetUL(At(c.at), c.value);
}

// original 0x470F70 (0x126 bytes): a draw mode (0x3C0, 0, transparency 1)
// committed 0xC in slot 1, EffectGte_LoadMapCamera; for each of the five
// segments (A at +0, B at +0x10), a semi-transparent Gouraud line at the packet
// cursor from A's screen point to that of A + ((B - A) * t sar 4) (each
// coordinate, 32 bits), shaded ((0x20 + the frame's bit) * shade * 4) >> 8 at A
// and (the frame's bit * 4 * shade) >> 8 at the far end, blue at half;
// committed 0x24 in slot 1. t and shade are read as bytes.
extern "C" void __cdecl EffectKind29_DrawSegments(unsigned t, unsigned shade) {
    DrawMode(1, 0x3C0, 0, 1);
    SH_CALL(Gfx_CommitPrim)(1, 0xC);
    SH_CALL(EffectGte_LoadMapCamera)();
    const U tt = t & 0xFFu, level = shade & 0xFFu;
    for (unsigned k = 0; k < at::kSegmentCount; ++k) {
        const unsigned char* const a = At(at::kSegments + at::kSegmentStride * k);
        const unsigned char* const b = a + 0x10;
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetLineG2)(p);
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        long point[3];
        for (unsigned i = 0; i < 3; ++i)
            point[i] = static_cast<long>(Sar((UL(b + 4 * i) - UL(a + 4 * i)) * tt, 4) + UL(a + 4 * i));
        SH_CALL(EffectGte_ProjectPoint)(reinterpret_cast<const long*>(a), reinterpret_cast<float*>(p + 8));
        SH_CALL(EffectGte_ProjectPoint)(point, reinterpret_cast<float*>(p + 0x18));
        const U bit = Frame_Counter & 1u;
        const auto near = static_cast<unsigned char>((((bit + 0x20u) * level) << 2) >> 8);
        const auto far = static_cast<unsigned char>(((bit << 2) * level) >> 8);
        p[4] = p[5] = near;
        p[0x14] = p[0x15] = far;
        p[6] = static_cast<unsigned char>(near >> 1);
        p[0x16] = static_cast<unsigned char>(far >> 1);
        SH_CALL(Gfx_CommitPrim)(1, 0x24);
    }
}

// ===========================================================================
// Kind 0x2A: Effect_KindHandlers[0x2A] (0x6553F8), EffectKind2A_States (four,
// the last Effect_StateRelease)
// ===========================================================================

// original 0x4710A0: jmp [EffectKind2A_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind2A_Run(void) { Dispatch("EffectKind2A_Run", AddressOf(EffectKind2A_States), EffectKind2A_States_count); }

// original 0x4710C0 (state 0): +1 = 2 while story flag 0xD is clear, 1 once it
// is set; the dword +0x3C = 0x1000000 either way.
extern "C" void __cdecl EffectKind2A_Start(void) {
    const unsigned char set = SH_CALL(Flags_Test)(At(at::kStoryFlags), 0xD);
    S()[1] = set == 0 ? 2 : 1;
    SetUL(S() + 0x3C, 0x1000000);
}

// original 0x471110 (state 1): +6 = 1; the beam from (0x608000, 0x118000) to
// (0x6B8000, 0x118000) at 0x100 above the ground under the first, drawn, the row
// of +6 pushed; while story flag 0xD is clear, +1 = 2.
extern "C" void __cdecl EffectKind2A_Beam(void) {
    S()[6] = 1;
    RowPoints2A(0x118000);
    Row2A();
    if (SH_CALL(Flags_Test)(At(at::kStoryFlags), 0xD) == 0) S()[1] = 2;
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
    if (SH_CALL(Flags_Test)(At(at::kStoryFlags), 0xD) != 0) S()[1] = 1;
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
        SetUL(S() + 0x34, x | 0x8000u);
        unsigned char* const s = S();
        SetUL(s + 0x38, static_cast<U>(At(at::kPushRows2A + 2u * s[6])[0]) << 16);
        const unsigned char* const t = S();
        const unsigned char m = SH_CALL(Party_MemberAt)(Long(t + 0x34), Long(t + 0x38), 0);
        if (m == 0xFF || PushBlocked()) continue;
        unsigned char* const member = Member("EffectKind2A_PushParty", m);
        const std::int32_t past = static_cast<std::int32_t>(UL(S() + 0x38) - UL(member + 0x38));
        member[8] = past > 0 ? 5 : 1;
        if (member[1] != 1) continue;
        SH_CALL(Member_SetState2_8)(m, At(at::kPushRows2A + 2u * S()[6] + 1)[0]);
    }
}

// original 0x4713C0 (0x1CC bytes): EffectKind28_DrawBeam's copy - its colours
// 0x654328, its end and side draws EffectKind2A_DrawEnd / _DrawSides.
extern "C" void __cdecl EffectKind2A_DrawBeam(const long* a, const long* b) {
    DrawBeam(a, b, at::kBeamColours2A, &EffectKind2A_DrawEnd, &EffectKind2A_DrawSides);
}

// original 0x471590 (0x160 bytes): EffectKind28_DrawEnd's copy (colours 0x654328).
extern "C" void __cdecl EffectKind2A_DrawEnd(unsigned x, unsigned y, unsigned radius, unsigned angle) {
    DrawEnd(at::kBeamColours2A, x, y, radius, angle);
}

// original 0x4716F0 (0x234 bytes): EffectKind28_DrawSides' copy (colours 0x654328).
extern "C" void __cdecl EffectKind2A_DrawSides(unsigned x0, unsigned y0, unsigned r0, unsigned a0, unsigned x1,
                                               unsigned y1, unsigned r1, unsigned a1) {
    DrawSides(at::kBeamColours2A, x0, y0, r0, a0, x1, y1, r1, a1);
}

// ===========================================================================
// Kind 0x2B: Effect_KindHandlers[0x2B] (0x6553FC), EffectKind2B_States (five)
// ===========================================================================

// original 0x471930 (0x25 bytes): call [EffectKind2B_States + +1 * 4]
// (unbounded), then, when +1 (Sprite_Current read again) is not 0, a tail jmp
// to EffectKind2B_Draw.
extern "C" void __cdecl EffectKind2B_Run(void) {
    Dispatch("EffectKind2B_Run", AddressOf(EffectKind2B_States), EffectKind2B_States_count);
    if (S()[1] != 0) SH_CALL(EffectKind2B_Draw)();
}

// original 0x471960 (state 0): the words +0x2E (the radius) = 0x100, +0x30 (the
// rise) = 0, +0x32 (a count) = 0x18; +1 up.
extern "C" void __cdecl EffectKind2B_Start(void) {
    SetWord(S() + 0x2E, 0x100);
    SetWord(S() + 0x30, 0);
    SetWord(S() + 0x32, 0x18);
    NextState();
}

// original 0x471990 (state 1): the rise +0x30 up 0x80, the count +0x32 down; at
// 0: EffectShards_Clear (the specks freed), +0x32 = 0x12C, +1 up,
// Sound_PlayEffect(0x209).
extern "C" void __cdecl EffectKind2B_Rise(void) {
    AddWord(S() + 0x30, 0x80);
    AddWord(S() + 0x32, 0xFFFFu);
    if (Word(S() + 0x32) != 0) return;
    SH_CALL(EffectShards_Clear)();
    SetWord(S() + 0x32, 0x12C);
    NextState();
    SH_CALL(Sound_PlayEffect)(0x209);
}

// original 0x4719E0 (state 2): on odd frames a speck spawned
// (EffectSpecks_Spawn); the specks moved (EffectSpecks_Move); when the counter
// byte 0x903848 is 0x29, +1 up.
extern "C" void __cdecl EffectKind2B_Specks(void) {
    if (Frame_Counter & 1u) SH_CALL(EffectSpecks_Spawn)();
    SH_CALL(EffectSpecks_Move)();
    if (At(at::kCounter0)[0] == 0x29) NextState();
}

// original 0x471A10 (state 3): the specks moved; once none is left, +0x32 =
// 0x10 and +1 up.
extern "C" void __cdecl EffectKind2B_Settle(void) {
    if (SH_CALL(EffectSpecks_Move)() != 0) return;
    SetWord(S() + 0x32, 0x10);
    NextState();
}

// original 0x471A30 (state 4): the radius +0x2E down 0x10, the count +0x32
// down; at 0 a tail jmp to Effect_Release.
extern "C" void __cdecl EffectKind2B_Shrink(void) {
    AddWord(S() + 0x2E, 0xFFF0u);
    AddWord(S() + 0x32, 0xFFFFu);
    if (Word(S() + 0x32) != 0) return;
    SH_CALL(Effect_Release)();
}

// original 0x471A60 (0x2AB bytes, no list held it; reached only by the
// dispatcher's tail jmp): the ring. EffectGte_LoadMapCamera; the point at angle
// u on the ring is (x + (cos(u) * r sar 4), z + (sin(u) * r sar 4)) with r the
// word +0x2E (signed) and (x, z) the record's +0x34 / +0x38 (Sprite_Current read
// after each Math_* call), projected at the ground height +0x3C and at the top
// +0x3C + (+0x30 << 16). For u = 0x100, 0x200, .. 0x1000 (each masked to its low
// word for Math_*), sixteen semi-transparent Gouraud quads at the packet cursor
// between the last angle's two projections and this one's, each preceded by a
// draw mode (0x3C0, 0, transparency 1, dithered) and followed by one (not
// dithered), all three linked at this angle's (x, z) by MapView_LinkPrimAt (0xC,
// 0x44, 0xC). The shade starts at (8 + the frame's bit) * 4; the first two
// vertices take it, then it moves by 4 - down for quads 4..11, up for the rest -
// and the last two take that (byte arithmetic, kept from quad to quad).
extern "C" void __cdecl EffectKind2B_Draw(void) {
    SH_CALL(EffectGte_LoadMapCamera)();
    long point[3];
    float ground[3], top[3];
    auto Project = [&](U u) {
        {
            const U c = static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(u)));
            const unsigned char* const s = S();
            point[0] = static_cast<long>(Sar(c * static_cast<U>(SW(s + 0x2E)), 4) + UL(s + 0x34));
        }
        {
            const U n = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(u)));
            const unsigned char* const s = S();
            point[1] = static_cast<long>(Sar(n * static_cast<U>(SW(s + 0x2E)), 4) + UL(s + 0x38));
            point[2] = static_cast<long>(UL(s + 0x3C));
        }
        SH_CALL(EffectGte_ProjectPoint)(point, ground);
        const unsigned char* const s = S();
        point[2] = static_cast<long>((static_cast<U>(SW(s + 0x30)) << 16) + UL(s + 0x3C));
        SH_CALL(EffectGte_ProjectPoint)(point, top);
    };
    Project(0);
    auto shade = static_cast<unsigned char>(static_cast<unsigned char>((Frame_Counter & 1u) + 8u) << 2);
    U u = 0;
    for (unsigned k = 0; k < 16; ++k) {
        float last_ground[3], last_top[3];
        std::memcpy(last_ground, ground, sizeof ground);
        std::memcpy(last_top, top, sizeof top);
        u += 0x100;
        Project(u & 0xFFFFu);
        DrawMode(1, 0x3C0, 0, 1);
        SH_CALL(MapView_LinkPrimAt)(static_cast<U>(point[0]), static_cast<U>(point[1]), 0, 0xC);
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG4)(p);
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        std::memcpy(p + 8, last_ground, 12);
        std::memcpy(p + 0x18, last_top, 12);
        std::memcpy(p + 0x28, ground, 12);
        std::memcpy(p + 0x38, top, 12);
        Shade3(p + 4, shade);
        Shade3(p + 0x14, shade);
        shade = static_cast<unsigned char>(k >= 4 && k < 0xC ? shade - 4 : shade + 4);
        Shade3(p + 0x24, shade);
        Shade3(p + 0x34, shade);
        SH_CALL(MapView_LinkPrimAt)(static_cast<U>(point[0]), static_cast<U>(point[1]), 0, 0x44);
        DrawMode(1, 0x3C0, 0, 0);
        SH_CALL(MapView_LinkPrimAt)(static_cast<U>(point[0]), static_cast<U>(point[1]), 0, 0xC);
    }
}

// original 0x471D10 (0xB7 bytes; kinds 0x1C and 0x1D's rise and hold call it
// too): the first free speck of the pool (+0 == 0, 0x80 of 0x14; none: return)
// gets +0 = 1, +1 = 0x40; a distance d = Rand % the word +0x2E (signed, idiv;
// read again after Rand) when +0x2E is not 0, else 0; an angle Rand & 0xFFF; +4 /
// +8 = (cos / sin(angle) * d (as s16) sar 4) + Sprite_Current's +0x34 / +0x38;
// +0xC = (the rise +0x30 << 16) + +0x3C; the word +2 = (Rand & 0xFFF) + 0x1000
// (Sprite_Current read after each call). Nothing a caller reads is answered.
extern "C" void __cdecl EffectSpecks_Spawn(void) {
    unsigned char* r = nullptr;
    for (unsigned i = 0; i < at::kPoolCount; ++i)
        if (Pool(at::kSpeckStride, i)[0] == 0) {
            r = Pool(at::kSpeckStride, i);
            break;
        }
    if (!r) return;
    r[0] = 1;
    r[1] = 0x40;
    std::int32_t d = 0;
    if (Word(S() + 0x2E) != 0) {
        const auto n = static_cast<std::int32_t>(SH_CALL(Rand)());
        const std::int32_t m = SW(S() + 0x2E);
        if (m == 0 || (m == -1 && n == INT32_MIN))
            bof3::Fatal("EffectSpecks_Spawn: %d / %d - the original's idiv faults (the word +0x2E read again after "
                        "Rand; docs/effect_2a.md section 2)",
                        n, m);
        d = n % m;
    }
    const U angle = static_cast<U>(SH_CALL(Rand)()) & 0xFFFu;
    const U dist = static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(d)));
    const U c = static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(angle)));
    SetUL(r + 4, Sar(c * dist, 4) + UL(S() + 0x34));
    const U n = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(angle)));
    const unsigned char* const s = S();
    SetUL(r + 8, Sar(n * dist, 4) + UL(s + 0x38));
    SetUL(r + 0xC, (static_cast<U>(SW(s + 0x30)) << 16) + UL(s + 0x3C));
    SetWord(r + 2, (static_cast<U>(SH_CALL(Rand)()) & 0xFFFu) + 0x1000u);
}

// original 0x471DD0 (0x4B bytes): each speck in use: its height +0xC down (the
// word +2, signed, << 8), drawn (EffectSpecks_Draw), freed (+0 = 0) once below
// Sprite_Current's +0x3C (signed); al 1 when any was in use, else 0.
extern "C" unsigned char __cdecl EffectSpecks_Move(void) {
    unsigned char any = 0;
    for (unsigned i = 0; i < at::kPoolCount; ++i) {
        unsigned char* const r = Pool(at::kSpeckStride, i);
        if (r[0] == 0) continue;
        SetUL(r + 0xC, UL(r + 0xC) - (static_cast<U>(SW(r + 2)) << 8));
        SH_CALL(EffectSpecks_Draw)(r);
        if (Long(r + 0xC) < Long(S() + 0x3C)) r[0] = 0;
        any = 1;
    }
    return any;
}

// original 0x471E20 (0x77 bytes): the speck's point (+4..+0xC) projected
// (EffectGte_ProjectPoint into a stack vector); a TILE_1 at the packet cursor
// (Gpu_SetTile1, opaque) at it, white on an odd Rand, else black; linked at the
// speck's x, z by MapView_LinkPrimAt(x, z, 0, 0x14).
extern "C" void __cdecl EffectSpecks_Draw(const unsigned char* speck) {
    float screen[3];
    SH_CALL(EffectGte_ProjectPoint)(reinterpret_cast<const long*>(speck + 4), screen);
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetTile1)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, 0);
    std::memcpy(p + 8, screen, 12);
    const auto c = static_cast<unsigned char>((SH_CALL(Rand)() & 1) ? 0xFF : 0);
    p[6] = p[5] = p[4] = c;
    SH_CALL(MapView_LinkPrimAt)(UL(speck + 4), UL(speck + 8), 0, 0x14);
}

// ===========================================================================
// Kind 0x2C: Effect_KindHandlers[0x2C] (0x655400), EffectKind2C_States (three,
// the last Effect_StateRelease)
// ===========================================================================

// original 0x471EA0: jmp [EffectKind2C_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind2C_Run(void) { Dispatch("EffectKind2C_Run", AddressOf(EffectKind2C_States), EffectKind2C_States_count); }

// The centre 0x92D380: Sprite_ObjectsExtra[0]'s x, z and height sar 8.
static void SparkCentreFromExtra() {
    const U h = UL(At(at::kExtra0Point + 8));
    const U x = UL(At(at::kExtra0Point));
    const U z = UL(At(at::kExtra0Point + 4));
    SetUL(At(at::kSparkCentre), x);
    SetUL(At(at::kSparkCentre + 4), z);
    SetUL(At(at::kSparkCentre + 8), Sar(h, 8));
}

// original 0x471EC0 (state 0): the centre from Sprite_ObjectsExtra[0]; eight
// ground points 0x92D390 (0x10 each): x, z = the centre's + the dwords of
// 0x65436C / 0x65438C, the height AreaMap_Elevation(x, z) (its low word, signed,
// << 8); the sparks cleared; the dword +0xC = 0x200, +1 up.
extern "C" void __cdecl EffectKind2C_Start(void) {
    SparkCentreFromExtra();
    for (unsigned i = 0; i < 8; ++i) {
        unsigned char* const pt = At(at::kSparkPoints + 0x10 * i);
        SetUL(pt, UL(At(at::kSparkOffsetX + 4 * i)) + UL(At(at::kSparkCentre)));
        const U z = UL(At(at::kSparkOffsetZ + 4 * i)) + UL(At(at::kSparkCentre + 4));
        SetUL(pt + 4, z);
        const long ground = SH_CALL(AreaMap_Elevation)(Long(pt), static_cast<long>(z));
        SetUL(pt + 8, static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(ground))) << 8);
    }
    SH_CALL(EffectSparks_Clear)();
    SetUL(S() + 0xC, 0x200);
    NextState();
}

// original 0x471F60 (state 1): when the low four bits of +0xC are 0, for each
// ground point a free spark (EffectSparks_FindFree; none: the next point): its
// +0..+0xC the centre's four dwords, its velocity +0x10..+0x18 = (the point -
// the centre) sar 4, normalised in place (Gte_VectorNormal), +0x24 (its shade)
// = 0x80, +0x20 (its size) = 0x1000, +0x25 (frames left) = 0x80. Then the centre
// from Sprite_ObjectsExtra[0] again, the sparks moved (EffectSparks_Move); +0xC
// down, at 0 +1 up.
extern "C" void __cdecl EffectKind2C_Emit(void) {
    if ((S()[0xC] & 0xF) == 0) {
        for (unsigned i = 0; i < 8; ++i) {
            unsigned char* const r = SH_CALL(EffectSparks_FindFree)();
            if (!r) continue;
            const unsigned char* const c = At(at::kSparkCentre);
            const unsigned char* const pt = At(at::kSparkPoints + 0x10 * i);
            for (unsigned k = 0; k < 4; ++k) SetUL(r + 4 * k, UL(c + 4 * k));
            for (unsigned k = 0; k < 3; ++k) SetUL(r + 0x10 + 4 * k, Sar(UL(pt + 4 * k) - UL(c + 4 * k), 4));
            SH_CALL(Gte_VectorNormal)(reinterpret_cast<const long*>(r + 0x10), reinterpret_cast<long*>(r + 0x10));
            r[0x24] = 0x80;
            SetUL(r + 0x20, 0x1000);
            r[0x25] = 0x80;
        }
    }
    SparkCentreFromExtra();
    SH_CALL(EffectSparks_Move)();
    unsigned char* t = S();
    SetUL(t + 0xC, UL(t + 0xC) - 1u);
    t = S();
    if (UL(t + 0xC) == 0) t[1] = static_cast<unsigned char>(t[1] + 1);
}

// original 0x4721A0 (0xA0 bytes): EffectGte_LoadMapCamera; each spark alive (+0x25
// not 0): drawn (EffectSparks_Draw), moved by its velocity (x, z, height in
// that order); AreaMap_Elevation(x, z) - above the height (its low word, signed),
// the height = it, the vertical velocity 0 and the velocity normalised in place;
// +0x24 down one, +0x20 up 4, +0x25 down one.
extern "C" void __cdecl EffectSparks_Move(void) {
    SH_CALL(EffectGte_LoadMapCamera)();
    for (unsigned i = 0; i < at::kPoolCount; ++i) {
        unsigned char* const r = Pool(at::kSparkStride, i);
        if (r[0x25] == 0) continue;
        SH_CALL(EffectSparks_Draw)(r);
        const U x = UL(r) + UL(r + 0x10);
        SetUL(r, x);
        const U z = UL(r + 4) + UL(r + 0x14);
        SetUL(r + 4, z);
        SetUL(r + 8, UL(r + 8) + UL(r + 0x18));
        const long ground = SH_CALL(AreaMap_Elevation)(static_cast<long>(x), static_cast<long>(z));
        const std::int32_t g = static_cast<std::int16_t>(ground);
        if (g > Long(r + 8)) {
            SetLong(r + 8, g);
            SetUL(r + 0x18, 0);
            SH_CALL(Gte_VectorNormal)(reinterpret_cast<const long*>(r + 0x10), reinterpret_cast<long*>(r + 0x10));
        }
        const auto shade = static_cast<unsigned char>(r[0x24] - 1);
        const U size = UL(r + 0x20) + 4u;
        const auto left = static_cast<unsigned char>(r[0x25] - 1);
        r[0x24] = shade;
        SetUL(r + 0x20, size);
        r[0x25] = left;
    }
}

// original 0x472240 (0x198 bytes): a draw mode (page 0x2C0, 0x100, transparency
// 1, not dithered) linked at the spark's x, z (MapView_LinkPrimAt(x, z, 1, 0xC));
// a semi-transparent textured quad at the packet cursor: the half size
// EffectGte_ProjectSize(the point (x, z, height << 8), {+0x20 >> 6, -}) - its
// first word only (the original's second size word is stack it never wrote, and
// the second quotient is not read) -, the centre EffectGte_ProjectPoint(the
// point), the corners (sx -/+ half, sy -/+ half), all at the centre's depth; the
// CLUT Gpu_GetClut(0xA0, 0x1E3), the page Gpu_GetTPage(0, 1, 0x2C0, 0x100), the
// texture corners fixed, shaded +0x24; linked at x, z (1, 0x48).
extern "C" void __cdecl EffectSparks_Draw(const unsigned char* spark) {
    DrawMode(1, 0x2C0, 0x100, 0);
    SH_CALL(MapView_LinkPrimAt)(UL(spark), UL(spark + 4), 1, 0xC);
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyFT4)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, 1);
    short size[2] = {static_cast<short>(UL(spark + 0x20) >> 6), 0};
    const long point[3] = {static_cast<long>(UL(spark)), static_cast<long>(UL(spark + 4)),
                           static_cast<long>(UL(spark + 8) << 8)};
    short out[2] = {0, 0};
    SH_CALL(EffectGte_ProjectSize)(point, size, out);
    float centre[3];
    SH_CALL(EffectGte_ProjectPoint)(point, centre);
    const double half = static_cast<double>(out[0]);
    const double cx = static_cast<double>(centre[0]), cy = static_cast<double>(centre[1]);
    SetD(p + 0x28, cx - half);
    SetD(p + 8, cx - half);
    SetD(p + 0x38, cx + half);
    SetD(p + 0x18, cx + half);
    SetD(p + 0x1C, cy - half);
    SetD(p + 0xC, cy - half);
    SetD(p + 0x3C, cy + half);
    SetD(p + 0x2C, cy + half);
    U depth;
    std::memcpy(&depth, &centre[2], sizeof depth);
    SetUL(p + 0x40, depth);
    SetUL(p + 0x30, depth);
    SetUL(p + 0x20, depth);
    SetUL(p + 0x10, depth);
    SetWord(p + 0x16, SH_CALL(Gpu_GetClut)(0xA0, 0x1E3));
    SetWord(p + 0x26, SH_CALL(Gpu_GetTPage)(0, 1, 0x2C0, 0x100));
    p[0x15] = 0x30;
    p[0x25] = 0x30;
    p[0x14] = 0xE0;
    p[0x24] = 0xFF;
    p[0x34] = 0xE0;
    p[0x35] = 0x4F;
    p[0x44] = 0xFF;
    p[0x45] = 0x4F;
    p[4] = spark[0x24];
    p[5] = spark[0x24];
    p[6] = spark[0x24];
    SH_CALL(MapView_LinkPrimAt)(UL(spark), UL(spark + 4), 1, 0x48);
}

// original 0x4723E0 (0x14 bytes): every spark's +0x25 = 0 (all free).
extern "C" void __cdecl EffectSparks_Clear(void) {
    for (unsigned i = 0; i < at::kPoolCount; ++i) Pool(at::kSparkStride, i)[0x25] = 0;
}

// original 0x472400 (0x1B bytes): the first spark whose +0x25 is 0, or 0 (eax).
extern "C" unsigned char* __cdecl EffectSparks_FindFree(void) {
    for (unsigned i = 0; i < at::kPoolCount; ++i)
        if (Pool(at::kSparkStride, i)[0x25] == 0) return Pool(at::kSparkStride, i);
    return nullptr;
}

// ===========================================================================
// Kind 0x2E: Effect_KindHandlers[0x2E] (0x655408), EffectKind2E_States (three;
// the dword after them is kind 0x2D's table)
// ===========================================================================

// original 0x472060: jmp [EffectKind2E_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind2E_Run(void) { Dispatch("EffectKind2E_Run", AddressOf(EffectKind2E_States), EffectKind2E_States_count); }

// original 0x472080 (state 0): the sparks cleared, +1 up.
extern "C" void __cdecl EffectKind2E_Start(void) {
    SH_CALL(EffectSparks_Clear)();
    NextState();
}

// Sprite_Objects[1]'s point into a spark: x, z, height sar 8.
static void SparkAtObject1(unsigned char* r) {
    SetUL(r, UL(At(at::kObject1Point)));
    SetUL(r + 4, UL(At(at::kObject1Point + 4)));
    SetUL(r + 8, Sar(UL(At(at::kObject1Point + 8)), 8));
}

// original 0x472090 (state 1): when the frame's low three bits are 0, a free
// spark at Sprite_Objects[1]'s point (x, z, height sar 8), still (velocity 0),
// shade 0x80, size 0x800, 0x80 frames; the sparks moved; when the counter byte
// 0x903848 is 3, +1 up.
extern "C" void __cdecl EffectKind2E_Trickle(void) {
    if ((Frame_Counter & 7u) == 0) {
        unsigned char* const r = SH_CALL(EffectSparks_FindFree)();
        if (r) {
            SparkAtObject1(r);
            SetUL(r + 0x10, 0);
            SetUL(r + 0x14, 0);
            SetUL(r + 0x18, 0);
            r[0x24] = 0x80;
            SetUL(r + 0x20, 0x800);
            r[0x25] = 0x80;
        }
    }
    SH_CALL(EffectSparks_Move)();
    if (At(at::kCounter0)[0] == 3) NextState();
}

// original 0x4720F0 (state 2): sixteen free sparks at Sprite_Objects[1]'s point
// (a spark not found skips its angle), each thrown flat at the angle 0x100 k:
// velocity (sin sar 2, cos sar 2, 0), shade 0x40, size 0x800, 0x40 frames; the
// sparks moved; the dword +0xC = 0x40. +1 is not moved: the state repeats.
extern "C" void __cdecl EffectKind2E_Burst(void) {
    for (unsigned k = 0; k < 16; ++k) {
        unsigned char* const r = SH_CALL(EffectSparks_FindFree)();
        if (!r) continue;
        SparkAtObject1(r);
        const U angle = (0x1000u * k / 16u) & 0xFFFFu;
        SetUL(r + 0x10, Sar(static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(angle))), 2));
        const U c = static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(angle)));
        SetUL(r + 0x18, 0);
        SetUL(r + 0x14, Sar(c, 2));
        r[0x24] = 0x40;
        SetUL(r + 0x20, 0x800);
        r[0x25] = 0x40;
    }
    SH_CALL(EffectSparks_Move)();
    SetUL(S() + 0xC, 0x40);
}

// ===========================================================================
// Kind 0x2D: Effect_KindHandlers[0x2D] (0x655404), EffectKind2D_States (eight)
// ===========================================================================

// original 0x472420: jmp [EffectKind2D_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind2D_Run(void) { Dispatch("EffectKind2D_Run", AddressOf(EffectKind2D_States), EffectKind2D_States_count); }

// original 0x472440 (state 0): the link cell 0x676100 / 0x676104 = (0x1C0000,
// 0x180000); EffectGte_LoadMapCamera; Sprite_Objects[0]'s point projected into
// the record's +0xC..+0x14 (three floats), Sprite_Objects[1]'s into a stack
// vector, the two added into +0xC..+0x14 (x, y, depth), the words +0x18 / +0x1A
// = 0, x and y scaled by the float 0x5C41D8; Msg_OpenScript(0x14), Field_Request
// = 2, +9 = 0x32, +1 up, Sound_PlayEffect(0x200).
extern "C" void __cdecl EffectKind2D_Start(void) {
    SetUL(At(at::kLinkX), 0x1C0000);
    SetUL(At(at::kLinkZ), 0x180000);
    unsigned char* const q = S() + 0xC;
    SH_CALL(EffectGte_LoadMapCamera)();
    long point[3] = {Long(At(at::kObject0Point)), Long(At(at::kObject0Point + 4)), Long(At(at::kObject0Point + 8))};
    SH_CALL(EffectGte_ProjectPoint)(point, reinterpret_cast<float*>(q));
    point[0] = Long(At(at::kObject1Point));
    point[1] = Long(At(at::kObject1Point + 4));
    point[2] = Long(At(at::kObject1Point + 8));
    float other[3];
    SH_CALL(EffectGte_ProjectPoint)(point, other);
    SetD(q, static_cast<double>(other[0]) + D(q));
    SetD(q + 4, static_cast<double>(other[1]) + D(q + 4));
    const double depth = static_cast<double>(other[2]) + D(q + 8);
    SetWord(q + 0xC, 0);
    SetWord(q + 0xE, 0);
    SetD(q + 8, depth);
    const double scale = D(At(at::kScreenScale));
    SetD(q, D(q) * scale);
    SetD(q + 4, D(q + 4) * scale);
    SH_CALL(Msg_OpenScript)(0x14);
    Field_Request = 2;
    S()[9] = 0x32;
    NextState();
    SH_CALL(Sound_PlayEffect)(0x200);
}

// original 0x472530 (state 1): the radius word +0x18 up 2 while +9 is above 0x19
// (unsigned), else down 2; the angle +0x1A up 0x40; the rays drawn (the record +
// 0xC taken on entry); +9 down, at 0 +1 up.
extern "C" void __cdecl EffectKind2D_Spin(void) {
    unsigned char* const s = S();
    unsigned char* const q = s + 0xC;
    AddWord(q + 0xC, s[9] > 0x19 ? 2u : 0xFFFEu);
    AddWord(q + 0xE, 0x40);
    SH_CALL(EffectKind2D_DrawRays)(q);
    CountDown(-1);
}

// original 0x472580 (state 2): the words +0x18 / +0x1A = 0, +9 = 0x5A, +1 up,
// Sound_PlayEffect(0x201).
extern "C" void __cdecl EffectKind2D_Pause(void) {
    unsigned char* const q = S() + 0xC;
    SetWord(q + 0xC, 0);
    SetWord(q + 0xE, 0);
    S()[9] = 0x5A;
    NextState();
    SH_CALL(Sound_PlayEffect)(0x201);
}

// original 0x4725B0 (state 3): the radius +0x18 up one, held at 0x40 (signed);
// the angle +0x1A up 0x40; the rings drawn; +9 down - at 0 the drops cleared,
// the level word +0x1C = 0, +9 = 0 and +1 up.
extern "C" void __cdecl EffectKind2D_Open(void) {
    unsigned char* const s = S();
    AddWord(s + 0x18, 1);
    unsigned char* const q = s + 0xC;
    if (SW(s + 0x18) > 0x40) SetWord(q + 0xC, 0x40);
    AddWord(q + 0xE, 0x40);
    SH_CALL(EffectKind2D_DrawRings)(q);
    unsigned char* t = S();
    t[9] = static_cast<unsigned char>(t[9] - 1);
    if (S()[9] != 0) return;
    SH_CALL(EffectDrops_Clear)();
    SetWord(q + 0x10, 0);
    S()[9] = 0;
    NextState();
}

// original 0x472620 (state 4): the angle +0x1A up 0x40, the level +0x1C up 2;
// +9 up, at 0xF Sound_PlayEffect(0x202); the rings drawn, the drops moved, the
// curtain drawn; on odd frames a free drop (EffectDrops_FindFree) launched;
// once the level is above 0x100 (signed), +9 = 7 and +1 up.
extern "C" void __cdecl EffectKind2D_Pour(void) {
    unsigned char* const s = S();
    AddWord(s + 0x1A, 0x40);
    AddWord(s + 0x1C, 2);
    unsigned char* const q = s + 0xC;
    unsigned char* const t = S();
    t[9] = static_cast<unsigned char>(t[9] + 1);
    if (S()[9] == 0xF) SH_CALL(Sound_PlayEffect)(0x202);
    SH_CALL(EffectKind2D_DrawRings)(q);
    SH_CALL(EffectDrops_Move)();
    SH_CALL(EffectKind2D_DrawCurtain)(q);
    if (Frame_Counter & 1u) {
        unsigned char* const drop = SH_CALL(EffectDrops_FindFree)();
        if (drop) SH_CALL(EffectDrops_Launch)(drop);
    }
    if (SW(q + 0x10) <= 0x100) return;
    S()[9] = 7;
    NextState();
}

// original 0x4726B0 (state 5): the curtain drawn; the radius +0x18 down 8; +9
// down - at 0 the counter byte 0x903848 = 0x31, +0x18 = 2, +0x1A = 0, +9 = 0xF,
// +1 up, Sound_PlayEffect(0x203).
extern "C" void __cdecl EffectKind2D_Close(void) {
    unsigned char* const q = S() + 0xC;
    SH_CALL(EffectKind2D_DrawCurtain)(q);
    AddWord(q + 0xC, 0xFFF8u);
    unsigned char* const t = S();
    t[9] = static_cast<unsigned char>(t[9] - 1);
    if (S()[9] != 0) return;
    At(at::kCounter0)[0] = 0x31;
    SetWord(q + 0xC, 2);
    SetWord(q + 0xE, 0);
    S()[9] = 0xF;
    NextState();
    SH_CALL(Sound_PlayEffect)(0x203);
}

// original 0x472720 (state 6): the curtain drawn; the word +0x1A up 2 and the
// screen y (+0x10) up by it (y - the word, signed); drawn again; +9 down, at 0
// +1 up.
extern "C" void __cdecl EffectKind2D_Lift(void) {
    unsigned char* const q = S() + 0xC;
    SH_CALL(EffectKind2D_DrawCurtain)(q);
    AddWord(q + 0xE, 2);
    SetD(q + 4, D(q + 4) - static_cast<double>(SW(q + 0xE)));
    SH_CALL(EffectKind2D_DrawCurtain)(q);
    CountDown(-1);
}

// original 0x472770 (state 7): the byte 0x7DEE44 |= 2, the counter byte
// 0x903848 = 0x32, a tail jmp to Effect_Release.
extern "C" void __cdecl EffectKind2D_End(void) {
    At(at::kMessageBits)[0] = static_cast<unsigned char>(At(at::kMessageBits)[0] | 2);
    At(at::kCounter0)[0] = 0x32;
    SH_CALL(Effect_Release)();
}

// original 0x472790 (0x181 bytes): a draw mode (page 0x380, 0x100, transparency
// 1, not dithered) linked at kind 0x2D's cell (4, 0xC); eight semi-transparent
// Gouraud lines at the packet cursor from the screen point (x, y copied, the
// depth for both ends) to (x + cos(u) * r sar s, y + sin(u) * r sar s), u = the
// angle word + 0x200 k (its low word), r the radius word (signed), s 12 on even
// k and 13 on odd; grey 0x80 at the centre, 0 at the tip, each linked (4,
// 0x24). Then EffectKind2D_DrawDisc and the draw mode again.
extern "C" void __cdecl EffectKind2D_DrawRays(const unsigned char* q) {
    LinkedMode(1, 0);
    for (unsigned k = 0; k < 8; ++k) {
        unsigned char* const p = Gfx_PacketNext;
        const U u = ((k << 9) + Word(q + 0xE)) & 0xFFFFu;
        SH_CALL(Gpu_SetLineG2)(p);
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        SetUL(p + 8, UL(q));
        SetUL(p + 0xC, UL(q + 4));
        const unsigned shift = (k & 1) + 12;
        const U c = static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(u)));
        SetD(p + 0x18, I(Sar(c * static_cast<U>(SW(q + 0xC)), shift)) + D(q));
        const U n = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(u)));
        SetD(p + 0x1C, I(Sar(n * static_cast<U>(SW(q + 0xC)), shift)) + D(q + 4));
        CopyQuiet(p + 0x20, q + 8);
        CopyQuiet(p + 0x10, q + 8);
        Shade3(p + 4, 0x80);
        Shade3(p + 0x14, 0);
        SH_CALL(MapView_LinkPrimAt)(LinkX(), LinkZ(), 4, 0x24);
    }
    SH_CALL(EffectKind2D_DrawDisc)(q);
    LinkedMode(1, 0);
}

// original 0x472920 (0x18C bytes): the draw mode linked (as the rays'); h = the
// radius word sar 2 (16 bits); when h is not negative, for each row i = -h..h a
// half width w = the low word of 0x5A7A90(h * h - i * i) (the square root,
// truncated) and two semi-transparent TILE_1s at the packet cursor: one at (x +
// w, y + i), grey 0x40, at the point's depth, linked (4, 0x14); a copy of it
// right after (not the cursor read again) at x - w, linked. Then the draw mode
// again.
extern "C" void __cdecl EffectKind2D_DrawDisc(const unsigned char* q) {
    LinkedMode(1, 0);
    const auto h = static_cast<std::int16_t>(static_cast<std::int16_t>(Word(q + 0xC)) >> 2);
    const auto minus = static_cast<std::int16_t>(-h);
    if (minus <= h) {
        const std::int32_t square = static_cast<std::int32_t>(h) * h;
        std::int32_t i = minus;
        std::int32_t count = static_cast<std::int32_t>(h) - minus + 1;
        do {
            const U root = SH_AT(U (__cdecl*)(long), at::kRootWord)(static_cast<long>(square - i * i));
            const std::int32_t w = static_cast<std::int16_t>(root);
            unsigned char* const p = Gfx_PacketNext;
            SH_CALL(Gpu_SetTile1)(p);
            SH_CALL(Gpu_SetSemiTrans)(p, 1);
            const float fw = static_cast<float>(w);
            SetD(p + 8, static_cast<double>(w) + D(q));
            SetD(p + 0xC, static_cast<double>(i) + D(q + 4));
            SetUL(p + 0x10, UL(q + 8));
            Shade3(p + 4, 0x40);
            SH_CALL(MapView_LinkPrimAt)(LinkX(), LinkZ(), 4, 0x14);
            unsigned char* const mirror = p + 0x14;
            std::memcpy(mirror, p, 0x14);
            SetD(mirror + 8, D(q) - static_cast<double>(fw));
            SH_CALL(MapView_LinkPrimAt)(LinkX(), LinkZ(), 4, 0x14);
            ++i;
        } while (--count != 0);
    }
    LinkedMode(1, 0);
}

// original 0x472AB0 (0x2B4 bytes): a draw mode (0x380, 0x100, transparency 2)
// linked (4, 0xC); eight rings j = 0..7 between the radii j R / 8 and (j + 1) R
// / 8 (R the radius word, signed; signed divides toward zero, each an s16), each
// of sixteen semi-transparent Gouraud quads at the packet cursor: the points
// (x + cos(u) r sar 12, y + sin(u) r sar 13) - an ellipse half as tall - at
// (inner, A - 0x80 j + 0x100 n), (outer, A - 0x80 (j + 1) + 0x100 n) and the
// same two a step 0x100 on (A the angle word, each u its low word), shaded 0,
// 0, 0x40, 0x40, at the point's depth, linked (4, 0x44). Then the draw mode with
// transparency 1, linked (4, 0xC).
extern "C" void __cdecl EffectKind2D_DrawRings(const unsigned char* q) {
    LinkedMode(2, 0);
    for (U j = 0; j < 8; ++j) {
        const U radius = static_cast<U>(SW(q + 0xC));
        const U inner = static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(Div8(j * radius))));
        const U outer = static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(Div8((j + 1) * radius))));
        const U angle = Word(q + 0xE);
        U a1 = angle - 0x80 * j;
        U a2 = angle - 0x80 * (j + 1);
        auto Cos = [](U u) { return static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(u & 0xFFFFu))); };
        auto Sin = [](U u) { return static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(u & 0xFFFFu))); };
        for (unsigned n = 0; n < 16; ++n) {
            unsigned char* const p = Gfx_PacketNext;
            SH_CALL(Gpu_SetPolyG4)(p);
            SH_CALL(Gpu_SetSemiTrans)(p, 1);
            SetD(p + 8, I(Sar(Cos(a1) * inner, 12)) + D(q));
            SetD(p + 0xC, I(Sar(Sin(a1) * inner, 13)) + D(q + 4));
            SetD(p + 0x18, I(Sar(Cos(a2) * outer, 12)) + D(q));
            SetD(p + 0x1C, I(Sar(Sin(a2) * outer, 13)) + D(q + 4));
            a1 += 0x100;
            a2 += 0x100;
            SetD(p + 0x28, I(Sar(Cos(a1) * inner, 12)) + D(q));
            SetD(p + 0x2C, I(Sar(Sin(a1) * inner, 13)) + D(q + 4));
            SetD(p + 0x38, I(Sar(Cos(a2) * outer, 12)) + D(q));
            SetD(p + 0x3C, I(Sar(Sin(a2) * outer, 13)) + D(q + 4));
            CopyQuiet(p + 0x40, q + 8);
            CopyQuiet(p + 0x30, q + 8);
            CopyQuiet(p + 0x20, q + 8);
            CopyQuiet(p + 0x10, q + 8);
            Shade3(p + 4, 0);
            Shade3(p + 0x14, 0);
            Shade3(p + 0x24, 0x40);
            Shade3(p + 0x34, 0x40);
            SH_CALL(MapView_LinkPrimAt)(LinkX(), LinkZ(), 4, 0x44);
        }
    }
    LinkedMode(1, 0);
}

// original 0x472F00 (0x1F6 bytes): a draw mode (0x380, 0x100, transparency 1,
// dithered) linked (4, 0xC); eight semi-transparent Gouraud quads at the packet
// cursor over u = 0, 0x100, .. 0x800: at u and the next, a top vertex (x + cos(u)
// r sar 12, 0.0) and a bottom one (the same x, y + sin(u) r sar 13), r the radius
// word (signed), at the point's depth; the shade the level word +0x10 clamped to
// 0..0xFF, the first pair's before it moves by 4 (up for quads 0 and 1, down
// after), the second pair's after (16 bits, kept from quad to quad); linked (4,
// 0x44). Then the draw mode not dithered, linked (4, 0xC).
extern "C" void __cdecl EffectKind2D_DrawCurtain(const unsigned char* q) {
    LinkedMode(1, 1);
    auto level = static_cast<std::uint16_t>(Word(q + 0x10));
    auto Clamp = [](std::uint16_t v) {
        const auto s = static_cast<std::int16_t>(v);
        return static_cast<unsigned char>(s > 0xFF ? 0xFF : s < 0 ? 0 : v);
    };
    U next = 0;   // the original's angle slot, stepped whole
    U u = 0;      // what Math_* are handed: its low word
    for (unsigned k = 0; k < 8; ++k) {
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG4)(p);
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        {
            const U c = static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(u)));
            const double x = I(Sar(c * static_cast<U>(SW(q + 0xC)), 12)) + D(q);
            SetUL(p + 0xC, 0);
            SetD(p + 0x18, x);
            SetD(p + 8, x);
            const U n = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(u)));
            SetD(p + 0x1C, I(Sar(n * static_cast<U>(SW(q + 0xC)), 13)) + D(q + 4));
        }
        Shade3(p + 4, Clamp(level));
        Shade3(p + 0x14, Clamp(level));
        level = static_cast<std::uint16_t>(k < 2 ? level + 4 : level - 4);
        next += 0x100;
        u = next & 0xFFFFu;
        {
            const U c = static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(u)));
            const double x = I(Sar(c * static_cast<U>(SW(q + 0xC)), 12)) + D(q);
            SetUL(p + 0x2C, 0);
            SetD(p + 0x38, x);
            SetD(p + 0x28, x);
            const U n = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(u)));
            SetD(p + 0x3C, I(Sar(n * static_cast<U>(SW(q + 0xC)), 13)) + D(q + 4));
        }
        CopyQuiet(p + 0x40, q + 8);
        CopyQuiet(p + 0x30, q + 8);
        CopyQuiet(p + 0x20, q + 8);
        CopyQuiet(p + 0x10, q + 8);
        Shade3(p + 0x24, Clamp(level));
        Shade3(p + 0x34, Clamp(level));
        SH_CALL(MapView_LinkPrimAt)(LinkX(), LinkZ(), 4, 0x44);
    }
    LinkedMode(1, 0);
}

// original 0x472D70 (0x16 bytes): every drop's word +0x14 = 0 (all free).
extern "C" void __cdecl EffectDrops_Clear(void) {
    for (unsigned i = 0; i < at::kPoolCount; ++i) SetWord(Pool(at::kDropStride, i) + 0x14, 0);
}

// original 0x472D90 (0x67 bytes): each drop alive (+0x14 not 0): drawn
// (EffectDrops_Draw); its screen y +4 down by the word +0xE (y - it, signed),
// +0xE = its old value + (+0x14's low bit), +0xC up one, +0x14 down one - at 0
// the drop launched again (EffectDrops_Launch).
extern "C" void __cdecl EffectDrops_Move(void) {
    for (unsigned i = 0; i < at::kPoolCount; ++i) {
        unsigned char* const r = Pool(at::kDropStride, i);
        if (Word(r + 0x14) == 0) continue;
        SH_CALL(EffectDrops_Draw)(r);
        const U speed = Word(r + 0xE);
        const U left = Word(r + 0x14);
        const double y = D(r + 4) - static_cast<double>(static_cast<std::int16_t>(speed));
        AddWord(r + 0xC, 1);
        SetWord(r + 0xE, (left & 1) + speed);
        SetD(r + 4, y);
        SetWord(r + 0x14, left - 1);
        if (((left - 1) & 0xFFFFu) == 0) SH_CALL(EffectDrops_Launch)(r);
    }
}

// original 0x472E00 (0xDF bytes): an opaque flat quad at the packet cursor
// (Gpu_SetPolyF4), grey 0x80: the column x .. x + the float 0x5C41C0, from y -
// the word +0xC (held at the word +0x10 at least) to y + +0xC (held at +0x12 at
// most) - each held by the x87 compare of the unrounded sum with the bound
// (fcomp: an unordered compare holds the top, not the bottom) - at the drop's
// depth +8; linked at kind 0x2D's cell (4, 0x38).
extern "C" void __cdecl EffectDrops_Draw(const unsigned char* drop) {
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyF4)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, 0);
    const double top = D(drop + 4) - static_cast<double>(SW(drop + 0xC));
    SetD(p + 0x18, top);
    SetD(p + 0xC, top);
    const float least = static_cast<float>(SW(drop + 0x10));
    if (!(top >= static_cast<double>(least))) {
        SetF(p + 0x18, least);
        SetF(p + 0xC, least);
    }
    const double bottom = static_cast<double>(SW(drop + 0xC)) + D(drop + 4);
    SetD(p + 0x30, bottom);
    SetD(p + 0x24, bottom);
    const float most = static_cast<float>(SW(drop + 0x12));
    if (bottom > static_cast<double>(most)) {
        SetF(p + 0x30, most);
        SetF(p + 0x24, most);
    }
    CopyQuiet(p + 0x20, drop);
    CopyQuiet(p + 8, drop);
    const double right = D(drop) + D(At(at::kDropWidth));
    SetD(p + 0x2C, right);
    SetD(p + 0x14, right);
    CopyQuiet(p + 0x34, drop + 8);
    CopyQuiet(p + 0x28, drop + 8);
    CopyQuiet(p + 0x1C, drop + 8);
    CopyQuiet(p + 0x10, drop + 8);
    Shade3(p + 4, 0x80);
    SH_CALL(MapView_LinkPrimAt)(LinkX(), LinkZ(), 4, 0x38);
}

// original 0x472EE0 (0x1B bytes): the first drop whose word +0x14 is 0, or 0.
extern "C" unsigned char* __cdecl EffectDrops_FindFree(void) {
    for (unsigned i = 0; i < at::kPoolCount; ++i)
        if (Word(Pool(at::kDropStride, i) + 0x14) == 0) return Pool(at::kDropStride, i);
    return nullptr;
}

// original 0x473100 (0x92 bytes): the drop at the record's screen point (+0xC,
// Sprite_Current read on entry): an angle Rand & 0xFC0 and a distance Rand & 0x38;
// x = (cos * d sar 12) + the point's x, y = (sin * d sar 13) + its y (floats), the
// depth copied; the word +0x12 (its floor) = _ftol(y)'s low word, +0x10, +0xC,
// +0xE = 0, +0x14 = 0x20 frames.
extern "C" void __cdecl EffectDrops_Launch(unsigned char* drop) {
    const unsigned char* const q = S() + 0xC;
    const U angle = static_cast<U>(SH_CALL(Rand)()) & 0xFC0u;
    const U distance = static_cast<U>(SH_CALL(Rand)()) & 0x38u;
    const U c = static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(angle)));
    SetD(drop, I(Sar(c * distance, 12)) + D(q));
    const U n = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(angle)));
    SetD(drop + 4, I(Sar(n * distance, 13)) + D(q + 4));
    SetUL(drop + 8, UL(q + 8));
    SetWord(drop + 0x12, Ftol(D(drop + 4)));
    SetWord(drop + 0x10, 0);
    SetWord(drop + 0xC, 0);
    SetWord(drop + 0xE, 0);
    SetWord(drop + 0x14, 0x20);
}

void Effect2A_Inject() {
    if (bof3::WantsShadow("effect_2a")) effect_2a::SelfTest();
    BOF3_INJECT(EffectKind28_Run);
    BOF3_INJECT(EffectKind28_Start);
    BOF3_INJECT(EffectKind28_WaitFlag);
    BOF3_INJECT(EffectKind28_Beam);
    BOF3_INJECT(EffectKind28_PushParty);
    BOF3_INJECT(EffectKind28_DrawBeam);
    BOF3_INJECT(EffectKind28_DrawEnd);
    BOF3_INJECT(EffectKind28_DrawSides);
    BOF3_INJECT(EffectKind29_Run);
    BOF3_INJECT(EffectKind29_Start);
    BOF3_INJECT(EffectKind29_Extend);
    BOF3_INJECT(EffectKind29_Hold);
    BOF3_INJECT(EffectKind29_Fade);
    BOF3_INJECT(EffectKind29_Linger);
    BOF3_INJECT(EffectKind29_DrawBoxes);
    BOF3_INJECT(EffectKind29_DrawFace);
    BOF3_INJECT(EffectKind29_SetSegments);
    BOF3_INJECT(EffectKind29_DrawSegments);
    BOF3_INJECT(EffectKind2A_Run);
    BOF3_INJECT(EffectKind2A_Start);
    BOF3_INJECT(EffectKind2A_Beam);
    BOF3_INJECT(EffectKind2A_Beams);
    BOF3_INJECT(EffectKind2A_PushParty);
    BOF3_INJECT(EffectKind2A_DrawBeam);
    BOF3_INJECT(EffectKind2A_DrawEnd);
    BOF3_INJECT(EffectKind2A_DrawSides);
    BOF3_INJECT(EffectKind2B_Run);
    BOF3_INJECT(EffectKind2B_Start);
    BOF3_INJECT(EffectKind2B_Rise);
    BOF3_INJECT(EffectKind2B_Specks);
    BOF3_INJECT(EffectKind2B_Settle);
    BOF3_INJECT(EffectKind2B_Shrink);
    BOF3_INJECT(EffectKind2B_Draw);
    BOF3_INJECT(EffectSpecks_Spawn);
    BOF3_INJECT(EffectSpecks_Move);
    BOF3_INJECT(EffectSpecks_Draw);
    BOF3_INJECT(EffectKind2C_Run);
    BOF3_INJECT(EffectKind2C_Start);
    BOF3_INJECT(EffectKind2C_Emit);
    BOF3_INJECT(EffectSparks_Move);
    BOF3_INJECT(EffectSparks_Draw);
    BOF3_INJECT(EffectSparks_Clear);
    BOF3_INJECT(EffectSparks_FindFree);
    BOF3_INJECT(EffectKind2E_Run);
    BOF3_INJECT(EffectKind2E_Start);
    BOF3_INJECT(EffectKind2E_Trickle);
    BOF3_INJECT(EffectKind2E_Burst);
    BOF3_INJECT(EffectKind2D_Run);
    BOF3_INJECT(EffectKind2D_Start);
    BOF3_INJECT(EffectKind2D_Spin);
    BOF3_INJECT(EffectKind2D_Pause);
    BOF3_INJECT(EffectKind2D_Open);
    BOF3_INJECT(EffectKind2D_Pour);
    BOF3_INJECT(EffectKind2D_Close);
    BOF3_INJECT(EffectKind2D_Lift);
    BOF3_INJECT(EffectKind2D_End);
    BOF3_INJECT(EffectKind2D_DrawRays);
    BOF3_INJECT(EffectKind2D_DrawDisc);
    BOF3_INJECT(EffectKind2D_DrawRings);
    BOF3_INJECT(EffectKind2D_DrawCurtain);
    BOF3_INJECT(EffectDrops_Clear);
    BOF3_INJECT(EffectDrops_Move);
    BOF3_INJECT(EffectDrops_Draw);
    BOF3_INJECT(EffectDrops_FindFree);
    BOF3_INJECT(EffectDrops_Launch);
}

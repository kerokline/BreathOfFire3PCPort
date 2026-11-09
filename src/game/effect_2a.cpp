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
#include "game/record_views.h"
#include "game/records.h"
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
void SetD(unsigned char* p, double d) { SetF(p, static_cast<float>(d)); }

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

// ===========================================================================
// Kind 0x2D's draws: the link cell and the record's screen point
// ===========================================================================

// The pool's records.
unsigned char* Pool(U stride, unsigned i) { return At(::bof3::Img(EffectKind30_Shards_addr) + stride * i); }

}  // namespace

// ===========================================================================
// Kind 0x28: Effect_KindHandlers[0x28] (0x6553F0), EffectKind28_States (four,
// the last FC1's Effect_StateRelease)
// ===========================================================================

// ===========================================================================
// Kind 0x29: Effect_KindHandlers[0x29] (0x6553F4), EffectKind29_States (six,
// the last Effect_StateRelease)
// ===========================================================================

// ===========================================================================
// Kind 0x2A: Effect_KindHandlers[0x2A] (0x6553F8), EffectKind2A_States (four,
// the last Effect_StateRelease)
// ===========================================================================

// ===========================================================================
// Kind 0x2B: Effect_KindHandlers[0x2B] (0x6553FC), EffectKind2B_States (five)
// ===========================================================================

// ===========================================================================
// Kind 0x2C: Effect_KindHandlers[0x2C] (0x655400), EffectKind2C_States (three,
// the last Effect_StateRelease)
// ===========================================================================

// ===========================================================================
// Kind 0x2E: Effect_KindHandlers[0x2E] (0x655408), EffectKind2E_States (three;
// the dword after them is kind 0x2D's table)
// ===========================================================================

// ===========================================================================
// Kind 0x2D: Effect_KindHandlers[0x2D] (0x655404), EffectKind2D_States (eight)
// ===========================================================================

void Effect2A_Inject() {
    if (bof3::WantsShadow("effect_2a")) effect_2a::SelfTest();
}

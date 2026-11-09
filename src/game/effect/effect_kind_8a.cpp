// Moved here by tools/move_funcs.py from effect_4b.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_8a.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/cheats.h"
#include "game/effect_4b.h"
#include "game/effect_4b_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "game/sprite_draw.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace {

namespace at = effect_4b::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

using Handler = scenario_harness::Handler;

unsigned char* S() { return Sprite_Current; }

U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }

std::int32_t S32(U v) { return static_cast<std::int32_t>(v); }

U AddressOf(const void* p) { return Low32(p); }

// sar: an arithmetic right shift of the 32 bits as the original holds them.
U Sar(U v, unsigned n) { return static_cast<U>(static_cast<std::int32_t>(v) >> n); }

// imul of two whole registers: the low 32 bits.
U Mul(U a, std::int32_t b) { return a * static_cast<U>(b); }

// --- x87 as the original has it (the game's control word); every value goes
// through the FPU as Capcom's does.
// `fild dword [v]; fstp dword [o]`
void Fild(std::int32_t v, void* o) {
    __asm__ volatile("fildl %1\n\tfstps (%0)" : : "r"(o), "m"(v) : "st", "memory");
}

// The CRT's _ftol 0x5B9550 on st(0): truncation toward zero set in a copy of
// the control word for one fistp to 64 bits, the word put back; the low dword
// (eax), which is all the callers keep.
// `fld dword [a]; fsub dword [b]; call _ftol`
U FldSubFtol(const void* a, const void* b) {
    std::int64_t result;
    unsigned short saved, truncating;
    __asm__ volatile(
        "flds (%[a])\n\t"
        "fsubs (%[b])\n\t"
        "fnstcw %[saved]\n\t"
        "movw %[saved], %%ax\n\t"
        "orb $0x0C, %%ah\n\t"
        "movw %%ax, %[truncating]\n\t"
        "fldcw %[truncating]\n\t"
        "fistpll %[result]\n\t"
        "fldcw %[saved]\n\t"
        : [result] "=m"(result), [saved] "=m"(saved), [truncating] "=m"(truncating)
        : [a] "r"(a), [b] "r"(b)
        : "eax", "st", "memory");
    return static_cast<U>(static_cast<std::uint64_t>(result));
}

// `fld dword [a]; call _ftol`
U FldFtol(const void* a) {
    std::int64_t result;
    unsigned short saved, truncating;
    __asm__ volatile(
        "flds (%[a])\n\t"
        "fnstcw %[saved]\n\t"
        "movw %[saved], %%ax\n\t"
        "orb $0x0C, %%ah\n\t"
        "movw %%ax, %[truncating]\n\t"
        "fldcw %[truncating]\n\t"
        "fistpll %[result]\n\t"
        "fldcw %[saved]\n\t"
        : [result] "=m"(result), [saved] "=m"(saved), [truncating] "=m"(truncating)
        : [a] "r"(a)
        : "eax", "st", "memory");
    return static_cast<U>(static_cast<std::uint64_t>(result));
}

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 1]; jmp [table + eax
// * 4]: the table's `entries` handlers read in place (the fuzz swaps the cells
// for recorders); a Fatal past them, where the original jumps through the
// dword after - the next kind's table or data.
void Dispatch(const char* who, U table, unsigned entries) {
    const unsigned state = Sprite_Current[1];
    if (state >= entries)
        bof3::Fatal("%s: state byte +1 is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/effect_4b.md section 6)",
                    who, state, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(At(table + 4 * state))))();
}

// The draw mode most of these put first: Gpu_GetTPage(0, abr, x, y) and
// Gpu_SetDrawMode at the cursor (read after the page) - its fifth argument is
// the first of the page's five pushes, a zero, left on the stack.
void DrawMode(unsigned abr, int x, int y, int dtd) {
    const unsigned tpage = SH_CALL(Gpu_GetTPage)(0, abr, x, y) & 0xFFFFu;
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, dtd, tpage, 0);
}

// Kind 0x8A's colour row, by Sprite_Current +6 (read afresh each time): one
// row of three bytes; a +6 past it reads into kKind8ACells.
unsigned char Kind8AColour(unsigned byte, const char* who) {
    const unsigned k = S()[6];
    if (k >= at::kKind8AColourCount)
        bof3::Fatal("%s: +6 is %u, past kind 0x8A's one colour row at 0x%X - the original reads the map cells after "
                    "as a colour (docs/effect_4b.md section 6)",
                    who, k, at::kKind8AColour.v);
    return At(::bof3::Img(at::kKind8AColour.v) + 3 * k + byte)[0];
}

}  // namespace

// original 0x48A5F0 (Effect_KindHandlers[0x8A], hidden after 0x48A2A0):
// jmp [EffectKind8A_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind8A_Run(void) {
    Dispatch("EffectKind8A_Run", AddressOf(EffectKind8A_States), EffectKind8A_States_count);
}

// original 0x48A610 (state 0): the six cells of kKind8ACells at x 0x10 given
// map byte 0x89 (AreaMap_SetByte); +1 = 1.
extern "C" void __cdecl EffectKind8A_Block(void) {
    for (unsigned i = 0; i < at::kKind8ACellCount; ++i) SH_CALL(AreaMap_SetByte)(0x10, At(::bof3::Img(at::kKind8ACells.v) + i)[0], 0x89);
    S()[1] = 1;
}

// original 0x48A650 (state 1): +6 0; the lines' x 0x100000 (+0xC and +0x18),
// their foot +0x3C 0 - so their top +0x20 (and +0x14) (+0x3E + 0x80) << 16;
// each of kKind8ALines' three z pairs to +0x10 / +0x1C and drawn
// (EffectKind8A_DrawSegment(+0xC, +0x18)). Story flag 0x91 set: +1 = 2.
extern "C" void __cdecl EffectKind8A_Lines(void) {
    S()[6] = 0;
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->work_18, 0x100000);
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_0c, 0x100000);
    rv::Current<records::EffectObject>()->height = 0;
    for (unsigned i = 0; i < at::kKind8ALineCount; ++i) {
        const U pair = ::bof3::Img(at::kKind8ALines.v) + 8 * i;
        rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_10, UL(At(pair)));
        rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->work_1c, UL(At(pair + 4)));
        unsigned char* s = S();
        rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->work_20, static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(rv::As<records::EffectObject>(s)->height_whole)) + 0x80) << 16);
        s = S();
        rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->step_y, static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->work_20));
        s = S();
        SH_CALL(EffectKind8A_DrawSegment)(reinterpret_cast<const long*>(s + 0xC), reinterpret_cast<const long*>(s + 0x18));
    }
    if (SH_CALL(Flags_Test)(At(::bof3::Img(Cond_Flags_addr) + 0xa0), 0x91) != 0) S()[1] = 2;
}

// original 0x48A700 (state 2): the six cells given map byte 0; Effect_Release.
extern "C" void __cdecl EffectKind8A_Unblock(void) {
    for (unsigned i = 0; i < at::kKind8ACellCount; ++i) SH_CALL(AreaMap_SetByte)(0x10, At(::bof3::Img(at::kKind8ACells.v) + i)[0], 0);
    SH_CALL(Effect_Release)();
}

// original 0x48A730 (a, b: two points of three dwords; E3C's
// EffectKind6F_DrawSegment's twin, linked at +0xC / +0x10 and without its
// closing draw mode): a draw mode (page (0x3C0, 0), dtd 1) linked at the
// record's +0xC / +0x10 with dy 1; EffectGte_LoadMapCamera; a LINE_F2 at the
// cursor, opaque, from a to b projected, kind 0x8A's colour, linked (.., 1,
// 0x20); the screen angle Math_Ratan2(dy, dx) (each through _ftol, then a
// float); the two ends' sizes EffectGte_ProjectSize(end, {0x40, 0}, out) -
// out's dword plus Frame_Counter & 1 (read after each);
// EffectKind8A_DrawRibbon(the four screen words through _ftol, the sizes, the
// angle + 0x400 and + 0xC00).
extern "C" void __cdecl EffectKind8A_DrawSegment(const long* a, const long* b) {
    DrawMode(1, 0x3C0, 0, 1);
    unsigned char* s = S();
    SH_CALL(MapView_LinkPrimAt)(rv::As<records::EffectWork>(s)->work_0c, rv::As<records::EffectWork>(s)->work_10, 1, 0xC);
    SH_CALL(EffectGte_LoadMapCamera)();
    unsigned char* const prim = Gfx_PacketNext;
    SH_CALL(Gpu_SetLineF2)(prim);
    SH_CALL(Gpu_SetSemiTrans)(prim, 0);
    SH_CALL(EffectGte_ProjectPoint)(a, reinterpret_cast<float*>(prim + 8));
    SH_CALL(EffectGte_ProjectPoint)(b, reinterpret_cast<float*>(prim + 0x14));
    prim[4] = Kind8AColour(0, "EffectKind8A_DrawSegment");
    prim[5] = Kind8AColour(1, "EffectKind8A_DrawSegment");
    prim[6] = Kind8AColour(2, "EffectKind8A_DrawSegment");
    s = S();
    SH_CALL(MapView_LinkPrimAt)(rv::As<records::EffectWork>(s)->work_0c, rv::As<records::EffectWork>(s)->work_10, 1, 0x20);
    const U dx = FldSubFtol(prim + 0x14, prim + 8);
    float fx, fy;
    Fild(S32(dx), &fx);
    const U dy = FldSubFtol(prim + 0x18, prim + 0xC);
    Fild(S32(dy), &fy);
    const U angle = static_cast<U>(SH_CALL(Math_Ratan2)(fy, fx));
    short size[2] = {0x40, 0};
    short out[2];
    SH_CALL(EffectGte_ProjectSize)(a, size, out);
    U wa;
    std::memcpy(&wa, out, 4);
    wa += Frame_Counter & 1u;
    SH_CALL(EffectGte_ProjectSize)(b, size, out);
    U wb;
    std::memcpy(&wb, out, 4);
    wb += Frame_Counter & 1u;
    const U by = FldFtol(prim + 0x18);
    const U bx = FldFtol(prim + 0x14);
    const U ay = FldFtol(prim + 0xC);
    const U ax = FldFtol(prim + 8);
    SH_CALL(EffectKind8A_DrawRibbon)(S32(ax), S32(ay), S32(wa), S32(angle + 0x400u), S32(bx), S32(by), S32(wb),
                                     S32(angle + 0xC00u));
}

// original 0x48A8E0 (ax, ay, wa, ta, bx, by, wb, tb - every one read as its low
// s16, the angles masked to 16 bits; E3C's EffectKind6F_DrawRibbon's twin,
// linked at +0xC / +0x10): two POLY_G4s at the cursor (read once),
// semi-transparent, sharing the edge (ax, ay)-(bx, by) in kind 0x8A's colour;
// the far corners black at (Math_Cos, Math_Sin) * radius >> 12 off the ends -
// the first quad at ta round a (radius wa) and tb + 0x800 round b (wb), the
// second (a copy of the first's 0x44 bytes) at ta + 0x800 and tb. Each linked
// at the record's +0xC / +0x10 (.., 1, 0x44); the second is the cursor read at
// entry + 0x44.
extern "C" void __cdecl EffectKind8A_DrawRibbon(int ax, int ay, int wa, int ta, int bx, int by, int wb, int tb) {
    unsigned char* const q = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyG4)(q);
    SH_CALL(Gpu_SetSemiTrans)(q, 1);
    const std::int32_t x0 = static_cast<short>(ax), y0 = static_cast<short>(ay);
    const std::int32_t x1 = static_cast<short>(bx), y1 = static_cast<short>(by);
    Fild(x0, q + 8);
    Fild(y0, q + 0xC);
    Fild(x1, q + 0x18);
    Fild(y1, q + 0x1C);
    const U t1 = static_cast<U>(ta) & 0xFFFFu;
    const std::int32_t r1 = static_cast<short>(wa);
    const std::int32_t r2 = static_cast<short>(wb);
    auto corner = [](int trig, std::int32_t radius, std::int32_t base) {
        return S32(Sar(Mul(static_cast<U>(trig), radius), 12) + static_cast<U>(base));
    };
    Fild(corner(SH_CALL(Math_Cos)(static_cast<int>(t1)), r1, x0), q + 0x28);
    Fild(corner(SH_CALL(Math_Sin)(static_cast<int>(t1)), r1, y0), q + 0x2C);
    const U t2 = static_cast<U>(tb) & 0xFFFFu;
    Fild(corner(SH_CALL(Math_Cos)(static_cast<int>(t2 + 0x800u)), r2, x1), q + 0x38);
    Fild(corner(SH_CALL(Math_Sin)(static_cast<int>(t2 + 0x800u)), r2, y1), q + 0x3C);
    q[4] = Kind8AColour(0, "EffectKind8A_DrawRibbon");
    q[5] = Kind8AColour(1, "EffectKind8A_DrawRibbon");
    q[6] = Kind8AColour(2, "EffectKind8A_DrawRibbon");
    q[0x14] = Kind8AColour(0, "EffectKind8A_DrawRibbon");
    q[0x15] = Kind8AColour(1, "EffectKind8A_DrawRibbon");
    q[0x16] = Kind8AColour(2, "EffectKind8A_DrawRibbon");
    q[0x24] = q[0x25] = q[0x26] = 0;
    q[0x34] = q[0x35] = q[0x36] = 0;
    unsigned char* s = S();
    SH_CALL(MapView_LinkPrimAt)(rv::As<records::EffectWork>(s)->work_0c, rv::As<records::EffectWork>(s)->work_10, 1, 0x44);
    unsigned char* const q2 = q + 0x44;
    std::memcpy(q2, q, 0x44);
    const U t3 = t1 + 0x800u;
    Fild(corner(SH_CALL(Math_Cos)(static_cast<int>(t3)), r1, x0), q2 + 0x28);
    Fild(corner(SH_CALL(Math_Sin)(static_cast<int>(t3)), r1, y0), q2 + 0x2C);
    Fild(corner(SH_CALL(Math_Cos)(static_cast<int>(t2)), r2, x1), q2 + 0x38);
    Fild(corner(SH_CALL(Math_Sin)(static_cast<int>(t2)), r2, y1), q2 + 0x3C);
    s = S();
    SH_CALL(MapView_LinkPrimAt)(rv::As<records::EffectWork>(s)->work_0c, rv::As<records::EffectWork>(s)->work_10, 1, 0x44);
}

void EffectKind8a_Inject() {
    BOF3_INJECT(EffectKind8A_Run);
    BOF3_INJECT(EffectKind8A_Block);
    BOF3_INJECT(EffectKind8A_Lines);
    BOF3_INJECT(EffectKind8A_Unblock);
    BOF3_INJECT(EffectKind8A_DrawSegment);
    BOF3_INJECT(EffectKind8A_DrawRibbon);
}

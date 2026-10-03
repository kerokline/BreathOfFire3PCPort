// Round thirteen group E3D (docs/effect_3d.md): the 63 functions of
// analysis/round13_cut.tsv's group E3D, 0x485CB0..0x48801A, and five starts the
// cut does not list (kind 0x82's state 2 0x487DE0, and kind 0x77's sub-state
// dispatcher 0x486280 and its three sub-states 0x4861C0, 0x4861E0, 0x4862A0,
// catalog rows no group of the round holds), each read with capstone to its
// last instruction. Effect_RunObjects (ours) makes each live record of
// Effect_Objects (20 of 0x80 bytes) Sprite_Current and calls
// Effect_KindHandlers[+5]; each kind here is a dispatcher by +1 through its
// state table (none bounded by a compare) and the states it names, or (kinds
// 0x76, 0x79, 0x7A) a single draw. What each kind is, as far as the code says:
//
//   kinds 0x76, 0x79, 0x7A   a full-screen quad of colour 0 committed
//               semi-transparent (abr 2, 2, 0); kind 0x79 ends when the
//               chapter counter 0x903848 is 1
//   kinds 0x7B, 0x7C   the same quad with a red that pulses between two
//               shades every 17 frames; kind 0x7C ends at counter 4
//   kind 0x77   a count in a box (to 200, one every 30 or 40 frames, a sound
//               each when the chapter's flag 0x14 is set); at 200 it sets run
//               5 step 0x19 unless the chapter's flag 0x2E is set
//   kind 0x78   a spinning ring of 32 red-and-white triangles about
//               Sprite_Objects record 2, which it turns (area 130 spawns it)
//   kind 0x7D   area 170's three dials: a panel of three 7 x 9 grids the
//               player turns, confirm or cancel; confirm applies them to the
//               map (EffectKind7D_SetMap, which area 170's init also calls)
//   kind 0x7F   a widening cone of 32 shaded quads about the record's point
//   kind 0x80   a trail of 32 points that rises along x and fades
//   kind 0x81   drops from sixteen sources about the leader, pouring until
//               the counter reaches 12, then draining
//   kind 0x82   Sprite_Objects record 0 pushed a cell at a time (three, four
//               or five pushes by 0x903849) while it is short of the leader
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies. No divergence:
// each is a faithful replacement. Where the original jumps through a state
// table past its end or indexes a table past its entries, ours aborts with a
// message (docs/effect_3d.md section 7). The x87 sequences (fild, fadd, fsub,
// fld / fstp copies, _ftol) run as the original's instructions (inline asm), so
// the rounding and a signalling NaN's quieting are the original's.
#include "game/effect_3d.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/draw_pool.h"
#include "game/effect_3d_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_3d::at;
using U = std::uint32_t;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

unsigned char* S() { return Sprite_Current; }
U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }
U UL(U a) { return UL(At(a)); }
void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }
U W(const unsigned char* p) { return Word(p); }
U W(U a) { return Word(At(a)); }
unsigned char& B(U a) { return *At(a); }
std::int32_t S16(U v) { return static_cast<std::int16_t>(v & 0xFFFFu); }
U AddressOf(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
unsigned char* P(U a) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a)); }
void Copy4(unsigned char* to, const unsigned char* from) { std::memcpy(to, from, 4); }   // mov reg, [from]; mov [to], reg
void SetBits(unsigned char* to, U bits) { std::memcpy(to, &bits, 4); }

// --- the x87 sequences, as the original's instructions --------------------------
// fild dword v; fstp dword [to]
void Fild(unsigned char* to, std::int32_t v) {
    __asm__ volatile("fildl %1\n\tfstps %0" : "=m"(*reinterpret_cast<float*>(to)) : "m"(v));
}
// fild dword v; fadd dword [f]; fstp dword [to]
void FildAdd(unsigned char* to, std::int32_t v, const unsigned char* f) {
    __asm__ volatile("fildl %1\n\tfadds %2\n\tfstps %0"
                     : "=m"(*reinterpret_cast<float*>(to))
                     : "m"(v), "m"(*reinterpret_cast<const float*>(f)));
}
// fld dword [from]; fst dword [a]; fstp dword [b]
void FldTwo(unsigned char* a, unsigned char* b, const unsigned char* from) {
    __asm__ volatile("flds %2\n\tfsts %0\n\tfstps %1"
                     : "=m"(*reinterpret_cast<float*>(a)), "=m"(*reinterpret_cast<float*>(b))
                     : "m"(*reinterpret_cast<const float*>(from)));
}
// fld dword [a]; fsub dword [b]; fstp dword [to]
void FldSub(unsigned char* to, const unsigned char* a, const unsigned char* b) {
    __asm__ volatile("flds %1\n\tfsubs %2\n\tfstps %0"
                     : "=m"(*reinterpret_cast<float*>(to))
                     : "m"(*reinterpret_cast<const float*>(a)), "m"(*reinterpret_cast<const float*>(b)));
}
// fld dword [a]; fsub dword [b]; call _ftol (0x5B9550: the control word's
// rounding set to chop, fistp qword, restored) - the low dword of the answer.
U FldSubFtol(const unsigned char* a, const unsigned char* b) {
    std::int64_t r;
    std::uint16_t cw, chop;
    __asm__ volatile("fnstcw %0" : "=m"(cw));
    chop = static_cast<std::uint16_t>(cw | 0x0C00u);
    __asm__ volatile("fldcw %3\n\tflds %1\n\tfsubs %2\n\tfistpll %0\n\tfldcw %4"
                     : "=m"(r)
                     : "m"(*reinterpret_cast<const float*>(a)), "m"(*reinterpret_cast<const float*>(b)), "m"(chop),
                       "m"(cw));
    return static_cast<U>(static_cast<std::uint64_t>(r));
}
// fld dword [a]; fcomp dword [b]; fnstsw ax; test ah, 0x40: C3, set when the two
// are equal or unordered (a NaN).
bool FcompSame(const unsigned char* a, const unsigned char* b) {
    std::uint16_t sw;
    __asm__ volatile("flds %1\n\tfcomps %2\n\tfnstsw %0"
                     : "=a"(sw)
                     : "m"(*reinterpret_cast<const float*>(a)), "m"(*reinterpret_cast<const float*>(b)));
    return (sw & 0x4000u) != 0;
}
// fild dword v; fstp dword: a float argument (whole numbers only, exact).
float FildValue(std::int32_t v) {
    float f;
    __asm__ volatile("fildl %1\n\tfstps %0" : "=m"(f) : "m"(v));
    return f;
}

using Handler = scenario_harness::Handler;

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + n]; jmp [table + eax * 4]:
// the table's `entries` handlers read in place (the fuzz swaps the cells for
// recorders); a Fatal past them, where the original jumps through the dword
// after - the next table, data or the next kind's states.
void Dispatch(const char* who, U table, unsigned entries, unsigned byte) {
    const unsigned state = Sprite_Current[byte];
    if (state >= entries)
        bof3::Fatal("%s: state byte +%u is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/effect_3d.md section 7)",
                    who, byte, state, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(table + 4 * state)))();
}

void NextState() { S()[1] = static_cast<unsigned char>(S()[1] + 1); }

// +9 down one; true when it reaches 0 (Sprite_Current read for each access).
bool CountDown() {
    S()[9] = static_cast<unsigned char>(S()[9] - 1);
    return S()[9] == 0;
}

constexpr U kFloat320 = 0x43A00000u;   // 320.0f

// The full-screen quad of kinds 0x76, 0x79, 0x7A, 0x7B and 0x7C: a draw mode
// (page Gpu_GetTPage(2, abr, 0x140, 0x140), dtd 1) committed (6, 0xC); a POLY_G4
// over (0, 0) - (320, 320), its four vertices' red `shade` (read from `cell`
// for each vertex when given) and green and blue 0, semi-transparent,
// committed (2, 0x44). The depth words are not written.
void FullScreen(unsigned abr, const unsigned char* cell) {
    const U page = SH_CALL(Gpu_GetTPage)(2, abr, 0x140, 0x140);
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, page & 0xFFFFu, 0);
    SH_CALL(Gfx_CommitPrim)(6, 0xC);
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyG4)(p);
    SetBits(p + 8, 0);
    SetBits(p + 0xC, 0);
    SetBits(p + 0x18, kFloat320);
    SetBits(p + 0x1C, 0);
    SetBits(p + 0x28, 0);
    SetBits(p + 0x2C, kFloat320);
    SetBits(p + 0x38, kFloat320);
    SetBits(p + 0x3C, kFloat320);
    p[0x14] = cell ? *cell : 0;
    p[4] = cell ? *cell : 0;
    p[0x15] = 0;
    p[5] = 0;
    p[0x16] = 0;
    p[6] = 0;
    p[0x34] = cell ? *cell : 0;
    p[0x24] = cell ? *cell : 0;
    p[0x35] = 0;
    p[0x25] = 0;
    p[0x36] = 0;
    p[0x26] = 0;
    SH_CALL(Gpu_SetSemiTrans)(Gfx_PacketNext, 1);
    SH_CALL(Gfx_CommitPrim)(2, 0x44);
}

// Kinds 0x7B / 0x7C state 1: +9 up; past 0x10 the shade is the next of the two
// (0x654AF8 by 0x676279, which alternates 0, 1) and +9 0. Then the quad.
void Pulse(const char* who) {
    unsigned char* const s = S();
    const unsigned frames = s[9];
    s[9] = static_cast<unsigned char>(frames + 1);
    if (frames >= 0x10) {
        const unsigned index = B(at::kPulseIndex);
        if (index >= at::kPulseShadeCount)
            bof3::Fatal("%s: 0x676279 is %u, past the two shades at 0x654AF8 - the original reads the bytes after "
                        "(docs/effect_3d.md section 7)",
                        who, index);
        B(at::kPulseShade) = B(at::kPulseShades + index);
        S()[9] = 0;
        const unsigned old = B(at::kPulseIndex);
        B(at::kPulseIndex) = static_cast<unsigned char>(old + 1);
        if (old >= 1) B(at::kPulseIndex) = 0;
    }
    FullScreen(2, At(at::kPulseShade));
}

// Kinds 0x7B / 0x7C state 0: +9 0, +1 1, the shade 0xF, the index 0.
void PulseStart() {
    S()[9] = 0;
    S()[1] = 1;
    B(at::kPulseShade) = 0xF;
    B(at::kPulseIndex) = 0;
}

}  // namespace

// ===========================================================================
// Kinds 0x76, 0x79, 0x7A: Effect_KindHandlers[0x76] / [0x79] / [0x7A], a draw each
// ===========================================================================

// original 0x485CB0 (Effect_KindHandlers[0x76], hidden in E3C's 0x485960): the
// full-screen quad, colour 0, abr 2.
extern "C" void __cdecl EffectKind76_Shade(void) { FullScreen(2, nullptr); }

// original 0x485D60 (Effect_KindHandlers[0x79]): the same; then, when the
// counter 0x903848 is 1, a tail jump to Effect_Release.
extern "C" void __cdecl EffectKind79_Shade(void) {
    FullScreen(2, nullptr);
    if (B(at::kCounter) == 1) SH_CALL(Effect_Release)();
}

// original 0x485E10 (Effect_KindHandlers[0x7A]): the quad with abr 0.
extern "C" void __cdecl EffectKind7A_Shade(void) { FullScreen(0, nullptr); }

// ===========================================================================
// Kinds 0x7B, 0x7C: EffectKind7B_States, EffectKind7C_States (two each)
// ===========================================================================

// original 0x485EC0 (Effect_KindHandlers[0x7B]): jmp [EffectKind7B_States + +1 * 4].
extern "C" void __cdecl EffectKind7B_Run(void) {
    Dispatch("EffectKind7B_Run", AddressOf(EffectKind7B_States), EffectKind7B_States_count, 1);
}
// original 0x485EE0 (state 0).
extern "C" void __cdecl EffectKind7B_Start(void) { PulseStart(); }
// original 0x485F10 (state 1): the pulse, the quad; no end of its own.
extern "C" void __cdecl EffectKind7B_Pulse(void) { Pulse("EffectKind7B_Pulse"); }

// original 0x486020 (Effect_KindHandlers[0x7C]): jmp [EffectKind7C_States + +1 * 4].
extern "C" void __cdecl EffectKind7C_Run(void) {
    Dispatch("EffectKind7C_Run", AddressOf(EffectKind7C_States), EffectKind7C_States_count, 1);
}
// original 0x486040 (state 0): byte for byte 0x485EE0.
extern "C" void __cdecl EffectKind7C_Start(void) { PulseStart(); }
// original 0x486070 (state 1): the pulse, the quad; then, when the counter
// 0x903848 is 4, a tail jump to Effect_Release.
extern "C" void __cdecl EffectKind7C_Pulse(void) {
    Pulse("EffectKind7C_Pulse");
    if (B(at::kCounter) == 4) SH_CALL(Effect_Release)();
}

// ===========================================================================
// Kind 0x77: EffectKind77_States (three); states 0 and 1 dispatch by +2
// ===========================================================================

// original 0x486180 (Effect_KindHandlers[0x77]): jmp [EffectKind77_States + +1 * 4].
extern "C" void __cdecl EffectKind77_Run(void) {
    Dispatch("EffectKind77_Run", AddressOf(EffectKind77_States), EffectKind77_States_count, 1);
}
// original 0x4861A0 (state 0): jmp [EffectKind77_CountSteps + +2 * 4].
extern "C" void __cdecl EffectKind77_Count(void) {
    Dispatch("EffectKind77_Count", AddressOf(EffectKind77_CountSteps), EffectKind77_CountSteps_count, 2);
}
// original 0x486280 (state 1, a start no list of the cut has): jmp
// [EffectKind77_ResumeSteps + +2 * 4] - the count without its reset.
extern "C" void __cdecl EffectKind77_Resume(void) {
    Dispatch("EffectKind77_Resume", AddressOf(EffectKind77_ResumeSteps), EffectKind77_ResumeSteps_count, 2);
}

// original 0x4861C0 (state 0's step 0): the tally word 0x939A00 = Rand() << 15
// (count 0, frames 0, the pace bit Rand's bit 0); +2 up.
extern "C" void __cdecl EffectKind77_Begin(void) {
    const U r = static_cast<U>(SH_CALL(Rand)());
    SetWord(At(at::kTally), (r << 15) & 0xFFFFu);
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
}

// original 0x4861E0 (step 1 of both, PSX twin 0x801F7240): Effect_Release when
// the counter 0x903848 is 1. Else a count of 200 (0xC8) or more: +2 up.
// Else the frames (bits 0..6 of the high byte) up one; at 30 (pace 0) or 40
// (pace 1) they are 0 and the count up, Sound_PlayEffect(0x20A) when the
// chapter row's flag 0x14 is set. The pace bit kept; a frame count of 0x80
// (from 0x7F) carries into it.
extern "C" void __cdecl EffectKind77_Tick(void) {
    if (B(at::kCounter) == 1) {
        SH_CALL(Effect_Release)();
        return;
    }
    const U word = W(at::kTally);
    unsigned count = word & 0xFFu;
    if (count >= 0xC8) {
        S()[2] = static_cast<unsigned char>(S()[2] + 1);
        return;
    }
    const unsigned pace = (word >> 15) & 1u;
    unsigned frames = (((word >> 8) & 0x7Fu) + 1u) & 0xFFu;
    if (pace == 0 ? frames >= 0x1E : frames >= 0x28) {
        count = (count + 1u) & 0xFFu;
        if (SH_CALL(Flags_Test)(P(UL(at::kFlagRow)), at::kTallySoundFlag)) SH_CALL(Sound_PlayEffect)(0x20A);
        frames = 0;
    }
    const unsigned high = ((pace << 7) | frames) & 0xFFu;
    SetWord(At(at::kTally), (high << 8) | count);
}

// original 0x4862A0 (step 2 of state 0, step 1 of state 1): nothing while a
// message is open (Field_Request 2), in game mode 4, or while the chapter
// row's flag 0x2E is set; else 0x9039A3 bit 0, the step 0x19, run 5, and a
// tail jump to Effect_Release.
extern "C" void __cdecl EffectKind77_End(void) {
    if (Field_Request == 2) return;
    if (Game_Mode == 4) return;
    if (SH_CALL(Flags_Test)(P(UL(at::kFlagRow)), at::kTallyEndFlag)) return;
    B(at::kScriptFlagsHigh) = static_cast<unsigned char>(B(at::kScriptFlagsHigh) | 1u);
    B(at::kStep) = 0x19;
    MoveScript_Var7 = 5;
    SH_CALL(Effect_Release)();
}

// original 0x4862F0 (state 2): the box and the count; then Effect_Release
// unless a message is open or the game mode is 4.
extern "C" void __cdecl EffectKind77_Show(void) {
    SH_CALL(EffectKind77_DrawCount)();
    if (Field_Request == 2) return;
    if (Game_Mode == 4) return;
    SH_CALL(Effect_Release)();
}

// original 0x486310: a menu box (0x586160(0x8A, 0x1E, 0x2D, 0x14, 1)), the
// count (0x939A00's low byte) printed by the format 0x653074 into 0x904BA0 and
// drawn by Text_DrawFont12 at (0x8F, 0x21), colour 0.
extern "C" void __cdecl EffectKind77_DrawCount(void) {
    SH_AT(void (__cdecl*)(int, int, int, int, int), at::kBox)(0x8A, 0x1E, 0x2D, 0x14, 1);
    SH_CALL(Crt_sprintf)(reinterpret_cast<char*>(P(at::kText)), reinterpret_cast<const char*>(P(at::kTallyFormat)),
                         static_cast<int>(B(at::kTally)));
    SH_CALL(Text_DrawFont12)(0x8F, 0x21, 0, P(at::kText));
}

// ===========================================================================
// Kind 0x78: EffectKind78_States (four)
// ===========================================================================

// original 0x486360 (Effect_KindHandlers[0x78], hidden in 0x486310): jmp
// [EffectKind78_States + +1 * 4].
extern "C" void __cdecl EffectKind78_Run(void) {
    Dispatch("EffectKind78_Run", AddressOf(EffectKind78_States), EffectKind78_States_count, 1);
}

// original 0x486380 (state 0): +0x32 the turn word of Sprite_Objects record 2
// (0x7DEFFA), +9 0x14, +0xC and +0x10 0, +0x2E / +0x30 its screen words
// (0x7DEFF6 / 0x7DEFF8); +1 up.
extern "C" void __cdecl EffectKind78_Start(void) {
    SetWord(S() + 0x32, W(at::kObject2Turn));
    S()[9] = 0x14;
    SetUL(S() + 0xC, 0);
    SetUL(S() + 0x10, 0);
    SetWord(S() + 0x2E, W(at::kObject2Screen));
    SetWord(S() + 0x30, W(at::kObject2Screen + 2));
    NextState();
}

namespace {
// The ring with the record's words: centre +0x2E, +0x30, radii +0xC, +0x10.
// The original pushes the dwords +0xC, +0x10 whole and the centre's words over
// the upper halves of those registers; the ring reads each as its low word.
void DrawRingOfRecord() {
    unsigned char* const s = S();
    const U rx = UL(s + 0xC), ry = UL(s + 0x10);
    SH_CALL(EffectKind78_DrawRing)((rx & 0xFFFF0000u) | W(s + 0x2E), (ry & 0xFFFF0000u) | W(s + 0x30), rx, ry);
}
}  // namespace

// original 0x4863E0 (state 1): record 2 turned 0xF00; the radii grow (+0xC by
// 0xF, +0x10 by 0xA); the ring; +9 down, at 0 +9 0x3C and +1 up.
extern "C" void __cdecl EffectKind78_Grow(void) {
    SetWord(At(at::kObject2Turn), W(at::kObject2Turn) + 0xF00u);
    SetUL(S() + 0xC, UL(S() + 0xC) + 0xF);
    SetUL(S() + 0x10, UL(S() + 0x10) + 0xA);
    DrawRingOfRecord();
    if (CountDown()) {
        S()[9] = 0x3C;
        NextState();
    }
}

// original 0x486450 (state 2): record 2 turned 0xF00, the ring; +9 down, at 0
// +1 up.
extern "C" void __cdecl EffectKind78_Spin(void) {
    SetWord(At(at::kObject2Turn), W(at::kObject2Turn) + 0xF00u);
    unsigned char* const s = S();
    // here the upper halves are the caller's (the dispatcher's ecx, Sprite_Current); the ring reads the low words
    SH_CALL(EffectKind78_DrawRing)(W(s + 0x2E), W(s + 0x30), W(s + 0xC), W(s + 0x10));
    if (CountDown()) NextState();
}

// original 0x4864A0 (state 3): record 2's turn put back from +0x32; a tail jump
// to Effect_Release.
extern "C" void __cdecl EffectKind78_End(void) {
    SetWord(At(at::kObject2Turn), W(S() + 0x32));
    SH_CALL(Effect_Release)();
}

// original 0x4864C0 (0x173 bytes, cdecl): 32 flat triangles about (cx, cy) - each
// the centre, the previous rim point and the next at angles 0x80 k, the rim
// (cx + (cos a * rx >> 12), cy + (sin a * ry >> 12)) each term's low word sign-
// extended; the first previous point at angle 0. Red (0xFF, 0, 0) or white by
// Frame_Counter bit 2 and k's parity; linked at Sprite_Objects record 2's x, z
// (MapView_LinkPrimAt, dy 0xC, 0x2C). Each argument read as its low word.
extern "C" void __cdecl EffectKind78_DrawRing(unsigned cx, unsigned cy, unsigned rx, unsigned ry) {
    const std::int32_t rxs = S16(rx);
    const U c0 = static_cast<U>(SH_CALL(Math_Cos)(0));
    const U x0 = static_cast<U>(static_cast<std::int32_t>(c0 * static_cast<U>(rxs)) >> 12);
    const std::int32_t rys = S16(ry);
    const U s0 = static_cast<U>(SH_CALL(Math_Sin)(0));
    const U y0 = static_cast<U>(static_cast<std::int32_t>(s0 * static_cast<U>(rys)) >> 12);
    const std::int32_t cxs = S16(cx), cys = S16(cy);
    unsigned char centre_x[4], centre_y[4], prev_x[4], prev_y[4];
    Fild(centre_x, cxs);
    Fild(centre_y, cys);
    Fild(prev_x, static_cast<std::int32_t>(static_cast<U>(S16(x0)) + static_cast<U>(cxs)));
    Fild(prev_y, static_cast<std::int32_t>(static_cast<U>(S16(y0)) + static_cast<U>(cys)));
    U angle = 0;
    for (U k = 0; k < 0x20; ++k) {
        unsigned char* const p = Gfx_PacketNext;
        SH_AT(void (__cdecl*)(unsigned char*), at::kPolyF3)(p);
        angle += 0x80;
        const U a = angle & 0xFFFFu;
        Copy4(p + 8, centre_x);
        Copy4(p + 0xC, centre_y);
        Copy4(p + 0x14, prev_x);
        Copy4(p + 0x18, prev_y);
        const U c = static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(a)));
        const U xr = static_cast<U>(static_cast<std::int32_t>(c * static_cast<U>(S16(rx))) >> 12);
        const U sn = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(a)));
        const U yr = static_cast<U>(static_cast<std::int32_t>(sn * static_cast<U>(S16(ry))) >> 12);
        const U x = static_cast<U>(S16(xr)) + static_cast<U>(S16(cx));
        Fild(prev_x, static_cast<std::int32_t>(x));
        Copy4(p + 0x20, prev_x);
        const U y = static_cast<U>(S16(yr)) + static_cast<U>(S16(cy));
        Fild(prev_y, static_cast<std::int32_t>(y));
        Copy4(p + 0x24, prev_y);
        p[4] = 0xFF;
        if ((((Frame_Counter >> 2) & 1u) ^ (k & 1u)) == 0) {
            p[5] = 0xFF;
            p[6] = 0xFF;
        } else {
            p[5] = 0;
            p[6] = 0;
        }
        SH_CALL(MapView_LinkPrimAt)(UL(at::kObject2Point), UL(at::kObject2Point + 4), 0xC, 0x2C);
    }
}

// ===========================================================================
// Kind 0x7D: EffectKind7D_States (four) - area 170's dials
// ===========================================================================

// original 0x486640 (Effect_KindHandlers[0x7D], hidden in 0x4864C0): jmp
// [EffectKind7D_States + +1 * 4].
extern "C" void __cdecl EffectKind7D_Run(void) {
    Dispatch("EffectKind7D_Run", AddressOf(EffectKind7D_States), EffectKind7D_States_count, 1);
}

// original 0x486660 (state 0): +1 up, +0x2E 5, +0x4C the address of dial +6
// (0x675DC8 + +6, unchecked: only an address).
extern "C" void __cdecl EffectKind7D_Start(void) {
    S()[1] = static_cast<unsigned char>(S()[1] + 1);
    SetWord(S() + 0x2E, 5);
    unsigned char* const s = S();
    SetUL(s + 0x4C, at::kDials + s[6]);
}

// original 0x486690 (state 1): the dial +0x4C points at turned by the press
// (bit 15 of Input_Pressed up, wrapping 9 to 0; bit 13 down, below 0 to 8;
// Sound_PlayEffect(0x204) when it moved); confirm (Field_ConfirmButtons):
// sound 0x205, the answer 0x9039F6 1, dial +6 the turn, +1 up one; cancel:
// sound 0x206, the answer 0xFF, +1 up two. Then the panel at (+0x2E, +0x30).
extern "C" void __cdecl EffectKind7D_Input(void) {
    unsigned char* dial = P(UL(S() + 0x4C));
    const unsigned before = *dial;
    if (UL(at::kInputPressed) & 0x8000u) {
        *dial = static_cast<unsigned char>(before + 1);
        dial = P(UL(S() + 0x4C));
        if (*dial >= 9) *dial = 0;
    }
    if (UL(at::kInputPressed) & 0x2000u) {
        dial = P(UL(S() + 0x4C));
        *dial = static_cast<unsigned char>(*dial - 1);
        dial = P(UL(S() + 0x4C));
        if (static_cast<signed char>(*dial) < 0) *dial = 8;
    }
    if (before != *P(UL(S() + 0x4C))) SH_CALL(Sound_PlayEffect)(0x204);
    const U pressed = UL(at::kInputPressed);
    if ((Field_ConfirmButtons & pressed & 0xFFFFu) != 0) {
        SH_CALL(Sound_PlayEffect)(0x205);
        unsigned char* const s = S();
        const unsigned which = s[6];
        const unsigned char turn = *P(UL(s + 0x4C));
        SetWord(At(at::kDialAnswer), 1);
        if (which >= at::kDialCount)
            bof3::Fatal("EffectKind7D_Input: +6 is %u, past the three dials at 0x675DC8 - the original writes the byte "
                        "after (docs/effect_3d.md section 7)",
                        which);
        B(at::kDials + which) = turn;
        s[1] = static_cast<unsigned char>(s[1] + 1);
    } else if ((Field_CancelButtons & pressed & 0xFFFFu) != 0) {
        SH_CALL(Sound_PlayEffect)(0x206);
        unsigned char* const s = S();
        SetWord(At(at::kDialAnswer), 0xFF);
        s[1] = static_cast<unsigned char>(s[1] + 2);
    }
    unsigned char* const s = S();
    SH_CALL(EffectKind7D_DrawPanel)(W(s + 0x2E), W(s + 0x30));
}

// original 0x486790 (state 2): the dials applied to the map, +1 up (state 3 is
// Effect_StateRelease).
extern "C" void __cdecl EffectKind7D_Apply(void) {
    SH_CALL(EffectKind7D_SetMap)();
    NextState();
}

// original 0x486C90 (cdecl): a draw mode (page Gpu_GetTPage(0, abr's low byte,
// 0x380, 0x100), dfe 0, dtd 0) committed (2, 0xC).
extern "C" void __cdecl EffectKind7D_DrawMode(unsigned abr) {
    const U page = SH_CALL(Gpu_GetTPage)(0, abr & 0xFFu, 0x380, 0x100);
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, page & 0xFFFFu, 0);
    SH_CALL(Gfx_CommitPrim)(2, 0xC);
}

// original 0x486AB0 (cdecl, twelve words): a POLY_F4 (Gpu_SetPolyF4) at the
// packet cursor, semi-transparent by abe's low byte; its four points the
// coordinates' low words (fild), the colour (r, g, b) bytes; committed (2, 0x38).
extern "C" void __cdecl EffectKind7D_FillF4(unsigned x0, unsigned y0, unsigned x1, unsigned y1, unsigned x2,
                                             unsigned y2, unsigned x3, unsigned y3, unsigned r, unsigned g,
                                             unsigned b, unsigned abe) {
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyF4)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, abe & 0xFFu);
    Fild(p + 8, S16(x0));
    Fild(p + 0xC, S16(y0));
    Fild(p + 0x14, S16(x1));
    Fild(p + 0x18, S16(y1));
    Fild(p + 0x20, S16(x2));
    Fild(p + 0x24, S16(y2));
    p[4] = static_cast<unsigned char>(r);
    p[5] = static_cast<unsigned char>(g);
    Fild(p + 0x2C, S16(x3));
    p[6] = static_cast<unsigned char>(b);
    Fild(p + 0x30, S16(y3));
    SH_CALL(Gfx_CommitPrim)(2, 0x38);
}

// original 0x486B70 (cdecl, ten words): the same with a flat triangle (0x5A7570),
// three points; committed (2, 0x2C).
extern "C" void __cdecl EffectKind7D_FillF3(unsigned x0, unsigned y0, unsigned x1, unsigned y1, unsigned x2,
                                             unsigned y2, unsigned r, unsigned g, unsigned b, unsigned abe) {
    unsigned char* const p = Gfx_PacketNext;
    SH_AT(void (__cdecl*)(unsigned char*), at::kPolyF3)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, abe & 0xFFu);
    Fild(p + 8, S16(x0));
    Fild(p + 0xC, S16(y0));
    Fild(p + 0x14, S16(x1));
    Fild(p + 0x18, S16(y1));
    p[4] = static_cast<unsigned char>(r);
    p[5] = static_cast<unsigned char>(g);
    Fild(p + 0x20, S16(x2));
    p[6] = static_cast<unsigned char>(b);
    Fild(p + 0x24, S16(y2));
    SH_CALL(Gfx_CommitPrim)(2, 0x2C);
}

// original 0x486C10 (cdecl, eight words): the same with a flat line
// (Gpu_SetLineF2), two points; committed (2, 0x20).
extern "C" void __cdecl EffectKind7D_Line(unsigned x0, unsigned y0, unsigned x1, unsigned y1, unsigned r,
                                           unsigned g, unsigned b, unsigned abe) {
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetLineF2)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, abe & 0xFFu);
    Fild(p + 8, S16(x0));
    Fild(p + 0xC, S16(y0));
    Fild(p + 0x14, S16(x1));
    p[4] = static_cast<unsigned char>(r);
    p[5] = static_cast<unsigned char>(g);
    p[6] = static_cast<unsigned char>(b);
    Fild(p + 0x18, S16(y1));
    SH_CALL(Gfx_CommitPrim)(2, 0x20);
}

// original 0x486CE0 (cdecl): a grid cell's mark at (x, y) by its byte - 1 a red
// cross ((x+2, y+5) - (x+8, y+5) and (x+5, y+2) - (x+5, y+8)), 0xFF a blue bar
// ((x+2, y+5) - (x+8, y+5)), anything else nothing.
extern "C" void __cdecl EffectKind7D_DrawMark(unsigned x, unsigned y, unsigned mark) {
    const unsigned m = mark & 0xFFu;
    if (m == 1) {
        SH_CALL(EffectKind7D_Line)(x + 2, y + 5, x + 8, y + 5, 0xC8, 0, 0, 0);
        SH_CALL(EffectKind7D_Line)(x + 5, y + 2, x + 5, y + 8, 0xC8, 0, 0, 0);
    } else if (m == 0xFF) {
        SH_CALL(EffectKind7D_Line)(x + 2, y + 5, x + 8, y + 5, 0, 0, 0xC8, 0);
    }
}

// original 0x486990 (cdecl): one dial at (x, y): a grey quad 0x5A by 0x46 (abr
// 2), then (abr 1) ten vertical and eight horizontal grey lines 10 apart, and
// the grid's marks: column r at x + 10 r shows the grid's column (turn + r)
// mod 9, row b at y + 10 b its row b (grid + 9 b). `turn` read as its low byte.
extern "C" void __cdecl EffectKind7D_DrawDial(unsigned x, unsigned y, const unsigned char* grid, unsigned turn) {
    SH_CALL(EffectKind7D_DrawMode)(2);
    SH_CALL(EffectKind7D_FillF4)(x, y, x + 0x5A, y, x, y + 0x46, x + 0x5A, y + 0x46, 0x40, 0x40, 0x40, 1);
    SH_CALL(EffectKind7D_DrawMode)(1);
    for (U b = 0; b < 10; ++b) SH_CALL(EffectKind7D_Line)(x + 10 * b, y, x + 10 * b, y + 0x45, 0x96, 0x96, 0x96, 0);
    for (U b = 0; b < 8; ++b) SH_CALL(EffectKind7D_Line)(x, y + 10 * b, x + 0x59, y + 10 * b, 0x96, 0x96, 0x96, 0);
    unsigned t = turn & 0xFFu;
    for (U r = 0; r < at::kGridColumns; ++r) {
        if (t >= at::kGridColumns)
            bof3::Fatal("EffectKind7D_DrawDial: the turn is %u, past the grid's nine columns - the original reads on into "
                        "the next rows (docs/effect_3d.md section 7)",
                        t);
        const unsigned char* cell = grid + t;
        for (U b = 0; b < at::kGridRows; ++b) {
            SH_CALL(EffectKind7D_DrawMark)(x + 10 * r, y + 10 * b, *cell);
            cell += at::kGridColumns;
        }
        t = (t + 1) & 0xFFu;
        if (t >= at::kGridColumns) t = 0;
    }
}

// original 0x4867A0 (cdecl): the panel at (x, y): three green bars (abr 0), the
// three dials (0x63CA48, 0x63CA87, 0x63CAC6 at x + 9, x + 0x6D, x + 0xD1, y +
// 10, by the dials 0x675DC8..CA), and while Frame_Counter has bit 4 two white
// arrows and a white frame about dial +6.
extern "C" void __cdecl EffectKind7D_DrawPanel(unsigned x, unsigned y) {
    SH_CALL(EffectKind7D_DrawMode)(0);
    SH_CALL(EffectKind7D_FillF4)(x, y + 5, x + 5, y, x, y + 0x55, x + 5, y + 0x5A, 0, 0x80, 0, 1);
    SH_CALL(EffectKind7D_FillF4)(x + 5, y, x + 0x131, y, x + 5, y + 0x5A, x + 0x131, y + 0x5A, 0, 0x80, 0, 1);
    SH_CALL(EffectKind7D_FillF4)(x + 0x131, y, x + 0x136, y + 5, x + 0x131, y + 0x5A, x + 0x136, y + 0x55, 0, 0x80, 0, 1);
    const U xm = x - 1, yd = y + 0xA;
    SH_CALL(EffectKind7D_DrawDial)(xm + 0xA, yd, P(at::kDialGrids), B(at::kDials));
    SH_CALL(EffectKind7D_DrawDial)(xm + 0x6E, yd, P(at::kDialGrids + at::kGridSize), B(at::kDials + 1));
    SH_CALL(EffectKind7D_DrawDial)(xm + 0xD2, yd, P(at::kDialGrids + 2 * at::kGridSize), B(at::kDials + 2));
    if ((Frame_Counter & 0x10u) == 0) return;
    const U ya = y + 0x2D;
    const U l = xm + 100u * S()[6];
    SH_CALL(EffectKind7D_FillF3)(l + 7, ya - 4, l + 3, ya, l + 7, ya + 4, 0xC8, 0xC8, 0xC8, 0);
    const U rgt = xm + 100u * (S()[6] + 1u);
    SH_CALL(EffectKind7D_FillF3)(rgt + 4, ya - 4, rgt + 8, ya, rgt + 4, ya + 4, 0xC8, 0xC8, 0xC8, 0);
    const U left = xm + 100u * S()[6] + 0xA, right = left + 0x5A, bottom = yd + 0x46;
    SH_CALL(EffectKind7D_Line)(left, yd, right, yd, 0xC8, 0xC8, 0xC8, 0);
    SH_CALL(EffectKind7D_Line)(left, yd, left, bottom, 0xC8, 0xC8, 0xC8, 0);
    SH_CALL(EffectKind7D_Line)(left, bottom, right, bottom, 0xC8, 0xC8, 0xC8, 0);
    SH_CALL(EffectKind7D_Line)(right, yd, right, bottom, 0xC8, 0xC8, 0xC8, 0);
}

// original 0x486D60 (0x1A6 bytes, PSX twin 0x801F3994; called by Area170_Init
// and kind 0x7D's state 2): the 7 x 9 cells from (0x16, 0x8E) on the map. A
// cell is on when story flag 0x7E is clear and any of the three grids, turned
// by its dial, marks it (grid i's row, column (dial + column) - 9 when that is
// 9 or more). Each cell: AreaMap_SetByte(column, row, the height 0x654BCC
// holds when on, else 0); the low byte of the area block's dword the cell word
// names (AreaMap_Header: width w, depth d, base; the word at 0x8CB5AC + 2 (w
// row + column + 2 base), the dword (d w + 1) / 2 + base + that word) set from
// 0x654B4C (off or on); and the draw item MapView_ItemAt answers there, if any,
// retextured from that dword (Prim_SetTexture, 2).
extern "C" void __cdecl EffectKind7D_SetMap(void) {
    const bool open = SH_CALL(Flags_Test)(P(at::kStoryFlags), at::kDialsSolvedFlag) == 0;
    for (U row = 0; row < at::kGridRows; ++row) {
        for (U column = 0; column < at::kGridColumns; ++column) {
            unsigned on = 0;
            if (open) {
                unsigned sum = 0;
                for (U i = 0; i < at::kDialCount; ++i) {
                    unsigned turned = (B(at::kDials + i) + column) & 0xFFu;
                    if (turned >= 9) turned = (turned - 9) & 0xFFu;
                    if (turned >= at::kGridColumns)
                        bof3::Fatal("EffectKind7D_SetMap: dial %u is %u, past the grid's nine columns - the original "
                                    "reads on into the next rows (docs/effect_3d.md section 7)",
                                    (unsigned)i, (unsigned)B(at::kDials + i));
                    sum = (sum + B(at::kDialGrids + at::kGridSize * i + at::kGridColumns * row + turned)) & 0xFFu;
                }
                on = sum != 0 ? 1u : 0u;
            }
            const unsigned height = (B(at::kMapHeights + at::kGridColumns * row + column) * on) & 0xFFu;
            SH_CALL(AreaMap_SetByte)(column + at::kMapColumn0, row + at::kMapRow0, height);
            const U width = B(at::kAreaHeader);
            const U z = row + at::kMapRow0;
            const U base = W(at::kAreaHeader + 2);
            const U word = W(at::kAreaWords + 2 * (width * z + column + 2 * base));
            const U index = (B(at::kAreaHeader + 1) * width + 1) / 2 + base + word;
            unsigned char* const cell = P(at::kAreaHeader + 4 * index);
            SetUL(cell, UL(cell) & 0xFFFFFF00u);
            SetUL(cell, UL(cell) | B(at::kMapBits + at::kGridColumns * (at::kGridRows * on + row) + column));
            const U item = static_cast<U>(SH_CALL(MapView_ItemAt)(static_cast<long>(column + at::kMapColumn0),
                                                                 static_cast<long>(z)));
            if (item != 0)
                SH_CALL(Prim_SetTexture)(UL(cell), draw_pool::Items() + item * at::kDrawItemStride, 2);
        }
    }
}

// ===========================================================================
// Kind 0x7F: EffectKind7F_States (three)
// ===========================================================================

// original 0x486F10 (Effect_KindHandlers[0x7F], hidden in 0x486D60): jmp
// [EffectKind7F_States + +1 * 4].
extern "C" void __cdecl EffectKind7F_Run(void) {
    Dispatch("EffectKind7F_Run", AddressOf(EffectKind7F_States), EffectKind7F_States_count, 1);
}

// original 0x486F30 (state 0): +0x2E 0, +0x30 0x100, +9 0x28, +1 up;
// Sound_PlayEffect(0x206).
extern "C" void __cdecl EffectKind7F_Start(void) {
    SetWord(S() + 0x2E, 0);
    SetWord(S() + 0x30, 0x100);
    S()[9] = 0x28;
    NextState();
    SH_CALL(Sound_PlayEffect)(0x206);
}

// original 0x486F70 (state 1): the cone at the record's point (+0x34) between
// the radii +0x2E and +0x30, both then 0x40 wider; +9 down, at 0 +1 up.
extern "C" void __cdecl EffectKind7F_Grow(void) {
    unsigned char* const s = S();
    SH_CALL(EffectKind7F_DrawCone)(reinterpret_cast<const long*>(s + 0x34), W(s + 0x2E), W(s + 0x30));
    SetWord(S() + 0x2E, W(S() + 0x2E) + 0x40);
    SetWord(S() + 0x30, W(S() + 0x30) + 0x40);
    if (CountDown()) NextState();
}

namespace {
// The cone's rim point at angle a and radius r about the point: (x, z + (cos a r
// >> 4), height + (sin a r << 4)), each product a 32-bit imul; projected. The
// original writes x only for the inner point of each pair (the outer reuses it).
void ConePoint(long* rim, const long* point, U a, unsigned radius, float* out, bool inner) {
    if (inner) rim[0] = point[0];
    const U c = static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(a)));
    rim[1] = static_cast<long>(static_cast<U>(static_cast<std::int32_t>(c * static_cast<U>(S16(radius))) >> 4) +
                               static_cast<U>(point[1]));
    const U sn = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(a)));
    rim[2] = static_cast<long>(((sn * static_cast<U>(S16(radius))) << 4) + static_cast<U>(point[2]));
    SH_CALL(EffectGte_ProjectPoint)(rim, out);
}
}  // namespace

// original 0x486FC0 (0x2A1 bytes, cdecl; PSX twin 0x801F2DA0): a draw mode (page
// Gpu_GetTPage(0, 1, 0x380, 0x100), dtd 1) committed (2, 0xC); the map camera;
// then 32 Gouraud quads round the point at angles 0x80 k between the inner rim
// (radius r1) and the outer (r2), grey 0x80 inside and black outside, each with
// a draw mode before it, both linked at (x, the previous outer point's z).
// r1 and r2 read as their low words.
extern "C" void __cdecl EffectKind7F_DrawCone(const long* point, unsigned r1, unsigned r2) {
    const U page = SH_CALL(Gpu_GetTPage)(0, 1, 0x380, 0x100);
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, page & 0xFFFFu, 0);
    SH_CALL(Gfx_CommitPrim)(2, 0xC);
    SH_CALL(EffectGte_LoadMapCamera)();
    long rim[3];
    float inner[3], outer[3];
    ConePoint(rim, point, 0, r1, inner, true);
    ConePoint(rim, point, 0, r2, outer, false);
    U angle = 0;
    for (int n = 0x20; n != 0; --n) {
        const U lx = static_cast<U>(rim[0]), lz = static_cast<U>(rim[1]);
        const U pg = SH_CALL(Gpu_GetTPage)(0, 1, 0x380, 0x100);
        SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, pg & 0xFFFFu, 0);
        SH_CALL(MapView_LinkPrimAt)(lx, lz, 0, 0xC);
        unsigned char* const q = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG4)(q);
        SH_CALL(Gpu_SetSemiTrans)(q, 1);
        std::memcpy(q + 8, inner, 12);
        std::memcpy(q + 0x18, outer, 12);
        angle += 0x80;
        const U a = angle & 0xFFFFu;
        ConePoint(rim, point, a, r1, inner, true);
        ConePoint(rim, point, a, r2, outer, false);
        std::memcpy(q + 0x28, inner, 12);
        std::memcpy(q + 0x38, outer, 12);
        q[4] = 0x80;
        q[5] = 0x80;
        q[6] = 0x80;
        q[0x14] = 0;
        q[0x15] = 0;
        q[0x16] = 0;
        q[0x24] = 0x80;
        q[0x25] = 0x80;
        q[0x26] = 0x80;
        q[0x34] = 0;
        q[0x35] = 0;
        q[0x36] = 0;
        SH_CALL(MapView_LinkPrimAt)(lx, lz, 0, 0x44);
    }
}

// ===========================================================================
// Kind 0x80: EffectKind80_States (nine; 4..7 are 0x492750, a catalog row no
// group of the round holds, and 8 Effect_StateRelease)
// ===========================================================================

// original 0x487270 (Effect_KindHandlers[0x80], hidden in 0x486FC0): jmp
// [EffectKind80_States + +1 * 4].
extern "C" void __cdecl EffectKind80_Run(void) {
    Dispatch("EffectKind80_Run", AddressOf(EffectKind80_States), EffectKind80_States_count, 1);
}

// original 0x487290 (state 0): the point (1, 0x25, 0x38 cells: 0x10000,
// 0x250000, 0x3800000), the trail's size 0x60, the trail stepped 32 times (all
// its points at the point); +9 0, +1 up.
extern "C" void __cdecl EffectKind80_Start(void) {
    SetUL(S() + 0x34, 0x10000);
    SetUL(S() + 0x38, 0x250000);
    SetUL(S() + 0x3C, 0x3800000);
    SetWord(At(at::kTrailSize), 0x60);
    for (int n = 0x20; n != 0; --n) SH_CALL(EffectKind80_TrailStep)(P(at::kTrail));
    S()[9] = 0;
    NextState();
}

// original 0x4872F0 (state 1): +9 up (Sound_PlayEffect(0x200) on the first
// frame); the trail stepped and drawn; x a cell on; at x = 0x17 cells +9 0x20
// and +1 up.
extern "C" void __cdecl EffectKind80_Rise(void) {
    unsigned char* const s = S();
    const unsigned frames = s[9];
    s[9] = static_cast<unsigned char>(frames + 1);
    if (frames == 0) SH_CALL(Sound_PlayEffect)(0x200);
    SH_CALL(EffectKind80_TrailStep)(P(at::kTrail));
    SH_CALL(EffectKind80_DrawTrail)(P(at::kTrail));
    SetUL(S() + 0x34, UL(S() + 0x34) + 0x10000);
    if (UL(S() + 0x34) == 0x170000) {
        S()[9] = 0x20;
        NextState();
    }
}

// original 0x487360 (state 2): stepped and drawn; +9 down, at 0 +9 0x10 and +1 up.
extern "C" void __cdecl EffectKind80_Hold(void) {
    SH_CALL(EffectKind80_TrailStep)(P(at::kTrail));
    SH_CALL(EffectKind80_DrawTrail)(P(at::kTrail));
    if (CountDown()) {
        S()[9] = 0x10;
        NextState();
    }
}

// original 0x4873A0 (state 3): stepped and drawn, the size 6 narrower; +9 down,
// at 0 +1 up.
extern "C" void __cdecl EffectKind80_Fade(void) {
    SH_CALL(EffectKind80_TrailStep)(P(at::kTrail));
    SH_CALL(EffectKind80_DrawTrail)(P(at::kTrail));
    SetWord(At(at::kTrailSize), W(at::kTrailSize) - 6u);
    if (CountDown()) NextState();
}

// original 0x4873E0 (0x1D1 bytes, cdecl): the trail at `pool` - 32 points of
// 0x20 (+0 x, z, height; +0x10 the screen x, y, depth; +0x1C the angle and
// +0x1E the half width, words), 31 angle words at +0x400, the size word
// +0x43E. The points move down one (31 is dropped), point 0 is the record's
// point; each projected (EffectGte_ProjectPoint) and its half width the first
// word EffectGte_ProjectSize gives for (size, size). Each pair's screen
// direction (Math_Ratan2 of the differences, each _ftol'd to a word; 0x1000 for
// none), a 0x1000 filled from the next real one ahead or else behind (0 when
// there is none); each point's angle the mean of its two neighbours'
// (EffectAngle_Mean), the ends their own.
extern "C" void __cdecl EffectKind80_TrailStep(unsigned char* pool) {
    for (int i = static_cast<int>(at::kTrailPoints) - 2; i >= 0; --i)
        std::memcpy(pool + at::kTrailStride * (i + 1), pool + at::kTrailStride * i, at::kTrailStride);
    SetUL(pool, UL(S() + 0x34));
    SetUL(pool + 4, UL(S() + 0x38));
    SetUL(pool + 8, UL(S() + 0x3C));
    SH_CALL(EffectGte_LoadMapCamera)();
    for (U i = 0; i < at::kTrailPoints; ++i) {
        unsigned char* const p = pool + at::kTrailStride * i;
        SH_CALL(EffectGte_ProjectPoint)(reinterpret_cast<const long*>(p), reinterpret_cast<float*>(p + 0x10));
        const auto w = static_cast<short>(W(pool + 0x43E));
        short size[2] = {w, w};
        short out[2];
        SH_CALL(EffectGte_ProjectSize)(reinterpret_cast<const long*>(p), size, out);
        SetWord(p + 0x1E, static_cast<U>(static_cast<std::uint16_t>(out[0])));
    }
    unsigned char* const angles = pool + 0x400;
    for (U i = 0; i < at::kTrailPoints - 1; ++i) {
        const unsigned char* const a = pool + at::kTrailStride * i + 0x10;
        const unsigned char* const b = a + at::kTrailStride;
        const U dx = FldSubFtol(a, b) & 0xFFFFu;
        const U dy = FldSubFtol(a + 4, b + 4) & 0xFFFFu;
        if (dx == 0 && dy == 0) {
            SetWord(angles + 2 * i, 0x1000);
        } else {
            const int angle = SH_CALL(Math_Ratan2)(FildValue(S16(dy)), FildValue(S16(dx)));
            SetWord(angles + 2 * i, static_cast<U>(angle) & 0xFFFFu);
        }
    }
    const unsigned n = at::kTrailPoints - 1;   // 31 angles
    for (unsigned c = 0; c < n; ++c) {
        if (W(angles + 2 * c) != 0x1000) continue;
        unsigned found = n;
        for (unsigned a = c + 1; a < n; ++a)
            if (W(angles + 2 * a) != 0x1000) {
                found = a;
                break;
            }
        if (found == n)
            for (unsigned a = 0; a < c; ++a)
                if (W(angles + 2 * a) != 0x1000) {
                    found = a;
                    break;
                }
        SetWord(angles + 2 * c, found == n ? 0u : W(angles + 2 * found));
    }
    SetWord(pool + 0x1C, W(angles));
    for (U i = 0; i + 1 < n; ++i) {
        const long mean = SH_CALL(EffectAngle_Mean)(W(angles + 2 * i), W(angles + 2 * (i + 1)));
        SetWord(pool + at::kTrailStride * (i + 1) + 0x1C, static_cast<U>(mean) & 0xFFFFu);
    }
    SetWord(pool + at::kTrailStride * (at::kTrailPoints - 1) + 0x1C, W(angles + 2 * (n - 1)));
}

// original 0x4875C0 (0x2D0 bytes, cdecl): the trail at `pool` drawn - a draw mode
// (page Gpu_GetTPage(0, 1, 0x3C0, 0), dtd 1) committed (1, 0xC); point 0's end
// cap (EffectTrail_DrawCap, its angle - 0x400, shade 0x80); for each pair whose
// screen points differ, two Gouraud quads, each from the pair's points to their
// rims at the angles + 0x400 and - 0x400 (cos / sin of the half width >> 12),
// the shade at the first point and 4 less at the second, black at the rims,
// semi-transparent, committed (1, 0x44) - the second a copy of the first with
// its rims moved; the shade 4 less a pair drawn. Point 31's cap last.
extern "C" void __cdecl EffectKind80_DrawTrail(unsigned char* pool) {
    const U page = SH_CALL(Gpu_GetTPage)(0, 1, 0x3C0, 0);
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, page & 0xFFFFu, 0);
    SH_CALL(Gfx_CommitPrim)(1, 0xC);
    unsigned char shade = 0x80;
    SH_CALL(EffectTrail_DrawCap)(pool + 0x10, W(pool + 0x1E), (W(pool + 0x1C) - 0x400u) & 0xFFFFu, 0x80);
    for (U i = 0; i < at::kTrailPoints - 1; ++i) {
        const unsigned char* const a = pool + at::kTrailStride * i + 0x10;
        const unsigned char* const b = a + at::kTrailStride;
        if (FcompSame(a, b) && FcompSame(a + 4, b + 4)) continue;
        unsigned char* const q = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG4)(q);
        SH_CALL(Gpu_SetSemiTrans)(q, 1);
        Copy4(q + 8, a);
        Copy4(q + 0xC, a + 4);
        const U ca = static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(W(a + 0xC) + 0x400u)));
        FildAdd(q + 0x18, static_cast<std::int32_t>(ca * W(a + 0xE)) >> 12, a);
        const U sa = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(W(a + 0xC) + 0x400u)));
        FildAdd(q + 0x1C, static_cast<std::int32_t>(sa * W(a + 0xE)) >> 12, a + 4);
        FldTwo(q + 0x20, q + 0x10, a + 8);
        Copy4(q + 0x28, b);
        Copy4(q + 0x2C, b + 4);
        const U cb = static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(W(b + 0xC) + 0x400u)));
        FildAdd(q + 0x38, static_cast<std::int32_t>(cb * W(b + 0xE)) >> 12, b);
        const U sb = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(W(b + 0xC) + 0x400u)));
        FildAdd(q + 0x3C, static_cast<std::int32_t>(sb * W(b + 0xE)) >> 12, b + 4);
        const auto next = static_cast<unsigned char>(shade - 4);
        FldTwo(q + 0x40, q + 0x30, b + 8);
        q[6] = shade;
        q[5] = shade;
        q[4] = shade;
        q[0x16] = 0;
        q[0x15] = 0;
        q[0x14] = 0;
        q[0x26] = next;
        q[0x25] = next;
        q[0x24] = next;
        q[0x36] = 0;
        q[0x35] = 0;
        q[0x34] = 0;
        SH_CALL(Gfx_CommitPrim)(1, 0x44);
        unsigned char* const r = Gfx_PacketNext;
        for (unsigned d = 0; d < 0x11; ++d) Copy4(r + 4 * d, r - 0x44 + 4 * d);   // rep movsd
        const U c2 = static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(W(a + 0xC) - 0x400u)));
        FildAdd(r + 0x18, static_cast<std::int32_t>(c2 * W(a + 0xE)) >> 12, a);
        const U s2 = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(W(a + 0xC) - 0x400u)));
        FildAdd(r + 0x1C, static_cast<std::int32_t>(s2 * W(a + 0xE)) >> 12, a + 4);
        const U c3 = static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(W(b + 0xC) - 0x400u)));
        FildAdd(r + 0x38, static_cast<std::int32_t>(c3 * W(b + 0xE)) >> 12, b);
        const U s3 = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(W(b + 0xC) - 0x400u)));
        FildAdd(r + 0x3C, static_cast<std::int32_t>(s3 * W(b + 0xE)) >> 12, b + 4);
        SH_CALL(Gfx_CommitPrim)(1, 0x44);
        shade = next;
    }
    SH_CALL(EffectTrail_DrawCap)(pool + 0x3F0, W(pool + 0x3FE), (W(pool + 0x3FC) + 0x400u) & 0xFFFFu, shade);
}

// ===========================================================================
// Kind 0x81: EffectKind81_States (three) - drops from sixteen sources
// ===========================================================================

// original 0x487890 (Effect_KindHandlers[0x81], hidden in 0x4875C0): jmp
// [EffectKind81_States + +1 * 4].
extern "C" void __cdecl EffectKind81_Run(void) {
    Dispatch("EffectKind81_Run", AddressOf(EffectKind81_States), EffectKind81_States_count, 1);
}

// original 0x4878B0 (state 0): the record's point the leader's (ObjTrio +0x34,
// +0x38, +0x3C); the drops cleared, the sources placed; +1 up.
extern "C" void __cdecl EffectKind81_Start(void) {
    SetUL(S() + 0x34, UL(at::kLeaderPoint));
    SetUL(S() + 0x38, UL(at::kLeaderPoint + 4));
    SetUL(S() + 0x3C, UL(at::kLeaderPoint + 8));
    SH_CALL(EffectKind81_ClearDrops)();
    SH_CALL(EffectKind81_PlaceSources)();
    NextState();
}

// original 0x4878F0 (state 1): +1 up when the counter 0x903848 is 12; the
// sources emit; a tail jump to EffectKind81_MoveDrops (its answer unread).
extern "C" void __cdecl EffectKind81_Pour(void) {
    if (B(at::kCounter) == 0xC) NextState();
    SH_CALL(EffectKind81_Emit)();
    SH_CALL(EffectKind81_MoveDrops)();
}

// original 0x487910 (state 2): the drops moved; when none was in use, a tail
// jump to Effect_Release.
extern "C" void __cdecl EffectKind81_Drain(void) {
    if (SH_CALL(EffectKind81_MoveDrops)() != 0) return;
    SH_CALL(Effect_Release)();
}

// original 0x487920 (cdecl; also called by 0x492530, a catalog row): bytes 0..2
// of the 256 drop records at 0x92BF80 (0x18 each) cleared.
extern "C" void __cdecl EffectKind81_ClearDrops(void) {
    for (U i = 0; i < at::kDropCount; ++i) {
        unsigned char* const d = P(at::kDrops + at::kDropStride * i);
        d[0] = 0;
        d[1] = 0;
        d[2] = 0;
    }
}

// original 0x487940 (cdecl): the map camera; the moving count 0x67627C 0; each
// drop in use counted, its speed +0x14 0x20000 more and its height +0xC moved
// by the new speed, its blink bit +3 flipped, drawn, its life +2
// down - at 0 its +0 and +1 cleared. al 1 when any was in use, else 0.
extern "C" unsigned char __cdecl EffectKind81_MoveDrops(void) {
    unsigned char any = 0;
    SetWord(At(at::kDropsMoving), 0);
    SH_CALL(EffectGte_LoadMapCamera)();
    for (U i = 0; i < at::kDropCount; ++i) {
        unsigned char* const d = P(at::kDrops + at::kDropStride * i);
        if (d[0] == 0) continue;
        const U speed = UL(d + 0x14) + 0x20000u;
        const U height = UL(d + 0xC);
        SetWord(At(at::kDropsMoving), W(at::kDropsMoving) + 1u);
        any = 1;
        const auto blink = static_cast<unsigned char>(d[3] ^ 1u);
        SetUL(d + 0x14, speed);
        SetUL(d + 0xC, height + speed);
        d[3] = blink;
        SH_CALL(EffectKind81_DrawDrop)(d);
        d[2] = static_cast<unsigned char>(d[2] - 1);
        if (d[2] == 0) {
            d[0] = 0;
            d[1] = 0;
        }
    }
    return any;
}

// original 0x4879C0 (cdecl): a drop (+4 x, z, height) drawn when its blink bit
// +3 is set: a black one-pixel tile at its projection (MapView_LinkPrimAt at
// its x, z, dy 0, 0x14), a draw mode (page Gpu_GetTPage(0, 2, 0x3C0, 0)) linked
// (0xC), and a 3 by 3 tile of 8 one up and left of the projection,
// semi-transparent (0x1C).
extern "C" void __cdecl EffectKind81_DrawDrop(unsigned char* drop) {
    if (drop[3] == 0) return;
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetTile1)(p);
    SH_CALL(EffectGte_ProjectPoint)(reinterpret_cast<const long*>(drop + 4), reinterpret_cast<float*>(p + 8));
    p[6] = 0;
    p[5] = 0;
    p[4] = 0;
    SH_CALL(MapView_LinkPrimAt)(UL(drop + 4), UL(drop + 8), 0, 0x14);
    const U page = SH_CALL(Gpu_GetTPage)(0, 2, 0x3C0, 0);
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, page & 0xFFFFu, 0);
    SH_CALL(MapView_LinkPrimAt)(UL(drop + 4), UL(drop + 8), 0, 0xC);
    unsigned char* const t = Gfx_PacketNext;
    SH_CALL(Gpu_SetTile)(t);
    SH_CALL(Gpu_SetSemiTrans)(t, 1);
    SH_CALL(EffectGte_ProjectPoint)(reinterpret_cast<const long*>(drop + 4), reinterpret_cast<float*>(t + 8));
    FldSub(t + 8, t + 8, At(at::kOne));
    SetBits(t + 0x14, 0x40400000u);   // 3.0f
    SetBits(t + 0x18, 0x40400000u);
    FldSub(t + 0xC, t + 0xC, At(at::kOne));
    t[6] = 8;
    t[5] = 8;
    t[4] = 8;
    SH_CALL(MapView_LinkPrimAt)(UL(drop + 4), UL(drop + 8), 0, 0x1C);
}

// original 0x487AB0 (cdecl): the sixteen sources at 0x92D780 (0x14 each): +0 1,
// +1 0, +2 (Rand & 0xF) - 0x10, +3 Rand & 0x1F (the first wait), +4 / +8 the
// cell 0x654C3C gives (s8 x, z << 16), +0xC the ground's height there
// (AreaMap_Elevation's low word << 16).
extern "C" void __cdecl EffectKind81_PlaceSources(void) {
    for (U i = 0; i < at::kSourceCount; ++i) {
        unsigned char* const src = P(at::kSources + at::kSourceStride * i);
        src[0] = 1;
        src[1] = 0;
        src[2] = static_cast<unsigned char>((static_cast<U>(SH_CALL(Rand)()) & 0xFu) - 0x10u);
        const U r = static_cast<U>(SH_CALL(Rand)());
        const U z = static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(B(at::kSourceCells + 2 * i + 1)))) << 16;
        src[3] = static_cast<unsigned char>(r & 0x1Fu);
        const U x = static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(B(at::kSourceCells + 2 * i)))) << 16;
        SetUL(src + 4, x);
        SetUL(src + 8, z);
        const long ground = SH_CALL(AreaMap_Elevation)(static_cast<long>(x), static_cast<long>(z));
        SetUL(src + 0xC, static_cast<U>(S16(static_cast<U>(ground))) << 16);
    }
}

// original 0x487B20 (cdecl): each source on (+0): its wait +3 down; when it was
// 0, two drops from free records (EffectKind81_FindFreeDrop; none, none) - +0 1, +2
// life 0x20 + (Rand & 0x1F), +4 / +8 the source's x / z less 0x10000 and plus
// Rand's low byte << 8, +0xC its height, +0x14 0, +3 Rand & 1 - and the wait
// Rand & 7. al 1 when any source was on, else 0.
extern "C" unsigned char __cdecl EffectKind81_Emit(void) {
    unsigned char any = 0;
    for (U i = 0; i < at::kSourceCount; ++i) {
        unsigned char* const src = P(at::kSources + at::kSourceStride * i);
        if (src[0] == 0) continue;
        const unsigned wait = src[3];
        any = 1;
        src[3] = static_cast<unsigned char>(wait - 1);
        if (wait != 0) continue;
        for (int n = 2; n != 0; --n) {
            unsigned char* const d = SH_CALL(EffectKind81_FindFreeDrop)();
            if (d == nullptr) continue;
            d[0] = 1;
            d[2] = static_cast<unsigned char>((static_cast<U>(SH_CALL(Rand)()) & 0x1Fu) + 0x20u);
            const U rx = static_cast<U>(SH_CALL(Rand)()) & 0xFFu;
            SetUL(d + 4, ((rx - 0x100u) << 8) + UL(src + 4));
            const U rz = static_cast<U>(SH_CALL(Rand)()) & 0xFFu;
            SetUL(d + 8, ((rz - 0x100u) << 8) + UL(src + 8));
            SetUL(d + 0xC, UL(src + 0xC));
            SetUL(d + 0x14, 0);
            d[3] = static_cast<unsigned char>(static_cast<U>(SH_CALL(Rand)()) & 1u);
        }
        src[3] = static_cast<unsigned char>(static_cast<U>(SH_CALL(Rand)()) & 7u);
    }
    return any;
}

// original 0x487BF0 (cdecl; also called by 0x492CF0, a catalog row): the first
// of the 256 drop records whose +0 is 0, else null (eax).
extern "C" unsigned char* __cdecl EffectKind81_FindFreeDrop(void) {
    for (U i = 0; i < at::kDropCount; ++i) {
        unsigned char* const d = P(at::kDrops + at::kDropStride * i);
        if (d[0] == 0) return d;
    }
    return nullptr;
}

// ===========================================================================
// Kind 0x82: EffectKind82_States (24; 11..23 are E4A's and two of ours)
// ===========================================================================

// original 0x487C10 (Effect_KindHandlers[0x82], hidden in 0x487BF0): jmp
// [EffectKind82_States + +1 * 4].
extern "C" void __cdecl EffectKind82_Run(void) {
    Dispatch("EffectKind82_Run", AddressOf(EffectKind82_States), EffectKind82_States_count, 1);
}

// original 0x487C30 (state 0, and entry 0 of the next table 0x654CE8): +9 0, +1
// 1, +2 0.
extern "C" void __cdecl EffectKind82_Start(void) {
    S()[9] = 0;
    S()[1] = 1;
    S()[2] = 0;
}

namespace {
// Field_State the leader; true when Sprite_Objects record 0's x is the
// leader's or past it (signed).
bool ObjectReached() {
    const auto object_x = static_cast<std::int32_t>(UL(at::kObject0X));
    const auto leader_x = static_cast<std::int32_t>(UL(at::kLeaderX));
    Field_State = ObjTrio;
    return object_x >= leader_x;
}
// Record 0 a cell on (x + 0x4000 four times, Sprite_Current pointed at it for
// the adds as the original does), +1 `next`.
void Push(unsigned next) {
    unsigned char* const saved = Sprite_Current;
    Sprite_Current = Sprite_Objects;
    for (int n = 4; n != 0; --n) SetUL(Sprite_Current + 0x34, UL(Sprite_Current + 0x34) + 0x4000u);
    saved[1] = static_cast<unsigned char>(next);
    Sprite_Current = saved;
}
// +1 0xD and +9 0 when record 0 has reached the leader; else +1 `next`.
void Check(unsigned next) {
    unsigned char* const s = S();
    if (ObjectReached()) {
        s[1] = 0xD;
        S()[9] = 0;
    } else {
        s[1] = static_cast<unsigned char>(next);
    }
}
}  // namespace

// original 0x487C50 (state 1): +1 0x16 when record 0 has reached the leader,
// 0x17 when 0x90384A is 0x80; then by 0x903849 - 3: +1 6, 4: +1 4, 5: +1 2
// (three, four or five pushes), 0xFF: a tail jump to Effect_Release (MSVC's switch
// through the byte table 0x487CD4 and the jump table 0x487CC0 in the code).
extern "C" void __cdecl EffectKind82_Wait(void) {
    unsigned char* s = S();
    if (ObjectReached()) {
        s[1] = 0x16;
        s = S();
    }
    if (B(at::kCounterC) == 0x80) {
        s[1] = 0x17;
        s = S();
    }
    switch (B(at::kCounterB)) {
    case 3: s[1] = 6; return;
    case 4: s[1] = 4; return;
    case 5: s[1] = 2; return;
    case 0xFF: SH_CALL(Effect_Release)(); return;
    default: return;
    }
}

// original 0x487DE0 (state 2, a start no list of the cut has): a push, +1 3.
extern "C" void __cdecl EffectKind82_Push2(void) { Push(3); }
// original 0x487E20 (state 3): reached: +1 0xD; else +1 4.
extern "C" void __cdecl EffectKind82_Check3(void) { Check(4); }
// original 0x487E60 (state 4): a push, +1 5.
extern "C" void __cdecl EffectKind82_Push4(void) { Push(5); }
// original 0x487EA0 (state 5): reached: +1 0xD; else +1 6.
extern "C" void __cdecl EffectKind82_Check5(void) { Check(6); }
// original 0x487EE0 (state 6): a push, +1 7.
extern "C" void __cdecl EffectKind82_Push6(void) { Push(7); }
// original 0x487F20 (state 7): reached: +1 0xD; else +1 8.
extern "C" void __cdecl EffectKind82_Check7(void) { Check(8); }
// original 0x487F60 (state 8): a push, +1 9.
extern "C" void __cdecl EffectKind82_Push8(void) { Push(9); }
// original 0x487FA0 (state 9): reached: +1 0xD; else +1 0xA.
extern "C" void __cdecl EffectKind82_Check9(void) { Check(0xA); }
// original 0x487FE0 (state 10): a push, +1 0xB (E4A's state 11 follows).
extern "C" void __cdecl EffectKind82_Push10(void) { Push(0xB); }

void Effect3D_Inject() {
    if (bof3::WantsShadow("effect_3d")) effect_3d::SelfTest();
    BOF3_INJECT(EffectKind76_Shade);
    BOF3_INJECT(EffectKind79_Shade);
    BOF3_INJECT(EffectKind7A_Shade);
    BOF3_INJECT(EffectKind7B_Run);
    BOF3_INJECT(EffectKind7B_Start);
    BOF3_INJECT(EffectKind7B_Pulse);
    BOF3_INJECT(EffectKind7C_Run);
    BOF3_INJECT(EffectKind7C_Start);
    BOF3_INJECT(EffectKind7C_Pulse);
    BOF3_INJECT(EffectKind77_Run);
    BOF3_INJECT(EffectKind77_Count);
    BOF3_INJECT(EffectKind77_Begin);
    BOF3_INJECT(EffectKind77_Tick);
    BOF3_INJECT(EffectKind77_Resume);
    BOF3_INJECT(EffectKind77_End);
    BOF3_INJECT(EffectKind77_Show);
    BOF3_INJECT(EffectKind77_DrawCount);
    BOF3_INJECT(EffectKind78_Run);
    BOF3_INJECT(EffectKind78_Start);
    BOF3_INJECT(EffectKind78_Grow);
    BOF3_INJECT(EffectKind78_Spin);
    BOF3_INJECT(EffectKind78_End);
    BOF3_INJECT(EffectKind78_DrawRing);
    BOF3_INJECT(EffectKind7D_Run);
    BOF3_INJECT(EffectKind7D_Start);
    BOF3_INJECT(EffectKind7D_Input);
    BOF3_INJECT(EffectKind7D_Apply);
    BOF3_INJECT(EffectKind7D_DrawPanel);
    BOF3_INJECT(EffectKind7D_DrawDial);
    BOF3_INJECT(EffectKind7D_FillF4);
    BOF3_INJECT(EffectKind7D_FillF3);
    BOF3_INJECT(EffectKind7D_Line);
    BOF3_INJECT(EffectKind7D_DrawMode);
    BOF3_INJECT(EffectKind7D_DrawMark);
    BOF3_INJECT(EffectKind7D_SetMap);
    BOF3_INJECT(EffectKind7F_Run);
    BOF3_INJECT(EffectKind7F_Start);
    BOF3_INJECT(EffectKind7F_Grow);
    BOF3_INJECT(EffectKind7F_DrawCone);
    BOF3_INJECT(EffectKind80_Run);
    BOF3_INJECT(EffectKind80_Start);
    BOF3_INJECT(EffectKind80_Rise);
    BOF3_INJECT(EffectKind80_Hold);
    BOF3_INJECT(EffectKind80_Fade);
    BOF3_INJECT(EffectKind80_TrailStep);
    BOF3_INJECT(EffectKind80_DrawTrail);
    BOF3_INJECT(EffectKind81_Run);
    BOF3_INJECT(EffectKind81_Start);
    BOF3_INJECT(EffectKind81_Pour);
    BOF3_INJECT(EffectKind81_Drain);
    BOF3_INJECT(EffectKind81_ClearDrops);
    BOF3_INJECT(EffectKind81_MoveDrops);
    BOF3_INJECT(EffectKind81_DrawDrop);
    BOF3_INJECT(EffectKind81_PlaceSources);
    BOF3_INJECT(EffectKind81_Emit);
    BOF3_INJECT(EffectKind81_FindFreeDrop);
    BOF3_INJECT(EffectKind82_Run);
    BOF3_INJECT(EffectKind82_Start);
    BOF3_INJECT(EffectKind82_Wait);
    BOF3_INJECT(EffectKind82_Push2);
    BOF3_INJECT(EffectKind82_Check3);
    BOF3_INJECT(EffectKind82_Push4);
    BOF3_INJECT(EffectKind82_Check5);
    BOF3_INJECT(EffectKind82_Push6);
    BOF3_INJECT(EffectKind82_Check7);
    BOF3_INJECT(EffectKind82_Push8);
    BOF3_INJECT(EffectKind82_Check9);
    BOF3_INJECT(EffectKind82_Push10);
}

// Effect kinds 0x48 (states 7..12) and 0x4A..0x4E - round thirteen, wave two,
// group E2E: the 51 functions of the cut (analysis/round13_cut.tsv) in
// 0x4789D0..0x47B7CF, each read with capstone to its last instruction
// (docs/effect_2e.md section 1). Effect_RunObjects (ours) makes each live
// record of Effect_Objects (20 of 0x80 bytes) Sprite_Current and calls
// Effect_KindHandlers[+5]; each dispatcher here jumps through its state table
// by a byte of the record (none bounded by a compare):
//
//   kind 0x48's states 7..12 (E2D's dispatcher 0x4781B0 by +1 through
//   0x654578; kind 0x49's 0x478550 reaches the same cells as its states 4..9)
//   are each a dispatcher by +2 through a table of their own - state 7's is
//   E2D's (0x4789B0), 8..12 ours:
//     7   a ring record at 0x92D1C8 swelling, widening and lifting;
//     8   a spiral record at 0x92C4A4 at the record's point, turning up;
//     9   sparks (the 8 records at 0x92BF80) round the leader;
//     10  the spiral at the leader, turning down;
//     11  a lit sphere at the leader (EffectSphere_Build / _Draw);
//     12  a burst record at 0x92C060 that travels to a point, holds, grows
//         and shrinks;
//   kind 0x4A  a trail of 64 points after Sprite_Objects[1], a glow at its head;
//   kind 0x4B  32 debris scattering at a fixed cell;
//   kind 0x4C  random spark lines at a fixed cell for 300 frames;
//   kind 0x4D  a textured column rising at its point;
//   kind 0x4E  a disc at the leader's screen point (E2F's 0x47B7D0 draws it).
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies. No divergence:
// each is a faithful replacement. Where the original jumps through a state
// table past its end or indexes the sphere past its 0x1E2 points, ours aborts
// with a message (docs/effect_2e.md section 2).
#include "game/effect_2e.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_2e_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_2e::at;
using U = std::uint32_t;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

unsigned char* S() { return Sprite_Current; }
U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }
void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }
std::int32_t SL(const unsigned char* p) { return Long(p); }
U SW(const unsigned char* p) { return static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(Word(p)))); }
void AddWord(unsigned char* p, U d) { SetWord(p, Word(p) + d); }
void AddLong(unsigned char* p, U d) { SetUL(p, UL(p) + d); }
U AddressOf(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
// sar: an arithmetic right shift of the 32 bits as the original holds them.
U Sar(U v, unsigned n) { return static_cast<U>(static_cast<std::int32_t>(v) >> n); }
// `fild dword i; fadd dword [f]; fstp dword [out]`, on the x87 as the original
// does it: the precision and rounding the control word holds round the sum
// (magic_s32.cpp's AddIntToFloat idiom).
void StoreSum(unsigned char* out, U whole, const unsigned char* f) {
    const auto i = static_cast<std::int32_t>(whole);
    __asm__ volatile("fildl %1\n\tfadds %2\n\tfstps %0"
                     : "=m"(*reinterpret_cast<float*>(out))
                     : "m"(i), "m"(*reinterpret_cast<const float*>(f))
                     : "st");
}
void Copy12(unsigned char* to, const void* from) { std::memcpy(to, from, 12); }
// The pointer a .data cell holds.
unsigned char* Cell(U cell) { return At(UL(At(cell))); }

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + byte]; jmp [table +
// eax * 4]: the table's `entries` handlers read in place (the fuzz swaps the
// cells for recorders); a Fatal past them, where the original jumps through
// the dword after - the next table or data.
void Dispatch(const char* who, U table, unsigned entries, unsigned byte) {
    const unsigned index = Sprite_Current[byte];
    if (index >= entries)
        bof3::Fatal("%s: byte +%u is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/effect_2e.md section 2)",
                    who, byte, index, entries, (unsigned)table);
    reinterpret_cast<scenario_harness::Handler>(static_cast<std::uintptr_t>(UL(At(table + 4 * index))))();
}

// +2 up, Sprite_Current read afresh.
void NextStep() {
    unsigned char* const s = S();
    s[2] = static_cast<unsigned char>(s[2] + 1);
}
void NextState() {
    unsigned char* const s = S();
    s[1] = static_cast<unsigned char>(s[1] + 1);
}
// The dword +0xC down one (Sprite_Current read afresh); true at 0 (the
// record as read then in *out).
bool CountDownC(unsigned char** out) {
    unsigned char* s = S();
    SetUL(s + 0xC, UL(s + 0xC) - 1u);
    s = S();
    *out = s;
    return UL(s + 0xC) == 0;
}
// The byte +9 down one; true at 0.
bool CountDown9(unsigned char** out) {
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    s = S();
    *out = s;
    return s[9] == 0;
}

// Sprite_Current's +0x3C = the ground under its +0x34 / +0x38 (AreaMap_Elevation's
// low word, sign-extended, plus `lift`) << 16, the record read again after the call.
void Ground(U lift) {
    const unsigned char* const s = S();
    const long g = SH_CALL(AreaMap_Elevation)(Long(s + 0x34), Long(s + 0x38));
    SetUL(S() + 0x3C, (SW(reinterpret_cast<const unsigned char*>(&g)) + lift) << 16);
}
// Sprite_Current's +0x34 / +0x38 = the leader's x / z (each store to the record
// as read then).
void AtLeader() {
    SetUL(S() + 0x34, UL(At(at::kLeaderX)));
    SetUL(S() + 0x38, UL(At(at::kLeaderZ)));
}

// States 8 and 10's start: 0x6761CC = the spiral record; its point = the
// record's +0x34, +0x38 and +0x3C + 0x2000000; its turn word `turn`; set up
// (0x4799C0); +0xC = 0x28, +2 up.
void SpiralStart(unsigned turn) {
    unsigned char* s = S();
    SetUL(At(at::kSpiralPtr), at::kSpiral);
    SetUL(At(at::kSpiral), UL(s + 0x34));
    SetUL(At(at::kSpiral + 4), UL(s + 0x38));
    const U height = UL(s + 0x3C) + 0x2000000u;
    SetWord(At(at::kSpiralTurn), turn);
    SetUL(At(at::kSpiral + 8), height);
    SH_AT(void (__cdecl*)(unsigned char*), at::kSpiralInit)(At(at::kSpiral));
    s = S();
    SetUL(s + 0xC, 0x28);
    NextStep();
}
// States 8 and 10's frame: the spiral (0x6761CC) stepped and drawn (0x479B70);
// while +0xC is above 8 (signed) its turn word +0xD10 up `turn`; +0xC down, at 0
// a tail jmp to Effect_Release.
void SpiralTurn(unsigned turn) {
    SH_AT(void (__cdecl*)(unsigned char*), at::kSpiralDraw)(Cell(at::kSpiralPtr));
    const unsigned char* s = S();
    if (SL(s + 0xC) > 8) AddWord(Cell(at::kSpiralPtr) + 0xD10, turn);
    unsigned char* t;
    if (!CountDownC(&t)) return;
    SH_CALL(Effect_Release)();
}

// State 12's frame: the burst record stepped (0x4794D0) and drawn (0x4796B0).
void BurstFrame() {
    SH_AT(void (__cdecl*)(unsigned char*), at::kBurstStep)(At(at::kBurst));
    SH_AT(void (__cdecl*)(unsigned char*), at::kBurstDraw)(At(at::kBurst));
}

// A draw mode: Gpu_GetTPage(0, 1, x, y) into Gpu_SetDrawMode(packet, 0, dtd,
// it, 0), committed 0xC.
void DrawMode(int x, int y, int dtd) {
    const unsigned tpage = SH_CALL(Gpu_GetTPage)(0, 1, x, y);
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, dtd, tpage & 0xFFFFu, 0);
    SH_CALL(Gfx_CommitPrim)(1, 0xC);
}

// libgte's MATRIX: nine s16 of rotation, two bytes of padding, a translation
// of three s32.
struct Matrix {
    short m[9];
    short pad;
    long t[3];
};
static_assert(sizeof(Matrix) == 0x20, "a MATRIX is 32 bytes");

}  // namespace

// ===========================================================================
// Kind 0x48's state 7: E2D's dispatcher 0x4789B0 by +2 through 0x6545FC (four)
// ===========================================================================

// original 0x4789D0 (step 0): the ground under the record (+0x3C); 0x6761D0 =
// the ring record 0x92D1C8, its point the record's +0x34 / +0x38 / +0x3C, its
// word +0x10 = 0; +9 = 0, +0xC = 0x5F, +2 up.
extern "C" void __cdecl EffectKind48_State7_Start(void) {
    Ground(0);
    unsigned char* const s = S();
    SetUL(At(at::kRingPtr), at::kRingRecord);
    SetUL(At(at::kRingRecord), UL(s + 0x34));
    SetUL(At(at::kRingRecord + 4), UL(s + 0x38));
    SetUL(At(at::kRingRecord + 8), UL(s + 0x3C));
    SetWord(At(at::kRingRecord + 0x10), 0);
    s[9] = 0;
    SetUL(S() + 0xC, 0x5F);
    NextStep();
}

// original 0x478A40 (step 1): unless the low byte of +0xC meets the mask
// 0x65460C[+9], the ring's word +0x10 up 0x654614[+9]; when +0xC's low four
// bits are 0, +9 up; the ring drawn (0x479EE0); +0xC down, at 0 +0xC = 0x10 and
// +2 up. The two byte tables are read in place by the whole byte +9 (the
// original's reach past their eight entries is the image's next bytes).
extern "C" void __cdecl EffectKind48_State7_Swell(void) {
    unsigned char* s = S();
    const unsigned index = s[9];
    if ((s[0xC] & At(at::kState7Steps)[index]) == 0) AddWord(Cell(at::kRingPtr) + 0x10, At(at::kState7Adds)[index]);
    if ((s[0xC] & 0xF) == 0) s[9] = static_cast<unsigned char>(s[9] + 1);
    SH_AT(void (__cdecl*)(unsigned char*), at::kRing)(Cell(at::kRingPtr));
    if (!CountDownC(&s)) return;
    SetUL(s + 0xC, 0x10);
    NextStep();
}

// original 0x478AB0 (step 2): the ring's word +0x10 up 4; the ring drawn; +0xC
// down, at 0 +2 up.
extern "C" void __cdecl EffectKind48_State7_Widen(void) {
    AddWord(Cell(at::kRingPtr) + 0x10, 4);
    SH_AT(void (__cdecl*)(unsigned char*), at::kRing)(Cell(at::kRingPtr));
    unsigned char* s;
    if (!CountDownC(&s)) return;
    s[2] = static_cast<unsigned char>(s[2] + 1);
}

// original 0x478AF0 (step 3): the ring's height +8 up 0x1000000; the ring
// drawn; once its height is 0x10000000 or more (signed), a tail jmp to
// Effect_Release.
extern "C" void __cdecl EffectKind48_State7_Lift(void) {
    AddLong(Cell(at::kRingPtr) + 8, 0x1000000u);
    SH_AT(void (__cdecl*)(unsigned char*), at::kRing)(Cell(at::kRingPtr));
    if (SL(Cell(at::kRingPtr) + 8) < 0x10000000) return;
    SH_CALL(Effect_Release)();
}

// ===========================================================================
// Kind 0x48's state 8: EffectKind48_State8_Steps (two), by +2
// ===========================================================================

// original 0x478B30: jmp [EffectKind48_State8_Steps + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind48_State8_Run(void) {
    Dispatch("EffectKind48_State8_Run", AddressOf(EffectKind48_State8_Steps), EffectKind48_State8_Steps_count, 2);
}

// original 0x478B50 (step 0): the ground under the record; the spiral at its
// point, turn word 0 (SpiralStart); Sound_PlayEffect(0x209) after +2 up.
extern "C" void __cdecl EffectKind48_State8_Start(void) {
    Ground(0);
    SpiralStart(0);
    SH_CALL(Sound_PlayEffect)(0x209);
}

// original 0x478BE0 (step 1): the spiral's frame, its turn up 0x40.
extern "C" void __cdecl EffectKind48_State8_Turn(void) { SpiralTurn(0x40); }

// ===========================================================================
// Kind 0x48's state 9: EffectKind48_State9_Steps (three), by +2
// ===========================================================================

// original 0x478C30: jmp [EffectKind48_State9_Steps + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind48_State9_Run(void) {
    Dispatch("EffectKind48_State9_Run", AddressOf(EffectKind48_State9_Steps), EffectKind48_State9_Steps_count, 2);
}

// original 0x478C50 (step 0): the record at the leader's x / z, the ground
// under it; the sparks cleared (0x4790C0); +0xC = 0x28, +6 = 1, +2 up.
extern "C" void __cdecl EffectKind48_State9_Start(void) {
    AtLeader();
    Ground(0);
    SH_AT(void (__cdecl*)(), at::kSparksInit)();
    SetUL(S() + 0xC, 0x28);
    S()[6] = 1;
    NextStep();
}

// original 0x478CC0 (step 1): the spiral stepped and drawn (0x479B70); while
// +0xC is above 8 (signed), a free spark looked for (0x47CF20, eax) and, when
// +0xC's low two bits are 0 and one was found, set (0x479160); the sparks run
// (0x479260); +0xC down, at 0 +2 up.
extern "C" void __cdecl EffectKind48_State9_Emit(void) {
    SH_AT(void (__cdecl*)(unsigned char*), at::kSpiralDraw)(Cell(at::kSpiralPtr));
    if (SL(S() + 0xC) > 8) {
        unsigned char* const spark = SH_AT(unsigned char* (__cdecl*)(), at::kSparkFree)();
        if ((S()[0xC] & 3) == 0 && spark != nullptr) SH_AT(void (__cdecl*)(unsigned char*), at::kSparkSet)(spark);
    }
    SH_AT(unsigned char (__cdecl*)(), at::kSparksRun)();
    unsigned char* s;
    if (!CountDownC(&s)) return;
    s[2] = static_cast<unsigned char>(s[2] + 1);
}

// original 0x478D20 (step 2): the sparks run; when none is left (al 0), a tail
// jmp to Effect_Release.
extern "C" void __cdecl EffectKind48_State9_Drain(void) {
    if (SH_AT(unsigned char (__cdecl*)(), at::kSparksRun)() != 0) return;
    SH_CALL(Effect_Release)();
}

// ===========================================================================
// Kind 0x48's state 10: EffectKind48_State10_Steps (two), by +2
// ===========================================================================

// original 0x478D30: jmp [EffectKind48_State10_Steps + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind48_State10_Run(void) {
    Dispatch("EffectKind48_State10_Run", AddressOf(EffectKind48_State10_Steps), EffectKind48_State10_Steps_count, 2);
}

// original 0x478D50 (step 0): the record at the leader's x / z, the ground under
// it; the spiral there, turn word 0x800 (no sound).
extern "C" void __cdecl EffectKind48_State10_Start(void) {
    AtLeader();
    Ground(0);
    SpiralStart(0x800);
}

// original 0x478DF0 (step 1): the spiral's frame, its turn down 0x40.
extern "C" void __cdecl EffectKind48_State10_Turn(void) { SpiralTurn(0xFFC0u); }

// ===========================================================================
// Kind 0x48's state 11: EffectKind48_State11_Steps (two), by +2
// ===========================================================================

// original 0x478E40: jmp [EffectKind48_State11_Steps + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind48_State11_Run(void) {
    Dispatch("EffectKind48_State11_Run", AddressOf(EffectKind48_State11_Steps), EffectKind48_State11_Steps_count, 2);
}

// original 0x478E60 (step 0): the record at the leader's x / z, the ground under
// it; the sphere built; +0xC = 0x78, +2 up.
extern "C" void __cdecl EffectKind48_State11_Start(void) {
    AtLeader();
    Ground(0);
    SH_CALL(EffectSphere_Build)();
    SetUL(S() + 0xC, 0x78);
    NextStep();
}

// original 0x478EC0 (step 1): the sphere drawn; +0xC down, at 0 a tail jmp to
// Effect_Release.
extern "C" void __cdecl EffectKind48_State11_Draw(void) {
    SH_CALL(EffectSphere_Draw)();
    unsigned char* s;
    if (!CountDownC(&s)) return;
    SH_CALL(Effect_Release)();
}

// ===========================================================================
// Kind 0x48's state 12: EffectKind48_State12_Steps (eight: five here, then
// WeretigerFx_Next twice and Effect_StateRelease), by +2
// ===========================================================================

// original 0x478EF0: jmp [EffectKind48_State12_Steps + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind48_State12_Run(void) {
    Dispatch("EffectKind48_State12_Run", AddressOf(EffectKind48_State12_Steps), EffectKind48_State12_Steps_count, 2);
}

// original 0x478F10 (step 0): the burst's size word 0x92C49E = 0xC0; the burst
// stepped 32 times (0x4794D0); when +6 is set Sound_PlayEffect(0x203); +2 up.
extern "C" void __cdecl EffectKind48_State12_Start(void) {
    SetWord(At(at::kBurstSize), 0xC0);
    for (int i = 0; i < 0x20; ++i) SH_AT(void (__cdecl*)(unsigned char*), at::kBurstStep)(At(at::kBurst));
    if (S()[6] != 0) SH_CALL(Sound_PlayEffect)(0x203);
    NextStep();
}

// original 0x478F60 (step 1): the burst's frame; the record's point +0x34 /
// +0x38 / +0x3C moved by +0xC / +0x10 / +0x14; once +0x34 is +0x18 and +0x38 is
// +0x1C, +9 = 0x2F and +2 up.
extern "C" void __cdecl EffectKind48_State12_Travel(void) {
    BurstFrame();
    unsigned char* const s = S();
    AddLong(s + 0x34, UL(s + 0xC));
    AddLong(s + 0x38, UL(s + 0x10));
    AddLong(s + 0x3C, UL(s + 0x14));
    if (UL(s + 0x34) != UL(s + 0x18) || UL(s + 0x38) != UL(s + 0x1C)) return;
    s[9] = 0x2F;
    s[2] = static_cast<unsigned char>(s[2] + 1);
}

// original 0x478FD0 (step 2): the burst's frame; +9 down; at 0, when +6 is 0 a
// tail jmp to Effect_Release, else Sound_PlayEffect(0x204), +9 = 0x20 and +2 up.
extern "C" void __cdecl EffectKind48_State12_Hold(void) {
    BurstFrame();
    unsigned char* s;
    if (!CountDown9(&s)) return;
    if (s[6] == 0) {
        SH_CALL(Effect_Release)();
        return;
    }
    SH_CALL(Sound_PlayEffect)(0x204);
    S()[9] = 0x20;
    NextStep();
}

// original 0x479030 (step 3): the burst's frame; its size word up 6; +9 down,
// at 0 +9 = 0x20 and +2 up.
extern "C" void __cdecl EffectKind48_State12_Grow(void) {
    BurstFrame();
    AddWord(At(at::kBurstSize), 6);
    unsigned char* s;
    if (!CountDown9(&s)) return;
    s[9] = 0x20;
    NextStep();
}

// original 0x479080 (step 4): the burst's frame; its size word down 0xC; +9
// down, at 0 +2 up (to WeretigerFx_Next).
extern "C" void __cdecl EffectKind48_State12_Shrink(void) {
    BurstFrame();
    AddWord(At(at::kBurstSize), 0xFFF4u);
    unsigned char* s;
    if (!CountDown9(&s)) return;
    s[2] = static_cast<unsigned char>(s[2] + 1);
}

// ===========================================================================
// Helpers: an angle mean, the sphere
// ===========================================================================

// original 0x479970 (0x44 bytes): a & 0xFFF and b & 0xFFF ordered (lo, hi);
// (lo + hi) >> 1, plus 0x800 when hi - lo is 0x800 or more. eax.
extern "C" long __cdecl EffectAngle_Mean(unsigned a, unsigned b) {
    U lo = a & 0xFFFu;
    U hi = b & 0xFFFu;
    if (lo > hi) {
        const U t = lo;
        lo = hi;
        hi = t;
    }
    const U mean = (lo + hi) >> 1;
    return static_cast<long>(hi - lo < 0x800u ? mean : mean + 0x800u);
}

// original 0x47A560 (0x211 bytes): the sphere's vertices (x, y, z of s16 and a
// pad, 0x1000 = 1) - the pole (0, 0, 0x1000), 15 rings at the polar angles p =
// 0x80..0x780 of 32 at t = 0..0xF80 ((sin p cos t) sar 12, (sin p sin t) sar
// 12, cos p; Math_Sin(p), Math_Cos(t), Math_Sin(p), Math_Sin(t), Math_Cos(p)),
// the pole (0, 0, -0x1000); its 0x200 quads (vertex numbers: the cap round the
// first pole, 14 bands, the cap round the last); the light (0, 0x1000, 0x1000)
// normalised in place (Gte_VectorNormal); each vertex's shade: its dot with
// the light (32-bit), when above 0 the square root (0x5A7A90) at most 0xFFF,
// sar 7, else 0. al 0.
extern "C" unsigned char __cdecl EffectSphere_Build(void) {
    unsigned char* v = At(at::kSphereVerts);
    SetWord(v, 0);
    SetWord(v + 2, 0);
    SetWord(v + 4, 0x1000);
    v += 8;
    U polar = 0;
    for (int ring = 0; ring < 15; ++ring) {
        polar += 0x80;
        const int p = static_cast<int>(polar & 0xFFFFu);
        U turn = 0;
        for (int k = 0; k < 0x20; ++k) {
            const int t = static_cast<int>(turn & 0xFFFFu);
            U a = static_cast<U>(SH_CALL(Math_Sin)(p));
            SetWord(v, Sar(a * static_cast<U>(SH_CALL(Math_Cos)(t)), 12));
            a = static_cast<U>(SH_CALL(Math_Sin)(p));
            SetWord(v + 2, Sar(a * static_cast<U>(SH_CALL(Math_Sin)(t)), 12));
            SetWord(v + 4, static_cast<U>(SH_CALL(Math_Cos)(p)));
            v += 8;
            turn += 0x80;
        }
    }
    SetWord(v + 4, 0xF000);
    SetWord(v, 0);
    SetWord(v + 2, 0);
    unsigned char* q = At(at::kSphereQuads);
    auto quad = [&q](U a, U b, U c, U d) {
        SetWord(q, a);
        SetWord(q + 2, b);
        SetWord(q + 4, c);
        SetWord(q + 6, d);
        q += 8;
    };
    for (U j = 1; j <= 0x20; ++j) quad(0, 0, j, (j < 0x20 ? j : 0) + 1);
    for (U base = 0x21; base < 0x1E1; base += 0x20)
        for (U k = 0; k < 0x20; ++k) {
            const U e = k + 1 < 0x20 ? k + 1 : 0;
            quad(base + k - 0x20, base + e - 0x20, base + k, base + e);
        }
    for (U k = 0; k < 0x20; ++k) {
        const U e = k + 1 < 0x20 ? k + 1 : 0;
        quad(0x1C1 + k, 0x1C1 + e, 0x1E1, 0x1E1);
    }
    unsigned char* const light = At(at::kSphereLight);
    SetUL(light, 0);
    SetUL(light + 4, 0x1000);
    SetUL(light + 8, 0x1000);
    SH_CALL(Gte_VectorNormal)(reinterpret_cast<const long*>(light), reinterpret_cast<long*>(light));
    const unsigned char* vert = At(at::kSphereVerts);
    unsigned char* shade = At(at::kSphereShades);
    for (unsigned i = 0; i < at::kSphereVertCount; ++i, vert += 8, ++shade) {
        const U dot = SW(vert + 4) * UL(light + 8) + SW(vert) * UL(light) + SW(vert + 2) * UL(light + 4);
        if (static_cast<std::int32_t>(dot) <= 0) {
            *shade = 0;
            continue;
        }
        U root = static_cast<U>(SH_AT(long (__cdecl*)(long), at::kSqrt)(static_cast<long>(dot)));
        if (static_cast<std::int32_t>(root) > 0xFFF) root = 0xFFF;
        *shade = static_cast<unsigned char>(Sar(root, 7));
    }
    return 0;
}

namespace {
// A sphere vertex number, checked against the 0x1E2 (the original reads past
// the projected points and the shades into the next arrays).
unsigned SphereVertex(U n) {
    if (n >= at::kSphereVertCount)
        bof3::Fatal("EffectSphere_Draw: a quad names vertex %u, past the sphere's %u - the original reads past its points "
                    "(docs/effect_2e.md section 2)",
                    (unsigned)n, at::kSphereVertCount);
    return n;
}
}  // namespace

// original 0x47A780 (0x1CE bytes): EffectGte_LoadMapCamera; each vertex (x, y,
// z) projected as the world point (x << 5 + Sprite_Current +0x34, y << 5 + +0x38,
// (z + 0x1000) << 13 + +0x3C; the record read afresh each vertex) into the
// screen table; a draw mode (Gpu_GetTPage(0, 1, 0x3C0, 0), committed 0xC);
// then per quad a Gouraud quad at the packet cursor (Gpu_SetPolyG4,
// Gpu_SetSemiTrans(1)) of its four vertices' screen points, each shaded its
// vertex's shade (r = g = b), committed 0x44. al 0.
extern "C" unsigned char __cdecl EffectSphere_Draw(void) {
    SH_CALL(EffectGte_LoadMapCamera)();
    const unsigned char* v = At(at::kSphereVerts);
    for (unsigned i = 0; i < at::kSphereVertCount; ++i, v += 8) {
        const unsigned char* const s = S();
        long point[3];
        point[0] = static_cast<long>((SW(v) << 5) + UL(s + 0x34));
        point[1] = static_cast<long>((SW(v + 2) << 5) + UL(s + 0x38));
        point[2] = static_cast<long>(((SW(v + 4) + 0x1000u) << 13) + UL(s + 0x3C));
        SH_CALL(EffectGte_ProjectPoint)(point, reinterpret_cast<float*>(At(at::kSphereScreen + 0xC * i)));
    }
    DrawMode(0x3C0, 0, 1);
    const unsigned char* q = At(at::kSphereQuads);
    const unsigned char* const shades = At(at::kSphereShades);
    for (unsigned n = 0; n < at::kSphereQuadCount; ++n, q += 8) {
        unsigned char* const prim = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG4)(prim);
        SH_CALL(Gpu_SetSemiTrans)(prim, 1);
        unsigned vertex[4];
        for (unsigned c = 0; c < 4; ++c) vertex[c] = SphereVertex(Word(q + 2 * c));
        for (unsigned c = 0; c < 4; ++c) Copy12(prim + 8 + 0x10 * c, At(at::kSphereScreen + 0xC * vertex[c]));
        for (unsigned c = 0; c < 4; ++c) prim[4 + 0x10 * c] = prim[5 + 0x10 * c] = prim[6 + 0x10 * c] = shades[vertex[c]];
        SH_CALL(Gfx_CommitPrim)(1, 0x44);
    }
    return 0;
}

// ===========================================================================
// Kind 0x4A: Effect_KindHandlers[0x4A] (0x655478), EffectKind4A_States (two)
// ===========================================================================

// original 0x47A950: jmp [EffectKind4A_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind4A_Run(void) {
    Dispatch("EffectKind4A_Run", AddressOf(EffectKind4A_States), EffectKind4A_States_count, 1);
}

// original 0x47A970 (state 0): the record's point = Sprite_Objects[1]'s
// (+0x34 / +0x38 / +0x3C); +0xC = 0xB4; the trail set (EffectKind4A_TrailInit);
// +1 up.
extern "C" void __cdecl EffectKind4A_Start(void) {
    SetUL(S() + 0x34, UL(At(at::kSprite1Point)));
    SetUL(S() + 0x38, UL(At(at::kSprite1Point + 4)));
    SetUL(S() + 0x3C, UL(At(at::kSprite1Point + 8)));
    SetUL(S() + 0xC, 0xB4);
    SH_CALL(EffectKind4A_TrailInit)();
    NextState();
}

// original 0x47A9C0 (state 1): Gte_PushMatrix; at +0xC = 0xA5 the trail's spin
// byte 0x92CC82 = 1; while +0xC is above 0x5A (signed) the record's point =
// Sprite_Objects[1]'s; the trail stepped and drawn, the glow at its head
// (EffectKind4A_DrawGlow(0x92BF80, 0x80, 0x80, 0)); Gte_PopMatrix; +0xC down, at
// 0 a tail jmp to Effect_Release.
extern "C" void __cdecl EffectKind4A_Follow(void) {
    SH_CALL(Gte_PushMatrix)();
    unsigned char* const s = S();
    if (UL(s + 0xC) == 0xA5) At(at::kTrailSpin)[0] = 1;
    if (SL(s + 0xC) > 0x5A) {
        SetUL(s + 0x34, UL(At(at::kSprite1Point)));
        SetUL(S() + 0x38, UL(At(at::kSprite1Point + 4)));
        SetUL(S() + 0x3C, UL(At(at::kSprite1Point + 8)));
    }
    SH_CALL(EffectKind4A_TrailStep)();
    SH_CALL(EffectKind4A_TrailDraw)();
    SH_CALL(EffectKind4A_DrawGlow)(reinterpret_cast<const long*>(At(at::kTrail)), 0x80, 0x80, 0);
    SH_CALL(Gte_PopMatrix)();
    unsigned char* t;
    if (!CountDownC(&t)) return;
    SH_CALL(Effect_Release)();
}

// original 0x47AC80 (0x3F bytes): the trail's turn word 0x92CC80 and spin byte
// 0x92CC82 = 0; each of its 64 points at Sprite_Current's point (read once),
// its angle word +0x30 = 0.
extern "C" void __cdecl EffectKind4A_TrailInit(void) {
    const unsigned char* const s = S();
    SetWord(At(at::kTrailTurn), 0);
    At(at::kTrailSpin)[0] = 0;
    for (unsigned i = 0; i < at::kTrailCount; ++i) {
        unsigned char* const p = At(at::kTrail + at::kTrailStride * i);
        SetUL(p, UL(s + 0x34));
        SetUL(p + 4, UL(s + 0x38));
        SetUL(p + 8, UL(s + 0x3C));
        SetWord(p + 0x30, 0);
    }
}

// original 0x47ACC0 (0xF9 bytes): the trail's points moved one on (62 down to 0
// copied over the next, 0x34 bytes each); the newest at Sprite_Current's +0x34
// / +0x38 and +0x3C + Math_Sin(~(Frame_Counter << 7) & 0xFFFF) << 10, its angle
// word the turn word 0x92CC80 - which, while the spin byte is set, turns 0x40
// (the byte cleared when it comes round to 0). Then each point's two edge
// points: c = Math_Cos(its angle) << 3, s = Math_Sin(its angle) << 3; +0x10 /
// +0x14 / +0x18 = x, z + c, height + (s << 8); +0x20 / +0x24 / +0x28 = x, z -
// c, height - (s << 8).
extern "C" void __cdecl EffectKind4A_TrailStep(void) {
    for (int i = static_cast<int>(at::kTrailCount) - 2; i >= 0; --i)
        std::memcpy(At(at::kTrail + at::kTrailStride * (i + 1)), At(at::kTrail + at::kTrailStride * i), at::kTrailStride);
    const U frame = Frame_Counter;
    const unsigned char* s = S();
    unsigned char* const head = At(at::kTrail);
    SetUL(head, UL(s + 0x34));
    SetUL(head + 4, UL(s + 0x38));
    const U bob = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(~(frame << 7) & 0xFFFFu)));
    s = S();
    const unsigned char spin = At(at::kTrailSpin)[0];
    SetUL(head + 8, (bob << 10) + UL(s + 0x3C));
    SetWord(head + 0x30, Word(At(at::kTrailTurn)));
    if (spin != 0) {
        AddWord(At(at::kTrailTurn), 0x40);
        if (Word(At(at::kTrailTurn)) == 0) At(at::kTrailSpin)[0] = 0;
    }
    for (unsigned i = 0; i < at::kTrailCount; ++i) {
        unsigned char* const p = At(at::kTrail + at::kTrailStride * i);
        const U c = static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(Word(p + 0x30)))) << 3;
        const U n = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(Word(p + 0x30)))) << 3;
        const U x = UL(p);
        const U z = UL(p + 4);
        SetUL(p + 0x14, z + c);
        const U height = UL(p + 8);
        SetUL(p + 0x10, x);
        SetUL(p + 0x18, (n << 8) + height);
        SetUL(p + 0x20, x);
        SetUL(p + 0x24, z - c);
        SetUL(p + 0x28, height - (n << 8));
    }
}

// original 0x47ADC0 (0x144 bytes): a draw mode (Gpu_GetTPage(0, 1, 0x3C0, 0),
// committed 0xC), EffectGte_LoadMapCamera; the newest point's edge points
// projected (A, B); then for points 1..63 a Gouraud quad at the packet cursor
// (Gpu_SetPolyG4, Gpu_SetSemiTrans(1)): A and B as they stand, then the point's
// own edge points projected into A and B; the red shade 0x40 falling by one a
// quad, green and blue half the red (the first pair's green and blue 0x20);
// committed 0x44.
extern "C" void __cdecl EffectKind4A_TrailDraw(void) {
    DrawMode(0x3C0, 0, 1);
    SH_CALL(EffectGte_LoadMapCamera)();
    float a[3];
    float b[3];
    SH_CALL(EffectGte_ProjectPoint)(reinterpret_cast<const long*>(At(at::kTrail + 0x10)), a);
    SH_CALL(EffectGte_ProjectPoint)(reinterpret_cast<const long*>(At(at::kTrail + 0x20)), b);
    unsigned char red = 0x40;
    unsigned char half = 0x20;
    U edge = at::kTrail + 0x20;
    for (unsigned n = 0; n < at::kTrailCount - 1; ++n) {
        unsigned char* const prim = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG4)(prim);
        SH_CALL(Gpu_SetSemiTrans)(prim, 1);
        edge += at::kTrailStride;
        Copy12(prim + 8, a);
        Copy12(prim + 0x18, b);
        prim[0x14] = prim[4] = red;
        prim[0x16] = prim[0x15] = prim[6] = prim[5] = half;
        SH_CALL(EffectGte_ProjectPoint)(reinterpret_cast<const long*>(At(edge - 0x10)), a);
        SH_CALL(EffectGte_ProjectPoint)(reinterpret_cast<const long*>(At(edge)), b);
        Copy12(prim + 0x28, a);
        Copy12(prim + 0x38, b);
        red = static_cast<unsigned char>(red - 1);
        prim[0x34] = prim[0x24] = red;
        half = static_cast<unsigned char>(red >> 1);
        prim[0x36] = prim[0x35] = prim[0x26] = prim[0x25] = half;
        SH_CALL(Gfx_CommitPrim)(1, 0x44);
    }
}

// original 0x47AF10 (0x15F bytes): `point` projected (L: x, y, depth); its
// radius EffectGte_ProjectSize(point, {size's low word, the high word of
// point's address}, out) - the original hands the callee its own argument slot
// with `size` stored over point's low word - then r = out[0] + (Frame_Counter &
// 1), as an s16. For a = 0, 0x80, .. 0xF80 (b = (a + 0x80) & 0xFFF) a
// Gouraud triangle at the packet cursor (Gpu_SetPolyG3, Gpu_SetSemiTrans(1)):
// the centre L, the rim points L + ((cos a) * r sar 12, (sin a) * r sar 12)
// and likewise at b (fild + fadd: each an s32 plus L's float), every depth L's;
// the centre shaded `centre`, the rim `rim` (bytes); committed 0x34.
extern "C" void __cdecl EffectKind4A_DrawGlow(const long* point, unsigned size, unsigned centre, unsigned rim) {
    float l[3];
    SH_CALL(EffectGte_ProjectPoint)(point, l);
    const short slot[2] = {static_cast<short>(size & 0xFFFFu), static_cast<short>(AddressOf(point) >> 16)};
    short out[2];
    SH_CALL(EffectGte_ProjectSize)(point, slot, out);
    const U r = static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(static_cast<std::uint16_t>(out[0]) + (Frame_Counter & 1u))));
    const auto* const lb = reinterpret_cast<const unsigned char*>(l);
    U a = 0;
    U next = 0;
    do {
        unsigned char* const prim = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG3)(prim);
        SH_CALL(Gpu_SetSemiTrans)(prim, 1);
        std::memcpy(prim + 8, lb, 4);
        next += 0x80;
        std::memcpy(prim + 0xC, lb + 4, 4);
        const U b = next & 0xFFFu;
        StoreSum(prim + 0x18, Sar(static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(a))) * r, 12), lb);
        StoreSum(prim + 0x1C, Sar(static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(a))) * r, 12), lb + 4);
        StoreSum(prim + 0x28, Sar(static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(b))) * r, 12), lb);
        StoreSum(prim + 0x2C, Sar(static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(b))) * r, 12), lb + 4);
        std::memcpy(prim + 0x30, lb + 8, 4);
        std::memcpy(prim + 0x20, lb + 8, 4);
        std::memcpy(prim + 0x10, lb + 8, 4);
        prim[4] = prim[5] = prim[6] = static_cast<unsigned char>(centre);
        prim[0x14] = prim[0x15] = prim[0x16] = static_cast<unsigned char>(rim);
        prim[0x24] = prim[0x25] = prim[0x26] = static_cast<unsigned char>(rim);
        SH_CALL(Gfx_CommitPrim)(1, 0x34);
        a += 0x80;
    } while ((next & 0xFFFFu) < 0x1000u);
}

// ===========================================================================
// Kind 0x4B: Effect_KindHandlers[0x4B] (0x65547C), EffectKind4B_States (two)
// ===========================================================================

// original 0x47AA50: jmp [EffectKind4B_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind4B_Run(void) {
    Dispatch("EffectKind4B_Run", AddressOf(EffectKind4B_States), EffectKind4B_States_count, 1);
}

// original 0x47AA70 (state 0): the record at the cell x 0x738000, z 0xB8000,
// its height the ground there + 0x80 (<< 16); Gte_PushMatrix, the 32 debris
// set (EffectKind4B_DebrisInit), Gte_PopMatrix; +0xC = 0, +1 up.
extern "C" void __cdecl EffectKind4B_Start(void) {
    SetUL(S() + 0x34, 0x738000);
    SetUL(S() + 0x38, 0xB8000);
    Ground(0x80);
    SH_CALL(Gte_PushMatrix)();
    for (unsigned i = 0; i < at::kDebrisCount; ++i) SH_CALL(EffectKind4B_DebrisInit)(At(at::kDebris + at::kDebrisStride * i));
    SH_CALL(Gte_PopMatrix)();
    SetUL(S() + 0xC, 0);
    NextState();
}

// original 0x47AAF0 (state 1): a draw mode (Gpu_GetTPage(0, 1, 0x380, 0x100),
// Gpu_SetDrawMode(packet, 0, 0, it, 0), committed 0xC); Gte_PushMatrix,
// EffectGte_LoadMapCamera; each debris drawn (E3C's 0x485030), its angle +0x24
// up 0x10 and its shade +0x2A up 0x20 while the record's +9 is below 4, else
// down 2; Gte_PopMatrix; +0xC up, above 0xB8 (signed) a tail jmp to
// Effect_Release.
extern "C" void __cdecl EffectKind4B_Scatter(void) {
    DrawMode(0x380, 0x100, 0);
    SH_CALL(Gte_PushMatrix)();
    SH_CALL(EffectGte_LoadMapCamera)();
    for (unsigned i = 0; i < at::kDebrisCount; ++i) {
        unsigned char* const d = At(at::kDebris + at::kDebrisStride * i);
        SH_AT(void (__cdecl*)(unsigned char*), at::kDebrisDraw)(d);
        const unsigned char* const s = S();
        AddWord(d + 0x24, 0x10);
        AddWord(d + 0x2A, s[9] < 4 ? 0x20u : 0xFFFEu);
    }
    SH_CALL(Gte_PopMatrix)();
    unsigned char* s = S();
    AddLong(s + 0xC, 1);
    s = S();
    if (SL(s + 0xC) <= 0xB8) return;
    SH_CALL(Effect_Release)();
}

// original 0x47B070 (0x10A bytes): a debris record: its point +0 / +4 / +8
// Sprite_Current's +0x34 / +0x38 / +0x3C (read for each); three angles Rand &
// 0xFFF, Rand & 0x3FF, Rand & 0xFFF; two edges (Math_Cos, Math_Sin of 0x20; of
// -0x20; z 0) at +0x10 and +0x18 turned in place by a matrix of the three
// angles (EffectGte_SetDiagonalOne, Gte_RotMatrixX, _Y by the second negated,
// _Z, then 0x5A7C70 twice - no push of the GTE matrix here: the caller pushes
// once round all 32); its scale +0x28 1 + (Rand & 0xF); +0x20, +0x22, +0x24,
// +0x2A 0. EffectKind1E_DebrisInitOne (E1C) is the same with 0x10, 10 + Rand
// % 4 and its own push.
extern "C" void __cdecl EffectKind4B_DebrisInit(unsigned char* debris) {
    SetUL(debris + 0, UL(S() + 0x34));
    SetUL(debris + 4, UL(S() + 0x38));
    SetUL(debris + 8, UL(S() + 0x3C));
    const U ax = static_cast<U>(SH_CALL(Rand)()) & 0xFFFu;
    const U ay = static_cast<U>(SH_CALL(Rand)()) & 0x3FFu;
    const U az = static_cast<U>(SH_CALL(Rand)()) & 0xFFFu;
    SetWord(debris + 0x10, static_cast<U>(SH_CALL(Math_Cos)(0x20)));
    SetWord(debris + 0x12, static_cast<U>(SH_CALL(Math_Sin)(0x20)));
    SetWord(debris + 0x14, 0);
    SetWord(debris + 0x18, static_cast<U>(SH_CALL(Math_Cos)(-0x20)));
    SetWord(debris + 0x1A, static_cast<U>(SH_CALL(Math_Sin)(-0x20)));
    SetWord(debris + 0x1C, 0);
    Matrix m;
    SH_CALL(EffectGte_SetDiagonalOne)(m.m);
    SH_CALL(Gte_RotMatrixX)(static_cast<short>(ax), m.m);
    SH_CALL(Gte_RotMatrixY)(-static_cast<int>(static_cast<short>(ay)), m.m);
    SH_CALL(Gte_RotMatrixZ)(static_cast<short>(az), m.m);
    using Turn = void (__cdecl*)(const short*, const short*, short*);
    SH_AT(Turn, at::kMatrixVector)(m.m, reinterpret_cast<const short*>(debris + 0x10), reinterpret_cast<short*>(debris + 0x10));
    SH_AT(Turn, at::kMatrixVector)(m.m, reinterpret_cast<const short*>(debris + 0x18), reinterpret_cast<short*>(debris + 0x18));
    SetWord(debris + 0x28, (static_cast<U>(SH_CALL(Rand)()) & 0xFu) + 1u);
    SetWord(debris + 0x20, 0);
    SetWord(debris + 0x22, 0);
    SetWord(debris + 0x24, 0);
    SetWord(debris + 0x2A, 0);
}

// ===========================================================================
// Kind 0x4C: Effect_KindHandlers[0x4C] (0x655480), EffectKind4C_States (two)
// ===========================================================================

// original 0x47ABA0: jmp [EffectKind4C_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind4C_Run(void) {
    Dispatch("EffectKind4C_Run", AddressOf(EffectKind4C_States), EffectKind4C_States_count, 1);
}

// original 0x47ABC0 (state 0): the record at the cell x 0x740000, z 0x60000,
// its height the ground there (<< 16); +0xC = 300 frames, +1 up.
extern "C" void __cdecl EffectKind4C_Start(void) {
    SetUL(S() + 0x34, 0x740000);
    SetUL(S() + 0x38, 0x60000);
    Ground(0);
    SetUL(S() + 0xC, 0x12C);
    NextState();
}

// original 0x47AC20 (state 1): at +0xC % 30 = 0 Sound_PlayEffect(0x20B); while
// +0xC % 30 is below 15 (signed idiv) a spark line (EffectKind4C_DrawSpark); +0xC
// down, at 0 a tail jmp to Effect_Release.
extern "C" void __cdecl EffectKind4C_Crackle(void) {
    unsigned char* s = S();
    if (SL(s + 0xC) % 30 == 0) {
        SH_CALL(Sound_PlayEffect)(0x20B);
        s = S();
    }
    if (SL(s + 0xC) % 30 < 15) {
        SH_CALL(EffectKind4C_DrawSpark)();
        s = S();
    }
    SetUL(s + 0xC, UL(s + 0xC) - 1u);
    if (UL(S() + 0xC) != 0) return;
    SH_CALL(Effect_Release)();
}

// original 0x47B180 (0x121 bytes): two points round the record's (x and z
// each moved by ((Rand & 0xFF) - 0x80) << 8: the first's x, z, the second's x,
// z); Gte_PushMatrix, EffectGte_LoadMapCamera; a line at the packet cursor
// (Gpu_SetLineF2, Gpu_SetSemiTrans(0)) between the two projected, grey 0xC0,
// committed 0x20; Gte_PopMatrix.
extern "C" void __cdecl EffectKind4C_DrawSpark(void) {
    const unsigned char* const s = S();
    long from[3] = {Long(s + 0x34), Long(s + 0x38), Long(s + 0x3C)};
    long to[3] = {from[0], from[1], from[2]};
    auto jitter = [](long v) { return static_cast<long>(static_cast<U>(v) + (((static_cast<U>(SH_CALL(Rand)()) & 0xFFu) - 0x80u) << 8)); };
    from[0] = jitter(from[0]);
    from[1] = jitter(from[1]);
    to[0] = jitter(to[0]);
    to[1] = jitter(to[1]);
    SH_CALL(Gte_PushMatrix)();
    SH_CALL(EffectGte_LoadMapCamera)();
    unsigned char* const prim = Gfx_PacketNext;
    SH_CALL(Gpu_SetLineF2)(prim);
    SH_CALL(Gpu_SetSemiTrans)(prim, 0);
    float out[3];
    SH_CALL(EffectGte_ProjectPoint)(from, out);
    Copy12(prim + 8, out);
    SH_CALL(EffectGte_ProjectPoint)(to, out);
    Copy12(prim + 0x14, out);
    prim[6] = prim[5] = prim[4] = 0xC0;
    SH_CALL(Gfx_CommitPrim)(1, 0x20);
    SH_CALL(Gte_PopMatrix)();
}

// ===========================================================================
// Kind 0x4D: Effect_KindHandlers[0x4D] (0x655484), EffectKind4D_States (three)
// ===========================================================================

// original 0x47B2B0: jmp [EffectKind4D_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind4D_Run(void) {
    Dispatch("EffectKind4D_Run", AddressOf(EffectKind4D_States), EffectKind4D_States_count, 1);
}

// original 0x47B2D0 (state 0): the column's base +0xC / +0x10 / +0x14 = the
// record's point, its top +0x1C = the base height; its turn word +0x20 = 0, its
// fade +0x24 = 0x1000000; +9 = 0x3C, +1 up; Sound_PlayEffect(0x208, or 0x209
// when the byte 0x6761D8 is set).
extern "C" void __cdecl EffectKind4D_Start(void) {
    unsigned char* const s = S();
    SetUL(s + 0xC, UL(s + 0x34));
    SetUL(s + 0x10, UL(s + 0x38));
    SetUL(s + 0x14, UL(s + 0x3C));
    SetUL(s + 0x1C, UL(s + 0x14));
    SetWord(s + 0x20, 0);
    SetUL(s + 0x24, 0x1000000);
    s[9] = 0x3C;
    s[1] = static_cast<unsigned char>(s[1] + 1);
    SH_CALL(Sound_PlayEffect)(static_cast<unsigned short>(0x208 + (At(at::kSoundSwitch)[0] != 0 ? 1 : 0)));
}

// original 0x47B350 (state 1): the top +0x1C up 0x1000000, at most the base +
// 0x8000000 (signed); the turn word down 0x200; the column drawn; +9 down, at 0
// +9 = 0x10 and +1 up.
extern "C" void __cdecl EffectKind4D_Rise(void) {
    unsigned char* const s = S();
    AddLong(s + 0x1C, 0x1000000u);
    const U most = UL(s + 0x14) + 0x8000000u;
    if (SL(s + 0x1C) > static_cast<std::int32_t>(most)) SetUL(s + 0x1C, most);
    AddWord(s + 0x20, 0xFE00u);
    SH_CALL(EffectKind4D_DrawColumn)();
    unsigned char* t;
    if (!CountDown9(&t)) return;
    t[9] = 0x10;
    NextState();
}

// original 0x47B3B0 (state 2): +9 down; at 0 a tail jmp to Effect_Release; else
// the turn word down 0x200, the fade +0x24 down 0x100000, and a tail jmp to
// EffectKind4D_DrawColumn.
extern "C" void __cdecl EffectKind4D_Fade(void) {
    unsigned char* s;
    if (CountDown9(&s)) {
        SH_CALL(Effect_Release)();
        return;
    }
    AddWord(s + 0x20, 0xFE00u);
    AddLong(S() + 0x24, 0xFFF00000u);
    SH_CALL(EffectKind4D_DrawColumn)();
}

// original 0x47B3F0 (0x238 bytes): EffectGte_LoadMapCamera; with the turn t =
// +0x20, the base (x0, z0, y = +0x14) and the ring point (x0 + cos t << 5, z0 +
// sin t << 5): the lower point at height max(y - +0x24, +0x14) and the upper at
// min(y, +0x1C) projected (O1, O2; the record read afresh after each call);
// then while y is below +0x1C + 0x1000000 (signed): a textured quad at the
// packet cursor (Gpu_SetPolyFT4, Gpu_SetSemiTrans(0)) from O1 / O2 as they
// stand, t up 0x100 and y up 0x100000, O1 / O2 of the new ring point, its uv
// (0x7F, 0xBF), (0x7F, 0xA0), (0x60, 0xBF), (0x60, 0xA0), colour 0x80, tpage
// Gpu_GetTPage(1, 0, 0x140, 0x100), clut Gpu_GetClut(0, 0x1E7), committed
// 0x48.
extern "C" void __cdecl EffectKind4D_DrawColumn(void) {
    SH_CALL(EffectGte_LoadMapCamera)();
    const unsigned char* s = S();
    U turn = Word(s + 0x20);
    const U x0 = UL(s + 0xC);
    const U z0 = UL(s + 0x10);
    U y = UL(s + 0x14);
    long ring[3];
    float lower[3];
    float upper[3];
    // the ring point at `turn` and y: the lower and upper projections
    auto project = [&](int angle) {
        ring[0] = static_cast<long>((static_cast<U>(SH_CALL(Math_Cos)(angle)) << 5) + x0);
        ring[1] = static_cast<long>((static_cast<U>(SH_CALL(Math_Sin)(angle)) << 5) + z0);
        const unsigned char* r = S();
        ring[2] = static_cast<long>(y - UL(r + 0x24));
        if (ring[2] < SL(r + 0x14)) ring[2] = SL(r + 0x14);
        SH_CALL(EffectGte_ProjectPoint)(ring, lower);
        r = S();
        ring[2] = static_cast<long>(y);
        if (static_cast<std::int32_t>(y) > SL(r + 0x1C)) ring[2] = SL(r + 0x1C);
        SH_CALL(EffectGte_ProjectPoint)(ring, upper);
    };
    project(static_cast<int>(turn & 0xFFFFu));
    s = S();
    if (!(static_cast<std::int32_t>(y) < static_cast<std::int32_t>(UL(s + 0x1C) + 0x1000000u))) return;
    do {
        unsigned char* const prim = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyFT4)(prim);
        SH_CALL(Gpu_SetSemiTrans)(prim, 0);
        Copy12(prim + 8, lower);
        turn += 0x100;
        Copy12(prim + 0x18, upper);
        y += 0x100000;
        project(static_cast<int>(turn & 0xFFFFu));
        Copy12(prim + 0x28, lower);
        Copy12(prim + 0x38, upper);
        prim[0x14] = 0x7F;
        prim[0x15] = 0xBF;
        prim[0x24] = 0x7F;
        prim[0x25] = 0xA0;
        prim[0x34] = 0x60;
        prim[0x35] = 0xBF;
        prim[0x44] = 0x60;
        prim[0x45] = 0xA0;
        prim[4] = prim[5] = prim[6] = 0x80;
        SetWord(prim + 0x26, SH_CALL(Gpu_GetTPage)(1, 0, 0x140, 0x100));
        SetWord(prim + 0x16, SH_CALL(Gpu_GetClut)(0, 0x1E7));
        SH_CALL(Gfx_CommitPrim)(1, 0x48);
        s = S();
    } while (static_cast<std::int32_t>(y) < static_cast<std::int32_t>(UL(s + 0x1C) + 0x1000000u));
}

// ===========================================================================
// Kind 0x4E: Effect_KindHandlers[0x4E] (0x655488), EffectKind4E_States (five,
// the last Effect_StateRelease)
// ===========================================================================

// original 0x47B630: jmp [EffectKind4E_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind4E_Run(void) {
    Dispatch("EffectKind4E_Run", AddressOf(EffectKind4E_States), EffectKind4E_States_count, 1);
}

// original 0x47B650 (state 0): the leader's point as the camera vector ((x sar
// 9) - 0x4000, (z sar 9) - 0x4000, -(height / 2)) in Prim_VertexScratch,
// Gte_RotTransPers into the record's +0x74 (the screen x, y floats; the
// record read before the call), its answer to +0x60 and Gte_StoreDepthF to
// +0x7C (the record read after); +9 = 0, +1 up. The original hands a fourth
// argument, a stack slot the callee never reads.
extern "C" void __cdecl EffectKind4E_Start(void) {
    const U x = UL(At(at::kLeaderX));
    const U z = UL(At(at::kLeaderZ));
    Prim_VertexScratch[0] = static_cast<short>(Sar(x, 9) - 0x4000u);
    const std::int32_t height = static_cast<std::int16_t>(Word(At(at::kLeaderHeight)));
    Prim_VertexScratch[1] = static_cast<short>(Sar(z, 9) - 0x4000u);
    Prim_VertexScratch[2] = static_cast<short>(-(height / 2));
    long depth = 0;
    const long answer = SH_CALL(Gte_RotTransPers)(Prim_VertexScratch, reinterpret_cast<unsigned long*>(S() + 0x74), &depth);
    SetUL(S() + 0x60, static_cast<U>(answer));
    SH_CALL(Gte_StoreDepthF)(reinterpret_cast<float*>(S() + 0x7C));
    S()[9] = 0;
    NextState();
}

namespace {
// E2F's disc (0x47B7D0) at the record's screen point: radius (read as an s16;
// the callers push a whole register), centre 0x80, rim 0.
void Disc(unsigned radius) { SH_AT(void (__cdecl*)(unsigned, unsigned, unsigned), at::kDiscDraw)(radius, 0x80, 0); }
}  // namespace

// original 0x47B6E0 (state 1): the disc, radius +9; +9 up 0xF; above 0x3C
// (unsigned), +9 = 0xFF and +1 up.
extern "C" void __cdecl EffectKind4E_Grow(void) {
    Disc(S()[9]);
    unsigned char* t = S();
    t[9] = static_cast<unsigned char>(t[9] + 0xF);
    t = S();
    if (t[9] <= 0x3C) return;
    t[9] = 0xFF;
    NextState();
}

// original 0x47B720 (state 2): the disc, radius 0x3C + (+9 & 1); at +9 = 0xD7
// Sound_PlayEffect(0x203); +9 down, at 0 +9 = 0x3C and +1 up.
extern "C" void __cdecl EffectKind4E_Glow(void) {
    Disc(0x3Cu + (S()[9] & 1u));
    if (S()[9] == 0xD7) SH_CALL(Sound_PlayEffect)(0x203);
    unsigned char* s;
    if (!CountDown9(&s)) return;
    s[9] = 0x3C;
    NextState();
}

// original 0x47B790 (state 3): the disc, radius +9; +9 down; below 0 (s8), +1
// up (to Effect_StateRelease).
extern "C" void __cdecl EffectKind4E_Shrink(void) {
    Disc(S()[9]);
    unsigned char* t = S();
    t[9] = static_cast<unsigned char>(t[9] - 1);
    t = S();
    if (static_cast<signed char>(t[9]) >= 0) return;
    t[1] = static_cast<unsigned char>(t[1] + 1);
}

void Effect2E_Inject() {
    if (bof3::WantsShadow("effect_2e")) effect_2e::SelfTest();
    BOF3_INJECT(EffectKind48_State7_Start);
    BOF3_INJECT(EffectKind48_State7_Swell);
    BOF3_INJECT(EffectKind48_State7_Widen);
    BOF3_INJECT(EffectKind48_State7_Lift);
    BOF3_INJECT(EffectKind48_State8_Run);
    BOF3_INJECT(EffectKind48_State8_Start);
    BOF3_INJECT(EffectKind48_State8_Turn);
    BOF3_INJECT(EffectKind48_State9_Run);
    BOF3_INJECT(EffectKind48_State9_Start);
    BOF3_INJECT(EffectKind48_State9_Emit);
    BOF3_INJECT(EffectKind48_State9_Drain);
    BOF3_INJECT(EffectKind48_State10_Run);
    BOF3_INJECT(EffectKind48_State10_Start);
    BOF3_INJECT(EffectKind48_State10_Turn);
    BOF3_INJECT(EffectKind48_State11_Run);
    BOF3_INJECT(EffectKind48_State11_Start);
    BOF3_INJECT(EffectKind48_State11_Draw);
    BOF3_INJECT(EffectKind48_State12_Run);
    BOF3_INJECT(EffectKind48_State12_Start);
    BOF3_INJECT(EffectKind48_State12_Travel);
    BOF3_INJECT(EffectKind48_State12_Hold);
    BOF3_INJECT(EffectKind48_State12_Grow);
    BOF3_INJECT(EffectKind48_State12_Shrink);
    BOF3_INJECT(EffectAngle_Mean);
    BOF3_INJECT(EffectSphere_Build);
    BOF3_INJECT(EffectSphere_Draw);
    BOF3_INJECT(EffectKind4A_Run);
    BOF3_INJECT(EffectKind4A_Start);
    BOF3_INJECT(EffectKind4A_Follow);
    BOF3_INJECT(EffectKind4A_TrailInit);
    BOF3_INJECT(EffectKind4A_TrailStep);
    BOF3_INJECT(EffectKind4A_TrailDraw);
    BOF3_INJECT(EffectKind4A_DrawGlow);
    BOF3_INJECT(EffectKind4B_Run);
    BOF3_INJECT(EffectKind4B_Start);
    BOF3_INJECT(EffectKind4B_Scatter);
    BOF3_INJECT(EffectKind4B_DebrisInit);
    BOF3_INJECT(EffectKind4C_Run);
    BOF3_INJECT(EffectKind4C_Start);
    BOF3_INJECT(EffectKind4C_Crackle);
    BOF3_INJECT(EffectKind4C_DrawSpark);
    BOF3_INJECT(EffectKind4D_Run);
    BOF3_INJECT(EffectKind4D_Start);
    BOF3_INJECT(EffectKind4D_Rise);
    BOF3_INJECT(EffectKind4D_Fade);
    BOF3_INJECT(EffectKind4D_DrawColumn);
    BOF3_INJECT(EffectKind4E_Run);
    BOF3_INJECT(EffectKind4E_Start);
    BOF3_INJECT(EffectKind4E_Grow);
    BOF3_INJECT(EffectKind4E_Glow);
    BOF3_INJECT(EffectKind4E_Shrink);
}

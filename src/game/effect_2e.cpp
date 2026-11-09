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
#include "game/record_views.h"
#include "game/records.h"
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
U AddressOf(const void* p) { return Low32(p); }
// sar: an arithmetic right shift of the 32 bits as the original holds them.
U Sar(U v, unsigned n) { return static_cast<U>(static_cast<std::int32_t>(v) >> n); }
void Copy12(unsigned char* to, const void* from) { std::memcpy(to, from, 12); }
// The pointer a .data cell holds.
unsigned char* Cell(U cell) { return At(UL(At(cell))); }

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + byte]; jmp [table +
// eax * 4]: the table's `entries` handlers read in place (the fuzz swaps the
// cells for recorders); a Fatal past them, where the original jumps through
// the dword after - the next table or data.
void DispatchEffect2e(const char* who, U table, unsigned entries, unsigned byte) {
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
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_0c, rv::As<records::EffectWork>(s)->work_0c - 1u);
    s = S();
    *out = s;
    return rv::As<records::EffectWork>(s)->work_0c == 0;
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
    auto* const so = rv::AsConst<records::EffectObject>(s);
    const long g = SH_CALL(AreaMap_Elevation)(so->x, so->z);
    rv::Store<U>(rv::Current<records::EffectObject>()->height, (SW(reinterpret_cast<const unsigned char*>(&g)) + lift) << 16);
}
// Sprite_Current's +0x34 / +0x38 = the leader's x / z (each store to the record
// as read then).
void AtLeader() {
    rv::Store<U>(rv::Current<records::EffectObject>()->x, static_cast<std::uint32_t>(ObjTrio[0].x));
    rv::Store<U>(rv::Current<records::EffectObject>()->z, static_cast<std::uint32_t>(ObjTrio[0].z));
}

// States 8 and 10's start: 0x6761CC = the spiral record; its point = the
// record's +0x34, +0x38 and +0x3C + 0x2000000; its turn word `turn`; set up
// (0x4799C0); +0xC = 0x28, +2 up.
void SpiralStart(unsigned turn) {
    unsigned char* s = S();
    Effect2e_SpiralPtr = ::bof3::Img(EffectKind30_Shards_addr) + 0x524;
    SetUL(At(::bof3::Img(EffectKind30_Shards_addr) + 0x524), static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->x));
    SetUL(At(::bof3::Img(EffectKind30_Shards_addr) + 0x524 + 4), static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->z));
    const U height = static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->height) + 0x2000000u;
    Effect2e_SpiralTurn = turn;
    SetUL(At(::bof3::Img(EffectKind30_Shards_addr) + 0x524 + 8), height);
    SH_AT(void (__cdecl*)(unsigned char*), at::kSpiralInit.v)(At(::bof3::Img(EffectKind30_Shards_addr) + 0x524));
    s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_0c, 0x28);
    NextStep();
}
// States 8 and 10's frame: the spiral (0x6761CC) stepped and drawn (0x479B70);
// while +0xC is above 8 (signed) its turn word +0xD10 up `turn`; +0xC down, at 0
// a tail jmp to Effect_Release.
void SpiralTurn(unsigned turn) {
    SH_AT(void (__cdecl*)(unsigned char*), at::kSpiralDraw.v)(Cell(::bof3::Img(at::kSpiralPtr.v)));
    const unsigned char* s = S();
    if (SL(s + 0xC) > 8) AddWord(Cell(::bof3::Img(at::kSpiralPtr.v)) + 0xD10, turn);
    unsigned char* t;
    if (!CountDownC(&t)) return;
    SH_CALL(Effect_Release)();
}

// State 12's frame: the burst record stepped (0x4794D0) and drawn (0x4796B0).
void BurstFrame() {
    SH_AT(void (__cdecl*)(unsigned char*), at::kBurstStep.v)(At(::bof3::Img(EffectKind30_Shards_addr) + 0xe0));
    SH_AT(void (__cdecl*)(unsigned char*), at::kBurstDraw.v)(At(::bof3::Img(EffectKind30_Shards_addr) + 0xe0));
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
    auto* const so = rv::As<records::EffectObject>(s);
    Effect2d_DustAnchor = ::bof3::Img(Effect2e_RingRecord_addr);
    Effect2e_RingRecord = static_cast<std::uint32_t>(so->x);
    SetUL(At(::bof3::Img(Effect2e_RingRecord_addr) + 4), static_cast<std::uint32_t>(so->z));
    SetUL(At(::bof3::Img(Effect2e_RingRecord_addr) + 8), static_cast<std::uint32_t>(so->height));
    SetWord(At(::bof3::Img(Effect2e_RingRecord_addr) + 0x10), 0);
    s[9] = 0;
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_0c, 0x5F);
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
    if ((s[0xC] & At(::bof3::Img(at::kState7Steps.v))[index]) == 0) AddWord(Cell(::bof3::Img(at::kRingPtr.v)) + 0x10, At(::bof3::Img(at::kState7Adds.v))[index]);
    if ((s[0xC] & 0xF) == 0) s[9] = static_cast<unsigned char>(s[9] + 1);
    SH_AT(void (__cdecl*)(unsigned char*), at::kRing.v)(Cell(::bof3::Img(at::kRingPtr.v)));
    if (!CountDownC(&s)) return;
    SetUL(s + 0xC, 0x10);
    NextStep();
}

// original 0x478AB0 (step 2): the ring's word +0x10 up 4; the ring drawn; +0xC
// down, at 0 +2 up.
extern "C" void __cdecl EffectKind48_State7_Widen(void) {
    AddWord(Cell(::bof3::Img(at::kRingPtr.v)) + 0x10, 4);
    SH_AT(void (__cdecl*)(unsigned char*), at::kRing.v)(Cell(::bof3::Img(at::kRingPtr.v)));
    unsigned char* s;
    if (!CountDownC(&s)) return;
    s[2] = static_cast<unsigned char>(s[2] + 1);
}

// original 0x478AF0 (step 3): the ring's height +8 up 0x1000000; the ring
// drawn; once its height is 0x10000000 or more (signed), a tail jmp to
// Effect_Release.
extern "C" void __cdecl EffectKind48_State7_Lift(void) {
    AddLong(Cell(::bof3::Img(at::kRingPtr.v)) + 8, 0x1000000u);
    SH_AT(void (__cdecl*)(unsigned char*), at::kRing.v)(Cell(::bof3::Img(at::kRingPtr.v)));
    if (SL(Cell(::bof3::Img(at::kRingPtr.v)) + 8) < 0x10000000) return;
    SH_CALL(Effect_Release)();
}

// ===========================================================================
// Kind 0x48's state 8: EffectKind48_State8_Steps (two), by +2
// ===========================================================================

// original 0x478B30: jmp [EffectKind48_State8_Steps + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind48_State8_Run(void) {
    DispatchEffect2e("EffectKind48_State8_Run", AddressOf(EffectKind48_State8_Steps), EffectKind48_State8_Steps_count, 2);
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
    DispatchEffect2e("EffectKind48_State9_Run", AddressOf(EffectKind48_State9_Steps), EffectKind48_State9_Steps_count, 2);
}

// original 0x478C50 (step 0): the record at the leader's x / z, the ground
// under it; the sparks cleared (0x4790C0); +0xC = 0x28, +6 = 1, +2 up.
extern "C" void __cdecl EffectKind48_State9_Start(void) {
    AtLeader();
    Ground(0);
    SH_AT(void (__cdecl*)(), at::kSparksInit.v)();
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_0c, 0x28);
    S()[6] = 1;
    NextStep();
}

// original 0x478CC0 (step 1): the spiral stepped and drawn (0x479B70); while
// +0xC is above 8 (signed), a free spark looked for (0x47CF20, eax) and, when
// +0xC's low two bits are 0 and one was found, set (0x479160); the sparks run
// (0x479260); +0xC down, at 0 +2 up.
extern "C" void __cdecl EffectKind48_State9_Emit(void) {
    SH_AT(void (__cdecl*)(unsigned char*), at::kSpiralDraw.v)(Cell(::bof3::Img(at::kSpiralPtr.v)));
    if (SL(S() + 0xC) > 8) {
        unsigned char* const spark = SH_AT(unsigned char* (__cdecl*)(), at::kSparkFree.v)();
        if ((S()[0xC] & 3) == 0 && spark != nullptr) SH_AT(void (__cdecl*)(unsigned char*), at::kSparkSet.v)(spark);
    }
    SH_AT(unsigned char (__cdecl*)(), at::kSparksRun.v)();
    unsigned char* s;
    if (!CountDownC(&s)) return;
    s[2] = static_cast<unsigned char>(s[2] + 1);
}

// original 0x478D20 (step 2): the sparks run; when none is left (al 0), a tail
// jmp to Effect_Release.
extern "C" void __cdecl EffectKind48_State9_Drain(void) {
    if (SH_AT(unsigned char (__cdecl*)(), at::kSparksRun.v)() != 0) return;
    SH_CALL(Effect_Release)();
}

// ===========================================================================
// Kind 0x48's state 10: EffectKind48_State10_Steps (two), by +2
// ===========================================================================

// original 0x478D30: jmp [EffectKind48_State10_Steps + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind48_State10_Run(void) {
    DispatchEffect2e("EffectKind48_State10_Run", AddressOf(EffectKind48_State10_Steps), EffectKind48_State10_Steps_count, 2);
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
    DispatchEffect2e("EffectKind48_State11_Run", AddressOf(EffectKind48_State11_Steps), EffectKind48_State11_Steps_count, 2);
}

// original 0x478E60 (step 0): the record at the leader's x / z, the ground under
// it; the sphere built; +0xC = 0x78, +2 up.
extern "C" void __cdecl EffectKind48_State11_Start(void) {
    AtLeader();
    Ground(0);
    SH_CALL(EffectSphere_Build)();
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_0c, 0x78);
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
    DispatchEffect2e("EffectKind48_State12_Run", AddressOf(EffectKind48_State12_Steps), EffectKind48_State12_Steps_count, 2);
}

// original 0x478F10 (step 0): the burst's size word 0x92C49E = 0xC0; the burst
// stepped 32 times (0x4794D0); when +6 is set Sound_PlayEffect(0x203); +2 up.
extern "C" void __cdecl EffectKind48_State12_Start(void) {
    SetWord(At(::bof3::Img(at::kBurstSize.v)), 0xC0);
    for (int i = 0; i < 0x20; ++i) SH_AT(void (__cdecl*)(unsigned char*), at::kBurstStep.v)(At(::bof3::Img(EffectKind30_Shards_addr) + 0xe0));
    if (S()[6] != 0) SH_CALL(Sound_PlayEffect)(0x203);
    NextStep();
}

// original 0x478F60 (step 1): the burst's frame; the record's point +0x34 /
// +0x38 / +0x3C moved by +0xC / +0x10 / +0x14; once +0x34 is +0x18 and +0x38 is
// +0x1C, +9 = 0x2F and +2 up.
extern "C" void __cdecl EffectKind48_State12_Travel(void) {
    BurstFrame();
    unsigned char* const s = S();
    auto* const so = rv::As<records::EffectObject>(s);
    AddLong(s + 0x34, rv::As<records::EffectWork>(s)->work_0c);
    AddLong(s + 0x38, rv::As<records::EffectWork>(s)->work_10);
    AddLong(s + 0x3C, static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->step_y));
    if (static_cast<std::uint32_t>(so->x) != static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->work_18) || static_cast<std::uint32_t>(so->z) != static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->work_1c)) return;
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
    AddWord(At(::bof3::Img(at::kBurstSize.v)), 6);
    unsigned char* s;
    if (!CountDown9(&s)) return;
    s[9] = 0x20;
    NextStep();
}

// original 0x479080 (step 4): the burst's frame; its size word down 0xC; +9
// down, at 0 +2 up (to WeretigerFx_Next).
extern "C" void __cdecl EffectKind48_State12_Shrink(void) {
    BurstFrame();
    AddWord(At(::bof3::Img(at::kBurstSize.v)), 0xFFF4u);
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
    unsigned char* v = At(::bof3::Img(Effect2e_SphereVerts_addr));
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
    unsigned char* q = At(::bof3::Img(at::kSphereQuads.v));
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
    unsigned char* const light = At(::bof3::Img(Effect2e_SphereLight_addr));
    SetUL(light, 0);
    SetUL(light + 4, 0x1000);
    SetUL(light + 8, 0x1000);
    SH_CALL(Gte_VectorNormal)(reinterpret_cast<const long*>(light), reinterpret_cast<long*>(light));
    const unsigned char* vert = At(::bof3::Img(Effect2e_SphereVerts_addr));
    unsigned char* shade = At(::bof3::Img(at::kSphereShades.v));
    for (unsigned i = 0; i < at::kSphereVertCount; ++i, vert += 8, ++shade) {
        const U dot = SW(vert + 4) * UL(light + 8) + SW(vert) * UL(light) + SW(vert + 2) * UL(light + 4);
        if (static_cast<std::int32_t>(dot) <= 0) {
            *shade = 0;
            continue;
        }
        U root = static_cast<U>(SH_AT(long (__cdecl*)(long), effect_2e::at::kSqrt.v)(static_cast<long>(dot)));
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
    const unsigned char* v = At(::bof3::Img(Effect2e_SphereVerts_addr));
    for (unsigned i = 0; i < at::kSphereVertCount; ++i, v += 8) {
        const unsigned char* const s = S();
        long point[3];
        point[0] = static_cast<long>((SW(v) << 5) + UL(s + 0x34));
        point[1] = static_cast<long>((SW(v + 2) << 5) + UL(s + 0x38));
        point[2] = static_cast<long>(((SW(v + 4) + 0x1000u) << 13) + UL(s + 0x3C));
        SH_CALL(EffectGte_ProjectPoint)(point, reinterpret_cast<float*>(At(::bof3::Img(at::kSphereScreen.v) + 0xC * i)));
    }
    DrawMode(0x3C0, 0, 1);
    const unsigned char* q = At(::bof3::Img(at::kSphereQuads.v));
    const unsigned char* const shades = At(::bof3::Img(at::kSphereShades.v));
    for (unsigned n = 0; n < at::kSphereQuadCount; ++n, q += 8) {
        unsigned char* const prim = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG4)(prim);
        SH_CALL(Gpu_SetSemiTrans)(prim, 1);
        unsigned vertex[4];
        for (unsigned c = 0; c < 4; ++c) vertex[c] = SphereVertex(Word(q + 2 * c));
        for (unsigned c = 0; c < 4; ++c) Copy12(prim + 8 + 0x10 * c, At(::bof3::Img(at::kSphereScreen.v) + 0xC * vertex[c]));
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
    DispatchEffect2e("EffectKind4A_Run", AddressOf(EffectKind4A_States), EffectKind4A_States_count, 1);
}

// original 0x47A970 (state 0): the record's point = Sprite_Objects[1]'s
// (+0x34 / +0x38 / +0x3C); +0xC = 0xB4; the trail set (EffectKind4A_TrailInit);
// +1 up.
extern "C" void __cdecl EffectKind4A_Start(void) {
    rv::Store<U>(rv::Current<records::EffectObject>()->x, static_cast<std::uint32_t>(Sprite_Objects[1].x));
    rv::Store<U>(rv::Current<records::EffectObject>()->z, static_cast<std::uint32_t>(Sprite_Objects[1].z));
    rv::Store<U>(rv::Current<records::EffectObject>()->height, static_cast<std::uint32_t>(Sprite_Objects[1].height));
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_0c, 0xB4);
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
    auto* const so = rv::As<records::EffectObject>(s);
    if (rv::As<records::EffectWork>(s)->work_0c == 0xA5) Effect2e_TrailSpin = 1;
    if (SL(s + 0xC) > 0x5A) {
        so->x = static_cast<std::uint32_t>(Sprite_Objects[1].x);
        rv::Store<U>(rv::Current<records::EffectObject>()->z, static_cast<std::uint32_t>(Sprite_Objects[1].z));
        rv::Store<U>(rv::Current<records::EffectObject>()->height, static_cast<std::uint32_t>(Sprite_Objects[1].height));
    }
    SH_CALL(EffectKind4A_TrailStep)();
    SH_CALL(EffectKind4A_TrailDraw)();
    SH_CALL(EffectKind4A_DrawGlow)(reinterpret_cast<const long*>(At(::bof3::Img(EffectKind30_Shards_addr))), 0x80, 0x80, 0);
    SH_CALL(Gte_PopMatrix)();
    unsigned char* t;
    if (!CountDownC(&t)) return;
    SH_CALL(Effect_Release)();
}

// ===========================================================================
// Kind 0x4B: Effect_KindHandlers[0x4B] (0x65547C), EffectKind4B_States (two)
// ===========================================================================

// ===========================================================================
// Kind 0x4C: Effect_KindHandlers[0x4C] (0x655480), EffectKind4C_States (two)
// ===========================================================================

// ===========================================================================
// Kind 0x4D: Effect_KindHandlers[0x4D] (0x655484), EffectKind4D_States (three)
// ===========================================================================

// ===========================================================================
// Kind 0x4E: Effect_KindHandlers[0x4E] (0x655488), EffectKind4E_States (five,
// the last Effect_StateRelease)
// ===========================================================================

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
}

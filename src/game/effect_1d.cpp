// Effect kinds 0x21..0x27 - round thirteen, wave one, group E1D: the 30
// functions of the cut (analysis/round13_cut.tsv) in 0x46F2B0..0x4702F6, each
// read with capstone to its last instruction (docs/effect_1d.md section 1).
// Effect_RunObjects (ours) makes each live record of Effect_Objects (20 of 0x80
// bytes) Sprite_Current and calls Effect_KindHandlers[+5]; each kind here is a
// dispatcher by +1 through its state table (none bounded by a compare) and the
// states it names:
//
//   kind 0x21  an arm: four points round a centre built by 0x46F570 and drawn
//              by 0x46F690 each frame; it grows, lifts, holds, swirls and folds,
//              then marks itself done (+6) and is released;
//   kind 0x22  a sound, five kind-0x21 arms spawned at its point (their
//              records kept at +0xC..+0x1C), then a wait until all five are done:
//              the counter byte 0x903848 up and released;
//   kind 0x23  a sound, then 0x60 frames of kind-0x24 rays, one a frame in four
//              of every eight;
//   kind 0x24  a ray of random direction and colour from the leader (0x46FAE0
//              draws it), shrinking;
//   kind 0x25  a disc at the leader's screen point: grows, glows (a sound at one
//              frame), shrinks;
//   kind 0x26  a band of a dome at its point, widening over 0x50 frames;
//   kind 0x27  a kind-0x26 band every 16 frames for 0x1000 frames, on the ground
//              at its point (area 81's handler 0 spawns it, area_w2a.cpp).
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies. No divergence:
// each is a faithful replacement. Where the original jumps through a state
// table past its end, or would write a record past the pool on an answer the
// callee never gives, ours aborts with a message (docs/effect_1d.md section 2).
#include "game/effect_1d.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_1d_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_1d::at;
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
void AddWord(unsigned char* p, U d) { SetWord(p, Word(p) + d); }
U AddressOf(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
// sar: an arithmetic right shift of the 32 bits as the original holds them.
U Sar(U v, unsigned n) { return static_cast<U>(static_cast<std::int32_t>(v) >> n); }
// cdq; and edx, 0xFF; add eax, edx; sar eax, 8: a signed divide by 256, toward zero.
U Div256(U v) { return Sar(v + (static_cast<std::int32_t>(v) < 0 ? 0xFFu : 0u), 8); }
float ToFloat(U v) { return static_cast<float>(static_cast<std::int32_t>(v)); }
void StoreFloat(unsigned char* p, float f) { std::memcpy(p, &f, sizeof f); }

// `fld dword` then the CRT's _ftol 0x5B9550: truncation through a 64-bit
// fistp, whose NaN and out-of-range answer is the integer indefinite
// 0x8000000000000000 - so 0 in the low word the caller keeps (battle_items.cpp's
// Ftol16).
std::uint16_t Ftol16(const unsigned char* p) {
    float v;
    std::memcpy(&v, p, sizeof v);
    if (!(v > -9.2233720368547758e18f && v < 9.2233720368547758e18f)) return 0;   // NaN included
    return static_cast<std::uint16_t>(static_cast<std::uint64_t>(static_cast<std::int64_t>(v)));
}

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 1]; jmp [table + eax * 4]:
// the table's `entries` handlers read in place (the fuzz swaps the cells for
// recorders); a Fatal past them, where the original jumps through the dword
// after - the next kind's table or data.
void Dispatch(const char* who, U table, unsigned entries) {
    const unsigned state = Sprite_Current[1];
    if (state >= entries)
        bof3::Fatal("%s: state byte +1 is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/effect_1d.md section 2)",
                    who, state, entries, (unsigned)table);
    reinterpret_cast<scenario_harness::Handler>(static_cast<std::uintptr_t>(UL(At(table + 4 * state))))();
}

// Effect_FindFree's record (answer & 0xFF, as the originals widen it): a Fatal
// on an answer past the twenty (the callee answers 0xFF or 0..19; the original
// would write past the pool).
unsigned char* NewRecord(const char* who, unsigned index) {
    if (index >= at::kEffects)
        bof3::Fatal("%s: Effect_FindFree answered %u, past the 20 records - the original writes past the pool "
                    "(docs/effect_1d.md section 2)",
                    who, index);
    return Effect_Objects + index * at::kEffectStride;
}

// Kind 0x21's frame: the arm's points built and drawn (the arm is the record +
// 0xC as read on entry; both callees take it).
unsigned char* ArmFrame() {
    unsigned char* const arm = S() + 0xC;
    SH_AT(void (__cdecl*)(unsigned char*), at::kArmVertices)(arm);
    SH_AT(void (__cdecl*)(unsigned char*), at::kArmDraw)(arm);
    return arm;
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

// +1 up, Sprite_Current read afresh.
void NextState() {
    unsigned char* const s = S();
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

}  // namespace

// ===========================================================================
// Kind 0x21: Effect_KindHandlers[0x21] (0x6553D4), EffectKind21_States (six)
// ===========================================================================

// original 0x46F2B0 (0x12 bytes): jmp [EffectKind21_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind21_Run(void) { Dispatch("EffectKind21_Run", AddressOf(EffectKind21_States), EffectKind21_States_count); }

// original 0x46F2D0 (state 0): +9 = 0xF frames, +1 up.
extern "C" void __cdecl EffectKind21_Start(void) {
    S()[9] = 0xF;
    NextState();
}

// original 0x46F2F0 (state 1): the arm built and drawn; its length word +0x5E
// (the arm's +0x52, through the pointer taken on entry) one longer; +9 down, at
// 0 +9 = 8 and +1 up.
extern "C" void __cdecl EffectKind21_Grow(void) {
    unsigned char* const arm = ArmFrame();
    AddWord(arm + 0x52, 1);
    CountDown(8);
}

// original 0x46F340 (state 2): the arm built and drawn; its angle word +0x58
// (the arm's +0x4C) up 4; +9 down, at 0 +9 = 8 and +1 up.
extern "C" void __cdecl EffectKind21_Lift(void) {
    unsigned char* const arm = ArmFrame();
    AddWord(arm + 0x4C, 4);
    CountDown(8);
}

// original 0x46F390 (state 3): the arm built and drawn; +9 down, at 0 +9 =
// 0x80 and +1 up.
extern "C" void __cdecl EffectKind21_Hold(void) {
    ArmFrame();
    CountDown(0x80);
}

// original 0x46F3D0 (state 4): the arm built and drawn; its turn words +0x5A
// down 8 and +0x5C down 0x40 (the arm's +0x4E, +0x50); +9 down, at 0 +1 up
// (+9 left 0).
extern "C" void __cdecl EffectKind21_Swirl(void) {
    unsigned char* const arm = ArmFrame();
    AddWord(arm + 0x4E, 0xFFF8u);
    AddWord(arm + 0x50, 0xFFC0u);
    CountDown(-1);
}

// original 0x46F410 (state 5): the arm built and drawn; its angle word +0x58
// down 8; once it is below 0 (s16), +6 = 1 (done: EffectKind22_WaitArms reads
// it) on Sprite_Current as read then, and a tail jmp to Effect_Release.
extern "C" void __cdecl EffectKind21_Fold(void) {
    unsigned char* const arm = ArmFrame();
    AddWord(arm + 0x4C, 0xFFF8u);
    if (SW(arm + 0x4C) >= 0) return;
    S()[6] = 1;
    SH_CALL(Effect_Release)();
}

// ===========================================================================
// Kind 0x22: Effect_KindHandlers[0x22] (0x6553D8), EffectKind22_States (three)
// ===========================================================================

// original 0x46F450: jmp [EffectKind22_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind22_Run(void) { Dispatch("EffectKind22_Run", AddressOf(EffectKind22_States), EffectKind22_States_count); }

// original 0x46F470 (state 0): Sound_PlayEffect(0x209); +1 up (Sprite_Current
// read after the call).
extern "C" void __cdecl EffectKind22_Sound(void) {
    SH_CALL(Sound_PlayEffect)(0x209);
    NextState();
}

// original 0x46F490 (state 1): five arms. For i = 0..4, Effect_FindFree; when it
// answers a record: the record's address into the dword +0xC + 4 i of the
// record current on entry (none written when it answers 0xFF), +0 = 1, +6 = 0,
// +5 = 0x21; its +0xC, +0x10, +0x14 = Sprite_Current's +0x34, +0x38, +0x3C
// (the centre: x, z, height; Sprite_Current read for each); its words +0x58 =
// 6, +0x5A = 0, +0x5E = 0, +0x5C = 0x333 i (a fifth of the circle each). Then
// +1 up.
extern "C" void __cdecl EffectKind22_SpawnArms(void) {
    unsigned char* const cells = S() + 0xC;
    for (unsigned i = 0; i < 5; ++i) {
        const unsigned char found = SH_CALL(Effect_FindFree)();
        if (found == 0xFF) continue;
        unsigned char* const e = NewRecord("EffectKind22_SpawnArms", found);
        SetUL(cells + 4 * i, AddressOf(e));
        e[0] = 1;
        e[6] = 0;
        e[5] = 0x21;
        SetUL(e + 0xC, UL(S() + 0x34));
        SetUL(e + 0x10, UL(S() + 0x38));
        const U height = UL(S() + 0x3C);
        SetUL(e + 0x14, height);
        SetWord(e + 0x58, 6);
        SetWord(e + 0x5A, 0);
        SetWord(e + 0x5E, 0);
        SetWord(e + 0x5C, 0x333u * i);
    }
    NextState();
}

// original 0x46F530 (state 2): the five records whose addresses +0xC..+0x1C
// hold - each dereferenced as it stands (a cell EffectKind22_SpawnArms left
// unwritten is whatever the record held: docs/effect_1d.md section 7). Any
// whose +6 is 0: return. All five done: the counter byte 0x903848 up and
// Effect_Release.
extern "C" void __cdecl EffectKind22_WaitArms(void) {
    const unsigned char* const s = S();
    for (unsigned i = 0; i < 5; ++i)
        if (At(UL(s + 0xC + 4 * i))[6] == 0) return;
    unsigned char* const counter = At(at::kCounter0);
    counter[0] = static_cast<unsigned char>(counter[0] + 1);
    SH_CALL(Effect_Release)();
}

// ===========================================================================
// Kind 0x23: Effect_KindHandlers[0x23] (0x6553DC), EffectKind23_States (three,
// the third FC1's Effect_StateRelease)
// ===========================================================================

// original 0x46F790: jmp [EffectKind23_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind23_Run(void) { Dispatch("EffectKind23_Run", AddressOf(EffectKind23_States), EffectKind23_States_count); }

// original 0x46F7B0 (state 0): +9 = 0x60 frames, +1 up, then
// Sound_PlayEffect(0x204).
extern "C" void __cdecl EffectKind23_Start(void) {
    S()[9] = 0x60;
    NextState();
    SH_CALL(Sound_PlayEffect)(0x204);
}

// original 0x46F7D0 (state 1): with +9 bit 2 set, Effect_FindFree; a record
// found gets +0 = 1 and +5 = 0x24 (a ray). Then +9 down, at 0 +1 up.
extern "C" void __cdecl EffectKind23_SpawnRays(void) {
    if (S()[9] & 4) {
        const unsigned char found = SH_CALL(Effect_FindFree)();
        if (found != 0xFF) {
            unsigned char* const e = NewRecord("EffectKind23_SpawnRays", found);
            e[0] = 1;
            e[5] = 0x24;
        }
    }
    CountDown(-1);
}

// ===========================================================================
// Kind 0x24: Effect_KindHandlers[0x24] (0x6553E0), EffectKind24_States (three,
// the third Effect_StateRelease)
// ===========================================================================

// original 0x46F820: jmp [EffectKind24_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind24_Run(void) { Dispatch("EffectKind24_Run", AddressOf(EffectKind24_States), EffectKind24_States_count); }

// original 0x46F840 (state 0): the ray, from the record current on entry: the
// length word +0xC = 0x600, the scales +0x12 = +0x14 = 0x100, the angles +0xE =
// Rand & 0x700 and +0x10 = Rand & 0xF00; the colour +0x18, +0x17, +0x16 = 0xFF
// (in that order), then +0x16 = 0 when Rand is odd and +0x18 = 0 when the next
// is. +1 up (Sprite_Current read then).
extern "C" void __cdecl EffectKind24_Start(void) {
    unsigned char* const ray = S() + 0xC;
    SetWord(ray, 0x600);
    SetWord(ray + 6, 0x100);
    SetWord(ray + 8, 0x100);
    SetWord(ray + 2, static_cast<U>(SH_CALL(Rand)()) & 0x700u);
    SetWord(ray + 4, static_cast<U>(SH_CALL(Rand)()) & 0xF00u);
    ray[0xC] = 0xFF;
    ray[0xB] = 0xFF;
    ray[0xA] = 0xFF;
    if (SH_CALL(Rand)() & 1) ray[0xA] = 0;
    if (SH_CALL(Rand)() & 1) ray[0xC] = 0;
    NextState();
}

// original 0x46F8B0 (state 1): the ray drawn (0x46FAE0, the record + 0xC as read
// on entry); its first scale +0x12 down 0x10, held at 0 (s16); while that is
// below 0x80 the second +0x14 down 0x10; once the second is below 0 (s16), the
// FIRST is set 0 (as the original writes it) and +1 up.
extern "C" void __cdecl EffectKind24_Shrink(void) {
    unsigned char* const ray = S() + 0xC;
    SH_AT(void (__cdecl*)(const unsigned char*), at::kRayDraw)(ray);
    AddWord(ray + 6, 0xFFF0u);
    if (SW(ray + 6) < 0) SetWord(ray + 6, 0);
    if (SW(ray + 6) < 0x80) AddWord(ray + 8, 0xFFF0u);
    if (SW(ray + 8) >= 0) return;
    SetWord(ray + 6, 0);
    NextState();
}

// ===========================================================================
// Kind 0x25: Effect_KindHandlers[0x25] (0x6553E4), EffectKind25_States (five,
// the fifth Effect_StateRelease)
// ===========================================================================

// original 0x46F900: jmp [EffectKind25_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind25_Run(void) { Dispatch("EffectKind25_Run", AddressOf(EffectKind25_States), EffectKind25_States_count); }

// original 0x46F920 (state 0): the leader's point projected. Prim_VertexScratch
// [0] = (leader x sar 9) - 0x4000, [2] = -(the leader's height word / 2, toward
// zero), [1] = (leader z sar 9) - 0x4000 (stored in that order; [3] not
// written); Gte_RotTransPers(Prim_VertexScratch, MapView_ScreenXY, a local) -
// the original hands a fourth argument, a second local, the callee never reads.
// Its answer to the dword +0x60; the two floats of MapView_ScreenXY through
// _ftol to the words +0x2E and +0x30 (Sprite_Current read for each store); +9 =
// 0, +1 up.
extern "C" void __cdecl EffectKind25_Start(void) {
    const U x = UL(At(at::kLeaderX));
    const U z = UL(At(at::kLeaderZ));
    Prim_VertexScratch[0] = static_cast<short>(Sar(x, 9) - 0x4000u);
    const std::int32_t height = static_cast<std::int16_t>(Word(At(at::kLeaderHeight)));
    Prim_VertexScratch[2] = static_cast<short>(-(height / 2));
    Prim_VertexScratch[1] = static_cast<short>(Sar(z, 9) - 0x4000u);
    long depth = 0;
    const long answer =
        SH_CALL(Gte_RotTransPers)(Prim_VertexScratch, reinterpret_cast<unsigned long*>(MapView_ScreenXY), &depth);
    SetUL(S() + 0x60, static_cast<U>(answer));
    const auto* const screen = reinterpret_cast<const unsigned char*>(MapView_ScreenXY);
    SetWord(S() + 0x2E, Ftol16(screen));
    SetWord(S() + 0x30, Ftol16(screen + 4));
    S()[9] = 0;
    NextState();
}

// original 0x46F9D0 (state 1): the disc at (+0x2E, +0x30), radius +9, the
// centre 0x80 and the rim 0; +9 up 0xF; above 0x3C (unsigned), +9 = 0xFF and
// +1 up.
extern "C" void __cdecl EffectKind25_Grow(void) {
    const unsigned char* const s = S();
    SH_CALL(EffectKind25_DrawDisc)(Word(s + 0x2E), Word(s + 0x30), s[9], 0x80, 0);
    unsigned char* t = S();
    t[9] = static_cast<unsigned char>(t[9] + 0xF);
    t = S();
    if (t[9] <= 0x3C) return;
    t[9] = 0xFF;
    NextState();
}

// original 0x46FA20 (state 2): the disc at (+0x2E, +0x30), radius 0x3C + (+9 &
// 1), the centre 0x80 and the rim 0; at +9 = 0xD7 Sound_PlayEffect(0x208); +9
// down, at 0 +9 = 0x3C and +1 up.
extern "C" void __cdecl EffectKind25_Glow(void) {
    const unsigned char* const s = S();
    SH_CALL(EffectKind25_DrawDisc)(Word(s + 0x2E), Word(s + 0x30), 0x3Cu + (s[9] & 1u), 0x80, 0);
    if (S()[9] == 0xD7) SH_CALL(Sound_PlayEffect)(0x208);
    CountDown(0x3C);
}

// original 0x46FA90 (state 3): the disc at (+0x2E, +0x30), radius +9, the
// centre 0x80 and the rim 0; +9 down; below 0 (s8), +1 up.
extern "C" void __cdecl EffectKind25_Shrink(void) {
    const unsigned char* const s = S();
    SH_CALL(EffectKind25_DrawDisc)(Word(s + 0x2E), Word(s + 0x30), s[9], 0x80, 0);
    unsigned char* t = S();
    t[9] = static_cast<unsigned char>(t[9] - 1);
    t = S();
    if (static_cast<signed char>(t[9]) >= 0) return;
    t[1] = static_cast<unsigned char>(t[1] + 1);
}

// original 0x46FCF0 (0x16E bytes): a draw mode (Gpu_GetTPage(0, 1, 0x3C0, 0),
// Gpu_SetDrawMode(packet, 0, 1, it, 0), committed 0xC); then for the angles a =
// 0, 0x80, .. 0xF80, a triangle at the packet cursor (read afresh each time):
// Gpu_SetPolyG3, Gpu_SetSemiTrans(1); vertex 0 the centre (x, y as floats), then
// Math_Cos(a), vertex 1 x = x + (cos * r sar 12), Math_Sin(a), its y likewise;
// Math_Cos(b), vertex 2 x, Math_Sin(b), vertex 2 y, with b = (a + 0x80) & 0xFFF;
// vertex 0 shaded `centre`, 1 and 2 `rim` (each r, g, b); committed 0x34. x, y
// and r are read as s16, the shades as bytes (the callers push whole registers).
extern "C" void __cdecl EffectKind25_DrawDisc(unsigned x, unsigned y, unsigned radius, unsigned centre, unsigned rim) {
    const unsigned tpage = SH_CALL(Gpu_GetTPage)(0, 1, 0x3C0, 0);
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage & 0xFFFFu, 0);
    SH_CALL(Gfx_CommitPrim)(1, 0xC);
    const U cx = static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(x)));
    const U cy = static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(y)));
    const U r = static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(radius)));
    const auto rim_shade = static_cast<unsigned char>(rim);
    const float fx = ToFloat(cx);
    const float fy = ToFloat(cy);
    U a = 0;
    U next = 0;
    do {
        unsigned char* const prim = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG3)(prim);
        SH_CALL(Gpu_SetSemiTrans)(prim, 1);
        next += 0x80;
        const U b = next & 0xFFFu;
        StoreFloat(prim + 8, fx);
        StoreFloat(prim + 0xC, fy);
        StoreFloat(prim + 0x18, ToFloat(Sar(static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(a))) * r, 12) + cx));
        StoreFloat(prim + 0x1C, ToFloat(Sar(static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(a))) * r, 12) + cy));
        StoreFloat(prim + 0x28, ToFloat(Sar(static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(b))) * r, 12) + cx));
        const float y2 = ToFloat(Sar(static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(b))) * r, 12) + cy);
        const auto centre_shade = static_cast<unsigned char>(centre);
        prim[4] = prim[5] = prim[6] = centre_shade;
        prim[0x14] = prim[0x15] = prim[0x16] = rim_shade;
        StoreFloat(prim + 0x2C, y2);
        prim[0x24] = prim[0x25] = prim[0x26] = rim_shade;
        SH_CALL(Gfx_CommitPrim)(1, 0x34);
        a += 0x80;
    } while ((next & 0xFFFFu) < 0x1000u);
}

// ===========================================================================
// Kind 0x26: Effect_KindHandlers[0x26] (0x6553E8), EffectKind26_States (three,
// the third Effect_StateRelease)
// ===========================================================================

// original 0x46FE60: jmp [EffectKind26_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind26_Run(void) { Dispatch("EffectKind26_Run", AddressOf(EffectKind26_States), EffectKind26_States_count); }

// original 0x46FE80 (state 0): the dword +0xC = 0, +1 up.
extern "C" void __cdecl EffectKind26_Start(void) {
    SetUL(S() + 0xC, 0);
    NextState();
}

// original 0x46FEA0 (state 1): the band at the record's point (+0x34, +0x38,
// +0x3C) and t = the word +0xC, all read on entry; then the dword +0xC up 0x10;
// above 0x500 (signed), +1 up.
extern "C" void __cdecl EffectKind26_Widen(void) {
    const unsigned char* const s = S();
    SH_CALL(EffectKind26_DrawBand)(Long(s + 0x34), Long(s + 0x38), Long(s + 0x3C), Word(s + 0xC));
    unsigned char* t = S();
    SetUL(t + 0xC, UL(t + 0xC) + 0x10u);
    t = S();
    if (static_cast<std::int32_t>(UL(t + 0xC)) <= 0x500) return;
    t[1] = static_cast<unsigned char>(t[1] + 1);
}

// original 0x46FFB0 (0x346 bytes): a draw mode (as EffectKind25_DrawDisc's,
// committed 0xC) and EffectGte_LoadMapCamera; then, of t's low word: the outer
// angle ro = t, at most 0x400, the outer shade 0x80 - or, past 0x400, ro =
// 0x400 and the shade (0x500 - t) * 0x80 / 0x100 (a signed divide, its low
// byte); the inner angle ri = t - 0x100 (low word) and the inner shade 0 - or,
// when that is below 0 (s16), ri = 0 and the shade (0x100 - t) * 0x80 / 0x100's
// low byte. For a = 0, 0x100, .. 0xF00 (b = a + 0x100, each as its low word): a
// Gouraud quad at the packet cursor (read afresh), Gpu_SetPolyG4,
// Gpu_SetSemiTrans(1), its four vertices EffectGte_ProjectPoint of the world
// points (x + ((3 cos(u) sin(p)) sar 8 with the low four bits cleared), z +
// ((3 sin(u) sin(p)) sar 8, cleared likewise), height + (3 cos(p) << 12)) for
// (u, p) = (a, ro), (a, ri), (b, ro), (b, ri), shaded outer, inner, outer,
// inner; committed 0x44. Each Math_* is called in the original's order (below).
extern "C" void __cdecl EffectKind26_DrawBand(long x, long z, long height, unsigned t) {
    const unsigned tpage = SH_CALL(Gpu_GetTPage)(0, 1, 0x3C0, 0);
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage & 0xFFFFu, 0);
    SH_CALL(Gfx_CommitPrim)(1, 0xC);
    SH_CALL(EffectGte_LoadMapCamera)();
    const U tw = t & 0xFFFFu;
    U outer = 0x80;   // bl: only its low byte is stored
    U ro = tw;
    if (tw > 0x400) {
        ro = 0x400;
        outer = Div256((0x500u - tw) << 7);
    }
    U ri = (t - 0x100u) & 0xFFFFu;
    U inner = 0;
    if (static_cast<std::int16_t>(ri) < 0) {
        inner = Div256(static_cast<U>(-static_cast<std::int32_t>(static_cast<std::int16_t>(ri))) << 7);
        ri = 0;
    }
    const auto outer_shade = static_cast<unsigned char>(outer);
    const auto inner_shade = static_cast<unsigned char>(inner);
    const U ux = static_cast<U>(x), uz = static_cast<U>(z), uh = static_cast<U>(height);
    // The original's point: three dwords on its stack (x, z, height), rewritten
    // before each projection.
    long point[3];
    auto horizontal = [](U trig, U polar) { return Sar(trig * polar * 3u, 8) & 0xFFFFFFF0u; };
    auto Cos = [](U angle) { return static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(angle))); };
    auto Sin = [](U angle) { return static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(angle))); };
    U a = 0;
    for (unsigned n = 0; n < 16; ++n) {
        unsigned char* const prim = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG4)(prim);
        SH_CALL(Gpu_SetSemiTrans)(prim, 1);
        const U a16 = a & 0xFFFFu;
        // vertex 0: (a, ro), outer
        {
            const U cu = Cos(a16);
            const U sp = Sin(ro);
            point[0] = static_cast<long>(horizontal(cu, sp) + ux);
            const U su = Sin(a16);
            const U sp2 = Sin(ro);
            point[1] = static_cast<long>(horizontal(su, sp2) + uz);
            point[2] = static_cast<long>((Cos(ro) * 3u << 12) + uh);
            SH_CALL(EffectGte_ProjectPoint)(point, reinterpret_cast<float*>(prim + 8));
            prim[4] = prim[5] = prim[6] = outer_shade;
        }
        // vertex 1: (a, ri), inner
        {
            const U sp = Sin(ri);
            const U cu = Cos(a16);
            point[0] = static_cast<long>(horizontal(cu, sp) + ux);
            const U sp2 = Sin(ri);
            const U su = Sin(a16);
            point[1] = static_cast<long>(horizontal(su, sp2) + uz);
            point[2] = static_cast<long>((Cos(ri) * 3u << 12) + uh);
            SH_CALL(EffectGte_ProjectPoint)(point, reinterpret_cast<float*>(prim + 0x18));
            prim[0x14] = prim[0x15] = prim[0x16] = inner_shade;
        }
        a += 0x100;
        const U b16 = a & 0xFFFFu;
        // vertex 2: (b, ro), outer
        {
            const U cu = Cos(b16);
            const U sp = Sin(ro);
            point[0] = static_cast<long>(horizontal(cu, sp) + ux);
            const U su = Sin(b16);
            const U sp2 = Sin(ro);
            point[1] = static_cast<long>(horizontal(su, sp2) + uz);
            point[2] = static_cast<long>((Cos(ro) * 3u << 12) + uh);
            SH_CALL(EffectGte_ProjectPoint)(point, reinterpret_cast<float*>(prim + 0x28));
            prim[0x24] = prim[0x25] = prim[0x26] = outer_shade;
        }
        // vertex 3: (b, ri), inner
        {
            const U cu = Cos(b16);
            const U sp = Sin(ri);
            point[0] = static_cast<long>(horizontal(cu, sp) + ux);
            const U su = Sin(b16);
            const U sp2 = Sin(ri);
            point[1] = static_cast<long>(horizontal(su, sp2) + uz);
            point[2] = static_cast<long>((Cos(ri) * 3u << 12) + uh);
            SH_CALL(EffectGte_ProjectPoint)(point, reinterpret_cast<float*>(prim + 0x38));
            prim[0x34] = prim[0x35] = prim[0x36] = inner_shade;
        }
        SH_CALL(Gfx_CommitPrim)(1, 0x44);
    }
}

// ===========================================================================
// Kind 0x27: Effect_KindHandlers[0x27] (0x6553EC), EffectKind27_States (three,
// the third Effect_StateRelease)
// ===========================================================================

// original 0x46FEE0: jmp [EffectKind27_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind27_Run(void) { Dispatch("EffectKind27_Run", AddressOf(EffectKind27_States), EffectKind27_States_count); }

// original 0x46FF00 (state 0): the dword +0xC = 0x1000 frames, +1 up.
extern "C" void __cdecl EffectKind27_Start(void) {
    SetUL(S() + 0xC, 0x1000);
    NextState();
}

// original 0x46FF20 (state 1): when the low four bits of +0xC are 0,
// Effect_FindFree; a record found gets +0 = 1, +5 = 0x26 (a band), +0x34 /
// +0x38 = Sprite_Current's, and +0x3C = AreaMap_Elevation(Sprite_Current's
// +0x34, +0x38) (its low word, sign-extended) << 16 - the ground under it.
// Then the dword +0xC down; at 0, +1 up.
extern "C" void __cdecl EffectKind27_Emit(void) {
    if ((S()[0xC] & 0xF) == 0) {
        const unsigned char found = SH_CALL(Effect_FindFree)();
        if (found != 0xFF) {
            unsigned char* const e = NewRecord("EffectKind27_Emit", found);
            e[0] = 1;
            e[5] = 0x26;
            SetUL(e + 0x34, UL(S() + 0x34));
            SetUL(e + 0x38, UL(S() + 0x38));
            const unsigned char* const s = S();
            const long ground = SH_CALL(AreaMap_Elevation)(Long(s + 0x34), Long(s + 0x38));
            SetUL(e + 0x3C, static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(ground))) << 16);
        }
    }
    unsigned char* t = S();
    SetUL(t + 0xC, UL(t + 0xC) - 1u);
    t = S();
    if (UL(t + 0xC) != 0) return;
    t[1] = static_cast<unsigned char>(t[1] + 1);
}

void Effect1D_Inject() {
    if (bof3::WantsShadow("effect_1d")) effect_1d::SelfTest();
    BOF3_INJECT(EffectKind21_Run);
    BOF3_INJECT(EffectKind21_Start);
    BOF3_INJECT(EffectKind21_Grow);
    BOF3_INJECT(EffectKind21_Lift);
    BOF3_INJECT(EffectKind21_Hold);
    BOF3_INJECT(EffectKind21_Swirl);
    BOF3_INJECT(EffectKind21_Fold);
    BOF3_INJECT(EffectKind22_Run);
    BOF3_INJECT(EffectKind22_Sound);
    BOF3_INJECT(EffectKind22_SpawnArms);
    BOF3_INJECT(EffectKind22_WaitArms);
    BOF3_INJECT(EffectKind23_Run);
    BOF3_INJECT(EffectKind23_Start);
    BOF3_INJECT(EffectKind23_SpawnRays);
    BOF3_INJECT(EffectKind24_Run);
    BOF3_INJECT(EffectKind24_Start);
    BOF3_INJECT(EffectKind24_Shrink);
    BOF3_INJECT(EffectKind25_Run);
    BOF3_INJECT(EffectKind25_Start);
    BOF3_INJECT(EffectKind25_Grow);
    BOF3_INJECT(EffectKind25_Glow);
    BOF3_INJECT(EffectKind25_Shrink);
    BOF3_INJECT(EffectKind25_DrawDisc);
    BOF3_INJECT(EffectKind26_Run);
    BOF3_INJECT(EffectKind26_Start);
    BOF3_INJECT(EffectKind26_Widen);
    BOF3_INJECT(EffectKind26_DrawBand);
    BOF3_INJECT(EffectKind27_Run);
    BOF3_INJECT(EffectKind27_Start);
    BOF3_INJECT(EffectKind27_Emit);
}

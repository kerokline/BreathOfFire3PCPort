// Moved here by tools/move_funcs.py from magic_s36.cpp (docs/source-layout-round.md).
#include "game/magic/magic_ball.h"

#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s36.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/records_gpu.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = magic_harness::at;

}  // namespace

namespace {

using magic_harness::Mem;

using magic_harness::Pointer;

using move_script::Long;

using move_script::SetWord;

using move_script::Word;

// The scratch the overlays keep their working values in: SharedScratch's
// sixteen bytes (0x903850..0x90385F, Scratch_Swap at +0xC) and the four
// SVECTORs of Prim_VertexScratch (0x9037A0..0x9037BF). Both are read again
// after every call, as the originals read them.
constexpr ::bof3::image::Va kS{SharedScratch_addr};

unsigned char* Sc() { return Sprite_Current; }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

unsigned char TargetByte() { return Mem(::bof3::Img(at::kTarget))[0]; }

std::uint16_t SW(unsigned k) { return Word(Mem(::bof3::Img(kS.v) + k)); }

short SS(unsigned k) { return static_cast<short>(SW(k)); }

unsigned char SB(unsigned k) { return Mem(::bof3::Img(kS.v) + k)[0]; }

void SetSW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(kS.v) + k), v & 0xFFFF); }

std::int32_t SD(unsigned k) { return Long(Mem(::bof3::Img(kS.v) + k)); }

short S16(const unsigned char* at) { return static_cast<short>(move_script::Word(at)); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

void AddB(unsigned char& b, unsigned v) { b = static_cast<unsigned char>(b + v); }

std::uint32_t Key(const void* p) { return Low32(p); }

std::uint32_t U32(const unsigned char* at) { return static_cast<std::uint32_t>(Long(at)); }

std::int32_t I32(std::uint32_t v) { return static_cast<std::int32_t>(v); }

// `imul` then `sar 0xC`: the 32-bit product wraps, the shift is arithmetic.
int Mul12(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12; }

// `add` of two dwords, wrapping.
int Add(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b)); }

// `fild dword` then `fstp dword`: an integer vertex as a float.
void PutFloat(unsigned char* at, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(at, &f, sizeof f);
}

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }

std::uint32_t RandCall() { return static_cast<std::uint32_t>(MH_CALL(Rand)()); }

unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }

// This group's functions called by address, as the originals call them: in
// the game the jmp Inject put there (or Capcom's code under
// BOF3X_ORIGINAL), in the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();

using Fn2 = void (__cdecl*)(int, int);

void Call0(std::uint32_t address) { MH_WORD(Fn0, address)(); }

void Call2(std::uint32_t address, int a, int b) { MH_WORD(Fn2, address)(a, b); }

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

// Math_Ratan2 of two dword differences, each `fild` into a float.
int Ratan2(std::int32_t a, std::int32_t b) {
    return MH_CALL(Math_Ratan2)(static_cast<float>(a), static_cast<float>(b));
}

// The angle from Sprite_Current to `to` in the effects' plane: Math_Ratan2 of
// (to +0x34 - +0x34, to +0x38 - +0x38), Sprite_Current read now.
int AngleTo(const unsigned char* to) {
    const unsigned char* const s = Sc();
    const std::int32_t dz = I32(U32(to + 0x38) - U32(s + 0x38));
    const std::int32_t dx = I32(U32(to + 0x34) - U32(s + 0x34));
    return Ratan2(dx, dz);
}

// A draw-mode packet (tpage `tpage`, dithered) committed to layer 3.
void DrawModeCommit3(unsigned tpage) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0);
    MH_CALL(Gfx_CommitPrim)(3, 0xC);
}

// The source sprite's point as MagicFx_StepTowardPoint takes it: (x sar 9) -
// 0x4000, (z sar 9) - 0x4000, (y + lift) sar 0x11; the fourth word the
// original pushes is its own uninitialised stack, which the callee never reads.
void StepTowardSource(const unsigned char* src, std::uint32_t lift, int speed) {
    const std::uint32_t x = static_cast<std::uint32_t>((rv::As<records::SpriteHead>(src)->x >> 9) - 0x4000);
    const std::uint32_t z = static_cast<std::uint32_t>((rv::As<records::SpriteHead>(src)->z >> 9) - 0x4000);
    const std::uint32_t y = static_cast<std::uint32_t>(I32(U32(src + 0x3C) + lift) >> 0x11);
    MH_CALL(MagicFx_StepTowardPoint)(x, z, y, 0, speed);
}

// Sprite_Current's +0x10 = (+0x10 & 0xFFF) - (+0xC & 0xFFF), then its
// absolute value (`not / inc`): how far the heading turned this frame. True
// when it is above 0x600 and below 0xA00 (it went past the target).
bool TurnedPast() {
    {
        unsigned char* const s = Sc();
        records::BattleTask* const ts = rv::As<records::BattleTask>(s);
        ts->step_z = I32((U32(s + 0x10) & 0xFFF) - (U32(s + 0xC) & 0xFFF));
    }
    {
        unsigned char* const s = Sc();
        records::BattleTask* const ts = rv::As<records::BattleTask>(s);
        const std::int32_t v = ts->step_z;
        if (v < 0) ts->step_z = I32(0u - static_cast<std::uint32_t>(v));
    }
    const std::int32_t v = rv::Current<records::BattleTask>()->step_z;
    return v > 0x600 && v < 0xA00;
}

}  // namespace

#define S36_EXPORT extern "C" __attribute__((disable_tail_calls))

namespace {

// The core's sparks: five lines at (Rand & 0x1FF) + 0, 0x333, 0x666, 0x999,
// 0xCCC, alternately long and short, with the jitter mask `mask`.
void FiveSparks(int mask) {
    static constexpr int kAngles[5] = {0, 0x333, 0x666, 0x999, 0xCCC};
    for (unsigned i = 0; i < 5; ++i) {
        const int angle = static_cast<int>(RandCall() & 0x1FF) + kAngles[i];
        Call2(i & 1 ? bof3::addr::MagicBall_DrawSparkShort : bof3::addr::MagicBall_DrawSpark, angle, mask);
    }
}

// Three more long ones at + 0x400, 0x800, 0xC14.
void ThreeSparks(int mask) {
    static constexpr int kAngles[3] = {0x400, 0x800, 0xC14};
    for (int k : kAngles) Call2(bof3::addr::MagicBall_DrawSpark, static_cast<int>(RandCall() & 0x1FF) + k, mask);
}

// Target flags 0x10, sound 0x101, +9 0x10, +2 on: the core's hit.
void CoreHit() {
    MH_CALL(Battle_SetTargetFlags)(TargetByte(), 0x10);
    MH_CALL(Sound_PlayById)(0x101);
    Sc()[9] = 0x10;
    Inc(Sc()[2]);
}

// A triangle's second and third points, `radius` (the scratch word, read at
// each use) round the centre (x, y) at the scratch angle 0x903854 and at
// `next` (the angle stored there first).
void RimPair(unsigned char* p, unsigned first, unsigned second, int x, int y, unsigned radius_word, unsigned next) {
    int v = MH_CALL(Math_Sin)(SS(4));
    PutFloat(p + first, Add(Mul12(v, SS(radius_word)), x));
    v = MH_CALL(Math_Cos)(SS(4));
    PutFloat(p + first + 4, Add(Mul12(v, SS(radius_word)), y));
    SetSW(4, next);
    v = MH_CALL(Math_Sin)(static_cast<short>(next));
    PutFloat(p + second, Add(Mul12(v, SS(radius_word)), x));
    v = MH_CALL(Math_Cos)(SS(4));
    PutFloat(p + second + 4, Add(Mul12(v, SS(radius_word)), y));
}

// Sixteen semi-transparent gouraud triangles round the screen point (x, y),
// the centre and the rim shaded from the scratch bytes: the centre (c0, c1,
// c2), the rim (r0, r1, r2) - each 0xFF for the constant 1.
void DiscG3(int x, int y, const unsigned (&centre)[3], const unsigned (&rim)[3]) {
    const auto shade = [](unsigned k) { return k == 0xFF ? static_cast<unsigned char>(1) : SB(k); };
    for (int i = 0; i < 0x10; ++i) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        PutFloat(p + 8, x);
        PutFloat(p + 0xC, y);
        RimPair(p, 0x18, 0x28, x, y, 0, static_cast<unsigned>(i + 1) << 8);
        for (unsigned c = 0; c < 3; ++c) p[4 + c] = shade(centre[c]);
        for (unsigned c = 0; c < 3; ++c) p[0x14 + c] = shade(rim[c]);
        for (unsigned c = 0; c < 3; ++c) p[0x24 + c] = shade(rim[c]);
        MH_CALL(Gfx_CommitPrim)(3, 0x34);
    }
}

// Sixteen semi-transparent gouraud quads between the radii (words 0x903850
// and 0x903852) round (x, y): the shade (0x903858, 0x90385A, 0x90385C) on the
// inner edge and 1 on the outer, or the other way round.
void RingG4(int x, int y, bool shade_inner) {
    for (int i = 0; i < 0x10; ++i) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        int v = MH_CALL(Math_Sin)(SS(4));
        PutFloat(p + 8, Add(Mul12(v, SS(0)), x));
        v = MH_CALL(Math_Cos)(SS(4));
        PutFloat(p + 0xC, Add(Mul12(v, SS(0)), y));
        v = MH_CALL(Math_Sin)(SS(4));
        PutFloat(p + 0x28, Add(Mul12(v, SS(2)), x));
        v = MH_CALL(Math_Cos)(SS(4));
        PutFloat(p + 0x2C, Add(Mul12(v, SS(2)), y));
        const unsigned next = static_cast<unsigned>(i + 1) << 8;
        SetSW(4, next);
        v = MH_CALL(Math_Sin)(static_cast<short>(next));
        PutFloat(p + 0x18, Add(Mul12(v, SS(0)), x));
        v = MH_CALL(Math_Cos)(SS(4));
        PutFloat(p + 0x1C, Add(Mul12(v, SS(0)), y));
        v = MH_CALL(Math_Sin)(SS(4));
        PutFloat(p + 0x38, Add(Mul12(v, SS(2)), x));
        v = MH_CALL(Math_Cos)(SS(4));
        PutFloat(p + 0x3C, Add(Mul12(v, SS(2)), y));
        const unsigned lit = shade_inner ? 0x04u : 0x24u, dim = shade_inner ? 0x24u : 0x04u;
        for (unsigned c = 0; c < 3; ++c) {
            p[dim + c] = 1;
            p[dim + 0x10 + c] = 1;
        }
        for (unsigned c = 0; c < 3; ++c) {
            p[lit + c] = SB(8 + 2 * c);
            p[lit + 0x10 + c] = SB(8 + 2 * c);
        }
        MH_CALL(Gfx_CommitPrim)(3, 0x44);
    }
}

// 0x4F2F20 / 0x4F3140: a spark - gouraud lines out from the core's screen
// point, starting at radius 0x10 on the word angle `angle`; each line's far
// end turns by (Rand & jitter) and its radius moves by sin((i & 0x1F) << 7) x
// (Rand & mask) >> 12, until the radius drops below `limit`. The counter i
// lives in the argument's own slot from 1 on. Shades: +9 x 6, or before step 3
// (r x 3 + near) x 2 and (r x 3 + far) x 2 for the two ends, blue a third.
void Spark(int angle, int mask, unsigned jitter, int near, int far, short limit) {
    DrawModeCommit3(0x35);
    SetSW(0, 0x10);
    SetSW(4, static_cast<unsigned>(angle));
    const unsigned char* const s = Sc();
    const int x = S16(s + 0x2E), y = S16(s + 0x30);
    SetSW(6, s[9] * 6u);
    std::uint32_t i = 1;
    do {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetLineG2)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        int v = MH_CALL(Math_Sin)(SS(4));
        PutFloat(p + 8, Add(Mul12(v, SS(0)), x));
        v = MH_CALL(Math_Cos)(SS(4));
        PutFloat(p + 0xC, Add(Mul12(v, SS(0)), y));
        const unsigned wave = (i & 0x1F) << 7;
        SetSW(2, wave);
        const int sw = MH_CALL(Math_Sin)(static_cast<short>(wave));
        const std::uint32_t r1 = RandCall() & static_cast<std::uint32_t>(static_cast<int>(static_cast<short>(mask)));
        const int step = Mul12(sw, static_cast<int>(r1));
        SetSW(0, SW(0) + static_cast<unsigned>(step));
        const std::uint32_t r2 = RandCall();
        const unsigned turned = ((r2 & jitter) + static_cast<std::uint32_t>(SD(4))) & 0xFFF;
        SetSW(4, turned);
        v = MH_CALL(Math_Sin)(static_cast<short>(turned));
        PutFloat(p + 0x18, Add(Mul12(v, SS(0)), x));
        v = MH_CALL(Math_Cos)(SS(4));
        PutFloat(p + 0x1C, Add(Mul12(v, SS(0)), y));
        if (Sc()[2] < 3) SetSW(6, static_cast<unsigned>((static_cast<std::uint32_t>(SD(0)) * 3u + static_cast<std::uint32_t>(near)) * 2u));
        p[4] = SB(6);
        p[5] = SB(6);
        p[6] = static_cast<unsigned char>(SS(6) / 3);
        if (Sc()[2] < 3) SetSW(6, static_cast<unsigned>((static_cast<std::uint32_t>(SD(0)) * 3u + static_cast<std::uint32_t>(far)) * 2u));
        p[0x14] = SB(6);
        p[0x15] = SB(6);
        p[0x16] = static_cast<unsigned char>(SS(6) / 3);
        MH_CALL(Gfx_CommitPrim)(3, 0x24);
        ++i;
    } while (SS(0) >= limit);
}

}  // namespace

// original 0x4F1E40: the kind-2 task. A two-entry stack table by +1:
// MagicBall_Start, BattleFx_Finish.
S36_EXPORT void __cdecl MagicBall_Task(void) {
    static constexpr std::uint32_t kPhases[2] = {bof3::addr::MagicBall_Start, bof3::addr::BattleFx_Finish};
    const unsigned phase = Sc()[1];
    if (phase >= 2) PastTable("MagicBall_Task", phase, 2);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x4F1E70: the owner's direction and point; +0xB 0, +1 on; the core
// (kind 1, 0x51, +1 0) and eight orbs (the same, +1 1, +4 the core's slot, +9
// i + 1, +0xB i), each with +0x80 this task and counted in +0xB; sound 0x100.
S36_EXPORT void __cdecl MagicBall_Start(void) {
    Sc()[8] = Owner()[8];
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(Owner())->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(Owner())->z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(Owner())->height);
    Sc()[0xB] = 0;
    Inc(Sc()[1]);
    const unsigned core = NewTask(0x51);
    {
        unsigned char* const s = Sc();
        unsigned char* const child = TaskSlot(core);
        records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
        tchild->owner = I32(Key(s));
        child[1] = 0;
        Inc(s[0xB]);
    }
    for (unsigned i = 0; i < 8; ++i) {
        const unsigned slot = NewTask(0x51);
        unsigned char* const s = Sc();
        unsigned char* const child = TaskSlot(slot);
        records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
        tchild->owner = I32(Key(s));
        child[1] = 1;
        child[4] = static_cast<unsigned char>(core);
        child[9] = static_cast<unsigned char>(i + 1);
        child[0xB] = static_cast<unsigned char>(i);
        Inc(s[0xB]);
    }
    MH_CALL(Sound_PlayById)(0x100);
}

// original 0x4F1F70: the children's kind-1 task, a jmp through
// MagicBallChild_Kinds (two entries: the core, an orb) by +1, unchecked.
S36_EXPORT void __cdecl MagicBallChild_Task(void) {
    static constexpr std::uint32_t kKinds[2] = {bof3::addr::MagicBallCore_Run, bof3::addr::MagicBallOrb_Run};
    const unsigned phase = Sc()[1];
    if (phase >= 2) PastTable("MagicBallChild_Task", phase, 2);
    magic_harness::Phase(kKinds[phase])();
}

// original 0x4F1F90: a call through MagicBallCore_Steps (five entries: _Start,
// _Fly, _Swell, _Shrink, MAGIC060's MagicFx_CountDown2Release) by +2; then
// while +0 is set, by +2 (read again): 2 - eight sparks, the disc, the ring
// and both swell rings; 3 - the two swell rings; 4 - eight sparks; any other -
// five sparks, the disc and the ring. The screen point is updated first.
S36_EXPORT void __cdecl MagicBallCore_Run(void) {
    static constexpr std::uint32_t kSteps[5] = {bof3::addr::MagicBallCore_Start, bof3::addr::MagicBallCore_Fly,
                                                bof3::addr::MagicBallCore_Swell, bof3::addr::MagicBallCore_Shrink,
                                                bof3::addr::MagicFx_CountDown2Release};
    const unsigned phase = Sc()[2];
    if (phase >= 5) PastTable("MagicBallCore_Run", phase, 5);
    magic_harness::Phase(kSteps[phase])();
    const unsigned char* const s = Sc();
    if (s[0] == 0) return;
    switch (s[2]) {
    case 2:
        MH_CALL(BattleActor_UpdateScreenXY)();
        FiveSparks(7);
        ThreeSparks(0xF);
        Call0(bof3::addr::MagicBall_DrawDisc);
        Call0(bof3::addr::MagicBall_DrawRing);
        Call0(bof3::addr::MagicBall_DrawRingOut);
        Call0(bof3::addr::MagicBall_DrawRingIn);
        break;
    case 3:
        MH_CALL(BattleActor_UpdateScreenXY)();
        Call0(bof3::addr::MagicBall_DrawRingOut);
        Call0(bof3::addr::MagicBall_DrawRingIn);
        break;
    case 4:
        MH_CALL(BattleActor_UpdateScreenXY)();
        FiveSparks(3);
        ThreeSparks(7);
        break;
    default:
        MH_CALL(BattleActor_UpdateScreenXY)();
        FiveSparks(3);
        Call0(bof3::addr::MagicBall_DrawDisc);
        Call0(bof3::addr::MagicBall_DrawRing);
        break;
    }
}

// original 0x4F2220: the owner's point, 0x800000 higher; the heading +0xC the
// angle to the source sprite (0x904B4C); +0xB, +9, +0xA 0; +2 on.
S36_EXPORT void __cdecl MagicBallCore_Start(void) {
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(Owner())->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(Owner())->z);
    rv::Store(rv::Current<records::BattleTask>()->height, I32(U32(Owner() + 0x3C) + 0x800000u));
    const int angle = AngleTo(Pointer(::bof3::Img(BattleOdds_ActorObject_addr)));
    rv::Current<records::BattleTask>()->step_x = angle;
    Sc()[0xB] = 0;
    Sc()[9] = 0;
    Sc()[0xA] = 0;
    Inc(Sc()[2]);
}

// original 0x4F22D0: +9 up by 4 below 0x10; a step (0x40) toward the source
// sprite's point 0x1000000 higher; the old heading to +0x10, the new one the
// angle to the source; at the source (MagicFx_NearSprite, 0x8000), or once
// the heading has turned by more than 0x600 and less than 0xA00: the hit.
S36_EXPORT void __cdecl MagicBallCore_Fly(void) {
    {
        unsigned char* const s = Sc();
        if (s[9] < 0x10) AddB(s[9], 4);
    }
    const unsigned char* const src = Pointer(::bof3::Img(BattleOdds_ActorObject_addr));
    StepTowardSource(src, 0x1000000u, 0x40);
    {
        unsigned char* const s = Sc();
        records::BattleTask* const ts = rv::As<records::BattleTask>(s);
        ts->step_z = ts->step_x;
    }
    const int angle = AngleTo(src);
    rv::Current<records::BattleTask>()->step_x = angle;
    if (MH_CALL(MagicFx_NearSprite)(src, 0x8000) != 0) {
        CoreHit();
        return;
    }
    if (TurnedPast()) CoreHit();
}

// original 0x4F2440: +0xB up by 2; +0xA up by 2 below 0x10; +9 down, at 0 +2
// on.
S36_EXPORT void __cdecl MagicBallCore_Swell(void) {
    unsigned char* const s = Sc();
    AddB(s[0xB], 2);
    if (s[0xA] < 0x10) AddB(s[0xA], 2);
    Dec(s[9]);
    if (s[9] == 0) Inc(s[2]);
}

// original 0x4F2480: +0xB up by 4, +0xA down by 2; at 0 sound 0x102, +9 0x10,
// +2 on.
S36_EXPORT void __cdecl MagicBallCore_Shrink(void) {
    unsigned char* const s = Sc();
    AddB(s[0xB], 4);
    AddB(s[0xA], 0xFE);
    if (s[0xA] != 0) return;
    MH_CALL(Sound_PlayById)(0x102);
    Sc()[9] = 0x10;
    Inc(Sc()[2]);
}

// original 0x4F24D0: a call through MagicBallOrb_Steps (three entries:
// _Start, _Fly, MAGIC168's ShadowSeeker_Fade) by +2; then while +0 AND +2
// (the bytes, bitwise) is not 0: the screen point and the orb's disc.
S36_EXPORT void __cdecl MagicBallOrb_Run(void) {
    static constexpr std::uint32_t kSteps[3] = {bof3::addr::MagicBallOrb_Start, bof3::addr::MagicBallOrb_Fly,
                                                bof3::addr::ShadowSeeker_Fade};
    const unsigned phase = Sc()[2];
    if (phase >= 3) PastTable("MagicBallOrb_Run", phase, 3);
    magic_harness::Phase(kSteps[phase])();
    const unsigned char* const s = Sc();
    if ((s[2] & s[0]) == 0) return;
    MH_CALL(BattleActor_UpdateScreenXY)();
    Call0(bof3::addr::MagicBallOrb_DrawDisc);
}

// original 0x4F2500: +9 down; at 0 the owner's point, 0x800000 higher, the
// heading the angle to the source sprite, +9 0, +2 on.
S36_EXPORT void __cdecl MagicBallOrb_Start(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(Owner())->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(Owner())->z);
    rv::Store(rv::Current<records::BattleTask>()->height, I32(U32(Owner() + 0x3C) + 0x800000u));
    const int angle = AngleTo(Pointer(::bof3::Img(BattleOdds_ActorObject_addr)));
    rv::Current<records::BattleTask>()->step_x = angle;
    Sc()[9] = 0;
    Inc(Sc()[2]);
}

// original 0x4F25B0: the core's flight for an orb (+9 up by 4 below 0x10, the
// step, the heading); +2 on once the core's task (the slot +4, unchecked) is
// at step 2.
S36_EXPORT void __cdecl MagicBallOrb_Fly(void) {
    {
        unsigned char* const s = Sc();
        if (s[9] < 0x10) AddB(s[9], 4);
    }
    const unsigned char* const src = Pointer(::bof3::Img(BattleOdds_ActorObject_addr));
    StepTowardSource(src, 0x1000000u, 0x40);
    {
        unsigned char* const s = Sc();
        records::BattleTask* const ts = rv::As<records::BattleTask>(s);
        ts->step_z = ts->step_x;
    }
    const int angle = AngleTo(src);
    rv::Current<records::BattleTask>()->step_x = angle;
    unsigned char* const s = Sc();
    if (TaskSlot(s[4])[2] == 2) Inc(s[2]);
}

// original 0x4F2680: the core's disc - sixteen triangles of radius 0x10 round
// its screen point (+0x2E, +0x30), the centre shaded +9 x 12, the rim (+9 x 2,
// +9 x 2, +9 x 14); after a draw-mode packet (tpage 0x35).
S36_EXPORT void __cdecl MagicBall_DrawDisc(void) {
    DrawModeCommit3(0x35);
    const unsigned char* const s = Sc();
    SetSW(4, 0);
    SetSW(0, 0x10);
    SetSW(6, s[9] * 12u);
    SetSW(8, s[9] * 2u);
    SetSW(0xA, s[9] * 2u);
    const int x = S16(s + 0x2E), y = S16(s + 0x30);
    SetSW(0xC, s[9] * 14u);
    static constexpr unsigned kCentre[3] = {6, 6, 6}, kRim[3] = {8, 0xA, 0xC};
    DiscG3(x, y, kCentre, kRim);
}

// original 0x4F2860: the core's ring - sixteen quads between radii 0x10 and
// 0x18, the inner edge shaded (+9, +9, +9 x 14), the outer 1.
S36_EXPORT void __cdecl MagicBall_DrawRing(void) {
    DrawModeCommit3(0x35);
    const unsigned char* const s = Sc();
    SetSW(4, 0);
    SetSW(0, 0x10);
    SetSW(2, 0x18);
    SetSW(8, s[9]);
    SetSW(0xA, s[9]);
    SetSW(0xC, s[9] * 14u);
    RingG4(S16(s + 0x2E), S16(s + 0x30), true);
}

// original 0x4F2AA0: the swell ring - between +0xB and +0xB x 2, the outer
// edge shaded (1, +0xA x 6, +0xA x 8), the inner 1.
S36_EXPORT void __cdecl MagicBall_DrawRingOut(void) {
    DrawModeCommit3(0x35);
    const unsigned char* const s = Sc();
    SetSW(4, 0);
    SetSW(0, s[0xB]);
    SetSW(2, s[0xB] * 2u);
    SetSW(8, 1);
    SetSW(0xA, s[0xA] * 6u);
    SetSW(0xC, s[0xA] * 8u);
    RingG4(S16(s + 0x2E), S16(s + 0x30), false);
}

// original 0x4F2CE0: the second swell ring - between +0xB x 2 and +0xB x 4,
// the inner edge shaded (1, +0xA x 5, +0xA x 8), the outer 1.
S36_EXPORT void __cdecl MagicBall_DrawRingIn(void) {
    DrawModeCommit3(0x35);
    const unsigned char* const s = Sc();
    SetSW(4, 0);
    SetSW(0, s[0xB] * 2u);
    SetSW(2, s[0xB] * 4u);
    SetSW(8, 1);
    SetSW(0xA, s[0xA] * 5u);
    SetSW(0xC, s[0xA] * 8u);
    RingG4(S16(s + 0x2E), S16(s + 0x30), true);
}

// original 0x4F2F20: the long spark (radius down to 0x10, the far end turning
// by Rand & 0x7F, shades from r x 3 - 0x24 and - 0x27).
S36_EXPORT void __cdecl MagicBall_DrawSpark(int angle, int mask) { Spark(angle, mask, 0x7F, -0x24, -0x27, 0x10); }

// original 0x4F3140: the short spark (radius down to 0xC, Rand & 0x3F, shades
// from r x 3 - 0x18 and - 0x1B).
S36_EXPORT void __cdecl MagicBall_DrawSparkShort(int angle, int mask) { Spark(angle, mask, 0x3F, -0x18, -0x1B, 0xC); }

// original 0x4F3360: an orb's disc - sixteen triangles of radius 0x18 round
// its screen point, the centre shaded (MagicBallOrb_Shades[+0xB] x +9, +9,
// the pair's second x +9; the index unchecked), the rim 1.
S36_EXPORT void __cdecl MagicBallOrb_DrawDisc(void) {
    DrawModeCommit3(0x35);
    const unsigned char* const s = Sc();
    SetSW(4, 0);
    SetSW(0, 0x18);
    SetSW(6, static_cast<unsigned>(s[0xB]) * s[9]);
    const int x = S16(s + 0x2E), y = S16(s + 0x30);
    SetSW(8, static_cast<unsigned>(Mem(::bof3::Img(MagicBallOrb_Shades_addr) + s[0xB] * 2u)[0]) * s[9]);
    SetSW(0xA, s[9]);
    SetSW(0xC, static_cast<unsigned>(Mem(::bof3::Img(MagicBallOrb_Shades_addr) + 1 + s[0xB] * 2u)[0]) * s[9]);
    static constexpr unsigned kCentre[3] = {8, 0xA, 0xC}, kRim[3] = {0xFF, 0xFF, 0xFF};
    DiscG3(x, y, kCentre, kRim);
}

void MagicBall_Inject() {
    BOF3_INJECT(MagicBall_Task);
    BOF3_INJECT(MagicBall_Start);
    BOF3_INJECT(MagicBallChild_Task);
    BOF3_INJECT(MagicBallCore_Run);
    BOF3_INJECT(MagicBallCore_Start);
    BOF3_INJECT(MagicBallCore_Fly);
    BOF3_INJECT(MagicBallCore_Swell);
    BOF3_INJECT(MagicBallCore_Shrink);
    BOF3_INJECT(MagicBallOrb_Run);
    BOF3_INJECT(MagicBallOrb_Start);
    BOF3_INJECT(MagicBallOrb_Fly);
    BOF3_INJECT(MagicBall_DrawDisc);
    BOF3_INJECT(MagicBall_DrawRing);
    BOF3_INJECT(MagicBall_DrawRingOut);
    BOF3_INJECT(MagicBall_DrawRingIn);
    BOF3_INJECT(MagicBall_DrawSpark);
    BOF3_INJECT(MagicBall_DrawSparkShort);
    BOF3_INJECT(MagicBallOrb_DrawDisc);
}

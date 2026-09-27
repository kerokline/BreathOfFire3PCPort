// Three spell overlays compiled into the exe, round nine group S36
// (docs/magic_s36.md): the PSX's MAGIC172, MAGIC173 and MAGIC218.EMI,
// Magic_Rows rows 23, 34 and 142. Read one id down (docs/cut-content.md
// section 2) the sibling labels them Magic Ball, Intimidate and Aura Breath;
// the names below use those labels as hypotheses, and say what the code does.
//
//   - MAGIC172 0x4F1E40..0x4F3535: a core ball (kind 1, 0x51) that flies from
//     above the caster to the source sprite and bursts there - screen-space
//     discs, rings and jittered spark lines - and eight orbs (the same kind,
//     phase 1) that follow it and fade out;
//   - MAGIC173 0x4F3540..0x4F4A53: two trails (kind 1, 0x18) that fly from
//     beside the caster to the source sprite, each keeping 32 screen points
//     (0x6B2858) and drawing a ribbon of gouraud quads through them, and at
//     the hit a burst (the same kind, phase 1) that plays a sprite script and
//     draws a flat disc;
//   - MAGIC218 0x4F52F0..0x4F59CF: one dome (kind 1, 0x67) of 8 x 32 textured
//     quads round the caster that grows, flags each enemy it reaches once
//     (0x6B4A58) and fades.
//
// Every call goes through the harness (MH_CALL / MH_AT / Phase), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies.
// No divergence: each is a faithful replacement, except that a phase past a
// task's table aborts where the original would call through whatever follows
// it (docs/magic_fx_reached.md section 3, the precedent).
#include "game/magic_s36.h"

#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = magic_harness::at;
using magic_harness::Mem;
using magic_harness::Pointer;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// The scratch the overlays keep their working values in: DamageScratch's
// sixteen bytes (0x903850..0x90385F, Scratch_Swap at +0xC) and the four
// SVECTORs of Prim_VertexScratch (0x9037A0..0x9037BF). Both are read again
// after every call, as the originals read them.
constexpr std::uint32_t kS = 0x903850;
constexpr std::uint32_t kV = 0x9037A0;

// The cells the overlays read beyond the harness's names.
constexpr std::uint32_t kFrameSet = 0x9039D8;        // the sprite frame-offset table pointer (sprite_pose.h)
constexpr std::uint32_t kFrameSetBattle = 0x8B3580;
constexpr std::uint32_t kFrameSetEffect = 0x8E3580;
constexpr std::uint32_t kOrbShades = 0x65C1A8;       // MagicBallOrb_Shades: 10 byte pairs, by the orb's +0xB
constexpr std::uint32_t kTrailPoints = 0x6B2858;     // IntimidateTrail_Points: 32 (x, y) words a trail, by its +4
constexpr std::uint32_t kStruck = 0x6B4A58;          // AuraBreath_Struck: a byte an enemy, 0xFF once flagged
constexpr std::uint32_t kDomeV = 0x65C21C;           // AuraBreathDome_V: 8 bytes, a texture v a band
constexpr std::uint32_t kDomeHeight = 0x65C224;      // AuraBreathDome_Height: 8 bytes, a texture height a band

unsigned char* Sc() { return Sprite_Current; }
unsigned char* Owner() { return Pointer(at::kOwner); }
unsigned char TargetByte() { return Mem(at::kTarget)[0]; }
unsigned ActorIndex() { return static_cast<unsigned>(Long(Mem(at::kActor))) & 0xFF; }

std::uint16_t SW(unsigned k) { return Word(Mem(kS + k)); }
short SS(unsigned k) { return static_cast<short>(SW(k)); }
unsigned char SB(unsigned k) { return Mem(kS + k)[0]; }
void SetSW(unsigned k, unsigned v) { SetWord(Mem(kS + k), v & 0xFFFF); }
std::int32_t SD(unsigned k) { return Long(Mem(kS + k)); }

std::uint16_t VW(unsigned k) { return Word(Mem(kV + k)); }
void SetVW(unsigned k, unsigned v) { SetWord(Mem(kV + k), v & 0xFFFF); }
const short* VP(unsigned k) { return reinterpret_cast<const short*>(Mem(kV + k)); }

short S16(const unsigned char* at) { return static_cast<short>(Word(at)); }
void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }
void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }
void AddB(unsigned char& b, unsigned v) { b = static_cast<unsigned char>(b + v); }
std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
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

unsigned char* TaskSlot(unsigned slot) { return Mem(at::kTasks + slot * at::kTaskStride); }
// The originals index the enemy records by the battle index - 3, unchecked.
unsigned char* EnemyRecord(unsigned battle_index) {
    return Mem(at::kEnemies + static_cast<std::uint32_t>(static_cast<int>(battle_index) - 3) * at::kEnemyStride);
}

std::uint32_t RandCall() { return static_cast<std::uint32_t>(MH_CALL(Rand)()); }
unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }

// This group's functions called by address, as the originals call them: in
// the game the jmp Inject put there (or Capcom's code under
// BOF3X_ORIGINAL), in the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();
using Fn2 = void (__cdecl*)(int, int);
using ReachFn = int (__cdecl*)(unsigned char*);
void Call0(std::uint32_t address) { MH_AT(Fn0, address)(); }
void Call2(std::uint32_t address, int a, int b) { MH_AT(Fn2, address)(a, b); }

// Capcom's, unnamed, in no group: turns the dx / dz pair +0xC / +0x10 of the
// task it is given by its direction byte +8 (docs/magic_s22.md).
constexpr std::uint32_t kTurnOffset = 0x446770;
using TaskFn = void (__cdecl*)(unsigned char*);
void Turn(unsigned char* task) { MH_AT(TaskFn, kTurnOffset)(task); }

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

// Gte_RotTransPers4 with the depth and flag pointers the original passes (ours
// reads the first).
using Rtp4Fn = long (__cdecl*)(const short*, const short*, const short*, const short*, unsigned char*, unsigned char*,
                               unsigned char*, unsigned char*, long*, long*);
#define S36_AS(type, name) ::magic_harness::Call(reinterpret_cast<type>(reinterpret_cast<void*>(&::name)))
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

// Row 26 of Gfx_ClutStrip back from its source (0x812980 / 0x80E980).
void RestoreRow26() {
    for (unsigned k = 0x1A00; k < 0x1B00; ++k) Gfx_ClutStrip[k] = Gfx_ClutStripSource[k];
    Gfx_ClutStripDirty = 1;
}
// The same, each word with its semi-transparency bit set.
void RestoreRow26Stp() {
    for (unsigned k = 0x1A00; k < 0x1B00; ++k) Gfx_ClutStrip[k] = static_cast<unsigned short>(Gfx_ClutStripSource[k] | 0x8000);
    Gfx_ClutStripDirty = 1;
}

// The source sprite's point as MagicFx_StepTowardPoint takes it: (x sar 9) -
// 0x4000, (z sar 9) - 0x4000, (y + lift) sar 0x11; the fourth word the
// original pushes is its own uninitialised stack, which the callee never reads.
void StepTowardSource(const unsigned char* src, std::uint32_t lift, int speed) {
    const std::uint32_t x = static_cast<std::uint32_t>((Long(src + 0x34) >> 9) - 0x4000);
    const std::uint32_t z = static_cast<std::uint32_t>((Long(src + 0x38) >> 9) - 0x4000);
    const std::uint32_t y = static_cast<std::uint32_t>(I32(U32(src + 0x3C) + lift) >> 0x11);
    MH_CALL(MagicFx_StepTowardPoint)(x, z, y, 0, speed);
}

// Sprite_Current's +0x10 = (+0x10 & 0xFFF) - (+0xC & 0xFFF), then its
// absolute value (`not / inc`): how far the heading turned this frame. True
// when it is above 0x600 and below 0xA00 (it went past the target).
bool TurnedPast() {
    {
        unsigned char* const s = Sc();
        SetLong(s + 0x10, I32((U32(s + 0x10) & 0xFFF) - (U32(s + 0xC) & 0xFFF)));
    }
    {
        unsigned char* const s = Sc();
        const std::int32_t v = Long(s + 0x10);
        if (v < 0) SetLong(s + 0x10, I32(0u - static_cast<std::uint32_t>(v)));
    }
    const std::int32_t v = Long(Sc() + 0x10);
    return v > 0x600 && v < 0xA00;
}

// One point of a trail: IntimidateTrail_Points + ((trail << 5) + k) x 4, the
// x word and the y word; neither index is checked by the originals.
unsigned char* TrailPoint(unsigned trail, unsigned k) { return Mem(kTrailPoints + ((trail << 5) + k) * 4); }

}  // namespace

#define S36_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// MAGIC172 (row 23, Magic Ball read one id down)

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
    SetLong(Sc() + 0x34, Long(Owner() + 0x34));
    SetLong(Sc() + 0x38, Long(Owner() + 0x38));
    SetLong(Sc() + 0x3C, Long(Owner() + 0x3C));
    Sc()[0xB] = 0;
    Inc(Sc()[1]);
    const unsigned core = NewTask(0x51);
    {
        unsigned char* const s = Sc();
        unsigned char* const child = TaskSlot(core);
        SetLong(child + 0x80, I32(Key(s)));
        child[1] = 0;
        Inc(s[0xB]);
    }
    for (unsigned i = 0; i < 8; ++i) {
        const unsigned slot = NewTask(0x51);
        unsigned char* const s = Sc();
        unsigned char* const child = TaskSlot(slot);
        SetLong(child + 0x80, I32(Key(s)));
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

}  // namespace

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
    SetLong(Sc() + 0x34, Long(Owner() + 0x34));
    SetLong(Sc() + 0x38, Long(Owner() + 0x38));
    SetLong(Sc() + 0x3C, I32(U32(Owner() + 0x3C) + 0x800000u));
    const int angle = AngleTo(Pointer(at::kSource));
    SetLong(Sc() + 0xC, angle);
    Sc()[0xB] = 0;
    Sc()[9] = 0;
    Sc()[0xA] = 0;
    Inc(Sc()[2]);
}

namespace {

// Target flags 0x10, sound 0x101, +9 0x10, +2 on: the core's hit.
void CoreHit() {
    MH_CALL(Battle_SetTargetFlags)(TargetByte(), 0x10);
    MH_CALL(Sound_PlayById)(0x101);
    Sc()[9] = 0x10;
    Inc(Sc()[2]);
}

}  // namespace

// original 0x4F22D0: +9 up by 4 below 0x10; a step (0x40) toward the source
// sprite's point 0x1000000 higher; the old heading to +0x10, the new one the
// angle to the source; at the source (MagicFx_NearSprite, 0x8000), or once
// the heading has turned by more than 0x600 and less than 0xA00: the hit.
S36_EXPORT void __cdecl MagicBallCore_Fly(void) {
    {
        unsigned char* const s = Sc();
        if (s[9] < 0x10) AddB(s[9], 4);
    }
    const unsigned char* const src = Pointer(at::kSource);
    StepTowardSource(src, 0x1000000u, 0x40);
    {
        unsigned char* const s = Sc();
        SetLong(s + 0x10, Long(s + 0xC));
    }
    const int angle = AngleTo(src);
    SetLong(Sc() + 0xC, angle);
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
    SetLong(Sc() + 0x34, Long(Owner() + 0x34));
    SetLong(Sc() + 0x38, Long(Owner() + 0x38));
    SetLong(Sc() + 0x3C, I32(U32(Owner() + 0x3C) + 0x800000u));
    const int angle = AngleTo(Pointer(at::kSource));
    SetLong(Sc() + 0xC, angle);
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
    const unsigned char* const src = Pointer(at::kSource);
    StepTowardSource(src, 0x1000000u, 0x40);
    {
        unsigned char* const s = Sc();
        SetLong(s + 0x10, Long(s + 0xC));
    }
    const int angle = AngleTo(src);
    SetLong(Sc() + 0xC, angle);
    unsigned char* const s = Sc();
    if (TaskSlot(s[4])[2] == 2) Inc(s[2]);
}

namespace {

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

}  // namespace

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

namespace {

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
    SetSW(8, static_cast<unsigned>(Mem(kOrbShades + s[0xB] * 2u)[0]) * s[9]);
    SetSW(0xA, s[9]);
    SetSW(0xC, static_cast<unsigned>(Mem(kOrbShades + 1 + s[0xB] * 2u)[0]) * s[9]);
    static constexpr unsigned kCentre[3] = {8, 0xA, 0xC}, kRim[3] = {0xFF, 0xFF, 0xFF};
    DiscG3(x, y, kCentre, kRim);
}

// ===========================================================================
// MAGIC173 (row 34, Intimidate read one id down)

// original 0x4F3540: the kind-2 task. A two-entry stack table by +1:
// Intimidate_Start, BattleFx_Finish.
S36_EXPORT void __cdecl Intimidate_Task(void) {
    static constexpr std::uint32_t kPhases[2] = {bof3::addr::Intimidate_Start, bof3::addr::BattleFx_Finish};
    const unsigned phase = Sc()[1];
    if (phase >= 2) PastTable("Intimidate_Task", phase, 2);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x4F3570: the owner's direction and point; +0xB 0, +1 on; two
// trails (kind 1, 0x18, +1 0, +4 0 and 1), each with +0x80 this task and
// counted in +0xB; row 26 of the CLUT strip back; sound 0x100.
S36_EXPORT void __cdecl Intimidate_Start(void) {
    Sc()[8] = Owner()[8];
    SetLong(Sc() + 0x34, Long(Owner() + 0x34));
    SetLong(Sc() + 0x38, Long(Owner() + 0x38));
    SetLong(Sc() + 0x3C, Long(Owner() + 0x3C));
    Sc()[0xB] = 0;
    Inc(Sc()[1]);
    for (unsigned i = 0; i < 2; ++i) {
        const unsigned slot = NewTask(0x18);
        unsigned char* const s = Sc();
        unsigned char* const child = TaskSlot(slot);
        SetLong(child + 0x80, I32(Key(s)));
        child[1] = 0;
        child[4] = static_cast<unsigned char>(i);
        Inc(s[0xB]);
    }
    RestoreRow26();
    MH_CALL(Sound_PlayById)(0x100);
}

// original 0x4F3640: the children's kind-1 task, a jmp through
// IntimidateChild_Kinds (two entries: a trail, the burst) by +1, unchecked.
S36_EXPORT void __cdecl IntimidateChild_Task(void) {
    static constexpr std::uint32_t kKinds[2] = {bof3::addr::IntimidateTrail_Run, bof3::addr::IntimidateBurst_Run};
    const unsigned phase = Sc()[1];
    if (phase >= 2) PastTable("IntimidateChild_Task", phase, 2);
    magic_harness::Phase(kKinds[phase])();
}

// original 0x4F3660: a call through IntimidateTrail_Steps (four entries:
// _Start, _Fly, _Grow, _Fade) by +2; then while +0 and +2 are set, trail 1
// (+4 not 0) draws the thin ribbon, trail 0 the wide one.
S36_EXPORT void __cdecl IntimidateTrail_Run(void) {
    static constexpr std::uint32_t kSteps[4] = {bof3::addr::IntimidateTrail_Start, bof3::addr::IntimidateTrail_Fly,
                                                bof3::addr::IntimidateTrail_Grow, bof3::addr::IntimidateTrail_Fade};
    const unsigned phase = Sc()[2];
    if (phase >= 4) PastTable("IntimidateTrail_Run", phase, 4);
    magic_harness::Phase(kSteps[phase])();
    const unsigned char* const s = Sc();
    if (s[0] == 0 || s[2] == 0) return;
    Call0(s[4] != 0 ? bof3::addr::IntimidateTrail_DrawThin : bof3::addr::IntimidateTrail_DrawWide);
}

// original 0x4F36B0: the owner's direction; an offset by the acting actor (a
// party member (0x4000, 0, 0xC00000); an enemy whose record +0x8C is 0x61
// (0x38000, 0, 0), 0x6C (0x20000 or, facing bit 1 clear, 0x30000, 0,
// 0x800000), any other (0x10000, 0, 0x800000)) turned by the direction and
// added to the owner's point; the heading the angle to the source sprite,
// +0x10 0; the screen point, its trail's first two points; +0xB 0, +9 0x10,
// +0xA 1, +2 on.
S36_EXPORT void __cdecl IntimidateTrail_Start(void) {
    Sc()[8] = Owner()[8];
    const unsigned char* const src = Pointer(at::kSource);
    std::uint32_t dx, dy;
    const unsigned actor = ActorIndex();
    if (actor < 3) {
        dx = 0x4000;
        dy = 0xC00000;
    } else {
        const unsigned char kind = EnemyRecord(actor)[0x8C];
        dy = 0x800000;
        if (kind == 0x61) {
            dx = 0x38000;
            dy = 0;
        } else if (kind == 0x6C) {
            dx = Sc()[8] & 2 ? 0x20000u : 0x30000u;
        } else {
            dx = 0x10000;
        }
    }
    SetLong(Sc() + 0xC, I32(dx));
    SetLong(Sc() + 0x10, 0);
    SetLong(Sc() + 0x14, I32(dy));
    Turn(Sc());
    for (unsigned k = 0; k < 3; ++k) {
        unsigned char* const s = Sc();
        SetLong(s + 0x34 + 4 * k, I32(U32(Owner() + 0x34 + 4 * k) + U32(s + 0xC + 4 * k)));
    }
    const int angle = AngleTo(src);
    SetLong(Sc() + 0xC, angle);
    SetLong(Sc() + 0x10, 0);
    MH_CALL(BattleActor_UpdateScreenXY)();
    unsigned char* const s = Sc();
    SetWord(TrailPoint(s[4], 0), Word(s + 0x2E));
    SetWord(TrailPoint(s[4], 0) + 2, Word(s + 0x30));
    SetWord(TrailPoint(s[4], 1), Word(s + 0x2E));
    SetWord(TrailPoint(s[4], 1) + 2, Word(s + 0x30));
    s[0xB] = 0;
    Sc()[9] = 0x10;
    Sc()[0xA] = 1;
    Inc(Sc()[2]);
}

// original 0x4F38B0: +0xA up below 8; a step (0xC0) toward the source sprite's
// point 0x800000 higher; the heading as the core's; at the source, or turned
// past it, the point set on the source's x / z and +2 on. Then the screen
// point, pushed onto the trail; trail 0 flags the target 0x10 every frame but
// the first step's; on reaching step 2 the burst (kind 1, 0x18, +1 1, the
// owner's child, at this point), counted in the owner's +0xB.
S36_EXPORT void __cdecl IntimidateTrail_Fly(void) {
    {
        unsigned char* const s = Sc();
        if (s[0xA] < 8) Inc(s[0xA]);
    }
    const unsigned char* const src = Pointer(at::kSource);
    StepTowardSource(src, 0x800000u, 0xC0);
    {
        unsigned char* const s = Sc();
        SetLong(s + 0x10, Long(s + 0xC));
    }
    const int angle = AngleTo(src);
    SetLong(Sc() + 0xC, angle);
    bool arrived;
    if (MH_CALL(MagicFx_NearSprite)(src, 0x8000) != 0) {
        arrived = true;
    } else {
        arrived = TurnedPast();
    }
    if (arrived) {
        SetLong(Sc() + 0x34, Long(src + 0x34));
        SetLong(Sc() + 0x38, Long(src + 0x38));
        Inc(Sc()[2]);
    }
    MH_CALL(BattleActor_UpdateScreenXY)();
    Call0(bof3::addr::IntimidateTrail_Push);
    {
        const unsigned char* const s = Sc();
        if (s[2] != 1 && s[4] == 0) MH_CALL(Battle_SetTargetFlags)(TargetByte(), 0x10);
    }
    if (Sc()[2] != 2) return;
    const unsigned slot = NewTask(0x18);
    const unsigned char* const s = Sc();
    unsigned char* const owner = Owner();
    unsigned char* const child = TaskSlot(slot);
    SetLong(child + 0x80, I32(Key(owner)));
    child[1] = 1;
    SetLong(child + 0x34, Long(s + 0x34));
    SetLong(child + 0x38, Long(s + 0x38));
    SetLong(child + 0x3C, Long(s + 0x3C));
    Inc(owner[0xB]);
}

// original 0x4F3A80: below +0xA 8, +0xB and +0xA up, the screen point pushed
// onto the trail; once +0xA is 8 (at entry, or after the push) +9 8 and +2 on.
S36_EXPORT void __cdecl IntimidateTrail_Grow(void) {
    {
        unsigned char* const s = Sc();
        if (s[0xA] >= 8) {
            if (s[0xA] == 8) {
                s[9] = 8;
                Inc(Sc()[2]);
            }
            return;
        }
        Inc(s[0xB]);
        Inc(Sc()[0xA]);
    }
    MH_CALL(BattleActor_UpdateScreenXY)();
    Call0(bof3::addr::IntimidateTrail_Push);
    unsigned char* const s = Sc();
    if (s[0xA] != 8) return;
    s[9] = 8;
    Inc(Sc()[2]);
}

// original 0x4F3AC0: a gap pushed onto the trail; +9 down, at 0 the owner's
// +0xB down and the task freed.
S36_EXPORT void __cdecl IntimidateTrail_Fade(void) {
    Call0(bof3::addr::IntimidateTrail_PushGap);
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

namespace {

// The trail's points +0xB .. +0xA - 1 moved one on (from the end), then
// (x, y) at +0xB.
void TrailShift(unsigned x, unsigned y) {
    const unsigned char* const s = Sc();
    int k = static_cast<int>(s[0xA]) - 1;
    while (static_cast<int>(s[0xB]) <= k) {
        SetWord(TrailPoint(s[4], static_cast<unsigned>(k) + 1), Word(TrailPoint(s[4], static_cast<unsigned>(k))));
        SetWord(TrailPoint(s[4], static_cast<unsigned>(k) + 1) + 2, Word(TrailPoint(s[4], static_cast<unsigned>(k)) + 2));
        --k;
    }
    SetWord(TrailPoint(s[4], s[0xB]), x);
    SetWord(TrailPoint(s[4], s[0xB]) + 2, y);
}

}  // namespace

// original 0x4F4640: the screen point (+0x2E, +0x30) pushed onto the trail at
// +0xB, the points to +0xA moved on.
S36_EXPORT void __cdecl IntimidateTrail_Push(void) {
    const unsigned char* const s = Sc();
    TrailShift(Word(s + 0x2E), Word(s + 0x30));
}

// original 0x4F46D0: a gap (-1, -1) pushed onto the trail the same way.
S36_EXPORT void __cdecl IntimidateTrail_PushGap(void) { TrailShift(0xFFFF, 0xFFFF); }

namespace {

// The ribbon's head: from the trail's point +0xB past any gap (x -1, the scan
// unbounded), the first point to (0x90385C, 0x90385E), the next to (0x903858,
// 0x90385A), the angle between them (Math_Ratan2(dx, dy)) to 0x903852; then,
// Sprite_Current read again, the first point to (0x903858, 0x90385A).
// Answers the index after it.
unsigned RibbonHead() {
    const unsigned char* s = Sc();
    unsigned k = s[0xB];
    for (const unsigned char* q = TrailPoint(s[4], k); Word(q) == 0xFFFF; q += 4) ++k;
    const short x0 = S16(TrailPoint(s[4], k)), y0 = S16(TrailPoint(s[4], k) + 2);
    SetSW(0xC, static_cast<unsigned short>(x0));
    SetSW(0xE, static_cast<unsigned short>(y0));
    const short x1 = S16(TrailPoint(s[4], k + 1)), y1 = S16(TrailPoint(s[4], k + 1) + 2);
    SetSW(8, static_cast<unsigned short>(x1));
    SetSW(0xA, static_cast<unsigned short>(y1));
    SetSW(2, static_cast<unsigned>(Ratan2(x1 - x0, y1 - y0)));
    s = Sc();
    SetSW(8, Word(TrailPoint(s[4], k)));
    SetSW(0xA, Word(TrailPoint(s[4], k) + 2));
    return k + 1;
}

// One side of a quad: the point at the scratch pair (x word, y word) offset
// by the radius word 0x903850 at (0x903852 + turn) & 0xFFF, into the
// floats at p + at.
void Offset(unsigned char* p, unsigned at, unsigned xw, unsigned yw, unsigned turn) {
    const unsigned a = (SW(2) + turn) & 0xFFF;
    SetSW(4, a);
    int v = MH_CALL(Math_Sin)(static_cast<short>(a));
    PutFloat(p + at, Add(Mul12(v, SS(0)), SS(xw)));
    v = MH_CALL(Math_Cos)(SS(4));
    PutFloat(p + at + 4, Add(Mul12(v, SS(0)), SS(yw)));
}

// The next point of the ribbon: the last one to (0x90385C, 0x90385E), point k
// to (0x903858, 0x90385A); a gouraud quad at Gfx_PacketNext.
unsigned char* RibbonStep(unsigned k) {
    SetSW(0xC, SW(8));
    SetSW(0xE, SW(0xA));
    const unsigned char* const s = Sc();
    unsigned char* const p = Gfx_PacketNext;
    SetSW(8, Word(TrailPoint(s[4], k)));
    SetSW(0xA, Word(TrailPoint(s[4], k) + 2));
    MH_CALL(Gpu_SetPolyG4)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    return p;
}
// The angle from the last point to this one, to 0x903852.
void RibbonTurn() { SetSW(2, static_cast<unsigned>(Ratan2(SS(8) - SS(0xC), SS(0xA) - SS(0xE)))); }
// The thin ribbon's shades: `first` to 0x903856, its low byte red and green
// of corners 0 and 2 (blue 1); then `second`, the same for corners 1 and 3.
void ThinShade(unsigned char* p, unsigned first, unsigned second) {
    SetSW(6, first);
    p[4] = static_cast<unsigned char>(first);
    p[5] = SB(6);
    p[6] = 1;
    p[0x24] = SB(6);
    p[0x25] = SB(6);
    p[0x26] = 1;
    SetSW(6, second);
    p[0x14] = static_cast<unsigned char>(second);
    p[0x15] = SB(6);
    p[0x16] = 1;
    p[0x34] = SB(6);
    p[0x35] = SB(6);
    p[0x36] = 1;
}
// The wide ribbon's: corners 0 and 1 (the offset edge) 1; `first` to
// 0x903856 and its low byte into corner 2, `second` into corner 3.
void WideShade(unsigned char* p, unsigned first, unsigned second) {
    SetSW(6, first);
    for (unsigned c = 0; c < 3; ++c) p[4 + c] = 1;
    for (unsigned c = 0; c < 3; ++c) p[0x24 + c] = SB(6);
    SetSW(6, second);
    for (unsigned c = 0; c < 3; ++c) p[0x14 + c] = 1;
    for (unsigned c = 0; c < 3; ++c) p[0x34 + c] = SB(6);
}

}  // namespace

// original 0x4F3AF0: trail 1's ribbon - a gouraud quad between each pair of
// its points from +0xB (past any gap) to +0xA, 2 either side of the line
// between them, the near ends shaded (0x6D - 12 k, 0x6D - 12 k, 1) and the far
// (0x61 - 12 k, ..., 1); between two draw-mode packets (0x35, 0x15).
S36_EXPORT void __cdecl IntimidateTrail_DrawThin(void) {
    DrawModeCommit3(0x35);
    SetSW(0, 2);
    unsigned k = RibbonHead();
    if (static_cast<int>(k) < static_cast<int>(Sc()[0xA])) {
        do {
            unsigned char* const p = RibbonStep(k);
            Offset(p, 8, 0xC, 0xE, 0x400);
            Offset(p, 0x28, 0xC, 0xE, 0xFFFFFC00u);
            RibbonTurn();
            Offset(p, 0x18, 8, 0xA, 0x400);
            Offset(p, 0x38, 8, 0xA, 0xFFFFFC00u);
            const unsigned t = 0u - 12u * k;
            ThinShade(p, t + 0x6D, t + 0x61);
            MH_CALL(Gfx_CommitPrim)(3, 0x44);
            ++k;
        } while (static_cast<int>(k) < static_cast<int>(Sc()[0xA]));
    }
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x15, 0);
    MH_CALL(Gfx_CommitPrim)(3, 0xC);
}

namespace {

// One pass of trail 0's ribbon: a quad from each point to 0xC to one side
// (`turn` 0x400 or -0x400) of the line through it, the line's own edge shaded
// (`near` - step k, `far` - step k), the offset edge 1.
void WidePass(unsigned turn, unsigned near, unsigned far, unsigned step) {
    unsigned k = RibbonHead();
    if (static_cast<int>(k) >= static_cast<int>(Sc()[0xA])) return;
    do {
        unsigned char* const p = RibbonStep(k);
        Offset(p, 8, 0xC, 0xE, turn);
        PutFloat(p + 0x28, SS(0xC));
        PutFloat(p + 0x2C, SS(0xE));
        RibbonTurn();
        Offset(p, 0x18, 8, 0xA, turn);
        PutFloat(p + 0x38, SS(8));
        PutFloat(p + 0x3C, SS(0xA));
        const unsigned t = 0u - step * k;
        WideShade(p, t + near, t + far);
        MH_CALL(Gfx_CommitPrim)(3, 0x44);
        ++k;
    } while (static_cast<int>(k) < static_cast<int>(Sc()[0xA]));
}

}  // namespace

// original 0x4F3F40: trail 0's ribbon - two passes over its points, 0xC to
// either side of the line: first the +0x400 side (shades 0x6D and 0x61 less
// 12 k), then the -0x400 side (0x91 and 0x81 less 16 k); between two
// draw-mode packets (0x35, 0x15).
S36_EXPORT void __cdecl IntimidateTrail_DrawWide(void) {
    DrawModeCommit3(0x35);
    SetSW(0, 0xC);
    WidePass(0x400, 0x6D, 0x61, 12);
    WidePass(0xFFFFFC00u, 0x91, 0x81, 16);
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x15, 0);
    MH_CALL(Gfx_CommitPrim)(3, 0xC);
}

// original 0x4F4760: the burst's kind-1 task, with the frame-offset table the
// effects' (0x8E3580) round it: a call through IntimidateBurst_Steps (four
// entries: _Start, _Rise, _Play, _Fade) by +2; then while +0 is set the
// screen point, the disc, and before step 3 the sprite's screen update.
S36_EXPORT void __cdecl IntimidateBurst_Run(void) {
    static constexpr std::uint32_t kSteps[4] = {bof3::addr::IntimidateBurst_Start, bof3::addr::IntimidateBurst_Rise,
                                                bof3::addr::IntimidateBurst_Play, bof3::addr::IntimidateBurst_Fade};
    const unsigned phase = Sc()[2];
    SetLong(Mem(kFrameSet), static_cast<std::int32_t>(kFrameSetEffect));
    if (phase >= 4) PastTable("IntimidateBurst_Run", phase, 4);
    magic_harness::Phase(kSteps[phase])();
    if (Sc()[0] != 0) {
        MH_CALL(BattleActor_UpdateScreenXY)();
        Call0(bof3::addr::IntimidateBurst_DrawDisc);
        if (Sc()[2] < 3) MH_CALL(Sprite_UpdateScreen)();
    }
    SetLong(Mem(kFrameSet), static_cast<std::int32_t>(kFrameSetBattle));
}

// original 0x4F47B0: the sprite set up (+0x25 0x1D, +0x26 0, +0x27 0xA0, +0x28
// 0, +0x24 4, +0x5C..+0x5F 0, +0x2A 0, +0x29 3, word +0x2C 0, +0x2B 1),
// animation 0; +9 0, +2 on.
S36_EXPORT void __cdecl IntimidateBurst_Start(void) {
    unsigned char* const s = Sc();
    s[0x25] = 0x1D;
    s[0x26] = 0;
    s[0x27] = 0xA0;
    s[0x28] = 0;
    s[0x24] = 4;
    s[0x5D] = 0;
    s[0x5E] = 0;
    s[0x5F] = 0;
    s[0x5C] = 0;
    s[0x2A] = 0;
    s[0x29] = 3;
    SetWord(s + 0x2C, 0);
    s[0x2B] = 1;
    MH_CALL(Sprite_SetAnimation)(0);
    Sc()[9] = 0;
    Inc(Sc()[2]);
}

// original 0x4F4850: the script ticked; +9 up by 4, at 0x10 +2 on.
S36_EXPORT void __cdecl IntimidateBurst_Rise(void) {
    MH_CALL(Sprite_ScriptTickOnce)();
    AddB(Sc()[9], 4);
    if (Sc()[9] == 0x10) Inc(Sc()[2]);
}

// original 0x4F4880: the script ticked twice; +2 on when the second tick
// reports its end.
S36_EXPORT void __cdecl IntimidateBurst_Play(void) {
    MH_CALL(Sprite_ScriptTickOnce)();
    if (MH_CALL(Sprite_ScriptTickOnce)() != 0) Inc(Sc()[2]);
}

// original 0x4F48A0: +9 down by 4; at 0 the owner's +0xB down and the task
// freed.
S36_EXPORT void __cdecl IntimidateBurst_Fade(void) {
    AddB(Sc()[9], 0xFC);
    if (Sc()[9] != 0) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4F48D0: the burst's disc - eight triangles of radius (Rand & 3) +
// 0x1C round the screen point (read again after every call), the centre
// shaded +9 x 6, the rim 1; after a draw-mode packet (0x35).
S36_EXPORT void __cdecl IntimidateBurst_DrawDisc(void) {
    DrawModeCommit3(0x35);
    const std::uint32_t r = RandCall();
    SetSW(0, (r & 3) + 0x1C);
    SetSW(6, Sc()[9] * 6u);
    for (int a = 0; a < 0x1000;) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        PutFloat(p + 8, S16(Sc() + 0x2E));
        PutFloat(p + 0xC, S16(Sc() + 0x30));
        int v = MH_CALL(Math_Sin)(a);
        PutFloat(p + 0x18, Add(Mul12(v, SS(0)), S16(Sc() + 0x2E)));
        v = MH_CALL(Math_Cos)(a);
        PutFloat(p + 0x1C, Add(Mul12(v, SS(0)), S16(Sc() + 0x30)));
        a += 0x200;
        v = MH_CALL(Math_Sin)(a);
        PutFloat(p + 0x28, Add(Mul12(v, SS(0)), S16(Sc() + 0x2E)));
        v = MH_CALL(Math_Cos)(a);
        PutFloat(p + 0x2C, Add(Mul12(v, SS(0)), S16(Sc() + 0x30)));
        p[4] = SB(6);
        p[5] = SB(6);
        p[6] = SB(6);
        for (unsigned k : {0x14u, 0x15u, 0x16u, 0x24u, 0x25u, 0x26u}) p[k] = 1;
        MH_CALL(Gfx_CommitPrim)(3, 0x34);
    }
}

// ===========================================================================
// MAGIC218 (row 142, Aura Breath read one id down)

// original 0x4F52F0: the kind-2 task. A two-entry stack table by +1:
// AuraBreath_Start, BattleFx_Finish.
S36_EXPORT void __cdecl AuraBreath_Task(void) {
    static constexpr std::uint32_t kPhases[2] = {bof3::addr::AuraBreath_Start, bof3::addr::BattleFx_Finish};
    const unsigned phase = Sc()[1];
    if (phase >= 2) PastTable("AuraBreath_Task", phase, 2);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x4F5320: AuraBreath_Struck cleared; the owner's direction and
// point; +0xB 0, +1 on; the dome (kind 1, 0x67, +1 0, +9 1) with +0x80 this
// task, counted in +0xB; row 26 of the CLUT strip back with its STP bits;
// sound 0x100.
S36_EXPORT void __cdecl AuraBreath_Start(void) {
    SetLong(Mem(kStruck), 0);
    SetLong(Mem(kStruck + 4), 0);
    Sc()[8] = Owner()[8];
    SetLong(Sc() + 0x34, Long(Owner() + 0x34));
    SetLong(Sc() + 0x38, Long(Owner() + 0x38));
    SetLong(Sc() + 0x3C, Long(Owner() + 0x3C));
    Sc()[0xB] = 0;
    Inc(Sc()[1]);
    const unsigned slot = NewTask(0x67);
    unsigned char* const s = Sc();
    unsigned char* const child = TaskSlot(slot);
    SetLong(child + 0x80, I32(Key(s)));
    child[1] = 0;
    child[9] = 1;
    Inc(s[0xB]);
    RestoreRow26Stp();
    MH_CALL(Sound_PlayById)(0x100);
}

// original 0x4F5400: the dome's kind-1 task, a jmp through
// AuraBreathDome_TaskTable (one entry) by +1, unchecked.
S36_EXPORT void __cdecl AuraBreathDome_Task(void) {
    const unsigned phase = Sc()[1];
    if (phase >= 1) PastTable("AuraBreathDome_Task", phase, 1);
    magic_harness::Phase(bof3::addr::AuraBreathDome_Run)();
}

// original 0x4F5420: a call through AuraBreathDome_Steps (three entries:
// _Wait, _Grow, _Fade) by +2; then while +0 and +2 are set the actor matrix,
// the dome, the matrix popped.
S36_EXPORT void __cdecl AuraBreathDome_Run(void) {
    static constexpr std::uint32_t kSteps[3] = {bof3::addr::AuraBreathDome_Wait, bof3::addr::AuraBreathDome_Grow,
                                                bof3::addr::AuraBreathDome_Fade};
    const unsigned phase = Sc()[2];
    if (phase >= 3) PastTable("AuraBreathDome_Run", phase, 3);
    magic_harness::Phase(kSteps[phase])();
    const unsigned char* const s = Sc();
    if (s[0] == 0 || s[2] == 0) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    Call0(bof3::addr::AuraBreathDome_Draw);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4F5460: +9 down; at 0 the owner's point, +0x5D 0x10, +9 0x10,
// +0xA 4, +2 on.
S36_EXPORT void __cdecl AuraBreathDome_Wait(void) {
    unsigned char* const s = Sc();
    Dec(s[9]);
    if (s[9] != 0) return;
    const unsigned char* const o = Owner();
    SetLong(s + 0x34, Long(o + 0x34));
    SetLong(s + 0x38, Long(o + 0x38));
    SetLong(s + 0x3C, Long(o + 0x3C));
    s[0x5D] = 0x10;
    s[9] = 0x10;
    s[0xA] = 4;
    Inc(s[2]);
}

// original 0x4F54D0: +9 up by 2, the reach word 0x903850 +9 x 16; each enemy
// (3..10) not yet struck, not out, within reach (AuraBreath_InReach of its
// record) and not the acting actor: struck (0xFF) and flagged 0x10. On odd
// frames +0xA up below 0x20; at +9 0x60 +2 on.
S36_EXPORT void __cdecl AuraBreathDome_Grow(void) {
    AddB(Sc()[9], 2);
    SetSW(0, static_cast<unsigned>(Sc()[9]) << 4);
    for (unsigned i = 0; i < 8; ++i) {
        unsigned char* const struck = Mem(kStruck + i);
        if (struck[0] != 0) continue;
        if (MH_CALL(Battle_ActorIsOut)(3 + i) != 0) continue;
        if (MH_AT(ReachFn, bof3::addr::AuraBreath_InReach)(EnemyRecord(3 + i)) == 0) continue;
        if (ActorIndex() == 3 + i) continue;
        struck[0] = 0xFF;
        MH_CALL(Battle_SetTargetFlags)(3 + i, 0x10);
    }
    if (static_cast<unsigned char>(Frame_Counter) & 1) {
        unsigned char* const s = Sc();
        if (s[0xA] < 0x20) Inc(s[0xA]);
    }
    unsigned char* const s = Sc();
    if (s[9] == 0x60) Inc(s[2]);
}

// original 0x4F5590: on odd frames +0xA up below 0x20; +9 up; +0x5D down, at
// 0 the owner's +0xB down and the task freed.
S36_EXPORT void __cdecl AuraBreathDome_Fade(void) {
    const bool odd = (static_cast<unsigned char>(Frame_Counter) & 1) != 0;
    unsigned char* const s = Sc();
    if (odd && s[0xA] < 0x20) Inc(s[0xA]);
    Inc(s[9]);
    Dec(s[0x5D]);
    if (s[0x5D] != 0) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4F55E0: the dome, under the actor matrix: 8 bands (ring radius
// +9 x 16 stepping in by 0x40, height -(sin(0x100 (band + 1)) x +0xA x 16))
// of 32 semi-transparent textured quads (tpage 0x340 / 0x100, CLUT row 0x1FA,
// u by the quad's parity and the band's half, v and height from
// AuraBreathDome_V / _Height), each after a draw-mode packet (0xB5), linked
// at its corner's map point, shaded +0x5D x 8 (a signed byte).
S36_EXPORT void __cdecl AuraBreathDome_Draw(void) {
    {
        const unsigned char* const s = Sc();
        const unsigned shade = static_cast<unsigned>(static_cast<int>(static_cast<signed char>(s[0x5D])) << 3);
        SetSW(8, shade);
        SetSW(0xA, shade);
        SetSW(0xC, shade);
        SetSW(0, static_cast<unsigned>(s[0xA]) << 4);
        SetSW(2, static_cast<unsigned>(s[9]) << 4);
    }
    int v = MH_CALL(Math_Sin)(0);
    SetVW(0xC, 0u - static_cast<unsigned>(Mul12(v, SS(0))));
    int angle = 0x100;
    for (unsigned band = 0; band < 8; ++band, angle += 0x100) {
        const unsigned radius = SW(2);
        SetSW(4, radius);
        SetSW(2, radius - 0x40);
        v = MH_CALL(Math_Sin)(0);
        SetVW(0x18, static_cast<unsigned>(Mul12(v, SS(4))));
        v = MH_CALL(Math_Cos)(0);
        SetVW(0x1A, static_cast<unsigned>(Mul12(v, SS(4))));
        {
            const std::uint16_t z = VW(0xC);
            SetVW(0x14, z);
            SetVW(0x1C, z);
        }
        v = MH_CALL(Math_Sin)(0);
        SetVW(8, static_cast<unsigned>(Mul12(v, SS(2))));
        v = MH_CALL(Math_Cos)(0);
        SetVW(0xA, static_cast<unsigned>(Mul12(v, SS(2))));
        v = MH_CALL(Math_Sin)(angle);
        {
            const unsigned z = 0u - static_cast<unsigned>(Mul12(v, SS(0)));
            SetVW(4, z);
            SetVW(0xC, z);
        }
        const unsigned char half = static_cast<unsigned char>((band >> 2) << 1);
        unsigned char j = 1;
        do {
            const unsigned a = (j & 0x1Fu) << 7;
            SetSW(6, a);
            {
                const std::uint16_t x = VW(8), y = VW(0xA);
                SetVW(0, x);
                SetVW(2, y);
            }
            v = MH_CALL(Math_Sin)(static_cast<short>(a));
            SetVW(8, static_cast<unsigned>(Mul12(v, SS(2))));
            v = MH_CALL(Math_Cos)(SS(6));
            const short next = SS(6);
            {
                const std::uint16_t x = VW(0x18);
                SetVW(0xA, static_cast<unsigned>(Mul12(v, SS(2))));
                const std::uint16_t y = VW(0x1A);
                SetVW(0x10, x);
                SetVW(0x12, y);
            }
            v = MH_CALL(Math_Sin)(next);
            SetVW(0x18, static_cast<unsigned>(Mul12(v, SS(4))));
            v = MH_CALL(Math_Cos)(SS(6));
            const int corner_x = static_cast<short>(VW(0x18));
            const int zv = Mul12(v, SS(4));
            const unsigned char* const s = Sc();
            SetVW(0x1A, static_cast<unsigned>(zv));
            const int px = Add(static_cast<int>(static_cast<std::uint32_t>(corner_x) << 9), Long(s + 0x34));
            const int pz = Add(static_cast<int>(static_cast<std::uint32_t>(static_cast<short>(zv)) << 9), Long(s + 0x38));
            MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0xB5, 0);
            MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(px), static_cast<unsigned long>(pz), 1, 0xC);
            unsigned char* const p = Gfx_PacketNext;
            MH_CALL(Gpu_SetPolyFT4)(p);
            MH_CALL(Gpu_SetSemiTrans)(p, 1);
            SetWord(p + 0x26, MH_CALL(Gpu_GetTPage)(1, 1, 0x340, 0x100) & 0xFFFF);
            SetWord(p + 0x16, MH_CALL(Gpu_GetClut)(0, 0x1FA) & 0xFFFF);
            const unsigned char u = static_cast<unsigned char>(((j & 1) + half) << 6);
            const unsigned char tv = Mem(kDomeV + band)[0];
            const unsigned char bottom = static_cast<unsigned char>(tv + Mem(kDomeHeight + band)[0] - 1);
            p[0x14] = u;
            p[0x15] = tv;
            p[0x24] = static_cast<unsigned char>(u + 0x3F);
            p[0x25] = tv;
            p[0x34] = u;
            p[0x35] = bottom;
            p[0x44] = static_cast<unsigned char>(u + 0x3F);
            p[0x45] = bottom;
            {
                long depth, flag;
                S36_AS(Rtp4Fn, Gte_RotTransPers4)(VP(0), VP(8), VP(0x10), VP(0x18), p + 8, p + 0x18, p + 0x28, p + 0x38,
                                                 &depth, &flag);
            }
            MH_CALL(Gte_PrimDepths4_10)(p);
            p[4] = SB(8);
            p[5] = SB(0xA);
            p[6] = SB(0xC);
            MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(px), static_cast<unsigned long>(pz), 1, 0x48);
            ++j;
        } while (j < 0x21);
    }
}

// original 0x4F5970: 1 when the record's map point lies within the reach word
// 0x903850 of Sprite_Current's (the squares of the (x sar 9, z sar 9)
// differences, wrapping, against the reach squared, signed), else 0 - in all
// of eax. Also called by MAGIC058 (group S11's Sanctuary).
S36_EXPORT int __cdecl AuraBreath_InReach(unsigned char* record) {
    const unsigned char* const s = Sc();
    const std::uint32_t sx = static_cast<std::uint32_t>(Long(s + 0x34) >> 9);
    const std::uint32_t sz = static_cast<std::uint32_t>(Long(s + 0x38) >> 9);
    const std::uint32_t rx = static_cast<std::uint32_t>(Long(record + 0x34) >> 9);
    const std::uint32_t rz = static_cast<std::uint32_t>(Long(record + 0x38) >> 9);
    const std::uint32_t dx = rx - sx, dz = rz - sz;
    const int reach = SS(0);
    const std::int32_t d2 = I32(dx * dx + dz * dz);
    const std::int32_t r2 = I32(static_cast<std::uint32_t>(reach) * static_cast<std::uint32_t>(reach));
    return r2 >= d2 ? 1 : 0;
}

void MagicS36_Inject() {
    if (bof3::WantsShadow("magic_s36")) magic_s36::SelfTest();
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
    BOF3_INJECT(Intimidate_Task);
    BOF3_INJECT(Intimidate_Start);
    BOF3_INJECT(IntimidateChild_Task);
    BOF3_INJECT(IntimidateTrail_Run);
    BOF3_INJECT(IntimidateTrail_Start);
    BOF3_INJECT(IntimidateTrail_Fly);
    BOF3_INJECT(IntimidateTrail_Grow);
    BOF3_INJECT(IntimidateTrail_Fade);
    BOF3_INJECT(IntimidateTrail_DrawThin);
    BOF3_INJECT(IntimidateTrail_DrawWide);
    BOF3_INJECT(IntimidateTrail_Push);
    BOF3_INJECT(IntimidateTrail_PushGap);
    BOF3_INJECT(IntimidateBurst_Run);
    BOF3_INJECT(IntimidateBurst_Start);
    BOF3_INJECT(IntimidateBurst_Rise);
    BOF3_INJECT(IntimidateBurst_Play);
    BOF3_INJECT(IntimidateBurst_Fade);
    BOF3_INJECT(IntimidateBurst_DrawDisc);
    BOF3_INJECT(AuraBreath_Task);
    BOF3_INJECT(AuraBreath_Start);
    BOF3_INJECT(AuraBreathDome_Task);
    BOF3_INJECT(AuraBreathDome_Run);
    BOF3_INJECT(AuraBreathDome_Wait);
    BOF3_INJECT(AuraBreathDome_Grow);
    BOF3_INJECT(AuraBreathDome_Fade);
    BOF3_INJECT(AuraBreathDome_Draw);
    BOF3_INJECT(AuraBreath_InReach);
}

// Moved here by tools/move_funcs.py from magic_s32.cpp (docs/source-layout-round.md).
#include "game/magic/eye_beam.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = magic_harness::at;

}  // namespace

namespace {

using magic_harness::Mem;

using magic_harness::Pointer;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

// MAGIC150's state block (EyeBeam_State): the beam's origin x, z, height
// (dwords), the cylinder's radius, the spiral's radius and the spiral's angle
// (words); the eight sparks (EyeBeam_Sparks, 8 bytes each: +0 live, +2
// radius, +4 angle, +6 disc radius), the spark pointer cell, and two spiral
// points of 0x34 bytes (the previous one, the new one).
// kBeamX: now EyeBeam_State (symbols.toml).
// kBeamZ: now EyeBeam_State[4] (symbols.toml).
// kBeamH: now EyeBeam_State[8] (symbols.toml).
// kBeamR1: now EyeBeam_State[0x10] (symbols.toml).
// kBeamR2: now EyeBeam_State[0x12] (symbols.toml).
// kBeamAngle: now EyeBeam_State[0x14] (symbols.toml).
// kSparks: now EyeBeam_Sparks (symbols.toml).
// kSparkCell: now EyeBeam_SparkCurrent (symbols.toml).
constexpr ::bof3::image::Va kSpiralLast{0x6A90A0};

// kSpiralNew: now EyeBeam_SpiralPoints[0x34] (symbols.toml).
constexpr unsigned kSpiralPoint = 0x34;

// Capcom's, unnamed, outside the band and in no group: the map camera's
// rotation and translation (no arguments), a world point (x, z, height << 16)
// projected into a vertex (two floats and a depth), and the sign of the cross
// product of three projected vertices, truncated by _ftol (docs/magic_c2.md
// section 5 reads the first two).
constexpr ::bof3::image::CodeVa kSetMapCamera{bof3::addr::EffectGte_LoadMapCamera};

constexpr ::bof3::image::CodeVa kProjectPoint{bof3::addr::EffectGte_ProjectPoint};

constexpr ::bof3::image::CodeVa kFacing{bof3::addr::Screen_TriangleWinding};

unsigned char* Sc() { return Sprite_Current; }

unsigned char TargetByte() { return Mem(::bof3::Img(at::kTarget))[0]; }

int S16MagicS32(const unsigned char* p) { return static_cast<short>(Word(p)); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

void AddWord(unsigned char* p, unsigned v) { SetWord(p, (Word(p) + v) & 0xFFFF); }

std::int32_t Add(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b));
}

// `imul` then `sar n`: the 32-bit product wraps, the shift is arithmetic.
int MulSar(int a, int b, unsigned n) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> n;
}

// `imul` then `shl 4`.
int MulShl4(int a, int b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b) << 4);
}

// `fild dword i; fadd dword [f]; fstp dword [out]`, on the x87 as the original
// does it (the precision control the game set rounds the sum, fstp to float).
void AddIntToFloat(unsigned char* out, int i, const unsigned char* f) {
    __asm__ volatile("fildl %1\n\tfadds %2\n\tfstps %0"
                     : "=m"(*reinterpret_cast<float*>(out))
                     : "m"(i), "m"(*reinterpret_cast<const float*>(f))
                     : "st");
}

// `fld dword [f]; fst [a]; fst [b]; fstp [c]`: one float through the x87 into
// three cells.
void CopyFloat3(const unsigned char* f, unsigned char* a, unsigned char* b, unsigned char* c) {
    __asm__ volatile("flds %3\n\tfsts %0\n\tfsts %1\n\tfstps %2"
                     : "=m"(*reinterpret_cast<float*>(a)), "=m"(*reinterpret_cast<float*>(b)),
                       "=m"(*reinterpret_cast<float*>(c))
                     : "m"(*reinterpret_cast<const float*>(f))
                     : "st");
}

int RandCall() { return MH_CALL(Rand)(); }

int Sin(int a) { return MH_CALL(Math_Sin)(a); }

int Cos(int a) { return MH_CALL(Math_Cos)(a); }

// This group's functions and Capcom's called by address, as the originals
// call them: in the game the jmp Inject put there (or Capcom's code under
// BOF3X_ORIGINAL), in the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();

using ByteFn = unsigned char (__cdecl*)();

using ProjectFn = void (__cdecl*)(const std::int32_t*, unsigned char*);

using FacingFn = int (__cdecl*)(const unsigned char*, const unsigned char*, const unsigned char*);

using DiscFn = void (__cdecl*)(const unsigned char*, unsigned, unsigned, unsigned);

void Call0(std::uint32_t address) { MH_WORD(Fn0, address)(); }

void Project(const std::int32_t* v, unsigned char* out) { MH_AT(ProjectFn, kProjectPoint.v)(v, out); }

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

// A stack table's call, `call [esp + phase * 4]`, unchecked in the original.
void StackCall(const std::uint32_t* phases, unsigned n, unsigned phase, const char* who) {
    if (phase >= n) PastTable(who, phase, n);
    magic_harness::Phase(phases[phase])();
}

// The draw-mode packet of MAGIC150's draws: texture page Gpu_GetTPage(0, 1,
// 0x2C0, 0x100), committed to layer 1.
void BeamDrawMode() {
    const unsigned tpage = MH_CALL(Gpu_GetTPage)(0, 1, 0x2C0, 0x100) & 0xFFFFu;
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, tpage, 0);
    MH_CALL(Gfx_CommitPrim)(1, 0xC);
}

}  // namespace

#define S32_EXPORT extern "C" __attribute__((disable_tail_calls))

namespace {

std::int32_t BeamX() { return Long(Mem(::bof3::Img(EyeBeam_State_addr))); }

std::int32_t BeamZ() { return Long(Mem(::bof3::Img(EyeBeam_State_addr) + 0x4)); }

std::int32_t BeamH() { return Long(Mem(::bof3::Img(EyeBeam_State_addr) + 0x8)); }

unsigned char* SparkCurrent() { return Pointer(::bof3::Img(EyeBeam_SparkCurrent_addr)); }

// A point of the beam at angle `a` round the x axis, radius `r` (a word read
// after the call): z = Z + cos x r >> 4, height = H + sin x r << 4, each
// origin read after its call.
std::int32_t RingZ(int c, int r) { return Add(MulSar(c, r, 4), BeamZ()); }

std::int32_t RingH(int s, int r) { return Add(MulShl4(s, r), BeamH()); }

}  // namespace

// original 0x4EA450: the kind-2 task. A seven-entry stack table by +1:
// EyeBeam_Start, _Charge, _WaitSparks, _Widen, _Fire, _Narrow,
// MagicFx_DoneAndFree.
S32_EXPORT void __cdecl EyeBeam_Task(void) {
    static constexpr std::uint32_t kPhases[7] = {bof3::addr::EyeBeam_Start,  bof3::addr::EyeBeam_Charge,
                                                 bof3::addr::EyeBeam_WaitSparks, bof3::addr::EyeBeam_Widen,
                                                 bof3::addr::EyeBeam_Fire,   bof3::addr::EyeBeam_Narrow,
                                                 bof3::addr::MagicFx_DoneAndFree};
    StackCall(kPhases, 7, Sc()[1], "EyeBeam_Task");
}

// original 0x4EA4A0: the beam's origin from the current slot's (0x93B8C4)
// owner: x + 0x14000, z, height + 0x2C00000; the two radii and the angle 0;
// the sparks cleared; +9 0x3C, +1 on; sound effect 0x100.
S32_EXPORT void __cdecl EyeBeam_Start(void) {
    const unsigned char* const slot = Pointer(::bof3::Img(BattleFlow_TaskCurrent_addr));
    const auto* caster = reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(rv::As<records::BattleTask>(slot)->owner));
    SetLong(Mem(::bof3::Img(EyeBeam_State_addr)), Add(rv::As<records::SpriteHead>(caster)->x, 0x14000));
    SetLong(Mem(::bof3::Img(EyeBeam_State_addr) + 0x4), rv::As<records::SpriteHead>(caster)->z);
    SetLong(Mem(::bof3::Img(EyeBeam_State_addr) + 0x8), Add(rv::As<records::SpriteHead>(caster)->height, 0x2C00000));
    SetWord(Mem(::bof3::Img(EyeBeam_State_addr) + 0x10), 0);
    SetWord(Mem(::bof3::Img(EyeBeam_State_addr) + 0x12), 0);
    SetWord(Mem(::bof3::Img(EyeBeam_State_addr) + 0x14), 0);
    Call0(bof3::addr::EyeBeamSparks_Clear);
    Sc()[9] = 0x3C;
    Inc(Sc()[1]);
    MH_CALL(Sound_PlayEffect)(0x100);
}

// original 0x4EA520: a spark spawned (EyeBeamSparks_Spawn) and the sparks
// drawn (EyeBeamSparks_Draw, its answer unread); +9 down; at 0 +1 on.
S32_EXPORT void __cdecl EyeBeam_Charge(void) {
    Call0(bof3::addr::EyeBeamSparks_Spawn);
    MH_AT(ByteFn, bof3::addr::EyeBeamSparks_Draw)();
    Dec(Sc()[9]);
    if (Sc()[9] == 0) Inc(Sc()[1]);
}

// original 0x4EA550: the sparks drawn; once none is live, +9 4 and +1 on.
S32_EXPORT void __cdecl EyeBeam_WaitSparks(void) {
    if (MH_AT(ByteFn, bof3::addr::EyeBeamSparks_Draw)() != 0) return;
    Sc()[9] = 4;
    Inc(Sc()[1]);
}

// original 0x4EA570: the cylinder's radius up by 0x40, the spiral's by 0x60;
// the beam drawn; +9 down; at 0 +9 0x78, +1 on, sound effect 0x101.
S32_EXPORT void __cdecl EyeBeam_Widen(void) {
    AddWord(Mem(::bof3::Img(EyeBeam_State_addr) + 0x10), 0x40);
    AddWord(Mem(::bof3::Img(EyeBeam_State_addr) + 0x12), 0x60);
    Call0(bof3::addr::EyeBeam_Draw);
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    Sc()[9] = 0x78;
    Inc(Sc()[1]);
    MH_CALL(Sound_PlayEffect)(0x101);
}

// original 0x4EA5C0: the spiral turned by -0x200; the cylinder's radius 0x100
// on even frames, 0x110 on odd; the beam drawn; +9 down; at 0 +9 4, +1 on,
// the target flagged 0x40, sound effect 0x102.
S32_EXPORT void __cdecl EyeBeam_Fire(void) {
    const unsigned frame = static_cast<unsigned char>(Frame_Counter);
    AddWord(Mem(::bof3::Img(EyeBeam_State_addr) + 0x14), 0xFE00);
    SetWord(Mem(::bof3::Img(EyeBeam_State_addr) + 0x10), ((frame & 1u) + 0x10u) << 4);
    Call0(bof3::addr::EyeBeam_Draw);
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    Sc()[9] = 4;
    Inc(Sc()[1]);
    MH_CALL(Battle_SetTargetFlag40)(TargetByte());
    MH_CALL(Sound_PlayEffect)(0x102);
}

// original 0x4EA630: the radii down by 0x40 and 0x60; the beam drawn; +9 down;
// at 0 +1 on.
S32_EXPORT void __cdecl EyeBeam_Narrow(void) {
    AddWord(Mem(::bof3::Img(EyeBeam_State_addr) + 0x10), 0xFFC0);
    AddWord(Mem(::bof3::Img(EyeBeam_State_addr) + 0x12), 0xFFA0);
    Call0(bof3::addr::EyeBeam_Draw);
    Dec(Sc()[9]);
    if (Sc()[9] == 0) Inc(Sc()[1]);
}

// original 0x4EA670: a draw-mode packet (layer 1), the map camera (0x494060),
// the cylinder and the spiral (a tail jmp).
S32_EXPORT void __cdecl EyeBeam_Draw(void) {
    BeamDrawMode();
    Call0(kSetMapCamera.v);
    Call0(bof3::addr::EyeBeam_DrawCylinder);
    Call0(bof3::addr::EyeBeam_DrawSpiral);
}

// original 0x4EA6C0: sixteen semi-transparent flat quads (red 0x80) round the
// x axis from the origin to 0x100000 along it: quad i between the angles
// 0x100 i and 0x100 (i + 1), radius the word 0x6A9050; each corner a world
// point projected by 0x494110 (a vector of the caller's, x changed between the
// two of an angle); committed at 1 only when 0x4941B0 of its first three
// vertices is negative in its low word (the quad faces the camera).
S32_EXPORT void __cdecl EyeBeam_DrawCylinder(void) {
    unsigned angle = 0;
    unsigned next = 0;   // the original's stack dword, stepped by 0x100
    for (unsigned n = 0x10; n != 0; --n) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyF4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        std::int32_t v[3];
        unsigned char out[12];
        v[0] = BeamX();
        int c = Cos(static_cast<int>(angle));
        v[1] = RingZ(c, S16MagicS32(Mem(::bof3::Img(EyeBeam_State_addr) + 0x10)));
        int s = Sin(static_cast<int>(angle));
        v[2] = RingH(s, S16MagicS32(Mem(::bof3::Img(EyeBeam_State_addr) + 0x10)));
        Project(v, out);
        std::memcpy(p + 8, out, 12);
        v[0] = Add(BeamX(), 0x100000);
        Project(v, out);
        next += 0x100;
        angle = next & 0xFFFF;
        std::memcpy(p + 0x14, out, 12);
        v[0] = BeamX();
        c = Cos(static_cast<int>(angle));
        v[1] = RingZ(c, S16MagicS32(Mem(::bof3::Img(EyeBeam_State_addr) + 0x10)));
        s = Sin(static_cast<int>(angle));
        v[2] = RingH(s, S16MagicS32(Mem(::bof3::Img(EyeBeam_State_addr) + 0x10)));
        Project(v, out);
        std::memcpy(p + 0x20, out, 12);
        v[0] = Add(BeamX(), 0x100000);
        Project(v, out);
        std::memcpy(p + 0x2C, out, 12);
        p[4] = 0x80;
        p[5] = 0;
        p[6] = 0;
        const int facing = MH_AT(FacingFn, kFacing.v)(p + 8, p + 0x14, p + 0x20);
        if (static_cast<short>(facing) < 0) MH_CALL(Gfx_CommitPrim)(1, 0x38);
    }
}

// original 0x4EA890: the map camera; a spiral of 0x80 segments along x from
// the origin (x read once, after the camera), the steps 0x1000, 0x1040, ...,
// each point jittered in x by ((Rand & 0xFFF) - 0x800) << 4, turned 0x80 on
// from the angle 0x6A9054 (only its low word counts), radius the word
// 0x6A9052: each new point (EyeBeam_SpiralNew) projected three ways
// (EyeBeam_ProjectSpiralPoint), the previous one copied to EyeBeam_SpiralLast
// first, and the segment between them drawn (EyeBeam_DrawSpiralSegment).
S32_EXPORT void __cdecl EyeBeam_DrawSpiral(void) {
    using PointFn = void (__cdecl*)(unsigned char*);
    Call0(kSetMapCamera.v);
    std::uint32_t angle = static_cast<std::uint32_t>(Long(Mem(::bof3::Img(EyeBeam_State_addr) + 0x14)));
    std::int32_t x = BeamX();
    std::int32_t step = 0x1000;
    unsigned char* const point = Mem(::bof3::Img(EyeBeam_SpiralPoints_addr) + 0x34);
    SetLong(point, x);
    unsigned a = angle & 0xFFFF;
    int c = Cos(static_cast<int>(a));
    SetLong(point + 4, RingZ(c, S16MagicS32(Mem(::bof3::Img(EyeBeam_State_addr) + 0x12))));
    int s = Sin(static_cast<int>(a));
    SetLong(point + 8, RingH(s, S16MagicS32(Mem(::bof3::Img(EyeBeam_State_addr) + 0x12))));
    MH_AT(PointFn, bof3::addr::EyeBeam_ProjectSpiralPoint)(point);
    for (unsigned n = 0x80; n != 0; --n) {
        x = Add(x, step);
        step += 0x40;
        for (unsigned k = 0; k < kSpiralPoint; k += 4) SetLong(Mem(::bof3::Img(kSpiralLast.v) + k), Long(Mem(::bof3::Img(EyeBeam_SpiralPoints_addr) + 0x34 + k)));
        angle += 0x80;
        const int r = RandCall();
        SetLong(point, Add(static_cast<std::int32_t>((static_cast<std::uint32_t>(r & 0xFFF) - 0x800u) << 4), x));
        a = angle & 0xFFFF;
        c = Cos(static_cast<int>(a));
        SetLong(point + 4, RingZ(c, S16MagicS32(Mem(::bof3::Img(EyeBeam_State_addr) + 0x12))));
        s = Sin(static_cast<int>(a));
        SetLong(point + 8, RingH(s, S16MagicS32(Mem(::bof3::Img(EyeBeam_State_addr) + 0x12))));
        MH_AT(PointFn, bof3::addr::EyeBeam_ProjectSpiralPoint)(point);
        MH_AT(PointFn, bof3::addr::EyeBeam_DrawSpiralSegment)(Mem(::bof3::Img(kSpiralLast.v)));
    }
}

// original 0x4EA9D0: a spiral point (x, z, height at +0, +4, +8) projected
// three times by 0x494110: as it is into +0x1C, 0x8000 back in x into +0x10,
// 0x8000 on into +0x28 (x left 0x8000 on).
S32_EXPORT void __cdecl EyeBeam_ProjectSpiralPoint(unsigned char* point) {
    const auto* v = reinterpret_cast<const std::int32_t*>(point);
    Project(v, point + 0x1C);
    SetLong(point, Add(Long(point), -0x8000));
    Project(v, point + 0x10);
    SetLong(point, Add(Long(point), 0x10000));
    Project(v, point + 0x28);
}

// original 0x4EAA10: the segment from a spiral point to the next (the next
// 0x34 bytes on): two semi-transparent gouraud quads (yellow 0x40 on the
// first and third corners, black on the others), the second a copy of the
// first packet with two corners replaced, then a flat line (0x20) between the
// two points' middles; all committed at 1.
S32_EXPORT void __cdecl EyeBeam_DrawSpiralSegment(unsigned char* point) {
    unsigned char* const next = point + kSpiralPoint;
    unsigned char* p = Gfx_PacketNext;
    MH_CALL(Gpu_SetPolyG4)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    std::memcpy(p + 8, point + 0x1C, 12);
    std::memcpy(p + 0x18, point + 0x10, 12);
    std::memcpy(p + 0x28, next + 0x1C, 12);
    std::memcpy(p + 0x38, next + 0x10, 12);
    p[4] = 0x40;
    p[5] = 0x40;
    p[6] = 0;
    p[0x14] = 0;
    p[0x15] = 0;
    p[0x16] = 0;
    p[0x24] = 0x40;
    p[0x25] = 0x40;
    p[0x26] = 0;
    p[0x34] = 0;
    p[0x35] = 0;
    p[0x36] = 0;
    MH_CALL(Gfx_CommitPrim)(1, 0x44);
    // `rep movsd` from the 0x44 bytes before the packet pointer: the quad just
    // committed, whichever the pointer is now.
    p = Gfx_PacketNext;
    for (unsigned k = 0; k < 0x44; k += 4) SetLong(p + k, Long(p - 0x44 + k));
    std::memcpy(p + 0x18, point + 0x28, 12);
    std::memcpy(p + 0x38, next + 0x28, 12);
    MH_CALL(Gfx_CommitPrim)(1, 0x44);
    p = Gfx_PacketNext;
    MH_CALL(Gpu_SetLineF2)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    std::memcpy(p + 8, point + 0x1C, 12);
    std::memcpy(p + 0x14, next + 0x1C, 12);
    p[6] = 0;
    p[4] = 0x20;
    p[5] = 0x20;
    MH_CALL(Gfx_CommitPrim)(1, 0x20);
}

// original 0x4EAB70: the eight sparks' +0 words cleared through the pointer
// cell 0x6A9098 (left one past the last).
S32_EXPORT void __cdecl EyeBeamSparks_Clear(void) {
    SetLong(Mem(::bof3::Img(EyeBeam_SparkCurrent_addr)), static_cast<std::int32_t>(::bof3::Img(EyeBeam_Sparks_addr)));
    for (unsigned n = 8; n != 0; --n) {
        SetWord(SparkCurrent(), 0);
        SetLong(Mem(::bof3::Img(EyeBeam_SparkCurrent_addr)), Add(Long(Mem(::bof3::Img(EyeBeam_SparkCurrent_addr))), 8));
    }
}

// original 0x4EABA0: the first spark whose +0 word is 0 (the pointer cell left
// at it, or one past the last when none): +0 1, radius +2 0x300, angle +4 (Rand
// & 0xFF) << 4, disc radius +6 8 - each written through the cell, read again
// after Rand.
S32_EXPORT void __cdecl EyeBeamSparks_Spawn(void) {
    std::uint32_t e = ::bof3::Img(EyeBeam_Sparks_addr);
    SetLong(Mem(::bof3::Img(EyeBeam_SparkCurrent_addr)), static_cast<std::int32_t>(e));
    for (unsigned n = 0;; ) {
        if (Word(Mem(e)) == 0) break;
        e += 8;
        ++n;
        SetLong(Mem(::bof3::Img(EyeBeam_SparkCurrent_addr)), static_cast<std::int32_t>(e));
        if (n >= 8) return;
    }
    SetWord(Mem(e), 1);
    SetWord(SparkCurrent() + 2, 0x300);
    const int r = RandCall();
    SetWord(SparkCurrent() + 4, (static_cast<unsigned>(r) & 0xFFu) << 4);
    SetWord(SparkCurrent() + 6, 8);
}

// original 0x4EAC00: a draw-mode packet (layer 1) and the map camera; then
// each live spark (walked through the pointer cell, re-read after every call)
// drawn as a disc (EyeBeamSpark_DrawDisc: shade 0x80 at the centre, radius
// its +6) at the origin x and angle +4 round the x axis at radius +2 (z by
// cos >> 4, height by sin << 4); its angle on 0x80, radius down 0x60, disc
// radius down 1; freed at radius 0. Answers in al whether any was live.
S32_EXPORT unsigned char __cdecl EyeBeamSparks_Draw(void) {
    BeamDrawMode();
    Call0(kSetMapCamera.v);
    std::uint32_t e = ::bof3::Img(EyeBeam_Sparks_addr);
    unsigned char any = 0;
    SetLong(Mem(::bof3::Img(EyeBeam_SparkCurrent_addr)), static_cast<std::int32_t>(e));
    for (unsigned n = 8; n != 0; --n) {
        if (Word(Mem(e)) != 0) {
            std::int32_t v[3];
            unsigned char out[12];
            v[0] = BeamX();
            const int c = Cos(Word(Mem(e) + 4));
            unsigned char* cur = SparkCurrent();
            v[1] = Add(MulSar(c, S16MagicS32(cur + 2), 4), BeamZ());
            const int s = Sin(Word(cur + 4));
            cur = SparkCurrent();
            v[2] = Add(MulShl4(s, S16MagicS32(cur + 2)), BeamH());
            Project(v, out);
            cur = SparkCurrent();
            MH_AT(DiscFn, bof3::addr::EyeBeamSpark_DrawDisc)(out, Word(cur + 6), 0x80, 0);
            AddWord(SparkCurrent() + 4, 0x80);
            AddWord(SparkCurrent() + 2, 0xFFA0);
            AddWord(SparkCurrent() + 6, 0xFFFF);
            e = static_cast<std::uint32_t>(Long(Mem(::bof3::Img(EyeBeam_SparkCurrent_addr))));
            any = 1;
            if (Word(Mem(e) + 2) == 0) {
                SetWord(Mem(e), 0);
                e = static_cast<std::uint32_t>(Long(Mem(::bof3::Img(EyeBeam_SparkCurrent_addr))));
            }
        }
        e += 8;
        SetLong(Mem(::bof3::Img(EyeBeam_SparkCurrent_addr)), static_cast<std::int32_t>(e));
    }
    return any;
}

// original 0x4EAD40: a disc of sixteen semi-transparent gouraud triangles in
// screen space round a projected point (x, y floats and a depth at `centre`),
// radius the low word of `radius`: the centre coloured red `shade` (a byte),
// the rim red `rim`; each rim point cos / sin x radius >> 12 added on the
// x87 to the centre's floats, all three at the centre's depth; committed at 1.
S32_EXPORT void __cdecl EyeBeamSpark_DrawDisc(const unsigned char* centre, unsigned radius, unsigned shade,
                                              unsigned rim) {
    const int r = static_cast<short>(radius & 0xFFFF);
    std::uint32_t a = 0, b = 0x100;
    for (;;) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        SetLong(p + 8, Long(centre));
        SetLong(p + 0xC, Long(centre + 4));
        int v = MulSar(Cos(static_cast<int>(a)), r, 12);
        AddIntToFloat(p + 0x18, v, centre);
        v = MulSar(Sin(static_cast<int>(a)), r, 12);
        const unsigned b16 = b & 0xFFFF;
        AddIntToFloat(p + 0x1C, v, centre + 4);
        v = MulSar(Cos(static_cast<int>(b16)), r, 12);
        AddIntToFloat(p + 0x28, v, centre);
        v = MulSar(Sin(static_cast<int>(b16)), r, 12);
        AddIntToFloat(p + 0x2C, v, centre + 4);
        CopyFloat3(centre + 8, p + 0x30, p + 0x20, p + 0x10);
        p[4] = static_cast<unsigned char>(shade);
        p[5] = 0;
        p[6] = 0;
        p[0x14] = static_cast<unsigned char>(rim);
        p[0x15] = 0;
        p[0x16] = 0;
        p[0x24] = static_cast<unsigned char>(rim);
        p[0x25] = 0;
        p[0x26] = 0;
        MH_CALL(Gfx_CommitPrim)(1, 0x34);
        const std::uint32_t was = b;
        b += 0x100;
        a += 0x100;
        if ((was & 0xFFFF) >= 0x1000) break;
    }
}

void EyeBeam_Inject() {
    BOF3_INJECT(EyeBeam_Task);
    BOF3_INJECT(EyeBeam_Start);
    BOF3_INJECT(EyeBeam_Charge);
    BOF3_INJECT(EyeBeam_WaitSparks);
    BOF3_INJECT(EyeBeam_Widen);
    BOF3_INJECT(EyeBeam_Fire);
    BOF3_INJECT(EyeBeam_Narrow);
    BOF3_INJECT(EyeBeam_Draw);
    BOF3_INJECT(EyeBeam_DrawCylinder);
    BOF3_INJECT(EyeBeam_DrawSpiral);
    BOF3_INJECT(EyeBeam_ProjectSpiralPoint);
    BOF3_INJECT(EyeBeam_DrawSpiralSegment);
    BOF3_INJECT(EyeBeamSparks_Clear);
    BOF3_INJECT(EyeBeamSparks_Spawn);
    BOF3_INJECT(EyeBeamSparks_Draw);
    BOF3_INJECT(EyeBeamSpark_DrawDisc);
}

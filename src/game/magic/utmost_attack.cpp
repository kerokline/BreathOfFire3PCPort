// Moved here by tools/move_funcs.py from magic_c2.cpp (docs/source-layout-round.md).
#include "game/magic/utmost_attack.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_c2.h"
#include "game/magic_harness.h"
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

namespace addr = bof3::addr;

using magic_harness::Mem;

using magic_harness::Pointer;

using move_script::Long;

using move_script::SetLong;

constexpr ::bof3::image::Va kStreaks{0x698EC0};          // UtmostAttack_Streaks: 128 records of 0x3C

constexpr std::uint32_t kStreak = 0x3C;

// Capcom's, unnamed, outside the band and in no group: the map camera's
// rotation and translation set from Camera_Angles and the map focus (no
// arguments), and a world point (x, z, height << 16) projected into a
// primitive's vertex (docs/magic_c2.md section 5).
constexpr ::bof3::image::CodeVa kSetMapCamera{bof3::addr::EffectGte_LoadMapCamera};

constexpr ::bof3::image::CodeVa kProjectPoint{bof3::addr::EffectGte_ProjectPoint};

unsigned char* Sc() { return Sprite_Current; }

// [[0x93B8C4] + 0x80]: the current slot's owner - the caster's sprite.
unsigned char* Caster() {
    return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(static_cast<std::int32_t>(rv::As<records::BattleTask>(Pointer(::bof3::Img(BattleFlow_TaskCurrent_addr)))->owner))));
}

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

std::int32_t Add(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b));
}

void AddLong(unsigned char* at, std::int32_t v) { SetLong(at, Add(Long(at), v)); }

int RandCall() { return MH_CALL(Rand)(); }

// This group's functions called by address, as the originals call them: in the
// game the jmp Inject put there (or Capcom's code under BOF3X_ORIGINAL), in
// the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();

using FnB = unsigned char (__cdecl*)();

using FnP = void (__cdecl*)(unsigned char*);

void Call0(std::uint32_t address) { MH_WORD(Fn0, address)(); }

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

// A stack table's dispatch: the phase byte through its entries.
void Step(const std::uint32_t* table, unsigned entries, unsigned phase, const char* who) {
    if (phase >= entries) PastTable(who, phase, entries);
    magic_harness::Phase(table[phase])();
}

// The caster's animation, run with Sprite_Current its sprite and put back.
void CasterAnimation(unsigned animation) {
    unsigned char* const caster = Caster();
    unsigned char* const self = Sc();
    Sprite_Current = caster;
    MH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>(animation));
    Sprite_Current = self;
}

}  // namespace

#define C2_EXPORT extern "C" __attribute__((disable_tail_calls))

// original 0x4D9AE0: the kind-2 task. A four-entry stack table by +1:
// UtmostAttack_Start, _WaitCaster, _Stream, _End.
C2_EXPORT void __cdecl UtmostAttack_Task(void) {
    static constexpr std::uint32_t kPhases[4] = {addr::UtmostAttack_Start, addr::UtmostAttack_WaitCaster,
                                                 addr::UtmostAttack_Stream, addr::UtmostAttack_End};
    Step(kPhases, 4, Sc()[1], "UtmostAttack_Task");
}

// original 0x4D9B20: the task at the caster, 0x100 units higher (+0x3C +
// 0x1000000); the streaks cleared; the caster's animation 1; +1 on.
C2_EXPORT void __cdecl UtmostAttack_Start(void) {
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(Caster())->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(Caster())->z);
    rv::Store(rv::Current<records::BattleTask>()->height, Add(rv::As<records::SpriteHead>(Caster())->height, 0x1000000));
    Call0(addr::UtmostAttack_ClearStreaks);
    CasterAnimation(1);
    Inc(Sc()[1]);
}

// original 0x4D9BB0: the caster's script ticked once; when it reports its end,
// +9 0x78 (frames of streaks), +1 on, sound effect 0x100 (Sprite_Current put
// back after it too).
C2_EXPORT void __cdecl UtmostAttack_WaitCaster(void) {
    unsigned char* const caster = Caster();
    unsigned char* const self = Sc();
    Sprite_Current = caster;
    const unsigned char done = MH_CALL(Sprite_ScriptTickOnce)();
    Sprite_Current = self;
    if (!done) return;
    self[9] = 0x78;
    Inc(Sc()[1]);
    MH_CALL(Sound_PlayEffect)(0x100);
    Sprite_Current = self;
}

// original 0x4D9C00: eight streaks spawned, every streak drawn and moved (the
// answer unread); +9 down, at 0 +1 on, Battle_SetTargetFlag40(target), sound
// effect 0x101.
C2_EXPORT void __cdecl UtmostAttack_Stream(void) {
    Call0(addr::UtmostAttack_SpawnStreaks);
    MH_AT(FnB, addr::UtmostAttack_DrawStreaks)();
    Dec(Sc()[9]);
    unsigned char* const s = Sc();
    if (s[9] != 0) return;
    Inc(s[1]);
    MH_CALL(Battle_SetTargetFlag40)(Mem(::bof3::Img(at::kTarget))[0]);
    MH_CALL(Sound_PlayEffect)(0x101);
}

// original 0x4D9C50: the streaks drawn and moved until none is left; then the
// done flag and free (a tail jmp).
C2_EXPORT void __cdecl UtmostAttack_End(void) {
    if (MH_AT(FnB, addr::UtmostAttack_DrawStreaks)() != 0) return;
    Mem(::bof3::Img(magic_harness::at::kFlags))[0] = static_cast<unsigned char>(Mem(::bof3::Img(magic_harness::at::kFlags))[0] | 4);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4D9C70: +0 of the 128 streaks cleared.
C2_EXPORT void __cdecl UtmostAttack_ClearStreaks(void) {
    for (unsigned i = 0; i < 128; ++i) Mem(::bof3::Img(kStreaks.v) + i * kStreak)[0] = 0;
}

// original 0x4D9C90: a draw-mode packet (tpage Gpu_GetTPage(0, 1, 0x2C0, 0x100),
// committed at 3); the map camera (0x494060); every live streak drawn, then
// both its points moved by its velocity (+0x24, +0x28, +0x2C, read after the
// draw) and its life +2 down, +0 cleared at 0. Answers in al whether any
// streak was live.
C2_EXPORT unsigned char __cdecl UtmostAttack_DrawStreaks(void) {
    unsigned char any = 0;
    const unsigned tpage = MH_CALL(Gpu_GetTPage)(0, 1, 0x2C0, 0x100) & 0xFFFF;
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, tpage, 0);
    MH_CALL(Gfx_CommitPrim)(3, 0xC);
    Call0(kSetMapCamera.v);
    for (unsigned i = 0; i < 128; ++i) {
        unsigned char* const r = Mem(::bof3::Img(kStreaks.v) + i * kStreak);
        if (r[0] == 0) continue;
        MH_AT(FnP, addr::UtmostAttack_DrawStreak)(r);
        const std::int32_t vx = Long(r + 0x24), vz = Long(r + 0x28), vy = Long(r + 0x2C);
        AddLong(r + 4, vx);
        AddLong(r + 8, vz);
        AddLong(r + 0xC, vy);
        AddLong(r + 0x14, vx);
        AddLong(r + 0x18, vz);
        AddLong(r + 0x1C, vy);
        Dec(r[2]);
        any = 1;
        if (r[2] == 0) r[0] = 0;
    }
    return any;
}

// original 0x4D9D50: one semi-transparent gouraud quad (Gpu_SetPolyG4): the
// streak's first point (+4, +8, +0xC) 0x1000 either side in x and its second
// (+0x14, +0x18, +0x1C) likewise, each projected by 0x494110 from a vector in
// the caller's frame (x changed between the two of a point, z and height
// kept); the first two vertices +0x34..+0x36's colour, the last two
// +0x38..+0x3A's; committed at 3, 0x44 bytes.
C2_EXPORT void __cdecl UtmostAttack_DrawStreak(unsigned char* streak) {
    using ProjectFn = void (__cdecl*)(const std::int32_t*, unsigned char*);
    unsigned char* const p = Gfx_PacketNext;
    MH_CALL(Gpu_SetPolyG4)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    std::int32_t v[3] = {Add(Long(streak + 4), 0x1000), Long(streak + 8), Long(streak + 0xC)};
    MH_AT(ProjectFn, kProjectPoint.v)(v, p + 8);
    v[0] = Add(Long(streak + 4), -0x1000);
    MH_AT(ProjectFn, kProjectPoint.v)(v, p + 0x18);
    v[0] = Add(Long(streak + 0x14), 0x1000);
    v[1] = Long(streak + 0x18);
    v[2] = Long(streak + 0x1C);
    MH_AT(ProjectFn, kProjectPoint.v)(v, p + 0x28);
    v[0] = Add(Long(streak + 0x14), -0x1000);
    MH_AT(ProjectFn, kProjectPoint.v)(v, p + 0x38);
    for (unsigned k = 0; k < 3; ++k) {
        const unsigned char first = streak[0x34 + k];
        p[0x14 + k] = first;
        p[4 + k] = first;
        const unsigned char second = streak[0x38 + k];
        p[0x34 + k] = second;
        p[0x24 + k] = second;
    }
    MH_CALL(Gfx_CommitPrim)(3, 0x44);
}

// original 0x4D9E40: eight times, a free streak (UtmostAttack_FreeStreak)
// started (UtmostAttack_InitStreak) if there is one.
C2_EXPORT void __cdecl UtmostAttack_SpawnStreaks(void) {
    using FindFn = unsigned char* (__cdecl*)();
    for (unsigned n = 0; n < 8; ++n) {
        unsigned char* const r = MH_AT(FindFn, addr::UtmostAttack_FreeStreak)();
        if (r) MH_AT(FnP, addr::UtmostAttack_InitStreak)(r);
    }
}

// original 0x4D9E60: the first streak whose +0 is 0, or null.
C2_EXPORT unsigned char* __cdecl UtmostAttack_FreeStreak(void) {
    for (unsigned i = 0; i < 128; ++i) {
        unsigned char* const r = Mem(::bof3::Img(kStreaks.v) + i * kStreak);
        if (r[0] == 0) return r;
    }
    return nullptr;
}

// original 0x4D9E80: +0 1, +1 0, life +2 0x10; d = ((Rand & 0x1FF) - 0x100) <<
// 8; the first point the task's x + 2d, z + 2 units, height; the second the
// task's x + d, z, height; colours +0x34..+0x36 each Rand << 7 (0 or 0x80),
// +0x38..+0x3A 0; the velocity (d / 2, one unit, 0).
C2_EXPORT void __cdecl UtmostAttack_InitStreak(unsigned char* streak) {
    streak[0] = 1;
    streak[1] = 0;
    streak[2] = 0x10;
    const std::int32_t d =
        static_cast<std::int32_t>(((static_cast<std::uint32_t>(RandCall()) & 0x1FF) - 0x100) << 8);
    SetLong(streak + 4, Add(rv::Current<records::BattleTask>()->x, static_cast<std::int32_t>(static_cast<std::uint32_t>(d) * 2)));
    SetLong(streak + 8, Add(rv::Current<records::BattleTask>()->z, 0x20000));
    SetLong(streak + 0xC, rv::Current<records::BattleTask>()->height);
    SetLong(streak + 0x14, Add(rv::Current<records::BattleTask>()->x, d));
    SetLong(streak + 0x18, rv::Current<records::BattleTask>()->z);
    SetLong(streak + 0x1C, rv::Current<records::BattleTask>()->height);
    streak[0x34] = static_cast<unsigned char>(static_cast<unsigned>(RandCall()) << 7);
    streak[0x35] = static_cast<unsigned char>(static_cast<unsigned>(RandCall()) << 7);
    const auto blue = static_cast<unsigned char>(static_cast<unsigned>(RandCall()) << 7);
    SetLong(streak + 0x24, d >> 1);
    streak[0x36] = blue;
    SetLong(streak + 0x28, 0x10000);
    SetLong(streak + 0x2C, 0);
    streak[0x38] = 0;
    streak[0x39] = 0;
    streak[0x3A] = 0;
}

void UtmostAttack_Inject() {
    BOF3_INJECT(UtmostAttack_Task);
    BOF3_INJECT(UtmostAttack_Start);
    BOF3_INJECT(UtmostAttack_WaitCaster);
    BOF3_INJECT(UtmostAttack_Stream);
    BOF3_INJECT(UtmostAttack_End);
    BOF3_INJECT(UtmostAttack_ClearStreaks);
    BOF3_INJECT(UtmostAttack_DrawStreaks);
    BOF3_INJECT(UtmostAttack_DrawStreak);
    BOF3_INJECT(UtmostAttack_SpawnStreaks);
    BOF3_INJECT(UtmostAttack_FreeStreak);
    BOF3_INJECT(UtmostAttack_InitStreak);
}

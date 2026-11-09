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
#include "game/record_views.h"
#include "game/records.h"
#include "game/records_gpu.h"
#include "hook/detour.h"
#include "hook/log.h"

// Rebound 2026-09-29 (round twelve group BE4, docs/battle_e4.md section 9): the constants here naming BE4's functions read
// bof3::addr::<Name>; the values are unchanged (the fuzz keys on them).

namespace {

namespace at = magic_harness::at;
using magic_harness::Mem;
using magic_harness::Pointer;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// The scratch the overlays keep their working values in: SharedScratch's
// sixteen bytes (0x903850..0x90385F, Scratch_Swap at +0xC) and the four
// SVECTORs of Prim_VertexScratch (0x9037A0..0x9037BF). Both are read again
// after every call, as the originals read them.
constexpr ::bof3::image::Va kS{SharedScratch_addr};
// kV: now Prim_VertexScratch (symbols.toml).

// kOrbShades: now MagicBallOrb_Shades (symbols.toml). MagicBallOrb_Shades: 10 byte pairs, by the orb's +0xB
// kTrailPoints: now IntimidateTrail_Points (symbols.toml). IntimidateTrail_Points: 32 (x, y) words a trail, by its +4
// kStruck: now AuraBreath_Struck (symbols.toml). AuraBreath_Struck: a byte an enemy, 0xFF once flagged
// kDomeV: now AuraBreathDome_V (symbols.toml). AuraBreathDome_V: 8 bytes, a texture v a band
// kDomeHeight: now AuraBreathDome_Height (symbols.toml). AuraBreathDome_Height: 8 bytes, a texture height a band

unsigned char* Sc() { return Sprite_Current; }
unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }
unsigned char TargetByte() { return Mem(::bof3::Img(at::kTarget))[0]; }
unsigned ActorIndex() { return static_cast<unsigned>(Long(Mem(::bof3::Img(at::kActor)))) & 0xFF; }

std::uint16_t SW(unsigned k) { return Word(Mem(::bof3::Img(kS.v) + k)); }
short SS(unsigned k) { return static_cast<short>(SW(k)); }
unsigned char SB(unsigned k) { return Mem(::bof3::Img(kS.v) + k)[0]; }
void SetSW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(kS.v) + k), v & 0xFFFF); }

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
// The originals index the enemy records by the battle index - 3, unchecked.
unsigned char* EnemyRecord(unsigned battle_index) {
    return Mem(::bof3::Img(Battle_Enemies_addr) + static_cast<std::uint32_t>(static_cast<int>(battle_index) - 3) * at::kEnemyStride);
}

std::uint32_t RandCall() { return static_cast<std::uint32_t>(MH_CALL(Rand)()); }
unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }

// This group's functions called by address, as the originals call them: in
// the game the jmp Inject put there (or Capcom's code under
// BOF3X_ORIGINAL), in the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();
void Call0(std::uint32_t address) { MH_WORD(Fn0, address)(); }

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

// ===========================================================================
// MAGIC172 (row 23, Magic Ball read one id down)

// ===========================================================================
// MAGIC173 (row 34, Intimidate read one id down)

// ===========================================================================
// MAGIC218 (row 142, Aura Breath read one id down)

void MagicS36_Inject() {
    if (bof3::WantsShadow("magic_s36")) magic_s36::SelfTest();
}

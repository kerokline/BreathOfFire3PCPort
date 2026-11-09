// Chapter 9's first block of scenario code, 0x553B30..0x557170 (round ten
// group SC9a). docs/scena_sc9a.md.
//
//   - The vtable at 0x6613E8 (0x662C80's entry 9): slot 0 Scena09_Frame, a
//     tail jump through Scena09_States on the state byte; slot 1
//     Scena09_ObjectTrigger, a call through Scena09_Objects on the object's
//     +0x86. Slots 2 (0x557270) and 4 (0x557A20) lie in group SC9b's block;
//     slot 3 is Scenario_NoHook (ours, SC1).
//   - State 1 Scena09_EnterArea (every way out stores state 2); state 2
//     Scena09_Run, a tail jump through Scena09_Runs on MoveScript_Var7; the
//     runs 1..11 and 14..16 are each a switch on the step byte 0x8034E5 whose
//     cases wait on a counter, the request byte, the wait word, the stream or
//     a timer, do one thing and set the next step. State 0 (0x5646B0) lies in
//     group SC13's block.
//   - Objects 1..4, the entries of Scena09_Objects in this band: each starts a
//     run at a step.
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies. The tables
// are read in place and their entries called directly, as Scena12_Frame
// does; the fuzz swaps them for recorders. No divergence: each function is a
// faithful replacement, except that a dispatcher whose index lies outside its
// table (a negative state or run, or one reading the next table) aborts where
// the original would jump through its neighbour - the project's rule for an
// index past a table (round nine, section 6).
#include "game/scena_sc9a.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/record_views.h"
#include "game/scena_sc9a_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = scena_sc9a::at;

unsigned char& B(std::uint32_t a) { return *reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a)); }
std::uint16_t& W(std::uint32_t a) { return *reinterpret_cast<std::uint16_t*>(static_cast<std::uintptr_t>(a)); }

unsigned char Counter(unsigned k) { return B(::bof3::Img(scena_sc9a::at::kCounters) + k); }
void SetCounter(unsigned k, unsigned char v) { B(::bof3::Img(scena_sc9a::at::kCounters) + k) = v; }
void SetStep(unsigned char v) { MoveScript_PartyRecords[0x65] = v; }
void SetRun(unsigned char v) { MoveScript_Var7 = v; }
void SetPass(unsigned char v) { Draw_PassFlags = v; }
void Redraw() { MapView_Redraw = 2; }
void CamAdd(std::uint16_t v) { Camera_Distance = static_cast<std::uint16_t>(static_cast<std::uint16_t>(Camera_Distance) + v); }

// The flag bits are read from 0x929ED0 afresh for every call, as the
// originals load the dword before each push.
unsigned char* Bits() { return reinterpret_cast<unsigned char*>(Chapter_FlagRow); }
void Set(unsigned i) { SH_CALL(Flags_Set)(Bits(), i); }

void Clear40() { SH_CALL(ScriptFlags_Clear40)(); }
void Transition(unsigned char k) { SH_CALL(Transition_Start)(k); }

// A run's end: ScriptFlags_Clear40, then the step and the run 0.
void EndRun() {
    Clear40();
    SetStep(0);
    SetRun(0);
}

unsigned char* Effect(unsigned char slot) { return &B(::bof3::Img(scena_sc9a::at::kEffects) + static_cast<std::uint32_t>(slot) * scena_sc9a::at::kEffectStride); }

// An effect of kind 0x13 at a camera-relative point: +0 = 1, +5 = 0x13, the
// dwords +0x64 = x, +0x68 = y, +0x6C = z, the byte +9 = life.
void Place13(unsigned char slot, std::int32_t x, std::int32_t y, std::int32_t z, unsigned char life) {
    unsigned char* const e = Effect(slot);
    e[0] = 1;
    e[5] = 0x13;
    *reinterpret_cast<std::int32_t*>(e + 0x64) = x;
    *reinterpret_cast<std::int32_t*>(e + 0x68) = y;
    *reinterpret_cast<std::int32_t*>(e + 0x6C) = z;
    e[9] = life;
}
std::int32_t AngleY() { return Camera_Angles[1]; }

// The camera sweep several steps share: counter 3 + 1, Camera_Distance +=
// delta, MapView_Redraw 2; when the count reaches `limit`, Redraw 2 again,
// Camera_Distance = `final`, counter 3 = 0, and true (the caller sets the
// next step).
bool Sweep(std::uint16_t delta, unsigned char limit, std::uint16_t final) {
    const unsigned char n = static_cast<unsigned char>(Counter(3) + 1);
    CamAdd(delta);
    Redraw();
    SetCounter(3, n);
    if (n < limit) return false;
    Redraw();
    Camera_Distance = final;
    SetCounter(3, 0);
    return true;
}

}  // namespace

// ===========================================================================
// The vtable's slots, the state and run dispatchers

// ===========================================================================
// The runs (Scena09_Runs entries), each a switch on the step 0x8034E5

// Run 6's hop into area 0x76 behind a camera effect (steps 0x21 / 0x2B):
// Effect_FindFree to 0x903850 and counter 3, the next step, a kind 0x13 at
// (x, angle Y, z) of life 4.
void CameraHop(unsigned char next, std::int32_t x, std::int32_t z) {
    const unsigned char slot = SH_CALL(Effect_FindFree)();
    at::kEffectSlot = slot;
    if (slot == 0xFF) return;
    SetCounter(3, slot);
    SetStep(next);
    Place13(at::kEffectSlot, x, AngleY(), z, 4);
}
// Its wait for that effect (steps 0x22 / 0x2C): the record counter 3 names
// free, counter 3 0, counter 0 1, Transition_Start(5), the pass flags, the
// next step.
void CameraLand(unsigned char next) {
    if (Effect(Counter(3))[0] != 0) return;
    SetCounter(3, 0);
    SetCounter(0, 1);
    Transition(5);
    SetPass(0x1F);
    SetStep(next);
}

// Run 11's countdown: the word timer 0x8034E6 less 1; true when it reaches 0.
bool Tick() {
    W(::bof3::Img(at::kTimer)) = static_cast<std::uint16_t>(W(::bof3::Img(at::kTimer)) - 1);
    return W(::bof3::Img(at::kTimer)) == 0;
}
// Run 11's effect slot cell 0x6BC730 (its own): Effect_FindFree stored there.
unsigned char Take11() {
    const unsigned char slot = SH_CALL(Effect_FindFree)();
    Scena09_Slot11 = slot;
    return slot;
}

// Run 15's ends: Flags_Set(row, flag), then the run's end.
void SetAndEnd(unsigned flag) {
    Set(flag);
    EndRun();
}
// Its camera sweeps (steps 0x4F..0x52): once counter 0 is `wait`, the sweep,
// then the next step.
void SweepAt(unsigned char wait, std::uint16_t delta, unsigned char limit, std::uint16_t final, unsigned char next) {
    if (Counter(0) != wait) return;
    if (!Sweep(delta, limit, final)) return;
    SetStep(next);
}

// ===========================================================================
// The object handlers (Scena09_Objects entries 1..4; the entry's (object,
// bits) are not read)

void ScenaSc9a_Inject() {
    if (bof3::WantsShadow("scena_sc9a")) scena_sc9a::SelfTest();
}

// Chapters 13 and 14 of the scenario code, 0x561DB0..0x567DC0 (round ten
// group SC13). docs/scena_sc13.md.
//
//   - Chapter 13 (vtable Scena13_Hooks 0x661788): slot 0 Scena13_Frame, a
//     tail jump through Scena13_States on the state byte; slot 1
//     Scena13_ObjectTrigger, a call through Scena13_Objects on the object's
//     +0x86; slots 2 and 3 the step and arrive hooks (x, z) answering in al;
//     slot 4 empty. State 0 Scena13_Start, state 1 Scena13_EnterArea (every
//     way out stores state 2), state 2 Scena13_Run, a tail jump through
//     Scena13_Runs on MoveScript_Var7 to the eight runs.
//   - Chapter 14 (vtable Scena14_Hooks 0x661820): the same five slots (slot 4
//     empty), Scena14_States (state 0 ScenaShared_State0, which seven other
//     chapters' state tables also hold), Scena14_EnterArea, Scena14_Run and
//     its seven runs, the object handlers, and the helpers the runs share.
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies. The tables
// are read in place and their entries called directly; the fuzz swaps them
// for recorders or stand-ins. No divergence: each function is a faithful
// replacement, except that a dispatcher whose index lies outside its table
// (a negative state or run, or one reading the next table) aborts where the
// original would jump through its neighbour - the project's rule for an index
// past a table (round nine, section 6).
#include "game/scena_sc13.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/record_views.h"
#include "game/scena_sc13_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace {

namespace at = scena_sc13::at;
using scena_sc13::ArgFn;
using scena_sc13::ObjectEntry;
using scena_sc13::PlaceFn;
using scena_sc13::VoidFn;

unsigned char& B(std::uint32_t a) { return *reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a)); }
std::uint16_t& W(std::uint32_t a) { return *reinterpret_cast<std::uint16_t*>(static_cast<std::uintptr_t>(a)); }
std::uint32_t& D(std::uint32_t a) { return *reinterpret_cast<std::uint32_t*>(static_cast<std::uintptr_t>(a)); }

unsigned Area() { return Game_AreaNumber; }
unsigned char FD() { return Cond_ByteFD; }
unsigned char Counter(unsigned k) { return B(::bof3::Img(scena_sc13::at::kCounters) + k); }
void SetCounter(unsigned k, unsigned char v) { B(::bof3::Img(scena_sc13::at::kCounters) + k) = v; }
void BumpCounter0() { SetCounter(0, static_cast<unsigned char>(Counter(0) + 1)); }
unsigned char Step() { return MoveScript_PartyRecords[0x65]; }
void SetStep(unsigned char v) { MoveScript_PartyRecords[0x65] = v; }
void SetRun(unsigned char v) { MoveScript_Var7 = v; }
void SetPass(unsigned char v) { Draw_PassFlags = v; }
unsigned char Request() { return Field_Request; }
void SetRequest(unsigned char v) { Field_Request = v; }
bool WaitClear() { return Transition_Busy == 0; }
void ScriptAnd(std::uint16_t v) { Field_ScriptFlags = static_cast<std::uint16_t>(Field_ScriptFlags & v); }
std::uint16_t Timer() { return W(::bof3::Img(scena_sc13::at::kTimer)); }
void SetTimer(std::uint16_t v) { W(::bof3::Img(scena_sc13::at::kTimer)) = v; }
// dec word [0x8034E6] / jne: true while the timer, one less, is not 0.
bool TimerRuns() {
    W(::bof3::Img(scena_sc13::at::kTimer)) = static_cast<std::uint16_t>(W(::bof3::Img(scena_sc13::at::kTimer)) - 1);
    return W(::bof3::Img(scena_sc13::at::kTimer)) != 0;
}

// The flag bits are read from 0x929ED0 afresh for every call, as the
// originals load the dword before each push.
unsigned char* Bits() { return reinterpret_cast<unsigned char*>(Chapter_FlagRow); }
unsigned char* Story() { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(::bof3::Img(Cond_Flags_addr) + 0xa0)); }
bool Flag(unsigned i) { return SH_CALL(Flags_Test)(Bits(), i) != 0; }   // test al, al
void Set(unsigned i) { SH_CALL(Flags_Set)(Bits(), i); }
void Clr(unsigned i) { SH_CALL(Flags_Clear)(Bits(), i); }

void Set40() { SH_CALL(ScriptFlags_Set40)(); }
void Clear40() { SH_CALL(ScriptFlags_Clear40)(); }
void Msg(unsigned short id) { SH_CALL(Msg_OpenScript)(id); }
void ChangeArea(unsigned area, int x, int z, unsigned flags) { SH_CALL(Field_ChangeArea)(area, x, z, flags); }
void CallA(unsigned n) { SH_CALL(Scenario_CallA)(n); }
void DropIn(unsigned n) { SH_CALL(Party_DropIn)(n); }
void Transition(unsigned char k) { SH_CALL(Transition_Start)(k); }
void Sound(unsigned id) { SH_CALL(Sound_PlayEffect)(id); }
void FadeOutStop(int f) { SH_CALL(Music_FadeOutStop)(f); }
void PartyPass() { SH_AT(VoidFn, scena_sc13::at::kPartyPass.v)(); }
void SetBit80() { SH_AT(VoidFn, scena_sc13::at::kSetBit80.v)(); }

// The chapter's message: Msg_OpenScript(id), the request byte 2.
void Say(unsigned short id) {
    Msg(id);
    SetRequest(2);
}

// A run's end with nothing between the call and the stores.
void EndRun() {
    Clear40();
    SetStep(0);
    SetRun(0);
}
void StopRun() {
    SetRun(0);
    SetStep(0);
}

unsigned char* Effect(unsigned slot) { return &B(::bof3::Img(scena_sc13::at::kEffects) + (slot & 0xFFu) * scena_sc13::at::kEffectStride); }
// Effect_FindFree, the slot stored to `cell` (the originals store it before
// testing it). Answers the slot.
unsigned char Take(std::uint32_t cell) {
    const unsigned char slot = SH_CALL(Effect_FindFree)();
    B(cell) = slot;
    return slot;
}
// A slot's record: +0 = 1, +5 = kind.
void Fx(unsigned slot, unsigned char kind) {
    unsigned char* const e = Effect(slot);
    e[0] = 1;
    e[5] = kind;
}
// test byte [effect(slot)], 1
bool EffectBusy(std::uint32_t cell) { return (Effect(B(cell))[0] & 1) != 0; }

// A table entry read in place, the index checked against the table.
std::uint32_t Entry(std::uint32_t table, int index, unsigned count, const char* who) {
    if (index < 0 || static_cast<unsigned>(index) >= count)
        bof3::Fatal("%s: index %d outside its table of %u at 0x%X", who, index, count, table);
    return D(table + 4 * static_cast<std::uint32_t>(index));
}

}  // namespace

// Exported with C linkage (the symbols.gen.h prototypes); no tail calls, so a
// Fatal's stack shows the dispatcher.
#define SC13_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// Chapter 13

// A hook's start of a run: ScriptFlags_Set40, the run and the step, al 1.
static unsigned char Hit(unsigned char run, unsigned char step) {
    Set40();
    SetRun(run);
    SetStep(step);
    return 1;
}
// (u16)(v - lo) < n, as the hooks test a cell's range.
static bool In(std::uint32_t v, unsigned lo, unsigned n) { return static_cast<std::uint16_t>(v - lo) < n; }

// ===========================================================================
// Chapter 14

void ScenaSc13_Inject() {
    if (bof3::WantsShadow("scena_sc13")) scena_sc13::SelfTest();
}

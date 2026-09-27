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
#include "game/scena_sc9a_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = scena_sc9a::at;
using scena_sc9a::ObjectEntry;
using scena_sc9a::PlaceFn;
using scena_sc9a::VoidFn;

unsigned char& B(std::uint32_t a) { return *reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a)); }
std::uint16_t& W(std::uint32_t a) { return *reinterpret_cast<std::uint16_t*>(static_cast<std::uintptr_t>(a)); }
std::uint32_t& D(std::uint32_t a) { return *reinterpret_cast<std::uint32_t*>(static_cast<std::uintptr_t>(a)); }
std::int32_t S32(std::uint32_t a) { return static_cast<std::int32_t>(D(a)); }

unsigned Area() { return W(at::kArea); }
unsigned char Counter(unsigned k) { return B(at::kCounters + k); }
void SetCounter(unsigned k, unsigned char v) { B(at::kCounters + k) = v; }
unsigned char Step() { return B(at::kStep); }
void SetStep(unsigned char v) { B(at::kStep) = v; }
void SetRun(unsigned char v) { B(at::kRun) = v; }
void SetPass(unsigned char v) { B(at::kPassFlags) = v; }
unsigned char Request() { return B(at::kRequest); }
void SetRequest(unsigned char v) { B(at::kRequest) = v; }
bool WaitClear() { return W(at::kWait) == 0; }
unsigned char FD() { return B(at::kCondFD); }
void SetFE(unsigned char v) { B(at::kCondFE) = v; }
// Field_ScriptFlags as the originals touch it: a word or / xor / and, or its
// low byte or.
void ScriptOr(std::uint16_t v) { W(at::kScriptFlags) = static_cast<std::uint16_t>(W(at::kScriptFlags) | v); }
void ScriptXor(std::uint16_t v) { W(at::kScriptFlags) = static_cast<std::uint16_t>(W(at::kScriptFlags) ^ v); }
void ScriptAnd(std::uint16_t v) { W(at::kScriptFlags) = static_cast<std::uint16_t>(W(at::kScriptFlags) & v); }
void ScriptOrByte(unsigned char v) { B(at::kScriptFlags) = static_cast<unsigned char>(B(at::kScriptFlags) | v); }
void Redraw() { B(at::kRedraw) = 2; }
void CamAdd(std::uint16_t v) { W(at::kCamDist) = static_cast<std::uint16_t>(W(at::kCamDist) + v); }

// The four counters 0x903848..B zeroed, in that order; counters 1..3 alone
// where a run's end leaves counter 0.
void ClearCounters() {
    for (unsigned k = 0; k < 4; ++k) SetCounter(k, 0);
}
void ClearCounters123() {
    for (unsigned k = 1; k < 4; ++k) SetCounter(k, 0);
}

// The flag bits are read from 0x929ED0 afresh for every call, as the
// originals load the dword before each push.
unsigned char* Bits() { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(D(at::kFlagBits))); }
bool Flag(unsigned i) { return SH_CALL(Flags_Test)(Bits(), i) != 0; }   // test al, al
void Set(unsigned i) { SH_CALL(Flags_Set)(Bits(), i); }
void Clr(unsigned i) { SH_CALL(Flags_Clear)(Bits(), i); }
// Flags_Set / Flags_Clear on a row handed as a literal (row 6 0x903FC0, the
// story flags 0x904030), not through 0x929ED0.
void SetAt(std::uint32_t row, unsigned i) { SH_CALL(Flags_Set)(&B(row), i); }
void ClrAt(std::uint32_t row, unsigned i) { SH_CALL(Flags_Clear)(&B(row), i); }

void Set40() { SH_CALL(ScriptFlags_Set40)(); }
void Clear40() { SH_CALL(ScriptFlags_Clear40)(); }
void Msg(unsigned short id) { SH_CALL(Msg_OpenScript)(id); }
void ChangeArea(unsigned area, int x, int z, unsigned flags) { SH_CALL(Field_ChangeArea)(area, x, z, flags); }
void CallA(unsigned n) { SH_CALL(Scenario_CallA)(n); }
void CallB(unsigned n) { SH_CALL(Scenario_CallB)(n); }
void DropIn(unsigned e) { SH_CALL(Party_DropIn)(e); }
void Kind2(unsigned char a) { SH_CALL(Kind2_Place)(a); }
void Transition(unsigned char k) { SH_CALL(Transition_Start)(k); }
void Sound(unsigned short id) { SH_CALL(Sound_PlayEffect)(id); }
void LoadStream(unsigned id) { SH_CALL(Sound_LoadStream)(id); }
bool StreamDone() { return SH_CALL(Sound_StreamDone)() != 0; }   // test eax, eax
void FadeOutStop(int frames) { SH_CALL(Music_FadeOutStop)(frames); }
void MusicPlay(unsigned track, int frames) { SH_CALL(Music_Play)(track, frames); }
void PartyPass() { SH_AT(VoidFn, at::kPartyPass)(); }
void PartyPlace(int x, int z, unsigned kind) { SH_AT(PlaceFn, at::kPartyPlace)(x, z, kind); }

// The chapter's message: Msg_OpenScript(id), the request byte 2.
void Say(unsigned short id) {
    Msg(id);
    SetRequest(2);
}

// A run's end: ScriptFlags_Clear40, then the step and the run 0.
void EndRun() {
    Clear40();
    SetStep(0);
    SetRun(0);
}
// The same, counters 1..3 zeroed between (counter 0 kept).
void EndRun123() {
    Clear40();
    ClearCounters123();
    SetStep(0);
    SetRun(0);
}

unsigned char* Effect(unsigned char slot) { return &B(at::kEffects + static_cast<std::uint32_t>(slot) * at::kEffectStride); }

// Effect_FindFree, the answer stored to `cell` first; a slot taken gets +0 =
// 1, +5 = kind (the originals index by the byte read back from the cell).
// Answers the slot.
unsigned char Spawn(std::uint32_t cell, unsigned char kind) {
    const unsigned char slot = SH_CALL(Effect_FindFree)();
    B(cell) = slot;
    if (slot == 0xFF) return slot;
    unsigned char* const e = Effect(B(cell));
    e[0] = 1;
    e[5] = kind;
    return slot;
}

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
std::int32_t AngleX() { return static_cast<std::int16_t>(W(at::kAngleX)); }
std::int32_t AngleY() { return static_cast<std::int16_t>(W(at::kAngleY)); }

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
    W(at::kCamDist) = final;
    SetCounter(3, 0);
    return true;
}

// A table entry read in place, the index checked against the table.
std::uint32_t Entry(std::uint32_t table, int index, unsigned count, const char* who) {
    if (index < 0 || static_cast<unsigned>(index) >= count)
        bof3::Fatal("%s: index %d outside its table of %u at 0x%X", who, index, count, table);
    return D(table + 4 * static_cast<std::uint32_t>(index));
}

// Scena09_EnterArea's body; every return leads to its state store.
void EnterAreaBody() {
    // area 0x25: flag 0x2E without 0x2F - run 0xF step 0xA, the call-table
    // entries by the selector
    if (Area() == 0x25 && Flag(0x2E) && !Flag(0x2F)) {
        Set40();
        SetAt(at::kRow6, 0x3E);
        ScriptOr(8);
        Set(0x2F);
        const unsigned v = B(at::kSelector) & 0x7F;
        SetPass(0);
        SetRun(0xF);
        SetStep(0xA);
        SetCounter(0, 0);
        switch (v) {
        case 7: CallB(2); CallA(0xA); break;
        case 8: CallB(2); CallB(7); CallA(0xB); break;
        case 9: CallB(2); CallB(8); CallA(0xB); break;
        case 10: CallB(2); CallA(0xC); break;
        case 13: CallB(7); CallA(0xA); break;
        case 14: CallB(8); CallA(0xA); break;
        case 16: CallB(7); CallB(8); CallA(0xB); break;
        case 17: CallB(7); CallA(0xC); break;
        case 18: CallB(8); CallA(0xC); break;
        default: break;   // 11, 12, 15 and the rest: none
        }
    }
    // area 0x27: the kind-2 sprite unless flag 0x30; flags 0x36, 0x31, not
    // 0x34 and Cond_ByteFD 0 - run 0xF step 0x46
    if (Area() == 0x27) {
        if (!Flag(0x30)) Kind2(2);
        if (Flag(0x36) && Flag(0x31) && FD() == 0 && !Flag(0x34)) {
            Set40();
            ScriptOr(8);
            SetCounter(0, 0);
            SetStep(0x46);
            SetRun(0xF);
        }
    }
    // area 0x29: flag 0x32 set the first time (run 0xF step 0x3C); counter 2
    // at 1 with flag 0x38 and Cond_ByteFD 0 - run 0xF step 0x6E
    if (Area() == 0x29) {
        if (!Flag(0x32)) {
            Set(0x32);
            SetPass(0);
            Set40();
            SetCounter(0, 0);
            SetStep(0x3C);
            SetRun(0xF);
        }
        if (Counter(2) == 1 && Flag(0x38) && FD() == 0) {
            ScriptOr(8);
            SetPass(0);
            Set40();
            SetCounter(0, 0);
            SetStep(0x6E);
            SetRun(0xF);
        }
    }
    // area 0x35: the kind-2 sprite unless flag 0x2E
    if (Area() == 0x35 && !Flag(0x2E)) Kind2(2);
    // area 0x37: flag 4 without 5 - the camera out, run 3, entries by the selector
    if (Area() == 0x37 && Flag(4) && !Flag(5)) {
        Set40();
        const unsigned v = B(at::kSelector) & 0x7F;
        CamAdd(0xA80);
        Redraw();
        SetPass(0);
        SetRun(3);
        SetStep(0);
        SetCounter(0, 0);
        switch (v) {
        case 7: CallB(1); CallA(2); break;
        case 8: CallB(2); CallA(4); break;
        case 10: CallB(2); CallA(3); break;
        case 13: CallB(0); CallA(2); break;
        case 15: CallB(0); CallA(3); break;
        default: break;   // 9, 11, 12, 14 and the rest: none
        }
    }
    // area 0x45: flag 0 clear - run 1, entries by the selector (read as a dword)
    if (Area() == 0x45 && !Flag(0)) {
        const unsigned v = D(at::kSelector) & 0x7F;
        SetPass(0);
        SetRun(1);
        SetStep(0);
        SetCounter(0, 0);
        if (v == 7) {
            CallB(0);
            CallA(0);
        } else if (v == 0xF) {
            CallB(0);
            CallA(1);
        }
    }
    // area 0x31: four tests in turn
    if (Area() == 0x31) {
        if (Flag(2) && !Flag(3)) {
            Kind2(2);
            const unsigned v = D(at::kSelector) & 0x7F;
            if (v == 7) {
                CallB(1);
                CallA(2);
            } else if (v == 0xA) {
                CallB(2);
                CallA(3);
            } else if (v == 0xF) {
                CallB(0);
                CallA(3);
            }
        }
        if (Flag(6) && !Flag(7)) {
            SetPass(0);
            SetRun(4);
            SetStep(0);
            ClearCounters();
        }
        if (Counter(2) == 1) {
            CamAdd(0xB80);
            SetPass(0);
            Redraw();
        }
        if (FD() == 1 && !Flag(0x1E)) Clr(0x11);
    }
    // area 0x64: nothing more unless counter 2 is 2; then flag 0x38 with
    // Cond_ByteFD 2 - run 0xF step 0x78
    if (Area() == 0x64) {
        if (Counter(2) != 2) return;
        if (Flag(0x38) && FD() == 2) {
            SetPass(0);
            Set40();
            SetCounter(0, 0);
            SetStep(0x78);
            SetRun(0xF);
        }
    }
    // area 0x76: nothing more unless Cond_ByteFD is 4; effects 0x96 / 0x95 by
    // flag 0x13 and ObjTrio +0x3C (read afresh at each test)
    if (Area() == 0x76) {
        if (FD() != 4) return;
        if (!Flag(0x13)) {
            Set40();
            SetRun(7);
            SetStep(0);
            ClearCounters();
            Spawn(at::kEffectSlot, 0x96);
            if (S32(at::kObjTrioY) <= 0x2000000) Spawn(at::kEffectSlot, 0x95);
        } else if (S32(at::kObjTrioY) <= 0x2000000) {
            Spawn(at::kEffectSlot, 0x96);
            if (S32(at::kObjTrioY) <= 0x2000000) Spawn(at::kEffectSlot, 0x95);
        }
    }
    // area 0x77: nothing more unless Cond_ByteFD is 2; flag 0x3D without 0x16
    // - run 8 step 0x1E, flag 0x16
    if (Area() == 0x77) {
        if (FD() != 2) return;
        if (Flag(0x3D) && !Flag(0x16)) {
            ScriptOr(8);
            SetPass(0);
            Set40();
            SetRun(8);
            SetStep(0x1E);
            ClearCounters();
            Set(0x16);
        }
    }
    // the areas that end a run: the counters 0
    const unsigned area = Area();
    if (area == 0x2D || area == 0x41 || area == 0x57 || area == 0x10 || area == 0x73) ClearCounters();
}

}  // namespace

// Exported with C linkage (the symbols.gen.h prototypes); no tail calls, so a
// Fatal's stack shows the dispatcher.
#define SC9A_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// The vtable's slots, the state and run dispatchers

// original 0x553B30: slot 0, Field_ModeDispatch's call every field frame - a
// tail jump through Scena09_States on the s8 state 0x8034E2: 0 0x5646B0
// (group SC13's block), 1 Scena09_EnterArea, 2 Scena09_Run.
SC9A_EXPORT void __cdecl Scena09_Frame(void) {
    const int state = static_cast<signed char>(B(at::kState));
    reinterpret_cast<VoidFn>(static_cast<std::uintptr_t>(Entry(at::kStates, state, at::kStateCount, "Scena09_Frame")))();
}

// original 0x554270: state 2, a tail jump through Scena09_Runs on the s8 run
// MoveScript_Var7: 0, 12, 13 a bare ret (0x437CC0), 1..11 and 14..16 the runs.
SC9A_EXPORT void __cdecl Scena09_Run(void) {
    const int run = static_cast<signed char>(B(at::kRun));
    reinterpret_cast<VoidFn>(static_cast<std::uintptr_t>(Entry(at::kRuns, run, at::kRunCount, "Scena09_Run")))();
}

// original 0x5570D0: slot 1, called by 0x56D6D0 with the object that
// triggered: Scena09_Objects[object +0x86] (object, the flag bits' pointer).
// The entry's eax is the original's answer; 0x56D6D0 does not read it.
SC9A_EXPORT void __cdecl Scena09_ObjectTrigger(unsigned char* object) {
    const std::uint32_t bits = D(at::kFlagBits);
    const unsigned index = object[0x86];
    reinterpret_cast<ObjectEntry>(static_cast<std::uintptr_t>(
        Entry(at::kObjects, static_cast<int>(index), at::kObjectCount, "Scena09_ObjectTrigger")))(object, bits);
}

// original 0x553B40: state 1, the chapter's area entry - by the area, the
// flags, Cond_ByteFD and counter 2, a run started, the kind-2 sprite, the
// camera, effects, call-table entries by the selector; every exit stores
// state 2.
SC9A_EXPORT void __cdecl Scena09_EnterArea(void) {
    EnterAreaBody();
    B(at::kState) = 2;
}

// ===========================================================================
// The runs (Scena09_Runs entries), each a switch on the step 0x8034E5

// original 0x554280: run 1.
SC9A_EXPORT void __cdecl Scena09_Run1(void) {
    switch (Step()) {
    case 0:
        SetStep(1);
        Set(0);
        ChangeArea(0x45, 0x1F8000, 0x4C0000, 0x86);
        break;
    case 1:
        SetPass(0x1F);
        SetStep(2);
        break;
    case 2:
        if (Counter(0) != 0x11) break;
        Set(1);
        EndRun();
        break;
    default: break;
    }
}

// original 0x554300: run 2.
SC9A_EXPORT void __cdecl Scena09_Run2(void) {
    switch (Step()) {
    case 0:
        if ((B(at::kScriptFlags2) & 7) != 0) break;
        ScriptOrByte(8);
        Kind2(1);
        DropIn(5);
        SetStep(2);
        break;
    case 2:
        if (Counter(0) != 0x12) break;
        SetCounter(0, 0);
        SetCounter(2, 0);
        SetStep(3);
        Set(2);
        ChangeArea(0x31, 0x540000, 0x640000, 0x86);
        break;
    case 3:
        if (B(at::kHold) != 0) break;
        SetCounter(0, 1);
        SetStep(5);
        break;
    case 5:
        if (Counter(0) != 0x11) break;
        FadeOutStop(0x1E);
        LoadStream(3);
        SetStep(6);
        break;
    case 6:
        if (!StreamDone()) break;
        SetCounter(0, 0x12);
        MusicPlay(0x10, 8);
        SetStep(7);
        break;
    case 7:
        if (Counter(0) != 0x12) break;
        ScriptXor(8);
        Set(3);
        Clr(0x25);
        EndRun();
        break;
    case 0xA:
        SetStep(0xB);
        Set(4);
        ChangeArea(0x31, 0x3F8000, 0x140000, 0x87);
        break;
    case 0xB:
        if (Counter(0) != 0xB) break;
        Set(4);
        EndRun();
        break;
    default: break;
    }
}

// original 0x5544D0: run 3.
SC9A_EXPORT void __cdecl Scena09_Run3(void) {
    switch (Step()) {
    case 0:
        if (!WaitClear()) break;
        Msg(0);
        SetRequest(2);
        Kind2(0);
        DropIn(0);
        SetStep(1);
        break;
    case 1:
        if (Request() == 2) break;
        Transition(5);
        SetStep(2);
        SetPass(0x1F);
        break;
    case 2:
        if (!WaitClear()) break;
        SetCounter(0, 1);
        SetStep(3);
        break;
    case 3: {
        const unsigned char slot = SH_CALL(Effect_FindFree)();
        B(at::kEffectSlot) = slot;
        if (slot == 0xFF) break;
        SetStep(4);
        Place13(B(at::kEffectSlot), -0x252, AngleY(), 0xA0, 0x14);
        break;
    }
    case 4: {
        if (Counter(0) != 2) break;
        const unsigned char slot = SH_CALL(Effect_FindFree)();
        B(at::kEffectSlot) = slot;
        if (slot == 0xFF) break;
        SetCounter(3, slot);
        SetStep(5);
        Place13(B(at::kEffectSlot), -0x35A, AngleY(), 0x3FA, 0x8C);
        break;
    }
    case 5:
        if (!Sweep(0xFF00, 0xD, 0xFD80)) break;
        SetCounter(0, 3);
        SetStep(7);
        break;
    case 7:
        if (Counter(0) != 4) break;
        SetStep(8);
        Set(5);
        ChangeArea(0x37, 0x250000, 0x130000, 0x81);
        break;
    case 8:
        if (Counter(0) != 0x16) break;
        Transition(0xD);
        SetStep(9);
        break;
    case 9:
        if (!WaitClear()) break;
        SetPass(0);
        SetCounter(0, 0x17);
        Msg(0x14);
        SetRequest(2);
        SetStep(0xA);
        FadeOutStop(0x1E);
        break;
    case 0xA:
        if (Request() == 2) break;
        LoadStream(3);
        SetStep(0xB);
        break;
    case 0xB:
        if (!StreamDone()) break;
        CallA(5);
        SetStep(0xC);
        Set(6);
        break;
    case 0xC:
        if (Request() != 0) break;
        SetStep(0xD);
        ChangeArea(0x37, 0x160000, 0x130000, 0x82);
        MusicPlay(0x79, 8);
        break;
    case 0xD:
        SetPass(0x1F);
        Clear40();
        SetCounter(0, 0);
        SetStep(0);
        SetRun(0);
        break;
    default: break;   // 6
    }
}

// original 0x5547F0: run 4.
SC9A_EXPORT void __cdecl Scena09_Run4(void) {
    switch (Step()) {
    case 0:
        SetStep(1);
        Set(7);
        ChangeArea(0x31, 0x490000, 0x238000, 0x88);
        break;
    case 1:
        SetStep(2);
        SetPass(0x1F);
        break;
    case 2:
        if (Counter(0) != 1) break;
        EndRun();
        break;
    case 5:
    case 6: {
        // ObjTrio +0x3C at least 0x8000000: set one of flags 0x27 / 0x26 and
        // go on only when the other is set too
        const bool five = Step() == 5;
        if (S32(at::kObjTrioY) < 0x8000000) {
            EndRun();
            break;
        }
        Set(five ? 0x27 : 0x26);
        if (!Flag(five ? 0x26 : 0x27)) {
            EndRun();
            break;
        }
        SetCounter(0, 5);
        SetCounter(1, five ? 1 : 2);
        SetStep(0xA);
        break;
    }
    case 0xA:
        DropIn(9);
        SetStep(0xB);
        break;
    case 0xB:
        if (Counter(0) != 7) break;
        SetCounter(2, 1);
        SetStep(0xC);
        ChangeArea(0x31, 0x248000, 0x210000, 0x8A);
        break;
    case 0xC: {
        Kind2(3);
        const unsigned char slot = SH_CALL(Effect_FindFree)();
        B(at::kEffectSlot) = slot;
        if (slot == 0xFF) break;
        SetStep(0xD);
        Place13(B(at::kEffectSlot), -0x268, AngleY(), 0x10E, 2);
        break;
    }
    case 0xD:
        if (!WaitClear()) break;
        SetPass(0x1F);
        SetStep(0xE);
        break;
    case 0xE:
        if (!Sweep(0xFFA4, 0x20, 0)) break;
        SetCounter(0, 8);
        SetStep(0xF);
        break;
    case 0xF:
        if (Counter(0) != 9) break;
        Set(0x3E);
        SetStep(0x10);
        break;
    case 0x10: {
        if (Counter(0) != 0xA) break;
        SetCounter(2, 0);
        SetStep(0x11);
        Set(8);
        const unsigned char which = Counter(1);
        if (which == 1)
            ChangeArea(0x31, 0x80000, 0x378000, 0x8B);
        else if (which == 2)
            ChangeArea(0x31, 0x1A0000, 0x2F0000, 0x8C);
        break;
    }
    case 0x11:
        if (Counter(0) != 5) break;
        ScriptAnd(0xFFBF);
        SetCounter(1, 0);
        EndRun();
        break;
    case 0x1E:
        SH_CALL(MoveCmd_TestFB)(0xB, 0x54);
        DropIn(0xD);
        SetStep(0x1F);
        break;
    case 0x1F:
        if (Counter(0) != 7) break;
        ScriptOrByte(8);
        SetStep(0x20);
        ChangeArea(0x75, 0x3F0000, 0x58000, 0x80);
        break;
    case 0x20:
        if (Counter(0) != 5) break;
        ScriptOrByte(8);
        SetStep(0x21);
        ChangeArea(0x75, 0xE0000, 0x148000, 0x81);
        break;
    case 0x21:
        Kind2(0);
        SetStep(0x22);
        break;
    case 0x22:
        if (Counter(0) != 0x11) break;
        if (B(at::kHold) != 0) break;
        Set(0xB);
        Clear40();
        ScriptXor(8);
        ClearCounters123();
        SetStep(0);
        SetRun(0);
        break;
    default: break;   // 3, 4, 7..9, 0x12..0x1D
    }
}

// original 0x554C70: run 5.
SC9A_EXPORT void __cdecl Scena09_Run5(void) {
    switch (Step()) {
    case 0:
        if (Request() == 2) break;
        SetCounter(0, 1);
        DropIn(0xC);
        SetStep(1);
        break;
    case 1:
        if (Counter(0) != 9) break;
        Set(0x3F);
        EndRun123();
        break;
    case 5:
        if (Request() == 2) break;
        SetCounter(0, 1);
        SetStep(6);
        break;
    case 6:
        if (Counter(0) != 2) break;
        Clear40();
        ClearCounters();
        SetStep(0);
        SetRun(0);
        break;
    default: break;   // 2..4
    }
}

// Run 6's hop into area 0x76 behind a camera effect (steps 0x21 / 0x2B):
// Effect_FindFree to 0x903850 and counter 3, the next step, a kind 0x13 at
// (x, angle Y, z) of life 4.
void CameraHop(unsigned char next, std::int32_t x, std::int32_t z) {
    const unsigned char slot = SH_CALL(Effect_FindFree)();
    B(at::kEffectSlot) = slot;
    if (slot == 0xFF) return;
    SetCounter(3, slot);
    SetStep(next);
    Place13(B(at::kEffectSlot), x, AngleY(), z, 4);
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

// original 0x554D40: run 6.
SC9A_EXPORT void __cdecl Scena09_Run6(void) {
    switch (Step()) {
    case 0: Say(0xA); SetStep(1); break;
    case 2: Say(0xB); SetStep(1); break;
    case 3: Say(0xD); SetStep(1); break;
    case 5: Say(0xC); SetStep(1); break;
    case 6: Say(0xF); SetStep(1); break;
    case 1:
        if (Request() == 2) break;
        EndRun();
        break;
    case 7:
        Say(0x2B);
        SetStep(8);
        break;
    case 8: {
        if (Request() == 2) break;
        const unsigned char c = Counter(0);
        if (c == 0xA)
            SetStep(9);
        else if (c == 0x14)
            SetStep(0xA);
        break;
    }
    case 9:
        SetCounter(0, 0);
        EndRun();
        break;
    case 0xA:
        Set(0x11);
        Clear40();
        SetCounter(0, 0);
        ClearCounters123();
        SetStep(0);
        SetRun(0);
        break;
    case 0x14:
        DropIn(3);
        SetStep(0x15);
        break;
    case 0x15:
        if (Counter(0) != 1) break;
        SetStep(0x16);
        ChangeArea(0x75, 0x38000, 0x158000, 0x84);
        break;
    case 0x16:
        if (Counter(0) != 2) break;
        SetStep(0x17);
        ChangeArea(0x75, 0x560000, 0x2E0000, 0x85);
        break;
    case 0x17:
        if (Counter(0) != 0xF) break;
        SetStep(0x18);
        ChangeArea(0x75, 0x38000, 0x150000, 0x86);
        break;
    case 0x18:
        SetFE(1);
        Set(0xC);
        SetStep(0x19);
        break;
    case 0x19:
        if (Counter(0) != 1) break;
        SetCounter(0, 0);
        SetStep(0x1A);
        ChangeArea(0x75, 0x560000, 0x2E0000, 0x87);
        break;
    case 0x1A:
        if (Counter(0) != 4) break;
        EndRun123();
        break;
    case 0x1E:
        DropIn(8);
        SetStep(0x1F);
        break;
    case 0x1F:
        if (Counter(0) != 1) break;
        Transition(0);
        SetStep(0x20);
        break;
    case 0x20:
        if (!WaitClear()) break;
        SetPass(0);
        SetStep(0x21);
        ChangeArea(0x76, 0x70000, 0x118000, 0x83);
        break;
    case 0x21: CameraHop(0x22, -0x3B2, 0x38C); break;
    case 0x22: CameraLand(0x23); break;
    case 0x23:
        if (!WaitClear()) break;
        SetFE(2);
        Set(0xD);
        SetStep(0x24);
        break;
    case 0x24:
        if (Counter(0) != 2) break;
        SetPass(0x1F);
        SetCounter(0, 0);
        SetStep(0x25);
        ChangeArea(0x75, 0x560000, 0x2B0000, 0x89);
        break;
    case 0x25:
        if (Counter(0) != 9) break;
        EndRun123();
        break;
    case 0x28:
        DropIn(0xA);
        SetStep(0x29);
        break;
    case 0x29:
        if (Counter(0) != 1) break;
        Transition(4);
        SetStep(0x2A);
        break;
    case 0x2A:
        if (!WaitClear()) break;
        SetPass(0);
        SetStep(0x2B);
        ChangeArea(0x76, 0x88000, 0x78000, 0x83);
        break;
    case 0x2B: CameraHop(0x2C, -0x2AA, 0x31E); break;
    case 0x2C: CameraLand(0x2D); break;
    case 0x2D:
        if (!WaitClear()) break;
        SetFE(3);
        Set(0xF);
        SetStep(0x2E);
        break;
    case 0x2E:
        if (Counter(0) != 2) break;
        SetCounter(0, 0);
        SetStep(0x2F);
        ChangeArea(0x75, 0x530000, 0x300000, 0x8B);
        break;
    case 0x2F:
        if (Counter(0) != 8) break;
        EndRun123();
        break;
    case 0x32:
        Say(0x2B);
        SetStep(0x33);
        break;
    case 0x33: {
        if (Request() == 2) break;
        const unsigned char c = Counter(0);
        if (c == 0xA) {
            DropIn(0xC);
            SetStep(0x34);
        } else if (c == 0x14) {
            SetStep(0x35);
        }
        break;
    }
    case 0x34:
        if (Counter(0) != 1) break;
        EndRun();
        break;
    case 0x35:
        Transition(0);
        SetStep(0x36);
        break;
    case 0x36:
        if (!WaitClear()) break;
        SetCounter(0, 1);
        SetPass(0);
        SetStep(0x37);
        ChangeArea(0x76, 0x78000, 0x38000, 0x83);
        break;
    case 0x37:
        Transition(5);
        SetPass(0x1F);
        SetStep(0x38);
        break;
    case 0x38:
        if (!WaitClear()) break;
        SetFE(4);
        Set(0x11);
        Set(0x1D);
        SetStep(0x39);
        break;
    case 0x39:
        if (Counter(0) != 2) break;
        SetCounter(0, 0);
        SetStep(0x3A);
        ChangeArea(0x75, 0x530000, 0x290000, 0x8D);
        break;
    case 0x3A:
        if (Counter(0) != 7) break;
        EndRun123();
        break;
    case 0x64:
        if (Request() == 2) break;
        Msg(0x1B);
        SetRequest(2);
        SetStep(0x65);
        break;
    case 0x65: {
        const unsigned char c = Counter(0);
        if (c == 0x64) {
            SetCounter(0, 0);
            SetStep(0x66);
            Transition(0xD);
            PartyPass();
            FadeOutStop(0x1E);
        } else if (c == 0x65) {
            SetCounter(0, 0);
            EndRun();
        }
        break;
    }
    case 0x66:
        LoadStream(0);
        SetStep(0x67);
        break;
    case 0x67:
        if (!WaitClear()) break;
        SetPass(0);
        if (!StreamDone()) break;
        SetStep(0x68);
        break;
    case 0x68:
        Transition(0xE);
        SetPass(0x1F);
        SetStep(0x69);
        break;
    case 0x69:
        if (!WaitClear()) break;
        MusicPlay(0x61, 8);
        EndRun();
        break;
    default: break;   // 4, 0xB..0x13, 0x1B..0x1D, 0x26, 0x27, 0x30, 0x31, 0x3B..0x63
    }
}

// original 0x5555D0: run 7.
SC9A_EXPORT void __cdecl Scena09_Run7(void) {
    switch (Step()) {
    case 0:
        if (B(at::kMember2Byte) == 2) break;
        DropIn(0);
        SetStep(1);
        break;
    case 1:
        if (Request() == 2) break;
        Set(0x13);
        EndRun();
        break;
    case 5:
        if ((W(at::kInputPressed) & W(at::kMenuButton)) != 0) break;
        if (Request() != 0) break;
        SetCounter(0, 0);
        SetStep(6);
        ChangeArea(0x76, 0x490000, 0x188000, 0x81);
        break;
    case 6:
        if (Counter(0) != 1) break;
        if (Request() == 2) break;
        SetCounter(0, 0);
        EndRun();
        break;
    default: break;   // 2..4
    }
}

// original 0x5556D0: run 8.
SC9A_EXPORT void __cdecl Scena09_Run8(void) {
    switch (Step()) {
    case 0: Say(0x23); SetStep(0xA); break;
    case 1: Say(0x24); SetStep(0xA); break;
    case 2: Say(0x25); SetStep(0xA); break;
    case 3: Say(0x26); SetStep(0xA); break;
    case 4: Say(0x27); SetStep(0xA); break;
    case 5: Say(0x21); SetStep(0xA); break;
    case 6: Say(0x22); SetStep(0xA); break;
    case 0x19: Say(0x2C); SetStep(0xA); break;
    case 0xA:
        if (Request() == 2) break;
        EndRun();
        break;
    case 0xF:
        DropIn(0);
        SetStep(0x10);
        break;
    case 0x10:
        if (Counter(0) != 1) break;
        Set(0x12);
        EndRun123();
        break;
    case 0x14:
        Sound(0x206);
        SetFE(3);
        DropIn(2);
        SetStep(0x15);
        break;
    case 0x15:
        if (Counter(0) != 1) break;
        EndRun();
        break;
    case 0x1E:
        SetStep(0x1F);
        ChangeArea(0x77, 0x98000, 0x348000, 0x83);
        break;
    case 0x1F:
        ScriptOrByte(8);
        Transition(1);
        SetPass(0x1F);
        Sound(0x207);
        SetFE(3);
        SetStep(0x20);
        break;
    case 0x20:
        if (!WaitClear()) break;
        PartyPlace(0x118000, 0x348000, 0x35);
        SetCounter(0, 2);
        SetStep(0x21);
        break;
    case 0x21: {
        // each of the three members listed as 7: an effect of kind 6 at the
        // member's +0x2E / +0x30 words, its slot to the member's +0xB
        if (Counter(0) != 3) break;
        unsigned char* member = &B(0x802D40);   // ObjTrio
        for (unsigned i = 0; i < 3; ++i, member += 0x14C) {
            if (B(at::kPartyList + i) != 7) continue;
            Sprite_Current = member;
            const unsigned char slot = SH_CALL(Effect_FindFree)();
            unsigned char* const s = Sprite_Current;   // read again after the call
            s[0xB] = slot;
            if (s[0xB] == 0xFF) continue;
            unsigned char* const e = Effect(s[0xB]);
            e[0] = 1;
            e[5] = 6;
            e[6] = 2;
            *reinterpret_cast<std::uint32_t*>(e + 0xC) = 0;
            *reinterpret_cast<std::uint32_t*>(e + 0x10) = 0x24;
            *reinterpret_cast<std::uint16_t*>(e + 0x2E) = *reinterpret_cast<const std::uint16_t*>(s + 0x2E);
            SetCounter(0, 4);
            *reinterpret_cast<std::uint16_t*>(e + 0x30) = *reinterpret_cast<const std::uint16_t*>(s + 0x30);
        }
        SetStep(0x22);
        break;
    }
    case 0x22: {
        // a message per member for the members listed as 2, 4, 8, 5 (in that
        // order of kinds)
        if (Counter(0) != 6) break;
        static const unsigned char kKind[4] = {2, 4, 8, 5};
        static const unsigned short kMessage[4] = {0x30, 0x2F, 0x2E, 0x2D};
        for (unsigned k = 0; k < 4; ++k)
            for (unsigned i = 0; i < 3; ++i)
                if (B(at::kPartyList + i) == kKind[k]) Msg(kMessage[k]);
        SetRequest(2);
        SetStep(0x23);
        break;
    }
    case 0x23:
        if (Counter(0) != 7) break;
        SetFE(0);
        Sound(0x208);
        SetStep(0x24);
        break;
    case 0x24:
        if (Counter(0) != 0xA) break;
        SetCounter(0, 0);
        PartyPlace(0xD8000, 0x348000, 0x35);
        SH_CALL(Field_StartEventBattle)(0x35);
        SetStep(0x25);
        break;
    case 0x26:
        D(at::kKind2X) = D(at::kObjTrioX);
        D(at::kKind2Z) = D(at::kObjTrioZ);
        W(at::kF3Divisor) = 0x20;
        SetStep(0x27);
        break;
    case 0x27:
        if (B(at::kHold) != 0) break;
        DropIn(4);
        SetStep(0x28);
        break;
    case 0x28:
        if (Counter(0) != 1) break;
        Set(0x17);
        ScriptXor(8);
        EndRun();
        break;
    default: break;   // 7..9, 0xB..0xE, 0x11..0x13, 0x16..0x18, 0x1A..0x1D, 0x25
    }
}

// original 0x555B80: run 9.
SC9A_EXPORT void __cdecl Scena09_Run9(void) {
    switch (Step()) {
    case 0:
        if (B(at::kMember2Byte) == 3) break;
        ScriptOrByte(8);
        DropIn(5);
        Kind2(1);
        SetStep(1);
        break;
    case 1:
        if (Counter(0) != 2) break;
        DropIn(6);
        SetStep(2);
        break;
    case 2:
        if (Counter(0) != 0x25) break;
        Spawn(at::kEffectSlot, 0x9A);
        SetStep(3);
        break;
    case 3:
        if (Counter(0) != 0x28) break;
        PartyPlace(0xE8000, 0xE8000, 0x36);
        SH_CALL(Field_StartEventBattle)(0x36);
        SetStep(4);
        break;
    case 4:
        if (Counter(0) != 0x32) break;
        SetCounter(0, 0);
        DropIn(9);
        Transition(4);
        SetStep(5);
        break;
    case 5:
        if (!WaitClear()) break;
        SetPass(0);
        SetStep(6);
        Set(0x1E);
        ChangeArea(0x77, 0xA8000, 0xB0000, 0x87);
        B(at::kMusicCurrent) = 0x61;
        break;
    case 6:
        if (Counter(0) != 1) break;
        Transition(5);
        SetPass(0x1F);
        Sound(0x20C);
        SetStep(7);
        break;
    case 7:
        if (!WaitClear()) break;
        SetCounter(0, 2);
        SetStep(8);
        break;
    case 8:
        if (Counter(0) != 0xE) break;
        ScriptXor(8);
        EndRun();
        break;
    case 0x14:
        Say(0x4B);
        SetStep(0x15);
        break;
    case 0x15:
        if (Request() == 2) break;
        if (Counter(0) == 0xA) {
            DropIn(8);
            SetStep(0x16);
            break;
        }
        EndRun();
        break;
    case 0x16:
        if (Counter(0) != 0x19) break;
        Set(0x2C);
        EndRun();
        break;
    case 0x1E:
        Say(0x4C);
        SetStep(0x1F);
        break;
    case 0x1F:
        if (Request() == 2) break;
        EndRun();
        break;
    default: break;   // 9..0x13, 0x17..0x1D
    }
}

// original 0x556C10: run 10 - at step 0, once the request byte is not 2,
// Party_DropIn(1) and the run and the step 0 (no ScriptFlags_Clear40).
SC9A_EXPORT void __cdecl Scena09_Run10(void) {
    if (Step() != 0) return;
    if (Request() == 2) return;
    DropIn(1);
    SetRun(0);
    SetStep(0);
}

// Run 11's countdown: the word timer 0x8034E6 less 1; true when it reaches 0.
bool Tick() {
    W(at::kTimer) = static_cast<std::uint16_t>(W(at::kTimer) - 1);
    return W(at::kTimer) == 0;
}
// Run 11's effect slot cell 0x6BC730 (its own): Effect_FindFree stored there.
unsigned char Take11() {
    const unsigned char slot = SH_CALL(Effect_FindFree)();
    B(at::kSlot11) = slot;
    return slot;
}

// original 0x556C40: run 11.
SC9A_EXPORT void __cdecl Scena09_Run11(void) {
    switch (Step()) {
    case 0:
        DropIn(2);
        SetCounter(0, 0x20);
        SetStep(1);
        break;
    case 1:
        if (Counter(0) != 0x25) break;
        if (Take11() == 0xFF) break;
        SetStep(2);
        Place13(B(at::kSlot11), -0x3C8, AngleY(), 0xF8, 0x30);
        break;
    case 2:
        if (Counter(0) != 0x26) break;
        if (Take11() != 0xFF) {
            // kind 0x2B at (0x238000, 0x1F8000), its +0x3C the ground there
            unsigned char* const e = Effect(B(at::kSlot11));
            e[0] = 1;
            e[5] = 0x2B;
            *reinterpret_cast<std::int32_t*>(e + 0x34) = 0x238000;
            *reinterpret_cast<std::int32_t*>(e + 0x38) = 0x1F8000;
            const std::int32_t x = *reinterpret_cast<const std::int32_t*>(e + 0x34);
            const std::int32_t z = *reinterpret_cast<const std::int32_t*>(e + 0x38);
            const short ground = static_cast<short>(SH_CALL(AreaMap_Elevation)(x, z));
            *reinterpret_cast<std::int32_t*>(Effect(B(at::kSlot11)) + 0x3C) =
                static_cast<std::int32_t>(static_cast<std::uint32_t>(static_cast<std::int32_t>(ground)) << 16);
        }
        W(at::kTimer) = 0x5F;
        SetStep(3);
        break;
    case 3:
        if (!Tick()) break;
        SetStep(4);
        SetCounter(0, static_cast<unsigned char>(Counter(0) + 1));
        break;
    case 4:
        if (Counter(0) != 0x28) break;
        if (Take11() == 0xFF) break;
        SetStep(5);
        Place13(B(at::kSlot11), AngleX(), AngleY(), -0x144, 0x28);
        break;
    case 5:
        if (Counter(0) != 0x29) break;
        W(at::kTimer) = 0x78;
        SetStep(6);
        break;
    case 6:
        if (!Tick()) break;
        if (Take11() == 0xFF) break;
        SetStep(7);
        Place13(B(at::kSlot11), AngleX(), AngleY(), 0x200, 0x20);
        break;
    case 7:
        // the last effect gone (bit 0 of its +0): kind 0x13 at (-0x2AA, angle
        // Y, Cond_AngleFB), counter 0 + 1
        if ((Effect(B(at::kSlot11))[0] & 1) != 0) break;
        if (Take11() == 0xFF) break;
        SetStep(8);
        Place13(B(at::kSlot11), -0x2AA, AngleY(), static_cast<std::int16_t>(W(at::kAngleFB)), 0x7C);
        SetCounter(0, static_cast<unsigned char>(Counter(0) + 1));
        break;
    case 8:
        if (Counter(0) != 0x32) break;
        W(at::kTimer) = 0x5A;
        SetStep(9);
        break;
    case 9:
        if (!Tick()) break;
        SetStep(0xA);
        SetCounter(0, static_cast<unsigned char>(Counter(0) + 1));
        break;
    case 0xA:
        if (Counter(0) != 0x35) break;
        W(at::kTimer) = 0x1E;
        SetStep(0xB);
        break;
    case 0xB:
        if (!Tick()) break;
        SetStep(0xC);
        SetCounter(0, static_cast<unsigned char>(Counter(0) + 1));
        break;
    case 0xC:
        if (Counter(0) != 0x37) break;
        SetStep(0xD);
        break;
    case 0xD:
        if (Counter(0) != 0x3B) break;
        D(at::kKind2X) = D(at::kObjTrioX);
        D(at::kKind2Z) = D(at::kObjTrioZ);
        SH_CALL(Field_ViewReset)();
        SetStep(0xE);
        break;
    case 0xE:
        if (Counter(0) != 0) break;
        Clear40();
        Set(0x20);
        SetRun(0);
        SetStep(0);
        break;
    case 0x1E:
        Clear40();
        DropIn(3);
        SetCounter(0, 0x20);
        SH_AT(VoidFn, at::kSetBit80)();
        SetRun(0);
        SetStep(0);
        break;
    case 0x23:
        Clear40();
        DropIn(4);
        SetCounter(0, 0x20);
        SetRun(0);
        SetStep(0);
        break;
    default: break;   // 0xF..0x1D, 0x1F..0x22
    }
}

// original 0x555E40: run 14.
SC9A_EXPORT void __cdecl Scena09_Run14(void) {
    switch (Step()) {
    case 0:
        SetStep(1);
        ChangeArea(0x35, 0x3D8000, 0xB0000, 0x85);
        break;
    case 1:
        if (Counter(0) != 0xF) break;
        Transition(0xD);
        PartyPass();
        LoadStream(0);
        SetStep(2);
        break;
    case 2:
        if (!WaitClear()) break;
        SetPass(0);
        if (!StreamDone()) break;
        SetStep(3);
        break;
    case 3:
        SetStep(4);
        ChangeArea(0x41, 0x290000, 0x320000, 4);
        break;
    case 4:
        Set(0x2E);
        ScriptXor(0xE);
        SetPass(0x1F);
        Clear40();
        ClearCounters();
        SetStep(0);
        SetRun(0);
        break;
    default: break;
    }
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

// original 0x555F40: run 15.
SC9A_EXPORT void __cdecl Scena09_Run15(void) {
    switch (Step()) {
    case 0:
        if (Request() == 2) break;
        SetCounter(0, 1);
        DropIn(1);
        SetStep(1);
        break;
    case 1:
        if (Counter(0) != 5) break;
        SetAndEnd(0x2D);
        break;
    case 0xA:
        SetStep(0xB);
        ChangeArea(0x25, 0x200000, 0x340000, 0x84);
        break;
    case 0xB:
        SetPass(0x1F);
        SetStep(0xC);
        break;
    case 0xC:
        if (Counter(0) != 0x19) break;
        CallB(9);
        SetAndEnd(0x2F);
        break;
    case 0xF: Say(0xD7); SetStep(0x10); break;
    case 0x1E: Say(0x5A); SetStep(0x10); break;
    case 0x34: Say(0xA5); SetStep(0x10); break;
    case 0x10:
        if (Request() == 2) break;
        EndRun();
        break;
    case 0x14:
        if (Request() == 2) break;
        ClrAt(at::kRow6, 0x3E);
        SetCounter(0, 1);
        DropIn(2);
        SetStep(0x15);
        break;
    case 0x15:
        if (Counter(0) != 4) break;
        SetCounter(0, 0);
        SetStep(0x16);
        ChangeArea(0x27, 0x150000, 0x280000, 0x88);
        break;
    case 0x16:
        if (Counter(0) != 0xF) break;
        SetAndEnd(0x30);
        break;
    case 0x28:
        if (Request() == 2) break;
        SetCounter(0, 1);
        DropIn(9);
        SetStep(0x29);
        break;
    case 0x29:
        if (Counter(0) != 7) break;
        SetAndEnd(0x31);
        break;
    case 0x32:
        DropIn(0xA);
        SetStep(0x33);
        break;
    case 0x33:
        if (Counter(0) != 5) break;
        SetAndEnd(0x33);
        break;
    case 0x3C:
        SetStep(0x3D);
        ChangeArea(0x29, 0x400000, 0x278000, 0x89);
        break;
    case 0x3D:
        SetPass(0x1F);
        SetStep(0x3E);
        break;
    case 0x3E:
        if (Counter(0) != 0x1E) break;
        SetAndEnd(0x36);
        break;
    case 0x46:
        if (B(at::kMember1Byte) == 3) break;
        Msg(0x87);
        SetRequest(2);
        SetStep(0x47);
        ClrAt(at::kStoryFlags, 0x45);
        SetAt(at::kRow6, 0x3E);
        break;
    case 0x47:
        if (Request() == 2) break;
        Transition(4);
        SetStep(0x48);
        break;
    case 0x48:
        if (!WaitClear()) break;
        SetPass(0);
        SetStep(0x49);
        break;
    case 0x49:
        Say(0x88);
        SetStep(0x4A);
        break;
    case 0x4A:
        if (Request() == 2) break;
        SetStep(0x4B);
        break;
    case 0x4B:
        SetStep(0x4C);
        Set(0x34);
        ChangeArea(0x27, 0xA0000, 0x2D8000, 0x8B);
        break;
    case 0x4C:
        Kind2(3);
        SetPass(0x1F);
        SetStep(0x4D);
        break;
    case 0x4D:
        if (Counter(0) != 4) break;
        LoadStream(2);
        SetStep(0x4E);
        break;
    case 0x4E:
        if (!StreamDone()) break;
        SH_CALL(Sound_ResumeAll)();
        SetStep(0x4F);
        break;
    case 0x4F: SweepAt(5, 0x20, 0x14, 0x280, 0x50); break;
    case 0x50: SweepAt(7, 0xFFF0, 0x28, 0, 0x51); break;
    case 0x51: SweepAt(0x1E, 0x80, 0xA, 0x500, 0x52); break;
    case 0x52: SweepAt(0x1F, 0xFF80, 0xA, 0, 0x53); break;
    case 0x53:
        if (Counter(0) != 0x1F) break;
        W(at::kCamDist) = 0;
        Redraw();
        SetStep(0x5A);
        break;
    case 0x5A:
        if (Counter(0) != 0x4E) break;
        Transition(4);
        SetStep(0x5B);
        break;
    case 0x5B:
        if (!WaitClear()) break;
        ScriptOrByte(0x88);
        SetCounter(0, 0);
        SetPass(0);
        SetStep(0x5C);
        Set(0x37);
        ChangeArea(0x29, 0x400000, 0x278000, 0x8A);
        B(at::kMusicCurrent) = 0x87;
        break;
    case 0x5C:
        if (Counter(0) != 1) break;
        Transition(1);
        SetPass(0x1F);
        SetStep(0x5D);
        break;
    case 0x5D:
        if (!WaitClear()) break;
        SetCounter(0, 2);
        Kind2(6);
        SetStep(0x5E);
        break;
    case 0x5E:
        if (Counter(0) != 5) break;
        SetAt(at::kRow6, 0x3E);
        SetCounter(2, 1);
        Set(0x38);
        Clear40();
        ScriptXor(8);
        SetStep(0);
        SetRun(0);
        break;
    case 0x64:
        Say(0x94);
        SetStep(0x65);
        break;
    case 0x65:
        if (Request() == 2) break;
        EndRun();
        break;
    case 0x6E:
        if (B(at::kMember1Byte) == 3) break;
        Say(0x9C);
        SetStep(0x6F);
        break;
    case 0x6F:
        if (Request() == 2) break;
        SetCounter(2, 0);
        SetStep(0x70);
        break;
    case 0x70:
        SetStep(0x71);
        ChangeArea(0x29, 0x40000, 0x28000, 0x8B);
        break;
    case 0x71:
        SetCounter(0, 1);
        SetPass(0x1F);
        Kind2(7);
        SetStep(0x72);
        break;
    case 0x72:
        if (Counter(0) != 8) break;
        SetCounter(2, 2);
        Clear40();
        ScriptXor(8);
        SetStep(0);
        SetRun(0);
        break;
    case 0x78:
        if (B(at::kMember1Byte) == 3) break;
        SetCounter(2, 0);
        SetStep(0x79);
        DropIn(2);
        break;
    case 0x79:
        SetCounter(0, 1);
        SetPass(0x1F);
        SetStep(0x7A);
        break;
    case 0x7A:
        if (Counter(0) != 0xF) break;
        SetCounter(0, 0);
        SetStep(0x7B);
        ChangeArea(0x64, 0x4C0000, 0x338000, 0x83);
        break;
    case 0x7B:
        Kind2(1);
        SetStep(0x7C);
        break;
    case 0x7C:
        if (Counter(0) != 0xA) break;
        SetCounter(0, 0);
        SetStep(0x7D);
        Set(0x39);
        ChangeArea(0x64, 0x4A0000, 0x1E8000, 0x84);
        break;
    case 0x7D:
        if (Counter(0) != 0x14) break;
        SetCounter(0, 0);
        SetStep(0x7E);
        ChangeArea(0x64, 0x480000, 0x340000, 0x85);
        break;
    case 0x7E:
        if (Counter(0) != 5) break;
        ScriptXor(0x80);
        SetCounter(0, 0);
        SetStep(0x7F);
        SetAt(at::kStoryFlags, 0x40);
        ChangeArea(0x3B, 0x480000, 0x2A0000, 0x82);
        break;
    case 0x7F:
        if (Counter(0) != 6) break;
        SetAndEnd(0x3A);
        break;
    case 0x82:
        SetStep(0x83);
        ChangeArea(0x3B, 0x108000, 0x140000, 0x83);
        break;
    case 0x83:
        if (Counter(0) != 7) break;
        SetAndEnd(0x22);
        break;
    case 0x8C:
        CallA(0xD);
        SetStep(0x8D);
        SetPass(0);
        ChangeArea(0x35, 0x120000, 0x180000, 0x86);
        break;
    case 0x8D:
        if (Counter(0) != 1) break;
        Transition(5);
        SetPass(0x1F);
        SetStep(0x8E);
        break;
    case 0x8E:
        if (!WaitClear()) break;
        SetCounter(0, 2);
        Kind2(3);
        SetStep(0x8F);
        break;
    case 0x8F:
        if (Counter(0) != 7) break;
        Transition(0xD);
        SetStep(0x90);
        break;
    case 0x90:
        if (!WaitClear()) break;
        SetPass(0);
        PartyPass();
        SetStep(0x91);
        FadeOutStop(0x1E);
        break;
    case 0x91:
        LoadStream(0);
        SetStep(0x92);
        break;
    case 0x92:
        if (!StreamDone()) break;
        SetStep(0x93);
        break;
    case 0x93:
        Say(0x37);
        SetStep(0x94);
        break;
    case 0x94:
        if (Request() == 2) break;
        SetRequest(6);
        SetStep(0x95);
        break;
    case 0x95:
        if (Request() != 0) break;
        SetStep(0x96);
        Set(0x23);
        ChangeArea(0x2D, 0x230000, 0x240000, 4);
        break;
    case 0x96:
        SetAt(at::kStoryFlags, 0x33);
        SetPass(0x1F);
        Clear40();
        ClearCounters();
        SetStep(0);
        SetRun(0);
        break;
    default: break;   // the steps between the cases above
    }
}

// original 0x556BC0: run 16.
SC9A_EXPORT void __cdecl Scena09_Run16(void) {
    const unsigned char step = Step();
    if (step == 0) {
        DropIn(2);
        SetStep(1);
    } else if (step == 1) {
        if (Counter(0) != 3) return;
        Set(0x24);
        EndRun();
    }
}

// ===========================================================================
// The object handlers (Scena09_Objects entries 1..4; the entry's (object,
// bits) are not read)

// original 0x5570F0: object 1 - run 5 from step 0, counter 0 cleared.
SC9A_EXPORT void __cdecl Scena09_Object01(void) {
    Set40();
    SetRun(5);
    SetCounter(0, 0);
    SetStep(0);
}

// original 0x557110: object 2 - run 5 from step 5, counter 0 cleared.
SC9A_EXPORT void __cdecl Scena09_Object02(void) {
    Set40();
    SetCounter(0, 0);
    SetStep(5);
    SetRun(5);
}

// original 0x557130: object 3 - run 4 from step 5.
SC9A_EXPORT void __cdecl Scena09_Object03(void) {
    Set40();
    SetStep(5);
    SetRun(4);
}

// original 0x557150: object 4 - run 10 from step 0.
SC9A_EXPORT void __cdecl Scena09_Object04(void) {
    Set40();
    SetRun(0xA);
    SetStep(0);
}

void ScenaSc9a_Inject() {
    if (bof3::WantsShadow("scena_sc9a")) scena_sc9a::SelfTest();
    BOF3_INJECT(Scena09_Frame);
    BOF3_INJECT(Scena09_EnterArea);
    BOF3_INJECT(Scena09_Run);
    BOF3_INJECT(Scena09_Run1);
    BOF3_INJECT(Scena09_Run2);
    BOF3_INJECT(Scena09_Run3);
    BOF3_INJECT(Scena09_Run4);
    BOF3_INJECT(Scena09_Run5);
    BOF3_INJECT(Scena09_Run6);
    BOF3_INJECT(Scena09_Run7);
    BOF3_INJECT(Scena09_Run8);
    BOF3_INJECT(Scena09_Run9);
    BOF3_INJECT(Scena09_Run10);
    BOF3_INJECT(Scena09_Run11);
    BOF3_INJECT(Scena09_Run14);
    BOF3_INJECT(Scena09_Run15);
    BOF3_INJECT(Scena09_Run16);
    BOF3_INJECT(Scena09_ObjectTrigger);
    BOF3_INJECT(Scena09_Object01);
    BOF3_INJECT(Scena09_Object02);
    BOF3_INJECT(Scena09_Object03);
    BOF3_INJECT(Scena09_Object04);
}

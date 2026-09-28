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
#include "game/scena_sc13_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = scena_sc13::at;
using scena_sc13::ArgFn;
using scena_sc13::ByteFn;
using scena_sc13::FindByteFn;
using scena_sc13::LevelFn;
using scena_sc13::ObjectEntry;
using scena_sc13::PlaceFn;
using scena_sc13::ScriptIdFn;
using scena_sc13::VoidFn;

unsigned char& B(std::uint32_t a) { return *reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a)); }
std::uint16_t& W(std::uint32_t a) { return *reinterpret_cast<std::uint16_t*>(static_cast<std::uintptr_t>(a)); }
std::uint32_t& D(std::uint32_t a) { return *reinterpret_cast<std::uint32_t*>(static_cast<std::uintptr_t>(a)); }
std::int32_t S16(std::uint32_t a) { return static_cast<std::int16_t>(W(a)); }

unsigned Area() { return W(at::kArea); }
unsigned char FD() { return B(at::kCondFD); }
unsigned char Counter(unsigned k) { return B(at::kCounters + k); }
void SetCounter(unsigned k, unsigned char v) { B(at::kCounters + k) = v; }
void BumpCounter0() { SetCounter(0, static_cast<unsigned char>(Counter(0) + 1)); }
unsigned char Step() { return B(at::kStep); }
void SetStep(unsigned char v) { B(at::kStep) = v; }
void SetRun(unsigned char v) { B(at::kRun) = v; }
void SetPass(unsigned char v) { B(at::kPassFlags) = v; }
unsigned char Request() { return B(at::kRequest); }
void SetRequest(unsigned char v) { B(at::kRequest) = v; }
bool WaitClear() { return W(at::kWait) == 0; }
void ScriptAnd(std::uint16_t v) { W(at::kScriptFlags) = static_cast<std::uint16_t>(W(at::kScriptFlags) & v); }
std::uint16_t Timer() { return W(at::kTimer); }
void SetTimer(std::uint16_t v) { W(at::kTimer) = v; }
// dec word [0x8034E6] / jne: true while the timer, one less, is not 0.
bool TimerRuns() {
    W(at::kTimer) = static_cast<std::uint16_t>(W(at::kTimer) - 1);
    return W(at::kTimer) != 0;
}
void AddElevOffset(std::uint16_t v) { W(at::kElevOffset) = static_cast<std::uint16_t>(W(at::kElevOffset) + v); }

// The flag bits are read from 0x929ED0 afresh for every call, as the
// originals load the dword before each push.
unsigned char* Bits() { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(D(at::kFlagBits))); }
unsigned char* Story() { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(at::kStoryFlags)); }
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
void PartyPass() { SH_AT(VoidFn, at::kPartyPass)(); }
void SetBit80() { SH_AT(VoidFn, at::kSetBit80)(); }

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

unsigned char* Effect(unsigned slot) { return &B(at::kEffects + (slot & 0xFFu) * at::kEffectStride); }
unsigned char* Sprite(unsigned slot) { return &B(at::kSprites + (slot & 0xFFu) * at::kSpriteStride); }
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
// A camera effect: +0 = 1, +5 = kind, the dwords +0x64 / +0x68 / +0x6C, the
// byte +9 = life.
void FxCam(unsigned slot, unsigned char kind, std::int32_t x, std::int32_t y, std::int32_t z, unsigned char life) {
    unsigned char* const e = Effect(slot);
    e[0] = 1;
    e[5] = kind;
    *reinterpret_cast<std::int32_t*>(e + 0x64) = x;
    *reinterpret_cast<std::int32_t*>(e + 0x68) = y;
    *reinterpret_cast<std::int32_t*>(e + 0x6C) = z;
    e[9] = life;
}
// As FxCam, and the dword +0xC = 0 (kind 0x31's).
void FxCam0(unsigned slot, unsigned char kind, std::int32_t x, std::int32_t y, std::int32_t z, unsigned char life) {
    FxCam(slot, kind, x, y, z, life);
    *reinterpret_cast<std::uint32_t*>(Effect(slot) + 0xC) = 0;
}
// test byte [effect(slot)], 1
bool EffectBusy(std::uint32_t cell) { return (Effect(B(cell))[0] & 1) != 0; }
std::int32_t AngleY() { return S16(at::kAngleY); }

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

// original 0x561DB0: slot 0, Field_ModeDispatch's call every field frame - a
// tail jump through Scena13_States on the s8 state 0x8034E2: 0
// Scena13_Start, 1 Scena13_EnterArea, 2 Scena13_Run.
SC13_EXPORT void __cdecl Scena13_Frame(void) {
    const int state = static_cast<signed char>(B(at::kState));
    reinterpret_cast<VoidFn>(static_cast<std::uintptr_t>(Entry(at::kStates13, state, at::kStateCount13, "Scena13_Frame")))();
}

// original 0x561DC0: state 0 - ScriptFlags_Set40, Draw_PassFlags 0x1F, the
// party pass 0x533E50, call-table A entry 0, story flag 0x43 set and 0x42
// cleared (the literal 0x904030), state 1.
SC13_EXPORT void __cdecl Scena13_Start(void) {
    Set40();
    SetPass(0x1F);
    PartyPass();
    CallA(0);
    SH_CALL(Flags_Set)(Story(), 0x43);
    SH_CALL(Flags_Clear)(Story(), 0x42);
    B(at::kState) = 1;
}

// original 0x561E00: state 1, by the area. Three failed tests jump straight
// to the end (area 0x56 at Cond_ByteFD 1 with the word 0x802290 not 0x70,
// area 0x58 with it not 0x91, area 0x70 at Cond_ByteFD not 4); every way out
// stores state 2.
static void EnterArea13Body() {
    if (Area() == 0x56) {
        if (FD() == 1) {
            if (W(at::kExtraWord) != 0x70) return;
            if (Flag(0xC) && !Flag(0xD)) {
                Set40();
                SetRun(4);
                SetStep(0xA);
            }
        }
        if (FD() == 0 && Flag(0xD) && !Flag(0xE)) {
            CallA(4);
            DropIn(0);
        }
    }
    if (Area() == 0x58) {
        if (W(at::kExtraWord) != 0x91) return;
        if (!Flag(0xB)) {
            W(at::kAngleX) = 0xFD2A;
            W(at::kAngleFB) = 0x2F2;
            W(at::kCamDistance) = 0x5DC;
        }
    }
    if (Area() == 0x70) {
        if (FD() != 4) return;
        if (!Flag(0xC)) {
            DropIn(2);
            Set(0xC);
        }
    }
    if (Area() == 0x7E && !Flag(0)) {
        DropIn(0);
        W(at::kAngleX) = 0xFC7A;
        W(at::kAngleFB) = 0x150;
        SetRun(1);
        SetStep(0);
    }
    if (Area() == 0x8F) {
        if (FD() == 0) {
            if (!Flag(0x11)) {
                W(at::kAngleX) = 0xFD40;
                W(at::kAngleFB) = 0x10E;
                const unsigned char slot = Take(at::kSlot13);
                if (slot != 0xFF) FxCam(slot, 0x13, -0x2AA, AngleY(), 0x2C6, 0xFF);
            } else if (!Flag(0x12)) {
                SH_AT(ArgFn, at::kArea143)(1);
                SetPass(0);
                Sound(0x203);
            } else if (!Flag(0x13)) {
                CallA(6);
                DropIn(6);
                Set(0x13);
            }
        }
        if (FD() == 2) {
            if (!Flag(0x11)) {
                CallA(5);
                DropIn(3);
            } else if (!Flag(0x12)) {
                SH_AT(ArgFn, at::kArea143)(0);
            }
        }
    }
    if (Area() == 0x90) {
        Set(0x1F);
        if (FD() == 2 && !Flag(0x17)) {
            SH_CALL(Music_FadeOut)(0x10);
            DropIn(0);
            SetRun(7);
            SetStep(0);
        }
    }
    if (Area() == 0x91) {
        if (FD() == 0 && !Flag(7)) {
            DropIn(5);
            SetRun(3);
            SetStep(0);
        }
        if (FD() == 4 && !Flag(8)) {
            DropIn(6);
            Set(8);
        }
    }
    if (Area() != 0xC2) return;
    if (!Flag(2)) {
        CallA(1);
        DropIn(0);
        SH_CALL(Scena13_SpawnPairA)();
        W(at::kCamDistance) = 0xF200;
        const unsigned char slot = Take(at::kSlot13);
        if (slot == 0xFF) return;
        FxCam0(slot, 0x31, -0x2AA, AngleY(), 0x200, 0x1E);
        return;
    }
    if (Flag(3) && !Flag(4)) {
        CallA(2);
        DropIn(2);
        SH_CALL(Scena13_SpawnPairB)();
    }
}
SC13_EXPORT void __cdecl Scena13_EnterArea(void) {
    EnterArea13Body();
    B(at::kState) = 2;
}

// original 0x562280: state 2, a tail jump through Scena13_Runs on the s8 run
// MoveScript_Var7: 0 a bare ret (0x437CC0), 1..8 Run1..Run8.
SC13_EXPORT void __cdecl Scena13_Run(void) {
    const int run = static_cast<signed char>(B(at::kRun));
    reinterpret_cast<VoidFn>(static_cast<std::uintptr_t>(Entry(at::kRuns13, run, at::kRunCount13, "Scena13_Run")))();
}

// original 0x562290: run 1, steps 0..5.
SC13_EXPORT void __cdecl Scena13_Run1(void) {
    switch (Step()) {
    case 0: {
        const unsigned char slot = Take(at::kSlot13);
        if (slot == 0xFF) return;
        SetStep(1);
        FxCam(slot, 0x13, -0x2AA, AngleY(), 0x200, 0x80);
        return;
    }
    case 1:
        if (Counter(0) != 8) return;
        Transition(0);
        SetStep(2);
        return;
    case 2:
        if (!WaitClear()) return;
        SetPass(0);
        SH_CALL(Task_Sleep)(1);
        SetRequest(6);
        SetStep(3);
        return;
    case 3:
        if (Request() != 0) return;
        SetCounter(0, 0);
        Set(0);
        ChangeArea(0x7E, 0x4E0000, 0x2C8000, 1);
        B(at::kSpriteMode) = 0xFF;
        B(at::kEntryByte) = 0xFF;
        SetStep(4);
        return;
    case 4:
        if (Request() != 0) return;
        SetPass(0x1F);
        Transition(1);
        SetStep(5);
        return;
    case 5:
        if (!WaitClear()) return;
        ScriptAnd(0xFFF7);
        Clear40();
        StopRun();
        return;
    default:
        return;
    }
}

// Run 2's effect of kind 0x75 (a slot taken, kind stored; none is no matter).
static void Spawn75() {
    const unsigned char slot = Take(at::kSlot13);
    if (slot != 0xFF) Fx(slot, 0x75);
}

// original 0x562400: run 2, steps 0..0xB.
SC13_EXPORT void __cdecl Scena13_Run2(void) {
    switch (Step()) {
    case 0:
        ChangeArea(0xC2, 0x130000, 0x190000, 1);
        SetStep(1);
        return;
    case 1:
        if (Counter(0) != 3) return;
        Spawn75();
        FadeOutStop(8);
        SetStep(2);
        return;
    case 2:
        if (Counter(0) != 0) return;
        Set(2);
        ChangeArea(0xC2, 0x3C8000, 0xB0000, 0x81);
        B(at::kEntryByte) = 0xFF;
        B(at::kMusicCurrent) = 0xFF;
        SetStep(3);
        return;
    case 3:
        if (Counter(0) != 2) return;
        Spawn75();
        SetStep(4);
        return;
    case 4:
        if (Counter(0) != 0) return;
        Set(3);
        ChangeArea(0xC2, 0x130000, 0x130000, 3);
        SetStep(5);
        return;
    case 5:
        if (Counter(0) != 5) return;
        Spawn75();
        FadeOutStop(8);
        SetStep(6);
        return;
    case 6:
        if (Counter(0) != 0) return;
        Set(4);
        ChangeArea(0xC2, 0x3C8000, 0xB0000, 0x83);
        B(at::kMusicCurrent) = 0xFF;
        SetStep(7);
        return;
    case 7:
        if (Counter(0) != 1) return;
        Transition(0);
        SetCounter(0, 0);
        SetStep(8);
        return;
    case 8:
        if (!WaitClear()) return;
        SetPass(0);
        SetTimer(0x78);
        SetStep(9);
        return;
    case 9:
        if (TimerRuns()) return;
        CallA(3);
        SH_CALL(Sound_LoadStream)(0);
        Set(5);
        SetStep(0xA);
        return;
    case 0xA:
        if (SH_CALL(Sound_StreamDone)() == 0) return;
        PartyPass();
        ChangeArea(0x97, 0x330000, 0x370000, 1);
        B(at::kSpriteMode) = 0xFF;
        B(at::kEntryByte) = 0xFF;
        SetStep(0xB);
        return;
    case 0xB:
        if (!WaitClear()) return;
        SetPass(0x1F);
        Transition(0xB);
        ScriptAnd(0xFFF7);
        Clear40();
        StopRun();
        return;
    default:
        return;
    }
}

// Two free sprite records taken with 0x57CD90 (the first's +0 set to 1 before
// the second is looked for, and back to 0 when there is none), then each
// record's +0 set and its slot to the word 0x903850 before EventOp_6x runs
// one of two 16-byte event-op records of Scena13_EventOps on it.
static void SpawnPair(std::uint32_t first, std::uint32_t second) {
    const unsigned char a = SH_AT(ByteFn, at::kFreeSprite)();
    if (a == 0xFF) return;
    Sprite(a)[0] = 1;
    const unsigned char b = SH_AT(ByteFn, at::kFreeSprite)();
    if (b == 0xFF) {
        Sprite(a)[0] = 0;
        return;
    }
    W(at::kSlotWord) = a;
    Sprite(b)[0] = 1;
    SH_CALL(EventOp_6x)(reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(first)));
    W(at::kSlotWord) = b;
    SH_CALL(EventOp_6x)(reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(second)));
}

// original 0x562700: the pair of event objects Scena13_EventOps records 0 and
// 1 set up (Scena13_EnterArea, area 0xC2, flag 2 clear).
SC13_EXPORT void __cdecl Scena13_SpawnPairA(void) { SpawnPair(at::kEventOps13, at::kEventOps13 + 0x10); }

// original 0x5627A0: the same with records 2 and 3 (area 0xC2, flags 3 set
// and 4 clear).
SC13_EXPORT void __cdecl Scena13_SpawnPairB(void) { SpawnPair(at::kEventOps13 + 0x20, at::kEventOps13 + 0x30); }

// original 0x562840: run 3, steps 0..5, 0xA..0xD, 0x14..0x18 (MSVC's
// two-level switch: the byte table 0x562CD0 of 0x19, then 16 cases).
SC13_EXPORT void __cdecl Scena13_Run3(void) {
    switch (Step()) {
    case 0: {
        if (Counter(0) != 2) return;
        const unsigned char slot = Take(at::kSlot13);
        if (slot == 0xFF) return;
        SetStep(1);
        AddElevOffset(0xF800);
        SetTimer(0x80);
        FxCam(slot, 0x13, -0x370, AngleY(), 0x299, 0xFF);
        return;
    }
    case 1:
        if (TimerRuns()) return;
        AddElevOffset(0xF800);
        B(at::kCondFE) = 1;
        SetStep(2);
        return;
    case 2: {
        if (Counter(0) != 3) return;
        const unsigned char slot = Take(at::kSlot13);
        if (slot == 0xFF) return;
        SetStep(3);
        SetTimer(0x20);
        FxCam(slot, 0x13, -0x2AA, AngleY(), 0x200, 0x40);
        return;
    }
    case 3:
        if (TimerRuns()) return;
        AddElevOffset(0x800);
        B(at::kCondFE) = 0;
        SetStep(4);
        return;
    case 4:
        if (Counter(0) != 0) return;
        AddElevOffset(0x800);
        Set(7);
        StopRun();
        return;
    case 5:
        Clear40();
        DropIn(7);
        StopRun();
        return;
    case 0xA:
        SH_AT(FindByteFn, at::kFindByte)(0xB);
        SH_CALL(Kind2_Place)(2);
        SetStep(0xB);
        return;
    case 0xB:
        if (Counter(0) != 0x12) return;
        SetPass(0);
        Say(0x18);
        SetStep(0xC);
        return;
    case 0xC: {
        if (Request() == 2) return;
        D(at::kKind2X) = 0xA0000;
        D(at::kKind2 + 0x34) = 0xA0000;
        D(at::kKind2Z) = 0x620000;
        D(at::kKind2 + 0x38) = 0x620000;
        W(at::kKind2 + 0x3E) = static_cast<std::uint16_t>(SH_CALL(AreaMap_Elevation)(0xA0000, 0x620000));
        SH_CALL(Field_ViewReset)();
        Msg(0x19);
        SetRequest(2);
        Transition(1);
        SetPass(0x1F);
        W(at::kAngleX) = 0xFCBC;
        W(at::kAngleFB) = 0x3A2;
        const unsigned char slot = Take(at::kSlot13);
        if (slot == 0xFF) return;
        SetStep(0xD);
        FxCam(slot, 0x13, -0x2AA, AngleY(), 0x200, 0x64);
        return;
    }
    case 0xD:
        if (EffectBusy(at::kSlot13)) return;
        if (Request() == 2) return;
        Set(9);
        ChangeArea(0x91, 0x3F8000, 0x5E0000, 0x88);
        StopRun();
        B(at::kMusicTrack) = 0x5C;
        return;
    case 0x14:
        if (Counter(0) != 1) return;
        Set40();
        ChangeArea(0x58, 0x130000, 0x250000, 0x80);
        B(at::kSpriteMode) = 1;
        SetStep(0x15);
        return;
    case 0x15:
        if (!WaitClear()) return;
        SetTimer(0x3C);
        SetStep(0x16);
        return;
    case 0x16: {
        if (Timer() != 0) {
            SetTimer(static_cast<std::uint16_t>(Timer() - 1));
            return;
        }
        const unsigned char slot = SH_CALL(Effect_FindFree)();   // a slot of its own, not Scena13_Slot
        if (slot != 0xFF) Fx(slot, 0x80);
        SetTimer(0x46);
        SetStep(0x17);
        return;
    }
    case 0x17:
        if (Timer() != 0) {
            SetTimer(static_cast<std::uint16_t>(Timer() - 1));
            return;
        }
        ChangeArea(0x70, 0xC8000, 0x580000, 0x81);
        B(at::kEntryByte) = 0;
        SetStep(0x18);
        return;
    case 0x18:
        if (!WaitClear()) return;
        Clear40();
        Set(0xB);
        StopRun();
        return;
    default:
        return;
    }
}

// original 0x562CF0: run 4, steps 0, 1, 0xA, 0xB, 0x14..0x1F (the byte table
// 0x563070 of 0x20, then 17 cases). Its ebx holds 0x1F from the start: step
// 0x1E stores it to Draw_PassFlags and the step (below).
SC13_EXPORT void __cdecl Scena13_Run4(void) {
    switch (Step()) {
    case 0:
        Say(0x12);
        SetStep(1);
        return;
    case 1:
        if (Request() == 2) return;
        EndRun();
        return;
    case 0xA: {
        if (Counter(0) != 0x10) return;
        const unsigned char arg = B(at::kEffectArg + B(at::kLeaderName));
        B(at::kSlot13) = SH_CALL(Effect_SpawnAt)(4, 5, static_cast<signed char>(arg), static_cast<long>(D(at::kTrioX)),
                                                  static_cast<long>(D(at::kTrioY)), static_cast<long>(D(at::kTrioZ)));
        SetStep(0xB);
        return;
    }
    case 0xB:
        if (EffectBusy(at::kSlot13)) return;
        Clear40();
        Set(0xD);
        StopRun();
        return;
    case 0x14:
        SetCounter(0, 0);
        ChangeArea(0x56, 0xC0000, 0x98000, 3);
        B(at::kMusicCurrent) = 0xFF;
        SetStep(0x15);
        return;
    case 0x15:
        if (!WaitClear()) return;
        SH_CALL(MoveCmd_TestFB)(0xC, 9);
        Say(2);
        SetStep(0x16);
        return;
    case 0x16:
        if (Counter(0) != 7) return;
        AddElevOffset(0xFC00);
        SetStep(0x17);
        return;
    case 0x17:
        if (Counter(0) != 8) return;
        Set(0xE);
        ChangeArea(0x58, 0x240000, 0x200000, 0x81);
        B(at::kMusicCurrent) = 0xFF;
        B(at::kSpriteMode) = 1;
        SetStep(0x18);
        return;
    case 0x18:
        if (!WaitClear()) return;
        Say(0xA);
        SetStep(0x19);
        return;
    case 0x19:
        if (Counter(0) != 1) return;
        SetCounter(0, 0);
        ChangeArea(0x56, 0x100000, 0x98000, 0x81);
        B(at::kEntryByte) = 0;
        B(at::kMusicCurrent) = 0x5C;
        SetStep(0x1A);
        return;
    case 0x1A:
        if (Counter(0) != 0x18) return;
        Transition(0);
        SH_CALL(Music_FadeOut)(0x20);
        SetStep(0x1B);
        return;
    case 0x1B:
        if (!WaitClear()) return;
        SetPass(0);
        Msg(0x14);
        SetRequest(2);
        FadeOutStop(0xA);
        SetStep(0x1C);
        return;
    case 0x1C:
        if (Request() == 2) return;
        SH_CALL(Task_Sleep)(1);
        SetRequest(6);
        SetStep(0x1D);
        return;
    case 0x1D:
        if (Request() != 0) return;
        SetCounter(0, 0);
        Set(0xF);
        ChangeArea(0x56, 0x100000, 0x98000, 3);
        B(at::kSpriteMode) = 0xFF;
        B(at::kEntryByte) = 0xFF;
        SetStep(0x1E);
        return;
    case 0x1E:
        // As the original has it: ebx is still the 0x1F it loaded for the
        // switch bound, so the pass flags and the step both become 0x1F.
        if (Request() != 0) return;
        SetPass(0x1F);
        Transition(1);
        SetStep(0x1F);
        return;
    case 0x1F:
        if (!WaitClear()) return;
        Clear40();
        ScriptAnd(0xFFF7);
        StopRun();
        return;
    default:
        return;
    }
}

// original 0x563390: two sound voices' levels from the dial angle a (16-bit):
// 0x587890(0x208, |Math_Sin((a - 0x4C0) >> 1)| >> 5) and (0x207, the same of
// (a - 0xCC0) >> 1), each level masked to 16 bits.
static std::uint32_t Level(std::int32_t angle) {
    const std::uint32_t s = static_cast<std::uint32_t>(SH_CALL(Math_Sin)(angle));
    const std::uint32_t sign = static_cast<std::uint32_t>(static_cast<std::int32_t>(s) >> 31);
    const std::uint32_t mag = (s ^ sign) - sign;   // cdq / xor / sub
    return static_cast<std::uint32_t>(static_cast<std::int32_t>(mag) >> 5) & 0xFFFF;
}
SC13_EXPORT void __cdecl Scena13_ToneLevels(unsigned angle) {
    const std::int32_t a = static_cast<std::int32_t>(angle & 0xFFFF);
    SH_AT(LevelFn, at::kVoiceLevel)(0x208, Level((a - 0x4C0) >> 1));
    SH_AT(LevelFn, at::kVoiceLevel)(0x207, Level((a - 0xCC0) >> 1));
}

// original 0x563090: run 5, steps 0..3, 5, 6, 0xA..0x10 - a dial: the dword
// Sprite_ObjectsExtra record 1 + 0x6C turned by 0x10 while Input_Held bit
// 0x2000 (up) or 0x8000 (down) is held, the two voices following it, until
// Input_Pressed bit 0x20; 0x4A0..0x4E0 goes on to step 0xA, anything else to 5.
SC13_EXPORT void __cdecl Scena13_Run5(void) {
    switch (Step()) {
    case 0:
        FadeOutStop(8);
        Sound(0x208);
        Sound(0x207);
        SetStep(1);
        return;
    case 1:
        SH_CALL(Scena13_ToneLevels)(D(at::kExtra1Angle) & 0xFFF);
        SetStep(2);
        return;
    case 2: {
        std::uint32_t a = D(at::kExtra1Angle);
        W(at::kSlotWord) = static_cast<std::uint16_t>(a & 0xFFF);
        const std::uint32_t held = D(at::kInputHeld);
        bool turned = false;
        if (held & 0x2000) {
            a += 0x10;
            turned = true;
        } else if (held & 0x8000) {
            a -= 0x10;
            turned = true;
        }
        if (turned) {
            D(at::kExtra1Angle) = a;
            SH_CALL(Scena13_ToneLevels)(a & 0xFFF);
        }
        if (B(at::kInputPressed) & 0x20) SetStep(3);
        return;
    }
    case 3: {
        const std::uint16_t a = static_cast<std::uint16_t>(D(at::kExtra1Angle) & 0xFFF);
        W(at::kSlotWord) = a;
        SetStep(a <= 0x4E0 && a >= 0x4A0 ? 0xA : 5);
        return;
    }
    case 5:
        Sound(0x20A);
        Sound(0x209);
        SH_CALL(Music_Play)(B(at::kMusicTrack), 0x20);
        SetTimer(0x30);
        SetStep(6);
        return;
    case 6:
        if (TimerRuns()) return;
        Clear40();
        DropIn(1);
        StopRun();
        return;
    case 0xA:
        Sound(0x20A);
        Sound(0x209);
        SetTimer(0x1E);
        SetStep(0xB);
        return;
    case 0xB:
        if (TimerRuns()) return;
        DropIn(2);
        SH_CALL(MoveCmd_TestFB)(0xA, 8);
        SetStep(0xC);
        return;
    case 0xC: {
        if (Counter(0) != 1) return;
        const unsigned char slot = SH_CALL(Effect_FindFree)();   // a slot of its own
        if (slot != 0xFF) {
            unsigned char* const e = Effect(slot);
            e[0] = 1;
            e[5] = 0x7F;
            *reinterpret_cast<std::uint32_t*>(e + 0x34) = 0x98000;
            *reinterpret_cast<std::uint32_t*>(e + 0x38) = 0x68000;
            *reinterpret_cast<std::uint32_t*>(e + 0x3C) = 0xF000000;
        }
        SetTimer(0x28);
        SetStep(0xD);
        return;
    }
    case 0xD:
        if (TimerRuns()) return;
        SetCounter(0, 3);
        SetStep(0xE);
        return;
    case 0xE:
        if (Counter(0) != 4) return;
        SH_CALL(Sound_LoadStream)(5);
        Say(5);
        SetStep(0xF);
        return;
    case 0xF:
        if (SH_CALL(Sound_StreamDone)() == 0) return;
        if (Request() == 2) return;
        SH_CALL(Music_Play)(B(at::kMusicTrack), 0x20);
        SetStep(0x10);
        BumpCounter0();
        return;
    case 0x10:
        if (Counter(0) != 0) return;
        Clear40();
        Set(0x10);
        StopRun();
        return;
    default:
        return;
    }
}

// original 0x5633F0: run 6, steps 0..0xF, 0x14, 0x15, 0x19..0x1E.
SC13_EXPORT void __cdecl Scena13_Run6(void) {
    switch (Step()) {
    case 0:
        ChangeArea(0x8F, 0x3A8000, 0x1F8000, 0x82);
        SetStep(1);
        return;
    case 1: {
        if (Counter(0) != 7) return;
        const unsigned char slot = Take(at::kSlot13);
        if (slot == 0xFF) return;
        SetStep(2);
        FxCam(slot, 0x13, S16(at::kAngleX), AngleY(), 0x200, 0x20);
        return;
    }
    case 2:
        if (EffectBusy(at::kSlot13)) return;
        SetStep(3);
        BumpCounter0();
        return;
    case 3:
        if (Counter(0) != 0xE) return;
        Set(0x20);
        ChangeArea(0x8F, 0x98000, 0x518000, 0x28);
        SetStep(4);
        return;
    case 4:
        if (Counter(0) != 0xD) return;
        Set(0x11);
        Set(0x21);
        ChangeArea(0x8F, 0x350000, 0x330000, 0x84);
        B(at::kSpriteMode) = 0xFF;
        B(at::kMusicCurrent) = 0xFF;
        SetStep(5);
        return;
    case 5:
        if (!WaitClear()) return;
        Clr(0x20);
        SetTimer(0x3C);
        SetStep(6);
        return;
    case 6: {
        if (TimerRuns()) return;
        Transition(1);
        SetPass(0x1F);
        W(at::kAngleX) = 0xFE32;
        W(at::kCamDistance) = 0xF880;
        const unsigned char slot = Take(at::kSlot13);
        if (slot != 0xFF) FxCam0(slot, 0x31, -0x2AA, AngleY(), S16(at::kAngleFB), 0x3C);
        SetStep(7);
        return;
    }
    case 7:
        if (EffectBusy(at::kSlot13)) return;
        SetStep(8);
        BumpCounter0();
        return;
    case 8: {
        if (Counter(0) != 0xB) return;
        const unsigned char slot = Take(at::kSlot13);
        if (slot != 0xFF) Fx(slot, 0x81);
        SetStep(9);
        return;
    }
    case 9:
        if (Counter(0) != 0xC) return;
        Transition(0);
        SetStep(0xA);
        return;
    case 0xA:
        if (!WaitClear()) return;
        SetPass(0);
        SetTimer(0x3C);
        SetStep(0xB);
        return;
    case 0xB:
        if (TimerRuns()) return;
        Sound(0x205);
        Say(0x14);
        SetStep(0xC);
        return;
    case 0xC:
        if (Request() == 2) return;
        Set(0x22);
        Clr(0x21);
        ChangeArea(0x8F, 0x90000, 0x4C0000, 0x85);
        B(at::kEntryByte) = 0xFF;
        B(at::kSpriteMode) = 0xFF;
        SetStep(0xD);
        return;
    case 0xD:
        if (!WaitClear()) return;
        Transition(1);
        SetPass(0x1F);
        SetStep(0xE);
        return;
    case 0xE:
        if (!WaitClear()) return;
        SetStep(0xF);
        BumpCounter0();
        return;
    case 0xF:
        if (Counter(0) != 0) return;
        Clear40();
        Set(0x12);
        B(at::kScriptFlags) = static_cast<unsigned char>(B(at::kScriptFlags) | 0x10);
        StopRun();
        return;
    case 0x14:
        Say(0x26);
        SetStep(0x15);
        return;
    case 0x15:
        if (Request() == 2) return;
        EndRun();
        return;
    case 0x19: {
        if (Request() == 2) return;
        CallA(7);
        DropIn(7);
        const unsigned char* const o = reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(D(at::kCamObject)));
        const std::uint32_t x = *reinterpret_cast<const std::uint32_t*>(o + 0x34);
        D(at::kTrio + 2 * at::kTrioStride + 0x34) = x;
        const std::uint32_t z = *reinterpret_cast<const std::uint32_t*>(o + 0x38);
        D(at::kTrio + 2 * at::kTrioStride + 0x38) = z;
        W(at::kTrio + 2 * at::kTrioStride + 0x3E) =
            static_cast<std::uint16_t>(SH_CALL(AreaMap_Elevation)(static_cast<int>(x), static_cast<int>(z)));
        *reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(D(at::kCamObject))) = 0;
        SetStep(0x1A);
        return;
    }
    case 0x1A:
        if (Counter(0) != 2) return;
        Set(0x14);
        Clr(0x22);
        ChangeArea(0x8F, 0x78000, 0x508000, 0x88);
        B(at::kEntryByte) = 0xFF;
        B(at::kSpriteMode) = 0xFF;
        SetPass(0);
        SetStep(0x1B);
        return;
    case 0x1B:
        if (!WaitClear()) return;
        SetPass(0x1F);
        Say(0x2A);
        SetStep(0x1C);
        return;
    case 0x1C:
        if (Request() == 2) return;
        Set(0x15);
        ChangeArea(0x8F, 0xC8000, 0x218000, 0x89);
        SetStep(0x1D);
        return;
    case 0x1D:
        if (Counter(0) != 0xA) return;
        SetStep(0x1E);
        return;
    case 0x1E:
        if (Counter(0) != 0) return;
        Clear40();
        ScriptAnd(0xFFEF);
        Set(0x16);
        StopRun();
        return;
    default:
        return;
    }
}

// original 0x563F20: a caption for run 7 - the text `index` of the offsets at
// 0x803580 (text from 0x803582) drawn at (0x50, 0x3C) while the clock
// total - timer (total = 30 * seconds - 1, both 16-bit, to 0x903852 /
// 0x903850) is below the total; the CLUT strip faded in over its first 0x20
// frames (ClutStrip_FadeTo(clock)), restored at 0x20, and faded out over its
// last 0x20 (FadeTo(total - clock)); restored when the clock equals the
// total, nothing past it. The index also to the word 0x7DEE48.
SC13_EXPORT void __cdecl Scena13_Caption(unsigned index, unsigned seconds) {
    const std::uint16_t total = static_cast<std::uint16_t>((seconds & 0xFF) * 30 - 1);
    W(at::kCaptionIndex) = static_cast<std::uint16_t>(index);
    W(at::kCaptionTotal) = total;
    const std::uint16_t clock = static_cast<std::uint16_t>(total - Timer());
    W(at::kSlotWord) = clock;
    if (clock >= total) {
        if (clock == total) SH_CALL(ClutStrip_Restore)();
        return;
    }
    const std::uint32_t text = at::kCaptionText + 2 + W(at::kCaptionText + 2 * (index & 0xFFFF));
    SH_CALL(Text_DrawAt)(0x50, 0x3C, 0, 0xFF, reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(text)));
    const std::uint16_t now = W(at::kSlotWord);
    if (now < 0x20) {
        SH_CALL(ClutStrip_FadeTo)(now & 0xFF);
        return;
    }
    if (now == 0x20) {
        SH_CALL(ClutStrip_Restore)();
        return;
    }
    if (static_cast<std::int32_t>(W(at::kSlotWord)) <= static_cast<std::int32_t>(W(at::kCaptionTotal)) - 0x20) return;
    SH_CALL(ClutStrip_FadeTo)(static_cast<unsigned char>(B(at::kCaptionTotal) - B(at::kSlotWord)));
}

// Run 7's countdown after a caption: while the timer is not 0, one less and
// true.
static bool CountDown() {
    if (Timer() == 0) return false;
    SetTimer(static_cast<std::uint16_t>(Timer() - 1));
    return true;
}
static void Caption(unsigned index, unsigned seconds) { SH_CALL(Scena13_Caption)(index, seconds); }

// original 0x563A20: run 7, steps 0..0xA, 0x14..0x1A, 0x1C, 0x1D - four
// captions, then a battle (0x532ED0 at (0x288000, 0x1D8000), kind 0x25;
// Field_StartEventBattle(0x25)).
SC13_EXPORT void __cdecl Scena13_Run7(void) {
    switch (Step()) {
    case 0:
        if (Counter(0) != 0xB) return;
        Transition(0);
        FadeOutStop(0xA);
        SetStep(1);
        return;
    case 1:
        if (!WaitClear()) return;
        SetPass(0);
        SH_CALL(Music_Play)(0x6F, 0x10);
        SetTimer(0x96);
        SetStep(2);
        return;
    case 2:
        Caption(6, 5);
        if (CountDown()) return;
        SetStep(3);
        BumpCounter0();
        return;
    case 3: {
        if (!WaitClear()) return;
        Transition(7);
        SetPass(0x1F);
        W(at::kAngleX) = 0xFC7A;
        W(at::kAngleFB) = 0x3B8;
        W(at::kCamDistance) = 0xF204;
        const unsigned char slot = Take(at::kSlot13);
        if (slot != 0xFF) FxCam0(slot, 0x31, -0x2EC, AngleY(), S16(at::kAngleFB), 0x3C);
        SetTimer(0x96);
        SetStep(4);
        return;
    }
    case 4:
        Caption(7, 5);
        if (CountDown()) return;
        SetTimer(0x3C);
        SetStep(5);
        BumpCounter0();
        return;
    case 5:
        if (CountDown()) return;
        SetTimer(0x12C);
        SetStep(6);
        return;
    case 6:
        Caption(8, 10);
        if (CountDown()) return;
        SetStep(7);
        return;
    case 7:
        if (Counter(0) != 0xE) return;
        SetTimer(0x1E);
        SetStep(8);
        return;
    case 8:
        if (CountDown()) return;
        SetTimer(0x96);
        SetStep(9);
        Transition(6);
        return;
    case 9:
        Caption(9, 5);
        if (!CountDown()) {
            BumpCounter0();
            W(at::kAngleX) = 0xFD56;
            W(at::kAngleFB) = 0x200;
            Transition(1);
            SetPass(0x1F);
            SetStep(0xA);
        }
        if (WaitClear()) SetPass(0);
        return;
    case 0xA:
        if (Counter(0) != 0) return;
        Set(0x17);
        StopRun();
        return;
    case 0x14:
        if (Counter(0) != 9) return;
        SH_CALL(Music_FadeOut)(0x10);
        SetTimer(0x1E);
        SetStep(0x15);
        return;
    case 0x15: {
        W(at::kCamDistance) = static_cast<std::uint16_t>(W(at::kCamDistance) + 0x50);
        if (TimerRuns()) return;
        FadeOutStop(0xA);
        const unsigned char slot = SH_CALL(Effect_FindFree)();   // a slot of its own
        if (slot != 0xFF) Fx(slot, 0x87);
        SetTimer(0x1E);
        SetStep(0x16);
        return;
    }
    case 0x16:
        // As the original has it: waits for the timer at 0 without counting it
        // down (step 0x15 left it 0x1E).
        if (Timer() != 0) return;
        SetStep(0x17);
        BumpCounter0();
        return;
    case 0x17:
        if (Counter(0) != 0xB) return;
        SetTimer(0x1E);
        SetStep(0x18);
        return;
    case 0x18:
        W(at::kCamDistance) = static_cast<std::uint16_t>(W(at::kCamDistance) - 0x50);
        if (TimerRuns()) return;
        SetCounter(0, 0x32);
        SetStep(0x19);
        return;
    case 0x19:
        if (Counter(0) != 0x34) return;
        D(at::kKind2X) = 0x240000;
        D(at::kKind2Z) = 0x1C8000;
        SH_CALL(Field_ViewReset)();
        SH_AT(PlaceFn, at::kPartyPlace)(0x288000, 0x1D8000, 0x25);
        SetStep(0x1A);
        BumpCounter0();
        return;
    case 0x1A:
        if (Counter(0) != 0x37) return;
        SH_CALL(Field_StartEventBattle)(0x25);
        SetStep(0x1B);
        return;
    case 0x1C: {
        BumpCounter0();
        DropIn(7);
        const unsigned char slot = Take(at::kSlot13);
        if (slot != 0xFF) Fx(slot, 0x86);
        SetStep(0x1D);
        return;
    }
    case 0x1D:
        if (Counter(0) != 0) return;
        Clear40();
        Set(0x1A);
        StopRun();
        return;
    default:
        return;
    }
}

// original 0x563FE0: run 8, steps 0, 4..6, 8, 0xA, 0xB.
SC13_EXPORT void __cdecl Scena13_Run8(void) {
    switch (Step()) {
    case 0:
        // ObjTrio records 1 and 2 + 0x137 (0x802FC3, 0x80310F) both 0
        if (B(at::kTrio + at::kTrioStride + 0x137) != 0) return;
        if (B(at::kTrio + 2 * at::kTrioStride + 0x137) != 0) return;
        DropIn(0xA);
        Set(0x1D);
        StopRun();
        return;
    case 4:
        Say(1);
        SetStep(6);
        return;
    case 5:
        Say(0x4E);
        SetStep(6);
        return;
    case 6:
        if (Request() == 2) return;
        EndRun();
        return;
    case 8:
        if (Counter(0) != 0x28) return;
        SetCounter(0, 0);
        SetStep(0xA);
        return;
    case 0xA:
        if (Request() == 2) return;
        ChangeArea(0x97, 0x360000, 0x1D0000, 3);
        SetStep(0xB);
        return;
    case 0xB:
        if (Request() != 0) return;
        Set(0x1E);
        Clear40();
        StopRun();
        SetBit80();
        return;
    default:
        return;
    }
}

// original 0x564120: slot 1, called by 0x56D6D0 with the object that
// triggered: Scena13_Objects[object +0x86] (object, the flag bits' pointer).
SC13_EXPORT void __cdecl Scena13_ObjectTrigger(unsigned char* object) {
    const std::uint32_t bits = D(at::kFlagBits);
    const unsigned index = object[0x86];
    reinterpret_cast<ObjectEntry>(static_cast<std::uintptr_t>(
        Entry(at::kObjects13, static_cast<int>(index), at::kObjectCount13, "Scena13_ObjectTrigger")))(object, bits);
}

// original 0x564140: Scena13_Objects entry 0 - the object's word +0x8A + 1.
SC13_EXPORT void __cdecl Scena13_Object00(unsigned char* object, std::uint32_t /*bits*/) {
    std::uint16_t* const w = reinterpret_cast<std::uint16_t*>(object + 0x8A);
    *w = static_cast<std::uint16_t>(*w + 1);
}

// original 0x564150: entry 1 - ScriptFlags_Set40, run 3, step 5.
SC13_EXPORT void __cdecl Scena13_Object01(unsigned char* /*object*/, std::uint32_t /*bits*/) {
    Set40();
    SetRun(3);
    SetStep(5);
}

// original 0x564170: entry 2 - ScriptFlags_Set40, run 6, step 0x19.
SC13_EXPORT void __cdecl Scena13_Object02(unsigned char* /*object*/, std::uint32_t /*bits*/) {
    Set40();
    SetRun(6);
    SetStep(0x19);
}

// original 0x564190: entry 3 - flag 0x1C (through 0x929ED0, not the argument),
// ScriptFlags_Set40, then the three bytes 0x9039F3..F5 = 0x2C, 0, 0x11.
SC13_EXPORT void __cdecl Scena13_Object03(unsigned char* /*object*/, std::uint32_t /*bits*/) {
    Set(0x1C);
    Set40();
    B(at::kPendingKind) = 0x2C;
    B(at::kPendingKind + 1) = 0;
    B(at::kPendingKind + 2) = 0x11;
}

// A hook's start of a run: ScriptFlags_Set40, the run and the step, al 1.
static unsigned char Hit(unsigned char run, unsigned char step) {
    Set40();
    SetRun(run);
    SetStep(step);
    return 1;
}
// (u16)(v - lo) < n, as the hooks test a cell's range.
static bool In(std::uint32_t v, unsigned lo, unsigned n) { return static_cast<std::uint16_t>(v - lo) < n; }

// original 0x5641C0: slot 2, the step hook (Scenario_StepHook, x and z
// 16.16). x is compared whole (ebp) or by its high word, z by its high word;
// the leader's facing byte 0x802D48 is loaded into al at fixed points and
// area 0x90 stores that al to 0x903850 (below). Answers al 1 when it starts
// something, else 0.
SC13_EXPORT unsigned char __cdecl Scena13_StepHook(int x, int z) {
    const std::uint32_t xw = static_cast<std::uint32_t>(x);
    const std::uint32_t x16 = xw >> 16;
    const std::uint32_t z16 = static_cast<std::uint32_t>(z) >> 16;
    unsigned char facing = 0;   // al where the original loads 0x802D48
    if (Area() == 0x56) {
        if (FD() != 1) return 0;
        if (!Flag(0xE) && xw == 0x428000 && In(z16, 0x3A, 2)) return Hit(4, 0x14);
    }
    bool loaded = false;
    if (Area() == 0x70) {
        if (FD() != 4) return 0;
        if (!Flag(0xF) && In(x16, 0x36, 3) && In(z16, 0x32, 4)) {
            facing = B(at::kFacing);
            if (facing != 0 && facing != 7 && facing != 6) return Hit(4, 0);
            loaded = true;
        }
    }
    if (!loaded) facing = B(at::kFacing);
    if (Area() == 0x7F) {
        if (FD() != 0) return 0;
        if (xw != 0x38000) return 0;
        if (!In(z16, 6, 3)) return 0;
        Set(6);
        facing = B(at::kFacing);
    }
    if (Area() == 0x8F) {
        if (FD() == 0) {
            if (!Flag(0x14) && xw == 0x3B8000 && In(z16, 0x1F, 4)) return Hit(6, 0x14);
            if (Flag(0x1D) && !Flag(0x1E) && xw == 0x3B8000 && In(z16, 0x1F, 4)) return Hit(8, 5);
            facing = B(at::kFacing);
        }
        if (FD() == 1) {
            if (!Flag(0x11) && xw == 0x428000 && In(z16, 0x64, 3)) return Hit(6, 0);
            facing = B(at::kFacing);
        }
    }
    if (Area() == 0x90) {
        if (FD() != 2) return 0;
        if ((x16 & 0xFFFF) != 0x25) return 0;
        if (!In(z16, 0x1D, 2)) return 0;
        B(at::kSlotWord) = facing;
        if (facing != 6 && facing != 7 && facing != 0) return 0;
        if (!Flag(0x1B)) {
            Set40();
            if (!Flag(0x18)) {
                if (B(at::kLeaderName) == 2) DropIn(1);
            } else if (!Flag(0x19)) {
                const unsigned char who = B(at::kLeaderName);
                DropIn(who == 8 ? 4 : who == 5 ? 3 : 2);
            } else {
                const bool third = Flag(0x1A);
                const unsigned char who = B(at::kLeaderName);
                if (!third) {
                    if (who != 7) DropIn(5);
                } else if (who == 7) {
                    DropIn(8);
                }
            }
            SetCounter(0, 1);
            return 1;
        }
    }
    if (Area() == 0x91 && SH_CALL(Flags_Test)(Story(), 0x2D) != 0 && !Flag(0xB) && In(x16, 0xA, 2) && In(z16, 0x62, 3)) {
        const unsigned char f = B(at::kFacing);
        if (f == 0 || f == 7 || f == 6) {
            SetCounter(0, 0);
            DropIn(9);
            SetRun(3);
            SetStep(0x14);
            return 1;
        }
    }
    const unsigned area = Area();
    if (area == 0x97) {
        if (xw != 0x148000) return 0;
        if (!In(z16, 0x12, 9)) return 0;
        return Hit(8, 4);
    }
    if (area != 0x9B) return 0;
    if (FD() != 0) return 0;
    if (Flag(2)) return 0;
    if (xw != 0x28000) return 0;
    if (!In(z16, 0x30, 2)) return 0;
    return Hit(2, 0);
}

// original 0x5645D0: slot 3, the arrive hook (x, z 16.16, by their high
// words). Area 0x8F at Cond_ByteFD 0 with flag 0x1C set and 0x1D clear, x
// 0x2E and z 0x1F..0x22: run 8 step 0, al 1. Area 0x91 at Cond_ByteFD 5 with
// flag 0xA clear, x 0x40..0x41 and z 0x6A: flag 0xA and MoveCmd_TestFB(0x40,
// 0x6B). Otherwise al 0.
SC13_EXPORT unsigned char __cdecl Scena13_ArriveHook(int x, int z) {
    const std::uint32_t x16 = static_cast<std::uint32_t>(x) >> 16;
    const std::uint32_t z16 = static_cast<std::uint32_t>(z) >> 16;
    if (Area() == 0x8F) {
        if (FD() != 0) return 0;
        if (Flag(0x1C) && !Flag(0x1D) && x16 == 0x2E && In(z16, 0x1F, 4)) {
            Set40();
            SetRun(8);
            SetStep(0);
            return 1;
        }
    }
    if (Area() != 0x91) return 0;
    if (FD() != 5) return 0;
    if (Flag(0xA)) return 0;
    if (!In(x16, 0x40, 2)) return 0;
    if (z16 != 0x6A) return 0;
    Set(0xA);
    SH_CALL(MoveCmd_TestFB)(0x40, 0x6B);
    return 0;
}

// ===========================================================================
// Chapter 14

// original 0x5646A0: slot 0 - a tail jump through Scena14_States on the s8
// state 0x8034E2: 0 ScenaShared_State0, 1 Scena14_EnterArea, 2 Scena14_Run.
SC13_EXPORT void __cdecl Scena14_Frame(void) {
    const int state = static_cast<signed char>(B(at::kState));
    reinterpret_cast<VoidFn>(static_cast<std::uintptr_t>(Entry(at::kStates14, state, at::kStateCount14, "Scena14_Frame")))();
}

// original 0x5646B0: state 0 of chapters 5, 9, 10, 12, 14, 15, 18 and 19 (the
// eight state tables that hold it) - the state byte 1.
SC13_EXPORT void __cdecl ScenaShared_State0(void) { B(at::kState) = 1; }

// original 0x564E10: Gfx_ClutStrip's 0x2000 colours greyed - each the mean of
// its three 5-bit channels (the sum / 3, rounded toward 0) in all three, bit 15
// kept - and Gfx_ClutStripDirty 1.
SC13_EXPORT void __cdecl Scena14_GreyClut(void) {
    std::uint16_t* const c = &W(at::kClut);
    for (unsigned i = 0; i < at::kClutWords; ++i) {
        const std::uint32_t w = c[i];
        const std::int32_t sum = static_cast<std::int32_t>(((w >> 10) & 0x1F) + ((w >> 5) & 0x1F) + (w & 0x1F));
        const std::uint32_t grey = static_cast<std::uint32_t>(sum / 3);
        c[i] = static_cast<std::uint16_t>((((grey << 5) | grey) << 5) | (w & 0x8000) | grey);
    }
    B(at::kClutDirty) = 1;
}

// original 0x564E80: an effect record by Effect_FindFree, the slot to
// Scena14_Slot; for a slot, +0 = 1, +5 = kind, the dwords +0x64 / +0x68 /
// +0x6C = the sign-extended words x / y / z, +0xC = w, +9 = life, and al 1;
// none al 0.
SC13_EXPORT unsigned char __cdecl Scena14_SpawnEffect(unsigned kind, unsigned x, unsigned y, unsigned z, unsigned life,
                                                      std::uint32_t w) {
    const unsigned char slot = Take(at::kSlot14);
    if (slot == 0xFF) return 0;
    unsigned char* const e = Effect(slot);
    e[0] = 1;
    e[5] = static_cast<unsigned char>(kind);
    *reinterpret_cast<std::int32_t*>(e + 0x64) = static_cast<std::int16_t>(x);
    *reinterpret_cast<std::int32_t*>(e + 0x68) = static_cast<std::int16_t>(y);
    *reinterpret_cast<std::int32_t*>(e + 0x6C) = static_cast<std::int16_t>(z);
    *reinterpret_cast<std::uint32_t*>(e + 0xC) = w;
    e[9] = static_cast<unsigned char>(life);
    return 1;
}
static unsigned char Spawn(unsigned kind, unsigned x, unsigned y, unsigned z, unsigned life) {
    return SH_CALL(Scena14_SpawnEffect)(kind, x, y, z, life, 0);
}
static unsigned AngleXW() { return W(at::kAngleX); }
static unsigned AngleYW() { return W(at::kAngleY); }
static unsigned AngleFBW() { return W(at::kAngleFB); }

// The field's effect record at a fixed cell (0x1C8000, 0x268000): +0x38 and
// +0x34 the cell, +0 = 1, +5 = kind, +0x3C = (the elevation there + 0x80)
// << 16 (the elevation's low word sign-extended).
static void FxAtCell(unsigned slot, unsigned char kind) {
    unsigned char* const e = Effect(slot);
    *reinterpret_cast<std::uint32_t*>(e + 0x38) = 0x268000;
    const std::uint32_t z = *reinterpret_cast<std::uint32_t*>(e + 0x38);
    *reinterpret_cast<std::uint32_t*>(e + 0x34) = 0x1C8000;
    const std::uint32_t x = *reinterpret_cast<std::uint32_t*>(e + 0x34);
    e[0] = 1;
    e[5] = kind;
    const std::int32_t h = static_cast<std::int16_t>(SH_CALL(AreaMap_Elevation)(static_cast<long>(x), static_cast<long>(z)));
    *reinterpret_cast<std::uint32_t*>(e + 0x3C) = static_cast<std::uint32_t>(h + 0x80) << 16;
}

// Scena14_EnterArea's call-table entry with a party placement: CallA(n),
// DropIn(d), 0x532ED0(x, z, kind).
static void JoinAt(unsigned n, unsigned d, int x, int z, unsigned kind) {
    CallA(n);
    DropIn(d);
    SH_AT(PlaceFn, at::kPartyPlace)(x, z, kind);
}
static bool Pending1314() { return Flag(0x13) && !Flag(0x14); }

// original 0x5646C0: state 1, by the area; area 0xAC at Cond_ByteFD not 0
// jumps straight to the end. Every way out stores state 2.
static void EnterArea14Body() {
    if (Area() == 0xB && Pending1314()) {
        SetPass(0);   // the al of the Flags_Test just made, 0
        CallA(3);
        DropIn(5);
        W(at::kAngleX) = 0xFB9E;
        W(at::kAngleFB) = 0x3B8;
        SH_CALL(Scena14_GreyClut)();
    }
    if (Area() == 0x2B && Pending1314()) JoinAt(4, 5, 0x150000, 0x190000, 0x22);
    if (Area() == 0x67 && Pending1314()) JoinAt(5, 1, 0x4A0000, 0x220000, 0x1E);
    if (Area() == 0x78 && Pending1314()) {
        CallA(6);
        DropIn(8);
    }
    if (Area() == 0x8D) {
        if (!Flag(0x14)) {
            W(at::kCamDistance) = 0x7F00;
            CallA(9);
            DropIn(0);
            Spawn(0x31, AngleXW(), AngleYW(), AngleFBW(), 0xF0);
            Sound(0x200);
        }
        if (FD() == 1 && !Flag(0x18)) {
            Set(0x18);
            DropIn(4);
            SH_AT(VoidFn, at::kArea141c)();
        }
    }
    if (Area() == 0x8E) {
        if (!Flag(0x1B)) {
            Set(0x1B);
            CallA(0xA);
        }
        if (FD() == 2 && !Flag(0x1D)) {
            Set(0x1D);
            DropIn(1);
        }
        if (FD() == 3 && !Flag(0x1E)) {
            Set(0x1E);
            DropIn(2);
            SetRun(7);
            SetStep(6);
        }
    }
    if (Area() == 0x90 && Pending1314()) JoinAt(7, 9, 0x288000, 0x1D8000, 0x25);
    if (Area() == 0x93 && !Flag(0x35)) Set(0x35);
    if (Area() == 0x96) {
        if (!Flag(0xA)) {
            if (FD() == 0) {
                CallA(1);
                DropIn(0);
            }
            B(at::kCondFD) = 3;
            B(at::kCondFE) = 1;
        } else if (!Flag(0xB) && FD() == 0) {
            CallA(2);
            DropIn(2);
            SetPass(0);
            SetTimer(0x3C);
        }
    }
    if (Area() == 0xAC) {
        if (FD() != 0) return;
        if (!Flag(0x12)) {
            const unsigned y = AngleYW();
            W(at::kAngleX) = 0xFD56;
            W(at::kAngleFB) = 6;
            W(at::kCamDistance) = 0x900;
            Spawn(0x13, 0xFD56, y, 0x3A2, 0xE7);
        } else if (Flag(0x13)) {
            if (!Flag(0x14)) {
                SetPass(0x1F);
                CallA(8);
                DropIn(8);
            } else if (!Flag(0x1F)) {
                W(at::kCamDistance) = 0x200;
            } else if (!Flag(0x30)) {
                CallA(0xC);
                DropIn(0xB);
                Set(0x30);
                const unsigned char slot = SH_CALL(Effect_FindFree)();   // a slot of its own
                if (slot != 0xFF) FxAtCell(slot, 0x98);
            }
        }
    }
    if (Area() == 0xBD && Flag(1)) B(at::kEffects) = static_cast<unsigned char>(B(at::kEffects) | 0x40);
    if (Area() == 0xBF) {
        if (FD() == 0 && Flag(5)) {
            if (!Flag(6)) {
                W(at::kCamDistance) = 0x600;
                Spawn(0x31, AngleXW(), AngleYW(), AngleFBW(), 0xE8);
                SH_CALL(Scenario_CallB)(0);
                DropIn(0);
            } else if (!Flag(7)) {
                Set(7);
                DropIn(2);
            }
        }
        if (FD() == 1 && Flag(5) && Flag(8)) {
            Set(9);
            Clr(5);
            DropIn(5);
            D(at::kKind2X) = 0x468000;
            D(at::kKind2Z) = 0xD8000;
            SH_CALL(Field_ViewReset)();
            SetRun(2);
            SetStep(0x1E);
        }
    }
    if (Area() == 0xC1 && !Flag(0)) {
        Set(0);
        CallA(0);
        DropIn(0);
    }
    if (Area() != 0xC5) return;
    if (!Flag(0xD)) {
        Set(0xD);
        DropIn(1);
        SetRun(5);
        SetStep(0);
    }
    if (B(at::kKind2X + 4) == 5 && !Flag(0xF)) {   // the byte 0x905E68
        DropIn(2);
        SetRun(5);
        SetStep(0xA);
    }
}
SC13_EXPORT void __cdecl Scena14_EnterArea(void) {
    EnterArea14Body();
    B(at::kState) = 2;
}

// original 0x564EF0: state 2, a tail jump through Scena14_Runs on the s8 run:
// 0, 8 and 9 a bare ret (0x437CC0), 1..7 Run1..Run7.
SC13_EXPORT void __cdecl Scena14_Run(void) {
    const int run = static_cast<signed char>(B(at::kRun));
    reinterpret_cast<VoidFn>(static_cast<std::uintptr_t>(Entry(at::kRuns14, run, at::kRunCount14, "Scena14_Run")))();
}


// original 0x564F00: run 1, steps 0..7, 0x14, 0x19 (the byte table 0x56521C
// of 0x1A, then 10 cases). A choice with Menu_DrawHand while a message is up.
SC13_EXPORT void __cdecl Scena14_Run1(void) {
    const unsigned char step = Step();
    switch (step) {
    case 0:
        if (Request() == 2) return;
        SetCounter(0, 0);
        DropIn(1);
        SetStep(1);
        return;
    case 1: {
        if (Counter(0) != 1) return;
        Set(1);
        ChangeArea(0xBD, 0x10000000, 0x14000000, 2);
        const unsigned char bits = B(at::kStatusBits);
        B(at::kMusicCurrent) = B(at::kMusicTrack);
        B(at::kMusicTrack + 0x22) = 2;   // 0x904153
        B(at::kStatusBits) = static_cast<unsigned char>(bits | 1);
        W(0x90405C) = 0;
        SetStep(2);
        return;
    }
    case 2:
        if (!WaitClear()) return;
        Say(0x12);
        SetStep(3);
        return;
    case 3:
        if (Request() == 2) {
            SH_CALL(Menu_DrawHand)(0x98, 0x28, 0);
            return;
        }
        Spawn(0x13, AngleXW(), AngleYW(), 0xFE00, 0x20);
        SetStep(4);
        return;
    case 4:
        if (EffectBusy(at::kSlot14)) return;
        Say(0x13);
        SetStep(5);
        return;
    case 5:
        if (Request() == 2) {
            SH_CALL(Menu_DrawHand)(0x98, 0x38, 0);
            return;
        }
        Spawn(0x13, AngleXW(), AngleYW(), 0x400, 0x20);
        SetStep(6);
        return;
    case 6:
        if (EffectBusy(at::kSlot14)) return;
        Say(0x14);
        SetStep(7);
        return;
    case 7: {
        if (Request() == 2) {
            SH_CALL(Menu_DrawHand)(0x98, 0x10, 0);
            return;
        }
        Clear40();
        Clr(1);
        ChangeArea(0xC1, 0x1A8000, 0x1C0000, 0x82);
        const unsigned char bits = static_cast<unsigned char>(B(at::kStatusBits) & 0xFE);
        StopRun();
        B(at::kStatusBits) = bits;
        return;
    }
    case 0x14:
    case 0x19: {
        if (Request() == 2) return;
        if (step == 0x14) {
            SH_CALL(Flags_Clear)(Story(), 0x77);
            B(at::kMusicTrack + 0x21) = 0;   // 0x904152
            ChangeArea(0xC1, 0x1A8000, 0x1C0000, 0x83);
        } else {
            Set(2);
            Set(3);
            ChangeArea(0xC1, 0x1A8000, 0x1C0000, 0x84);
            B(at::kEntryByte) = 0xFF;
        }
        SH_CALL(Flags_Set)(Story(), 0x8A);
        const unsigned char bits = static_cast<unsigned char>(B(at::kStatusBits) & 0xFE);
        B(at::kSpriteMode) = 0xFF;
        B(at::kMusicCurrent) = 0xFF;
        StopRun();
        B(at::kStatusBits) = bits;
        return;
    }
    default:
        return;
    }
}

// original 0x565880: the camera shaken - MapView_Redraw 2, Camera_ShiftY +=
// Scena14_ShakeOffsets[Frame_Counter & 3] * amount (16-bit).
SC13_EXPORT void __cdecl Scena14_Shake(unsigned amount) {
    B(at::kRedraw) = 2;
    const std::int32_t offset = static_cast<signed char>(B(at::kShake14 + (D(at::kFrame) & 3)));
    W(at::kCamShiftY) = static_cast<std::uint16_t>(W(at::kCamShiftY) + offset * static_cast<std::int32_t>(amount & 0xFF));
}
static void Shake4() { SH_CALL(Scena14_Shake)(4); }

// The three party objects' +0x48 bytes (ObjTrio records 0..2) set to 1.
static void TrioFlags48() {
    B(at::kTrio + 2 * at::kTrioStride + 0x48) = 1;
    B(at::kTrio + at::kTrioStride + 0x48) = 1;
    B(at::kTrio + 0x48) = 1;
}

// original 0x565240: run 2, steps 0..8, 0xA..0x11, 0x14, 0x15, 0x19, 0x1D,
// 0x1E, 0x20..0x24 - a camera pull-out (Camera_Distance from the timer), a
// shaking wait, an event battle, and the 0xBD trip.
SC13_EXPORT void __cdecl Scena14_Run2(void) {
    switch (Step()) {
    case 0:
        if (Counter(0) != 2) return;
        SetTimer(0xF0);
        TrioFlags48();
        SetStep(1);
        return;
    case 1: {
        if (!TimerRuns()) {
            W(at::kCamDistance) = 0x76C;
            SH_AT(PlaceFn, at::kPartyPlace)(0x248000, 0x168000, 0x25);
            SetStep(2);
            B(at::kRedraw) = 2;
            TrioFlags48();
            return;
        }
        B(at::kRedraw) = 2;
        const std::int32_t d = 0xF0 - static_cast<std::int32_t>(Timer());
        W(at::kCamDistance) = static_cast<std::uint16_t>(static_cast<std::int32_t>(d * 1900) / 240);
        return;
    }
    case 2:
        if (Counter(0) != 4) return;
        B(at::kCondFE) = 1;
        Sound(0x202);
        SetTimer(0x1E);
        SetStep(3);
        return;
    case 3:
        if (Timer() != 0) {
            SetTimer(static_cast<std::uint16_t>(Timer() - 1));
            Shake4();
            return;
        }
        SetStep(4);
        BumpCounter0();
        return;
    case 4:
        if (Counter(0) != 6) return;
        Sound(0x202);
        SetTimer(0x3C);
        SetStep(5);
        return;
    case 5:
        if (Timer() != 0) {
            SetTimer(static_cast<std::uint16_t>(Timer() - 1));
            Shake4();
            return;
        }
        B(at::kCondFE) = 2;
        SetStep(6);
        BumpCounter0();
        return;
    case 6:
        if (Counter(0) != 8) return;
        B(at::kCondFE) = 3;
        Sound(0x200);
        SetTimer(0xD2);
        SetStep(7);
        return;
    case 7: {
        if (Timer() == 0) {
            W(at::kCamDistance) = 0;
            SetStep(8);
            BumpCounter0();
            B(at::kRedraw) = 2;
            return;
        }
        SetTimer(static_cast<std::uint16_t>(Timer() - 1));
        Shake4();
        const std::uint16_t t = Timer();
        B(at::kRedraw) = 2;
        if (t >= 0xB4) return;
        W(at::kCamDistance) = static_cast<std::uint16_t>(static_cast<std::int32_t>(t * 1900) / 180);
        return;
    }
    case 8:
        Sound(0x201);
        SH_AT(PlaceFn, at::kPartyPlace)(0x188000, 0x168000, 0x31);
        SH_CALL(Field_StartEventBattle)(0x31);
        SetStep(9);
        return;
    case 0xA:
        ChangeArea(0xBF, 0x1E8000, 0x228000, 1);
        Set(4);
        Set(5);
        SetStep(0xB);
        return;
    case 0xB:
        if (!WaitClear()) return;
        SetStep(0xC);
        return;
    case 0xC:
        if (Counter(0) != 1) return;
        Transition(0);
        SetStep(0xD);
        return;
    case 0xD:
        if (!WaitClear()) return;
        SetPass(0);
        Say(0x70);
        SetStep(0xE);
        return;
    case 0xE:
        if (Request() == 2) return;
        SetStep(0xF);
        return;
    case 0xF:
        ChangeArea(0xBF, 0x490000, 0xD0000, 0x81);
        B(at::kEntryByte) = 0xFF;
        B(at::kSpriteMode) = 0xFF;
        SetStep(0x10);
        return;
    case 0x10:
        if (!WaitClear()) return;
        SetPass(0x1F);
        Transition(1);
        SetStep(0x11);
        return;
    case 0x11: {
        if (Counter(0) != 0) return;
        const std::uint32_t r = static_cast<std::uint32_t>(B(at::kRecordIndex)) * at::kRecordStride;
        B(at::kRecords + r + 0x1E) = 9;
        W(at::kRecords + r + 0x18) = 1;
        Clear40();
        Set(6);
        StopRun();
        return;
    }
    case 0x14:
        Say(0x7D);
        SetStep(0x15);
        return;
    case 0x15:
        if (Request() == 2) return;
        EndRun();
        return;
    case 0x19:
        if (B(at::kTrio + 0x137) != 0) return;   // 0x802E77
        Clear40();
        DropIn(3);
        Set(8);
        SH_CALL(Inventory_Add)(0, 0x5A, 1);
        StopRun();
        return;
    case 0x1D:
        Clear40();
        Set(9);
        DropIn(4);
        StopRun();
        return;
    case 0x1E: {
        if (Counter(0) != 5) return;
        const unsigned char slot = Take(at::kSlot14);
        if (slot != 0xFF) Fx(slot, 0x91);
        SetStep(0x1F);
        return;
    }
    case 0x20: {
        ChangeArea(0xBD, 0x18000000, 0xC000000, 2);
        const unsigned char bits = static_cast<unsigned char>(B(at::kStatusBits) | 1);
        B(at::kMusicTrack + 0x22) = 2;   // 0x904153
        B(at::kStatusBits) = bits;
        W(0x90405C) = 0;
        SetStep(0x21);
        return;
    }
    case 0x21:
        if (!WaitClear()) return;
        SetStep(0x22);
        return;
    case 0x22:
        if (!TimerRuns()) {
            Say(4);
            SetStep(0x23);
        }
        B(at::kTrio + 9) = static_cast<unsigned char>(B(at::kTrio + 9) + 1);
        return;
    case 0x23:
        if (Request() != 2) SetStep(0x24);
        B(at::kTrio + 9) = static_cast<unsigned char>(B(at::kTrio + 9) + 1);
        return;
    case 0x24: {
        ChangeArea(0x96, 0x1C8000, 0x148000, 5);
        const unsigned char bits = static_cast<unsigned char>(B(at::kStatusBits) & 0xFE);
        B(at::kEntryByte) = 0;
        B(at::kStatusBits) = bits;
        SetRun(3);
        SetStep(0);
        return;
    }
    default:
        return;
    }
}

// original 0x5658B0: no table or call reaches it (below) - flags bit 0 of the
// bytes 0x802EB0 / 0x802FFC (ObjTrio records 1 and 2 + 0x24) cleared,
// Field_ChangeArea(0xC4, 0x298000, 0x168000, 0x80), ScriptFlags_Set40, run 2
// step 0, Field_StatusBits bit 0 cleared.
SC13_EXPORT void __cdecl Scena14_LeaveToC4(void) {
    B(at::kTrio + at::kTrioStride + 0x24) = static_cast<unsigned char>(B(at::kTrio + at::kTrioStride + 0x24) & 0xFE);
    B(at::kTrio + 2 * at::kTrioStride + 0x24) = static_cast<unsigned char>(B(at::kTrio + 2 * at::kTrioStride + 0x24) & 0xFE);
    ChangeArea(0xC4, 0x298000, 0x168000, 0x80);
    Set40();
    const unsigned char bits = static_cast<unsigned char>(B(at::kStatusBits) & 0xFE);
    SetRun(2);
    SetStep(0);
    B(at::kStatusBits) = bits;
}

// original 0x565910: run 3, steps 0..0xC.
SC13_EXPORT void __cdecl Scena14_Run3(void) {
    switch (Step()) {
    case 0:
        if (Counter(0) != 2) return;
        ChangeArea(0x96, 0x3C8000, 0x378000, 0x81);
        SetStep(1);
        return;
    case 1:
        if (Counter(0) != 8) return;
        Set(0xA);
        ChangeArea(0x96, 0x158000, 0x230000, 3);
        B(at::kSpriteMode) = 0xFF;
        B(at::kEntryByte) = 4;
        SetStep(2);
        return;
    case 2:
        if (!WaitClear()) return;
        SetStep(3);
        return;
    case 3:
        if (TimerRuns()) return;
        Transition(1);
        BumpCounter0();
        SetPass(0x1F);
        SetStep(4);
        return;
    case 4:
        if (Counter(0) != 0xA) return;
        ChangeArea(0x96, 0x3C8000, 0xD0000, 0x83);
        SetStep(5);
        return;
    case 5:
        if (Counter(0) != 0x18) return;
        Set(0xB);
        ChangeArea(0x96, 0x1A0000, 0x190000, 0x84);
        SetStep(6);
        return;
    case 6:
        if (Counter(0) != 0x1B) return;
        ChangeArea(0x96, 0x3B0000, 0x368000, 0x85);
        SetStep(7);
        return;
    case 7:
        if (Counter(0) != 0x24) return;
        Transition(0);
        SetStep(8);
        return;
    case 8:
        if (!WaitClear()) return;
        SetPass(0);
        Say(0x15);
        SetStep(9);
        return;
    case 9:
        if (Request() == 2) return;
        B(at::kRecords + static_cast<std::uint32_t>(B(at::kRecordIndex)) * at::kRecordStride + 0x1E) = 0;
        PartyPass();
        SH_CALL(Task_Sleep)(1);
        SetRequest(6);
        SetStep(0xA);
        return;
    case 0xA:
        if (Request() != 0) return;
        Set40();
        SetCounter(0, 0);
        Set(0xC);
        ChangeArea(0x96, 0x3C0000, 0x390000, 1);
        B(at::kSpriteMode) = 0xFF;
        B(at::kEntryByte) = 0xFF;
        SetStep(0xB);
        return;
    case 0xB:
        if (Request() != 0) return;
        SetPass(0x1F);
        Transition(1);
        SetStep(0xC);
        return;
    case 0xC:
        if (!WaitClear()) return;
        ScriptAnd(0xFFF7);
        Clear40();
        StopRun();
        return;
    default:
        return;
    }
}

// original 0x566000: with MapView_FocusX at exactly 0x77FF, the view moves
// east by 0x14 cells: MapView_Origin + 0x14, the kind-2 sprite's +0x34 and
// Field_Kind2X + 0x140000, the focus 0x63FF, the kind-2 sprite's elevation
// +0x3E from AreaMap_Elevation and MapView_SetElevation(it), the view shift
// 0x56FCA0, and Sprite_ObjectsExtra record 0's +0x34 + 0x140000 with its
// elevation +0x3E.
SC13_EXPORT void __cdecl Scena14_ScrollView(void) {
    if (D(at::kFocusX) != 0x77FF) return;
    const std::uint32_t kx = D(at::kKind2 + 0x34);
    const std::uint32_t fx = D(at::kKind2X);
    W(at::kOrigin) = static_cast<std::uint16_t>(W(at::kOrigin) + 0x14);
    D(at::kKind2 + 0x34) = kx + 0x140000;
    const std::uint32_t fz = D(at::kKind2Z);
    D(at::kFocusX) = 0x63FF;
    D(at::kKind2X) = fx + 0x140000;
    const long h = SH_CALL(AreaMap_Elevation)(static_cast<long>(fx + 0x140000), static_cast<long>(fz));
    W(at::kKind2 + 0x3E) = static_cast<std::uint16_t>(h);
    SH_CALL(MapView_SetElevation)(static_cast<int>(h));
    SH_AT(VoidFn, at::kViewShift)();
    const std::uint32_t ex = D(at::kExtra0 + 0x34) + 0x140000;
    const std::uint32_t ez = D(at::kExtra0 + 0x38);
    D(at::kExtra0 + 0x34) = ex;
    W(at::kExtra0 + 0x3E) = static_cast<std::uint16_t>(SH_CALL(AreaMap_Elevation)(static_cast<long>(ex), static_cast<long>(ez)));
}
static void Scroll() { SH_CALL(Scena14_ScrollView)(); }

// original 0x565BF0: run 4, steps 0..2, 0xA..0x18. Its ebx holds 0x18 from
// the start: step 0xB waits on counter 3 (0x90384B) at 0x18 and step 0x17
// stores it as the next step (below).
SC13_EXPORT void __cdecl Scena14_Run4(void) {
    switch (Step()) {
    case 0:
        SetCounter(0, 0);
        Set(0x10);
        ChangeArea(0x94, 0x180000, 0x268000, 0x82);
        SetStep(1);
        return;
    case 1:
        if (Counter(0) != 1) return;
        if (Spawn(0x13, 0xFCBC, AngleYW(), 0x284, 0x98) == 0) return;
        SetStep(2);
        return;
    case 2:
        if (Counter(0) != 2) return;
        if (Spawn(0x13, 0xFD56, AngleYW(), 0x200, 0x98) == 0) return;
        StopRun();
        return;
    case 0xA:
        DropIn(3);
        SetStep(0xB);
        return;
    case 0xB:
        if (Counter(3) != 0x18) return;   // cmp [0x90384B], bl - bl is the switch bound 0x18
        Transition(0);
        SetStep(0xC);
        return;
    case 0xC:
        if (!WaitClear()) return;
        SetPass(0);
        Say(0x14);
        SetStep(0xD);
        return;
    case 0xD:
        if (Request() == 2) return;
        SetStep(0xE);
        return;
    case 0xE:
        SH_CALL(Flags_Set)(Story(), 0x46);
        ChangeArea(0x95, 0x1C0000, 0xC0000, 0x80);
        B(at::kEntryByte) = 0xFF;
        B(at::kSpriteMode) = 0xFF;
        B(at::kMusicCurrent) = 0x27;
        SetStep(0xF);
        return;
    case 0xF: {
        if (!WaitClear()) return;
        const unsigned char slot = Take(at::kSlot14);
        if (slot != 0xFF) Fx(slot, 0x8D);
        Scroll();
        SetPass(0x1F);
        Transition(1);
        SetStep(0x10);
        return;
    }
    case 0x10:
        if (!WaitClear()) return;
        Scroll();
        Msg(1);
        SetRequest(2);
        SetStep(0x11);
        Effect(B(at::kSlot14))[1] = 2;
        return;
    case 0x11:
        Scroll();
        return;
    case 0x12:
        if (Request() != 2 && Spawn(0x13, 0xFB46, AngleYW(), 0x360, 0x78) != 0) {
            const std::int32_t v = static_cast<std::int16_t>(W(at::kF3Divisor));
            SetTimer(0x50);
            W(at::kF3Divisor) = static_cast<std::uint16_t>(v / 2);
            SetStep(0x13);
        }
        Scroll();
        return;
    case 0x13:
        if (!TimerRuns()) {
            Transition(0);
            SetStep(0x14);
        }
        Scroll();
        return;
    case 0x14:
        if (!WaitClear()) return;
        SetPass(0);
        Msg(2);
        SetRequest(2);
        SetStep(0x15);
        return;
    case 0x15:
        if (Request() == 2) return;
        SetStep(0x16);
        return;
    case 0x16:
        SetCounter(3, 0x20);
        ChangeArea(0xA7, 0x280000, 0x630000, 0x8A);
        B(at::kEntryByte) = 0xFF;
        B(at::kSpriteMode) = 0xFF;
        B(at::kMusicCurrent) = 0xFF;
        SetStep(0x17);
        return;
    case 0x17:
        if (!WaitClear()) return;
        Transition(1);
        SetPass(0x1F);
        SH_CALL(Music_LoadFile)(0x8C);
        SetStep(0x18);   // ebx, the switch bound
        return;
    case 0x18:
        if (Counter(3) != 0x31) return;
        if (SH_CALL(File_LoadDone)() == 0) return;
        SetCounter(3, static_cast<unsigned char>(Counter(3) + 1));
        Set(0x11);
        StopRun();
        return;
    default:
        return;
    }
}

// original 0x566090: run 5, steps 0..2, 5..7, 0xA..0xE, 0x10, 0x14, 0x15,
// 0x1E..0x21 (the byte table 0x566438 of 0x22, then 19 cases).
SC13_EXPORT void __cdecl Scena14_Run5(void) {
    switch (Step()) {
    case 0:
        if (Counter(0) != 1) return;
        if (Spawn(0x13, 0xFCE8, AngleYW(), 0x150, 0x70) == 0) return;
        SetStep(1);
        return;
    case 1:
        if (Counter(0) != 2) return;
        if (Spawn(0x13, 0xFD56, AngleYW(), 0x200, 0x40) == 0) return;
        SetStep(2);
        return;
    case 2:
        if (Counter(0) != 0) return;
        EndRun();
        return;
    case 5:
        if (Request() == 2) return;
        SH_CALL(Kind2_Place)(1);
        SetStep(6);
        return;
    case 6:
        if (Counter(0) != 1) return;
        Set(0xE);
        SetTimer(0x78);
        SetStep(7);
        return;
    case 7:
        if (TimerRuns()) return;
        SetCounter(0, 2);
        SetStep(2);
        return;
    case 0xA:
        if (Counter(0) != 0x1C) return;
        SH_AT(PlaceFn, at::kPartyPlace)(0xB8000, 0x728000, 0x25);
        SetStep(0xB);
        return;
    case 0xB:
        if (Counter(0) != 0x1E) return;
        SetTimer(0x10);
        TrioFlags48();
        SetStep(0xC);
        return;
    case 0xC: {
        if (TimerRuns()) {
            W(at::kCamDistance) = static_cast<std::uint16_t>(W(at::kCamDistance) + 0x58);
            B(at::kRedraw) = 2;
            Shake4();
            return;
        }
        const unsigned char slot = SH_CALL(Effect_FindFree)();   // a slot of its own
        if (slot != 0xFF) Fx(slot, 0x8E);
        SetTimer(0x64);
        SetStep(0xD);
        Shake4();
        return;
    }
    case 0xD:
        if (!TimerRuns()) {
            SetTimer(0x10);
            SetStep(0xE);
            BumpCounter0();
        }
        Shake4();
        return;
    case 0xE:
        if (TimerRuns()) {
            W(at::kCamDistance) = static_cast<std::uint16_t>(W(at::kCamDistance) - 0x58);
            B(at::kRedraw) = 2;
            return;
        }
        SH_CALL(Field_StartEventBattle)(0x32);
        SetStep(0xF);
        return;
    case 0x10:
        Clear40();
        ScriptAnd(0xFFF7);
        SetCounter(0, 0);
        Set(0xF);
        StopRun();
        return;
    case 0x14:
        Say(0x1C);
        SetStep(0x15);
        return;
    case 0x15:
        if (Request() == 2) return;
        EndRun();
        return;
    case 0x1E:
        SetCounter(0, 0);
        DropIn(3);
        SetStep(0x1F);
        return;
    case 0x1F:
        if (Counter(0) != 1) return;
        SH_AT(VoidFn, at::kSound587B80)();
        SH_CALL(Sound_LoadStream)(2);
        SH_AT(ArgFn, at::kParty591900)(0xD);
        Say(0x1D);
        SetStep(0x20);
        return;
    case 0x20:
        if (SH_CALL(Sound_StreamDone)() == 0) return;
        if (Request() == 2) return;
        SH_CALL(Sound_ResumeAll)();
        SetStep(0x21);
        BumpCounter0();
        return;
    case 0x21:
        if (Counter(0) != 0) return;
        EndRun();
        return;
    default:
        return;
    }
}

// original 0x567040: a line by who leads - for the first of the four bytes at
// `who` that any member's ObjTrio +0x89 (members below Field_MemberCount, read
// once) equals, Msg_OpenScript(lines[i]), Field_Request 2, and ax lines[i];
// none, ax 0xFFFF. Its callers do not read the answer.
SC13_EXPORT std::uint16_t __cdecl Scena14_TalkByMember(const unsigned char* who, const std::uint16_t* lines) {
    const unsigned char n = B(at::kMemberCount);
    for (unsigned i = 0; i < 4; ++i)
        for (unsigned m = 0; m < n; ++m)
            if (B(at::kLeaderName + m * at::kTrioStride) == who[i]) {
                Msg(lines[i]);
                SetRequest(2);
                return lines[i];
            }
    return 0xFFFF;
}
static void Talk(std::uint32_t who, std::uint32_t lines) {
    SH_CALL(Scena14_TalkByMember)(reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(who)),
                                  reinterpret_cast<const std::uint16_t*>(static_cast<std::uintptr_t>(lines)));
}

// Run 6's four repeated handlers (the jump table holds each at several steps):
// once the wait word is 0, the step + 1 read afresh after the call.
static void NextStep() { SetStep(static_cast<unsigned char>(Step() + 1)); }
static void GreyIn() {   // 0x566873
    if (!WaitClear()) return;
    SetPass(0x1F);
    SH_CALL(Scena14_GreyClut)();
    Transition(1);
    NextStep();
}
static void Hold3C() {   // 0x5668A6
    if (!WaitClear()) return;
    SetTimer(0x3C);
    NextStep();
}
static void HoldThenFade() {   // 0x5668CC
    if (TimerRuns()) return;
    Transition(0);
    NextStep();
}
static void AfterMessage() {   // 0x566922
    if (Request() == 2) return;
    NextStep();
}
static void Hold(std::uint16_t t, unsigned char next) {
    if (!WaitClear()) return;
    SetTimer(t);
    SetStep(next);
}
static void Recall(unsigned short msg, unsigned char next) {
    if (!WaitClear()) return;
    SetPass(0);
    Say(msg);
    SetStep(next);
}
static void Visit(unsigned area, int x, int z, unsigned flags, unsigned char next) {
    ChangeArea(area, x, z, flags);
    B(at::kEntryByte) = 0xFF;
    B(at::kSpriteMode) = 0xFF;
    B(at::kMusicCurrent) = 0xFF;
    SetStep(next);
}
// Steps 0x40..0x42: an effect of kind 0x94 with +1 = 3, the slot to
// Scena14_Slot.
static void Spawn94() {
    const unsigned char slot = Take(at::kSlot14);
    if (slot == 0xFF) return;
    Fx(slot, 0x94);
    Effect(slot)[1] = 3;
}
static void TrioOr40(unsigned k) { B(at::kTrio + k * at::kTrioStride) = static_cast<unsigned char>(B(at::kTrio + k * at::kTrioStride) | 0x40); }

// original 0x566460: run 6, steps 0..7, 0xA..0x49 (one jump table of 74) - a
// talk, a battle's lead-in, then a chain of visits (area, a greyed CLUT,
// a hold, a fade, a message) through areas 0x33, 0x43, 0x55, 0x2B, 0x67,
// 0x78, 0x90 and back to 0xAC.
SC13_EXPORT void __cdecl Scena14_Run6(void) {
    switch (Step()) {
    case 0:
        Set40();
        DropIn(0);
        SetStep(1);
        return;
    case 1:
        if (Counter(3) != 0x14) return;
        SH_CALL(Flags_Set)(Story(), 0x4E);
        ChangeArea(0xAC, 0x260000, 0x360000, 0x84);
        SetStep(2);
        return;
    case 2:
        if (!WaitClear()) return;
        if (EffectBusy(at::kSlot14)) return;
        Talk(at::kTalkWho14, at::kTalkLines14);
        SetStep(3);
        return;
    case 3:
        if (Request() == 2) return;
        SetCounter(0, 0xA);
        SetStep(4);
        return;
    case 4:
        if (Counter(0) != 0xB) return;
        if (Spawn(0x31, 0xFD56, AngleYW(), 0x200, 0x1E) == 0) return;
        SetStep(5);
        return;
    case 5:
        if (EffectBusy(at::kSlot14)) return;
        BumpCounter0();
        Talk(at::kTalkWho14 + 4, at::kTalkLines14 + 8);
        SetStep(6);
        return;
    case 6:
        if (Request() == 2) return;
        BumpCounter0();
        SetStep(7);
        return;
    case 7:
        if (Counter(0) != 0) return;
        Set(0x12);
        Clear40();
        StopRun();
        return;
    case 0xA:
        SetCounter(0, 0xF);
        DropIn(5);
        SetStep(0xB);
        return;
    case 0xB: {
        if (Counter(0) != 0x12) return;
        const unsigned char n = B(at::kMemberCount);
        SetStep(0xC);
        B(at::kSlotWord) = 0;
        for (unsigned char m = 0; m < n;) {
            if (B(at::kLeaderName + m * at::kTrioStride) == 4) {
                DropIn(6);
                return;
            }
            ++m;
            B(at::kSlotWord) = m;
        }
        DropIn(7);
        return;
    }
    case 0xC: {
        if (Counter(0) != 0x23) return;
        const unsigned char slot = Take(at::kSlot14);
        if (slot != 0xFF) Fx(slot, 0x94);
        SetTimer(0x5A);
        SetStep(0xD);
        return;
    }
    case 0xD:
        if (TimerRuns()) return;
        SetStep(0xE);
        BumpCounter0();
        return;
    case 0xE:
        if (Counter(0) != 0) return;
        Set(0x13);
        ChangeArea(0xB, 0x1E0000, 0xB0000, 0);
        B(at::kSpriteMode) = 0xFF;
        SetStep(0xF);
        return;
    case 0xF:
        if (Counter(0) != 1) return;
        SetPass(0x1F);
        Transition(1);
        SetStep(0x10);
        return;
    case 0x10:
        if (WaitClear()) {
            SetStep(0x11);
            BumpCounter0();
        }
        [[fallthrough]];
    case 0x11:
        if (Counter(0) != 3) return;
        Spawn(0x13, 0xFD56, AngleYW(), 0x200, 0x80);
        SetStep(0x12);
        return;
    case 0x12:
        if (Counter(0) != 0) return;
        Transition(0);
        SH_CALL(Music_FadeOut)(0x20);
        SetStep(0x13);
        return;
    case 0x13:
        if (!WaitClear()) return;
        FadeOutStop(0xA);
        SetPass(0);
        Say(0x30);
        SetStep(0x14);
        return;
    case 0x14:
        if (Request() == 2) return;
        SetStep(0x15);
        return;
    case 0x15: Visit(0x33, 0x30000, 0x70000, 0x82, 0x16); return;
    case 0x16: case 0x1C: case 0x22: case 0x28: case 0x2E: case 0x34: case 0x3A: GreyIn(); return;
    case 0x17: case 0x1D: case 0x23: case 0x29: Hold3C(); return;
    case 0x18: case 0x1E: case 0x24: case 0x2A: case 0x30: case 0x36: case 0x3C: HoldThenFade(); return;
    case 0x1A: case 0x20: case 0x26: case 0x2C: case 0x32: case 0x38: case 0x3E: AfterMessage(); return;
    case 0x19: Recall(0x27, 0x1A); return;
    case 0x1B: Visit(0x43, 0x1E0000, 0x360000, 0x86, 0x1C); return;
    case 0x1F: Recall(0x41, 0x20); return;
    case 0x21: Visit(0x55, 0x268000, 0x438000, 0x83, 0x22); return;
    case 0x25: Recall(0x14, 0x26); return;
    case 0x27: Visit(0x2B, 0x110000, 0x190000, 0, 0x28); return;
    case 0x2B: Recall(0x27, 0x2C); return;
    case 0x2D: Visit(0x67, 0x4C0000, 0x1F0000, 0, 0x2E); return;
    case 0x2F: Hold(0x32, 0x30); return;
    case 0x31: Recall(0xA, 0x32); return;
    case 0x33: Visit(0x78, 0x1F8000, 0x748000, 0, 0x34); return;
    case 0x35: Hold(0x28, 0x36); return;
    case 0x37: Recall(0x49, 0x38); return;
    case 0x39: Visit(0x90, 0x230000, 0x1D8000, 0, 0x3A); return;
    case 0x3B: Hold(0x1E, 0x3C); return;
    case 0x3D: Recall(0x49, 0x3E); return;
    case 0x3F:
        ChangeArea(0xAC, 0x68000, 0x2C8000, 0);
        B(at::kEntryByte) = 0xFF;
        B(at::kMusicCurrent) = 8;
        SetStep(0x40);
        return;
    case 0x40:
        if (Counter(0) != 0x14) return;
        Spawn94();
        TrioOr40(1);
        TrioOr40(2);
        Sound(0x208);
        SetStep(0x41);
        return;
    case 0x41:
        if (Counter(0) != 0x15) return;
        Spawn94();
        Sound(0x208);
        SetStep(0x42);
        return;
    case 0x42:
        if (Counter(0) != 0x16) return;
        Spawn94();
        TrioOr40(0);
        Sound(0x208);
        SetStep(0x43);
        return;
    case 0x43:
        if (Counter(0) != 0) return;
        ChangeArea(0x8D, 0x5F8000, 0xA8000, 0);
        B(at::kSpriteMode) = 0xE;
        B(at::kMusicCurrent) = 0xFF;
        SetStep(0x44);
        return;
    case 0x44:
        if (!WaitClear()) return;
        if (EffectBusy(at::kSlot14)) return;
        SH_CALL(Music_LoadFile)(0x50);
        SetStep(0x45);
        BumpCounter0();
        return;
    case 0x45:
        if (Counter(0) != 3) return;
        if (Spawn(0x13, 0xFCBC, AngleYW(), 0x34A, 0x20) == 0) return;
        SetStep(0x46);
        return;
    case 0x46:
        if (EffectBusy(at::kSlot14)) return;
        Set(0x14);
        SetTimer(0x1E);
        SetStep(0x47);
        return;
    case 0x47:
        if (TimerRuns()) return;
        if (Spawn(0x13, 0xFD56, AngleYW(), 0x200, 0x20) == 0) return;
        SetStep(0x48);
        BumpCounter0();
        return;
    case 0x48:
        if (EffectBusy(at::kSlot14)) return;
        SetStep(0x49);
        BumpCounter0();
        return;
    case 0x49:
        if (Counter(0) != 0) return;
        Clear40();
        SH_CALL(Music_Play)(0x50, 8);
        StopRun();
        return;
    default:
        return;
    }
}

// Run 7's first six steps: ScriptFlags_Clear40, a flag, a drop-in, an area's
// own object set-up where there is one, and the run's end.
static void Farewell(unsigned flag, unsigned member, std::uint32_t setup) {
    Clear40();
    Set(flag);
    DropIn(member);
    if (setup != 0) SH_AT(VoidFn, setup)();
    StopRun();
}

// Step 0x17's two copies: records 1..7 of the eight at 0x903A70 (+0xB bit 0
// set: +0x1C = +0x2E, +0x18 = +0x20, +0x1A = +0x22, +0x10 = 0), then for the
// members after the leader each ObjTrio record + 0x80 gets the 0xA4 bytes of
// the record MoveScript_EffectState[party list byte] names.
static void RestoreRecords() {
    for (unsigned r = 1; r < 8; ++r) {
        const std::uint32_t p = at::kRecords + r * at::kRecordStride;
        if ((B(p + 0xB) & 1) == 0) continue;
        B(p + 0x1C) = B(p + 0x2E);
        W(p + 0x18) = W(p + 0x20);
        W(p + 0x1A) = W(p + 0x22);
        W(p + 0x10) = 0;
    }
    const unsigned char n = B(at::kMemberCount);
    B(at::kSlotWord) = 1;
    if (n <= 1) return;
    B(at::kSlotWord) = n;
    for (unsigned j = 0; j + 1 < n; ++j) {
        const unsigned char c = B(at::kPartyList + 1 + j);
        const unsigned char idx = B(at::kEffectState + c);
        const unsigned char* const src = &B(at::kRecords + static_cast<std::uint32_t>(idx) * at::kRecordStride);
        unsigned char* const dst = &B(at::kTrio + (j + 1) * at::kTrioStride + 0x80);
        for (unsigned k = 0; k < at::kRecordStride; ++k) dst[k] = src[k];
    }
}

// original 0x5670E0: run 7, steps 0..7, 0xA..0x13, 0x14..0x20.
SC13_EXPORT void __cdecl Scena14_Run7(void) {
    switch (Step()) {
    case 0: Farewell(0x15, 1, 0); return;
    case 1: Farewell(0x16, 2, at::kArea141a); return;
    case 2: Farewell(0x17, 3, at::kArea141b); return;
    case 3: Farewell(0x19, 6, at::kArea141d); return;
    case 4: Farewell(0x1A, 7, at::kArea141e); return;
    case 5: Farewell(0x1C, 0, 0); return;
    case 6:
        if (Counter(0) != 7) return;
        SetPass(0);
        FadeOutStop(4);
        SetStep(7);
        SetTimer(8);
        return;
    case 7:
        if (TimerRuns()) return;
        BumpCounter0();
        StopRun();
        SetPass(0x1F);
        return;
    case 0xA:
        if (SH_CALL(File_LoadDone)() == 0) return;
        DropIn(3);
        SH_CALL(Music_Play)(0x9F, 8);
        SetStep(0xB);
        return;
    case 0xB:
        if (Counter(0) != 0x19) return;
        if (Request() == 2) return;
        SetPass(0);
        SetTimer(0xA);
        SetStep(0xC);
        return;
    case 0xC:
        if (TimerRuns()) return;
        SetPass(0x1F);
        BumpCounter0();
        SetStep(0xD);
        return;
    case 0xD:
        if (Counter(0) != 0) return;
        SH_AT(PlaceFn, at::kPartyPlace)(0x2C8000, 0x278000, 0x25);
        SH_CALL(Field_StartEventBattle)(0x33);
        SetStep(0xE);
        return;
    case 0xF:
        ChangeArea(0xAC, 0x208000, 0x268000, 0x89);
        B(at::kMusicCurrent) = 0xFF;
        SetStep(0x10);
        return;
    case 0x10:
        if (Counter(0) != 1) return;
        Spawn(0x31, AngleXW(), AngleYW(), AngleFBW(), 0x14);
        SH_CALL(Music_LoadFile)(0x9F);
        SetStep(0x11);
        return;
    case 0x11: {
        if (Counter(0) != 9) return;
        const unsigned char slot = SH_CALL(Effect_FindFree)();   // a slot of its own
        if (slot != 0xFF) FxAtCell(slot, 0x97);
        SetStep(0x12);
        return;
    }
    case 0x12:
        if (Counter(0) != 0xA) return;
        SetTimer(0x78);
        SetStep(0x13);
        return;
    case 0x14:
        if (!WaitClear()) return;
        BumpCounter0();
        SetPass(0);
        Say(0x22);
        SetStep(0x15);
        return;
    case 0x15:
        if (Request() == 2) return;
        CallA(0xB);
        SH_CALL(Task_Sleep)(1);
        SetRequest(6);
        SetStep(0x16);
        return;
    case 0x16:
        if (Request() != 0) return;
        SetRequest(1);
        B(at::kViewByte) = 1;
        SetStep(0x17);
        return;
    case 0x17: {
        if (Request() != 0) return;
        B(at::kOtSlot) = 4;
        B(at::kSortOnX) = 0;
        B(at::kRedraw) = 2;
        DropIn(0xA);
        SH_AT(PlaceFn, at::kPartyPlace)(0x258000, 0x278000, 0x22);
        RestoreRecords();
        SetPass(0x1F);
        Transition(1);
        SetTimer(0x40);
        SetStep(0x18);
        const unsigned char slot = SH_CALL(Effect_FindFree)();   // a slot of its own
        if (slot != 0xFF) FxAtCell(slot, 0x98);
        return;
    }
    case 0x18:
        if (TimerRuns()) return;
        SH_CALL(Field_StartEventBattle)(0x34);
        SetStep(0x19);
        return;
    case 0x1A:
        BumpCounter0();
        Set(0x1F);
        ChangeArea(0xAC, 0x208000, 0x268000, 3);
        StopRun();
        return;
    case 0x1E:
        SH_AT(VoidFn, at::kSound587B80)();
        SH_CALL(Sound_LoadStream)(2);
        SH_AT(ArgFn, at::kParty591900)(0xC);
        Say(0x2A);
        SetStep(0x1F);
        return;
    case 0x1F:
        if (SH_CALL(Sound_StreamDone)() == 0) return;
        if (Request() == 2) return;
        SH_CALL(Sound_ResumeAll)();
        SetStep(0x20);
        BumpCounter0();
        return;
    case 0x20:
        if (Counter(0) != 0) return;
        SetBit80();
        Set(0x31);
        Clear40();
        StopRun();
        return;
    default:
        return;
    }
}

// original 0x5677B0: slot 1 - Scena14_Objects[object +0x86] (object, the flag
// bits' pointer).
SC13_EXPORT void __cdecl Scena14_ObjectTrigger(unsigned char* object) {
    const std::uint32_t bits = D(at::kFlagBits);
    const unsigned index = object[0x86];
    reinterpret_cast<ObjectEntry>(static_cast<std::uintptr_t>(
        Entry(at::kObjects14, static_cast<int>(index), at::kObjectCount14, "Scena14_ObjectTrigger")))(object, bits);
}

// Scena14_Objects entries 0..4: the line area code picks for `who` - in area
// 0xC0 0x42C0A0(who), elsewhere 0x42BA90(who) (each an index into its own
// five-entry table, area 192's and area 191's) - opened, Field_Request 2.
static void AreaLine(unsigned who) {
    const unsigned id = Area() == 0xC0 ? SH_AT(ScriptIdFn, at::kArea192)(who) : SH_AT(ScriptIdFn, at::kArea191)(who);
    Msg(static_cast<unsigned short>(id));
    SetRequest(2);
}
// original 0x5677D0 / 0x567810 / 0x567850 / 0x567890 / 0x5678D0: entries 0..4,
// the line for 8, 4, 2, 5 and 6.
SC13_EXPORT void __cdecl Scena14_Object00(unsigned char* /*object*/, std::uint32_t /*bits*/) { AreaLine(8); }
SC13_EXPORT void __cdecl Scena14_Object01(unsigned char* /*object*/, std::uint32_t /*bits*/) { AreaLine(4); }
SC13_EXPORT void __cdecl Scena14_Object02(unsigned char* /*object*/, std::uint32_t /*bits*/) { AreaLine(2); }
SC13_EXPORT void __cdecl Scena14_Object03(unsigned char* /*object*/, std::uint32_t /*bits*/) { AreaLine(5); }
SC13_EXPORT void __cdecl Scena14_Object04(unsigned char* /*object*/, std::uint32_t /*bits*/) { AreaLine(6); }

// original 0x567910: entry 5 - ScriptFlags_Set40, run 5, step 5.
SC13_EXPORT void __cdecl Scena14_Object05(unsigned char* /*object*/, std::uint32_t /*bits*/) {
    Set40();
    SetRun(5);
    SetStep(5);
}

// original 0x567930: entry 6 - ScriptFlags_Set40, run 5, step 0x1E, the
// object's word +0x8A + 1.
SC13_EXPORT void __cdecl Scena14_Object06(unsigned char* object, std::uint32_t /*bits*/) {
    Set40();
    SetRun(5);
    SetStep(0x1E);
    std::uint16_t* const w = reinterpret_cast<std::uint16_t*>(object + 0x8A);
    *w = static_cast<std::uint16_t>(*w + 1);
}

// original 0x567950: entry 7 - ScriptFlags_Set40, run 7, step 0x1E, +0x8A + 1.
SC13_EXPORT void __cdecl Scena14_Object07(unsigned char* object, std::uint32_t /*bits*/) {
    Set40();
    SetRun(7);
    SetStep(0x1E);
    std::uint16_t* const w = reinterpret_cast<std::uint16_t*>(object + 0x8A);
    *w = static_cast<std::uint16_t>(*w + 1);
}

// original 0x567970: slot 2, the step hook. Area 0x94 at Cond_ByteFD 4 (any
// other value answers 0 at once) with flag 0x10 clear, x exactly 0x738000 and
// z 0x36..0x37: run 4 step 0, al 1. Area 0xBF with flag 5 set: the cell byte
// 0xA6 (AreaMap_ByteAt) under the step - the cell (x, z), and (x + 1, z) /
// (x, z + 1) / (x + 1, z + 1) where x's / z's fraction is not 0 - starts run 2
// at step 0x14, al 1. Otherwise al 0.
SC13_EXPORT unsigned char __cdecl Scena14_StepHook(int x, int z) {
    const std::uint32_t xw = static_cast<std::uint32_t>(x);
    const std::uint32_t zw = static_cast<std::uint32_t>(z);
    const unsigned x16 = xw >> 16, z16 = zw >> 16;
    if (Area() == 0x94) {
        if (FD() != 4) return 0;
        if (!Flag(0x10) && xw == 0x738000 && In(z16, 0x36, 2)) return Hit(4, 0);
    }
    if (Area() != 0xBF) return 0;
    if (!Flag(5)) return 0;
    const auto at = [](unsigned cx, unsigned cz) {
        return SH_CALL(AreaMap_ByteAt)(static_cast<short>(cx), static_cast<short>(cz)) == 0xA6;
    };
    unsigned char hits = at(x16, z16) ? 1 : 0;
    if ((xw & 0xFFFF) != 0 && at(x16 + 1, z16)) ++hits;
    if ((zw & 0xFFFF) != 0 && at(x16, z16 + 1)) ++hits;
    if ((xw & 0xFFFF) != 0 && (zw & 0xFFFF) != 0 && at(x16 + 1, z16 + 1)) ++hits;
    if (hits == 0) return 0;
    return Hit(2, 0x14);
}

// original 0x567A90: slot 3, the arrive hook. Starts runs by rectangles (x
// whole, signed, below a bound; z by its high word; area 0x8E's first by x's
// high word and z whole) in areas 0x8D, 0x8E, 0xAC - answering al 0 even
// then - and in 0x94 and 0xBF, answering al 1; Cond_ByteFD not 0 in 0x94 or
// 0xAC answers 0 at once.
SC13_EXPORT unsigned char __cdecl Scena14_ArriveHook(int x, int z) {
    const unsigned x16 = static_cast<std::uint32_t>(x) >> 16;
    const unsigned z16 = static_cast<std::uint32_t>(z) >> 16;
    const auto start = [](unsigned char run, unsigned char step) {
        Set40();
        SetRun(run);
        SetStep(step);
    };
    if (Area() == 0x8D) {
        if (FD() == 0) {
            if (!Flag(0x15)) {
                if (x < 0x4F8000 && In(z16, 7, 8)) start(7, 0);
            } else if (!Flag(0x16)) {
                if (x < 0x498000 && In(z16, 9, 4)) start(7, 1);
            } else if (!Flag(0x17)) {
                if (x < 0x408000 && In(z16, 9, 4)) start(7, 2);
            }
        }
        if (FD() == 1 && SH_CALL(Flags_Test)(Story(), 0x7A) != 0) {
            if (!Flag(0x19)) {
                if (x < 0x398000 && In(z16, 0x31, 4)) start(7, 3);
            } else if (!Flag(0x1A)) {
                if (x < 0x2D8000 && In(z16, 0x31, 4)) start(7, 4);
            }
        }
    }
    if (Area() == 0x8E) {
        if (FD() == 1 && !Flag(0x1C) && z < 0x1D8000 && In(x16, 0x13, 4)) start(7, 5);
        if (FD() == 3 && !Flag(0x1F) && x < 0x2C8000 && In(z16, 0x26, 4)) {
            Set40();
            FadeOutStop(0xA);
            SH_CALL(Music_LoadFile)(0x9F);
            SetRun(7);
            SetStep(0xA);
        }
    }
    if (Area() == 0x94) {
        if (FD() != 0) return 0;
        if (!Flag(0x11) && (z16 & 0xFFFF) == 0x1C && In(x16, 0x12, 3)) {
            start(4, 0xA);
            return 1;
        }
    }
    if (Area() == 0xAC) {
        if (FD() != 0) return 0;
        if (!Flag(0x13) && x < 0xB8000 && In(z16, 0x3F, 2)) start(6, 0xA);
    }
    if (Area() != 0xBF) return 0;
    if (FD() != 0) return 0;
    if (!In(x16, 0x1D, 5)) return 0;
    if (!In(z16, 0x14, 2)) return 0;
    if (!Flag(8)) return 0;
    if (Flag(9)) return 0;
    start(2, 0x1D);
    return 1;
}

void ScenaSc13_Inject() {
    if (bof3::WantsShadow("scena_sc13")) scena_sc13::SelfTest();
    BOF3_INJECT(Scena13_Frame);
    BOF3_INJECT(Scena13_Start);
    BOF3_INJECT(Scena13_EnterArea);
    BOF3_INJECT(Scena13_Run);
    BOF3_INJECT(Scena13_Run1);
    BOF3_INJECT(Scena13_Run2);
    BOF3_INJECT(Scena13_SpawnPairA);
    BOF3_INJECT(Scena13_SpawnPairB);
    BOF3_INJECT(Scena13_Run3);
    BOF3_INJECT(Scena13_Run4);
    BOF3_INJECT(Scena13_Run5);
    BOF3_INJECT(Scena13_ToneLevels);
    BOF3_INJECT(Scena13_Run6);
    BOF3_INJECT(Scena13_Run7);
    BOF3_INJECT(Scena13_Caption);
    BOF3_INJECT(Scena13_Run8);
    BOF3_INJECT(Scena13_ObjectTrigger);
    BOF3_INJECT(Scena13_Object00);
    BOF3_INJECT(Scena13_Object01);
    BOF3_INJECT(Scena13_Object02);
    BOF3_INJECT(Scena13_Object03);
    BOF3_INJECT(Scena13_StepHook);
    BOF3_INJECT(Scena13_ArriveHook);
    BOF3_INJECT(Scena14_Frame);
    BOF3_INJECT(ScenaShared_State0);
    BOF3_INJECT(Scena14_EnterArea);
    BOF3_INJECT(Scena14_GreyClut);
    BOF3_INJECT(Scena14_SpawnEffect);
    BOF3_INJECT(Scena14_Run);
    BOF3_INJECT(Scena14_Run1);
    BOF3_INJECT(Scena14_Run2);
    BOF3_INJECT(Scena14_Shake);
    BOF3_INJECT(Scena14_LeaveToC4);
    BOF3_INJECT(Scena14_Run3);
    BOF3_INJECT(Scena14_Run4);
    BOF3_INJECT(Scena14_ScrollView);
    BOF3_INJECT(Scena14_Run5);
    BOF3_INJECT(Scena14_Run6);
    BOF3_INJECT(Scena14_TalkByMember);
    BOF3_INJECT(Scena14_Run7);
    BOF3_INJECT(Scena14_ObjectTrigger);
    BOF3_INJECT(Scena14_Object00);
    BOF3_INJECT(Scena14_Object01);
    BOF3_INJECT(Scena14_Object02);
    BOF3_INJECT(Scena14_Object03);
    BOF3_INJECT(Scena14_Object04);
    BOF3_INJECT(Scena14_Object05);
    BOF3_INJECT(Scena14_Object06);
    BOF3_INJECT(Scena14_Object07);
    BOF3_INJECT(Scena14_StepHook);
    BOF3_INJECT(Scena14_ArriveHook);
}

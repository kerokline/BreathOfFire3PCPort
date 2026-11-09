// Moved here by tools/move_funcs.py from scena_sc13.cpp (docs/source-layout-round.md).
#include "game/scenario/scena_13_2.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/record_views.h"
#include "game/scena_sc13.h"
#include "game/scena_sc13_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace {

namespace at = scena_sc13::at;

}  // namespace

// Group HT of the button round: a face or shoulder bit the original tests by
// a constant is Japan's, asked through σ (docs/button-tests.md).
using bof3x::input::sigma::Sigma;

namespace {

using scena_sc13::ArgFn;

using scena_sc13::ByteFn;

using scena_sc13::FindByteFn;

using scena_sc13::LevelFn;

using scena_sc13::ObjectEntry;

using scena_sc13::PlaceFn;

using scena_sc13::VoidFn;

unsigned char& B(std::uint32_t a) { return *reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a)); }

std::uint16_t& W(std::uint32_t a) { return *reinterpret_cast<std::uint16_t*>(static_cast<std::uintptr_t>(a)); }

std::uint32_t& D(std::uint32_t a) { return *reinterpret_cast<std::uint32_t*>(static_cast<std::uintptr_t>(a)); }

std::int32_t S16(std::uint32_t a) { return static_cast<std::int16_t>(W(a)); }

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

void AddElevOffset(std::uint16_t v) { MapView_ElevationOffset = static_cast<std::uint16_t>(MapView_ElevationOffset + v); }

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

unsigned char* Sprite(unsigned slot) { return &B(::bof3::Img(scena_sc13::at::kSprites) + (slot & 0xFFu) * scena_sc13::at::kSpriteStride); }

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

std::int32_t AngleY() { return Camera_Angles[1]; }

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

// original 0x561E00: state 1, by the area. Three failed tests jump straight
// to the end (area 0x56 at Cond_ByteFD 1 with the word 0x802290 not 0x70,
// area 0x58 with it not 0x91, area 0x70 at Cond_ByteFD not 4); every way out
// stores state 2.
static void EnterArea13Body() {
    if (Area() == 0x56) {
        if (FD() == 1) {
            if (AreaW2c_LastArea != 0x70) return;
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
        if (AreaW2c_LastArea != 0x91) return;
        if (!Flag(0xB)) {
            Camera_Angles[0] = 0xFD2A;
            W(::bof3::Img(scena_sc13::at::kAngleFB)) = 0x2F2;
            Camera_Distance = 0x5DC;
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
        Camera_Angles[0] = 0xFC7A;
        W(::bof3::Img(scena_sc13::at::kAngleFB)) = 0x150;
        SetRun(1);
        SetStep(0);
    }
    if (Area() == 0x8F) {
        if (FD() == 0) {
            if (!Flag(0x11)) {
                Camera_Angles[0] = 0xFD40;
                W(::bof3::Img(scena_sc13::at::kAngleFB)) = 0x10E;
                const unsigned char slot = Take(::bof3::Img(Scena13_Slot_addr));
                if (slot != 0xFF) FxCam(slot, 0x13, -0x2AA, AngleY(), 0x2C6, 0xFF);
            } else if (!Flag(0x12)) {
                SH_AT(ArgFn, scena_sc13::at::kArea143.v)(1);
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
                SH_AT(ArgFn, scena_sc13::at::kArea143.v)(0);
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
        Camera_Distance = 0xF200;
        const unsigned char slot = Take(::bof3::Img(Scena13_Slot_addr));
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

// Run 2's effect of kind 0x75 (a slot taken, kind stored; none is no matter).
static void Spawn75() {
    const unsigned char slot = Take(::bof3::Img(Scena13_Slot_addr));
    if (slot != 0xFF) Fx(slot, 0x75);
}

// Two free sprite records taken with 0x57CD90 (the first's +0 set to 1 before
// the second is looked for, and back to 0 when there is none), then each
// record's +0 set and its slot to the word 0x903850 before EventOp_6x runs
// one of two 16-byte event-op records of Scena13_EventOps on it.
static void SpawnPair(std::uint32_t first, std::uint32_t second) {
    const unsigned char a = SH_AT(ByteFn, scena_sc13::at::kFreeSprite.v)();
    if (a == 0xFF) return;
    Sprite(a)[0] = 1;
    const unsigned char b = SH_AT(ByteFn, scena_sc13::at::kFreeSprite.v)();
    if (b == 0xFF) {
        Sprite(a)[0] = 0;
        return;
    }
    scena_sc13::at::kSlotWord.w_00 = a;
    Sprite(b)[0] = 1;
    SH_CALL(EventOp_6x)(reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(first)));
    scena_sc13::at::kSlotWord.w_00 = b;
    SH_CALL(EventOp_6x)(reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(second)));
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

// Run 7's countdown after a caption: while the timer is not 0, one less and
// true.
static bool CountDown() {
    if (Timer() == 0) return false;
    SetTimer(static_cast<std::uint16_t>(Timer() - 1));
    return true;
}

static void Caption(unsigned index, unsigned seconds) { SH_CALL(Scena13_Caption)(index, seconds); }

// A hook's start of a run: ScriptFlags_Set40, the run and the step, al 1.
static unsigned char Hit(unsigned char run, unsigned char step) {
    Set40();
    SetRun(run);
    SetStep(step);
    return 1;
}

// (u16)(v - lo) < n, as the hooks test a cell's range.
static bool In(std::uint32_t v, unsigned lo, unsigned n) { return static_cast<std::uint16_t>(v - lo) < n; }

// original 0x561DB0: slot 0, Field_ModeDispatch's call every field frame - a
// tail jump through Scena13_States on the s8 state 0x8034E2: 0
// Scena13_Start, 1 Scena13_EnterArea, 2 Scena13_Run.
SC13_EXPORT void __cdecl Scena13_Frame(void) {
    const int state = static_cast<signed char>(MoveScript_PartyRecords[0x62]);
    reinterpret_cast<VoidFn>(static_cast<std::uintptr_t>(Entry(::bof3::Img(Scena13_States_addr), state, scena_sc13::at::kStateCount13, "Scena13_Frame")))();
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
    MoveScript_PartyRecords[0x62] = 1;
}

SC13_EXPORT void __cdecl Scena13_EnterArea(void) {
    EnterArea13Body();
    MoveScript_PartyRecords[0x62] = 2;
}

// original 0x562280: state 2, a tail jump through Scena13_Runs on the s8 run
// MoveScript_Var7: 0 a bare ret (0x437CC0), 1..8 Run1..Run8.
SC13_EXPORT void __cdecl Scena13_Run(void) {
    const int run = MoveScript_Var7;
    reinterpret_cast<VoidFn>(static_cast<std::uintptr_t>(Entry(::bof3::Img(Scena13_Runs_addr), run, scena_sc13::at::kRunCount13, "Scena13_Run")))();
}

// original 0x562290: run 1, steps 0..5.
SC13_EXPORT void __cdecl Scena13_Run1(void) {
    switch (Step()) {
    case 0: {
        const unsigned char slot = Take(::bof3::Img(Scena13_Slot_addr));
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
        Area_EnterTransition = 0xFF;
        ModeFlow_AreaTransition = 0xFF;
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
        ModeFlow_AreaTransition = 0xFF;
        AreaW1f_MusicCurrent = 0xFF;
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
        AreaW1f_MusicCurrent = 0xFF;
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
        Area_EnterTransition = 0xFF;
        ModeFlow_AreaTransition = 0xFF;
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

// original 0x562700: the pair of event objects Scena13_EventOps records 0 and
// 1 set up (Scena13_EnterArea, area 0xC2, flag 2 clear).
SC13_EXPORT void __cdecl Scena13_SpawnPairA(void) { SpawnPair(::bof3::Img(Scena13_EventOps_addr), ::bof3::Img(Scena13_EventOps_addr) + 0x10); }

// original 0x5627A0: the same with records 2 and 3 (area 0xC2, flags 3 set
// and 4 clear).
SC13_EXPORT void __cdecl Scena13_SpawnPairB(void) { SpawnPair(::bof3::Img(Scena13_EventOps_addr) + 0x20, ::bof3::Img(Scena13_EventOps_addr) + 0x30); }

// original 0x562840: run 3, steps 0..5, 0xA..0xD, 0x14..0x18 (MSVC's
// two-level switch: the byte table 0x562CD0 of 0x19, then 16 cases).
SC13_EXPORT void __cdecl Scena13_Run3(void) {
    auto* const kind2 = &Sprite_Kind2;
    switch (Step()) {
    case 0: {
        if (Counter(0) != 2) return;
        const unsigned char slot = Take(::bof3::Img(Scena13_Slot_addr));
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
        Cond_ByteFE = 1;
        SetStep(2);
        return;
    case 2: {
        if (Counter(0) != 3) return;
        const unsigned char slot = Take(::bof3::Img(Scena13_Slot_addr));
        if (slot == 0xFF) return;
        SetStep(3);
        SetTimer(0x20);
        FxCam(slot, 0x13, -0x2AA, AngleY(), 0x200, 0x40);
        return;
    }
    case 3:
        if (TimerRuns()) return;
        AddElevOffset(0x800);
        Cond_ByteFE = 0;
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
        SH_AT(FindByteFn, scena_sc13::at::kFindByte.v)(0xB);
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
        Field_Kind2X = 0xA0000;
        kind2->x = 0xA0000;
        Field_Kind2Z = 0x620000;
        kind2->z = 0x620000;
        kind2->height_whole = static_cast<std::uint16_t>(SH_CALL(AreaMap_Elevation)(0xA0000, 0x620000));
        SH_CALL(Field_ViewReset)();
        Msg(0x19);
        SetRequest(2);
        Transition(1);
        SetPass(0x1F);
        Camera_Angles[0] = 0xFCBC;
        W(::bof3::Img(scena_sc13::at::kAngleFB)) = 0x3A2;
        const unsigned char slot = Take(::bof3::Img(Scena13_Slot_addr));
        if (slot == 0xFF) return;
        SetStep(0xD);
        FxCam(slot, 0x13, -0x2AA, AngleY(), 0x200, 0x64);
        return;
    }
    case 0xD:
        if (EffectBusy(::bof3::Img(Scena13_Slot_addr))) return;
        if (Request() == 2) return;
        Set(9);
        ChangeArea(0x91, 0x3F8000, 0x5E0000, 0x88);
        StopRun();
        Music_Track = 0x5C;
        return;
    case 0x14:
        if (Counter(0) != 1) return;
        Set40();
        ChangeArea(0x58, 0x130000, 0x250000, 0x80);
        Area_EnterTransition = 1;
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
        ModeFlow_AreaTransition = 0;
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
    auto* const member0 = ObjTrio;
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
        const unsigned char arg = B(::bof3::Img(scena_sc13::at::kEffectArg) + ObjTrio[0].char_id);
        Scena13_Slot[0] = SH_CALL(Effect_SpawnAt)(4, 5, static_cast<signed char>(arg), static_cast<long>(static_cast<std::uint32_t>(member0->x)),
                                                  static_cast<long>(static_cast<std::uint32_t>(member0->z)), static_cast<long>(static_cast<std::uint32_t>(member0->height)));
        SetStep(0xB);
        return;
    }
    case 0xB:
        if (EffectBusy(::bof3::Img(Scena13_Slot_addr))) return;
        Clear40();
        Set(0xD);
        StopRun();
        return;
    case 0x14:
        SetCounter(0, 0);
        ChangeArea(0x56, 0xC0000, 0x98000, 3);
        AreaW1f_MusicCurrent = 0xFF;
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
        AreaW1f_MusicCurrent = 0xFF;
        Area_EnterTransition = 1;
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
        ModeFlow_AreaTransition = 0;
        AreaW1f_MusicCurrent = 0x5C;
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
        Area_EnterTransition = 0xFF;
        ModeFlow_AreaTransition = 0xFF;
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

// original 0x563090: run 5, steps 0..3, 5, 6, 0xA..0x10 - a dial: the dword
// Sprite_ObjectsExtra record 1 + 0x6C turned by 0x10 while Input_Held bit
// 0x2000 (up) or 0x8000 (down) is held, the two voices following it, until
// Input_Pressed bit 0x20; 0x4A0..0x4E0 goes on to step 0xA, anything else to 5.
SC13_EXPORT void __cdecl Scena13_Run5(void) {
    auto* const xobj1 = &Sprite_ObjectsExtra[1];
    switch (Step()) {
    case 0:
        FadeOutStop(8);
        Sound(0x208);
        Sound(0x207);
        SetStep(1);
        return;
    case 1:
        SH_CALL(Scena13_ToneLevels)(xobj1->angle[2] & 0xFFF);
        SetStep(2);
        return;
    case 2: {
        std::uint32_t a = xobj1->angle[2];
        scena_sc13::at::kSlotWord.w_00 = static_cast<std::uint16_t>(a & 0xFFF);
        const std::uint32_t held = D(::bof3::Img(scena_sc13::at::kInputHeld));
        bool turned = false;
        if (held & 0x2000) {
            a += 0x10;
            turned = true;
        } else if (held & 0x8000) {
            a -= 0x10;
            turned = true;
        }
        if (turned) {
            xobj1->angle[2] = a;
            SH_CALL(Scena13_ToneLevels)(a & 0xFFF);
        }
        if (B(::bof3::Img(scena_sc13::at::kInputPressed)) & Sigma(0x20)) SetStep(3);
        return;
    }
    case 3: {
        const std::uint16_t a = static_cast<std::uint16_t>(xobj1->angle[2] & 0xFFF);
        scena_sc13::at::kSlotWord.w_00 = a;
        SetStep(a <= 0x4E0 && a >= 0x4A0 ? 0xA : 5);
        return;
    }
    case 5:
        Sound(0x20A);
        Sound(0x209);
        SH_CALL(Music_Play)(Music_Track, 0x20);
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
        SH_CALL(Music_Play)(Music_Track, 0x20);
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

SC13_EXPORT void __cdecl Scena13_ToneLevels(unsigned angle) {
    const std::int32_t a = static_cast<std::int32_t>(angle & 0xFFFF);
    SH_AT(LevelFn, scena_sc13::at::kVoiceLevel.v)(0x208, Level((a - 0x4C0) >> 1));
    SH_AT(LevelFn, scena_sc13::at::kVoiceLevel.v)(0x207, Level((a - 0xCC0) >> 1));
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
        const unsigned char slot = Take(::bof3::Img(Scena13_Slot_addr));
        if (slot == 0xFF) return;
        SetStep(2);
        FxCam(slot, 0x13, Camera_Angles[0], AngleY(), 0x200, 0x20);
        return;
    }
    case 2:
        if (EffectBusy(::bof3::Img(Scena13_Slot_addr))) return;
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
        Area_EnterTransition = 0xFF;
        AreaW1f_MusicCurrent = 0xFF;
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
        Camera_Angles[0] = 0xFE32;
        Camera_Distance = 0xF880;
        const unsigned char slot = Take(::bof3::Img(Scena13_Slot_addr));
        if (slot != 0xFF) FxCam0(slot, 0x31, -0x2AA, AngleY(), S16(::bof3::Img(scena_sc13::at::kAngleFB)), 0x3C);
        SetStep(7);
        return;
    }
    case 7:
        if (EffectBusy(::bof3::Img(Scena13_Slot_addr))) return;
        SetStep(8);
        BumpCounter0();
        return;
    case 8: {
        if (Counter(0) != 0xB) return;
        const unsigned char slot = Take(::bof3::Img(Scena13_Slot_addr));
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
        ModeFlow_AreaTransition = 0xFF;
        Area_EnterTransition = 0xFF;
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
        B(::bof3::Img(scena_sc13::at::kScriptFlags)) = static_cast<unsigned char>(B(::bof3::Img(scena_sc13::at::kScriptFlags)) | 0x10);
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
        const unsigned char* const o = reinterpret_cast<const unsigned char*>(AreaW1a_FocusObject);
        const std::uint32_t x = *reinterpret_cast<const std::uint32_t*>(o + 0x34);
        ObjTrio[2].x = x;
        const std::uint32_t z = *reinterpret_cast<const std::uint32_t*>(o + 0x38);
        ObjTrio[2].z = z;
        ObjTrio[2].height_whole =
            static_cast<std::uint16_t>(SH_CALL(AreaMap_Elevation)(static_cast<int>(x), static_cast<int>(z)));
        *reinterpret_cast<unsigned char*>(AreaW1a_FocusObject) = 0;
        SetStep(0x1A);
        return;
    }
    case 0x1A:
        if (Counter(0) != 2) return;
        Set(0x14);
        Clr(0x22);
        ChangeArea(0x8F, 0x78000, 0x508000, 0x88);
        ModeFlow_AreaTransition = 0xFF;
        Area_EnterTransition = 0xFF;
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
        Camera_Angles[0] = 0xFC7A;
        W(::bof3::Img(scena_sc13::at::kAngleFB)) = 0x3B8;
        Camera_Distance = 0xF204;
        const unsigned char slot = Take(::bof3::Img(Scena13_Slot_addr));
        if (slot != 0xFF) FxCam0(slot, 0x31, -0x2EC, AngleY(), S16(::bof3::Img(scena_sc13::at::kAngleFB)), 0x3C);
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
            Camera_Angles[0] = 0xFD56;
            W(::bof3::Img(scena_sc13::at::kAngleFB)) = 0x200;
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
        Camera_Distance = static_cast<std::uint16_t>(static_cast<std::uint16_t>(Camera_Distance) + 0x50);
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
        Camera_Distance = static_cast<std::uint16_t>(static_cast<std::uint16_t>(Camera_Distance) - 0x50);
        if (TimerRuns()) return;
        SetCounter(0, 0x32);
        SetStep(0x19);
        return;
    case 0x19:
        if (Counter(0) != 0x34) return;
        Field_Kind2X = 0x240000;
        Field_Kind2Z = 0x1C8000;
        SH_CALL(Field_ViewReset)();
        SH_AT(PlaceFn, scena_sc13::at::kPartyPlace.v)(0x288000, 0x1D8000, 0x25);
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
        const unsigned char slot = Take(::bof3::Img(Scena13_Slot_addr));
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

// original 0x563F20: a caption for run 7 - the text `index` of the offsets at
// 0x803580 (text from 0x803582) drawn at (0x50, 0x3C) while the clock
// total - timer (total = 30 * seconds - 1, both 16-bit, to 0x903852 /
// 0x903850) is below the total; the CLUT strip faded in over its first 0x20
// frames (ClutStrip_FadeTo(clock)), restored at 0x20, and faded out over its
// last 0x20 (FadeTo(total - clock)); restored when the clock equals the
// total, nothing past it. The index also to the word 0x7DEE48.
SC13_EXPORT void __cdecl Scena13_Caption(unsigned index, unsigned seconds) {
    const std::uint16_t total = static_cast<std::uint16_t>((seconds & 0xFF) * 30 - 1);
    W(::bof3::Img(scena_sc13::at::kCaptionIndex)) = static_cast<std::uint16_t>(index);
    scena_sc13::at::kCaptionTotal = total;
    const std::uint16_t clock = static_cast<std::uint16_t>(total - Timer());
    scena_sc13::at::kSlotWord.w_00 = clock;
    if (clock >= total) {
        if (clock == total) SH_CALL(ClutStrip_Restore)();
        return;
    }
    const std::uint32_t text = ::bof3::Img(scena_sc13::at::kCaptionText) + 2 + W(::bof3::Img(scena_sc13::at::kCaptionText) + 2 * (index & 0xFFFF));
    SH_CALL(Text_DrawAt)(0x50, 0x3C, 0, 0xFF, reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(text)));
    const std::uint16_t now = scena_sc13::at::kSlotWord.w_00;
    if (now < 0x20) {
        SH_CALL(ClutStrip_FadeTo)(now & 0xFF);
        return;
    }
    if (now == 0x20) {
        SH_CALL(ClutStrip_Restore)();
        return;
    }
    if (scena_sc13::at::kSlotWord.w_00 <= scena_sc13::at::kCaptionTotal - 0x20) return;
    SH_CALL(ClutStrip_FadeTo)(static_cast<unsigned char>(SharedScratch.b_02 - scena_sc13::at::kSlotWord.b_00));
}

// original 0x563FE0: run 8, steps 0, 4..6, 8, 0xA, 0xB.
SC13_EXPORT void __cdecl Scena13_Run8(void) {
    switch (Step()) {
    case 0:
        // ObjTrio records 1 and 2 + 0x137 (0x802FC3, 0x80310F) both 0
        if (rv::Bytes(&ObjTrio[1])[0x137] != 0) return;
        if (rv::Bytes(&ObjTrio[2])[0x137] != 0) return;
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
    const std::uint32_t bits = Chapter_FlagRow;
    const unsigned index = object[0x86];
    reinterpret_cast<ObjectEntry>(static_cast<std::uintptr_t>(
        Entry(::bof3::Img(Scena13_Objects_addr), static_cast<int>(index), scena_sc13::at::kObjectCount13, "Scena13_ObjectTrigger")))(object, bits);
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
    AreaW0a_TailKind = 0x2C;
    B(::bof3::Img(scena_sc13::at::kPendingKind) + 1) = 0;
    B(::bof3::Img(scena_sc13::at::kPendingKind) + 2) = 0x11;
}

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
            facing = ObjTrio[0].direction;
            if (facing != 0 && facing != 7 && facing != 6) return Hit(4, 0);
            loaded = true;
        }
    }
    if (!loaded) facing = ObjTrio[0].direction;
    if (Area() == 0x7F) {
        if (FD() != 0) return 0;
        if (xw != 0x38000) return 0;
        if (!In(z16, 6, 3)) return 0;
        Set(6);
        facing = ObjTrio[0].direction;
    }
    if (Area() == 0x8F) {
        if (FD() == 0) {
            if (!Flag(0x14) && xw == 0x3B8000 && In(z16, 0x1F, 4)) return Hit(6, 0x14);
            if (Flag(0x1D) && !Flag(0x1E) && xw == 0x3B8000 && In(z16, 0x1F, 4)) return Hit(8, 5);
            facing = ObjTrio[0].direction;
        }
        if (FD() == 1) {
            if (!Flag(0x11) && xw == 0x428000 && In(z16, 0x64, 3)) return Hit(6, 0);
            facing = ObjTrio[0].direction;
        }
    }
    if (Area() == 0x90) {
        if (FD() != 2) return 0;
        if ((x16 & 0xFFFF) != 0x25) return 0;
        if (!In(z16, 0x1D, 2)) return 0;
        scena_sc13::at::kSlotWord.b_00 = facing;
        if (facing != 6 && facing != 7 && facing != 0) return 0;
        if (!Flag(0x1B)) {
            Set40();
            if (!Flag(0x18)) {
                if (ObjTrio[0].char_id == 2) DropIn(1);
            } else if (!Flag(0x19)) {
                const unsigned char who = ObjTrio[0].char_id;
                DropIn(who == 8 ? 4 : who == 5 ? 3 : 2);
            } else {
                const bool third = Flag(0x1A);
                const unsigned char who = ObjTrio[0].char_id;
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
        const unsigned char f = ObjTrio[0].direction;
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

void Scena132_Inject() {
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
}

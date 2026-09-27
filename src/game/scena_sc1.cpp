// Scenario chapter 1's bank (the PSX's SCENA01.EMI), compiled into the exe at
// 0x539AE0..0x53DD92. docs/scena_sc1.md.
//
//   - The state machine: Scena01_Frame (ours since round eight, mode_states)
//     jumps through Scena01_States: state 0 Scena01_Start, 1
//     Scena01_EnterArea (mode_states), 2 Scena01_Run, which jumps through
//     Scena01_Runs on the run MoveScript_Var7 to one of eighteen scenes (the
//     other six entries are 0x437CC0, a bare ret).
//   - A scene is a switch on the step byte 0x8034E5: each step waits on a
//     script counter (0x903848..B), the message box (Field_Request), the wait
//     word (MoveScript_WaitWordDA), a flag, a timer or a position, does one
//     thing - a message, a sound, the music, an effect, an area change, a
//     party member dropped in - and sets the next step; the last clears the
//     counters and sets the run and step back to 0.
//   - Slot 1, the object hook: through Scena01_ObjectHandlers on the object's
//     +0x86 to one of seventeen small handlers that start a scene.
//   - Slot 4, the cell hook: 0x56D800 over Scena01_Cells, then through
//     Scena01_CellHandlers to Scena01_Cell, which starts run 5.
//
// Every call goes through the scenario harness (SH_CALL / SH_AT), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies.
// No divergence: each is a faithful replacement, except that an index past a
// table - the run past Scena01_Runs, the object's +0x86 past
// Scena01_ObjectHandlers, 0x56D800's answer past Scena01_CellHandlers - aborts
// where the original would call through whatever lies there (the spell
// round's rule, docs/takeover-queue-round9.md section 6).
#include "game/scena_sc1.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/scena_sc1_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = scena_sc1::at;
namespace cl = scena_sc1::callee;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using scenario_harness::Handler;

// --- the chapter's bytes ------------------------------------------------------

unsigned Step() { return At(at::kStep)[0]; }
void SetStep(unsigned v) { At(at::kStep)[0] = static_cast<unsigned char>(v); }
void SetRun(unsigned v) { MoveScript_Var7 = static_cast<signed char>(v); }
unsigned char& Counter(unsigned i) { return At(at::kCounters + i)[0]; }
unsigned Timer() { return Word(At(at::kTimer)); }
void SetTimer(unsigned v) { SetWord(At(at::kTimer), v); }
void SetAreaByte(unsigned v) { At(at::kAreaByte)[0] = static_cast<unsigned char>(v); }
bool MessageUp() { return Field_Request == 2; }
void OpenMessage(unsigned id) {
    SH_CALL(Msg_OpenScript)(static_cast<unsigned short>(id));
    Field_Request = 2;
}
bool Waiting() { return MoveScript_WaitWordDA != 0; }
void ScriptOr(unsigned bits) { Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags | bits); }
void ScriptXor(unsigned bits) { Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags ^ bits); }
void Distance(int delta) { Camera_Distance = static_cast<short>(Camera_Distance + delta); }

// The leader's record (ObjTrio's first member) and the second's.
unsigned char* Leader() { return ObjTrio; }
unsigned char* Second() { return ObjTrio + 0x14C; }
short LeaderWord(unsigned off) { return static_cast<short>(Word(Leader() + off)); }
int LeaderLong(unsigned off) { return Long(Leader() + off); }

// The flag bits: the dword 0x929ED0 read afresh at each call.
unsigned char* Bank() { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(At(at::kFlagBank))))); }
bool Flag(unsigned n) { return SH_CALL(Flags_Test)(Bank(), n) != 0; }
void SetFlag(unsigned n) { SH_CALL(Flags_Set)(Bank(), n); }
void ClearFlag(unsigned n) { SH_CALL(Flags_Clear)(Bank(), n); }

// The calls every scene makes.
void ChangeArea(unsigned area, int x, int z, unsigned flags) { SH_CALL(Field_ChangeArea)(area, x, z, flags); }
void MusicPlay(unsigned track) { SH_CALL(Music_Play)(track, 8); }
void MusicStop(int frames) { SH_CALL(Music_FadeOutStop)(frames); }
void Sound(unsigned id) { SH_CALL(Sound_PlayEffect)(static_cast<unsigned short>(id)); }
void Transition(unsigned kind) { SH_CALL(Transition_Start)(static_cast<unsigned char>(kind)); }
void DropIn(unsigned entry) { SH_CALL(Party_DropIn)(entry); }
void Kind2(unsigned n) { SH_CALL(Kind2_Place)(static_cast<unsigned char>(n)); }
void Set40() { SH_CALL(ScriptFlags_Set40)(); }
void Clear40() { SH_CALL(ScriptFlags_Clear40)(); }
void LoadStream() { SH_CALL(Sound_LoadStream)(0); }
bool StreamDone() { return SH_CALL(Sound_StreamDone)() != 0; }
void PartyRestore() { SH_AT(cl::VoidFn, cl::kPartyRestore)(); }
void PartyPlace(int x, int z, unsigned facing) { SH_AT(cl::PartyPlaceFn, cl::kPartyPlace)(x, z, facing); }
bool AngleTest(int a, int b) { return SH_AT(cl::AngleTestFn, cl::kAngleTest)(a, b) != 0; }
void SetCell(unsigned x, unsigned z, unsigned value) { SH_CALL(AreaMap_SetByte)(x, z, value); }

// An effect slot taken into 0x903850; false (the slot 0xFF stored) when none
// is free.
bool TakeEffect() {
    const unsigned char slot = SH_CALL(Effect_FindFree)();
    At(at::kEffectSlot)[0] = slot;
    return slot != 0xFF;
}
// The record of the slot 0x903850 holds, read back as the original does.
unsigned char* Effect() { return Effect_Objects + (static_cast<std::uint32_t>(At(at::kEffectSlot)[0]) << 7); }
// A kind-0x13 effect at (x, the camera's second angle word) with +0x6C `far`
// and +9 `life`: the record written as the original writes it.
void AimEffect(int x, int far, unsigned life) {
    unsigned char* const e = Effect();
    e[0] = 1;
    e[5] = 0x13;
    SetLong(e + 0x64, x);
    SetLong(e + 0x68, Camera_Angles[1]);
    SetLong(e + 0x6C, far);
    e[9] = static_cast<unsigned char>(life);
}
// The same at the camera's first angle word.
void AimEffectAtCamera(int far, unsigned life) {
    const int x = Camera_Angles[0];
    const int z = Camera_Angles[1];
    unsigned char* const e = Effect();
    e[0] = 1;
    e[5] = 0x13;
    SetLong(e + 0x64, x);
    SetLong(e + 0x68, z);
    SetLong(e + 0x6C, far);
    e[9] = static_cast<unsigned char>(life);
}
// Two kind-0x15 / 0x55 effects, each only when a slot is free.
void TwoEffects() {
    if (TakeEffect()) {
        Effect()[0] = 1;
        Effect()[5] = 0x15;
    }
    if (TakeEffect()) {
        Effect()[0] = 1;
        Effect()[5] = 0x55;
    }
}

// The step every scene ends with: the counters from `first` on cleared (the
// original's order does not matter between calls), the step and run 0.
void Done(unsigned first) {
    for (unsigned i = first; i < 4; ++i) Counter(i) = 0;
    SetStep(0);
    SetRun(0);
}

// Game_Mode 7 with the object 0xFE: the shop (docs/mode_states.md section 1).
void OpenShop(unsigned step) {
    At(at::kShopObject)[0] = 0xFE;
    At(at::kShopByteA)[0] = 0;
    At(at::kShopByteB)[0] = 0;
    At(at::kShopByteC)[0] = 1;
    Game_Mode = 7;
    SetStep(step);
}

// The camera distance stepped by `delta` once a frame for `frames` frames,
// counted in counter 3: true on the last, with the counter cleared.
bool Zoom(int delta, unsigned frames, bool pass_flags = false) {
    const unsigned char n = static_cast<unsigned char>(Counter(3) + 1);
    Distance(delta);
    if (pass_flags) Draw_PassFlags = 0x1F;
    MapView_Redraw = 2;
    Counter(3) = n;
    return n >= frames;
}

Handler Entry(std::uint32_t table, unsigned index) {
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(At(table + 4u * index)))));
}

}  // namespace

// original 0x539AE0: chapter 1's state 0 (Scena01_States entry 0), the
// chapter's first frame: Scenario_CallA(0), the area change to area 9 at
// (0x570000, 0x40000) facing 7, the dword 0x903F98 and the four counters
// cleared, state 1.
extern "C" void __cdecl Scena01_Start(void) {
    SH_CALL(Scenario_CallA)(0);
    ChangeArea(9, 0x570000, 0x40000, 7);
    SetLong(At(at::kStartWord), 0);
    SetLong(At(at::kCounters), 0);
    At(at::kState)[0] = 1;
}

// original 0x53A2B0: chapter 1's state 2 (Scena01_States entry 2), a tail
// jump through Scena01_Runs on the s8 MoveScript_Var7. The table holds 24;
// the original reads any s8 index, ours aborts past the table.
extern "C" void __cdecl Scena01_Run(void) {
    const int run = MoveScript_Var7;
    if (run < 0 || run >= static_cast<int>(at::kRunCount))
        bof3::Fatal("Scena01_Run: run %d past Scena01_Runs (%u entries)", run, at::kRunCount);
    Entry(at::kRuns, static_cast<unsigned>(run))();
}

// original 0x53A2C0: run 1, three steps: on counter 0 = 1 the change to area
// 9 at (0x470000, 0x1D0000) (counters 1 and 2 = 1); the pass flags on and
// Kind2_Place(0); on counter 0 = 0x1E flag 0 set, the counters, step and run
// cleared, and the change to area 0xA at (0x2B0000, 0x200000).
extern "C" void __cdecl Scena01_Scene01(void) {
    switch (Step()) {
    case 0:
        if (Counter(0) != 1) return;
        Counter(1) = 1;
        Counter(2) = 1;
        SetStep(1);
        ChangeArea(9, 0x470000, 0x1D0000, 3);
        return;
    case 1:
        Draw_PassFlags = 0x1F;
        Kind2(0);
        SetStep(2);
        return;
    case 2:
        if (Counter(0) != 0x1E) return;
        SetFlag(0);
        Counter(0) = 0;
        Counter(1) = 0;
        Counter(2) = 0;
        SetStep(0);
        SetRun(0);
        ChangeArea(0xA, 0x2B0000, 0x200000, 3);
        return;
    default:
        return;
    }
}

// original 0x53A380: run 2, steps 0..0xB (a jump table of 12; 5..7 nothing):
// area 0xA's changes by counter 0, the music, transitions 0xF and 0x10 with
// sound 0x201, the last effect cleared, and on the leader's settling (counter
// 0 = 1, Field_Kind2Hold 0) flag 1 and the script flags 0x60.
extern "C" void __cdecl Scena01_Scene02(void) {
    switch (Step()) {
    case 0:
        if (Counter(0) != 2) return;
        Counter(1) = 1;
        SetStep(1);
        ChangeArea(0xA, 0x520000, 0x230000, 3);
        return;
    case 1:
        if (Counter(0) != 4) return;
        Counter(1) = 2;
        SetStep(2);
        MusicStop(0xA);
        SH_CALL(Music_Play)(0xE, 8);
        ScriptOr(0x80);
        ChangeArea(0xA, 0x4F0000, 0x50000, 3);
        return;
    case 2:
        if (Counter(0) != 0xA) return;
        Counter(1) = 3;
        Counter(2) = 7;
        SetStep(3);
        MusicStop(0xA);
        ChangeArea(0xA, 0x600000, 0x120000, 3);
        return;
    case 3:
        if (Counter(0) != 1) return;
        Transition(0xF);
        Sound(0x201);
        SetStep(4);
        return;
    case 4:
        if (Waiting()) return;
        Transition(0x10);
        SetStep(8);
        Effect()[0] = 0;
        return;
    case 8:
        if (Waiting()) return;
        Counter(0) = 2;
        SetStep(9);
        return;
    case 9:
        if (Counter(0) != 3) return;
        Counter(1) = 4;
        Counter(2) = 0;
        SetStep(0xA);
        ChangeArea(0xA, 0x5F0000, 0x310000, 3);
        return;
    case 0xA:
        if (Counter(0) != 1) return;
        ScriptXor(0x80);
        Counter(1) = 5;
        SetStep(0xB);
        ChangeArea(0xA, 0x4F0000, 0x50000, 0x85);
        return;
    case 0xB:
        if (Counter(0) != 1 || Field_Kind2Hold != 0) return;
        SetFlag(1);
        Clear40();
        ScriptOr(0x60);
        Done(0);
        return;
    default:   // 5..7, and 0xC and up
        return;
    }
}

// original 0x53A5E0: run 3, steps 0..0x16 (a jump table of 23; 5..9, 0xF and
// 0x10 nothing): the camera pulled out and back over 0x50 frames each, a
// pointing hand at (0xD2, 0x3C) until a button, Kind2 moved to x 0x210000 and
// 0x160000, four kind-0x13 effects at the camera, the music, and the last
// effect's end waited for; flags 2 and 3.
extern "C" void __cdecl Scena01_Scene03(void) {
    switch (Step()) {
    case 0:
        Kind2(0);
        DropIn(0);
        SetStep(1);
        return;
    case 1:
        if (!Zoom(0x20, 0x50)) return;
        Camera_Distance = 0xA00;
        MapView_Redraw = 2;
        Counter(3) = 0;
        SetStep(2);
        return;
    case 2:
        if (Counter(0) != 0x1F) return;
        if (!Zoom(-0x20, 0x50)) return;
        MapView_Redraw = 2;
        Camera_Distance = 0;
        Counter(3) = 0;
        SetStep(3);
        return;
    case 3:
        if (Counter(0) != 0x28) return;
        SH_CALL(Menu_DrawHand)(0xD2, 0x3C, 0);
        if (Input_Pressed == 0) return;
        Counter(0) = 0x29;
        SetStep(4);
        return;
    case 4:
        if (Counter(0) != 0x2A || Field_Kind2Hold != 0) return;
        SetFlag(2);
        Clear40();
        SetStep(0);
        SetRun(0);
        return;
    case 0xA:
        DropIn(1);
        Kind2(5);
        SetStep(0xB);
        return;
    case 0xB:
        if (Counter(0) != 1) return;
        Field_Kind2X = 0x210000;
        Field_Kind2Z = LeaderLong(0x38);
        MoveScript_F3Divisor = 0x20;
        SetStep(0xC);
        return;
    case 0xC:
        if (Field_Kind2Hold != 0) return;
        Counter(0) = 2;
        SetStep(0xD);
        return;
    case 0xD:
        if (Counter(0) != 3) return;
        Field_Kind2X = 0x160000;
        Field_Kind2Z = LeaderLong(0x38);
        MoveScript_F3Divisor = 0x20;
        if (!TakeEffect()) return;
        SetStep(0xE);
        AimEffect(-0x1CE, 0x26E, 0x14);
        return;
    case 0xE:
        if (Field_Kind2Hold != 0) return;
        Counter(0) = 4;
        SetStep(0x11);
        return;
    case 0x11:
        if (Counter(0) != 0xA) return;
        SH_CALL(Music_Play)(0x14, 8);
        SetStep(0x12);
        return;
    case 0x12:
        if (Counter(0) != 0xB) return;
        if (!TakeEffect()) return;
        SetStep(0x13);
        AimEffect(-0x2AA, 0x200, 0x14);
        return;
    case 0x13:
        if (Counter(0) != 0x10) return;
        if (!TakeEffect()) return;
        AimEffect(-0x35A, 0x200, 0x14);
        SetStep(0x14);
        return;
    case 0x14:
        if (Counter(0) != 0x18) return;
        MusicStop(0xA);
        SH_CALL(Music_Play)(0x13, 8);
        if (!TakeEffect()) return;
        Counter(3) = At(at::kEffectSlot)[0];
        SetStep(0x15);
        AimEffect(-0x2AA, 0x200, 0x14);
        return;
    case 0x15:
        if (Effect_Objects[static_cast<std::uint32_t>(Counter(3)) << 7] != 0) return;
        ScriptOr(8);
        Counter(3) = 0;
        Counter(0) = 0x19;
        SetStep(0x16);
        return;
    case 0x16:
        if (Counter(0) != 0x22) return;
        SetFlag(3);
        ScriptXor(8);
        Clear40();
        Done(1);
        return;
    default:
        return;
    }
}

// original 0x53AA90: run 5, steps 0, 1, 2 and 0xA..0xC (a byte table over
// 0..0xC into a jump table of 7): message 7, or with the leader's +0x89 = 4
// Party_DropIn(0) and message 8; sound 0x103 and the script flags 0x1E; the
// change to area 8 at (0x540000, 0x1D0000) with 0x904CD0 = 0xB; Kind2_Place(0);
// on counter 0 = 0x16 flag 4, the script flags ^ 8 | 0x80 and
// Inventory_Add(0, 0x22, 1).
extern "C" void __cdecl Scena01_Scene05(void) {
    switch (Step()) {
    case 0:
        if (Leader()[0x89] != 4) {
            OpenMessage(7);
            SetStep(1);
        } else {
            DropIn(0);
            OpenMessage(8);
            SetStep(2);
        }
        return;
    case 1:
        if (MessageUp()) return;
        Counter(0) = 0;
        Counter(1) = 0;
        Counter(2) = 0;
        Counter(3) = 0;
        Clear40();
        SetStep(0);
        SetRun(0);
        return;
    case 2:
        if (MessageUp()) return;
        Sound(0x103);
        ScriptOr(0x1E);
        SetStep(0xA);
        return;
    case 0xA:
        Set40();
        SetStep(0xB);
        ChangeArea(8, 0x540000, 0x1D0000, 0x81);
        SetAreaByte(0xB);
        return;
    case 0xB:
        Counter(0) = 2;
        Kind2(0);
        SetStep(0xC);
        return;
    case 0xC:
        if (Counter(0) != 0x16) return;
        SetFlag(4);
        Counter(1) = 1;
        SetStep(0);
        SetRun(0);
        Counter(2) = 2;
        Clear40();
        Field_ScriptFlags = static_cast<unsigned short>((Field_ScriptFlags ^ 8) | 0x80);
        SH_CALL(Inventory_Add)(0, 0x22, 1);
        return;
    default:
        return;
    }
}

// original 0x53AC30: run 6, steps 0..3, 0xA..0xC, 0xF..0x15, 0x19 and 0x1A (a
// byte table over 0..0x1A into a jump table of 17): started from the message
// box's choice (word 0x7DEE48 = 2, bit 1 of 0x7DEE44) and story flag 4; flag
// 6, a member dropped in, the music, Kind2, transition 4; at the wait's end
// Scenario_CallB(0), flag 5 and the change to area 8; then a loop in which
// the leader (Sprite_Current, Field_State = the leader's record) plays
// animation 0x44 on button 0x40 and 5 when it ends, until counter 3 or 2's
// bit 7; two effects and the second member's turn; flag 7.
extern "C" void __cdecl Scena01_Scene06(void) {
    switch (Step()) {
    case 0:
        if (Word(At(at::kChoiceWord)) != 2) return;
        if ((At(at::kChoiceBits)[0] & 2) != 2) return;
        if (SH_CALL(Flags_Test)(At(at::kStoryFlags), 4) == 0) return;
        Set40();
        Counter(0) = 5;
        SetStep(1);
        SetRun(6);
        return;
    case 1:
        SetFlag(6);
        SetStep(2);
        DropIn(2);
        Counter(3) = 0;
        return;
    case 2:
        if (Counter(0) != 0xC) return;
        MusicStop(0xA);
        SetStep(3);
        return;
    case 3:
        if (Counter(0) != 0x23) return;
        SetStep(0);
        SetRun(0);
        Counter(1) = 3;
        Counter(2) = 3;
        Counter(0) = 0;
        Clear40();
        return;
    case 0xA:
        if (Counter(0) != 2) return;
        SH_CALL(Music_Play)(0xA, 8);
        Draw_PassFlags = 0;
        Kind2(1);
        SetStep(0xB);
        return;
    case 0xB:
        if (Counter(0) != 3) return;
        Draw_PassFlags = 0x1F;
        SetStep(0xC);
        return;
    case 0xC:
        if (Counter(0) != 0x1A) return;   // ebx, the switch's bound, in the original
        SetStep(0xF);
        Transition(4);
        return;
    case 0xF: {
        if (Waiting()) return;
        const unsigned member = At(at::kScene06Member)[0];
        SetStep(0x10);
        Counter(2) = 4;
        Draw_PassFlags = 0;
        At(at::kMemberRecords + member * 0xA4)[0] &= 0xFE;
        SH_AT(cl::CallBFn, cl::kScenarioCallB)(0);
        SetFlag(5);
        ChangeArea(8, 0x320000, 0x120000, 0x84);
        return;
    }
    case 0x10:
        if (Field_Request == 2 || Counter(0) != 1) return;
        Counter(0) = 2;
        SetStep(0x11);
        return;
    case 0x11:
        if (Counter(0) != 5) return;
        SH_CALL(Music_Play)(9, 8);
        TwoEffects();
        SetStep(0x12);
        return;
    case 0x12:
        if ((Counter(3) & 0x80) == 0x80) {
            SetStep(0x14);
            Field_State = Leader();
            Sprite_Current = Leader();
            Leader()[0x124] &= 0xBF;
            return;
        }
        if (Counter(1) == 2) return;
        if (Input_Pressed != 0x40) return;
        Field_State = Leader();
        Sprite_Current = Leader();
        Leader()[0x124] |= 0x40;
        Sound(0x20F);
        SH_CALL(Sprite_SetAnimation)(0x44);
        Counter(1) = 1;
        SetStep(0x13);
        return;
    case 0x13: {
        const bool ended = LeaderWord(0x58) == 0xC;
        Field_State = Leader();
        Sprite_Current = Leader();
        if (!ended || Leader()[0x4A] != 1) return;
        SH_CALL(Sprite_SetAnimation)(5);
        SetStep(0x12);
        return;
    }
    case 0x14:
        if ((Counter(2) & 0x80) == 0x80) {
            Counter(0) = 6;
            SetStep(0x19);
        } else {
            Counter(0) = 7;
            SetStep(0x15);
        }
        return;
    case 0x15:
        if (Counter(0) != 0x1E) return;
        Counter(0) = 0x1F;
        Counter(1) = 0;
        Counter(2) = 0;
        Counter(3) = 0;
        SetTimer(0);
        SH_CALL(Music_Play)(9, 8);
        TwoEffects();
        Leader()[0x124] |= 0x40;
        Field_State = Second();
        Sprite_Current = Second();
        Second()[0x124] |= 0x40;
        DropIn(5);
        SetStep(0x12);
        return;
    case 0x19:
        DropIn(6);
        Kind2(2);
        SetStep(0x1A);   // ebx, the switch's bound, in the original
        return;
    case 0x1A:
        if (Counter(0) != 0xF || Field_Kind2Hold != 0) return;
        ScriptXor(0x10);
        SetFlag(7);
        Clear40();
        Counter(1) = 0;
        Counter(2) = 0;
        Counter(3) = 0;
        SetStep(0);
        SetTimer(0);
        SetRun(0);
        return;
    default:
        return;
    }
}

namespace {

// Scena01_Scene08 step 0's two places (the leader's cell x word 0x1B or 0x1C,
// its z word up to 0x23): the scene started, the member 6 dropped in. False:
// the leader's z word is past 0x23 and the original returns from the scene.
bool Scene08Start(short x_word) {
    if (LeaderWord(0x36) != x_word) return true;
    if (LeaderWord(0x3A) > 0x23) return false;
    Set40();
    SetTimer(0);
    SetStep(1);
    Counter(0) = 1;
    Counter(3) = 0;
    At(at::kScene08A)[0] = 0x7F;
    At(at::kScene08B)[0] = 0;
    DropIn(6);
    Kind2(0);
    Cond_ByteFE = 1;
    return true;
}

// The timer counted up to `limit` (u16): true when it gets there.
bool Count(unsigned limit) {
    const unsigned t = (Timer() + 1) & 0xFFFF;
    SetTimer(t);
    return t >= limit;
}

}  // namespace

// original 0x53B130: run 8, steps 0..6, 0xE..0x13 and 0x37..0x3C (a byte
// table over 0..0x3C into a jump table of 20): the leader on the cells x word
// 0x1B / 0x1C, z word up to 0x23 starts it; 0x57C550's tests and timed
// waits of 0x6E and 0xB4 frames with sounds 0x200 / 0x207 and Cond_ByteFE; flag
// 9 and the changes to area 0x17; the music out and the stream loaded, then
// 0x78 frames; sound 0x204 and message 0x13; transition 0xE; the party
// restored, music 0x1D; Kind2, the party placed at (0x1E0000, 0x1A0000),
// 0x4410B0(2); flag 0xA.
extern "C" void __cdecl Scena01_Scene08(void) {
    switch (Step()) {
    case 0:
        if (!Scene08Start(0x1B)) return;
        Scene08Start(0x1C);
        return;
    case 1:
        if (Counter(0) != 3) return;
        if (AngleTest(-0x5A, -2)) return;
        SetStep(2);
        return;
    case 2:
        if (!Count(0x6E)) return;
        Counter(0) = 5;
        SetTimer(0);
        SetStep(3);
        Sound(0x200);
        Cond_ByteFE = 2;
        return;
    case 3:
        if (!Count(0xB4)) return;
        Sound(0x207);
        SetTimer(0);
        Counter(0) = 8;
        SetStep(4);
        return;
    case 4:
        if (AngleTest(-0x3C, 2)) return;
        SetStep(5);
        return;
    case 5:
        if (!Count(0x6E)) return;
        SetTimer(0);
        SetStep(6);
        Counter(0) = 0xF;
        return;
    case 6:
        if (Counter(0) != 0x12) return;
        Field_StatusBits = static_cast<unsigned char>(Field_StatusBits | 1);
        Counter(2) = 3;
        SetStep(0xE);
        SetFlag(9);
        ChangeArea(0x17, 0x480000, 0x60000, 0x87);
        return;
    case 0xE:
        if (Counter(0) != 0x14) return;
        Counter(2) = 4;
        SetStep(0xF);
        ChangeArea(0x17, 0x4B0000, 0x50000, 0x88);
        return;
    case 0xF:
        SetTimer(0x78);
        MusicStop(0xA);
        LoadStream();
        SetStep(0x10);
        [[fallthrough]];
    case 0x10: {
        const unsigned t = Timer();
        SetTimer((t - 1) & 0xFFFF);
        if (t != 0) return;
        Sound(0x204);
        OpenMessage(0x13);
        SetStep(0x11);
        return;
    }
    case 0x11:
        if (MessageUp()) return;
        Transition(0xE);
        Draw_PassFlags = 0x1F;
        SetStep(0x12);
        return;
    case 0x12:
        if (Waiting()) return;
        Counter(0) = 6;
        SetStep(0x13);
        PartyRestore();
        SH_CALL(Music_Play)(0x1D, 8);
        ScriptOr(0x80);
        return;
    case 0x13:
        if (Counter(0) != 0xB) return;
        Clear40();
        Counter(2) = 7;
        Counter(3) = 0;
        SetStep(0);
        SetRun(8);
        return;
    case 0x37:
        if (Counter(0) != 0x1F) return;
        Kind2(1);
        SetStep(0x38);
        return;
    case 0x38:
        if (Counter(0) != 0x20) return;
        PartyPlace(0x1E0000, 0x1A0000, 2);
        Kind2(2);
        SetStep(0x39);
        return;
    case 0x39:
        if (Counter(0) != 0x23) return;
        SetStep(0x3A);
        SH_AT(cl::SeHelperFn, cl::kSeHelper)(2);
        return;
    case 0x3A:
        if (Counter(0) != 0x24) return;
        ScriptXor(0x80);
        SetStep(0x3B);
        return;
    case 0x3B:
        SetStep(0x3C);
        Counter(0) = 0x28;
        return;
    case 0x3C:
        if (Counter(0) != 0x28) return;
        Counter(1) = 0;
        Counter(2) = 0;
        SetStep(0);
        SetRun(0);
        SetTimer(0);
        Clear40();
        SetFlag(0xA);
        return;
    default:
        return;
    }
}

namespace {

// Scena01_Scene09's four cells set to `a` (row 0x1F) and `b` (row 0x20) at x
// 0x51 and 0x52.
void Scene09Gate(unsigned a, unsigned b) {
    SetCell(0x51, 0x1F, a);
    SetCell(0x52, 0x1F, a);
    SetCell(0x51, 0x20, b);
    SetCell(0x52, 0x20, b);
}
// And the four at (4, 7), (4, 8) = 0xC0, (3, 7), (3, 8) = 0xA1.
void Scene09Door() {
    SetCell(4, 7, 0xC0);
    SetCell(4, 8, 0xC0);
    SetCell(3, 7, 0xA1);
    SetCell(3, 8, 0xA1);
    Clear40();
}
// The leader at x 0x480000 or 0x488000, z 0x3E0000..0x3F8000 (signed): run 0xA
// and the change to area 0x16 at (0x540000, 0xA0000).
void Scene09Exit() {
    const int x = LeaderLong(0x38);
    if (x != 0x480000 && x != 0x488000) return;
    const int z = LeaderLong(0x34);
    if (z < 0x3E0000 || z > 0x3F8000) return;
    SetStep(0);
    SetRun(0xA);
    Set40();
    ChangeArea(0x16, 0x540000, 0xA0000, 0x8C);
}

}  // namespace

// original 0x53B5F0: run 9, steps 0..0x2D (a jump table of 46; 6, 7, 0xA,
// 0xF, 0x10, 0x1C, 0x1D and 0x2A..0x2C nothing): the script flags 0x100 and
// messages 1 and 2; the leader at cell x words 0x5B..0x5E, z word 0xD; four
// map cells closed and opened (AreaMap_SetByte); members dropped in on
// counter 0; flags 0xC, 0xF, 0x10, 0x12, 0x1D and 0x3F; message 0xC; the
// changes to area 0x16; sound 0x1206 (Sound_PlayById) at cells 0x3D..0x3F;
// the music, the party placed at (0x3F0000, 0x320000), 0x4410B0(3); and the
// leader at the exit (steps 0x1A and 0x2D) for run 0xA.
extern "C" void __cdecl Scena01_Scene09(void) {
    switch (Step()) {
    case 0:
        ScriptOr(0x100);
        SetStep(1);
        return;
    case 1:
        SH_CALL(Msg_OpenScript)(1);
        Field_Request = 2;
        SetStep(2);
        return;
    case 2:
        if (MessageUp()) return;
        ScriptXor(0x100);
        SetStep(0);
        SetRun(0);
        Counter(0) = 0;
        return;
    case 3: {
        if (LeaderWord(0x3A) != 0xD) return;
        const short x = LeaderWord(0x36);
        if (x != 0x5B && x != 0x5C && x != 0x5D && x != 0x5E) return;
        ScriptOr(0x100);
        SetStep(4);
        return;
    }
    case 4:
        SH_CALL(Msg_OpenScript)(2);
        Field_Request = 2;
        SetStep(5);
        return;
    case 5:
        if (MessageUp()) return;
        Scene09Gate(0, 0);
        ScriptXor(0x100);
        SetStep(6);
        SetFlag(0xC);
        return;
    case 8:
        if (Counter(0) != 7) return;
        Scene09Gate(0xC0, 0xA1);
        Clear40();
        SetStep(9);
        return;
    case 9:
        if (LeaderWord(0x36) != 0x41 || LeaderWord(0x3A) != 9) return;
        SetStep(0xB);
        return;
    case 0xB:
        if (Counter(0) == 0x1E) {
            DropIn(2);
            SetStep(0xC);
            SetFlag(0xF);
        }
        if (Counter(0) != 0x32) return;
        DropIn(4);
        SetStep(0xE);
        SetFlag(0xF);
        return;
    case 0xC:
        if (Counter(0) != 0x21) return;
        DropIn(3);
        Kind2(1);
        SetStep(0xD);
        return;
    case 0xD:
        if (Field_Kind2Hold != 0 || Counter(0) != 0x22) return;
        SetStep(0xF);
        Scene09Door();
        return;
    case 0xE:
        if (Counter(0) != 0x35) return;
        SetStep(0x11);
        Scene09Door();
        return;
    case 0x11:
        if (!Flag(0x12)) return;
        SetStep(0);
        SetRun(0);
        return;
    case 0x12:
        OpenMessage(0xC);
        SetStep(0x13);
        return;
    case 0x13:
        if (MessageUp()) return;
        SetStep(0x14);
        return;
    case 0x14:
        if (Counter(0) == 0x3C) {
            SetFlag(0x10);
            DropIn(5);
            SetStep(0x15);
        }
        if (Counter(0) != 0x46) return;
        Clear40();
        SetStep(0x11);
        return;
    case 0x15:
        SetStep(0x16);
        return;
    case 0x16:
        SetStep(0x17);
        return;
    case 0x17:
        if (Counter(0) != 0x45) return;
        SetStep(0x18);
        ChangeArea(0x16, 0x540000, 0x70000, 0x86);
        return;
    case 0x18: {
        if (Counter(0) != 4) return;
        const bool seen = Flag(0x1D);
        SetStep(0x19);
        if (!seen) {
            SetFlag(0x1D);
            ChangeArea(0x16, 0x3D0000, 0x470000, 0x87);
        } else {
            ChangeArea(0x16, 0x3D0000, 0x470000, 0x8E);
        }
        return;
    }
    case 0x19:
        if (Counter(0) != 9) return;
        Clear40();
        ScriptXor(8);
        SetStep(0x1A);
        return;
    case 0x1A:
        if (LeaderWord(0x3A) <= 0x2A) {
            for (short x = 0x3D; x <= 0x3F; ++x) {
                if (LeaderWord(0x36) != x) continue;
                SetStep(0x1E);
                SH_CALL(Sound_PlayById)(0x1206);   // pushed with 0x50, 0x50, which it does not read
                Set40();
            }
        }
        Scene09Exit();
        return;
    case 0x1B:
        if (Cond_ByteFD != 4) return;
        SetStep(0x1A);
        return;
    case 0x1E:
        Counter(0) = 0x64;
        Sound(0x202);
        Counter(0) = 0x65;
        DropIn(9);
        Kind2(0);
        SetStep(0x1F);
        return;
    case 0x1F:
        if (Counter(0) != 0x66) return;
        MusicStop(0xA);
        SH_CALL(Music_Play)(0x19, 8);
        SetStep(0x20);
        return;
    case 0x20:
        SetStep(0x21);
        return;
    case 0x21:
        SetStep(0x22);
        return;
    case 0x22:
        SetStep(0x23);
        return;
    case 0x23:
        if (Counter(0) != 0x68) return;
        PartyPlace(0x3F0000, 0x320000, 3);
        SetStep(0x24);
        return;
    case 0x24:
        if (Counter(0) != 0x6C) return;
        SH_AT(cl::SeHelperFn, cl::kSeHelper)(3);
        SetStep(0x25);
        return;
    case 0x25:
        if (Counter(0) != 0x6D) return;
        SetFlag(0x12);
        ClearFlag(0x3F);
        DropIn(0xA);
        SetStep(0x26);
        return;
    case 0x26:
        Sound(0x203);
        Counter(0) = 0x6E;
        SetStep(0x27);
        return;
    case 0x27:
        if (Counter(0) != 0x78) return;
        ScriptOr(0x20);
        Clear40();
        SetStep(0x2D);   // ebx, the switch's bound, in the original
        Counter(1) = 0;
        SetRun(9);
        return;
    case 0x28:
        SetStep(0x29);
        ChangeArea(0x16, 0x2A0000, 0x490000, 0x8D);
        return;
    case 0x29:
        Clear40();
        SetStep(0x2D);   // ebx
        SetRun(9);
        return;
    case 0x2D:
        Scene09Exit();
        return;
    default:
        return;
    }
}

// original 0x53BD80: run 0xA, steps 0..4, 0xB..0xD, 0x14, 0x15, 0x19, 0x1E and
// 0x1F (a byte table over 0..0x1F into a jump table of 14): message 0x18;
// flag 0x10 cleared, member 8 dropped in; sounds 0x205 / 0x206 and the change
// to area 0x17 by flags 0x12 and 0x14; Kind2 moves; the change to area 5 at
// (0x300000, 0x10000) with 0x904CD0 = 0xFF.
extern "C" void __cdecl Scena01_Scene0A(void) {
    switch (Step()) {
    case 0:
        Counter(0) = 0xA;
        OpenMessage(0x18);
        SetStep(1);
        return;
    case 1:
        if (MessageUp()) return;
        SetStep(2);
        return;
    case 2:
        if (Counter(0) == 0x14) {
            Clear40();
            SetStep(0x1B);
            Counter(1) = 0;
            Counter(2) = 0;
            SetRun(9);
            return;
        }
        SetStep(3);
        ClearFlag(0x10);
        Counter(0) = 0xB;
        return;
    case 3:
        DropIn(8);
        SetStep(4);
        return;
    case 4:
        SetStep(0xB);
        return;
    case 0xB: {
        if (Counter(0) != 0x14) return;
        SetStep(0xC);
        Sound(0x205);
        Sound(0x206);
        Counter(2) = 5;
        unsigned flags = 0x84;
        if (!Flag(0x12)) {
            SetStep(0xC);
            Counter(2) = 6;
        } else {
            const bool second = Flag(0x14);
            Counter(2) = 6;
            if (!second) {
                SetStep(0x1E);
            } else {
                SetStep(0x14);
                flags = 0x85;
            }
        }
        ChangeArea(0x17, 0xF0000, 0x280000, flags);
        return;
    }
    case 0xC:
        Kind2(3);
        SetStep(0xD);
        return;
    case 0xD:
        if (Counter(0) != 0xA) return;
        Clear40();
        Counter(1) = 0;
        Counter(2) = 0;
        SetStep(0);
        SetRun(0);
        return;
    case 0x14:
        Kind2(6);
        SetStep(0x15);
        return;
    case 0x15:
        if (Counter(0) != 8) return;
        Kind2(4);
        SetStep(0x19);
        return;
    case 0x19:
        if (Counter(0) != 0xD) return;
        ScriptXor(0x80);
        Counter(1) = 0;
        Counter(2) = 0;
        SetStep(0);
        SetRun(0);
        ChangeArea(5, 0x300000, 0x10000, 0);
        SetAreaByte(0xFF);
        return;
    case 0x1E:
        Kind2(3);
        SetStep(0x1F);
        return;
    case 0x1F:
        if (Counter(0) != 0xA) return;
        Counter(1) = 0;
        Counter(2) = 0;
        SetStep(0);
        SetRun(0);
        ChangeArea(5, 0x300000, 0x10000, 0);
        SetAreaByte(0xFF);
        return;
    default:
        return;
    }
}

// original 0x53C020: run 0xB, steps 0..4 and 0xA..0x13 (a jump table of 20;
// 5..9, 0xC..0xE nothing): flag 0x19 and the change to area 7; two kind-0x13
// effects at the camera; flag 0x1A; message 0x20; member 1 dropped in,
// transition 0xD, flag 0x1B and the change to area 0xE; the camera pulled in
// over 0x2E frames; flag 0x1C with the script flags ^ 8 and ^ 0x60.
extern "C" void __cdecl Scena01_Scene0B(void) {
    switch (Step()) {
    case 0:
        SetStep(1);
        SetFlag(0x19);
        ChangeArea(7, 0x20000, 0x140000, 0x80);
        return;
    case 1:
        Draw_PassFlags = 0x1F;
        SetStep(2);
        return;
    case 2:
        if (Counter(0) != 1) return;
        if (!TakeEffect()) return;
        SetStep(3);
        AimEffectAtCamera(0x31E, 0x14);
        return;
    case 3:
        if (Counter(0) != 3) return;
        if (!TakeEffect()) return;
        SetStep(4);
        AimEffectAtCamera(0x200, 0x14);
        return;
    case 4:
        if (Counter(0) != 0x12) return;
        SetFlag(0x1A);
        Clear40();
        Done(1);
        return;
    case 0xA:
        OpenMessage(0x20);
        SetStep(0xB);
        return;
    case 0xB:
        if (MessageUp()) return;
        Clear40();
        SetStep(0);
        SetRun(0);
        return;
    case 0xF:
        if (MessageUp()) return;
        DropIn(1);
        SetStep(0x10);
        return;
    case 0x10:
        if (Counter(0) != 0x19) return;
        Transition(0xD);
        SetStep(0x11);
        return;
    case 0x11:
        Counter(2) = 0;
        SetStep(0x12);
        SetFlag(0x1B);
        ChangeArea(0xE, 0xF0000, 0x1F0000, 0x80);
        return;
    case 0x12:
        if (!Zoom(-0x40, 0x2E)) return;
        ScriptOr(8);
        Camera_Distance = 0;
        Counter(3) = 0;
        MapView_Redraw = 2;
        Counter(0) = 2;
        SetStep(0x13);
        return;
    case 0x13:
        if (Counter(0) != 0x19 || Field_Kind2Hold != 0) return;
        ScriptXor(8);
        SetFlag(0x1C);
        ScriptXor(0x60);
        Clear40();
        Done(1);
        return;
    default:
        return;
    }
}

// original 0x53C330: run 0xC, steps 0..0xA (a jump table of 11): the change to
// area 5 with 0x904CD0 = 0xFF; Kind2_Place(0), transition 0xE, music 8, a
// kind-0x13 effect at the camera; 0x57C550's test; transition 0xD, message 4;
// flags 0x15 and 0x16 with the changes to area 5; the party restored.
extern "C" void __cdecl Scena01_Scene0C(void) {
    switch (Step()) {
    case 0:
        ScriptXor(0x20);
        Counter(2) = 1;
        SetStep(1);
        ChangeArea(5, 0x300000, 0x10000, 0);
        SetAreaByte(0xFF);
        return;
    case 1:
        Counter(0) = 1;
        Kind2(0);
        SetStep(2);
        return;
    case 2:
        if (Counter(0) != 2) return;
        Transition(0xE);
        Draw_PassFlags = 0x1F;
        SetStep(3);
        return;
    case 3:
        if (Waiting()) return;
        SH_CALL(Music_Play)(8, 8);
        SetStep(4);
        return;
    case 4:
        if (Counter(0) != 5) return;
        if (!TakeEffect()) return;
        SetStep(5);
        AimEffectAtCamera(0xE3, 0x40);
        return;
    case 5:
        if (Counter(0) != 6) return;
        if (AngleTest(-0x50, -2)) return;
        SetStep(6);
        return;
    case 6:
        if (Counter(0) != 8) return;
        Transition(0xD);
        SetStep(7);
        return;
    case 7:
        if (Waiting()) return;
        Draw_PassFlags = 0;
        Counter(0) = 9;
        OpenMessage(4);
        SetStep(8);
        return;
    case 8:
        if (MessageUp()) return;
        Counter(2) = 2;
        SetStep(9);
        SetFlag(0x15);
        ChangeArea(5, 0x540000, 0x250000, 0x81);
        return;
    case 9:
        if (Counter(0) != 0x14) return;
        Counter(2) = 3;
        SetStep(0xA);
        SetFlag(0x16);
        ChangeArea(5, 0x100000, 0xD0000, 0x82);
        return;
    case 0xA:
        Clear40();
        PartyRestore();
        Done(0);
        return;
    default:
        return;
    }
}

namespace {

// Scena01_Scene0D's ends: the counters 1..3 (and 0 when `all`) cleared, the
// script flag 0x40 off, the step and run 0, flag `n`.
void Scene0DEnd(bool all, unsigned n) {
    if (all) Counter(0) = 0;
    Counter(1) = 0;
    Counter(2) = 0;
    Counter(3) = 0;
    Clear40();
    SetStep(0);
    SetRun(0);
    SetFlag(n);
}
// Steps 0xF and 0x12: on counter 3 = 0x14 or 0x19 and bit 1 of the choice
// bits, the end with flag 0x3C or 0x3D. Counter 3 is read once for the first
// test and afresh for the second; the first with the bit clear returns.
void Scene0DChoice(bool all) {
    if (Counter(3) == 0x14) {
        if ((At(at::kChoiceBits)[0] & 2) != 2) return;
        Scene0DEnd(all, 0x3C);
    }
    if (Counter(3) != 0x19) return;
    if ((At(at::kChoiceBits)[0] & 2) != 2) return;
    Scene0DEnd(all, 0x3D);
}

}  // namespace

// original 0x53C5A0: run 0xD, steps 0..6 and 0xA..0x12 (a jump table of 19):
// the change to area 0 at (0x150000, 0x130000) with 0x904CD0 = 1, the camera
// pulled in over 0x14 frames, Kind2, the music; flags 0x3A, 0x3B; two effects
// by Scena01_PlaceEffect (0x92, 0x93); the ends on counter 3 = 0x14 / 0x19
// (flags 0x3C / 0x3D), directly or after message 0x36 and the choice's bit.
extern "C" void __cdecl Scena01_Scene0D(void) {
    switch (Step()) {
    case 0:
        Counter(2) = 1;
        Set40();
        SetStep(1);
        ChangeArea(0, 0x150000, 0x130000, 0x80);
        SetAreaByte(1);
        return;
    case 1:
        if (!Zoom(-0x80, 0x14, true) || Counter(3) != 0x14) return;
        MapView_Redraw = 2;
        Camera_Distance = 0;
        Counter(3) = 0;
        Counter(1) = 0;
        SetStep(2);
        return;
    case 2:
        if (Counter(0) != 6) return;
        Kind2(0);
        SetStep(3);
        return;
    case 3:
        if (Counter(0) != 0x13) return;
        MusicStop(0xA);
        SetStep(4);
        return;
    case 4:
        if (Counter(0) != 0x1E) return;
        SH_CALL(Music_Play)(0, 8);
        SetStep(5);
        return;
    case 5:
        if (Counter(0) != 0x28) return;
        Set40();
        Counter(0) = 0;
        Counter(2) = 2;
        SetStep(6);
        SetFlag(0x3A);
        ChangeArea(0, 0x150000, 0x130000, 0x81);
        return;
    case 6:
        if (Counter(0) != 4) return;
        Counter(1) = 0;
        Counter(2) = 0;
        Counter(3) = 0;
        Clear40();
        SetStep(0);
        SetRun(0);
        SetFlag(0x3B);
        return;
    case 0xA:
        if (Counter(0) != 0xB) return;
        SH_CALL(Scena01_PlaceEffect)(0x92);
        SetStep(0xB);
        Counter(0) = 0xC;
        DropIn(2);
        return;
    case 0xB:
        if (Counter(0) != 0xD) return;
        SH_CALL(Scena01_PlaceEffect)(0x93);
        SetStep(0xC);
        Counter(0) = 0xE;
        return;
    case 0xC: {
        const unsigned c = Counter(3);
        if (c == 0x14) {
            Scene0DEnd(false, 0x3C);
            return;
        }
        if (c != 0x19) return;
        Scene0DEnd(false, 0x3D);
        return;
    }
    case 0xD:
        if (MessageUp()) return;
        OpenMessage(0x36);
        SetStep(0xE);
        return;
    case 0xE:
        if (MessageUp()) return;
        SetStep(0xF);
        return;
    case 0xF:
        Scene0DChoice(false);
        return;
    case 0x10:
        if (MessageUp()) return;
        OpenMessage(0x36);
        SetStep(0x11);
        return;
    case 0x11:
        if (MessageUp()) return;
        SetStep(0x12);
        return;
    case 0x12:
        Scene0DChoice(true);
        return;
    default:
        return;
    }
}

// original 0x53CA20: run 0xE, steps 0 and 3..9 (a jump table of 10; 1, 2 and
// 6 nothing): the music 0x11, message 0x10; flag 0x3E and the change to area
// 0xD; transition 0xD; Field_StatusBits bit 0, Field_ViewReset, message
// 0x1C; transition 0xE; the change to area 0xF at (0x360000, 0x500000), then
// Field_StatusBits bit 7 (the tail jump to 0x56D6F0).
extern "C" void __cdecl Scena01_Scene0E(void) {
    switch (Step()) {
    case 0:
        SetStep(3);
        MusicStop(0xA);
        SH_CALL(Music_Play)(0x11, 8);
        ScriptOr(0x80);
        Draw_PassFlags = 0;
        SH_CALL(Msg_OpenScript)(0x10);
        Field_Request = 2;
        return;
    case 3:
        if (MessageUp()) return;
        Counter(2) = 1;
        SetStep(4);
        SetFlag(0x3E);
        ChangeArea(0xD, 0x80000, 0x530000, 0x81);
        return;
    case 4:
        if (Counter(0) != 7) return;
        Transition(0xD);
        SetStep(5);
        return;
    case 5: {
        if (Waiting()) return;
        const unsigned char bits = Field_StatusBits;
        Draw_PassFlags = 0;
        SetStep(7);
        Field_StatusBits = static_cast<unsigned char>(bits | 1);
        SH_CALL(Field_ViewReset)();
        Counter(0) = 8;
        SH_CALL(Msg_OpenScript)(0x1C);
        Field_Request = 2;
        return;
    }
    case 7:
        if (MessageUp()) return;
        Transition(0xE);
        Draw_PassFlags = 0x1F;
        SetStep(8);
        return;
    case 8:
        if (Waiting()) return;
        Counter(0) = 9;   // ecx, the switch's bound, in the original
        SetStep(9);
        return;
    case 9:
        if (Counter(0) != 0x14) return;
        SetStep(0);
        SetRun(0);
        Draw_PassFlags = 0;
        ChangeArea(0xF, 0x360000, 0x500000, 0);
        SH_AT(cl::VoidFn, cl::kStatusBit80)();
        return;
    default:   // 1, 2, 6, and 0xA and up
        return;
    }
}

// original 0x53CBB0: run 0xF, steps 0..5 (a jump table of 6): on counter 0 =
// 0x64 transition 0xD, the party restored and the music out over 0x1E; on
// 0x65 the end; the stream loaded and waited for; transition 0xE, music 0x1B
// and message 0xA.
extern "C" void __cdecl Scena01_Scene0F(void) {
    switch (Step()) {
    case 0: {
        if (MessageUp()) return;
        const unsigned c = Counter(0);
        if (c == 0x64) {
            Counter(0) = 0;
            SetStep(1);
            Transition(0xD);
            PartyRestore();
            MusicStop(0x1E);
            return;
        }
        if (c != 0x65) return;
        Clear40();
        Counter(0) = 0;
        Counter(1) = 0;
        Counter(2) = 0;
        SetStep(0);
        SetRun(0);
        return;
    }
    case 1:
        LoadStream();
        SetStep(2);
        return;
    case 2:
        if (Waiting()) return;
        Draw_PassFlags = 0;
        if (!StreamDone()) return;
        SetStep(3);
        return;
    case 3:
        Transition(0xE);
        Draw_PassFlags = 0x1F;
        SetStep(4);
        return;
    case 4:
        if (Waiting()) return;
        SH_CALL(Music_Play)(0x1B, 8);
        OpenMessage(0xA);
        SetStep(5);
        return;
    case 5:
        if (MessageUp()) return;
        Clear40();
        SetStep(0);
        SetRun(0);
        return;
    default:
        return;
    }
}

namespace {

// Scena01_Scene11 / 17's shared tails.
void EndAfter65() {   // counter 0 = 0x65 (read afresh): the end, counters 0..2
    if (Counter(0) != 0x65) return;
    Counter(0) = 0;
    Counter(1) = 0;
    Counter(2) = 0;
    Clear40();
    SetStep(0);
    SetRun(0);
}
void EndAll() {   // counters 0..3, then the end
    Counter(0) = 0;
    Counter(1) = 0;
    Counter(2) = 0;
    Counter(3) = 0;
    Clear40();
    SetStep(0);
    SetRun(0);
}
// The party restored and the music out, then step `next`.
void RestoreAndFade(unsigned next) {
    if (Waiting()) return;
    Draw_PassFlags = 0;
    PartyRestore();
    SetStep(next);
    MusicStop(0x1E);
}
void Stream(unsigned next) {
    LoadStream();
    SetStep(next);
}
void StreamWait(unsigned next) {
    if (!StreamDone()) return;
    SetStep(next);
}
// Steps 2 of both: after the message, counter 0 = 0xC8 the shop, 0xC9 the end.
bool ShopOrEnd() {
    if (MessageUp()) return false;
    const unsigned c = Counter(0);
    if (c == 0xC8) {
        OpenShop(3);
        return false;
    }
    return c == 0xC9;
}

}  // namespace

// original 0x53CCE0: run 0x11, steps 0, 2, 3, 5..7 and 0x14..0x18 (a byte
// table over 0..0x18 into a jump table of 12): on counter 0 = 0x64
// transition 0xD, 0x65 the end; counter 0 = 0xC8 opens the shop (Game_Mode
// 7, object 0xFE), 0xC9 ends; the party restored and the stream played; with
// three members the change to area 5 at (0x510000, 0x80000) and music 7.
extern "C" void __cdecl Scena01_Scene11(void) {
    switch (Step()) {
    case 0:
        if (Counter(0) == 0x64) {
            Counter(0) = 0;
            Counter(2) = 5;
            Transition(0xD);
            SetStep(0x14);
        }
        EndAfter65();
        return;
    case 2:
        if (ShopOrEnd()) {
            Counter(0) = 0;
            Counter(1) = 0;
            Counter(2) = 0;
            Clear40();
            SetStep(0);
            SetRun(0);
        }
        return;
    case 3:
        if (Field_Request != 0) return;
        EndAll();
        return;
    case 5:
        if (Counter(0) == 0x64) {
            SetStep(6);
            Counter(0) = 0;
            Counter(2) = 5;
            PartyRestore();
            LoadStream();
            ChangeArea(5, 0x510000, 0x80000, Field_MemberCount == 3 ? 0x83 : 0x84);
        }
        EndAfter65();
        return;
    case 6:
        if (Counter(0) != 1) return;
        OpenShop(7);
        return;
    case 7:
        if (Field_Request != 0) return;
        EndAll();
        return;
    case 0x14:
        RestoreAndFade(0x15);
        return;
    case 0x15:
        Stream(0x16);
        return;
    case 0x16:
        StreamWait(0x17);
        return;
    case 0x17:
        if (Field_MemberCount != 3) return;
        SetStep(0x18);
        ChangeArea(5, 0x510000, 0x80000, 0x85);
        SH_CALL(Music_Play)(7, 8);
        return;
    case 0x18:
        if (Counter(0) != 1) return;
        Counter(1) = 0;
        Counter(2) = 0;
        Clear40();
        SetStep(0);
        SetRun(0);
        return;
    default:
        return;
    }
}

// original 0x53CFA0: run 0x12: the music 0x1A, the script flags 0x80, member
// 0xB dropped in; on counter 0 = 2 run 9 at step 0x2D.
extern "C" void __cdecl Scena01_Scene12(void) {
    switch (Step()) {
    case 0:
        MusicStop(0xA);
        SH_CALL(Music_Play)(0x1A, 8);
        ScriptOr(0x80);
        DropIn(0xB);
        SetStep(1);
        return;
    case 1:
        if (Counter(0) != 2) return;
        Clear40();
        Counter(1) = 0;
        Counter(2) = 0;
        SetStep(0x2D);
        SetRun(9);
        return;
    default:
        return;
    }
}

// original 0x53D010: run 0x14: message 0x18, then the end.
extern "C" void __cdecl Scena01_Scene14(void) {
    switch (Step()) {
    case 0:
        OpenMessage(0x18);
        SetStep(1);
        return;
    case 1:
        if (MessageUp()) return;
        Clear40();
        Counter(1) = 0;
        Counter(2) = 0;
        SetStep(0);
        SetRun(0);
        return;
    default:
        return;
    }
}

// original 0x53D070: run 0x15: message 0x34, then run 6 from step 0.
extern "C" void __cdecl Scena01_Scene15(void) {
    switch (Step()) {
    case 0:
        OpenMessage(0x34);
        SetStep(1);
        return;
    case 1:
        if (MessageUp()) return;
        Clear40();
        SetStep(0);
        SetRun(6);
        return;
    default:
        return;
    }
}

// original 0x53D0C0: run 0x16: message 0x34, then the end (run 0).
extern "C" void __cdecl Scena01_Scene16(void) {
    switch (Step()) {
    case 0:
        OpenMessage(0x34);
        SetStep(1);
        return;
    case 1:
        if (MessageUp()) return;
        Clear40();
        SetStep(0);
        SetRun(0);
        return;
    default:
        return;
    }
}

namespace {

// Scena01_Scene17 steps 0xD and 0x17: with two or three members the change to
// area 0xA (two: (0x500000, 0x50000); three: (0x510000, 0x80000)), counter 2
// = 8, step 0x18, music 7.
void Scene17Leave() {
    const unsigned n = Field_MemberCount;
    if (n != 2 && n != 3) return;
    Counter(2) = 8;
    SetStep(0x18);   // ecx, the switch's bound, in the original
    if (n == 2)
        ChangeArea(0xA, 0x500000, 0x50000, 0x88);
    else
        ChangeArea(0xA, 0x510000, 0x80000, 0x89);
    SH_CALL(Music_Play)(7, 8);
}

}  // namespace

// original 0x53D110: run 0x17, steps 0, 2, 3, 5, 0xA..0xD and 0x14..0x18 (a
// byte table over 0..0x18 into a jump table of 13; 0xD and 0x17 share a
// case): the shape of run 0x11 - transition 0xD on counter 0 = 0x64, the end
// on 0x65, the shop on 0xC8 - with the party restored and the stream played
// twice and the change to area 0xA by the member count.
extern "C" void __cdecl Scena01_Scene17(void) {
    switch (Step()) {
    case 0:
        if (Counter(0) == 0x64) {
            Counter(0) = 0;
            Transition(0xD);
            SetStep(0x14);
        }
        EndAfter65();
        return;
    case 2:
        if (ShopOrEnd()) {
            Counter(0) = 0;
            Counter(1) = 0;
            Counter(2) = 0;
            Clear40();
            SetStep(0);
            SetRun(0);
        }
        return;
    case 3:
        if (Field_Request != 0) return;
        Counter(0) = 0;
        Counter(1) = 0;
        Counter(2) = 0;
        Counter(3) = 0;
        Clear40();
        SetStep(0);
        SetRun(0);
        return;
    case 5:
        if (Counter(0) == 0x64) {
            SetStep(1);
            Counter(0) = 0;
            Counter(2) = 2;
            Transition(0xD);
            SetStep(0xA);
        }
        EndAfter65();
        return;
    case 0xA:
        RestoreAndFade(0xB);
        return;
    case 0xB:
        Stream(0xC);
        return;
    case 0xC:
        StreamWait(0xD);
        return;
    case 0xD:
    case 0x17:
        Scene17Leave();
        return;
    case 0x14:
        RestoreAndFade(0x15);
        return;
    case 0x15:
        Stream(0x16);
        return;
    case 0x16:
        StreamWait(0x17);
        return;
    case 0x18:
        if (Counter(0) != 1) return;
        Counter(1) = 0;
        Counter(2) = 0;
        Counter(3) = 0;
        Clear40();
        SetStep(0);
        SetRun(0);
        return;
    default:
        return;
    }
}

// original 0x53D3B0: Scena01_Scene0D's effect (0x92 and 0x93): Sprite_Current
// set to the leader's record, an effect slot taken into the leader's +0xB
// and, when there is one, its record made live as kind 6 with +6 = the
// argument + 0x70, +0xC = 0, +0x10 = the s8 at 0x660D60 by the byte 0x904062,
// and the leader's words +0x2E / +0x30 at +0x2E / +0x30.
//
// As the original has it: Sprite_Current is read back after Effect_FindFree
// (the store goes where it then points), and the slot is read back from +0xB
// for each field; the s8 table is read unbounded (0x904062 is a member's id;
// past the table lie the chapter's hooks, readable .data, no fault).
extern "C" void __cdecl Scena01_PlaceEffect(unsigned char n) {
    Sprite_Current = Leader();
    const unsigned char slot = SH_CALL(Effect_FindFree)();
    Sprite_Current[0xB] = slot;
    unsigned char* const cur = Sprite_Current;
    if (cur[0xB] == 0xFF) return;
    auto rec = [cur] { return Effect_Objects + (static_cast<std::uint32_t>(cur[0xB]) << 7); };
    rec()[0] = 1;
    const unsigned char kind = static_cast<unsigned char>(n + 0x70);
    rec()[5] = 6;
    rec()[6] = kind;
    SetLong(rec() + 0xC, 0);
    SetLong(rec() + 0x10, static_cast<signed char>(At(at::kEffectX + At(at::kMemberOrder)[0])[0]));
    SetWord(rec() + 0x2E, Word(cur + 0x2E));
    SetWord(rec() + 0x30, Word(cur + 0x30));
}

// original 0x53D470: slot 1 of chapter 1's vtable, the object hook (0x56D6D0
// calls it with the object whose trigger fired): the handler
// Scena01_ObjectHandlers[object +0x86] called with the object and 0x903F98.
// The table holds 18 (entry 0 is 0x437CC0, a bare ret); the original indexes
// it by any byte, ours aborts past it. 0x56D6D0 reads no answer.
extern "C" void __cdecl Scena01_ObjectHook(unsigned char* object) {
    const unsigned k = object[0x86];
    if (k >= at::kObjectCount)
        bof3::Fatal("Scena01_ObjectHook: object +0x86 %u past Scena01_ObjectHandlers (%u entries)", k, at::kObjectCount);
    using ObjectFn = void (__cdecl*)(unsigned char*, unsigned char*);
    reinterpret_cast<ObjectFn>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(At(at::kObjects + 4 * k)))))(object, At(at::kStartWord));
}

namespace {

// The object handlers' start of a scene: ScriptFlags_Set40, then the counters
// 1 and 2 cleared with the step and run.
void ObjectScene(unsigned step, unsigned run) {
    Set40();
    Counter(1) = 0;
    Counter(2) = 0;
    SetStep(step);
    SetRun(run);
}
// The object handlers 6..9's turn: the object's state 4, its +0x87 0 and
// +0x83 `pose`; Sprite_Current turned to the leader's facing ^ 4 - the
// original tests whether it already faces that way, but its test (setne,
// xor 4) can never be 0, so it always turns - Sprite_FaceDirection, and +7
// bit 3 on Sprite_Current as it is after the call.
void Turn() {
    unsigned char* const cur = Sprite_Current;
    cur[8] = static_cast<unsigned char>(Leader()[8] ^ 4);
    SH_CALL(Sprite_FaceDirection)(Sprite_Current[8]);
    Sprite_Current[7] |= 8;
}

}  // namespace

// original 0x53D490: object handler 1: once (flag 0x14), counter 0 = 0, run
// 0x12 from step 0.
extern "C" void __cdecl Scena01_Object01(unsigned char*, unsigned char*) {
    if (Flag(0x14)) return;
    Counter(0) = 0;
    Set40();
    SetStep(0);
    SetRun(0x12);
    SetFlag(0x14);
}
// original 0x53D4D0: object handler 2: run 0x17 from step 0.
extern "C" void __cdecl Scena01_Object02(unsigned char*, unsigned char*) { ObjectScene(0, 0x17); }
// original 0x53D4F0: object handler 3: run 0x17 from step 5.
extern "C" void __cdecl Scena01_Object03(unsigned char*, unsigned char*) { ObjectScene(5, 0x17); }
// original 0x53D510: object handler 4: run 0x17 from step 2.
extern "C" void __cdecl Scena01_Object04(unsigned char*, unsigned char*) { ObjectScene(2, 0x17); }
// original 0x53D530: object handler 5: run 0x11 from step 2.
extern "C" void __cdecl Scena01_Object05(unsigned char*, unsigned char*) { ObjectScene(2, 0x11); }

// original 0x53D550: object handler 6: counter 0 = 0xA, counters 1, 2 = 0;
// the object's state 4, +0x8A 0, +0x87 0, +0x83 0xA; the turn; run 0xD at
// step 0xA.
extern "C" void __cdecl Scena01_Object06(unsigned char* object, unsigned char*) {
    Set40();
    Counter(0) = 0xA;
    Counter(1) = 0;
    Counter(2) = 0;
    object[1] = 4;
    SetWord(object + 0x8A, 0);
    object[0x87] = 0;
    object[0x83] = 0xA;
    Turn();
    SetStep(0xA);
    SetRun(0xD);
}
// original 0x53D5D0: object handler 7: the object's state 4, +0x87 0, +0x83
// 0xA (+0x8A kept); the turn; the counters cleared; run 0xD at step 0xD.
extern "C" void __cdecl Scena01_Object07(unsigned char* object, unsigned char*) {
    Set40();
    object[1] = 4;
    object[0x87] = 0;
    object[0x83] = 0xA;
    Turn();
    Counter(0) = 0;
    Counter(1) = 0;
    Counter(2) = 0;
    Counter(3) = 0;
    SetStep(0xD);
    SetRun(0xD);
}
// original 0x53D650: object handler 8: the object's state 4, +0x8A 0, +0x87
// 0, +0x83 0xB; the turn; the counters cleared; run 0xD at step 0x10.
extern "C" void __cdecl Scena01_Object08(unsigned char* object, unsigned char*) {
    Set40();
    object[1] = 4;
    SetWord(object + 0x8A, 0);
    object[0x87] = 0;
    object[0x83] = 0xB;
    Turn();
    Counter(0) = 0;
    Counter(1) = 0;
    Counter(2) = 0;
    Counter(3) = 0;
    SetStep(0x10);
    SetRun(0xD);
}
// original 0x53D6E0: object handler 9: as 8, but only counter 3 cleared.
extern "C" void __cdecl Scena01_Object09(unsigned char* object, unsigned char*) {
    Set40();
    object[1] = 4;
    SetWord(object + 0x8A, 0);
    object[0x87] = 0;
    object[0x83] = 0xB;
    Turn();
    Counter(3) = 0;
    SetStep(0x10);
    SetRun(0xD);
}
// original 0x53D760 / 0x53D770 / 0x53D780 / 0x53D790: object handlers 0xA..0xD:
// counter 0 = 0x64, 0x66, 0x67, 0x68 (the waits of runs 0x11, 0x17 and 9).
extern "C" void __cdecl Scena01_Object0A(unsigned char*, unsigned char*) { Counter(0) = 0x64; }
extern "C" void __cdecl Scena01_Object0B(unsigned char*, unsigned char*) { Counter(0) = 0x66; }
extern "C" void __cdecl Scena01_Object0C(unsigned char*, unsigned char*) { Counter(0) = 0x67; }
extern "C" void __cdecl Scena01_Object0D(unsigned char*, unsigned char*) { Counter(0) = 0x68; }
// original 0x53D7A0: object handler 0xE: the counters cleared, run 9 at step
// 0x28.
extern "C" void __cdecl Scena01_Object0E(unsigned char*, unsigned char*) {
    Set40();
    SetStep(0x28);
    Counter(0) = 0;
    Counter(1) = 0;
    Counter(2) = 0;
    Counter(3) = 0;
    SetRun(9);
}
// original 0x53D7D0: object handler 0xF: run 0x11 from step 0.
extern "C" void __cdecl Scena01_Object0F(unsigned char*, unsigned char*) { ObjectScene(0, 0x11); }
// original 0x53D7F0: object handler 0x10: run 0x11 from step 5.
extern "C" void __cdecl Scena01_Object10(unsigned char*, unsigned char*) { ObjectScene(5, 0x11); }
// original 0x53D810: object handler 0x11: run 0xB at step 0xF (the counters
// kept).
extern "C" void __cdecl Scena01_Object11(unsigned char*, unsigned char*) {
    Set40();
    SetStep(0xF);
    SetRun(0xB);
}

// original 0x53DD20: slot 4 of chapter 1's vtable, the cell hook (x, z; al):
// 0x56D800 finds (x, z) among Scena01_Cells' two records; none (a negative
// al) answers 0xFF, else a tail jump through Scena01_CellHandlers with the
// same arguments. The original indexes the table by any al of 0..0x7F; ours
// aborts past its two.
extern "C" unsigned char __cdecl Scena01_CellHook(int x, int z) {
    const unsigned char found = SH_AT(cl::CellFindFn, cl::kCellFind)(At(at::kCells), 2, x, z);
    if (static_cast<signed char>(found) < 0) return 0xFF;
    if (found >= at::kCellCount)
        bof3::Fatal("Scena01_CellHook: cell %u past Scena01_CellHandlers (%u entries)", found, at::kCellCount);
    using CellFn = unsigned char (__cdecl*)(int, int);
    return reinterpret_cast<CellFn>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(At(at::kCellHandlers + 4u * found)))))(x, z);
}

// original 0x53DD50: both of Scena01_CellHandlers' entries: with flag 3 set
// and flag 4 clear, run 5 from step 0 and 1; else 0xFF.
extern "C" unsigned char __cdecl Scena01_Cell(int, int) {
    if (!Flag(3) || Flag(4)) return 0xFF;
    Set40();
    SetStep(0);
    SetRun(5);
    return 1;
}

void ScenaSc1_Inject() {
    if (bof3::WantsShadow("scena_sc1")) scena_sc1::SelfTest();
    BOF3_INJECT(Scena01_Start);
    BOF3_INJECT(Scena01_Run);
    BOF3_INJECT(Scena01_Scene01);
    BOF3_INJECT(Scena01_Scene02);
    BOF3_INJECT(Scena01_Scene03);
    BOF3_INJECT(Scena01_Scene05);
    BOF3_INJECT(Scena01_Scene06);
    BOF3_INJECT(Scena01_Scene08);
    BOF3_INJECT(Scena01_Scene09);
    BOF3_INJECT(Scena01_Scene0A);
    BOF3_INJECT(Scena01_Scene0B);
    BOF3_INJECT(Scena01_Scene0C);
    BOF3_INJECT(Scena01_Scene0D);
    BOF3_INJECT(Scena01_Scene0E);
    BOF3_INJECT(Scena01_Scene0F);
    BOF3_INJECT(Scena01_Scene11);
    BOF3_INJECT(Scena01_Scene12);
    BOF3_INJECT(Scena01_Scene14);
    BOF3_INJECT(Scena01_Scene15);
    BOF3_INJECT(Scena01_Scene16);
    BOF3_INJECT(Scena01_Scene17);
    BOF3_INJECT(Scena01_PlaceEffect);
    BOF3_INJECT(Scena01_ObjectHook);
    BOF3_INJECT(Scena01_Object01);
    BOF3_INJECT(Scena01_Object02);
    BOF3_INJECT(Scena01_Object03);
    BOF3_INJECT(Scena01_Object04);
    BOF3_INJECT(Scena01_Object05);
    BOF3_INJECT(Scena01_Object06);
    BOF3_INJECT(Scena01_Object07);
    BOF3_INJECT(Scena01_Object08);
    BOF3_INJECT(Scena01_Object09);
    BOF3_INJECT(Scena01_Object0A);
    BOF3_INJECT(Scena01_Object0B);
    BOF3_INJECT(Scena01_Object0C);
    BOF3_INJECT(Scena01_Object0D);
    BOF3_INJECT(Scena01_Object0E);
    BOF3_INJECT(Scena01_Object0F);
    BOF3_INJECT(Scena01_Object10);
    BOF3_INJECT(Scena01_Object11);
    BOF3_INJECT(Scena01_CellHook);
    BOF3_INJECT(Scena01_Cell);
}

// Scenario chapter 2's bank (the PSX's SCENA02.EMI), compiled into the exe at
// 0x53DDA0..0x5428B2. docs/scena_sc2.md.
//
//   - The state machine: Scena02_Frame (vtable slot 0) jumps through
//     Scena02_States on the state byte: 0 Scena02_Start, 1 Scena02_EnterArea
//     (once per area entered; every way out stores state 2), 2 Scena02_Run,
//     which jumps through Scena02_Runs on the run MoveScript_Var7 to one of
//     twenty-four scenes (the other four entries are 0x437CC0, a bare ret).
//     Run 0 is not a bare ret here: Scena02_Scene00 runs whenever no scene
//     does, and in area 0x1A starts run 7 when the leader walks up to one of
//     the sprites (the twelve Scena02_Near helpers) and run 0x1A at three
//     cells (Scena02_DoorStart / Scena02_DoorSound).
//   - A scene is a switch on the step byte 0x8034E5: each step waits on a
//     script counter (0x903848..B), the message box (Field_Request), the wait
//     word (MoveScript_WaitWordDA), a flag, a stream or the leader's cell,
//     does one thing - a message, a sound, the music, an effect, an area
//     change, a party member dropped in, an event battle - and sets the next
//     step; the last clears the counters and sets the run and step to 0.
//   - Slot 1, the object hook: through Scena02_ObjectHandlers on the object's
//     +0x86 to one of twenty-two small handlers that start a scene or set a
//     counter (entry 0xA is 0x557170, group SC9b's block).
//   - Slot 2, the step hook: by area, rectangles of the leader's target
//     (x, z) that start a scene, answering 1 in al; 0 otherwise.
//
// Every call goes through the scenario harness (SH_CALL / SH_AT), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies.
// No divergence: each is a faithful replacement, except that an index past a
// table - the state past Scena02_States, the run past Scena02_Runs, the
// object's +0x86 past Scena02_ObjectHandlers - aborts where the original
// would jump through whatever lies there (the spell round's rule,
// docs/takeover-queue-round9.md section 6).
#include "game/scena_sc2.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/scena_sc2_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = scena_sc2::at;
namespace cl = scena_sc2::callee;
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
void SetAreaByte(unsigned v) { At(at::kAreaByte)[0] = static_cast<unsigned char>(v); }
unsigned Area() { return Game_AreaNumber; }
unsigned LeadMember() { return At(at::kLeadMember)[0]; }
bool MessageUp() { return Field_Request == 2; }
void OpenMessage(unsigned id) {
    SH_CALL(Msg_OpenScript)(static_cast<unsigned short>(id));
    Field_Request = 2;
}
bool Waiting() { return MoveScript_WaitWordDA != 0; }
bool Chosen() { return (At(at::kChoiceBits)[0] & 2) != 0; }
// Field_ScriptFlags: the original ors its low byte (`or byte ptr`) and xors
// the word; on a u16 both are these.
void ScriptOr(unsigned bits) { Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags | bits); }
void ScriptXor(unsigned bits) { Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags ^ bits); }

// The leader's record (ObjTrio's first member): +0x34 x, +0x38 z (16.16),
// +0x36 / +0x3A the cell words, +0x4B, +8.
unsigned char* Leader() { return ObjTrio; }
unsigned LeaderCellX() { return Word(Leader() + 0x36); }
short LeaderCellZ() { return static_cast<short>(Word(Leader() + 0x3A)); }

// The flag row: the dword 0x929ED0 read afresh at each call.
unsigned char* Bank() { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(At(at::kFlagBank))))); }
bool Flag(unsigned n) { return SH_CALL(Flags_Test)(Bank(), n) != 0; }
void SetFlag(unsigned n) { SH_CALL(Flags_Set)(Bank(), n); }
void ClearFlag(unsigned n) { SH_CALL(Flags_Clear)(Bank(), n); }

// The calls every scene makes.
void ChangeArea(unsigned area, int x, int z, unsigned flags) { SH_CALL(Field_ChangeArea)(area, x, z, flags); }
void MusicStop(int frames) { SH_CALL(Music_FadeOutStop)(frames); }
void MusicPlay(unsigned track) { SH_CALL(Music_Play)(track, 8); }
void Sound(unsigned id) { SH_CALL(Sound_PlayEffect)(static_cast<unsigned short>(id)); }
void Transition(unsigned kind) { SH_CALL(Transition_Start)(static_cast<unsigned char>(kind)); }
void DropIn(unsigned entry) { SH_CALL(Party_DropIn)(entry); }
void Kind2(unsigned n) { SH_CALL(Kind2_Place)(static_cast<unsigned char>(n)); }
void Set40() { SH_CALL(ScriptFlags_Set40)(); }
void Clear40() { SH_CALL(ScriptFlags_Clear40)(); }
void LoadStream(unsigned id) { SH_CALL(Sound_LoadStream)(id); }
bool StreamDone() { return SH_CALL(Sound_StreamDone)() != 0; }
void Battle(unsigned id) { SH_CALL(Field_StartEventBattle)(id); }
void PartyRestore() { SH_AT(cl::VoidFn, cl::kPartyRestore)(); }
void PartyPlace(int x, int z, unsigned kind) { SH_AT(cl::PartyPlaceFn, cl::kPartyPlace)(x, z, kind); }
bool TurnTest(int a, int b) { return SH_AT(cl::TurnTestFn, cl::kTurnTest)(a, b) != 0; }
// Music_LoadFile(track), then Task_Sleep(1) until File_LoadDone.
void LoadMusic(unsigned track) {
    SH_CALL(Music_LoadFile)(track);
    while (SH_CALL(File_LoadDone)() == 0) SH_CALL(Task_Sleep)(1);
}

// An effect slot taken into 0x903850; false (the slot 0xFF stored) when none
// is free.
bool TakeEffect() {
    const unsigned char slot = SH_CALL(Effect_FindFree)();
    At(at::kEffectSlot)[0] = slot;
    return slot != 0xFF;
}
// The record of the slot 0x903850 holds, read back as the original does.
unsigned char* Effect() { return Effect_Objects + (static_cast<std::uint32_t>(At(at::kEffectSlot)[0]) << 7); }
// A kind-0x13 effect live at (x, z) with +0x6C `far` and +9 `life`.
void AimEffect(int x, int z, int far, unsigned life) {
    unsigned char* const e = Effect();
    e[0] = 1;
    e[5] = 0x13;
    SetLong(e + 0x64, x);
    SetLong(e + 0x68, z);
    SetLong(e + 0x6C, far);
    e[9] = static_cast<unsigned char>(life);
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

// The counters 1..3 cleared, ScriptFlags_Clear40, the step and run 0: the
// end most scenes share (counter 0 kept).
void EndKeep0() {
    Counter(1) = 0;
    Counter(2) = 0;
    Counter(3) = 0;
    Clear40();
    SetStep(0);
    SetRun(0);
}
// The same with counter 0 cleared first.
void EndAll() {
    Counter(0) = 0;
    EndKeep0();
}
// The other order: ScriptFlags_Clear40 first, then the counters 1..3, the
// step and run 0.
void ClearThenEnd() {
    Clear40();
    Counter(1) = 0;
    Counter(2) = 0;
    Counter(3) = 0;
    SetStep(0);
    SetRun(0);
}
// ScriptFlags_Clear40, the step and run 0 (no counter touched).
void EndPlain() {
    Clear40();
    SetStep(0);
    SetRun(0);
}

// The flags scenes 6 and 7 reset when flag 7 is set: 0x2B, 0x2C, 0x3E cleared,
// 0x34 set, 0x35, 9, 0xA, 0xB cleared.
void ResetFlags7() {
    ClearFlag(0x2B);
    ClearFlag(0x2C);
    ClearFlag(0x3E);
    SetFlag(0x34);
    ClearFlag(0x35);
    ClearFlag(9);
    ClearFlag(0xA);
    ClearFlag(0xB);
}

std::uint32_t Entry(std::uint32_t table, unsigned index) { return static_cast<std::uint32_t>(Long(At(table + 4u * index))); }

// A 32-bit sum as the original's lea / add computes it (wrapping).
int Plus(int v, int d) { return static_cast<int>(static_cast<std::uint32_t>(v) + static_cast<std::uint32_t>(d)); }

}  // namespace

// original 0x53DDA0: slot 0 of chapter 2's vtable (Field_ModeDispatch's call
// every field frame), a tail jump through Scena02_States on the s8 state
// 0x8034E2: 0 Scena02_Start, 1 Scena02_EnterArea, 2 Scena02_Run. The table
// holds 3; the original reads any s8 index, ours aborts past it.
extern "C" void __cdecl Scena02_Frame(void) {
    const int state = static_cast<signed char>(At(at::kState)[0]);
    if (state < 0 || state >= static_cast<int>(at::kStateCount))
        bof3::Fatal("Scena02_Frame: state %d past Scena02_States (%u entries)", state, at::kStateCount);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(Entry(at::kStates, static_cast<unsigned>(state))))();
}

// original 0x53DDB0: chapter 2's state 0 (Scena02_States entry 0): state 1,
// the first dword of chapter 2's flag row 0x903FA0 and the four counters
// cleared.
extern "C" void __cdecl Scena02_Start(void) {
    At(at::kState)[0] = 1;
    SetLong(At(at::kRow2), 0);
    SetLong(At(at::kCounters), 0);
}

namespace {

// Scena02_EnterArea's body: every way out of it ends with state 2, which the
// caller stores. Each area test reads Game_AreaNumber afresh, each counter
// test the counter byte afresh.
void EnterAreaBody() {
    if (Area() == 0) {
        if (!Flag(0x37)) Draw_PassFlags = 0x1F;
        if (Flag(0x19)) SetFlag(3);
    }
    if (Area() == 3 && Flag(0x1D)) {
        if (!Flag(0x1F)) {
            SH_CALL(Scenario_CallB)(1);
            SH_CALL(Scena02_StripMember)(3);
            SH_CALL(Scena02_StripMember)(4);
            Counter(0) = 0;
            Counter(1) = 0;
            Counter(2) = 0;
            Counter(3) = 0;
            SetRun(0x15);
            SetStep(0);
        }
        if (!Flag(0x20)) {
            LoadMusic(0xF);
            Counter(0) = 0;
            Counter(1) = 0;
            Counter(2) = 0;
            Counter(3) = 0;
        }
    }
    if (Area() == 5) {
        switch (Counter(2)) {
        case 2:
            ScriptXor(7);
            return;
        case 4:
            Counter(0) = 0;
            SetStep(3);
            SetRun(6);
            return;
        case 5:
            Draw_PassFlags = 0x1F;
            return;
        case 6:
            if (!Flag(0x19)) {
                Draw_PassFlags = 0x1F;
                Counter(0) = 0;
                Counter(1) = 0;
                Counter(3) = 0;
                SetRun(0x11);
                SetFlag(0x19);
                SetFlag(0x37);
            }
            break;
        default:   // 0, 1, 3, and 7 and up
            return;
        }
    }
    if (Area() == 0xB && !Flag(0x1D)) {
        Draw_PassFlags = 0;
        Counter(0) = 0;
        SetRun(0x13);
        SetStep(0xA);
    }
    if (Area() == 0xD) {
        const unsigned c = Counter(2);
        if (c == 1) {
            Draw_PassFlags = 0x1F;
            return;
        }
        if (c != 0) return;
        if (Flag(0x1A) && !Flag(0x1B) && Cond_ByteFD == 1) {
            Draw_PassFlags = 0;
            Set40();
            Counter(0) = 0;
            Counter(1) = 0;
            Counter(2) = 0;
            Counter(3) = 0;
            SetStep(5);
            SetRun(0x12);
        }
    }
    if (Area() == 0xF && !Flag(0)) {
        SetFlag(0);
        const unsigned char status = Field_StatusBits;
        ScriptOr(0x80);
        Field_StatusBits = static_cast<unsigned char>(status | 1);
        Set40();
        ScriptOr(0x30);
        Counter(0) = 0;
        Counter(1) = 0;
        Counter(2) = 0;
        Counter(3) = 0;
        SetStep(0);
        SetRun(2);
    }
    // One read of the area for the five tests and the 0x1A test after them
    // (no call between).
    const unsigned area = Area();
    if (area == 0x12 || area == 0x17 || area == 0x16 || area == 0x1D || area == 0x2D) {
        Counter(0) = 0;
        Counter(1) = 0;
        Counter(2) = 0;
        Counter(3) = 0;
        SetStep(0);
        SetRun(0);
        SetWord(At(at::kTimer), 0);
    }
    if (area == 0x1A) {
        switch (Counter(2)) {
        case 0:
            Clear40();
            Counter(0) = 0;
            Counter(1) = 0;
            Counter(2) = 0;
            Counter(3) = 0;
            break;
        case 1:
            if (!Flag(7)) {
                Counter(0) = 0;
                Counter(1) = 0;
                Counter(3) = 0;
                SetStep(4);
            }
            break;
        case 3:
            if (!Flag(0xC)) {
                Counter(0) = 0;
                Counter(1) = 0;
                Counter(3) = 0;
                SetStep(0xF);
            }
            break;
        default:   // 2, and 4 and up
            return;
        }
    }
    if (Area() == 0x1B) {
        switch (Counter(2)) {
        case 0:
            if (!Flag(8)) {
                Counter(0) = 0;
                Counter(1) = 0;
                Counter(2) = 0;
                Counter(3) = 0;
                SetFlag(8);
            }
            break;
        case 2:
            Draw_PassFlags = 0x1F;
            return;
        case 4:
            Counter(0) = 0;
            return;
        default:   // 1, 3, and 5 and up
            return;
        }
    }
    if (Area() == 0x1C) {
        switch (Counter(2)) {
        case 0:
            if (!Flag(0x15)) {
                Counter(0) = 0;
                Counter(1) = 0;
                Counter(2) = 0;
                Counter(3) = 0;
                Draw_PassFlags = 0;
                SetStep(0);
                SetRun(0x10);
            }
            break;
        case 2:
            SetFlag(0x16);
            SH_CALL(MoveCmd_TestFB)(0x27, 0x15);
            break;
        default:   // 1, and 3 and up
            return;
        }
    }
    if (Area() == 0x5A) {
        if ((Field_StatusBits & 1) == 0) return;
        if (!Flag(2)) {
            SetFlag(2);
            MusicStop(0xA);
            LoadMusic(0x4D);
            MusicPlay(0x4D);
        }
    }
    if (Area() == 0x10) {
        const unsigned char status = Field_StatusBits;
        Counter(0) = 0;
        Counter(1) = 0;
        Counter(2) = 0;
        Counter(3) = 0;
        SetStep(0);
        SetRun(0);
        SetWord(At(at::kTimer), 0);
        if ((status & 1) != 0 && Word(At(at::kExtraWord)) == 0x5A) {
            ClearFlag(2);
            MusicStop(0xA);
            LoadMusic(0x11);
            MusicPlay(0x11);
        }
    }
}

}  // namespace

// original 0x53DDD0: chapter 2's state 1 (Scena02_States entry 1), once per
// area entered. By the area number (read afresh at each test), the flags,
// the counter 0x90384A and Cond_ByteFD: area 0 the pass flags and flag 3;
// area 3 with flag 0x1D the party's call B(1), members 3 and 4 stripped and
// run 0x15 (flag 0x1F clear), music 0xF loaded (flag 0x20 clear); area 5 by
// counter 2 (a jump table of 5 on 2..6): the script flags ^ 7, run 6 at step
// 3, the pass flags, or run 0x11 with flags 0x19, 0x37; area 0xB run 0x13 at
// step 0xA; area 0xD run 0x12 at step 5; area 0xF flag 0 and run 2; areas
// 0x12, 0x16, 0x17, 0x1D, 0x2D all cleared; areas 0x1A, 0x1B, 0x1C by
// counter 2; area 0x5A music 0x4D; area 0x10 music 0x11. Every way out
// stores state 2.
extern "C" void __cdecl Scena02_EnterArea(void) {
    EnterAreaBody();
    At(at::kState)[0] = 2;
}

// original 0x53E3D0: chapter 2's state 2 (Scena02_States entry 2), a tail
// jump through Scena02_Runs on the s8 MoveScript_Var7. The table holds 28;
// the original reads any s8 index, ours aborts past the table.
extern "C" void __cdecl Scena02_Run(void) {
    const int run = MoveScript_Var7;
    if (run < 0 || run >= static_cast<int>(at::kRunCount))
        bof3::Fatal("Scena02_Run: run %d past Scena02_Runs (%u entries)", run, at::kRunCount);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(Entry(at::kRuns, static_cast<unsigned>(run))))();
}

// original 0x53E3E0: run 0, while no scene runs. Only in area 0x1A without
// flag 0x3F: the near tests on sprites 0..9 by flags 0x28, 0x29, 0x2B, 0x2C,
// 7, 0x34 and 0x2E (each starts run 7 when the leader is inside its
// rectangle), then a tail jump to Scena02_DoorStart (flag 0x2E clear, after
// the test on sprite 9) or Scena02_DoorSound.
extern "C" void __cdecl Scena02_Scene00(void) {
    if (Flag(0x3F)) return;
    if (Area() != 0x1A) return;
    if (Flag(0x28)) SH_CALL(Scena02_Near02)(0);
    if (Flag(0x29)) SH_CALL(Scena02_Near01)(0);
    if (!Flag(7)) {
        if (Flag(0x2B)) SH_CALL(Scena02_Near03)(5);
        if (Flag(0x2C)) SH_CALL(Scena02_Near04)(5);
    } else if (Flag(0x34)) {
        if (Flag(0x2B)) SH_CALL(Scena02_Near03)(5);
        if (Flag(0x2C)) SH_CALL(Scena02_Near04)(5);
    }
    SH_CALL(Scena02_Near05)(1);
    SH_CALL(Scena02_Near06)(2);
    SH_CALL(Scena02_Near09)(3);
    SH_CALL(Scena02_Near0A)(4);
    if (!Flag(7) || Flag(0x34)) {
        SH_CALL(Scena02_Near07)(6);
        SH_CALL(Scena02_Near08)(7);
        SH_CALL(Scena02_Near0B)(8);
    }
    if (!Flag(0x2E)) {
        SH_CALL(Scena02_Near0C)(9);
        SH_CALL(Scena02_DoorStart)();
        return;
    }
    SH_CALL(Scena02_DoorSound)();
}

// original 0x53E570: run 2, steps 0..0x23 (a byte table of 36 into a jump
// table of 15): the change to area 0xF, flag 4 on counter 0 = 8; members 1
// and 2 dropped in, flags 5 and 6 and the change to area 0x1A; on counter 0 =
// 0x1E the change to area 0x1A by the member count; the choice 0x64 / 0x65
// (the party restored, the stream played), music 0x11 and message 0xA.
extern "C" void __cdecl Scena02_Scene02(void) {
    switch (Step()) {
    case 0:
        SetStep(1);
        ChangeArea(0xF, 0x360000, 0x500000, 0x80);
        return;
    case 1:
        Draw_PassFlags = 0x1F;
        SetStep(2);
        return;
    case 2:
        if (Counter(0) != 8) return;
        ScriptOr(6);
        Clear40();
        Counter(1) = 0;
        Counter(2) = 0;
        SetStep(0);
        SetRun(0);
        SetFlag(4);
        return;
    case 5:
        DropIn(1);
        SetStep(6);
        return;
    case 6:
        if (Counter(0) != 0x22) return;
        Clear40();
        Counter(1) = 0;
        Counter(2) = 0;
        SetStep(0);
        SetRun(0);
        SetFlag(5);
        SetFlag(6);
        ScriptXor(0xB0);
        ChangeArea(0x1A, 0x310000, 0x4B0000, 0x85);
        return;
    case 7:
        DropIn(2);
        SetStep(8);
        return;
    case 8:
        if (Counter(0) != 0x22) return;
        Clear40();
        Counter(1) = 0;
        Counter(2) = 0;
        SetRun(0);
        SetStep(0);
        SetFlag(5);
        SetFlag(6);
        ScriptXor(0xB0);
        ChangeArea(0x1A, 0x310000, 0x4B0000, 0x85);
        return;
    case 0x14:
        if (Counter(0) != 0x1E) return;
        Counter(2) = 0;
        SetStep(0);
        SetRun(0);
        ScriptXor(0x80);
        ChangeArea(0x1A, 0x310000, 0x4B0000, Field_MemberCount == 3 ? 0x85 : 0x8B);
        return;
    case 0x1E: {
        if (MessageUp()) return;
        const unsigned c = Counter(0);
        if (c == 0x64) {
            Counter(0) = 0;
            SetStep(0x1F);
            Transition(0xD);
            PartyRestore();
            MusicStop(0x1E);
        } else if (c == 0x65) {
            Clear40();
            Counter(0) = 0;
            SetStep(0);
            SetRun(0);
        }
        return;
    }
    case 0x1F:
        LoadStream(0);
        SetStep(0x20);
        return;
    case 0x20:
        if (Waiting()) return;
        Draw_PassFlags = 0;
        if (!StreamDone()) return;
        SetStep(0x21);
        return;
    case 0x21:
        Transition(0xE);
        Draw_PassFlags = 0x1F;
        SetStep(0x22);
        return;
    case 0x22:
        if (Waiting()) return;
        MusicPlay(0x11);
        OpenMessage(0xA);
        SetStep(0x23);
        return;
    case 0x23:
        if (MessageUp()) return;
        Clear40();
        SetStep(0);
        SetRun(0);
        return;
    default:   // 3, 4, 9..0x13, 0x15..0x1D, and 0x24 and up
        return;
    }
}

// original 0x53E890: run 4, steps 0..8 (a jump table of 9; 2..4 nothing):
// run 2's first steps with the change to area 0xF at (0x2C0000, 0x430000),
// and its member steps ending with the script flags ^ 0x80 (not 0xB0) and the
// byte 0x937F98 set.
extern "C" void __cdecl Scena02_Scene04(void) {
    switch (Step()) {
    case 0:
        SetStep(1);
        ChangeArea(0xF, 0x2C0000, 0x430000, 0x80);
        return;
    case 1:
        if (Counter(0) != 8) return;
        ScriptOr(6);
        Clear40();
        Counter(1) = 0;
        Counter(2) = 0;
        SetStep(0);
        SetRun(0);
        SetFlag(4);
        return;
    case 5:
        DropIn(1);
        SetStep(6);
        return;
    case 6:
        if (Counter(0) != 0x22) return;
        Clear40();
        Counter(1) = 0;
        Counter(2) = 0;
        SetStep(0);
        SetRun(0);
        SetFlag(5);
        SetFlag(6);
        ScriptXor(0x80);
        ChangeArea(0x1A, 0x310000, 0x4B0000, 0x85);
        At(at::kFrameByte)[0] = 1;
        return;
    case 7:
        DropIn(2);
        SetStep(8);
        return;
    case 8:
        if (Counter(0) != 0x22) return;
        Clear40();
        Counter(1) = 0;
        Counter(2) = 0;
        SetRun(0);
        SetStep(0);
        SetFlag(5);
        SetFlag(6);
        ScriptXor(0x80);
        ChangeArea(0x1A, 0x310000, 0x4B0000, 0x85);
        At(at::kFrameByte)[0] = 1;
        return;
    default:   // 2..4, and 9 and up
        return;
    }
}

// original 0x53EA00: run 5, two steps: message 0x3E, then the end once the box
// closes.
extern "C" void __cdecl Scena02_Scene05(void) {
    switch (Step()) {
    case 0:
        OpenMessage(0x3E);
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

// original 0x53EA50: run 6, steps 0..5 (a jump table of 6; 4 nothing): message
// 0x30 and its choice (counter 0 = 0xBE / 0xBF); the change to area 0xF with
// the byte 0x904CD0 = 0x11; flag 0x24 cleared and, with flag 7, the flags
// reset; or the end with counter 1 = flag 0x3A.
extern "C" void __cdecl Scena02_Scene06(void) {
    switch (Step()) {
    case 0:
        Set40();
        OpenMessage(0x30);
        SetStep(1);
        return;
    case 1: {
        if (MessageUp()) return;
        const unsigned c = Counter(0);
        if (c == 0xBE) SetStep(2);
        else if (c == 0xBF) SetStep(5);
        return;
    }
    case 2:
        Counter(0) = 0;
        SetStep(3);
        ChangeArea(0xF, 0x110000, 0x3E0000, 0x88);
        ScriptOr(0x80);
        SetAreaByte(0x11);
        return;
    case 3:
        ClearFlag(0x24);
        if (Flag(7)) ResetFlags7();
        Clear40();
        SetStep(0);
        SetRun(0);
        return;
    case 5:
        Clear40();
        Counter(0) = 0;
        Counter(2) = 0;
        Counter(3) = 0;
        SetStep(0);
        SetRun(0);
        Counter(1) = Flag(0x3A) ? 1 : 0;
        return;
    default:   // 4, and 6 and up
        return;
    }
}

// original 0x53EBF0: run 7, steps 0..4: member 6 dropped in (counter 2 = 5);
// transition 0xD on counter 0 = 0x21; message 0x16 once the wait word is 0;
// the change to area 0xF with the byte 0x904CD0 = 0x11; the end, flag 0x24
// cleared and, with flag 7, the flags reset.
extern "C" void __cdecl Scena02_Scene07(void) {
    switch (Step()) {
    case 0:
        Counter(2) = 5;
        DropIn(6);
        SetStep(1);
        return;
    case 1:
        if (Counter(0) != 0x21) return;
        Transition(0xD);
        SetStep(2);
        return;
    case 2:
        if (Waiting()) return;
        Draw_PassFlags = 0;
        OpenMessage(0x16);
        SetStep(3);
        return;
    case 3:
        if (MessageUp()) return;
        Counter(2) = 0;
        Counter(0) = 0;
        SetStep(4);
        ChangeArea(0xF, 0x260000, 0x190000, 0x86);
        ScriptOr(0x80);
        SetAreaByte(0x11);
        return;
    case 4:
        Clear40();
        SetStep(0);
        SetRun(0);
        ClearFlag(0x24);
        if (Flag(7)) ResetFlags7();
        return;
    default:
        return;
    }
}

namespace {

// Scena02_Scene08 step 8's three cell tests: the leader on cell x `xa` or
// `xb` with the cell z word (s16) at most `zmax`, +0x4B 0x42 and +8 1, and
// flag `flag` clear: ScriptFlags_Set40, counter 3 = `c3`, the flag set, the
// step `step`. The x word is read once for its two compares.
void Scene08Cell(unsigned flag, unsigned xa, unsigned xb, int zmax, unsigned c3, unsigned step) {
    if (Flag(flag)) return;
    const unsigned x = LeaderCellX();
    if (x != xa && x != xb) return;
    if (LeaderCellZ() > zmax) return;
    if (Leader()[0x4B] != 0x42) return;
    if (Leader()[8] != 1) return;
    Set40();
    Counter(3) = static_cast<unsigned char>(c3);
    SetFlag(flag);
    SetStep(step);
}

}  // namespace

// original 0x53ED70: run 8, steps 0..0xB (a jump table of 12): Kind2 placed,
// the party placed and event battle 0xE; member 0x12 dropped in, flags 7 and
// 0x3E and the change to area 0x1A (0x904CD0 = 0x1F); flag 0xC; message 0x9A
// into run 8 at step 8; step 8, with the first member 0: three cells of the
// leader's (flags 9, 0xA, 0xB, counter 3 = 1, 5, 0xA) and, on counter 2 = 3,
// the end with flags 0x35, 0x3E set and 0x34, 0x2B, 0x2C cleared; steps 9..0xB
// wait on counter 3 = 2, 6, 0xB and tail-jump to Scena02_Scene08Next.
extern "C" void __cdecl Scena02_Scene08(void) {
    switch (Step()) {
    case 0:
        if (Counter(0) != 3) return;
        Kind2(4);
        SetStep(1);
        return;
    case 1:
        if (Counter(0) != 8) return;
        PartyPlace(0x70000, 0xE0000, 0xE);
        SetStep(2);
        return;
    case 2:
        if (Counter(0) != 0xB) return;
        Battle(0xE);
        SetStep(3);
        return;
    case 3:
        if (Counter(0) != 0xC) return;
        Counter(2) = 1;
        DropIn(0x12);
        SetStep(4);
        SetFlag(7);
        SetFlag(0x3E);
        ChangeArea(0x1A, 0x340000, 0x80000, 0x84);
        SetAreaByte(0x1F);
        return;
    case 4:
        if (Counter(0) != 0x31) return;
        SetFlag(0xC);
        ClearFlag(0x2C);
        ClearFlag(0x2B);
        Clear40();
        Counter(1) = 0;
        Counter(2) = 0;
        Counter(3) = 0;
        SetStep(0);
        SetRun(0);
        return;
    case 5:
        if (Counter(0) != 1) return;
        Clear40();
        Counter(1) = 0;
        Counter(2) = 0;
        Counter(3) = 0;
        SetStep(8);
        SetRun(8);
        return;
    case 6:
        OpenMessage(0x9A);
        SetStep(7);
        return;
    case 7:
        if (MessageUp()) return;
        Clear40();
        SetStep(8);
        SetRun(8);
        return;
    case 8:
        if (LeadMember() == 0) {
            Scene08Cell(9, 4, 5, 4, 1, 9);
            Scene08Cell(0xA, 8, 9, 8, 5, 0xA);
            Scene08Cell(0xB, 0xC, 0xD, 4, 0xA, 0xB);
        }
        if (Counter(2) != 3) return;
        Clear40();
        SetFlag(0x35);
        SetFlag(0x3E);
        ClearFlag(0x34);
        ClearFlag(0x2B);
        ClearFlag(0x2C);
        Counter(0) = 0;
        Counter(1) = 0;
        Counter(2) = 0;
        Counter(3) = 0;
        SetStep(0);
        SetRun(0);
        return;
    case 9:
        if (Counter(3) != 2) return;
        SH_CALL(Scena02_Scene08Next)();
        return;
    case 0xA:
        if (Counter(3) != 6) return;
        SH_CALL(Scena02_Scene08Next)();
        return;
    case 0xB:
        if (Counter(3) != 0xB) return;
        SH_CALL(Scena02_Scene08Next)();
        return;
    default:
        return;
    }
}

// original 0x5413D0: the tail of Scena02_Scene08 steps 9..0xB: on counter 2 =
// 3 a tail jump to ScriptFlags_Set40; else ScriptFlags_Clear40, counter 3
// cleared, step 8, counter 2 + 1 (read after the call). The scenario tool
// listed it inside Scena02_Scene1B's extent (a byte table's run to the next
// start, docs/scena_sc2.md section 1.2).
extern "C" void __cdecl Scena02_Scene08Next(void) {
    if (Counter(2) == 3) {
        Set40();
        return;
    }
    Clear40();
    const unsigned char c = Counter(2);
    Counter(3) = 0;
    SetStep(8);
    Counter(2) = static_cast<unsigned char>(c + 1);
}

// original 0x53F120: run 9, steps 0..7 (a jump table of 8): member 0x18 and
// Kind2 5; two kind-0x13 effects at the camera (the second on counter 0 =
// 0xA2, its slot kept in counter 3) and the second's end waited for; member
// 0xA; music 0x20 on counter 0 = 0x63; Kind2 1; call B(0) and the change to
// area 0x1B on 0x7E; flag 0x3F and the end with counter 2 = 1.
extern "C" void __cdecl Scena02_Scene09(void) {
    switch (Step()) {
    case 0:
        MusicStop(0xA);
        DropIn(0x18);
        Kind2(5);
        SetStep(1);
        return;
    case 1:
        if (!TakeEffect()) return;
        SetStep(2);
        AimEffect(Camera_Angles[0], Camera_Angles[1], 0x2C6, 0x10);
        return;
    case 2: {
        if (Counter(0) != 0xA2) return;
        if (!TakeEffect()) return;
        Counter(3) = At(at::kEffectSlot)[0];
        SetStep(3);
        const int x = Camera_Angles[0];
        unsigned char* const e = Effect();
        e[0] = 1;
        e[5] = 0x13;
        SetLong(e + 0x64, x);
        SetLong(e + 0x68, Camera_Angles[1]);
        SetLong(e + 0x6C, 0x1FE);
        e[9] = 0x10;
        return;
    }
    case 3:
        if (Effect_Objects[static_cast<std::uint32_t>(Counter(3)) << 7] != 0) return;
        Counter(3) = 0;
        DropIn(0xA);
        SetStep(4);
        return;
    case 4:
        if (Counter(0) != 0x63) return;
        ScriptOr(8);
        MusicPlay(0x20);
        ScriptOr(0x80);
        SetStep(5);
        return;
    case 5:
        if (Counter(0) != 0x71) return;
        Kind2(1);
        SetStep(6);
        return;
    case 6:
        if (Counter(0) != 0x7E) return;
        ScriptXor(8);
        Counter(2) = 0;
        SH_CALL(Scenario_CallB)(0);
        ScriptXor(0x80);
        SetStep(7);
        ChangeArea(0x1B, 0x220000, 0x260000, 0x80);
        return;
    case 7:
        SetFlag(0x3F);
        Clear40();
        Counter(2) = 1;
        SetStep(0);
        SetRun(0);
        return;
    default:
        return;
    }
}

// original 0x53F340: run 0xA, steps 0..0xA (a jump table of 11; 2 and 4
// nothing): member 3; the change to area 0x1B on counter 0 = 1 (counter 2 =
// 4); flag 0xF; member 4, Kind2 0, the party placed and event battle 8,
// member 0x12, flag 0x10.
extern "C" void __cdecl Scena02_Scene0A(void) {
    switch (Step()) {
    case 0:
        Set40();
        DropIn(3);
        SetStep(1);
        return;
    case 1:
        if (Counter(0) != 1) return;
        ScriptOr(8);
        Counter(2) = 4;
        SetStep(3);
        ChangeArea(0x1B, 0x400000, 0x1B0000, 0x94);
        return;
    case 3:
        if (Counter(0) != 9) return;
        SetFlag(0xF);
        ScriptXor(8);
        ClearThenEnd();
        return;
    case 5:
        DropIn(4);
        SetStep(6);
        return;
    case 6:
        if (Counter(0) != 1) return;
        Kind2(0);
        SetStep(7);
        return;
    case 7:
        if (Counter(0) != 2) return;
        PartyPlace(0x200000, 0x260000, 8);
        SetStep(8);
        return;
    case 8:
        if (Counter(0) != 0x10) return;
        Battle(8);
        SetStep(9);
        return;
    case 9:
        if (Counter(0) != 0x11) return;
        DropIn(0x12);
        SetStep(0xA);
        return;
    case 0xA:
        if (Counter(0) != 0x14) return;
        SetFlag(0x10);
        ClearThenEnd();
        return;
    default:   // 2, 4, and 0xB and up
        return;
    }
}

namespace {

// The counters 1..3 cleared, then ScriptFlags_Clear40, the step and run 0 -
// with counter 0 first when `all` (Scena02_Scene0B's two ends).
void Scene0BEnd(bool all) {
    if (all) Counter(0) = 0;
    Counter(1) = 0;
    Counter(2) = 0;
    Counter(3) = 0;
    Clear40();
    SetStep(0);
    SetRun(0);
}
// Its choice end: counters 0..2 (not 3) cleared, ScriptFlags_Clear40, 0.
void Scene0BChoiceEnd() {
    Counter(0) = 0;
    Counter(1) = 0;
    Counter(2) = 0;
    Clear40();
    SetStep(0);
    SetRun(0);
}
// Steps 2 and 7: the shop on counter 0 = 0xC8, the end on 0xC9.
void Scene0BChoice() {
    if (MessageUp()) return;
    const unsigned c = Counter(0);
    if (c == 0xC8) OpenShop(3);
    else if (c == 0xC9) Scene0BChoiceEnd();
}

}  // namespace

// original 0x53F4D0: run 0xB, steps 0..0x18 (a byte table of 25 into a jump
// table of 11): counter 0 = 0x64 transition 0xD and step 0x14, = 0x65 the end;
// the shop (Game_Mode 7) on the choice 0xC8, the end on 0xC9, both at steps 2
// and 7; the party restored and the stream played; the change to area 5 by
// the member count, music 7.
extern "C" void __cdecl Scena02_Scene0B(void) {
    switch (Step()) {
    case 0:
        if (Counter(0) == 0x64) {
            Counter(0) = 0;
            Transition(0xD);
            SetStep(0x14);
        }
        if (Counter(0) != 0x65) return;
        Scene0BChoiceEnd();
        return;
    case 1:
    case 0x18:
        if (Counter(0) != 1) return;
        Scene0BEnd(false);
        return;
    case 2:
    case 7:
        Scene0BChoice();
        return;
    case 3:
    case 8:
        if (Field_Request != 0) return;
        Scene0BEnd(true);
        return;
    case 0x14:
        if (Waiting()) return;
        Draw_PassFlags = 0;
        PartyRestore();
        SetStep(0x15);
        MusicStop(0x1E);
        return;
    case 0x15:
        LoadStream(0);
        SetStep(0x16);
        return;
    case 0x16:
        if (!StreamDone()) return;
        SetStep(0x17);
        return;
    case 0x17: {
        const unsigned count = Field_MemberCount;
        Counter(2) = 5;
        SetStep(0x18);
        if (count == 3) ChangeArea(5, 0x510000, 0x80000, 0x85);
        else ChangeArea(5, 0x500000, 0x50000, 0x86);
        MusicPlay(7);
        return;
    }
    default:   // 4..6, 9..0x13, and 0x19 and up
        return;
    }
}

// original 0x53F700: run 0xC, steps 0..4 (a jump table of 5): the script flag
// 8, member 0x1A and Kind2 1; member 5 on counter 0 = 0x15; the party placed
// and event battle 9 on 0x1F; member 0x15 on 0x20; flag 0x11 on 0x25.
extern "C" void __cdecl Scena02_Scene0C(void) {
    switch (Step()) {
    case 0:
        ScriptOr(8);
        DropIn(0x1A);
        Kind2(1);
        SetStep(1);
        return;
    case 1:
        if (Counter(0) != 0x15) return;
        DropIn(5);
        SetStep(2);
        return;
    case 2:
        if (Counter(0) != 0x1F) return;
        PartyPlace(0x50000, 0x370000, 9);
        Battle(9);
        SetStep(3);
        return;
    case 3:
        if (Counter(0) != 0x20) return;
        DropIn(0x15);
        SetStep(4);
        return;
    case 4:
        if (Counter(0) != 0x25) return;
        SetFlag(0x11);
        ScriptXor(8);
        ClearThenEnd();
        return;
    default:
        return;
    }
}

// original 0x53F800: run 0xD, steps 0..6 (a jump table of 7): member 6 and
// Kind2 2; the party placed on counter 0 = 0x2B; a kind-0x13 effect at x
// -0x2BA; event battle 0xA on 0x2F; member 0x16 on 0x30; a second effect at
// x -0x2AA; flags 0x12, 0x13 on 0x31.
extern "C" void __cdecl Scena02_Scene0D(void) {
    switch (Step()) {
    case 0:
        DropIn(6);
        Kind2(2);
        SetStep(1);
        return;
    case 1:
        if (Counter(0) != 0x2B) return;
        PartyPlace(0x100000, 0x70000, 0xA);
        SetStep(2);
        return;
    case 2:
        if (!TakeEffect()) return;
        SetStep(3);
        AimEffect(-0x2BA, Camera_Angles[1], 0x260, 0x14);
        return;
    case 3:
        if (Counter(0) != 0x2F) return;
        Battle(0xA);
        SetStep(4);
        return;
    case 4:
        if (Counter(0) != 0x30) return;
        DropIn(0x16);
        SetStep(5);
        return;
    case 5:
        if (!TakeEffect()) return;
        SetStep(6);
        AimEffect(-0x2AA, Camera_Angles[1], 0x200, 0x14);
        return;
    case 6:
        if (Counter(0) != 0x31) return;
        SetFlag(0x12);
        SetFlag(0x13);
        ClearThenEnd();
        return;
    default:
        return;
    }
}

// original 0x53F9C0: run 0xE, steps 0..0xA (a jump table of 11; 9 nothing):
// member 8 or 9 by the first member; Kind2 3; sound 0x204; flag 0x17 and the
// party placed; event battle 0xB; member 0x17 or 0x18; the change to area 0x1B
// (flags 0x8A / 0x8B by the first member, 0x904CD0 = 0x22); Kind2 4; flag
// 0x14 and a second change to area 0x1B; the end.
extern "C" void __cdecl Scena02_Scene0E(void) {
    switch (Step()) {
    case 0:
        DropIn(LeadMember() == 0 ? 8 : 9);
        SetStep(1);
        return;
    case 1:
        if (Counter(0) != 0x3E) return;
        Kind2(3);
        SetStep(2);
        return;
    case 2:
        if (Counter(0) != 0x41) return;
        Sound(0x204);
        SetStep(3);
        return;
    case 3:
        if (Counter(0) != 0x42) return;
        SetFlag(0x17);
        PartyPlace(0x3B0000, 0x488000, 0xB);
        SetStep(4);
        return;
    case 4:
        if (Counter(0) != 0x43) return;
        Battle(0xB);
        SetStep(5);
        return;
    case 5:
        if (Counter(0) != 0x44) return;
        DropIn(LeadMember() == 0 ? 0x17 : 0x18);
        SetStep(6);
        return;
    case 6: {
        if (Counter(0) != 0x46) return;
        const unsigned lead = LeadMember();
        Counter(2) = 1;
        SetStep(7);
        ChangeArea(0x1B, 0x3B0000, 0x1A0000, lead == 0 ? 0x8A : 0x8B);
        SetAreaByte(0x22);
        return;
    }
    case 7:
        Kind2(4);
        SetStep(8);
        return;
    case 8: {
        if (Counter(0) != 0x4B) return;
        SetFlag(0x14);
        const unsigned lead = LeadMember();
        Counter(2) = 1;
        SetStep(0xA);
        ChangeArea(0x1B, 0x3C0000, 0x480000, lead == 0 ? 0x8A : 0x8B);
        return;
    }
    case 0xA:
        Clear40();
        Counter(1) = 0;
        Counter(2) = 0;
        Counter(3) = 0;
        SetStep(0);
        SetRun(0);
        return;
    default:   // 9, and 0xB and up
        return;
    }
}

// original 0x53FC00: run 0xF, steps 0..0xC (a jump table of 13; 6..9
// nothing): counter 0 = 5 transition 0xD (counter 2 = 2), = 6 the end; the
// party restored and the stream played; the change to area 0x1B by the member
// count and the first member, music 0x21; the end on counter 0 = 1; flag 0x18
// and member 0x11; a second change to area 0x1B; on counter 0 = 5 with the
// choice made the party restored, and the end on 6.
extern "C" void __cdecl Scena02_Scene0F(void) {
    switch (Step()) {
    case 0:
        if (Counter(0) == 5) {
            Counter(0) = 0;
            Counter(2) = 2;
            Transition(0xD);
            SetStep(1);
        }
        if (Counter(0) != 6) return;
        Counter(0) = 0;
        Counter(1) = 0;
        Counter(2) = 0;
        Clear40();
        SetStep(0);
        SetRun(0);
        return;
    case 1:
        if (Waiting()) return;
        Draw_PassFlags = 0;
        PartyRestore();
        SetStep(2);
        MusicStop(0x1E);
        return;
    case 2:
        LoadStream(0);
        SetStep(3);
        return;
    case 3:
        if (!StreamDone()) return;
        SetStep(4);
        return;
    case 4: {
        unsigned flags = 0x8E;
        if (Field_MemberCount == 3) {
            SetStep(5);
        } else {
            const unsigned lead = LeadMember();
            SetStep(5);
            flags = lead == 0 ? 0x8C : 0x8D;
        }
        ChangeArea(0x1B, 0x5E0000, 0x490000, flags);
        MusicPlay(0x21);
        return;
    }
    case 5:
        if (Counter(0) != 1) return;
        Counter(2) = 0;
        Clear40();
        SetStep(0);
        SetRun(0);
        return;
    case 0xA:
        Set40();
        ScriptOr(8);
        SetFlag(0x18);
        DropIn(0x11);
        SetStep(0xB);
        return;
    case 0xB:
        if (Counter(0) != 1) return;
        Counter(2) = 4;
        SetStep(0xC);
        ChangeArea(0x1B, 0x5C0000, 0x480000, 0x93);
        return;
    case 0xC:
        if (Counter(0) == 5) {
            if (!Chosen()) return;
            MusicStop(0x1E);
            PartyRestore();
            ScriptXor(8);
            SetStep(0);
        }
        if (Counter(0) != 6) return;
        Counter(1) = 0;
        Counter(2) = 0;
        Clear40();
        ScriptXor(8);
        SetStep(0);
        SetRun(0);
        return;
    default:   // 6..9, and 0xD and up
        return;
    }
}

// original 0x53FE30: run 0x10, steps 0..0x16 (a byte table of 23 into a jump
// table of 14): the change to area 0x1C; transition 1; Kind2 0; a second
// change; flag 0x15 and the end; member 2 (while the second member's +0x137 is
// not 7); call A(0), flag 1 and a third change; the script flag 8 and Kind2
// 1; two turns of the camera tested through 0x57C600 (0x5A, 2) and (0x28, -2)
// with six frames between; the last change to area 0x1C and the end.
extern "C" void __cdecl Scena02_Scene10(void) {
    switch (Step()) {
    case 0:
        Counter(2) = 1;
        SetStep(1);
        ChangeArea(0x1C, 0x380000, 0x70000, 0x80);
        return;
    case 1:
        if (Counter(0) != 1) return;
        Transition(1);
        Draw_PassFlags = 0x1F;
        SetStep(2);
        return;
    case 2:
        if (Counter(0) != 6) return;
        Kind2(0);
        SetStep(6);
        return;
    case 6:
        if (Counter(0) != 7) return;
        Counter(2) = 1;
        SetStep(7);
        ChangeArea(0x1C, 0x370000, 0x90000, 0x81);
        return;
    case 7:
        if (Counter(0) != 9) return;
        SetFlag(0x15);
        Clear40();
        SetStep(0);
        SetRun(0);
        return;
    case 0xF:
        if (At(at::kMemberSeven)[0] == 7) return;
        DropIn(2);
        SetStep(0x10);
        return;
    case 0x10:
        if (Counter(0) != 2) return;
        Counter(2) = 1;
        SH_CALL(Scenario_CallA)(0);
        SetStep(0x11);
        SetFlag(1);
        ChangeArea(0x1C, 0x280000, 0x70000, 0x83);
        return;
    case 0x11:
        if (Counter(0) != 0x12) return;
        ScriptOr(8);
        Kind2(1);
        SetStep(0x12);
        return;
    case 0x12:
        if (Counter(0) != 0x17) return;
        if (TurnTest(0x5A, 2)) return;
        SetStep(0x13);
        Counter(3) = 0;   // the original stores al, 0 here (the test's answer)
        return;
    case 0x13: {
        const unsigned char n = static_cast<unsigned char>(Counter(3) + 1);
        Counter(3) = n;
        if (n != 6) return;
        Counter(3) = 0;
        Counter(0) = 0x18;
        SetStep(0x14);
        return;
    }
    case 0x14:
        if (Counter(0) != 0x18) return;
        if (TurnTest(0x28, -2)) return;
        SetStep(0x15);
        return;
    case 0x15:
        if (Counter(0) != 0x1A) return;
        ScriptXor(8);
        Counter(2) = 2;
        SetStep(0x16);
        ChangeArea(0x1C, 0x270000, 0x90000, 0x84);
        return;
    case 0x16:
        if (Counter(0) != 7) return;
        Counter(2) = 0;
        Clear40();
        SetStep(0);
        SetRun(0);
        return;
    default:   // 3..5, 8..0xE, and 0x17 and up
        return;
    }
}

// original 0x540100: run 0x11, steps 0..0x1D (a jump table of 30; 6..0xE
// nothing): music 0x25 and message 0x14; Kind2 2; member 5; the script flag
// 8; the stream 1 and sound 0x203, the camera shaken (Scena02_Shake(0xA))
// until counter 0 = 0x14, sound 0x204; sound 0x201 and transition 0xF; the
// party placed and event battle 0xC; member 6; transition 0xD; message 0x29,
// flag 0x36 and the change to area 0 (0x904CD0 = 1); Kind2 1; message 0x38,
// Field_StatusBits bit 0 flipped and the change to area 5; the end.
extern "C" void __cdecl Scena02_Scene11(void) {
    switch (Step()) {
    case 0:
        MusicStop(0xA);
        MusicPlay(0x25);
        OpenMessage(0x14);
        SetStep(1);
        return;
    case 1:
        if (MessageUp()) return;
        Kind2(2);
        SetStep(2);
        return;
    case 2:
        if (Counter(0) != 1) return;
        DropIn(5);
        SetStep(3);
        return;
    case 3:
        if (Counter(0) != 6) return;
        MusicStop(0xA);
        ScriptOr(8);
        SetStep(4);
        return;
    case 4:
        if (Counter(0) != 0xE) return;
        LoadStream(1);
        Sound(0x203);
        Counter(1) = 0;
        Counter(3) = 0;
        SetStep(5);
        return;
    case 5:
        SH_CALL(Scena02_Shake)(0xA);
        if (Counter(0) != 0x14) return;
        Camera_ShiftY = 0;
        MapView_Redraw = 2;
        Counter(1) = 0;
        Counter(3) = 0;
        Counter(0) = 0x15;
        SetStep(0xF);
        Sound(0x204);
        return;
    case 0xF:
        if (Counter(0) != 0x19) return;
        Sound(0x201);
        Transition(0xF);
        SetStep(0x10);
        return;
    case 0x10:
        if (Waiting()) return;
        Draw_PassFlags = 0;
        Counter(0) = 0x1A;
        SetStep(0x11);
        return;
    case 0x11:
        if (Counter(0) != 0x1B) return;
        Draw_PassFlags = 0x1F;
        Transition(0x10);
        SetStep(0x12);
        PartyPlace(0x1C0000, 0x5B0000, 0xC);
        Counter(0) = 0x1C;
        return;
    case 0x12:
        if (Counter(0) != 0x1D) return;
        Battle(0xC);
        SetStep(0x13);
        return;
    case 0x13:
        if (Counter(0) != 0x23) return;
        MusicStop(0xA);
        DropIn(6);
        SetStep(0x14);
        return;
    case 0x14:
        if (Counter(0) != 0x2B) return;
        Transition(0xD);
        SetStep(0x15);
        return;
    case 0x15:
        if (Waiting()) return;
        Draw_PassFlags = 0;
        SetStep(0x16);
        return;
    case 0x16:
        OpenMessage(0x29);
        SetStep(0x17);
        return;
    case 0x17:
        if (MessageUp()) return;
        SetStep(0x18);
        SetFlag(0x36);
        ChangeArea(0, 0x200000, 0x160000, 0x84);
        SetAreaByte(1);
        return;
    case 0x18:
        Kind2(1);
        SetStep(0x19);
        return;
    case 0x19:
        if (Counter(0) != 1) return;
        Transition(0xD);
        SetStep(0x1A);
        return;
    case 0x1A:
        if (Waiting()) return;
        Draw_PassFlags = 0;
        SetStep(0x1B);
        return;
    case 0x1B:
        OpenMessage(0x38);
        SetStep(0x1C);
        return;
    case 0x1C:
        if (MessageUp()) return;
        SetStep(0x1D);
        Counter(2) = 6;
        Field_StatusBits = static_cast<unsigned char>(Field_StatusBits ^ 1);
        ChangeArea(5, 0x530000, 0x230000, 0x87);
        return;
    case 0x1D:
        if (Counter(0) != 6) return;
        PartyRestore();
        ScriptXor(8);
        EndKeep0();
        return;
    default:   // 6..0xE, and 0x1E and up
        return;
    }
}

// original 0x540510: run 0x12, steps 0..7 (a jump table of 8; 3 and 4
// nothing): flag 0x1A and member 2; the end; the change to area 0xD; flags
// 0x1C, 0x38, 0x1B; with Cond_ByteFD 0 music 0x10 and the end.
extern "C" void __cdecl Scena02_Scene12(void) {
    switch (Step()) {
    case 0:
        Counter(0) = 1;
        SetStep(1);
        return;
    case 1:
        if (Counter(0) != 2) return;
        SetFlag(0x1A);
        DropIn(2);
        SetStep(2);
        return;
    case 2:
        if (Counter(0) != 4) return;
        Counter(1) = 0;
        Counter(3) = 0;
        Clear40();
        SetStep(0);
        SetRun(0);
        return;
    case 5:
        ScriptOr(8);
        SetStep(6);
        MusicStop(0xA);
        Counter(2) = 1;
        ChangeArea(0xD, 0x60000, 0x540000, 0x83);
        return;
    case 6:
        if (Counter(0) != 0x11) return;
        SetFlag(0x1C);
        SetFlag(0x38);
        SetFlag(0x1B);
        ScriptXor(8);
        SetStep(7);
        Counter(1) = 0;
        Counter(2) = 0;
        Counter(3) = 0;
        Clear40();
        return;
    case 7:
        if (Cond_ByteFD != 0) return;
        MusicPlay(0x10);
        Counter(0) = 0;
        SetStep(0);
        SetRun(0);
        return;
    default:   // 3, 4, and 8 and up
        return;
    }
}

// original 0x540680: run 0x13, steps 0xA..0x11 (a jump table of 8 on the step
// - 0xA): flag 0x1D and the change to area 0xB; transition 5; Kind2 0 and a
// kind-0x13 effect at x -0x4D0; a second at -0x2AA; the script flag 8 and
// member 1; music 0xD and the change to area 0xB (0x904CD0 = 0xD); the party
// placed and event battle 0xD; flag 0x1E and the change to area 3 (0x904CD0 =
// 0xFF).
extern "C" void __cdecl Scena02_Scene13(void) {
    switch (Step()) {
    case 0xA:
        SetStep(0xB);
        SetFlag(0x1D);
        ChangeArea(0xB, 0x200000, 0x110000, 0x80);
        return;
    case 0xB:
        Transition(5);
        Draw_PassFlags = 0x1F;
        SetStep(0xC);
        return;
    case 0xC:
        if (Waiting()) return;
        Counter(0) = 1;
        Kind2(0);
        if (!TakeEffect()) return;
        SetStep(0xD);
        AimEffect(-0x4D0, Camera_Angles[1], 0x2B0, 0xB4);
        return;
    case 0xD:
        if (Counter(0) != 2) return;
        if (!TakeEffect()) return;
        SetStep(0xE);
        AimEffect(-0x2AA, Camera_Angles[1], 0x200, 0x40);
        return;
    case 0xE:
        if (Counter(0) != 3) return;
        ScriptOr(8);
        DropIn(1);
        SetStep(0xF);
        return;
    case 0xF:
        if (Counter(0) != 1) return;
        MusicStop(0xA);
        MusicPlay(0xD);
        SetStep(0x10);
        Counter(0) = 0x1E;
        ChangeArea(0xB, 0x220000, 0x110000, 0x82);
        SetAreaByte(0xD);
        return;
    case 0x10:
        if (Counter(0) != 0x27) return;
        PartyPlace(0x1E0000, 0x110000, 0xD);
        Battle(0xD);
        SetStep(0x11);
        return;
    case 0x11:
        if (Counter(0) != 0x3C) return;
        SetStep(0);
        SetRun(0);
        Counter(2) = 0;
        SetFlag(0x1E);
        ChangeArea(3, 0x4F0000, 0x1A0000, 0x80);
        SetAreaByte(0xFF);
        return;
    default:   // 0..9, and 0x12 and up
        return;
    }
}

// original 0x5408D0: run 0x15, steps 0..7 (a jump table of 8; 2..4 nothing):
// music 0xF and Kind2 0; on Kind2 settled and counter 0 = 3 the party
// restored, flag 0x1F and the end; transition 5; flag 0x20 and the end with
// Field_StatusBits bit 7 (0x56D6F0).
extern "C" void __cdecl Scena02_Scene15(void) {
    switch (Step()) {
    case 0:
        Draw_PassFlags = 0x1F;
        MusicPlay(0xF);
        Kind2(0);
        SetStep(1);
        return;
    case 1:
        if (Field_Kind2Hold != 0) return;
        if (Counter(0) != 3) return;
        PartyRestore();
        ScriptXor(8);
        SetFlag(0x1F);
        ScriptOr(0x80);
        Counter(0) = 0;
        Counter(1) = 0;
        Counter(3) = 0;
        Clear40();
        SetStep(0);
        SetRun(0);
        return;
    case 5:
        if (Counter(0) != 1) return;
        Transition(5);
        Draw_PassFlags = 0x1F;
        SetStep(6);
        return;
    case 6:
        if (Waiting()) return;
        Counter(0) = 2;
        SetStep(7);
        return;
    case 7:
        if (Counter(0) != 0xF) return;
        SetFlag(0x20);
        ScriptXor(0x80);
        EndAll();
        SH_AT(cl::VoidFn, cl::kStatusBit80)();
        return;
    default:   // 2..4, and 8 and up
        return;
    }
}

// original 0x540A30: run 0x17, one step: 0x591BE0(0x32, 0) (the party's money
// raised), flag 0x23, sound 0x106, the end.
extern "C" void __cdecl Scena02_Scene17(void) {
    if (Step() != 0) return;
    SH_AT(cl::MoneyGiveFn, cl::kMoneyGive)(0x32, 0);
    SetFlag(0x23);
    Sound(0x106);
    EndPlain();
}

namespace {

// Scena02_Scene18's payment: 0x591BC0(0x32, 0) answering al; paid: counter 0
// = 9, the others cleared, the end, flags 0x24, 0x25 and sound 0x106; not:
// message 0x6D and the step `refused`.
void Scene18Pay(unsigned refused) {
    if (SH_AT(cl::MoneyTakeFn, cl::kMoneyTake)(0x32, 0) != 0) {
        Counter(0) = 9;
        Counter(1) = 0;
        Counter(2) = 0;
        Counter(3) = 0;
        Clear40();
        SetStep(0);
        SetRun(0);
        SetFlag(0x24);
        SetFlag(0x25);
        Sound(0x106);
        return;
    }
    OpenMessage(0x6D);
    SetStep(refused);
}
// Its answer other than 8: 0xF message 0x6C and step 4; any other nothing.
void Scene18Other(unsigned c) {
    if (c != 0xF) return;
    OpenMessage(0x6C);
    SetStep(4);
}
// Its refusal's end: counter 0 = 0x10, counter 1 = 1, flag 0x3A, the end.
void Scene18Refused() {
    if (MessageUp()) return;
    Counter(0) = 0x10;
    Counter(1) = 1;
    Counter(2) = 0;
    Counter(3) = 0;
    SetFlag(0x3A);
    EndPlain();
}

}  // namespace

// original 0x540A70: run 0x18, steps 0..0x14 (a byte table of 21 into a jump
// table of 9): message 0x65; member 0xC, 0x1C or 0x1B by the first member;
// the payment of 0x32 (0x591BC0) on counter 0 = 8, message 0x6D when it
// fails, message 0x6C on 0xF; the refusal's end with flag 0x3A.
extern "C" void __cdecl Scena02_Scene18(void) {
    switch (Step()) {
    case 0:
        OpenMessage(0x65);
        SetStep(1);
        return;
    case 1:
        if (MessageUp()) return;
        Counter(0) = 1;
        switch (LeadMember()) {
        case 0: DropIn(0xC); break;
        case 3: DropIn(0x1C); break;
        case 4: DropIn(0x1B); break;
        default: break;
        }
        SetStep(2);
        return;
    case 2: {
        const unsigned c = Counter(0);
        if (c == 8) Scene18Pay(3);
        else Scene18Other(c);
        return;
    }
    case 3:
    case 4:
    case 0xD:
        Scene18Refused();
        return;
    case 0xA:
        Counter(0) = 1;
        SetStep(0xC);
        return;
    case 0xC: {
        const unsigned c = Counter(0);
        if (c == 8) Scene18Pay(0xD);
        else Scene18Other(c);
        return;
    }
    case 0x14: {
        if (MessageUp()) return;
        const unsigned c = Counter(0);
        if (c == 8) Scene18Pay(0xD);
        else Scene18Other(c);
        return;
    }
    default:   // 5..9, 0xB, 0xE..0x13, and 0x15 and up
        return;
    }
}

// original 0x540D50: run 0x19, steps 0..0x11 (a byte table of 18 into a jump
// table of 7): flag 0x27 and 0x591B60(4, 2, 1, 0) (an item taken); the end on
// counter 0 = 0x16; flag 0x26 and Inventory_Add(4, 2, 1); message 0x73; the
// kind-6 effect 0x91 at the leader; the end on 0x1A.
extern "C" void __cdecl Scena02_Scene19(void) {
    switch (Step()) {
    case 0:
        Counter(0) = 0x15;
        SetFlag(0x27);
        SH_AT(cl::InventoryTakeFn, cl::kInventoryTake)(4, 2, 1, 0);
        SetStep(1);
        return;
    case 1:
        if (Counter(0) != 0x16) return;
        EndAll();
        return;
    case 0xA:
        Counter(0) = 0x14;
        SetFlag(0x26);
        SH_CALL(Inventory_Add)(4, 2, 1);
        ClearThenEnd();
        return;
    case 0xF:
        OpenMessage(0x73);
        SetStep(0x10);
        return;
    case 0x10:
        if (MessageUp()) return;
        SH_CALL(Scena02_PlaceEffect70)(0x91);
        SetStep(0x11);
        Counter(0) = 0x19;
        return;
    case 0x11:
        if (Counter(0) != 0x1A) return;
        EndAll();
        return;
    default:   // 2..9, 0xB..0xE, and 0x12 and up
        return;
    }
}

// original 0x540EA0: run 0x1A, steps 0..0x1E (a byte table of 31 into a jump
// table of 15, and a second jump table of 8 on the leader's +8): sound 0x20B,
// counter 3 counted to 0x4B, Kind2 2 and member 0xF; the end on counter 0 =
// 0x27 with Kind2 settled; member 0x10, 0x1A or 0x19 by the first member; the
// kind-6 effect 0x92; flag 0x2F; message 0x7F; the kind-0x19 effect 0x99,
// member 0x11 and Kind2 3; the party placed and event battle 0xF; flag 0x30;
// a member by the leader's +8; flag 0x32; sixteen frames and the end.
extern "C" void __cdecl Scena02_Scene1A(void) {
    switch (Step()) {
    case 0:
        Sound(0x20B);
        SetStep(1);
        return;
    case 1:
        if (Counter(3) == 0x4B) {
            Kind2(2);
            DropIn(0xF);
            SetStep(2);
        }
        Counter(3) = static_cast<unsigned char>(Counter(3) + 1);
        return;
    case 2:
        if (Counter(0) != 0x27) return;
        if (Field_Kind2Hold != 0) return;
        EndKeep0();
        return;
    case 5:
        Counter(0) = 0x37;
        switch (LeadMember()) {
        case 0: DropIn(0x10); break;
        case 3: DropIn(0x1A); break;
        case 4: DropIn(0x19); break;
        default: break;
        }
        SetStep(6);
        return;
    case 6:
        if (Counter(0) != 0x39) return;
        SH_CALL(Scena02_PlaceEffect70)(0x92);
        SetStep(7);
        Counter(0) = 0x3A;
        return;
    case 7:
        if (Counter(0) != 0x3C) return;
        SetFlag(0x2F);
        EndKeep0();
        return;
    case 0xB:
        if (MessageUp()) return;
        EndPlain();
        return;
    case 0xF:
        OpenMessage(0x7F);
        SetStep(0xB);
        return;
    case 0x14:
        SH_CALL(Scena02_PlaceEffect68)(0x99);
        MusicStop(0xA);
        Counter(0) = 0x41;
        DropIn(0x11);
        Kind2(3);
        SetStep(0x15);
        return;
    case 0x15:
        if (Counter(0) != 0x47) return;
        PartyPlace(0x400000, 0x278000, 0xF);
        Battle(0xF);
        SetStep(0x18);
        return;
    case 0x18:
        if (Counter(0) != 0x4B) return;
        SetFlag(0x30);
        EndKeep0();
        return;
    case 0x19: {
        unsigned entry;
        switch (Leader()[8]) {
        case 0: entry = 0x16; break;
        case 1: entry = 0x14; break;
        case 2: entry = 0x17; break;
        case 3: entry = 0x13; break;
        case 7: entry = 0x15; break;
        default: return;   // 4..6, and 8 and up
        }
        DropIn(entry);
        SetStep(0x1A);
        return;
    }
    case 0x1A:
        if (Counter(0) != 0x51) return;
        SetFlag(0x32);
        EndKeep0();
        return;
    case 0x1E: {
        const unsigned char n = static_cast<unsigned char>(Counter(3) + 1);
        Counter(3) = n;
        if (n < 0x10) return;
        Counter(3) = 0;
        SetStep(0);
        SetRun(0);
        return;
    }
    default:   // 3, 4, 8..0xA, 0xC..0xE, 0x10..0x13, 0x16, 0x17, 0x1B..0x1D, and 0x1F and up
        return;
    }
}

// original 0x5411E0: run 0x1B, steps 0..0x19 (a byte table of 26 into a jump
// table of 11): message 0x6F and the end; with the choice made, counter 0 =
// 0x5A the end, = 0x5B transition 0xD (counter 2 = 1); the shop on 0x5C, the
// end on 0x5D; the party restored, the music stopped (0x587B80), the stream
// played and the sound resumed; the change to area 0x1B; the shop or the end
// by the choice again; the end once the request byte is 0.
extern "C" void __cdecl Scena02_Scene1B(void) {
    switch (Step()) {
    case 0:
        OpenMessage(0x6F);
        SetStep(1);
        return;
    case 1:
        if (MessageUp()) return;
        EndAll();
        return;
    case 5: {
        if (!Chosen()) return;
        const unsigned c = Counter(0);
        if (c == 0x5A) {
            EndAll();
        } else if (c == 0x5B) {
            Counter(2) = 1;
            SetStep(0x14);
            Transition(0xD);
        }
        return;
    }
    case 0xA: {
        if (MessageUp()) return;
        const unsigned c = Counter(0);
        if (c == 0x5C) OpenShop(0x19);
        else if (c == 0x5D) EndAll();
        return;
    }
    case 0x14:
        if (Waiting()) return;
        Draw_PassFlags = 0;
        PartyRestore();
        SetStep(0x15);
        SH_AT(cl::VoidFn, cl::kMusicStop)();
        return;
    case 0x15:
        LoadStream(0);
        SetStep(0x16);
        return;
    case 0x16:
        if (!StreamDone()) return;
        SH_CALL(Sound_ResumeAll)();
        SetStep(0x17);
        return;
    case 0x17:
        Counter(2) = 2;
        SetStep(0x18);
        ChangeArea(0x1B, 0x490000, 0xA0000, 0x99);
        return;
    case 0x18: {
        if (!Chosen()) return;
        const unsigned c = Counter(0);
        if (c == 0x5C) OpenShop(0x19);
        else if (c == 0x5D) EndKeep0();
        return;
    }
    case 0x19:
        if (Field_Request != 0) return;
        EndKeep0();
        return;
    default:   // 2..4, 6..9, 0xB..0x13, and 0x1A and up
        return;
    }
}

namespace {

// The near helpers' test: with Cond_ByteFD 0, the leader's x (+0x34) and z
// (+0x38) inside a rectangle around sprite n's (Sprite_Objects record n,
// +0x34 / +0x38), every bound inclusive and a signed compare of a wrapping
// 32-bit sum as the original's lea / add makes it; inside, a tail jump to
// Scena02_StartRun07.
void Near(unsigned char n, int xlo, int xhi, int zlo, int zhi) {
    if (Cond_ByteFD != 0) return;
    const unsigned char* const s = Sprite_Objects + 0xA4u * n;
    const int sx = Long(s + 0x34), sz = Long(s + 0x38);
    const int lx = Long(Leader() + 0x34), lz = Long(Leader() + 0x38);
    if (lz > Plus(sz, zhi) || lz < Plus(sz, zlo)) return;
    if (lx < Plus(sx, xlo) || lx > Plus(sx, xhi)) return;
    SH_CALL(Scena02_StartRun07)();
}

}  // namespace

// original 0x541400..0x541840: twelve near helpers, each (n) - the sprite
// record n - with its own rectangle of the leader's (x, z) around the
// sprite's (x in [sx + a, sx + b], z in [sz + c, sz + d], 16.16), called by
// Scena02_Scene00 on sprites 0..9; Cond_ByteFD must be 0. Inside, a tail jump
// to Scena02_StartRun07 (the arguments left in place; it reads none).
extern "C" void __cdecl Scena02_Near01(unsigned char n) { Near(n, 0, 0x50000, -0x18000, 0x18000); }
extern "C" void __cdecl Scena02_Near02(unsigned char n) { Near(n, -0x40000, 0, -0x18000, 0x18000); }
extern "C" void __cdecl Scena02_Near03(unsigned char n) { Near(n, -0x18000, 0x18000, 0x10000, 0x40000); }
extern "C" void __cdecl Scena02_Near04(unsigned char n) { Near(n, -0x18000, 0x18000, -0x40000, 0x10000); }
extern "C" void __cdecl Scena02_Near05(unsigned char n) { Near(n, 0, 0x60000, -0x20000, 0x20000); }
extern "C" void __cdecl Scena02_Near06(unsigned char n) { Near(n, -0x10000, 0x10000, -0x50000, 0); }
extern "C" void __cdecl Scena02_Near07(unsigned char n) { Near(n, -0x48000, 0, -0x18000, 0x18000); }
extern "C" void __cdecl Scena02_Near08(unsigned char n) { Near(n, 0, 0x40000, -0x10000, 0x10000); }
extern "C" void __cdecl Scena02_Near09(unsigned char n) { Near(n, 0, 0x40000, -0x20000, 0x28000); }
extern "C" void __cdecl Scena02_Near0A(unsigned char n) { Near(n, 0, 0x30000, -0x10000, 0x18000); }
extern "C" void __cdecl Scena02_Near0B(unsigned char n) { Near(n, -0x10000, 0x10000, 0, 0x50000); }
extern "C" void __cdecl Scena02_Near0C(unsigned char n) { Near(n, -0x18000, 0x18000, 0, 0x40000); }

namespace {

// The door helpers' cell test: the leader's cell z word, +0x4B and +8.
bool AtDoor(unsigned z, unsigned b4b, unsigned b8) {
    return Word(Leader() + 0x3A) == z && Leader()[0x4B] == b4b && Leader()[8] == b8;
}
// Scena02_DoorStart's start: ScriptFlags_Set40, counter 0 = 0x23, the others
// cleared, run 0x1A at step 0.
void DoorStart() {
    Counter(0) = 0x23;
    Counter(1) = 0;
    Counter(2) = 0;
    Counter(3) = 0;
    SetStep(0);
    SetRun(0x1A);
}

}  // namespace

// original 0x5418A0: tail-jumped by Scena02_Scene00 (flag 0x2E clear). With
// the first member 0 or 3: the leader on cell x 0x44 / 0x45 at z 0x3F (+0x4B
// 0x42, +8 1), on 0x46 at 0x3E (0x45, 7) or on 0x44 at 0x3E (0x43, 3):
// ScriptFlags_Set40 and run 0x1A at step 0 with counter 0 = 0x23. The x word
// is read again after each ScriptFlags_Set40, as the original does; a leader
// on x 0x46 failing its other tests ends the whole test.
extern "C" void __cdecl Scena02_DoorStart(void) {
    const unsigned lead = LeadMember();
    if (lead != 0 && lead != 3) return;
    unsigned x = LeaderCellX();
    if ((x == 0x45 || x == 0x44) && AtDoor(0x3F, 0x42, 1)) {
        Set40();
        x = LeaderCellX();
        DoorStart();
    }
    if (x == 0x46) {
        if (!AtDoor(0x3E, 0x45, 7)) return;
        Set40();
        x = LeaderCellX();
        DoorStart();
    }
    if (x == 0x44 && AtDoor(0x3E, 0x43, 3)) {
        Set40();
        DoorStart();
    }
}

// original 0x5419C0: tail-jumped by Scena02_Scene00 (flag 0x2E set): the same
// three cells, each playing sound 0x20B and setting run 0x1A at step 0x1E.
extern "C" void __cdecl Scena02_DoorSound(void) {
    const unsigned lead = LeadMember();
    if (lead != 0 && lead != 3) return;
    unsigned x = LeaderCellX();
    if ((x == 0x45 || x == 0x44) && AtDoor(0x3F, 0x42, 1)) {
        Sound(0x20B);
        x = LeaderCellX();
        SetStep(0x1E);
        SetRun(0x1A);
    }
    if (x == 0x46) {
        if (!AtDoor(0x3E, 0x45, 7)) return;
        Sound(0x20B);
        x = LeaderCellX();
        SetStep(0x1E);
        SetRun(0x1A);
    }
    if (x == 0x44 && AtDoor(0x3E, 0x43, 3)) {
        Sound(0x20B);
        SetStep(0x1E);
        SetRun(0x1A);
    }
}

// original 0x541AB0: the near helpers' shared tail (reached only by their
// tail jumps - the start the chapter walk did not reach): counter 0 = 0x1F,
// the others cleared, run 7 at step 0.
extern "C" void __cdecl Scena02_StartRun07(void) {
    Counter(0) = 0x1F;
    Counter(1) = 0;
    Counter(2) = 0;
    Counter(3) = 0;
    SetStep(0);
    SetRun(7);
}

// original 0x541AE0: the camera shaken by n, called once a frame by
// Scena02_Scene11 step 5 with 0xA: by counter 1 (the phase), Camera_ShiftY =
// n + Camera_ShiftX (16 bits), the view redrawn, counter 3 counted; on its
// second frame both shifts back to 0 and the phase flipped (0 -> 1 -> 0).
extern "C" void __cdecl Scena02_Shake(unsigned char n) {
    switch (Counter(1)) {
    case 1: {
        const short v = static_cast<short>(n + Camera_ShiftX);
        MapView_Redraw = 2;
        Camera_ShiftY = v;
        const unsigned char c = static_cast<unsigned char>(Counter(3) + 1);
        Counter(3) = c;
        if (c != 2) return;
        Camera_ShiftY = 0;
        Camera_ShiftX = 0;
        MapView_Redraw = 2;
        Counter(3) = 0;
        Counter(1) = 0;
        return;
    }
    case 0: {
        const short v = static_cast<short>(n + Camera_ShiftX);
        const unsigned char c = static_cast<unsigned char>(Counter(3) + 1);
        Camera_ShiftY = v;
        MapView_Redraw = 2;
        Counter(3) = c;
        if (c != 2) return;
        Camera_ShiftY = 0;
        Camera_ShiftX = 0;
        MapView_Redraw = 2;
        Counter(1) = 1;
        Counter(3) = 0;
        return;
    }
    default:
        return;
    }
}

namespace {

// The two place helpers' body: Sprite_Current = the leader; an effect slot
// into its +0xB; when there is one, its record made live as `kind` with +6 =
// `sub`, +0xC = 0, +0x10 = the s8 at 0x660E48 by the first member, and the
// leader's words +0x2E / +0x30. As the original has it: Sprite_Current is
// read back after Effect_FindFree (the store goes where it then points), and
// the slot is read back from +0xB for each field; the s8 table is read
// unbounded (past its 8 bytes lies Scena02_Hooks, readable .data, no fault).
void PlaceEffect(unsigned char kind, unsigned char sub) {
    Sprite_Current = Leader();
    const unsigned char slot = SH_CALL(Effect_FindFree)();
    Sprite_Current[0xB] = slot;
    unsigned char* const cur = Sprite_Current;
    if (cur[0xB] == 0xFF) return;
    auto rec = [cur] { return Effect_Objects + (static_cast<std::uint32_t>(cur[0xB]) << 7); };
    rec()[0] = 1;
    rec()[5] = kind;
    rec()[6] = sub;
    SetLong(rec() + 0xC, 0);
    SetLong(rec() + 0x10, static_cast<signed char>(At(at::kEffectX + LeadMember())[0]));
    SetWord(rec() + 0x2E, Word(cur + 0x2E));
    SetWord(rec() + 0x30, Word(cur + 0x30));
}

}  // namespace

// original 0x541B90: a kind-6 effect at the leader with +6 = n + 0x70
// (Scena02_Scene19 0x91, Scena02_Scene1A 0x92). Chapter 1's
// Scena01_PlaceEffect compiled again with its own s8 table.
extern "C" void __cdecl Scena02_PlaceEffect70(unsigned char n) { PlaceEffect(6, static_cast<unsigned char>(n + 0x70)); }

// original 0x541C50: the same as a kind-0x19 effect with +6 = n + 0x68
// (Scena02_Scene1A, 0x99).
extern "C" void __cdecl Scena02_PlaceEffect68(unsigned char n) { PlaceEffect(0x19, static_cast<unsigned char>(n + 0x68)); }

// original 0x541D10: slot 1 of chapter 2's vtable, the object hook (0x56D6D0
// calls it with the object whose trigger fired): the handler
// Scena02_ObjectHandlers[object +0x86] called with the object and chapter 2's
// row 0x903FA0. The table holds 27 (entries 0, 3 and 0x12 are 0x437CC0, a
// bare ret; entry 0xA is 0x557170 in group SC9b's block); the original
// indexes it by any byte, ours aborts past it. 0x56D6D0 reads no answer.
extern "C" void __cdecl Scena02_ObjectHook(unsigned char* object) {
    const unsigned k = object[0x86];
    if (k >= at::kObjectCount)
        bof3::Fatal("Scena02_ObjectHook: object +0x86 %u past Scena02_ObjectHandlers (%u entries)", k, at::kObjectCount);
    using ObjectFn = void (__cdecl*)(unsigned char*, unsigned char*);
    reinterpret_cast<ObjectFn>(static_cast<std::uintptr_t>(Entry(at::kObjects, k)))(object, At(at::kRow2));
}

// original 0x541D30..0x542060: the object handlers (object, row), neither
// read. Most set ScriptFlags_Set40 and a run at a step with counters 1 and 2
// cleared; the rest set a counter or a flag.
extern "C" void __cdecl Scena02_Object01(unsigned char*, unsigned char*) {
    Set40();
    SetStep(7);
    Counter(1) = 0;
    Counter(2) = 0;
    SetRun(0xB);
}
extern "C" void __cdecl Scena02_Object02(unsigned char*, unsigned char*) {
    Set40();
    SetStep(2);
    Counter(1) = 0;
    Counter(2) = 0;
    SetRun(0xB);
}
// Entries 4 and 5: with flag 5 clear, the script flags ^ 0x36 and run 4 at
// step 5 or 7.
extern "C" void __cdecl Scena02_Object04(unsigned char*, unsigned char*) {
    if (Flag(5)) return;
    ScriptXor(0x36);
    Set40();
    SetStep(5);
    Counter(1) = 0;
    Counter(2) = 0;
    SetRun(4);
}
extern "C" void __cdecl Scena02_Object05(unsigned char*, unsigned char*) {
    if (Flag(5)) return;
    ScriptXor(0x36);
    Set40();
    SetStep(7);
    Counter(1) = 0;
    Counter(2) = 0;
    SetRun(4);
}
// Entry 7: counter 2 = 1. Entries 6 and 8: counter 1 = 1. Entry 9: flag 0xD.
extern "C" void __cdecl Scena02_Object07(unsigned char*, unsigned char*) { Counter(2) = 1; }
extern "C" void __cdecl Scena02_Object06(unsigned char*, unsigned char*) { Counter(1) = 1; }
extern "C" void __cdecl Scena02_Object09(unsigned char*, unsigned char*) { SetFlag(0xD); }
extern "C" void __cdecl Scena02_Object0B(unsigned char*, unsigned char*) {
    Set40();
    Counter(0) = 1;
    Counter(1) = 0;
    Counter(2) = 0;
    SetStep(0);
    SetRun(7);
}
extern "C" void __cdecl Scena02_Object0C(unsigned char*, unsigned char*) {
    Set40();
    SetRun(0xB);
    Counter(1) = 0;
    Counter(2) = 0;
    SetStep(0);
}
extern "C" void __cdecl Scena02_Object0D(unsigned char*, unsigned char*) {
    Set40();
    SetStep(5);
    Counter(1) = 0;
    Counter(2) = 0;
    SetRun(0xB);
}
extern "C" void __cdecl Scena02_Object0E(unsigned char*, unsigned char*) {
    Set40();
    Counter(0) = 0x14;
    Counter(1) = 0;
    Counter(2) = 0;
    SetStep(0x14);
    SetRun(2);
}
extern "C" void __cdecl Scena02_Object0F(unsigned char*, unsigned char*) {
    Set40();
    SetRun(0xF);
    Counter(1) = 0;
    Counter(2) = 0;
    SetStep(0);
}
extern "C" void __cdecl Scena02_Object10(unsigned char*, unsigned char*) {
    Set40();
    SetStep(0xF);
    SetRun(0x10);
    Sound(0x106);
}
extern "C" void __cdecl Scena02_Object11(unsigned char*, unsigned char*) {
    Set40();
    SetRun(0x12);
    Counter(0) = 0;
    Counter(1) = 0;
    Counter(2) = 0;
    Counter(3) = 0;
    SetStep(0);
}
extern "C" void __cdecl Scena02_Object13(unsigned char*, unsigned char*) {
    Set40();
    SetStep(0x1E);
    Counter(1) = 0;
    Counter(2) = 0;
    Counter(3) = 0;
    SetRun(2);
}
extern "C" void __cdecl Scena02_Object14(unsigned char*, unsigned char*) {
    Set40();
    SetStep(0xA);
    SetRun(0x1B);
}
extern "C" void __cdecl Scena02_Object15(unsigned char*, unsigned char*) {
    Set40();
    SetStep(0);
    SetRun(0x17);
}
extern "C" void __cdecl Scena02_Object16(unsigned char*, unsigned char*) {
    Set40();
    SetRun(0x19);
    Counter(0) = 0;
    Counter(1) = 0;
    Counter(2) = 0;
    Counter(3) = 0;
    SetStep(0);
}
extern "C" void __cdecl Scena02_Object17(unsigned char*, unsigned char*) {
    Set40();
    Counter(0) = 0;
    Counter(1) = 0;
    Counter(2) = 0;
    Counter(3) = 0;
    SetStep(0xA);
    SetRun(0x19);
    Sound(0x106);
}
extern "C" void __cdecl Scena02_Object18(unsigned char*, unsigned char*) { SetFlag(0x31); }
extern "C" void __cdecl Scena02_Object19(unsigned char*, unsigned char*) {
    Set40();
    SetStep(0x19);
    SetRun(0x1A);
}
extern "C" void __cdecl Scena02_Object1A(unsigned char*, unsigned char*) {
    Set40();
    SetStep(5);
    SetRun(0x1B);
}

// original 0x542080: member n's ten bytes at +0x7E of its record (0x903A70 +
// 0xA4 * n) handed back one by one: each byte not 0 passed to
// 0x590C90(byte, 0, 1, 0) and then cleared. Called by Scena02_EnterArea with
// 3 and 4; n is unchecked (section 6).
extern "C" void __cdecl Scena02_StripMember(unsigned char member) {
    unsigned char* p = At(at::kMemberRecords + at::kMemberStride * member + at::kMemberItems);
    for (unsigned i = 0; i < 10; ++i, ++p) {
        const unsigned char item = p[0];
        if (item == 0) continue;
        SH_AT(cl::ItemPutFn, cl::kItemPut)(item, 0, 1, 0);
        p[0] = 0;
    }
}

namespace {

bool In(int v, int lo, int hi) { return v >= lo && v <= hi; }
// A start the step hook makes: ScriptFlags_Set40 then run `run` at `step`.
unsigned char Start(unsigned run, unsigned step) {
    Set40();
    SetStep(step);
    SetRun(run);
    return 1;
}
// The same with the four counters cleared first (counter 0 = `c0`).
unsigned char StartCleared(unsigned run, unsigned step, unsigned c0) {
    Set40();
    Counter(0) = static_cast<unsigned char>(c0);
    Counter(1) = 0;
    Counter(2) = 0;
    Counter(3) = 0;
    SetStep(step);
    SetRun(run);
    return 1;
}

// Area 0x1A's tests, after its two changes by flag 0x32.
unsigned char StepHook1A(int x, int z) {
    if (Cond_ByteFD == 1 && Flag(0x34) && !Flag(0x35) && In(x, 0x50000, 0x90000) && z > 0xF0000) return Start(8, 6);
    if (Flag(7) && !Flag(0x34) && (z == 0x320000 || z == 0x328000) && In(x, 0x1D0000, 0x200000)) {
        Set40();
        Counter(0) = 0xA0;
        SetStep(0);
        SetRun(9);
        return 1;
    }
    // x 0x350000 / 0x358000: z in [0x4C0000, 0x4D8000] run 6; else, as for
    // any x, z 0x4C0000 / 0x4C8000 with x in [0x250000, 0x278000] run 0x19
    // (the original's branches reach that test only through z's compares,
    // which come to the same).
    if ((x == 0x350000 || x == 0x358000) && In(z, 0x4C0000, 0x4D8000)) return Start(6, 0);
    if ((z == 0x4C0000 || z == 0x4C8000) && In(x, 0x250000, 0x278000)) return StartCleared(0x19, 0xF, 0);
    if (!Flag(0x2F) && In(x, 0x300000, 0x320000) && (z == 0x2A0000 || z == 0x298000)) return Start(0x1A, 5);
    if (!Flag(0x30)) {
        if (In(x, 0x310000, 0x320000) && (z == 0x250000 || z == 0x248000)) return Start(0x1A, 0xF);
        if ((x == 0x3B0000 || x == 0x3B8000) && In(z, 0x260000, 0x298000)) return Start(0x1A, 0x14);
    }
    if (Flag(0x24)) return 0xFE;   // on to area 0x1B's tests
    const bool f25 = Flag(0x25);
    if (!(x == 0x298000 || x == 0x2A8000) || !In(z, 0x500000, 0x540000)) return 0xFE;
    const unsigned c1 = Counter(1);
    if (c1 == 0) {
        if (!f25) return StartCleared(0x18, 0, 0);
        Set40();
        Counter(0) = 0;
        Counter(1) = 0;
        Counter(2) = 0;
        Counter(3) = 0;
        SetStep(0xA);
        SetRun(0x18);
        DropIn(0xD);
        return 1;
    }
    if (c1 != 1) return 0xFE;
    Set40();
    Counter(0) = 1;
    Counter(1) = 0;
    Counter(2) = 0;
    Counter(3) = 0;
    SetStep(0x14);
    SetRun(0x18);
    DropIn(0xD);
    return 1;
}

}  // namespace

// original 0x5420C0: slot 2 of chapter 2's vtable, the step hook
// (Scenario_StepHook calls it with the leader's target x and z, 16.16): 1 in
// al when the step starts a scene (ScriptFlags_Set40 in all but two, the run
// and its step, sometimes an area change), 0 otherwise. By area: 3 a
// rectangle to area 3 (flags 0x1F set, 0x20 clear); 0xF a strip (flag 0x19
// clear) to run 5; 0x1A two changes of area by flag 0x32 and ten rectangles
// by flags 7, 0x2F, 0x30, 0x34, 0x35, 0x24, 0x25 and counter 1; 0x1B seven
// rectangles by flags 0xF..0x18 and the member count (one tests the high
// word of z alone); 0x1C one rectangle to run 0x11. Every bound is a signed
// 32-bit compare of the argument, inclusive unless a strict one is written.
extern "C" unsigned char __cdecl Scena02_StepHook(int x, int z) {
    if (Area() == 3 && Flag(0x1F) && !Flag(0x20) && In(x, 0x558000, 0x568000) && In(z, 0x1C0000, 0x1D8000)) {
        Set40();
        Counter(0) = 0;
        Counter(1) = 0;
        Counter(2) = 0;
        Counter(3) = 0;
        SetStep(5);
        SetRun(0x15);
        Draw_PassFlags = 0;
        ChangeArea(3, 0x330000, 0x140000, 0x81);
        return 1;
    }
    if (Area() == 0xF && !Flag(0x19) && z > 0x510000 && In(x, 0x360000, 0x378000)) return Start(5, 0);
    if (Area() == 0x1A) {
        if (Flag(0x32)) {
            if (!Flag(7)) {
                if (z == 0x80000 && In(x, 0x340000, 0x358000)) {
                    Counter(2) = 0;   // the original stores al, 0 here (flag 7's answer)
                    SetStep(0);
                    SetRun(8);
                    ChangeArea(0x1A, 0x70000, 0xD0000, 0x80);
                }
            } else if (Flag(0x34) && !Flag(0x35) && z == 0x80000 && In(x, 0x340000, 0x358000)) {
                Counter(2) = 0;       // al, 0 (flag 0x35's answer)
                SetStep(5);
                SetRun(8);
                ChangeArea(0x1A, 0x70000, 0xD0000, 0x81);
                SetAreaByte(0x1F);
            }
        }
        const unsigned char r = StepHook1A(x, z);
        if (r != 0xFE) return r;
    }
    if (Area() == 0x1B) {
        if ((x == 0x278000 || x == 0x288000) && In(z, 0x250000, 0x278000)) {
            Set40();
            SetRun(0x1B);
            SetStep(0);
            return 1;
        }
        if (Field_MemberCount == 2 && !Flag(0xF) && In(x, 0x1B0000, 0x1C8000) && z == 0x1C8000) {
            SetStep(0);   // al, 0 (flag 0xF's answer)
            SetRun(0xA);
            return 1;
        }
        if (!Flag(0x10) && In(x, 0x178000, 0x188000) && In(z, 0x250000, 0x278000)) {
            Set40();
            Counter(0) = 0;
            SetStep(5);
            SetRun(0xA);
            return 1;
        }
        if (!Flag(0x11) && In(x, 0x90000, 0xB8000) && In(z, 0x400000, 0x418000)) {
            Set40();
            Counter(0) = 0x14;
            SetStep(0);
            SetRun(0xC);
            return 1;
        }
        if (!Flag(0x13) && x == 0xD8000) {
            const unsigned hi = static_cast<std::uint32_t>(z) >> 16;
            if (hi == 3 || hi == 4) {
                Counter(0) = 0x25;
                SetStep(0);
                SetRun(0xD);
                Set40();
                return 1;
            }
        }
        if (!Flag(0x14) && In(x, 0x390000, 0x3B8000) && In(z, 0x480000, 0x490000)) {
            Set40();
            Counter(0) = 0x31;
            SetStep(0);
            SetRun(0xE);
            return 1;
        }
        if (!Flag(0x18) && In(z, 0x340000, 0x358000) && x == 0x208000) {
            Counter(0) = 0;   // al, 0 (flag 0x18's answer)
            SetStep(0xA);
            SetRun(0xF);
            return 1;
        }
    }
    if (Area() == 0x1C && !Flag(0x19) && (x == 0x230000 || x == 0x238000) && In(z, 0x5F0000, 0x600000)) return Start(0x11, 0);
    return 0;
}

void ScenaSc2_Inject() {
    if (bof3::WantsShadow("scena_sc2")) scena_sc2::SelfTest();
    BOF3_INJECT(Scena02_Frame);
    BOF3_INJECT(Scena02_Start);
    BOF3_INJECT(Scena02_EnterArea);
    BOF3_INJECT(Scena02_Run);
    BOF3_INJECT(Scena02_Scene00);
    BOF3_INJECT(Scena02_Scene02);
    BOF3_INJECT(Scena02_Scene04);
    BOF3_INJECT(Scena02_Scene05);
    BOF3_INJECT(Scena02_Scene06);
    BOF3_INJECT(Scena02_Scene07);
    BOF3_INJECT(Scena02_Scene08);
    BOF3_INJECT(Scena02_Scene08Next);
    BOF3_INJECT(Scena02_Scene09);
    BOF3_INJECT(Scena02_Scene0A);
    BOF3_INJECT(Scena02_Scene0B);
    BOF3_INJECT(Scena02_Scene0C);
    BOF3_INJECT(Scena02_Scene0D);
    BOF3_INJECT(Scena02_Scene0E);
    BOF3_INJECT(Scena02_Scene0F);
    BOF3_INJECT(Scena02_Scene10);
    BOF3_INJECT(Scena02_Scene11);
    BOF3_INJECT(Scena02_Scene12);
    BOF3_INJECT(Scena02_Scene13);
    BOF3_INJECT(Scena02_Scene15);
    BOF3_INJECT(Scena02_Scene17);
    BOF3_INJECT(Scena02_Scene18);
    BOF3_INJECT(Scena02_Scene19);
    BOF3_INJECT(Scena02_Scene1A);
    BOF3_INJECT(Scena02_Scene1B);
    BOF3_INJECT(Scena02_Near01);
    BOF3_INJECT(Scena02_Near02);
    BOF3_INJECT(Scena02_Near03);
    BOF3_INJECT(Scena02_Near04);
    BOF3_INJECT(Scena02_Near05);
    BOF3_INJECT(Scena02_Near06);
    BOF3_INJECT(Scena02_Near07);
    BOF3_INJECT(Scena02_Near08);
    BOF3_INJECT(Scena02_Near09);
    BOF3_INJECT(Scena02_Near0A);
    BOF3_INJECT(Scena02_Near0B);
    BOF3_INJECT(Scena02_Near0C);
    BOF3_INJECT(Scena02_DoorStart);
    BOF3_INJECT(Scena02_DoorSound);
    BOF3_INJECT(Scena02_StartRun07);
    BOF3_INJECT(Scena02_Shake);
    BOF3_INJECT(Scena02_PlaceEffect70);
    BOF3_INJECT(Scena02_PlaceEffect68);
    BOF3_INJECT(Scena02_ObjectHook);
    BOF3_INJECT(Scena02_Object01);
    BOF3_INJECT(Scena02_Object02);
    BOF3_INJECT(Scena02_Object04);
    BOF3_INJECT(Scena02_Object05);
    BOF3_INJECT(Scena02_Object06);
    BOF3_INJECT(Scena02_Object07);
    BOF3_INJECT(Scena02_Object09);
    BOF3_INJECT(Scena02_Object0B);
    BOF3_INJECT(Scena02_Object0C);
    BOF3_INJECT(Scena02_Object0D);
    BOF3_INJECT(Scena02_Object0E);
    BOF3_INJECT(Scena02_Object0F);
    BOF3_INJECT(Scena02_Object10);
    BOF3_INJECT(Scena02_Object11);
    BOF3_INJECT(Scena02_Object13);
    BOF3_INJECT(Scena02_Object14);
    BOF3_INJECT(Scena02_Object15);
    BOF3_INJECT(Scena02_Object16);
    BOF3_INJECT(Scena02_Object17);
    BOF3_INJECT(Scena02_Object18);
    BOF3_INJECT(Scena02_Object19);
    BOF3_INJECT(Scena02_Object1A);
    BOF3_INJECT(Scena02_StripMember);
    BOF3_INJECT(Scena02_StepHook);
}

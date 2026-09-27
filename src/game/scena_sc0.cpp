// Scenario chapter 0 (the PSX's SCENA00.EMI, compiled into the exe at
// 0x537F20..0x539AD0): its vtable Scena00_Hooks 0x660CD0 - the frame, the
// object hook, the step hook (slot 3 is Scenario_NoHook, slot 4 none) - the
// frame's three states, the run table's ten runs and the helpers they share.
// Round ten group SCH, the scenario harness's proof (scenario_harness.h).
// docs/scena_sc0.md.
//
// Every call out goes through the scenario harness (SH_CALL by name, the
// callees nobody owns by address in scena_sc0_callees.h), and every cell is
// read where and when the original reads it: the fuzz stands recorders in
// that move the step, the run, the timer, the counters and the flags after a
// call, so a value held across a call where the original reads memory again
// shows. Each function's comment says what it does and what of the
// original's shape it keeps.
#include "game/scena_sc0.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/scena_sc0_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

using namespace scena_sc0;
using std::int32_t;
using std::uint32_t;

unsigned char* At(uint32_t a) { return move_script::At(a); }
unsigned char& B(uint32_t a) { return At(a)[0]; }
std::uint16_t W(uint32_t a) { return move_script::Word(At(a)); }
short S(uint32_t a) { return static_cast<short>(move_script::Word(At(a))); }
void SetW(uint32_t a, unsigned v) { move_script::SetWord(At(a), v); }
uint32_t L(uint32_t a) { return static_cast<uint32_t>(move_script::Long(At(a))); }
void SetL(uint32_t a, uint32_t v) { move_script::SetLong(At(a), static_cast<int32_t>(v)); }
void SetL(unsigned char* p, uint32_t v) { move_script::SetLong(p, static_cast<int32_t>(v)); }
uint32_t L(const unsigned char* p) { return static_cast<uint32_t>(move_script::Long(p)); }

// The chapter's flag row, as the original reads it: the pointer afresh.
unsigned char* Row() { return At(L(kFlagRow)); }

constexpr uint32_t kTextLo = 0x401000, kTextHi = 0x5C3000;

// A handler out of one of the chapter's .data tables, read in place and
// unchecked as the original reads it. The tables run on into other data (the
// state table into the run table, whose entries are code too; the run and
// object tables into bytes that are not): where the original would jump to
// what is not code, ours aborts (docs/scena_sc0.md section 7).
uint32_t CodeAt(uint32_t table, int index, const char* who) {
    const uint32_t at = table + 4u * static_cast<uint32_t>(index);
    const uint32_t entry = L(at);
    const bool swapped = scenario_harness::g_active;   // the fuzz's recorders live outside .text
    if (!swapped && (entry < kTextLo || entry >= kTextHi))
        bof3::Fatal("%s: index %d reads 0x%X at 0x%X, not code - past its table (the original jumps there)", who, index,
                    (unsigned)entry, (unsigned)at);
    return entry;
}

// The effect slot just taken: Effect_FindFree's al into 0x903850, read back
// as the original reads it (a dword masked to its byte).
bool TakeSlot() {
    const unsigned char slot = SH_CALL(Effect_FindFree)();
    B(kSlot) = slot;
    return slot != 0xFF;
}
unsigned char* SlotRecord() { return At(kEffects + (static_cast<uint32_t>(B(kSlot)) << 7)); }

// A kind-0x13 effect at (x, y, z) with +9 = mode: the shape every run of this
// chapter places after TakeSlot.
void Kind13(int32_t x, int32_t y, int32_t z, unsigned char mode) {
    unsigned char* const e = SlotRecord();
    e[0] = 1;
    e[5] = 0x13;
    SetL(e + 0x64, static_cast<uint32_t>(x));
    SetL(e + 0x68, static_cast<uint32_t>(y));
    SetL(e + 0x6C, static_cast<uint32_t>(z));
    e[9] = mode;
}

void WaitLoad() {
    while (SH_CALL(File_LoadDone)() == 0) SH_CALL(Task_Sleep)(1);
}

void Timer(unsigned v) { SetW(kTimer, v); }
void Step(unsigned v) { B(kStep) = static_cast<unsigned char>(v); }
void Run(unsigned v) { B(kRun) = static_cast<unsigned char>(v); }

// The corner words of the current area's descriptor block (+0xC, then
// +0x12): MapView_CornerPtr set to them and returned. The index is
// Game_AreaNumber, unchecked in the original against Area_Descriptors' 200.
unsigned char* Corner(const char* who) {
    const unsigned area = W(kArea);
    if (area >= Area_Descriptors_count)
        bof3::Fatal("%s: Game_AreaNumber 0x%X past Area_Descriptors' %u", who, area, Area_Descriptors_count);
    const uint32_t desc = L(kDescriptors + 4 * area);
    const uint32_t corner = L(desc + 0xC) + 0x12;
    SetL(kCorner, corner);
    return At(corner);
}

// Sprite_EnsureAnimation with the whole dword the original pushes (the
// callee hands it on to Sprite_SetAnimation).
void EnsureAnimation(uint32_t eax) {
    const auto callee = reinterpret_cast<std::uintptr_t>(SH_CALL(Sprite_EnsureAnimation));
    reinterpret_cast<unsigned char (__cdecl*)(uint32_t)>(callee)(eax);
}

}  // namespace

// original 0x537F20: chapter 0's slot 0 (Scena00_Hooks, Field_ModeDispatch's):
// a tail jump through Scena00_States on the s8 0x8034E2 - 0 Scena00_Start,
// 1 Scena00_EnterArea, 2 Scena00_Run; 3..14 are Scena00_Runs' entries (the
// tables are one run of words), as Scena16_Frame's.
extern "C" void __cdecl Scena00_Frame(void) {
    reinterpret_cast<void (__cdecl*)()>(CodeAt(kStates, static_cast<signed char>(B(kState)), "Scena00_Frame"))();
}

// original 0x537F30: state 0, the chapter's start: call-table entry 0
// (Scena16_PartyReset), area 0x18 at (0x630000, 0xC0000) facing 3 - and the
// same into the pending change -, script flag 0x40, the 18 dwords
// 0x904608..0x90464F to -1, run 5 step 0, a frame at a time until
// File_LoadDone, the counter dword 0 and state 1.
extern "C" void __cdecl Scena00_Start(void) {
    SH_CALL(Scenario_CallA)(0);
    SH_CALL(Field_ChangeArea)(0x18, 0x630000, 0xC0000, 3);
    B(kScriptFlags) = static_cast<unsigned char>(B(kScriptFlags) | 0x40);
    SetW(kPending, 0x18);
    SetL(kPending + 4, 0x630000);
    SetL(kPending + 8, 0xC0000);
    B(kPending + 3) = 3;
    for (uint32_t a = 0x904608; a < 0x904650; a += 4) SetL(a, 0xFFFFFFFFu);
    Run(5);
    Step(0);
    WaitLoad();
    SetL(kCounter, 0);
    B(kState) = 1;
}

// original 0x537FE0: state 1, the set-up of the area just entered, then state
// 2. Area 1: Party_DropIn(0); with 0x90384A 0 a kind-0x10 effect, the camera's
// third angle back 0x155 and (unless 0x90384A is 3, read again after the
// effect) 0x90384A and the step 0; with 0x90384A 3 the angle on 0x155 and
// three sprite words; run 10 either way. Every area: Scena00_Area02. Area 4:
// once (flag 0) the pass flags 0, the flag, script flag 0x40, run 2; with
// Cond_ByteFD 2 the pass flags 0x1F and ObjTrio_SetBit40. Every area:
// Scena00_Area18. Area 0x19: sprite words, sound 0x200, run 11, timer 0x14,
// pass flags 0. Area 0x1F: the camera to (0x100, 0, 0) at 0x100; once (flag
// 1) the pass flags 0, the flag, state 2 and run 3 and return; else
// Party_DropIn(0) and run 9.
extern "C" void __cdecl Scena00_EnterArea(void) {
    if (W(kArea) == 1) {
        SH_CALL(Party_DropIn)(0);
        const unsigned char n = B(kByte4A);
        bool clear;
        if (n == 0) {
            if (TakeSlot()) {
                unsigned char* const e = SlotRecord();
                e[0] = 1;
                e[5] = 0x10;
                SetL(e + 0x18, 2);
                SetL(e + 0x1C, 2);
                SetL(e + 0xC, 3);
                SetL(e + 0x10, 1);
            }
            const unsigned char again = B(kByte4A);
            SetW(kAngles + 4, W(kAngles + 4) + 0xFEABu);
            clear = again != 3;
        } else if (n == 3) {
            SetW(kAngles + 4, W(kAngles + 4) + 0x155u);
            SetL(0x802204, 1);
            SetL(0x802208, 4);
            B(0x8021ED) = 7;
            clear = false;
        } else {
            clear = true;
        }
        if (clear) {
            B(kByte4A) = 0;
            Step(0);
        }
        Run(0xA);
    }
    SH_CALL(Scena00_Area02)();
    if (W(kArea) == 4) {
        const unsigned char t = SH_CALL(Flags_Test)(Row(), 0);
        if (t == 0) {
            B(kPassFlags) = t;
            SH_CALL(Flags_Set)(Row(), 0);
            SH_CALL(ScriptFlags_Set40)();
            Run(2);
        }
        if (B(kByteFD) == 2) {
            B(kPassFlags) = 0x1F;
            SH_CALL(ObjTrio_SetBit40)();
        }
    }
    SH_CALL(Scena00_Area18)();
    if (W(kArea) == 0x19) {
        SetL(0x7DEE98, 2);
        SetL(0x7DEE9C, 2);
        SetL(0x802160, 1);
        SetL(0x802164, 3);
        SH_CALL(Sound_PlayEffect)(0x200);
        Run(0xB);
        Timer(0x14);
        B(kPassFlags) = 0;
    }
    if (W(kArea) == 0x1F) {
        unsigned char* const row = Row();
        SetW(kAngles, 0x100);
        SetW(kAngles + 4, 0);
        SetW(kAngles + 2, 0);
        SetW(kDistance, 0x100);
        const unsigned char t = SH_CALL(Flags_Test)(row, 1);
        if (t == 0) {
            B(kPassFlags) = t;
            SH_CALL(Flags_Set)(Row(), 1);
            B(kState) = 2;
            Run(3);
            return;
        }
        SH_CALL(Party_DropIn)(0);
        Run(9);
    }
    B(kState) = 2;
}

// original 0x5381E0: area 2's set-up. Once (flag 2): the counter dword 0,
// elevation 0x300, Kind2_Place(0), the first camera angle on 0xAA, a
// kind-0x13 effect at (that angle - 0xAA, the second, the third) mode 0x40,
// ObjTrio_SetBit40, ObjTrio 0's +0x34 / +0x38 to (0x330000, 0x400000), the
// flag. Else once (flag 10): the flag, Party_DropIn(0), the kind-2 object to
// (0x260000, 0x1B8000).
extern "C" void __cdecl Scena00_Area02(void) {
    if (W(kArea) != 2) return;
    if (SH_CALL(Flags_Test)(Row(), 2) == 0) {
        SetL(kCounter, 0);
        SH_CALL(MapView_SetElevation)(0x300);
        SH_CALL(Kind2_Place)(0);
        SetW(kAngles, W(kAngles) + 0xAAu);
        if (TakeSlot()) Kind13(S(kAngles) - 0xAA, S(kAngles + 2), S(kAngles + 4), 0x40);
        SH_CALL(ObjTrio_SetBit40)();
        unsigned char* const row = Row();
        SetL(0x802D74, 0x330000);
        SetL(0x802D78, 0x400000);
        SH_CALL(Flags_Set)(row, 2);
        return;
    }
    if (SH_CALL(Flags_Test)(Row(), 0xA) != 0) return;
    SH_CALL(Flags_Set)(Row(), 0xA);
    SH_CALL(Party_DropIn)(0);
    SetL(kKind2X, 0x260000);
    SetL(kKind2Z, 0x1B8000);
}

// original 0x538300: area 0x18's set-up by Cond_ByteFD (a five-entry jump
// table, 0..4): 0 - the counter 0 and, unless flag 6, run 8 step 0; 1 -
// unless flag 3 (then the CLUT rows copied, ClutStrip dirty, and Party_DropIn(1)
// with the pass flags 0x1F and the counter 0 unless flag 5, else script flag
// 0x40 cleared): music 3 loaded and waited for, the pass flags 0x1F, the
// counter 0, the flag, elevation 0x480, the camera at 0x480 with its third
// angle 0x350, Party_DropIn(0), Kind2_Place(0), a kind-0x13 effect mode 0x90;
// 2 - unless flag 8, run 6; 3 - nothing; 4 - unless flag 9, run 6.
extern "C" void __cdecl Scena00_Area18(void) {
    if (W(kArea) != 0x18) return;
    switch (B(kByteFD)) {
    case 0: {
        unsigned char* const row = Row();
        SetL(kCounter, 0);
        if (SH_CALL(Flags_Test)(row, 6) != 0) return;
        Step(0);
        Run(8);
        return;
    }
    case 1:
        if (SH_CALL(Flags_Test)(Row(), 3) == 0) {
            SH_CALL(Music_LoadFile)(3);
            WaitLoad();
            unsigned char* const row = Row();
            B(kPassFlags) = 0x1F;
            SetL(kCounter, 0);
            SH_CALL(Flags_Set)(row, 3);
            SH_CALL(MapView_SetElevation)(0x480);
            SetW(kDistance, 0x480);
            SetW(kAngles + 4, 0x350);
            SH_CALL(Party_DropIn)(0);
            SH_CALL(Kind2_Place)(0);
            if (!TakeSlot()) return;
            Kind13(S(kAngles), S(kAngles + 2), 0x200, 0x90);
            return;
        }
        // the two 16-word CLUT rows 0x80BC00 and 0x80FC00 each from 0x20 on
        for (uint32_t i = 0; i < 0x20; i += 2) {
            SetW(0x80BC00 + i, W(0x80BC20 + i));
            SetW(0x80FC00 + i, W(0x80FC20 + i));
        }
        {
            unsigned char* const row = Row();
            Gfx_ClutStripDirty = 1;
            if (SH_CALL(Flags_Test)(row, 5) == 0) {
                B(kPassFlags) = 0x1F;
                SetL(kCounter, 0);
                SH_CALL(Party_DropIn)(1);
                return;
            }
        }
        SH_CALL(ScriptFlags_Clear40)();
        return;
    case 2:
        if (SH_CALL(Flags_Test)(Row(), 8) != 0) return;
        break;
    case 4:
        if (SH_CALL(Flags_Test)(Row(), 9) != 0) return;
        break;
    default:
        return;
    }
    Run(6);
    SetL(kCounter, 0);
    Step(0);
}

// original 0x5384F0: state 2, a tail jump through Scena00_Runs on the s8
// MoveScript_Var7 - 0 and 1 a bare ret (0x437CC0), 2..11 the runs below.
extern "C" void __cdecl Scena00_Run(void) {
    reinterpret_cast<void (__cdecl*)()>(CodeAt(kRuns, static_cast<signed char>(B(kRun)), "Scena00_Run"))();
}

// original 0x538500: run 2, steps 0..7 (a jump table; past 7 nothing).
extern "C" void __cdecl Scena00_Run2(void) {
    switch (B(kStep)) {
    case 0:
        if (W(kWait) != 0) return;
        SH_CALL(Msg_OpenScript)(0x10);
        B(kRequest) = 2;
        SH_CALL(Music_Play)(6, 8);
        B(kScriptFlags) = static_cast<unsigned char>(B(kScriptFlags) | 0x80);
        Timer(0);
        Step(1);
        return;
    case 1:
        if (W(kTimer) < 0x7C) Timer(W(kTimer) + 1u);
        if (B(kRequest) == 2) return;
        B(kPassFlags) = 0x1F;
        SH_CALL(ScriptFlags_Set40)();
        SH_CALL(ObjTrio_SetBit40)();
        SH_CALL(Transition_Start)(1);
        B(kByte4B) = 1;
        Timer(0);
        Step(2);
        return;
    case 2:
        if (B(kByte4B) != 0x54) return;
        SH_CALL(Transition_Start)(0);
        Step(3);
        return;
    case 3:
        if (W(kWait) != 0) return;
        B(kPassFlags) = 0;
        SH_CALL(Msg_OpenScript)(0x11);
        B(kRequest) = 2;
        Step(4);
        return;
    case 4:
        if (B(kRequest) == 2) return;
        SH_CALL(Field_ChangeArea)(4, 0x440000, 0x80000, 5);
        Step(5);
        return;
    case 5:
        if (B(kByteFD) != 2) return;
        B(kPassFlags) = 0x1F;
        Step(6);
        return;
    case 6:
        if (B(kByte4A) != 0x23) return;
        if (TakeSlot()) Kind13(S(kAngles) + 0x180, S(kAngles + 2), S(kAngles + 4), 0x60);
        Step(7);
        return;
    case 7:
        if (B(kByte4B) != 0x60) return;
        SH_CALL(Field_ChangeArea)(0x1F, 0x70000, 0x140000, 0);
        B(0x937F98) = 0xFF;
        SH_CALL(Music_FadeOutStop)(0x40);
        Run(3);
        Step(0);
        return;
    default:
        return;
    }
}

// original 0x538710: run 3, steps 0..3. Step 1's view test is the one scene
// 3 of chapter 16 makes: at MapView_FocusZ 0x4400 the kind-2 object, a
// sprite and the kind-2 sprite 0x1E0000 back, 0x7E068A back 0x1E, the focus
// to 0x6200, then the view shift (a tail jump in the original).
extern "C" void __cdecl Scena00_Run3(void) {
    switch (B(kStep)) {
    case 0:
        if (B(kByte4A) != 0x30) return;
        B(kPassFlags) = 0x1F;
        SH_CALL(Transition_Start)(1);
        if (TakeSlot()) Kind13(S(kAngles) + 0x1C0, S(kAngles + 2), S(kAngles + 4), 0xFF);
        Step(1);
        return;
    case 1: {
        if (B(kByte4A) == 0x35) {
            Step(2);
            return;
        }
        if (L(kFocusZ) != 0x4400) return;
        const uint32_t z = L(kKind2Z), s = L(0x802038);
        SetW(0x7E068A, W(0x7E068A) - 0x1Eu);
        SetL(kKind2Z, z - 0x1E0000);
        const uint32_t k = L(0x7E0978);
        SetL(kFocusZ, 0x6200);
        SetL(0x802038, s - 0x1E0000);
        SetL(0x7E0978, k - 0x1E0000);
        ViewShift();
        return;
    }
    case 2:
        B(kByte4A) = 0x37;
        Step(3);
        return;
    case 3:
        if (B(kByte4A) != 0x38) return;
        SH_CALL(Music_Play)(2, 0x20);
        SH_CALL(Field_ChangeArea)(2, 0x340000, 0x430000, 3);
        Run(4);
        Step(0);
        return;
    default:
        return;
    }
}

// original 0x538870: run 4, steps 0..10: three script messages on the
// counter, a 0x130-frame camera pull (the distance from 0x67FFF90 less
// 0x57943 per frame left, high word), two effects, a 0x200-frame pull back
// (13 * frames << 14, high word), the transition, message 4 and a fade, then
// area 0x18 and run 5.
extern "C" void __cdecl Scena00_Run4(void) {
    switch (B(kStep)) {
    case 0:
        if (W(kWait) != 0) return;
        SH_CALL(Msg_OpenScript)(1);
        B(kRequest) = 2;
        Step(1);
        return;
    case 1:
        if (B(kRequest) == 2) return;
        Step(2);
        return;
    case 2:
        if (B(kCounter) != 3) return;
        SH_CALL(Msg_OpenScript)(2);
        B(kRequest) = 2;
        Step(3);
        return;
    case 3:
        if (B(kCounter) != 4) return;
        SH_CALL(Msg_OpenScript)(3);
        B(kRequest) = 2;
        Timer(0x130);
        Step(4);
        return;
    case 4: {
        const std::uint16_t t = W(kTimer);
        if (t == 0) {
            Step(5);
            return;
        }
        const std::uint16_t left = static_cast<std::uint16_t>(t - 1);
        const uint32_t d = 0x67FFF90u - static_cast<uint32_t>(left) * 0x57943u;
        Timer(left);
        SetW(kDistance, d >> 16);
        return;
    }
    case 5:
        if (B(kCounter) != 5) return;
        if (TakeSlot()) Kind13(static_cast<int32_t>(0xFFFFFCB6u), S(kAngles + 2), S(kAngles + 4), 0x60);
        Step(6);
        return;
    case 6:
        if (B(kCounter) != 8) return;
        if (TakeSlot()) Kind13(static_cast<int32_t>(0xFFFFFD54u), S(kAngles + 2), S(kAngles + 4), 0x20);
        Timer(0x200);
        Step(7);
        return;
    case 7: {
        const std::uint16_t t = W(kTimer);
        if (t == 0) {
            Step(8);
            return;
        }
        const uint32_t d = ((static_cast<uint32_t>(t) * 13u) << 14) >> 16;
        SetW(kDistance, d);
        Timer(static_cast<std::uint16_t>(t - 1));
        return;
    }
    case 8:
        if (B(kCounter) != 0xB) return;
        SH_CALL(Transition_Start)(0);
        Step(9);
        return;
    case 9:
        if (W(kWait) != 0) return;
        B(kPassFlags) = 0;
        SH_CALL(Msg_OpenScript)(4);
        B(kRequest) = 2;
        SH_CALL(Music_FadeOutStop)(0x20);
        Step(0xA);
        return;
    case 0xA:
        if (B(kRequest) == 2) return;
        SH_CALL(Field_ChangeArea)(0x18, 0x630000, 0xC0000, 3);
        Run(5);
        Step(0);
        return;
    default:
        return;
    }
}

// original 0x538B10: run 5, steps 0..17 (a jump table). First, on steps
// 4..6 every 16th frame: Rand % 3 picks 1, 2 or 4 into 0x90384B (a negative
// Rand shifts the byte out: 0), and above 1 adds 0x10 to 0x90384A; the frame
// counter is read again after Rand. Steps 4 and 5 shake a sprite word by
// Scena00_Shake[frame & 15] * 4 while 0x90384A counts down; 14 shakes the
// sprite records' +0x34 / +0x38 by the table's value << 11.
extern "C" void __cdecl Scena00_Run5(void) {
    uint32_t frame = Frame_Counter;
    if (static_cast<unsigned char>(B(kStep) - 4) < 3 && (frame & 0xF) == 0) {
        const int r = SH_CALL(Rand)();
        const unsigned count = static_cast<unsigned>(r % 3) & 0x1F;   // cl, as shl masks it
        const unsigned char bit = count < 8 ? static_cast<unsigned char>(1u << count) : 0;
        B(kByte4B) = bit;
        if (bit > 1) B(kByte4A) = static_cast<unsigned char>(B(kByte4A) + 0x10);
        frame = Frame_Counter;
    }
    switch (B(kStep)) {
    case 0:
        B(kScriptFlags) = static_cast<unsigned char>(B(kScriptFlags) | 0x80);
        SH_CALL(Flags_Set)(Row(), 0);
        SH_CALL(Flags_Set)(Row(), 1);
        SH_CALL(Flags_Set)(Row(), 2);
        SH_CALL(Flags_Set)(Row(), 3);
        Timer(0x90);
        Step(1);
        return;
    case 1:
    case 2: {
        const std::uint16_t t = W(kTimer);
        if (t == 0) {
            SetW(kDistance, 0);
            B(0x905E69) = 2;   // MapView_Redraw
            Step(3);
            return;
        }
        SetW(kDistance, ((static_cast<uint32_t>(t) << 19) >> 16));
        Timer(static_cast<std::uint16_t>(t - 1));
        return;
    }
    case 3:
        if (B(kCounter) != 0xC) return;
        Timer(0x3C);
        Step(4);
        B(kByte4A) = 0;
        return;
    case 4: {
        if (W(kTimer) != 0) {
            Timer(W(kTimer) - 1u);
        } else {
            const unsigned char c = B(kCounter);
            Step(5);
            B(kCounter) = static_cast<unsigned char>(c + 1);
        }
        const unsigned char n = B(kByte4A);
        if (n == 0) return;
        const auto d = static_cast<std::uint16_t>(static_cast<signed char>(B(kShake + (frame & 0xF))) * 4);
        SetW(0x80203E, W(0x80203E) + d);
        B(kByte4A) = static_cast<unsigned char>(n - 1);
        return;
    }
    case 5: {
        if (B(kCounter) == 0x11) {
            Timer(0x3C);
            Step(6);
        }
        const unsigned char n = B(kByte4A);
        if (n == 0) return;
        B(kByte4A) = static_cast<unsigned char>(n - 1);
        const auto d = static_cast<std::uint16_t>(static_cast<signed char>(B(kShake + (frame & 0xF))) * 4);
        SetW(0x80203E, W(0x80203E) + d);
        return;
    }
    case 6: {
        if (W(kTimer) != 0) {
            Timer(W(kTimer) - 1u);
            return;
        }
        if (TakeSlot()) {
            unsigned char* const e = SlotRecord();
            e[0] = 1;
            e[5] = 0x1E;
            SetL(e + 0x34, L(0x802D74));
            SetL(e + 0x38, L(0x802D78));
            const long y = SH_CALL(AreaMap_Elevation)(static_cast<long>(L(e + 0x34)), static_cast<long>(L(e + 0x38)));
            SetL(e + 0x3C, static_cast<uint32_t>(static_cast<int32_t>(static_cast<short>(y))) << 16);
            e[0x29] = 0;
        }
        const unsigned char c = B(kCounter);
        Timer(0x84);
        Step(7);
        B(kCounter) = static_cast<unsigned char>(c + 1);
        return;
    }
    case 7: {
        const auto t = static_cast<std::uint16_t>(W(kTimer) - 1);
        Timer(t);
        if (t != 0x20) return;
        SH_CALL(Transition_Start)(8);
        Step(8);
        return;
    }
    case 8:
        if (W(kTimer) != 0) {
            Timer(W(kTimer) - 1u);
            if (W(kWait) != 0) return;
            B(kPassFlags) = 0;
            return;
        }
        B(kCounter) = 0x14;
        Timer(0x40);
        Step(0xA);
        return;
    case 9:
        if (B(kCounter) != 0x14) return;
        Timer(0x25C);
        Step(0xA);
        return;
    case 0xA:
        if (W(kTimer) != 0) {
            Timer(W(kTimer) - 1u);
            return;
        }
        SH_CALL(Flags_Set)(Row(), 4);
        SH_CALL(Field_ChangeArea)(0x18, 0x630000, 0x110000, 3);
        B(0x937F98) = 9;
        B(0x904EE0) = 0xFF;
        Step(0xB);
        return;
    case 0xB:
        if (W(kWait) != 0) return;
        B(kCounter) = 1;
        Step(0xC);
        return;
    case 0xC:
        if (B(kCounter) != 4) return;
        if (SH_CALL(File_LoadDone)() == 0) return;
        SH_CALL(Music_Play)(3, 8);
        Step(0xD);
        return;
    case 0xD:
        if (B(kCounter) != 5) return;
        Timer(0x20);
        Step(0xE);
        return;
    case 0xE: {
        if (W(kTimer) == 0) {
            Step(0xF);
            return;
        }
        const uint32_t k = B(kTimer) & 0xF;
        const uint32_t z = L(0x7DF000);
        const uint32_t d = static_cast<uint32_t>(static_cast<int32_t>(static_cast<signed char>(B(kShake + k)))) << 11;
        const uint32_t x = L(0x7DEFFC) + d;
        Timer(W(kTimer) - 1u);
        SetL(0x7DEFFC, x);
        SetL(0x7DF000, z - d);
        return;
    }
    case 0xF: {
        if (B(kCounter) != 7) return;
        const uint32_t z = L(0x802D78), x = L(0x802D74);
        PlaceParty(x, z, 1);
        EventBattle(1);
        SH_CALL(Flags_Set)(Row(), 5);
        Step(0x10);
        return;
    }
    case 0x11:
        B(kCounter) = static_cast<unsigned char>(B(kCounter) + 1);
        SH_CALL(ScriptFlags_Clear40)();
        SH_CALL(Party_DropIn)(6);
        Run(0);
        Step(0);
        return;
    default:   // 0x10, and past 0x11
        return;
    }
}

// original 0x538F90: run 6, steps 0..0x32 through a byte table and an
// 11-entry jump table: 0 takes its first step from Scena00_StartSteps by
// Cond_ByteFD (unchecked); 0xA, 0x14 and 0x1E join a member (script flag
// 0x40, Party_DropIn(3 / 4 / 5), flag 7 / 8 / 9; the last two only unless
// ObjTrio 0's +1 is 2); 0xB, 0x15 and 0x1F, on the counter 0xB / 5 / 4, place
// the party at ObjTrio 0's position and start event battle 4 / 5 / 6; 0xD,
// 0x17 and 0x21 end the run (the counter + 1, script flag 0x40 cleared, run
// and step 0 - 0xD counts before the call, 0x17 / 0x21 after); 0x32 goes to
// area 0x1F. Every other step to 0x32 nothing.
extern "C" void __cdecl Scena00_Run6(void) {
    const unsigned step = B(kStep);
    if (step > 0x32) return;
    switch (step) {
    case 0:
        Step(B(kStartSteps + B(kByteFD)));
        return;
    case 0xA:
        SH_CALL(ScriptFlags_Set40)();
        SH_CALL(Party_DropIn)(3);
        SH_CALL(Flags_Set)(Row(), 7);
        Step(0xB);
        return;
    case 0xB: {
        if (B(kCounter) != 0xB) return;
        const uint32_t z = L(0x802D78), x = L(0x802D74);
        PlaceParty(x, z, 4);
        EventBattle(4);
        Step(0xC);
        return;
    }
    case 0xD:
        B(kCounter) = static_cast<unsigned char>(B(kCounter) + 1);
        SH_CALL(ScriptFlags_Clear40)();
        Step(0);
        Run(0);
        return;
    case 0x14:
        if (B(kObjTrio + 1) == 2) return;
        SH_CALL(ScriptFlags_Set40)();
        SH_CALL(Party_DropIn)(4);
        SH_CALL(Flags_Set)(Row(), 8);
        Step(0x15);
        return;
    case 0x15: {
        if (B(kCounter) != 5) return;
        const uint32_t z = L(0x802D78), x = L(0x802D74);
        PlaceParty(x, z, 5);
        EventBattle(5);
        Step(0x16);
        return;
    }
    case 0x17:
    case 0x21: {
        SH_CALL(ScriptFlags_Clear40)();
        B(kCounter) = static_cast<unsigned char>(B(kCounter) + 1);
        Step(0);
        Run(0);
        return;
    }
    case 0x1E:
        if (B(kObjTrio + 1) == 2) return;
        SH_CALL(ScriptFlags_Set40)();
        SH_CALL(Party_DropIn)(5);
        SH_CALL(Flags_Set)(Row(), 9);
        Step(0x1F);
        return;
    case 0x1F: {
        if (B(kCounter) != 4) return;
        const uint32_t z = L(0x802D78), x = L(0x802D74);
        PlaceParty(x, z, 6);
        EventBattle(6);
        Step(0x20);
        return;
    }
    case 0x32:
        SH_CALL(Field_ChangeArea)(0x1F, 0x40000, 0x240000, 1);
        B(0x904CD0) = 4;
        Run(0);
        Step(0);
        return;
    default:
        return;
    }
}

// original 0x5391B0: run 7, steps 0..8 (2 does nothing): a member in and the
// kind-2 object placed; event battle 7 on the counter 0xD; message 0xE with
// script flags 0x180; the transition and the camera set (the kind-2 object and
// sprite to (0x2F0000, 0x1D0000), Field_ViewReset, elevation 0x580); a
// 0x1E-frame wait, then the kind-2 object again and an effect mode 0x20; a
// 0x20-frame pull (distance - 0x24 a frame); music 4 on the counter 0x24;
// area 0x1F on 0x25.
extern "C" void __cdecl Scena00_Run7(void) {
    switch (B(kStep)) {
    case 0:
        B(kCounter) = 0xA;
        SH_CALL(Party_DropIn)(1);
        SH_CALL(Kind2_Place)(1);
        Step(1);
        return;
    case 1: {
        if (B(kCounter) != 0xD) return;
        const uint32_t z = L(0x802D78), x = L(0x802D74);
        PlaceParty(x, z, 7);
        EventBattle(7);
        Step(2);
        return;
    }
    case 3:
        SetW(kScriptFlags, W(kScriptFlags) | 0x180u);
        SH_CALL(Msg_OpenScript)(0xE);
        B(kRequest) = 2;
        Step(4);
        return;
    case 4:
        if (B(kRequest) == 2) return;
        B(kCounter) = 0x1E;
        B(kPassFlags) = 0x1F;
        SH_CALL(Transition_Start)(3);
        SetL(0x7E0974, 0x2F0000);
        SetL(kKind2X, 0x2F0000);
        SetL(0x7E0978, 0x1D0000);
        SetL(kKind2Z, 0x1D0000);
        SH_CALL(Field_ViewReset)();
        SetL(0x7E097C, 0x5800000);
        SH_CALL(MapView_SetElevation)(0x580);
        SetW(kAngles, 0xFCF6);
        SetW(kAngles + 4, 0x180);
        SetW(kDistance, 0x480);
        Timer(0x1E);
        Step(5);
        return;
    case 5:
        if (W(kWait) != 0) return;
        Timer(W(kTimer) - 1u);
        if (W(kTimer) != 0) return;
        SH_CALL(Kind2_Place)(2);
        SetL(0x7E097C, 0x5800000);
        SH_CALL(MapView_SetElevation)(0x580);
        if (TakeSlot()) Kind13(static_cast<int32_t>(0xFFFFFD56u), S(kAngles + 2), 0x200, 0x20);
        Timer(0x20);
        Step(6);
        return;
    case 6: {
        if (W(kTimer) != 0) {
            Timer(W(kTimer) - 1u);
            SetW(kDistance, W(kDistance) - 0x24u);
            return;
        }
        const unsigned char c = B(kCounter);
        Step(7);
        B(kCounter) = static_cast<unsigned char>(c + 1);
        return;
    }
    case 7:
        if (B(kCounter) != 0x24) return;
        SH_CALL(Music_Play)(4, 8);
        Step(8);
        return;
    case 8:
        if (B(kCounter) != 0x25) return;
        SH_CALL(Field_ChangeArea)(0x1F, 0x40000, 0x240000, 1);
        Run(0);
        Step(0);
        return;
    default:
        return;
    }
}

// original 0x539400: run 8: at step 0, flag 6, Party_DropIn(2), run and step 0.
extern "C" void __cdecl Scena00_Run8(void) {
    if (B(kStep) != 0) return;
    SH_CALL(Flags_Set)(Row(), 6);
    SH_CALL(Party_DropIn)(2);
    Run(0);
    Step(0);
}

// original 0x539430: run 9: step 0 waits for 0x90384A 0x71 (timer 0x3C, step
// 2); step 2 counts the timer down, then area 1 at (0xF0000, 0x210000), sound
// 0x206, the counter 0, run 10.
extern "C" void __cdecl Scena00_Run9(void) {
    const unsigned step = B(kStep);
    if (step == 0) {
        if (B(kByte4A) != 0x71) return;
        Timer(0x3C);
        Step(2);
        return;
    }
    if (step != 2) return;
    if (W(kTimer) != 0) {
        Timer(W(kTimer) - 1u);
        return;
    }
    SH_CALL(Field_ChangeArea)(1, 0xF0000, 0x210000, 1);
    SH_CALL(Sound_PlayEffect)(0x206);
    SetL(kCounter, 0);
    Step(0);
    Run(0xA);
}

// original 0x5394B0: run 10, steps 0..3: the camera turned to 0x2D (speed
// 0xA), then a timer of 0x32; while 0x90384A is odd two random sprite
// coordinates, and at 0x90384A 3 after the timer area 1; the camera turned to
// 0xF, then 0x90384A 4; at 9 script flag bit 7 cleared, area 0x19 and run 11.
extern "C" void __cdecl Scena00_Run10(void) {
    switch (B(kStep)) {
    case 0:
        if (TurnCamera(0x2D, 0xA) != 0) return;
        Timer(0x32);
        Step(1);
        return;
    case 1:
        if (B(kByte4A) & 1) {
            SetL(0x802220, (static_cast<uint32_t>(SH_CALL(Rand)() & 7) + 0x69D) << 10);
            SetL(0x802224, (static_cast<uint32_t>(SH_CALL(Rand)() & 7) + 0x55D) << 10);
        }
        if (B(kByte4A) != 3) return;
        if (W(kTimer) != 0) {
            Timer(W(kTimer) - 1u);
            return;
        }
        SH_CALL(Field_ChangeArea)(1, 0x1A0000, 0x160000, 1);
        Step(2);
        return;
    case 2:
        if (TurnCamera(0xF, 0xA) != 0) return;
        Step(3);
        B(kByte4A) = 4;
        return;
    case 3:
        if (B(kByte4A) != 9) return;
        SetW(kScriptFlags, W(kScriptFlags) & 0xFF7Fu);
        SH_CALL(Field_ChangeArea)(0x19, 0xB8000, 0x5A0000, 1);
        Run(0xB);
        Timer(0x258);
        Step(0);
        return;
    default:
        return;
    }
}

// original 0x539950: steering by the pad - the current area's corner words
// (MapView_CornerPtr) zeroed and moved by 2 for right / left (0x2000 / 0x8000)
// and down / up (0x4000 / 0x1000), the sprite words 0x8021AC / 0x8021B0 by
// 0x40 with them, then sound 0x204.
extern "C" void __cdecl Scena00_Steer(void) {
    move_script::SetWord(Corner("Scena00_Steer"), 0);
    if (B(kInput + 1) & 0x20) {
        SetW(L(kCorner), W(L(kCorner)) + 2u);
        SetL(0x8021AC, L(0x8021AC) + 0x40);
    }
    if (B(kInput + 1) & 0x80) {
        SetW(L(kCorner), W(L(kCorner)) - 2u);
        SetL(0x8021AC, L(0x8021AC) - 0x40);
    }
    SetW(L(kCorner) + 2, 0);
    if (B(kInput + 1) & 0x40) {
        SetW(L(kCorner) + 2, W(L(kCorner) + 2) + 2u);
        SetL(0x8021B0, L(0x8021B0) + 0x40);
    }
    if (B(kInput + 1) & 0x10) {
        SetW(L(kCorner) + 2, W(L(kCorner) + 2) - 2u);
        SetL(0x8021B0, L(0x8021B0) - 0x40);
    }
    SH_CALL(Sound_PlayEffect)(0x204);
}

// original 0x5395D0: run 11. Every 32nd frame Rand & 7 into 0x90384B; while
// 0x90384A is below 0x14 and at MapView_FocusZ 0x5000 the view moves on
// 0x200000 (the kind-2 object, two sprites, the kind-2 sprite; 0x7E068A + 0x20;
// the focus to 0x3000; the view shift). Then steps 0..9 (1 and 2 nothing):
// timed transitions, the steering (Scena00_Steer, with the pad's direction
// animating sprite 0 through Scena00_InputAnims), a random wobble of the
// corner words, and at the end area 9 with ObjTrio 0's +0x24 bit 5 cleared.
extern "C" void __cdecl Scena00_Run11(void) {
    if ((B(0x937F94) & 0x1F) == 0) B(kByte4B) = static_cast<unsigned char>(SH_CALL(Rand)() & 7);
    if (B(kByte4A) < 0x14 && L(kFocusZ) == 0x5000) {
        const uint32_t z = L(kKind2Z), s = L(0x802038);
        SetW(0x7E068A, W(0x7E068A) + 0x20u);
        SetL(kKind2Z, z + 0x200000);
        const uint32_t s2 = L(0x8020DC);
        SetL(0x802038, s + 0x200000);
        const uint32_t k = L(0x7E0978);
        SetL(kFocusZ, 0x3000);
        SetL(0x8020DC, s2 + 0x200000);
        SetL(0x7E0978, k + 0x200000);
        ViewShift();
    }
    switch (B(kStep)) {
    case 0:
        Timer(W(kTimer) - 1u);
        if (W(kTimer) != 0) return;
        B(kByte4A) = 0x10;
        B(kPassFlags) = 0x1F;
        SH_CALL(Transition_Start)(1);
        SH_CALL(Sound_PlayEffect)(0x203);
        Timer(0x3C);
        Step(3);
        return;
    case 3:
        if (W(kTimer) != 0) {
            Timer(W(kTimer) - 1u);
            return;
        }
        B(kByte4A) = 0x11;
        Step(4);
        Timer(0x384);
        return;
    case 4: {
        const unsigned char pressed = B(kInput + 1);
        Timer(W(kTimer) - 1u);
        if ((pressed & 0xF0) == 0 && W(kTimer) != 0) return;
        B(kCount2) = 0;
        B(kCounter) = 0;
        B(kByte4A) = 0x12;
        SH_CALL(Scena00_Steer)();
        Timer(0x384);
        Step(5);
        return;
    }
    case 5: {
        if (L(0x8021AC) != 0 || L(0x8021B0) != 0) {
            SetL(0x8021AC, 0);
            SetL(0x8021B0, 0);
        }
        bool check = true;
        if (B(kInput + 1) & 0xF0) {
            const unsigned char n = B(kCount2);
            B(kCounter) = 0;
            B(kCount2) = static_cast<unsigned char>(n + 1);
            SH_CALL(Scena00_Steer)();
            const uint32_t i = (static_cast<uint32_t>(W(kInput)) >> 12) << 1;
            if (B(kInputAnims + i) != 0) {
                const unsigned char animation = B(kInputAnims + i + 1);
                Sprite_Current = Sprite_Objects;
                B(0x7DEE88) = animation;
                EnsureAnimation(animation);
            }
        } else {
            const auto c = static_cast<unsigned char>(B(kCounter) + 1);
            B(kCounter) = c;
            if (c > 0x14) {
                B(kCount2) = 0;
                B(kCounter) = 0;
                check = false;
            }
        }
        if (check && B(kCount2) > 0x1E) {
            B(kByte4A) = 0x13;
            Step(6);
            Timer(0);
        }
        Timer(W(kTimer) - 1u);
        if (W(kTimer) != 0) return;
        B(kByte4A) = 0x13;
        Step(6);
        return;
    }
    case 6: {
        Corner("Scena00_Run11");
        {
            const int r = SH_CALL(Rand)();
            SetW(L(kCorner), static_cast<unsigned>((r & 7) - 3));
        }
        {
            const int r = SH_CALL(Rand)();
            SetW(L(kCorner) + 2, static_cast<unsigned>((r & 7) - 3));
        }
        const uint32_t p = L(kCorner);
        SetL(0x8021AC, S(p) >= 0 ? 0x40u : 0xFFFFFFC0u);
        SetL(0x8021B0, S(p + 2) >= 0 ? 0x40u : 0xFFFFFFC0u);
        if ((B(0x937F94) & 3) == 0) {
            const auto animation = static_cast<unsigned char>((B(0x7DEE88) + 1) & 3);
            Sprite_Current = Sprite_Objects;
            B(0x7DEE88) = animation;
            EnsureAnimation((p & 0xFFFFFF00u) | animation);
        }
        SH_CALL(Sound_PlayEffect)(0x204);
        if (B(kByte4A) != 0x15) return;
        SetL(0x8021AC, 0);
        SetL(0x8021B0, 0);
        Step(7);
        return;
    }
    case 7:
        if (B(kByte4A) != 0x17) return;
        Step(8);
        return;
    case 8:
        SH_CALL(Transition_Start)(0);
        Step(9);
        return;
    case 9: {
        if (W(kWait) != 0) return;
        const auto bits = static_cast<unsigned char>(B(0x802D64) & 0xDF);
        SetW(kScriptFlags, W(kScriptFlags) & 0xFFB7u);
        B(kPassFlags) = 0;
        B(0x802D64) = bits;
        SH_CALL(Field_ChangeArea)(9, 0x570000, 0x40000, 7);
        Status80();
        return;
    }
    default:
        return;
    }
}

// original 0x539A10: chapter 0's slot 1, the object hook (0x56D6D0 pushes
// the object): Scena00_ObjectHooks[object +0x86](object, the flag row).
// Called only for +0x86 not 0xFF (Field_ObjectIdle); the table has one entry,
// a bare ret, and ours aborts where the original would jump past it.
extern "C" void __cdecl Scena00_ObjectHook(unsigned char* object) {
    unsigned char* const row = Row();
    const auto fn = reinterpret_cast<void (__cdecl*)(unsigned char*, unsigned char*)>(
        CodeAt(kObjectHooks, object[0x86], "Scena00_ObjectHook"));
    fn(object, row);
}

// original 0x539A30: chapter 0's slot 2, the step hook (x, z), answering in al;
// only x's cell (its high word) is read. Area 2, cell 0x2B, flag 6: run 7
// step 0 (and on to the next test, which area 2 fails). Area 0x18 with
// Cond_ByteFD 0, cell 0x14, not flag 7: script flag 0x40, run 6 step 0, al 1.
// Else al 0. Game_AreaNumber is read again after the flag test, as the
// original reads it.
extern "C" unsigned char __cdecl Scena00_StepHook(long x, long z) {
    (void)z;
    const auto cell = static_cast<short>(static_cast<std::uint32_t>(x) >> 16);
    if (W(kArea) == 2) {
        if (cell != 0x2B) return 0;
        if (SH_CALL(Flags_Test)(Row(), 6) != 0) {
            Run(7);
            Step(0);
        }
    }
    if (W(kArea) != 0x18) return 0;
    if (B(kByteFD) != 0) return 0;
    if (cell != 0x14) return 0;
    if (SH_CALL(Flags_Test)(Row(), 7) != 0) return 0;
    SH_CALL(ScriptFlags_Set40)();
    Run(6);
    Step(0);
    return 1;
}

void ScenaSc0_Inject() {
    if (bof3::WantsShadow("scena_sc0")) scena_sc0::SelfTest();
    BOF3_INJECT(Scena00_Frame);
    BOF3_INJECT(Scena00_Start);
    BOF3_INJECT(Scena00_EnterArea);
    BOF3_INJECT(Scena00_Area02);
    BOF3_INJECT(Scena00_Area18);
    BOF3_INJECT(Scena00_Run);
    BOF3_INJECT(Scena00_Run2);
    BOF3_INJECT(Scena00_Run3);
    BOF3_INJECT(Scena00_Run4);
    BOF3_INJECT(Scena00_Run5);
    BOF3_INJECT(Scena00_Run6);
    BOF3_INJECT(Scena00_Run7);
    BOF3_INJECT(Scena00_Run8);
    BOF3_INJECT(Scena00_Run9);
    BOF3_INJECT(Scena00_Run10);
    BOF3_INJECT(Scena00_Steer);
    BOF3_INJECT(Scena00_Run11);
    BOF3_INJECT(Scena00_ObjectHook);
    BOF3_INJECT(Scena00_StepHook);
}

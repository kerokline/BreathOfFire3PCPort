// Chapter 5's scenario bank, 0x546390..0x54A910 (round ten group SC5).
// docs/scena_sc5.md.
//
//   - The vtable Scena05_Hooks 0x661020 (0x662C80's entry 5): slot 0
//     Scena05_Frame, a tail jump through Scena05_States on the state byte;
//     slot 1 Scena05_ObjectTrigger, a call through Scena05_Objects on the
//     object's +0x86; slot 2 Scena05_StepHook (x, z) answering in al; slot 3
//     Scenario_NoHook (Capcom's, ours already); slot 4 Scena05_CellHook
//     (a, b), the record search 0x56D800 over Scena05_CellRecords and a tail
//     jump through Scena05_CellHooks.
//   - State 1 Scena05_EnterArea (every way out stores state 2); state 2
//     Scena05_Run, a tail jump through Scena05_Runs on MoveScript_Var7; the
//     eighteen runs are each a switch on the step byte 0x8034E5 whose cases
//     wait on a counter, the request byte, the wait word or the message box's
//     done bit, do one thing and set the next step. State 0 (0x5646B0) lies
//     in group SC13's block.
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies. The tables
// are read in place and their entries called directly, as Scena12_Frame's
// dispatches do; the fuzz swaps them for recorders. No divergence: each
// function is a faithful replacement, except that a dispatcher whose index
// lies outside its table (a negative state or run, or one reading the next
// table) aborts where the original would jump through its neighbour - the
// project's rule for an index past a table (round nine, section 6).
#include "game/scena_sc5.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/scena_sc5_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = scena_sc5::at;
using scena_sc5::CellEntry;
using scena_sc5::CellFindFn;
using scena_sc5::ObjectEntry;
using scena_sc5::PlaceFn;
using scena_sc5::VoidFn;

unsigned char& B(std::uint32_t a) { return *reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a)); }
std::uint16_t& W(std::uint32_t a) { return *reinterpret_cast<std::uint16_t*>(static_cast<std::uintptr_t>(a)); }
std::uint32_t& D(std::uint32_t a) { return *reinterpret_cast<std::uint32_t*>(static_cast<std::uintptr_t>(a)); }
std::int32_t S32(std::uint32_t a) { return static_cast<std::int32_t>(D(a)); }

unsigned Area() { return W(at::kArea); }
unsigned char Counter(unsigned k) { return B(at::kCounters + k); }
void SetCounter(unsigned k, unsigned char v) { B(at::kCounters + k) = v; }
void SetStep(unsigned char v) { B(at::kStep) = v; }
void SetRun(unsigned char v) { B(at::kRun) = v; }
void SetPass(unsigned char v) { B(at::kPassFlags) = v; }
unsigned char Request() { return B(at::kRequest); }
void SetRequest(unsigned char v) { B(at::kRequest) = v; }
bool WaitClear() { return W(at::kWait) == 0; }
bool MsgDone() { return (B(at::kMsgFlags) & 2) == 2; }
unsigned Sel() { return B(at::kSelector) & 0x7Fu; }
void MusicCurrent(unsigned char v) { B(at::kMusicCurrent) = v; }
// Field_ScriptFlags: `or byte [0x9039A2], v` / `xor word [0x9039A2], v`.
void ScriptOr(unsigned char v) { B(at::kScriptFlags) = static_cast<unsigned char>(B(at::kScriptFlags) | v); }
void ScriptXor(std::uint16_t v) { W(at::kScriptFlags) = static_cast<std::uint16_t>(W(at::kScriptFlags) ^ v); }

// The four counters 0x903848..B zeroed, in that order; the three from 1.
void ClearCounters() {
    for (unsigned k = 0; k < 4; ++k) SetCounter(k, 0);
}
void ClearCounters123() {
    for (unsigned k = 1; k < 4; ++k) SetCounter(k, 0);
}

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
void CallB(unsigned n) { SH_CALL(Scenario_CallB)(n); }
void DropIn(unsigned e) { SH_CALL(Party_DropIn)(e); }
void Kind2(unsigned char a) { SH_CALL(Kind2_Place)(a); }
void Sound(unsigned short id) { SH_CALL(Sound_PlayEffect)(id); }
void Transition(unsigned char k) { SH_CALL(Transition_Start)(k); }
void PartyPass() { SH_AT(VoidFn, at::kPartyPass)(); }

// The chapter's message: Msg_OpenScript(id), the request byte 2, the step.
void Say(unsigned short id, unsigned char step) {
    Msg(id);
    SetRequest(2);
    SetStep(step);
}

// A run's end: ScriptFlags_Clear40, then the step and the run 0.
void EndRun() {
    Clear40();
    SetStep(0);
    SetRun(0);
}
// The commoner end: ScriptFlags_Clear40, counters 1..3, the step and the run 0.
void EndRun123() {
    Clear40();
    ClearCounters123();
    SetStep(0);
    SetRun(0);
}
// Counters 1..3 zeroed before ScriptFlags_Clear40, then the step and the run.
void EndRun123First() {
    ClearCounters123();
    Clear40();
    SetStep(0);
    SetRun(0);
}

// Music_LoadFile(track), then Task_Sleep(1) until File_LoadDone.
void LoadMusic(unsigned track) {
    SH_CALL(Music_LoadFile)(track);
    while (SH_CALL(File_LoadDone)() == 0) SH_CALL(Task_Sleep)(1);
}

// An event battle at the leader: 0x532ED0(ObjTrio +0x34, +0x38, kind), then
// Field_StartEventBattle(kind) (group SE's).
void Battle(unsigned kind) {
    SH_AT(PlaceFn, at::kPartyPlace)(S32(at::kLeaderX), S32(at::kLeaderZ), kind);
    SH_CALL(Field_StartEventBattle)(kind);
}

// A table entry read in place, the index checked against the table.
std::uint32_t Entry(std::uint32_t table, int index, unsigned count, const char* who) {
    if (index < 0 || static_cast<unsigned>(index) >= count)
        bof3::Fatal("%s: index %d outside its table of %u at 0x%X", who, index, count, table);
    return D(table + 4 * static_cast<std::uint32_t>(index));
}

}  // namespace

// Exported with C linkage (the symbols.gen.h prototypes); no tail calls, so a
// Fatal's stack shows the dispatcher.
#define SC5_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// The vtable's slots and the dispatchers

// original 0x546390: slot 0, Field_ModeDispatch's call every field frame - a
// tail jump through Scena05_States on the s8 state 0x8034E2: 0 0x5646B0
// (group SC13's), 1 Scena05_EnterArea, 2 Scena05_Run.
SC5_EXPORT void __cdecl Scena05_Frame(void) {
    const int state = static_cast<signed char>(B(at::kState));
    reinterpret_cast<VoidFn>(static_cast<std::uintptr_t>(Entry(at::kStates, state, at::kStateCount, "Scena05_Frame")))();
}

// original 0x546AB0: state 2, a tail jump through Scena05_Runs on the s8 run
// MoveScript_Var7: 0, 2, 3, 12, 15 a bare ret (0x437CC0); 1, 4..11, 13, 14,
// 16..22 the runs below.
SC5_EXPORT void __cdecl Scena05_Run(void) {
    const int run = static_cast<signed char>(B(at::kRun));
    reinterpret_cast<VoidFn>(static_cast<std::uintptr_t>(Entry(at::kRuns, run, at::kRunCount, "Scena05_Run")))();
}

// original 0x54A2A0: slot 1, called by 0x56D6D0 with the object that
// triggered: Scena05_Objects[object +0x86] (object, the flag bits' pointer).
// The entry's eax is the original's answer; 0x56D6D0 does not read it.
SC5_EXPORT void __cdecl Scena05_ObjectTrigger(unsigned char* object) {
    const std::uint32_t bits = D(at::kFlagBits);
    const unsigned index = object[0x86];
    reinterpret_cast<ObjectEntry>(static_cast<std::uintptr_t>(
        Entry(at::kObjects, static_cast<int>(index), at::kObjectCount, "Scena05_ObjectTrigger")))(object, bits);
}

// original 0x54A800: slot 4, the cell hook (0x56D7A0, a cell (a, b) faced):
// 0x56D800(Scena05_CellRecords, 2, a, b) - the record the area and the cell
// match, or 0xFF; negative (as a signed byte) answers 0xFF; else a tail jump
// through Scena05_CellHooks with (a, b), whose al is the answer.
SC5_EXPORT unsigned char __cdecl Scena05_CellHook(unsigned a, unsigned b) {
    const unsigned char found = SH_AT(CellFindFn, at::kCellFind)(reinterpret_cast<const void*>(at::kCellRecords), 2, a, b);
    const int index = static_cast<signed char>(found);
    if (index < 0) return 0xFF;
    return reinterpret_cast<CellEntry>(static_cast<std::uintptr_t>(
        Entry(at::kCellHooks, index, at::kCellHookCount, "Scena05_CellHook")))(a, b);
}

// A step hook's hit: ScriptFlags_Set40, the four counters 0, the step and the
// run; 1.
namespace {
unsigned char Hit(unsigned char step, unsigned char run) {
    Set40();
    ClearCounters();
    SetStep(step);
    SetRun(run);
    return 1;
}
}  // namespace

// original 0x54A440: slot 2, the step hook (Scenario_StepHook, x and z
// 16.16, signed compares). By the area, re-read after each area's calls:
//   0x31: flag 9 set, 0xB clear, x 0x4D.0 or 0x4D.8, z 0x21.0..0x25.8: run 7;
//   0x34: flag 5 clear, x 0x93.0..0x94.8, z 0x6.0 or 0x6.8: run 4; flag 5
//         set, 9 clear, x 0x84.0..0x86.8, z 0xC.0, 0xC.8, 0x18.0 or 0x18.8:
//         run 4 step 0x14;
//   0x43: z 0x34.0 or 0x34.8, x 0x1D.0..0x1E.8: run 0xB (else 0, no later area);
//   0x44: flag 0 clear, z 0x24.0 or 0x24.8, x 0x19.0..0x1F.8: run 1;
//   0x4E: flag 0x3D set; flag 0x32 clear: x 0x28.8 or 0x29.0 with z
//         0x7E.0..0x81.8 run 0x13, x 0x1F.0 or 0x1F.8 with z 0x71.0..0x72.8
//         run 0x14; flag 0x32 set: that second rectangle, run 0x16.
// Each hit: ScriptFlags_Set40, the counters, the step (0 unless said) and the
// run, 1. Else 0.
SC5_EXPORT unsigned char __cdecl Scena05_StepHook(int x, int z) {
    if (Area() == 0x31) {
        if (Flag(9) && !Flag(0xB) && (x == 0x4D0000 || x == 0x4D8000) && z >= 0x210000 && z <= 0x258000) return Hit(0, 7);
    }
    if (Area() == 0x34) {
        if (!Flag(5)) {
            if (x >= 0x930000 && x <= 0x948000 && (z == 0x60000 || z == 0x68000)) return Hit(0, 4);
        } else if (!Flag(9) && x >= 0x840000 && x <= 0x868000) {
            // The original tests x <= 0x86.8 a second time before the 0x18
            // pair; it cannot fail here.
            if (z == 0xC0000 || z == 0xC8000 || z == 0x180000 || z == 0x188000) return Hit(0x14, 4);
        }
    }
    const unsigned area = Area();
    if (area == 0x43) {
        if ((z == 0x340000 || z == 0x348000) && x >= 0x1D0000 && x <= 0x1E8000) return Hit(0, 0xB);
        return 0;
    }
    if (area == 0x44) {
        if (!Flag(0) && (z == 0x240000 || z == 0x248000) && x >= 0x190000 && x <= 0x1F8000) return Hit(0, 1);
    }
    if (Area() != 0x4E) return 0;
    if (!Flag(0x3D)) return 0;
    const bool second = (x == 0x1F0000 || x == 0x1F8000) && z >= 0x710000 && z <= 0x728000;
    if (!Flag(0x32)) {
        if (x == 0x288000 || x == 0x290000) {
            if (z >= 0x7E0000 && z <= 0x818000) return Hit(0, 0x13);
            return 0;
        }
        if (second) return Hit(0, 0x14);
        return 0;
    }
    if (second) return Hit(0, 0x16);
    return 0;
}

// ===========================================================================
// Scena05_CellHooks: the cell hook's two entries, (a, b) in place, unread.

// Both: with flag 6 set and flag 9 clear, Flags_Set(the story flags, n),
// ScriptFlags_Set40, counters 2 and 3 0, Sound_PlayEffect(0x202), the step,
// the timer 0; 1. Else 0xFF.
namespace {
unsigned char CellCue(unsigned flag, unsigned char step) {
    if (!Flag(6)) return 0xFF;
    if (Flag(9)) return 0xFF;
    SH_CALL(Flags_Set)(Story(), flag);
    Set40();
    SetCounter(2, 0);
    SetCounter(3, 0);
    Sound(0x202);
    SetStep(step);
    W(at::kTimer) = 0;
    return 1;
}
}  // namespace

// original 0x54A830: entry 0 - story flag 0x26, step 1.
SC5_EXPORT unsigned char __cdecl Scena05_Cell0(void) { return CellCue(0x26, 1); }

// original 0x54A8A0: entry 1 - story flag 0x25, step 0x19.
SC5_EXPORT unsigned char __cdecl Scena05_Cell1(void) { return CellCue(0x25, 0x19); }

// ===========================================================================
// Scena05_Objects: the handlers of the object trigger (entries 1..8; entry 0
// a bare ret). Each starts a run: ScriptFlags_Set40, the run, the counters
// (from 0 or from 1) and the step 0.

namespace {
void StartRun(unsigned char run, bool all) {
    Set40();
    SetRun(run);
    if (all) SetCounter(0, 0);
    ClearCounters123();
    SetStep(0);
}
}  // namespace

// original 0x54A2C0: entry 1 - with flag 2 clear, Flags_Set(2) and run 0x15.
SC5_EXPORT void __cdecl Scena05_Object01(void) {
    if (Flag(2)) return;
    Set(2);
    StartRun(0x15, true);
}

// original 0x54A310: entry 2 - Flags_Set(0xB), run 8.
SC5_EXPORT void __cdecl Scena05_Object02(void) {
    Set(0xB);
    StartRun(8, true);
}

// original 0x54A350: entry 3 - run 0xC.
SC5_EXPORT void __cdecl Scena05_Object03(void) { StartRun(0xC, true); }

// original 0x54A380: entries 4 and 8 - run 0xD.
SC5_EXPORT void __cdecl Scena05_Object04(void) { StartRun(0xD, true); }

// original 0x54A3B0: entry 5 - run 0x10, counter 0 kept.
SC5_EXPORT void __cdecl Scena05_Object05(void) { StartRun(0x10, false); }

// original 0x54A3E0: entry 6 - run 0x11, counter 0 kept.
SC5_EXPORT void __cdecl Scena05_Object06(void) { StartRun(0x11, false); }

// original 0x54A410: entry 7 - run 0x12, counter 0 kept.
SC5_EXPORT void __cdecl Scena05_Object07(void) { StartRun(0x12, false); }

// ===========================================================================
// Two helpers the runs call directly

// original 0x54A1A0: counter 1 = 1 when any of the three party bytes
// 0x904062..64 is 5, then 2 when any is 6 (two passes, the second wins).
SC5_EXPORT void __cdecl Scena05_PartyCounter(void) {
    for (unsigned i = 0; i < 3; ++i)
        if (B(at::kParty + i) == 5) SetCounter(1, 1);
    for (unsigned i = 0; i < 3; ++i)
        if (B(at::kParty + i) == 6) SetCounter(1, 2);
}

// original 0x54A1E0: Sprite_Current = ObjTrio record 0; Effect_FindFree into
// Sprite_Current +0xB (the pointer re-read after the call); a slot taken gets
// +0 = 1, +5 = 6, +6 = kind + 0x70, the dword +0xC = 0, the dword +0x10 =
// Scena05_MemberBytes[the first member 0x904062] (s8, unchecked: the byte
// table is 8 long and the member a byte), the words +0x2E / +0x30 from the
// sprite's. Each store re-reads the slot from the sprite, as the original.
SC5_EXPORT void __cdecl Scena05_SpawnMember(unsigned char kind) {
    D(at::kSpriteCurrent) = at::kObjTrio;
    const unsigned char slot = SH_CALL(Effect_FindFree)();
    unsigned char* const sprite = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(D(at::kSpriteCurrent)));
    sprite[0xB] = slot;
    if (sprite[0xB] == 0xFF) return;
    auto effect = [&]() { return &B(at::kEffects + static_cast<std::uint32_t>(sprite[0xB]) * at::kEffectStride); };
    effect()[0] = 1;
    effect()[5] = 6;
    effect()[6] = static_cast<unsigned char>(kind + 0x70);
    *reinterpret_cast<std::uint32_t*>(effect() + 0xC) = 0;
    const std::int32_t lift = static_cast<signed char>(B(at::kMemberBytes + B(at::kParty)));
    *reinterpret_cast<std::int32_t*>(effect() + 0x10) = lift;
    *reinterpret_cast<std::uint16_t*>(effect() + 0x2E) = *reinterpret_cast<const std::uint16_t*>(sprite + 0x2E);
    *reinterpret_cast<std::uint16_t*>(effect() + 0x30) = *reinterpret_cast<const std::uint16_t*>(sprite + 0x30);
}

// ===========================================================================
// State 1

namespace {

// The body of Scena05_EnterArea: by the area, re-read before each test.
void EnterAreaBody() {
    // 0x2D: flag 0x32 set, 0x3E clear - Flags_Set(0x3E), 0x56D6F0.
    if (Area() == 0x2D && Flag(0x32) && !Flag(0x3E)) {
        Set(0x3E);
        SH_AT(VoidFn, at::kSetBit80)();
    }
    // 0x31: flag 9 set, 0xA clear - run 6, the step and counters 0; by the
    // selector 1 Party_DropIn(4), 2 call B 3 / call A 5 / drop-in 4, 6 call
    // B 3 / call A 6 / drop-in 4.
    if (Area() == 0x31 && Flag(9) && !Flag(0xA)) {
        const unsigned sel = Sel();
        SetRun(6);
        SetStep(0);
        ClearCounters();
        if (sel == 1) {
            DropIn(4);
        } else if (sel == 2) {
            CallB(3);
            CallA(5);
            DropIn(4);
        } else if (sel == 6) {
            CallB(3);
            CallA(6);
            DropIn(4);
        }
    }
    // 0x34: flag 9 clear - by counter 2, 1..5.
    if (Area() == 0x34 && !Flag(9)) {
        switch (Counter(2)) {
        case 1:
            if (!Flag(3)) SetCounter(0, 0);
            break;
        case 3:
            CallB(0);
            ClearCounters();
            break;
        case 4:
            SetCounter(0, 0);
            SH_CALL(Flags_Clear)(Story(), 0x26);
            break;
        case 5:
            SetPass(0x1F);
            break;
        default:   // 2 and the rest: nothing
            break;
        }
    }
    // 0x41: flag 0xC clear - run 0; else flag 0xD clear - run 9; either, the
    // step and the counters 0.
    if (Area() == 0x41) {
        bool start = false;
        if (!Flag(0xC)) {
            SetRun(0);
            start = true;
        } else if (!Flag(0xD)) {
            SetRun(9);
            start = true;
        }
        if (start) {
            SetStep(0);
            ClearCounters();
        }
    }
    // 0x44: counter 2 not 1 ends here; flag 1 clear - run 1, step 2.
    if (Area() == 0x44) {
        if (Counter(2) != 1) return;
        if (!Flag(1)) {
            SetRun(1);
            SetStep(2);
        }
    }
    // 0x45: counter 2 not 0 ends here; flag 0x10 clear - music 1 loaded and
    // waited for, the counters 0, run 9, step 0.
    if (Area() == 0x45) {
        if (Counter(2) != 0) return;
        if (!Flag(0x10)) {
            LoadMusic(1);
            ClearCounters();
            SetRun(9);
            SetStep(0);
        }
    }
    // 0x4E: by counter 2, 0..6 (above 6 ends here).
    if (Area() == 0x4E) {
        const unsigned c = Counter(2);
        if (c > 6) return;
        switch (c) {
        case 0:
            if (Flag(0x18)) break;
            LoadMusic(0x4A);
            SH_CALL(Music_Play)(0x4B, 8);
            SetRun(0xA);
            SetStep(0);
            ClearCounters();
            B(at::kBitsE53) = static_cast<unsigned char>(B(at::kBitsE53) & 0xFE);
            B(at::kBitsDAF) = static_cast<unsigned char>(B(at::kBitsDAF) | 1);
            break;
        case 1:
            if (Flag(0x14)) break;
            SetRun(0xA);
            SetStep(4);
            ClearCounters();
            Sound(0x200);
            break;
        case 2:
        case 3:
            if (Flag(0x18)) break;
            LoadMusic(0x4A);
            SH_CALL(Music_Play)(0x4B, 8);
            SetRun(0xA);
            SetStep(0);
            ClearCounters();
            CallA(c == 2 ? 5 : 6);
            DropIn(0);
            break;
        case 4:
            if (Flag(0x14)) break;
            CallB(2);
            SetRun(0xA);
            SetStep(4);
            ClearCounters();
            Sound(0x200);
            CallA(7);
            B(at::kBitsDAF) = static_cast<unsigned char>(B(at::kBitsDAF) & 0xFE);
            B(at::kBitsE53) = static_cast<unsigned char>(B(at::kBitsE53) | 1);
            DropIn(4);
            break;
        case 5:
            CallB(2);
            SetRun(0xD);
            SetStep(6);
            ClearCounters();
            Sound(0x200);
            CallA(7);
            B(at::kBitsDAF) = static_cast<unsigned char>(B(at::kBitsDAF) & 0xFE);
            B(at::kBitsE53) = static_cast<unsigned char>(B(at::kBitsE53) | 1);
            DropIn(0xF);
            break;
        default:   // 6
            CallB(3);
            SetRun(0xD);
            SetStep(6);
            ClearCounters();
            Sound(0x200);
            CallA(5);
            B(at::kBitsE53) = static_cast<unsigned char>(B(at::kBitsE53) & 0xFE);
            B(at::kBitsDAF) = static_cast<unsigned char>(B(at::kBitsDAF) | 1);
            DropIn(0x10);
            break;
        }
    }
    // 0x4F: counter 2 not 0 ends here; flag 0x1D clear - the counters 0.
    if (Area() == 0x4F) {
        if (Counter(2) != 0) return;
        if (!Flag(0x1D)) ClearCounters();
    }
    // 0x50: counter 2 not 1 ends here; flag 0x2F clear - music 0x66 loaded
    // and waited for (not played).
    if (Area() == 0x50) {
        if (Counter(2) != 1) return;
        if (!Flag(0x2F)) LoadMusic(0x66);
    }
    // 0x51: counter 2 1 - the counters 0; 2 - the counters 0 and
    // 0x532ED0(0xC9.0, 0xE.0, 0x15).
    if (Area() == 0x51) {
        const unsigned c = Counter(2);
        if (c == 1) {
            ClearCounters();
        } else if (c == 2) {
            ClearCounters();
            SH_AT(PlaceFn, at::kPartyPlace)(0xC90000, 0xE0000, 0x15);
        }
    }
}

}  // namespace

// original 0x5463A0: state 1, the chapter's entry to an area (above); every
// way out stores state 2.
SC5_EXPORT void __cdecl Scena05_EnterArea(void) {
    EnterAreaBody();
    B(at::kState) = 2;
}

// ===========================================================================
// The runs (Scena05_Runs entries), each a switch on the step 0x8034E5

// original 0x546AC0: run 1 - 0 message 5; 1 when the message closed, counter
// 2 = 1, flag 0, Field_ChangeArea(0x44, 0x1D.0, 0x1E.0, 0x80); 2 at counter 0
// 0x2D, flag 1, the run's end (counters 1..3).
SC5_EXPORT void __cdecl Scena05_Run1(void) {
    switch (B(at::kStep)) {
    case 0:
        Say(5, 1);
        break;
    case 1:
        if (Request() == 2) break;
        SetCounter(2, 1);
        SetStep(2);
        Set(0);
        ChangeArea(0x44, 0x1D0000, 0x1E0000, 0x80);
        break;
    case 2:
        if (Counter(0) != 0x2D) break;
        Set(1);
        EndRun123();
        break;
    default:
        break;
    }
}

// original 0x546B80: run 4 - a battle scene in area 0x34 (a two-level switch
// over steps 0..0x15).
SC5_EXPORT void __cdecl Scena05_Run4(void) {
    switch (B(at::kStep)) {
    case 0:
        SetCounter(2, 2);
        SetStep(1);
        ChangeArea(0x34, 0x3E0000, 0x270000, 0x84);
        MusicCurrent(0xFF);
        break;
    case 1:
        if (Counter(0) != 3) break;
        Kind2(0);
        SetStep(2);
        break;
    case 2:
        if (Counter(0) != 0xD) break;
        SH_AT(PlaceFn, at::kPartyPlace)(0x3E0000, 0x200000, 0x11);
        SetStep(3);
        break;
    case 3:
        if (Counter(0) != 0xF) break;
        SH_CALL(Field_StartEventBattle)(0x11);
        SetStep(7);
        break;
    case 7:
        if (Counter(0) != 0x14) break;
        DropIn(0);
        SetStep(8);
        break;
    case 8:
        if (Counter(0) != 0x16) break;
        SetCounter(2, 3);
        SetStep(9);
        Set(5);
        ChangeArea(0x34, 0x850000, 0xF0000, 0x85);
        MusicCurrent(0x3B);
        break;
    case 9:
        if (Counter(0) != 8) break;
        ScriptOr(0x20);
        Set(6);
        Clear40();
        SetRun(5);
        SetCounter(1, 0);
        SetCounter(2, 0);
        SetStep(0);
        break;
    case 0x14:
        Say(0xB, 0x15);
        break;
    case 0x15:
        if (Request() == 2) break;
        Clear40();
        SetRun(5);
        SetCounter(0, 0);
        SetCounter(2, 0);
        SetStep(0);
        break;
    default:   // 4..6, 0xA..0x13 and above 0x15
        break;
    }
}

// original 0x546D60: run 5 - steps 1..6 and 0x19..0x23 (a two-level switch
// on the step less 1).
SC5_EXPORT void __cdecl Scena05_Run5(void) {
    switch (B(at::kStep)) {
    case 1:
        SetStep(2);
        break;
    case 2:
        Say(0xD, 3);
        break;
    case 3:
        if (Request() == 2) break;
        SetCounter(0, 0x1E);
        SetStep(4);
        break;
    case 4:
        SetStep(5);
        DropIn(6);
        break;
    case 5:
        if (Counter(0) != 0x23) break;
        SetCounter(2, 4);
        SetCounter(1, 1);
        SetStep(6);
        ChangeArea(0x34, 0x850000, 0x100000, 0x87);
        MusicCurrent(0x3B);
        break;
    case 6:
        Clear40();
        SetRun(5);
        SetCounter(0, 0);
        SetCounter(2, 0);
        SetStep(0);
        break;
    case 0x19:
        Say(0xD, 0x1A);
        break;
    case 0x1A:
        if (Request() == 2) break;
        SetCounter(0, 0x28);
        Kind2(1);
        SetStep(0x1B);
        break;
    case 0x1B:
        DropIn(8);
        SetStep(0x1C);
        break;
    case 0x1C:
        if (Counter(0) != 0x2C) break;
        SetStep(0x1D);
        SetCounter(0, 0x2D);
        SH_CALL(Music_FadeOutStop)(0xA);
        break;
    case 0x1D:
        if (Counter(0) != 0x32) break;
        SH_CALL(Music_Play)(0x39, 8);
        SetStep(0x1F);
        break;
    case 0x1F:
        if (Counter(0) != 0x50) break;
        SH_CALL(Music_FadeOutStop)(0xA);
        Transition(0xD);
        SetStep(0x20);
        break;
    case 0x20:
        if (!WaitClear()) break;
        SetPass(0);
        SH_CALL(Music_Play)(0x69, 0xA);
        Say(0x13, 0x21);
        break;
    case 0x21:
        if (Request() == 2) break;
        Set(8);
        CallA(0);
        SetStep(0x22);
        SetCounter(0, 0x51);
        break;
    case 0x22:
        if (Request() != 0) break;
        SetCounter(2, 5);
        SetStep(0x23);
        ChangeArea(0x34, 0x850000, 0x150000, 0x89);
        MusicCurrent(0x38);
        break;
    case 0x23:
        Set(9);
        ScriptXor(0x20);
        Clear40();
        ClearCounters();
        SetStep(0);
        SetRun(0);
        break;
    default:
        break;
    }
}

// original 0x547040: run 6 - step 0 at counter 0 0x18: flag 0xA, the run's
// end (counters 1..3).
SC5_EXPORT void __cdecl Scena05_Run6(void) {
    if (B(at::kStep) != 0) return;
    if (Counter(0) != 0x18) return;
    Set(0xA);
    EndRun123();
}

namespace {
// Runs 7, 11, 19, 22: step 0 message `id`; step 1 when it closed, the run's end.
void Talk(unsigned short id) {
    const unsigned char step = B(at::kStep);
    if (step == 0) {
        Say(id, 1);
    } else if (step == 1) {
        if (Request() == 2) return;
        EndRun();
    }
}
}  // namespace

// original 0x547090: run 7 - message 0x26.
SC5_EXPORT void __cdecl Scena05_Run7(void) { Talk(0x26); }

// original 0x5470E0: run 8 - steps 0..6: a transition, the party pass and a
// stream, a hop into area 0x31, then at counter 0 0x28 flags 0xC, 0xE, 0xF
// set and 0x3F cleared and a change into area 0x45.
SC5_EXPORT void __cdecl Scena05_Run8(void) {
    switch (B(at::kStep)) {
    case 0:
        SetStep(1);
        Set(0xB);
        ScriptOr(8);
        Transition(0xD);
        break;
    case 1:
        if (!WaitClear()) break;
        SetPass(0);
        PartyPass();
        SetStep(2);
        SH_CALL(Music_FadeOutStop)(0x1E);
        break;
    case 2:
        SH_CALL(Sound_LoadStream)(0);
        SetStep(3);
        break;
    case 3:
        if (SH_CALL(Sound_StreamDone)() == 0) break;
        SetStep(4);
        break;
    case 4:
        SetStep(5);
        ChangeArea(0x31, 0x440000, 0x210000, 0x80);
        SH_CALL(Music_Play)(0x10, 8);
        break;
    case 5:
        SetStep(6);
        SetPass(0x1F);
        break;
    case 6:
        if (Counter(0) != 0x28) break;
        SetStep(7);
        Set(0xC);
        Set(0xE);
        Set(0xF);
        Clr(0x3F);
        ScriptXor(8);
        ChangeArea(0x45, 0x1E0000, 0x190000, 0x80);
        MusicCurrent(0xFF);
        break;
    default:
        break;
    }
}

// original 0x547240: run 9 - steps 0..3 and 0xA..0xC. Counter 0 is read once
// on entry for steps 1 and 2 (step 1 falls into step 2's test).
SC5_EXPORT void __cdecl Scena05_Run9(void) {
    const unsigned char step = B(at::kStep);
    if (step > 0xC) return;
    const unsigned char c0 = Counter(0);
    switch (step) {
    case 0:
        SH_CALL(Music_Play)(1, 8);
        SetStep(1);
        break;
    case 1:
        if (c0 == 5) {
            SetCounter(0, 6);
            SetStep(2);
            break;
        }
        [[fallthrough]];
    case 2:
        if (c0 != 8) break;
        SetStep(3);
        Set(0x10);
        ChangeArea(0x2D, 0x3C0000, 0x2E0000, 0x83);
        MusicCurrent(1);
        break;
    case 3:
        Kind2(3);
        SetStep(0xA);
        break;
    case 0xA:
        if (Counter(0) != 0xC) break;
        SetStep(0xB);
        SetCounter(0, 0x14);
        Set(0x11);
        ScriptOr(8);
        ChangeArea(0x32, 0x80000, 0x260000, 0x80);
        break;
    case 0xB:
        if (Counter(0) != 0x1C) break;
        Kind2(0);
        SetStep(0xC);
        break;
    case 0xC: {
        if (Counter(0) != 0x1D) break;
        unsigned char* const bits = Bits();
        ScriptXor(8);
        SH_CALL(Flags_Set)(bits, 0x12);
        const unsigned sel = Sel();
        if (sel == 1) {
            SetCounter(2, 0);
            Set(0x13);
            ChangeArea(0x4E, 0x240000, 0xAD0000, 0x80);
            MusicCurrent(0xFF);
        } else if (sel == 2 || sel == 6) {
            SetCounter(2, sel == 2 ? 2 : 3);
            Set(0x13);
            CallB(3);
            ChangeArea(0x4E, 0x240000, 0xAD0000, 3);
            MusicCurrent(0xFF);
        }
        break;
    }
    default:   // 4..9
        break;
    }
}

// original 0x547430: run 10 - steps 0, 1, 4 at counter 0 0x23 / 0x23 / 3.
SC5_EXPORT void __cdecl Scena05_Run10(void) {
    switch (B(at::kStep)) {
    case 0:
        if (Counter(0) != 0x23) break;
        SetStep(1);
        break;
    case 1:
        if (Counter(0) != 0x23) break;
        SetStep(4);
        Set(0x18);
        if (!Flag(0x17)) {
            SetCounter(2, 4);
            ChangeArea(0x4E, 0x270000, 0xAD0000, 3);
            MusicCurrent(0xFF);
        }
        if (Flag(0x16)) break;
        SetCounter(2, 1);
        ChangeArea(0x4E, 0x270000, 0xAD0000, 0x83);
        MusicCurrent(0xFF);
        break;
    case 4:
        if (Counter(0) != 3) break;
        Set(0x19);
        Set(0x14);
        Set(0x1A);
        EndRun123First();
        break;
    default:
        break;
    }
}

// original 0x547560: run 11 - message 0.
SC5_EXPORT void __cdecl Scena05_Run11(void) { Talk(0); }

namespace {
// Runs 13's steps 3 and 0x11: at counter 0 0x2B (the message closed) counter
// 0 0x2D and step 4; at 0x2C, counter 0 0x32 and step `next`.
void Choice(unsigned char next) {
    const unsigned char c0 = Counter(0);
    if (c0 == 0x2B) {
        if (Request() == 2) return;
        SetCounter(0, 0x2D);
        SetStep(4);
    } else if (c0 == 0x2C) {
        if (Request() == 2) return;
        SetCounter(0, 0x32);
        SetStep(next);
    }
}
// Run 13's steps 5 and 0x12: counter 2, step 6, one flag set and the other
// cleared, Field_ChangeArea(0x4E, 0x27.0, 0xAD.0, 3).
void Pick13(unsigned char c2, unsigned set, unsigned clear) {
    SetCounter(2, c2);
    SetStep(6);
    Set(set);
    Clr(clear);
    ChangeArea(0x4E, 0x270000, 0xAD0000, 3);
}
}  // namespace

// original 0x5475B0: run 13 - steps 0..6, 0xF, 0x11, 0x12 (a two-level
// switch): a choice after a message, by the selector.
SC5_EXPORT void __cdecl Scena05_Run13(void) {
    switch (B(at::kStep)) {
    case 0:
        if (!MsgDone()) break;
        if (Sel() == 1) SetStep(0xF);
        else if (Sel() == 2) SetStep(1);
        break;
    case 1:
        if (!MsgDone()) break;
        Say(0x20, 2);
        break;
    case 2:
        if (Request() == 2) break;
        SetStep(3);
        break;
    case 3:
        Choice(5);
        break;
    case 4:
        if (Counter(0) != 0x2E) break;
        if (Request() == 2) break;
        SetCounter(0, 0);
        EndRun123First();
        break;
    case 5:
        if (Counter(0) != 0x33) break;
        if (Request() == 2) break;
        Pick13(6, 0x17, 0x16);
        break;
    case 6:
        if (Counter(0) != 2) break;
        EndRun123First();
        break;
    case 0xF:
        if (!MsgDone()) break;
        Say(0x1F, 0x11);
        break;
    case 0x11:
        Choice(0x12);
        break;
    case 0x12:
        if (Counter(0) != 0x33) break;
        if (Request() == 2) break;
        Pick13(5, 0x16, 0x17);
        break;
    default:
        break;
    }
}

// original 0x547820: run 14 - steps 0..4: a message by the selector, then
// at counter 0 2 (closed) message 0x25, or at 0xA straight to step 4; the
// run's end (all four counters), step 4 with flag 0x1A at counter 0 0x14.
SC5_EXPORT void __cdecl Scena05_Run14(void) {
    switch (B(at::kStep)) {
    case 0:
        if (!MsgDone()) break;
        if (Sel() == 1) Say(0x23, 1);
        else if (Sel() == 2) Say(0x24, 1);
        break;
    case 1: {
        const unsigned char c0 = Counter(0);
        if (c0 == 2) {
            if (Request() == 2) break;
            SetStep(2);
        } else if (c0 == 0xA) {
            SetStep(4);
        }
        break;
    }
    case 2:
        Say(0x25, 3);
        break;
    case 3:
        if (Request() == 2) break;
        Clear40();
        ClearCounters();
        SetStep(0);
        SetRun(0);
        break;
    case 4:
        if (Counter(0) != 0x14) break;
        Set(0x1A);
        Clear40();
        ClearCounters();
        SetStep(0);
        SetRun(0);
        break;
    default:
        break;
    }
}

// ===========================================================================
// Run 16 (0x547920), the chapter's longest: a two-level switch over steps
// 0..0x66, whose cases pick by counter 3 (0x90384B, a code of the party's
// pair set at steps 5 / 6: 1..6 and 0xB..0x10) and by the first member
// 0x904062 (0..6).

namespace {

// Field_ChangeArea(0x4F, x, z, flags) - the area every run 16 change goes to.
void To4F(int x, int z, unsigned flags) { ChangeArea(0x4F, x, z, flags); }
void To4FGate(unsigned flags) { To4F(0xC30000, 0xE0000, flags); }
void To4FHall(unsigned flags) { To4F(0x480000, 0x80000, flags); }

// Two call-table B entries, then the change into 0x4F at (0xC3.0, 0xE.0).
void GateB(unsigned a, unsigned b, unsigned flags) {
    CallB(a);
    CallB(b);
    To4FGate(flags);
}
// Two call-table A entries, then the change into 0x4F at (0x48.0, 0x8.0).
void HallA(unsigned a, unsigned b, unsigned flags) {
    CallA(a);
    CallA(b);
    To4FHall(flags);
}
// The pass flags 0 before (true) or after (false) Transition_Start(0xD).
void Fade(bool before) {
    if (before) SetPass(0);
    Transition(0xD);
    if (!before) SetPass(0);
}

// Step 0x36 / 0x40's message by flags 0x1E / 0x1F and the first member; the
// step after, message or not.
void MemberMessage(unsigned char next) {
    const unsigned party = B(at::kParty);
    int id = -1;
    if (!Flag(0x1E)) {
        static const short kIds[7] = {0x3E, 0x3F, -1, -1, -1, 0x40, 0x41};
        if (party <= 6) id = kIds[party];
    } else if (!Flag(0x1F)) {
        static const short kIds[7] = {0x49, 0x4A, -1, -1, -1, 0x4B, 0x4C};
        if (party <= 6) id = kIds[party];
    } else {
        static const short kIds[7] = {0x4D, 0x4E, 0x4F, 0x4F, 0x4F, 0x4F, 0x50};
        if (party <= 6) id = kIds[party];
    }
    if (id >= 0) {
        Msg(static_cast<unsigned short>(id));
        SetRequest(2);
    }
    SetStep(next);
}
// Step 0x38 / 0x42's battle by flags 0x1E / 0x1F: 0x12, 0x13 or 0x14.
void FlagBattle(unsigned char next) {
    unsigned kind = 0x12;
    if (Flag(0x1E)) kind = Flag(0x1F) ? 0x14 : 0x13;
    Battle(kind);
    SetStep(next);
}
// A message by the first member from a table of seven (-1 none), the request
// byte 2 and the step; nothing for a member above 6.
void PartyMessage(const short (&ids)[7], unsigned char next) {
    const unsigned party = B(at::kParty);
    if (party > 6) return;
    if (ids[party] < 0) return;
    Say(static_cast<unsigned short>(ids[party]), next);
}
// The change into 0x4F at (0xC3.0, 0xE.0) by the first member: 0 0x8A,
// 1 0x8B, 2..5 0x8C, 6 0x8D; nothing above 6.
void PartyGate() {
    const unsigned party = B(at::kParty);
    if (party > 6) return;
    static const unsigned char kFlags[7] = {0x8A, 0x8B, 0x8C, 0x8C, 0x8C, 0x8C, 0x8D};
    To4FGate(kFlags[party]);
}
// A message by counter 3 when the request byte is not 2: the four ids for
// the code pairs (1, 2), (3, 4), (5, 6), (0xB, 0xC), (0xD, 0xE), (0xF, 0x10)
// as the step's table has them (-1 none; 7..0xA and the rest none).
void CodeMessage(const short (&ids)[16], unsigned char next) {
    const unsigned c3 = Counter(3);
    if (c3 < 1 || c3 > 0x10) return;
    const short id = ids[c3 - 1];
    if (id < 0) return;
    if (Request() == 2) return;
    Say(static_cast<unsigned short>(id), next);
}

// Step 0x34's and 0x3E's exits by counter 3: Fade (the pass flags before or
// after), two call-table B entries, the gate change.
struct Exit { bool before; unsigned char a, b, flags; };
void CodeExit(const Exit (&exits)[16]) {
    const unsigned c3 = Counter(3);
    if (c3 < 1 || c3 > 0x10) return;
    const Exit& e = exits[c3 - 1];
    if (e.flags == 0) return;
    Fade(e.before);
    GateB(e.a, e.b, e.flags);
}
// Steps 0x47's and 0x65's exits by counter 3: two call-table A entries and
// the hall change.
struct HallExit { unsigned char a, b, flags; };
void CodeHall(const HallExit (&exits)[16]) {
    const unsigned c3 = Counter(3);
    if (c3 < 1 || c3 > 0x10) return;
    const HallExit& e = exits[c3 - 1];
    if (e.flags == 0) return;
    HallA(e.a, e.b, e.flags);
}

constexpr short N = -1;
const short kMsg12[16] = {0x33, 0x33, 0x34, 0x34, 0x35, 0x35, N, N, N, N, 0x33, 0x33, 0x34, 0x34, 0x36, 0x36};
const short kMsg32[16] = {0x34, 0x35, 0x33, 0x35, 0x33, 0x34, N, N, N, N, 0x34, 0x36, 0x33, 0x36, 0x33, 0x34};
const short kMsg3C[16] = {0x35, 0x34, 0x35, 0x33, 0x34, 0x33, N, N, N, N, 0x36, 0x34, 0x36, 0x33, 0x34, 0x33};
const short kParty16[7] = {0x3E, 0x3F, N, N, N, 0x40, 0x41};
const short kParty1F[7] = {0x49, 0x4A, 0x4B, 0x4B, 0x4B, 0x4B, 0x4C};
const short kParty25[7] = {0x4D, 0x4E, 0x4F, 0x4F, 0x4F, 0x4F, 0x50};
const Exit kExit34[16] = {
    {true, 4, 2, 0x8B},  {true, 4, 1, 0x8C},  {true, 1, 2, 0x8A},  {false, 1, 4, 0x8C}, {true, 2, 1, 0x8A},
    {true, 2, 4, 0x8B},  {}, {}, {}, {},
    {true, 4, 3, 0x8B},  {false, 4, 1, 0x8D}, {true, 1, 3, 0x8A},  {false, 1, 4, 0x8D}, {true, 3, 1, 0x8A},
    {true, 3, 4, 0x8B},
};
const Exit kExit3E[16] = {
    {true, 4, 1, 0x8C},  {true, 4, 2, 0x8B},  {true, 1, 4, 0x8C},  {false, 1, 2, 0x8A}, {true, 2, 4, 0x8B},
    {true, 2, 1, 0x8A},  {}, {}, {}, {},
    {true, 4, 1, 0x8D},  {false, 4, 3, 0x8B}, {true, 1, 4, 0x8D},  {false, 1, 3, 0x8A}, {true, 3, 4, 0x8B},
    {true, 3, 1, 0x8A},
};
const HallExit kHall47[16] = {
    {1, 2, 0x9A}, {1, 2, 0x9B}, {2, 4, 0x9C}, {2, 4, 0x9D}, {1, 4, 0x9E}, {1, 4, 0x9F}, {}, {}, {}, {},
    {1, 3, 0x94}, {1, 3, 0x95}, {3, 4, 0x96}, {3, 4, 0x97}, {1, 4, 0x98}, {1, 4, 0x99},
};
const HallExit kHall65[16] = {
    {4, 2, 0xA6}, {4, 1, 0xA7}, {1, 2, 0xA8}, {1, 4, 0xA9}, {2, 1, 0xAA}, {2, 4, 0xAB}, {}, {}, {}, {},
    {4, 3, 0xA0}, {4, 1, 0xA1}, {1, 3, 0xA2}, {1, 4, 0xA3}, {1, 3, 0xA4}, {3, 4, 0xA5},
};

// Step 5 / 6's code from the pair 0x904065 / 0x904066: (first, second) ->
// counter 3; step 0xF on a match.
struct Pair { unsigned char a, b, code; };
void PairCode(const Pair (&pairs)[6]) {
    const unsigned char a = B(at::kPartyA);
    const unsigned char b = B(at::kPartyB);
    for (const Pair& p : pairs)
        if (p.a == a && p.b == b) {
            SetCounter(3, p.code);
            SetStep(0xF);
            return;
        }
}
const Pair kPairs5[6] = {{0, 1, 1}, {0, 5, 2}, {1, 0, 3}, {1, 5, 4}, {5, 0, 5}, {5, 1, 6}};
const Pair kPairs6[6] = {{0, 1, 0xB}, {0, 6, 0xC}, {1, 0, 0xD}, {1, 6, 0xE}, {6, 0, 0xF}, {6, 1, 0x10}};

}  // namespace

// original 0x547920: run 16 (above). Steps not listed return.
SC5_EXPORT void __cdecl Scena05_Run16(void) {
    switch (B(at::kStep)) {
    case 0: {
        if (!MsgDone()) break;
        const unsigned char c0 = Counter(0);
        SetCounter(0, 0);
        if (c0 == 0x14) Say(0x20, 2);
        else Say(0x24, 1);
        break;
    }
    case 1:
        if (Request() == 2) break;
        Say(0x20, 2);
        break;
    case 2:
        if (Request() == 2) break;
        SetStep(Counter(0) != 0x14 ? 4 : 3);
        break;
    case 3:
        Clear40();
        ClearCounters123();
        SetStep(0);
        SetRun(0);
        break;
    case 4:
        if (Sel() == 1) SetStep(5);
        else if (Sel() == 2) SetStep(6);
        break;
    case 5:
        PairCode(kPairs5);
        break;
    case 6:
        PairCode(kPairs6);
        break;
    case 0xF:
        Say(0x32, 0x10);
        break;
    case 0x10:
        if (Request() == 2) break;
        SetCounter(0, 0xA);
        SetStep(0x11);
        break;
    case 0x11: {
        if (Counter(0) != 0xB) break;
        SetStep(0x12);
        Set(0x1D);
        SH_CALL(Music_FadeOutStop)(0xA);
        ScriptOr(8);
        SH_CALL(Scena05_PartyCounter)();
        const bool one = Counter(1) == 1;
        SetCounter(0, 0);
        SetCounter(1, 0);
        const unsigned c3 = Counter(3);
        if (one) {
            if (c3 >= 1 && c3 <= 6) To4F(0x480000, 0x70000, 0x87 + (c3 - 1) / 2);
        } else {
            if (c3 >= 0xB && c3 <= 0x10) To4F(0x480000, 0x70000, 0x84 + (c3 - 0xB) / 2);
        }
        break;
    }
    case 0x12:
        Sound(0x208);
        CodeMessage(kMsg12, 0x13);
        break;
    case 0x13:
        if (Request() == 2) break;
        SetCounter(0, 1);
        SetStep(0x14);
        break;
    case 0x14:
        if (Counter(0) != 0xA) break;
        SetStep(0x15);
        To4F(0x5C0000, 0x60000, 0x8E);
        break;
    case 0x15: {
        if (Counter(0) != 0x14) break;
        SetStep(0x16);
        switch (Counter(3)) {
        case 1: case 2: GateB(1, 2, 0x8A); break;
        case 3: case 4: GateB(2, 4, 0x8B); break;
        case 5: case 6: GateB(1, 4, 0x8C); break;
        case 0xB: case 0xC: GateB(1, 3, 0x8A); break;
        case 0xD: case 0xE: GateB(3, 4, 0x8B); break;
        case 0xF: case 0x10: GateB(1, 4, 0x8D); break;
        default: break;
        }
        break;
    }
    case 0x16:
        if (Counter(0) != 0x16) break;
        PartyMessage(kParty16, 0x17);
        break;
    case 0x17:
    case 0x20:
    case 0x26:
    case 0x37:
    case 0x41: {
        // The message closed: counter 0 0x17, the next step.
        if (Request() == 2) break;
        static const unsigned char kNext[] = {0x18, 0x21, 0x27, 0x38, 0x42};
        const unsigned char step = B(at::kStep);
        SetCounter(0, 0x17);
        SetStep(kNext[step == 0x17 ? 0 : step == 0x20 ? 1 : step == 0x26 ? 2 : step == 0x37 ? 3 : 4]);
        break;
    }
    case 0x18:
        if (Counter(0) != 0x1B) break;
        Sound(0x209);
        Battle(0x12);
        SetStep(0x19);
        break;
    case 0x1A:
        DropIn(0x30);
        Kind2(2);
        SetStep(0x5A);
        break;
    case 0x1B:
        DropIn(0x30);
        SetCounter(0, 0xB);
        SetStep(0x1E);
        Set(0x1E);
        To4F(0x5A0000, 0x28000, 0x8E);
        break;
    case 0x1C:
        Transition(4);
        SetStep(0x46);
        break;
    case 0x1E:
        if (Counter(0) != 0x14) break;
        SetStep(0x1F);
        PartyGate();
        break;
    case 0x1F:
        if (Counter(0) != 0x16) break;
        PartyMessage(kParty1F, 0x20);
        break;
    case 0x21:
        if (Counter(0) != 0x1B) break;
        Battle(0x13);
        SetStep(0x22);
        break;
    case 0x23:
        DropIn(0x30);
        SetCounter(0, 0xC);
        SetStep(0x24);
        Set(0x1F);
        To4F(0x5A0000, 0x80000, 0x8E);
        break;
    case 0x24:
        if (Counter(0) != 0x14) break;
        SetStep(0x25);
        PartyGate();
        break;
    case 0x25:
        if (Counter(0) != 0x16) break;
        PartyMessage(kParty25, 0x26);
        break;
    case 0x27:
        if (Counter(0) != 0x1B) break;
        Battle(0x14);
        SetStep(0x28);
        break;
    case 0x29:
        Set(0x20);
        Set(0x22);
        SetStep(0x1A);
        break;
    case 0x32:
        CodeMessage(kMsg32, 0x33);
        break;
    case 0x33:
    case 0x3D:
        if (Request() == 2) break;
        SetCounter(0, 1);
        SetStep(B(at::kStep) == 0x33 ? 0x34 : 0x3E);
        break;
    case 0x34:
        if (Counter(0) != 0xA) break;
        SetStep(0x35);
        CodeExit(kExit34);
        break;
    case 0x35:
    case 0x3F:
        SetPass(0x1F);
        SetStep(B(at::kStep) == 0x35 ? 0x36 : 0x40);
        SetCounter(0, 0x14);
        break;
    case 0x36:
        if (Counter(0) != 0x16) break;
        MemberMessage(0x37);
        break;
    case 0x38:
        if (Counter(0) != 0x1B) break;
        FlagBattle(0x39);
        break;
    case 0x3A:
        Transition(4);
        SetStep(0x64);
        break;
    case 0x3C:
        CodeMessage(kMsg3C, 0x3D);
        break;
    case 0x3E:
        if (Counter(0) != 0xA) break;
        SetStep(0x3F);
        CodeExit(kExit3E);
        break;
    case 0x40:
        if (Counter(0) != 0x16) break;
        MemberMessage(0x41);
        break;
    case 0x42:
        if (Counter(0) != 0x1B) break;
        FlagBattle(0x43);
        break;
    case 0x44:
        Set(0x25);   // the step is left at 0x44
        break;
    case 0x46:
    case 0x5C:
    case 0x64:
        if (!WaitClear()) break;
        SetPass(0);
        SetStep(static_cast<unsigned char>(B(at::kStep) + 1));
        break;
    case 0x47:
        SetStep(0x48);
        Set(0x23);
        ScriptOr(8);
        CodeHall(kHall47);
        break;
    case 0x48:
        SetPass(0x1F);
        SetStep(0x32);
        break;
    case 0x5A:
        if (Counter(0) != 0x1C) break;
        SetCounter(0, 0);
        SetStep(0x5B);
        break;
    case 0x5B:
        SetStep(0x5C);
        Set(0x21);
        Transition(0xD);
        break;
    case 0x5D: {
        SetStep(0x5E);
        B(at::kMemberCount) = 0;
        const unsigned c3 = Counter(3);
        if (c3 >= 1 && c3 <= 6) {
            CallA(1);
            CallA(2);
            CallA(4);
            To4FHall(0x90);
        } else if (c3 >= 0xB && c3 <= 0x10) {
            CallA(1);
            CallA(3);
            CallA(4);
            To4FHall(0x8F);
        }
        break;
    }
    case 0x5E:
        SetPass(0x1F);
        SetStep(0x5F);
        break;
    case 0x5F: {
        if (Counter(0) != 9) break;
        Clear40();
        unsigned char* const bits = Bits();
        ClearCounters123();
        SetStep(0);
        SetRun(0);
        SH_CALL(Flags_Set)(bits, 0x1E);
        Set(0x1F);
        Set(0x20);
        Set(0x22);
        Set(0x3B);
        ScriptXor(8);
        break;
    }
    case 0x65:
        SetStep(0x66);
        Set(0x24);
        ScriptOr(8);
        CodeHall(kHall65);
        break;
    case 0x66:
        SetPass(0x1F);
        SetStep(0x3C);
        break;
    default:   // 7..0xE, 0x19, 0x1D, 0x22, 0x28, 0x2A..0x31, 0x39, 0x3B, 0x43, 0x45, 0x49..0x59, 0x60..0x63, above 0x66
        break;
    }
}

// ===========================================================================
// Runs 17..22

// original 0x5494F0: run 17 - steps 0..0x1E: a message by counter 0, a
// choice, a timed wait with two dropped PSX calls, battle 0x15, changes into
// areas 0x51 and 0x4E by the selector.
SC5_EXPORT void __cdecl Scena05_Run17(void) {
    switch (B(at::kStep)) {
    case 0: {
        if (!MsgDone()) break;
        const unsigned char c0 = Counter(0);
        SetCounter(0, 0);
        if (c0 == 1) Say(0x22, 1);
        else Say(0x20, 2);
        break;
    }
    case 1:
        if (Request() == 2) break;
        Say(0x20, 2);
        break;
    case 2:
        if (Request() == 2) break;
        SetStep(Counter(0) == 3 ? 6 : 5);
        break;
    case 5:
        Clear40();
        SetCounter(0, 0);
        ClearCounters123();
        SetStep(0);
        SetRun(0);
        break;
    case 6:
        SetCounter(0, 5);
        SetStep(7);
        break;
    case 7:
        if (Counter(0) != 6) break;
        SetCounter(0, 0);
        SetStep(8);
        Set(0x28);
        if (Sel() == 1) ChangeArea(0x51, 0x350000, 0x70000, 0x81);
        else if (Sel() == 2) ChangeArea(0x51, 0x350000, 0x70000, 0x80);
        break;
    case 8:
        SH_CALL(Music_FadeOutStop)(0xA);
        Sound(0x208);
        B(at::kLevel) = 0x7F;
        B(at::kLevelB) = 0;
        SetStep(9);
        break;
    case 9:
        if (Counter(0) != 1) break;
        SetStep(0xA);
        break;
    case 0xA:
        if (Counter(0) != 5) break;
        SetStep(0xB);
        W(at::kTimer) = 0;
        break;
    case 0xB: {
        const std::uint16_t t = static_cast<std::uint16_t>(W(at::kTimer) + 1);
        W(at::kTimer) = t;
        if (t >= 0xC8) {
            Sound(0x209);
            W(at::kTimer) = 0;
            SetStep(0xC);
        }
        SH_CALL(Port_DroppedCall)(0x10);   // the original pushes 4 above the first: unread
        SH_CALL(Port_DroppedCall)(0x10);
        if (static_cast<signed char>(B(at::kLevel)) > 8) break;
        Sound(0x209);
        W(at::kTimer) = 0;
        SetStep(0xC);
        break;
    }
    case 0xC:
        if (Counter(0) != 6) break;
        SH_CALL(Music_Play)(0x4E, 8);
        SetStep(0xF);
        break;
    case 0xF:
        if (Counter(0) != 0xF) break;
        SetStep(0x10);
        Set(0x29);
        if (Sel() == 1) {
            ChangeArea(0x51, 0x350000, 0x70000, 0x83);
            MusicCurrent(0xFF);
        } else if (Sel() == 2) {
            ChangeArea(0x51, 0x350000, 0x70000, 0x82);
            MusicCurrent(0xFF);
        }
        break;
    case 0x10:
        Sound(0x208);
        SetStep(0x15);
        break;
    case 0x15:
        if (Counter(0) != 0x11) break;
        SetStep(0x16);
        Set(0x2A);
        ChangeArea(0x51, 0x5A0000, 0x60000, 0x84);
        MusicCurrent(0xFF);
        break;
    case 0x16: {
        if (Counter(0) != 0x10) break;
        const unsigned sel = Sel();
        SetStep(0x18);
        SetCounter(2, 2);
        if (sel == 1 || sel == 2) {
            ChangeArea(0x51, 0xC50000, 0xE0000, sel == 1 ? 0x86 : 0x85);
            MusicCurrent(0xFF);
        }
        break;
    }
    case 0x18:
        if (Counter(0) != 6) break;
        Sound(0x209);
        SH_CALL(Field_StartEventBattle)(0x15);
        SetStep(0x19);
        break;
    case 0x19:
        if (Counter(0) != 0xA) break;
        Set(0x3C);
        SetStep(0x1A);
        if (Sel() == 1) DropIn(8);
        else if (Sel() == 2) DropIn(7);
        break;
    case 0x1A: {
        if (Counter(0) != 0xE) break;
        const unsigned sel = Sel();
        SetStep(0x1B);
        if (sel == 1 || sel == 2) {
            ChangeArea(0x51, 0x340000, 0x90000, sel == 1 ? 0x8A : 0x89);
            MusicCurrent(0x4E);
        }
        break;
    }
    case 0x1B:
        if (Counter(0) != 0x15) break;
        SetStep(0x1C);
        ChangeArea(0x4E, 0x280000, 0xAD0000, 0x88);
        MusicCurrent(0x4F);
        break;
    case 0x1C:
        if (Counter(0) != 1) break;
        Kind2(0);
        SetStep(0x1D);
        Set(0x2C);
        break;
    case 0x1D:
        if (Counter(0) != 0xA) break;
        SetStep(0x1E);
        Set(0x2B);
        if (Sel() == 1) ChangeArea(0x51, 0x340000, 0x70000, 0x8C);
        else if (Sel() == 2) ChangeArea(0x51, 0x340000, 0x70000, 0x8B);
        break;
    case 0x1E:
        if (Counter(0) != 1) break;
        EndRun123();
        break;
    default:   // 3, 4, 0xD, 0xE, 0x11..0x14, 0x17, above 0x1E
        break;
    }
}

// original 0x549A80: run 18 - steps 0..0x10: area 0x50's scene, battle 0x16.
SC5_EXPORT void __cdecl Scena05_Run18(void) {
    switch (B(at::kStep)) {
    case 0:
        if (!MsgDone()) break;
        if (Counter(0) == 1) {
            SetStep(1);
            break;
        }
        Clear40();
        SetCounter(0, 0);
        ClearCounters123();
        SetStep(0);
        SetRun(0);
        break;
    case 1:
        SetCounter(0, 2);
        SetStep(2);
        break;
    case 2:
        if (Counter(0) != 3) break;
        SetCounter(0, 0);
        SetStep(3);
        Set(0x2D);
        ChangeArea(0x50, 0x200000, 0xA0000, 0x8E);
        MusicCurrent(0x4F);
        break;
    case 3:
        if (Counter(0) != 7) break;
        SetStep(4);
        Set(0x2E);
        ScriptOr(8);
        Sound(0x208);
        if (Sel() == 1) {
            ChangeArea(0x50, 0x810000, 0x70000, 0x85);
            MusicCurrent(0xFF);
        } else if (Sel() == 2) {
            ChangeArea(0x50, 0x810000, 0x70000, 0x84);
            MusicCurrent(0xFF);
        }
        break;
    case 4:
        if (Counter(0) != 2) break;
        Transition(0xD);
        SetStep(5);
        break;
    case 5:
    case 0xD:
        if (!WaitClear()) break;
        SetPass(0);
        SetStep(static_cast<unsigned char>(B(at::kStep) + 1));
        break;
    case 6: {
        const unsigned sel = Sel();
        if (sel == 1) {
            CallB(0);
        } else if (sel == 2) {
            CallB(1);
            CallB(3);
        }
        SetCounter(2, 1);
        SetStep(7);
        ChangeArea(0x50, 0xC60000, 0xF8000, 0x82);
        MusicCurrent(0xFF);
        break;
    }
    case 7:
        SetPass(0x1F);
        SetStep(8);
        break;
    case 8:
        if (Counter(0) != 1) break;
        Kind2(0);
        SetStep(9);
        break;
    case 9:
        if (Counter(0) != 7) break;
        Sound(0x209);
        SH_CALL(Music_Play)(0x66, 8);
        SetStep(0xA);
        break;
    case 0xA:
        if (Counter(0) != 0xB) break;
        Battle(0x16);
        SetStep(0xB);
        break;
    case 0xB:
        if (Counter(0) != 0xC) break;
        DropIn(3);
        SH_CALL(Music_Play)(0x6A, 0x10);
        SetStep(0xC);
        break;
    case 0xC:
        if (Counter(0) != 0x14) break;
        SetStep(0xD);
        Set(0x2F);
        Transition(0xD);
        break;
    case 0xE: {
        const unsigned sel = Sel();
        if (sel != 1 && sel != 2) break;
        SetStep(0xF);
        CallA(1);
        CallA(sel == 1 ? 2 : 3);
        ChangeArea(0x50, 0x810000, 0xB0000, sel == 1 ? 0x81 : 0x80);
        break;
    }
    case 0xF:
        SetPass(0x1F);
        SetStep(0x10);
        break;
    case 0x10: {
        if (Counter(0) != 8) break;
        PartyPass();
        unsigned char* const bits = Bits();
        ScriptXor(8);
        SH_CALL(Flags_Set)(bits, 0x3D);
        EndRun123();
        break;
    }
    default:
        break;
    }
}

// original 0x549E50: run 19 - message 0x84.
SC5_EXPORT void __cdecl Scena05_Run19(void) { Talk(0x84); }

// original 0x549EA0: run 20 - steps 0..6: an effect at the leader, changes
// into area 0x4E by the selector, three bytes' bit 0, the call-table A entry
// by the selector.
SC5_EXPORT void __cdecl Scena05_Run20(void) {
    switch (B(at::kStep)) {
    case 0:
        SH_CALL(Scena05_SpawnMember)(0x91);
        SetStep(1);
        break;
    case 1:
        SetStep(2);
        if (Sel() == 1) ChangeArea(0x4E, 0x250000, 0xAE0000, 0x8A);
        else if (Sel() == 2) ChangeArea(0x4E, 0x250000, 0xAE0000, 0x89);
        break;
    case 2: {
        if (Counter(0) != 1) break;
        SetStep(3);
        Set(0x30);
        const unsigned sel = Sel();
        ScriptOr(8);
        if (sel == 1) ChangeArea(0x4E, 0x270000, 0xAD0000, 0x8C);
        else if (sel == 2) ChangeArea(0x4E, 0x270000, 0xAD0000, 0x8B);
        break;
    }
    case 3: {
        if (Counter(0) != 0xA) break;
        SetStep(4);
        Set(0x31);
        const unsigned sel = Sel();
        ScriptXor(8);
        if (sel == 1) ChangeArea(0x4E, 0x220000, 0x720000, 0x8E);
        else if (sel == 2) ChangeArea(0x4E, 0x220000, 0x720000, 0x8D);
        break;
    }
    case 4:
        if (Counter(0) != 4) break;
        SetStep(5);
        B(at::kBitsB1F) = static_cast<unsigned char>(B(at::kBitsB1F) | 1);
        B(at::kBitsDAF) = static_cast<unsigned char>(B(at::kBitsDAF) | 1);
        B(at::kBitsE53) = static_cast<unsigned char>(B(at::kBitsE53) | 1);
        if (Sel() == 1) CallA(3);
        else if (Sel() == 2) CallA(2);
        break;
    case 5:
        if (Request() != 0) break;
        SetCounter(0, 5);
        SetStep(6);
        break;
    case 6:
        if (Counter(0) != 5) break;
        Set(0x32);
        Clear40();
        ClearCounters123();
        SetStep(0);
        SetRun(0);
        break;
    default:
        break;
    }
}

// original 0x54A0D0: run 21 - step 0 (the message done) counter 0 1, flag
// 0x3A, Party_DropIn(3), Kind2_Place(0); step 1 at counter 0 0x1C, flag 0x3A
// cleared, the run's end.
SC5_EXPORT void __cdecl Scena05_Run21(void) {
    const unsigned char step = B(at::kStep);
    if (step == 0) {
        if (!MsgDone()) return;
        SetCounter(0, 1);
        Set(0x3A);
        DropIn(3);
        Kind2(0);
        SetStep(1);
    } else if (step == 1) {
        if (Counter(0) != 0x1C) return;
        Clr(0x3A);
        EndRun();
    }
}

// original 0x54A150: run 22 - message 0x4C.
SC5_EXPORT void __cdecl Scena05_Run22(void) { Talk(0x4C); }

void ScenaSc5_Inject() {
    if (bof3::WantsShadow("scena_sc5")) scena_sc5::SelfTest();
    BOF3_INJECT(Scena05_Frame);
    BOF3_INJECT(Scena05_EnterArea);
    BOF3_INJECT(Scena05_Run);
    BOF3_INJECT(Scena05_Run1);
    BOF3_INJECT(Scena05_Run4);
    BOF3_INJECT(Scena05_Run5);
    BOF3_INJECT(Scena05_Run6);
    BOF3_INJECT(Scena05_Run7);
    BOF3_INJECT(Scena05_Run8);
    BOF3_INJECT(Scena05_Run9);
    BOF3_INJECT(Scena05_Run10);
    BOF3_INJECT(Scena05_Run11);
    BOF3_INJECT(Scena05_Run13);
    BOF3_INJECT(Scena05_Run14);
    BOF3_INJECT(Scena05_Run16);
    BOF3_INJECT(Scena05_Run17);
    BOF3_INJECT(Scena05_Run18);
    BOF3_INJECT(Scena05_Run19);
    BOF3_INJECT(Scena05_Run20);
    BOF3_INJECT(Scena05_Run21);
    BOF3_INJECT(Scena05_Run22);
    BOF3_INJECT(Scena05_PartyCounter);
    BOF3_INJECT(Scena05_SpawnMember);
    BOF3_INJECT(Scena05_ObjectTrigger);
    BOF3_INJECT(Scena05_Object01);
    BOF3_INJECT(Scena05_Object02);
    BOF3_INJECT(Scena05_Object03);
    BOF3_INJECT(Scena05_Object04);
    BOF3_INJECT(Scena05_Object05);
    BOF3_INJECT(Scena05_Object06);
    BOF3_INJECT(Scena05_Object07);
    BOF3_INJECT(Scena05_StepHook);
    BOF3_INJECT(Scena05_CellHook);
    BOF3_INJECT(Scena05_Cell0);
    BOF3_INJECT(Scena05_Cell1);
}

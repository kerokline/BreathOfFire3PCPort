// Chapter 12's first block of scenario code, 0x55E4E0..0x561DB0 (round ten
// group SC12). docs/scena_sc12.md.
//
//   - The vtable at 0x6616E0 (0x662C80's entry 12): slot 0 Scena12_Frame, a
//     tail jump through Scena12_States on the state byte; slot 1
//     Scena12_ObjectTrigger, a call through Scena12_Objects on the object's
//     +0x86; slots 2 and 3 the step and arrive hooks (x, z) answering in al;
//     slot 4 Scena12_CellHook (a, b), the record search 0x56D800 over
//     Scena12_CellRecords and a tail jump through Scena12_CellHooks.
//   - State 1 Scena12_EnterArea (every way out stores state 2); state 2
//     Scena12_Run, a tail jump through Scena12_Runs on MoveScript_Var7; the
//     runs 1, 2, 4..9 are each a switch on the step byte 0x8034E5 whose cases
//     wait on a counter, the request byte or the wait word, do one thing and
//     set the next step. State 0 (0x5646B0) lies in group SC13's block.
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies. The tables
// are read in place and their entries called directly, as Scena01_Frame and
// magic_s16's dispatches do; the fuzz swaps them for recorders. No divergence:
// each function is a faithful replacement, except that a dispatcher whose
// index lies outside its table (a negative state or run, or one reading the
// next table) aborts where the original would jump through its neighbour -
// the project's rule for an index past a table (round nine, section 6).
#include "game/scena_sc12.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/scena_sc12_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = scena_sc12::at;
using scena_sc12::CellEntry;
using scena_sc12::CellFindFn;
using scena_sc12::KindFn;
using scena_sc12::ObjectEntry;
using scena_sc12::PlaceFn;
using scena_sc12::VoidFn;

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
void ScriptXor(std::uint16_t v) { W(at::kScriptFlags) = static_cast<std::uint16_t>(W(at::kScriptFlags) ^ v); }
void MusicCurrent(unsigned char v) { B(at::kMusicCurrent) = v; }

// The four counters 0x903848..B zeroed (a run's end), in that order.
void ClearCounters() {
    for (unsigned k = 0; k < 4; ++k) SetCounter(k, 0);
}

// The flag bits are read from 0x929ED0 afresh for every call, as the
// originals load the dword before each push.
unsigned char* Bits() { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(D(at::kFlagBits))); }
bool Flag(unsigned i) { return SH_CALL(Flags_Test)(Bits(), i) != 0; }   // test al, al
void Set(unsigned i) { SH_CALL(Flags_Set)(Bits(), i); }
void Clr(unsigned i) { SH_CALL(Flags_Clear)(Bits(), i); }

void Set40() { SH_CALL(ScriptFlags_Set40)(); }
void Clear40() { SH_CALL(ScriptFlags_Clear40)(); }
void Msg(unsigned short id) { SH_CALL(Msg_OpenScript)(id); }
void ChangeArea(unsigned area, int x, int z, unsigned flags) { SH_CALL(Field_ChangeArea)(area, x, z, flags); }
void CallA(unsigned n) { SH_CALL(Scenario_CallA)(n); }
void CallB(unsigned n) { SH_CALL(Scenario_CallB)(n); }
void PartyPass() { SH_AT(VoidFn, at::kPartyPass)(); }

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

// Music_LoadFile(track), then Task_Sleep(1) until File_LoadDone, then
// Music_Play(track, 8).
void LoadAndPlay(unsigned track) {
    SH_CALL(Music_LoadFile)(track);
    while (SH_CALL(File_LoadDone)() == 0) SH_CALL(Task_Sleep)(1);
    SH_CALL(Music_Play)(track, 8);
}

unsigned char* Effect(unsigned char slot) { return &B(at::kEffects + static_cast<std::uint32_t>(slot) * at::kEffectStride); }

// Effect_FindFree; the slot to `cell` when not 0 (the originals store it
// before testing it); a slot taken gets +0 = 1 and +5 = kind, and +1 = one
// when one is not negative. Answers the slot.
unsigned char Spawn(std::uint32_t cell, unsigned char kind, int one = -1) {
    const unsigned char slot = SH_CALL(Effect_FindFree)();
    if (cell != 0) B(cell) = slot;
    if (slot == 0xFF) return slot;
    unsigned char* const e = Effect(slot);
    e[0] = 1;
    e[5] = kind;
    if (one >= 0) e[1] = static_cast<unsigned char>(one);
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

// The selector Cond_Flags +0x19C & 0x7F picks one of ten flags base..base+9:
// 7..10 the first four, 13..18 the other six; 11, 12 and any other value
// none (the originals' jump tables of twelve, entries 4 and 5 empty).
int Pick(unsigned base) {
    const unsigned v = B(at::kSelector) & 0x7F;
    if (v >= 7 && v <= 10) return static_cast<int>(base + v - 7);
    if (v >= 13 && v <= 18) return static_cast<int>(base + v - 9);
    return -1;
}
// The picked flag set; with `rest`, the other nine cleared in ascending order.
void SetPicked(unsigned base, bool rest) {
    const int f = Pick(base);
    if (f < 0) return;
    Set(static_cast<unsigned>(f));
    if (!rest) return;
    for (unsigned i = base; i < base + 10; ++i)
        if (static_cast<int>(i) != f) Clr(i);
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
#define SC12_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// The vtable's slots

// original 0x55E4E0: slot 0, Field_ModeDispatch's call every field frame - a
// tail jump through Scena12_States on the s8 state 0x8034E2: 0 0x5646B0
// (group SC13's), 1 Scena12_EnterArea, 2 Scena12_Run.
SC12_EXPORT void __cdecl Scena12_Frame(void) {
    const int state = static_cast<signed char>(B(at::kState));
    reinterpret_cast<VoidFn>(static_cast<std::uintptr_t>(Entry(at::kStates, state, at::kStateCount, "Scena12_Frame")))();
}

// original 0x55EA80: state 2, a tail jump through Scena12_Runs on the s8 run
// MoveScript_Var7: 0, 3 a bare ret (0x437CC0), 1 Run1, 2 Run2, 4..9 Run4..9.
SC12_EXPORT void __cdecl Scena12_Run(void) {
    const int run = static_cast<signed char>(B(at::kRun));
    reinterpret_cast<VoidFn>(static_cast<std::uintptr_t>(Entry(at::kRuns, run, at::kRunCount, "Scena12_Run")))();
}

// original 0x561880: slot 1, called by 0x56D6D0 with the object that
// triggered: Scena12_Objects[object +0x86] (object, the flag bits' pointer).
// The entry's eax is the original's answer; 0x56D6D0 does not read it.
SC12_EXPORT void __cdecl Scena12_ObjectTrigger(unsigned char* object) {
    const std::uint32_t bits = D(at::kFlagBits);
    const unsigned index = object[0x86];
    reinterpret_cast<ObjectEntry>(static_cast<std::uintptr_t>(
        Entry(at::kObjects, static_cast<int>(index), at::kObjectCount, "Scena12_ObjectTrigger")))(object, bits);
}

// original 0x5619E0: slot 3, the arrive hook (Scenario_ArriveHook, x and z
// 16.16): in area 0x79 with flag 0 set and x at most 0x16.0 (signed),
// ScriptFlags_Set40, step 0x3C, run 2, 1; else 0.
SC12_EXPORT unsigned char __cdecl Scena12_ArriveHook(int x, int /*z*/) {
    if (Area() != 0x79) return 0;
    if (!Flag(0)) return 0;
    if (x > 0x160000) return 0;
    Set40();
    SetStep(0x3C);
    SetRun(2);
    return 1;
}

// original 0x561A30: slot 2, the step hook (Scenario_StepHook, x and z 16.16,
// every bound a signed inclusive compare). 1 when a step starts a scene:
//   area 0x86, flag 1 clear: x 0x1F.8..0x20.8, z 0x24.8..0x27 - counters 0,
//     step 0xA, run 9, the four cells (0x2D..0x2E, 0x29..0x2A) to 0x70, flag 1;
//   area 0x86, flag 1 set: ObjTrio +0x3C at most 0x200.0, x 0x2E..0x2F.8,
//     z 0x29..0x2A - counters 0, step 0x21, run 9;
//   area 0x85, flag 0x19 clear: x 0x33..0x34.8, z 0x29..0x29.8 - counter 0
//     0, step 0xA, run 4;
//   area 0x88, flag 0x25 set and 0x26 clear: x above 0x1B.0 and z's high
//     word 7..11 (a 16-bit unsigned test) - run 5, step 0x1B;
//   area 0xBC, flag 0x2B clear: two rectangles (step 5) or a third (step
//     0xA) - counter 0 0, run 7.
// Each area test reads Game_AreaNumber afresh.
SC12_EXPORT unsigned char __cdecl Scena12_StepHook(int x, int z) {
    if (Area() == 0x86) {
        if (!Flag(1)) {
            if (x >= 0x1F8000 && x <= 0x208000 && z >= 0x248000 && z <= 0x270000) {
                Set40();
                ClearCounters();
                SetStep(0xA);
                SetRun(9);
                SH_CALL(AreaMap_SetByte)(0x2D, 0x29, 0x70);
                SH_CALL(AreaMap_SetByte)(0x2D, 0x2A, 0x70);
                SH_CALL(AreaMap_SetByte)(0x2E, 0x29, 0x70);
                SH_CALL(AreaMap_SetByte)(0x2E, 0x2A, 0x70);
                Set(1);
                return 1;
            }
        } else if (S32(at::kObjTrioY) <= 0x2000000 && x >= 0x2E0000 && x <= 0x2F8000 && z >= 0x290000 && z <= 0x2A0000) {
            Set40();
            ClearCounters();
            SetStep(0x21);
            SetRun(9);
            return 1;
        }
    }
    if (Area() == 0x85 && !Flag(0x19) && x >= 0x330000 && x <= 0x348000 && z >= 0x290000 && z <= 0x298000) {
        Set40();
        SetCounter(0, 0);
        SetStep(0xA);
        SetRun(4);
        return 1;
    }
    if (Area() == 0x88 && Flag(0x25) && !Flag(0x26) && x > 0x1B0000 &&
        static_cast<std::uint16_t>((static_cast<std::uint32_t>(z) >> 16) - 7) < 5) {
        Set40();
        SetRun(5);
        SetStep(0x1B);
        return 1;
    }
    if (Area() == 0xBC && !Flag(0x2B)) {
        const bool first = x >= 0x1C0000 && x <= 0x1D8000 && z >= 0x38000 && z <= 0x48000;
        if (first || (x >= 0x390000 && x <= 0x3A8000 && z >= 0x470000 && z <= 0x488000)) {
            Set40();
            SetCounter(0, 0);
            SetStep(5);
            SetRun(7);
            return 1;
        }
        if (x >= 0x368000 && x <= 0x370000 && z >= 0x3F0000 && z <= 0x3F8000) {
            Set40();
            SetCounter(0, 0);
            SetStep(0xA);
            SetRun(7);
            return 1;
        }
    }
    return 0;
}

// original 0x561CE0: slot 4, the cell hook (0x56D7A0, a cell (a, b) faced):
// 0x56D800(Scena12_CellRecords, 4, a, b) - the record the area and the cell
// match, or 0xFF; negative (as a signed byte) answers 0xFF; else a tail jump
// through Scena12_CellHooks with (a, b), whose al is the answer.
SC12_EXPORT unsigned char __cdecl Scena12_CellHook(unsigned a, unsigned b) {
    const unsigned char found = SH_AT(CellFindFn, at::kCellFind)(reinterpret_cast<const void*>(at::kCellRecords), 4, a, b);
    const int index = static_cast<signed char>(found);
    if (index < 0) return 0xFF;
    return reinterpret_cast<CellEntry>(static_cast<std::uintptr_t>(
        Entry(at::kCellHooks, index, at::kCellHookCount, "Scena12_CellHook")))(a, b);
}

// original 0x561D10: Scena12_CellHooks entries 0..2: ScriptFlags_Set40, the
// counters and step 0 (step 5 when the leader's byte 0x802DC9, read after the
// call, is 5), run 5; 1.
SC12_EXPORT unsigned char __cdecl Scena12_CellTalk(void) {
    Set40();
    const unsigned char leader = B(at::kLeaderName);
    ClearCounters();
    SetStep(0);
    if (leader == 5) SetStep(5);
    SetRun(5);
    return 1;
}

// original 0x561D50: Scena12_CellHooks entry 3: with flag 0x2A set, 0xFF;
// else ScriptFlags_Set40, run 7, counters 0, 1, 3 and the step 0,
// MoveCmd_TestFB(Sprite_Current +0x36, +0x3A) (words), Sound_PlayEffect(0x103); 1.
SC12_EXPORT unsigned char __cdecl Scena12_CellDoor(void) {
    if (Flag(0x2A)) return 0xFF;
    Set40();
    SetRun(7);
    SetCounter(0, 0);
    SetCounter(1, 0);
    SetCounter(3, 0);
    SetStep(0);
    const unsigned char* const sprite = Sprite_Current;
    SH_CALL(MoveCmd_TestFB)(static_cast<short>(W(static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(sprite)) + 0x36)),
                            static_cast<short>(W(static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(sprite)) + 0x3A)));
    SH_CALL(Sound_PlayEffect)(0x103);
    return 1;
}

// ===========================================================================
// Scena12_Objects: the handlers of the object trigger (entries 6, 9..14; the
// rest a bare ret). Each starts a run: ScriptFlags_Set40, the counters (some
// of them) 0, the step and the run.

// original 0x5618A0: entry 6 - run 1, step 0.
SC12_EXPORT void __cdecl Scena12_Object06(void) {
    Set40();
    SetRun(1);
    SetCounter(1, 0);
    SetCounter(2, 0);
    SetCounter(3, 0);
    SetStep(0);
}

// original 0x5618D0: entry 9 - run 4, step 0x14.
SC12_EXPORT void __cdecl Scena12_Object09(void) {
    Set40();
    SetStep(0x14);
    SetCounter(1, 0);
    SetCounter(2, 0);
    SetCounter(3, 0);
    SetRun(4);
}

// original 0x561900: entry 10 - run 5, step 0x14.
SC12_EXPORT void __cdecl Scena12_Object10(void) {
    Set40();
    SetStep(0x14);
    SetCounter(1, 0);
    SetCounter(2, 0);
    SetCounter(3, 0);
    SetRun(5);
}

// original 0x561930: entry 11 - run 5, step 2, all four counters 0.
SC12_EXPORT void __cdecl Scena12_Object11(void) {
    Set40();
    SetStep(2);
    ClearCounters();
    SetRun(5);
}

// original 0x561960: entry 12 - run 8, step 0.
SC12_EXPORT void __cdecl Scena12_Object12(void) {
    Set40();
    SetRun(8);
    SetCounter(1, 0);
    SetCounter(2, 0);
    SetCounter(3, 0);
    SetStep(0);
}

// original 0x561990: entry 13 - run 8, step 0x1E.
SC12_EXPORT void __cdecl Scena12_Object13(void) {
    Set40();
    SetStep(0x1E);
    SetCounter(1, 0);
    SetCounter(2, 0);
    SetCounter(3, 0);
    SetRun(8);
}

// original 0x5619C0: entry 14 - run 1, step 0xA, the counters left.
SC12_EXPORT void __cdecl Scena12_Object14(void) {
    Set40();
    SetStep(0xA);
    SetRun(1);
}

// ===========================================================================
// State 1: once per area entered

namespace {

// Scena12_EnterArea's body up to its common tail (the caller stores state 2).
// Answers false where the original returns at once (also with state 2).
// Game_AreaNumber is read three times for the six first tests (0x65 / 0x79,
// 0x82 / 0x83, 0x84 / 0x85) and afresh for each later one; the counter byte
// 0x90384A afresh at each test.
void EnterAreaBody() {
    unsigned area = Area();
    if (area == 0x65) {
        const unsigned c = Counter(2);
        if (c == 1) { SetPass(0x1F); return; }
        if (c == 2) SetPass(0);
        return;
    }
    if (area == 0x79) {
        const unsigned c = Counter(2);
        if (c == 1) { SetPass(0); return; }
        if (c != 2) return;
        SetPass(0x1F);
        Clear40();
        ClearCounters();
    }
    area = Area();
    if (area == 0x82) {
        if (Counter(2) == 1) SetPass(0x1F);
        return;
    }
    if (area == 0x83) {
        const unsigned c = Counter(2);
        if (c == 1) {
            SH_CALL(MapView_SetElevation)(0x498);
            B(at::kRedraw) = 2;
        } else if (c == 2) {
            B(at::kScriptFlags) = static_cast<unsigned char>(B(at::kScriptFlags) | 0x16);
            SetPass(0x1F);
            return;
        } else {
            if (c == 3) SetPass(0x1F);
            return;
        }
    }
    area = Area();
    if (area == 0x84) {
        const unsigned c = Counter(2);
        if (c == 1) {
            B(at::kScriptFlags) = static_cast<unsigned char>(B(at::kScriptFlags) | 0x16);
            SetPass(0);
            return;
        }
        if (c == 2) SetPass(0x1F);
        return;
    }
    if (area == 0x85) {
        if (Flag(0x24) && !Flag(0x26) && B(at::kCondFD) == 2) {
            Set40();
            SetStep(0x19);
            SetRun(5);
        }
        const unsigned c = Counter(2);
        if (c == 3) {
            const unsigned v = B(at::kSelector) & 0x7F;
            B(at::kScriptFlags) = static_cast<unsigned char>(B(at::kScriptFlags) | 0xE);
            switch (v) {
            case 8: CallB(1); CallA(3); break;
            case 13: CallB(0); CallA(3); break;
            case 16: CallB(2); CallA(3); break;
            case 17: CallB(4); CallA(9); break;
            default: break;
            }
        } else if (c == 0xA) {
            Spawn(0, 0x77, 1);
        }
    }
    if (Area() == 0x86 && !Flag(0x2F) && S32(at::kObjTrioZ) < 0x300000) {
        Set(0x2F);
        Set40();
        ClearCounters();
        SetStep(0);
        SetRun(9);
    }
    if (Area() == 0x87) {
        if (Counter(2) != 0xA) return;
        Spawn(0, 0x77, 1);
    }
    if (Area() == 0x88) {
        const unsigned c = Counter(2);
        if (c == 1) {
            CallB(3);
        } else if (c == 2) {
            SH_CALL(MapView_SetElevation)(0x200);
            B(at::kRedraw) = 2;
        } else if (c == 0xA) {
            Spawn(0, 0x77, 1);
        } else {
            return;
        }
    }
    if (Area() == 0xBC) {
        const unsigned c = Counter(2);
        if (c < 1 || c > 9) return;
        switch (c) {
        case 1:
            if (!Flag(0x28)) Spawn(at::kEffectSlot, 0x76);
            break;
        case 2:
            Spawn(at::kEffectSlot, 0x79);
            LoadAndPlay(0x8F);
            break;
        case 3:
            B(at::kScriptFlags) = static_cast<unsigned char>(B(at::kScriptFlags) | 8);
            SH_CALL(MapView_SetElevation)(0x290);
            B(at::kRedraw) = 2;
            break;
        case 4:
            SH_CALL(MapView_SetElevation)(0x100);
            B(at::kRedraw) = 2;
            break;
        case 5:
            CallB(2);
            CallB(3);
            CallA(8);
            break;
        case 6:
            SetPass(0x1F);
            Spawn(at::kEffectSlot, 0x7B);
            break;
        case 7:
            Spawn(at::kEffectSlot, 0x7B);
            break;
        case 8:
            Spawn(at::kEffectSlot, 0x7C);
            break;
        default:   // 9
            LoadAndPlay(0x8B);
            break;
        }
    }
    area = Area();
    if (area == 0x2D || area == 0x41 || area == 0x57 || area == 0x10 || area == 0x73) {
        ClearCounters();
        SetStep(0);
        SetRun(0);
    }
}

}  // namespace

// original 0x55E4F0: state 1, once per area entered - by Game_AreaNumber
// (0x65, 0x79, 0x82..0x88, 0xBC, and 0x10 / 0x2D / 0x41 / 0x57 / 0x73 which
// end the run) and the counter byte 0x90384A: pass flags, script flags, the
// elevation, effects (Effect_FindFree, kinds 0x76..0x7C), the music (loaded
// and waited for), call-table entries by the selector; every way out stores
// state 2.
SC12_EXPORT void __cdecl Scena12_EnterArea(void) {
    EnterAreaBody();
    B(at::kState) = 2;
}

// ===========================================================================
// State 2's runs: each a switch on the step byte 0x8034E5 (unsigned); a step
// no case holds does nothing.

// original 0x55EA90: run 1. Step 0xA: unless the request byte is 2, counter 0
// = 0x32 and step 0xB. Step 0xB: at counter 1 = 0xA, counter 1 0 and the run
// ends.
SC12_EXPORT void __cdecl Scena12_Run1(void) {
    const unsigned step = B(at::kStep);
    if (step == 0xA) {
        if (Request() == 2) return;
        SetCounter(0, 0x32);
        SetStep(0xB);
    } else if (step == 0xB) {
        if (Counter(1) != 0xA) return;
        SetCounter(1, 0);
        EndRun();
    }
}

// original 0x55EAE0: run 2. Step 0x32 message 6, step 0x3C message 7, each
// then step 0x33; step 0x33 ends the run once the request byte is not 2.
SC12_EXPORT void __cdecl Scena12_Run2(void) {
    const unsigned step = B(at::kStep);
    if (step == 0x32 || step == 0x3C) {
        Say(step == 0x32 ? 6 : 7);
        SetStep(0x33);
    } else if (step == 0x33) {
        if (Request() == 2) return;
        EndRun();
    }
}

namespace {

// Run 4's step 0x17: the 0x2000 colours at 0x80F580 greyed - each channel the
// mean of the three (the sum over 3, a signed divide of a non-negative sum),
// bit 15 kept.
void GreyClut() {
    std::uint16_t* const c = &W(at::kClut);
    for (unsigned i = 0; i < at::kClutWords; ++i) {
        const unsigned v = c[i];
        const unsigned sum = (((v >> 10) & 0x1F) + ((v >> 5) & 0x1F) + (v & 0x1F)) & 0xFFFF;
        const unsigned g = sum / 3;
        c[i] = static_cast<std::uint16_t>((((g << 5) | g) << 5) | (v & 0x8000) | g);
    }
}

// The run's end the originals share (0x55F4CE): the four counters, the step
// and the run 0.
void EndCounters() {
    ClearCounters();
    SetStep(0);
    SetRun(0);
}

}  // namespace

// original 0x55EB30: run 4, steps 0..2, 0xA, 0xB, 0x14..0x1E, 0x27 (a byte
// table over 0..0x27, then 17 cases). docs/scena_sc12.md section 3.
SC12_EXPORT void __cdecl Scena12_Run4(void) {
    switch (B(at::kStep)) {
    case 0:
        SetCounter(0, 0);
        SetCounter(2, 0);
        SetStep(1);
        ChangeArea(0x85, 0x50000, 0x5D0000, 0x80);
        break;
    case 1:
        SH_CALL(Kind2_Place)(0);
        SetStep(2);
        break;
    case 2:
        if (Counter(0) != 5 || !WaitClear()) break;
        B(at::kScriptFlags) = static_cast<unsigned char>(B(at::kScriptFlags) | 0x16);
        Set(0x16);
        Set(0);
        Clear40();
        EndCounters();
        break;
    case 0xA:
        Say(0xB);
        SetStep(0xB);
        break;
    case 0xB:
        if (Request() == 2) break;
        EndRun();
        break;
    case 0x14:
        if (Request() == 2) break;
        SetCounter(0, 1);
        SH_CALL(Party_DropIn)(1);
        SetStep(0x15);
        break;
    case 0x15:
        if (Counter(0) != 6) break;
        SetCounter(2, 2);
        SetCounter(0, 0);
        SetStep(0x16);
        Set(0x17);
        ChangeArea(0x65, 0x440000, 0x88000, 0x82);
        MusicCurrent(0xFF);
        break;
    case 0x16:
        if (!WaitClear()) break;
        Say(0x11);
        SetStep(0x17);
        break;
    case 0x17:
        if (Request() == 2) break;
        GreyClut();
        B(at::kClutDirty) = 1;
        SetPass(0x1F);
        SetCounter(2, 0);
        SetCounter(0, 1);
        SetStep(0x18);
        break;
    case 0x18:
        if (Counter(0) != 2) break;
        SetCounter(2, 0);
        SetCounter(0, 0);
        SetStep(0x19);
        Set(0x18);
        ChangeArea(0x85, 0x1E0000, 0x5D0000, 0x82);
        SH_CALL(Music_Play)(0x8F, 8);
        break;
    case 0x19:
        if (Counter(0) != 4) break;
        SH_CALL(Transition_Start)(4);
        SetStep(0x1A);
        break;
    case 0x1A:
        if (!WaitClear()) break;
        ScriptXor(0x16);
        SetCounter(0, 0);
        SetPass(0);
        Say(0x11);
        SetStep(0x1B);
        break;
    case 0x1B:
        if (Request() == 2) break;
        SetRequest(6);
        SetStep(0x1C);
        break;
    case 0x1C:
        if (Request() != 0) break;
        SetStep(0x1D);
        Set(0x19);
        PartyPass();
        SetPicked(0x1A, false);
        ChangeArea(0x85, 0x3D0000, 0x2B8000, 0x83);
        break;
    case 0x1E:
        if (Request() != 0 || B(at::kLoadByte) != 1) break;
        SetStep(0x27);
        SetPicked(0x1A, true);
        ChangeArea(0x85, 0x3D0000, 0x2B8000, 0x83);
        break;
    case 0x1D:
    case 0x27:
        SetPass(0x1F);
        Clear40();
        EndCounters();
        break;
    default:
        break;
    }
}

// original 0x55F5C0: run 5, steps 0..3, 5..9, 0x14..0x16, 0x19..0x1C,
// 0x28..0x2D (a byte table over 0..0x2D, then 22 cases).
SC12_EXPORT void __cdecl Scena12_Run5(void) {
    switch (B(at::kStep)) {
    case 0:
        Say(2);
        SetStep(1);
        break;
    case 1:
        if (Request() == 2) break;
        SetCounter(0, 0);
        SetCounter(1, 0);
        SetCounter(2, 0);
        SetCounter(3, 0);
        EndRun();
        break;
    case 2:
        // Step 3 whether or not the request byte was 2.
        if (Request() != 2) {
            Spawn(0, 0x77, 2);
            Say(0xD);
        }
        SetStep(3);
        break;
    case 3:
        if (Request() == 2) break;
        Set(0x14);
        SetCounter(0, 0xA);
        SetCounter(1, 0);
        SetCounter(2, 0xA);
        SetCounter(3, 0);
        EndRun();
        break;
    case 5:
        B(at::kScriptFlags) = static_cast<unsigned char>(B(at::kScriptFlags) | 0x20);
        SH_CALL(Party_DropIn)(3);
        SetStep(6);
        break;
    case 6:
        if (Counter(0) != 4) break;
        SetStep(7);
        SetCounter(2, 2);
        ChangeArea(0x88, 0x40000, 0x10000, 0x84);
        break;
    case 7:
        SH_CALL(Kind2_Place)(0);
        SetStep(8);
        break;
    case 8:
        if (Counter(0) != 0xA) break;
        SetCounter(0, 0);
        SetCounter(2, 1);
        SetStep(9);
        Set(0x24);
        ChangeArea(0x88, 0xC0000, 0x590000, 0x85);
        break;
    case 9:
        if (Counter(0) != 0xA) break;
        Set(0x25);
        SetCounter(1, 0);
        SetCounter(2, 0xA);
        SetCounter(3, 0);
        EndRun();
        Spawn(0, 0x77, 0);
        break;
    case 0x14: {
        if (Request() == 2) break;
        const unsigned tile = W(at::kTile) & 0xFF;
        SetCounter(0, 1);
        if (tile >= 0x62 && tile <= 0x66) {
            SH_CALL(Party_DropIn)(9);
            Set(0x16);
            SetStep(0x28);
        } else {
            SH_CALL(Party_DropIn)(6);
            SetStep(0x15);
        }
        break;
    }
    case 0x15:
        if (Counter(0) != 0x23) break;
        SetCounter(0, 0);
        SetCounter(1, 1);
        SetStep(0x16);
        ChangeArea(0x88, 0xC0000, 0x590000, 0x87);
        break;
    case 0x16:
        if (Counter(0) != 0xA) break;
        SetCounter(2, 0xA);
        SetCounter(1, 0);
        SetCounter(3, 0);
        Clear40();
        SetStep(0);
        SetRun(0);
        SH_CALL(Flags_Set)(&B(at::kStoryFlags), 0x29);
        Clr(0x14);
        Spawn(0, 0x77, 0);
        break;
    case 0x19: {
        SetStep(0x1A);
        const unsigned area = Area();
        if (area == 0x85) SH_CALL(Party_DropIn)(4);
        else if (area == 0x87) SH_CALL(Party_DropIn)(2);
        else if (area == 0x88) SH_CALL(Party_DropIn)(8);
        break;
    }
    case 0x1A:
        if (Counter(0) != 0x28) break;
        SetCounter(1, 1);
        SetCounter(0, 0);
        SetCounter(2, 0);
        SetStep(0x16);
        ChangeArea(0x88, 0xC0000, 0x590000, 0x87);
        break;
    case 0x1B:
        Say(0x13);
        SetStep(0x1C);
        break;
    case 0x1C:
        if (Request() == 2) break;
        Clear40();
        SetRun(0);
        SetStep(0);
        break;
    case 0x28: {
        if (Counter(0) != 0x14) break;
        SetCounter(0, 0);
        SetCounter(2, 3);
        SetStep(0x29);
        Set(0x26);
        const unsigned v = B(at::kSelector) & 0x7F;
        if (v == 8) { SetCounter(1, 0); ChangeArea(0x85, 0x60000, 0x5C0000, 0x86); }
        else if (v == 13) { SetCounter(1, 1); ChangeArea(0x85, 0x60000, 0x5C0000, 0x86); }
        else if (v == 16) { SetCounter(1, 2); ChangeArea(0x85, 0x60000, 0x5C0000, 0x86); }
        else if (v == 17) { SetCounter(1, 0); ChangeArea(0x85, 0x60000, 0x5C0000, 0x85); }
        break;
    }
    case 0x29: {
        if (Counter(0) != 0xA) break;
        SetStep(0x2A);
        SetCounter(0, 0);
        SetCounter(2, 0);
        Set(0x27);
        const unsigned v = D(at::kSelector) & 0x7F;
        if (v == 8) ChangeArea(0x85, 0x318000, 0x268000, 0x87);
        else if (v == 0x11) ChangeArea(0x85, 0x318000, 0x268000, 0x88);
        break;
    }
    case 0x2A:
        SetCounter(1, 0);
        SH_CALL(Kind2_Place)(1);
        SetStep(0x2B);
        break;
    case 0x2B:
        if (Counter(0) != 5) break;
        SH_CALL(Transition_Start)(4);
        SetStep(0x2C);
        break;
    case 0x2C: {
        if (!WaitClear()) break;
        const unsigned v = B(at::kSelector) & 0x7F;
        ScriptXor(0xE);
        SetCounter(0, 0);
        SetPass(0);
        SetStep(0x2D);
        switch (v) {
        case 7: CallB(1); CallB(0); CallA(6); break;
        case 8: CallB(1); CallA(7); break;
        case 9: CallB(1); CallA(6); break;
        case 10: CallB(1); CallB(4); CallA(6); break;
        case 13: CallB(0); CallA(7); break;
        case 14: CallB(0); CallA(6); break;
        case 15: CallB(0); CallB(4); CallA(6); break;
        case 17: CallB(4); CallA(7); break;
        case 18: CallB(4); CallA(6); break;
        default: break;
        }
        break;
    }
    case 0x2D:
        SetRun(6);
        ClearCounters();
        SetStep(0);
        break;
    default:
        break;
    }
}

namespace {

// Run 6's area changes into 0xBC: the counters, the step, the music byte
// 0xFF before the change and `after` behind it.
void Hop(unsigned char c0, unsigned char c2, unsigned char c1, unsigned char step, int x, int z, unsigned flags,
         unsigned char after = 0x90) {
    SetCounter(0, c0);
    SetCounter(2, c2);
    SetCounter(1, c1);
    SetStep(step);
    MusicCurrent(0xFF);
    ChangeArea(0xBC, x, z, flags);
    MusicCurrent(after);
}

}  // namespace

// original 0x55FD70: run 6, steps 0..0x16 through a jump table of 23.
SC12_EXPORT void __cdecl Scena12_Run6(void) {
    switch (B(at::kStep)) {
    case 0:
        ScriptXor(0x20);
        SetCounter(2, 1);
        SetCounter(1, 0);
        SetStep(1);
        ChangeArea(0xBC, 0x68000, 0x598000, 0x80);
        MusicCurrent(0xFF);
        break;
    case 1:
        SetPass(0x1F);
        SH_CALL(Kind2_Place)(0);
        SetStep(2);
        break;
    case 2:
        if (Counter(0) != 5) break;
        Set(0x28);
        SetCounter(0, 0);
        SetCounter(2, 1);
        SetStep(3);
        Set(0x15);
        ChangeArea(0x79, 0x238000, 0x310000, 6);
        MusicCurrent(0xFF);
        break;
    case 3: {
        B(at::kCondFE) = 1;
        const unsigned char slot = SH_CALL(Effect_FindFree)();
        B(at::kEffectSlot) = slot;
        if (slot == 0xFF) break;
        SetCounter(3, slot);
        Place13(slot, -0x370, AngleY(), 0x200, 4);
        SetStep(4);
        break;
    }
    case 4:
        if (Effect(Counter(3))[0] != 0) break;
        SetCounter(3, 0);
        SetPass(0x1F);
        SetStep(5);
        break;
    case 5:
        if (Counter(0) != 5) break;
        SetCounter(0, 0);
        SetCounter(2, 2);
        SetCounter(1, 1);
        SetStep(6);
        ChangeArea(0xBC, 0x68000, 0x598000, 0x81);
        MusicCurrent(0x8F);
        break;
    case 6:
        if (Counter(0) != 0xA) break;
        Hop(0, 3, 2, 7, 0x100000, 0x490000, 0x82);
        break;
    case 7:
        SH_CALL(Kind2_Place)(1);
        SetStep(8);
        break;
    case 8:
        if (Counter(0) != 2) break;
        Hop(0, 4, 3, 9, 0x3C0000, 0x2F0000, 0x83);
        break;
    case 9:
        SH_CALL(Kind2_Place)(2);
        SetStep(0xA);
        break;
    case 0xA:
        if (Counter(0) != 1) break;
        ScriptXor(8);
        Hop(0, 4, 4, 0xB, 0x68000, 0x88000, 0x84);
        break;
    case 0xB:
        if (Counter(0) != 1) break;
        ScriptXor(8);
        Hop(0, 0, 5, 0xC, 0x78000, 0x1F0000, 0x85);
        break;
    case 0xC:
        if (Counter(0) != 2) break;
        SH_CALL(Sound_PlayEffect)(0x20C);
        B(at::kScriptFlags) = static_cast<unsigned char>(B(at::kScriptFlags) | 8);
        Hop(0, 4, 6, 0xD, 0x120000, 0x340000, 0x89);
        break;
    case 0xD:
        SH_CALL(Kind2_Place)(3);
        SetStep(0xE);
        break;
    case 0xE:
        if (Counter(0) != 3) break;
        ScriptXor(8);
        Hop(0, 0, 7, 0xF, 0x1D8000, 0x98000, 0x8A);
        break;
    case 0xF:
        if (Counter(0) != 1) break;
        Hop(0, 0, 8, 0x10, 0x40000, 0x3E0000, 0x8B);
        break;
    case 0x10:
        if (Counter(0) != 1) break;
        Hop(0, 0, 0, 0x11, 0xA0000, 0x5A0000, 0x8C);
        break;
    case 0x11:
        if (Counter(0) != 1) break;
        Hop(0, 5, 9, 0x12, 0x120000, 0x400000, 0x8D);
        break;
    case 0x12: {
        if (Counter(0) != 3) break;
        const unsigned char slot = SH_CALL(Effect_FindFree)();
        B(at::kEffectSlot) = slot;
        if (slot == 0xFF) break;
        SetCounter(0, 4);
        SetStep(0x13);
        Effect(slot)[0] = 1;
        Effect(slot)[5] = 0x7A;
        break;
    }
    case 0x13:
        if (Counter(0) != 4) break;
        SH_CALL(Music_FadeOutStop)(0x1E);
        SH_CALL(Transition_Start)(0xD);
        SetStep(0x14);
        break;
    case 0x14:
        if (!WaitClear()) break;
        SetPass(0);
        SetStep(0x15);
        break;
    case 0x15:
        SetCounter(0, 0);
        SetCounter(2, 6);
        SetCounter(1, 0);
        SetStep(0x16);
        Set(0x29);
        ChangeArea(0xBC, 0x120000, 0x400000, 0x8E);
        MusicCurrent(0xFF);
        break;
    case 0x16:
        if (Counter(0) != 4) break;
        B(at::kScriptFlags) = static_cast<unsigned char>(B(at::kScriptFlags) | 0x20);
        SetCounter(1, 0);
        SetCounter(2, 7);
        SetCounter(3, 0);
        EndRun();
        break;
    default:
        break;
    }
}

// original 0x560400: run 7, steps 0, 1, 5, 6, 0xA..0x14, 0x1D, 0x1E (a byte
// table over 0..0x1E, then 17 cases).
SC12_EXPORT void __cdecl Scena12_Run7(void) {
    switch (B(at::kStep)) {
    case 0:
        SetStep(1);
        ChangeArea(0xBC, 0x158000, 0xB0000, 0x8F);
        MusicCurrent(0xFF);
        break;
    case 1:
        if (Counter(0) != 2) break;
        B(at::kScriptFlags) = static_cast<unsigned char>(B(at::kScriptFlags) | 0x80);
        Set(0x2A);
        SetCounter(2, 7);
        EndRun();
        break;
    case 5:
        Say(0xB);
        SetStep(6);
        break;
    case 6:
        if (Request() == 2) break;
        Clear40();
        SetCounter(2, 7);
        SetStep(0);
        SetRun(0);
        break;
    case 0xA:
        Set40();
        B(at::kScriptFlags) = static_cast<unsigned char>(B(at::kScriptFlags) | 8);
        SH_CALL(Party_DropIn)(0x10);
        SetStep(0xB);
        break;
    case 0xB:
        if (Counter(0) != 1) break;
        SetCounter(0, 0);
        SetCounter(2, 8);
        SetStep(0xC);
        MusicCurrent(0xFF);
        ChangeArea(0xBC, 0xA0000, 0x570000, 0x91);
        break;
    case 0xC:
        if (Counter(0) != 4) break;
        SetCounter(0, 5);
        SetStep(0xD);
        SH_CALL(Sound_PlayEffect)(0x20E);
        SH_CALL(Music_Play)(0x8F, 8);
        break;
    case 0xD:
        if (Counter(0) != 0x14) break;
        SetCounter(0, 0);
        SetCounter(2, 0);
        SetStep(0xE);
        Set(0x2B);
        MusicCurrent(0xFF);
        ChangeArea(0xBC, 0x120000, 0x70000, 0x92);
        MusicCurrent(0x8F);
        break;
    case 0xE:
        SH_CALL(Kind2_Place)(4);
        SetStep(0xF);
        break;
    case 0xF:
        if (Counter(0) != 1) break;
        SetCounter(0, 0);
        SetCounter(2, 0);
        SetStep(0x10);
        MusicCurrent(0xFF);
        ChangeArea(0xBC, 0xC0000, 0x590000, 0x93);
        MusicCurrent(0x8F);
        break;
    case 0x10:
        if (Counter(0) != 5) break;
        SH_CALL(Transition_Start)(0xD);
        SetStep(0x11);
        break;
    case 0x11:
        if (!WaitClear()) break;
        SetPass(0);
        SetStep(0x12);
        break;
    case 0x12:
        Say(0x17);
        SetStep(0x13);
        break;
    case 0x13:
        if (Request() == 2) break;
        SetRequest(6);
        SetStep(0x14);
        break;
    case 0x14:
        if (Request() != 0) break;
        ScriptXor(0x80);
        SetStep(0x1D);
        SH_CALL(Music_FadeOutStop)(0xA);
        Set(0x2C);
        PartyPass();
        SetPicked(0x32, false);
        SetCounter(2, 9);
        ChangeArea(0xBC, 0x370000, 0x3F8000, 0x94);
        MusicCurrent(0x8B);
        break;
    case 0x1D:
        SetPass(0x1F);
        SetStep(0x1E);
        break;
    case 0x1E:
        if (Counter(0) != 1) break;
        SH_CALL(AreaMap_SetByte)(0x38, 0x47, 0xC0);
        SH_CALL(AreaMap_SetByte)(0x38, 0x48, 0xC0);
        SH_CALL(AreaMap_SetByte)(0x39, 0x47, 0xA1);
        SH_CALL(AreaMap_SetByte)(0x39, 0x48, 0xA1);
        SH_CALL(AreaMap_SetByte)(0x37, 0x3F, 0xC0);
        SH_CALL(AreaMap_SetByte)(0x37, 0x40, 0xC0);
        SH_CALL(AreaMap_SetByte)(0x1F, 0x15, 0xA1);
        SH_CALL(AreaMap_SetByte)(0x1F, 0x16, 0xA1);
        SH_CALL(AreaMap_SetByte)(0x1C, 3, 0xC0);
        SH_CALL(AreaMap_SetByte)(0x1D, 3, 0xC0);
        ScriptXor(0x28);
        Clear40();
        EndCounters();
        break;
    default:
        break;
    }
}

namespace {

// Run 8 step 0: by Cond_ByteFD (2 or 3) and the leader's byte 0x802DC9
// (2..8), a message and the next step; 3 and anything else nothing.
void Run8Greet() {
    const unsigned fd = B(at::kCondFD);
    if (fd != 2 && fd != 3) return;
    const unsigned who = B(at::kLeaderName);
    if (who < 2 || who > 8 || who == 3) return;
    const bool odd = who == 5 || who == 8;
    unsigned short id;
    unsigned char step;
    if (fd == 3) {
        id = odd ? 0x19 : 0x18;
        step = odd ? 1 : 0xA;
    } else {
        id = odd ? 0x1A : 0x19;
        step = odd ? 0xA : 1;
    }
    Say(id);
    SetStep(step);
}

}  // namespace

// original 0x5608E0: run 8, steps 0..5, 0xA, 0x14, 0x15, 0x1E..0x20 (a byte
// table over 0..0x20, then 12 cases; step 0's two tables of seven by the
// leader's byte).
SC12_EXPORT void __cdecl Scena12_Run8(void) {
    switch (B(at::kStep)) {
    case 0:
        if (Request() == 2) break;
        Run8Greet();
        break;
    case 1: {
        if (Request() == 2) break;
        const unsigned c = Counter(0);
        if (c == 0xA) {
            SetCounter(0, 0);
            SetStep(2);
            SH_CALL(Transition_Start)(0xD);
            PartyPass();
            SH_CALL(Music_FadeOutStop)(0x1E);
        } else if (c == 5) {
            Clear40();
            SetCounter(0, 0);
            SetStep(0);
            SetRun(0);
        }
        break;
    }
    case 2:
        SH_CALL(Sound_LoadStream)(0);
        SetStep(3);
        break;
    case 3:
        if (!WaitClear()) break;
        SetPass(0);
        if (SH_CALL(Sound_StreamDone)() == 0) break;
        SetStep(4);
        break;
    case 4:
        SH_CALL(Transition_Start)(0xE);
        SetPass(0x1F);
        SetStep(5);
        break;
    case 5:
        if (!WaitClear()) break;
        SH_CALL(Music_Play)(0x8B, 8);
        EndRun();
        break;
    case 0xA:
        if (Request() == 2) break;
        Clear40();
        SetCounter(0, 0);
        SetStep(0);
        SetRun(0);
        break;
    case 0x14: {
        if (Request() != 0) break;
        const unsigned char load = B(at::kLoadByte);
        SetPass(0);
        if (load != 1) break;
        SetStep(0x15);
        SetPicked(0x32, true);
        ChangeArea(0xBC, 0xC8000, 0xA8000, 0x95);
        break;
    }
    case 0x15:
        SetPass(0x1F);
        Clear40();
        EndCounters();
        break;
    case 0x1E:
        if (Request() != 0) break;
        SetCounter(0, 1);
        Say(0x20);
        SetStep(0x1F);
        break;
    case 0x1F:
        if (Request() != 0) break;
        SetCounter(0, 2);
        SetStep(0x20);
        break;
    case 0x20:
        if (Counter(0) != 3) break;
        Clear40();
        EndCounters();
        break;
    default:
        break;
    }
}

namespace {

// Run 9 step 0xE: for each of the values 2, 8, 5, 4 in turn, a message (5, 4,
// 3, 2) per byte of the three at 0x904062 equal to it, each byte read afresh.
void Run9Roll() {
    static const unsigned char kValue[4] = {2, 8, 5, 4};
    static const unsigned short kMessage[4] = {5, 4, 3, 2};
    for (unsigned k = 0; k < 4; ++k)
        for (unsigned i = 0; i < 3; ++i)
            if (B(at::kPartyBytes + i) == kValue[k]) Msg(kMessage[k]);
}

void Place(int x, int z) { SH_AT(PlaceFn, at::kPartyPlace)(x, z, 0x26); }
void BattleBytes() { SH_AT(KindFn, at::kBattleBytes)(0x26); }

}  // namespace

// original 0x561170: run 9, steps 0..2, 0xA..0x12, 0x14..0x16, 0x1E..0x22,
// 0x2B..0x2F (a byte table over 0..0x2F, then 25 cases).
SC12_EXPORT void __cdecl Scena12_Run9(void) {
    switch (B(at::kStep)) {
    case 0:
        if (B(at::kMemberState) == 3) break;
        SH_CALL(Party_DropIn)(0);
        SetStep(1);
        break;
    case 1:
        if (Counter(0) != 1) break;
        Set(0x30);
        Clear40();
        SetCounter(0, 0);
        SetStep(0);
        SetRun(0);
        break;
    case 2:
        Clear40();
        EndCounters();
        break;
    case 0xA:
        SH_CALL(Music_FadeOutStop)(0xA);
        SH_CALL(Music_Play)(0xA4, 8);
        SH_CALL(Party_DropIn)(1);
        SH_CALL(Kind2_Place)(0);
        SetStep(0xB);
        break;
    case 0xB: {
        if (Counter(0) != 2) break;
        const unsigned char slot = SH_CALL(Effect_FindFree)();
        B(at::kEffectSlot) = slot;
        if (slot == 0xFF) break;
        SetStep(0xC);
        Place13(slot, -0x32E, AngleY(), 0x8A, 0x28);
        break;
    }
    case 0xC: {
        if (Counter(0) != 6) break;
        SH_CALL(Party_DropIn)(2);
        const unsigned char slot = SH_CALL(Effect_FindFree)();
        B(at::kEffectSlot) = slot;
        if (slot == 0xFF) break;
        SetCounter(3, slot);
        SetStep(0xD);
        Place13(slot, -0x2AA, AngleY(), 0x200, 0x28);
        break;
    }
    case 0xD:
        if (Effect(Counter(3))[0] != 0 || B(at::kHold) != 0) break;
        SetCounter(0, 7);
        SetStep(0xE);
        break;
    case 0xE:
        if (Counter(0) != 9) break;
        Run9Roll();
        SetRequest(2);
        SetStep(0xF);
        break;
    case 0xF:
        if (Request() != 0) break;
        SetCounter(0, 0xA);
        SetStep(0x10);
        break;
    case 0x10:
        B(at::kScriptFlags) = static_cast<unsigned char>(B(at::kScriptFlags) | 8);
        SetStep(0x11);
        break;
    case 0x11:
        if (Counter(0) != 0xE || B(at::kHold) != 0) break;
        Spawn(at::kEffectSlot, 0x82);
        Spawn(at::kEffectSlot, 0x83);
        Spawn(at::kEffectSlot, 0x84);
        SetStep(0x12);
        break;
    case 0x12: {
        if (Counter(1) == 0xFF) break;
        const unsigned held = W(at::kInputHeld);
        if (held == 0x3000 || held == 0x6000 || held == 0x2000) {
            if (B(at::kHold) != 0) break;
            SH_CALL(Party_DropIn)(7);
            SH_CALL(Kind2_Place)(3);
            SetStep(0x12);
        } else if (Counter(2) == 0x80) {
            SetStep(0x13);
        } else {
            SetCounter(2, 0);
            SetStep(0x12);
        }
        break;
    }
    case 0x14:
        if (B(at::kHold) != 0) break;
        SH_CALL(Party_DropIn)(4);
        SH_CALL(Kind2_Place)(4);
        SetStep(0x15);
        break;
    case 0x15:
        if (Counter(0) != 0x18) break;
        SetStep(0x16);
        Set(2);
        Place(0x348000, 0x258000);
        break;
    case 0x16:
        if (Counter(0) != 0x19) break;
        SetStep(0x17);
        BattleBytes();
        break;
    case 0x1E:
        if (B(at::kHold) != 0) break;
        SH_CALL(Party_DropIn)(3);
        SetStep(0x1F);
        break;
    case 0x1F:
        if (Counter(0) != 1) break;
        Place(static_cast<int>(D(at::kMember1) + 0x30000), 0x258000);
        BattleBytes();
        SetStep(0x20);
        break;
    case 0x20:
        if (Counter(0) != 5) break;
        Set(0x2D);
        ScriptXor(8);
        Clear40();
        SetCounter(1, 0);
        SetCounter(2, 0);
        SetCounter(3, 0);
        SetStep(0);
        SetRun(0);
        break;
    case 0x21:
        SetCounter(0, 0);
        SH_CALL(Kind2_Place)(2);
        Set(0x2D);
        SetStep(0x22);
        break;
    case 0x22: {
        if (Counter(0) != 1) break;
        const unsigned char slot = SH_CALL(Effect_FindFree)();
        B(at::kEffectSlot) = slot;
        if (slot == 0xFF) break;
        SetStep(0x2B);
        Place13(slot, -0x35A, AngleY(), 0x200, 0x14);
        break;
    }
    case 0x2B:
        if (Counter(0) != 2) break;
        SH_CALL(Transition_Start)(0xD);
        SetStep(0x2C);
        break;
    case 0x2C: {
        if (Counter(0) != 3) break;
        const unsigned char slot = SH_CALL(Effect_FindFree)();
        B(at::kEffectSlot) = slot;
        if (slot == 0xFF) break;
        SetStep(0x2D);
        Place13(slot, AngleX(), AngleY(), 0x2B0, 0x14);
        break;
    }
    case 0x2D:
        if (!WaitClear()) break;
        SetPass(0);
        SetStep(0x2E);
        break;
    case 0x2E:
        Say(0xB);
        SetStep(0x2F);
        break;
    case 0x2F:
        if (Request() != 0) break;
        SetCounter(2, 0);
        SetStep(0);
        SetRun(0);
        Set(0x3F);
        SH_AT(VoidFn, at::kSetBit80)();
        ChangeArea(0x7E, 0x500000, 0x1E0000, 4);
        break;
    default:
        break;
    }
}

void ScenaSc12_Inject() {
    if (bof3::WantsShadow("scena_sc12")) scena_sc12::SelfTest();
    BOF3_INJECT(Scena12_Frame);
    BOF3_INJECT(Scena12_EnterArea);
    BOF3_INJECT(Scena12_Run);
    BOF3_INJECT(Scena12_Run1);
    BOF3_INJECT(Scena12_Run2);
    BOF3_INJECT(Scena12_Run4);
    BOF3_INJECT(Scena12_Run5);
    BOF3_INJECT(Scena12_Run6);
    BOF3_INJECT(Scena12_Run7);
    BOF3_INJECT(Scena12_Run8);
    BOF3_INJECT(Scena12_Run9);
    BOF3_INJECT(Scena12_ObjectTrigger);
    BOF3_INJECT(Scena12_Object06);
    BOF3_INJECT(Scena12_Object09);
    BOF3_INJECT(Scena12_Object10);
    BOF3_INJECT(Scena12_Object11);
    BOF3_INJECT(Scena12_Object12);
    BOF3_INJECT(Scena12_Object13);
    BOF3_INJECT(Scena12_Object14);
    BOF3_INJECT(Scena12_ArriveHook);
    BOF3_INJECT(Scena12_StepHook);
    BOF3_INJECT(Scena12_CellHook);
    BOF3_INJECT(Scena12_CellTalk);
    BOF3_INJECT(Scena12_CellDoor);
}

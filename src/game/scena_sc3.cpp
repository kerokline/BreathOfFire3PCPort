// Scenario chapters 3 and 4 (the PSX's SCENA03 / SCENA04 overlays, compiled
// into the exe at 0x5428C0..0x54638D): chapter 3's vtable Scena03_Hooks
// 0x660F50 and chapter 4's Scena04_Hooks 0x660FD0 and everything they reach
// inside the band. docs/scena_sc3.md.
//
//   - Each chapter's slot 0 is a frame that jumps through its state table on
//     the s8 0x8034E2 (0 the start, 1 the area entry, 2 the run); the run
//     jumps through the run table on MoveScript_Var7 to a scene, a switch on
//     the step byte 0x8034E5 whose every case waits on one thing (a script
//     counter, the timer 0x8034E6, the message request, the wait word, a
//     stream), does one thing and sets the next step - Scena16_*'s shape.
//   - Slot 1 (the object trigger) dispatches on the object's +0x86 through a
//     table of handlers given (object, flag row); slots 2 and 3 (the step and
//     arrive hooks) test (x, z) by area and answer in al whether a scene
//     starts; slot 4 (the cell hook) asks 0x56D800 for the chapter's cell
//     record and runs its handler.
//
// Every call goes through the scenario harness (SH_CALL / SH_AT), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies.
// The .data tables are read in place and called through, as the originals do,
// unchecked: the fuzz swaps them for recorders. No divergence: each function
// is a faithful replacement; every read the original makes after a call is
// made after the same call here, every store before a call before it.
#include "game/scena_sc3.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/scena_sc3_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = scena_sc3::at;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

using Handler = void (__cdecl*)();
using ObjectHandler = std::uint32_t (__cdecl*)(unsigned char* object, unsigned char* bank);

// --- the cells --------------------------------------------------------------

unsigned char& Byte(std::uint32_t a) { return At(a)[0]; }
unsigned Area() { return Word(At(at::kArea)); }
void SetStep(unsigned v) { Byte(at::kStep) = static_cast<unsigned char>(v); }
void SetRun(unsigned v) { Byte(at::kRun) = static_cast<unsigned char>(v); }
unsigned Timer() { return Word(At(at::kTimer)); }
void SetTimer(unsigned v) { SetWord(At(at::kTimer), v); }
void DecTimer() { SetWord(At(at::kTimer), Word(At(at::kTimer)) - 1u); }
unsigned char& C0() { return Byte(at::kCounter0); }
unsigned char& C1() { return Byte(at::kCounter1); }
void ClearCounters() { SetLong(At(at::kCounters), 0); }
void PassFlags(unsigned v) { Byte(at::kPassFlags) = static_cast<unsigned char>(v); }
bool WaitDone() { return Word(At(at::kWait)) == 0; }
bool Requested() { return Byte(at::kRequest) == 2; }
unsigned Message() { return Word(At(at::kMessage)); }
void ScriptFlagsOr80() { Byte(at::kScriptFlags) = static_cast<unsigned char>(Byte(at::kScriptFlags) | 0x80); }
void ScriptFlagsAnd(unsigned mask) { SetWord(At(at::kScriptFlags), Word(At(at::kScriptFlags)) & mask); }
short S16(std::uint32_t a) { return static_cast<short>(Word(At(a))); }
std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }

// A table of code pointers in the exe's data, read as the original reads it:
// afresh, and indexed without a bound.
std::uint32_t Entry(std::uint32_t table, int index) {
    return static_cast<std::uint32_t>(Long(At(table + 4u * static_cast<std::uint32_t>(index))));
}

// The flag row, the dword 0x929ED0 read afresh at each call.
unsigned char* Bank() { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(At(at::kFlagBank))))); }
bool Flag(unsigned n) { return SH_CALL(Flags_Test)(Bank(), n) != 0; }
void SetFlag(unsigned n) { SH_CALL(Flags_Set)(Bank(), n); }

// --- the callees nobody owns, by address -------------------------------------

void CallB(unsigned n) { SH_AT(void (__cdecl*)(unsigned), scena_sc3::kCallB)(n); }
void PlaceParty(std::int32_t x, std::int32_t z, unsigned k) { SH_AT(void (__cdecl*)(std::int32_t, std::int32_t, unsigned), scena_sc3::kPlaceParty)(x, z, k); }
void PartyRestore() { SH_AT(void (__cdecl*)(), scena_sc3::kPartyRestore)(); }
void StatusBit80() { SH_AT(void (__cdecl*)(), scena_sc3::kStatusBit80)(); }
unsigned char CellFind(std::uint32_t records, unsigned n, int x, int z) {
    return static_cast<unsigned char>(SH_AT(std::uint32_t (__cdecl*)(std::uint32_t, unsigned, int, int), scena_sc3::kCellFind)(records, n, x, z));
}
unsigned char SpriteFindFree() { return static_cast<unsigned char>(SH_AT(std::uint32_t (__cdecl*)(), scena_sc3::kSpriteFindFree)()); }
void MusicStop() { SH_AT(void (__cdecl*)(), scena_sc3::kMusicStop)(); }
void ItemEvent(unsigned id, unsigned b, unsigned c, unsigned d) { SH_AT(void (__cdecl*)(unsigned, unsigned, unsigned, unsigned), scena_sc3::kItemEvent)(id, b, c, d); }
void KeyItemAdd(unsigned id) { SH_AT(void (__cdecl*)(unsigned), scena_sc3::kKeyItemAdd)(id); }
void LeaderState5(unsigned k) { SH_AT(void (__cdecl*)(unsigned), scena_sc3::kLeaderState5)(k); }
void EventObjFace() { SH_AT(void (__cdecl*)(), scena_sc3::kEventObjFace)(); }

// --- the effects ---------------------------------------------------------------

// Takes an effect slot into the byte 0x903850 and, when there is one, marks
// its record live as `kind`; the record is indexed by the stored byte read
// back as a dword's low byte, as the originals have it.
unsigned char* TakeEffect(unsigned char kind) {
    const unsigned char slot = SH_CALL(Effect_FindFree)();
    Byte(at::kSlot) = slot;
    if (slot == 0xFF) return nullptr;
    unsigned char* const e = At(at::kEffects + ((static_cast<std::uint32_t>(Long(At(at::kSlot))) & 0xFF) << 7));
    e[0] = 1;
    e[5] = kind;
    return e;
}
// Kind 0x13 with its three words (+0x64, +0x68, +0x6C) and +9.
void Effect13(std::int32_t a, std::int32_t b, std::int32_t c, unsigned char b9) {
    unsigned char* const e = TakeEffect(0x13);
    if (!e) return;
    SetLong(e + 0x64, a);
    SetLong(e + 0x68, b);
    SetLong(e + 0x6C, c);
    e[9] = b9;
}
// The effect whose slot is kept in the s8 0x8034E3: indexed by the answer as
// a signed byte (an answer of 0x80..0xFE would index below the pool).
unsigned char* TakeKeptEffect(unsigned char kind) {
    const unsigned char slot = SH_CALL(Effect_FindFree)();
    Byte(at::kEffectByte) = slot;
    if (slot == 0xFF) return nullptr;
    unsigned char* const e = At(at::kEffects + static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<signed char>(slot)) << 7));
    e[0] = 1;
    e[5] = kind;
    return e;
}

// The high word of a 16.16 coordinate, and "within n cells of lo" as the
// originals test it: a 16-bit subtraction compared unsigned.
unsigned Hi(int v) { return (static_cast<std::uint32_t>(v) >> 16) & 0xFFFF; }
bool Near(unsigned hi, unsigned lo, unsigned n) { return ((hi - lo) & 0xFFFF) < n; }

void ChangeArea(unsigned area, int x, int z, unsigned flags) { SH_CALL(Field_ChangeArea)(area, x, z, flags); }
void MusicPlayTrack() { SH_CALL(Music_Play)(Byte(at::kMusicTrack), 8); }

}  // namespace

// ===========================================================================
// Chapter 3
// ===========================================================================

// original 0x5428C0: slot 0 of Scena03_Hooks, Field_ModeDispatch's call - a
// tail jump through Scena03_States 0x660F64 on the s8 0x8034E2, unchecked.
extern "C" void __cdecl Scena03_Frame(void) {
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(Entry(at::kStates3, static_cast<signed char>(Byte(at::kState)))))();
}

// original 0x5428D0: state 0, the chapter's start - state 1 and chapter 3's
// flag row 0x903FA8 cleared.
extern "C" void __cdecl Scena03_Start(void) {
    Byte(at::kState) = 1;
    SetLong(At(at::kRow3), 0);
}

// original 0x5428F0: state 1, once per area entered. By the area number (read
// afresh at each test), the flags and Cond_ByteFD: the pass flags, map cells,
// the counters, the camera, a party drop-in, a scene armed (run and step);
// then state 2 on every way out.
extern "C" void __cdecl Scena03_EnterArea(void) {
    if (Area() == 0x25 && !Flag(0x15)) PassFlags(0x1F);
    if (Area() == 0x26) {
        SH_CALL(AreaMap_SetByte)(0xC, 0x1C, 0);
        SH_CALL(AreaMap_SetByte)(0xC, 0x1E, 0);
    }
    if (Area() == 0x27 && !Flag(3)) {
        ClearCounters();
        PassFlags(0);   // the test's answer, 0
    }
    if (Area() == 0x29 && !Flag(3)) {
        ClearCounters();
        SetTimer(0x3C);
        SetWord(At(at::kYaw), 0xFCBC);
        SetWord(At(at::kAngleY), 0);
        SetWord(At(at::kPitch), 0x10E);
        SH_CALL(Party_DropIn)(0);
    }
    if (Area() == 0x2D) {
        if (Flag(1) && !Flag(2)) {
            SH_CALL(ScriptFlags_Set40)();
            SH_CALL(Scenario_CallA)(0);
            ClearCounters();
            SH_CALL(MapView_SetElevation)(0x1C0);
            SetWord(At(at::kCamDistance), 0x2BC);
            SH_CALL(Kind2_Place)(1);
        }
        if (Flag(0x18) && !Flag(0x19)) {
            SH_CALL(ScriptFlags_Set40)();
            SetWord(At(at::kYaw), 0xFD14);
            SetWord(At(at::kPitch), 0x376);
            SH_CALL(Kind2_Place)(2);
            PassFlags(0x1F);
        }
    }
    const unsigned area = Area();
    if (area == 0x32) {
        ClearCounters();
        Byte(at::kState) = 2;
        return;
    }
    if (area == 0x38) {
        if (Byte(at::kByteFD) != 1) {
            Byte(at::kState) = 2;
            return;
        }
        if (!Flag(0xF)) {
            SetFlag(0xF);
            ClearCounters();
            SH_CALL(Party_DropIn)(0);
        }
    }
    if (Area() == 0x45) {
        if (Byte(at::kByteFD) == 0) {
            if (!Flag(0x16)) {
                SH_CALL(Party_DropIn)(3);
                SetFlag(0x16);
            } else if (Byte(at::kKind2Mode) == 1 && !Flag(0x18)) {
                SH_CALL(ScriptFlags_Set40)();
                SH_CALL(Music_FadeOutStop)(8);
                SH_CALL(Music_Play)(0x14, 8);
                ScriptFlagsOr80();
                SH_CALL(Party_DropIn)(5);
                SetFlag(0x18);
                SetRun(5);
                SetStep(2);
            }
        }
        if (Byte(at::kByteFD) == 1 && !Flag(0x17)) {
            CallB(0);
            SH_CALL(Party_DropIn)(4);
            SH_CALL(Sound_PlayEffect)(0x201);
            SetFlag(0x17);
        }
    }
    if (Area() == 0x47 && !Flag(0x1E)) {
        SH_CALL(AreaMap_SetByte)(0x19, 0x77, 0x50);
        SH_CALL(AreaMap_SetByte)(0x19, 0x78, 0x50);
        SH_CALL(AreaMap_SetByte)(0x19, 0x79, 0x50);
    }
    if (Area() == 0x63 && !Flag(0x1B)) {
        const unsigned char fd = Byte(at::kByteFD);
        if (fd == 2) {
            Byte(at::kState) = 2;
            PassFlags(0x1F);
            return;
        }
        if (fd == 3) {
            SH_CALL(ScriptFlags_Set40)();
            ClearCounters();
            SetLong(At(at::kKind2X), 0x58000);
            SetLong(At(at::kKind2Z), 0xB8000);
            SH_CALL(Party_DropIn)(1);
            unsigned char* const bank = Bank();
            PassFlags(0);
            SH_CALL(Flags_Set)(bank, 0x1B);
            SetTimer(0x1E);
            SetRun(6);
            SetStep(6);
        }
    }
    Byte(at::kState) = 2;
}

// original 0x542CA0: state 2, the run - a tail jump through Scena03_Runs
// 0x660F70 on MoveScript_Var7 (s8), unchecked.
extern "C" void __cdecl Scena03_Run(void) {
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(Entry(at::kRuns3, static_cast<signed char>(Byte(at::kRun)))))();
}

// original 0x542CB0: run 1, steps 0..7 (a jump table of 8 after it at
// 0x542EF8): a drop-in, three effects of kind 0x14 with sound 0x203 on
// counter 0 = 0x28 / 0x29 / 0x2A, on 0x32 the event 0x590C90(0x97, ...), two
// flags and the change to area 0x2D, then an effect of kind 0x13, 0x20
// frames, and on counter 2 the change to area 0x27 and run 3.
extern "C" void __cdecl Scena03_Scene1(void) {
    switch (Byte(at::kStep)) {
    case 0:
        SH_CALL(Party_DropIn)(0);
        C0() = 1;
        SetStep(1);
        return;
    case 1:
    case 2:
    case 3: {
        const unsigned step = Byte(at::kStep);
        if (C0() != 0x27 + step) return;
        if (TakeEffect(0x14)) SH_CALL(Sound_PlayEffect)(0x203);
        SetStep(step + 1);
        return;
    }
    case 4: {
        if (C0() != 0x32) return;
        const unsigned state = Byte(at::kEffectState);
        ItemEvent(0x97, state, 0, 0);
        SH_CALL(Flags_Set)(At(at::kStoryFlags), 0);
        SetFlag(1);
        ChangeArea(0x2D, 0x280000, 0x350000, 0x86);
        Byte(at::kAreaTrack) = 0x2F;
        Byte(at::kPendingKind) = 0xFF;
        SetStep(5);
        return;
    }
    case 5:
        Effect13(-0x31A, S16(at::kAngleY), 0x360, 0xF0);
        SetTimer(0x20);
        SetStep(6);
        return;
    case 6:
        if (Timer() != 0) {
            DecTimer();
            return;
        }
        SetStep(7);
        return;
    case 7:
        if (C0() != 2) return;
        SetFlag(2);
        ScriptFlagsOr80();
        ChangeArea(0x27, 0x290000, 0x270000, 5);
        Byte(at::kPendingKind) = 0xFF;
        Byte(at::kAreaTransition) = 0;
        SetRun(3);
        SetStep(0);
        return;
    default:
        return;
    }
}

// original 0x542F20: run 2 - step 0 a drop-in and counter 0 = 10; step 1 on
// counter 0x28 run 1 step 1.
extern "C" void __cdecl Scena03_Scene2(void) {
    const unsigned step = Byte(at::kStep);
    if (step == 0) {
        SH_CALL(Party_DropIn)(1);
        C0() = 0xA;
        SetStep(1);
    } else if (step == 1 && C0() == 0x28) {
        SetRun(1);
        SetStep(1);
    }
}

// original 0x542F60: run 3, a two-level switch (the byte table 0x543480 of
// 0x33 steps, the jump table 0x543418 of 26): steps 0..0x10, 0x14..0x1A and
// 0x32; 0x11..0x13 and 0x1B..0x31 do nothing.
extern "C" void __cdecl Scena03_Scene3(void) {
    switch (Byte(at::kStep)) {
    case 0:
        if (!WaitDone()) return;
        SH_CALL(Msg_OpenScript)(1);
        Byte(at::kRequest) = 2;
        SetStep(1);
        return;
    case 1:
        if (Requested()) return;
        SH_CALL(Transition_Start)(1);
        PassFlags(0x1F);
        SH_CALL(Party_DropIn)(0);
        SH_CALL(Kind2_Place)(0);
        SetStep(2);
        return;
    case 2:
        if (C0() != 0x22) return;
        PassFlags(0);
        SH_CALL(Sound_PlayEffect)(0x203);
        SetTimer(0x2D);
        SetStep(3);
        return;
    case 3:
        DecTimer();
        if (Timer() != 0) return;
        ScriptFlagsAnd(0xFF7F);
        ChangeArea(0x29, 0x230000, 0x2D0000, 5);
        Byte(at::kAreaTrack) = 0x31;
        SetStep(4);
        return;
    case 4:
        if (!WaitDone()) return;
        if (Timer() != 0) {
            DecTimer();
            return;
        }
        SH_CALL(Transition_Start)(1);
        PassFlags(0x1F);
        C0() = 1;
        SetStep(5);
        return;
    case 5:
        if (C0() != 2) return;
        Effect13(-0x2AA, S16(at::kAngleY), 0x200, 0x80);
        SetStep(0x32);
        return;
    case 0x32:
        if (C0() != 9) return;
        SH_CALL(Music_FadeOutStop)(8);
        SH_CALL(Music_Play)(0x55, 8);
        SetStep(6);
        return;
    case 6:
        if (C0() != 0x18) return;
        SetFlag(4);
        SetStep(7);
        return;
    case 7:
        if (C0() != 0x25) return;
        SetFlag(3);
        C1() = 0;
        PartyRestore();
        SH_CALL(ScriptFlags_Clear40)();
        SetStep(8);
        return;
    case 8:
        if (C1() != 0x15) return;
        SetFlag(5);
        SetStep(9);
        return;
    case 9: {
        if (C0() != 0x2B) return;
        unsigned char* const bank = Bank();
        ScriptFlagsOr80();
        SH_CALL(Flags_Set)(bank, 6);
        SetRun(0);
        SetStep(0);
        return;
    }
    case 0xA:
        SH_CALL(ScriptFlags_Set40)();
        SH_CALL(Party_DropIn)(5);
        SetStep(0xB);
        return;
    case 0xB: {
        if (C0() != 0xF) return;
        const std::int32_t z = Long(At(at::kMembers + 0x38));
        const std::int32_t x = Long(At(at::kMembers + 0x34));
        PlaceParty(x, z, 0x10);
        LeaderState5(0x10);
        SetStep(0xC);
        return;
    }
    case 0xC: {
        const unsigned char c = C0();
        if (c <= 0x13) return;
        if (c == 0x14) {
            SetLong(At(at::kKind2X), Long(At(at::kMembers + at::kMemberStride + 0x34)));
            SetLong(At(at::kKind2Z), Long(At(at::kMembers + at::kMemberStride + 0x38)));
            SetWord(At(at::kF3Divisor), 0x20);
            SetStep(0xD);
            return;
        }
        SH_CALL(Transition_Start)(0);
        SetStep(0x14);
        return;
    }
    case 0xD:
        if (Byte(at::kKind2Hold) != 0) return;
        C0() = 0x15;
        SetStep(0xE);
        return;
    case 0xE:
        if (C0() != 0x1D) return;
        MusicStop();
        SH_CALL(Sound_LoadStream)(3);
        SetStep(0xF);
        return;
    case 0xF:
        if (SH_CALL(Sound_StreamDone)() == 0) return;
        MusicPlayTrack();
        C0() = static_cast<unsigned char>(C0() + 1);
        SetStep(0x10);
        return;
    case 0x10:
        if (C0() != 0x1F) return;
        C0() = 0x58;
        SetStep(0x1A);
        return;
    case 0x14:
        if (!WaitDone()) return;
        PassFlags(0);
        SetLong(At(at::kKind2X), 0x5E0000);
        SetLong(At(at::kKind2Z), 0x80000);
        SetWord(At(at::kF3Divisor), 0x20);
        SetTimer(0x78);
        SetStep(0x15);
        return;
    case 0x15:
        if (Timer() != 0) {
            DecTimer();
            return;
        }
        SH_CALL(Msg_OpenScript)(0x32);
        Byte(at::kRequest) = 2;
        SetStep(0x16);
        return;
    case 0x16:
        if (Requested()) return;
        SH_CALL(Transition_Start)(1);
        PassFlags(0x1F);
        PartyRestore();
        C0() = 0x51;
        SetStep(0x17);
        return;
    case 0x17:
        if (WaitDone()) SetStep(0x18);
        return;
    case 0x18:
        if (C0() != 0x55) return;
        MusicStop();
        SH_CALL(Sound_LoadStream)(3);
        SetStep(0x19);
        return;
    case 0x19:
        if (SH_CALL(Sound_StreamDone)() == 0) return;
        MusicPlayTrack();
        C0() = static_cast<unsigned char>(C0() + 1);
        SetStep(0x1A);
        return;
    case 0x1A:
        if (C0() != 0x58) return;
        SH_CALL(ScriptFlags_Clear40)();
        ScriptFlagsAnd(0xFF7F);
        SetRun(0);
        SetStep(0);
        return;
    default:
        return;
    }
}

// original 0x5434C0: run 4, steps 0..10 (the jump table 0x543660 of 11;
// step 9 does nothing).
extern "C" void __cdecl Scena03_Scene4(void) {
    switch (Byte(at::kStep)) {
    case 0:
        SH_CALL(Party_DropIn)(0);
        SetStep(1);
        return;
    case 1:
        if (C0() != 4) return;
        SH_CALL(Transition_Start)(0);
        SetTimer(0x3C);
        SetStep(2);
        return;
    case 2:
        if (!WaitDone()) return;
        SH_CALL(Msg_OpenScript)(0x20);
        Byte(at::kRequest) = 2;
        PassFlags(0);
        SetStep(3);
        return;
    case 3:
        if (Requested()) return;
        ChangeArea(0x25, 0x2D0000, 0x280000, 0x81);
        Byte(at::kAreaTransition) = 0xFF;
        SetStep(4);
        return;
    case 4:
        if (!WaitDone()) return;
        C0() = static_cast<unsigned char>(C0() + 1);
        SH_CALL(ScriptFlags_Clear40)();
        SetStep(5);
        return;
    case 5:
        if (Requested() && Message() == 0x33) SetStep(0xA);
        return;
    case 6:
        if (Timer() != 0) {
            DecTimer();
            return;
        }
        SH_CALL(ScriptFlags_Set40)();
        SetFlag(0x15);
        C0() = 1;
        ChangeArea(0x25, 0x2D0000, 0x280000, 0x82);
        SetStep(7);
        return;
    case 7: {
        if (!WaitDone()) return;
        const unsigned char c = C0();
        SetStep(8);
        C0() = static_cast<unsigned char>(c + 1);
        return;
    }
    case 8:
        if (C0() != 0xF) return;
        C0() = 0;
        SH_CALL(ScriptFlags_Clear40)();
        SetRun(0);
        SetStep(0);
        return;
    case 0xA:
        if (Requested()) return;
        ScriptFlagsAnd(0xFEFF);
        SetStep(5);
        return;
    default:   // 9, and 11 and up
        return;
    }
}

// original 0x543690: run 5, steps 0..0x11 (the jump table 0x543A9C of 18).
extern "C" void __cdecl Scena03_Scene5(void) {
    switch (Byte(at::kStep)) {
    case 0:
        if (Requested() && Message() == 0x12) SetStep(1);
        return;
    case 1:
        if (Requested()) return;
        ScriptFlagsAnd(0xFEFF);
        SetStep(0);
        return;
    case 2:
        if (C0() != 8) return;
        PassFlags(0);
        SetTimer(0xA);
        SetStep(3);
        return;
    case 3:
        if (Timer() != 0) {
            DecTimer();
            return;
        }
        ScriptFlagsOr80();
        ChangeArea(0x2D, 0x280000, 0x220000, 0x86);
        Byte(at::kPendingKind) = 1;
        Byte(at::kAreaTransition) = 0xFF;
        SetStep(4);
        return;
    case 4:
        if (C0() != 9) return;
        ScriptFlagsAnd(0xFF7F);
        ChangeArea(0x32, 0x290000, 0x250000, 5);
        Byte(at::kPendingKind) = 0xFE;
        Byte(at::kAreaTransition) = 0;
        SetStep(5);
        return;
    case 5:
        if (!WaitDone()) return;
        SH_CALL(ScriptFlags_Set40)();
        SH_CALL(ObjTrio_SetBit40)();
        SH_CALL(Msg_OpenScript)(0xA);
        Byte(at::kRequest) = 2;
        SetStep(6);
        return;
    case 6:
        if (Requested()) return;
        SH_CALL(Transition_Start)(1);
        PassFlags(0x1F);
        SetTimer(0x3C);
        SetStep(7);
        return;
    case 7:
        if (Timer() != 0) {
            DecTimer();
            return;
        }
        SH_CALL(Kind2_Place)(1);
        SetStep(8);
        return;
    case 8:
        if (C0() != 2) return;
        Effect13(-0x2EC, S16(at::kAngleY), 0x376, 0x80);
        SetTimer(0x3C);
        SetStep(9);
        return;
    case 9:
        if (C0() != 3) return;
        if (Timer() != 0) {
            DecTimer();
            return;
        }
        ChangeArea(0x32, 0xB0000, 0x610000, 5);
        Byte(at::kPendingKind) = 0xFE;
        SetStep(0xA);
        return;
    case 0xA:
        if (!WaitDone()) return;
        SH_CALL(ObjTrio_ClearBit40)();
        SH_CALL(Msg_OpenScript)(0xB);
        Byte(at::kRequest) = 2;
        SetStep(0xB);
        return;
    case 0xB:
        if (Requested()) return;
        PassFlags(0x1F);
        SH_CALL(Transition_Start)(1);
        SH_CALL(Scenario_CallA)(1);
        SH_CALL(Party_DropIn)(3);
        C0() = 1;
        SetStep(0xC);
        return;
    case 0xC:
        if (C0() != 5) return;
        Effect13(-0x1B8, S16(at::kAngleY), 0xB4, 0x60);
        SetStep(0xD);
        return;
    case 0xD:
        if (C0() != 6) return;
        Effect13(-0x2AA, S16(at::kAngleY), 0x200, 0x60);
        SetStep(0xE);
        return;
    case 0xE:
        if (C0() != 0x15) return;
        SetFlag(0x19);
        ChangeArea(0x32, 0x170000, 0x360000, 0x84);
        SetStep(0xF);
        return;
    case 0xF:
        if (C0() != 7) return;
        SetFlag(0x1A);
        SetStep(0);
        SH_CALL(ScriptFlags_Clear40)();
        return;
    case 0x10:
        if (Requested() && Message() == 0x1F) SetStep(0x11);
        return;
    case 0x11:
        if (Requested()) return;
        ScriptFlagsAnd(0xFEFF);
        SetRun(0);
        SetStep(0);
        return;
    default:
        return;
    }
}

// original 0x543AF0: run 6, steps 0..0x1C (the jump table 0x543FC4 of 29).
extern "C" void __cdecl Scena03_Scene6(void) {
    switch (Byte(at::kStep)) {
    case 0:
        if (Requested()) return;
        C0() = 0;
        SH_CALL(Transition_Start)(0);
        SetStep(1);
        return;
    case 1:
        if (!WaitDone()) return;
        PassFlags(0);
        SetTimer(0x3C);
        SetStep(2);
        return;
    case 2:
        if (Timer() != 0) {
            DecTimer();
            return;
        }
        ChangeArea(0x63, 0x60000, 0x260000, 0x80);
        {
            const unsigned char bits = static_cast<unsigned char>(Byte(at::kStatusBits) | 1);
            SetStep(3);
            Byte(at::kStatusBits) = bits;
        }
        return;
    case 3:
        if (C0() != 0xA) return;
        SH_CALL(ScriptFlags_Clear40)();
        SetRun(0);
        SetStep(0);
        return;
    case 4:
        if (Requested() && Message() == 0xC) SetStep(5);
        return;
    case 5:
        if (Requested()) return;
        ScriptFlagsAnd(0xFEFF);
        C1() = 0;
        SetRun(0);
        SetStep(0);
        return;
    case 6:
        if (Timer() != 0) {
            DecTimer();
            return;
        }
        PassFlags(0x1F);
        C0() = 1;
        SH_CALL(Transition_Start)(1);
        SetStep(7);
        return;
    case 7:
        if (C0() != 5) return;
        SH_CALL(Transition_Start)(0);
        SetStep(8);
        return;
    case 8:
        if (!WaitDone()) return;
        PassFlags(0);
        SetTimer(0x3C);
        SetStep(9);
        return;
    case 9:
        if (Timer() != 0) {
            DecTimer();
            return;
        }
        SH_CALL(Music_FadeOutStop)(8);
        SH_CALL(Sound_LoadStream)(0);
        PartyRestore();
        SetStep(0xA);
        return;
    case 0xA: {
        if (SH_CALL(Sound_StreamDone)() == 0) return;
        const unsigned char bits = Byte(at::kStatusBits);
        PassFlags(0x1F);
        Byte(at::kStatusBits) = static_cast<unsigned char>(bits & 0xFE);
        SH_CALL(AreaMap_SetupEntries)();
        SH_CALL(Transition_Start)(1);
        SetStep(0xB);
        return;
    }
    case 0xB:
        if (!WaitDone()) return;
        C0() = 6;
        SetStep(0xC);
        return;
    case 0xC:
        if (C0() != 8) return;
        ScriptFlagsOr80();
        SH_CALL(ScriptFlags_Clear40)();
        C0() = 0;
        SetRun(0);
        SetStep(0);
        return;
    case 0xD:
        if (Requested() && Message() == 0x11) SetStep(0xE);
        return;
    case 0xE:
        if (Requested()) return;
        ScriptFlagsAnd(0xFEFF);
        SetRun(0);
        SetStep(0);
        return;
    case 0xF:
        SH_CALL(Party_DropIn)(2);
        SetStep(0x10);
        return;
    case 0x10:
        if (C0() != 1) return;
        PassFlags(0);
        SetStep(0x11);
        return;
    case 0x11: {
        ChangeArea(0x63, 0x68000, 0x230000, 0x83);
        Byte(at::kPendingKind) = 0xFF;
        Byte(at::kAreaTransition) = 0xFF;
        SetStep(0x12);
        return;
    }
    case 0x12:
        PassFlags(0x1F);
        SetTimer(0x3C);
        SetStep(0x13);
        return;
    case 0x13: {
        const unsigned t = Timer();
        if (t == 0) {
            PassFlags(0);
            SetStep(0x14);
            return;
        }
        if (t == 0x32) SH_CALL(Sound_PlayEffect)(0x201);
        if (Timer() == 0xA) SH_CALL(Sound_PlayEffect)(0x201);
        DecTimer();
        return;
    }
    case 0x14:
        SetLong(At(at::kKind2X), 0x58000);
        SetLong(At(at::kKind2Z), 0xB8000);
        SH_CALL(Field_ViewReset)();
        SetTimer(0xA);
        SetStep(0x15);
        return;
    case 0x15:
    case 0x18:
    case 0x1B: {
        if (Timer() != 0) {
            DecTimer();
            return;
        }
        const unsigned step = Byte(at::kStep);
        const unsigned char c = C0();
        PassFlags(0x1F);
        SetStep(step + 1);
        C0() = static_cast<unsigned char>(c + 1);
        return;
    }
    case 0x16:
        if (C0() != 4) return;
        PassFlags(0);
        SetStep(0x17);
        return;
    case 0x17:
        SetLong(At(at::kKind2X), 0x68000);
        SetLong(At(at::kKind2Z), 0x230000);
        SH_CALL(Field_ViewReset)();
        SetTimer(0xA);
        SetStep(0x18);
        return;
    case 0x19:
        if (C0() != 9) return;
        PassFlags(0);
        SetStep(0x1A);
        return;
    case 0x1A:
        SetLong(At(at::kKind2X), 0x60000);
        SetLong(At(at::kKind2Z), 0x90000);
        SH_CALL(Field_ViewReset)();
        SetStep(0x1B);
        return;
    case 0x1C: {
        if (C0() != 0xD) return;
        SH_CALL(ScriptFlags_Clear40)();
        unsigned char* const bank = Bank();
        Byte(at::kByteFD) = 3;
        SH_CALL(Flags_Set)(bank, 0x1C);
        SetRun(0);
        SetStep(0);
        return;
    }
    default:
        return;
    }
}

// original 0x544040: run 7, steps 0..0xD (the jump table 0x5442F4 of 14).
extern "C" void __cdecl Scena03_Scene7(void) {
    switch (Byte(at::kStep)) {
    case 0:
        if (Requested()) return;
        if (Timer() != 0) {
            DecTimer();
            return;
        }
        C0() = 0;
        SH_CALL(Party_DropIn)(4);
        SetStep(1);
        return;
    case 1:
        if (C0() != 2) return;
        PassFlags(0);
        SetStep(2);
        return;
    case 2:
        ChangeArea(0x63, 0x68000, 0x230000, 1);
        Byte(at::kPendingKind) = 0xFF;
        Byte(at::kAreaTransition) = 0xFF;
        SetStep(3);
        return;
    case 3:
        PassFlags(0x1F);
        SetStep(4);
        return;
    case 4:
        if (C0() != 7) return;
        ChangeArea(0x63, 0x230000, 0x510000, 0x85);
        SetStep(5);
        return;
    case 5:
        if (C0() != 0x12) return;
        {
            unsigned char* const e = TakeEffect(0x13);
            if (e) {
                const std::int32_t y = S16(at::kAngleY), z = S16(at::kPitch);
                SetLong(e + 0x64, -0x32E);
                SetLong(e + 0x68, y);
                SetLong(e + 0x6C, z);
                e[9] = 0x38;
            }
        }
        SetStep(6);
        return;
    case 6:
        if (C0() != 0x16) return;
        if (unsigned char* const e = TakeKeptEffect(0x22)) {
            SetLong(e + 0x34, 0x230000);
            SetLong(e + 0x38, 0x4F0000);
            SetLong(e + 0x3C, 0x4800000);
        }
        SetStep(7);
        return;
    case 7:
        if (C0() != 0x18) return;
        {
            unsigned char* const e = TakeEffect(0x13);
            if (e) {
                const std::int32_t y = S16(at::kAngleY), z = S16(at::kPitch);
                SetLong(e + 0x64, -0x2AA);
                SetLong(e + 0x68, y);
                SetLong(e + 0x6C, z);
                e[9] = 0x38;
            }
        }
        SetStep(8);
        return;
    case 8:
        if (C0() != 0x27) return;
        PassFlags(0);
        SetStep(9);
        return;
    case 9:
        SetTimer(0xA);
        SetStep(0xA);
        C0() = static_cast<unsigned char>(C0() + 1);
        return;
    case 0xA: {
        if (Timer() != 0) {
            DecTimer();
            return;
        }
        const unsigned char c = C0();
        PassFlags(0x1F);
        SetStep(0xB);
        C0() = static_cast<unsigned char>(c + 1);
        return;
    }
    case 0xB:
        if (C0() == 0x2F) SetStep(0xC);
        return;
    case 0xC:
        SetStep(0xD);
        return;
    case 0xD: {
        if (C0() != 0x34) return;
        SH_CALL(ScriptFlags_Clear40)();
        unsigned char* const bank = Bank();
        C0() = 0;
        SH_CALL(Flags_Set)(bank, 0x1D);
        ScriptFlagsAnd(0xFF7F);
        SetRun(0);
        SetStep(0);
        return;
    }
    default:
        return;
    }
}

namespace {

// Scena03_Scene8's shared tails: the stand-ins spawned at members 0 and 1,
// then (or only) the party bobbed on the two-entry table and the timer on.
void Scene8Spawn() {
    SH_CALL(Scena03_SpawnAtMember)(0);
    SH_CALL(Scena03_SpawnAtMember)(1);
}
void Scene8Bob2() {
    SH_CALL(Scena03_BobParty2)();
    SetTimer(Timer() + 1);
}
// Timer's low byte & 1 == 1: spawn, then bob; else bob.
void Scene8Odd() {
    if ((Byte(at::kTimer) & 1) == 1) Scene8Spawn();
    Scene8Bob2();
}

}  // namespace

// original 0x544330: run 8, steps 0..0x14 (the jump table 0x5447D0 of 21,
// entries 14 and 15 the same case): a long timed shake of the party with
// sounds at fixed timer values, stand-ins spawned at members 0 and 1 on
// alternate frames, effects of kinds 0x13, 0x23 and 0x25, and at the end the
// music 0x51, flags 0x1E and 0x904650's 6, and a tail jump to 0x56D6F0.
extern "C" void __cdecl Scena03_Scene8(void) {
    switch (Byte(at::kStep)) {
    case 0:
        if (Requested() && Message() == 1) SetStep(1);
        return;
    case 1:
        if (Requested()) return;
        ScriptFlagsAnd(0xFEFF);
        SetRun(0);
        SetStep(0);
        return;
    case 2:
        if (Timer() != 0) {
            DecTimer();
            return;
        }
        SetStep(3);
        return;
    case 3:
        SH_CALL(Music_FadeOut)(0x20);
        SH_CALL(Party_DropIn)(0);
        SetStep(4);
        return;
    case 4:
        if (C0() != 0x10) return;
        Effect13(-0x344, S16(at::kAngleY), 0x192, 0x10);
        SetTimer(0x10);
        SetStep(5);
        return;
    case 5:
        if (C0() != 0x13) return;
        if (Timer() != 0) {
            DecTimer();
            SetWord(At(at::kCamDistance), Word(At(at::kCamDistance)) + 0x40u);
            Byte(at::kRedraw) = 2;
            return;
        }
        SetTimer(6);
        SetStep(6);
        return;
    case 6:
        if (Timer() != 0) {
            DecTimer();
            return;
        }
        SetStep(7);
        return;
    case 7:
        if (Timer() == 0x20) {
            SH_CALL(Sound_PlayEffect)(0x201);
            SetStep(8);
            return;
        }
        if ((Byte(at::kTimer) & 3) == 3) Scene8Spawn();
        SH_CALL(Scena03_BobParty4)();
        SetTimer(Timer() + 1);
        return;
    case 8:
        if (Timer() == 0x30) {
            SetStep(9);
            return;
        }
        Scene8Odd();
        return;
    case 9:
        if (Timer() == 0x50) {
            SH_CALL(Sound_PlayEffect)(0x201);
            SetStep(0xA);
            return;
        }
        Scene8Odd();
        return;
    case 0xA:
        if (Timer() == 0x90) {
            SH_CALL(Sound_PlayEffect)(0x204);
            SetStep(0xB);
            TakeEffect(0x23);
            return;
        }
        Scene8Odd();
        return;
    case 0xB:
        if (Timer() != 0xA0) {
            Scene8Odd();
            return;
        }
        SetStep(0xC);
        SH_CALL(Sound_PlayEffect)(0x201);
        SH_CALL(Sound_PlayEffect)(0x205);
        return;
    case 0xC:
        if (Timer() == 0x110) {
            SH_CALL(Sound_PlayEffect)(0x202);
            SH_CALL(Sound_PlayEffect)(0x206);
            SetStep(0xD);
            return;
        }
        Scene8Odd();
        return;
    case 0xD:
        if (Timer() == 0x130) {
            SetStep(0xE);
            return;
        }
        if ((Byte(at::kTimer) & 3) == 1) Scene8Spawn();
        Scene8Bob2();
        return;
    case 0xE:
    case 0xF: {
        const unsigned char c = C0();
        if (c == 0x15) {
            Effect13(-0x2AA, S16(at::kAngleY), 0x200, 0x3C);
            SetStep(0x10);
            return;
        }
        if (c > 0x15) return;
        Scene8Spawn();
        return;
    }
    case 0x10:
        if (C0() != 0x17) return;
        PassFlags(0);
        SH_CALL(Music_FadeOutStop)(8);
        SetStep(0x11);
        return;
    case 0x11:
        SetLong(At(at::kKind2X), 0x240000);
        SetLong(At(at::kKind2Z), 0x4A0000);
        SH_CALL(Field_ViewReset)();
        SetTimer(0xA);
        SetStep(0x12);
        return;
    case 0x12: {
        if (Timer() != 0) {
            DecTimer();
            return;
        }
        PassFlags(0x1F);
        SH_CALL(Transition_Start)(1);
        const unsigned char c = C0();
        C0() = static_cast<unsigned char>(c + 1);
        SH_CALL(Music_Play)(0x49, 8);
        SetStep(0x13);
        return;
    }
    case 0x13:
        if (C0() != 0x1C) return;
        TakeEffect(0x25);
        SetStep(0x14);
        return;
    case 0x14:
        if (C0() != 0x27) return;
        SH_CALL(Music_FadeOutStop)(8);
        SH_CALL(Music_Play)(0x51, 8);
        SetFlag(0x1E);
        SH_CALL(Flags_Set)(At(at::kStoryFlags), 6);
        SH_CALL(ScriptFlags_Clear40)();
        C0() = 0;
        SetRun(0);
        SetStep(0);
        StatusBit80();
        return;
    default:
        return;
    }
}

// original 0x544830 (Scena03_Scene8's): a stand-in sprite at party member
// `member` (the byte): a free Sprite_Objects record from 0x57CD90 (0 when
// none), made current and the active member, reset, animation bank 0x67, at
// the member's x + 0x8000 and z, on the map's elevation there, state 4,
// flags 0x88 (EventObj_SetFlags, from a byte of the original's own frame),
// then faced (0x579D70); 1.
extern "C" unsigned char __cdecl Scena03_SpawnAtMember(unsigned char member) {
    const unsigned char index = SpriteFindFree();
    if (index == 0xFF) return 0;
    unsigned char* const rec = At(at::kSprites + index * at::kSpriteStride);
    SetLong(At(at::kSpriteCurrent), static_cast<std::int32_t>(Key(rec)));
    SetLong(At(at::kActiveMember), static_cast<std::int32_t>(Key(rec)));
    SH_CALL(EventObj_Reset)();
    SH_CALL(Sprite_SetAnimationBank)(0x67);
    const std::uint32_t m = static_cast<unsigned char>(*reinterpret_cast<const volatile unsigned char*>(&member)) * at::kMemberStride;
    auto cur = [] { return move_script::At(static_cast<std::uint32_t>(Long(At(at::kSpriteCurrent)))); };
    cur()[0] = 1;
    SetLong(cur() + 0x34, Long(At(at::kMembers + m + 0x34)) + 0x8000);
    const std::int32_t z = Long(At(at::kMembers + m + 0x38));
    SetLong(rec + 0x8C, Long(cur() + 0x34));
    SetLong(cur() + 0x38, z);
    SetLong(rec + 0x90, Long(cur() + 0x38));
    SetLong(rec + 0x94, 0);
    const long elevation = SH_CALL(AreaMap_Elevation)(Long(cur() + 0x34), Long(cur() + 0x38));
    SetWord(cur() + 0x3E, static_cast<unsigned>(elevation));
    cur()[1] = 4;
    cur()[2] = 0;
    cur()[8] = 0;
    cur()[6] = 0;
    SetWord(rec + 0x9A, 0);
    SetWord(rec + 0x98, 0);
    SetWord(rec + 0x88, 0);
    rec[0x83] = 2;
    rec[0x84] = 2;
    const unsigned char flags = 0x88;
    SH_CALL(EventObj_SetFlags)(&flags);
    cur()[0] = static_cast<unsigned char>(cur()[0] | 0x20);
    cur()[0x5F] = 0;
    cur()[0x5E] = 0;
    cur()[0x5D] = 0;
    cur()[0x5C] = 1;
    EventObjFace();
    return 1;
}

namespace {
// Members 0 and 1 bobbed: the s8 at `table` + (the timer's low byte & mask),
// times 16, added to each one's +0x3E (a 16-bit add).
void Bob(std::uint32_t table, unsigned mask) {
    const unsigned d = static_cast<unsigned>(static_cast<int>(static_cast<signed char>(Byte(table + (Byte(at::kTimer) & mask)))) << 4);
    SetWord(At(at::kMembers + 0x3E), Word(At(at::kMembers + 0x3E)) + d);
    SetWord(At(at::kMembers + at::kMemberStride + 0x3E), Word(At(at::kMembers + at::kMemberStride + 0x3E)) + d);
}
}  // namespace

// original 0x544990: Scena03_Bob4 0x660F94 by the timer & 3.
extern "C" void __cdecl Scena03_BobParty4(void) { Bob(at::kBob4, 3); }
// original 0x5449C0: Scena03_Bob2 0x660F98 by the timer & 1.
extern "C" void __cdecl Scena03_BobParty2(void) { Bob(at::kBob2, 1); }

// original 0x5449F0: slot 1, the object trigger (0x56D6D0 with the object):
// Scena03_ObjectHandlers 0x660F9C by the object's +0x86, unchecked, called
// with (object, the flag row). 0x56D6D0 reads no answer.
extern "C" void __cdecl Scena03_ObjectHook(unsigned char* object) {
    unsigned char* const bank = Bank();
    const unsigned index = object[0x86];
    reinterpret_cast<ObjectHandler>(static_cast<std::uintptr_t>(Entry(at::kObjects3, static_cast<int>(index))))(object, bank);
}

// original 0x544A10: object handler 0 - flag 0x14 on the row given; run 4.
extern "C" void __cdecl Scena03_Object0(unsigned char*, unsigned char* bank) {
    SH_CALL(ScriptFlags_Set40)();
    SH_CALL(Flags_Set)(bank, 0x14);
    SetRun(4);
    SetStep(0);
}

// original 0x544A40 / 0x544A60 / 0x544A80 / 0x544AA0: object handlers 1..4 -
// flag 0x10..0x13 on the row given, then (a tail jump) the four tested.
extern "C" unsigned char __cdecl Scena03_Object1(unsigned char*, unsigned char* bank) {
    SH_CALL(Flags_Set)(bank, 0x10);
    return SH_CALL(Scena03_ObjectsAllFour)();
}
extern "C" unsigned char __cdecl Scena03_Object2(unsigned char*, unsigned char* bank) {
    SH_CALL(Flags_Set)(bank, 0x11);
    return SH_CALL(Scena03_ObjectsAllFour)();
}
extern "C" unsigned char __cdecl Scena03_Object3(unsigned char*, unsigned char* bank) {
    SH_CALL(Flags_Set)(bank, 0x12);
    return SH_CALL(Scena03_ObjectsAllFour)();
}
extern "C" unsigned char __cdecl Scena03_Object4(unsigned char*, unsigned char* bank) {
    SH_CALL(Flags_Set)(bank, 0x13);
    return SH_CALL(Scena03_ObjectsAllFour)();
}

// original 0x544AC0 (the shared tail of handlers 1..4): with chapter 3's
// row's bits 0x10..0x13 all set (0x903FAA & 0xF), run 6's step 6 and 0x1E
// frames armed, 1; else 0.
extern "C" unsigned char __cdecl Scena03_ObjectsAllFour(void) {
    if ((Byte(at::kRow3Byte2) & 0xF) != 0xF) return 0;
    SH_CALL(ScriptFlags_Set40)();
    SetStep(6);
    SetTimer(0x1E);
    return 1;
}

// original 0x544AF0: object handler 5 - key item 1 (0x591900), message 0x2F,
// stream 2 played to its end with a loading frame and a frame's sleep each
// wait, then Sound_ResumeAll (a tail jump).
extern "C" void __cdecl Scena03_Object5(unsigned char*, unsigned char*) {
    KeyItemAdd(1);
    SH_CALL(Msg_OpenScript)(0x2F);
    Byte(at::kRequest) = 2;
    MusicStop();
    SH_CALL(Sound_LoadStream)(2);
    while (SH_CALL(Sound_StreamDone)() == 0) {
        SH_CALL(Field_LoadingFrame)();
        SH_CALL(Task_Sleep)(1);
    }
    SH_CALL(Sound_ResumeAll)();
}

// original 0x544B40: object handler 7 - run 6; 0.
extern "C" unsigned char __cdecl Scena03_Object7(unsigned char*, unsigned char*) {
    SH_CALL(ScriptFlags_Set40)();
    SetRun(6);
    SetStep(0);
    return 0;
}

// original 0x544B60: object handler 8 - run 7 with 0x1E frames; 0.
extern "C" unsigned char __cdecl Scena03_Object8(unsigned char*, unsigned char*) {
    SH_CALL(ScriptFlags_Set40)();
    SetTimer(0x1E);
    SetRun(7);
    SetStep(0);
    return 0;
}

// original 0x544B80: slot 2, the step hook (x, z) - by the area (a two-level
// switch: Game_AreaNumber - 0x25 through the byte table 0x544C34 of 0x3F and
// the jump table 0x544C18 of 7) one of six area tests, its answer; 0 for any
// other area.
extern "C" unsigned char __cdecl Scena03_StepHook(int x, int z) {
    switch (Area()) {
    case 0x25: return SH_CALL(Scena03_StepArea25)(x, z);
    case 0x29: return SH_CALL(Scena03_StepArea29)(x, z);
    case 0x32: return SH_CALL(Scena03_StepArea32)(x, z);
    case 0x33: return SH_CALL(Scena03_StepArea33)(x, z);
    case 0x45: return SH_CALL(Scena03_StepArea45)(x, z);
    case 0x63: return SH_CALL(Scena03_StepArea63)(x, z);
    default: return 0;
    }
}

// original 0x544C80: area 0x33 - without flag 1: without flag 0, x's cell
// 2..4 at z 0x60000 sets run 2; with flag 0 (asked again), x's cell 3..4 at
// z 0x40000 run 1. Always 0 (the step goes on).
extern "C" unsigned char __cdecl Scena03_StepArea33(int x, int z) {
    if (Flag(1)) return 0;
    if (!Flag(0)) {
        if (Near(Hi(x), 2, 3) && z == 0x60000) SetRun(2);
        return 0;
    }
    if (!Flag(0)) return 0;
    if (Near(Hi(x), 3, 2) && z == 0x40000) SetRun(1);
    return 0;
}

// original 0x544D00: area 0x29 - without flag 7, z's cell 7..8 at x
// 0x5C0000: flag 7, run 3 step 0xA; 1.
extern "C" unsigned char __cdecl Scena03_StepArea29(int x, int z) {
    if (Flag(7)) return 0;
    if (!Near(Hi(z), 7, 2) || x != 0x5C0000) return 0;
    SetFlag(7);
    SetRun(3);
    SetStep(0xA);
    return 1;
}

// original 0x544D50: area 0x25 - without flag 0x15, z 0x418000 and x's cell
// 0x20..0x23: run 4 step 5, message 0x33 (its answer, 1).
extern "C" unsigned char __cdecl Scena03_StepArea25(int x, int z) {
    if (Flag(0x15)) return 0;
    if (z != 0x418000 || !Near(Hi(x), 0x20, 4)) return 0;
    SetRun(4);
    SetStep(5);
    return SH_CALL(Scena04_Message)(0x33);
}

// original 0x544DA0: area 0x45 - without flag 0x18, three doors (x 0x3A8000
// at z's cell 0x3C..0x3D; z 0x28000 or 0x4C8000 at x's cell 0x1F..0x20):
// run 5 step 0, message 0x12.
extern "C" unsigned char __cdecl Scena03_StepArea45(int x, int z) {
    if (Flag(0x18)) return 0;
    const bool door = (x == 0x3A8000 && Near(Hi(z), 0x3C, 2)) ||
                      ((z == 0x28000 || z == 0x4C8000) && Near(Hi(x), 0x1F, 2));
    if (!door) return 0;
    SetRun(5);
    SetStep(0);
    return SH_CALL(Scena04_Message)(0x12);
}

// original 0x544E30: area 0x32 - with flag 0x1A, x's cell 0x15 and z's cell
// 0x34..0x35: run 5 step 0x10, message 0x1F.
extern "C" unsigned char __cdecl Scena03_StepArea32(int x, int z) {
    if (!Flag(0x1A)) return 0;
    if (Hi(x) != 0x15 || !Near(Hi(z), 0x34, 2)) return 0;
    SetRun(5);
    SetStep(0x10);
    return SH_CALL(Scena04_Message)(0x1F);
}

// original 0x544E80: area 0x63 - without flag 0x1B: with Cond_ByteFD 2, x
// 0xA8000 and z's cell 0x24..0x25, counter 1 = 1, run 6 step 5, message 0xC.
// With it: with Cond_ByteFD 3, z 0xB8000 and x's cell 5..6, run 6 at step
// 0xF (flag 0x1C clear: 1) or 0xD with message 0x11.
extern "C" unsigned char __cdecl Scena03_StepArea63(int x, int z) {
    const bool done = Flag(0x1B);
    const unsigned char fd = Byte(at::kByteFD);
    if (!done) {
        if (fd != 2 || x != 0xA8000 || !Near(Hi(z), 0x24, 2)) return 0;
        C1() = 1;
        SetRun(6);
        SetStep(5);
        return SH_CALL(Scena04_Message)(0xC);
    }
    if (fd != 3 || z != 0xB8000 || !Near(Hi(x), 5, 2)) return 0;
    if (!Flag(0x1C)) {
        SH_CALL(ScriptFlags_Set40)();
        SetStep(0xF);
        SetRun(6);
        return 1;
    }
    SetStep(0xD);
    SetRun(6);
    return SH_CALL(Scena04_Message)(0x11);
}

// original 0x544F40: slot 3, the arrive hook (x, z) - area 0x47's test, else 0.
extern "C" unsigned char __cdecl Scena03_ArriveHook(int x, int z) {
    if (Area() != 0x47) return 0;
    return SH_CALL(Scena03_ArriveArea47)(x, z);
}

// original 0x544F60: area 0x47 - without flag 0x1E, z 0x490000 and x's cell
// 0xE..0x11: run 8 step 2, 0x1E frames; 1.
extern "C" unsigned char __cdecl Scena03_ArriveArea47(int x, int z) {
    if (Flag(0x1E)) return 0;
    if (z != 0x490000 || !Near(Hi(x), 0xE, 4)) return 0;
    SH_CALL(ScriptFlags_Set40)();
    SetRun(8);
    SetStep(2);
    SetTimer(0x1E);
    return 1;
}

// original 0x544FB0: slot 4, the cell hook (x, z) - 0x56D800 over the one
// record Scena03_Cells 0x660FC0; none (a negative answer): 0xFF; else the
// handler Scena03_CellHandlers 0x660FC8 [answer] (read in place, unchecked)
// and 1.
extern "C" unsigned char __cdecl Scena03_CellHook(int x, int z) {
    const unsigned char r = CellFind(at::kCells3, 1, x, z);
    if (static_cast<signed char>(r) < 0) return 0xFF;
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(Entry(at::kCellHandlers3, static_cast<signed char>(r))))();
    return 1;
}

// original 0x544FE0: the cell's handler - message 0x19, run 7 with 0x1E frames.
extern "C" void __cdecl Scena03_Cell0(void) {
    SH_CALL(Msg_OpenScript)(0x19);
    Byte(at::kRequest) = 2;
    SH_CALL(ScriptFlags_Set40)();
    SetTimer(0x1E);
    SetRun(7);
    SetStep(0);
}

// ===========================================================================
// Chapter 4
// ===========================================================================

// original 0x545010: slot 0 of Scena04_Hooks - Scena04_States 0x660FE4 on the
// s8 0x8034E2, unchecked.
extern "C" void __cdecl Scena04_Frame(void) {
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(Entry(at::kStates4, static_cast<signed char>(Byte(at::kState)))))();
}

// original 0x545020: state 0 - state 1, chapter 4's row 0x903FB0 cleared.
extern "C" void __cdecl Scena04_Start(void) {
    Byte(at::kState) = 1;
    SetLong(At(at::kRow4), 0);
}

// original 0x545040: state 1, once per area entered (areas 0x28, 0x2A, 0x30,
// 0x36, 0x41); then state 2.
extern "C" void __cdecl Scena04_EnterArea(void) {
    if (Area() == 0x28) {
        if (Byte(at::kByteFD) == 7 && !Flag(3)) {
            SetFlag(3);
            ClearCounters();
            SH_CALL(Music_FadeOutStop)(8);
            SH_CALL(Music_Play)(0x3F, 8);
            SH_CALL(ScriptFlags_Set40)();
            SetRun(1);
            SetStep(7);
            SH_CALL(Scenario_CallA)(1);
            SH_CALL(Party_DropIn)(1);
        }
        if (Flag(4) && !Flag(5)) {
            const unsigned char c = C1();
            if (c == 0) {
                PassFlags(0x1F);
            } else {
                if (c == 1) SH_CALL(Kind2_Place)(1);
                SH_CALL(ObjTrio_SetBit40)();
            }
        }
    }
    if (Area() == 0x2A) {
        if (!Flag(1)) {
            SetFlag(1);
            ClearCounters();
            SH_CALL(Scenario_CallA)(0);
            SH_CALL(Party_DropIn)(0);
        }
        if (Flag(3) && !Flag(4)) {
            SH_CALL(ObjTrio_SetBit40)();
            PassFlags(0x1F);
        }
    }
    if (Area() == 0x30) {
        if (Byte(at::kByteFD) != 3) {
            Byte(at::kState) = 2;
            return;
        }
        unsigned char* const bank = Bank();
        ClearCounters();
        if (SH_CALL(Flags_Test)(bank, 6) == 0) {
            SetFlag(6);
            Byte(at::kPendingKind) = 5;
            SH_CALL(Party_DropIn)(3);
        }
        if (!Flag(7)) {
            SH_CALL(AreaMap_SetByte)(0x83, 6, 0x51);
            SH_CALL(AreaMap_SetByte)(0x84, 6, 0x51);
        }
    }
    const unsigned area = Area();
    if (area == 0x36) {
        ClearCounters();
        Byte(at::kState) = 2;
        return;
    }
    if (area == 0x41 && Flag(7) && !Flag(0xC)) {
        ClearCounters();
        SH_CALL(Kind2_Place)(0);
        for (unsigned i = 0; i < 3; ++i) {
            unsigned char& b = Byte(at::kMembers + i * at::kMemberStride + 0x24);
            b = static_cast<unsigned char>(b & 0xDF);
        }
        if (unsigned char* const e = TakeEffect(0x13)) {
            const std::int32_t y = S16(at::kAngleY), z = S16(at::kPitch);
            SetLong(e + 0x64, -0xC6);
            SetLong(e + 0x68, y);
            SetLong(e + 0x6C, z);
            e[9] = 0x40;
        }
        TakeEffect(0x2E);
    }
    Byte(at::kState) = 2;
}

// original 0x545330: state 2, the run - Scena04_Runs 0x660FF0 on
// MoveScript_Var7 (s8), unchecked.
extern "C" void __cdecl Scena04_Run(void) {
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(Entry(at::kRuns4, static_cast<signed char>(Byte(at::kRun)))))();
}

// original 0x545340: run 1, steps 0..0x1E (the jump table 0x545948 of 31;
// 0x5453D0, 0x5455A0 and 0x545910, listed as hidden starts, are three of its
// cases): a quake (the map elevation shaken on Scena04_Quake), the camera
// pulled back, messages, streams, the area 0x28 changes on the counters, and
// at the end the mode 7 switch waited out (Game_Mode 2).
extern "C" void __cdecl Scena04_Scene1(void) {
    switch (Byte(at::kStep)) {
    case 0:
        if (Requested()) return;
        ScriptFlagsAnd(0xFEFF);
        SetRun(0);
        SetStep(0);
        return;
    case 1:
        if (Timer() != 0) {
            DecTimer();
            return;
        }
        SH_CALL(Party_DropIn)(0);
        SetStep(2);
        return;
    case 2:
        if (C0() != 1) return;
        SH_CALL(MoveCmd_TestFB)(0x57, 0x12);
        SH_CALL(Sound_PlayEffect)(0x103);
        SetStep(3);
        return;
    case 3:
        if (C0() != 2) return;
        TakeKeptEffect(0x29);
        SetStep(4);
        return;
    case 4: {
        const std::int32_t i = static_cast<signed char>(Byte(at::kEffectByte));
        if (At(at::kEffects + static_cast<std::uint32_t>(i << 7))[0] & 1) return;
        SetTimer(0x1E);
        SetStep(5);
        return;
    }
    case 5: {
        if (Timer() != 0) {
            DecTimer();
            return;
        }
        SH_CALL(Sound_PlayEffect)(0x200);
        const unsigned char c = C0();
        SetTimer(0x3C);
        C0() = static_cast<unsigned char>(c + 1);
        SetStep(6);
        return;
    }
    case 6: {
        if (Timer() != 0) {
            const unsigned t = Byte(at::kTimer);
            Byte(at::kRedraw) = 2;
            const std::int32_t d = static_cast<signed char>(Byte(at::kQuake4 + (t & 3)));
            const std::int32_t elevation = Long(At(at::kElevation));
            DecTimer();
            SetLong(At(at::kElevation), elevation + (d << 6));
            return;
        }
        SH_CALL(ScriptFlags_Clear40)();
        const unsigned char c = C0();
        SetRun(0);
        SetStep(0);
        C0() = static_cast<unsigned char>(c + 1);
        return;
    }
    case 7:
        if (C0() != 0x14) return;
        SH_CALL(Music_FadeOut)(0x40);
        SH_CALL(Sound_LoadStream)(4);
        SetStep(8);
        return;
    case 8: {
        const unsigned t = Timer();
        if (t != 0x80) {
            Byte(at::kRedraw) = 2;
            SetWord(At(at::kCamDistance), t * 9);
            SetTimer(t + 1);
            return;
        }
        SH_CALL(Music_FadeOutStop)(0xA);
        SetStep(9);
        SetTimer(Timer() >> 1);
        return;
    }
    case 9:
        if (Timer() != 0) {
            DecTimer();
            return;
        }
        SH_CALL(Msg_OpenScript)(0xD);
        Byte(at::kRequest) = 2;
        SetStep(0xA);
        return;
    case 0xA:
        if (!Requested()) SetStep(0xB);
        return;
    case 0xB:
        if (!WaitDone()) return;
        PassFlags(0);
        SetTimer(0x3C);
        SetStep(0xC);
        return;
    case 0xC:
        if (Timer() != 0) {
            DecTimer();
            return;
        }
        SH_CALL(Msg_OpenScript)(0xE);
        Byte(at::kRequest) = 2;
        SetStep(0xD);
        return;
    case 0xD:
        if (Requested()) return;
        C0() = static_cast<unsigned char>(C0() + 1);
        SH_CALL(Music_Play)(0x3F, 8);
        PassFlags(0x1F);
        SetWord(At(at::kCamDistance), 0);
        Byte(at::kRedraw) = 2;
        SH_CALL(Transition_Start)(1);
        SetStep(0xE);
        return;
    case 0xE:
        if (C0() != 0x23) return;
        SH_CALL(Transition_Start)(0);
        SetStep(0xF);
        return;
    case 0xF:
        if (!WaitDone()) return;
        PartyRestore();
        SH_CALL(Music_FadeOut)(0x10);
        SH_CALL(Sound_LoadStream)(0);
        PassFlags(0);
        SetLong(At(at::kKind2X), 0xAC0000);
        SetLong(At(at::kKind2Z), 0x210000);
        SH_CALL(Field_ViewReset)();
        SetStep(0x10);
        return;
    case 0x10: {
        if (SH_CALL(Sound_StreamDone)() == 0) return;
        SH_CALL(Music_FadeOutStop)(0xA);
        SH_CALL(Music_Play)(0x30, 8);
        const unsigned char c = C0();
        PassFlags(0x1F);
        C0() = static_cast<unsigned char>(c + 1);
        SH_CALL(Transition_Start)(1);
        SetStep(0x11);
        return;
    }
    case 0x11:
        if (C0() != 0x2B) return;
        PassFlags(0);
        SetTimer(0xA);
        SetStep(0x12);
        return;
    case 0x12:
        if (Timer() != 0) {
            DecTimer();
            return;
        }
        C0() = 0;
        ChangeArea(0x2A, 0x1E0000, 0x240000, 3);
        Byte(at::kAreaTrack) = 0x54;
        Byte(at::kAreaTransition) = 0xFF;
        SetStep(0x13);
        return;
    case 0x13:
        if (C0() != 1) return;
        ScriptFlagsOr80();
        PassFlags(0);
        SetTimer(0xA);
        SetStep(0x14);
        return;
    case 0x14: {
        if (Timer() != 0) {
            DecTimer();
            return;
        }
        unsigned char* const bank = Bank();
        C1() = 0;
        C0() = 0;
        SH_CALL(Flags_Set)(bank, 4);
        ChangeArea(0x28, 0xAC0000, 0x210000, 0x82);
        Byte(at::kAreaTransition) = 0xFF;
        SetStep(0x15);
        return;
    }
    case 0x15:
        if (C0() != 5) return;
        C1() = 1;
        C0() = 0;
        ChangeArea(0x28, 0x6C0000, 0x70000, 0);
        SetStep(0x16);
        return;
    case 0x16:
        if (C0() != 2) return;
        C1() = 2;
        C0() = 0;
        ChangeArea(0x28, 0x420000, 0x60000, 0);
        SetStep(0x17);
        return;
    case 0x17:
        if (C0() != 1) return;
        C1() = 0;
        C0() = 0;
        ChangeArea(0x28, 0xAC0000, 0x210000, 0x83);
        SetStep(0x18);
        return;
    case 0x18:
        if (C0() != 9) return;
        SH_CALL(Music_FadeOut)(0x10);
        SH_CALL(Sound_LoadStream)(3);
        SetStep(0x19);
        return;
    case 0x19: {
        if (SH_CALL(Sound_StreamDone)() == 0) return;
        const unsigned char c = C0();
        SetStep(0x1B);
        C0() = static_cast<unsigned char>(c + 1);
        return;
    }
    case 0x1A: {
        if (SH_CALL(File_LoadDone)() == 0) return;
        const unsigned char c = C0();
        SetStep(0x1B);
        C0() = static_cast<unsigned char>(c + 1);
        return;
    }
    case 0x1C:
        if (Requested()) return;
        Byte(at::kLoadNext) = 0xFE;
        Byte(at::kLoadByte) = 0;
        Byte(at::kMemberCount3) = 0;
        Byte(at::kMemberCount2) = 1;
        SetWord(At(at::kGameMode), 7);
        SetStep(0x1D);
        return;
    case 0x1D:
        if (Word(At(at::kGameMode)) != 2) return;
        [[fallthrough]];
    case 0x1E:
        MusicPlayTrack();
        C0() = 0;
        SH_CALL(ScriptFlags_Clear40)();
        SetRun(0);
        SetStep(0);
        return;
    default:   // 0x1B, and 0x1F and up
        return;
    }
}

namespace {

// Scena04_Scene2's common exit: with the step it last stored (or read) - 2
// below 0xB as a byte, the tremor (a tail jump to 0x546130).
void Scene2Exit(unsigned char al) {
    if (static_cast<unsigned char>(al - 2) < 0xB) SH_CALL(Scena04_Tremor)();
}

// The effect of kind 0x13 Scena04_Scene2 places with the camera's words.
void Effect13Cam(std::int32_t a, bool yaw, std::int32_t c, bool pitch, unsigned char b9) {
    unsigned char* const e = TakeEffect(0x13);
    if (!e) return;
    const std::int32_t first = yaw ? S16(at::kYaw) : a;
    const std::int32_t y = S16(at::kAngleY);
    const std::int32_t third = pitch ? S16(at::kPitch) : c;
    SetLong(e + 0x64, first);
    SetLong(e + 0x68, y);
    SetLong(e + 0x6C, third);
    e[9] = b9;
}

// The body: returns the byte in al at the common exit.
unsigned char Scene2Body() {
    unsigned char al = Byte(at::kStep);
    constexpr unsigned kBx = 0x1E;   // ebx at the top; the cases that clear it clear it for their own stores
    switch (al) {
    case 0:
        if (Timer() != 0) {
            DecTimer();
            return al;
        }
        C0() = 0;
        SH_CALL(Party_DropIn)(4);
        SetStep(1);
        return 1;
    case 1:
        if (C0() != 0xC) return al;
        SH_CALL(Music_FadeOutStop)(8);
        SH_CALL(Sound_PlayEffect)(0x204);
        SetStep(2);
        return 2;
    case 2:
        if (C0() != 0xF) return al;
        Byte(at::kByteFE) = 1;
        SetTimer(0x60);
        SetStep(3);
        return 3;
    case 3:
        if (Timer() != 0) {
            DecTimer();
            return al;
        }
        PassFlags(0);
        SetStep(4);
        return 4;
    case 4:
        SetLong(At(at::kKind2X), 0x4E0000);
        SetLong(At(at::kKind2Z), 0x280000);
        SH_CALL(Field_ViewReset)();
        SetTimer(4);
        SetStep(5);
        return 5;
    case 5: {
        if (Timer() != 0) {
            DecTimer();
            return al;
        }
        const unsigned char c = C0();
        PassFlags(0x1F);
        C0() = static_cast<unsigned char>(c + 1);
        SetStep(6);
        return 6;
    }
    case 6:
        if (C0() != 0x11) return al;
        PassFlags(0);
        SetStep(7);
        return 7;
    case 7:
        SetLong(At(at::kKind2X), 0x850000);
        SetLong(At(at::kKind2Z), 0xA0000);
        SH_CALL(Field_ViewReset)();
        SH_CALL(Music_LoadFile)(0x41);
        SetStep(kBx);
        return static_cast<unsigned char>(kBx);
    case 0x1E:
        if (SH_CALL(File_LoadDone)() == 0) return Byte(at::kStep);
        SetTimer(4);
        SetStep(8);
        return 8;
    case 8: {
        if (Timer() != 0) {
            DecTimer();
            return al;
        }
        PassFlags(0x1F);
        SetWord(At(at::kYaw), 0xFCA6);
        SetWord(At(at::kPitch), 0x74);
        SetWord(At(at::kCamDistance), 0x400);
        SH_CALL(MapView_SetElevation)(0x100);
        const unsigned char c = C0();
        C0() = static_cast<unsigned char>(c + 1);
        SH_CALL(Music_Play)(0x41, 8);
        SH_CALL(Party_DropIn)(5);
        Effect13Cam(0, true, 0x192, false, 0x3C);
        SetTimer(kBx);
        SetStep(9);
        return 9;
    }
    case 9: {
        DecTimer();
        if (Timer() != 0) {
            const unsigned d = (Word(At(at::kCamDistance)) - 0x22u) & 0xFFFF;
            SetWord(At(at::kCamDistance), d);
            if (d & 0x8000) SetWord(At(at::kCamDistance), 0);
            return al;
        }
        const unsigned char c = C0();
        SetWord(At(at::kCamDistance), 0);
        SetTimer(kBx);
        C0() = static_cast<unsigned char>(c + 1);
        SetStep(0xA);
        return 0xA;
    }
    case 0xA:
        DecTimer();
        if (Timer() != 0) return al;
        Effect13Cam(-0x2D6, false, 0x2B0, false, 0x3C);
        SetTimer(0xF);
        SetStep(0xB);
        return 0xB;
    case 0xB: {
        if (Timer() != 0) {
            DecTimer();
            return al;
        }
        const unsigned char c = C0();
        SetTimer(0xF);
        C0() = static_cast<unsigned char>(c + 1);
        SetStep(0xC);
        return 0xC;
    }
    case 0xC: {
        if (Timer() != 0) {
            DecTimer();
            return al;
        }
        const unsigned char c = C0();
        SetTimer(kBx);
        C0() = static_cast<unsigned char>(c + 1);
        SetStep(0xD);
        return 0xD;
    }
    case 0xD:
        if (Timer() != 0) {
            DecTimer();
            return al;
        }
        SH_CALL(MapView_SetElevation)(0x180);
        SetStep(0xE);
        return 0xE;
    case 0xE:
        if (C0() != 0x22) return al;
        if (!TakeEffect(0x2C)) return Byte(at::kStep);
        SetStep(0xF);
        return 0xF;
    case 0xF:
        if (C0() != 0x23) return al;
        Effect13Cam(-0x3B2, false, 0, true, static_cast<unsigned char>(kBx));
        SetTimer(0xA0);
        SetStep(0x10);
        return 0x10;
    case 0x10: {
        if (Timer() != 0) {
            const unsigned d = (Word(At(at::kCamDistance)) + 9u) & 0xFFFF;
            SetWord(At(at::kCamDistance), d);
            if (static_cast<short>(d) > 0x400) SetWord(At(at::kCamDistance), 0x400);
            DecTimer();
            return al;
        }
        ScriptFlagsOr80();
        ChangeArea(0x41, 0x2E0000, 0x210000, 1);
        unsigned char* const bank = Bank();
        Byte(at::kPendingKind) = 0xFF;
        SH_CALL(Flags_Set)(bank, 7);
        SetStep(0x11);
        return 0x11;
    }
    case 0x11:
        if (C0() != 2) return al;
        SH_CALL(Music_FadeOut)(0x10);
        SetTimer(0x1F);
        SetStep(0x12);
        return 0x12;
    case 0x12: {
        const unsigned t = (Timer() - 1u) & 0xFFFF;
        SetTimer(t);
        if (t != 0xC) return al;
        Effect13Cam(-0x58, false, 0, true, 0xA);
        SetStep(0x13);
        return 0x13;
    }
    case 0x13:
        DecTimer();
        if (Timer() != 0) return al;
        Effect13Cam(0, true, -0x262, false, 0x20);
        SetStep(0x14);
        return 0x14;
    case 0x14:
        if (C0() != 3) return al;
        SetTimer(kBx);
        SetStep(0x15);
        return 0x15;
    case 0x15: {
        const unsigned t = (Timer() - 1u) & 0xFFFF;
        SetTimer(t);
        if (t > 0x12) {
            SH_CALL(Scena04_Tremor)();
            al = Byte(at::kStep);
        }
        if (Timer() != 0) return al;
        ScriptFlagsAnd(0xFF7F);
        ChangeArea(0x36, 0xE0000, 0x100000, 0x80);
        Byte(at::kAreaTransition) = 0;
        SetStep(0x16);
        return 0x16;
    }
    case 0x16: {
        if (C0() != 0x11) return al;
        SH_CALL(ScriptFlags_Clear40)();
        unsigned char* const bank = Bank();
        C0() = 0;
        SH_CALL(Flags_Set)(bank, 0xC);
        StatusBit80();
        SetRun(0);
        SetStep(0);
        return 0;
    }
    default:   // 0x17..0x1D, and 0x1F and up
        return al;
    }
}

}  // namespace

// original 0x5459D0: run 2, steps 0..0x16 and 0x1E (the jump table 0x5460A8
// of 31; 0x545AA0 and 0x545F70, listed as hidden starts, are two of its
// cases): the camera and music of the chapter's second scene, effects, the
// change to area 0x41 and to area 0x36. Every case leaves through one exit
// that runs the tremor (0x546130, a tail jump) when the byte left in al - the
// step just stored, or the step read at the top where a case stores none, or
// read again where the original reloads it - is 2..12.
extern "C" void __cdecl Scena04_Scene2(void) {
    Scene2Exit(Scene2Body());
}

// original 0x546130: the tremor - MapView_Elevation moved by 4 * the s8 of
// Scena04_TremorSteps 0x661000 at (Frame_Counter >> 1) & 3; the map redrawn.
extern "C" void __cdecl Scena04_Tremor(void) {
    const unsigned i = (static_cast<std::uint32_t>(Long(At(at::kFrameCounter))) >> 1) & 3;
    const std::int32_t elevation = Long(At(at::kElevation));
    Byte(at::kRedraw) = 2;
    const std::int32_t d = static_cast<signed char>(Byte(at::kTremor4 + i));
    SetLong(At(at::kElevation), elevation + d * 4);
}

// original 0x546160: slot 1, the object trigger - Scena04_ObjectHandlers
// 0x661004 by the object's +0x86, called with (object, the flag row).
extern "C" void __cdecl Scena04_ObjectHook(unsigned char* object) {
    unsigned char* const bank = Bank();
    const unsigned index = object[0x86];
    reinterpret_cast<ObjectHandler>(static_cast<std::uintptr_t>(Entry(at::kObjects4, static_cast<int>(index))))(object, bank);
}

// original 0x546180: object handler 0 - flag 0 on the row given; 0.
extern "C" unsigned char __cdecl Scena04_Object0(unsigned char*, unsigned char* bank) {
    SH_CALL(Flags_Set)(bank, 0);
    return 0;
}

// original 0x5461A0: slot 2, the step hook - area 0x28's test, else 0.
extern "C" unsigned char __cdecl Scena04_StepHook(int x, int z) {
    if (Area() != 0x28) return 0;
    return SH_CALL(Scena04_StepArea28)(x, z);
}

namespace {
// The leader's +8 into the byte 0x903850; true unless it is 4, 5 or 6.
bool LeaderFree() {
    const unsigned char k = Byte(at::kLeaderKind);
    Byte(at::kSlot) = k;
    return k != 6 && k != 5 && k != 4;
}
}  // namespace

// original 0x5461C0: area 0x28 - without flag 2, x's cell 0x57..0x5A and z's
// 0x13..0x15 with the leader's +8 not 4..6: flag 2, sound 0x204, run 1 step
// 1 with 0x1E frames, 1. Without flag 3: the same x cells at z's 0xB..0x10
// (leader free), message 2; else z's cell 0x18..0x19 at x 0x4D8000, message
// 3. With it: without flag 7, x's cell 0x1F..0x21 at z 0x248000, message
// 0x26. Each message sets run 1 step 0.
extern "C" unsigned char __cdecl Scena04_StepArea28(int x, int z) {
    if (!Flag(2)) {
        if (!Near(Hi(x), 0x57, 4) || !Near(Hi(z), 0x13, 3)) return 0;
        if (!LeaderFree()) return 0;
        SH_CALL(ScriptFlags_Set40)();
        SetFlag(2);
        SH_CALL(Sound_PlayEffect)(0x204);
        SetTimer(0x1E);
        SetRun(1);
        SetStep(1);
        return 1;
    }
    if (!Flag(3)) {
        if (Near(Hi(x), 0x57, 4) && Near(Hi(z), 0xB, 6) && LeaderFree()) {
            SetRun(1);
            SetStep(0);
            return SH_CALL(Scena04_Message)(2);
        }
        if (!Near(Hi(z), 0x18, 2) || x != 0x4D8000) return 0;
        SetRun(1);
        SetStep(0);
        return SH_CALL(Scena04_Message)(3);
    }
    if (Flag(7)) return 0;
    if (!Near(Hi(x), 0x1F, 3) || z != 0x248000) return 0;
    SetRun(1);
    SetStep(0);
    return SH_CALL(Scena04_Message)(0x26);
}

// original 0x546320 (chapters 3's and 4's hooks): script message `id` (the
// byte), the script flags' high byte | 1, the request 2; 1.
extern "C" unsigned char __cdecl Scena04_Message(unsigned char id) {
    SH_CALL(Msg_OpenScript)(static_cast<unsigned short>(*reinterpret_cast<const volatile unsigned char*>(&id)));
    Byte(at::kScriptFlagsHi) = static_cast<unsigned char>(Byte(at::kScriptFlagsHi) | 1);
    Byte(at::kRequest) = 2;
    return 1;
}

// original 0x546340: slot 4, the cell hook - 0x56D800 over Scena04_Cells
// 0x661008; none: 0xFF; else Scena04_CellHandlers 0x661010 [answer], 1.
extern "C" unsigned char __cdecl Scena04_CellHook(int x, int z) {
    const unsigned char r = CellFind(at::kCells4, 1, x, z);
    if (static_cast<signed char>(r) < 0) return 0xFF;
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(Entry(at::kCellHandlers4, static_cast<signed char>(r))))();
    return 1;
}

// original 0x546370: the cell's handler - run 2 with 0xA frames.
extern "C" void __cdecl Scena04_Cell0(void) {
    SH_CALL(ScriptFlags_Set40)();
    SetTimer(0xA);
    SetRun(2);
    SetStep(0);
}

// ===========================================================================

void ScenaSc3_Inject() {
    if (bof3::WantsShadow("scena_sc3")) scena_sc3::SelfTest();
    BOF3_INJECT(Scena03_Frame);
    BOF3_INJECT(Scena03_Start);
    BOF3_INJECT(Scena03_EnterArea);
    BOF3_INJECT(Scena03_Run);
    BOF3_INJECT(Scena03_Scene1);
    BOF3_INJECT(Scena03_Scene2);
    BOF3_INJECT(Scena03_Scene3);
    BOF3_INJECT(Scena03_Scene4);
    BOF3_INJECT(Scena03_Scene5);
    BOF3_INJECT(Scena03_Scene6);
    BOF3_INJECT(Scena03_Scene7);
    BOF3_INJECT(Scena03_Scene8);
    BOF3_INJECT(Scena03_SpawnAtMember);
    BOF3_INJECT(Scena03_BobParty4);
    BOF3_INJECT(Scena03_BobParty2);
    BOF3_INJECT(Scena03_ObjectHook);
    BOF3_INJECT(Scena03_Object0);
    BOF3_INJECT(Scena03_Object1);
    BOF3_INJECT(Scena03_Object2);
    BOF3_INJECT(Scena03_Object3);
    BOF3_INJECT(Scena03_Object4);
    BOF3_INJECT(Scena03_ObjectsAllFour);
    BOF3_INJECT(Scena03_Object5);
    BOF3_INJECT(Scena03_Object7);
    BOF3_INJECT(Scena03_Object8);
    BOF3_INJECT(Scena03_StepHook);
    BOF3_INJECT(Scena03_StepArea33);
    BOF3_INJECT(Scena03_StepArea29);
    BOF3_INJECT(Scena03_StepArea25);
    BOF3_INJECT(Scena03_StepArea45);
    BOF3_INJECT(Scena03_StepArea32);
    BOF3_INJECT(Scena03_StepArea63);
    BOF3_INJECT(Scena03_ArriveHook);
    BOF3_INJECT(Scena03_ArriveArea47);
    BOF3_INJECT(Scena03_CellHook);
    BOF3_INJECT(Scena03_Cell0);
    BOF3_INJECT(Scena04_Frame);
    BOF3_INJECT(Scena04_Start);
    BOF3_INJECT(Scena04_EnterArea);
    BOF3_INJECT(Scena04_Run);
    BOF3_INJECT(Scena04_Scene1);
    BOF3_INJECT(Scena04_Scene2);
    BOF3_INJECT(Scena04_Tremor);
    BOF3_INJECT(Scena04_ObjectHook);
    BOF3_INJECT(Scena04_Object0);
    BOF3_INJECT(Scena04_StepHook);
    BOF3_INJECT(Scena04_StepArea28);
    BOF3_INJECT(Scena04_Message);
    BOF3_INJECT(Scena04_CellHook);
    BOF3_INJECT(Scena04_Cell0);
}

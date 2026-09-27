// Scenario chapters 7 and 8 (the PSX's SCENA07 / SCENA08 overlays, compiled
// into the exe at 0x54F080..0x553B21): chapter 7's vtable Scena07_Hooks
// 0x6611B8 and chapter 8's Scena08_Hooks 0x661368 and everything they reach
// inside the band. docs/scena_sc7.md.
//
//   - Each chapter's slot 0 is a frame that jumps through its state table on
//     the s8 0x8034E2 (0 the start, 1 the area entry, 2 the run); the run
//     jumps through the run table on MoveScript_Var7 to a scene, a switch on
//     the step byte 0x8034E5 whose every case waits on one thing (a script
//     counter, the timer 0x8034E6, the message request, the wait word, a
//     stream or a file), does one thing and sets the next step -
//     Scena16_*'s shape.
//   - Slot 1 (the object trigger) dispatches on the object's +0x86 through a
//     table of handlers given (object, flag row); slots 2 and 3 (the step and
//     arrive hooks) test (x, z) by area and answer in al whether a scene
//     starts; chapter 7's slot 4 (the cell hook) asks 0x56D800 for the
//     chapter's cell record and runs its handler. Chapter 8 has no slot 4.
//
// Every call goes through the scenario harness (SH_CALL / SH_AT), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies.
// The .data tables are read in place and called through, as the originals do,
// unchecked: the fuzz swaps them for recorders. No divergence: each function
// is a faithful replacement; every read the original makes after a call is
// made after the same call here, every store before a call before it.
#include "game/scena_sc7.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/scena_sc7_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = scena_sc7::at;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

using Handler = void (__cdecl*)();
using ObjectHandler = void (__cdecl*)(unsigned char* object, unsigned char* bank);

// --- the cells --------------------------------------------------------------

unsigned char& Byte(std::uint32_t a) { return At(a)[0]; }
unsigned Area() { return Word(At(at::kArea)); }
unsigned Step() { return Byte(at::kStep); }
void SetState(unsigned v) { Byte(at::kState) = static_cast<unsigned char>(v); }
void SetStep(unsigned v) { Byte(at::kStep) = static_cast<unsigned char>(v); }
void SetRun(unsigned v) { Byte(at::kRun) = static_cast<unsigned char>(v); }
unsigned Timer() { return Word(At(at::kTimer)); }
void SetTimer(unsigned v) { SetWord(At(at::kTimer), v); }
// `dec word [timer]` then the flags of the result: the timer after.
unsigned DecTimer() {
    SetWord(At(at::kTimer), Word(At(at::kTimer)) - 1u);
    return Word(At(at::kTimer));
}
// `mov ax, [timer]; mov cx, ax; dec ax; test cx, cx; mov [timer], ax`: the
// timer before (every chapter 8 wait of this shape stores the decrement
// first, so a timer of 0 wraps to 0xFFFF on the way out).
unsigned PostDecTimer() {
    const unsigned t = Word(At(at::kTimer));
    SetWord(At(at::kTimer), t - 1u);
    return t;
}
void IncTimer() { SetWord(At(at::kTimer), Word(At(at::kTimer)) + 1u); }
unsigned char& C0() { return Byte(at::kCounter0); }
void IncC0() { C0() = static_cast<unsigned char>(C0() + 1); }
void PassFlags(unsigned v) { Byte(at::kPassFlags) = static_cast<unsigned char>(v); }
bool WaitDone() { return Word(At(at::kWait)) == 0; }
bool Requested() { return Byte(at::kRequest) == 2; }
void Request(unsigned v) { Byte(at::kRequest) = static_cast<unsigned char>(v); }
void ScriptFlagsOr(unsigned v) { Byte(at::kScriptFlags) = static_cast<unsigned char>(Byte(at::kScriptFlags) | v); }
void ScriptFlagsAnd(unsigned mask) { SetWord(At(at::kScriptFlags), Word(At(at::kScriptFlags)) & mask); }
short S16(std::uint32_t a) { return static_cast<short>(Word(At(a))); }
void Kind2At(std::int32_t x, std::int32_t z) {
    SetLong(At(at::kKind2X), x);
    SetLong(At(at::kKind2Z), z);
}
unsigned char* SpriteCurrent() {
    return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(At(at::kSpriteCurrent)))));
}
unsigned char* ActiveMember() {
    return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(At(at::kActiveMember)))));
}
unsigned char* Sprite(std::uint32_t k) { return At(at::kSprites + k * at::kSpriteStride); }
unsigned char* Record(std::uint32_t k) { return At(at::kCharRecords + k * at::kRecordStride); }
// Chapter 8 run 11's character records: by MoveScript_EffectState [7] (a
// byte) and [4] (the dword at [4] read, its low byte).
unsigned char* RecordOfMember7() { return Record(Byte(at::kEffectState + 7)); }
unsigned char* RecordOfMember4() { return Record(static_cast<std::uint32_t>(Long(At(at::kEffectState + 4))) & 0xFF); }
// The sprite 0x903804 points at, read afresh.
unsigned char* FocusObject() {
    return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(At(at::kFocusObject)))));
}

// A table of code pointers in the exe's data, read as the original reads it:
// afresh, and indexed without a bound.
std::uint32_t Entry(std::uint32_t table, int index) {
    return static_cast<std::uint32_t>(Long(At(table + 4u * static_cast<std::uint32_t>(index))));
}

// The flag row, the dword 0x929ED0 read afresh at each call.
unsigned char* Bank() { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(At(at::kFlagBank))))); }
bool Flag(unsigned n) { return SH_CALL(Flags_Test)(Bank(), n) != 0; }
void SetFlag(unsigned n) { SH_CALL(Flags_Set)(Bank(), n); }

// The high word of a 16.16 coordinate, and "within n cells of lo" as the
// originals test it: a 16-bit subtraction compared unsigned.
unsigned Hi(int v) { return (static_cast<std::uint32_t>(v) >> 16) & 0xFFFF; }
bool Near(unsigned hi, unsigned lo, unsigned n) { return ((hi - lo) & 0xFFFF) < n; }

void ChangeArea(unsigned area, int x, int z, unsigned flags) { SH_CALL(Field_ChangeArea)(area, x, z, flags); }
void Set40() { SH_CALL(ScriptFlags_Set40)(); }
void Clear40() { SH_CALL(ScriptFlags_Clear40)(); }
void DropIn(unsigned k) { SH_CALL(Party_DropIn)(k); }
void Message(unsigned id) {
    SH_CALL(Msg_OpenScript)(static_cast<unsigned short>(id));
    Request(2);
}
void Sound(unsigned id) { SH_CALL(Sound_PlayEffect)(static_cast<unsigned short>(id)); }
void Transition(unsigned kind) { SH_CALL(Transition_Start)(static_cast<unsigned char>(kind)); }
void SetElevationAt(std::int32_t x, std::int32_t z) {
    const long e = SH_CALL(AreaMap_Elevation)(x, z);
    SH_CALL(MapView_SetElevation)(static_cast<int>(e));
}
// The file loaded (a load already started): polled with a frame's sleep.
void WaitFile() {
    if (SH_CALL(File_LoadDone)() != 0) return;
    do SH_CALL(Task_Sleep)(1);
    while (SH_CALL(File_LoadDone)() == 0);
}

// --- the callees nobody owns, by address -------------------------------------

void PlaceParty(std::int32_t x, std::int32_t z, unsigned k) { SH_AT(void (__cdecl*)(std::int32_t, std::int32_t, unsigned), scena_sc7::kPlaceParty)(x, z, k); }
void PartyRestore() { SH_AT(void (__cdecl*)(), scena_sc7::kPartyRestore)(); }
void StatusBit80() { SH_AT(void (__cdecl*)(), scena_sc7::kStatusBit80)(); }
unsigned char CellFind(std::uint32_t records, unsigned n, int x, int z) {
    return static_cast<unsigned char>(SH_AT(std::uint32_t (__cdecl*)(std::uint32_t, unsigned, int, int), scena_sc7::kCellFind)(records, n, x, z));
}
std::uint32_t SpriteFindFree() { return SH_AT(std::uint32_t (__cdecl*)(), scena_sc7::kSpriteFindFree)(); }
void MusicStop() { SH_AT(void (__cdecl*)(), scena_sc7::kMusicStop)(); }
void KeyItemAdd(unsigned id) { SH_AT(void (__cdecl*)(unsigned), scena_sc7::kKeyItemAdd)(id); }
void Call591BE0(unsigned a, unsigned b) { SH_AT(void (__cdecl*)(unsigned, unsigned), scena_sc7::kCall591BE0)(a, b); }
void Call498DE0(unsigned a) { SH_AT(void (__cdecl*)(unsigned), scena_sc7::kCall498DE0)(a); }

// --- the effects ---------------------------------------------------------------

unsigned char* EffectAt(unsigned slot) { return At(at::kEffects + ((slot & 0xFFu) << 7)); }
// Effect_FindFree into the byte `cell`: the slot (0xFF none).
unsigned char TakeInto(std::uint32_t cell) {
    const unsigned char slot = SH_CALL(Effect_FindFree)();
    Byte(cell) = slot;
    return slot;
}
// A record marked live as `kind`.
void Live(unsigned char* e, unsigned kind) {
    e[0] = 1;
    e[5] = static_cast<unsigned char>(kind);
}
// Kind 0x13 with its three words (+0x64, +0x68, +0x6C) and +9.
void Effect13(unsigned char* e, std::int32_t a, std::int32_t b, std::int32_t c, unsigned b9) {
    Live(e, 0x13);
    SetLong(e + 0x64, a);
    SetLong(e + 0x68, b);
    SetLong(e + 0x6C, c);
    e[9] = static_cast<unsigned char>(b9);
}
// The record 0x903850 names, read back as a dword's low byte, as the
// originals index it after the store.
unsigned char* Slot850() { return EffectAt(static_cast<std::uint32_t>(Long(At(at::kSlot))) & 0xFF); }

}  // namespace

// ===========================================================================
// Chapter 7
// ===========================================================================

// original 0x54F080: slot 0 of Scena07_Hooks, Field_ModeDispatch's call - a
// tail jump through Scena07_States 0x6611CC on the s8 0x8034E2, unchecked.
extern "C" void __cdecl Scena07_Frame(void) {
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(Entry(at::kStates7, static_cast<signed char>(Byte(at::kState)))))();
}

// original 0x54F090: state 0, the chapter's start - chapter 7's flag row
// 0x903FC8 cleared and state 1.
extern "C" void __cdecl Scena07_Start(void) {
    SetLong(At(at::kRow7), 0);
    SetState(1);
}

// original 0x54F0B0: state 1, once per area entered. By the area number (read
// afresh at each test), the flags and Cond_ByteFD: a party drop-in, map cells,
// the kind-2 sprite's place, the music, a scene armed (run and step); then
// state 2 on every way out.
extern "C" void __cdecl Scena07_EnterArea(void) {
    if (Area() == 0x35 && !Flag(7)) {
        SH_CALL(Scenario_CallA)(1);
        DropIn(2);
        SetWord(At(at::kKind2Sprite + 0x3E), 0xD80);
        SH_CALL(MapView_SetElevation)(0xD80);
    }
    if (Area() == 0x4A) {
        if (Byte(at::kByteFD) != 1) {
            SetState(2);
            return;
        }
        if (!Flag(4)) {
            SetFlag(4);
            Kind2At(0x5B8000, 0x98000);
            SH_CALL(Field_ViewReset)();
            DropIn(0);
            SH_CALL(Music_FadeOut)(0x20);
            Byte(at::kMusicTrack) = 1;
        }
    } else if (!Flag(4)) {
        SH_CALL(Flags_Clear)(Bank(), 5);
        SH_CALL(Flags_Clear)(Bank(), 6);
    }
    if (Area() == 0x55) {
        if (!Flag(8)) {
            Set40();
            C0() = 0;
            DropIn(0);
            SetRun(6);
            SetStep(0);
        }
        if (!Flag(9)) {
            SH_CALL(AreaMap_SetByte)(0x25, 0x3D, 0x51);
            SH_CALL(AreaMap_SetByte)(0x26, 0x3D, 0x51);
        } else {
            SH_CALL(LoadDatFile)(0);
            WaitFile();
            SH_CALL(AreaMap_SetByte)(0x25, 0x3D, 0x50);
            SH_CALL(AreaMap_SetByte)(0x26, 0x3D, 0x50);
        }
    }
    if (Area() == 0x69) SH_CALL(Flags_Set)(At(at::kStoryFlags), 0x4F);
    if (Area() == 0xAF && Byte(at::kByteFD) == 1) {
        if (!Flag(0)) {
            Set40();
            C0() = 0;
            SH_CALL(Scenario_CallA)(0);
            DropIn(0);
            PassFlags(0);
            SetStep(0);
            SetRun(1);
            SetState(2);
            return;
        }
        if (!Flag(2) && Flag(1)) {
            Set40();
            Kind2At(0x2D8000, 0x5D0000);
            SH_CALL(Field_ViewReset)();
            DropIn(2);
            ScriptFlagsOr(8);
            SetRun(3);
            SetStep(0);
        }
    }
    SetState(2);
}

// original 0x54F330: state 2, the run - a tail jump through Scena07_Runs
// 0x6611D8 on MoveScript_Var7 (s8), unchecked.
extern "C" void __cdecl Scena07_Run(void) {
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(Entry(at::kRuns7, static_cast<signed char>(Byte(at::kRun)))))();
}

// original 0x54F340: run 1, steps 0..3 (a jump table of 4 after it at
// 0x54F40C): message 1; on its close a fade in and the kind-2 sprite placed;
// counter 0 set 1; on 0x15 flag 0 and the run over.
extern "C" void __cdecl Scena07_Scene1(void) {
    switch (Step()) {
    case 0:
        if (!WaitDone()) return;
        Message(1);
        SetStep(1);
        return;
    case 1:
        if (Requested()) return;
        Transition(1);
        Kind2At(0x2D8000, 0x5D0000);
        SH_CALL(Field_ViewReset)();
        SetStep(2);
        PassFlags(0x1F);
        return;
    case 2:
        if (!WaitDone()) return;
        C0() = 1;
        SetStep(3);
        return;
    case 3:
        if (C0() != 0x15) return;
        Clear40();
        SetFlag(0);
        C0() = 0;
        SetRun(0);
        SetStep(0);
        return;
    default:
        return;
    }
}

// original 0x54F420: run 2, steps 0..0x1C (a jump table of 29 after it at
// 0x54F96C): message 0x11, a fade out and the change to area 0xAF, effects of
// kind 0x13 / 0x31 / 0x38 on counter 0 values, music 0x60 and 0x63, the
// camera set, the party placed and event battle 0x1D, then flag 1; step 0x1C
// ends the run.
extern "C" void __cdecl Scena07_Scene2(void) {
    switch (Step()) {
    case 0:
        Message(0x11);
        SetStep(1);
        return;
    case 2:
        if (Requested()) return;
        Clear40();
        SetRun(0);
        SetStep(0);
        return;
    case 3:
        if (Requested()) return;
        Transition(0);
        SH_CALL(Music_FadeOutStop)(8);
        SetStep(4);
        return;
    case 4:
        if (!WaitDone()) return;
        PassFlags(0);
        SetTimer(0x1E);
        SetStep(5);
        return;
    case 5:
        if (DecTimer() != 0) return;
        Byte(at::kStatusBits) = static_cast<unsigned char>(Byte(at::kStatusBits) | 1);
        SH_CALL(Scenario_CallA)(0);
        ChangeArea(0xAF, 0x240000, 0x1E0000, 0x81);
        Byte(at::kAreaTransition) = 0xFF;
        Byte(at::kPendingKind) = 0xFF;
        SetStep(6);
        return;
    case 6:
        if (!WaitDone()) return;
        PassFlags(0x1F);
        Transition(1);
        SetStep(7);
        return;
    case 7:
        if (!WaitDone()) return;
        SetTimer(0x1E);
        SetStep(8);
        return;
    case 8:
        if (DecTimer() != 0) return;
        C0() = 1;
        SetStep(9);
        return;
    case 9:
        if (C0() != 3) return;
        if (TakeInto(at::kSlot) == 0xFF) return;
        SetStep(0xA);
        Effect13(Slot850(), S16(at::kYaw), S16(at::kAngleY), 0x31E, 0xE0);
        return;
    case 0xA:
        if (C0() != 4) return;
        if (Requested()) return;
        SH_CALL(Music_Play)(0x60, 8);
        SetStep(0xB);
        return;
    case 0xB:
        if (C0() != 5) return;
        if (TakeInto(at::kSlot) == 0xFF) return;
        SetStep(0xC);
        Effect13(Slot850(), S16(at::kYaw), S16(at::kAngleY), 0x200, 0x70);
        return;
    case 0xC:
        if (C0() != 0xA) return;
        SH_CALL(Music_FadeOutStop)(8);
        SH_CALL(Music_Play)(0x63, 8);
        SetStep(0xD);
        return;
    case 0xD:
        if (C0() != 0xB) return;
        if (TakeInto(at::kSlot) == 0xFF) return;
        SetStep(0xE);
        Effect13(Slot850(), -0x35A, S16(at::kAngleY), 0x48, 0x30);
        SetTimer(0x30);
        return;
    case 0xE: {
        if (C0() != 0xC) return;
        if (TakeInto(at::kSlot) == 0xFF) return;
        SetStep(0xF);
        unsigned char* const e = Slot850();
        Live(e, 0x31);
        SetLong(e + 0x64, S16(at::kYaw));
        SetLong(e + 0x68, S16(at::kAngleY));
        SetLong(e + 0x6C, S16(at::kPitch));
        SetLong(e + 0xC, -0x380);
        e[9] = 0x40;
        return;
    }
    case 0xF:
        if (C0() != 0xD) return;
        Transition(0);
        SetStep(0x10);
        return;
    case 0x10:
        if (!WaitDone()) return;
        SH_CALL(Music_FadeOutStop)(8);
        PassFlags(0);
        SetStep(0x11);
        return;
    case 0x11:
        if (C0() != 0x12) return;
        Transition(1);
        SetWord(At(at::kAngleY), 0);
        SetWord(At(at::kCamDistance), 0);
        PassFlags(0x1F);
        SetWord(At(at::kYaw), 0xFD56);
        SetWord(At(at::kPitch), 0x200);
        Byte(at::kRedraw) = 2;
        SetStep(0x12);
        return;
    case 0x12:
        if (!WaitDone()) return;
        SetStep(0x13);
        IncC0();
        return;
    case 0x13:
        if (C0() != 0x22) return;
        if (TakeInto(at::kSlot) == 0xFF) return;
        SetStep(0x14);
        Live(Slot850(), 0x38);
        return;
    case 0x15:
        Transition(0);
        SetStep(0x16);
        return;
    case 0x16:
        if (!WaitDone()) return;
        IncC0();
        Transition(1);
        SetStep(0x19);
        return;
    case 0x19:
        if (C0() != 0x45) return;
        Sound(0x20C);
        PlaceParty(0x240000, 0x1C8000, 0x1D);
        SetStep(0x1A);
        return;
    case 0x1A:
        if (C0() != 0x46) return;
        SH_CALL(Field_StartEventBattle)(0x1D);
        SetFlag(1);
        SetStep(0x1B);
        return;
    case 0x1C:
        Sound(0x201);
        Clear40();
        ScriptFlagsAnd(0xFFF7);
        C0() = 0;
        SetRun(0);
        SetStep(0);
        return;
    default:   // 1, 0x14, 0x17, 0x18, 0x1B and past 0x1C
        return;
    }
}

// original 0x54F9E0: run 3, steps 0..6 (a jump table of 7 after it at
// 0x54FB24): counter 0 = 3, 0xD, 0x10 in turn, a fade out, the party
// refreshed and stream 0 played; on its end the change to area 0xAF with flag
// 2; a fade in; the run over.
extern "C" void __cdecl Scena07_Scene3(void) {
    switch (Step()) {
    case 0:
        if (C0() != 3) return;
        SetStep(1);
        return;
    case 1:
        if (C0() != 0xD) return;
        SetStep(2);
        return;
    case 2:
        if (C0() != 0x10) return;
        Transition(0);
        SetStep(3);
        return;
    case 3:
        if (!WaitDone()) return;
        IncC0();
        PartyRestore();
        MusicStop();
        SH_CALL(Sound_LoadStream)(0);
        PassFlags(0);
        SetStep(4);
        return;
    case 4:
        if (SH_CALL(Sound_StreamDone)() == 0) return;
        SH_CALL(Sound_ResumeAll)();
        Byte(at::kStatusBits) = static_cast<unsigned char>(Byte(at::kStatusBits) & 0xFE);
        SetFlag(2);
        ChangeArea(0xAF, 0x2D8000, 0x5E0000, 0x83);
        Byte(at::kPendingKind) = 0xFF;
        Byte(at::kAreaTransition) = 0xFF;
        SetStep(5);
        return;
    case 5:
        if (!WaitDone()) return;
        PassFlags(0x1F);
        Transition(1);
        SetStep(6);
        return;
    case 6:
        if (!WaitDone()) return;
        IncC0();
        Clear40();
        ScriptFlagsAnd(0xFFF7);
        SetRun(0);
        SetStep(0);
        return;
    default:
        return;
    }
}

// original 0x54FE90: run 4's timed effects - for each of the 12 records of
// Scena07_TimedEffects 0x6611FC whose s16 frame equals the timer (read afresh
// each record): an effect of kind 0x18 at the record's x, z (+0x36, +0x3A)
// and sound 0x202 + record % 3; stops at the first record no slot is free for.
extern "C" void __cdecl Scena07_TimedEffects(void) {
    for (unsigned i = 0; i < 12; ++i) {
        const std::uint32_t rec = at::kTimed7 + 6 * i;
        if (static_cast<int>(S16(rec + 4)) != static_cast<int>(Timer())) continue;
        const unsigned char slot = SH_CALL(Effect_FindFree)();
        if (slot == 0xFF) return;
        unsigned char* const e = EffectAt(slot);
        e[0] = 1;
        e[5] = 0x18;
        e[0xB] = 0x18;
        SetWord(e + 0x36, Word(At(rec)));
        SetWord(e + 0x3A, Word(At(rec + 2)));
        Sound(0x202 + i % 3);
    }
}

// original 0x54FB40: run 4, steps 0..0xC (a jump table of 13 after it at
// 0x54FE5C): an effect of kind 6 kept in member 0's +0xB; on its end the
// kind-2 sprite placed; the party placed; effects of kind 0x3B at sprite 0
// and kind 0x13; steps 8 and 9 run the timed effects while the timer counts
// up; music 0x5E; event battle 0x1E; step 0xC (set from outside) flag 3 and
// the run over.
extern "C" void __cdecl Scena07_Scene4(void) {
    switch (Step()) {
    case 0: {
        const unsigned char slot = SH_CALL(Effect_FindFree)();
        Byte(at::kLeaderSlot) = slot;
        if (slot == 0xFF) return;
        unsigned char* const e = EffectAt(slot);
        const unsigned arg = Byte(at::kEffectArg + Byte(at::kMembers + 0x89));
        e[0] = 1;
        e[5] = 6;
        e[6] = 1;
        SetLong(e + 0xC, 0);
        SetLong(e + 0x10, static_cast<std::int32_t>(arg));
        SetWord(e + 0x2E, Word(At(at::kMembers + 0x2E)));
        SetStep(1);
        SetWord(e + 0x30, Word(At(at::kMembers + 0x30)));
        return;
    }
    case 1:
        if (EffectAt(Byte(at::kLeaderSlot))[0] & 1) return;
        C0() = 1;
        SH_CALL(Kind2_Place)(0);
        SetStep(2);
        return;
    case 2:
        if (C0() != 2) return;
        PlaceParty(0x4A0000, 0x220000, 0x1E);
        DropIn(0);
        SetStep(3);
        return;
    case 3:
        if (C0() != 9) return;
        SH_CALL(Music_FadeOutStop)(8);
        SetStep(4);
        return;
    case 4: {
        if (C0() != 0xA) return;
        if (TakeInto(at::kSlot) == 0xFF) return;
        unsigned char* const e = Slot850();
        SetTimer(0x8C);
        SetStep(5);
        Live(e, 0x3B);
        SetLong(e + 0x34, Long(Sprite(0) + 0x34));
        SetLong(e + 0x38, Long(Sprite(0) + 0x38));
        SetLong(e + 0x3C, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(Sprite(0) + 0x3C)) + 0x1000000u));
        return;
    }
    case 5:
        if (Timer() != 0) {
            DecTimer();
            return;
        }
        if (TakeInto(at::kSlot) == 0xFF) return;
        Effect13(Slot850(), -0x33E, S16(at::kAngleY), S16(at::kPitch), 0xA0);
        IncC0();
        Sound(0x205);
        SetStep(6);
        return;
    case 6:
        if (C0() != 0xC) return;
        SetStep(7);
        return;
    case 7:
        if (C0() != 0xD) return;
        SetTimer(0);
        SetStep(8);
        return;
    case 8:
        if (C0() == 0xE && TakeInto(at::kSlot) != 0xFF) {
            SetStep(9);
            Effect13(Slot850(), -0x2AA, S16(at::kAngleY), S16(at::kPitch), 0x90);
        }
        SH_CALL(Scena07_TimedEffects)();
        IncTimer();
        return;
    case 9:
        if (C0() == 0xF) {
            SH_CALL(Music_Play)(0x5E, 8);
            SetTimer(0);
            SetStep(0xA);
            return;
        }
        SH_CALL(Scena07_TimedEffects)();
        IncTimer();
        return;
    case 0xA:
        if (C0() != 0x12) return;
        SH_CALL(Field_StartEventBattle)(0x1E);
        SetStep(0xB);
        return;
    case 0xC:
        SetFlag(3);
        Clear40();
        C0() = 0;
        SetRun(0);
        SetStep(0);
        return;
    default:   // 0xB and past 0xC
        return;
    }
}

// original 0x54FF40: run 5, steps 0..2 (a chain of decrements): the change to
// area 0x35 with track 0x4D; on counter 0 = 9 flag 7 and the change to area
// 0x58; the run over.
extern "C" void __cdecl Scena07_Scene5(void) {
    switch (Step()) {
    case 0:
        ChangeArea(0x35, 0x120000, 0x180000, 1);
        Byte(at::kAreaTrack) = 0x4D;
        SetStep(1);
        return;
    case 1:
        if (!WaitDone()) return;
        SetStep(2);
        return;
    case 2:
        if (C0() != 9) return;
        SetFlag(7);
        ChangeArea(0x58, 0x350000, 0x230000, 3);
        ScriptFlagsAnd(0xFFF7);
        Clear40();
        SetRun(0);
        SetStep(0);
        return;
    default:
        return;
    }
}

// original 0x550CE0: an effect of kind 0x49 for `member` (+1): Effect_FindFree,
// the record +0 = 1, +5 = 0x49, +1 = member when there is a slot. Answers the
// record's address either way - for no slot (0xFF) the address 0xFF records
// past the pool, 0x7E9160, which its callers then write through (latent,
// docs/scena_sc7.md section 7).
extern "C" unsigned char* __cdecl Scena07_TakeEffect49(unsigned member) {
    const unsigned char slot = SH_CALL(Effect_FindFree)();
    unsigned char* const e = EffectAt(slot);
    if (slot != 0xFF) {
        e[0] = 1;
        e[5] = 0x49;
        e[1] = static_cast<unsigned char>(member);
    }
    return e;
}

// original 0x550890: the camera shaken - MapView_Redraw 2 and Camera_ShiftY
// (u16) += 4 * Scena07_Shake 0x661244 [Frame_Counter & 3] (s8).
extern "C" void __cdecl Scena07_ShakeCamera(void) {
    Byte(at::kRedraw) = 2;
    const std::uint32_t k = static_cast<std::uint32_t>(Long(At(at::kFrameCounter))) & 3;
    const unsigned d = static_cast<unsigned>(static_cast<int>(static_cast<signed char>(Byte(at::kShake7 + k)))) << 2;
    SetWord(At(at::kShiftY), Word(At(at::kShiftY)) + d);
}

// original 0x5508C0: 14 event objects placed - for each record of
// Scena07_PlacedObjects 0x661248 (0x11 bytes): a free sprite from 0x57CD90
// (its index stored as a u16 in 0x903850; 0xFF skips the record), EventOp_0x
// on the record, Sprite_Current's +0x3E = 0xA00, and when the record's byte in
// Scena07_PlacedSeats 0x661338 is not 0: Field_ActiveMember +0x81 = the byte,
// Sprite_Current +3 = its +1, +1 = 5.
extern "C" void __cdecl Scena07_PlaceObjects(void) {
    for (unsigned i = 0; i < 14; ++i) {
        const unsigned slot = SpriteFindFree() & 0xFF;
        SetWord(At(at::kSlot), slot);
        if (slot == 0xFF) continue;
        SH_CALL(EventOp_0x)(At(at::kPlaced7 + 0x11 * i));
        SetWord(SpriteCurrent() + 0x3E, 0xA00);
        const unsigned char seat = Byte(at::kPlacedSeat7 + i);
        if (seat == 0) continue;
        ActiveMember()[0x81] = seat;
        unsigned char* const cur = SpriteCurrent();
        cur[3] = cur[1];
        SpriteCurrent()[1] = 5;
    }
}

// original 0x54FFE0: run 6, steps 0..0x27 (a jump table of 40 after it at
// 0x5507EC): scenario call B 0 and 1, messages 6, 0x11 and 0x12, effects of
// kind 0x49 moved on timers (Scena07_TakeEffect49), event battle 0x1F, the
// change to area 0x55, effects of kind 0x46 and 0x13, the 14 objects placed,
// the change to area 0x20. Its tail: past step 0x1F the camera shakes, past
// 0x26 twice - some cases return before the tail, some run it, some shake
// once unconditionally first.
extern "C" void __cdecl Scena07_Scene6(void) {
    enum Exit { kDone, kTail, kShake };
    Exit out = kTail;
    // The timer counting down on a case that waits on it: decrement, then the
    // tail.
    auto count_down = [] { DecTimer(); };
    switch (Step()) {
    case 0:
        if (C0() != 5) break;
        SH_CALL(Scenario_CallB)(0);
        SetStep(1);
        out = kDone;
        break;
    case 1:
        if (C0() != 7) break;
        SetFlag(8);
        Clear40();
        C0() = 0;
        SetRun(0);
        SetStep(0);
        out = kDone;
        break;
    case 5:
        if (!Requested() || Word(At(at::kMessage)) != 2) break;
        SetStep(6);
        out = kDone;
        break;
    case 6:
        if (Requested()) break;
        ScriptFlagsAnd(0xFEFF);
        SetRun(0);
        SetStep(0);
        out = kDone;
        break;
    case 0xA:
        Message(6);
        SetStep(0xB);
        out = kDone;
        break;
    case 0xB:
        if (Requested()) break;
        SH_CALL(Scenario_CallB)(1);
        DropIn(1);
        SetStep(0xC);
        out = kDone;
        break;
    case 0xC: {
        if (C0() != 3) break;
        unsigned char* const p = SH_CALL(Scena07_TakeEffect49)(0);
        SetLong(p + 0x34, 0x160000);
        SetLong(p + 0x38, 0x3E0000);
        SetTimer(0xC8);
        SetStep(0xD);
        out = kDone;
        break;
    }
    case 0xD:
        if (Timer() == 0x88) SH_CALL(Scena07_TakeEffect49)(1);
        if (Timer() != 0) {
            count_down();
            break;
        }
        IncC0();
        SetStep(0xE);
        out = kDone;
        break;
    case 0xE: {
        if (C0() != 7) break;
        unsigned char* const p = SH_CALL(Scena07_TakeEffect49)(0);
        SetLong(p + 0x34, 0x360000);
        SetLong(p + 0x38, 0x410000);
        SetTimer(0xA8);
        SetStep(0xF);
        out = kDone;
        break;
    }
    case 0xF: {
        if (Timer() != 0) {
            count_down();
            break;
        }
        unsigned char* const p = SH_CALL(Scena07_TakeEffect49)(2);
        SetLong(p + 0xC, 0x270000);
        const std::int32_t x = Long(p + 0xC);
        SetLong(p + 0x34, 0x390000);
        SetLong(p + 0x38, 0x410000);
        SetLong(p + 0x10, 0x410000);
        const long e = SH_CALL(AreaMap_Elevation)(x, 0x410000);
        SetLong(p + 0x3C, static_cast<std::int32_t>(static_cast<std::uint32_t>(static_cast<int>(static_cast<short>(e))) << 16));
        SetTimer(8);
        SetStep(0x10);
        out = kDone;
        break;
    }
    case 0x10:
        if (Timer() != 0) {
            count_down();
            break;
        }
        SetTimer(1);
        SetStep(0x11);
        out = kDone;
        break;
    case 0x11:
        if (Timer() != 0) {
            count_down();
            break;
        }
        IncC0();
        SetStep(0x12);
        out = kDone;
        break;
    case 0x12: {
        if (C0() != 0x13) break;
        const std::int32_t z = Long(At(at::kLeaderZ));
        const std::int32_t x = Long(At(at::kLeaderX));
        ScriptFlagsOr(0x80);
        PlaceParty(x, z, 0x1F);
        SH_CALL(Field_StartEventBattle)(0x1F);
        SetStep(0x13);
        out = kDone;
        break;
    }
    case 0x14:
        SetFlag(9);
        ChangeArea(0x55, 0x258000, 0x410000, 0x82);
        Byte(at::kPendingKind) = 5;
        Byte(at::kAreaTransition) = 4;
        SetStep(0x15);
        out = kDone;
        break;
    case 0x15:
        if (C0() != 0x14) break;
        if (TakeInto(at::kSlot) == 0xFF) break;
        SetStep(0x16);
        Live(Slot850(), 0x46);
        out = kDone;
        break;
    case 0x16: {
        if (C0() != 0x15) break;
        unsigned char* const p = SH_CALL(Scena07_TakeEffect49)(9);
        p[6] = 0;
        SetLong(p + 0x34, 0x170000);
        SetLong(p + 0x38, 0x3F0000);
        SetLong(p + 0x3C, 0x5C00000);
        SetLong(p + 0xC, 0x10000);
        SetLong(p + 0x10, 0);
        SetLong(p + 0x14, static_cast<std::int32_t>(0xFFE00000u));
        SetLong(p + 0x18, 0x2D0000);
        SetLong(p + 0x1C, 0x3F0000);
        SetLong(p + 0x20, 0x3000000);
        Sound(0x203);
        Sound(0x206);
        SetTimer(0x78);
        SetStep(0x17);
        out = kDone;
        break;
    }
    case 0x17: {
        if (Timer() != 0) {
            count_down();
            break;
        }
        unsigned char* const p = SH_CALL(Scena07_TakeEffect49)(9);
        SetLong(p + 0x38, 0x410000);
        SetLong(p + 0x1C, 0x410000);
        p[6] = 1;
        SetLong(p + 0x34, 0x2D0000);
        SetLong(p + 0x3C, 0x3000000);
        SetLong(p + 0xC, static_cast<std::int32_t>(0xFFFF0000u));
        SetLong(p + 0x10, 0);
        SetLong(p + 0x14, 0);
        SetLong(p + 0x18, 0x270000);
        SetLong(p + 0x20, 0x3000000);
        SetTimer(0xA);
        SetStep(0x18);
        out = kDone;
        break;
    }
    case 0x18:
        if (Timer() != 0) {
            count_down();
            break;
        }
        IncC0();
        SetTimer(0x5A);
        SetStep(0x19);
        out = kDone;
        break;
    case 0x19:
        if (Timer() != 0) {
            count_down();
            break;
        }
        SetTimer(0x10);
        SetStep(0x1A);
        out = kDone;
        break;
    case 0x1A:
        if (Timer() != 0) {
            count_down();
            break;
        }
        SetStep(0x1B);
        out = kDone;
        break;
    case 0x1B:
        if (C0() != 0x1A) break;
        PassFlags(0);
        MusicStop();
        Message(0x11);
        SetStep(0x1C);
        out = kDone;
        break;
    case 0x1C:
        if (Requested()) break;
        Kind2At(0x258000, 0x410000);
        SH_CALL(Field_ViewReset)();
        IncC0();
        PassFlags(0x1F);
        SetStep(0x1D);
        out = kDone;
        break;
    case 0x1D:
        if (C0() != 0x1C) break;
        PassFlags(0);
        Message(0x12);
        SetStep(0x1E);
        out = kDone;
        break;
    case 0x1E:
        if (Requested()) break;
        IncC0();
        PassFlags(0x1F);
        SetStep(0x1F);
        out = kDone;
        break;
    case 0x1F:
        if (C0() != 0x1E) break;
        Sound(0x202);
        SetStep(0x20);
        out = kShake;
        break;
    case 0x20: {
        if (C0() != 0x1F) break;
        unsigned char* const p = SH_CALL(Scena07_TakeEffect49)(3);
        SetLong(p + 0x34, 0x258000);
        SetLong(p + 0x38, 0x410000);
        unsigned char* const q = SH_CALL(Scena07_TakeEffect49)(4);
        SetLong(q + 0x34, 0x258000);
        SetLong(q + 0x38, 0x410000);
        SetTimer(0x70);
        SetStep(0x21);
        out = kShake;
        break;
    }
    case 0x21: {
        if (Timer() != 0) {
            count_down();
            break;
        }
        unsigned char* const p = SH_CALL(Scena07_TakeEffect49)(5);
        SetLong(p + 0x34, 0x258000);
        SetLong(p + 0x38, 0x410000);
        unsigned char* const q = SH_CALL(Scena07_TakeEffect49)(8);
        SetLong(q + 0x34, 0x258000);
        SetLong(q + 0x38, 0x410000);
        const unsigned char c = static_cast<unsigned char>(C0() + 1);
        SetTimer(0x3C);
        C0() = c;
        SetStep(0x22);
        SH_CALL(Music_FadeOutStop)(8);
        break;
    }
    case 0x22:
        if (Timer() != 0) {
            count_down();
            break;
        }
        SetStep(0x23);
        IncC0();
        SetTimer(0x96);
        out = kShake;
        break;
    case 0x23: {
        if (Timer() != 0) {
            count_down();
            break;
        }
        unsigned char* const p = SH_CALL(Scena07_TakeEffect49)(6);
        SetLong(p + 0x34, 0x258000);
        SetLong(p + 0x38, 0x410000);
        unsigned char* const q = SH_CALL(Scena07_TakeEffect49)(7);
        SetLong(q + 0x34, 0x258000);
        SetLong(q + 0x38, 0x410000);
        SetTimer(0x3C);
        SetStep(0x24);
        out = kShake;
        break;
    }
    case 0x24:
        if (C0() != 0x25) break;
        if (TakeInto(at::kSlot) == 0xFF) break;
        SetStep(0x25);
        Effect13(Slot850(), -0x370, S16(at::kAngleY), 0xB6, 0xA8);
        out = kShake;
        break;
    case 0x25:
        if (C0() != 0x26) break;
        if (TakeInto(at::kSlot) == 0xFF) break;
        SetStep(0x26);
        Effect13(Slot850(), -0x2AA, S16(at::kAngleY), 0x200, 0x1E);
        out = kShake;
        break;
    case 0x26:
        if (C0() != 0x27) break;
        SH_CALL(Scena07_PlaceObjects)();
        SetStep(0x27);
        out = kShake;
        break;
    case 0x27:
        if (C0() != 0x29) break;
        Clear40();
        C0() = 0;
        SetRun(0);
        SetStep(0);
        StatusBit80();
        ChangeArea(0x20, 0x900000, 0x550000, 3);
        Byte(at::kAreaTransition) = 4;
        Sound(0x20A);
        ScriptFlagsAnd(0xFF7F);
        break;
    default:   // 2..4, 7..9, 0x13 and past 0x27: the tail
        break;
    }
    if (out == kDone) return;
    if (out == kShake || Step() > 0x1F) SH_CALL(Scena07_ShakeCamera)();
    if (Step() > 0x26) SH_CALL(Scena07_ShakeCamera)();
}

// original 0x550930: run 7, steps 0..0x15 (a byte table of 0x16 at 0x550B08
// into a jump table of 7 at 0x550AEC): two item rewards - counter 0 = 0x1E /
// 0x23 armed; on 0x1F / 0x24 Inventory_Add of item [0x903EFE] + 1 (category
// 1) / [0x903F01] + 1 (category 2); a full inventory ends the run (counter 0
// = 0x20 / 0x25), else the item's name copied to Text_Records +0 / +0x20,
// message 0x94 / 0x95, flag 0x22 / 0x23, sound 0x106, step 0x15; step 0x14
// system message 1; step 0x15 on the message's close the run over.
extern "C" void __cdecl Scena07_Scene7(void) {
    auto finish = [] {
        Byte(at::kCounter1) = 0;
        Clear40();
        SetStep(0);
        SetRun(0);
    };
    auto reward = [&finish](std::uint32_t item_cell, unsigned category, unsigned full, std::uint32_t text, unsigned message,
                            unsigned flag) {
        const unsigned id = (Byte(item_cell) + 1u) & 0xFF;
        if (SH_CALL(Inventory_Add)(category, id, 1) == 0) {
            C0() = static_cast<unsigned char>(full);
            finish();
            return;
        }
        const unsigned id2 = (Byte(item_cell) + 1u) & 0xFF;
        const unsigned char* const name = SH_CALL(Item_NamePtr)(category, id2);
        for (unsigned i = 0; i < 16; i += 4) SetLong(At(text + i), Long(name + i));
        Message(message);
        SetFlag(flag);
        Sound(0x106);
        SetStep(0x15);
    };
    switch (Step()) {
    case 0:
        if (Requested()) return;
        C0() = 0x1E;
        SetStep(1);
        return;
    case 1:
        if (C0() != 0x1F) return;
        reward(at::kCharRecords + 7 * at::kRecordStride + 0x12, 1, 0x20, at::kTextRecords, 0x94, 0x22);
        return;
    case 0xA:
        if (Requested()) return;
        C0() = 0x23;
        SetStep(0xC);
        return;
    case 0xC:
        if (C0() != 0x24) return;
        reward(at::kCharRecords + 7 * at::kRecordStride + 0x15, 2, 0x25, at::kTextRecords + 0x20, 0x95, 0x23);
        return;
    case 0x14:
        SH_CALL(Msg_OpenSystem)(1);
        Request(2);
        SetStep(0x15);
        return;
    case 0x15:
        if (Requested()) return;
        C0() = 0;
        finish();
        return;
    default:
        return;
    }
}

// original 0x550B20: run 8 - at step 0, once the message is closed, the run
// over.
extern "C" void __cdecl Scena07_Scene8(void) {
    if (Step() != 0) return;
    if (Requested()) return;
    Clear40();
    SetRun(0);
    SetStep(0);
}

// original 0x550B50: slot 1, the object trigger (0x56D6D0 with the object):
// Scena07_ObjectHandlers 0x661348 by the object's +0x86, unchecked, called
// with (object, the flag row). 0x56D6D0 reads no answer.
extern "C" void __cdecl Scena07_ObjectHook(unsigned char* object) {
    unsigned char* const bank = Bank();
    const unsigned index = object[0x86];
    reinterpret_cast<ObjectHandler>(static_cast<std::uintptr_t>(Entry(at::kObjects7, static_cast<int>(index))))(object, bank);
}

namespace {
// Object handlers 0..2's end: the object set to state 4 with +0x84 = 2,
// +0x83 = the animation, +0x8A = 0.
void ObjectTo4(unsigned char* object, unsigned animation) {
    object[1] = 4;
    object[0x84] = 2;
    object[0x83] = static_cast<unsigned char>(animation);
    SetWord(object + 0x8A, 0);
}
// Object handlers 3 and 4: the counters cleared, run 7 at `step`, or at 0x14
// when chapter 6's flag `flag` is set.
void ObjectReward(unsigned flag, unsigned step) {
    Set40();
    C0() = 0;
    Byte(at::kCounter1) = 0;
    Byte(at::kCounter2) = 0;
    Byte(at::kCounter3) = 0;
    const bool had = SH_CALL(Flags_Test)(At(at::kRow6), flag) != 0;
    SetStep(step);
    if (had) SetStep(0x14);
    SetRun(7);
}
}  // namespace

// original 0x550B70: object handler 0 - flag 0xA (the row 0x929ED0 names, not
// the one given), a drop-in 3, the object to state 4 animation 5.
extern "C" void __cdecl Scena07_Object0(unsigned char* object, unsigned char*) {
    SetFlag(0xA);
    Set40();
    DropIn(3);
    ObjectTo4(object, 5);
}

// original 0x550BB0: object handler 1 - flag 0xB, key item 5, a drop-in 4, the
// object to state 4 animation 6.
extern "C" void __cdecl Scena07_Object1(unsigned char* object, unsigned char*) {
    SetFlag(0xB);
    KeyItemAdd(5);
    Set40();
    DropIn(4);
    ObjectTo4(object, 6);
}

// original 0x550C00: object handler 2 - flag 0xC, a drop-in 9, the object to
// state 4 animation 0x1D.
extern "C" void __cdecl Scena07_Object2(unsigned char* object, unsigned char*) {
    SetFlag(0xC);
    Set40();
    DropIn(9);
    ObjectTo4(object, 0x1D);
}

// original 0x550C40: object handler 3 - run 7 at step 0 (the first reward), or
// 0x14 when chapter 6's flag 0x22 is set.
extern "C" void __cdecl Scena07_Object3(unsigned char*, unsigned char*) { ObjectReward(0x22, 0); }

// original 0x550C90: object handler 4 - run 7 at step 0xA (the second
// reward), or 0x14 when chapter 6's flag 0x23 is set.
extern "C" void __cdecl Scena07_Object4(unsigned char*, unsigned char*) { ObjectReward(0x23, 0xA); }

// original 0x550F80: 1 in al when a party member (of Field_MemberCount, a
// byte) has +0x89 = 2, else 0. Chapter 7's step hook calls it, and chapter
// 10's code at 0x55BBB1.
extern "C" unsigned char __cdecl Scena07_PartyHas89State2(void) {
    const unsigned n = Byte(at::kMemberCount);
    for (unsigned i = 0; i < n; ++i)
        if (Byte(at::kMembers + i * at::kMemberStride + 0x89) == 2) return 1;
    return 0;
}

// original 0x550D20: slot 2, the step hook (x, z) - by area: 0x4A (flag 4 and
// not 7, x 0x348000, z cells 0x13..0x16) run 5; 0x52 (z cells 0x3C..0x40, x
// cells 0x22..0x25) counter 0 = 0x10 unless flag 4 with a member in state 2,
// or member 0's kind is 4..6; 0x55 (z 0x1C8000, x cells 0x25..0x26, not flag
// 9) message 2, run 6 step 5; 0x69 (member 0's kind 3 or more, z cell above
// 0x1D) message 1, run 8; 0xAF (flag 0 not 1, z cells 0x20..0x24, x cells
// 0x18..0x2D) run 2. 1 in al when a scene starts.
extern "C" unsigned char __cdecl Scena07_StepHook(int x, int z) {
    const unsigned xh = Hi(x), zh = Hi(z);
    if (Area() == 0x4A && Flag(4) && !Flag(7) && x == 0x348000 && Near(zh, 0x13, 4)) {
        Set40();
        SetRun(5);
        SetStep(0);
        return 1;
    }
    if (Area() == 0x52) {
        if (!Near(zh, 0x3C, 5)) return 0;
        if (!Near(xh, 0x22, 4)) return 0;
        if (!Flag(4) || SH_CALL(Scena07_PartyHas89State2)() == 0) {
            const unsigned char kind = Byte(at::kLeaderKind);
            Byte(at::kSlot) = kind;
            if (kind != 6 && kind != 5 && kind != 4) {
                C0() = 0x10;
                Set40();
                return 1;
            }
        }
    }
    if (Area() == 0x55) {
        if (z != 0x1C8000) return 0;
        if (!Near(xh, 0x25, 2)) return 0;
        if (!Flag(9)) {
            SH_CALL(Msg_OpenScript)(2);
            Byte(at::kScriptFlagsHi) = static_cast<unsigned char>(Byte(at::kScriptFlagsHi) | 1);
            Request(2);
            SetRun(6);
            SetStep(5);
            return 1;
        }
    }
    const unsigned area = Area();
    if (area == 0x69) {
        const unsigned char kind = Byte(at::kLeaderKind);
        Byte(at::kSlot) = kind;
        if (kind == 0 || kind == 1 || kind == 2) return 0;
        if (static_cast<short>(zh) <= 0x1D) return 0;
        Set40();
        Message(1);
        SetRun(8);
        SetStep(0);
        return 1;
    }
    if (area == 0xAF) {
        if (!Flag(0)) return 0;
        if (Flag(1)) return 0;
        if (!Near(zh, 0x20, 5)) return 0;
        if (!Near(xh, 0x18, 0x16)) return 0;
        Set40();
        SetRun(2);
        SetStep(0);
        return 1;
    }
    return 0;
}

// original 0x550F20: slot 3, the arrive hook (x, z) - area 0x67, not flag 3, z
// up to 0x200000 (signed), x cells 0x4A..0x4B: the music faded, run 4. 1 in
// al when it starts.
extern "C" unsigned char __cdecl Scena07_ArriveHook(int x, int z) {
    if (Area() != 0x67) return 0;
    if (Flag(3)) return 0;
    if (z > 0x200000) return 0;
    if (!Near(Hi(x), 0x4A, 2)) return 0;
    Set40();
    SH_CALL(Music_FadeOut)(0x20);
    SetRun(4);
    SetStep(0);
    return 1;
}

// original 0x550FD0: slot 4, the cell hook (x, z) - 0x56D800 over the one
// record Scena07_Cells 0x66135C; none (a negative answer): 0xFF in al (the
// answer's upper bytes kept); else the handler Scena07_CellHandlers 0x661364
// [answer] (read in place, unchecked) and 1.
extern "C" unsigned char __cdecl Scena07_CellHook(int x, int z) {
    const unsigned char r = CellFind(at::kCells7, 1, x, z);
    if (static_cast<signed char>(r) < 0) return 0xFF;
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(Entry(at::kCellHandlers7, static_cast<signed char>(r))))();
    return 1;
}

// original 0x551000: the cell's handler - run 6 at step 0xA.
extern "C" void __cdecl Scena07_Cell0(void) {
    Set40();
    SetRun(6);
    SetStep(0xA);
}

// ===========================================================================
// Chapter 8
// ===========================================================================

// original 0x551020: slot 0 of Scena08_Hooks, Field_ModeDispatch's call - a
// tail jump through Scena08_States 0x66137C on the s8 0x8034E2, unchecked.
extern "C" void __cdecl Scena08_Frame(void) {
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(Entry(at::kStates8, static_cast<signed char>(Byte(at::kState)))))();
}

// original 0x551060: bit 0 of +0xB set on the eight CharacterRecords
// (0x903A70, stride 0xA4), the party refreshed (0x533E50), then bit 0
// cleared on records 1..7 - record 0 alone keeps it.
extern "C" void __cdecl Scena08_PartyBitsToRecord0(void) {
    for (std::uint32_t i = 0; i < 8; ++i) Record(i)[0xB] = static_cast<unsigned char>(Record(i)[0xB] | 1);
    PartyRestore();
    for (std::uint32_t i = 1; i < 8; ++i) Record(i)[0xB] = static_cast<unsigned char>(Record(i)[0xB] & 0xFE);
}

// original 0x553730: every key-item byte of the 32 at 0x904554 that is 4
// becomes 0xE.
extern "C" void __cdecl Scena08_SwapKeyItem4(void) {
    for (std::uint32_t i = 0; i < 0x20; ++i)
        if (Byte(at::kKeyItems + i) == 4) Byte(at::kKeyItems + i) = 0xE;
}

// original 0x551030: state 0, the chapter's start - state 1, chapter 8's flag
// row 0x903FD0 and the four counters cleared, record 0 alone given bit 0, key
// item 4 swapped for 0xE, story flag 0x43.
extern "C" void __cdecl Scena08_Start(void) {
    SetState(1);
    SetLong(At(at::kRow8), 0);
    SetLong(At(at::kCounters), 0);
    SH_CALL(Scena08_PartyBitsToRecord0)();
    SH_CALL(Scena08_SwapKeyItem4)();
    SH_CALL(Flags_Set)(At(at::kStoryFlags), 0x43);
}

// original 0x5510A0: state 1, once per area entered. By the area number (read
// afresh at each test), the flags and Cond_ByteFD: drop-ins, scenario call A,
// the kind-2 sprite and the view, sprites 0 and 1 set up, an effect of kind
// 0x13, member 1's +0 bit 0x40, a kept area change (0x6BC720), map cells, the
// music loaded, a scene armed (run and step); then state 2 on every way out.
extern "C" void __cdecl Scena08_EnterArea(void) {
    if (Area() == 0) {
        if (Byte(at::kByteFD) != 0) {
            SetState(2);
            return;
        }
        if (!Flag(0xA)) {
            SetFlag(0xA);
            SH_CALL(Scenario_CallA)(3);
            DropIn(5);
        }
    }
    if (Area() == 1) {
        if (Byte(at::kByteFD) != 2) {
            SetState(2);
            return;
        }
        if (!Flag(6)) {
            Set40();
            DropIn(3);
            Kind2At(0x518000, 0x1E8000);
            SetElevationAt(0x518000, 0x1E8000);
            SH_CALL(Field_ViewReset)();
            Sprite(0)[1] = 4;
            Sprite(1)[1] = 4;
            Sprite(0)[0x83] = 0xD;
            Sprite(1)[0x83] = 0xE;
            SetRun(4);
            SetStep(0xA);
        }
    }
    if (Area() == 2 && Flag(6) && !Flag(7)) {
        const unsigned char slot = TakeInto(at::kSlot);
        SetWord(At(at::kYaw), 0xFDC4);
        if (slot != 0xFF) Effect13(Slot850(), -0x302, S16(at::kAngleY), S16(at::kPitch), 0xFF);
    }
    if (Area() == 3 && !Flag(0xE)) {
        SH_CALL(Scenario_CallA)(3);
        Byte(at::kMember1) = static_cast<unsigned char>(Byte(at::kMember1) | 0x40);
    }
    if (Area() == 0xC && !Flag(0xC) && Flag(0xB)) {
        Set40();
        const unsigned char kind = Byte(at::kLeaderKind);
        const std::int32_t x = Long(At(at::kLeaderX));
        const std::int32_t z = Long(At(at::kLeaderZ));
        PassFlags(0);
        SetRun(8);
        SetStep(0);
        Byte(at::kKeptFlags) = kind;
        SetLong(At(at::kKeptX), x);
        SetLong(At(at::kKeptZ), z);
    }
    if (Area() == 0xF && Flag(0xE) && !Flag(0xF)) {
        Set40();
        C0() = 0;
        SetRun(9);
        SetStep(0);
        SH_CALL(Scenario_CallA)(4);
        DropIn(0xA);
    }
    if (Area() == 0x12 && !Flag(0x17)) {
        for (unsigned i = 0; i < 0xB; ++i) SH_CALL(AreaMap_SetByte)(0x1A + i, 0x18, 0x10);
    }
    if (Area() == 0x15) {
        if (Byte(at::kByteFD) != 0) {
            SetState(2);
            return;
        }
        Byte(at::kCounter3) = 0;
        if (Flag(0xF)) {
            if (!Flag(0x11)) {
                SH_CALL(Music_LoadFile)(0x74);
                WaitFile();
                DropIn(0);
                Kind2At(0x298000, 0xD0000);
                SetElevationAt(0x298000, 0xD0000);
                SH_CALL(Field_ViewReset)();
                SetFlag(0x11);
            }
            if (Flag(0x13) && !Flag(0x14)) {
                Set40();
                DropIn(2);
                Kind2At(0xA8000, 0x140000);
                SetElevationAt(0xA8000, 0x140000);
                SH_CALL(Field_ViewReset)();
                // The original pushes (row, the row pointer's low byte, 0x14):
                // Flags_Set sets the flag the pointer's low byte names and
                // never reads the 0x14 (docs/scena_sc7.md section 7).
                unsigned char* const bank = Bank();
                SH_CALL(Flags_Set)(bank, static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(bank)) & 0xFF);
                SetRun(0xA);
                SetStep(0xA);
            }
        }
    }
    if (Area() != 0x20) {
        SetState(2);
        return;
    }
    if (Byte(at::kByteFD) == 5 && !Flag(0)) {
        Set40();
        PassFlags(0);
        DropIn(0);
        SetRun(1);
        SetStep(0);
        SetTimer(0x96);
    }
    if (Byte(at::kByteFD) == 1) {
        if (!Flag(0)) {
            DropIn(1);
            PassFlags(0x1F);
        } else if (!Flag(1)) {
            SH_CALL(Scenario_CallA)(1);
            DropIn(2);
            PassFlags(0);
        }
    }
    if (Byte(at::kByteFD) == 0 && !Flag(2)) {
        SH_CALL(Scenario_CallA)(2);
        DropIn(3);
        SetFlag(2);
        ScriptFlagsAnd(0xFFDF);
    }
    SetState(2);
}

// original 0x551580: state 2, the run - a tail jump through Scena08_Runs
// 0x661388 on MoveScript_Var7 (s8), unchecked.
extern "C" void __cdecl Scena08_Run(void) {
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(Entry(at::kRuns8, static_cast<signed char>(Byte(at::kRun)))))();
}

// original 0x551590: run 1, steps 0..0xC (a jump table of 13 after it at
// 0x5518A0): a timed music start (step 0 runs on into step 1's wait), counter
// 0 = 1, effects of kind 0x52 on counter 4 and 7, a transition, the music
// faded, two character records set (by MoveScript_EffectState [7] and [8])
// and the change to area 0x20; again on 0x16 with flag 0; sound 0x204; on
// 0x1D the change to area 0x20 with flag 1; the run over.
extern "C" void __cdecl Scena08_Scene1(void) {
    switch (Step()) {
    case 0:
        if (DecTimer() != 0) return;
        SH_CALL(Music_Play)(0x2B, 0x40);
        SetTimer(0x3C);
        SetStep(1);
        [[fallthrough]];
    case 1:
        if (DecTimer() != 0) return;
        C0() = 1;
        SetTimer(0);
        SetStep(2);
        PassFlags(0x1F);
        return;
    case 2: {
        if (C0() != 4) return;
        if (TakeInto(at::kSlot) != 0xFF) {
            unsigned char* const e = Slot850();
            SetLong(e + 0x38, 0x550000);
            const std::int32_t z = Long(e + 0x38);
            SetLong(e + 0x34, 0x8E0000);
            const std::int32_t x = Long(e + 0x34);
            e[0] = 1;
            e[5] = 0x52;
            e[1] = 0;
            const long h = SH_CALL(AreaMap_Elevation)(x, z);
            SetLong(e + 0x3C, static_cast<std::int32_t>(static_cast<std::uint32_t>(static_cast<int>(static_cast<short>(h))) << 16));
        }
        SetTimer(0x3C);
        SetStep(3);
        return;
    }
    case 3:
        if (C0() != 7) return;
        if (TakeInto(at::kSlot) != 0xFF) {
            unsigned char* const e = Slot850();
            e[0] = 1;
            e[5] = 0x52;
            e[1] = 1;
        }
        SetStep(4);
        return;
    case 4:
        if (C0() != 0x10) return;
        Transition(4);
        SetStep(5);
        return;
    case 5:
        if (!WaitDone()) return;
        PassFlags(0);
        SetTimer(0x40);
        SH_CALL(Music_FadeOut)(0x40);
        SetStep(6);
        return;
    case 6: {
        if (DecTimer() != 0) return;
        SH_CALL(Music_FadeOutStop)(0xA);
        unsigned char* const a = Record(Byte(at::kEffectState + 7));
        a[0x09] = 7;
        a[0x58] = 0x37;
        a[0x5B] = 8;
        a[0x5C] = 0x64;
        unsigned char* const b = Record(Byte(at::kEffectState + 8));
        b[0x09] = 8;
        b[0x5B] = 0xC;
        SH_CALL(Scenario_CallA)(0);
        ChangeArea(0x20, 0x650000, 0x80000, 1);
        SetStep(7);
        Byte(at::kAreaTrack) = 0xFF;
        return;
    }
    case 7: {
        if (C0() != 0x16) return;
        ChangeArea(0x20, 0x640000, 0x120000, 1);
        unsigned char* const bank = Bank();
        Byte(at::kAreaTrack) = 0xFF;
        SH_CALL(Flags_Set)(bank, 0);
        SetStep(8);
        return;
    }
    case 8:
        if (!WaitDone()) return;
        SetTimer(0x1E);
        SetStep(9);
        return;
    case 9:
        if (DecTimer() != 0) return;
        Sound(0x204);
        PassFlags(0x1F);
        Transition(1);
        SetStep(0xA);
        return;
    case 0xA:
        if (!WaitDone()) return;
        C0() = 0x17;
        SetStep(0xB);
        return;
    case 0xB:
        if (C0() != 0x1D) return;
        ChangeArea(0x20, 0x640000, 0x140000, 5);
        SetFlag(1);
        ScriptFlagsOr(0x20);
        SetStep(0xC);
        return;
    case 0xC:
        if (!WaitDone()) return;
        Clear40();
        C0() = 0;
        SetRun(0);
        SetStep(0);
        return;
    default:
        return;
    }
}

// original 0x5518E0: run 2, steps 0..2 (a chain of decrements): extra sprite
// 0's +0x83 = 3 and +0x8A = 0; on counter 3 (0x90384B) = 0x12 a 0x10-frame
// wait; flag 3, three bytes of 0x9039F0 set, the run over.
extern "C" void __cdecl Scena08_Scene2(void) {
    switch (Step()) {
    case 0:
        At(at::kSpritesExtra)[0x83] = 3;
        SetWord(At(at::kSpritesExtra) + 0x8A, 0);
        SetStep(1);
        return;
    case 1:
        if (Byte(at::kCounter3) != 0x12) return;
        SetTimer(0x10);
        SetStep(2);
        return;
    case 2:
        if (PostDecTimer() != 0) return;
        SetFlag(3);
        SetWord(At(at::kDrawPool + 6), 1);
        Byte(at::kDrawPool + 3) = 5;
        Byte(at::kDrawPool + 4) = 3;
        SetRun(0);
        SetStep(0);
        return;
    default:
        return;
    }
}

// original 0x551980: run 3, steps 0..0x12 (a jump table of 19 after it at
// 0x551D8C): a drop-in with the kind-2 sprite at member 0 and the F3 divisor
// from Field_MoveSpeeds +4; the kind-2 sprite placed on the map; an effect of
// kind 0x51; the music faded and the party placed; timed waits; sound 0x20A;
// message 0x12; event battle 0x20 (step 0xD waits for the battle to move the
// step); file 1 loaded; an effect of kind 0x63; music 0x6D; on counter 0 = 0
// three flags and the run over.
extern "C" void __cdecl Scena08_Scene3(void) {
    switch (Step()) {
    case 0: {
        SetLong(At(at::kCounters), 0);
        DropIn(6);
        const unsigned speed = Byte(at::kMoveSpeeds + 4);
        Kind2At(Long(At(at::kLeaderX)), Long(At(at::kLeaderZ)));
        SetWord(At(at::kF3Divisor), speed << 3);
        SetStep(1);
        return;
    }
    case 1: {
        if (Byte(at::kKind2Hold) != 0) return;
        const std::int32_t z = Long(At(at::kKind2Z));
        const std::int32_t x = Long(At(at::kKind2X));
        SetLong(At(at::kKind2Sprite + 0x34), x);
        SetLong(At(at::kKind2Sprite + 0x38), z);
        const long h = SH_CALL(AreaMap_Elevation)(x, z);
        SetWord(At(at::kKind2Sprite + 0x3E), static_cast<unsigned>(h));
        SetElevationAt(Long(At(at::kKind2X)), Long(At(at::kKind2Z)));
        C0() = 1;
        SetStep(2);
        return;
    }
    case 2:
        if (C0() != 5) return;
        if (TakeInto(at::kSlot) != 0xFF) {
            unsigned char* const e = Slot850();
            SetLong(e + 0x38, 0x418000);
            const std::int32_t z = Long(e + 0x38);
            SetLong(e + 0x34, 0x158000);
            const std::int32_t x = Long(e + 0x34);
            e[0] = 1;
            e[5] = 0x51;
            e[1] = 0;
            const long h = SH_CALL(AreaMap_Elevation)(x, z);
            SetLong(e + 0x3C, static_cast<std::int32_t>(static_cast<std::uint32_t>(static_cast<int>(static_cast<short>(h))) << 16));
        }
        SetStep(3);
        return;
    case 3:
        if (C0() != 6) return;
        SetTimer(0x20);
        SetStep(4);
        SH_CALL(Music_FadeOut)(0x20);
        return;
    case 4:
        if (PostDecTimer() != 0) return;
        IncC0();
        PlaceParty(0x1C8000, 0x418000, 0x20);
        SH_CALL(Music_FadeOutStop)(0xA);
        SetStep(5);
        return;
    case 5:
        if (C0() != 0xB) return;
        SetTimer(0x61);
        SetStep(6);
        return;
    case 6:
        if (PostDecTimer() != 0) return;
        SetTimer(0x20);
        IncC0();
        SetStep(7);
        return;
    case 7:
        if (PostDecTimer() != 0) return;
        SetStep(8);
        return;
    case 8:
        Sound(0x20A);
        SetStep(9);
        return;
    case 9:
        if (!WaitDone()) return;
        SetStep(0xA);
        IncC0();
        return;
    case 0xA:
        if (C0() != 0x17) return;
        PassFlags(0);
        MusicStop();
        Message(0x12);
        SetStep(0xB);
        return;
    case 0xB:
        if (Requested()) return;
        PassFlags(0x1F);
        SH_CALL(Sound_ResumeAll)();
        Sound(0x20B);
        IncC0();
        SetStep(0xC);
        return;
    case 0xC:
        if (C0() != 0x19) return;
        SH_CALL(Field_StartEventBattle)(0x20);
        SetStep(0xD);
        return;
    case 0xE:
        SH_CALL(LoadDatFile)(1);
        if (SH_CALL(File_LoadDone)() == 0) {
            do {
                SH_CALL(Field_LoadingFrame)();
                SH_CALL(Task_Sleep)(1);
            } while (SH_CALL(File_LoadDone)() == 0);
        }
        DropIn(7);
        IncC0();
        SetStep(0xF);
        return;
    case 0xF:
        if (C0() != 0x23) return;
        if (TakeInto(at::kSlot) != 0xFF) Live(Slot850(), 0x63);
        SetTimer(0x3C);
        SetStep(0x10);
        return;
    case 0x10:
        if (PostDecTimer() != 0) return;
        SH_CALL(Music_Play)(0x6D, 0x10);
        IncC0();
        SetTimer(0xA);
        SetStep(0x11);
        return;
    case 0x11:
        if (PostDecTimer() != 0) return;
        SetStep(0x12);
        return;
    case 0x12:
        if (C0() != 0) return;
        SH_CALL(Flags_Set)(At(at::kEventFlags), 3);
        SH_CALL(Flags_Set)(At(at::kEventFlags), 0x10);
        SetFlag(4);
        ScriptFlagsAnd(0xFFF7);
        Clear40();
        SetRun(0);
        SetStep(0);
        return;
    default:   // 0xD and past 0x12
        return;
    }
}

// original 0x551DE0: run 4, steps 0..0xA (a jump table of 11 after it at
// 0x551F08): counter 0 = 0xA and a drop-in; on 0 flag 5; step 2 waits for the
// sprite counter 3 names to raise bit 0x80, then a drop-in and the run over;
// message 0x24 (step 5); step 0xA (on counter 0xA) flag 6, 0x591BE0(0xBB8, 0)
// and the change to area 1.
extern "C" void __cdecl Scena08_Scene4(void) {
    switch (Step()) {
    case 0:
        C0() = 0xA;
        DropIn(1);
        SetStep(1);
        return;
    case 1:
        if (C0() != 0) return;
        Clear40();
        SetFlag(5);
        SetStep(2);
        return;
    case 2:
        if ((Sprite(Byte(at::kCounter3))[0] & 0x80) == 0) return;
        DropIn(2);
        SetRun(0);
        SetStep(0);
        return;
    case 5:
        Message(0x24);
        SetStep(6);
        return;
    case 6:
        if (Requested()) return;
        Clear40();
        SetRun(0);
        SetStep(0);
        return;
    case 0xA: {
        if (C0() != 0xA) return;
        unsigned char* const bank = Bank();
        C0() = 0;
        SH_CALL(Flags_Set)(bank, 6);
        Call591BE0(0xBB8, 0);
        SetRun(0);
        SetStep(0);
        Clear40();
        ChangeArea(1, 0x2C0000, 0xE8000, 3);
        ScriptFlagsAnd(0xFFF7);
        return;
    }
    default:   // 3, 4, 7..9 and past 0xA
        return;
    }
}

// original 0x551F40: run 5, steps 0..4 and 0x64 (a byte table of 0x65 at
// 0x5520B4 into a jump table of 7 at 0x552098): the change to area 2 and step
// 0x64; in area 2 an effect of kind 0x57; on counter 1 the change to area
// 0x35 (step 1 runs on into step 2's test); on 2 another; Cond_ByteFE on 6;
// on 8 the party refreshed, flag 7, the change to area 0x73 and the run over.
extern "C" void __cdecl Scena08_Scene5(void) {
    switch (Step()) {
    case 0:
        ChangeArea(2, 0x4E0000, 0x340000, 0x85);
        Byte(at::kAreaTrack) = 0xFF;
        SetStep(0x64);
        return;
    case 0x64: {
        if (Area() != 2) return;
        const unsigned char slot = SH_CALL(Effect_FindFree)();
        if (slot != 0xFF) Live(EffectAt(slot), 0x57);
        SetStep(1);
        return;
    }
    case 1:
        if (C0() == 1) {
            ChangeArea(0x35, 0x3D8000, 0xD8000, 0x83);
            Byte(at::kPendingKind) = 0xFF;
            Byte(at::kAreaTrack) = 0xFF;
            SetStep(2);
        }
        [[fallthrough]];
    case 2:
        if (C0() != 2) return;
        ChangeArea(0x35, 0x138000, 0x180000, 0x84);
        SetStep(3);
        return;
    case 3:
        if (C0() != 6) return;
        Byte(at::kByteFE) = 1;
        SetStep(4);
        return;
    case 4:
        if (C0() != 8) return;
        PartyRestore();
        SetFlag(7);
        ChangeArea(0x73, 0x1B0000, 0x210000, 3);
        ScriptFlagsAnd(0xFFF7);
        Clear40();
        C0() = 0;
        SetRun(0);
        SetStep(0);
        return;
    default:
        return;
    }
}

// original 0x552120: run 6, steps 0..8 (a jump table of 9 after it at
// 0x5523B0): the music faded over 0x20 frames and a drop-in; effects of kind
// 0x13 / 0x5F kept in 0x6BC72F and waited on; the party placed for event
// battle 0x21 (step 7 waits for the battle to move the step); step 8 flag 8
// and the run over.
extern "C" void __cdecl Scena08_Scene6(void) {
    switch (Step()) {
    case 0:
        SH_CALL(Music_FadeOut)(0x20);
        SetTimer(0x20);
        SetStep(1);
        return;
    case 1:
        if (PostDecTimer() != 0) return;
        SH_CALL(Music_FadeOutStop)(0x20);
        C0() = 0;
        DropIn(0);
        SetStep(2);
        return;
    case 2: {
        if (C0() != 1) return;
        const unsigned char slot = TakeInto(at::kKeptEffect);
        if (slot != 0xFF) Effect13(EffectAt(slot), -0x318, S16(at::kAngleY), S16(at::kPitch), 0x1E);
        SetStep(3);
        return;
    }
    case 3: {
        if (EffectAt(Byte(at::kKeptEffect))[0] & 1) return;
        if (TakeInto(at::kSlot) != 0xFF) {
            unsigned char* const e = Slot850();
            e[0] = 1;
            e[5] = 0x5F;
            e[1] = 0;
        }
        const unsigned char slot = TakeInto(at::kKeptEffect);
        if (slot != 0xFF) Effect13(EffectAt(slot), S16(at::kYaw), S16(at::kAngleY), 0x1C, 0xB4);
        IncC0();
        SetStep(4);
        return;
    }
    case 4: {
        if (EffectAt(Byte(at::kKeptEffect))[0] & 1) return;
        const unsigned char slot = TakeInto(at::kKeptEffect);
        if (slot != 0xFF) Effect13(EffectAt(slot), -0x2AA, 0, 0x200, 0x5A);
        IncC0();
        PlaceParty(0x430000, 0x380000, 0x21);
        DropIn(1);
        SetStep(5);
        return;
    }
    case 5:
        if (EffectAt(Byte(at::kKeptEffect))[0] & 1) return;
        IncC0();
        SetStep(6);
        return;
    case 6:
        if (C0() != 5) return;
        SH_CALL(Field_StartEventBattle)(0x21);
        SetStep(7);
        return;
    case 8:
        C0() = 0;
        Clear40();
        SetFlag(8);
        ScriptFlagsAnd(0xFFF7);
        SetRun(0);
        SetStep(0);
        return;
    default:   // 7 and past 8
        return;
    }
}

// original 0x5523E0: run 7, steps 0..0xF (a byte table of 0x10 at 0x552594
// into a jump table of 9 at 0x552570): a timed drop-in; on counter 0 flag 9
// and the run over; step 0xA the change to area 0; on 0xC a fade out, the
// party refreshed and stream 0; on its end a fade in and flag 0xB; on counter
// 0 the object 0x903804 points at set up (+6, +0x18, +0x1C) and the run over.
extern "C" void __cdecl Scena08_Scene7(void) {
    switch (Step()) {
    case 0:
        if (PostDecTimer() != 0) return;
        DropIn(0);
        SetStep(1);
        return;
    case 1:
        if (C0() != 0) return;
        SetFlag(9);
        Clear40();
        SetRun(0);
        SetStep(0);
        return;
    case 0xA:
        ChangeArea(0, 0x5C0000, 0x498000, 0x86);
        SetStep(0xB);
        return;
    case 0xB:
        if (C0() != 0xC) return;
        Transition(0);
        SetStep(0xC);
        return;
    case 0xC:
        if (!WaitDone()) return;
        PassFlags(0);
        PartyRestore();
        MusicStop();
        SH_CALL(Sound_LoadStream)(0);
        SetStep(0xD);
        return;
    case 0xD:
        if (SH_CALL(Sound_StreamDone)() == 0) return;
        SH_CALL(Sound_ResumeAll)();
        PassFlags(0x1F);
        Transition(1);
        SetStep(0xE);
        return;
    case 0xE:
        if (!WaitDone()) return;
        SetFlag(0xB);
        IncC0();
        SetStep(0xF);
        return;
    case 0xF:
        if (C0() != 0) return;
        FocusObject()[6] = 5;
        SetLong(FocusObject() + 0x18, 1);
        SetLong(FocusObject() + 0x1C, 3);
        Clear40();
        SetRun(0);
        SetStep(0);
        return;
    default:   // 2..9 and past 0xF
        return;
    }
}

// original 0x552CA0: two event objects for run 8 - two free sprites from
// 0x57CD90 (their indices kept as dwords in 0x6BC724 / 0x6BC728, each marked
// live; no second one: the first freed again), then for each (its index to
// 0x903850 as a u16) EventOp_6x on its record of Scena08_PairOps 0x6613B8 and
// Sprite_SetAnimationAt 0x4C / 0x36.
extern "C" void __cdecl Scena08_SpawnPair(void) {
    const std::uint32_t a = SpriteFindFree() & 0xFF;
    SetLong(At(at::kKeptX), static_cast<std::int32_t>(a));
    if (a == 0xFF) return;
    Sprite(a)[0] = 1;
    const std::uint32_t b = SpriteFindFree() & 0xFF;
    SetLong(At(at::kKeptZ), static_cast<std::int32_t>(b));
    if (b == 0xFF) {
        Sprite(static_cast<std::uint32_t>(Long(At(at::kKeptX))))[0] = 0;
        return;
    }
    SetWord(At(at::kSlot), Word(At(at::kKeptX)));
    Sprite(b)[0] = 1;
    SH_CALL(EventOp_6x)(At(at::kPairOps8));
    SH_CALL(Sprite_SetAnimationAt)(0x4C, 0);
    SetWord(At(at::kSlot), Word(At(at::kKeptZ)));
    SH_CALL(EventOp_6x)(At(at::kPairOps8 + 0x10));
    SH_CALL(Sprite_SetAnimationAt)(0x36, 0);
}

// original 0x5525B0: run 8, steps 0..0x1B (a jump table of 28 after it at
// 0x552C30): the change to area 0xB, message 0x27, music 0xC, effects of
// kind 0x13 kept in 0x6BC72F, a transition and the kept area change back
// (0x6BC720); from step 0xA effects on counter values, scenario call B 0 and
// the pair of event objects (the party lists' bytes 0x904065 / 0x904066 kept),
// the kind-2 sprite placed, music of the area's track; on message 0xE the
// object 0x903804 points at kept (its place and sprite index); on counter 0
// flag 0xE, that sprite freed, scenario call A 2, the party lists' bytes put
// back and member 1 placed at the kept point.
extern "C" void __cdecl Scena08_Scene8(void) {
    switch (Step()) {
    case 0:
        if (!WaitDone()) return;
        ChangeArea(0xB, 0x138000, 0x238000, 0x84);
        Byte(at::kPendingKind) = 0xFF;
        Byte(at::kAreaTransition) = 0xFF;
        Byte(at::kAreaTrack) = 0xFF;
        SetStep(1);
        return;
    case 1:
        if (!WaitDone()) return;
        SH_CALL(Msg_OpenScript)(0x27);
        Request(2);
        SetStep(2);
        return;
    case 2:
        if (Requested()) return;
        PassFlags(0x1F);
        SH_CALL(Music_Play)(0xC, 0x20);
        Transition(1);
        SetStep(3);
        return;
    case 3:
        if (!WaitDone()) return;
        C0() = 1;
        SetStep(4);
        return;
    case 4: {
        if (C0() != 2) return;
        const unsigned char slot = TakeInto(at::kKeptEffect);
        if (slot != 0xFF) Effect13(EffectAt(slot), -0x44C, S16(at::kAngleY), 0x124, 0x40);
        SetStep(5);
        return;
    }
    case 5:
        if (C0() != 5) return;
        Transition(6);
        SetStep(6);
        return;
    case 6:
        if (!WaitDone()) return;
        PassFlags(0);
        SetStep(7);
        return;
    case 7: {
        if (Requested()) return;
        SetFlag(0xC);
        const unsigned flags = Byte(at::kKeptFlags);
        const std::int32_t z = Long(At(at::kKeptZ));
        const std::int32_t x = Long(At(at::kKeptX));
        ChangeArea(0xC, x, z, flags);
        Byte(at::kAreaTransition) = 0xFF;
        Byte(at::kPendingKind) = 0xFF;
        SetStep(8);
        return;
    }
    case 8:
        if (!WaitDone()) return;
        if (Byte(at::kRequest) == 5) return;
        Transition(1);
        PassFlags(0x1F);
        C0() = 0;
        Clear40();
        SetRun(0);
        SetStep(0);
        return;
    case 0xA:
        if (PostDecTimer() != 0) return;
        Clear40();
        DropIn(0);
        SetStep(0xB);
        return;
    case 0xB: {
        if (C0() != 2) return;
        const unsigned char slot = TakeInto(at::kKeptEffect);
        if (slot == 0xFF) return;
        SetStep(0xC);
        Effect13(EffectAt(slot), S16(at::kYaw), S16(at::kAngleY), 0x2B0, 0x20);
        SetTimer(0x20);
        SH_CALL(Music_FadeOut)(0x20);
        return;
    }
    case 0xC:
        if (PostDecTimer() != 0) return;
        SH_CALL(Music_FadeOutStop)(0x20);
        Sound(0x201);
        IncC0();
        SetStep(0xD);
        return;
    case 0xD: {
        if (C0() != 6) return;
        const unsigned char slot = TakeInto(at::kKeptEffect);
        if (slot == 0xFF) return;
        SetStep(0xE);
        Effect13(EffectAt(slot), S16(at::kYaw), S16(at::kAngleY), 0x200, 0x20);
        return;
    }
    case 0xE: {
        if (EffectAt(Byte(at::kKeptEffect))[0] & 1) return;
        const unsigned char c = C0();
        ScriptFlagsOr(0x20);
        const unsigned char list5 = Byte(at::kPartyList5);
        C0() = static_cast<unsigned char>(c + 1);
        const unsigned char list6 = Byte(at::kPartyList6);
        Byte(at::kKeptList5) = list5;
        Byte(at::kKeptList6) = list6;
        SH_CALL(Scenario_CallB)(0);
        SH_CALL(Scena08_SpawnPair)();
        SetStep(0xF);
        return;
    }
    case 0x14:
        DropIn(1);
        SetStep(0x15);
        return;
    case 0x15: {
        if (C0() != 0xA) return;
        const unsigned char slot = TakeInto(at::kKeptEffect);
        if (slot == 0xFF) return;
        SetStep(0x16);
        Effect13(EffectAt(slot), S16(at::kYaw), S16(at::kAngleY), 0x13A, 0x40);
        return;
    }
    case 0x16: {
        if (C0() != 0x12) return;
        const unsigned char slot = TakeInto(at::kKeptEffect);
        if (slot == 0xFF) return;
        Effect13(EffectAt(slot), -0x2AA, S16(at::kAngleY), 0x360, 0x20);
        SH_CALL(Kind2_Place)(0);
        ScriptFlagsOr(8);
        SetStep(0x17);
        return;
    }
    case 0x17: {
        if (C0() != 0x16) return;
        const unsigned char slot = TakeInto(at::kKeptEffect);
        if (slot == 0xFF) return;
        SetStep(0x18);
        Effect13(EffectAt(slot), -0x2AA, S16(at::kAngleY), 0x200, 0x20);
        return;
    }
    case 0x18:
        if (C0() != 0x1A) return;
        Clear40();
        SH_CALL(Music_FadeOutStop)(8);
        SetFlag(0xD);
        SetTimer(0x3C);
        SetStep(0x19);
        return;
    case 0x19:
        if (PostDecTimer() != 0) return;
        SH_CALL(Music_Play)(Byte(at::kAreaTrack), 8);
        SetStep(0x1A);
        return;
    case 0x1A: {
        if (Word(At(at::kMessage)) != 0xE) return;
        if (!Requested()) return;
        const std::uint32_t p = static_cast<std::uint32_t>(Long(At(at::kFocusObject)));
        SetLong(At(at::kKeptX), Long(At(p + 0x34)));
        SetLong(At(at::kKeptZ), Long(At(p + 0x38)));
        // (p - Sprite_Objects) / 0xA4, a signed division truncated toward 0.
        Byte(at::kKeptFlags) =
            static_cast<unsigned char>(static_cast<std::int32_t>(p - at::kSprites) / static_cast<std::int32_t>(at::kSpriteStride));
        DropIn(2);
        SetStep(0x1B);
        return;
    }
    case 0x1B: {
        if (C0() != 0) return;
        SetFlag(0xE);
        const std::uint32_t index = static_cast<std::uint32_t>(Long(At(at::kKeptFlags))) & 0xFF;
        ScriptFlagsAnd(0xFFDF);
        Sprite(index)[0] = 0;
        SH_CALL(Scenario_CallA)(2);
        Byte(at::kPartyList5) = Byte(at::kKeptList5);
        Byte(at::kPartyList6) = Byte(at::kKeptList6);
        const std::int32_t x = Long(At(at::kKeptX));
        const std::int32_t z = Long(At(at::kKeptZ));
        SetLong(At(at::kMembers + at::kMemberStride + 0x34), x);
        SetLong(At(at::kMembers + at::kMemberStride + 0x38), z);
        const long h = SH_CALL(AreaMap_Elevation)(x, z);
        SetWord(At(at::kMembers + at::kMemberStride + 0x3E), static_cast<unsigned>(h));
        SetRun(0);
        SetStep(0);
        return;
    }
    default:   // 9, 0xF..0x13 and past 0x1B
        return;
    }
}

// original 0x552D50: run 9, steps 0..7 (a jump table of 8 after it at
// 0x552EC8): on counter 8 the kind-2 sprite placed; on 0x14 a 0x78-frame
// wait; on counter 0 the change to area 0xD with flag 0xF; step 5 a drop-in;
// on 5 an effect of kind 0x5F; after 0x1E frames sound 0x203, flag 0x10 and
// the change to area 0xF.
extern "C" void __cdecl Scena08_Scene9(void) {
    switch (Step()) {
    case 0:
        if (C0() != 8) return;
        SH_CALL(Kind2_Place)(0);
        ScriptFlagsOr(8);
        SetStep(1);
        return;
    case 1:
        if (C0() != 0x14) return;
        C0() = 0x15;
        SetTimer(0x78);
        SetStep(2);
        return;
    case 2:
        if (DecTimer() != 0) return;
        SetStep(3);
        return;
    case 3:
        if (C0() != 0) return;
        Clear40();
        ChangeArea(0xD, 0x190000, 0x100000, 0x84);
        SetFlag(0xF);
        SetRun(0);
        SetStep(0);
        return;
    case 5:
        DropIn(5);
        SetStep(6);
        return;
    case 6:
        if (C0() != 5) return;
        if (TakeInto(at::kSlot) != 0xFF) {
            unsigned char* const e = Slot850();
            e[0] = 1;
            e[5] = 0x5F;
            e[1] = 6;
        }
        SetTimer(0x1E);
        SetStep(7);
        return;
    case 7:
        if (DecTimer() != 0) return;
        C0() = 0;
        Clear40();
        Sound(0x203);
        SetFlag(0x10);
        ChangeArea(0xF, 0x198000, 0x510000, 1);
        SetRun(0);
        SetStep(0);
        return;
    default:   // 4 and past 7
        return;
    }
}

// original 0x552EF0: run 10, steps 0 and 0xA..0xC (a byte table of 0xD at
// 0x553034 into a jump table of 5 at 0x553020): step 0 a drop-in and the run
// over; effects of kind 0x13 (the second kept in 0x6BC72F and waited on);
// then flag 0x14 and the run over.
extern "C" void __cdecl Scena08_Scene10(void) {
    switch (Step()) {
    case 0:
        Clear40();
        DropIn(1);
        SetRun(0);
        SetStep(0);
        return;
    case 0xA:
        if (TakeInto(at::kSlot) == 0xFF) return;
        SetStep(0xB);
        Effect13(Slot850(), -0x2EC, S16(at::kAngleY), 0xB6, 0x20);
        return;
    case 0xB: {
        if (C0() != 6) return;
        const unsigned char slot = TakeInto(at::kKeptEffect);
        if (slot == 0xFF) return;
        SetStep(0xC);
        Effect13(EffectAt(slot), -0x2AA, S16(at::kAngleY), 0x200, 0x20);
        return;
    }
    case 0xC:
        if (EffectAt(Byte(at::kKeptEffect))[0] & 1) return;
        Clear40();
        SetFlag(0x14);
        SetRun(0);
        SetStep(0);
        return;
    default:
        return;
    }
}

// original 0x553630: character record 4 (0x903D00) set up - its dword +0xC
// += 10000, 0x498DE0(4), its bytes +0x12..+0x15 set, Char_RecalcStats on the
// record, Char_HealHp(4, 0, 0) and Char_HealAp(4, 0, 0).
extern "C" void __cdecl Scena08_SetUpRecord4(void) {
    SetLong(At(at::kRecord4 + 0xC), static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(At(at::kRecord4 + 0xC))) + 0x2710u));
    Call498DE0(4);
    Byte(at::kRecord4 + 0x12) = 7;
    Byte(at::kRecord4 + 0x13) = 0x38;
    Byte(at::kRecord4 + 0x14) = 0x21;
    Byte(at::kRecord4 + 0x15) = 0xE;
    SH_CALL(Char_RecalcStats)(At(at::kRecord4));
    SH_CALL(Char_HealHp)(4, 0, 0);
    SH_CALL(Char_HealAp)(4, 0, 0);
}

// original 0x553050: run 11, steps 0..0x15 and 0x64 (a byte table of 0x65 at
// 0x5535BC into a jump table of 23 at 0x553560): a party set loaded in two
// halves with the kind-2 sprite placed; scenario call A 5; an effect of kind
// 0x60 and sound 0x200; message 9 and record 4 set up (Scena08_SetUpRecord4),
// two character records' bits set; the party placed for event battle 0x22 (step 0xB
// waits for the battle to move the step); flag 0x15; scenario call A 6 and
// the change to area 0x2B; music 0x73, stream 3; message 0x22 and a character
// record's bit cleared; music 0x64 and the kind-2 sprite at member 0; the run
// over with Field_StatusBits |= 0x80.
extern "C" void __cdecl Scena08_Scene11(void) {
    switch (Step()) {
    case 0:
        DropIn(0);
        SH_CALL(PartySet_LoadFirst)(7, 8, 4);
        SetStep(1);
        return;
    case 1: {
        if (C0() != 1) return;
        const unsigned speed = Byte(at::kMoveSpeeds + 5);
        SetStep(2);
        SetLong(At(at::kKind2X), 0x170000);
        SetLong(At(at::kKind2Sprite + 0x34), 0x170000);
        SetLong(At(at::kKind2Z), 0xF0000);
        SetLong(At(at::kKind2Sprite + 0x38), 0xF0000);
        SetWord(At(at::kF3Divisor), speed << 3);
        return;
    }
    case 2:
        if (SH_CALL(File_LoadDone)() == 0) return;
        if (Byte(at::kKind2Hold) != 0) return;
        if (TakeInto(at::kSlot) != 0xFF) {
            unsigned char* const e = Slot850();
            e[0] = 1;
            e[5] = 0x5F;
            e[1] = 8;
        }
        SetTimer(0x10);
        SetStep(0x64);
        return;
    case 0x64:
        if (DecTimer() != 0) return;
        SH_CALL(PartySet_LoadSecond)(7, 8, 4, 0);
        IncC0();
        SetStep(3);
        return;
    case 3:
        if (C0() != 3) return;
        if (SH_CALL(File_LoadDone)() == 0) return;
        SH_CALL(Scenario_CallA)(5);
        DropIn(1);
        IncC0();
        SetStep(4);
        return;
    case 4:
        if (C0() != 8) return;
        if (TakeInto(at::kSlot) != 0xFF) {
            unsigned char* const e = Slot850();
            e[0] = 1;
            e[5] = 0x60;
            e[1] = 0;
        }
        Sound(0x200);
        SetTimer(0xC);
        SetStep(5);
        return;
    case 5:
        if (DecTimer() != 0) return;
        SetStep(6);
        IncC0();
        return;
    case 6:
        if (C0() != 0x17) return;
        PassFlags(0);
        SH_CALL(Music_FadeOutStop)(0x20);
        Message(9);
        SetStep(7);
        return;
    case 7: {
        if (Requested()) return;
        SH_CALL(Scena08_SetUpRecord4)();
        Request(6);
        unsigned char* const a = RecordOfMember7();
        SetStep(8);
        a[0xB] = static_cast<unsigned char>(a[0xB] | 2);
        unsigned char* const b = RecordOfMember4();
        b[0xB] = static_cast<unsigned char>(b[0xB] | 2);
        SetWord(b + 0x18, 0);
        b[0x11] = static_cast<unsigned char>(b[0x11] | 0x60);
        return;
    }
    case 8:
        if (Byte(at::kRequest) != 0) return;
        SetStep(9);
        Request(1);
        Byte(at::kLoadByte) = 1;
        return;
    case 9:
        if (Byte(at::kRequest) != 0) return;
        DropIn(2);
        PassFlags(0x1F);
        Transition(1);
        PlaceParty(0x150000, 0x190000, 0x22);
        SetStep(0xA);
        return;
    case 0xA:
        if (C0() != 0x1A) return;
        SH_CALL(Field_StartEventBattle)(0x22);
        SetStep(0xB);
        return;
    case 0xC: {
        IncC0();
        DropIn(3);
        unsigned char* const bank = Bank();
        SetStep(0xD);
        SH_CALL(Flags_Set)(bank, 0x15);
        return;
    }
    case 0xD:
        if (C0() != 0x1E) return;
        Transition(0);
        SetStep(0xE);
        return;
    case 0xE:
        if (!WaitDone()) return;
        PassFlags(0);
        SH_CALL(Scenario_CallA)(6);
        ChangeArea(0x2B, 0x110000, 0x170000, 0x84);
        Byte(at::kAreaTrack) = 0xFF;
        SetTimer(0x78);
        SetStep(0xF);
        return;
    case 0xF:
        if (DecTimer() != 0) return;
        PassFlags(0x1F);
        SH_CALL(Music_Play)(0x73, 0x20);
        Transition(1);
        IncC0();
        SetStep(0x10);
        return;
    case 0x10:
        if (C0() != 0x3A) return;
        SH_CALL(Music_FadeOutStop)(8);
        SH_CALL(Sound_LoadStream)(3);
        SetStep(0x11);
        return;
    case 0x11:
        if (SH_CALL(Sound_StreamDone)() == 0) return;
        Transition(0);
        SetStep(0x12);
        return;
    case 0x12:
        if (!WaitDone()) return;
        PassFlags(0);
        Message(0x22);
        SetStep(0x13);
        return;
    case 0x13: {
        if (Requested()) return;
        Request(6);
        unsigned char* const a = RecordOfMember7();
        SetStep(0x14);
        a[0xB] = static_cast<unsigned char>(a[0xB] | 2);
        unsigned char* const b = RecordOfMember4();
        b[0xB] = static_cast<unsigned char>(b[0xB] & 0xFD);
        return;
    }
    case 0x14: {
        if (Byte(at::kRequest) != 0) return;
        SH_CALL(Music_Play)(0x64, 0x20);
        PassFlags(0x1F);
        Transition(1);
        const std::int32_t z = Long(At(at::kLeaderZ));
        const std::int32_t x = Long(At(at::kLeaderX));
        C0() = 0;
        Kind2At(x, z);
        SetElevationAt(x, z);
        SH_CALL(Field_ViewReset)();
        SetStep(0x15);
        return;
    }
    case 0x15:
        if (!WaitDone()) return;
        Clear40();
        StatusBit80();
        SetRun(0);
        SetStep(0);
        return;
    default:   // 0xB and 0x16..0x63
        return;
    }
}

// original 0x553690: slot 1, the object trigger (0x56D6D0 with the object):
// Scena08_ObjectHandlers 0x6613D8 by the object's +0x86, unchecked, called
// with (object, the flag row). 0x56D6D0 reads no answer.
extern "C" void __cdecl Scena08_ObjectHook(unsigned char* object) {
    unsigned char* const bank = Bank();
    const unsigned index = object[0x86];
    reinterpret_cast<ObjectHandler>(static_cast<std::uintptr_t>(Entry(at::kObjects8, static_cast<int>(index))))(object, bank);
}

// original 0x5536B0: object handler 0 - counter 0 cleared, run 7 with a
// 0x20-frame timer.
extern "C" void __cdecl Scena08_Object0(unsigned char*, unsigned char*) {
    C0() = 0;
    Set40();
    SetRun(7);
    SetTimer(0x20);
    SetStep(0);
}

// original 0x5536E0: object handler 1 - counter 0 cleared, run 7 at step 0xA.
extern "C" void __cdecl Scena08_Object1(unsigned char*, unsigned char*) {
    C0() = 0;
    Set40();
    SetRun(7);
    SetStep(0xA);
}

// original 0x553700: object handler 2 - counter 0 cleared, run 10, flag 0x13
// (the row 0x929ED0 names).
extern "C" void __cdecl Scena08_Object2(unsigned char*, unsigned char*) {
    C0() = 0;
    Set40();
    unsigned char* const bank = Bank();
    SetRun(0xA);
    SetStep(0);
    SH_CALL(Flags_Set)(bank, 0x13);
}

// original 0x553750: slot 2, the step hook (x, z) - area 1 (x 0x338000, z
// cells 0x27..0x28): run 4 step 5 until flag 6, then run 5 until flag 7;
// area 0x46 (x 0x388000, z cells 0x2E..0x31) flag 0x16 and 0. 1 in al when
// a scene starts.
extern "C" unsigned char __cdecl Scena08_StepHook(int x, int z) {
    const unsigned zh = Hi(z);
    if (Area() == 1) {
        if (x != 0x338000) return 0;
        if (!Near(zh, 0x27, 2)) return 0;
        if (!Flag(6)) {
            Set40();
            SetRun(4);
            SetStep(5);
            return 1;
        }
        if (!Flag(7)) {
            Set40();
            C0() = 0;
            SetStep(0);
            SetRun(5);
            return 1;
        }
    }
    if (Area() == 0x46 && x == 0x388000 && Near(zh, 0x2E, 4)) SetFlag(0x16);
    return 0;
}

// original 0x553810: slot 3, the arrive hook (x, z) - area 1 (not flag 5, x
// cells 0x1C..0x1D, z cells 0x2B..0x2C) run 4; area 0xC (flag 0xB not 0xD) by
// exact x: 0x2F0000 (z up to 0x1F0000, not already run 8) run 8 step 0xA;
// 0x1B0000 / 0x160000 / 0x110000 at an exact z flags 0x18 / 0x19 / 0x1A
// once with sounds 0x201..0x203; 0x70000 (z 0x130000) sound 0x204 and run 8
// step 0x14; area 0xD (flag 0xF not 0x10, z up to 0x38000, x cells
// 0x1C..0x1D) run 9 step 5; area 0x23 (not flag 8, x cells 0x45..0x47, z
// cells 0x37..0x39, member 0's kind not 0, 6 or 7) run 6; area 0x2B (flag
// 0x14 not 0x15, z 0xF0000 or more, x cells 0x20..0x25) run 11. 1 in al when
// a scene starts.
extern "C" unsigned char __cdecl Scena08_ArriveHook(int x, int z) {
    const unsigned xh = Hi(x), zh = Hi(z);
    if (Area() == 1 && !Flag(5) && Near(xh, 0x1C, 2) && Near(zh, 0x2B, 2)) {
        SetRun(4);
        SetStep(0);   // the flag test's al, 0 here
        Set40();
        return 1;
    }
    if (Area() == 0xC && Flag(0xB) && !Flag(0xD)) {
        if (x == 0x2F0000) {
            if (z <= 0x1F0000 && Byte(at::kRun) != 8) {
                Set40();
                SetRun(8);
                SetStep(0xA);
                SetTimer(0x14);
                return 1;
            }
        } else if (x == 0x1B0000) {
            if (z == 0x110000 && !Flag(0x18)) {
                SetFlag(0x18);
                Sound(0x201);
            }
        } else if (x == 0x160000) {
            if (z == 0xD0000 && !Flag(0x19)) {
                SetFlag(0x19);
                Sound(0x202);
            }
        } else if (x == 0x110000) {
            if (z == 0x130000 && !Flag(0x1A)) {
                SetFlag(0x1A);
                Sound(0x203);
            }
        } else if (x == 0x70000 && z == 0x130000) {
            Sound(0x204);
            Set40();
            SetRun(8);
            SetStep(0x14);
            return 1;
        }
    }
    if (Area() == 0xD && !Flag(0x10) && Flag(0xF) && z <= 0x38000 && Near(xh, 0x1C, 2)) {
        Set40();
        SetRun(9);
        SetStep(5);
        return 1;
    }
    if (Area() == 0x23 && !Flag(8) && Near(xh, 0x45, 3) && Near(zh, 0x37, 3)) {
        const unsigned char kind = Byte(at::kLeaderKind);
        Byte(at::kSlot) = kind;
        if (kind != 6 && kind != 7 && kind != 0) {
            SetRun(6);
            SetStep(0);
            Set40();
            return 1;
        }
    }
    if (Area() == 0x2B && Flag(0x14) && !Flag(0x15) && z >= 0xF0000 && Near(xh, 0x20, 6)) {
        Set40();
        SetRun(0xB);
        SetStep(0);
        C0() = 0;
        return 1;
    }
    return 0;
}

void ScenaSc7_Inject() {
    if (bof3::WantsShadow("scena_sc7")) scena_sc7::SelfTest();
    BOF3_INJECT(Scena07_Frame);
    BOF3_INJECT(Scena07_Start);
    BOF3_INJECT(Scena07_EnterArea);
    BOF3_INJECT(Scena07_Run);
    BOF3_INJECT(Scena07_Scene1);
    BOF3_INJECT(Scena07_Scene2);
    BOF3_INJECT(Scena07_Scene3);
    BOF3_INJECT(Scena07_Scene4);
    BOF3_INJECT(Scena07_TimedEffects);
    BOF3_INJECT(Scena07_Scene5);
    BOF3_INJECT(Scena07_Scene6);
    BOF3_INJECT(Scena07_ShakeCamera);
    BOF3_INJECT(Scena07_PlaceObjects);
    BOF3_INJECT(Scena07_Scene7);
    BOF3_INJECT(Scena07_Scene8);
    BOF3_INJECT(Scena07_ObjectHook);
    BOF3_INJECT(Scena07_Object0);
    BOF3_INJECT(Scena07_Object1);
    BOF3_INJECT(Scena07_Object2);
    BOF3_INJECT(Scena07_Object3);
    BOF3_INJECT(Scena07_Object4);
    BOF3_INJECT(Scena07_TakeEffect49);
    BOF3_INJECT(Scena07_StepHook);
    BOF3_INJECT(Scena07_ArriveHook);
    BOF3_INJECT(Scena07_PartyHas89State2);
    BOF3_INJECT(Scena07_CellHook);
    BOF3_INJECT(Scena07_Cell0);
    BOF3_INJECT(Scena08_Frame);
    BOF3_INJECT(Scena08_Start);
    BOF3_INJECT(Scena08_PartyBitsToRecord0);
    BOF3_INJECT(Scena08_EnterArea);
    BOF3_INJECT(Scena08_Run);
    BOF3_INJECT(Scena08_Scene1);
    BOF3_INJECT(Scena08_Scene2);
    BOF3_INJECT(Scena08_Scene3);
    BOF3_INJECT(Scena08_Scene4);
    BOF3_INJECT(Scena08_Scene5);
    BOF3_INJECT(Scena08_Scene6);
    BOF3_INJECT(Scena08_Scene7);
    BOF3_INJECT(Scena08_Scene8);
    BOF3_INJECT(Scena08_SpawnPair);
    BOF3_INJECT(Scena08_Scene9);
    BOF3_INJECT(Scena08_Scene10);
    BOF3_INJECT(Scena08_Scene11);
    BOF3_INJECT(Scena08_SetUpRecord4);
    BOF3_INJECT(Scena08_ObjectHook);
    BOF3_INJECT(Scena08_Object0);
    BOF3_INJECT(Scena08_Object1);
    BOF3_INJECT(Scena08_Object2);
    BOF3_INJECT(Scena08_SwapKeyItem4);
    BOF3_INJECT(Scena08_StepHook);
    BOF3_INJECT(Scena08_ArriveHook);
}

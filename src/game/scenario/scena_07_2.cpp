// Moved here by tools/move_funcs.py from scena_sc7.cpp (docs/source-layout-round.md).
#include "game/scenario/scena_07_2.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scena_sc7_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = scena_sc7::at;

}  // namespace

namespace {

using move_script::At;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

using Handler = void (__cdecl*)();

using ObjectHandler = void (__cdecl*)(unsigned char* object, unsigned char* bank);

unsigned char& Byte(std::uint32_t a) { return At(a)[0]; }

unsigned Area() { return Game_AreaNumber; }

unsigned Step() { return MoveScript_PartyRecords[0x65]; }

void SetState(unsigned v) { MoveScript_PartyRecords[0x62] = static_cast<unsigned char>(v); }

void SetStep(unsigned v) { MoveScript_PartyRecords[0x65] = static_cast<unsigned char>(v); }

void SetRun(unsigned v) { MoveScript_Var7 = static_cast<unsigned char>(v); }

unsigned Timer() { return Word(At(::bof3::Img(MoveScript_PartyRecords_addr) + 0x66)); }

void SetTimer(unsigned v) { SetWord(At(::bof3::Img(MoveScript_PartyRecords_addr) + 0x66), v); }

// `dec word [timer]` then the flags of the result: the timer after.
unsigned DecTimer() {
    SetWord(At(::bof3::Img(MoveScript_PartyRecords_addr) + 0x66), Word(At(::bof3::Img(MoveScript_PartyRecords_addr) + 0x66)) - 1u);
    return Word(At(::bof3::Img(MoveScript_PartyRecords_addr) + 0x66));
}

void IncTimer() { SetWord(At(::bof3::Img(MoveScript_PartyRecords_addr) + 0x66), Word(At(::bof3::Img(MoveScript_PartyRecords_addr) + 0x66)) + 1u); }

unsigned char& C0() { return Byte(::bof3::Img(scena_sc7::at::kCounter0)); }

void IncC0() { C0() = static_cast<unsigned char>(C0() + 1); }

void PassFlags(unsigned v) { Draw_PassFlags = static_cast<unsigned char>(v); }

bool WaitDone() { return Transition_Busy == 0; }

bool Requested() { return Field_Request == 2; }

void Request(unsigned v) { Field_Request = static_cast<unsigned char>(v); }

void ScriptFlagsOr(unsigned v) { Byte(::bof3::Img(scena_sc7::at::kScriptFlags)) = static_cast<unsigned char>(Byte(::bof3::Img(scena_sc7::at::kScriptFlags)) | v); }

void ScriptFlagsAnd(unsigned mask) { Field_ScriptFlags = Field_ScriptFlags & mask; }

short S16(std::uint32_t a) { return static_cast<short>(Word(At(a))); }

void Kind2At(std::int32_t x, std::int32_t z) {
    Field_Kind2X = x;
    Field_Kind2Z = z;
}

unsigned char* SpriteCurrent() {
    return Sprite_Current;
}

unsigned char* ActiveMember() {
    return Field_ActiveMember;
}

unsigned char* Sprite(std::uint32_t k) { return At(::bof3::Img(Sprite_Objects_addr) + k * scena_sc7::at::kSpriteStride); }

// A table of code pointers in the exe's data, read as the original reads it:
// afresh, and indexed without a bound.
std::uint32_t Entry(std::uint32_t table, int index) {
    return static_cast<std::uint32_t>(Long(At(table + 4u * static_cast<std::uint32_t>(index))));
}

// The flag row, the dword 0x929ED0 read afresh at each call.
unsigned char* Bank() { return reinterpret_cast<unsigned char*>(Chapter_FlagRow); }

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

// The file loaded (a load already started): polled with a frame's sleep.
void WaitFile() {
    if (SH_CALL(File_LoadDone)() != 0) return;
    do SH_CALL(Task_Sleep)(1);
    while (SH_CALL(File_LoadDone)() == 0);
}

void PlaceParty(std::int32_t x, std::int32_t z, unsigned k) { SH_AT(void (__cdecl*)(std::int32_t, std::int32_t, unsigned), scena_sc7::kPlaceParty.v)(x, z, k); }

void PartyRestore() { SH_AT(void (__cdecl*)(), scena_sc7::kPartyRestore.v)(); }

void StatusBit80() { SH_AT(void (__cdecl*)(), scena_sc7::kStatusBit80.v)(); }

unsigned char CellFind(std::uint32_t records, unsigned n, int x, int z) {
    return static_cast<unsigned char>(SH_AT(std::uint32_t (__cdecl*)(std::uint32_t, unsigned, int, int), scena_sc7::kCellFind.v)(records, n, x, z));
}

std::uint32_t SpriteFindFree() { return SH_AT(std::uint32_t (__cdecl*)(), scena_sc7::kSpriteFindFree.v)(); }

void MusicStop() { SH_AT(void (__cdecl*)(), scena_sc7::kMusicStop.v)(); }

void KeyItemAdd(unsigned id) { SH_AT(void (__cdecl*)(unsigned), scena_sc7::kKeyItemAdd.v)(id); }

unsigned char* EffectAt(unsigned slot) { return At(::bof3::Img(Effect_Objects_addr) + ((slot & 0xFFu) << 7)); }

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
unsigned char* Slot850() { return EffectAt(static_cast<std::uint32_t>(scena_sc7::at::kSlot.d_00) & 0xFF); }

// Object handlers 0..2's end: the object set to state 4 with +0x84 = 2,
// +0x83 = the animation, +0x8A = 0.
void ObjectTo4(unsigned char* object, unsigned animation) {
    object[1] = 4;
    object[0x84] = 2;
    object[0x83] = static_cast<unsigned char>(animation);
    rv::As<records::SpriteObject>(object)->script_position = 0;
}

// Object handlers 3 and 4: the counters cleared, run 7 at `step`, or at 0x14
// when chapter 6's flag `flag` is set.
void ObjectReward(unsigned flag, unsigned step) {
    Set40();
    C0() = 0;
    Byte(::bof3::Img(scena_sc7::at::kCounter1)) = 0;
    Byte(::bof3::Img(scena_sc7::at::kCounter2)) = 0;
    Byte(::bof3::Img(scena_sc7::at::kCounter3)) = 0;
    const bool had = SH_CALL(Flags_Test)(At(::bof3::Img(Cond_Flags_addr) + 0x30), flag) != 0;
    SetStep(step);
    if (had) SetStep(0x14);
    SetRun(7);
}

}  // namespace

// original 0x54F080: slot 0 of Scena07_Hooks, Field_ModeDispatch's call - a
// tail jump through Scena07_States 0x6611CC on the s8 0x8034E2, unchecked.
extern "C" void __cdecl Scena07_Frame(void) {
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(Entry(::bof3::Img(Scena07_States_addr), static_cast<signed char>(MoveScript_PartyRecords[0x62]))))();
}

// original 0x54F090: state 0, the chapter's start - chapter 7's flag row
// 0x903FC8 cleared and state 1.
extern "C" void __cdecl Scena07_Start(void) {
    SetLong(At(::bof3::Img(Cond_Flags_addr) + 0x38), 0);
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
        Sprite_Kind2.height_whole = 0xD80;
        SH_CALL(MapView_SetElevation)(0xD80);
    }
    if (Area() == 0x4A) {
        if (Cond_ByteFD != 1) {
            SetState(2);
            return;
        }
        if (!Flag(4)) {
            SetFlag(4);
            Kind2At(0x5B8000, 0x98000);
            SH_CALL(Field_ViewReset)();
            DropIn(0);
            SH_CALL(Music_FadeOut)(0x20);
            Music_Track = 1;
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
    if (Area() == 0x69) SH_CALL(Flags_Set)(At(::bof3::Img(Cond_Flags_addr) + 0xa0), 0x4F);
    if (Area() == 0xAF && Cond_ByteFD == 1) {
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
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(Entry(::bof3::Img(Scena07_Runs_addr), MoveScript_Var7)))();
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
        Field_StatusBits = static_cast<unsigned char>(Field_StatusBits | 1);
        SH_CALL(Scenario_CallA)(0);
        ChangeArea(0xAF, 0x240000, 0x1E0000, 0x81);
        ModeFlow_AreaTransition = 0xFF;
        Area_EnterTransition = 0xFF;
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
        if (TakeInto(::bof3::Img(scena_sc7::at::kSlot_addr)) == 0xFF) return;
        SetStep(0xA);
        Effect13(Slot850(), Camera_Angles[0], Camera_Angles[1], 0x31E, 0xE0);
        return;
    case 0xA:
        if (C0() != 4) return;
        if (Requested()) return;
        SH_CALL(Music_Play)(0x60, 8);
        SetStep(0xB);
        return;
    case 0xB:
        if (C0() != 5) return;
        if (TakeInto(::bof3::Img(scena_sc7::at::kSlot_addr)) == 0xFF) return;
        SetStep(0xC);
        Effect13(Slot850(), Camera_Angles[0], Camera_Angles[1], 0x200, 0x70);
        return;
    case 0xC:
        if (C0() != 0xA) return;
        SH_CALL(Music_FadeOutStop)(8);
        SH_CALL(Music_Play)(0x63, 8);
        SetStep(0xD);
        return;
    case 0xD:
        if (C0() != 0xB) return;
        if (TakeInto(::bof3::Img(scena_sc7::at::kSlot_addr)) == 0xFF) return;
        SetStep(0xE);
        Effect13(Slot850(), -0x35A, Camera_Angles[1], 0x48, 0x30);
        SetTimer(0x30);
        return;
    case 0xE: {
        if (C0() != 0xC) return;
        if (TakeInto(::bof3::Img(scena_sc7::at::kSlot_addr)) == 0xFF) return;
        SetStep(0xF);
        unsigned char* const e = Slot850();
        Live(e, 0x31);
        SetLong(e + 0x64, Camera_Angles[0]);
        SetLong(e + 0x68, Camera_Angles[1]);
        SetLong(e + 0x6C, S16(::bof3::Img(scena_sc7::at::kPitch)));
        rv::As<records::EffectObject>(e)->step_x = -0x380;
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
        Camera_Angles[1] = 0;
        Camera_Distance = 0;
        PassFlags(0x1F);
        Camera_Angles[0] = 0xFD56;
        SetWord(At(::bof3::Img(Cond_AngleFB_addr)), 0x200);
        MapView_Redraw = 2;
        SetStep(0x12);
        return;
    case 0x12:
        if (!WaitDone()) return;
        SetStep(0x13);
        IncC0();
        return;
    case 0x13:
        if (C0() != 0x22) return;
        if (TakeInto(::bof3::Img(scena_sc7::at::kSlot_addr)) == 0xFF) return;
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
        Field_StatusBits = static_cast<unsigned char>(Field_StatusBits & 0xFE);
        SetFlag(2);
        ChangeArea(0xAF, 0x2D8000, 0x5E0000, 0x83);
        Area_EnterTransition = 0xFF;
        ModeFlow_AreaTransition = 0xFF;
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
        rv::Bytes(ObjTrio)[0xb] = slot;
        if (slot == 0xFF) return;
        unsigned char* const e = EffectAt(slot);
        const unsigned arg = Byte(::bof3::Img(scena_sc7::at::kEffectArg) + ObjTrio[0].char_id);
        e[0] = 1;
        e[5] = 6;
        e[6] = 1;
        rv::As<records::EffectObject>(e)->step_x = 0;
        rv::As<records::EffectObject>(e)->step_z = static_cast<std::int32_t>(arg);
        rv::As<records::EffectObject>(e)->screen_x = static_cast<std::uint16_t>(ObjTrio[0].screen_x);
        SetStep(1);
        rv::As<records::EffectObject>(e)->screen_y = static_cast<std::uint16_t>(ObjTrio[0].screen_y);
        return;
    }
    case 1:
        if (EffectAt(rv::Bytes(ObjTrio)[0xb])[0] & 1) return;
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
        if (TakeInto(::bof3::Img(scena_sc7::at::kSlot_addr)) == 0xFF) return;
        unsigned char* const e = Slot850();
        SetTimer(0x8C);
        SetStep(5);
        Live(e, 0x3B);
        rv::As<records::EffectObject>(e)->x = rv::As<records::SpriteObject>(Sprite(0))->x;
        rv::As<records::EffectObject>(e)->z = rv::As<records::SpriteObject>(Sprite(0))->z;
        rv::As<records::EffectObject>(e)->height = static_cast<std::int32_t>(static_cast<std::uint32_t>(rv::As<records::SpriteObject>(Sprite(0))->height) + 0x1000000u);
        return;
    }
    case 5:
        if (Timer() != 0) {
            DecTimer();
            return;
        }
        if (TakeInto(::bof3::Img(scena_sc7::at::kSlot_addr)) == 0xFF) return;
        Effect13(Slot850(), -0x33E, Camera_Angles[1], S16(::bof3::Img(scena_sc7::at::kPitch)), 0xA0);
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
        if (C0() == 0xE && TakeInto(::bof3::Img(scena_sc7::at::kSlot_addr)) != 0xFF) {
            SetStep(9);
            Effect13(Slot850(), -0x2AA, Camera_Angles[1], S16(::bof3::Img(scena_sc7::at::kPitch)), 0x90);
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

// original 0x54FE90: run 4's timed effects - for each of the 12 records of
// Scena07_TimedEffects 0x6611FC whose s16 frame equals the timer (read afresh
// each record): an effect of kind 0x18 at the record's x, z (+0x36, +0x3A)
// and sound 0x202 + record % 3; stops at the first record no slot is free for.
extern "C" void __cdecl Scena07_TimedEffects(void) {
    for (unsigned i = 0; i < 12; ++i) {
        const std::uint32_t rec = ::bof3::Img(Scena07_TimedRecords_addr) + 6 * i;
        if (static_cast<int>(S16(rec + 4)) != static_cast<int>(Timer())) continue;
        const unsigned char slot = SH_CALL(Effect_FindFree)();
        if (slot == 0xFF) return;
        unsigned char* const e = EffectAt(slot);
        e[0] = 1;
        e[5] = 0x18;
        e[0xB] = 0x18;
        rv::Store<unsigned>(rv::As<records::SpriteHead>(e)->x_whole, Word(At(rec)));
        rv::Store<unsigned>(rv::As<records::SpriteHead>(e)->z_whole, Word(At(rec + 2)));
        Sound(0x202 + i % 3);
    }
}

// original 0x54FF40: run 5, steps 0..2 (a chain of decrements): the change to
// area 0x35 with track 0x4D; on counter 0 = 9 flag 7 and the change to area
// 0x58; the run over.
extern "C" void __cdecl Scena07_Scene5(void) {
    switch (Step()) {
    case 0:
        ChangeArea(0x35, 0x120000, 0x180000, 1);
        AreaW1f_MusicCurrent = 0x4D;
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
        if (!Requested() || Word(At(::bof3::Img(scena_sc7::at::kMessage))) != 2) break;
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
        rv::As<records::EffectObject>(p)->x = 0x160000;
        rv::As<records::EffectObject>(p)->z = 0x3E0000;
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
        rv::As<records::EffectObject>(p)->x = 0x360000;
        rv::As<records::EffectObject>(p)->z = 0x410000;
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
        rv::As<records::EffectObject>(p)->step_x = 0x270000;
        const std::int32_t x = rv::As<records::EffectObject>(p)->step_x;
        rv::As<records::EffectObject>(p)->x = 0x390000;
        rv::As<records::EffectObject>(p)->z = 0x410000;
        rv::As<records::EffectObject>(p)->step_z = 0x410000;
        const long e = SH_CALL(AreaMap_Elevation)(x, 0x410000);
        rv::As<records::EffectObject>(p)->height = static_cast<std::int32_t>(static_cast<std::uint32_t>(static_cast<int>(static_cast<short>(e))) << 16);
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
        const std::int32_t z = ObjTrio[0].z;
        const std::int32_t x = ObjTrio[0].x;
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
        Area_EnterTransition = 5;
        ModeFlow_AreaTransition = 4;
        SetStep(0x15);
        out = kDone;
        break;
    case 0x15:
        if (C0() != 0x14) break;
        if (TakeInto(::bof3::Img(scena_sc7::at::kSlot_addr)) == 0xFF) break;
        SetStep(0x16);
        Live(Slot850(), 0x46);
        out = kDone;
        break;
    case 0x16: {
        if (C0() != 0x15) break;
        unsigned char* const p = SH_CALL(Scena07_TakeEffect49)(9);
        p[6] = 0;
        rv::As<records::EffectObject>(p)->x = 0x170000;
        rv::As<records::EffectObject>(p)->z = 0x3F0000;
        rv::As<records::EffectObject>(p)->height = 0x5C00000;
        rv::As<records::EffectObject>(p)->step_x = 0x10000;
        rv::As<records::EffectObject>(p)->step_z = 0;
        rv::Store<std::int32_t>(rv::As<records::EffectObject>(p)->step_y, static_cast<std::int32_t>(0xFFE00000u));
        rv::Store<std::int32_t>(rv::As<records::EffectObject>(p)->work_18, 0x2D0000);
        rv::Store<std::int32_t>(rv::As<records::EffectObject>(p)->work_1c, 0x3F0000);
        rv::Store<std::int32_t>(rv::As<records::EffectObject>(p)->work_20, 0x3000000);
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
        rv::As<records::EffectObject>(p)->z = 0x410000;
        rv::Store<std::int32_t>(rv::As<records::EffectObject>(p)->work_1c, 0x410000);
        p[6] = 1;
        rv::As<records::EffectObject>(p)->x = 0x2D0000;
        rv::As<records::EffectObject>(p)->height = 0x3000000;
        rv::As<records::EffectObject>(p)->step_x = static_cast<std::int32_t>(0xFFFF0000u);
        rv::As<records::EffectObject>(p)->step_z = 0;
        rv::Store<std::int32_t>(rv::As<records::EffectObject>(p)->step_y, 0);
        rv::Store<std::int32_t>(rv::As<records::EffectObject>(p)->work_18, 0x270000);
        rv::Store<std::int32_t>(rv::As<records::EffectObject>(p)->work_20, 0x3000000);
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
        rv::As<records::EffectObject>(p)->x = 0x258000;
        rv::As<records::EffectObject>(p)->z = 0x410000;
        unsigned char* const q = SH_CALL(Scena07_TakeEffect49)(4);
        rv::As<records::EffectObject>(q)->x = 0x258000;
        rv::As<records::EffectObject>(q)->z = 0x410000;
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
        rv::As<records::EffectObject>(p)->x = 0x258000;
        rv::As<records::EffectObject>(p)->z = 0x410000;
        unsigned char* const q = SH_CALL(Scena07_TakeEffect49)(8);
        rv::As<records::EffectObject>(q)->x = 0x258000;
        rv::As<records::EffectObject>(q)->z = 0x410000;
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
        rv::As<records::EffectObject>(p)->x = 0x258000;
        rv::As<records::EffectObject>(p)->z = 0x410000;
        unsigned char* const q = SH_CALL(Scena07_TakeEffect49)(7);
        rv::As<records::EffectObject>(q)->x = 0x258000;
        rv::As<records::EffectObject>(q)->z = 0x410000;
        SetTimer(0x3C);
        SetStep(0x24);
        out = kShake;
        break;
    }
    case 0x24:
        if (C0() != 0x25) break;
        if (TakeInto(::bof3::Img(scena_sc7::at::kSlot_addr)) == 0xFF) break;
        SetStep(0x25);
        Effect13(Slot850(), -0x370, Camera_Angles[1], 0xB6, 0xA8);
        out = kShake;
        break;
    case 0x25:
        if (C0() != 0x26) break;
        if (TakeInto(::bof3::Img(scena_sc7::at::kSlot_addr)) == 0xFF) break;
        SetStep(0x26);
        Effect13(Slot850(), -0x2AA, Camera_Angles[1], 0x200, 0x1E);
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
        ModeFlow_AreaTransition = 4;
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

// original 0x550890: the camera shaken - MapView_Redraw 2 and Camera_ShiftY
// (u16) += 4 * Scena07_Shake 0x661244 [Frame_Counter & 3] (s8).
extern "C" void __cdecl Scena07_ShakeCamera(void) {
    MapView_Redraw = 2;
    const std::uint32_t k = Frame_Counter & 3;
    const unsigned d = static_cast<unsigned>(static_cast<int>(static_cast<signed char>(Byte(::bof3::Img(scena_sc7::at::kShake7) + k)))) << 2;
    Camera_ShiftY = static_cast<std::uint16_t>(Camera_ShiftY) + d;
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
        scena_sc7::at::kSlot.w_00 = slot;
        if (slot == 0xFF) continue;
        SH_CALL(EventOp_0x)(At(::bof3::Img(Scena07_PlacedObjects_addr) + 0x11 * i));
        rv::Store<unsigned>(rv::As<records::SpriteHead>(SpriteCurrent())->height_whole, 0xA00);
        const unsigned char seat = Byte(::bof3::Img(scena_sc7::at::kPlacedSeat7) + i);
        if (seat == 0) continue;
        ActiveMember()[0x81] = seat;
        unsigned char* const cur = SpriteCurrent();
        cur[3] = cur[1];
        SpriteCurrent()[1] = 5;
    }
}

// DIVERGENCE DIV-0210: 1 through PatchBytes("GiftChestRow") in Scena072_Inject,
// after the self-test. Run 7 then records its two gifts in chapter 6's row,
// the row ObjectReward and area 94's placement script test in chapter 7,
// not in the running row, which nothing reads (D410).
namespace scena_sc7 { std::uint8_t g_gift_chest_row = 0; }

// original 0x550930: run 7, steps 0..0x15 (a byte table of 0x16 at 0x550B08
// into a jump table of 7 at 0x550AEC): two item rewards - counter 0 = 0x1E /
// 0x23 armed; on 0x1F / 0x24 Inventory_Add of item [0x903EFE] + 1 (category
// 1) / [0x903F01] + 1 (category 2); a full inventory ends the run (counter 0
// = 0x20 / 0x25), else the item's name copied to Text_Records +0 / +0x20,
// message 0x94 / 0x95, flag 0x22 / 0x23, sound 0x106, step 0x15; step 0x14
// system message 1; step 0x15 on the message's close the run over.
extern "C" void __cdecl Scena07_Scene7(void) {
    auto finish = [] {
        Byte(::bof3::Img(scena_sc7::at::kCounter1)) = 0;
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
        if (scena_sc7::g_gift_chest_row != 0) SH_CALL(Flags_Set)(&Cond_Flags[0x30], flag);   // DIV-0210: chapter 6's row
        else SetFlag(flag);
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
        reward(::bof3::Img(CharacterRecords_addr) + 7 * scena_sc7::at::kRecordStride + 0x12, 1, 0x20, ::bof3::Img(scena_sc7::at::kTextRecords), 0x94, 0x22);
        return;
    case 0xA:
        if (Requested()) return;
        C0() = 0x23;
        SetStep(0xC);
        return;
    case 0xC:
        if (C0() != 0x24) return;
        reward(::bof3::Img(CharacterRecords_addr) + 7 * scena_sc7::at::kRecordStride + 0x15, 2, 0x25, ::bof3::Img(scena_sc7::at::kTextRecords) + 0x20, 0x95, 0x23);
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
    reinterpret_cast<ObjectHandler>(static_cast<std::uintptr_t>(Entry(::bof3::Img(Scena07_ObjectHandlers_addr), static_cast<int>(index))))(object, bank);
}

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
            const unsigned char kind = ObjTrio[0].direction;
            scena_sc7::at::kSlot.b_00 = kind;
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
            Byte(::bof3::Img(scena_sc7::at::kScriptFlagsHi)) = static_cast<unsigned char>(Byte(::bof3::Img(scena_sc7::at::kScriptFlagsHi)) | 1);
            Request(2);
            SetRun(6);
            SetStep(5);
            return 1;
        }
    }
    const unsigned area = Area();
    if (area == 0x69) {
        const unsigned char kind = ObjTrio[0].direction;
        scena_sc7::at::kSlot.b_00 = kind;
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

// original 0x550F80: 1 in al when a party member (of Field_MemberCount, a
// byte) has +0x89 = 2, else 0. Chapter 7's step hook calls it, and chapter
// 10's code at 0x55BBB1.
extern "C" unsigned char __cdecl Scena07_PartyHas89State2(void) {
    const unsigned n = Field_MemberCount;
    for (unsigned i = 0; i < n; ++i)
        if (Byte(::bof3::Img(scena_sc7::at::kMembers) + i * scena_sc7::at::kMemberStride + 0x89) == 2) return 1;
    return 0;
}

// original 0x550FD0: slot 4, the cell hook (x, z) - 0x56D800 over the one
// record Scena07_Cells 0x66135C; none (a negative answer): 0xFF in al (the
// answer's upper bytes kept); else the handler Scena07_CellHandlers 0x661364
// [answer] (read in place, unchecked) and 1.
extern "C" unsigned char __cdecl Scena07_CellHook(int x, int z) {
    const unsigned char r = CellFind(::bof3::Img(Scena07_Cells_addr), 1, x, z);
    if (static_cast<signed char>(r) < 0) return 0xFF;
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(Entry(::bof3::Img(Scena07_CellHandlers_addr), static_cast<signed char>(r))))();
    return 1;
}

// original 0x551000: the cell's handler - run 6 at step 0xA.
extern "C" void __cdecl Scena07_Cell0(void) {
    Set40();
    SetRun(6);
    SetStep(0xA);
}

void Scena072_Inject() {
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
    {
        // DIV-0210: after the self-test, which compared Capcom's row.
        const std::uint8_t was = 0, is = 1;
        bof3::PatchBytes("GiftChestRow", Low32(&scena_sc7::g_gift_chest_row), &was, &is, 1);
        bof3::Log("DIV-0210    chapter 7 records the two gift chests in chapter 6's row (on unless the line above says OFF)");
    }
}

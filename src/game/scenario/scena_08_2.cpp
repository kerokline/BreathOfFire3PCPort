// Moved here by tools/move_funcs.py from scena_sc7.cpp (docs/source-layout-round.md).
#include "game/scenario/scena_08_2.h"

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

void SetTimer(unsigned v) { SetWord(At(::bof3::Img(MoveScript_PartyRecords_addr) + 0x66), v); }

// `dec word [timer]` then the flags of the result: the timer after.
unsigned DecTimer() {
    SetWord(At(::bof3::Img(MoveScript_PartyRecords_addr) + 0x66), Word(At(::bof3::Img(MoveScript_PartyRecords_addr) + 0x66)) - 1u);
    return Word(At(::bof3::Img(MoveScript_PartyRecords_addr) + 0x66));
}

// `mov ax, [timer]; mov cx, ax; dec ax; test cx, cx; mov [timer], ax`: the
// timer before (every chapter 8 wait of this shape stores the decrement
// first, so a timer of 0 wraps to 0xFFFF on the way out).
unsigned PostDecTimer() {
    const unsigned t = Word(At(::bof3::Img(MoveScript_PartyRecords_addr) + 0x66));
    SetWord(At(::bof3::Img(MoveScript_PartyRecords_addr) + 0x66), t - 1u);
    return t;
}

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

unsigned char* Sprite(std::uint32_t k) { return At(::bof3::Img(Sprite_Objects_addr) + k * scena_sc7::at::kSpriteStride); }

unsigned char* Record(std::uint32_t k) { return At(::bof3::Img(CharacterRecords_addr) + k * scena_sc7::at::kRecordStride); }

// Chapter 8 run 11's character records: by Character_IndexById [7] (a
// byte) and [4] (the dword at [4] read, its low byte).
unsigned char* RecordOfMember7() { return Record(Character_IndexById[7]); }

unsigned char* RecordOfMember4() { return Record(static_cast<std::uint32_t>(Long(At(::bof3::Img(Character_IndexById_addr) + 4))) & 0xFF); }

// The sprite 0x903804 points at, read afresh.
unsigned char* FocusObject() {
    return reinterpret_cast<unsigned char*>(AreaW1a_FocusObject);
}

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

void PlaceParty(std::int32_t x, std::int32_t z, unsigned k) { SH_AT(void (__cdecl*)(std::int32_t, std::int32_t, unsigned), scena_sc7::kPlaceParty.v)(x, z, k); }

void PartyRestore() { SH_AT(void (__cdecl*)(), scena_sc7::kPartyRestore.v)(); }

void StatusBit80() { SH_AT(void (__cdecl*)(), scena_sc7::kStatusBit80.v)(); }

std::uint32_t SpriteFindFree() { return SH_AT(std::uint32_t (__cdecl*)(), scena_sc7::kSpriteFindFree.v)(); }

void MusicStop() { SH_AT(void (__cdecl*)(), scena_sc7::kMusicStop.v)(); }

void Call591BE0(unsigned a, unsigned b) { SH_AT(void (__cdecl*)(unsigned, unsigned), scena_sc7::kCall591BE0.v)(a, b); }

void Call498DE0(unsigned a) { SH_AT(void (__cdecl*)(unsigned), scena_sc7::kCall498DE0.v)(a); }

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

}  // namespace

// original 0x551020: slot 0 of Scena08_Hooks, Field_ModeDispatch's call - a
// tail jump through Scena08_States 0x66137C on the s8 0x8034E2, unchecked.
extern "C" void __cdecl Scena08_Frame(void) {
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(Entry(::bof3::Img(Scena08_States_addr), static_cast<signed char>(MoveScript_PartyRecords[0x62]))))();
}

// original 0x551030: state 0, the chapter's start - state 1, chapter 8's flag
// row 0x903FD0 and the four counters cleared, record 0 alone given bit 0, key
// item 4 swapped for 0xE, story flag 0x43.
extern "C" void __cdecl Scena08_Start(void) {
    SetState(1);
    SetLong(At(::bof3::Img(Cond_Flags_addr) + 0x40), 0);
    SetLong(At(::bof3::Img(scena_sc7::at::kCounters)), 0);
    SH_CALL(Scena08_PartyBitsToRecord0)();
    SH_CALL(Scena08_SwapKeyItem4)();
    SH_CALL(Flags_Set)(At(::bof3::Img(Cond_Flags_addr) + 0xa0), 0x43);
}

// original 0x551060: bit 0 of +0xB set on the eight CharacterRecords
// (0x903A70, stride 0xA4), the party refreshed (0x533E50), then bit 0
// cleared on records 1..7 - record 0 alone keeps it.
extern "C" void __cdecl Scena08_PartyBitsToRecord0(void) {
    for (std::uint32_t i = 0; i < 8; ++i) Record(i)[0xB] = static_cast<unsigned char>(Record(i)[0xB] | 1);
    PartyRestore();
    for (std::uint32_t i = 1; i < 8; ++i) Record(i)[0xB] = static_cast<unsigned char>(Record(i)[0xB] & 0xFE);
}

// original 0x5510A0: state 1, once per area entered. By the area number (read
// afresh at each test), the flags and Cond_ByteFD: drop-ins, scenario call A,
// the kind-2 sprite and the view, sprites 0 and 1 set up, an effect of kind
// 0x13, member 1's +0 bit 0x40, a kept area change (0x6BC720), map cells, the
// music loaded, a scene armed (run and step); then state 2 on every way out.
extern "C" void __cdecl Scena08_EnterArea(void) {
    if (Area() == 0) {
        if (Cond_ByteFD != 0) {
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
        if (Cond_ByteFD != 2) {
            SetState(2);
            return;
        }
        if (!Flag(6)) {
            Set40();
            DropIn(3);
            // x 0x518000 is a 16.16 coordinate (cell 0x51.8), not a function:
            // its value only equals Field_ObjectInHome's address, so it stays raw
            // (docs/round-10-cleanup.md item 1).
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
        const unsigned char slot = TakeInto(::bof3::Img(scena_sc7::at::kSlot_addr));
        Camera_Angles[0] = 0xFDC4;
        if (slot != 0xFF) Effect13(Slot850(), -0x302, Camera_Angles[1], S16(::bof3::Img(scena_sc7::at::kPitch)), 0xFF);
    }
    if (Area() == 3 && !Flag(0xE)) {
        SH_CALL(Scenario_CallA)(3);
        ObjTrio[1].flags = static_cast<unsigned char>(ObjTrio[1].flags | 0x40);
    }
    if (Area() == 0xC && !Flag(0xC) && Flag(0xB)) {
        Set40();
        const unsigned char kind = ObjTrio[0].direction;
        const std::int32_t x = ObjTrio[0].x;
        const std::int32_t z = ObjTrio[0].z;
        PassFlags(0);
        SetRun(8);
        SetStep(0);
        Scena08_Kept[0] = kind;
        SetLong(At(::bof3::Img(Scena08_Kept_addr) + 0x4), x);
        SetLong(At(::bof3::Img(Scena08_Kept_addr) + 0x8), z);
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
        if (Cond_ByteFD != 0) {
            SetState(2);
            return;
        }
        Byte(::bof3::Img(scena_sc7::at::kCounter3)) = 0;
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
                SH_CALL(Flags_Set)(bank, Low32(bank) & 0xFF);
                SetRun(0xA);
                SetStep(0xA);
            }
        }
    }
    if (Area() != 0x20) {
        SetState(2);
        return;
    }
    if (Cond_ByteFD == 5 && !Flag(0)) {
        Set40();
        PassFlags(0);
        DropIn(0);
        SetRun(1);
        SetStep(0);
        SetTimer(0x96);
    }
    if (Cond_ByteFD == 1) {
        if (!Flag(0)) {
            DropIn(1);
            PassFlags(0x1F);
        } else if (!Flag(1)) {
            SH_CALL(Scenario_CallA)(1);
            DropIn(2);
            PassFlags(0);
        }
    }
    if (Cond_ByteFD == 0 && !Flag(2)) {
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
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(Entry(::bof3::Img(Scena08_Runs_addr), MoveScript_Var7)))();
}

// original 0x551590: run 1, steps 0..0xC (a jump table of 13 after it at
// 0x5518A0): a timed music start (step 0 runs on into step 1's wait), counter
// 0 = 1, effects of kind 0x52 on counter 4 and 7, a transition, the music
// faded, two character records set (by Character_IndexById [7] and [8])
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
        if (TakeInto(::bof3::Img(scena_sc7::at::kSlot_addr)) != 0xFF) {
            unsigned char* const e = Slot850();
            rv::As<records::EffectObject>(e)->z = 0x550000;
            const std::int32_t z = rv::As<records::EffectObject>(e)->z;
            rv::As<records::EffectObject>(e)->x = 0x8E0000;
            const std::int32_t x = rv::As<records::EffectObject>(e)->x;
            e[0] = 1;
            e[5] = 0x52;
            e[1] = 0;
            const long h = SH_CALL(AreaMap_Elevation)(x, z);
            rv::As<records::EffectObject>(e)->height = static_cast<std::int32_t>(static_cast<std::uint32_t>(static_cast<int>(static_cast<short>(h))) << 16);
        }
        SetTimer(0x3C);
        SetStep(3);
        return;
    }
    case 3:
        if (C0() != 7) return;
        if (TakeInto(::bof3::Img(scena_sc7::at::kSlot_addr)) != 0xFF) {
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
        unsigned char* const a = Record(Character_IndexById[7]);
        a[0x09] = 7;
        a[0x58] = 0x37;
        a[0x5B] = 8;
        a[0x5C] = 0x64;
        unsigned char* const b = Record(Character_IndexById[8]);
        b[0x09] = 8;
        b[0x5B] = 0xC;
        SH_CALL(Scenario_CallA)(0);
        ChangeArea(0x20, 0x650000, 0x80000, 1);
        SetStep(7);
        AreaW1f_MusicCurrent = 0xFF;
        return;
    }
    case 7: {
        if (C0() != 0x16) return;
        ChangeArea(0x20, 0x640000, 0x120000, 1);
        unsigned char* const bank = Bank();
        AreaW1f_MusicCurrent = 0xFF;
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
        Sprite_ObjectsExtra[0].area_script = 3;
        Sprite_ObjectsExtra[0].script_position = 0;
        SetStep(1);
        return;
    case 1:
        if (Byte(::bof3::Img(scena_sc7::at::kCounter3)) != 0x12) return;
        SetTimer(0x10);
        SetStep(2);
        return;
    case 2:
        if (PostDecTimer() != 0) return;
        SetFlag(3);
        SetWord(At(::bof3::Img(ScenaSc7_DrawPool_addr) + 6), 1);
        Byte(::bof3::Img(scena_sc7::at::kDrawPool) + 3) = 5;
        Byte(::bof3::Img(scena_sc7::at::kDrawPool) + 4) = 3;
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
        SetLong(At(::bof3::Img(scena_sc7::at::kCounters)), 0);
        DropIn(6);
        const unsigned speed = Field_MoveSpeeds[4];
        Kind2At(ObjTrio[0].x, ObjTrio[0].z);
        MoveScript_F3Divisor = speed << 3;
        SetStep(1);
        return;
    }
    case 1: {
        if (Field_Kind2Hold != 0) return;
        const std::int32_t z = Field_Kind2Z;
        const std::int32_t x = Field_Kind2X;
        Sprite_Kind2.x = x;
        Sprite_Kind2.z = z;
        const long h = SH_CALL(AreaMap_Elevation)(x, z);
        Sprite_Kind2.height_whole = static_cast<unsigned>(h);
        SetElevationAt(Field_Kind2X, Field_Kind2Z);
        C0() = 1;
        SetStep(2);
        return;
    }
    case 2:
        if (C0() != 5) return;
        if (TakeInto(::bof3::Img(scena_sc7::at::kSlot_addr)) != 0xFF) {
            unsigned char* const e = Slot850();
            rv::As<records::EffectObject>(e)->z = 0x418000;
            const std::int32_t z = rv::As<records::EffectObject>(e)->z;
            rv::As<records::EffectObject>(e)->x = 0x158000;
            const std::int32_t x = rv::As<records::EffectObject>(e)->x;
            e[0] = 1;
            e[5] = 0x51;
            e[1] = 0;
            const long h = SH_CALL(AreaMap_Elevation)(x, z);
            rv::As<records::EffectObject>(e)->height = static_cast<std::int32_t>(static_cast<std::uint32_t>(static_cast<int>(static_cast<short>(h))) << 16);
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
        if (TakeInto(::bof3::Img(scena_sc7::at::kSlot_addr)) != 0xFF) Live(Slot850(), 0x63);
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
        SH_CALL(Flags_Set)(At(::bof3::Img(Gene_Bits_addr)), 3);
        SH_CALL(Flags_Set)(At(::bof3::Img(Gene_Bits_addr)), 0x10);
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
        if ((Sprite(Byte(::bof3::Img(scena_sc7::at::kCounter3)))[0] & 0x80) == 0) return;
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
        AreaW1f_MusicCurrent = 0xFF;
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
            Area_EnterTransition = 0xFF;
            AreaW1f_MusicCurrent = 0xFF;
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
        Cond_ByteFE = 1;
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
        const unsigned char slot = TakeInto(::bof3::Img(Scena08_Kept_addr) + 0xf);
        if (slot != 0xFF) Effect13(EffectAt(slot), -0x318, Camera_Angles[1], S16(::bof3::Img(scena_sc7::at::kPitch)), 0x1E);
        SetStep(3);
        return;
    }
    case 3: {
        if (EffectAt(Scena08_Kept[0xf])[0] & 1) return;
        if (TakeInto(::bof3::Img(scena_sc7::at::kSlot_addr)) != 0xFF) {
            unsigned char* const e = Slot850();
            e[0] = 1;
            e[5] = 0x5F;
            e[1] = 0;
        }
        const unsigned char slot = TakeInto(::bof3::Img(Scena08_Kept_addr) + 0xf);
        if (slot != 0xFF) Effect13(EffectAt(slot), Camera_Angles[0], Camera_Angles[1], 0x1C, 0xB4);
        IncC0();
        SetStep(4);
        return;
    }
    case 4: {
        if (EffectAt(Scena08_Kept[0xf])[0] & 1) return;
        const unsigned char slot = TakeInto(::bof3::Img(Scena08_Kept_addr) + 0xf);
        if (slot != 0xFF) Effect13(EffectAt(slot), -0x2AA, 0, 0x200, 0x5A);
        IncC0();
        PlaceParty(0x430000, 0x380000, 0x21);
        DropIn(1);
        SetStep(5);
        return;
    }
    case 5:
        if (EffectAt(Scena08_Kept[0xf])[0] & 1) return;
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
        rv::As<records::SpriteObject>(FocusObject())->work_18 = 1;
        rv::As<records::SpriteObject>(FocusObject())->work_1c = 3;
        Clear40();
        SetRun(0);
        SetStep(0);
        return;
    default:   // 2..9 and past 0xF
        return;
    }
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
        Area_EnterTransition = 0xFF;
        ModeFlow_AreaTransition = 0xFF;
        AreaW1f_MusicCurrent = 0xFF;
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
        const unsigned char slot = TakeInto(::bof3::Img(Scena08_Kept_addr) + 0xf);
        if (slot != 0xFF) Effect13(EffectAt(slot), -0x44C, Camera_Angles[1], 0x124, 0x40);
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
        const unsigned flags = Scena08_Kept[0];
        const std::int32_t z = Long(At(::bof3::Img(Scena08_Kept_addr) + 0x8));
        const std::int32_t x = Long(At(::bof3::Img(Scena08_Kept_addr) + 0x4));
        ChangeArea(0xC, x, z, flags);
        ModeFlow_AreaTransition = 0xFF;
        Area_EnterTransition = 0xFF;
        SetStep(8);
        return;
    }
    case 8:
        if (!WaitDone()) return;
        if (Field_Request == 5) return;
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
        const unsigned char slot = TakeInto(::bof3::Img(Scena08_Kept_addr) + 0xf);
        if (slot == 0xFF) return;
        SetStep(0xC);
        Effect13(EffectAt(slot), Camera_Angles[0], Camera_Angles[1], 0x2B0, 0x20);
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
        const unsigned char slot = TakeInto(::bof3::Img(Scena08_Kept_addr) + 0xf);
        if (slot == 0xFF) return;
        SetStep(0xE);
        Effect13(EffectAt(slot), Camera_Angles[0], Camera_Angles[1], 0x200, 0x20);
        return;
    }
    case 0xE: {
        if (EffectAt(Scena08_Kept[0xf])[0] & 1) return;
        const unsigned char c = C0();
        ScriptFlagsOr(0x20);
        const unsigned char list5 = Cond_Flags[0xd5];
        C0() = static_cast<unsigned char>(c + 1);
        const unsigned char list6 = Cond_Flags[0xd6];
        Scena08_Kept[0xc] = list5;
        Scena08_Kept[0xd] = list6;
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
        const unsigned char slot = TakeInto(::bof3::Img(Scena08_Kept_addr) + 0xf);
        if (slot == 0xFF) return;
        SetStep(0x16);
        Effect13(EffectAt(slot), Camera_Angles[0], Camera_Angles[1], 0x13A, 0x40);
        return;
    }
    case 0x16: {
        if (C0() != 0x12) return;
        const unsigned char slot = TakeInto(::bof3::Img(Scena08_Kept_addr) + 0xf);
        if (slot == 0xFF) return;
        Effect13(EffectAt(slot), -0x2AA, Camera_Angles[1], 0x360, 0x20);
        SH_CALL(Kind2_Place)(0);
        ScriptFlagsOr(8);
        SetStep(0x17);
        return;
    }
    case 0x17: {
        if (C0() != 0x16) return;
        const unsigned char slot = TakeInto(::bof3::Img(Scena08_Kept_addr) + 0xf);
        if (slot == 0xFF) return;
        SetStep(0x18);
        Effect13(EffectAt(slot), -0x2AA, Camera_Angles[1], 0x200, 0x20);
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
        SH_CALL(Music_Play)(AreaW1f_MusicCurrent, 8);
        SetStep(0x1A);
        return;
    case 0x1A: {
        if (Word(At(::bof3::Img(scena_sc7::at::kMessage))) != 0xE) return;
        if (!Requested()) return;
        const std::uint32_t p = AreaW1a_FocusObject;
        SetLong(At(::bof3::Img(Scena08_Kept_addr) + 0x4), Long(At(p + 0x34)));
        SetLong(At(::bof3::Img(Scena08_Kept_addr) + 0x8), Long(At(p + 0x38)));
        // (p - Sprite_Objects) / 0xA4, a signed division truncated toward 0.
        Scena08_Kept[0] =
            static_cast<unsigned char>(static_cast<std::int32_t>(p - ::bof3::Img(Sprite_Objects_addr)) / static_cast<std::int32_t>(scena_sc7::at::kSpriteStride));
        DropIn(2);
        SetStep(0x1B);
        return;
    }
    case 0x1B: {
        if (C0() != 0) return;
        SetFlag(0xE);
        const std::uint32_t index = static_cast<std::uint32_t>(Long(At(::bof3::Img(Scena08_Kept_addr)))) & 0xFF;
        ScriptFlagsAnd(0xFFDF);
        Sprite(index)[0] = 0;
        SH_CALL(Scenario_CallA)(2);
        Cond_Flags[0xd5] = Scena08_Kept[0xc];
        Cond_Flags[0xd6] = Scena08_Kept[0xd];
        const std::int32_t x = Long(At(::bof3::Img(Scena08_Kept_addr) + 0x4));
        const std::int32_t z = Long(At(::bof3::Img(Scena08_Kept_addr) + 0x8));
        ObjTrio[1].x = x;
        ObjTrio[1].z = z;
        const long h = SH_CALL(AreaMap_Elevation)(x, z);
        ObjTrio[1].height_whole = static_cast<unsigned>(h);
        SetRun(0);
        SetStep(0);
        return;
    }
    default:   // 9, 0xF..0x13 and past 0x1B
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
    SetLong(At(::bof3::Img(Scena08_Kept_addr) + 0x4), static_cast<std::int32_t>(a));
    if (a == 0xFF) return;
    Sprite(a)[0] = 1;
    const std::uint32_t b = SpriteFindFree() & 0xFF;
    SetLong(At(::bof3::Img(Scena08_Kept_addr) + 0x8), static_cast<std::int32_t>(b));
    if (b == 0xFF) {
        Sprite(static_cast<std::uint32_t>(Long(At(::bof3::Img(Scena08_Kept_addr) + 0x4))))[0] = 0;
        return;
    }
    scena_sc7::at::kSlot.w_00 = Word(At(::bof3::Img(Scena08_Kept_addr) + 0x4));
    Sprite(b)[0] = 1;
    SH_CALL(EventOp_6x)(At(::bof3::Img(Scena08_PairOps_addr)));
    SH_CALL(Sprite_SetAnimationAt)(0x4C, 0);
    scena_sc7::at::kSlot.w_00 = Word(At(::bof3::Img(Scena08_Kept_addr) + 0x8));
    SH_CALL(EventOp_6x)(At(::bof3::Img(Scena08_PairOps_addr) + 0x10));
    SH_CALL(Sprite_SetAnimationAt)(0x36, 0);
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
        if (TakeInto(::bof3::Img(scena_sc7::at::kSlot_addr)) != 0xFF) {
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
        if (TakeInto(::bof3::Img(scena_sc7::at::kSlot_addr)) == 0xFF) return;
        SetStep(0xB);
        Effect13(Slot850(), -0x2EC, Camera_Angles[1], 0xB6, 0x20);
        return;
    case 0xB: {
        if (C0() != 6) return;
        const unsigned char slot = TakeInto(::bof3::Img(Scena08_Kept_addr) + 0xf);
        if (slot == 0xFF) return;
        SetStep(0xC);
        Effect13(EffectAt(slot), -0x2AA, Camera_Angles[1], 0x200, 0x20);
        return;
    }
    case 0xC:
        if (EffectAt(Scena08_Kept[0xf])[0] & 1) return;
        Clear40();
        SetFlag(0x14);
        SetRun(0);
        SetStep(0);
        return;
    default:
        return;
    }
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
        const unsigned speed = Field_MoveSpeeds[5];
        SetStep(2);
        Field_Kind2X = 0x170000;
        Sprite_Kind2.x = 0x170000;
        Field_Kind2Z = 0xF0000;
        Sprite_Kind2.z = 0xF0000;
        MoveScript_F3Divisor = speed << 3;
        return;
    }
    case 2:
        if (SH_CALL(File_LoadDone)() == 0) return;
        if (Field_Kind2Hold != 0) return;
        if (TakeInto(::bof3::Img(scena_sc7::at::kSlot_addr)) != 0xFF) {
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
        if (TakeInto(::bof3::Img(scena_sc7::at::kSlot_addr)) != 0xFF) {
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
        rv::Store<unsigned>(rv::As<records::CharRecord>(b)->hp, 0);   // a character record (Record: CharacterRecords + n * 0xA4)
        b[0x11] = static_cast<unsigned char>(b[0x11] | 0x60);
        return;
    }
    case 8:
        if (Field_Request != 0) return;
        SetStep(9);
        Request(1);
        ScenaSc6_Load10 = 1;
        return;
    case 9:
        if (Field_Request != 0) return;
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
        AreaW1f_MusicCurrent = 0xFF;
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
        if (Field_Request != 0) return;
        SH_CALL(Music_Play)(0x64, 0x20);
        PassFlags(0x1F);
        Transition(1);
        const std::int32_t z = ObjTrio[0].z;
        const std::int32_t x = ObjTrio[0].x;
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

// original 0x553630: character record 4 (0x903D00) set up - its dword +0xC
// += 10000, 0x498DE0(4), its bytes +0x12..+0x15 set, Char_RecalcStats on the
// record, Char_HealHp(4, 0, 0) and Char_HealAp(4, 0, 0).
extern "C" void __cdecl Scena08_SetUpRecord4(void) {
    auto* const chr4 = rv::As<records::CharRecord>(At(::bof3::Img(CharacterRecords_addr)) + 4 * sizeof(records::CharRecord));
    rv::Store<std::int32_t>(chr4->exp, static_cast<std::int32_t>(static_cast<std::uint32_t>(chr4->exp) + 0x2710u));
    Call498DE0(4);
    chr4->weapon = 7;
    chr4->armour[0] = 0x38;
    chr4->armour[1] = 0x21;
    chr4->armour[2] = 0xE;
    SH_CALL(Char_RecalcStats)(At(::bof3::Img(CharacterRecords_addr) + 0x290));
    SH_CALL(Char_HealHp)(4, 0, 0);
    SH_CALL(Char_HealAp)(4, 0, 0);
}

// original 0x553690: slot 1, the object trigger (0x56D6D0 with the object):
// Scena08_ObjectHandlers 0x6613D8 by the object's +0x86, unchecked, called
// with (object, the flag row). 0x56D6D0 reads no answer.
extern "C" void __cdecl Scena08_ObjectHook(unsigned char* object) {
    unsigned char* const bank = Bank();
    const unsigned index = object[0x86];
    reinterpret_cast<ObjectHandler>(static_cast<std::uintptr_t>(Entry(::bof3::Img(Scena08_ObjectHandlers_addr), static_cast<int>(index))))(object, bank);
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

// original 0x553730: every key-item byte of the 32 at 0x904554 that is 4
// becomes 0xE.
extern "C" void __cdecl Scena08_SwapKeyItem4(void) {
    for (std::uint32_t i = 0; i < 0x20; ++i)
        if (Byte(::bof3::Img(scena_sc7::at::kKeyItems) + i) == 4) Byte(::bof3::Img(scena_sc7::at::kKeyItems) + i) = 0xE;
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
            if (z <= 0x1F0000 && static_cast<unsigned char>(MoveScript_Var7) != 8) {
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
        const unsigned char kind = ObjTrio[0].direction;
        scena_sc7::at::kSlot.b_00 = kind;
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

void Scena082_Inject() {
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

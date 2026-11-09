// Moved here by tools/move_funcs.py from scena_sc3.cpp (docs/source-layout-round.md).
#include "game/scenario/scena_04_2.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scena_sc3.h"
#include "game/scena_sc3_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = scena_sc3::at;

}  // namespace

namespace {

using move_script::At;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

using Handler = void (__cdecl*)();

// The record views (game/record_views.h): rv::As<records::SpriteObject>(p)
// views a
// sprite-family pointer; it is used below +0x80 only (the sprite head every
// family shares) unless the pointer is a Sprite_Objects / Sprite_ObjectsExtra /
// Sprite_Kind2 record. rv::As<records::EffectObject>(p) is given effect
// objects (Effect_Objects).
using ObjectHandler = std::uint32_t (__cdecl*)(unsigned char* object, unsigned char* bank);

unsigned char& Byte(std::uint32_t a) { return At(a)[0]; }

unsigned Area() { return Game_AreaNumber; }

void SetStep(unsigned v) { MoveScript_PartyRecords[0x65] = static_cast<unsigned char>(v); }

void SetRun(unsigned v) { MoveScript_Var7 = static_cast<unsigned char>(v); }

unsigned Timer() { return Word(At(::bof3::Img(MoveScript_PartyRecords_addr) + 0x66)); }

void SetTimer(unsigned v) { SetWord(At(::bof3::Img(MoveScript_PartyRecords_addr) + 0x66), v); }

void DecTimer() { SetWord(At(::bof3::Img(MoveScript_PartyRecords_addr) + 0x66), Word(At(::bof3::Img(MoveScript_PartyRecords_addr) + 0x66)) - 1u); }

unsigned char& C0() { return Byte(::bof3::Img(scena_sc3::at::kCounter0)); }

unsigned char& C1() { return Byte(::bof3::Img(scena_sc3::at::kCounter1)); }

void ClearCounters() { SetLong(At(::bof3::Img(scena_sc3::at::kCounters)), 0); }

void PassFlags(unsigned v) { Draw_PassFlags = static_cast<unsigned char>(v); }

bool WaitDone() { return Transition_Busy == 0; }

bool Requested() { return Field_Request == 2; }

void ScriptFlagsOr80() { Byte(::bof3::Img(scena_sc3::at::kScriptFlags)) = static_cast<unsigned char>(Byte(::bof3::Img(scena_sc3::at::kScriptFlags)) | 0x80); }

void ScriptFlagsAnd(unsigned mask) { Field_ScriptFlags = Field_ScriptFlags & mask; }

short S16(std::uint32_t a) { return static_cast<short>(Word(At(a))); }

// A table of code pointers in the exe's data, read as the original reads it:
// afresh, and indexed without a bound.
std::uint32_t Entry(std::uint32_t table, int index) {
    return static_cast<std::uint32_t>(Long(At(table + 4u * static_cast<std::uint32_t>(index))));
}

// The flag row, the dword 0x929ED0 read afresh at each call.
unsigned char* Bank() { return reinterpret_cast<unsigned char*>(Chapter_FlagRow); }

bool Flag(unsigned n) { return SH_CALL(Flags_Test)(Bank(), n) != 0; }

void SetFlag(unsigned n) { SH_CALL(Flags_Set)(Bank(), n); }

void PartyRestore() { SH_AT(void (__cdecl*)(), scena_sc3::kPartyRestore.v)(); }

void StatusBit80() { SH_AT(void (__cdecl*)(), scena_sc3::kStatusBit80.v)(); }

unsigned char CellFind(std::uint32_t records, unsigned n, int x, int z) {
    return static_cast<unsigned char>(SH_AT(std::uint32_t (__cdecl*)(std::uint32_t, unsigned, int, int), scena_sc3::kCellFind.v)(records, n, x, z));
}

// Takes an effect slot into the byte 0x903850 and, when there is one, marks
// its record live as `kind`; the record is indexed by the stored byte read
// back as a dword's low byte, as the originals have it.
unsigned char* TakeEffect(unsigned char kind) {
    const unsigned char slot = SH_CALL(Effect_FindFree)();
    scena_sc3::at::kSlot.b_00 = slot;
    if (slot == 0xFF) return nullptr;
    unsigned char* const e = At(::bof3::Img(Effect_Objects_addr) + ((static_cast<std::uint32_t>(scena_sc3::at::kSlot.d_00) & 0xFF) << 7));
    e[0] = 1;
    e[5] = kind;
    return e;
}

// The effect whose slot is kept in the s8 0x8034E3: indexed by the answer as
// a signed byte (an answer of 0x80..0xFE would index below the pool).
unsigned char* TakeKeptEffect(unsigned char kind) {
    const unsigned char slot = SH_CALL(Effect_FindFree)();
    MoveScript_PartyRecords[0x63] = slot;
    if (slot == 0xFF) return nullptr;
    unsigned char* const e = At(::bof3::Img(Effect_Objects_addr) + static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<signed char>(slot)) << 7));
    e[0] = 1;
    e[5] = kind;
    return e;
}

// The high word of a 16.16 coordinate, and "within n cells of lo" as the
// originals test it: a 16-bit subtraction compared unsigned.
unsigned Hi(int v) { return (static_cast<std::uint32_t>(v) >> 16) & 0xFFFF; }

bool Near(unsigned hi, unsigned lo, unsigned n) { return ((hi - lo) & 0xFFFF) < n; }

void ChangeArea(unsigned area, int x, int z, unsigned flags) { SH_CALL(Field_ChangeArea)(area, x, z, flags); }

void MusicPlayTrack() { SH_CALL(Music_Play)(Music_Track, 8); }

// Scena04_Scene2's common exit: with the step it last stored (or read) - 2
// below 0xB as a byte, the tremor (a tail jump to 0x546130).
void Scene2Exit(unsigned char al) {
    if (static_cast<unsigned char>(al - 2) < 0xB) SH_CALL(Scena04_Tremor)();
}

// The effect of kind 0x13 Scena04_Scene2 places with the camera's words.
void Effect13Cam(std::int32_t a, bool yaw, std::int32_t c, bool pitch, unsigned char b9) {
    unsigned char* const e = TakeEffect(0x13);
    if (!e) return;
    const std::int32_t first = yaw ? Camera_Angles[0] : a;
    const std::int32_t y = Camera_Angles[1];
    const std::int32_t third = pitch ? S16(::bof3::Img(scena_sc3::at::kPitch)) : c;
    SetLong(e + 0x64, first);
    SetLong(e + 0x68, y);
    SetLong(e + 0x6C, third);
    e[9] = b9;
}

// The body: returns the byte in al at the common exit.
unsigned char Scene2Body() {
    unsigned char al = MoveScript_PartyRecords[0x65];
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
        Cond_ByteFE = 1;
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
        Field_Kind2X = 0x4E0000;
        Field_Kind2Z = 0x280000;
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
        Field_Kind2X = 0x850000;
        Field_Kind2Z = 0xA0000;
        SH_CALL(Field_ViewReset)();
        SH_CALL(Music_LoadFile)(0x41);
        SetStep(kBx);
        return static_cast<unsigned char>(kBx);
    case 0x1E:
        if (SH_CALL(File_LoadDone)() == 0) return MoveScript_PartyRecords[0x65];
        SetTimer(4);
        SetStep(8);
        return 8;
    case 8: {
        if (Timer() != 0) {
            DecTimer();
            return al;
        }
        PassFlags(0x1F);
        Camera_Angles[0] = 0xFCA6;
        SetWord(At(::bof3::Img(Cond_AngleFB_addr)), 0x74);
        Camera_Distance = 0x400;
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
            const unsigned d = (static_cast<std::uint16_t>(Camera_Distance) - 0x22u) & 0xFFFF;
            Camera_Distance = d;
            if (d & 0x8000) Camera_Distance = 0;
            return al;
        }
        const unsigned char c = C0();
        Camera_Distance = 0;
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
        if (!TakeEffect(0x2C)) return MoveScript_PartyRecords[0x65];
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
            const unsigned d = (static_cast<std::uint16_t>(Camera_Distance) + 9u) & 0xFFFF;
            Camera_Distance = d;
            if (static_cast<short>(d) > 0x400) Camera_Distance = 0x400;
            DecTimer();
            return al;
        }
        ScriptFlagsOr80();
        ChangeArea(0x41, 0x2E0000, 0x210000, 1);
        unsigned char* const bank = Bank();
        Area_EnterTransition = 0xFF;
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
            al = MoveScript_PartyRecords[0x65];
        }
        if (Timer() != 0) return al;
        ScriptFlagsAnd(0xFF7F);
        ChangeArea(0x36, 0xE0000, 0x100000, 0x80);
        ModeFlow_AreaTransition = 0;
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

// The leader's +8 into the byte 0x903850; true unless it is 4, 5 or 6.
bool LeaderFree() {
    const unsigned char k = ObjTrio[0].direction;
    scena_sc3::at::kSlot.b_00 = k;
    return k != 6 && k != 5 && k != 4;
}

}  // namespace

// original 0x545010: slot 0 of Scena04_Hooks - Scena04_States 0x660FE4 on the
// s8 0x8034E2, unchecked.
extern "C" void __cdecl Scena04_Frame(void) {
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(Entry(::bof3::Img(Scena04_States_addr), static_cast<signed char>(MoveScript_PartyRecords[0x62]))))();
}

// original 0x545020: state 0 - state 1, chapter 4's row 0x903FB0 cleared.
extern "C" void __cdecl Scena04_Start(void) {
    MoveScript_PartyRecords[0x62] = 1;
    SetLong(At(::bof3::Img(Cond_Flags_addr) + 0x20), 0);
}

// original 0x545040: state 1, once per area entered (areas 0x28, 0x2A, 0x30,
// 0x36, 0x41); then state 2.
extern "C" void __cdecl Scena04_EnterArea(void) {
    if (Area() == 0x28) {
        if (Cond_ByteFD == 7 && !Flag(3)) {
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
        if (Cond_ByteFD != 3) {
            MoveScript_PartyRecords[0x62] = 2;
            return;
        }
        unsigned char* const bank = Bank();
        ClearCounters();
        if (SH_CALL(Flags_Test)(bank, 6) == 0) {
            SetFlag(6);
            Area_EnterTransition = 5;
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
        MoveScript_PartyRecords[0x62] = 2;
        return;
    }
    if (area == 0x41 && Flag(7) && !Flag(0xC)) {
        ClearCounters();
        SH_CALL(Kind2_Place)(0);
        for (unsigned i = 0; i < 3; ++i) {
            unsigned char& b = Byte(::bof3::Img(scena_sc3::at::kMembers) + i * scena_sc3::at::kMemberStride + 0x24);
            b = static_cast<unsigned char>(b & 0xDF);
        }
        if (unsigned char* const e = TakeEffect(0x13)) {
            const std::int32_t y = Camera_Angles[1], z = S16(::bof3::Img(scena_sc3::at::kPitch));
            SetLong(e + 0x64, -0xC6);
            SetLong(e + 0x68, y);
            SetLong(e + 0x6C, z);
            e[9] = 0x40;
        }
        TakeEffect(0x2E);
    }
    MoveScript_PartyRecords[0x62] = 2;
}

// original 0x545330: state 2, the run - Scena04_Runs 0x660FF0 on
// MoveScript_Var7 (s8), unchecked.
extern "C" void __cdecl Scena04_Run(void) {
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(Entry(::bof3::Img(Scena04_Runs_addr), MoveScript_Var7)))();
}

// original 0x545340: run 1, steps 0..0x1E (the jump table 0x545948 of 31;
// 0x5453D0, 0x5455A0 and 0x545910, listed as hidden starts, are three of its
// cases): a quake (the map elevation shaken on Scena04_Quake), the camera
// pulled back, messages, streams, the area 0x28 changes on the counters, and
// at the end the mode 7 switch waited out (Game_Mode 2).
extern "C" void __cdecl Scena04_Scene1(void) {
    switch (MoveScript_PartyRecords[0x65]) {
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
        const std::int32_t i = static_cast<signed char>(MoveScript_PartyRecords[0x63]);
        if (At(::bof3::Img(Effect_Objects_addr) + static_cast<std::uint32_t>(i << 7))[0] & 1) return;
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
            const unsigned t = MoveScript_PartyRecords[0x66];
            MapView_Redraw = 2;
            const std::int32_t d = static_cast<signed char>(Byte(::bof3::Img(scena_sc3::at::kQuake4) + (t & 3)));
            const std::int32_t elevation = MapView_Elevation;
            DecTimer();
            MapView_Elevation = elevation + (d << 6);
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
            MapView_Redraw = 2;
            Camera_Distance = t * 9;
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
        Field_Request = 2;
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
        Field_Request = 2;
        SetStep(0xD);
        return;
    case 0xD:
        if (Requested()) return;
        C0() = static_cast<unsigned char>(C0() + 1);
        SH_CALL(Music_Play)(0x3F, 8);
        PassFlags(0x1F);
        Camera_Distance = 0;
        MapView_Redraw = 2;
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
        Field_Kind2X = 0xAC0000;
        Field_Kind2Z = 0x210000;
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
        AreaW1f_MusicCurrent = 0x54;
        ModeFlow_AreaTransition = 0xFF;
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
        ModeFlow_AreaTransition = 0xFF;
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
        ModeStates_ShopObject = 0xFE;
        BattleE1_Mode = 0;
        ScenaSc1_ShopByteB = 0;
        Byte(::bof3::Img(scena_sc3::at::kMemberCount2)) = 1;
        Game_Mode = 7;
        SetStep(0x1D);
        return;
    case 0x1D:
        if (Game_Mode != 2) return;
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
    const unsigned i = (Frame_Counter >> 1) & 3;
    const std::int32_t elevation = MapView_Elevation;
    MapView_Redraw = 2;
    const std::int32_t d = static_cast<signed char>(Byte(::bof3::Img(scena_sc3::at::kTremor4) + i));
    MapView_Elevation = elevation + d * 4;
}

// original 0x546160: slot 1, the object trigger - Scena04_ObjectHandlers
// 0x661004 by the object's +0x86, called with (object, the flag row).
extern "C" void __cdecl Scena04_ObjectHook(unsigned char* object) {
    unsigned char* const bank = Bank();
    const unsigned index = object[0x86];
    reinterpret_cast<ObjectHandler>(static_cast<std::uintptr_t>(Entry(::bof3::Img(Scena04_ObjectHandlers_addr), static_cast<int>(index))))(object, bank);
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
    Byte(::bof3::Img(scena_sc3::at::kScriptFlagsHi)) = static_cast<unsigned char>(Byte(::bof3::Img(scena_sc3::at::kScriptFlagsHi)) | 1);
    Field_Request = 2;
    return 1;
}

// original 0x546340: slot 4, the cell hook - 0x56D800 over Scena04_Cells
// 0x661008; none: 0xFF; else Scena04_CellHandlers 0x661010 [answer], 1.
extern "C" unsigned char __cdecl Scena04_CellHook(int x, int z) {
    const unsigned char r = CellFind(::bof3::Img(Scena04_Cells_addr), 1, x, z);
    if (static_cast<signed char>(r) < 0) return 0xFF;
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(Entry(::bof3::Img(Scena04_CellHandlers_addr), static_cast<signed char>(r))))();
    return 1;
}

// original 0x546370: the cell's handler - run 2 with 0xA frames.
extern "C" void __cdecl Scena04_Cell0(void) {
    SH_CALL(ScriptFlags_Set40)();
    SetTimer(0xA);
    SetRun(2);
    SetStep(0);
}

void Scena042_Inject() {
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

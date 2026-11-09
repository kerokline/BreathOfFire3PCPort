// Moved here by tools/move_funcs.py from scena_sc15.cpp (docs/source-layout-round.md).
#include "game/scenario/scena_15_2.h"

#include <bit>
#include <cstdint>
#include <cstring>
#include <vector>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scena_sc15.h"
#include "game/scena_sc15_callees.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace {

using namespace scena_sc15;

}  // namespace

#define SC15_EXPORT extern "C" __attribute__((disable_tail_calls))

namespace {

using std::int32_t;

using std::uint32_t;

using VoidFn = void (__cdecl*)();

using ObjectFn = void (__cdecl*)(unsigned char*, unsigned char*);

unsigned char* AtScenaSc15(uint32_t a) { return move_script::At(a); }

unsigned char& B(uint32_t a) { return AtScenaSc15(a)[0]; }

std::uint16_t W(uint32_t a) { return move_script::Word(AtScenaSc15(a)); }

void SetW(uint32_t a, unsigned v) { move_script::SetWord(AtScenaSc15(a), v); }

uint32_t L(uint32_t a) { return static_cast<uint32_t>(move_script::Long(AtScenaSc15(a))); }

uint32_t L(const unsigned char* p) { return static_cast<uint32_t>(move_script::Long(p)); }

void SetL(uint32_t a, uint32_t v) { move_script::SetLong(AtScenaSc15(a), static_cast<int32_t>(v)); }

void SetL(unsigned char* p, uint32_t v) { move_script::SetLong(p, static_cast<int32_t>(v)); }

std::uint16_t W(const unsigned char* p) { return move_script::Word(p); }

void SetW(unsigned char* p, unsigned v) { move_script::SetWord(p, v); }

// The chapter bytes (docs/scenario-roots.md section 2).
// kChapter: now Cond_ByteFA (symbols.toml). Cond_ByteFA
// kStatus: now Field_StatusBits (symbols.toml). Field_StatusBits
// kState: now MoveScript_PartyRecords[0x62] (symbols.toml). s8, the frame's state
// kPart: now MoveScript_PartyRecords[0x63] (symbols.toml). s8, chapter 17's part (Scena17_Parts)
// kRun: now MoveScript_Var7 (symbols.toml). MoveScript_Var7: the run (15, 18, 19), the part's step (17)
// kStep: now MoveScript_PartyRecords[0x65] (symbols.toml). u8, the run's step
constexpr uint32_t kTimer = 0x8034E6;       // u16

// kByteFD: now Cond_ByteFD (symbols.toml). Cond_ByteFD
// The script counters (MoveScript_CounterOps) and the chapters' other cells.
constexpr uint32_t kCounter0 = 0x903848;

constexpr uint32_t kCounter1 = 0x903849;

constexpr uint32_t kCounter3 = 0x90384B;

inline auto& kSlotWord = SharedScratch.w_00;  // u16: the event object's slot (Scena15_EventObjects)

// kFlagRow: now Chapter_FlagRow (symbols.toml). the flag bits Flags_Test / Flags_Set are given
// kStoryRow: now Cond_Flags[0xa0] (symbols.toml). Cond_Flags + 0xA0, the story flags
// kRow14: now Cond_Flags[0x70] (symbols.toml). Cond_Flags + 0x70
// kRow3: now Cond_Flags[0x78] (symbols.toml). Cond_Flags + 0x78 (chapter 15's row is 0x904008)
// kWait: now Transition_Busy (symbols.toml). Transition_Busy
// kRequest: now Field_Request (symbols.toml). Field_Request
// kArea: now Game_AreaNumber (symbols.toml). Game_AreaNumber
// kPass: now Draw_PassFlags (symbols.toml). Draw_PassFlags
// kEffects: now Effect_Objects (symbols.toml). Effect_Objects, 20 records of 0x80
// kSprites: now Sprite_Objects (symbols.toml). Sprite_Objects, 30 records of 0xA4
// kDistance: now Camera_Distance (symbols.toml). Camera_Distance (s16)
// kShiftY: now Camera_ShiftY (symbols.toml). Camera_ShiftY
// kAngle0: now Camera_Angles (symbols.toml). Camera_Angles[0]
// kAngle1: now Camera_Angles[1] (symbols.toml). Camera_Angles[1]
constexpr uint32_t kAngle2 = 0x929ECC;      // Cond_AngleFB's word

constexpr uint32_t kInputHeld = 0x7E1BE8;   // Input_Held (u16; read as a dword by run 5 step 0x18)

// Chapter 15's and 17's own bytes (Scena15_Bytes 0x6BC740, read by no other code).
// kShakeShift: now Scena15_Bytes (symbols.toml). the camera shake's shift (Scena15_Shake's argument)
// kShakeOn: now Scena15_Bytes[1] (symbols.toml). the camera shakes while set
constexpr uint32_t kKept = 0x6BC742;        // run 4: the party byte kept; run 6: Scena15_RandomPause's count

// Chapter 15's tables.
// kStates15: now Scena15_States (symbols.toml). Scena15_States, 3
// kRuns15: now Scena15_Runs (symbols.toml). Scena15_Runs, 7
constexpr uint32_t kBattles = 0x6618C0;     // Scena15_Battles: 10 records of 10 bytes

// kOpRecords0: now Scena15_EventOps (symbols.toml). Scena15_EventOps: three EventOp_0x records of 0x11
// kOpRecords6: now Scena15_EventOps[0x38] (symbols.toml). Scena15_EventOps6: two EventOp_6x records of 0x10
constexpr uint32_t kShakeSteps = 0x6619A8;  // Scena15_ShakeSteps: four s8

// kObjects15: now Scena15_Objects (symbols.toml). Scena15_Objects, 12
constexpr uint32_t kTalkWho = 0x6619DC;     // Scena15_TalkWho: five bytes

constexpr ::bof3::image::CodeVa kBootTask{bof3::addr::Boot_Task};    // Boot_Task's address, as the original pushes it

// A handler out of one of the chapters' .data tables, read in place and
// unchecked as the original reads it; where the original would jump to what
// is not code (an index past its table), ours aborts. During the fuzz the
// tables hold the harness's recorders, outside .text.
uint32_t CodeAt(uint32_t table, int index, const char* who) {
    const uint32_t at = table + 4u * static_cast<uint32_t>(index);
    const uint32_t entry = L(at);
    // A code word: hosted a VA in BOF3.exe's .text, in bof3x.exe a function of ours
    // the bind table names (common/image_base.h IsCodeWord); not the trap, not data.
    if (!scenario_harness::g_active && !::bof3::IsCodeWord(entry))
        bof3::Fatal("%s: index %d reads 0x%X at 0x%X (VA 0x%X), not code - past its table (the original jumps there)", who,
                    index, (unsigned)entry, (unsigned)at, (unsigned)::bof3::VaOf(at));
    return entry;
}

VoidFn Handler(uint32_t table, int index, const char* who) {
    return reinterpret_cast<VoidFn>(static_cast<std::uintptr_t>(CodeAt(table, index, who)));
}

// An object hook's handler, called (object, the flag row) as 0x56D6D0's slot 1
// calls through a chapter's object table.
void CallObject(uint32_t table, unsigned char* object, const char* who) {
    unsigned char* const row = AtScenaSc15(Chapter_FlagRow);
    reinterpret_cast<ObjectFn>(static_cast<std::uintptr_t>(CodeAt(table, object[0x86], who)))(object, row);
}

// The chapter's flag row, as the original reads it: the pointer afresh.
unsigned char* Row() { return AtScenaSc15(Chapter_FlagRow); }

bool Test(unsigned n) { return SH_CALL(Flags_Test)(Row(), n) != 0; }

bool TestAt(uint32_t row, unsigned n) { return SH_CALL(Flags_Test)(AtScenaSc15(row), n) != 0; }

void Set(unsigned n) { SH_CALL(Flags_Set)(Row(), n); }

void SetAt(uint32_t row, unsigned n) { SH_CALL(Flags_Set)(AtScenaSc15(row), n); }

void ClearAt(uint32_t row, unsigned n) { SH_CALL(Flags_Clear)(AtScenaSc15(row), n); }

unsigned char* Effect(unsigned slot) { return AtScenaSc15(::bof3::Img(Effect_Objects_addr) + ((slot & 0xFFu) << 7)); }

unsigned char* Sprite(unsigned k) { return AtScenaSc15(::bof3::Img(Sprite_Objects_addr) + (k & 0xFFu) * 0xA4u); }

unsigned Area() { return Game_AreaNumber; }

void Step(unsigned v) { MoveScript_PartyRecords[0x65] = static_cast<unsigned char>(v); }

void Timer(unsigned v) { SetW(::bof3::Img(kTimer), v); }

// dec word [0x8034E6]: true when it reaches 0.
bool TimerDone() {
    SetW(::bof3::Img(kTimer), W(::bof3::Img(kTimer)) - 1u);
    return W(::bof3::Img(kTimer)) == 0;
}

void CounterUp() { B(::bof3::Img(kCounter0)) = static_cast<unsigned char>(B(::bof3::Img(kCounter0)) + 1); }

// The runs' common end: MoveScript_Var7 and the step 0 (no run).
void EndRun() {
    MoveScript_Var7 = 0;
    Step(0);
}

bool LoadDone() { return SH_CALL(File_LoadDone)() != 0; }

void Msg(unsigned id) { SH_CALL(Msg_OpenScript)(static_cast<unsigned short>(id)); }

// A script message opened, the request 2 (a message is open).
void Say(unsigned id) {
    Msg(id);
    Field_Request = 2;
}

void ChangeArea(unsigned area, int32_t x, int32_t z, unsigned flags) { SH_CALL(Field_ChangeArea)(area, x, z, flags); }

void DropIn(unsigned n) { SH_CALL(Party_DropIn)(n); }

void Transition(unsigned k) { SH_CALL(Transition_Start)(static_cast<unsigned char>(k)); }

void CallA(unsigned n) { SH_CALL(Scenario_CallA)(n); }

void Clear40() { SH_CALL(ScriptFlags_Clear40)(); }

void Set40() { SH_CALL(ScriptFlags_Set40)(); }

// Effect_FindFree; with a slot, its record live (+0 = 1) and of `kind` (+5).
unsigned char NewEffect(unsigned char kind) {
    const unsigned char s = SH_CALL(Effect_FindFree)();
    if (s != 0xFF) {
        unsigned char* const e = Effect(s);
        e[0] = 1;
        e[5] = kind;
    }
    return s;
}

// The same with the slot kept in 0x6BC743, where the runs wait on it.
unsigned char NewEffectKept(unsigned char kind) {
    const unsigned char s = SH_CALL(Effect_FindFree)();
    Scena15_Bytes[3] = s;
    if (s != 0xFF) {
        unsigned char* const e = Effect(s);
        e[0] = 1;
        e[5] = kind;
    }
    return s;
}

// The kept slot's record still live (+0 bit 0); the slot read as a byte,
// unchecked (0xFF reads past the 20 records).
bool KeptLive() { return (Effect(Scena15_Bytes[3])[0] & 1) != 0; }

// One channel stepped toward the grey level as 0x569DA0 steps it: +1 below or
// at it, +0xFFFF (a 16-bit -1, carried in 32 bits) above; kept when equal.
uint32_t Toward(uint32_t channel, uint32_t grey) {
    if (static_cast<std::uint16_t>(channel) == static_cast<std::uint16_t>(grey)) return channel;
    const int32_t d = static_cast<int32_t>((grey & 0xFFFFu) - channel);
    return channel + (d >= 0 ? 1u : 0xFFFFu);
}

// Scena15_Objects entries 6..11: the script flags' bit 0x40, a flag of the
// chapter's row, a party member dropped in (0xFF none), the object's word
// +0x8A counted.
void ObjectFlag(unsigned char* object, unsigned flag, unsigned member) {
    Set40();
    Set(flag);
    if (member != 0xFF) DropIn(member);
    rv::As<records::SpriteObject>(object)->script_position = rv::As<records::SpriteObject>(object)->script_position + 1u;
}

}  // namespace

// original 0x567DC0: chapter 15's vtable slot 0 (PSX 0x801F9CAC): a tail jump
// through Scena15_States on the s8 0x8034E2 - 0 is 0x5646B0 (state 1, chapter
// 13/14's), 1 Scena15_EnterArea, 2 Scena15_Run. Unchecked.
SC15_EXPORT void __cdecl Scena15_Frame(void) {
    Handler(::bof3::Img(Scena15_States_addr), static_cast<signed char>(MoveScript_PartyRecords[0x62]), "Scena15_Frame")();
}

// original 0x567DD0: state 1: the set-up of the area entered, by
// Game_AreaNumber (read afresh at every test), Cond_ByteFD and the chapter's
// flags, then state 2. Areas 0x9E..0xA6 start the event battles 0..9
// (Scena15_BattleSetup) or set a flag; 0xA4 and 0xA6 with Cond_ByteFD not 0,
// and 0xAD with it not 2, go straight to state 2.
SC15_EXPORT void __cdecl Scena15_EnterArea(void) {
    auto* const xobj0 = Sprite_ObjectsExtra;
    if (Area() == 2 && Test(7) && !Test(8)) Draw_PassFlags = 0;
    if (Area() == 0x2D && Test(9) && !Test(0xA)) {
        Camera_Distance = 0x5DC;
        MapView_Redraw = 2;
    }
    if (Area() == 0x95 && Test(0x16)) {
        const unsigned a2 = W(::bof3::Img(kAngle2)), a1 = static_cast<std::uint16_t>(Camera_Angles[1]), a0 = static_cast<std::uint16_t>(Camera_Angles[0]);
        xobj0->area_script = 2;
        rv::Store<unsigned>(xobj0->script_position, 0);
        rv::Store<uint32_t>(xobj0->x, static_cast<uint32_t>(xobj0->x) + 0xC0000u);
        Cond_ByteFE = 2;
        Scena15_Bytes[1] = 0;
        Camera_Distance = 0xD00;
        SH_CALL(Scena15_PlaceEffect)(0x31, static_cast<int>(a0), static_cast<int>(a1), static_cast<int>(a2), 0xFF, 0);
    }
    if (Area() == 0x98 && Test(0xA) && !Test(0xB)) {
        Camera_Distance = 0x5DC;
        MapView_Redraw = 2;
    }
    // 0x9E..0xA6: on Cond_ByteFD 0 and 1 (read afresh), a battle or a flag
    struct Pair { unsigned short area; unsigned char flag0, battle0, flag1, battle1; };
    // battle 0xFF: the flag set instead
    static const Pair kPairs[] = {
        {0x9E, 0x20, 0, 0x21, 0xFF}, {0x9F, 0x22, 1, 0x23, 0xFF}, {0xA0, 0x24, 0xFF, 0x25, 2},
        {0xA1, 0x26, 0xFF, 0x27, 3}, {0xA2, 0x28, 4, 0x29, 5},    {0xA3, 0x2A, 6, 0x2B, 0xFF},
    };
    for (const Pair& p : kPairs) {
        if (Area() != p.area) continue;
        if (Cond_ByteFD == 0 && !Test(p.flag0)) {
            if (p.battle0 == 0xFF) Set(p.flag0);
            else SH_CALL(Scena15_BattleSetup)(p.battle0);
        }
        if (Cond_ByteFD == 1 && !Test(p.flag1)) {
            if (p.battle1 == 0xFF) Set(p.flag1);
            else SH_CALL(Scena15_BattleSetup)(p.battle1);
        }
    }
    if (Area() == 0xA4) {
        if (Cond_ByteFD != 0) {
            MoveScript_PartyRecords[0x62] = 2;
            return;
        }
        if (!Test(0x2C)) SH_CALL(Scena15_BattleSetup)(7);
    }
    if (Area() == 0xA5) {
        if (Cond_ByteFD == 0 && !Test(0x2D)) Set(0x2D);
        if (Cond_ByteFD == 1 && !Test(0x2E)) SH_CALL(Scena15_BattleSetup)(8);
    }
    if (Area() == 0xA6) {
        if (Cond_ByteFD != 0) {
            MoveScript_PartyRecords[0x62] = 2;
            return;
        }
        if (!Test(0x2F)) SH_CALL(Scena15_BattleSetup)(9);
    }
    if (Area() == 0xAC && Test(0x12)) Draw_PassFlags = 0;
    if (Area() == 0xAD) {
        if (Cond_ByteFD != 2) {
            MoveScript_PartyRecords[0x62] = 2;
            return;
        }
        if (!Test(0)) {
            Set(0);
            DropIn(3);
        }
        if (Test(0x15) && !Test(0x16)) {
            NewEffect(0xAF);
            SH_CALL(Scena15_SpawnMarker)(0x2E8000, 0x6D0000, 2, 3, 3);
            CallA(3);
            DropIn(0xA);
            Draw_PassFlags = 0x1F;
        }
    }
    if (Area() == 0xAE) {
        if (Test(2) && !Test(3)) SH_CALL(Scena15_SpawnMarker)(0x440000, 0x20000, 0xC, 6, 1);
        if (Test(0xA) && !Test(0xB)) {
            SH_CALL(Music_LoadFile)(0xA2);
            Draw_PassFlags = 0;
        }
        if (Test(0x13)) {
            if (!Test(0x14)) {
                Draw_PassFlags = 0x1F;
                CallA(2);
                DropIn(0x13);
            } else if (!Test(0x15)) {
                Draw_PassFlags = 0x1F;
            }
        }
    }
    if (Area() == 0xBD && TestAt(::bof3::Img(Cond_Flags_addr) + 0x70, 1)) Effect_Objects[0].flags = static_cast<unsigned char>(Effect_Objects[0].flags | 0x40);
    if (Area() == 0xC6) {
        if (Test(0x13)) {
            NewEffect(0xAB);
            NewEffect(0xB0);
            Scena15_Bytes[0] = 4;
            Draw_PassFlags = 0x1F;
        }
        if (Test(0x14) && !Test(0x15)) Draw_PassFlags = 0x1F;
        if (Test(0x15) && !Test(0x16)) {
            Scena15_Bytes[0] = 4;
            Draw_PassFlags = 0x1F;
        }
    }
    MoveScript_PartyRecords[0x62] = 2;
}

// original 0x568510: event battle n's set-up (areas 0x9E..0xA6): the script
// flags' bit 0x40, counter 0 = 1; an effect of kind 0x89 with +0xB = n (its
// slot in 0x6BC743); the kind-2 sprite at Scena15_Battles[n]'s (+0, +2) << 8
// (the index n & 0xFF, unchecked: ten records); Field_ViewReset; 0x904EF0 = 0,
// Party_DropIn(0); Field_ScriptFlags2 bit 4 off; the party placed at
// (+4, +6) << 8 with kind +9 (0x532ED0); the OT slot 4, run 2 at step 0.
SC15_EXPORT void __cdecl Scena15_BattleSetup(unsigned n) {
    Set40();
    B(::bof3::Img(kCounter0)) = 1;
    const unsigned char s = SH_CALL(Effect_FindFree)();
    Scena15_Bytes[3] = s;
    if (s != 0xFF) {
        unsigned char* const e = Effect(s);
        e[0] = 1;
        e[5] = 0x89;
        e[0xB] = static_cast<unsigned char>(n);
    }
    const uint32_t rec = ::bof3::Img(Scena15_Battles_addr) + (n & 0xFFu) * 10u;
    Field_Kind2X = static_cast<uint32_t>(static_cast<int32_t>(static_cast<short>(W(rec))) << 8);
    Field_Kind2Z = static_cast<uint32_t>(static_cast<int32_t>(static_cast<short>(W(rec + 2))) << 8);
    SH_CALL(Field_ViewReset)();
    EventLeader_PendingFlag = 0;
    DropIn(0);
    const int32_t z = static_cast<int32_t>(static_cast<short>(W(rec + 6))) << 8;
    const int32_t x = static_cast<int32_t>(static_cast<short>(W(rec + 4))) << 8;
    const unsigned kind = B(rec + 9);
    Field_ScriptFlags2 = Field_ScriptFlags2 & 0xFFEFu;
    SH_AT(void (__cdecl*)(long, long, unsigned), kPartyPlace.v)(x, z, kind);
    Draw_OtSlot = 4;
    MoveScript_Var7 = 2;
    Step(0);
}

// original 0x5685D0: an effect placed: Effect_FindFree into 0x6BC743; with a
// slot, +0 = 1, +5 = kind, +0x64 / +0x68 / +0x6C the three s16 x, y, z, +0xC
// the dword, +9 the byte; al 1. No slot: al 0.
SC15_EXPORT unsigned char __cdecl Scena15_PlaceEffect(unsigned kind, int x, int y, int z, unsigned nine, unsigned long twelve) {
    const unsigned char s = SH_CALL(Effect_FindFree)();
    Scena15_Bytes[3] = s;
    if (s == 0xFF) return 0;
    unsigned char* const e = Effect(s);
    e[0] = 1;
    e[5] = static_cast<unsigned char>(kind);
    SetL(e + 0x64, static_cast<uint32_t>(static_cast<int32_t>(static_cast<short>(x))));
    SetL(e + 0x68, static_cast<uint32_t>(static_cast<int32_t>(static_cast<short>(y))));
    SetL(e + 0x6C, static_cast<uint32_t>(static_cast<int32_t>(static_cast<short>(z))));
    rv::As<records::EffectObject>(e)->step_x = static_cast<uint32_t>(twelve);
    e[9] = static_cast<unsigned char>(nine);
    return 1;
}

// original 0x568640: state 2: a tail jump through Scena15_Runs on the s8
// MoveScript_Var7 - 0 a bare ret (0x437CC0), 1..6 Scena15_Run1..6. Unchecked.
SC15_EXPORT void __cdecl Scena15_Run(void) {
    Handler(::bof3::Img(Scena15_Runs_addr), MoveScript_Var7, "Scena15_Run")();
}

// original 0x568650: run 1, steps 0..7, 0x14 and 0x19 (a byte table into a
// 10-entry jump table; any other step nothing): counter 0 cleared and the
// party dropped in, flag 1 of row 14 and area 0xBD; three script messages
// (0x12..0x14) between two effects of kind 0x13, the hand drawn at (0x98,
// 0x28 / 0x38 / 0x10) while each message is open; then the flag cleared and
// area 0xC1 (facing 0x82). Steps 0x14 and 0x19 (set by the field scripts)
// change to area 0xC1 facing 0x83 or 0x84 with their flags.
SC15_EXPORT void __cdecl Scena15_Run1(void) {
    const unsigned step = MoveScript_PartyRecords[0x65];
    switch (step) {
    case 0:
        if (Field_Request == 2) return;
        B(::bof3::Img(kCounter0)) = 0;
        DropIn(1);
        Step(1);
        return;
    case 1:
        if (B(::bof3::Img(kCounter0)) != 1) return;
        SetAt(::bof3::Img(Cond_Flags_addr) + 0x70, 1);
        ChangeArea(0xBD, 0x10000000, 0x14000000, 2);
        AreaW1f_MusicCurrent = Music_Track;
        AreaW3g_HealByte = 2;
        Field_StatusBits = static_cast<unsigned char>(Field_StatusBits | 1);
        SetW(::bof3::Img(0x90405C), 0);
        Step(2);
        return;
    case 2:
        if (Transition_Busy != 0) return;
        Say(0x12);
        Step(3);
        return;
    case 3:
    case 5:
        if (Field_Request == 2) {
            SH_CALL(Menu_DrawHand)(0x98, step == 3 ? 0x28 : 0x38, 0);
            return;
        }
        SH_CALL(Scena15_PlaceEffect)(0x13, static_cast<std::uint16_t>(Camera_Angles[0]), static_cast<std::uint16_t>(Camera_Angles[1]), step == 3 ? -0x200 : 0x400, 0x20, 0);
        Step(step + 1);
        return;
    case 4:
    case 6:
        if (KeptLive()) return;
        Say(step == 4 ? 0x13 : 0x14);
        Step(step + 1);
        return;
    case 7:
        if (Field_Request == 2) {
            SH_CALL(Menu_DrawHand)(0x98, 0x10, 0);
            return;
        }
        Clear40();
        ClearAt(::bof3::Img(Cond_Flags_addr) + 0x70, 1);
        ChangeArea(0xC1, 0x1A8000, 0x1C0000, 0x82);
        Field_StatusBits = static_cast<unsigned char>(Field_StatusBits & 0xFE);
        EndRun();
        return;
    case 0x14:
    case 0x19:
        if (Field_Request == 2) return;
        if (step == 0x14) {
            ClearAt(::bof3::Img(Cond_Flags_addr) + 0xa0, 0x77);
            AreaW3c_CampFlag = 0;
            ChangeArea(0xC1, 0x1A8000, 0x1C0000, 0x83);
        } else {
            SetAt(::bof3::Img(Cond_Flags_addr) + 0x70, 2);
            SetAt(::bof3::Img(Cond_Flags_addr) + 0x70, 3);
            ChangeArea(0xC1, 0x1A8000, 0x1C0000, 0x84);
            ModeFlow_AreaTransition = 0xFF;
        }
        SetAt(::bof3::Img(Cond_Flags_addr) + 0xa0, 0x8A);
        Field_StatusBits = static_cast<unsigned char>(Field_StatusBits & 0xFE);
        Area_EnterTransition = 0xFF;
        AreaW1f_MusicCurrent = 0xFF;
        EndRun();
        return;
    default: return;
    }
}

// original 0x568980: run 2, steps 0..5 (a jump table): the event battle set
// up by Scena15_BattleSetup - on any held button counter 0 = 2; on counter 0
// 3 the effect's +1 = 2; step 3 starts the battle Scena15_Battles[effect
// +0xB] +9 (Field_StartEventBattle) with the timer 0x78; step 5 sets the
// record's flag +8 and ends the run. Steps 2 and 4 wait (the field scripts
// move them on).
SC15_EXPORT void __cdecl Scena15_Run2(void) {
    switch (MoveScript_PartyRecords[0x65]) {
    case 0:
        if (Input_Held == 0) return;
        B(::bof3::Img(kCounter0)) = 2;
        Step(1);
        return;
    case 1: {
        if (B(::bof3::Img(kCounter0)) != 3) return;
        const unsigned s = Scena15_Bytes[3];
        Step(2);
        Effect(s)[1] = 2;
        return;
    }
    case 3: {
        const unsigned idx = Effect(Scena15_Bytes[3])[0xB];
        SH_CALL(Field_StartEventBattle)(B(::bof3::Img(kBattles) + 9 + idx * 10u));
        Timer(0x78);
        Step(4);
        return;
    }
    case 5: {
        Clear40();
        const unsigned idx = Effect(Scena15_Bytes[3])[0xB];
        SH_CALL(Flags_Set)(Row(), B(::bof3::Img(kBattles) + 8 + idx * 10u));
        B(::bof3::Img(kCounter0)) = 0;
        EndRun();
        return;
    }
    default: return;
    }
}

// original 0x568A70: run 3, steps 0..9, 0xF, 0x14..0x1B, 0x1E, 0x1F, 0x28..0x2B,
// 0x32..0x34 (a byte table into a 29-entry jump table; any other step
// nothing). Each waits on counter 0, the timer, a file, a message or an
// effect, then loads a file, places an effect or a marker, drops the party
// in, sets a flag or changes the area; see docs/scena_sc15.md section 3.
SC15_EXPORT void __cdecl Scena15_Run3(void) {
    switch (MoveScript_PartyRecords[0x65]) {
    case 0:
        SH_CALL(LoadDatFile)(0x302);
        DropIn(4);
        Step(1);
        return;
    case 1:
        if (B(::bof3::Img(kCounter0)) != 0xB) return;
        if (SH_CALL(Scena15_PlaceEffect)(0x13, -0x32A, static_cast<std::uint16_t>(Camera_Angles[1]), 0x80, 0x70, 0) == 0) return;
        Step(2);
        return;
    case 2: {
        if (!LoadDone()) return;
        if (B(::bof3::Img(kCounter0)) != 0xC) return;
        const unsigned char s = SH_CALL(Effect_FindFree)();
        Scena15_Bytes[3] = s;
        if (s == 0xFF) return;
        Timer(0xF0);
        Step(3);
        unsigned char* const e = Effect(s);
        e[0] = 1;
        e[5] = 0x9C;
        return;
    }
    case 3:
        if (!TimerDone()) return;
        Cond_ByteFE = 2;
        Step(4);
        return;
    case 4:
        if (!TestAt(::bof3::Img(Cond_Flags_addr) + 0xa0, 0x81)) return;
        if (SH_CALL(Scena15_PlaceEffect)(0x13, -0x2AA, static_cast<std::uint16_t>(Camera_Angles[1]), 0x200, 0x3C, 0) == 0) return;
        Step(5);
        return;
    case 5:
        if (KeptLive()) return;
        for (unsigned n = 5; n < 9; ++n)
            if (static_cast<unsigned char>(SH_CALL(Party_DropIn)(n)) == 0) break;   // al tested
        Step(6);
        return;
    case 6:
        if (B(::bof3::Img(kCounter0)) != 0xE) return;
        SH_CALL(Scena15_SpawnMarker)(0x2E8000, 0x6D0000, 0, 0x13, 0);
        Timer(0xA);
        Step(7);
        return;
    case 7:
        if (!TimerDone()) return;
        Step(8);
        CounterUp();
        return;
    case 8:
        if (B(::bof3::Img(kCounter0)) != 0x13) return;
        Timer(0xA);
        Step(9);
        return;
    case 9:
        if (!TimerDone()) return;
        CounterUp();
        EndRun();
        return;
    case 0xF:
        Clear40();
        {
            unsigned char* const row = Row();
            Cond_ByteFE = 2;
            SH_CALL(Flags_Set)(row, 1);
        }
        DropIn(9);
        EndRun();
        return;
    case 0x14:
        SH_CALL(Scena15_SpawnMarker)(0x440000, 0x20000, 4, 0xFF, 0);
        B(::bof3::Img(kCounter0)) = 1;
        DropIn(1);
        Step(0x15);
        return;
    case 0x15:
        if (B(::bof3::Img(kCounter0)) != 3) return;
        Transition(0);
        Step(0x16);
        return;
    case 0x16:
        if (Transition_Busy != 0) return;
        Draw_PassFlags = 0;
        Say(3);
        Step(0x17);
        return;
    case 0x17:
        if (Field_Request == 2) return;
        Step(0x18);
        return;
    case 0x18:
        Set(2);
        ChangeArea(0x58, 0x3E0000, 0x210000, 0x82);
        ModeFlow_AreaTransition = 0xFF;
        Area_EnterTransition = 0xFF;
        AreaW1f_MusicCurrent = 0x8E;
        Step(0x19);
        return;
    case 0x19:
        if (Transition_Busy != 0) return;
        Draw_PassFlags = 0x1F;
        Transition(1);
        Camera_Distance = 0x5DC;
        MapView_Redraw = 2;
        Step(0x1A);
        return;
    case 0x1A:
        if (B(::bof3::Img(kCounter0)) != 5) return;
        ChangeArea(0xAE, 0x440000, 0x38000, 0x82);
        ModeFlow_AreaTransition = 0;
        Step(0x1B);
        return;
    case 0x1B:
        if (B(::bof3::Img(kCounter0)) != 6) return;
        Set(3);
        B(::bof3::Img(kCounter0)) = 0;
        EndRun();
        return;
    case 0x1E:
        SH_CALL(Scena15_SpawnMarker)(0x520000, 0x2F0000, 5, 2, 0);
        DropIn(3);
        Step(0x1F);
        return;
    case 0x1F:
        if (B(::bof3::Img(kCounter0)) != 2) return;
        Set(4);
        B(::bof3::Img(kCounter0)) = 0;
        EndRun();
        return;
    case 0x28:
        B(::bof3::Img(kCounter0)) = 0xA;
        DropIn(4);
        Step(0x29);
        return;
    case 0x29:
        if (B(::bof3::Img(kCounter0)) != 0xC) return;
        Set(5);
        ChangeArea(0x3A, 0x140000, 0x10000, 0x81);
        AreaW1f_MusicCurrent = Music_Track;
        Step(0x2A);
        return;
    case 0x2A:
        if (B(::bof3::Img(kCounter0)) != 0xE) return;
        ChangeArea(0xAE, 0x250000, 0x3A0000, 0x85);
        Step(0x2B);
        return;
    case 0x2B:
        if (B(::bof3::Img(kCounter0)) != 0) return;
        Clear40();
        Set(6);
        EndRun();
        return;
    case 0x32:
        Set(7);
        ChangeArea(2, 0x1A0000, 0x170000, 0x87);
        Area_EnterTransition = 0xFF;
        AreaW1f_MusicCurrent = 0x8E;
        Step(0x33);
        return;
    case 0x33:
        if (B(::bof3::Img(kCounter0)) != 1) return;
        Draw_PassFlags = 0x1F;
        Transition(1);
        Step(0x34);
        return;
    case 0x34:
        if (B(::bof3::Img(kCounter0)) != 2) return;
        Clear40();
        Set(8);
        ChangeArea(0xAE, 0xA0000, 0x78000, 0x86);
        EndRun();
        return;
    default: return;
    }
}

// original 0x5690C0: run 4, steps 0..0x18 (a jump table): a file, an effect
// of kind 0x88 flying on counter 3, the party dropped in, two effects and the
// music faded; on counter 0 0x16 the second party set loaded with the party
// byte 0x90412C kept in 0x6BC742, and once the file is in, call-table entry 0
// with the first set (the byte put back), Party_DropIn(8), the event objects
// placed (Scena15_EventObjects) and the music; then four area changes, a
// message, two effects of kind 0x13, and the flag 0xB.
SC15_EXPORT void __cdecl Scena15_Run4(void) {
    switch (MoveScript_PartyRecords[0x65]) {
    case 0:
        SH_CALL(LoadDatFile)(0x303);
        DropIn(0);
        Step(1);
        return;
    case 1: {
        if (B(::bof3::Img(kCounter3)) != 0x10) return;
        const unsigned char s = SH_CALL(Effect_FindFree)();
        if (s != 0xFF) {
            unsigned char* const e = Effect(s);
            e[0] = 1;
            e[5] = 0x88;
            e[6] = 1;
            rv::As<records::EffectObject>(e)->x = 0x3B8000;
            rv::As<records::EffectObject>(e)->z = 0x220000;
            rv::As<records::EffectObject>(e)->height = 0x8000000;
            rv::As<records::EffectObject>(e)->step_x = 0x3F8000;
            rv::As<records::EffectObject>(e)->step_z = 0x220000;
            rv::Store<uint32_t>(rv::As<records::EffectObject>(e)->step_y, 0x6000000);
        }
        Timer(0x20);
        Step(2);
        return;
    }
    case 2:
        if (!TimerDone()) return;
        B(::bof3::Img(kCounter3)) = 0x18;
        Step(3);
        return;
    case 3:
        if (B(::bof3::Img(kCounter3)) != 0) return;
        DropIn(7);
        Step(4);
        return;
    case 4:
        if (!LoadDone()) return;
        if (B(::bof3::Img(kCounter0)) != 0xF) return;
        NewEffectKept(0xA0);
        SH_CALL(Music_FadeOut)(0x10);
        Step(5);
        return;
    case 5: {
        if (B(::bof3::Img(kCounter0)) != 0x11) return;
        const unsigned char s = SH_CALL(Effect_FindFree)();
        Scena15_Bytes[3] = s;
        if (s != 0xFF) {
            Step(6);
            unsigned char* const e = Effect(s);
            e[0] = 1;
            e[5] = 0x9F;
        }
        SH_CALL(Music_FadeOutStop)(0xA);
        return;
    }
    case 6:
        if (B(::bof3::Img(kCounter0)) != 0x12) return;
        Timer(0x1E);
        Step(7);
        return;
    case 7:
        if (!TimerDone()) return;
        SH_CALL(Music_LoadFile)(0xA2);
        CounterUp();
        Step(8);
        return;
    case 8:
        if (B(::bof3::Img(kCounter0)) != 0x14) return;
        if (!LoadDone()) return;
        SH_CALL(Kind2_Place)(1);
        Step(9);
        return;
    case 9:
        if (B(::bof3::Img(kCounter0)) != 0x15) return;
        Step(0xA);
        return;
    case 0xA:
        if (B(::bof3::Img(kCounter0)) != 0x16) return;
        Scena15_Bytes[2] = AreaW0b_PartySet;
        SH_CALL(PartySet_LoadSecond)(7, 4, 2, 0);
        if (Scena15_Bytes[2] != AreaW0b_PartySet) {
            Scena15_Bytes[2] = AreaW0b_PartySet;
            AreaW0b_PartySet = 0xFF;
        } else {
            Scena15_Bytes[2] = 0xFF;
        }
        Timer(0x3C);
        Step(0xB);
        return;
    case 0xB: {
        if (W(::bof3::Img(kTimer)) != 0) {
            Timer(W(::bof3::Img(kTimer)) - 1u);
            return;
        }
        if (!LoadDone()) return;
        const unsigned char kept = Scena15_Bytes[2];
        if (kept != 0xFF) {
            AreaW0b_PartySet = kept;
            CallA(0);
            AreaW0b_PartySet = 0xFF;
            SH_CALL(PartySet_LoadFirst)(7, 4, 2);
            AreaW0b_PartySet = Scena15_Bytes[2];
        } else {
            CallA(0);
        }
        DropIn(8);
        SH_CALL(Scena15_EventObjects)();
        CounterUp();
        SH_CALL(Music_Play)(0xA2, 8);
        unsigned char* const e = Effect(Scena15_Bytes[3]);
        Step(0xC);
        e[1] = static_cast<unsigned char>(e[1] + 1);
        return;
    }
    case 0xC:
        if (B(::bof3::Img(kCounter0)) != 0x2B) return;
        if (!LoadDone()) return;
        Set(9);
        ChangeArea(0xC4, 0x120000, 0x140000, 0x81);
        AreaW1f_MusicCurrent = 0x95;
        Step(0xD);
        return;
    case 0xD:
        if (B(::bof3::Img(kCounter0)) != 1) return;
        ChangeArea(0x2D, 0xF8000, 0x358000, 0x87);
        Area_EnterTransition = 1;
        AreaW1f_MusicCurrent = 0xA2;
        Step(0xE);
        return;
    case 0xE:
        if (B(::bof3::Img(kCounter0)) != 2) return;
        ChangeArea(0xAE, 0x320000, 0x270000, 0x89);
        ModeFlow_AreaTransition = 0;
        AreaW1f_MusicCurrent = 0xA2;
        Step(0xF);
        return;
    case 0xF:
        if (B(::bof3::Img(kCounter0)) != 0xD) return;
        Set(0xA);
        ChangeArea(0x98, 0x180000, 0x270000, 0x80);
        Area_EnterTransition = 1;
        AreaW1f_MusicCurrent = 0xA2;
        Step(0x10);
        return;
    case 0x10:
        if (B(::bof3::Img(kCounter0)) != 0) return;
        Set(0xA);
        ChangeArea(0xAE, 0x2F8000, 0x278000, 0x8A);
        ModeFlow_AreaTransition = 0;
        Area_EnterTransition = 0xFF;
        AreaW1f_MusicCurrent = 0xFF;
        Step(0x11);
        return;
    case 0x11:
        if (Transition_Busy != 0) return;
        Say(0x1E);
        Step(0x12);
        return;
    case 0x12:
        if (Field_Request == 2) return;
        if (!LoadDone()) return;
        SH_CALL(Music_Play)(0xA2, 8);
        Draw_PassFlags = 0x1F;
        Transition(1);
        B(::bof3::Img(kCounter0)) = 1;
        Step(0x13);
        return;
    case 0x13:
        if (B(::bof3::Img(kCounter0)) != 0x14) return;
        B(::bof3::Img(kCounter0)) = 0x15;
        Timer(0x5A);
        Step(0x14);
        return;
    case 0x14:
        if (B(::bof3::Img(kCounter0)) != 0x26) return;
        if (!TimerDone()) return;
        SH_CALL(Scena15_PlaceEffect)(0x13, -0x2AA, static_cast<std::uint16_t>(Camera_Angles[1]), 0x5E, 0x40, 0);
        Step(0x15);
        return;
    case 0x15:
        if (B(::bof3::Img(kCounter0)) != 0x2B) return;
        SH_CALL(Scena15_PlaceEffect)(0x13, -0x2AA, static_cast<std::uint16_t>(Camera_Angles[1]), 0x200, 0x28, 0);
        Step(0x16);
        return;
    case 0x16:
        if (B(::bof3::Img(kCounter0)) != 0x3C) return;
        SH_CALL(Music_LoadFile)(0x79);
        Step(0x17);
        return;
    case 0x17:
        if (B(::bof3::Img(kCounter0)) != 0x3F) return;
        if (!LoadDone()) return;
        CounterUp();
        Step(0x18);
        return;
    case 0x18:
        if (B(::bof3::Img(kCounter0)) != 0) return;
        Clear40();
        Set(0xB);
        EndRun();
        return;
    default: return;
    }
}

// original 0x569730: the event objects of run 4: for each of the three
// EventOp_0x records at 0x661950 (0x11 bytes each) and the two EventOp_6x
// records at 0x661988 (0x10 each), the first free Sprite_Objects record
// (0x57CD90, 0xFF none) into the word 0x903850 and, with one, the op run on
// the record.
SC15_EXPORT void __cdecl Scena15_EventObjects(void) {
    for (unsigned i = 0; i < 3; ++i) {
        const unsigned char s = SH_AT(unsigned char (__cdecl*)(), kEventSlot.v)();
        kSlotWord = s;
        if (s != 0xFF) SH_CALL(EventOp_0x)(AtScenaSc15(::bof3::Img(Scena15_EventOps_addr) + 0x11u * i));
    }
    for (unsigned i = 0; i < 2; ++i) {
        const unsigned char s = SH_AT(unsigned char (__cdecl*)(), kEventSlot.v)();
        kSlotWord = s;
        if (s != 0xFF) SH_CALL(EventOp_6x)(AtScenaSc15(::bof3::Img(Scena15_EventOps_addr) + 0x38 + 0x10u * i));
    }
}

// original 0x5697A0: run 5, steps 0, 5, 6, 8, 0xA..0x16, 0x18, 0x19, 0x1E..0x24
// (a 37-entry jump table; the rest nothing): the party dropped in, an
// effect, a pass bit off, a file and an effect of kind 0x1D on the ground at
// (0x2F8000, 0x278000), area 0xAC, a message, the CLUT strip faded to grey
// over 0x20 frames (Scena15_ClutToGrey), the music out, a message and an
// effect of kind 0xA5; on a held button (Input_Held bit 11) a transition and,
// once the wait word is 0, the sound stopped and the task restarted at
// Boot_Task. Steps 0x1E.. find the object whose record word is 0x308 and
// hand it on, then a message, call-table entry 1, the request 6 and area 0xC6.
SC15_EXPORT void __cdecl Scena15_Run5(void) {
    switch (MoveScript_PartyRecords[0x65]) {
    case 0:
        Clear40();
        DropIn(0xF);
        EndRun();
        return;
    case 5:
        DropIn(0x10);
        Step(6);
        return;
    case 6: {
        if (B(::bof3::Img(kCounter0)) != 1) return;
        const unsigned char s = SH_CALL(Effect_FindFree)();
        Scena15_Bytes[3] = s;
        if (s == 0xFF) return;
        Step(7);
        unsigned char* const e = Effect(s);
        e[0] = 1;
        e[5] = 0xA4;
        return;
    }
    case 8:
        Draw_PassFlags = static_cast<unsigned char>(Draw_PassFlags & 0xFB);
        CounterUp();
        Step(9);
        return;
    case 0xA:
        SH_CALL(Music_LoadFile)(0x82);
        DropIn(0x11);
        Step(0xB);
        return;
    case 0xB: {
        if (B(::bof3::Img(kCounter0)) != 0x15) return;
        if (!LoadDone()) return;
        const unsigned char s = SH_CALL(Effect_FindFree)();
        if (s != 0xFF) {
            unsigned char* const e = Effect(s);
            e[0] = 1;
            e[5] = 0x1D;
            rv::As<records::EffectObject>(e)->x = 0x2F8000;
            rv::As<records::EffectObject>(e)->z = 0x278000;
            const long z = static_cast<long>(static_cast<uint32_t>(rv::As<records::EffectObject>(e)->z));
            const long x = static_cast<long>(static_cast<uint32_t>(rv::As<records::EffectObject>(e)->x));
            const long elev = SH_CALL(AreaMap_Elevation)(x, z);
            rv::As<records::EffectObject>(e)->height = static_cast<uint32_t>(static_cast<int32_t>(static_cast<short>(elev)) << 16);
        }
        SH_CALL(Music_Play)(0x82, 8);
        Timer(0x3C);
        Step(0xC);
        return;
    }
    case 0xC:
        if (!TimerDone()) return;
        CounterUp();
        Step(0xD);
        return;
    case 0xD:
        if (B(::bof3::Img(kCounter0)) != 0x1F) return;
        Timer(0x3C);
        Step(0xE);
        return;
    case 0xE:
        if (!TimerDone()) return;
        Set(0x12);
        ChangeArea(0xAC, 0x178000, 0x210000, 0x8C);
        Area_EnterTransition = 0xFF;
        Step(0xF);
        return;
    case 0xF:
        if (Transition_Busy != 0) return;
        Say(0x2B);
        Step(0x10);
        return;
    case 0x10:
        if (Field_Request == 2) return;
        Draw_PassFlags = 0x1F;
        Transition(1);
        CounterUp();
        Step(0x11);
        return;
    case 0x11:
        if (B(::bof3::Img(kCounter0)) != 3) return;
        if (W(::bof3::Img(kTimer)) < 0x20) {
            SH_CALL(Scena15_ClutToGrey)(0);
            Timer(W(::bof3::Img(kTimer)) + 1u);
            return;
        }
        SH_CALL(Scena15_ClutToGrey)(1);
        SH_CALL(Music_FadeOut)(8);
        Timer(0);
        Step(0x12);
        return;
    case 0x12:
        if (B(::bof3::Img(kCounter0)) != 4) return;
        SH_CALL(Music_FadeOutStop)(0xA);
        Transition(0xD);
        Step(0x13);
        return;
    case 0x13:
        if (Transition_Busy != 0) return;
        Draw_PassFlags = 0;
        Timer(0x3C);
        Step(0x14);
        return;
    case 0x14:
        if (!TimerDone()) return;
        Say(0x2D);
        Step(0x15);
        return;
    case 0x15:
        if (Field_Request == 2) return;
        Timer(0x3C);
        Step(0x16);
        return;
    case 0x16:
        if (!TimerDone()) return;
        NewEffectKept(0xA5);
        Step(0x17);
        return;
    case 0x18:
        if ((L(::bof3::Img(kInputHeld)) & 0x800u) == 0) return;
        Transition(0xD);
        Step(0x19);
        return;
    case 0x19:
        if (Transition_Busy != 0) return;
        SH_AT(VoidFn, kSoundStop.v)();
        SH_CALL(Task_Restart)(reinterpret_cast<void*>(static_cast<std::uintptr_t>(::bof3::Code(kBootTask.v))));
        return;
    case 0x1E:
        DropIn(0x12);
        SH_CALL(Music_LoadFile)(0xA2);
        Step(0x1F);
        return;
    case 0x1F:
        if (B(::bof3::Img(kCounter0)) != 0x2A) return;
        for (unsigned k = 0; k < 0x1E; ++k) {
            if ((SH_CALL(Scena15_RecordWord)(rv::As<records::SpriteObject>(Sprite(k))->column) & 0xFFFFu) != 0x308) continue;
            unsigned char* const o = Sprite(k);
            o[0x83] = 0x24;
            rv::As<records::SpriteObject>(o)->script_position = 0;
            break;
        }
        Set(0x11);
        Step(0x20);
        return;
    case 0x20:
        if (B(::bof3::Img(kCounter0)) != 0x31) return;
        Transition(0);
        Step(0x21);
        return;
    case 0x21:
        if (Transition_Busy != 0) return;
        Draw_PassFlags = 0;
        Say(0x65);
        Step(0x22);
        return;
    case 0x22:
        if (Field_Request == 2) return;
        CallA(1);
        Field_Request = 6;
        Step(0x23);
        return;
    case 0x23:
        if (Field_Request != 0) return;
        Field_Request = 1;
        ScenaSc6_Load10 = 1;
        Step(0x24);
        return;
    case 0x24:
        if (Field_Request != 0) return;
        ChangeArea(0xC6, 0x160000, 0x120000, 0x80);
        ModeFlow_AreaTransition = 0xFF;
        Area_EnterTransition = 0xFF;
        AreaW1f_MusicCurrent = 0xFF;
        Scena15_Bytes[1] = 0;
        MoveScript_Var7 = 6;
        Step(0);
        return;
    default: return;
    }
}

// original 0x569DA0: every CLUT strip row but 0xB (32 rows of 0x100 colours
// from Gfx_ClutStrip) toward grey: each colour's grey is (r + g + b) / 3;
// with `snap` (a byte) not 0 the colour becomes that grey (bit 15 kept),
// else each channel steps one toward it. Then Gfx_ClutStripDirty = 1.
SC15_EXPORT void __cdecl Scena15_ClutToGrey(unsigned snap) {
    const bool to_grey = static_cast<unsigned char>(snap) != 0;
    for (unsigned row = 0; row < 0x20; ++row) {
        if (row == 0xB) continue;
        unsigned char* p = AtScenaSc15(::bof3::Img(Gfx_ClutStrip_addr) + row * 0x200u);
        for (unsigned i = 0; i < 0x100; ++i, p += 2) {
            const uint32_t c = W(p);
            const uint32_t r = c & 0x1F, g = (c >> 5) & 0x1F, b = (c >> 10) & 0x1F, top = c & 0x8000;
            const uint32_t grey = static_cast<uint32_t>(static_cast<int32_t>(r + g + b) / 3);
            if (to_grey) {
                SetW(p, ((grey << 5 | grey) << 5 | grey) | top);
                continue;
            }
            const uint32_t r2 = Toward(r, grey), g2 = Toward(g, grey), b2 = Toward(b, grey);
            SetW(p, (((b2 << 5) | g2) << 5 | top) | r2);
        }
    }
    Gfx_ClutStripDirty = 1;
}

// original 0x569F20: run 6, steps 0..0x36 (a 55-entry jump table; 0xE
// nothing), and after every step - whichever it was - the camera shake
// (Scena15_Shake with the byte 0x6BC740). Two files, the music, eight
// effects, the party placed (0x532ED0), event battle 0x37, a pause of random
// length (Scena15_RandomPause) with the rumble effect, messages 0xA, 0x6B and
// 0xC, eight area changes between 0xAE, 0xC6, 0xAD and 0x95, a camera pulled
// back past 0x780, the view shift 0x423380 each frame on step 0x33, and at
// the end area 0xC7 at (0xB60000, 0x128000), Field_ScriptFlags bit 0x0800 off,
// the run ended and Field_StatusBits |= 0x80 (0x56D6F0).
SC15_EXPORT void __cdecl Scena15_Run6(void) {
    auto* const kind2 = &Sprite_Kind2;
    switch (MoveScript_PartyRecords[0x65]) {
    case 0:
        if (Transition_Busy != 0) break;
        Draw_PassFlags = 0xFB;
        Transition(1);
        {
            const unsigned char c = static_cast<unsigned char>(B(::bof3::Img(kCounter0)) + 1);
            Step(1);
            B(::bof3::Img(kCounter0)) = c;
        }
        SH_CALL(LoadDatFile)(0x304);
        break;
    case 1:
        if (!LoadDone()) break;
        if (B(::bof3::Img(kCounter0)) != 5) break;
        SH_CALL(Music_LoadFile)(0x5F);
        Step(2);
        break;
    case 2:
        if (B(::bof3::Img(kCounter0)) != 8) break;
        NewEffect(0xA9);
        Step(3);
        break;
    case 3:
        if (B(::bof3::Img(kCounter0)) != 9) break;
        Timer(0x1E);
        Step(4);
        break;
    case 4:
        if (!TimerDone()) break;
        Step(5);
        CounterUp();
        break;
    case 5:
        if (B(::bof3::Img(kCounter0)) != 0xB) break;
        if (!LoadDone()) break;
        if (SH_CALL(Scena15_PlaceEffect)(0x31, static_cast<std::uint16_t>(Camera_Angles[0]), static_cast<std::uint16_t>(Camera_Angles[1]), W(::bof3::Img(kAngle2)), 0x20, 0x380) == 0) break;
        SH_CALL(Music_Play)(0x5F, 8);
        Step(6);
        break;
    case 6:
        if (KeptLive()) break;
        Timer(0x1E);
        Step(7);
        break;
    case 7:
        if (!TimerDone()) break;
        NewEffect(0xA8);
        Draw_PassFlags = 0x1F;
        SH_AT(void (__cdecl*)(long, long, unsigned), kPartyPlace.v)(0x170000, 0x120000, 0x22);
        Step(8);
        break;
    case 8:
        Step(9);
        CounterUp();
        break;
    case 9:
        if (B(::bof3::Img(kCounter0)) != 0xD) break;
        Step(0xA);
        break;
    case 0xA:
        if (B(::bof3::Img(kCounter0)) != 0xE) break;
        SH_CALL(Music_FadeOut)(0x10);
        Step(0xB);
        break;
    case 0xB:
        if (B(::bof3::Img(kCounter0)) != 0xF) break;
        NewEffect(0xAA);
        SH_CALL(Music_FadeOutStop)(0xA);
        Timer(4);
        Step(0xC);
        break;
    case 0xC:
        if (!TimerDone()) break;
        if (SH_CALL(Scena15_PlaceEffect)(0x31, static_cast<std::uint16_t>(Camera_Angles[0]), static_cast<std::uint16_t>(Camera_Angles[1]), W(::bof3::Img(kAngle2)), 0x20, 0) == 0) break;
        Step(0xD);
        break;
    case 0xD:
        if (KeptLive()) break;
        SH_CALL(Field_StartEventBattle)(0x37);
        Step(0xE);
        break;
    case 0xF:
        if (!LoadDone()) break;
        B(::bof3::Img(kCounter0)) = 0;
        SH_CALL(LoadDatFile)(0x305);
        DropIn(1);
        Step(0x10);
        break;
    case 0x10:
        if (!LoadDone()) break;
        if (B(::bof3::Img(kCounter0)) != 1) break;
        NewEffect(0xAB);
        Timer(0x3C);
        Step(0x11);
        break;
    case 0x11:
        if (!TimerDone()) break;
        Scena15_Bytes[2] = 0;
        SH_CALL(Scena15_RandomPause)();
        SH_CALL(Sound_PlayEffect)(0x206);
        Timer(0x3C);
        Step(0x12);
        break;
    case 0x12:
        if (TimerDone()) {
            B(::bof3::Img(kCounter0)) = 2;
            Step(0x13);
        }
        SH_CALL(Scena15_RandomPause)();
        break;
    case 0x13:
        if (B(::bof3::Img(kCounter0)) == 0) {
            Timer(0x3C);
            Step(0x14);
        }
        SH_CALL(Scena15_RandomPause)();
        break;
    case 0x14:
        if (TimerDone()) {
            Transition(0);
            Step(0x15);
        }
        SH_CALL(Scena15_RandomPause)();
        break;
    case 0x15:
        if (Transition_Busy != 0) break;
        Draw_PassFlags = 0;
        SH_CALL(Sound_PlayEffect)(0x206);
        Timer(0x78);
        Step(0x16);
        break;
    case 0x16:
        if (!TimerDone()) break;
        Set(0x13);
        ChangeArea(0xAE, 0x350000, 0x270000, 0);
        ModeFlow_AreaTransition = 0xFF;
        AreaW1f_MusicCurrent = 0xFF;
        Step(0x17);
        break;
    case 0x17:
        if (B(::bof3::Img(kCounter0)) != 3) break;
        SH_CALL(Music_LoadFile)(0x5F);
        Scena15_Bytes[1] = 1;
        Scena15_Bytes[0] = 4;
        SH_CALL(Sound_PlayEffect)(0x205);
        Timer(8);
        Step(0x18);
        break;
    case 0x18:
        if (!TimerDone()) break;
        Scena15_Bytes[1] = 0;
        CounterUp();
        Step(0x19);
        break;
    case 0x19:
        if (B(::bof3::Img(kCounter0)) != 7) break;
        SH_CALL(Music_Play)(0x5F, 8);
        Scena15_Bytes[0] = 3;
        Scena15_Bytes[1] = 1;
        Step(0x1A);
        break;
    case 0x1A:
        if (B(::bof3::Img(kCounter0)) != 9) break;
        NewEffect(0xAD);
        Step(0x1B);
        break;
    case 0x1B:
        if (B(::bof3::Img(kCounter0)) != 0xB) break;
        ChangeArea(0xC6, 0x170000, 0x120000, 0x82);
        AreaW1f_MusicCurrent = 0x5F;
        Step(0x1C);
        break;
    case 0x1C:
        if (B(::bof3::Img(kCounter0)) != 1) break;
        NewEffect(0xAE);
        Timer(0xC);
        Step(0x1D);
        break;
    case 0x1D:
        if (!TimerDone()) break;
        Step(0x1E);
        CounterUp();
        break;
    case 0x1E:
        if (B(::bof3::Img(kCounter0)) != 0) break;
        Transition(0);
        Step(0x1F);
        break;
    case 0x1F:
    case 0x23:
    case 0x27:
    case 0x2C: {
        if (Transition_Busy != 0) break;
        const unsigned step = MoveScript_PartyRecords[0x65];
        Draw_PassFlags = 0;
        Say(step == 0x1F ? 0xA : step == 0x23 ? 0x6B : 0xC);
        Step(step + 1);
        break;
    }
    case 0x20:
    case 0x24:
    case 0x28:
    case 0x2D:
        if (Field_Request == 2) break;
        Step(MoveScript_PartyRecords[0x65] + 1u);
        break;
    case 0x21:
        Set(0x14);
        ChangeArea(0xAE, 0x340000, 0x270000, 0x94);
        ModeFlow_AreaTransition = 0xFF;
        AreaW1f_MusicCurrent = 0x5F;
        Step(0x22);
        break;
    case 0x22:
        if (B(::bof3::Img(kCounter0)) != 3) break;
        Transition(0);
        Step(0x23);
        break;
    case 0x25:
    case 0x2E: {
        const unsigned next = MoveScript_PartyRecords[0x65] + 1u; // a constant in the original, stored after the call
        ChangeArea(0xC6, 0x170000, 0x120000, 0x82);
        ModeFlow_AreaTransition = 0xFF;
        AreaW1f_MusicCurrent = 0x5F;
        Step(next);
        break;
    }
    case 0x26:
        if (B(::bof3::Img(kCounter0)) != 1) break;
        B(::bof3::Img(kCounter0)) = 0;
        Transition(0);
        Step(0x27);
        break;
    case 0x29:
        Set(0x15);
        SetAt(::bof3::Img(Cond_Flags_addr) + 0xa0, 0x58);
        ChangeArea(0xAD, 0x2E0000, 0x610000, 0);
        ModeFlow_AreaTransition = 0xFF;
        AreaW1f_MusicCurrent = 0xA1;
        Step(0x2A);
        break;
    case 0x2A:
        if (B(::bof3::Img(kCounter0)) != 1) break;
        Cond_ByteFE = 2;
        Step(0x2B);
        break;
    case 0x2B:
        if (B(::bof3::Img(kCounter0)) != 3) break;
        B(::bof3::Img(kCounter0)) = 0;
        Transition(0);
        Step(0x2C);
        break;
    case 0x2F:
        if (B(::bof3::Img(kCounter0)) != 2) break;
        Scena15_Bytes[0] = 3;
        Step(0x30);
        break;
    case 0x30:
        if (B(::bof3::Img(kCounter0)) != 3) break;
        if (SH_CALL(Scena15_PlaceEffect)(0x31, static_cast<std::uint16_t>(Camera_Angles[0]), static_cast<std::uint16_t>(Camera_Angles[1]), W(::bof3::Img(kAngle2)), 0x80, 0xB00) == 0) break;
        Step(0x31);
        break;
    case 0x31:
        if (Camera_Distance <= 0x780) break;
        Set(0x16);
        ChangeArea(0x95, 0x80000, 0xC0000, 0x82);
        ModeFlow_AreaTransition = 8;
        Area_EnterTransition = 9;
        AreaW1f_MusicCurrent = 0x5F;
        Step(0x32);
        break;
    case 0x32:
        if (Transition_Busy != 0) break;
        Step(0x33);
        break;
    case 0x33:
        if (B(::bof3::Img(kCounter0)) == 0x15) {
            SH_CALL(Music_FadeOut)(8);
            SH_CALL(Sound_PlayEffect)(2);
            Transition(0xF);
            Step(0x34);
        }
        SH_AT(VoidFn, kViewShift.v)();
        break;
    case 0x34:
        if (Transition_Busy != 0) break;
        SH_CALL(Music_FadeOutStop)(0xA);
        Draw_PassFlags = 0;
        Transition(0x10);
        Step(0x35);
        break;
    case 0x35:
        if (Transition_Busy != 0) break;
        Timer(0x96);
        Step(0x36);
        break;
    case 0x36:
        if (!TimerDone()) break;
        ChangeArea(0xC7, 0xB60000, 0x128000, 7);
        Field_ScriptFlags = Field_ScriptFlags & 0xFFF7u;
        EndRun();
        kind2->state = 0;
        SH_AT(VoidFn, kStatusBit80.v)();
        break;
    default: break;
    }
    SH_CALL(Scena15_Shake)(Scena15_Bytes[0]);
}

// original 0x56AD80: with the byte 0x6BC742 at 0: counter 1 = 1 and the byte
// set to ((Rand() % 0x26) + 0x2A) / 2 (signed, as idiv and the halving round
// toward 0); else the byte counted down. Run 6's pause.
SC15_EXPORT void __cdecl Scena15_RandomPause(void) {
    if (Scena15_Bytes[2] != 0) {
        Scena15_Bytes[2] = static_cast<unsigned char>(Scena15_Bytes[2] - 1);
        return;
    }
    B(::bof3::Img(kCounter1)) = 1;
    const int r = SH_CALL(Rand)() % 0x26 + 0x2A;
    B(::bof3::Img(kKept)) = static_cast<unsigned char>(r / 2);
}

// original 0x56ADC0: while the byte 0x6BC741 is set, the camera shakes:
// MapView_Redraw = 2 and Camera_ShiftY += Scena15_ShakeSteps[Frame_Counter &
// 3] (an s8) << the byte `shift` (a 16-bit add).
SC15_EXPORT void __cdecl Scena15_Shake(unsigned shift) {
    if (Scena15_Bytes[1] == 0) return;
    const uint32_t i = Frame_Counter & 3u;
    MapView_Redraw = 2;
    const uint32_t d = static_cast<uint32_t>(static_cast<int32_t>(static_cast<signed char>(B(::bof3::Img(kShakeSteps) + i))))
                       << (shift & 0x1Fu);
    Camera_ShiftY = static_cast<std::uint16_t>(Camera_ShiftY) + d;
}

// original 0x56ADF0: a marker effect of kind 0x9E at (x, z) on the ground:
// +0x34 = x, +0x38 = z, +0x3C = (AreaMap_Elevation(+0x34, z) + 0x200) << 16,
// then +6, +7 and +0xB the three bytes; al 1. No slot: al 0.
SC15_EXPORT unsigned char __cdecl Scena15_SpawnMarker(long x, long z, unsigned six, unsigned seven, unsigned eleven) {
    const unsigned char s = SH_CALL(Effect_FindFree)();
    if (s == 0xFF) return 0;
    unsigned char* const e = Effect(s);
    e[0] = 1;
    e[5] = 0x9E;
    rv::As<records::EffectObject>(e)->x = static_cast<uint32_t>(x);
    rv::As<records::EffectObject>(e)->z = static_cast<uint32_t>(z);
    const long elev = SH_CALL(AreaMap_Elevation)(static_cast<long>(static_cast<uint32_t>(rv::As<records::EffectObject>(e)->x)), z);
    rv::As<records::EffectObject>(e)->height = static_cast<uint32_t>(static_cast<int32_t>(static_cast<short>(elev)) + 0x200) << 16;
    e[6] = static_cast<unsigned char>(six);
    e[7] = static_cast<unsigned char>(seven);
    e[0xB] = static_cast<unsigned char>(eleven);
    return 1;
}

// original 0x56AE80: chapter 15's vtable slot 1, the object hook (PSX
// 0x801FDD80): Scena15_Objects[object +0x86] (object, the flag row).
// Unchecked (12 entries).
SC15_EXPORT void __cdecl Scena15_ObjectTrigger(unsigned char* object) {
    CallObject(::bof3::Img(Scena15_Objects_addr), object, "Scena15_ObjectTrigger");
}

// original 0x56AEA0: Scena15_Objects entries 0..4: a talk. The byte
// Scena15_TalkWho[object +0x86] (put in the low byte of the object argument's
// stack slot, the rest the pointer's) handed to 0x42C0A0 in area 0xC0, else
// to 0x42BA90, whose answer is the script message opened; the request 2.
SC15_EXPORT void __cdecl Scena15_ObjectTalk(unsigned char* object, unsigned char*) {
    const unsigned area = Area();
    const uint32_t who = (Low32(object) & 0xFFFFFF00u) |
                         B(::bof3::Img(kTalkWho) + object[0x86]);
    const unsigned id = area == 0xC0 ? SH_AT(unsigned (__cdecl*)(uint32_t), kTalkIdC0.v)(who)
                                     : SH_AT(unsigned (__cdecl*)(uint32_t), kTalkId.v)(who);
    Say(id);
}

// original 0x56AF00: Scena15_Objects entry 5: the script flags' bit 0x40, run
// 3 at step 0x32.
SC15_EXPORT void __cdecl Scena15_Object05(unsigned char*, unsigned char*) {
    Set40();
    MoveScript_Var7 = 3;
    Step(0x32);
}

// original 0x56AF20: entry 6: flag 0xD, Party_DropIn(0xB), +0x8A counted.
SC15_EXPORT void __cdecl Scena15_Object06(unsigned char* object, unsigned char*) { ObjectFlag(object, 0xD, 0xB); }

// original 0x56AF50: entry 7: flag 0xE, Party_DropIn(0xC).
SC15_EXPORT void __cdecl Scena15_Object07(unsigned char* object, unsigned char*) { ObjectFlag(object, 0xE, 0xC); }

// original 0x56AF80: entry 8: flag 0xC, no member.
SC15_EXPORT void __cdecl Scena15_Object08(unsigned char* object, unsigned char*) { ObjectFlag(object, 0xC, 0xFF); }

// original 0x56AFB0: entry 9: flag 0x10, Party_DropIn(0xE).
SC15_EXPORT void __cdecl Scena15_Object09(unsigned char* object, unsigned char*) { ObjectFlag(object, 0x10, 0xE); }

// original 0x56AFE0: entry 10: flag 0xF, Party_DropIn(0xD).
SC15_EXPORT void __cdecl Scena15_Object10(unsigned char* object, unsigned char*) { ObjectFlag(object, 0xF, 0xD); }

// original 0x56B010: entry 11: the script flags' bit 0x40 and the object's
// word +0x8A counted.
SC15_EXPORT void __cdecl Scena15_Object11(unsigned char* object, unsigned char*) {
    Set40();
    rv::As<records::SpriteObject>(object)->script_position = rv::As<records::SpriteObject>(object)->script_position + 1u;
}

// original 0x56B030: chapter 15's vtable slot 2, the step hook (PSX
// 0x801FE024), (x, z) 16.16: in area 0xAD, with Cond_ByteFD 1 and flag 1
// clear, z above 0x470000 starts run 3 at step 0xF; with Cond_ByteFD 2 (read
// again) and story flag 0x81 clear, x's cell above 0x2C and z's cell
// 0x78..0x7F run 3 at step 0 - neither answers. In area 0xAE with Cond_ByteFD
// 0, x exactly 0x418000 and z's cell 0x22..0x24: run 4 at step 0, al 1. Else
// al 0. The cells are the high words (x's signed, z's as a u16 compare).
SC15_EXPORT unsigned char __cdecl Scena15_StepHook(long x, long z) {
    const unsigned zcell = static_cast<uint32_t>(z) >> 16;
    if (Area() == 0xAD) {
        if (Cond_ByteFD == 1 && !Test(1) && z > 0x470000) {
            Set40();
            MoveScript_Var7 = 3;
            Step(0xF);
        }
        if (Cond_ByteFD == 2 && !TestAt(::bof3::Img(Cond_Flags_addr) + 0xa0, 0x81) && static_cast<short>(static_cast<uint32_t>(x) >> 16) > 0x2C &&
            static_cast<std::uint16_t>(zcell - 0x78) < 8) {
            Set40();
            MoveScript_Var7 = 3;
            Step(0);
        }
    }
    if (Area() == 0xAE && Cond_ByteFD == 0 && x == 0x418000 && static_cast<std::uint16_t>(zcell - 0x22) < 3) {
        Set40();
        MoveScript_Var7 = 4;
        Step(0);
        return 1;
    }
    return 0;
}

// original 0x56B100: chapter 15's vtable slot 3, the arrive hook (PSX
// 0x801FE18C), (x, z) 16.16, always al 0. Only in area 0xAE, by the first of
// the chapter's flags 2, 4 and 5 still clear: flag 2 - x at least 0x440000
// and z's cell 3..4: run 3 step 0x14; flag 4 - x at most 0x530000 and z's
// cell 0x2E..0x30: run 3 step 0x1E; flag 5 - z at most 0x3A0000 and x's cell
// 0x23..0x26: run 3 step 0x28; all three set - x exactly 0x310000 and z's
// cell 0x27..0x28: run 5 at step 5 when flags 0xC, 0xE, 0xD, 0xF and 0x10 are
// all set, else at step 0. Each with the script flags' bit 0x40.
SC15_EXPORT unsigned char __cdecl Scena15_ArriveHook(long x, long z) {
    if (Area() != 0xAE) return 0;
    const std::uint16_t xcell = static_cast<std::uint16_t>(static_cast<uint32_t>(x) >> 16);
    const std::uint16_t zcell = static_cast<std::uint16_t>(static_cast<uint32_t>(z) >> 16);
    if (!Test(2)) {
        if (x < 0x440000) return 0;
        if (static_cast<std::uint16_t>(zcell - 3) >= 2) return 0;
        Set40();
        MoveScript_Var7 = 3;
        Step(0x14);
        return 0;
    }
    if (!Test(4)) {
        if (x > 0x530000) return 0;
        if (static_cast<std::uint16_t>(zcell - 0x2E) >= 3) return 0;
        Set40();
        MoveScript_Var7 = 3;
        Step(0x1E);
        return 0;
    }
    if (!Test(5)) {
        if (z > 0x3A0000) return 0;
        if (static_cast<std::uint16_t>(xcell - 0x23) >= 4) return 0;
        Set40();
        MoveScript_Var7 = 3;
        Step(0x28);
        return 0;
    }
    if (x != 0x310000) return 0;
    if (static_cast<std::uint16_t>(zcell - 0x27) >= 2) return 0;
    if (Test(0xC) && Test(0xE) && Test(0xD) && Test(0xF) && Test(0x10)) {
        Set40();
        MoveScript_Var7 = 5;
        Step(5);
        return 0;
    }
    Set40();
    MoveScript_Var7 = 5;
    Step(0);
    return 0;
}

void Scena152_Inject() {
    BOF3_INJECT(Scena15_Frame);
    BOF3_INJECT(Scena15_EnterArea);
    BOF3_INJECT(Scena15_BattleSetup);
    BOF3_INJECT(Scena15_PlaceEffect);
    BOF3_INJECT(Scena15_Run);
    BOF3_INJECT(Scena15_Run1);
    BOF3_INJECT(Scena15_Run2);
    BOF3_INJECT(Scena15_Run3);
    BOF3_INJECT(Scena15_Run4);
    BOF3_INJECT(Scena15_EventObjects);
    BOF3_INJECT(Scena15_Run5);
    BOF3_INJECT(Scena15_ClutToGrey);
    BOF3_INJECT(Scena15_Run6);
    BOF3_INJECT(Scena15_RandomPause);
    BOF3_INJECT(Scena15_Shake);
    BOF3_INJECT(Scena15_SpawnMarker);
    BOF3_INJECT(Scena15_ObjectTrigger);
    BOF3_INJECT(Scena15_ObjectTalk);
    BOF3_INJECT(Scena15_Object05);
    BOF3_INJECT(Scena15_Object06);
    BOF3_INJECT(Scena15_Object07);
    BOF3_INJECT(Scena15_Object08);
    BOF3_INJECT(Scena15_Object09);
    BOF3_INJECT(Scena15_Object10);
    BOF3_INJECT(Scena15_Object11);
    BOF3_INJECT(Scena15_StepHook);
    BOF3_INJECT(Scena15_ArriveHook);
}

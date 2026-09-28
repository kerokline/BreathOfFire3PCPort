// Scenario chapters 15..19 (the PSX's SCENA15..SCENA19.EMI), compiled into the
// exe at 0x567DC0..0x56B2A0 with 0x537580 (chapter 15), 0x56C080 (chapter 16's
// object hook, the one of its slots the title-states work left) and
// 0x56C130..0x56D5E0 (chapters 17, 18 and 19). docs/scena_sc15.md.
//
//   - Chapter 15, vtable Scena15_Hooks: the frame jumps through Scena15_States
//     on the s8 0x8034E2 (0 is chapter 13/14's 0x5646B0, state 1; 1
//     Scena15_EnterArea, the set-up of the area entered, state 2; 2
//     Scena15_Run, which jumps through Scena15_Runs on MoveScript_Var7 to six
//     runs). Each run is a step machine on the u8 0x8034E5 (a switch in .text,
//     bounded) that waits on the script counters 0x903848.., the message
//     request, the wait word, a file or an effect, calls two or three engine
//     functions and sets the next step. The object hook calls
//     Scena15_Objects[object +0x86] (object, row); the step and arrive hooks
//     answer in al.
//   - Chapter 16's object hook calls Scena16_Objects[object +0x86].
//   - Chapter 17 (the staff roll): the frame's states 0 and 1 set up area 0xC7;
//     state 2 jumps through Scena17_Parts on the s8 0x8034E3 to three parts,
//     each a table of steps on MoveScript_Var7 (Scena17_Part1Steps,
//     Part2Steps, Part3Steps). The parts draw: a fade, a disc of rays, a
//     letterbox, the scrolling roll of names from Scena17_RollLines in a
//     font of Scena17_GlyphWidths. The last step restarts the task at
//     Scena17_EndTask, which runs Scena17_EndSteps on Game_Step: the story
//     flags and a return to chapter 15's area 0x8F, then a restart to Boot_Task.
//   - Chapters 18 and 19: a frame, a run table of one entry, an object table of
//     one bare ret; 19's one run places an effect on a key.
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies, and every
// cell is read where and when the original reads it. The .data tables are
// read in place and unchecked as the original reads them; where the original
// would jump to what is not code, ours aborts (CodeAt). No divergence: each
// function is a faithful replacement; the latent defects are described in the
// doc, section 7.
#include "game/scena_sc15.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/scena_sc15_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

#define SC15_EXPORT extern "C" __attribute__((disable_tail_calls))

namespace {

using namespace scena_sc15;
using std::int32_t;
using std::uint32_t;
using VoidFn = void (__cdecl*)();
using ObjectFn = void (__cdecl*)(unsigned char*, unsigned char*);

unsigned char* At(uint32_t a) { return move_script::At(a); }
unsigned char& B(uint32_t a) { return At(a)[0]; }
std::uint16_t W(uint32_t a) { return move_script::Word(At(a)); }
void SetW(uint32_t a, unsigned v) { move_script::SetWord(At(a), v); }
uint32_t L(uint32_t a) { return static_cast<uint32_t>(move_script::Long(At(a))); }
uint32_t L(const unsigned char* p) { return static_cast<uint32_t>(move_script::Long(p)); }
void SetL(uint32_t a, uint32_t v) { move_script::SetLong(At(a), static_cast<int32_t>(v)); }
void SetL(unsigned char* p, uint32_t v) { move_script::SetLong(p, static_cast<int32_t>(v)); }
std::uint16_t W(const unsigned char* p) { return move_script::Word(p); }
void SetW(unsigned char* p, unsigned v) { move_script::SetWord(p, v); }

// The chapter bytes (docs/scenario-roots.md section 2).
constexpr uint32_t kChapter = 0x8034E0;     // Cond_ByteFA
constexpr uint32_t kStatus = 0x8034E1;      // Field_StatusBits
constexpr uint32_t kState = 0x8034E2;       // s8, the frame's state
constexpr uint32_t kPart = 0x8034E3;        // s8, chapter 17's part (Scena17_Parts)
constexpr uint32_t kRun = 0x8034E4;         // MoveScript_Var7: the run (15, 18, 19), the part's step (17)
constexpr uint32_t kStep = 0x8034E5;        // u8, the run's step
constexpr uint32_t kTimer = 0x8034E6;       // u16
constexpr uint32_t kByteFD = 0x8034F1;      // Cond_ByteFD
// The script counters (MoveScript_CounterOps) and the chapters' other cells.
constexpr uint32_t kCounter0 = 0x903848;
constexpr uint32_t kCounter1 = 0x903849;
constexpr uint32_t kCounter2 = 0x90384A;
constexpr uint32_t kCounter3 = 0x90384B;
constexpr uint32_t kSlotWord = 0x903850;    // u16: the event object's slot (Scena15_EventObjects)
constexpr uint32_t kFlagRow = 0x929ED0;     // the flag bits Flags_Test / Flags_Set are given
constexpr uint32_t kStoryRow = 0x904030;    // Cond_Flags + 0xA0, the story flags
constexpr uint32_t kRow14 = 0x904000;       // Cond_Flags + 0x70
constexpr uint32_t kRow3 = 0x904008;        // Cond_Flags + 0x78 (chapter 15's row is 0x904008)
constexpr uint32_t kWait = 0x66C810;        // MoveScript_WaitWordDA
constexpr uint32_t kRequest = 0x66C7D8;     // Field_Request
constexpr uint32_t kArea = 0x904EFC;        // Game_AreaNumber
constexpr uint32_t kPass = 0x7E0918;        // Draw_PassFlags
constexpr uint32_t kEffects = 0x7E11E0;     // Effect_Objects, 20 records of 0x80
constexpr uint32_t kSprites = 0x7DEE80;     // Sprite_Objects, 30 records of 0xA4
constexpr uint32_t kDistance = 0x903840;    // Camera_Distance (s16)
constexpr uint32_t kShiftY = 0x903802;      // Camera_ShiftY
constexpr uint32_t kAngle0 = 0x929EC8;      // Camera_Angles[0]
constexpr uint32_t kAngle1 = 0x929ECA;      // Camera_Angles[1]
constexpr uint32_t kAngle2 = 0x929ECC;      // Cond_AngleFB's word
constexpr uint32_t kRedraw = 0x905E69;      // MapView_Redraw
constexpr uint32_t kKind2Z = 0x905E60;      // Field_Kind2Z
constexpr uint32_t kKind2X = 0x905E64;      // Field_Kind2X
constexpr uint32_t kByteFE = 0x905E20;      // Cond_ByteFE
constexpr uint32_t kMusicTrack = 0x904131;  // Music_Track
constexpr uint32_t kAreaMusic = 0x904CD0;   // the byte the area changes set with the track to play
constexpr uint32_t kByteEE0 = 0x904EE0;     // set with some area changes
constexpr uint32_t kByte7F98 = 0x937F98;
constexpr uint32_t kPartyByte = 0x90412C;
constexpr uint32_t kFrame = 0x937F94;       // Frame_Counter
constexpr uint32_t kInputHeld = 0x7E1BE8;   // Input_Held (u16; read as a dword by run 5 step 0x18)
constexpr uint32_t kInputPressed = 0x7E1BEC;   // Input_Pressed
constexpr uint32_t kHold = 0x929F12;        // Field_Kind2Hold
constexpr uint32_t kElevation = 0x929F1C;   // MapView_Elevation (long)
constexpr uint32_t kF3Divisor = 0x937F8C;   // MoveScript_F3Divisor
constexpr uint32_t kFAWord = 0x904EFE;      // MoveScript_FAWord
constexpr uint32_t kPacketNext = 0x7E0670;  // Gfx_PacketNext
constexpr uint32_t kClutStrip = 0x80F580;   // Gfx_ClutStrip: 32 rows of 0x100 colours
constexpr uint32_t kClutDirty = 0x937F90;   // Gfx_ClutStripDirty
constexpr uint32_t kGameStep = 0x66C7EA;    // Game_Step
constexpr uint32_t kVertices = 0x9037A0;    // Prim_VertexScratch

// Chapter 15's and 17's own bytes (Scena15_Bytes 0x6BC740, read by no other code).
constexpr uint32_t kShakeShift = 0x6BC740;  // the camera shake's shift (Scena15_Shake's argument)
constexpr uint32_t kShakeOn = 0x6BC741;     // the camera shakes while set
constexpr uint32_t kKept = 0x6BC742;        // run 4: the party byte kept; run 6: Scena15_RandomPause's count
constexpr uint32_t kSlot = 0x6BC743;        // the effect slot a run waits on
constexpr uint32_t kRollLine = 0x6BC744;    // u16: the roll's first line (Scena17_RollLines)
constexpr uint32_t kRollScroll = 0x6BC746;  // the roll's scroll within a line, 0..0x15
constexpr uint32_t kRollSkip = 0x6BC747;    // the roll skipped (Input_Pressed 0x20)

// Chapter 15's tables.
constexpr uint32_t kStates15 = 0x661924;    // Scena15_States, 3
constexpr uint32_t kRuns15 = 0x661930;      // Scena15_Runs, 7
constexpr uint32_t kBattles = 0x6618C0;     // Scena15_Battles: 10 records of 10 bytes
constexpr uint32_t kOpRecords0 = 0x661950;  // Scena15_EventOps: three EventOp_0x records of 0x11
constexpr uint32_t kOpRecords6 = 0x661988;  // Scena15_EventOps6: two EventOp_6x records of 0x10
constexpr uint32_t kShakeSteps = 0x6619A8;  // Scena15_ShakeSteps: four s8
constexpr uint32_t kObjects15 = 0x6619AC;   // Scena15_Objects, 12
constexpr uint32_t kTalkWho = 0x6619DC;     // Scena15_TalkWho: five bytes
constexpr uint32_t kObjects16 = 0x661A1C;   // Scena16_Objects, 1
// Chapter 17's.
constexpr uint32_t kRollLines = 0x661A20;   // Scena17_RollLines: string pointers, -1 at 351
constexpr uint32_t kStates17 = 0x661FEC;    // Scena17_States, 3
constexpr uint32_t kParts17 = 0x661FF8;     // Scena17_Parts, 4
constexpr uint32_t kPart1Steps = 0x662008;  // Scena17_Part1Steps, 16
constexpr uint32_t kPart2Steps = 0x662048;  // Scena17_Part2Steps, 3
constexpr uint32_t kPart3Steps = 0x662054;  // Scena17_Part3Steps, 13
constexpr uint32_t kObjects17 = 0x662088;   // Scena17_Objects, 1
constexpr uint32_t kGlyphWidths = 0x66208C; // Scena17_GlyphWidths: 0x40 bytes
constexpr uint32_t kEndSteps = 0x6620CC;    // Scena17_EndSteps, 3
// Chapters 18's and 19's.
constexpr uint32_t kStates18 = 0x662C44;    // Scena18_States, 3
constexpr uint32_t kRuns18 = 0x662C50;      // Scena18_Runs, 1
constexpr uint32_t kObjects18 = 0x662C54;   // Scena18_Objects, 1
constexpr uint32_t kStates19 = 0x662C6C;    // Scena19_States, 3
constexpr uint32_t kRuns19 = 0x662C78;      // Scena19_Runs, 1
constexpr uint32_t kObjects19 = 0x662C7C;   // Scena19_Objects, 1

constexpr uint32_t kBootTask = 0x496B60;    // Boot_Task's address, as the original pushes it
constexpr uint32_t kEndTask = 0x56D3B0;     // Scena17_EndTask's, likewise

constexpr uint32_t kTextLo = 0x401000, kTextHi = 0x5C3000;

// A handler out of one of the chapters' .data tables, read in place and
// unchecked as the original reads it; where the original would jump to what
// is not code (an index past its table), ours aborts. During the fuzz the
// tables hold the harness's recorders, outside .text.
uint32_t CodeAt(uint32_t table, int index, const char* who) {
    const uint32_t at = table + 4u * static_cast<uint32_t>(index);
    const uint32_t entry = L(at);
    if (!scenario_harness::g_active && (entry < kTextLo || entry >= kTextHi))
        bof3::Fatal("%s: index %d reads 0x%X at 0x%X, not code - past its table (the original jumps there)", who, index,
                    (unsigned)entry, (unsigned)at);
    return entry;
}
VoidFn Handler(uint32_t table, int index, const char* who) {
    return reinterpret_cast<VoidFn>(static_cast<std::uintptr_t>(CodeAt(table, index, who)));
}
// An object hook's handler, called (object, the flag row) as 0x56D6D0's slot 1
// calls through a chapter's object table.
void CallObject(uint32_t table, unsigned char* object, const char* who) {
    unsigned char* const row = At(L(kFlagRow));
    reinterpret_cast<ObjectFn>(static_cast<std::uintptr_t>(CodeAt(table, object[0x86], who)))(object, row);
}

// The chapter's flag row, as the original reads it: the pointer afresh.
unsigned char* Row() { return At(L(kFlagRow)); }
bool Test(unsigned n) { return SH_CALL(Flags_Test)(Row(), n) != 0; }
bool TestAt(uint32_t row, unsigned n) { return SH_CALL(Flags_Test)(At(row), n) != 0; }
void Set(unsigned n) { SH_CALL(Flags_Set)(Row(), n); }
void SetAt(uint32_t row, unsigned n) { SH_CALL(Flags_Set)(At(row), n); }
void ClearAt(uint32_t row, unsigned n) { SH_CALL(Flags_Clear)(At(row), n); }

unsigned char* Effect(unsigned slot) { return At(kEffects + ((slot & 0xFFu) << 7)); }
unsigned char* Sprite(unsigned k) { return At(kSprites + (k & 0xFFu) * 0xA4u); }

unsigned Area() { return W(kArea); }
void Step(unsigned v) { B(kStep) = static_cast<unsigned char>(v); }
void Timer(unsigned v) { SetW(kTimer, v); }
// dec word [0x8034E6]: true when it reaches 0.
bool TimerDone() {
    SetW(kTimer, W(kTimer) - 1u);
    return W(kTimer) == 0;
}
void CounterUp() { B(kCounter0) = static_cast<unsigned char>(B(kCounter0) + 1); }
void RunUp() { B(kRun) = static_cast<unsigned char>(B(kRun) + 1); }
// The runs' common end: MoveScript_Var7 and the step 0 (no run).
void EndRun() {
    B(kRun) = 0;
    Step(0);
}

bool LoadDone() { return SH_CALL(File_LoadDone)() != 0; }
void Msg(unsigned id) { SH_CALL(Msg_OpenScript)(static_cast<unsigned short>(id)); }
// A script message opened, the request 2 (a message is open).
void Say(unsigned id) {
    Msg(id);
    B(kRequest) = 2;
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
    B(kSlot) = s;
    if (s != 0xFF) {
        unsigned char* const e = Effect(s);
        e[0] = 1;
        e[5] = kind;
    }
    return s;
}
// The kept slot's record still live (+0 bit 0); the slot read as a byte,
// unchecked (0xFF reads past the 20 records).
bool KeptLive() { return (Effect(B(kSlot))[0] & 1) != 0; }

}  // namespace

// ===========================================================================
// Chapter 15

// original 0x537580: the word at [0x7E0880] + (index & 0xFFFF) * 8 - an
// 8-byte record's first word, the table a pointer in 0x7E0880; eax the word
// zero-extended. Called by area code (0x48A57A, 0x48A59D, testing 0x261 and
// 0x308) and Scena15_Run5 (0x308). Placed by the linker before chapter 0's
// block.
SC15_EXPORT unsigned __cdecl Scena15_RecordWord(unsigned index) {
    return W(L(0x7E0880) + (index & 0xFFFFu) * 8u);
}

// original 0x567DC0: chapter 15's vtable slot 0 (PSX 0x801F9CAC): a tail jump
// through Scena15_States on the s8 0x8034E2 - 0 is 0x5646B0 (state 1, chapter
// 13/14's), 1 Scena15_EnterArea, 2 Scena15_Run. Unchecked.
SC15_EXPORT void __cdecl Scena15_Frame(void) {
    Handler(kStates15, static_cast<signed char>(B(kState)), "Scena15_Frame")();
}

// original 0x567DD0: state 1: the set-up of the area entered, by
// Game_AreaNumber (read afresh at every test), Cond_ByteFD and the chapter's
// flags, then state 2. Areas 0x9E..0xA6 start the event battles 0..9
// (Scena15_BattleSetup) or set a flag; 0xA4 and 0xA6 with Cond_ByteFD not 0,
// and 0xAD with it not 2, go straight to state 2.
SC15_EXPORT void __cdecl Scena15_EnterArea(void) {
    if (Area() == 2 && Test(7) && !Test(8)) B(kPass) = 0;
    if (Area() == 0x2D && Test(9) && !Test(0xA)) {
        SetW(kDistance, 0x5DC);
        B(kRedraw) = 2;
    }
    if (Area() == 0x95 && Test(0x16)) {
        const unsigned a2 = W(kAngle2), a1 = W(kAngle1), a0 = W(kAngle0);
        B(0x802083) = 2;
        SetW(0x80208A, 0);
        SetL(0x802034, L(0x802034) + 0xC0000u);
        B(kByteFE) = 2;
        B(kShakeOn) = 0;
        SetW(kDistance, 0xD00);
        SH_CALL(Scena15_PlaceEffect)(0x31, static_cast<int>(a0), static_cast<int>(a1), static_cast<int>(a2), 0xFF, 0);
    }
    if (Area() == 0x98 && Test(0xA) && !Test(0xB)) {
        SetW(kDistance, 0x5DC);
        B(kRedraw) = 2;
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
        if (B(kByteFD) == 0 && !Test(p.flag0)) {
            if (p.battle0 == 0xFF) Set(p.flag0);
            else SH_CALL(Scena15_BattleSetup)(p.battle0);
        }
        if (B(kByteFD) == 1 && !Test(p.flag1)) {
            if (p.battle1 == 0xFF) Set(p.flag1);
            else SH_CALL(Scena15_BattleSetup)(p.battle1);
        }
    }
    if (Area() == 0xA4) {
        if (B(kByteFD) != 0) {
            B(kState) = 2;
            return;
        }
        if (!Test(0x2C)) SH_CALL(Scena15_BattleSetup)(7);
    }
    if (Area() == 0xA5) {
        if (B(kByteFD) == 0 && !Test(0x2D)) Set(0x2D);
        if (B(kByteFD) == 1 && !Test(0x2E)) SH_CALL(Scena15_BattleSetup)(8);
    }
    if (Area() == 0xA6) {
        if (B(kByteFD) != 0) {
            B(kState) = 2;
            return;
        }
        if (!Test(0x2F)) SH_CALL(Scena15_BattleSetup)(9);
    }
    if (Area() == 0xAC && Test(0x12)) B(kPass) = 0;
    if (Area() == 0xAD) {
        if (B(kByteFD) != 2) {
            B(kState) = 2;
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
            B(kPass) = 0x1F;
        }
    }
    if (Area() == 0xAE) {
        if (Test(2) && !Test(3)) SH_CALL(Scena15_SpawnMarker)(0x440000, 0x20000, 0xC, 6, 1);
        if (Test(0xA) && !Test(0xB)) {
            SH_CALL(Music_LoadFile)(0xA2);
            B(kPass) = 0;
        }
        if (Test(0x13)) {
            if (!Test(0x14)) {
                B(kPass) = 0x1F;
                CallA(2);
                DropIn(0x13);
            } else if (!Test(0x15)) {
                B(kPass) = 0x1F;
            }
        }
    }
    if (Area() == 0xBD && TestAt(kRow14, 1)) B(kEffects) = static_cast<unsigned char>(B(kEffects) | 0x40);
    if (Area() == 0xC6) {
        if (Test(0x13)) {
            NewEffect(0xAB);
            NewEffect(0xB0);
            B(kShakeShift) = 4;
            B(kPass) = 0x1F;
        }
        if (Test(0x14) && !Test(0x15)) B(kPass) = 0x1F;
        if (Test(0x15) && !Test(0x16)) {
            B(kShakeShift) = 4;
            B(kPass) = 0x1F;
        }
    }
    B(kState) = 2;
}

// original 0x568510: event battle n's set-up (areas 0x9E..0xA6): the script
// flags' bit 0x40, counter 0 = 1; an effect of kind 0x89 with +0xB = n (its
// slot in 0x6BC743); the kind-2 sprite at Scena15_Battles[n]'s (+0, +2) << 8
// (the index n & 0xFF, unchecked: ten records); Field_ViewReset; 0x904EF0 = 0,
// Party_DropIn(0); Field_ScriptFlags2 bit 4 off; the party placed at
// (+4, +6) << 8 with kind +9 (0x532ED0); the OT slot 4, run 2 at step 0.
SC15_EXPORT void __cdecl Scena15_BattleSetup(unsigned n) {
    Set40();
    B(kCounter0) = 1;
    const unsigned char s = SH_CALL(Effect_FindFree)();
    B(kSlot) = s;
    if (s != 0xFF) {
        unsigned char* const e = Effect(s);
        e[0] = 1;
        e[5] = 0x89;
        e[0xB] = static_cast<unsigned char>(n);
    }
    const uint32_t rec = kBattles + (n & 0xFFu) * 10u;
    SetL(kKind2X, static_cast<uint32_t>(static_cast<int32_t>(static_cast<short>(W(rec))) << 8));
    SetL(kKind2Z, static_cast<uint32_t>(static_cast<int32_t>(static_cast<short>(W(rec + 2))) << 8));
    SH_CALL(Field_ViewReset)();
    B(0x904EF0) = 0;
    DropIn(0);
    const int32_t z = static_cast<int32_t>(static_cast<short>(W(rec + 6))) << 8;
    const int32_t x = static_cast<int32_t>(static_cast<short>(W(rec + 4))) << 8;
    const unsigned kind = B(rec + 9);
    SetW(0x905BA4, W(0x905BA4) & 0xFFEFu);
    SH_AT(void (__cdecl*)(long, long, unsigned), kPartyPlace)(x, z, kind);
    B(0x92BF19) = 4;   // Draw_OtSlot
    B(kRun) = 2;
    Step(0);
}

// original 0x5685D0: an effect placed: Effect_FindFree into 0x6BC743; with a
// slot, +0 = 1, +5 = kind, +0x64 / +0x68 / +0x6C the three s16 x, y, z, +0xC
// the dword, +9 the byte; al 1. No slot: al 0.
SC15_EXPORT unsigned char __cdecl Scena15_PlaceEffect(unsigned kind, int x, int y, int z, unsigned nine, unsigned long twelve) {
    const unsigned char s = SH_CALL(Effect_FindFree)();
    B(kSlot) = s;
    if (s == 0xFF) return 0;
    unsigned char* const e = Effect(s);
    e[0] = 1;
    e[5] = static_cast<unsigned char>(kind);
    SetL(e + 0x64, static_cast<uint32_t>(static_cast<int32_t>(static_cast<short>(x))));
    SetL(e + 0x68, static_cast<uint32_t>(static_cast<int32_t>(static_cast<short>(y))));
    SetL(e + 0x6C, static_cast<uint32_t>(static_cast<int32_t>(static_cast<short>(z))));
    SetL(e + 0xC, static_cast<uint32_t>(twelve));
    e[9] = static_cast<unsigned char>(nine);
    return 1;
}

// original 0x568640: state 2: a tail jump through Scena15_Runs on the s8
// MoveScript_Var7 - 0 a bare ret (0x437CC0), 1..6 Scena15_Run1..6. Unchecked.
SC15_EXPORT void __cdecl Scena15_Run(void) {
    Handler(kRuns15, static_cast<signed char>(B(kRun)), "Scena15_Run")();
}

// original 0x568650: run 1, steps 0..7, 0x14 and 0x19 (a byte table into a
// 10-entry jump table; any other step nothing): counter 0 cleared and the
// party dropped in, flag 1 of row 14 and area 0xBD; three script messages
// (0x12..0x14) between two effects of kind 0x13, the hand drawn at (0x98,
// 0x28 / 0x38 / 0x10) while each message is open; then the flag cleared and
// area 0xC1 (facing 0x82). Steps 0x14 and 0x19 (set by the field scripts)
// change to area 0xC1 facing 0x83 or 0x84 with their flags.
SC15_EXPORT void __cdecl Scena15_Run1(void) {
    const unsigned step = B(kStep);
    switch (step) {
    case 0:
        if (B(kRequest) == 2) return;
        B(kCounter0) = 0;
        DropIn(1);
        Step(1);
        return;
    case 1:
        if (B(kCounter0) != 1) return;
        SetAt(kRow14, 1);
        ChangeArea(0xBD, 0x10000000, 0x14000000, 2);
        B(kAreaMusic) = B(kMusicTrack);
        B(0x904153) = 2;
        B(kStatus) = static_cast<unsigned char>(B(kStatus) | 1);
        SetW(0x90405C, 0);
        Step(2);
        return;
    case 2:
        if (W(kWait) != 0) return;
        Say(0x12);
        Step(3);
        return;
    case 3:
    case 5:
        if (B(kRequest) == 2) {
            SH_CALL(Menu_DrawHand)(0x98, step == 3 ? 0x28 : 0x38, 0);
            return;
        }
        SH_CALL(Scena15_PlaceEffect)(0x13, W(kAngle0), W(kAngle1), step == 3 ? -0x200 : 0x400, 0x20, 0);
        Step(step + 1);
        return;
    case 4:
    case 6:
        if (KeptLive()) return;
        Say(step == 4 ? 0x13 : 0x14);
        Step(step + 1);
        return;
    case 7:
        if (B(kRequest) == 2) {
            SH_CALL(Menu_DrawHand)(0x98, 0x10, 0);
            return;
        }
        Clear40();
        ClearAt(kRow14, 1);
        ChangeArea(0xC1, 0x1A8000, 0x1C0000, 0x82);
        B(kStatus) = static_cast<unsigned char>(B(kStatus) & 0xFE);
        EndRun();
        return;
    case 0x14:
    case 0x19:
        if (B(kRequest) == 2) return;
        if (step == 0x14) {
            ClearAt(kStoryRow, 0x77);
            B(0x904152) = 0;
            ChangeArea(0xC1, 0x1A8000, 0x1C0000, 0x83);
        } else {
            SetAt(kRow14, 2);
            SetAt(kRow14, 3);
            ChangeArea(0xC1, 0x1A8000, 0x1C0000, 0x84);
            B(kByteEE0) = 0xFF;
        }
        SetAt(kStoryRow, 0x8A);
        B(kStatus) = static_cast<unsigned char>(B(kStatus) & 0xFE);
        B(kByte7F98) = 0xFF;
        B(kAreaMusic) = 0xFF;
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
    switch (B(kStep)) {
    case 0:
        if (W(kInputHeld) == 0) return;
        B(kCounter0) = 2;
        Step(1);
        return;
    case 1: {
        if (B(kCounter0) != 3) return;
        const unsigned s = B(kSlot);
        Step(2);
        Effect(s)[1] = 2;
        return;
    }
    case 3: {
        const unsigned idx = Effect(B(kSlot))[0xB];
        SH_CALL(Field_StartEventBattle)(B(kBattles + 9 + idx * 10u));
        Timer(0x78);
        Step(4);
        return;
    }
    case 5: {
        Clear40();
        const unsigned idx = Effect(B(kSlot))[0xB];
        SH_CALL(Flags_Set)(Row(), B(kBattles + 8 + idx * 10u));
        B(kCounter0) = 0;
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
    switch (B(kStep)) {
    case 0:
        SH_CALL(LoadDatFile)(0x302);
        DropIn(4);
        Step(1);
        return;
    case 1:
        if (B(kCounter0) != 0xB) return;
        if (SH_CALL(Scena15_PlaceEffect)(0x13, -0x32A, W(kAngle1), 0x80, 0x70, 0) == 0) return;
        Step(2);
        return;
    case 2: {
        if (!LoadDone()) return;
        if (B(kCounter0) != 0xC) return;
        const unsigned char s = SH_CALL(Effect_FindFree)();
        B(kSlot) = s;
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
        B(kByteFE) = 2;
        Step(4);
        return;
    case 4:
        if (!TestAt(kStoryRow, 0x81)) return;
        if (SH_CALL(Scena15_PlaceEffect)(0x13, -0x2AA, W(kAngle1), 0x200, 0x3C, 0) == 0) return;
        Step(5);
        return;
    case 5:
        if (KeptLive()) return;
        for (unsigned n = 5; n < 9; ++n)
            if (static_cast<unsigned char>(SH_CALL(Party_DropIn)(n)) == 0) break;   // al tested
        Step(6);
        return;
    case 6:
        if (B(kCounter0) != 0xE) return;
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
        if (B(kCounter0) != 0x13) return;
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
            B(kByteFE) = 2;
            SH_CALL(Flags_Set)(row, 1);
        }
        DropIn(9);
        EndRun();
        return;
    case 0x14:
        SH_CALL(Scena15_SpawnMarker)(0x440000, 0x20000, 4, 0xFF, 0);
        B(kCounter0) = 1;
        DropIn(1);
        Step(0x15);
        return;
    case 0x15:
        if (B(kCounter0) != 3) return;
        Transition(0);
        Step(0x16);
        return;
    case 0x16:
        if (W(kWait) != 0) return;
        B(kPass) = 0;
        Say(3);
        Step(0x17);
        return;
    case 0x17:
        if (B(kRequest) == 2) return;
        Step(0x18);
        return;
    case 0x18:
        Set(2);
        ChangeArea(0x58, 0x3E0000, 0x210000, 0x82);
        B(kByteEE0) = 0xFF;
        B(kByte7F98) = 0xFF;
        B(kAreaMusic) = 0x8E;
        Step(0x19);
        return;
    case 0x19:
        if (W(kWait) != 0) return;
        B(kPass) = 0x1F;
        Transition(1);
        SetW(kDistance, 0x5DC);
        B(kRedraw) = 2;
        Step(0x1A);
        return;
    case 0x1A:
        if (B(kCounter0) != 5) return;
        ChangeArea(0xAE, 0x440000, 0x38000, 0x82);
        B(kByteEE0) = 0;
        Step(0x1B);
        return;
    case 0x1B:
        if (B(kCounter0) != 6) return;
        Set(3);
        B(kCounter0) = 0;
        EndRun();
        return;
    case 0x1E:
        SH_CALL(Scena15_SpawnMarker)(0x520000, 0x2F0000, 5, 2, 0);
        DropIn(3);
        Step(0x1F);
        return;
    case 0x1F:
        if (B(kCounter0) != 2) return;
        Set(4);
        B(kCounter0) = 0;
        EndRun();
        return;
    case 0x28:
        B(kCounter0) = 0xA;
        DropIn(4);
        Step(0x29);
        return;
    case 0x29:
        if (B(kCounter0) != 0xC) return;
        Set(5);
        ChangeArea(0x3A, 0x140000, 0x10000, 0x81);
        B(kAreaMusic) = B(kMusicTrack);
        Step(0x2A);
        return;
    case 0x2A:
        if (B(kCounter0) != 0xE) return;
        ChangeArea(0xAE, 0x250000, 0x3A0000, 0x85);
        Step(0x2B);
        return;
    case 0x2B:
        if (B(kCounter0) != 0) return;
        Clear40();
        Set(6);
        EndRun();
        return;
    case 0x32:
        Set(7);
        ChangeArea(2, 0x1A0000, 0x170000, 0x87);
        B(kByte7F98) = 0xFF;
        B(kAreaMusic) = 0x8E;
        Step(0x33);
        return;
    case 0x33:
        if (B(kCounter0) != 1) return;
        B(kPass) = 0x1F;
        Transition(1);
        Step(0x34);
        return;
    case 0x34:
        if (B(kCounter0) != 2) return;
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
    switch (B(kStep)) {
    case 0:
        SH_CALL(LoadDatFile)(0x303);
        DropIn(0);
        Step(1);
        return;
    case 1: {
        if (B(kCounter3) != 0x10) return;
        const unsigned char s = SH_CALL(Effect_FindFree)();
        if (s != 0xFF) {
            unsigned char* const e = Effect(s);
            e[0] = 1;
            e[5] = 0x88;
            e[6] = 1;
            SetL(e + 0x34, 0x3B8000);
            SetL(e + 0x38, 0x220000);
            SetL(e + 0x3C, 0x8000000);
            SetL(e + 0xC, 0x3F8000);
            SetL(e + 0x10, 0x220000);
            SetL(e + 0x14, 0x6000000);
        }
        Timer(0x20);
        Step(2);
        return;
    }
    case 2:
        if (!TimerDone()) return;
        B(kCounter3) = 0x18;
        Step(3);
        return;
    case 3:
        if (B(kCounter3) != 0) return;
        DropIn(7);
        Step(4);
        return;
    case 4:
        if (!LoadDone()) return;
        if (B(kCounter0) != 0xF) return;
        NewEffectKept(0xA0);
        SH_CALL(Music_FadeOut)(0x10);
        Step(5);
        return;
    case 5: {
        if (B(kCounter0) != 0x11) return;
        const unsigned char s = SH_CALL(Effect_FindFree)();
        B(kSlot) = s;
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
        if (B(kCounter0) != 0x12) return;
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
        if (B(kCounter0) != 0x14) return;
        if (!LoadDone()) return;
        SH_CALL(Kind2_Place)(1);
        Step(9);
        return;
    case 9:
        if (B(kCounter0) != 0x15) return;
        Step(0xA);
        return;
    case 0xA:
        if (B(kCounter0) != 0x16) return;
        B(kKept) = B(kPartyByte);
        SH_CALL(PartySet_LoadSecond)(7, 4, 2, 0);
        if (B(kKept) != B(kPartyByte)) {
            B(kKept) = B(kPartyByte);
            B(kPartyByte) = 0xFF;
        } else {
            B(kKept) = 0xFF;
        }
        Timer(0x3C);
        Step(0xB);
        return;
    case 0xB: {
        if (W(kTimer) != 0) {
            Timer(W(kTimer) - 1u);
            return;
        }
        if (!LoadDone()) return;
        const unsigned char kept = B(kKept);
        if (kept != 0xFF) {
            B(kPartyByte) = kept;
            CallA(0);
            B(kPartyByte) = 0xFF;
            SH_CALL(PartySet_LoadFirst)(7, 4, 2);
            B(kPartyByte) = B(kKept);
        } else {
            CallA(0);
        }
        DropIn(8);
        SH_CALL(Scena15_EventObjects)();
        CounterUp();
        SH_CALL(Music_Play)(0xA2, 8);
        unsigned char* const e = Effect(B(kSlot));
        Step(0xC);
        e[1] = static_cast<unsigned char>(e[1] + 1);
        return;
    }
    case 0xC:
        if (B(kCounter0) != 0x2B) return;
        if (!LoadDone()) return;
        Set(9);
        ChangeArea(0xC4, 0x120000, 0x140000, 0x81);
        B(kAreaMusic) = 0x95;
        Step(0xD);
        return;
    case 0xD:
        if (B(kCounter0) != 1) return;
        ChangeArea(0x2D, 0xF8000, 0x358000, 0x87);
        B(kByte7F98) = 1;
        B(kAreaMusic) = 0xA2;
        Step(0xE);
        return;
    case 0xE:
        if (B(kCounter0) != 2) return;
        ChangeArea(0xAE, 0x320000, 0x270000, 0x89);
        B(kByteEE0) = 0;
        B(kAreaMusic) = 0xA2;
        Step(0xF);
        return;
    case 0xF:
        if (B(kCounter0) != 0xD) return;
        Set(0xA);
        ChangeArea(0x98, 0x180000, 0x270000, 0x80);
        B(kByte7F98) = 1;
        B(kAreaMusic) = 0xA2;
        Step(0x10);
        return;
    case 0x10:
        if (B(kCounter0) != 0) return;
        Set(0xA);
        ChangeArea(0xAE, 0x2F8000, 0x278000, 0x8A);
        B(kByteEE0) = 0;
        B(kByte7F98) = 0xFF;
        B(kAreaMusic) = 0xFF;
        Step(0x11);
        return;
    case 0x11:
        if (W(kWait) != 0) return;
        Say(0x1E);
        Step(0x12);
        return;
    case 0x12:
        if (B(kRequest) == 2) return;
        if (!LoadDone()) return;
        SH_CALL(Music_Play)(0xA2, 8);
        B(kPass) = 0x1F;
        Transition(1);
        B(kCounter0) = 1;
        Step(0x13);
        return;
    case 0x13:
        if (B(kCounter0) != 0x14) return;
        B(kCounter0) = 0x15;
        Timer(0x5A);
        Step(0x14);
        return;
    case 0x14:
        if (B(kCounter0) != 0x26) return;
        if (!TimerDone()) return;
        SH_CALL(Scena15_PlaceEffect)(0x13, -0x2AA, W(kAngle1), 0x5E, 0x40, 0);
        Step(0x15);
        return;
    case 0x15:
        if (B(kCounter0) != 0x2B) return;
        SH_CALL(Scena15_PlaceEffect)(0x13, -0x2AA, W(kAngle1), 0x200, 0x28, 0);
        Step(0x16);
        return;
    case 0x16:
        if (B(kCounter0) != 0x3C) return;
        SH_CALL(Music_LoadFile)(0x79);
        Step(0x17);
        return;
    case 0x17:
        if (B(kCounter0) != 0x3F) return;
        if (!LoadDone()) return;
        CounterUp();
        Step(0x18);
        return;
    case 0x18:
        if (B(kCounter0) != 0) return;
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
        const unsigned char s = SH_AT(unsigned char (__cdecl*)(), kEventSlot)();
        SetW(kSlotWord, s);
        if (s != 0xFF) SH_CALL(EventOp_0x)(At(kOpRecords0 + 0x11u * i));
    }
    for (unsigned i = 0; i < 2; ++i) {
        const unsigned char s = SH_AT(unsigned char (__cdecl*)(), kEventSlot)();
        SetW(kSlotWord, s);
        if (s != 0xFF) SH_CALL(EventOp_6x)(At(kOpRecords6 + 0x10u * i));
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
    switch (B(kStep)) {
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
        if (B(kCounter0) != 1) return;
        const unsigned char s = SH_CALL(Effect_FindFree)();
        B(kSlot) = s;
        if (s == 0xFF) return;
        Step(7);
        unsigned char* const e = Effect(s);
        e[0] = 1;
        e[5] = 0xA4;
        return;
    }
    case 8:
        B(kPass) = static_cast<unsigned char>(B(kPass) & 0xFB);
        CounterUp();
        Step(9);
        return;
    case 0xA:
        SH_CALL(Music_LoadFile)(0x82);
        DropIn(0x11);
        Step(0xB);
        return;
    case 0xB: {
        if (B(kCounter0) != 0x15) return;
        if (!LoadDone()) return;
        const unsigned char s = SH_CALL(Effect_FindFree)();
        if (s != 0xFF) {
            unsigned char* const e = Effect(s);
            e[0] = 1;
            e[5] = 0x1D;
            SetL(e + 0x34, 0x2F8000);
            SetL(e + 0x38, 0x278000);
            const long z = static_cast<long>(L(e + 0x38));
            const long x = static_cast<long>(L(e + 0x34));
            const long elev = SH_CALL(AreaMap_Elevation)(x, z);
            SetL(e + 0x3C, static_cast<uint32_t>(static_cast<int32_t>(static_cast<short>(elev)) << 16));
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
        if (B(kCounter0) != 0x1F) return;
        Timer(0x3C);
        Step(0xE);
        return;
    case 0xE:
        if (!TimerDone()) return;
        Set(0x12);
        ChangeArea(0xAC, 0x178000, 0x210000, 0x8C);
        B(kByte7F98) = 0xFF;
        Step(0xF);
        return;
    case 0xF:
        if (W(kWait) != 0) return;
        Say(0x2B);
        Step(0x10);
        return;
    case 0x10:
        if (B(kRequest) == 2) return;
        B(kPass) = 0x1F;
        Transition(1);
        CounterUp();
        Step(0x11);
        return;
    case 0x11:
        if (B(kCounter0) != 3) return;
        if (W(kTimer) < 0x20) {
            SH_CALL(Scena15_ClutToGrey)(0);
            Timer(W(kTimer) + 1u);
            return;
        }
        SH_CALL(Scena15_ClutToGrey)(1);
        SH_CALL(Music_FadeOut)(8);
        Timer(0);
        Step(0x12);
        return;
    case 0x12:
        if (B(kCounter0) != 4) return;
        SH_CALL(Music_FadeOutStop)(0xA);
        Transition(0xD);
        Step(0x13);
        return;
    case 0x13:
        if (W(kWait) != 0) return;
        B(kPass) = 0;
        Timer(0x3C);
        Step(0x14);
        return;
    case 0x14:
        if (!TimerDone()) return;
        Say(0x2D);
        Step(0x15);
        return;
    case 0x15:
        if (B(kRequest) == 2) return;
        Timer(0x3C);
        Step(0x16);
        return;
    case 0x16:
        if (!TimerDone()) return;
        NewEffectKept(0xA5);
        Step(0x17);
        return;
    case 0x18:
        if ((L(kInputHeld) & 0x800u) == 0) return;
        Transition(0xD);
        Step(0x19);
        return;
    case 0x19:
        if (W(kWait) != 0) return;
        SH_AT(VoidFn, kSoundStop)();
        SH_CALL(Task_Restart)(reinterpret_cast<void*>(static_cast<std::uintptr_t>(kBootTask)));
        return;
    case 0x1E:
        DropIn(0x12);
        SH_CALL(Music_LoadFile)(0xA2);
        Step(0x1F);
        return;
    case 0x1F:
        if (B(kCounter0) != 0x2A) return;
        for (unsigned k = 0; k < 0x1E; ++k) {
            if ((SH_CALL(Scena15_RecordWord)(W(Sprite(k) + 0x2C)) & 0xFFFFu) != 0x308) continue;
            unsigned char* const o = Sprite(k);
            o[0x83] = 0x24;
            SetW(o + 0x8A, 0);
            break;
        }
        Set(0x11);
        Step(0x20);
        return;
    case 0x20:
        if (B(kCounter0) != 0x31) return;
        Transition(0);
        Step(0x21);
        return;
    case 0x21:
        if (W(kWait) != 0) return;
        B(kPass) = 0;
        Say(0x65);
        Step(0x22);
        return;
    case 0x22:
        if (B(kRequest) == 2) return;
        CallA(1);
        B(kRequest) = 6;
        Step(0x23);
        return;
    case 0x23:
        if (B(kRequest) != 0) return;
        B(kRequest) = 1;
        B(0x929F10) = 1;
        Step(0x24);
        return;
    case 0x24:
        if (B(kRequest) != 0) return;
        ChangeArea(0xC6, 0x160000, 0x120000, 0x80);
        B(kByteEE0) = 0xFF;
        B(kByte7F98) = 0xFF;
        B(kAreaMusic) = 0xFF;
        B(kShakeOn) = 0;
        B(kRun) = 6;
        Step(0);
        return;
    default: return;
    }
}

namespace {

// One channel stepped toward the grey level as 0x569DA0 steps it: +1 below or
// at it, +0xFFFF (a 16-bit -1, carried in 32 bits) above; kept when equal.
uint32_t Toward(uint32_t channel, uint32_t grey) {
    if (static_cast<std::uint16_t>(channel) == static_cast<std::uint16_t>(grey)) return channel;
    const int32_t d = static_cast<int32_t>((grey & 0xFFFFu) - channel);
    return channel + (d >= 0 ? 1u : 0xFFFFu);
}

}  // namespace

// original 0x569DA0: every CLUT strip row but 0xB (32 rows of 0x100 colours
// from Gfx_ClutStrip) toward grey: each colour's grey is (r + g + b) / 3;
// with `snap` (a byte) not 0 the colour becomes that grey (bit 15 kept),
// else each channel steps one toward it. Then Gfx_ClutStripDirty = 1.
SC15_EXPORT void __cdecl Scena15_ClutToGrey(unsigned snap) {
    const bool to_grey = static_cast<unsigned char>(snap) != 0;
    for (unsigned row = 0; row < 0x20; ++row) {
        if (row == 0xB) continue;
        unsigned char* p = At(kClutStrip + row * 0x200u);
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
    B(kClutDirty) = 1;
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
    switch (B(kStep)) {
    case 0:
        if (W(kWait) != 0) break;
        B(kPass) = 0xFB;
        Transition(1);
        {
            const unsigned char c = static_cast<unsigned char>(B(kCounter0) + 1);
            Step(1);
            B(kCounter0) = c;
        }
        SH_CALL(LoadDatFile)(0x304);
        break;
    case 1:
        if (!LoadDone()) break;
        if (B(kCounter0) != 5) break;
        SH_CALL(Music_LoadFile)(0x5F);
        Step(2);
        break;
    case 2:
        if (B(kCounter0) != 8) break;
        NewEffect(0xA9);
        Step(3);
        break;
    case 3:
        if (B(kCounter0) != 9) break;
        Timer(0x1E);
        Step(4);
        break;
    case 4:
        if (!TimerDone()) break;
        Step(5);
        CounterUp();
        break;
    case 5:
        if (B(kCounter0) != 0xB) break;
        if (!LoadDone()) break;
        if (SH_CALL(Scena15_PlaceEffect)(0x31, W(kAngle0), W(kAngle1), W(kAngle2), 0x20, 0x380) == 0) break;
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
        B(kPass) = 0x1F;
        SH_AT(void (__cdecl*)(long, long, unsigned), kPartyPlace)(0x170000, 0x120000, 0x22);
        Step(8);
        break;
    case 8:
        Step(9);
        CounterUp();
        break;
    case 9:
        if (B(kCounter0) != 0xD) break;
        Step(0xA);
        break;
    case 0xA:
        if (B(kCounter0) != 0xE) break;
        SH_CALL(Music_FadeOut)(0x10);
        Step(0xB);
        break;
    case 0xB:
        if (B(kCounter0) != 0xF) break;
        NewEffect(0xAA);
        SH_CALL(Music_FadeOutStop)(0xA);
        Timer(4);
        Step(0xC);
        break;
    case 0xC:
        if (!TimerDone()) break;
        if (SH_CALL(Scena15_PlaceEffect)(0x31, W(kAngle0), W(kAngle1), W(kAngle2), 0x20, 0) == 0) break;
        Step(0xD);
        break;
    case 0xD:
        if (KeptLive()) break;
        SH_CALL(Field_StartEventBattle)(0x37);
        Step(0xE);
        break;
    case 0xF:
        if (!LoadDone()) break;
        B(kCounter0) = 0;
        SH_CALL(LoadDatFile)(0x305);
        DropIn(1);
        Step(0x10);
        break;
    case 0x10:
        if (!LoadDone()) break;
        if (B(kCounter0) != 1) break;
        NewEffect(0xAB);
        Timer(0x3C);
        Step(0x11);
        break;
    case 0x11:
        if (!TimerDone()) break;
        B(kKept) = 0;
        SH_CALL(Scena15_RandomPause)();
        SH_CALL(Sound_PlayEffect)(0x206);
        Timer(0x3C);
        Step(0x12);
        break;
    case 0x12:
        if (TimerDone()) {
            B(kCounter0) = 2;
            Step(0x13);
        }
        SH_CALL(Scena15_RandomPause)();
        break;
    case 0x13:
        if (B(kCounter0) == 0) {
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
        if (W(kWait) != 0) break;
        B(kPass) = 0;
        SH_CALL(Sound_PlayEffect)(0x206);
        Timer(0x78);
        Step(0x16);
        break;
    case 0x16:
        if (!TimerDone()) break;
        Set(0x13);
        ChangeArea(0xAE, 0x350000, 0x270000, 0);
        B(kByteEE0) = 0xFF;
        B(kAreaMusic) = 0xFF;
        Step(0x17);
        break;
    case 0x17:
        if (B(kCounter0) != 3) break;
        SH_CALL(Music_LoadFile)(0x5F);
        B(kShakeOn) = 1;
        B(kShakeShift) = 4;
        SH_CALL(Sound_PlayEffect)(0x205);
        Timer(8);
        Step(0x18);
        break;
    case 0x18:
        if (!TimerDone()) break;
        B(kShakeOn) = 0;
        CounterUp();
        Step(0x19);
        break;
    case 0x19:
        if (B(kCounter0) != 7) break;
        SH_CALL(Music_Play)(0x5F, 8);
        B(kShakeShift) = 3;
        B(kShakeOn) = 1;
        Step(0x1A);
        break;
    case 0x1A:
        if (B(kCounter0) != 9) break;
        NewEffect(0xAD);
        Step(0x1B);
        break;
    case 0x1B:
        if (B(kCounter0) != 0xB) break;
        ChangeArea(0xC6, 0x170000, 0x120000, 0x82);
        B(kAreaMusic) = 0x5F;
        Step(0x1C);
        break;
    case 0x1C:
        if (B(kCounter0) != 1) break;
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
        if (B(kCounter0) != 0) break;
        Transition(0);
        Step(0x1F);
        break;
    case 0x1F:
    case 0x23:
    case 0x27:
    case 0x2C: {
        if (W(kWait) != 0) break;
        const unsigned step = B(kStep);
        B(kPass) = 0;
        Say(step == 0x1F ? 0xA : step == 0x23 ? 0x6B : 0xC);
        Step(step + 1);
        break;
    }
    case 0x20:
    case 0x24:
    case 0x28:
    case 0x2D:
        if (B(kRequest) == 2) break;
        Step(B(kStep) + 1u);
        break;
    case 0x21:
        Set(0x14);
        ChangeArea(0xAE, 0x340000, 0x270000, 0x94);
        B(kByteEE0) = 0xFF;
        B(kAreaMusic) = 0x5F;
        Step(0x22);
        break;
    case 0x22:
        if (B(kCounter0) != 3) break;
        Transition(0);
        Step(0x23);
        break;
    case 0x25:
    case 0x2E: {
        const unsigned next = B(kStep) + 1u;   // a constant in the original, stored after the call
        ChangeArea(0xC6, 0x170000, 0x120000, 0x82);
        B(kByteEE0) = 0xFF;
        B(kAreaMusic) = 0x5F;
        Step(next);
        break;
    }
    case 0x26:
        if (B(kCounter0) != 1) break;
        B(kCounter0) = 0;
        Transition(0);
        Step(0x27);
        break;
    case 0x29:
        Set(0x15);
        SetAt(kStoryRow, 0x58);
        ChangeArea(0xAD, 0x2E0000, 0x610000, 0);
        B(kByteEE0) = 0xFF;
        B(kAreaMusic) = 0xA1;
        Step(0x2A);
        break;
    case 0x2A:
        if (B(kCounter0) != 1) break;
        B(kByteFE) = 2;
        Step(0x2B);
        break;
    case 0x2B:
        if (B(kCounter0) != 3) break;
        B(kCounter0) = 0;
        Transition(0);
        Step(0x2C);
        break;
    case 0x2F:
        if (B(kCounter0) != 2) break;
        B(kShakeShift) = 3;
        Step(0x30);
        break;
    case 0x30:
        if (B(kCounter0) != 3) break;
        if (SH_CALL(Scena15_PlaceEffect)(0x31, W(kAngle0), W(kAngle1), W(kAngle2), 0x80, 0xB00) == 0) break;
        Step(0x31);
        break;
    case 0x31:
        if (static_cast<short>(W(kDistance)) <= 0x780) break;
        Set(0x16);
        ChangeArea(0x95, 0x80000, 0xC0000, 0x82);
        B(kByteEE0) = 8;
        B(kByte7F98) = 9;
        B(kAreaMusic) = 0x5F;
        Step(0x32);
        break;
    case 0x32:
        if (W(kWait) != 0) break;
        Step(0x33);
        break;
    case 0x33:
        if (B(kCounter0) == 0x15) {
            SH_CALL(Music_FadeOut)(8);
            SH_CALL(Sound_PlayEffect)(2);
            Transition(0xF);
            Step(0x34);
        }
        SH_AT(VoidFn, kViewShift)();
        break;
    case 0x34:
        if (W(kWait) != 0) break;
        SH_CALL(Music_FadeOutStop)(0xA);
        B(kPass) = 0;
        Transition(0x10);
        Step(0x35);
        break;
    case 0x35:
        if (W(kWait) != 0) break;
        Timer(0x96);
        Step(0x36);
        break;
    case 0x36:
        if (!TimerDone()) break;
        ChangeArea(0xC7, 0xB60000, 0x128000, 7);
        SetW(0x9039A2, W(0x9039A2) & 0xFFF7u);
        EndRun();
        B(0x7E0941) = 0;
        SH_AT(VoidFn, kStatusBit80)();
        break;
    default: break;
    }
    SH_CALL(Scena15_Shake)(B(kShakeShift));
}

// original 0x56AD80: with the byte 0x6BC742 at 0: counter 1 = 1 and the byte
// set to ((Rand() % 0x26) + 0x2A) / 2 (signed, as idiv and the halving round
// toward 0); else the byte counted down. Run 6's pause.
SC15_EXPORT void __cdecl Scena15_RandomPause(void) {
    if (B(kKept) != 0) {
        B(kKept) = static_cast<unsigned char>(B(kKept) - 1);
        return;
    }
    B(kCounter1) = 1;
    const int r = SH_CALL(Rand)() % 0x26 + 0x2A;
    B(kKept) = static_cast<unsigned char>(r / 2);
}

// original 0x56ADC0: while the byte 0x6BC741 is set, the camera shakes:
// MapView_Redraw = 2 and Camera_ShiftY += Scena15_ShakeSteps[Frame_Counter &
// 3] (an s8) << the byte `shift` (a 16-bit add).
SC15_EXPORT void __cdecl Scena15_Shake(unsigned shift) {
    if (B(kShakeOn) == 0) return;
    const uint32_t i = L(kFrame) & 3u;
    B(kRedraw) = 2;
    const uint32_t d = static_cast<uint32_t>(static_cast<int32_t>(static_cast<signed char>(B(kShakeSteps + i))))
                       << (shift & 0x1Fu);
    SetW(kShiftY, W(kShiftY) + d);
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
    SetL(e + 0x34, static_cast<uint32_t>(x));
    SetL(e + 0x38, static_cast<uint32_t>(z));
    const long elev = SH_CALL(AreaMap_Elevation)(static_cast<long>(L(e + 0x34)), z);
    SetL(e + 0x3C, static_cast<uint32_t>(static_cast<int32_t>(static_cast<short>(elev)) + 0x200) << 16);
    e[6] = static_cast<unsigned char>(six);
    e[7] = static_cast<unsigned char>(seven);
    e[0xB] = static_cast<unsigned char>(eleven);
    return 1;
}

// original 0x56AE80: chapter 15's vtable slot 1, the object hook (PSX
// 0x801FDD80): Scena15_Objects[object +0x86] (object, the flag row).
// Unchecked (12 entries).
SC15_EXPORT void __cdecl Scena15_ObjectTrigger(unsigned char* object) {
    CallObject(kObjects15, object, "Scena15_ObjectTrigger");
}

// original 0x56AEA0: Scena15_Objects entries 0..4: a talk. The byte
// Scena15_TalkWho[object +0x86] (put in the low byte of the object argument's
// stack slot, the rest the pointer's) handed to 0x42C0A0 in area 0xC0, else
// to 0x42BA90, whose answer is the script message opened; the request 2.
SC15_EXPORT void __cdecl Scena15_ObjectTalk(unsigned char* object, unsigned char*) {
    const unsigned area = Area();
    const uint32_t who = (static_cast<uint32_t>(reinterpret_cast<std::uintptr_t>(object)) & 0xFFFFFF00u) |
                         B(kTalkWho + object[0x86]);
    const unsigned id = area == 0xC0 ? SH_AT(unsigned (__cdecl*)(uint32_t), kTalkIdC0)(who)
                                     : SH_AT(unsigned (__cdecl*)(uint32_t), kTalkId)(who);
    Say(id);
}

// original 0x56AF00: Scena15_Objects entry 5: the script flags' bit 0x40, run
// 3 at step 0x32.
SC15_EXPORT void __cdecl Scena15_Object05(unsigned char*, unsigned char*) {
    Set40();
    B(kRun) = 3;
    Step(0x32);
}

namespace {

// Scena15_Objects entries 6..11: the script flags' bit 0x40, a flag of the
// chapter's row, a party member dropped in (0xFF none), the object's word
// +0x8A counted.
void ObjectFlag(unsigned char* object, unsigned flag, unsigned member) {
    Set40();
    Set(flag);
    if (member != 0xFF) DropIn(member);
    SetW(object + 0x8A, W(object + 0x8A) + 1u);
}

}  // namespace

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
    SetW(object + 0x8A, W(object + 0x8A) + 1u);
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
        if (B(kByteFD) == 1 && !Test(1) && z > 0x470000) {
            Set40();
            B(kRun) = 3;
            Step(0xF);
        }
        if (B(kByteFD) == 2 && !TestAt(kStoryRow, 0x81) && static_cast<short>(static_cast<uint32_t>(x) >> 16) > 0x2C &&
            static_cast<std::uint16_t>(zcell - 0x78) < 8) {
            Set40();
            B(kRun) = 3;
            Step(0);
        }
    }
    if (Area() == 0xAE && B(kByteFD) == 0 && x == 0x418000 && static_cast<std::uint16_t>(zcell - 0x22) < 3) {
        Set40();
        B(kRun) = 4;
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
        B(kRun) = 3;
        Step(0x14);
        return 0;
    }
    if (!Test(4)) {
        if (x > 0x530000) return 0;
        if (static_cast<std::uint16_t>(zcell - 0x2E) >= 3) return 0;
        Set40();
        B(kRun) = 3;
        Step(0x1E);
        return 0;
    }
    if (!Test(5)) {
        if (z > 0x3A0000) return 0;
        if (static_cast<std::uint16_t>(xcell - 0x23) >= 4) return 0;
        Set40();
        B(kRun) = 3;
        Step(0x28);
        return 0;
    }
    if (x != 0x310000) return 0;
    if (static_cast<std::uint16_t>(zcell - 0x27) >= 2) return 0;
    if (Test(0xC) && Test(0xE) && Test(0xD) && Test(0xF) && Test(0x10)) {
        Set40();
        B(kRun) = 5;
        Step(5);
        return 0;
    }
    Set40();
    B(kRun) = 5;
    Step(0);
    return 0;
}

// ===========================================================================
// Chapter 16's object hook

// original 0x56C080: chapter 16's vtable slot 1 (PSX 0x801F8344):
// Scena16_Objects[object +0x86] (object, the flag row). The table holds one
// entry (0x43C9F0, al 0) and a 0 after it: unchecked.
SC15_EXPORT void __cdecl Scena16_ObjectTrigger(unsigned char* object) {
    CallObject(kObjects16, object, "Scena16_ObjectTrigger");
}

// ===========================================================================
// Chapter 17: the staff roll

namespace {

unsigned char* Packet() { return At(L(kPacketNext)); }
void SetF(unsigned char* p, float f) {
    uint32_t v;
    static_assert(sizeof v == sizeof f, "a float is a dword");
    __builtin_memcpy(&v, &f, sizeof v);
    SetL(p, v);
}
// fild of an s16 widened to a dword, fstp to a float: exact.
void SetCoord(unsigned char* p, unsigned v) { SetF(p, static_cast<float>(static_cast<short>(v))); }
void Commit(unsigned slot, unsigned size) { SH_CALL(Gfx_CommitPrim)(slot, size); }
void DrawMode(unsigned tpage, int dtd) { SH_CALL(Gpu_SetDrawMode)(Packet(), 0, dtd, tpage, 0); }
void Rgb(unsigned char* p, unsigned off, unsigned char c) {
    p[off] = c;
    p[off + 1] = c;
    p[off + 2] = c;
}
// Part 1's and part 3's steps end by drawing these.
void Fade(unsigned on, unsigned level) { SH_CALL(Scena17_DrawFade)(on, level); }
void Rays(unsigned radius, unsigned level) { SH_CALL(Scena17_DrawRays)(radius, level); }
void Panel(unsigned level) { SH_CALL(Scena17_DrawPanel)(0xE0, 0xBE, level, 1); }
// The ground elevation under the kind-2 sprite handed to MapView_SetElevation.
void ElevationToView() {
    const long z = static_cast<long>(L(kKind2Z));
    const long x = static_cast<long>(L(kKind2X));
    SH_CALL(MapView_SetElevation)(static_cast<int>(SH_CALL(AreaMap_Elevation)(x, z)));
}
bool Held() { return B(kHold) != 0; }
// imul then sar 12, in 32 bits (wrapping as the original's does).
unsigned Scaled(int v, int32_t by) {
    return static_cast<unsigned>(static_cast<int32_t>(static_cast<uint32_t>(v) * static_cast<uint32_t>(by)) >> 12);
}
// dec byte [0x8034E5]: true when it reaches 0.
bool StepDone() {
    Step(B(kStep) - 1u);
    return B(kStep) == 0;
}

}  // namespace

// original 0x56C130: chapter 17's vtable slot 0 (PSX 0x801F775C): a tail jump
// through Scena17_States on the s8 0x8034E2 - 0 Scena17_Start, 1
// Scena17_EnterArea, 2 Scena17_Run. Unchecked.
SC15_EXPORT void __cdecl Scena17_Frame(void) {
    Handler(kStates17, static_cast<signed char>(B(kState)), "Scena17_Frame")();
}

// original 0x56C140: state 0: the CLUT strip restored (Gfx_ClutStripRestore,
// dirty 1), the pass flags 0x1F, area 0xC7 at (0xB60000, 0x128000) facing 7,
// the four counters 0, state 1.
SC15_EXPORT void __cdecl Scena17_Start(void) {
    SH_CALL(Gfx_ClutStripRestore)();
    B(kClutDirty) = 1;
    B(kPass) = 0x1F;
    ChangeArea(0xC7, 0xB60000, 0x128000, 7);
    SetL(kCounter0, 0);
    B(kState) = 1;
}

// original 0x56C180: state 1: in area 0xC7, the script flags' and the party
// objects' bit 0x40, the camera angles (0xFD1C, -, 0x12C), the elevation
// 0x300, one field frame drawn by hand (AreaMap_Frame, Field_ObjectsScreen,
// Field_DrawFrame) and a DR_MOVE of the frame buffer's 320 x 240 (y by
// Gfx_BufferIndex) to (0x2C0, 0) committed to OT slot 5. In any area: the
// pass flags 0, Port_DroppedCall(2), state 2, part 1.
SC15_EXPORT void __cdecl Scena17_EnterArea(void) {
    if (Area() == 0xC7) {
        Set40();
        SH_CALL(ObjTrio_SetBit40)();
        SetW(kAngle0, 0xFD1C);
        SetW(kAngle2, 0x12C);
        SH_CALL(MapView_SetElevation)(0x300);
        SH_CALL(AreaMap_Frame)();
        SH_CALL(Field_ObjectsScreen)();
        SH_CALL(Field_DrawFrame)();
        unsigned char rect[8];
        const unsigned y = static_cast<unsigned>(B(0x905B89)) * 240u;
        SetW(rect, 0);
        SetW(rect + 2, y);
        SetW(rect + 4, 0x140);
        SetW(rect + 6, 0xF0);
        SH_CALL(Gpu_SetDrawMove)(Packet(), rect, 0x2C0, 0);
        Commit(5, 0x18);
    }
    B(kPass) = 0;
    SH_CALL(Port_DroppedCall)(2);
    B(kState) = 2;
    B(kPart) = 1;
}

// original 0x56C240: state 2: a tail jump through Scena17_Parts on the s8
// 0x8034E3 - 0 a bare ret, 1 Scena17_Intro, 2 Scena17_Roll, 3 Scena17_Outro.
// Unchecked.
SC15_EXPORT void __cdecl Scena17_Run(void) {
    Handler(kParts17, static_cast<signed char>(B(kPart)), "Scena17_Run")();
}

// original 0x56C250: part 1: Scena17_Part1Steps[MoveScript_Var7] (s8,
// unchecked), then the letterbox (a tail jump).
SC15_EXPORT void __cdecl Scena17_Intro(void) {
    Handler(kPart1Steps, static_cast<signed char>(B(kRun)), "Scena17_Intro")();
    SH_CALL(Scena17_DrawLetterbox)();
}

// original 0x56C270: part 1 step 0: once the wait word is 0, the kind-2 sprite
// at (0xB00000, 0x138000), Field_ViewReset, Cond_ByteFE 4, track 0x96 loaded,
// the transition in, the fade drawn (off, 0x80), the next step.
SC15_EXPORT void __cdecl Scena17_Intro00(void) {
    if (W(kWait) != 0) return;
    SetL(kKind2X, 0xB00000);
    SetL(kKind2Z, 0x138000);
    SH_CALL(Field_ViewReset)();
    B(kByteFE) = 4;
    SH_CALL(Music_LoadFile)(0x96);
    Transition(1);
    Fade(0, 0x80);
    RunUp();
}

// original 0x56C2D0: step 1: once the wait word is 0 and the file is in, the
// run's step 0x5A and the next step; the fade drawn every frame.
SC15_EXPORT void __cdecl Scena17_Intro01(void) {
    if (W(kWait) == 0 && LoadDone()) {
        Step(0x5A);
        RunUp();
    }
    Fade(0, 0x80);
}

// original 0x56C310: step 2: the byte 0x8034E5 counted down; at 0 track 0x96
// played, the next step, 0x8034E5 = 0x1E. The fade drawn every frame.
SC15_EXPORT void __cdecl Scena17_Intro02(void) {
    if (StepDone()) {
        SH_CALL(Music_Play)(0x96, 8);
        RunUp();
        Step(0x1E);
    }
    Fade(0, 0x80);
}

// original 0x56C350: step 3: counted down; at 0 the kind-2 sprite's x
// 0x940000, MoveScript_F3Divisor 0x20, 0x8034E5 = 0x80, the camera angles
// (0xFE96, -, 0x200), the pass flags 0x1F, the next step. The fade drawn.
SC15_EXPORT void __cdecl Scena17_Intro03(void) {
    if (StepDone()) {
        const unsigned char run = B(kRun);
        SetL(kKind2X, 0x940000);
        SetW(kF3Divisor, 0x20);
        Step(0x80);
        SetW(kAngle0, 0xFE96);
        SetW(kAngle2, 0x200);
        B(kPass) = 0x1F;
        B(kRun) = static_cast<unsigned char>(run + 1);
    }
    Fade(0, 0x80);
}

// original 0x56C3B0: step 4: counted down, the fade drawn on at that level;
// at 0 (read again after the draw) the next step.
SC15_EXPORT void __cdecl Scena17_Intro04(void) {
    const unsigned char v = static_cast<unsigned char>(B(kStep) - 1);
    Step(v);
    Fade(1, v);
    if (B(kStep) == 0) RunUp();
}

// original 0x56C3E0: step 5: once Field_Kind2Hold is 0, x 0x8C0000 and the
// next step.
SC15_EXPORT void __cdecl Scena17_Intro05(void) {
    if (Held()) return;
    const unsigned char run = B(kRun);
    SetL(kKind2X, 0x8C0000);
    B(kRun) = static_cast<unsigned char>(run + 1);
}

// original 0x56C400: step 6: the camera's angle 0 turned by -5 every frame;
// once Field_Kind2Hold (read before the turn) is 0, x 0x540000, counter 3
// 0xA and the next step.
SC15_EXPORT void __cdecl Scena17_Intro06(void) {
    const bool held = Held();
    SetW(kAngle0, W(kAngle0) - 5u);
    if (held) return;
    const unsigned char run = B(kRun);
    SetL(kKind2X, 0x540000);
    B(kCounter3) = 0xA;
    B(kRun) = static_cast<unsigned char>(run + 1);
}

// original 0x56C430: step 7: once Field_Kind2Hold is 0 and counter 3 is 0x14,
// MoveScript_F3Divisor 0x10, x 0x3C0000, the next step.
SC15_EXPORT void __cdecl Scena17_Intro07(void) {
    if (Held()) return;
    if (B(kCounter3) != 0x14) return;
    const unsigned char run = B(kRun);
    SetW(kF3Divisor, 0x10);
    SetL(kKind2X, 0x3C0000);
    B(kRun) = static_cast<unsigned char>(run + 1);
}

// original 0x56C470: step 8: once Field_Kind2Hold is 0, the view to the
// ground's elevation, the next step and x 0x2E0000; while it holds, the view's
// elevation down one every other frame.
SC15_EXPORT void __cdecl Scena17_Intro08(void) {
    if (Held()) {
        if (B(kFrame) & 1) SetL(kElevation, L(kElevation) - 1u);
        return;
    }
    ElevationToView();
    const unsigned char run = B(kRun);
    SetL(kKind2X, 0x2E0000);
    B(kRun) = static_cast<unsigned char>(run + 1);
}

// original 0x56C4C0: step 9: once Field_Kind2Hold is 0, the view to the
// ground's elevation, x 0x260000, Cond_ByteFE 1, MoveScript_FAWord = (the
// elevation at (0x260000, z) - MapView_Elevation) / 128 (signed, toward 0),
// the next step; while it holds, the view's elevation up 2 a frame and one
// more every fourth.
SC15_EXPORT void __cdecl Scena17_Intro09(void) {
    if (Held()) {
        const uint32_t e = L(kElevation) + 2u;
        const unsigned char f = B(kFrame);
        SetL(kElevation, e);
        if ((f & 3) == 0) SetL(kElevation, e + 1u);
        return;
    }
    ElevationToView();
    const long z = static_cast<long>(L(kKind2Z));
    SetL(kKind2X, 0x260000);
    const long e = SH_CALL(AreaMap_Elevation)(0x260000, z);
    const int32_t d = static_cast<int32_t>(static_cast<uint32_t>(static_cast<int32_t>(static_cast<short>(e))) - L(kElevation));
    B(kByteFE) = 1;
    SetW(kFAWord, static_cast<unsigned>(d / 128));
    RunUp();
}

// original 0x56C550: step 10: once Field_Kind2Hold is 0, the view to the
// ground, MoveScript_F3Divisor 0x10, x 0x190000, MoveScript_FAWord 0, the
// next step; while it holds, angle 0 down one and the camera distance down 6
// a frame.
SC15_EXPORT void __cdecl Scena17_Intro10(void) {
    if (Held()) {
        SetW(kAngle0, W(kAngle0) - 1u);
        SetW(kDistance, W(kDistance) - 6u);
        return;
    }
    ElevationToView();
    const unsigned char run = B(kRun);
    SetW(kF3Divisor, 0x10);
    SetL(kKind2X, 0x190000);
    SetW(kFAWord, 0);
    B(kRun) = static_cast<unsigned char>(run + 1);
}

// original 0x56C5B0: step 11: once Field_Kind2Hold is 0, Cond_ByteFE 2 and the
// next step; while it holds, the view's elevation up 2 a frame to at most
// 0x540 (unsigned).
SC15_EXPORT void __cdecl Scena17_Intro11(void) {
    if (Held()) {
        const uint32_t e = L(kElevation) + 2u;
        SetL(kElevation, e);
        if (e > 0x540) SetL(kElevation, 0x540);
        return;
    }
    const unsigned char run = B(kRun);
    B(kByteFE) = 2;
    B(kRun) = static_cast<unsigned char>(run + 1);
}

// original 0x56C5F0: step 12: on counter 3 0x64, Cond_ByteFE 3 and the next
// step.
SC15_EXPORT void __cdecl Scena17_Intro12(void) {
    if (B(kCounter3) != 0x64) return;
    const unsigned char run = B(kRun);
    B(kByteFE) = 3;
    B(kRun) = static_cast<unsigned char>(run + 1);
}

// original 0x56C610: step 13: the view's elevation up 0x10 a frame; past
// 0xCC0 (unsigned) 0x8034E5 = 0xF and the next step. MapView_Redraw 2.
SC15_EXPORT void __cdecl Scena17_Intro13(void) {
    const uint32_t e = L(kElevation) + 0x10u;
    SetL(kElevation, e);
    if (e > 0xCC0) {
        const unsigned char run = B(kRun);
        Step(0xF);
        B(kRun) = static_cast<unsigned char>(run + 1);
    }
    B(kRedraw) = 2;
}

// original 0x56C640: step 14: counted down; at 0 the transition 0xD and the
// next step.
SC15_EXPORT void __cdecl Scena17_Intro14(void) {
    if (!StepDone()) return;
    Transition(0xD);
    RunUp();
}

// original 0x56C670: step 15: once the wait word is 0, the music stopped over
// 10 frames, Music_Track 0xFF, the pass flags 0, Cond_ByteFE 6, part 2 at
// step 0.
SC15_EXPORT void __cdecl Scena17_Intro15(void) {
    if (W(kWait) != 0) return;
    SH_CALL(Music_FadeOutStop)(0xA);
    const unsigned char part = static_cast<unsigned char>(B(kPart) + 1);
    B(kMusicTrack) = 0xFF;
    B(kPass) = 0;
    B(kByteFE) = 6;
    B(kPart) = part;
    B(kRun) = 0;
}

// original 0x56C6B0: part 2: Scena17_Part2Steps[MoveScript_Var7] (s8,
// unchecked), then the letterbox (a tail jump).
SC15_EXPORT void __cdecl Scena17_Roll(void) {
    Handler(kPart2Steps, static_cast<signed char>(B(kRun)), "Scena17_Roll")();
    SH_CALL(Scena17_DrawLetterbox)();
}

// original 0x56C6D0: part 2 step 0: stream 8 loaded, the roll at line 0 and
// scroll 0, the next step.
SC15_EXPORT void __cdecl Scena17_Roll0(void) {
    SH_CALL(Sound_LoadStream)(8);
    SetW(kRollLine, 0);
    B(kRollScroll) = 0;
    RunUp();
}

// original 0x56C6F0: step 1: the roll drawn and scrolled
// (Scena17_ScrollRoll); once the stream is done and the roll at its end, the
// transition 0xD and the next step.
SC15_EXPORT void __cdecl Scena17_Roll1(void) {
    const unsigned char end = SH_CALL(Scena17_ScrollRoll)();
    if (SH_CALL(Sound_StreamDone)() == 0) return;
    if (end == 0) return;
    Transition(0xD);
    RunUp();
}

// original 0x56C720: step 2: once the wait word is 0, part 3 at step 0; until
// then the roll drawn (a tail jump to Scena17_ScrollRoll).
SC15_EXPORT void __cdecl Scena17_Roll2(void) {
    if (W(kWait) == 0) {
        const unsigned char part = B(kPart);
        B(kRun) = 0;
        B(kPart) = static_cast<unsigned char>(part + 1);
        return;
    }
    SH_CALL(Scena17_ScrollRoll)();
}

// original 0x56C750: part 3: a tail jump through Scena17_Part3Steps on the s8
// MoveScript_Var7. Unchecked.
SC15_EXPORT void __cdecl Scena17_Outro(void) {
    Handler(kPart3Steps, static_cast<signed char>(B(kRun)), "Scena17_Outro")();
}

// original 0x56C760: part 3 step 0: the kind-2 sprite at (0x258000,
// 0x1B0000), Field_ViewReset, the transition in, a frame's sleep, the pass
// flags 0x1F, track 0xA0 played, the next step.
SC15_EXPORT void __cdecl Scena17_Outro00(void) {
    SetL(kKind2X, 0x258000);
    SetL(kKind2Z, 0x1B0000);
    SH_CALL(Field_ViewReset)();
    Transition(1);
    SH_CALL(Task_Sleep)(1);
    B(kPass) = 0x1F;
    SH_CALL(Music_Play)(0xA0, 8);
    RunUp();
}

// original 0x56C7B0: step 1: once the wait word is 0, 0x8034E5 = 0 and the
// next step; the rays drawn at radius 0, level 0xFF.
SC15_EXPORT void __cdecl Scena17_Outro01(void) {
    if (W(kWait) == 0) {
        const unsigned char run = B(kRun);
        Step(0);
        B(kRun) = static_cast<unsigned char>(run + 1);
    }
    Rays(0, 0xFF);
}

// original 0x56C7E0: step 2: 0x8034E5 counted up; at 0x32 the next step,
// 0x8034E5 = 0x96 and the rays at 0x1F4; below, at radius 10 times the count.
// The low 16 bits of the radius are what the draw reads; the original's
// upper bits are its eax's.
SC15_EXPORT void __cdecl Scena17_Outro02(void) {
    const unsigned char v = static_cast<unsigned char>(B(kStep) + 1);
    Step(v);
    if (v == 0x32) {
        const unsigned char run = B(kRun);
        Step(0x96);
        B(kRun) = static_cast<unsigned char>(run + 1);
        Rays(0x1F4, 0xFF);
        return;
    }
    Rays(v * 10u, 0xFF);
}

// original 0x56C830: step 3: counted down; at 0 counter 3 0x6E, 0x8034E5 =
// 0x46 and the next step. The rays at 0x1F4.
SC15_EXPORT void __cdecl Scena17_Outro03(void) {
    if (StepDone()) {
        const unsigned char run = B(kRun);
        B(kCounter3) = 0x6E;
        Step(0x46);
        B(kRun) = static_cast<unsigned char>(run + 1);
    }
    Rays(0x1F4, 0xFF);
}

// original 0x56C870: step 4: counted down; at 0 the next step. The rays.
SC15_EXPORT void __cdecl Scena17_Outro04(void) {
    if (StepDone()) RunUp();
    Rays(0x1F4, 0xFF);
}

// original 0x56C8A0: step 5: 0x8034E5 up by 2; at 0x46 or more (unsigned)
// the next step, 0x8034E5 = 0 and the rays at 0x4B0; below, at (count + 0x32)
// times 10.
SC15_EXPORT void __cdecl Scena17_Outro05(void) {
    const unsigned char v = static_cast<unsigned char>(B(kStep) + 2);
    Step(v);
    if (v >= 0x46) {
        const unsigned char run = B(kRun);
        Step(0);
        B(kRun) = static_cast<unsigned char>(run + 1);
        Rays(0x4B0, 0xFF);
        return;
    }
    Rays((v + 0x32u) * 10u, 0xFF);
}

// original 0x56C900: step 6: 0x8034E5 up by 2; at 0x7F or more the next step,
// 0x8034E5 = 0 and the rays at level 0x80; below, at level 0xFF - count (a
// byte).
SC15_EXPORT void __cdecl Scena17_Outro06(void) {
    const unsigned char v = static_cast<unsigned char>(B(kStep) + 2);
    Step(v);
    if (v >= 0x7F) {
        const unsigned char run = B(kRun);
        Step(0);
        B(kRun) = static_cast<unsigned char>(run + 1);
        Rays(0x4B0, 0x80);
        return;
    }
    Rays(0x4B0, static_cast<unsigned char>(0xFF - v));
}

// original 0x56C950: step 7: 0x8034E5 counted up; at 0x80 or more counter 3
// 0x3C, 0x8034E5 = 0x1E, the skip flag 0, the next step, file 0x15C loaded,
// the panel at level 0x80; below, the panel at the count. The rays at 0x4B0,
// level 0x80, after the panel.
SC15_EXPORT void __cdecl Scena17_Outro07(void) {
    const unsigned char v = static_cast<unsigned char>(B(kStep) + 1);
    Step(v);
    if (v >= 0x80) {
        const unsigned char run = static_cast<unsigned char>(B(kRun) + 1);
        B(kCounter3) = 0x3C;
        Step(0x1E);
        B(kRollSkip) = 0;
        B(kRun) = run;
        SH_CALL(LoadDatFile)(0x15C);
        Panel(0x80);
    } else {
        Panel(v);
    }
    Rays(0x4B0, 0x80);
}

// original 0x56C9F0: step 8: Input_Pressed bit 0x20 sets the skip flag; once
// the file is in, 0x8034E5 counted down and at 0 counter 3 counted down: not
// yet 0, 0x8034E5 = 0x1E again, else the next step. The rays, then the
// panel.
SC15_EXPORT void __cdecl Scena17_Outro08(void) {
    if (B(kInputPressed) & 0x20) B(kRollSkip) = 1;
    if (LoadDone() && StepDone()) {
        B(kCounter3) = static_cast<unsigned char>(B(kCounter3) - 1);
        if (B(kCounter3) != 0) Step(0x1E);
        else RunUp();
    }
    Rays(0x4B0, 0x80);
    Panel(0x80);
}

// original 0x56CA60: step 9: on Input_Pressed bit 0x20 or the skip flag, the
// music faded over 0x10, the transition 0xD and the next step. The rays and
// the panel.
SC15_EXPORT void __cdecl Scena17_Outro09(void) {
    if ((B(kInputPressed) & 0x20) || B(kRollSkip) != 0) {
        SH_CALL(Music_FadeOut)(0x10);
        Transition(0xD);
        RunUp();
    }
    Rays(0x4B0, 0x80);
    Panel(0x80);
}

// original 0x56CAC0: step 10: once the wait word is 0, the music stopped,
// Music_Track 0xFF, Port_DroppedCall(0), the pass flags 0, script message 1,
// the next step, the request 2. The rays and the panel.
SC15_EXPORT void __cdecl Scena17_Outro10(void) {
    if (W(kWait) == 0) {
        SH_CALL(Music_FadeOutStop)(0xA);
        B(kMusicTrack) = 0xFF;
        SH_CALL(Port_DroppedCall)(0);
        B(kPass) = 0;
        Msg(1);
        const unsigned char run = static_cast<unsigned char>(B(kRun) + 1);
        B(kRequest) = 2;
        B(kRun) = run;
    }
    Rays(0x4B0, 0x80);
    Panel(0x80);
}

// original 0x56CB30: step 11: once the message is closed: 0x8034E5 0 (the
// message's answer) the next step; else the sound stopped and the task
// restarted at Boot_Task.
SC15_EXPORT void __cdecl Scena17_Outro11(void) {
    if (B(kRequest) == 2) return;
    if (B(kStep) == 0) {
        RunUp();
        return;
    }
    SH_AT(VoidFn, kSoundStop)();
    SH_CALL(Task_Restart)(reinterpret_cast<void*>(static_cast<std::uintptr_t>(kBootTask)));
}

// original 0x56CB60: step 12: part 0, step 0, the task restarted at
// Scena17_EndTask.
SC15_EXPORT void __cdecl Scena17_Outro12(void) {
    B(kPart) = 0;
    B(kRun) = 0;
    SH_CALL(Task_Restart)(reinterpret_cast<void*>(static_cast<std::uintptr_t>(kEndTask)));
}

// original 0x56CB80: chapter 17's vtable slot 1 (PSX 0x801F88C4):
// Scena17_Objects[object +0x86] (object, the flag row); one entry, 0x43C9F0.
SC15_EXPORT void __cdecl Scena17_ObjectTrigger(unsigned char* object) {
    CallObject(kObjects17, object, "Scena17_ObjectTrigger");
}

// original 0x56CBA0: a fade over the frame: with `on` (a byte) a TILE of the
// screen (320.0 x 240.0) at level * 2 (a byte), semi-transparent, under
// tpage 0x14B; then two sprites of the frame's copy (256 x 240 at 0.0 under
// tpage 0x12B, 64 x 240 at 256.0 under 0x12F) at `level`, semi-transparent
// when `on`. Each draw-mode packet to OT slot 2 first.
SC15_EXPORT void __cdecl Scena17_DrawFade(unsigned on, unsigned level) {
    const unsigned char a = static_cast<unsigned char>(on), c = static_cast<unsigned char>(level);
    if (a != 0) {
        DrawMode(0x14B, 0);
        Commit(2, 0xC);
        unsigned char* const p = Packet();
        SH_CALL(Gpu_SetTile)(p);
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        Rgb(p, 4, static_cast<unsigned char>(c << 1));
        SetL(p + 8, 0);
        SetL(p + 0xC, 0);
        SetL(p + 0x14, 0x43A00000);
        SetL(p + 0x18, 0x43700000);
        Commit(2, 0x1C);
    }
    DrawMode(0x12B, 0);
    Commit(2, 0xC);
    unsigned char* p = Packet();
    SH_CALL(Gpu_SetSprt)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, a);
    SetL(p + 8, 0);
    SetL(p + 0xC, 0);
    SetW(p + 0x18, 0x100);
    SetW(p + 0x1A, 0xF0);
    p[0x14] = 0;
    p[0x15] = 0;
    Rgb(p, 4, c);
    Commit(2, 0x1C);
    DrawMode(0x12F, 0);
    Commit(2, 0xC);
    p = Packet();
    SH_CALL(Gpu_SetSprt)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, a);
    SetL(p + 8, 0x43800000);
    SetL(p + 0xC, 0);
    SetW(p + 0x18, 0x40);
    SetW(p + 0x1A, 0xF0);
    p[0x14] = 0;
    p[0x15] = 0;
    Rgb(p, 4, c);
    Commit(2, 0x1C);
}

// original 0x56CCF0: a disc of 32 rays around the kind-2 sprite: the GTE
// matrix pushed; the sprite's point ((x >> 9) - 0x4000, (z >> 9) - 0x4000,
// -(elevation / 2)) through Gte_RotTrans into a matrix's translation, the
// rotation of angles (0, 0, 0) composed with Camera_Matrix and both set; a
// draw mode (dtd, tpage 0x40) to OT slot 2; then for each angle 0, 0x80, ..,
// 0xF80: a POLY_G3 from the centre (black) to the rim at `radius` (its low 16
// bits) coloured `level`, and a POLY_F4 from the rim to 1600 coloured
// `level`, its first two corners copied from the triangle's, both
// semi-transparent to OT slot 2; the matrix popped.
SC15_EXPORT void __cdecl Scena17_DrawRays(unsigned radius, unsigned level) {
    SH_CALL(Gte_PushMatrix)();
    short vec[4];
    short angles[4];
    struct { short m[9]; short pad; long t[3]; } mat;
    long flag;
    const int32_t x = static_cast<int32_t>(L(kKind2X));
    angles[0] = 0;
    angles[1] = 0;
    vec[0] = static_cast<short>((x >> 9) - 0x4000);
    const int32_t z = static_cast<int32_t>(L(kKind2Z));
    angles[2] = 0;
    vec[1] = static_cast<short>((z >> 9) - 0x4000);
    const long e = SH_CALL(AreaMap_Elevation)(x, z);
    vec[2] = static_cast<short>(-(static_cast<int32_t>(static_cast<short>(e)) / 2));
    SH_CALL(Gte_RotTrans)(vec, mat.t);
    (void)flag;
    SH_CALL(Gte_RotMatrix)(angles, mat.m);
    SH_CALL(Gte_MulMatrix0)(reinterpret_cast<const short*>(At(0x905E40)), mat.m, mat.m);
    SH_CALL(Gte_SetRotMatrix)(reinterpret_cast<const unsigned long*>(&mat));
    SH_CALL(Gte_SetTransMatrix)(reinterpret_cast<const unsigned long*>(&mat));
    SH_CALL(Gpu_SetDrawMode)(Packet(), 0, 1, 0x40, 0);
    Commit(2, 0xC);
    const int32_t r = static_cast<int32_t>(radius & 0xFFFFu);
    const unsigned char c = static_cast<unsigned char>(level);
    unsigned char* const v = At(kVertices);
    long depth, flags;
    for (int32_t a = 0; a < 0x1000; a += 0x80) {
        SetW(v, 0);
        SetW(v + 2, 0);
        SetW(v + 4, 0);
        SetW(v + 8, Scaled(SH_CALL(Math_Cos)(a), r));
        const unsigned s1 = Scaled(SH_CALL(Math_Sin)(a), r);
        SetW(v + 0xC, 0);
        SetW(v + 0xA, s1);
        SetW(v + 0x10, Scaled(SH_CALL(Math_Cos)(a + 0x80), r));
        const unsigned s2 = Scaled(SH_CALL(Math_Sin)(a + 0x80), r);
        unsigned char* const tri = Packet();
        SetW(v + 0x14, 0);
        SetW(v + 0x12, s2);
        SH_CALL(Gpu_SetPolyG3)(tri);
        SH_CALL(Gpu_SetSemiTrans)(tri, 1);
        SH_CALL(Gte_RotTransPers3)(reinterpret_cast<const short*>(v), reinterpret_cast<const short*>(v + 8),
                                   reinterpret_cast<const short*>(v + 0x10), reinterpret_cast<float*>(tri + 8),
                                   reinterpret_cast<float*>(tri + 0x18), reinterpret_cast<float*>(tri + 0x28), &depth);
        SH_CALL(Gte_PrimDepths3_10B)(tri);
        Rgb(tri, 4, 0);
        Rgb(tri, 0x14, c);
        Rgb(tri, 0x24, c);
        Commit(2, 0x34);
        SetW(v, 0);
        SetW(v + 2, 0);
        SetW(v + 4, 0);
        SetW(v + 8, Scaled(SH_CALL(Math_Cos)(a), 1600));
        SetW(v + 0xA, Scaled(SH_CALL(Math_Sin)(a), 1600));
        SetW(v + 0xC, 0);
        SetW(v + 0x10, Scaled(SH_CALL(Math_Cos)(a + 0x80), 1600));
        const unsigned s4 = Scaled(SH_CALL(Math_Sin)(a + 0x80), 1600);
        SetW(v + 0x14, 0);
        unsigned char* const quad = Packet();
        SetW(v + 0x12, s4);
        SH_CALL(Gpu_SetPolyF4)(quad);
        SH_CALL(Gpu_SetSemiTrans)(quad, 1);
        SH_CALL(Gte_RotTransPers3)(reinterpret_cast<const short*>(v), reinterpret_cast<const short*>(v + 8),
                                   reinterpret_cast<const short*>(v + 0x10), reinterpret_cast<float*>(quad + 8),
                                   reinterpret_cast<float*>(quad + 0x20), reinterpret_cast<float*>(quad + 0x2C), &flags);
        SH_CALL(Gte_StoreDepthF3)(reinterpret_cast<float*>(quad + 0x10), reinterpret_cast<float*>(quad + 0x28),
                                  reinterpret_cast<float*>(quad + 0x34));
        // One dword at a time in the original's order: in the game the two
        // primitives are apart, but nothing says so to the copy.
        SetL(quad + 8, L(tri + 0x18));
        SetL(quad + 0x14, L(tri + 0x28));
        SetL(quad + 0xC, L(tri + 0x1C));
        SetL(quad + 0x18, L(tri + 0x2C));
        SetL(quad + 0x10, L(tri + 0x20));
        SetL(quad + 0x1C, L(tri + 0x30));
        Rgb(quad, 4, c);
        Commit(2, 0x38);
    }
    SH_CALL(Gte_PopMatrix)();
}

// original 0x56CFF0: the letterbox: two black TILEs of 320.0 x 30.0, at y 0.0
// and 210.0, to OT slot 1.
SC15_EXPORT void __cdecl Scena17_DrawLetterbox(void) {
    unsigned char* p = Packet();
    SH_CALL(Gpu_SetTile)(p);
    Rgb(p, 4, 0);
    SetL(p + 8, 0);
    SetL(p + 0xC, 0);
    SetL(p + 0x14, 0x43A00000);
    SetL(p + 0x18, 0x41F00000);
    Commit(1, 0x1C);
    p = Packet();
    SH_CALL(Gpu_SetTile)(p);
    Rgb(p, 4, 0);
    SetL(p + 8, 0);
    SetL(p + 0xC, 0x43520000);
    SetL(p + 0x14, 0x43A00000);
    SetL(p + 0x18, 0x41F00000);
    Commit(1, 0x1C);
}

// original 0x56D070: with the pass flags set, a 64 x 32 sprite at (x, y)
// (s16s) from (0, 0) under tpage 0xBD and CLUT 0x7B40, at `level`,
// semi-transparent by the byte `semi`, to OT slot 2 after its draw mode.
SC15_EXPORT void __cdecl Scena17_DrawPanel(int x, int y, unsigned level, unsigned semi) {
    if (B(kPass) == 0) return;
    DrawMode(0xBD, 0);
    Commit(2, 0xC);
    unsigned char* const p = Packet();
    SH_CALL(Gpu_SetSprt)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, semi & 0xFFu);
    SetW(p + 0x18, 0x40);
    SetCoord(p + 8, static_cast<unsigned>(x));
    SetW(p + 0x1A, 0x20);
    p[0x14] = 0;
    p[0x15] = 0;
    SetW(p + 0x16, 0x7B40);
    SetCoord(p + 0xC, static_cast<unsigned>(y));
    Rgb(p, 4, static_cast<unsigned char>(level));
    Commit(2, 0x1C);
}

// original 0x56D110: the roll: the sixteen lines from Scena17_RollLines[the
// u16 0x6BC744 + i] (-1 and 0 skipped) drawn at (0x10, 22 * i - the scroll
// byte 0x6BC746) - the y the low 16 bits of 22 * i less the line's pointer
// with its low 16 bits the scroll, as the original subtracts it. With the
// line at the top -1, al 1 (the end); else the scroll up one and at 0x16 the
// next line, al 0. Lines past the -1 are read as the table runs on.
SC15_EXPORT unsigned char __cdecl Scena17_ScrollRoll(void) {
    for (uint32_t i = 0; i < 0x10; ++i) {
        const uint32_t entry = L(kRollLines + 4u * ((L(kRollLine) & 0xFFFFu) + i));
        if (entry == 0xFFFFFFFFu || entry == 0) continue;
        const uint32_t y = i * 22u - ((entry & 0xFFFF0000u) | B(kRollScroll));
        SH_CALL(Scena17_DrawLine)(0x10, static_cast<int>(y),
                                  reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(entry)));
    }
    if (L(kRollLines + 4u * (L(kRollLine) & 0xFFFFu)) == 0xFFFFFFFFu) return 1;
    B(kRollScroll) = static_cast<unsigned char>(B(kRollScroll) + 1);
    if (B(kRollScroll) == 0x16) {
        SetW(kRollLine, W(kRollLine) + 1u);
        B(kRollScroll) = 0;
    }
    return 0;
}

// original 0x56D1A0: a line of the roll at (x, y): a draw mode (tpage 0x3F) to
// OT slot 1, then each character: ' ' 12 pixels; '!' and a digit the palette
// (digit - '0'); '#' and a byte the glyph byte + 4; '@' the logo (88 pixels);
// 'A'..'Z' glyphs 0..25, anything else the glyph c - 0x47 (a byte) - each
// glyph drawn (Scena17_DrawGlyph) and x moved by Scena17_GlyphWidths[glyph].
// The glyph index is unchecked (64 widths).
SC15_EXPORT void __cdecl Scena17_DrawLine(int x, int y, const unsigned char* text) {
    unsigned char palette = 0;
    DrawMode(0x3F, 0);
    Commit(1, 0xC);
    const unsigned char* p = text;
    if (*p == 0) return;
    uint32_t cx = static_cast<uint32_t>(x);
    for (unsigned char ch = *p; ch != 0; ch = *++p) {
        unsigned char glyph;
        switch (ch) {
        case ' ':
            cx += 0xC;
            continue;
        case '!':
            palette = static_cast<unsigned char>(*++p - 0x30);
            continue;
        case '#':
            glyph = static_cast<unsigned char>(*++p + 4);
            break;
        case '@':
            SH_CALL(Scena17_DrawLogo)(static_cast<int>(cx), y);
            cx += 0x58;
            continue;
        default:
            glyph = static_cast<unsigned char>(ch >= 'A' && ch <= 'Z' ? ch - 'A' : ch - 0x47);
            break;
        }
        SH_CALL(Scena17_DrawGlyph)(static_cast<int>(cx), y, palette, glyph);
        cx += B(kGlyphWidths + glyph);
    }
}

// original 0x56D2D0: a glyph: a 16 x 16 sprite (0x5A7730) at (x, y) (s16s) at
// level 0x80, CLUT 0x7B80 | (palette & 0x3F), texture (glyph << 4, glyph &
// 0xF0) - bytes - to OT slot 1.
SC15_EXPORT void __cdecl Scena17_DrawGlyph(int x, int y, unsigned palette, unsigned glyph) {
    unsigned char* const p = Packet();
    SH_AT(void (__cdecl*)(unsigned char*), kPrimSprt16)(p);
    Rgb(p, 4, 0x80);
    SetCoord(p + 8, static_cast<unsigned>(x));
    SetCoord(p + 0xC, static_cast<unsigned>(y));
    SetW(p + 0x16, (palette & 0x3Fu) | 0x7B80u);
    p[0x14] = static_cast<unsigned char>(glyph << 4);
    p[0x15] = static_cast<unsigned char>(glyph & 0xF0);
    Commit(1, 0x18);
}

// original 0x56D350: the logo: an 88 x 18 sprite at (x, y) (s16s) from (0,
// 0x40), CLUT 0x7B85, level 0x80, to OT slot 1.
SC15_EXPORT void __cdecl Scena17_DrawLogo(int x, int y) {
    unsigned char* const p = Packet();
    SH_CALL(Gpu_SetSprt)(p);
    Rgb(p, 4, 0x80);
    SetW(p + 0x16, 0x7B85);
    SetCoord(p + 8, static_cast<unsigned>(x));
    p[0x14] = 0;
    p[0x15] = 0x40;
    SetW(p + 0x18, 0x58);
    SetCoord(p + 0xC, static_cast<unsigned>(y));
    SetW(p + 0x1A, 0x12);
    Commit(1, 0x1C);
}

// original 0x56D3B0: the task after the roll (Scena17_Outro12 restarts the
// field task here): Game_Step 0, the task's private words cleared, the
// windows reset; then every frame Scena17_EndSteps[Game_Step] (a u16,
// unchecked) and a frame's sleep. Never returns (the task is restarted from
// its steps).
SC15_EXPORT void __cdecl Scena17_EndTask(void) {
    SetW(kGameStep, 0);
    SH_CALL(Task_ClearPrivate)();
    SH_CALL(Window_ResetAll)();
    for (;;) {
        Handler(kEndSteps, W(kGameStep), "Scena17_EndTask")();
        SH_CALL(Task_Sleep)(1);
    }
}

// original 0x56D3E0: end step 0: story flag 0x92 set, flags 9..0x16 of
// chapter 15's row (0x904008) cleared, the party's pass (0x533E50), area 0x8F,
// the script flags' bit 0x40 off; the lead member at (0xF0000, 0x218000)
// with +8 = 3, Music_Track 0x6F, the chapter byte 15 and its part, run and
// step 0, the menu bytes (0x929F00 0, 0x929F0C 0xFE), the members' bytes
// 0x929EC2 1 / C3 0, 0x66C7DA 1; file 0x31B loaded a frame at a time; CLUT
// strip rows 1 and 2 copied; Game_Step + 1; the strip dirty.
SC15_EXPORT void __cdecl Scena17_EndReturn(void) {
    SetAt(kStoryRow, 0x92);
    for (unsigned i = 9; i <= 0x16; ++i) ClearAt(kRow3, i);
    SH_AT(VoidFn, kPartyPass)();
    SetW(kArea, 0x8F);
    Clear40();
    SetL(0x802D74, 0xF0000);
    SetL(0x802D78, 0x218000);
    B(0x802D48) = 3;
    B(kMusicTrack) = 0x6F;
    B(kChapter) = 0xF;
    B(kPart) = 0;
    B(kRun) = 0;
    Step(0);
    B(0x929F00) = 0;
    B(0x929F0C) = 0xFE;
    B(0x929EC2) = 1;
    B(0x929EC3) = 0;
    B(0x66C7DA) = 1;
    SH_CALL(LoadDatFile)(0x31B);
    while (!LoadDone()) SH_CALL(Task_Sleep)(1);
    SH_CALL(Gfx_ClutStripCopyRow)(1);
    SH_CALL(Gfx_ClutStripCopyRow)(2);
    SetW(kGameStep, W(kGameStep) + 1u);
    B(kClutDirty) = 1;
}

// original 0x56D4D0: end step 1: the field's task records run, then the menu
// mode's state (ShopMode_Dispatch, a tail jump).
SC15_EXPORT void __cdecl Scena17_EndField(void) {
    SH_CALL(Field_RunTaskRecords)();
    SH_CALL(ShopMode_Dispatch)();
}

// original 0x56D4E0: end step 2: the four counters 0, the sound stopped, the
// task restarted at Boot_Task.
SC15_EXPORT void __cdecl Scena17_EndRestart(void) {
    B(kCounter0) = 0;
    B(kCounter1) = 0;
    B(kCounter2) = 0;
    B(kCounter3) = 0;
    SH_AT(VoidFn, kSoundStop)();
    SH_CALL(Task_Restart)(reinterpret_cast<void*>(static_cast<std::uintptr_t>(kBootTask)));
}

// ===========================================================================
// Chapters 18 and 19

// original 0x56D510: chapter 18's vtable slot 0 (PSX 0x801F6C04): a tail jump
// through Scena18_States - 0 is 0x5646B0 (state 1), 1 Scena18_EnterArea, 2
// Scena18_Run. Unchecked.
SC15_EXPORT void __cdecl Scena18_Frame(void) {
    Handler(kStates18, static_cast<signed char>(B(kState)), "Scena18_Frame")();
}

// original 0x56D520: chapter 18's state 2: a tail jump through Scena18_Runs
// on the s8 MoveScript_Var7 (one entry, a bare ret). Unchecked.
SC15_EXPORT void __cdecl Scena18_Run(void) {
    Handler(kRuns18, static_cast<signed char>(B(kRun)), "Scena18_Run")();
}

// original 0x56D530: chapter 18's vtable slot 1 (PSX 0x801F6CAC):
// Scena18_Objects[object +0x86] (object, the flag row); one entry, a bare ret.
SC15_EXPORT void __cdecl Scena18_ObjectTrigger(unsigned char* object) {
    CallObject(kObjects18, object, "Scena18_ObjectTrigger");
}

// original 0x56D550: chapter 19's vtable slot 0 (PSX 0x801F6C04 in SCENA19):
// a tail jump through Scena19_States - 0 is 0x5646B0, 1 Scena18_EnterArea, 2
// Scena19_Run. Unchecked.
SC15_EXPORT void __cdecl Scena19_Frame(void) {
    Handler(kStates19, static_cast<signed char>(B(kState)), "Scena19_Frame")();
}

// original 0x56D560: chapters 18's and 19's state 1: state 2.
SC15_EXPORT void __cdecl Scena18_EnterArea(void) { B(kState) = 2; }

// original 0x56D570: chapter 19's state 2: a tail jump through Scena19_Runs on
// the s8 MoveScript_Var7 (one entry, Scena19_Run0). Unchecked.
SC15_EXPORT void __cdecl Scena19_Run(void) {
    Handler(kRuns19, static_cast<signed char>(B(kRun)), "Scena19_Run")();
}

// original 0x56D580: chapter 19's run 0: on Input_Pressed bit 8, an effect of
// kind 0x2F.
SC15_EXPORT void __cdecl Scena19_Run0(void) {
    if ((B(kInputPressed) & 8) == 0) return;
    NewEffect(0x2F);
}

// original 0x56D5C0: chapter 19's vtable slot 1 (PSX 0x801F6D10):
// Scena19_Objects[object +0x86] (object, the flag row); one entry, a bare ret.
SC15_EXPORT void __cdecl Scena19_ObjectTrigger(unsigned char* object) {
    CallObject(kObjects19, object, "Scena19_ObjectTrigger");
}

void ScenaSc15_Inject() {
    if (bof3::WantsShadow("scena_sc15")) scena_sc15::SelfTest();
    BOF3_INJECT(Scena15_RecordWord);
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
    BOF3_INJECT(Scena16_ObjectTrigger);
    BOF3_INJECT(Scena17_Frame);
    BOF3_INJECT(Scena17_Start);
    BOF3_INJECT(Scena17_EnterArea);
    BOF3_INJECT(Scena17_Run);
    BOF3_INJECT(Scena17_Intro);
    BOF3_INJECT(Scena17_Intro00);
    BOF3_INJECT(Scena17_Intro01);
    BOF3_INJECT(Scena17_Intro02);
    BOF3_INJECT(Scena17_Intro03);
    BOF3_INJECT(Scena17_Intro04);
    BOF3_INJECT(Scena17_Intro05);
    BOF3_INJECT(Scena17_Intro06);
    BOF3_INJECT(Scena17_Intro07);
    BOF3_INJECT(Scena17_Intro08);
    BOF3_INJECT(Scena17_Intro09);
    BOF3_INJECT(Scena17_Intro10);
    BOF3_INJECT(Scena17_Intro11);
    BOF3_INJECT(Scena17_Intro12);
    BOF3_INJECT(Scena17_Intro13);
    BOF3_INJECT(Scena17_Intro14);
    BOF3_INJECT(Scena17_Intro15);
    BOF3_INJECT(Scena17_Roll);
    BOF3_INJECT(Scena17_Roll0);
    BOF3_INJECT(Scena17_Roll1);
    BOF3_INJECT(Scena17_Roll2);
    BOF3_INJECT(Scena17_Outro);
    BOF3_INJECT(Scena17_Outro00);
    BOF3_INJECT(Scena17_Outro01);
    BOF3_INJECT(Scena17_Outro02);
    BOF3_INJECT(Scena17_Outro03);
    BOF3_INJECT(Scena17_Outro04);
    BOF3_INJECT(Scena17_Outro05);
    BOF3_INJECT(Scena17_Outro06);
    BOF3_INJECT(Scena17_Outro07);
    BOF3_INJECT(Scena17_Outro08);
    BOF3_INJECT(Scena17_Outro09);
    BOF3_INJECT(Scena17_Outro10);
    BOF3_INJECT(Scena17_Outro11);
    BOF3_INJECT(Scena17_Outro12);
    BOF3_INJECT(Scena17_ObjectTrigger);
    BOF3_INJECT(Scena17_DrawFade);
    BOF3_INJECT(Scena17_DrawRays);
    BOF3_INJECT(Scena17_DrawLetterbox);
    BOF3_INJECT(Scena17_DrawPanel);
    BOF3_INJECT(Scena17_ScrollRoll);
    BOF3_INJECT(Scena17_DrawLine);
    BOF3_INJECT(Scena17_DrawGlyph);
    BOF3_INJECT(Scena17_DrawLogo);
    BOF3_INJECT(Scena17_EndTask);
    BOF3_INJECT(Scena17_EndReturn);
    BOF3_INJECT(Scena17_EndField);
    BOF3_INJECT(Scena17_EndRestart);
    BOF3_INJECT(Scena18_Frame);
    BOF3_INJECT(Scena18_Run);
    BOF3_INJECT(Scena18_ObjectTrigger);
    BOF3_INJECT(Scena19_Frame);
    BOF3_INJECT(Scena18_EnterArea);
    BOF3_INJECT(Scena19_Run);
    BOF3_INJECT(Scena19_Run0);
    BOF3_INJECT(Scena19_ObjectTrigger);
}

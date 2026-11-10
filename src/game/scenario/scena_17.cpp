// Moved here by tools/move_funcs.py from scena_sc15.cpp (docs/source-layout-round.md).
#include "game/scenario/scena_17.h"

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

// Group HT of the button round: a face or shoulder bit the original tests by
// a constant is Japan's, asked through σ (docs/button-tests.md).
using bof3x::input::sigma::Sigma;

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

// kByteFD: now Cond_ByteFD (symbols.toml). Cond_ByteFD
// The script counters (MoveScript_CounterOps) and the chapters' other cells.
constexpr uint32_t kCounter0 = 0x903848;

constexpr uint32_t kCounter1 = 0x903849;

constexpr uint32_t kCounter2 = 0x90384A;

constexpr uint32_t kCounter3 = 0x90384B;

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

// kRedraw: now MapView_Redraw (symbols.toml). MapView_Redraw
// kKind2Z: now Field_Kind2Z (symbols.toml). Field_Kind2Z
// kKind2X: now Field_Kind2X (symbols.toml). Field_Kind2X
// kByteFE: now Cond_ByteFE (symbols.toml). Cond_ByteFE
// kMusicTrack: now Music_Track (symbols.toml). Music_Track
// kAreaMusic: now AreaW1f_MusicCurrent (symbols.toml). the byte the area changes set with the track to play
// kByteEE0: now ModeFlow_AreaTransition (symbols.toml). set with some area changes
// kByte7F98: now Area_EnterTransition (symbols.toml).
// kPartyByte: now AreaW0b_PartySet (symbols.toml).
constexpr uint32_t kFrame = 0x937F94;       // Frame_Counter

constexpr uint32_t kInputPressed = 0x7E1BEC;   // Input_Pressed

// kHold: now Field_Kind2Hold (symbols.toml). Field_Kind2Hold
constexpr uint32_t kElevation = 0x929F1C;   // MapView_Elevation (long)

// kSlot: now Scena15_Bytes[3] (symbols.toml). the effect slot a run waits on
constexpr uint32_t kRollLine = 0x6BC744;    // u16: the roll's first line (Scena17_RollLines)

// kObjects16: now Scena16_Objects (symbols.toml). Scena16_Objects, 1
// Chapter 17's.
constexpr uint32_t kRollLines = 0x661A20;   // Scena17_RollLines: string pointers, -1 at 351

// kStates17: now Scena17_States (symbols.toml). Scena17_States, 3
// kParts17: now Scena17_Parts (symbols.toml). Scena17_Parts, 4
// kPart1Steps: now Scena17_Part1Steps (symbols.toml). Scena17_Part1Steps, 16
// kPart2Steps: now Scena17_Part2Steps (symbols.toml). Scena17_Part2Steps, 3
// kPart3Steps: now Scena17_Part3Steps (symbols.toml). Scena17_Part3Steps, 13
// kObjects17: now Scena17_Objects (symbols.toml). Scena17_Objects, 1
constexpr uint32_t kGlyphWidths = 0x66208C; // Scena17_GlyphWidths: 0x40 bytes

constexpr ::bof3::image::CodeVa kBootTask{bof3::addr::Boot_Task};    // Boot_Task's address, as the original pushes it

constexpr ::bof3::image::CodeVa kEndTask{bof3::addr::Scena17_EndTask};     // Scena17_EndTask's, likewise

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

void SetAt(uint32_t row, unsigned n) { SH_CALL(Flags_Set)(AtScenaSc15(row), n); }

void ClearAt(uint32_t row, unsigned n) { SH_CALL(Flags_Clear)(AtScenaSc15(row), n); }

unsigned Area() { return Game_AreaNumber; }

void Step(unsigned v) { MoveScript_PartyRecords[0x65] = static_cast<unsigned char>(v); }

void RunUp() { MoveScript_Var7 = static_cast<unsigned char>(static_cast<unsigned char>(MoveScript_Var7) + 1); }

bool LoadDone() { return SH_CALL(File_LoadDone)() != 0; }

void Msg(unsigned id) { SH_CALL(Msg_OpenScript)(static_cast<unsigned short>(id)); }

void ChangeArea(unsigned area, int32_t x, int32_t z, unsigned flags) { SH_CALL(Field_ChangeArea)(area, x, z, flags); }

void Transition(unsigned k) { SH_CALL(Transition_Start)(static_cast<unsigned char>(k)); }

void Clear40() { SH_CALL(ScriptFlags_Clear40)(); }

void Set40() { SH_CALL(ScriptFlags_Set40)(); }

// Line `i` of the roll: the release's (DIV-0148) or Scena17_RollLines[i].
uint32_t RollEntry(uint32_t i) {
    if (scena_sc15::g_roll_lines.empty()) return L(::bof3::Img(kRollLines) + 4u * i);
    if (i >= scena_sc15::g_roll_lines.size()) bof3::Fatal("DIV-0148: the roll read line %u of %u", i, (unsigned)scena_sc15::g_roll_lines.size());
    return scena_sc15::g_roll_lines[i];
}

unsigned char* Packet() { return Gfx_PacketNext; }

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

constexpr unsigned kFinRadius = 1000;

void FinRays(unsigned radius, unsigned level) {
    if (scena_sc15::g_fin_night == 0) { Rays(radius, level); return; }
    Rays(radius > kFinRadius ? kFinRadius : radius, 0xFF);
}

void Panel(unsigned level) { SH_CALL(Scena17_DrawPanel)(0xE0, 0xBE, level, 1); }

// The ground elevation under the kind-2 sprite handed to MapView_SetElevation.
void ElevationToView() {
    const long z = Field_Kind2Z;
    const long x = Field_Kind2X;
    SH_CALL(MapView_SetElevation)(static_cast<int>(SH_CALL(AreaMap_Elevation)(x, z)));
}

bool Held() { return Field_Kind2Hold != 0; }

// imul then sar 12, in 32 bits (wrapping as the original's does).
unsigned Scaled(int v, int32_t by) {
    return static_cast<unsigned>(static_cast<int32_t>(static_cast<uint32_t>(v) * static_cast<uint32_t>(by)) >> 12);
}

// dec byte [0x8034E5]: true when it reaches 0.
bool StepDone() {
    Step(MoveScript_PartyRecords[0x65] - 1u);
    return MoveScript_PartyRecords[0x65] == 0;
}

}  // namespace

// original 0x56C130: chapter 17's vtable slot 0 (PSX 0x801F775C): a tail jump
// through Scena17_States on the s8 0x8034E2 - 0 Scena17_Start, 1
// Scena17_EnterArea, 2 Scena17_Run. Unchecked.
SC15_EXPORT void __cdecl Scena17_Frame(void) {
    Handler(::bof3::Img(Scena17_States_addr), static_cast<signed char>(MoveScript_PartyRecords[0x62]), "Scena17_Frame")();
}

// original 0x56C140: state 0: the CLUT strip restored (Gfx_ClutStripRestore,
// dirty 1), the pass flags 0x1F, area 0xC7 at (0xB60000, 0x128000) facing 7,
// the four counters 0, state 1.
SC15_EXPORT void __cdecl Scena17_Start(void) {
    SH_CALL(Gfx_ClutStripRestore)();
    Gfx_ClutStripDirty = 1;
    Draw_PassFlags = 0x1F;
    ChangeArea(0xC7, 0xB60000, 0x128000, 7);
    SetL(::bof3::Img(kCounter0), 0);
    MoveScript_PartyRecords[0x62] = 1;
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
        Camera_Angles[0] = 0xFD1C;
        SetW(::bof3::Img(kAngle2), 0x12C);
        SH_CALL(MapView_SetElevation)(0x300);
        SH_CALL(AreaMap_Frame)();
        SH_CALL(Field_ObjectsScreen)();
        SH_CALL(Field_DrawFrame)();
        unsigned char rect[8];
        const unsigned y = static_cast<std::uint32_t>(Gfx_BufferIndex) * 240u;
        SetW(rect, 0);
        SetW(rect + 2, y);
        SetW(rect + 4, 0x140);
        SetW(rect + 6, 0xF0);
        SH_CALL(Gpu_SetDrawMove)(Packet(), rect, 0x2C0, 0);
        Commit(5, 0x18);
    }
    Draw_PassFlags = 0;
    SH_CALL(Port_DroppedCall)(2);
    MoveScript_PartyRecords[0x62] = 2;
    MoveScript_PartyRecords[0x63] = 1;
}

// original 0x56C240: state 2: a tail jump through Scena17_Parts on the s8
// 0x8034E3 - 0 a bare ret, 1 Scena17_Intro, 2 Scena17_Roll, 3 Scena17_Outro.
// Unchecked.
SC15_EXPORT void __cdecl Scena17_Run(void) {
    Handler(::bof3::Img(Scena17_Parts_addr), static_cast<signed char>(MoveScript_PartyRecords[0x63]), "Scena17_Run")();
}

// original 0x56C250: part 1: Scena17_Part1Steps[MoveScript_Var7] (s8,
// unchecked), then the letterbox (a tail jump).
SC15_EXPORT void __cdecl Scena17_Intro(void) {
    Handler(::bof3::Img(Scena17_Part1Steps_addr), MoveScript_Var7, "Scena17_Intro")();
    SH_CALL(Scena17_DrawLetterbox)();
}

// original 0x56C270: part 1 step 0: once the wait word is 0, the kind-2 sprite
// at (0xB00000, 0x138000), Field_ViewReset, Cond_ByteFE 4, track 0x96 loaded,
// the transition in, the fade drawn (off, 0x80), the next step.
SC15_EXPORT void __cdecl Scena17_Intro00(void) {
    if (Transition_Busy != 0) return;
    Field_Kind2X = 0xB00000;
    Field_Kind2Z = 0x138000;
    SH_CALL(Field_ViewReset)();
    Cond_ByteFE = 4;
    SH_CALL(Music_LoadFile)(0x96);
    Transition(1);
    Fade(0, 0x80);
    RunUp();
}

// original 0x56C2D0: step 1: once the wait word is 0 and the file is in, the
// run's step 0x5A and the next step; the fade drawn every frame.
SC15_EXPORT void __cdecl Scena17_Intro01(void) {
    if (Transition_Busy == 0 && LoadDone()) {
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
        const unsigned char run = static_cast<unsigned char>(MoveScript_Var7);
        Field_Kind2X = 0x940000;
        MoveScript_F3Divisor = 0x20;
        Step(0x80);
        Camera_Angles[0] = 0xFE96;
        SetW(::bof3::Img(kAngle2), 0x200);
        Draw_PassFlags = 0x1F;
        MoveScript_Var7 = static_cast<unsigned char>(run + 1);
    }
    Fade(0, 0x80);
}

// original 0x56C3B0: step 4: counted down, the fade drawn on at that level;
// at 0 (read again after the draw) the next step.
SC15_EXPORT void __cdecl Scena17_Intro04(void) {
    const unsigned char v = static_cast<unsigned char>(MoveScript_PartyRecords[0x65] - 1);
    Step(v);
    Fade(1, v);
    if (MoveScript_PartyRecords[0x65] == 0) RunUp();
}

// original 0x56C3E0: step 5: once Field_Kind2Hold is 0, x 0x8C0000 and the
// next step.
SC15_EXPORT void __cdecl Scena17_Intro05(void) {
    if (Held()) return;
    const unsigned char run = static_cast<unsigned char>(MoveScript_Var7);
    Field_Kind2X = 0x8C0000;
    MoveScript_Var7 = static_cast<unsigned char>(run + 1);
}

// original 0x56C400: step 6: the camera's angle 0 turned by -5 every frame;
// once Field_Kind2Hold (read before the turn) is 0, x 0x540000, counter 3
// 0xA and the next step.
SC15_EXPORT void __cdecl Scena17_Intro06(void) {
    const bool held = Held();
    Camera_Angles[0] = static_cast<std::uint16_t>(Camera_Angles[0]) - 5u;
    if (held) return;
    const unsigned char run = static_cast<unsigned char>(MoveScript_Var7);
    Field_Kind2X = 0x540000;
    B(::bof3::Img(kCounter3)) = 0xA;
    MoveScript_Var7 = static_cast<unsigned char>(run + 1);
}

// original 0x56C430: step 7: once Field_Kind2Hold is 0 and counter 3 is 0x14,
// MoveScript_F3Divisor 0x10, x 0x3C0000, the next step.
SC15_EXPORT void __cdecl Scena17_Intro07(void) {
    if (Held()) return;
    if (B(::bof3::Img(kCounter3)) != 0x14) return;
    const unsigned char run = static_cast<unsigned char>(MoveScript_Var7);
    MoveScript_F3Divisor = 0x10;
    Field_Kind2X = 0x3C0000;
    MoveScript_Var7 = static_cast<unsigned char>(run + 1);
}

// original 0x56C470: step 8: once Field_Kind2Hold is 0, the view to the
// ground's elevation, the next step and x 0x2E0000; while it holds, the view's
// elevation down one every other frame.
SC15_EXPORT void __cdecl Scena17_Intro08(void) {
    if (Held()) {
        if (B(::bof3::Img(kFrame)) & 1) MapView_Elevation = static_cast<std::uint32_t>(MapView_Elevation) - 1u;
        return;
    }
    ElevationToView();
    const unsigned char run = static_cast<unsigned char>(MoveScript_Var7);
    Field_Kind2X = 0x2E0000;
    MoveScript_Var7 = static_cast<unsigned char>(run + 1);
}

// original 0x56C4C0: step 9: once Field_Kind2Hold is 0, the view to the
// ground's elevation, x 0x260000, Cond_ByteFE 1, MoveScript_FAWord = (the
// elevation at (0x260000, z) - MapView_Elevation) / 128 (signed, toward 0),
// the next step; while it holds, the view's elevation up 2 a frame and one
// more every fourth.
SC15_EXPORT void __cdecl Scena17_Intro09(void) {
    if (Held()) {
        const uint32_t e = static_cast<std::uint32_t>(MapView_Elevation) + 2u;
        const unsigned char f = B(::bof3::Img(kFrame));
        MapView_Elevation = e;
        if ((f & 3) == 0) SetL(::bof3::Img(kElevation), e + 1u);
        return;
    }
    ElevationToView();
    const long z = Field_Kind2Z;
    Field_Kind2X = 0x260000;
    const long e = SH_CALL(AreaMap_Elevation)(0x260000, z);
    const int32_t d = static_cast<int32_t>(static_cast<uint32_t>(static_cast<int32_t>(static_cast<short>(e))) - static_cast<std::uint32_t>(MapView_Elevation));
    Cond_ByteFE = 1;
    MoveScript_FAWord = static_cast<unsigned>(d / 128);
    RunUp();
}

// original 0x56C550: step 10: once Field_Kind2Hold is 0, the view to the
// ground, MoveScript_F3Divisor 0x10, x 0x190000, MoveScript_FAWord 0, the
// next step; while it holds, angle 0 down one and the camera distance down 6
// a frame.
SC15_EXPORT void __cdecl Scena17_Intro10(void) {
    if (Held()) {
        Camera_Angles[0] = static_cast<std::uint16_t>(Camera_Angles[0]) - 1u;
        Camera_Distance = static_cast<std::uint16_t>(Camera_Distance) - 6u;
        return;
    }
    ElevationToView();
    const unsigned char run = static_cast<unsigned char>(MoveScript_Var7);
    MoveScript_F3Divisor = 0x10;
    Field_Kind2X = 0x190000;
    MoveScript_FAWord = 0;
    MoveScript_Var7 = static_cast<unsigned char>(run + 1);
}

// original 0x56C5B0: step 11: once Field_Kind2Hold is 0, Cond_ByteFE 2 and the
// next step; while it holds, the view's elevation up 2 a frame to at most
// 0x540 (unsigned).
SC15_EXPORT void __cdecl Scena17_Intro11(void) {
    if (Held()) {
        const uint32_t e = static_cast<std::uint32_t>(MapView_Elevation) + 2u;
        MapView_Elevation = e;
        if (e > 0x540) MapView_Elevation = 0x540;
        return;
    }
    const unsigned char run = static_cast<unsigned char>(MoveScript_Var7);
    Cond_ByteFE = 2;
    MoveScript_Var7 = static_cast<unsigned char>(run + 1);
}

// original 0x56C5F0: step 12: on counter 3 0x64, Cond_ByteFE 3 and the next
// step.
SC15_EXPORT void __cdecl Scena17_Intro12(void) {
    if (B(::bof3::Img(kCounter3)) != 0x64) return;
    const unsigned char run = static_cast<unsigned char>(MoveScript_Var7);
    Cond_ByteFE = 3;
    MoveScript_Var7 = static_cast<unsigned char>(run + 1);
}

// original 0x56C610: step 13: the view's elevation up 0x10 a frame; past
// 0xCC0 (unsigned) 0x8034E5 = 0xF and the next step. MapView_Redraw 2.
SC15_EXPORT void __cdecl Scena17_Intro13(void) {
    const uint32_t e = static_cast<std::uint32_t>(MapView_Elevation) + 0x10u;
    MapView_Elevation = e;
    if (e > 0xCC0) {
        const unsigned char run = static_cast<unsigned char>(MoveScript_Var7);
        Step(0xF);
        MoveScript_Var7 = static_cast<unsigned char>(run + 1);
    }
    MapView_Redraw = 2;
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
    if (Transition_Busy != 0) return;
    SH_CALL(Music_FadeOutStop)(0xA);
    const unsigned char part = static_cast<unsigned char>(MoveScript_PartyRecords[0x63] + 1);
    Music_Track = 0xFF;
    Draw_PassFlags = 0;
    Cond_ByteFE = 6;
    MoveScript_PartyRecords[0x63] = part;
    MoveScript_Var7 = 0;
}

// original 0x56C6B0: part 2: Scena17_Part2Steps[MoveScript_Var7] (s8,
// unchecked), then the letterbox (a tail jump).
SC15_EXPORT void __cdecl Scena17_Roll(void) {
    Handler(::bof3::Img(Scena17_Part2Steps_addr), MoveScript_Var7, "Scena17_Roll")();
    SH_CALL(Scena17_DrawLetterbox)();
}

// original 0x56C6D0: part 2 step 0: stream 8 loaded, the roll at line 0 and
// scroll 0, the next step.
SC15_EXPORT void __cdecl Scena17_Roll0(void) {
    SH_CALL(Sound_LoadStream)(8);
    SetW(::bof3::Img(kRollLine), 0);
    Scena15_Bytes[6] = 0;
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
    if (Transition_Busy == 0) {
        const unsigned char part = MoveScript_PartyRecords[0x63];
        MoveScript_Var7 = 0;
        MoveScript_PartyRecords[0x63] = static_cast<unsigned char>(part + 1);
        return;
    }
    SH_CALL(Scena17_ScrollRoll)();
}

// original 0x56C750: part 3: a tail jump through Scena17_Part3Steps on the s8
// MoveScript_Var7. Unchecked.
SC15_EXPORT void __cdecl Scena17_Outro(void) {
    Handler(::bof3::Img(Scena17_Part3Steps_addr), MoveScript_Var7, "Scena17_Outro")();
}

// original 0x56C760: part 3 step 0: the kind-2 sprite at (0x258000,
// 0x1B0000), Field_ViewReset, the transition in, a frame's sleep, the pass
// flags 0x1F, track 0xA0 played, the next step.
SC15_EXPORT void __cdecl Scena17_Outro00(void) {
    Field_Kind2X = 0x258000;
    Field_Kind2Z = 0x1B0000;
    SH_CALL(Field_ViewReset)();
    Transition(1);
    SH_CALL(Task_Sleep)(1);
    Draw_PassFlags = 0x1F;
    SH_CALL(Music_Play)(0xA0, 8);
    RunUp();
}

// original 0x56C7B0: step 1: once the wait word is 0, 0x8034E5 = 0 and the
// next step; the rays drawn at radius 0, level 0xFF.
SC15_EXPORT void __cdecl Scena17_Outro01(void) {
    if (Transition_Busy == 0) {
        const unsigned char run = static_cast<unsigned char>(MoveScript_Var7);
        Step(0);
        MoveScript_Var7 = static_cast<unsigned char>(run + 1);
    }
    Rays(0, 0xFF);
}

// original 0x56C7E0: step 2: 0x8034E5 counted up; at 0x32 the next step,
// 0x8034E5 = 0x96 and the rays at 0x1F4; below, at radius 10 times the count.
// The low 16 bits of the radius are what the draw reads; the original's
// upper bits are its eax's.
SC15_EXPORT void __cdecl Scena17_Outro02(void) {
    const unsigned char v = static_cast<unsigned char>(MoveScript_PartyRecords[0x65] + 1);
    Step(v);
    if (v == 0x32) {
        const unsigned char run = static_cast<unsigned char>(MoveScript_Var7);
        Step(0x96);
        MoveScript_Var7 = static_cast<unsigned char>(run + 1);
        Rays(0x1F4, 0xFF);
        return;
    }
    Rays(v * 10u, 0xFF);
}

// original 0x56C830: step 3: counted down; at 0 counter 3 0x6E, 0x8034E5 =
// 0x46 and the next step. The rays at 0x1F4.
SC15_EXPORT void __cdecl Scena17_Outro03(void) {
    if (StepDone()) {
        const unsigned char run = static_cast<unsigned char>(MoveScript_Var7);
        B(::bof3::Img(kCounter3)) = 0x6E;
        Step(0x46);
        MoveScript_Var7 = static_cast<unsigned char>(run + 1);
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
    const unsigned char v = static_cast<unsigned char>(MoveScript_PartyRecords[0x65] + 2);
    Step(v);
    if (v >= 0x46) {
        const unsigned char run = static_cast<unsigned char>(MoveScript_Var7);
        Step(0);
        MoveScript_Var7 = static_cast<unsigned char>(run + 1);
        FinRays(0x4B0, 0xFF);
        return;
    }
    FinRays((v + 0x32u) * 10u, 0xFF);
}

// original 0x56C900: step 6: 0x8034E5 up by 2; at 0x7F or more the next step,
// 0x8034E5 = 0 and the rays at level 0x80; below, at level 0xFF - count (a
// byte).
SC15_EXPORT void __cdecl Scena17_Outro06(void) {
    const unsigned char v = static_cast<unsigned char>(MoveScript_PartyRecords[0x65] + 2);
    Step(v);
    if (v >= 0x7F) {
        const unsigned char run = static_cast<unsigned char>(MoveScript_Var7);
        Step(0);
        MoveScript_Var7 = static_cast<unsigned char>(run + 1);
        FinRays(0x4B0, 0x80);
        return;
    }
    FinRays(0x4B0, static_cast<unsigned char>(0xFF - v));
}

// original 0x56C950: step 7: 0x8034E5 counted up; at 0x80 or more counter 3
// 0x3C, 0x8034E5 = 0x1E, the skip flag 0, the next step, file 0x15C loaded,
// the panel at level 0x80; below, the panel at the count. The rays at 0x4B0,
// level 0x80, after the panel.
SC15_EXPORT void __cdecl Scena17_Outro07(void) {
    const unsigned char v = static_cast<unsigned char>(MoveScript_PartyRecords[0x65] + 1);
    Step(v);
    if (v >= 0x80) {
        const unsigned char run = static_cast<unsigned char>(static_cast<unsigned char>(MoveScript_Var7) + 1);
        B(::bof3::Img(kCounter3)) = 0x3C;
        Step(0x1E);
        Scena15_Bytes[7] = 0;
        MoveScript_Var7 = run;
        SH_CALL(LoadDatFile)(0x15C);
        Panel(0x80);
    } else {
        Panel(v);
    }
    FinRays(0x4B0, 0x80);
}

// original 0x56C9F0: step 8: Input_Pressed bit 0x20 sets the skip flag; once
// the file is in, 0x8034E5 counted down and at 0 counter 3 counted down: not
// yet 0, 0x8034E5 = 0x1E again, else the next step. The rays, then the
// panel.
SC15_EXPORT void __cdecl Scena17_Outro08(void) {
    if (B(::bof3::Img(kInputPressed)) & Sigma(0x20)) Scena15_Bytes[7] = 1;
    if (LoadDone() && StepDone()) {
        B(::bof3::Img(kCounter3)) = static_cast<unsigned char>(B(::bof3::Img(kCounter3)) - 1);
        if (B(::bof3::Img(kCounter3)) != 0) Step(0x1E);
        else RunUp();
    }
    FinRays(0x4B0, 0x80);
    Panel(0x80);
}

// original 0x56CA60: step 9: on Input_Pressed bit 0x20 or the skip flag, the
// music faded over 0x10, the transition 0xD and the next step. The rays and
// the panel.
SC15_EXPORT void __cdecl Scena17_Outro09(void) {
    if ((B(::bof3::Img(kInputPressed)) & Sigma(0x20)) || Scena15_Bytes[7] != 0) {
        SH_CALL(Music_FadeOut)(0x10);
        Transition(0xD);
        RunUp();
    }
    FinRays(0x4B0, 0x80);
    Panel(0x80);
}

// original 0x56CAC0: step 10: once the wait word is 0, the music stopped,
// Music_Track 0xFF, Port_DroppedCall(0), the pass flags 0, script message 1,
// the next step, the request 2. The rays and the panel.
SC15_EXPORT void __cdecl Scena17_Outro10(void) {
    if (Transition_Busy == 0) {
        SH_CALL(Music_FadeOutStop)(0xA);
        Music_Track = 0xFF;
        SH_CALL(Port_DroppedCall)(0);
        Draw_PassFlags = 0;
        Msg(1);
        const unsigned char run = static_cast<unsigned char>(static_cast<unsigned char>(MoveScript_Var7) + 1);
        Field_Request = 2;
        MoveScript_Var7 = run;
    }
    FinRays(0x4B0, 0x80);
    Panel(0x80);
}

// original 0x56CB30: step 11: once the message is closed: 0x8034E5 0 (the
// message's answer) the next step; else the sound stopped and the task
// restarted at Boot_Task.
SC15_EXPORT void __cdecl Scena17_Outro11(void) {
    if (Field_Request == 2) return;
    if (MoveScript_PartyRecords[0x65] == 0) {
        RunUp();
        return;
    }
    SH_AT(VoidFn, kSoundStop.v)();
    SH_CALL(Task_Restart)(reinterpret_cast<void*>(static_cast<std::uintptr_t>(::bof3::Code(kBootTask.v))));
}

// original 0x56CB60: step 12: part 0, step 0, the task restarted at
// Scena17_EndTask.
SC15_EXPORT void __cdecl Scena17_Outro12(void) {
    MoveScript_PartyRecords[0x63] = 0;
    MoveScript_Var7 = 0;
    SH_CALL(Task_Restart)(reinterpret_cast<void*>(static_cast<std::uintptr_t>(::bof3::Code(kEndTask.v))));
}

// original 0x56CB80: chapter 17's vtable slot 1 (PSX 0x801F88C4):
// Scena17_Objects[object +0x86] (object, the flag row); one entry, 0x43C9F0.
SC15_EXPORT void __cdecl Scena17_ObjectTrigger(unsigned char* object) {
    CallObject(::bof3::Img(Scena17_Objects_addr), object, "Scena17_ObjectTrigger");
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
        SetL(p + 8, std::bit_cast<std::uint32_t>(Widescreen_FillX()));   // DIV-0041: (-53, 0) 426 wide under the wide picture
        SetL(p + 0xC, 0);
        SetL(p + 0x14, std::bit_cast<std::uint32_t>(Widescreen_FillWidth()));   // 0x43A00000, 320.0f narrow
        SetL(p + 0x18, 0x43700000);
        Commit(2, 0x1C);
    }
    DrawMode(0x12B, 0);
    Commit(2, 0xC);
    unsigned char* p = Packet();
    SH_CALL(Gpu_SetSprt)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, a);
    SetL(p + 8, 0);   // the floats x, y as their bits
    SetL(p + 0xC, 0);
    rv::Store<unsigned>(rv::As<records::GpuSprt>(p)->w, 0x100);   // a SPRT: Gpu_SetSprt's packet
    rv::Store<unsigned>(rv::As<records::GpuSprt>(p)->h, 0xF0);
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
    rv::Store<unsigned>(rv::As<records::GpuSprt>(p)->w, 0x40);   // a SPRT: Gpu_SetSprt's packet
    rv::Store<unsigned>(rv::As<records::GpuSprt>(p)->h, 0xF0);
    p[0x14] = 0;
    p[0x15] = 0;
    Rgb(p, 4, c);
    Commit(2, 0x1C);
    // DIV-0127 (amended 2026-11-07): with the fade off the copy is drawn
    // opaque over the 320 columns and nothing else is meant to show - the
    // field pass is off through the copy's hold, and the one frame step 3
    // turns it on before step 4's dark tile covers it, the live field flashed
    // in the wide bands beside the copy (the owner's "flicker"; measured at
    // 21:9, one frame at full brightness before the bands' fade from black).
    // Two opaque black tiles over the bands keep them black until the fade,
    // which then brings them up with the field. Nothing narrow or in a fuzz
    // (Widescreen_Fill() is 0 there).
    if (a == 0 && Widescreen_Fill() != 0) {
        const float cols = static_cast<float>(Widescreen_Fill());
        for (int side = 0; side < 2; ++side) {
            p = Packet();
            SH_CALL(Gpu_SetTile)(p);
            Rgb(p, 4, 0);
            auto* const tile = rv::As<records::GpuTile>(p);   // a TILE: Gpu_SetTile's packet
            rv::Store<float>(tile->x, side == 0 ? -cols : 320.0f);
            rv::Store<float>(tile->y, 0.0f);
            rv::Store<float>(tile->w, cols);
            rv::Store<float>(tile->h, 240.0f);
            Commit(2, 0x1C);
        }
    }
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
    const int32_t x = Field_Kind2X;
    angles[0] = 0;
    angles[1] = 0;
    vec[0] = static_cast<short>((x >> 9) - 0x4000);
    const int32_t z = Field_Kind2Z;
    angles[2] = 0;
    vec[1] = static_cast<short>((z >> 9) - 0x4000);
    const long e = SH_CALL(AreaMap_Elevation)(x, z);
    vec[2] = static_cast<short>(-(static_cast<int32_t>(static_cast<short>(e)) / 2));
    SH_CALL(Gte_RotTrans)(vec, mat.t);
    (void)flag;
    SH_CALL(Gte_RotMatrix)(angles, mat.m);
    SH_CALL(Gte_MulMatrix0)(reinterpret_cast<const short*>(AtScenaSc15(::bof3::Img(Camera_Matrix_addr))), mat.m, mat.m);
    SH_CALL(Gte_SetRotMatrix)(reinterpret_cast<const unsigned long*>(&mat));
    SH_CALL(Gte_SetTransMatrix)(reinterpret_cast<const unsigned long*>(&mat));
    SH_CALL(Gpu_SetDrawMode)(Packet(), 0, 1, 0x40, 0);
    Commit(2, 0xC);
    const int32_t r = static_cast<int32_t>(radius & 0xFFFFu);
    const unsigned char c = static_cast<unsigned char>(level);
    // DIV-0127 (amended 2026-11-07): the outer quads ran to 1600, which fills
    // the 320-wide picture; under the wide picture they stop short of the
    // band corners, so the rim grows by 10 a column a side (1600 x the fill's
    // width over 320: 2130 at 16:9, 2800 at 21:9). 1600 narrow and in every
    // fuzz (Widescreen_Fill() is 0 there); the disc's radius, the logic, is
    // Capcom's.
    const int32_t outer = 1600 + 10 * static_cast<int32_t>(Widescreen_Fill());
    unsigned char* const v = AtScenaSc15(::bof3::Img(Prim_VertexScratch_addr));
    auto* const sv = rv::As<records::SVector>(v);   // Prim_VertexScratch: four SVECTORs (records_gpu.h SVector)
    long depth, flags;
    for (int32_t a = 0; a < 0x1000; a += 0x80) {
        rv::Store<unsigned>(sv[0].x, 0);
        rv::Store<unsigned>(sv[0].y, 0);
        rv::Store<unsigned>(sv[0].z, 0);
        rv::Store<unsigned>(sv[1].x, Scaled(SH_CALL(Math_Cos)(a), r));
        const unsigned s1 = Scaled(SH_CALL(Math_Sin)(a), r);
        rv::Store<unsigned>(sv[1].z, 0);
        rv::Store<unsigned>(sv[1].y, s1);
        rv::Store<unsigned>(sv[2].x, Scaled(SH_CALL(Math_Cos)(a + 0x80), r));
        const unsigned s2 = Scaled(SH_CALL(Math_Sin)(a + 0x80), r);
        unsigned char* const tri = Packet();
        rv::Store<unsigned>(sv[2].z, 0);
        rv::Store<unsigned>(sv[2].y, s2);
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
        rv::Store<unsigned>(sv[0].x, 0);
        rv::Store<unsigned>(sv[0].y, 0);
        rv::Store<unsigned>(sv[0].z, 0);
        rv::Store<unsigned>(sv[1].x, Scaled(SH_CALL(Math_Cos)(a), outer));
        rv::Store<unsigned>(sv[1].y, Scaled(SH_CALL(Math_Sin)(a), outer));
        rv::Store<unsigned>(sv[1].z, 0);
        rv::Store<unsigned>(sv[2].x, Scaled(SH_CALL(Math_Cos)(a + 0x80), outer));
        const unsigned s4 = Scaled(SH_CALL(Math_Sin)(a + 0x80), outer);
        rv::Store<unsigned>(sv[2].z, 0);
        unsigned char* const quad = Packet();
        rv::Store<unsigned>(sv[2].y, s4);
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
// and 210.0, to OT slot 1. DIV-0127: under the wide picture the two tiles are
// full-frame fills, (Widescreen_FillX(), y) Widescreen_FillWidth() x 30 - the
// rule of DIV-0041's fade tile; 0.0 and 320.0 narrow and in every fuzz.
SC15_EXPORT void __cdecl Scena17_DrawLetterbox(void) {
    const uint32_t x = std::bit_cast<uint32_t>(Widescreen_FillX());        // 0x00000000 narrow
    const uint32_t w = std::bit_cast<uint32_t>(Widescreen_FillWidth());    // 0x43A00000 narrow
    unsigned char* p = Packet();
    SH_CALL(Gpu_SetTile)(p);
    Rgb(p, 4, 0);
    SetL(p + 8, x);
    SetL(p + 0xC, 0);
    SetL(p + 0x14, w);
    SetL(p + 0x18, 0x41F00000);
    Commit(1, 0x1C);
    p = Packet();
    SH_CALL(Gpu_SetTile)(p);
    Rgb(p, 4, 0);
    SetL(p + 8, x);
    SetL(p + 0xC, 0x43520000);
    SetL(p + 0x14, w);
    SetL(p + 0x18, 0x41F00000);
    Commit(1, 0x1C);
}

// original 0x56D070: with the pass flags set, a 64 x 32 sprite at (x, y)
// (s16s) from (0, 0) under tpage 0xBD and CLUT 0x7B40, at `level`,
// semi-transparent by the byte `semi`, to OT slot 2 after its draw mode.
SC15_EXPORT void __cdecl Scena17_DrawPanel(int x, int y, unsigned level, unsigned semi) {
    if (Draw_PassFlags == 0) return;
    DrawMode(0xBD, 0);
    Commit(2, 0xC);
    unsigned char* const p = Packet();
    SH_CALL(Gpu_SetSprt)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, semi & 0xFFu);
    auto* const s = rv::As<records::GpuSprt>(p);   // a SPRT: Gpu_SetSprt's packet
    rv::Store<unsigned>(s->w, 0x40);
    SetCoord(p + 8, static_cast<unsigned>(x));
    rv::Store<unsigned>(s->h, 0x20);
    p[0x14] = 0;
    p[0x15] = 0;
    rv::Store<unsigned>(s->clut, 0x7B40);
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
        const uint32_t entry = RollEntry((L(::bof3::Img(kRollLine)) & 0xFFFFu) + i);
        if (entry == 0xFFFFFFFFu || entry == 0) continue;
        const uint32_t y = i * 22u - ((entry & 0xFFFF0000u) | Scena15_Bytes[6]);
        SH_CALL(Scena17_DrawLine)(0x10, static_cast<int>(y),
                                  reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(entry)));
    }
    if (RollEntry(L(::bof3::Img(kRollLine)) & 0xFFFFu) == 0xFFFFFFFFu) return 1;
    Scena15_Bytes[6] = static_cast<unsigned char>(Scena15_Bytes[6] + 1);
    if (Scena15_Bytes[6] == 0x16) {
        SetW(::bof3::Img(kRollLine), W(::bof3::Img(kRollLine)) + 1u);
        Scena15_Bytes[6] = 0;
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
        cx += B(::bof3::Img(kGlyphWidths) + glyph);
    }
}

// original 0x56D2D0: a glyph: a 16 x 16 sprite (0x5A7730) at (x, y) (s16s) at
// level 0x80, CLUT 0x7B80 | (palette & 0x3F), texture (glyph << 4, glyph &
// 0xF0) - bytes - to OT slot 1.
SC15_EXPORT void __cdecl Scena17_DrawGlyph(int x, int y, unsigned palette, unsigned glyph) {
    unsigned char* const p = Packet();
    SH_AT(void (__cdecl*)(unsigned char*), kPrimSprt16.v)(p);
    Rgb(p, 4, 0x80);
    SetCoord(p + 8, static_cast<unsigned>(x));
    SetCoord(p + 0xC, static_cast<unsigned>(y));
    rv::Store<unsigned>(rv::As<records::GpuSprt>(p)->clut, (palette & 0x3Fu) | 0x7B80u);   // a SPRT_16 (kPrimSprt16): GpuSprt to +0x18
    p[0x14] = static_cast<unsigned char>(glyph << 4);
    p[0x15] = static_cast<unsigned char>(glyph & 0xF0);
    Commit(1, 0x18);
}

// original 0x56D350: the logo: an 88 x 18 sprite at (x, y) (s16s) from (0,
// 0x40), CLUT 0x7B85, level 0x80, to OT slot 1.
SC15_EXPORT void __cdecl Scena17_DrawLogo(int x, int y) {
    unsigned char* const p = Packet();
    SH_CALL(Gpu_SetSprt)(p);
    auto* const s = rv::As<records::GpuSprt>(p);   // a SPRT: Gpu_SetSprt's packet
    Rgb(p, 4, 0x80);
    rv::Store<unsigned>(s->clut, 0x7B85);
    SetCoord(p + 8, static_cast<unsigned>(x));
    p[0x14] = 0;
    p[0x15] = 0x40;
    rv::Store<unsigned>(s->w, 0x58);
    SetCoord(p + 0xC, static_cast<unsigned>(y));
    rv::Store<unsigned>(s->h, 0x12);
    Commit(1, 0x1C);
}

// original 0x56D3B0: the task after the roll (Scena17_Outro12 restarts the
// field task here): Game_Step 0, the task's private words cleared, the
// windows reset; then every frame Scena17_EndSteps[Game_Step] (a u16,
// unchecked) and a frame's sleep. Never returns (the task is restarted from
// its steps).
SC15_EXPORT void __cdecl Scena17_EndTask(void) {
    Game_Step = 0;
    SH_CALL(Task_ClearPrivate)();
    SH_CALL(Window_ResetAll)();
    for (;;) {
        Handler(::bof3::Img(Scena17_EndSteps_addr), Game_Step, "Scena17_EndTask")();
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
    auto* const member0 = ObjTrio;
    SetAt(::bof3::Img(Cond_Flags_addr) + 0xa0, 0x92);
    for (unsigned i = 9; i <= 0x16; ++i) ClearAt(::bof3::Img(Cond_Flags_addr) + 0x78, i);
    SH_AT(VoidFn, kPartyPass.v)();
    Game_AreaNumber = 0x8F;
    Clear40();
    rv::Store<uint32_t>(member0->x, 0xF0000);
    rv::Store<uint32_t>(member0->z, 0x218000);
    member0->direction = 3;
    Music_Track = 0x6F;
    Cond_ByteFA = 0xF;
    MoveScript_PartyRecords[0x63] = 0;
    MoveScript_Var7 = 0;
    Step(0);
    BattleE1_Mode = 0;
    ModeStates_ShopObject = 0xFE;
    B(::bof3::Img(0x929EC2)) = 1;
    ScenaSc1_ShopByteB = 0;
    ShopStates_InnFlag = 1;
    SH_CALL(LoadDatFile)(0x31B);
    while (!LoadDone()) SH_CALL(Task_Sleep)(1);
    SH_CALL(Gfx_ClutStripCopyRow)(1);
    SH_CALL(Gfx_ClutStripCopyRow)(2);
    Game_Step = Game_Step + 1u;
    Gfx_ClutStripDirty = 1;
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
    B(::bof3::Img(kCounter0)) = 0;
    B(::bof3::Img(kCounter1)) = 0;
    B(::bof3::Img(kCounter2)) = 0;
    B(::bof3::Img(kCounter3)) = 0;
    SH_AT(VoidFn, kSoundStop.v)();
    SH_CALL(Task_Restart)(reinterpret_cast<void*>(static_cast<std::uintptr_t>(::bof3::Code(kBootTask.v))));
}

void Scena17_Inject() {
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
}

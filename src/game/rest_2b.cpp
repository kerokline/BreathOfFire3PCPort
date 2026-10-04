// Group R2B of round fourteen (wave two): 38 functions at 0x56E040..0x57F33C -
// the cut's 39 rows for R2B (analysis/round14_cut.tsv) less three that are
// jump-table cases of hosts already ours (0x551E40, 0x553E50, 0x559AD0), and
// two starts in the band no list had (0x57E720, a state of Shisu_PickStates;
// 0x57EDF0, the draw four states jump to), each read to its last instruction
// with capstone (2026-10-04) and fuzzed through the scenario harness's field
// mode (rest_2b_fuzz.cpp). docs/rest_2b.md has them one row each.
//
// Game mode 8's step 8 (Mode8_Step8 0x517340) calls Shisu_ModeDispatch every
// frame: a screen on the menu block 0x929F00 (mode, state +1, step +2, timer
// +4) whose PSX twins lie in the SHISU overlay. Mode 0 counts four items and
// sets two models up (records 0x9398E0 "A" and 0x939960 "B", laid out as the
// sprite records Sprite_ObjectMatrix reads); 1 opens the windows; 2 picks - a
// side, then up to four counts - and shows the two models turning, dropping
// and lighting up; 3 closes; 4 takes the given items, scores them and hands
// the chapter its step. Each model has its own state table (Shisu_ModelBStates
// here; Shisu_ModelAStates' draws past its entry 1 are R2C's).
//
// Every one is a faithful replacement. Where the original indexes a .data
// table by a byte it never bounds (every dispatcher here) ours aborts with a
// message (round9 doc section 6); where it reads memory it never wrote (a
// light matrix half filled, Shisu_DrawModel) ours writes zeros and the doc
// says so (section 7). Every call goes through the harness (SH_CALL / SH_AT),
// so the start-up fuzz can stand recorders in for the callees; Sprite_Current
// and the menu cells are re-read after every call, as the originals re-read
// them.
#include "game/rest_2b.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/rest_2b_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

using U = std::uint32_t;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using Handler = void (__cdecl*)();

// --- the cells ------------------------------------------------------------------------

// The menu block (docs/scenario_harness.md section 7.3).
constexpr U kMode = 0x929F00;          // u8: Shisu_Modes' index
constexpr U kState = 0x929F01;         // u8: the mode's state
constexpr U kStep = 0x929F02;          // u8: Shisu_ShowSteps' index
constexpr U kTimer = 0x929F04;         // u8: the show's fade level

// The screen's own cells 0x9399E0..0x9399FF.
constexpr U kOwned = 0x9399E0;         // u8 x 4: the four items' counts (Inventory_Count)
constexpr U kGiven = 0x9399E4;         // u8 x 4: how many of each the player put in
constexpr U kRounds = 0x9399E8;        // u8: shows run so far (8 at most); also read as a dword & 0xFF
constexpr U kSide = 0x9399E9;          // u8: the pick screen's 0 / 1 choice
constexpr U kCursor = 0x9399EA;        // u8: the count screen's row 0..3 (0xFF backs out)
constexpr U kLevel = 0x9399EB;         // u8: Shisu_ScaleIndex's answer, 9..0x10
constexpr U kScoreA = 0x9399EC;        // s32 x 5: the score's four terms and the score
constexpr U kScoreB = 0x9399F0;
constexpr U kScoreC = 0x9399F4;
constexpr U kScoreD = 0x9399F8;
constexpr U kScore = 0x9399FC;

// The two models (0x80 bytes each, read as Sprite_Current records): +1 the
// state, +6 the "done" flag the steps wait on, +0x34 / +0x38 / +0x3C x, z, y
// (16.16), +0x40 the scale, +0x48 "no scale", +0x50 the quads, +0x54 the
// model's header (count byte +0, flags +3), +0x5D..+0x5F the colour, +0x64 /
// +0x68 / +0x6C the angles.
constexpr U kModelA = 0x9398E0;
constexpr U kModelB = 0x939960;
constexpr U kAState = kModelA + 1, kADone = kModelA + 6, kAY = kModelA + 0x3C, kAColour = kModelA + 0x5D,
            kAAngle = kModelA + 0x6C;
constexpr U kBState = kModelB + 1, kBDone = kModelB + 6, kBY = kModelB + 0x3C, kBScale = kModelB + 0x40,
            kBColour = kModelB + 0x5D, kBAngle = kModelB + 0x6C;
constexpr U kSavedB = 0x6BC878;        // u8 x 3: model B's colour while the show fades
constexpr U kSavedA = 0x6BC87C;        // u8 x 3: model A's

constexpr U kModelFile = 0x628C88;     // pointer: the two models' headers (+0, +8) and quad pointers (+4, +0xC)
constexpr U kLevelByte = 0x904101;     // u8 (save data) the scale index is read from
constexpr U kBackdrop = 0x903A5B;      // u8: Config's Background, Menu_DrawBackdrop's kind
constexpr U kStyle = 0x903A5A;         // s8: the window style (its colour row)
constexpr U kClutShadow = 0x80B7A8;    // 0x40 bytes a style: the window colour's first word
constexpr U kChapterStep = 0x8034E5;   // u8: the scenario step the result hands over
constexpr U kCounterB = 0x90384B;      // u8: the chapter's counter byte (1 low score, 2 high)
constexpr U kRank = 0x903F6A;          // u8: the score's rank 0..5
constexpr U kTriggerCounter = 0x903848;   // u8: the chapters' counter the handlers wait on

// WindowRecords (0x803160, 0x24 a record): records 0..3 are this screen's.
constexpr U kWin0 = 0x803160, kWin1 = 0x803184, kWin2 = 0x8031A8, kWin3 = 0x8031CC;
constexpr U kMessage = 0x803194;       // u16: record 1 +0x10, the message the screen shows

// Image floats (.rdata, read in place): the screen-space offsets the model's
// projected x and y are moved by.
constexpr U kOffsetX = 0x5C4278;
constexpr U kOffsetY = 0x5C4274;

unsigned char& B(U address) { return At(address)[0]; }
U W(U address) { return Word(At(address)); }
void SetW(U address, U v) { SetWord(At(address), v); }
U L(U address) { return static_cast<U>(Long(At(address))); }
void SetL(U address, U v) { SetLong(At(address), static_cast<std::int32_t>(v)); }
U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <class T> T Get(const unsigned char* p, unsigned offset) {
    T v;
    std::memcpy(&v, p + offset, sizeof v);
    return v;
}
template <class T> void Put(unsigned char* p, unsigned offset, T v) { std::memcpy(p + offset, &v, sizeof v); }

// jmp / call [table + 4 * byte]: the table's `entries` handlers, read in place
// (the fuzz swaps the cells for recorders); a Fatal past them, where the
// original jumps through the dword after.
void Run(const char* who, const unsigned long* table, unsigned entries, unsigned index, const char* byte) {
    if (index >= entries)
        bof3::Fatal("%s: %s is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/rest_2b.md section 7)",
                    who, byte, index, entries, (unsigned)Key(table));
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(table[index]))();
}

// --- the callees ------------------------------------------------------------------------
void Sound(unsigned id) { SH_CALL(Sound_PlayEffect)(static_cast<unsigned short>(id)); }
void Backdrop() { SH_CALL(Menu_DrawBackdrop)(B(kBackdrop)); }
void Models() {
    SH_CALL(Shisu_ModelBDispatch)();
    SH_CALL(Shisu_ModelADispatch)();
}
// While a show has run, model A stands on model B: A's y = B's scale * 0xA00
// + B's y (32-bit, wrapping).
void StackModels() {
    if (B(kRounds) != 0) SetL(kAY, L(kBScale) * 0xA00u + L(kBY));
}
unsigned char Max(unsigned char a, unsigned char b) { return a > b ? a : b; }

}  // namespace

// ===========================================================================
// Effect records, a field object trigger, two menu primitives
// ===========================================================================

// original 0x56E040: Field_ObjectTriggers[1] (0x662E20) - the chapters'
// counter 0x903848 = 0xF0; al 0 (the rest of eax the caller's). Its two
// arguments (the object, 0x904030) are not read.
extern "C" unsigned char __cdecl Field_TriggerCounterF0(unsigned char* object, unsigned char* flags) {
    (void)object;
    (void)flags;
    B(kTriggerCounter) = 0xF0;
    return 0;
}

// original 0x57CE10: an effect record of kind 6 - Effect_FindFree's slot n;
// none (0xFF): nothing. Else record n: +0 = 1, +5 = 6, +6 = kind, +0xC / +0x10
// = a / b sign-extended to dwords, +0x2E / +0x30 = the words x / z. al = n.
// Effect_FindFree answers 0..19 or 0xFF; past 19 ours aborts where the
// original would write past Effect_Objects.
extern "C" unsigned char __cdecl Effect_Spawn(unsigned char kind, signed char a, signed char b, short x, short z) {
    const unsigned char slot = SH_CALL(Effect_FindFree)();
    if (slot == 0xFF) return slot;
    if (slot >= 20) bof3::Fatal("Effect_Spawn (0x57CE10): Effect_FindFree answered %u, past the 20 records", slot);
    unsigned char* const r = Effect_Objects + 0x80u * slot;
    r[0] = 1;
    r[5] = 6;
    r[6] = kind;
    Put<std::int32_t>(r, 0xC, a);
    Put<std::int32_t>(r, 0x10, b);
    Put<std::uint16_t>(r, 0x2E, static_cast<std::uint16_t>(x));
    Put<std::uint16_t>(r, 0x30, static_cast<std::uint16_t>(z));
    return slot;
}

// original 0x57CE80: Effect_Spawn's sibling of kind 0x19 - the same record
// with +5 = 0x19 and the dwords x, y, z at +0x34, +0x38, +0x3C.
extern "C" unsigned char __cdecl Effect_SpawnAt(unsigned char kind, signed char a, signed char b, long x, long y, long z) {
    const unsigned char slot = SH_CALL(Effect_FindFree)();
    if (slot == 0xFF) return slot;
    if (slot >= 20) bof3::Fatal("Effect_SpawnAt (0x57CE80): Effect_FindFree answered %u, past the 20 records", slot);
    unsigned char* const r = Effect_Objects + 0x80u * slot;
    r[0] = 1;
    r[5] = 0x19;
    r[6] = kind;
    Put<std::int32_t>(r, 0xC, a);
    Put<std::int32_t>(r, 0x10, b);
    Put<std::int32_t>(r, 0x34, x);
    Put<std::int32_t>(r, 0x38, y);
    Put<std::int32_t>(r, 0x3C, z);
    return slot;
}

// original 0x57CEF0: a horizontal LINE_F2 at the packet cursor from (x, y) to
// (x + w, y) - x and y the arguments' low words signed, w the third's low word
// unsigned, the floats built by fild - grey 0x28 when the fourth argument's
// byte is non-zero, else 0x8C; Gfx_CommitPrim(1, 0x20). The original also
// writes into its own argument slots (scratch for fild), which its two callers
// (community code) pop unread.
extern "C" void __cdecl Menu_DrawGreyHLine(int x, int y, unsigned w, unsigned bright) {
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetLineF2)(p);
    const unsigned char shade = (bright & 0xFF) != 0 ? 0x28 : 0x8C;
    p[4] = shade;
    p[5] = shade;
    p[6] = shade;
    const int x0 = static_cast<short>(x);
    const int y0 = static_cast<short>(y);
    const int width = static_cast<int>(w & 0xFFFF);
    Put<float>(p, 8, static_cast<float>(x0));
    Put<float>(p, 0x14, static_cast<float>(width + x0));
    Put<float>(p, 0x18, static_cast<float>(y0));
    Put<float>(p, 0xC, static_cast<float>(y0));
    SH_CALL(Gfx_CommitPrim)(1, 0x20);
}

// original 0x57D520 (PSX 0x801AFD54): Menu_DrawOutline's sibling with notched
// corners - the window colour (entry 0 of CLUT row (s8) 0x903A5A in the shadow
// 0x80B7A8, 5:5:5 shifted up by 3) in eight Menu_DrawLines at most, abr 2 / 1
// by flag bit 0 as Menu_DrawOutline's. Flag bits 4..7 make the notch words
// n0..n3 3 (else 0): the left edge from y + n0 to y + h - 1 - n2, the bottom
// from x + n2 to x + w - 1 - n3, the right from y + h - n3 up to y + n1 + 1,
// the top from x + w - n1 to x + n0 + 1; a corner's diagonal is drawn when
// the word BEFORE its own is set (bottom-left n2 by n0, bottom-right n3 by
// n1, top-right n1 by n2, top-left n0 by n3). The original builds the
// coordinates from dwords spanning two notch words (n0 | n1 << 16, ...) and
// Menu_DrawLine reads their low words: the same lines.
extern "C" void __cdecl Menu_DrawOutlineNotched(int x, int y, int w, int h, int flags) {
    const int row = static_cast<signed char>(B(kStyle));
    const unsigned char* const entry = At(kClutShadow + static_cast<U>(row * 64));
    const U c = Word(entry);
    const auto r = static_cast<unsigned char>((entry[0] & 0x1F) << 3);
    const auto gr = static_cast<unsigned char>(((c >> 5) & 0x1F) << 3);
    const auto b = static_cast<unsigned char>(((c >> 10) & 0x1F) << 3);
    const bool bit0 = (flags & 1) != 0;
    const int abr1 = bit0 ? 2 : 1, abr2 = bit0 ? 1 : 2;
    const unsigned n = (static_cast<unsigned>(flags) & 0xFF) >> 4;
    const int n0 = n & 1 ? 3 : 0, n1 = n & 2 ? 3 : 0, n2 = n & 4 ? 3 : 0, n3 = n & 8 ? 3 : 0;
    auto line = [&](int x0, int y0, int x1, int y1, int abr) { SH_CALL(Menu_DrawLine)(x0, y0, x1, y1, r, gr, b, abr); };
    const int left_end = y + h - 1 - n2;
    line(x, y + n0, x, left_end, abr1);
    if (n0 != 0) line(x, left_end, x + n2, y + h, abr2);
    const int bottom_end = x + w - 1 - n3;
    line(x + n2, y + h, bottom_end, y + h, abr2);
    if (n1 != 0) line(bottom_end, y + h, x + w, y + h - n3, abr2);
    line(x + w, y + h - n3, x + w, y + n1 + 1, abr2);
    if (n2 != 0) line(x + w, y + n1 + 1, x + w - n1, y, abr1);
    line(x + w - n1, y, x + n0 + 1, y, abr1);
    if (n3 != 0) line(x + n0 + 1, y, x, y + n0, abr1);
}

// ===========================================================================
// Game mode 8's step 8: the mode dispatcher and the five modes
// ===========================================================================

// original 0x57DFF0: jmp through Shisu_Modes by the menu mode 0x929F00
// (read as a dword & 0xFF), unchecked (ours aborts past its 5).
extern "C" void __cdecl Shisu_ModeDispatch(void) {
    Run("Shisu_ModeDispatch (0x57DFF0)", Shisu_Modes, Shisu_Modes_count, B(kMode), "the menu mode 0x929F00");
}

// original 0x57E010: Shisu_Modes[0] - the counts; the side, cursor, rounds and
// the four given counts 0; the scale index from Shisu_ScaleIndex; the models
// set up; then the mode up one with its state and step 0.
extern "C" void __cdecl Shisu_Begin(void) {
    SH_CALL(Shisu_CountItems)();
    B(kSide) = 0;
    B(kCursor) = 0;
    B(kRounds) = 0;
    for (unsigned i = 0; i < 4; ++i) B(kGiven + i) = 0;
    const unsigned char level = SH_CALL(Shisu_ScaleIndex)();
    B(kLevel) = level;
    SH_CALL(Shisu_InitModels)();
    const unsigned char mode = B(kMode);
    B(kState) = 0;
    B(kStep) = 0;
    B(kMode) = static_cast<unsigned char>(mode + 1);
}

// original 0x57E070: Shisu_Modes[1] - jmp through Shisu_OpenStates by the
// state 0x929F01, unchecked (ours aborts past its 2).
extern "C" void __cdecl Shisu_OpenDispatch(void) {
    Run("Shisu_OpenDispatch (0x57E070)", Shisu_OpenStates, Shisu_OpenStates_count, B(kState), "the menu state 0x929F01");
}

// original 0x57E080: Shisu_OpenStates[0] - Transition_Start(3), the windows
// set up, the state up one.
extern "C" void __cdecl Shisu_OpenFade(void) {
    SH_CALL(Transition_Start)(3);
    SH_CALL(Shisu_SetupWindows)();
    B(kState) = static_cast<unsigned char>(B(kState) + 1);
}

// original 0x57E0A0: Shisu_OpenStates[1] - the backdrop; once the wait word is
// 0, the message 0x2B, the mode up one and its state 0.
extern "C" void __cdecl Shisu_OpenWait(void) {
    Backdrop();
    if (MoveScript_WaitWordDA != 0) return;
    const unsigned char mode = B(kMode);
    B(kState) = 0;
    SetW(kMessage, 0x2B);
    B(kMode) = static_cast<unsigned char>(mode + 1);
}

// original 0x57E0E0: Shisu_Modes[3] - jmp through Shisu_CloseStates by the
// state, unchecked (ours aborts past its 2).
extern "C" void __cdecl Shisu_CloseDispatch(void) {
    Run("Shisu_CloseDispatch (0x57E0E0)", Shisu_CloseStates, Shisu_CloseStates_count, B(kState), "the menu state 0x929F01");
}

// original 0x57E0F0: Shisu_CloseStates[0] - the backdrop, Transition_Start(2),
// the windows' flags for closing, the state up one.
extern "C" void __cdecl Shisu_CloseFade(void) {
    Backdrop();
    SH_CALL(Transition_Start)(2);
    SH_CALL(Shisu_CloseWindows)();
    B(kState) = static_cast<unsigned char>(B(kState) + 1);
}

// original 0x57E120: Shisu_CloseStates[1] - while the wait word is set, the
// backdrop; once it is 0, the mode up one with its state 0 and windows 1..3's
// first bytes 0.
extern "C" void __cdecl Shisu_CloseWait(void) {
    if (MoveScript_WaitWordDA != 0) {
        Backdrop();
        return;
    }
    const unsigned char mode = B(kMode);
    B(kState) = 0;
    B(kWin1) = 0;
    B(kMode) = static_cast<unsigned char>(mode + 1);
    B(kWin2) = 0;
    B(kWin3) = 0;
}

// original 0x57E160 (PSX 0x801D1018): Shisu_Modes[4] - with no show run, the
// scenario step 0x8034E5 = 5. Else the four items taken by the counts given
// (Inventory_Remove(0, item, count), a fourth word 0 pushed), the step 0x14,
// the score; a score of 0x32 or less sets the counter byte 0x90384B to 1, a
// higher one to 2 and the rank 0x903F6A to 0 (below 0x5A), 1 (0x78), 2
// (0x96), 3 (0xAA), 4 (0xB4) or 5. Then Window_ResetAll and Game_Step up one.
extern "C" void __cdecl Shisu_Result(void) {
    if (B(kRounds) == 0) {
        B(kChapterStep) = 5;
    } else {
        static const unsigned char kItems[4] = {0x4D, 0x23, 0x24, 0x56};
        for (unsigned i = 0; i < 4; ++i) SH_CALL(Inventory_Remove)(0, kItems[i], B(kGiven + i));
        B(kChapterStep) = 0x14;
        SH_CALL(Shisu_Score)();
        const auto score = static_cast<std::int32_t>(L(kScore));
        if (score <= 0x32) {
            B(kCounterB) = 1;
        } else {
            B(kCounterB) = 2;
            unsigned char rank;
            if (score < 0x5A) rank = 0;
            else if (score < 0x78) rank = 1;
            else if (score < 0x96) rank = 2;
            else if (score < 0xAA) rank = 3;
            else rank = score >= 0xB4 ? 5 : 4;
            B(kRank) = rank;
        }
    }
    SH_CALL(Window_ResetAll)();
    Game_Step = static_cast<unsigned short>(Game_Step + 1);
}

// original 0x57E290: Shisu_Modes[2] - the backdrop, then jmp through
// Shisu_PickStates by the state (read after it), unchecked (ours aborts past
// its 3).
extern "C" void __cdecl Shisu_PickDispatch(void) {
    Backdrop();
    Run("Shisu_PickDispatch (0x57E290)", Shisu_PickStates, Shisu_PickStates_count, B(kState), "the menu state 0x929F01");
}

// original 0x57E2B0: Shisu_PickStates[0] - Input_AutoRepeat(Input_Pressed &
// 0xF000): 0xA000 flips the side (sound 0x101, message 0x2B + side); 0x1000 /
// 0x4000 go to the counts with the cursor at 3 / 0 (sound 0x100, message
// 0x2A). Else, the confirm buttons pressed on side 0: eight shows run - sound
// 0x107; some run - sound 0x103 and the rounds up one; none - with the first
// two counts given, sound 0x103 and the state up two (to the show), else
// sound 0x107 and message 0x31. The confirm buttons on side 1, or the cancel
// buttons: sound 0x103, the mode up one (closing). Then window 0's place by
// the side, model A stacked on B, the two models' states.
extern "C" void __cdecl Shisu_PickSide(void) {
    const U repeat = SH_CALL(Input_AutoRepeat)(static_cast<unsigned>(Input_Pressed) & 0xF000u);
    const unsigned hi = (repeat >> 8) & 0xFF;
    if (hi & 0xA0) {
        Sound(0x101);
        const auto side = static_cast<unsigned char>(B(kSide) ^ 1);
        B(kSide) = side;
        SetW(kMessage, side + 0x2Bu);
    } else if (hi & 0x10) {
        Sound(0x100);
        const unsigned char state = B(kState);
        SetW(kMessage, 0x2A);
        B(kState) = static_cast<unsigned char>(state + 1);
        B(kCursor) = 3;
    } else if (hi & 0x40) {
        Sound(0x100);
        const unsigned char state = B(kState);
        SetW(kMessage, 0x2A);
        B(kState) = static_cast<unsigned char>(state + 1);
        B(kCursor) = 0;
    } else {
        const unsigned pressed = Input_Pressed;
        const unsigned confirm = Field_ConfirmButtons & pressed;
        const unsigned char side = B(kSide);
        if (confirm != 0 && side == 0) {
            const unsigned char rounds = B(kRounds);
            if (rounds >= 8) {
                Sound(0x107);
            } else if (rounds != 0) {
                Sound(0x103);
                B(kRounds) = static_cast<unsigned char>(B(kRounds) + 1);
            } else if (B(kGiven) != 0 && B(kGiven + 1) != 0) {
                Sound(0x103);
                B(kState) = static_cast<unsigned char>(B(kState) + 2);
            } else {
                Sound(0x107);
                SetW(kMessage, 0x31);
            }
        } else if (confirm != 0 || (Field_CancelButtons & pressed) != 0) {
            Sound(0x103);
            const unsigned char mode = B(kMode);
            B(kState) = 0;
            B(kMode) = static_cast<unsigned char>(mode + 1);
        }
    }
    const unsigned side = B(kSide);
    B(kWin0) = 1;
    const U base = L(kWin2 + 4);
    SetW(kWin0 + 4, side * 0x30u + base);
    SetW(kWin0 + 6, W(kWin2 + 6) + 4u);
    StackModels();
    Models();
}

// original 0x57E490: Shisu_PickStates[1] - Input_AutoRepeat(Input_Pressed &
// 0x5000); any button: message 0x2A. 0x1000 / 0x4000 move the cursor up /
// down (sound 0x100); off the top or past row 3: message 0x2B + side, the
// cursor 0xFF and the state down one. Else, confirm without cancel: row r
// takes one of item r into the given counts - row 0 at most one, rows 1..3 at
// most 0x14; row 2 wants row 1 given (message 0x2F), row 3 row 0 (0x30); none
// left: message 0x2E; full: 0x2D; each refusal sound 0x107, each take sound
// 0x103 (row 0's first sets model A's state to 1, row 1's model B's). Then
// window 0 at the cursor's row and the records' cursor bytes, model A stacked,
// the two models' states.
extern "C" void __cdecl Shisu_PickCounts(void) {
    const U repeat = SH_CALL(Input_AutoRepeat)(static_cast<unsigned>(Input_Pressed) & 0x5000u);
    const unsigned pressed = Input_Pressed;
    if (pressed != 0) SetW(kMessage, 0x2A);
    const unsigned hi = (repeat >> 8) & 0xFF;
    auto back = [] {
        SetW(kMessage, B(kSide) + 0x2Bu);
        const unsigned char state = B(kState);
        B(kCursor) = 0xFF;
        B(kState) = static_cast<unsigned char>(state - 1);
    };
    auto refuse = [](unsigned message) {
        SetW(kMessage, message);
        Sound(0x107);
    };
    if (hi & 0x10) {
        Sound(0x100);
        const auto c = static_cast<unsigned char>(B(kCursor) - 1);
        B(kCursor) = c;
        if (static_cast<signed char>(c) < 0) back();
    } else if (hi & 0x40) {
        Sound(0x100);
        const auto c = static_cast<unsigned char>(B(kCursor) + 1);
        B(kCursor) = c;
        if (c > 3) back();
    } else if ((Field_CancelButtons & pressed) == 0 && (Field_ConfirmButtons & pressed) != 0) {
        switch (B(kCursor)) {
        case 0:
            if (B(kOwned) == 0) refuse(0x2E);
            else if (B(kGiven) >= 1) refuse(0x2D);
            else {
                const unsigned char given = B(kGiven), owned = B(kOwned);
                B(kAState) = 1;
                B(kGiven) = static_cast<unsigned char>(given + 1);
                B(kOwned) = static_cast<unsigned char>(owned - 1);
                Sound(0x103);
            }
            break;
        case 1:
            if (B(kOwned + 1) == 0) refuse(0x2E);
            else if (B(kGiven + 1) >= 0x14) refuse(0x2D);
            else {
                if (B(kGiven + 1) == 0) B(kBState) = 1;
                const unsigned char owned = B(kOwned + 1), given = B(kGiven + 1);
                B(kGiven + 1) = static_cast<unsigned char>(given + 1);
                B(kOwned + 1) = static_cast<unsigned char>(owned - 1);
                Sound(0x103);
            }
            break;
        case 2:
            if (B(kGiven + 1) == 0) refuse(0x2F);
            else if (B(kOwned + 2) == 0) refuse(0x2E);
            else if (B(kGiven + 2) >= 0x14) refuse(0x2D);
            else {
                const unsigned char owned = B(kOwned + 2), given = B(kGiven + 2);
                B(kGiven + 2) = static_cast<unsigned char>(given + 1);
                B(kOwned + 2) = static_cast<unsigned char>(owned - 1);
                Sound(0x103);
            }
            break;
        case 3:
            if (B(kGiven) == 0) refuse(0x30);
            else if (B(kOwned + 3) == 0) refuse(0x2E);
            else if (B(kGiven + 3) >= 0x14) refuse(0x2D);
            else {
                const unsigned char owned = B(kOwned + 3), given = B(kGiven + 3);
                B(kGiven + 3) = static_cast<unsigned char>(given + 1);
                B(kOwned + 3) = static_cast<unsigned char>(owned - 1);
                Sound(0x103);
            }
            break;
        default: break;   // the original's `cmp eax, 3; ja`: nothing
        }
    }
    const U base = L(kWin3 + 4);
    const unsigned char c = B(kCursor);
    B(kWin0) = 1;
    SetW(kWin0 + 4, base + 7u);
    B(kWin0 + 0xA) = c;
    B(kWin3 + 0xA) = c;
    SetW(kWin0 + 6, (c + 2u) * 13u + W(kWin3 + 6));
    StackModels();
    Models();
}

// original 0x57E720 (no list had it; Shisu_PickStates[2]): call through
// Shisu_ShowSteps by the step 0x929F02, unchecked (ours aborts past its 5),
// then the two models' states.
extern "C" void __cdecl Shisu_PickShow(void) {
    Run("Shisu_PickShow (0x57E720)", Shisu_ShowSteps, Shisu_ShowSteps_count, B(kStep), "the menu step 0x929F02");
    Models();
}

// original 0x57E740: Shisu_ShowSteps[0] - both models to state 2 (squaring
// their turn), the step up one.
extern "C" void __cdecl Shisu_ShowStart(void) {
    B(kAState) = 2;
    B(kBState) = 2;
    B(kStep) = static_cast<unsigned char>(B(kStep) + 1);
}

// original 0x57E760: Shisu_ShowSteps[1] - both done: both to state 3 (the
// drop), the step up one.
extern "C" void __cdecl Shisu_ShowDrop(void) {
    if (B(kADone) == 0 || B(kBDone) == 0) return;
    B(kAState) = 3;
    B(kBState) = 3;
    B(kStep) = static_cast<unsigned char>(B(kStep) + 1);
}

// original 0x57E790: Shisu_ShowSteps[2] - both landed: both to state 4 (still),
// their colours kept at 0x6BC87C (A) and 0x6BC878 (B), the timer 0, the step
// up one.
extern "C" void __cdecl Shisu_ShowLanded(void) {
    if (B(kADone) == 0 || B(kBDone) == 0) return;
    const unsigned char a1 = B(kAColour + 1), a2 = B(kAColour + 2);
    B(kAState) = 4;
    B(kBState) = 4;
    B(kSavedA) = B(kAColour);
    B(kSavedB) = B(kBColour);
    const unsigned char step = B(kStep);
    B(kSavedA + 1) = a1;
    const unsigned char b1 = B(kBColour + 1);
    B(kSavedA + 2) = a2;
    const unsigned char b2 = B(kBColour + 2);
    B(kSavedB + 1) = b1;
    B(kSavedB + 2) = b2;
    B(kTimer) = 0;
    B(kStep) = static_cast<unsigned char>(step + 1);
}

// The six colour bytes: each kept one or the timer, the larger.
static void Brighten(unsigned char t) {
    const unsigned char a0 = B(kSavedA), a1 = B(kSavedA + 1);
    B(kAColour) = Max(a0, t);
    B(kAColour + 1) = Max(a1, t);
    B(kAColour + 2) = Max(B(kSavedA + 2), t);
    const unsigned char b0 = B(kSavedB), b1 = B(kSavedB + 1);
    B(kBColour) = Max(b0, t);
    B(kBColour + 1) = Max(b1, t);
    B(kBColour + 2) = Max(B(kSavedB + 2), t);
}

// original 0x57E810: Shisu_ShowSteps[3] - every colour byte at least the
// timer; at 0x80 sound 0x200 (the timer read again after it); the timer up 8,
// and when it wraps to 0, 0xF8 and the step up one.
extern "C" void __cdecl Shisu_ShowFlash(void) {
    unsigned char t = B(kTimer);
    Brighten(t);
    if (t == 0x80) {
        Sound(0x200);
        t = B(kTimer);
    }
    t = static_cast<unsigned char>(t + 8);
    B(kTimer) = t;
    if (t == 0) {
        const unsigned char step = B(kStep);
        B(kTimer) = 0xF8;
        B(kStep) = static_cast<unsigned char>(step + 1);
    }
}

// original 0x57E8C0: Shisu_ShowSteps[4] - the same floor, the timer down 8;
// at 0 the kept colours back, model A to state 5 and B to 1 (turning), the
// rounds up one, the state down two (to the pick) and the step 0.
extern "C" void __cdecl Shisu_ShowFade(void) {
    const unsigned char t = B(kTimer);
    const unsigned char a0 = B(kSavedA), a1 = B(kSavedA + 1);
    B(kAColour) = Max(a0, t);
    B(kAColour + 1) = Max(a1, t);
    B(kAColour + 2) = Max(B(kSavedA + 2), t);
    const unsigned char b0 = B(kSavedB), b1 = B(kSavedB + 1);
    B(kBColour) = Max(b0, t);
    B(kBColour + 1) = Max(b1, t);
    const unsigned char b2 = B(kSavedB + 2);
    B(kBColour + 2) = Max(b2, t);
    const auto now = static_cast<unsigned char>(t - 8);
    B(kTimer) = now;
    if (now != 0) return;
    const unsigned char a2 = B(kSavedA + 2);
    B(kBColour) = b0;
    const unsigned char rounds = B(kRounds);
    B(kAColour + 2) = a2;
    const unsigned char state = B(kState);
    B(kAColour) = a0;
    B(kAColour + 1) = a1;
    B(kBColour + 1) = b1;
    B(kBColour + 2) = b2;
    B(kAState) = 5;
    B(kBState) = 1;
    B(kRounds) = static_cast<unsigned char>(rounds + 1);
    B(kState) = static_cast<unsigned char>(state - 2);
    B(kStep) = 0;
}

// original 0x57E9A0: the four counts - Inventory_Count(0, item, 0)'s al for
// the items 0x4D, 0x23, 0x24, 0x56 into 0x9399E0..0x9399E3.
extern "C" void __cdecl Shisu_CountItems(void) {
    static const unsigned char kItems[4] = {0x4D, 0x23, 0x24, 0x56};
    for (unsigned i = 0; i < 4; ++i) {
        const unsigned short n = SH_CALL(Inventory_Count)(0, kItems[i], 0);
        B(kOwned + i) = static_cast<unsigned char>(n);
    }
}

// original 0x57E9F0: WindowRecords 0..3 for the screen (bytes and words as
// stored; record 3's +0x20 the counts' address 0x9399E0).
extern "C" void __cdecl Shisu_SetupWindows(void) {
    B(kWin0) = 0;
    B(kWin0 + 2) = 0;
    SetW(kWin0 + 4, 0);
    SetW(kWin0 + 6, 0);
    SetW(kWin1 + 6, 0xFF9C);
    SetW(kWin2 + 6, 0xFF9C);
    B(kWin0 + 1) = 1;
    B(kWin0 + 3) = 2;
    B(kWin1) = 1;
    B(kWin1 + 1) = 1;
    B(kWin1 + 2) = 1;
    B(kWin1 + 3) = 2;
    SetW(kWin1 + 4, 0x14);
    B(kWin2) = 1;
    B(kWin2 + 1) = 1;
    B(kWin2 + 2) = 2;
    B(kWin2 + 3) = 2;
    SetW(kWin2 + 4, 0x70);
    B(kWin2 + 0xA) = 8;
    B(kWin2 + 0xB) = 0xFF;
    B(kWin3) = 1;
    B(kWin3 + 1) = 1;
    B(kWin3 + 2) = 3;
    B(kWin3 + 3) = 2;
    SetW(kWin3 + 4, 0xFF88);
    SetW(kWin3 + 6, 0x3F);
    B(kWin3 + 0xA) = 0xFF;
    SetL(kWin3 + 0x20, kOwned);
}

// original 0x57EAC0: window 0's first byte 0, windows 1..3's +3 = 1.
extern "C" void __cdecl Shisu_CloseWindows(void) {
    B(kWin0) = 0;
    B(kWin1 + 3) = 1;
    B(kWin2 + 3) = 1;
    B(kWin3 + 3) = 1;
}

// original 0x57EAE0: the byte 0x904101 to a scale index - below 0x26: 9, 0x28:
// 0xA, 0x2D: 0xB, 0x32: 0xC, 0x3C: 0xD, 0x41: 0xE, 0x44: 0xF, else 0x10 (al;
// the rest of eax the caller's).
extern "C" unsigned char __cdecl Shisu_ScaleIndex(void) {
    const unsigned char v = B(kLevelByte);
    if (v < 0x26) return 9;
    if (v < 0x28) return 0xA;
    if (v < 0x2D) return 0xB;
    if (v < 0x32) return 0xC;
    if (v < 0x3C) return 0xD;
    if (v < 0x41) return 0xE;
    return v < 0x44 ? 0xF : 0x10;
}

namespace {
// (term >= ... ) the score's ramp: q = num / den toward zero; den - edge > 0
// gives q, else 200 - q; below 0, 0. den 0: 0.
std::int32_t Ramp(std::int32_t den, std::int32_t edge, std::int32_t num) {
    if (den == 0) return 0;
    const std::int32_t q = num / den;
    const std::int32_t v = den - edge > 0 ? q : 200 - q;
    return v < 0 ? 0 : v;
}
std::int32_t S(U v) { return static_cast<std::int32_t>(v); }
}  // namespace

// original 0x57EB20: the score. With L the scale index, g0..g3 the given
// counts and n the rounds: A = ramp(4 L, 5 (g1 - n), 500 (g1 - n)), B =
// ramp(g1, 4 g2, 400 g2), C = 25 n + 0x32 for n below 2 else 0x96 - 25 n
// (below 0, 0), D = ramp(g2 + L, g3 - n + g1, 100 (g3 - n + g1)); each of A,
// B, C, D at 0x9399EC.. and the score 0x9399FC = (16 D + 10 L A + (C B / 100)
// g1) / 100, every division toward zero, every product 32-bit.
extern "C" void __cdecl Shisu_Score(void) {
    const std::int32_t level = B(kLevel);
    const std::int32_t g1 = B(kGiven + 1);
    const std::int32_t rounds = B(kRounds);
    const std::int32_t d = g1 - rounds;
    const std::int32_t a = Ramp(4 * level, 5 * d, 500 * d);
    SetL(kScoreA, static_cast<U>(a));
    const std::int32_t g2 = B(kGiven + 2);
    const std::int32_t b = Ramp(g1, 4 * g2, 400 * g2);
    SetL(kScoreB, static_cast<U>(b));
    std::int32_t c = 2 - rounds > 0 ? 25 * rounds + 0x32 : 0x96 - 25 * rounds;
    if (c < 0) c = 0;
    SetL(kScoreC, static_cast<U>(c));
    const std::int32_t e = B(kGiven + 3) - rounds + g1;
    const std::int32_t dd = Ramp(g2 + level, e, 100 * e);
    SetL(kScoreD, static_cast<U>(dd));
    const U la = static_cast<U>(level) * L(kScoreA);
    const U cb = static_cast<U>(c) * L(kScoreB);
    const U part = static_cast<U>(S(cb) / 100) * static_cast<U>(g1);
    const U total = static_cast<U>(dd) * 16u + la * 10u + part;
    SetL(kScore, static_cast<U>(S(total) / 100));
}

// original 0x57ECC0: the two models from the file 0x628C88 - B's header the
// file, its quads the file's +4 dword; A's header the file + 8, its quads the
// dword at +0xC; each at the kind-2 sprite's x, z (Field_Kind2X / Z), state 0,
// scale 0x1000, +0x48 0, angles 0, colour 0x80 x 3; B's y the elevation there
// less 0x200, A's plus 0x200 (each << 16). The file pointer and the place are
// read again after the first elevation.
extern "C" void __cdecl Shisu_InitModels(void) {
    const U file = L(kModelFile);
    const long kz = Field_Kind2Z;
    SetL(kModelB + 0x54, file);
    SetL(kModelB + 0x50, L(file + 4));
    const long kx = Field_Kind2X;
    B(kBState) = 0;
    B(kModelB + 0x48) = 0;
    SetL(kBScale, 0x1000);
    SetL(kModelB + 0x64, 0);
    SetL(kModelB + 0x68, 0);
    SetL(kBAngle, 0);
    B(kBColour) = 0x80;
    B(kBColour + 1) = 0x80;
    B(kBColour + 2) = 0x80;
    SetL(kModelB + 0x34, static_cast<U>(kx));
    SetL(kModelB + 0x38, static_cast<U>(kz));
    const long hb = SH_CALL(AreaMap_Elevation)(kx, kz);
    const U file2 = L(kModelFile);
    SetL(kBY, (static_cast<U>(static_cast<std::int32_t>(static_cast<short>(hb))) - 0x200u) << 16);
    const long kz2 = Field_Kind2Z;
    SetL(kModelA + 0x54, file2 + 8);
    SetL(kModelA + 0x50, L(file2 + 8 + 4));
    const long kx2 = Field_Kind2X;
    B(kAState) = 0;
    B(kModelA + 0x48) = 0;
    SetL(kModelA + 0x40, 0x1000);
    SetL(kModelA + 0x64, 0);
    SetL(kModelA + 0x68, 0);
    SetL(kAAngle, 0);
    B(kAColour) = 0x80;
    B(kAColour + 1) = 0x80;
    B(kAColour + 2) = 0x80;
    SetL(kModelA + 0x34, static_cast<U>(kx2));
    SetL(kModelA + 0x38, static_cast<U>(kz2));
    const long ha = SH_CALL(AreaMap_Elevation)(kx2, kz2);
    SetL(kAY, (static_cast<U>(static_cast<std::int32_t>(static_cast<short>(ha))) + 0x200u) << 16);
}

// ===========================================================================
// The two models' states and their draw
// ===========================================================================

// original 0x57EDD0: jmp through Shisu_ModelBStates by model B's state
// 0x939961, unchecked (ours aborts past its 5).
extern "C" void __cdecl Shisu_ModelBDispatch(void) {
    Run("Shisu_ModelBDispatch (0x57EDD0)", Shisu_ModelBStates, Shisu_ModelBStates_count, B(kBState),
        "model B's state 0x939961");
}

// original 0x57EDE0: Shisu_ModelBStates[1] - model B's angle +0x6C up 0x20, then
// its draw (a tail jmp).
extern "C" void __cdecl Shisu_ModelBTurn(void) {
    SetL(kBAngle, L(kBAngle) + 0x20u);
    SH_CALL(Shisu_ModelBDraw)();
}

// original 0x57EDF0 (no list had it; the tail of four of model B's states):
// model B's scale from Shisu_ModelScales by g1 - rounds held to 1..0x14
// (signed bytes); its colour lifted by 3 g2 in red and green and lowered by 3
// g2 in blue (each byte's sum taken only below 0xFF / above 0, else 0xFF / 0),
// the model drawn, the three colour bytes put back.
extern "C" void __cdecl Shisu_ModelBDraw(void) {
    auto d = static_cast<signed char>(B(kGiven + 1) - B(kRounds));
    if (d < 1) d = 1;
    else if (d > 0x14) d = 0x14;
    const unsigned char kept_g = B(kBColour + 1);
    const unsigned char g2 = B(kGiven + 2);
    const auto scale = static_cast<U>(Shisu_ModelScales[d]);
    const unsigned char kept_b = B(kBColour + 2);
    SetL(kBScale, scale);
    const unsigned char kept_r = B(kBColour);
    const int three = g2 * 3;
    const auto up = static_cast<unsigned char>(g2 * 3);
    if (0xFF - kept_r > three) B(kBColour) = static_cast<unsigned char>(kept_r + up);
    else B(kBColour) = 0xFF;
    const unsigned char g = B(kBColour + 1);
    if (0xFF - g > three) B(kBColour + 1) = static_cast<unsigned char>(g + up);
    else B(kBColour + 1) = 0xFF;
    const unsigned char bl = B(kBColour + 2);
    if (bl > three) B(kBColour + 2) = static_cast<unsigned char>(bl - up);
    else B(kBColour + 2) = 0;
    SH_CALL(Shisu_DrawModel)(At(kModelB));
    B(kBColour) = kept_r;
    B(kBColour + 1) = kept_g;
    B(kBColour + 2) = kept_b;
}

// original 0x57EEF0 (PSX 0x801D2308): a model record's quads, drawn with the
// record as Sprite_Current (the caller's put back at the end) - Sprite_AddDrawRecords'
// set-up (Gte_PushMatrix, Sprite_ObjectMatrix, the scale when +0x48 is 0, the
// light direction, Camera_LoadMatrix) with its light matrix a LOCAL of which
// Light_ObjectDirection fills the first three shorts; then per quad of the
// count (the s8 at the header's +0, widened to a u16) a POLY_FT4 at the packet
// cursor: Gte_RotTransPers4 of the four vertices copied to Prim_VertexScratch,
// Gte_PrimDepths4_10, the screen offsets 0x5C4278 / 0x5C4274 added to each x /
// y, the record's colour (lit through Gte_NormalColor when the header's flags
// have 0x80), the page (Gpu_GetTPage(0, mode & 3, 0x2C0, 0x100), mode the
// flags when they have 0x40, else 2), the CLUT (the quad's s16 +0 << 4, row
// 0x1E3), semi-transparency by mode bit 6, and Gfx_CommitPrim(1, 0x48) only
// when 0x4941B0 finds the first three corners wound forward (ax > 0).
//
// The light matrix's other 26 bytes are the stack's (no instruction writes
// them) and go into Gte_Matrix2 through Gte_SetMatrix2; ours zeroes them
// (docs/rest_2b.md section 7, L1). A negative count draws some 65,000 quads,
// as the original would.
extern "C" void __cdecl Shisu_DrawModel(unsigned char* record) {
    unsigned char* const old = Sprite_Current;
    Sprite_Current = record;
    SH_CALL(Gte_PushMatrix)();
    const auto count = static_cast<std::uint16_t>(static_cast<std::int16_t>(
        static_cast<signed char>(Get<const unsigned char*>(Sprite_Current, 0x54)[0])));
    alignas(4) short matrix[16];
    SH_CALL(Sprite_ObjectMatrix)(matrix);
    SH_CALL(Gte_SetRotMatrix)(reinterpret_cast<const unsigned long*>(matrix));
    SH_CALL(Gte_SetTransMatrix)(reinterpret_cast<const unsigned long*>(matrix));
    if (Sprite_Current[0x48] == 0) {
        const long s = Get<long>(Sprite_Current, 0x40);
        const long scale[3] = {s, s, s};
        SH_CALL(Gte_ScaleMatrix)(matrix, scale);
    }
    alignas(4) short view[16];
    std::memcpy(view, matrix, sizeof view);
    alignas(4) short light[16] = {};
    SH_CALL(Light_ObjectDirection)(light, reinterpret_cast<const long*>(Sprite_Current + 0x64));
    SH_CALL(Camera_LoadMatrix)(view);
    SH_CALL(Gte_SetMatrix2)(reinterpret_cast<const unsigned long*>(light));
    const unsigned char flags = Get<const unsigned char*>(Sprite_Current, 0x54)[3];
    const unsigned mode = flags & 0x40 ? flags : 2u;
    const unsigned char* quad = Get<const unsigned char*>(Sprite_Current, 0x50);
    const unsigned tpage = SH_CALL(Gpu_GetTPage)(0, mode & 3, 0x2C0, 0x100);
    if (count != 0) {
        const unsigned semi = (mode >> 6) & 1;
        short* const v = Prim_VertexScratch;
        for (unsigned n = count; n != 0; --n) {
            unsigned char* const p = Gfx_PacketNext;
            SH_CALL(Gpu_SetPolyFT4)(p);
            for (unsigned i = 0; i < 12; ++i) v[i / 3 * 4 + i % 3] = Get<short>(quad, 2 + 2 * i);
            const long normal[3] = {Get<short>(quad, 0x1A), Get<short>(quad, 0x1C), Get<short>(quad, 0x1E)};
            for (unsigned k = 0; k < 4; ++k) Put<std::uint16_t>(p, 0x14 + 0x10 * k, Get<std::uint16_t>(quad, 0x20 + 2 * k));
            long depth;
            SH_CALL(Gte_RotTransPers4)(v, v + 4, v + 8, v + 12, reinterpret_cast<float*>(p + 8),
                                       reinterpret_cast<float*>(p + 0x18), reinterpret_cast<float*>(p + 0x28),
                                       reinterpret_cast<float*>(p + 0x38), &depth);
            SH_CALL(Gte_PrimDepths4_10)(p);
            for (unsigned k = 0; k < 4; ++k) {
                Put<float>(p, 8 + 0x10 * k, Get<float>(p, 8 + 0x10 * k) + Get<float>(At(kOffsetX), 0));
                Put<float>(p, 0xC + 0x10 * k, Get<float>(p, 0xC + 0x10 * k) + Get<float>(At(kOffsetY), 0));
            }
            p[4] = Sprite_Current[0x5D];
            p[5] = Sprite_Current[0x5E];
            p[6] = Sprite_Current[0x5F];
            if (Get<const unsigned char*>(Sprite_Current, 0x54)[3] & 0x80) {
                SH_CALL(Gte_VectorNormalS)(normal, Prim_VertexScratch);
                const unsigned char* const s = Sprite_Current;
                const unsigned char colour[4] = {s[0x5D], s[0x5E], s[0x5F], p[7]};
                SH_CALL(Gte_NormalColor)(Prim_VertexScratch, colour, p + 4);
            }
            Put<std::uint16_t>(p, 0x26, static_cast<std::uint16_t>(tpage));
            const int clut_x = static_cast<int>(static_cast<unsigned>(static_cast<int>(Get<short>(quad, 0))) << 4);
            const unsigned clut = SH_CALL(Gpu_GetClut)(clut_x, 0x1E3);
            Put<std::uint16_t>(p, 0x16, static_cast<std::uint16_t>(clut));
            SH_CALL(Gpu_SetSemiTrans)(p, semi);
            using Winding = int (__cdecl*)(const float*, const float*, const float*);
            const int wound = SH_AT(Winding, rest_2b::at::kWinding)(
                reinterpret_cast<const float*>(p + 8), reinterpret_cast<const float*>(p + 0x18),
                reinterpret_cast<const float*>(p + 0x28));
            if (static_cast<short>(wound) > 0) SH_CALL(Gfx_CommitPrim)(1, 0x48);
            quad += 0x28;
        }
    }
    SH_CALL(Gte_PopMatrix)();
    Sprite_Current = old;
}

// original 0x57F270: Shisu_ModelBStates[2] - model B's angle not a whole turn
// (low 12 bits set): (angle + 0x40) & 0xFC0 and its done flag 0; a whole turn:
// done 1. Then its draw.
extern "C" void __cdecl Shisu_ModelBSquare(void) {
    const U angle = L(kBAngle);
    if (angle & 0xFFF) {
        B(kBDone) = 0;
        SetL(kBAngle, (angle + 0x40) & 0xFC0);
    } else {
        B(kBDone) = 1;
    }
    SH_CALL(Shisu_ModelBDraw)();
}

// original 0x57F2A0: Shisu_ModelBStates[3] - model B's y up 0x200000; above
// the elevation at the kind-2 sprite's place (<< 16, signed): on it (asked
// again) and done 1; else done 0. Then its draw.
extern "C" void __cdecl Shisu_ModelBDrop(void) {
    const long kz = Field_Kind2Z;
    const long kx = Field_Kind2X;
    SetL(kBY, L(kBY) + 0x200000u);
    const long h = SH_CALL(AreaMap_Elevation)(kx, kz);
    const auto ground = static_cast<std::int32_t>(static_cast<U>(static_cast<std::int32_t>(static_cast<short>(h))) << 16);
    if (static_cast<std::int32_t>(L(kBY)) > ground) {
        const long kz2 = Field_Kind2Z;
        const long kx2 = Field_Kind2X;
        const long h2 = SH_CALL(AreaMap_Elevation)(kx2, kz2);
        B(kBDone) = 1;
        SetL(kBY, static_cast<U>(static_cast<std::int32_t>(static_cast<short>(h2))) << 16);
    } else {
        B(kBDone) = 0;
    }
    SH_CALL(Shisu_ModelBDraw)();
}

// original 0x57F310: Shisu_ModelBStates[4] - model B's draw (a tail jmp).
extern "C" void __cdecl Shisu_ModelBStill(void) { SH_CALL(Shisu_ModelBDraw)(); }

// original 0x57F320: jmp through Shisu_ModelAStates by model A's state
// 0x9398E1, unchecked (ours aborts past its 6).
extern "C" void __cdecl Shisu_ModelADispatch(void) {
    Run("Shisu_ModelADispatch (0x57F320)", Shisu_ModelAStates, Shisu_ModelAStates_count, B(kAState),
        "model A's state 0x9398E1");
}

// original 0x57F330: Shisu_ModelAStates[1] - model A's angle +0x6C down 0x20,
// then its draw (a tail jmp to R2C's 0x57F340).
extern "C" void __cdecl Shisu_ModelATurn(void) {
    SetL(kAAngle, L(kAAngle) - 0x20u);
    SH_AT(Handler, rest_2b::at::kModelADraw)();
}

void Rest2B_Inject() {
    if (bof3::WantsShadow("rest_2b")) rest_2b::SelfTest();
    BOF3_INJECT(Field_TriggerCounterF0);
    BOF3_INJECT(Effect_Spawn);
    BOF3_INJECT(Effect_SpawnAt);
    BOF3_INJECT(Menu_DrawGreyHLine);
    BOF3_INJECT(Menu_DrawOutlineNotched);
    BOF3_INJECT(Shisu_ModeDispatch);
    BOF3_INJECT(Shisu_Begin);
    BOF3_INJECT(Shisu_OpenDispatch);
    BOF3_INJECT(Shisu_OpenFade);
    BOF3_INJECT(Shisu_OpenWait);
    BOF3_INJECT(Shisu_CloseDispatch);
    BOF3_INJECT(Shisu_CloseFade);
    BOF3_INJECT(Shisu_CloseWait);
    BOF3_INJECT(Shisu_Result);
    BOF3_INJECT(Shisu_PickDispatch);
    BOF3_INJECT(Shisu_PickSide);
    BOF3_INJECT(Shisu_PickCounts);
    BOF3_INJECT(Shisu_PickShow);
    BOF3_INJECT(Shisu_ShowStart);
    BOF3_INJECT(Shisu_ShowDrop);
    BOF3_INJECT(Shisu_ShowLanded);
    BOF3_INJECT(Shisu_ShowFlash);
    BOF3_INJECT(Shisu_ShowFade);
    BOF3_INJECT(Shisu_CountItems);
    BOF3_INJECT(Shisu_SetupWindows);
    BOF3_INJECT(Shisu_CloseWindows);
    BOF3_INJECT(Shisu_ScaleIndex);
    BOF3_INJECT(Shisu_Score);
    BOF3_INJECT(Shisu_InitModels);
    BOF3_INJECT(Shisu_ModelBDispatch);
    BOF3_INJECT(Shisu_ModelBTurn);
    BOF3_INJECT(Shisu_ModelBDraw);
    BOF3_INJECT(Shisu_DrawModel);
    BOF3_INJECT(Shisu_ModelBSquare);
    BOF3_INJECT(Shisu_ModelBDrop);
    BOF3_INJECT(Shisu_ModelBStill);
    BOF3_INJECT(Shisu_ModelADispatch);
    BOF3_INJECT(Shisu_ModelATurn);
}

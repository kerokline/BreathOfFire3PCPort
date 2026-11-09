// Top-level modes, the system choice and the field core: the look-around
// mode's two camera steps (0x496250, 0x496A00, 0x496AD0); the shop mode and
// its three steps (0x496290, 0x4962A0, 0x517300, 0x496390); the menu's
// transition wait and frame (0x496830, 0x5172F0); the message box's system
// choice (0x498A30); the 8 px UI string draw (0x516E70); the field's loading
// frame (0x517290); the field core's state 2 and its fade steps (0x525370,
// 0x5258B0, 0x5258D0, 0x525920); scenario chapter 1's frame, area entry and
// step hook (0x539AD0, 0x539B20, 0x53D830); and the window cursor draw
// (0x5960D0). docs/mode_states.md.
//
// Every call goes through mode_states::g (mode_states_callees.h), so that the
// start-up fuzz can stand recorders in for them - for ours and for the
// originals' copies alike. Everything here is a faithful replacement; the
// one index the original leaves unbounded into its own stack (the system
// choice, docs/known-defects.md D22) aborts instead where no original frame
// is left to reproduce (section 2 of the doc).
#include "game/mode_states.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/labels.h"
#include "game/mode_states_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace mode_states {

using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

namespace {
template <typename T> T Fn(std::uint32_t address) { return reinterpret_cast<T>(static_cast<std::uintptr_t>(address)); }
}  // namespace

const Callees kOriginals = {
    {MsgBox_SysChoice80, MsgBox_SysChoice81, MsgBox_SysChoice82, MsgBox_SysChoice83, MsgBox_SysChoice84,
     MsgBox_SysChoice85, MsgBox_SysChoice86, MsgBox_SysChoice87, MsgBox_SysChoice88, MsgBox_SysChoice89,
     MsgBox_SysChoice8A, MsgBox_SysChoice8B, MsgBox_SysChoice8C, MsgBox_SysChoice8D, MsgBox_SysChoice8E,
     MsgBox_SysChoice8F},
    {0x4981F1, 0x4983F1},
    Look_Return, Field_LoadingFrame, Menu_Frame,
    Field_Frame, LoadDatFile, File_LoadDone, Task_Sleep, Gfx_ClutStripCopyRow, Sprite_FaceDirection,
    Snd_LoadBankFile, Fn<void (__cdecl*)()>(::bof3::Code(kAfterShop.v)), Text_EmitGlyph,
    AreaMap_Frame, Party_ExtraScreens, Party_UpdateScreens, Field_ObjectsScreen, Effect_RunObjects,
    MoveScript_TintFrame, Field_DrawFrame, Field_ModeDispatch, Fn<void (__cdecl*)()>(::bof3::Code(kMenuStates.v)),
    Field_RunTaskRecords, Fn<void (__cdecl*)()>(::bof3::Code(kShopStates.v)),
    Sprite_ShadeFadeBegin, Sprite_ShadeFadeStep,
    Flags_Test, Flags_Set, ScriptFlags_Set40, Music_LoadFile, Music_FadeOutStop, Sound_PlayEffect,
    Effect_FindFree, Scenario_CallA, AreaMap_SetupEntries, MoveCmd_TestFB, Field_ChangeArea,
    Menu_DrawHand,
};
Callees g = kOriginals;

}  // namespace mode_states

using namespace mode_states;

namespace {

std::uint32_t Address(const void* p) { return Low32(p); }

// A table of code pointers in the exe's data, read as the original reads it:
// afresh, and indexed without a bound.
mode_states::Handler Entry(std::uint32_t table, int index) {
    return reinterpret_cast<mode_states::Handler>(static_cast<std::uintptr_t>(
        static_cast<std::uint32_t>(mode_states::Long(mode_states::At(table + 4u * static_cast<std::uint32_t>(index))))));
}

// A byte argument read from its stack slot: the callers push whole
// registers or constants, and the original reads the byte.
unsigned char SlotByte(const unsigned char& argument) { return *reinterpret_cast<const volatile unsigned char*>(&argument); }

// The camera words: yaw Camera_Angles[0] and the pitch word Camera_Angles[2]
// (Cond_AngleFB's low half), and their copies Light_AnglesCopy[0] / [2].
constexpr std::uint32_t kYaw = 0x929EC8, kPitch = 0x929ECC, kYawCopy = 0x7E0680, kPitchCopy = 0x7E0684;
short Signed16(std::uint32_t address) { return static_cast<short>(mode_states::Word(mode_states::At(address))); }
void AddWordModeStates(std::uint32_t address, int delta) { mode_states::SetWord(mode_states::At(address), mode_states::Word(mode_states::At(address)) + static_cast<unsigned>(delta)); }

// The same with a loading frame before each sleep (the mode handlers').
void WaitLoadFramed() {
    while (mode_states::g.load_done() == 0) {
        mode_states::g.loading_frame();
        mode_states::g.sleep(1);
    }
}

}  // namespace

// ===========================================================================
// The look-around mode (Game_Mode 6) and its camera
// ===========================================================================

// original 0x496A00 (no PSX twin paired; PSX 0x80199F1C): mode 6's step 0, and mode 11's
// step 1. While none of the buttons of the map word 0x903586 is held in
// Input_Held, the look ends: the pitch word above 0x200 leaves the turn byte
// 0x66C7D9 0, below it sets it 1 (equal leaves it), and the step moves on.
// Otherwise bits 8..15 of the held word turn the camera 22 a frame, each
// word and its Light_AnglesCopy word together: 0x40 yaw down to 0xFD56,
// 0x10 yaw up to 0xFE39, 0x20 pitch up to 0x355, 0x80 pitch down to 0xAA.
//
// As the original has it: every limit is a signed 16-bit compare of the word
// read afresh, and the step past a limit is not clamped (the word may end up
// to 21 past it); the held word is read once.
extern "C" void __cdecl Look_PadControl(void) {
    const unsigned held = Input_Held;
    if ((mode_states::Word(mode_states::At(::bof3::Img(Gfx_UnpackScratch_addr) + 0x1f006)) & held & 0xFFFF) == 0) {
        const short pitch = Signed16(::bof3::Img(kPitch));
        if (pitch > 0x200) {
            ModeStates_LookTurn = 0;
        } else if (pitch < 0x200) {
            ModeStates_LookTurn = 1;
        }
        Game_Step = static_cast<unsigned short>(Game_Step + 1);
        return;
    }
    const unsigned buttons = (held >> 8) & 0xFF;
    if ((buttons & 0x40) != 0 && Camera_Angles[0] > static_cast<short>(0xFD56)) {
        AddWordModeStates(::bof3::Img(kYaw), -0x16);
        AddWordModeStates(::bof3::Img(kYawCopy), -0x16);
    }
    if ((buttons & 0x10) != 0 && Camera_Angles[0] < static_cast<short>(0xFE39)) {
        AddWordModeStates(::bof3::Img(kYaw), 0x16);
        AddWordModeStates(::bof3::Img(kYawCopy), 0x16);
    }
    if ((buttons & 0x20) != 0 && Signed16(::bof3::Img(kPitch)) < 0x355) {
        AddWordModeStates(::bof3::Img(kPitch), 0x16);
        AddWordModeStates(::bof3::Img(kPitchCopy), 0x16);
    }
    if ((buttons & 0x80) != 0 && Signed16(::bof3::Img(kPitch)) > 0xAA) {
        AddWordModeStates(::bof3::Img(kPitch), -0x16);
        AddWordModeStates(::bof3::Img(kPitchCopy), -0x16);
    }
}

// original 0x496AD0 (no PSX twin paired; PSX 0x8019A0D0): one frame of the camera's way back
// from a look - yaw 22 down to 0xFD56 (below it, snapped there), and the
// pitch word 22 towards 0x200, down with the turn byte 0x66C7D9 at 0 and up
// otherwise, snapped to 0x200 once there or past it; each word with its
// Light_AnglesCopy word. Nothing moves once the pitch is 0x200.
//
// As the original has it: the yaw copy is stepped by 22 or set to 0xFD56 with
// the yaw, never compared; the pitch is read once and its copy stepped where
// it lies.
extern "C" void __cdecl Look_Return(void) {
    if (Camera_Angles[0] > static_cast<short>(0xFD56)) {
        AddWordModeStates(::bof3::Img(kYaw), -0x16);
        AddWordModeStates(::bof3::Img(kYawCopy), -0x16);
    } else {
        Camera_Angles[0] = 0xFD56;
        Light_AnglesCopy[0] = 0xFD56;
    }
    const short pitch = Signed16(::bof3::Img(kPitch));
    if (pitch == 0x200) return;
    if (ModeStates_LookTurn == 0) {
        if (pitch <= 0x200) {
            mode_states::SetWord(mode_states::At(::bof3::Img(Cond_AngleFB_addr)), 0x200);
            Light_AnglesCopy[2] = 0x200;
            return;
        }
        AddWordModeStates(::bof3::Img(kPitchCopy), -0x16);
        mode_states::SetWord(mode_states::At(::bof3::Img(Cond_AngleFB_addr)), static_cast<unsigned>(pitch - 0x16));
        return;
    }
    if (pitch >= 0x200) {
        mode_states::SetWord(mode_states::At(::bof3::Img(Cond_AngleFB_addr)), 0x200);
        Light_AnglesCopy[2] = 0x200;
        return;
    }
    AddWordModeStates(::bof3::Img(kPitchCopy), 0x16);
    mode_states::SetWord(mode_states::At(::bof3::Img(Cond_AngleFB_addr)), static_cast<unsigned>(pitch + 0x16));
}

// original 0x496250 (no PSX twin paired): mode 6's step 1 - Look_Return, and
// with the camera back at yaw 0xFD56 and pitch 0x200, mode 2 (the field)
// with Field_Request and Game_Step 0.
extern "C" void __cdecl GameMode_LookEnd(void) {
    mode_states::g.look_return();
    if (static_cast<std::uint16_t>(Camera_Angles[0]) == 0xFD56 && mode_states::Word(mode_states::At(::bof3::Img(Cond_AngleFB_addr))) == 0x200) {
        Game_Mode = 2;
        Field_Request = 0;
        Game_Step = 0;
    }
}

// ===========================================================================
// The shop mode (Game_Mode 7)
// ===========================================================================

// original 0x496290 (no PSX twin paired): Game_Mode 7 (GameMode_Handlers
// entry 7) - a tail jump through GameMode_ShopSteps 0x656AAC on Game_Step:
// 0 Shop_Open, 1 Shop_Frame, 2 Shop_Close. Unchecked: step 3 would run mode
// 8's first step, GameMode8_Enter 0x496440, the word after the table.
extern "C" void __cdecl GameMode_Shop(void) { Entry(::bof3::Img(GameMode_ShopSteps_addr), Game_Step)(); }

// original 0x4962A0 (no PSX twin paired; PSX 0x80199270): the shop's step 0. The object the
// shop was opened on is the s8 0x929F0C (Field_ObjectIdle sets it with
// Game_Mode 7). When it is an object and its dword +0x18 is 9, there is no
// shop: back to mode 2 at once - the object turned to face +0x85 unless its
// +7 bit 3 (and then its +0x80 bit 3 set), its +0x80 bit 5 cleared,
// Field_ScriptFlags bit 8 cleared, a field frame. Otherwise the shop's file
// DAT 0x31B is loaded with a loading frame and a sleep each frame of the
// wait, CLUT strip rows 1 and 2 kept, a field frame, and step 1.
//
// As the original has it: Sprite_Current is set to the object before the
// +0x18 test either way; the facing is read through Sprite_Current, the rest
// through the object; Game_Step goes up after the field frame, read afresh.
extern "C" void __cdecl Shop_Open(void) {
    const signed char index = static_cast<signed char>(ModeStates_ShopObject);
    if (index >= 0) {
        unsigned char* const o = rv::Bytes(Sprite_Objects) + static_cast<std::uint32_t>(static_cast<int>(index)) * 0xA4u;
        Sprite_Current = o;
        if (mode_states::Long(o + 0x18) == 9) {
            Game_Mode = 2;
            Game_Step = 0;
            Field_Request = 0;
            if ((o[7] & 8) == 0) {
                o[8] = o[0x85];
                mode_states::g.face_direction(Sprite_Current[8]);
                o[0x80] = static_cast<unsigned char>(o[0x80] | 8);
            }
            o[0x80] = static_cast<unsigned char>(o[0x80] & ~0x20u);
            Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags & 0xFEFF);
            mode_states::g.field_frame();
            return;
        }
    }
    mode_states::g.load_dat(0x31B);
    WaitLoadFramed();
    mode_states::g.clut_copy_row(1);
    mode_states::g.clut_copy_row(2);
    Gfx_ClutStripDirty = 1;
    mode_states::g.field_frame();
    Game_Step = static_cast<unsigned short>(Game_Step + 1);
}

// original 0x496390 (no PSX twin paired; PSX 0x801993D8): the shop's step 2. The party
// combination byte 0x90412C gets bit 7, and the sound bank 0x2C2 + its low
// seven bits is loaded (the area's own, which the shop's file replaced) with
// the framed wait; with Field_InputFlags bit 4, DAT 0x12A too, with its wait,
// and CommuSim_RollOffers 0x4560D0 (R4A's); then mode 2 with Game_Step and Field_Request 0
// and a tail jump to Field_Frame.
extern "C" __attribute__((disable_tail_calls)) void __cdecl Shop_Close(void) {
    const unsigned char combo = static_cast<unsigned char>(AreaW0b_PartySet | 0x80);
    AreaW0b_PartySet = combo;
    mode_states::g.bank_file((combo & 0x7Fu) + 0x2C2);
    WaitLoadFramed();
    if ((Field_InputFlags & 0x10) != 0) {
        mode_states::g.load_dat(0x12A);
        WaitLoadFramed();
        mode_states::g.after_shop();
    }
    Game_Mode = 2;
    Game_Step = 0;
    Field_Request = 0;
    mode_states::g.field_frame();
}

// ===========================================================================
// The field's per-frame entries
// ===========================================================================

// original 0x496830 (no PSX twin paired; PSX 0x80199C38): Field_WaitTransition with the menu's
// frame - called by mode 3's steps 0 and 2 after Transition_Start(3) / (2).
// Loop: a frame's sleep; with Transition_Busy 0 and `run` 0, one more
// sleep and return; else Menu_Frame, and return once the word is 0.
//
// As the original has it: `run` is the argument's byte, read once, and
// tested every pass; the wait word is read afresh at both tests.
extern "C" __attribute__((disable_tail_calls)) void __cdecl Menu_WaitTransition(unsigned char run_argument) {
    const unsigned char run = SlotByte(run_argument);
    for (;;) {
        mode_states::g.sleep(1);
        if (Transition_Busy == 0 && run == 0) {
            mode_states::g.sleep(1);
            return;
        }
        mode_states::g.menu_frame();
        if (Transition_Busy == 0) return;
    }
}

// ===========================================================================
// The message box's system choice
// ===========================================================================

// original 0x498A30 (PSX 0x80152DB4): the handler of a message-box choice id
// 0x7DEE64 of 0x80 or more, called by MsgBox_ChoiceCommit and
// MsgBox_MenuCommit. The original stores sixteen handlers on its stack
// (MsgBox_SysChoice80..8F, 0x498AD0..0x498D00) and calls [esp + 4 * id -
// 0x200] unbounded (D22).
//
// Ids 0x80..0x8F call their handler. Id 0x90 calls the word above the table:
// the original's own return address, so the caller's tail runs, returns here
// and runs again - reproduced exactly when the caller is one of the two
// commits' original bodies (their tails 0x4981F1 / 0x4983F1 touch no stack
// but their own return), by calling the return address. Every other id -
// 0x91 and up call further words of the caller's frame, below 0x80 words
// below the table - and 0x90 from any other caller (both commits are ours,
// item_use.cpp, so the frame the original would call into is compiled code
// of ours) abort (CLAUDE.md rule 4): there is no original frame left to
// reproduce. docs/mode_states.md section 2.
extern "C" __attribute__((noinline, disable_tail_calls)) void __cdecl MsgBox_SystemChoice(void) {
    const unsigned id = ItemUse_ChoiceId;
    if (id - 0x80u < 16u) {
        mode_states::g.choice[id - 0x80u]();
        return;
    }
    if (id == 0x90) {
        const std::uint32_t resume = Address(__builtin_return_address(0));
        if (resume == mode_states::g.tails[0] || resume == mode_states::g.tails[1]) {
            reinterpret_cast<mode_states::Handler>(static_cast<std::uintptr_t>(resume))();
            return;
        }
        bof3::Fatal("MsgBox_SystemChoice: choice id 0x90 calls its caller's resume point 0x%X, not a commit's tail",
                    static_cast<unsigned>(resume));
    }
    bof3::Fatal("MsgBox_SystemChoice: choice id 0x%X, outside the sixteen-entry table", id);
}

// ===========================================================================
// The 8 px UI string draw
// ===========================================================================

// ===========================================================================
// The field core: state 2 and its fade
// ===========================================================================

// ===========================================================================
// Scenario chapter 1
// ===========================================================================

// ===========================================================================
// The window cursor
// ===========================================================================

// original 0x5960D0 (no PSX twin paired; PSX 0x8015ABC8): the window kind 2's cursor, a tail
// jump from its state 2 (0x595C44) - Menu_DrawHand at the window 0x905B84's
// x + 4 and at its y plus the list set's first row and the row stride times
// the cursor's row: record 0x7DEE65 of the 6-byte table 0x66AE2C (+2 the
// first row, +4 the stride), the s8 row 0x7DEE67; the window's +4 / +6 are
// 12.4 words.
//
// As the original has it: the window words are shifted as signed 16 bits
// and the product and sums are 16-bit; the upper halves of both coordinates
// are the registers' leftovers in the original, and Menu_DrawHand reads the
// low words only, so ours passes them zero-extended.
extern "C" __attribute__((disable_tail_calls)) void __cdecl Window_DrawCursor(void) {
    const unsigned set = MenuWindows_ListSet;
    const unsigned char* const rows = mode_states::At(::bof3::Img(at::kCursorRows) + set * 6);
    const short row = AreaW0c_ChoiceAnswer;
    const unsigned char* const w = reinterpret_cast<const unsigned char*>(BattleWinStates_Current);
    const unsigned y = (mode_states::Word(rows + 4) * static_cast<unsigned>(row) + static_cast<unsigned>(static_cast<short>(mode_states::Word(w + 6)) >> 4) +
                        mode_states::Word(rows + 2)) & 0xFFFF;
    const unsigned x = (static_cast<unsigned>(static_cast<short>(mode_states::Word(w + 4)) >> 4) + 4) & 0xFFFF;
    mode_states::g.draw_hand(static_cast<int>(x), static_cast<int>(y), 0);
}

// ===========================================================================

void ModeStates_Inject() {
    if (bof3::WantsShadow("mode_states")) mode_states::SelfTest();
    BOF3_INJECT(GameMode_LookEnd);
    BOF3_INJECT(GameMode_Shop);
    BOF3_INJECT(Shop_Open);
    BOF3_INJECT(Shop_Close);
    BOF3_INJECT(Menu_WaitTransition);
    BOF3_INJECT(Look_PadControl);
    BOF3_INJECT(Look_Return);
    BOF3_INJECT(MsgBox_SystemChoice);
    BOF3_INJECT(Window_DrawCursor);
}

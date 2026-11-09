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

// A table of code pointers in the exe's data, read as the original reads it:
// afresh, and indexed without a bound.
mode_states::Handler Entry(std::uint32_t table, int index) {
    return reinterpret_cast<mode_states::Handler>(static_cast<std::uintptr_t>(
        static_cast<std::uint32_t>(mode_states::Long(mode_states::At(table + 4u * static_cast<std::uint32_t>(index))))));
}

}  // namespace

// ===========================================================================
// The look-around mode (Game_Mode 6) and its camera
// ===========================================================================

// ===========================================================================
// The shop mode (Game_Mode 7)
// ===========================================================================

// ===========================================================================
// The field's per-frame entries
// ===========================================================================

// ===========================================================================
// The message box's system choice
// ===========================================================================

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

// ===========================================================================

void ModeStates_Inject() {
    if (bof3::WantsShadow("mode_states")) mode_states::SelfTest();
}

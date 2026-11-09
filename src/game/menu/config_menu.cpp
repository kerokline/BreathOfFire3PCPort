// Moved here by tools/move_funcs.py from rest_2f.cpp (docs/source-layout-round.md).
#include "game/menu/config_menu.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/list_title.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_2f.h"
#include "game/rest_2f_callees.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

using U = std::uint32_t;

using move_script::SetWord;

using Handler = void (__cdecl*)();

using rest_2f::kConfigMachine;

// The menu block.
// kMode: now BattleE1_Mode (symbols.toml). FieldMenu_States' index
// kStep: now FieldS_State (symbols.toml). the screen's step
// kSub: now FieldS_Step (symbols.toml). its sub-step
// kByte03: now Rest2d_MenuSub (symbols.toml).
constexpr ::bof3::image::Va kTimer{0x929F04};       // a countdown of frames

// kStyle: now Config_WindowColour (symbols.toml). the window colour
// kBackdrop: now Config_Background (symbols.toml). the Config "Background" byte
// kGameStep: now Game_Step (symbols.toml). Game_Step (u16)
// kPressed: now Input_Pressed (symbols.toml). Input_Pressed (u16)
// kConfirm: now Field_ConfirmButtons (symbols.toml). Field_ConfirmButtons (u16)
// kCancel: now Field_CancelButtons (symbols.toml). Field_CancelButtons (u16)
// kInputFlags: now Field_InputFlags (symbols.toml). Field_InputFlags: bit 0, the members screen allowed
// kCurrent: now BattleWinStates_Current (symbols.toml). the window record Field_RunTaskRecords runs
// kWindows: now WindowRecords (symbols.toml). WindowRecords
constexpr U kWindowStride = 0x24;

unsigned char& BRest2f(U address) { return move_script::At(address)[0]; }

void SetWRest2f(U address, U v) { SetWord(move_script::At(address), v); }

U Key(const void* p) { return Low32(p); }

// Window record n's byte / word at `at`.
U Rec(unsigned n, unsigned at) { return ::bof3::Img(WindowRecords_addr) + kWindowStride * n + at; }

// jmp / call [table + 4 * byte]: the table's `entries` handlers, read in place
// (the fuzz swaps the cells for recorders); a Fatal past them, where the
// original jumps through the dword after.
void Run(const char* who, const unsigned long* table, unsigned entries, unsigned index, const char* byte) {
    if (index >= entries)
        bof3::Fatal("%s: %s is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/rest_2f.md section 7)",
                    who, byte, index, entries, (unsigned)Key(table));
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(table[index]))();
}

// --- the callees ------------------------------------------------------------------------
void Backdrop() { SH_CALL(Menu_DrawBackdrop)(Config_Background); }

unsigned char PartyCountRest2f() { return static_cast<unsigned char>(SH_CALL(Party_Count)(0)); }

// The party's window records 4.. : `body(i)` for i below Party_Count(0), the
// count asked again after each (the shape of six of the Tactics steps).
template <typename Body> void ForParty(Body body) {
    unsigned char i = 0;
    if (PartyCountRest2f() == 0) return;
    do {
        const unsigned char at = i;
        ++i;
        body(at);
    } while (i < PartyCountRest2f());
}

}  // namespace

// original 0x5902E0: FieldMenu_States[7] - jmp through ConfigMenu_Steps by
// 0x929F01, unchecked (ours aborts past its 3).
extern "C" void __cdecl ConfigMenu_Run(void) {
    Run("ConfigMenu_Run (0x5902E0)", ConfigMenu_Steps, ConfigMenu_Steps_count, FieldS_State, "0x929F01");
}

// original 0x5902F0: ConfigMenu_Steps[0] - the backdrop; the step on, the
// countdown 5, the sub-step 0, 0x929F03 = 2, the top bar's cursor kept at
// 0x6BDFD8 and cleared (Config uses it for its row).
extern "C" void __cdecl ConfigMenu_Open(void) {
    Backdrop();
    const unsigned char step = FieldS_State;
    const unsigned char cursor = static_cast<unsigned char>(MenuLists_Cursor);
    FieldS_State = static_cast<unsigned char>(step + 1);
    BRest2f(::bof3::Img(kTimer.v)) = 5;
    FieldS_Step = 0;
    Rest2d_MenuSub = 2;
    Rest2f_ConfigCursor = cursor;
    MenuLists_Cursor = 0;
}

// original 0x590340: ConfigMenu_Steps[1] - a tail jump to the Config screen's
// own machine 0x460CB0 (R4F's; docs/menu-screens.md section 1).
extern "C" void __cdecl ConfigMenu_Body(void) {
    SH_AT(Handler, kConfigMachine.v)();
}

// original 0x590350: ConfigMenu_Steps[2] - the backdrop; the party's records
// 4.. to state 4 at x -0x96; records 7..10 to state 2; the countdown 0, the
// field menu back to its top bar (state 1, step 0, sub-step 0) with its cursor
// from 0x6BDFD8.
extern "C" void __cdecl ConfigMenu_Close(void) {
    Backdrop();
    ForParty([](unsigned char i) {
        BRest2f(Rec(4u + i, 3)) = 4;
        SetWRest2f(Rec(4u + i, 4), 0xFF6A);
    });
    const unsigned char cursor = Rest2f_ConfigCursor;
    for (unsigned n = 7; n <= 10; ++n) BRest2f(Rec(n, 3)) = 2;
    BRest2f(::bof3::Img(kTimer.v)) = 0;
    BattleE1_Mode = 1;
    FieldS_State = 0;
    FieldS_Step = 0;
    MenuLists_Cursor = cursor;
}

void ConfigMenu_Inject() {
    BOF3_INJECT(ConfigMenu_Run);
    BOF3_INJECT(ConfigMenu_Open);
    BOF3_INJECT(ConfigMenu_Body);
    BOF3_INJECT(ConfigMenu_Close);
}

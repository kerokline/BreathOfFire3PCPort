// Group R2F of round fourteen (wave two): 49 functions at 0x58ED40..0x596F98 -
// the cut's 48 rows (analysis/round14_cut.tsv) and the start in their spans no
// list had, 0x596530 (Field_RunTaskRecords' handler 1), each read to its last
// instruction with capstone (2026-10-04) and fuzzed through the scenario
// harness's field mode (rest_2f_fuzz.cpp). docs/rest_2f.md has them one row
// each.
//
// The menu block (0x929F00..): +0 the field menu's state (FieldMenu_States),
// +1 the screen's step, +2 a sub-step, +3, +4 a frame countdown, +5 the top
// bar's cursor, +6 the member a screen shows, +8 / +9 the Tactics members
// screen's cursor (column, row), +0xA / +0xD the cell picked (0x7F none), +0x10
// "Tactics entered directly". The window records (WindowRecords 0x803160, 22 of
// 0x24): +0 in use, +1 the handler (Field_RunTaskRecords), +2 the kind, +3 the
// state, +4 / +6 x / y, +0xA.. the kind's own bytes, +0x10 a word, +0x20 a
// pointer; Field_RunTaskRecords puts the record it runs in 0x905B84.
//
// Every one is a faithful replacement. Where the original indexes a .data
// table of handlers by a byte it never bounds, ours aborts with a message past
// the table (round9 doc section 6); where it reads a .data table of values by
// such a byte, ours reads the same bytes in place. Every call goes through the
// harness (SH_CALL / SH_AT), so the start-up fuzz can stand recorders in for
// the callees; memory a stand-in could move is read after the call, as the
// originals read it.
#include "game/rest_2f.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_2f_callees.h"
#include "game/scenario_harness.h"
#include "game/list_title.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

using U = std::uint32_t;
// move_script::At: named in full below (source-layout B0, a clash on At)
using move_script::Long;
using move_script::SetWord;
using move_script::Word;
using Handler = void (__cdecl*)();
using rest_2f::kTitleCall;

// --- the cells ------------------------------------------------------------------------

// The menu block.
// kMode: now BattleE1_Mode (symbols.toml). FieldMenu_States' index
// kStep: now FieldS_State (symbols.toml). the screen's step
// kSub: now FieldS_Step (symbols.toml). its sub-step
// kByte03: now Rest2d_MenuSub (symbols.toml).
constexpr ::bof3::image::Va kTimer{0x929F04};       // a countdown of frames
// kTopCursor: now MenuLists_Cursor (symbols.toml). the top bar's cursor
// kShown: now BattleActorCopies_ItemActor (symbols.toml). the party slot a screen shows (s8)
// kCol: now FieldS_Column (symbols.toml). Tactics' members screen: the cursor's column (0 party, 1 reserve), s8
// kRow: now BattleE1_ModeByte9 (symbols.toml). its row, s8
// kHeldCol: now ShopStates_Clear0A (symbols.toml). the cell picked: 0x7F none
// kHeldRow: now FieldS_PickRow (symbols.toml).
// kDirect: now ScenaSc6_Load10 (symbols.toml). 1: Tactics was entered at its step 5 (the cancel leaves the menu)

// kRefused: now Rest2f_Refused (symbols.toml). the member a swap refused, 0xFF none (= kReserve + 3)
// kConfigSaved: now Rest2f_ConfigCursor (symbols.toml). the top bar's cursor while Config runs (= kReserve + 4)

// kCharacters: now CharacterRecords (symbols.toml). CharacterRecords: 0xA4 a member
constexpr U kCharacterStride = 0xA4;
constexpr ::bof3::image::Va kEffectState{0x66972C}; // Character_IndexById: the record of a member id
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
U WRest2f(U address) { return move_script::Word(move_script::At(address)); }
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

}  // namespace
// DIV-0059's site, read at inject (0: not read yet - the fuzz's runs, before
// the patch is read back, call Text_DrawAt as the original's copy does).
namespace rest_2f { U g_title_call = 0; }
// ===========================================================================
// The Ability screen's helpers (R2E's states call them)
// ===========================================================================

// ===========================================================================
// The Tactics screen (FieldMenu_States[5])
// ===========================================================================

namespace {
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

// ===========================================================================
// The field menu's states 7 (Config) and 8
// ===========================================================================

// ===========================================================================
// Three helpers other modules called by address
// ===========================================================================

// ===========================================================================
// Window kind 1 (Window_Kind1Open / Window_Kind1Frame's calls)
// ===========================================================================

// ===========================================================================
// Window-record handler 1 (Field_RunTaskRecords' table slot 1)
// ===========================================================================

// ===========================================================================
// Window-record handler 2 (Field_RunTaskRecords' table slot 2)
// ===========================================================================

void Rest2F_Inject() {
    if (bof3::WantsShadow("rest_2f")) rest_2f::SelfTest();
    // DIV-0059 survives the takeover: ours calls what the site calls, as
    // battle_draw.cpp holds it (BattleDraw_Inject ran first; operand-store.md)
    rest_2f::g_title_call = ListTitle_SiteDraw();
    bof3::Log("rest_2f     Win2_DrawItemList's title call (0x%X) reaches 0x%X%s", kTitleCall.v, rest_2f::g_title_call,
              rest_2f::g_title_call == bof3::addr::Text_DrawAt ? " (Text_DrawAt)" : " (DIV-0059's ListTitle_DrawAt)");
}

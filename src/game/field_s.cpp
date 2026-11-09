// Round twelve group FS (docs/takeover-queue-field-battle.md section 3,
// docs/field_s.md): the PSX SHOP.EMI's remaining states compiled into the exe
// and the equip screen's two choosers, taken with the scenario harness in
// field mode (scenario_harness.h, docs/scenario_harness.md section 7). Each
// read to its last instruction with capstone against bof3/BOF3.exe,
// 2026-09-29; entry and size (bytes of code, the padding after it left out):
//
//   FieldSave_Confirm      0x57FF80 0x85    ShopResist_Grant        0x583D30 0xAB
//   Rest_Begin             0x580310 0x6B    ShopResist_Farewell     0x583DE0 0x47
//   Rest_PlaceParty        0x580380 0x1D8   ShopResist_Notice       0x583E30 0x64
//   PartyForm_OpenStep     0x580A50 0xE     ShopResist_DrawBits     0x583EA0 0x166
//   PartyForm_Setup        0x580A60 0x116   ShopResist_DrawMembers  0x584010 0x103
//   PartyForm_FadeWait     0x580B80 0x21    ShopResist_Message      0x584120 0x5D
//   PartyForm_OpenWait     0x580BB0 0x23    SharedList_Begin        0x584190 0x19
//   PartyForm_SlideIn      0x580BE0 0x35    SharedList_Menu         0x5841B0 0x2A4
//   PartyForm_Choose       0x580C20 0x257   SharedList_MoveStep     0x584460 0xE
//   PartyForm_LeaveStep    0x580E80 0xE     SharedList_UseItem      0x584470 0x16D
//   PartyForm_LeaveFade    0x580E90 0x28    Menu_StepAfterTimer     0x5845E0 0x15
//   PartyForm_LeaveBlack   0x580EC0 0x1A    SharedList_PickMember   0x584600 0x1E3
//   PartyForm_Reload       0x580EE0 0x2CC   SharedList_PickSlot     0x5847F0 0x1B3
//   PartyForm_End          0x5811B0 0x29    SharedList_PickShared   0x5849B0 0x1FD
//   PartyForm_DrawSliding  0x5811E0 0x113   SharedList_SortStep     0x584BB0 0xE
//   PartyForm_DrawReserve  0x581300 0x285   SharedList_SortOpen     0x584BC0 0x50
//   PartyForm_Swap         0x581590 0x18F   SharedList_SortMenu     0x584C10 0x117
//   PartyForm_Draw         0x581720 0x18F   SharedList_Arrange      0x584D30 0x23A
//   ShopBrowse_InitWindows 0x5836E0 0x90    SharedList_Setup        0x584F90 0xFC
//   ShopBrowse_OpenDetail  0x583770 0x65    SharedList_DrawList     0x585090 0x469
//   ShopResist_Open        0x5837F0 0x82    SharedList_DrawMember   0x585500 0x2D3
//   ShopResist_PickMember  0x583880 0x196   SharedList_Sort         0x5857E0 0x10
//   ShopResist_PickBit     0x583A20 0x1BA   SharedList_Compact      0x5857F0 0x4E
//   ShopResist_Confirm     0x583BE0 0xF0    SharedList_SortCostDown 0x585840 0x76
//   ShopResist_Close       0x583CD0 0x5D    SharedList_SortCostUp   0x5858C0 0x76
//   Equip_ChooseSlot       0x58C7A0 0x338   SharedList_DrawItemCount 0x585940 0xB9
//   Equip_ChooseItem       0x58CAE0 0x251
//
// Faithful but for DIV-0027 in SharedList_UseItem under a Latin overlay
// (amended 2026-10-03). Every call out goes through the harness (SH_CALL /
// SH_AT), so the start-up fuzz can stand recorders in for the callees. Names
// are from what the code does; the screens' meaning in the game is the
// owner's to say (docs/field_s.md section 1).
//
// Registers the originals push whole - a byte loaded into al over whatever
// eax held, a coordinate computed from a register whose upper half is a
// callee's leftover - are passed here from the value the callee reads: each
// callee's reading is cited where it matters (docs/field_s.md section 3), and
// the fuzz lists those callees with masks of exactly what they read.
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/field_s.h"
#include "game/field_s_callees.h"
#include "game/lang_layout.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "game/yes_no_layout.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "game/menu_frame.h"
#include "game/record_views.h"
#include "game/records.h"
#include "input/sigma.h"

// Group HT of the button round: a face or shoulder bit the original tests by
// a constant is Japan's, asked through σ (docs/button-tests.md).
using bof3x::input::sigma::Sigma;

namespace {

namespace at = field_s::at;
using U = std::uint32_t;

unsigned char* At(U address) { return move_script::At(address); }
unsigned char& B(U address) { return *At(address); }
U W(U address) { return move_script::Word(At(address)); }
void SetW(U address, U v) { move_script::SetWord(At(address), v); }
U L(U address) { return static_cast<U>(move_script::Long(At(address))); }
int S8(U v) { return static_cast<signed char>(static_cast<unsigned char>(v)); }

// CharacterRecords' record n (stride 0xA4), unchecked.
unsigned char* Rec(U n) { return At(::bof3::Img(CharacterRecords_addr) + n * field_s::at::kRecordStride); }
U RecAddr(U n) { return ::bof3::Img(CharacterRecords_addr) + n * field_s::at::kRecordStride; }

// A member id's record index: Character_IndexById[id], unchecked.
U RecordOf(U id) { return Character_IndexById[id & 0xFF]; }

U Pressed() { return Input_Pressed; }
bool Confirmed(U pressed) { return (static_cast<std::uint32_t>(Field_ConfirmButtons) & pressed & 0xFFFF) != 0; }
bool Cancelled(U pressed) { return (static_cast<std::uint32_t>(Field_CancelButtons) & pressed & 0xFFFF) != 0; }

void Sound(U id) { SH_CALL(Sound_PlayEffect)(static_cast<unsigned short>(id)); }
U PartyCount() { return static_cast<U>(SH_CALL(Party_Count)(0)) & 0xFF; }
const unsigned char* Message(U id) { return SH_CALL(Msg_SystemPtr)(id); }
char* PrintBuf() { return reinterpret_cast<char*>(At(::bof3::Img(field_s::at::kPrintBuf))); }
const char* Format(U address) { return reinterpret_cast<const char*>(At(address)); }
const unsigned char* Text(U address) { return At(address); }
void DrawFont8(int x, int y, int colour) {
    SH_CALL(Text_DrawFont8)(x, y, colour, reinterpret_cast<const unsigned char*>(PrintBuf()));
}

// A .data table's word read in place (the fuzz swaps its entries for
// recorders). Outside the fuzz a word that is not code is past the table:
// the original jumps there; ours aborts (docs/field_s.md section 6).
U CodeAt(U table, U index, const char* who) {
    const U cell = table + 4 * index;
    const U entry = L(cell);
    // A code word: hosted a VA in BOF3.exe's .text, in bof3x.exe a function of ours
    // the bind table names (common/image_base.h IsCodeWord); not the trap, not data.
    if (!scenario_harness::g_active && !::bof3::IsCodeWord(entry))
        bof3::Fatal("%s: index %u reads 0x%X at 0x%X (VA 0x%X), not code - past its table (the original jumps there)", who,
                    (unsigned)index, (unsigned)entry, (unsigned)cell, (unsigned)::bof3::VaOf(cell));
    return entry;
}
using Handler = void (__cdecl*)();

// The swap helper 0x58BD50 (nobody's): the two bytes exchanged.
void SwapBytes(U a, U b) { SH_AT(void (__cdecl*)(unsigned char*, unsigned char*), field_s::at::kSwapBytes.v)(At(a), At(b)); }

}  // namespace

std::uint32_t field_s::FrameCallTarget() { return MenuFrame_Draw(); }

// ===========================================================================
// The field save's confirm (FieldSave_States[3]) and the rest sequence
// (Rest_States 0x663FB8, ShopMode_States[10]'s)
// ===========================================================================

// ===========================================================================
// The party formation (PartyForm_States 0x66409C, ShopMode_States[6]'s)
// ===========================================================================

// ===========================================================================
// ShopMode_States[9]'s two window set-ups (group FS's; the states are nobody's)
// ===========================================================================

// ===========================================================================
// The resistance shop (ShopResist_States 0x6641BC, ShopMode_States[5]'s,
// jumped through by 0x5837E0 on 0x929F01): a member (0x929F06) is given one
// of the eight bits of its record's +0x1D (0x929F0D) for ten zenny a level
// (+0xA) - the bits Char_RecalcStats turns into resistances
// (docs/char-stats.md section 2) - once: a record with any bit set is refused.
// ===========================================================================

// ===========================================================================
// The shared ability list (SharedList_States 0x664254, ShopMode_States[7]'s,
// jumped through by 0x584180 on 0x929F01): the 128 ability ids at 0x904574
// that AbilityList_Add fills when asked for the shared list, and the members'
// ten-byte lists at record +0x7E. State 1 chooses between moving abilities (1:
// SharedList_MoveSteps) and arranging the list (0: SharedList_SortSteps); the
// first wants key item 0xF or, without it, one of item 0x58 of category 0,
// used up (SharedList_UseItem).
// ===========================================================================

// original 0x584470: SharedList_MoveSteps[0], "use the item?". The help word
// 0x36; the hand on (at window 0's x - 36 answer (s8) + 0xE8, its y + 5, as
// words). The pressed dword's 0xA000 flips 0x929F0B with sound 0x101 (the
// dword read again). Confirm on yes: sounds 0x104 and 0x102, the pick 0,
// Inventory_Remove(0, 0x58, 1), windows 2 and 3 open on the joined record at
// the pick (read again), the step up one. Confirm on no, or cancel: sound
// 0x106 and the state down one.
//
// DIVERGENCE DIV-0027 (amended 2026-10-03, group YN): with the flag on (a
// Latin overlay, set after the self-test) the hand over the line
// ShopWin_TitleRun re-spaces (help 0x36 is one of its four yes / no ids).
unsigned char g_use_item_yes_no_hand = 0;

// ===========================================================================
// The equip screen's two choosers (the field menu's Equipment states 5 and 6,
// entries 12 and 13 of 0x667380 - its state table 0x66739C by 0x929F01,
// docs/menu-screens.md section 1)
// ===========================================================================

void FieldS_Inject() {
    if (bof3::WantsShadow("field_s")) field_s::SelfTest();
    if (Lang_Latin()) {   // DIVERGENCE DIV-0027 (amended 2026-10-03), DIV-0056
        static const std::uint8_t was = 0, is = 1;
        bof3::PatchBytes("ShopYesNoLayout",
                         Low32(&g_use_item_yes_no_hand), &was, &is, 1);
        bof3::Log("DIV-0027    SharedList_UseItem's yes / no hand over the re-spaced line");
    }
}

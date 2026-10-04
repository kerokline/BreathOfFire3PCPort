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
#include "game/rest_2f_callees.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
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
using rest_2f::kConfigMachine;
using rest_2f::kLeftOff170Bound;
using rest_2f::kSwapBytes;
using rest_2f::kTitleCall;

// --- the cells ------------------------------------------------------------------------

// The menu block.
constexpr U kMode = 0x929F00;        // FieldMenu_States' index
constexpr U kStep = 0x929F01;        // the screen's step
constexpr U kSub = 0x929F02;         // its sub-step
constexpr U kByte03 = 0x929F03;
constexpr U kTimer = 0x929F04;       // a countdown of frames
constexpr U kTopCursor = 0x929F05;   // the top bar's cursor
constexpr U kShown = 0x929F06;       // the party slot a screen shows (s8)
constexpr U kCol = 0x929F08;         // Tactics' members screen: the cursor's column (0 party, 1 reserve), s8
constexpr U kRow = 0x929F09;         // its row, s8
constexpr U kHeldCol = 0x929F0A;     // the cell picked: 0x7F none
constexpr U kHeldRow = 0x929F0D;
constexpr U kDirect = 0x929F10;      // 1: Tactics was entered at its step 5 (the cancel leaves the menu)

// Tactics' own cells (0x6BDFBC..0x6BDFD8).
constexpr U kGrid = 0x6BDFBC;        // 3 x 3 bytes: the formation's cells that may be picked
constexpr U kSetCol = 0x6BDFC5;      // the formation set: column 1..2, row (0x904060 = col + 2 row - 1)
constexpr U kPickCol = 0x6BDFC6;     // the cell picked: 0x7F none
constexpr U kSetRow = 0x6BDFC7;
constexpr U kPickRow = 0x6BDFC8;
constexpr U kCurCol = 0x6BDFC9;      // the grid cursor
constexpr U kCurRow = 0x6BDFCA;
constexpr U kTopChoice = 0x6BDFCB;   // the screen's top: 0 formation, 1 members
constexpr U kMemberFlags = 0x6BDFCC; // two bytes a row: party, reserve
constexpr U kSwappedA = 0x6BDFD2;    // the two rows the last formation swap moved
constexpr U kSwappedB = 0x6BDFD3;
constexpr U kReserve = 0x6BDFD4;     // the reserve's member ids
constexpr U kRefused = 0x6BDFD7;     // the member a swap refused, 0xFF none (= kReserve + 3)
constexpr U kConfigSaved = 0x6BDFD8; // the top bar's cursor while Config runs (= kReserve + 4)

// Save data and the records.
constexpr U kFormation = 0x904060;   // the formation set (u8)
constexpr U kFormationBits = 0x904061;   // which of the six cells may hold a member
constexpr U kParty = 0x904062;       // the party's member ids (0x904065: the battle order's copy)
constexpr U kPartyCopy = 0x904065;
constexpr U kCharacters = 0x903A70;  // CharacterRecords: 0xA4 a member
constexpr U kCharacterStride = 0xA4;
constexpr U kWorking = 0x802DC0;     // the party's working records' +0x80 (ObjTrio + 0x80), 0x14C a slot
constexpr U kWorkingStride = 0x14C;
constexpr U kEffectState = 0x66972C; // MoveScript_EffectState: the record of a member id
constexpr U kStyle = 0x903A5A;       // the window colour
constexpr U kBackdrop = 0x903A5B;    // the Config "Background" byte
constexpr U kGameStep = 0x66C7EA;    // Game_Step (u16)
constexpr U kPressed = 0x7E1BEC;     // Input_Pressed (u16)
constexpr U kConfirm = 0x90358E;     // Field_ConfirmButtons (u16)
constexpr U kCancel = 0x903590;      // Field_CancelButtons (u16)
constexpr U kInputFlags = 0x905BA2;  // Field_InputFlags: bit 0, the members screen allowed
constexpr U kCurrent = 0x905B84;     // the window record Field_RunTaskRecords runs
constexpr U kWindows = 0x803160;     // WindowRecords
constexpr U kWindowStride = 0x24;
constexpr U kPools = 0x803580;       // MessagePools
constexpr U kPrint = 0x904BA0;       // the print buffer
// The members screen's portrait cells: 0x9398C0 + 4 row, 0x939880 / 0x9398A0 + 6 row.
constexpr U kPortrait4 = 0x9398C0;
constexpr U kPortraitA = 0x939880;
constexpr U kPortraitB = 0x9398A0;

// Window kind 1's message cells (the choice list).
constexpr U kListText = 0x7DEE50;    // the choices' text (a pointer)
constexpr U kListLast = 0x7DEE66;    // the last choice's index (s8)
constexpr U kListCursor = 0x7DEE67;  // the cursor (s8)
constexpr U kListRows = 0x7DEE6F;    // a byte a choice: its line, then its y

// Image tables, read in place.
constexpr U kFormCells = 0x667450;   // 3 x 3 of 8 bytes: x, y words, a byte (the formation's cursor places)
constexpr U kMemberCells = 0x6674C8; // 2 a row of 8 bytes: the members screen's cursor places
constexpr U kApCost = 0x65C4DA;      // Ability_Records + 0x12: the AP cost byte, 24 a record
constexpr U kTradeKinds = 0x66AD10;  // 10 a spot: the trade's kind
constexpr U kTradeNeeds = 0x66AB5A;  // 8 a kind: three item bytes, then their counts at +3
constexpr U kIdLists = 0x656B00;     // Inventory_IdLists
constexpr U kCountLists = 0x656B14;  // Inventory_CountLists
constexpr U kListTitles = 0x66AF10;  // Win2_DrawItemList's titles by category
constexpr U kShisuTitle = 0x66AE90;  // Win1_DrawShisuPanel's title (a string)
constexpr U kShisuIds = 0x66AE8C;    // its four rows' item ids
constexpr U kShisuPiecesA = 0x66AE74;
constexpr U kShisuPiecesB = 0x66AE80;
constexpr U kListPieces0 = 0x66AE98; // Win2_DrawItemList's arrows: up off / on, down off / on
constexpr U kListPieces2 = 0x66AEB0;
constexpr U kListPieces1 = 0x66AEA4;
constexpr U kListPieces3 = 0x66AEBC;
constexpr U kUsedFormat = 0x6639C8;  // "%3d/%3d"

unsigned char& B(U address) { return At(address)[0]; }
U W(U address) { return Word(At(address)); }
void SetW(U address, U v) { SetWord(At(address), v); }
signed char Sb(U address) { return static_cast<signed char>(At(address)[0]); }
U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
// Window record n's byte / word at `at`.
U Rec(unsigned n, unsigned at) { return kWindows + kWindowStride * n + at; }
// The record Field_RunTaskRecords runs, read afresh at each use.
unsigned char* Cur() { return At(static_cast<U>(Long(At(kCurrent)))); }
int Wx(const unsigned char* r) { return static_cast<int>(Word(r + 4)); }
int Wy(const unsigned char* r) { return static_cast<int>(Word(r + 6)); }

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
void Backdrop() { SH_CALL(Menu_DrawBackdrop)(B(kBackdrop)); }
void Sound(unsigned id) { SH_CALL(Sound_PlayEffect)(static_cast<unsigned short>(id)); }
unsigned char PartyCount() { return static_cast<unsigned char>(SH_CALL(Party_Count)(0)); }
U Repeat(U pressed) { return SH_CALL(Input_AutoRepeat)(pressed); }
void Swap(unsigned char* a, unsigned char* b) { SH_AT(void (__cdecl*)(unsigned char*, unsigned char*), kSwapBytes)(a, b); }
using TitleFn = const unsigned char* (__cdecl*)(int, int, int, int, const unsigned char*);
void Piece(int x, int y, unsigned id) { SH_CALL(Menu_DrawPiece)(x, y, id, 1); }
void Row(int x, U y, unsigned colour, unsigned category, unsigned id, unsigned count, unsigned dim) {
    SH_CALL(Menu_DrawItemRow)(x, static_cast<int>(y), static_cast<int>(colour), category, id, count, static_cast<int>(dim));
}

// The member a CharacterRecords flag test is about: record MoveScript_EffectState[id].
unsigned char* Character(unsigned char id) { return At(kCharacters + kCharacterStride * B(kEffectState + id)); }

// DIV-0059's site, read at inject (0: not read yet - the fuzz's runs, before
// the patch is read back, call Text_DrawAt as the original's copy does).
U g_title_call = 0;

}  // namespace

// ===========================================================================
// The Ability screen's helpers (R2E's states call them)
// ===========================================================================

// original 0x58ED40: called by 0x58D7D0 (the Ability screen's step 0, R2E):
// window records 11 (kind 9), 12 (kind 0xD), 16 (kind 0x11, slid in from x
// 0x140) set up and opened; 19 and 21 set up, closed.
extern "C" void __cdecl AbilityMenu_InitRecords(void) {
    B(Rec(11, 3)) = 2;
    B(Rec(12, 3)) = 2;
    B(Rec(12, 0xA)) = 2;
    B(Rec(16, 3)) = 2;
    B(Rec(11, 1)) = 6;
    B(Rec(11, 2)) = 9;
    B(Rec(11, 0)) = 1;
    SetW(Rec(11, 4), 0x14);
    SetW(Rec(11, 6), 0xFFEC);
    SetW(Rec(11, 0x10), 0);
    B(Rec(12, 1)) = 6;
    B(Rec(12, 2)) = 0xD;
    B(Rec(12, 0)) = 1;
    SetW(Rec(12, 4), 0x40);
    SetW(Rec(12, 6), 0xFFEC);
    B(Rec(12, 0xB)) = 0xFF;
    B(Rec(16, 1)) = 6;
    B(Rec(16, 2)) = 0x11;
    B(Rec(16, 0)) = 1;
    SetW(Rec(16, 4), 0x140);
    SetW(Rec(16, 6), 0x3E);
    B(Rec(16, 0xB)) = 0;
    B(Rec(16, 0xA)) = 0;
    B(Rec(16, 0xC)) = 0xFF;
    B(Rec(16, 0xD)) = 0xFF;
    B(Rec(16, 8)) = 0;
    B(Rec(16, 9)) = 0;
    SetW(Rec(16, 0x10), 0);
    B(Rec(19, 1)) = 6;
    B(Rec(19, 2)) = 0xA;
    B(Rec(19, 0)) = 0;
    B(Rec(19, 0xA)) = 1;
    B(Rec(19, 0xB)) = 1;
    B(Rec(21, 1)) = 6;
    B(Rec(21, 2)) = 0xE;
    B(Rec(21, 3)) = 0;
    B(Rec(21, 0)) = 0;
}

// original 0x58EE40: called by 0x58E640 (R2E) with a mode byte: a tail jump
// through AbilityList_SortModes by it, unchecked (ours aborts past its 3).
extern "C" void __cdecl AbilityList_SortBy(unsigned mode) {
    Run("AbilityList_SortBy (0x58EE40)", AbilityList_SortModes, AbilityList_SortModes_count, mode & 0xFF, "the mode");
}

namespace {
// The three sorts' shared shape: nine passes; pass p (9 - p swaps a pass, a
// bubble) fetches the list of window record 16's member +0xA and type +0xB
// (Char_AbilityList, battle 0) afresh and walks it, `test(a, b)` deciding the
// swap of each neighbouring pair through 0x58BD50. The bytes are read again
// after every swap.
template <typename Test> void Bubble(Test swap_if) {
    U span = 9;
    for (unsigned pass = 9; pass != 0; --pass) {
        unsigned char* p = SH_CALL(Char_AbilityList)(B(Rec(16, 0xA)), B(Rec(16, 0xB)), 0);
        for (unsigned char i = 0; i < span; ++i, ++p)
            if (swap_if(p)) Swap(p, p + 1);
        --span;
    }
}
}  // namespace

// original 0x58EE50: AbilityList_SortModes[0] - the empty slots moved to the
// end: a pair swaps when its first byte is 0 and its second is not.
extern "C" void __cdecl AbilityList_Compact(void) {
    Bubble([](const unsigned char* p) { return p[0] == 0 && p[1] != 0; });
}

// original 0x58EEC0: AbilityList_SortModes[1] - compacted (AbilityList_Compact,
// called), then by the AP cost byte (Ability_Records + 0x12), highest first: a
// pair of two abilities swaps when the first costs less (unsigned).
extern "C" void __cdecl AbilityList_SortByApDown(void) {
    SH_CALL(AbilityList_Compact)();
    Bubble([](const unsigned char* p) {
        const unsigned char a = p[0];
        if (a == 0) return false;
        const unsigned char b = p[1];
        if (b == 0) return false;
        return B(kApCost + 24u * a) < B(kApCost + 24u * b);
    });
}

// original 0x58EF60: AbilityList_SortModes[2] - as AbilityList_SortByApDown,
// lowest first: the pair swaps when the first costs more.
extern "C" void __cdecl AbilityList_SortByApUp(void) {
    SH_CALL(AbilityList_Compact)();
    Bubble([](const unsigned char* p) {
        const unsigned char a = p[0];
        if (a == 0) return false;
        const unsigned char b = p[1];
        if (b == 0) return false;
        return B(kApCost + 24u * a) > B(kApCost + 24u * b);
    });
}

// original 0x58F000: called by 0x58EA70 (R2E): window record 17 (kind 0x14)
// set up at x 0x140, y 0x3E and opened, its +8, +0xA..+0xC cleared.
extern "C" void __cdecl AbilityMenu_InitRecord17(void) {
    B(Rec(17, 1)) = 6;
    B(Rec(17, 2)) = 0x14;
    B(Rec(17, 3)) = 2;
    B(Rec(17, 0)) = 1;
    SetW(Rec(17, 4), 0x140);
    SetW(Rec(17, 6), 0x3E);
    B(Rec(17, 0xB)) = 0;
    B(Rec(17, 0xA)) = 0;
    B(Rec(17, 0xC)) = 0;
    B(Rec(17, 8)) = 0;
}

// original 0x58F050: called by 0x58E9D0 (R2E): window records 13..18 freed
// (+0 = 0).
extern "C" void __cdecl FieldMenu_FreeRecords13To18(void) {
    for (unsigned n = 13; n <= 18; ++n) B(Rec(n, 0)) = 0;
}

// ===========================================================================
// The Tactics screen (FieldMenu_States[5])
// ===========================================================================

// original 0x58F080: FieldMenu_States[5] - jmp through TacticsMenu_Steps by the
// step byte 0x929F01, unchecked (ours aborts past its 7: steps 0..6, of which 6
// is TacticsFormation_Steps[0] by the tables' layout).
extern "C" void __cdecl Tactics_Run(void) {
    Run("Tactics_Run (0x58F080)", TacticsMenu_Steps, TacticsMenu_Steps_count, B(kStep), "0x929F01");
}

// original 0x58F090: TacticsMenu_Steps[0] - window records 11 (kind 9), 19, 20
// (kind 0xA, closed), 18 (kind 0xD: the top's two choices at x 0x70) and 21
// (kind 0xE, closed) set up; the backdrop; the top's choice 0; sound 0x102;
// the step on, the sub-step 0, the countdown 5.
extern "C" void __cdecl Tactics_Open(void) {
    B(Rec(11, 1)) = 6;
    B(Rec(11, 2)) = 9;
    B(Rec(11, 3)) = 2;
    B(Rec(11, 0)) = 1;
    SetW(Rec(11, 4), 0x14);
    SetW(Rec(11, 6), 0xFFEC);
    SetW(Rec(11, 0x10), 0);
    for (unsigned n = 19; n <= 20; ++n) {
        B(Rec(n, 1)) = 6;
        B(Rec(n, 2)) = 0xA;
        B(Rec(n, 0)) = 0;
        B(Rec(n, 0xA)) = 0;
    }
    const unsigned char kind = B(kBackdrop);
    B(Rec(18, 1)) = 6;
    B(Rec(18, 2)) = 0xD;
    B(Rec(18, 3)) = 2;
    B(Rec(18, 0)) = 1;
    B(Rec(18, 0xA)) = 7;
    B(Rec(18, 0xB)) = 0xFF;
    SetW(Rec(18, 4), 0x70);
    SetW(Rec(18, 6), 0xFFEC);
    B(Rec(21, 1)) = 6;
    B(Rec(21, 2)) = 0xE;
    B(Rec(21, 0)) = 0;
    SH_CALL(Menu_DrawBackdrop)(kind);
    B(kTopChoice) = 0;
    Sound(0x102);
    const unsigned char step = B(kStep);
    B(kSub) = 0;
    B(kTimer) = 5;
    B(kStep) = static_cast<unsigned char>(step + 1);
}

// original 0x58F170: TacticsMenu_Steps[1] - the top. Record 11's word +0x10 is
// 0x1F + the choice; while record 18 has arrived (+3 0) record 21 (the hand)
// is shown at 18's x + 0x30 * choice, y + 4. Left / right (auto-repeated, 0xA000)
// toggle the choice with sound 0x101. Confirm: choice 0 builds the formation
// grid (TacticsFormation_Build), choice 1 the reserve (TacticsMembers_Build)
// when Field_InputFlags bit 0 allows, else sound 0x107 and nothing; then
// sounds 0x102 and 0x104, record 18's +0xB the choice, the hand closed, the
// step on by the choice + 1 (2 formation, 3 members). Cancel: sounds 0x102,
// 0x106, records 11 and 18 to state 1 (out), 19..21 closed, the step on by 3
// (4, the close).
extern "C" void __cdecl Tactics_Top(void) {
    Backdrop();
    const unsigned choice = B(kTopChoice);
    SetW(Rec(11, 0x10), choice + 0x1F);
    if (B(Rec(18, 3)) == 0) {
        const U x = (choice * 0x30u + static_cast<U>(Long(At(Rec(18, 4))))) & 0xFFFF;
        const U y = (W(Rec(18, 6)) + 4u) & 0xFFFF;
        B(Rec(21, 0)) = 1;
        SetW(Rec(21, 4), x);
        SetW(Rec(21, 6), y);
    } else {
        B(Rec(21, 0)) = 0;
    }
    const U pressed = W(kPressed);
    B(Rec(18, 0xB)) = 0xFF;
    if (Repeat(pressed & 0xA000) & 0xA000) {
        Sound(0x101);
        B(kTopChoice) = static_cast<unsigned char>(B(kTopChoice) ^ 1);
    }
    const U now = W(kPressed);
    if (W(kConfirm) & now) {
        if (B(kTopChoice) == 0) {
            SH_CALL(TacticsFormation_Build)();
        } else if (B(kInputFlags) & 1) {
            SH_CALL(TacticsMembers_Build)();
        } else {
            Sound(0x107);
            return;
        }
        Sound(0x102);
        Sound(0x104);
        const unsigned char c = B(kTopChoice);
        const unsigned char step = B(kStep);
        B(Rec(18, 0xB)) = c;
        B(Rec(21, 0)) = 0;
        B(kSub) = 0;
        B(kStep) = static_cast<unsigned char>(step + c + 1);
        B(kTimer) = 5;
        return;
    }
    if (!(W(kCancel) & now)) return;
    Sound(0x102);
    Sound(0x106);
    const unsigned char step = B(kStep);
    B(Rec(11, 3)) = 1;
    B(Rec(18, 3)) = 1;
    B(Rec(21, 0)) = 0;
    B(Rec(19, 0)) = 0;
    B(Rec(20, 0)) = 0;
    B(kStep) = static_cast<unsigned char>(step + 3);
    B(kSub) = 0;
    B(kTimer) = 5;
}

// original 0x58F2F0: TacticsMenu_Steps[2] - jmp through TacticsFormation_Steps
// by the sub-step 0x929F02, unchecked (ours aborts past its 3).
extern "C" void __cdecl Tactics_Formation(void) {
    Run("Tactics_Formation (0x58F2F0)", TacticsFormation_Steps, TacticsFormation_Steps_count, B(kSub), "0x929F02");
}

namespace {
// The party's window records 4.. : `body(i)` for i below Party_Count(0), the
// count asked again after each (the shape of six of the Tactics steps).
template <typename Body> void ForParty(Body body) {
    unsigned char i = 0;
    if (PartyCount() == 0) return;
    do {
        const unsigned char at = i;
        ++i;
        body(at);
    } while (i < PartyCount());
}
// The countdown: one less; true when it reached 0.
bool CountedDown() {
    const unsigned char t = static_cast<unsigned char>(B(kTimer) - 1);
    B(kTimer) = t;
    return t == 0;
}
}  // namespace

// original 0x58F300: TacticsFormation_Steps[0] (and TacticsMenu_Steps[6]) -
// the backdrop; at the countdown's end the party's records 4.. get +0xB = 1
// and the sub-step goes on.
extern "C" void __cdecl TacticsFormation_Enter(void) {
    Backdrop();
    if (!CountedDown()) return;
    ForParty([](unsigned char i) { B(Rec(4u + i, 0xB)) = 1; });
    B(kSub) = static_cast<unsigned char>(B(kSub) + 1);
}

// original 0x58F370: TacticsFormation_Steps[1] - the formation grid. Record 19
// (the picked cell's mark) at kFormCells[3 row + col] of the cell picked, or
// closed; record 20 (the cursor) at the cursor's cell; records 12..17's +0xC
// cleared, then the set cell's record (11 + col + 2 row) +0xC = 1 << row for a
// cursor in column 0, else 7; record 11's word +0x10 0x3B, or 0x76 + col +
// 2 row. Confirm: on column 1..2 the cursor's cell becomes the formation set
// (sound 0x104, the pick dropped, 0x6BDFD2 / D3 the cell); on column 0 with no
// pick the cursor's cell is picked (0x103); with a pick, the two party bytes
// 0x904065 + row swapped (0x58BD50, sound 0x104) and the rows noted when they
// differ, the pick dropped. Cancel (0x106): a pick dropped; else entered
// directly (0x929F10 1) Game_Step on and the step 6; else 0x904060 written,
// the sound 0x102, records 12..17 of kind 0xB in use to state 1, the sub-step
// on and the countdown 5. Then up / down (0x5000) move the cursor's row with a
// wrap, skipping cells the grid has 0 for; with a pick, left / right (0xA000)
// its column likewise; sound 0x101 when the cell moved; 0x904060 = the set's
// col + 2 row - 1 again. The grid loops never end on a column of three empty
// cells, as the original's never would (section 7).
extern "C" void __cdecl TacticsFormation_Pick(void) {
    unsigned char vdelta = 0, hdelta = 0;
    const unsigned char zero = 0;
    Backdrop();
    {
        const unsigned char pick = B(kPickCol);
        B(Rec(19, 0)) = 0;
        if (pick != 0x7F) {
            const U e = kFormCells + 8u * static_cast<U>(Sb(kPickRow) * 3 + static_cast<signed char>(pick));
            B(Rec(19, 0)) = 1;
            B(Rec(19, 0xB)) = 0;
            SetW(Rec(19, 4), W(e));
            SetW(Rec(19, 6), W(e + 2));
            B(Rec(19, 0xA)) = static_cast<unsigned char>(B(e + 4) + 1);
        }
    }
    unsigned char col = B(kCurCol);
    {
        const int row = Sb(kCurRow);
        const int c = static_cast<signed char>(col);
        B(Rec(20, 0)) = 1;
        B(Rec(20, 0xB)) = 1;
        const U e = kFormCells + 8u * static_cast<U>(3 * row + c);
        SetW(Rec(20, 4), W(e));
        SetW(Rec(20, 6), W(e + 2));
        B(Rec(20, 0xA)) = static_cast<unsigned char>(B(e + 4) + 1);
        for (unsigned n = 12; n <= 17; ++n) B(Rec(n, 0xC)) = 0;
        const unsigned char mark = col == 0 ? static_cast<unsigned char>(1u << (static_cast<U>(row) & 31)) : 7;
        const int set = Sb(kSetCol) + 2 * Sb(kSetRow);
        B(Rec(11, 0xC) + kWindowStride * static_cast<U>(set)) = mark;
        SetW(Rec(11, 0x10), col == 0 ? 0x3Bu : static_cast<U>(c + 2 * row + 0x76));
    }
    const U pressed = W(kPressed);
    if (W(kConfirm) & pressed) {
        B(Rec(19, 0)) = 0;
        if (col != 0) {
            Sound(0x104);
            const unsigned char cc = B(kCurCol), cr = B(kCurRow);
            B(kSetCol) = cc;
            B(kSetRow) = cr;
            B(kPickCol) = 0x7F;
            B(kPickRow) = 0x7F;
            B(kSwappedA) = cc;
            B(kSwappedB) = cr;
        } else if (B(kPickCol) == 0x7F) {
            Sound(0x103);
            B(kPickCol) = B(kCurCol);
            B(kPickRow) = B(kCurRow);
        } else {
            Sound(0x104);
            unsigned char* const picked = At(kPartyCopy + static_cast<U>(static_cast<int>(Sb(kPickRow))));
            unsigned char* const cursor = At(kPartyCopy + static_cast<U>(static_cast<int>(Sb(kCurRow))));
            Swap(cursor, picked);
            const unsigned char a = B(kCurRow), b = B(kPickRow);
            if (a != b) {
                B(kSwappedA) = a;
                B(kSwappedB) = b;
            }
            B(kPickCol) = 0x7F;
            B(kPickRow) = 0x7F;
        }
    } else if (W(kCancel) & pressed) {
        Sound(0x106);
        const unsigned char pick = B(kPickCol);
        B(Rec(19, 0)) = 0;
        B(Rec(20, 0)) = 0;
        if (pick != 0x7F) {
            B(kPickCol) = 0x7F;
            B(kPickRow) = 0x7F;
        } else if (B(kDirect) == 1) {
            SetW(kGameStep, W(kGameStep) + 1);
            B(kStep) = 6;
            B(kSub) = 0;
            B(kDirect) = 0;
        } else {
            const unsigned char set = static_cast<unsigned char>(B(kSetRow) * 2 + B(kSetCol) - 1);
            B(kTimer) = 0;
            B(kFormation) = set;
            Sound(0x102);
            for (unsigned n = 12; n <= 17; ++n)
                if (B(Rec(n, 0)) != 0 && B(Rec(n, 2)) == 0xB) B(Rec(n, 3)) = 1;
            const unsigned char sub = B(kSub);
            B(kTimer) = 5;
            B(kSub) = static_cast<unsigned char>(sub + 1);
        }
    }
    // the cursor: up / down, then (with a pick) left / right
    unsigned char step;
    const U v = Repeat(W(kPressed) & 0x5000);
    if (v & 0x1000) step = 0xFF;
    else step = vdelta;
    if (v & 0x4000) {
        vdelta = 1;
        step = vdelta;
    }
    unsigned char row = B(kCurRow);
    unsigned char c = B(kCurCol);
    const unsigned char old_row = row;
    const int ccol = static_cast<signed char>(c);
    do {
        row = static_cast<unsigned char>(row + step);
        if (static_cast<signed char>(row) < 0) row = 2;
        else if (static_cast<signed char>(row) > 2) row = 0;
    } while (B(kGrid + static_cast<U>(3 * static_cast<signed char>(row) + ccol)) == 0);
    B(kCurRow) = row;
    unsigned char before;
    if (B(kPickCol) == 0) {
        before = zero;
    } else {
        const U h = Repeat(W(kPressed) & 0xA000);
        if (h & 0x8000) hdelta = 0xFF;
        if (h & 0x2000) hdelta = 1;
        row = B(kCurRow);
        c = B(kCurCol);
        const int base = 3 * static_cast<signed char>(row);
        before = c;
        do {
            c = static_cast<unsigned char>(c + hdelta);
            if (static_cast<signed char>(c) < 0) c = 2;
            else if (static_cast<signed char>(c) > 2) c = 0;
        } while (B(kGrid + static_cast<U>(base + static_cast<signed char>(c))) == 0);
        B(kCurCol) = c;
    }
    if (before != c || old_row != row) Sound(0x101);
    B(kFormation) = static_cast<unsigned char>(B(kSetRow) * 2 + B(kSetCol) - 1);
}

// original 0x58F760: TacticsFormation_Steps[2] - the backdrop; at the
// countdown's end the party's records 4.. get +0xB = 0 and the step goes back
// one (to the top).
extern "C" void __cdecl TacticsFormation_Leave(void) {
    Backdrop();
    if (!CountedDown()) return;
    ForParty([](unsigned char i) { B(Rec(4u + i, 0xB)) = 0; });
    B(kStep) = static_cast<unsigned char>(B(kStep) - 1);
}

// original 0x58F7D0: TacticsMenu_Steps[3] - jmp through TacticsMembers_Steps by
// the sub-step 0x929F02, unchecked (ours aborts past its 3).
extern "C" void __cdecl Tactics_Members(void) {
    Run("Tactics_Members (0x58F7D0)", TacticsMembers_Steps, TacticsMembers_Steps_count, B(kSub), "0x929F02");
}

// original 0x58F7E0: TacticsMembers_Steps[0] - the backdrop; at the countdown's
// end each party record 4.. whose member (the party byte 0x904062 + 3 rec[+0xB]
// + rec[+0xA]) has CharacterRecords +0xB bit 1 gets +0xC = 1; the sub-step on.
extern "C" void __cdecl TacticsMembers_Enter(void) {
    Backdrop();
    if (!CountedDown()) return;
    ForParty([](unsigned char i) {
        const U r = Rec(4u + i, 0);
        const unsigned char id = B(kParty + 3u * B(r + 0xB) + B(r + 0xA));
        if (Character(id)[0xB] & 2) B(r + 0xC) = 1;
    });
    B(kSub) = static_cast<unsigned char>(B(kSub) + 1);
}

// original 0x58F890: TacticsMembers_Steps[1] - the members screen. Record 19
// (the cursor) at kMemberCells[col + 2 row]; record 20 (the cell picked) there
// too, or closed; with a member refused (0x6BDFD7) its name to text record 0
// (TextRecord_Set, 8 bytes) and record 11's word +0x10 0x61, else 0x60. The
// cursor: up / down / left / right auto-repeated (0xF000); left / right switch
// the column while the reserve has members (record 12's +0xA); the row is held
// inside the column's count, then up and down wrap; sound 0x100 when it moved.
// Confirm: with nothing picked the cursor's cell is picked (0x103); else
// TacticsMembers_Swap, its answer to 0x6BDFD7 and sound 0x103 (done, 0xFF) or
// 0x107 (refused), the pick dropped. Cancel (0x106): a pick dropped; else the
// sub-step on, the countdown 5, record 12 to state 1, 19 and 20 closed.
extern "C" void __cdecl TacticsMembers_Pick(void) {
    Backdrop();
    {
        const U e = kMemberCells + 8u * static_cast<U>(Sb(kCol) + 2 * Sb(kRow));
        B(Rec(20, 0)) = 0;
        B(Rec(19, 0)) = 1;
        B(Rec(19, 0xB)) = 1;
        SetW(Rec(19, 4), W(e));
        B(Rec(19, 0xA)) = B(e + 4);
        SetW(Rec(19, 6), W(e + 2));
        const unsigned char held = B(kHeldRow);
        if (held != 0x7F) {
            const U f = kMemberCells + 8u * static_cast<U>(Sb(kHeldCol) + 2 * static_cast<signed char>(held));
            B(Rec(20, 0)) = 1;
            B(Rec(20, 0xB)) = 0;
            SetW(Rec(20, 4), W(f));
            SetW(Rec(20, 6), W(f + 2));
            B(Rec(20, 0xA)) = B(f + 4);
        }
    }
    {
        const unsigned char refused = B(kRefused);
        if (refused == 0xFF) {
            SetW(Rec(11, 0x10), 0x60);
        } else {
            SH_CALL(TextRecord_Set)(0, 8, Character(refused));
            SetW(Rec(11, 0x10), 0x61);
        }
    }
    const U keys = Repeat(W(kPressed) & 0xF000);
    const unsigned char count = PartyCount();
    unsigned char col = B(kCol);
    const unsigned char reserve = B(Rec(12, 0xA));
    const unsigned char old_col = col;
    if ((keys & 0xA000) && reserve != 0) {
        col = static_cast<unsigned char>(col ^ 1);
        B(kCol) = col;
    }
    unsigned char row = B(kRow);
    const int old_row = static_cast<signed char>(row);
    if (col == 0) {
        if (old_row > static_cast<int>(count) - 1) {
            row = static_cast<unsigned char>(count - 1);
            B(kRow) = row;
        }
    } else {
        if (old_row > static_cast<int>(reserve) - 1) {
            row = static_cast<unsigned char>(reserve - 1);
            B(kRow) = row;
        }
    }
    if (keys & 0x1000) {
        row = static_cast<unsigned char>(row - 1);
        B(kRow) = row;
        if (static_cast<signed char>(row) < 0) {
            row = B(kCol) == 0 ? static_cast<unsigned char>(count - 1) : static_cast<unsigned char>(reserve - 1);
            B(kRow) = row;
        }
    } else if (keys & 0x4000) {
        const unsigned char now = B(kCol);
        row = static_cast<unsigned char>(row + 1);
        B(kRow) = row;
        const int last = now == 0 ? static_cast<int>(count) - 1 : static_cast<int>(reserve) - 1;
        if (static_cast<signed char>(row) > last) {
            row = 0;
            B(kRow) = row;
        }
    }
    if (static_cast<int>(old_col) != Sb(kCol) || old_row != static_cast<signed char>(row)) Sound(0x100);
    const U pressed = W(kPressed);
    if (W(kConfirm) & pressed) {
        if (B(kHeldCol) == 0x7F) {
            Sound(0x103);
            B(kHeldCol) = B(kCol);
            B(kHeldRow) = B(kRow);
            return;
        }
        const unsigned char refused = SH_CALL(TacticsMembers_Swap)();
        B(kRefused) = refused;
        Sound(refused == 0xFF ? 0x103 : 0x107);
        B(kHeldCol) = 0x7F;
        B(kHeldRow) = 0x7F;
        return;
    }
    if (!(W(kCancel) & pressed)) return;
    Sound(0x106);
    if (B(kHeldCol) != 0x7F) {
        B(kHeldCol) = 0x7F;
        B(kHeldRow) = 0x7F;
        return;
    }
    const unsigned char sub = B(kSub);
    B(kTimer) = 5;
    B(Rec(12, 3)) = 1;
    B(kSub) = static_cast<unsigned char>(sub + 1);
    B(Rec(19, 0)) = 0;
    B(Rec(20, 0)) = 0;
}

// original 0x58FB60: TacticsMembers_Steps[2] - the backdrop; at the
// countdown's end the party's records 4.. get +0xC = 0 and the step goes back
// two (to the top).
extern "C" void __cdecl TacticsMembers_Leave(void) {
    Backdrop();
    if (!CountedDown()) return;
    ForParty([](unsigned char i) { B(Rec(4u + i, 0xC)) = 0; });
    B(kStep) = static_cast<unsigned char>(B(kStep) - 2);
}

// original 0x58FBD0: TacticsMenu_Steps[4] - the backdrop; at the countdown's
// end records 12..18 freed (FieldMenu_FreeRecords12To18), the party's records
// 4.. to state 4, records 7..10 to state 2, the countdown 5, and the field
// menu back to its top bar (state 1, step 0).
extern "C" void __cdecl Tactics_Close(void) {
    Backdrop();
    if (!CountedDown()) return;
    SH_CALL(FieldMenu_FreeRecords12To18)();
    ForParty([](unsigned char i) { B(Rec(4u + i, 3)) = 4; });
    B(kTimer) = 5;
    for (unsigned n = 7; n <= 10; ++n) B(Rec(n, 3)) = 2;
    B(kMode) = 1;
    B(kStep) = 0;
}

// original 0x58FC60: TacticsMenu_Steps[5] - Tactics entered at the formation:
// record 11 (kind 9) at x 0x14, y 0x10; records 19, 20 set up, closed; the
// party's records 4.. (kind 0 of handler 6, +0xA the slot) at x 0x11, y 0x3E +
// 0x36 slot, +0xB 1; the backdrop; the grid built (TacticsFormation_Build); the
// step 2, sub-step 0, countdown 5.
extern "C" void __cdecl Tactics_OpenFormation(void) {
    B(Rec(11, 1)) = 6;
    B(Rec(11, 2)) = 9;
    B(Rec(11, 3)) = 2;
    B(Rec(11, 0)) = 1;
    SetW(Rec(11, 4), 0x14);
    SetW(Rec(11, 6), 0x10);
    SetW(Rec(11, 0x10), 0);
    for (unsigned n = 19; n <= 20; ++n) {
        B(Rec(n, 1)) = 6;
        B(Rec(n, 2)) = 0xA;
        B(Rec(n, 0)) = 0;
        B(Rec(n, 0xA)) = 0;
    }
    ForParty([](unsigned char i) {
        const U r = Rec(4u + i, 0);
        B(r + 0xA) = i;
        B(r + 1) = 6;
        B(r + 2) = 0;
        B(r + 3) = 0;
        B(r + 0) = 1;
        SetW(r + 4, 0x11);
        SetW(r + 6, 0x36u * i + 0x3E);
        B(r + 0xB) = 1;
        B(r + 0xC) = 0;
    });
    Backdrop();
    SH_CALL(TacticsFormation_Build)();
    B(kStep) = 2;
    B(kSub) = 0;
    B(kTimer) = 5;
}

// original 0x58FD60: called by Tactics_Top and Tactics_OpenFormation - the
// formation grid. By Party_Count(0): 1 member 1 cell from record 12 (+0xA from
// 0), 2 members 3 (from 1), else 6 (from 4). Rows 0..2: column 0 is open for a
// row below the count; columns 1..2 are six cells n = 0..5 in order, open when
// 0x904061 bit n is set and n is below the limit, each such cell's window
// record 12 + n (handler 6, kind 0xB, state 2) placed at x 0xF5 + 0x4B col, y
// 0x3E + 0x36 row, +0xA n + the base, +0xB n, +0x10 0xE00. Then the set's cell
// from 0x904060 (column (v & 1) + 1, row v >> 1), the pick dropped, the cursor
// at column 0 row 0.
extern "C" void __cdecl TacticsFormation_Build(void) {
    const unsigned char count = PartyCount();
    unsigned char limit, base;
    if (count == 1) {
        base = 0;
        limit = 1;
    } else if (count == 2) {
        base = 1;
        limit = 3;
    } else {
        base = 4;
        limit = 6;
    }
    unsigned char mask = 1, n = 0;
    for (unsigned char row = 0; row < 3; ++row) {
        for (unsigned char col = 0; col < 3; ++col) {
            unsigned char* const cell = At(kGrid + 3u * row + col);
            if (col == 0 && row < count) {
                *cell = 1;
                continue;
            }
            *cell = 0;
            if (col == 0) continue;
            if ((B(kFormationBits) & mask) && n < limit) {
                *cell = 1;
                const U r = Rec(12u + n, 0);
                SetW(r + 4, 0x4Bu * col + 0xF5);
                B(r + 1) = 6;
                B(r + 2) = 0xB;
                B(r + 3) = 2;
                B(r + 0) = 1;
                B(r + 0xB) = n;
                B(r + 0xC) = 0;
                SetW(r + 0x10, 0xE00);
                SetW(r + 6, 0x36u * row + 0x3E);
                B(r + 0xA) = static_cast<unsigned char>(n + base);
            } else {
                *cell = 0;
            }
            ++n;
            mask = static_cast<unsigned char>(mask << 1);
        }
    }
    const unsigned char set = B(kFormation);
    B(kCurCol) = 0;
    B(kSetRow) = static_cast<unsigned char>(set >> 1);
    B(kSetCol) = static_cast<unsigned char>((set & 1) + 1);
    B(kPickCol) = 0x7F;
    B(kPickRow) = 0x7F;
    B(kCurRow) = 0;
}

// original 0x58FEF0: called by Tactics_Top - the reserve: each of the eight
// CharacterRecords with +0xB bit 0 whose id (+9) is not among the party's
// Party_Count(0) bytes goes into 0x6BDFD4.. in record order; record 12 (handler
// 6, kind 0x13, at x 0x140, y 0x3B, +0x20 the list) holds their count at +0xA.
// The flags 0x6BDFCC + 2 row (party) are 1 below the count and 0 to row 2, and
// 0x6BDFCD + 2 row (reserve) 1 below the reserve's count - written after the
// list, which they overlap from the fourth row on (section 7). The cursor at
// 0, 0, nothing picked, nobody refused.
extern "C" void __cdecl TacticsMembers_Build(void) {
    const unsigned char count = PartyCount();
    unsigned char n = 0;
    B(Rec(12, 0xA)) = 0;
    for (U r = kCharacters; r < kCharacters + 8 * kCharacterStride; r += kCharacterStride) {
        if (!(B(r + 0xB) & 1)) continue;
        unsigned j = 0;
        while (j < count && B(r + 9) != B(kParty + j)) ++j;
        if (j == count) B(kReserve + n++) = B(r + 9);
    }
    B(Rec(12, 0xA)) = n;
    B(Rec(12, 0)) = 1;
    B(Rec(12, 1)) = 6;
    B(Rec(12, 2)) = 0x13;
    B(Rec(12, 3)) = 2;
    SetW(Rec(12, 4), 0x140);
    SetW(Rec(12, 6), 0x3B);
    SetLong(At(Rec(12, 0x20)), static_cast<std::int32_t>(kReserve));
    unsigned from = 0;
    if (count > 0) {
        for (unsigned i = 0; i < count; ++i) B(kMemberFlags + 2 * i) = 1;
        from = count;
    }
    if (count == 0 || count < 3)
        for (U p = kMemberFlags + 2 * from; p < kMemberFlags + 6; p += 2) B(p) = 0;
    for (unsigned j = 0; j < n; ++j) B(kMemberFlags + 1 + 2 * j) = 1;
    B(kCol) = 0;
    B(kRow) = 0;
    B(kHeldCol) = 0x7F;
    B(kHeldRow) = 0x7F;
    B(kRefused) = 0xFF;
}

// original 0x590020: called by TacticsMembers_Pick - the swap of the cell
// picked with the cursor's, answering al 0xFF when done, or the member that
// refused it. Both in the party column: the two party records' +0xC, the party
// bytes (0x58BD50) and the rows' portrait cells (0x9398C0 + 4 row, and 0x939880
// / 0x9398A0 + 6 row, byte by byte) exchanged. Both in the reserve column: the
// reserve bytes exchanged (0x58BD50). Across: the party member A (row of the
// party column) and reserve member B; A's or B's CharacterRecords +0xB bit 1
// refuses (A first) and is the answer; else sound 0x103, every party byte
// (0x904062.. and the copy 0x904065..) that is A becomes B, every reserve byte
// that is B becomes A, and A's row's portrait cells cleared.
extern "C" unsigned char __cdecl TacticsMembers_Swap(void) {
    const unsigned char col = B(kCol);
    if (col == B(kHeldCol)) {
        if (col != 0) {
            Swap(At(kReserve + static_cast<U>(static_cast<int>(Sb(kRow)))),
                 At(kReserve + static_cast<U>(static_cast<int>(Sb(kHeldRow)))));
            return 0xFF;
        }
        {
            const int held = Sb(kHeldRow), row = Sb(kRow);
            const U h = Rec(4, 0xC) + kWindowStride * static_cast<U>(held);
            const U r = Rec(4, 0xC) + kWindowStride * static_cast<U>(row);
            const unsigned char a = B(h), b = B(r);
            B(r) = a;
            B(h) = b;
            Swap(At(kParty + static_cast<U>(row)), At(kParty + static_cast<U>(held)));
        }
        const int held = Sb(kHeldRow), row = Sb(kRow);
        U p = kPortrait4 + 4u * static_cast<U>(held), q = kPortrait4 + 4u * static_cast<U>(row);
        for (unsigned k = 0; k < 4; ++k, ++p, ++q) {
            const unsigned char x = B(p), y = B(q);
            B(q) = x;
            B(p) = y;
        }
        const U c = 6u * static_cast<U>(held), e = 6u * static_cast<U>(row);
        for (unsigned k = 0; k < 6; ++k) {
            unsigned char x = B(kPortraitB + c + k);
            unsigned char y = B(kPortraitB + e + k);
            B(kPortraitB + e + k) = x;
            x = B(kPortraitA + c + k);
            B(kPortraitB + c + k) = y;
            y = B(kPortraitA + e + k);
            B(kPortraitA + e + k) = x;
            B(kPortraitA + c + k) = y;
        }
        return 0xFF;
    }
    int row;
    unsigned char a, b;
    if (col == 0) {
        row = Sb(kRow);
        a = B(kParty + static_cast<U>(row));
        b = B(kReserve + static_cast<U>(static_cast<int>(Sb(kHeldRow))));
    } else {
        row = Sb(kHeldRow);
        a = B(kParty + static_cast<U>(row));
        b = B(kReserve + static_cast<U>(static_cast<int>(Sb(kRow))));
    }
    if (Character(a)[0xB] & 2) return a;
    if (Character(b)[0xB] & 2) return b;
    Sound(0x103);
    unsigned char i = 0;
    if (PartyCount() != 0) {
        do {
            if (B(kParty + i) == a) B(kParty + i) = b;
            if (B(kPartyCopy + i) == a) B(kPartyCopy + i) = b;
            ++i;
        } while (i < PartyCount());
    }
    const unsigned char reserve = B(Rec(12, 0xA));
    for (unsigned j = 0; j < reserve; ++j)
        if (B(kReserve + j) == b) B(kReserve + j) = a;
    SetLong(At(kPortrait4 + 4u * static_cast<U>(row)), 0);
    SetLong(At(kPortraitA + 6u * static_cast<U>(row)), 0);
    SetW(kPortraitA + 6u * static_cast<U>(row) + 4, 0);
    SetLong(At(kPortraitB + 6u * static_cast<U>(row)), 0);
    SetW(kPortraitB + 6u * static_cast<U>(row) + 4, 0);
    return 0xFF;
}

// original 0x5902A0: called by Tactics_Close - window records 12..18 freed.
extern "C" void __cdecl FieldMenu_FreeRecords12To18(void) {
    for (unsigned n = 12; n <= 18; ++n) B(Rec(n, 0)) = 0;
}

// ===========================================================================
// The field menu's states 7 (Config) and 8
// ===========================================================================

// original 0x5902D0: FieldMenu_States[8] - jmp through FieldMenu_State8Steps
// (four bare rets) by 0x929F01, unchecked (ours aborts past its 4).
extern "C" void __cdecl FieldMenu_State8Run(void) {
    Run("FieldMenu_State8Run (0x5902D0)", FieldMenu_State8Steps, FieldMenu_State8Steps_count, B(kStep), "0x929F01");
}

// original 0x5902E0: FieldMenu_States[7] - jmp through ConfigMenu_Steps by
// 0x929F01, unchecked (ours aborts past its 3).
extern "C" void __cdecl ConfigMenu_Run(void) {
    Run("ConfigMenu_Run (0x5902E0)", ConfigMenu_Steps, ConfigMenu_Steps_count, B(kStep), "0x929F01");
}

// original 0x5902F0: ConfigMenu_Steps[0] - the backdrop; the step on, the
// countdown 5, the sub-step 0, 0x929F03 = 2, the top bar's cursor kept at
// 0x6BDFD8 and cleared (Config uses it for its row).
extern "C" void __cdecl ConfigMenu_Open(void) {
    Backdrop();
    const unsigned char step = B(kStep);
    const unsigned char cursor = B(kTopCursor);
    B(kStep) = static_cast<unsigned char>(step + 1);
    B(kTimer) = 5;
    B(kSub) = 0;
    B(kByte03) = 2;
    B(kConfigSaved) = cursor;
    B(kTopCursor) = 0;
}

// original 0x590340: ConfigMenu_Steps[1] - a tail jump to the Config screen's
// own machine 0x460CB0 (R4F's; docs/menu-screens.md section 1).
extern "C" void __cdecl ConfigMenu_Body(void) {
    SH_AT(Handler, kConfigMachine)();
}

// original 0x590350: ConfigMenu_Steps[2] - the backdrop; the party's records
// 4.. to state 4 at x -0x96; records 7..10 to state 2; the countdown 0, the
// field menu back to its top bar (state 1, step 0, sub-step 0) with its cursor
// from 0x6BDFD8.
extern "C" void __cdecl ConfigMenu_Close(void) {
    Backdrop();
    ForParty([](unsigned char i) {
        B(Rec(4u + i, 3)) = 4;
        SetW(Rec(4u + i, 4), 0xFF6A);
    });
    const unsigned char cursor = B(kConfigSaved);
    for (unsigned n = 7; n <= 10; ++n) B(Rec(n, 3)) = 2;
    B(kTimer) = 0;
    B(kMode) = 1;
    B(kStep) = 0;
    B(kSub) = 0;
    B(kTopCursor) = cursor;
}

// ===========================================================================
// Three helpers other modules called by address
// ===========================================================================

// original 0x590E80 (PSX 0x80165F60, pairs_propagated gap47): *stat (u16) +=
// delta's low word as s16, clamped to [0, cap's low word] (unsigned against
// the cap); answers the change made: 0 in ax when the stat is already at the
// cap (a rise) or at 0 (a fall), or the delta is 0; the delta when nothing was
// clamped; cap - old or -old when clamped. Callers (Effect_DrainHp /
// Effect_DrainAp) read the answer's low word; the original's upper half is
// the delta's, or in the clamped answers its caller's ecx's (not reproducible:
// ours puts 0 there).
extern "C" unsigned __cdecl Stat_AddClampedTo(unsigned short* stat, unsigned cap, unsigned delta) {
    const U old = *stat;
    const auto d = static_cast<short>(delta);
    if (d > 0) {
        if (old == (cap & 0xFFFF)) return delta & 0xFFFF0000u;
        const U sum = old + delta;
        *stat = static_cast<unsigned short>(sum);
        if ((sum & 0xFFFF) <= (cap & 0xFFFF)) return delta;
        *stat = static_cast<unsigned short>(cap);
        return cap - old;
    }
    if (d < 0) {
        if (old == 0) return delta & 0xFFFF0000u;
        const U sum = old + delta;
        *stat = static_cast<unsigned short>(sum);
        if (static_cast<short>(sum) >= 0) return delta;
        *stat = 0;
        return 0u - old;
    }
    return delta & 0xFFFF0000u;
}

// original 0x591AC0: called by SharedList_DrawList (field_s.cpp) - how many of
// a member's ten ability bytes of list `which` (0..3: +0x60, +0x6A, +0x74,
// +0x7E) are set, in the CharacterRecords record of member byte (current 0) or
// the party's working record (ObjTrio + 0x80, 0x14C a slot); any other `which`
// counts the 0x80 bytes at 0x904574. Answers al.
extern "C" unsigned char __cdecl AbilityList_CountSet(unsigned member, unsigned which, unsigned current) {
    const U m = member & 0xFF;
    const U base = (current & 0xFF) == 0 ? kCharacters + kCharacterStride * m : kWorking + kWorkingStride * m;
    U p;
    unsigned n = 10;
    switch (which & 0xFF) {
    case 0: p = base + 0x60; break;
    case 1: p = base + 0x6A; break;
    case 2: p = base + 0x74; break;
    case 3: p = base + 0x7E; break;
    default:
        p = 0x904574;
        n = 0x80;
        break;
    }
    unsigned char set = 0;
    for (unsigned i = 0; i < n; ++i, ++p)
        if (B(p) != 0) ++set;
    return set;
}

// original 0x594D90: called by ItemTrade_Confirm - the trade's needs taken: for
// the trade kind 0x66AD10[the s8 0x6BE08C + 10 * spot 0x905B88], up to three
// item bytes of 0x66AB5A + 8 kind (0xFF ends) each removed from category 0 as
// item byte + 0x38, count (u16) the s8 product of its count byte (+3) and the
// s8 0x6BE08E - Inventory_Remove(0, item, count). The removal's kind is looked
// up again with 0x6BE08C's low nibble.
extern "C" void __cdecl ItemTrade_TakeNeeds(void) {
    for (U k = 0; k < 3; ++k) {
        const int c = Sb(0x6BE08C);
        const U row = 10u * B(0x905B88);
        const U kind = B(kTradeKinds + static_cast<U>(c) + row);
        if (B(kTradeNeeds + 8 * kind + k) == 0xFF) return;
        const auto q = static_cast<signed char>(B(kTradeNeeds + 8 * kind + 3 + k));
        const U count = static_cast<U>(static_cast<std::uint16_t>(q * Sb(0x6BE08E)));
        const U kind2 = B(kTradeKinds + (static_cast<U>(c) & 0xF) + row);
        const U item = static_cast<unsigned char>(B(kTradeNeeds + k + 8 * kind2) + 0x38);
        SH_CALL(Inventory_Remove)(0, item, count);
    }
}

// ===========================================================================
// Window kind 1 (Window_Kind1Open / Window_Kind1Frame's calls)
// ===========================================================================

// original 0x596090 (PSX 0x8015AB60, gap47): kind 1's choices drawn - each of
// choices 0..the s8 0x7DEE66 through Text_DrawImmediate at x 0x3A and its y
// byte (0x7DEE6F + n), the text from 0x7DEE50 and on from where each ended.
// The y word's upper half is the text pointer's, as the original's eax has it.
extern "C" void __cdecl Window_Kind1List(void) {
    U text = static_cast<U>(Long(At(kListText)));
    if (Sb(kListLast) < 0) return;
    int n = 0;
    do {
        const U y = (text & 0xFFFF0000u) | B(kListRows + static_cast<U>(n));
        text = Key(SH_CALL(Text_DrawImmediate)(0x3A, static_cast<int>(y), At(text)));
        ++n;
    } while (n <= Sb(kListLast));
}

// original 0x596120 (PSX 0x8015AC54, gap47): Window_Kind1Frame's tail jump -
// the hand at x (the record's x >> 4, signed) + 4, the cursor's choice's y.
extern "C" void __cdecl Window_Kind1Cursor(void) {
    const int c = Sb(kListCursor);
    const unsigned char* const r = Cur();
    const U y = B(kListRows + static_cast<U>(c));
    const U hi = c < 0 ? 0xFFFF0000u : 0u;
    const U x = (hi | static_cast<std::uint16_t>(static_cast<short>(Word(r + 4)) >> 4)) + 4u;
    SH_CALL(Menu_DrawHand)(static_cast<int>(x), static_cast<int>(y), 0);
}

// original 0x596330: called by Window_Kind1Open - kind 1's layout. The
// choices' text is walked (0 ends a choice and counts a line, 1 counts a line,
// 4 5 7 8 9 take a byte more, 2 3 6 none, a byte from 0x80 a byte more), each
// choice's first line kept in 0x7DEE6F + n. The record (0x905B84): +0xA 0xDC
// wide, +0xB (last + 4) 2 + 13 lines high (bytes); x (+4) = (0xDC / 2 + 0x30)
// << 4, y (+6) = (+0xB / 2 - 13 lines / 2 + 0x78) << 4; +0x18 0x30, +0x1A 0x78
// - (last + 13 lines) / 2 (toward zero); +0x10, +0x12 0; +0x14 / +0x16 the
// width / height * 16 / 5. Then each choice's y = 13 line + 2 n + 0x7C - (last
// + 13 lines) / 2, bytes.
extern "C" void __cdecl Window_Kind1Layout(void) {
    unsigned char lines = 0;
    if (Sb(kListLast) >= 0) {
        const unsigned char* text = At(static_cast<U>(Long(At(kListText))));
        unsigned char n = 0;
        do {
            B(kListRows + n) = lines;
            for (bool more = true; more; ++text) {
                const unsigned char c = *text;
                switch (c) {
                case 0:
                    more = false;
                    ++lines;
                    break;
                case 1: ++lines; break;
                case 4:
                case 5:
                case 7:
                case 8:
                case 9: ++text; break;
                case 2:
                case 3:
                case 6: break;
                default:
                    if (c & 0x80) ++text;
                    break;
                }
            }
            ++n;
        } while (static_cast<int>(n) <= Sb(kListLast));
    }
    unsigned char* r = Cur();
    r[0xA] = 0xDC;
    r = Cur();
    r[0xB] = static_cast<unsigned char>((B(kListLast) + 4) * 2 + static_cast<unsigned char>(lines * 13));
    r = Cur();
    SetWord(r + 4, ((r[0xA] >> 1) + 0x30u) << 4);
    const U n13 = 13u * lines;
    r = Cur();
    SetWord(r + 6, ((r[0xB] >> 1) - (n13 >> 1) + 0x78u) << 4);
    SetWord(Cur() + 0x18, 0x30);
    {
        const int sum = Sb(kListLast) + static_cast<int>(n13);
        SetWord(Cur() + 0x1A, static_cast<U>(0x78 - sum / 2));
    }
    SetWord(Cur() + 0x10, 0);
    SetWord(Cur() + 0x12, 0);
    r = Cur();
    SetWord(r + 0x14, (static_cast<U>(r[0xA]) << 4) / 5);
    r = Cur();
    SetWord(r + 0x16, (static_cast<U>(r[0xB]) << 4) / 5);
    const int last = Sb(kListLast);
    const auto top = static_cast<unsigned char>(0x7C - static_cast<unsigned char>((last + static_cast<int>(n13)) / 2));
    if (last < 0) return;
    unsigned char add = 0, n = 0;
    do {
        const unsigned char v = B(kListRows + n);
        B(kListRows + n) = static_cast<unsigned char>(static_cast<unsigned char>(v * 13) + add + top);
        add = static_cast<unsigned char>(add + 2);
        ++n;
    } while (static_cast<int>(n) <= Sb(kListLast));
}

// ===========================================================================
// Window-record handler 1 (Field_RunTaskRecords' table slot 1)
// ===========================================================================

// original 0x596530 (in no list; Field_RunTaskRecords' stack table, the imm32
// at 0x59E245): jmp through Window_Handler1KindTable by the record's +2,
// unchecked (ours aborts past its 4).
extern "C" void __cdecl Window_Handler1Kinds(void) {
    Run("Window_Handler1Kinds (0x596530)", Window_Handler1KindTable, Window_Handler1KindTable_count, Cur()[2],
        "the window record's +2");
}

// original 0x596550: Window_Handler1KindTable[1] - its state (Win1_TitleStripStates
// by +3, a call, unchecked), the title box (x, y, 0x118 x 0x13, the window
// colour), and while the word +0x10 is not 0 the MessagePools string it indexes
// (s16) at (x + 7, y + 3), colour 0, count 0xFF.
extern "C" void __cdecl Win1_TitleStrip(void) {
    Run("Win1_TitleStrip (0x596550)", Win1_TitleStripStates, Win1_TitleStripStates_count, Cur()[3],
        "the window record's +3");
    const unsigned char colour = B(kStyle);
    const unsigned char* r = Cur();
    SH_CALL(Menu_DrawTitleBox)(Wx(r), Wy(r), 0x118, 0x13, colour);
    r = Cur();
    const U v = Word(r + 0x10);
    if (v == 0) return;
    const U at = kPools + 2u * static_cast<U>(static_cast<int>(static_cast<short>(v)));
    const unsigned char* const text = At(kPools + W(at));
    SH_CALL(Text_DrawAt)(static_cast<int>((Word(r + 4) + 7u) & 0xFFFF), static_cast<int>((Word(r + 6) + 3u) & 0xFFFF), 0, 0xFF,
                         text);
}

// original 0x5965D0: Window_Handler1KindTable[2] - its state (Win1_ButtonRowStates
// by +3), then Menu_DrawButtonRow(x, y, set +0xA, selected +0xB, 0).
extern "C" void __cdecl Win1_ButtonRow(void) {
    Run("Win1_ButtonRow (0x5965D0)", Win1_ButtonRowStates, Win1_ButtonRowStates_count, Cur()[3], "the window record's +3");
    const unsigned char* const r = Cur();
    SH_CALL(Menu_DrawButtonRow)(Wx(r), Wy(r), r[0xA], r[0xB], 0);
}

// original 0x596610: Window_Handler1KindTable[3] - its state (Win1_ShisuPanelStates
// by +3), then the panel (Win1_DrawShisuPanel) of the record.
extern "C" void __cdecl Win1_ShisuPanel(void) {
    Run("Win1_ShisuPanel (0x596610)", Win1_ShisuPanelStates, Win1_ShisuPanelStates_count, Cur()[3],
        "the window record's +3");
    SH_CALL(Win1_DrawShisuPanel)(Cur());
}

// original 0x596630: called by Win1_ShisuPanel - a 0x95 x 0xA0 box at (x + 3,
// y + 3); four rows, row i (y + 13 (i + 2)) an item row of category 0, id
// 0x66AE8C[i], count the byte i of the record's +0x20 list - colour 7 and dim
// where that byte is 0; the row at +0xA (the cursor) raised 2 over a colour-7
// dim shadow (none for a 0 byte); the title string 0x66AE90 at x + 6 (13 -
// its bytes), y + 7, count 0x10; the two piece lists 0x66AE74 / 0x66AE80; the
// frame's pieces.
extern "C" void __cdecl Win1_DrawShisuPanel(unsigned char* rec) {
    SH_CALL(Menu_DrawBox)(static_cast<int>((Word(rec + 4) + 3u) & 0xFFFF), static_cast<int>((Word(rec + 6) + 3u) & 0xFFFF), 0x95,
                          0xA0, 0, B(kStyle));
    for (unsigned char i = 0; i < 4; ++i) {
        const unsigned char held = At(static_cast<U>(Long(rec + 0x20)))[i];
        const unsigned colour = held != 0 ? 0 : 7;
        const unsigned dim = held != 0 ? 0 : 1;
        if (i != rec[0xA]) {
            Row(Wx(rec) + 7, (13u * (i + 2u) + Word(rec + 6)) & 0xFFFF, colour, 0, B(kShisuIds + i), held, dim);
            continue;
        }
        if (held != 0) Row(Wx(rec) + 7, (13u * (i + 2u) + Word(rec + 6)) & 0xFFFF, 7, 0, B(kShisuIds + i), held, 1);
        const unsigned char again = At(static_cast<U>(Long(rec + 0x20)))[i];
        Row(Wx(rec) + 7, ((13u * i + Word(rec + 6)) & 0xFFFF) + 0x18u, colour, 0, B(kShisuIds + i), again, dim);
    }
    {
        unsigned n = 0;
        while (B(kShisuTitle + n) != 0) ++n;
        const int x = static_cast<int>((6u * (13u - n) + Word(rec + 4)) & 0xFFFF);
        SH_CALL(Text_DrawAt)(x, static_cast<int>((Word(rec + 6) + 7u) & 0xFFFF), 0, 0x10, At(kShisuTitle));
    }
    SH_CALL(Menu_DrawPieces)(Wx(rec), Wy(rec), At(kShisuPiecesA), 1);
    SH_CALL(Menu_DrawPieces)(Wx(rec), Wy(rec), At(kShisuPiecesB), 1);
    for (unsigned i = 0; i < 10; ++i) Piece(Wx(rec) + static_cast<int>(8 * i + 0x28), Wy(rec), 1);
    for (unsigned i = 0; i < 17; ++i) Piece(Wx(rec), Wy(rec) + static_cast<int>(8 * i + 0x18), 4);
    Piece(Wx(rec) + 0x90, Wy(rec) + 0x18, 0x20);
    for (unsigned i = 0; i < 15; ++i) Piece(Wx(rec) + 0x90, Wy(rec) + static_cast<int>(8 * i + 0x20), 0x21);
    Piece(Wx(rec), Wy(rec) + 0xA0, 5);
    Piece(Wx(rec) + 0x90, Wy(rec) + 0x98, 0x22);
    Piece(Wx(rec) + 8, Wy(rec), 0x32);
    Piece(Wx(rec) + 0x90, Wy(rec), 0x34);
    for (unsigned i = 0; i < 17; ++i) Piece(Wx(rec) + static_cast<int>(8 * i + 8), Wy(rec) + 0xA0, 6);
}

// ===========================================================================
// Window-record handler 2 (Field_RunTaskRecords' table slot 2)
// ===========================================================================

// original 0x5968E0 (Field_RunTaskRecords' stack table, the imm32 at 0x59E250):
// jmp through Window_Handler2KindTable by the record's +2, unchecked (ours
// aborts past its 5).
extern "C" void __cdecl Window_Handler2Kinds(void) {
    Run("Window_Handler2Kinds (0x5968E0)", Window_Handler2KindTable, Window_Handler2KindTable_count, Cur()[2],
        "the window record's +2");
}

// original 0x596900: Window_Handler2KindTable[0] - its state (Win2_ItemListStates
// by +3), then the item list (Win2_DrawItemList) of the record.
extern "C" void __cdecl Win2_ItemList(void) {
    Run("Win2_ItemList (0x596900)", Win2_ItemListStates, Win2_ItemListStates_count, Cur()[3], "the window record's +3");
    SH_CALL(Win2_DrawItemList)(Cur());
}

// original 0x596920: a slide state (Win2_ItemListStates[1], Win2_EquipCompareStates[1],
// and two other handlers' tables, 0x66B2E8 and 0x66B570): x - 0x20 a frame to
// the bound of its `mov ecx, -170` (s16), held there with +3 0. DIV-0041
// widens the bound (widescreen.cpp kSlides); ours reads it from the operand.
extern "C" void __cdecl MenuSlide_LeftOff170(void) {
    const auto bound = static_cast<short>(Long(At(kLeftOff170Bound)));
    unsigned char* const moved = Cur();
    SetWord(moved + 4, Word(moved + 4) + 0xFFE0u);
    unsigned char* const r = Cur();
    if (static_cast<short>(Word(r + 4)) < bound) {
        SetWord(r + 4, static_cast<std::uint16_t>(bound));
        Cur()[3] = 0;
    }
}

// original 0x596950: Win2_ItemListStates[2]: x + 0x20 a frame to 0x50 (s16),
// held there with +3 0.
extern "C" void __cdecl MenuSlide_RightTo80(void) {
    unsigned char* const moved = Cur();
    SetWord(moved + 4, Word(moved + 4) + 0x20u);
    unsigned char* const r = Cur();
    if (static_cast<short>(Word(r + 4)) > 0x50) {
        SetWord(r + 4, 0x50);
        Cur()[3] = 0;
    }
}

// original 0x596980: Window_Handler2KindTable[2] - its state
// (Win2_ItemPanelStates by +3), then Menu_DrawItemPanel(the record).
extern "C" void __cdecl Win2_ItemPanel(void) {
    Run("Win2_ItemPanel (0x596980)", Win2_ItemPanelStates, Win2_ItemPanelStates_count, Cur()[3], "the window record's +3");
    SH_CALL(Menu_DrawItemPanel)(Cur());
}

// original 0x5969A0: Window_Handler2KindTable[3] - its state
// (Win2_EquipCompareStates by +3), then Menu_DrawEquipCompare(7, x, y, the
// record's +0x20, +0xD, the record).
extern "C" void __cdecl Win2_EquipCompare(void) {
    Run("Win2_EquipCompare (0x5969A0)", Win2_EquipCompareStates, Win2_EquipCompareStates_count, Cur()[3],
        "the window record's +3");
    unsigned char* const r = Cur();
    SH_CALL(Menu_DrawEquipCompare)(7, Wx(r), Wy(r), At(static_cast<U>(Long(r + 0x20))), r[0xD], r);
}

// original 0x5969E0: Window_Handler2KindTable[4] - its state
// (Win2_TitleBoxStates by +3), the title box (x, y, the word +0x12 wide, 0x13
// high, the window colour), and while the word +0x10 is not 0 the system
// message it names (Msg_SystemPtr) at (x + 7, y + 3), colour 0, count 0xFF.
extern "C" void __cdecl Win2_TitleBox(void) {
    Run("Win2_TitleBox (0x5969E0)", Win2_TitleBoxStates, Win2_TitleBoxStates_count, Cur()[3], "the window record's +3");
    const unsigned char colour = B(kStyle);
    const unsigned char* r = Cur();
    SH_CALL(Menu_DrawTitleBox)(Wx(r), Wy(r), static_cast<int>(Word(r + 0x12)), 0x13, colour);
    r = Cur();
    const U v = Word(r + 0x10);
    if (v == 0) return;
    const unsigned char* const text = SH_CALL(Msg_SystemPtr)(v);
    r = Cur();
    SH_CALL(Text_DrawAt)(static_cast<int>((Word(r + 4) + 7u) & 0xFFFF), static_cast<int>((Word(r + 6) + 3u) & 0xFFFF), 0, 0xFF,
                         text);
}

// original 0x596A60: Win2_TitleBoxStates[3]: y + 0x10 a frame to 0x28 (s16),
// held there with +3 0.
extern "C" void __cdecl MenuSlide_DownTo40(void) {
    unsigned char* const moved = Cur();
    SetWord(moved + 6, Word(moved + 6) + 0x10u);
    unsigned char* const r = Cur();
    if (static_cast<short>(Word(r + 6)) > 0x28) {
        SetWord(r + 6, 0x28);
        Cur()[3] = 0;
    }
}

// original 0x596A90: called by Win2_ItemList - the item list window, the
// shape of Menu_DrawItemList's (menu_windows.cpp) with nine rows: the box;
// Menu_ListScroll (top +0xB, its offset kept in the record pointer argument's
// own low byte as the original keeps it - ours a local -, moving, state +0x12);
// moving + 9 rows of category +0xA's ids and counts from the top: dim by
// Item_CanUse(+8, the shown member, category, id) unless +9 (then all dim),
// colour 7 dim / 0, 2 on the cursor +0xD; the count 1 for category 4; the
// cursor's and the marked (+0xC) row raised 2 over a colour-7 shadow (none when
// dim); the title and footer boxes; the category's title (0x66AF10) at x + 6 (13
// - Text_CharCount) - the call DIV-0059 re-aims; "%3d/%3d" of
// Inventory_CountUsed and the room (0x20 for category 4, else 0x80) in the
// 8-px font; the arrows by +0x10's bits 1 and 0, its high nibble counted down
// (or the word cleared); the frame's pieces; the scroll bar over 9 rows.
extern "C" void __cdecl Win2_DrawItemList(unsigned char* rec) {
    SH_CALL(Menu_DrawBox)(static_cast<int>((Word(rec + 4) + 3u) & 0xFFFF), static_cast<int>((Word(rec + 6) + 3u) & 0xFFFF), 0x99,
                          0x9A, rec[9], B(kStyle));
    unsigned char offset = static_cast<unsigned char>(Key(rec));
    unsigned char moving = 0;
    const unsigned char top = SH_CALL(Menu_ListScroll)(rec + 0xB, &offset, &moving, rec + 0x12);
    const unsigned rows = moving + 9u;
    const U category = rec[0xA];
    const unsigned char* ids = At(static_cast<U>(Long(At(kIdLists + 4 * category)))) + top;
    const unsigned char* counts = At(static_cast<U>(Long(At(kCountLists + 4 * category)))) + top;
    U y = static_cast<std::uint16_t>(static_cast<signed char>(offset) + Word(rec + 6)) + 0x1Au;
    for (unsigned char i = 0; i < rows; ++i, ++ids, ++counts, y += 0xD) {
        const unsigned char item = *ids;
        if (item == 0) continue;
        unsigned char dim = 1;
        if (rec[9] == 0) {
            const unsigned char who = B(kParty + static_cast<U>(static_cast<int>(Sb(kShown))));
            const unsigned char member = B(kEffectState + who);
            dim = SH_CALL(Item_CanUse)(rec[8], member, rec[0xA], item) ? 0 : 1;
        }
        unsigned char colour = dim ? 7 : 0;
        const U here = static_cast<U>(i) + rec[0xB];
        const U cursor = rec[0xD];
        if (here == cursor) colour = 2;
        const unsigned char cat = rec[0xA];
        const unsigned char count = cat == 4 ? 1 : *counts;
        if (here != cursor && here != rec[0xC]) {
            Row(Wx(rec) + 7, y, colour, cat, *ids, count, dim);
            continue;
        }
        if (colour != 7) Row(Wx(rec) + 7, y, 7, cat, *ids, count, 1);
        Row(Wx(rec) + 7, y - 2, colour, rec[0xA], *ids, count, dim);
    }
    SH_CALL(Menu_DrawBox)(static_cast<int>((Word(rec + 4) + 3u) & 0xFFFF), static_cast<int>((Word(rec + 6) + 3u) & 0xFFFF), 0x99,
                          0x14, rec[9], B(kStyle));
    {
        const unsigned char colour = B(kStyle);
        const unsigned char flags = rec[9];
        SH_CALL(Menu_DrawBox)(static_cast<int>((Word(rec + 4) + 3u) & 0xFFFF), static_cast<int>((Word(rec + 6) + 0x92u) & 0xFFFF),
                              0x99, 8, flags, colour);
    }
    {
        const U cat = rec[0xA];
        const unsigned grey = rec[9] ? 7 : 0;
        const int y7 = static_cast<int>((Word(rec + 6) + 7u) & 0xFFFF);
        const unsigned char* const title = At(static_cast<U>(Long(At(kListTitles + 4 * cat))));
        const unsigned n = SH_CALL(Text_CharCount)(title);
        const int x = static_cast<int>((6u * (13u - n) + Word(rec + 4)) & 0xFFFF);
        const U site = g_title_call;
        if (site == 0 || site == bof3::addr::Text_DrawAt) SH_CALL(Text_DrawAt)(x, y7, static_cast<int>(grey), 0x10, title);
        else reinterpret_cast<TitleFn>(static_cast<std::uintptr_t>(site))(x, y7, static_cast<int>(grey), 0x10, title);
    }
    {
        const unsigned char cat = rec[0xA];
        const U room = cat == 4 ? 0x20 : 0x80;
        const unsigned char used = SH_CALL(Inventory_CountUsed)(cat);
        SH_CALL(Crt_sprintf)(reinterpret_cast<char*>(At(kPrint)), reinterpret_cast<const char*>(At(kUsedFormat)),
                             static_cast<U>(used), room);
        const unsigned grey = rec[9] ? 7 : 0;
        SH_CALL(Text_DrawFont8)(static_cast<int>((Word(rec + 4) + 0x55u) & 0xFFFF), static_cast<int>((Word(rec + 6) + 0x92u) & 0xFFFF),
                                static_cast<int>(grey), At(kPrint));
    }
    SH_CALL(Menu_DrawPieces)(Wx(rec), Wy(rec), At(rec[0x10] & 2 ? kListPieces2 : kListPieces0), 1);
    SH_CALL(Menu_DrawPieces)(Wx(rec), Wy(rec), At(rec[0x10] & 1 ? kListPieces3 : kListPieces1), 1);
    {
        const U w = Word(rec + 0x10);
        if ((w & 0xF0) == 0) SetWord(rec + 0x10, 0);
        else SetWord(rec + 0x10, w - 0x10);
    }
    for (unsigned i = 0; i < 10; ++i) Piece(Wx(rec) + static_cast<int>(8 * i + 0x28), Wy(rec), 1);
    for (unsigned i = 0; i < 15; ++i) Piece(Wx(rec), Wy(rec) + static_cast<int>(8 * i + 0x18), 4);
    Piece(Wx(rec) + 0x90, Wy(rec) + 0x18, 0x16);
    for (unsigned i = 0; i < 12; ++i) Piece(Wx(rec) + 0x90, Wy(rec) + static_cast<int>(8 * i + 0x28), 0x17);
    Piece(Wx(rec) + 0x90, Wy(rec) + 0x88, 0x18);
    Piece(Wx(rec), Wy(rec) + 0x90, 0x19);
    for (unsigned i = 0; i < 9; ++i) Piece(Wx(rec) + static_cast<int>(8 * i + 8), Wy(rec) + 0x90, 0x1A);
    Piece(Wx(rec) + 0x50, Wy(rec) + 0x90, 0x1B);
    for (unsigned i = 0; i < 6; ++i) Piece(Wx(rec) + static_cast<int>(8 * i + 0x58), Wy(rec) + 0x90, 0x1C);
    Piece(Wx(rec) + 0x88, Wy(rec) + 0x90, 0x1D);
    {
        const U cat = rec[0xA];
        const U total = cat == 4 ? 0x20 : 0x80;
        SH_CALL(Menu_DrawScrollBar)(At(static_cast<U>(Long(At(kIdLists + 4 * cat)))), rec[0xB], Wx(rec) + 0x90, Wy(rec) + 0x18, 9,
                                    total, 0x74);
    }
}

namespace {
// DIV-0041's operand inside MenuSlide_LeftOff170: refused unless the byte
// before it is `mov ecx`'s B9 and the bound is the original's -170 or the
// widened one (as menu_lists.cpp's CheckSlideBound).
void CheckLeftOff170() {
    const int wide = -170 - static_cast<int>(Widescreen_Live());
    const int bound = Long(At(kLeftOff170Bound));
    if (At(kLeftOff170Bound - 1)[0] != 0xB9 || (bound != -170 && bound != wide))
        bof3::Fatal("rest_2f: 0x%X should be `mov ecx, -170` (or %d, DIV-0041), holds %02X then %d", kLeftOff170Bound - 1, wide,
                    At(kLeftOff170Bound - 1)[0], bound);
}
// DIV-0059's site inside Win2_DrawItemList: an E8 reaching Text_DrawAt, or the
// ListTitle_DrawAt BattleDraw_Inject put there (anything else is refused).
U ReadTitleCall() {
    const unsigned char* const site = At(kTitleCall);
    if (site[0] != 0xE8) bof3::Fatal("rest_2f: 0x%X is not a call (%02X)", kTitleCall, site[0]);
    const U target = kTitleCall + 5u + static_cast<U>(Long(site + 1));
    return target;
}
}  // namespace

void Rest2F_Inject() {
    CheckLeftOff170();
    if (bof3::WantsShadow("rest_2f")) rest_2f::SelfTest();
    // DIV-0059 survives the takeover: ours calls what the site calls
    g_title_call = ReadTitleCall();
    bof3::Log("rest_2f     Win2_DrawItemList's title call (0x%X) reaches 0x%X%s", kTitleCall, g_title_call,
              g_title_call == bof3::addr::Text_DrawAt ? " (Text_DrawAt)" : " (DIV-0059's ListTitle_DrawAt)");
    BOF3_INJECT(AbilityMenu_InitRecords);
    BOF3_INJECT(AbilityList_SortBy);
    BOF3_INJECT(AbilityList_Compact);
    BOF3_INJECT(AbilityList_SortByApDown);
    BOF3_INJECT(AbilityList_SortByApUp);
    BOF3_INJECT(AbilityMenu_InitRecord17);
    BOF3_INJECT(FieldMenu_FreeRecords13To18);
    BOF3_INJECT(Tactics_Run);
    BOF3_INJECT(Tactics_Open);
    BOF3_INJECT(Tactics_Top);
    BOF3_INJECT(Tactics_Formation);
    BOF3_INJECT(TacticsFormation_Enter);
    BOF3_INJECT(TacticsFormation_Pick);
    BOF3_INJECT(TacticsFormation_Leave);
    BOF3_INJECT(Tactics_Members);
    BOF3_INJECT(TacticsMembers_Enter);
    BOF3_INJECT(TacticsMembers_Pick);
    BOF3_INJECT(TacticsMembers_Leave);
    BOF3_INJECT(Tactics_Close);
    BOF3_INJECT(Tactics_OpenFormation);
    BOF3_INJECT(TacticsFormation_Build);
    BOF3_INJECT(TacticsMembers_Build);
    BOF3_INJECT(TacticsMembers_Swap);
    BOF3_INJECT(FieldMenu_FreeRecords12To18);
    BOF3_INJECT(FieldMenu_State8Run);
    BOF3_INJECT(ConfigMenu_Run);
    BOF3_INJECT(ConfigMenu_Open);
    BOF3_INJECT(ConfigMenu_Body);
    BOF3_INJECT(ConfigMenu_Close);
    BOF3_INJECT(Stat_AddClampedTo);
    BOF3_INJECT(AbilityList_CountSet);
    BOF3_INJECT(ItemTrade_TakeNeeds);
    BOF3_INJECT(Window_Kind1List);
    BOF3_INJECT(Window_Kind1Cursor);
    BOF3_INJECT(Window_Kind1Layout);
    BOF3_INJECT(Window_Handler1Kinds);
    BOF3_INJECT(Win1_TitleStrip);
    BOF3_INJECT(Win1_ButtonRow);
    BOF3_INJECT(Win1_ShisuPanel);
    BOF3_INJECT(Win1_DrawShisuPanel);
    BOF3_INJECT(Window_Handler2Kinds);
    BOF3_INJECT(Win2_ItemList);
    BOF3_INJECT(MenuSlide_LeftOff170);
    BOF3_INJECT(MenuSlide_RightTo80);
    BOF3_INJECT(Win2_ItemPanel);
    BOF3_INJECT(Win2_EquipCompare);
    BOF3_INJECT(Win2_TitleBox);
    BOF3_INJECT(MenuSlide_DownTo40);
    BOF3_INJECT(Win2_DrawItemList);
}

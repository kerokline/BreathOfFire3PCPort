// BOF3X_SHADOW=rest_2f: group R2F's 49 functions through the scenario harness's
// field mode (scenario_harness.h, used unchanged; docs/scenario_harness.md
// section 7), once at start-up. docs/rest_2f.md section 4.
// BOF3X_R2F_ONLY=<name> runs the clones whose name contains it (the controls).
//
// The clone table is tools/band_rows.py --group R2F --clones --harness scenario
// (2026-10-04) with the names given; every extent and call site agrees with
// the capstone read. The field menu's states are kMenu, the window-record
// handlers and their kinds kState (with 0x905B84 one of the 22 window
// records), the five that take arguments kCall.
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/rest_2f.h"
#include "game/rest_2f_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace rest_2f {
namespace {

namespace sh = scenario_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using sh::Shape;
using U = std::uint32_t;

// band_rows.py's call sites (2026-10-04), each checked against the capstone read.
constexpr sh::CallSite kCalls58EE50[] = {{0x1F, 0x591E50}, {0x3E, 0x58BD50}};
constexpr sh::CallSite kCalls58EEC0[] = {{0x7, 0x58EE50}, {0x24, 0x591E50}, {0x68, 0x58BD50}};
constexpr sh::CallSite kCalls58EF60[] = {{0x7, 0x58EE50}, {0x24, 0x591E50}, {0x68, 0x58BD50}};
constexpr sh::CallSite kCalls58F090[] = {{0xA9, 0x575690}, {0xB9, 0x587740}};
constexpr sh::CallSite kCalls58F170[] = {{0x7, 0x575690},   {0x75, 0x461EB0},  {0x87, 0x587740},  {0xB2, 0x58FD60},
                                         {0xC2, 0x58FEF0},  {0xCC, 0x587740},  {0xD6, 0x587740},  {0x112, 0x587740},
                                         {0x12A, 0x587740}, {0x134, 0x587740}};
constexpr sh::CallSite kCalls58F300[] = {{0x7, 0x575690}, {0x26, 0x531BB0}, {0x4E, 0x531BB0}};
constexpr sh::CallSite kCalls58F370[] = {{0x16, 0x575690},  {0x14A, 0x587740}, {0x194, 0x587740}, {0x1BE, 0x587740},
                                         {0x1DE, 0x58BD50}, {0x225, 0x587740}, {0x284, 0x587740}, {0x2EA, 0x461EB0},
                                         {0x362, 0x461EB0}, {0x3C7, 0x587740}};
constexpr sh::CallSite kCalls58F760[] = {{0x7, 0x575690}, {0x26, 0x531BB0}, {0x4E, 0x531BB0}};
constexpr sh::CallSite kCalls58F7E0[] = {{0x7, 0x575690}, {0x28, 0x531BB0}, {0x8E, 0x531BB0}};
constexpr sh::CallSite kCalls58F890[] = {{0xA, 0x575690},   {0xE1, 0x591940},  {0x103, 0x461EB0}, {0x10B, 0x531BB0},
                                         {0x1F2, 0x587740}, {0x21C, 0x587740}, {0x241, 0x590020}, {0x25B, 0x587740},
                                         {0x282, 0x587740}};
constexpr sh::CallSite kCalls58FB60[] = {{0x7, 0x575690}, {0x26, 0x531BB0}, {0x4E, 0x531BB0}};
constexpr sh::CallSite kCalls58FBD0[] = {{0x7, 0x575690}, {0x1E, 0x5902A0}, {0x2B, 0x531BB0}, {0x53, 0x531BB0}};
constexpr sh::CallSite kCalls58FC60[] = {{0x60, 0x531BB0}, {0xBA, 0x531BB0}, {0xCC, 0x575690}, {0xD4, 0x58FD60}};
constexpr sh::CallSite kCalls58FD60[] = {{0x5, 0x531BB0}};
constexpr sh::CallSite kCalls58FEF0[] = {{0x6, 0x531BB0}};
constexpr sh::CallSite kCalls590020[] = {{0x60, 0x58BD50}, {0xFF, 0x58BD50}, {0x1B8, 0x587740}, {0x1C4, 0x531BB0}, {0x209, 0x531BB0}};
constexpr sh::CallSite kCalls5902F0[] = {{0x6, 0x575690}};
constexpr sh::CallSite kCalls590340[] = {{0x0, 0x460CB0}};
constexpr sh::CallSite kCalls590350[] = {{0x8, 0x575690}, {0x15, 0x531BB0}, {0x48, 0x531BB0}};
constexpr sh::JumpTable kTables591AC0[] = {{0x47, 0x90, 4}};
constexpr sh::CallSite kCalls594D90[] = {{0x55, 0x591B60}};
constexpr sh::CallSite kCalls596090[] = {{0x1E, 0x5961C0}};
constexpr sh::CallSite kCalls596120[] = {{0x24, 0x5905D0}};
constexpr sh::JumpTable kTables596330[] = {{0x46, 0x1D8, 4}};
constexpr sh::CallSite kCalls596550[] = {{0x2F, 0x574AB0}, {0x73, 0x516B30}};
constexpr sh::CallSite kCalls5965D0[] = {{0x2B, 0x574890}};
constexpr sh::CallSite kCalls596610[] = {{0x19, 0x596630}};
constexpr sh::CallSite kCalls596630[] = {{0x2C, 0x57CF60},  {0xB3, 0x57DBF0},  {0xF1, 0x57DBF0},  {0x137, 0x516B30},
                                         {0x14D, 0x57D910}, {0x163, 0x57D910}, {0x187, 0x57D860}, {0x1B2, 0x57D860},
                                         {0x1D8, 0x57D860}, {0x201, 0x57D860}, {0x222, 0x57D860}, {0x23E, 0x57D860},
                                         {0x255, 0x57D860}, {0x26D, 0x57D860}, {0x296, 0x57D860}};
constexpr sh::CallSite kCalls596900[] = {{0x19, 0x596A90}};
constexpr sh::CallSite kCalls596980[] = {{0x19, 0x5763F0}};
constexpr sh::CallSite kCalls5969A0[] = {{0x2C, 0x574EC0}};
constexpr sh::CallSite kCalls5969E0[] = {{0x2F, 0x574AB0}, {0x47, 0x497740}, {0x6B, 0x516B30}};
constexpr sh::CallSite kCalls596A90[] = {
    {0x30, 0x57CF60},  {0x4B, 0x57DF00},  {0xD9, 0x57D9A0},  {0x176, 0x57DBF0}, {0x1A1, 0x57DBF0}, {0x205, 0x57CF60},
    {0x22E, 0x57CF60}, {0x265, 0x57D800}, {0x283, 0x516B30}, {0x29F, 0x591A80}, {0x2B7, 0x5B9380}, {0x2DE, 0x517090},
    {0x304, 0x57D910}, {0x32A, 0x57D910}, {0x365, 0x57D860}, {0x390, 0x57D860}, {0x3B6, 0x57D860}, {0x3DF, 0x57D860},
    {0x405, 0x57D860}, {0x41D, 0x57D860}, {0x445, 0x57D860}, {0x46A, 0x57D860}, {0x493, 0x57D860}, {0x4B9, 0x57D860},
    {0x4F9, 0x57DD10}};

#define SH_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define G_FN(name) reinterpret_cast<const void*>(&::name)
#define G_CLONE(name, base, size, calls, shape, ret) #name, base, size, calls, SH_N(calls), nullptr, 0, nullptr, 0, G_FN(name), ret, false, shape
#define G_TABLE(name, base, size, tables, shape, ret) #name, base, size, nullptr, 0, nullptr, 0, tables, SH_N(tables), G_FN(name), ret, false, shape
#define G_LEAF(name, base, size, shape, ret) #name, base, size, nullptr, 0, nullptr, 0, nullptr, 0, G_FN(name), ret, false, shape
constexpr Shape kMe = Shape::kMenu;
constexpr Shape kSt = Shape::kState;
constexpr Shape kCa = Shape::kCall;
const sh::Clone kClones[] = {
    {G_LEAF(AbilityMenu_InitRecords, 0x58ED40, 0xFC, kMe, 0)},
    {G_LEAF(AbilityList_SortBy, 0x58EE40, 0x10, kCa, 0)},
    {G_CLONE(AbilityList_Compact, 0x58EE50, 0x70, kCalls58EE50, kMe, 0)},
    {G_CLONE(AbilityList_SortByApDown, 0x58EEC0, 0x9A, kCalls58EEC0, kMe, 0)},
    {G_CLONE(AbilityList_SortByApUp, 0x58EF60, 0x9A, kCalls58EF60, kMe, 0)},
    {G_LEAF(AbilityMenu_InitRecord17, 0x58F000, 0x45, kMe, 0)},
    {G_LEAF(FieldMenu_FreeRecords13To18, 0x58F050, 0x21, kMe, 0)},
    {G_LEAF(Tactics_Run, 0x58F080, 0xE, kMe, 0)},
    {G_CLONE(Tactics_Open, 0x58F090, 0xDC, kCalls58F090, kMe, 0)},
    {G_CLONE(Tactics_Top, 0x58F170, 0x177, kCalls58F170, kMe, 0)},
    {G_LEAF(Tactics_Formation, 0x58F2F0, 0xE, kMe, 0)},
    {G_CLONE(TacticsFormation_Enter, 0x58F300, 0x69, kCalls58F300, kMe, 0)},
    {G_CLONE(TacticsFormation_Pick, 0x58F370, 0x3EF, kCalls58F370, kMe, 0)},
    {G_CLONE(TacticsFormation_Leave, 0x58F760, 0x69, kCalls58F760, kMe, 0)},
    {G_LEAF(Tactics_Members, 0x58F7D0, 0xE, kMe, 0)},
    {G_CLONE(TacticsMembers_Enter, 0x58F7E0, 0xA7, kCalls58F7E0, kMe, 0)},
    {G_CLONE(TacticsMembers_Pick, 0x58F890, 0x2D0, kCalls58F890, kMe, 0)},
    {G_CLONE(TacticsMembers_Leave, 0x58FB60, 0x69, kCalls58FB60, kMe, 0)},
    {G_CLONE(Tactics_Close, 0x58FBD0, 0x8D, kCalls58FBD0, kMe, 0)},
    {G_CLONE(Tactics_OpenFormation, 0x58FC60, 0xF1, kCalls58FC60, kMe, 0)},
    {G_CLONE(TacticsFormation_Build, 0x58FD60, 0x185, kCalls58FD60, kMe, 0)},
    {G_CLONE(TacticsMembers_Build, 0x58FEF0, 0x12F, kCalls58FEF0, kMe, 0)},
    {G_CLONE(TacticsMembers_Swap, 0x590020, 0x273, kCalls590020, kMe, 0xFFu)},
    {G_LEAF(FieldMenu_FreeRecords12To18, 0x5902A0, 0x26, kMe, 0)},
    {G_LEAF(FieldMenu_State8Run, 0x5902D0, 0xE, kMe, 0)},
    {G_LEAF(ConfigMenu_Run, 0x5902E0, 0xE, kMe, 0)},
    {G_CLONE(ConfigMenu_Open, 0x5902F0, 0x41, kCalls5902F0, kMe, 0)},
    {G_CLONE(ConfigMenu_Body, 0x590340, 0x5, kCalls590340, kMe, 0)},
    {G_CLONE(ConfigMenu_Close, 0x590350, 0x95, kCalls590350, kMe, 0)},
    {G_LEAF(Stat_AddClampedTo, 0x590E80, 0x54, kCa, 0xFFFFu)},
    {G_TABLE(AbilityList_CountSet, 0x591AC0, 0xA0, kTables591AC0, kCa, 0xFFu)},
    {G_CLONE(ItemTrade_TakeNeeds, 0x594D90, 0x65, kCalls594D90, kSt, 0)},
    {G_CLONE(Window_Kind1List, 0x596090, 0x34, kCalls596090, kSt, 0)},
    {G_CLONE(Window_Kind1Cursor, 0x596120, 0x2D, kCalls596120, kSt, 0)},
    {G_TABLE(Window_Kind1Layout, 0x596330, 0x1F2, kTables596330, kSt, 0)},
    {G_LEAF(Window_Handler1Kinds, 0x596530, 0x12, kSt, 0)},
    {G_CLONE(Win1_TitleStrip, 0x596550, 0x7C, kCalls596550, kSt, 0)},
    {G_CLONE(Win1_ButtonRow, 0x5965D0, 0x34, kCalls5965D0, kSt, 0)},
    {G_CLONE(Win1_ShisuPanel, 0x596610, 0x20, kCalls596610, kSt, 0)},
    {G_CLONE(Win1_DrawShisuPanel, 0x596630, 0x2AA, kCalls596630, kCa, 0)},
    {G_LEAF(Window_Handler2Kinds, 0x5968E0, 0x12, kSt, 0)},
    {G_CLONE(Win2_ItemList, 0x596900, 0x20, kCalls596900, kSt, 0)},
    {G_LEAF(MenuSlide_LeftOff170, 0x596920, 0x28, kSt, 0)},
    {G_LEAF(MenuSlide_RightTo80, 0x596950, 0x28, kSt, 0)},
    {G_CLONE(Win2_ItemPanel, 0x596980, 0x20, kCalls596980, kSt, 0)},
    {G_CLONE(Win2_EquipCompare, 0x5969A0, 0x35, kCalls5969A0, kSt, 0)},
    {G_CLONE(Win2_TitleBox, 0x5969E0, 0x74, kCalls5969E0, kSt, 0)},
    {G_LEAF(MenuSlide_DownTo40, 0x596A60, 0x28, kSt, 0)},
    {G_CLONE(Win2_DrawItemList, 0x596A90, 0x508, kCalls596A90, kCa, 0)},
};
#undef G_LEAF
#undef G_TABLE
#undef G_CLONE
#undef G_FN
#undef SH_N

enum : unsigned {
    kInitRecords, kSortBy, kCompact, kSortDown, kSortUp, kInitRecord17, kFree13, kTacticsRun, kOpen, kTop, kFormation,
    kFormEnter, kFormPick, kFormLeave, kMembers, kMemEnter, kMemPick, kMemLeave, kClose, kOpenFormation, kFormBuild,
    kMemBuild, kSwapK, kFree12, kState8, kConfigRun, kConfigOpen, kConfigBody, kConfigClose, kStatAdd, kCountSet,
    kTakeNeeds, kKind1List, kKind1Cursor, kKind1Layout, kHandler1, kTitleStrip, kButtonRow, kShisu, kDrawShisu,
    kHandler2, kItemList, kLeftOff, kRight80, kItemPanel, kEquip, kTitleBox, kDown40, kDrawItemList, kCount
};
static_assert(kCount == sizeof kClones / sizeof kClones[0], "one enum entry a clone, in order");

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
template <typename... T> U PickOf(T... v) {
    const U values[] = {static_cast<U>(v)...};
    return sh::Pick(values, sizeof...(v));
}
unsigned char* Mem(U a) { return sh::Mem(a); }
unsigned char& B(U a) { return sh::Mem(a)[0]; }

// The cells (rest_2f.cpp's).
constexpr U kStep = 0x929F01, kSub = 0x929F02, kTimer = 0x929F04, kCol = 0x929F08, kRow = 0x929F09, kHeldCol = 0x929F0A,
            kHeldRow = 0x929F0D, kDirect = 0x929F10, kShown = 0x929F06;
constexpr U kGrid = 0x6BDFBC, kSetCol = 0x6BDFC5, kPickCol = 0x6BDFC6, kSetRow = 0x6BDFC7, kPickRow = 0x6BDFC8,
            kCurCol = 0x6BDFC9, kCurRow = 0x6BDFCA, kTopChoice = 0x6BDFCB, kReserve = 0x6BDFD4, kRefused = 0x6BDFD7;
constexpr U kFormationSet = 0x904060, kParty = 0x904062, kCharacters = 0x903A70, kPressed = 0x7E1BEC, kConfirm = 0x90358E,
            kCancel = 0x903590, kCurrent = 0x905B84, kListText = 0x7DEE50, kListLast = 0x7DEE66, kListCursor = 0x7DEE67,
            kWindows = 0x803160;
U Rec(unsigned n, unsigned at) { return kWindows + 0x24 * n + at; }

// The kind-1 choices' text (a region of its own; a tail of zeros after it that
// no one writes, so a walk past the region still ends).
unsigned char g_text[0x200];
// Win1_DrawShisuPanel's +0x20 list.
unsigned char* ShisuList() { return sh::Scratch(1); }

// --- the stand-ins' effects (Noise() and the state only: both passes the same) -----

// Text_DrawImmediate answers one past the NUL that ended its text: the text a
// few bytes on (the next choice's start).
U ImmediateEffect(const U* a, U) { return a[2] + 1 + sh::Noise() % 6; }
// Crt_sprintf (re-listed with its fourth word, the room, which the standard
// row does not log): up to seven characters and a NUL where the destination is
// a region, as the standard row's FxSprintf writes them.
U SprintfEffect(const U* a, U answer) {
    auto* const dst = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a[0]));
    if (!sh::InRegions(dst, 8)) return answer;
    const unsigned n = sh::Noise() % 8;
    for (unsigned i = 0; i < n; ++i) dst[i] = static_cast<unsigned char>('0' + sh::Noise() % 43);
    dst[n] = 0;
    return n;
}
// TacticsFormation_Build / TacticsMembers_Build: their callers read the cursor
// and the pick after them; the stand-ins move them as the real ones set them.
U FormBuildEffect(const U*, U answer) {
    B(kCurCol) = 0;
    B(kCurRow) = 0;
    B(kPickCol) = 0x7F;
    B(kPickRow) = 0x7F;
    return answer;
}

#define G_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
constexpr sh::Answer kG = sh::Answer::kGarbage;
constexpr U kAll = 0xFFFFFFFFu;
const sh::Callee kCallees[] = {
    // the group's own, called directly (E8) by the group's
    {G_OURS(AbilityList_Compact), 0, {}, kG, 0, 0},
    {G_OURS(TacticsFormation_Build), 0, {}, kG, 0, 0, {}, &FormBuildEffect},
    {G_OURS(TacticsMembers_Build), 0, {}, kG, 0, 0},
    {G_OURS(TacticsMembers_Swap), 0, {}, sh::Answer::kByte, 0xFF, 0x07},   // al 0xFF done, or a member id
    {G_OURS(FieldMenu_FreeRecords12To18), 0, {}, kG, 0, 0},
    {G_OURS(Win1_DrawShisuPanel), 1, {kAll}, kG, 0, 0},
    {G_OURS(Win2_DrawItemList), 1, {kAll}, kG, 0, 0},
    // ours, which the standard set lacks, with the width each reads
    {G_OURS(Text_DrawImmediate), 3, {0xFFFF, 0xFFFF, kAll}, kG, 0, 0, {}, &ImmediateEffect},   // x, y reach Text_DrawAt's shorts
    {G_OURS(Menu_DrawButtonRow), 5, {0xFFFF, 0xFFFF, 0xFF, 0xFF, 0}, kG, 0, 0},              // menu_windows.cpp: set and sel bytes
    {G_OURS(Menu_DrawItemPanel), 1, {kAll}, kG, 0, 0},
    {G_OURS(Menu_DrawEquipCompare), 6, {0xFF, 0xFFFF, 0xFFFF, kAll, 0xFF, kAll}, kG, 0, 0},   // field_o.cpp: no_preview a byte
    {G_OURS(Inventory_CountUsed), 1, {0xFF}, kG, 0, 0},                                       // char_stats.cpp: category & 0xFF
    // Capcom's, re-listed: the room (the fourth word) logged
    {"Crt_sprintf", 0x5B9380, 0x5B9380, 4, {kAll, kAll, kAll, kAll}, kG, 0, 0, {0, 16, 0, 0}, &SprintfEffect},
    // nobody's yet (R4F): the Config screen's machine
    {"0x460CB0", kConfigMachine, kConfigMachine, 0, {}, kG, 0, 0},
};
#undef G_OURS

// The tables the dispatchers read in place, swapped for recorders on both sides
// (TacticsMenu_Steps as its first six: its seventh is TacticsFormation_Steps[0],
// swapped with that table; docs/rest_2f.md section 1).
const sh::DataTable kTables[] = {
    {Key(AbilityList_SortModes), AbilityList_SortModes_count},
    {Key(TacticsMenu_Steps), 6},
    {Key(TacticsFormation_Steps), TacticsFormation_Steps_count},
    {Key(TacticsMembers_Steps), TacticsMembers_Steps_count},
    {Key(FieldMenu_State8Steps), FieldMenu_State8Steps_count},
    {Key(ConfigMenu_Steps), ConfigMenu_Steps_count},
    {Key(Window_Handler1KindTable), Window_Handler1KindTable_count},
    {Key(Win1_TitleStripStates), Win1_TitleStripStates_count},
    {Key(Win1_ButtonRowStates), Win1_ButtonRowStates_count},
    {Key(Win1_ShisuPanelStates), Win1_ShisuPanelStates_count},
    {Key(Window_Handler2KindTable), Window_Handler2KindTable_count},
    {Key(Win2_ItemListStates), Win2_ItemListStates_count},
    {Key(Win2_ItemPanelStates), Win2_ItemPanelStates_count},
    {Key(Win2_EquipCompareStates), Win2_EquipCompareStates_count},
    {Key(Win2_TitleBoxStates), Win2_TitleBoxStates_count},
};

// Beyond field mode's standard regions.
const sh::Region kRegions[] = {
    {kWindows, 22 * 0x24},       // WindowRecords
    {0x7DEE20, 0x60},            // the message cells: kind 1's text pointer, count, cursor, rows
    {0x6BDFB0, 0x30},            // Tactics' cells
    {0x903A94, 0x4FC},           // CharacterRecords past the style region, to Cond_Flags
    {0x904160, 0x400},           // the inventory's id and count lists
    {0x939870, 0x70},            // the members screen's portrait cells (rows -1..7)
    {0x903584, 0x10},            // Field_MenuButton, the confirm and cancel words
    {0x66C7E8, 4},               // Game_Mode, Game_Step
    {0x6BE080, 0x10},            // the trade's cells
    {0, 0x100},                  // g_text (set at Run)
};

// --- the seed ---------------------------------------------------------------------------

// The table lengths (each dispatcher's index drawn below it).
unsigned Entries(unsigned k) {
    switch (k) {
    case kTacticsRun: return 7;
    case kFormation: return TacticsFormation_Steps_count;
    case kMembers: return TacticsMembers_Steps_count;
    case kState8: return FieldMenu_State8Steps_count;
    case kConfigRun: return ConfigMenu_Steps_count;
    default: return 0;
    }
}
unsigned KindEntries(unsigned k) {
    switch (k) {
    case kHandler1: return Window_Handler1KindTable_count;
    case kHandler2: return Window_Handler2KindTable_count;
    default: return 0;
    }
}
unsigned StateEntries(unsigned k) {
    switch (k) {
    case kTitleStrip: return Win1_TitleStripStates_count;
    case kButtonRow: return Win1_ButtonRowStates_count;
    case kShisu: return Win1_ShisuPanelStates_count;
    case kItemList: return Win2_ItemListStates_count;
    case kItemPanel: return Win2_ItemPanelStates_count;
    case kEquip: return Win2_EquipCompareStates_count;
    case kTitleBox: return Win2_TitleBoxStates_count;
    default: return 0;
    }
}

// A window record's bytes, at their boundaries.
void SeedRecord(unsigned char* r) {
    r[0] = static_cast<unsigned char>(PickOf(0, 1, 1, sh::Next()));
    r[2] = static_cast<unsigned char>(PickOf(0, 1, 2, 3, 4, 5, 0xB, 0xB, 0xC, sh::Next()));   // the kind tables, and records 12..17 of kind 0xB
    r[3] = static_cast<unsigned char>(PickOf(0, 1, 2, 2, 4, sh::Next()));
    const U x = PickOf(0x50, 0x4F, 0x51, 0x30, 0x31, 0x2F, 0x70, 0xFF56, 0xFF57, 0xFF55, 0xFF76, 0xFF36, 0x11, 0x140, 0x7FF0, 0x8010,
                       sh::Next());
    SetWord(r + 4, x);
    SetWord(r + 6, PickOf(0x28, 0x27, 0x29, 0x18, 0x38, 0x10, 0xFFEC, 0x7FF8, 0x8008, sh::Next()));
    r[8] = static_cast<unsigned char>(PickOf(0, 1, 2, 3, 4, sh::Next()));
    r[9] = static_cast<unsigned char>(PickOf(0, 0, 1, sh::Next()));
    r[0xA] = static_cast<unsigned char>(sh::Next() % 5);
    r[0xB] = static_cast<unsigned char>(PickOf(0, 0, 1, 2, 0x70, 0x76, sh::Next() % 0x80));
    r[0xC] = static_cast<unsigned char>(PickOf(r[0xB], r[0xB] + 1u, r[0xB] + 8u, 0xFF, sh::Next()));
    r[0xD] = static_cast<unsigned char>(PickOf(r[0xB], r[0xB] + 2u, r[0xB] + 8u, r[0xB] + 9u, sh::Next()));
    SetWord(r + 0x10, PickOf(0, 0, 1, 2, 3, 0x10, 0x13, 0x22, 0xF3, 0x103, 0x0F, 0x50, sh::Next()));
    SetWord(r + 0x12, PickOf(0x48, 0x60, 0x100, sh::Next()));
    SetLong(r + 0x20, static_cast<std::int32_t>(Key(ShisuList() + sh::Next() % 0x30)));
}

// The kind-1 text: choices of control codes, glyph bytes and ends.
void SeedText() {
    for (unsigned i = 0; i < 0x100; ++i) {
        const U r = sh::Next() % 16;
        g_text[i] = static_cast<unsigned char>(r < 4 ? 0 : r < 6 ? 1 : r < 10 ? sh::Next() % 10 : r < 12 ? 0x80 | sh::Next() : sh::Next());
    }
    std::memset(g_text + 0x100, 0, 0x100);
}

// The formation grid with the cursor's cell open (the loops wrap over a column
// and a row until an open cell: the cursor's own ends them).
void SeedGrid() {
    for (unsigned i = 0; i < 9; ++i) B(kGrid + i) = static_cast<unsigned char>(PickOf(0, 1, 1, sh::Next()));
    const unsigned col = sh::Next() % 3, row = sh::Next() % 3;
    B(kCurCol) = static_cast<unsigned char>(col);
    B(kCurRow) = static_cast<unsigned char>(row);
    B(kGrid + 3 * row + col) = static_cast<unsigned char>(PickOf(1, 1, 2, 0x80));
    const bool picked = sh::Half();
    B(kPickCol) = static_cast<unsigned char>(picked ? PickOf(0, 0, 1, 2) : 0x7F);
    B(kPickRow) = static_cast<unsigned char>(picked ? sh::Next() % 3 : PickOf(0x7F, 0x7F, sh::Next() % 3));
    B(kSetCol) = static_cast<unsigned char>(PickOf(1, 2, 1, 2, 0, 3));
    B(kSetRow) = static_cast<unsigned char>(sh::Next() % 3);
}

// Stat_AddClampedTo's word, cap and delta at their boundaries.
U g_cap, g_delta;
unsigned g_arg_rec;

void Seed(unsigned k) {
    // the window records, the current one, the message cells
    for (unsigned n = 0; n < 22; ++n) SeedRecord(Mem(Rec(n, 0)));
    const unsigned current = sh::Next() % 22;
    unsigned char* const cur = Mem(Rec(current, 0));
    sh::SetPointer(kCurrent, cur);
    if (const unsigned e = KindEntries(k)) cur[2] = static_cast<unsigned char>(sh::Next() % e);
    if (const unsigned e = StateEntries(k)) cur[3] = static_cast<unsigned char>(sh::Next() % e);
    SeedText();
    sh::SetPointer(kListText, g_text + sh::Next() % 0x40);
    B(kListLast) = static_cast<unsigned char>(PickOf(0xFF, 0, 1, 2, 3, 4, 5, 6, 0x80));
    B(kListCursor) = static_cast<unsigned char>(PickOf(0, 1, 2, 3, 0xFF, sh::Next() % 8));
    // the menu block
    B(kStep) = static_cast<unsigned char>(PickOf(0, 1, 2, 3, 4, 5, 6, 0xFF, sh::Next()));
    B(kSub) = static_cast<unsigned char>(PickOf(0, 1, 2, 0xFF, sh::Next()));
    B(kTimer) = static_cast<unsigned char>(PickOf(1, 1, 2, 0, 5, sh::Next()));
    if (const unsigned e = Entries(k)) B(k == kFormation || k == kMembers ? kSub : kStep) = static_cast<unsigned char>(sh::Next() % e);
    B(kShown) = static_cast<unsigned char>(PickOf(0, 1, 2, 0xFF, 3));
    B(kCol) = static_cast<unsigned char>(PickOf(0, 1, 0, 1, 2, 0xFF));
    B(kRow) = static_cast<unsigned char>(PickOf(0, 1, 2, 3, 4, 0xFF, 5, 7));
    const bool held = sh::Half();
    B(kHeldCol) = static_cast<unsigned char>(held ? PickOf(0, 1, B(kCol)) : 0x7F);
    B(kHeldRow) = static_cast<unsigned char>(held ? PickOf(0, 1, 2, 3, B(kRow)) : PickOf(0x7F, 0x7F, 1));
    if (k == kSwapK && B(kHeldCol) == 0x7F) B(kHeldCol) = static_cast<unsigned char>(sh::Next() & 1);
    if (k == kSwapK && B(kHeldRow) == 0x7F) B(kHeldRow) = static_cast<unsigned char>(sh::Next() % 4);
    B(kDirect) = static_cast<unsigned char>(PickOf(0, 0, 1, sh::Next()));
    // Tactics' cells
    SeedGrid();
    B(kTopChoice) = static_cast<unsigned char>(PickOf(0, 1, 0, 1, sh::Next()));
    for (unsigned i = 0; i < 5; ++i) B(kReserve + i) = static_cast<unsigned char>(PickOf(0, 1, 2, 3, 4, 5, 6, 7, sh::Next() % 8));
    B(kRefused) = static_cast<unsigned char>(PickOf(0xFF, 0xFF, 0, 1, 5, 7));
    B(Rec(12, 0xA)) = static_cast<unsigned char>(PickOf(0, 1, 2, 3, 5));
    // the party and the characters: ids 0..7, the flags +0xB at bits 0 and 1
    for (unsigned i = 0; i < 6; ++i) B(kParty + i) = static_cast<unsigned char>(PickOf(0, 1, 2, 3, 4, 5, 6, 7, sh::Next() % 8));
    for (unsigned m = 0; m < 8; ++m) {
        unsigned char* const c = Mem(kCharacters + 0xA4 * m);
        c[9] = static_cast<unsigned char>(PickOf(m, m, m, sh::Next() % 8));
        c[0xB] = static_cast<unsigned char>(PickOf(0, 1, 1, 3, 2, sh::Next()));
    }
    B(kFormationSet) = static_cast<unsigned char>(PickOf(0, 1, 2, 3, 4, 5, sh::Next()));
    // the buttons
    SetWord(Mem(kConfirm), PickOf(0x20, 0x20, 0x4000, sh::Next()));
    SetWord(Mem(kCancel), PickOf(0x40, 0x40, 0x1000, sh::Next()));
    SetWord(Mem(kPressed), PickOf(0, 0, 0x20, 0x40, 0x60, 0x1000, 0x2000, 0x4000, 0x8000, 0x5000, 0xA000, 0x1020, sh::Next()));
    // the formation build's limits by the members' bits
    B(0x904061) = static_cast<unsigned char>(PickOf(0x3F, 0x15, 0x2A, 0, sh::Next()));
    // the trade's s8 cells
    B(0x6BE08C) = static_cast<unsigned char>(PickOf(0, 1, 2, 9, 0x10, 0x11, 0xFF, sh::Next()));
    B(0x6BE08E) = static_cast<unsigned char>(PickOf(1, 2, 0x7F, 0x80, 0xFF, sh::Next()));
    // Stat_AddClampedTo: the word at the cap or 0 or one from either
    g_cap = PickOf(999, 0x7FFF, 0xFFFF, 0x8000, 1, 0, sh::Next()) | (sh::Half() ? sh::Next() & 0xFFFF0000u : 0);
    g_delta = PickOf(1, 0xFFFF, 0, 0x7FFF, 0x8000, 0x10, 0xFFF0, 2, 0xFFFE, sh::Next() & 0xFFFF) | (sh::Next() & 0xFFFF0000u);
    const U c = g_cap & 0xFFFF;
    SetWord(sh::Scratch(0), PickOf(c, c - 1, c + 1, 0, 1, 0xFFFF, c - (g_delta & 0xFFFF), sh::Next()));
    // the record the two draws are handed: its category 0..4 (record 12's +0xA
    // is the reserve's count above, drawn again here inside the categories)
    g_arg_rec = sh::Next() % 22;
    B(Rec(g_arg_rec, 0xA)) = static_cast<unsigned char>(sh::Next() % 5);
}

// The arguments of the five kCall functions, after the seed.
void Args(unsigned k, U* a) {
    switch (k) {
    case kSortBy: a[0] = (a[0] & 0xFFFFFF00u) | (sh::Next() % AbilityList_SortModes_count); break;
    case kStatAdd:
        a[0] = Key(sh::Scratch(0));
        a[1] = g_cap;
        a[2] = g_delta;
        break;
    case kCountSet:
        a[0] = (a[0] & 0xFFFFFF00u) | PickOf(0, 1, 2, 3, 4, 5, 7, 15);
        a[1] = (a[1] & 0xFFFFFF00u) | PickOf(0, 1, 2, 3, 4, 0xFF);
        a[2] = PickOf(0, 0x100, 1, a[2]);
        break;
    case kDrawShisu:
    case kDrawItemList: a[0] = Key(Mem(Rec(g_arg_rec, 0))); break;
    default: break;
    }
}

// --- the disturbance: a cell these read again after a call ------------------------------
void Disturb(U h) {
    const auto b = static_cast<unsigned char>(h >> 24);
    const U v = h >> 8;
    unsigned char* const cur = sh::Pointer(kCurrent);
    const bool rec = sh::InRegions(cur, 0x24);
    switch (h % 15) {
    case 0: B(kStep) = static_cast<unsigned char>(b % 7); break;
    case 1: B(kSub) = static_cast<unsigned char>(b % 3); break;
    case 2: B(kTopChoice) = static_cast<unsigned char>(b & 1); break;
    case 3: B(kTimer) = static_cast<unsigned char>(b % 4); break;
    case 4: if (rec) SetWord(cur + (b & 1 ? 4 : 6), v & 0x1FF); break;
    // a category stays 0..4, and a 4 stays a 4: Win2_DrawItemList takes its
    // count list (null for category 4) before the rows and re-reads the
    // category in them (both sides fault on a 4 turned 3; docs/rest_2f.md section 7)
    case 5: if (rec && cur[0xA] != 4) cur[0xA] = static_cast<unsigned char>(b % 4); break;
    case 6: if (rec) cur[0xB] = static_cast<unsigned char>(b % 0x80); break;
    case 7: if (rec) SetWord(cur + 0x10, v & 0x1F3); break;
    case 8: sh::SetPointer(kCurrent, Mem(Rec(b % 22, 0))); break;
    case 9: B(kListLast) = static_cast<unsigned char>(b % 7); break;
    case 10: B(kParty + b % 6) = static_cast<unsigned char>(v % 8); break;
    case 11: if (B(Rec(12, 0xA)) != 4) B(Rec(12, 0xA)) = static_cast<unsigned char>(b % 4); break;   // record 12 may be the list's
    case 12: B(b & 1 ? kRow : kHeldRow) = static_cast<unsigned char>(v % 5); break;
    case 13: if (rec) cur[9] = static_cast<unsigned char>(b & 1); break;
    // the category of the record the two draws are handed (a 4 stays a 4)
    case 14: if (B(Rec(g_arg_rec, 0xA)) != 4) B(Rec(g_arg_rec, 0xA)) = static_cast<unsigned char>(b % 4); break;
    default: break;
    }
}

}  // namespace

void SelfTest() {
    // BOF3X_R2F_ONLY: the clones whose name contains it (a control's run)
    static sh::Clone chosen[kCount];
    static unsigned index[kCount];
    const char* const only = std::getenv("BOF3X_R2F_ONLY");
    // DIV-0059: under a Latin overlay BattleDraw_Inject has re-aimed
    // Win2_DrawItemList's title call already, and its copy cannot be made (the
    // site no longer reaches Text_DrawAt); it is left out, and said so.
    const U title = kTitleCall + 5u + static_cast<U>(Long(Mem(kTitleCall + 1)));
    const bool title_plain = Mem(kTitleCall)[0] == 0xE8 && title == bof3::addr::Text_DrawAt;
    if (!title_plain)
        bof3::Log("shadow      rest_2f: Win2_DrawItemList left out - its title call 0x%X reaches 0x%X (DIV-0059), not "
                  "Text_DrawAt; run without BOF3X_LANG",
                  kTitleCall, title);
    unsigned n = 0;
    for (unsigned k = 0; k < kCount; ++k) {
        if (k == kDrawItemList && !title_plain) continue;
        if (!only || !*only || std::strstr(kClones[k].name, only)) {
            index[n] = k;
            chosen[n++] = kClones[k];
        }
    }
    if (n == 0) bof3::Fatal("rest_2f: BOF3X_R2F_ONLY=%s names no clone", only);
    static unsigned* s_index = index;
    static sh::Region regions[sizeof kRegions / sizeof kRegions[0]];
    std::memcpy(regions, kRegions, sizeof kRegions);
    regions[sizeof kRegions / sizeof kRegions[0] - 1].at = Key(g_text);
    sh::Group g = {"rest_2f", chosen, n, kCallees, sizeof kCallees / sizeof kCallees[0], kTables,
                   sizeof kTables / sizeof kTables[0], regions, sizeof regions / sizeof regions[0],
                   [](unsigned k) { Seed(s_index[k]); }, &Disturb, 6000};
    g.args = [](unsigned k, U* a) { Args(s_index[k], a); };
    g.field = true;
    g.menu_span = 3;
    sh::Run(g);
}

}  // namespace rest_2f

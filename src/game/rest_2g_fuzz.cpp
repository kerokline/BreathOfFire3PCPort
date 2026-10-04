// BOF3X_SHADOW=rest_2g: group R2G's 48 functions through the scenario
// harness's field mode (scenario_harness.h, used unchanged; docs/scenario_harness.md
// sections 7 and 8), once at start-up. docs/rest_2g.md section 4.
// BOF3X_R2G_ONLY=<name> runs the clones whose name contains it (the controls).
//
// The clone table is tools/band_rows.py --group R2G --clones --harness scenario
// (2026-10-04) with the names given; every extent, call site and stack
// immediate agrees with the capstone read. Every function is a window
// record's handler, kind or state run on the record 0x905B84 names - kState in
// field mode (void, no arguments), the seed pointing 0x905B84 at one of the 22
// WindowRecords and its bytes inside the tables; the two that take an argument
// are kCall (BattleResultWin_ExpToNext a party slot, answering eax;
// MenuList_DrawLabelList a record).
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/rest_2g.h"
#include "game/rest_2g_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace rest_2g {
namespace {

namespace sh = scenario_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using sh::Shape;
using U = std::uint32_t;

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
unsigned char* Mem(U address) { return sh::Mem(address); }
unsigned char* Rec(unsigned k) { return Mem(at::kWindows + at::kWindowStride * (k % at::kWindowCount)); }
unsigned char* Current() { return sh::Pointer(at::kCurrent); }
U PickOf(std::initializer_list<U> v) { return *(v.begin() + sh::Next() % v.size()); }

// --- the clones -------------------------------------------------------------------------
// band_rows.py's call sites and stack immediates (2026-10-04), each checked
// against the capstone read.
constexpr sh::Imm kImms597FA0[] = {{0xD, 0x597FC0}};
constexpr sh::CallSite kCalls598810[] = {{0x1E, 0x4469D0}};
constexpr sh::Imm kImms598890[] = {{0xF, 0x5988D0}, {0x17, 0x598DC0}, {0x22, 0x598F60}, {0x2A, 0x599360}, {0x32, 0x599470}};
constexpr sh::Imm kImms5988D0[] = {{0xF, 0x598900}, {0x17, 0x5989B0}, {0x22, 0x5989F0}};
constexpr sh::CallSite kCalls598900[] = {{0x40, 0x435180}, {0x8C, 0x435180}};
constexpr sh::CallSite kCalls5989B0[] = {{0x0, 0x598A30}};
constexpr sh::CallSite kCalls5989F0[] = {{0x0, 0x598A30}, {0x1C, 0x59E310}, {0x30, 0x59E310}};
constexpr sh::CallSite kCalls59A030[] = {{0x35, 0x5738A0}};
constexpr sh::CallSite kCalls59A190[] = {{0x35, 0x573A80}};
constexpr sh::CallSite kCalls59A200[] = {{0x37, 0x573BF0}};
constexpr sh::CallSite kCalls59A270[] = {{0x37, 0x573BF0}};
constexpr sh::CallSite kCalls59A2E0[] = {{0x2F, 0x574AB0}, {0x46, 0x497740}, {0x6A, 0x516B30}, {0x89, 0x497740}, {0xAD, 0x516B30}};
constexpr sh::CallSite kCalls59A400[] = {{0x2E, 0x573CE0}};
constexpr sh::CallSite kCalls59A440[] = {{0x2E, 0x573F70}, {0x62, 0x573CE0}};
constexpr sh::CallSite kCalls59A560[] = {{0x19, 0x5759C0}};
constexpr sh::CallSite kCalls59A640[] = {{0x2B, 0x574890}};
constexpr sh::CallSite kCalls59A6B0[] = {{0x6, 0x59A6C0}};
constexpr sh::CallSite kCalls59A6C0[] = {{0x3B, 0x57CF60}, {0x60, 0x516B30}, {0x76, 0x57D910}, {0xE8, 0x516B30}, {0x10D, 0x516B30},
                                         {0x146, 0x516B30}, {0x16B, 0x516B30}, {0x194, 0x516B30}, {0x1B3, 0x57D910},
                                         {0x1CF, 0x57D910}, {0x211, 0x57D910}};
constexpr sh::CallSite kCalls59A8E0[] = {{0x3E, 0x574EC0}};
constexpr sh::CallSite kCalls59A9C0[] = {{0x19, 0x575F50}};
constexpr sh::CallSite kCalls59AA10[] = {{0x19, 0x5763F0}};
constexpr sh::CallSite kCalls59AA30[] = {{0x19, 0x59AA80}};

#define R2G_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define R2G_FN(name) reinterpret_cast<const void*>(&::name)
#define R2G_S(name, base, size) {#name, base, size, nullptr, 0, nullptr, 0, nullptr, 0, R2G_FN(name), 0, false, Shape::kState}
#define R2G_C(name, base, size, calls) \
    {#name, base, size, calls, R2G_N(calls), nullptr, 0, nullptr, 0, R2G_FN(name), 0, false, Shape::kState}
#define R2G_I(name, base, size, imms) \
    {#name, base, size, nullptr, 0, imms, R2G_N(imms), nullptr, 0, R2G_FN(name), 0, false, Shape::kState}

const sh::Clone kClones[] = {
    R2G_I(BattleResultWin_LevelUpStates, 0x597FA0, 0x1A, kImms597FA0),
    {"BattleResultWin_ExpToNext", 0x598810, 0x79, kCalls598810, R2G_N(kCalls598810), nullptr, 0, nullptr, 0,
     R2G_FN(BattleResultWin_ExpToNext), 0xFFFFFFFFu, false, Shape::kCall},
    R2G_I(Window_Handler5Kinds, 0x598890, 0x3E, kImms598890),
    R2G_I(GeneWin_GridStates, 0x5988D0, 0x2E, kImms5988D0),
    R2G_C(GeneWin_GridOpen, 0x598900, 0xA7, kCalls598900),
    R2G_C(GeneWin_GridSlideIn, 0x5989B0, 0x3F, kCalls5989B0),
    R2G_C(GeneWin_GridSlideOut, 0x5989F0, 0x3D, kCalls5989F0),
    R2G_S(MenuList_SlideRightTo182, 0x599C30, 0x28),
    R2G_S(MenuList_SlideLeftTo92, 0x599C60, 0x28),
    R2G_S(MenuList_SlideRightTo92, 0x599C90, 0x28),
    R2G_S(MenuList_SlideLeftOff300, 0x599CC0, 0x28),
    R2G_S(MenuList_SlideRightTo17Ge, 0x599CF0, 0x28),
    R2G_S(MenuList_SlideLeftTo182, 0x599D20, 0x28),
    R2G_S(MenuList_SlideLeftTo200, 0x599D90, 0x28),
    R2G_S(MenuList_SlideLeftOff100, 0x599DF0, 0x28),
    R2G_S(MenuList_SlideRightTo16, 0x599E20, 0x28),
    R2G_S(MenuList_SlideDownTo42, 0x599F70, 0x28),
    R2G_C(MenuList_StatsPanel, 0x59A030, 0x3E, kCalls59A030),
    R2G_S(MenuList_SlideLeftBesidePanel, 0x59A070, 0x36),
    R2G_S(MenuList_SlideUpTo62Le, 0x59A0B0, 0x28),
    R2G_S(MenuList_SlideDownToPanelRow, 0x59A0E0, 0x4C),
    R2G_S(MenuList_SlideLeftOff180, 0x59A130, 0x28),
    R2G_S(MenuList_SlideRightTo137Ge, 0x59A160, 0x28),
    R2G_C(MenuList_EquipPanel, 0x59A190, 0x3E, kCalls59A190),
    R2G_S(MenuList_SlideLeftTo166, 0x59A1D0, 0x28),
    R2G_C(MenuList_ExpPanel, 0x59A200, 0x40, kCalls59A200),
    R2G_S(MenuList_SlideRightTo45, 0x59A240, 0x28),
    R2G_C(MenuList_NextLevelPanel, 0x59A270, 0x40, kCalls59A270),
    R2G_S(MenuList_SlideLeftOff110, 0x59A2B0, 0x28),
    R2G_C(MenuList_WideTitleBox, 0x59A2E0, 0xB6, kCalls59A2E0),
    R2G_C(MenuList_CursorBox, 0x59A400, 0x37, kCalls59A400),
    R2G_C(MenuList_IconWheel, 0x59A440, 0x6B, kCalls59A440),
    R2G_S(MenuList_SlideRightOffColumn, 0x59A4B0, 0x51),
    R2G_S(MenuList_SlideLeftToColumn, 0x59A510, 0x4F),
    R2G_C(MenuList_ItemList, 0x59A560, 0x20, kCalls59A560),
    R2G_S(MenuList_SlideLeftTo152, 0x59A610, 0x28),
    R2G_C(MenuList_ButtonRow, 0x59A640, 0x34, kCalls59A640),
    R2G_C(MenuList_LabelList, 0x59A6B0, 0xD, kCalls59A6B0),
    {"MenuList_DrawLabelList", 0x59A6C0, 0x21D, kCalls59A6C0, R2G_N(kCalls59A6C0), nullptr, 0, nullptr, 0,
     R2G_FN(MenuList_DrawLabelList), 0, false, Shape::kCall},
    R2G_C(MenuList_EquipCompare, 0x59A8E0, 0x47, kCalls59A8E0),
    R2G_S(MenuList_SlideLeftTo165, 0x59A930, 0x28),
    R2G_S(MenuList_SlideLeftTo17, 0x59A960, 0x28),
    R2G_S(MenuList_SlideRightTo165, 0x59A990, 0x28),
    R2G_C(MenuList_AbilityPanel, 0x59A9C0, 0x20, kCalls59A9C0),
    R2G_S(MenuList_SlideRightTo150, 0x59A9E0, 0x28),
    R2G_C(MenuList_ItemPanel, 0x59AA10, 0x20, kCalls59AA10),
    R2G_C(MenuList_ReservePanel, 0x59AA30, 0x20, kCalls59AA30),
    R2G_S(MenuList_SlideLeftTo150, 0x59AA50, 0x28),
};
constexpr unsigned kCount = sizeof kClones / sizeof kClones[0];
static_assert(kCount == 48, "the cut's 48 rows");

// What each function's seed puts in: the record's +3 below `states` (its
// table's length; 0 leaves the byte random - the slides never read it), +2
// below `kinds`, and for a slide the word it moves (+4 or +6, 0 none) seeded
// around its bound: `step` the move, `bound` the fixed bound, or one of the
// computed ones below.
enum Bound : int { kOperand32 = 0x10000, kOperand16, kBesidePanel, kPanelRow, kRightColumn, kLeftColumn, kGrid };
struct Spec {
    unsigned char states, kinds, off;
    int step;
    int bound;
    U operand;
};
const Spec kSpecs[kCount] = {
    {1, 0, 0, 0, 0, 0},                                      // BattleResultWin_LevelUpStates: its stack table of one
    {0, 0, 0, 0, 0, 0},                                      // BattleResultWin_ExpToNext
    {0, 5, 0, 0, 0, 0},                                      // Window_Handler5Kinds: +2 below its five
    {3, 0, 0, 0, 0, 0},                                      // GeneWin_GridStates
    {0, 0, 0, 0, 0, 0},                                      // GeneWin_GridOpen
    {0, 0, 4, 0x20, kGrid, 0},                               // GeneWin_GridSlideIn
    {0, 0, 4, 0x20, kGrid, 0},                               // GeneWin_GridSlideOut
    {0, 0, 4, 0x20, 0xB6, 0},                                // MenuList_SlideRightTo182
    {0, 0, 4, -0x20, 0x5C, 0},                               // MenuList_SlideLeftTo92
    {0, 0, 4, 0x20, 0x5C, 0},                                // MenuList_SlideRightTo92
    {0, 0, 4, -0x20, kOperand32, at::kLeftOff300Bound},      // MenuList_SlideLeftOff300
    {0, 0, 4, 0x20, 0x11, 0},                                // MenuList_SlideRightTo17Ge
    {0, 0, 4, -0x20, 0xB6, 0},                               // MenuList_SlideLeftTo182
    {0, 0, 4, -0x20, 0xC8, 0},                               // MenuList_SlideLeftTo200
    {0, 0, 4, -0x20, kOperand32, at::kLeftOff100Bound},      // MenuList_SlideLeftOff100
    {0, 0, 4, 0x20, 0x10, 0},                                // MenuList_SlideRightTo16
    {0, 0, 6, 0x10, 0x2A, 0},                                // MenuList_SlideDownTo42
    {7, 0, 0, 0, 0, 0},                                      // MenuList_StatsPanel
    {0, 0, 4, -0x20, kBesidePanel, 0},                       // MenuList_SlideLeftBesidePanel
    {0, 0, 6, -0x10, 0x3E, 0},                               // MenuList_SlideUpTo62Le
    {0, 0, 6, 0x10, kPanelRow, 0},                           // MenuList_SlideDownToPanelRow
    {0, 0, 4, -0x20, kOperand32, at::kLeftOff180Bound},      // MenuList_SlideLeftOff180
    {0, 0, 4, 0x20, 0x89, 0},                                // MenuList_SlideRightTo137Ge
    {3, 0, 0, 0, 0, 0},                                      // MenuList_EquipPanel
    {0, 0, 4, -0x20, 0xA6, 0},                               // MenuList_SlideLeftTo166
    {3, 0, 0, 0, 0, 0},                                      // MenuList_ExpPanel
    {0, 0, 4, 0x20, 0x2D, 0},                                // MenuList_SlideRightTo45
    {3, 0, 0, 0, 0, 0},                                      // MenuList_NextLevelPanel
    {0, 0, 4, -0x20, kOperand32, at::kLeftOff110Bound},      // MenuList_SlideLeftOff110
    {3, 0, 0, 0, 0, 0},                                      // MenuList_WideTitleBox
    {0, 0, 0, 0, 0, 0},                                      // MenuList_CursorBox
    {3, 0, 0, 0, 0, 0},                                      // MenuList_IconWheel
    {0, 0, 4, 0x20, kRightColumn, 0},                        // MenuList_SlideRightOffColumn
    {0, 0, 4, -0x20, kLeftColumn, 0},                        // MenuList_SlideLeftToColumn
    {5, 0, 0, 0, 0, 0},                                      // MenuList_ItemList
    {0, 0, 4, -0x20, 0x98, 0},                               // MenuList_SlideLeftTo152
    {3, 0, 0, 0, 0, 0},                                      // MenuList_ButtonRow
    {0, 0, 0, 0, 0, 0},                                      // MenuList_LabelList
    {0, 0, 0, 0, 0, 0},                                      // MenuList_DrawLabelList
    {5, 0, 0, 0, 0, 0},                                      // MenuList_EquipCompare
    {0, 0, 4, -0x20, 0xA5, 0},                               // MenuList_SlideLeftTo165
    {0, 0, 4, -0x20, 0x11, 0},                               // MenuList_SlideLeftTo17
    {0, 0, 4, 0x20, 0xA5, 0},                                // MenuList_SlideRightTo165
    {5, 0, 0, 0, 0, 0},                                      // MenuList_AbilityPanel
    {0, 0, 4, 0x20, 0x96, 0},                                // MenuList_SlideRightTo150
    {5, 0, 0, 0, 0, 0},                                      // MenuList_ItemPanel
    {3, 0, 0, 0, 0, 0},                                      // MenuList_ReservePanel
    {0, 0, 4, -0x20, 0x96, 0},                               // MenuList_SlideLeftTo150
};

// --- the .data tables the kinds call through (swapped for recorders) -------------------
// Each table's own length (its count in symbols.toml): the seed draws +3 below it.
const sh::DataTable kTables[] = {
    {at::kStatsStates, 7},  {at::kEquipStates, 3},    {at::kExpStates, 3},         {at::kNextLevelStates, 3},
    {at::kWideTitleStates, 3}, {at::kIconWheelStates, 3}, {at::kItemListStates, 5}, {at::kButtonRowStates, 3},
    {at::kEquipCompareStates, 5}, {at::kAbilityStates, 5}, {at::kItemPanelStates, 5}, {at::kReserveStates, 3},
};

// --- the regions beyond the harness's field ones ----------------------------------------
const sh::Region kRegions[] = {
    {at::kWindows, at::kWindowCount * at::kWindowStride},   // WindowRecords 0x803160..0x803477
    {0x903A94, 0x903F90 - 0x903A94},                         // CharacterRecords past the style region
    {at::kGridWords, 0x24},                                  // the eighteen words GeneWin_GridOpen clears
    {at::kGridCells, 0x14},                                  // the eighteen grid cells
    {at::kTasks, at::kTaskCount * at::kTaskStride},         // the battle task pool
};

// --- the movers: what the functions read again after a call ------------------------------
// Repoint 0x905B84 at another record, or move a field of the current one, a
// party byte, the style byte or a grid cell - each only inside the regions.
void Move(U h) {
    const auto b = static_cast<unsigned char>(h >> 24);
    const U v = h >> 8;
    unsigned char* const r = Current();
    const bool in = sh::InRegions(r, at::kWindowStride);
    switch (h % 14) {
    case 0:
    case 1: sh::SetPointer(at::kCurrent, Rec(b)); break;
    case 2: if (in) SetWord(r + 4, b & 1 ? v & 0x1FF : v); break;
    case 3: if (in) SetWord(r + 6, b & 1 ? v & 0xFF : v); break;
    case 4: if (in) r[0xA] = static_cast<unsigned char>(b & 1 ? (b >> 1) & 3 : b); break;
    case 5: if (in) r[0xB] = static_cast<unsigned char>(b & 1 ? (b >> 1) & 3 : b); break;
    case 6: if (in) r[0xC] = static_cast<unsigned char>(b & 1 ? (b >> 1) & 3 : b); break;
    case 7: if (in) r[0xD] = b; break;
    case 8: {
        static constexpr U kIds[4] = {0, 0x1A, 0x31, 0x19};
        if (in) SetWord(r + 0x10, b & 1 ? kIds[(b >> 1) & 3] : v);
        break;
    }
    case 9: if (in) SetLong(r + 0x20, static_cast<std::int32_t>(h * 0x9E3779B1u)); break;
    case 10: Mem(at::kParty + b % 6)[0] = static_cast<unsigned char>(v % 9); break;
    case 11: Mem(at::kColour)[0] = b; break;
    case 12: Mem(at::kGridCells + b % 0x12)[0] ^= static_cast<unsigned char>(v | 1); break;
    case 13: if (in) r[3] = b; break;
    default: break;
    }
}
// The group disturb (one case of the harness's sixteen): from the hash only.
void Disturb(U h) { Move(h); }
// The effect of a stand-in the functions read after: moves from the noise.
U Moving(const U*, U answer) {
    Move(sh::Noise());
    if (sh::Noise() & 1) Move(sh::Noise());
    return answer;
}
// Msg_SystemPtr as the field set's FxText (its answer lands in the text
// buffer), and louder.
U TextAnswer(const U* a, U answer) {
    Moving(a, answer);
    return Key(sh::Text() + (answer & 0xF0));
}
// BattleTask_Create: moves, and a grid cell flipped (the open reads each cell
// after the call before it).
U TaskMoving(const U* a, U answer) {
    Moving(a, answer);
    const U n = sh::Noise();
    Mem(at::kGridCells + n % 0x12)[0] = static_cast<unsigned char>(n & 0x100 ? 0 : n >> 9);
    return answer;
}
// 0x4469D0: the record its answer names gets another level and EXP (the
// caller reads both after the call).
U RosterMoving(const U*, U answer) {
    const U n = sh::Noise();
    unsigned char* const rec = Mem(at::kCharRecords + at::kCharStride * (answer & 7));
    if (n & 1) rec[0xA] = static_cast<unsigned char>(n >> 8);
    if (n & 2) SetLong(rec + 0xC, static_cast<std::int32_t>(n >> 4) & 0xFFFFF);
    return answer;
}

constexpr sh::Answer kG = sh::Answer::kGarbage;
constexpr U kAll = 0xFFFFFFFFu, kU16 = 0xFFFF, kU8 = 0xFF;
#define R2G_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
#define R2G_STATE(address) "state " #address, address, address, 0, {}, kG, 0, 0, {}, &Moving

const sh::Callee kCallees[] = {
    // ours, called by E8 / E9: the masks what each push holds that the callee reads (docs/rest_2g.md section 3)
    {R2G_OURS(BattleTask_Create), 2, {kAll, kAll}, sh::Answer::kByte, 0, at::kTaskCount - 1, {}, &TaskMoving},
    {R2G_OURS(GeneWin_DrawGrid), 0, {}, kG, 0, 0, {}, &Moving},
    {R2G_OURS(Window_FreeCurrent), 0, {}, kG, 0, 0},
    {R2G_OURS(Menu_DrawStatsPanel), 3, {kAll, kAll, kAll}, kG, 0, 0},
    {R2G_OURS(Menu_DrawEquipPanel), 3, {kAll, kAll, kAll}, kG, 0, 0},
    {R2G_OURS(Menu_DrawExpPanel), 4, {kAll, kAll, kAll, kAll}, kG, 0, 0},
    {R2G_OURS(Menu_DrawTitleBox), 5, {kU16, kU16, kU16, kU16, kU8}, kG, 0, 0, {}, &Moving},
    {R2G_OURS(Msg_SystemPtr), 1, {kU16}, kG, 0, 0, {}, &TextAnswer},
    {R2G_OURS(Text_DrawAt), 5, {kU16, kU16, kU8, kU8, kAll}, kG, 0, 0, {}, &Moving},
    {R2G_OURS(Menu_DrawCursorBox), 6, {kU16, kU16, kU16, kU16, kU8, kU8}, kG, 0, 0},
    {R2G_OURS(Menu_DrawIconWheel), 5, {kAll, kU16, kAll, kU8, kAll}, kG, 0, 0, {}, &Moving},
    {R2G_OURS(Menu_DrawItemList), 1, {kAll}, kG, 0, 0},
    {R2G_OURS(Menu_DrawButtonRow), 5, {kAll, kU16, kU8, kU8, kAll}, kG, 0, 0},
    {R2G_OURS(Menu_DrawEquipCompare), 6, {kAll, kAll, kU16, kAll, kU8, kAll}, kG, 0, 0},
    {R2G_OURS(Menu_DrawAbilityPanel), 1, {kAll}, kG, 0, 0},
    {R2G_OURS(Menu_DrawItemPanel), 1, {kAll}, kG, 0, 0},
    {R2G_OURS(Menu_DrawBox), 6, {kU16, kU16, kU16, kU16, kU8, kU8}, kG, 0, 0, {}, &Moving},
    {R2G_OURS(Menu_DrawPieces), 4, {kU16, kU16, kAll, kU8}, kG, 0, 0, {0, 0, 16, 0}, &Moving, nullptr, true},
    {R2G_OURS(MenuList_DrawLabelList), 1, {kAll}, kG, 0, 0},
    // the group's own table handlers, louder than the harness's handler recorder:
    // the kinds read the record again after their state
    {R2G_STATE(0x437CC0)},
    {R2G_STATE(0x59A5E0)},
    {R2G_STATE(0x59A070)},
    {R2G_STATE(0x59A0B0)},
    {R2G_STATE(0x59A0E0)},
    {R2G_STATE(0x59A130)},
    {R2G_STATE(0x59A160)},
    {R2G_STATE(0x59A1D0)},
    {R2G_STATE(0x59A2B0)},
    {R2G_STATE(0x59A240)},
    {R2G_STATE(0x59A3A0)},
    {R2G_STATE(0x59A3D0)},
    {R2G_STATE(0x59A4B0)},
    {R2G_STATE(0x59A510)},
    {R2G_STATE(0x59A580)},
    {R2G_STATE(0x59A5B0)},
    {R2G_STATE(0x59A610)},
    {R2G_STATE(0x59A680)},
    {R2G_STATE(0x59A930)},
    {R2G_STATE(0x59A960)},
    {R2G_STATE(0x59A990)},
    {R2G_STATE(0x59A9E0)},
    {R2G_STATE(0x59AA50)},
    // nobody's yet, by address: R3B's roster index, R2H's reserve list
    {"0x4469D0", kRosterIndex, kRosterIndex, 1, {kAll}, sh::Answer::kByte, 0, 7, {}, &RosterMoving},
    {"0x59AA80", kReserveList, kReserveList, 1, {kAll}, kG, 0, 0},
};
#undef R2G_OURS
#undef R2G_STATE

// --- the seed ----------------------------------------------------------------------------

int BoundOf(const Spec& s, const unsigned char* r) {
    switch (s.bound) {
    case kOperand32: return static_cast<short>(Long(Mem(s.operand)));
    case kBesidePanel: return static_cast<short>(Word(Mem(at::kRecord4X))) + 0x78;
    case kPanelRow: return 0x36 * r[0xA] + 0x3E;
    case kRightColumn: return 75 * (r[0xB] & 1) + 0x140;
    case kLeftColumn: return 75 * ((r[0xB] & 1) + 2);
    case kGrid: {
        // GridSlideIn's 0x42, or GridSlideOut's bounds by +0xA
        const int n = sh::Next() % 3;
        if (n == 0) return 0x42;
        return r[0xA] == 0 ? static_cast<short>(Word(Mem(at::kGridOutRight))) : static_cast<short>(Word(Mem(at::kGridOutLeft)));
    }
    default: return s.bound;
    }
}

void Seed(unsigned k) {
    const Spec& s = kSpecs[k];
    unsigned char* const r = Rec(sh::Next());
    sh::SetPointer(at::kCurrent, r);
    // the record's bytes against their compares
    r[0xA] = static_cast<unsigned char>(PickOf({0, 1, 2, 3, 0, 1, sh::Next()}));
    r[0xB] = static_cast<unsigned char>(PickOf({0, 1, 2, 3, 4, 0xFF, sh::Next()}));
    r[0xC] = static_cast<unsigned char>(PickOf({0, 1, 2, 3, 4, 0, 0xFF, sh::Next()}));
    r[0xD] = static_cast<unsigned char>(PickOf({0, 1, 2, sh::Next()}));
    SetWord(r + 0x10, PickOf({0, 0x1A, 0x31, 0x19, 0x30, sh::Next() % 0x40, sh::Next()}));
    if (s.states) r[3] = static_cast<unsigned char>(sh::Next() % s.states);
    if (s.kinds) r[2] = static_cast<unsigned char>(sh::Next() % s.kinds);
    // record 4's x (MenuList_SlideLeftBesidePanel's bound)
    SetWord(Mem(at::kRecord4X), PickOf({0x62, 0x10, 0x7F80, 0x8000, sh::Next()}));
    // the party, the grid cells, the battle party's ids
    for (unsigned i = 0; i < 6; ++i)
        Mem(at::kParty + i)[0] = static_cast<unsigned char>(sh::Next() % 5 ? sh::Next() % 9 : sh::Next());
    for (unsigned i = 0; i < 0x12; ++i) Mem(at::kGridCells + i)[0] = static_cast<unsigned char>(sh::Next() % 3 ? 0 : sh::Next() | 1);
    for (unsigned i = 0; i < 3; ++i)
        Mem(at::kObjTrio + 0x89 + at::kObjStride * i)[0] = static_cast<unsigned char>(sh::Next() % 4 ? sh::Next() % 9 : sh::Next());
    // the levels and the EXP the next level is compared with
    for (unsigned i = 0; i < 8; ++i) {
        unsigned char* const c = Mem(at::kCharRecords + at::kCharStride * i);
        c[0xA] = static_cast<unsigned char>(PickOf({1, 10, 50, 98, 99, 0, 0xFF, sh::Next() % 99}));
        SetLong(c + 0xC, static_cast<std::int32_t>(PickOf({0, 1, 100, sh::Next() % 1000000, sh::Next()})));
    }
    // a slide's word around its bound: one step before it, within two of the frame that reaches it
    // (the grid's slides compare before they move: around the bound itself)
    if (s.off) {
        const int bound = BoundOf(s, r);
        const int before = s.bound == kGrid ? bound : bound - s.step;
        const int at = before + static_cast<int>(PickOf({0, 1, 2, 0xFFFFFFFF, 0xFFFFFFFE, 0, sh::Next() % 0x41 - 0x20}));
        SetWord(r + s.off, sh::Next() % 6 ? static_cast<unsigned>(at) : sh::Next());
    }
}

// The arguments: ExpToNext a party slot (a byte; the rest of the word left
// random), DrawLabelList a record, the current one most of the time.
void Args(unsigned k, U* a) {
    if (std::strcmp(kClones[k].name, "BattleResultWin_ExpToNext") == 0)
        a[0] = (a[0] & 0xFFFFFF00u) | PickOf({0, 1, 2, 0, 1, 2, 3, sh::Next() & 0xFF});
    else if (std::strcmp(kClones[k].name, "MenuList_DrawLabelList") == 0)
        a[0] = sh::Next() % 4 ? Key(Current()) : Key(Rec(sh::Next()));
}

}  // namespace

void SelfTest() {
    // BOF3X_R2G_ONLY: the clones whose name contains it (a control's run)
    static sh::Clone chosen[kCount];
    static unsigned index[kCount];
    const char* const only = std::getenv("BOF3X_R2G_ONLY");
    unsigned n = 0;
    for (unsigned k = 0; k < kCount; ++k)
        if (!only || !*only || std::strstr(kClones[k].name, only)) {
            index[n] = k;
            chosen[n++] = kClones[k];
        }
    if (n == 0) bof3::Fatal("rest_2g: BOF3X_R2G_ONLY=%s names no clone", only);
    static unsigned* s_index = index;
    sh::Group g = {"rest_2g", chosen, n, kCallees, sizeof kCallees / sizeof kCallees[0], kTables,
                   sizeof kTables / sizeof kTables[0], kRegions, sizeof kRegions / sizeof kRegions[0],
                   [](unsigned k) { Seed(s_index[k]); }, &Disturb, 4000};
    g.args = [](unsigned k, U* a) { Args(s_index[k], a); };
    g.field = true;
    sh::Run(g);
}

}  // namespace rest_2g

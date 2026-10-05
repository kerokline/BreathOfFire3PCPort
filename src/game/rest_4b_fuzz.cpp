// BOF3X_SHADOW=rest_4b: group R4B's 60 functions through the scenario harness
// (scenario_harness.h, used unchanged) in field mode, once at start-up.
// docs/rest_4b.md section 4. BOF3X_R4B_ONLY=<name> runs the clones whose name
// contains it (the controls' speed-up).
//
// The clone rows are tools/band_rows.py --group R4B --clones --harness scenario
// (2026-10-05, through the scratch wrapper band14.py), each extent read again
// to its last instruction (capstone); the cut's sizes are padding past them.
// Shapes: the tail kinds' and the board's states and dispatchers kState, the
// draws and helpers kCall with their arguments set by Args. The four runs of
// code words the dispatchers read are DataTables (their entries recorders
// while the fuzz runs; each run listed once from its first cell, so the tables
// that are its tails are covered); each dispatcher's index byte is seeded
// below its own table's count. Every callee the standard set types otherwise
// or lacks is listed here (registered before the standard rows: the group's
// listing stands) with what the callee reads.
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/rest_4b.h"
#include "game/rest_4b_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace rest_4b {
namespace {

namespace sh = scenario_harness;
namespace sa = scenario_harness::at;
using U = std::uint32_t;
using move_script::SetLong;
using move_script::SetWord;

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
template <typename... T> U PickOf(T... v) {
    const U values[] = {static_cast<U>(v)...};
    return sh::Pick(values, sizeof...(v));
}
unsigned char& B(U a) { return *sh::Mem(a); }
void SetW(U a, U v) { SetWord(sh::Mem(a), v); }
void SetL(U a, U v) { SetLong(sh::Mem(a), static_cast<std::int32_t>(v)); }
U Rec(unsigned k) { return at::kMembers + 8 * k; }
U SlotAt(unsigned s) { return at::kSlots + 8 * s; }   // s 1..8

// --- the clone table (band_rows.py --group R4B --clones --harness scenario, 2026-10-05) ---
constexpr sh::CallSite kCalls456D60[] = {{0x20, 0x4976D0}};
constexpr sh::CallSite kCalls456D90[] = {{0x5, 0x454590}};
constexpr sh::CallSite kCalls456DB0[] = {{0x0, 0x454810}, {0x9, 0x4549B0}};
constexpr sh::CallSite kCalls456DF0[] = {{0x15, 0x454770}};
constexpr sh::CallSite kCalls456E20[] = {{0x0, 0x454810}, {0x9, 0x41D430}, {0xE, 0x456E40}, {0x13, 0x456080}, {0x18, 0x455950}};
constexpr sh::CallSite kCalls456E40[] = {{0x1D, 0x57C4C0}};
constexpr sh::CallSite kCalls456E80[] = {{0x183, 0x4976D0}, {0x1A2, 0x591680}, {0x1CE, 0x4976D0}, {0x1E0, 0x590BB0}};
constexpr sh::CallSite kCalls4570C0[] = {{0x9, 0x41D430}, {0xE, 0x456E40}};
constexpr sh::CallSite kCalls4570E0[] = {{0xE, 0x4976D0}};
constexpr sh::CallSite kCalls457120[] = {{0x54, 0x4976D0}, {0x9C, 0x5B93D2}, {0xF4, 0x5B93D2}, {0x12E, 0x590BB0},
                                         {0x13C, 0x4976D0}, {0x148, 0x591680}, {0x172, 0x4976D0}};
constexpr sh::CallSite kCalls457300[] = {{0x1C, 0x454590}};
constexpr sh::CallSite kCalls457350[] = {{0x0, 0x454810}, {0x12, 0x41D430}, {0x17, 0x456E40}};
constexpr sh::CallSite kCalls457380[] = {{0xC, 0x454590}};
constexpr sh::CallSite kCalls4573C0[] = {{0x4C, 0x4976D0}, {0x6A, 0x591680}, {0xB5, 0x590BB0}, {0xFE, 0x4976D0}};
constexpr sh::CallSite kCalls4574E0[] = {{0x15, 0x454590}};
constexpr sh::CallSite kCalls457520[] = {{0x2F, 0x454590}};
constexpr sh::CallSite kCalls457570[] = {{0x0, 0x454810}, {0x12, 0x41D430}};
constexpr sh::CallSite kCalls457590[] = {{0x2D, 0x587AE0}, {0x3E, 0x41D430}, {0x43, 0x456E40}, {0x48, 0x587A00},
                                         {0x56, 0x4976D0}, {0x80, 0x587B40}, {0x97, 0x587910}};
constexpr sh::CallSite kCalls457680[] = {{0x79, 0x587740}, {0x9B, 0x587740}, {0xA3, 0x459D80}, {0xB7, 0x459430},
                                         {0xC8, 0x587740}, {0xD4, 0x4594A0}, {0xDC, 0x458D70}, {0xEA, 0x459250}};
constexpr sh::CallSite kCalls4577A0[] = {{0x19, 0x587740}, {0x49, 0x587740}, {0x7C, 0x459430}, {0x95, 0x461EB0},
                                         {0xF8, 0x587740}, {0x100, 0x458D70}, {0x10E, 0x459250}, {0x133, 0x45ECC0},
                                         {0x14B, 0x459460}, {0x15A, 0x458830}, {0x1B6, 0x45ECC0}, {0x1CD, 0x459460},
                                         {0x1E1, 0x458830}};
constexpr sh::CallSite kCalls457990[] = {{0x17, 0x587740}, {0x1F, 0x459D80}, {0x24, 0x459C40}, {0x36, 0x459C80},
                                         {0x54, 0x4594A0}, {0x5E, 0x459E50}, {0x66, 0x458D70}, {0x73, 0x459250},
                                         {0x81, 0x459250}, {0xAF, 0x45ECC0}, {0x101, 0x45ECC0}, {0x119, 0x459460},
                                         {0x129, 0x458830}};
constexpr sh::CallSite kCalls457AE0[] = {{0x1A, 0x587740}, {0x4E, 0x587740}, {0xD2, 0x461EB0}, {0xE9, 0x459430},
                                         {0x15A, 0x587740}, {0x162, 0x458D70}, {0x170, 0x459250}, {0x18C, 0x459B70},
                                         {0x1A8, 0x45ECC0}, {0x1C2, 0x45ECC0}, {0x1DA, 0x459460}, {0x1F3, 0x458830}};
constexpr sh::CallSite kCalls457CE0[] = {{0x15, 0x459C40}, {0x1A, 0x459D80}, {0x2C, 0x459C80}, {0x4A, 0x4594A0},
                                         {0x54, 0x459E50}, {0x5C, 0x458D70}, {0x69, 0x459250}, {0x77, 0x459250},
                                         {0x9B, 0x459B70}, {0xAF, 0x45ECC0}, {0xC8, 0x459460}, {0xD8, 0x458830}};
constexpr sh::CallSite kCalls457DD0[] = {{0x17, 0x587740}, {0x1F, 0x459D80}, {0x4F, 0x459EE0}, {0x93, 0x587740},
                                         {0xC9, 0x461EB0}, {0x12D, 0x587740}, {0x14D, 0x458D70}, {0x15B, 0x459250},
                                         {0x172, 0x459B70}, {0x183, 0x45ECC0}, {0x188, 0x459A30}, {0x1B9, 0x4598A0},
                                         {0x1F9, 0x5905D0}};
constexpr sh::CallSite kCalls457FE0[] = {{0x17, 0x587740}, {0x57, 0x459EE0}, {0xA2, 0x587740}, {0xCA, 0x461EB0},
                                         {0x141, 0x587740}, {0x149, 0x458D70}, {0x157, 0x459250}, {0x16E, 0x459B70},
                                         {0x17F, 0x45ECC0}, {0x184, 0x459A30}, {0x1B5, 0x4598A0}, {0x20C, 0x459A80},
                                         {0x24C, 0x5905D0}};
constexpr sh::CallSite kCalls458240[] = {{0x17, 0x587740}, {0x46, 0x459EE0}, {0x53, 0x461EB0}, {0x65, 0x587740},
                                         {0x79, 0x458D70}, {0x86, 0x459250}, {0x9E, 0x459B70}, {0xAF, 0x45ECC0},
                                         {0xB4, 0x459A30}, {0xE2, 0x4598A0}, {0x12D, 0x459A80}, {0x179, 0x459A80},
                                         {0x1B6, 0x5905D0}};
constexpr sh::CallSite kCalls458410[] = {{0x11C, 0x587740}, {0x144, 0x587740}, {0x176, 0x587740}, {0x19E, 0x461EB0},
                                         {0x1B0, 0x587740}, {0x1C4, 0x458D70}, {0x1D2, 0x459250}, {0x1E9, 0x459B70},
                                         {0x1FA, 0x45ECC0}, {0x202, 0x459A30}, {0x23F, 0x4598A0}, {0x27F, 0x5905D0},
                                         {0x2B4, 0x4598A0}, {0x30C, 0x459A80}, {0x349, 0x5905D0}, {0x385, 0x459A80},
                                         {0x3C9, 0x459A80}, {0x406, 0x5905D0}};
constexpr sh::CallSite kCalls458830[] = {{0x23, 0x57CF60}, {0x3A, 0x57CF60}, {0x41, 0x458AE0}, {0x61, 0x516B30},
                                         {0x70, 0x5A7740}, {0xB7, 0x461E50}, {0xE9, 0x458B90}, {0x106, 0x458B90},
                                         {0x123, 0x458B90}, {0x140, 0x458B90}, {0x1CD, 0x458B90}, {0x20F, 0x458B90},
                                         {0x24E, 0x458B90}, {0x274, 0x458B90}, {0x299, 0x458B90}};
constexpr sh::CallSite kCalls458AE0[] = {{0x10, 0x4597E0}, {0x29, 0x4597E0}, {0x36, 0x4597E0}, {0x4F, 0x4597E0},
                                         {0x64, 0x4597E0}, {0x71, 0x4597E0}, {0x87, 0x4597E0}, {0x94, 0x4597E0}};
constexpr sh::CallSite kCalls458B90[] = {{0x77, 0x5A7610}, {0x147, 0x461E50}, {0x153, 0x5A7610}, {0x1C9, 0x461E50}};
constexpr sh::CallSite kCalls458D70[] = {{0x1D, 0x57CF60}, {0x2F, 0x57D520}, {0x68, 0x516B30}, {0x8C, 0x5A77C0},
                                         {0x95, 0x461E50}, {0xB8, 0x5A7740}, {0xC0, 0x5A7780}, {0x11F, 0x461E50},
                                         {0x141, 0x57CF60}, {0x14A, 0x4591A0}, {0x15C, 0x57D520}, {0x163, 0x459430},
                                         {0x186, 0x4597E0}, {0x1BD, 0x57D520}, {0x1DB, 0x4597E0}, {0x1E1, 0x459430},
                                         {0x22F, 0x4597E0}, {0x29D, 0x57D520}, {0x2C8, 0x57CF60}, {0x2DF, 0x4597E0},
                                         {0x31C, 0x459720}, {0x32B, 0x459430}, {0x350, 0x459B70}, {0x361, 0x4597E0},
                                         {0x3A7, 0x4597E0}, {0x3BE, 0x5B9380}, {0x3CE, 0x516F60}, {0x3E0, 0x516B30},
                                         {0x3FC, 0x5B9380}, {0x412, 0x516F60}};
constexpr sh::CallSite kCalls4591A0[] = {{0x10, 0x4597E0}, {0x2C, 0x4597E0}, {0x39, 0x4597E0}, {0x55, 0x4597E0},
                                         {0x67, 0x4597E0}, {0x74, 0x4597E0}, {0x8A, 0x4597E0}, {0x97, 0x4597E0}};
constexpr sh::CallSite kCalls459250[] = {{0x3C, 0x5A7670}, {0x10A, 0x461E50}, {0x116, 0x5A7670}, {0x1C7, 0x461E50}};
constexpr sh::CallSite kCalls4594A0[] = {{0xD, 0x461EB0}, {0x276, 0x587740}};
constexpr sh::CallSite kCalls459720[] = {{0x11, 0x5A77C0}, {0x1A, 0x461E50}, {0x26, 0x5A7730}, {0xB3, 0x461E50}};
constexpr sh::CallSite kCalls4597E0[] = {{0xF, 0x5A77C0}, {0x18, 0x461E50}, {0x24, 0x5A7710}, {0xB5, 0x461E50}};
constexpr sh::CallSite kCalls4598A0[] = {{0x35, 0x57CF60}, {0x50, 0x57CF60}, {0x5B, 0x459960}, {0x97, 0x516B30}};
constexpr sh::CallSite kCalls459960[] = {{0x10, 0x4597E0}, {0x2E, 0x4597E0}, {0x3B, 0x4597E0}, {0x54, 0x4597E0},
                                         {0x82, 0x4597E0}, {0x8F, 0x4597E0}, {0xAC, 0x4597E0}, {0xB5, 0x4597E0}};
constexpr sh::CallSite kCalls459A80[] = {{0x5E, 0x57CF60}, {0x79, 0x57CF60}, {0x84, 0x459960}, {0xCB, 0x516B30}};
constexpr sh::CallSite kCalls459B70[] = {{0x3F, 0x459430}};
constexpr sh::CallSite kCalls459C40[] = {{0x27, 0x587740}};
constexpr sh::CallSite kCalls459C80[] = {{0x1C, 0x587740}, {0x2E, 0x459430}, {0x9C, 0x459460}, {0xEA, 0x587740}};

#define R4B_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define R4B_ROW(name, base, size, calls) #name, base, size, calls, R4B_N(calls), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::name)
#define R4B_LEAF(name, base, size) #name, base, size, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::name)
constexpr sh::Shape kS = sh::Shape::kState, kC = sh::Shape::kCall;
constexpr U kXY = sh::ArgAt(0, sh::Arg::kScratch) | sh::ArgAt(1, sh::Arg::kScratch);
const sh::Clone kAll[] = {
    {R4B_LEAF(CommuTail14_Dispatch, 0x456D50, 0xE), 0, false, kS},
    {R4B_ROW(CommuTail14_OpenF8, 0x456D60, 0x30, kCalls456D60), 0, false, kS},
    {R4B_ROW(CommuTail14_LoadDat, 0x456D90, 0x1A, kCalls456D90), 0, false, kS},
    {R4B_ROW(CommuTail_WaitLoad, 0x456DB0, 0x3C, kCalls456DB0), 0, false, kS},
    {R4B_ROW(CommuTail_LoadSoundBank, 0x456DF0, 0x2A, kCalls456DF0), 0, false, kS},
    {R4B_ROW(CommuTail14_End, 0x456E20, 0x1E, kCalls456E20), 0, false, kS},
    {R4B_ROW(CommuTail_RestoreFacing, 0x456E40, 0x24, kCalls456E40), 0, false, kS},
    {R4B_LEAF(CommuTail21_Dispatch, 0x456E70, 0xE), 0, false, kS},
    {R4B_ROW(CommuTail_TimedGift, 0x456E80, 0x236, kCalls456E80), 0, false, kS},
    {R4B_ROW(CommuTail_EndAfterMessage, 0x4570C0, 0x14, kCalls4570C0), 0, false, kS},
    {R4B_ROW(CommuTail_Open97, 0x4570E0, 0x2A, kCalls4570E0), 0, false, kS},
    {R4B_LEAF(CommuTail22_Dispatch, 0x457110, 0xE), 0, false, kS},
    {R4B_ROW(CommuTail_RandomGift, 0x457120, 0x1C5, kCalls457120), 0, false, kS},
    {R4B_LEAF(CommuTail23_Dispatch, 0x4572F0, 0xE), 0, false, kS},
    {R4B_ROW(CommuTail23_LoadDat, 0x457300, 0x31, kCalls457300), 0, false, kS},
    {R4B_LEAF(CommuTail_GameDispatch, 0x457340, 0xE), 0, false, kS},
    {R4B_ROW(CommuTail_EndAfterLoad, 0x457350, 0x1D, kCalls457350), 0, false, kS},
    {R4B_LEAF(CommuTail24_Dispatch, 0x457370, 0xE), 0, false, kS},
    {R4B_ROW(CommuTail24_LoadDat, 0x457380, 0x21, kCalls457380), 0, false, kS},
    {R4B_LEAF(CommuTail25_Dispatch, 0x4573B0, 0xE), 0, false, kS},
    {R4B_ROW(CommuTail_NibbleGift, 0x4573C0, 0x111, kCalls4573C0), 0, false, kS},
    {R4B_ROW(CommuTail25_LoadDat, 0x4574E0, 0x2A, kCalls4574E0), 0, false, kS},
    {R4B_LEAF(CommuTail26_Dispatch, 0x457510, 0xE), 0, false, kS},
    {R4B_ROW(CommuTail26_LoadDat, 0x457520, 0x44, kCalls457520), 0, false, kS},
    {R4B_ROW(CommuTail26_End, 0x457570, 0x18, kCalls457570), 0, false, kS},
    {R4B_ROW(CommuTail60_Stream, 0x457590, 0xAC, kCalls457590), 0, false, kS},
    {R4B_LEAF(CommuBoard_Dispatch, 0x457640, 0xE), 0, false, kS},
    {R4B_LEAF(CommuBoard_Init, 0x457650, 0x29), 0, false, kS},
    {R4B_ROW(CommuBoard_Grid, 0x457680, 0xF3, kCalls457680), 0, false, kS},
    {R4B_LEAF(CommuBoard_ModeDispatch, 0x457780, 0xE), 0, false, kS},
    {R4B_LEAF(CommuBoard_StepDispatch, 0x457790, 0xE), 0, false, kS},
    {R4B_ROW(CommuBoard_PickRecord, 0x4577A0, 0x1F0, kCalls4577A0), 0, false, kS},
    {R4B_ROW(CommuBoard_MoveRecord, 0x457990, 0x133, kCalls457990), 0, false, kS},
    {R4B_LEAF(CommuBoard_StepDispatchB, 0x457AD0, 0xE), 0, false, kS},
    {R4B_ROW(CommuBoard_PickRecordB, 0x457AE0, 0x200, kCalls457AE0), 0, false, kS},
    {R4B_ROW(CommuBoard_MoveRecordB, 0x457CE0, 0xE4, kCalls457CE0), 0, false, kS},
    {R4B_ROW(CommuBoard_PickList, 0x457DD0, 0x202, kCalls457DD0), 0, false, kS},
    {R4B_ROW(CommuBoard_PickListB, 0x457FE0, 0x258, kCalls457FE0), 0, false, kS},
    {R4B_ROW(CommuBoard_PickListC, 0x458240, 0x1C2, kCalls458240), 0, false, kS},
    {R4B_ROW(CommuBoard_Confirm, 0x458410, 0x413, kCalls458410), 0, false, kS},
    {R4B_ROW(CommuBoard_DrawRecordCard, 0x458830, 0x2A6, kCalls458830), 0, false, kC},
    {R4B_ROW(CommuBoard_DrawCardFrame, 0x458AE0, 0xA1, kCalls458AE0), 0, false, kC},
    {R4B_ROW(CommuBoard_DrawBar, 0x458B90, 0x1D9, kCalls458B90), 0, false, kC},
    {R4B_ROW(CommuBoard_DrawPanel, 0x458D70, 0x422, kCalls458D70), 0, false, kS},
    {R4B_ROW(CommuBoard_DrawPanelFrame, 0x4591A0, 0xA4, kCalls4591A0), 0, false, kC},
    {R4B_ROW(CommuBoard_DrawSlotLines, 0x459250, 0x1D3, kCalls459250), 0, false, kC},
    {R4B_LEAF(Commu_CountInSlot, 0x459430, 0x26), 0xFF, false, kC},
    {R4B_LEAF(Commu_NthInSlot, 0x459460, 0x31), 0xFF, false, kC},
    {R4B_ROW(CommuBoard_MoveGridCursor, 0x4594A0, 0x27D, kCalls4594A0), 0, false, kC},
    {R4B_ROW(CommuBoard_DrawDigits, 0x459720, 0xBF, kCalls459720), 0, false, kC},
    {R4B_ROW(CommuBoard_DrawSprite, 0x4597E0, 0xBF, kCalls4597E0), 0, false, kC},
    {R4B_ROW(CommuBoard_DrawListBox, 0x4598A0, 0xB3, kCalls4598A0), 0, false, kC},
    {R4B_ROW(CommuBoard_DrawListFrame, 0x459960, 0xC2, kCalls459960), 0, false, kC},
    {R4B_LEAF(CommuBoard_ListY, 0x459A30, 0x46), 0xFFFF, false, kC},
    {R4B_ROW(CommuBoard_DrawListBoxB, 0x459A80, 0xE8, kCalls459A80), 0xFFFF, false, kC},
    {R4B_ROW(CommuBoard_SlotRecordXY, 0x459B70, 0xC3, kCalls459B70), 0, false, kC, kXY},
    {R4B_ROW(CommuBoard_CancelStep, 0x459C40, 0x3C, kCalls459C40), 0, false, kS},
    {R4B_ROW(CommuBoard_PlaceRecord, 0x459C80, 0xF6, kCalls459C80), 0xFF, false, kC},
    {R4B_LEAF(CommuBoard_SlotHelp, 0x459D80, 0xC7), 0, false, kS},
    {R4B_LEAF(CommuBoard_PickHelp, 0x459E50, 0x8A), 0, false, kC},
};
#undef R4B_ROW
#undef R4B_LEAF
#undef R4B_N
constexpr unsigned kCount = sizeof kAll / sizeof kAll[0];
static_assert(kCount == 60, "the cut's 60 rows for R4B");

// A dispatcher's index byte and its table's count (the run of code words from
// its cell, docs/rest_4b.md section 3).
struct Dispatch { U base, by; unsigned count; };
const Dispatch kDispatch[] = {
    {0x456D50, at::kTailState, 11}, {0x456E70, at::kTailState, 5},  {0x457110, at::kTailState, 2},
    {0x4572F0, at::kTailState, 21}, {0x457340, at::kTailArg, 16},   {0x457370, at::kTailState, 12},
    {0x4573B0, at::kTailState, 7},  {0x457510, at::kTailState, 5},  {0x457640, at::kBoardState, 14},
    {0x457780, at::kSavedMode, 11}, {0x457790, at::kBoardStep, 8},  {0x457AD0, at::kBoardStep, 6},
};
const Dispatch* DispatchOf(U base) {
    for (const Dispatch& d : kDispatch)
        if (d.base == base) return &d;
    return nullptr;
}

// The four runs; the other eight tables are their tails.
const sh::DataTable kTables[] = {
    {at::kTail14States, 11},
    {at::kTail23States, 21},
    {at::kTail26States, 5},
    {at::kBoardStates, 14},
};

// --- the moves (from a hash: the harness's Noise() or Disturb's) -----------------------
//
// What the functions read again after a call: the tail's state and argument,
// the board's state and step, the list cursor, the picks, the toggle, the help
// word, a slot's bytes, a record's slot and marks, the list count, the clock,
// the grid cursor, the kept object.
constexpr unsigned kCases = 15;
void Move(U h) {
    const unsigned v = (h >> 8) & 0xFF;
    const unsigned w = h >> 16;
    switch (sh::DisturbCase(h, kCases)) {
    case 0: B(at::kTailState) = static_cast<unsigned char>(v % 22); break;
    case 1: B(at::kTailArg) = static_cast<unsigned char>(v % 8 + 1); break;
    case 2: B(at::kBoardState) = static_cast<unsigned char>(v % 14); break;
    case 3: B(at::kBoardStep) = static_cast<unsigned char>(v % 8); break;
    case 4: B(at::kListCursor) = static_cast<unsigned char>(v % 10 - 1); break;
    case 5: B(at::kPick) = static_cast<unsigned char>(v % 13 - 1); break;
    case 6: B(at::kPickB + w % 2) = static_cast<unsigned char>(v % 5); break;          // 0x675F7E or 0x675F7F
    case 7: B(w & 1 ? at::kPickD : at::kPickE) = static_cast<unsigned char>(v & 1 ? 0xFF : v % 6); break;
    case 8: SetW(at::kHelp, v & 1 ? 0x3D : (v & 2 ? 0xFFFF : w & 0x1FF)); break;
    case 9: B(SlotAt(w % 8 + 1) + (v % 3)) = static_cast<unsigned char>(v % 3 == 1 ? (w >> 3) % 3 : (v & 4 ? 0 : 4 + (w >> 3) % 10)); break;
    case 10: B(Rec(w % 60) + 1 + v % 3) = static_cast<unsigned char>(v % 3 == 0 ? (w >> 6) % 8 + 1 : w >> 6); break;
    case 11: B(at::kListCount) = static_cast<unsigned char>(v % 7); break;
    case 12: SetL(at::kClock, h * 0x9E3779B1u); break;
    case 13: B(at::kToggle + (w % 2 ? 1 : 0)) = static_cast<unsigned char>(v & 1); break;   // the toggle or 0x675F7D
    case 14: B(at::kCursorMode + w % 3) = static_cast<unsigned char>(v % 5); break;
    default: break;
    }
}

U Stir(const U*, U answer) {
    Move(sh::Noise());
    return answer;
}
// Item_NamePtr's answer lands in the harness's text buffer; the callers copy 16
// bytes from it.
U TextAnswer(const U*, U answer) {
    Move(sh::Noise());
    return Key(sh::Text() + (answer & 0xF0));
}
// Input_AutoRepeat: each key the states test, alone, together and none.
U KeysAnswer(const U*, U answer) {
    const U n = sh::Noise();
    static const U kKeys[] = {0x1000, 0x2000, 0x4000, 0x8000, 0, 0x5000, 0xA000, 0x3000, 0xC000, 0xF000};
    Move(n >> 4);
    return n % 7 == 0 ? answer : kKeys[(n >> 3) % (sizeof kKeys / sizeof kKeys[0])];
}
// CommuBoard_SlotRecordXY writes two words through its pointers (the caller's
// stack): the same words on both sides.
U XYAnswer(const U* a, U answer) {
    const U n = sh::Noise();
    auto* const x = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a[0]));
    auto* const y = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a[1]));
    SetWord(x, n & 0x1FF);
    SetWord(y, (n >> 9) & 0xFF);
    Move(n >> 3);
    return answer;
}
// Crt_sprintf as the standard set's FxSprintf: up to seven letters and a NUL at
// the destination, the count answered.
U Sprintf(const U* a, U) {
    auto* const dst = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a[0]));
    if (!sh::InRegions(dst, 8)) return 0;
    const unsigned n = sh::Noise() % 8;
    for (unsigned i = 0; i < n; ++i) dst[i] = static_cast<unsigned char>('0' + sh::Noise() % 43);
    dst[n] = 0;
    return n;
}

#define R4B_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
constexpr sh::Answer kG = sh::Answer::kGarbage;
constexpr U kW = 0xFFFFFFFFu;
const sh::Callee kCallees[] = {
    // the group's own, called directly (E8 / E9), with what each reads
    {R4B_OURS(CommuTail_RestoreFacing), 0, {}, kG, 0, 0, {}, &Stir},
    {R4B_OURS(CommuBoard_DrawRecordCard), 4, {0xFFFF, 0xFFFF, 0xFF, 0xFF}, kG, 0, 0},
    {R4B_OURS(CommuBoard_DrawCardFrame), 2, {0xFFFF, 0xFFFF}, kG, 0, 0},
    {R4B_OURS(CommuBoard_DrawBar), 5, {0xFFFF, 0xFFFF, 0xFFFF, 0xFF, 0xFF}, kG, 0, 0},
    {R4B_OURS(CommuBoard_DrawPanel), 0, {}, kG, 0, 0, {}, &Stir},
    {R4B_OURS(CommuBoard_DrawPanelFrame), 2, {0xFFFF, 0xFFFF}, kG, 0, 0},
    {R4B_OURS(CommuBoard_DrawSlotLines), 2, {0xFF, 0xFF}, kG, 0, 0, {}, &Stir},
    {R4B_OURS(Commu_CountInSlot), 1, {0xFF}, sh::Answer::kByte, 0, 4, {}, &Stir},
    {R4B_OURS(Commu_NthInSlot), 2, {0xFF, 0xFF}, sh::Answer::kByte, 0, 0x3B, {}, &Stir},
    {R4B_OURS(CommuBoard_MoveGridCursor), 1, {kW}, kG, 0, 0, {}, &Stir},
    {R4B_OURS(CommuBoard_DrawDigits), 3, {0xFFFF, 0xFFFF, 0xFF}, kG, 0, 0},
    {R4B_OURS(CommuBoard_DrawSprite), 3, {0xFFFF, 0xFFFF, 0xFF}, kG, 0, 0},
    {R4B_OURS(CommuBoard_DrawListBox), 3, {0xFFFF, 0xFFFF, 0xFF}, kG, 0, 0},
    {R4B_OURS(CommuBoard_DrawListFrame), 3, {0xFFFF, 0xFFFF, 0xFF}, kG, 0, 0},
    {R4B_OURS(CommuBoard_ListY), 0, {}, kG, 0, 0, {}, &Stir},
    {R4B_OURS(CommuBoard_DrawListBoxB), 4, {0xFFFF, 0xFFFF, 0xFF, 0xFF}, kG, 0, 0, {}, &Stir},
    {R4B_OURS(CommuBoard_SlotRecordXY), 4, {0, 0, 0xFF, 0xFF}, kG, 0, 0, {}, &XYAnswer},
    {R4B_OURS(CommuBoard_CancelStep), 0, {}, kG, 0, 0, {}, &Stir},
    {R4B_OURS(CommuBoard_PlaceRecord), 1, {0xFF}, sh::Answer::kFlag, 0, 0, {}, &Stir},
    {R4B_OURS(CommuBoard_SlotHelp), 0, {}, kG, 0, 0, {}, &Stir},
    {R4B_OURS(CommuBoard_PickHelp), 1, {kW}, kG, 0, 0, {}, &Stir},
    // other groups' of this wave, by address (docs/rest_4b.md section 6)
    {"0x456080", at::kR4ACountSlots, at::kR4ACountSlots, 0, {}, kG, 0, 0, {}, &Stir},   // R4A
    {"0x455950", at::kR4ASettle, at::kR4ASettle, 0, {}, kG, 0, 0, {}, &Stir},           // R4A
    {"0x459EE0", at::kR4CAsk, at::kR4CAsk, 0, {}, kG, 0, 0, {}, &Stir},                 // R4C
    {"0x45ECC0", at::kR4EHand, at::kR4EHand, 3, {0xFFFF, 0xFFFF, 0xFF}, kG, 0, 0},       // R4E: movsx words, a byte
    // ours, outside the standard set or typed otherwise, with what each reads
    {R4B_OURS(Item_NamePtr), 2, {0xFF, 0xFF}, kG, 0, 0, {}, &TextAnswer},
    {R4B_OURS(Inventory_Add), 3, {0xFF, 0xFF, 0xFF}, sh::Answer::kFlag, 0, 0, {}, &Stir},
    {R4B_OURS(Msg_OpenScript), 1, {0xFFFF}, kG, 0, 0, {}, &Stir},
    {R4B_OURS(Sound_PlayEffect), 1, {0xFFFF}, kG, 0, 0, {}, &Stir},
    {R4B_OURS(Input_AutoRepeat), 1, {kW}, kG, 0, 0, {}, &KeysAnswer},
    {R4B_OURS(File_LoadDone), 0, {}, sh::Answer::kBool, 0, 0, {}, &Stir},
    {R4B_OURS(LoadDatFile), 1, {kW}, kG, 0, 0, {}, &Stir},
    {R4B_OURS(Gfx_ClutStripRestore), 0, {}, kG, 0, 0},
    {R4B_OURS(Snd_LoadBankFile), 1, {kW}, kG, 0, 0, {}, &Stir},
    {R4B_OURS(Area131_DisarmTail), 0, {}, kG, 0, 0, {}, &Stir},
    {R4B_OURS(Sprite_FaceDirection), 1, {0xFF}, kG, 0, 0},
    {R4B_OURS(Menu_DrawOutlineNotched), 5, {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFF}, kG, 0, 0},
    {R4B_OURS(Menu_DrawBox), 6, {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFF, 0xFF}, kG, 0, 0},
    {R4B_OURS(Text_DrawAt), 5, {0xFFFF, 0xFFFF, 0xFF, 0xFF, kW}, kG, 0, 0, {}, &Stir},
    {R4B_OURS(Text_DrawFont12), 4, {0xFFFF, 0xFFFF, 0x3F, kW}, kG, 0, 0, {0, 0, 0, sh::kDerefString}, nullptr, nullptr, true},
    {R4B_OURS(Gpu_SetLineF3), 1, {kW}, kG, 0, 0, {16}, nullptr, nullptr, true},
    {R4B_OURS(Gpu_SetSprt16), 1, {kW}, kG, 0, 0, {16}, nullptr, nullptr, true},
    {R4B_OURS(Gpu_SetSprt), 1, {kW}, kG, 0, 0, {16}, nullptr, nullptr, true},
    {R4B_OURS(Gpu_SetTile), 1, {kW}, kG, 0, 0, {16}, nullptr, nullptr, true},
    {R4B_OURS(Gpu_SetDrawMode), 5, {kW, kW, kW, kW, kW}, kG, 0, 0, {16, 0, 0, 0, 0}, nullptr, nullptr, true},
    {R4B_OURS(Gpu_SetSemiTrans), 2, {kW, kW}, kG, 0, 0, {16, 0}, nullptr, nullptr, true},
    {R4B_OURS(Menu_DrawHand), 3, {0xFFFF, 0xFFFF, 0}, kG, 0, 0},
    {R4B_OURS(Music_Play), 2, {kW, kW}, kG, 0, 0, {}, &Stir},
    {R4B_OURS(Music_FadeOutStop), 1, {kW}, kG, 0, 0, {}, &Stir},
    {R4B_OURS(Sound_LoadStream), 1, {kW}, kG, 0, 0, {}, &Stir},
    {R4B_OURS(Sound_StreamDone), 0, {}, sh::Answer::kBool, 0, 0, {}, &Stir},
    // every format here takes one number: three words
    {"Crt_sprintf", 0x5B9380, 0x5B9380, 3, {kW, kW, kW}, kG, 0, 0, {0, 16}, &Sprintf, nullptr, true},
};
#undef R4B_OURS

// Beyond the harness's field regions (which hold Field_Request, Game_AreaNumber,
// Sprite_Current, Gfx_ClutStripDirty, Frame_Counter, the style byte, the text
// scratch, Field_ScriptFlags2, the save block's 0x904098..0x904160 with the
// clock 0x904134 and 0x904560..0x904700 with the counters 0x9046CA.. and the
// first records, Field_ScriptFlags, the buttons, MessagePools' offsets).
const sh::Region kRegions[] = {
    {0x9039F0, 8},             // the tail's kind, state and argument 0x9039F3..0x9039F5
    {0x939A38, 0xC},           // the kept object 0x939A38, the board's bytes 0x939A3C..0x939A41
    {0x675F70, 0x18},          // the board's cursor and picks 0x675F74..0x675F86
    {0x904700, 0x394},         // the records to 0x9048B0, the slots, the names, 0x904A90
    {0x937F80, 8},             // 0x937F80
    {0x7DEE40, 0x10},          // MsgBoxState's message index 0x7DEE48
    {at::kTextRecords, 0x10},  // Text_Records' first 16 bytes
};

// --- the seed ----------------------------------------------------------------------------

void SeedButtons() {
    const U confirm = 1u << (sh::Next() % 16);
    U cancel = 1u << (sh::Next() % 16);
    if (cancel == confirm) cancel = confirm == 0x8000 ? 1 : confirm << 1;
    SetW(at::kConfirm, confirm | (sh::Half() ? 0 : 1u << (sh::Next() % 16)));
    SetW(at::kCancel, cancel);
    const U other = sh::Next() & 0xFFFF & ~(confirm | cancel);
    SetW(at::kPressed, PickOf(confirm, cancel, confirm | cancel, 0, 0, other, confirm | other, cancel | other));
}

// The 60 records: in use or not, a slot 0..11 mostly, the marks, a start time.
void SeedRecords() {
    for (unsigned k = 0; k < 60; ++k) {
        const U r = Rec(k);
        B(r) = static_cast<unsigned char>(PickOf(0, 1, 1, sh::Next()));
        B(r + 1) = static_cast<unsigned char>(sh::Often() ? sh::Next() % 9 : PickOf(9, 10, 11, sh::Next()));
        B(r + 2) = static_cast<unsigned char>(PickOf(0, 2, 3, sh::Next()));
        B(r + 3) = static_cast<unsigned char>(sh::Next());
        SetL(r + 4, sh::Next());
    }
}

// The eight slots: a kind 0 or 4..13 mostly, a level 0..2 (+1), a toggle (+2).
void SeedSlots() {
    for (unsigned s = 1; s <= 8; ++s) {
        const U p = SlotAt(s);
        B(p) = static_cast<unsigned char>(sh::Often() ? PickOf(0, 4, 5, 6 + sh::Next() % 8) : sh::Next());
        B(p + 1) = static_cast<unsigned char>(sh::Next() % 3);
        B(p + 2) = static_cast<unsigned char>(PickOf(0, 1, sh::Next()));
        B(p + 3) = static_cast<unsigned char>(sh::Next());
        SetL(p + 4, sh::Next());
    }
}

void SeedBoard() {
    B(at::kTailKind) = static_cast<unsigned char>(sh::Next());
    B(at::kTailState) = static_cast<unsigned char>(PickOf(0, 1, 2, 3, sh::Next() % 22, sh::Next()));
    B(at::kTailArg) = static_cast<unsigned char>(PickOf(0, 1, 3, 8, 9, 11, sh::Next() % 12));
    SetL(at::kTailObject, Key(sh::SpriteRecord(sh::Next())));
    B(at::kBoardFlag) = static_cast<unsigned char>(PickOf(0, 1, sh::Next()));
    B(at::kBoardTrack) = static_cast<unsigned char>(sh::Next());
    B(at::kBoardState) = static_cast<unsigned char>(sh::Next() % 14);
    B(at::kBoardStep) = static_cast<unsigned char>(sh::Next() % 8);
    B(at::kCursorMode) = static_cast<unsigned char>(PickOf(0, 1, 2, sh::Next()));
    B(at::kCursorA) = static_cast<unsigned char>(PickOf(0, 1, sh::Next()));
    B(at::kCursorB) = static_cast<unsigned char>(PickOf(1, 2, 4, 0, 5, sh::Next()));
    SetW(at::kKeptHelp, sh::Next());
    SetW(at::kHelp, PickOf(0x3D, 0x3D, 0xFFFF, 0xB5, sh::Next() & 0x1FF));
    B(at::kToggle) = static_cast<unsigned char>(PickOf(0, 1, sh::Next()));
    B(at::kPickC) = static_cast<unsigned char>(PickOf(0, 1, sh::Next()));
    B(at::kPickB) = static_cast<unsigned char>(PickOf(0, 1, 2, 0xFF, sh::Next()));
    B(at::kPick) = static_cast<unsigned char>(PickOf(0, 1, 8, 9, 11, 0xFF, sh::Next() % 12, sh::Next()));
    B(at::kListCursor) = static_cast<unsigned char>(PickOf(0, 1, 2, 0xFF, sh::Next() % 5, sh::Next()));
    B(at::kKeptStep) = static_cast<unsigned char>(sh::Next());
    B(at::kPickE) = static_cast<unsigned char>(PickOf(0xFF, 0xFF, 0, 1, sh::Next() % 6));
    B(at::kPickD) = static_cast<unsigned char>(PickOf(0xFF, 0xFF, 0, 1, sh::Next() % 6));
    B(at::kSavedB) = static_cast<unsigned char>(sh::Next());
    B(at::kSavedA) = static_cast<unsigned char>(sh::Next());
    B(at::kSavedMode) = static_cast<unsigned char>(sh::Next() % 11);
    B(at::kCountA) = static_cast<unsigned char>(sh::Next());
    B(at::kListCount) = static_cast<unsigned char>(PickOf(0, 1, 4, 5, 6, sh::Next() % 9));
    B(at::kCountC) = static_cast<unsigned char>(sh::Next());
    SetW(sa::kArea, PickOf(0xAF, 0xB0, 0xB4, 0xB9, sh::Next() % 0x100, sh::Next()));
    B(at::kRequest) = static_cast<unsigned char>(PickOf(0, 2, 2, 1, sh::Next()));
    SetW(at::kMessageIndex, PickOf(0xF9, 0xF8, sh::Next()));
    B(at::kScriptFlags) = static_cast<unsigned char>(PickOf(0, 0, sh::Next()));
    B(at::kBusyA) = static_cast<unsigned char>(PickOf(0, 0, sh::Next()));
    B(at::kBusyB) = static_cast<unsigned char>(PickOf(0, 0, sh::Next()));
    B(at::kSoundBankBits) = static_cast<unsigned char>(sh::Next());
    // the clock around the bounds of CommuTail_TimedGift's table from the argument's record
    static const U kSteps[] = {0, 1, 2, 3, 4, 7, 8, 0xB, 0x10, 0x15, 0x1A, 0x1F, 0x29, 0x33, 0x3D, 0x47, 0x4D, 0x4E, 0x51, 0x5B, 0xC9, 0x1F4};
    const U start = move_script::Long(sh::Mem(Rec(B(at::kTailArg) % 60) + 4));
    const U step = kSteps[sh::Next() % (sizeof kSteps / sizeof kSteps[0])];
    SetL(at::kClock, PickOf(start + step, start + step - 1, start + step, sh::Next()));
}

void Seed(unsigned k) {
    SeedButtons();
    SeedRecords();
    SeedSlots();
    SeedBoard();
    const sh::Clone& c = kAll[k];
    if (const Dispatch* d = DispatchOf(c.base)) B(d->by) = static_cast<unsigned char>(sh::Next() % d->count);
    switch (c.base) {
    case 0x457120: {   // RandomGift: the record's slot 1..8 (its level below 3), the marks 2 and 3 often
        const unsigned arg = B(at::kTailArg) % 60;
        B(at::kTailArg) = static_cast<unsigned char>(arg);
        B(Rec(arg) + 1) = static_cast<unsigned char>(sh::Next() % 8 + 1);
        B(Rec(arg) + 2) = static_cast<unsigned char>(PickOf(2, 2, 3, 3, sh::Next()));
        break;
    }
    case 0x456E80:     // TimedGift, NibbleGift: the argument within the 60
    case 0x4573C0:
        B(at::kTailArg) = static_cast<unsigned char>(B(at::kTailArg) % 60);
        if (c.base == 0x4573C0) B(Rec(B(at::kTailArg)) + 3) = static_cast<unsigned char>(PickOf(0, 0x10, 0x23, sh::Next()));
        break;
    case 0x458410:     // Confirm writes the slot: 1..8; the picks often the slot's own
        B(at::kTailArg) = static_cast<unsigned char>(sh::Next() % 8 + 1);
        if (sh::Half()) {
            const U p = SlotAt(B(at::kTailArg));
            B(at::kPick) = static_cast<unsigned char>(B(p) - 4);
            B(at::kPickB) = B(p + 1);
            B(at::kPickC) = sh::Often() ? B(p + 2) : static_cast<unsigned char>(sh::Next());
        }
        break;
    case 0x459D80: {   // SlotHelp: a kind 0 or 4..13 at the argument's slot (others read the frame above)
        const unsigned arg = B(at::kTailArg);
        if (arg >= 1 && arg <= 8)
            B(SlotAt(arg)) = static_cast<unsigned char>(PickOf(0, 4, 4 + sh::Next() % 10, 13, 5));
        break;
    }
    case 0x459C80:     // PlaceRecord, PickHelp: the target the source often
    case 0x459E50:
        if (sh::Half()) B(at::kPick) = B(at::kTailArg);
        break;
    case 0x4594A0:     // MoveGridCursor: the cells at the cursor's boundaries
        B(at::kTailArg) = static_cast<unsigned char>(PickOf(0, 1, 4, 5, 8, 9, 0xA, 0xB, sh::Next()));
        B(at::kPick) = static_cast<unsigned char>(PickOf(0, 1, 4, 5, 8, 9, 0xA, 0xB, sh::Next()));
        break;
    default: break;
    }
    // the help word on the yes / no message, so DrawPanel's tile is drawn
    if (c.base == 0x458D70 && sh::Half()) SetW(at::kHelp, 0x3D);
}

// The helpers' arguments: coordinates at boundaries or random words; a record
// below 60 mostly; a slot 0..11 mostly; the colour row below 4; the cells the
// cursor functions are handed.
void Args(unsigned k, U* a) {
    const U base = kAll[k].base;
    const auto coordinate = [](U r) { return PickOf(r, r & 0xFFFF, 0, 0x14, 0xFFFF, 0x8000, 0x7FFF, r % 0x140); };
    const auto byte = [](U r, U v) { return (r & 0xFFFFFF00u) | v; };
    switch (base) {
    case 0x458830:   // DrawRecordCard(x, y, record, highlight)
        a[0] = coordinate(a[0]);
        a[1] = coordinate(a[1]);
        a[2] = byte(a[2], sh::Often() ? sh::Next() % 60 : sh::Next() & 0xFF);
        a[3] = byte(a[3], PickOf(0, 1, sh::Next() & 0xFF));
        break;
    case 0x458AE0:   // DrawCardFrame(x, y), DrawPanelFrame(x, y)
    case 0x4591A0:
        a[0] = coordinate(a[0]);
        a[1] = coordinate(a[1]);
        break;
    case 0x458B90:   // DrawBar(x, y, w, row, flash)
        a[0] = coordinate(a[0]);
        a[1] = coordinate(a[1]);
        a[2] = PickOf(0, 12, 0x3C, 0xFFFF, a[2]);
        a[3] = byte(a[3], sh::Next() % 4);
        a[4] = byte(a[4], PickOf(0, 1, sh::Next() & 0xFF));
        break;
    case 0x459250:   // DrawSlotLines(slot, steady)
        a[0] = byte(a[0], PickOf(0, 1, 4, 8, 9, 11, sh::Next() % 12, sh::Next() & 0xFF));
        a[1] = byte(a[1], PickOf(0, 1, sh::Next() & 0xFF));
        break;
    case 0x459430:   // CountInSlot(slot), NthInSlot(slot, nth)
    case 0x459460:
        a[0] = byte(a[0], PickOf(0, 1, 9, sh::Next() % 12, sh::Next() & 0xFF));
        a[1] = byte(a[1], PickOf(0, 1, 2, sh::Next() % 8, 0xFF));
        break;
    case 0x4594A0:   // MoveGridCursor(cell), PickHelp(cell)
    case 0x459E50:
        a[0] = sh::Half() ? at::kTailArg : at::kPick;   // both seeded (Seed)
        break;
    case 0x459720:   // DrawDigits(x, y, n), DrawSprite(x, y, id), DrawListBox(x, y, steady), DrawListFrame(x, y, h)
    case 0x4597E0:
    case 0x4598A0:
    case 0x459960:
        a[0] = coordinate(a[0]);
        a[1] = coordinate(a[1]);
        a[2] = byte(a[2], PickOf(0, 1, 7, 8, 9, 0x10, 0x40, sh::Next() & 0xFF));
        break;
    case 0x459A80:   // DrawListBoxB(x, y, list, steady)
        a[0] = coordinate(a[0]);
        a[1] = coordinate(a[1]);
        a[2] = byte(a[2], PickOf(0, 1, 2, 3, 4, 5, sh::Next() & 0xFF));
        a[3] = byte(a[3], PickOf(0, 1, sh::Next() & 0xFF));
        break;
    case 0x459B70:   // SlotRecordXY(px, py, slot, k)
        a[2] = byte(a[2], PickOf(0, 1, 4, 5, 8, sh::Next() % 12, sh::Next() & 0xFF));
        a[3] = byte(a[3], PickOf(0, 1, 2, 3, sh::Next() & 0xFF));
        break;
    case 0x459C80:   // PlaceRecord(back)
        a[0] = byte(a[0], PickOf(0, 1, sh::Next() & 0xFF));
        break;
    default: break;
    }
}

// What the functions read again after a call, moved by the group's case of the
// harness's disturbance (from its hash only).
void Disturb(U h) { Move(h); }

}  // namespace

void SelfTest() {
    static sh::Clone chosen[kCount];
    static unsigned index[kCount];
    const char* const only = std::getenv("BOF3X_R4B_ONLY");
    unsigned n = 0;
    for (unsigned k = 0; k < kCount; ++k)
        if (!only || !*only || std::strstr(kAll[k].name, only)) {
            index[n] = k;
            chosen[n++] = kAll[k];
        }
    if (n == 0) bof3::Fatal("rest_4b: BOF3X_R4B_ONLY=%s names no clone", only);
    static unsigned* s_index = index;
    sh::Group g = {"rest_4b", chosen, n, kCallees, sizeof kCallees / sizeof kCallees[0], kTables,
                   sizeof kTables / sizeof kTables[0], kRegions, sizeof kRegions / sizeof kRegions[0],
                   [](unsigned k) { Seed(s_index[k]); }, &Disturb, 6000};
    g.args = [](unsigned k, U* a) { Args(s_index[k], a); };
    g.field = true;
    sh::Run(g);
}

}  // namespace rest_4b

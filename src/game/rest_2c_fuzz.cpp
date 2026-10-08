// BOF3X_SHADOW=rest_2c: group R2C's 61 functions through the scenario harness
// (scenario_harness.h, used unchanged) in field mode, once at start-up.
// docs/rest_2c.md section 4. BOF3X_R2C_ONLY=<name> runs the clones whose name
// contains it (the controls' speed-up).
//
// The clone rows are tools/band_rows.py --group R2C --clones --harness scenario
// (2026-10-04), each extent read again to its last instruction (capstone); the
// cut's sizes are padding past them. 0x583350 (ShopMode_States[9]) is a start
// no list had (docs/rest_2c.md section 2). Shapes: the menu block's states and
// dispatchers kMenu, the master's and the figure's kState, the draw helpers
// and the block builder kCall with their arguments set by Args,
// MasterPanel_ExpForLevel answering a whole eax. The fourteen .data tables the
// dispatchers read are DataTables (their entries recorders while the fuzz
// runs); each dispatcher's index byte is seeded below its own table's count.
// Every callee the standard set types otherwise is re-listed here (registered
// before the standard rows: the group's listing stands) with what the callee
// reads.
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/rest_2c.h"
#include "game/rest_2c_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace rest_2c {
namespace {

namespace sh = scenario_harness;
using U = std::uint32_t;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
template <typename... T> U PickOf(T... v) {
    const U values[] = {static_cast<U>(v)...};
    return sh::Pick(values, sizeof...(v));
}
unsigned char& B(U a) { return *sh::Mem(a); }
void SetW(U a, U v) { SetWord(sh::Mem(a), v); }
void SetL(U a, U v) { SetLong(sh::Mem(a), static_cast<std::int32_t>(v)); }
U Win(unsigned n, unsigned off) { return at::kWindows + n * at::kWindowStride + off; }

// --- the clone table (band_rows.py --group R2C --clones --harness scenario, 2026-10-04) ---
constexpr sh::CallSite kCalls57F340[] = {{0xAF, 0x57EEF0}};
constexpr sh::CallSite kCalls57F420[] = {{0x29, 0x57F340}};
constexpr sh::CallSite kCalls57F450[] = {{0xE, 0x5720C0}, {0x33, 0x5720C0}, {0x72, 0x57F340}, {0x80, 0x57F340}};
constexpr sh::CallSite kCalls57F4E0[] = {{0x0, 0x57F340}};
constexpr sh::CallSite kCalls57F4F0[] = {{0x7, 0x57F340}};
constexpr sh::CallSite kCalls57FA40[] = {{0x4, 0x5808E0}, {0x1A, 0x574AB0}, {0x38, 0x574610}, {0x45, 0x497740}, {0x56, 0x516B30}};
constexpr sh::CallSite kCalls580010[] = {{0x11, 0x574AB0}, {0x1B, 0x497740}, {0x2C, 0x516B30}, {0x37, 0x588D20},
                                         {0x3C, 0x5806F0}, {0x52, 0x5B9380}, {0x64, 0x454870}};
constexpr sh::CallSite kCalls5800D0[] = {{0x11, 0x574AB0}, {0x1B, 0x497740}, {0x2C, 0x516B30}, {0x37, 0x588D20}, {0x6A, 0x587740}};
constexpr sh::CallSite kCalls580230[] = {{0x11, 0x574AB0}};
constexpr sh::CallSite kCalls580280[] = {{0x25, 0x574AB0}};
constexpr sh::CallSite kCalls580560[] = {{0x0, 0x580970}, {0x7, 0x587910}};
constexpr sh::CallSite kCalls580590[] = {{0x0, 0x580970}, {0x19, 0x587A00}, {0x32, 0x495040}};
constexpr sh::CallSite kCalls5805D0[] = {{0xC, 0x580630}};
constexpr sh::CallSite kCalls5806F0[] = {{0xEF, 0x5B9450}, {0xFF, 0x5B9450}, {0x17E, 0x57C140}};
constexpr sh::CallSite kCalls5809C0[] = {{0x16, 0x5B9380}, {0x1B, 0x5806F0}, {0x30, 0x5B9380}, {0x3F, 0x454870}};
constexpr sh::CallSite kCalls583370[] = {{0x0, 0x5836E0}, {0x1F, 0x587740}};
constexpr sh::CallSite kCalls5833E0[] = {{0x5E, 0x461EB0}, {0x139, 0x587740}, {0x16A, 0x587740}, {0x174, 0x587740},
                                         {0x17C, 0x583770}, {0x1A9, 0x587740}, {0x1C2, 0x587740}, {0x1CC, 0x587740}};
constexpr sh::CallSite kCalls5835F0[] = {{0x45, 0x587740}, {0x4F, 0x587740}};
constexpr sh::CallSite kCalls585A20[] = {{0x5, 0x587740}};
constexpr sh::CallSite kCalls585A50[] = {{0x46, 0x585DC0}, {0x62, 0x585BE0}, {0xA3, 0x586160}};
constexpr sh::CallSite kCalls585BE0[] = {{0x23, 0x57CF60}, {0x52, 0x516B30}, {0x68, 0x5B9380}, {0x7C, 0x517090}, {0x95, 0x516B30},
                                         {0xAB, 0x5B9380}, {0xBF, 0x517090}, {0xD8, 0x516B30}, {0xF1, 0x5B9380}, {0x105, 0x517090},
                                         {0x11E, 0x516B30}, {0x134, 0x5B9380}, {0x148, 0x517090}, {0x163, 0x497740}, {0x16D, 0x591940},
                                         {0x174, 0x497740}, {0x186, 0x516B30}, {0x197, 0x57D910}, {0x1B3, 0x57D910}, {0x1CB, 0x57D910}};
constexpr sh::CallSite kCalls585DC0[] = {{0x20, 0x57CF60}, {0x59, 0x586570}, {0x68, 0x516B30}, {0x7D, 0x5B9380}, {0x94, 0x517090},
                                         {0x102, 0x516E70}, {0x139, 0x5B9380}, {0x14D, 0x517090}, {0x163, 0x5B9380}, {0x174, 0x517090},
                                         {0x1B1, 0x5B9380}, {0x1C5, 0x517090}, {0x1DB, 0x5B9380}, {0x1EC, 0x517090}, {0x1FA, 0x57D910},
                                         {0x217, 0x586030}, {0x22B, 0x5A77C0}, {0x234, 0x461E50}, {0x248, 0x5A79E0}, {0x262, 0x59DB70}};
constexpr sh::CallSite kCalls586030[] = {{0xE, 0x5A7650}, {0x16, 0x5A7780}, {0x56, 0x586110}, {0x65, 0x586110}, {0xC4, 0x461E50}};
constexpr sh::CallSite kCalls586160[] = {{0x3D, 0x5A77C0}, {0x46, 0x461E50}, {0x60, 0x5A75D0}, {0x68, 0x5A7780}, {0x141, 0x5A79E0},
                                         {0x154, 0x461E50}, {0x181, 0x5A75D0}, {0x189, 0x5A7780}, {0x20E, 0x5A79E0}, {0x221, 0x461E50},
                                         {0x22D, 0x5A75D0}, {0x235, 0x5A7780}, {0x2B7, 0x5A79E0}, {0x2CA, 0x461E50}, {0x2D6, 0x5A75D0},
                                         {0x2DE, 0x5A7780}, {0x376, 0x5A79E0}, {0x38C, 0x461E50}, {0x3C8, 0x5A77C0}, {0x3D1, 0x461E50},
                                         {0x3F4, 0x57D420}};
constexpr sh::CallSite kCalls586570[] = {{0x3C, 0x5A77C0}, {0x45, 0x461E50}, {0x51, 0x5A7710}, {0xEE, 0x461E50}};
constexpr sh::CallSite kCalls586680[] = {{0xB, 0x57C140}, {0x2A, 0x4976D0}};
constexpr sh::CallSite kCalls5866E0[] = {{0xB, 0x57C0F0}, {0x21, 0x4976D0}};
constexpr sh::CallSite kCalls586720[] = {{0x14, 0x57C0F0}};
constexpr sh::CallSite kCalls586760[] = {{0x14, 0x4976D0}};
constexpr sh::CallSite kCallsSay[] = {{0x1D, 0x4976D0}};       // 0x586790, 0x586860, 0x586930
constexpr sh::CallSite kCallsCheck[] = {{0x6B, 0x4976D0}};     // 0x5867D0, 0x5868A0
constexpr sh::CallSite kCalls586980[] = {{0x16, 0x5869A0}};

#define R2C_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define R2C_ROW(name, base, size, calls) #name, base, size, calls, R2C_N(calls), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::name)
#define R2C_LEAF(name, base, size) #name, base, size, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::name)
constexpr sh::Shape kM = sh::Shape::kMenu, kS = sh::Shape::kState, kC = sh::Shape::kCall;
const sh::Clone kAll[] = {
    {R2C_ROW(MasterFigure_DrawFaded, 0x57F340, 0xD5, kCalls57F340), 0, false, kS},
    {R2C_ROW(MasterFigure_TurnHome, 0x57F420, 0x2E, kCalls57F420), 0, false, kS},
    {R2C_ROW(MasterFigure_Settle, 0x57F450, 0x87, kCalls57F450), 0, false, kS},
    {R2C_ROW(MasterFigure_Hold, 0x57F4E0, 0x5, kCalls57F4E0), 0, false, kS},
    {R2C_ROW(MasterFigure_TurnOn, 0x57F4F0, 0xC, kCalls57F4F0), 0, false, kS},
    {R2C_ROW(InnPrompt_NotEnough, 0x57FA40, 0x6F, kCalls57FA40), 0, false, kM},
    {R2C_ROW(FieldSave_Write, 0x580010, 0xB1, kCalls580010), 0, false, kM},
    {R2C_ROW(FieldSave_Written, 0x5800D0, 0x7F, kCalls5800D0), 0, false, kM},
    {R2C_ROW(FieldSave_PromptAnswer, 0x580230, 0x4D, kCalls580230), 0, false, kM},
    {R2C_ROW(Inn_TitleOut, 0x580280, 0x77, kCalls580280), 0, false, kM},
    {R2C_LEAF(Rest_Dispatch, 0x580300, 0xE), 0, false, kM},
    {R2C_ROW(Rest_LoadJingle, 0x580560, 0x23, kCalls580560), 0, false, kM},
    {R2C_ROW(Rest_WaitJingle, 0x580590, 0x39, kCalls580590), 0, false, kM},
    {R2C_ROW(Rest_Restore, 0x5805D0, 0x21, kCalls5805D0), 0, false, kM},
    {R2C_LEAF(Rest_End, 0x580600, 0xF), 0, false, kM},
    {R2C_LEAF(Rest_EndAfterMessage, 0x580610, 0x18), 0, false, kM},
    {R2C_ROW(Save_BuildBlock, 0x5806F0, 0x1E9, kCalls5806F0), 0, false, kC},
    {R2C_ROW(Save_QuickWrite, 0x5809C0, 0x72, kCalls5809C0), 0, false, kC},
    {R2C_LEAF(PartyForm_Dispatch, 0x580A40, 0xE), 0, false, kM},
    {R2C_LEAF(ShopSell_Dispatch, 0x582EB0, 0xE), 0, false, kM},
    {R2C_LEAF(ShopSell_Open, 0x582EC0, 0xED), 0, false, kM},
    {R2C_LEAF(ShopSell_Leave, 0x582FB0, 0x33), 0, false, kM},
    {R2C_LEAF(ShopSell_SellStep, 0x582FF0, 0xE), 0, false, kM},
    {R2C_LEAF(ShopSell_LeaveWait, 0x583000, 0x16), 0, false, kM},
    {R2C_LEAF(ShopBrowse_Dispatch, 0x583350, 0xE), 0, false, kM},
    {R2C_LEAF(ShopBrowse_OpenStep, 0x583360, 0xE), 0, false, kM},
    {R2C_ROW(ShopBrowse_Open, 0x583370, 0x26, kCalls583370), 0, false, kM},
    {R2C_LEAF(ShopBrowse_OpenWait, 0x5833A0, 0x22), 0, false, kM},
    {R2C_LEAF(ShopBrowse_ChooseStep, 0x5833D0, 0xE), 0, false, kM},
    {R2C_ROW(ShopBrowse_Choose, 0x5833E0, 0x204, kCalls5833E0), 0, false, kM},
    {R2C_ROW(ShopBrowse_Detail, 0x5835F0, 0x85, kCalls5835F0), 0, false, kM},
    {R2C_LEAF(ShopBrowse_DetailClose, 0x583680, 0x16), 0, false, kM},
    {R2C_LEAF(ShopBrowse_CloseStep, 0x5836A0, 0xE), 0, false, kM},
    {R2C_LEAF(ShopBrowse_CloseNext, 0x5836B0, 0x7), 0, false, kM},
    {R2C_LEAF(ShopBrowse_End, 0x5836C0, 0x14), 0, false, kM},
    {R2C_LEAF(ShopResist_Dispatch, 0x5837E0, 0xE), 0, false, kM},
    {R2C_LEAF(SharedList_Dispatch, 0x584180, 0xE), 0, false, kM},
    {R2C_LEAF(MasterTalk_Reset, 0x585A00, 0x1C), 0, false, kS},
    {R2C_ROW(MasterTalk_PanelsOpen, 0x585A20, 0x21, kCalls585A20), 0, false, kS},
    {R2C_ROW(MasterTalk_PanelsIn, 0x585A50, 0xC9, kCalls585A50), 0, false, kS},
    {R2C_ROW(MasterTalk_PanelsOut, 0x585B20, 0xBC, kCalls585A50), 0, false, kS},
    {R2C_ROW(MasterPanel_DrawStats, 0x585BE0, 0x1D8, kCalls585BE0), 0, false, kC},
    {R2C_ROW(MasterPanel_DrawMember, 0x585DC0, 0x26F, kCalls585DC0), 0, false, kC},
    {R2C_ROW(MasterPanel_DrawExpBar, 0x586030, 0xD4, kCalls586030), 0, false, kC},
    {R2C_LEAF(MasterPanel_ExpForLevel, 0x586110, 0x45), 0xFFFFFFFFu, false, kC},
    {R2C_ROW(Menu_DrawPanelBox, 0x586160, 0x404, kCalls586160), 0, false, kC},
    {R2C_ROW(MasterPanel_DrawFace, 0x586570, 0xF9, kCalls586570), 0, false, kC},
    {R2C_LEAF(MasterTalk_Dispatch, 0x586670, 0xE), 0, false, kS},
    {R2C_ROW(MasterTalk_Begin, 0x586680, 0x49, kCalls586680), 0, false, kS},
    {R2C_LEAF(MasterTalk_IntroStep, 0x5866D0, 0xE), 0, false, kS},
    {R2C_ROW(MasterTalk_IntroSay, 0x5866E0, 0x3D, kCalls5866E0), 0, false, kS},
    {R2C_ROW(MasterTalk_IntroWait, 0x586720, 0x2B, kCalls586720), 0, false, kS},
    {R2C_LEAF(MasterTalk_AskStep, 0x586750, 0xE), 0, false, kS},
    {R2C_ROW(MasterTalk_Say5, 0x586760, 0x30, kCalls586760), 0, false, kS},
    {R2C_ROW(MasterTalk_Say6, 0x586790, 0x39, kCallsSay), 0, false, kS},
    {R2C_ROW(MasterTalk_CheckAllPupils, 0x5867D0, 0x88, kCallsCheck), 0, false, kS},
    {R2C_ROW(MasterTalk_Say8, 0x586860, 0x39, kCallsSay), 0, false, kS},
    {R2C_ROW(MasterTalk_CheckAnyPupil, 0x5868A0, 0x88, kCallsCheck), 0, false, kS},
    {R2C_ROW(MasterTalk_SayFarewell, 0x586930, 0x3B, kCallsSay), 0, false, kS},
    {R2C_LEAF(MasterTalk_PickStep, 0x586970, 0xE), 0, false, kS},
    {R2C_ROW(MasterTalk_PickAsk, 0x586980, 0x1F, kCalls586980), 0, false, kS},
};
#undef R2C_ROW
#undef R2C_LEAF
#undef R2C_N
constexpr unsigned kCount = sizeof kAll / sizeof kAll[0];
static_assert(kCount == 61, "the cut's 60 rows for R2C and 0x583350");

// A dispatcher's index byte and its table's count (its reader's reach,
// docs/rest_2c.md section 3).
struct Dispatch { U base, by; unsigned count; };
const Dispatch kDispatch[] = {
    {0x580300, at::kState, 7},     {0x580A40, at::kState, 4},     {0x582EB0, at::kState, 4},
    {0x582FF0, at::kStep, 5},      {0x583350, at::kState, 3},     {0x583360, at::kStep, 2},
    {0x5833D0, at::kStep, 3},      {0x5836A0, at::kStep, 3},      {0x5837E0, at::kState, 8},
    {0x584180, at::kState, 5},     {0x586670, at::kTalkMode, 7},  {0x5866D0, at::kTalkStep, 2},
    {0x586750, at::kTalkStep, 6},  {0x586970, at::kTalkStep, 9},
};
const Dispatch* DispatchOf(U base) {
    for (const Dispatch& d : kDispatch)
        if (d.base == base) return &d;
    return nullptr;
}

const sh::DataTable kTables[] = {
    {at::kRestStates, 7},        {at::kPartyFormStates, 4},  {at::kShopSellStates, 4},  {at::kShopSellSellSteps, 5},
    {at::kBrowseStates, 3},      {at::kBrowseOpenSteps, 2},  {at::kBrowseChooseSteps, 3}, {at::kBrowseCloseSteps, 3},
    {at::kResistStates, 8},      {at::kSharedListStates, 5}, {at::kTalkStates, 7},      {at::kTalkIntroSteps, 2},
    {at::kTalkAskSteps, 6},      {at::kTalkPickSteps, 9},
};

// --- the moves (from a hash: the harness's Noise() or Disturb's) -----------------------
//
// What the functions read again after a call: the menu block's state, step and
// timer, the slot (kept below 16), window 1's cursor and first shown, the
// pressed word, the master's step and slide, the member count (kept to 3) and
// their record indexes, Field_Request, the figure's bytes, a record's level and
// experience.
void Move(U h) {
    const unsigned v = (h >> 8) & 0xFF;
    switch (h % 18) {
    case 16: B(at::kRecords + ((h >> 16) % 8) * at::kRecordStride + 0xA) = static_cast<unsigned char>(v); break;
    case 17: SetL(at::kRecords + ((h >> 16) % 8) * at::kRecordStride + 0xC, h * 0x9E3779B1u); break;
    case 0: B(at::kState) = static_cast<unsigned char>(v); break;
    case 1: B(at::kStep) = static_cast<unsigned char>(v); break;
    case 2: B(at::kTimer) = static_cast<unsigned char>(v & 1 ? v >> 1 : (v >> 1) % 3); break;
    case 3: SetL(at::kSlot, v % at::kSlotCount); break;
    case 4: B(Win(1, 0xB)) = static_cast<unsigned char>(v % 0x12); break;
    case 5: B(Win(1, 0xA)) = static_cast<unsigned char>(v % 10); break;
    case 6: SetW(at::kPressed, h >> 16); break;
    case 7: B(at::kTalkStep) = static_cast<unsigned char>(v); break;
    case 8: B(at::kTalkSlide) = static_cast<unsigned char>(v % 6); break;
    case 9: B(at::kMemberCount) = static_cast<unsigned char>(v % 4); break;
    case 10: B(at::kMembers + (v % 3) * at::kObjStride) = static_cast<unsigned char>((v >> 2) % 8); break;
    case 11: B(at::kRequest) = static_cast<unsigned char>(v & 1 ? 2 : v >> 1); break;
    case 12: B(at::kFigureRgb + v % 3) = static_cast<unsigned char>(h >> 16); break;
    case 13: B(at::kMaster) = static_cast<unsigned char>(v % 18); break;
    case 14: B(Win(1, 0xD)) = static_cast<unsigned char>(v & 1 ? 0xFF : v); break;
    case 15: SetL(at::kFigureY, h); break;
    default: break;
    }
}

U Stir(const U*, U answer) {
    Move(sh::Noise());
    return answer;
}
// R2B's 0x57EEF0 draws the record it is handed: its bytes (the faded colour,
// the scale) are logged at the call, as the draw would see them.
U FigureDraw(const U*, U answer) {
    sh::NoteBytes(sh::Mem(at::kFigure), 0x110);
    Move(sh::Noise());
    return answer;
}
// Msg_SystemPtr's answer lands in the harness's text buffer (the standard set's
// FxText); the callers compare the pointer.
U TextAnswer(const U*, U answer) { return Key(sh::Text() + (answer & 0xF0)); }
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
// Save_WriteFile: -1 a third of the time (the failure path), else a handle.
U WriteAnswer(const U*, U) {
    const U n = sh::Noise();
    Move(n >> 4);
    return n % 3 == 0 ? 0xFFFFFFFFu : n % 16;
}
// Input_AutoRepeat: each key the choose step tests, alone, together and none.
U KeysAnswer(const U*, U answer) {
    const U n = sh::Noise();
    static const U kKeys[] = {0x1000, 0x4000, 0x0004, 0x0008, 0, 0x5000, 0x100C, 0x400C, 0x000C};
    Move(n >> 8);
    return n % 6 == 0 ? answer : kKeys[(n >> 3) % (sizeof kKeys / sizeof kKeys[0])];
}
// AreaMap_Elevation: a low word from a small set (the seed puts the figure's
// height around the target it makes) with garbage above; the settle reads it as
// a short. One height a round, so the seed can put the height exactly where
// the fall lands on the target.
constexpr U kHeights[] = {0, 0x40, 0xFFC0, 0x100, 0x7FFF, 0x8000};
unsigned g_height;   // the round's height (Seed's choice): both calls answer it
U ElevationAnswer(const U*, U answer) { return (answer & 0xFFFF0000u) | kHeights[g_height]; }
// MasterPanel_ExpForLevel: totals from a small set, so that the two calls agree
// a third of the time (the bar's zero-length path) and straddle the experience.
U ExpAnswer(const U*, U answer) {
    const U n = sh::Noise();
    static const U kTotals[] = {0, 100, 100, 250, 0xFFFFFFFFu, 1000};
    return n % 5 == 0 ? answer : kTotals[(n >> 3) % (sizeof kTotals / sizeof kTotals[0])];
}

#define R2C_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
constexpr sh::Answer kG = sh::Answer::kGarbage;
constexpr U kW = 0xFFFFFFFFu;
const sh::Callee kCallees[] = {
    // the group's own, called directly (E8 / E9), with what each reads
    {R2C_OURS(MasterFigure_DrawFaded), 0, {}, kG, 0, 0},
    {R2C_OURS(Save_BuildBlock), 0, {}, kG, 0, 0, {}, &Stir},
    {R2C_OURS(MasterPanel_DrawMember), 4, {0xFFFF, 0xFFFF, 0xFF, 0xFF}, kG, 0, 0, {}, &Stir},
    {R2C_OURS(MasterPanel_DrawStats), 3, {0xFFFF, 0xFFFF, 0xFF}, kG, 0, 0, {}, &Stir},
    {R2C_OURS(Menu_DrawPanelBox), 5, {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFF}, kG, 0, 0},   // as Menu_DrawTitleBox: 16 bits
    {R2C_OURS(MasterPanel_DrawFace), 4, {0xFFFF, 0xFFFF, 0xFF, 0xFF}, kG, 0, 0},
    {R2C_OURS(MasterPanel_DrawExpBar), 5, {0xFFFF, 0xFFFF, 0xFF, 0xFF, kW}, kG, 0, 0},
    {R2C_OURS(MasterPanel_ExpForLevel), 2, {0xFF, 0xFF}, kG, 0, 0, {}, &ExpAnswer},
    // other groups' of this wave, by address (docs/rest_2c.md section 6)
    {"0x57EEF0", at::kFigureDraw, at::kFigureDraw, 1, {kW}, kG, 0, 0, {}, &FigureDraw},                     // R2B
    {"0x5869A0", at::kPickAsk, at::kPickAsk, 2, {0xFFFF, 0xFF}, kG, 0, 0, {}, &Stir},                 // R2D: and ecx, 0xFFFF; dl
    {"0x59DB70", at::kGlyph, at::kGlyph, 6, {0xFFFF, 0xFFFF, 0xFF, 0xFF, 0xFFFF, 0xFF}, kG, 0, 0},    // R2H
    // the C runtime's strncpy: both sides call it for real
    {R2C_OURS(Crt_strncpy), 3, {kW, kW, kW}, sh::Answer::kThrough, 0, 0},
    // ours, outside the standard set or typed otherwise, with what each reads
    {R2C_OURS(Menu_DrawTitleBox), 5, {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFF}, kG, 0, 0},
    {R2C_OURS(Menu_DrawMoneyBox), 4, {0xFFFF, 0xFFFF, 0, kW}, kG, 0, 0},
    {R2C_OURS(Menu_DrawBox), 6, {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFF, 0xFF}, kG, 0, 0},
    {R2C_OURS(Menu_DrawPieces), 4, {0xFFFF, 0xFFFF, kW, 0xFF}, kG, 0, 0, {0, 0, 16, 0}, &Stir, nullptr, true},
    {R2C_OURS(Menu_DrawOutline), 5, {kW, kW, kW, kW, kW}, kG, 0, 0},
    {R2C_OURS(Text_DrawAt), 5, {0xFFFF, 0xFFFF, 0xFF, 0xFF, kW}, kG, 0, 0},
    {R2C_OURS(Text_DrawFont8), 4, {0xFFFF, 0xFFFF, 0x3F, kW}, kG, 0, 0, {0, 0, 0, 16}, nullptr, nullptr, true},
    {R2C_OURS(Msg_SystemPtr), 1, {0xFFFF}, kG, 0, 0, {}, &TextAnswer},
    {R2C_OURS(TextRecord_Set), 3, {0xFF, 0xFF, kW}, kG, 0, 0},
    {R2C_OURS(SaveMenu_DrawChoices), 2, {kW, kW}, kG, 0, 0},
    {R2C_OURS(SaveMenu_DrawSlots), 3, {kW, kW, kW}, kG, 0, 0},
    {R2C_OURS(Save_WriteFile), 2, {kW, kW}, kG, 0, 0, {16, 0}, &WriteAnswer, nullptr, true},
    {R2C_OURS(Party_RestoreAll), 1, {kW}, kG, 0, 0, {}, &Stir},
    {R2C_OURS(ShopBrowse_InitWindows), 0, {}, kG, 0, 0, {}, &Stir},
    {R2C_OURS(ShopBrowse_OpenDetail), 0, {}, kG, 0, 0, {}, &Stir},
    {R2C_OURS(Input_AutoRepeat), 1, {kW}, kG, 0, 0, {}, &KeysAnswer},
    {R2C_OURS(Sound_PlayEffect), 1, {0xFFFF}, kG, 0, 0, {}, &Stir},
    {R2C_OURS(AreaMap_Elevation), 2, {kW, kW}, kG, 0, 0, {}, &ElevationAnswer},
    // every format here takes one number: three words (the standard row logs a
    // fourth, the caller's stack)
    {"Crt_sprintf", 0x5B9380, KeyOf(&::Crt_sprintf), 3, {kW, kW, kW}, kG, 0, 0, {0, 16}, &Sprintf, nullptr, true},
};
#undef R2C_OURS

// Beyond the harness's field regions (which hold the menu block, the style byte
// and records 0x903A70..0x903A93, Cond_Flags with the story flags, the party
// list and the zenny, the save block's 0x904098..0x904160 and
// 0x904560..0x904700, ObjTrio with Field_Members, the text scratch, the packet
// cursor, the camera turn's cells with Field_InputFlags, the buttons'
// 0x90358C..0x904593).
const sh::Region kRegions[] = {
    {at::kWindows, 22 * at::kWindowStride},       // WindowRecords 0x803160..0x803477
    {0x6BC880, 0x48},                             // the save / inn bytes, the sell flags
    {0x903580, 0xC},                              // the buttons' dwords before Field_ConfirmButtons
    {at::kBlock, 0x24},                           // the save block 0x9039E0..0x903A03 (the master byte 0x9039F5)
    {0x903A94, 0x903F90 - 0x903A94},              // the records past the style region
    {0x904160, 0x400},                            // the save block 0x904160..0x904560
    {0x904700, 0x390},                            // and 0x904700..0x904A90
    {at::kStaging, at::kBlockBytes + at::kStagingClear},   // the staging copy and what it clears
    {at::kSummaries, at::kSlotCount * at::kSummaryBytes},  // the slots' summaries
    {at::kSlot, 4},                               // the save cursor
    {0x939880, 0x180},                            // the master's bytes 0x9398CD..0x9398D3, the figure record 0x9398E0..0x9399EC
    {0x7DEE40, 8},                                // the message box's flags 0x7DEE44
};

// --- the seed ----------------------------------------------------------------------------

unsigned g_clone;   // the round's clone's index in kAll

void SeedButtons() {
    // one confirm bit and one cancel bit; the pressed word hits either, both or none
    const U confirm = 1u << (sh::Next() % 16);
    U cancel = 1u << (sh::Next() % 16);
    if (cancel == confirm) cancel = confirm == 0x8000 ? 1 : confirm << 1;
    SetW(at::kConfirm, confirm | (sh::Half() ? 0 : 1u << (sh::Next() % 16)));
    SetW(at::kCancel, cancel);
    const U other = sh::Next() & 0xFFFF & ~(confirm | cancel);
    SetW(at::kPressed, PickOf(confirm, cancel, confirm | cancel, 0, 0, other, confirm | other, cancel | other));
}

void SeedMenu() {
    B(at::kTimer) = static_cast<unsigned char>(PickOf(0, 1, 1, 2, 4, 4, 5, 6, 0x1E, sh::Next()));
    B(at::kAnswer) = static_cast<unsigned char>(PickOf(0, 0, 1, sh::Next()));
    B(at::kObject) = static_cast<unsigned char>(PickOf(0xFF, 0xFF, 0xFE, sh::Next()));
    B(at::kMsgFlags) = static_cast<unsigned char>(sh::Half() ? sh::Next() | 2 : sh::Next() & ~2u);
    B(at::kInputFlags) = static_cast<unsigned char>(PickOf(0x40, 0x40, 0, 0x41, sh::Next()));
    SetW(at::kArea, PickOf(0xBC, 0x85, 0xC1, 0xBB, 0xBD, 0x84, 0x86, 0xC0, 0xC2, sh::Next()));
    SetW(at::kWait, PickOf(0, 0, sh::Next()));
    SetL(at::kSlot, sh::Next() % at::kSlotCount);
}

void SeedWindows() {
    B(Win(1, 0xA)) = static_cast<unsigned char>(PickOf(0, 1, 7, 8, 9, 10, sh::Next() % 10));
    B(Win(1, 0xB)) = static_cast<unsigned char>(PickOf(0, 1, 8, 9, 0xF, 0x10, 0x11, B(Win(1, 0xA)), B(Win(1, 0xA)) + 8u,
                                                       B(Win(1, 0xA)) + 9u, sh::Next()));
    B(Win(1, 8)) = static_cast<unsigned char>(PickOf(0, 0, 0, 0x10, sh::Next()));
    B(Win(1, 0xD)) = static_cast<unsigned char>(PickOf(0xFF, 0, 5, sh::Next()));
}

void SeedMaster() {
    B(at::kMaster) = static_cast<unsigned char>(PickOf(9, 9, 0, 1, 17, sh::Next() % 18, sh::Next() % 18));
    B(at::kRequest) = static_cast<unsigned char>(PickOf(2, 2, 0, 1, sh::Next()));
    B(at::kTalkSlide) = static_cast<unsigned char>(PickOf(0, 1, 1, 3, 4, 5, sh::Next()));
    B(at::kMemberCount) = static_cast<unsigned char>(PickOf(0, 1, 2, 3, 3));
    const unsigned char master = B(at::kMaster);
    const bool all = sh::Half();
    for (unsigned i = 0; i < 3; ++i) {
        const auto record = static_cast<unsigned char>(sh::Next() % 8);
        B(at::kMembers + i * at::kObjStride) = record;
        B(at::kRecords + 0x1F + record * at::kRecordStride) =
            static_cast<unsigned char>(all || sh::Half() ? master : PickOf(0xFF, master + 1u, sh::Next()));
    }
    // the introduction flag of the master (Flags_Test is a recorder: its row only logged)
    if (sh::Half()) B(at::kIntroFlags + master / 8) ^= static_cast<unsigned char>(0x80u >> (master % 8));
}

void SeedFigure() {
    const unsigned char step = static_cast<unsigned char>(PickOf(0, 1, 2, 0x2A, 0x55, sh::Next()));
    B(at::kFigureFade) = step;
    const U t = 3u * step;
    for (unsigned i = 0; i < 3; ++i)
        B(at::kFigureRgb + i) = static_cast<unsigned char>(PickOf(t, t + 1, t - 1, 0xFF - t, 0xFE - t, 0, 0xFF, sh::Next()));
    B(at::kFigureScaleIndex) = static_cast<unsigned char>(sh::Often() ? sh::Next() % 21 : sh::Next());
    SetL(at::kFigureAngle, PickOf(0, 0x1000, 0x40, 0xFC0, 0x1040, 0x20, sh::Next()));
    SetL(at::kFigureLift, PickOf(0, 1, 2, sh::Next() % 8, sh::Next()));
    // the height around the target for one of the elevation stand-in's heights
    g_height = sh::Next() % (sizeof kHeights / sizeof kHeights[0]);
    const U e = kHeights[g_height];
    const U lift = static_cast<U>(move_script::Long(sh::Mem(at::kFigureLift)));
    const U target = (static_cast<U>(static_cast<int>(static_cast<short>(e))) << 16) + lift * 0xA00u;
    const U fall = static_cast<U>(static_cast<int>(((static_cast<U>(static_cast<int>(static_cast<short>(e))) + 0x200u) << 16) - target) / 16);
    // y - fall lands on the target, one either side, or far
    SetL(at::kFigureY, PickOf(target + fall, target + fall, target + fall + 1, target + fall - 1, target + 0x100000,
                              target - 0x100000, sh::Next()));
}

// The eight records' panel fields at their tests: HP 0, 1, 2; AP 0, at and
// past a quarter of its maximum; the status word's bits 7, 5 and 0x2000.
void SeedRecords() {
    for (unsigned r = 0; r < 8; ++r) {
        const U rec = at::kRecords + r * at::kRecordStride;
        SetW(rec + 0x18, PickOf(0, 1, 2, sh::Next() % 1000));
        const U max_ap = PickOf(0, 4, 100, 103, sh::Next() % 1000);
        SetW(rec + 0x22, max_ap);
        SetW(rec + 0x1A, PickOf(0, 1, max_ap >> 2, (max_ap >> 2) + 1, sh::Next() % 1000));
        SetW(rec + 0x10, PickOf(0, 0x80, 0x20, 0xA0, 0x2000, 0x20A0, sh::Next()));
        B(rec + 0xA) = static_cast<unsigned char>(PickOf(1, 50, 98, 99, 100, sh::Next()));
    }
}

void Seed(unsigned k) {
    g_clone = k;
    SeedRecords();
    const sh::Clone& c = kAll[k];
    SeedButtons();
    SeedMenu();
    SeedWindows();
    SeedMaster();
    SeedFigure();
    B(at::kStoryByte) = static_cast<unsigned char>(PickOf(5, 0x85, 0xC, 0x8C, 0, sh::Next()));
    B(at::kPartyList) = static_cast<unsigned char>(PickOf(4, 4, 0, 1, 2, sh::Next() % 24));
    B(at::kPartyList + 1) = static_cast<unsigned char>(PickOf(4, 0, sh::Next() % 24));
    B(at::kPartyList + 2) = static_cast<unsigned char>(PickOf(4, 3, sh::Next() % 24));
    if (const Dispatch* d = DispatchOf(c.base)) B(d->by) = static_cast<unsigned char>(sh::Next() % d->count);
}

// The helpers' arguments: coordinates at boundaries or random words; a record
// byte 0..7 mostly (the records lie inside the regions) under random upper
// bytes; levels around 0, 99, 100 and 0xFF.
void Args(unsigned k, U* a) {
    const U base = kAll[k].base;
    const U hi = a[9] & 0xFFFFFF00u;
    const auto coordinate = [](U r) { return PickOf(r, r & 0xFFFF, 0, 0x14, 0xFFFF, 0x10000, 0x8000, r % 0x140); };
    switch (base) {
    case 0x585BE0:   // DrawStats(x, y, record)
    case 0x585DC0:   // DrawMember(x, y, record, slot)
    case 0x586030:   // DrawExpBar(x, y, record, level, exp)
        a[0] = coordinate(a[0]);
        a[1] = coordinate(a[1]);
        a[2] = hi | (sh::Often() ? sh::Next() % 8 : sh::Next() & 0xFF);
        if (base == 0x585DC0) a[3] = (a[3] & 0xFFFFFF00u) | PickOf(0, 1, 2, 0xFD, sh::Next());
        if (base == 0x586030) {
            a[3] = (a[3] & 0xFFFFFF00u) | PickOf(0, 1, 50, 98, 99, 100, 0xFF, sh::Next());
            a[4] = PickOf(0, 100, 250, 1000, 0xFFFFFFFFu, a[4]);
        }
        break;
    case 0x586110:   // ExpForLevel(record, level)
        a[0] = hi | (sh::Often() ? sh::Next() % 8 : sh::Next() & 0xFF);
        a[1] = (a[1] & 0xFFFFFF00u) | PickOf(0, 1, 2, 50, 98, 99, 100, 0xFF, sh::Next());
        break;
    case 0x586160:   // DrawPanelBox(x, y, w, h, colour)
        a[0] = coordinate(a[0]);
        a[1] = coordinate(a[1]);
        a[2] = PickOf(0x118, 0, 1, 2, 3, 4, 5, 0xFFFF, 0x10003, a[2]);
        a[3] = PickOf(0x13, 0, 1, 2, 3, 4, 0xFF, 0x100, a[3]);
        break;
    case 0x586570:   // DrawFace(x, y, id, shade)
        a[0] = coordinate(a[0]);
        a[1] = coordinate(a[1]);
        a[2] = (a[2] & 0xFFFFFF00u) | PickOf(4, 4, 0xB, 0, 1, 11, 12, sh::Next());
        a[3] = (a[3] & 0xFFFFFF00u) | PickOf(0, 1, 2, 2, sh::Next());
        break;
    default: break;
    }
}

// What the functions read again after a call, moved by the group's case of the
// harness's disturbance (from its hash only).
void Disturb(U h) { Move(h); }

}  // namespace

void SelfTest() {
    // BOF3X_R2C_ONLY: the clones whose name contains it (a control's run)
    static sh::Clone chosen[kCount];
    static unsigned index[kCount];
    const char* const only = std::getenv("BOF3X_R2C_ONLY");
    unsigned n = 0;
    for (unsigned k = 0; k < kCount; ++k)
        if (!only || !*only || std::strstr(kAll[k].name, only)) {
            index[n] = k;
            chosen[n++] = kAll[k];
        }
    if (n == 0) bof3::Fatal("rest_2c: BOF3X_R2C_ONLY=%s names no clone", only);
    static unsigned* s_index = index;
    sh::Group g = {"rest_2c", chosen, n, kCallees, sizeof kCallees / sizeof kCallees[0], kTables,
                   sizeof kTables / sizeof kTables[0], kRegions, sizeof kRegions / sizeof kRegions[0],
                   [](unsigned k) { Seed(s_index[k]); }, &Disturb, 4000};
    g.args = [](unsigned k, U* a) { Args(s_index[k], a); };
    g.field = true;
    sh::Run(g);
}

}  // namespace rest_2c

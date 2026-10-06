// BOF3X_SHADOW=rest_4e: group R4E's 48 functions through the scenario harness's
// field mode (scenario_harness.h, used unchanged; docs/scenario_harness.md
// section 7), once at start-up. docs/rest_4e.md section 4.
// BOF3X_R4E_ONLY=<name> runs the clones whose name contains it (the controls).
//
// The clone table is tools/band_rows.py --group R4E --clones --harness scenario
// (2026-10-05, through the scratch wrapper band14.py) with the names given;
// every extent and call site agrees with the capstone read (25 extents differ
// from the cut's sizes by padding only). The facility's modes and steps are
// kState (their dispatchers' indexes seeded below their own tables), the draws
// and helpers kCall.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/rest_4e.h"
#include "game/rest_4e_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace rest_4e {
namespace {

namespace sh = scenario_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using sh::Shape;
using U = std::uint32_t;

// band_rows.py's call sites (2026-10-05), each checked against the capstone read.
constexpr sh::CallSite kCalls45E870[] = {{0x1D, 0x57CF60}, {0x24, 0x45E9B0}, {0x65, 0x516B30},
                                         {0x74, 0x5A7740}, {0xBB, 0x461E50}, {0xE2, 0x45EA60},
                                         {0xFD, 0x45EA60}, {0x118, 0x45EA60}, {0x133, 0x45EA60}};
constexpr sh::CallSite kCalls45E9B0[] = {{0x10, 0x45EC00}, {0x29, 0x45EC00}, {0x36, 0x45EC00}, {0x4F, 0x45EC00},
                                         {0x64, 0x45EC00}, {0x71, 0x45EC00}, {0x87, 0x45EC00}, {0x94, 0x45EC00}};
constexpr sh::CallSite kCalls45EA60[] = {{0x44, 0x5A7610}, {0x10D, 0x461E50}, {0x119, 0x5A7610}, {0x18F, 0x461E50}};
constexpr sh::CallSite kCalls45EC00[] = {{0xF, 0x5A77C0}, {0x18, 0x461E50}, {0x24, 0x5A7710}, {0xB5, 0x461E50}};
constexpr sh::CallSite kCalls45ECC0[] = {{0x35, 0x5A7570}, {0x96, 0x461E50}};
constexpr sh::CallSite kCalls45ED70[] = {{0x12, 0x5B93D2}, {0x52, 0x5B93D2}};
constexpr sh::CallSite kCalls45EE10[] = {{0x1F, 0x57CF60}, {0x47, 0x45EEB0}, {0x7D, 0x516B30}, {0x8E, 0x57D910}};
constexpr sh::CallSite kCalls45EEB0[] = {{0x21, 0x5A77C0}, {0x2A, 0x461E50}, {0x36, 0x5A7710}, {0xD3, 0x461E50}};
constexpr sh::CallSite kCalls45EF90[] = {{0x44, 0x45EE10}};
constexpr sh::CallSite kCalls45F050[] = {{0x65, 0x5A7670}, {0xE5, 0x461E50}, {0xF1, 0x5A7690}, {0x140, 0x461E50}};
constexpr sh::CallSite kCalls45F1A0[] = {
    {0x36, 0x5A77C0},  {0x3F, 0x461E50},  {0x4B, 0x5A7710},  {0xA9, 0x5A79E0},  {0xC1, 0x461E50},  {0xF0, 0x5A77C0},
    {0xFC, 0x461E50},  {0x108, 0x5A7710}, {0x155, 0x5A79E0}, {0x16B, 0x461E50}, {0x19A, 0x5A77C0}, {0x1A3, 0x461E50},
    {0x1AF, 0x5A7710}, {0x1F8, 0x5A79E0}, {0x211, 0x461E50}, {0x240, 0x5A77C0}, {0x249, 0x461E50}, {0x255, 0x5A7710},
    {0x2A4, 0x5A79E0}, {0x2BA, 0x461E50}, {0x2E9, 0x5A77C0}, {0x2F5, 0x461E50}, {0x301, 0x5A7710}, {0x35A, 0x5A79E0},
    {0x370, 0x461E50}, {0x39F, 0x5A77C0}, {0x3A8, 0x461E50}, {0x3B7, 0x57D860}, {0x3CD, 0x57D860}, {0x3DC, 0x57D860},
    {0x3E7, 0x57D860}};
constexpr sh::CallSite kCalls45F5A0[] = {{0x7, 0x45F020}, {0x5F, 0x45F020}, {0x8A, 0x4976D0}};
constexpr sh::CallSite kCalls45F650[] = {{0xF, 0x45E6D0}, {0x3C, 0x45E6D0}, {0x8E, 0x45E6D0}, {0xA8, 0x45E6D0}, {0xD0, 0x4976D0}};
constexpr sh::CallSite kCalls45F7D0[] = {{0x22, 0x45FA80}};
constexpr sh::CallSite kCalls45F830[] = {{0x26, 0x461EB0}, {0x61, 0x45FA80}, {0x90, 0x45FA80}, {0xB0, 0x587B40},
                                         {0xD7, 0x45FA80}, {0xF1, 0x45FA80}, {0x115, 0x45FA80}};
constexpr sh::CallSite kCalls45F950[] = {{0x0, 0x454810}, {0x1B, 0x587AE0}, {0x2E, 0x45FA80}};
constexpr sh::CallSite kCalls45F9A0[] = {{0x13, 0x587B40}, {0x1F, 0x45FA80}};
constexpr sh::CallSite kCalls45F9E0[] = {{0x0, 0x454810}, {0x16, 0x587AE0}, {0x35, 0x45FA80}};
constexpr sh::CallSite kCalls45FA20[] = {{0x22, 0x45FA80}, {0x35, 0x4976D0}};
constexpr sh::CallSite kCalls45FA80[] = {{0x23, 0x57CF60}, {0x39, 0x57D520},  {0x4F, 0x57D520},  {0x68, 0x57D520},
                                         {0xBF, 0x516B30}, {0xDC, 0x57CF60},  {0xE9, 0x57D520},  {0x119, 0x45FCE0},
                                         {0x133, 0x45FCE0}, {0x143, 0x517090}, {0x154, 0x517090}, {0x167, 0x517090},
                                         {0x17E, 0x517090}, {0x1BF, 0x5A75D0}, {0x22C, 0x461E50}};
constexpr sh::CallSite kCalls45FCE0[] = {{0x3C, 0x5A75D0}, {0xFC, 0x461E50}};
constexpr sh::CallSite kCalls45FE00[] = {{0x17, 0x59E230}};
constexpr sh::CallSite kCalls45FE30[] = {{0xB, 0x4976D0}};
constexpr sh::CallSite kCalls45FE60[] = {{0x9, 0x59E330}, {0xE, 0x460480}};
constexpr sh::CallSite kCalls45FE90[] = {{0x39, 0x57CF60},  {0x5F, 0x57D520},  {0xBF, 0x461EB0},  {0xD3, 0x587740},
                                         {0x10E, 0x587740}, {0x21E, 0x587740}, {0x256, 0x587740}, {0x29A, 0x587740},
                                         {0x2AA, 0x587740}, {0x316, 0x591C20}, {0x31C, 0x497740}, {0x32D, 0x516B30}};
constexpr sh::CallSite kCalls4601D0[] = {{0x37, 0x57CF60}, {0x5D, 0x57D520}, {0x6E, 0x59E330}, {0x75, 0x4976D0}};
constexpr sh::CallSite kCalls460270[] = {{0x37, 0x57CF60},  {0x5D, 0x57D520},  {0x73, 0x59E330},
                                         {0x10D, 0x4976D0}, {0x126, 0x591680}, {0x14F, 0x4976D0}};
constexpr sh::CallSite kCalls4603F0[] = {{0x25, 0x591B60}};
constexpr sh::CallSite kCalls460460[] = {{0xB, 0x4976D0}};
constexpr sh::CallSite kCalls4605D0[] = {{0x3E, 0x460B20}, {0x5D, 0x460A40},  {0x6D, 0x460920},
                                         {0x7D, 0x460730}, {0x110, 0x5B9380}, {0x123, 0x517090}};
constexpr sh::CallSite kCalls460730[] = {{0x26, 0x516B30}, {0x35, 0x460890}, {0x56, 0x516B30}, {0x6E, 0x5B9380},
                                         {0x82, 0x516F60}, {0x98, 0x516B30}, {0xF0, 0x516B30}, {0x13E, 0x516B30}};
constexpr sh::CallSite kCalls460890[] = {{0xF, 0x5A77C0}, {0x18, 0x461E50}, {0x24, 0x5A7710}, {0x7B, 0x461E50}};
constexpr sh::CallSite kCalls460920[] = {{0x26, 0x516B30}, {0x35, 0x460890}, {0x54, 0x516B30}, {0x6C, 0x5B9380},
                                         {0x80, 0x516F60}, {0x96, 0x516B30}, {0x105, 0x516B30}};
constexpr sh::CallSite kCalls460A40[] = {{0x28, 0x516B30}, {0xC6, 0x516B30}};
constexpr sh::CallSite kCalls460B20[] = {{0x13, 0x5A77C0}, {0x1C, 0x461E50}, {0x45, 0x460C40},
                                         {0x99, 0x460C40}, {0xB6, 0x460C40}, {0xFF, 0x460C40}};
constexpr sh::CallSite kCalls460C40[] = {{0x8, 0x5A7710}, {0x64, 0x461E50}};

#define SH_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define G_FN(name) reinterpret_cast<const void*>(&::name)
#define G_CLONE(name, base, size, calls, shape, ret) #name, base, size, calls, SH_N(calls), nullptr, 0, nullptr, 0, G_FN(name), ret, false, shape
#define G_LEAF(name, base, size, shape, ret) #name, base, size, nullptr, 0, nullptr, 0, nullptr, 0, G_FN(name), ret, false, shape
constexpr Shape kSt = Shape::kState;
constexpr Shape kCa = Shape::kCall;
const sh::Clone kClones[] = {
    {G_CLONE(CommuEntry_DrawPanel, 0x45E870, 0x140, kCalls45E870, kCa, 0)},
    {G_CLONE(CommuEntry_DrawPanelFrame, 0x45E9B0, 0xA1, kCalls45E9B0, kCa, 0)},
    {G_CLONE(CommuEntry_DrawBar, 0x45EA60, 0x19E, kCalls45EA60, kCa, 0)},
    {G_CLONE(Commu_DrawPiece6, 0x45EC00, 0xBF, kCalls45EC00, kCa, 0)},
    {G_CLONE(CommuCursor_DrawArrow, 0x45ECC0, 0xA1, kCalls45ECC0, kCa, 0)},
    {G_CLONE(CommuName_MakeRandom, 0x45ED70, 0x9F, kCalls45ED70, kCa, 0xFFFFFFFFu)},
    {G_CLONE(CommuMember_DrawPanel, 0x45EE10, 0x9A, kCalls45EE10, kCa, 0)},
    {G_CLONE(CommuMember_DrawPortrait, 0x45EEB0, 0xDE, kCalls45EEB0, kCa, 0)},
    {G_CLONE(CommuMember_DrawAll, 0x45EF90, 0x61, kCalls45EF90, kCa, 0)},
    {G_LEAF(CommuMember_Count, 0x45F000, 0x1D, kCa, 0xFFu)},
    {G_LEAF(CommuMember_Nth, 0x45F020, 0x2C, kCa, 0xFFu)},
    {G_CLONE(CommuMember_DrawFrame, 0x45F050, 0x14D, kCalls45F050, kCa, 0)},
    {G_CLONE(Commu_DrawTiledFrame, 0x45F1A0, 0x3F7, kCalls45F1A0, kCa, 0)},
    {G_CLONE(CommuName_CommitMember, 0x45F5A0, 0xB0, kCalls45F5A0, kSt, 0)},
    {G_CLONE(CommuName_CommitEntry, 0x45F650, 0xF7, kCalls45F650, kSt, 0)},
    {G_LEAF(CommuMusic_Dispatch, 0x45F750, 0xE, kSt, 0)},
    {G_LEAF(CommuMusic_OpenDispatch, 0x45F760, 0xE, kSt, 0)},
    {G_LEAF(CommuMusic_Open, 0x45F770, 0x58, kSt, 0)},
    {G_CLONE(CommuMusic_SlideIn, 0x45F7D0, 0x47, kCalls45F7D0, kSt, 0)},
    {G_LEAF(CommuMusic_BrowseDispatch, 0x45F820, 0xE, kSt, 0)},
    {G_CLONE(CommuMusic_Browse, 0x45F830, 0x11F, kCalls45F830, kSt, 0)},
    {G_CLONE(CommuMusic_Play, 0x45F950, 0x37, kCalls45F950, kSt, 0)},
    {G_LEAF(CommuMusic_CloseDispatch, 0x45F990, 0xE, kSt, 0)},
    {G_CLONE(CommuMusic_CloseFade, 0x45F9A0, 0x34, kCalls45F9A0, kSt, 0)},
    {G_CLONE(CommuMusic_CloseRestore, 0x45F9E0, 0x3E, kCalls45F9E0, kSt, 0)},
    {G_CLONE(CommuMusic_SlideOut, 0x45FA20, 0x51, kCalls45FA20, kSt, 0)},
    {G_CLONE(CommuMusic_DrawList, 0x45FA80, 0x255, kCalls45FA80, kCa, 0)},
    {"CommuMusic_DrawLabel", 0x45FCE0, 0x120, kCalls45FCE0, SH_N(kCalls45FCE0), nullptr, 0, nullptr, 0,
     G_FN(CommuMusic_DrawLabel), 0, false, kCa, sh::ArgAt(3, sh::Arg::kScratch)},
    {G_CLONE(CommuItem_Frame, 0x45FE00, 0x1D, kCalls45FE00, kSt, 0)},
    {G_LEAF(CommuItem_StepDispatch, 0x45FE20, 0xE, kSt, 0)},
    {G_CLONE(CommuItem_Prompt, 0x45FE30, 0x27, kCalls45FE30, kSt, 0)},
    {G_CLONE(CommuItem_OpenWindow, 0x45FE60, 0x27, kCalls45FE60, kSt, 0)},
    {G_CLONE(CommuItem_Choose, 0x45FE90, 0x338, kCalls45FE90, kSt, 0)},
    {G_CLONE(CommuItem_Cancel, 0x4601D0, 0x91, kCalls4601D0, kSt, 0)},
    {G_CLONE(CommuItem_Confirm, 0x460270, 0x172, kCalls460270, kSt, 0)},
    {G_CLONE(CommuItem_Give, 0x4603F0, 0x66, kCalls4603F0, kSt, 0)},
    {G_CLONE(CommuItem_Refused, 0x460460, 0x1B, kCalls460460, kSt, 0)},
    {G_LEAF(CommuItem_SetupWindow, 0x460480, 0x72, kSt, 0)},
    {G_LEAF(CommuRank_Dispatch, 0x460500, 0xE, kSt, 0)},
    {G_LEAF(CommuRank_Pages, 0x460510, 0xB1, kSt, 0)},
    {G_CLONE(CommuRank_Show, 0x4605D0, 0x14C, kCalls4605D0, kSt, 0)},
    {G_LEAF(CommuRank_Close, 0x460720, 0x7, kSt, 0)},
    {G_CLONE(CommuRank_DrawListA, 0x460730, 0x155, kCalls460730, kCa, 0)},
    {G_CLONE(CommuRank_DrawIcon, 0x460890, 0x85, kCalls460890, kCa, 0)},
    {G_CLONE(CommuRank_DrawListB, 0x460920, 0x119, kCalls460920, kCa, 0)},
    {G_CLONE(CommuRank_DrawListC, 0x460A40, 0xE0, kCalls460A40, kCa, 0)},
    {G_CLONE(CommuRank_DrawBackdrop, 0x460B20, 0x119, kCalls460B20, kCa, 0)},
    {G_CLONE(CommuRank_DrawTile, 0x460C40, 0x6E, kCalls460C40, kCa, 0)},
};
#undef G_LEAF
#undef G_CLONE
#undef G_FN
#undef SH_N

enum : unsigned {
    kPanel, kPanelFrame, kBar, kPiece, kArrow, kRandomName, kMemberPanel, kPortrait, kDrawAll, kCount, kNth,
    kMemberFrame, kTiled, kCommitMember, kCommitEntry, kMusicDispatch, kOpenDispatch, kOpen, kSlideIn,
    kBrowseDispatch, kBrowse, kPlay, kCloseDispatch, kCloseFade, kCloseRestore, kSlideOut, kDrawList, kLabel,
    kItemFrame, kStepDispatch, kPrompt, kOpenWindow, kChoose, kCancel, kConfirm, kGive, kRefused, kSetupWindow,
    kRankDispatch, kRankPages, kShow, kClose, kDrawA, kIcon, kDrawB, kDrawC, kBackdrop, kTile, kTotal
};
static_assert(kTotal == sizeof kClones / sizeof kClones[0], "one enum entry a clone, in order");

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
template <typename... T> U PickOf(T... v) {
    const U values[] = {static_cast<U>(v)...};
    return sh::Pick(values, sizeof...(v));
}
unsigned char* Mem(U a) { return sh::Mem(a); }
unsigned char& B(U a) { return Mem(a)[0]; }

// The cells (rest_4e.cpp's).
constexpr U kMode = 0x939A3E, kStep = 0x939A40, kStyle = 0x903A5A;
constexpr U kCursor = 0x675F8C, kEdit = 0x675F98, kSavedTrack = 0x675FD0, kSlide = 0x675FD1, kTrack = 0x675FD2,
            kItemSlide = 0x675FD4, kPages = 0x675FD8, kPage = 0x675FDB;
constexpr U kEntries = 0x9046D0, kEntry = 0x9039F5, kTrackCounts = 0x9048AB;
constexpr U kMembers = 0x903A70;
constexpr U kListA = 0x9039A0, kListAEntries = 0x9039C0, kListB = 0x904A90, kListBEntries = 0x904F00,
            kListC = 0x937F80, kListCPairs = 0x904CA0;
constexpr U kWin0 = 0x803160;
constexpr U kWinOff = kWin0 + 3, kTab = kWin0 + 0xA, kTop = kWin0 + 0xB, kRow = kWin0 + 0xC, kScroll = kWin0 + 0x12;
constexpr U kPool = 0x803580;          // the script pool's offsets and, from +0x600, the seeded strings
constexpr U kStrings = 0x600;          // 0x60 slots of 16 bytes
constexpr U kPairMessages = 0x653180;
constexpr U kTracks = 0x653098;

// --- the stand-ins' effects (Noise() and the state only: both passes the same) -----

// Input_AutoRepeat: each branch's bit (CommuMusic_Browse 0x2000 / 0x8000,
// CommuItem_Choose 0x8000 / 0x2000 with 0x1000 / 0x4000 / 4 / 8), none, or the
// pressed word as it came.
U RepeatEffect(const U* a, U answer) {
    static const U kAnswers[] = {0,      0,      0x2000, 0x8000, 0x1000, 0x4000, 4,      8,      0x10000,
                                 0xA000, 0x3000, 0x9000, 0x6000, 0x2004, 0x8008, 0x1004, 0x4008, 0x0C};
    const U n = sh::Noise();
    if (n % 4 == 0) return a[0];
    return (n >> 2) % 7 == 0 ? answer : kAnswers[(n >> 5) % (sizeof kAnswers / sizeof kAnswers[0])];
}
// CommuMember_Nth: a member 0..6 (CommuName_CommitMember aborts past them).
U NthEffect(const U*, U answer) { return (answer & 0xFFFFFF00u) | (sh::Noise() % 7); }
// 0x45E6D0: an entry 0..59, often the ends.
U EntryNthEffect(const U*, U answer) {
    const U n = sh::Noise();
    const U e = n % 3 == 0 ? (n >> 2 & 1 ? 59 : 0) : (n >> 2) % 60;
    return (answer & 0xFFFFFF00u) | e;
}
// 0x5A7570: the primitive's 0x2C bytes written (a stand-in louder than the
// real header: every byte ours does not write is compared as the noise).
U PolyF3Effect(const U* a, U answer) {
    auto* const p = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a[0]));
    if (sh::InRegions(p, 0x2C)) sh::FillBytes(p, 0x2C);
    return answer;
}
// Gpu_SetPolyG4 / Gpu_SetLineF3 answer their primitive (unread by these).
U Arg0Effect(const U* a, U) { return a[0]; }
// Item_NamePtr: a name in the harness's text buffer.
U NameEffect(const U*, U) { return Key(sh::Text()); }
// Crt_sprintf: the third argument's digits and a NUL at the destination.
U SprintfEffect(const U* a, U answer) {
    auto* const p = reinterpret_cast<char*>(static_cast<std::uintptr_t>(a[0]));
    if (sh::InRegions(p, 12)) std::snprintf(p, 12, "%u", a[2] & 0xFFFFu);
    return answer;
}

#define G_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
constexpr sh::Answer kG = sh::Answer::kGarbage;
constexpr sh::Answer kF = sh::Answer::kFlag;
constexpr U kAll = 0xFFFFFFFFu, kW = 0xFFFFu, kB = 0xFFu;
const sh::Callee kCallees[] = {
    // the group's own, called directly (E8 / E9) by the group's; masks by what
    // each reads (the low words of x and y, the bytes of the indexes)
    {G_OURS(CommuEntry_DrawPanelFrame), 2, {kW, kW}, kG, 0, 0},
    {G_OURS(CommuEntry_DrawBar), 4, {kW, kW, kW, kB}, kG, 0, 0},
    {G_OURS(Commu_DrawPiece6), 3, {kW, kW, kB}, kG, 0, 0},
    {G_OURS(CommuMember_DrawPanel), 4, {kW, kW, kB, kB}, kG, 0, 0},
    {G_OURS(CommuMember_DrawPortrait), 4, {kW, kW, kB, kB}, kG, 0, 0},
    {G_OURS(CommuMember_Nth), 1, {kB}, kG, 0, 0, {}, &NthEffect},
    {G_OURS(CommuMusic_DrawList), 2, {kW, kW}, kG, 0, 0},
    {G_OURS(CommuMusic_DrawLabel), 4, {kW, kW, 0x3Fu, kAll}, kG, 0, 0, {0, 0, 0, sh::kDerefString}, nullptr, nullptr, true},
    {G_OURS(CommuItem_SetupWindow), 0, {}, kG, 0, 0},
    {G_OURS(CommuRank_DrawBackdrop), 2, {kW, kW}, kG, 0, 0},
    {G_OURS(CommuRank_DrawListA), 3, {kW, kW, kB}, kG, 0, 0},
    {G_OURS(CommuRank_DrawListB), 3, {kW, kW, kB}, kG, 0, 0},
    {G_OURS(CommuRank_DrawListC), 3, {kW, kW, kB}, kG, 0, 0},
    {G_OURS(CommuRank_DrawIcon), 3, {kW, kW, kB}, kG, 0, 0},
    {G_OURS(CommuRank_DrawTile), 5, {kW, kW, kB, kB, kB}, kG, 0, 0},
    // by address: R4D's entry search (ours), the library's POLY_F3 header (rest_4e_callees.h)
    {"0x45E6D0", at::kEntryNth, at::kEntryNth, 1, {kB}, kG, 0, 0, {}, &EntryNthEffect},
    {"0x5A7570", at::kSetPolyF3, at::kSetPolyF3, 1, {kAll}, kG, 0, 0, {}, &PolyF3Effect, nullptr, true},
    // ours, with the width each reads: the draw mode's texture window is a
    // RECT on the caller's stack (logged by its 8 bytes, not its address)
    {G_OURS(Gpu_SetDrawMode), 5, {kAll, kAll, kAll, kAll, 0}, kG, 0, 0, {16, 0, 0, 0, 8}, nullptr, nullptr, true},
    {G_OURS(Gpu_SetLineF3), 1, {kAll}, kG, 0, 0, {16}, &Arg0Effect, nullptr, true},
    {G_OURS(Gpu_SetLineF4), 1, {kAll}, kG, 0, 0, {16}, nullptr, nullptr, true},
    {G_OURS(Menu_DrawOutlineNotched), 5, {kW, kW, kW, kW, kB}, kG, 0, 0},
    {G_OURS(Window_ResetAll), 0, {}, kG, 0, 0},
    {G_OURS(Input_AutoRepeat), 1, {kAll}, kG, 0, 0, {}, &RepeatEffect},
    {G_OURS(Item_NamePtr), 2, {kB, kB}, kG, 0, 0, {}, &NameEffect},   // the bytes of dwords with leftovers above
    {G_OURS(Inventory_Remove), 3, {kB, kB, kB}, kF, 0, 0},           // each argument's byte; a fourth 0 pushed
    {"Crt_sprintf", 0x5B9380, KeyOf(&::Crt_sprintf), 3, {kAll, kAll, kAll}, kG, 0, 0, {0, sh::kDerefString, 0}, &SprintfEffect,
     nullptr, true},
};
#undef G_OURS

// The tables the dispatchers read in place, swapped for recorders on both
// sides; each count is its reader's reach (docs/rest_4e.md section 1).
const sh::DataTable kTables[] = {
    {Key(CommuMusic_Modes), CommuMusic_Modes_count},     {Key(CommuMusic_OpenSteps), CommuMusic_OpenSteps_count},
    {Key(CommuMusic_BrowseSteps), CommuMusic_BrowseSteps_count},
    {Key(CommuMusic_CloseSteps), CommuMusic_CloseSteps_count},
    {Key(CommuItem_Modes), CommuItem_Modes_count},       {Key(CommuItem_Steps), CommuItem_Steps_count},
    {Key(CommuRank_Modes), CommuRank_Modes_count},
};

// Beyond field mode's standard regions: the band's cells, the facility bytes,
// WindowRecords, the character records past the style region, the save block
// 0x904160..0x904560 (the inventory lists) and 0x904700..0x904A30 (the entries'
// end, the track counts, the names), the lists' counts and entries, the pairs
// and Text_Records, the script pool's head and the seeded strings, the
// confirm / cancel words.
const sh::Region kRegions[] = {
    {0x675F8C, 0x50},
    {0x939A3C, 8},
    {sh::at::kWindows, sh::at::kWindowCount * sh::at::kWindowStride},
    {0x903A94, 0x460},
    {0x904160, 0x400},
    {0x904700, 0x330},
    {kListB, 4},
    {kListCPairs, 0x140},
    {kListBEntries, 0x100},
    {kListC, 8},
    {0x9039A8, 0x58},
    {kPool, 0xC00},
    {0x903584, 0x10},
};

// --- the seed ---------------------------------------------------------------------------

// A message id's offset word pointed at one of the seeded strings.
void PointMessage(unsigned id) { SetWord(Mem(kPool + 2 * id), kStrings + 16 * (sh::Next() % 0x60)); }

void Seed(unsigned k) {
    B(kStyle) = static_cast<unsigned char>(sh::Next() % 8);
    // the strings: 0x60 slots of up to 12 bytes (an early NUL often), so the two
    // halves of a random name fit the 32 bytes
    for (unsigned s = 0; s < 0x60; ++s) {
        unsigned char* const slot = Mem(kPool + kStrings + 16 * s);
        if (sh::Half()) slot[sh::Next() % 12] = 0;
        if (sh::Next() % 4 == 0) slot[sh::Next() % 12] = 0x20;
        slot[12] = slot[13] = slot[14] = slot[15] = 0;
    }
    for (unsigned id = 0x1F0; id < 0x270; ++id) PointMessage(id);
    for (unsigned id = 0x271; id < 0x271 + 0x2A; ++id) PointMessage(id);
    for (unsigned id = 0x37; id <= 0x3B; ++id) PointMessage(id);
    PointMessage(0xEC);
    for (unsigned kind = 0; kind < 8; ++kind) PointMessage(Word(Mem(kPairMessages + 4 * kind)));
    // the facility bytes near their bounds, then each dispatcher's below its table
    B(kMode) = static_cast<unsigned char>(PickOf(0, 1, 2, 3, 0xFF, sh::Next()));
    B(kStep) = static_cast<unsigned char>(PickOf(0, 1, 2, 3, 4, 5, 6, 0xFF, sh::Next()));
    switch (k) {
    case kMusicDispatch: B(kMode) = static_cast<unsigned char>(sh::Next() % CommuMusic_Modes_count); break;
    case kOpenDispatch: B(kStep) = static_cast<unsigned char>(sh::Next() % CommuMusic_OpenSteps_count); break;
    case kBrowseDispatch: B(kStep) = static_cast<unsigned char>(sh::Next() % CommuMusic_BrowseSteps_count); break;
    case kCloseDispatch: B(kStep) = static_cast<unsigned char>(sh::Next() % CommuMusic_CloseSteps_count); break;
    case kItemFrame: B(kMode) = static_cast<unsigned char>(sh::Next() % CommuItem_Modes_count); break;
    case kStepDispatch: B(kStep) = static_cast<unsigned char>(sh::Next() % CommuItem_Steps_count); break;
    case kRankDispatch: B(kMode) = static_cast<unsigned char>(sh::Next() % CommuRank_Modes_count); break;
    default: break;
    }
    // the members: present or not (bit 0 of +0xB) - all, none, or random
    const U presence = PickOf(0, 0x7F, sh::Next(), sh::Next(), sh::Next());
    for (unsigned i = 0; i < 7; ++i) {
        unsigned char& flags = B(kMembers + 0xA4 * i + 0xB);
        flags = static_cast<unsigned char>((flags & 0xFE) | (presence >> i & 1));
    }
    // the entry (below the 60: two functions write through it), its kind and
    // that kind's track count (the 40 tracks), the row near the count
    B(kEntry) = static_cast<unsigned char>(PickOf(0, 59, sh::Next() % 60, sh::Next() % 60));
    const unsigned kind = PickOf(0, 1, 8, sh::Next() % 9, sh::Next() % 9);
    unsigned char* const entry = Mem(kEntries + 8 * B(kEntry));
    entry[1] = static_cast<unsigned char>(kind);
    entry[2] = static_cast<unsigned char>(PickOf(0, 1, sh::Next()));
    entry[3] = static_cast<unsigned char>(PickOf(0, 1, 2, 3, 0x13, 0x18, 0xF, sh::Next()));
    for (unsigned c = 0; c < 9; ++c) B(kTrackCounts + 8 * c) = static_cast<unsigned char>(PickOf(0, 1, 2, 0x26, 0x27, sh::Next() % 40));
    const unsigned count = B(kTrackCounts + 8 * kind);
    SetWord(Mem(kTrack), PickOf(0, 1, count, count - 1, count + 1, 39, 0xFFFF, 0xFFFE, sh::Next() % 40));
    B(kSlide) = static_cast<unsigned char>(PickOf(0, 1, 2, 3, 4, 5, 0xFF, sh::Next()));
    const auto row = static_cast<short>(Word(Mem(kTrack)));
    const unsigned char playing = Mem(kTracks + row)[0];
    Music_Track = static_cast<unsigned char>(PickOf(0xFF, playing, playing, sh::Next()));
    B(kSavedTrack) = static_cast<unsigned char>(PickOf(0xFF, Music_Track, sh::Next()));
    // the item window: shut or not, tab 0..4, top and row at the paging bounds,
    // the scroll word 0 most of the time, the row's item empty or not
    B(kWinOff) = static_cast<unsigned char>(PickOf(0, 0, 0, 1, 2));
    B(kTab) = static_cast<unsigned char>(PickOf(0, 1, 2, 3, 4, 0, 3));
    const unsigned top = PickOf(0, 1, 8, 9, 0x6E, 0x6F, 0x76, 0x77, sh::Next() % 0x78);
    B(kTop) = static_cast<unsigned char>(top);
    B(kRow) = static_cast<unsigned char>(PickOf(0, top, top - 1, top + 8, top + 9, 0x7E, 0x7F, sh::Next() % 0x80));
    SetWord(Mem(kScroll), PickOf(0, 0, 0, 0x10, sh::Next()));
    B(kItemSlide) = static_cast<unsigned char>(PickOf(0, 1, 3, 4, 5, sh::Next()));
    const U list = Long(Mem(bof3::addr::Inventory_IdLists + 4 * B(kTab)));
    if (sh::Half()) Mem(list + B(kRow))[0] = 0;
    // the three lists: counts at their page bounds, the pairs' kinds below 8 and
    // entries below 60 for the four pages the counts reach
    B(kListA) = static_cast<unsigned char>(PickOf(0, 1, 7, 8, 14, 15, 0x20, sh::Next()));
    B(kListB) = static_cast<unsigned char>(PickOf(0, 1, 20, 21, 40, 0x3C, sh::Next()));
    B(kListC) = static_cast<unsigned char>(PickOf(0, 1, 8, 9, 16, 0x20, sh::Next() % 0x21));
    for (unsigned i = 0; i < 32; ++i) {
        B(kListCPairs + 2 * i) = static_cast<unsigned char>(sh::Next() % 8);
        B(kListCPairs + 2 * i + 1) = static_cast<unsigned char>(sh::Next() % 60);
    }
    // the page counts and the page: the counts' sum above the page (the walk
    // aborts past them)
    for (unsigned i = 0; i < 3; ++i) B(kPages + i) = static_cast<unsigned char>(PickOf(0, 0, 1, 2, 3, sh::Next() % 14));
    if (B(kPages) + B(kPages + 1) + B(kPages + 2) == 0) B(kPages + PickOf(0, 1, 2)) = 1;
    const unsigned sum = B(kPages) + B(kPages + 1) + B(kPages + 2);
    B(kPage) = static_cast<unsigned char>(PickOf(0, sum - 1, sh::Next() % sum, sh::Next() % sum, B(kPages), B(kPages) - 1));
    if (B(kPage) >= sum) B(kPage) = 0;
    // the pad and the buttons
    Input_Pressed = static_cast<unsigned short>(PickOf(0, 0, 0x20, 0x40, 0x60, 0x2000, 0x8000, 0xA000, 0x1000, 0x4000,
                                                       4, 8, 0x2020, 0x8040, sh::Next()));
    Field_ConfirmButtons = static_cast<unsigned short>(PickOf(0x20, 0x40, 0x60, 0, sh::Next()));
    Field_CancelButtons = static_cast<unsigned short>(PickOf(0x40, 0x10, 0, sh::Next()));
    Field_Request = static_cast<unsigned char>(PickOf(0, 2, 2, 1, sh::Next()));
    // CommuItem_Choose: the window open, not scrolling and a confirm pressed
    // most of the time, so the paths past its sounds run often
    if (k == kChoose && sh::Often()) {
        B(kWinOff) = 0;
        SetWord(Mem(kScroll), 0);
        if (sh::Half()) Field_ConfirmButtons = static_cast<unsigned short>(Field_ConfirmButtons | Input_Pressed | 0x20);
    }
    // a label for CommuMusic_DrawLabel: letters, spaces, a NUL by the end
    unsigned char* const text = sh::Scratch(3);
    for (unsigned i = 0; i < 0x40; ++i)
        if (sh::Next() % 5 == 0) text[i] = 0x20;
    text[sh::Next() % 0x40] = 0;
    text[0x3F] = 0;
}

// The arguments of the kCall functions, after the seed.
void Args(unsigned k, U* a) {
    switch (k) {
    case kPanel: a[2] = (a[2] & 0xFFFFFF00u) | PickOf(0, 59, sh::Next() % 60, 0xFF, a[2] & 0xFF);
                 a[3] = PickOf(0, 1, 0x100, a[3]); break;
    case kMemberPanel: a[2] = (a[2] & 0xFFFFFF00u) | PickOf(0, 6, sh::Next() % 7, a[2] & 0xFF);
                       a[3] = PickOf(0, 1, 0x100, a[3]); break;
    case kBar: a[3] = (a[3] & 0xFFFFFF00u) | (sh::Next() % 4); break;
    case kPiece: a[2] = (a[2] & 0xFFFFFF00u) | PickOf(1, 6, 7, 8, 9, 0xA, 0xB, 0xC, 0xD, a[2] & 0xFF); break;
    case kPortrait: a[2] = (a[2] & 0xFFFFFF00u) | PickOf(0, 8, sh::Next() % 9, a[2] & 0xFF);
                    a[3] = PickOf(0, 1, 2, 0x100, 0x101, a[3]); break;
    case kArrow: a[2] = PickOf(0, 1, 0x100, a[2]); break;
    case kMemberFrame: a[2] = PickOf(0, 1, 0x100, a[2]); break;
    case kNth: a[0] = PickOf(0, 1, 6, 7, a[0]); break;
    case kLabel: a[2] = PickOf(0, 0x3F, 0x40, 0x41, a[2]); break;
    case kDrawA: case kDrawB: case kDrawC: a[2] = (a[2] & 0xFFFFFF00u) | PickOf(0, 1, 2, 3, sh::Next() % 4); break;
    case kTile: a[4] = PickOf(0, 1, 0x100, a[4]); break;
    default: break;
    }
}

// --- the disturbance: a cell these read again after a call ------------------------------
void Disturb(U h) {
    const auto b = static_cast<unsigned char>(h >> 24);
    const U v = h >> 8;
    switch (sh::DisturbCase(h, 19)) {
    case 0: B(kStep) = b; break;
    case 1: B(kMode) = b; break;
    case 2: B(kSlide) = static_cast<unsigned char>(b % 6); break;
    case 3: B(kItemSlide) = static_cast<unsigned char>(b % 6); break;
    case 4: B(kWinOff) = static_cast<unsigned char>(b & 1); break;
    case 5: B(kTab) = static_cast<unsigned char>(b % 5); break;
    case 6: B(kRow) = static_cast<unsigned char>(b & 0x7F); break;
    case 7: SetWord(Mem(kScroll), b & 1 ? 0 : v | 1); break;
    case 8: B(kEntry) = static_cast<unsigned char>(b % 60); break;
    case 9: Music_Track = static_cast<unsigned char>(b & 1 ? 0xFF : b); break;
    case 10: B(kEdit + (b & 7)) = static_cast<unsigned char>(v); break;
    case 11: B(b % 3 == 0 ? kListA : b % 3 == 1 ? kListB : kListC) = static_cast<unsigned char>(v % 0x21); break;
    case 12:   // every row of the first list, so the row being drawn moves
        for (unsigned i = 0; i < 0x20; ++i) B(kListAEntries + i) ^= static_cast<unsigned char>(0x80 | (b & 0x7F));
        break;
    case 13: B(kStyle) = static_cast<unsigned char>(b % 8); break;
    case 14: sh::Scratch(3)[b % 0x3F] = static_cast<unsigned char>(v); break;
    case 15: SetWord(Mem(kTrack), (v % 44) - 2); break;
    case 16: B(kEntries + 8 * B(kEntry) + 2 + (b & 1)) = static_cast<unsigned char>(v); break;
    case 17: if (b & 1) B(kSavedTrack) = static_cast<unsigned char>(v); else B(kPage) = static_cast<unsigned char>(v); break;
    case 18: B(kCursor) = b; break;
    default: break;
    }
}

}  // namespace

void SelfTest() {
    // BOF3X_R4E_ONLY: the clones whose name contains it (a control's run)
    static sh::Clone chosen[kTotal];
    static unsigned index[kTotal];
    const char* const only = std::getenv("BOF3X_R4E_ONLY");
    unsigned n = 0;
    for (unsigned k = 0; k < kTotal; ++k)
        if (!only || !*only || std::strstr(kClones[k].name, only)) {
            index[n] = k;
            chosen[n++] = kClones[k];
        }
    if (n == 0) bof3::Fatal("rest_4e: BOF3X_R4E_ONLY=%s names no clone", only);
    static unsigned* s_index = index;
    sh::Group g = {"rest_4e", chosen, n, kCallees, sizeof kCallees / sizeof kCallees[0], kTables,
                   sizeof kTables / sizeof kTables[0], kRegions, sizeof kRegions / sizeof kRegions[0],
                   [](unsigned k) { Seed(s_index[k]); }, &Disturb, 6000};
    g.args = [](unsigned k, U* a) { Args(s_index[k], a); };
    g.field = true;
    sh::Run(g);
}

}  // namespace rest_4e

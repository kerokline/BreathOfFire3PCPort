// BOF3X_SHADOW=rest_4c: group R4C's 60 functions through the scenario harness's
// field mode (scenario_harness.h, used unchanged; docs/scenario_harness.md
// section 7), once at start-up. docs/rest_4c.md section 4.
// BOF3X_R4C_ONLY=<name> runs the clones whose name contains it (the controls).
//
// The clone table is tools/band_rows.py --group R4C --clones --harness scenario
// (2026-10-05) with the names given; every extent and call site agrees with the
// capstone read (docs/rest_4c.md section 5). The games' states are kState
// (they dispatch on 0x939A3E..0x939A40, not the menu block), the helpers kCall.
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/rest_4c.h"
#include "game/rest_4c_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace rest_4c {
namespace {

namespace sh = scenario_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using sh::Shape;
using U = std::uint32_t;

// band_rows.py's call sites (2026-10-05), each checked against the capstone read.
constexpr sh::CallSite kCalls459EE0[] = {{0x1D, 0x587740}};
constexpr sh::CallSite kCalls459F40[] = {{0x21, 0x587B40}};
constexpr sh::CallSite kCalls459F80[] = {{0x0, 0x454810}, {0x10, 0x587AE0}};
constexpr sh::CallSite kCalls459FC0[] = {{0x15, 0x587740}, {0x4A, 0x45B490}, {0x69, 0x45B5F0}, {0x7C, 0x4976D0}};
constexpr sh::CallSite kCalls45A070[] = {{0x4, 0x45B490}, {0xF, 0x45B5F0}};
constexpr sh::CallSite kCalls45A0B0[] = {{0x62, 0x461EB0}, {0x7C, 0x587740}, {0x186, 0x587740}, {0x1A3, 0x587740},
                                         {0x1D6, 0x587740}, {0x208, 0x45B490}, {0x219, 0x45B5F0}, {0x239, 0x45B3A0}};
constexpr sh::CallSite kCalls45A300[] = {{0x26, 0x45B490}, {0x50, 0x45B5F0}};
constexpr sh::CallSite kCalls45A380[] = {{0x20, 0x45B490}, {0x3F, 0x45B5F0}, {0x50, 0x45B370}};
constexpr sh::CallSite kCalls45A3F0[] = {{0x4, 0x45B490}, {0x12, 0x45B5F0}, {0x35, 0x45B080}, {0x3F, 0x587740}};
constexpr sh::CallSite kCalls45A460[] = {{0x6, 0x45B490}, {0x14, 0x45B5F0}, {0x45, 0x45B2C0}, {0x76, 0x45B2C0},
                                         {0xA3, 0x5B93D2}, {0xAD, 0x5B93D2}, {0xC3, 0x5B93D2}, {0x110, 0x587740}};
constexpr sh::CallSite kCalls45A590[] = {{0x7, 0x45AFF0}, {0x37, 0x45B2C0}, {0x49, 0x45B490}, {0x57, 0x45B5F0},
                                         {0x5E, 0x4976D0}};
constexpr sh::CallSite kCalls45A610[] = {{0x7, 0x45AFF0}, {0x37, 0x45B2C0}, {0x49, 0x45B490}, {0x57, 0x45B5F0},
                                         {0x6E, 0x587740}};
constexpr sh::CallSite kCalls45A6A0[] = {{0x6, 0x45AFF0}, {0x31, 0x45B1E0}, {0x57, 0x45B1E0}, {0x7D, 0x45B1E0},
                                         {0x86, 0x45B490}, {0x97, 0x45B5F0}};
constexpr sh::CallSite kCalls45A770[] = {{0xD, 0x461EB0},  {0x2E, 0x587740},  {0xC1, 0x587740},  {0xCB, 0x587740},
                                         {0x110, 0x587740}, {0x14F, 0x587740}, {0x192, 0x45B100}, {0x19E, 0x45B490},
                                         {0x1AC, 0x45B5F0}, {0x1BB, 0x45AFF0}, {0x1DA, 0x45AF60}, {0x1F8, 0x5905D0}};
constexpr sh::CallSite kCalls45A980[] = {{0x5, 0x45B490}, {0x13, 0x45B5F0}, {0x25, 0x45AFF0}, {0x69, 0x45B2C0},
                                         {0x91, 0x45B100}};
constexpr sh::CallSite kCalls45AA40[] = {{0x4, 0x45B490}, {0x12, 0x45B5F0}, {0x53, 0x587740}, {0xB3, 0x587740},
                                         {0xE7, 0x45AFF0}};
constexpr sh::CallSite kCalls45AB30[] = {{0x4, 0x45B490}, {0x12, 0x45B5F0}, {0x24, 0x45AFF0}, {0x3F, 0x587740}};
constexpr sh::CallSite kCalls45AB90[] = {{0x3D, 0x45B2C0}, {0xA6, 0x4976D0}, {0xC5, 0x45B490}, {0xD3, 0x45B5F0}};
constexpr sh::CallSite kCalls45AC70[] = {{0x17, 0x4976D0}, {0x36, 0x4976D0}, {0x50, 0x45B490}, {0x5E, 0x45B5F0}};
constexpr sh::CallSite kCalls45ACE0[] = {{0x11, 0x591BE0}, {0x18, 0x4976D0}, {0x35, 0x45B490}, {0x43, 0x45B5F0}};
constexpr sh::CallSite kCalls45AD30[] = {{0xD, 0x45B490},  {0x1B, 0x45B5F0}, {0x31, 0x45B490}, {0x3F, 0x45B5F0},
                                         {0x49, 0x587740},  {0x95, 0x45B490}, {0xBA, 0x45B5F0}, {0xCD, 0x4976D0}};
constexpr sh::CallSite kCalls45AE30[] = {{0x1A, 0x45B490}, {0x28, 0x45B5F0}, {0x32, 0x587740}, {0x6E, 0x45B490},
                                         {0x90, 0x45B5F0}, {0xB4, 0x45B490}, {0xC2, 0x45B5F0}};
constexpr sh::CallSite kCalls45AF00[] = {{0x14, 0x587B40}, {0x29, 0x454810}, {0x3F, 0x587AE0}};
constexpr sh::CallSite kCalls45AF60[] = {{0x1D, 0x5A7570}, {0x7E, 0x461E50}};
constexpr sh::CallSite kCalls45AFF0[] = {{0x73, 0x45B2C0}};
constexpr sh::CallSite kCalls45B080[] = {{0x3, 0x45B0D0}, {0x17, 0x45B0D0}};
constexpr sh::CallSite kCalls45B0D0[] = {{0x5, 0x5B93D2}};
constexpr sh::CallSite kCalls45B100[] = {{0x2C, 0x45B1E0}, {0x4E, 0x45B1E0}, {0x5C, 0x45B1E0}, {0x6A, 0x45B1E0},
                                         {0x8F, 0x45B1E0}, {0xAD, 0x45B1E0}, {0xBB, 0x45B1E0}, {0xC9, 0x45B1E0}};
constexpr sh::CallSite kCalls45B1E0[] = {{0x18, 0x57CF60}, {0x2C, 0x5A77C0}, {0x35, 0x461E50}, {0x55, 0x45B400},
                                         {0x78, 0x516B30}, {0x87, 0x45B400}, {0xA4, 0x45B400}, {0xC6, 0x516B30}};
constexpr sh::CallSite kCalls45B2C0[] = {{0xF, 0x5A77C0}, {0x18, 0x461E50}, {0x24, 0x5A7710}, {0x7D, 0x461E50},
                                         {0xA6, 0x461E50}};
constexpr sh::CallSite kCalls45B370[] = {{0x5, 0x587740}, {0xC, 0x4976D0}};
constexpr sh::CallSite kCalls45B3A0[] = {{0x8, 0x5A7650}, {0x4D, 0x461E50}};
constexpr sh::CallSite kCalls45B400[] = {{0x8, 0x5A7710}, {0x78, 0x461E50}};
constexpr sh::CallSite kCalls45B490[] = {{0x21, 0x57CF60}, {0x28, 0x45B520}, {0x3E, 0x516B30},
                                         {0x54, 0x5B9380}, {0x6B, 0x516F60}, {0x7E, 0x516B30}};
constexpr sh::CallSite kCalls45B520[] = {{0x11, 0x5A77C0}, {0x1A, 0x461E50}, {0x2B, 0x45B400}, {0x3D, 0x45B400},
                                         {0x53, 0x45B400}, {0x63, 0x45B400}, {0x78, 0x45B400}, {0x85, 0x45B400},
                                         {0x9A, 0x45B400}, {0xAD, 0x45B400}, {0xC3, 0x45B400}};
constexpr sh::CallSite kCalls45B5F0[] = {{0x21, 0x57CF60}, {0x28, 0x45B520}, {0x3E, 0x516B30}, {0x73, 0x5B9380},
                                         {0x87, 0x516F60}, {0x9D, 0x516B30}, {0xBF, 0x5B9380}, {0xDF, 0x5B9380},
                                         {0x10A, 0x5B9380}, {0x12C, 0x5B9380}, {0x15E, 0x5B9380}};
constexpr sh::CallSite kCalls45B790[] = {{0xC, 0x587B40}};
constexpr sh::CallSite kCalls45B7C0[] = {{0x0, 0x454810}, {0x10, 0x587AE0}, {0x17, 0x4976D0}};
constexpr sh::CallSite kCalls45B800[] = {{0xE, 0x587740}};
constexpr sh::CallSite kCalls45B840[] = {{0x24, 0x45C400}, {0x41, 0x45B490}, {0x60, 0x45B5F0}, {0x84, 0x45B2C0}};
constexpr sh::CallSite kCalls45B910[] = {{0x1E, 0x45B0D0}, {0x6B, 0x45C400}, {0x79, 0x45B2C0}, {0x85, 0x45B490},
                                         {0x93, 0x45B5F0}, {0xA4, 0x587740}};
constexpr sh::CallSite kCalls45B9D0[] = {{0x14, 0x45C400}, {0x20, 0x45B490}, {0x2E, 0x45B5F0}, {0x5B, 0x45B2C0},
                                         {0x88, 0x45B2C0}, {0xAC, 0x45B2C0}, {0xCE, 0x45B2C0}, {0xED, 0x45B2C0},
                                         {0xFB, 0x45B2C0}};
constexpr sh::CallSite kCalls45BAF0[] = {{0x5, 0x4976D0}, {0x25, 0x45C400}, {0x30, 0x45C850}, {0x41, 0x45C7D0},
                                         {0x4D, 0x45B490}, {0x5B, 0x45B5F0}};
constexpr sh::CallSite kCalls45BB60[] = {{0x22, 0x45C400}, {0x2D, 0x45C850}, {0x3E, 0x45C7D0}, {0x4A, 0x45B490},
                                         {0x58, 0x45B5F0}};
constexpr sh::CallSite kCalls45BBD0[] = {{0xE, 0x461EB0},  {0x26, 0x587740},  {0xF4, 0x587740},  {0x10E, 0x45C400},
                                         {0x119, 0x45C850}, {0x12A, 0x45C7D0}, {0x136, 0x45B490}, {0x144, 0x45B5F0}};
constexpr sh::CallSite kCalls45BD20[] = {{0x89, 0x5B9380},  {0xA3, 0x5B9380},  {0xAD, 0x4976D0},  {0xD2, 0x5B9380},
                                         {0xDA, 0x5B93D2},  {0xEA, 0x5B93D2},  {0x12B, 0x591680}, {0x157, 0x4976D0},
                                         {0x173, 0x45C400}, {0x17E, 0x45C850}, {0x190, 0x45C7D0}, {0x19C, 0x45B490},
                                         {0x1AA, 0x45B5F0}};
constexpr sh::CallSite kCalls45BEE0[] = {{0x43, 0x4976D0}, {0x64, 0x45C400}, {0x7B, 0x45C400}, {0x8D, 0x45C7D0},
                                         {0x9B, 0x45C850}, {0xA7, 0x45B490}, {0xB5, 0x45B5F0}};
constexpr sh::CallSite kCalls45BFA0[] = {{0xE, 0x4976D0}, {0x2A, 0x45C400}, {0x35, 0x45C850}, {0x51, 0x45C7D0},
                                         {0x60, 0x45B490}, {0x6E, 0x45B5F0}};
constexpr sh::CallSite kCalls45C020[] = {{0x30, 0x590BB0}, {0x41, 0x4976D0}, {0x5D, 0x45C400}, {0x68, 0x45C850},
                                         {0x7A, 0x45C7D0}, {0x86, 0x45B490}, {0x94, 0x45B5F0}};
constexpr sh::CallSite kCalls45C0C0[] = {
    {0x38, 0x45C850},  {0x47, 0x45C7D0},  {0x52, 0x45C400},  {0x5E, 0x45B490},  {0x6B, 0x45B5F0},  {0xCB, 0x45B2C0},
    {0xFE, 0x45B2C0},  {0x128, 0x45B2C0}, {0x148, 0x45B2C0}, {0x168, 0x45B2C0}, {0x177, 0x45B2C0}, {0x185, 0x45C400},
    {0x191, 0x45B490}, {0x19E, 0x45B5F0}, {0x1D0, 0x587740}, {0x20B, 0x45C400}, {0x228, 0x45B490}, {0x246, 0x45B5F0},
    {0x26A, 0x45B2C0}, {0x29D, 0x45C850}, {0x2AE, 0x45C7D0}, {0x2B9, 0x45C400}, {0x2C5, 0x45B490}, {0x2D2, 0x45B5F0}};
constexpr sh::CallSite kCalls45C3A0[] = {{0x14, 0x587B40}, {0x29, 0x454810}, {0x3F, 0x587AE0}};

#define SH_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define G_FN(name) reinterpret_cast<const void*>(&::name)
#define G_CLONE(name, base, size, calls, shape, ret) #name, base, size, calls, SH_N(calls), nullptr, 0, nullptr, 0, G_FN(name), ret, false, shape
#define G_LEAF(name, base, size, shape, ret) #name, base, size, nullptr, 0, nullptr, 0, nullptr, 0, G_FN(name), ret, false, shape
constexpr Shape kSt = Shape::kState;
constexpr Shape kCa = Shape::kCall;
const sh::Clone kClones[] = {
    {G_CLONE(Commu_PushSubscreen, 0x459EE0, 0x3D, kCalls459EE0, kSt, 0)},
    {G_LEAF(CommuHiLo_Run, 0x459F20, 0xE, kSt, 0)},
    {G_LEAF(CommuHiLo_OpenDispatch, 0x459F30, 0xE, kSt, 0)},
    {G_CLONE(CommuHiLo_OpenFade, 0x459F40, 0x36, kCalls459F40, kSt, 0)},
    {G_CLONE(CommuHiLo_OpenMusic, 0x459F80, 0x2C, kCalls459F80, kSt, 0)},
    {G_LEAF(CommuHiLo_PlayDispatch, 0x459FB0, 0xE, kSt, 0)},
    {G_CLONE(CommuHiLo_Intro, 0x459FC0, 0xB0, kCalls459FC0, kSt, 0)},
    {G_CLONE(CommuHiLo_WaitIntro, 0x45A070, 0x27, kCalls45A070, kSt, 0)},
    {G_LEAF(CommuHiLo_BetDispatch, 0x45A0A0, 0xE, kSt, 0)},
    {G_CLONE(CommuHiLo_BetInput, 0x45A0B0, 0x242, kCalls45A0B0, kSt, 0)},
    {G_CLONE(CommuHiLo_BetSlide, 0x45A300, 0x75, kCalls45A300, kSt, 0)},
    {G_CLONE(CommuHiLo_BetBack, 0x45A380, 0x56, kCalls45A380, kSt, 0)},
    {G_LEAF(CommuHiLo_DealDispatch, 0x45A3E0, 0xE, kSt, 0)},
    {G_CLONE(CommuHiLo_DealStart, 0x45A3F0, 0x62, kCalls45A3F0, kSt, 0)},
    {G_CLONE(CommuHiLo_DealCards, 0x45A460, 0x121, kCalls45A460, kSt, 0)},
    {G_CLONE(CommuHiLo_DealHints, 0x45A590, 0x7B, kCalls45A590, kSt, 0)},
    {G_CLONE(CommuHiLo_DealWait, 0x45A610, 0x8A, kCalls45A610, kSt, 0)},
    {G_CLONE(CommuHiLo_DealChoices, 0x45A6A0, 0xCA, kCalls45A6A0, kSt, 0)},
    {G_CLONE(CommuHiLo_Pick, 0x45A770, 0x201, kCalls45A770, kSt, 0)},
    {G_CLONE(CommuHiLo_PickClose, 0x45A980, 0xBE, kCalls45A980, kSt, 0)},
    {G_CLONE(CommuHiLo_Reveal, 0x45AA40, 0xF0, kCalls45AA40, kSt, 0)},
    {G_CLONE(CommuHiLo_RevealPause, 0x45AB30, 0x54, kCalls45AB30, kSt, 0)},
    {G_CLONE(CommuHiLo_Payout, 0x45AB90, 0xDC, kCalls45AB90, kSt, 0)},
    {G_CLONE(CommuHiLo_CheckWinnings, 0x45AC70, 0x67, kCalls45AC70, kSt, 0)},
    {G_CLONE(CommuHiLo_CashOut, 0x45ACE0, 0x4C, kCalls45ACE0, kSt, 0)},
    {G_CLONE(CommuHiLo_Restart, 0x45AD30, 0xFC, kCalls45AD30, kSt, 0)},
    {G_CLONE(CommuHiLo_Leave, 0x45AE30, 0xCB, kCalls45AE30, kSt, 0)},
    {G_CLONE(CommuHiLo_Close, 0x45AF00, 0x54, kCalls45AF00, kSt, 0)},
    {G_CLONE(CommuHiLo_DrawMarker, 0x45AF60, 0x89, kCalls45AF60, kCa, 0)},
    {G_CLONE(CommuHiLo_DrawRow, 0x45AFF0, 0x85, kCalls45AFF0, kCa, 0)},
    {G_CLONE(CommuHiLo_Shuffle, 0x45B080, 0x44, kCalls45B080, kCa, 0)},
    {G_CLONE(Commu_RandDigit, 0x45B0D0, 0x2B, kCalls45B0D0, kCa, 0xFFu)},
    {G_CLONE(CommuHiLo_DrawChoices, 0x45B100, 0xD4, kCalls45B100, kCa, 0)},
    {G_CLONE(CommuHiLo_DrawChoice, 0x45B1E0, 0xD1, kCalls45B1E0, kCa, 0)},
    {G_CLONE(Commu_DrawCard, 0x45B2C0, 0xB0, kCalls45B2C0, kCa, 0)},
    {G_CLONE(CommuHiLo_Quit, 0x45B370, 0x2F, kCalls45B370, kSt, 0)},
    {G_CLONE(Commu_DrawUnderline, 0x45B3A0, 0x57, kCalls45B3A0, kCa, 0)},
    {G_CLONE(Commu_DrawPiece, 0x45B400, 0x82, kCalls45B400, kCa, 0)},
    {G_CLONE(Commu_DrawZennyBox, 0x45B490, 0x89, kCalls45B490, kCa, 0)},
    {G_CLONE(Commu_DrawFrame, 0x45B520, 0xCF, kCalls45B520, kCa, 0)},
    {G_CLONE(Commu_DrawStakeBox, 0x45B5F0, 0x177, kCalls45B5F0, kCa, 0)},
    {G_LEAF(CommuHitBlow_Run, 0x45B770, 0xE, kSt, 0)},
    {G_LEAF(CommuHitBlow_OpenDispatch, 0x45B780, 0xE, kSt, 0)},
    {G_CLONE(CommuHitBlow_OpenFade, 0x45B790, 0x21, kCalls45B790, kSt, 0)},
    {G_CLONE(CommuHitBlow_OpenMusic, 0x45B7C0, 0x33, kCalls45B7C0, kSt, 0)},
    {G_CLONE(CommuHitBlow_OpenWait, 0x45B800, 0x34, kCalls45B800, kSt, 0)},
    {G_CLONE(CommuHitBlow_OpenSlide, 0x45B840, 0xA9, kCalls45B840, kSt, 0)},
    {G_LEAF(CommuHitBlow_PlayDispatch, 0x45B8F0, 0xE, kSt, 0)},
    {G_LEAF(CommuHitBlow_StartDispatch, 0x45B900, 0xE, kSt, 0)},
    {G_CLONE(CommuHitBlow_Start, 0x45B910, 0xBB, kCalls45B910, kSt, 0)},
    {G_CLONE(CommuHitBlow_StartSlide, 0x45B9D0, 0x120, kCalls45B9D0, kSt, 0)},
    {G_CLONE(CommuHitBlow_Prompt, 0x45BAF0, 0x64, kCalls45BAF0, kSt, 0)},
    {G_CLONE(CommuHitBlow_WaitPrompt, 0x45BB60, 0x61, kCalls45BB60, kSt, 0)},
    {G_CLONE(CommuHitBlow_Input, 0x45BBD0, 0x14F, kCalls45BBD0, kSt, 0)},
    {G_CLONE(CommuHitBlow_Score, 0x45BD20, 0x1B3, kCalls45BD20, kSt, 0)},
    {G_CLONE(CommuHitBlow_NextGuess, 0x45BEE0, 0xBE, kCalls45BEE0, kSt, 0)},
    {G_CLONE(CommuHitBlow_Lost, 0x45BFA0, 0x77, kCalls45BFA0, kSt, 0)},
    {G_CLONE(CommuHitBlow_Prize, 0x45C020, 0x9D, kCalls45C020, kSt, 0)},
    {G_CLONE(CommuHitBlow_End, 0x45C0C0, 0x2DD, kCalls45C0C0, kSt, 0)},
    {G_CLONE(CommuHitBlow_Close, 0x45C3A0, 0x54, kCalls45C3A0, kSt, 0)},
};
#undef G_LEAF
#undef G_CLONE
#undef G_FN
#undef SH_N

enum : unsigned {
    kPush, kHRun, kHOpenDispatch, kHOpenFade, kHOpenMusic, kHPlayDispatch, kHIntro, kHWaitIntro, kHBetDispatch,
    kHBetInput, kHBetSlide, kHBetBack, kHDealDispatch, kHDealStart, kHDealCards, kHDealHints, kHDealWait,
    kHDealChoices, kHPick, kHPickClose, kHReveal, kHRevealPause, kHPayout, kHCheckWinnings, kHCashOut, kHRestart,
    kHLeave, kHClose, kMarker, kRow, kShuffle, kRandDigit, kChoices, kChoice, kCard, kQuit, kUnderline, kPiece,
    kZennyBox, kFrame, kStakeBox, kBRun, kBOpenDispatch, kBOpenFade, kBOpenMusic, kBOpenWait, kBOpenSlide,
    kBPlayDispatch, kBStartDispatch, kBStart, kBStartSlide, kBPrompt, kBWaitPrompt, kBInput, kBScore, kBNextGuess,
    kBLost, kBPrize, kBEnd, kBClose, kCount
};
static_assert(kCount == sizeof kClones / sizeof kClones[0], "one enum entry a clone, in order");

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
template <typename... T> U PickOf(T... v) {
    const U values[] = {static_cast<U>(v)...};
    return sh::Pick(values, sizeof...(v));
}
unsigned char* Mem(U a) { return sh::Mem(a); }
unsigned char& B(U a) { return Mem(a)[0]; }

// The function being fuzzed (set by the seed; the disturbance keeps each cell
// it moves inside what that function survives - the same in both passes).
unsigned g_current = 0;
bool IsHitBlow(unsigned k) { return k >= kBRun && k <= kBClose; }

// The rooms the functions index by (docs/rest_4c.md section 7): ours aborts
// outside them, so the seed and the disturbance stay inside.
void CountRoom(unsigned k, int* lo, int* hi) {
    *lo = 0, *hi = 9;
    if (k == kHBetInput) *hi = 2;
    else if (k == kBLost || k == kBEnd) *hi = 8;   // they test 8 (the eighth miss) and index no record by it
    else if (IsHitBlow(k)) *hi = 7;
    else if (k == kHPayout || k == kHPick) *hi = 8;
}
unsigned CursorRoom(unsigned k) { return IsHitBlow(k) ? 3 : 0x100; }

// --- the stand-ins' effects (Noise() and the state only: both passes the same) -----

// Input_AutoRepeat: an answer that takes each branch (0x8000, 0x2000, 0x4000,
// 0x1000, none), a third of the time the pressed word as it came.
U RepeatEffect(const U* a, U answer) {
    const U n = sh::Noise();
    if (n % 3 == 0) return a[0];
    static const U kAnswers[] = {0, 0, 0x1000, 0x4000, 0x8000, 0x2000, 0xA000, 0x5000, 0x0100, 0xF000, 0xFFFF0000u};
    return (n >> 2) % 6 == 0 ? answer : kAnswers[(n >> 4) % 11];
}
// 0x5A7570, the POLY_F3 set-up: the primitive's 0x2C bytes written (so a store
// ours made before the call is overwritten as the original's would be).
U PolyF3Effect(const U* a, U answer) {
    auto* const p = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a[0]));
    if (sh::InRegions(p, 0x2C)) sh::FillBytes(p, 0x2C);
    return answer;
}
// Gpu_SetSprt: its 0x1C bytes written (likewise).
U SprtEffect(const U* a, U answer) {
    auto* const p = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a[0]));
    if (sh::InRegions(p, 0x1C)) sh::FillBytes(p, 0x1C);
    return answer;
}
// Item_NamePtr: a pointer into the text buffer (its 16 bytes compared there).
U NameEffect(const U*, U answer) { return Key(sh::Text() + (answer & 0xF0)); }
// The group's own helpers that move the cells their callers read back: the
// card values (CommuHiLo_Shuffle), any one of the block's cells (the draws,
// one call in four: a louder stand-in than the real draw, so a re-read missed
// after a draw is seen).
U ShuffleEffect(const U*, U answer) {
    for (unsigned i = 0; i < 9; ++i) B(at::kCards + i) = static_cast<unsigned char>(1 + sh::Noise() % 9);
    return answer;
}
void MoveCell(U n);
U DrawEffect(const U*, U answer) {
    const U n = sh::Noise();
    if (n % 4 == 0) MoveCell(n >> 2);
    return answer;
}

#define G_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
constexpr sh::Answer kG = sh::Answer::kGarbage;
constexpr U kAll = 0xFFFFFFFFu;
const sh::Callee kCallees[] = {
    // the group's own, called directly (E8 / E9) by the group's; the coordinates
    // are read as s16 (movsx word) or through callees that read 16 bits, the
    // bytes as bytes (the callers push whole registers over them)
    {G_OURS(Commu_DrawCard), 3, {0xFFFF, 0xFFFF, 0xFF}, kG, 0, 0, {}, &DrawEffect},
    {G_OURS(Commu_DrawPiece), 3, {0xFFFF, 0xFFFF, 0xFF}, kG, 0, 0},
    {G_OURS(Commu_DrawZennyBox), 2, {0xFFFF, 0xFFFF}, kG, 0, 0, {}, &DrawEffect},
    {G_OURS(Commu_DrawStakeBox), 3, {0xFFFF, 0xFFFF, 0xFF}, kG, 0, 0, {}, &DrawEffect},
    {G_OURS(Commu_DrawFrame), 2, {0xFFFF, 0xFFFF}, kG, 0, 0},
    {G_OURS(Commu_DrawUnderline), 2, {0xFFFF, 0xFFFF}, kG, 0, 0},
    {G_OURS(CommuHiLo_DrawRow), 3, {0xFF, 0xFF, 0xFF}, kG, 0, 0, {}, &DrawEffect},
    {G_OURS(CommuHiLo_DrawChoice), 4, {0xFFFF, 0xFFFF, 0xFF, 0xFF}, kG, 0, 0, {}, &DrawEffect},
    {G_OURS(CommuHiLo_DrawChoices), 3, {0xFFFF, 0xFFFF, 0xFF}, kG, 0, 0, {}, &DrawEffect},
    {G_OURS(CommuHiLo_DrawMarker), 2, {0xFFFF, 0xFFFF}, kG, 0, 0},
    {G_OURS(CommuHiLo_Shuffle), 0, {}, kG, 0, 0, {}, &ShuffleEffect},
    {G_OURS(Commu_RandDigit), 1, {0xFF}, sh::Answer::kByte, 1, 9},
    {G_OURS(CommuHiLo_Quit), 0, {}, kG, 0, 0},
    // R4D's (wave four), by address (rest_4c_callees.h)
    {"0x45C400", at::kGuessPanel, at::kGuessPanel, 3, {0xFFFF, 0xFFFF, 0xFF}, kG, 0, 0, {}, &DrawEffect},
    {"0x45C7D0", at::kGuessRow, at::kGuessRow, 4, {0xFFFF, 0xFFFF, 0xFF, 0xFF}, kG, 0, 0, {}, &DrawEffect},
    {"0x45C850", at::kSecret, at::kSecret, 3, {0xFFFF, 0xFFFF, 0xFF}, kG, 0, 0, {}, &DrawEffect},
    // Capcom's library POLY_F3 set-up, by address
    {"0x5A7570", at::kSetPolyF3, at::kSetPolyF3, 1, {kAll}, kG, 0, 0, {}, &PolyF3Effect, nullptr, true},
    // ours, re-listed with what this group needs
    {G_OURS(Input_AutoRepeat), 1, {kAll}, kG, 0, 0, {}, &RepeatEffect},
    {G_OURS(Sound_PlayEffect), 1, {0xFFFF}, kG, 0, 0, {}, &DrawEffect},   // louder: the stake and the money are read after sounds
    {G_OURS(Gpu_SetSprt), 1, {kAll}, kG, 0, 0, {16}, &SprtEffect, nullptr, true},
    // the two names' bytes (Item_NamePtr reads each argument's low byte, its
    // evidence; CommuHitBlow_Score pushes eax / ecx over leftovers)
    {G_OURS(Item_NamePtr), 2, {0xFF, 0xFF}, kG, 0, 0, {}, &NameEffect},
};
#undef G_OURS

// The tables the dispatchers read in place, swapped for recorders on both
// sides; each count is its reader's reach (docs/rest_4c.md section 3).
const sh::DataTable kTables[] = {
    {Key(CommuHiLo_Phases), CommuHiLo_Phases_count},
    {Key(CommuHiLo_OpenStates), CommuHiLo_OpenStates_count},
    {Key(CommuHiLo_PlayStates), CommuHiLo_PlayStates_count},
    {Key(CommuHiLo_BetSteps), CommuHiLo_BetSteps_count},
    {Key(CommuHiLo_DealSteps), CommuHiLo_DealSteps_count},
    {Key(CommuHitBlow_Phases), CommuHitBlow_Phases_count},
    {Key(CommuHitBlow_OpenStates), CommuHitBlow_OpenStates_count},
    {Key(CommuHitBlow_PlayStates), CommuHitBlow_PlayStates_count},
    {Key(CommuHitBlow_StartSteps), CommuHitBlow_StartSteps_count},
};

// Beyond field mode's standard regions: the choice bytes 0x939A3E..0x939A41,
// the community block 0x675F78..0x675FC3, the field hook's tail state, and
// Text_Records' first two records (sprintf and the prize name).
const sh::Region kRegions[] = {
    {0x939A30, 0x20},
    {0x675F70, 0x60},
    {0x9039F0, 8},
    {bof3::addr::Text_Records, 0x40},
};

// --- the seed ---------------------------------------------------------------------------

void SeedCount(unsigned k) {
    int lo, hi;
    CountRoom(k, &lo, &hi);
    U v = lo + sh::Next() % (hi - lo + 1);
    // the bounds and one past them where the function survives it
    if (sh::Half()) v = PickOf(lo, hi, hi - 1, lo + 1, v);
    B(at::kCount) = static_cast<unsigned char>(v);
}

void Seed(unsigned k) {
    g_current = k;
    // the choice bytes near every compare; the dispatchers' below their tables
    B(at::kPhase) = static_cast<unsigned char>(PickOf(0, 1, 2, 1, sh::Next()));
    B(at::kState) = static_cast<unsigned char>(PickOf(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, sh::Next()));
    B(at::kStep) = static_cast<unsigned char>(PickOf(0, 0, 1, 2, 3, 4, sh::Next()));
    B(at::kAgain) = static_cast<unsigned char>(PickOf(0, 0, 1, sh::Next()));
    switch (k) {
    case kHRun: B(at::kPhase) = static_cast<unsigned char>(sh::Next() % CommuHiLo_Phases_count); break;
    case kHOpenDispatch: B(at::kState) = static_cast<unsigned char>(sh::Next() % CommuHiLo_OpenStates_count); break;
    case kHPlayDispatch: B(at::kState) = static_cast<unsigned char>(sh::Next() % CommuHiLo_PlayStates_count); break;
    case kHBetDispatch: B(at::kStep) = static_cast<unsigned char>(sh::Next() % CommuHiLo_BetSteps_count); break;
    case kHDealDispatch: B(at::kStep) = static_cast<unsigned char>(sh::Next() % CommuHiLo_DealSteps_count); break;
    case kBRun: B(at::kPhase) = static_cast<unsigned char>(sh::Next() % CommuHitBlow_Phases_count); break;
    case kBOpenDispatch:
        B(at::kState) = static_cast<unsigned char>(sh::Next() % CommuHitBlow_OpenStates_count);
        break;
    case kBPlayDispatch:
        B(at::kState) = static_cast<unsigned char>(sh::Next() % CommuHitBlow_PlayStates_count);
        break;
    case kBStartDispatch:
        B(at::kStep) = static_cast<unsigned char>(sh::Next() % CommuHitBlow_StartSteps_count);
        break;
    case kHBetInput:
        // the tail's draws run at phase 1, state 2
        if (sh::Often()) B(at::kPhase) = 1, B(at::kState) = 2;
        break;
    default: break;
    }
    // the counters at their compares
    B(at::kSlide) = static_cast<unsigned char>(PickOf(0, 1, 2, 3, 4, 5, 6, 1, 4, 0x1E, 0x3C, 0xFF, sh::Next()));
    SeedCount(k);
    const unsigned cursor_room = CursorRoom(k);
    B(at::kCursor) = static_cast<unsigned char>(
        cursor_room == 3 ? sh::Next() % 3 : PickOf(0, 1, 2, 3, 0xFF, 0x80, 0, 1, 2, sh::Next()));
    // the turn 1..8; half the time at the picks' end (count + 1, Reveal's compare)
    B(at::kTurn) = static_cast<unsigned char>(1 + sh::Next() % 8);
    if (sh::Half()) {
        const unsigned end = B(at::kCount) + PickOf(0, 1, 2);
        B(at::kTurn) = static_cast<unsigned char>(end < 1 ? 1 : (end > 8 ? 8 : end));
    }
    B(at::kLost) = static_cast<unsigned char>(PickOf(0, 0, 1, sh::Next()));
    // the stake and the money at the compares (0, 10, 100, 1000, 10000; the
    // stake's three digits from its low byte)
    SetLong(Mem(at::kBet), static_cast<std::int32_t>(PickOf(0, 1, 9, 10, 11, 50, 99, 100, 101, 127, 128, 0xFF, 999,
                                                            1000, 1001, 9999, 10000, 10001, 0xFFFFFFFFu, 0x80000000u,
                                                            sh::Next())));
    SetLong(Mem(bof3::addr::Party_Zenny),
            static_cast<std::int32_t>(PickOf(0, 1, 9, 50, 99, 100, 101, 499, 500, 501, 0x8000, 0xFFFF, 0x1FFFF, 0x10050,
                                             sh::Next())));
    for (unsigned i = 0; i < 3; ++i)
        B(at::kDigits + i) = static_cast<unsigned char>(PickOf(0, 1, 5, 8, 9, 10, 0xFF, 0x80, sh::Next()));
    // the values, the picks and the guess records: digits 1..9 mostly
    for (unsigned i = 0; i < 9; ++i) B(at::kCards + i) = static_cast<unsigned char>(sh::Often() ? 1 + sh::Next() % 9 : sh::Next());
    for (unsigned i = 1; i <= 8; ++i) B(at::kPicks + i) = static_cast<unsigned char>(PickOf(0, 1, 0, 1, sh::Next()));
    for (unsigned i = 0; i < 40; ++i)
        B(at::kRecords + i) = static_cast<unsigned char>(sh::Often() ? 1 + sh::Next() % 9 : sh::Next());
    // a guess sometimes the secret itself, or a permutation of it (three hits,
    // or blows)
    if (k == kBScore && sh::Half()) {
        const int r = static_cast<signed char>(B(at::kCount));
        unsigned char* const rec = Mem(at::kRecords + 5 * r);
        const unsigned shift = sh::Next() % 3;
        for (unsigned j = 0; j < 3; ++j) rec[j] = B(at::kCards + (j + shift) % 3);
    }
    // a pick that matches the card comparison half the time (the win path)
    if (k == kHReveal && sh::Half()) {
        const int t = static_cast<signed char>(B(at::kTurn));
        B(at::kPicks + t) = B(at::kCards + t - 1) >= B(at::kCards + t) ? 1 : 0;
    }
    // the paths that wait on one value: the ninth card dealt, the second
    // game's end at its step 1 with the counter one short of 4
    if (k == kHDealCards && sh::Half()) B(at::kCount) = 8, B(at::kSlide) = 1;
    if (k == kBEnd && sh::Half()) B(at::kStep) = 1, B(at::kSlide) = 3;
    // the message and the music
    Field_Request = static_cast<unsigned char>(PickOf(2, 0, 2, 1, 5, sh::Next()));
    // the pad
    Input_Pressed = static_cast<unsigned short>(PickOf(0, 0x1000, 0x4000, 0x8000, 0x2000, 0xA000, 0xF000, 0x20, 0x40,
                                                       0x60, 0x10, sh::Next()));
    Field_ConfirmButtons = static_cast<unsigned short>(PickOf(0x20, 0x40, 0x60, 0, sh::Next()));
    Field_CancelButtons = static_cast<unsigned short>(PickOf(0x40, 0x10, 0, sh::Next()));
}

// The kCall functions' arguments, after the seed.
void Args(unsigned k, U* a) {
    switch (k) {
    case kCard: a[2] = (a[2] & 0xFFFFFF00u) | PickOf(0xFF, 0, 1, 7, 8, 9, 0x3F, a[2] & 0xFF); break;
    case kPiece: a[2] = (a[2] & 0xFFFFFF00u) | (sh::Next() % at::kPiecesCount); break;
    case kStakeBox: a[2] = (a[2] & 0xFFFFFF00u) | PickOf(0, 1, 2, 3, 4, 5, a[2] & 0xFF); break;
    case kRandDigit: a[0] = (a[0] & 0xFFFFFF00u) | (1 + sh::Next() % 15); break;
    case kChoices: a[2] = (a[2] & 0xFFFFFF00u) | PickOf(0, 1, 2, 3, 0xFF, 4, a[2] & 0xFF); break;
    case kChoice:
        a[2] = (a[2] & 0xFFFFFF00u) | (sh::Next() % at::kChoiceLabelsCount);
        a[3] = (a[3] & 0xFFFFFF00u) | PickOf(0, 1, 2, 3, 0xFF, a[3] & 0xFF);
        break;
    case kRow:
        a[0] = (a[0] & 0xFFFFFF00u) | (sh::Next() % 13);
        a[1] = (a[1] & 0xFFFFFF00u) | (sh::Next() % 10);
        a[2] = (a[2] & 0xFFFFFF00u) | (sh::Next() % 9);
        break;
    default: break;
    }
}

// --- the disturbance: a cell these read again after a call -------------------------------

// One cell of the group's, its value from n alone, inside what the function
// being fuzzed survives.
void MoveCell(U n) {
    const auto b = static_cast<unsigned char>(n >> 8);
    const U v = n >> 4;
    switch (sh::DisturbCase(n, 11)) {
    case 0: {
        static const unsigned char kSlides[] = {0, 1, 2, 3, 4, 5, 0x1E, 0x3C};
        B(at::kSlide) = (b & 1) ? kSlides[(b >> 1) % 8] : b;
        break;
    }
    case 1: {
        int lo, hi;
        CountRoom(g_current, &lo, &hi);
        B(at::kCount) = static_cast<unsigned char>(lo + b % (hi - lo + 1));
        break;
    }
    case 2: B(at::kCursor) = static_cast<unsigned char>(CursorRoom(g_current) == 3 ? b % 3 : b); break;
    case 3: B(at::kTurn) = static_cast<unsigned char>(1 + b % 8); break;
    case 4: B(at::kPhase + (b & 3) % 3) = static_cast<unsigned char>((b >> 2) % 14); break;
    case 5: SetLong(Mem(at::kBet), static_cast<std::int32_t>((b & 1) ? (b >> 1) % 2 * 10000 + v % 3 : v)); break;
    case 6: SetLong(Mem(bof3::addr::Party_Zenny), static_cast<std::int32_t>(v)); break;
    case 7: B(at::kCards + b % 0x2B) = static_cast<unsigned char>(1 + (v >> 8) % 9); break;
    case 8: B(at::kDigits + b % 3) = static_cast<unsigned char>(static_cast<int>((v >> 8) % 12) - 1); break;
    case 9: B((b & 1) ? at::kLost : at::kAgain) = static_cast<unsigned char>((b & 2) ? 0 : b); break;
    case 10: B(at::kTailState) = b; break;
    default: break;
    }
}

void Disturb(U h) { MoveCell(h); }

}  // namespace

void SelfTest() {
    // BOF3X_R4C_ONLY: the clones whose name contains it (a control's run)
    static sh::Clone chosen[kCount];
    static unsigned index[kCount];
    const char* const only = std::getenv("BOF3X_R4C_ONLY");
    unsigned n = 0;
    for (unsigned k = 0; k < kCount; ++k)
        if (!only || !*only || std::strstr(kClones[k].name, only)) {
            index[n] = k;
            chosen[n++] = kClones[k];
        }
    if (n == 0) bof3::Fatal("rest_4c: BOF3X_R4C_ONLY=%s names no clone", only);
    static unsigned* s_index = index;
    sh::Group g = {"rest_4c", chosen, n, kCallees, sizeof kCallees / sizeof kCallees[0], kTables,
                   sizeof kTables / sizeof kTables[0], kRegions, sizeof kRegions / sizeof kRegions[0],
                   [](unsigned k) { Seed(s_index[k]); }, &Disturb, 6000};
    g.args = [](unsigned k, U* a) { Args(s_index[k], a); };
    g.field = true;
    sh::Run(g);
}

}  // namespace rest_4c

// BOF3X_SHADOW=effect_6d: group E6D's 51 functions through the scenario harness
// in effect mode (scenario_harness.h, docs/scenario_harness.md section 8), once
// at start-up. docs/effect_6d.md section 4. BOF3X_E6D_ONLY=<name> runs the
// clones whose name contains it (the controls' speed-up).
//
// The clone table is tools/band_rows.py --group E6D --clones --harness scenario
// (2026-10-03), each extent read again to its last instruction (capstone) and
// the names given; every extent is the tool's. Shapes: the five sub-state
// dispatchers, the states, sub-kind 0x61's run and steps and the draws that
// take no argument kEffect (Sprite_Current one of the 20 Effect_Objects
// records, +5 0x18, a dispatcher's +2 below its table's length); the six draws
// with arguments kCall. No function answers.
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_6d.h"
#include "game/effect_6d_callees.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace effect_6d {
namespace {

namespace sh = scenario_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using U = std::uint32_t;

// tools/band_rows.py --group E6D --clones, 2026-10-03.
constexpr sh::CallSite kCalls514290[] = {{0x51, 0x57C0F0}, {0x6E, 0x5144F0}, {0x77, 0x5144F0}};
constexpr sh::CallSite kCalls514310[] = {{0x6A, 0x57C110}, {0x80, 0x587740}, {0x94, 0x5144F0}, {0x9D, 0x5144F0}};
constexpr sh::CallSite kCalls5143C0[] = {{0x37, 0x5144F0}, {0x40, 0x5144F0}};
constexpr sh::CallSite kCalls514470[] = {{0x1F, 0x57C0F0}, {0x35, 0x587740}, {0x5F, 0x5144F0}, {0x68, 0x5144F0}};
constexpr sh::CallSite kCalls5144F0[] = {{0x7F, 0x5A77C0}, {0x94, 0x572FA0}, {0xA0, 0x5A75D0},  {0xA8, 0x5A77A0},
                                         {0xC7, 0x5B9550}, {0xE1, 0x5B9550}, {0x12D, 0x5A85F0}, {0x133, 0x5A9290},
                                         {0x15E, 0x572A00}, {0x173, 0x572FA0}};
constexpr sh::CallSite kCalls5146B0[] = {{0x88, 0x514880}};
constexpr sh::CallSite kCalls514740[] = {{0x64, 0x587740}, {0x75, 0x514880}};
constexpr sh::CallSite kCalls5147C0[] = {{0x1A, 0x514880}};
constexpr sh::CallSite kCalls5147E0[] = {{0x59, 0x514880}};
constexpr sh::CallSite kCalls514840[] = {{0x24, 0x587740}, {0x36, 0x514880}};
constexpr sh::CallSite kCalls514880[] = {{0x43, 0x5A77C0},  {0x4C, 0x461E50},  {0x68, 0x5A75D0},  {0x70, 0x5A77A0},
                                         {0xEC, 0x5720C0},  {0x103, 0x5720C0}, {0x140, 0x5720C0}, {0x157, 0x5720C0},
                                         {0x1A0, 0x5A85F0}, {0x1A9, 0x5A9290}, {0x1B9, 0x572A00}, {0x1C2, 0x461E50}};
constexpr sh::CallSite kCalls514A90[] = {{0x148, 0x514DF0}};
constexpr sh::CallSite kCalls514BE0[] = {{0xC7, 0x587740}, {0xD8, 0x514DF0}};
constexpr sh::CallSite kCalls514CC0[] = {{0x1A, 0x514DF0}};
constexpr sh::CallSite kCalls514CE0[] = {{0x64, 0x514DF0}, {0xBB, 0x514DF0}};
constexpr sh::CallSite kCalls514DB0[] = {{0x24, 0x587740}, {0x36, 0x514DF0}};
constexpr sh::CallSite kCalls514DF0[] = {{0xA1, 0x5A77C0},  {0xB7, 0x572FA0},  {0xC3, 0x5A75D0},  {0xCB, 0x5A77A0},
                                         {0x1BB, 0x5A85F0}, {0x1C1, 0x5A9290}, {0x1DE, 0x572A00}, {0x1F4, 0x572FA0}};
constexpr sh::CallSite kCalls515000[] = {{0xE, 0x515440}, {0x13, 0x515E00}};
constexpr sh::CallSite kCalls515040[] = {{0x25, 0x5151A0}};
constexpr sh::CallSite kCalls515070[] = {{0x2, 0x5151A0}};
constexpr sh::CallSite kCalls515080[] = {{0x10, 0x5156B0}, {0x1E, 0x5151A0}, {0x2C, 0x5156B0},
                                         {0x48, 0x5151A0}, {0x63, 0x5151A0}, {0x77, 0x5151A0}};
constexpr sh::CallSite kCalls515150[] = {{0x0, 0x515160}};
constexpr sh::CallSite kCalls5151A0[] = {{0x45, 0x5A77C0},  {0x4E, 0x461E50},  {0x5A, 0x5A75D0},  {0x62, 0x5A77A0},
                                         {0xB0, 0x461E50},  {0xBC, 0x5A75D0},  {0xC4, 0x5A77A0},  {0x112, 0x461E50},
                                         {0x14E, 0x5A77C0}, {0x157, 0x461E50}, {0x16F, 0x5A75D0}, {0x177, 0x5A77A0},
                                         {0x281, 0x461E50}};
constexpr sh::CallSite kCalls515440[] = {{0x91, 0x572F70},  {0xAA, 0x5A75D0},  {0xB2, 0x5A77A0},  {0x12A, 0x572A00},
                                         {0x144, 0x572FA0}, {0x15D, 0x572F70}, {0x176, 0x5A75D0}, {0x17E, 0x5A77A0},
                                         {0x1F5, 0x572A00}, {0x218, 0x572FA0}};
constexpr sh::CallSite kCalls5156B0[] = {{0x11, 0x5A77C0},  {0x1A, 0x461E50},  {0x8D, 0x515CC0},  {0xBF, 0x5A7A50},
                                         {0xEA, 0x5A7A00},  {0x11C, 0x515950}, {0x139, 0x5A7A50}, {0x164, 0x5A7A00},
                                         {0x196, 0x515950}, {0x1B3, 0x5A7A50}, {0x1E0, 0x5A7A00}, {0x214, 0x515950},
                                         {0x231, 0x5A7A50}, {0x25C, 0x5A7A00}, {0x28E, 0x515950}};
constexpr sh::CallSite kCalls515950[] = {{0x12, 0x5A7610},  {0x1A, 0x5A7780},  {0x2D, 0x5A7A00},  {0x5E, 0x5A7A50},
                                         {0x9B, 0x5A7A00},  {0xC8, 0x5A7A50},  {0xF5, 0x5A7A00},  {0x11D, 0x5A7A50},
                                         {0x145, 0x5A7A00}, {0x16D, 0x5A7A50}, {0x1C2, 0x461E50}, {0x1CE, 0x5A7610},
                                         {0x1D6, 0x5A7780}, {0x1DF, 0x5A7A00}, {0x207, 0x5A7A50}, {0x22F, 0x5A7A00},
                                         {0x257, 0x5A7A50}, {0x27F, 0x5A7A00}, {0x2AC, 0x5A7A50}, {0x2D9, 0x5A7A00},
                                         {0x2FF, 0x5A7A50}, {0x352, 0x461E50}};
constexpr sh::CallSite kCalls515CC0[] = {{0x24, 0x5A75F0}, {0x2C, 0x5A7780}, {0x4D, 0x5A7A00}, {0x73, 0x5A7A50},
                                         {0xAC, 0x5A7A00}, {0xD2, 0x5A7A50}, {0x120, 0x461E50}};
constexpr sh::CallSite kCalls515E00[] = {{0x4E, 0x5A7A50},  {0xBC, 0x5A77C0},  {0xC5, 0x461E50},  {0xD1, 0x5A75D0},
                                         {0xD9, 0x5A77A0},  {0xE1, 0x5A7780},  {0x181, 0x461E50}, {0x18D, 0x5A75D0},
                                         {0x195, 0x5A77A0}, {0x1A0, 0x5A7780}, {0x214, 0x461E50}, {0x253, 0x5A77C0},
                                         {0x25C, 0x461E50}};
constexpr sh::CallSite kCalls516090[] = {{0x48, 0x5A7A00}, {0x64, 0x5A7A00}, {0x85, 0x5A7A00}, {0xA5, 0x5A7A00}, {0x1BB, 0x5720C0}};
constexpr sh::CallSite kCalls516280[] = {{0x1F, 0x5A77C0},  {0x28, 0x461E50},  {0x39, 0x5A7610},  {0x41, 0x5A7780},
                                         {0xAB, 0x5A7A00},  {0xF0, 0x5A7A50},  {0x16D, 0x5A85F0}, {0x173, 0x5A9350},
                                         {0x1C4, 0x461E50}, {0x1E7, 0x5A77C0}, {0x1F0, 0x461E50}};
constexpr sh::CallSite kCalls5164A0[] = {{0x69, 0x5167C0}};
constexpr sh::CallSite kCalls516520[] = {{0x2A, 0x5167C0}};
constexpr sh::CallSite kCalls516560[] = {{0xC2, 0x5167C0}, {0xD7, 0x5167C0}};
constexpr sh::CallSite kCalls516640[] = {{0x28, 0x5167C0}, {0x3C, 0x589840}};
constexpr sh::CallSite kCalls516690[] = {{0x26, 0x5167C0}};
constexpr sh::CallSite kCalls5166C0[] = {{0x57, 0x5167C0}};
constexpr sh::CallSite kCalls516730[] = {{0x21, 0x5166C0}, {0x7E, 0x5167C0}};
constexpr sh::CallSite kCalls5167C0[] = {{0xB5, 0x5A77C0},  {0xCB, 0x572FA0},  {0xD7, 0x5A75D0},  {0xDF, 0x5A77A0},
                                         {0x157, 0x5720C0}, {0x18E, 0x5720C0}, {0x1E1, 0x5A85F0}, {0x1E7, 0x5A9290},
                                         {0x1F6, 0x572A00}, {0x20C, 0x572FA0}};
constexpr sh::CallSite kCalls516A10[] = {{0x2, 0x516A90}};
constexpr sh::CallSite kCalls516A50[] = {{0x19, 0x516A90}, {0x2D, 0x589840}};

#define E6D_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define E6D_CALLS(a) a, E6D_N(a)
#define E6D_FN(name) reinterpret_cast<const void*>(&::name)
constexpr sh::Shape kEf = sh::Shape::kEffect;
constexpr sh::Shape kCa = sh::Shape::kCall;
// {name, base, size, calls, imms, tables, ours, ret_mask, calm, shape, pointers, state_span, sub_span, kind}
const sh::Clone kAll[] = {
    {"EffectKind18Sub5C_Run", 0x514270, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E6D_FN(EffectKind18Sub5C_Run), 0, false, kEf, 0, 0, 5, 0x18},
    {"EffectKind18Sub5C_Place", 0x514290, 0x80, E6D_CALLS(kCalls514290), nullptr, 0, nullptr, 0, E6D_FN(EffectKind18Sub5C_Place), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub5C_WaitNear", 0x514310, 0xA7, E6D_CALLS(kCalls514310), nullptr, 0, nullptr, 0, E6D_FN(EffectKind18Sub5C_WaitNear), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub5C_Open", 0x5143C0, 0x49, E6D_CALLS(kCalls5143C0), nullptr, 0, nullptr, 0, E6D_FN(EffectKind18Sub5C_Open), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub5C_WaitFar", 0x514410, 0x5B, nullptr, 0, nullptr, 0, nullptr, 0, E6D_FN(EffectKind18Sub5C_WaitFar), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub5C_Close", 0x514470, 0x71, E6D_CALLS(kCalls514470), nullptr, 0, nullptr, 0, E6D_FN(EffectKind18Sub5C_Close), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub5C_DrawPanel", 0x5144F0, 0x194, E6D_CALLS(kCalls5144F0), nullptr, 0, nullptr, 0, E6D_FN(EffectKind18Sub5C_DrawPanel), 0, false, kCa, 0, 0, 0, 0x18},
    {"EffectKind18Sub5E_Run", 0x514690, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E6D_FN(EffectKind18Sub5E_Run), 0, false, kEf, 0, 0, 5, 0x18},
    {"EffectKind18Sub5E_Start", 0x5146B0, 0x8F, E6D_CALLS(kCalls5146B0), nullptr, 0, nullptr, 0, E6D_FN(EffectKind18Sub5E_Start), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub5E_WaitNear", 0x514740, 0x7C, E6D_CALLS(kCalls514740), nullptr, 0, nullptr, 0, E6D_FN(EffectKind18Sub5E_WaitNear), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub5E_Open", 0x5147C0, 0x1F, E6D_CALLS(kCalls5147C0), nullptr, 0, nullptr, 0, E6D_FN(EffectKind18Sub5E_Open), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub5E_WaitFar", 0x5147E0, 0x60, E6D_CALLS(kCalls5147E0), nullptr, 0, nullptr, 0, E6D_FN(EffectKind18Sub5E_WaitFar), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub5E_Close", 0x514840, 0x3B, E6D_CALLS(kCalls514840), nullptr, 0, nullptr, 0, E6D_FN(EffectKind18Sub5E_Close), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub5E_Draw", 0x514880, 0x1E8, E6D_CALLS(kCalls514880), nullptr, 0, nullptr, 0, E6D_FN(EffectKind18Sub5E_Draw), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub5D_Run", 0x514A70, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E6D_FN(EffectKind18Sub5D_Run), 0, false, kEf, 0, 0, 5, 0x18},
    {"EffectKind18Sub5D_Place", 0x514A90, 0x14F, E6D_CALLS(kCalls514A90), nullptr, 0, nullptr, 0, E6D_FN(EffectKind18Sub5D_Place), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub5D_WaitNear", 0x514BE0, 0xDF, E6D_CALLS(kCalls514BE0), nullptr, 0, nullptr, 0, E6D_FN(EffectKind18Sub5D_WaitNear), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub5D_Open", 0x514CC0, 0x1F, E6D_CALLS(kCalls514CC0), nullptr, 0, nullptr, 0, E6D_FN(EffectKind18Sub5D_Open), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub5D_WaitFar", 0x514CE0, 0xC2, E6D_CALLS(kCalls514CE0), nullptr, 0, nullptr, 0, E6D_FN(EffectKind18Sub5D_WaitFar), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub5D_Close", 0x514DB0, 0x3B, E6D_CALLS(kCalls514DB0), nullptr, 0, nullptr, 0, E6D_FN(EffectKind18Sub5D_Close), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub5D_Draw", 0x514DF0, 0x20D, E6D_CALLS(kCalls514DF0), nullptr, 0, nullptr, 0, E6D_FN(EffectKind18Sub5D_Draw), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub61_Run", 0x515000, 0x18, E6D_CALLS(kCalls515000), nullptr, 0, nullptr, 0, E6D_FN(EffectKind18Sub61_Run), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub61_WaitFocus", 0x515020, 0x16, nullptr, 0, nullptr, 0, nullptr, 0, E6D_FN(EffectKind18Sub61_WaitFocus), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub61_Slide", 0x515040, 0x2C, E6D_CALLS(kCalls515040), nullptr, 0, nullptr, 0, E6D_FN(EffectKind18Sub61_Slide), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub61_Hold", 0x515070, 0x9, E6D_CALLS(kCalls515070), nullptr, 0, nullptr, 0, E6D_FN(EffectKind18Sub61_Hold), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub61_Rise", 0x515080, 0x7E, E6D_CALLS(kCalls515080), nullptr, 0, nullptr, 0, E6D_FN(EffectKind18Sub61_Rise), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub61_Reset", 0x515100, 0x4F, nullptr, 0, nullptr, 0, nullptr, 0, E6D_FN(EffectKind18Sub61_Reset), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub61_Rearm", 0x515150, 0xD, E6D_CALLS(kCalls515150), nullptr, 0, nullptr, 0, E6D_FN(EffectKind18Sub61_Rearm), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub61_ClearTracks", 0x515160, 0x3F, nullptr, 0, nullptr, 0, nullptr, 0, E6D_FN(EffectKind18Sub61_ClearTracks), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub61_DrawOverlay", 0x5151A0, 0x29C, E6D_CALLS(kCalls5151A0), nullptr, 0, nullptr, 0, E6D_FN(EffectKind18Sub61_DrawOverlay), 0, false, kCa, 0, 0, 0, 0x18},
    {"EffectKind18Sub61_DrawTracks", 0x515440, 0x26B, E6D_CALLS(kCalls515440), nullptr, 0, nullptr, 0, E6D_FN(EffectKind18Sub61_DrawTracks), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub61_DrawGlow", 0x5156B0, 0x299, E6D_CALLS(kCalls5156B0), nullptr, 0, nullptr, 0, E6D_FN(EffectKind18Sub61_DrawGlow), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub61_DrawRing", 0x515950, 0x370, E6D_CALLS(kCalls515950), nullptr, 0, nullptr, 0, E6D_FN(EffectKind18Sub61_DrawRing), 0, false, kCa, 0, 0, 0, 0x18},
    {"EffectKind18Sub61_DrawDisc", 0x515CC0, 0x13C, E6D_CALLS(kCalls515CC0), nullptr, 0, nullptr, 0, E6D_FN(EffectKind18Sub61_DrawDisc), 0, false, kCa, 0, 0, 0, 0x18},
    {"EffectKind18Sub61_DrawMist", 0x515E00, 0x286, E6D_CALLS(kCalls515E00), nullptr, 0, nullptr, 0, E6D_FN(EffectKind18Sub61_DrawMist), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub62_Ripple", 0x516090, 0x1E2, E6D_CALLS(kCalls516090), nullptr, 0, nullptr, 0, E6D_FN(EffectKind18Sub62_Ripple), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub63_Glow", 0x516280, 0x200, E6D_CALLS(kCalls516280), nullptr, 0, nullptr, 0, E6D_FN(EffectKind18Sub63_Glow), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub64_Run", 0x516480, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E6D_FN(EffectKind18Sub64_Run), 0, false, kEf, 0, 0, 7, 0x18},
    {"EffectKind18Sub64_Place", 0x5164A0, 0x72, E6D_CALLS(kCalls5164A0), nullptr, 0, nullptr, 0, E6D_FN(EffectKind18Sub64_Place), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub64_WaitBattle", 0x516520, 0x33, E6D_CALLS(kCalls516520), nullptr, 0, nullptr, 0, E6D_FN(EffectKind18Sub64_WaitBattle), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub64_Watch", 0x516560, 0xE0, E6D_CALLS(kCalls516560), nullptr, 0, nullptr, 0, E6D_FN(EffectKind18Sub64_Watch), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub64_Fade", 0x516640, 0x42, E6D_CALLS(kCalls516640), nullptr, 0, nullptr, 0, E6D_FN(EffectKind18Sub64_Fade), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub64_Hold", 0x516690, 0x2F, E6D_CALLS(kCalls516690), nullptr, 0, nullptr, 0, E6D_FN(EffectKind18Sub64_Hold), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub64_Pulse", 0x5166C0, 0x62, E6D_CALLS(kCalls5166C0), nullptr, 0, nullptr, 0, E6D_FN(EffectKind18Sub64_Pulse), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub64_Swell", 0x516730, 0x8A, E6D_CALLS(kCalls516730), nullptr, 0, nullptr, 0, E6D_FN(EffectKind18Sub64_Swell), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub64_Draw", 0x5167C0, 0x22D, E6D_CALLS(kCalls5167C0), nullptr, 0, nullptr, 0, E6D_FN(EffectKind18Sub64_Draw), 0, false, kCa, 0, 0, 0, 0x18},
    {"EffectKind18Sub65_Run", 0x5169F0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E6D_FN(EffectKind18Sub65_Run), 0, false, kEf, 0, 0, 3, 0x18},
    {"EffectKind18Sub65_Start", 0x516A10, 0x13, E6D_CALLS(kCalls516A10), nullptr, 0, nullptr, 0, E6D_FN(EffectKind18Sub65_Start), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub65_Wait", 0x516A30, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E6D_FN(EffectKind18Sub65_Wait), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub65_FadeIn", 0x516A50, 0x33, E6D_CALLS(kCalls516A50), nullptr, 0, nullptr, 0, E6D_FN(EffectKind18Sub65_FadeIn), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub65_ShadeCluts", 0x516A90, 0x9E, nullptr, 0, nullptr, 0, nullptr, 0, E6D_FN(EffectKind18Sub65_ShadeCluts), 0, false, kCa, 0, 0, 0, 0x18},
};
#undef E6D_FN
#undef E6D_CALLS
#undef E6D_N

enum : unsigned {
    k5CRun, k5CPlace, k5CWaitNear, k5COpen, k5CWaitFar, k5CClose, k5CDraw,
    k5ERun, k5EStart, k5EWaitNear, k5EOpen, k5EWaitFar, k5EClose, k5EDraw,
    k5DRun, k5DPlace, k5DWaitNear, k5DOpen, k5DWaitFar, k5DClose, k5DDraw,
    k61Run, k61WaitFocus, k61Slide, k61Hold, k61Rise, k61Reset, k61Rearm, k61Clear,
    k61Overlay, k61Tracks, k61Glow, k61Ring, k61Disc, k61Mist,
    k62Ripple, k63Glow,
    k64Run, k64Place, k64WaitBattle, k64Watch, k64Fade, k64Hold, k64Pulse, k64Swell, k64Draw,
    k65Run, k65Start, k65Wait, k65FadeIn, k65Shade, kCount
};
static_assert(kCount == sizeof kAll / sizeof kAll[0], "one enum entry a clone, in order");

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
unsigned char* Mem(U a) { return sh::Mem(a); }
template <typename... T> U PickOf(T... v) {
    const U values[] = {static_cast<U>(v)...};
    return sh::Pick(values, sizeof...(v));
}

// --- the effects ---------------------------------------------------------------------

// The group's own draws read Sprite_Current's record: its address and the
// dwords +0 (the state bytes), +8 (+8, the counter +9, the frame +0xA, the
// variant +0xB) and +0x30 (the slide) logged, so a draw on the wrong record, or
// before a state's write it should follow, shows.
U FxCurrent(const U*, U answer) {
    unsigned char* const s = Sprite_Current;
    const bool in = sh::InRegions(s, 0x80);
    sh::Note(Key(s), in ? static_cast<U>(Long(s)) : 0u, in ? static_cast<U>(Long(s + 8)) : 0u,
             in ? static_cast<U>(Long(s + 0x30)) : 0u);
    return answer;
}

#define E6D_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
constexpr sh::Answer kG = sh::Answer::kGarbage;
constexpr U kW = 0xFFFFFFFFu;
const sh::Callee kCallees[] = {
    // the group's own called directly, by name. The callers push immediates or
    // whole computed words; a byte is read of DrawRing's and DrawDisc's shade
    // (mov bl / al) and of Sub64_Draw's step (mov cl, [esp + 0xC]).
    {E6D_OURS(EffectKind18Sub5C_DrawPanel), 2, {kW, kW}, kG, 0, 0, {}, &FxCurrent},
    {E6D_OURS(EffectKind18Sub5E_Draw), 0, {}, kG, 0, 0, {}, &FxCurrent},
    {E6D_OURS(EffectKind18Sub5D_Draw), 0, {}, kG, 0, 0, {}, &FxCurrent},
    {E6D_OURS(EffectKind18Sub61_DrawOverlay), 1, {kW}, kG, 0, 0, {}, &FxCurrent},
    {E6D_OURS(EffectKind18Sub61_DrawTracks), 0, {}, kG, 0, 0, {}, &FxCurrent},
    {E6D_OURS(EffectKind18Sub61_DrawGlow), 0, {}, kG, 0, 0, {}, &FxCurrent},
    {E6D_OURS(EffectKind18Sub61_DrawRing), 4, {kW, kW, kW, 0xFF}, kG, 0, 0, {}, &FxCurrent},
    {E6D_OURS(EffectKind18Sub61_DrawDisc), 4, {kW, kW, kW, 0xFF}, kG, 0, 0, {}, &FxCurrent},
    {E6D_OURS(EffectKind18Sub61_DrawMist), 0, {}, kG, 0, 0, {}, &FxCurrent},
    {E6D_OURS(EffectKind18Sub61_ClearTracks), 0, {}, kG, 0, 0, {}, &FxCurrent},
    {E6D_OURS(EffectKind18Sub64_Pulse), 0, {}, kG, 0, 0, {}, &FxCurrent},
    {E6D_OURS(EffectKind18Sub64_Draw), 4, {kW, kW, 0xFF, kW}, kG, 0, 0, {}, &FxCurrent},
    {E6D_OURS(EffectKind18Sub65_ShadeCluts), 1, {kW}, kG, 0, 0, {}, &FxCurrent},
};
#undef E6D_OURS

// The tables the dispatchers jump (or call) through, read in place; each
// table's own length (symbols.toml [[data]], none bounded by a compare).
const sh::DataTable kTables[] = {
    {0x65F374, 5}, {0x65F40C, 5}, {0x65F424, 5}, {0x65F450, 7}, {0x65F55C, 7}, {0x65F57C, 3},
};
const std::uint8_t kKinds[] = {0x18};

// The cells the group reads that the effect mode's standard regions lack.
const sh::Region kRegions[] = {
    {at::kTrackRing, at::kTrackEntries * at::kTrackStride + 4},   // the tracks' ring and the two heads
    {at::kBattleWord, 4},                                         // the battle dword sub-kind 0x64 scales
    {at::kEnemyRecords, at::kEnemyStride * 2},                    // the two enemy records +0xC may point at
    {at::kClutRows, 2 * at::kClutWords},                          // CLUT rows 4 and 5 as loaded
    {at::kClutRowsLive, 2 * at::kClutWords},                      // ... and live
};

// --- the seed ------------------------------------------------------------------------

unsigned char* Rec(unsigned r) { return sh::EffectRecord(r); }
std::int32_t S16(const unsigned char* p) { return static_cast<std::int16_t>(Word(p)); }

// A 16.16 point about a cell: a whole cell -5..+5 from it and a fraction at the
// tests' boundaries (half a cell, a cell, one either side).
U Around(std::int32_t cell) {
    const std::int32_t d = static_cast<std::int32_t>(sh::Next() % 11) - 5;
    const U frac = PickOf(0, 1, 0xFFFF, 0x8000, 0x7FFF, 0x8001, sh::Next() & 0xFFFF);
    return (static_cast<U>(cell + d) << 16) + frac;
}
// A slide +0x30 one step (0x10) from its ends and one either side: 0xC0 and
// 0x80 open, 0 shut - and at them.
U Slide() {
    return PickOf(0, 0xF, 0x10, 0x11, 0xFFF0, 0x6F, 0x70, 0x71, 0x80, 0x81, 0x90, 0xAF, 0xB0, 0xB1, 0xC0, sh::Next());
}
// A cell word: small (0..0x7F) or random.
U Cell() { return sh::Often() ? sh::Next() % 0x80 : sh::Next() & 0xFFFF; }
// A record's counter +9 at the states' and the glow's boundaries.
U Counter() {
    return PickOf(0, 1, 3, 4, 5, 7, 8, 9, 0xE, 0xF, 0x10, 0x13, 0x14, 0x15, 0x1E, 0x1F, 0x20, 0x2B, 0x2C, 0x2D, 0x3D, 0x3E,
                  0x3F, 0x40, 0x77, 0x78, 0x79, 0x7E, 0x7F, 0x80, 0xFE, 0xFF, sh::Next());
}

void Seed(unsigned k) {
    // every record the disturbance may move Sprite_Current to: the slide at
    // its ends, +8 the axis (0 half the time), +9 at its boundaries, +0xA a
    // frame about 11, +0xB a variant (sub-kind 0x5C's draw indexes by it), +0xC
    // one of the two enemy records (sub-kind 0x64's states read through it)
    for (unsigned r = 0; r < 20; ++r) {
        unsigned char* const e = Rec(r);
        SetWord(e + 0x30, Slide());
        if (sh::Half()) e[8] = 0;
        e[9] = static_cast<unsigned char>(Counter());
        e[0xA] = static_cast<unsigned char>(PickOf(0, 1, 5, 9, 0xA, 0xB, 0xC, 0xD, 0xFF, sh::Next()));
        e[0xB] = static_cast<unsigned char>(sh::Next() % at::kSub5CVariants);
        SetLong(e + 0xC, static_cast<std::int32_t>(at::kEnemyRecords + at::kEnemyStride * (sh::Next() & 1)));
        SetLong(e + 0x38, static_cast<std::int32_t>(
                              PickOf(0x5F, 0x60, 0x61, 0x7F, 0x80, 0x81, 0xFF, 0x100, 0x101, 0, 0xFFFFFFFFu, sh::Next())));
    }
    unsigned char* const s = Sprite_Current;
    SetWord(s + 0x36, Cell());
    SetWord(s + 0x3A, Cell());
    if (sh::Half()) SetWord(s + 0x3A, 0);   // 0x5D's axis from the spawn's z cell
    // the cell the leader is tested against: the table's for a place state
    // (after the write), the record's own otherwise
    std::int32_t cx = S16(s + 0x36), cz = S16(s + 0x3A);
    switch (k) {
    case k5CPlace: {
        const U v = sh::Next() % at::kSub5CVariants;
        SetWord(s + 0x36, v);
        cx = Mem(at::kSub5CCells + 2 * v)[0];
        cz = Mem(at::kSub5CCells + 2 * v + 1)[0];
        break;
    }
    case k5DPlace: {
        const U v = sh::Next() % at::kSub5DVariants;
        SetWord(s + 0x36, v);
        cx = Mem(at::kSub5DCells + 2 * v)[0];
        cz = Mem(at::kSub5DCells + 2 * v + 1)[0];
        break;
    }
    case k64Place: SetWord(s + 0x36, sh::Next() % at::kSub64Variants); break;
    default: break;
    }
    // the leader about the cell seven times in eight
    if (sh::Next() % 8 != 0) {
        SetLong(Mem(at::kLeaderX), static_cast<std::int32_t>(Around(cx)));
        SetLong(Mem(at::kLeaderZ), static_cast<std::int32_t>(Around(cz)));
    }
    if (sh::Half()) Field_Request = 0;
    if (sh::Half()) Frame_Counter &= ~3u;
    // Cond_ByteFE: 1 half the time for 0x5C's waits; inside 0x61's seven steps
    // for its run, every one of them reached
    if (k == k61Run) Cond_ByteFE = static_cast<unsigned char>(sh::Next() % 7);
    else if (sh::Half()) Cond_ByteFE = 1;
    if (sh::Half()) MapView_FocusX = 0x16FF;
    if (sh::Half()) Draw_PassFlags |= 4;
    // the tracks: the heads inside the ring, the marks' cells about the
    // followers' x cells
    for (unsigned r = 0; r < at::kFollowerCount; ++r) {
        Mem(at::kTrackHeads)[r] = static_cast<unsigned char>(sh::Next() % at::kTrackEntries);
        unsigned char* const f = Mem(at::kFollowers + at::kFollowerStride * r);
        const U fx = Long(f);
        const std::int32_t cell = static_cast<std::int32_t>(fx + 0x4000u) >> 16;
        const unsigned h = Mem(at::kTrackHeads)[r];
        unsigned char* const last = Mem(at::kTrackRing + at::kTrackStride * h + 3 * r);
        if (sh::Often()) last[0] = static_cast<unsigned char>(cell + PickOf(0, 1, 0xFFFFFFFFu, 2));
        if (sh::Half()) SetLong(f, static_cast<std::int32_t>((fx & 0x00FFFFFFu) | (sh::Half() ? 0 : 0xFF000000u)));
    }
    // the mist's counter +2 at its boundaries
    if (k == k61Mist || k == k61Run)
        s[2] = static_cast<unsigned char>(
            PickOf(0, 1, 4, 0x3F, 0x40, 0x41, 0x7F, 0x80, 0x81, 0xBF, 0xC0, 0xC1, 0xFC, sh::Next()));
    // sub-kind 0x62: Field_Kind2's cells about the map (header below 0x20)
    if (k == k62Ripple) {
        SetWord(Mem(at::kKind2ZCell), PickOf(0, 0x10, 0x14, 0x20, 0x30, 0x34, 0x40, sh::Next() % 0x40, sh::Next()));
        SetWord(Mem(at::kKind2XCell), PickOf(0, 0x10, 0x14, 0x20, 0x30, 0x34, 0x40, sh::Next() % 0x40, sh::Next()));
    }
    // sub-kind 0x64: battle mode 5 and the watched record's state pairs
    if (sh::Half()) Game_Mode = 5;
    if (sh::Half()) Game_Step = 5;
    for (unsigned e = 0; e < 2; ++e) {
        unsigned char* const r = Mem(at::kEnemyRecords + at::kEnemyStride * e);
        static const unsigned char kPairs[][2] = {{8, 2}, {6, 1}, {6, 2}, {6, 3}, {6, 4}, {3, 1}, {8, 1}, {6, 5}};
        if (sh::Often()) {
            const unsigned p = sh::Next() % 8;
            r[1] = kPairs[p][0];
            r[2] = kPairs[p][1];
        }
    }
    // sub-kind 0x65's wait
    if (sh::Half()) Mem(at::kCounter48)[0] = 0xD;
}

// The draws' arguments: the 5C panel's side inside its table (0, 1) and dy
// the callers' -2..1 or any; the overlay's y small or any; the ring's and the
// disc's centre about the screen, a radius small or any, a shade; sub-kind
// 0x64's draw at the callers' values or any; the shade level 0..0x80 or any.
void Args(unsigned k, U* a) {
    switch (k) {
    case k5CDraw:
        a[0] = sh::Next() % at::kSub5CSidesUsed;
        a[1] = PickOf(1, 0xFFFFFFFFu, 0xFFFFFFFEu, 0, sh::Next());
        break;
    case k61Overlay: a[0] = PickOf(0, 0x54, 0x60, 0x100, sh::Next() % 0x200, sh::Next()); break;
    case k61Ring:
    case k61Disc:
        if (sh::Often()) {
            a[0] = 0x60 + sh::Next() % 0x40;
            a[1] = sh::Next() % 0x100;
            a[2] = sh::Next() % 0x200;
        }
        break;
    case k64Draw:
        if (sh::Often()) {
            a[0] = PickOf(0x80, 0xE0, 0x40, sh::Next() & 0xFF);
            a[1] = PickOf(0x40, 0x70, 0x20, sh::Next() & 0x7F);
            a[2] = PickOf(1, 2, 0xC, 0xFF);
            a[3] = PickOf(0, 0xFFFFFF80u, 0xFFFFFFF0u, 0x10);
        }
        break;
    case k65Shade: a[0] = sh::Often() ? sh::Next() % 0x81 : sh::Next(); break;
    default: break;
    }
}

// What the states read again after a call, moved by the group's case of the
// harness's disturbance (from its hash only): the slide +0x30, +8, the
// counter +9, the frame +0xA, the leader's point, the cell words, the y
// dwords +0x34 / +0x38.
void Disturb(U h) {
    const U v = h >> 8;
    unsigned char* const s = Sprite_Current;
    if (!sh::InRegions(s, 0x80)) return;
    switch (h % 8) {
    case 0: SetWord(s + 0x30, (v & 1) ? 0xC0u - ((v >> 1) & 0x10) : v >> 1); break;
    case 1: s[8] = static_cast<unsigned char>((v & 1) ? 0u : v >> 1); break;
    case 2: SetLong(Mem(at::kLeaderX), static_cast<std::int32_t>(v)); break;
    case 3: SetLong(Mem(at::kLeaderZ), static_cast<std::int32_t>(v)); break;
    case 4: SetWord(s + ((v & 1) ? 0x36 : 0x3A), v >> 1); break;
    case 5: s[9] = static_cast<unsigned char>(v); break;
    case 6: s[0xA] = static_cast<unsigned char>(v); break;
    case 7: SetLong(s + ((v & 1) ? 0x34 : 0x38), static_cast<std::int32_t>(v >> 1)); break;
    default: break;
    }
}

}  // namespace

void SelfTest() {
    // BOF3X_E6D_ONLY: the clones whose name contains it (a control's run)
    static sh::Clone chosen[kCount];
    static unsigned index[kCount];
    const char* const only = std::getenv("BOF3X_E6D_ONLY");
    unsigned n = 0;
    for (unsigned k = 0; k < kCount; ++k)
        if (!only || !*only || std::strstr(kAll[k].name, only)) {
            index[n] = k;
            chosen[n++] = kAll[k];
        }
    if (n == 0) bof3::Fatal("effect_6d: BOF3X_E6D_ONLY=%s names no clone", only);
    static unsigned* s_index = index;
    sh::Group g = {"effect_6d", chosen, n, kCallees, sizeof kCallees / sizeof kCallees[0], kTables,
                   sizeof kTables / sizeof kTables[0], kRegions, sizeof kRegions / sizeof kRegions[0],
                   [](unsigned k) { Seed(s_index[k]); }, &Disturb, 4000};
    g.args = [](unsigned k, U* a) { Args(s_index[k], a); };
    g.effect = true;
    g.kinds = kKinds;
    g.n_kinds = sizeof kKinds;
    sh::Run(g);
}

}  // namespace effect_6d

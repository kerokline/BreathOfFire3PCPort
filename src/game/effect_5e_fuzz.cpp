// BOF3X_SHADOW=effect_5e: group E5E's 54 functions through the scenario harness
// in effect mode (scenario_harness.h, docs/scenario_harness.md section 8), once
// at start-up. docs/effect_5e.md section 4. BOF3X_E5E_ONLY=<name>,<name>,...
// runs the clones it names exactly (the controls' speed-up).
//
// The clone table is tools/band_rows.py --group E5E --clones --harness scenario
// (2026-10-03), each extent read again to its last instruction (capstone) and
// the names given; every extent is the tool's but 0x506A80's, which the tool ran
// on into the draw it tail-jumps to (0x506AB0, its own frame and ret, taken
// here as its own clone with the calls the tool gave 0x506A80 past +0x30), and
// the two dispatchers no list had (0x506BD0, 0x507620). Shapes: every
// dispatcher, sub-state and the two argument-less draws kEffect (Sprite_Current
// one of the 20 Effect_Objects records, +5 0x18, a dispatcher's +2 below its
// table's length); the six helpers with arguments kCall.
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_5e.h"
#include "game/effect_5e_callees.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace effect_5e {
namespace {

namespace sh = scenario_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using U = std::uint32_t;

// tools/band_rows.py --group E5E --clones, 2026-10-03 (0x506AB0's: 0x506A80's
// past +0x30, re-based).
constexpr sh::CallSite kCalls506A30[] = {{0x12, 0x57C140}, {0x1E, 0x589840}, {0x2B, 0x506AB0}};
constexpr sh::CallSite kCalls506A60[] = {{0x7, 0x57C140}, {0x1B, 0x506AB0}};
constexpr sh::CallSite kCalls506A80[] = {{0x17, 0x589840}, {0x1C, 0x506AB0}};
constexpr sh::CallSite kCalls506AB0[] = {{0x15, 0x5A77C0}, {0x2B, 0x572FA0}, {0x37, 0x5A75D0}, {0x3F, 0x5A77A0},
                                         {0xDF, 0x5A85F0}, {0xE8, 0x5A9290}, {0xF5, 0x572A00}, {0x10B, 0x572FA0}};
constexpr sh::CallSite kCalls506BF0[] = {{0x14, 0x57C0F0}, {0xA8, 0x587740}, {0xB8, 0x507190}};
constexpr sh::CallSite kCalls506CB0[] = {{0x2D, 0x507190}};
constexpr sh::CallSite kCalls506CF0[] = {{0x2D, 0x507190}};
constexpr sh::CallSite kCalls506D30[] = {{0x21, 0x587740}, {0x31, 0x507190}};
constexpr sh::CallSite kCalls506D70[] = {{0x61, 0x5073D0}, {0x107, 0x5073D0}, {0x121, 0x507190}, {0x14E, 0x587740}};
constexpr sh::CallSite kCalls506ED0[] = {{0x91, 0x5073D0}, {0xAF, 0x507190}};
constexpr sh::CallSite kCalls506FC0[] = {{0x99, 0x5B93D2}, {0xAC, 0x5B93D2}, {0xCE, 0x5B93D2}, {0xE1, 0x5B93D2}, {0x116, 0x507190}};
constexpr sh::CallSite kCalls5070E0[] = {{0x2A, 0x5B93D2}, {0x3D, 0x5B93D2}, {0xA5, 0x589840}, {0xAA, 0x507190}};
constexpr sh::CallSite kCalls507190[] = {{0x3C, 0x5A7B90}, {0x50, 0x5A8200},  {0x5F, 0x5A8060},  {0x73, 0x5A7D70},  {0x7D, 0x5A8DE0},
                                         {0x87, 0x5A8E00}, {0xAF, 0x587740},  {0xDF, 0x5A75D0},  {0xE6, 0x5A77A0},  {0x1A6, 0x5A85F0},
                                         {0x1AC, 0x5A9290}, {0x1CD, 0x5A7A50}, {0x1FC, 0x572A00}, {0x205, 0x461E50}, {0x22B, 0x5A7BC0}};
constexpr sh::CallSite kCalls5073D0[] = {{0x4, 0x5A7B90},   {0x17, 0x5A77C0},  {0x20, 0x461E50},  {0x34, 0x5A8200},  {0x43, 0x5A8060},
                                         {0x52, 0x5A7FF0},  {0x66, 0x5A7D70},  {0x73, 0x5A8DE0},  {0x7D, 0x5A8E00},  {0x11D, 0x5A7610},
                                         {0x125, 0x5A7780}, {0x150, 0x5A84A0}, {0x162, 0x5A9130}, {0x1E1, 0x461E50}, {0x229, 0x5A77C0},
                                         {0x232, 0x461E50}, {0x23A, 0x5A7BC0}};
constexpr sh::CallSite kCalls5076C0[] = {{0xB, 0x507C40}, {0x19, 0x507CB0}};
constexpr sh::CallSite kCalls507700[] = {{0x5, 0x507C40}};
constexpr sh::CallSite kCalls507720[] = {{0x60, 0x507CB0}};
constexpr sh::CallSite kCalls5077B0[] = {{0xC, 0x507CB0}};
constexpr sh::CallSite kCalls5077E0[] = {{0x5A, 0x507CB0}};
constexpr sh::CallSite kCalls507870[] = {{0xE, 0x587740}};
constexpr sh::CallSite kCalls5078A0[] = {{0x60, 0x507CB0}, {0x86, 0x507C40}};
constexpr sh::CallSite kCalls507980[] = {{0xC, 0x507CB0}, {0x2A, 0x587740}};
constexpr sh::CallSite kCalls5079C0[] = {{0x50, 0x5073D0}, {0x61, 0x507CB0}};
constexpr sh::CallSite kCalls507A50[] = {{0xC, 0x507CB0}};
constexpr sh::CallSite kCalls507A80[] = {{0x9, 0x507CB0}, {0x13, 0x587740}, {0x1D, 0x587740}};
constexpr sh::CallSite kCalls507AC0[] = {{0xC, 0x507CB0}, {0x3E, 0x507190}};
constexpr sh::CallSite kCalls507B20[] = {{0xC, 0x507CB0}, {0x48, 0x507190}};
constexpr sh::CallSite kCalls507B90[] = {{0xC, 0x507CB0}};
constexpr sh::CallSite kCalls507BC0[] = {{0x9, 0x5A75B0}, {0x41, 0x461E50}, {0x4B, 0x587740}, {0x5C, 0x507C40}, {0x76, 0x589840}};
constexpr sh::CallSite kCalls507CB0[] = {{0x13, 0x5A77C0}, {0x1C, 0x461E50}, {0x28, 0x5A7610}, {0xB7, 0x461E50}, {0xCB, 0x5A77C0}, {0xD4, 0x461E50}};
constexpr sh::CallSite kCalls507DB0[] = {{0x14C, 0x5080A0}};
constexpr sh::CallSite kCalls507F10[] = {{0xC7, 0x587740}, {0xDA, 0x5080A0}};
constexpr sh::CallSite kCalls508000[] = {{0x2D, 0x5080A0}, {0x38, 0x5080A0}};
constexpr sh::CallSite kCalls508040[] = {{0x24, 0x587740}, {0x45, 0x5080A0}, {0x50, 0x5080A0}};
constexpr sh::CallSite kCalls5080A0[] = {{0x91, 0x5A75D0},  {0x99, 0x5A77A0},  {0x189, 0x5A85F0}, {0x18F, 0x5A9290},
                                         {0x1B3, 0x572A00}, {0x1C8, 0x572FA0}, {0x1F1, 0x5A75D0}, {0x1F9, 0x5A77A0},
                                         {0x2C1, 0x5A85F0}, {0x2C7, 0x5A9290}, {0x2E1, 0x572A00}, {0x2F7, 0x572FA0}};
constexpr sh::CallSite kCalls5083D0[] = {{0x15C, 0x508790}};
constexpr sh::CallSite kCalls508540[] = {{0xC7, 0x587740}, {0xDA, 0x508790}};
constexpr sh::CallSite kCalls508630[] = {{0x2D, 0x508790}, {0x38, 0x508790}};
constexpr sh::CallSite kCalls508730[] = {{0x24, 0x587740}, {0x45, 0x508790}, {0x50, 0x508790}};
constexpr sh::CallSite kCalls508790[] = {{0x9A, 0x5A75D0},  {0xA2, 0x5A77A0},  {0x192, 0x5A85F0}, {0x198, 0x5A9290},
                                         {0x1B5, 0x572A00}, {0x1CA, 0x572FA0}, {0x1F3, 0x5A75D0}, {0x1FB, 0x5A77A0},
                                         {0x2A4, 0x5A85F0}, {0x2AA, 0x5A9290}, {0x2C4, 0x572A00}, {0x2DA, 0x572FA0}};
constexpr sh::CallSite kCalls508AA0[] = {{0x7, 0x57C140}, {0x2C, 0x508BA0}, {0x4D, 0x508BA0}, {0x56, 0x508BA0}};
constexpr sh::CallSite kCalls508B00[] = {{0x7, 0x57C140}, {0x21, 0x587740}, {0x35, 0x508BA0}, {0x3E, 0x508BA0}};
constexpr sh::CallSite kCalls508B50[] = {{0x1E, 0x508BA0}, {0x27, 0x508BA0}};
constexpr sh::CallSite kCalls508B80[] = {{0xB, 0x508BA0}};
constexpr sh::CallSite kCalls508BA0[] = {{0x3A, 0x5A75D0}, {0x42, 0x5A77A0}, {0xC1, 0x5A85F0}, {0xC7, 0x5A9290}, {0xE2, 0x572A00}, {0xFB, 0x572FA0}};

#define E5E_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define E5E_CALLS(a) a, E5E_N(a)
#define E5E_FN(name) reinterpret_cast<const void*>(&::name)
constexpr sh::Shape kEf = sh::Shape::kEffect;
constexpr sh::Shape kCa = sh::Shape::kCall;
// {name, base, size, calls, imms, tables, ours, ret_mask, calm, shape, pointers, state_span, sub_span, kind}
const sh::Clone kAll[] = {
    {"EffectKind18Sub23_Run", 0x506A10, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E5E_FN(EffectKind18Sub23_Run), 0, false, kEf, 0, 0, 3, 0x18},
    {"EffectKind18Sub23_Start", 0x506A30, 0x30, E5E_CALLS(kCalls506A30), nullptr, 0, nullptr, 0, E5E_FN(EffectKind18Sub23_Start), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub23_WaitFlag", 0x506A60, 0x20, E5E_CALLS(kCalls506A60), nullptr, 0, nullptr, 0, E5E_FN(EffectKind18Sub23_WaitFlag), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub23_Slide", 0x506A80, 0x21, E5E_CALLS(kCalls506A80), nullptr, 0, nullptr, 0, E5E_FN(EffectKind18Sub23_Slide), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub23_Draw", 0x506AB0, 0x118, E5E_CALLS(kCalls506AB0), nullptr, 0, nullptr, 0, E5E_FN(EffectKind18Sub23_Draw), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub24_Run", 0x506BD0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E5E_FN(EffectKind18Sub24_Run), 0, false, kEf, 0, 0, 8, 0x18},
    {"EffectKind18Sub24_Start", 0x506BF0, 0xBE, E5E_CALLS(kCalls506BF0), nullptr, 0, nullptr, 0, E5E_FN(EffectKind18Sub24_Start), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub24_Beat1", 0x506CB0, 0x32, E5E_CALLS(kCalls506CB0), nullptr, 0, nullptr, 0, E5E_FN(EffectKind18Sub24_Beat1), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub24_Beat2", 0x506CF0, 0x32, E5E_CALLS(kCalls506CF0), nullptr, 0, nullptr, 0, E5E_FN(EffectKind18Sub24_Beat2), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub24_Beat3", 0x506D30, 0x36, E5E_CALLS(kCalls506D30), nullptr, 0, nullptr, 0, E5E_FN(EffectKind18Sub24_Beat3), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub24_Trails", 0x506D70, 0x155, E5E_CALLS(kCalls506D70), nullptr, 0, nullptr, 0, E5E_FN(EffectKind18Sub24_Trails), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub24_TrailsOut", 0x506ED0, 0xEA, E5E_CALLS(kCalls506ED0), nullptr, 0, nullptr, 0, E5E_FN(EffectKind18Sub24_TrailsOut), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub24_Spin", 0x506FC0, 0x11B, E5E_CALLS(kCalls506FC0), nullptr, 0, nullptr, 0, E5E_FN(EffectKind18Sub24_Spin), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub24_Fade", 0x5070E0, 0xAF, E5E_CALLS(kCalls5070E0), nullptr, 0, nullptr, 0, E5E_FN(EffectKind18Sub24_Fade), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub24_Draw", 0x507190, 0x238, E5E_CALLS(kCalls507190), nullptr, 0, nullptr, 0, E5E_FN(EffectKind18Sub24_Draw), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub24_DrawTrail", 0x5073D0, 0x244, E5E_CALLS(kCalls5073D0), nullptr, 0, nullptr, 0, E5E_FN(EffectKind18Sub24_DrawTrail), 0, false, kCa, 0, 0, 0, 0x18},
    {"EffectKind18Sub3F_Run", 0x507620, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E5E_FN(EffectKind18Sub3F_Run), 0, false, kEf, 0, 0, 16, 0x18},
    {"EffectKind18Sub3F_Start", 0x507640, 0x80, nullptr, 0, nullptr, 0, nullptr, 0, E5E_FN(EffectKind18Sub3F_Start), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub3F_WaitDim", 0x5076C0, 0x33, E5E_CALLS(kCalls5076C0), nullptr, 0, nullptr, 0, E5E_FN(EffectKind18Sub3F_WaitDim), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub3F_Undim", 0x507700, 0x1D, E5E_CALLS(kCalls507700), nullptr, 0, nullptr, 0, E5E_FN(EffectKind18Sub3F_Undim), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub3F_SkyWarm", 0x507720, 0x8E, E5E_CALLS(kCalls507720), nullptr, 0, nullptr, 0, E5E_FN(EffectKind18Sub3F_SkyWarm), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub3F_WaitCue29", 0x5077B0, 0x26, E5E_CALLS(kCalls5077B0), nullptr, 0, nullptr, 0, E5E_FN(EffectKind18Sub3F_WaitCue29), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub3F_SkyCool", 0x5077E0, 0x8B, E5E_CALLS(kCalls5077E0), nullptr, 0, nullptr, 0, E5E_FN(EffectKind18Sub3F_SkyCool), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub3F_WaitCue32", 0x507870, 0x28, E5E_CALLS(kCalls507870), nullptr, 0, nullptr, 0, E5E_FN(EffectKind18Sub3F_WaitCue32), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub3F_SkyDim", 0x5078A0, 0xE0, E5E_CALLS(kCalls5078A0), nullptr, 0, nullptr, 0, E5E_FN(EffectKind18Sub3F_SkyDim), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub3F_WaitCue35", 0x507980, 0x31, E5E_CALLS(kCalls507980), nullptr, 0, nullptr, 0, E5E_FN(EffectKind18Sub3F_WaitCue35), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub3F_Trail", 0x5079C0, 0x85, E5E_CALLS(kCalls5079C0), nullptr, 0, nullptr, 0, E5E_FN(EffectKind18Sub3F_Trail), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub3F_WaitCue37", 0x507A50, 0x26, E5E_CALLS(kCalls507A50), nullptr, 0, nullptr, 0, E5E_FN(EffectKind18Sub3F_WaitCue37), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub3F_Flash", 0x507A80, 0x37, E5E_CALLS(kCalls507A80), nullptr, 0, nullptr, 0, E5E_FN(EffectKind18Sub3F_Flash), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub3F_Spin", 0x507AC0, 0x55, E5E_CALLS(kCalls507AC0), nullptr, 0, nullptr, 0, E5E_FN(EffectKind18Sub3F_Spin), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub3F_SpinFade", 0x507B20, 0x69, E5E_CALLS(kCalls507B20), nullptr, 0, nullptr, 0, E5E_FN(EffectKind18Sub3F_SpinFade), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub3F_WaitCue3B", 0x507B90, 0x26, E5E_CALLS(kCalls507B90), nullptr, 0, nullptr, 0, E5E_FN(EffectKind18Sub3F_WaitCue3B), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub3F_WhiteOut", 0x507BC0, 0x7E, E5E_CALLS(kCalls507BC0), nullptr, 0, nullptr, 0, E5E_FN(EffectKind18Sub3F_WhiteOut), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub3F_ShadeClut", 0x507C40, 0x6A, nullptr, 0, nullptr, 0, nullptr, 0, E5E_FN(EffectKind18Sub3F_ShadeClut), 0, false, kCa, 0, 0, 0, 0x18},
    {"EffectKind18Sub3F_DrawSky", 0x507CB0, 0xDF, E5E_CALLS(kCalls507CB0), nullptr, 0, nullptr, 0, E5E_FN(EffectKind18Sub3F_DrawSky), 0, false, kCa, 0, 0, 0, 0x18},
    {"EffectKind18Sub25_Run", 0x507D90, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E5E_FN(EffectKind18Sub25_Run), 0, false, kEf, 0, 0, 5, 0x18},
    {"EffectKind18Sub25_Start", 0x507DB0, 0x156, E5E_CALLS(kCalls507DB0), nullptr, 0, nullptr, 0, E5E_FN(EffectKind18Sub25_Start), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub25_WaitNear", 0x507F10, 0xE4, E5E_CALLS(kCalls507F10), nullptr, 0, nullptr, 0, E5E_FN(EffectKind18Sub25_WaitNear), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub25_Open", 0x508000, 0x3F, E5E_CALLS(kCalls508000), nullptr, 0, nullptr, 0, E5E_FN(EffectKind18Sub25_Open), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub25_WaitAway", 0x508670, 0xBD, nullptr, 0, nullptr, 0, nullptr, 0, E5E_FN(EffectKind18Sub25_WaitAway), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub25_Close", 0x508040, 0x57, E5E_CALLS(kCalls508040), nullptr, 0, nullptr, 0, E5E_FN(EffectKind18Sub25_Close), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub25_Draw", 0x5080A0, 0x305, E5E_CALLS(kCalls5080A0), nullptr, 0, nullptr, 0, E5E_FN(EffectKind18Sub25_Draw), 0, false, kCa, 0, 0, 0, 0x18},
    {"EffectKind18Sub26_Run", 0x5083B0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E5E_FN(EffectKind18Sub26_Run), 0, false, kEf, 0, 0, 5, 0x18},
    {"EffectKind18Sub26_Start", 0x5083D0, 0x166, E5E_CALLS(kCalls5083D0), nullptr, 0, nullptr, 0, E5E_FN(EffectKind18Sub26_Start), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub26_WaitNear", 0x508540, 0xE4, E5E_CALLS(kCalls508540), nullptr, 0, nullptr, 0, E5E_FN(EffectKind18Sub26_WaitNear), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub26_Open", 0x508630, 0x3F, E5E_CALLS(kCalls508630), nullptr, 0, nullptr, 0, E5E_FN(EffectKind18Sub26_Open), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub26_Close", 0x508730, 0x57, E5E_CALLS(kCalls508730), nullptr, 0, nullptr, 0, E5E_FN(EffectKind18Sub26_Close), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub26_Draw", 0x508790, 0x2E8, E5E_CALLS(kCalls508790), nullptr, 0, nullptr, 0, E5E_FN(EffectKind18Sub26_Draw), 0, false, kCa, 0, 0, 0, 0x18},
    {"EffectKind18Sub39_Run", 0x508A80, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E5E_FN(EffectKind18Sub39_Run), 0, false, kEf, 0, 0, 4, 0x18},
    {"EffectKind18Sub39_Start", 0x508AA0, 0x5F, E5E_CALLS(kCalls508AA0), nullptr, 0, nullptr, 0, E5E_FN(EffectKind18Sub39_Start), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub39_WaitFlag", 0x508B00, 0x47, E5E_CALLS(kCalls508B00), nullptr, 0, nullptr, 0, E5E_FN(EffectKind18Sub39_WaitFlag), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub39_Open", 0x508B50, 0x30, E5E_CALLS(kCalls508B50), nullptr, 0, nullptr, 0, E5E_FN(EffectKind18Sub39_Open), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub39_Hold", 0x508B80, 0x14, E5E_CALLS(kCalls508B80), nullptr, 0, nullptr, 0, E5E_FN(EffectKind18Sub39_Hold), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub39_Draw", 0x508BA0, 0x11C, E5E_CALLS(kCalls508BA0), nullptr, 0, nullptr, 0, E5E_FN(EffectKind18Sub39_Draw), 0, false, kCa, 0, 0, 0, 0x18},
};
#undef E5E_FN
#undef E5E_CALLS
#undef E5E_N

enum : unsigned {
    k23Run, k23Start, k23WaitFlag, k23Slide, k23Draw,
    k24Run, k24Start, k24Beat1, k24Beat2, k24Beat3, k24Trails, k24TrailsOut, k24Spin, k24Fade, k24Draw, k24Trail,
    k3FRun, k3FStart, k3FWaitDim, k3FUndim, k3FSkyWarm, k3FCue29, k3FSkyCool, k3FCue32, k3FSkyDim, k3FCue35, k3FTrail,
    k3FCue37, k3FFlash, k3FSpin, k3FSpinFade, k3FCue3B, k3FWhiteOut, k3FShadeClut, k3FDrawSky,
    k25Run, k25Start, k25WaitNear, k25Open, k25WaitAway, k25Close, k25Draw,
    k26Run, k26Start, k26WaitNear, k26Open, k26Close, k26Draw,
    k39Run, k39Start, k39WaitFlag, k39Open, k39Hold, k39Draw, kCount
};
static_assert(kCount == sizeof kAll / sizeof kAll[0], "one enum entry a clone, in order");

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
unsigned char* Mem(U a) { return sh::Mem(a); }
template <typename... T> U PickOf(T... v) {
    const U values[] = {static_cast<U>(v)...};
    return sh::Pick(values, sizeof...(v));
}

// The draws with arguments read Sprite_Current (the record's point, level,
// cell, lid): its address logged.
U FxCurrent(const U*, U answer) {
    sh::Note(Key(Sprite_Current));
    return answer;
}

#define E5E_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
constexpr sh::Answer kG = sh::Answer::kGarbage, kPh = sh::Answer::kPhase;
constexpr U kW = 0xFFFFFFFFu;
const sh::Callee kCallees[] = {
    // the group's own called directly (or tail-jumped to), by name
    {E5E_OURS(EffectKind18Sub23_Draw), 0, {}, kPh, 0, 0},
    {E5E_OURS(EffectKind18Sub24_Draw), 0, {}, kPh, 0, 0},
    // four whole words: the callers push constants and registers they cleared
    // first (xor ecx, ecx / xor eax, eax before the byte +9 is loaded)
    {E5E_OURS(EffectKind18Sub24_DrawTrail), 4, {kW, kW, kW, kW}, kG, 0, 0},
    // the level, a whole word (imul of the full register); the colours, a word
    // read to bit 30
    {E5E_OURS(EffectKind18Sub3F_ShadeClut), 1, {kW}, kG, 0, 0},
    {E5E_OURS(EffectKind18Sub3F_DrawSky), 1, {kW}, kG, 0, 0},
    // dy handed on whole to MapView_LinkPrimAt; the variant a whole word
    {E5E_OURS(EffectKind18Sub25_Draw), 1, {kW}, kG, 0, 0, {}, &FxCurrent},
    {E5E_OURS(EffectKind18Sub26_Draw), 1, {kW}, kG, 0, 0, {}, &FxCurrent},
    {E5E_OURS(EffectKind18Sub39_Draw), 2, {kW, kW}, kG, 0, 0, {}, &FxCurrent},
};
#undef E5E_OURS

// The sub-state tables the dispatchers jump through, read in place; each
// table's own length (symbols.toml [[data]], none bounded by a compare).
const sh::DataTable kTables[] = {
    {0x65E698, 3}, {0x65E6AC, 8}, {0x65E710, 16}, {0x65E750, 5}, {0x65E7B0, 5}, {0x65E810, 4},
};
const std::uint8_t kKinds[] = {0x18};

// Beyond effect mode's standard regions: CLUT row 4 as loaded and as live.
const sh::Region kRegions[] = {
    {at::kClutRow4, 2 * at::kClutRowWords},
    {at::kClutRow4Live, 2 * at::kClutRowWords},
};

// --- the seed ------------------------------------------------------------------------

unsigned char* Rec(unsigned r) { return sh::EffectRecord(r); }
unsigned g_k = 0;   // the clone this round runs (Disturb keeps a lid index for the draws)

bool LidDraw(unsigned k) { return k == k25Draw || k == k26Draw; }

U Level() { return PickOf(0, 2, 4, 6, 0x10, 0x40, 0x41, 0x3F, 0x70, 0x78, 0x7F, 0x80, 0xFC, 0x100, 0xFFF0, 0x8000, sh::Next()); }
// The speed +0x14 at its tests' boundaries - before the spin's step (q - 10) q
// is added, and so that the step lands on them: 0xA5155, 0xF7079 and 0x14A5A5
// step to 0xA6040, 0xF9060 and 0x14E790 exactly.
U Speed() {
    return PickOf(0xA6040, 0xA603F, 0xF9060, 0xF9061, 0xF905F, 0x14E790, 0x14E78F, 0x53020, 0, 0xFFFFFFFFu, 0x80000000u,
                  0xA5155, 0xA5154, 0xA5156, 0xF7079, 0xF7078, 0xF707A, 0x14A5A5, 0x14A5A4, sh::Next());
}
U Cue() { return PickOf(5, 6, 7, 0x26, 0x29, 0x32, 0x35, 0x37, 0x3A, 0x3B, 4, 0x25, sh::Next()); }
U Count9() { return PickOf(0, 1, 2, 3, 7, 8, 0xE, 0xF, 0x10, 0x11, 0x59, 0x5A, 0x5B, 0x77, 0x78, 0x79, 0xFF, sh::Next()); }
// The leader's offset from the cell's edges, at the tests' boundaries.
U Near() {
    return PickOf(0, 1, 0x7FFF, 0x8000, 0x8001, 0xFFFF, 0x10000, 0x10001, 0x1FFFF, 0x20000, 0x20001, 0x30000, 0x30001,
                  0xFFFF8000u, 0xFFFF7FFFu, 0xFFFF0000u, 0xFFFEFFFFu, 0xFFFE0000u, 0xFFFDFFFFu, 0x80000000u, sh::Next());
}

void Seed(unsigned k) {
    g_k = k;
    for (unsigned r = 0; r < 20; ++r) {
        unsigned char* const e = Rec(r);
        e[9] = static_cast<unsigned char>(Count9());
        e[8] = static_cast<unsigned char>(PickOf(0, 1, 0, 1, 0x80, sh::Next()));
        e[0xB] = static_cast<unsigned char>(PickOf(0, 0, 1, 2, sh::Next()));
        SetWord(e + 0x30, Level());
        // the angle (sub-kinds 0x24, 0x3F) or the lid (0x25, 0x26: 0 is none)
        const U angle = PickOf(0, 0, 1, 2, sh::Next(), sh::Next());
        SetWord(e + 0x32, LidDraw(k) ? sh::Next() % 3 : angle);
        // +0x2E the angle's last value: bit 11 the same or not
        SetWord(e + 0x2E, sh::Half() ? (Word(e + 0x32) ^ 0x800u) : (Word(e + 0x32) ^ (sh::Next() & 0x7FFu)));
        SetLong(e + 0x14, static_cast<std::int32_t>(Speed()));
        // the variant +0x36: inside its tables for the starts (ours aborts
        // past), else a cell
        if (k == k25Start)
            SetWord(e + 0x36, sh::Next() % at::kSub25Variants);
        else if (k == k26Start)
            SetWord(e + 0x36, sh::Next() % at::kSub26Variants);
        else
            SetWord(e + 0x36, PickOf(0, 1, 0x7F, 0xFFFF, 0x8000, sh::Next()));
        SetWord(e + 0x3A, PickOf(0, 0, 1, 0x40, 0xFFFF, sh::Next()));
    }
    // the leader at the boundaries of the current record's cell - for the
    // starts the cell their variant picks (read in place) and +8 as they set it
    unsigned char* const s = Sprite_Current;
    if (sh::InRegions(s, 0x80)) {
        bool along = s[8] != 0;
        U x = Word(s + 0x36), z = Word(s + 0x3A);
        if (k == k25Start || k == k26Start) {
            const U cells = k == k25Start ? at::kSub25Cells : at::kSub26Cells;
            along = Word(s + 0x3A) == 0;
            x = move_script::At(cells + 2 * Word(s + 0x36))[0];
            z = move_script::At(cells + 2 * Word(s + 0x36) + 1)[0];
        }
        const U cell = static_cast<U>(static_cast<std::int16_t>(along ? x : z)) << 16;
        const U row = ((static_cast<U>(static_cast<std::int16_t>(along ? z : x)) + 1u) << 16) | 0x8000u;
        const U a = row + Near(), b = cell + PickOf(0, 0x10000, 0x20000) + Near();
        SetLong(Mem(along ? at::kLeaderZ : at::kLeaderX), static_cast<std::int32_t>(a));
        SetLong(Mem(along ? at::kLeaderX : at::kLeaderZ), static_cast<std::int32_t>(b));
    }
    Mem(at::kCounter)[0] = static_cast<unsigned char>(Cue());
    Field_Request = static_cast<unsigned char>(PickOf(0, 0, 1, 2, sh::Next()));
    if (k == k3FShadeClut)
        for (U i = 0; i < 2 * at::kClutRowWords; i += 4) SetLong(Mem(at::kClutRow4 + i), static_cast<std::int32_t>(sh::Next()));
}

// The helpers' arguments: a trail inside the polyline's 34 pairs (from 0..32,
// to 0..33 - ours aborts past them; from at or above to draws nothing), the
// levels and colours at their callers' values and wider, dy as the callers
// push it, the variant 0..2.
void Args(unsigned k, U* a) {
    switch (k) {
    case k24Trail:
        a[0] = PickOf(0, 4, 8, 0x1C, 0x1E, 0x1F, 0x20, sh::Next() % 33, sh::Next() % 33, 0xFFFFFFF0u);
        a[1] = PickOf(0x1F, 0x1E, 0x21, 0x20, 0, a[0], a[0] + 1, sh::Next() % 34, sh::Next() % 34);
        if (static_cast<std::int32_t>(a[0]) < 0) a[1] = 0xFFFFFFE0u;   // from below to: nothing drawn
        a[2] = PickOf(0xE00, 0x1A00, 0, 0xC00, sh::Next());
        a[3] = PickOf(0x18, 0x10, sh::Next());
        break;
    case k3FShadeClut: a[0] = PickOf(0x40, 0x80, 0x7F, 0x55, 0x6A, 0, 1, 0x100, 0xFFFFFFC0u, sh::Next()); break;
    case k3FDrawSky: a[0] = PickOf(0xFFFFFFFFu, 0x10001805, 0x10001234, sh::Next()); break;
    case k25Draw:
    case k26Draw: a[0] = PickOf(0, 0xFFFFFFFFu, sh::Next()); break;
    case k39Draw:
        a[0] = PickOf(0, 1, 2);
        a[1] = PickOf(0xFFFFFFFEu, 0xFFFFFFFFu, 0, 1, sh::Next());
        break;
    default: break;
    }
}

// What the states read again after a call, moved by the group's case of the
// harness's disturbance (from its hash only): +9, the level +0x30, the speed
// +0x14, the counter at its cues, the sound byte +0xB, the angle +0x32 (a lid
// index 0..2 while a lid draw runs: ours aborts past the textures), +0x2E,
// the leader's point, +8.
void Disturb(U h) {
    const U v = h >> 8;
    unsigned char* const s = Sprite_Current;
    if (!sh::InRegions(s, 0x80)) return;
    switch (h % 9) {
    case 0: {
        static const unsigned char kNine[] = {0, 2, 3, 7, 8, 0xF, 0x10, 0x11, 0x5A, 0x5B, 0x78, 0x79};
        s[9] = static_cast<unsigned char>((v & 1) ? kNine[(v >> 1) % sizeof kNine] : v >> 1);
        break;
    }
    case 1: SetWord(s + 0x30, (v & 1) ? ((v & 2) ? 0x40u : 0x80u) + ((v >> 2) & 3) - 1 : v >> 1); break;
    case 2: SetLong(s + 0x14, static_cast<std::int32_t>((v & 1) ? 0xF9060u + ((v >> 1) & 1) : v)); break;
    case 3: Mem(at::kCounter)[0] = static_cast<unsigned char>((v & 1) ? 0x26u + ((v >> 1) % 0x16) : v >> 1); break;
    case 4: s[0xB] = static_cast<unsigned char>(v % 3); break;
    case 5: SetWord(s + 0x32, LidDraw(g_k) ? v % 3 : v); break;
    case 6: SetWord(s + 0x2E, Word(s + 0x2E) ^ 0x800u); break;
    case 7: SetLong(Mem((v & 1) ? at::kLeaderX : at::kLeaderZ), static_cast<std::int32_t>(Long(Mem((v & 1) ? at::kLeaderX : at::kLeaderZ)) + static_cast<std::int32_t>((v >> 1) & 0x3FFFF) - 0x20000)); break;
    case 8: s[8] = static_cast<unsigned char>(v & 1); break;
    default: break;
    }
}

}  // namespace

void SelfTest() {
    // BOF3X_E5E_ONLY: the clones it names, exactly, separated by commas (a
    // control's run; several controls share a build)
    static sh::Clone chosen[kCount];
    static unsigned index[kCount];
    const char* const only = std::getenv("BOF3X_E5E_ONLY");
    const auto named = [only](const char* name) {
        const std::size_t length = std::strlen(name);
        for (const char* p = only; (p = std::strstr(p, name)) != nullptr; p += length)
            if ((p == only || p[-1] == ',') && (p[length] == ',' || p[length] == 0)) return true;
        return false;
    };
    unsigned n = 0;
    for (unsigned k = 0; k < kCount; ++k)
        if (!only || !*only || named(kAll[k].name)) {
            index[n] = k;
            chosen[n++] = kAll[k];
        }
    if (n == 0) bof3::Fatal("effect_5e: BOF3X_E5E_ONLY=%s names no clone", only);
    static unsigned* s_index = index;
    sh::Group g = {"effect_5e", chosen, n, kCallees, sizeof kCallees / sizeof kCallees[0], kTables,
                   sizeof kTables / sizeof kTables[0], kRegions, sizeof kRegions / sizeof kRegions[0],
                   [](unsigned k) { Seed(s_index[k]); }, &Disturb, 4000};
    g.args = [](unsigned k, U* a) { Args(s_index[k], a); };
    g.effect = true;
    g.kinds = kKinds;
    g.n_kinds = sizeof kKinds;
    sh::Run(g);
}

}  // namespace effect_5e

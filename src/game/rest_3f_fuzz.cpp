// BOF3X_SHADOW=rest_3f: group R3F's 50 functions through the scenario harness
// in effect mode (scenario_harness.h, docs/scenario_harness.md section 8), once
// at start-up. docs/rest_3f.md section 4. BOF3X_R3F_ONLY=<name> runs the clones
// whose name contains it, BOF3X_R3F_ROUNDS the rounds (the controls' speed-up).
//
// The clone table is tools/band_rows.py --group R3F --clones --harness scenario
// (2026-10-04, through the round's scratch wrapper band14.py: the tool stops at
// its fixpoint limit on this cut), each extent read again to its last
// instruction (capstone) and the names given; every extent is the tool's.
// Shapes: the states kEffect (Sprite_Current one of the 20 Effect_Objects
// records, +5 the kind's); the draws with arguments kCall; two answer in al
// (ret_mask 0xFF).
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/rest_3f.h"
#include "game/rest_3f_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace rest_3f {
namespace {

namespace sh = scenario_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using U = std::uint32_t;

// tools/band_rows.py --group R3F --clones, 2026-10-04.
constexpr sh::CallSite kCalls480270[] = {{0x2C, 0x4804C0}};
constexpr sh::CallSite kCalls4802C0[] = {{0x1E, 0x4804C0}};
constexpr sh::CallSite kCalls480300[] = {{0x16, 0x5A79A0}, {0x2E, 0x5A77C0}, {0x45, 0x572FA0}, {0x4A, 0x494060}, {0x55, 0x494110},
                                         {0x74, 0x4941E0}, {0x7C, 0x5B93D2}, {0xC0, 0x5A7A90}, {0xD0, 0x5A7650}, {0xD8, 0x5A7780},
                                         {0x104, 0x5A7A00}, {0x127, 0x5A7A00}, {0x183, 0x572FA0}, {0x18B, 0x5B93D2}};
constexpr sh::CallSite kCalls4837B0[] = {{0x31, 0x5B93D2}, {0x4D, 0x5A7A00}, {0x92, 0x5A77C0}, {0x9B, 0x461E50}, {0xA7, 0x5A76B0},
                                         {0xAF, 0x5A7780}, {0xC7, 0x5A8250}, {0xD0, 0x5A9110}, {0xD8, 0x5B93D2}, {0xE1, 0x5B93D2},
                                         {0xF5, 0x5B93D2}, {0x12A, 0x5A7A00}, {0x160, 0x5A8250}, {0x169, 0x5A9110}, {0x182, 0x5B93D2},
                                         {0x1A2, 0x461E50}};
constexpr sh::CallSite kCalls48ED80[] = {{0x17, 0x5A79A0}, {0x2E, 0x5A77C0}, {0x37, 0x461E50}, {0x43, 0x5A7610}, {0x4B, 0x5A7780},
                                         {0x5A, 0x494110}, {0x9F, 0x494110}, {0x109, 0x461E50}, {0x118, 0x5A7610}, {0x120, 0x5A7780},
                                         {0x12B, 0x494110}, {0x16C, 0x494110}, {0x1D3, 0x461E50}, {0x259, 0x494110}, {0x265, 0x5A7750},
                                         {0x26D, 0x5A7780}, {0x29E, 0x461E50}};
constexpr sh::CallSite kCalls48F090[] = {{0xD1, 0x587740}};
constexpr sh::CallSite kCalls48F170[] = {{0x30, 0x494060}, {0x42, 0x48F3D0}};
constexpr sh::CallSite kCalls48F1E0[] = {{0x0, 0x494060}, {0x12, 0x48F3D0}, {0x24, 0x48F5D0}};
constexpr sh::CallSite kCalls48F240[] = {{0x25, 0x494060}, {0x40, 0x48FA80}, {0x70, 0x48F720}, {0xA3, 0x48F8F0}};
constexpr sh::CallSite kCalls48F300[] = {{0x0, 0x494060}, {0x12, 0x48F5D0}, {0x4D, 0x587740}};
constexpr sh::CallSite kCalls48F360[] = {{0x30, 0x494060}, {0x42, 0x48F3D0}};
constexpr sh::CallSite kCalls48F3D0[] = {{0x3A, 0x494110}, {0x8B, 0x494110}, {0xDA, 0x494110}, {0x10B, 0x494110}, {0x117, 0x5A7690},
                                         {0x11F, 0x5A7780}, {0x18E, 0x572FA0}, {0x19A, 0x5A7650}, {0x1A2, 0x5A7780}, {0x1E5, 0x572FA0}};
constexpr sh::CallSite kCalls490A80[] = {{0x1, 0x589810}, {0x49, 0x461E10}, {0xAE, 0x5A79A0}, {0xE4, 0x589200},
                                         {0xFC, 0x589200}, {0x104, 0x588F20}, {0x109, 0x589840}};
constexpr sh::CallSite kCalls490BB0[] = {{0x56, bof3::addr::Gfx_StoreImage}};
constexpr sh::CallSite kCalls490C50[] = {{0x6, 0x494060}, {0x38, 0x494110}};
constexpr sh::CallSite kCalls490E20[] = {{0x3F, 0x5B93D2}, {0x4A, 0x5B93D2}, {0xA2, 0x5B93D2}, {0xAD, 0x5B93D2}, {0xBB, 0x5B93D2},
                                         {0xC4, 0x5B93D2}, {0xFB, 0x5B93D2}, {0x137, 0x5B93D2}, {0x172, 0x587740}};
constexpr sh::CallSite kCalls490FA0[] = {{0xA6, 0x5A7750}, {0xE4, 0x461E50}};
constexpr sh::CallSite kCalls4910F0[] = {{0x3C, 0x587740}};
constexpr sh::CallSite kCalls491140[] = {{0x53, 0x4918B0}, {0x74, 0x589840}};
constexpr sh::CallSite kCalls4911C0[] = {{0xA3, 0x587740}};
constexpr sh::CallSite kCalls491270[] = {{0x11, 0x4918B0}, {0x74, 0x589840}};
constexpr sh::CallSite kCalls491310[] = {{0xE, 0x589810}, {0x56, 0x461E10}, {0xB1, 0x5A79A0}, {0xD3, 0x5891F0}, {0xDB, 0x588F20}, {0xE0, 0x589840}};
constexpr sh::CallSite kCalls491410[] = {{0x56, bof3::addr::Gfx_StoreImage}};
constexpr sh::CallSite kCalls4914B0[] = {{0x6, 0x494060}, {0x38, 0x494110}};
constexpr sh::CallSite kCalls491680[] = {{0x3F, 0x5B93D2}, {0x4A, 0x5B93D2}, {0x95, 0x5B93D2}, {0x9E, 0x5B93D2}, {0xC9, 0x5B93D2}, {0x130, 0x587740}};
constexpr sh::CallSite kCalls4917C0[] = {{0x64, 0x5A7750}, {0xA2, 0x461E50}};
constexpr sh::CallSite kCalls4918B0[] = {{0x17, 0x5A79A0}, {0x2D, 0x5A77C0}, {0x36, 0x461E50}, {0x3B, 0x494060}, {0x6C, 0x494110},
                                         {0x8B, 0x494110}, {0xB4, 0x5A7610}, {0xBC, 0x5A7780}, {0xF2, 0x5A7A50}, {0x109, 0x5A7A00},
                                         {0x129, 0x494110}, {0x12F, 0x5A7A50}, {0x146, 0x5A7A00}, {0x166, 0x494110}, {0x1C1, 0x461E50}};
constexpr sh::CallSite kCalls491B70[] = {{0x27, 0x589840}};
constexpr sh::CallSite kCalls491BC0[] = {{0x15, 0x491FF0}};
constexpr sh::CallSite kCalls491BF0[] = {{0xD, 0x492010}, {0x12, 0x5B93D2}, {0x2D, 0x492030}};
constexpr sh::CallSite kCalls491C40[] = {{0x5, 0x492030}, {0x11, 0x589840}};
constexpr sh::CallSite kCalls491D30[] = {{0x2D, 0x589840}};
constexpr sh::CallSite kCalls491DB0[] = {{0xD, 0x492400}};
constexpr sh::CallSite kCalls491DF0[] = {{0x17, 0x492400}, {0x39, 0x589840}};
constexpr sh::CallSite kCalls491E30[] = {{0x17, 0x5A79A0}, {0x2F, 0x5A77C0}, {0x38, 0x461E50}, {0x3D, 0x494060}, {0x4C, 0x494110}, {0x6B, 0x4941E0},
                                         {0xD9, 0x5A75F0}, {0xE1, 0x5A7780}, {0x10B, 0x5A7A50}, {0x12C, 0x5A7A00}, {0x198, 0x461E50}};
constexpr sh::CallSite kCalls492030[] = {{0x16, 0x5A79A0}, {0x2E, 0x5A77C0}, {0x37, 0x461E50}, {0x3F, 0x494060}, {0xA2, 0x4920F0}};
constexpr sh::CallSite kCalls4920F0[] = {{0x17, 0x5A7610}, {0x20, 0x5A7780}, {0xBC, 0x461E50}, {0xC8, 0x5A7610}, {0xCF, 0x5A7780}, {0x162, 0x461E50}};
constexpr sh::CallSite kCalls492260[] = {{0x17, 0x5A79A0}, {0x2E, 0x5A77C0}, {0x37, 0x461E50}, {0x55, 0x5A7A50}, {0x68, 0x5A7A00},
                                         {0xC2, 0x5A75F0}, {0xCA, 0x5A7780}, {0xF9, 0x5A7A50}, {0x118, 0x5A7A00}, {0x17E, 0x461E50}};
constexpr sh::CallSite kCalls492400[] = {{0x14, 0x5A79A0}, {0x2B, 0x5A77C0}, {0x34, 0x461E50}, {0x40, 0x5A7610}, {0x47, 0x5A7780},
                                         {0x9A, 0x461E50}, {0xA6, 0x5A7610}, {0xAD, 0x5A7780}, {0xFB, 0x461E50}};
constexpr sh::CallSite kCalls492580[] = {{0x12, 0x492CF0}, {0x17, 0x492AF0}};

#define R3F_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define R3F_CALLS(a) a, R3F_N(a)
#define R3F_FN(name) reinterpret_cast<const void*>(&::name)
constexpr sh::Shape kEf = sh::Shape::kEffect;
constexpr sh::Shape kCa = sh::Shape::kCall;
// {name, base, size, calls, imms, tables, ours, ret_mask, calm, shape, pointers, state_span, sub_span, kind}
const sh::Clone kAll[] = {
    {"EffectKind60_Start", 0x480210, 0x53, nullptr, 0, nullptr, 0, nullptr, 0, R3F_FN(EffectKind60_Start), 0, false, kEf, 0, 0, 0, 0x60},
    {"EffectKind60_SlideBoth", 0x480270, 0x46, R3F_CALLS(kCalls480270), nullptr, 0, nullptr, 0, R3F_FN(EffectKind60_SlideBoth), 0, false, kEf, 0, 0, 0, 0x60},
    {"EffectKind60_SlideEnd", 0x4802C0, 0x38, R3F_CALLS(kCalls4802C0), nullptr, 0, nullptr, 0, R3F_FN(EffectKind60_SlideEnd), 0, false, kEf, 0, 0, 0, 0x60},
    {"EffectKind5F_DrawLineDisc", 0x480300, 0x1B7, R3F_CALLS(kCalls480300), nullptr, 0, nullptr, 0, R3F_FN(EffectKind5F_DrawLineDisc), 0, false, kCa, 0, 0, 0, 0x5F},
    {"EffectKind69_DrawLines", 0x4837B0, 0x1C0, R3F_CALLS(kCalls4837B0), nullptr, 0, nullptr, 0, R3F_FN(EffectKind69_DrawLines), 0, false, kEf, 0, 0, 0, 0x69},
    {"EffectKind9C_DrawTrail", 0x48ED80, 0x2E5, R3F_CALLS(kCalls48ED80), nullptr, 0, nullptr, 0, R3F_FN(EffectKind9C_DrawTrail), 0, false, kCa, 0, 0, 0, 0x9C},
    {"EffectKind9E_Start", 0x48F090, 0xD8, R3F_CALLS(kCalls48F090), nullptr, 0, nullptr, 0, R3F_FN(EffectKind9E_Start), 0, false, kEf, 0, 0, 0, 0x9E},
    {"EffectKind9E_Grow", 0x48F170, 0x67, R3F_CALLS(kCalls48F170), nullptr, 0, nullptr, 0, R3F_FN(EffectKind9E_Grow), 0, false, kEf, 0, 0, 0, 0x9E},
    {"EffectKind9E_Flash", 0x48F1E0, 0x5F, R3F_CALLS(kCalls48F1E0), nullptr, 0, nullptr, 0, R3F_FN(EffectKind9E_Flash), 0, false, kEf, 0, 0, 0, 0x9E},
    {"EffectKind9E_Hold", 0x48F240, 0xBF, R3F_CALLS(kCalls48F240), nullptr, 0, nullptr, 0, R3F_FN(EffectKind9E_Hold), 0, false, kEf, 0, 0, 0, 0x9E},
    {"EffectKind9E_Close", 0x48F300, 0x54, R3F_CALLS(kCalls48F300), nullptr, 0, nullptr, 0, R3F_FN(EffectKind9E_Close), 0, false, kEf, 0, 0, 0, 0x9E},
    {"EffectKind9E_Shrink", 0x48F360, 0x67, R3F_CALLS(kCalls48F360), nullptr, 0, nullptr, 0, R3F_FN(EffectKind9E_Shrink), 0, false, kEf, 0, 0, 0, 0x9E},
    {"EffectKind9E_DrawOutline", 0x48F3D0, 0x1F4, R3F_CALLS(kCalls48F3D0), nullptr, 0, nullptr, 0, R3F_FN(EffectKind9E_DrawOutline), 0, false, kCa, 0, 0, 0, 0x9E},
    {"EffectKindA1_CaptureSprite", 0x490A80, 0x125, R3F_CALLS(kCalls490A80), nullptr, 0, nullptr, 0, R3F_FN(EffectKindA1_CaptureSprite), 0, false, kEf, 0, 0, 0, 0xA1},
    {"EffectKindA1_ReadBack", 0x490BB0, 0x91, R3F_CALLS(kCalls490BB0), nullptr, 0, nullptr, 0, R3F_FN(EffectKindA1_ReadBack), 0, false, kEf, 0, 0, 0, 0xA1},
    {"EffectKindA1_SplitPixels", 0x490C50, 0x1C7, R3F_CALLS(kCalls490C50), nullptr, 0, nullptr, 0, R3F_FN(EffectKindA1_SplitPixels), 0, false, kEf, 0, 0, 0, 0xA1},
    {"EffectKindA1_AimPixels", 0x490E20, 0x180, R3F_CALLS(kCalls490E20), nullptr, 0, nullptr, 0, R3F_FN(EffectKindA1_AimPixels), 0, false, kEf, 0, 0, 0, 0xA1},
    {"EffectKindA1_MovePixels", 0x490FA0, 0x126, R3F_CALLS(kCalls490FA0), nullptr, 0, nullptr, 0, R3F_FN(EffectKindA1_MovePixels), 0, false, kEf, 0, 0, 0, 0xA1},
    {"EffectKindA2_Start", 0x4910F0, 0x43, R3F_CALLS(kCalls4910F0), nullptr, 0, nullptr, 0, R3F_FN(EffectKindA2_Start), 0, false, kEf, 0, 0, 0, 0xA2},
    {"EffectKindA2_Grow", 0x491140, 0x7A, R3F_CALLS(kCalls491140), nullptr, 0, nullptr, 0, R3F_FN(EffectKindA2_Grow), 0, false, kEf, 0, 0, 0, 0xA2},
    {"EffectKindA2_Rewind", 0x4911C0, 0xAE, R3F_CALLS(kCalls4911C0), nullptr, 0, nullptr, 0, R3F_FN(EffectKindA2_Rewind), 0, false, kEf, 0, 0, 0, 0xA2},
    {"EffectKindA2_Shrink", 0x491270, 0x7A, R3F_CALLS(kCalls491270), nullptr, 0, nullptr, 0, R3F_FN(EffectKindA2_Shrink), 0, false, kEf, 0, 0, 0, 0xA2},
    {"EffectKindA3_CaptureSprite", 0x491310, 0xFC, R3F_CALLS(kCalls491310), nullptr, 0, nullptr, 0, R3F_FN(EffectKindA3_CaptureSprite), 0, false, kEf, 0, 0, 0, 0xA3},
    {"EffectKindA3_ReadBack", 0x491410, 0x91, R3F_CALLS(kCalls491410), nullptr, 0, nullptr, 0, R3F_FN(EffectKindA3_ReadBack), 0, false, kEf, 0, 0, 0, 0xA3},
    {"EffectKindA3_SplitPixels", 0x4914B0, 0x1C7, R3F_CALLS(kCalls4914B0), nullptr, 0, nullptr, 0, R3F_FN(EffectKindA3_SplitPixels), 0, false, kEf, 0, 0, 0, 0xA3},
    {"EffectKindA3_AimPixels", 0x491680, 0x13E, R3F_CALLS(kCalls491680), nullptr, 0, nullptr, 0, R3F_FN(EffectKindA3_AimPixels), 0, false, kEf, 0, 0, 0, 0xA3},
    {"EffectKindA3_MovePixels", 0x4917C0, 0xF0, R3F_CALLS(kCalls4917C0), nullptr, 0, nullptr, 0, R3F_FN(EffectKindA3_MovePixels), 0, false, kEf, 0, 0, 0, 0xA3},
    {"EffectKindA2_DrawRing", 0x4918B0, 0x1E2, R3F_CALLS(kCalls4918B0), nullptr, 0, nullptr, 0, R3F_FN(EffectKindA2_DrawRing), 0xFF, false, kCa, 0, 0, 0, 0xA2},
    {"EffectKindA7_Start", 0x491AE0, 0x1F, nullptr, 0, nullptr, 0, nullptr, 0, R3F_FN(EffectKindA7_Start), 0, false, kEf, 0, 0, 0, 0xA7},
    {"EffectKindA7_Grow", 0x491B00, 0x34, nullptr, 0, nullptr, 0, nullptr, 0, R3F_FN(EffectKindA7_Grow), 0, false, kEf, 0, 0, 0, 0xA7},
    {"EffectKindA7_Hold", 0x491B40, 0x26, nullptr, 0, nullptr, 0, nullptr, 0, R3F_FN(EffectKindA7_Hold), 0, false, kEf, 0, 0, 0, 0xA7},
    {"EffectKindA7_Shrink", 0x491B70, 0x2D, R3F_CALLS(kCalls491B70), nullptr, 0, nullptr, 0, R3F_FN(EffectKindA7_Shrink), 0, false, kEf, 0, 0, 0, 0xA7},
    {"EffectKindA8_Start", 0x491BC0, 0x23, R3F_CALLS(kCalls491BC0), nullptr, 0, nullptr, 0, R3F_FN(EffectKindA8_Start), 0, false, kEf, 0, 0, 0, 0xA8},
    {"EffectKindA8_Bars", 0x491BF0, 0x47, R3F_CALLS(kCalls491BF0), nullptr, 0, nullptr, 0, R3F_FN(EffectKindA8_Bars), 0, false, kEf, 0, 0, 0, 0xA8},
    {"EffectKindA8_End", 0x491C40, 0x17, R3F_CALLS(kCalls491C40), nullptr, 0, nullptr, 0, R3F_FN(EffectKindA8_End), 0, false, kEf, 0, 0, 0, 0xA8},
    {"EffectKindA9_Start", 0x491CA0, 0x28, nullptr, 0, nullptr, 0, nullptr, 0, R3F_FN(EffectKindA9_Start), 0, false, kEf, 0, 0, 0, 0xA9},
    {"EffectKindA9_Grow", 0x491CD0, 0x3A, nullptr, 0, nullptr, 0, nullptr, 0, R3F_FN(EffectKindA9_Grow), 0, false, kEf, 0, 0, 0, 0xA9},
    {"EffectKindA9_Wait", 0x491D10, 0x1B, nullptr, 0, nullptr, 0, nullptr, 0, R3F_FN(EffectKindA9_Wait), 0, false, kEf, 0, 0, 0, 0xA9},
    {"EffectKindA9_Shrink", 0x491D30, 0x33, R3F_CALLS(kCalls491D30), nullptr, 0, nullptr, 0, R3F_FN(EffectKindA9_Shrink), 0, false, kEf, 0, 0, 0, 0xA9},
    {"EffectKindAA_Start", 0x491D90, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, R3F_FN(EffectKindAA_Start), 0, false, kEf, 0, 0, 0, 0xAA},
    {"EffectKindAA_FadeIn", 0x491DB0, 0x3B, R3F_CALLS(kCalls491DB0), nullptr, 0, nullptr, 0, R3F_FN(EffectKindAA_FadeIn), 0, false, kEf, 0, 0, 0, 0xAA},
    {"EffectKindAA_FadeOut", 0x491DF0, 0x3F, R3F_CALLS(kCalls491DF0), nullptr, 0, nullptr, 0, R3F_FN(EffectKindAA_FadeOut), 0, false, kEf, 0, 0, 0, 0xAA},
    {"EffectKindA7_DrawGlow", 0x491E30, 0x1B7, R3F_CALLS(kCalls491E30), nullptr, 0, nullptr, 0, R3F_FN(EffectKindA7_DrawGlow), 0, false, kCa, 0, 0, 0, 0xA7},
    {"EffectKindA8_ClearBars", 0x491FF0, 0x14, nullptr, 0, nullptr, 0, nullptr, 0, R3F_FN(EffectKindA8_ClearBars), 0, false, kEf, 0, 0, 0, 0xA8},
    {"EffectKindA8_StartBar", 0x492010, 0x1F, nullptr, 0, nullptr, 0, nullptr, 0, R3F_FN(EffectKindA8_StartBar), 0, false, kEf, 0, 0, 0, 0xA8},
    {"EffectKindA8_StepBars", 0x492030, 0xB7, R3F_CALLS(kCalls492030), nullptr, 0, nullptr, 0, R3F_FN(EffectKindA8_StepBars), 0xFF, false, kCa, 0, 0, 0, 0xA8},
    {"EffectKindA8_DrawBar", 0x4920F0, 0x170, R3F_CALLS(kCalls4920F0), nullptr, 0, nullptr, 0, R3F_FN(EffectKindA8_DrawBar), 0, false, kCa, 0, 0, 0, 0xA8},
    {"EffectKindA9_DrawDisc", 0x492260, 0x19D, R3F_CALLS(kCalls492260), nullptr, 0, nullptr, 0, R3F_FN(EffectKindA9_DrawDisc), 0, false, kCa, 0, 0, 0, 0xA9},
    {"EffectKindAA_DrawFill", 0x492400, 0x108, R3F_CALLS(kCalls492400), nullptr, 0, nullptr, 0, R3F_FN(EffectKindAA_DrawFill), 0, false, kCa, 0, 0, 0, 0xAA},
    {"EffectKindAB_Drops", 0x492580, 0x1C, R3F_CALLS(kCalls492580), nullptr, 0, nullptr, 0, R3F_FN(EffectKindAB_Drops), 0, false, kEf, 0, 0, 0, 0xAB},
};
#undef R3F_FN
#undef R3F_CALLS
#undef R3F_N

enum : unsigned {
    k60Start, k60SlideBoth, k60SlideEnd, k5FDisc, k69Lines, k9CTrail,
    k9EStart, k9EGrow, k9EFlash, k9EHold, k9EClose, k9EShrink, k9EOutline,
    kA1Capture, kA1ReadBack, kA1Split, kA1Aim, kA1Move,
    kA2Start, kA2Grow, kA2Rewind, kA2Shrink,
    kA3Capture, kA3ReadBack, kA3Split, kA3Aim, kA3Move, kA2Ring,
    kA7Start, kA7Grow, kA7Hold, kA7Shrink, kA8Start, kA8Bars, kA8End,
    kA9Start, kA9Grow, kA9Wait, kA9Shrink, kAAStart, kAAFadeIn, kAAFadeOut,
    kA7Glow, kA8Clear, kA8StartBar, kA8Step, kA8DrawBar, kA9Disc, kAAFill, kABDrops, kCount
};
static_assert(kCount == sizeof kAll / sizeof kAll[0], "one enum entry a clone, in order");

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
unsigned char* P(U a) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a)); }
unsigned char* At(U a) { return P(a); }
template <typename... T> U PickOf(T... v) {
    const U values[] = {static_cast<U>(v)...};
    return sh::Pick(values, sizeof...(v));
}

// --- the effects -----------------------------------------------------------------------

// The caller's stack from just below this frame to its base (the TIB's
// StackBase): where the originals' and ours' locals are (effect_4e_fuzz.cpp).
bool OnStack(const void* p, unsigned n) {
    std::uint32_t base;
    __asm__("movl %%fs:4, %0" : "=r"(base));
    const auto here = static_cast<U>(reinterpret_cast<std::uintptr_t>(&base));
    const auto at = static_cast<U>(reinterpret_cast<std::uintptr_t>(p));
    return at > here && at + n > at && at + n <= base;
}
bool Writable(U at, unsigned n) { return sh::InRegions(P(at), n) || OnStack(P(at), n); }

// The harness's packet buffer (scenario_harness.cpp g_packets, 0x800 bytes).
// The draws here write up to 0x44 bytes past the cursor before it moves (the
// POLY_G4s), past the harness's 0x40, so the cursor stops 0x90 short of the
// end (effect_4e_fuzz.cpp's spare).
constexpr unsigned kPacketsSize = 0x800;
constexpr unsigned kSpare = 0x90;
void Advance(unsigned size) {
    unsigned char* const next = Gfx_PacketNext;
    unsigned char* const base = sh::Packets();
    if (next >= base && next + size + kSpare <= base + kPacketsSize) Gfx_PacketNext = next + size;
}
// Gfx_CommitPrim: the cursor += size (a byte) while the packet stays in the buffer.
U FxCommit(const U* a, U answer) {
    Advance(a[1] & 0xFF);
    return answer;
}
// MapView_LinkPrimAt: the cursor += size & 0xFF two times in three (the
// harness's fold row), with this file's spare.
U FxLink(const U* a, U answer) {
    if (sh::Noise() % 3 != 0) Advance(a[3] & 0xFF);
    return answer;
}
// EffectGte_ProjectSize(point, size, out): both callers write both words of
// the size (hashed, four bytes); out's first word a radius - small, at the
// loop's edges (0, 1, -1, 0x8000) or random below 0x60 - never a large
// positive one, which would run EffectKind5F_DrawLineDisc's rows for minutes;
// the second word random.
U FxProjectSize(const U* a, U answer) {
    if (Writable(a[2], 4)) {
        const U n = sh::Noise();
        static const U kRadii[] = {0, 1, 0x3F, 0x50, 0xFFFF, 0xFFC0, 0x8000, 0x10, 0x20};
        const U r = (n & 1) ? kRadii[(n >> 1) % (sizeof kRadii / sizeof kRadii[0])] : (n >> 4) % 0x60;
        const U v = (r & 0xFFFFu) | (n & 0xFFFF0000u);
        std::memcpy(P(a[2]), &v, 4);
    }
    return answer;
}

#define R3F_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
constexpr sh::Answer kG = sh::Answer::kGarbage, kPh = sh::Answer::kPhase, kF = sh::Answer::kFlag;
constexpr U kW = 0xFFFFFFFFu, k16 = 0xFFFFu, k8 = 0xFFu;
const sh::Callee kCallees[] = {
    // the group's own called directly, by name: the void ones with no argument
    {R3F_OURS(EffectKindA8_ClearBars), 0, {}, kPh, 0, 0},
    {R3F_OURS(EffectKindA8_StartBar), 0, {}, kPh, 0, 0},
    // those with arguments, each mask what the callee reads: a record's point
    // (the same address on both sides) logged and hashed; the shade and the
    // length a byte / word (FadeIn pushes eax with Sprite_Current's upper half
    // and the product's high byte above it; the bars' caller an immediate)
    {R3F_OURS(EffectKind9E_DrawOutline), 2, {kW, kW}, kG, 0, 0, {12, 12}, nullptr, nullptr, true},
    {R3F_OURS(EffectKindA2_DrawRing), 3, {kW, kW, kW}, kG, 0, 0, {12}, nullptr, nullptr, true},
    {R3F_OURS(EffectKindA8_StepBars), 1, {k16}, kF, 0, 0},
    {R3F_OURS(EffectKindA8_DrawBar), 1, {kW}, kG, 0, 0, {6}, nullptr, nullptr, true},
    {R3F_OURS(EffectKindAA_DrawFill), 1, {k8}, kG, 0, 0},
    // merged groups' draws the states call by name: E3A's line, E4E's plates
    // (the wall's second point the cell 0x6762A0, a region of this group's),
    // E4F's drops
    {R3F_OURS(EffectKind60_DrawLine), 2, {kW, kW}, kG, 0, 0, {12, 12}, nullptr, nullptr, true},
    {R3F_OURS(EffectKind9E_DrawPlate), 2, {kW, kW}, kG, 0, 0, {12, 12}, nullptr, nullptr, true},
    {R3F_OURS(EffectKind9E_DrawTexPlate), 2, {kW, kW}, kG, 0, 0, {12, 12}, nullptr, nullptr, true},
    {R3F_OURS(EffectKind9E_DrawWall), 2, {kW, kW}, kG, 0, 0, {12, 12}, nullptr, nullptr, true},
    {R3F_OURS(EffectKind9E_DrawShadePlate), 2, {kW, kW}, kG, 0, 0, {12, 12}, nullptr, nullptr, true},
    {R3F_OURS(EffectKindAB_Emit), 0, {}, kPh, 0, 0},
    {R3F_OURS(EffectKindAB_MoveDrops), 0, {}, kPh, 0, 0},
    // standard rows re-listed: the cursor's spare for the commits and the map
    // links; EffectGte_ProjectSize's radius bounded
    {R3F_OURS(Gfx_CommitPrim), 2, {k8, k8}, kG, 0, 0, {0, 0}, &FxCommit, nullptr, true},
    {R3F_OURS(MapView_LinkPrimAt), 4, {kW, kW, k8, k8}, kG, 0, 0, {}, &FxLink, nullptr, true},
    {R3F_OURS(EffectGte_ProjectSize), 3, {0, 0, 0}, kG, 0, 0, {12, 4}, &FxProjectSize, nullptr, true},
};
#undef R3F_OURS

const std::uint8_t kKinds[] = {0x5F, 0x60, 0x69, 0x9C, 0x9E, 0xA1, 0xA2, 0xA3, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB};

// Beyond the standard effect regions: kind 0x9E's wall vector, kinds 0xA1 /
// 0xA3's cells, the rest of the read-back buffer past the standard
// EffectKind30_Shards region (to 0x3000 bytes from its start: the largest
// capture record's half window, 0x2D00, from a cursor up to 0x300 in), and the
// first 64 particles.
constexpr U kShardsEnd = sh::at::kShards + sh::at::kShardsSize;
constexpr U kPixelsEnd = sh::at::kShards + 0x3000;
constexpr unsigned kPartRoom = 64;
const sh::Region kRegions[] = {
    {at::k9EWall, 0x10},
    {at::kPixCursor, at::kPixCellsSize},
    {kShardsEnd, kPixelsEnd - kShardsEnd},
    {at::kParticles, kPartRoom * at::kPartStride},
};

// --- the seed ------------------------------------------------------------------------

unsigned char* Rec(unsigned r) { return sh::EffectRecord(r); }
unsigned char* Bar(unsigned i) { return EffectKind30_Shards + i * at::kBarStride; }
unsigned g_current = 0;   // the clone being fuzzed (Seed sets it; Disturb reads it)

bool Splits(unsigned k) { return k == kA1Split || k == kA3Split; }
bool Moves(unsigned k) { return k == kA1Aim || k == kA1Move || k == kA3Aim || k == kA3Move; }

// The capture cells for the split: a record 0..2, the pixel cursor up to
// 0x300 bytes into the read-back, its largest window (0x2D00 bytes) zeroed
// and up to 24 pixels planted, the particle cursor so that every particle the
// split can make stays inside the first 64.
void SeedSplit() {
    At(at::kPixIndex)[0] = static_cast<unsigned char>(PickOf(1, 1, 0, 2));
    const U cursor = sh::at::kShards + 2 * (sh::Next() % 0x180);
    SetLong(At(at::kPixCursor), static_cast<std::int32_t>(cursor));
    std::memset(P(cursor), 0, 0x2D00);
    const unsigned planted = sh::Next() % 25;
    for (unsigned n = 0; n < planted; ++n) {
        // inside the smallest record's half window (0x20 x 0x18 pixels) most of the time
        const U at = sh::Often() ? sh::Next() % (0x20 * 0x18) : sh::Next() % (0x60 * 0x3C);
        SetWord(P(cursor + 2 * at), (sh::Next() | 1) & 0xFFFF);
    }
    const U first = sh::Next() % (kPartRoom - 24);
    SetLong(At(at::kPartCursor), static_cast<std::int32_t>(at::kParticles + first * at::kPartStride));
}

void Seed(unsigned k) {
    g_current = k;
    // every record the disturbance may make current: +0x4C a sprite record,
    // +9 the countdowns' edges (0..3 for the split, whose row loop needs it)
    for (unsigned r = 0; r < sh::at::kEffectCount; ++r) {
        unsigned char* const e = Rec(r);
        SetLong(e + 0x4C, static_cast<std::int32_t>(Key(sh::SpriteRecord(r))));
        e[9] = static_cast<unsigned char>(Splits(k) ? sh::Next() % 4 : PickOf(1, 1, 0, 2, 0x10, 0x11, 0x14, 0xFF, sh::Next()));
    }
    // the sprites' bits the capture's copy tests: +0x24 bit 0, the word +0x2C
    for (unsigned r = 0; r < 4; ++r) {
        unsigned char* const sp = sh::SpriteRecord(r);
        if (sh::Half()) sp[0x24] = static_cast<unsigned char>(sp[0x24] ^ 1);
        if (sh::Half()) SetWord(sp + 0x2C, 0);
    }
    unsigned char* const s = Sprite_Current;
    s[6] = static_cast<unsigned char>(PickOf(0, 0, 1, 4, 5, 8, 9, 0xC, 0xD, sh::Next()));
    s[0xB] = static_cast<unsigned char>(PickOf(0, 0, 1, 2, 3, sh::Next()));
    At(at::kCounter)[0] = static_cast<unsigned char>(PickOf(0x29, 0xE, 9, 0x2B, 0, 0x28, sh::Next()));
    switch (k) {
    case k60Start:
    case k60SlideBoth:
    case k60SlideEnd:
        // the ends one step from 0x98000, at it, either side
        if (sh::Often()) SetLong(s + 0x34, static_cast<std::int32_t>(0xB8000u + PickOf(0, 0, 1, 0xFFFFFFFFu, 0x20000)));
        if (sh::Often()) SetLong(s + 0xC, static_cast<std::int32_t>(0xB8000u + PickOf(0, 0, 1, 0xFFFFFFFFu, 0x20000)));
        break;
    case k69Lines: s[0xA] = static_cast<unsigned char>(PickOf(0, 1, 2, 3, 5, 0x10, sh::Next() % 0x20)); break;
    case k9CTrail:
        // the far end below the near one (the dots run) half the time
        if (sh::Half())
            SetLong(s + 0x10, Long(s + 0x38) - static_cast<std::int32_t>(PickOf(0, 0x40, 0x3F, 0x200, 0x1000, 0x10000, sh::Next() % 0x100000)));
        break;
    case k9EHold:
        // the wall's third word landing on +0x14 half the time; +7 the count
        if (sh::Half())
            SetLong(At(at::k9EWall + 8), static_cast<std::int32_t>(static_cast<U>(Long(s + 0x14)) -
                                                                      static_cast<U>(Long(s + 0x20) >> 1) +
                                                                      PickOf(0, 0, 1, 0xFFFFFFFFu)));
        if (sh::Half()) s[7] = At(at::kCounter)[0];
        break;
    case kA1Capture:
    case kA3Capture:
        if (sh::Half()) At(at::kCounter)[0] = 0x29;
        break;
    case kA1ReadBack:
    case kA3ReadBack: At(at::kPixIndex)[0] = static_cast<unsigned char>(PickOf(1, 1, 0, 2)); break;
    case kA1Split:
    case kA3Split: SeedSplit(); break;
    case kA1Aim:
    case kA1Move:
    case kA3Aim:
    case kA3Move:
        SetWord(At(at::kPartCount), PickOf(0, 1, 15, 16, 17, 33, kPartRoom, sh::Next() % (kPartRoom + 1)));
        break;
    case kA9Wait:
        if (sh::Half()) At(at::kCounter)[0] = 9;
        break;
    case kA8Start:
    case kA8Bars:
    case kA8End:
    case kA8Clear:
    case kA8StartBar:
    case kA8Step:
    case kA8DrawBar:
        for (unsigned i = 0; i < at::kBarCount; ++i) {
            unsigned char* const b = Bar(i);
            b[0] = static_cast<unsigned char>(sh::Half() ? 0u : PickOf(1, 1, 0x80, sh::Next()));
            b[2] = static_cast<unsigned char>(PickOf(0, 1, 2, 2, 3, sh::Next()));
            SetWord(b + 4, PickOf(0, 8, 7, 9, 0xF0, 0x7FFF, 0x8000, sh::Next()));
        }
        if (k == kA8Bars && sh::Half()) At(at::kCounter)[0] = 0xE;
        break;
    case kABDrops: Draw_PassFlags = static_cast<unsigned char>(PickOf(0, 0, 1, 2, sh::Next())); break;
    default: break;
    }
}

// The draws' arguments: the record's own points as the callers pass them, the
// radii, sizes, shades and lengths over leftover upper bytes.
void Args(unsigned k, U* a) {
    const U s = Key(Sprite_Current);
    switch (k) {
    case k5FDisc:
        a[0] = sh::Half() ? s + 0x34 : s + 0xC;
        a[2] = PickOf(8, 0xFFFFFFF8u, 0, 0x8000, 0x7FFF, sh::Next());
        a[3] = (sh::Next() & 0xFFFFFF00u) | PickOf(0, 0xFE, 1, sh::Next() & 0xFF);
        break;
    case k9CTrail:
        a[0] = s + 0x34;
        a[1] = s + 0xC;
        a[2] = (sh::Next() & 0xFFFFFF00u) | PickOf(0, 8, 0x80, 0xFF, sh::Next() & 0xFF);
        break;
    case k9EOutline:
        a[0] = s + 0x34;
        a[1] = sh::Half() ? s + 0xC : at::k9EWall;
        break;
    case kA2Ring:
        a[0] = s + 0x34;
        a[1] = PickOf(0, 0x200, 0x1000, 0xFFFFF000u, sh::Next());
        a[2] = PickOf(0, 0x100, 0x2000, 0xFFFFE000u, sh::Next());
        break;
    case kA7Glow:
        a[0] = s + 0x34;
        a[1] = (sh::Next() & 0xFFFF0000u) | PickOf(0, 0x10, 0x100, 0x7FFF, 0x8000, sh::Next() & 0xFFFF);
        a[2] = (sh::Next() & 0xFFFFFF00u) | PickOf(7, 0, 0xFF, 3, sh::Next() & 0xFF);
        break;
    case kA8Step: a[0] = (sh::Next() & 0xFFFF0000u) | PickOf(0xF0, 0, 8, 7, 0x7FFF, 0x8000, sh::Next() & 0xFFFF); break;
    case kA8DrawBar: a[0] = Key(Bar(sh::Next() % at::kBarCount)); break;
    case kA9Disc:
        a[0] = (sh::Next() & 0xFFFF0000u) | PickOf(0, 0xD2, 0x7FFF, 0x8000, 0xFFF9, sh::Next() & 0xFFFF);
        a[1] = (sh::Next() & 0xFFFF0000u) | PickOf(0, 0x5A, 0x7FFF, 0x8000, 0xFFFD, sh::Next() & 0xFFFF);
        a[2] = (sh::Next() & 0xFFFFFF00u) | PickOf(0x80, 0, 0xFF, 3, sh::Next() & 0xFF);
        break;
    case kAAFill: a[0] = (sh::Next() & 0xFFFFFF00u) | PickOf(0, 0xF0, 0xFF, 0x10, sh::Next() & 0xFF); break;
    default: break;
    }
}

// What the functions read again after a call, moved by the group's case of the
// harness's disturbance (from its hash only): +9 (0..3 for the split), +6,
// +0xB, the chapter's count, Draw_PassFlags, the particle count (inside the
// 64), the capture record (inside the three).
void Disturb(U h) {
    const U v = h >> 8;
    unsigned char* const s = Sprite_Current;
    const bool in = sh::InRegions(s, 0x80);
    switch (h % 7) {
    case 0:
        if (in) s[9] = static_cast<unsigned char>(Splits(g_current) ? v % 4 : ((v & 1) ? 1u : v >> 1));
        break;
    case 1:
        if (in) s[6] = static_cast<unsigned char>(v);
        break;
    case 2:
        if (in) s[0xB] = static_cast<unsigned char>(v);
        break;
    case 3: {
        static const unsigned char kCounts[] = {0x29, 0xE, 9, 0x2B};
        At(at::kCounter)[0] = (v & 1) ? kCounts[(v >> 1) % 4] : static_cast<unsigned char>(v >> 1);
        break;
    }
    case 4: Draw_PassFlags = static_cast<unsigned char>((v & 1) ? 0u : v >> 1); break;
    case 5:
        if (Moves(g_current)) SetWord(At(at::kPartCount), v % (kPartRoom + 1));
        break;
    case 6: At(at::kPixIndex)[0] = static_cast<unsigned char>(v % at::kPixRecords); break;
    default: break;
    }
}

}  // namespace

void SelfTest() {
    // BOF3X_R3F_ONLY: the clones whose name contains it (a control's run)
    static sh::Clone chosen[kCount];
    static unsigned index[kCount];
    const char* const only = std::getenv("BOF3X_R3F_ONLY");
    unsigned n = 0;
    for (unsigned k = 0; k < kCount; ++k)
        if (!only || !*only || std::strstr(kAll[k].name, only)) {
            index[n] = k;
            chosen[n++] = kAll[k];
        }
    if (n == 0) bof3::Fatal("rest_3f: BOF3X_R3F_ONLY=%s names no clone", only);
    static unsigned* s_index = index;
    sh::Group g = {"rest_3f", chosen, n, kCallees, sizeof kCallees / sizeof kCallees[0], nullptr, 0,
                   kRegions, sizeof kRegions / sizeof kRegions[0], [](unsigned k) { Seed(s_index[k]); }, &Disturb, 4000};
    if (const char* r = std::getenv("BOF3X_R3F_ROUNDS")) g.rounds = static_cast<unsigned>(std::atoi(r));
    g.args = [](unsigned k, U* a) { Args(s_index[k], a); };
    g.effect = true;
    g.kinds = kKinds;
    g.n_kinds = sizeof kKinds;
    sh::Run(g);
}

}  // namespace rest_3f

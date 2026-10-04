// BOF3X_SHADOW=effect_5c: group E5C's 62 functions through the scenario harness
// in effect mode (scenario_harness.h, docs/scenario_harness.md section 8), once
// at start-up. docs/effect_5c.md section 4. BOF3X_E5C_ONLY=<name> runs the
// clones whose name contains it (the controls' speed-up).
//
// The clone table is tools/band_rows.py --group E5C --clones --harness scenario
// (2026-10-03), each extent read again to its last instruction (capstone) and
// the names given, with two changes: 0x501520 ends at its jmp to 0x5016E0 (the
// tool's extent ran on through 0x5015E0, the state table's entry 1, taken as
// its own function EffectKind18Sub50_Shine; the jmp into it at +0xA9 is a call
// site), and 0x5016E0, which no list of the cut holds, added. Shapes: every
// dispatcher and state kEffect (Sprite_Current one of the 20 Effect_Objects
// records, +5 0x18, a dispatcher's +2 below its table's length); the draws
// with arguments kCall.
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/effect_5c.h"
#include "game/effect_5c_callees.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace effect_5c {
namespace {

namespace sh = scenario_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using U = std::uint32_t;

// tools/band_rows.py --group E5C --clones, 2026-10-03 (0x501520 / 0x5015E0
// split by hand: the header's note).
constexpr sh::CallSite kCalls501520[] = {{0x4A, 0x5720C0}, {0x95, 0x57C140}, {0xA9, 0x5015E0}, {0xB2, 0x5016E0}};
constexpr sh::CallSite kCalls5015E0[] = {{0x17, 0x57C140}, {0x4D, 0x57C140}, {0x6A, 0x501850}, {0x72, 0x501A10}};
constexpr sh::CallSite kCalls501660[] = {{0x20, 0x501850}, {0x47, 0x57C140}, {0x64, 0x501850}, {0x6C, 0x501A10}};
constexpr sh::CallSite kCalls5016E0[] = {{0x17, 0x57C140}, {0x38, 0x501850}};
constexpr sh::CallSite kCalls501730[] = {{0x32, 0x5A77C0}, {0x48, 0x572FA0}, {0x69, 0x57C140}, {0x86, 0x501850}, {0x92, 0x5A75B0},
                                         {0x9A, 0x5A7780}, {0xE3, 0x5A85F0}, {0xE9, 0x5A9240}, {0x102, 0x572FA0}, {0x10A, 0x501A10}};
constexpr sh::CallSite kCalls501850[] = {{0x88, 0x5720C0},  {0xAE, 0x5720C0},  {0xF1, 0x5720C0},  {0x117, 0x5720C0}, {0x13F, 0x5A75D0},
                                         {0x147, 0x5A77A0}, {0x17A, 0x5A85F0}, {0x183, 0x5A9290}, {0x195, 0x572A00}, {0x1AB, 0x572FA0}};
constexpr sh::CallSite kCalls501A10[] = {{0x16, 0x5A77C0},  {0x2C, 0x572FA0},  {0x38, 0x5A7650},  {0x40, 0x5A7780},
                                         {0x79, 0x5720C0},  {0xCD, 0x5720C0},  {0x124, 0x5A84A0}, {0x136, 0x5A9130},
                                         {0x14C, 0x572FA0}, {0x162, 0x5A77C0}, {0x17B, 0x572FA0}};
constexpr sh::CallSite kCalls501BC0[] = {{0x13, 0x57C140}, {0x5F, 0x501E80}, {0xD5, 0x501E80}};
constexpr sh::CallSite kCalls501CA0[] = {{0x17, 0x587740}, {0x84, 0x587740}, {0x96, 0x501E80}};
constexpr sh::CallSite kCalls501D40[] = {{0x1A, 0x501E80}};
constexpr sh::CallSite kCalls501DD0[] = {{0x16, 0x501CA0}, {0x38, 0x587740}, {0x40, 0x501E80}};
constexpr sh::CallSite kCalls501E20[] = {{0x7, 0x57C140}, {0x53, 0x501E80}};
constexpr sh::CallSite kCalls501E80[] = {{0x24, 0x5A77C0},  {0x2D, 0x461E50},  {0x39, 0x5A75D0},  {0x41, 0x5A77A0},
                                         {0xA7, 0x5720C0},  {0xC4, 0x5720C0},  {0x101, 0x5720C0}, {0x121, 0x5720C0},
                                         {0x16A, 0x5A85F0}, {0x170, 0x5A9290}, {0x17D, 0x572A00}, {0x189, 0x461E50}};
constexpr sh::CallSite kCalls502060[] = {{0x37, 0x589840}, {0x3D, 0x5020C0}};
constexpr sh::CallSite kCalls5020C0[] = {{0x74, 0x5A75D0},  {0x7C, 0x5A77A0},  {0x84, 0x5A7780},  {0x8E, 0x5A7A00},  {0x16C, 0x5A85F0},
                                         {0x175, 0x5A9290}, {0x1E3, 0x461E50}, {0x253, 0x5A8250}, {0x28A, 0x5A75D0}, {0x292, 0x5A77A0},
                                         {0x30B, 0x5A92E0}, {0x313, 0x5A7780}, {0x381, 0x572FA0}};
constexpr sh::CallSite kCalls5024C0[] = {{0x9, 0x589840}, {0x2B, 0x589810}, {0x92, 0x5B93D2}};
constexpr sh::CallSite kCalls5025A0[] = {{0xE, 0x587740}};
constexpr sh::CallSite kCalls5025C0[] = {{0xC, 0x502670}};
constexpr sh::CallSite kCalls502600[] = {{0x2, 0x502670}};
constexpr sh::CallSite kCalls502630[] = {{0xF, 0x502670}, {0x30, 0x589840}};
constexpr sh::CallSite kCalls502670[] = {{0x75, 0x5A75D0}, {0x7D, 0x5A77A0},  {0xC5, 0x5A8250},
                                         {0xCB, 0x5A92E0}, {0x156, 0x572A00}, {0x174, 0x572FA0}};
constexpr sh::CallSite kCalls502830[] = {{0x7, 0x57C140}, {0x1A, 0x57C140}, {0x6F, 0x502A00}, {0x87, 0x589840}};
constexpr sh::CallSite kCalls5028C0[] = {{0x7, 0x57C140}, {0x1A, 0x57C140}, {0x68, 0x502A00}};
constexpr sh::CallSite kCalls502940[] = {{0xE, 0x502A00}};
constexpr sh::CallSite kCalls5029B0[] = {{0x13, 0x502A00}, {0x43, 0x589840}};
constexpr sh::CallSite kCalls502A00[] = {{0x22, 0x5A75D0}, {0x2A, 0x5A77A0},  {0xDE, 0x5A85F0},
                                         {0xE4, 0x5A9290}, {0xED, 0x572A00}, {0x10C, 0x572FA0}};
constexpr sh::CallSite kCalls502B50[] = {{0x7, 0x57C140}, {0x1A, 0x589840}, {0x5F, 0x502CE0}};
constexpr sh::CallSite kCalls502BC0[] = {{0x7, 0x57C140}, {0x4C, 0x502CE0}};
constexpr sh::CallSite kCalls502C20[] = {{0xE, 0x502CE0}};
constexpr sh::CallSite kCalls502C90[] = {{0x13, 0x502CE0}, {0x43, 0x589840}};
constexpr sh::CallSite kCalls502CE0[] = {{0x22, 0x5A75D0}, {0x2A, 0x5A77A0},  {0xDE, 0x5A85F0},
                                         {0xE4, 0x5A9290}, {0xED, 0x572A00}, {0x10C, 0x572FA0}};
constexpr sh::CallSite kCalls502E30[] = {{0x13, 0x5A7AE0}};
constexpr sh::CallSite kCalls502E60[] = {{0x3E, 0x5A77C0},  {0x47, 0x461E50},  {0x53, 0x5A75D0},  {0x5B, 0x5A77A0},  {0xB3, 0x461E50},
                                         {0xEC, 0x5A77C0},  {0xF8, 0x461E50},  {0x109, 0x5A75D0}, {0x111, 0x5A77A0}, {0x1D4, 0x5032C0},
                                         {0x1FE, 0x5A77C0}, {0x205, 0x5032C0}, {0x233, 0x5A7610}, {0x23E, 0x5A7780}, {0x2EC, 0x5032C0},
                                         {0x31E, 0x5A77C0}, {0x325, 0x5032C0}};
constexpr sh::CallSite kCalls5032C0[] = {{0x34, 0x5A7560}};
constexpr sh::CallSite kCalls503320[] = {{0x47, 0x5A7B90},  {0x5D, 0x5A77C0},  {0x66, 0x461E50},  {0xA9, 0x5A8200},  {0xB8, 0x5A8060},
                                         {0xCC, 0x5A7D70},  {0xD6, 0x5A8DE0},  {0xE3, 0x5A8E00},  {0xF2, 0x5A7630},  {0xFA, 0x5A77A0},
                                         {0x178, 0x5A7A50}, {0x1A0, 0x5A7A50}, {0x1D5, 0x5A7A50}, {0x243, 0x5A7A00}, {0x264, 0x5A7A00},
                                         {0x28E, 0x5A7A00}, {0x2EC, 0x5A85F0}, {0x2F2, 0x5A93A0}, {0x2FB, 0x461E50}, {0x320, 0x5A77C0},
                                         {0x329, 0x461E50}, {0x331, 0x5A7BC0}};
constexpr sh::CallSite kCalls5036D0[] = {{0x1D, 0x589810}, {0x6B, 0x5043B0}, {0x75, 0x503FA0}, {0x7C, 0x503FA0}, {0x84, 0x503E50}};
constexpr sh::CallSite kCalls503760[] = {{0x4B, 0x5043B0}, {0x55, 0x503FA0}, {0x5C, 0x503FA0}, {0x64, 0x503E50}};
constexpr sh::CallSite kCalls5037D0[] = {{0x3D, 0x5043B0}, {0x47, 0x503FA0}, {0x4E, 0x503FA0}, {0x56, 0x503E50}};
constexpr sh::CallSite kCalls503830[] = {{0x4E, 0x5043B0}, {0x58, 0x503FA0}, {0x5F, 0x503FA0}, {0x67, 0x503E50}};
constexpr sh::CallSite kCalls5038A0[] = {{0x54, 0x5043B0}, {0x5E, 0x503FA0}, {0x65, 0x503FA0}, {0x6D, 0x503E50}};
constexpr sh::CallSite kCalls503920[] = {{0x2E, 0x5043B0}, {0x46, 0x503FA0}};
constexpr sh::CallSite kCalls5039B0[] = {{0x62, 0x5043B0}, {0x7A, 0x503FA0}};
constexpr sh::CallSite kCalls503A40[] = {{0x70, 0x5043B0}, {0x88, 0x503FA0}};
constexpr sh::CallSite kCalls503AE0[] = {{0x26, 0x589840}, {0x44, 0x5043B0}, {0x5C, 0x503FA0}};
constexpr sh::CallSite kCalls503B50[] = {{0x48, 0x572650}};
constexpr sh::CallSite kCalls503BB0[] = {{0x23, 0x5043B0}, {0x2C, 0x572650}, {0x5C, 0x5043B0}, {0x66, 0x503FA0}};
constexpr sh::CallSite kCalls503C20[] = {{0x2E, 0x5043B0}};
constexpr sh::CallSite kCalls503CA0[] = {{0x62, 0x5043B0}};
constexpr sh::CallSite kCalls503D30[] = {{0x70, 0x5043B0}, {0xA3, 0x572650}};

#define E5C_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define E5C_CALLS(a) a, E5C_N(a)
#define E5C_FN(name) reinterpret_cast<const void*>(&::name)
constexpr sh::Shape kEf = sh::Shape::kEffect;
constexpr sh::Shape kCa = sh::Shape::kCall;
// {name, base, size, calls, imms, tables, ours, ret_mask, calm, shape, pointers, state_span, sub_span, kind}
const sh::Clone kAll[] = {
    {"EffectKind18Sub50_Run", 0x501500, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E5C_FN(EffectKind18Sub50_Run), 0, false, kEf, 0, 0, 5, 0x18},
    {"EffectKind18Sub50_Start", 0x501520, 0xB7, E5C_CALLS(kCalls501520), nullptr, 0, nullptr, 0, E5C_FN(EffectKind18Sub50_Start), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub50_Shine", 0x5015E0, 0x77, E5C_CALLS(kCalls5015E0), nullptr, 0, nullptr, 0, E5C_FN(EffectKind18Sub50_Shine), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub50_Sink", 0x501660, 0x71, E5C_CALLS(kCalls501660), nullptr, 0, nullptr, 0, E5C_FN(EffectKind18Sub50_Sink), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub50_WaitSet", 0x5016E0, 0x41, E5C_CALLS(kCalls5016E0), nullptr, 0, nullptr, 0, E5C_FN(EffectKind18Sub50_WaitSet), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub50_Flash", 0x501730, 0x114, E5C_CALLS(kCalls501730), nullptr, 0, nullptr, 0, E5C_FN(EffectKind18Sub50_Flash), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub50_DrawWall", 0x501850, 0x1B8, E5C_CALLS(kCalls501850), nullptr, 0, nullptr, 0, E5C_FN(EffectKind18Sub50_DrawWall), 0, false, kCa, 0, 0, 0, 0x18},
    {"EffectKind18Sub50_DrawLine", 0x501A10, 0x189, E5C_CALLS(kCalls501A10), nullptr, 0, nullptr, 0, E5C_FN(EffectKind18Sub50_DrawLine), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub10_Run", 0x501BA0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E5C_FN(EffectKind18Sub10_Run), 0, false, kEf, 0, 0, 6, 0x18},
    {"EffectKind18Sub10_Start", 0x501BC0, 0xDC, E5C_CALLS(kCalls501BC0), nullptr, 0, nullptr, 0, E5C_FN(EffectKind18Sub10_Start), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub10_Wait", 0x501CA0, 0x9D, E5C_CALLS(kCalls501CA0), nullptr, 0, nullptr, 0, E5C_FN(EffectKind18Sub10_Wait), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub10_SlideOut", 0x501D40, 0x1F, E5C_CALLS(kCalls501D40), nullptr, 0, nullptr, 0, E5C_FN(EffectKind18Sub10_SlideOut), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub10_Hold", 0x501D60, 0x61, nullptr, 0, nullptr, 0, nullptr, 0, E5C_FN(EffectKind18Sub10_Hold), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub10_SlideIn", 0x501DD0, 0x46, E5C_CALLS(kCalls501DD0), nullptr, 0, nullptr, 0, E5C_FN(EffectKind18Sub10_SlideIn), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub10_WaitFlag", 0x501E20, 0x58, E5C_CALLS(kCalls501E20), nullptr, 0, nullptr, 0, E5C_FN(EffectKind18Sub10_WaitFlag), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub10_DrawPanel", 0x501E80, 0x198, E5C_CALLS(kCalls501E80), nullptr, 0, nullptr, 0, E5C_FN(EffectKind18Sub10_DrawPanel), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub56_Run", 0x502020, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E5C_FN(EffectKind18Sub56_Run), 0, false, kEf, 0, 0, 2, 0x18},
    {"EffectKind18Sub56_Wait", 0x502040, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E5C_FN(EffectKind18Sub56_Wait), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub56_Glow", 0x502060, 0x54, E5C_CALLS(kCalls502060), nullptr, 0, nullptr, 0, E5C_FN(EffectKind18Sub56_Glow), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub56_DrawRays", 0x5020C0, 0x3A9, E5C_CALLS(kCalls5020C0), nullptr, 0, nullptr, 0, E5C_FN(EffectKind18Sub56_DrawRays), 0, false, kCa, 0, 0, 0, 0x18},
    {"EffectKind18Sub57_Run", 0x502470, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E5C_FN(EffectKind18Sub57_Run), 0, false, kEf, 0, 0, 2, 0x18},
    {"EffectKind18Sub57_Wait", 0x502490, 0x25, nullptr, 0, nullptr, 0, nullptr, 0, E5C_FN(EffectKind18Sub57_Wait), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub57_Spawn", 0x5024C0, 0xB6, E5C_CALLS(kCalls5024C0), nullptr, 0, nullptr, 0, E5C_FN(EffectKind18Sub57_Spawn), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub58_Run", 0x502580, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E5C_FN(EffectKind18Sub58_Run), 0, false, kEf, 0, 0, 4, 0x18},
    {"EffectKind18Sub58_Start", 0x5025A0, 0x1F, E5C_CALLS(kCalls5025A0), nullptr, 0, nullptr, 0, E5C_FN(EffectKind18Sub58_Start), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub58_Grow", 0x5025C0, 0x39, E5C_CALLS(kCalls5025C0), nullptr, 0, nullptr, 0, E5C_FN(EffectKind18Sub58_Grow), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub58_Hold", 0x502600, 0x2F, E5C_CALLS(kCalls502600), nullptr, 0, nullptr, 0, E5C_FN(EffectKind18Sub58_Hold), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub58_Fade", 0x502630, 0x36, E5C_CALLS(kCalls502630), nullptr, 0, nullptr, 0, E5C_FN(EffectKind18Sub58_Fade), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub58_DrawColumn", 0x502670, 0x19F, E5C_CALLS(kCalls502670), nullptr, 0, nullptr, 0, E5C_FN(EffectKind18Sub58_DrawColumn), 0, false, kCa, 0, 0, 0, 0x18},
    {"EffectKind18Sub11_Run", 0x502810, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E5C_FN(EffectKind18Sub11_Run), 0, false, kEf, 0, 0, 4, 0x18},
    {"EffectKind18Sub11_Start", 0x502830, 0x8C, E5C_CALLS(kCalls502830), nullptr, 0, nullptr, 0, E5C_FN(EffectKind18Sub11_Start), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub11_Wait", 0x5028C0, 0x71, E5C_CALLS(kCalls5028C0), nullptr, 0, nullptr, 0, E5C_FN(EffectKind18Sub11_Wait), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub11_Open", 0x502940, 0x6D, E5C_CALLS(kCalls502940), nullptr, 0, nullptr, 0, E5C_FN(EffectKind18Sub11_Open), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub11_Sink", 0x5029B0, 0x49, E5C_CALLS(kCalls5029B0), nullptr, 0, nullptr, 0, E5C_FN(EffectKind18Sub11_Sink), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub11_DrawTiles", 0x502A00, 0x129, E5C_CALLS(kCalls502A00), nullptr, 0, nullptr, 0, E5C_FN(EffectKind18Sub11_DrawTiles), 0, false, kCa, 0, 0, 0, 0x18},
    {"EffectKind18Sub12_Run", 0x502B30, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E5C_FN(EffectKind18Sub12_Run), 0, false, kEf, 0, 0, 4, 0x18},
    {"EffectKind18Sub12_Start", 0x502B50, 0x70, E5C_CALLS(kCalls502B50), nullptr, 0, nullptr, 0, E5C_FN(EffectKind18Sub12_Start), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub12_Wait", 0x502BC0, 0x55, E5C_CALLS(kCalls502BC0), nullptr, 0, nullptr, 0, E5C_FN(EffectKind18Sub12_Wait), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub12_Open", 0x502C20, 0x6D, E5C_CALLS(kCalls502C20), nullptr, 0, nullptr, 0, E5C_FN(EffectKind18Sub12_Open), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub12_Sink", 0x502C90, 0x49, E5C_CALLS(kCalls502C90), nullptr, 0, nullptr, 0, E5C_FN(EffectKind18Sub12_Sink), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub12_DrawTiles", 0x502CE0, 0x129, E5C_CALLS(kCalls502CE0), nullptr, 0, nullptr, 0, E5C_FN(EffectKind18Sub12_DrawTiles), 0, false, kCa, 0, 0, 0, 0x18},
    {"EffectKind18Sub15_Run", 0x502E10, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E5C_FN(EffectKind18Sub15_Run), 0, false, kEf, 0, 0, 2, 0x18},
    {"EffectKind18Sub15_Start", 0x502E30, 0x24, E5C_CALLS(kCalls502E30), nullptr, 0, nullptr, 0, E5C_FN(EffectKind18Sub15_Start), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub15_Draw", 0x502E60, 0x45D, E5C_CALLS(kCalls502E60), nullptr, 0, nullptr, 0, E5C_FN(EffectKind18Sub15_Draw), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub15_LinkLayer", 0x5032C0, 0x5A, E5C_CALLS(kCalls5032C0), nullptr, 0, nullptr, 0, E5C_FN(EffectKind18Sub15_LinkLayer), 0, false, kCa, 0, 0, 0, 0x18},
    {"EffectKind18Sub16_Run", 0x503320, 0x33E, E5C_CALLS(kCalls503320), nullptr, 0, nullptr, 0, E5C_FN(EffectKind18Sub16_Run), 0, false, kEf, 0, 0, 2, 0x18},
    {"EffectKind18Sub17_Run", 0x503660, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E5C_FN(EffectKind18Sub17_Run), 0, false, kEf, 0, 0, 16, 0x18},
    {"EffectKind18Sub17_Start", 0x503680, 0x4C, nullptr, 0, nullptr, 0, nullptr, 0, E5C_FN(EffectKind18Sub17_Start), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub17_WaitCue", 0x5036D0, 0x89, E5C_CALLS(kCalls5036D0), nullptr, 0, nullptr, 0, E5C_FN(EffectKind18Sub17_WaitCue), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub17_Rise", 0x503760, 0x69, E5C_CALLS(kCalls503760), nullptr, 0, nullptr, 0, E5C_FN(EffectKind18Sub17_Rise), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub17_MoveZ", 0x5037D0, 0x5B, E5C_CALLS(kCalls5037D0), nullptr, 0, nullptr, 0, E5C_FN(EffectKind18Sub17_MoveZ), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub17_MoveXZ", 0x503830, 0x6C, E5C_CALLS(kCalls503830), nullptr, 0, nullptr, 0, E5C_FN(EffectKind18Sub17_MoveXZ), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub17_MoveX", 0x5038A0, 0x72, E5C_CALLS(kCalls5038A0), nullptr, 0, nullptr, 0, E5C_FN(EffectKind18Sub17_MoveX), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub17_Count", 0x503920, 0x84, E5C_CALLS(kCalls503920), nullptr, 0, nullptr, 0, E5C_FN(EffectKind18Sub17_Count), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub17_WaitEnemy0", 0x5039B0, 0x85, E5C_CALLS(kCalls5039B0), nullptr, 0, nullptr, 0, E5C_FN(EffectKind18Sub17_WaitEnemy0), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub17_Settle", 0x503A40, 0x93, E5C_CALLS(kCalls503A40), nullptr, 0, nullptr, 0, E5C_FN(EffectKind18Sub17_Settle), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub17_End", 0x503AE0, 0x67, E5C_CALLS(kCalls503AE0), nullptr, 0, nullptr, 0, E5C_FN(EffectKind18Sub17_End), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub17_Start10", 0x503B50, 0x59, E5C_CALLS(kCalls503B50), nullptr, 0, nullptr, 0, E5C_FN(EffectKind18Sub17_Start10), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub17_WaitCue11", 0x503BB0, 0x6D, E5C_CALLS(kCalls503BB0), nullptr, 0, nullptr, 0, E5C_FN(EffectKind18Sub17_WaitCue11), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub17_Count12", 0x503C20, 0x79, E5C_CALLS(kCalls503C20), nullptr, 0, nullptr, 0, E5C_FN(EffectKind18Sub17_Count12), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub17_WaitEnemy1", 0x503CA0, 0x85, E5C_CALLS(kCalls503CA0), nullptr, 0, nullptr, 0, E5C_FN(EffectKind18Sub17_WaitEnemy1), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub17_Settle14", 0x503D30, 0xAE, E5C_CALLS(kCalls503D30), nullptr, 0, nullptr, 0, E5C_FN(EffectKind18Sub17_Settle14), 0, false, kEf, 0, 0, 0, 0x18},
};
#undef E5C_FN
#undef E5C_CALLS
#undef E5C_N

enum : unsigned {
    k50Run, k50Start, k50Shine, k50Sink, k50WaitSet, k50Flash, k50Wall, k50Line,
    k10Run, k10Start, k10Wait, k10SlideOut, k10Hold, k10SlideIn, k10WaitFlag, k10Panel,
    k56Run, k56Wait, k56Glow, k56Rays,
    k57Run, k57Wait, k57Spawn,
    k58Run, k58Start, k58Grow, k58Hold, k58Fade, k58Column,
    k11Run, k11Start, k11Wait, k11Open, k11Sink, k11Tiles,
    k12Run, k12Start, k12Wait, k12Open, k12Sink, k12Tiles,
    k15Run, k15Start, k15Draw, k15Link,
    k16Run,
    k17Run, k17Start, k17WaitCue, k17Rise, k17MoveZ, k17MoveXZ, k17MoveX, k17Count, k17WaitEnemy0, k17Settle, k17End,
    k17Start10, k17WaitCue11, k17Count12, k17WaitEnemy1, k17Settle14, kCount
};
static_assert(kCount == sizeof kAll / sizeof kAll[0], "one enum entry a clone, in order");

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
unsigned char* P(U a) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a)); }
unsigned char* Mem(U a) { return sh::Mem(a); }
unsigned char* Rec(unsigned r) { return sh::EffectRecord(r); }
template <typename... T> U PickOf(T... v) {
    const U values[] = {static_cast<U>(v)...};
    return sh::Pick(values, sizeof...(v));
}

unsigned g_k;   // the clone being fuzzed (Seed's k)
U g_size;       // EffectKind18Sub15_LinkLayer's size this round (Seed picks it, Args passes it)

// --- the stand-ins -----------------------------------------------------------------

// The harness's packet buffer (scenario_harness.cpp g_packets, 0x800 bytes;
// its own advances keep 0x40 to spare).
constexpr unsigned kPacketsSize = 0x800;
void Advance(unsigned n) {
    unsigned char* const next = Gfx_PacketNext;
    unsigned char* const base = sh::Packets();
    if (next >= base && next + n + 0x40 <= base + kPacketsSize) Gfx_PacketNext = next + n;
}
// MapView_LinkPrimAt (world_map.cpp): the cursor += size & 0xFF when the row is
// on the map - two times in three here (the harness's row, re-listed for the
// row byte's mask below).
U FxLink(const U* a, U answer) {
    if (sh::Noise() % 3 != 0) Advance(a[3] & 0xFF);
    return answer;
}
// EffectKind18Sub15_LinkLayer for its caller: the cursor += size & 0xFF two
// times in three (the real one when the pool has room); the gradients after
// it are built at the cursor as it stood, so a caller that read it again
// would show.
U FxLinkLayer(const U* a, U answer) {
    if (sh::Noise() % 3 != 0) Advance(a[0] & 0xFF);
    return answer;
}
// EffectKind18Sub50_DrawWall for its callers: the cursor on by the quad (0x48)
// two times in three - EffectKind18Sub50_Flash builds its POLY_F4 at the
// cursor after it.
U FxWall(const U*, U answer) {
    if (sh::Noise() % 3 != 0) Advance(0x48);
    return answer;
}
// E5D's 0x5043B0 reads Sprite_Current +0xB beside its step; E5D's 0x503E50 the
// point +0x34 / +0x38: logged, so a call on the wrong record shows.
U FxStep(const U*, U answer) {
    unsigned char* const s = Sprite_Current;
    if (sh::InRegions(s, 0x80)) sh::Note(Key(s), s[0xB]);
    return answer;
}
U FxMoves(const U*, U answer) {
    unsigned char* const s = Sprite_Current;
    if (sh::InRegions(s, 0x80)) sh::Note(Key(s), static_cast<U>(Long(s + 0x34)), static_cast<U>(Long(s + 0x38)));
    return answer;
}
// Effect_FindFree: as the effect-mode row (a free record from a start the
// answer picks, none a quarter of the time), except for
// EffectKind18Sub17_WaitCue, which writes the answer's record untested: never
// 0xFF there (the original would write past the pool, ours abort); a free
// record when there is one, else any (E1B's form).
U FxFindFree(const U*, U answer) {
    const bool unchecked = g_k == k17WaitCue;
    const U high = answer & 0xFFFFFF00u;
    if (!unchecked && (answer >> 8) % 4 == 0) return high | 0xFF;
    const unsigned from = (answer >> 12) % 20;
    for (unsigned i = 0; i < 20; ++i) {
        const unsigned k = (from + i) % 20;
        if (Rec(k)[0] == 0) return high | k;
    }
    return high | (unchecked ? from : 0xFFu);
}

#define E5C_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
#define E5C_RAW(address) #address, address, address
constexpr sh::Answer kG = sh::Answer::kGarbage, kPh = sh::Answer::kPhase;
constexpr U kW = 0xFFFFFFFFu, k16 = 0xFFFFu, k8 = 0xFFu;
const sh::Callee kCallees[] = {
    // the group's own called (or tail-jumped to) directly, by name
    {E5C_OURS(EffectKind18Sub50_Shine), 0, {}, kPh, 0, 0},
    {E5C_OURS(EffectKind18Sub50_WaitSet), 0, {}, kPh, 0, 0},
    {E5C_OURS(EffectKind18Sub50_DrawLine), 0, {}, kPh, 0, 0},
    {E5C_OURS(EffectKind18Sub10_Wait), 0, {}, kPh, 0, 0},
    {E5C_OURS(EffectKind18Sub10_DrawPanel), 0, {}, kPh, 0, 0},
    // the texture word whole (added to 0x285000F3); the height's low word (every
    // use of it lands in a vertex word)
    {E5C_OURS(EffectKind18Sub50_DrawWall), 2, {kW, k16}, kG, 0, 0, {}, &FxWall},
    // the level whole (multiplied, divided)
    {E5C_OURS(EffectKind18Sub56_DrawRays), 1, {kW}, kG, 0, 0},
    // n whole (multiplied, 8 - n the divisor)
    {E5C_OURS(EffectKind18Sub58_DrawColumn), 1, {kW}, kG, 0, 0},
    // y and the shade: each use lands in a word (the shade << 16 in the texture)
    {E5C_OURS(EffectKind18Sub11_DrawTiles), 2, {k16, k16}, kG, 0, 0},
    {E5C_OURS(EffectKind18Sub12_DrawTiles), 2, {k16, k16}, kG, 0, 0},
    // `and esi, 0xFF`
    {E5C_OURS(EffectKind18Sub15_LinkLayer), 1, {k8}, kG, 0, 0, {}, &FxLinkLayer},
    // group E5D's, raw: the step a whole word (an index); no argument
    {"0x5043B0", at::kDrawMoveStep, at::kDrawMoveStep, 1, {kW}, kG, 0, 0, {}, &FxStep},
    {"0x503E50", at::kDrawMoves, at::kDrawMoves, 0, {}, kG, 0, 0, {}, &FxMoves},
    {E5C_OURS(Effect_FindFree), 0, {}, sh::Answer::kByte, 0xFF, 0x13, {}, &FxFindFree, nullptr, true},
    // re-listed: the tiles pass the row byte in dl over a leftover edx; the
    // callee reads it as a signed byte (world_map.cpp), and the size as a byte
    {E5C_OURS(MapView_LinkPrimAt), 4, {kW, kW, k8, k8}, kG, 0, 0, {}, &FxLink, nullptr, true},
};
#undef E5C_RAW
#undef E5C_OURS

// The state tables the dispatchers jump through, read in place; each table's
// own length (symbols.toml [[data]], none bounded by a compare).
const sh::DataTable kTables[] = {
    {Key(EffectKind18Sub50_States), EffectKind18Sub50_States_count},
    {Key(EffectKind18Sub10_States), EffectKind18Sub10_States_count},
    {Key(EffectKind18Sub56_States), EffectKind18Sub56_States_count},
    {Key(EffectKind18Sub57_States), EffectKind18Sub57_States_count},
    {Key(EffectKind18Sub58_States), EffectKind18Sub58_States_count},
    {Key(EffectKind18Sub11_States), EffectKind18Sub11_States_count},
    {Key(EffectKind18Sub12_States), EffectKind18Sub12_States_count},
    {Key(EffectKind18Sub15_States), EffectKind18Sub15_States_count},
    {Key(EffectKind18Sub17_States), EffectKind18Sub17_States_count},
};
const std::uint8_t kKinds[] = {0x18};

// Beyond effect mode's standard regions: the two enemies' words sub-kind 0x17
// tests, layer 15's two last pointers and the buffer byte sub-kind 0x15's
// commit reads.
const sh::Region kRegions[] = {
    {at::kEnemy0Bits, 4},
    {at::kEnemy1Bits, 4},
    {at::kLayer15Tails, 0xC},
    {Key(&Gfx_BufferIndex), 1},
};

// --- the seed ------------------------------------------------------------------------

bool Sub50(unsigned k) { return k >= k50Run && k <= k50Line; }

void Seed(unsigned k) {
    g_k = k;
    // every record the disturbance may move Sprite_Current to: +9 at the
    // compares' boundaries, +0xB inside what the function indexes by it (two
    // variants for sub-kind 0x50's start, four flag bytes for its states, eight
    // points for sub-kind 0x57; any byte elsewhere), the words the states compare
    for (unsigned r = 0; r < 20; ++r) {
        unsigned char* const e = Rec(r);
        e[9] = static_cast<unsigned char>(PickOf(0, 1, 2, 4, 5, 7, 8, 9, 0xE, 0xF, 0x10, 0x13, 0x14, 0x18, 0x19, 0x1E, 0x1F, 0x20,
                                                 0x21, 0x58, 0x59, 0x77, 0x78, 0x79, 0xFF, sh::Next()));
        if (k == k50Start) {
            e[0xB] = static_cast<unsigned char>(sh::Next() % at::kSub50Variants);
            e[0x36] = static_cast<unsigned char>(sh::Next() % at::kSub50Variants);
        } else if (Sub50(k)) {
            e[0xB] = static_cast<unsigned char>(sh::Next() % at::kSub50FlagCount);
        } else if (k == k57Spawn) {
            e[0xB] = static_cast<unsigned char>(sh::Next() % at::kSub57PointCount);
        }
        SetWord(e + 0x30, PickOf(0, 0xFF00, 0xFF01, 0xFF20, 0xFF1F, 0xFFE0, 0xFFDF, 0x7FFF, 0x8000, 0xFFFF, 0x20, sh::Next()));
        SetWord(e + 0x32, PickOf(0x7F, 0x80, 0x81, 0, 1, 0xFFFF, 0x8000, 0x7FFF, 6, 0xFFFA, sh::Next()));
        SetWord(e + 0x3A, PickOf(0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x8000, 0x7FFF, sh::Next()));
        if (k != k50Start) SetWord(e + 0x36, PickOf(0x4E, 0x4F, 0x50, 0x8000, 0x7FFF, sh::Next()));
        SetLong(e + 0x3C, static_cast<std::int32_t>(PickOf(0xFFFFFF40u, 0xFFFFFF3Fu, 0xFFFFFF41u, 0xFFFFFF00u, 0, 0x7FFFFFFFu, sh::Next())));
    }
    // the shared cells at their compares
    Cond_ByteFE = static_cast<unsigned char>(PickOf(0, 0, 1, 0x23, sh::Next()));
    Field_Request = static_cast<unsigned char>(PickOf(0, 0, 2, 1, sh::Next()));
    Mem(at::kCounter)[0] = static_cast<unsigned char>(PickOf(0xC, 0xF, 0xD, 0xB, 0, sh::Next()));
    if (sh::Half()) Frame_Counter = 5 * (sh::Next() & 0x3FFFFFF) + PickOf(0, 0, 1, 4);
    if (sh::Half()) {
        Game_Mode = 5;
        Game_Step = static_cast<unsigned short>(PickOf(5, 5, 4, 0x105));
    }
    for (U bits : {at::kEnemy0Bits, at::kEnemy1Bits})
        if (sh::Half()) SetLong(Mem(bits), static_cast<std::int32_t>(sh::Next() | (sh::Half() ? 0x4000u : 0u)));
    // Cond_ByteFA (the chapter the harness writes) at sub-kind 0x11's compare
    Cond_ByteFA = static_cast<signed char>(PickOf(7, 8, 6, 0, 0x80, 0x7F, sh::Next()));
    // the leader near and at the edges of sub-kind 0x10's boxes
    SetLong(Mem(at::kLeaderX), static_cast<std::int32_t>(
                                   0x428000u + PickOf(0, 0x8000, 0x8001, 0xFFFF8000u, 0xFFFF7FFFu, 0x18000, 0x18001, 0xFFFE8000u,
                                                      0xFFFE7FFFu, sh::Next() & 0xFFFFF, sh::Next())));
    SetLong(Mem(at::kLeaderZ), static_cast<std::int32_t>(
                                   PickOf(0x3E0000u, 0x3F0000u) + PickOf(0, 0x10000, 0x10001, 0xFFFF0000u, 0xFFFEFFFFu, 0x18000,
                                                                         0x18001, 0xFFFE8000u, 0xFFFE7FFFu, sh::Next())));
    // Draw_PassFlags bit 2 (the panel) half the time
    Draw_PassFlags = static_cast<unsigned char>(sh::Half() ? (Draw_PassFlags | 4) : (Draw_PassFlags & ~4));
    // sub-kind 0x15's corner rows inside the area block's 8 KiB (a row of at
    // most 0x1F cells: rows 17..0x33 keep row - 17 .. row + 13 inside it)
    SetWord(Mem(at::kKind2ZCell), 17 + sh::Next() % 35);
    // the buffer byte 0 or 1 (it indexes the two last pointers); the pointers
    // into the scratch buffer (Gpu_LinkPrim's stand-in writes through them)
    Gfx_BufferIndex = static_cast<unsigned char>(sh::Next() % 2);
    for (unsigned b = 0; b < 2; ++b)
        SetLong(Mem(at::kLayer15Tails + 8 * b), static_cast<std::int32_t>(sh::Half() ? Key(sh::Scratch(b) + (sh::Next() & 0x1C)) : sh::Next()));
    // sub-kind 0x15's scroll: (Frame_Counter >> 3) & 0xFF at 0x3E..0x42 half the
    // time, where the middle quad's right edge crosses 320
    if (k == k15Draw && sh::Half())
        Frame_Counter = (sh::Next() & ~0x7FFu) | ((0x3Eu + sh::Next() % 5) << 3) | (sh::Next() & 7u);
    // sub-kind 0x57's spawn: +9 at 1 half the time (it spawns when +9 reaches 0)
    if (k == k57Spawn && sh::Half()) Sprite_Current[9] = 1;
    g_size = PickOf(0x48, 0x44, 0xC, 0, 0xFF, sh::Next() & 0xFF);
    if (k == k15Link && sh::Half()) {
        // the cursor at the pool's room test: the real pool's address, the
        // limit within a byte or two of cursor + size (nothing is written there)
        const U limit = (static_cast<U>(Gfx_BufferIndex) << 16) + at::kPoolLimit;
        Gfx_PacketNext = P(limit - g_size + PickOf(0, 1, 0xFFFFFFFFu, 0x40, 0xFFFFFFC0u));
    }
}

// The helpers with arguments: the level, n, y and shade at their boundaries;
// n never 8 (the original divides by zero there, ours aborts).
void Args(unsigned k, U* a) {
    switch (k) {
    case k50Wall:
        a[0] = PickOf(0, 1, 2, 5, 6, sh::Next());
        a[1] = PickOf(0x40, 0, 0x48, 8, sh::Next());
        break;
    case k56Rays: a[0] = PickOf(0, 0x80, 0x7C, 1, 0xFFFFFFFCu, 0x40, 0xFFFFFF80u, sh::Next()); break;
    case k58Column:
        a[0] = PickOf(0, 1, 2, 3, 4, 5, 6, 7, 9, 0x3F, 0xFF, sh::Next());
        if (a[0] == 8) a[0] = 7;
        break;
    case k11Tiles:
    case k12Tiles:
        a[0] = PickOf(0xFFFFFF00u, 0xFFFFFF40u, 0, sh::Next());
        a[1] = PickOf(0x80, 0x78, 0xF8, 0, sh::Next());
        break;
    case k15Link: a[0] = (sh::Next() & 0xFFFFFF00u) | g_size; break;
    default: break;
    }
}

// What the states read again after a call, moved by the group's case of the
// harness's disturbance (from its hash only): +9, +0xB (0 or 1: inside every
// table it indexes), the slide word +0x30, +0x32, +0x3C, Cond_ByteFE, the
// counter 0x903848, Field_Request.
void Disturb(U h) {
    const U v = h >> 8;
    unsigned char* const s = Sprite_Current;
    if (!sh::InRegions(s, 0x80)) return;
    switch (h % 8) {
    case 0: {
        static const unsigned char kNine[] = {0, 1, 7, 8, 0xF, 0x14, 0x1E, 0x78};
        s[9] = static_cast<unsigned char>((v & 1) ? kNine[(v >> 1) & 7] : v >> 4);
        break;
    }
    case 1: s[0xB] = static_cast<unsigned char>(v & 1); break;
    case 2: SetWord(s + 0x30, (v & 1) ? 0xFF00u : v >> 1); break;
    case 3: SetWord(s + 0x32, (v & 1) ? 0x80u : v >> 1); break;
    case 4: SetLong(s + 0x3C, static_cast<std::int32_t>((v & 1) ? 0xFFFFFF40u : v >> 1)); break;
    case 5: Cond_ByteFE = static_cast<unsigned char>((v & 1) ? 0u : v >> 1); break;
    case 6: Mem(at::kCounter)[0] = static_cast<unsigned char>((v & 1) ? ((v & 2) ? 0xCu : 0xFu) : v >> 2); break;
    case 7: Field_Request = static_cast<unsigned char>((v & 1) ? 0u : v >> 1); break;
    default: break;
    }
}

}  // namespace

void SelfTest() {
    // BOF3X_E5C_ONLY: the clones whose name contains it (a control's run)
    static sh::Clone chosen[kCount];
    static unsigned index[kCount];
    const char* const only = std::getenv("BOF3X_E5C_ONLY");
    unsigned n = 0;
    for (unsigned k = 0; k < kCount; ++k)
        if (!only || !*only || std::strstr(kAll[k].name, only)) {
            index[n] = k;
            chosen[n++] = kAll[k];
        }
    if (n == 0) bof3::Fatal("effect_5c: BOF3X_E5C_ONLY=%s names no clone", only);
    static unsigned* s_index = index;
    sh::Group g = {"effect_5c", chosen, n, kCallees, sizeof kCallees / sizeof kCallees[0], kTables,
                   sizeof kTables / sizeof kTables[0], kRegions, sizeof kRegions / sizeof kRegions[0],
                   [](unsigned k) { Seed(s_index[k]); }, &Disturb, 4000};
    g.args = [](unsigned k, U* a) { Args(s_index[k], a); };
    g.effect = true;
    g.kinds = kKinds;
    g.n_kinds = sizeof kKinds;
    sh::Run(g);
}

}  // namespace effect_5c

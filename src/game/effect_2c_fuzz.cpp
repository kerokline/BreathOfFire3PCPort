// BOF3X_SHADOW=effect_2c: group E2C's 53 functions through the scenario harness
// in effect mode (scenario_harness.h, docs/scenario_harness.md section 8), once
// at start-up. docs/effect_2c.md section 4. BOF3X_E2C_ONLY=<names> runs the
// clones named (comma-separated; the controls' speed-up).
//
// The clone table is tools/band_rows.py --group E2C --clones --harness scenario
// (2026-09-29), each extent read again to its last instruction (capstone), the
// names given, and kind 0x44's dispatcher 0x476680 added (in no list; its
// extent read by hand). Shapes: every dispatcher and state kEffect
// (Sprite_Current one of the 20 Effect_Objects records, +5 the kind, a
// dispatcher's +1 below its table's length); the helpers taking arguments
// kCall; the void helpers that read the record kEffect with no span.
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_2c.h"
#include "game/effect_2c_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace effect_2c {
namespace {

namespace sh = scenario_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using U = std::uint32_t;

// tools/band_rows.py --group E2C --clones, 2026-09-29.
constexpr sh::CallSite kCalls474F60[] = {{0x35, 0x587900}};
constexpr sh::CallSite kCalls474FC0[] = {{0x67, 0x4750D0}};
constexpr sh::CallSite kCalls475070[] = {{0xE, 0x587740}};
constexpr sh::CallSite kCalls4750B0[] = {{0xD, 0x475390}};
constexpr sh::CallSite kCalls4750D0[] = {{0x7, 0x494060},  {0x19, 0x5A79A0}, {0x31, 0x5A77C0}, {0x3A, 0x461E50},
                                         {0x6E, 0x494110}, {0x85, 0x5A7650}, {0x8D, 0x5A7780}, {0xF0, 0x5B93D2},
                                         {0x116, 0x494110}, {0x143, 0x461E50}, {0x14A, 0x475240}};
constexpr sh::CallSite kCalls475240[] = {{0x17, 0x5B9550}, {0x25, 0x5B9550}, {0x40, 0x5A8B60}, {0x68, 0x5A7610},
                                         {0x70, 0x5A7780}, {0xF5, 0x461E50}, {0x136, 0x461E50}};
constexpr sh::CallSite kCalls475390[] = {{0x14, 0x5A79A0}, {0x2C, 0x5A77C0}, {0x35, 0x461E50}, {0x3A, 0x494060},
                                         {0x49, 0x494110}, {0x58, 0x494110}, {0x77, 0x5A7A70}, {0x97, 0x4941E0},
                                         {0xBA, 0x4754A0}, {0xCA, 0x4941E0}, {0xEA, 0x4754A0}, {0xFD, 0x4755B0}};
constexpr sh::CallSite kCalls4754A0[] = {{0x26, 0x5A75F0}, {0x2E, 0x5A7780}, {0x4B, 0x5A7A50}, {0x64, 0x5A7A00},
                                         {0x92, 0x5A7A50}, {0xAB, 0x5A7A00}, {0xEC, 0x461E50}};
constexpr sh::CallSite kCalls4755B0[] = {{0xB, 0x5A7610},  {0x13, 0x5A7780},  {0x58, 0x5A7A50},  {0x72, 0x5A7A00},
                                         {0xA4, 0x5A7A50}, {0xBE, 0x5A7A00},  {0x103, 0x461E50}, {0x11E, 0x5A7A50},
                                         {0x13D, 0x5A7A00}, {0x15B, 0x5A7A50}, {0x17A, 0x5A7A00}, {0x197, 0x461E50}};
constexpr sh::CallSite kCalls475780[] = {{0x0, 0x4757F0}};
constexpr sh::CallSite kCalls4757A0[] = {{0x17, 0x475A90}, {0x1F, 0x475820}};
constexpr sh::CallSite kCalls475820[] = {{0x1, 0x494060},  {0x13, 0x5A79A0}, {0x2B, 0x5A77C0},
                                         {0x34, 0x461E50}, {0x65, 0x4758D0}, {0x7A, 0x475A60},
                                         {0x8A, 0x475A20}, {0x92, 0x475A60}, {0xA1, 0x475A20}};
constexpr sh::CallSite kCalls4758D0[] = {{0xE4, 0x5A7750}, {0xEC, 0x5A7780}, {0x13E, 0x461E50}};
constexpr sh::CallSite kCalls475A20[] = {{0x8, 0x5B93D2}, {0x1B, 0x5B93D2}};
constexpr sh::CallSite kCalls475A90[] = {{0x13, 0x5A79A0}, {0x2A, 0x5A77C0}, {0x33, 0x461E50},  {0x38, 0x494060},
                                         {0x61, 0x494110}, {0x8E, 0x5A7A50}, {0xA7, 0x5A7A00},  {0xD3, 0x494110},
                                         {0xF8, 0x5A75F0}, {0x100, 0x5A7780}, {0x140, 0x5A7A50}, {0x159, 0x5A7A00},
                                         {0x185, 0x494110}, {0x1DB, 0x461E50}};
constexpr sh::CallSite kCalls475CC0[] = {{0x22, 0x4220D0}};
constexpr sh::CallSite kCalls475D10[] = {{0x0, 0x476230}, {0x27, 0x587740}};
constexpr sh::CallSite kCalls475D40[] = {{0x0, 0x476290}, {0x5, 0x476290}, {0xA, 0x476290}, {0xF, 0x476290}, {0x14, 0x4762D0}};
constexpr sh::CallSite kCalls475D90[] = {{0x0, 0x4762D0}, {0x13, 0x587740}, {0x1D, 0x587740}};
constexpr sh::CallSite kCalls475DD0[] = {{0x0, 0x4762D0}, {0x9, 0x589840}};
constexpr sh::CallSite kCalls475E00[] = {{0x1, 0x589810}, {0x42, 0x461E10}, {0x9F, 0x588F20}, {0xA4, 0x589840}};
constexpr sh::CallSite kCalls475EC0[] = {{0x4C, 0x59E930}};
constexpr sh::CallSite kCalls475F20[] = {{0x14, 0x494060}, {0x40, 0x494110}, {0x116, 0x5B9550}, {0x123, 0x5B93D2}, {0x13B, 0x5B93D2}};
constexpr sh::CallSite kCalls4760E0[] = {{0x24, 0x587740}};
constexpr sh::CallSite kCalls476120[] = {{0x96, 0x5A7750}, {0xDF, 0x461E50}, {0x100, 0x589840}};
constexpr sh::CallSite kCalls476230[] = {{0x26, 0x5720C0}};
constexpr sh::CallSite kCalls4762D0[] = {{0x4, 0x494060},   {0x70, 0x5B93D2},  {0xDA, 0x5B93D2},  {0x11C, 0x5A7A50},
                                         {0x13C, 0x5A7A00}, {0x18F, 0x5A7A50}, {0x1B1, 0x5A7A00}, {0x1F3, 0x5B93D2},
                                         {0x218, 0x5A7A50}, {0x233, 0x5A7A00}, {0x258, 0x476560}};
constexpr sh::JumpTable kTables4762D0[] = {{0x39, 0x27C, 5}};   // the phase's five cases, 0x47654C
constexpr sh::CallSite kCalls476560[] = {{0x16, 0x5A79A0}, {0x2E, 0x5A77C0}, {0x46, 0x572FA0},  {0x51, 0x494110},
                                         {0x5D, 0x5A7750}, {0x65, 0x5A7780}, {0x87, 0x5B93D2},  {0x9D, 0x572FA0},
                                         {0xAC, 0x5A7740}, {0xB4, 0x5A7780}, {0xED, 0x5B93D2},  {0x105, 0x572FA0}};
constexpr sh::CallSite kCalls4766A0[] = {{0x8A, 0x476D00}};
constexpr sh::CallSite kCalls476750[] = {{0x14, 0x476950}, {0x20, 0x476A00}, {0x42, 0x476D00}};
constexpr sh::CallSite kCalls4767B0[] = {{0x6, 0x476950}, {0x12, 0x476A00}, {0x1A, 0x476D20}, {0x1F, 0x476FC0}};
constexpr sh::CallSite kCalls476800[] = {{0x24, 0x476950}, {0x2F, 0x476A00}, {0x3B, 0x476C00}, {0x43, 0x476D20}};
constexpr sh::CallSite kCalls476870[] = {{0x31, 0x476950}, {0x3D, 0x476A00}, {0x49, 0x476C00}, {0x51, 0x476D20}};
constexpr sh::CallSite kCalls4768F0[] = {{0x14, 0x476950}, {0x20, 0x476A00}, {0x2C, 0x476C00}, {0x34, 0x476D20}, {0x52, 0x589840}};
constexpr sh::CallSite kCalls476950[] = {{0x7, 0x494060},  {0x15, 0x494110}, {0x22, 0x494110},
                                         {0x40, 0x5A7A50}, {0x59, 0x5A7A00}, {0x7E, 0x494110}};
constexpr sh::CallSite kCalls476A00[] = {{0x14, 0x5A79A0},  {0x2C, 0x5A77C0},  {0x35, 0x461E50},  {0x7B, 0x5B9550},
                                         {0x9C, 0x5B9550},  {0xAC, 0x5A75B0},  {0xB4, 0x5A7780},  {0x138, 0x461E50},
                                         {0x154, 0x5A7740}, {0x15C, 0x5A7780}, {0x191, 0x461E50}, {0x19D, 0x5A7740},
                                         {0x1A5, 0x5A7780}, {0x1E2, 0x461E50}};
constexpr sh::CallSite kCalls476C00[] = {{0x12, 0x5A79A0}, {0x2A, 0x5A77C0}, {0x33, 0x461E50}, {0x67, 0x4941B0},
                                         {0x7B, 0x5A7570}, {0x83, 0x5A7780}, {0xE0, 0x461E50}};
constexpr sh::CallSite kCalls476D20[] = {{0x3D, 0x5720C0}, {0x58, 0x476DB0}};
constexpr sh::CallSite kCalls476DB0[] = {{0x14, 0x5A79A0},  {0x2C, 0x5A77C0},  {0x35, 0x461E50},  {0x3D, 0x494060},
                                         {0x88, 0x494110},  {0x95, 0x494110},  {0xCD, 0x5A7650},  {0xD5, 0x5A7780},
                                         {0x11B, 0x461E50}, {0x132, 0x5A7610}, {0x13A, 0x5A7780}, {0x1E7, 0x461E50}};
constexpr sh::CallSite kCalls476FC0[] = {{0x0, 0x477180},  {0x57, 0x5B93D2},  {0x64, 0x5B93D2},  {0x7E, 0x5A7A50},
                                         {0x9B, 0x5A7A00}, {0xC4, 0x5720C0},  {0xE3, 0x5A7A50},  {0xF5, 0x5A7A00},
                                         {0x112, 0x5B93D2}, {0x13C, 0x494060}, {0x14E, 0x494110}, {0x160, 0x494110}};

#define E2C_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define E2C_CALLS(a) a, E2C_N(a)
#define E2C_FN(name) reinterpret_cast<const void*>(&::name)
constexpr sh::Shape kEf = sh::Shape::kEffect;
constexpr sh::Shape kCa = sh::Shape::kCall;
constexpr U kScr01 = sh::ArgAt(0, sh::Arg::kScratch) | sh::ArgAt(1, sh::Arg::kScratch);
constexpr U kScr0 = sh::ArgAt(0, sh::Arg::kScratch);
constexpr U kScr03 = sh::ArgAt(0, sh::Arg::kScratch) | sh::ArgAt(3, sh::Arg::kScratch);
// {name, base, size, calls, imms, tables, ours, ret_mask, calm, shape, pointers, state_span, sub_span, kind}
const sh::Clone kAll[] = {
    {"EffectKind3E_Run", 0x474F40, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E2C_FN(EffectKind3E_Run), 0, false, kEf, 0, 2, 0, 0x3E},
    {"EffectKind3E_Start", 0x474F60, 0x55, E2C_CALLS(kCalls474F60), nullptr, 0, nullptr, 0, E2C_FN(EffectKind3E_Start), 0, false, kEf, 0, 0, 0, 0x3E},
    {"EffectKind3E_Bolts", 0x474FC0, 0x8A, E2C_CALLS(kCalls474FC0), nullptr, 0, nullptr, 0, E2C_FN(EffectKind3E_Bolts), 0, false, kEf, 0, 0, 2, 0x3E},
    {"EffectKind3E_SubWait", 0x475050, 0x13, nullptr, 0, nullptr, 0, nullptr, 0, E2C_FN(EffectKind3E_SubWait), 0, false, kEf, 0, 0, 0, 0x3E},
    {"EffectKind3E_SubSound", 0x475070, 0x15, E2C_CALLS(kCalls475070), nullptr, 0, nullptr, 0, E2C_FN(EffectKind3E_SubSound), 0, false, kEf, 0, 0, 0, 0x3E},
    {"EffectKind3E_DrawBolt", 0x4750D0, 0x165, E2C_CALLS(kCalls4750D0), nullptr, 0, nullptr, 0, E2C_FN(EffectKind3E_DrawBolt), 0, false, kCa, kScr01, 0, 0, 0x3E},
    {"EffectKind3E_BoltGlow", 0x475240, 0x146, E2C_CALLS(kCalls475240), nullptr, 0, nullptr, 0, E2C_FN(EffectKind3E_BoltGlow), 0, false, kCa, kScr01, 0, 0, 0x3E},
    {"EffectKind3F_Run", 0x475090, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E2C_FN(EffectKind3F_Run), 0, false, kEf, 0, 2, 0, 0x3F},
    {"EffectKind3F_Draw", 0x4750B0, 0x1E, E2C_CALLS(kCalls4750B0), nullptr, 0, nullptr, 0, E2C_FN(EffectKind3F_Draw), 0, false, kEf, 0, 0, 0, 0x3F},
    {"EffectKind3F_DrawBeam", 0x475390, 0x10D, E2C_CALLS(kCalls475390), nullptr, 0, nullptr, 0, E2C_FN(EffectKind3F_DrawBeam), 0, false, kCa, kScr01, 0, 0, 0x3F},
    {"EffectKind3F_DrawCap", 0x4754A0, 0x108, E2C_CALLS(kCalls4754A0), nullptr, 0, nullptr, 0, E2C_FN(EffectKind3F_DrawCap), 0, false, kCa, kScr0, 0, 0, 0x3F},
    {"EffectKind3F_DrawBand", 0x4755B0, 0x1A4, E2C_CALLS(kCalls4755B0), nullptr, 0, nullptr, 0, E2C_FN(EffectKind3F_DrawBand), 0, false, kCa, kScr03, 0, 0, 0x3F},
    {"EffectKind40_Run", 0x475760, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E2C_FN(EffectKind40_Run), 0, false, kEf, 0, 3, 0, 0x40},
    {"EffectKind40_Start", 0x475780, 0x19, E2C_CALLS(kCalls475780), nullptr, 0, nullptr, 0, E2C_FN(EffectKind40_Start), 0, false, kEf, 0, 0, 0, 0x40},
    {"EffectKind40_Grow", 0x4757A0, 0x43, E2C_CALLS(kCalls4757A0), nullptr, 0, nullptr, 0, E2C_FN(EffectKind40_Grow), 0, false, kEf, 0, 0, 0, 0x40},
    {"EffectKind40_ShardsClear", 0x4757F0, 0x2B, nullptr, 0, nullptr, 0, nullptr, 0, E2C_FN(EffectKind40_ShardsClear), 0, false, kEf, 0, 0, 0, 0x40},
    {"EffectKind40_ShardsStep", 0x475820, 0xA8, E2C_CALLS(kCalls475820), nullptr, 0, nullptr, 0, E2C_FN(EffectKind40_ShardsStep), 0, false, kEf, 0, 0, 0, 0x40},
    {"EffectKind40_ShardDraw", 0x4758D0, 0x14B, E2C_CALLS(kCalls4758D0), nullptr, 0, nullptr, 0, E2C_FN(EffectKind40_ShardDraw), 0, false, kEf, 0, 0, 0, 0x40},
    {"EffectKind40_ShardSpawn", 0x475A20, 0x36, E2C_CALLS(kCalls475A20), nullptr, 0, nullptr, 0, E2C_FN(EffectKind40_ShardSpawn), 0, false, kCa, 0, 0, 0, 0x40},
    {"EffectKind40_ShardFind", 0x475A60, 0x23, nullptr, 0, nullptr, 0, nullptr, 0, E2C_FN(EffectKind40_ShardFind), 0xFFFFFFFFu, false, kEf, 0, 0, 0, 0x40},
    {"EffectKind40_DrawDisc", 0x475A90, 0x201, E2C_CALLS(kCalls475A90), nullptr, 0, nullptr, 0, E2C_FN(EffectKind40_DrawDisc), 0, false, kCa, 0, 0, 0, 0x40},
    {"EffectKind42_Run", 0x475CA0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E2C_FN(EffectKind42_Run), 0, false, kEf, 0, 2, 0, 0x42},
    {"EffectKind42_Glow", 0x475CC0, 0x2B, E2C_CALLS(kCalls475CC0), nullptr, 0, nullptr, 0, E2C_FN(EffectKind42_Glow), 0, false, kEf, 0, 0, 0, 0x42},
    {"EffectKind43_Run", 0x475CF0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E2C_FN(EffectKind43_Run), 0, false, kEf, 0, 4, 0, 0x43},
    {"EffectKind43_Start", 0x475D10, 0x2E, E2C_CALLS(kCalls475D10), nullptr, 0, nullptr, 0, E2C_FN(EffectKind43_Start), 0, false, kEf, 0, 0, 0, 0x43},
    {"EffectKind43_Gather", 0x475D40, 0x46, E2C_CALLS(kCalls475D40), nullptr, 0, nullptr, 0, E2C_FN(EffectKind43_Gather), 0, false, kEf, 0, 0, 0, 0x43},
    {"EffectKind43_Wait", 0x475D90, 0x37, E2C_CALLS(kCalls475D90), nullptr, 0, nullptr, 0, E2C_FN(EffectKind43_Wait), 0, false, kEf, 0, 0, 0, 0x43},
    {"EffectKind43_Fade", 0x475DD0, 0xF, E2C_CALLS(kCalls475DD0), nullptr, 0, nullptr, 0, E2C_FN(EffectKind43_Fade), 0, false, kEf, 0, 0, 0, 0x43},
    {"EffectKind43_Setup", 0x476230, 0x51, E2C_CALLS(kCalls476230), nullptr, 0, nullptr, 0, E2C_FN(EffectKind43_Setup), 0, false, kEf, 0, 0, 0, 0x43},
    {"EffectKind43_AddSpark", 0x476290, 0x32, nullptr, 0, nullptr, 0, nullptr, 0, E2C_FN(EffectKind43_AddSpark), 0, false, kEf, 0, 0, 0, 0x43},
    {"EffectKind43_SparksRun", 0x4762D0, 0x290, E2C_CALLS(kCalls4762D0), nullptr, 0, E2C_CALLS(kTables4762D0), E2C_FN(EffectKind43_SparksRun), 0xFF, false, kEf, 0, 0, 0, 0x43},
    {"EffectKind43_SparkDraw", 0x476560, 0x114, E2C_CALLS(kCalls476560), nullptr, 0, nullptr, 0, E2C_FN(EffectKind43_SparkDraw), 0, false, kCa, 0, 0, 0, 0x43},
    {"EffectKind6B_Run", 0x475DE0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E2C_FN(EffectKind6B_Run), 0, false, kEf, 0, 5, 0, 0x6B},
    {"EffectKind6B_Capture", 0x475E00, 0xC0, E2C_CALLS(kCalls475E00), nullptr, 0, nullptr, 0, E2C_FN(EffectKind6B_Capture), 0, false, kEf, 0, 0, 0, 0x6B},
    {"EffectKind6B_Store", 0x475EC0, 0x60, E2C_CALLS(kCalls475EC0), nullptr, 0, nullptr, 0, E2C_FN(EffectKind6B_Store), 0, false, kEf, 0, 0, 0, 0x6B},
    {"EffectKind6B_Scatter", 0x475F20, 0x1B6, E2C_CALLS(kCalls475F20), nullptr, 0, nullptr, 0, E2C_FN(EffectKind6B_Scatter), 0, false, kEf, 0, 0, 0, 0x6B},
    {"EffectKind6B_Arm", 0x4760E0, 0x35, E2C_CALLS(kCalls4760E0), nullptr, 0, nullptr, 0, E2C_FN(EffectKind6B_Arm), 0, false, kEf, 0, 0, 0, 0x6B},
    {"EffectKind6B_Fall", 0x476120, 0x108, E2C_CALLS(kCalls476120), nullptr, 0, nullptr, 0, E2C_FN(EffectKind6B_Fall), 0, false, kEf, 0, 0, 0, 0x6B},
    {"EffectKind44_Run", 0x476680, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E2C_FN(EffectKind44_Run), 0, false, kEf, 0, 6, 0, 0x44},
    {"EffectKind44_Start", 0x4766A0, 0xA1, E2C_CALLS(kCalls4766A0), nullptr, 0, nullptr, 0, E2C_FN(EffectKind44_Start), 0, false, kEf, 0, 0, 0, 0x44},
    {"EffectKind44_FadeIn", 0x476750, 0x59, E2C_CALLS(kCalls476750), nullptr, 0, nullptr, 0, E2C_FN(EffectKind44_FadeIn), 0, false, kEf, 0, 0, 0, 0x44},
    {"EffectKind44_Sparks", 0x4767B0, 0x4A, E2C_CALLS(kCalls4767B0), nullptr, 0, nullptr, 0, E2C_FN(EffectKind44_Sparks), 0, false, kEf, 0, 0, 0, 0x44},
    {"EffectKind44_Widen", 0x476800, 0x6E, E2C_CALLS(kCalls476800), nullptr, 0, nullptr, 0, E2C_FN(EffectKind44_Widen), 0, false, kEf, 0, 0, 0, 0x44},
    {"EffectKind44_Follow", 0x476870, 0x7C, E2C_CALLS(kCalls476870), nullptr, 0, nullptr, 0, E2C_FN(EffectKind44_Follow), 0, false, kEf, 0, 0, 0, 0x44},
    {"EffectKind44_FadeOut", 0x4768F0, 0x58, E2C_CALLS(kCalls4768F0), nullptr, 0, nullptr, 0, E2C_FN(EffectKind44_FadeOut), 0, false, kEf, 0, 0, 0, 0x44},
    {"EffectKind44_RingProject", 0x476950, 0xA2, E2C_CALLS(kCalls476950), nullptr, 0, nullptr, 0, E2C_FN(EffectKind44_RingProject), 0, false, kCa, 0, 0, 0, 0x44},
    {"EffectKind44_RingDraw", 0x476A00, 0x1F2, E2C_CALLS(kCalls476A00), nullptr, 0, nullptr, 0, E2C_FN(EffectKind44_RingDraw), 0, false, kCa, 0, 0, 0, 0x44},
    {"EffectKind44_RingCone", 0x476C00, 0xFE, E2C_CALLS(kCalls476C00), nullptr, 0, nullptr, 0, E2C_FN(EffectKind44_RingCone), 0, false, kCa, 0, 0, 0, 0x44},
    {"EffectKind44_SparksClear", 0x476D00, 0x1C, nullptr, 0, nullptr, 0, nullptr, 0, E2C_FN(EffectKind44_SparksClear), 0, false, kEf, 0, 0, 0, 0x44},
    {"EffectKind44_SparksStep", 0x476D20, 0x90, E2C_CALLS(kCalls476D20), nullptr, 0, nullptr, 0, E2C_FN(EffectKind44_SparksStep), 0, false, kEf, 0, 0, 0, 0x44},
    {"EffectKind44_SparkTrail", 0x476DB0, 0x206, E2C_CALLS(kCalls476DB0), nullptr, 0, nullptr, 0, E2C_FN(EffectKind44_SparkTrail), 0, false, kCa, 0, 0, 0, 0x44},
    {"EffectKind44_SparkEmit", 0x476FC0, 0x1B5, E2C_CALLS(kCalls476FC0), nullptr, 0, nullptr, 0, E2C_FN(EffectKind44_SparkEmit), 0, false, kEf, 0, 0, 0, 0x44},
    {"EffectKind44_SparkFind", 0x477180, 0x25, nullptr, 0, nullptr, 0, nullptr, 0, E2C_FN(EffectKind44_SparkFind), 0xFFFFFFFFu, false, kEf, 0, 0, 0, 0x44},
};
#undef E2C_FN
#undef E2C_CALLS
#undef E2C_N

enum : unsigned {
    k3ERun, k3EStart, k3EBolts, k3ESubWait, k3ESubSound, k3EDrawBolt, k3EBoltGlow,
    k3FRun, k3FDraw, k3FDrawBeam, k3FDrawCap, k3FDrawBand,
    k40Run, k40Start, k40Grow, k40ShardsClear, k40ShardsStep, k40ShardDraw, k40ShardSpawn, k40ShardFind, k40DrawDisc,
    k42Run, k42Glow,
    k43Run, k43Start, k43Gather, k43Wait, k43Fade, k43Setup, k43AddSpark, k43SparksRun, k43SparkDraw,
    k6BRun, k6BCapture, k6BStore, k6BScatter, k6BArm, k6BFall,
    k44Run, k44Start, k44FadeIn, k44Sparks, k44Widen, k44Follow, k44FadeOut, k44RingProject, k44RingDraw, k44RingCone,
    k44SparksClear, k44SparksStep, k44SparkTrail, k44SparkEmit, k44SparkFind,
    kCount
};
static_assert(kCount == sizeof kAll / sizeof kAll[0], "one enum entry a clone, in order");

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
unsigned char* P(U a) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a)); }
template <typename... T> U PickOf(T... v) {
    const U values[] = {static_cast<U>(v)...};
    return sh::Pick(values, sizeof...(v));
}
void SetU(U a, U v) { SetLong(P(a), static_cast<std::int32_t>(v)); }
void SetFloat(unsigned char* at, float f) { std::memcpy(at, &f, sizeof f); }

// --- the effects -----------------------------------------------------------------------

// The caller's stack from just below this frame to its base (the TIB's
// StackBase): where the originals' and ours' locals are.
bool OnStack(const void* p, unsigned n) {
    std::uint32_t base;
    __asm__("movl %%fs:4, %0" : "=r"(base));
    const auto here = static_cast<U>(reinterpret_cast<std::uintptr_t>(&base));
    const auto at = static_cast<U>(reinterpret_cast<std::uintptr_t>(p));
    return at > here && at + n > at && at + n <= base;
}
bool Writable(U at, unsigned n) { return sh::InRegions(P(at), n) || OnStack(P(at), n); }
// A float with a random mantissa and an exponent 2^-17..2^18 (no NaN, no
// infinity): what the projection's screen words and depth are.
void FillFloat(unsigned char* at) {
    const U n = sh::Noise();
    const U bits = (n & 0x807FFFFFu) | ((0x6Eu + (sh::Noise() % 0x24u)) << 23);
    std::memcpy(at, &bits, 4);
}
// An `out` pointer that is not a local of the caller's (a record's cell, the
// packet) is compared by its value; a local's address differs between the copy
// and ours, so it is not.
void NoteOut(U out) {
    if (!OnStack(P(out), 4)) sh::Note(out);
}
// EffectGte_ProjectPoint: out[0..2] the projected x, y and depth (floats).
U FxProjectPoint(const U* a, U answer) {
    NoteOut(a[1]);
    if (Writable(a[1], 12))
        for (unsigned i = 0; i < 3; ++i) FillFloat(P(a[1] + 4 * i));
    return answer;
}
// EffectGte_ProjectSize: out[0], out[1] (s16); small positive half the time,
// as a radius is.
U FxProjectSize(const U* a, U answer) {
    NoteOut(a[2]);
    if (Writable(a[2], 4)) {
        const U n = sh::Noise();
        const U v = (n & 1) ? (n >> 1) : ((n >> 1) & 0x003F003Fu);
        std::memcpy(P(a[2]), &v, 4);
    }
    return answer;
}
// Gte_VectorNormal (in place in EffectKind3E_BoltGlow): out[0..2] a unit
// vector's s32 components, 4096 scale and noise.
U FxVectorNormal(const U* a, U answer) {
    NoteOut(a[1]);
    if (Writable(a[1], 12))
        for (unsigned i = 0; i < 3; ++i) {
            const U n = sh::Noise();
            const U v = (n & 1) ? n : static_cast<U>(static_cast<std::int32_t>((n >> 1) % 0x2001u) - 0x1000);
            std::memcpy(P(a[1] + 4 * i), &v, 4);
        }
    return answer;
}
// 0x59E930 (rect, to): Gfx_VramShadow's w x h rectangle copied to `to` - filled
// here (its first w * h words, at most the pixels the fuzz seeds).
U FxStoreImage(const U* a, U answer) {
    const std::int32_t w = static_cast<std::int16_t>(Word(P(a[0]) + 4));
    const std::int32_t h = static_cast<std::int16_t>(Word(P(a[0]) + 6));
    if (w > 0 && h > 0 && w * h <= 0x100 && Writable(a[1], static_cast<unsigned>(2 * w * h)))
        sh::FillBytes(P(a[1]), static_cast<unsigned>(2 * w * h));
    return answer;
}
// Sprite_UpdateScreen draws Sprite_Current: its address and the record logged.
U FxUpdateScreen(const U*, U answer) {
    unsigned char* const s = Sprite_Current;
    sh::Note(Key(s));
    if (sh::InRegions(s, 0x80)) sh::NoteBytes(s, 0x80);
    return answer;
}
// EffectKind40_ShardDraw reads the shard cursor: logged.
U FxShardDraw(const U*, U answer) {
    sh::Note(static_cast<U>(Long(P(at::kShardCursor))));
    return answer;
}
// EffectKind40_ShardFind / EffectKind44_SparkFind: 0 a third of the time, else
// one of the records (a caller writes through it).
U FxShardFind(const U*, U answer) {
    if (answer % 3 == 0) return 0;
    return at::kShards + at::kShardStride * ((answer >> 2) % at::kShardCount);
}
U FxSparkFind(const U*, U answer) {
    if (answer % 3 == 0) return 0;
    return at::kSparks44 + at::kSparkStride44 * ((answer >> 2) % at::kSparkCount44);
}

#define E2C_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
constexpr sh::Answer kG = sh::Answer::kGarbage;
constexpr U kW = 0xFFFFFFFFu, k16 = 0xFFFFu;
const sh::Callee kCallees[] = {
    // the group's own called directly, by name (docs/effect_2c.md section 4):
    // record and packet pointers logged by value and their points hashed; a
    // caller's local (a projection) hashed only; radii and angles read as words
    {E2C_OURS(EffectKind3E_DrawBolt), 2, {kW, kW}, kG, 0, 0, {12, 12}},
    {E2C_OURS(EffectKind3E_BoltGlow), 2, {kW, kW}, kG, 0, 0, {12, 12}},
    {E2C_OURS(EffectKind3F_DrawBeam), 2, {kW, kW}, kG, 0, 0, {12, 12}},
    {E2C_OURS(EffectKind3F_DrawCap), 3, {0, k16, k16}, kG, 0, 0, {12}, nullptr, nullptr, true},
    {E2C_OURS(EffectKind3F_DrawBand), 6, {0, k16, k16, 0, k16, k16}, kG, 0, 0, {12, 0, 0, 12}, nullptr, nullptr, true},
    {E2C_OURS(EffectKind40_ShardsClear), 0, {}, kG, 0, 0},
    {E2C_OURS(EffectKind40_ShardsStep), 0, {}, kG, 0, 0},
    {E2C_OURS(EffectKind40_ShardDraw), 0, {}, kG, 0, 0, {}, &FxShardDraw},
    {E2C_OURS(EffectKind40_ShardSpawn), 1, {kW}, kG, 0, 0},
    {E2C_OURS(EffectKind40_ShardFind), 0, {}, kG, 0, 0, {}, &FxShardFind},
    {E2C_OURS(EffectKind40_DrawDisc), 1, {k16}, kG, 0, 0},
    {E2C_OURS(EffectKind43_Setup), 0, {}, kG, 0, 0},
    {E2C_OURS(EffectKind43_AddSpark), 0, {}, kG, 0, 0},
    {E2C_OURS(EffectKind43_SparksRun), 0, {}, kG, 0, 0},
    {E2C_OURS(EffectKind43_SparkDraw), 1, {kW}, kG, 0, 0},
    {E2C_OURS(EffectKind44_RingProject), 1, {kW}, kG, 0, 0},
    {E2C_OURS(EffectKind44_RingDraw), 1, {kW}, kG, 0, 0},
    {E2C_OURS(EffectKind44_RingCone), 1, {kW}, kG, 0, 0},
    {E2C_OURS(EffectKind44_SparksClear), 0, {}, kG, 0, 0},
    {E2C_OURS(EffectKind44_SparksStep), 0, {}, kG, 0, 0},
    {E2C_OURS(EffectKind44_SparkTrail), 1, {kW}, kG, 0, 0},
    {E2C_OURS(EffectKind44_SparkEmit), 0, {}, kG, 0, 0},
    {E2C_OURS(EffectKind44_SparkFind), 0, {}, kG, 0, 0, {}, &FxSparkFind},
    // standard rows re-listed: a point handed on the stack hashed, not logged;
    // an out logged when it is not a local, and filled
    {E2C_OURS(EffectGte_ProjectPoint), 2, {0, 0}, kG, 0, 0, {12, 0}, &FxProjectPoint, nullptr, true},
    {E2C_OURS(EffectGte_ProjectSize), 3, {0, 0, 0}, kG, 0, 0, {12, 4, 0}, &FxProjectSize, nullptr, true},
    {E2C_OURS(Gte_VectorNormal), 2, {0, 0}, kG, 0, 0, {12, 0}, &FxVectorNormal, nullptr, true},
    {E2C_OURS(Sprite_UpdateScreen), 0, {}, kG, 0, 0, {}, &FxUpdateScreen, nullptr, true},
    // nobody's, by address: the winding test reads 8 of each of its three
    // points (the effect-standard row writes 8 at each: this caller's are the
    // ring's projections, which the real one only reads); the read-back's
    // rectangle hashed and its target logged and filled
    {"0x4941B0", at::kWinding, at::kWinding, 3, {0, 0, 0}, kG, 0, 0, {8, 8, 8}, nullptr, nullptr, true},
    {"0x59E930", at::kStoreImage, at::kStoreImage, 2, {0, kW}, kG, 0, 0, {8, 0}, &FxStoreImage, nullptr, true},
};
#undef E2C_OURS

// The eight state tables the dispatchers jump through (and kind 0x3E's
// sub-state table it calls through), read in place; each table's length to the
// next table a dispatcher names (none bounded by a compare).
const sh::DataTable kTables[] = {
    {0x654468, 2}, {0x654480, 2}, {0x654488, 2}, {0x654490, 3}, {0x65449C, 2}, {0x6544B0, 4}, {0x6544C0, 5}, {0x6544D4, 6},
};
const std::uint8_t kKinds[] = {0x3E, 0x3F, 0x40, 0x42, 0x43, 0x44, 0x6B};

// Beyond the standard effect regions: kind 0x43's 256 spark records past the
// shards' 0x644 (to 0x92DB80), kind 0x6B's first 80 particles, the kinds' cells
// 0x67610C..0x6761C3, and kind 0x6B's rectangle words (seeded small: the
// original's 0x50 x 0x48 would make 5,760 particles, past the state the
// harness holds).
const sh::Region kRegions[] = {
    {at::kShards + 0x644, at::kSparkCount43 * at::kSparkStride43 - 0x644},
    {at::kPixels, 80 * at::kPixelStride},
    {at::kShardCursor, 0xB8},
    {at::kFrameX, 8},
};
constexpr unsigned kPixelsSeeded = 80;

// --- the seed ------------------------------------------------------------------------

float Moderate(float lo, float hi) {
    const U n = sh::Next();
    return lo + (hi - lo) * static_cast<float>(n % 10000u) / 10000.0f;
}
// A float that is finite, sometimes 0, negative, tiny or huge.
void SeedFloat(unsigned char* at, float lo, float hi) {
    const U n = sh::Next() % 8;
    if (n == 0) {
        SetFloat(at, 0.0f);
    } else if (n == 1) {
        const U bits = (sh::Next() & 0x807FFFFFu) | ((0x60u + sh::Next() % 0x3Fu) << 23);
        std::memcpy(at, &bits, 4);
    } else {
        SetFloat(at, Moderate(lo, hi));
    }
}
void SeedPoint(unsigned char* at) {
    SeedFloat(at, -50.0f, 370.0f);
    SeedFloat(at + 4, -50.0f, 290.0f);
    SeedFloat(at + 8, 0.0f, 4.0f);
}

// Kind 0x40's disc: the centre at 0x92C280 and its 32 rim points round it,
// anticlockwise (every edge's cross product with the centre positive), or
// clockwise, or all at one point (every product 0), or noise.
void SeedDisc() {
    const float cx = Moderate(60.0f, 260.0f), cy = Moderate(40.0f, 200.0f);
    SetFloat(P(at::kDiscCentre), cx);
    SetFloat(P(at::kDiscCentre + 4), cy);
    SeedFloat(P(at::kDiscCentre + 8), 0.0f, 4.0f);
    const U how = sh::Next() % 5;
    const float r = Moderate(20.0f, 140.0f);
    for (unsigned i = 0; i < 32; ++i) {
        unsigned char* const v = P(at::kDiscRim + 12 * i);
        const float t = 6.2831853f * static_cast<float>(i) / 32.0f;
        if (how <= 1) {
            SetFloat(v, cx + r * std::cos(t));
            SetFloat(v + 4, cy + (how == 0 ? r : -r) * std::sin(t));
        } else if (how == 2) {
            SetFloat(v, cx);
            SetFloat(v + 4, cy);
        } else if (how == 3) {
            SeedPoint(v);
        }
        if (how != 4) SeedFloat(v + 8, 0.0f, 4.0f);
    }
}
// Every shard record: in use or not, its height +8 never 0 (the original
// divides by it; a disturbed cursor may land on any of them), its offsets
// inside the rim or not.
void SeedShards() {
    for (unsigned i = 0; i < at::kShardCount; ++i) {
        unsigned char* const p = P(at::kShards + at::kShardStride * i);
        if (sh::Next() % 3 == 0) p[0] = 0;
        U h = PickOf(0x280, 0xA0, 0x9F, 0x81, 0x80, 0x7F, 0x60, 0x20, 1, 0xFFFF, 0x8000, 0x7FFF, sh::Next());
        if ((h & 0xFFFF) == 0) h = 0x80;
        SetWord(p + 8, h);
        if (sh::Half()) {
            SetWord(p + 4, (sh::Next() & 0xFF) - 0x80);
            SetWord(p + 6, (sh::Next() & 0xFF) - 0x80);
        }
    }
}
// In-use bytes of `count` records `stride` apart: all taken a quarter of the
// time, else about half.
void SeedInUse(U base, U stride, unsigned count) {
    const bool full = sh::Next() % 4 == 0;
    for (unsigned i = 0; i < count; ++i) {
        unsigned char* const p = P(base + stride * i);
        p[0] = full ? static_cast<unsigned char>(sh::Next() | 1) : (sh::Half() ? 0 : static_cast<unsigned char>(sh::Next() | 1));
    }
}
// Kind 0x43's 256 spark records: in use or not, every phase (and past the
// five), counts at 1, sides 0 and 1.
void SeedSparks43() {
    for (unsigned i = 0; i < at::kSparkCount43; ++i) {
        unsigned char* const r = P(at::kShards + at::kSparkStride43 * i);
        r[0] = sh::Half() ? 0 : static_cast<unsigned char>(sh::Next() | 1);
        r[1] = static_cast<unsigned char>(PickOf(1, 1, 2, 0, 0x20, sh::Next()));
        r[2] = static_cast<unsigned char>(PickOf(0, 1, 2, 3, 4, 5, 0xFF, sh::Next()));
        r[3] = static_cast<unsigned char>(PickOf(0, 1, sh::Next()));
    }
}
// Kind 0x6B's rectangle: x and y small, w * h at most the 80 particles seeded
// (w up to the original's 0x50 with one row), with 0 and negatives.
void SeedFrame() {
    SetWord(P(at::kFrameX), PickOf(0x30, 0, 0xFFF0, sh::Next() & 0x7F));
    SetWord(P(at::kFrameY), PickOf(0x36, 0, 0xFFF0, sh::Next() & 0x7F));
    U w = PickOf(0, 0xFFFF, 0x8000, 1, 2, 3, 5, 8, 10, 0x50, 0x4F, 0x51, 1 + sh::Next() % 16);
    U h;
    if (w == 0x50 || w == 0x4F || w == 0x51) {
        h = PickOf(0, 1, 0xFFFF);
        if (w == 0x51) w = 0x50;
    } else if (static_cast<std::int16_t>(w) > 0) {
        h = PickOf(0, 0xFFFF, 0x8000, 1 + sh::Next() % (kPixelsSeeded / w));
    } else {
        h = PickOf(0, 1, 3, 0xFFFF);
    }
    SetWord(P(at::kFrameW), w);
    SetWord(P(at::kFrameH), h);
}
// Kind 0x6B's heights (80 s16) round the particles' y range.
void SeedHeights() {
    for (unsigned i = 0; i < at::kHeightCount; ++i)
        SetWord(P(at::kHeights + 2 * i), PickOf(0, 0x80, 0x100, 0x7FFF, 0x8000, sh::Next() & 0x3FF, sh::Next()));
}
// Kind 0x6B's particles: every flags phase, counts at 1, columns inside the
// 80 heights (a disturbed count may reach any of the 80).
void SeedPixels() {
    for (unsigned i = 0; i < kPixelsSeeded; ++i) {
        unsigned char* const p = P(at::kPixels + at::kPixelStride * i);
        p[0] = static_cast<unsigned char>(PickOf(0, 1, 3, 0x11, 0x13, 0x10, 0x21, 0xF1, sh::Next()));
        p[1] = static_cast<unsigned char>(PickOf(1, 2, 0, sh::Next()));
        SetWord(p + 0x12, sh::Next() % at::kHeightCount);
        SetWord(p + 0x10, PickOf(0, 2, 0xFFFE, 0x7FFE, sh::Next() & 0x3F, sh::Next()));
        SeedFloat(p + 8, -200.0f, 0x1000 * 1.0f);
        SeedFloat(p + 4, -200.0f, 0x1400 * 1.0f);
    }
}
// Kind 0x44's ring: the cell on a record inside the regions; its points
// projected somewhere on or off the screen.
U RingAt() { return at::kShards + 0x40 * (sh::Next() % 8); }
void SeedRing(U ring) {
    unsigned char* const r = P(ring);
    SetWord(r + 0x20, PickOf(0x150, 0x151, 0x152, 0x180, 0x181, 0x7FF0, 0, sh::Next()));
    for (unsigned i = 0; i < 18; ++i) SeedPoint(r + 0x24 + 12 * i);
}

void Seed(unsigned k) {
    unsigned char* const s = Sprite_Current;
    s[9] = static_cast<unsigned char>(PickOf(1, 1, 2, 0, 0x10, sh::Next()));
    switch (k) {
    case k3ESubWait:
    case k3ESubSound:
        if (sh::Half()) Field_Request = 5;
        break;
    case k3EBoltGlow:
    case k3FDrawCap:
    case k3FDrawBand:
        // the screen points handed in: finite most of the time
        for (unsigned i = 0; i < 4; ++i)
            if (sh::Next() % 4 != 0) SeedPoint(sh::Scratch(i));
        break;
    case k40Grow:
        if (sh::Next() % 3 == 0) sh::Mem(at::kCounter)[0] = 0x19;
        SetWord(s + 0x2E, PickOf(0x3BF, 0x3C0, 0x3C1, 0x3FF, 0x400, 0x401, 0x7FC0, 0x7FFF, 0xFFC0, sh::Next()));
        break;
    case k40ShardsStep:
    case k40ShardDraw:
        SeedShards();
        SeedDisc();
        SetU(at::kShardCursor, at::kShards + at::kShardStride * (sh::Next() % at::kShardCount));
        break;
    case k40ShardFind:
        SeedInUse(at::kShards, at::kShardStride, at::kShardCount);
        break;
    case k43Gather:
        SetWord(s + 0x32, PickOf(1, 1, 2, 0, sh::Next()));
        break;
    case k43Wait:
        if (sh::Next() % 3 == 0) sh::Mem(at::kCounter)[0] = 0x11;
        break;
    case k43AddSpark:
        SeedInUse(at::kShards, at::kSparkStride43, at::kSparkCount43);
        break;
    case k43SparksRun:
        SeedSparks43();
        s[6] = static_cast<unsigned char>(PickOf(0, 1, 2, 2, 3, sh::Next()));
        break;
    case k6BStore:
    case k6BScatter:
        SeedFrame();
        SeedHeights();
        break;
    case k6BArm:
    case k6BFall:
        SetWord(P(at::kPixelCount), PickOf(0, 1, 2, 79, 80, sh::Next() % (kPixelsSeeded + 1)));
        SeedPixels();
        SeedHeights();
        break;
    case k44FadeIn:
    case k44Sparks:
    case k44Widen:
    case k44Follow:
    case k44FadeOut: {
        const U ring = RingAt();
        SetU(at::kRingCell, ring);
        SeedRing(ring);
        break;
    }
    case k44SparksStep:
    case k44SparkFind:
        SeedInUse(at::kSparks44, at::kSparkStride44, at::kSparkCount44);
        for (unsigned i = 0; i < at::kSparkCount44; ++i)
            P(at::kSparks44 + at::kSparkStride44 * i)[1] = static_cast<unsigned char>(PickOf(1, 2, 0, sh::Next()));
        break;
    case k44SparkEmit:
        if (sh::Half()) Frame_Counter &= ~3u;
        break;
    default: break;
    }
}

void Args(unsigned k, U* a) {
    switch (k) {
    case k3FDrawCap:
        a[1] = (sh::Next() & 0xFFFF0000u) | PickOf(0, 1, 0x10, 0x40, 0x8000, 0xFFFF, sh::Next() & 0xFFFF);
        break;
    case k3FDrawBand:
        a[1] = (sh::Next() & 0xFFFF0000u) | PickOf(0, 1, 0x10, 0x40, 0x8000, 0xFFFF, sh::Next() & 0xFFFF);
        a[4] = (sh::Next() & 0xFFFF0000u) | PickOf(0, 1, 0x10, 0x40, 0x8000, 0xFFFF, sh::Next() & 0xFFFF);
        break;
    case k40ShardSpawn: a[0] = at::kShards + at::kShardStride * (sh::Next() % at::kShardCount); break;
    case k40DrawDisc: a[0] = (sh::Next() & 0xFFFF0000u) | PickOf(0, 0x40, 0x3C0, 0x400, 0x8000, 0xFFFF, sh::Next() & 0xFFFF); break;
    case k43SparkDraw: a[0] = at::kShards + at::kSparkStride43 * (sh::Next() % at::kSparkCount43); break;
    case k44RingProject:
    case k44RingDraw:
    case k44RingCone: a[0] = RingAt(); break;
    case k44SparkTrail: a[0] = at::kSparks44 + at::kSparkStride44 * (sh::Next() % at::kSparkCount44); break;
    default: break;
    }
}
// The rings' points want seeding where the ring is handed in: the seed runs
// before the arguments, so the three ring helpers' records are seeded whole.
void SeedRingArgs(unsigned k) {
    if (k == k44RingProject || k == k44RingDraw || k == k44RingCone)
        for (unsigned i = 0; i < 8; ++i) SeedRing(at::kShards + 0x40 * i);
}

// What the states read again after a call, moved by the group's case of the
// harness's disturbance (from its hash only): the two cursors, the ring cell,
// the counter byte, +9, the particle count, the words +0x2E / +0x32.
void Disturb(U h) {
    const U v = h >> 8;
    unsigned char* const s = Sprite_Current;
    switch (h % 8) {
    case 0: SetU(at::kShardCursor, at::kShards + at::kShardStride * (v % at::kShardCount)); break;
    case 1: SetU(at::kSparkCursor, at::kSparks44 + at::kSparkStride44 * (v % at::kSparkCount44)); break;
    case 2: SetU(at::kRingCell, at::kShards + 0x40 * (v % 8)); break;
    case 3: sh::Mem(at::kCounter)[0] = static_cast<unsigned char>((v & 1) ? ((v & 2) ? 0x19 : 0x11) : v >> 2); break;
    case 4: s[9] = static_cast<unsigned char>((v & 1) ? 1 : v >> 1); break;
    case 5: SetWord(P(at::kPixelCount), v % (kPixelsSeeded + 1)); break;
    case 6: SetWord(s + 0x32, (v & 1) ? 1u : v >> 1); break;
    case 7: SetWord(s + 0x2E, (v & 1) ? 0x3C0u : v >> 1); break;
    default: break;
    }
}

// BOF3X_E2C_ONLY: a comma-separated list; a clone runs when its name equals
// one of them (a control's run: several controls, each in a function of its
// own, share one build, and each is judged by its own clone's count).
bool Chosen(const char* name, const char* only) {
    if (!only || !*only) return true;
    const std::size_t n = std::strlen(name);
    for (const char* p = only; *p;) {
        const char* const end = std::strchr(p, ',');
        const std::size_t len = end ? static_cast<std::size_t>(end - p) : std::strlen(p);
        if (len == n && std::strncmp(p, name, n) == 0) return true;
        if (!end) break;
        p = end + 1;
    }
    return false;
}

}  // namespace

void SelfTest() {
    static sh::Clone chosen[kCount];
    static unsigned index[kCount];
    const char* const only = std::getenv("BOF3X_E2C_ONLY");
    unsigned n = 0;
    for (unsigned k = 0; k < kCount; ++k)
        if (Chosen(kAll[k].name, only)) {
            index[n] = k;
            chosen[n++] = kAll[k];
        }
    if (n == 0) bof3::Fatal("effect_2c: BOF3X_E2C_ONLY=%s names no clone", only);
    static unsigned* s_index = index;
    sh::Group g = {"effect_2c", chosen, n, kCallees, sizeof kCallees / sizeof kCallees[0], kTables,
                   sizeof kTables / sizeof kTables[0], kRegions, sizeof kRegions / sizeof kRegions[0],
                   [](unsigned k) {
                       Seed(s_index[k]);
                       SeedRingArgs(s_index[k]);
                   },
                   &Disturb, 6000};
    g.args = [](unsigned k, U* a) { Args(s_index[k], a); };
    g.effect = true;
    g.kinds = kKinds;
    g.n_kinds = sizeof kKinds;
    sh::Run(g);
}

}  // namespace effect_2c

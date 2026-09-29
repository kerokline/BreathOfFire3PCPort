// BOF3X_SHADOW=effect_2e: group E2E's 51 functions through the scenario harness
// in effect mode (scenario_harness.h, docs/scenario_harness.md section 8), once
// at start-up. docs/effect_2e.md section 4. BOF3X_E2E_ONLY=<name> runs the
// clones whose name contains it (the controls' speed-up).
//
// The clone table is tools/band_rows.py --group E2E --byte-tables --clones
// --harness scenario (2026-09-29), each extent read again to its last
// instruction (capstone) and the names given. Shapes: every dispatcher and
// state kEffect (Sprite_Current one of the 20 Effect_Objects records, +5 the
// kind, a dispatcher's +1 or +2 below its table's length); the helpers kCall.
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_2e.h"
#include "game/effect_2e_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace effect_2e {
namespace {

namespace sh = scenario_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using U = std::uint32_t;

// tools/band_rows.py --group E2E --clones, 2026-09-29.
constexpr sh::CallSite kCalls4789D0[] = {{0xD, 0x5720C0}};
constexpr sh::CallSite kCalls478A40[] = {{0x3F, 0x479EE0}};
constexpr sh::CallSite kCalls478AB0[] = {{0x10, 0x479EE0}};
constexpr sh::CallSite kCalls478AF0[] = {{0x17, 0x479EE0}, {0x2E, 0x589840}};
constexpr sh::CallSite kCalls478B50[] = {{0xD, 0x5720C0}, {0x5F, 0x4799C0}, {0x7D, 0x587740}};
constexpr sh::CallSite kCalls478BE0[] = {{0x6, 0x479B70}, {0x3F, 0x589840}};
constexpr sh::CallSite kCalls478C50[] = {{0x29, 0x5720C0}, {0x40, 0x4790C0}};
constexpr sh::CallSite kCalls478CC0[] = {{0x6, 0x479B70}, {0x1A, 0x47CF20}, {0x30, 0x479160}, {0x38, 0x479260}};
constexpr sh::CallSite kCalls478D20[] = {{0x0, 0x479260}, {0x9, 0x589840}};
constexpr sh::CallSite kCalls478D50[] = {{0x29, 0x5720C0}, {0x7B, 0x4799C0}};
constexpr sh::CallSite kCalls478DF0[] = {{0x6, 0x479B70}, {0x40, 0x589840}};
constexpr sh::CallSite kCalls478E60[] = {{0x29, 0x5720C0}, {0x40, 0x47A560}};
constexpr sh::CallSite kCalls478EC0[] = {{0x0, 0x47A780}, {0x1D, 0x589840}};
constexpr sh::CallSite kCalls478F10[] = {{0x14, 0x4794D0}, {0x31, 0x587740}};
constexpr sh::CallSite kCalls478F60[] = {{0x5, 0x4794D0}, {0xF, 0x4796B0}};
constexpr sh::CallSite kCalls478FD0[] = {{0x5, 0x4794D0}, {0xF, 0x4796B0}, {0x37, 0x589840}, {0x41, 0x587740}};
constexpr sh::CallSite kCalls479030[] = {{0x5, 0x4794D0}, {0xF, 0x4796B0}};
constexpr sh::CallSite kCalls479080[] = {{0x5, 0x4794D0}, {0xF, 0x4796B0}};
constexpr sh::CallSite kCalls47A560[] = {{0x58, 0x5A7A00}, {0x60, 0x5A7A50}, {0x6F, 0x5A7A00}, {0x77, 0x5A7A00},
                                         {0x87, 0x5A7A50}, {0x1A2, 0x5A8B60}, {0x1E7, 0x5A7A90}};
constexpr sh::CallSite kCalls47A780[] = {{0x6, 0x494060}, {0x58, 0x494110}, {0x76, 0x5A79A0}, {0x8E, 0x5A77C0},
                                         {0x97, 0x461E50}, {0xB0, 0x5A7610}, {0xB8, 0x5A7780}, {0x1B3, 0x461E50}};
constexpr sh::CallSite kCalls47A970[] = {{0x37, 0x47AC80}};
constexpr sh::CallSite kCalls47A9C0[] = {{0x0, 0x5A7B90}, {0x46, 0x47ACC0}, {0x4B, 0x47ADC0},
                                         {0x61, 0x47AF10}, {0x69, 0x5A7BC0}, {0x86, 0x589840}};
constexpr sh::CallSite kCalls47AA70[] = {{0x28, 0x5720C0}, {0x45, 0x5A7B90}, {0x55, 0x47B070}, {0x63, 0x5A7BC0}};
constexpr sh::CallSite kCalls47AAF0[] = {{0x12, 0x5A79A0}, {0x2A, 0x5A77C0}, {0x33, 0x461E50}, {0x3B, 0x5A7B90},
                                         {0x40, 0x494060}, {0x53, 0x485030}, {0x7C, 0x5A7BC0}, {0x9D, 0x589840}};
constexpr sh::CallSite kCalls47ABC0[] = {{0x26, 0x5720C0}};
constexpr sh::CallSite kCalls47AC20[] = {{0x1B, 0x587740}, {0x3A, 0x47B180}, {0x58, 0x589840}};
constexpr sh::CallSite kCalls47ACC0[] = {{0x49, 0x5A7A00}, {0x9E, 0x5A7A50}, {0xAE, 0x5A7A00}};
constexpr sh::CallSite kCalls47ADC0[] = {{0x14, 0x5A79A0}, {0x2C, 0x5A77C0}, {0x35, 0x461E50}, {0x3A, 0x494060},
                                         {0x49, 0x494110}, {0x58, 0x494110}, {0x78, 0x5A7610}, {0x80, 0x5A7780},
                                         {0xD1, 0x494110}, {0xDC, 0x494110}, {0x12D, 0x461E50}};
constexpr sh::CallSite kCalls47AF10[] = {{0x11, 0x494110}, {0x2B, 0x4941E0}, {0x55, 0x5A75F0}, {0x5D, 0x5A7780},
                                         {0x85, 0x5A7A50}, {0xA4, 0x5A7A00}, {0xC5, 0x5A7A50}, {0xE0, 0x5A7A00},
                                         {0x132, 0x461E50}};
constexpr sh::CallSite kCalls47B070[] = {{0x2D, 0x5B93D2}, {0x3A, 0x5B93D2}, {0x47, 0x5B93D2}, {0x56, 0x5A7A50},
                                         {0x61, 0x5A7A00}, {0x72, 0x5A7A50}, {0x7D, 0x5A7A00}, {0x91, 0x494180},
                                         {0x9F, 0x5A7F10}, {0xAF, 0x5A7F80}, {0xBD, 0x5A7FF0}, {0xCC, 0x5A7C70},
                                         {0xDB, 0x5A7C70}, {0xE3, 0x5B93D2}};
constexpr sh::CallSite kCalls47B180[] = {{0x2A, 0x5B93D2}, {0x46, 0x5B93D2}, {0x62, 0x5B93D2}, {0x7E, 0x5B93D2},
                                         {0x9A, 0x5A7B90}, {0x9F, 0x494060}, {0xAB, 0x5A7650}, {0xB3, 0x5A7780},
                                         {0xC2, 0x494110}, {0xE6, 0x494110}, {0x10F, 0x461E50}, {0x117, 0x5A7BC0}};
constexpr sh::CallSite kCalls47B2D0[] = {{0x6E, 0x587740}};
constexpr sh::CallSite kCalls47B350[] = {{0x34, 0x47B3F0}};
constexpr sh::CallSite kCalls47B3B0[] = {{0x19, 0x589840}, {0x30, 0x47B3F0}};
constexpr sh::CallSite kCalls47B3F0[] = {{0x7, 0x494060}, {0x38, 0x5A7A50}, {0x47, 0x5A7A00}, {0x82, 0x494110},
                                         {0xA9, 0x494110}, {0xCE, 0x5A75D0}, {0xD6, 0x5A7780}, {0x11A, 0x5A7A50},
                                         {0x12D, 0x5A7A00}, {0x166, 0x494110}, {0x18C, 0x494110}, {0x1F5, 0x5A79A0},
                                         {0x205, 0x5A79E0}, {0x212, 0x461E50}};
constexpr sh::CallSite kCalls47B650[] = {{0x59, 0x5A8250}, {0x70, 0x5A9110}};
constexpr sh::CallSite kCalls47B6E0[] = {{0x12, 0x47B7D0}};
constexpr sh::CallSite kCalls47B720[] = {{0x1B, 0x47B7D0}, {0x33, 0x587740}};
constexpr sh::CallSite kCalls47B790[] = {{0x12, 0x47B7D0}};

#define E2E_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define E2E_CALLS(a) a, E2E_N(a)
#define E2E_FN(name) reinterpret_cast<const void*>(&::name)
constexpr sh::Shape kEf = sh::Shape::kEffect;
constexpr sh::Shape kCa = sh::Shape::kCall;
// {name, base, size, calls, imms, tables, ours, ret_mask, calm, shape, pointers, state_span, sub_span, kind}
const sh::Clone kAll[] = {
    {"EffectKind48_State7_Start", 0x4789D0, 0x6F, E2E_CALLS(kCalls4789D0), nullptr, 0, nullptr, 0, E2E_FN(EffectKind48_State7_Start), 0, false, kEf, 0, 0, 0, 0x48},
    {"EffectKind48_State7_Swell", 0x478A40, 0x6F, E2E_CALLS(kCalls478A40), nullptr, 0, nullptr, 0, E2E_FN(EffectKind48_State7_Swell), 0, false, kEf, 0, 0, 0, 0x48},
    {"EffectKind48_State7_Widen", 0x478AB0, 0x34, E2E_CALLS(kCalls478AB0), nullptr, 0, nullptr, 0, E2E_FN(EffectKind48_State7_Widen), 0, false, kEf, 0, 0, 0, 0x48},
    {"EffectKind48_State7_Lift", 0x478AF0, 0x34, E2E_CALLS(kCalls478AF0), nullptr, 0, nullptr, 0, E2E_FN(EffectKind48_State7_Lift), 0, false, kEf, 0, 0, 0, 0x48},
    {"EffectKind48_State8_Run", 0x478B30, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E2E_FN(EffectKind48_State8_Run), 0, false, kEf, 0, 0, 2, 0x48},
    {"EffectKind48_State8_Start", 0x478B50, 0x86, E2E_CALLS(kCalls478B50), nullptr, 0, nullptr, 0, E2E_FN(EffectKind48_State8_Start), 0, false, kEf, 0, 0, 0, 0x48},
    {"EffectKind48_State8_Turn", 0x478BE0, 0x45, E2E_CALLS(kCalls478BE0), nullptr, 0, nullptr, 0, E2E_FN(EffectKind48_State8_Turn), 0, false, kEf, 0, 0, 0, 0x48},
    {"EffectKind48_State9_Run", 0x478C30, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E2E_FN(EffectKind48_State9_Run), 0, false, kEf, 0, 0, 3, 0x48},
    {"EffectKind48_State9_Start", 0x478C50, 0x64, E2E_CALLS(kCalls478C50), nullptr, 0, nullptr, 0, E2E_FN(EffectKind48_State9_Start), 0, false, kEf, 0, 0, 0, 0x48},
    {"EffectKind48_State9_Emit", 0x478CC0, 0x59, E2E_CALLS(kCalls478CC0), nullptr, 0, nullptr, 0, E2E_FN(EffectKind48_State9_Emit), 0, false, kEf, 0, 0, 0, 0x48},
    {"EffectKind48_State9_Drain", 0x478D20, 0xF, E2E_CALLS(kCalls478D20), nullptr, 0, nullptr, 0, E2E_FN(EffectKind48_State9_Drain), 0, false, kEf, 0, 0, 0, 0x48},
    {"EffectKind48_State10_Run", 0x478D30, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E2E_FN(EffectKind48_State10_Run), 0, false, kEf, 0, 0, 2, 0x48},
    {"EffectKind48_State10_Start", 0x478D50, 0x98, E2E_CALLS(kCalls478D50), nullptr, 0, nullptr, 0, E2E_FN(EffectKind48_State10_Start), 0, false, kEf, 0, 0, 0, 0x48},
    {"EffectKind48_State10_Turn", 0x478DF0, 0x46, E2E_CALLS(kCalls478DF0), nullptr, 0, nullptr, 0, E2E_FN(EffectKind48_State10_Turn), 0, false, kEf, 0, 0, 0, 0x48},
    {"EffectKind48_State11_Run", 0x478E40, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E2E_FN(EffectKind48_State11_Run), 0, false, kEf, 0, 0, 2, 0x48},
    {"EffectKind48_State11_Start", 0x478E60, 0x5B, E2E_CALLS(kCalls478E60), nullptr, 0, nullptr, 0, E2E_FN(EffectKind48_State11_Start), 0, false, kEf, 0, 0, 0, 0x48},
    {"EffectKind48_State11_Draw", 0x478EC0, 0x23, E2E_CALLS(kCalls478EC0), nullptr, 0, nullptr, 0, E2E_FN(EffectKind48_State11_Draw), 0, false, kEf, 0, 0, 0, 0x48},
    {"EffectKind48_State12_Run", 0x478EF0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E2E_FN(EffectKind48_State12_Run), 0, false, kEf, 0, 0, 8, 0x48},
    {"EffectKind48_State12_Start", 0x478F10, 0x42, E2E_CALLS(kCalls478F10), nullptr, 0, nullptr, 0, E2E_FN(EffectKind48_State12_Start), 0, false, kEf, 0, 0, 0, 0x48},
    {"EffectKind48_State12_Travel", 0x478F60, 0x6D, E2E_CALLS(kCalls478F60), nullptr, 0, nullptr, 0, E2E_FN(EffectKind48_State12_Travel), 0, false, kEf, 0, 0, 0, 0x48},
    {"EffectKind48_State12_Hold", 0x478FD0, 0x5B, E2E_CALLS(kCalls478FD0), nullptr, 0, nullptr, 0, E2E_FN(EffectKind48_State12_Hold), 0, false, kEf, 0, 0, 0, 0x48},
    {"EffectKind48_State12_Grow", 0x479030, 0x45, E2E_CALLS(kCalls479030), nullptr, 0, nullptr, 0, E2E_FN(EffectKind48_State12_Grow), 0, false, kEf, 0, 0, 0, 0x48},
    {"EffectKind48_State12_Shrink", 0x479080, 0x3C, E2E_CALLS(kCalls479080), nullptr, 0, nullptr, 0, E2E_FN(EffectKind48_State12_Shrink), 0, false, kEf, 0, 0, 0, 0x48},
    {"EffectAngle_Mean", 0x479970, 0x44, nullptr, 0, nullptr, 0, nullptr, 0, E2E_FN(EffectAngle_Mean), 0xFFFFFFFFu, false, kCa, 0, 0, 0, -1},
    {"EffectSphere_Build", 0x47A560, 0x211, E2E_CALLS(kCalls47A560), nullptr, 0, nullptr, 0, E2E_FN(EffectSphere_Build), 0xFF, false, kCa, 0, 0, 0, 0x48},
    {"EffectSphere_Draw", 0x47A780, 0x1CE, E2E_CALLS(kCalls47A780), nullptr, 0, nullptr, 0, E2E_FN(EffectSphere_Draw), 0xFF, false, kCa, 0, 0, 0, 0x48},
    {"EffectKind4A_Run", 0x47A950, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E2E_FN(EffectKind4A_Run), 0, false, kEf, 0, 2, 0, 0x4A},
    {"EffectKind4A_Start", 0x47A970, 0x45, E2E_CALLS(kCalls47A970), nullptr, 0, nullptr, 0, E2E_FN(EffectKind4A_Start), 0, false, kEf, 0, 0, 0, 0x4A},
    {"EffectKind4A_Follow", 0x47A9C0, 0x8C, E2E_CALLS(kCalls47A9C0), nullptr, 0, nullptr, 0, E2E_FN(EffectKind4A_Follow), 0, false, kEf, 0, 0, 0, 0x4A},
    {"EffectKind4A_TrailInit", 0x47AC80, 0x3F, nullptr, 0, nullptr, 0, nullptr, 0, E2E_FN(EffectKind4A_TrailInit), 0, false, kCa, 0, 0, 0, 0x4A},
    {"EffectKind4A_TrailStep", 0x47ACC0, 0xF9, E2E_CALLS(kCalls47ACC0), nullptr, 0, nullptr, 0, E2E_FN(EffectKind4A_TrailStep), 0, false, kCa, 0, 0, 0, 0x4A},
    {"EffectKind4A_TrailDraw", 0x47ADC0, 0x144, E2E_CALLS(kCalls47ADC0), nullptr, 0, nullptr, 0, E2E_FN(EffectKind4A_TrailDraw), 0, false, kCa, 0, 0, 0, 0x4A},
    {"EffectKind4A_DrawGlow", 0x47AF10, 0x15F, E2E_CALLS(kCalls47AF10), nullptr, 0, nullptr, 0, E2E_FN(EffectKind4A_DrawGlow), 0, false, kCa, 0, 0, 0, 0x4A},
    {"EffectKind4B_Run", 0x47AA50, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E2E_FN(EffectKind4B_Run), 0, false, kEf, 0, 2, 0, 0x4B},
    {"EffectKind4B_Start", 0x47AA70, 0x7F, E2E_CALLS(kCalls47AA70), nullptr, 0, nullptr, 0, E2E_FN(EffectKind4B_Start), 0, false, kEf, 0, 0, 0, 0x4B},
    {"EffectKind4B_Scatter", 0x47AAF0, 0xA3, E2E_CALLS(kCalls47AAF0), nullptr, 0, nullptr, 0, E2E_FN(EffectKind4B_Scatter), 0, false, kEf, 0, 0, 0, 0x4B},
    {"EffectKind4B_DebrisInit", 0x47B070, 0x10A, E2E_CALLS(kCalls47B070), nullptr, 0, nullptr, 0, E2E_FN(EffectKind4B_DebrisInit), 0, false, kCa, 0, 0, 0, 0x4B},
    {"EffectKind4C_Run", 0x47ABA0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E2E_FN(EffectKind4C_Run), 0, false, kEf, 0, 2, 0, 0x4C},
    {"EffectKind4C_Start", 0x47ABC0, 0x52, E2E_CALLS(kCalls47ABC0), nullptr, 0, nullptr, 0, E2E_FN(EffectKind4C_Start), 0, false, kEf, 0, 0, 0, 0x4C},
    {"EffectKind4C_Crackle", 0x47AC20, 0x5E, E2E_CALLS(kCalls47AC20), nullptr, 0, nullptr, 0, E2E_FN(EffectKind4C_Crackle), 0, false, kEf, 0, 0, 0, 0x4C},
    {"EffectKind4C_DrawSpark", 0x47B180, 0x121, E2E_CALLS(kCalls47B180), nullptr, 0, nullptr, 0, E2E_FN(EffectKind4C_DrawSpark), 0, false, kCa, 0, 0, 0, 0x4C},
    {"EffectKind4D_Run", 0x47B2B0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E2E_FN(EffectKind4D_Run), 0, false, kEf, 0, 3, 0, 0x4D},
    {"EffectKind4D_Start", 0x47B2D0, 0x75, E2E_CALLS(kCalls47B2D0), nullptr, 0, nullptr, 0, E2E_FN(EffectKind4D_Start), 0, false, kEf, 0, 0, 0, 0x4D},
    {"EffectKind4D_Rise", 0x47B350, 0x5F, E2E_CALLS(kCalls47B350), nullptr, 0, nullptr, 0, E2E_FN(EffectKind4D_Rise), 0, false, kEf, 0, 0, 0, 0x4D},
    {"EffectKind4D_Fade", 0x47B3B0, 0x35, E2E_CALLS(kCalls47B3B0), nullptr, 0, nullptr, 0, E2E_FN(EffectKind4D_Fade), 0, false, kEf, 0, 0, 0, 0x4D},
    {"EffectKind4D_DrawColumn", 0x47B3F0, 0x238, E2E_CALLS(kCalls47B3F0), nullptr, 0, nullptr, 0, E2E_FN(EffectKind4D_DrawColumn), 0, false, kCa, 0, 0, 0, 0x4D},
    {"EffectKind4E_Run", 0x47B630, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E2E_FN(EffectKind4E_Run), 0, false, kEf, 0, 5, 0, 0x4E},
    {"EffectKind4E_Start", 0x47B650, 0x8B, E2E_CALLS(kCalls47B650), nullptr, 0, nullptr, 0, E2E_FN(EffectKind4E_Start), 0, false, kEf, 0, 0, 0, 0x4E},
    {"EffectKind4E_Grow", 0x47B6E0, 0x40, E2E_CALLS(kCalls47B6E0), nullptr, 0, nullptr, 0, E2E_FN(EffectKind4E_Grow), 0, false, kEf, 0, 0, 0, 0x4E},
    {"EffectKind4E_Glow", 0x47B720, 0x61, E2E_CALLS(kCalls47B720), nullptr, 0, nullptr, 0, E2E_FN(EffectKind4E_Glow), 0, false, kEf, 0, 0, 0, 0x4E},
    {"EffectKind4E_Shrink", 0x47B790, 0x37, E2E_CALLS(kCalls47B790), nullptr, 0, nullptr, 0, E2E_FN(EffectKind4E_Shrink), 0, false, kEf, 0, 0, 0, 0x4E},
};
#undef E2E_FN
#undef E2E_CALLS
#undef E2E_N

enum : unsigned {
    k7Start, k7Swell, k7Widen, k7Lift, k8Run, k8Start, k8Turn, k9Run, k9Start, k9Emit, k9Drain, k10Run, k10Start,
    k10Turn, k11Run, k11Start, k11Draw, k12Run, k12Start, k12Travel, k12Hold, k12Grow, k12Shrink, kMean,
    kSphereBuild, kSphereDraw, k4ARun, k4AStart, k4AFollow, k4ATrailInit, k4ATrailStep, k4ATrailDraw, k4AGlow,
    k4BRun, k4BStart, k4BScatter, k4BDebris, k4CRun, k4CStart, k4CCrackle, k4CSpark, k4DRun, k4DStart, k4DRise,
    k4DFade, k4DColumn, k4ERun, k4EStart, k4EGrow, k4EGlow, k4EShrink, kCount
};
static_assert(kCount == sizeof kAll / sizeof kAll[0], "one enum entry a clone, in order");

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
unsigned char* P(U a) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a)); }
template <typename... T> U PickOf(T... v) {
    const U values[] = {static_cast<U>(v)...};
    return sh::Pick(values, sizeof...(v));
}

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
void Fill(U at, unsigned n) {
    if (Writable(at, n)) sh::FillBytes(P(at), n);
}
// A float with a random mantissa and an exponent 2^-17..2^18 (no NaN, no
// infinity): what the projections' screen words and depths are.
void FillFloat(U at) {
    if (!Writable(at, 4)) return;
    const U n = sh::Noise();
    const U bits = (n & 0x807FFFFFu) | ((0x6Eu + (sh::Noise() % 0x24u)) << 23);
    std::memcpy(P(at), &bits, 4);
}
// EffectGte_ProjectPoint: out[0..2] the screen x, y and depth (floats); every caller reads all three.
U FxProjectPoint(const U* a, U answer) {
    for (unsigned i = 0; i < 3; ++i) FillFloat(a[1] + 4 * i);
    return answer;
}
// EffectGte_ProjectSize: out's two s16 - the first a radius of the callers' range half the time.
U FxProjectSize(const U* a, U answer) {
    Fill(a[2], 4);
    if (Writable(a[2], 2) && sh::Noise() % 2 == 0) SetWord(P(a[2]), PickOf(0, 1, 0x7FFF, 0xFFFF, 0x8000, sh::Noise() % 0x100));
    return answer;
}
// Gte_RotTransPers: the screen point (two floats) at sxy, the depth cue word at p.
U FxRotTransPers(const U* a, U answer) {
    FillFloat(a[1]);
    FillFloat(a[1] + 4);
    Fill(a[2], 4);
    return answer;
}
// Gte_StoreDepthF: one float.
U FxStoreDepth(const U* a, U answer) {
    FillFloat(a[0]);
    return answer;
}
// 0x47CF20: a free spark record (one of the 8 of 0x1C at 0x92BF80) a third of
// the time none (eax 0) - the answer the state reads whole.
U FxSparkFree(const U*, U answer) {
    const U n = sh::Noise();
    if (n % 3 == 0) return 0;
    (void)answer;
    return at::kSparks + 0x1C * ((n >> 2) % 8);
}
// 0x479160: the spark record's 24 bytes written.
U FxSparkSet(const U* a, U answer) {
    Fill(a[0], 24);
    return answer;
}

#define E2E_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
constexpr sh::Answer kG = sh::Answer::kGarbage, kPh = sh::Answer::kPhase, kF = sh::Answer::kFlag;
constexpr U kW = 0xFFFFFFFFu, k16 = 0xFFFFu, k8 = 0xFFu;
const sh::Callee kCallees[] = {
    // the group's own called directly, by name (the void helpers log the state they ran with)
    {E2E_OURS(EffectSphere_Build), 0, {}, kPh, 0, 0},
    {E2E_OURS(EffectSphere_Draw), 0, {}, kPh, 0, 0},
    {E2E_OURS(EffectKind4A_TrailInit), 0, {}, kPh, 0, 0},
    {E2E_OURS(EffectKind4A_TrailStep), 0, {}, kPh, 0, 0},
    {E2E_OURS(EffectKind4A_TrailDraw), 0, {}, kPh, 0, 0},
    // point, size (its low word: cx), centre and rim (bl, the byte after it)
    {E2E_OURS(EffectKind4A_DrawGlow), 4, {kW, k16, k8, k8}, kG, 0, 0},
    {E2E_OURS(EffectKind4B_DebrisInit), 1, {kW}, kG, 0, 0},
    {E2E_OURS(EffectKind4C_DrawSpark), 0, {}, kPh, 0, 0},
    {E2E_OURS(EffectKind4D_DrawColumn), 0, {}, kPh, 0, 0},
    // other groups' of round thirteen, by address (docs/effect_2e.md section 6):
    // E3C's debris draw reads the record's 0x2C bytes; E2F's disc reads the
    // radius as an s16 (movsx word [esp + 0x38] at 0x47B808 - the callers push
    // a whole register, its upper half theirs), the rim as a byte (bl)
    {"0x485030 (E3C)", at::kDebrisDraw, at::kDebrisDraw, 1, {kW}, kG, 0, 0, {0x2C}, nullptr, nullptr, true},
    {"0x47B7D0 (E2F)", at::kDiscDraw, at::kDiscDraw, 3, {k16, kW, k8}, kG, 0, 0},
    // nobody's, re-listed louder: the free spark answered as a record or 0, the
    // set logging the record it is handed
    {"0x47CF20", at::kSparkFree, at::kSparkFree, 0, {}, kG, 0, 0, {}, &FxSparkFree},
    {"0x479160", at::kSparkSet, at::kSparkSet, 1, {kW}, kG, 0, 0, {}, &FxSparkSet, nullptr, true},
    {"0x479260", at::kSparksRun, at::kSparksRun, 0, {}, kF, 0, 0, {}, nullptr, nullptr, true},
    // standard entries re-listed: the stack pointers never logged (their
    // addresses differ between the copy and ours), the points hashed, the outs filled
    {E2E_OURS(EffectGte_ProjectPoint), 2, {0, 0}, kG, 0, 0, {12, 0}, &FxProjectPoint, nullptr, true},
    {E2E_OURS(EffectGte_ProjectSize), 3, {0, 0, 0}, kG, 0, 0, {12, 4, 0}, &FxProjectSize, nullptr, true},
    // the vertex (Prim_VertexScratch) and sxy (the record's +0x74) are fixed
    // cells, logged; p is a local, not logged, filled
    {E2E_OURS(Gte_RotTransPers), 3, {kW, kW, 0}, kG, 0, 0, {8, 0, 0}, &FxRotTransPers, nullptr, true},
    {E2E_OURS(Gte_StoreDepthF), 1, {kW}, kG, 0, 0, {}, &FxStoreDepth, nullptr, true},
};
#undef E2E_OURS

// The ten state tables the dispatchers jump through, read in place; each
// table's own length to the next a dispatcher names (none bounded by a compare).
const sh::DataTable kTables[] = {
    {0x65461C, 2},   // EffectKind48_State8_Steps
    {0x654624, 3},   // EffectKind48_State9_Steps
    {0x654630, 2},   // EffectKind48_State10_Steps
    {0x654638, 2},   // EffectKind48_State11_Steps
    {0x654640, 8},   // EffectKind48_State12_Steps
    {0x65466C, 2},   // EffectKind4A_States
    {0x654674, 2},   // EffectKind4B_States
    {0x65467C, 2},   // EffectKind4C_States
    {0x654684, 3},   // EffectKind4D_States
    {0x654690, 5},   // EffectKind4E_States
};
const std::uint8_t kKinds[] = {0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E};

// Beyond effect mode's standard regions: the scratch past EffectKind30_Shards'
// standard 0x644 - kind 0x4A's trail to 0x92CC84, kind 0x4B's debris to
// 0x92D204, the spiral record 0x92C4A4..0x92D1C4, the ring 0x92D1C8, the sphere
// 0x92D9DC..0x931174 - and the pointers 0x6761CC / 0x6761D0 and the byte 0x6761D8.
const sh::Region kRegions[] = {
    {sh::at::kShards + sh::at::kShardsSize, at::kSphereLight + 12 - (sh::at::kShards + sh::at::kShardsSize)},
    {at::kSpiralPtr, 0x10},
};

// The pointer a cell holds.
unsigned char* Pointer(U cell) { return P(static_cast<U>(Long(P(cell)))); }

// --- the seed ------------------------------------------------------------------------

// The dword +0xC at every compare the states make.
U CountC() {
    return PickOf(0, 1, 2, 8, 9, 0x10, 0x11, 0x20, 0x5A, 0x5B, 0xA5, 0xB8, 0xB9, 30, 31, 44, 45, 60, 0x12C,
                  0xFFFFFFFFu, 0xFFFFFFF1u, 0x80000000u, 0x7FFFFFFFu, sh::Next() % 0x200, sh::Next());
}
// The byte +9.
U Count9() { return PickOf(0, 1, 2, 3, 4, 5, 7, 8, 0x2D, 0x2E, 0x3C, 0x3D, 0x7F, 0x80, 0x81, 0xD7, 0xD8, 0xFF, sh::Next()); }

// Kind 0x4D's column: every record's base height +0x14 and top +0x1C near one
// height (so the column's loop, which may move to another record's top after
// a call, stays under some 180 quads), the fade +0x24 anywhere.
void Columns() {
    const U base = (sh::Next() & 0x7FFFFFFFu) - 0x40000000u;
    for (unsigned r = 0; r < sh::at::kEffectCount; ++r) {
        unsigned char* const e = sh::EffectRecord(r);
        SetLong(e + 0x14, static_cast<std::int32_t>(base + sh::Next() % 0x2000000u - 0x1000000u));
        SetLong(e + 0x1C, static_cast<std::int32_t>(base + (sh::Half() ? sh::Next() % 0x9000000u : sh::Next() % 0x300000u)));
        if (sh::Half()) SetLong(e + 0x24, static_cast<std::int32_t>(PickOf(0, 0x100000, 0x1000000, sh::Next() % 0x4000000u, sh::Next())));
    }
}

void Seed(unsigned k) {
    unsigned char* const s = Sprite_Current;
    // the pointers the states write through: at their records, now and then another place in the regions
    SetLong(P(at::kRingPtr), static_cast<std::int32_t>(sh::Often() ? at::kRingRecord : PickOf(0x92D1E0, 0x92C600, 0x92E000)));
    SetLong(P(at::kSpiralPtr), static_cast<std::int32_t>(sh::Often() ? at::kSpiral : PickOf(0x92C5C4, 0x92C700)));
    SetLong(s + 0xC, static_cast<std::int32_t>(CountC()));
    s[9] = static_cast<unsigned char>(Count9());
    s[6] = static_cast<unsigned char>(PickOf(0, 0, 1, sh::Next()));
    switch (k) {
    case k7Swell:
        // +9 inside the two byte tables (and a little past), +0xC's low bits against the masks
        s[9] = static_cast<unsigned char>(sh::Often() ? sh::Next() % 8 : sh::Next() % 0x40);
        SetLong(s + 0xC, static_cast<std::int32_t>(PickOf(1, 0x10, 0x11, 0x20, 0x5F, 0x50, 3, sh::Next() % 0x60)));
        break;
    case k7Lift:
        SetLong(Pointer(at::kRingPtr) + 8, static_cast<std::int32_t>(PickOf(0xEFFFFFF, 0xF000000, 0xF000001, 0x7F000000, 0xFF000000, 0, sh::Next())));
        break;
    case k12Travel:
        // the point one step from its goal on both axes, one, or neither
        if (sh::Often()) {
            SetLong(s + 0x18, Long(s + 0x34) + Long(s + 0xC));
            if (sh::Often()) SetLong(s + 0x1C, Long(s + 0x38) + Long(s + 0x10));
        }
        break;
    case k4ATrailStep:
        SetWord(P(at::kTrailTurn), PickOf(0, 0xFFC0, 0xFF80, sh::Next()));
        P(at::kTrailSpin)[0] = static_cast<unsigned char>(PickOf(0, 1, sh::Next()));
        break;
    case kSphereDraw:
    case k11Draw:
        // the quads' vertex numbers below the 0x1E2 (a number past them reads past the table; ours aborts)
        for (unsigned i = 0; i < 4 * at::kSphereQuadCount; ++i)
            SetWord(P(at::kSphereQuads + 2 * i), sh::Next() % 8 == 0 ? PickOf(0, 0x1E1) : sh::Next() % at::kSphereVertCount);
        break;
    case k4DRise:
    case k4DFade:
    case k4DColumn:
    case k4DRun:
        Columns();
        if (k == k4DRise && sh::Half())
            SetLong(s + 0x1C, Long(s + 0x14) + static_cast<std::int32_t>(PickOf(0x6FFFFFF, 0x7000000, 0x7000001, 0x8000000)));
        break;
    default: break;
    }
}

void Args(unsigned k, U* a) {
    const U i = sh::Next();
    switch (k) {
    case kMean: {
        // the two 12-bit angles across the 0x800 apart boundary, and each's upper bits
        a[0] = (sh::Next() & 0xFFFFF000u) | (i & 0xFFFu);
        const U d = PickOf(0, 1, 0x7FF, 0x800, 0x801, 0xFFF, sh::Next() & 0xFFFu);
        a[1] = (sh::Next() & 0xFFFFF000u) | ((i + (sh::Half() ? d : 0u - d)) & 0xFFFu);
        break;
    }
    case k4AGlow: a[0] = at::kTrail + at::kTrailStride * (i % at::kTrailCount); break;
    case k4BDebris: a[0] = at::kDebris + at::kDebrisStride * (i % at::kDebrisCount); break;
    default: break;
    }
}

// What the states read again after a call, moved by the group's case of the
// harness's disturbance (from its hash only): +9, +0xC, +6, the spin byte, the
// burst's size word, the ring's height.
void Disturb(U h) {
    const U v = h >> 8;
    unsigned char* const s = Sprite_Current;
    switch (h % 6) {
    case 0: s[9] = static_cast<unsigned char>((v & 1) ? PickOf(1, 0x3C, 0xD7, 0x80) : v >> 1); break;
    case 1: SetLong(s + 0xC, static_cast<std::int32_t>((v & 1) ? PickOf(1, 9, 0x5B, 30, 45) : v >> 1)); break;
    case 2: s[6] = static_cast<unsigned char>(v & 1 ? 0 : v >> 1); break;
    case 3: P(at::kTrailSpin)[0] = static_cast<unsigned char>(v & 1); break;
    case 4: SetWord(P(at::kBurstSize), v); break;
    case 5: {
        unsigned char* const ring = P(static_cast<U>(Long(P(at::kRingPtr))));
        if (sh::InRegions(ring + 8, 4)) SetLong(ring + 8, static_cast<std::int32_t>((v & 1) ? 0xF000000u : v));
        break;
    }
    default: break;
    }
}

}  // namespace

void SelfTest() {
    // BOF3X_E2E_ONLY: the clones whose name contains it (a control's run)
    static sh::Clone chosen[kCount];
    static unsigned index[kCount];
    const char* const only = std::getenv("BOF3X_E2E_ONLY");
    unsigned n = 0;
    for (unsigned k = 0; k < kCount; ++k)
        if (!only || !*only || std::strstr(kAll[k].name, only)) {
            index[n] = k;
            chosen[n++] = kAll[k];
        }
    if (n == 0) bof3::Fatal("effect_2e: BOF3X_E2E_ONLY=%s names no clone", only);
    static unsigned* s_index = index;
    sh::Group g = {"effect_2e", chosen, n, kCallees, sizeof kCallees / sizeof kCallees[0], kTables,
                   sizeof kTables / sizeof kTables[0], kRegions, sizeof kRegions / sizeof kRegions[0],
                   [](unsigned k) { Seed(s_index[k]); }, &Disturb, 4000};
    g.args = [](unsigned k, U* a) { Args(s_index[k], a); };
    g.effect = true;
    g.kinds = kKinds;
    g.n_kinds = sizeof kKinds;
    sh::Run(g);
}

}  // namespace effect_2e

// BOF3X_SHADOW=effect_3a: group E3A's 48 functions through the scenario harness
// in effect mode (scenario_harness.h, docs/scenario_harness.md section 8), once
// at start-up. docs/effect_3a.md section 4. BOF3X_E3A_ONLY=<name> runs the
// clones whose name contains it (the controls' speed-up).
//
// The clone table is tools/band_rows.py --group E3A --clones --harness scenario
// (2026-10-03), each extent read again to its last instruction (capstone) and
// the names given; the tool's extents stand (none differs from the code).
// Shapes: every dispatcher, state and no-argument helper kEffect
// (Sprite_Current one of the 20 Effect_Objects records, +5 the kind, a
// dispatcher's +1 below its table's length); the helpers with arguments kCall.
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_3a.h"
#include "game/effect_3a_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace effect_3a {
namespace {

namespace sh = scenario_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using U = std::uint32_t;

// tools/band_rows.py --group E3A --clones, 2026-10-03.
constexpr sh::CallSite kCalls4804C0[] = {{0x14, 0x5A79A0}, {0x2C, 0x5A77C0}, {0x35, 0x461E50}, {0x41, 0x5A76B0}, {0x49, 0x5A7780},
                                         {0x4E, 0x494060}, {0x5D, 0x494110}, {0x81, 0x494110}, {0xB6, 0x461E50}};
constexpr sh::CallSite kCalls4805B0[] = {{0x1, 0x589810}, {0x67, 0x461E10}, {0x12F, 0x5A79A0}, {0x153, 0x5891F0}, {0x15B, 0x588F20}, {0x160, 0x589840}};
constexpr sh::CallSite kCalls480730[] = {{0x56, 0x59E930}};
constexpr sh::CallSite kCalls4807A0[] = {{0x5, 0x494060}, {0x65, 0x494110}};
constexpr sh::CallSite kCalls480910[] = {{0x3F, 0x5B93D2}, {0x4A, 0x5B93D2}, {0x95, 0x5B93D2}, {0xA1, 0x5B93D2},
                                         {0xBA, 0x5B93D2}, {0xF5, 0x5B93D2}, {0x12F, 0x587740}};
constexpr sh::CallSite kCalls480A50[] = {{0x96, 0x5A7750}, {0xD8, 0x461E50}, {0x115, 0x589840}};
constexpr sh::CallSite kCalls480B90[] = {{0x0, 0x4812B0}, {0x2F, 0x587740}};
constexpr sh::CallSite kCalls480BD0[] = {{0x13, 0x4812D0}, {0x3A, 0x4813B0}};
constexpr sh::CallSite kCalls480C70[] = {{0x10, 0x587740}, {0x3B, 0x481550}, {0xAB, 0x4813B0}, {0xC7, 0x589840}};
constexpr sh::CallSite kCalls480DB0[] = {{0x1A, 0x481740}, {0x4C, 0x587740}};
constexpr sh::CallSite kCalls480E10[] = {{0x10, 0x481740}};
constexpr sh::CallSite kCalls480E50[] = {{0x2B, 0x481740}, {0x64, 0x482050}, {0x95, 0x482240}, {0xA8, 0x587740}};
constexpr sh::CallSite kCalls480F10[] = {{0x17, 0x493B50}, {0x1C, 0x482360}, {0x21, 0x482070}};
constexpr sh::CallSite kCalls480F90[] = {{0x17, 0x493B50}, {0x1C, 0x482360}, {0x21, 0x482070}};
constexpr sh::CallSite kCalls480FD0[] = {{0x17, 0x493B50}, {0x1C, 0x482070}, {0x21, 0x482360}, {0x35, 0x481910}};
constexpr sh::CallSite kCalls481090[] = {{0x17, 0x493B50}, {0x1C, 0x482070}, {0x21, 0x482360}, {0x35, 0x481910}, {0x66, 0x587740}};
constexpr sh::CallSite kCalls481100[] = {{0x13, 0x482070}, {0x29, 0x481910}, {0x4A, 0x589840}};
constexpr sh::CallSite kCalls481170[] = {{0x0, 0x481B80}, {0x29, 0x587740}};
constexpr sh::CallSite kCalls4811A0[] = {{0x14, 0x481BB0}, {0x1E, 0x481BD0}, {0x26, 0x481C40}};
constexpr sh::CallSite kCalls4811F0[] = {{0x11, 0x587740}, {0x44, 0x481EA0}, {0x4C, 0x481C40}};
constexpr sh::CallSite kCalls481260[] = {{0x2C, 0x481EA0}, {0x4A, 0x589840}};
constexpr sh::CallSite kCalls4812D0[] = {{0x25, 0x5B93D2}, {0x32, 0x5B93D2}, {0x6B, 0x5A7A50}, {0x88, 0x5A7A00}, {0xAE, 0x5A7A50}, {0xBB, 0x5A7A00}};
constexpr sh::CallSite kCalls4813B0[] = {{0x4, 0x494060}, {0x19, 0x5A79A0}, {0x31, 0x5A77C0}, {0x3A, 0x461E50}, {0x5F, 0x4814B0}};
constexpr sh::CallSite kCalls4814B0[] = {{0xC, 0x5A76B0}, {0x14, 0x5A7780}, {0x26, 0x494110}, {0x4C, 0x494110}, {0x90, 0x461E50}};
constexpr sh::CallSite kCalls481550[] = {{0x17, 0x5A79A0}, {0x2D, 0x5A77C0}, {0x36, 0x461E50}, {0x3B, 0x494060}, {0x6C, 0x494110},
                                         {0x8B, 0x494110}, {0xB4, 0x5A7610}, {0xBC, 0x5A7780}, {0xF6, 0x5A7A50}, {0x10D, 0x5A7A00},
                                         {0x12D, 0x494110}, {0x133, 0x5A7A50}, {0x14A, 0x5A7A00}, {0x16A, 0x494110}, {0x1CA, 0x461E50}};
constexpr sh::CallSite kCalls481740[] = {{0x17, 0x5A79A0}, {0x2E, 0x5A77C0}, {0x37, 0x461E50}, {0x3C, 0x494060}, {0x4B, 0x494110}, {0x6A, 0x4941E0},
                                         {0xDF, 0x5A75F0}, {0xE7, 0x5A7780}, {0x120, 0x5A7A50}, {0x141, 0x5A7A00}, {0x1A3, 0x461E50}};
constexpr sh::CallSite kCalls481910[] = {{0xE, 0x5A7610}, {0x16, 0x5A7780}, {0x25, 0x494110}, {0x6A, 0x494110}, {0xD9, 0x461E50},
                                         {0xE5, 0x5A7610}, {0xED, 0x5A7780}, {0xF8, 0x494110}, {0x139, 0x494110}, {0x1A7, 0x461E50},
                                         {0x1EB, 0x494110}, {0x1F7, 0x5A7750}, {0x1FF, 0x5A7780}, {0x229, 0x461E50}};
constexpr sh::CallSite kCalls481BD0[] = {{0x1B, 0x5B93D2}, {0x47, 0x5B93D2}};
constexpr sh::CallSite kCalls481C40[] = {{0x12, 0x5A79A0}, {0x2A, 0x5A77C0}, {0x33, 0x461E50}, {0x3B, 0x494060}, {0x5D, 0x481CC0}};
constexpr sh::CallSite kCalls481CC0[] = {{0x20, 0x4941E0}, {0x2B, 0x494110}, {0x57, 0x5A75F0}, {0x5F, 0x5A7780}, {0x73, 0x5A7A50},
                                         {0x8E, 0x5A7A00}, {0xAF, 0x5A7A50}, {0xCA, 0x5A7A00}, {0x11F, 0x461E50}};
constexpr sh::CallSite kCalls481EA0[] = {{0x13, 0x5A79A0}, {0x2B, 0x5A77C0}, {0x34, 0x461E50}, {0x39, 0x494060}, {0x69, 0x494110},
                                         {0x80, 0x494110}, {0xAC, 0x5A79A0}, {0xC5, 0x5A77C0}, {0xD8, 0x572FA0}, {0xE4, 0x5A75B0},
                                         {0xEC, 0x5A7780}, {0x132, 0x494110}, {0x149, 0x494110}, {0x196, 0x572FA0}};
constexpr sh::CallSite kCalls482070[] = {{0x5, 0x494060}, {0x31, 0x4820C0}};
constexpr sh::CallSite kCalls4820C0[] = {{0x14, 0x5A79A0}, {0x2C, 0x5A77C0}, {0x35, 0x461E50}, {0x55, 0x4941E0}, {0x60, 0x494110}, {0x8A, 0x5A75F0},
                                         {0x92, 0x5A7780}, {0xA6, 0x5A7A50}, {0xC1, 0x5A7A00}, {0xE2, 0x5A7A50}, {0xFD, 0x5A7A00}, {0x154, 0x461E50}};
constexpr sh::CallSite kCalls482240[] = {{0x2D, 0x5B93D2}, {0x3A, 0x5B93D2}, {0x4D, 0x5B93D2}, {0x5C, 0x5A7A50}, {0x67, 0x5A7A00},
                                         {0x78, 0x5A7A50}, {0x83, 0x5A7A00}, {0x97, 0x494180}, {0xA5, 0x5A7F10}, {0xB5, 0x5A7F80},
                                         {0xC3, 0x5A7FF0}, {0xD2, 0x5A7C70}, {0xE1, 0x5A7C70}, {0xE9, 0x5B93D2}};
constexpr sh::CallSite kCalls482360[] = {{0x12, 0x5A79A0}, {0x2A, 0x5A77C0}, {0x33, 0x461E50}, {0x3B, 0x494060}, {0x4E, 0x493C60}};

#define E3A_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define E3A_CALLS(a) a, E3A_N(a)
#define E3A_FN(name) reinterpret_cast<const void*>(&::name)
constexpr sh::Shape kEf = sh::Shape::kEffect;
constexpr sh::Shape kCa = sh::Shape::kCall;
// {name, base, size, calls, imms, tables, ours, ret_mask, calm, shape, pointers, state_span, sub_span, kind}
const sh::Clone kAll[] = {
    {"EffectKind60_Run", 0x4801F0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E3A_FN(EffectKind60_Run), 0, false, kEf, 0, 6, 0, 0x60},
    {"EffectKind60_DrawLine", 0x4804C0, 0xC3, E3A_CALLS(kCalls4804C0), nullptr, 0, nullptr, 0, E3A_FN(EffectKind60_DrawLine), 0, false, kCa, 0, 0, 0, 0x60},
    {"EffectKind61_Run", 0x480590, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E3A_FN(EffectKind61_Run), 0, false, kEf, 0, 5, 0, 0x61},
    {"EffectKind61_Capture", 0x4805B0, 0x17C, E3A_CALLS(kCalls4805B0), nullptr, 0, nullptr, 0, E3A_FN(EffectKind61_Capture), 0, false, kEf, 0, 0, 0, 0x61},
    {"EffectKind61_Store", 0x480730, 0x6A, E3A_CALLS(kCalls480730), nullptr, 0, nullptr, 0, E3A_FN(EffectKind61_Store), 0, false, kEf, 0, 0, 0, 0x61},
    {"EffectKind61_Scatter", 0x4807A0, 0x16B, E3A_CALLS(kCalls4807A0), nullptr, 0, nullptr, 0, E3A_FN(EffectKind61_Scatter), 0, false, kEf, 0, 0, 0, 0x61},
    {"EffectKind61_Arm", 0x480910, 0x13D, E3A_CALLS(kCalls480910), nullptr, 0, nullptr, 0, E3A_FN(EffectKind61_Arm), 0, false, kEf, 0, 0, 0, 0x61},
    {"EffectKind61_Twinkle", 0x480A50, 0x11C, E3A_CALLS(kCalls480A50), nullptr, 0, nullptr, 0, E3A_FN(EffectKind61_Twinkle), 0, false, kEf, 0, 0, 0, 0x61},
    {"EffectKind62_Run", 0x480B70, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E3A_FN(EffectKind62_Run), 0, false, kEf, 0, 3, 0, 0x62},
    {"EffectKind62_Start", 0x480B90, 0x36, E3A_CALLS(kCalls480B90), nullptr, 0, nullptr, 0, E3A_FN(EffectKind62_Start), 0, false, kEf, 0, 0, 0, 0x62},
    {"EffectKind62_Rays", 0x480BD0, 0x98, E3A_CALLS(kCalls480BD0), nullptr, 0, nullptr, 0, E3A_FN(EffectKind62_Rays), 0, false, kEf, 0, 0, 0, 0x62},
    {"EffectKind62_Blast", 0x480C70, 0xCD, E3A_CALLS(kCalls480C70), nullptr, 0, nullptr, 0, E3A_FN(EffectKind62_Blast), 0, false, kEf, 0, 0, 0, 0x62},
    {"EffectKind62_ClearRays", 0x4812B0, 0x14, nullptr, 0, nullptr, 0, nullptr, 0, E3A_FN(EffectKind62_ClearRays), 0, false, kEf, 0, 0, 0, 0x62},
    {"EffectKind62_SpawnRay", 0x4812D0, 0xD5, E3A_CALLS(kCalls4812D0), nullptr, 0, nullptr, 0, E3A_FN(EffectKind62_SpawnRay), 0, false, kEf, 0, 0, 0, 0x62},
    {"EffectKind62_StepRays", 0x4813B0, 0xFC, E3A_CALLS(kCalls4813B0), nullptr, 0, nullptr, 0, E3A_FN(EffectKind62_StepRays), 0xFF, false, kEf, 0, 0, 0, 0x62},
    {"EffectKind62_DrawRay", 0x4814B0, 0x9E, E3A_CALLS(kCalls4814B0), nullptr, 0, nullptr, 0, E3A_FN(EffectKind62_DrawRay), 0, false, kCa, 0, 0, 0, 0x62},
    {"EffectKind62_DrawRing", 0x481550, 0x1EB, E3A_CALLS(kCalls481550), nullptr, 0, nullptr, 0, E3A_FN(EffectKind62_DrawRing), 0xFF, false, kCa, 0, 0, 0, 0x62},
    {"EffectKind64_Run", 0x480D40, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E3A_FN(EffectKind64_Run), 0, false, kEf, 0, 9, 0, 0x64},
    {"EffectKind64_Start", 0x480D60, 0x4F, nullptr, 0, nullptr, 0, nullptr, 0, E3A_FN(EffectKind64_Start), 0, false, kEf, 0, 0, 0, 0x64},
    {"EffectKind64_Grow", 0x480DB0, 0x53, E3A_CALLS(kCalls480DB0), nullptr, 0, nullptr, 0, E3A_FN(EffectKind64_Grow), 0, false, kEf, 0, 0, 0, 0x64},
    {"EffectKind64_Hold", 0x480E10, 0x3E, E3A_CALLS(kCalls480E10), nullptr, 0, nullptr, 0, E3A_FN(EffectKind64_Hold), 0, false, kEf, 0, 0, 0, 0x64},
    {"EffectKind64_Rise", 0x480E50, 0xB3, E3A_CALLS(kCalls480E50), nullptr, 0, nullptr, 0, E3A_FN(EffectKind64_Rise), 0, false, kEf, 0, 0, 0, 0x64},
    {"EffectKind64_Burst", 0x480F10, 0x7D, E3A_CALLS(kCalls480F10), nullptr, 0, nullptr, 0, E3A_FN(EffectKind64_Burst), 0, false, kEf, 0, 0, 0, 0x64},
    {"EffectKind64_Launch", 0x480F90, 0x38, E3A_CALLS(kCalls480F90), nullptr, 0, nullptr, 0, E3A_FN(EffectKind64_Launch), 0, false, kEf, 0, 0, 0, 0x64},
    {"EffectKind64_Fly", 0x480FD0, 0xB3, E3A_CALLS(kCalls480FD0), nullptr, 0, nullptr, 0, E3A_FN(EffectKind64_Fly), 0, false, kEf, 0, 0, 0, 0x64},
    {"EffectKind64_FlyWait", 0x481090, 0x6D, E3A_CALLS(kCalls481090), nullptr, 0, nullptr, 0, E3A_FN(EffectKind64_FlyWait), 0, false, kEf, 0, 0, 0, 0x64},
    {"EffectKind64_Fade", 0x481100, 0x50, E3A_CALLS(kCalls481100), nullptr, 0, nullptr, 0, E3A_FN(EffectKind64_Fade), 0, false, kEf, 0, 0, 0, 0x64},
    {"EffectKind64_DrawGlow", 0x481740, 0x1C2, E3A_CALLS(kCalls481740), nullptr, 0, nullptr, 0, E3A_FN(EffectKind64_DrawGlow), 0, false, kCa, 0, 0, 0, 0x64},
    {"EffectKind64_DrawTrail", 0x481910, 0x263, E3A_CALLS(kCalls481910), nullptr, 0, nullptr, 0, E3A_FN(EffectKind64_DrawTrail), 0, false, kCa, 0, 0, 0, 0x64},
    {"EffectKind64_ClearSparks", 0x482050, 0x14, nullptr, 0, nullptr, 0, nullptr, 0, E3A_FN(EffectKind64_ClearSparks), 0, false, kEf, 0, 0, 0, 0x64},
    {"EffectKind64_StepSparks", 0x482070, 0x45, E3A_CALLS(kCalls482070), nullptr, 0, nullptr, 0, E3A_FN(EffectKind64_StepSparks), 0xFF, false, kEf, 0, 0, 0, 0x64},
    {"EffectKind64_DrawSpark", 0x4820C0, 0x173, E3A_CALLS(kCalls4820C0), nullptr, 0, nullptr, 0, E3A_FN(EffectKind64_DrawSpark), 0, false, kCa, 0, 0, 0, 0x64},
    {"EffectKind64_InitShard", 0x482240, 0x114, E3A_CALLS(kCalls482240), nullptr, 0, nullptr, 0, E3A_FN(EffectKind64_InitShard), 0, false, kCa, 0, 0, 0, 0x64},
    {"EffectKind64_DrawShards", 0x482360, 0x63, E3A_CALLS(kCalls482360), nullptr, 0, nullptr, 0, E3A_FN(EffectKind64_DrawShards), 0, false, kEf, 0, 0, 0, 0x64},
    {"EffectKind68_Run", 0x481150, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E3A_FN(EffectKind68_Run), 0, false, kEf, 0, 4, 0, 0x68},
    {"EffectKind68_Start", 0x481170, 0x30, E3A_CALLS(kCalls481170), nullptr, 0, nullptr, 0, E3A_FN(EffectKind68_Start), 0, false, kEf, 0, 0, 0, 0x68},
    {"EffectKind68_Rise", 0x4811A0, 0x4D, E3A_CALLS(kCalls4811A0), nullptr, 0, nullptr, 0, E3A_FN(EffectKind68_Rise), 0, false, kEf, 0, 0, 0, 0x68},
    {"EffectKind68_Wall", 0x4811F0, 0x6A, E3A_CALLS(kCalls4811F0), nullptr, 0, nullptr, 0, E3A_FN(EffectKind68_Wall), 0, false, kEf, 0, 0, 0, 0x68},
    {"EffectKind68_End", 0x481260, 0x50, E3A_CALLS(kCalls481260), nullptr, 0, nullptr, 0, E3A_FN(EffectKind68_End), 0, false, kEf, 0, 0, 0, 0x68},
    {"EffectKind68_ClearMotes", 0x481B80, 0x21, nullptr, 0, nullptr, 0, nullptr, 0, E3A_FN(EffectKind68_ClearMotes), 0, false, kEf, 0, 0, 0, 0x68},
    {"EffectKind68_FindMote", 0x481BB0, 0x19, nullptr, 0, nullptr, 0, nullptr, 0, E3A_FN(EffectKind68_FindMote), 0xFFFFFFFFu, false, kEf, 0, 0, 0, 0x68},
    {"EffectKind68_InitMote", 0x481BD0, 0x66, E3A_CALLS(kCalls481BD0), nullptr, 0, nullptr, 0, E3A_FN(EffectKind68_InitMote), 0, false, kCa, 0, 0, 0, 0x68},
    {"EffectKind68_StepMotes", 0x481C40, 0x73, E3A_CALLS(kCalls481C40), nullptr, 0, nullptr, 0, E3A_FN(EffectKind68_StepMotes), 0xFF, false, kEf, 0, 0, 0, 0x68},
    {"EffectKind68_DrawMote", 0x481CC0, 0x13E, E3A_CALLS(kCalls481CC0), nullptr, 0, nullptr, 0, E3A_FN(EffectKind68_DrawMote), 0, false, kCa, 0, 0, 0, 0x68},
    {"EffectKind68_MoteGlow", 0x481E00, 0x36, nullptr, 0, nullptr, 0, nullptr, 0, E3A_FN(EffectKind68_MoteGlow), 0, false, kCa, 0, 0, 0, 0x68},
    {"EffectKind68_MoteHold", 0x481E40, 0x1B, nullptr, 0, nullptr, 0, nullptr, 0, E3A_FN(EffectKind68_MoteHold), 0, false, kCa, 0, 0, 0, 0x68},
    {"EffectKind68_MoteRise", 0x481E60, 0x3F, nullptr, 0, nullptr, 0, nullptr, 0, E3A_FN(EffectKind68_MoteRise), 0, false, kCa, 0, 0, 0, 0x68},
    {"EffectKind68_DrawWall", 0x481EA0, 0x1AC, E3A_CALLS(kCalls481EA0), nullptr, 0, nullptr, 0, E3A_FN(EffectKind68_DrawWall), 0, false, kCa, 0, 0, 0, 0x68},
};
#undef E3A_FN
#undef E3A_CALLS
#undef E3A_N

enum : unsigned {
    k60Run, k60Line,
    k61Run, k61Capture, k61Store, k61Scatter, k61Arm, k61Twinkle,
    k62Run, k62Start, k62Rays, k62Blast, k62Clear, k62Spawn, k62Step, k62Ray, k62Ring,
    k64Run, k64Start, k64Grow, k64Hold, k64Rise, k64Burst, k64Launch, k64Fly, k64FlyWait, k64Fade,
    k64Glow, k64Trail, k64ClearSparks, k64StepSparks, k64Spark, k64InitShard, k64Shards,
    k68Run, k68Start, k68Rise, k68Wall, k68End, k68Clear, k68Find, k68Init, k68Step, k68Mote,
    k68MoteGlow, k68MoteHold, k68MoteRise, k68Wall2, kCount
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

// The harness's packet buffer (scenario_harness.cpp g_packets, 0x800 bytes; its
// FxCommitPrim keeps 0x40 to spare).
constexpr unsigned kPacketsSize = 0x800;
// While EffectKind64_DrawGlow runs (the seed sets it): its POLY_G3s' rim depths
// (+0x20, +0x30) are a stack word the original never writes and the centre's
// in ours (DIV-0068), so the committed triangle's +0x10 is copied over both on
// either side before the packet is compared.
bool g_glow = false;
// The triangle Gpu_SetPolyG3 last made (the harness's disturbance may move the
// cursor before the commit, so the commit's cursor is not always it).
unsigned char* g_triangle = nullptr;
U FxTriangle(const U* a, U answer) {
    g_triangle = P(a[0]);
    return answer;
}
// Gfx_CommitPrim: the cursor += size (a byte) while the packet stays in the
// buffer, as the harness's FxCommitPrim; the glow's depths levelled first.
U FxCommit(const U* a, U answer) {
    unsigned char* const next = Gfx_PacketNext;
    unsigned char* const base = sh::Packets();
    const unsigned size = a[1] & 0xFF;
    if (g_glow && size == 0x34 && g_triangle != nullptr && sh::InRegions(g_triangle, 0x34)) {
        std::memcpy(g_triangle + 0x20, g_triangle + 0x10, 4);
        std::memcpy(g_triangle + 0x30, g_triangle + 0x10, 4);
    }
    g_triangle = nullptr;
    if (next >= base && next + size + 0x40 <= base + kPacketsSize) Gfx_PacketNext = next + size;
    return answer;
}
// Sprite_SetAnimation acts on Sprite_Current: kind 0x61 points it at the
// borrowed record for the call - logged, so a call on the wrong record shows.
U FxOnCurrent(const U*, U answer) {
    sh::Note(Key(Sprite_Current));
    return answer;
}
// 0x59E930 (rect, to): Gfx_VramShadow's w x h rectangle copied to `to` - filled
// here (its first w * h words, at most the 0x100 pixels the seed allows).
U FxStoreImage(const U* a, U answer) {
    const std::int32_t w = static_cast<std::int16_t>(Word(P(a[0]) + 4));
    const std::int32_t h = static_cast<std::int16_t>(Word(P(a[0]) + 6));
    if (w > 0 && h > 0 && w * h <= 0x100 && Writable(a[1], static_cast<unsigned>(2 * w * h)))
        sh::FillBytes(P(a[1]), static_cast<unsigned>(2 * w * h));
    return answer;
}
// EffectKind68_FindMote: null a quarter of the time, else one of the 16 motes
// (the caller hands it on).
U FxFindMote(const U*, U answer) {
    if (answer % 4 == 0) return 0;
    return at::kShards + ((answer >> 4) % at::kMotes) * at::kMoteStride;
}

#define E3A_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
constexpr sh::Answer kG = sh::Answer::kGarbage, kPh = sh::Answer::kPhase, kF = sh::Answer::kFlag;
constexpr U kW = 0xFFFFFFFFu, k16 = 0xFFFFu, k8 = 0xFFu;
const sh::Callee kCallees[] = {
    // the group's own called directly, by name: the void ones with no argument
    {E3A_OURS(EffectKind62_ClearRays), 0, {}, kPh, 0, 0},
    {E3A_OURS(EffectKind62_SpawnRay), 0, {}, kPh, 0, 0},
    {E3A_OURS(EffectKind64_ClearSparks), 0, {}, kPh, 0, 0},
    {E3A_OURS(EffectKind64_DrawShards), 0, {}, kPh, 0, 0},
    {E3A_OURS(EffectKind68_ClearMotes), 0, {}, kPh, 0, 0},
    // those answering in al (the callers test it) or eax (a mote, handed on)
    {E3A_OURS(EffectKind62_StepRays), 0, {}, kF, 0, 0},
    {E3A_OURS(EffectKind64_StepSparks), 0, {}, kF, 0, 0},
    {E3A_OURS(EffectKind68_StepMotes), 0, {}, kF, 0, 0},
    {E3A_OURS(EffectKind68_FindMote), 0, {}, kG, 0, 0, {}, &FxFindMote, nullptr, true},
    // those with arguments, each mask what the callee reads: records and the
    // record's points (same address on both sides) logged and hashed; the
    // glow's size a word (`mov ax, [esp + 0x64]`, pushed `mov cx` over the
    // dispatcher's ecx), its colour and the shades bytes (`mov cl / bl`)
    {E3A_OURS(EffectKind62_DrawRay), 1, {kW}, kG, 0, 0, {0x38}, nullptr, nullptr, true},
    {E3A_OURS(EffectKind62_DrawRing), 3, {kW, kW, kW}, kG, 0, 0, {12}, nullptr, nullptr, true},
    {E3A_OURS(EffectKind64_DrawGlow), 3, {kW, k16, k8}, kG, 0, 0, {12}, nullptr, nullptr, true},
    {E3A_OURS(EffectKind64_DrawTrail), 3, {kW, kW, k8}, kG, 0, 0, {12, 12}, nullptr, nullptr, true},
    {E3A_OURS(EffectKind64_DrawSpark), 1, {kW}, kG, 0, 0, {0x18}, nullptr, nullptr, true},
    {E3A_OURS(EffectKind64_InitShard), 1, {kW}, kG, 0, 0, {}, nullptr, nullptr, true},
    {E3A_OURS(EffectKind68_InitMote), 1, {kW}, kG, 0, 0, {}, nullptr, nullptr, true},
    {E3A_OURS(EffectKind68_DrawMote), 1, {kW}, kG, 0, 0, {0x1C}, nullptr, nullptr, true},
    {E3A_OURS(EffectKind68_DrawWall), 1, {k8}, kG, 0, 0},
    // group E4F's two (wave four), raw: the spark it spawns into the pool (no
    // argument) and a shard of 0x2C read to +0x2B
    {"0x493B50", at::kSparkSpawn, at::kSparkSpawn, 0, {}, kG, 0, 0},
    {"0x493C60", at::kShardDraw, at::kShardDraw, 1, {kW}, kG, 0, 0, {0x2C}, nullptr, nullptr, true},
    // standard rows re-listed: the cursor with the glow's depths levelled;
    // Sprite_SetAnimation logging Sprite_Current; 0x59E930 filling the pixels
    {E3A_OURS(Gfx_CommitPrim), 2, {k8, k8}, kG, 0, 0, {0, 0}, &FxCommit, nullptr, true},
    {E3A_OURS(Gpu_SetPolyG3), 1, {kW}, kG, 0, 0, {16}, &FxTriangle, nullptr, true},
    {E3A_OURS(Sprite_SetAnimation), 1, {k8}, kG, 0, 0, {}, &FxOnCurrent, nullptr, true},
    {"0x59E930", at::kStoreImage, at::kStoreImage, 2, {0, kW}, kG, 0, 0, {8, 0}, &FxStoreImage, nullptr, true},
};
#undef E3A_OURS

// The five state tables the dispatchers jump through, read in place; each
// table's own length (symbols.toml [[data]], none bounded by a compare).
// EffectKind68_MoteStates (0x6549AC, 3) is left in place: its handlers take
// the mote as an argument, which a handler recorder does not log, so both
// sides run Capcom's three (they write only the mote and the two flags,
// compared).
const sh::DataTable kTables[] = {
    {0x65492C, 6}, {0x654958, 5}, {0x65496C, 3}, {0x654978, 9}, {0x65499C, 4},
};
const std::uint8_t kKinds[] = {0x60, 0x61, 0x62, 0x64, 0x68};

// Beyond the standard effect regions: the kinds' cells 0x676260..0x676267,
// kind 0x61's frames in .data (seeded small: the original's second frame makes
// up to 4,096 particles), its first 0x100 particles.
constexpr unsigned kParticlesSeeded = 0x100;
const sh::Region kRegions[] = {
    {at::kMoteFlagB, at::kCellsSize},
    {at::kFrames, 8 * at::kFrameCount},
    {at::kParticles, kParticlesSeeded * at::kParticleStride},
};

// --- the seed ------------------------------------------------------------------------

unsigned char* Rec(unsigned r) { return sh::EffectRecord(r); }
unsigned char* RayAt(unsigned i) { return P(at::kShards + i * at::kRayStride); }
unsigned char* SparkAt(unsigned i) { return P(at::kShards + i * at::kSparkStride); }
unsigned char* MoteAt(unsigned i) { return P(at::kShards + i * at::kMoteStride); }
unsigned char* ShardAt(unsigned i) { return P(at::kShards64 + i * at::kShardStride64); }
unsigned char* ParticleAt(unsigned i) { return P(at::kParticles + i * at::kParticleStride); }

// A float in [lo, hi) with a fraction, or (one in sixteen) a whole number.
void SeedFloat(unsigned char* p, float lo, float hi) {
    const U n = sh::Next();
    float f = lo + (hi - lo) * static_cast<float>(n & 0xFFFFF) / static_cast<float>(0x100000);
    if ((n >> 20) % 16 == 0) f = static_cast<float>(static_cast<int>(f));
    std::memcpy(p, &f, sizeof f);
}

// Kind 0x61's two frames: x and y small (with 0 and negatives), w * h at most
// the 0x100 particles seeded (w up to 0x40 with few rows), 0 and negatives.
void SeedFrames() {
    for (unsigned v = 0; v < at::kFrameCount; ++v) {
        unsigned char* const f = P(at::kFrames + 8 * v);
        SetWord(f, PickOf(0x10, 0x20, 0, 0xFFF0, sh::Next() & 0x7F));
        SetWord(f + 2, PickOf(0x28, 0x30, 0, 0xFFF0, sh::Next() & 0x7F));
        U w = PickOf(0, 0xFFFF, 0x8000, 1, 2, 3, 5, 8, 0x10, 0x20, 0x40, 1 + sh::Next() % 16);
        U h;
        if (static_cast<std::int16_t>(w) > 0)
            // at most 0xFF rows: the original's row byte never passes 0x100 (it never ends)
            h = PickOf(0, 0xFFFF, 0x8000, 1, 1 + sh::Next() % (kParticlesSeeded / w < 0xFF ? kParticlesSeeded / w : 0xFF));
        else
            h = PickOf(0, 1, 3, 0xFFFF);
        SetWord(f + 4, w);
        SetWord(f + 6, h);
    }
}

// Kind 0x61's particles: flags 0 / 1 and others, counts round 0 and 1, the
// points sane floats, the speeds any byte.
void SeedParticles() {
    for (unsigned i = 0; i < kParticlesSeeded; ++i) {
        unsigned char* const p = ParticleAt(i);
        p[0] = static_cast<unsigned char>(PickOf(0, 1, 1, 0x80, sh::Next()));
        p[1] = static_cast<unsigned char>(PickOf(0, 1, 2, 0x28, sh::Next()));
        SeedFloat(p + 4, -400.0f, 400.0f);
        SeedFloat(p + 8, -300.0f, 300.0f);
    }
}

// The rays: half in use, phases 0, 1 and others, counts at 1.
void SeedRays() {
    for (unsigned i = 0; i < at::kRays; ++i) {
        unsigned char* const r = RayAt(i);
        r[0] = static_cast<unsigned char>(sh::Half() ? 0 : PickOf(1, 1, 0x80, sh::Next()));
        r[1] = static_cast<unsigned char>(PickOf(0, 1, 0, 1, 2, sh::Next()));
        r[2] = static_cast<unsigned char>(PickOf(1, 1, 2, 0, 8, sh::Next()));
    }
}

// The sparks: half in use, lives at 1.
void SeedSparks() {
    for (unsigned i = 0; i < at::kSparks; ++i) {
        unsigned char* const k = SparkAt(i);
        k[0] = static_cast<unsigned char>(sh::Half() ? 0 : PickOf(1, 1, sh::Next()));
        k[2] = static_cast<unsigned char>(PickOf(1, 1, 2, 0, 0x10, sh::Next()));
    }
}

// The motes: half in use, every state below the table's three (past them ours
// aborts and the original calls through the next kind's table), counts at 1.
void SeedMotes() {
    for (unsigned i = 0; i < at::kMotes; ++i) {
        unsigned char* const m = MoteAt(i);
        m[0] = static_cast<unsigned char>(sh::Half() ? 0 : PickOf(1, 1, sh::Next()));
        m[1] = static_cast<unsigned char>(sh::Next() % 3);
        m[2] = static_cast<unsigned char>(PickOf(1, 1, 2, 0, 0x10, sh::Next()));
    }
    sh::Mem(at::kMoteFlagA)[0] = static_cast<unsigned char>(PickOf(0, 0, 1, sh::Next()));
    sh::Mem(at::kMoteFlagB)[0] = static_cast<unsigned char>(PickOf(0, 0, 1, sh::Next()));
}

void Seed(unsigned k) {
    g_glow = k == k64Glow;
    // every record (the disturbance may move Sprite_Current among them, and
    // onto the one kind 0x61's capture fills from ObjTrio): +7 inside ObjTrio
    for (unsigned r = 0; r < sh::at::kEffectCount; ++r) Rec(r)[7] = static_cast<unsigned char>(sh::Next() % at::kObjTrioCount);
    for (unsigned m = 0; m < at::kObjTrioCount; ++m) ObjTrio[at::kObjTrioStride * m + 7] = static_cast<unsigned char>(sh::Next() % at::kObjTrioCount);
    sh::Mem(at::kVariant)[0] = static_cast<unsigned char>(sh::Next() % at::kFrameCount);
    SetWord(sh::Mem(at::kParticleCount), PickOf(0, 1, 2, 0x10, 0x11, kParticlesSeeded, sh::Next() % (kParticlesSeeded + 1)));
    SeedFrames();
    unsigned char* const s = Sprite_Current;
    s[9] = static_cast<unsigned char>(PickOf(1, 1, 0, 2, 0x13, 0x14, 0x15, 0xFF, sh::Next()));
    s[6] = static_cast<unsigned char>(PickOf(0, 0, 1, 2, 3, 4, 0x10, sh::Next()));
    switch (k) {
    case k61Capture:
        for (unsigned m = 0; m < at::kObjTrioCount; ++m)
            ObjTrio[at::kObjTrioStride * m + at::kMemberFlag] = static_cast<unsigned char>(PickOf(0, 0, 1, sh::Next()));
        break;
    case k61Arm:
    case k61Twinkle: SeedParticles(); break;
    case k62Rays:
        s[0xA] = sh::Half() ? s[6] : static_cast<unsigned char>(PickOf(0, 1, s[6] - 1, sh::Next()));
        if (sh::Half()) {
            Field_Request = static_cast<unsigned char>(PickOf(0, 1, 3, 2, sh::Next()));
            SetWord(sh::Mem(at::kMessageWord), PickOf(0x12, 0x12, 0x13, 0x112, sh::Next()));
            sh::Mem(at::kStep)[0] = static_cast<unsigned char>(PickOf(0xC, 0xC, 0xB, 0xD, sh::Next()));
        }
        SeedRays();
        break;
    case k62Blast:
        SetLong(s + 0xC, static_cast<std::int32_t>(PickOf(0, 0x80, 0x100, 0x7F, 0xA000, 0xFFFFFF80u, sh::Next() & 0x1FF, sh::Next())));
        SetLong(s + 0x10, static_cast<std::int32_t>(PickOf(0, 0x8C, 0x118, 0x8B, 0xB000, 0xFFFFFF74u, sh::Next() & 0x1FF, sh::Next())));
        SetLong(s + 0x18, static_cast<std::int32_t>(PickOf(0, 0x80, 0xFFFFFF80u, sh::Next() & 0x7FF, sh::Next())));
        SetLong(s + 0x1C, static_cast<std::int32_t>(PickOf(0, 0x8C, 0xFFFFFF74u, sh::Next() & 0x7FF, sh::Next())));
        break;
    case k62Spawn:
    case k62Step:
    case k62Clear:
    case k62Ray: SeedRays(); break;
    case k64Rise:
    case k64Grow:
    case k64Hold:
    case k64Fade: SetWord(s + 0x2E, PickOf(0, 0x10, 6, 0xFFFF, 0x20, sh::Next())); break;
    case k64FlyWait: sh::Mem(at::kCounter)[0] = static_cast<unsigned char>(PickOf(0x18, 0x18, 0x17, 0x19, sh::Next())); break;
    case k64Trail:
        // from (+0x18) behind to (+0x34) on x by 0..9 steps of 0x10000, half the time
        if (sh::Half()) SetLong(s + 0x34, Long(s + 0x18) + static_cast<std::int32_t>(sh::Next() % 0xA0000));
        break;
    case k64StepSparks:
    case k64Spark:
    case k64ClearSparks: SeedSparks(); break;
    case k68Rise: SetWord(s + 0x2E, PickOf(0, 1, 4, 5, 0x40, 0x41, sh::Next())); SeedMotes(); break;
    case k68Wall:
    case k68End:
        SetWord(s + 0x30, PickOf(0x59, 0x5A, 0x5B, 0x99, 0x9A, 0x7FFF, 0xFFFF, 0x4059, sh::Next()));
        SetWord(s + 0x2E, PickOf(0, 1, 2, sh::Next()));
        SeedMotes();
        break;
    case k68Clear:
    case k68Find:
    case k68Init:
    case k68Step:
    case k68Mote:
    case k68MoteGlow:
    case k68MoteHold:
    case k68MoteRise: SeedMotes(); break;
    default: break;
    }
}

// The helpers with arguments: the record's own points (as the states pass
// them), a pool record, the size, colour and shades over leftover upper bytes.
void Args(unsigned k, U* a) {
    const U s = Key(Sprite_Current);
    switch (k) {
    case k60Line:
        a[0] = s + 0x34;
        a[1] = s + 0xC;
        break;
    case k62Ray: a[0] = Key(RayAt(sh::Next() % at::kRays)); break;
    case k62Ring:
        a[0] = s + 0x34;
        a[1] = PickOf(0, 1, 0xA000, 0x100, sh::Next() & 0xFFFF, sh::Next());
        a[2] = PickOf(0, 1, 0xB000, 0x200, sh::Next() & 0xFFFF, sh::Next());
        break;
    case k64Glow:
        a[0] = s + 0x64;
        a[1] = (sh::Next() & 0xFFFF0000u) | PickOf(0, 0x20, 0x100, 0x7FFF, 0x8000, 0xFFFA, sh::Next() & 0xFFFF);
        a[2] = PickOf(6, 6, 0, 1, 3, 0xFF, sh::Next());
        break;
    case k64Trail:
        a[0] = s + 0x18;
        a[1] = s + 0x34;
        a[2] = (sh::Next() & 0xFFFFFF00u) | PickOf(0x40, 0x80, 0, 0xFF, sh::Next() & 0xFF);
        break;
    case k64Spark: a[0] = Key(SparkAt(sh::Next() % at::kSparks)); break;
    case k64InitShard: a[0] = Key(ShardAt(sh::Next() % at::kShardCount64)); break;
    case k68Init:
    case k68Mote:
    case k68MoteGlow:
    case k68MoteHold:
    case k68MoteRise: a[0] = Key(MoteAt(sh::Next() % at::kMotes)); break;
    case k68Wall2: a[0] = (sh::Next() & 0xFFFFFF00u) | PickOf(0, 0xFF, 0x80, 1, sh::Next() & 0xFF); break;
    default: break;
    }
}

// What the functions read again after a call, moved by the group's case of the
// harness's disturbance (from its hash only): the record's counts and frames,
// the shade, the particles' count (inside the seeded 0x100), the chapter's
// count, the motes' counts.
void Disturb(U h) {
    const U v = h >> 8;
    unsigned char* const s = Sprite_Current;
    switch (h % 8) {
    case 0:
        if (sh::InRegions(s, 0x80)) s[9] = static_cast<unsigned char>((v & 1) ? 1u : v >> 1);
        break;
    case 1:
        if (sh::InRegions(s, 0x80)) s[6] = static_cast<unsigned char>((v & 1) ? 0u : v >> 1);
        break;
    case 2:
        if (sh::InRegions(s, 0x80)) s[0xA] = static_cast<unsigned char>(v);
        break;
    case 3:
        if (sh::InRegions(s, 0x80)) SetWord(s + 0x2E, (v & 1) ? 1u : v >> 1);
        break;
    case 4:
        if (sh::InRegions(s, 0x80)) SetWord(s + 0x30, (v & 1) ? 0x5Au : v >> 1);
        break;
    case 5:
        if (sh::InRegions(s, 0x80)) s[3] = static_cast<unsigned char>(v);
        break;
    case 6: SetWord(sh::Mem(at::kParticleCount), v % (kParticlesSeeded + 1)); break;
    case 7: sh::Mem(at::kCounter)[0] = static_cast<unsigned char>((v & 1) ? 0x18u : v >> 1); break;
    default: break;
    }
}

}  // namespace

void SelfTest() {
    // BOF3X_E3A_ONLY: the clones whose name contains it (a control's run)
    static sh::Clone chosen[kCount];
    static unsigned index[kCount];
    const char* const only = std::getenv("BOF3X_E3A_ONLY");
    unsigned n = 0;
    for (unsigned k = 0; k < kCount; ++k)
        if (!only || !*only || std::strstr(kAll[k].name, only)) {
            index[n] = k;
            chosen[n++] = kAll[k];
        }
    if (n == 0) bof3::Fatal("effect_3a: BOF3X_E3A_ONLY=%s names no clone", only);
    static unsigned* s_index = index;
    sh::Group g = {"effect_3a", chosen, n, kCallees, sizeof kCallees / sizeof kCallees[0], kTables,
                   sizeof kTables / sizeof kTables[0], kRegions, sizeof kRegions / sizeof kRegions[0],
                   [](unsigned k) { Seed(s_index[k]); }, &Disturb, 4000};
    if (const char* r = std::getenv("BOF3X_E3A_ROUNDS")) g.rounds = static_cast<unsigned>(std::atoi(r));
    g.args = [](unsigned k, U* a) { Args(s_index[k], a); };
    g.effect = true;
    g.kinds = kKinds;
    g.n_kinds = sizeof kKinds;
    sh::Run(g);
    g_glow = false;
}

}  // namespace effect_3a

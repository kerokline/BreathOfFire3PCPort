// BOF3X_SHADOW=effect_2d: group E2D's 54 functions through the scenario harness
// in effect mode (scenario_harness.h, docs/scenario_harness.md section 8), once
// at start-up. docs/effect_2d.md section 4. BOF3X_E2D_ONLY=<name> runs the
// clones whose name contains it (the controls' speed-up).
//
// The clone table is tools/band_rows.py --group E2D --clones --harness
// scenario (2026-09-29), each extent read again to its last instruction
// (capstone) and the names given. Shapes: every dispatcher, state and
// argument-less helper kEffect (Sprite_Current one of the 20 Effect_Objects
// records, +5 the kind, a dispatcher's +1 or +2 below its table's length); the
// helpers that take arguments kCall.
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_2d.h"
#include "game/effect_2d_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace effect_2d {
namespace {

namespace sh = scenario_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using U = std::uint32_t;

// tools/band_rows.py --group E2D --clones, 2026-09-29.
constexpr sh::CallSite kCalls4771D0[] = {{0x5D, 0x57C140}, {0x6B, 0x587B40}, {0x75, 0x587740}};
constexpr sh::CallSite kCalls477250[] = {{0xA, 0x477500}, {0x44, 0x57C140}, {0xBA, 0x4777A0}, {0xD0, 0x477820}};
constexpr sh::CallSite kCalls477370[] = {{0x1F, 0x477500}, {0x79, 0x4777A0}, {0x92, 0x587740}, {0xA9, 0x477820}, {0xE6, 0x57C0F0}, {0x101, 0x57C110}};
constexpr sh::CallSite kCalls4774A0[] = {{0x8, 0x57C140}, {0x1D, 0x57C140}, {0x35, 0x587740}, {0x46, 0x57C110}, {0x4E, 0x589840}};
constexpr sh::CallSite kCalls477500[] = {{0x6, 0x477760},   {0x2E, 0x477B20},  {0x35, 0x477760},  {0x55, 0x477B20},  {0x5C, 0x477760},
                                         {0x7D, 0x4776E0},  {0x89, 0x4776E0},  {0x9E, 0x4776E0},  {0xAD, 0x4776E0},  {0xB4, 0x477760},
                                         {0xC0, 0x4776E0},  {0xCC, 0x4776E0},  {0xDE, 0x4776E0},  {0xED, 0x4776E0},  {0xF4, 0x477760},
                                         {0x135, 0x4776E0}, {0x15A, 0x4776E0}, {0x192, 0x4776E0}, {0x19E, 0x4776E0}, {0x1B0, 0x4776E0},
                                         {0x1BC, 0x4776E0}};
constexpr sh::CallSite kCalls4776E0[] = {{0x8, 0x5A7650}, {0x10, 0x5A7780}, {0x73, 0x461E50}};
constexpr sh::CallSite kCalls477760[] = {{0x15, 0x5A79A0}, {0x2E, 0x5A77C0}, {0x37, 0x461E50}};
constexpr sh::CallSite kCalls4777A0[] = {{0x52, 0x5A7A00}};
constexpr sh::CallSite kCalls477820[] = {{0x2C, 0x477760}, {0x5F, 0x477AC0}, {0x73, 0x4778A0}};
constexpr sh::CallSite kCalls4778A0[] = {{0x57, 0x477940}, {0x62, 0x477AC0}};
constexpr sh::CallSite kCalls477940[] = {{0xAB, 0x477AC0}, {0x137, 0x477AC0}};
constexpr sh::CallSite kCalls477AC0[] = {{0x8, 0x5A7750}, {0x10, 0x5A7780}, {0x47, 0x461E50}};
constexpr sh::CallSite kCalls477B20[] = {{0x8, 0x5A7740}, {0x10, 0x5A7780}, {0x73, 0x461E50}};
constexpr sh::CallSite kCalls477BC0[] = {{0x39, 0x477D20}, {0x3E, 0x477DB0}};
constexpr sh::CallSite kCalls477C10[] = {{0x47, 0x477D20}, {0x4C, 0x477DB0}};
constexpr sh::CallSite kCalls477C70[] = {{0x47, 0x477D20}, {0x4C, 0x477DB0}};
constexpr sh::CallSite kCalls477CD0[] = {{0x33, 0x477D20}, {0x38, 0x477DB0}};
constexpr sh::CallSite kCalls477D10[] = {{0x0, 0x477D20}, {0x5, 0x477DB0}};
constexpr sh::CallSite kCalls477D20[] = {{0xF, 0x5A77C0}, {0x18, 0x461E50}, {0x24, 0x5A7740}, {0x6B, 0x5A7780}, {0x73, 0x5A77A0}, {0x7C, 0x461E50}};
constexpr sh::CallSite kCalls477DB0[] = {{0x23, 0x588F20}, {0x42, 0x588F20}};
constexpr sh::CallSite kCalls477E30[] = {{0x2, 0x57CD90},   {0x34, 0x57CD90},  {0xD3, 0x57A010},  {0xFB, 0x589200},
                                         {0x11E, 0x57A010}, {0x12C, 0x589200}, {0x250, 0x454CC0}, {0x270, 0x454CC0}};
constexpr sh::CallSite kCalls478160[] = {{0x19, 0x454DC0}, {0x37, 0x454DC0}, {0x3F, 0x589840}};
constexpr sh::CallSite kCalls4781D0[] = {{0x35, 0x478340}, {0x3F, 0x587740}, {0x50, 0x589840}};
constexpr sh::CallSite kCalls478230[] = {{0xC, 0x478290}, {0x35, 0x478360}};
constexpr sh::CallSite kCalls478280[] = {{0x0, 0x478360}, {0x9, 0x589840}};
constexpr sh::CallSite kCalls478290[] = {{0x11, 0x478320}, {0x2D, 0x5A7A50}, {0x41, 0x5A7A00}};
constexpr sh::CallSite kCalls478360[] = {{0x5, 0x494060}, {0x1F, 0x4783C0}};
constexpr sh::CallSite kCalls4783C0[] = {{0x15, 0x5A79A0}, {0x2D, 0x5A77C0}, {0x36, 0x461E50},  {0x42, 0x5A75D0},  {0x4A, 0x5A7780},
                                         {0x8D, 0x4941E0}, {0x9C, 0x494110}, {0x123, 0x5A79E0}, {0x13D, 0x5A79A0}, {0x17C, 0x461E50}};
constexpr sh::CallSite kCalls478590[] = {{0x0, 0x4790C0}};
constexpr sh::CallSite kCalls4785B0[] = {{0xB, 0x47CF20}, {0x15, 0x4790F0}, {0x1D, 0x479260}};
constexpr sh::CallSite kCalls4785F0[] = {{0x0, 0x479260}, {0x9, 0x589840}};
constexpr sh::CallSite kCalls478640[] = {{0x9, 0x40F9E0}, {0x38, 0x589840}};
constexpr sh::CallSite kCalls4786A0[] = {{0x57, 0x587740}};
constexpr sh::CallSite kCalls478720[] = {{0x5, 0x4794D0}, {0xF, 0x4796B0}};
constexpr sh::CallSite kCalls4787A0[] = {{0x5, 0x4794D0}, {0xF, 0x4796B0}, {0x35, 0x587740}};
constexpr sh::CallSite kCalls4787F0[] = {{0x5, 0x4794D0}, {0xF, 0x4796B0}};
constexpr sh::CallSite kCalls478840[] = {{0x5, 0x4794D0}, {0xF, 0x4796B0}};
constexpr sh::CallSite kCalls4788A0[] = {{0xD, 0x5720C0}, {0x21, 0x47A110}, {0x49, 0x587740}};
constexpr sh::CallSite kCalls478900[] = {{0x3F, 0x47A130}, {0x49, 0x47A150}, {0x71, 0x47A200}};
constexpr sh::CallSite kCalls4789A0[] = {{0x0, 0x47A200}, {0x9, 0x589840}};

#define E2D_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define E2D_CALLS(a) a, E2D_N(a)
#define E2D_FN(name) reinterpret_cast<const void*>(&::name)
constexpr sh::Shape kEf = sh::Shape::kEffect;
constexpr sh::Shape kCa = sh::Shape::kCall;
constexpr U kAnswerAll = 0xFFFFFFFFu;
// {name, base, size, calls, imms, tables, ours, ret_mask, calm, shape, pointers, state_span, sub_span, kind}
const sh::Clone kAll[] = {
    {"EffectKind45_Run", 0x4771B0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E2D_FN(EffectKind45_Run), 0, false, kEf, 0, 4, 0, 0x45},
    {"EffectKind45_Start", 0x4771D0, 0x7E, E2D_CALLS(kCalls4771D0), nullptr, 0, nullptr, 0, E2D_FN(EffectKind45_Start), 0, false, kEf, 0, 0, 0, 0x45},
    {"EffectKind45_Tune", 0x477250, 0x116, E2D_CALLS(kCalls477250), nullptr, 0, nullptr, 0, E2D_FN(EffectKind45_Tune), 0, false, kEf, 0, 0, 0, 0x45},
    {"EffectKind45_Aim", 0x477370, 0x127, E2D_CALLS(kCalls477370), nullptr, 0, nullptr, 0, E2D_FN(EffectKind45_Aim), 0, false, kEf, 0, 0, 0, 0x45},
    {"EffectKind45_End", 0x4774A0, 0x53, E2D_CALLS(kCalls4774A0), nullptr, 0, nullptr, 0, E2D_FN(EffectKind45_End), 0, false, kEf, 0, 0, 0, 0x45},
    {"EffectKind45_DrawPanel", 0x477500, 0x1DB, E2D_CALLS(kCalls477500), nullptr, 0, nullptr, 0, E2D_FN(EffectKind45_DrawPanel), 0, false, kCa, 0, 0, 0, 0x45},
    {"EffectKind45_DrawLine", 0x4776E0, 0x7D, E2D_CALLS(kCalls4776E0), nullptr, 0, nullptr, 0, E2D_FN(EffectKind45_DrawLine), 0, false, kCa, 0, 0, 0, 0x45},
    {"EffectKind45_DrawMode", 0x477760, 0x40, E2D_CALLS(kCalls477760), nullptr, 0, nullptr, 0, E2D_FN(EffectKind45_DrawMode), 0, false, kCa, 0, 0, 0, 0x45},
    {"EffectKind45_PushSample", 0x4777A0, 0x75, E2D_CALLS(kCalls4777A0), nullptr, 0, nullptr, 0, E2D_FN(EffectKind45_PushSample), 0, false, kCa, 0, 0, 0, 0x45},
    {"EffectKind45_DrawTrace", 0x477820, 0x7C, E2D_CALLS(kCalls477820), nullptr, 0, nullptr, 0, E2D_FN(EffectKind45_DrawTrace), 0, false, kCa, 0, 0, 0, 0x45},
    {"EffectKind45_DrawPoints", 0x4778A0, 0x9D, E2D_CALLS(kCalls4778A0), nullptr, 0, nullptr, 0, E2D_FN(EffectKind45_DrawPoints), 0, false, kCa, 0, 0, 0, 0x45},
    {"EffectKind45_DrawSegment", 0x477940, 0x175, E2D_CALLS(kCalls477940), nullptr, 0, nullptr, 0, E2D_FN(EffectKind45_DrawSegment), 0, false, kCa, 0, 0, 0, 0x45},
    {"EffectKind45_Plot", 0x477AC0, 0x51, E2D_CALLS(kCalls477AC0), nullptr, 0, nullptr, 0, E2D_FN(EffectKind45_Plot), 0, false, kCa, 0, 0, 0, 0x45},
    {"EffectKind45_FillRect", 0x477B20, 0x7D, E2D_CALLS(kCalls477B20), nullptr, 0, nullptr, 0, E2D_FN(EffectKind45_FillRect), 0, false, kCa, 0, 0, 0, 0x45},
    {"EffectKind46_Run", 0x477BA0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E2D_FN(EffectKind46_Run), 0, false, kEf, 0, 5, 0, 0x46},
    {"EffectKind46_Start", 0x477BC0, 0x43, E2D_CALLS(kCalls477BC0), nullptr, 0, nullptr, 0, E2D_FN(EffectKind46_Start), 0, false, kEf, 0, 0, 0, 0x46},
    {"EffectKind46_FadeIn", 0x477C10, 0x51, E2D_CALLS(kCalls477C10), nullptr, 0, nullptr, 0, E2D_FN(EffectKind46_FadeIn), 0, false, kEf, 0, 0, 0, 0x46},
    {"EffectKind46_FadeOut", 0x477C70, 0x51, E2D_CALLS(kCalls477C70), nullptr, 0, nullptr, 0, E2D_FN(EffectKind46_FadeOut), 0, false, kEf, 0, 0, 0, 0x46},
    {"EffectKind46_Count", 0x477CD0, 0x3D, E2D_CALLS(kCalls477CD0), nullptr, 0, nullptr, 0, E2D_FN(EffectKind46_Count), 0, false, kEf, 0, 0, 0, 0x46},
    {"EffectKind46_Hold", 0x477D10, 0xA, E2D_CALLS(kCalls477D10), nullptr, 0, nullptr, 0, E2D_FN(EffectKind46_Hold), 0, false, kEf, 0, 0, 0, 0x46},
    {"EffectKind46_DrawFlash", 0x477D20, 0x86, E2D_CALLS(kCalls477D20), nullptr, 0, nullptr, 0, E2D_FN(EffectKind46_DrawFlash), 0, false, kEf, 0, 0, 0, 0x46},
    {"EffectKind46_RedrawSprites", 0x477DB0, 0x51, E2D_CALLS(kCalls477DB0), nullptr, 0, nullptr, 0, E2D_FN(EffectKind46_RedrawSprites), 0, false, kEf, 0, 0, 0, 0x46},
    {"EffectKind47_Run", 0x477E10, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E2D_FN(EffectKind47_Run), 0, false, kEf, 0, 3, 0, 0x47},
    {"EffectKind47_Start", 0x477E30, 0x291, E2D_CALLS(kCalls477E30), nullptr, 0, nullptr, 0, E2D_FN(EffectKind47_Start), 0, false, kEf, 0, 0, 0, 0x47},
    {"EffectKind47_Blink", 0x4780D0, 0x84, nullptr, 0, nullptr, 0, nullptr, 0, E2D_FN(EffectKind47_Blink), 0, false, kEf, 0, 0, 0, 0x47},
    {"EffectTwinSprites_Release", 0x478160, 0x44, E2D_CALLS(kCalls478160), nullptr, 0, nullptr, 0, E2D_FN(EffectTwinSprites_Release), 0, false, kEf, 0, 0, 0, 0x47},
    {"EffectKind48_Run", 0x4781B0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E2D_FN(EffectKind48_Run), 0, false, kEf, 0, 3, 0, 0x48},
    {"EffectKind48_Start", 0x4781D0, 0x55, E2D_CALLS(kCalls4781D0), nullptr, 0, nullptr, 0, E2D_FN(EffectKind48_Start), 0, false, kEf, 0, 0, 0, 0x48},
    {"EffectKind48_Burst", 0x478230, 0x4D, E2D_CALLS(kCalls478230), nullptr, 0, nullptr, 0, E2D_FN(EffectKind48_Burst), 0, false, kEf, 0, 0, 0, 0x48},
    {"EffectKind48_Fade", 0x478280, 0xF, E2D_CALLS(kCalls478280), nullptr, 0, nullptr, 0, E2D_FN(EffectKind48_Fade), 0, false, kEf, 0, 0, 0, 0x48},
    {"EffectKind48_SpawnRing", 0x478290, 0x84, E2D_CALLS(kCalls478290), nullptr, 0, nullptr, 0, E2D_FN(EffectKind48_SpawnRing), 0, false, kEf, 0, 0, 0, 0x48},
    {"EffectKind48_FindFreeSpark", 0x478320, 0x1B, nullptr, 0, nullptr, 0, nullptr, 0, E2D_FN(EffectKind48_FindFreeSpark), kAnswerAll, false, kEf, 0, 0, 0, 0x48},
    {"EffectKind48_ClearSparks", 0x478340, 0x14, nullptr, 0, nullptr, 0, nullptr, 0, E2D_FN(EffectKind48_ClearSparks), 0, false, kEf, 0, 0, 0, 0x48},
    {"EffectKind48_StepSparks", 0x478360, 0x56, E2D_CALLS(kCalls478360), nullptr, 0, nullptr, 0, E2D_FN(EffectKind48_StepSparks), 0xFF, false, kEf, 0, 0, 0, 0x48},
    {"EffectKind48_DrawSpark", 0x4783C0, 0x18A, E2D_CALLS(kCalls4783C0), nullptr, 0, nullptr, 0, E2D_FN(EffectKind48_DrawSpark), 0, false, kCa, 0, 0, 0, 0x48},
    {"EffectKind49_Run", 0x478550, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E2D_FN(EffectKind49_Run), 0, false, kEf, 0, 10, 0, 0x49},
    {"EffectKind49_V0Run", 0x478570, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E2D_FN(EffectKind49_V0Run), 0, false, kEf, 0, 0, 3, 0x49},
    {"EffectKind49_V0Start", 0x478590, 0x1A, E2D_CALLS(kCalls478590), nullptr, 0, nullptr, 0, E2D_FN(EffectKind49_V0Start), 0, false, kEf, 0, 0, 0, 0x49},
    {"EffectKind49_V0Emit", 0x4785B0, 0x3E, E2D_CALLS(kCalls4785B0), nullptr, 0, nullptr, 0, E2D_FN(EffectKind49_V0Emit), 0, false, kEf, 0, 0, 0, 0x49},
    {"EffectKind49_V0End", 0x4785F0, 0xF, E2D_CALLS(kCalls4785F0), nullptr, 0, nullptr, 0, E2D_FN(EffectKind49_V0End), 0, false, kEf, 0, 0, 0, 0x49},
    {"EffectKind49_V1Run", 0x478600, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E2D_FN(EffectKind49_V1Run), 0, false, kEf, 0, 0, 2, 0x49},
    {"EffectKind49_V1Start", 0x478620, 0x15, nullptr, 0, nullptr, 0, nullptr, 0, E2D_FN(EffectKind49_V1Start), 0, false, kEf, 0, 0, 0, 0x49},
    {"EffectKind49_V1Fade", 0x478640, 0x3E, E2D_CALLS(kCalls478640), nullptr, 0, nullptr, 0, E2D_FN(EffectKind49_V1Fade), 0, false, kEf, 0, 0, 0, 0x49},
    {"EffectKind49_V2Run", 0x478680, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E2D_FN(EffectKind49_V2Run), 0, false, kEf, 0, 0, 8, 0x49},
    {"EffectKind49_V2Launch", 0x4786A0, 0x77, E2D_CALLS(kCalls4786A0), nullptr, 0, nullptr, 0, E2D_FN(EffectKind49_V2Launch), 0, false, kEf, 0, 0, 0, 0x49},
    {"EffectKind49_V2Fly", 0x478720, 0x73, E2D_CALLS(kCalls478720), nullptr, 0, nullptr, 0, E2D_FN(EffectKind49_V2Fly), 0, false, kEf, 0, 0, 0, 0x49},
    {"EffectKind49_V2Wait", 0x4787A0, 0x50, E2D_CALLS(kCalls4787A0), nullptr, 0, nullptr, 0, E2D_FN(EffectKind49_V2Wait), 0, false, kEf, 0, 0, 0, 0x49},
    {"EffectKind49_V2Rise", 0x4787F0, 0x45, E2D_CALLS(kCalls4787F0), nullptr, 0, nullptr, 0, E2D_FN(EffectKind49_V2Rise), 0, false, kEf, 0, 0, 0, 0x49},
    {"EffectKind49_V2Sink", 0x478840, 0x3C, E2D_CALLS(kCalls478840), nullptr, 0, nullptr, 0, E2D_FN(EffectKind49_V2Sink), 0, false, kEf, 0, 0, 0, 0x49},
    {"EffectKind49_V3Run", 0x478880, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E2D_FN(EffectKind49_V3Run), 0, false, kEf, 0, 0, 3, 0x49},
    {"EffectKind49_V3Start", 0x4788A0, 0x52, E2D_CALLS(kCalls4788A0), nullptr, 0, nullptr, 0, E2D_FN(EffectKind49_V3Start), 0, false, kEf, 0, 0, 0, 0x49},
    {"EffectKind49_V3Burst", 0x478900, 0x92, E2D_CALLS(kCalls478900), nullptr, 0, nullptr, 0, E2D_FN(EffectKind49_V3Burst), 0, false, kEf, 0, 0, 0, 0x49},
    {"EffectKind49_V3End", 0x4789A0, 0xF, E2D_CALLS(kCalls4789A0), nullptr, 0, nullptr, 0, E2D_FN(EffectKind49_V3End), 0, false, kEf, 0, 0, 0, 0x49},
    {"EffectKind49_V4Run", 0x4789B0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E2D_FN(EffectKind49_V4Run), 0, false, kEf, 0, 0, 4, 0x49},
};
#undef E2D_FN
#undef E2D_CALLS
#undef E2D_N

enum : unsigned {
    k45Run, k45Start, k45Tune, k45Aim, k45End, k45Panel, k45Line, k45Mode, k45Sample, k45Trace, k45Points, k45Segment, k45Plot,
    k45Fill, k46Run, k46Start, k46FadeIn, k46FadeOut, k46Count, k46Hold, k46Flash, k46Redraw, k47Run, k47Start, k47Blink,
    kTwinRelease, k48Run, k48Start, k48Burst, k48Fade, k48Ring, k48FindSpark, k48Clear, k48Step, k48Draw, k49Run, k49V0Run,
    k49V0Start, k49V0Emit, k49V0End, k49V1Run, k49V1Start, k49V1Fade, k49V2Run, k49V2Launch, k49V2Fly, k49V2Wait, k49V2Rise,
    k49V2Sink, k49V3Run, k49V3Start, k49V3Burst, k49V3End, k49V4Run, kCount
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
// A float with a random mantissa and an exponent 2^-17..2^18 (no NaN, no
// infinity): what the projections' screen words and depths are - with
// fractions, so the corners' rounding shows.
void FillFloat(U at) {
    if (!Writable(at, 4)) return;
    const U n = sh::Noise();
    const U bits = (n & 0x807FFFFFu) | ((0x6Eu + (sh::Noise() % 0x24u)) << 23);
    std::memcpy(P(at), &bits, 4);
}
// EffectGte_ProjectPoint: out[0..2] the screen x, y and depth (floats), all read.
U FxProjectPoint(const U* a, U answer) {
    for (unsigned i = 0; i < 3; ++i) FillFloat(a[1] + 4 * i);
    return answer;
}
// EffectGte_ProjectSize: out's two s16 (the first is the half-size the corners use).
U FxProjectSize(const U* a, U answer) {
    if (Writable(a[2], 4)) sh::FillBytes(P(a[2]), 4);
    return answer;
}
// EffectKind48_FindFreeSpark, as the real one: the first spark whose +0x15 is 0,
// or null - its callers write through it.
U FxFindSpark(const U*, U) {
    for (unsigned i = 0; i < at::kSparks; ++i) {
        unsigned char* const s = EffectKind30_Shards + i * at::kSparkStride;
        if (s[0x15] == 0) return Key(s);
    }
    return 0;
}

#define E2D_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
constexpr sh::Answer kG = sh::Answer::kGarbage, kF = sh::Answer::kFlag, kPh = sh::Answer::kPhase;
constexpr U kW = 0xFFFFFFFFu, k16 = 0xFFFFu, k8 = 0xFFu;
const sh::Callee kCallees[] = {
    // the group's own called directly, by name (docs/effect_2d.md section 3):
    // the coordinates read as s16 (movsx), the callers pushing whole registers
    // with leftovers above; the shades and blends as bytes
    {E2D_OURS(EffectKind45_DrawPanel), 2, {kW, kW}, kG, 0, 0},
    {E2D_OURS(EffectKind45_DrawLine), 4, {k16, k16, k16, k16}, kG, 0, 0},
    {E2D_OURS(EffectKind45_DrawMode), 1, {k8}, kG, 0, 0},
    {E2D_OURS(EffectKind45_PushSample), 6, {k16, k16, k16, k16, k16, k16}, kG, 0, 0},
    {E2D_OURS(EffectKind45_DrawTrace), 2, {kW, kW}, kG, 0, 0},
    // the clip is the caller's stack (not logged; its four s16 hashed)
    {E2D_OURS(EffectKind45_DrawPoints), 3, {k8, kW, 0}, kG, 0, 0, {0, 0, 8}, nullptr, nullptr, true},
    {E2D_OURS(EffectKind45_DrawSegment), 3, {kW, kW, k8}, kG, 0, 0},
    // the point is the history (fixed) or the segment's stepping point (the
    // caller's stack): its two s16 hashed, the pointer not logged
    {E2D_OURS(EffectKind45_Plot), 2, {0, k8}, kG, 0, 0, {4, 0}, nullptr, nullptr, true},
    {E2D_OURS(EffectKind45_FillRect), 4, {k16, k16, k16, k16}, kG, 0, 0},
    {E2D_OURS(EffectKind46_DrawFlash), 0, {}, kPh, 0, 0},
    {E2D_OURS(EffectKind46_RedrawSprites), 0, {}, kPh, 0, 0},
    {E2D_OURS(EffectKind48_SpawnRing), 0, {}, kPh, 0, 0},
    {E2D_OURS(EffectKind48_ClearSparks), 0, {}, kPh, 0, 0},
    {E2D_OURS(EffectKind48_FindFreeSpark), 0, {}, kG, 0, 0, {}, &FxFindSpark},
    {E2D_OURS(EffectKind48_StepSparks), 0, {}, kF, 0, 0},
    {E2D_OURS(EffectKind48_DrawSpark), 1, {kW}, kG, 0, 0, {0x18}, nullptr, nullptr, true},
    // standard entries re-listed louder: Sprite_UpdateScreen logs the
    // Sprite_Current it runs on (the redraw's whole point); Sprite_ReleaseTint's
    // record address logged (it is compared, not read, by the real one)
    {E2D_OURS(Sprite_UpdateScreen), 0, {}, kPh, 0, 0},
    {E2D_OURS(Sprite_ReleaseTint), 1, {kW}, kG, 0, 0, {16}, nullptr, nullptr, true},
    // the stack points and outs never logged, the outs filled; ProjectSize's
    // second size word is the original's uninitialised stack (not hashed)
    {E2D_OURS(EffectGte_ProjectPoint), 2, {0, 0}, kG, 0, 0, {12, 0}, &FxProjectPoint, nullptr, true},
    {E2D_OURS(EffectGte_ProjectSize), 3, {0, 0, 0}, kG, 0, 0, {12, 2, 0}, &FxProjectSize, nullptr, true},
};
#undef E2D_OURS

// The ten state tables the dispatchers jump through, read in place; each
// table's length to the next table a dispatcher names (none bounded by a compare).
const sh::DataTable kTables[] = {
    {0x65450C, 4}, {0x654520, 5}, {0x654534, 3}, {0x654578, 3}, {0x654584, 10},
    {0x6545AC, 3}, {0x6545B8, 2}, {0x6545C0, 8}, {0x6545E0, 3}, {0x6545FC, 4},
};
const std::uint8_t kKinds[] = {0x45, 0x46, 0x47, 0x48, 0x49};
// The panel's colour bytes and the dust anchor pointer (0x6761C4..0x6761D3);
// the word the anchor points at (0x92D1C8.., where 0x4789D0 aims it).
const sh::Region kRegions[] = {{at::kPanelColour, 0x10}, {0x92D1C8, 0x20}};

// --- the seed ------------------------------------------------------------------------

unsigned char* Rec(unsigned k) { return sh::EffectRecord(k); }
// Every record's sprite indices +3 / +4 inside the pool: the states read them
// from whichever record Sprite_Current is after a call (the harness moves it).
void SpriteIndices() {
    for (unsigned r = 0; r < at::kEffects; ++r) {
        Rec(r)[3] = static_cast<unsigned char>(sh::Next() % at::kSprites);
        Rec(r)[4] = static_cast<unsigned char>(sh::Next() % at::kSprites);
    }
}
unsigned char* Spark(unsigned i) { return EffectKind30_Shards + i * at::kSparkStride; }
// The sparks' live bytes: all free, all live, or mixed; lives at 1 (to 0) and around.
void Sparks() {
    const U mode = sh::Next() % 3;
    for (unsigned i = 0; i < at::kSparks; ++i) {
        Spark(i)[0x15] = static_cast<unsigned char>(mode == 0 ? 0 : mode == 1 ? (sh::Next() | 1) : (sh::Half() ? 0 : sh::Next()));
        Spark(i)[0x16] = static_cast<unsigned char>(PickOf(0, 1, 2, 8, sh::Next()));
    }
}
// The history's points (x, y s16) near the clip (x 0x80..0xBF, the columns)
// and their neighbours, y close enough that a segment stays short.
void History(unsigned char* at, unsigned n) {
    for (unsigned i = 0; i < n; ++i) {
        SetWord(at + 4 * i, PickOf(0x7F, 0x80, 0x81, 0xA0, 0xBE, 0xBF, 0xC0, 0xC1, 0x80 + sh::Next() % 0x40, sh::Next()));
        SetWord(at + 4 * i + 2, 0x20 + sh::Next() % 0x40);
    }
}

void Seed(unsigned k) {
    unsigned char* const s = Sprite_Current;
    s[9] = static_cast<unsigned char>(PickOf(0, 1, 2, 3, 4, 5, 0xC, sh::Next()));
    const unsigned short buttons = static_cast<unsigned short>(PickOf(0, Field_ConfirmButtons, Field_CancelButtons, sh::Next()));
    switch (k) {
    case k45Tune:
        SetWord(s + 0x2E, PickOf(0, 0x46, 0x47, 0x48, 0x7FFF, 0xFFFF, sh::Next()));
        SetWord(s + 0x30, PickOf(0x1FF, 0x200, 0x20F, 0x210, 0x211, 0x220, 0x8000, sh::Next()));
        SetWord(s + 0x32, PickOf(0x16, 0x17, 0x18, 0x19, 0x7FFF, sh::Next()));
        Input_Pressed = buttons;
        break;
    case k45Aim:
        SetWord(s + 0x2E, PickOf(0x1B, 0x1C, 0x1D, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x46, 0x47, 1, 2, 0xFFFF, sh::Next()));
        SetWord(s + 0x36, PickOf(0, 7, 8, 9, 0x10, 0x108, sh::Next()));
        Input_Pressed = buttons;
        break;
    case k45Panel:
    case k45Trace:
        s[0x5D] = static_cast<unsigned char>(PickOf(0, 0x10, 0x80, 0xF, sh::Next()));
        History(EffectKind30_Shards, 2);
        break;
    case k45Points:
        // the clip in argument 2's scratch: x 0x80, y 0x20, 0x40 square (or any)
        History(EffectKind30_Shards, 0x80);
        if (sh::Often()) {
            unsigned char* const c = sh::Scratch(2);
            SetWord(c, 0x80);
            SetWord(c + 2, 0x20);
            SetWord(c + 4, PickOf(0x40, 0x41, 0x3F, 0, 0xFFC0, sh::Next()));
            SetWord(c + 6, 0x40);
        }
        break;
    case k46FadeIn:
    case k46FadeOut:
        s[9] = static_cast<unsigned char>(PickOf(0, 1, 2, 0xC, sh::Next()));
        break;
    case k46Count:
        s[2] = static_cast<unsigned char>(PickOf(3, 4, 5, 6, 0xFF, sh::Next()));
        break;
    case k47Start:
    case k47Blink:
        SpriteIndices();
        s[0xB] = static_cast<unsigned char>(PickOf(0, 1, 0, 1, sh::Next()));
        s[9] = static_cast<unsigned char>(PickOf(0, 1, 2, 4, sh::Next()));
        if (sh::Often()) {   // the model's animation byte one of the five keys (or not)
            const unsigned char key = move_script::At(at::kTwinAnims + 3 * (sh::Next() % 5))[0];
            ObjTrio[0x4B] = key;
            move_script::At(at::kObject2)[0x4B] = sh::Half() ? key : static_cast<unsigned char>(sh::Next());
        }
        break;
    case k48Start:
        // half the time no other live kind-0x48 record
        if (sh::Half())
            for (unsigned r = 0; r < at::kEffects; ++r)
                if (Rec(r) != s && Rec(r)[5] == 0x48) Rec(r)[5] = 0x47;
        break;
    case k48Burst:
        s[9] = static_cast<unsigned char>(PickOf(0, 3, 4, 5, sh::Next()));
        if (sh::Half()) s[6] = static_cast<unsigned char>(s[0xB] - (sh::Half() ? 1 : 0));
        break;
    case k48Ring:
    case k48FindSpark:
    case k48Step:
    case k48Fade:
        Sparks();
        break;
    case k49V0Emit:
        SetLong(s + 0xC, static_cast<std::int32_t>(PickOf(0, 1, 2, 8, 9, 0x40, 0x41, sh::Next())));
        break;
    case k49V1Fade:
        SetLong(s + 0x10, static_cast<std::int32_t>(PickOf(0xFFFFFFFF, 0, 1, 0xFFFFFFF8, sh::Next())));
        break;
    case k49V2Fly:
        // at the target after the step half the time; the speed 0 a third
        if (sh::Half()) SetLong(s + 0xC, Long(s + 0x34) - 0x10000);
        if (sh::Half()) SetLong(s + 0x10, Long(s + 0x38));
        if (sh::Next() % 3 == 0) SetLong(s + 0x14, 0);
        break;
    case k49V3Burst:
        // the step +9 inside the byte tables' eight (every record: the count is
        // read again by the current record's +9 after each start), the count's
        // low byte against the masks, and the anchor at the word the harness compares
        for (unsigned r = 0; r < at::kEffects; ++r) Rec(r)[9] = static_cast<unsigned char>(sh::Next() % 8);
        SetLong(s + 0xC, static_cast<std::int32_t>(PickOf(0, 1, 2, 3, 0x10, 0x11, 0x5F, 0x50, sh::Next())));
        SetLong(move_script::At(at::kDustAnchor), static_cast<std::int32_t>(0x92D1C8 + sh::Next() % 0xE));
        break;
    default: break;
    }
}

void Args(unsigned k, U* a) {
    switch (k) {
    case k45Trace:
        // x so that the newest sample's x lies on, in and past the clip's columns
        a[0] = (a[0] & 0xFFFF0000u) | ((Word(EffectKind30_Shards) - 4u - PickOf(0, 1, 0x20, 0x3F, 0x40, 0x41, 0xFFFF, sh::Next())) & 0xFFFFu);
        break;
    case k45Points:
        a[1] = Key(EffectKind30_Shards + 4 * (sh::Next() % 0x40));
        a[2] = Key(sh::Scratch(2));
        if (sh::Often()) a[0] = (a[0] & 0xFFFFFF00u) | (sh::Next() % 0x50);
        break;
    case k45Segment: {
        // two points a byte's distance apart on each axis at most (past 0xFF the
        // original never returns), anywhere in the s16 plane (the distances wrap)
        const U x0 = sh::Half() ? 0x100 + sh::Next() % 0x100 : sh::Next() & 0xFFFF;
        const U y0 = sh::Half() ? 0x100 + sh::Next() % 0x100 : sh::Next() & 0xFFFF;
        const U dx = PickOf(0, 1, 0xFF, 0xFFFF, 0xFF01, sh::Next() % 0x1FF - 0xFF);
        const U dy = PickOf(0, 1, 0xFF, 0xFFFF, 0xFF01, sh::Next() % 0x1FF - 0xFF);
        a[0] = (y0 << 16) | x0;
        a[1] = (((y0 + dy) & 0xFFFF) << 16) | ((x0 + dx) & 0xFFFF);
        break;
    }
    case k45Plot:
        a[0] = Key(EffectKind30_Shards + 4 * (sh::Next() % 0x40));
        break;
    case k48Draw:
        a[0] = Key(EffectKind30_Shards + at::kSparkStride * (sh::Next() % at::kSparks));
        break;
    default: break;
    }
}

// What the states read again after a call, moved by the group's case of the
// harness's disturbance (from its hash only): the frame count +9 (below 8,
// variant 3's step), the dwords +0xC / +0x10, the phase and offset words, +6
// against +0xB, a spark's live byte.
void Disturb(U h) {
    const U v = h >> 8;
    unsigned char* const s = Sprite_Current;
    switch (h % 7) {
    case 0: s[9] = static_cast<unsigned char>(v % 8); break;
    case 1: SetLong(s + 0xC, static_cast<std::int32_t>((v & 1) ? 1u : (v >> 1) & 0xFFu)); break;
    case 2: SetLong(s + 0x10, static_cast<std::int32_t>((v & 1) ? 0xFFFFFFFFu : v >> 1)); break;
    case 3: SetWord(s + 0x2E, (v & 1) ? 0x20u : (v >> 1) % 0x49u); break;
    case 4: SetWord(s + 0x36, (v & 1) ? 8u : (v >> 1) & 0x1Fu); break;
    case 5: s[6] = (v & 1) ? s[0xB] : static_cast<unsigned char>(v >> 1); break;
    case 6: Spark((v >> 1) % at::kSparks)[0x15] = static_cast<unsigned char>((v & 1) ? 0 : v >> 4); break;
    default: break;
    }
}

}  // namespace

void SelfTest() {
    // BOF3X_E2D_ONLY: the clones whose name contains it (a control's run)
    static sh::Clone chosen[kCount];
    static unsigned index[kCount];
    const char* const only = std::getenv("BOF3X_E2D_ONLY");
    unsigned n = 0;
    for (unsigned k = 0; k < kCount; ++k)
        if (!only || !*only || std::strstr(kAll[k].name, only)) {
            index[n] = k;
            chosen[n++] = kAll[k];
        }
    if (n == 0) bof3::Fatal("effect_2d: BOF3X_E2D_ONLY=%s names no clone", only);
    static unsigned* s_index = index;
    sh::Group g = {"effect_2d", chosen, n, kCallees, sizeof kCallees / sizeof kCallees[0], kTables,
                   sizeof kTables / sizeof kTables[0], kRegions, sizeof kRegions / sizeof kRegions[0],
                   [](unsigned k) { Seed(s_index[k]); }, &Disturb, 6000};
    g.args = [](unsigned k, U* a) { Args(s_index[k], a); };
    g.effect = true;
    g.kinds = kKinds;
    g.n_kinds = sizeof kKinds;
    sh::Run(g);
}

}  // namespace effect_2d

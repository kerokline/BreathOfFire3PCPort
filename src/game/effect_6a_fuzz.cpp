// BOF3X_SHADOW=effect_6a: group E6A's 48 functions through the scenario harness
// in effect mode (scenario_harness.h, docs/scenario_harness.md section 8), once
// at start-up. docs/effect_6a.md section 4. BOF3X_E6A_ONLY=<name> runs the
// clones whose name contains it (the controls' speed-up).
//
// The clone table is tools/band_rows.py --group E6A --clones --harness scenario
// (2026-10-03), each extent read again to its last instruction (capstone) and
// the names given; every extent is the tool's. Shape: every function kEffect
// (Sprite_Current one of the 20 Effect_Objects records, +5 0x18, a
// dispatcher's +2 below its table's length); none takes an argument and none
// answers (no ret_mask).
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_6a.h"
#include "game/effect_6a_callees.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace effect_6a {
namespace {

namespace sh = scenario_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using U = std::uint32_t;

// tools/band_rows.py --group E6A --clones, 2026-10-03.
constexpr sh::CallSite kCalls50C0D0[] = {{0x28, 0x579F00}, {0x43, 0x579F00}, {0x59, 0x587740}, {0x69, 0x50B8B0}};
constexpr sh::CallSite kCalls50C160[] = {{0x137, 0x50C4B0}};
constexpr sh::CallSite kCalls50C2A0[] = {{0xC7, 0x587740}, {0xD8, 0x50C4B0}};
constexpr sh::CallSite kCalls50C380[] = {{0x1A, 0x50C4B0}};
constexpr sh::CallSite kCalls50C3A0[] = {{0x64, 0x50C4B0}, {0xBB, 0x50C4B0}};
constexpr sh::CallSite kCalls50C470[] = {{0x24, 0x587740}, {0x36, 0x50C4B0}};
constexpr sh::CallSite kCalls50C4B0[] = {{0x16, 0x5A77C0},  {0x2C, 0x572FA0},  {0x38, 0x5A75D0},  {0x40, 0x5A77A0},
                                         {0x1C4, 0x5A85F0}, {0x1CA, 0x5A9290}, {0x1FD, 0x572A00}, {0x213, 0x572FA0}};
constexpr sh::CallSite kCalls50C830[] = {{0xCB, 0x587740}};
constexpr sh::CallSite kCalls50CA00[] = {{0x24, 0x587740}};
constexpr sh::CallSite kCalls50CA60[] = {{0x113, 0x50CD90}};
constexpr sh::CallSite kCalls50CB80[] = {{0xC7, 0x587740}, {0xD8, 0x50CD90}};
constexpr sh::CallSite kCalls50CC60[] = {{0x1A, 0x50CD90}};
constexpr sh::CallSite kCalls50CC80[] = {{0x64, 0x50CD90}, {0xBB, 0x50CD90}};
constexpr sh::CallSite kCalls50CD50[] = {{0x24, 0x587740}, {0x36, 0x50CD90}};
constexpr sh::CallSite kCalls50CD90[] = {{0x17, 0x5A77C0},  {0x20, 0x461E50},  {0x2C, 0x5A75D0},  {0x34, 0x5A77A0},
                                         {0x13E, 0x5720C0}, {0x15B, 0x5720C0}, {0x198, 0x5720C0}, {0x1B5, 0x5720C0},
                                         {0x1FE, 0x5A85F0}, {0x207, 0x5A9290}, {0x22B, 0x572A00}, {0x234, 0x461E50}};
constexpr sh::CallSite kCalls50D000[] = {{0x113, 0x50E1C0}};
constexpr sh::CallSite kCalls50D120[] = {{0xC7, 0x587740}, {0xD8, 0x50E1C0}};
constexpr sh::CallSite kCalls50D200[] = {{0x1A, 0x50E1C0}};
constexpr sh::CallSite kCalls50D220[] = {{0x64, 0x50E1C0}, {0xBB, 0x50E1C0}};
constexpr sh::CallSite kCalls50D2F0[] = {{0x24, 0x587740}, {0x36, 0x50E1C0}};
constexpr sh::CallSite kCalls50D350[] = {{0x113, 0x50D680}};
constexpr sh::CallSite kCalls50D470[] = {{0xC7, 0x587740}, {0xD8, 0x50D680}};
constexpr sh::CallSite kCalls50D550[] = {{0x1A, 0x50D680}};
constexpr sh::CallSite kCalls50D570[] = {{0x64, 0x50D680}, {0xBB, 0x50D680}};
constexpr sh::CallSite kCalls50D640[] = {{0x24, 0x587740}, {0x36, 0x50D680}};
constexpr sh::CallSite kCalls50D680[] = {{0x17, 0x5A77C0},  {0x20, 0x461E50},  {0x2C, 0x5A75D0},  {0x34, 0x5A77A0},
                                         {0x13E, 0x5720C0}, {0x15B, 0x5720C0}, {0x198, 0x5720C0}, {0x1B5, 0x5720C0},
                                         {0x1FE, 0x5A85F0}, {0x207, 0x5A9290}, {0x232, 0x572A00}, {0x23B, 0x461E50}};
constexpr sh::CallSite kCalls50D8F0[] = {{0x113, 0x50DC20}};
constexpr sh::CallSite kCalls50DA10[] = {{0xC7, 0x587740}, {0xD8, 0x50DC20}};
constexpr sh::CallSite kCalls50DAF0[] = {{0x1A, 0x50DC20}};
constexpr sh::CallSite kCalls50DB10[] = {{0x64, 0x50DC20}, {0xBB, 0x50DC20}};
constexpr sh::CallSite kCalls50DBE0[] = {{0x24, 0x587740}, {0x36, 0x50DC20}};
constexpr sh::CallSite kCalls50DC20[] = {{0x17, 0x5A77C0},  {0x20, 0x461E50},  {0x2C, 0x5A75D0},  {0x34, 0x5A77A0},
                                         {0x13E, 0x5720C0}, {0x15B, 0x5720C0}, {0x198, 0x5720C0}, {0x1B5, 0x5720C0},
                                         {0x1FE, 0x5A85F0}, {0x207, 0x5A9290}, {0x237, 0x572A00}, {0x240, 0x461E50}};
constexpr sh::CallSite kCalls50DE90[] = {{0x113, 0x50E1C0}};
constexpr sh::CallSite kCalls50DFB0[] = {{0xC7, 0x587740}, {0xD8, 0x50E1C0}};
constexpr sh::CallSite kCalls50E090[] = {{0x1A, 0x50E1C0}};
constexpr sh::CallSite kCalls50E0B0[] = {{0x64, 0x50E1C0}, {0xBB, 0x50E1C0}};
constexpr sh::CallSite kCalls50E180[] = {{0x24, 0x587740}, {0x36, 0x50E1C0}};
constexpr sh::CallSite kCalls50E1C0[] = {{0x17, 0x5A77C0},  {0x20, 0x461E50},  {0x2C, 0x5A75D0},  {0x34, 0x5A77A0},
                                         {0x13E, 0x5720C0}, {0x15B, 0x5720C0}, {0x198, 0x5720C0}, {0x1B5, 0x5720C0},
                                         {0x1FE, 0x5A85F0}, {0x207, 0x5A9290}, {0x223, 0x572A00}, {0x22C, 0x461E50}};

#define E6A_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define E6A_CALLS(a) a, E6A_N(a)
#define E6A_FN(name) reinterpret_cast<const void*>(&::name)
constexpr sh::Shape kEf = sh::Shape::kEffect;
// {name, base, size, calls, imms, tables, ours, ret_mask, calm, shape, pointers, state_span, sub_span, kind}
const sh::Clone kAll[] = {
    {"EffectKind18Sub4A_WaitCond", 0x50C0D0, 0x6E, E6A_CALLS(kCalls50C0D0), nullptr, 0, nullptr, 0, E6A_FN(EffectKind18Sub4A_WaitCond), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub2D_Run", 0x50C140, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E6A_FN(EffectKind18Sub2D_Run), 0, false, kEf, 0, 0, 5, 0x18},
    {"EffectKind18Sub2D_Place", 0x50C160, 0x13E, E6A_CALLS(kCalls50C160), nullptr, 0, nullptr, 0, E6A_FN(EffectKind18Sub2D_Place), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub2D_WaitNear", 0x50C2A0, 0xDF, E6A_CALLS(kCalls50C2A0), nullptr, 0, nullptr, 0, E6A_FN(EffectKind18Sub2D_WaitNear), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub2D_Open", 0x50C380, 0x1F, E6A_CALLS(kCalls50C380), nullptr, 0, nullptr, 0, E6A_FN(EffectKind18Sub2D_Open), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub2D_WaitFar", 0x50C3A0, 0xC2, E6A_CALLS(kCalls50C3A0), nullptr, 0, nullptr, 0, E6A_FN(EffectKind18Sub2D_WaitFar), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub2D_Close", 0x50C470, 0x3B, E6A_CALLS(kCalls50C470), nullptr, 0, nullptr, 0, E6A_FN(EffectKind18Sub2D_Close), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub2D_Draw", 0x50C4B0, 0x221, E6A_CALLS(kCalls50C4B0), nullptr, 0, nullptr, 0, E6A_FN(EffectKind18Sub2D_Draw), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub3E_Run", 0x50C6E0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E6A_FN(EffectKind18Sub3E_Run), 0, false, kEf, 0, 0, 5, 0x18},
    {"EffectKind18Sub3E_Place", 0x50C700, 0x129, nullptr, 0, nullptr, 0, nullptr, 0, E6A_FN(EffectKind18Sub3E_Place), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub3E_WaitNear", 0x50C830, 0xDE, E6A_CALLS(kCalls50C830), nullptr, 0, nullptr, 0, E6A_FN(EffectKind18Sub3E_WaitNear), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub3E_Open", 0x50C910, 0x1B, nullptr, 0, nullptr, 0, nullptr, 0, E6A_FN(EffectKind18Sub3E_Open), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub3E_WaitFar", 0x50C930, 0xC1, nullptr, 0, nullptr, 0, nullptr, 0, E6A_FN(EffectKind18Sub3E_WaitFar), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub3E_Close", 0x50CA00, 0x37, E6A_CALLS(kCalls50CA00), nullptr, 0, nullptr, 0, E6A_FN(EffectKind18Sub3E_Close), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub2E_Run", 0x50CA40, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E6A_FN(EffectKind18Sub2E_Run), 0, false, kEf, 0, 0, 5, 0x18},
    {"EffectKind18Sub2E_Place", 0x50CA60, 0x11A, E6A_CALLS(kCalls50CA60), nullptr, 0, nullptr, 0, E6A_FN(EffectKind18Sub2E_Place), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub2E_WaitNear", 0x50CB80, 0xDF, E6A_CALLS(kCalls50CB80), nullptr, 0, nullptr, 0, E6A_FN(EffectKind18Sub2E_WaitNear), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub2E_Open", 0x50CC60, 0x1F, E6A_CALLS(kCalls50CC60), nullptr, 0, nullptr, 0, E6A_FN(EffectKind18Sub2E_Open), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub2E_WaitFar", 0x50CC80, 0xC2, E6A_CALLS(kCalls50CC80), nullptr, 0, nullptr, 0, E6A_FN(EffectKind18Sub2E_WaitFar), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub2E_Close", 0x50CD50, 0x3B, E6A_CALLS(kCalls50CD50), nullptr, 0, nullptr, 0, E6A_FN(EffectKind18Sub2E_Close), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub2E_Draw", 0x50CD90, 0x243, E6A_CALLS(kCalls50CD90), nullptr, 0, nullptr, 0, E6A_FN(EffectKind18Sub2E_Draw), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub2F_Run", 0x50CFE0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E6A_FN(EffectKind18Sub2F_Run), 0, false, kEf, 0, 0, 5, 0x18},
    {"EffectKind18Sub2F_Place", 0x50D000, 0x11A, E6A_CALLS(kCalls50D000), nullptr, 0, nullptr, 0, E6A_FN(EffectKind18Sub2F_Place), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub2F_WaitNear", 0x50D120, 0xDF, E6A_CALLS(kCalls50D120), nullptr, 0, nullptr, 0, E6A_FN(EffectKind18Sub2F_WaitNear), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub2F_Open", 0x50D200, 0x1F, E6A_CALLS(kCalls50D200), nullptr, 0, nullptr, 0, E6A_FN(EffectKind18Sub2F_Open), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub2F_WaitFar", 0x50D220, 0xC2, E6A_CALLS(kCalls50D220), nullptr, 0, nullptr, 0, E6A_FN(EffectKind18Sub2F_WaitFar), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub2F_Close", 0x50D2F0, 0x3B, E6A_CALLS(kCalls50D2F0), nullptr, 0, nullptr, 0, E6A_FN(EffectKind18Sub2F_Close), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub30_Run", 0x50D330, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E6A_FN(EffectKind18Sub30_Run), 0, false, kEf, 0, 0, 5, 0x18},
    {"EffectKind18Sub30_Place", 0x50D350, 0x11A, E6A_CALLS(kCalls50D350), nullptr, 0, nullptr, 0, E6A_FN(EffectKind18Sub30_Place), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub30_WaitNear", 0x50D470, 0xDF, E6A_CALLS(kCalls50D470), nullptr, 0, nullptr, 0, E6A_FN(EffectKind18Sub30_WaitNear), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub30_Open", 0x50D550, 0x1F, E6A_CALLS(kCalls50D550), nullptr, 0, nullptr, 0, E6A_FN(EffectKind18Sub30_Open), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub30_WaitFar", 0x50D570, 0xC2, E6A_CALLS(kCalls50D570), nullptr, 0, nullptr, 0, E6A_FN(EffectKind18Sub30_WaitFar), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub30_Close", 0x50D640, 0x3B, E6A_CALLS(kCalls50D640), nullptr, 0, nullptr, 0, E6A_FN(EffectKind18Sub30_Close), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub30_Draw", 0x50D680, 0x24A, E6A_CALLS(kCalls50D680), nullptr, 0, nullptr, 0, E6A_FN(EffectKind18Sub30_Draw), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub31_Run", 0x50D8D0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E6A_FN(EffectKind18Sub31_Run), 0, false, kEf, 0, 0, 5, 0x18},
    {"EffectKind18Sub31_Place", 0x50D8F0, 0x11A, E6A_CALLS(kCalls50D8F0), nullptr, 0, nullptr, 0, E6A_FN(EffectKind18Sub31_Place), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub31_WaitNear", 0x50DA10, 0xDF, E6A_CALLS(kCalls50DA10), nullptr, 0, nullptr, 0, E6A_FN(EffectKind18Sub31_WaitNear), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub31_Open", 0x50DAF0, 0x1F, E6A_CALLS(kCalls50DAF0), nullptr, 0, nullptr, 0, E6A_FN(EffectKind18Sub31_Open), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub31_WaitFar", 0x50DB10, 0xC2, E6A_CALLS(kCalls50DB10), nullptr, 0, nullptr, 0, E6A_FN(EffectKind18Sub31_WaitFar), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub31_Close", 0x50DBE0, 0x3B, E6A_CALLS(kCalls50DBE0), nullptr, 0, nullptr, 0, E6A_FN(EffectKind18Sub31_Close), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub31_Draw", 0x50DC20, 0x24F, E6A_CALLS(kCalls50DC20), nullptr, 0, nullptr, 0, E6A_FN(EffectKind18Sub31_Draw), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub32_Run", 0x50DE70, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E6A_FN(EffectKind18Sub32_Run), 0, false, kEf, 0, 0, 5, 0x18},
    {"EffectKind18Sub32_Place", 0x50DE90, 0x11A, E6A_CALLS(kCalls50DE90), nullptr, 0, nullptr, 0, E6A_FN(EffectKind18Sub32_Place), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub32_WaitNear", 0x50DFB0, 0xDF, E6A_CALLS(kCalls50DFB0), nullptr, 0, nullptr, 0, E6A_FN(EffectKind18Sub32_WaitNear), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub32_Open", 0x50E090, 0x1F, E6A_CALLS(kCalls50E090), nullptr, 0, nullptr, 0, E6A_FN(EffectKind18Sub32_Open), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub32_WaitFar", 0x50E0B0, 0xC2, E6A_CALLS(kCalls50E0B0), nullptr, 0, nullptr, 0, E6A_FN(EffectKind18Sub32_WaitFar), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub32_Close", 0x50E180, 0x3B, E6A_CALLS(kCalls50E180), nullptr, 0, nullptr, 0, E6A_FN(EffectKind18Sub32_Close), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub2F_Draw", 0x50E1C0, 0x23B, E6A_CALLS(kCalls50E1C0), nullptr, 0, nullptr, 0, E6A_FN(EffectKind18Sub2F_Draw), 0, false, kEf, 0, 0, 0, 0x18},
};
#undef E6A_FN
#undef E6A_CALLS
#undef E6A_N

enum : unsigned {
    k4AWaitCond,
    k2DRun, k2DPlace, k2DWaitNear, k2DOpen, k2DWaitFar, k2DClose, k2DDraw,
    k3ERun, k3EPlace, k3EWaitNear, k3EOpen, k3EWaitFar, k3EClose,
    k2ERun, k2EPlace, k2EWaitNear, k2EOpen, k2EWaitFar, k2EClose, k2EDraw,
    k2FRun, k2FPlace, k2FWaitNear, k2FOpen, k2FWaitFar, k2FClose,
    k30Run, k30Place, k30WaitNear, k30Open, k30WaitFar, k30Close, k30Draw,
    k31Run, k31Place, k31WaitNear, k31Open, k31WaitFar, k31Close, k31Draw,
    k32Run, k32Place, k32WaitNear, k32Open, k32WaitFar, k32Close,
    k2FDraw, kCount
};
static_assert(kCount == sizeof kAll / sizeof kAll[0], "one enum entry a clone, in order");

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
unsigned char* Mem(U a) { return sh::Mem(a); }
template <typename... T> U PickOf(T... v) {
    const U values[] = {static_cast<U>(v)...};
    return sh::Pick(values, sizeof...(v));
}

// The draws the states call or tail-jump to take no argument and read
// Sprite_Current's record: kPhase (the recorder logs Sprite_Current), so a draw
// on the wrong record, or before a state's write it should follow, shows.
// E5G's EffectKind18Sub2C_Draw is sub-kind 0x4A's state 1's tail jump.
#define E6A_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
constexpr sh::Answer kPh = sh::Answer::kPhase;
const sh::Callee kCallees[] = {
    {E6A_OURS(EffectKind18Sub2C_Draw), 0, {}, kPh, 0, 0},
    {E6A_OURS(EffectKind18Sub2D_Draw), 0, {}, kPh, 0, 0},
    {E6A_OURS(EffectKind18Sub2E_Draw), 0, {}, kPh, 0, 0},
    {E6A_OURS(EffectKind18Sub2F_Draw), 0, {}, kPh, 0, 0},
    {E6A_OURS(EffectKind18Sub30_Draw), 0, {}, kPh, 0, 0},
    {E6A_OURS(EffectKind18Sub31_Draw), 0, {}, kPh, 0, 0},
};
#undef E6A_OURS

// The sub-state tables the dispatchers jump through, read in place; each
// table's own length (symbols.toml [[data]], none bounded by a compare).
const sh::DataTable kTables[] = {
    {0x65EAB0, 5}, {0x65EB08, 5}, {0x65EB30, 5}, {0x65EB50, 5}, {0x65EB6C, 5}, {0x65EB88, 5}, {0x65EBA4, 5},
};
const std::uint8_t kKinds[] = {0x18};

// --- the seed ------------------------------------------------------------------------

unsigned char* Rec(unsigned r) { return sh::EffectRecord(r); }
std::int32_t S16(const unsigned char* p) { return static_cast<std::int16_t>(Word(p)); }

// Which way function k's sub-kind slides open: true toward +0x100 (0x2D, 0x3E,
// 0x2F, 0x32), false toward -0x100 (0x2E, 0x30, 0x31). 0x4A's state and the
// draws: either.
bool SlidesUp(unsigned k) {
    if (k >= k2ERun && k <= k2EDraw) return false;
    if (k >= k30Run && k <= k31Draw) return false;
    if (k == k4AWaitCond || k == k2FDraw) return sh::Half();
    return true;
}
// A slide +0x30 one step (0x20) from its ends and one either side, at them,
// and past them.
U Slide(bool up) {
    if (up) return PickOf(0, 0x1F, 0x20, 0x21, 0xDF, 0xE0, 0xE1, 0x100, 0x120, 0xFFE0, sh::Next());
    return PickOf(0, 0xFFE1, 0xFFE0, 0xFFDF, 0xFF21, 0xFF20, 0xFF1F, 0xFF00, 0xFEE0, 0x20, sh::Next());
}
// A 16.16 point about a cell: a whole cell -3..+4 from it and a fraction at the
// tests' boundaries (half a cell, a cell, one either side).
U Around(std::int32_t cell) {
    const std::int32_t d = static_cast<std::int32_t>(sh::Next() % 8) - 3;
    const U frac = PickOf(0, 1, 0xFFFF, 0x8000, 0x7FFF, 0x8001, sh::Next() & 0xFFFF);
    return (static_cast<U>(cell + d) << 16) + frac;
}
// A cell word: small (0..0x7F) or random.
U Cell() { return sh::Often() ? sh::Next() % 0x80 : sh::Next() & 0xFFFF; }

// A place state's variant tables (its cells) and room; 0 for the rest.
struct Cells { U at; unsigned room; };
Cells PlaceOf(unsigned k) {
    switch (k) {
    case k2DPlace: return {at::kSub2DCells, at::kSub2DVariants};
    case k3EPlace: return {at::kSub3ECells, at::kSub3EVariants};
    case k2EPlace: return {at::kSub2ECells, at::kSub2EVariants};
    case k2FPlace: return {at::kSub2FCells, at::kSmallVariants};
    case k30Place: return {at::kSub30Cells, at::kSmallVariants};
    case k31Place: return {at::kSub31Cells, at::kSmallVariants};
    case k32Place: return {at::kSub32Cells, at::kSmallVariants};
    default: return {0, 0};
    }
}
bool Is3E(unsigned k) { return k >= k3ERun && k <= k3EClose; }

void Seed(unsigned k) {
    const bool up = SlidesUp(k);
    // every record the disturbance may move Sprite_Current to: the slide at
    // its ends, +8 the axis (0 half the time), +0xA (sub-kind 0x2D's sign
    // index) inside its room
    for (unsigned r = 0; r < 20; ++r) {
        unsigned char* const e = Rec(r);
        SetWord(e + 0x30, Slide(up));
        if (sh::Half()) e[8] = 0;
        e[0xA] = static_cast<unsigned char>(sh::Next() % at::kSidesRoom);
    }
    unsigned char* const s = Sprite_Current;
    SetWord(s + 0x36, Cell());
    SetWord(s + 0x3A, Cell());
    // the cell the leader is tested against: the table's for a place state
    // (after the write), the record's own otherwise
    std::int32_t cx = S16(s + 0x36), cz = S16(s + 0x3A);
    const Cells place = PlaceOf(k);
    if (place.room != 0) {
        const U v = sh::Next() % place.room;
        SetWord(s + 0x36, v);
        if (sh::Half()) SetWord(s + 0x3A, sh::Half() ? 0u : sh::Next() % 4);   // +8 / +0xA from the z cell
        cx = Mem(place.at + 2 * v)[0];
        cz = Mem(place.at + 2 * v + 1)[0];
    }
    // the leader about the front seven times in eight: the centre (cell + 1,
    // or cell - 2 for 0x3E) on either axis, or the edge
    const std::int32_t delta = Is3E(k) ? -2 : 1;
    if (sh::Next() % 8 != 0) {
        SetLong(Mem(at::kLeaderX), static_cast<std::int32_t>(Around(cx + (sh::Half() ? delta : 0))));
        SetLong(Mem(at::kLeaderZ), static_cast<std::int32_t>(Around(cz + (sh::Half() ? delta : 0))));
    }
    Field_Request = static_cast<unsigned char>(PickOf(0, 0, 2, sh::Next()));
    // sub-kind 0x4A's flag: +0xB against Cond_ByteFE, often sharing a bit
    if (k == k4AWaitCond) {
        const unsigned bit = 1u << (sh::Next() % 8);
        s[0xB] = static_cast<unsigned char>(sh::Half() ? bit : sh::Next());
        Cond_ByteFE = static_cast<unsigned char>(PickOf(0, bit, ~bit, sh::Next()));
    }
}

// What the states read again after a call, moved by the group's case of the
// harness's disturbance (from its hash only): the slide +0x30, +8, +0xA inside
// its room, the leader's point, the cell words, Field_Request.
void Disturb(U h) {
    const U v = h >> 8;
    unsigned char* const s = Sprite_Current;
    if (!sh::InRegions(s, 0x80)) return;
    switch (h % 7) {
    case 0: SetWord(s + 0x30, (v & 1) ? 0x100u - ((v >> 1) & 0x20) : v >> 1); break;
    case 1: s[8] = static_cast<unsigned char>((v & 1) ? 0u : v >> 1); break;
    case 2: SetLong(Mem(at::kLeaderX), static_cast<std::int32_t>(v)); break;
    case 3: SetLong(Mem(at::kLeaderZ), static_cast<std::int32_t>(v)); break;
    case 4: SetWord(s + ((v & 1) ? 0x36 : 0x3A), v >> 1); break;
    case 5: s[0xA] = static_cast<unsigned char>(v % at::kSidesRoom); break;
    case 6: Field_Request = static_cast<unsigned char>((v & 1) ? 0u : v >> 1); break;
    default: break;
    }
}

}  // namespace

void SelfTest() {
    // BOF3X_E6A_ONLY: the clones whose name contains it (a control's run)
    static sh::Clone chosen[kCount];
    static unsigned index[kCount];
    const char* const only = std::getenv("BOF3X_E6A_ONLY");
    unsigned n = 0;
    for (unsigned k = 0; k < kCount; ++k)
        if (!only || !*only || std::strstr(kAll[k].name, only)) {
            index[n] = k;
            chosen[n++] = kAll[k];
        }
    if (n == 0) bof3::Fatal("effect_6a: BOF3X_E6A_ONLY=%s names no clone", only);
    static unsigned* s_index = index;
    sh::Group g = {"effect_6a", chosen, n, kCallees, sizeof kCallees / sizeof kCallees[0], kTables,
                   sizeof kTables / sizeof kTables[0], nullptr, 0,
                   [](unsigned k) { Seed(s_index[k]); }, &Disturb, 4000};
    g.effect = true;
    g.kinds = kKinds;
    g.n_kinds = sizeof kKinds;
    sh::Run(g);
}

}  // namespace effect_6a

// BOF3X_SHADOW=effect_4f: group E4F's 49 functions through the scenario harness
// in effect mode (scenario_harness.h, docs/scenario_harness.md section 8), once
// at start-up. docs/effect_4f.md section 4. BOF3X_E4F_ONLY=<name> runs the
// clones whose name contains it (the controls' speed-up); BOF3X_E4F_ROUNDS the
// rounds.
//
// The clone table is tools/band_rows.py --group E4F --clones --harness scenario
// (2026-10-03), each extent read again to its last instruction (capstone) and
// the names given; the tool's extents stand (none differs from the code), with
// three rows added by hand: 0x492530 (0x47), 0x492750 (9) and 0x492CF0 (0xC4),
// the band's unplaced rows of its own kinds. Shapes: every dispatcher, state
// and no-argument helper kEffect (Sprite_Current one of the 20 Effect_Objects
// records, +5 the kind, a dispatcher's +1 below its table's length); the
// helpers with arguments kCall.
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_4f.h"
#include "game/effect_4f_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace effect_4f {
namespace {

namespace sh = scenario_harness;
using move_script::SetWord;
using move_script::Word;
using U = std::uint32_t;

// tools/band_rows.py --group E4F --clones, 2026-10-03 (0x492530 and 0x492CF0 by hand).
constexpr sh::CallSite kCalls492530[] = {{0x2B, 0x487920}, {0x30, 0x492C80}};
constexpr sh::CallSite kCalls492880[] = {{0xC, 0x493010}};
constexpr sh::CallSite kCalls4928C0[] = {{0x14, 0x493010}};
constexpr sh::CallSite kCalls492900[] = {{0x14, 0x493010}};
constexpr sh::CallSite kCalls492940[] = {{0x17, 0x493010}, {0x35, 0x589840}};
constexpr sh::CallSite kCalls4929C0[] = {{0x1F, 0x4932E0}, {0x31, 0x4932E0}};
constexpr sh::CallSite kCalls492A00[] = {{0x20, 0x4932E0}, {0x3E, 0x589840}};
constexpr sh::CallSite kCalls492A70[] = {{0x15, 0x493330}};
constexpr sh::CallSite kCalls492AA0[] = {{0xD, 0x493350}, {0x12, 0x5B93D2}, {0x36, 0x493370}};
constexpr sh::CallSite kCalls492AF0[] = {{0x6, 0x494060}, {0x41, 0x492B60}};
constexpr sh::CallSite kCalls492B60[] = {{0x13, 0x5A79A0}, {0x29, 0x5A77C0}, {0x40, 0x572FA0}, {0x4B, 0x494110}, {0x57, 0x5A7750},
                                         {0x5F, 0x5A7780}, {0xA5, 0x572FA0}, {0xB1, 0x5A7740}, {0xB9, 0x5A7780}, {0x10D, 0x572FA0}};
constexpr sh::CallSite kCalls492C80[] = {{0x1A, 0x5B93D2}, {0x25, 0x5B93D2}, {0x44, 0x5720C0}};
constexpr sh::CallSite kCalls492CF0[] = {{0x39, 0x487BF0}, {0x47, 0x5B93D2}, {0x53, 0x5B93D2}, {0x6D, 0x5B93D2}, {0x90, 0x5B93D2}, {0x9D, 0x5B93D2}};
constexpr sh::CallSite kCalls492DC0[] = {{0x17, 0x5A79A0}, {0x2F, 0x5A77C0}, {0x38, 0x461E50}, {0x43, 0x5A7A50}, {0x5F, 0x5A7A00},
                                         {0x87, 0x494110}, {0x8D, 0x5A7A50}, {0xAB, 0x5A7A00}, {0xD3, 0x494110}, {0xF7, 0x5A7610},
                                         {0xFF, 0x5A7780}, {0x13F, 0x5A7A50}, {0x157, 0x5A7A00}, {0x17F, 0x494110}, {0x185, 0x5A7A50},
                                         {0x1A0, 0x5A7A00}, {0x1CE, 0x494110}, {0x22C, 0x461E50}};
constexpr sh::CallSite kCalls493010[] = {{0x13, 0x5A79A0}, {0x2B, 0x5A77C0}, {0x34, 0x461E50}, {0x6E, 0x493090}};
constexpr sh::CallSite kCalls493090[] = {{0x7, 0x494060}, {0x16, 0x494110}, {0x3A, 0x4941E0}, {0x7A, 0x5A75F0}, {0x82, 0x5A7780},
                                         {0xAB, 0x5A7A50}, {0xBE, 0x5A7A00}, {0xD9, 0x5A7A50}, {0xE8, 0x5A7A00}, {0x109, 0x5A7A00},
                                         {0x116, 0x5A7A50}, {0x147, 0x5A7A50}, {0x15A, 0x5A7A00}, {0x179, 0x5A7A50}, {0x188, 0x5A7A00},
                                         {0x1AB, 0x5A7A00}, {0x1B8, 0x5A7A50}, {0x22E, 0x461E50}};
constexpr sh::CallSite kCalls4932E0[] = {{0x8, 0x5A7740}, {0x10, 0x5A7780}, {0x3C, 0x461E50}};
constexpr sh::CallSite kCalls493370[] = {{0x16, 0x5A79A0}, {0x2E, 0x5A77C0}, {0x37, 0x461E50}, {0x3F, 0x494060}, {0xA2, 0x4920F0}};
constexpr sh::CallSite kCalls4934B0[] = {{0x15, 0x5725F0}, {0x36, 0x587740}};
constexpr sh::CallSite kCalls493500[] = {{0x15, 0x5725F0}, {0x36, 0x587740}};
constexpr sh::CallSite kCalls493570[] = {{0x1A, 0x4901D0}, {0x4C, 0x587740}};
constexpr sh::CallSite kCalls4935D0[] = {{0x10, 0x4901D0}};
constexpr sh::CallSite kCalls493610[] = {{0x2B, 0x4901D0}, {0x64, 0x482050}, {0x95, 0x482240}, {0xA8, 0x587740}};
constexpr sh::CallSite kCalls4936D0[] = {{0x17, 0x493B50}, {0x1C, 0x493BF0}, {0x21, 0x493BA0}};
constexpr sh::CallSite kCalls493750[] = {{0x17, 0x493B50}, {0x1C, 0x493BF0}, {0x21, 0x493BA0}};
constexpr sh::CallSite kCalls493790[] = {{0x17, 0x493B50}, {0x1C, 0x493BA0}, {0x21, 0x493BF0}, {0x35, 0x4938E0}};
constexpr sh::CallSite kCalls493850[] = {{0x17, 0x493B50}, {0x1C, 0x493BA0}, {0x21, 0x493BF0}, {0x35, 0x4938E0}, {0x56, 0x587740}};
constexpr sh::CallSite kCalls4938E0[] = {{0xE, 0x5A7610}, {0x16, 0x5A7780}, {0x25, 0x494110}, {0x6A, 0x494110}, {0xD9, 0x461E50},
                                         {0xE5, 0x5A7610}, {0xED, 0x5A7780}, {0xF8, 0x494110}, {0x139, 0x494110}, {0x1A7, 0x461E50},
                                         {0x1EB, 0x494110}, {0x1F7, 0x5A7750}, {0x1FF, 0x5A7780}, {0x227, 0x461E50}};
constexpr sh::CallSite kCalls493BA0[] = {{0x5, 0x494060}, {0x31, 0x4820C0}};
constexpr sh::CallSite kCalls493BF0[] = {{0x12, 0x5A79A0}, {0x2A, 0x5A77C0}, {0x33, 0x461E50}, {0x3B, 0x494060}, {0x4E, 0x493C60}};
constexpr sh::CallSite kCalls493C60[] = {{0xE, 0x5A75F0}, {0x16, 0x5A7780}, {0x24, 0x494110}, {0x2E, 0x5A7A50}, {0x41, 0x5A7A00},
                                         {0x66, 0x5A7A00}, {0x79, 0x5A7A50}, {0xBE, 0x494110}, {0xC8, 0x5A7A50}, {0xDB, 0x5A7A00},
                                         {0x100, 0x5A7A00}, {0x113, 0x5A7A50}, {0x158, 0x494110}, {0x19D, 0x461E50}};
constexpr sh::CallSite kCalls493E30[] = {{0x18, 0x587740}};
constexpr sh::CallSite kCalls493F70[] = {{0x11, 0x5A79A0}, {0x29, 0x5A77C0}, {0x32, 0x461E50}, {0x37, 0x494060}, {0x43, 0x5A7650},
                                         {0x4B, 0x5A7780}, {0x5A, 0x494110}, {0x7E, 0x494110}, {0xAA, 0x461E50}};

#define E4F_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define E4F_CALLS(a) a, E4F_N(a)
#define E4F_FN(name) reinterpret_cast<const void*>(&::name)
constexpr sh::Shape kEf = sh::Shape::kEffect;
constexpr sh::Shape kCa = sh::Shape::kCall;
// {name, base, size, calls, imms, tables, ours, ret_mask, calm, shape, pointers, state_span, sub_span, kind}
const sh::Clone kAll[] = {
    {"EffectKindAA_Run", 0x491D70, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E4F_FN(EffectKindAA_Run), 0, false, kEf, 0, 3, 0, 0xAA},
    {"EffectKindAB_Run", 0x492510, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E4F_FN(EffectKindAB_Run), 0, false, kEf, 0, 3, 0, 0xAB},
    {"EffectKindAB_Start", 0x492530, 0x47, E4F_CALLS(kCalls492530), nullptr, 0, nullptr, 0, E4F_FN(EffectKindAB_Start), 0, false, kEf, 0, 0, 0, 0xAB},
    {"EffectKindAB_MoveDrops", 0x492AF0, 0x6F, E4F_CALLS(kCalls492AF0), nullptr, 0, nullptr, 0, E4F_FN(EffectKindAB_MoveDrops), 0xFF, false, kEf, 0, 0, 0, 0xAB},
    {"EffectKindAB_DrawDrop", 0x492B60, 0x11D, E4F_CALLS(kCalls492B60), nullptr, 0, nullptr, 0, E4F_FN(EffectKindAB_DrawDrop), 0, false, kCa, 0, 0, 0, 0xAB},
    {"EffectKindAB_PlaceSources", 0x492C80, 0x62, E4F_CALLS(kCalls492C80), nullptr, 0, nullptr, 0, E4F_FN(EffectKindAB_PlaceSources), 0, false, kEf, 0, 0, 0, 0xAB},
    {"EffectKindAB_Emit", 0x492CF0, 0xC4, E4F_CALLS(kCalls492CF0), nullptr, 0, nullptr, 0, E4F_FN(EffectKindAB_Emit), 0xFF, false, kEf, 0, 0, 0, 0xAB},
    {"EffectKindAC_Run", 0x4925A0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E4F_FN(EffectKindAC_Run), 0, false, kEf, 0, 3, 0, 0xAC},
    {"EffectKindAD_Run", 0x492660, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E4F_FN(EffectKindAD_Run), 0, false, kEf, 0, 5, 0, 0xAD},
    {"Effect_StateNext", 0x492750, 0x9, nullptr, 0, nullptr, 0, nullptr, 0, E4F_FN(Effect_StateNext), 0, false, kEf, 0, 0, 0, 0xAD},
    {"EffectKindAD_DrawArc", 0x492DC0, 0x24E, E4F_CALLS(kCalls492DC0), nullptr, 0, nullptr, 0, E4F_FN(EffectKindAD_DrawArc), 0, false, kCa, 0, 0, 0, 0xAD},
    {"EffectKindAE_Run", 0x492760, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E4F_FN(EffectKindAE_Run), 0, false, kEf, 0, 5, 0, 0xAE},
    {"EffectKindAE_Sink", 0x492880, 0x37, E4F_CALLS(kCalls492880), nullptr, 0, nullptr, 0, E4F_FN(EffectKindAE_Sink), 0, false, kEf, 0, 0, 0, 0xAE},
    {"EffectKindAE_Widen", 0x4928C0, 0x3F, E4F_CALLS(kCalls4928C0), nullptr, 0, nullptr, 0, E4F_FN(EffectKindAE_Widen), 0, false, kEf, 0, 0, 0, 0xAE},
    {"EffectKindAE_Narrow", 0x492900, 0x3F, E4F_CALLS(kCalls492900), nullptr, 0, nullptr, 0, E4F_FN(EffectKindAE_Narrow), 0, false, kEf, 0, 0, 0, 0xAE},
    {"EffectKindAE_Shrink", 0x492940, 0x3B, E4F_CALLS(kCalls492940), nullptr, 0, nullptr, 0, E4F_FN(EffectKindAE_Shrink), 0, false, kEf, 0, 0, 0, 0xAE},
    {"EffectKindAE_Draw", 0x493010, 0x77, E4F_CALLS(kCalls493010), nullptr, 0, nullptr, 0, E4F_FN(EffectKindAE_Draw), 0, false, kEf, 0, 0, 0, 0xAE},
    {"Effect_DrawEllipse", 0x493090, 0x24D, E4F_CALLS(kCalls493090), nullptr, 0, nullptr, 0, E4F_FN(Effect_DrawEllipse), 0, false, kCa, 0, 0, 0, 0xAE},
    {"EffectKindAF_Run", 0x492980, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E4F_FN(EffectKindAF_Run), 0, false, kEf, 0, 3, 0, 0xAF},
    {"EffectKindAF_Start", 0x4929A0, 0x1C, nullptr, 0, nullptr, 0, nullptr, 0, E4F_FN(EffectKindAF_Start), 0, false, kEf, 0, 0, 0, 0xAF},
    {"EffectKindAF_FadeIn", 0x4929C0, 0x38, E4F_CALLS(kCalls4929C0), nullptr, 0, nullptr, 0, E4F_FN(EffectKindAF_FadeIn), 0, false, kEf, 0, 0, 0, 0xAF},
    {"EffectKindAF_FadeOut", 0x492A00, 0x47, E4F_CALLS(kCalls492A00), nullptr, 0, nullptr, 0, E4F_FN(EffectKindAF_FadeOut), 0, false, kEf, 0, 0, 0, 0xAF},
    {"EffectKindAF_DrawScreen", 0x4932E0, 0x46, E4F_CALLS(kCalls4932E0), nullptr, 0, nullptr, 0, E4F_FN(EffectKindAF_DrawScreen), 0, false, kCa, 0, 0, 0, 0xAF},
    {"EffectKindB0_Run", 0x492A50, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E4F_FN(EffectKindB0_Run), 0, false, kEf, 0, 3, 0, 0xB0},
    {"EffectKindB0_Start", 0x492A70, 0x23, E4F_CALLS(kCalls492A70), nullptr, 0, nullptr, 0, E4F_FN(EffectKindB0_Start), 0, false, kEf, 0, 0, 0, 0xB0},
    {"EffectKindB0_Spawn", 0x492AA0, 0x50, E4F_CALLS(kCalls492AA0), nullptr, 0, nullptr, 0, E4F_FN(EffectKindB0_Spawn), 0, false, kEf, 0, 0, 0, 0xB0},
    {"EffectKindB0_ClearBars", 0x493330, 0x14, nullptr, 0, nullptr, 0, nullptr, 0, E4F_FN(EffectKindB0_ClearBars), 0, false, kEf, 0, 0, 0, 0xB0},
    {"EffectKindB0_NewBar", 0x493350, 0x1F, nullptr, 0, nullptr, 0, nullptr, 0, E4F_FN(EffectKindB0_NewBar), 0, false, kEf, 0, 0, 0, 0xB0},
    {"EffectKindB0_StepBars", 0x493370, 0xB7, E4F_CALLS(kCalls493370), nullptr, 0, nullptr, 0, E4F_FN(EffectKindB0_StepBars), 0xFF, false, kCa, 0, 0, 0, 0xB0},
    {"EffectKindB1_Run", 0x493430, 0x1A, nullptr, 0, nullptr, 0, nullptr, 0, E4F_FN(EffectKindB1_Run), 0, false, kEf, 0, 3, 0, 0xB1},
    {"EffectKindB1_Start", 0x493450, 0x53, nullptr, 0, nullptr, 0, nullptr, 0, E4F_FN(EffectKindB1_Start), 0, false, kEf, 0, 0, 0, 0xB1},
    {"EffectKindB1_Sway", 0x4934B0, 0x48, E4F_CALLS(kCalls4934B0), nullptr, 0, nullptr, 0, E4F_FN(EffectKindB1_Sway), 0, false, kEf, 0, 0, 0, 0xB1},
    {"EffectKindB1_SwayBack", 0x493500, 0x48, E4F_CALLS(kCalls493500), nullptr, 0, nullptr, 0, E4F_FN(EffectKindB1_SwayBack), 0, false, kEf, 0, 0, 0, 0xB1},
    {"EffectKindB9_Run", 0x493550, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E4F_FN(EffectKindB9_Run), 0, false, kEf, 0, 9, 0, 0xB9},
    {"EffectKindB9_Grow", 0x493570, 0x53, E4F_CALLS(kCalls493570), nullptr, 0, nullptr, 0, E4F_FN(EffectKindB9_Grow), 0, false, kEf, 0, 0, 0, 0xB9},
    {"EffectKindB9_Hold", 0x4935D0, 0x3E, E4F_CALLS(kCalls4935D0), nullptr, 0, nullptr, 0, E4F_FN(EffectKindB9_Hold), 0, false, kEf, 0, 0, 0, 0xB9},
    {"EffectKindB9_Rise", 0x493610, 0xB3, E4F_CALLS(kCalls493610), nullptr, 0, nullptr, 0, E4F_FN(EffectKindB9_Rise), 0, false, kEf, 0, 0, 0, 0xB9},
    {"EffectKindB9_Burst", 0x4936D0, 0x7D, E4F_CALLS(kCalls4936D0), nullptr, 0, nullptr, 0, E4F_FN(EffectKindB9_Burst), 0, false, kEf, 0, 0, 0, 0xB9},
    {"EffectKindB9_Launch", 0x493750, 0x38, E4F_CALLS(kCalls493750), nullptr, 0, nullptr, 0, E4F_FN(EffectKindB9_Launch), 0, false, kEf, 0, 0, 0, 0xB9},
    {"EffectKindB9_Fly", 0x493790, 0xB3, E4F_CALLS(kCalls493790), nullptr, 0, nullptr, 0, E4F_FN(EffectKindB9_Fly), 0, false, kEf, 0, 0, 0, 0xB9},
    {"EffectKindB9_FlyWait", 0x493850, 0x84, E4F_CALLS(kCalls493850), nullptr, 0, nullptr, 0, E4F_FN(EffectKindB9_FlyWait), 0, false, kEf, 0, 0, 0, 0xB9},
    {"EffectKindB9_DrawTrail", 0x4938E0, 0x261, E4F_CALLS(kCalls4938E0), nullptr, 0, nullptr, 0, E4F_FN(EffectKindB9_DrawTrail), 0, false, kCa, 0, 0, 0, 0xB9},
    {"EffectKindB9_SpawnSpark", 0x493B50, 0x4A, nullptr, 0, nullptr, 0, nullptr, 0, E4F_FN(EffectKindB9_SpawnSpark), 0, false, kEf, 0, 0, 0, 0xB9},
    {"EffectKindB9_StepSparks", 0x493BA0, 0x45, E4F_CALLS(kCalls493BA0), nullptr, 0, nullptr, 0, E4F_FN(EffectKindB9_StepSparks), 0xFF, false, kEf, 0, 0, 0, 0xB9},
    {"EffectKindB9_DrawShards", 0x493BF0, 0x63, E4F_CALLS(kCalls493BF0), nullptr, 0, nullptr, 0, E4F_FN(EffectKindB9_DrawShards), 0, false, kEf, 0, 0, 0, 0xB9},
    {"EffectKindB9_DrawShard", 0x493C60, 0x1AD, E4F_CALLS(kCalls493C60), nullptr, 0, nullptr, 0, E4F_FN(EffectKindB9_DrawShard), 0, false, kCa, 0, 0, 0, 0xB9},
    {"EffectKindBA_Run", 0x493E10, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E4F_FN(EffectKindBA_Run), 0, false, kEf, 0, 3, 0, 0xBA},
    {"EffectKindBA_Start", 0x493E30, 0x1F, E4F_CALLS(kCalls493E30), nullptr, 0, nullptr, 0, E4F_FN(EffectKindBA_Start), 0, false, kEf, 0, 0, 0, 0xBA},
    {"EffectKindBA_DrawLine", 0x493F70, 0xB7, E4F_CALLS(kCalls493F70), nullptr, 0, nullptr, 0, E4F_FN(EffectKindBA_DrawLine), 0, false, kCa, 0, 0, 0, 0xBA},
};
#undef E4F_FN
#undef E4F_CALLS
#undef E4F_N

enum : unsigned {
    kAARun,
    kABRun, kABStart, kABMove, kABDrawDrop, kABPlace, kABEmit,
    kACRun,
    kADRun, kNext, kADArc,
    kAERun, kAESink, kAEWiden, kAENarrow, kAEShrink, kAEDraw, kEllipse,
    kAFRun, kAFStart, kAFIn, kAFOut, kAFScreen,
    kB0Run, kB0Start, kB0Spawn, kB0Clear, kB0New, kB0Step,
    kB1Run, kB1Start, kB1Sway, kB1Back,
    kB9Run, kB9Grow, kB9Hold, kB9Rise, kB9Burst, kB9Launch, kB9Fly, kB9FlyWait, kB9Trail, kB9Spawn, kB9Step,
    kB9Shards, kB9Shard,
    kBARun, kBAStart, kBALine, kCount
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

// EffectGte_ProjectSize(in, size, out): out's two s16, small half the time (the
// harness's FxProjectSize); re-listed to hash both words of the size, which
// Effect_DrawEllipse writes (w, h) - the standard row hashes the first only.
U FxProjectSize(const U* a, U answer) {
    if (Writable(a[2], 4)) {
        const U n = sh::Noise();
        const U v = (n & 1) ? (n >> 1) : ((n >> 1) & 0x003F003Fu);
        std::memcpy(P(a[2]), &v, 4);
    }
    return answer;
}
// EffectKind81_FindFreeDrop: as the real one, the first of the 256 drop records
// whose +0 is 0, or null; null also a quarter of the time (effect_3d_fuzz.cpp's).
U FxFindFreeDrop(const U*, U answer) {
    if (answer % 4 == 0) return 0;
    for (U i = 0; i < at::kDropCount; ++i)
        if (sh::Mem(at::kDrops + at::kDropStride * i)[0] == 0) return at::kDrops + at::kDropStride * i;
    return 0;
}

#define E4F_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
constexpr sh::Answer kG = sh::Answer::kGarbage, kPh = sh::Answer::kPhase, kF = sh::Answer::kFlag;
constexpr U kW = 0xFFFFFFFFu, k16 = 0xFFFFu, k8 = 0xFFu;
const sh::Callee kCallees[] = {
    // the group's own called directly, by name: the void ones with no argument
    {E4F_OURS(EffectKindAB_PlaceSources), 0, {}, kPh, 0, 0},
    {E4F_OURS(EffectKindAE_Draw), 0, {}, kPh, 0, 0},
    {E4F_OURS(EffectKindB0_ClearBars), 0, {}, kPh, 0, 0},
    {E4F_OURS(EffectKindB0_NewBar), 0, {}, kPh, 0, 0},
    {E4F_OURS(EffectKindB9_SpawnSpark), 0, {}, kPh, 0, 0},
    {E4F_OURS(EffectKindB9_DrawShards), 0, {}, kPh, 0, 0},
    // answering in al (no caller of the group reads it; the original's al)
    {E4F_OURS(EffectKindB9_StepSparks), 0, {}, kF, 0, 0},
    // those with arguments, each mask what the callee reads: the record's
    // points (same address on both sides) logged and hashed; a drop of 0x18,
    // a shard of 0x2C; the ellipse's point a local (its address differs, only
    // its 12 bytes compared), its w / h / angle words (`mov cx, [esp + 0x3c]`,
    // `and edi, 0xffff`; pushed as whole registers) and shade / rim bytes; the
    // screen's shade a byte (`mov cl, [esp + 0x14]`, pushed in eax over the
    // record pointer's upper bytes); the bars' length a word (`mov bp, [esp +
    // 0x14]`)
    {E4F_OURS(EffectKindAB_DrawDrop), 1, {kW}, kG, 0, 0, {0x18}, nullptr, nullptr, true},
    {E4F_OURS(Effect_DrawEllipse), 6, {0, k16, k16, k16, k8, k8}, kG, 0, 0, {12}, nullptr, nullptr, true},
    {E4F_OURS(EffectKindAF_DrawScreen), 1, {k8}, kG, 0, 0},
    {E4F_OURS(EffectKindB0_StepBars), 1, {k16}, kF, 0, 0},
    {E4F_OURS(EffectKindB9_DrawTrail), 3, {kW, kW, k8}, kG, 0, 0, {12, 12}, nullptr, nullptr, true},
    {E4F_OURS(EffectKindB9_DrawShard), 1, {kW}, kG, 0, 0, {0x2C}, nullptr, nullptr, true},
    // other groups' (merged), by name: E3D's drops, E3A's sparks and shards
    {E4F_OURS(EffectKind81_ClearDrops), 0, {}, kPh, 0, 0},
    {E4F_OURS(EffectKind81_FindFreeDrop), 0, {}, kG, 0, 0, {}, &FxFindFreeDrop, nullptr, true},
    {E4F_OURS(EffectKind64_ClearSparks), 0, {}, kPh, 0, 0},
    {E4F_OURS(EffectKind64_InitShard), 1, {kW}, kG, 0, 0},
    {E4F_OURS(EffectKind64_DrawSpark), 1, {kW}, kG, 0, 0, {0x18}, nullptr, nullptr, true},
    // raw: group E4E's glow (wave four, beside this one) - the record's point
    // (12 read), the size a word (`mov ax, [esp + 0x60]`, pushed in ecx over
    // the record pointer's upper bytes), the colour a byte; catalog part 6's
    // bar draw - the bar read to +5
    {"0x4901D0", at::kGlowDraw, at::kGlowDraw, 3, {kW, k16, k8}, kG, 0, 0, {12}, nullptr, nullptr, true},
    {"EffectKindA8_DrawBar", at::kBarDraw, at::kBarDraw, 1, {kW}, kG, 0, 0, {6}, nullptr, nullptr, true},
    // a standard row re-listed: the size's two words hashed
    {E4F_OURS(EffectGte_ProjectSize), 3, {0, 0, 0}, kG, 0, 0, {12, 4, 0}, &FxProjectSize, nullptr, true},
};
#undef E4F_OURS

// The ten state tables the dispatchers jump (kind 0xB1's calls) through, read in
// place; each table's own length (symbols.toml [[data]], none bounded by a
// compare; the tables sit back to back, 0x6552BC..0x655350).
const sh::DataTable kTables[] = {
    {0x6552A0, 3}, {0x6552BC, 3}, {0x6552C8, 3}, {0x6552D4, 5}, {0x6552E8, 5},
    {0x6552FC, 3}, {0x655308, 3}, {0x655314, 3}, {0x655320, 9}, {0x655344, 3},
};
const std::uint8_t kKinds[] = {0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB0, 0xB1, 0xB9, 0xBA};

// Beyond the standard effect regions (0x92BF80 + 0x644): the rest of kind 0xAB's
// 256 drops, its eight sources and kind 0xB0's sixteen bars, to 0x92D880.
const sh::Region kRegions[] = {
    {0x92C5C4, at::kPoolsEnd - 0x92C5C4},
};

// --- the seed ------------------------------------------------------------------------

unsigned char* Rec(unsigned r) { return sh::EffectRecord(r); }
unsigned char* DropAt(U i) { return P(at::kDrops + i * at::kDropStride); }
unsigned char* SourceAt(U i) { return P(at::kSources + i * at::kSourceStride); }
unsigned char* BarAt(U i) { return P(at::kBars + i * at::kBarStride); }
unsigned char* SparkAt(U i) { return P(at::kSparks + i * at::kSparkStride); }
unsigned char* ShardAt(U i) { return P(at::kShards + i * at::kShardStride); }

void SeedDrops() {
    for (U i = 0; i < at::kDropCount; ++i) {
        unsigned char* const d = DropAt(i);
        d[0] = static_cast<unsigned char>(sh::Next() % 4 == 0 ? 0 : sh::Next());
        d[2] = static_cast<unsigned char>(PickOf(1, 2, 0, sh::Next()));
        d[3] = static_cast<unsigned char>(PickOf(0, 1, sh::Next()));
    }
}
void SeedSources() {
    for (U i = 0; i < at::kSourceCount; ++i) {
        unsigned char* const src = SourceAt(i);
        src[0] = static_cast<unsigned char>(sh::Next() % 3 == 0 ? 0 : PickOf(1, 1, sh::Next()));
        src[3] = static_cast<unsigned char>(PickOf(0, 0, 1, sh::Next()));
    }
    // the drop pool full a quarter of the time, else some free
    const bool full = sh::Next() % 4 == 0;
    for (U i = 0; i < at::kDropCount; ++i) DropAt(i)[0] = static_cast<unsigned char>(full ? 1 : (sh::Next() % 3 == 0 ? 0 : 1));
}
void SeedBars() {
    for (U i = 0; i < at::kBarCount; ++i) {
        unsigned char* const b = BarAt(i);
        b[0] = static_cast<unsigned char>(sh::Half() ? 0 : PickOf(1, 1, sh::Next()));
        b[2] = static_cast<unsigned char>(PickOf(0, 1, 2, 2, 3, sh::Next()));
        SetWord(b + 4, PickOf(0, 7, 8, 9, 0x7FFF, 0x8000, 0x8007, 0xF0, sh::Next()));
    }
}
void SeedSparks() {
    for (U i = 0; i < at::kSparkCount; ++i) {
        unsigned char* const k = SparkAt(i);
        k[0] = static_cast<unsigned char>(sh::Half() ? 0 : PickOf(1, 1, sh::Next()));
        k[2] = static_cast<unsigned char>(PickOf(1, 1, 2, 0, 0x10, sh::Next()));
    }
}
void SeedShards() {
    for (U i = 0; i < at::kShardCount; ++i)
        SetWord(ShardAt(i) + 0x2A, PickOf(0, 0xFF, 0x100, 0xFFFF, 0x80, 0x7FFF, 0x8000, 0x20, sh::Next()));
}

void Seed(unsigned k) {
    unsigned char* const s = Sprite_Current;
    s[9] = static_cast<unsigned char>(PickOf(1, 1, 0, 2, 0x10, 0x11, 0xF0, 0xFF, sh::Next()));
    s[6] = static_cast<unsigned char>(PickOf(0, 1, 2, 3, 4, sh::Next()));
    Draw_PassFlags = static_cast<unsigned char>(PickOf(0, 0, 1, 2, sh::Next()));
    sh::Mem(at::kCounter)[0] = static_cast<unsigned char>(PickOf(3, 3, 2, 4, sh::Next()));
    switch (k) {
    case kABMove:
    case kABDrawDrop: SeedDrops(); break;
    case kABEmit: SeedSources(); break;
    case kB0Spawn: s[9] = static_cast<unsigned char>(PickOf(0, 0, 1, 2, sh::Next())); [[fallthrough]];
    case kB0Step:
    case kB0New:
    case kB0Clear: SeedBars(); break;
    case kB1Sway:
    case kB1Back:
        SetWord(sh::Mem(at::kTilt), PickOf(0xEC, 0xED, 0xEE, 0xEF, 0xF0, 0xF1, 0xFF10, 0xFF11, 0xFF13, 0xFF14, 0xFF0F, sh::Next()));
        Field_Request = static_cast<unsigned char>(PickOf(0, 0, 1, sh::Next()));
        break;
    case kAFIn: s[9] = static_cast<unsigned char>(PickOf(0xF0, 0xF0, 0xE0, 0, 0xEF, sh::Next())); break;
    case kAFOut: s[9] = static_cast<unsigned char>(PickOf(0x10, 0x10, 0x20, 0, 0x11, sh::Next())); break;
    case kB9FlyWait: s[9] = static_cast<unsigned char>(PickOf(0x10, 1, 0x11, 2, sh::Next())); [[fallthrough]];
    case kB9Burst:
    case kB9Launch:
    case kB9Fly:
    case kB9Spawn:
    case kB9Step: SeedSparks(); SeedShards(); break;
    case kB9Shards:
    case kB9Shard: SeedShards(); break;
    case kB9Trail:
        // from (+0x18) behind to (+0x34) on x by 0..9 steps of 0x10000, half
        // the time, so the dots run (a plant in the seed: the args hook's writes
        // are lost)
        if (sh::Half())
            move_script::SetLong(s + 0x34, move_script::Long(s + 0x18) + static_cast<std::int32_t>(sh::Next() % 0xA0000));
        break;
    default: break;
    }
}

// The helpers with arguments: the record's own points (as the states pass
// them), a pool record, the sizes, shades and lengths over leftover upper bytes.
void Args(unsigned k, U* a) {
    const U s = Key(Sprite_Current);
    switch (k) {
    case kABDrawDrop: a[0] = Key(DropAt(sh::Next() % at::kDropCount)); break;
    case kADArc:
        a[0] = s + 0x34;
        a[1] = s + 0xC;
        break;
    case kEllipse:
        a[0] = Key(Rec(sh::Next() % 20) + 0x34);
        a[1] = (sh::Next() & 0xFFFF0000u) | PickOf(0, 0x40, 0x100, 0xFFC0, 0x8000, sh::Next() & 0xFFFF);
        a[2] = (sh::Next() & 0xFFFF0000u) | PickOf(0, 0x40, 0x100, 0xFFC0, 0x8000, sh::Next() & 0xFFFF);
        break;
    case kB0Step: a[0] = (sh::Next() & 0xFFFF0000u) | PickOf(0xF0, 0, 8, 0x8000, sh::Next() & 0xFFFF); break;
    case kB9Trail:
        a[0] = s + 0x18;
        a[1] = s + 0x34;
        break;
    case kB9Shard: a[0] = Key(ShardAt(sh::Next() % at::kShardCount)); break;
    case kBALine:
        a[0] = s + 0x34;
        a[1] = s + 0xC;
        break;
    default: break;
    }
}

// What the functions read again after a call, moved by the group's case of the
// harness's disturbance (from its hash only): the record's counts, frame and
// shade, Draw_PassFlags, the chapter's count, the camera's tilt, a drop's life,
// a bar's word.
void Disturb(U h) {
    const U v = h >> 8;
    unsigned char* const s = Sprite_Current;
    switch (h % 8) {
    case 0:
        if (sh::InRegions(s, 0x80)) s[9] = static_cast<unsigned char>((v & 1) ? 1u : v >> 1);
        break;
    case 1:
        if (sh::InRegions(s, 0x80)) s[6] = static_cast<unsigned char>(v);
        break;
    case 2: Draw_PassFlags = static_cast<unsigned char>((v & 1) ? 0u : v >> 1); break;
    case 3: sh::Mem(at::kCounter)[0] = static_cast<unsigned char>((v & 1) ? 3u : v >> 1); break;
    case 4: SetWord(sh::Mem(at::kTilt), (v & 1) ? 0xF1u : v >> 1); break;
    case 5:
        if (sh::InRegions(s, 0x80)) s[3] = static_cast<unsigned char>(v);
        break;
    case 6: DropAt(v % at::kDropCount)[2] = static_cast<unsigned char>(v >> 8); break;
    case 7: SetWord(BarAt(v % at::kBarCount) + 4, v >> 4); break;
    default: break;
    }
}

}  // namespace

void SelfTest() {
    // BOF3X_E4F_ONLY: the clones whose name contains it (a control's run)
    static sh::Clone chosen[kCount];
    static unsigned index[kCount];
    const char* const only = std::getenv("BOF3X_E4F_ONLY");
    unsigned n = 0;
    for (unsigned k = 0; k < kCount; ++k)
        if (!only || !*only || std::strstr(kAll[k].name, only)) {
            index[n] = k;
            chosen[n++] = kAll[k];
        }
    if (n == 0) bof3::Fatal("effect_4f: BOF3X_E4F_ONLY=%s names no clone", only);
    static unsigned* s_index = index;
    sh::Group g = {"effect_4f", chosen, n, kCallees, sizeof kCallees / sizeof kCallees[0], kTables,
                   sizeof kTables / sizeof kTables[0], kRegions, sizeof kRegions / sizeof kRegions[0],
                   [](unsigned k) { Seed(s_index[k]); }, &Disturb, 4000};
    if (const char* r = std::getenv("BOF3X_E4F_ROUNDS")) g.rounds = static_cast<unsigned>(std::atoi(r));
    g.args = [](unsigned k, U* a) { Args(s_index[k], a); };
    g.effect = true;
    g.kinds = kKinds;
    g.n_kinds = sizeof kKinds;
    sh::Run(g);
}

}  // namespace effect_4f

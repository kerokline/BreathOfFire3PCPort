// BOF3X_SHADOW=effect_3b: group E3B's 49 functions through the scenario harness
// in effect mode (scenario_harness.h, docs/scenario_harness.md section 8), once
// at start-up. docs/effect_3b.md section 4. BOF3X_E3B_ONLY=<name> runs the
// clones whose name contains it (the controls' speed-up).
//
// The clone table is tools/band_rows.py --group E3B --clones --harness scenario
// (2026-10-03), each extent read again to its last instruction (capstone) and
// the names given; no start dropped or added. Shapes: every dispatcher, state
// and no-argument helper kEffect (Sprite_Current one of the 20 Effect_Objects
// records, +5 the kind, a dispatcher's byte below its table's length); the
// three draws with arguments kCall.
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_3b.h"
#include "game/effect_3b_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace effect_3b {
namespace {

namespace sh = scenario_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using U = std::uint32_t;

// tools/band_rows.py --group E3B --clones, 2026-10-03.
constexpr sh::CallSite kCalls4823F0[] = {{0x26, 0x5720C0}};
constexpr sh::CallSite kCalls482470[] = {{0xD, 0x494110}, {0x25, 0x482740}};
constexpr sh::CallSite kCalls4824E0[] = {{0xD, 0x494110}, {0x27, 0x482740}};
constexpr sh::CallSite kCalls482550[] = {{0xD, 0x494110}, {0x25, 0x482740}, {0x2D, 0x48CA90}, {0x32, 0x4826B0}};
constexpr sh::CallSite kCalls4825D0[] = {{0x1C, 0x48CA90}, {0x21, 0x4826B0}};
constexpr sh::CallSite kCalls482600[] = {{0x3E, 0x48CA90}, {0x43, 0x4826B0}};
constexpr sh::CallSite kCalls482650[] = {{0x4C, 0x589840}};
constexpr sh::CallSite kCalls4826B0[] = {{0x25, 0x588F20}, {0x64, 0x588F20}};
constexpr sh::CallSite kCalls482740[] = {{0x11, 0x5A79A0}, {0x28, 0x5A77C0}, {0x31, 0x461E50}, {0x4D, 0x5A75F0}, {0x55, 0x5A7780},
                                         {0x86, 0x5A7A50}, {0xA9, 0x5A7A00}, {0xCF, 0x5A7A50}, {0xEE, 0x5A7A00}, {0x142, 0x461E50}};
constexpr sh::CallSite kCalls4828F0[] = {{0x26, 0x57C7C0}};
constexpr sh::CallSite kCalls482930[] = {{0xF, 0x587740}, {0x1B, 0x57C160}};
constexpr sh::CallSite kCalls482970[] = {{0x1B, 0x4976D0}};
constexpr sh::CallSite kCalls4829D0[] = {{0x9, 0x57C7A0}, {0x13, 0x587740}};
constexpr sh::CallSite kCalls482A20[] = {{0x0, 0x589810}};
constexpr sh::Imm kImms482A80[] = {{0xF, 0x482AB0}, {0x17, 0x482BB0}, {0x22, 0x482BD0}};
constexpr sh::CallSite kCalls482AB0[] = {{0xC, 0x589810}, {0x46, 0x589810}, {0x8D, 0x589810}, {0xE9, 0x587740}};
constexpr sh::CallSite kCalls482BB0[] = {{0xB, 0x589840}};
constexpr sh::CallSite kCalls482BF0[] = {{0x12, 0x483B00}, {0x24, 0x482C30}, {0x2F, 0x483080}, {0x37, 0x5A7BC0}};
constexpr sh::CallSite kCalls482C30[] = {{0x3, 0x5A7B90}, {0x6E, 0x5A8200}, {0x7D, 0x5A8060}, {0x91, 0x5A7D70}, {0x9B, 0x5A8DE0}, {0xA5, 0x5A8E00}};
constexpr sh::CallSite kCalls482CE0[] = {{0x0, 0x5B93D2}, {0x55, 0x5720C0}};
constexpr sh::CallSite kCalls482DB0[] = {{0x2E, 0x589840}};
constexpr sh::CallSite kCalls482DF0[] = {{0x12, 0x483B00}, {0x24, 0x482C30}, {0x2F, 0x483080}, {0x37, 0x483970}, {0x3C, 0x5A7BC0}};
constexpr sh::CallSite kCalls482E40[] = {{0x0, 0x5B93D2}, {0x52, 0x5A7A50}, {0x78, 0x5A7A00}};
constexpr sh::CallSite kCalls482F30[] = {{0x37, 0x5A7A50}, {0x5D, 0x5A7A00}};
constexpr sh::CallSite kCalls482FD0[] = {{0x37, 0x5A7A50}, {0x5D, 0x5A7A00}, {0xA1, 0x589840}};
constexpr sh::CallSite kCalls483080[] = {
    {0x4B, 0x5A7A00},  {0x7A, 0x5A77C0},  {0x83, 0x461E50},  {0xAF, 0x5B93D2},  {0xB8, 0x5B93D2},  {0xD2, 0x5B93D2},  {0x130, 0x5A7A00},
    {0x171, 0x5A7610}, {0x178, 0x5A7780}, {0x219, 0x5A85F0}, {0x21F, 0x5A9350}, {0x23F, 0x461E50}, {0x267, 0x5A7610}, {0x26E, 0x5A7780},
    {0x2E4, 0x5A85F0}, {0x2EA, 0x5A9350}, {0x2F3, 0x461E50}, {0x31E, 0x5A7610}, {0x325, 0x5A7780}, {0x3CD, 0x5A85F0}, {0x3D3, 0x5A9350},
    {0x3DC, 0x461E50}, {0x404, 0x5A7610}, {0x40B, 0x5A7780}, {0x485, 0x5A85F0}, {0x48B, 0x5A9350}, {0x494, 0x461E50}};
constexpr sh::CallSite kCalls483540[] = {{0x12, 0x483B00}, {0x24, 0x482C30}, {0x29, 0x4837B0}, {0x2E, 0x483970}, {0x33, 0x5A7BC0}};
constexpr sh::CallSite kCalls483580[] = {{0x0, 0x5B93D2}, {0x55, 0x5A7A50}, {0x7B, 0x5A7A00}};
constexpr sh::CallSite kCalls483660[] = {{0x37, 0x5A7A50}, {0x5D, 0x5A7A00}};
constexpr sh::CallSite kCalls483700[] = {{0x37, 0x5A7A50}, {0x5D, 0x5A7A00}, {0xA2, 0x589840}};
constexpr sh::CallSite kCalls483970[] = {{0x4, 0x5B93D2},  {0x37, 0x5A77C0}, {0x40, 0x461E50}, {0x51, 0x5A75F0}, {0x58, 0x5A7780},
                                         {0x88, 0x5A7A00}, {0xAF, 0x5A7A50}, {0xDC, 0x5A7A00}, {0x103, 0x5A7A50}, {0x16C, 0x461E50}};
constexpr sh::CallSite kCalls483B00[] = {{0x43, 0x5A7750}, {0x5B, 0x5A8250}, {0x64, 0x5A9110}, {0x6E, 0x5B9550}, {0x80, 0x5B9550}};
constexpr sh::CallSite kCalls483BC0[] = {{0x26, 0x483C10}};
constexpr sh::CallSite kCalls483C00[] = {{0x0, 0x483D10}, {0x9, 0x589840}};
constexpr sh::CallSite kCalls483C10[] = {{0x48, 0x5B93D2}, {0x55, 0x5B93D2}, {0x72, 0x5A7A50}, {0x87, 0x5A7A00}};
constexpr sh::CallSite kCalls483D10[] = {{0x1, 0x494060}, {0x13, 0x5A79A0}, {0x2B, 0x5A77C0}, {0x34, 0x461E50}, {0x65, 0x483DA0}};
constexpr sh::CallSite kCalls483DA0[] = {{0xD, 0x5A75D0}, {0x15, 0x5A7780}, {0x27, 0x494110}, {0x45, 0x4941E0},
                                         {0x195, 0x5A79E0}, {0x1AC, 0x5A79A0}, {0x1E9, 0x461E50}};

#define E3B_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define E3B_CALLS(a) a, E3B_N(a)
#define E3B_FN(name) reinterpret_cast<const void*>(&::name)
constexpr sh::Shape kEf = sh::Shape::kEffect;
constexpr sh::Shape kCa = sh::Shape::kCall;
// {name, base, size, calls, imms, tables, ours, ret_mask, calm, shape, pointers, state_span, sub_span, kind}
const sh::Clone kAll[] = {
    {"EffectKind63_Run", 0x4823D0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E3B_FN(EffectKind63_Run), 0, false, kEf, 0, 7, 0, 0x63},
    {"EffectKind63_Start", 0x4823F0, 0x73, E3B_CALLS(kCalls4823F0), nullptr, 0, nullptr, 0, E3B_FN(EffectKind63_Start), 0, false, kEf, 0, 0, 0, 0x63},
    {"EffectKind63_Grow", 0x482470, 0x64, E3B_CALLS(kCalls482470), nullptr, 0, nullptr, 0, E3B_FN(EffectKind63_Grow), 0, false, kEf, 0, 0, 0, 0x63},
    {"EffectKind63_Hold", 0x4824E0, 0x67, E3B_CALLS(kCalls4824E0), nullptr, 0, nullptr, 0, E3B_FN(EffectKind63_Hold), 0, false, kEf, 0, 0, 0, 0x63},
    {"EffectKind63_Shrink", 0x482550, 0x76, E3B_CALLS(kCalls482550), nullptr, 0, nullptr, 0, E3B_FN(EffectKind63_Shrink), 0, false, kEf, 0, 0, 0, 0x63},
    {"EffectKind63_WaitCue", 0x4825D0, 0x26, E3B_CALLS(kCalls4825D0), nullptr, 0, nullptr, 0, E3B_FN(EffectKind63_WaitCue), 0, false, kEf, 0, 0, 0, 0x63},
    {"EffectKind63_FadeOut", 0x482600, 0x48, E3B_CALLS(kCalls482600), nullptr, 0, nullptr, 0, E3B_FN(EffectKind63_FadeOut), 0, false, kEf, 0, 0, 0, 0x63},
    {"EffectKind63_End", 0x482650, 0x54, E3B_CALLS(kCalls482650), nullptr, 0, nullptr, 0, E3B_FN(EffectKind63_End), 0, false, kEf, 0, 0, 0, 0x63},
    {"EffectKind63_RefreshSprites", 0x4826B0, 0x84, E3B_CALLS(kCalls4826B0), nullptr, 0, nullptr, 0, E3B_FN(EffectKind63_RefreshSprites), 0, false, kEf, 0, 0, 0, 0x63},
    {"EffectKind63_DrawDisc", 0x482740, 0x16D, E3B_CALLS(kCalls482740), nullptr, 0, nullptr, 0, E3B_FN(EffectKind63_DrawDisc), 0, false, kCa, 0, 0, 0, 0x63},
    {"EffectKind65_Run", 0x4828B0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E3B_FN(EffectKind65_Run), 0, false, kEf, 0, 5, 0, 0x65},
    {"EffectKind65_WaitRequest", 0x4828D0, 0x13, nullptr, 0, nullptr, 0, nullptr, 0, E3B_FN(EffectKind65_WaitRequest), 0, false, kEf, 0, 0, 0, 0x65},
    {"EffectKind65_Check", 0x4828F0, 0x35, E3B_CALLS(kCalls4828F0), nullptr, 0, nullptr, 0, E3B_FN(EffectKind65_Check), 0, false, kEf, 0, 0, 0, 0x65},
    {"EffectKind65_WaitInput", 0x482930, 0x37, E3B_CALLS(kCalls482930), nullptr, 0, nullptr, 0, E3B_FN(EffectKind65_WaitInput), 0, false, kEf, 0, 0, 0, 0x65},
    {"EffectKind65_Shake", 0x482970, 0x52, E3B_CALLS(kCalls482970), nullptr, 0, nullptr, 0, E3B_FN(EffectKind65_Shake), 0, false, kEf, 0, 0, 0, 0x65},
    {"EffectKind65_Close", 0x4829D0, 0x25, E3B_CALLS(kCalls4829D0), nullptr, 0, nullptr, 0, E3B_FN(EffectKind65_Close), 0, false, kEf, 0, 0, 0, 0x65},
    {"EffectKind67_Run", 0x482A00, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E3B_FN(EffectKind67_Run), 0, false, kEf, 0, 3, 0, 0x67},
    {"EffectKind67_SpawnKind13", 0x482A20, 0x52, E3B_CALLS(kCalls482A20), nullptr, 0, nullptr, 0, E3B_FN(EffectKind67_SpawnKind13), 0, false, kEf, 0, 0, 0, 0x67},
    {"EffectKind69_Run", 0x482A80, 0x2E, nullptr, 0, E3B_CALLS(kImms482A80), nullptr, 0, E3B_FN(EffectKind69_Run), 0, false, kEf, 0, 3, 0, 0x69},
    {"EffectKind69_Spawn", 0x482AB0, 0xF4, E3B_CALLS(kCalls482AB0), nullptr, 0, nullptr, 0, E3B_FN(EffectKind69_Spawn), 0, false, kEf, 0, 0, 0, 0x69},
    {"EffectKind69_WaitParts", 0x482BB0, 0x11, E3B_CALLS(kCalls482BB0), nullptr, 0, nullptr, 0, E3B_FN(EffectKind69_WaitParts), 0, false, kEf, 0, 0, 0, 0x69},
    {"EffectKind69_PartRun", 0x482BD0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E3B_FN(EffectKind69_PartRun), 0, false, kEf, 0, 0, 3, 0x69},
    {"EffectKind69_Part0", 0x482BF0, 0x3D, E3B_CALLS(kCalls482BF0), nullptr, 0, nullptr, 0, E3B_FN(EffectKind69_Part0), 0, false, kEf, 0, 0, 0, 0x69},
    {"EffectKind69_PushPointMatrix", 0x482C30, 0xAE, E3B_CALLS(kCalls482C30), nullptr, 0, nullptr, 0, E3B_FN(EffectKind69_PushPointMatrix), 0, false, kEf, 0, 0, 0, 0x69},
    {"EffectKind69_Part0Place", 0x482CE0, 0x70, E3B_CALLS(kCalls482CE0), nullptr, 0, nullptr, 0, E3B_FN(EffectKind69_Part0Place), 0, false, kEf, 0, 0, 0, 0x69},
    {"EffectKind69_Part0Grow", 0x482D50, 0x2D, nullptr, 0, nullptr, 0, nullptr, 0, E3B_FN(EffectKind69_Part0Grow), 0, false, kEf, 0, 0, 0, 0x69},
    {"EffectKind69_Part0Hold", 0x482D80, 0x2A, nullptr, 0, nullptr, 0, nullptr, 0, E3B_FN(EffectKind69_Part0Hold), 0, false, kEf, 0, 0, 0, 0x69},
    {"EffectKind69_Part0Fade", 0x482DB0, 0x34, E3B_CALLS(kCalls482DB0), nullptr, 0, nullptr, 0, E3B_FN(EffectKind69_Part0Fade), 0, false, kEf, 0, 0, 0, 0x69},
    {"EffectKind69_Part1", 0x482DF0, 0x42, E3B_CALLS(kCalls482DF0), nullptr, 0, nullptr, 0, E3B_FN(EffectKind69_Part1), 0, false, kEf, 0, 0, 0, 0x69},
    {"EffectKind69_Part1Place", 0x482E40, 0xB7, E3B_CALLS(kCalls482E40), nullptr, 0, nullptr, 0, E3B_FN(EffectKind69_Part1Place), 0, false, kEf, 0, 0, 0, 0x69},
    {"EffectKind69_Part1Grow", 0x482F00, 0x2D, nullptr, 0, nullptr, 0, nullptr, 0, E3B_FN(EffectKind69_Part1Grow), 0, false, kEf, 0, 0, 0, 0x69},
    {"EffectKind69_Part1Orbit", 0x482F30, 0x9D, E3B_CALLS(kCalls482F30), nullptr, 0, nullptr, 0, E3B_FN(EffectKind69_Part1Orbit), 0, false, kEf, 0, 0, 0, 0x69},
    {"EffectKind69_Part1Fade", 0x482FD0, 0xA7, E3B_CALLS(kCalls482FD0), nullptr, 0, nullptr, 0, E3B_FN(EffectKind69_Part1Fade), 0, false, kEf, 0, 0, 0, 0x69},
    {"EffectKind69_DrawColumn", 0x483080, 0x4BD, E3B_CALLS(kCalls483080), nullptr, 0, nullptr, 0, E3B_FN(EffectKind69_DrawColumn), 0, false, kCa, 0, 0, 0, 0x69},
    {"EffectKind69_Part2", 0x483540, 0x39, E3B_CALLS(kCalls483540), nullptr, 0, nullptr, 0, E3B_FN(EffectKind69_Part2), 0, false, kEf, 0, 0, 0, 0x69},
    {"EffectKind69_Part2Place", 0x483580, 0xBA, E3B_CALLS(kCalls483580), nullptr, 0, nullptr, 0, E3B_FN(EffectKind69_Part2Place), 0, false, kEf, 0, 0, 0, 0x69},
    {"EffectKind69_Part2Grow", 0x483640, 0x20, nullptr, 0, nullptr, 0, nullptr, 0, E3B_FN(EffectKind69_Part2Grow), 0, false, kEf, 0, 0, 0, 0x69},
    {"EffectKind69_Part2Orbit", 0x483660, 0x9D, E3B_CALLS(kCalls483660), nullptr, 0, nullptr, 0, E3B_FN(EffectKind69_Part2Orbit), 0, false, kEf, 0, 0, 0, 0x69},
    {"EffectKind69_Part2Fade", 0x483700, 0xA8, E3B_CALLS(kCalls483700), nullptr, 0, nullptr, 0, E3B_FN(EffectKind69_Part2Fade), 0, false, kEf, 0, 0, 0, 0x69},
    {"EffectKind69_DrawGlow", 0x483970, 0x185, E3B_CALLS(kCalls483970), nullptr, 0, nullptr, 0, E3B_FN(EffectKind69_DrawGlow), 0, false, kEf, 0, 0, 0, 0x69},
    {"EffectKind69_UpdateScreenXY", 0x483B00, 0x95, E3B_CALLS(kCalls483B00), nullptr, 0, nullptr, 0, E3B_FN(EffectKind69_UpdateScreenXY), 0, false, kEf, 0, 0, 0, 0x69},
    {"EffectKind6C_Run", 0x483BA0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E3B_FN(EffectKind6C_Run), 0, false, kEf, 0, 2, 0, 0x6C},
    {"EffectKind6C_Start", 0x483BC0, 0x34, E3B_CALLS(kCalls483BC0), nullptr, 0, nullptr, 0, E3B_FN(EffectKind6C_Start), 0, false, kEf, 0, 0, 0, 0x6C},
    {"EffectKind6C_Live", 0x483C00, 0xF, E3B_CALLS(kCalls483C00), nullptr, 0, nullptr, 0, E3B_FN(EffectKind6C_Live), 0, false, kEf, 0, 0, 0, 0x6C},
    {"EffectKind6C_ScatterSparks", 0x483C10, 0xF1, E3B_CALLS(kCalls483C10), nullptr, 0, nullptr, 0, E3B_FN(EffectKind6C_ScatterSparks), 0, false, kEf, 0, 0, 0, 0x6C},
    {"EffectKind6C_DrawSparks", 0x483D10, 0x83, E3B_CALLS(kCalls483D10), nullptr, 0, nullptr, 0, E3B_FN(EffectKind6C_DrawSparks), 0xFF, false, kEf, 0, 0, 0, 0x6C},
    {"EffectKind6C_DrawSpark", 0x483DA0, 0x1F8, E3B_CALLS(kCalls483DA0), nullptr, 0, nullptr, 0, E3B_FN(EffectKind6C_DrawSpark), 0, false, kCa, 0, 0, 0, 0x6C},
    {"EffectKind6C_SparkFly", 0x483FA0, 0x55, nullptr, 0, nullptr, 0, nullptr, 0, E3B_FN(EffectKind6C_SparkFly), 0, false, kEf, 0, 0, 0, 0x6C},
    {"EffectKind6C_SparkFade", 0x484000, 0x4A, nullptr, 0, nullptr, 0, nullptr, 0, E3B_FN(EffectKind6C_SparkFade), 0, false, kEf, 0, 0, 0, 0x6C},
};
#undef E3B_FN
#undef E3B_CALLS
#undef E3B_N

enum : unsigned {
    k63Run, k63Start, k63Grow, k63Hold, k63Shrink, k63WaitCue, k63FadeOut, k63End, k63Refresh, k63Disc,
    k65Run, k65WaitRequest, k65Check, k65WaitInput, k65Shake, k65Close,
    k67Run, k67Spawn,
    k69Run, k69Spawn, k69WaitParts, k69PartRun, k69Part0, k69Push, k69P0Place, k69P0Grow, k69P0Hold, k69P0Fade,
    k69Part1, k69P1Place, k69P1Grow, k69P1Orbit, k69P1Fade, k69Column, k69Part2, k69P2Place, k69P2Grow, k69P2Orbit,
    k69P2Fade, k69Glow, k69Screen,
    k6CRun, k6CStart, k6CLive, k6CScatter, k6CDraw, k6CSpark, k6CFly, k6CFade, kCount
};
static_assert(kCount == sizeof kAll / sizeof kAll[0], "one enum entry a clone, in order");

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
unsigned char* P(U a) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a)); }
unsigned char* Mem(U a) { return sh::Mem(a); }
template <typename... T> U PickOf(T... v) {
    const U values[] = {static_cast<U>(v)...};
    return sh::Pick(values, sizeof...(v));
}

// The clone the round is fuzzing (Seed sets it; the effects read it - the same
// on both passes).
unsigned g_clone = 0;

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
// A float with a random mantissa and an exponent 2^-17..2^18 (no NaN, no infinity).
void FillFloat(unsigned char* at) {
    const U n = sh::Noise();
    const U bits = (n & 0x807FFFFFu) | ((0x6Eu + (sh::Noise() % 0x24u)) << 23);
    std::memcpy(at, &bits, 4);
}
// One time in eight a float _ftol answers with the integer indefinite (NaN, or
// past 2^63 either way) or a compare is unordered; else a fraction.
void FillOdd(unsigned char* at) {
    const U n = sh::Noise();
    if (n % 8 != 0) {
        FillFloat(at);
        return;
    }
    const U odd[] = {0x7FC00000u, 0xFFC00000u, 0x7F800001u, 0x5F000000u, 0xDF000001u, 0x7F7FFFFFu, 0xFF7FFFFFu};
    std::memcpy(at, &odd[(n >> 3) % 7], 4);
}
// The packet buffer (scenario_harness.cpp g_packets, 0x800 bytes; 0x40 kept spare).
constexpr unsigned kPacketsSize = 0x800;
void Advance(unsigned n) {
    unsigned char* const next = Gfx_PacketNext;
    unsigned char* const base = sh::Packets();
    if (next >= base && next + n + 0x40 <= base + kPacketsSize) Gfx_PacketNext = next + n;
}

// Effect_FindFree: as the effect-mode row (a free record from a start the
// answer picks, none a quarter of the time), except for kind 0x69's spawner,
// which never tests for none (its seed frees enough records): there the free
// record only, none only when none is free.
U FxFindFree(const U*, U answer) {
    const U high = answer & 0xFFFFFF00u;
    if (g_clone != k69Spawn && (answer >> 8) % 4 == 0) return high | 0xFF;
    const unsigned from = (answer >> 12) % at::kEffects;
    for (unsigned i = 0; i < at::kEffects; ++i) {
        const unsigned k = (from + i) % at::kEffects;
        if (sh::EffectRecord(k)[0] == 0) return high | k;
    }
    return high | 0xFF;
}
// Gte_RotTransPers (EffectKind69_UpdateScreenXY): the screen point, two floats
// the caller takes through _ftol - fractions, and the integer indefinite's
// values one time in eight; the depth word.
U FxRotTransPers(const U* a, U answer) {
    if (Writable(a[1], 8))
        for (unsigned i = 0; i < 2; ++i) FillOdd(P(a[1] + 4 * i));
    if (Writable(a[2], 4)) sh::FillBytes(P(a[2]), 4);
    return answer;
}
// Gte_RotTransPers4 (the column's quads): four screen points; the third's y is
// what the column culls on (below the float at 0x5C41DC or unordered) - 0.0,
// -0.0, NaN, the smallest values either side, or a fraction either sign.
U FxRotTransPers4(const U* a, U answer) {
    for (unsigned k = 4; k < 8; ++k) {
        if (!Writable(a[k], 8)) continue;
        FillFloat(P(a[k]));
        const U n = sh::Noise();
        const U edge[] = {0x00000000u, 0x80000000u, 0x7FC00000u, 0x00000001u, 0x80000001u, 0x3F800000u, 0xBF800000u};
        if (n % 2 == 0)
            std::memcpy(P(a[k] + 4), &edge[(n >> 1) % 7], 4);
        else
            FillFloat(P(a[k] + 4));
    }
    if (Writable(a[8], 4)) sh::FillBytes(P(a[8]), 4);
    return answer;
}
// The group's draws on Sprite_Current (the disc reads its point and floats, the
// column its +0xA / +0xB): the record they ran on, logged.
U FxOnCurrent(const U*, U answer) {
    sh::Note(Key(Sprite_Current));
    return answer;
}
// 0x48CA90 (E4D's): a full-screen TILE in Sprite_Current's +0x5D..+0x5F after a
// draw mode - the record and the three bytes logged, the cursor moved 0xC +
// 0x1C as its two commits do.
U FxScreenTint(const U*, U answer) {
    unsigned char* const s = Sprite_Current;
    sh::Note(Key(s));
    if (sh::InRegions(s, 0x80)) sh::Note(s[0x5D], s[0x5E], s[0x5F]);
    Advance(0x28);
    return answer;
}
// EffectKind69_DrawLines (R3F's): lines from Sprite_Current's +0xA / +0xB, writing the
// scratch cells 0x903850..0x90385F and the first vertex - the record and the
// two bytes logged, the cells filled.
U FxLines(const U*, U answer) {
    unsigned char* const s = Sprite_Current;
    sh::Note(Key(s));
    if (sh::InRegions(s, 0x80)) sh::Note(s[0xA], s[0xB]);
    if (sh::InRegions(Mem(at::kScale), 0x10)) sh::FillBytes(Mem(at::kScale), 0x10);
    if (sh::InRegions(Mem(at::kV0), 6)) sh::FillBytes(Mem(at::kV0), 6);
    Advance(0x30);
    return answer;
}

#define E3B_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
#define E3B_RAW(address) #address, address, address
constexpr sh::Answer kG = sh::Answer::kGarbage, kPh = sh::Answer::kPhase;
constexpr U kW = 0xFFFFFFFFu, k16 = 0xFFFFu, k8 = 0xFFu;
const sh::Callee kCallees[] = {
    // the group's own called directly, by name
    {E3B_OURS(EffectKind63_RefreshSprites), 0, {}, kPh, 0, 0},
    {E3B_OURS(EffectKind69_PushPointMatrix), 0, {}, kPh, 0, 0},
    {E3B_OURS(EffectKind69_DrawGlow), 0, {}, kPh, 0, 0},
    {E3B_OURS(EffectKind69_UpdateScreenXY), 0, {}, kPh, 0, 0},
    {E3B_OURS(EffectKind6C_ScatterSparks), 0, {}, kPh, 0, 0},
    {E3B_OURS(EffectKind6C_DrawSparks), 0, {}, kPh, 0, 0},
    // the radius's word (`mov ax, [+0x18]` over the pointer's upper half), the
    // two shades' bytes (`mov dl, [+0x5C]` over a leftover): what the disc reads
    {E3B_OURS(EffectKind63_DrawDisc), 3, {k16, k8, k8}, kG, 0, 0, {}, &FxOnCurrent, nullptr, true},
    // three immediates; the column reads the first's low word and the two s16
    {E3B_OURS(EffectKind69_DrawColumn), 3, {k16, k16, k16}, kG, 0, 0, {}, &FxOnCurrent, nullptr, true},
    // the spark record (the cursor's value, the same on both passes) and its 0x28 bytes
    {E3B_OURS(EffectKind6C_DrawSpark), 1, {kW}, kG, 0, 0, {0x28}, nullptr, nullptr, true},
    // raw: E4D's tint and the lines no group owns
    {"0x48CA90", at::kScreenTint, at::kScreenTint, 0, {}, kG, 0, 0, {}, &FxScreenTint, nullptr, true},   // at::kScreenTint
    {"EffectKind69_DrawLines", at::kKind69Lines, at::kKind69Lines, 0, {}, kG, 0, 0, {}, &FxLines, nullptr, true},        // at::kKind69Lines
    // standard rows re-listed: kind 0x69's spawner never answered none
    {E3B_OURS(Effect_FindFree), 0, {}, sh::Answer::kByte, 0xFF, 0x13, {}, &FxFindFree, nullptr, true},
    // the vertex on the stack hashed by its six bytes (its pad, the callers'
    // leftover, never written), the screen point filled with fractions and odd values
    {E3B_OURS(Gte_RotTransPers), 3, {0, kW, 0}, kG, 0, 0, {6, 0, 0}, &FxRotTransPers, nullptr, true},
    // the four SVECTORs of Prim_VertexScratch hashed (six bytes each), the
    // screen points in the packet: the culled y at its edges
    {E3B_OURS(Gte_RotTransPers4), 9, {0, 0, 0, 0, kW, kW, kW, kW, 0}, kG, 0, 0, {6, 6, 6, 6, 0, 0, 0, 0, 0}, &FxRotTransPers4,
     nullptr, true},
};
#undef E3B_RAW
#undef E3B_OURS

// The tables the dispatchers go through, read in place; each its own length
// (to the next table a dispatcher names; none bounded by a compare).
const sh::DataTable kTables[] = {
    {0x6549B8, 7},   // EffectKind63_States
    {0x6549D4, 5},   // EffectKind65_States
    {0x6549EC, 3},   // EffectKind67_States
    {0x6549F8, 3},   // EffectKind69_Parts
    {0x654A04, 4},   // EffectKind69_Part0Steps
    {0x654A14, 4},   // EffectKind69_Part1Steps
    {0x654A24, 4},   // EffectKind69_Part2Steps
    {0x654A34, 2},   // EffectKind6C_States
    {0x654A3C, 2},   // EffectKind6C_SparkStates
};
const std::uint8_t kKinds[] = {0x63, 0x65, 0x67, 0x69, 0x6C};

// Beyond effect mode's standard regions: kind 0x69's parent and kind 0x6C's
// spark cursor (two pointers).
const sh::Region kRegions[] = {
    {at::kParent, 8},
};

// --- the seed ------------------------------------------------------------------------

unsigned char* Rec(unsigned r) { return sh::EffectRecord(r); }
U SparkAt(unsigned k) { return at::kSparks + at::kSparkStride * k; }
// The spark records a cursor can reach: the sixteen and sixteen more past them
// (a cursor moved to the last by the disturbance runs on), inside the shards'
// region (0x644 bytes = 40 records).
constexpr unsigned kSparkReach = 40;

void Seed(unsigned k) {
    g_clone = k;
    unsigned char* const s = Sprite_Current;
    // every record's part step +3 below its four-entry table, its part index +4
    for (unsigned r = 0; r < at::kEffects; ++r) {
        Rec(r)[3] = static_cast<unsigned char>(sh::Next() % 4);
        Rec(r)[4] = static_cast<unsigned char>(sh::Half() ? sh::Next() % 4 : sh::Next());
    }
    // the parent (a record) and the spark cursor (one of the sixteen)
    SetLong(Mem(at::kParent), static_cast<std::int32_t>(Key(Rec(sh::Next()))));
    SetLong(Mem(at::kSparkCursor), static_cast<std::int32_t>(SparkAt(sh::Next() % at::kSparkCount)));
    // the sparks: live a third of the time not, +1 below the two states
    for (unsigned i = 0; i < kSparkReach; ++i) {
        unsigned char* const c = P(SparkAt(i));
        c[0] = static_cast<unsigned char>(sh::Next() % 3 == 0 ? 0 : (sh::Next() | 1));
        c[1] = static_cast<unsigned char>(sh::Next() % 2);
        c[2] = static_cast<unsigned char>(PickOf(1, 2, 0, 0x40, sh::Next()));
    }
    Field_MemberCount = static_cast<unsigned char>(sh::Next() % 4);
    s[9] = static_cast<unsigned char>(PickOf(1, 2, 0, 0xFF, sh::Next()));
    switch (k) {
    case k63Grow:
        SetLong(s + 0x18, static_cast<std::int32_t>(PickOf(0x18B, 0x18C, 0x18D, 0x190, 0x191, 0x7FFFFFFF, 0x80000000u, sh::Next())));
        break;
    case k63Shrink:
        SetLong(s + 0x18, static_cast<std::int32_t>(PickOf(0, 1, 9, 10, 11, 0xFFFFFFFFu, 0x8000000Au, sh::Next())));
        break;
    case k63WaitCue: Mem(at::kCounter)[0] = static_cast<unsigned char>(PickOf(0x2B, 0x2B, 0x2A, 0x2C, sh::Next())); break;
    case k65WaitRequest:
    case k65Check:
    case k65Close:
        Field_Request = static_cast<unsigned char>(PickOf(3, 2, 3, 2, 0, sh::Next()));
        SetLong(Mem(at::kKind65Count), static_cast<std::int32_t>(sh::Half() ? 5u * (sh::Next() % 0x33333333u) : sh::Next()));
        break;
    case k65WaitInput: Input_Held = static_cast<unsigned short>(sh::Half() ? 0 : PickOf(1, 0x100, 0x8000, sh::Next())); break;
    case k69Spawn:
    {
        // nine records or more free beside the spawner (it never tests for none)
        unsigned free = 0;
        for (unsigned r = 0; r < at::kEffects; ++r) {
            if (Rec(r) == s) continue;
            if (Rec(r)[0] != 0 && sh::Next() % 4 == 0) continue;
            Rec(r)[0] = 0;
            ++free;
        }
        for (unsigned r = 0; r < at::kEffects && free < 9; ++r)
            if (Rec(r) != s && Rec(r)[0] != 0) {
                Rec(r)[0] = 0;
                ++free;
            }
        break;
    }
    case k69WaitParts: s[0xB] = static_cast<unsigned char>(PickOf(9, 9, 8, 10, 0, sh::Next())); break;
    case k69P0Grow: s[0xA] = static_cast<unsigned char>(PickOf(0x10, 0x11, 0x12, 0x13, 0, 0xFF, sh::Next())); break;
    case k69P1Grow:
    case k69P2Grow: s[0xA] = static_cast<unsigned char>(PickOf(0xE, 0xF, 0x10, 0x11, 0, 0xFF, sh::Next())); break;
    case k69P0Fade:
    case k69P1Fade: s[0xA] = static_cast<unsigned char>(PickOf(1, 1, 2, 0, sh::Next())); break;
    case k69P2Fade: s[0xA] = static_cast<unsigned char>(PickOf(2, 2, 1, 3, 0, sh::Next())); break;
    case k69Part0:
    case k69Part1:
    case k69Part2:
        if (sh::Next() % 3 == 0) s[3] = 0;
        break;
    case k6CDraw:
        if (sh::Next() % 4 == 0)   // none live
            for (unsigned i = 0; i < kSparkReach; ++i) P(SparkAt(i))[0] = 0;
        break;
    default: break;
    }
}

// The three draws with arguments: the disc's radius, centre and rim; the
// column's width, base and spread (the callers' immediates half the time); a
// spark record.
void Args(unsigned k, U* a) {
    switch (k) {
    case k63Disc:
        a[0] = (sh::Next() & 0xFFFF0000u) | PickOf(0, 1, 0x190, 0x18F, 0x7FFF, 0x8000, 0xFFFF, sh::Next() & 0xFFFF);
        break;
    case k69Column:
        if (sh::Half()) {
            const bool first = sh::Half();
            a[0] = first ? 0x60 : 0x20;
            a[1] = first ? 0x20 : 0x50;
            a[2] = first ? 7 : 0x1F;
        }
        break;
    case k6CSpark: a[0] = SparkAt(sh::Next() % at::kSparkCount); break;
    default: break;
    }
}

// What the states read again after a call, moved by the group's case of the
// harness's disturbance (from its hash only): the record's counts and steps,
// the scratch cells, the vertex scratch, the parent and the spark cursor
// (always at a record), the member count, the counter.
void Disturb(U h) {
    const U v = h >> 8;
    unsigned char* const s = Sprite_Current;
    const bool here = sh::InRegions(s, 0x80);
    switch (h % 14) {
    case 0: if (here) s[9] = static_cast<unsigned char>((v & 1) ? 1u : v >> 1); break;
    case 1: if (here) s[0xA] = static_cast<unsigned char>((v & 1) ? 1u : v >> 1); break;
    case 2: if (here) s[0xB] = static_cast<unsigned char>(v); break;
    case 3: if (here) s[3] = static_cast<unsigned char>((v & 1) ? 0u : (v >> 1) % 4); break;
    case 4: SetLong(Mem(at::kScale), static_cast<std::int32_t>(v)); break;
    case 5: SetLong(Mem(at::kAngle), static_cast<std::int32_t>(v)); break;
    case 6: Mem(at::kShade)[0] = static_cast<unsigned char>(v); break;
    case 7: SetLong(Mem(at::kParent), static_cast<std::int32_t>(Key(Rec(v)))); break;
    case 8: SetLong(Mem(at::kSparkCursor), static_cast<std::int32_t>(SparkAt(v % at::kSparkCount))); break;
    case 9: Field_MemberCount = static_cast<unsigned char>(v % 4); break;
    case 10: Mem(at::kCounter)[0] = static_cast<unsigned char>((v & 1) ? at::kCounterCue : v >> 1); break;
    case 11: if (here) SetLong(s + 0x18, static_cast<std::int32_t>(v)); break;
    case 12: SetWord(Mem(at::kV0 + 2 * (v % 16)), v >> 4); break;
    case 13: if (here) s[0x5C + (v % 4)] = static_cast<unsigned char>(v >> 2); break;
    default: break;
    }
}

}  // namespace

void SelfTest() {
    // BOF3X_E3B_ONLY: the clones whose name contains it (a control's run)
    static sh::Clone chosen[kCount];
    static unsigned index[kCount];
    const char* const only = std::getenv("BOF3X_E3B_ONLY");
    unsigned n = 0;
    for (unsigned k = 0; k < kCount; ++k)
        if (!only || !*only || std::strstr(kAll[k].name, only)) {
            index[n] = k;
            chosen[n++] = kAll[k];
        }
    if (n == 0) bof3::Fatal("effect_3b: BOF3X_E3B_ONLY=%s names no clone", only);
    static unsigned* s_index = index;
    sh::Group g = {"effect_3b", chosen, n, kCallees, sizeof kCallees / sizeof kCallees[0], kTables,
                   sizeof kTables / sizeof kTables[0], kRegions, sizeof kRegions / sizeof kRegions[0],
                   [](unsigned k) { Seed(s_index[k]); }, &Disturb, 4000};
    g.args = [](unsigned k, U* a) { Args(s_index[k], a); };
    g.effect = true;
    g.kinds = kKinds;
    g.n_kinds = sizeof kKinds;
    sh::Run(g);
}

}  // namespace effect_3b

// BOF3X_SHADOW=effect_1b: group E1B's 48 functions through the scenario harness
// in effect mode (scenario_harness.h, used unchanged; docs/scenario_harness.md
// section 8), once at start-up. docs/effect_1b.md section 4. BOF3X_E1B_ONLY=
// <name> runs the one clone of that exact name (the controls' speed-up).
//
// The clone rows are tools/band_rows.py's (--group E1B --clones --harness
// scenario, 2026-09-29), each read against the disassembly to its last
// instruction (the tool's extents; the cut's larger sizes are padding). The
// shapes: kind 0xF's states and sub-kind steps, and the handlers of kinds 0x11,
// 0x12, 0x14 and 0x92, kEffect (Sprite_Current one of the 20 effect records, +5
// the kind); the panels and windows, called with arguments, kCall. The seven
// state tables the dispatchers read in place, and EffectKind14_States, are
// DataTables (their entries recorders while the fuzz runs).
//
// The records' index bytes (+3, +9, +0xA, +0xB, word +0x3C, the low byte of
// +0x14) are seeded inside their tables in EVERY record, not only the current
// one: the harness's disturbance moves Sprite_Current among the 20 after a
// call, and the originals read the new record's bytes.
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_1b.h"
#include "game/effect_1b_callees.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "hook/log.h"

namespace effect_1b {
namespace {

namespace sh = scenario_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using U = std::uint32_t;

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
constexpr U kAll = 0xFFFFFFFFu, kU8 = 0xFFu, kU16 = 0xFFFFu;
unsigned char& B(U a) { return sh::Mem(a)[0]; }
unsigned char* Rec(unsigned k) { return sh::EffectRecord(k); }
template <typename... T> U PickOf(T... v) {
    const U values[] = {static_cast<U>(v)...};
    return sh::Pick(values, sizeof...(v));
}

// --- the clone table (band_rows.py --clones, 2026-09-29) --------------------------
constexpr sh::CallSite kCalls4672F0[] = {{0xE, 0x469750}, {0x38, 0x468AC0}, {0x65, 0x468560}, {0x7A, 0x589810}};
constexpr sh::CallSite kCalls4673B0[] = {{0xD, 0x469750}, {0x37, 0x468AC0}, {0x40, 0x468560}};
constexpr sh::CallSite kCalls467400[] = {{0x34, 0x469750}, {0x50, 0x468AC0}, {0x5E, 0x469750}, {0x88, 0x468AC0}, {0xAC, 0x468560}};
constexpr sh::CallSite kCalls4674F0[] = {{0x24, 0x589840}};
constexpr sh::CallSite kCalls467540[] = {{0x1, 0x589810}, {0x36, 0x589810}, {0x6D, 0x589590}};
constexpr sh::CallSite kCalls467630[] = {{0x37, 0x5891F0}, {0x62, 0x5893A0}, {0x67, 0x5890E0}};
constexpr sh::CallSite kCalls4676A0[] = {{0x2, 0x589590}, {0x9C, 0x5891F0}};
constexpr sh::CallSite kCalls467750[] = {{0x0, 0x5893A0}, {0x5, 0x5890E0}};
constexpr sh::CallSite kCalls467760[] = {{0x2, 0x589590}, {0x75, 0x5891F0}};
constexpr sh::CallSite kCalls467810[] = {{0x2, 0x589590}};
constexpr sh::CallSite kCalls4678C0[] = {{0x5C, 0x5891F0}, {0x88, 0x5893A0}, {0x8D, 0x5890E0}, {0x96, 0x52CF60}, {0xA3, 0x52CFE0}, {0xB3, 0x52CFE0}, {0xC3, 0x52CFE0}, {0x156, 0x465120}, {0x171, 0x5B9380}, {0x187, 0x516F60}, {0x18F, 0x5890E0}};
constexpr sh::CallSite kCalls467A80[] = {{0x2, 0x589590}};
constexpr sh::CallSite kCalls467B10[] = {{0x49, 0x5891F0}, {0x7B, 0x589810}, {0xD5, 0x5893A0}, {0xDA, 0x5890E0}, {0x12F, 0x464DA0}, {0x13D, 0x464E40}, {0x146, 0x52CF60}, {0x156, 0x52CFE0}, {0x184, 0x464EC0}};
constexpr sh::CallSite kCalls467CA0[] = {{0x2, 0x589590}, {0x75, 0x5891F0}};
constexpr sh::CallSite kCalls467D30[] = {{0x19, 0x589840}, {0x22, 0x5890E0}};
constexpr sh::CallSite kCalls467D80[] = {{0x2, 0x589590}, {0x86, 0x5891F0}};
constexpr sh::CallSite kCalls467E10[] = {{0x23, 0x4652D0}, {0xC8, 0x5893A0}, {0x138, 0x516B30}, {0x19C, 0x516E70}, {0x1C4, 0x5890E0}};
constexpr sh::CallSite kCalls468040[] = {{0x2, 0x589590}};
constexpr sh::CallSite kCalls4680F0[] = {{0x3C, 0x589330}, {0x7B, 0x5893A0}, {0xE8, 0x468210}, {0xF8, 0x465D90}, {0x10C, 0x465E50}};
constexpr sh::CallSite kCalls468210[] = {{0x0, 0x5890E0}, {0x9, 0x52CF60}, {0x19, 0x52CFE0}, {0x2C, 0x52CFE0}, {0x3C, 0x52CFE0}, {0xBD, 0x465E50}, {0xE0, 0x465D90}, {0xFB, 0x465D90}};
constexpr sh::CallSite kCalls468340[] = {{0x0, 0x468040}};
constexpr sh::CallSite kCalls468350[] = {{0x62, 0x589330}, {0x82, 0x589810}, {0x107, 0x5893A0}, {0x174, 0x468210}, {0x1C6, 0x5B9380}, {0x1E8, 0x516F60}, {0x1FE, 0x516F60}};
constexpr sh::CallSite kCalls468560[] = {{0x23, 0x57CF60}, {0xC6, 0x589810}, {0x1FD, 0x516B30}, {0x235, 0x57CF60}, {0x252, 0x57CF60}, {0x289, 0x516B30}, {0x293, 0x468840}, {0x2AF, 0x5B9380}, {0x2C9, 0x517090}};
constexpr sh::CallSite kCalls468840[] = {{0x7, 0x52CF60}, {0x1A, 0x52CFE0}, {0x2E, 0x52CFE0}, {0x49, 0x52CFE0}, {0x5A, 0x52CFE0}, {0x68, 0x468950}, {0x73, 0x468950}, {0x87, 0x52CFE0}, {0x9C, 0x52CFE0}, {0xB6, 0x52CFE0}, {0xCD, 0x52CFE0}, {0xE7, 0x52CFE0}, {0xF8, 0x52CFE0}};
constexpr sh::CallSite kCalls468950[] = {{0x8, 0x5A75D0}, {0xD9, 0x461E50}};
constexpr sh::CallSite kCalls468A40[] = {{0x4, 0x52CF60}, {0x14, 0x52CFE0}, {0x27, 0x516F60}, {0x41, 0x5B9380}, {0x6A, 0x516B30}};
constexpr sh::CallSite kCalls468AC0[] = {{0x26, 0x468BB0}, {0x4F, 0x516B30}, {0x63, 0x516B30}, {0x78, 0x516B30}, {0xA1, 0x516B30}, {0xC1, 0x516B30}, {0xE0, 0x516B30}};
constexpr sh::CallSite kCalls468BB0[] = {{0x20, 0x57CF60}, {0x29, 0x52CF60}, {0x3E, 0x52CFE0}, {0x50, 0x52CFE0}, {0x66, 0x52CFE0}, {0x85, 0x52CFE0}};
constexpr sh::CallSite kCalls468C50[] = {{0x26, 0x57CF60}, {0xA5, 0x516B30}, {0xC6, 0x516B30}, {0xE8, 0x5B9380}, {0x100, 0x517090}, {0x111, 0x517090}, {0x177, 0x516B30}, {0x199, 0x5B9380}, {0x1AD, 0x517090}, {0x1C0, 0x469630}, {0x1D2, 0x468E50}, {0x1E8, 0x516B30}};
constexpr sh::CallSite kCalls468E50[] = {{0x8, 0x5A75D0}, {0x9B, 0x461E50}};
constexpr sh::CallSite kCalls468F00[] = {{0x27, 0x57CF60}, {0x128, 0x516B30}, {0x149, 0x516B30}, {0x16B, 0x5B9380}, {0x183, 0x517090}, {0x194, 0x517090}, {0x217, 0x516B30}, {0x23D, 0x5B9380}, {0x251, 0x517090}, {0x267, 0x469630}, {0x2E7, 0x468E50}, {0x2FD, 0x516B30}};
constexpr sh::CallSite kCalls469210[] = {{0xE, 0x469490}, {0x28, 0x516B30}, {0x4F, 0x594D50}, {0x75, 0x516B30}, {0x8A, 0x594D50}, {0xAA, 0x516B30}, {0xD2, 0x594D50}, {0xF8, 0x516B30}, {0x10A, 0x594D50}, {0x135, 0x516B30}, {0x14D, 0x594D50}, {0x180, 0x594D50}, {0x1A6, 0x516B30}, {0x1BA, 0x594D50}, {0x1DA, 0x516B30}, {0x1EF, 0x594D50}, {0x21A, 0x516B30}, {0x234, 0x516B30}, {0x274, 0x516B30}};
constexpr sh::CallSite kCalls469490[] = {{0x23, 0x57CF60}, {0x40, 0x57CF60}, {0x49, 0x52CF60}, {0x54, 0x52CFE0}, {0x68, 0x52CFE0}, {0x7F, 0x52CFE0}, {0x90, 0x52CFE0}, {0x9E, 0x468950}, {0xA9, 0x468950}, {0xBA, 0x52CFE0}, {0xD1, 0x52CFE0}, {0xE8, 0x52CFE0}, {0xF6, 0x52CFE0}, {0x114, 0x52CFE0}, {0x133, 0x52CFE0}, {0x13E, 0x52CFE0}, {0x14C, 0x468950}, {0x157, 0x468950}, {0x16B, 0x52CFE0}, {0x17F, 0x52CFE0}, {0x193, 0x52CFE0}};
constexpr sh::CallSite kCalls469630[] = {{0x20, 0x57CF60}, {0x29, 0x52CF60}, {0x34, 0x52CFE0}, {0x42, 0x52CFE0}, {0x53, 0x52CFE0}, {0x67, 0x52CFE0}, {0x7E, 0x52CFE0}, {0x8C, 0x52CFE0}, {0x9D, 0x52CFE0}, {0xAB, 0x468950}, {0xB9, 0x52CFE0}, {0xC7, 0x468950}, {0xD8, 0x52CFE0}, {0xF2, 0x52CFE0}, {0x10C, 0x52CFE0}};
constexpr sh::CallSite kCalls469750[] = {{0x18, 0x469790}, {0x32, 0x469960}};
constexpr sh::CallSite kCalls469790[] = {{0x40, 0x52CF60}, {0x4C, 0x5A75B0}, {0xED, 0x5A7780}, {0xF6, 0x461E50}, {0x102, 0x5A7740}, {0x148, 0x5A7780}, {0x151, 0x461E50}, {0x15D, 0x5A75B0}, {0x1B3, 0x5A7780}, {0x1BC, 0x461E50}};
constexpr sh::CallSite kCalls469960[] = {{0x4A, 0x52CF60}, {0x59, 0x5A7670}, {0xDE, 0x5A7780}, {0xE7, 0x461E50}, {0xFF, 0x52CF60}, {0x10E, 0x5A7670}, {0x14D, 0x5A7780}, {0x156, 0x461E50}};
constexpr sh::CallSite kCalls469AD0[] = {{0x8, 0x5A7760}, {0xD0, 0x461E50}};
constexpr sh::CallSite kCalls46A3E0[] = {{0x5F, 0x589840}};
constexpr sh::CallSite kCalls46A450[] = {{0x13, 0x5A79A0}, {0x2A, 0x5A77C0}, {0x33, 0x461E50}, {0x3F, 0x5A7610}, {0x8E, 0x5A7780}, {0x97, 0x461E50}};
constexpr sh::CallSite kCalls46A500[] = {{0xF, 0x5A79A0}, {0x26, 0x5A77C0}, {0x2F, 0x461E50}, {0x3B, 0x5A7610}, {0x43, 0x5A7780}, {0x94, 0x461E50}, {0xAB, 0x5A79A0}, {0xC2, 0x5A77C0}, {0xCB, 0x461E50}};
#define E1B_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define E1B_FN(name) reinterpret_cast<const void*>(&::name)
#define E1B_ROW(name, base, size, calls) #name, base, size, calls, E1B_N(calls), nullptr, 0, nullptr, 0, E1B_FN(name)
#define E1B_LEAF(name, base, size) #name, base, size, nullptr, 0, nullptr, 0, nullptr, 0, E1B_FN(name)
constexpr sh::Shape kEf = sh::Shape::kEffect, kCa = sh::Shape::kCall;
constexpr U kText1 = sh::ArgAt(1, sh::Arg::kScratch);
// {row, ret_mask, calm, shape, pointers, state_span, sub_span, kind}
const sh::Clone kAll_[] = {
    {E1B_ROW(EffectKind0F_ListOpen, 0x4672F0, 0xBD, kCalls4672F0), 0, false, kEf, 0, 0, 0, 0xF},
    {E1B_ROW(EffectKind0F_ListShow, 0x4673B0, 0x49, kCalls4673B0), 0, false, kEf, 0, 0, 0, 0xF},
    {E1B_ROW(EffectKind0F_ListClose, 0x467400, 0xE1, kCalls467400), 0, false, kEf, 0, 0, 0, 0xF},
    {E1B_ROW(EffectKind0F_Child, 0x4674F0, 0x29, kCalls4674F0), 0, false, kEf, 0, 0, at::kChildrenCount, 0xF},
    {E1B_LEAF(EffectKind0F_Child0, 0x467520, 0x12), 0, false, kEf, 0, 0, 0, 0xF},
    {E1B_ROW(EffectKind0F_Child0Start, 0x467540, 0xEA, kCalls467540), 0, false, kEf, 0, 0, 0, 0xF},
    {E1B_ROW(EffectKind0F_Child0Animate, 0x467630, 0x6C, kCalls467630), 0, false, kEf, 0, 0, 0, 0xF},
    {E1B_ROW(EffectKind0F_Child0Second, 0x4676A0, 0xAD, kCalls4676A0), 0, false, kEf, 0, 0, 0, 0xF},
    {E1B_ROW(EffectKind0F_Child0Tick, 0x467750, 0xA, kCalls467750), 0, false, kEf, 0, 0, 0, 0xF},
    {E1B_ROW(EffectKind0F_Child0Back, 0x467760, 0x86, kCalls467760), 0, false, kEf, 0, 0, 0, 0xF},
    {E1B_LEAF(EffectKind0F_Child1, 0x4677F0, 0x12), 0, false, kEf, 0, 0, 0, 0xF},
    {E1B_ROW(EffectKind0F_Child1Start, 0x467810, 0xA7, kCalls467810), 0, false, kEf, 0, 0, 0, 0xF},
    {E1B_ROW(EffectKind0F_Child1Meter, 0x4678C0, 0x194, kCalls4678C0), 0, false, kEf, 0, 0, 0, 0xF},
    {E1B_LEAF(EffectKind0F_Child2, 0x467A60, 0x12), 0, false, kEf, 0, 0, 0, 0xF},
    {E1B_ROW(EffectKind0F_Child2Start, 0x467A80, 0x88, kCalls467A80), 0, false, kEf, 0, 0, 0, 0xF},
    {E1B_ROW(EffectKind0F_Child2Gauge, 0x467B10, 0x18E, kCalls467B10), 0, false, kEf, 0, 0, 0, 0xF},
    {E1B_ROW(EffectKind0F_Child2Fade, 0x467CA0, 0x86, kCalls467CA0), 0, false, kEf, 0, 0, 0, 0xF},
    {E1B_ROW(EffectKind0F_Child2Blink, 0x467D30, 0x28, kCalls467D30), 0, false, kEf, 0, 0, 0, 0xF},
    {E1B_LEAF(EffectKind0F_Child3, 0x467D60, 0x12), 0, false, kEf, 0, 0, 0, 0xF},
    {E1B_ROW(EffectKind0F_Child3Start, 0x467D80, 0x8F, kCalls467D80), 0, false, kEf, 0, 0, 0, 0xF},
    {E1B_ROW(EffectKind0F_Child3Grid, 0x467E10, 0x20D, kCalls467E10), 0, false, kEf, 0, 0, 0, 0xF},
    {E1B_LEAF(EffectKind0F_Child4, 0x468020, 0x12), 0, false, kEf, 0, 0, 0, 0xF},
    {E1B_ROW(EffectKind0F_Child4Start, 0x468040, 0xA9, kCalls468040), 0, false, kEf, 0, 0, 0, 0xF},
    {E1B_ROW(EffectKind0F_Child4Move, 0x4680F0, 0x115, kCalls4680F0), 0, false, kEf, 0, 0, 0, 0xF},
    {E1B_ROW(EffectKind0F_DrawCursorPanel, 0x468210, 0x104, kCalls468210), 0, false, kCa, 0},
    {E1B_LEAF(EffectKind0F_Child5, 0x468320, 0x12), 0, false, kEf, 0, 0, 0, 0xF},
    {E1B_ROW(EffectKind0F_Child5Start, 0x468340, 0xF, kCalls468340), 0, false, kEf, 0, 0, 0, 0xF},
    {E1B_ROW(EffectKind0F_Child5Move, 0x468350, 0x208, kCalls468350), 0, false, kEf, 0, 0, 0, 0xF},
    {E1B_ROW(EffectKind0F_DrawMessageList, 0x468560, 0x2D6, kCalls468560), 0, false, kCa, 0},
    {E1B_ROW(EffectKind0F_DrawListFrame, 0x468840, 0x104, kCalls468840), 0, false, kCa, 0},
    {E1B_ROW(Panel_DrawEdgeQuad, 0x468950, 0xE3, kCalls468950), 0, false, kCa, 0},
    {E1B_ROW(EffectKind0F_DrawCountHeader, 0x468A40, 0x73, kCalls468A40), 0, false, kCa, 0},
    {E1B_ROW(EffectKind0F_DrawToggles, 0x468AC0, 0xEC, kCalls468AC0), 0, false, kCa, 0},
    {E1B_ROW(EffectKind0F_DrawToggle, 0x468BB0, 0x92, kCalls468BB0), 0, false, kCa, 0},
    {E1B_ROW(EffectKind0F_DrawItemsB, 0x468C50, 0x1F5, kCalls468C50), 0, false, kCa, 0},
    {E1B_ROW(EffectKind0F_DrawScrollMark, 0x468E50, 0xA5, kCalls468E50), 0, false, kCa, 0},
    {E1B_ROW(EffectKind0F_DrawItemsA, 0x468F00, 0x30D, kCalls468F00), 0, false, kCa, 0},
    {E1B_ROW(EffectKind0F_DrawEquipped, 0x469210, 0x27D, kCalls469210), 0, false, kCa, 0},
    {E1B_ROW(EffectKind0F_DrawTwinFrame, 0x469490, 0x1A0, kCalls469490), 0, false, kCa, 0},
    {E1B_ROW(EffectKind0F_DrawItemFrame, 0x469630, 0x119, kCalls469630), 0, false, kCa, 0},
    {E1B_ROW(Panel_DrawWindow, 0x469750, 0x3F, kCalls469750), 0, false, kCa, 0},
    {E1B_ROW(Panel_DrawWindowBevel, 0x469790, 0x1CC, kCalls469790), 0, false, kCa, 0},
    {E1B_ROW(Panel_DrawWindowEdges, 0x469960, 0x164, kCalls469960), 0, false, kCa, 0},
    {E1B_ROW(EffectKind0F_DrawGlyph, 0x469AD0, 0xDA, kCalls469AD0), 0, false, kCa, kText1},
    {E1B_ROW(EffectKind92_Follow, 0x46A3E0, 0x66, kCalls46A3E0), 0, false, kEf, 0, 0, 0, 0x92},
    {E1B_ROW(EffectKind11_DrawShade, 0x46A450, 0xA2, kCalls46A450), 0, false, kEf, 0, 0, 0, 0x11},
    {E1B_ROW(EffectKind12_DrawGradient, 0x46A500, 0xD6, kCalls46A500), 0, false, kEf, 0, 0, 0, 0x12},
    {E1B_LEAF(EffectKind14_Run, 0x46A5E0, 0x12), 0, false, kEf, 0, at::kKind14Count, 0, 0x14},
};
#undef E1B_ROW
#undef E1B_LEAF
enum : unsigned {
    kListOpen, kListShow, kListClose, kChild, kChild0, kChild0Start, kChild0Animate, kChild0Second, kChild0Tick,
    kChild0Back, kChild1, kChild1Start, kChild1Meter, kChild2, kChild2Start, kChild2Gauge, kChild2Fade, kChild2Blink,
    kChild3, kChild3Start, kChild3Grid, kChild4, kChild4Start, kChild4Move, kCursorPanel, kChild5, kChild5Start,
    kChild5Move, kMessageList, kListFrame, kEdgeQuad, kCountHeader, kToggles, kToggle, kItemsB, kScrollMark, kItemsA,
    kEquipped, kTwinFrame, kItemFrame, kWindow, kBevel, kEdges, kGlyph, kFollow, kShade, kGradient, kKind14, kCount
};
static_assert(kCount == sizeof kAll_ / sizeof kAll_[0], "one enum entry a clone, in order");

// The .data tables the dispatchers read in place, each to its own length.
const sh::DataTable kTables[] = {
    {at::kChildren, at::kChildrenCount},       {at::kChild0Steps, at::kChild0Count},
    {at::kChild1Steps, at::kChild1Count},      {at::kChild2Steps, at::kChild2Count},
    {at::kChild3Steps, at::kChild3Count},      {at::kChild4Steps, at::kChild4Count},
    {at::kChild5Steps, at::kChild5Count},      {at::kKind14States, at::kKind14Count},
};

// --- the stand-ins ------------------------------------------------------------------

// Effect_FindFree for the three callers that write the answer's record without
// testing it (0x4672F0, 0x467B10, 0x468560): never 0xFF (none free), which
// would send the original past the pool and ours to its abort; a free record
// when there is one, else any. The rest see the effect-mode stand-in's
// behaviour (0xFF a quarter of the time).
bool g_unchecked;
U FxFindFree(const U*, U answer) {
    const U high = answer & 0xFFFFFF00u;
    if (!g_unchecked && (answer >> 8) % 4 == 0) return high | 0xFF;
    const unsigned from = (answer >> 12) % at::kEffectCount;
    for (unsigned i = 0; i < at::kEffectCount; ++i) {
        const unsigned k = (from + i) % at::kEffectCount;
        if (Rec(k)[0] == 0) return high | k;
    }
    return high | (g_unchecked ? from : 0xFFu);
}

#define E1B_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
#define E1B_RAW(address) #address, address, address
constexpr sh::Answer kG = sh::Answer::kGarbage;
const sh::Callee kCallees[] = {
    {E1B_OURS(Effect_FindFree), 0, {}, sh::Answer::kByte, 0xFF, 0x13, {}, &FxFindFree, nullptr, true},
    // Text_DrawAt with the widths it reads (x, y words; colour, count bytes -
    // the originals push colours built in byte registers over garbage), the
    // text hashed 16 bytes where readable
    {E1B_OURS(Text_DrawAt), 5, {kU16, kU16, kU8, kU8, kAll}, kG, 0, 0, {0, 0, 0, 0, 16}, nullptr, nullptr, true},
    // the group's own, called by E8: recorders here (each is fuzzed on its
    // own), with the widths each reads (docs/effect_1b.md section 5)
    {E1B_OURS(Panel_DrawWindow), 5, {kU16, kU16, kU16, kU16, kU8}, kG, 0, 0},
    {E1B_OURS(Panel_DrawWindowBevel), 4, {kU16, kU16, kU16, kU16}, kG, 0, 0},
    {E1B_OURS(Panel_DrawWindowEdges), 5, {kU16, kU16, kU16, kU16, kU8}, kG, 0, 0},
    {E1B_OURS(Panel_DrawEdgeQuad), 4, {kU16, kU16, kU8, kU8}, kG, 0, 0},
    {E1B_OURS(EffectKind0F_DrawToggles), 3, {kU16, kU16, kU8}, kG, 0, 0},
    {E1B_OURS(EffectKind0F_DrawToggle), 3, {kU16, kU16, kU8}, kG, 0, 0},
    {E1B_OURS(EffectKind0F_DrawMessageList), 2, {kU16, kU16}, kG, 0, 0},
    {E1B_OURS(EffectKind0F_DrawListFrame), 2, {kU16, kU16}, kG, 0, 0},
    {E1B_OURS(EffectKind0F_DrawItemFrame), 2, {kU16, kU16}, kG, 0, 0},
    {E1B_OURS(EffectKind0F_DrawTwinFrame), 2, {kU16, kU16}, kG, 0, 0},
    {E1B_OURS(EffectKind0F_DrawScrollMark), 3, {kU16, kU16, kU8}, kG, 0, 0},
    {E1B_OURS(EffectKind0F_DrawCursorPanel), 1, {kU8}, kG, 0, 0},
    {E1B_OURS(EffectKind0F_Child4Start), 0, {}, kG, 0, 0},
    // E1A's, E1G's (round thirteen, raw until they merge), with the widths
    // each reads where E1B hands it a value built over garbage
    {E1B_RAW(at::kE1aBarG4), 4, {kU16, kU16, kU16, kAll}, kG, 0, 0},
    {E1B_RAW(at::kE1aGaugeA), 3, {kAll, kAll, kAll}, kG, 0, 0},
    {E1B_RAW(at::kE1aGaugeB), 3, {kAll, kAll, kAll}, kG, 0, 0},
    {E1B_RAW(at::kE1aGaugeMark), 3, {kAll, kAll, kAll}, kG, 0, 0},
    {E1B_RAW(at::kE1aDigits), 4, {kAll, kAll, kAll, kAll}, kG, 0, 0},
    {E1B_RAW(at::kE1aMarker), 4, {kU16, kU16, kU8, kAll}, kG, 0, 0},
    {E1B_RAW(at::kE1aRange), 6, {kAll, kAll, kAll, kAll, kAll, kAll}, kG, 0, 0},
    {E1B_RAW(at::kE1gItemIcon), 5, {kAll, kAll, kU8, kU8, kAll}, kG, 0, 0},
};
#undef E1B_OURS
#undef E1B_RAW

// --- the state --------------------------------------------------------------------

// Beyond effect mode's standard regions.
const sh::Region kRegions[] = {
    {at::kItemIds, 0x80},        // the accessories' ids (seeded below 52)
    {at::kItemCounts, 0x80},     // their counts
    {0x803668, 0x318},           // MessagePools past the standard 0xE8, to 0x803980 (the offsets the ids reach)
    {at::kStyleColours, 0x400},  // the colour words of styles 0..15 (the seed keeps the style byte there)
};

// A move of the cells the functions read again after a call, from the hash
// alone; each index byte kept inside the table it indexes.
void Disturb(U h) {
    const unsigned v = (h >> 8) & 0xFF;
    unsigned char* const s = Sprite_Current;
    switch ((h >> 16) % 9) {
    case 0: B(at::kLeader4) = static_cast<unsigned char>(v & 1 ? 2 : v); break;
    case 1: B(at::kItemIds + (h >> 24) % 0x80) = static_cast<unsigned char>(v % at::kAccessoryCount); break;
    case 2: B(at::kItemCounts + (h >> 24) % 0x80) = static_cast<unsigned char>(v % 4); break;
    case 3: B(at::kStyle) = static_cast<unsigned char>(v % 16); break;
    case 4: if (sh::InRegions(s, 0x80)) s[7] = static_cast<unsigned char>(s[7] ^ (1u << (v & 7))); break;
    case 5: if (sh::InRegions(s, 0x80)) SetLong(s + 0xC + 4 * (v & 1), static_cast<std::int32_t>(v % 12)); break;
    case 6: B(v & 1 ? at::kRecord6State : at::kRecord6Hold) = static_cast<unsigned char>(v & 2 ? (v & 1 ? 0xE : 0) : v); break;
    case 7: if (sh::InRegions(s, 0x80)) s[6] = static_cast<unsigned char>(v); break;
    case 8: B(at::kEquipA + 2 * (v & 1)) = static_cast<unsigned char>(v); break;
    default: break;
    }
}

// --- the seed -----------------------------------------------------------------------

// Every record's index bytes inside the ranges every read of them stays in.
void ForRecords(void (*f)(unsigned char* r, bool current)) {
    for (unsigned i = 0; i < at::kEffectCount; ++i) f(Rec(i), Rec(i) == Sprite_Current);
}

void Seed(unsigned k) {
    g_unchecked = k == kListOpen || k == kChild2Gauge || k == kMessageList;
    unsigned char* const s = Sprite_Current;
    for (unsigned i = 0; i < 0x80; ++i) {
        B(at::kItemIds + i) = static_cast<unsigned char>(sh::Next() % at::kAccessoryCount);
        if (sh::Next() % 3 == 0) B(at::kItemCounts + i) = static_cast<unsigned char>(sh::Next() % 3);
    }
    B(at::kStyle) = static_cast<unsigned char>(sh::Next() % 16);
    B(at::kLeader3) = static_cast<unsigned char>(PickOf(2, 3, 4, sh::Next()));
    B(at::kLeader4) = static_cast<unsigned char>(sh::Half() ? 2 : sh::Next() % 4);
    B(at::kRecord6Hold) = static_cast<unsigned char>(sh::Often() ? 0 : sh::Next());
    B(at::kRecord6State) = static_cast<unsigned char>(sh::Often() ? 0xE : PickOf(0xD, 0xF, sh::Next()));
    switch (k) {
    case kListOpen:
    case kListShow:
    case kListClose:
        s[9] = static_cast<unsigned char>(PickOf(0, 1, 2, 3, 4, 5, sh::Next()));
        s[0xB] = static_cast<unsigned char>(sh::Half() ? 0 : sh::Next());
        s[6] = static_cast<unsigned char>(PickOf(0, 1, 2, sh::Next() % 32, sh::Next()));
        break;
    case kChild0: s[3] = static_cast<unsigned char>(sh::Next() % at::kChild0Count); break;
    case kChild1: s[3] = static_cast<unsigned char>(sh::Next() % at::kChild1Count); break;
    case kChild2: s[3] = static_cast<unsigned char>(sh::Next() % at::kChild2Count); break;
    case kChild3: s[3] = static_cast<unsigned char>(sh::Next() % at::kChild3Count); break;
    case kChild4: s[3] = static_cast<unsigned char>(sh::Next() % at::kChild4Count); break;
    case kChild5: s[3] = static_cast<unsigned char>(sh::Next() % at::kChild5Count); break;
    case kChild0Animate:
        ForRecords([](unsigned char* r, bool) { r[0xA] = static_cast<unsigned char>(sh::Next() % 11); });
        s[9] = static_cast<unsigned char>(sh::Half() ? 0 : sh::Next());
        break;
    case kChild1Meter:
        ForRecords([](unsigned char* r, bool) { r[0xA] = static_cast<unsigned char>(1 + sh::Next() % 3); });
        s[9] = static_cast<unsigned char>(sh::Half() ? 0 : sh::Next());
        s[0xA] = static_cast<unsigned char>(s[9] == 0 ? sh::Next() % 4 : 1 + sh::Next() % 4);
        SetLong(s + 0xC, static_cast<std::int32_t>(PickOf(0, 0x60, 0x5E, 2, 0x61, sh::Next() % 0x62)));
        SetLong(s + 0x18, static_cast<std::int32_t>(PickOf(4, 0xFFFFFFFCu, 0, 0x60, sh::Next())));
        break;
    case kChild2Gauge:
        ForRecords([](unsigned char* r, bool) { r[0xA] = static_cast<unsigned char>(1 + sh::Next() % 11); });
        s[9] = static_cast<unsigned char>(sh::Half() ? 0 : sh::Next());
        s[0xA] = static_cast<unsigned char>(s[9] == 0 ? sh::Next() % 12 : 1 + sh::Next() % 12);
        s[6] = static_cast<unsigned char>(PickOf(0, 0x18, 0x17, 0x7F, 0x80, sh::Next()));
        break;
    case kChild2Blink: s[9] = static_cast<unsigned char>(PickOf(1, 2, 8, 9, 0, sh::Next())); break;
    case kChild3Grid:
        ForRecords([](unsigned char* r, bool) {
            r[0xB] = static_cast<unsigned char>(sh::Next() % 4);
            r[9] = static_cast<unsigned char>(sh::Next() % 95);
            r[0xA] = static_cast<unsigned char>(sh::Half() ? 0 : PickOf(1, 2, 0x32, sh::Next()));
        });
        break;
    case kChild4Move:
    case kChild5Move:
        ForRecords([](unsigned char* r, bool) { r[0xA] = static_cast<unsigned char>(1 + sh::Next() % 16); });
        s[9] = static_cast<unsigned char>(sh::Half() ? 0 : PickOf(1, 2, sh::Next()));
        s[0xA] = static_cast<unsigned char>(s[9] == 0 ? sh::Next() % 18 : 1 + sh::Next() % 18);
        s[0xB] = static_cast<unsigned char>(PickOf(0x3B, 0x3C, 0x3D, 0x37, sh::Next()));
        SetLong(s + 0xC, static_cast<std::int32_t>(PickOf(0x38, 0x39, 0x79, 0x7A, 0x6A, sh::Next())));
        SetLong(s + 0x10, static_cast<std::int32_t>(PickOf(0x40, 0x41, 0x94, 0x95, 0x6A, sh::Next())));
        break;
    case kCursorPanel:
        SetLong(s + 0xC, static_cast<std::int32_t>(PickOf(0x38, 0x39, 0x50, 0x79, 0x7A, sh::Next())));
        SetLong(s + 0x10, static_cast<std::int32_t>(PickOf(0x40, 0x41, 0x50, 0x70, 0x71, 0x94, 0x95, sh::Next())));
        break;
    case kMessageList:
        ForRecords([](unsigned char* r, bool) {
            SetWord(r + 0x3C, sh::Next() % at::kTitleCount);
            SetLong(r + 0x14, static_cast<std::int32_t>(sh::Next() % at::kLineCount));
        });
        s[8] = static_cast<unsigned char>(PickOf(0, 1, 0xFF, 2, 0x80));
        s[9] = static_cast<unsigned char>(PickOf(0, 0, 1, sh::Next()));
        s[0xA] = static_cast<unsigned char>(PickOf(0, 0, 1, 3, 4, sh::Next()));
        if (sh::Half()) SetWord(s + 0x3E, move_script::Word(s + 0x3C));
        SetLong(s + 0x14, static_cast<std::int32_t>(PickOf(0, 0x35, 1, sh::Next() % at::kLineCount)));
        if (sh::Half()) SetLong(s + 0x20, Long(s + 0x14) + static_cast<std::int32_t>(PickOf(0, 1, 0xFFFFFFFFu)));
        break;
    case kItemsB:
    case kItemsA:
        s[7] = static_cast<unsigned char>(sh::Next());
        s[8] = static_cast<unsigned char>(sh::Half() ? 0 : sh::Next());
        s[0xA] = static_cast<unsigned char>(PickOf(0, 0, 1, 3, sh::Next()));
        s[0x38] = static_cast<unsigned char>(PickOf(0, 1, 2, 5, sh::Next() % 12));
        SetLong(s + 0xC, static_cast<std::int32_t>(PickOf(0, 1, 3, 8, 12, 0xFFFFFFFFu, sh::Next() % 20)));
        SetLong(s + 0x10, static_cast<std::int32_t>(PickOf(0, 1, 3, 8, 9, 0xFFFFFFFFu, sh::Next() % 20)));
        SetLong(s + 0x1C, static_cast<std::int32_t>(PickOf(0, 1, sh::Next())));
        break;
    case kEquipped:
        ForRecords([](unsigned char* r, bool) { SetWord(r + 0x2C, sh::Half() ? 0xFFFFu : sh::Next() % 0x1FC); });
        Sprite_Current[7] = static_cast<unsigned char>(PickOf(0x80, 0, 1, 0x81, 2, 3, sh::Next()));
        break;
    case kCountHeader:
        if (sh::Half()) SetWord(s + 0x3E, PickOf(0xFFFF, 0, 8, 98, 99, 0x7FFF, 0xFFFE));
        break;
    case kFollow:
        s[0xB] = static_cast<unsigned char>(sh::Next() % at::kSpriteCount);
        break;
    default: break;
    }
}

// The arguments each function reads (garbage above the bytes it masks).
void Args(unsigned k, U* a) {
    switch (k) {
    case kEdgeQuad:
        a[3] = (a[3] & 0xFFFFFF00u) | (sh::Next() % at::kEdgeQuadCount);
        break;
    case kToggles:
        a[2] = (a[2] & 0xFFFFFF00u) | PickOf(0, 1, 2, 4, 7, 8, sh::Next() & 0xFF);
        break;
    case kCursorPanel:
        a[0] = sh::Half() ? (a[0] & 0xFFFFFF00u) : a[0];
        break;
    case kWindow:
    case kEdges:
        a[4] = sh::Half() ? (a[4] & 0xFFFFFF00u) : a[4];
        break;
    case kGlyph:
        sh::Scratch(1)[0] = static_cast<unsigned char>(sh::Half() ? 0x80 | sh::Next() : sh::Next());
        a[2] = PickOf(0, 0xC, 0x86, 0x8C, sh::Next());
        break;
    default: break;
    }
}

}  // namespace

void SelfTest() {
    // BOF3X_E1B_ONLY: the one clone of that exact name (a control's run)
    static sh::Clone chosen[kCount];
    static unsigned index[kCount];
    const char* const only = std::getenv("BOF3X_E1B_ONLY");
    unsigned n = 0;
    for (unsigned k = 0; k < kCount; ++k)
        if (!only || !*only || std::strcmp(kAll_[k].name, only) == 0) {
            index[n] = k;
            chosen[n++] = kAll_[k];
        }
    if (n == 0) bof3::Fatal("effect_1b: BOF3X_E1B_ONLY=%s names no clone", only);
    static const std::uint8_t kKinds[] = {0xF, 0x11, 0x12, 0x14, 0x92};
    sh::Group g = {"effect_1b", chosen, n, kCallees, sizeof kCallees / sizeof kCallees[0], kTables,
                   sizeof kTables / sizeof kTables[0], kRegions, sizeof kRegions / sizeof kRegions[0],
                   [](unsigned k) { Seed(index[k]); }, &Disturb, 6000};
    g.args = [](unsigned k, U* a) { Args(index[k], a); };
    g.effect = true;
    g.kinds = kKinds;
    g.n_kinds = sizeof kKinds;
    sh::Run(g);
}

}  // namespace effect_1b

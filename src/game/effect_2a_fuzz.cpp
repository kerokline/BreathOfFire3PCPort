// BOF3X_SHADOW=effect_2a: group E2A's 65 functions through the scenario harness
// in effect mode (scenario_harness.h, docs/scenario_harness.md section 8), once
// at start-up. docs/effect_2a.md section 4. BOF3X_E2A_ONLY=<name> runs the
// clones whose name contains it (the controls' speed-up).
//
// The clone table is tools/band_rows.py --group E2A --clones --harness scenario
// (2026-09-29), each extent read again to its last instruction (capstone) and
// the names given. Shapes: every dispatcher, state and argument-less helper
// kEffect (Sprite_Current one of the 20 Effect_Objects records, +5 the kind, a
// dispatcher's +1 below its table's length); the helpers handed arguments kCall.
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_2a.h"
#include "game/effect_2a_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace effect_2a {
namespace {

namespace sh = scenario_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using U = std::uint32_t;

// tools/band_rows.py --group E2A --clones, 2026-09-29.
constexpr sh::CallSite kCalls470320[] = {{0xD, 0x5720C0}, {0x4E, 0x57C140}};
constexpr sh::CallSite kCalls470390[] = {{0x7, 0x57C140}};
constexpr sh::CallSite kCalls4703B0[] = {{0xD, 0x470470}, {0x12, 0x4703F0}, {0x1E, 0x57C140}};
constexpr sh::CallSite kCalls4703F0[] = {{0x39, 0x531F10}, {0x63, 0x57C8A0}};
constexpr sh::CallSite kCalls470470[] = {{0x14, 0x5A79A0}, {0x2C, 0x5A77C0}, {0x35, 0x461E50}, {0x3A, 0x494060},
    {0x46, 0x5A7650}, {0x4E, 0x5A7780}, {0x5C, 0x494110}, {0x6A, 0x494110}, {0xB2, 0x461E50}, {0xBE, 0x5B9550},
    {0xD5, 0x5B9550}, {0xE6, 0x5A7A70}, {0x10A, 0x4941E0}, {0x130, 0x5B9550}, {0x138, 0x5B9550}, {0x13E, 0x470640},
    {0x152, 0x4941E0}, {0x174, 0x5B9550}, {0x17C, 0x5B9550}, {0x182, 0x470640}, {0x193, 0x5B9550}, {0x19B, 0x5B9550},
    {0x1AE, 0x5B9550}, {0x1B6, 0x5B9550}, {0x1BC, 0x4707A0}};
constexpr sh::CallSite kCalls470640[] = {{0x49, 0x5A75F0}, {0x51, 0x5A7780}, {0x65, 0x5A7A50}, {0x82, 0x5A7A00},
    {0xB3, 0x5A7A50}, {0xD0, 0x5A7A00}, {0x141, 0x461E50}};
constexpr sh::CallSite kCalls4707A0[] = {{0xC, 0x5A7610}, {0x14, 0x5A7780}, {0x69, 0x5A7A50}, {0x82, 0x5A7A00},
    {0xB7, 0x5A7A50}, {0xD4, 0x5A7A00}, {0x186, 0x461E50}, {0x1A1, 0x5A7A50}, {0x1C3, 0x5A7A00}, {0x1E4, 0x5A7A50},
    {0x206, 0x5A7A00}, {0x226, 0x461E50}};
constexpr sh::CallSite kCalls470A00[] = {{0x49, 0x470E70}};
constexpr sh::CallSite kCalls470A70[] = {{0x41, 0x470F70}, {0x47, 0x470BF0}};
constexpr sh::CallSite kCalls470AF0[] = {{0x9, 0x470BF0}, {0x15, 0x470F70}};
constexpr sh::CallSite kCalls470B40[] = {{0x21, 0x470BF0}, {0x48, 0x470F70}};
constexpr sh::CallSite kCalls470BC0[] = {{0x9, 0x470BF0}};
constexpr sh::CallSite kCalls470BF0[] = {{0x12, 0x5A79A0}, {0x2A, 0x5A77C0}, {0x33, 0x461E50}, {0x128, 0x494060},
    {0x139, 0x470D80}, {0x14A, 0x470D80}, {0x15E, 0x470D80}, {0x16F, 0x470D80}, {0x180, 0x470D80}};
constexpr sh::CallSite kCalls470D80[] = {{0x8, 0x5A7610}, {0x10, 0x5A7780}, {0x2D, 0x494110}, {0x48, 0x494110},
    {0x65, 0x494110}, {0x82, 0x494110}, {0xD8, 0x461E50}};
constexpr sh::CallSite kCalls470F70[] = {{0x14, 0x5A79A0}, {0x2C, 0x5A77C0}, {0x35, 0x461E50}, {0x3D, 0x494060},
    {0x6A, 0x5A76B0}, {0x72, 0x5A7780}, {0xBA, 0x494110}, {0xC8, 0x494110}, {0x104, 0x461E50}};
constexpr sh::CallSite kCalls4710C0[] = {{0x7, 0x57C140}};
constexpr sh::CallSite kCalls471110[] = {{0x47, 0x5720C0}, {0x78, 0x4713C0}, {0x7D, 0x4712E0}, {0x89, 0x57C140}};
constexpr sh::CallSite kCalls4711B0[] = {{0x47, 0x5720C0}, {0x78, 0x4713C0}, {0x7D, 0x4712E0}, {0xC9, 0x5720C0},
    {0xFB, 0x4713C0}, {0x100, 0x4712E0}, {0x10C, 0x57C140}};
constexpr sh::CallSite kCalls4712E0[] = {{0x46, 0x531F10}, {0xBE, 0x57C8A0}};
constexpr sh::CallSite kCalls4713C0[] = {{0x14, 0x5A79A0}, {0x2C, 0x5A77C0}, {0x35, 0x461E50}, {0x3A, 0x494060},
    {0x46, 0x5A7650}, {0x4E, 0x5A7780}, {0x5C, 0x494110}, {0x6A, 0x494110}, {0xB2, 0x461E50}, {0xBE, 0x5B9550},
    {0xD5, 0x5B9550}, {0xE6, 0x5A7A70}, {0x10A, 0x4941E0}, {0x130, 0x5B9550}, {0x138, 0x5B9550}, {0x13E, 0x471590},
    {0x152, 0x4941E0}, {0x174, 0x5B9550}, {0x17C, 0x5B9550}, {0x182, 0x471590}, {0x193, 0x5B9550}, {0x19B, 0x5B9550},
    {0x1AE, 0x5B9550}, {0x1B6, 0x5B9550}, {0x1BC, 0x4716F0}};
constexpr sh::CallSite kCalls471590[] = {{0x49, 0x5A75F0}, {0x51, 0x5A7780}, {0x65, 0x5A7A50}, {0x82, 0x5A7A00},
    {0xB3, 0x5A7A50}, {0xD0, 0x5A7A00}, {0x141, 0x461E50}};
constexpr sh::CallSite kCalls4716F0[] = {{0xC, 0x5A7610}, {0x14, 0x5A7780}, {0x69, 0x5A7A50}, {0x82, 0x5A7A00},
    {0xB7, 0x5A7A50}, {0xD4, 0x5A7A00}, {0x186, 0x461E50}, {0x1A1, 0x5A7A50}, {0x1C3, 0x5A7A00}, {0x1E4, 0x5A7A50},
    {0x206, 0x5A7A00}, {0x226, 0x461E50}};
constexpr sh::CallSite kCalls471930[] = {{0x1F, 0x471A60}};
constexpr sh::CallSite kCalls471990[] = {{0x20, 0x46E120}, {0x3E, 0x587740}};
constexpr sh::CallSite kCalls4719E0[] = {{0x9, 0x471D10}, {0xE, 0x471DD0}};
constexpr sh::CallSite kCalls471A10[] = {{0x0, 0x471DD0}};
constexpr sh::CallSite kCalls471A30[] = {{0x1F, 0x589840}};
constexpr sh::CallSite kCalls471A60[] = {{0x7, 0x494060}, {0xF, 0x5A7A50}, {0x2F, 0x5A7A00}, {0x5F, 0x494110},
    {0x83, 0x494110}, {0xDE, 0x5A7A50}, {0xFE, 0x5A7A00}, {0x12E, 0x494110}, {0x152, 0x494110}, {0x167, 0x5A79A0},
    {0x180, 0x5A77C0}, {0x193, 0x572FA0}, {0x19F, 0x5A7610}, {0x1A7, 0x5A7780}, {0x249, 0x572FA0}, {0x25E, 0x5A79A0},
    {0x276, 0x5A77C0}, {0x289, 0x572FA0}};
constexpr sh::CallSite kCalls471D10[] = {{0x2D, 0x5B93D2}, {0x45, 0x5B93D2}, {0x5C, 0x5A7A50}, {0x74, 0x5A7A00},
    {0xA0, 0x5B93D2}};
constexpr sh::CallSite kCalls471DD0[] = {{0x27, 0x471E20}};
constexpr sh::CallSite kCalls471E20[] = {{0x13, 0x494110}, {0x1F, 0x5A7750}, {0x27, 0x5A7780}, {0x44, 0x5B93D2},
    {0x68, 0x572FA0}};
constexpr sh::CallSite kCalls471EC0[] = {{0x5A, 0x5720C0}, {0x74, 0x4723E0}};
constexpr sh::CallSite kCalls471F60[] = {{0x22, 0x472400}, {0x84, 0x5A8B60}, {0xD3, 0x4721A0}};
constexpr sh::CallSite kCalls472080[] = {{0x0, 0x4723E0}};
constexpr sh::CallSite kCalls472090[] = {{0x9, 0x472400}, {0x49, 0x4721A0}};
constexpr sh::CallSite kCalls4720F0[] = {{0x14, 0x472400}, {0x4F, 0x5A7A00}, {0x5B, 0x5A7A50}, {0x8E, 0x4721A0}};
constexpr sh::CallSite kCalls4721A0[] = {{0x4, 0x494060}, {0x21, 0x472240}, {0x4C, 0x5720C0}, {0x68, 0x5A8B60}};
constexpr sh::CallSite kCalls472240[] = {{0x15, 0x5A79A0}, {0x2D, 0x5A77C0}, {0x41, 0x572FA0}, {0x4D, 0x5A75D0},
    {0x55, 0x5A7780}, {0x94, 0x4941E0}, {0xA3, 0x494110}, {0x12D, 0x5A79E0}, {0x144, 0x5A79A0}, {0x18A, 0x572FA0}};
constexpr sh::CallSite kCalls472440[] = {{0x20, 0x494060}, {0x48, 0x494110}, {0x74, 0x494110}, {0xB7, 0x4976D0},
    {0xDA, 0x587740}};
constexpr sh::CallSite kCalls472530[] = {{0x23, 0x472790}};
constexpr sh::CallSite kCalls472580[] = {{0x29, 0x587740}};
constexpr sh::CallSite kCalls4725B0[] = {{0x24, 0x472AB0}, {0x46, 0x472D70}};
constexpr sh::CallSite kCalls472620[] = {{0x31, 0x587740}, {0x3A, 0x472AB0}, {0x3F, 0x472D90}, {0x45, 0x472F00},
    {0x56, 0x472EE0}, {0x60, 0x473100}};
constexpr sh::CallSite kCalls4726B0[] = {{0xA, 0x472F00}, {0x5B, 0x587740}};
constexpr sh::CallSite kCalls472720[] = {{0xB, 0x472F00}, {0x28, 0x472F00}};
constexpr sh::CallSite kCalls472770[] = {{0xE, 0x589840}};
constexpr sh::CallSite kCalls472790[] = {{0x17, 0x5A79A0}, {0x2F, 0x5A77C0}, {0x46, 0x572FA0}, {0x6E, 0x5A76B0},
    {0x76, 0x5A7780}, {0x9B, 0x5A7A50}, {0xBB, 0x5A7A00}, {0x10D, 0x572FA0}, {0x12A, 0x472920}, {0x142, 0x5A79A0},
    {0x15B, 0x5A77C0}, {0x171, 0x572FA0}};
constexpr sh::CallSite kCalls472920[] = {{0x14, 0x5A79A0}, {0x2C, 0x5A77C0}, {0x43, 0x572FA0}, {0x93, 0x5A7A90},
    {0xA1, 0x5A7750}, {0xA9, 0x5A7780}, {0xEF, 0x572FA0}, {0x11D, 0x572FA0}, {0x150, 0x5A79A0}, {0x169, 0x5A77C0},
    {0x17F, 0x572FA0}};
constexpr sh::CallSite kCalls472AB0[] = {{0x17, 0x5A79A0}, {0x2F, 0x5A77C0}, {0x46, 0x572FA0}, {0xD2, 0x5A7610},
    {0xDA, 0x5A7780}, {0xE0, 0x5A7A50}, {0xFB, 0x5A7A00}, {0x117, 0x5A7A50}, {0x134, 0x5A7A00}, {0x170, 0x5A7A50},
    {0x18B, 0x5A7A00}, {0x1AD, 0x5A7A50}, {0x1C8, 0x5A7A00}, {0x233, 0x572FA0}, {0x275, 0x5A79A0}, {0x28E, 0x5A77C0},
    {0x2A4, 0x572FA0}};
constexpr sh::CallSite kCalls472D90[] = {{0x19, 0x472E00}, {0x54, 0x473100}};
constexpr sh::CallSite kCalls472E00[] = {{0x8, 0x5A75B0}, {0x10, 0x5A7780}, {0xD5, 0x572FA0}};
constexpr sh::CallSite kCalls472F00[] = {{0x17, 0x5A79A0}, {0x2E, 0x5A77C0}, {0x45, 0x572FA0}, {0x65, 0x5A7610},
    {0x6D, 0x5A7780}, {0x73, 0x5A7A50}, {0x9A, 0x5A7A00}, {0x102, 0x5A7A50}, {0x129, 0x5A7A00}, {0x18D, 0x572FA0},
    {0x1B7, 0x5A79A0}, {0x1D0, 0x5A77C0}, {0x1E6, 0x572FA0}};
constexpr sh::CallSite kCalls473100[] = {{0xD, 0x5B93D2}, {0x1A, 0x5B93D2}, {0x2E, 0x5A7A50}, {0x4A, 0x5A7A00},
    {0x6F, 0x5B9550}};

#define E2A_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define E2A_CALLS(a) a, E2A_N(a)
#define E2A_FN(name) reinterpret_cast<const void*>(&::name)
constexpr sh::Shape kEf = sh::Shape::kEffect;
constexpr sh::Shape kCa = sh::Shape::kCall;
// {name, base, size, calls, imms, tables, ours, ret_mask, calm, shape, pointers, state_span, sub_span, kind}
const sh::Clone kAll[] = {
    {"EffectKind28_Run", 0x470300, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E2A_FN(EffectKind28_Run), 0, false, kEf, 0, 4, 0, 0x28},
    {"EffectKind28_Start", 0x470320, 0x6F, E2A_CALLS(kCalls470320), nullptr, 0, nullptr, 0, E2A_FN(EffectKind28_Start), 0, false, kEf, 0, 0, 0, 0x28},
    {"EffectKind28_WaitFlag", 0x470390, 0x1D, E2A_CALLS(kCalls470390), nullptr, 0, nullptr, 0, E2A_FN(EffectKind28_WaitFlag), 0, false, kEf, 0, 0, 0, 0x28},
    {"EffectKind28_Beam", 0x4703B0, 0x34, E2A_CALLS(kCalls4703B0), nullptr, 0, nullptr, 0, E2A_FN(EffectKind28_Beam), 0, false, kEf, 0, 0, 0, 0x28},
    {"EffectKind28_PushParty", 0x4703F0, 0x73, E2A_CALLS(kCalls4703F0), nullptr, 0, nullptr, 0, E2A_FN(EffectKind28_PushParty), 0, false, kEf, 0, 0, 0, 0x28},
    {"EffectKind28_DrawBeam", 0x470470, 0x1CC, E2A_CALLS(kCalls470470), nullptr, 0, nullptr, 0, E2A_FN(EffectKind28_DrawBeam), 0, false, kCa, 0, 0, 0, 0x28},
    {"EffectKind28_DrawEnd", 0x470640, 0x160, E2A_CALLS(kCalls470640), nullptr, 0, nullptr, 0, E2A_FN(EffectKind28_DrawEnd), 0, false, kCa, 0, 0, 0, 0x28},
    {"EffectKind28_DrawSides", 0x4707A0, 0x234, E2A_CALLS(kCalls4707A0), nullptr, 0, nullptr, 0, E2A_FN(EffectKind28_DrawSides), 0, false, kCa, 0, 0, 0, 0x28},
    {"EffectKind29_Run", 0x4709E0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E2A_FN(EffectKind29_Run), 0, false, kEf, 0, 6, 0, 0x29},
    {"EffectKind29_Start", 0x470A00, 0x61, E2A_CALLS(kCalls470A00), nullptr, 0, nullptr, 0, E2A_FN(EffectKind29_Start), 0, false, kEf, 0, 0, 0, 0x29},
    {"EffectKind29_Extend", 0x470A70, 0x76, E2A_CALLS(kCalls470A70), nullptr, 0, nullptr, 0, E2A_FN(EffectKind29_Extend), 0, false, kEf, 0, 0, 0, 0x29},
    {"EffectKind29_Hold", 0x470AF0, 0x43, E2A_CALLS(kCalls470AF0), nullptr, 0, nullptr, 0, E2A_FN(EffectKind29_Hold), 0, false, kEf, 0, 0, 0, 0x29},
    {"EffectKind29_Fade", 0x470B40, 0x76, E2A_CALLS(kCalls470B40), nullptr, 0, nullptr, 0, E2A_FN(EffectKind29_Fade), 0, false, kEf, 0, 0, 0, 0x29},
    {"EffectKind29_Linger", 0x470BC0, 0x2E, E2A_CALLS(kCalls470BC0), nullptr, 0, nullptr, 0, E2A_FN(EffectKind29_Linger), 0, false, kEf, 0, 0, 0, 0x29},
    {"EffectKind29_DrawBoxes", 0x470BF0, 0x18A, E2A_CALLS(kCalls470BF0), nullptr, 0, nullptr, 0, E2A_FN(EffectKind29_DrawBoxes), 0, false, kCa, 0, 0, 0, 0x29},
    {"EffectKind29_DrawFace", 0x470D80, 0xE2, E2A_CALLS(kCalls470D80), nullptr, 0, nullptr, 0, E2A_FN(EffectKind29_DrawFace), 0, false, kCa, 0, 0, 0, 0x29},
    {"EffectKind29_SetSegments", 0x470E70, 0xFA, nullptr, 0, nullptr, 0, nullptr, 0, E2A_FN(EffectKind29_SetSegments), 0, false, kEf, 0, 0, 0, 0x29},
    {"EffectKind29_DrawSegments", 0x470F70, 0x126, E2A_CALLS(kCalls470F70), nullptr, 0, nullptr, 0, E2A_FN(EffectKind29_DrawSegments), 0, false, kCa, 0, 0, 0, 0x29},
    {"EffectKind2A_Run", 0x4710A0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E2A_FN(EffectKind2A_Run), 0, false, kEf, 0, 4, 0, 0x2A},
    {"EffectKind2A_Start", 0x4710C0, 0x41, E2A_CALLS(kCalls4710C0), nullptr, 0, nullptr, 0, E2A_FN(EffectKind2A_Start), 0, false, kEf, 0, 0, 0, 0x2A},
    {"EffectKind2A_Beam", 0x471110, 0xA0, E2A_CALLS(kCalls471110), nullptr, 0, nullptr, 0, E2A_FN(EffectKind2A_Beam), 0, false, kEf, 0, 0, 0, 0x2A},
    {"EffectKind2A_Beams", 0x4711B0, 0x122, E2A_CALLS(kCalls4711B0), nullptr, 0, nullptr, 0, E2A_FN(EffectKind2A_Beams), 0, false, kEf, 0, 0, 0, 0x2A},
    {"EffectKind2A_PushParty", 0x4712E0, 0xD8, E2A_CALLS(kCalls4712E0), nullptr, 0, nullptr, 0, E2A_FN(EffectKind2A_PushParty), 0, false, kEf, 0, 0, 0, 0x2A},
    {"EffectKind2A_DrawBeam", 0x4713C0, 0x1CC, E2A_CALLS(kCalls4713C0), nullptr, 0, nullptr, 0, E2A_FN(EffectKind2A_DrawBeam), 0, false, kCa, 0, 0, 0, 0x2A},
    {"EffectKind2A_DrawEnd", 0x471590, 0x160, E2A_CALLS(kCalls471590), nullptr, 0, nullptr, 0, E2A_FN(EffectKind2A_DrawEnd), 0, false, kCa, 0, 0, 0, 0x2A},
    {"EffectKind2A_DrawSides", 0x4716F0, 0x234, E2A_CALLS(kCalls4716F0), nullptr, 0, nullptr, 0, E2A_FN(EffectKind2A_DrawSides), 0, false, kCa, 0, 0, 0, 0x2A},
    {"EffectKind2B_Run", 0x471930, 0x25, E2A_CALLS(kCalls471930), nullptr, 0, nullptr, 0, E2A_FN(EffectKind2B_Run), 0, false, kEf, 0, 5, 0, 0x2B},
    {"EffectKind2B_Start", 0x471960, 0x2C, nullptr, 0, nullptr, 0, nullptr, 0, E2A_FN(EffectKind2B_Start), 0, false, kEf, 0, 0, 0, 0x2B},
    {"EffectKind2B_Rise", 0x471990, 0x45, E2A_CALLS(kCalls471990), nullptr, 0, nullptr, 0, E2A_FN(EffectKind2B_Rise), 0, false, kEf, 0, 0, 0, 0x2B},
    {"EffectKind2B_Specks", 0x4719E0, 0x25, E2A_CALLS(kCalls4719E0), nullptr, 0, nullptr, 0, E2A_FN(EffectKind2B_Specks), 0, false, kEf, 0, 0, 0, 0x2B},
    {"EffectKind2B_Settle", 0x471A10, 0x1D, E2A_CALLS(kCalls471A10), nullptr, 0, nullptr, 0, E2A_FN(EffectKind2B_Settle), 0, false, kEf, 0, 0, 0, 0x2B},
    {"EffectKind2B_Shrink", 0x471A30, 0x25, E2A_CALLS(kCalls471A30), nullptr, 0, nullptr, 0, E2A_FN(EffectKind2B_Shrink), 0, false, kEf, 0, 0, 0, 0x2B},
    {"EffectKind2B_Draw", 0x471A60, 0x2AB, E2A_CALLS(kCalls471A60), nullptr, 0, nullptr, 0, E2A_FN(EffectKind2B_Draw), 0, false, kEf, 0, 0, 0, 0x2B},
    {"EffectSpecks_Spawn", 0x471D10, 0xB7, E2A_CALLS(kCalls471D10), nullptr, 0, nullptr, 0, E2A_FN(EffectSpecks_Spawn), 0, false, kEf, 0, 0, 0, 0x2B},
    {"EffectSpecks_Move", 0x471DD0, 0x4B, E2A_CALLS(kCalls471DD0), nullptr, 0, nullptr, 0, E2A_FN(EffectSpecks_Move), 0xFF, false, kEf, 0, 0, 0, 0x2B},
    {"EffectSpecks_Draw", 0x471E20, 0x77, E2A_CALLS(kCalls471E20), nullptr, 0, nullptr, 0, E2A_FN(EffectSpecks_Draw), 0, false, kCa, 0, 0, 0, 0x2B},
    {"EffectKind2C_Run", 0x471EA0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E2A_FN(EffectKind2C_Run), 0, false, kEf, 0, 3, 0, 0x2C},
    {"EffectKind2C_Start", 0x471EC0, 0x92, E2A_CALLS(kCalls471EC0), nullptr, 0, nullptr, 0, E2A_FN(EffectKind2C_Start), 0, false, kEf, 0, 0, 0, 0x2C},
    {"EffectKind2C_Emit", 0x471F60, 0xF5, E2A_CALLS(kCalls471F60), nullptr, 0, nullptr, 0, E2A_FN(EffectKind2C_Emit), 0, false, kEf, 0, 0, 0, 0x2C},
    {"EffectKind2E_Run", 0x472060, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E2A_FN(EffectKind2E_Run), 0, false, kEf, 0, 3, 0, 0x2E},
    {"EffectKind2E_Start", 0x472080, 0xE, E2A_CALLS(kCalls472080), nullptr, 0, nullptr, 0, E2A_FN(EffectKind2E_Start), 0, false, kEf, 0, 0, 0, 0x2E},
    {"EffectKind2E_Trickle", 0x472090, 0x60, E2A_CALLS(kCalls472090), nullptr, 0, nullptr, 0, E2A_FN(EffectKind2E_Trickle), 0, false, kEf, 0, 0, 0, 0x2E},
    {"EffectKind2E_Burst", 0x4720F0, 0xA1, E2A_CALLS(kCalls4720F0), nullptr, 0, nullptr, 0, E2A_FN(EffectKind2E_Burst), 0, false, kEf, 0, 0, 0, 0x2E},
    {"EffectSparks_Move", 0x4721A0, 0xA0, E2A_CALLS(kCalls4721A0), nullptr, 0, nullptr, 0, E2A_FN(EffectSparks_Move), 0, false, kEf, 0, 0, 0, 0x2C},
    {"EffectSparks_Draw", 0x472240, 0x198, E2A_CALLS(kCalls472240), nullptr, 0, nullptr, 0, E2A_FN(EffectSparks_Draw), 0, false, kCa, 0, 0, 0, 0x2C},
    {"EffectSparks_Clear", 0x4723E0, 0x14, nullptr, 0, nullptr, 0, nullptr, 0, E2A_FN(EffectSparks_Clear), 0, false, kEf, 0, 0, 0, 0x2C},
    {"EffectSparks_FindFree", 0x472400, 0x1B, nullptr, 0, nullptr, 0, nullptr, 0, E2A_FN(EffectSparks_FindFree), 0xFFFFFFFFu, false, kEf, 0, 0, 0, 0x2C},
    {"EffectKind2D_Run", 0x472420, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E2A_FN(EffectKind2D_Run), 0, false, kEf, 0, 8, 0, 0x2D},
    {"EffectKind2D_Start", 0x472440, 0xE7, E2A_CALLS(kCalls472440), nullptr, 0, nullptr, 0, E2A_FN(EffectKind2D_Start), 0, false, kEf, 0, 0, 0, 0x2D},
    {"EffectKind2D_Spin", 0x472530, 0x48, E2A_CALLS(kCalls472530), nullptr, 0, nullptr, 0, E2A_FN(EffectKind2D_Spin), 0, false, kEf, 0, 0, 0, 0x2D},
    {"EffectKind2D_Pause", 0x472580, 0x30, E2A_CALLS(kCalls472580), nullptr, 0, nullptr, 0, E2A_FN(EffectKind2D_Pause), 0, false, kEf, 0, 0, 0, 0x2D},
    {"EffectKind2D_Open", 0x4725B0, 0x65, E2A_CALLS(kCalls4725B0), nullptr, 0, nullptr, 0, E2A_FN(EffectKind2D_Open), 0, false, kEf, 0, 0, 0, 0x2D},
    {"EffectKind2D_Pour", 0x472620, 0x84, E2A_CALLS(kCalls472620), nullptr, 0, nullptr, 0, E2A_FN(EffectKind2D_Pour), 0, false, kEf, 0, 0, 0, 0x2D},
    {"EffectKind2D_Close", 0x4726B0, 0x65, E2A_CALLS(kCalls4726B0), nullptr, 0, nullptr, 0, E2A_FN(EffectKind2D_Close), 0, false, kEf, 0, 0, 0, 0x2D},
    {"EffectKind2D_Lift", 0x472720, 0x4F, E2A_CALLS(kCalls472720), nullptr, 0, nullptr, 0, E2A_FN(EffectKind2D_Lift), 0, false, kEf, 0, 0, 0, 0x2D},
    {"EffectKind2D_End", 0x472770, 0x13, E2A_CALLS(kCalls472770), nullptr, 0, nullptr, 0, E2A_FN(EffectKind2D_End), 0, false, kEf, 0, 0, 0, 0x2D},
    {"EffectKind2D_DrawRays", 0x472790, 0x181, E2A_CALLS(kCalls472790), nullptr, 0, nullptr, 0, E2A_FN(EffectKind2D_DrawRays), 0, false, kCa, 0, 0, 0, 0x2D},
    {"EffectKind2D_DrawDisc", 0x472920, 0x18C, E2A_CALLS(kCalls472920), nullptr, 0, nullptr, 0, E2A_FN(EffectKind2D_DrawDisc), 0, false, kCa, 0, 0, 0, 0x2D},
    {"EffectKind2D_DrawRings", 0x472AB0, 0x2B4, E2A_CALLS(kCalls472AB0), nullptr, 0, nullptr, 0, E2A_FN(EffectKind2D_DrawRings), 0, false, kCa, 0, 0, 0, 0x2D},
    {"EffectDrops_Clear", 0x472D70, 0x16, nullptr, 0, nullptr, 0, nullptr, 0, E2A_FN(EffectDrops_Clear), 0, false, kEf, 0, 0, 0, 0x2D},
    {"EffectDrops_Move", 0x472D90, 0x67, E2A_CALLS(kCalls472D90), nullptr, 0, nullptr, 0, E2A_FN(EffectDrops_Move), 0, false, kEf, 0, 0, 0, 0x2D},
    {"EffectDrops_Draw", 0x472E00, 0xDF, E2A_CALLS(kCalls472E00), nullptr, 0, nullptr, 0, E2A_FN(EffectDrops_Draw), 0, false, kCa, 0, 0, 0, 0x2D},
    {"EffectDrops_FindFree", 0x472EE0, 0x1B, nullptr, 0, nullptr, 0, nullptr, 0, E2A_FN(EffectDrops_FindFree), 0xFFFFFFFFu, false, kEf, 0, 0, 0, 0x2D},
    {"EffectKind2D_DrawCurtain", 0x472F00, 0x1F6, E2A_CALLS(kCalls472F00), nullptr, 0, nullptr, 0, E2A_FN(EffectKind2D_DrawCurtain), 0, false, kCa, 0, 0, 0, 0x2D},
    {"EffectDrops_Launch", 0x473100, 0x92, E2A_CALLS(kCalls473100), nullptr, 0, nullptr, 0, E2A_FN(EffectDrops_Launch), 0, false, kCa, 0, 0, 0, 0x2D},
};
#undef E2A_FN
#undef E2A_CALLS
#undef E2A_N

enum : unsigned {
    k28Run, k28Start, k28Wait, k28Beam, k28Push, k28DrawBeam, k28DrawEnd, k28DrawSides, k29Run, k29Start, k29Extend,
    k29Hold, k29Fade, k29Linger, k29DrawBoxes, k29DrawFace, k29SetSegments, k29DrawSegments, k2ARun, k2AStart,
    k2ABeam, k2ABeams, k2APush, k2ADrawBeam, k2ADrawEnd, k2ADrawSides, k2BRun, k2BStart, k2BRise, k2BSpecks,
    k2BSettle, k2BShrink, k2BDraw, kSpecksSpawn, kSpecksMove, kSpecksDraw, k2CRun, k2CStart, k2CEmit, k2ERun,
    k2EStart, k2ETrickle, k2EBurst, kSparksMove, kSparksDraw, kSparksClear, kSparksFree, k2DRun, k2DStart, k2DSpin,
    k2DPause, k2DOpen, k2DPour, k2DClose, k2DLift, k2DEnd, k2DRays, k2DDisc, k2DRings, kDropsClear, kDropsMove,
    kDropsDraw, kDropsFree, k2DCurtain, kDropsLaunch,
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
unsigned char* Pool(U stride, unsigned i) { return P(at::kPool + stride * (i % at::kPoolCount)); }

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
// infinity) - what the projections' screen words and depths are; one time in
// eight a value _ftol answers with the integer indefinite (NaN, or past 2^63
// either way), or a NaN of another sign or payload - quiet or signalling -
// which an x87 sum of two NaNs picks by its own rule.
void FillFloat(U at) {
    if (!Writable(at, 4)) return;
    const U n = sh::Noise();
    U bits;
    if (n % 8 == 0) {
        static const U kOdd[] = {0x7FC00000u, 0xFFC00000u, 0x7FC00001u, 0x7F800001u, 0xFF800002u, 0x5F000000u, 0xDF000001u, 0x7F7FFFFFu};
        bits = kOdd[(n >> 3) % 8];
    } else {
        bits = (n & 0x807FFFFFu) | ((0x6Eu + (sh::Noise() % 0x24u)) << 23);
    }
    std::memcpy(P(at), &bits, 4);
}
// EffectGte_ProjectPoint: out[0..2] the screen x, y and depth (floats), which
// every caller reads (into the packet, or a stack vector it copies from).
U FxProjectPoint(const U* a, U answer) {
    for (unsigned i = 0; i < 3; ++i) FillFloat(a[1] + 4 * i);
    return answer;
}
// EffectGte_ProjectSize: out's two s16 (the beam reads both as one dword, the
// sparks the first).
U FxProjectSize(const U* a, U answer) {
    if (Writable(a[2], 4)) sh::FillBytes(P(a[2]), 4);
    return answer;
}
// EffectSparks_FindFree / EffectDrops_FindFree: a free record of the pool (as
// the real one decides free) looked for from a start the answer picks - not
// always the first, so a caller that finds its own shows - or 0 a quarter of
// the time and when none is free.
U FindIn(U stride, bool (*is_free)(const unsigned char*), U answer) {
    if ((answer >> 8) % 4 == 0) return 0;
    const unsigned from = (answer >> 12) % at::kPoolCount;
    for (unsigned i = 0; i < at::kPoolCount; ++i) {
        unsigned char* const r = Pool(stride, from + i);
        if (is_free(r)) return Key(r);
    }
    return 0;
}
U FxSparkFree(const U*, U answer) {
    return FindIn(at::kSparkStride, [](const unsigned char* r) { return r[0x25] == 0; }, answer);
}
U FxDropFree(const U*, U answer) {
    return FindIn(at::kDropStride, [](const unsigned char* r) { return Word(r + 0x14) == 0; }, answer);
}
// AreaMap_Elevation: half the time a low word of 0, 1, -1 or 2 (the sparks'
// heights are seeded there, so "above the ground" meets its boundary), else
// the answer; the upper half the answer's (the callers movsx the word).
U FxGround(const U*, U answer) {
    const U n = sh::Noise();
    if (n % 2) return answer;
    static const U kLow[] = {0, 1, 0xFFFF, 2};
    return (answer & 0xFFFF0000u) | kLow[(n >> 1) % 4];
}

#define E2A_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
constexpr sh::Answer kG = sh::Answer::kGarbage, kF = sh::Answer::kFlag, kPh = sh::Answer::kPhase;
constexpr U kW = 0xFFFFFFFFu, k16 = 0xFFFFu, k8 = 0xFFu;
const sh::Callee kCallees[] = {
    // the group's own called directly, by name, with the masks the callee reads
    // (docs/effect_2a.md section 3): the beam's two points (three dwords each);
    // the ends' and sides' low words; the faces' index and shade bytes; the
    // segments' t and shade bytes; the records handed by pointer hashed whole
    {E2A_OURS(EffectKind28_DrawBeam), 2, {kW, kW}, kG, 0, 0, {12, 12}, nullptr, nullptr, true},
    {E2A_OURS(EffectKind28_DrawEnd), 4, {k16, k16, k16, k16}, kG, 0, 0},
    {E2A_OURS(EffectKind28_DrawSides), 8, {k16, k16, k16, k16, k16, k16, k16, k16}, kG, 0, 0},
    {E2A_OURS(EffectKind28_PushParty), 0, {}, kPh, 0, 0},
    {E2A_OURS(EffectKind2A_DrawBeam), 2, {kW, kW}, kG, 0, 0, {12, 12}, nullptr, nullptr, true},
    {E2A_OURS(EffectKind2A_DrawEnd), 4, {k16, k16, k16, k16}, kG, 0, 0},
    {E2A_OURS(EffectKind2A_DrawSides), 8, {k16, k16, k16, k16, k16, k16, k16, k16}, kG, 0, 0},
    {E2A_OURS(EffectKind2A_PushParty), 0, {}, kPh, 0, 0},
    {E2A_OURS(EffectKind29_SetSegments), 0, {}, kPh, 0, 0},
    {E2A_OURS(EffectKind29_DrawSegments), 2, {k8, k8}, kG, 0, 0},
    {E2A_OURS(EffectKind29_DrawBoxes), 1, {kW}, kG, 0, 0, {0x30}, nullptr, nullptr, true},
    {E2A_OURS(EffectKind29_DrawFace), 6, {k8, k8, k8, k8, k8, k8}, kG, 0, 0},
    {E2A_OURS(EffectKind2B_Draw), 0, {}, kPh, 0, 0},
    {E2A_OURS(EffectSpecks_Spawn), 0, {}, kPh, 0, 0},
    {E2A_OURS(EffectSpecks_Move), 0, {}, kF, 0, 0},
    {E2A_OURS(EffectSpecks_Draw), 1, {kW}, kG, 0, 0, {0x10}, nullptr, nullptr, true},
    {E2A_OURS(EffectSparks_Move), 0, {}, kPh, 0, 0},
    {E2A_OURS(EffectSparks_Draw), 1, {kW}, kG, 0, 0, {0x28}, nullptr, nullptr, true},
    {E2A_OURS(EffectSparks_Clear), 0, {}, kPh, 0, 0},
    {E2A_OURS(EffectSparks_FindFree), 0, {}, kG, 0, 0, {}, &FxSparkFree},
    {E2A_OURS(EffectKind2D_DrawRays), 1, {kW}, kG, 0, 0, {0x12}, nullptr, nullptr, true},
    {E2A_OURS(EffectKind2D_DrawDisc), 1, {kW}, kG, 0, 0, {0x12}, nullptr, nullptr, true},
    {E2A_OURS(EffectKind2D_DrawRings), 1, {kW}, kG, 0, 0, {0x12}, nullptr, nullptr, true},
    {E2A_OURS(EffectKind2D_DrawCurtain), 1, {kW}, kG, 0, 0, {0x12}, nullptr, nullptr, true},
    {E2A_OURS(EffectDrops_Clear), 0, {}, kPh, 0, 0},
    {E2A_OURS(EffectDrops_Move), 0, {}, kPh, 0, 0},
    {E2A_OURS(EffectDrops_Draw), 1, {kW}, kG, 0, 0, {0x18}, nullptr, nullptr, true},
    {E2A_OURS(EffectDrops_FindFree), 0, {}, kG, 0, 0, {}, &FxDropFree},
    {E2A_OURS(EffectDrops_Launch), 1, {kW}, kG, 0, 0, {0x18}, nullptr, nullptr, true},
    // E1C's, already ours
    {E2A_OURS(EffectShards_Clear), 0, {}, kPh, 0, 0},
    // the standard row, answering at the boundaries the sparks are seeded at
    {E2A_OURS(AreaMap_Elevation), 2, {kW, kW}, kG, 0, 0, {}, &FxGround, nullptr, true},
    // standard entries re-listed: the point hashed (a stack vector's address
    // differs between the copy and ours), out not logged and filled; the size
    // hashed to its first word only - EffectSparks_Draw's second is stack the
    // original never wrote (docs/effect_2a.md section 3)
    {E2A_OURS(EffectGte_ProjectPoint), 2, {0, 0}, kG, 0, 0, {12, 0}, &FxProjectPoint, nullptr, true},
    {E2A_OURS(EffectGte_ProjectSize), 3, {0, 0, 0}, kG, 0, 0, {12, 2, 0}, &FxProjectSize, nullptr, true},
};
#undef E2A_OURS

// The seven state tables the dispatchers go through, read in place: each its
// own length (to the next table a dispatcher names; none bounded).
const sh::DataTable kTables[] = {
    {0x6542F8, 4},   // EffectKind28_States
    {0x654310, 6},   // EffectKind29_States
    {0x654334, 4},   // EffectKind2A_States
    {0x65434C, 5},   // EffectKind2B_States
    {0x654360, 3},   // EffectKind2C_States
    {0x6543AC, 3},   // EffectKind2E_States
    {0x6543B8, 8},   // EffectKind2D_States
};

// Beyond effect mode's standard regions: kind 0x29's segments and corners and
// kind 0x2D's link cell (0x675FE0..0x676108), and the pool past the standard
// 0x644 bytes of EffectKind30_Shards to the spark centre and points (0x92D410).
const sh::Region kRegions[] = {
    {at::kSegments, at::kLinkZ + 4 - at::kSegments},
    {at::kPool + 0x644, at::kPoolEnd - (at::kPool + 0x644)},
};

const std::uint8_t kKinds[] = {0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E};

// --- the seed ------------------------------------------------------------------------

// The frame count +9 at every compare the states make: 0 and 1 (down to 0xFF,
// 0), 8 (the fade's full shade), 0xE / 0xF (the pour's sound), 0x19 / 0x1A (the
// spin's turn), 0x80, 0xFF.
U FrameCount() { return PickOf(0, 1, 2, 7, 8, 9, 0xE, 0xF, 0x19, 0x1A, 0x80, 0xFF, sh::Next()); }

// The pool: some specks / sparks / drops in use, not all (the moves walk all
// 0x80 and call per record in use); a quarter of the time no speck (the move's
// answer 0); some specks landing exactly on Sprite_Current's ground after the
// move, some sparks at the heights the ground stand-in answers.
void SeedPool() {
    // the three families share the pool (a drop's +0x14 is a speck's +0 every
    // fifth drop): the specks last, so "no speck in use" holds
    for (unsigned i = 0; i < at::kPoolCount; ++i) {
        const bool live = sh::Next() % 4 == 0;
        unsigned char* const r = Pool(at::kSparkStride, i);
        r[0x25] = static_cast<unsigned char>(live ? PickOf(1, 2, 0x80, sh::Next() | 1) : 0);
        if (live && sh::Half()) {
            SetLong(r + 8, static_cast<std::int32_t>(PickOf(0, 1, 0xFFFFFFFFu, 2)));
            SetLong(r + 0x18, static_cast<std::int32_t>(PickOf(0, 1, 0xFFFFFFFFu)));
        }
    }
    for (unsigned i = 0; i < at::kPoolCount; ++i) {
        const bool live = sh::Next() % 4 == 0;
        unsigned char* const r = Pool(at::kDropStride, i);
        SetWord(r + 0x14, live ? PickOf(1, 2, 0x20, 0x8000, sh::Next() | 1) : 0);
        if (live && sh::Half()) {
            // a whole y and bounds at, above and below its top and bottom
            const std::int32_t y = static_cast<std::int16_t>(sh::Next() % 0x200 - 0x100);
            const std::int32_t h = static_cast<std::int32_t>(sh::Next() % 0x40);
            const float fy = static_cast<float>(y);
            std::memcpy(r + 4, &fy, 4);
            SetWord(r + 0xC, static_cast<U>(h));
            SetWord(r + 0x10, static_cast<U>(y - h + static_cast<std::int32_t>(PickOf(0, 0, 1, 0xFFFFFFFFu))));
            SetWord(r + 0x12, static_cast<U>(y + h + static_cast<std::int32_t>(PickOf(0, 0, 1, 0xFFFFFFFFu))));
        }
    }
    const bool none = sh::Next() % 4 == 0;
    for (unsigned i = 0; i < at::kPoolCount; ++i) {
        const bool live = !none && sh::Next() % 4 == 0;
        unsigned char* const r = Pool(at::kSpeckStride, i);
        r[0] = static_cast<unsigned char>(live ? PickOf(1, 0x80, sh::Next() | 1) : 0);
        if (live && sh::Next() % 4 == 0)
            SetLong(r + 0xC, Long(Sprite_Current + 0x3C) + static_cast<std::int32_t>(static_cast<U>(static_cast<std::int16_t>(Word(r + 2))) << 8) +
                                 static_cast<std::int32_t>(PickOf(0, 0, 1, 0xFFFFFFFFu)));
    }
}

// Every record's radius word +0x2E not 0 (EffectSpecks_Spawn divides by the word
// of the record current after Rand, which the disturbance may move); the
// current one 0 now and then (the path that does not divide).
void SeedRadius() {
    for (unsigned r = 0; r < at::kEffects; ++r) {
        unsigned char* const e = sh::EffectRecord(r);
        SetWord(e + 0x2E, PickOf(1, 0xFFFF, 0x100, 0x7FFF, 0x8000, sh::Next() | 1));
    }
    if (sh::Next() % 4 == 0) SetWord(Sprite_Current + 0x2E, 0);
}

// Kind 0x2D's screen point floats: ordinary values mostly (the drops' _ftol,
// the curtain's sums), a NaN or an out-of-range one now and then.
void SeedScreen(unsigned char* q) {
    for (unsigned i = 0; i < 3; ++i) {
        const U n = sh::Next();
        U bits;
        if (n % 16 == 0) bits = PickOf(0x7FC00000u, 0x7F800001u, 0x5F000000u, 0xDF000001u);
        else bits = (n & 0x807FFFFFu) | ((0x6Eu + (sh::Next() % 0x24u)) << 23);
        SetLong(q + 4 * i, static_cast<std::int32_t>(bits));
    }
}

// Kind 0x2D's radius word +0x18 (q + 0xC): small, as the states keep it (the
// disc draws 2 (r >> 2) + 1 rows), its compares and signs.
U SmallRadius() { return PickOf(0, 1, 2, 3, 4, 7, 8, 0x10, 0x3F, 0x40, 0x41, 0x80, 0x100, 0xFFF8, 0xFFFC, 0xFFFF, sh::Next() % 0x104); }

// The party: members at rows (their z +0x38 around the pushes' rows) and their
// +1 at 1 half the time.
void SeedParty() {
    for (unsigned m = 0; m < 3; ++m) {
        unsigned char* const o = sh::ObjectOf(m);
        o[1] = static_cast<unsigned char>(sh::Half() ? 1 : sh::Next());
        SetLong(o + 0x38, static_cast<std::int32_t>(PickOf(0xD0000, 0xD7FFF, 0xD8000, 0x110000, 0x118000, 0x150000, 0x158001,
                                                           0x100000, sh::Next())));
    }
    if (sh::Next() % 3) Field_Request = 0;
    if (sh::Next() % 3) Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 & ~0x1400u);
}

void Seed(unsigned k) {
    unsigned char* const s = Sprite_Current;
    s[9] = static_cast<unsigned char>(FrameCount());
    SeedPool();
    SeedRadius();
    if (sh::Next() % 3 == 0)
        sh::Mem(at::kCounter0)[0] = static_cast<unsigned char>(PickOf(3, 0x29, 0x28, 2, 4, 0x2A));
    // the colour index +6 inside its three entries mostly (past them the
    // original reads the bytes after the table: image data)
    if (sh::Often()) s[6] = static_cast<unsigned char>(sh::Next() % 3);
    SeedParty();
    switch (k) {
    case k2BRise:
    case k2BShrink:
        SetWord(s + 0x32, PickOf(0, 1, 2, 0x12C, sh::Next()));
        break;
    case k2CEmit:
        SetLong(s + 0xC, static_cast<std::int32_t>(PickOf(1, 2, 0x10, 0x11, 0x20, 0x200, 0x1F0, sh::Next())));
        break;
    case k2DSpin:
    case k2DPause:
    case k2DOpen:
    case k2DPour:
    case k2DClose:
    case k2DLift:
    case k2DStart:
        SeedScreen(s + 0xC);
        SetWord(s + 0x18, SmallRadius());
        SetWord(s + 0x1C, PickOf(0, 0xFE, 0xFF, 0x100, 0x101, 0x7FFF, 0xFFFF, sh::Next()));
        break;
    case k2DDisc:
    case k2DRays:
    case k2DRings:
    case k2DCurtain:
        SeedScreen(s + 0xC);
        SetWord(s + 0x18, SmallRadius());
        SetWord(s + 0x1C, PickOf(0, 0x80, 0xFB, 0xFC, 0xFF, 0x100, 0xFFFC, 0xFFFF, 0x8000, sh::Next()));
        break;
    case kDropsLaunch:
    case kDropsMove:
    case kDropsDraw:
        SeedScreen(s + 0xC);
        break;
    default: break;
    }
}

// The arguments of the kCall helpers: the records as their callers hand them.
void Args(unsigned k, U* a) {
    const U s = Key(Sprite_Current);
    switch (k) {
    case k28DrawBeam:
    case k2ADrawBeam:
        a[0] = s + 0xC;
        a[1] = s + 0x18;
        break;
    case k29DrawBoxes:
        a[0] = s + 0xC;
        break;
    case k29DrawFace:
        // the corners' index bytes below eight, with leftovers above them
        for (unsigned i = 0; i < 4; ++i) a[i] = (a[i] & 0xFFFFFF00u) | (sh::Next() % 8);
        break;
    case k2DRays:
    case k2DDisc:
    case k2DRings:
    case k2DCurtain:
        a[0] = s + 0xC;
        break;
    case kSpecksDraw:
        a[0] = Key(Pool(at::kSpeckStride, sh::Next()));
        break;
    case kSparksDraw:
        a[0] = Key(Pool(at::kSparkStride, sh::Next()));
        break;
    case kDropsDraw:
    case kDropsLaunch:
        a[0] = Key(Pool(at::kDropStride, sh::Next()));
        break;
    default: break;
    }
}

// What the states read again after a call, moved by the group's case of the
// harness's disturbance (from its hash only): the frame count, kind 0x2B's rise
// and count words (its radius never to 0: the speck spawn divides by it), kind
// 0x2D's words, the dword +0xC.
void Disturb(U h) {
    const U v = h >> 8;
    unsigned char* const s = Sprite_Current;
    switch (h % 7) {
    case 0: {
        static const unsigned char kAt[] = {1, 0xE, 0x19, 8};
        s[9] = (v & 1) ? kAt[(v >> 1) % 4] : static_cast<unsigned char>(v >> 1);
        break;
    }
    case 1: SetWord(s + 0x2E, (v >> 1) | 1); break;
    case 2: SetWord(s + 0x32, (v & 1) ? 1u : v >> 1); break;
    case 3: SetWord(s + 0x30, v); break;
    case 4: SetWord(s + 0x1A, v); break;
    case 5: SetWord(s + 0x1C, (v & 1) ? 0xFFu : v >> 1); break;
    case 6: SetLong(s + 0xC, static_cast<std::int32_t>((v & 1) ? 1u : v >> 1)); break;
    default: break;
    }
}

}  // namespace

void SelfTest() {
    // BOF3X_E2A_ONLY: the clones whose name contains it (a control's run)
    static sh::Clone chosen[kCount];
    static unsigned index[kCount];
    const char* const only = std::getenv("BOF3X_E2A_ONLY");
    unsigned n = 0;
    for (unsigned k = 0; k < kCount; ++k)
        if (!only || !*only || std::strstr(kAll[k].name, only)) {
            index[n] = k;
            chosen[n++] = kAll[k];
        }
    if (n == 0) bof3::Fatal("effect_2a: BOF3X_E2A_ONLY=%s names no clone", only);
    static unsigned* s_index = index;
    sh::Group g = {"effect_2a", chosen, n, kCallees, sizeof kCallees / sizeof kCallees[0], kTables,
                   sizeof kTables / sizeof kTables[0], kRegions, sizeof kRegions / sizeof kRegions[0],
                   [](unsigned k) { Seed(s_index[k]); }, &Disturb, 6000};
    g.args = [](unsigned k, U* a) { Args(s_index[k], a); };
    g.effect = true;
    g.kinds = kKinds;
    g.n_kinds = sizeof kKinds;
    sh::Run(g);
}

}  // namespace effect_2a

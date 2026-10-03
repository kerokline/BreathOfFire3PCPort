// BOF3X_SHADOW=effect_3d: group E3D's 68 functions through the scenario harness
// in effect mode (scenario_harness.h, docs/scenario_harness.md section 8), once
// at start-up. docs/effect_3d.md section 4. BOF3X_E3D_ONLY=<name> runs the
// clones whose name contains it (the controls' speed-up); BOF3X_E3D_ROUNDS
// sets the rounds (default 4,000).
//
// The clone table is tools/band_rows.py --group E3D --clones --harness scenario
// (2026-10-03), each extent read again to its last instruction (capstone) and
// the names given, with five starts added: kind 0x82's state 2 0x487DE0 (code
// no list has, inside the cut's span of 0x487C50) and kind 0x77's sub-state
// dispatcher 0x486280 with its three sub-states 0x4861C0, 0x4861E0, 0x4862A0
// (catalog rows no group of the round holds). Shapes: every dispatcher and
// state kEffect (Sprite_Current one of the 20 Effect_Objects records, +5 the
// kind, a dispatcher's byte below its table's length); the helpers with
// arguments kCall.
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_3d.h"
#include "game/effect_3d_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace effect_3d {
namespace {

namespace sh = scenario_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using U = std::uint32_t;

// tools/band_rows.py --group E3D --clones, 2026-10-03 (the four kind-0x77 starts read by hand).
constexpr sh::CallSite kCalls485CB0[] = {{0x13, 0x5A79A0}, {0x2A, 0x5A77C0}, {0x33, 0x461E50}, {0x3F, 0x5A7610}, {0x8E, 0x5A7780}, {0x97, 0x461E50}};
constexpr sh::CallSite kCalls485D60[] = {{0x13, 0x5A79A0}, {0x2A, 0x5A77C0}, {0x33, 0x461E50}, {0x3F, 0x5A7610}, {0x8E, 0x5A7780}, {0x97, 0x461E50}, {0xAA, 0x589840}};
constexpr sh::CallSite kCalls485E10[] = {{0x12, 0x5A79A0}, {0x29, 0x5A77C0}, {0x32, 0x461E50}, {0x3E, 0x5A7610}, {0x8D, 0x5A7780}, {0x96, 0x461E50}};
constexpr sh::CallSite kCalls485F10[] = {{0x5C, 0x5A79A0}, {0x74, 0x5A77C0}, {0x7D, 0x461E50}, {0x89, 0x5A7610}, {0xEE, 0x5A7780}, {0xF7, 0x461E50}};
constexpr sh::CallSite kCalls486070[] = {{0x5C, 0x5A79A0}, {0x74, 0x5A77C0}, {0x7D, 0x461E50}, {0x89, 0x5A7610}, {0xEE, 0x5A7780}, {0xF7, 0x461E50}, {0x10A, 0x589840}};
constexpr sh::CallSite kCalls4861C0[] = {{0x0, 0x5B93D2}};
constexpr sh::CallSite kCalls4861E0[] = {{0xA, 0x589840}, {0x6C, 0x57C140}, {0x7D, 0x587740}};
constexpr sh::CallSite kCalls4862A0[] = {{0x1B, 0x57C140}, {0x3C, 0x589840}};
constexpr sh::CallSite kCalls4862F0[] = {{0x0, 0x486310}, {0x18, 0x589840}};
constexpr sh::CallSite kCalls486310[] = {{0xD, 0x586160}, {0x27, 0x5B9380}, {0x3A, 0x516F60}};
constexpr sh::CallSite kCalls4863E0[] = {{0x3E, 0x4864C0}};
constexpr sh::CallSite kCalls486450[] = {{0x22, 0x4864C0}};
constexpr sh::CallSite kCalls4864A0[] = {{0x10, 0x589840}};
constexpr sh::CallSite kCalls4864C0[] = {{0xF, 0x5A7A50}, {0x22, 0x5A7A00}, {0x87, 0x5A7570}, {0xB7, 0x5A7A50}, {0xCA, 0x5A7A00}, {0x14B, 0x572FA0}};
constexpr sh::CallSite kCalls486690[] = {{0x66, 0x587740}, {0x81, 0x587740}, {0xBC, 0x587740}, {0xEA, 0x4867A0}};
constexpr sh::CallSite kCalls486790[] = {{0x0, 0x486D60}};
constexpr sh::CallSite kCalls4867A0[] = {{0x6, 0x486C90}, {0x32, 0x486AB0}, {0x50, 0x486AB0}, {0x7D, 0x486AB0}, {0x96, 0x486990},
                                         {0xAF, 0x486990}, {0xC8, 0x486990}, {0x11A, 0x486B70}, {0x155, 0x486B70}, {0x189, 0x486C10},
                                         {0x1A6, 0x486C10}, {0x1C3, 0x486C10}, {0x1DD, 0x486C10}};
constexpr sh::CallSite kCalls486990[] = {{0x6, 0x486C90}, {0x29, 0x486AB0}, {0x30, 0x486C90}, {0x5E, 0x486C10}, {0x92, 0x486C10}, {0xDD, 0x486CE0}};
constexpr sh::CallSite kCalls486AB0[] = {{0x8, 0x5A75B0}, {0x18, 0x5A7780}, {0xB6, 0x461E50}};
constexpr sh::CallSite kCalls486B70[] = {{0x8, 0x5A7570}, {0x18, 0x5A7780}, {0x96, 0x461E50}};
constexpr sh::CallSite kCalls486C10[] = {{0x8, 0x5A7650}, {0x18, 0x5A7780}, {0x76, 0x461E50}};
constexpr sh::CallSite kCalls486C90[] = {{0x18, 0x5A79A0}, {0x31, 0x5A77C0}, {0x3A, 0x461E50}};
constexpr sh::CallSite kCalls486CE0[] = {{0x2A, 0x486C10}, {0x47, 0x486C10}, {0x76, 0x486C10}};
constexpr sh::CallSite kCalls486D60[] = {{0xE, 0x57C140}, {0xB6, 0x579F00}, {0x147, 0x572ED0}, {0x165, 0x572A00}};
constexpr sh::CallSite kCalls486F30[] = {{0x2E, 0x587740}};
constexpr sh::CallSite kCalls486F70[] = {{0x13, 0x486FC0}};
constexpr sh::CallSite kCalls486FC0[] = {{0x17, 0x5A79A0}, {0x2F, 0x5A77C0}, {0x38, 0x461E50}, {0x3D, 0x494060}, {0x54, 0x5A7A50},
                                         {0x69, 0x5A7A00}, {0x87, 0x494110}, {0x95, 0x5A7A50}, {0xAA, 0x5A7A00}, {0xC8, 0x494110},
                                         {0x10A, 0x5A79A0}, {0x123, 0x5A77C0}, {0x12E, 0x572FA0}, {0x13A, 0x5A7610}, {0x142, 0x5A7780},
                                         {0x188, 0x5A7A50}, {0x1A7, 0x5A7A00}, {0x1CF, 0x494110}, {0x1D8, 0x5A7A50}, {0x1F4, 0x5A7A00},
                                         {0x219, 0x494110}, {0x282, 0x572FA0}};
constexpr sh::CallSite kCalls487290[] = {{0x3A, 0x4873E0}};
constexpr sh::CallSite kCalls4872F0[] = {{0x19, 0x587740}, {0x26, 0x4873E0}, {0x30, 0x4875C0}};
constexpr sh::CallSite kCalls487360[] = {{0x5, 0x4873E0}, {0xF, 0x4875C0}};
constexpr sh::CallSite kCalls4873A0[] = {{0x5, 0x4873E0}, {0xF, 0x4875C0}};
constexpr sh::CallSite kCalls4873E0[] = {{0x4B, 0x494060}, {0x5C, 0x494110}, {0x7D, 0x4941E0}, {0xAA, 0x5B9550},
                                         {0xBD, 0x5B9550}, {0xF6, 0x5A7A70}, {0x197, 0x479970}};
constexpr sh::CallSite kCalls4875C0[] = {{0x14, 0x5A79A0}, {0x2C, 0x5A77C0}, {0x35, 0x461E50}, {0x5C, 0x47D4F0}, {0x94, 0x5A7610},
                                         {0x9C, 0x5A7780}, {0xB9, 0x5A7A50}, {0xE5, 0x5A7A00}, {0x126, 0x5A7A50}, {0x151, 0x5A7A00},
                                         {0x1AB, 0x461E50}, {0x1CF, 0x5A7A50}, {0x1FB, 0x5A7A00}, {0x227, 0x5A7A50}, {0x253, 0x5A7A00},
                                         {0x276, 0x461E50}, {0x2C0, 0x47D4F0}};
constexpr sh::CallSite kCalls4878B0[] = {{0x2B, 0x487920}, {0x30, 0x487AB0}};
constexpr sh::CallSite kCalls4878F0[] = {{0x11, 0x487B20}, {0x16, 0x487940}};
constexpr sh::CallSite kCalls487910[] = {{0x0, 0x487940}, {0x9, 0x589840}};
constexpr sh::CallSite kCalls487940[] = {{0xF, 0x494060}, {0x51, 0x4879C0}};
constexpr sh::CallSite kCalls4879C0[] = {{0x1A, 0x5A7750}, {0x27, 0x494110}, {0x43, 0x572FA0}, {0x58, 0x5A79A0}, {0x70, 0x5A77C0},
                                         {0x80, 0x572FA0}, {0x8C, 0x5A7740}, {0x94, 0x5A7780}, {0x9E, 0x494110}, {0xDA, 0x572FA0}};
constexpr sh::CallSite kCalls487AB0[] = {{0x1A, 0x5B93D2}, {0x25, 0x5B93D2}, {0x44, 0x5720C0}};
constexpr sh::CallSite kCalls487B20[] = {{0x39, 0x487BF0}, {0x47, 0x5B93D2}, {0x53, 0x5B93D2}, {0x6D, 0x5B93D2}, {0x90, 0x5B93D2}, {0x9D, 0x5B93D2}};
constexpr sh::CallSite kCalls487C50[] = {{0x5C, 0x589840}};
constexpr sh::JumpTable kTables487C50[] = {{0x58, 0x70, 5}};

#define E3D_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define E3D_CALLS(a) a, E3D_N(a)
#define E3D_FN(name) reinterpret_cast<const void*>(&::name)
constexpr sh::Shape kEf = sh::Shape::kEffect;
constexpr sh::Shape kCa = sh::Shape::kCall;
// {name, base, size, calls, imms, tables, ours, ret_mask, calm, shape, pointers, state_span, sub_span, kind}
const sh::Clone kAll[] = {
    {"EffectKind76_Shade", 0x485CB0, 0xA2, E3D_CALLS(kCalls485CB0), nullptr, 0, nullptr, 0, E3D_FN(EffectKind76_Shade), 0, false, kEf, 0, 0, 0, 0x76},
    {"EffectKind79_Shade", 0x485D60, 0xB0, E3D_CALLS(kCalls485D60), nullptr, 0, nullptr, 0, E3D_FN(EffectKind79_Shade), 0, false, kEf, 0, 0, 0, 0x79},
    {"EffectKind7A_Shade", 0x485E10, 0xA1, E3D_CALLS(kCalls485E10), nullptr, 0, nullptr, 0, E3D_FN(EffectKind7A_Shade), 0, false, kEf, 0, 0, 0, 0x7A},
    {"EffectKind7B_Run", 0x485EC0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E3D_FN(EffectKind7B_Run), 0, false, kEf, 0, 2, 0, 0x7B},
    {"EffectKind7B_Start", 0x485EE0, 0x22, nullptr, 0, nullptr, 0, nullptr, 0, E3D_FN(EffectKind7B_Start), 0, false, kEf, 0, 0, 0, 0x7B},
    {"EffectKind7B_Pulse", 0x485F10, 0x102, E3D_CALLS(kCalls485F10), nullptr, 0, nullptr, 0, E3D_FN(EffectKind7B_Pulse), 0, false, kEf, 0, 0, 0, 0x7B},
    {"EffectKind7C_Run", 0x486020, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E3D_FN(EffectKind7C_Run), 0, false, kEf, 0, 2, 0, 0x7C},
    {"EffectKind7C_Start", 0x486040, 0x22, nullptr, 0, nullptr, 0, nullptr, 0, E3D_FN(EffectKind7C_Start), 0, false, kEf, 0, 0, 0, 0x7C},
    {"EffectKind7C_Pulse", 0x486070, 0x110, E3D_CALLS(kCalls486070), nullptr, 0, nullptr, 0, E3D_FN(EffectKind7C_Pulse), 0, false, kEf, 0, 0, 0, 0x7C},
    {"EffectKind77_Run", 0x486180, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E3D_FN(EffectKind77_Run), 0, false, kEf, 0, 3, 0, 0x77},
    {"EffectKind77_Count", 0x4861A0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E3D_FN(EffectKind77_Count), 0, false, kEf, 0, 0, 3, 0x77},
    {"EffectKind77_Begin", 0x4861C0, 0x17, E3D_CALLS(kCalls4861C0), nullptr, 0, nullptr, 0, E3D_FN(EffectKind77_Begin), 0, false, kEf, 0, 0, 0, 0x77},
    {"EffectKind77_Tick", 0x4861E0, 0xA0, E3D_CALLS(kCalls4861E0), nullptr, 0, nullptr, 0, E3D_FN(EffectKind77_Tick), 0, false, kEf, 0, 0, 0, 0x77},
    {"EffectKind77_Resume", 0x486280, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E3D_FN(EffectKind77_Resume), 0, false, kEf, 0, 0, 2, 0x77},
    {"EffectKind77_End", 0x4862A0, 0x42, E3D_CALLS(kCalls4862A0), nullptr, 0, nullptr, 0, E3D_FN(EffectKind77_End), 0, false, kEf, 0, 0, 0, 0x77},
    {"EffectKind77_Show", 0x4862F0, 0x1E, E3D_CALLS(kCalls4862F0), nullptr, 0, nullptr, 0, E3D_FN(EffectKind77_Show), 0, false, kEf, 0, 0, 0, 0x77},
    {"EffectKind77_DrawCount", 0x486310, 0x43, E3D_CALLS(kCalls486310), nullptr, 0, nullptr, 0, E3D_FN(EffectKind77_DrawCount), 0, false, kEf, 0, 0, 0, 0x77},
    {"EffectKind78_Run", 0x486360, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E3D_FN(EffectKind78_Run), 0, false, kEf, 0, 4, 0, 0x78},
    {"EffectKind78_Start", 0x486380, 0x57, nullptr, 0, nullptr, 0, nullptr, 0, E3D_FN(EffectKind78_Start), 0, false, kEf, 0, 0, 0, 0x78},
    {"EffectKind78_Grow", 0x4863E0, 0x6C, E3D_CALLS(kCalls4863E0), nullptr, 0, nullptr, 0, E3D_FN(EffectKind78_Grow), 0, false, kEf, 0, 0, 0, 0x78},
    {"EffectKind78_Spin", 0x486450, 0x47, E3D_CALLS(kCalls486450), nullptr, 0, nullptr, 0, E3D_FN(EffectKind78_Spin), 0, false, kEf, 0, 0, 0, 0x78},
    {"EffectKind78_End", 0x4864A0, 0x15, E3D_CALLS(kCalls4864A0), nullptr, 0, nullptr, 0, E3D_FN(EffectKind78_End), 0, false, kEf, 0, 0, 0, 0x78},
    {"EffectKind78_DrawRing", 0x4864C0, 0x173, E3D_CALLS(kCalls4864C0), nullptr, 0, nullptr, 0, E3D_FN(EffectKind78_DrawRing), 0, false, kCa, 0, 0, 0, 0x78},
    {"EffectKind7D_Run", 0x486640, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E3D_FN(EffectKind7D_Run), 0, false, kEf, 0, 4, 0, 0x7D},
    {"EffectKind7D_Start", 0x486660, 0x2C, nullptr, 0, nullptr, 0, nullptr, 0, E3D_FN(EffectKind7D_Start), 0, false, kEf, 0, 0, 0, 0x7D},
    {"EffectKind7D_Input", 0x486690, 0xF3, E3D_CALLS(kCalls486690), nullptr, 0, nullptr, 0, E3D_FN(EffectKind7D_Input), 0, false, kEf, 0, 0, 0, 0x7D},
    {"EffectKind7D_Apply", 0x486790, 0xE, E3D_CALLS(kCalls486790), nullptr, 0, nullptr, 0, E3D_FN(EffectKind7D_Apply), 0, false, kEf, 0, 0, 0, 0x7D},
    {"EffectKind7D_DrawPanel", 0x4867A0, 0x1EA, E3D_CALLS(kCalls4867A0), nullptr, 0, nullptr, 0, E3D_FN(EffectKind7D_DrawPanel), 0, false, kCa, 0, 0, 0, 0x7D},
    {"EffectKind7D_DrawDial", 0x486990, 0x115, E3D_CALLS(kCalls486990), nullptr, 0, nullptr, 0, E3D_FN(EffectKind7D_DrawDial), 0, false, kCa, 0, 0, 0, 0x7D},
    {"EffectKind7D_FillF4", 0x486AB0, 0xC0, E3D_CALLS(kCalls486AB0), nullptr, 0, nullptr, 0, E3D_FN(EffectKind7D_FillF4), 0, false, kCa, 0, 0, 0, 0x7D},
    {"EffectKind7D_FillF3", 0x486B70, 0xA0, E3D_CALLS(kCalls486B70), nullptr, 0, nullptr, 0, E3D_FN(EffectKind7D_FillF3), 0, false, kCa, 0, 0, 0, 0x7D},
    {"EffectKind7D_Line", 0x486C10, 0x80, E3D_CALLS(kCalls486C10), nullptr, 0, nullptr, 0, E3D_FN(EffectKind7D_Line), 0, false, kCa, 0, 0, 0, 0x7D},
    {"EffectKind7D_DrawMode", 0x486C90, 0x43, E3D_CALLS(kCalls486C90), nullptr, 0, nullptr, 0, E3D_FN(EffectKind7D_DrawMode), 0, false, kCa, 0, 0, 0, 0x7D},
    {"EffectKind7D_DrawMark", 0x486CE0, 0x7F, E3D_CALLS(kCalls486CE0), nullptr, 0, nullptr, 0, E3D_FN(EffectKind7D_DrawMark), 0, false, kCa, 0, 0, 0, 0x7D},
    {"EffectKind7D_SetMap", 0x486D60, 0x1A6, E3D_CALLS(kCalls486D60), nullptr, 0, nullptr, 0, E3D_FN(EffectKind7D_SetMap), 0, false, kEf, 0, 0, 0, 0x7D},
    {"EffectKind7F_Run", 0x486F10, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E3D_FN(EffectKind7F_Run), 0, false, kEf, 0, 3, 0, 0x7F},
    {"EffectKind7F_Start", 0x486F30, 0x35, E3D_CALLS(kCalls486F30), nullptr, 0, nullptr, 0, E3D_FN(EffectKind7F_Start), 0, false, kEf, 0, 0, 0, 0x7F},
    {"EffectKind7F_Grow", 0x486F70, 0x4F, E3D_CALLS(kCalls486F70), nullptr, 0, nullptr, 0, E3D_FN(EffectKind7F_Grow), 0, false, kEf, 0, 0, 0, 0x7F},
    {"EffectKind7F_DrawCone", 0x486FC0, 0x2A1, E3D_CALLS(kCalls486FC0), nullptr, 0, nullptr, 0, E3D_FN(EffectKind7F_DrawCone), 0, false, kCa, 0, 0, 0, 0x7F},
    {"EffectKind80_Run", 0x487270, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E3D_FN(EffectKind80_Run), 0, false, kEf, 0, 9, 0, 0x80},
    {"EffectKind80_Start", 0x487290, 0x58, E3D_CALLS(kCalls487290), nullptr, 0, nullptr, 0, E3D_FN(EffectKind80_Start), 0, false, kEf, 0, 0, 0, 0x80},
    {"EffectKind80_Rise", 0x4872F0, 0x64, E3D_CALLS(kCalls4872F0), nullptr, 0, nullptr, 0, E3D_FN(EffectKind80_Rise), 0, false, kEf, 0, 0, 0, 0x80},
    {"EffectKind80_Hold", 0x487360, 0x3D, E3D_CALLS(kCalls487360), nullptr, 0, nullptr, 0, E3D_FN(EffectKind80_Hold), 0, false, kEf, 0, 0, 0, 0x80},
    {"EffectKind80_Fade", 0x4873A0, 0x3C, E3D_CALLS(kCalls4873A0), nullptr, 0, nullptr, 0, E3D_FN(EffectKind80_Fade), 0, false, kEf, 0, 0, 0, 0x80},
    {"EffectKind80_TrailStep", 0x4873E0, 0x1D1, E3D_CALLS(kCalls4873E0), nullptr, 0, nullptr, 0, E3D_FN(EffectKind80_TrailStep), 0, false, kCa, 0, 0, 0, 0x80},
    {"EffectKind80_DrawTrail", 0x4875C0, 0x2D0, E3D_CALLS(kCalls4875C0), nullptr, 0, nullptr, 0, E3D_FN(EffectKind80_DrawTrail), 0, false, kCa, 0, 0, 0, 0x80},
    {"EffectKind81_Run", 0x487890, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E3D_FN(EffectKind81_Run), 0, false, kEf, 0, 3, 0, 0x81},
    {"EffectKind81_Start", 0x4878B0, 0x3E, E3D_CALLS(kCalls4878B0), nullptr, 0, nullptr, 0, E3D_FN(EffectKind81_Start), 0, false, kEf, 0, 0, 0, 0x81},
    {"EffectKind81_Pour", 0x4878F0, 0x1B, E3D_CALLS(kCalls4878F0), nullptr, 0, nullptr, 0, E3D_FN(EffectKind81_Pour), 0, false, kEf, 0, 0, 0, 0x81},
    {"EffectKind81_Drain", 0x487910, 0xF, E3D_CALLS(kCalls487910), nullptr, 0, nullptr, 0, E3D_FN(EffectKind81_Drain), 0, false, kEf, 0, 0, 0, 0x81},
    {"EffectKind81_ClearDrops", 0x487920, 0x1B, nullptr, 0, nullptr, 0, nullptr, 0, E3D_FN(EffectKind81_ClearDrops), 0, false, kEf, 0, 0, 0, 0x81},
    {"EffectKind81_MoveDrops", 0x487940, 0x7F, E3D_CALLS(kCalls487940), nullptr, 0, nullptr, 0, E3D_FN(EffectKind81_MoveDrops), 0xFF, false, kEf, 0, 0, 0, 0x81},
    {"EffectKind81_DrawDrop", 0x4879C0, 0xE7, E3D_CALLS(kCalls4879C0), nullptr, 0, nullptr, 0, E3D_FN(EffectKind81_DrawDrop), 0, false, kCa, 0, 0, 0, 0x81},
    {"EffectKind81_PlaceSources", 0x487AB0, 0x62, E3D_CALLS(kCalls487AB0), nullptr, 0, nullptr, 0, E3D_FN(EffectKind81_PlaceSources), 0, false, kEf, 0, 0, 0, 0x81},
    {"EffectKind81_Emit", 0x487B20, 0xC4, E3D_CALLS(kCalls487B20), nullptr, 0, nullptr, 0, E3D_FN(EffectKind81_Emit), 0xFF, false, kEf, 0, 0, 0, 0x81},
    {"EffectKind81_FindFreeDrop", 0x487BF0, 0x1A, nullptr, 0, nullptr, 0, nullptr, 0, E3D_FN(EffectKind81_FindFreeDrop), 0xFFFFFFFFu, false, kEf, 0, 0, 0, 0x81},
    {"EffectKind82_Run", 0x487C10, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E3D_FN(EffectKind82_Run), 0, false, kEf, 0, 24, 0, 0x82},
    {"EffectKind82_Start", 0x487C30, 0x1F, nullptr, 0, nullptr, 0, nullptr, 0, E3D_FN(EffectKind82_Start), 0, false, kEf, 0, 0, 0, 0x82},
    {"EffectKind82_Wait", 0x487C50, 0x181, E3D_CALLS(kCalls487C50), nullptr, 0, E3D_CALLS(kTables487C50), E3D_FN(EffectKind82_Wait), 0, false, kEf, 0, 0, 0, 0x82},
    {"EffectKind82_Push2", 0x487DE0, 0x3B, nullptr, 0, nullptr, 0, nullptr, 0, E3D_FN(EffectKind82_Push2), 0, false, kEf, 0, 0, 0, 0x82},
    {"EffectKind82_Check3", 0x487E20, 0x32, nullptr, 0, nullptr, 0, nullptr, 0, E3D_FN(EffectKind82_Check3), 0, false, kEf, 0, 0, 0, 0x82},
    {"EffectKind82_Push4", 0x487E60, 0x3B, nullptr, 0, nullptr, 0, nullptr, 0, E3D_FN(EffectKind82_Push4), 0, false, kEf, 0, 0, 0, 0x82},
    {"EffectKind82_Check5", 0x487EA0, 0x32, nullptr, 0, nullptr, 0, nullptr, 0, E3D_FN(EffectKind82_Check5), 0, false, kEf, 0, 0, 0, 0x82},
    {"EffectKind82_Push6", 0x487EE0, 0x3B, nullptr, 0, nullptr, 0, nullptr, 0, E3D_FN(EffectKind82_Push6), 0, false, kEf, 0, 0, 0, 0x82},
    {"EffectKind82_Check7", 0x487F20, 0x32, nullptr, 0, nullptr, 0, nullptr, 0, E3D_FN(EffectKind82_Check7), 0, false, kEf, 0, 0, 0, 0x82},
    {"EffectKind82_Push8", 0x487F60, 0x3B, nullptr, 0, nullptr, 0, nullptr, 0, E3D_FN(EffectKind82_Push8), 0, false, kEf, 0, 0, 0, 0x82},
    {"EffectKind82_Check9", 0x487FA0, 0x32, nullptr, 0, nullptr, 0, nullptr, 0, E3D_FN(EffectKind82_Check9), 0, false, kEf, 0, 0, 0, 0x82},
    {"EffectKind82_Push10", 0x487FE0, 0x3B, nullptr, 0, nullptr, 0, nullptr, 0, E3D_FN(EffectKind82_Push10), 0, false, kEf, 0, 0, 0, 0x82},
};
#undef E3D_FN
#undef E3D_CALLS
#undef E3D_N

enum : unsigned {
    k76, k79, k7A,
    k7BRun, k7BStart, k7BPulse, k7CRun, k7CStart, k7CPulse,
    k77Run, k77Count, k77Begin, k77Tick, k77Resume, k77End, k77Show, k77DrawCount,
    k78Run, k78Start, k78Grow, k78Spin, k78End, k78Ring,
    k7DRun, k7DStart, k7DInput, k7DApply, k7DPanel, k7DDial, k7DF4, k7DF3, k7DLine, k7DMode, k7DMark, k7DSetMap,
    k7FRun, k7FStart, k7FGrow, k7FCone,
    k80Run, k80Start, k80Rise, k80Hold, k80Fade, k80Step, k80Draw,
    k81Run, k81Start, k81Pour, k81Drain, k81Clear, k81Move, k81DrawDrop, k81Place, k81Emit, k81Find,
    k82Run, k82Start, k82Wait, k82Push2, k82Check3, k82Push4, k82Check5, k82Push6, k82Check7, k82Push8, k82Check9, k82Push10,
    kCount
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
// A float with a fraction, 2^-17..2^18 in size, either sign (the harness's
// FillFractions).
void FillFraction(unsigned char* at) {
    const U bits = (sh::Noise() & 0x807FFFFFu) | ((0x6Eu + sh::Noise() % 0x24u) << 23);
    std::memcpy(at, &bits, 4);
}
// EffectGte_ProjectPoint(in, out): out the screen x, y, depth - three floats
// with fractions; where out is a trail point's (+0x10 of points 1..31 at
// 0x92BF80), the point before's projection exactly when the two world points
// are the same (as the real projection gives) and else a third of the time,
// so the trail's "no direction" (0x1000) and its fills run.
U FxProjectPoint(const U* a, U answer) {
    if (!Writable(a[1], 12)) return answer;
    const U trail = at::kTrail + 0x10;
    if (a[1] >= trail + at::kTrailStride && a[1] < trail + at::kTrailStride * at::kTrailPoints &&
        (a[1] - trail) % at::kTrailStride == 0 && a[0] == a[1] - 0x10 &&
        (std::memcmp(P(a[0]), P(a[0] - at::kTrailStride), 12) == 0 || sh::Noise() % 3 == 0)) {
        std::memcpy(P(a[1]), P(a[1] - at::kTrailStride), 12);
        return answer;
    }
    for (unsigned i = 0; i < 3; ++i) FillFraction(P(a[1] + 4 * i));
    return answer;
}
// EffectGte_ProjectSize(in, size, out): out's two s16, small half the time (the
// harness's FxProjectSize); re-listed to hash both words of the size, which
// the trail writes (the standard row hashes the first only).
U FxProjectSize(const U* a, U answer) {
    if (Writable(a[2], 4)) {
        const U n = sh::Noise();
        const U v = (n & 1) ? (n >> 1) : ((n >> 1) & 0x003F003Fu);
        std::memcpy(P(a[2]), &v, 4);
    }
    return answer;
}
// EffectKind81_FindFreeDrop: as the real one, the first of the 256 drop records whose
// +0 is 0, or null; null also a quarter of the time.
U FxFindFreeDrop(const U*, U answer) {
    if (answer % 4 == 0) return 0;
    for (U i = 0; i < at::kDropCount; ++i)
        if (Mem(at::kDrops + at::kDropStride * i)[0] == 0) return at::kDrops + at::kDropStride * i;
    return 0;
}

#define E3D_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
constexpr sh::Answer kG = sh::Answer::kGarbage, kPh = sh::Answer::kPhase, kF = sh::Answer::kFlag;
constexpr U kW = 0xFFFFFFFFu, k16 = 0xFFFFu, k8 = 0xFFu;
const sh::Callee kCallees[] = {
    // the group's own called directly, by name; each argument masked to what
    // the callee reads (movsx of a word, a byte; the callers push leftovers above)
    {E3D_OURS(EffectKind77_DrawCount), 0, {}, kPh, 0, 0},
    {E3D_OURS(EffectKind78_DrawRing), 4, {k16, k16, k16, k16}, kG, 0, 0},
    {E3D_OURS(EffectKind7D_DrawPanel), 2, {k16, k16}, kG, 0, 0},
    {E3D_OURS(EffectKind7D_DrawDial), 4, {k16, k16, kW, k8}, kG, 0, 0},
    {E3D_OURS(EffectKind7D_FillF4), 12, {k16, k16, k16, k16, k16, k16, k16, k16, k8, k8, k8, k8}, kG, 0, 0},
    {E3D_OURS(EffectKind7D_FillF3), 10, {k16, k16, k16, k16, k16, k16, k8, k8, k8, k8}, kG, 0, 0},
    {E3D_OURS(EffectKind7D_Line), 8, {k16, k16, k16, k16, k8, k8, k8, k8}, kG, 0, 0},
    {E3D_OURS(EffectKind7D_DrawMode), 1, {k8}, kG, 0, 0},
    {E3D_OURS(EffectKind7D_DrawMark), 3, {k16, k16, k8}, kG, 0, 0},
    {E3D_OURS(EffectKind7D_SetMap), 0, {}, kPh, 0, 0},
    // the point hashed (the record's +0x34..+0x3F)
    {E3D_OURS(EffectKind7F_DrawCone), 3, {kW, k16, k16}, kG, 0, 0, {12, 0, 0}, nullptr, nullptr, true},
    {E3D_OURS(EffectKind80_TrailStep), 1, {kW}, kG, 0, 0},
    {E3D_OURS(EffectKind80_DrawTrail), 1, {kW}, kG, 0, 0},
    {E3D_OURS(EffectKind81_ClearDrops), 0, {}, kPh, 0, 0},
    {E3D_OURS(EffectKind81_MoveDrops), 0, {}, kF, 0, 0},
    {E3D_OURS(EffectKind81_DrawDrop), 1, {kW}, kG, 0, 0, {0x18}, nullptr, nullptr, true},
    {E3D_OURS(EffectKind81_PlaceSources), 0, {}, kPh, 0, 0},
    {E3D_OURS(EffectKind81_Emit), 0, {}, kF, 0, 0},
    {E3D_OURS(EffectKind81_FindFreeDrop), 0, {}, kG, 0, 0, {}, &FxFindFreeDrop, nullptr, true},
    // other groups' (merged), not in the standard set: EffectAngle_Mean reads
    // each angle's low 12 bits (E2E); EffectTrail_DrawCap the point's three
    // floats, size and angle as words, the shade a byte (E2F's listing)
    {E3D_OURS(EffectAngle_Mean), 2, {0xFFFu, 0xFFFu}, kG, 0, 0},
    {E3D_OURS(EffectTrail_DrawCap), 4, {kW, k16, k16, k8}, kG, 0, 0, {12, 0, 0, 0}, nullptr, nullptr, true},
    // re-listed: the trail's projections (see FxProjectPoint), and the size's
    // two words, both written by the trail
    {E3D_OURS(EffectGte_ProjectPoint), 2, {0, 0}, kG, 0, 0, {12, 0}, &FxProjectPoint, nullptr, true},
    {E3D_OURS(EffectGte_ProjectSize), 3, {0, 0, 0}, kG, 0, 0, {12, 4, 0}, &FxProjectSize, nullptr, true},
};
#undef E3D_OURS

// The state tables the dispatchers jump through, read in place; each table's
// own length (symbols.toml [[data]], none bounded by a compare).
const sh::DataTable kTables[] = {
    {0x654AFC, 2}, {0x654B04, 2}, {0x654B0C, 3}, {0x654B18, 3}, {0x654B24, 2}, {0x654B2C, 4},
    {0x654B3C, 4}, {0x654C0C, 3}, {0x654C18, 9}, {0x654C5C, 3}, {0x654C88, 24},
};
const std::uint8_t kKinds[] = {0x76, 0x77, 0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7F, 0x80, 0x81, 0x82};

// Beyond effect mode's standard regions: the pulse's two bytes and the moving
// count, the dials, the dials' answer, the drops past EffectKind30_Shards and
// the sources after them.
const sh::Region kRegions[] = {
    {0x676278, 8},
    {at::kDials, 4},
    {0x9039F4, 4},
    {0x92C5C4, at::kSources + at::kSourceCount * at::kSourceStride - 0x92C5C4},
};

// --- the seed ------------------------------------------------------------------------

unsigned char* Rec(unsigned r) { return sh::EffectRecord(r); }

// Every one of the 20 records the disturbance may move Sprite_Current among:
// kind 0x7D's +6 one of the three dials and +0x4C its address (the state
// writes 0x675DC8 + +6 and reads through +0x4C).
void Records() {
    for (unsigned r = 0; r < 20; ++r) {
        unsigned char* const e = Rec(r);
        e[6] = static_cast<unsigned char>(sh::Next() % at::kDialCount);
        SetLong(e + 0x4C, static_cast<std::int32_t>(at::kDials + (sh::Half() ? e[6] : sh::Next() % at::kDialCount)));
    }
}

// The area header and the cell words EffectKind7D_SetMap reads, small enough
// that its dword stays inside the area block's first 8 KiB (the region).
void SeedAreaHeader() {
    unsigned char* const h = Mem(at::kAreaHeader);
    h[0] = static_cast<unsigned char>(sh::Next() % 5);
    h[1] = static_cast<unsigned char>(sh::Next() % 9);
    SetWord(h + 2, sh::Next() % 0x40);
    for (U row = 0; row < at::kGridRows; ++row)
        for (U column = 0; column < at::kGridColumns; ++column) {
            const U i = h[0] * (row + at::kMapRow0) + column + 2u * Word(h + 2);
            SetWord(Mem(at::kAreaWords + 2 * i), sh::Next() % 0x300);
        }
}

void Seed(unsigned k) {
    Records();
    unsigned char* const s = Sprite_Current;
    // the dials 0..8 (past 8 ours aborts where the original reads the next rows)
    for (U i = 0; i < at::kDialCount; ++i) Mem(at::kDials + i)[0] = static_cast<unsigned char>(sh::Next() % 9);
    // the pulse's index 0 or 1 (only these functions write it)
    Mem(at::kPulseIndex)[0] = static_cast<unsigned char>(sh::Next() % 2);
    // the counter the kinds end on, and the frame counts
    Mem(at::kCounter)[0] = static_cast<unsigned char>(PickOf(1, 4, 0xC, 0, 2, sh::Next()));
    s[9] = static_cast<unsigned char>(PickOf(0, 1, 2, 0xF, 0x10, 0x11, 0xFF, sh::Next()));
    switch (k) {
    case k77Tick:
    case k77Begin: {
        const U count = PickOf(0xC7, 0xC8, 0xC6, 0, 0xFF, sh::Next() & 0xFF);
        const U frames = PickOf(0x1C, 0x1D, 0x1E, 0x26, 0x27, 0x28, 0x7F, sh::Next() & 0x7F);
        SetWord(Mem(at::kTally), (sh::Half() ? 0x8000u : 0u) | (frames << 8) | count);
        break;
    }
    case k77End:
    case k77Show:
        Mem(at::kStep)[0] = static_cast<unsigned char>(sh::Next());
        Field_Request = static_cast<unsigned char>(PickOf(2, 0, 1, 5, sh::Next()));
        Game_Mode = static_cast<unsigned short>(PickOf(4, 2, 0x104, sh::Next()));
        break;
    case k7DInput:
        SetWord(Mem(at::kInputPressed), PickOf(0x8000, 0x2000, 0xA000, 0, sh::Next()) | (sh::Half() ? 0u : sh::Next() & 0x5FFFu));
        SetWord(Mem(at::kConfirmButtons), PickOf(0x20, 0x40, 0, sh::Next()));
        SetWord(Mem(at::kCancelButtons), PickOf(0x40, 0x10, 0, sh::Next()));
        break;
    case k7DSetMap: SeedAreaHeader(); break;
    case k80Rise: SetLong(s + 0x34, static_cast<std::int32_t>(PickOf(0x160000, 0x170000 - 0x10000, 0x16FFFF, 0x170000, sh::Next()))); break;
    case k80Step:
        // a quarter of the time every point at the record's point (as the
        // start leaves the trail): no pair has a direction
        if (sh::Next() % 4 == 0)
            for (U i = 0; i < at::kTrailPoints; ++i) std::memcpy(Mem(at::kTrail + at::kTrailStride * i), s + 0x34, 12);
        break;
    case k80Draw: {
        // some pairs on the same screen point (the skip), whole or one axis
        for (U i = 1; i < at::kTrailPoints; ++i) {
            unsigned char* const p = Mem(at::kTrail + at::kTrailStride * i + 0x10);
            if (sh::Next() % 3 == 0) std::memcpy(p, p - at::kTrailStride, sh::Half() ? 8 : 4);
        }
        break;
    }
    case k81Move:
    case k81Drain:
    case k81Pour:
        for (U i = 0; i < at::kDropCount; ++i) {
            unsigned char* const d = Mem(at::kDrops + at::kDropStride * i);
            d[0] = static_cast<unsigned char>(sh::Next() % 4 == 0 ? 0 : sh::Next());
            d[2] = static_cast<unsigned char>(PickOf(1, 2, 0, sh::Next()));
            d[3] = static_cast<unsigned char>(sh::Next() % 2);
        }
        if (k == k81Move && sh::Next() % 4 == 0)
            for (U i = 0; i < at::kDropCount; ++i) Mem(at::kDrops + at::kDropStride * i)[0] = 0;
        break;
    case k81Emit:
        for (U i = 0; i < at::kSourceCount; ++i) {
            unsigned char* const src = Mem(at::kSources + at::kSourceStride * i);
            src[0] = static_cast<unsigned char>(sh::Next() % 3 == 0 ? 0 : 1);
            src[3] = static_cast<unsigned char>(PickOf(0, 0, 1, sh::Next()));
        }
        break;
    case k82Wait:
    case k82Check3:
    case k82Check5:
    case k82Check7:
    case k82Check9: {
        const U leader = sh::Next();
        SetLong(Mem(at::kLeaderX), static_cast<std::int32_t>(leader));
        SetLong(Mem(at::kObject0X), static_cast<std::int32_t>(leader + PickOf(0, 1, 0xFFFFFFFFu, 0x10000, 0xFFFF0000u, sh::Next())));
        Mem(at::kCounterB)[0] = static_cast<unsigned char>(PickOf(3, 4, 5, 0xFF, 6, 2, 7, 0xFE, sh::Next()));
        Mem(at::kCounterC)[0] = static_cast<unsigned char>(PickOf(0x80, 0, 0x81, sh::Next()));
        break;
    }
    default: break;
    }
}

// The arguments: the dial's grid one of the three and its turn's low byte
// below 9; a mark byte at its values; the cone's point a record's +0x34; the
// trail's pool; a drop record.
void Args(unsigned k, U* a) {
    switch (k) {
    case k7DDial:
        a[2] = at::kDialGrids + at::kGridSize * (sh::Next() % at::kDialCount);
        a[3] = (sh::Next() & 0xFFFFFF00u) | (sh::Next() % at::kGridColumns);
        break;
    case k7DMark: a[2] = (sh::Next() & 0xFFFFFF00u) | PickOf(1, 0xFF, 0, 2, 0xFE, sh::Next() & 0xFF); break;
    case k7DF4: a[11] = sh::Next(); break;
    case k7FCone: a[0] = Key(Rec(sh::Next() % 20) + 0x34); break;
    case k80Step:
    case k80Draw: a[0] = at::kTrail; break;
    case k81DrawDrop: a[0] = at::kDrops + at::kDropStride * (sh::Next() % at::kDropCount); break;
    default: break;
    }
}

// What the states read again after a call, moved by the group's case of the
// harness's disturbance (from its hash only): the frame count +9, the radii
// +0x2E / +0x30, the counter 0x903848, the tally, Input_Pressed, a trail
// point's words, a drop's life, record 0's x.
void Disturb(U h) {
    const U v = h >> 8;
    unsigned char* const s = Sprite_Current;
    if (!sh::InRegions(s, 0x80)) return;
    switch (h % 9) {
    case 0: s[9] = static_cast<unsigned char>((v & 1) ? 1u : v >> 1); break;
    case 1: SetWord(s + 0x2E + 2 * (v & 1), v >> 1); break;
    case 2: Mem(at::kCounter)[0] = static_cast<unsigned char>((v & 1) ? 0xCu : v >> 1); break;
    case 3: SetWord(Mem(at::kTally), v); break;
    case 4: SetWord(Mem(at::kInputPressed), v); break;
    case 5: SetWord(Mem(at::kTrail + 0x1C + at::kTrailStride * (v % at::kTrailPoints)), v >> 5); break;
    case 6: Mem(at::kDrops + 2 + at::kDropStride * (v % at::kDropCount))[0] = static_cast<unsigned char>(v >> 8); break;
    case 7: SetLong(Mem(at::kObject0X), static_cast<std::int32_t>(v)); break;
    case 8: SetLong(s + 0x34 + 4 * (v % 3), static_cast<std::int32_t>(v)); break;
    default: break;
    }
}

}  // namespace

void SelfTest() {
    // BOF3X_E3D_ONLY: the clones whose name contains it (a control's run)
    static sh::Clone chosen[kCount];
    static unsigned index[kCount];
    const char* const only = std::getenv("BOF3X_E3D_ONLY");
    unsigned n = 0;
    for (unsigned k = 0; k < kCount; ++k)
        if (!only || !*only || std::strstr(kAll[k].name, only)) {
            index[n] = k;
            chosen[n++] = kAll[k];
        }
    if (n == 0) bof3::Fatal("effect_3d: BOF3X_E3D_ONLY=%s names no clone", only);
    const char* const rounds = std::getenv("BOF3X_E3D_ROUNDS");
    static unsigned* s_index = index;
    sh::Group g = {"effect_3d", chosen, n, kCallees, sizeof kCallees / sizeof kCallees[0], kTables,
                   sizeof kTables / sizeof kTables[0], kRegions, sizeof kRegions / sizeof kRegions[0],
                   [](unsigned k) { Seed(s_index[k]); }, &Disturb,
                   rounds && *rounds ? static_cast<unsigned>(std::strtoul(rounds, nullptr, 0)) : 4000u};
    g.args = [](unsigned k, U* a) { Args(s_index[k], a); };
    g.effect = true;
    g.kinds = kKinds;
    g.n_kinds = sizeof kKinds;
    sh::Run(g);
}

}  // namespace effect_3d

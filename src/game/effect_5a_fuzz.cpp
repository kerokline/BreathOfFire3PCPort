// BOF3X_SHADOW=effect_5a: group E5A's 55 functions through the scenario harness
// in effect mode (scenario_harness.h, docs/scenario_harness.md section 8), once
// at start-up. docs/effect_5a.md section 4. BOF3X_E5A_ONLY=<name> runs the
// clones whose name contains it (the controls' speed-up); BOF3X_E5A_ROUNDS
// sets the rounds (default 4,000).
//
// The clone table is tools/band_rows.py --group E5A --clones --harness scenario
// (2026-10-03), each extent read again to its last instruction (capstone) and
// the names given, with four starts added: sub-kind 4's state 1 0x4FD4C0 (the
// tool counted it into 0x4FD490, which tail-jumps to it) and state 3 0x4FD630
// (code no list has), sub-kind 0x1A's draw 0x4FDCC0 (its own frame and ret,
// counted into 0x4FDC80) and sub-kind 7's top draw 0x4FE640 (code no list has).
// The cut's 0x4FD350 and 0x4FD3E0 are area_backdrop.cpp's already. Every
// function runs in effect mode with Sprite_Current one of the 20 records, +5
// 0x18; the four that take arguments are kCall clones.
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_5a.h"
#include "game/effect_5a_callees.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace effect_5a {
namespace {

namespace sh = scenario_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using U = std::uint32_t;

// tools/band_rows.py --group E5A --clones, 2026-10-03 (0x4FD490 and 0x4FDC80 cut
// at their tail jumps, 0x4FD4C0 and 0x4FDCC0 their own).
constexpr sh::CallSite kCalls4FD2E0[] = {{0x46, 0x4FD350}, {0x4E, 0x4FD3E0}, {0x67, 0x4FD350}};
constexpr sh::CallSite kCalls4FD490[] = {{0x7, 0x57C140}, {0x1C, 0x4FD630}, {0x24, 0x4FD4C0}};
constexpr sh::CallSite kCalls4FD4C0[] = {{0xA, 0x57C140}, {0x5C, 0x4FD6A0}, {0x67, 0x572FA0}, {0x95, 0x4FD6A0}, {0xA0, 0x572FA0}};
constexpr sh::CallSite kCalls4FD570[] = {{0x5C, 0x4FD6A0}, {0x67, 0x572FA0}, {0x9F, 0x4FD6A0}, {0xAA, 0x572FA0}};
constexpr sh::CallSite kCalls4FD630[] = {{0x20, 0x5A77C0}, {0x2B, 0x572FA0}, {0x54, 0x4FD6A0}, {0x5F, 0x572FA0}};
constexpr sh::CallSite kCalls4FD6A0[] = {{0x1B, 0x5A7B90}, {0x2F, 0x5A8200}, {0x3E, 0x5A8060}, {0x52, 0x5A7D70}, {0x5C, 0x5A8DE0}, {0x66, 0x5A8E00}, {0x72, 0x5A75D0}, {0x79, 0x5A77A0}, {0x106, 0x5A85F0}, {0x10F, 0x5A9290}, {0x114, 0x5A7BC0}, {0x121, 0x572A00}};
constexpr sh::CallSite kCalls4FD800[] = {{0x60, 0x4FDB70}, {0x82, 0x4FDB70}};
constexpr sh::CallSite kCalls4FD890[] = {{0x18, 0x4FDB70}, {0x37, 0x4FDB70}};
constexpr sh::CallSite kCalls4FD910[] = {{0x43, 0x4FDB70}, {0x81, 0x4FDB70}};
constexpr sh::CallSite kCalls4FD9D0[] = {{0x39, 0x4FDB70}, {0x59, 0x4FDB70}};
constexpr sh::CallSite kCalls4FDAA0[] = {{0x3E, 0x4FDB70}, {0x89, 0x4FDB70}};
constexpr sh::CallSite kCalls4FDBE0[] = {{0x13, 0x4FDCC0}};
constexpr sh::CallSite kCalls4FDC00[] = {{0x11, 0x4FDCC0}};
constexpr sh::CallSite kCalls4FDC20[] = {{0x2C, 0x4FDCC0}};
constexpr sh::CallSite kCalls4FDC60[] = {{0x11, 0x4FDCC0}};
constexpr sh::CallSite kCalls4FDC80[] = {{0x2D, 0x4FDCC0}};
constexpr sh::CallSite kCalls4FDCC0[] = {{0x2A, 0x5A75D0}, {0x32, 0x5A77A0}, {0xD2, 0x5A85F0}, {0xD8, 0x5A9290}, {0xE5, 0x572A00}, {0xFD, 0x572FA0}};
constexpr sh::CallSite kCalls4FDDF0[] = {{0x10, 0x57C140}, {0x2F, 0x4FDB70}};
constexpr sh::CallSite kCalls4FDE30[] = {{0x7, 0x57C140}};
constexpr sh::CallSite kCalls4FDE50[] = {{0x17, 0x4FDB70}, {0x32, 0x4FDB70}};
constexpr sh::CallSite kCalls4FDEB0[] = {{0x19, 0x4FDB70}, {0x37, 0x4FDB70}, {0x56, 0x4FDB70}};
constexpr sh::CallSite kCalls4FDF40[] = {{0x32, 0x4FE040}, {0x37, 0x4FE1A0}};
constexpr sh::CallSite kCalls4FDF80[] = {{0xE, 0x57C140}, {0x31, 0x587740}, {0x3B, 0x587740}, {0x43, 0x4FE1A0}};
constexpr sh::CallSite kCalls4FDFD0[] = {{0x48, 0x57C110}, {0x58, 0x4FE040}, {0x5D, 0x4FE1A0}};
constexpr sh::CallSite kCalls4FE040[] = {{0xE0, 0x5720C0}};
constexpr sh::CallSite kCalls4FE1A0[] = {{0x3A, 0x5A75D0}, {0x42, 0x5A77A0}, {0xFF, 0x5A85F0}, {0x105, 0x5A9290}, {0x111, 0x572A00}, {0x155, 0x572FA0}};
constexpr sh::CallSite kCalls4FE350[] = {{0x2D, 0x57C140}, {0x96, 0x579F00}, {0xB3, 0x579F00}, {0xDB, 0x4FE510}, {0xE0, 0x4FE640}};
constexpr sh::CallSite kCalls4FE440[] = {{0x22, 0x57C140}, {0x3F, 0x4FE510}, {0x44, 0x4FE640}};
constexpr sh::CallSite kCalls4FE490[] = {{0x46, 0x579F00}, {0x64, 0x579F00}, {0x6C, 0x4FE510}};
constexpr sh::CallSite kCalls4FE510[] = {{0x17, 0x5A75D0}, {0x1F, 0x5A77A0}, {0xC1, 0x5A85F0}, {0xC7, 0x5A9290}, {0xD2, 0x572A00}, {0xFE, 0x572FA0}};
constexpr sh::CallSite kCalls4FE640[] = {{0xB, 0x5A75D0}, {0x13, 0x5A77A0}, {0xC4, 0x5A85F0}, {0xCA, 0x5A9290}, {0xD7, 0x572A00}, {0x102, 0x572FA0}};
constexpr sh::CallSite kCalls4FE770[] = {{0x7, 0x57C140}, {0x58, 0x4FE510}, {0x5D, 0x4FE640}};
constexpr sh::CallSite kCalls4FE7E0[] = {{0x7, 0x57C140}, {0x24, 0x4FE510}, {0x29, 0x4FE640}};
constexpr sh::CallSite kCalls4FE810[] = {{0x45, 0x4FE510}, {0x4A, 0x4FE640}};
constexpr sh::CallSite kCalls4FE860[] = {{0x7, 0x57C140}, {0x32, 0x4FE510}, {0x37, 0x4FE640}};
constexpr sh::CallSite kCalls4FE8A0[] = {{0x25, 0x4FEC20}, {0x4A, 0x587740}};
constexpr sh::CallSite kCalls4FE920[] = {{0x2D, 0x4FEC20}, {0x37, 0x4FEAF0}, {0x47, 0x5A75D0}, {0x4F, 0x5A77A0}, {0xCE, 0x5A8250}, {0xD4, 0x5A92E0}, {0x16C, 0x572A00}, {0x194, 0x572FA0}};
constexpr sh::CallSite kCalls4FEAF0[] = {{0x1A, 0x5A75D0}, {0x22, 0x5A77A0}, {0xCA, 0x5A85F0}, {0xD0, 0x5A9290}, {0xE6, 0x572A00}, {0x103, 0x572FA0}};
constexpr sh::CallSite kCalls4FEC20[] = {{0x3E, 0x5A7B90}, {0x52, 0x5A8200}, {0x61, 0x5A8060}, {0x75, 0x5A7D70}, {0x7F, 0x5A8DE0}, {0x89, 0x5A8E00}, {0xA3, 0x5A75D0}, {0xAA, 0x5A77A0}, {0x13B, 0x5A85F0}, {0x141, 0x5A9290}, {0x14C, 0x572A00}, {0x17F, 0x572FA0}, {0x1A1, 0x5A7BC0}};
constexpr sh::CallSite kCalls4FEDD0[] = {{0x3, 0x4FEE70}, {0x17, 0x4FEE70}};
constexpr sh::CallSite kCalls4FEF70[] = {{0x9, 0x4FF150}, {0x1A, 0x4FF150}};
constexpr sh::CallSite kCalls4FEFC0[] = {{0x2E, 0x4FF150}, {0x67, 0x4FF150}};
constexpr sh::CallSite kCalls4FF060[] = {{0xC, 0x4FF150}, {0x1D, 0x4FF150}};
constexpr sh::CallSite kCalls4FF0B0[] = {{0x1D, 0x4FF150}, {0x3D, 0x4FF150}};
constexpr sh::CallSite kCalls4FF120[] = {{0xC, 0x4FF150}, {0x1D, 0x4FF150}};
constexpr sh::CallSite kCalls4FF150[] = {{0x3D, 0x5A7B90}, {0x51, 0x5A8200}, {0x60, 0x5A8060}, {0x74, 0x5A7D70}, {0x7E, 0x5A8DE0}, {0x88, 0x5A8E00}, {0xB4, 0x5A75D0}, {0xBC, 0x5A77A0}, {0x13C, 0x5A85F0}, {0x142, 0x5A9290}, {0x16F, 0x572A00}, {0x196, 0x572FA0}, {0x1BB, 0x5A7BC0}};

#define E5A_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define E5A_CALLS(a) a, E5A_N(a)
#define E5A_FN(name) reinterpret_cast<const void*>(&::name)
constexpr sh::Shape kEf = sh::Shape::kEffect, kCa = sh::Shape::kCall;
constexpr U kScratch0 = sh::ArgAt(0, sh::Arg::kScratch);
// {name, base, size, calls, imms, tables, ours, ret_mask, calm, shape, pointers, state_span, sub_span, kind}
const sh::Clone kAll[] = {
    {"EffectKind18_01_Sunset", 0x4FD2E0, 0x70, E5A_CALLS(kCalls4FD2E0), nullptr, 0, nullptr, 0, E5A_FN(EffectKind18_01_Sunset), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18_04_Run", 0x4FD470, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E5A_FN(EffectKind18_04_Run), 0, false, kEf, 0, 0, 4, 0x18},
    {"EffectKind18_04_Start", 0x4FD490, 0x29, E5A_CALLS(kCalls4FD490), nullptr, 0, nullptr, 0, E5A_FN(EffectKind18_04_Start), 0, false, kEf, 0, 0, 4, 0x18},
    {"EffectKind18_04_Shut", 0x4FD4C0, 0xAE, E5A_CALLS(kCalls4FD4C0), nullptr, 0, nullptr, 0, E5A_FN(EffectKind18_04_Shut), 0, false, kEf, 0, 0, 4, 0x18},
    {"EffectKind18_04_Swing", 0x4FD570, 0xB8, E5A_CALLS(kCalls4FD570), nullptr, 0, nullptr, 0, E5A_FN(EffectKind18_04_Swing), 0, false, kEf, 0, 0, 4, 0x18},
    {"EffectKind18_04_Open", 0x4FD630, 0x6D, E5A_CALLS(kCalls4FD630), nullptr, 0, nullptr, 0, E5A_FN(EffectKind18_04_Open), 0, false, kEf, 0, 0, 4, 0x18},
    {"EffectKind18_04_DrawPanel", 0x4FD6A0, 0x131, E5A_CALLS(kCalls4FD6A0), nullptr, 0, nullptr, 0, E5A_FN(EffectKind18_04_DrawPanel), 0, false, kCa, kScratch0, 0, 0, 0x18},
    {"EffectKind18_05_Run", 0x4FD7E0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E5A_FN(EffectKind18_05_Run), 0, false, kEf, 0, 0, 5, 0x18},
    {"EffectKind18_05_Start", 0x4FD800, 0x8B, E5A_CALLS(kCalls4FD800), nullptr, 0, nullptr, 0, E5A_FN(EffectKind18_05_Start), 0, false, kEf, 0, 0, 5, 0x18},
    {"EffectKind18_05_Idle", 0x4FD890, 0x77, E5A_CALLS(kCalls4FD890), nullptr, 0, nullptr, 0, E5A_FN(EffectKind18_05_Idle), 0, false, kEf, 0, 0, 5, 0x18},
    {"EffectKind18_05_FadeIn", 0x4FD910, 0xB8, E5A_CALLS(kCalls4FD910), nullptr, 0, nullptr, 0, E5A_FN(EffectKind18_05_FadeIn), 0, false, kEf, 0, 0, 5, 0x18},
    {"EffectKind18_05_Pulse", 0x4FD9D0, 0xC2, E5A_CALLS(kCalls4FD9D0), nullptr, 0, nullptr, 0, E5A_FN(EffectKind18_05_Pulse), 0, false, kEf, 0, 0, 5, 0x18},
    {"EffectKind18_05_FadeOut", 0x4FDAA0, 0xC2, E5A_CALLS(kCalls4FDAA0), nullptr, 0, nullptr, 0, E5A_FN(EffectKind18_05_FadeOut), 0, false, kEf, 0, 0, 5, 0x18},
    {"Gfx_ClutStripCopy16", 0x4FDB70, 0x47, nullptr, 0, nullptr, 0, nullptr, 0, E5A_FN(Gfx_ClutStripCopy16), 0, false, kCa, 0, 0, 0, 0x18},
    {"EffectKind18_1A_Run", 0x4FDBC0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E5A_FN(EffectKind18_1A_Run), 0, false, kEf, 0, 0, 5, 0x18},
    {"EffectKind18_1A_Start", 0x4FDBE0, 0x18, E5A_CALLS(kCalls4FDBE0), nullptr, 0, nullptr, 0, E5A_FN(EffectKind18_1A_Start), 0, false, kEf, 0, 0, 5, 0x18},
    {"EffectKind18_1A_WaitOn", 0x4FDC00, 0x16, E5A_CALLS(kCalls4FDC00), nullptr, 0, nullptr, 0, E5A_FN(EffectKind18_1A_WaitOn), 0, false, kEf, 0, 0, 5, 0x18},
    {"EffectKind18_1A_SlideOut", 0x4FDC20, 0x31, E5A_CALLS(kCalls4FDC20), nullptr, 0, nullptr, 0, E5A_FN(EffectKind18_1A_SlideOut), 0, false, kEf, 0, 0, 5, 0x18},
    {"EffectKind18_1A_WaitOff", 0x4FDC60, 0x16, E5A_CALLS(kCalls4FDC60), nullptr, 0, nullptr, 0, E5A_FN(EffectKind18_1A_WaitOff), 0, false, kEf, 0, 0, 5, 0x18},
    {"EffectKind18_1A_SlideBack", 0x4FDC80, 0x32, E5A_CALLS(kCalls4FDC80), nullptr, 0, nullptr, 0, E5A_FN(EffectKind18_1A_SlideBack), 0, false, kEf, 0, 0, 5, 0x18},
    {"EffectKind18_1A_Draw", 0x4FDCC0, 0x10C, E5A_CALLS(kCalls4FDCC0), nullptr, 0, nullptr, 0, E5A_FN(EffectKind18_1A_Draw), 0, false, kEf, 0, 0, 5, 0x18},
    {"EffectKind18_1F_Run", 0x4FDDD0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E5A_FN(EffectKind18_1F_Run), 0, false, kEf, 0, 0, 4, 0x18},
    {"EffectKind18_1F_Start", 0x4FDDF0, 0x40, E5A_CALLS(kCalls4FDDF0), nullptr, 0, nullptr, 0, E5A_FN(EffectKind18_1F_Start), 0, false, kEf, 0, 0, 4, 0x18},
    {"EffectKind18_1F_Wait", 0x4FDE30, 0x1C, E5A_CALLS(kCalls4FDE30), nullptr, 0, nullptr, 0, E5A_FN(EffectKind18_1F_Wait), 0, false, kEf, 0, 0, 4, 0x18},
    {"EffectKind18_1F_Brighten", 0x4FDE50, 0x56, E5A_CALLS(kCalls4FDE50), nullptr, 0, nullptr, 0, E5A_FN(EffectKind18_1F_Brighten), 0, false, kEf, 0, 0, 4, 0x18},
    {"EffectKind18_1F_Cycle", 0x4FDEB0, 0x67, E5A_CALLS(kCalls4FDEB0), nullptr, 0, nullptr, 0, E5A_FN(EffectKind18_1F_Cycle), 0, false, kEf, 0, 0, 4, 0x18},
    {"EffectKind18_06_Run", 0x4FDF20, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E5A_FN(EffectKind18_06_Run), 0, false, kEf, 0, 0, 3, 0x18},
    {"EffectKind18_06_Start", 0x4FDF40, 0x3C, E5A_CALLS(kCalls4FDF40), nullptr, 0, nullptr, 0, E5A_FN(EffectKind18_06_Start), 0, false, kEf, 0, 0, 3, 0x18},
    {"EffectKind18_06_Wait", 0x4FDF80, 0x48, E5A_CALLS(kCalls4FDF80), nullptr, 0, nullptr, 0, E5A_FN(EffectKind18_06_Wait), 0, false, kEf, 0, 0, 3, 0x18},
    {"EffectKind18_06_Turn", 0x4FDFD0, 0x62, E5A_CALLS(kCalls4FDFD0), nullptr, 0, nullptr, 0, E5A_FN(EffectKind18_06_Turn), 0, false, kEf, 0, 0, 3, 0x18},
    {"EffectKind18_06_SetMap", 0x4FE040, 0x157, E5A_CALLS(kCalls4FE040), nullptr, 0, nullptr, 0, E5A_FN(EffectKind18_06_SetMap), 0, false, kEf, 0, 0, 3, 0x18},
    {"EffectKind18_06_Draw", 0x4FE1A0, 0x184, E5A_CALLS(kCalls4FE1A0), nullptr, 0, nullptr, 0, E5A_FN(EffectKind18_06_Draw), 0, false, kEf, 0, 0, 3, 0x18},
    {"EffectKind18_07_Run", 0x4FE330, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E5A_FN(EffectKind18_07_Run), 0, false, kEf, 0, 0, 4, 0x18},
    {"EffectKind18_07_Start", 0x4FE350, 0xE5, E5A_CALLS(kCalls4FE350), nullptr, 0, nullptr, 0, E5A_FN(EffectKind18_07_Start), 0, false, kEf, 0, 0, 4, 0x18},
    {"EffectKind18_07_Wait", 0x4FE440, 0x49, E5A_CALLS(kCalls4FE440), nullptr, 0, nullptr, 0, E5A_FN(EffectKind18_07_Wait), 0, false, kEf, 0, 0, 4, 0x18},
    {"EffectKind18_07_Slide", 0x4FE490, 0x71, E5A_CALLS(kCalls4FE490), nullptr, 0, nullptr, 0, E5A_FN(EffectKind18_07_Slide), 0, false, kEf, 0, 0, 4, 0x18},
    {"EffectKind18_07_Draw", 0x4FE510, 0x124, E5A_CALLS(kCalls4FE510), nullptr, 0, nullptr, 0, E5A_FN(EffectKind18_07_Draw), 0, false, kEf, 0, 0, 4, 0x18},
    {"EffectKind18_07_DrawTop", 0x4FE640, 0x10F, E5A_CALLS(kCalls4FE640), nullptr, 0, nullptr, 0, E5A_FN(EffectKind18_07_DrawTop), 0, false, kEf, 0, 0, 4, 0x18},
    {"EffectKind18_08_Run", 0x4FE750, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E5A_FN(EffectKind18_08_Run), 0, false, kEf, 0, 0, 7, 0x18},
    {"EffectKind18_08_Start", 0x4FE770, 0x62, E5A_CALLS(kCalls4FE770), nullptr, 0, nullptr, 0, E5A_FN(EffectKind18_08_Start), 0, false, kEf, 0, 0, 7, 0x18},
    {"EffectKind18_08_Wait", 0x4FE7E0, 0x2E, E5A_CALLS(kCalls4FE7E0), nullptr, 0, nullptr, 0, E5A_FN(EffectKind18_08_Wait), 0, false, kEf, 0, 0, 7, 0x18},
    {"EffectKind18_08_Shake", 0x4FE810, 0x4F, E5A_CALLS(kCalls4FE810), nullptr, 0, nullptr, 0, E5A_FN(EffectKind18_08_Shake), 0, false, kEf, 0, 0, 7, 0x18},
    {"EffectKind18_08_WaitPush", 0x4FE860, 0x3C, E5A_CALLS(kCalls4FE860), nullptr, 0, nullptr, 0, E5A_FN(EffectKind18_08_WaitPush), 0, false, kEf, 0, 0, 7, 0x18},
    {"EffectKind18_08_Topple", 0x4FE8A0, 0x71, E5A_CALLS(kCalls4FE8A0), nullptr, 0, nullptr, 0, E5A_FN(EffectKind18_08_Topple), 0, false, kEf, 0, 0, 7, 0x18},
    {"EffectKind18_08_Crash", 0x4FE920, 0x1C9, E5A_CALLS(kCalls4FE920), nullptr, 0, nullptr, 0, E5A_FN(EffectKind18_08_Crash), 0, false, kEf, 0, 0, 7, 0x18},
    {"EffectKind18_08_DrawFallen", 0x4FEAF0, 0x123, E5A_CALLS(kCalls4FEAF0), nullptr, 0, nullptr, 0, E5A_FN(EffectKind18_08_DrawFallen), 0, false, kEf, 0, 0, 7, 0x18},
    {"EffectKind18_08_DrawTilted", 0x4FEC20, 0x1AE, E5A_CALLS(kCalls4FEC20), nullptr, 0, nullptr, 0, E5A_FN(EffectKind18_08_DrawTilted), 0, false, kCa, 0, 0, 0, 0x18},
    {"EffectKind18_09_Pattern", 0x4FEDD0, 0x99, E5A_CALLS(kCalls4FEDD0), nullptr, 0, nullptr, 0, E5A_FN(EffectKind18_09_Pattern), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18_0A_Run", 0x4FEF50, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E5A_FN(EffectKind18_0A_Run), 0, false, kEf, 0, 0, 5, 0x18},
    {"EffectKind18_0A_Shut", 0x4FEF70, 0x42, E5A_CALLS(kCalls4FEF70), nullptr, 0, nullptr, 0, E5A_FN(EffectKind18_0A_Shut), 0, false, kEf, 0, 0, 5, 0x18},
    {"EffectKind18_0A_Swing", 0x4FEFC0, 0x99, E5A_CALLS(kCalls4FEFC0), nullptr, 0, nullptr, 0, E5A_FN(EffectKind18_0A_Swing), 0, false, kEf, 0, 0, 5, 0x18},
    {"EffectKind18_0A_Hold", 0x4FF060, 0x4F, E5A_CALLS(kCalls4FF060), nullptr, 0, nullptr, 0, E5A_FN(EffectKind18_0A_Hold), 0, false, kEf, 0, 0, 5, 0x18},
    {"EffectKind18_0A_Slide", 0x4FF0B0, 0x70, E5A_CALLS(kCalls4FF0B0), nullptr, 0, nullptr, 0, E5A_FN(EffectKind18_0A_Slide), 0, false, kEf, 0, 0, 5, 0x18},
    {"EffectKind18_0A_Open", 0x4FF120, 0x26, E5A_CALLS(kCalls4FF120), nullptr, 0, nullptr, 0, E5A_FN(EffectKind18_0A_Open), 0, false, kEf, 0, 0, 5, 0x18},
    {"EffectKind18_0A_DrawDoor", 0x4FF150, 0x1C8, E5A_CALLS(kCalls4FF150), nullptr, 0, nullptr, 0, E5A_FN(EffectKind18_0A_DrawDoor), 0, false, kCa, 0, 0, 0, 0x18},
};
#undef E5A_FN
#undef E5A_CALLS
#undef E5A_N

constexpr unsigned kCount = sizeof kAll / sizeof kAll[0];
static_assert(kCount == 55, "the cut's 51 not ours yet and the four added");

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
unsigned char* Mem(U a) { return sh::Mem(a); }
template <typename... T> U PickOf(T... v) {
    const U values[] = {static_cast<U>(v)...};
    return sh::Pick(values, sizeof...(v));
}

// --- the effects -----------------------------------------------------------------------

// 0x4FEE70 (no group's): the three story flags of 0x65DE60 as bits, plus 1 -
// 1..8 in a whole eax (EffectKind18_09_Pattern compares all of it with +2 and
// indexes two tables by it). A quarter of the time the current record's +2
// when that is 1..8, so the "unchanged" path runs; drawn from the log's hash.
U FxPattern(const U*, U) {
    const U n = sh::Noise();
    const unsigned char* const s = Sprite_Current;
    if ((n & 3) == 0 && sh::InRegions(s, 0x80) && s[2] >= 1 && s[2] <= 8) return s[2];
    return 1 + (n >> 2) % 8;
}

// MapView_LinkPrimAt, as effect mode's row has it (the cursor + size & 0xFF two
// times in three, inside the packet buffer), its dy compared on the low byte
// only: the real one reads a signed byte (symbols.toml), and
// EffectKind18_0A_DrawDoor pushes dl over whatever Prim_SetTexture left in edx.
U FxLink(const U* a, U answer) {
    if (sh::Noise() % 3 != 0) {
        unsigned char* const next = sh::Pointer(0x7E0670);
        const unsigned n = a[3] & 0xFF;
        if (next >= sh::Packets() && next + n + 0x40 <= sh::Packets() + 0x800) sh::SetPointer(0x7E0670, next + n);
    }
    return answer;
}
// Gfx_ClutStripCopy16, louder under sub-kind 5's states: a quarter of the time
// the current record's nibble word +0x3A moved (from the noise), which _Idle
// and _Pulse test again after their copies (the "on" test) - the group's case 3
// alone reached _Pulse's re-read 72 times in 40,000 rounds (section 6).
bool In(U lo, U hi);
U FxCopy(const U*, U answer) {
    const U n = sh::Noise();
    unsigned char* const s = Sprite_Current;
    if (n % 4 == 0 && In(0x4FD7E0, 0x4FDAA0) && sh::InRegions(s + 0x3A, 2)) SetWord(s + 0x3A, n >> 8);
    return answer;
}
#define E5A_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
constexpr sh::Answer kG = sh::Answer::kGarbage, kPh = sh::Answer::kPhase;
constexpr U kW = 0xFFFFFFFFu;
const sh::Callee kCallees[] = {
    // the group's own, called by name (directly, by a tail jump, or through a
    // swapped table cell): each logs what it is handed
    {E5A_OURS(EffectKind18_04_Shut), 0, {}, kPh, 0, 0},
    {E5A_OURS(EffectKind18_04_Open), 0, {}, kPh, 0, 0},
    // the point hashed (6 bytes: the pad is the caller's stale stack), the
    // angle's word (0x4FD570 pushes 0x800 - +0xC in ax over a stale eax)
    {E5A_OURS(EffectKind18_04_DrawPanel), 3, {kW, 0xFFFF, kW}, kG, 0, 0, {6, 0, 0}},
    {E5A_OURS(Gfx_ClutStripCopy16), 4, {kW, kW, kW, kW}, kG, 0, 0, {}, &FxCopy},
    {E5A_OURS(EffectKind18_1A_Draw), 0, {}, kPh, 0, 0},
    {E5A_OURS(EffectKind18_06_SetMap), 0, {}, kPh, 0, 0},
    {E5A_OURS(EffectKind18_06_Draw), 0, {}, kPh, 0, 0},
    {E5A_OURS(EffectKind18_07_Draw), 0, {}, kPh, 0, 0},
    {E5A_OURS(EffectKind18_07_DrawTop), 0, {}, kPh, 0, 0},
    {E5A_OURS(EffectKind18_08_DrawTilted), 1, {kW}, kG, 0, 0},
    {E5A_OURS(EffectKind18_08_DrawFallen), 0, {}, kPh, 0, 0},
    {E5A_OURS(EffectKind18_0A_DrawDoor), 3, {kW, kW, kW}, kG, 0, 0},
    // ours of area_backdrop.cpp, in no standard set: the gradient reads its
    // three arguments' low bytes
    {E5A_OURS(Gfx_DrawSkyGradient), 3, {0xFF, 0xFF, 0xFF}, kG, 0, 0},
    {E5A_OURS(Gfx_DrawSunsetGlow), 0, {}, kPh, 0, 0},
    // no group's: re-listed over effect mode's kFlag row (a byte with garbage
    // above), which the whole-eax compare cannot use
    {"0x4FEE70 (no group)", at::kPattern, at::kPattern, 0, {}, kG, 0, 0, {}, &FxPattern},
    // re-listed: dy (a signed byte to the real one) masked to its byte
    {E5A_OURS(MapView_LinkPrimAt), 4, {kW, kW, 0xFF, 0xFF}, kG, 0, 0, {}, &FxLink, nullptr, true},
};
#undef E5A_OURS

// The sub-state tables the dispatchers jump through, read in place; each
// table's own length (symbols.toml [[data]], none bounded by a compare).
const sh::DataTable kTables[] = {
    {0x65DAE8, 4}, {0x65DB04, 5}, {0x65DB28, 5}, {0x65DB3C, 4}, {0x65DB78, 3}, {0x65DDC4, 4}, {0x65DDD4, 7}, {0x65DE78, 5},
};
const std::uint8_t kKinds[] = {0x18};

// Beyond effect mode's standard regions: the CLUT strip's first 16 rows (the
// rows sub-kinds 5 and 0x1F copy within: 3, 5..0xE).
const sh::Region kRegions[] = {
    {at::kClutStrip, 0x2000},
};

// --- the seed ------------------------------------------------------------------------

const sh::Clone* g_chosen = nullptr;
U g_base = 0;   // the clone being fuzzed (its base), for the disturbance

unsigned char* Rec(unsigned r) { return sh::EffectRecord(r); }
bool In(U lo, U hi) { return g_base >= lo && g_base <= hi; }

// The bounds every record keeps (the disturbance moves Sprite_Current among
// the 20 after a call, and the states index the image's tables by these):
// +0x36 / +0x3A / +0xB / +9 / +0xA for the sub-kind being fuzzed.
unsigned NineSpan() {
    if (In(0x4FD7E0, 0x4FDAA0)) return 0x24;        // sub-kind 5: 0x1E / 0x1F and the fifths
    if (In(0x4FDDD0, 0x4FDEB0)) return 0x10;        // sub-kind 0x1F
    if (In(0x4FDF20, 0x4FE1A0)) return 0x14;        // sub-kind 6: 0x10
    if (In(0x4FE330, 0x4FE640)) return 0x24;        // sub-kind 7: 0x20
    if (g_base == 0x4FE8A0) return at::kTopple8Count;
    if (g_base == 0x4FE920) return at::kCrash8Count;
    if (In(0x4FE750, 0x4FEC20)) return 0x10;        // sub-kind 8: 8
    if (In(0x4FEF50, 0x4FF150)) return 0x48;        // sub-kind 0xA: 0x30, 0x40, 4, 0x10
    return 0x100;
}
void Records() {
    const unsigned nine = NineSpan();
    for (unsigned r = 0; r < 20; ++r) {
        unsigned char* const e = Rec(r);
        unsigned sets = at::kRows;                   // sub-kind 5's rows (five)
        if (In(0x4FDF20, 0x4FE1A0)) sets = at::kSets6;
        if (In(0x4FE330, 0x4FE640)) sets = at::kDoors7;
        SetWord(e + 0x36, sh::Next() % sets);
        if (In(0x4FDF20, 0x4FE1A0)) SetWord(e + 0x3A, sh::Next() % at::kHeightCount6);
        e[0xB] = static_cast<unsigned char>(sh::Next() % at::kDoors7);
        e[9] = static_cast<unsigned char>(sh::Next() % nine);
        e[0xA] = static_cast<unsigned char>(sh::Next() % 0xC);
        if (g_base == 0x4FEDD0) e[2] = static_cast<unsigned char>(sh::Next() % 10);
    }
}

void Seed(unsigned k) {
    g_base = g_chosen[k].base;
    Records();
    unsigned char* const s = Sprite_Current;
    // the map's height base small (sub-kind 6 writes 4 * it past AreaMap_Header)
    SetWord(Mem(at::kHeightBase), sh::Next() % 0x600);
    Mem(0x905E20)[0] = static_cast<unsigned char>(PickOf(0, 1, 2, 3, sh::Next()));   // Cond_ByteFE
    // the boundaries each state compares against
    const unsigned nine = NineSpan();
    s[9] = static_cast<unsigned char>(PickOf(0, 4, 5, 8, 0x10, 0x1E, 0x1F, 0x20, 0x28, 0x30, 0x31, 0x40, 0x41, sh::Next()) % nine);
    s[0xA] = static_cast<unsigned char>(PickOf(0, 8, 9, 0xA, 1, sh::Next()) % 0xC);
    SetWord(s + 0x2E, PickOf(0xBE, 0xBF, 0xC0, 0, 0x7F, 0x80, 0xFFFF, 0x8000, sh::Next()));
    SetLong(s + 0xC, static_cast<std::int32_t>(PickOf(0x3E0, 0x3FF, 0x400, 0x401, 0, 0xFFFFFFFFu, 0x80000000u, sh::Next())));
    // (not over sub-kind 6's level word +0x3A, the dword's high half)
    if (!In(0x4FDF20, 0x4FE1A0))
        SetLong(s + 0x38, static_cast<std::int32_t>(PickOf(0x340000, 0x350000, 0x34E000, 0x342000, 0x352000, 0x33E000, sh::Next())));
    SetWord(s + 0x3E, PickOf(0xFE00, 0xFE40, 0xFF80, 0xFE80, 0x320, sh::Next()));
    if (!In(0x4FDF20, 0x4FE1A0) && sh::Half()) SetWord(s + 0x3A, sh::Next());
}

// The kCall clones' arguments: the CLUT copy inside the strip's 16 rows, the
// door's variant one of those its callers pass (and two more inside the table).
void Args(unsigned k, U* a) {
    const U base = g_chosen[k].base;
    if (base == 0x4FDB70) {
        a[0] = sh::Next() % 0xF;
        a[1] = sh::Next() % 0x10;
        a[2] = sh::Next() % 0xF;
        a[3] = sh::Next() % 0x10;
    } else if (base == 0x4FF150) {
        a[2] = PickOf(0, 1, 0x10, 0x11, 0x20, 0x30);
    }
}

// What the states read again after a call, moved by the group's case of the
// harness's disturbance (from its hash only): +9 inside the clone's span,
// +0xA, Cond_ByteFE, the nibble word +0x3A (sub-kind 5), +0xC, +0x3E.
void Disturb(U h) {
    const U v = h >> 8;
    unsigned char* const s = Sprite_Current;
    if (!sh::InRegions(s, 0x80)) return;
    switch (sh::DisturbCase(h, 6)) {
    case 0: s[9] = static_cast<unsigned char>(v % NineSpan()); break;
    case 1: s[0xA] = static_cast<unsigned char>(v % 0xC); break;
    case 2: Mem(0x905E20)[0] = static_cast<unsigned char>(v & 3); break;
    case 3:
        if (!In(0x4FDF20, 0x4FE1A0)) SetWord(s + 0x3A, v);
        break;
    case 4: SetLong(s + 0xC, static_cast<std::int32_t>((v & 1) ? 0x400u : v >> 1)); break;
    case 5: SetWord(s + 0x3E, (v & 1) ? 0xFE00u : v >> 1); break;
    default: break;
    }
}

}  // namespace

void SelfTest() {
    // BOF3X_E5A_ONLY: the clones whose name contains it (a control's run)
    static sh::Clone chosen[kCount];
    const char* const only = std::getenv("BOF3X_E5A_ONLY");
    unsigned n = 0;
    for (unsigned k = 0; k < kCount; ++k)
        if (!only || !*only || std::strstr(kAll[k].name, only)) chosen[n++] = kAll[k];
    if (n == 0) bof3::Fatal("effect_5a: BOF3X_E5A_ONLY=%s names no clone", only);
    g_chosen = chosen;
    const char* const rounds = std::getenv("BOF3X_E5A_ROUNDS");
    sh::Group g = {"effect_5a", chosen, n, kCallees, sizeof kCallees / sizeof kCallees[0], kTables,
                   sizeof kTables / sizeof kTables[0], kRegions, sizeof kRegions / sizeof kRegions[0],
                   &Seed, &Disturb,
                   rounds && *rounds ? static_cast<unsigned>(std::strtoul(rounds, nullptr, 0)) : 4000u};
    g.args = &Args;
    g.effect = true;
    g.kinds = kKinds;
    g.n_kinds = sizeof kKinds;
    sh::Run(g);
}

}  // namespace effect_5a

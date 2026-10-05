// BOF3X_SHADOW=effect_1a: group E1A's 70 functions through the scenario harness
// in effect mode (scenario_harness.h, docs/scenario_harness.md section 8), once
// at start-up. docs/effect_1a.md section 4. BOF3X_E1A_ONLY=<name> runs the
// clones whose name contains it (the controls' speed-up).
//
// The clone table is tools/band_rows.py --group E1A --byte-tables --clones
// --harness scenario (2026-09-29), each extent read again to its last
// instruction (capstone), names given, and the six dispatchers the band holds
// that no list of the cut has (0x463EE0, 0x464F20, 0x465310, 0x465A60,
// 0x466080, 0x4667A0) added with their tail jumps read by hand. 0x464350 opens
// with a jmp over eleven nops: its clone is the body from 0x464360 (the call
// offsets from there). Shapes: every handler without arguments kEffect (the
// record's +5 its kind, +1 below its table); the nine panel draws kCall.
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_1a.h"
#include "game/effect_1a_callees.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace effect_1a {
namespace {

namespace sh = scenario_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using U = std::uint32_t;

// tools/band_rows.py --group E1A --clones, 2026-09-29; the six dispatchers by hand.
constexpr sh::CallSite kCalls462B00[] = {{0x0, 0x462A90}};
constexpr sh::CallSite kCalls462B20[] = {{0x0, 0x462A90}};
constexpr sh::CallSite kCalls462B60[] = {{0xA, 0x415780}, {0xF, 0x41C190}};
constexpr sh::CallSite kCalls462BF0[] = {{0x70, 0x5A7840}, {0x86, 0x572FA0}, {0x92, 0x5A75D0}, {0xA0, 0x578EB0}, {0xFF, 0x5A8250}, {0x108, 0x5A92E0}, {0x14A, 0x5A79E0}, {0x163, 0x5A79A0}, {0x1AD, 0x5B9550}, {0x1D4, 0x5B9550}, {0x1EA, 0x5A77A0}, {0x200, 0x572FA0}, {0x22C, 0x5A7840}, {0x242, 0x572FA0}};
constexpr sh::CallSite kCalls462EB0[] = {{0x2, bof3::addr::EffectKind07_DrawSprite}};
// from 0x464360 (0x464350 less 0x10)
constexpr sh::CallSite kCalls464360[] = {{0x70, 0x5A7840}, {0x86, 0x572FA0}, {0x92, 0x5A75D0}, {0xA0, 0x578EB0}, {0x101, 0x5A8250}, {0x10D, 0x5A9110}, {0x153, 0x578EB0}, {0x1B7, 0x5A8250}, {0x1C3, 0x5A9110}, {0x1FB, 0x5A79E0}, {0x213, 0x5A79A0}, {0x24A, 0x5B9550}, {0x288, 0x5A77A0}, {0x29E, 0x572FA0}, {0x2D1, 0x5A7840}, {0x2E7, 0x572FA0}};
constexpr sh::CallSite kCalls464BA0[] = {{0x7, 0x52CF60}, {0x20, 0x52CFE0}, {0x29, 0x52CF60}, {0x34, 0x52CFE0}, {0x45, 0x52CFE0}, {0x71, 0x5B9380}, {0xB8, 0x516F60}, {0xC2, 0x464C70}};
constexpr sh::CallSite kCalls464C70[] = {{0x16, 0x464DA0}, {0x34, 0x5720C0}, {0x5A, 0x464E40}, {0x66, 0x5A75B0}, {0xCD, 0x461E50}, {0xF5, 0x464EC0}, {0x11E, 0x464EC0}};
constexpr sh::CallSite kCalls464DA0[] = {{0x8, 0x5A7610}, {0x88, 0x461E50}};
constexpr sh::CallSite kCalls464E40[] = {{0x9, 0x52CF60}, {0x15, 0x5A7730}, {0x67, 0x461E50}};
constexpr sh::CallSite kCalls464EC0[] = {{0x8, 0x5A7720}, {0x4D, 0x461E50}};
constexpr sh::CallSite kCalls464F80[] = {{0x119, 0x52CF60}, {0x12C, 0x52CFE0}, {0x15F, 0x465120}};
constexpr sh::CallSite kCalls465120[] = {{0xC, 0x5A7610}, {0x93, 0x461E50}, {0x9F, 0x5A7610}, {0x102, 0x461E50}};
constexpr sh::CallSite kCalls465230[] = {{0x41, 0x516B30}, {0x88, 0x4652D0}};
constexpr sh::CallSite kCalls4652D0[] = {{0xA, 0x52CF60}, {0x1C, 0x52CFE0}, {0x2E, 0x52CFE0}};
constexpr sh::CallSite kCalls465330[] = {{0x31, 0x5720C0}, {0x1B0, 0x589590}, {0x1B7, 0x5891F0}, {0x1BF, 0x589810}};
constexpr sh::CallSite kCalls465540[] = {{0x44, 0x587740}, {0x9B, 0x5891F0}, {0xA3, 0x588F20}, {0x152, 0x5893A0}, {0x157, 0x588F20}};
constexpr sh::CallSite kCalls4656A0[] = {{0x0, 0x589410}, {0xB, 0x5891F0}, {0x1B, 0x588F20}};
constexpr sh::CallSite kCalls4656C0[] = {{0xB5, 0x589810}, {0x154, 0x589330}, {0x15C, 0x5893A0}, {0x16B, 0x52CD50}};
constexpr sh::CallSite kCalls465840[] = {{0xF, 0x589330}, {0x70, 0x5893A0}, {0x75, 0x52CD50}};
constexpr sh::CallSite kCalls4658E0[] = {{0xAF, 0x4659A0}};
constexpr sh::CallSite kCalls4659A0[] = {{0xAE, 0x588F20}, {0xB3, 0x589840}};
constexpr sh::CallSite kCalls465AB0[] = {{0xB, 0x52CF60}, {0x1E, 0x52CFE0}, {0xEA, 0x5A7650}, {0x117, 0x461E50}, {0x1AB, 0x465E50}, {0x1D5, 0x5A7650}, {0x232, 0x461E50}, {0x27E, 0x465D90}, {0x2CB, 0x587740}};
constexpr sh::CallSite kCalls465D90[] = {{0x8, 0x5A75D0}, {0xB2, 0x461E50}};
constexpr sh::CallSite kCalls465E50[] = {{0xA, 0x52CF60}, {0x1C, 0x52CFE0}, {0x53, 0x52CF60}, {0x5F, 0x5A7740}, {0x67, 0x5A7780}, {0xB1, 0x461E50}};
constexpr sh::CallSite kCalls465F30[] = {{0x2, 0x589590}, {0x6A, 0x5891F0}, {0x8C, 0x587740}};
constexpr sh::CallSite kCalls465FE0[] = {{0x2F, 0x5890E0}};
constexpr sh::CallSite kCalls466020[] = {{0x1C, 0x5890E0}};
constexpr sh::CallSite kCalls466050[] = {{0x26, 0x5890E0}};
constexpr sh::CallSite kCalls466080[] = {{0x15, 0x589840}};
constexpr sh::CallSite kCalls4660D0[] = {{0x40, 0x469750}};
constexpr sh::CallSite kCalls466120[] = {{0x18, 0x589810}, {0x7D, 0x469750}};
constexpr sh::CallSite kCalls4661B0[] = {{0xD, 0x469750}, {0x29, 0x5171E0}, {0x38, 0x516B30}};
constexpr sh::CallSite kCalls466200[] = {{0x0, 0x466260}, {0x19, 0x5171E0}, {0x35, 0x516B30}};
constexpr sh::CallSite kCalls466240[] = {{0xD, 0x469750}};
constexpr sh::CallSite kCalls466260[] = {{0x3C, 0x469750}};
constexpr sh::CallSite kCalls466310[] = {{0x6A, 0x5171E0}, {0x92, 0x5171E0}, {0x10D, 0x469AD0}, {0x140, 0x516B30}};
constexpr sh::CallSite kCalls466460[] = {{0x56, 0x469AD0}, {0xB7, 0x589810}, {0x151, 0x516B30}, {0x177, 0x516B30}};
constexpr sh::CallSite kCalls4665E0[] = {{0x89, 0x469AD0}, {0xB3, 0x516B30}, {0x101, 0x516B30}, {0x141, 0x516B30}, {0x162, 0x589840}};
constexpr sh::CallSite kCalls466750[] = {{0x3E, 0x469AD0}, {0x47, 0x589840}};
constexpr sh::CallSite kCalls466840[] = {{0x58, 0x469750}, {0x74, 0x468AC0}, {0x82, 0x469750}, {0xAC, 0x468AC0}, {0xCF, 0x469210}, {0xF6, 0x468C50}, {0x107, 0x468F00}};
constexpr sh::CallSite kCalls466950[] = {{0xD, 0x469750}, {0x37, 0x468AC0}, {0x40, 0x469210}, {0x4C, 0x468C50}};
constexpr sh::CallSite kCalls4669B0[] = {{0xD, 0x469750}, {0x37, 0x468AC0}, {0x40, 0x469210}, {0x4C, 0x468F00}};
constexpr sh::CallSite kCalls466A10[] = {{0x34, 0x469750}, {0x50, 0x468AC0}, {0x5E, 0x469750}, {0x88, 0x468AC0}, {0xAB, 0x469210}, {0xD2, 0x468C50}, {0xE1, 0x468F00}};
constexpr sh::CallSite kCalls466B30[] = {{0x61, 0x469750}, {0x8B, 0x468AC0}};
constexpr sh::CallSite kCalls466BE0[] = {{0x26, 0x52D080}, {0x59, 0x52D0C0}, {0x90, 0x52D0C0}, {0xA6, 0x52D140}, {0xCB, 0x52D320}, {0xEF, 0x52D560}, {0x101, 0x469750}, {0x12B, 0x468AC0}};
constexpr sh::CallSite kCalls466D30[] = {{0x4, 0x52D080}, {0x27, 0x52D140}, {0x3D, 0x52D0C0}, {0x56, 0x52D140}, {0x67, 0x52D0C0}, {0x78, 0x52D560}, {0x8A, 0x52D320}, {0x8F, 0x468A40}, {0xA1, 0x469750}, {0xC7, 0x468AC0}, {0xD7, 0x468AC0}};
constexpr sh::CallSite kCalls466E10[] = {{0xC, 0x52D080}, {0x38, 0x52D140}, {0xD1, 0x52D0C0}, {0xE3, 0x52D320}, {0xF4, 0x52D560}, {0xF9, 0x468A40}, {0x10B, 0x469750}, {0x135, 0x468AC0}};
constexpr sh::CallSite kCalls466F70[] = {{0xC, 0x52D080}, {0x38, 0x52D140}, {0xD0, 0x52D0C0}, {0xE2, 0x52D320}, {0xF3, 0x52D560}, {0xF8, 0x468A40}, {0x10A, 0x469750}, {0x134, 0x468AC0}};
constexpr sh::CallSite kCalls4670C0[] = {{0x27, 0x469750}, {0x43, 0x468AC0}, {0x51, 0x469750}, {0x7B, 0x468AC0}, {0xAA, 0x52D080}, {0xDD, 0x52D0C0}, {0x114, 0x52D0C0}, {0x12A, 0x52D140}, {0x150, 0x52D320}, {0x175, 0x52D560}};
constexpr sh::CallSite kCalls467270[] = {{0xF, 0x469750}, {0x39, 0x468AC0}};

#define E1A_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define E1A_CALLS(a) a, E1A_N(a)
#define E1A_FN(name) reinterpret_cast<const void*>(&::name)
constexpr sh::Shape kEf = sh::Shape::kEffect;
constexpr sh::Shape kCa = sh::Shape::kCall;
// {name, base, size, calls, imms, tables, ours, ret_mask, calm, shape, pointers, state_span, sub_span, kind}
const sh::Clone kAll[] = {
    {"EffectKind0E_WorldMap", 0x462B00, 0x1A, E1A_CALLS(kCalls462B00), nullptr, 0, nullptr, 0, E1A_FN(EffectKind0E_WorldMap), 0, false, kEf, 0, 0, 0, 0xE},
    {"EffectKind16_WorldMap", 0x462B20, 0x1A, E1A_CALLS(kCalls462B20), nullptr, 0, nullptr, 0, E1A_FN(EffectKind16_WorldMap), 0, false, kEf, 0, 0, 0, 0x16},
    {"EffectKind5C_Run", 0x462B60, 0x14, E1A_CALLS(kCalls462B60), nullptr, 0, nullptr, 0, E1A_FN(EffectKind5C_Run), 0, false, kEf, 0, 0, 0, 0x5C},
    {"EffectKind01_Run", 0x462BA0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E1A_FN(EffectKind01_Run), 0, false, kEf, 0, 2, 0, 1},
    {"EffectKind01_Start", 0x462BC0, 0x23, nullptr, 0, nullptr, 0, nullptr, 0, E1A_FN(EffectKind01_Start), 0, false, kEf, 0, 2, 0, 1},
    {"EffectKind01_Draw", 0x462BF0, 0x252, E1A_CALLS(kCalls462BF0), nullptr, 0, nullptr, 0, E1A_FN(EffectKind01_Draw), 0, false, kEf, 0, 2, 0, 1},
    {"EffectKind07_Run", 0x462E50, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E1A_FN(EffectKind07_Run), 0, false, kEf, 0, 5, 0, 7},
    {"EffectKind07_Start", 0x462E70, 0x31, nullptr, 0, nullptr, 0, nullptr, 0, E1A_FN(EffectKind07_Start), 0, false, kEf, 0, 5, 0, 7},
    {"EffectKind07_FadeIn", 0x462EB0, 0x52, E1A_CALLS(kCalls462EB0), nullptr, 0, nullptr, 0, E1A_FN(EffectKind07_FadeIn), 0, false, kEf, 0, 5, 0, 7},
    {"EffectKind08_Run", 0x463040, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E1A_FN(EffectKind08_Run), 0, false, kEf, 0, 4, 0, 8},
    {"EffectKind09_Run", 0x463440, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E1A_FN(EffectKind09_Run), 0, false, kEf, 0, 10, 0, 9},
    {"EffectKind0B_Run", 0x463EE0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E1A_FN(EffectKind0B_Run), 0, false, kEf, 0, 5, 0, 0xB},
    {"EffectKind10_Run", 0x464360, 0x2F7, E1A_CALLS(kCalls464360), nullptr, 0, nullptr, 0, E1A_FN(EffectKind10_Run), 0, false, kEf, 0, 0, 0, 0x10},
    {"EffectKind02_Run", 0x464660, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E1A_FN(EffectKind02_Run), 0, false, kEf, 0, 5, 0, 2},
    {"EffectHud_Draw", 0x464BA0, 0xCE, E1A_CALLS(kCalls464BA0), nullptr, 0, nullptr, 0, E1A_FN(EffectHud_Draw), 0, false, kCa, 0, 0, 0, 2},
    {"EffectHud_DrawGauge", 0x464C70, 0x12B, E1A_CALLS(kCalls464C70), nullptr, 0, nullptr, 0, E1A_FN(EffectHud_DrawGauge), 0, false, kCa, 0, 0, 0, 2},
    {"EffectHud_Bar", 0x464DA0, 0x92, E1A_CALLS(kCalls464DA0), nullptr, 0, nullptr, 0, E1A_FN(EffectHud_Bar), 0, false, kCa, 0, 0, 0, 2},
    {"EffectHud_Marker", 0x464E40, 0x72, E1A_CALLS(kCalls464E40), nullptr, 0, nullptr, 0, E1A_FN(EffectHud_Marker), 0, false, kCa, 0, 0, 0, 2},
    {"EffectHud_Sprite8", 0x464EC0, 0x57, E1A_CALLS(kCalls464EC0), nullptr, 0, nullptr, 0, E1A_FN(EffectHud_Sprite8), 0, false, kCa, 0, 0, 0, 2},
    {"EffectKind03_Run", 0x464F20, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E1A_FN(EffectKind03_Run), 0, false, kEf, 0, 4, 0, 3},
    {"EffectKind03_Start", 0x464F40, 0x39, nullptr, 0, nullptr, 0, nullptr, 0, E1A_FN(EffectKind03_Start), 0, false, kEf, 0, 4, 0, 3},
    {"EffectKind03_Fade", 0x464F80, 0x192, E1A_CALLS(kCalls464F80), nullptr, 0, nullptr, 0, E1A_FN(EffectKind03_Fade), 0, false, kEf, 0, 4, 0, 3},
    {"EffectHud_TwoBars", 0x465120, 0x110, E1A_CALLS(kCalls465120), nullptr, 0, nullptr, 0, E1A_FN(EffectHud_TwoBars), 0, false, kCa, 0, 0, 0, 3},
    {"EffectKind03_ShowName", 0x465230, 0x92, E1A_CALLS(kCalls465230), nullptr, 0, nullptr, 0, E1A_FN(EffectKind03_ShowName), 0, false, kEf, 0, 4, 0, 3},
    {"EffectHud_DrawCount", 0x4652D0, 0x3A, E1A_CALLS(kCalls4652D0), nullptr, 0, nullptr, 0, E1A_FN(EffectHud_DrawCount), 0, false, kCa, 0, 0, 0, 3},
    {"EffectKind05_Run", 0x465310, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E1A_FN(EffectKind05_Run), 0, false, kEf, 0, 6, 0, 5},
    {"EffectKind05_Start", 0x465330, 0x201, E1A_CALLS(kCalls465330), nullptr, 0, nullptr, 0, E1A_FN(EffectKind05_Start), 0, false, kEf, 0, 6, 0, 5},
    {"EffectKind05_Rise", 0x465540, 0x15E, E1A_CALLS(kCalls465540), nullptr, 0, nullptr, 0, E1A_FN(EffectKind05_Rise), 0, false, kEf, 0, 6, 0, 5},
    {"EffectKind05_Land", 0x4656A0, 0x20, E1A_CALLS(kCalls4656A0), nullptr, 0, nullptr, 0, E1A_FN(EffectKind05_Land), 0, false, kEf, 0, 6, 0, 5},
    {"EffectKind05_Bounce", 0x4656C0, 0x174, E1A_CALLS(kCalls4656C0), nullptr, 0, nullptr, 0, E1A_FN(EffectKind05_Bounce), 0, false, kEf, 0, 6, 0, 5},
    {"EffectKind05_Follow", 0x465840, 0x7A, E1A_CALLS(kCalls465840), nullptr, 0, nullptr, 0, E1A_FN(EffectKind05_Follow), 0, false, kEf, 0, 6, 0, 5},
    {"EffectKind0A_Run", 0x4658C0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E1A_FN(EffectKind0A_Run), 0, false, kEf, 0, 2, 0, 0xA},
    {"EffectKind0A_Start", 0x4658E0, 0xBD, E1A_CALLS(kCalls4658E0), nullptr, 0, nullptr, 0, E1A_FN(EffectKind0A_Start), 0, false, kEf, 0, 2, 0, 0xA},
    {"EffectKind0A_Follow", 0x4659A0, 0xB8, E1A_CALLS(kCalls4659A0), nullptr, 0, nullptr, 0, E1A_FN(EffectKind0A_Follow), 0, false, kEf, 0, 2, 0, 0xA},
    {"EffectKind0C_Run", 0x465A60, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E1A_FN(EffectKind0C_Run), 0, false, kEf, 0, 3, 0, 0xC},
    {"EffectKind0C_Start", 0x465A80, 0x30, nullptr, 0, nullptr, 0, nullptr, 0, E1A_FN(EffectKind0C_Start), 0, false, kEf, 0, 3, 0, 0xC},
    {"EffectKind0C_Aim", 0x465AB0, 0x2DB, E1A_CALLS(kCalls465AB0), nullptr, 0, nullptr, 0, E1A_FN(EffectKind0C_Aim), 0, false, kEf, 0, 3, 0, 0xC},
    {"EffectHud_DrawArrow", 0x465D90, 0xBC, E1A_CALLS(kCalls465D90), nullptr, 0, nullptr, 0, E1A_FN(EffectHud_DrawArrow), 0, false, kCa, 0, 0, 0, 0xC},
    {"EffectHud_DrawMark", 0x465E50, 0xBE, E1A_CALLS(kCalls465E50), nullptr, 0, nullptr, 0, E1A_FN(EffectHud_DrawMark), 0, false, kCa, 0, 0, 0, 0xC},
    {"EffectKind0D_Run", 0x465F10, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E1A_FN(EffectKind0D_Run), 0, false, kEf, 0, 5, 0, 0xD},
    {"EffectKind0D_Start", 0x465F30, 0xA1, E1A_CALLS(kCalls465F30), nullptr, 0, nullptr, 0, E1A_FN(EffectKind0D_Start), 0, false, kEf, 0, 5, 0, 0xD},
    {"EffectKind0D_SlideIn", 0x465FE0, 0x34, E1A_CALLS(kCalls465FE0), nullptr, 0, nullptr, 0, E1A_FN(EffectKind0D_SlideIn), 0, false, kEf, 0, 5, 0, 0xD},
    {"EffectKind0D_Hold", 0x466020, 0x21, E1A_CALLS(kCalls466020), nullptr, 0, nullptr, 0, E1A_FN(EffectKind0D_Hold), 0, false, kEf, 0, 5, 0, 0xD},
    {"EffectKind0D_SlideOut", 0x466050, 0x2B, E1A_CALLS(kCalls466050), nullptr, 0, nullptr, 0, E1A_FN(EffectKind0D_SlideOut), 0, false, kEf, 0, 5, 0, 0xD},
    {"EffectKind0F_Run", 0x466080, 0x26, E1A_CALLS(kCalls466080), nullptr, 0, nullptr, 0, E1A_FN(EffectKind0F_Run), 0, false, kEf, 0, 13, 0, 0xF},
    {"EffectKind0F_Start", 0x4660B0, 0x1E, nullptr, 0, nullptr, 0, nullptr, 0, E1A_FN(EffectKind0F_Start), 0, false, kEf, 0, 13, 0, 0xF},
    {"EffectKind0F_Open", 0x4660D0, 0x49, E1A_CALLS(kCalls4660D0), nullptr, 0, nullptr, 0, E1A_FN(EffectKind0F_Open), 0, false, kEf, 0, 13, 0, 0xF},
    {"EffectKind0F_Choose", 0x466120, 0x86, E1A_CALLS(kCalls466120), nullptr, 0, nullptr, 0, E1A_FN(EffectKind0F_Choose), 0, false, kEf, 0, 13, 0, 0xF},
    {"EffectKind0F_Title", 0x4661B0, 0x41, E1A_CALLS(kCalls4661B0), nullptr, 0, nullptr, 0, E1A_FN(EffectKind0F_Title), 0, false, kEf, 0, 13, 0, 0xF},
    {"EffectKind0F_TitleClose", 0x466200, 0x3E, E1A_CALLS(kCalls466200), nullptr, 0, nullptr, 0, E1A_FN(EffectKind0F_TitleClose), 0, false, kEf, 0, 13, 0, 0xF},
    {"EffectKind0F_Frame", 0x466240, 0x16, E1A_CALLS(kCalls466240), nullptr, 0, nullptr, 0, E1A_FN(EffectKind0F_Frame), 0, false, kEf, 0, 13, 0, 0xF},
    {"EffectKind0F_Close", 0x466260, 0x45, E1A_CALLS(kCalls466260), nullptr, 0, nullptr, 0, E1A_FN(EffectKind0F_Close), 0, false, kEf, 0, 13, 0, 0xF},
    {"EffectKind0F_LineStart", 0x4662B0, 0x60, nullptr, 0, nullptr, 0, nullptr, 0, E1A_FN(EffectKind0F_LineStart), 0, false, kEf, 0, 13, 0, 0xF},
    {"EffectKind0F_LineType", 0x466310, 0x149, E1A_CALLS(kCalls466310), nullptr, 0, nullptr, 0, E1A_FN(EffectKind0F_LineType), 0, false, kEf, 0, 13, 0, 0xF},
    {"EffectKind0F_LineNext", 0x466460, 0x180, E1A_CALLS(kCalls466460), nullptr, 0, nullptr, 0, E1A_FN(EffectKind0F_LineNext), 0, false, kEf, 0, 13, 0, 0xF},
    {"EffectKind0F_LineScroll", 0x4665E0, 0x167, E1A_CALLS(kCalls4665E0), nullptr, 0, nullptr, 0, E1A_FN(EffectKind0F_LineScroll), 0, false, kEf, 0, 13, 0, 0xF},
    {"EffectKind0F_LineFade", 0x466750, 0x4C, E1A_CALLS(kCalls466750), nullptr, 0, nullptr, 0, E1A_FN(EffectKind0F_LineFade), 0, false, kEf, 0, 13, 0, 0xF},
    {"EffectKind1A_Run", 0x4667A0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E1A_FN(EffectKind1A_Run), 0, false, kEf, 0, 17, 0, 0x1A},
    {"EffectKind1A_Start", 0x4667C0, 0x7C, nullptr, 0, nullptr, 0, nullptr, 0, E1A_FN(EffectKind1A_Start), 0, false, kEf, 0, 17, 0, 0x1A},
    {"EffectKind1A_Open", 0x466840, 0x110, E1A_CALLS(kCalls466840), nullptr, 0, nullptr, 0, E1A_FN(EffectKind1A_Open), 0, false, kEf, 0, 17, 0, 0x1A},
    {"EffectKind1A_ItemsA", 0x466950, 0x55, E1A_CALLS(kCalls466950), nullptr, 0, nullptr, 0, E1A_FN(EffectKind1A_ItemsA), 0, false, kEf, 0, 17, 0, 0x1A},
    {"EffectKind1A_ItemsB", 0x4669B0, 0x55, E1A_CALLS(kCalls4669B0), nullptr, 0, nullptr, 0, E1A_FN(EffectKind1A_ItemsB), 0, false, kEf, 0, 17, 0, 0x1A},
    {"EffectKind1A_Close", 0x466A10, 0x116, E1A_CALLS(kCalls466A10), nullptr, 0, nullptr, 0, E1A_FN(EffectKind1A_Close), 0, false, kEf, 0, 17, 0, 0x1A},
    {"EffectKind1A_FindKind", 0x466B30, 0xA7, E1A_CALLS(kCalls466B30), nullptr, 0, nullptr, 0, E1A_FN(EffectKind1A_FindKind), 0, false, kEf, 0, 17, 0, 0x1A},
    {"EffectKind1A_PanelIn", 0x466BE0, 0x143, E1A_CALLS(kCalls466BE0), nullptr, 0, nullptr, 0, E1A_FN(EffectKind1A_PanelIn), 0, false, kEf, 0, 17, 0, 0x1A},
    {"EffectKind1A_Panel", 0x466D30, 0xE0, E1A_CALLS(kCalls466D30), nullptr, 0, nullptr, 0, E1A_FN(EffectKind1A_Panel), 0, false, kEf, 0, 17, 0, 0x1A},
    {"EffectKind1A_PanelNext", 0x466E10, 0x159, E1A_CALLS(kCalls466E10), nullptr, 0, nullptr, 0, E1A_FN(EffectKind1A_PanelNext), 0, false, kEf, 0, 17, 0, 0x1A},
    {"EffectKind1A_PanelBack", 0x466F70, 0x14D, E1A_CALLS(kCalls466F70), nullptr, 0, nullptr, 0, E1A_FN(EffectKind1A_PanelBack), 0, false, kEf, 0, 17, 0, 0x1A},
    {"EffectKind1A_PanelOut", 0x4670C0, 0x1A9, E1A_CALLS(kCalls4670C0), nullptr, 0, nullptr, 0, E1A_FN(EffectKind1A_PanelOut), 0, false, kEf, 0, 17, 0, 0x1A},
    {"EffectKind1A_Reset", 0x467270, 0x80, E1A_CALLS(kCalls467270), nullptr, 0, nullptr, 0, E1A_FN(EffectKind1A_Reset), 0, false, kEf, 0, 17, 0, 0x1A},
};
#undef E1A_FN
#undef E1A_CALLS
#undef E1A_N

enum : unsigned {
    k0EMap, k16Map, k5C, k01Run, k01Start, k01Draw, k07Run, k07Start, k07FadeIn, k08Run, k09Run, k0BRun, k10Run, k02Run,
    kHudDraw, kHudGauge, kHudBar, kHudMarker, kHudSprite8, k03Run, k03Start, k03Fade, kHudTwoBars, k03ShowName,
    kHudCount, k05Run, k05Start, k05Rise, k05Land, k05Bounce, k05Follow, k0ARun, k0AStart, k0AFollow, k0CRun,
    k0CStart, k0CAim, kHudArrow, kHudMark, k0DRun, k0DStart, k0DSlideIn, k0DHold, k0DSlideOut, k0FRun, k0FStart,
    k0FOpen, k0FChoose, k0FTitle, k0FTitleClose, k0FFrame, k0FClose, k0FLineStart, k0FLineType, k0FLineNext,
    k0FLineScroll, k0FLineFade, k1ARun, k1AStart, k1AOpen, k1AItemsA, k1AItemsB, k1AClose, k1AFindKind, k1APanelIn,
    k1APanel, k1APanelNext, k1APanelBack, k1APanelOut, k1AReset, kCount
};
static_assert(kCount == sizeof kAll / sizeof kAll[0], "one enum entry a clone, in order");

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
unsigned char* Mem(U a) { return sh::Mem(a); }
unsigned char* P(U a) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a)); }
template <typename... T> U PickOf(T... v) {
    const U values[] = {static_cast<U>(v)...};
    return sh::Pick(values, sizeof...(v));
}

// The parameter blocks the pointers 0x939A20 / 0x939A24 name, and the
// characters kind 0xF's cursors +0x50 / +0x54 walk: buffers of the fuzz's own,
// compared (random every round, then seeded).
alignas(16) unsigned char g_script[0x40];
alignas(16) unsigned char g_chars[0x80];

// --- the effects ---------------------------------------------------------------------

// The harness's packet buffer (scenario_harness.cpp g_packets, 0x800 bytes; its
// Drew keeps 0x40 to spare).
constexpr unsigned kPacketsSize = 0x800;
void Advance(unsigned n) {
    unsigned char* const next = Gfx_PacketNext;
    unsigned char* const base = sh::Packets();
    if (next >= base && next + n + 0x40 <= base + kPacketsSize) Gfx_PacketNext = next + n;
}
// MapView_LinkPrimAt (world_map.cpp): the cursor += size & 0xFF when the row is
// on the map - two times in three here (the row test's answer).
U FxLink(const U* a, U answer) {
    if (sh::Noise() % 3 != 0) Advance(a[3] & 0xFF);
    return answer;
}
// The caller's stack from just below this frame to its base (the TIB's
// StackBase): where the originals' and ours' locals are.
bool OnStack(const void* p, unsigned n) {
    std::uint32_t base;
    __asm__("movl %%fs:4, %0" : "=r"(base));
    const auto here = static_cast<U>(reinterpret_cast<std::uintptr_t>(&base));
    const auto at = static_cast<U>(reinterpret_cast<std::uintptr_t>(p));
    return at > here && at + n > at && at + n <= base;
}
// Gte_RotTransPers: the screen point as two floats (small whole numbers, as the
// harness's FillFloats) where the caller's primitive is, the depth cue's dword
// on the caller's stack.
U FxRotTransPers(const U* a, U answer) {
    unsigned char* const sxy = P(a[1]);
    if (sh::InRegions(sxy, 8))
        for (unsigned i = 0; i < 2; ++i) {
            const float f = static_cast<float>(static_cast<std::int16_t>(sh::Noise() >> 9));
            std::memcpy(sxy + 4 * i, &f, 4);
        }
    if (OnStack(P(a[2]), 4)) sh::FillBytes(P(a[2]), 4);
    return answer;
}
// 0x5171E0: the characters of a string - a small count (the whole eax, as the
// real one's) the callers compare with +0x49 (the stand-in's own; the real
// one counts to the NUL).
U FxCount(const U*, U) { return sh::Noise() % 20; }

#define E1A_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
constexpr sh::Answer kG = sh::Answer::kGarbage, kPh = sh::Answer::kPhase;
constexpr U kW = 0xFFFFFFFFu, k16 = 0xFFFFu, k8 = 0xFFu;
const sh::Callee kCallees[] = {
    // the group's own called directly, by name: every argument ours passes is
    // the original's to the bit, except where the original pushed a register
    // whose high bytes are a caller's leftover - masked to what the callee
    // reads (docs/effect_1a.md section 4)
    {E1A_OURS(EffectKind0A_Follow), 0, {}, kPh, 0, 0},
    {E1A_OURS(EffectKind0F_Close), 0, {}, kPh, 0, 0},
    {E1A_OURS(EffectHud_DrawGauge), 2, {kW, kW}, kG, 0, 0},
    {E1A_OURS(EffectHud_Bar), 3, {kW, kW, kW}, kG, 0, 0},
    {E1A_OURS(EffectHud_Marker), 3, {kW, kW, kW}, kG, 0, 0},
    {E1A_OURS(EffectHud_Sprite8), 3, {kW, kW, kW}, kG, 0, 0},
    {E1A_OURS(EffectHud_TwoBars), 4, {kW, kW, kW, kW}, kG, 0, 0},
    // x and n: `mov al, [esp + 0x34]` reads n's byte; x goes to 0x52CFE0 (s16)
    {E1A_OURS(EffectHud_DrawCount), 4, {k16, kW, kW, k8}, kG, 0, 0},
    // x movsx word, flip `mov al, [esp + 0x10]` (EffectKind0C_Aim pushes a stack
    // dword whose high bytes are stale), the slot to Gfx_CommitPrim
    {E1A_OURS(EffectHud_DrawArrow), 4, {k16, kW, k8, kW}, kG, 0, 0},
    // x to 0x52CFE0 and movsx; width and shade `mov bl` / `mov dl`; blink `mov al`
    {E1A_OURS(EffectHud_DrawMark), 6, {k16, kW, k8, k8, kW, k8}, kG, 0, 0},
    // E1F's (round thirteen), by address until it merges
    {"0x52CD50 (E1F)", at::kDepthPair, at::kDepthPair, 0, {}, kG, 0, 0},
    // E1B's (round thirteen), by address until it merges, read to their use of
    // the arguments: 0x469AD0's pen `mov cl, [esp + 8]; and ecx, 0xF`, the text
    // (its first two bytes), the width `mov al`, x a word
    {"0x469AD0 (E1B)", at::kMessageLine, at::kMessageLine, 4, {0xF, kW, k8, k16}, kG, 0, 0, {0, 2, 0, 0}, nullptr, nullptr, true},
    {"0x469210 (E1B)", at::kMemberRows, at::kMemberRows, 2, {kW, kW}, kG, 0, 0},
    {"0x468C50 (E1B)", at::kItemListA, at::kItemListA, 2, {kW, kW}, kG, 0, 0},
    {"0x468F00 (E1B)", at::kItemListB, at::kItemListB, 2, {kW, kW}, kG, 0, 0},
    {"0x468A40 (E1B)", at::kPanelTitle, at::kPanelTitle, 0, {}, kG, 0, 0},
    // standard entries re-listed: the widths the callees read (msgbox.cpp
    // Text_DrawAt: x and y to shorts, text_draw.cpp: the colour's and the
    // count's low bytes; field_e1.cpp FieldPanel_*: x and y to 0x52CFE0 as s16,
    // the kind, count and row bytes, the message id a word)
    {E1A_OURS(Text_DrawAt), 5, {k16, k16, k8, k8, kW}, kG, 0, 0, {0, 0, 0, 0, sh::kDerefString}, nullptr, nullptr, true},
    {E1A_OURS(FieldPanel_DrawHeader), 2, {k16, k16}, kG, 0, 0},
    {E1A_OURS(FieldPanel_DrawKindIcon), 3, {k16, k16, k8}, kG, 0, 0},
    {E1A_OURS(FieldPanel_DrawKindRow), 3, {k8, k8, k8}, kG, 0, 0},
    {E1A_OURS(FieldPanel_DrawTotal), 2, {k16, k16}, kG, 0, 0},
    {E1A_OURS(FieldPanel_DrawMessage), 3, {k16, k16, k16}, kG, 0, 0},
    // the vertex and the depth cue are the callers' stack (their addresses not
    // compared); the vertex's x, y and z hashed, not its fourth word (DIV-0023:
    // the original's is stale stack, ours 0) - as field_e2_fuzz.cpp re-lists it
    {E1A_OURS(Gte_RotTransPers), 3, {0, kW, 0}, kG, 0, 0, {6, 0, 0}, &FxRotTransPers, nullptr, true},
    // louder: the cursor the draws read again after it
    {E1A_OURS(MapView_LinkPrimAt), 4, {kW, kW, k8, k8}, kG, 0, 0, {}, &FxLink, nullptr, true},
    {"0x5171E0", at::kStringCount, at::kStringCount, 1, {0}, kG, 0, 0, {sh::kDerefString}, &FxCount, nullptr, true},
};
#undef E1A_OURS

// The tables the dispatchers jump through, read in place (lengths: symbols.toml
// [[data]], each checked by hand), and the world map records' +4 / +8 cells
// of the eleven records (index 11's are kind 1's and kind 7's tables).
const sh::DataTable kTables[] = {
    {at::kKind01States, 2}, {at::kKind07States, 5}, {at::kKind08States, 4}, {at::kKind09States, 10},
    {at::kKind0BStates, 5}, {at::kKind02States, 5}, {at::kKind03States, 4}, {at::kKind05States, 6},
    {at::kKind0AStates, 2}, {at::kKind0CStates, 3}, {at::kKind0DStates, 5}, {at::kKind0FStates, 13},
    {at::kKind1AStates, 17},
    {0x653914, 2}, {0x653930, 2}, {0x65394C, 2}, {0x653968, 2}, {0x653984, 2}, {0x6539A0, 2},
    {0x6539BC, 2}, {0x6539D8, 2}, {0x6539F4, 2}, {0x653A10, 2}, {0x653A2C, 2},
};

// Beyond effect mode's standard regions: the fuzz's parameter block and characters.
const sh::Region kRegions[] = {
    {0, sizeof g_script},
    {0, sizeof g_chars},
};
sh::Region g_regions[sizeof kRegions / sizeof kRegions[0]];

// --- the seed ------------------------------------------------------------------------

unsigned char* Rec(unsigned r) { return sh::EffectRecord(r); }
bool LineOk(unsigned text) { return P(at::kKind0FTexts + text * 8)[4] != 0xFF; }

// Every one of the 20 records the disturbance may move Sprite_Current among:
// what function k's code indexes kept inside its table.
void Records(unsigned k) {
    for (unsigned r = 0; r < at::kEffectCount; ++r) {
        unsigned char* const s = Rec(r);
        // kinds 1 and 0x10: the extra records +0xC / +0x18 name
        SetLong(s + 0x18, static_cast<std::int32_t>(sh::Next() % 4));
        if (k == k10Run) SetLong(s + 0xC, static_cast<std::int32_t>(sh::Next() % 4));
        // +6: kind 5's rows (4), kind 0xD's sounds (6), kind 0xF's rows - the
        // first seven: rows 7 and 8 of 0x653C04 name texts 13 and 14, past the
        // thirteen records (docs/effect_1a.md section 6)
        s[6] = static_cast<unsigned char>(sh::Next() % (k >= k0DRun && k <= k0DSlideOut ? 6 : k >= k0FRun && k <= k0FLineFade ? 7 : 4));
        // kind 0xF: a text +0x4B (a line in LineNext / LineFade's wait: they
        // index the label tables unchecked), a column +0x4A of the row that
        // names a text (LineStart reads it: an 0xFF there is past the texts),
        // the cursors
        unsigned text = sh::Next() % at::kKind0FTextCount;
        if ((k == k0FLineNext || k == k0FLineFade) && !LineOk(text)) text = 0;
        s[0x4B] = static_cast<unsigned char>(text);
        unsigned column = sh::Next() % 3;
        if (k == k0FLineStart && P(at::kKind0FRows + s[6] * 4u + column)[0] >= at::kKind0FTextCount) column = 0;
        s[0x4A] = static_cast<unsigned char>(column);
        SetLong(s + 0x50, static_cast<std::int32_t>(Key(g_chars + sh::Next() % 0x40)));
        SetLong(s + 0x54, static_cast<std::int32_t>(Key(g_chars + sh::Next() % 0x40)));
        // kind 0x1A: the kind word +0x3E among the 32 counts (the original reads
        // any; kept near them so the counts matter)
        if (k >= k1ARun) SetWord(s + 0x3E, sh::Half() ? sh::Next() % 0x20 : sh::Next());
    }
}

void Seed(unsigned k) {
    Records(k);
    unsigned char* const s = static_cast<unsigned char*>(Sprite_Current);
    unsigned char* const r0 = Mem(at::kRecord0);
    // the parameter blocks
    sh::SetPointer(at::kPanelScript, g_script);
    sh::SetPointer(at::kPanelSet, g_script + 0x20);
    g_script[1] = static_cast<unsigned char>(PickOf(0, 1, 2, 3, sh::Next()));
    g_script[3] = static_cast<unsigned char>(PickOf(0, 1, 2, 3, 4, sh::Next()));
    Mem(at::kPanelObject)[0] = static_cast<unsigned char>(sh::Half() ? sh::Next() % 4 : sh::Next() % at::kObjectCount);
    // the shared cells: record 0's state and height, record 3's state, the
    // leader's bytes, the kind-2 hold, the area
    r0[1] = static_cast<unsigned char>(PickOf(0, 4, 1, sh::Next()));
    SetWord(r0 + 0x3E, PickOf(0, 1, 0xFFFF, 0x8000, 0x20, 0xFFE0, sh::Next()));
    Mem(at::kRecord3 + 1)[0] = static_cast<unsigned char>(PickOf(2, 3, 4, 7, sh::Next()));
    Mem(at::kLeader + 2)[0] = static_cast<unsigned char>(PickOf(3, 4, sh::Next()));
    Mem(at::kLeader + 3)[0] = static_cast<unsigned char>(PickOf(2, 3, 4, sh::Next()));
    Field_Kind2Hold = static_cast<unsigned char>(PickOf(0, 0, sh::Next()));
    Game_AreaNumber = static_cast<unsigned short>(PickOf(0x68, 0x79, sh::Next()));
    // the kind counts: none, some, all, or one (the last half the time: the
    // walk's wrap at 0x20)
    const U counts = PickOf(0, 1, 2, 2, 3);
    for (unsigned i = 0; i < 0x20; ++i)
        Mem(at::kKindCounts + i)[0] = static_cast<unsigned char>(counts == 0 || counts == 3 ? 0 : counts == 1 ? (sh::Half() ? 0 : sh::Next()) : sh::Next() | 1);
    if (counts == 3) Mem(at::kKindCounts + (sh::Half() ? 0x1F : sh::Next() % 0x20))[0] = static_cast<unsigned char>(sh::Next() | 1);
    // the small counters the states step and compare
    s[9] = static_cast<unsigned char>(PickOf(0, 1, 2, 4, 5, 6, 0xF, 0xE, sh::Next()));
    s[0xA] = static_cast<unsigned char>(PickOf(0, 1, 2, sh::Next()));
    s[0xB] = static_cast<unsigned char>(PickOf(0, 1, sh::Next()));
    s[7] = static_cast<unsigned char>(PickOf(0, 1, 0x80, 0x81, sh::Next()));
    s[8] = static_cast<unsigned char>(PickOf(0, 1, sh::Next()));
    switch (k) {
    case k07FadeIn: s[0x5D] = static_cast<unsigned char>(PickOf(0x7E, 0x7E, 0x7F, 0x80, sh::Next())); break;
    case k03Fade:
        SetLong(s + 0x18, static_cast<std::int32_t>(PickOf(0, 1, 0xFFFFFFFFu, 8, 0xFFFFFFF8u, 0x100, sh::Next())));
        SetLong(s + 0xC, static_cast<std::int32_t>(PickOf(0, 1, 0xFE, 0xFF, 0x100, 0xFFFFFFFFu, sh::Next())));
        SetLong(s + 0x10, static_cast<std::int32_t>(PickOf(0, 1, 0xFFFFFFFFu, 0xFFFFFFF8u, 8, sh::Next())));
        break;
    case k03ShowName:
        r0[7] = static_cast<unsigned char>(g_script[0x2F] + PickOf(0, 1, 2, 4, 5, 0xFF, sh::Next()));
        break;
    case k05Rise:
        if (sh::Half()) SetLong(s + 0x38, Long(s + 0x10));
        else if (sh::Half()) SetLong(s + 0x38, static_cast<std::int32_t>(static_cast<U>(Long(s + 0x10)) + static_cast<U>(Long(P(at::kKind05Bounce + s[6] * 8)))));
        if (sh::Half()) SetLong(s + 0x38, 0x3C4000);
        SetLong(s + 0x14, static_cast<std::int32_t>(PickOf(0, 1, 0xFFFFFFFFu, sh::Next())));
        break;
    case k05Bounce: {
        SetLong(s + 0x20, static_cast<std::int32_t>(PickOf(0, 0, 1, 0xFFFFFFFFu, sh::Next())));
        SetLong(s + 0x14, static_cast<std::int32_t>(PickOf(0, 1, 0xFFFFFFFFu, 0x100, sh::Next())));
        // the height after its adds exactly 0, one either side, or any
        const U add = static_cast<U>(Long(s + 0x14)) + static_cast<U>(Long(s + 0x20));
        SetLong(s + 0x3C, static_cast<std::int32_t>(PickOf(0, 1, 0xFFFFFFFFu, sh::Next()) - add));
        s[7] = static_cast<unsigned char>(g_script[0x2F] + PickOf(0, 1, 0xFF, sh::Next()));
        SetWord(s + 0x3E, PickOf(0, 1, 0xFFFF, sh::Next()));
        break;
    }
    case k05Follow: SetLong(Mem(at::kRecord5 + 0x10), static_cast<std::int32_t>(PickOf(0, 1, 0xFFFFFFFFu, sh::Next()))); break;
    case k0AStart:
    case k0AFollow: r0[1] = static_cast<unsigned char>(PickOf(0, 1, sh::Next())); break;
    case k0CAim: {
        SetLong(s + 0x10, static_cast<std::int32_t>(PickOf(0, 0, 1, 0xFFFFFFFFu, sh::Next())));
        unsigned char* const o = Mem(at::kObjects + Mem(at::kPanelObject)[0] * at::kObjectStride);
        SetWord(s + 0x30, sh::Next() % 0x40 - 0x20);
        const unsigned margin = g_script[3] * 4u + 0xC;
        o[0xA] = static_cast<unsigned char>(Word(s + 0x30) + PickOf(0, margin, margin + 1, 0u - margin, 0u - margin - 1, sh::Next()));
        o[1] = static_cast<unsigned char>(PickOf(4, 4, sh::Next()));
        o[4] = static_cast<unsigned char>(PickOf(3, sh::Next()));
        break;
    }
    case k0DSlideIn:
    case k0DSlideOut: SetWord(s + 0x2E, PickOf(0xBE, 0xBF, 0xC0, 0xFFD8, 0xFFD7, 0xFFD9, sh::Next())); break;
    case k0FRun:
        if (sh::Half()) Sprite_Current = Mem(at::kRecord3);
        break;
    case k0FOpen:
    case k0FClose: SetWord(s + 0x30, PickOf(0xA, 0xB, 0xC, 0xFFEF, 0xFFF0, 0xFFF1, sh::Next())); break;
    case k0FLineType:
    case k0FLineNext:
    case k0FLineScroll:
        s[0x49] = static_cast<unsigned char>(PickOf(0, 1, 2, 5, sh::Next()));
        SetWord(s + 0x2E, PickOf(0x1D, 0x1E, 0x1F, 0x20, sh::Next()));
        break;
    default: break;
    }
    // record 0's or record 3's state may be the current record's: the byte a
    // dispatcher indexes by stays inside its table
    unsigned char* const cur = static_cast<unsigned char*>(Sprite_Current);
    if (kAll[k].state_span) cur[1] = static_cast<unsigned char>(cur[1] % kAll[k].state_span);
}

// The panel draws' arguments: coordinates in a word over random high bytes, a
// slot, and the bytes they read.
void Args(unsigned k, U* a) {
    switch (k) {
    case kHudDraw:
        a[0] = sh::Next() % 0x140;
        a[1] = sh::Next() % 0xF0;
        break;
    case kHudGauge:
        // y near the s16 sign as well: the marker's compare is a word's
        a[0] = sh::Next() % 0x140;
        a[1] = PickOf(sh::Next() % 0xF0, 0x7C00 + sh::Next() % 0x400, 0x8000 + sh::Next() % 0x400, sh::Next());
        break;
    case kHudBar:
    case kHudMarker:
    case kHudSprite8:
        a[2] = sh::Next() % 8;
        break;
    case kHudTwoBars: a[3] = sh::Next() % 8; break;
    case kHudCount:
        a[2] = sh::Next() % 8;
        a[3] = PickOf(1, 2, 3, 4, sh::Next());
        break;
    case kHudArrow:
        a[2] = PickOf(0, 1, 0x100, sh::Next());
        a[3] = sh::Next() % 8;
        break;
    case kHudMark:
        a[4] = sh::Next() % 8;
        a[5] = PickOf(0, 1, 0x100, sh::Next());
        break;
    default: break;
    }
}

// What the functions read again after a call, moved by the group's case of the
// harness's disturbance (from its hash only) and kept inside what the code
// indexes: the counters +9 / +0xA / +0xB, +0x49, the text +0x4B, the word
// +0x3E, record 0's and record 3's state, the leader's bytes, 0x939A1C.
void Disturb(U h) {
    const U v = h >> 8;
    unsigned char* const s = static_cast<unsigned char*>(Sprite_Current);
    if (!sh::InRegions(s, 0x80)) return;
    switch (h % 8) {
    case 0: s[9] = static_cast<unsigned char>(v); break;
    case 1: s[0xA] = static_cast<unsigned char>(v % 3); break;
    case 2: s[0x49] = static_cast<unsigned char>(v); break;
    case 3: SetWord(s + 0x3E, v % 0x20); break;
    case 4:
        if (s != Mem(at::kRecord0)) Mem(at::kRecord0 + 1)[0] = static_cast<unsigned char>(v);
        break;
    case 5: Mem(at::kLeader + 2 + (v & 1))[0] = static_cast<unsigned char>(v >> 1); break;
    case 6: Mem(at::kPanelObject)[0] = static_cast<unsigned char>(v % at::kObjectCount); break;
    case 7:
        if (s != Mem(at::kRecord3)) Mem(at::kRecord3 + 1)[0] = static_cast<unsigned char>(v);
        break;
    default: break;
    }
}

}  // namespace

void SelfTest() {
    // BOF3X_E1A_ONLY: the clones whose name contains it (a control's run)
    static sh::Clone chosen[kCount];
    static unsigned index[kCount];
    const char* const only = std::getenv("BOF3X_E1A_ONLY");
    unsigned n = 0;
    for (unsigned k = 0; k < kCount; ++k)
        if (!only || !*only || std::strstr(kAll[k].name, only)) {
            index[n] = k;
            chosen[n++] = kAll[k];
        }
    if (n == 0) bof3::Fatal("effect_1a: BOF3X_E1A_ONLY=%s names no clone", only);
    static unsigned* s_index = index;
    for (unsigned i = 0; i < sizeof kRegions / sizeof kRegions[0]; ++i) g_regions[i] = kRegions[i];
    g_regions[0].at = Key(g_script);
    g_regions[1].at = Key(g_chars);
    static const std::uint8_t kKinds[] = {1, 2, 3, 5, 7, 8, 9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF, 0x10, 0x16, 0x1A, 0x5C};
    sh::Group g = {"effect_1a", chosen, n, kCallees, sizeof kCallees / sizeof kCallees[0], kTables,
                   sizeof kTables / sizeof kTables[0], g_regions, sizeof g_regions / sizeof g_regions[0],
                   [](unsigned k) { Seed(s_index[k]); }, &Disturb, 3000};
    g.args = [](unsigned k, U* a) { Args(s_index[k], a); };
    g.effect = true;
    g.kinds = kKinds;
    g.n_kinds = sizeof kKinds;
    sh::Run(g);
}

}  // namespace effect_1a

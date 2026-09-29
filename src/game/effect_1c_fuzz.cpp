// BOF3X_SHADOW=effect_1c: group E1C's 53 functions through the scenario harness
// in effect mode (scenario_harness.h, docs/scenario_harness.md section 8), once
// at start-up. docs/effect_1c.md section 4. BOF3X_E1C_ONLY=<name> runs the
// clones whose name contains it (the controls' speed-up).
//
// The clone table is tools/band_rows.py --group E1C --clones --harness
// scenario (2026-09-29), each extent read again to its last instruction
// (capstone), names given, and 0x46F230 (code no list has, EffectKind20_Run's
// tail) kept as its own clone. Shapes: every state handler, dispatcher and
// helper without arguments kEffect (Sprite_Current one of the 20
// Effect_Objects records, +5 its kind, +1 below its own table's length); the
// seven with arguments kCall.
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_1c.h"
#include "game/effect_1c_callees.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace effect_1c {
namespace {

namespace sh = scenario_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using U = std::uint32_t;

// tools/band_rows.py --group E1C --clones --harness scenario, 2026-09-29.
constexpr sh::CallSite kCalls46A850[] = {{0x87, 0x5A7810}, {0xB9, 0x461E50}, {0xCD, 0x589840}};
constexpr sh::CallSite kCalls46D780[] = {{0x1C, 0x409480}, {0x29, 0x41A0A0}, {0x36, 0x41A410}, {0x43, 0x426810}, {0x50, 0x4276D0}};
constexpr sh::CallSite kCalls46D8B0[] = {{0x26, 0x5720C0}, {0x5B, 0x46E120}, {0x6D, 0x587740}};
constexpr sh::CallSite kCalls46D930[] = {{0x9, 0x46DA20}, {0x1A, 0x471D10}, {0x1F, 0x46DD00}};
constexpr sh::CallSite kCalls46D980[] = {{0x5, 0x46DA20}, {0x16, 0x471D10}, {0x1B, 0x46DD00}};
constexpr sh::CallSite kCalls46D9D0[] = {{0x9, 0x46DA20}, {0x11, 0x46DD00}};
constexpr sh::CallSite kCalls46DA10[] = {{0x0, 0x46DD00}, {0x9, 0x589840}};
constexpr sh::CallSite kCalls46DA20[] = {{0x7, 0x494060}, {0xF, 0x5A7A50}, {0x2F, 0x5A7A00}, {0x5F, 0x494110}, {0x83, 0x494110}, {0xE9, 0x5A7A50}, {0x109, 0x5A7A00}, {0x139, 0x494110}, {0x15D, 0x494110}, {0x172, 0x5A79A0}, {0x18B, 0x5A77C0}, {0x19E, 0x572FA0}, {0x1AA, 0x5A7610}, {0x1B2, 0x5A7780}, {0x27E, 0x572FA0}, {0x293, 0x5A79A0}, {0x2AB, 0x5A77C0}, {0x2BE, 0x572FA0}};
constexpr sh::CallSite kCalls46DD00[] = {{0x27, 0x471E20}};
constexpr sh::CallSite kCalls46DD70[] = {{0x21, 0x46E120}};
constexpr sh::CallSite kCalls46DDA0[] = {{0x9, 0x46DE80}, {0x1A, 0x471D10}, {0x1F, 0x46E140}};
constexpr sh::CallSite kCalls46DDF0[] = {{0x5, 0x46DE80}, {0x16, 0x471D10}, {0x1B, 0x46E140}};
constexpr sh::CallSite kCalls46DE30[] = {{0x9, 0x46DE80}, {0x11, 0x46E140}};
constexpr sh::CallSite kCalls46DE70[] = {{0x0, 0x46E140}, {0x9, 0x589840}};
constexpr sh::CallSite kCalls46DE80[] = {{0x7, 0x494060}, {0xF, 0x5A7A50}, {0x2F, 0x5A7A00}, {0x5F, 0x494110}, {0x83, 0x494110}, {0xE6, 0x5A7A50}, {0x106, 0x5A7A00}, {0x136, 0x494110}, {0x15A, 0x494110}, {0x16F, 0x5A79A0}, {0x188, 0x5A77C0}, {0x19E, 0x461E50}, {0x1AD, 0x5A7610}, {0x1B5, 0x5A7780}, {0x27D, 0x461E50}};
constexpr sh::CallSite kCalls46E140[] = {{0x27, 0x46E190}};
constexpr sh::CallSite kCalls46E220[] = {{0x0, 0x46EA60}, {0x5, 0x46E2E0}, {0xA, 0x46E6E0}};
constexpr sh::CallSite kCalls46E250[] = {{0x9, 0x46EC20}, {0x30, 0x587740}, {0x3A, 0x587740}, {0x44, 0x587740}};
constexpr sh::CallSite kCalls46E2B0[] = {{0x0, 0x46EBA0}, {0x5, 0x46E830}, {0xA, 0x46E3B0}};
constexpr sh::CallSite kCalls46E2E0[] = {{0x16, 0x46E320}};
constexpr sh::CallSite kCalls46E320[] = {{0x2C, 0x5B93D2}, {0x3D, 0x5B93D2}, {0x4F, 0x5B93D2}, {0x5C, 0x5A8B60}};
constexpr sh::CallSite kCalls46E3B0[] = {{0x1, 0x494060}, {0x30, 0x46E400}};
constexpr sh::CallSite kCalls46E400[] = {{0xD, 0x5A75D0}, {0x15, 0x5A7780}, {0x27, 0x494110}, {0x45, 0x4941E0}, {0x198, 0x5A79E0}, {0x1AF, 0x5A79A0}, {0x1D5, 0x572FA0}};
constexpr sh::CallSite kCalls46E6E0[] = {{0xA3, 0x5A8C00}};
constexpr sh::CallSite kCalls46E830[] = {{0x41, 0x46E890}};
constexpr sh::CallSite kCalls46E890[] = {{0xD, 0x5B93D2}, {0x1A, 0x5B93D2}, {0x28, 0x5B93D2}, {0x3C, 0x5A8060}, {0x54, 0x5A8E00}, {0x5E, 0x5A8DE0}, {0x91, 0x5A8200}, {0xEA, 0x5A8200}, {0x13F, 0x5A8200}, {0x197, 0x5A8200}};
constexpr sh::CallSite kCalls46EA60[] = {{0xD, 0x46EA80}};
constexpr sh::CallSite kCalls46EA80[] = {{0x2D, 0x5B93D2}, {0x3A, 0x5B93D2}, {0x47, 0x5B93D2}, {0x56, 0x5A7A50}, {0x61, 0x5A7A00}, {0x72, 0x5A7A50}, {0x7D, 0x5A7A00}, {0x91, 0x494180}, {0x9F, 0x5A7F10}, {0xAF, 0x5A7F80}, {0xBD, 0x5A7FF0}, {0xC2, 0x5A7B90}, {0xD1, 0x5A7C70}, {0xE0, 0x5A7C70}, {0xE8, 0x5A7BC0}, {0xED, 0x5B93D2}};
constexpr sh::CallSite kCalls46EBA0[] = {{0x12, 0x5A79A0}, {0x2A, 0x5A77C0}, {0x33, 0x461E50}, {0x3B, 0x494060}, {0x4E, 0x485030}};
constexpr sh::CallSite kCalls46EC20[] = {{0x19, 0x5A7B90}, {0x2D, 0x57BFF0}, {0x37, 0x5A8DE0}, {0x41, 0x5A8E00}, {0x5A, 0x57C070}, {0x72, 0x5A79A0}, {0x8A, 0x5A77C0}, {0x93, 0x461E50}, {0xBC, 0x5A7690}, {0xC4, 0x5A7780}, {0x18E, 0x5A85F0}, {0x199, 0x46EE20}, {0x1AC, 0x46EE20}, {0x1C9, 0x5A9170}, {0x1D2, 0x461E50}, {0x1EA, 0x5A7BC0}};
constexpr sh::CallSite kCalls46EEB0[] = {{0xD, 0x4851E0}};
constexpr sh::CallSite kCalls46EEE0[] = {{0x12, 0x5A79A0}, {0x2A, 0x5A77C0}, {0x33, 0x461E50}, {0x3B, 0x494060}, {0x4E, 0x485030}};
constexpr sh::CallSite kCalls46EF70[] = {{0x1F, 0x46F230}};
constexpr sh::CallSite kCalls46F100[] = {{0x1C, 0x494060}, {0x45, 0x494110}, {0x62, 0x494110}, {0x81, 0x5A7A70}};
constexpr sh::CallSite kCalls46F1C0[] = {{0x65, 0x589840}};
constexpr sh::CallSite kCalls46F230[] = {{0x13, 0x5A79A0}, {0x2B, 0x5A77C0}, {0x34, 0x461E50}, {0x6E, 0x493090}};

#define E1C_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define E1C_CALLS(a) a, E1C_N(a)
#define E1C_FN(name) reinterpret_cast<const void*>(&::name)
constexpr sh::Shape kE = sh::Shape::kEffect;
constexpr sh::Shape kC = sh::Shape::kCall;
constexpr U kS3 = sh::ArgAt(0, sh::Arg::kScratch) | sh::ArgAt(1, sh::Arg::kScratch) | sh::ArgAt(2, sh::Arg::kScratch);
// {name, base, size, calls, imms, tables, ours, ret_mask, calm, shape, pointers, state_span, sub_span, kind}
const sh::Clone kAll[] = {
    {"EffectKind36_Run", 0x46A850, 0xDF, E1C_CALLS(kCalls46A850), nullptr, 0, nullptr, 0, E1C_FN(EffectKind36_Run), 0, false, kE, 0, 4, 0, 0x36},
    {"EffectKind3C_Run", 0x46A930, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E1C_FN(EffectKind3C_Run), 0, false, kE, 0, 3, 0, 0x3C},
    {"EffectKind70_Run", 0x46D780, 0xA5, E1C_CALLS(kCalls46D780), nullptr, 0, nullptr, 0, E1C_FN(EffectKind70_Run), 0, false, kE, 0, 1, 0, 0x70},
    {"EffectKind1C_Run", 0x46D890, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E1C_FN(EffectKind1C_Run), 0, false, kE, 0, 5, 0, 0x1C},
    {"EffectKind1C_Start", 0x46D8B0, 0x76, E1C_CALLS(kCalls46D8B0), nullptr, 0, nullptr, 0, E1C_FN(EffectKind1C_Start), 0, false, kE, 0, 5, 0, 0x1C},
    {"EffectKind1C_Rise", 0x46D930, 0x4B, E1C_CALLS(kCalls46D930), nullptr, 0, nullptr, 0, E1C_FN(EffectKind1C_Rise), 0, false, kE, 0, 5, 0, 0x1C},
    {"EffectKind1C_Hold", 0x46D980, 0x46, E1C_CALLS(kCalls46D980), nullptr, 0, nullptr, 0, E1C_FN(EffectKind1C_Hold), 0, false, kE, 0, 5, 0, 0x1C},
    {"EffectKind1C_Fade", 0x46D9D0, 0x34, E1C_CALLS(kCalls46D9D0), nullptr, 0, nullptr, 0, E1C_FN(EffectKind1C_Fade), 0, false, kE, 0, 5, 0, 0x1C},
    {"EffectKind1C_End", 0x46DA10, 0xF, E1C_CALLS(kCalls46DA10), nullptr, 0, nullptr, 0, E1C_FN(EffectKind1C_End), 0, false, kE, 0, 5, 0, 0x1C},
    {"EffectKind1C_DrawRing", 0x46DA20, 0x2D9, E1C_CALLS(kCalls46DA20), nullptr, 0, nullptr, 0, E1C_FN(EffectKind1C_DrawRing), 0, false, kC, 0, 5, 0, 0x1C},
    {"EffectKind1C_MoveShards", 0x46DD00, 0x4B, E1C_CALLS(kCalls46DD00), nullptr, 0, nullptr, 0, E1C_FN(EffectKind1C_MoveShards), 0xFF, false, kE, 0, 5, 0, 0x1C},
    {"EffectKind1D_Run", 0x46DD50, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E1C_FN(EffectKind1D_Run), 0, false, kE, 0, 5, 0, 0x1D},
    {"EffectKind1D_Start", 0x46DD70, 0x2F, E1C_CALLS(kCalls46DD70), nullptr, 0, nullptr, 0, E1C_FN(EffectKind1D_Start), 0, false, kE, 0, 5, 0, 0x1D},
    {"EffectKind1D_Rise", 0x46DDA0, 0x4B, E1C_CALLS(kCalls46DDA0), nullptr, 0, nullptr, 0, E1C_FN(EffectKind1D_Rise), 0, false, kE, 0, 5, 0, 0x1D},
    {"EffectKind1D_Hold", 0x46DDF0, 0x3B, E1C_CALLS(kCalls46DDF0), nullptr, 0, nullptr, 0, E1C_FN(EffectKind1D_Hold), 0, false, kE, 0, 5, 0, 0x1D},
    {"EffectKind1D_Fade", 0x46DE30, 0x34, E1C_CALLS(kCalls46DE30), nullptr, 0, nullptr, 0, E1C_FN(EffectKind1D_Fade), 0, false, kE, 0, 5, 0, 0x1D},
    {"EffectKind1D_End", 0x46DE70, 0xF, E1C_CALLS(kCalls46DE70), nullptr, 0, nullptr, 0, E1C_FN(EffectKind1D_End), 0, false, kE, 0, 5, 0, 0x1D},
    {"EffectKind1D_DrawRing", 0x46DE80, 0x298, E1C_CALLS(kCalls46DE80), nullptr, 0, nullptr, 0, E1C_FN(EffectKind1D_DrawRing), 0, false, kC, 0, 5, 0, 0x1D},
    {"EffectShards_Clear", 0x46E120, 0x14, nullptr, 0, nullptr, 0, nullptr, 0, E1C_FN(EffectShards_Clear), 0, false, kE, 0, 5, 0, 0x1C},
    {"EffectKind1D_MoveShards", 0x46E140, 0x4B, E1C_CALLS(kCalls46E140), nullptr, 0, nullptr, 0, E1C_FN(EffectKind1D_MoveShards), 0xFF, false, kE, 0, 5, 0, 0x1D},
    {"EffectKind1E_Run", 0x46E200, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E1C_FN(EffectKind1E_Run), 0, false, kE, 0, 5, 0, 0x1E},
    {"EffectKind1E_Start", 0x46E220, 0x21, E1C_CALLS(kCalls46E220), nullptr, 0, nullptr, 0, E1C_FN(EffectKind1E_Start), 0, false, kE, 0, 5, 0, 0x1E},
    {"EffectKind1E_Glow", 0x46E250, 0x5E, E1C_CALLS(kCalls46E250), nullptr, 0, nullptr, 0, E1C_FN(EffectKind1E_Glow), 0, false, kE, 0, 5, 0, 0x1E},
    {"EffectKind1E_Burst", 0x46E2B0, 0x1C, E1C_CALLS(kCalls46E2B0), nullptr, 0, nullptr, 0, E1C_FN(EffectKind1E_Burst), 0, false, kE, 0, 5, 0, 0x1E},
    {"EffectKind1E_FreeModel", 0x46E2D0, 0x10, nullptr, 0, nullptr, 0, nullptr, 0, E1C_FN(EffectKind1E_FreeModel), 0, false, kE, 0, 5, 0, 0x1E},
    {"EffectKind1E_ShardsInit", 0x46E2E0, 0x32, E1C_CALLS(kCalls46E2E0), nullptr, 0, nullptr, 0, E1C_FN(EffectKind1E_ShardsInit), 0, false, kE, 0, 5, 0, 0x1E},
    {"EffectKind1E_ShardInit", 0x46E320, 0x84, E1C_CALLS(kCalls46E320), nullptr, 0, nullptr, 0, E1C_FN(EffectKind1E_ShardInit), 0, false, kC, 0, 5, 0, 0x1E},
    {"EffectKind1E_ShardsDraw", 0x46E3B0, 0x4E, E1C_CALLS(kCalls46E3B0), nullptr, 0, nullptr, 0, E1C_FN(EffectKind1E_ShardsDraw), 0xFF, false, kE, 0, 5, 0, 0x1E},
    {"EffectKind1E_ShardQuad", 0x46E400, 0x1E4, E1C_CALLS(kCalls46E400), nullptr, 0, nullptr, 0, E1C_FN(EffectKind1E_ShardQuad), 0, false, kC, 0, 5, 0, 0x1E},
    {"EffectKind1E_ShardFly", 0x46E5F0, 0x67, nullptr, 0, nullptr, 0, nullptr, 0, E1C_FN(EffectKind1E_ShardFly), 0, false, kE, 0, 5, 0, 0x1E},
    {"EffectKind1E_ShardFade", 0x46E660, 0x5A, nullptr, 0, nullptr, 0, nullptr, 0, E1C_FN(EffectKind1E_ShardFade), 0, false, kE, 0, 5, 0, 0x1E},
    {"EffectKind1E_ShardNext2", 0x46E6C0, 0x9, nullptr, 0, nullptr, 0, nullptr, 0, E1C_FN(EffectKind1E_ShardNext2), 0, false, kE, 0, 5, 0, 0x1E},
    {"EffectKind1E_ShardNext3", 0x46E6D0, 0x9, nullptr, 0, nullptr, 0, nullptr, 0, E1C_FN(EffectKind1E_ShardNext3), 0, false, kE, 0, 5, 0, 0x1E},
    {"EffectKind1E_SplitModel", 0x46E6E0, 0x14F, E1C_CALLS(kCalls46E6E0), nullptr, 0, nullptr, 0, E1C_FN(EffectKind1E_SplitModel), 0, false, kE, 0, 5, 0, 0x1E},
    {"EffectKind1E_StepPieces", 0x46E830, 0x5A, E1C_CALLS(kCalls46E830), nullptr, 0, nullptr, 0, E1C_FN(EffectKind1E_StepPieces), 0, false, kE, 0, 5, 0, 0x1E},
    {"EffectKind1E_TurnPiece", 0x46E890, 0x1CC, E1C_CALLS(kCalls46E890), nullptr, 0, nullptr, 0, E1C_FN(EffectKind1E_TurnPiece), 0, false, kC, 0, 5, 0, 0x1E},
    {"EffectKind1E_DebrisInit", 0x46EA60, 0x1E, E1C_CALLS(kCalls46EA60), nullptr, 0, nullptr, 0, E1C_FN(EffectKind1E_DebrisInit), 0, false, kE, 0, 5, 0, 0x1E},
    {"EffectKind1E_DebrisInitOne", 0x46EA80, 0x116, E1C_CALLS(kCalls46EA80), nullptr, 0, nullptr, 0, E1C_FN(EffectKind1E_DebrisInitOne), 0, false, kC, 0, 5, 0, 0x1E},
    {"EffectKind1E_DebrisDraw", 0x46EBA0, 0x7A, E1C_CALLS(kCalls46EBA0), nullptr, 0, nullptr, 0, E1C_FN(EffectKind1E_DebrisDraw), 0, false, kE, 0, 5, 0, 0x1E},
    {"EffectKind1E_DrawModel", 0x46EC20, 0x200, E1C_CALLS(kCalls46EC20), nullptr, 0, nullptr, 0, E1C_FN(EffectKind1E_DrawModel), 0, false, kC, 0, 5, 0, 0x1E},
    {"EffectKind1E_Winding", 0x46EE20, 0x62, nullptr, 0, nullptr, 0, nullptr, 0, E1C_FN(EffectKind1E_Winding), 0xFFFF, false, kC, kS3, 5, 0, 0x1E},
    {"EffectKind1F_Run", 0x46EE90, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E1C_FN(EffectKind1F_Run), 0, false, kE, 0, 3, 0, 0x1F},
    {"EffectKind1F_Start", 0x46EEB0, 0x2F, E1C_CALLS(kCalls46EEB0), nullptr, 0, nullptr, 0, E1C_FN(EffectKind1F_Start), 0, false, kE, 0, 3, 0, 0x1F},
    {"EffectKind1F_Debris", 0x46EEE0, 0x8F, E1C_CALLS(kCalls46EEE0), nullptr, 0, nullptr, 0, E1C_FN(EffectKind1F_Debris), 0, false, kE, 0, 3, 0, 0x1F},
    {"EffectKind20_Run", 0x46EF70, 0x25, E1C_CALLS(kCalls46EF70), nullptr, 0, nullptr, 0, E1C_FN(EffectKind20_Run), 0, false, kE, 0, 7, 0, 0x20},
    {"EffectKind20_Start", 0x46EFA0, 0x31, nullptr, 0, nullptr, 0, nullptr, 0, E1C_FN(EffectKind20_Start), 0, false, kE, 0, 7, 0, 0x20},
    {"EffectKind20_Grow", 0x46EFE0, 0x43, nullptr, 0, nullptr, 0, nullptr, 0, E1C_FN(EffectKind20_Grow), 0, false, kE, 0, 7, 0, 0x20},
    {"EffectKind20_Wait", 0x46F030, 0x24, nullptr, 0, nullptr, 0, nullptr, 0, E1C_FN(EffectKind20_Wait), 0, false, kE, 0, 7, 0, 0x20},
    {"EffectKind20_Rise", 0x46F060, 0x51, nullptr, 0, nullptr, 0, nullptr, 0, E1C_FN(EffectKind20_Rise), 0, false, kE, 0, 7, 0, 0x20},
    {"EffectKind20_Shrink", 0x46F0C0, 0x40, nullptr, 0, nullptr, 0, nullptr, 0, E1C_FN(EffectKind20_Shrink), 0, false, kE, 0, 7, 0, 0x20},
    {"EffectKind20_Aim", 0x46F100, 0xBB, E1C_CALLS(kCalls46F100), nullptr, 0, nullptr, 0, E1C_FN(EffectKind20_Aim), 0, false, kE, 0, 7, 0, 0x20},
    {"EffectKind20_Fall", 0x46F1C0, 0x6B, E1C_CALLS(kCalls46F1C0), nullptr, 0, nullptr, 0, E1C_FN(EffectKind20_Fall), 0, false, kE, 0, 7, 0, 0x20},
    {"EffectKind20_Draw", 0x46F230, 0x77, E1C_CALLS(kCalls46F230), nullptr, 0, nullptr, 0, E1C_FN(EffectKind20_Draw), 0, false, kE, 0, 7, 0, 0x20},
};
#undef E1C_FN
#undef E1C_CALLS
#undef E1C_N

enum : unsigned {
    k36Run, k3CRun, k70Run, k1CRun, k1CStart, k1CRise, k1CHold, k1CFade, k1CEnd, k1CRing, k1CMove, k1DRun, k1DStart,
    k1DRise, k1DHold, k1DFade, k1DEnd, k1DRing, kClear, k1DMove, k1ERun, k1EStart, k1EGlow, k1EBurst, k1EFree,
    k1EShardsInit, k1EShardInit, k1EShardsDraw, k1EShardQuad, k1EFly, k1EFade, k1ENext2, k1ENext3, k1ESplit, k1EStep,
    k1ETurn, k1EDebrisInit, k1EDebrisOne, k1EDebrisDraw, k1EModel, k1EWinding, k1FRun, k1FStart, k1FDebris, k20Run,
    k20Start, k20Grow, k20Wait, k20Rise, k20Shrink, k20Aim, k20Fall, k20Draw, kCount
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

// The model Sprite_ObjectsExtra[0] +0x50 points at (27 faces of 0x28, read,
// split and rewritten) and the count byte its +0x54 points at: buffers of the
// fuzz's own, compared.
alignas(16) unsigned char g_model[0x440];
alignas(16) unsigned char g_count[4];

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
void Fill(U at, unsigned n) {
    if (Writable(at, n)) sh::FillBytes(P(at), n);
}
// A float with a random mantissa and an exponent 2^-17..2^18 (no NaN, no
// infinity): what the projections' screen words and depths are.
void FillFloat(U at) {
    if (!Writable(at, 4)) return;
    const U n = sh::Noise();
    const U bits = (n & 0x807FFFFFu) | ((0x6Eu + (sh::Noise() % 0x24u)) << 23);
    std::memcpy(P(at), &bits, 4);
}
// EffectGte_ProjectPoint: out[0..2] the screen x, y and depth (floats); every caller reads all three.
U FxProjectPoint(const U* a, U answer) {
    for (unsigned i = 0; i < 3; ++i) FillFloat(a[1] + 4 * i);
    return answer;
}
// EffectGte_ProjectSize: out's two s16 (the size's, in place, in EffectKind1E_ShardQuad).
U FxProjectSize(const U* a, U answer) {
    Fill(a[2], 4);
    return answer;
}
// Gte_RotTrans: the three longs of out.
U FxRotTrans(const U* a, U answer) {
    Fill(a[1], 12);
    return answer;
}
// Gte_RotTransPers4: four screen points of two floats, the depth cue word.
U FxPers4(const U* a, U answer) {
    for (unsigned i = 4; i < 8; ++i) {
        FillFloat(a[i]);
        FillFloat(a[i] + 4);
    }
    Fill(a[8], 4);
    return answer;
}
// Gte_SetTransMatrix reads the MATRIX's translation, +0x14..+0x1F: noted (the
// rotation hashed by the deref; the padding word +0x12, the callers' leftover
// stack, neither).
U FxSetTrans(const U* a, U answer) {
    if (Writable(a[0], 0x20)) sh::NoteBytes(P(a[0] + 0x14), 12);
    return answer;
}
// 0x471D10 (E2A's speck spawn): the first free speck (+0 == 0) of the 0x80 filled
// and marked in use, as the real one writes +0..+0xF; al its index, 0x80 none.
U FxShardSpawn(const U*, U answer) {
    for (U r = 0; r < at::kShardCount; ++r) {
        unsigned char* const s = P(at::kShards + r * at::kShardStride);
        if (s[0]) continue;
        sh::FillBytes(s, 0x10);
        s[0] = 1;
        return (answer & 0xFFFFFF00u) | r;
    }
    return (answer & 0xFFFFFF00u) | 0x80u;
}
// EffectKind1E_Winding: ax 0, 1 or -1 (each compare's boundary) three times in
// four, else any; the upper half the answer's.
U FxWinding(const U*, U answer) {
    const U n = sh::Noise();
    const U w = n % 4 == 3 ? answer : (n >> 2) % 3 == 0 ? 0u : (n >> 2) % 3 == 1 ? 1u : 0xFFFFu;
    return (answer & 0xFFFF0000u) | (w & 0xFFFFu);
}
// 0x4851E0 (E3C's debris set-up): the record's 0x2C bytes written.
U FxDebrisInit(const U* a, U answer) {
    Fill(a[0], 0x2C);
    return answer;
}

#define E1C_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
constexpr sh::Answer kG = sh::Answer::kGarbage, kF = sh::Answer::kFlag, kPh = sh::Answer::kPhase;
constexpr U kW = 0xFFFFFFFFu, k16 = 0xFFFFu, k8 = 0xFFu;
const sh::Callee kCallees[] = {
    // the group's own called directly, by name
    {E1C_OURS(EffectKind1C_DrawRing), 1, {k8}, kG, 0, 0},   // the brightness byte (push ecx: the upper bytes the caller's)
    {E1C_OURS(EffectKind1D_DrawRing), 1, {k8}, kG, 0, 0},
    {E1C_OURS(EffectKind1C_MoveShards), 0, {}, kF, 0, 0},
    {E1C_OURS(EffectKind1D_MoveShards), 0, {}, kF, 0, 0},
    {E1C_OURS(EffectShards_Clear), 0, {}, kPh, 0, 0},
    {E1C_OURS(EffectKind1E_DebrisInit), 0, {}, kPh, 0, 0},
    {E1C_OURS(EffectKind1E_ShardsInit), 0, {}, kPh, 0, 0},
    {E1C_OURS(EffectKind1E_SplitModel), 0, {}, kPh, 0, 0},
    {E1C_OURS(EffectKind1E_DebrisDraw), 0, {}, kPh, 0, 0},
    {E1C_OURS(EffectKind1E_StepPieces), 0, {}, kPh, 0, 0},
    {E1C_OURS(EffectKind1E_ShardsDraw), 0, {}, kF, 0, 0},
    {E1C_OURS(EffectKind1E_DrawModel), 1, {k8}, kG, 0, 0},   // the shade byte (push ecx)
    {E1C_OURS(EffectKind1E_ShardInit), 1, {kW}, kG, 0, 0},
    {E1C_OURS(EffectKind1E_ShardQuad), 1, {kW}, kG, 0, 0},
    {E1C_OURS(EffectKind1E_TurnPiece), 3, {kW, kW, kW}, kG, 0, 0},
    {E1C_OURS(EffectKind1E_DebrisInitOne), 1, {kW}, kG, 0, 0},
    {E1C_OURS(EffectKind1E_Winding), 3, {kW, kW, kW}, kG, 0, 0, {}, &FxWinding},   // the caller tests ax against 0
    {E1C_OURS(EffectKind20_Draw), 0, {}, kPh, 0, 0},
    // other groups' of round thirteen, by address (docs/effect_1c.md section 6)
    {"0x471D10 (E2A)", at::kShardSpawn, at::kShardSpawn, 0, {}, kG, 0, 0, {}, &FxShardSpawn},
    {"0x471E20 (E2A)", at::kShardTile, at::kShardTile, 1, {kW}, kG, 0, 0, {0x10}, nullptr, nullptr, true},
    {"0x485030 (E3C)", at::kDebrisDraw, at::kDebrisDraw, 1, {kW}, kG, 0, 0, {0x2C}, nullptr, nullptr, true},
    {"0x4851E0 (E3C)", at::kDebrisInit, at::kDebrisInit, 1, {kW}, kG, 0, 0, {}, &FxDebrisInit},
    // the point copied on the stack (12 read), three words, the shade and flag bytes
    {"0x493090 (E4F)", at::kCone, at::kCone, 6, {0, k16, k16, k16, k8, k8}, kG, 0, 0, {12}, nullptr, nullptr, true},
    // standard entries re-listed louder: the stack pointers never logged, the
    // outs filled, the SVECTORs' padding (the callers' leftovers) not hashed
    {E1C_OURS(EffectGte_ProjectPoint), 2, {0, 0}, kG, 0, 0, {12, 0}, &FxProjectPoint, nullptr, true},
    {E1C_OURS(EffectGte_ProjectSize), 3, {0, 0, 0}, kG, 0, 0, {12, 4, 0}, &FxProjectSize, nullptr, true},
    {E1C_OURS(Gte_RotTrans), 2, {0, 0}, kG, 0, 0, {6, 0}, &FxRotTrans, nullptr, true},
    {E1C_OURS(Gte_RotTransPers4), 9, {0, 0, 0, 0, 0, 0, 0, 0, 0}, kG, 0, 0, {6, 6, 6, 6, 0, 0, 0, 0, 0}, &FxPers4, nullptr, true},
    {E1C_OURS(Gte_SetTransMatrix), 1, {0}, kG, 0, 0, {18}, &FxSetTrans, nullptr, true},
    {E1C_OURS(Gte_SetRotMatrix), 1, {0}, kG, 0, 0, {18}, nullptr, nullptr, true},
};
#undef E1C_OURS

// The tables the dispatchers go through, read in place: each its own length
// (the run of code pointers to the next table a dispatcher names; none bounded).
const sh::DataTable kTables[] = {
    {0x653F98, 3},    // EffectKind3C_States
    {0x654210, 5},    // EffectKind1C_States
    {0x654224, 5},    // EffectKind1D_States
    {0x654238, 5},    // EffectKind1E_States
    {0x65424C, 4},    // EffectKind1E_ShardStates
    {0x65425C, 3},    // EffectKind1F_States
    {0x654268, 7},    // EffectKind20_States
};

// Beyond effect mode's standard regions: the rest of EffectKind30_Shards' 0x80
// specks past the standard 0x644 (with kind 0x1E's pieces 0x92C0C0.. and debris
// 0x92C780..0x92CA40 after), the shard cursor, the fuzz's model and count.
const sh::Region kRegions[] = {
    {at::kShards + 0x644, at::kDebris + 16 * 0x2C - (at::kShards + 0x644)},
    {at::kShardCursor, 4},
    {0, sizeof g_model},
    {0, sizeof g_count},
};
sh::Region g_regions[sizeof kRegions / sizeof kRegions[0]];

// --- the seed ------------------------------------------------------------------------

unsigned char* Shard28(unsigned i) { return P(at::kShards + 0x28 * (i % 8)); }

// Kind 0x1C's / 0x1D's specks: a third in use, the speed +2 small, the height
// +0xC after the rise at, one either side of, or anywhere about the record's
// +0x3C (the free test's boundary).
void Specks(const unsigned char* s) {
    for (U r = 0; r < at::kShardCount; ++r) {
        unsigned char* const k = P(at::kShards + r * at::kShardStride);
        k[0] = static_cast<unsigned char>(sh::Next() % 3 == 0 ? 1 + sh::Next() % 0xFF : 0);
        SetWord(k + 2, PickOf(0, 1, 0x1000, 0x1FFF, sh::Next()));
        const U rise = static_cast<U>(static_cast<int>(static_cast<short>(Word(k + 2)))) << 8;
        SetLong(k + 0xC, static_cast<std::int32_t>(static_cast<U>(Long(s + 0x3C)) + rise + PickOf(0, 1, 0xFFFFFFFFu, sh::Next())));
    }
}

// Kind 0x1E's eight shards: in use half the time, their state +1 inside
// EffectKind1E_ShardStates' four, their count +2 at 1 or 2 (the steps' zero) or any.
void Shards28() {
    for (unsigned i = 0; i < 8; ++i) {
        unsigned char* const k = Shard28(i);
        k[0] = static_cast<unsigned char>(sh::Half() ? 1 : 0);
        k[1] = static_cast<unsigned char>(sh::Next() % 4);
        k[2] = static_cast<unsigned char>(PickOf(1, 1, 2, 0, sh::Next()));
    }
    move_script::SetLong(Mem(at::kShardCursor), static_cast<std::int32_t>(Key(Shard28(sh::Next()))));
}

void Seed(unsigned k) {
    unsigned char* const s = Sprite_Current;
    // what every kind may read: the model and its count, the shard cursor, the counter byte
    SetLong(Mem(at::kModelFaces), static_cast<std::int32_t>(Key(g_model)));
    SetLong(Mem(at::kModelCount), static_cast<std::int32_t>(Key(g_count + sh::Next() % 4)));
    for (unsigned i = 0; i < 4; ++i) g_count[i] = static_cast<unsigned char>(PickOf(0, 1, 2, 27, sh::Next() % 28));
    move_script::SetLong(Mem(at::kShardCursor), static_cast<std::int32_t>(Key(Shard28(sh::Next()))));
    Mem(0x903848)[0] = static_cast<unsigned char>(PickOf(0x1F, 0x1F, 0x1E, 0x20, sh::Next()));
    s[9] = static_cast<unsigned char>(PickOf(0, 0, 1, 2, 4, 0xFC, 0xFF, 0x20, 0x7E, 0x80, 0x44, 0x45, 3, sh::Next()));
    switch (k) {
    case k36Run: s[9] = static_cast<unsigned char>(PickOf(0, 0, 1, sh::Next())); break;
    case k70Run: {
        Field_ScriptFlags2 = static_cast<unsigned short>(sh::Half() ? Field_ScriptFlags2 & ~0x400u : Field_ScriptFlags2 | 0x400u);
        Game_AreaNumber = static_cast<unsigned short>(PickOf(0x31, 0x75, 0x76, 0xA9, 0xAB, sh::Next() % 0x100));
        // up to eight members; a set bit past the three writes past ObjTrio in the original (ours aborts), so kept clear
        const unsigned n = PickOf(0, 1, 2, 3, 3, 4 + sh::Next() % 5);
        Field_MemberCount = static_cast<unsigned char>(n);
        // (every record's: the hooks' recorders may move Sprite_Current among them)
        if (n > 3)
            for (unsigned r = 0; r < sh::at::kEffectCount; ++r) sh::EffectRecord(r)[0xB] &= 7u;
        break;
    }
    case k1CMove:
    case k1DMove:
    case k1CEnd:
    case k1DEnd: Specks(s); break;
    case k1EShardsDraw:
    case k1EFly:
    case k1EFade:
    case k1ENext2:
    case k1ENext3: Shards28(); break;
    case k20Grow:
    case k20Wait:
    case k20Rise:
    case k20Shrink:
    case k20Aim:
    case k20Fall:
        SetWord(s + 0x2C, PickOf(1, 1, 2, 0, sh::Next()));
        SetLong(s + 0x64, static_cast<std::int32_t>(PickOf(13, 14, 15, 8, 0, sh::Next())));
        break;
    default: break;
    }
}

void Args(unsigned k, U* a) {
    const U i = sh::Next();
    switch (k) {
    case k1EShardInit:
    case k1EShardQuad: a[0] = Key(Shard28(i)); break;
    case k1ETurn:
        a[0] = at::kPieceCopies + 0x28 * (i % at::kPieceCount);
        a[1] = Key(g_model) + 0x28 * (i % at::kPieceCount);
        a[2] = at::kPieces + 0x18 * (i % at::kPieceCount);
        break;
    case k1EDebrisOne: a[0] = at::kDebris + 0x2C * (i % 16); break;
    default: break;
    }
}

// What the functions read again after a call, moved by the group's case of the
// harness's disturbance (from its hash only): the frame count +9, the timer
// +0x2C, a shard's in-use byte, state and
// count, a speck's in-use byte, the counter byte 0x903848, +0x3C.
void Disturb(U h) {
    const U v = h >> 8;
    unsigned char* const s = Sprite_Current;
    switch (h % 7) {
    case 0: s[9] = static_cast<unsigned char>((v & 1) ? (v >> 1) % 3 * 0x44 : v >> 3); break;
    case 1: SetWord(s + 0x2C, (v & 1) ? 1 : v >> 1); break;
    // (not the cursor itself: a state handler that moved it would send EffectKind1E_ShardsDraw's walk past
    // the eight shards, into bytes whose +1 indexes past the table - the original does not move it)
    case 2: Shard28(v)[0] = static_cast<unsigned char>(v >> 3); break;
    case 3: {
        unsigned char* const k = Shard28(v);
        k[1] = static_cast<unsigned char>((v >> 3) % 4);
        k[2] = static_cast<unsigned char>(v >> 5);
        break;
    }
    case 4: P(at::kShards + (v % at::kShardCount) * at::kShardStride)[0] = static_cast<unsigned char>(v >> 7); break;
    case 5: Mem(0x903848)[0] = static_cast<unsigned char>((v & 1) ? 0x1F : v >> 1); break;
    case 6: SetLong(s + 0x3C, static_cast<std::int32_t>(v)); break;
    default: break;
    }
}

}  // namespace

void SelfTest() {
    // BOF3X_E1C_ONLY: the clones whose name contains it (a control's run)
    static sh::Clone chosen[kCount];
    static unsigned index[kCount];
    const char* const only = std::getenv("BOF3X_E1C_ONLY");
    unsigned n = 0;
    for (unsigned k = 0; k < kCount; ++k)
        if (!only || !*only || std::strstr(kAll[k].name, only)) {
            index[n] = k;
            chosen[n++] = kAll[k];
        }
    if (n == 0) bof3::Fatal("effect_1c: BOF3X_E1C_ONLY=%s names no clone", only);
    static unsigned* s_index = index;
    for (unsigned i = 0; i < sizeof kRegions / sizeof kRegions[0]; ++i) g_regions[i] = kRegions[i];
    g_regions[2].at = Key(g_model);
    g_regions[3].at = Key(g_count);
    static const std::uint8_t kKinds[] = {0x36, 0x3C, 0x70, 0x1C, 0x1D, 0x1E, 0x1F, 0x20};
    sh::Group g = {"effect_1c", chosen, n, kCallees, sizeof kCallees / sizeof kCallees[0], kTables,
                   sizeof kTables / sizeof kTables[0], g_regions, sizeof g_regions / sizeof g_regions[0],
                   [](unsigned k) { Seed(s_index[k]); }, &Disturb, 4000};
    g.args = [](unsigned k, U* a) { Args(s_index[k], a); };
    g.effect = true;
    g.kinds = kKinds;
    g.n_kinds = sizeof kKinds;
    sh::Run(g);
}

}  // namespace effect_1c

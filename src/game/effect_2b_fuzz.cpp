// BOF3X_SHADOW=effect_2b: group E2B's 52 functions through the scenario harness
// in effect mode (scenario_harness.h, docs/scenario_harness.md section 8), once
// at start-up. docs/effect_2b.md section 4. BOF3X_E2B_ONLY=<name> runs the
// clones whose name contains it (the controls' speed-up).
//
// The clone table is tools/band_rows.py --group E2B --clones --harness
// scenario (2026-09-29), each extent read again to its last instruction
// (capstone), names given, and 0x473F10 (kind 0x38's tile, the tail of
// 0x473EC0 that 0x473E30 and 0x473E90 jump to) added as its own clone.
// 0x473EC0's clone keeps the tool's extent 0xD6, its tail included. Shapes:
// every state handler, dispatcher and helper without arguments kEffect
// (Sprite_Current one of the 20 Effect_Objects records, +5 its kind, +1 below
// its own table's length); the thirteen with arguments kCall.
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_2b.h"
#include "game/effect_2b_callees.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace effect_2b {
namespace {

namespace sh = scenario_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using U = std::uint32_t;

// tools/band_rows.py --group E2B --clones --harness scenario, 2026-09-29.
constexpr sh::CallSite kCalls4731C0[] = {{0x3F, 0x473460}, {0x5B, 0x587740}};
constexpr sh::CallSite kCalls473240[] = {{0x3B, 0x4732D0}, {0x4A, 0x473360}, {0x81, 0x589840}};
constexpr sh::CallSite kCalls4732D0[] = {{0x36, 0x5A7A50}, {0x51, 0x5A7A00}, {0x70, 0x494060}, {0x7E, 0x494110}};
constexpr sh::CallSite kCalls473360[] = {{0x14, 0x5A79A0}, {0x2A, 0x5A77C0}, {0x33, 0x461E50}, {0x56, 0x5A76B0}, {0x5E, 0x5A7780}, {0xCF, 0x461E50}};
constexpr sh::CallSite kCalls473460[] = {{0x11, 0x5A7A50}, {0x2C, 0x5A7A00}, {0x4B, 0x494060}, {0x59, 0x494110}};
constexpr sh::CallSite kCalls473550[] = {{0xE, 0x4735B0}, {0x42, 0x587740}};
constexpr sh::CallSite kCalls4735B0[] = {{0x4, 0x494060}, {0x2E, 0x473600}};
constexpr sh::CallSite kCalls473600[] = {{0x11, 0x494110}, {0x21, 0x5A7A50}, {0x39, 0x5A7A00}, {0x5E, 0x494110}, {0x7E, 0x5A79A0}, {0x97, 0x5A77C0}, {0xB3, 0x572FA0}, {0xBF, 0x5A7570}, {0xC7, 0x5A7780}, {0x112, 0x5A7A50}, {0x12C, 0x5A7A00}, {0x14C, 0x494110}, {0x17D, 0x572FA0}};
constexpr sh::CallSite kCalls4737C0[] = {{0x0, 0x473810}, {0x5, 0x473B60}};
constexpr sh::CallSite kCalls4737E0[] = {{0x0, 0x473C80}, {0x5, 0x473850}};
constexpr sh::CallSite kCalls473800[] = {{0x7, 0x589840}};
constexpr sh::CallSite kCalls473810[] = {{0x16, 0x46E320}};
constexpr sh::CallSite kCalls473850[] = {{0x1, 0x494060}, {0x30, 0x4738A0}};
constexpr sh::CallSite kCalls4738A0[] = {{0xD, 0x5A75D0}, {0x15, 0x5A7780}, {0x27, 0x494110}, {0x45, 0x4941E0}, {0x198, 0x5A79E0}, {0x1AF, 0x5A79A0}, {0x1D5, 0x572FA0}};
constexpr sh::CallSite kCalls473B60[] = {{0xA6, 0x5A8C00}};
constexpr sh::CallSite kCalls473C80[] = {{0x39, 0x473CE0}};
constexpr sh::CallSite kCalls473CE0[] = {{0xE, 0x5B93D2}, {0x1B, 0x5B93D2}, {0x29, 0x5B93D2}, {0x3D, 0x5A8060}, {0x55, 0x5A8E00}, {0x5F, 0x5A8DE0}, {0xAB, 0x5A8200}};
constexpr sh::CallSite kCalls473E30[] = {{0x3C, 0x4976D0}, {0x54, 0x473F10}};
constexpr sh::CallSite kCalls473E90[] = {{0x1C, 0x473F10}};
constexpr sh::CallSite kCalls473EC0[] = {{0x5F, 0x5A77C0}, {0x68, 0x461E50}, {0x74, 0x5A7740}, {0xBB, 0x5A7780}, {0xC3, 0x5A77A0}, {0xCC, 0x461E50}};
constexpr sh::CallSite kCalls473F10[] = {{0xF, 0x5A77C0}, {0x18, 0x461E50}, {0x24, 0x5A7740}, {0x6B, 0x5A7780}, {0x73, 0x5A77A0}, {0x7C, 0x461E50}};
constexpr sh::CallSite kCalls473FC0[] = {{0x16, 0x587740}};
constexpr sh::CallSite kCalls473FE0[] = {{0x15, 0x474070}};
constexpr sh::CallSite kCalls474030[] = {{0x15, 0x474070}};
constexpr sh::CallSite kCalls474070[] = {{0x14, 0x5A79A0}, {0x2C, 0x5A77C0}, {0x35, 0x461E50}, {0x7C, 0x5A75F0}, {0x84, 0x5A7780}, {0x98, 0x5A7A50}, {0xB5, 0x5A7A00}, {0xDA, 0x5A7A50}, {0xF7, 0x5A7A00}, {0x13B, 0x461E50}};
constexpr sh::CallSite kCalls474200[] = {{0x0, 0x474770}, {0x25, 0x587740}, {0x2F, 0x587740}};
constexpr sh::CallSite kCalls474240[] = {{0x16, 0x5A79A0}, {0x2E, 0x5A77C0}, {0x37, 0x461E50}, {0x6B, 0x4747D0}, {0x73, 0x494060}, {0x88, 0x474410}};
constexpr sh::CallSite kCalls474340[] = {{0x15, 0x5A79A0}, {0x2D, 0x5A77C0}, {0x36, 0x461E50}, {0x6A, 0x4747D0}, {0x72, 0x494060}, {0x85, 0x474410}};
constexpr sh::CallSite kCalls474410[] = {{0x2C, 0x5A7A50}, {0x43, 0x5A7A00}, {0x6A, 0x494110}, {0x9F, 0x5A7610}, {0xA7, 0x5A7780}, {0x153, 0x5A7A50}, {0x16E, 0x5A7A00}, {0x1B3, 0x5A8B60}, {0x1B9, 0x5A7A90}, {0x1D8, 0x5A7A00}, {0x1E7, 0x5A7A50}, {0x207, 0x5A7A00}, {0x216, 0x5A7A50}, {0x24F, 0x494110}, {0x256, 0x5A7A00}, {0x265, 0x5A7A50}, {0x28B, 0x5A7A00}, {0x29A, 0x5A7A50}, {0x2DC, 0x494110}, {0x339, 0x461E50}};
constexpr sh::CallSite kCalls4747D0[] = {{0x7, 0x494060}, {0x16, 0x494110}, {0x30, 0x4941E0}, {0x5A, 0x5A75F0}, {0x62, 0x5A7780}, {0x8A, 0x5A7A50}, {0xA9, 0x5A7A00}, {0xCA, 0x5A7A50}, {0xE5, 0x5A7A00}, {0x137, 0x461E50}};
constexpr sh::CallSite kCalls474980[] = {{0x1F, 0x474C30}};
constexpr sh::CallSite kCalls4749B0[] = {{0x5, 0x474A40}, {0xD, 0x474C80}, {0x1B, 0x474C50}};
constexpr sh::CallSite kCalls474A00[] = {{0x20, 0x474A40}, {0x28, 0x474C80}, {0x31, 0x589840}};
constexpr sh::CallSite kCalls474A40[] = {{0x13, 0x5A79A0}, {0x2B, 0x5A77C0}, {0x34, 0x461E50}, {0x69, 0x474AC0}};
constexpr sh::CallSite kCalls474AC0[] = {{0x7, 0x494060}, {0x16, 0x494110}, {0x30, 0x4941E0}, {0x5C, 0x5A75F0}, {0x64, 0x5A7780}, {0x8C, 0x5A7A50}, {0xAB, 0x5A7A00}, {0xCC, 0x5A7A50}, {0xE7, 0x5A7A00}, {0x139, 0x461E50}};
constexpr sh::CallSite kCalls474C80[] = {{0x61, 0x474D20}};
constexpr sh::CallSite kCalls474D20[] = {{0x11, 0x494110}, {0x4A, 0x4941E0}, {0x5D, 0x5A7610}, {0x65, 0x5A7780}, {0x7B, 0x5A7A50}, {0x9B, 0x5A7A00}, {0xC1, 0x5A7A50}, {0xE1, 0x5A7A00}, {0x101, 0x5A7A50}, {0x121, 0x5A7A00}, {0x141, 0x5A7A50}, {0x161, 0x5A7A00}, {0x1DC, 0x461E50}};

#define E2B_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define E2B_CALLS(a) a, E2B_N(a)
#define E2B_FN(name) reinterpret_cast<const void*>(&::name)
constexpr sh::Shape kE = sh::Shape::kEffect;
constexpr sh::Shape kC = sh::Shape::kCall;
constexpr U kP0 = sh::ArgAt(0, sh::Arg::kScratch);
// {name, base, size, calls, imms, tables, ours, ret_mask, calm, shape, pointers, state_span, sub_span, kind}
const sh::Clone kAll[] = {
    {"EffectKind2F_Run", 0x4731A0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E2B_FN(EffectKind2F_Run), 0, false, kE, 0, 2, 0, 0x2F},
    {"EffectKind2F_Start", 0x4731C0, 0x7E, E2B_CALLS(kCalls4731C0), nullptr, 0, nullptr, 0, E2B_FN(EffectKind2F_Start), 0, false, kE, 0, 2, 0, 0x2F},
    {"EffectKind2F_Spin", 0x473240, 0x88, E2B_CALLS(kCalls473240), nullptr, 0, nullptr, 0, E2B_FN(EffectKind2F_Spin), 0, false, kE, 0, 2, 0, 0x2F},
    {"EffectKind2F_StepTrail", 0x4732D0, 0x8D, E2B_CALLS(kCalls4732D0), nullptr, 0, nullptr, 0, E2B_FN(EffectKind2F_StepTrail), 0, false, kC, 0, 2, 0, 0x2F},
    {"EffectKind2F_DrawTrail", 0x473360, 0xF4, E2B_CALLS(kCalls473360), nullptr, 0, nullptr, 0, E2B_FN(EffectKind2F_DrawTrail), 0, false, kC, 0, 2, 0, 0x2F},
    {"EffectKind2F_InitTrail", 0x473460, 0x8A, E2B_CALLS(kCalls473460), nullptr, 0, nullptr, 0, E2B_FN(EffectKind2F_InitTrail), 0, false, kC, 0, 2, 0, 0x2F},
    {"EffectKind33_Run", 0x4734F0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E2B_FN(EffectKind33_Run), 0, false, kE, 0, 4, 0, 0x33},
    {"EffectKind33_Start", 0x473510, 0x40, nullptr, 0, nullptr, 0, nullptr, 0, E2B_FN(EffectKind33_Start), 0, false, kE, 0, 4, 0, 0x33},
    {"EffectKind33_Grow", 0x473550, 0x53, E2B_CALLS(kCalls473550), nullptr, 0, nullptr, 0, E2B_FN(EffectKind33_Grow), 0, false, kE, 0, 4, 0, 0x33},
    {"EffectKind33_DrawDisc", 0x4735B0, 0x45, E2B_CALLS(kCalls4735B0), nullptr, 0, nullptr, 0, E2B_FN(EffectKind33_DrawDisc), 0, false, kC, 0, 4, 0, 0x33},
    {"EffectKind33_DrawQuarter", 0x473600, 0x19C, E2B_CALLS(kCalls473600), nullptr, 0, nullptr, 0, E2B_FN(EffectKind33_DrawQuarter), 0, false, kC, 0, 4, 0, 0x33},
    {"EffectKind35_Run", 0x4737A0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E2B_FN(EffectKind35_Run), 0, false, kE, 0, 3, 0, 0x35},
    {"EffectKind35_Start", 0x4737C0, 0x13, E2B_CALLS(kCalls4737C0), nullptr, 0, nullptr, 0, E2B_FN(EffectKind35_Start), 0, false, kE, 0, 3, 0, 0x35},
    {"EffectKind35_Burst", 0x4737E0, 0x17, E2B_CALLS(kCalls4737E0), nullptr, 0, nullptr, 0, E2B_FN(EffectKind35_Burst), 0, false, kE, 0, 3, 0, 0x35},
    {"EffectKind35_FreeModel", 0x473800, 0xC, E2B_CALLS(kCalls473800), nullptr, 0, nullptr, 0, E2B_FN(EffectKind35_FreeModel), 0, false, kE, 0, 3, 0, 0x35},
    {"EffectKind35_ShardsInit", 0x473810, 0x32, E2B_CALLS(kCalls473810), nullptr, 0, nullptr, 0, E2B_FN(EffectKind35_ShardsInit), 0, false, kE, 0, 3, 0, 0x35},
    {"EffectKind35_ShardsDraw", 0x473850, 0x4E, E2B_CALLS(kCalls473850), nullptr, 0, nullptr, 0, E2B_FN(EffectKind35_ShardsDraw), 0xFF, false, kE, 0, 3, 0, 0x35},
    {"EffectKind35_ShardQuad", 0x4738A0, 0x1E4, E2B_CALLS(kCalls4738A0), nullptr, 0, nullptr, 0, E2B_FN(EffectKind35_ShardQuad), 0, false, kC, 0, 3, 0, 0x35},
    {"EffectKind35_ShardFly", 0x473A90, 0x67, nullptr, 0, nullptr, 0, nullptr, 0, E2B_FN(EffectKind35_ShardFly), 0, false, kE, 0, 3, 0, 0x35},
    {"EffectKind35_ShardFade", 0x473B00, 0x5A, nullptr, 0, nullptr, 0, nullptr, 0, E2B_FN(EffectKind35_ShardFade), 0, false, kE, 0, 3, 0, 0x35},
    {"EffectKind35_SplitModel", 0x473B60, 0x11B, E2B_CALLS(kCalls473B60), nullptr, 0, nullptr, 0, E2B_FN(EffectKind35_SplitModel), 0, false, kE, 0, 3, 0, 0x35},
    {"EffectKind35_StepPieces", 0x473C80, 0x52, E2B_CALLS(kCalls473C80), nullptr, 0, nullptr, 0, E2B_FN(EffectKind35_StepPieces), 0, false, kE, 0, 3, 0, 0x35},
    {"EffectKind35_TurnPiece", 0x473CE0, 0xEA, E2B_CALLS(kCalls473CE0), nullptr, 0, nullptr, 0, E2B_FN(EffectKind35_TurnPiece), 0, false, kC, 0, 3, 0, 0x35},
    {"EffectKind38_Run", 0x473DD0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E2B_FN(EffectKind38_Run), 0, false, kE, 0, 5, 0, 0x38},
    {"EffectKind38_Start", 0x473DF0, 0x31, nullptr, 0, nullptr, 0, nullptr, 0, E2B_FN(EffectKind38_Start), 0, false, kE, 0, 5, 0, 0x38},
    {"EffectKind38_FadeIn", 0x473E30, 0x59, E2B_CALLS(kCalls473E30), nullptr, 0, nullptr, 0, E2B_FN(EffectKind38_FadeIn), 0, false, kE, 0, 5, 0, 0x38},
    {"EffectKind38_WaitMessage", 0x473E90, 0x21, E2B_CALLS(kCalls473E90), nullptr, 0, nullptr, 0, E2B_FN(EffectKind38_WaitMessage), 0, false, kE, 0, 5, 0, 0x38},
    {"EffectKind38_FadeOut", 0x473EC0, 0xD6, E2B_CALLS(kCalls473EC0), nullptr, 0, nullptr, 0, E2B_FN(EffectKind38_FadeOut), 0, false, kE, 0, 5, 0, 0x38},
    {"EffectKind38_DrawTint", 0x473F10, 0x86, E2B_CALLS(kCalls473F10), nullptr, 0, nullptr, 0, E2B_FN(EffectKind38_DrawTint), 0, false, kE, 0, 5, 0, 0x38},
    {"EffectKind39_Run", 0x473FA0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E2B_FN(EffectKind39_Run), 0, false, kE, 0, 4, 0, 0x39},
    {"EffectKind39_Start", 0x473FC0, 0x1D, E2B_CALLS(kCalls473FC0), nullptr, 0, nullptr, 0, E2B_FN(EffectKind39_Start), 0, false, kE, 0, 4, 0, 0x39},
    {"EffectKind39_Grow", 0x473FE0, 0x42, E2B_CALLS(kCalls473FE0), nullptr, 0, nullptr, 0, E2B_FN(EffectKind39_Grow), 0, false, kE, 0, 4, 0, 0x39},
    {"EffectKind39_Shrink", 0x474030, 0x3A, E2B_CALLS(kCalls474030), nullptr, 0, nullptr, 0, E2B_FN(EffectKind39_Shrink), 0, false, kE, 0, 4, 0, 0x39},
    {"EffectKind39_DrawDisc", 0x474070, 0x16D, E2B_CALLS(kCalls474070), nullptr, 0, nullptr, 0, E2B_FN(EffectKind39_DrawDisc), 0, false, kC, 0, 4, 0, 0x39},
    {"EffectKind3B_Run", 0x4741E0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E2B_FN(EffectKind3B_Run), 0, false, kE, 0, 4, 0, 0x3B},
    {"EffectKind3B_Start", 0x474200, 0x38, E2B_CALLS(kCalls474200), nullptr, 0, nullptr, 0, E2B_FN(EffectKind3B_Start), 0, false, kE, 0, 4, 0, 0x3B},
    {"EffectKind3B_Rise", 0x474240, 0xFD, E2B_CALLS(kCalls474240), nullptr, 0, nullptr, 0, E2B_FN(EffectKind3B_Rise), 0, false, kE, 0, 4, 0, 0x3B},
    {"EffectKind3B_Fade", 0x474340, 0xC1, E2B_CALLS(kCalls474340), nullptr, 0, nullptr, 0, E2B_FN(EffectKind3B_Fade), 0, false, kE, 0, 4, 0, 0x3B},
    {"EffectKind3B_DrawSpiral", 0x474410, 0x358, E2B_CALLS(kCalls474410), nullptr, 0, nullptr, 0, E2B_FN(EffectKind3B_DrawSpiral), 0, false, kC, 0, 4, 0, 0x3B},
    {"EffectKind3B_InitSpirals", 0x474770, 0x5C, nullptr, 0, nullptr, 0, nullptr, 0, E2B_FN(EffectKind3B_InitSpirals), 0, false, kE, 0, 4, 0, 0x3B},
    {"EffectKind3B_DrawGlow", 0x4747D0, 0x164, E2B_CALLS(kCalls4747D0), nullptr, 0, nullptr, 0, E2B_FN(EffectKind3B_DrawGlow), 0, false, kC, kP0, 4, 0, 0x3B},
    {"EffectKind3D_Run", 0x474940, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E2B_FN(EffectKind3D_Run), 0, false, kE, 0, 4, 0, 0x3D},
    {"EffectKind3D_Start", 0x474960, 0x15, nullptr, 0, nullptr, 0, nullptr, 0, E2B_FN(EffectKind3D_Start), 0, false, kE, 0, 4, 0, 0x3D},
    {"EffectKind3D_Wait", 0x474980, 0x2D, E2B_CALLS(kCalls474980), nullptr, 0, nullptr, 0, E2B_FN(EffectKind3D_Wait), 0, false, kE, 0, 4, 0, 0x3D},
    {"EffectKind3D_Glow", 0x4749B0, 0x48, E2B_CALLS(kCalls4749B0), nullptr, 0, nullptr, 0, E2B_FN(EffectKind3D_Glow), 0, false, kE, 0, 4, 0, 0x3D},
    {"EffectKind3D_Fade", 0x474A00, 0x37, E2B_CALLS(kCalls474A00), nullptr, 0, nullptr, 0, E2B_FN(EffectKind3D_Fade), 0, false, kE, 0, 4, 0, 0x3D},
    {"EffectKind3D_DrawGlow", 0x474A40, 0x72, E2B_CALLS(kCalls474A40), nullptr, 0, nullptr, 0, E2B_FN(EffectKind3D_DrawGlow), 0, false, kC, 0, 4, 0, 0x3D},
    {"EffectKind3D_DrawFan", 0x474AC0, 0x166, E2B_CALLS(kCalls474AC0), nullptr, 0, nullptr, 0, E2B_FN(EffectKind3D_DrawFan), 0, false, kC, kP0, 4, 0, 0x3D},
    {"EffectKind3D_RingsClear", 0x474C30, 0x14, nullptr, 0, nullptr, 0, nullptr, 0, E2B_FN(EffectKind3D_RingsClear), 0, false, kE, 0, 4, 0, 0x3D},
    {"EffectKind3D_RingSpawn", 0x474C50, 0x22, nullptr, 0, nullptr, 0, nullptr, 0, E2B_FN(EffectKind3D_RingSpawn), 0, false, kE, 0, 4, 0, 0x3D},
    {"EffectKind3D_RingsStep", 0x474C80, 0x93, E2B_CALLS(kCalls474C80), nullptr, 0, nullptr, 0, E2B_FN(EffectKind3D_RingsStep), 0xFF, false, kE, 0, 4, 0, 0x3D},
    {"EffectKind3D_DrawRing", 0x474D20, 0x214, E2B_CALLS(kCalls474D20), nullptr, 0, nullptr, 0, E2B_FN(EffectKind3D_DrawRing), 0, false, kC, 0, 4, 0, 0x3D},
};
#undef E2B_FN
#undef E2B_CALLS
#undef E2B_N

enum : unsigned {
    k2FRun, k2FStart, k2FSpin, k2FStep, k2FDraw, k2FInit, k33Run, k33Start, k33Grow, k33Disc, k33Quarter, k35Run,
    k35Start, k35Burst, k35Free, k35ShardsInit, k35ShardsDraw, k35Quad, k35Fly, k35Fade, k35Split, k35Step, k35Turn,
    k38Run, k38Start, k38FadeIn, k38Wait, k38FadeOut, k38Tint, k39Run, k39Start, k39Grow, k39Shrink, k39Disc, k3BRun,
    k3BStart, k3BRise, k3BFade, k3BSpiral, k3BInit, k3BGlow, k3DRun, k3DStart, k3DWait, k3DGlow, k3DFade, k3DDrawGlow,
    k3DFan, k3DClear, k3DSpawn, k3DStep, k3DRing, kCount
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

// The model Sprite_ObjectsExtra[1] +0x50 points at (55 faces of 0x28, read and
// rewritten): a buffer of the fuzz's own, compared.
constexpr unsigned kModelSize = at::kPiece35Count * 0x28;
alignas(16) unsigned char g_model[kModelSize];

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
// EffectGte_ProjectSize: the size's words noted - only the first where out is
// four bytes past it (EffectKind3B_DrawGlow and EffectKind3D_DrawFan: the
// original's second in word is its stack's leftover and out's second word is
// never read), both elsewhere (in place in EffectKind35_ShardQuad, apart in
// EffectKind3D_DrawRing); out's two s16 filled.
U FxProjectSize(const U* a, U answer) {
    const unsigned n = a[2] == a[1] + 4 ? 2 : 4;
    if (Writable(a[1], n)) sh::NoteBytes(P(a[1]), n);
    Fill(a[2], 4);
    return answer;
}
// Gte_RotTrans: the three longs of out.
U FxRotTrans(const U* a, U answer) {
    Fill(a[1], 12);
    return answer;
}
// Gte_VectorNormal: the three longs of out (in place in EffectKind3B_DrawSpiral).
U FxVectorNormal(const U* a, U answer) {
    Fill(a[1], 12);
    return answer;
}
// Gte_SetTransMatrix reads the MATRIX's translation, +0x14..+0x1F: noted (the
// rotation hashed by the deref; the padding word +0x12, the callers' leftover
// stack, neither).
U FxSetTrans(const U* a, U answer) {
    if (Writable(a[0], 0x20)) sh::NoteBytes(P(a[0] + 0x14), 12);
    return answer;
}
// Math_Cos: any answer but 0 and -1 - EffectKind3B_DrawSpiral divides by
// Math_Cos(0x80) (a real cosine, never 0: the original's idiv would fault on
// a 0, and on 0x80000000 / -1).
U FxCos(const U*, U answer) { return answer == 0 || answer == 0xFFFFFFFFu ? 1u : answer; }

#define E2B_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
constexpr sh::Answer kG = sh::Answer::kGarbage, kF = sh::Answer::kFlag, kPh = sh::Answer::kPhase;
constexpr U kW = 0xFFFFFFFFu, k16 = 0xFFFFu, k8 = 0xFFu;
const sh::Callee kCallees[] = {
    // the group's own called directly, by name
    {E2B_OURS(EffectKind2F_StepTrail), 1, {kW}, kG, 0, 0},
    {E2B_OURS(EffectKind2F_DrawTrail), 1, {kW}, kG, 0, 0},
    {E2B_OURS(EffectKind2F_InitTrail), 1, {kW}, kG, 0, 0},
    {E2B_OURS(EffectKind33_DrawDisc), 2, {kW, kW}, kG, 0, 0},
    {E2B_OURS(EffectKind33_DrawQuarter), 5, {kW, kW, k16, kW, kW}, kG, 0, 0},   // the angle's low word (and 0xFFFF)
    {E2B_OURS(EffectKind35_ShardsInit), 0, {}, kPh, 0, 0},
    {E2B_OURS(EffectKind35_SplitModel), 0, {}, kPh, 0, 0},
    {E2B_OURS(EffectKind35_StepPieces), 0, {}, kPh, 0, 0},
    {E2B_OURS(EffectKind35_ShardsDraw), 0, {}, kF, 0, 0},
    {E2B_OURS(EffectKind35_ShardQuad), 1, {kW}, kG, 0, 0},
    {E2B_OURS(EffectKind35_TurnPiece), 3, {kW, kW, kW}, kG, 0, 0},
    {E2B_OURS(EffectKind38_DrawTint), 0, {}, kPh, 0, 0},   // 0x473E30's and 0x473E90's tail jmp
    {E2B_OURS(EffectKind39_DrawDisc), 3, {k16, k16, k16}, kG, 0, 0},   // three words pushed with their registers' upper halves
    {E2B_OURS(EffectKind3B_InitSpirals), 0, {}, kPh, 0, 0},
    {E2B_OURS(EffectKind3B_DrawSpiral), 1, {kW}, kG, 0, 0},
    // a point on the caller's stack (12 read), the size word, the two shade bytes
    {E2B_OURS(EffectKind3B_DrawGlow), 4, {0, k16, k8, k8}, kG, 0, 0, {12}, nullptr, nullptr, true},
    {E2B_OURS(EffectKind3D_DrawGlow), 1, {k16}, kG, 0, 0},   // the size: only its word is read, by EffectKind3D_DrawFan
    {E2B_OURS(EffectKind3D_DrawFan), 4, {0, k16, k8, k8}, kG, 0, 0, {12}, nullptr, nullptr, true},
    {E2B_OURS(EffectKind3D_RingsClear), 0, {}, kPh, 0, 0},
    {E2B_OURS(EffectKind3D_RingSpawn), 0, {}, kPh, 0, 0},
    {E2B_OURS(EffectKind3D_RingsStep), 0, {}, kF, 0, 0},
    {E2B_OURS(EffectKind3D_DrawRing), 1, {kW}, kG, 0, 0},
    // wave one's, ours (E1C): kind 0x1E's shard set-up, sixteen times here
    {E2B_OURS(EffectKind1E_ShardInit), 1, {kW}, kG, 0, 0},
    // standard entries re-listed louder: the stack pointers never logged, the
    // outs filled, the SVECTORs' padding (the callers' leftovers) not hashed
    {E2B_OURS(EffectGte_ProjectPoint), 2, {0, 0}, kG, 0, 0, {12, 0}, &FxProjectPoint, nullptr, true},
    {E2B_OURS(EffectGte_ProjectSize), 3, {0, 0, 0}, kG, 0, 0, {12, 0, 0}, &FxProjectSize, nullptr, true},
    {E2B_OURS(Gte_RotTrans), 2, {0, 0}, kG, 0, 0, {6, 0}, &FxRotTrans, nullptr, true},
    {E2B_OURS(Gte_VectorNormal), 2, {0, 0}, kG, 0, 0, {12, 0}, &FxVectorNormal, nullptr, true},
    {E2B_OURS(Gte_SetTransMatrix), 1, {0}, kG, 0, 0, {18}, &FxSetTrans, nullptr, true},
    {E2B_OURS(Gte_SetRotMatrix), 1, {0}, kG, 0, 0, {18}, nullptr, nullptr, true},
    {E2B_OURS(Math_Cos), 1, {kW}, kG, 0, 0, {}, &FxCos, nullptr, true},
};
#undef E2B_OURS

// The tables the dispatchers go through, read in place: each its own length
// (to the next table a dispatcher names; none bounded).
const sh::DataTable kTables[] = {
    {at::kKind2FStates, 2},       // EffectKind2F_States
    {at::kKind33States, 4},       // EffectKind33_States
    {at::kKind35States, 3},       // EffectKind35_States
    {at::kKind35ShardStates, 2},  // EffectKind35_ShardStates
    {at::kKind38States, 5},       // EffectKind38_States
    {at::kKind39States, 4},       // EffectKind39_States
    {at::kKind3BStates, 4},       // EffectKind3B_States
    {at::kKind3DStates, 4},       // EffectKind3D_States
};

// Beyond effect mode's standard regions: the rest of EffectKind30_Shards past
// the standard 0x644 to the end of kind 0x35's face copies (0x92CFC0; kind
// 0x3B's spirals end at 0x92CDB4), the shard cursor, the fuzz's model.
const sh::Region kRegions[] = {
    {at::kPool + 0x644, at::kPool35End - (at::kPool + 0x644)},
    {at::kShardCursor35, 4},
    {0, sizeof g_model},
};
sh::Region g_regions[sizeof kRegions / sizeof kRegions[0]];

// --- the seed ------------------------------------------------------------------------

unsigned char* Shard(unsigned i) { return P(at::kShards35 + 0x28 * (i % at::kShard35Count)); }
unsigned char* Ring(unsigned i) { return P(at::kRings + at::kRingStride * (i % at::kRingCount)); }
unsigned char* Spiral(unsigned i) { return P(at::kSpirals + at::kSpiralStride * (i % at::kSpiralCount)); }
unsigned char* Trail(unsigned i) { return P(at::kTrails + at::kTrailStride * (i % at::kTrailCount)); }

// Kind 0x35's sixteen shards: in use half the time, their state +1 inside
// EffectKind35_ShardStates' two, their count +2 at 1 or 2 (the steps' zero) or any.
void Shards() {
    for (unsigned i = 0; i < at::kShard35Count; ++i) {
        unsigned char* const k = Shard(i);
        k[0] = static_cast<unsigned char>(sh::Half() ? 1 : 0);
        k[1] = static_cast<unsigned char>(sh::Next() % 2);
        k[2] = static_cast<unsigned char>(PickOf(1, 1, 2, 0, sh::Next()));
    }
    SetLong(Mem(at::kShardCursor35), static_cast<std::int32_t>(Key(Shard(sh::Next()))));
}

// Kind 0x3D's sixteen rings: a third free, the rest in state 0, 1 or another,
// the count +0x16 at 1 (the free), 2, 0 or any.
void Rings() {
    for (unsigned i = 0; i < at::kRingCount; ++i) {
        unsigned char* const r = Ring(i);
        r[0x14] = static_cast<unsigned char>(sh::Next() % 3 == 0 ? 0 : 1 + sh::Next() % 0xFF);
        r[0x15] = static_cast<unsigned char>(PickOf(0, 1, 1, 2, sh::Next()));
        r[0x16] = static_cast<unsigned char>(PickOf(1, 1, 2, 0, sh::Next()));
    }
}

// Kind 0x3B's spirals: the shade word at the clamps' boundaries.
void Spirals() {
    for (unsigned i = 0; i < at::kSpiralCount; ++i)
        SetWord(Spiral(i) + 0x14, PickOf(0, 3, 4, 0x7C, 0x80, 0xFF, 0x100, 0x103, 0xFFFF, 0x8000, sh::Next()));
}

void Seed(unsigned k) {
    unsigned char* const s = Sprite_Current;
    // what every kind may read: the model, the shard cursor
    SetLong(Mem(at::kExtra1Model), static_cast<std::int32_t>(Key(g_model)));
    SetLong(Mem(at::kShardCursor35), static_cast<std::int32_t>(Key(Shard(sh::Next()))));
    s[9] = static_cast<unsigned char>(PickOf(0, 1, 1, 2, 0x1D, 0x1E, 0x1F, 0x10, 0x40, 0x80, 0xFF, sh::Next()));
    if (sh::Half()) Frame_Counter = (Frame_Counter & ~7u) | (sh::Next() % 2);   // Frame_Counter & 7 == 0, & 1
    switch (k) {
    case k35ShardsDraw:
    case k35Fly:
    case k35Fade:
    case k35Burst: Shards(); break;
    case k38Wait: Field_Request = static_cast<unsigned char>(PickOf(2, 2, 0, 1, sh::Next())); break;
    case k3BRise:
    case k3BFade:
    case k3BSpiral:
        s[0xA] = static_cast<unsigned char>(PickOf(0, 0x1F, 0x20, 0x21, 0xFF, sh::Next()));
        Spirals();
        break;
    case k3DWait:
    case k3DGlow:
    case k3DFade:
        SetLong(s + 0xC, static_cast<std::int32_t>(PickOf(0, 1, 1, 2, 0x80000000u, 0xFFFFFFFFu, 0x10, sh::Next())));
        Rings();
        break;
    case k3DSpawn:
    case k3DStep:
    case k3DClear: Rings(); break;
    default: break;
    }
}

void Args(unsigned k, U* a) {
    const U i = sh::Next();
    switch (k) {
    case k2FStep:
    case k2FDraw:
    case k2FInit: a[0] = Key(Trail(i)); break;
    case k33Disc:
    case k33Quarter:
        a[0] = Key(sh::EffectRecord(i) + 0xC);
        a[1] = PickOf(0, 0x1199, 0x10000, sh::Next());
        if (k == k33Quarter) {
            a[3] = PickOf(1, 0xFFFFFFFFu, sh::Next());
            a[4] = PickOf(1, 0xFFFFFFFFu, sh::Next());
        }
        break;
    case k35Quad: a[0] = Key(Shard(i)); break;
    case k35Turn: {
        const U n = i % at::kPiece35Count;
        a[0] = at::kCopies35 + 0x28 * n;
        a[1] = Key(g_model) + 0x28 * n;
        a[2] = at::kPieces35 + 0x18 * n;
        break;
    }
    case k3BSpiral: a[0] = Key(Spiral(i)); break;
    case k3DRing: a[0] = Key(Ring(i)); break;
    default: break;
    }
}

// What the functions read again after a call, moved by the group's case of the
// harness's disturbance (from its hash only): the frame count +9, +0xA, the
// timer +0xC, Field_Request, a shard's in-use byte, state and count, a ring's
// in-use byte, state and count, a spiral's shade.
void Disturb(U h) {
    const U v = h >> 8;
    unsigned char* const s = Sprite_Current;
    switch (h % 8) {
    case 0: s[9] = static_cast<unsigned char>((v & 1) ? (v >> 1) % 3 : v >> 3); break;
    case 1: s[0xA] = static_cast<unsigned char>((v & 1) ? 0x20 + (v >> 1) % 2 : v >> 3); break;
    case 2: SetLong(s + 0xC, static_cast<std::int32_t>((v & 1) ? (v >> 1) % 3 : v)); break;
    case 3: Field_Request = static_cast<unsigned char>((v & 1) ? 2 : v >> 1); break;
    case 4: Shard(v)[0] = static_cast<unsigned char>(v >> 4); break;
    case 5: {
        unsigned char* const k = Shard(v);
        k[1] = static_cast<unsigned char>((v >> 4) % 2);
        k[2] = static_cast<unsigned char>(v >> 5);
        break;
    }
    case 6: {
        unsigned char* const r = Ring(v);
        r[0x14] = static_cast<unsigned char>(v >> 4);
        r[0x15] = static_cast<unsigned char>((v >> 12) % 3);
        r[0x16] = static_cast<unsigned char>(v >> 14);
        break;
    }
    case 7: SetWord(Spiral(v) + 0x14, v >> 2); break;
    default: break;
    }
}

}  // namespace

void SelfTest() {
    // BOF3X_E2B_ONLY: the clones whose name contains it (a control's run)
    static sh::Clone chosen[kCount];
    static unsigned index[kCount];
    const char* const only = std::getenv("BOF3X_E2B_ONLY");
    unsigned n = 0;
    for (unsigned k = 0; k < kCount; ++k)
        if (!only || !*only || std::strstr(kAll[k].name, only)) {
            index[n] = k;
            chosen[n++] = kAll[k];
        }
    if (n == 0) bof3::Fatal("effect_2b: BOF3X_E2B_ONLY=%s names no clone", only);
    static unsigned* s_index = index;
    for (unsigned i = 0; i < sizeof kRegions / sizeof kRegions[0]; ++i) g_regions[i] = kRegions[i];
    g_regions[2].at = Key(g_model);
    static const std::uint8_t kKinds[] = {0x2F, 0x33, 0x35, 0x38, 0x39, 0x3B, 0x3D};
    sh::Group g = {"effect_2b", chosen, n, kCallees, sizeof kCallees / sizeof kCallees[0], kTables,
                   sizeof kTables / sizeof kTables[0], g_regions, sizeof g_regions / sizeof g_regions[0],
                   [](unsigned k) { Seed(s_index[k]); }, &Disturb, 4000};
    g.args = [](unsigned k, U* a) { Args(s_index[k], a); };
    g.effect = true;
    g.kinds = kKinds;
    g.n_kinds = sizeof kKinds;
    sh::Run(g);
}

}  // namespace effect_2b

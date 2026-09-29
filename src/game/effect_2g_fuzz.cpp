// BOF3X_SHADOW=effect_2g: group E2G's 58 functions through the scenario harness
// in effect mode (scenario_harness.h, docs/scenario_harness.md section 8), once
// at start-up. docs/effect_2g.md section 4. BOF3X_E2G_ONLY=<name> runs the
// clones whose name contains it (the controls' speed-up).
//
// The clone table is tools/band_rows.py --group E2G --clones --harness scenario
// (2026-09-29), each extent read again to its last instruction (capstone) and
// the names given, with two changes: 0x47E0F0 ends at its jmp to 0x47E120 (the
// tool's extent ran on into the draw, which three sub-states tail-jump to and
// which is taken as its own function, EffectKind18Sub20_Draw, its calls the
// tool's less 0x30); and kind 0x5E's dispatcher 0x47F5D0, which no list of
// the cut holds, added. Shapes: every dispatcher and state kEffect
// (Sprite_Current one of the 20 Effect_Objects records, +5 the kind, a
// dispatcher's byte below its table's length); the two draws with arguments
// kCall.
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_2g.h"
#include "game/effect_2g_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace effect_2g {
namespace {

namespace sh = scenario_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using U = std::uint32_t;

// tools/band_rows.py --group E2G --clones, 2026-09-29 (0x47E0F0 / 0x47E120 split by hand).
constexpr sh::CallSite kCalls47DC20[] = {{0x5A, 0x595F60}};
constexpr sh::CallSite kCalls47DCA0[] = {{0x29, 0x595C50}, {0x31, 0x47DE10}, {0x3A, 0x47DE50}, {0x43, 0x47DE50}};
constexpr sh::CallSite kCalls47DCF0[] = {{0x29, 0x595C50}, {0x31, 0x47DE10}, {0x3A, 0x47DE50}, {0x43, 0x47DE50}};
constexpr sh::CallSite kCalls47DD40[] = {{0x29, 0x595C50}, {0x31, 0x47DE10}, {0x3A, 0x47DE50}, {0x43, 0x47DE50}, {0x4C, 0x47DE50}};
constexpr sh::CallSite kCalls47DDA0[] = {{0x4E, 0x595F60}, {0x63, 0x589840}};
constexpr sh::CallSite kCalls47DE10[] = {{0xB, 0x4977F0}};
constexpr sh::CallSite kCalls47DE50[] = {{0x8, 0x5A75D0}, {0x14D, 0x5A79E0}, {0x164, 0x5A79A0}, {0x171, 0x461E50}};
constexpr sh::CallSite kCalls47DFF0[] = {{0x7, 0x57C140}, {0x40, 0x47E120}};
constexpr sh::CallSite kCalls47E040[] = {{0xE, 0x57C140}, {0x1F, 0x587740}};
constexpr sh::CallSite kCalls47E070[] = {{0x27, 0x572650}, {0x37, 0x47E120}};
constexpr sh::CallSite kCalls47E0B0[] = {{0xE, 0x57C140}, {0x1E, 0x572650}, {0x28, 0x587740}};
constexpr sh::CallSite kCalls47E0F0[] = {{0x27, 0x47E120}};
constexpr sh::CallSite kCalls47E120[] = {{0x1A, 0x5A75D0}, {0x22, 0x5A77A0}, {0xC6, 0x5A85F0}, {0xCC, 0x5A9290},
                                         {0xD9, 0x572A00}, {0xEC, 0x572FA0}, {0xF8, 0x5A75D0}, {0x100, 0x5A77A0},
                                         {0x159, 0x5A85F0}, {0x162, 0x5A9290}, {0x16F, 0x572A00}, {0x17F, 0x572FA0}};
constexpr sh::CallSite kCalls47E310[] = {{0x3D, 0x5891F0}};
constexpr sh::CallSite kCalls47E370[] = {{0x67, 0x5B93D2}};
constexpr sh::CallSite kCalls47E410[] = {{0x6A, 0x5891F0}, {0x72, 0x5B93D2}};
constexpr sh::CallSite kCalls47E540[] = {{0x4C, 0x587740}, {0x53, 0x5891F0}};
constexpr sh::CallSite kCalls47E5B0[] = {{0x44, 0x5891F0}, {0x4C, 0x5B93D2}};
constexpr sh::CallSite kCalls47E630[] = {{0x18, 0x5891F0}, {0x3A, 0x589840}};
constexpr sh::CallSite kCalls47E710[] = {{0x43, 0x5891F0}, {0x5D, 0x5891F0}};
constexpr sh::CallSite kCalls47E790[] = {{0x5C, 0x587740}, {0x7C, 0x5891F0}, {0x9E, 0x5891F0}};
constexpr sh::CallSite kCalls47E900[] = {{0x61, 0x5891F0}};
constexpr sh::CallSite kCalls47E980[] = {{0x47, 0x5891F0}};
constexpr sh::CallSite kCalls47EA30[] = {{0x44, 0x5891F0}, {0x52, 0x589840}};
constexpr sh::CallSite kCalls47EAD0[] = {{0x0, 0x47EB90}, {0x5, 0x47EC30}};
constexpr sh::CallSite kCalls47EB30[] = {{0x29, 0x587B40}, {0x31, 0x589840}, {0x47, 0x587B40}, {0x4F, 0x589840}};
constexpr sh::CallSite kCalls47EB90[] = {{0xB, 0x586160}, {0x46, 0x5B9380}, {0x56, 0x516F60}, {0x77, 0x516F60}, {0x8D, 0x516F60}};
constexpr sh::CallSite kCalls47EC30[] = {{0x10, 0x586160}, {0x27, 0x5B9380}, {0x3D, 0x516F60}, {0x58, 0x516B30}};
constexpr sh::CallSite kCalls47ECC0[] = {{0x0, 0x47ECF0}};
constexpr sh::CallSite kCalls47ECE0[] = {{0x0, 0x589840}};
constexpr sh::CallSite kCalls47ECF0[] = {{0x17, 0x5A79A0}, {0x2F, 0x5A77C0}, {0x38, 0x461E50}, {0x77, 0x5A7610}, {0x7F, 0x5A7780},
                                         {0xB1, 0x5A7A50}, {0xC6, 0x5A7A00}, {0xDB, 0x5A7A50}, {0xF7, 0x5A7A00}, {0x189, 0x461E50}};
constexpr sh::CallSite kCalls47EEC0[] = {{0x26, 0x5720C0}};
constexpr sh::CallSite kCalls47EF10[] = {{0x22, 0x4220D0}};
constexpr sh::CallSite kCalls47EFB0[] = {{0x2A, 0x47F040}};
constexpr sh::CallSite kCalls47EFF0[] = {{0x1A, 0x589840}, {0x3F, 0x47F040}};
constexpr sh::CallSite kCalls47F040[] = {{0x37, 0x5A7840}, {0x4D, 0x572FA0}, {0x6F, 0x5720C0}, {0x81, 0x5A75D0}, {0xB3, 0x494110},
                                         {0xE6, 0x494110}, {0x113, 0x494110}, {0x147, 0x494110}, {0x159, 0x5A79E0}, {0x171, 0x5A79A0},
                                         {0x1F5, 0x5A7780}, {0x1FD, 0x5A77A0}, {0x213, 0x572FA0}, {0x244, 0x5A7840}, {0x25D, 0x572FA0}};

#define E2G_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define E2G_CALLS(a) a, E2G_N(a)
#define E2G_FN(name) reinterpret_cast<const void*>(&::name)
constexpr sh::Shape kEf = sh::Shape::kEffect;
constexpr sh::Shape kCa = sh::Shape::kCall;
// {name, base, size, calls, imms, tables, ours, ret_mask, calm, shape, pointers, state_span, sub_span, kind}
const sh::Clone kAll[] = {
    {"EffectKind66_Run", 0x47DBE0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E2G_FN(EffectKind66_Run), 0, false, kEf, 0, 6, 0, 0x66},
    {"EffectKind66_Start", 0x47DC00, 0x1D, nullptr, 0, nullptr, 0, nullptr, 0, E2G_FN(EffectKind66_Start), 0, false, kEf, 0, 0, 0, 0x66},
    {"EffectKind66_Open", 0x47DC20, 0x72, E2G_CALLS(kCalls47DC20), nullptr, 0, nullptr, 0, E2G_FN(EffectKind66_Open), 0, false, kEf, 0, 0, 0, 0x66},
    {"EffectKind66_Show2", 0x47DCA0, 0x4C, E2G_CALLS(kCalls47DCA0), nullptr, 0, nullptr, 0, E2G_FN(EffectKind66_Show2), 0, false, kEf, 0, 0, 0, 0x66},
    {"EffectKind66_Show3", 0x47DCF0, 0x4C, E2G_CALLS(kCalls47DCF0), nullptr, 0, nullptr, 0, E2G_FN(EffectKind66_Show3), 0, false, kEf, 0, 0, 0, 0x66},
    {"EffectKind66_Show4", 0x47DD40, 0x55, E2G_CALLS(kCalls47DD40), nullptr, 0, nullptr, 0, E2G_FN(EffectKind66_Show4), 0, false, kEf, 0, 0, 0, 0x66},
    {"EffectKind66_Close", 0x47DDA0, 0x6E, E2G_CALLS(kCalls47DDA0), nullptr, 0, nullptr, 0, E2G_FN(EffectKind66_Close), 0, false, kEf, 0, 0, 0, 0x66},
    {"EffectKind66_Message", 0x47DE10, 0x34, E2G_CALLS(kCalls47DE10), nullptr, 0, nullptr, 0, E2G_FN(EffectKind66_Message), 0, false, kEf, 0, 0, 0, 0x66},
    {"EffectKind66_DrawPiece", 0x47DE50, 0x17B, E2G_CALLS(kCalls47DE50), nullptr, 0, nullptr, 0, E2G_FN(EffectKind66_DrawPiece), 0, false, kCa, 0, 0, 0, 0x66},
    {"EffectKind18Sub20_Run", 0x47DFD0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E2G_FN(EffectKind18Sub20_Run), 0, false, kEf, 0, 0, 5, 0x18},
    {"EffectKind18Sub20_Start", 0x47DFF0, 0x45, E2G_CALLS(kCalls47DFF0), nullptr, 0, nullptr, 0, E2G_FN(EffectKind18Sub20_Start), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub20_WaitSet", 0x47E040, 0x30, E2G_CALLS(kCalls47E040), nullptr, 0, nullptr, 0, E2G_FN(EffectKind18Sub20_WaitSet), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub20_Lower", 0x47E070, 0x3C, E2G_CALLS(kCalls47E070), nullptr, 0, nullptr, 0, E2G_FN(EffectKind18Sub20_Lower), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub20_WaitClear", 0x47E0B0, 0x39, E2G_CALLS(kCalls47E0B0), nullptr, 0, nullptr, 0, E2G_FN(EffectKind18Sub20_WaitClear), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub20_Raise", 0x47E0F0, 0x2C, E2G_CALLS(kCalls47E0F0), nullptr, 0, nullptr, 0, E2G_FN(EffectKind18Sub20_Raise), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub20_Draw", 0x47E120, 0x1A2, E2G_CALLS(kCalls47E120), nullptr, 0, nullptr, 0, E2G_FN(EffectKind18Sub20_Draw), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind15_Run", 0x47E2D0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E2G_FN(EffectKind15_Run), 0, false, kEf, 0, 10, 0, 0x15},
    {"EffectKind15_Start", 0x47E2F0, 0x1B, nullptr, 0, nullptr, 0, nullptr, 0, E2G_FN(EffectKind15_Start), 0, false, kEf, 0, 0, 0, 0x15},
    {"EffectKind15_Begin", 0x47E310, 0x58, E2G_CALLS(kCalls47E310), nullptr, 0, nullptr, 0, E2G_FN(EffectKind15_Begin), 0, false, kEf, 0, 0, 0, 0x15},
    {"EffectKind15_WaitPose4", 0x47E370, 0x94, E2G_CALLS(kCalls47E370), nullptr, 0, nullptr, 0, E2G_FN(EffectKind15_WaitPose4), 0, false, kEf, 0, 0, 0, 0x15},
    {"EffectKind15_Countdown", 0x47E410, 0x9F, E2G_CALLS(kCalls47E410), nullptr, 0, nullptr, 0, E2G_FN(EffectKind15_Countdown), 0, false, kEf, 0, 0, 0, 0x15},
    {"EffectKind15_WaitCue3", 0x47E4B0, 0x2A, nullptr, 0, nullptr, 0, nullptr, 0, E2G_FN(EffectKind15_WaitCue3), 0, false, kEf, 0, 0, 0, 0x15},
    {"EffectKind15_Cooldown", 0x47E4E0, 0x55, nullptr, 0, nullptr, 0, nullptr, 0, E2G_FN(EffectKind15_Cooldown), 0, false, kEf, 0, 0, 0, 0x15},
    {"EffectKind15_Hit", 0x47E540, 0x67, E2G_CALLS(kCalls47E540), nullptr, 0, nullptr, 0, E2G_FN(EffectKind15_Hit), 0, false, kEf, 0, 0, 0, 0x15},
    {"EffectKind15_WaitPose8", 0x47E5B0, 0x79, E2G_CALLS(kCalls47E5B0), nullptr, 0, nullptr, 0, E2G_FN(EffectKind15_WaitPose8), 0, false, kEf, 0, 0, 0, 0x15},
    {"EffectKind15_End", 0x47E630, 0x41, E2G_CALLS(kCalls47E630), nullptr, 0, nullptr, 0, E2G_FN(EffectKind15_End), 0, false, kEf, 0, 0, 0, 0x15},
    {"EffectKind54_Run", 0x47E680, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E2G_FN(EffectKind54_Run), 0, false, kEf, 0, 15, 0, 0x54},
    {"EffectKind54_Start", 0x47E6A0, 0x14, nullptr, 0, nullptr, 0, nullptr, 0, E2G_FN(EffectKind54_Start), 0, false, kEf, 0, 0, 0, 0x54},
    {"EffectKind54_Arm", 0x47E6C0, 0x4D, nullptr, 0, nullptr, 0, nullptr, 0, E2G_FN(EffectKind54_Arm), 0, false, kEf, 0, 0, 0, 0x54},
    {"EffectKind54_Cue", 0x47E710, 0x71, E2G_CALLS(kCalls47E710), nullptr, 0, nullptr, 0, E2G_FN(EffectKind54_Cue), 0, false, kEf, 0, 0, 0, 0x54},
    {"EffectKind54_Strike", 0x47E790, 0xB2, E2G_CALLS(kCalls47E790), nullptr, 0, nullptr, 0, E2G_FN(EffectKind54_Strike), 0, false, kEf, 0, 0, 0, 0x54},
    {"EffectKind54_WaitPose2", 0x47E850, 0x4E, nullptr, 0, nullptr, 0, nullptr, 0, E2G_FN(EffectKind54_WaitPose2), 0, false, kEf, 0, 0, 0, 0x54},
    {"EffectKind54_WaitPose8", 0x47E8A0, 0x55, nullptr, 0, nullptr, 0, nullptr, 0, E2G_FN(EffectKind54_WaitPose8), 0, false, kEf, 0, 0, 0, 0x54},
    {"EffectKind54_Rearm", 0x47E900, 0x75, E2G_CALLS(kCalls47E900), nullptr, 0, nullptr, 0, E2G_FN(EffectKind54_Rearm), 0, false, kEf, 0, 0, 0, 0x54},
    {"EffectKind54_Recoil", 0x47E980, 0x5B, E2G_CALLS(kCalls47E980), nullptr, 0, nullptr, 0, E2G_FN(EffectKind54_Recoil), 0, false, kEf, 0, 0, 0, 0x54},
    {"EffectKind54_WaitPose2B", 0x47E9E0, 0x4E, nullptr, 0, nullptr, 0, nullptr, 0, E2G_FN(EffectKind54_WaitPose2B), 0, false, kEf, 0, 0, 0, 0x54},
    {"EffectKind54_End", 0x47EA30, 0x59, E2G_CALLS(kCalls47EA30), nullptr, 0, nullptr, 0, E2G_FN(EffectKind54_End), 0, false, kEf, 0, 0, 0, 0x54},
    {"EffectKind55_Run", 0x47EA90, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E2G_FN(EffectKind55_Run), 0, false, kEf, 0, 3, 0, 0x55},
    {"EffectKind55_Start", 0x47EAB0, 0x1C, nullptr, 0, nullptr, 0, nullptr, 0, E2G_FN(EffectKind55_Start), 0, false, kEf, 0, 0, 0, 0x55},
    {"EffectKind55_Tick", 0x47EAD0, 0x52, E2G_CALLS(kCalls47EAD0), nullptr, 0, nullptr, 0, E2G_FN(EffectKind55_Tick), 0, false, kEf, 0, 0, 0, 0x55},
    {"EffectKind55_Finish", 0x47EB30, 0x54, E2G_CALLS(kCalls47EB30), nullptr, 0, nullptr, 0, E2G_FN(EffectKind55_Finish), 0, false, kEf, 0, 0, 0, 0x55},
    {"EffectKind55_DrawTime", 0x47EB90, 0x97, E2G_CALLS(kCalls47EB90), nullptr, 0, nullptr, 0, E2G_FN(EffectKind55_DrawTime), 0, false, kEf, 0, 0, 0, 0x55},
    {"EffectKind55_DrawCount", 0x47EC30, 0x61, E2G_CALLS(kCalls47EC30), nullptr, 0, nullptr, 0, E2G_FN(EffectKind55_DrawCount), 0, false, kEf, 0, 0, 0, 0x55},
    {"EffectKind57_Run", 0x47ECA0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E2G_FN(EffectKind57_Run), 0, false, kEf, 0, 2, 0, 0x57},
    {"EffectKind57_Show", 0x47ECC0, 0x18, E2G_CALLS(kCalls47ECC0), nullptr, 0, nullptr, 0, E2G_FN(EffectKind57_Show), 0, false, kEf, 0, 0, 0, 0x57},
    {"EffectKind57_Release", 0x47ECE0, 0x5, E2G_CALLS(kCalls47ECE0), nullptr, 0, nullptr, 0, E2G_FN(EffectKind57_Release), 0, false, kEf, 0, 0, 0, 0x57},
    {"EffectKind57_DrawRing", 0x47ECF0, 0x1A8, E2G_CALLS(kCalls47ECF0), nullptr, 0, nullptr, 0, E2G_FN(EffectKind57_DrawRing), 0, false, kEf, 0, 0, 0, 0x57},
    {"EffectKind5A_Run", 0x47EEA0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E2G_FN(EffectKind5A_Run), 0, false, kEf, 0, 2, 0, 0x5A},
    {"EffectKind5A_Place", 0x47EEC0, 0x46, E2G_CALLS(kCalls47EEC0), nullptr, 0, nullptr, 0, E2G_FN(EffectKind5A_Place), 0, false, kEf, 0, 0, 0, 0x5A},
    {"EffectKind5A_Draw", 0x47EF10, 0x2B, E2G_CALLS(kCalls47EF10), nullptr, 0, nullptr, 0, E2G_FN(EffectKind5A_Draw), 0, false, kEf, 0, 0, 0, 0x5A},
    {"EffectKind5B_Run", 0x47EF40, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E2G_FN(EffectKind5B_Run), 0, false, kEf, 0, 3, 0, 0x5B},
    {"EffectKind5B_Start", 0x47EF60, 0x48, nullptr, 0, nullptr, 0, nullptr, 0, E2G_FN(EffectKind5B_Start), 0, false, kEf, 0, 0, 0, 0x5B},
    {"EffectKind5B_Glow", 0x47EFB0, 0x33, E2G_CALLS(kCalls47EFB0), nullptr, 0, nullptr, 0, E2G_FN(EffectKind5B_Glow), 0, false, kEf, 0, 0, 0, 0x5B},
    {"EffectKind5B_Fade", 0x47EFF0, 0x48, E2G_CALLS(kCalls47EFF0), nullptr, 0, nullptr, 0, E2G_FN(EffectKind5B_Fade), 0, false, kEf, 0, 0, 0, 0x5B},
    {"EffectKind5B_DrawBeam", 0x47F040, 0x26D, E2G_CALLS(kCalls47F040), nullptr, 0, nullptr, 0, E2G_FN(EffectKind5B_DrawBeam), 0, false, kCa, 0, 0, 0, 0x5B},
    {"EffectKind5D_Run", 0x47F2B0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E2G_FN(EffectKind5D_Run), 0, false, kEf, 0, 5, 0, 0x5D},
    {"EffectKind5E_Run", 0x47F5D0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E2G_FN(EffectKind5E_Run), 0, false, kEf, 0, 4, 0, 0x5E},
    {"EffectKind5F_Run", 0x47FD80, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E2G_FN(EffectKind5F_Run), 0, false, kEf, 0, 10, 0, 0x5F},
};
#undef E2G_FN
#undef E2G_CALLS
#undef E2G_N

enum : unsigned {
    k66Run, k66Start, k66Open, k66Show2, k66Show3, k66Show4, k66Close, k66Message, k66Piece,
    k20Run, k20Start, k20WaitSet, k20Lower, k20WaitClear, k20Raise, k20Draw,
    k15Run, k15Start, k15Begin, k15Pose4, k15Countdown, k15Cue3, k15Cooldown, k15Hit, k15Pose8, k15End,
    k54Run, k54Start, k54Arm, k54Cue, k54Strike, k54Pose2, k54Pose8, k54Rearm, k54Recoil, k54Pose2B, k54End,
    k55Run, k55Start, k55Tick, k55Finish, k55Time, k55Count,
    k57Run, k57Show, k57Release, k57Ring,
    k5ARun, k5APlace, k5ADraw,
    k5BRun, k5BStart, k5BGlow, k5BFade, k5BBeam,
    k5DRun, k5ERun, k5FRun, kCount
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
// A float with a random mantissa and an exponent 2^-17..2^18 (no NaN, no
// infinity): what the projection's screen words and depth are.
void FillFloat(unsigned char* at) {
    const U n = sh::Noise();
    const U bits = (n & 0x807FFFFFu) | ((0x6Eu + (sh::Noise() % 0x24u)) << 23);
    std::memcpy(at, &bits, 4);
}
// EffectGte_ProjectPoint: out[0..2] the projected x, y and depth (floats) - the
// beam's corners in the packet, compared.
U FxProjectPoint(const U* a, U answer) {
    if (Writable(a[1], 12))
        for (unsigned i = 0; i < 3; ++i) FillFloat(P(a[1] + 4 * i));
    return answer;
}
// The harness's packet buffer (scenario_harness.cpp g_packets, 0x800 bytes; its
// Drew keeps 0x40 to spare).
constexpr unsigned kPacketsSize = 0x800;
void Advance(unsigned n) {
    unsigned char* const next = Gfx_PacketNext;
    unsigned char* const base = sh::Packets();
    if (next >= base && next + n + 0x40 <= base + kPacketsSize) Gfx_PacketNext = next + n;
}
// MapView_LinkPrimAt (world_map.cpp): the cursor += size & 0xFF when the row is
// on the map - two times in three here. Sub-kind 0x20's draw and kind 0x5B's
// beam read the cursor again after it.
U FxLink(const U* a, U answer) {
    if (sh::Noise() % 3 != 0) Advance(a[3] & 0xFF);
    return answer;
}
// Sprite_SetAnimation acts on Sprite_Current: kinds 0x15 and 0x54 point it at
// ObjTrio record 1 or a field object for the call, and Field_State with it -
// both logged, so a call on the wrong record shows.
U FxOnCurrent(const U*, U answer) {
    sh::Note(Key(Sprite_Current), Key(Field_State));
    return answer;
}
// Crt_sprintf: up to seven letters and a NUL where the caller's buffer is in
// the regions (the harness's FxSprintf), and the fourth word logged when the
// format is kind 0x55's clock (two numbers; the other caller passes three
// words).
U FxSprintf(const U* a, U) {
    if (a[1] == at::kKind55TimeFormat) sh::Note(a[3]);
    unsigned char* const dst = P(a[0]);
    if (!sh::InRegions(dst, 8)) return 0;
    const unsigned n = sh::Noise() % 8;
    for (unsigned i = 0; i < n; ++i) dst[i] = static_cast<unsigned char>('0' + sh::Noise() % 43);
    dst[n] = 0;
    return n;
}

#define E2G_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
constexpr sh::Answer kG = sh::Answer::kGarbage, kPh = sh::Answer::kPhase;
constexpr U kW = 0xFFFFFFFFu, k16 = 0xFFFFu, k8 = 0xFFu;
const sh::Callee kCallees[] = {
    // the group's own called directly, by name
    {E2G_OURS(EffectKind66_Message), 0, {}, kPh, 0, 0},
    {E2G_OURS(EffectKind18Sub20_Draw), 0, {}, kPh, 0, 0},
    {E2G_OURS(EffectKind55_DrawTime), 0, {}, kPh, 0, 0},
    {E2G_OURS(EffectKind55_DrawCount), 0, {}, kPh, 0, 0},
    {E2G_OURS(EffectKind57_DrawRing), 0, {}, kPh, 0, 0},
    // `and eax, 0xFF` on both: the low bytes read (the callers push immediates)
    {E2G_OURS(EffectKind66_DrawPiece), 2, {k8, k8}, kG, 0, 0},
    // length `and edi, 0xFFFF`, abr `and ecx, 0xFF`: the callers push +6 in a
    // whole register over a leftover (docs/effect_2g.md section 3)
    {E2G_OURS(EffectKind5B_DrawBeam), 2, {k16, k8}, kG, 0, 0},
    // standard rows re-listed: Window_DrawFrame reads x and y as s16 and w and
    // h as bytes (symbols.toml), and kind 0x66 pushes them over leftovers;
    // Window_DrawOutline the same w and h (`and edx, 0xFF`, `mov cl, bl`) - its x
    // and y it passes on whole, and ours computes them whole
    {E2G_OURS(Window_DrawFrame), 4, {k16, k16, k8, k8}, kG, 0, 0},
    {E2G_OURS(Window_DrawOutline), 4, {kW, kW, k8, k8}, kG, 0, 0},
    {E2G_OURS(Sprite_SetAnimation), 1, {k8}, kG, 0, 0, {}, &FxOnCurrent, nullptr, true},
    {"Crt_sprintf", 0x5B9380, 0x5B9380, 3, {kW, kW, kW}, kG, 0, 0, {0, 16, 0}, &FxSprintf, nullptr, true},
    // the point is a stack local (its address differs between the copy and
    // ours): hashed, not logged; out (the packet) logged and filled
    {E2G_OURS(EffectGte_ProjectPoint), 2, {0, kW}, kG, 0, 0, {12, 0}, &FxProjectPoint, nullptr, true},
    // louder: the cursor the draws read again after it
    {E2G_OURS(MapView_LinkPrimAt), 4, {kW, kW, k8, k8}, kG, 0, 0, {}, &FxLink, nullptr, true},
};
#undef E2G_OURS

// The state tables the dispatchers jump through, read in place; each table's
// own length (symbols.toml [[data]], none bounded by a compare).
const sh::DataTable kTables[] = {
    {0x654758, 6}, {0x654798, 5}, {0x6547C0, 10}, {0x6547E8, 15}, {0x654824, 3}, {0x65483C, 2},
    {0x654844, 2}, {0x65484C, 3}, {0x654898, 5}, {0x6548AC, 4}, {0x654904, 10},
};
const std::uint8_t kKinds[] = {0x15, 0x18, 0x54, 0x55, 0x57, 0x5A, 0x5B, 0x5D, 0x5E, 0x5F, 0x66};

// Beyond effect mode's standard regions: kind 0x15's wait byte and area 75's
// cells (the two effect slots among them).
const sh::Region kRegions[] = {
    {at::kKind15Wait, 1},
    {0x93C340, 0x14},
};

// --- the seed ------------------------------------------------------------------------

unsigned char* Rec(unsigned r) { return sh::EffectRecord(r); }

// Every one of the 20 records the disturbance may move Sprite_Current among:
// +0xB inside what the function indexes by it - area 75's two slots for kind
// 0x5B's start, the thirty field objects for the rest.
void Records(unsigned k) {
    for (unsigned r = 0; r < 20; ++r) Rec(r)[0xB] = static_cast<unsigned char>(sh::Next() % (k == k5BStart ? 2 : at::kObjectCount));
}

// The object +0xB names, at the animations and flags the states compare.
void SeedObject(unsigned char* s) {
    unsigned char* const o = Sprite_Objects + s[0xB] * at::kObjectStride;
    SetWord(o + 0x58, PickOf(2, 4, 5, 8, 0, 3, 9, 0x102, 0x108, sh::Next()));
    o[0x4A] = static_cast<unsigned char>(PickOf(1, 1, 0, 2, 0x101, sh::Next()));
}

void Seed(unsigned k) {
    Records(k);
    unsigned char* const s = Sprite_Current;
    // the shared cells: the end bit (a third of the time), the cue, the hit,
    // the timer word's mark, the wait
    Mem(at::kScore)[0] = static_cast<unsigned char>(sh::Next() % 3 == 0 ? (sh::Next() | 0x80) : PickOf(0xF, 0x10, 0x11, 0, 0x7F, sh::Next() & 0x7F));
    Mem(at::kCue)[0] = static_cast<unsigned char>(PickOf(0, 1, 2, 3, 5, 0x81, sh::Next()));
    Mem(at::kHit)[0] = static_cast<unsigned char>(PickOf(1, 1, 0, 2, sh::Next()));
    if (sh::Half()) SetWord(Mem(at::kTimerWord), PickOf(0xFF, 0xFF, 0x1FF, 0xFE));
    Mem(at::kKind15Wait)[0] = static_cast<unsigned char>(PickOf(0, 1, 2, 3, 0xFF, sh::Next()));
    // ObjTrio record 1's animation word and flag
    SetWord(Mem(at::kMember1Pose), PickOf(4, 8, 4, 8, 0, 0x104, 0x108, sh::Next()));
    Mem(at::kMember1PoseDone)[0] = static_cast<unsigned char>(PickOf(1, 1, 0, 0x81, sh::Next()));
    // the frame counts
    s[9] = static_cast<unsigned char>(PickOf(0, 1, 7, 8, 9, 0xFF, sh::Next()));
    switch (k) {
    case k66Open:
    case k66Show2:
    case k66Show3:
    case k66Show4:
    case k66Close:
    case k66Message:
        s[6] = static_cast<unsigned char>(sh::Half() ? 0 : PickOf(1, 0x80, sh::Next()));
        if (sh::Next() % 3 == 0) SetWord(Mem(at::kMessageWord), 0xFFFF);
        Mem(bof3::addr::MsgBoxState)[0] = static_cast<unsigned char>(PickOf(2, 7, 2, 7, 0, 6, sh::Next()));
        break;
    case k20Lower:
        SetLong(s + 0x3C, static_cast<std::int32_t>(PickOf(0xFFFFFE88u, 0xFFFFFE87u, 0xFFFFFE89u, 0xFFFFFE80u, 0x7FFFFFFFu, 0x80000007u, sh::Next())));
        break;
    case k20Raise:
        SetLong(s + 0x3C, static_cast<std::int32_t>(PickOf(0xFFFFFF68u, 0xFFFFFF67u, 0xFFFFFF69u, 0xFFFFFF70u, 0x7FFFFFF8u, 0x80000000u, sh::Next())));
        break;
    case k54Arm:
    case k54Cue:
    case k54Strike:
    case k54Pose2:
    case k54Pose8:
    case k54Rearm:
    case k54Recoil:
    case k54Pose2B:
    case k54End:
        SeedObject(s);
        break;
    case k55Tick:
    case k55Time:
        s[0x5D] = static_cast<unsigned char>(PickOf(0, 1, 0x1D, 0x80, 0x81, 0x7F, 0xFF, sh::Next()));
        s[0x5E] = static_cast<unsigned char>(PickOf(0, 1, 0x1E, 0x80, 0x7F, 0xFF, sh::Next()));
        break;
    case k57Show: Game_AreaNumber = static_cast<unsigned short>(PickOf(2, 2, 3, 0x102, sh::Next())); break;
    case k5BFade: s[0x5D] = static_cast<unsigned char>(PickOf(2, 1, 0x22, 0x21, 0x20, 0x42, 0, sh::Next())); break;
    default: break;
    }
}

// The two draws with arguments: kind 0x66's piece below its four and place
// below its three (past them ours aborts); the beam's length in a word over a
// leftover, its blend byte.
void Args(unsigned k, U* a) {
    switch (k) {
    case k66Piece:
        a[0] = (sh::Next() & 0xFFFFFF00u) | (sh::Next() % at::kKind66PieceCount);
        a[1] = (sh::Next() & 0xFFFFFF00u) | (sh::Next() % at::kKind66PlaceCount);
        break;
    case k5BBeam:
        a[0] = (sh::Next() & 0xFFFF0000u) | PickOf(0, 1, 0x1E, 0x20, 0x7F, 0x80, 0xFFFF, sh::Next() & 0xFFFF);
        a[1] = PickOf(0, 1, 2, 3, 0x100, sh::Next());
        break;
    default: break;
    }
}

// What the states read again after a call, moved by the group's case of the
// harness's disturbance (from its hash only): the frame count +9, kind 0x55's
// clock, the height +0x3C, the beam's cells, the object's animation word,
// Field_State, the area number.
void Disturb(U h) {
    const U v = h >> 8;
    unsigned char* const s = Sprite_Current;
    if (!sh::InRegions(s, 0x80)) return;
    switch (h % 8) {
    case 0: s[9] = static_cast<unsigned char>((v & 1) ? 8u : v >> 1); break;
    case 1: s[0x5D] = static_cast<unsigned char>((v & 1) ? 0u : v >> 1); break;
    case 2: s[0x5E] = static_cast<unsigned char>((v & 1) ? 0xFFu : v >> 1); break;
    case 3: SetLong(s + 0x3C, static_cast<std::int32_t>(v)); break;
    case 4: s[0xA + (v & 1)] = static_cast<unsigned char>(v >> 1); break;
    case 5: SetWord(s + 0x58, (v & 1) ? 4u : v >> 1); break;
    case 6: Field_State = sh::ObjectOf(v); break;
    case 7: Game_AreaNumber = static_cast<unsigned short>((v & 1) ? 2u : v >> 1); break;
    default: break;
    }
}

}  // namespace

void SelfTest() {
    // BOF3X_E2G_ONLY: the clones whose name contains it (a control's run)
    static sh::Clone chosen[kCount];
    static unsigned index[kCount];
    const char* const only = std::getenv("BOF3X_E2G_ONLY");
    unsigned n = 0;
    for (unsigned k = 0; k < kCount; ++k)
        if (!only || !*only || std::strstr(kAll[k].name, only)) {
            index[n] = k;
            chosen[n++] = kAll[k];
        }
    if (n == 0) bof3::Fatal("effect_2g: BOF3X_E2G_ONLY=%s names no clone", only);
    static unsigned* s_index = index;
    sh::Group g = {"effect_2g", chosen, n, kCallees, sizeof kCallees / sizeof kCallees[0], kTables,
                   sizeof kTables / sizeof kTables[0], kRegions, sizeof kRegions / sizeof kRegions[0],
                   [](unsigned k) { Seed(s_index[k]); }, &Disturb, 4000};
    g.args = [](unsigned k, U* a) { Args(s_index[k], a); };
    g.effect = true;
    g.kinds = kKinds;
    g.n_kinds = sizeof kKinds;
    sh::Run(g);
}

}  // namespace effect_2g

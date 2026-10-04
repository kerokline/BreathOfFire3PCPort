// BOF3X_SHADOW=effect_4b: group E4B's 66 functions through the scenario harness
// in effect mode (scenario_harness.h, docs/scenario_harness.md section 8), once
// at start-up. docs/effect_4b.md section 4. BOF3X_E4B_ONLY=<name> runs the
// clones whose name contains it (the controls' speed-up).
//
// The clone table is tools/band_rows.py --group E4B --clones --harness scenario
// (2026-10-03), each extent read again to its last instruction (capstone) and
// the names given, with four changes: 0x48A420 ends at its jmp to
// Effect_Release (the tool's extent 0xA0 ran on into the shared tail 0x48A480,
// whose jmp at +0x2E is a call site here), and three starts the tool printed
// only as callees added as clones of their own - the tail 0x48A480
// (EffectKind9F_TintParty, its own ret), kind 0xA4's state 2 0x48A550 and the
// tail 0x48A560 (EffectKindA4_MarkSprites, its own frame and ret). Shapes:
// every dispatcher, state and void draw kEffect (Sprite_Current one of the 20
// Effect_Objects records, +5 the kind, a dispatcher's byte below its table's
// length); the helpers with arguments kCall; the two that answer al and the
// two that answer a pointer or an angle with their ret_mask.
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_4b.h"
#include "game/effect_4b_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace effect_4b {
namespace {

namespace sh = scenario_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using U = std::uint32_t;

// tools/band_rows.py --group E4B --clones, 2026-10-03 (0x48A420 / 0x48A480 split
// by hand; 0x48A550 and 0x48A560 read by hand).
constexpr sh::CallSite kCalls489030[] = {{0x80, 0x489630}, {0xBD, 0x489630}, {0xFA, 0x489630},
                                         {0x130, 0x489630}, {0x16F, 0x489630}, {0x19A, 0x489630}};
constexpr sh::CallSite kCalls489220[] = {{0xA, 0x494060},  {0x20, 0x494110}, {0x31, 0x494060},
                                         {0x80, 0x587740}, {0xE3, 0x5891F0}, {0x142, 0x489390}};
constexpr sh::JumpTable kTables489220[] = {{0x62, 0x160, 4}};
constexpr sh::CallSite kCalls489390[] = {{0x12, 0x5A79A0},  {0x2A, 0x5A77C0},  {0x39, 0x461E50},  {0x45, 0x5A75F0},
                                         {0x4D, 0x5A7780},  {0xCC, 0x461E50},  {0xD8, 0x5A75F0},  {0xE0, 0x5A7780},
                                         {0x160, 0x461E50}, {0x16F, 0x5A75F0}, {0x177, 0x5A7780}, {0x1F7, 0x461E50},
                                         {0x203, 0x5A75F0}, {0x20B, 0x5A7780}, {0x28A, 0x461E50}};
constexpr sh::CallSite kCalls4896C0[] = {{0xC, 0x4897A0}};
constexpr sh::CallSite kCalls4896F0[] = {{0xB, 0x4897C0}, {0x15, 0x489B40}, {0x36, 0x4897E0}};
constexpr sh::CallSite kCalls489740[] = {{0xB, 0x4897C0}, {0x15, 0x489B40}, {0x36, 0x4897E0}};
constexpr sh::CallSite kCalls489790[] = {{0x0, 0x4897E0}, {0x9, 0x589840}};
constexpr sh::CallSite kCalls4897E0[] = {{0x6, 0x494060}, {0x4C, 0x489AF0}, {0x8F, 0x489890}};
constexpr sh::CallSite kCalls489890[] = {{0x13, 0x5A79A0},  {0x2A, 0x5A77C0},  {0x3D, 0x572FA0},  {0x48, 0x494110},
                                         {0x6C, 0x4941E0},  {0xA6, 0x5A75F0},  {0xAE, 0x5A7780},  {0xD7, 0x5A7A50},
                                         {0xEA, 0x5A7A00},  {0x105, 0x5A7A50}, {0x114, 0x5A7A00}, {0x135, 0x5A7A00},
                                         {0x142, 0x5A7A50}, {0x173, 0x5A7A50}, {0x186, 0x5A7A00}, {0x1A5, 0x5A7A50},
                                         {0x1B4, 0x5A7A00}, {0x1D7, 0x5A7A00}, {0x1E4, 0x5A7A50}, {0x237, 0x572FA0}};
constexpr sh::CallSite kCalls489AF0[] = {{0xD, 0x494110}, {0x1C, 0x494110}, {0x3B, 0x5A7A70}};
constexpr sh::CallSite kCalls489B40[] = {{0x88, 0x489AF0}};
constexpr sh::CallSite kCalls489C00[] = {{0x3A, 0x588F20}};
constexpr sh::CallSite kCalls489C80[] = {{0x0, 0x489CD0}};
constexpr sh::CallSite kCalls489C90[] = {{0x3B, 0x489CD0}};
constexpr sh::CallSite kCalls489CD0[] = {{0xE, 0x5A79A0},  {0x26, 0x5A77C0}, {0x2F, 0x461E50}, {0x3B, 0x5A7740},
                                         {0x82, 0x5A7780}, {0x8A, 0x5A77A0}, {0x93, 0x461E50}};
constexpr sh::CallSite kCalls489D90[] = {{0xA9, 0x489E50}};
constexpr sh::CallSite kCalls489E50[] = {{0x15, 0x48A2A0}, {0x37, 0x5A7A00}, {0x56, 0x5A7A50}, {0x71, 0x48A010}};
constexpr sh::CallSite kCalls489F00[] = {{0x8, 0x48A2A0}, {0x3E, 0x48A160}};
constexpr sh::CallSite kCalls489FA0[] = {{0x8, 0x48A2A0}, {0x3E, 0x48A160}, {0x59, 0x589840}};
constexpr sh::CallSite kCalls48A010[] = {{0x1E, 0x5A7630}, {0x24, 0x5A93F0}, {0x114, 0x5A79A0}, {0x120, 0x5A7780}, {0x136, 0x572FA0}};
constexpr sh::CallSite kCalls48A160[] = {{0x19, 0x5A7630}, {0x1F, 0x5A93F0}, {0x10A, 0x5A79A0}, {0x116, 0x5A7780}, {0x12C, 0x572FA0}};
constexpr sh::CallSite kCalls48A2A0[] = {{0x7F, 0x5A8250}};
constexpr sh::CallSite kCalls48A3C0[] = {{0x3B, 0x489CD0}, {0x40, 0x48A480}};
constexpr sh::CallSite kCalls48A410[] = {{0x0, 0x489CD0}, {0x5, 0x48A480}};
constexpr sh::CallSite kCalls48A420[] = {{0x29, 0x489CD0}, {0x2E, 0x48A480}, {0x58, 0x589840}};
constexpr sh::CallSite kCalls48A4E0[] = {{0x57, 0x489CD0}, {0x5C, 0x48A560}};
constexpr sh::CallSite kCalls48A550[] = {{0x0, 0x489CD0}, {0x5, 0x48A560}};
constexpr sh::CallSite kCalls48A560[] = {{0x1A, 0x537580}, {0x34, 0x588F20}, {0x3D, 0x537580}, {0x53, 0x57C140}, {0x6B, 0x588F20}};
constexpr sh::CallSite kCalls48A610[] = {{0x18, 0x579F00}};
constexpr sh::CallSite kCalls48A650[] = {{0x79, 0x48A730}, {0x91, 0x57C140}};
constexpr sh::CallSite kCalls48A700[] = {{0x15, 0x579F00}, {0x21, 0x589840}};
constexpr sh::CallSite kCalls48A730[] = {{0x14, 0x5A79A0},  {0x2C, 0x5A77C0},  {0x42, 0x572FA0},  {0x47, 0x494060},
                                         {0x53, 0x5A7650},  {0x5B, 0x5A7780},  {0x69, 0x494110},  {0x77, 0x494110},
                                         {0xCF, 0x572FA0},  {0xD8, 0x5B9550},  {0xF1, 0x5B9550},  {0x102, 0x5A7A70},
                                         {0x126, 0x4941E0}, {0x14C, 0x4941E0}, {0x16B, 0x5B9550}, {0x173, 0x5B9550},
                                         {0x188, 0x5B9550}, {0x190, 0x5B9550}, {0x196, 0x48A8E0}};
constexpr sh::CallSite kCalls48A8E0[] = {{0xC, 0x5A7610},  {0x14, 0x5A7780},  {0x69, 0x5A7A50},  {0x82, 0x5A7A00},
                                         {0xB7, 0x5A7A50}, {0xD4, 0x5A7A00},  {0x193, 0x572FA0}, {0x1AE, 0x5A7A50},
                                         {0x1D0, 0x5A7A00}, {0x1F1, 0x5A7A50}, {0x213, 0x5A7A00}, {0x240, 0x572FA0}};
constexpr sh::CallSite kCalls48AB70[] = {{0x28, 0x5891F0}};
constexpr sh::CallSite kCalls48ABC0[] = {{0x2B, 0x5891F0}};
constexpr sh::CallSite kCalls48AC50[] = {{0x1, 0x5B93D2}};
constexpr sh::CallSite kCalls48ACA0[] = {{0x13, 0x587740}};
constexpr sh::CallSite kCalls48AD50[] = {{0x13, 0x587740}};
constexpr sh::CallSite kCalls48ADF0[] = {{0x45, 0x587740}};
constexpr sh::CallSite kCalls48B1D0[] = {{0xE, 0x589840}};
constexpr sh::CallSite kCalls48B1F0[] = {{0x7, 0x589840}};

#define E4B_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define E4B_CALLS(a) a, E4B_N(a)
#define E4B_FN(name) reinterpret_cast<const void*>(&::name)
constexpr sh::Shape kEf = sh::Shape::kEffect;
constexpr sh::Shape kCa = sh::Shape::kCall;
constexpr U kAl = 0xFFu, kEax = 0xFFFFFFFFu;
constexpr U kScratch0 = sh::ArgAt(0, sh::Arg::kScratch);
// {name, base, size, calls, imms, tables, ours, ret_mask, calm, shape, pointers, state_span, sub_span, kind}
const sh::Clone kAll[] = {
    {"EffectKind87_Setup", 0x489030, 0x1B7, E4B_CALLS(kCalls489030), nullptr, 0, nullptr, 0, E4B_FN(EffectKind87_Setup), 0, false, kEf, 0, 0, 0, 0x87},
    {"EffectKind87_FadePanes", 0x4891F0, 0x28, nullptr, 0, nullptr, 0, nullptr, 0, E4B_FN(EffectKind87_FadePanes), 0, false, kEf, 0, 0, 0, 0x87},
    {"EffectKind87_StepPanes", 0x489220, 0x170, E4B_CALLS(kCalls489220), nullptr, 0, E4B_CALLS(kTables489220), E4B_FN(EffectKind87_StepPanes), kAl, false, kEf, 0, 0, 0, 0x87},
    {"EffectKind87_DrawPane", 0x489390, 0x295, E4B_CALLS(kCalls489390), nullptr, 0, nullptr, 0, E4B_FN(EffectKind87_DrawPane), 0, false, kCa, 0, 0, 0, 0x87},
    {"EffectKind87_Midpoint", 0x489630, 0x6D, nullptr, 0, nullptr, 0, nullptr, 0, E4B_FN(EffectKind87_Midpoint), 0, false, kCa, 0, 0, 0, 0x87},
    {"EffectKind88_Run", 0x4896A0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E4B_FN(EffectKind88_Run), 0, false, kEf, 0, 4, 0, 0x88},
    {"EffectKind88_Start", 0x4896C0, 0x24, E4B_CALLS(kCalls4896C0), nullptr, 0, nullptr, 0, E4B_FN(EffectKind88_Start), 0, false, kEf, 0, 0, 0, 0x88},
    {"EffectKind88_Emit", 0x4896F0, 0x50, E4B_CALLS(kCalls4896F0), nullptr, 0, nullptr, 0, E4B_FN(EffectKind88_Emit), 0, false, kEf, 0, 0, 0, 0x88},
    {"EffectKind88_EmitOn", 0x489740, 0x50, E4B_CALLS(kCalls489740), nullptr, 0, nullptr, 0, E4B_FN(EffectKind88_EmitOn), 0, false, kEf, 0, 0, 0, 0x88},
    {"EffectKind88_Fade", 0x489790, 0xF, E4B_CALLS(kCalls489790), nullptr, 0, nullptr, 0, E4B_FN(EffectKind88_Fade), 0, false, kEf, 0, 0, 0, 0x88},
    {"EffectKind88_ClearParticles", 0x4897A0, 0x14, nullptr, 0, nullptr, 0, nullptr, 0, E4B_FN(EffectKind88_ClearParticles), 0, false, kEf, 0, 0, 0, 0x88},
    {"EffectKind88_FindParticle", 0x4897C0, 0x1B, nullptr, 0, nullptr, 0, nullptr, 0, E4B_FN(EffectKind88_FindParticle), kEax, false, kEf, 0, 0, 0, 0x88},
    {"EffectKind88_MoveParticles", 0x4897E0, 0xA8, E4B_CALLS(kCalls4897E0), nullptr, 0, nullptr, 0, E4B_FN(EffectKind88_MoveParticles), kAl, false, kEf, 0, 0, 0, 0x88},
    {"EffectKind88_DrawBurst", 0x489890, 0x256, E4B_CALLS(kCalls489890), nullptr, 0, nullptr, 0, E4B_FN(EffectKind88_DrawBurst), 0, false, kCa, 0, 0, 0, 0x88},
    {"EffectKind88_ScreenAngle", 0x489AF0, 0x44, E4B_CALLS(kCalls489AF0), nullptr, 0, nullptr, 0, E4B_FN(EffectKind88_ScreenAngle), kEax, false, kCa, 0, 0, 0, 0x88},
    {"EffectKind88_InitParticle", 0x489B40, 0x96, E4B_CALLS(kCalls489B40), nullptr, 0, nullptr, 0, E4B_FN(EffectKind88_InitParticle), 0, false, kCa, 0, 0, 0, 0x88},
    {"EffectKind89_Run", 0x489BE0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E4B_FN(EffectKind89_Run), 0, false, kEf, 0, 4, 0, 0x89},
    {"EffectKind89_Start", 0x489C00, 0x78, E4B_CALLS(kCalls489C00), nullptr, 0, nullptr, 0, E4B_FN(EffectKind89_Start), 0, false, kEf, 0, 0, 0, 0x89},
    {"EffectKind89_Hold", 0x489C80, 0x5, E4B_CALLS(kCalls489C80), nullptr, 0, nullptr, 0, E4B_FN(EffectKind89_Hold), 0, false, kEf, 0, 0, 0, 0x89},
    {"EffectKind89_Fade", 0x489C90, 0x40, E4B_CALLS(kCalls489C90), nullptr, 0, nullptr, 0, E4B_FN(EffectKind89_Fade), 0, false, kEf, 0, 0, 0, 0x89},
    {"EffectKind89_DrawTint", 0x489CD0, 0x9D, E4B_CALLS(kCalls489CD0), nullptr, 0, nullptr, 0, E4B_FN(EffectKind89_DrawTint), 0, false, kEf, 0, 0, 0, -1},
    {"EffectKind9D_Run", 0x489D70, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E4B_FN(EffectKind9D_Run), 0, false, kEf, 0, 4, 0, 0x9D},
    {"EffectKind9D_Start", 0x489D90, 0xBF, E4B_CALLS(kCalls489D90), nullptr, 0, nullptr, 0, E4B_FN(EffectKind9D_Start), 0, false, kEf, 0, 0, 0, 0x9D},
    {"EffectKind9D_Pulse", 0x489E50, 0xA7, E4B_CALLS(kCalls489E50), nullptr, 0, nullptr, 0, E4B_FN(EffectKind9D_Pulse), 0, false, kEf, 0, 0, 0, 0x9D},
    {"EffectKind9D_Grow", 0x489F00, 0x9F, E4B_CALLS(kCalls489F00), nullptr, 0, nullptr, 0, E4B_FN(EffectKind9D_Grow), 0, false, kEf, 0, 0, 0, 0x9D},
    {"EffectKind9D_Shrink", 0x489FA0, 0x62, E4B_CALLS(kCalls489FA0), nullptr, 0, nullptr, 0, E4B_FN(EffectKind9D_Shrink), 0, false, kEf, 0, 0, 0, 0x9D},
    {"EffectKind9D_DrawPulse", 0x48A010, 0x142, E4B_CALLS(kCalls48A010), nullptr, 0, nullptr, 0, E4B_FN(EffectKind9D_DrawPulse), 0, false, kCa, 0, 0, 0, 0x9D},
    {"EffectKind9D_DrawGlow", 0x48A160, 0x138, E4B_CALLS(kCalls48A160), nullptr, 0, nullptr, 0, E4B_FN(EffectKind9D_DrawGlow), 0, false, kCa, 0, 0, 0, 0x9D},
    {"EffectKind9D_Project", 0x48A2A0, 0xAB, E4B_CALLS(kCalls48A2A0), nullptr, 0, nullptr, 0, E4B_FN(EffectKind9D_Project), 0, false, kCa, kScratch0, 0, 0, 0x9D},
    {"EffectKind9F_Run", 0x48A350, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E4B_FN(EffectKind9F_Run), 0, false, kEf, 0, 4, 0, 0x9F},
    {"EffectKind9F_Start", 0x48A370, 0x48, nullptr, 0, nullptr, 0, nullptr, 0, E4B_FN(EffectKind9F_Start), 0, false, kEf, 0, 0, 0, -1},
    {"EffectKind9F_Brighten", 0x48A3C0, 0x45, E4B_CALLS(kCalls48A3C0), nullptr, 0, nullptr, 0, E4B_FN(EffectKind9F_Brighten), 0, false, kEf, 0, 0, 0, 0x9F},
    {"EffectKind9F_Hold", 0x48A410, 0xA, E4B_CALLS(kCalls48A410), nullptr, 0, nullptr, 0, E4B_FN(EffectKind9F_Hold), 0, false, kEf, 0, 0, 0, 0x9F},
    {"EffectKind9F_Fade", 0x48A420, 0x5D, E4B_CALLS(kCalls48A420), nullptr, 0, nullptr, 0, E4B_FN(EffectKind9F_Fade), 0, false, kEf, 0, 0, 0, 0x9F},
    {"EffectKind9F_TintParty", 0x48A480, 0x40, nullptr, 0, nullptr, 0, nullptr, 0, E4B_FN(EffectKind9F_TintParty), 0, false, kEf, 0, 0, 0, 0x9F},
    {"EffectKindA4_Run", 0x48A4C0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E4B_FN(EffectKindA4_Run), 0, false, kEf, 0, 3, 0, 0xA4},
    {"EffectKindA4_Brighten", 0x48A4E0, 0x61, E4B_CALLS(kCalls48A4E0), nullptr, 0, nullptr, 0, E4B_FN(EffectKindA4_Brighten), 0, false, kEf, 0, 0, 0, 0xA4},
    {"EffectKindA4_Hold", 0x48A550, 0xA, E4B_CALLS(kCalls48A550), nullptr, 0, nullptr, 0, E4B_FN(EffectKindA4_Hold), 0, false, kEf, 0, 0, 0, 0xA4},
    {"EffectKindA4_MarkSprites", 0x48A560, 0x84, E4B_CALLS(kCalls48A560), nullptr, 0, nullptr, 0, E4B_FN(EffectKindA4_MarkSprites), 0, false, kEf, 0, 0, 0, 0xA4},
    {"EffectKind8A_Run", 0x48A5F0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E4B_FN(EffectKind8A_Run), 0, false, kEf, 0, 3, 0, 0x8A},
    {"EffectKind8A_Block", 0x48A610, 0x31, E4B_CALLS(kCalls48A610), nullptr, 0, nullptr, 0, E4B_FN(EffectKind8A_Block), 0, false, kEf, 0, 0, 0, 0x8A},
    {"EffectKind8A_Lines", 0x48A650, 0xA9, E4B_CALLS(kCalls48A650), nullptr, 0, nullptr, 0, E4B_FN(EffectKind8A_Lines), 0, false, kEf, 0, 0, 0, 0x8A},
    {"EffectKind8A_Unblock", 0x48A700, 0x29, E4B_CALLS(kCalls48A700), nullptr, 0, nullptr, 0, E4B_FN(EffectKind8A_Unblock), 0, false, kEf, 0, 0, 0, 0x8A},
    {"EffectKind8A_DrawSegment", 0x48A730, 0x1A6, E4B_CALLS(kCalls48A730), nullptr, 0, nullptr, 0, E4B_FN(EffectKind8A_DrawSegment), 0, false, kCa, 0, 0, 0, 0x8A},
    {"EffectKind8A_DrawRibbon", 0x48A8E0, 0x24E, E4B_CALLS(kCalls48A8E0), nullptr, 0, nullptr, 0, E4B_FN(EffectKind8A_DrawRibbon), 0, false, kCa, 0, 0, 0, 0x8A},
    {"EffectKind8B_Run", 0x48AB30, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E4B_FN(EffectKind8B_Run), 0, false, kEf, 0, 4, 0, 0x8B},
    {"EffectKind8B_Start", 0x48AB50, 0x14, nullptr, 0, nullptr, 0, nullptr, 0, E4B_FN(EffectKind8B_Start), 0, false, kEf, 0, 0, 0, 0x8B},
    {"EffectKind8B_Wait", 0x48AB70, 0x48, E4B_CALLS(kCalls48AB70), nullptr, 0, nullptr, 0, E4B_FN(EffectKind8B_Wait), 0, false, kEf, 0, 0, 0, 0x8B},
    {"EffectKind8B_Pose", 0x48ABC0, 0x4B, E4B_CALLS(kCalls48ABC0), nullptr, 0, nullptr, 0, E4B_FN(EffectKind8B_Pose), 0, false, kEf, 0, 0, 0, 0x8B},
    {"EffectKind8B_Again", 0x48AC10, 0x11, nullptr, 0, nullptr, 0, nullptr, 0, E4B_FN(EffectKind8B_Again), 0, false, kEf, 0, 0, 0, 0x8B},
    {"EffectKind8C_Run", 0x48AC30, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E4B_FN(EffectKind8C_Run), 0, false, kEf, 0, 23, 0, 0x8C},
    {"EffectKind8C_Start", 0x48AC50, 0x4F, E4B_CALLS(kCalls48AC50), nullptr, 0, nullptr, 0, E4B_FN(EffectKind8C_Start), 0, false, kEf, 0, 0, 0, 0x8C},
    {"EffectKind8C_WaitA", 0x48ACA0, 0x66, E4B_CALLS(kCalls48ACA0), nullptr, 0, nullptr, 0, E4B_FN(EffectKind8C_WaitA), 0, false, kEf, 0, 0, 0, 0x8C},
    {"EffectKind8C_PressA", 0x48AD10, 0x36, nullptr, 0, nullptr, 0, nullptr, 0, E4B_FN(EffectKind8C_PressA), 0, false, kEf, 0, 0, 0, 0x8C},
    {"EffectKind8C_WaitB", 0x48AD50, 0x66, E4B_CALLS(kCalls48AD50), nullptr, 0, nullptr, 0, E4B_FN(EffectKind8C_WaitB), 0, false, kEf, 0, 0, 0, 0x8C},
    {"EffectKind8C_Check", 0x48ADC0, 0x26, nullptr, 0, nullptr, 0, nullptr, 0, E4B_FN(EffectKind8C_Check), 0, false, kEf, 0, 0, 0, 0x8C},
    {"EffectKind8C_Steer", 0x48ADF0, 0x112, E4B_CALLS(kCalls48ADF0), nullptr, 0, nullptr, 0, E4B_FN(EffectKind8C_Steer), 0, false, kEf, 0, 0, 0, 0x8C},
    {"EffectKind8C_SteerWait", 0x48AF10, 0x1E, nullptr, 0, nullptr, 0, nullptr, 0, E4B_FN(EffectKind8C_SteerWait), 0, false, kEf, 0, 0, 0, 0x8C},
    {"EffectKind8C_SteerBack", 0x48AF30, 0x13C, nullptr, 0, nullptr, 0, nullptr, 0, E4B_FN(EffectKind8C_SteerBack), 0, false, kEf, 0, 0, 0, 0x8C},
    {"EffectKind8C_Ready", 0x48B070, 0x1B, nullptr, 0, nullptr, 0, nullptr, 0, E4B_FN(EffectKind8C_Ready), 0, false, kEf, 0, 0, 0, 0x8C},
    {"EffectKind8C_CountA", 0x48B090, 0x6C, nullptr, 0, nullptr, 0, nullptr, 0, E4B_FN(EffectKind8C_CountA), 0, false, kEf, 0, 0, 0, 0x8C},
    {"EffectKind8C_CountPress", 0x48B100, 0x36, nullptr, 0, nullptr, 0, nullptr, 0, E4B_FN(EffectKind8C_CountPress), 0, false, kEf, 0, 0, 0, 0x8C},
    {"EffectKind8C_CountB", 0x48B140, 0x4D, nullptr, 0, nullptr, 0, nullptr, 0, E4B_FN(EffectKind8C_CountB), 0, false, kEf, 0, 0, 0, 0x8C},
    {"EffectKind8C_Judge", 0x48B190, 0x3C, nullptr, 0, nullptr, 0, nullptr, 0, E4B_FN(EffectKind8C_Judge), 0, false, kEf, 0, 0, 0, 0x8C},
    {"EffectKind8C_Pass", 0x48B1D0, 0x13, E4B_CALLS(kCalls48B1D0), nullptr, 0, nullptr, 0, E4B_FN(EffectKind8C_Pass), 0, false, kEf, 0, 0, 0, 0x8C},
    {"EffectKind8C_Fail", 0x48B1F0, 0xC, E4B_CALLS(kCalls48B1F0), nullptr, 0, nullptr, 0, E4B_FN(EffectKind8C_Fail), 0, false, kEf, 0, 0, 0, 0x8C},
};
#undef E4B_FN
#undef E4B_CALLS
#undef E4B_N

enum : unsigned {
    k87Setup, k87Fade, k87Step, k87Draw, k87Mid,
    k88Run, k88Start, k88Emit, k88EmitOn, k88Fade, k88Clear, k88Find, k88Move, k88Burst, k88Angle, k88Init,
    k89Run, k89Start, k89Hold, k89Fade, k89Tint,
    k9DRun, k9DStart, k9DPulse, k9DGrow, k9DShrink, k9DDrawPulse, k9DDrawGlow, k9DProject,
    k9FRun, k9FStart, k9FBrighten, k9FHold, k9FFade, k9FTint,
    kA4Run, kA4Brighten, kA4Hold, kA4Mark,
    k8ARun, k8ABlock, k8ALines, k8AUnblock, k8ASegment, k8ARibbon,
    k8BRun, k8BStart, k8BWait, k8BPose, k8BAgain,
    k8CRun, k8CStart, k8CWaitA, k8CPressA, k8CWaitB, k8CCheck, k8CSteer, k8CSteerWait, k8CSteerBack,
    k8CReady, k8CCountA, k8CCountPress, k8CCountB, k8CJudge, k8CPass, k8CFail, kCount
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

// --- the effects ---------------------------------------------------------------------

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
// A finite float with a fraction, 2^-17..2^18 in size, either sign.
void FillFraction(U at) {
    if (!Writable(at, 4)) return;
    const U bits = (sh::Noise() & 0x807FFFFFu) | ((0x6Eu + (sh::Noise() % 0x24u)) << 23);
    std::memcpy(P(at), &bits, 4);
}
// EffectKind88_FindParticle: the first of the 32 particles whose +3 is 0, or
// null - as the real one; null also a quarter of the time.
U FxFindParticle(const U*, U answer) {
    if (answer % 4 == 0) return 0;
    for (unsigned i = 0; i < at::kParticleCount; ++i) {
        const U r = at::kParticles + at::kParticleStride * i;
        if (P(r)[3] == 0) return r;
    }
    return 0;
}
// EffectKind9D_Project(out): out's two floats filled (fractions); the pointer,
// a local of the caller, not logged.
U FxProject(const U* a, U answer) {
    FillFraction(a[0]);
    FillFraction(a[0] + 4);
    return answer;
}
// Gte_RotTransPers(vertex, sxy, p): the screen point two fractional floats, the
// depth word noise (where writable); re-listed so the vertex is hashed at six
// bytes - EffectKind9D_Project's SVECTOR has a fourth word Capcom never wrote
// (the effect row hashes eight).
U FxRotTransPers(const U* a, U answer) {
    FillFraction(a[1]);
    FillFraction(a[1] + 4);
    if (Writable(a[2], 4)) {
        const U n = sh::Noise();
        std::memcpy(P(a[2]), &n, 4);
    }
    return answer;
}
// EffectGte_ProjectSize(point, size, out): out's two s16, small and positive
// half the time; re-listed to hash both words of the size, which every caller
// here writes ({w, h} of a particle, {0x40, 0}).
U FxProjectSize(const U* a, U answer) {
    if (Writable(a[2], 4)) {
        const U n = sh::Noise();
        const U v = (n & 1) ? (n >> 1) : ((n >> 1) & 0x003F003Fu);
        std::memcpy(P(a[2]), &v, 4);
    }
    return answer;
}
// Scena15_RecordWord: 0x261 a third of the time, 0x308 a third (the two words
// EffectKindA4_MarkSprites tests), else garbage.
U FxRecordWord(const U*, U answer) {
    switch (answer % 3) {
    case 0: return (answer & 0xFFFF0000u) | 0x261u;
    case 1: return (answer & 0xFFFF0000u) | 0x308u;
    default: return answer;
    }
}

#define E4B_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
constexpr sh::Answer kG = sh::Answer::kGarbage, kF = sh::Answer::kFlag, kPh = sh::Answer::kPhase;
constexpr U kW = 0xFFFFFFFFu, k16 = 0xFFFFu, k8 = 0xFFu;
const sh::Callee kCallees[] = {
    // the group's own called directly, by name
    {E4B_OURS(EffectKind87_DrawPane), 1, {kW}, kG, 0, 0, {0x1C}, nullptr, nullptr, true},
    // three bytes (and 0xFF each); the callers push whole constants
    {E4B_OURS(EffectKind87_Midpoint), 3, {k8, k8, k8}, kG, 0, 0},
    {E4B_OURS(EffectKind88_ClearParticles), 0, {}, kPh, 0, 0},
    {E4B_OURS(EffectKind88_FindParticle), 0, {}, kG, 0, 0, {}, &FxFindParticle},
    {E4B_OURS(EffectKind88_InitParticle), 1, {kW}, kG, 0, 0},
    {E4B_OURS(EffectKind88_MoveParticles), 0, {}, kF, 0, 0},
    // the point (12 hashed), the size and turn as their low words, the greys as bytes
    {E4B_OURS(EffectKind88_DrawBurst), 6, {kW, k16, k16, k16, k8, k8}, kG, 0, 0, {12}, nullptr, nullptr, true},
    {E4B_OURS(EffectKind88_ScreenAngle), 2, {kW, kW}, kG, 0, 0, {12, 12}, nullptr, nullptr, true},
    {E4B_OURS(EffectKind89_DrawTint), 0, {}, kPh, 0, 0},
    {E4B_OURS(EffectKind9D_Pulse), 0, {}, kPh, 0, 0},
    {E4B_OURS(EffectKind9D_Project), 1, {0}, kG, 0, 0, {}, &FxProject},
    {E4B_OURS(EffectKind9D_DrawPulse), 2, {kW, kW}, kG, 0, 0},
    {E4B_OURS(EffectKind9D_DrawGlow), 2, {kW, kW}, kG, 0, 0},
    {E4B_OURS(EffectKind9F_TintParty), 0, {}, kPh, 0, 0},
    {E4B_OURS(EffectKindA4_MarkSprites), 0, {}, kPh, 0, 0},
    {E4B_OURS(EffectKind8A_DrawSegment), 2, {kW, kW}, kG, 0, 0, {12, 12}, nullptr, nullptr, true},
    // every argument read as its low s16 (movsx word, and 0xFFFF)
    {E4B_OURS(EffectKind8A_DrawRibbon), 8, {k16, k16, k16, k16, k16, k16, k16, k16}, kG, 0, 0},
    // ours, not in the standard sets: SC15's, the index a word (movzx)
    {E4B_OURS(Scena15_RecordWord), 1, {k16}, kG, 0, 0, {}, &FxRecordWord},
    // re-listed (section 4): the vertex at six bytes, the size at both words
    {E4B_OURS(Gte_RotTransPers), 3, {0, 0, 0}, kG, 0, 0, {6, 0, 0}, &FxRotTransPers, nullptr, true},
    {E4B_OURS(EffectGte_ProjectSize), 3, {0, 0, 0}, kG, 0, 0, {12, 4, 0}, &FxProjectSize, nullptr, true},
};
#undef E4B_OURS

// The state tables the dispatchers jump through, read in place; each table's
// own length (symbols.toml [[data]], none bounded by a compare).
const sh::DataTable kTables[] = {
    {0x654EAC, 4}, {0x654EBC, 4}, {0x654ECC, 4}, {0x654EDC, 4},
    {0x654EEC, 3}, {0x654F04, 3}, {0x654F38, 4}, {0x654F48, 23},
};
const std::uint8_t kKinds[] = {0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x9D, 0x9F, 0xA4};

// Beyond effect mode's standard regions: kind 0x87's and 0x8C's cells, kind
// 0x87's point rows in the image's .data (rows 8..13 written), Draw_OtSlot.
const sh::Region kRegions[] = {
    {at::kKind8CCells, at::kKind8CCellsSize},
    {at::kKind87Points, at::kKind87PointCount * at::kKind87PointStride},
    {0x92BF19, 1},   // Draw_OtSlot
};

// --- the seed ------------------------------------------------------------------------

unsigned char* Rec(unsigned r) { return sh::EffectRecord(r); }
unsigned char* Pane(unsigned i) { return P(at::kPanes + at::kPaneStride * i); }
unsigned char* Particle(unsigned i) { return P(at::kParticles + at::kParticleStride * i); }

// Kind 0x87's six panes: their five pointers on the fourteen points, the case
// 0..4 (4: past the switch), live a third of the time not, the timer at its
// boundaries.
void SeedPanes() {
    for (unsigned i = 0; i < at::kPaneCount; ++i) {
        unsigned char* const r = Pane(i);
        for (unsigned w = 0; w < 0x14; w += 4)
            SetLong(r + w, static_cast<std::int32_t>(at::kPanePoints + at::kPanePointStride * (sh::Next() % at::kPanePointCount)));
        r[0x17] = static_cast<unsigned char>(PickOf(0, 1, 2, 3, 0, 1, 2, 3, 4, sh::Next()));
        r[0x18] = static_cast<unsigned char>(sh::Next() % 3 == 0 ? 0 : PickOf(1, 1, 0x80, sh::Next() | 1));
        r[0x19] = static_cast<unsigned char>(PickOf(0, 1, 2, 0xA, 0xB, 0xF, 0x1E, sh::Next()));
    }
    Mem(at::kSoundIndex)[0] = static_cast<unsigned char>(sh::Next() % at::kKind87SoundCount);
}
// Kind 0x88's 32 particles: live a third of the time not, the life at its
// boundaries, x round extra object 0's x - 0x10000.
void SeedParticles() {
    const U goal = static_cast<U>(Long(Mem(at::kExtraX))) + 0xFFFF0000u;
    for (unsigned i = 0; i < at::kParticleCount; ++i) {
        unsigned char* const r = Particle(i);
        r[3] = static_cast<unsigned char>(sh::Next() % 3 == 0 ? 0 : PickOf(1, 1, 0x80, sh::Next() | 1));
        r[5] = static_cast<unsigned char>(PickOf(1, 2, 0, 0x20, 0xFF, sh::Next()));
        if (sh::Half())
            SetLong(r + 0xC, static_cast<std::int32_t>(goal - PickOf(0, 1, 0x10000, 0xFFFFFFFFu, 0x20000)
                                                       - PickOf(0, static_cast<U>(Long(r + 0x1C)))));
    }
}

void Seed(unsigned k) {
    unsigned char* const s = Sprite_Current;
    // what every record the disturbance may move Sprite_Current to needs: +6
    // 0 for kind 0x8A's draws (its one colour row), +0xB below thirty (the
    // field object kind 0x9D names), +9 and the colour +0x5D..+0x5F at their
    // boundaries
    for (unsigned r = 0; r < 20; ++r) {
        unsigned char* const e = Rec(r);
        e[0xB] = static_cast<unsigned char>(sh::Next() % at::kSpriteCount);
        if (k >= k8ARun && k <= k8ARibbon) e[6] = 0;
        else if (sh::Half()) e[6] = 0;
        e[9] = static_cast<unsigned char>(PickOf(0, 1, 2, 8, 0xC, 0xF8, 0xFF, sh::Next()));
        e[0x5D] = static_cast<unsigned char>(PickOf(0, 1, 2, 3, 0x6E, 0x6F, 0x70, 0x71, 0x7B, 0x7C, 0x7F, 0x80, 0xF7, 0xF8, 0xFF, sh::Next()));
        e[0x5E] = static_cast<unsigned char>(PickOf(0, 1, 2, 0x2F, 0x30, 0x31, 0x7F, 0x80, 0xFF, sh::Next()));
    }
    Field_MemberCount = static_cast<unsigned char>(sh::Next() % 4);
    for (unsigned m = 0; m < at::kObjTrioCount; ++m)
        if (sh::Half()) (ObjTrio + at::kObjTrioStride * m)[0x89] = 7;
    if (sh::Next() % 3 == 0) Field_Request = static_cast<unsigned char>(PickOf(1, 3, 0, 2));
    switch (k) {
    case k87Setup:
    case k87Fade:
    case k87Step:
    case k87Draw:
        SeedPanes();
        break;
    case k88Emit:
    case k88EmitOn:
        if (sh::Half()) SetLong(Mem(at::kExtraX), static_cast<std::int32_t>(k == k88Emit ? 0x3C8000 : 0x408000));
        if (sh::Half()) s[9] = static_cast<unsigned char>(s[9] & ~7u);
        SeedParticles();
        break;
    case k88Fade:
    case k88Clear:
    case k88Find:
    case k88Move:
        SeedParticles();
        break;
    case k9DGrow:
        s[0x5D] = static_cast<unsigned char>(PickOf(0x6E, 0x70, 0x70, 0x6F, sh::Next()));
        s[0x5E] = static_cast<unsigned char>(PickOf(0x2F, 0x30, 0x30, 0x31, sh::Next()));
        break;
    case kA4Mark:
        for (unsigned i = 0; i < at::kSpriteCount; ++i)
            SetWord(Sprite_Objects + at::kSpriteStride * i + 0x2C, sh::Next());
        break;
    case k8BWait:
        Mem(at::kCounter2)[0] = static_cast<unsigned char>(PickOf(0, 1, 2, 0xFF, sh::Next()));
        break;
    case k8BPose:
        if (sh::Often()) SetWord(ObjTrio + 0x58, 0xC);
        if (sh::Often()) ObjTrio[0x4A] = 1;
        break;
    default: break;
    }
    if (k >= k8CRun) {
        // kind 0x8C's cells at their compares
        Mem(at::kTimerA)[0] = static_cast<unsigned char>(PickOf(1, 2, 0, sh::Next()));
        Mem(at::kTimerB)[0] = static_cast<unsigned char>(PickOf(1, 2, 0, sh::Next()));
        Mem(at::kPresses)[0] = static_cast<unsigned char>(PickOf(0x1A, 0x1B, 0x1C, 0, sh::Next()));
        Mem(at::kSteps)[0] = static_cast<unsigned char>(PickOf(6, 7, 8, 0xFF, sh::Next()));
        const U dirs[] = {0x1000, 0x4000, 0x2000, 0x8000, 0x3000, 0x6000, 0x9000, 0xC000, 0x5000, 0, sh::Next()};
        SetWord(Mem(at::kDirection), sh::Pick(dirs, sizeof dirs / sizeof dirs[0]));
        const U target = PickOf(0, 1, 5, 0xFFFF, sh::Next());
        SetWord(Mem(at::kTarget), target);
        SetWord(Mem(at::kCount), target + PickOf(0, 1, 2, 0xFFFF, 0xFFFE, sh::Next()));
        const U pads[] = {0x40, 0x20, 0x1000, 0x4000, 0x2000, 0x8000, 0x3000, 0x6000, 0x9000, 0xC000, 0, sh::Next()};
        Input_Pressed = static_cast<unsigned short>(sh::Pick(pads, sizeof pads / sizeof pads[0]));
    }
}

// Floats for kind 0x9D's quads: fractions, and one time in eight a NaN (quiet
// or signalling) or an infinity, which the FPU adds turn quiet.
U FloatArg() {
    const U n = sh::Next();
    if (n % 8 == 0) {
        static const U kOdd[] = {0x7FC00000u, 0xFFC00000u, 0x7FC00001u, 0x7F800001u, 0xFF800002u, 0x7F800000u, 0xFF800000u, 0};
        return kOdd[(n >> 3) % 8];
    }
    return (sh::Next() & 0x807FFFFFu) | ((0x6Eu + (sh::Next() % 0x24u)) << 23);
}

// The helpers with arguments: a record of the pool each draws or sets up (the
// callers hand them so), the midpoint's rows below fourteen over random upper
// bytes, the particle draw's point a particle's, the angle's two points the
// record's +0x34 and +0xC, the segment's +0xC and +0x18 (as its callers), the
// quad's two floats.
void Args(unsigned k, U* a) {
    switch (k) {
    case k87Draw: a[0] = at::kPanes + at::kPaneStride * (sh::Next() % at::kPaneCount); break;
    case k87Mid:
        for (unsigned i = 0; i < 3; ++i) a[i] = (sh::Next() & 0xFFFFFF00u) | (sh::Next() % at::kKind87PointCount);
        break;
    case k88Burst: a[0] = at::kParticles + at::kParticleStride * (sh::Next() % at::kParticleCount) + 0xC; break;
    case k88Angle:
        a[0] = Key(Sprite_Current + 0x34);
        a[1] = Key(Sprite_Current + 0xC);
        break;
    case k88Init: a[0] = at::kParticles + at::kParticleStride * (sh::Next() % at::kParticleCount); break;
    case k9DDrawPulse:
    case k9DDrawGlow:
        a[0] = FloatArg();
        a[1] = FloatArg();
        break;
    case k8ASegment:
        a[0] = Key(Sprite_Current + 0xC);
        a[1] = Key(Sprite_Current + 0x18);
        break;
    default: break;
    }
}

// What the states read again after a call, moved by the group's case of the
// harness's disturbance (from its hash only): +9, the colour +0x5D..+0x5F,
// +0xB below thirty, Field_MemberCount below four, the counter 0x903849, kind
// 0x8C's count-downs and the pad's pressed word among the ones it tests.
void Disturb(U h) {
    const U v = h >> 8;
    unsigned char* const s = Sprite_Current;
    switch (h % 7) {
    case 0:
        if (sh::InRegions(s, 0x80)) s[9] = static_cast<unsigned char>((v & 1) ? 0u : v >> 1);
        break;
    case 1:
        if (sh::InRegions(s, 0x80)) s[0x5D + v % 3] = static_cast<unsigned char>(v >> 2);
        break;
    case 2:
        if (sh::InRegions(s, 0x80)) s[0xB] = static_cast<unsigned char>((v >> 1) % at::kSpriteCount);
        break;
    case 3: Field_MemberCount = static_cast<unsigned char>(v % 4); break;
    case 4: Mem(at::kCounter2)[0] = static_cast<unsigned char>(v >> 1); break;
    case 5: Mem((v & 1) ? at::kTimerA : at::kTimerB)[0] = static_cast<unsigned char>((v >> 1) % 3); break;
    case 6: {
        static const unsigned short kPads[] = {0x40, 0x20, 0x1000, 0x4000, 0x2000, 0x8000, 0x3000, 0x6000, 0x9000, 0xC000, 0};
        Input_Pressed = kPads[v % (sizeof kPads / sizeof kPads[0])];
        break;
    }
    default: break;
    }
}

}  // namespace

void SelfTest() {
    // BOF3X_E4B_ONLY: the clones whose name contains it (a control's run)
    static sh::Clone chosen[kCount];
    static unsigned index[kCount];
    const char* const only = std::getenv("BOF3X_E4B_ONLY");
    unsigned n = 0;
    for (unsigned k = 0; k < kCount; ++k)
        if (!only || !*only || std::strstr(kAll[k].name, only)) {
            index[n] = k;
            chosen[n++] = kAll[k];
        }
    if (n == 0) bof3::Fatal("effect_4b: BOF3X_E4B_ONLY=%s names no clone", only);
    static unsigned* s_index = index;
    sh::Group g = {"effect_4b", chosen, n, kCallees, sizeof kCallees / sizeof kCallees[0], kTables,
                   sizeof kTables / sizeof kTables[0], kRegions, sizeof kRegions / sizeof kRegions[0],
                   [](unsigned k) { Seed(s_index[k]); }, &Disturb, 4000};
    g.args = [](unsigned k, U* a) { Args(s_index[k], a); };
    g.effect = true;
    g.kinds = kKinds;
    g.n_kinds = sizeof kKinds;
    sh::Run(g);
}

}  // namespace effect_4b

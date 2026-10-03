// BOF3X_SHADOW=effect_5f: group E5F's 49 functions through the scenario harness
// in effect mode (scenario_harness.h, docs/scenario_harness.md section 8), once
// at start-up. docs/effect_5f.md section 4. BOF3X_E5F_ONLY=<name> runs the
// clones whose name contains it (the controls' speed-up).
//
// The clone table is tools/band_rows.py --group E5F --clones --harness scenario
// (2026-10-03), each extent read again to its last instruction (capstone) and
// the names given, with one start added: sub-kind 0x42's draw 0x509A70 (a
// catalog row of no group, its calls read by hand). Shapes: every dispatcher,
// state and void draw kEffect (Sprite_Current one of the 20 Effect_Objects
// records, +5 0x18, a dispatcher's +2 below its table's length); the two draws
// with arguments and the party test kCall.
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_5f.h"
#include "game/effect_5f_callees.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace effect_5f {
namespace {

namespace sh = scenario_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using U = std::uint32_t;

// tools/band_rows.py --group E5F --clones, 2026-10-03 (0x509A70's by hand).
constexpr sh::CallSite kCalls508CE0[] = {{0x138, 0x509030}};
constexpr sh::CallSite kCalls508E20[] = {{0xC7, 0x587740}, {0xD8, 0x509030}};
constexpr sh::CallSite kCalls508F00[] = {{0x1A, 0x509030}};
constexpr sh::CallSite kCalls508F20[] = {{0x64, 0x509030}, {0xBB, 0x509030}};
constexpr sh::CallSite kCalls508FF0[] = {{0x24, 0x587740}, {0x36, 0x509030}};
constexpr sh::CallSite kCalls509030[] = {{0x17, 0x5A77C0}, {0x2D, 0x572FA0}, {0x39, 0x5A75D0}, {0x41, 0x5A77A0},
                                         {0x14B, 0x5720C0}, {0x168, 0x5720C0}, {0x1A5, 0x5720C0}, {0x1C2, 0x5720C0},
                                         {0x20B, 0x5A85F0}, {0x214, 0x5A9290}, {0x236, 0x572A00}, {0x24C, 0x572FA0}};
constexpr sh::CallSite kCalls5092B0[] = {{0x27, 0x57C140}, {0x3D, 0x5094E0}, {0xA8, 0x5094E0}};
constexpr sh::CallSite kCalls509360[] = {{0x64, 0x587740}, {0x75, 0x5094E0}};
constexpr sh::CallSite kCalls5093E0[] = {{0x1A, 0x5094E0}};
constexpr sh::CallSite kCalls509400[] = {{0x59, 0x5094E0}};
constexpr sh::CallSite kCalls509460[] = {{0x24, 0x587740}, {0x36, 0x5094E0}};
constexpr sh::CallSite kCalls5094A0[] = {{0x7, 0x57C140}, {0x21, 0x587740}, {0x32, 0x5094E0}};
constexpr sh::CallSite kCalls5094E0[] = {{0x17, 0x5A77C0}, {0x20, 0x461E50}, {0x2C, 0x5A75D0}, {0x34, 0x5A77A0},
                                         {0xBE, 0x5720C0}, {0xDB, 0x5720C0}, {0x118, 0x5720C0}, {0x138, 0x5720C0},
                                         {0x181, 0x5A85F0}, {0x187, 0x5A9290}, {0x194, 0x572A00}, {0x1A0, 0x461E50}};
constexpr sh::CallSite kCalls5096B0[] = {{0x86, 0x509A70}};
constexpr sh::CallSite kCalls509740[] = {{0xE, 0x57C140}, {0x30, 0x509C00}, {0x4F, 0x572650}, {0x72, 0x587740}, {0x8A, 0x509A70}};
constexpr sh::CallSite kCalls5097E0[] = {{0x21, 0x572650}, {0x39, 0x572790}, {0x5E, 0x509A70}};
constexpr sh::CallSite kCalls509850[] = {{0x86, 0x572ED0}, {0xC6, 0x56FBD0}, {0xE6, 0x572A00}, {0x11D, 0x509A70}};
constexpr sh::CallSite kCalls509980[] = {{0xE, 0x57C140}, {0x35, 0x572650}, {0x4D, 0x572650}, {0x70, 0x587740}, {0x88, 0x509A70}};
constexpr sh::CallSite kCalls509A20[] = {{0x24, 0x572790}, {0x45, 0x509A70}};
constexpr sh::CallSite kCalls509A70[] = {{0x59, 0x5A77C0}, {0x64, 0x572FA0}, {0x70, 0x5A75D0}, {0x78, 0x5A77A0},
                                         {0x133, 0x5A85F0}, {0x139, 0x5A9290}, {0x15F, 0x572A00}, {0x16E, 0x572FA0}};
constexpr sh::CallSite kCalls509CB0[] = {{0x47, 0x5720C0}, {0x150, 0x50A020}};
constexpr sh::CallSite kCalls509E10[] = {{0xC7, 0x587740}, {0xD8, 0x50A020}};
constexpr sh::CallSite kCalls509EF0[] = {{0x1A, 0x50A020}};
constexpr sh::CallSite kCalls509F10[] = {{0x64, 0x50A020}, {0xBB, 0x50A020}};
constexpr sh::CallSite kCalls509FE0[] = {{0x24, 0x587740}, {0x36, 0x50A020}};
constexpr sh::CallSite kCalls50A020[] = {{0x72, 0x5A77C0}, {0x88, 0x572FA0}, {0x94, 0x5A75D0}, {0x9C, 0x5A77A0},
                                         {0x180, 0x5720C0}, {0x1A6, 0x5720C0}, {0x1E7, 0x5720C0}, {0x20D, 0x5720C0},
                                         {0x25A, 0x5A85F0}, {0x263, 0x5A9290}, {0x280, 0x572A00}, {0x296, 0x572FA0}};
constexpr sh::CallSite kCalls50A2F0[] = {{0x47, 0x5720C0}, {0x8E, 0x50A020}};
constexpr sh::CallSite kCalls50A390[] = {{0x17, 0x587740}, {0x27, 0x50A020}};
constexpr sh::CallSite kCalls50A3E0[] = {{0x17, 0x587740}, {0x32, 0x57C140}, {0xA1, 0x587740}, {0xB2, 0x50A020}};
constexpr sh::CallSite kCalls50A4A0[] = {{0x62, 0x50A020}};
constexpr sh::CallSite kCalls50A510[] = {{0x1B, 0x5B93D2}, {0x76, 0x5B93D2}, {0x11D, 0x5A77C0}, {0x126, 0x461E50}, {0x141, 0x5A75D0},
                                         {0x149, 0x5A77A0}, {0x20C, 0x5A85F0}, {0x212, 0x5A9290}, {0x21F, 0x572A00}, {0x22B, 0x461E50}};
constexpr sh::CallSite kCalls50A780[] = {{0x47, 0x5720C0}, {0x143, 0x50AA10}};
constexpr sh::CallSite kCalls50A8D0[] = {{0xC7, 0x587740}, {0xD8, 0x50AA10}};
constexpr sh::CallSite kCalls50A9B0[] = {{0x1A, 0x50AA10}};
constexpr sh::CallSite kCalls50A9D0[] = {{0x24, 0x587740}, {0x36, 0x50AA10}};
constexpr sh::CallSite kCalls50AA10[] = {{0x70, 0x5A77C0}, {0x79, 0x461E50}, {0x99, 0x5A75D0}, {0xA1, 0x5A77A0},
                                         {0x16D, 0x5720C0}, {0x193, 0x5720C0}, {0x1D4, 0x5720C0}, {0x1FA, 0x5720C0},
                                         {0x247, 0x5A85F0}, {0x250, 0x5A9290}, {0x26E, 0x572A00}, {0x277, 0x461E50}};
constexpr sh::CallSite kCalls50ACD0[] = {{0x15, 0x50AD70}};
constexpr sh::CallSite kCalls50ACF0[] = {{0x7, 0x57C140}, {0x21, 0x587740}, {0x33, 0x50AD70}};
constexpr sh::CallSite kCalls50AD30[] = {{0x1B, 0x572650}, {0x23, 0x589840}, {0x2A, 0x50AD70}};
constexpr sh::CallSite kCalls50AD70[] = {{0x55, 0x5A77C0}, {0x6B, 0x572FA0}, {0x77, 0x5A75D0}, {0x7F, 0x5A77A0},
                                         {0xDA, 0x5720C0}, {0x100, 0x5720C0}, {0x13A, 0x5720C0}, {0x151, 0x5720C0},
                                         {0x1CF, 0x5A85F0}, {0x1D5, 0x5A9290}, {0x1E0, 0x572A00}, {0x1F6, 0x572FA0}};

#define E5F_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define E5F_CALLS(a) a, E5F_N(a)
#define E5F_FN(name) reinterpret_cast<const void*>(&::name)
constexpr sh::Shape kEf = sh::Shape::kEffect;
constexpr sh::Shape kCa = sh::Shape::kCall;
// {name, base, size, calls, imms, tables, ours, ret_mask, calm, shape, pointers, state_span, sub_span, kind}
const sh::Clone kAll[] = {
    {"EffectKind18Sub27_Run", 0x508CC0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E5F_FN(EffectKind18Sub27_Run), 0, false, kEf, 0, 0, 5, 0x18},
    {"EffectKind18Sub27_Start", 0x508CE0, 0x13F, E5F_CALLS(kCalls508CE0), nullptr, 0, nullptr, 0, E5F_FN(EffectKind18Sub27_Start), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub27_WaitNear", 0x508E20, 0xDF, E5F_CALLS(kCalls508E20), nullptr, 0, nullptr, 0, E5F_FN(EffectKind18Sub27_WaitNear), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub27_Open", 0x508F00, 0x1F, E5F_CALLS(kCalls508F00), nullptr, 0, nullptr, 0, E5F_FN(EffectKind18Sub27_Open), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub27_WaitFar", 0x508F20, 0xC2, E5F_CALLS(kCalls508F20), nullptr, 0, nullptr, 0, E5F_FN(EffectKind18Sub27_WaitFar), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub27_Close", 0x508FF0, 0x3B, E5F_CALLS(kCalls508FF0), nullptr, 0, nullptr, 0, E5F_FN(EffectKind18Sub27_Close), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub27_Draw", 0x509030, 0x25B, E5F_CALLS(kCalls509030), nullptr, 0, nullptr, 0, E5F_FN(EffectKind18Sub27_Draw), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub28_Run", 0x509290, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E5F_FN(EffectKind18Sub28_Run), 0, false, kEf, 0, 0, 6, 0x18},
    {"EffectKind18Sub28_Start", 0x5092B0, 0xAF, E5F_CALLS(kCalls5092B0), nullptr, 0, nullptr, 0, E5F_FN(EffectKind18Sub28_Start), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub28_WaitNear", 0x509360, 0x7C, E5F_CALLS(kCalls509360), nullptr, 0, nullptr, 0, E5F_FN(EffectKind18Sub28_WaitNear), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub28_Open", 0x5093E0, 0x1F, E5F_CALLS(kCalls5093E0), nullptr, 0, nullptr, 0, E5F_FN(EffectKind18Sub28_Open), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub28_WaitFar", 0x509400, 0x60, E5F_CALLS(kCalls509400), nullptr, 0, nullptr, 0, E5F_FN(EffectKind18Sub28_WaitFar), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub28_Close", 0x509460, 0x3B, E5F_CALLS(kCalls509460), nullptr, 0, nullptr, 0, E5F_FN(EffectKind18Sub28_Close), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub28_WaitFlag", 0x5094A0, 0x37, E5F_CALLS(kCalls5094A0), nullptr, 0, nullptr, 0, E5F_FN(EffectKind18Sub28_WaitFlag), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub28_Draw", 0x5094E0, 0x1AF, E5F_CALLS(kCalls5094E0), nullptr, 0, nullptr, 0, E5F_FN(EffectKind18Sub28_Draw), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub42_Run", 0x509690, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E5F_FN(EffectKind18Sub42_Run), 0, false, kEf, 0, 0, 6, 0x18},
    {"EffectKind18Sub42_Start", 0x5096B0, 0x8F, E5F_CALLS(kCalls5096B0), nullptr, 0, nullptr, 0, E5F_FN(EffectKind18Sub42_Start), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub42_WaitFlag", 0x509740, 0x93, E5F_CALLS(kCalls509740), nullptr, 0, nullptr, 0, E5F_FN(EffectKind18Sub42_WaitFlag), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub42_Raise", 0x5097E0, 0x67, E5F_CALLS(kCalls5097E0), nullptr, 0, nullptr, 0, E5F_FN(EffectKind18Sub42_Raise), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub42_Mark", 0x509850, 0x12B, E5F_CALLS(kCalls509850), nullptr, 0, nullptr, 0, E5F_FN(EffectKind18Sub42_Mark), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub42_WaitFlagBack", 0x509980, 0x91, E5F_CALLS(kCalls509980), nullptr, 0, nullptr, 0, E5F_FN(EffectKind18Sub42_WaitFlagBack), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub42_Lower", 0x509A20, 0x4E, E5F_CALLS(kCalls509A20), nullptr, 0, nullptr, 0, E5F_FN(EffectKind18Sub42_Lower), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub42_Draw", 0x509A70, 0x190, E5F_CALLS(kCalls509A70), nullptr, 0, nullptr, 0, E5F_FN(EffectKind18Sub42_Draw), 0, false, kCa, 0, 0, 0, 0x18},
    {"EffectKind18Sub42_MemberNear", 0x509C00, 0x88, nullptr, 0, nullptr, 0, nullptr, 0, E5F_FN(EffectKind18Sub42_MemberNear), 0xFF, false, kCa, 0, 0, 0, 0x18},
    {"EffectKind18Sub29_Run", 0x509C90, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E5F_FN(EffectKind18Sub29_Run), 0, false, kEf, 0, 0, 5, 0x18},
    {"EffectKind18Sub29_Start", 0x509CB0, 0x157, E5F_CALLS(kCalls509CB0), nullptr, 0, nullptr, 0, E5F_FN(EffectKind18Sub29_Start), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub29_WaitNear", 0x509E10, 0xDF, E5F_CALLS(kCalls509E10), nullptr, 0, nullptr, 0, E5F_FN(EffectKind18Sub29_WaitNear), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub29_Open", 0x509EF0, 0x1F, E5F_CALLS(kCalls509EF0), nullptr, 0, nullptr, 0, E5F_FN(EffectKind18Sub29_Open), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub29_WaitFar", 0x509F10, 0xC2, E5F_CALLS(kCalls509F10), nullptr, 0, nullptr, 0, E5F_FN(EffectKind18Sub29_WaitFar), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub29_Close", 0x509FE0, 0x3B, E5F_CALLS(kCalls509FE0), nullptr, 0, nullptr, 0, E5F_FN(EffectKind18Sub29_Close), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub29_Draw", 0x50A020, 0x2B0, E5F_CALLS(kCalls50A020), nullptr, 0, nullptr, 0, E5F_FN(EffectKind18Sub29_Draw), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub48_Run", 0x50A2D0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E5F_FN(EffectKind18Sub48_Run), 0, false, kEf, 0, 0, 4, 0x18},
    {"EffectKind18Sub48_Start", 0x50A2F0, 0x95, E5F_CALLS(kCalls50A2F0), nullptr, 0, nullptr, 0, E5F_FN(EffectKind18Sub48_Start), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub48_WaitCue", 0x50A390, 0x2C, E5F_CALLS(kCalls50A390), nullptr, 0, nullptr, 0, E5F_FN(EffectKind18Sub48_WaitCue), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub49_Run", 0x50A3C0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E5F_FN(EffectKind18Sub49_Run), 0, false, kEf, 0, 0, 5, 0x18},
    {"EffectKind18Sub49_WaitCue", 0x50A3E0, 0xB9, E5F_CALLS(kCalls50A3E0), nullptr, 0, nullptr, 0, E5F_FN(EffectKind18Sub49_WaitCue), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub49_WaitFar", 0x50A4A0, 0x69, E5F_CALLS(kCalls50A4A0), nullptr, 0, nullptr, 0, E5F_FN(EffectKind18Sub49_WaitFar), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub4B_Run", 0x50A510, 0x245, E5F_CALLS(kCalls50A510), nullptr, 0, nullptr, 0, E5F_FN(EffectKind18Sub4B_Run), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub2A_Run", 0x50A760, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E5F_FN(EffectKind18Sub2A_Run), 0, false, kEf, 0, 0, 5, 0x18},
    {"EffectKind18Sub2A_Start", 0x50A780, 0x14A, E5F_CALLS(kCalls50A780), nullptr, 0, nullptr, 0, E5F_FN(EffectKind18Sub2A_Start), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub2A_WaitNear", 0x50A8D0, 0xDF, E5F_CALLS(kCalls50A8D0), nullptr, 0, nullptr, 0, E5F_FN(EffectKind18Sub2A_WaitNear), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub2A_Open", 0x50A9B0, 0x1F, E5F_CALLS(kCalls50A9B0), nullptr, 0, nullptr, 0, E5F_FN(EffectKind18Sub2A_Open), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub2A_Close", 0x50A9D0, 0x3B, E5F_CALLS(kCalls50A9D0), nullptr, 0, nullptr, 0, E5F_FN(EffectKind18Sub2A_Close), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub2A_Draw", 0x50AA10, 0x29D, E5F_CALLS(kCalls50AA10), nullptr, 0, nullptr, 0, E5F_FN(EffectKind18Sub2A_Draw), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub3C_Run", 0x50ACB0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E5F_FN(EffectKind18Sub3C_Run), 0, false, kEf, 0, 0, 3, 0x18},
    {"EffectKind18Sub3C_Start", 0x50ACD0, 0x1C, E5F_CALLS(kCalls50ACD0), nullptr, 0, nullptr, 0, E5F_FN(EffectKind18Sub3C_Start), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub3C_WaitFlag", 0x50ACF0, 0x3A, E5F_CALLS(kCalls50ACF0), nullptr, 0, nullptr, 0, E5F_FN(EffectKind18Sub3C_WaitFlag), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub3C_Rise", 0x50AD30, 0x31, E5F_CALLS(kCalls50AD30), nullptr, 0, nullptr, 0, E5F_FN(EffectKind18Sub3C_Rise), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub3C_Draw", 0x50AD70, 0x21E, E5F_CALLS(kCalls50AD70), nullptr, 0, nullptr, 0, E5F_FN(EffectKind18Sub3C_Draw), 0, false, kCa, 0, 0, 0, 0x18},
};
#undef E5F_FN
#undef E5F_CALLS
#undef E5F_N

enum : unsigned {
    k27Run, k27Start, k27WaitNear, k27Open, k27WaitFar, k27Close, k27Draw,
    k28Run, k28Start, k28WaitNear, k28Open, k28WaitFar, k28Close, k28WaitFlag, k28Draw,
    k42Run, k42Start, k42WaitFlag, k42Raise, k42Mark, k42WaitBack, k42Lower, k42Draw, k42Member,
    k29Run, k29Start, k29WaitNear, k29Open, k29WaitFar, k29Close, k29Draw,
    k48Run, k48Start, k48WaitCue,
    k49Run, k49WaitCue, k49WaitFar,
    k4BRun,
    k2ARun, k2AStart, k2AWaitNear, k2AOpen, k2AClose, k2ADraw,
    k3CRun, k3CStart, k3CWaitFlag, k3CRise, k3CDraw, kCount
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

// While EffectKind18Sub4B_Run runs (the seed sets it): the x words the original
// takes from a stack word it never writes (docs/effect_5f.md section 6) - v1's
// and v3's of the first quad, all four of the second and third - are levelled
// to 0 on both sides before the vertices are noted (ours writes the quad's own
// x there). g_quad counts the quads since the draw mode's commit (size 0xC),
// which comes once before them in each pass.
bool g_stale = false;
unsigned g_quad = 0;

// Gte_RotTransPers4 (the standard row's behaviour, louder for the stale words):
// the four vertices noted (6 bytes each - the pad is not read), the four screen
// points and the depth filled.
U FxProject(const U* a, U answer) {
    if (g_stale) {
        static const unsigned kFirst[] = {1, 3};
        static const unsigned kAllFour[] = {0, 1, 2, 3};
        const unsigned* const which = g_quad == 0 ? kFirst : kAllFour;
        const unsigned n = g_quad == 0 ? 2u : 4u;
        for (unsigned i = 0; i < n; ++i)
            if (sh::InRegions(P(a[which[i]]), 2)) SetWord(P(a[which[i]]), 0);
        ++g_quad;
    }
    for (unsigned i = 0; i < 4; ++i)
        if (sh::InRegions(P(a[i]), 6)) sh::NoteBytes(P(a[i]), 6);
    for (unsigned i = 4; i < 8; ++i)
        if (Writable(a[i], 8)) sh::FillBytes(P(a[i]), 8);
    if (Writable(a[8], 4)) sh::FillBytes(P(a[8]), 4);
    return answer;
}
// The harness's packet buffer (scenario_harness.cpp g_packets, 0x800 bytes; its
// FxCommitPrim keeps 0x40 to spare).
constexpr unsigned kPacketsSize = 0x800;
// Gfx_CommitPrim: the cursor += size (a byte) while the packet stays in the
// buffer, as the harness's FxCommitPrim; a draw mode's (0xC) restarts g_quad.
U FxCommit(const U* a, U answer) {
    unsigned char* const next = Gfx_PacketNext;
    unsigned char* const base = sh::Packets();
    const unsigned size = a[1] & 0xFF;
    if (size == 0xC) g_quad = 0;
    if (next >= base && next + size + 0x40 <= base + kPacketsSize) Gfx_PacketNext = next + size;
    return answer;
}
// DrawItemPool_Alloc: 0 (the pool empty) a third of the time, else an item of
// the 64 the group's DrawItems region holds (the real pool hands out 1..0x3FF).
U FxAlloc(const U*, U answer) { return answer % 3 == 0 ? 0u : 1u + (answer >> 8) % 0x3F; }

#define E5F_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
constexpr sh::Answer kG = sh::Answer::kGarbage, kPh = sh::Answer::kPhase;
constexpr U kW = 0xFFFFFFFFu, k8 = 0xFFu;
const sh::Callee kCallees[] = {
    // the group's own called directly, by name
    {E5F_OURS(EffectKind18Sub27_Draw), 0, {}, kPh, 0, 0},
    {E5F_OURS(EffectKind18Sub28_Draw), 0, {}, kPh, 0, 0},
    {E5F_OURS(EffectKind18Sub29_Draw), 0, {}, kPh, 0, 0},
    {E5F_OURS(EffectKind18Sub2A_Draw), 0, {}, kPh, 0, 0},
    // the variant (a byte the callers zero-extend) and the height (an s16 they
    // sign-extend): whole words
    {E5F_OURS(EffectKind18Sub42_Draw), 2, {kW, kW}, kG, 0, 0},
    // `test ecx, ecx` on the whole word (the callers push 0 or 1)
    {E5F_OURS(EffectKind18Sub3C_Draw), 1, {kW}, kG, 0, 0},
    // the point | 0x8000, whole; answers a member 0..2 or 0xFF (al, its sign read)
    {E5F_OURS(EffectKind18Sub42_MemberNear), 2, {kW, kW}, sh::Answer::kByte, 0xFF, 0x02},
    // standard rows re-listed: the projection louder for 0x50A510's stale words
    // (the vertices noted by the effect, not hashed by the row); the commit
    // marking the draw mode; the pool's answer inside the region
    {E5F_OURS(Gte_RotTransPers4), 9, {0, 0, 0, 0, 0, 0, 0, 0, 0}, kG, 0, 0, {}, &FxProject, nullptr, true},
    {E5F_OURS(Gfx_CommitPrim), 2, {k8, k8}, kG, 0, 0, {0, 0}, &FxCommit, nullptr, true},
    {E5F_OURS(DrawItemPool_Alloc), 0, {}, kG, 0, 0, {}, &FxAlloc, nullptr, true},
};
#undef E5F_OURS

// The eight state tables the dispatchers jump through, read in place; each
// table's own length (symbols.toml [[data]], none bounded by a compare).
const sh::DataTable kTables[] = {
    {0x65E890, 5}, {0x65E8B0, 6}, {0x65E8C8, 6}, {0x65E968, 5},
    {0x65E980, 4}, {0x65E990, 5}, {0x65E9A8, 5}, {0x65E9D8, 3},
};
const std::uint8_t kKinds[] = {0x18};

// Beyond effect mode's standard regions: the first 64 draw items
// (EffectKind18Sub42_Mark's; MapView_ItemAt's stand-in answers 0..0x3F).
const sh::Region kRegions[] = {
    {0x905E80, 0x40 * at::kDrawItemStride},
};

// --- the seed ------------------------------------------------------------------------

unsigned char* Rec(unsigned r) { return sh::EffectRecord(r); }

// The placement table each start reads by +0x36 (signed), and its records.
unsigned Places(unsigned k) {
    switch (k) {
    case k27Start: return at::kSub27Places;
    case k42Start: return at::kSub42PlaceCount;
    case k29Start:
    case k48Start: return at::kSub29Places;
    case k2AStart: return at::kSub2APlaces;
    default: return 0;
    }
}

// Every one of the 20 records the disturbance may move Sprite_Current among:
// the cell words small (EffectKind18Sub42_Mark indexes the area block by them,
// +0x36 from 2 so that the header's own words are never the cell's), a start's
// +0x36 inside its table, +8 0 or 1 mostly.
void Records(unsigned k) {
    const unsigned places = Places(k);
    for (unsigned r = 0; r < 20; ++r) {
        unsigned char* const e = Rec(r);
        if (places != 0) {
            SetWord(e + 0x36, sh::Next() % places);
        } else if (k == k42Mark) {
            SetWord(e + 0x36, 2 + sh::Next() % 8);
            SetWord(e + 0x3A, sh::Next() % 8);
            e[8] = static_cast<unsigned char>(sh::Next() % 3);
        } else {
            SetWord(e + 0x36, sh::Half() ? sh::Next() % 0x40 : sh::Next());
            SetWord(e + 0x3A, PickOf(0, 0, 1, 0x20, 0xFFFF, sh::Next()));
        }
        if (k != k42Mark) e[8] = static_cast<unsigned char>(PickOf(0, 1, 0, 1, sh::Next()));
    }
}

// The leader near the record's cell half the time: within two cells on each
// axis, the fraction random (the gate's and the far test's boundaries both).
void Leader(std::int32_t x, std::int32_t z) {
    if (!sh::Half()) return;
    const std::int32_t dx = static_cast<std::int32_t>(sh::Next() % 6) - 2;
    const std::int32_t dz = static_cast<std::int32_t>(sh::Next() % 6) - 2;
    const U fx = PickOf(0, 0x8000, 0xFFFF, 0x7FFF, sh::Next() & 0xFFFF);
    const U fz = PickOf(0, 0x8000, 0xFFFF, 0x7FFF, sh::Next() & 0xFFFF);
    SetLong(Mem(at::kLeaderX), static_cast<std::int32_t>((static_cast<U>(x + dx) << 16) | fx));
    SetLong(Mem(at::kLeaderZ), static_cast<std::int32_t>((static_cast<U>(z + dz) << 16) | fz));
}

// The party for EffectKind18Sub42_MemberNear: a count below four, records in
// use or not, their velocities and steps small half the time.
void Party() {
    Field_MemberCount = static_cast<unsigned char>(PickOf(0, 1, 2, 3, 3, 3));
    for (unsigned i = 0; i < at::kObjTrioCount; ++i) {
        unsigned char* const o = sh::ObjectOf(i);
        o[0] = static_cast<unsigned char>(PickOf(1, 1, 0, 0x80, sh::Next()));
        if (sh::Half()) {
            o[9] = static_cast<unsigned char>(sh::Next() % 4);
            SetLong(o + 0xC, static_cast<std::int32_t>(sh::Next() % 0x4000) - 0x2000);
            SetLong(o + 0x10, static_cast<std::int32_t>(sh::Next() % 0x4000) - 0x2000);
        }
        o[0x70] = static_cast<unsigned char>(PickOf(0, 1, 5, 0xFF, sh::Next()));
    }
}

// EffectKind18Sub42_Mark's area block: the header small (FixField keeps it
// below 0x20 x 0x20 and the base below 0x100), the cell words in its first
// 0x700 bytes below 0x100, so the texture dword they index stays in the 8 KiB
// compared; the items' marks 0 half the time.
void MarkArea() {
    unsigned char* const block = Mem(sh::at::kAreaBlock);
    block[0] = static_cast<unsigned char>(1 + sh::Next() % 8);
    block[1] = static_cast<unsigned char>(1 + sh::Next() % 8);
    SetWord(block + 2, sh::Next() % 0x20);
    for (unsigned i = 4; i < 0x700; i += 2) block[i + 1] = 0;
    for (unsigned item = 0; item < 0x40; ++item) {
        unsigned char* const it = Mem(0x905E80 + item * at::kDrawItemStride);
        if (sh::Half()) SetWord(it + at::kItemHalfB, 0);
        if (sh::Half()) SetWord(it + at::kItemHalfA, 0);
    }
}

void Seed(unsigned k) {
    g_stale = k == k4BRun;
    g_quad = 0;
    Records(k);
    unsigned char* const s = Sprite_Current;
    // a start tests the leader against the placement's cell it has just
    // written (its +8 from the old +0x3A being 0), the rest against the record's
    U table = 0;
    switch (k) {
    case k27Start: table = at::kSub27Cell; break;
    case k29Start: table = at::kSub29Cell; break;
    case k2AStart: table = at::kSub2ACell; break;
    default: break;
    }
    if (table != 0) {
        SetWord(s + 0x3A, PickOf(0, 0, 1, sh::Next()));
        const U i = Word(s + 0x36);
        Leader(Mem(table + 2 * i)[0], Mem(table + 2 * i + 1)[0]);
    } else {
        Leader(static_cast<std::int16_t>(Word(s + 0x36)), static_cast<std::int16_t>(Word(s + 0x3A)));
    }
    Cond_ByteFE = static_cast<unsigned char>(PickOf(0x10, 0x20, 0x10, 0x20, 0, sh::Next()));
    Field_Request = static_cast<unsigned char>(PickOf(0, 0, 2, sh::Next()));   // the sounds wait on 0
    Draw_PassFlags = static_cast<unsigned char>(sh::Next() % 3 == 0 ? sh::Next() & ~4u : sh::Next() | 4u);
    switch (k) {
    case k27Open:
    case k28Open:
        SetWord(s + 0x30, PickOf(0xFF20, 0xFF21, 0xFF1F, 0xFF00, 0, 0x8010, 0x7FFF, sh::Next()));
        break;
    case k27Close:
    case k28Close:
        SetWord(s + 0x30, PickOf(0xFFE0, 0xFFDF, 0xFFE1, 0xFF00, 0x7FF0, 0x7FE0, sh::Next()));
        break;
    case k29Open:
        SetWord(s + 0x30, PickOf(0x70, 0x6F, 0x71, 0x7FF0, 0xFFF0, sh::Next()));
        break;
    case k2AOpen:
        SetWord(s + 0x30, PickOf(0xB0, 0xAF, 0xB1, 0x7FF0, 0xFFF0, sh::Next()));
        break;
    case k29Close:
    case k2AClose:
        SetWord(s + 0x30, PickOf(0x10, 0x11, 0xF, 0x8005, 0x7FFF, sh::Next()));
        break;
    case k3CRise:
        SetWord(s + 0x30, PickOf(0xE0, 0xDF, 0xE1, 0x7FF0, 0xFFE0, sh::Next()));
        break;
    case k42Raise:
        SetWord(s + 0x3E, PickOf(0x70, 0x6F, 0x71, 0x7FF0, 0xFFF0, sh::Next()));
        break;
    case k42Lower:
        SetWord(s + 0x3E, PickOf(0x10, 0x11, 0xF, 0x8005, 0x7FFF, sh::Next()));
        break;
    case k42Mark:
        MarkArea();
        break;
    case k42Member:
        Party();
        break;
    case k4BRun:
        s[2] = static_cast<unsigned char>(PickOf(0, 0, 1, sh::Next()));
        s[0xB] = static_cast<unsigned char>(PickOf(0x4B, 0x4C, 0x4B, 0x4C, sh::Next()));
        if (sh::Half()) {   // +0x3A at, below and past +0x30
            const U bound = sh::Next() & 0xFFFF;
            SetWord(s + 0x30, bound);
            SetWord(s + 0x3A, bound + PickOf(0, 1, 0xFFFF, 8, 0xFFF9));
            SetLong(s + 0x10, static_cast<std::int32_t>(PickOf(0, 0, 0x100, sh::Next())));   // +0x3A is +0x38's high word: the drift keeps it
        }
        if (sh::Half()) SetWord(s + 0x34, 0);   // the bound pair's pick
        break;
    default: break;
    }
}

// The kCall functions' arguments: the draw's variant below its two (past them
// ours aborts) and its height; the flat / ground word; the party test's point
// near a member's half the time (the in-reach boundary).
void Args(unsigned k, U* a) {
    switch (k) {
    case k42Draw:
        a[0] = sh::Next() % at::kSub42Variants;
        a[1] = static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(PickOf(0, 0x80, 0x40, sh::Next()))));
        break;
    case k3CDraw: a[0] = PickOf(0, 1, 0, 1, sh::Next()); break;
    case k42Member:
        if (sh::Half()) {
            const unsigned char* const o = sh::ObjectOf(sh::Next() % 3);
            const U t = o[9];
            const U reach = (static_cast<U>(o[0x70]) + 3) << 15;
            const U px = static_cast<U>(Long(o + 0xC)) * t + static_cast<U>(Long(o + 0x34));
            const U pz = static_cast<U>(Long(o + 0x10)) * t + static_cast<U>(Long(o + 0x38));
            a[0] = px + PickOf(reach, reach - 1, 0u - reach, 1u - reach, 0, sh::Next() & 0xFFFF);
            a[1] = pz + PickOf(reach, reach - 1, 0u - reach, 1u - reach, 0, sh::Next() & 0xFFFF);
        }
        break;
    default: break;
    }
}

// What the states read again after a call, moved by the group's case of the
// harness's disturbance (from its hash only): +8 (0 or 1), the slide +0x30,
// the lift +0x3E, Cond_ByteFE, the leader's point, +0xB.
void Disturb(U h) {
    const U v = h >> 8;
    unsigned char* const s = Sprite_Current;
    if (!sh::InRegions(s, 0x80)) return;
    switch (h % 6) {
    case 0: s[8] = static_cast<unsigned char>(v & 1); break;
    case 1: SetWord(s + 0x30, (v & 1) ? 0u : v >> 1); break;
    case 2: SetWord(s + 0x3E, (v & 1) ? 0x80u : v >> 1); break;
    case 3: Cond_ByteFE = static_cast<unsigned char>((v & 1) ? 0x20u : v >> 1); break;
    case 4: SetLong(Mem((v & 1) ? at::kLeaderX : at::kLeaderZ), static_cast<std::int32_t>(v)); break;
    case 5: s[0xB] = static_cast<unsigned char>(v); break;
    default: break;
    }
}

}  // namespace

void SelfTest() {
    // BOF3X_E5F_ONLY: the clones whose name contains it (a control's run)
    static sh::Clone chosen[kCount];
    static unsigned index[kCount];
    const char* const only = std::getenv("BOF3X_E5F_ONLY");
    unsigned n = 0;
    for (unsigned k = 0; k < kCount; ++k)
        if (!only || !*only || std::strstr(kAll[k].name, only)) {
            index[n] = k;
            chosen[n++] = kAll[k];
        }
    if (n == 0) bof3::Fatal("effect_5f: BOF3X_E5F_ONLY=%s names no clone", only);
    static unsigned* s_index = index;
    sh::Group g = {"effect_5f", chosen, n, kCallees, sizeof kCallees / sizeof kCallees[0], kTables,
                   sizeof kTables / sizeof kTables[0], kRegions, sizeof kRegions / sizeof kRegions[0],
                   [](unsigned k) { Seed(s_index[k]); }, &Disturb, 4000};
    g.args = [](unsigned k, U* a) { Args(s_index[k], a); };
    g.effect = true;
    g.kinds = kKinds;
    g.n_kinds = sizeof kKinds;
    sh::Run(g);
    g_stale = false;
}

}  // namespace effect_5f

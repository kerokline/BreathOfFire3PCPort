// BOF3X_SHADOW=effect_5d: group E5D's 52 functions through the scenario harness
// in effect mode (scenario_harness.h, docs/scenario_harness.md section 8), once
// at start-up. docs/effect_5d.md section 4. BOF3X_E5D_ONLY=<name> runs the
// clones whose name contains it (the controls' speed-up); BOF3X_E5D_ROUNDS sets
// the rounds (default 4,000).
//
// The clone table is tools/band_rows.py --group E5D --clones --harness scenario
// (2026-10-03), each extent read again to its last instruction (capstone) and
// the names given, with three starts added: 0x503E50 (code no list has after
// 0x503DE0, E5C's tail-jump target) and the dispatchers 0x505100 and 0x505540
// (EffectKind18_States[28] / [29], part 2 catalog rows no group held). The
// tool's extents stand (0x504900's holds its tail 0x5049D0, reached only by its
// own jmp). Shapes: every dispatcher and state kEffect (kind 0x18, a
// dispatcher's +2 below its table's length); the draws and helpers with
// arguments kCall; EffectKind18Sub1C_OnScreen answers eax (ret_mask whole).
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_5d.h"
#include "game/effect_5d_callees.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace effect_5d {
namespace {

namespace sh = scenario_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using U = std::uint32_t;

// tools/band_rows.py --group E5D --clones, 2026-10-03.
constexpr sh::CallSite kCalls503DE0[] = {{0x2A, 0x572790}, {0x32, 0x589840}, {0x50, 0x5043B0}};
constexpr sh::CallSite kCalls503E50[] = {{0x58, 0x5A7810}, {0x61, 0x461E50}, {0xA2, 0x5A7810}, {0xAB, 0x461E50},
                                         {0xE5, 0x5A7810}, {0xF1, 0x461E50}, {0x129, 0x5A7810}, {0x132, 0x461E50}};
constexpr sh::CallSite kCalls503FA0[] = {{0x92, 0x5A77C0}, {0xB7, 0x572FA0}, {0xC3, 0x5A75D0}, {0xCB, 0x5A77A0}, {0xF2, 0x5B9550},
                                         {0x120, 0x5B9550}, {0x144, 0x5720C0}, {0x189, 0x5B9550}, {0x1A8, 0x5B9550}, {0x1CC, 0x5720C0},
                                         {0x213, 0x5B9550}, {0x232, 0x5B9550}, {0x256, 0x5720C0}, {0x29B, 0x5B9550}, {0x2BA, 0x5B9550},
                                         {0x2DE, 0x5720C0}, {0x34B, 0x5A85F0}, {0x360, 0x5A9170}, {0x398, 0x5A85F0}, {0x39E, 0x5A9290},
                                         {0x3C2, 0x572A00}, {0x3D7, 0x572FA0}};
constexpr sh::CallSite kCalls5043B0[] = {{0x63, 0x5A7810}, {0x6C, 0x461E50}};
constexpr sh::CallSite kCalls504450[] = {{0x11, 0x504570}, {0x21, 0x5047A0}};
constexpr sh::CallSite kCalls5044A0[] = {{0x7, 0x504570}, {0x22, 0x5047A0}};
constexpr sh::CallSite kCalls5044F0[] = {{0x24, 0x504570}};
constexpr sh::CallSite kCalls504550[] = {{0x18, 0x589840}};
constexpr sh::CallSite kCalls504570[] = {{0x36, 0x5720C0}, {0x8F, 0x5A75D0}, {0x97, 0x5A77A0}, {0x17B, 0x5A85F0},
                                         {0x190, 0x5A9170}, {0x1DB, 0x572A00}, {0x1F1, 0x572FA0}};
constexpr sh::CallSite kCalls5047A0[] = {{0x2E, 0x5720C0}, {0x65, 0x5A8250}, {0x7C, 0x5A77C0}, {0x92, 0x572FA0}, {0xB8, 0x5A7750},
                                         {0xC0, 0x5A7780}, {0xC9, 0x5A9110}, {0xD4, 0x5A7A00}, {0xF1, 0x5A7A50}, {0x145, 0x572FA0}};
constexpr sh::CallSite kCalls504900[] = {{0xD7, 0x5A7B90}, {0xED, 0x5A77C0}, {0xF6, 0x461E50}, {0x157, 0x5A8200}, {0x166, 0x5A8060},
                                         {0x17A, 0x5A7D70}, {0x184, 0x5A8DE0}, {0x191, 0x5A8E00}, {0x1A0, 0x5A7630}, {0x1A8, 0x5A77A0},
                                         {0x204, 0x5A7A50}, {0x22C, 0x5A7A50}, {0x261, 0x5A7A50}, {0x2DE, 0x5A7A00}, {0x2FF, 0x5A7A00},
                                         {0x329, 0x5A7A00}, {0x387, 0x5A85F0}, {0x38D, 0x5A93A0}, {0x396, 0x461E50}, {0x3BB, 0x5A77C0},
                                         {0x3C4, 0x461E50}, {0x3CC, 0x5A7BC0}};
constexpr sh::CallSite kCalls504CE0[] = {{0x7E, 0x5A77C0}, {0x9C, 0x572FA0}, {0xD0, 0x5A7750}, {0xD8, 0x5A7780}, {0x10B, 0x5A7A00},
                                         {0x13C, 0x5B9550}, {0x151, 0x5A7A50}, {0x175, 0x5B9550}, {0x184, 0x5A7A00}, {0x19F, 0x5A8E30},
                                         {0x1A4, 0x5A8E90}, {0x1AD, 0x5A90B0}, {0x1B6, 0x5A9110}, {0x1CF, 0x572FA0}};
constexpr sh::CallSite kCalls504F00[] = {{0x24, 0x572650}, {0x41, 0x572650}, {0x5B, 0x57C110}, {0x63, 0x589840}};
constexpr sh::CallSite kCalls504F90[] = {{0x16, 0x505000}};
constexpr sh::CallSite kCalls504FE0[] = {{0x10, 0x589840}};
constexpr sh::CallSite kCalls505000[] = {{0xB, 0x5A75D0}, {0x13, 0x5A77A0}, {0xB9, 0x5A85F0}, {0xBF, 0x5A9290}, {0xD9, 0x572A00}, {0xE5, 0x461E50}};
constexpr sh::CallSite kCalls505120[] = {{0xD, 0x5720C0}, {0x3A, 0x5B93D2}, {0x60, 0x579F00}};
constexpr sh::CallSite kCalls5051A0[] = {{0x25, 0x505480}, {0x33, 0x587740}};
constexpr sh::CallSite kCalls5051E0[] = {{0xC, 0x5052D0}, {0x3B, 0x579F00}};
constexpr sh::CallSite kCalls505240[] = {{0x2, 0x5052D0}, {0x2E, 0x579F00}};
constexpr sh::CallSite kCalls505290[] = {{0xF, 0x5052D0}};
constexpr sh::CallSite kCalls5052D0[] = {{0x86, 0x5A75D0}, {0x8E, 0x5A77A0}, {0xD5, 0x5A8250}, {0xDB, 0x5A92E0}, {0x166, 0x572A00}, {0x17C, 0x572FA0}};
constexpr sh::CallSite kCalls505480[] = {{0x56, 0x5A8250}};
constexpr sh::CallSite kCalls505560[] = {{0x36, 0x5720C0}, {0x5E, 0x5720C0}};
constexpr sh::CallSite kCalls505610[] = {{0x25, 0x505480}, {0x33, 0x587740}};
constexpr sh::CallSite kCalls505650[] = {{0xC, 0x5057D0}};
constexpr sh::CallSite kCalls505690[] = {{0x2D, 0x531F10}, {0x67, 0x535310}, {0x93, 0x534DB0}, {0x9D, 0x534C20}, {0xC4, 0x5057D0}};
constexpr sh::CallSite kCalls505790[] = {{0xF, 0x5057D0}};
constexpr sh::CallSite kCalls5057D0[] = {{0x86, 0x5A75D0}, {0x8E, 0x5A77A0}, {0xEE, 0x5A8250}, {0xF4, 0x5A92E0}, {0x17F, 0x572A00}, {0x199, 0x572FA0}};
constexpr sh::CallSite kCalls5059C0[] = {{0x7, 0x57C140}, {0x15, 0x505A40}, {0x1D, 0x589840}};
constexpr sh::CallSite kCalls5059F0[] = {{0x7, 0x57C140}, {0x35, 0x505A40}, {0x48, 0x589840}};
constexpr sh::CallSite kCalls505AD0[] = {{0x15, 0x57C140}, {0x2A, 0x587740}, {0x32, 0x589810}, {0x86, 0x589810}, {0xCA, 0x505BF0}};
constexpr sh::CallSite kCalls505BB0[] = {{0x1C, 0x589840}, {0x36, 0x505BF0}};
constexpr sh::CallSite kCalls505BF0[] = {{0xAD, 0x5A8250}, {0xE2, 0x5A75D0}, {0xEA, 0x5A77A0}, {0x163, 0x5A92E0}, {0x16B, 0x5A7780}, {0x1CD, 0x572FA0}};
constexpr sh::CallSite kCalls505E20[] = {{0x1D, 0x505E60}};
constexpr sh::CallSite kCalls505E60[] = {{0x25, 0x5A77C0}, {0x2E, 0x461E50}, {0x74, 0x5A75F0}, {0x7A, 0x5A7610}, {0x80, 0x5A7610},
                                         {0x9E, 0x5A7A50}, {0xD4, 0x5A7A00}, {0x107, 0x5A7A50}, {0x13D, 0x5A7A00}, {0x170, 0x5A7A50},
                                         {0x1A6, 0x5A7A00}, {0x1D3, 0x5A7A50}, {0x203, 0x5A7A00}, {0x230, 0x5A7A50}, {0x25D, 0x5A7A00},
                                         {0x291, 0x5A7A50}, {0x2BE, 0x5A7A00}, {0x303, 0x5A7A50}, {0x32F, 0x5A7A50}, {0x35A, 0x5A7A50},
                                         {0x380, 0x5A7A50}, {0x3BF, 0x461E50}, {0x3C8, 0x461E50}, {0x3D1, 0x461E50}, {0x412, 0x5A77C0},
                                         {0x41B, 0x461E50}};
constexpr sh::CallSite kCalls506290[] = {{0x50, 0x5A77C0}, {0x66, 0x572FA0}, {0xC3, 0x5A7750}, {0xCB, 0x5A7780}, {0xEF, 0x5A7A00},
                                         {0x12A, 0x5A7A50}, {0x17F, 0x5A8250}, {0x188, 0x5A9110}, {0x19E, 0x572FA0}, {0x1E8, 0x5A77C0},
                                         {0x1FE, 0x572FA0}};
constexpr sh::CallSite kCalls5064C0[] = {{0x23, 0x57C140}, {0x6E, 0x506640}, {0x73, 0x506860}};
constexpr sh::CallSite kCalls506540[] = {{0xE, 0x57C140}, {0x39, 0x506640}, {0x43, 0x587740}, {0x4D, 0x587740}, {0x55, 0x506860}};
constexpr sh::CallSite kCalls5065A0[] = {{0x28, 0x506640}, {0x2D, 0x506860}};
constexpr sh::CallSite kCalls5065E0[] = {{0xE, 0x57C140}, {0x3B, 0x506640}, {0x45, 0x587740}, {0x4F, 0x587740}, {0x57, 0x506860}};
constexpr sh::CallSite kCalls506640[] = {{0x15, 0x57C140}, {0x1A4, 0x5720C0}};
constexpr sh::CallSite kCalls506860[] = {{0x3C, 0x5A75D0}, {0x44, 0x5A77A0}, {0x103, 0x5A85F0}, {0x109, 0x5A9290}, {0x115, 0x572A00},
                                         {0x15D, 0x461E50}, {0x16A, 0x572FA0}};

#define E5D_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define E5D_CALLS(a) a, E5D_N(a)
#define E5D_FN(name) reinterpret_cast<const void*>(&::name)
constexpr sh::Shape kEf = sh::Shape::kEffect;
constexpr sh::Shape kCa = sh::Shape::kCall;
constexpr U kWhole = 0xFFFFFFFFu;
// {name, base, size, calls, imms, tables, ours, ret_mask, calm, shape, pointers, state_span, sub_span, kind}
const sh::Clone kAll[] = {
    {"EffectKind18Sub17_Close", 0x503DE0, 0x69, E5D_CALLS(kCalls503DE0), nullptr, 0, nullptr, 0, E5D_FN(EffectKind18Sub17_Close), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub17_ScrollTexture", 0x503E50, 0x142, E5D_CALLS(kCalls503E50), nullptr, 0, nullptr, 0, E5D_FN(EffectKind18Sub17_ScrollTexture), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub17_DrawPatch", 0x503FA0, 0x406, E5D_CALLS(kCalls503FA0), nullptr, 0, nullptr, 0, E5D_FN(EffectKind18Sub17_DrawPatch), 0, false, kCa, 0, 0, 0, 0x18},
    {"EffectKind18Sub17_CopyFrame", 0x5043B0, 0x75, E5D_CALLS(kCalls5043B0), nullptr, 0, nullptr, 0, E5D_FN(EffectKind18Sub17_CopyFrame), 0, false, kCa, 0, 0, 0, 0x18},
    {"EffectKind18Sub18_Run", 0x504430, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E5D_FN(EffectKind18Sub18_Run), 0, false, kEf, 0, 0, 4, 0x18},
    {"EffectKind18Sub18_Grow", 0x504450, 0x45, E5D_CALLS(kCalls504450), nullptr, 0, nullptr, 0, E5D_FN(EffectKind18Sub18_Grow), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub18_Hold", 0x5044A0, 0x50, E5D_CALLS(kCalls5044A0), nullptr, 0, nullptr, 0, E5D_FN(EffectKind18Sub18_Hold), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub18_Shrink", 0x5044F0, 0x51, E5D_CALLS(kCalls5044F0), nullptr, 0, nullptr, 0, E5D_FN(EffectKind18Sub18_Shrink), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub18_End", 0x504550, 0x1E, E5D_CALLS(kCalls504550), nullptr, 0, nullptr, 0, E5D_FN(EffectKind18Sub18_End), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub18_DrawColumn", 0x504570, 0x229, E5D_CALLS(kCalls504570), nullptr, 0, nullptr, 0, E5D_FN(EffectKind18Sub18_DrawColumn), 0, false, kCa, 0, 0, 0, 0x18},
    {"EffectKind18Sub18_DrawRing", 0x5047A0, 0x15F, E5D_CALLS(kCalls5047A0), nullptr, 0, nullptr, 0, E5D_FN(EffectKind18Sub18_DrawRing), 0, false, kCa, 0, 0, 0, 0x18},
    {"EffectKind18Sub19_Run", 0x504900, 0x3D9, E5D_CALLS(kCalls504900), nullptr, 0, nullptr, 0, E5D_FN(EffectKind18Sub19_Run), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub68_Draw", 0x504CE0, 0x212, E5D_CALLS(kCalls504CE0), nullptr, 0, nullptr, 0, E5D_FN(EffectKind18Sub68_Draw), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub1B_Run", 0x504F00, 0x69, E5D_CALLS(kCalls504F00), nullptr, 0, nullptr, 0, E5D_FN(EffectKind18Sub1B_Run), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub43_Run", 0x504F70, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E5D_FN(EffectKind18Sub43_Run), 0, false, kEf, 0, 0, 9, 0x18},
    {"EffectKind18Sub43_Glow", 0x504F90, 0x48, E5D_CALLS(kCalls504F90), nullptr, 0, nullptr, 0, E5D_FN(EffectKind18Sub43_Glow), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub43_End", 0x504FE0, 0x16, E5D_CALLS(kCalls504FE0), nullptr, 0, nullptr, 0, E5D_FN(EffectKind18Sub43_End), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub43_DrawQuad", 0x505000, 0xF2, E5D_CALLS(kCalls505000), nullptr, 0, nullptr, 0, E5D_FN(EffectKind18Sub43_DrawQuad), 0, false, kCa, 0, 0, 0, 0x18},
    {"EffectKind18Sub1C_Run", 0x505100, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E5D_FN(EffectKind18Sub1C_Run), 0, false, kEf, 0, 0, 5, 0x18},
    {"EffectKind18Sub1C_Start", 0x505120, 0x71, E5D_CALLS(kCalls505120), nullptr, 0, nullptr, 0, E5D_FN(EffectKind18Sub1C_Start), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub1C_Wait", 0x5051A0, 0x3A, E5D_CALLS(kCalls5051A0), nullptr, 0, nullptr, 0, E5D_FN(EffectKind18Sub1C_Wait), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub1C_Rise", 0x5051E0, 0x56, E5D_CALLS(kCalls5051E0), nullptr, 0, nullptr, 0, E5D_FN(EffectKind18Sub1C_Rise), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub1C_Hold", 0x505240, 0x48, E5D_CALLS(kCalls505240), nullptr, 0, nullptr, 0, E5D_FN(EffectKind18Sub1C_Hold), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub1C_Fall", 0x505290, 0x3F, E5D_CALLS(kCalls505290), nullptr, 0, nullptr, 0, E5D_FN(EffectKind18Sub1C_Fall), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub1C_Draw", 0x5052D0, 0x1A7, E5D_CALLS(kCalls5052D0), nullptr, 0, nullptr, 0, E5D_FN(EffectKind18Sub1C_Draw), 0, false, kCa, 0, 0, 0, 0x18},
    {"EffectKind18Sub1C_OnScreen", 0x505480, 0xBA, E5D_CALLS(kCalls505480), nullptr, 0, nullptr, 0, E5D_FN(EffectKind18Sub1C_OnScreen), kWhole, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub1D_Run", 0x505540, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E5D_FN(EffectKind18Sub1D_Run), 0, false, kEf, 0, 0, 5, 0x18},
    {"EffectKind18Sub1D_Start", 0x505560, 0xAB, E5D_CALLS(kCalls505560), nullptr, 0, nullptr, 0, E5D_FN(EffectKind18Sub1D_Start), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub1D_Wait", 0x505610, 0x3A, E5D_CALLS(kCalls505610), nullptr, 0, nullptr, 0, E5D_FN(EffectKind18Sub1D_Wait), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub1D_Rise", 0x505650, 0x39, E5D_CALLS(kCalls505650), nullptr, 0, nullptr, 0, E5D_FN(EffectKind18Sub1D_Rise), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub1D_Hurt", 0x505690, 0xF3, E5D_CALLS(kCalls505690), nullptr, 0, nullptr, 0, E5D_FN(EffectKind18Sub1D_Hurt), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub1D_Fall", 0x505790, 0x3F, E5D_CALLS(kCalls505790), nullptr, 0, nullptr, 0, E5D_FN(EffectKind18Sub1D_Fall), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub1D_Draw", 0x5057D0, 0x1C4, E5D_CALLS(kCalls5057D0), nullptr, 0, nullptr, 0, E5D_FN(EffectKind18Sub1D_Draw), 0, false, kCa, 0, 0, 0, 0x18},
    {"EffectKind18Sub14_Run", 0x5059A0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E5D_FN(EffectKind18Sub14_Run), 0, false, kEf, 0, 0, 5, 0x18},
    {"EffectKind18Sub14_Start", 0x5059C0, 0x2B, E5D_CALLS(kCalls5059C0), nullptr, 0, nullptr, 0, E5D_FN(EffectKind18Sub14_Start), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub14_Dim", 0x5059F0, 0x4E, E5D_CALLS(kCalls5059F0), nullptr, 0, nullptr, 0, E5D_FN(EffectKind18Sub14_Dim), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub14_ScaleClut", 0x505A40, 0x6A, nullptr, 0, nullptr, 0, nullptr, 0, E5D_FN(EffectKind18Sub14_ScaleClut), 0, false, kCa, 0, 0, 0, 0x18},
    {"EffectKind18Sub1E_Run", 0x505AB0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E5D_FN(EffectKind18Sub1E_Run), 0, false, kEf, 0, 0, 3, 0x18},
    {"EffectKind18Sub14_WaitFlag", 0x505AD0, 0xD3, E5D_CALLS(kCalls505AD0), nullptr, 0, nullptr, 0, E5D_FN(EffectKind18Sub14_WaitFlag), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub14_Fade", 0x505BB0, 0x3F, E5D_CALLS(kCalls505BB0), nullptr, 0, nullptr, 0, E5D_FN(EffectKind18Sub14_Fade), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub14_DrawTiles", 0x505BF0, 0x22A, E5D_CALLS(kCalls505BF0), nullptr, 0, nullptr, 0, E5D_FN(EffectKind18Sub14_DrawTiles), 0, false, kCa, 0, 0, 0, 0x18},
    {"EffectKind18Sub21_Run", 0x505E20, 0x24, E5D_CALLS(kCalls505E20), nullptr, 0, nullptr, 0, E5D_FN(EffectKind18Sub21_Run), 0, false, kEf, 0, 0, 2, 0x18},
    {"EffectKind18Sub21_Step", 0x505E50, 0xA, nullptr, 0, nullptr, 0, nullptr, 0, E5D_FN(EffectKind18Sub21_Step), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub21_DrawSpiral", 0x505E60, 0x42B, E5D_CALLS(kCalls505E60), nullptr, 0, nullptr, 0, E5D_FN(EffectKind18Sub21_DrawSpiral), 0, false, kCa, 0, 0, 0, 0x18},
    {"EffectKind18Sub52_Draw", 0x506290, 0x20E, E5D_CALLS(kCalls506290), nullptr, 0, nullptr, 0, E5D_FN(EffectKind18Sub52_Draw), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub22_Run", 0x5064A0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E5D_FN(EffectKind18Sub22_Run), 0, false, kEf, 0, 0, 5, 0x18},
    {"EffectKind18Sub22_Start", 0x5064C0, 0x78, E5D_CALLS(kCalls5064C0), nullptr, 0, nullptr, 0, E5D_FN(EffectKind18Sub22_Start), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub22_WaitSet", 0x506540, 0x5A, E5D_CALLS(kCalls506540), nullptr, 0, nullptr, 0, E5D_FN(EffectKind18Sub22_WaitSet), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub22_Move", 0x5065A0, 0x32, E5D_CALLS(kCalls5065A0), nullptr, 0, nullptr, 0, E5D_FN(EffectKind18Sub22_Move), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub22_WaitClear", 0x5065E0, 0x5C, E5D_CALLS(kCalls5065E0), nullptr, 0, nullptr, 0, E5D_FN(EffectKind18Sub22_WaitClear), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub22_SetMap", 0x506640, 0x21B, E5D_CALLS(kCalls506640), nullptr, 0, nullptr, 0, E5D_FN(EffectKind18Sub22_SetMap), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub22_Draw", 0x506860, 0x1A1, E5D_CALLS(kCalls506860), nullptr, 0, nullptr, 0, E5D_FN(EffectKind18Sub22_Draw), 0, false, kEf, 0, 0, 0, 0x18},
};
#undef E5D_FN
#undef E5D_CALLS
#undef E5D_N

enum : unsigned {
    k17Close, k17Scroll, k17Patch, k17Frame, k18Run, k18Grow, k18Hold, k18Shrink, k18End, k18Column, k18Ring, k19Run,
    k68Draw, k1BRun, k43Run, k43Glow, k43End, k43Quad, k1CRun, k1CStart, k1CWait, k1CRise, k1CHold, k1CFall, k1CDraw,
    k1COnScreen, k1DRun, k1DStart, k1DWait, k1DRise, k1DHurt, k1DFall, k1DDraw, k14Run, k14Start, k14Dim, k14Clut,
    k1ERun, k14WaitFlag, k14Fade, k14Tiles, k21Run, k21Step, k21Spiral, k52Draw, k22Run, k22Start, k22WaitSet,
    k22Move, k22WaitClear, k22SetMap, k22Draw, kCount
};
static_assert(kCount == sizeof kAll / sizeof kAll[0], "one enum entry a clone, in order");
static_assert(kCount == 52, "the cut's 49 and the three added");

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
unsigned char* Mem(U a) { return sh::Mem(a); }
unsigned char* P(U a) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a)); }
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
// Gte_RotTrans: the three longs of out.
U FxRotTrans(const U* a, U answer) {
    Fill(a[1], 12);
    return answer;
}
// Gte_RotMatrix / Gte_MulMatrix0: the nine shorts of the matrix written.
U FxMatrix1(const U* a, U answer) {
    Fill(a[1], 18);
    return answer;
}
U FxMatrix2(const U* a, U answer) {
    Fill(a[2], 18);
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
// rotation hashed by the deref; the padding word +0x12, never written, neither).
U FxSetTrans(const U* a, U answer) {
    if (Writable(a[0], 0x20)) sh::NoteBytes(P(a[0] + 0x14), 12);
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
// on the map - two times in three here. dy's low byte only is read (signed
// char): EffectKind18Sub68_Draw pushes it as a sete over a register's and a
// stack word's leftovers (docs/effect_5d.md section 4).
U FxLink(const U* a, U answer) {
    if (sh::Noise() % 3 != 0) Advance(a[3] & 0xFF);
    return answer;
}

// Gte_RotTransPers: the screen point two floats (as the effect-mode row), here
// a third of the time at or just past EffectKind18Sub1C_OnScreen's bounds
// (-20.0, 340.0) or a quiet NaN, so both its answers and the x87's unordered
// compare show; the rest the row's fractional floats.
U FxPers1(const U* a, U answer) {
    static const U kNear[] = {0xC1A00000u, 0xC1A00001u, 0xC19FFFFFu, 0x43AA0000u, 0x43AA0001u, 0x43A9FFFFu, 0x7FC00000u, 0x43200000u};
    for (unsigned i = 0; i < 2; ++i) {
        const U at = a[1] + 4 * i;
        if (sh::Noise() % 3 == 0 && Writable(at, 4)) {
            const U bits = kNear[sh::Noise() % (sizeof kNear / sizeof kNear[0])];
            std::memcpy(P(at), &bits, 4);
        } else {
            FillFloat(at);
        }
    }
    Fill(a[2], 4);
    return answer;
}

// AreaMap_Elevation: half the time one of four heights (the callers read ax),
// else the answer's garbage.
U FxElevation(const U*, U answer) {
    static const U kHeights[] = {0, 1, 0xFFFF, 0x100};
    if (answer % 2 == 0) return (answer & 0xFFFF0000u) | kHeights[(answer >> 8) % 4];
    return answer;
}

// Effect_FindFree as the effect-mode row (scenario_harness.cpp FxFindFree: none
// a quarter of the time, else a free record from a start the answer picks), but
// never Sprite_Current's own: in the game the running record is in use, and the
// disturbance can leave Sprite_Current on a free one, whose spawn would then
// overwrite the place word sub-kind 0x14's spawn reads again after it
// (docs/effect_5d.md section 4).
U FxFindFree(const U*, U answer) {
    const U high = answer & 0xFFFFFF00u;
    if ((answer >> 8) % 4 == 0) return high | 0xFF;
    const unsigned from = (answer >> 12) % at::kEffectCount;
    for (unsigned i = 0; i < at::kEffectCount; ++i) {
        const unsigned k = (from + i) % at::kEffectCount;
        unsigned char* const e = sh::EffectRecord(k);
        if (e[0] == 0 && e != Sprite_Current) return high | k;
    }
    return high | 0xFF;
}

#define E5D_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
constexpr sh::Answer kG = sh::Answer::kGarbage, kPh = sh::Answer::kPhase, kB = sh::Answer::kBool;
constexpr U kW = 0xFFFFFFFFu, k8 = 0xFFu;
const sh::Callee kCallees[] = {
    // the group's own called directly, by name (every caller pushes whole words)
    {E5D_OURS(EffectKind18Sub17_CopyFrame), 1, {kW}, kG, 0, 0},
    {E5D_OURS(EffectKind18Sub18_DrawColumn), 2, {kW, kW}, kG, 0, 0},
    {E5D_OURS(EffectKind18Sub18_DrawRing), 1, {kW}, kG, 0, 0},
    {E5D_OURS(EffectKind18Sub43_DrawQuad), 2, {kW, kW}, kG, 0, 0},
    {E5D_OURS(EffectKind18Sub1C_Draw), 1, {kW}, kG, 0, 0},
    {E5D_OURS(EffectKind18Sub1D_Draw), 1, {kW}, kG, 0, 0},
    {E5D_OURS(EffectKind18Sub1C_OnScreen), 0, {}, kB, 0, 0},
    {E5D_OURS(EffectKind18Sub14_ScaleClut), 1, {kW}, kG, 0, 0},
    {E5D_OURS(EffectKind18Sub14_DrawTiles), 2, {kW, kW}, kG, 0, 0},
    {E5D_OURS(EffectKind18Sub21_DrawSpiral), 1, {kW}, kG, 0, 0},
    {E5D_OURS(EffectKind18Sub22_SetMap), 0, {}, kPh, 0, 0},
    {E5D_OURS(EffectKind18Sub22_Draw), 0, {}, kPh, 0, 0},
    // standard rows re-listed: the stack pointers never logged (the originals'
    // locals and ours' lie at different addresses), the outs filled
    {E5D_OURS(Gte_RotTrans), 2, {0, 0}, kG, 0, 0, {6, 0}, &FxRotTrans, nullptr, true},
    {E5D_OURS(Gte_RotMatrix), 2, {0, 0}, kG, 0, 0, {6, 0}, &FxMatrix1, nullptr, true},
    {E5D_OURS(Gte_MulMatrix0), 3, {kW, 0, 0}, kG, 0, 0, {18, 18, 0}, &FxMatrix2, nullptr, true},
    {E5D_OURS(Gte_RotTransPers4), 9, {0, 0, 0, 0, 0, 0, 0, 0, 0}, kG, 0, 0, {6, 6, 6, 6, 0, 0, 0, 0, 0}, &FxPers4, nullptr, true},
    {E5D_OURS(Gte_SetTransMatrix), 1, {0}, kG, 0, 0, {18}, &FxSetTrans, nullptr, true},
    {E5D_OURS(Gte_SetRotMatrix), 1, {0}, kG, 0, 0, {18}, nullptr, nullptr, true},
    {E5D_OURS(Gte_RotTransPers), 3, {kW, 0, 0}, kG, 0, 0, {8, 0, 0}, &FxPers1, nullptr, true},
    // louder: the cursor the draws read again after it; dy's byte
    {E5D_OURS(MapView_LinkPrimAt), 4, {kW, kW, k8, k8}, kG, 0, 0, {}, &FxLink, nullptr, true},
    // AreaMap_Elevation answering a few heights half the time, so two calls can agree
    // (EffectKind18Sub1D_Start compares one with the other: a > or >= shows)
    {E5D_OURS(AreaMap_Elevation), 2, {kW, kW}, kG, 0, 0, {}, &FxElevation},
    {E5D_OURS(Effect_FindFree), 0, {}, sh::Answer::kByte, 0xFF, 0x13, {}, &FxFindFree, nullptr, true},
};
#undef E5D_OURS

// The sub-state tables the dispatchers jump through, read in place; each
// table's own length (symbols.toml [[data]], none bounded by a compare). Two
// tables lie inside others (EffectKind18Sub1C_States in Sub43's from its entry
// 4, Sub1E's in Sub14's from its entry 2): the outer one is swapped, which
// swaps the inner one's cells with it (a table listed twice would be put back
// from the recorders).
const sh::DataTable kTables[] = {
    {0x65E27C, 4}, {0x65E2B8, 9}, {0x65E2E0, 5}, {0x65E2F8, 5}, {0x65E328, 2}, {0x65E334, 5},
};
const std::uint8_t kKinds[] = {0x18};

// Beyond effect mode's standard regions: sub-kind 0x14's CLUT strip and the
// strip it writes.
const sh::Region kRegions[] = {
    {at::kClutStrip, 2 * at::kClutStripWords},
    {at::kClutStripOut, 2 * at::kClutStripWords},
};

// --- the seed ------------------------------------------------------------------------

unsigned char* Rec(unsigned r) { return sh::EffectRecord(r); }

bool Sub22(unsigned k) { return k >= k22Run && k <= k22Draw; }
// The functions that index a table by the place word +0x36 (sub-kinds 0x14 /
// 0x1E and 0x22) or copy its byte into +0xB (0x19); the rest leave it the
// harness's random x cell, so the draws see points far from the map's origin.
bool Placed(unsigned k) { return Sub22(k) || k == k19Run || (k >= k14Run && k <= k14Tiles); }

// A frame count the columns take whole or >> 2 without reaching 8 (their
// divisor 8 - rise; past it the original faults), the boundaries kept.
unsigned char SafeCount(U v) {
    unsigned char n = static_cast<unsigned char>(v);
    if (n == 8 || (n >> 2) == 8) n = static_cast<unsigned char>(n ^ 0x40);
    return n;
}

// Every one of the 20 records the disturbance may move Sprite_Current among:
// the place word +0x36 below the places the function's tables hold (two, or
// four for sub-kind 0x22), +0xB below sub-kind 0x19's two, +9 a safe count
// (below 40 for sub-kind 0x17's close, whose frame index 7 - +9 / 5 goes below
// 0 past it).
void Records(unsigned k) {
    const unsigned places = Sub22(k) ? at::kSub22Count : 2;
    for (unsigned r = 0; r < 20; ++r) {
        unsigned char* const e = Rec(r);
        if (Placed(k)) SetWord(e + 0x36, sh::Next() % places);
        e[0xB] = static_cast<unsigned char>(sh::Next() % at::kSub19Count);
        e[9] = k == k17Close ? static_cast<unsigned char>(sh::Next() % 40) : SafeCount(sh::Next());
    }
}

void Seed(unsigned k) {
    Records(k);
    unsigned char* const s = Sprite_Current;
    // the frame count at the compares' boundaries (each compare's value and the
    // one below it: the states step +9 before they compare)
    s[9] = SafeCount(PickOf(0, 1, 5, 6, 7, 9, 0xE, 0xF, 0x10, 0x17, 0x18, 0x19, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x6B, 0x6C, 0x6D,
                            0x7E, 0x7F, 0x80, 0xFF, sh::Next()));
    if (k == k17Close) s[9] = static_cast<unsigned char>(PickOf(0, 5, 10, 15, 19, 20, 21, 25, 1, sh::Next() % 40));
    // the sub-state compared (sub-kind 0x19 tests it against 0, 0x22's map and
    // draw against 2); a dispatcher's is the harness's, below its table's length
    if (k == k19Run || k == k22SetMap || k == k22Draw) s[2] = static_cast<unsigned char>(PickOf(0, 2, 1, 3, 4, sh::Next()));
    Draw_PassFlags = static_cast<unsigned char>(sh::Half() ? (Draw_PassFlags | 4) : (Draw_PassFlags & ~4u));
    Field_Request = static_cast<unsigned char>(PickOf(0, 0, 1, sh::Next()));
    Mem(at::kCounter)[0] = static_cast<unsigned char>(PickOf(0xE, 0x1C, 0xD, 0xF, 0x1B, sh::Next()));
    // the camera's cell near each place half the time (sub-kinds 0x19, 0x68, 0x52)
    if (sh::Half()) {
        std::int32_t cx = 0, cz = 0;
        switch (k) {
        case k19Run:
            cx = Mem(at::kSub19X)[s[0xB] % at::kSub19Count] - 6;
            cz = Mem(at::kSub19Z)[s[0xB] % at::kSub19Count] - 6;
            break;
        case k68Draw: cx = 0x2D; cz = 0x23; break;
        default: cx = static_cast<std::int16_t>(Word(s + 0x36)); cz = static_cast<std::int16_t>(Word(s + 0x3A)); break;
        }
        const std::int32_t off[] = {0, 10, -10, 11, -11, 5, -3, 20, -20, 21};
        SetWord(Mem(at::kCameraX), static_cast<U>(cx + off[sh::Next() % 10]));
        SetWord(Mem(at::kCameraZ), static_cast<U>(cz + off[sh::Next() % 10]));
    }
    // the party members' actor bytes (Field_ActorStates' index, read unchecked)
    for (unsigned m = 0; m < at::kMemberCount; ++m)
        if (sh::Half()) ObjTrio[at::kMemberStride * m + 0x148] = static_cast<unsigned char>(sh::Next() % 8);
    switch (k) {
    case k22SetMap:
    case k22Start:
    case k22WaitSet:
    case k22Move:
    case k22WaitClear: {
        // the height bytes' base: AreaMap_Header + it * 4 + a cell stays in the area
        // block the harness compares (the widest cell 0x22 + 0x1F * 0x1F)
        AreaMap_HeightBase = static_cast<unsigned short>(sh::Next() % 0x700);
        break;
    }
    case k1BRun: s[9] = static_cast<unsigned char>(PickOf(0, 4, 0xE, 0xD, 0xF, 3, 1, 5, sh::Next())); break;
    case k1CWait:
    case k1DWait: s[9] = static_cast<unsigned char>(PickOf(1, 1, 0, 2, sh::Next())); break;
    case k1DHurt:
        // the direction toward and along both axes
        SetLong(s + 0xC, static_cast<std::int32_t>(PickOf(0, 1, 0xFFFFFFFFu, sh::Next())));
        SetLong(s + 0x10, static_cast<std::int32_t>(PickOf(0, 1, 0xFFFFFFFFu, sh::Next())));
        break;
    case k1CStart:
    case k1DStart:
        SetWord(s + 0x3E, PickOf(0, 1, 0xFFFF, 0x7FFF, 0x8000, sh::Next()));
        break;
    default: break;
    }
}

// The helpers' arguments: the patch's variant below its three, the frame below
// its ten, the columns' rise never 8 (their divisor), the rest whole words at
// the compares' boundaries.
void Args(unsigned k, U* a) {
    switch (k) {
    case k17Patch: a[0] = PickOf(0, 1, 2, sh::Next() % 3); break;
    case k17Frame: a[0] = sh::Next() % at::kFrameCount; break;
    case k18Column:
        a[0] = PickOf(0, 1, 7, 0x10, 0xFFFFFFFFu, sh::Next());
        a[1] = PickOf(0x80, 0, 0x7C, sh::Next());
        break;
    case k18Ring: a[0] = PickOf(0, 1, 7, 0xF, 0x10, sh::Next()); break;
    case k43Quad:
        a[0] = PickOf(0, 1, 0x6C, 0xFF, sh::Next());
        a[1] = PickOf(0, 7, 8, 0xF8, sh::Next());
        break;
    case k1CDraw:
    case k1DDraw: {
        U v = PickOf(0, 1, 2, 7, 9, 0x3F, 0xFFFFFFFFu, sh::Next());
        if (v == 8) v = 9;
        a[0] = v;
        break;
    }
    case k14Clut: a[0] = PickOf(0x40, 0x80, 0, 0x7F, 0x41, sh::Next()); break;
    case k14Tiles:
        a[0] = PickOf(0x80, 0, 1, 0x7F, 0xFFFFFF81u, sh::Next());
        a[1] = 0;
        break;
    case k21Spiral: a[0] = PickOf(0, 0x80, 0x7F, 0x81, 0xFF, sh::Next()); break;
    default: break;
    }
}

// What the states read again after a call, moved by the group's case of the
// harness's disturbance (from its hash only): the frame count (a safe one),
// Field_Request, the counter 0x903848, the height word +0x3E, the step +0x3A,
// Draw_PassFlags' bit 2.
void Disturb(U h) {
    const U v = h >> 8;
    unsigned char* const s = Sprite_Current;
    if (!sh::InRegions(s, 0x80)) return;
    switch (sh::DisturbCase(h, 6)) {
    case 0: s[9] = SafeCount((v & 1) ? 7u : v >> 1); break;
    case 1: Field_Request = static_cast<unsigned char>((v & 1) ? 0u : v >> 1); break;
    case 2: Mem(at::kCounter)[0] = static_cast<unsigned char>((v & 1) ? 0xEu : v >> 1); break;
    case 3: SetWord(s + 0x3E, v); break;
    case 4: SetWord(s + 0x3A, (v & 1) ? 8u : v >> 1); break;
    case 5: Draw_PassFlags = static_cast<unsigned char>(Draw_PassFlags ^ 4); break;
    default: break;
    }
}

}  // namespace

void SelfTest() {
    // BOF3X_E5D_ONLY: the clones whose name contains it (a control's run)
    static sh::Clone chosen[kCount];
    static unsigned index[kCount];
    const char* const only = std::getenv("BOF3X_E5D_ONLY");
    unsigned n = 0;
    for (unsigned k = 0; k < kCount; ++k)
        if (!only || !*only || std::strstr(kAll[k].name, only)) {
            index[n] = k;
            chosen[n++] = kAll[k];
        }
    if (n == 0) bof3::Fatal("effect_5d: BOF3X_E5D_ONLY=%s names no clone", only);
    static unsigned* s_index = index;
    const char* const rounds = std::getenv("BOF3X_E5D_ROUNDS");
    sh::Group g = {"effect_5d", chosen, n, kCallees, sizeof kCallees / sizeof kCallees[0], kTables,
                   sizeof kTables / sizeof kTables[0], kRegions, sizeof kRegions / sizeof kRegions[0],
                   [](unsigned k) { Seed(s_index[k]); }, &Disturb,
                   rounds && *rounds ? static_cast<unsigned>(std::strtoul(rounds, nullptr, 0)) : 4000u};
    g.args = [](unsigned k, U* a) { Args(s_index[k], a); };
    g.effect = true;
    g.kinds = kKinds;
    g.n_kinds = sizeof kKinds;
    sh::Run(g);
}

}  // namespace effect_5d

// BOF3X_SHADOW=effect_3c: group E3C's 51 functions through the scenario harness
// in effect mode (scenario_harness.h, docs/scenario_harness.md section 8), once
// at start-up. docs/effect_3c.md section 4. BOF3X_E3C_ONLY=<name> runs the
// clones whose name contains it (the controls' speed-up).
//
// The clone table is tools/band_rows.py --group E3C --clones --harness scenario
// (2026-10-03), each extent read again to its last instruction (capstone) and
// the names given, with two changes: 0x485C50 ends at its jmp to 0x485C60 (the
// tool's extent 0x51 ran on into the shared tail), and that tail - kind 0x75's
// EffectKind75_MarkSprites, its own frame and ret, reached by the tail jumps
// of 0x485C00 and 0x485C50 - added as its own clone. Shapes: every dispatcher
// and state kEffect (Sprite_Current one of the 20 Effect_Objects records, +5
// the kind, a dispatcher's byte below its table's length); the helpers with
// arguments kCall; the three movers answering al and the two finders answering
// a pointer with their ret_mask.
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_3c.h"
#include "game/effect_3c_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace effect_3c {
namespace {

namespace sh = scenario_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using U = std::uint32_t;

// tools/band_rows.py --group E3C --clones, 2026-10-03 (0x485C50 / 0x485C60 split by hand).
constexpr sh::CallSite kCalls484050[] = {{0x12, 0x4841E0}};
constexpr sh::CallSite kCalls484070[] = {{0x7, 0x483C10}};
constexpr sh::CallSite kCalls484090[] = {{0x9, 0x4841C0}, {0x18, 0x484170}};
constexpr sh::CallSite kCalls4840F0[] = {{0x9, 0x4841C0}, {0x18, 0x484170}};
constexpr sh::CallSite kCalls484150[] = {{0x0, 0x4841E0}, {0x9, 0x589840}};
constexpr sh::CallSite kCalls4841E0[] = {{0x1, 0x494060}, {0x13, 0x5A79A0}, {0x2B, 0x5A77C0}, {0x34, 0x461E50}, {0x98, 0x483DA0}};
constexpr sh::CallSite kCalls4842C0[] = {{0x0, 0x4842F0}, {0x5, 0x4844C0}};
constexpr sh::CallSite kCalls4842E0[] = {{0x0, 0x484320}, {0x9, 0x589840}};
constexpr sh::CallSite kCalls484320[] = {{0x2, 0x494060}, {0x5C, 0x5B93D2}, {0x74, 0x4843B0}};
constexpr sh::CallSite kCalls4843B0[] = {{0x11, 0x5A79A0}, {0x29, 0x5A77C0}, {0x41, 0x572FA0}, {0x4D, 0x5A7570},
                                         {0x55, 0x5A7780}, {0x5F, 0x494110}, {0xF4, 0x572FA0}};
constexpr sh::CallSite kCalls4844C0[] = {{0x31, 0x5B93D2}, {0x46, 0x5B93D2}, {0x82, 0x5B93D2}, {0x9E, 0x5B93D2},
                                         {0xB3, 0x5B93D2}, {0xEF, 0x5B93D2}, {0x113, 0x5B93D2}};
constexpr sh::CallSite kCalls484610[] = {{0x10, 0x57C140}, {0x36, 0x57C140}};
constexpr sh::CallSite kCalls484670[] = {{0x45, 0x5720C0}, {0x56, 0x4849A0}, {0xC2, 0x484B10}, {0xF4, 0x484B10},
                                         {0x106, 0x437CC0}, {0x138, 0x57C140}, {0x14A, 0x587740}};
constexpr sh::CallSite kCalls4847D0[] = {{0x46, 0x5720C0}, {0x57, 0x4849A0}, {0xC3, 0x484B10}, {0xF5, 0x484B10},
                                         {0x107, 0x437CC0}, {0x139, 0x57C140}, {0x14B, 0x587740}};
constexpr sh::CallSite kCalls484930[] = {{0x10, 0x57C140}, {0x21, 0x587740}, {0x42, 0x57C140}, {0x53, 0x587740}};
constexpr sh::CallSite kCalls4849A0[] = {{0x43, 0x531F10}, {0xA3, 0x57C8A0}, {0xF4, 0x531F10}, {0x154, 0x57C8A0}};
constexpr sh::CallSite kCalls484B10[] = {{0x14, 0x5A79A0},  {0x2C, 0x5A77C0},  {0x42, 0x572FA0},  {0x47, 0x494060},
                                         {0x53, 0x5A7650},  {0x5B, 0x5A7780},  {0x69, 0x494110},  {0x77, 0x494110},
                                         {0xCF, 0x572FA0},  {0xD8, 0x5B9550},  {0xF1, 0x5B9550},  {0x102, 0x5A7A70},
                                         {0x126, 0x4941E0}, {0x14C, 0x4941E0}, {0x16B, 0x5B9550}, {0x173, 0x5B9550},
                                         {0x188, 0x5B9550}, {0x190, 0x5B9550}, {0x196, 0x484D00}, {0x1AB, 0x5A79A0},
                                         {0x1C4, 0x5A77C0}, {0x1DA, 0x572FA0}};
constexpr sh::CallSite kCalls484D00[] = {{0xC, 0x5A7610},  {0x14, 0x5A7780},  {0x69, 0x5A7A50},  {0x82, 0x5A7A00},
                                         {0xB7, 0x5A7A50}, {0xD4, 0x5A7A00},  {0x193, 0x572FA0}, {0x1AE, 0x5A7A50},
                                         {0x1D0, 0x5A7A00}, {0x1F1, 0x5A7A50}, {0x213, 0x5A7A00}, {0x240, 0x572FA0}};
constexpr sh::CallSite kCalls484F70[] = {{0xD, 0x4851E0}};
constexpr sh::CallSite kCalls484FA0[] = {{0x12, 0x5A79A0}, {0x2A, 0x5A77C0}, {0x33, 0x461E50}, {0x3B, 0x494060}, {0x4E, 0x485030}};
constexpr sh::CallSite kCalls485030[] = {{0xE, 0x5A75F0},  {0x16, 0x5A7780}, {0x24, 0x494110}, {0x2E, 0x5A7A50},
                                         {0x41, 0x5A7A00}, {0x66, 0x5A7A00}, {0x79, 0x5A7A50}, {0xBE, 0x494110},
                                         {0xC8, 0x5A7A50}, {0xDB, 0x5A7A00}, {0x100, 0x5A7A00}, {0x113, 0x5A7A50},
                                         {0x158, 0x494110}, {0x19D, 0x461E50}};
constexpr sh::CallSite kCalls4851E0[] = {{0x2D, 0x5B93D2}, {0x3A, 0x5B93D2}, {0x47, 0x5B93D2}, {0x56, 0x5A7A50},
                                         {0x61, 0x5A7A00}, {0x72, 0x5A7A50}, {0x7D, 0x5A7A00}, {0x91, 0x494180},
                                         {0x9F, 0x5A7F10}, {0xAF, 0x5A7F80}, {0xBD, 0x5A7FF0}, {0xCC, 0x5A7C70},
                                         {0xDB, 0x5A7C70}, {0xE3, 0x5B93D2}};
constexpr sh::CallSite kCalls485330[] = {{0x0, 0x485840}, {0x1B, 0x587740}};
constexpr sh::CallSite kCalls485360[] = {{0xB, 0x485810}, {0x15, 0x4857C0}, {0x1D, 0x485870}};
constexpr sh::CallSite kCalls4853A0[] = {{0x0, 0x485870}, {0x9, 0x589840}};
constexpr sh::CallSite kCalls4853D0[] = {{0xC, 0x485840}, {0x15, 0x485810}, {0x21, 0x4857C0}, {0x3E, 0x587740}};
constexpr sh::CallSite kCalls485430[] = {{0x0, 0x485870}, {0x9, 0x589840}};
constexpr sh::CallSite kCalls485460[] = {{0x5B, 0x587740}};
constexpr sh::CallSite kCalls4854D0[] = {{0x34, 0x485570}};
constexpr sh::CallSite kCalls485530[] = {{0x19, 0x589840}, {0x30, 0x485570}};
constexpr sh::CallSite kCalls485570[] = {{0x7, 0x494060},  {0x38, 0x5A7A50},  {0x47, 0x5A7A00},  {0x82, 0x494110},
                                         {0xA9, 0x494110}, {0xCE, 0x5A75D0},  {0xD6, 0x5A7780},  {0x11E, 0x5A7A50},
                                         {0x131, 0x5A7A00}, {0x16A, 0x494110}, {0x191, 0x494110}, {0x1FE, 0x5A79A0},
                                         {0x20E, 0x5A79E0}, {0x225, 0x572FA0}};
constexpr sh::CallSite kCalls485870[] = {{0x4, 0x494060}, {0x67, 0x5A7A50}, {0x83, 0x5A7A00}, {0xC3, 0x485960}};
constexpr sh::CallSite kCalls485960[] = {{0x13, 0x5A79A0}, {0x2B, 0x5A77C0}, {0x43, 0x572FA0}, {0x4F, 0x5A75D0},
                                         {0x57, 0x5A7780}, {0x62, 0x494110}, {0x80, 0x4941E0}, {0x1D6, 0x5A79E0},
                                         {0x1ED, 0x5A79A0}, {0x231, 0x572FA0}};
constexpr sh::CallSite kCalls485C00[] = {{0x3E, 0x48CA90}, {0x43, 0x485C60}};
constexpr sh::CallSite kCalls485C50[] = {{0x0, 0x48CA90}, {0x5, 0x485C60}};
constexpr sh::CallSite kCalls485C60[] = {{0x29, 0x588F20}};

#define E3C_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define E3C_CALLS(a) a, E3C_N(a)
#define E3C_FN(name) reinterpret_cast<const void*>(&::name)
constexpr sh::Shape kEf = sh::Shape::kEffect;
constexpr sh::Shape kCa = sh::Shape::kCall;
constexpr U kAl = 0xFFu, kEax = 0xFFFFFFFFu;
// {name, base, size, calls, imms, tables, ours, ret_mask, calm, shape, pointers, state_span, sub_span, kind}
const sh::Clone kAll[] = {
    {"EffectKind6E_Run", 0x484050, 0x17, E3C_CALLS(kCalls484050), nullptr, 0, nullptr, 0, E3C_FN(EffectKind6E_Run), 0, false, kEf, 0, 4, 0, 0x6E},
    {"EffectKind6E_Start", 0x484070, 0x1E, E3C_CALLS(kCalls484070), nullptr, 0, nullptr, 0, E3C_FN(EffectKind6E_Start), 0, false, kEf, 0, 0, 0, 0x6E},
    {"EffectKind6E_Emit", 0x484090, 0x5F, E3C_CALLS(kCalls484090), nullptr, 0, nullptr, 0, E3C_FN(EffectKind6E_Emit), 0, false, kEf, 0, 0, 0, 0x6E},
    {"EffectKind6E_EmitSlower", 0x4840F0, 0x5F, E3C_CALLS(kCalls4840F0), nullptr, 0, nullptr, 0, E3C_FN(EffectKind6E_EmitSlower), 0, false, kEf, 0, 0, 0, 0x6E},
    {"EffectKind6E_Fade", 0x484150, 0x18, E3C_CALLS(kCalls484150), nullptr, 0, nullptr, 0, E3C_FN(EffectKind6E_Fade), 0, false, kEf, 0, 0, 0, 0x6E},
    {"EffectKind6E_ShardInit", 0x484170, 0x45, nullptr, 0, nullptr, 0, nullptr, 0, E3C_FN(EffectKind6E_ShardInit), 0, false, kCa, 0, 0, 0, 0x6E},
    {"EffectKind6E_FindShard", 0x4841C0, 0x19, nullptr, 0, nullptr, 0, nullptr, 0, E3C_FN(EffectKind6E_FindShard), kEax, false, kCa, 0, 0, 0, 0x6E},
    {"EffectKind6E_DrawShards", 0x4841E0, 0xB6, E3C_CALLS(kCalls4841E0), nullptr, 0, nullptr, 0, E3C_FN(EffectKind6E_DrawShards), kAl, false, kEf, 0, 0, 0, 0x6E},
    {"EffectKind6D_Run", 0x4842A0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E3C_FN(EffectKind6D_Run), 0, false, kEf, 0, 2, 0, 0x6D},
    {"EffectKind6D_Start", 0x4842C0, 0x1C, E3C_CALLS(kCalls4842C0), nullptr, 0, nullptr, 0, E3C_FN(EffectKind6D_Start), 0, false, kEf, 0, 0, 0, 0x6D},
    {"EffectKind6D_Move", 0x4842E0, 0xF, E3C_CALLS(kCalls4842E0), nullptr, 0, nullptr, 0, E3C_FN(EffectKind6D_Move), 0, false, kEf, 0, 0, 0, 0x6D},
    {"EffectKind6D_ClearParticles", 0x4842F0, 0x25, nullptr, 0, nullptr, 0, nullptr, 0, E3C_FN(EffectKind6D_ClearParticles), 0, false, kEf, 0, 0, 0, 0x6D},
    {"EffectKind6D_MoveParticles", 0x484320, 0x87, E3C_CALLS(kCalls484320), nullptr, 0, nullptr, 0, E3C_FN(EffectKind6D_MoveParticles), kAl, false, kEf, 0, 0, 0, 0x6D},
    {"EffectKind6D_DrawParticle", 0x4843B0, 0x101, E3C_CALLS(kCalls4843B0), nullptr, 0, nullptr, 0, E3C_FN(EffectKind6D_DrawParticle), 0, false, kCa, 0, 0, 0, 0x6D},
    {"EffectKind6D_InitParticles", 0x4844C0, 0x12B, E3C_CALLS(kCalls4844C0), nullptr, 0, nullptr, 0, E3C_FN(EffectKind6D_InitParticles), 0, false, kEf, 0, 0, 0, 0x6D},
    {"EffectKind6F_Run", 0x4845F0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E3C_FN(EffectKind6F_Run), 0, false, kEf, 0, 4, 0, 0x6F},
    {"EffectKind6F_Start", 0x484610, 0x58, E3C_CALLS(kCalls484610), nullptr, 0, nullptr, 0, E3C_FN(EffectKind6F_Start), 0, false, kEf, 0, 0, 0, 0x6F},
    {"EffectKind6F_AlongZ", 0x484670, 0x15D, E3C_CALLS(kCalls484670), nullptr, 0, nullptr, 0, E3C_FN(EffectKind6F_AlongZ), 0, false, kEf, 0, 0, 0, 0x6F},
    {"EffectKind6F_AlongX", 0x4847D0, 0x15D, E3C_CALLS(kCalls4847D0), nullptr, 0, nullptr, 0, E3C_FN(EffectKind6F_AlongX), 0, false, kEf, 0, 0, 0, 0x6F},
    {"EffectKind6F_Watch", 0x484930, 0x66, E3C_CALLS(kCalls484930), nullptr, 0, nullptr, 0, E3C_FN(EffectKind6F_Watch), 0, false, kEf, 0, 0, 0, 0x6F},
    {"EffectKind6F_PushParty", 0x4849A0, 0x167, E3C_CALLS(kCalls4849A0), nullptr, 0, nullptr, 0, E3C_FN(EffectKind6F_PushParty), 0, false, kCa, 0, 0, 0, 0x6F},
    {"EffectKind6F_DrawSegment", 0x484B10, 0x1EA, E3C_CALLS(kCalls484B10), nullptr, 0, nullptr, 0, E3C_FN(EffectKind6F_DrawSegment), 0, false, kCa, 0, 0, 0, 0x6F},
    {"EffectKind6F_DrawRibbon", 0x484D00, 0x24E, E3C_CALLS(kCalls484D00), nullptr, 0, nullptr, 0, E3C_FN(EffectKind6F_DrawRibbon), 0, false, kCa, 0, 0, 0, 0x6F},
    {"EffectKind72_Run", 0x484F50, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E3C_FN(EffectKind72_Run), 0, false, kEf, 0, 3, 0, 0x72},
    {"EffectKind72_Start", 0x484F70, 0x2F, E3C_CALLS(kCalls484F70), nullptr, 0, nullptr, 0, E3C_FN(EffectKind72_Start), 0, false, kEf, 0, 0, 0, 0x72},
    {"EffectKind72_Debris", 0x484FA0, 0x8F, E3C_CALLS(kCalls484FA0), nullptr, 0, nullptr, 0, E3C_FN(EffectKind72_Debris), 0, false, kEf, 0, 0, 0, 0x72},
    {"EffectDebris_Draw", 0x485030, 0x1AD, E3C_CALLS(kCalls485030), nullptr, 0, nullptr, 0, E3C_FN(EffectDebris_Draw), 0, false, kCa, 0, 0, 0, 0x72},
    {"EffectDebris_InitOne", 0x4851E0, 0x10C, E3C_CALLS(kCalls4851E0), nullptr, 0, nullptr, 0, E3C_FN(EffectDebris_InitOne), 0, false, kCa, 0, 0, 0, 0x72},
    {"EffectKind73_Run", 0x4852F0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E3C_FN(EffectKind73_Run), 0, false, kEf, 0, 2, 0, 0x73},
    {"EffectKind73_RunA", 0x485310, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E3C_FN(EffectKind73_RunA), 0, false, kEf, 0, 2, 3, 0x73},
    {"EffectKind73A_Start", 0x485330, 0x22, E3C_CALLS(kCalls485330), nullptr, 0, nullptr, 0, E3C_FN(EffectKind73A_Start), 0, false, kEf, 0, 2, 3, 0x73},
    {"EffectKind73A_Emit", 0x485360, 0x3F, E3C_CALLS(kCalls485360), nullptr, 0, nullptr, 0, E3C_FN(EffectKind73A_Emit), 0, false, kEf, 0, 2, 3, 0x73},
    {"EffectKind73A_Fade", 0x4853A0, 0xF, E3C_CALLS(kCalls4853A0), nullptr, 0, nullptr, 0, E3C_FN(EffectKind73A_Fade), 0, false, kEf, 0, 2, 3, 0x73},
    {"EffectKind73_RunB", 0x4853B0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E3C_FN(EffectKind73_RunB), 0, false, kEf, 0, 2, 2, 0x73},
    {"EffectKind73B_Burst", 0x4853D0, 0x56, E3C_CALLS(kCalls4853D0), nullptr, 0, nullptr, 0, E3C_FN(EffectKind73B_Burst), 0, false, kEf, 0, 2, 2, 0x73},
    {"EffectKind73B_Fade", 0x485430, 0xF, E3C_CALLS(kCalls485430), nullptr, 0, nullptr, 0, E3C_FN(EffectKind73B_Fade), 0, false, kEf, 0, 2, 2, 0x73},
    {"EffectKind74_Run", 0x485440, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E3C_FN(EffectKind74_Run), 0, false, kEf, 0, 3, 0, 0x74},
    {"EffectKind74_Start", 0x485460, 0x62, E3C_CALLS(kCalls485460), nullptr, 0, nullptr, 0, E3C_FN(EffectKind74_Start), 0, false, kEf, 0, 0, 0, 0x74},
    {"EffectKind74_Rise", 0x4854D0, 0x5F, E3C_CALLS(kCalls4854D0), nullptr, 0, nullptr, 0, E3C_FN(EffectKind74_Rise), 0, false, kEf, 0, 0, 0, 0x74},
    {"EffectKind74_Fade", 0x485530, 0x35, E3C_CALLS(kCalls485530), nullptr, 0, nullptr, 0, E3C_FN(EffectKind74_Fade), 0, false, kEf, 0, 0, 0, 0x74},
    {"EffectKind74_Draw", 0x485570, 0x24C, E3C_CALLS(kCalls485570), nullptr, 0, nullptr, 0, E3C_FN(EffectKind74_Draw), 0, false, kEf, 0, 0, 0, 0x74},
    {"EffectKind73_SparkInit", 0x4857C0, 0x41, nullptr, 0, nullptr, 0, nullptr, 0, E3C_FN(EffectKind73_SparkInit), 0, false, kCa, 0, 0, 0, 0x73},
    {"EffectKind73_FindSpark", 0x485810, 0x23, nullptr, 0, nullptr, 0, nullptr, 0, E3C_FN(EffectKind73_FindSpark), kEax, false, kCa, 0, 0, 0, 0x73},
    {"EffectKind73_ClearSparks", 0x485840, 0x2B, nullptr, 0, nullptr, 0, nullptr, 0, E3C_FN(EffectKind73_ClearSparks), 0, false, kCa, 0, 0, 0, 0x73},
    {"EffectKind73_MoveSparks", 0x485870, 0xF0, E3C_CALLS(kCalls485870), nullptr, 0, nullptr, 0, E3C_FN(EffectKind73_MoveSparks), kAl, false, kEf, 0, 2, 0, 0x73},
    {"EffectKind73_DrawSpark", 0x485960, 0x240, E3C_CALLS(kCalls485960), nullptr, 0, nullptr, 0, E3C_FN(EffectKind73_DrawSpark), 0, false, kCa, 0, 0, 0, 0x73},
    {"EffectKind75_Run", 0x485BA0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E3C_FN(EffectKind75_Run), 0, false, kEf, 0, 3, 0, 0x75},
    {"EffectKind75_Start", 0x485BC0, 0x31, nullptr, 0, nullptr, 0, nullptr, 0, E3C_FN(EffectKind75_Start), 0, false, kEf, 0, 0, 0, 0x75},
    {"EffectKind75_Brighten", 0x485C00, 0x48, E3C_CALLS(kCalls485C00), nullptr, 0, nullptr, 0, E3C_FN(EffectKind75_Brighten), 0, false, kEf, 0, 0, 0, 0x75},
    {"EffectKind75_Hold", 0x485C50, 0xA, E3C_CALLS(kCalls485C50), nullptr, 0, nullptr, 0, E3C_FN(EffectKind75_Hold), 0, false, kEf, 0, 0, 0, 0x75},
    {"EffectKind75_MarkSprites", 0x485C60, 0x41, E3C_CALLS(kCalls485C60), nullptr, 0, nullptr, 0, E3C_FN(EffectKind75_MarkSprites), 0, false, kEf, 0, 0, 0, 0x75},
};
#undef E3C_FN
#undef E3C_CALLS
#undef E3C_N

enum : unsigned {
    k6ERun, k6EStart, k6EEmit, k6EEmitSlower, k6EFade, k6EShardInit, k6EFindShard, k6EDrawShards,
    k6DRun, k6DStart, k6DMove, k6DClear, k6DMoveParticles, k6DDrawParticle, k6DInit,
    k6FRun, k6FStart, k6FAlongZ, k6FAlongX, k6FWatch, k6FPushParty, k6FDrawSegment, k6FDrawRibbon,
    k72Run, k72Start, k72Debris, kDebrisDraw, kDebrisInit,
    k73Run, k73RunA, k73AStart, k73AEmit, k73AFade, k73RunB, k73BBurst, k73BFade,
    k74Run, k74Start, k74Rise, k74Fade, k74Draw,
    k73SparkInit, k73FindSpark, k73ClearSparks, k73MoveSparks, k73DrawSpark,
    k75Run, k75Start, k75Brighten, k75Hold, k75Mark, kCount
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
U Shards() { return Key(EffectKind30_Shards); }

// --- the effects ---------------------------------------------------------------------

// The first record of a pool whose +0 is 0, or null - as the real finders; null
// also a quarter of the time (the answer's draw).
U FirstFree(U base, U stride, unsigned n, U answer) {
    if (answer % 4 == 0) return 0;
    for (unsigned i = 0; i < n; ++i)
        if (P(base + i * stride)[0] == 0) return base + i * stride;
    return 0;
}
// EffectKind6E_FindShard: sixteen of 0x28 at EffectKind30_Shards.
U FxFindShard(const U*, U answer) { return FirstFree(Shards(), at::kShardStride, at::kShardCount, answer); }
// EffectKind73_FindSpark: 32 of 0x18, and the cursor left where the real one
// leaves it (the record found, or past the last).
U FxFindSpark(const U*, U answer) {
    const U r = FirstFree(Shards(), at::kSparkStride, at::kSparkCount, answer);
    SetLong(Mem(at::kSparkCursor), static_cast<std::int32_t>(r != 0 ? r : Shards() + at::kSparkStride * at::kSparkCount));
    return r;
}
// A record's set-up (EffectDebris_InitOne, the two shard / spark inits): what
// it writes filled, where the record is in the regions.
template <unsigned N> U FxFill(const U* a, U answer) {
    if (sh::InRegions(P(a[0]), N)) sh::FillBytes(P(a[0]), N);
    return answer;
}
// E3B's shard spread and E4D's tile act on Sprite_Current: its address and the
// bytes they read (+0x34..+0x3F, +0x5D..+0x5F) logged.
U FxSpread(const U*, U answer) {
    unsigned char* const s = Sprite_Current;
    sh::Note(Key(s), static_cast<U>(Long(s + 0x34)), static_cast<U>(Long(s + 0x38)), static_cast<U>(Long(s + 0x3C)));
    return answer;
}
U FxTile(const U*, U answer) {
    unsigned char* const s = Sprite_Current;
    sh::Note(Key(s), static_cast<U>(Long(s + 0x5C)));
    return answer;
}

#define E3C_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
constexpr sh::Answer kG = sh::Answer::kGarbage, kF = sh::Answer::kFlag, kPh = sh::Answer::kPhase;
constexpr U kW = 0xFFFFFFFFu, k16 = 0xFFFFu, k8 = 0xFFu;
const sh::Callee kCallees[] = {
    // the group's own called directly, by name
    {E3C_OURS(EffectKind6E_DrawShards), 0, {}, kF, 0, 0},
    {E3C_OURS(EffectKind6E_FindShard), 0, {}, kG, 0, 0, {}, &FxFindShard},
    {E3C_OURS(EffectKind6E_ShardInit), 1, {kW}, kG, 0, 0, {}, &FxFill<0x28>},
    {E3C_OURS(EffectKind6D_ClearParticles), 0, {}, kPh, 0, 0},
    {E3C_OURS(EffectKind6D_InitParticles), 0, {}, kPh, 0, 0},
    {E3C_OURS(EffectKind6D_MoveParticles), 0, {}, kF, 0, 0},
    {E3C_OURS(EffectKind6D_DrawParticle), 1, {kW}, kG, 0, 0, {0x28}, nullptr, nullptr, true},
    // the low byte read (`mov al, [esp + 4]`); the callers push 0 or 1
    {E3C_OURS(EffectKind6F_PushParty), 1, {k8}, kG, 0, 0},
    // two of the record's points (+0xC, +0x18), hashed
    {E3C_OURS(EffectKind6F_DrawSegment), 2, {kW, kW}, kG, 0, 0, {12, 12}, nullptr, nullptr, true},
    // every argument read as its low s16 (movsx word, and 0xFFFF)
    {E3C_OURS(EffectKind6F_DrawRibbon), 8, {k16, k16, k16, k16, k16, k16, k16, k16}, kG, 0, 0},
    {E3C_OURS(EffectDebris_Draw), 1, {kW}, kG, 0, 0, {0x2C}, nullptr, nullptr, true},
    {E3C_OURS(EffectDebris_InitOne), 1, {kW}, kG, 0, 0, {}, &FxFill<0x2C>},
    {E3C_OURS(EffectKind73_ClearSparks), 0, {}, kPh, 0, 0},
    {E3C_OURS(EffectKind73_FindSpark), 0, {}, kG, 0, 0, {}, &FxFindSpark},
    {E3C_OURS(EffectKind73_SparkInit), 1, {kW}, kG, 0, 0, {}, &FxFill<0x18>},
    {E3C_OURS(EffectKind73_MoveSparks), 0, {}, kF, 0, 0},
    {E3C_OURS(EffectKind73_DrawSpark), 1, {kW}, kG, 0, 0, {0x18}, nullptr, nullptr, true},
    {E3C_OURS(EffectKind74_Draw), 0, {}, kPh, 0, 0},
    {E3C_OURS(EffectKind75_MarkSprites), 0, {}, kPh, 0, 0},
    // other groups' of this round, by address until they merge
    {"0x483C10 (E3B)", at::kShardsSpread, at::kShardsSpread, 0, {}, kG, 0, 0, {}, &FxSpread},
    {"0x483DA0 (E3B)", at::kShardQuad, at::kShardQuad, 1, {kW}, kG, 0, 0, {0x28}, nullptr, nullptr, true},
    {"0x48CA90 (E4D)", at::kScreenTile, at::kScreenTile, 0, {}, kG, 0, 0, {}, &FxTile},
};
#undef E3C_OURS

// The state tables the dispatchers jump through, read in place; each table's
// own length (symbols.toml [[data]], none bounded by a compare).
const sh::DataTable kTables[] = {
    {0x654A44, 4}, {0x654A54, 2}, {0x654AA0, 4}, {0x654AB8, 3}, {0x654AC4, 2},
    {0x654ACC, 3}, {0x654AD8, 2}, {0x654AE0, 3}, {0x654AEC, 3},
};
const std::uint8_t kKinds[] = {0x6D, 0x6E, 0x6F, 0x72, 0x73, 0x74, 0x75};

// Beyond effect mode's standard regions: the two cursors and kind 0x6D's two
// particle blocks.
const sh::Region kRegions[] = {
    {at::kShardCursor, 4},
    {at::kSparkCursor, 4},
    {at::kParticles, 2 * at::kParticleBlock},
};

// --- the seed ------------------------------------------------------------------------

unsigned char* Rec(unsigned r) { return sh::EffectRecord(r); }

// A pool's records: +0 free a third of the time (some rounds all used), the
// life +2 at its boundaries.
void Pool(U base, U stride, unsigned n) {
    const bool full = sh::Next() % 5 == 0;
    for (unsigned i = 0; i < n; ++i) {
        unsigned char* const r = P(base + i * stride);
        r[0] = static_cast<unsigned char>(!full && sh::Next() % 3 == 0 ? 0 : PickOf(1, 1, 0x80, sh::Next() | 1));
        r[2] = static_cast<unsigned char>(PickOf(1, 1, 2, 0, 0x20, sh::Next()));
    }
}

void Seed(unsigned k) {
    unsigned char* const s = Sprite_Current;
    // every record the disturbance may move Sprite_Current to: +6 as each
    // function indexes by it (kind 0x6D's block below two; kind 0x6F's colour
    // row 0), +9 at its boundaries
    for (unsigned r = 0; r < 20; ++r) {
        unsigned char* const e = Rec(r);
        if (k >= k6DRun && k <= k6DInit)
            e[6] = static_cast<unsigned char>(sh::Next() % 2);
        else if (k >= k6FRun && k <= k6FDrawRibbon)
            e[6] = 0;
        e[9] = static_cast<unsigned char>(PickOf(1, 2, 0, 3, 4, 5, 0x44, 0x45, 0xFF, sh::Next()));
    }
    switch (k) {
    case k6EEmit:
    case k6EEmitSlower:
        if (sh::Half()) Frame_Counter &= ~3u;
        Pool(Shards(), at::kShardStride, at::kShardCount);
        break;
    case k6EFindShard:
    case k6EDrawShards:
        Pool(Shards(), at::kShardStride, at::kShardCount);
        break;
    case k6DClear:
    case k6DMoveParticles:
        for (unsigned b = 0; b < 2; ++b) Pool(at::kParticles + b * at::kParticleBlock, at::kParticleStride, at::kParticleCount);
        break;
    case k6DDrawParticle:
        for (unsigned i = 0; i < 2 * at::kParticleCount; ++i)
            P(at::kParticles + i * at::kParticleStride)[4] = static_cast<unsigned char>(sh::Next() % at::kParticleShapeCount);
        break;
    case k6FStart:
    case k6FWatch:
        Cond_ByteFD = static_cast<unsigned char>(PickOf(2, 3, 2, 3, 0, 1, sh::Next()));
        break;
    case k6FPushParty:
        if (sh::Half()) Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 & ~0x1400u);
        if (sh::Half()) Field_Request = 0;
        break;
    case k72Debris:
        s[9] = static_cast<unsigned char>(PickOf(3, 4, 0x43, 0x44, 0x45, 0xFF, sh::Next()));
        break;
    case kDebrisDraw:
        for (unsigned i = 0; i < at::kDebrisCount; ++i)
            SetWord(P(Shards() + i * at::kDebrisStride + 0x2A), PickOf(0xFFFF, 0, 1, 0xFF, 0x100, 0x7FFF, 0x8000, sh::Next()));
        break;
    case k73AEmit:
        Pool(Shards(), at::kSparkStride, at::kSparkCount);
        break;
    case k73BBurst:
        s[0xB] = static_cast<unsigned char>(sh::Half() ? 0 : sh::Next() | 1);
        Pool(Shards(), at::kSparkStride, at::kSparkCount);
        break;
    case k73FindSpark:
    case k73MoveSparks:
        Pool(Shards(), at::kSparkStride, at::kSparkCount);
        break;
    case k74Rise: {
        // the top at and around its cap, the foot + 0x8000000
        const U cap = static_cast<U>(Long(s + 0x14)) + 0x8000000u;
        SetLong(s + 0x1C, static_cast<std::int32_t>(cap - PickOf(0x1000000, 0x1000001, 0xFFFFFF, 0, 0x2000000, sh::Next())));
        break;
    }
    case k74Draw: {
        // the column a few steps tall on every record (Sprite_Current read
        // again for its bounds), the width and the foot round each other; the
        // height kept off the signed wrap, where the step count runs to 4,096
        const U y0 = sh::Next() % 0x40000000u - 0x20000000u;
        for (unsigned r = 0; r < 20; ++r) {
            unsigned char* const e = Rec(r);
            const U foot = y0 + (sh::Next() % 5) * 0x80000u - 0x100000u;
            SetLong(e + 0x14, static_cast<std::int32_t>(foot));
            SetLong(e + 0x1C, static_cast<std::int32_t>(y0 + (sh::Next() % 0x14) * 0x100000u - 0x1000000u - PickOf(0, 1, 0xFFFFF)));
            SetLong(e + 0x24, static_cast<std::int32_t>(PickOf(0x1000000, 0x100000, 0, 0xF00000, sh::Next() % 0x2000000)));
        }
        SetLong(s + 0x14, static_cast<std::int32_t>(y0));
        break;
    }
    case k75Mark:
        for (unsigned i = 0; i < at::kSpriteCount; ++i) {
            unsigned char* const o = Sprite_Objects + i * at::kSpriteStride;
            o[0] = static_cast<unsigned char>(PickOf(0, 1, 3, 0xFE, sh::Next()));
            o[6] = static_cast<unsigned char>(PickOf(6, 6, 5, 7, sh::Next()));
        }
        break;
    default: break;
    }
}

// The helpers with arguments: a record of the pool each draws or sets up (the
// callers hand them so), PushParty's byte over a leftover, the segment's two
// points of the record, the ribbon's words random.
void Args(unsigned k, U* a) {
    switch (k) {
    case k6EShardInit: a[0] = Shards() + at::kShardStride * (sh::Next() % at::kShardCount); break;
    case k6DDrawParticle: a[0] = at::kParticles + at::kParticleStride * (sh::Next() % (2 * at::kParticleCount)); break;
    case k6FPushParty: a[0] = (sh::Next() & 0xFFFFFF00u) | PickOf(0, 1, 0, 1, 2, sh::Next()); break;
    case k6FDrawSegment:
        a[0] = Key(Sprite_Current + 0xC);
        a[1] = Key(Sprite_Current + 0x18);
        break;
    case kDebrisDraw:
    case kDebrisInit: a[0] = Shards() + at::kDebrisStride * (sh::Next() % at::kDebrisCount); break;
    case k73SparkInit:
    case k73DrawSpark: a[0] = Shards() + at::kSparkStride * (sh::Next() % at::kSparkCount); break;
    default: break;
    }
}

// What the states read again after a call, moved by the group's case of the
// harness's disturbance (from its hash only): the frame count +9, the two
// cursors inside their pools, Cond_ByteFD, the leftover flags, kind 0x74's top
// near its foot, kind 0x75's colour, the variant +1 of kind 0x73's movers is
// the harness's own.
void Disturb(U h) {
    const U v = h >> 8;
    unsigned char* const s = Sprite_Current;
    if (!sh::InRegions(s, 0x80)) return;
    switch (h % 7) {
    case 0: s[9] = static_cast<unsigned char>((v & 1) ? 1u : v >> 1); break;
    case 1: SetLong(Mem(at::kShardCursor), static_cast<std::int32_t>(Shards() + at::kShardStride * ((v >> 1) % at::kShardCount))); break;
    case 2: SetLong(Mem(at::kSparkCursor), static_cast<std::int32_t>(Shards() + at::kSparkStride * ((v >> 1) % at::kSparkCount))); break;
    case 3: Cond_ByteFD = static_cast<unsigned char>((v & 1) ? 2u + ((v >> 1) & 1) : v >> 1); break;
    case 4: SetLong(s + 0x1C, static_cast<std::int32_t>(static_cast<U>(Long(s + 0x14)) + ((v >> 1) % 0x14) * 0x100000u)); break;
    case 5: s[0x5D + v % 3] = static_cast<unsigned char>(v >> 2); break;
    case 6: SetLong(s + 0xC, static_cast<std::int32_t>(v)); break;
    default: break;
    }
}

}  // namespace

void SelfTest() {
    // BOF3X_E3C_ONLY: the clones whose name contains it (a control's run)
    static sh::Clone chosen[kCount];
    static unsigned index[kCount];
    const char* const only = std::getenv("BOF3X_E3C_ONLY");
    unsigned n = 0;
    for (unsigned k = 0; k < kCount; ++k)
        if (!only || !*only || std::strstr(kAll[k].name, only)) {
            index[n] = k;
            chosen[n++] = kAll[k];
        }
    if (n == 0) bof3::Fatal("effect_3c: BOF3X_E3C_ONLY=%s names no clone", only);
    static unsigned* s_index = index;
    sh::Group g = {"effect_3c", chosen, n, kCallees, sizeof kCallees / sizeof kCallees[0], kTables,
                   sizeof kTables / sizeof kTables[0], kRegions, sizeof kRegions / sizeof kRegions[0],
                   [](unsigned k) { Seed(s_index[k]); }, &Disturb, 4000};
    g.args = [](unsigned k, U* a) { Args(s_index[k], a); };
    g.effect = true;
    g.kinds = kKinds;
    g.n_kinds = sizeof kKinds;
    sh::Run(g);
}

}  // namespace effect_3c

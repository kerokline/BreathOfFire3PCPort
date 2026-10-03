// BOF3X_SHADOW=effect_5b: group E5B's 50 functions through the scenario harness
// in effect mode (scenario_harness.h, docs/scenario_harness.md section 8), once
// at start-up. docs/effect_5b.md section 4. BOF3X_E5B_ONLY=<name> runs the
// clones whose name contains it (the controls' speed-up); BOF3X_E5B_ROUNDS sets
// the rounds (default 4,000).
//
// The clone table is tools/band_rows.py --group E5B --clones --harness scenario
// (2026-10-03), each extent read again to its last instruction (capstone) and
// the names given, with two starts added and one extent cut: sub-kind 0x13's
// state 1 0x5003A0 (EffectKind18Sub13_States[1]; the tool counted it into
// 0x500320, whose clone now ends at it, the jmp at +0x67 a call site) and
// state 3 0x5004A0 (code no list has, which the tool printed). Shapes: the
// seven dispatchers, the states and the two void draws kEffect (Sprite_Current
// one of the 20 Effect_Objects records, +5 0x18, a dispatcher's +2 below its
// table's length); the six helpers with arguments kCall.
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_5b.h"
#include "game/effect_5b_callees.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace effect_5b {
namespace {

namespace sh = scenario_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using U = std::uint32_t;

// tools/band_rows.py --group E5B --clones, 2026-10-03 (0x500320 cut at 0x5003A0).
constexpr sh::CallSite kCalls4FF340[] = {{0x1D, 0x57C140}, {0x2D, 0x4FF5A0}, {0x4C, 0x4FF5A0}};
constexpr sh::CallSite kCalls4FF3A0[] = {{0x13, 0x57C140}, {0x3A, 0x587740}};
constexpr sh::CallSite kCalls4FF400[] = {{0x1A, 0x4FF5A0}, {0x55, 0x4FF610}};
constexpr sh::CallSite kCalls4FF460[] = {{0x13, 0x57C140}, {0x62, 0x5B93D2}, {0x7F, 0x4FF610}};
constexpr sh::CallSite kCalls4FF4F0[] = {{0x27, 0x4FF5A0}};
constexpr sh::CallSite kCalls4FF540[] = {{0x1A, 0x4FF5A0}, {0x52, 0x4FF610}};
constexpr sh::CallSite kCalls4FF5A0[] = {{0x57, 0x5A7810}, {0x60, 0x461E50}};
constexpr sh::CallSite kCalls4FF610[] = {{0x86, 0x5A8250}, {0x106, 0x5A77C0}, {0x111, 0x572FA0}, {0x11C, 0x5B9550},
                                         {0x154, 0x5B9550}, {0x167, 0x5B9550}, {0x17A, 0x5B9550}, {0x19D, 0x5A7A00},
                                         {0x1CC, 0x5B9550}, {0x1E9, 0x5A7A00}, {0x21B, 0x5B9550}, {0x244, 0x5A75F0},
                                         {0x24C, 0x5A7780}, {0x255, 0x5A9110}, {0x30A, 0x572FA0}, {0x333, 0x5A77C0},
                                         {0x346, 0x572FA0}};
constexpr sh::CallSite kCalls4FF990[] = {{0x7, 0x57C140}, {0x18, 0x587740}, {0x4F, 0x4FFA90}};
constexpr sh::CallSite kCalls4FF9F0[] = {{0xE, 0x4FFA90}};
constexpr sh::CallSite kCalls4FFA30[] = {{0x16, 0x4FFA90}, {0x4F, 0x589840}};
constexpr sh::CallSite kCalls4FFA90[] = {{0x1E, 0x5A75D0}, {0x26, 0x5A77A0}, {0xCF, 0x5A85F0},
                                         {0xD5, 0x5A9290}, {0xDE, 0x572A00}, {0xFD, 0x572FA0}};
constexpr sh::CallSite kCalls4FFBD0[] = {{0x1F, 0x57C140}, {0x2D, 0x4FFDD0}, {0x42, 0x4FFDD0}};
constexpr sh::CallSite kCalls4FFC30[] = {{0x15, 0x57C140}, {0x26, 0x587740}};
constexpr sh::CallSite kCalls4FFC70[] = {{0x16, 0x4FFDD0}};
constexpr sh::CallSite kCalls4FFCC0[] = {{0x15, 0x57C140}, {0x4E, 0x5B93D2}};
constexpr sh::CallSite kCalls4FFD30[] = {{0x20, 0x4FFDD0}};
constexpr sh::CallSite kCalls4FFD80[] = {{0x16, 0x4FFDD0}};
constexpr sh::CallSite kCalls4FFDD0[] = {{0x58, 0x5A7810}, {0x61, 0x461E50}};
constexpr sh::CallSite kCalls4FFE60[] = {{0x13, 0x57C140}, {0x65, 0x500160}, {0x111, 0x500160}};
constexpr sh::CallSite kCalls4FFF80[] = {{0x17, 0x587740}, {0x84, 0x587740}, {0x96, 0x500160}};
constexpr sh::CallSite kCalls500020[] = {{0x1A, 0x500160}};
constexpr sh::CallSite kCalls500040[] = {{0x6A, 0x500160}};
constexpr sh::CallSite kCalls5000B0[] = {{0x16, 0x4FFF80}, {0x38, 0x587740}, {0x40, 0x500160}};
constexpr sh::CallSite kCalls500100[] = {{0x7, 0x57C140}, {0x58, 0x500160}};
constexpr sh::CallSite kCalls500160[] = {{0x24, 0x5A77C0}, {0x2D, 0x461E50}, {0x39, 0x5A75D0}, {0x41, 0x5A77A0},
                                         {0xA7, 0x5720C0}, {0xC4, 0x5720C0}, {0x101, 0x5720C0}, {0x121, 0x5720C0},
                                         {0x16A, 0x5A85F0}, {0x170, 0x5A9290}, {0x17D, 0x572A00}, {0x189, 0x461E50}};
constexpr sh::CallSite kCalls500320[] = {{0x53, 0x57C140}, {0x67, 0x5003A0}, {0x76, 0x5004A0}};
constexpr sh::CallSite kCalls5003A0[] = {{0x17, 0x57C140}, {0x4D, 0x57C140}, {0x6A, 0x500610}, {0x72, 0x5007B0}};
constexpr sh::CallSite kCalls500420[] = {{0x20, 0x500610}, {0x47, 0x57C140}, {0x64, 0x500610}, {0x6C, 0x5007B0}};
constexpr sh::CallSite kCalls5004A0[] = {{0x17, 0x57C140}, {0x38, 0x500610}};
constexpr sh::CallSite kCalls5004F0[] = {{0x32, 0x5A77C0}, {0x48, 0x572FA0}, {0x69, 0x57C140}, {0x86, 0x500610},
                                         {0x92, 0x5A75B0}, {0x9A, 0x5A7780}, {0xE3, 0x5A85F0}, {0xE9, 0x5A9240},
                                         {0x102, 0x572FA0}, {0x10A, 0x5007B0}};
constexpr sh::CallSite kCalls500610[] = {{0x88, 0x5720C0}, {0xA7, 0x5720C0}, {0xE4, 0x5720C0}, {0x103, 0x5720C0},
                                         {0x123, 0x5A75D0}, {0x12B, 0x5A77A0}, {0x15E, 0x5A85F0}, {0x167, 0x5A9290},
                                         {0x17A, 0x572A00}, {0x190, 0x572FA0}};
constexpr sh::CallSite kCalls5007B0[] = {{0x16, 0x5A77C0}, {0x2C, 0x572FA0}, {0x38, 0x5A7650}, {0x40, 0x5A7780},
                                         {0x79, 0x5720C0}, {0xC4, 0x5720C0}, {0x112, 0x5A84A0}, {0x124, 0x5A9130},
                                         {0x13A, 0x572FA0}, {0x150, 0x5A77C0}, {0x169, 0x572FA0}};
constexpr sh::CallSite kCalls500950[] = {{0x64, 0x5720C0}, {0xC6, 0x57C140}, {0x140, 0x500EF0}, {0x269, 0x500EF0}};
constexpr sh::CallSite kCalls500BD0[] = {{0x29, 0x587740}, {0x100, 0x587740}, {0x114, 0x500EF0}};
constexpr sh::CallSite kCalls500CF0[] = {{0x1C, 0x500EF0}};
constexpr sh::CallSite kCalls500E00[] = {{0x16, 0x500BD0}, {0x35, 0x587740}, {0x49, 0x500EF0}};
constexpr sh::CallSite kCalls500E50[] = {{0xE, 0x57C140}, {0x8C, 0x500EF0}};
constexpr sh::CallSite kCalls500EF0[] = {{0x23, 0x5A77C0}, {0x2C, 0x461E50}, {0x38, 0x5A75D0}, {0x40, 0x5A77A0},
                                         {0x157, 0x5720C0}, {0x17D, 0x5720C0}, {0x1BE, 0x5720C0}, {0x1E4, 0x5720C0},
                                         {0x25E, 0x5A85F0}, {0x264, 0x5A9290}, {0x28C, 0x572A00}, {0x295, 0x461E50}};
constexpr sh::CallSite kCalls5011C0[] = {{0x65, 0x5720C0}, {0x1CE, 0x500EF0}};
constexpr sh::CallSite kCalls5013A0[] = {{0x1F, 0x587740}, {0x84, 0x500EF0}};
constexpr sh::CallSite kCalls501430[] = {{0xB8, 0x500EF0}};

#define E5B_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define E5B_CALLS(a) a, E5B_N(a)
#define E5B_FN(name) reinterpret_cast<const void*>(&::name)
constexpr sh::Shape kEf = sh::Shape::kEffect, kCa = sh::Shape::kCall;
// {name, base, size, calls, imms, tables, ours, ret_mask, calm, shape, pointers, state_span, sub_span, kind}
const sh::Clone kAll[] = {
    {"EffectKind18Sub0D_Run", 0x4FF320, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E5B_FN(EffectKind18Sub0D_Run), 0, false, kEf, 0, 0, 6, 0x18},
    {"EffectKind18Sub0D_Start", 0x4FF340, 0x5F, E5B_CALLS(kCalls4FF340), nullptr, 0, nullptr, 0, E5B_FN(EffectKind18Sub0D_Start), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub0D_Wait", 0x4FF3A0, 0x55, E5B_CALLS(kCalls4FF3A0), nullptr, 0, nullptr, 0, E5B_FN(EffectKind18Sub0D_Wait), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub0D_Open", 0x4FF400, 0x5C, E5B_CALLS(kCalls4FF400), nullptr, 0, nullptr, 0, E5B_FN(EffectKind18Sub0D_Open), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub0D_Idle", 0x4FF460, 0x86, E5B_CALLS(kCalls4FF460), nullptr, 0, nullptr, 0, E5B_FN(EffectKind18Sub0D_Idle), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub0D_Shut", 0x4FF4F0, 0x4D, E5B_CALLS(kCalls4FF4F0), nullptr, 0, nullptr, 0, E5B_FN(EffectKind18Sub0D_Shut), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub0D_Flash", 0x4FF540, 0x59, E5B_CALLS(kCalls4FF540), nullptr, 0, nullptr, 0, E5B_FN(EffectKind18Sub0D_Flash), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub0D_MoveFrame", 0x4FF5A0, 0x69, E5B_CALLS(kCalls4FF5A0), nullptr, 0, nullptr, 0, E5B_FN(EffectKind18Sub0D_MoveFrame), 0, false, kCa, 0, 0, 0, 0x18},
    {"EffectKind18Sub0D_DrawGlow", 0x4FF610, 0x356, E5B_CALLS(kCalls4FF610), nullptr, 0, nullptr, 0, E5B_FN(EffectKind18Sub0D_DrawGlow), 0, false, kCa, 0, 0, 0, 0x18},
    {"EffectKind18Sub0B_Run", 0x4FF970, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E5B_FN(EffectKind18Sub0B_Run), 0, false, kEf, 0, 0, 3, 0x18},
    {"EffectKind18Sub0B_Start", 0x4FF990, 0x58, E5B_CALLS(kCalls4FF990), nullptr, 0, nullptr, 0, E5B_FN(EffectKind18Sub0B_Start), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub0B_Rise", 0x4FF9F0, 0x36, E5B_CALLS(kCalls4FF9F0), nullptr, 0, nullptr, 0, E5B_FN(EffectKind18Sub0B_Rise), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub0B_Fade", 0x4FFA30, 0x55, E5B_CALLS(kCalls4FFA30), nullptr, 0, nullptr, 0, E5B_FN(EffectKind18Sub0B_Fade), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub0B_DrawTiles", 0x4FFA90, 0x119, E5B_CALLS(kCalls4FFA90), nullptr, 0, nullptr, 0, E5B_FN(EffectKind18Sub0B_DrawTiles), 0, false, kCa, 0, 0, 0, 0x18},
    {"EffectKind18Sub0C_Run", 0x4FFBB0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E5B_FN(EffectKind18Sub0C_Run), 0, false, kEf, 0, 0, 6, 0x18},
    {"EffectKind18Sub0C_Start", 0x4FFBD0, 0x55, E5B_CALLS(kCalls4FFBD0), nullptr, 0, nullptr, 0, E5B_FN(EffectKind18Sub0C_Start), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub0C_Wait", 0x4FFC30, 0x40, E5B_CALLS(kCalls4FFC30), nullptr, 0, nullptr, 0, E5B_FN(EffectKind18Sub0C_Wait), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub0C_Open", 0x4FFC70, 0x43, E5B_CALLS(kCalls4FFC70), nullptr, 0, nullptr, 0, E5B_FN(EffectKind18Sub0C_Open), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub0C_Idle", 0x4FFCC0, 0x62, E5B_CALLS(kCalls4FFCC0), nullptr, 0, nullptr, 0, E5B_FN(EffectKind18Sub0C_Idle), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub0C_Shut", 0x4FFD30, 0x45, E5B_CALLS(kCalls4FFD30), nullptr, 0, nullptr, 0, E5B_FN(EffectKind18Sub0C_Shut), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub0C_Flash", 0x4FFD80, 0x44, E5B_CALLS(kCalls4FFD80), nullptr, 0, nullptr, 0, E5B_FN(EffectKind18Sub0C_Flash), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub0C_MoveFrame", 0x4FFDD0, 0x6A, E5B_CALLS(kCalls4FFDD0), nullptr, 0, nullptr, 0, E5B_FN(EffectKind18Sub0C_MoveFrame), 0, false, kCa, 0, 0, 0, 0x18},
    {"EffectKind18Sub0E_Run", 0x4FFE40, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E5B_FN(EffectKind18Sub0E_Run), 0, false, kEf, 0, 0, 6, 0x18},
    {"EffectKind18Sub0E_Start", 0x4FFE60, 0x118, E5B_CALLS(kCalls4FFE60), nullptr, 0, nullptr, 0, E5B_FN(EffectKind18Sub0E_Start), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub0E_Wait", 0x4FFF80, 0x9D, E5B_CALLS(kCalls4FFF80), nullptr, 0, nullptr, 0, E5B_FN(EffectKind18Sub0E_Wait), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub0E_Lower", 0x500020, 0x1F, E5B_CALLS(kCalls500020), nullptr, 0, nullptr, 0, E5B_FN(EffectKind18Sub0E_Lower), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub0E_Open", 0x500040, 0x6F, E5B_CALLS(kCalls500040), nullptr, 0, nullptr, 0, E5B_FN(EffectKind18Sub0E_Open), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub0E_Raise", 0x5000B0, 0x46, E5B_CALLS(kCalls5000B0), nullptr, 0, nullptr, 0, E5B_FN(EffectKind18Sub0E_Raise), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub0E_Closed", 0x500100, 0x5D, E5B_CALLS(kCalls500100), nullptr, 0, nullptr, 0, E5B_FN(EffectKind18Sub0E_Closed), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub0E_DrawGate", 0x500160, 0x198, E5B_CALLS(kCalls500160), nullptr, 0, nullptr, 0, E5B_FN(EffectKind18Sub0E_DrawGate), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub13_Run", 0x500300, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E5B_FN(EffectKind18Sub13_Run), 0, false, kEf, 0, 0, 5, 0x18},
    {"EffectKind18Sub13_Start", 0x500320, 0x80, E5B_CALLS(kCalls500320), nullptr, 0, nullptr, 0, E5B_FN(EffectKind18Sub13_Start), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub13_Shown", 0x5003A0, 0x77, E5B_CALLS(kCalls5003A0), nullptr, 0, nullptr, 0, E5B_FN(EffectKind18Sub13_Shown), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub13_Fall", 0x500420, 0x71, E5B_CALLS(kCalls500420), nullptr, 0, nullptr, 0, E5B_FN(EffectKind18Sub13_Fall), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub13_Hidden", 0x5004A0, 0x41, E5B_CALLS(kCalls5004A0), nullptr, 0, nullptr, 0, E5B_FN(EffectKind18Sub13_Hidden), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub13_Rise", 0x5004F0, 0x114, E5B_CALLS(kCalls5004F0), nullptr, 0, nullptr, 0, E5B_FN(EffectKind18Sub13_Rise), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub13_DrawPanel", 0x500610, 0x19D, E5B_CALLS(kCalls500610), nullptr, 0, nullptr, 0, E5B_FN(EffectKind18Sub13_DrawPanel), 0, false, kCa, 0, 0, 0, 0x18},
    {"EffectKind18Sub13_DrawLine", 0x5007B0, 0x177, E5B_CALLS(kCalls5007B0), nullptr, 0, nullptr, 0, E5B_FN(EffectKind18Sub13_DrawLine), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub0F_Run", 0x500930, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E5B_FN(EffectKind18Sub0F_Run), 0, false, kEf, 0, 0, 6, 0x18},
    {"EffectKind18Sub0F_Start", 0x500950, 0x273, E5B_CALLS(kCalls500950), nullptr, 0, nullptr, 0, E5B_FN(EffectKind18Sub0F_Start), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub0F_Wait", 0x500BD0, 0x11E, E5B_CALLS(kCalls500BD0), nullptr, 0, nullptr, 0, E5B_FN(EffectKind18Sub0F_Wait), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub0F_Lower", 0x500CF0, 0x23, E5B_CALLS(kCalls500CF0), nullptr, 0, nullptr, 0, E5B_FN(EffectKind18Sub0F_Lower), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub0F_Open", 0x500D20, 0xD1, nullptr, 0, nullptr, 0, nullptr, 0, E5B_FN(EffectKind18Sub0F_Open), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub0F_Raise", 0x500E00, 0x50, E5B_CALLS(kCalls500E00), nullptr, 0, nullptr, 0, E5B_FN(EffectKind18Sub0F_Raise), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub0F_Closed", 0x500E50, 0x93, E5B_CALLS(kCalls500E50), nullptr, 0, nullptr, 0, E5B_FN(EffectKind18Sub0F_Closed), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub0F_DrawGate", 0x500EF0, 0x2A3, E5B_CALLS(kCalls500EF0), nullptr, 0, nullptr, 0, E5B_FN(EffectKind18Sub0F_DrawGate), 0, false, kCa, 0, 0, 0, 0x18},
    {"EffectKind18Sub4F_Run", 0x5011A0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E5B_FN(EffectKind18Sub4F_Run), 0, false, kEf, 0, 0, 5, 0x18},
    {"EffectKind18Sub4F_Start", 0x5011C0, 0x1DA, E5B_CALLS(kCalls5011C0), nullptr, 0, nullptr, 0, E5B_FN(EffectKind18Sub4F_Start), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub4F_Wait", 0x5013A0, 0x8B, E5B_CALLS(kCalls5013A0), nullptr, 0, nullptr, 0, E5B_FN(EffectKind18Sub4F_Wait), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub4F_Open", 0x501430, 0xC3, E5B_CALLS(kCalls501430), nullptr, 0, nullptr, 0, E5B_FN(EffectKind18Sub4F_Open), 0, false, kEf, 0, 0, 0, 0x18},
};
#undef E5B_FN
#undef E5B_CALLS
#undef E5B_N

enum : unsigned {
    k0DRun, k0DStart, k0DWait, k0DOpen, k0DIdle, k0DShut, k0DFlash, k0DMove, k0DGlow,
    k0BRun, k0BStart, k0BRise, k0BFade, k0BTiles,
    k0CRun, k0CStart, k0CWait, k0COpen, k0CIdle, k0CShut, k0CFlash, k0CMove,
    k0ERun, k0EStart, k0EWait, k0ELower, k0EOpen, k0ERaise, k0EClosed, k0EGate,
    k13Run, k13Start, k13Shown, k13Fall, k13Hidden, k13Rise, k13Panel, k13Line,
    k0FRun, k0FStart, k0FWait, k0FLower, k0FOpen, k0FRaise, k0FClosed, k0FGate,
    k4FRun, k4FStart, k4FWait, k4FOpen, kCount
};
static_assert(kCount == sizeof kAll / sizeof kAll[0], "one enum entry a clone, in order");
static_assert(kCount == 50, "the cut's 48 and the two added");

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

// A float for a screen coordinate about [lo, hi]: inside with a fraction most of
// the time, at or just past the bounds, far out, and one time in sixteen a NaN
// (quiet or signalling) or an infinity.
U ScreenFloat(float lo, float hi) {
    const U n = sh::Noise();
    switch (n % 16) {
    case 0: {
        static const U kOdd[] = {0x7FC00000u, 0xFFC00000u, 0x7F800001u, 0x7F800000u, 0xFF800000u, 0x4F000000u};
        return kOdd[(n >> 4) % 6];
    }
    case 1: { const float f = lo; U u; std::memcpy(&u, &f, 4); return u; }
    case 2: { const float f = hi; U u; std::memcpy(&u, &f, 4); return u; }
    case 3: { const float f = lo - 0.25f; U u; std::memcpy(&u, &f, 4); return u; }
    case 4: { const float f = hi + 0.25f; U u; std::memcpy(&u, &f, 4); return u; }
    default: {
        const float f = lo - 8.0f + (hi - lo + 16.0f) * static_cast<float>((n >> 4) & 0xFFFF) / 65536.0f +
                        static_cast<float>((n >> 20) & 0xFF) / 256.0f;
        U u;
        std::memcpy(&u, &f, 4);
        return u;
    }
    }
}
// Gte_RotTransPers(vertex, sxy, p) for the glow: the screen point about the
// cull's bounds (-60..380, -150..300) with fractions, the depth word noise; the
// answer (the depth the glow divides by) 0, small either sign or garbage.
U FxProjectGlow(const U* a, U answer) {
    if (Writable(a[1], 8)) {
        const U x = ScreenFloat(-60.0f, 380.0f);
        const U y = ScreenFloat(-150.0f, 300.0f);
        std::memcpy(P(a[1]), &x, 4);
        std::memcpy(P(a[1] + 4), &y, 4);
    }
    if (Writable(a[2], 4)) {
        const U n = sh::Noise();
        std::memcpy(P(a[2]), &n, 4);
    }
    switch (answer % 5) {
    case 0: return 0;
    case 1: return 1 + (answer >> 8) % 0x400;
    case 2: return 0u - (1 + (answer >> 8) % 0x400);
    case 3: return 0x100 + (answer >> 8) % 0x2000;
    default: return answer;
    }
}
// MapView_LinkPrimAt(x, z, dy, size): as the effect row (the packet cursor moves
// by size & 0xFF two times in three) but the dy and size compared as the bytes
// the callee reads (symbols.toml: "signed byte dy", "size & 0xFF") -
// EffectKind18Sub0B_DrawTiles pushes its dy as a byte over a register's leftover.
U FxLink(const U* a, U answer) {
    if (sh::Noise() % 3 != 0) {
        unsigned char* const next = Gfx_PacketNext;
        const unsigned n = a[3] & 0xFF;
        if (sh::InRegions(next, n + 0x40)) Gfx_PacketNext = next + n;
    }
    return answer;
}

#define E5B_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
constexpr sh::Answer kG = sh::Answer::kGarbage, kPh = sh::Answer::kPhase;
constexpr U kW = 0xFFFFFFFFu, k16 = 0xFFFFu, k8 = 0xFFu;
const sh::Callee kCallees[] = {
    // the group's own called directly, by name
    {E5B_OURS(EffectKind18Sub0D_MoveFrame), 2, {kW, kW}, kG, 0, 0},
    {E5B_OURS(EffectKind18Sub0D_DrawGlow), 1, {kW}, kG, 0, 0},
    // the height is read as its low word (bp)
    {E5B_OURS(EffectKind18Sub0B_DrawTiles), 2, {k16, kW}, kG, 0, 0},
    {E5B_OURS(EffectKind18Sub0C_MoveFrame), 1, {kW}, kG, 0, 0},
    {E5B_OURS(EffectKind18Sub0E_Wait), 0, {}, kPh, 0, 0},
    {E5B_OURS(EffectKind18Sub0E_DrawGate), 0, {}, kPh, 0, 0},
    {E5B_OURS(EffectKind18Sub13_Shown), 0, {}, kPh, 0, 0},
    {E5B_OURS(EffectKind18Sub13_Hidden), 0, {}, kPh, 0, 0},
    {E5B_OURS(EffectKind18Sub13_DrawPanel), 2, {kW, kW}, kG, 0, 0},
    {E5B_OURS(EffectKind18Sub13_DrawLine), 0, {}, kPh, 0, 0},
    {E5B_OURS(EffectKind18Sub0F_Wait), 0, {}, kPh, 0, 0},
    {E5B_OURS(EffectKind18Sub0F_DrawGate), 1, {kW}, kG, 0, 0},
    // re-listed (section 4): the glow's projection near the cull, its depth
    // small; the link's dy and size as bytes
    {E5B_OURS(Gte_RotTransPers), 3, {0, 0, 0}, kG, 0, 0, {6, 0, 0}, &FxProjectGlow, nullptr, true},
    {E5B_OURS(MapView_LinkPrimAt), 4, {kW, kW, k8, k8}, kG, 0, 0, {}, &FxLink, nullptr, true},
};
#undef E5B_OURS

// The seven tables the dispatchers jump through by +2, read in place; each
// table's own length (symbols.toml [[data]], none bounded by a compare).
const sh::DataTable kTables[] = {
    {0x65DF4C, 6}, {0x65DF80, 3}, {0x65DFCC, 6}, {0x65DFF8, 6}, {0x65E018, 5}, {0x65E068, 6}, {0x65E080, 5},
};
const std::uint8_t kKinds[] = {0x18};

// Beyond effect mode's standard regions: area 48's count, the tail state byte.
const sh::Region kRegions[] = {
    {at::kCount48, 1},
    {at::kTailState, 4},
};

// --- the seed ------------------------------------------------------------------------

unsigned char* Rec(unsigned r) { return sh::EffectRecord(r); }

bool Column0D(unsigned k) { return k <= k0DGlow; }
bool Column0C(unsigned k) { return k >= k0CRun && k <= k0CMove; }
bool Sub13(unsigned k) { return k >= k13Run && k <= k13Line; }
bool GateStart(unsigned k) { return k == k0FStart || k == k4FStart; }

// Every one of the 20 records the disturbance may move Sprite_Current to:
// what the function k reads by index kept inside the arrays (sub-kind 0x0D's
// column below 4, 0x0C's below 8, 0x13's +0xB below 4, a gate start's variant
// below 8 and flag index below 4), the gates' cells small (the writes stay in
// the area block), +9 / +0x30 / +0x3C at their compares, +8 0 or 1.
void Records(unsigned k) {
    for (unsigned r = 0; r < 20; ++r) {
        unsigned char* const e = Rec(r);
        if (Column0D(k)) SetWord(e + 0x36, PickOf(0, 0, 1, 2, 3, sh::Next() % 4));
        else if (Column0C(k)) SetWord(e + 0x36, PickOf(0, 0, 1, 5, 7, sh::Next() % 8));
        else if (GateStart(k)) SetWord(e + 0x36, PickOf(0, 0, 1, 7, sh::Next() % 8));
        else SetWord(e + 0x36, sh::Next() % 0x48);
        if (GateStart(k)) SetWord(e + 0x3A, sh::Next() % 4);
        else SetWord(e + 0x3A, sh::Next() % 0x1D);
        if (Sub13(k)) {
            e[0x36] = static_cast<unsigned char>(sh::Next() % 4);
            e[0x37] = 0;
            e[0xB] = static_cast<unsigned char>(sh::Next() % 4);
        }
        if (k == k0DIdle || k == k0CIdle)
            e[9] = static_cast<unsigned char>(PickOf(0, 1, 0x3F, 0x40, 0x41, 0xFF, sh::Next()));
        else if (Column0D(k) || Column0C(k))   // the frame arrays by +9 >> 1 and +9 / 3: below 8
            e[9] = static_cast<unsigned char>(PickOf(0, 1, 2, 3, 4, 5, 6, 7, sh::Next() % 8));
        else
            e[9] = static_cast<unsigned char>(PickOf(0, 1, 2, 9, 0xFF, sh::Next()));
        e[8] = static_cast<unsigned char>(PickOf(0, 1, 0, 1, sh::Next()));
        SetWord(e + 0x30, PickOf(0, 0xFF00, 0xFF20, 0xFF1F, 0xFF21, 0xFFE0, 0xFFDF, 0x20, 0x7FFF, sh::Next()));
        SetLong(e + 0x3C, static_cast<std::int32_t>(PickOf(0xFFFFFE80u, 0xFFFFFEFCu, 0xFFFFFF00u, 0xFFFFFEFBu,
                                                          0xFFFFFF7Cu, 0xFFFFFF80u, 0xFFFFFF7Bu, sh::Next())));
    }
}

// The leader within, at and past the gates' bounds of the record (16.16 offsets
// about the next row's or column's centre and the cell's corner).
void Leader(const unsigned char* s) {
    const U x = static_cast<U>(static_cast<std::int16_t>(Word(s + 0x36)));
    const U z = static_cast<U>(static_cast<std::int16_t>(Word(s + 0x3A)));
    const U off[] = {0, 0x8000, 0x8001, 0xFFFF8000u, 0xFFFF7FFFu, 0x10000, 0x10001, 0x20000, 0x20001,
                     0xFFFE0000u, 0xFFFDFFFFu, 0x30000, 0x30001, sh::Next()};
    const U n = sizeof off / sizeof off[0];
    U lx, lz;
    if (sh::Half()) {   // about the gate's cell, either axis
        const bool along_x = sh::Half();
        const U centre_x = along_x ? x << 16 : ((x + 1) << 16 | 0x8000u);
        const U centre_z = along_x ? ((z + 1) << 16 | 0x8000u) : z << 16;
        lx = centre_x + off[sh::Next() % n];
        lz = centre_z + off[sh::Next() % n];
    } else {            // about sub-kind 0x0E's fixed gate
        lx = 0x28000u + off[sh::Next() % n];
        lz = PickOf(0x150000, 0x160000) + off[sh::Next() % n];
    }
    SetLong(Mem(at::kLeaderX), static_cast<std::int32_t>(lx));
    SetLong(Mem(at::kLeaderZ), static_cast<std::int32_t>(lz));
}

void Seed(unsigned k) {
    Records(k);
    unsigned char* const s = Sprite_Current;
    Leader(s);
    AreaMap_Header[0] = static_cast<unsigned char>(PickOf(0x40, 0x20, 1, sh::Next() % 0x41));
    if (sh::Next() % 3 == 0) Field_Request = 0;
    if (sh::Next() % 3 == 0) Cond_ByteFD = 0;
    if (sh::Next() % 3 == 0) Cond_ByteFE = sh::Half() ? 0 : s[0xA];
    if (sh::Next() % 3 == 0) Mem(at::kCount48)[0] = 0;
    if (sh::Often()) Draw_PassFlags = static_cast<unsigned char>(Draw_PassFlags | 4);
    if (k == k13Start) s[0x36] = static_cast<unsigned char>(sh::Next() % 4);
}

// The helpers with arguments: a column below 4 (0x0D's glow reads four arrays by
// it) or a small signed one, a frame 0..4, a height and colour, a panel face and
// height, a ground flag.
void Args(unsigned k, U* a) {
    switch (k) {
    case k0DMove:
        a[0] = PickOf(0, 1, 2, 3, 0xFFFFFFFFu, 0x7F, sh::Next() & 0xFFFF);
        a[1] = PickOf(0, 1, 2, 3, 4, sh::Next() & 0xFF);
        break;
    case k0DGlow: a[0] = sh::Next() % 4; break;
    case k0BTiles:
        a[0] = PickOf(0xFFFFFE80u, 0xFFFFFF00u, 0xFFFFFF80u, sh::Next());
        a[1] = PickOf(0x800000, 0xF80000, 0x780000, 0, sh::Next());
        break;
    case k0CMove: a[0] = PickOf(0, 1, 2, 3, 4, sh::Next() & 0xFF); break;
    case k13Panel:
        a[0] = PickOf(0, 1, 4, 5, 8, sh::Next() & 0xFF);
        a[1] = PickOf(0x40, 0, 8, 0x48, sh::Next() & 0x7F);
        break;
    case k0FGate: a[0] = PickOf(0, 1, sh::Next()); break;
    default: break;
    }
}

// What the states read again after a call, moved by the group's case of the
// harness's disturbance (from its hash only): +9, +0x30, +0xB below 4,
// Cond_ByteFE against +0xA, area 48's count, the leader's x.
void Disturb(U h) {
    const U v = h >> 8;
    unsigned char* const s = Sprite_Current;
    if (!sh::InRegions(s, 0x80)) return;
    switch (h % 6) {
    case 0: s[9] = static_cast<unsigned char>((v & 1) ? 1u : (v >> 1) % 8); break;
    case 1: SetWord(s + 0x30, (v & 1) ? 0xFF00u : v >> 1); break;
    case 2: s[0xB] = static_cast<unsigned char>((v >> 1) % 4); break;
    case 3: Cond_ByteFE = static_cast<unsigned char>((v & 1) ? s[0xA] : v >> 1); break;
    case 4: Mem(at::kCount48)[0] = static_cast<unsigned char>((v & 1) ? 0u : v >> 1); break;
    case 5: SetLong(Mem(at::kLeaderX), static_cast<std::int32_t>(Long(Mem(at::kLeaderX)) + static_cast<std::int32_t>((v & 0xFFFF) - 0x8000))); break;
    default: break;
    }
}

}  // namespace

void SelfTest() {
    // BOF3X_E5B_ONLY: the clones whose name contains it (a control's run)
    static sh::Clone chosen[kCount];
    static unsigned index[kCount];
    const char* const only = std::getenv("BOF3X_E5B_ONLY");
    unsigned n = 0;
    for (unsigned k = 0; k < kCount; ++k)
        if (!only || !*only || std::strstr(kAll[k].name, only)) {
            index[n] = k;
            chosen[n++] = kAll[k];
        }
    if (n == 0) bof3::Fatal("effect_5b: BOF3X_E5B_ONLY=%s names no clone", only);
    static unsigned* s_index = index;
    const char* const rounds = std::getenv("BOF3X_E5B_ROUNDS");
    sh::Group g = {"effect_5b", chosen, n, kCallees, sizeof kCallees / sizeof kCallees[0], kTables,
                   sizeof kTables / sizeof kTables[0], kRegions, sizeof kRegions / sizeof kRegions[0],
                   [](unsigned k) { Seed(s_index[k]); }, &Disturb,
                   rounds && *rounds ? static_cast<unsigned>(std::strtoul(rounds, nullptr, 0)) : 4000u};
    g.args = [](unsigned k, U* a) { Args(s_index[k], a); };
    g.effect = true;
    g.kinds = kKinds;
    g.n_kinds = sizeof kKinds;
    sh::Run(g);
}

}  // namespace effect_5b

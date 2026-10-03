// BOF3X_SHADOW=effect_4e: group E4E's 49 functions through the scenario harness
// in effect mode (scenario_harness.h, docs/scenario_harness.md section 8), once
// at start-up. docs/effect_4e.md section 4. BOF3X_E4E_ONLY=<name> runs the
// clones whose name contains it (the controls' speed-up), BOF3X_E4E_ROUNDS the
// rounds per function.
//
// The clone table is tools/band_rows.py --group E4E --clones --harness scenario
// (2026-10-03), each extent read again to its last instruction (capstone) and
// the names given; the tool's extents stand (none differs from the code), and
// kind 0xA3's dispatcher 0x4912F0 (in no list) is added. Shapes: every
// dispatcher, state and no-argument helper kEffect (Sprite_Current one of the
// 20 Effect_Objects records, +5 the kind, a dispatcher's +1 below its table's
// length); the helpers with arguments kCall.
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_4e.h"
#include "game/effect_4e_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace effect_4e {
namespace {

namespace sh = scenario_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using U = std::uint32_t;

// tools/band_rows.py --group E4E --clones, 2026-10-03.
constexpr sh::CallSite kCalls48DFB0[] = {{0xD, 0x5720C0}};
constexpr sh::CallSite kCalls48E000[] = {{0x47, 0x48E0A0}};
constexpr sh::CallSite kCalls48E070[] = {{0x27, 0x48E0A0}};
constexpr sh::CallSite kCalls48E0A0[] = {{0x7, 0x494060}, {0xF, 0x5A7A50}, {0x27, 0x5A7A00}, {0x4C, 0x494110}, {0x68, 0x494110},
                                         {0xC3, 0x5A7A50}, {0xD7, 0x5A7A00}, {0xFC, 0x494110}, {0x118, 0x494110}, {0x12D, 0x5A79A0},
                                         {0x145, 0x5A77C0}, {0x158, 0x572FA0}, {0x164, 0x5A7610}, {0x16C, 0x5A7780}, {0x20E, 0x572FA0},
                                         {0x223, 0x5A79A0}, {0x23C, 0x5A77C0}, {0x24F, 0x572FA0}};
constexpr sh::CallSite kCalls48E340[] = {{0x26, 0x5720C0}};
constexpr sh::CallSite kCalls48E3D0[] = {{0x29, 0x48ED80}, {0x3E, 0x4901D0}, {0x70, 0x587740}};
constexpr sh::CallSite kCalls48E450[] = {{0x11, 0x48ED80}, {0x26, 0x4901D0}};
constexpr sh::CallSite kCalls48E4B0[] = {{0x22, 0x48E6F0}, {0x3B, 0x48ED80}, {0x50, 0x4901D0}};
constexpr sh::CallSite kCalls48E530[] = {{0x17, 0x48E6F0}, {0x2D, 0x48ED80}, {0x42, 0x4901D0}, {0x49, 0x48E800}};
constexpr sh::CallSite kCalls48E5B0[] = {{0x11, 0x48ED80}, {0x26, 0x4901D0}, {0x2D, 0x48E800}, {0x5F, 0x587740}};
constexpr sh::CallSite kCalls48E620[] = {{0x38, 0x48ED80}, {0x4D, 0x4901D0}, {0x54, 0x48E800}};
constexpr sh::CallSite kCalls48E6B0[] = {{0x14, 0x48E800}, {0x36, 0x589840}};
constexpr sh::CallSite kCalls48E6F0[] = {{0x4C, 0x5A79A0}, {0x64, 0x5A77C0}, {0x77, 0x572FA0}, {0x83, 0x5A75B0}, {0x8B, 0x5A7780},
                                         {0x99, 0x494110}, {0xA7, 0x494110}, {0xCC, 0x494110}, {0xDA, 0x494110}, {0xEC, 0x461E50}};
constexpr sh::CallSite kCalls48E800[] = {{0x3F, 0x48E8E0}, {0x63, 0x48E8E0}, {0x87, 0x48E8E0}, {0xA7, 0x48E8E0}, {0xC7, 0x48E8E0}};
constexpr sh::CallSite kCalls48E8E0[] = {{0x13, 0x5A79A0}, {0x2A, 0x5A77C0}, {0x33, 0x461E50}, {0x38, 0x494060}, {0x44, 0x5A7650},
                                         {0x4B, 0x5A7780}, {0x59, 0x494110}, {0x67, 0x494110}, {0x7A, 0x461E50}, {0x87, 0x5B9550},
                                         {0x9E, 0x5B9550}, {0xAF, 0x5A7A70}, {0xD4, 0x4941E0}, {0xFA, 0x5B9550}, {0x102, 0x5B9550},
                                         {0x108, 0x48EA80}, {0x11C, 0x4941E0}, {0x13E, 0x5B9550}, {0x147, 0x5B9550}, {0x14D, 0x48EA80},
                                         {0x15E, 0x5B9550}, {0x167, 0x5B9550}, {0x17A, 0x5B9550}, {0x182, 0x5B9550}, {0x188, 0x48EBB0}};
constexpr sh::CallSite kCalls48EA80[] = {{0x49, 0x5A75F0}, {0x51, 0x5A7780}, {0x65, 0x5A7A50}, {0x82, 0x5A7A00}, {0xB3, 0x5A7A50},
                                         {0xD0, 0x5A7A00}, {0x10C, 0x461E50}};
constexpr sh::CallSite kCalls48EBB0[] = {{0xC, 0x5A7610}, {0x14, 0x5A7780}, {0x69, 0x5A7A50}, {0x82, 0x5A7A00}, {0xB7, 0x5A7A50},
                                         {0xD4, 0x5A7A00}, {0x11C, 0x461E50}, {0x137, 0x5A7A50}, {0x159, 0x5A7A00}, {0x17A, 0x5A7A50},
                                         {0x19C, 0x5A7A00}, {0x1BC, 0x461E50}};
constexpr sh::CallSite kCalls48F5D0[] = {{0xD, 0x5A75B0}, {0x15, 0x5A7780}, {0x4D, 0x494110}, {0x9C, 0x494110}, {0xEB, 0x494110},
                                         {0x11B, 0x494110}, {0x136, 0x572FA0}};
constexpr sh::CallSite kCalls48F720[] = {{0x25, 0x5A75D0}, {0x2D, 0x5A7780}, {0x86, 0x494110}, {0xB7, 0x494110}, {0xE8, 0x494110},
                                         {0x138, 0x494110}, {0x185, 0x5A79A0}, {0x198, 0x5A79E0}, {0x1B8, 0x572FA0}};
constexpr sh::CallSite kCalls48F8F0[] = {{0xD, 0x5A75B0}, {0x15, 0x5A7780}, {0x4D, 0x494110}, {0x9C, 0x494110}, {0xD1, 0x494110},
                                         {0x13D, 0x494110}, {0x176, 0x572FA0}};
constexpr sh::CallSite kCalls48FA80[] = {{0x13, 0x5A79A0}, {0x2B, 0x5A77C0}, {0x3F, 0x572FA0}, {0x4B, 0x5A75B0}, {0x53, 0x5A7780},
                                         {0x87, 0x494110}, {0xD6, 0x494110}, {0x125, 0x494110}, {0x155, 0x494110}, {0x18E, 0x572FA0}};
constexpr sh::CallSite kCalls48FC40[] = {{0x53, 0x587740}};
constexpr sh::CallSite kCalls48FCA0[] = {{0x1A, 0x4901D0}};
constexpr sh::CallSite kCalls48FCF0[] = {{0x10, 0x4901D0}};
constexpr sh::CallSite kCalls48FD30[] = {{0x2B, 0x4901D0}, {0x64, 0x490630}, {0x95, 0x4906F0}};
constexpr sh::CallSite kCalls48FDF0[] = {{0x17, 0x490650}, {0x1C, 0x490810}, {0x21, 0x4906A0}};
constexpr sh::CallSite kCalls48FEB0[] = {{0x17, 0x490650}, {0x1C, 0x490810}, {0x21, 0x4906A0}, {0x3C, 0x587740}};
constexpr sh::CallSite kCalls48FF00[] = {{0x17, 0x490650}, {0x1E, 0x4906A0}, {0x23, 0x490810}, {0x37, 0x490390}, {0xFD, 0x490A30},
                                         {0x10F, 0x490A30}, {0x121, 0x490A30}, {0x134, 0x4906F0}, {0x14F, 0x490A30}, {0x161, 0x490A30},
                                         {0x173, 0x490A30}};
constexpr sh::CallSite kCalls490090[] = {{0x17, 0x490650}, {0x29, 0x490A30}, {0x3B, 0x490A30}, {0x4D, 0x490A30}, {0x52, 0x490650},
                                         {0x64, 0x490A30}, {0x76, 0x490A30}, {0x88, 0x490A30}, {0x90, 0x4906A0}, {0x95, 0x490810},
                                         {0xA9, 0x490390}, {0xDA, 0x587740}};
constexpr sh::CallSite kCalls490180[] = {{0x13, 0x4906A0}, {0x29, 0x490390}, {0x4A, 0x589840}};
constexpr sh::CallSite kCalls4901D0[] = {{0x17, 0x5A79A0}, {0x2F, 0x5A77C0}, {0x38, 0x461E50}, {0x3D, 0x494060}, {0x4C, 0x494110},
                                         {0x6B, 0x4941E0}, {0xD9, 0x5A75F0}, {0xE1, 0x5A7780}, {0x10B, 0x5A7A50}, {0x12C, 0x5A7A00},
                                         {0x198, 0x461E50}};
constexpr sh::CallSite kCalls490390[] = {{0xE, 0x5A7610}, {0x16, 0x5A7780}, {0x25, 0x494110}, {0x6A, 0x494110}, {0xD9, 0x461E50},
                                         {0xE5, 0x5A7610}, {0xED, 0x5A7780}, {0xF8, 0x494110}, {0x139, 0x494110}, {0x1A7, 0x461E50},
                                         {0x207, 0x494110}, {0x213, 0x5A7750}, {0x21B, 0x5A7780}, {0x243, 0x461E50}};
constexpr sh::CallSite kCalls4906A0[] = {{0x5, 0x494060}, {0x31, 0x4820C0}};
constexpr sh::CallSite kCalls4906F0[] = {{0x2D, 0x5B93D2}, {0x3A, 0x5B93D2}, {0x4D, 0x5B93D2}, {0x5C, 0x5A7A50}, {0x67, 0x5A7A00},
                                         {0x78, 0x5A7A50}, {0x83, 0x5A7A00}, {0x97, 0x494180}, {0xA5, 0x5A7F10}, {0xB5, 0x5A7F80},
                                         {0xC3, 0x5A7FF0}, {0xD2, 0x5A7C70}, {0xE1, 0x5A7C70}, {0xE9, 0x5B93D2}};
constexpr sh::CallSite kCalls490810[] = {{0x12, 0x5A79A0}, {0x2A, 0x5A77C0}, {0x33, 0x461E50}, {0x3B, 0x494060}, {0x4E, 0x490880}};
constexpr sh::CallSite kCalls490880[] = {{0xE, 0x5A75F0}, {0x16, 0x5A7780}, {0x24, 0x494110}, {0x2E, 0x5A7A50}, {0x41, 0x5A7A00},
                                         {0x66, 0x5A7A00}, {0x79, 0x5A7A50}, {0xBE, 0x494110}, {0xC8, 0x5A7A50}, {0xDB, 0x5A7A00},
                                         {0x100, 0x5A7A00}, {0x113, 0x5A7A50}, {0x158, 0x494110}, {0x19B, 0x461E50}};
constexpr sh::CallSite kCalls491AA0[] = {{0x29, 0x491E30}};
constexpr sh::CallSite kCalls491C60[] = {{0x2D, 0x492260}};

#define E4E_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define E4E_CALLS(a) a, E4E_N(a)
#define E4E_FN(name) reinterpret_cast<const void*>(&::name)
constexpr sh::Shape kEf = sh::Shape::kEffect;
constexpr sh::Shape kCa = sh::Shape::kCall;
// {name, base, size, calls, imms, tables, ours, ret_mask, calm, shape, pointers, state_span, sub_span, kind}
const sh::Clone kAll[] = {
    {"EffectKind9B_Run", 0x48DF90, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E4E_FN(EffectKind9B_Run), 0, false, kEf, 0, 3, 0, 0x9B},
    {"EffectKind9B_Start", 0x48DFB0, 0x41, E4E_CALLS(kCalls48DFB0), nullptr, 0, nullptr, 0, E4E_FN(EffectKind9B_Start), 0, false, kEf, 0, 0, 0, 0x9B},
    {"EffectKind9B_Rise", 0x48E000, 0x64, E4E_CALLS(kCalls48E000), nullptr, 0, nullptr, 0, E4E_FN(EffectKind9B_Rise), 0, false, kEf, 0, 0, 0, 0x9B},
    {"EffectKind9B_Hold", 0x48E070, 0x30, E4E_CALLS(kCalls48E070), nullptr, 0, nullptr, 0, E4E_FN(EffectKind9B_Hold), 0, false, kEf, 0, 0, 0, 0x9B},
    {"EffectKind9B_DrawRing", 0x48E0A0, 0x271, E4E_CALLS(kCalls48E0A0), nullptr, 0, nullptr, 0, E4E_FN(EffectKind9B_DrawRing), 0, false, kCa, 0, 0, 0, 0x9B},
    {"EffectKind9C_Run", 0x48E320, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E4E_FN(EffectKind9C_Run), 0, false, kEf, 0, 8, 0, 0x9C},
    {"EffectKind9C_Start", 0x48E340, 0x8A, E4E_CALLS(kCalls48E340), nullptr, 0, nullptr, 0, E4E_FN(EffectKind9C_Start), 0, false, kEf, 0, 0, 0, 0x9C},
    {"EffectKind9C_Grow", 0x48E3D0, 0x77, E4E_CALLS(kCalls48E3D0), nullptr, 0, nullptr, 0, E4E_FN(EffectKind9C_Grow), 0, false, kEf, 0, 0, 0, 0x9C},
    {"EffectKind9C_Hold", 0x48E450, 0x5E, E4E_CALLS(kCalls48E450), nullptr, 0, nullptr, 0, E4E_FN(EffectKind9C_Hold), 0, false, kEf, 0, 0, 0, 0x9C},
    {"EffectKind9C_PanelsIn", 0x48E4B0, 0x7E, E4E_CALLS(kCalls48E4B0), nullptr, 0, nullptr, 0, E4E_FN(EffectKind9C_PanelsIn), 0, false, kEf, 0, 0, 0, 0x9C},
    {"EffectKind9C_PanelsOut", 0x48E530, 0x77, E4E_CALLS(kCalls48E530), nullptr, 0, nullptr, 0, E4E_FN(EffectKind9C_PanelsOut), 0, false, kEf, 0, 0, 0, 0x9C},
    {"EffectKind9C_Beams", 0x48E5B0, 0x66, E4E_CALLS(kCalls48E5B0), nullptr, 0, nullptr, 0, E4E_FN(EffectKind9C_Beams), 0, false, kEf, 0, 0, 0, 0x9C},
    {"EffectKind9C_Shrink", 0x48E620, 0x8E, E4E_CALLS(kCalls48E620), nullptr, 0, nullptr, 0, E4E_FN(EffectKind9C_Shrink), 0, false, kEf, 0, 0, 0, 0x9C},
    {"EffectKind9C_End", 0x48E6B0, 0x3C, E4E_CALLS(kCalls48E6B0), nullptr, 0, nullptr, 0, E4E_FN(EffectKind9C_End), 0, false, kEf, 0, 0, 0, 0x9C},
    {"EffectKind9C_DrawPanels", 0x48E6F0, 0x103, E4E_CALLS(kCalls48E6F0), nullptr, 0, nullptr, 0, E4E_FN(EffectKind9C_DrawPanels), 0, false, kCa, 0, 0, 0, 0x9C},
    {"EffectKind9C_DrawBeams", 0x48E800, 0xD6, E4E_CALLS(kCalls48E800), nullptr, 0, nullptr, 0, E4E_FN(EffectKind9C_DrawBeams), 0, false, kCa, 0, 0, 0, 0x9C},
    {"EffectKind9C_DrawBeam", 0x48E8E0, 0x198, E4E_CALLS(kCalls48E8E0), nullptr, 0, nullptr, 0, E4E_FN(EffectKind9C_DrawBeam), 0, false, kCa, 0, 0, 0, 0x9C},
    {"EffectKind9C_DrawBeamEnd", 0x48EA80, 0x12B, E4E_CALLS(kCalls48EA80), nullptr, 0, nullptr, 0, E4E_FN(EffectKind9C_DrawBeamEnd), 0, false, kCa, 0, 0, 0, 0x9C},
    {"EffectKind9C_DrawBeamSides", 0x48EBB0, 0x1CA, E4E_CALLS(kCalls48EBB0), nullptr, 0, nullptr, 0, E4E_FN(EffectKind9C_DrawBeamSides), 0, false, kCa, 0, 0, 0, 0x9C},
    {"EffectKind9E_Run", 0x48F070, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E4E_FN(EffectKind9E_Run), 0, false, kEf, 0, 7, 0, 0x9E},
    {"EffectKind9E_DrawPlate", 0x48F5D0, 0x145, E4E_CALLS(kCalls48F5D0), nullptr, 0, nullptr, 0, E4E_FN(EffectKind9E_DrawPlate), 0, false, kCa, 0, 0, 0, 0x9E},
    {"EffectKind9E_DrawTexPlate", 0x48F720, 0x1C8, E4E_CALLS(kCalls48F720), nullptr, 0, nullptr, 0, E4E_FN(EffectKind9E_DrawTexPlate), 0, false, kCa, 0, 0, 0, 0x9E},
    {"EffectKind9E_DrawWall", 0x48F8F0, 0x185, E4E_CALLS(kCalls48F8F0), nullptr, 0, nullptr, 0, E4E_FN(EffectKind9E_DrawWall), 0, false, kCa, 0, 0, 0, 0x9E},
    {"EffectKind9E_DrawShadePlate", 0x48FA80, 0x19D, E4E_CALLS(kCalls48FA80), nullptr, 0, nullptr, 0, E4E_FN(EffectKind9E_DrawShadePlate), 0, false, kCa, 0, 0, 0, 0x9E},
    {"EffectKindA0_Run", 0x48FC20, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E4E_FN(EffectKindA0_Run), 0, false, kEf, 0, 9, 0, 0xA0},
    {"EffectKindA0_Start", 0x48FC40, 0x5A, E4E_CALLS(kCalls48FC40), nullptr, 0, nullptr, 0, E4E_FN(EffectKindA0_Start), 0, false, kEf, 0, 0, 0, 0xA0},
    {"EffectKindA0_Grow", 0x48FCA0, 0x48, E4E_CALLS(kCalls48FCA0), nullptr, 0, nullptr, 0, E4E_FN(EffectKindA0_Grow), 0, false, kEf, 0, 0, 0, 0xA0},
    {"EffectKindA0_Hold", 0x48FCF0, 0x3E, E4E_CALLS(kCalls48FCF0), nullptr, 0, nullptr, 0, E4E_FN(EffectKindA0_Hold), 0, false, kEf, 0, 0, 0, 0xA0},
    {"EffectKindA0_Rise", 0x48FD30, 0xBF, E4E_CALLS(kCalls48FD30), nullptr, 0, nullptr, 0, E4E_FN(EffectKindA0_Rise), 0, false, kEf, 0, 0, 0, 0xA0},
    {"EffectKindA0_Sparkle", 0x48FDF0, 0xB7, E4E_CALLS(kCalls48FDF0), nullptr, 0, nullptr, 0, E4E_FN(EffectKindA0_Sparkle), 0, false, kEf, 0, 0, 0, 0xA0},
    {"EffectKindA0_Launch", 0x48FEB0, 0x43, E4E_CALLS(kCalls48FEB0), nullptr, 0, nullptr, 0, E4E_FN(EffectKindA0_Launch), 0, false, kEf, 0, 0, 0, 0xA0},
    {"EffectKindA0_Fly", 0x48FF00, 0x18F, E4E_CALLS(kCalls48FF00), nullptr, 0, nullptr, 0, E4E_FN(EffectKindA0_Fly), 0, false, kEf, 0, 0, 0, 0xA0},
    {"EffectKindA0_FlyWait", 0x490090, 0xE1, E4E_CALLS(kCalls490090), nullptr, 0, nullptr, 0, E4E_FN(EffectKindA0_FlyWait), 0, false, kEf, 0, 0, 0, 0xA0},
    {"EffectKindA0_Fade", 0x490180, 0x50, E4E_CALLS(kCalls490180), nullptr, 0, nullptr, 0, E4E_FN(EffectKindA0_Fade), 0, false, kEf, 0, 0, 0, 0xA0},
    {"EffectKindA0_DrawGlow", 0x4901D0, 0x1B7, E4E_CALLS(kCalls4901D0), nullptr, 0, nullptr, 0, E4E_FN(EffectKindA0_DrawGlow), 0, false, kCa, 0, 0, 0, 0xA0},
    {"EffectKindA0_DrawTrail", 0x490390, 0x293, E4E_CALLS(kCalls490390), nullptr, 0, nullptr, 0, E4E_FN(EffectKindA0_DrawTrail), 0, false, kCa, 0, 0, 0, 0xA0},
    {"EffectKindA0_ClearSparks", 0x490630, 0x14, nullptr, 0, nullptr, 0, nullptr, 0, E4E_FN(EffectKindA0_ClearSparks), 0, false, kEf, 0, 0, 0, 0xA0},
    {"EffectKindA0_SpawnSpark", 0x490650, 0x4A, nullptr, 0, nullptr, 0, nullptr, 0, E4E_FN(EffectKindA0_SpawnSpark), 0, false, kEf, 0, 0, 0, 0xA0},
    {"EffectKindA0_StepSparks", 0x4906A0, 0x45, E4E_CALLS(kCalls4906A0), nullptr, 0, nullptr, 0, E4E_FN(EffectKindA0_StepSparks), 0xFF, false, kEf, 0, 0, 0, 0xA0},
    {"EffectKindA0_InitShard", 0x4906F0, 0x114, E4E_CALLS(kCalls4906F0), nullptr, 0, nullptr, 0, E4E_FN(EffectKindA0_InitShard), 0, false, kCa, 0, 0, 0, 0xA0},
    {"EffectKindA0_DrawShards", 0x490810, 0x63, E4E_CALLS(kCalls490810), nullptr, 0, nullptr, 0, E4E_FN(EffectKindA0_DrawShards), 0, false, kEf, 0, 0, 0, 0xA0},
    {"EffectKindA0_DrawShard", 0x490880, 0x1AB, E4E_CALLS(kCalls490880), nullptr, 0, nullptr, 0, E4E_FN(EffectKindA0_DrawShard), 0, false, kCa, 0, 0, 0, 0xA0},
    {"EffectKindA0_SwapLong", 0x490A30, 0x23, nullptr, 0, nullptr, 0, nullptr, 0, E4E_FN(EffectKindA0_SwapLong), 0, false, kCa, 0, 0, 0, 0xA0},
    {"EffectKindA1_Run", 0x490A60, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E4E_FN(EffectKindA1_Run), 0, false, kEf, 0, 6, 0, 0xA1},
    {"EffectKindA2_Run", 0x4910D0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E4E_FN(EffectKindA2_Run), 0, false, kEf, 0, 4, 0, 0xA2},
    {"EffectKindA3_Run", 0x4912F0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E4E_FN(EffectKindA3_Run), 0, false, kEf, 0, 11, 0, 0xA3},
    {"EffectKindA7_Run", 0x491AA0, 0x32, E4E_CALLS(kCalls491AA0), nullptr, 0, nullptr, 0, E4E_FN(EffectKindA7_Run), 0, false, kEf, 0, 4, 0, 0xA7},
    {"EffectKindA8_Run", 0x491BA0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E4E_FN(EffectKindA8_Run), 0, false, kEf, 0, 3, 0, 0xA8},
    {"EffectKindA9_Run", 0x491C60, 0x36, E4E_CALLS(kCalls491C60), nullptr, 0, nullptr, 0, E4E_FN(EffectKindA9_Run), 0, false, kEf, 0, 4, 0, 0xA9},
};
#undef E4E_FN
#undef E4E_CALLS
#undef E4E_N

enum : unsigned {
    k9BRun, k9BStart, k9BRise, k9BHold, k9BRing,
    k9CRun, k9CStart, k9CGrow, k9CHold, k9CPanelsIn, k9CPanelsOut, k9CBeams, k9CShrink, k9CEnd,
    k9CPanels, k9CBeamsDraw, k9CBeam, k9CBeamEnd, k9CBeamSides,
    k9ERun, k9EPlate, k9ETexPlate, k9EWall, k9EShadePlate,
    kA0Run, kA0Start, kA0Grow, kA0Hold, kA0Rise, kA0Sparkle, kA0Launch, kA0Fly, kA0FlyWait, kA0Fade,
    kA0Glow, kA0Trail, kA0Clear, kA0Spawn, kA0Step, kA0InitShard, kA0Shards, kA0Shard, kA0Swap,
    kA1Run, kA2Run, kA3Run, kA7Run, kA8Run, kA9Run, kCount
};
static_assert(kCount == sizeof kAll / sizeof kAll[0], "one enum entry a clone, in order");

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
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

// The harness's packet buffer (scenario_harness.cpp g_packets, 0x800 bytes).
// The draws here write up to 0x88 bytes past the cursor before the next move
// (EffectKind9C_DrawBeamSides' copy), so the cursor stops 0x90 short of the
// end, not the harness's 0x40.
constexpr unsigned kPacketsSize = 0x800;
constexpr unsigned kSpare = 0x90;
void Advance(unsigned size) {
    unsigned char* const next = Gfx_PacketNext;
    unsigned char* const base = sh::Packets();
    if (next >= base && next + size + kSpare <= base + kPacketsSize) Gfx_PacketNext = next + size;
}
// Gfx_CommitPrim: the cursor += size (a byte) while the packet stays in the buffer.
U FxCommit(const U* a, U answer) {
    Advance(a[1] & 0xFF);
    return answer;
}
// MapView_LinkPrimAt: the cursor += size & 0xFF two times in three (the
// harness's fold row), with this file's spare.
U FxLink(const U* a, U answer) {
    if (sh::Noise() % 3 != 0) Advance(a[3] & 0xFF);
    return answer;
}
// EffectGte_ProjectSize(point, size, out): both callers write both words of
// the size, so all four bytes are hashed (the fold's row hashes the first
// word); out's two s16, small and positive half the time.
U FxProjectSize(const U* a, U answer) {
    if (Writable(a[2], 4)) {
        const U n = sh::Noise();
        const U v = (n & 1) ? (n >> 1) : ((n >> 1) & 0x003F003Fu);
        std::memcpy(P(a[2]), &v, 4);
    }
    return answer;
}
// EffectKindA0_SwapLong(a, b): swapped, as the real one (the states read the
// points again after it).
U FxSwap(const U* a, U answer) {
    if (Writable(a[0], 4) && Writable(a[1], 4)) {
        const U x = static_cast<U>(Long(P(a[0]))) ^ static_cast<U>(Long(P(a[1])));
        SetLong(P(a[0]), static_cast<std::int32_t>(x));
        const U y = static_cast<U>(Long(P(a[1]))) ^ x;
        SetLong(P(a[1]), static_cast<std::int32_t>(y));
        SetLong(P(a[0]), static_cast<std::int32_t>(x ^ y));
    }
    return answer;
}

#define E4E_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
constexpr sh::Answer kG = sh::Answer::kGarbage, kPh = sh::Answer::kPhase, kF = sh::Answer::kFlag;
constexpr U kW = 0xFFFFFFFFu, k16 = 0xFFFFu, k8 = 0xFFu;
const sh::Callee kCallees[] = {
    // the group's own called directly, by name: the void ones with no argument
    {E4E_OURS(EffectKindA0_ClearSparks), 0, {}, kPh, 0, 0},
    {E4E_OURS(EffectKindA0_SpawnSpark), 0, {}, kPh, 0, 0},
    {E4E_OURS(EffectKindA0_DrawShards), 0, {}, kPh, 0, 0},
    // answering in al (no caller reads it)
    {E4E_OURS(EffectKindA0_StepSparks), 0, {}, kF, 0, 0},
    // those with arguments, each mask what the callee reads: a local point
    // (the copies' and ours' stack differ) hashed and not logged; a record's
    // or a pool's point (same address on both sides) logged and hashed; the
    // bytes and words the callees read by `mov cl` / `mov cx` / movsx over
    // leftover upper bytes the callers push in a whole register
    {E4E_OURS(EffectKind9B_DrawRing), 2, {0, kW}, kG, 0, 0, {12}, nullptr, nullptr, true},
    {E4E_OURS(EffectKind9C_DrawPanels), 1, {k8}, kG, 0, 0},
    {E4E_OURS(EffectKind9C_DrawBeams), 1, {k16}, kG, 0, 0},
    {E4E_OURS(EffectKind9C_DrawBeam), 3, {0, 0, k16}, kG, 0, 0, {12, 12}, nullptr, nullptr, true},
    {E4E_OURS(EffectKind9C_DrawBeamEnd), 4, {k16, k16, k16, k16}, kG, 0, 0},
    {E4E_OURS(EffectKind9C_DrawBeamSides), 8, {k16, k16, k16, k16, k16, k16, k16, k16}, kG, 0, 0},
    {E4E_OURS(EffectKindA0_DrawGlow), 3, {kW, k16, k8}, kG, 0, 0, {12}, nullptr, nullptr, true},
    {E4E_OURS(EffectKindA0_DrawTrail), 3, {kW, kW, k8}, kG, 0, 0, {12, 12}, nullptr, nullptr, true},
    {E4E_OURS(EffectKindA0_InitShard), 1, {kW}, kG, 0, 0},
    {E4E_OURS(EffectKindA0_DrawShard), 1, {kW}, kG, 0, 0, {0x2C}, nullptr, nullptr, true},
    {E4E_OURS(EffectKindA0_SwapLong), 2, {kW, kW}, kG, 0, 0, {4, 4}, &FxSwap, nullptr, true},
    // group E3A's (wave three, merged): the spark of 0x18
    {E4E_OURS(EffectKind64_DrawSpark), 1, {kW}, kG, 0, 0, {0x18}, nullptr, nullptr, true},
    // catalog part 6's 0x491E30 (in no group): the point, the word +0xC (`mov
    // dx` over edx's leftover - the standard row compares the whole word), 7
    {"0x491E30", at::kGlowA7, at::kGlowA7, 3, {kW, k16, k8}, kG, 0, 0, {12}, nullptr, nullptr, true},
    // standard rows re-listed: the cursor's spare (0x90) for the commits and the
    // map links; EffectGte_ProjectSize hashing the size's two words
    {E4E_OURS(Gfx_CommitPrim), 2, {k8, k8}, kG, 0, 0, {0, 0}, &FxCommit, nullptr, true},
    {E4E_OURS(MapView_LinkPrimAt), 4, {kW, kW, kW, kW}, kG, 0, 0, {}, &FxLink, nullptr, true},
    {E4E_OURS(EffectGte_ProjectSize), 3, {0, 0, 0}, kG, 0, 0, {12, 4}, &FxProjectSize, nullptr, true},
};
#undef E4E_OURS

// The ten state tables the dispatchers jump (or call) through, read in place;
// each table's own length (symbols.toml [[data]], none bounded by a compare).
const sh::DataTable kTables[] = {
    {0x655190, 3}, {0x65519C, 8}, {0x6551BC, 7}, {0x6551E4, 9}, {0x655220, 6},
    {0x655238, 4}, {0x655248, 11}, {0x655274, 4}, {0x655284, 3}, {0x655290, 4},
};
const std::uint8_t kKinds[] = {0x9B, 0x9C, 0x9E, 0xA0, 0xA1, 0xA2, 0xA3, 0xA7, 0xA8, 0xA9};

// Beyond the standard effect regions: kind 0xA0's two pools.
const sh::Region kRegions[] = {
    {at::kSparks, at::kPoolsSize},
};

// --- the seed ------------------------------------------------------------------------

unsigned char* Rec(unsigned r) { return sh::EffectRecord(r); }
unsigned char* SparkAt(unsigned i) { return P(at::kSparks + i * at::kSparkStride); }
unsigned char* ShardAt(unsigned i) { return P(at::kShards + i * at::kShardStride); }

// The sparks: half in use, lives at 1.
void SeedSparks() {
    for (unsigned i = 0; i < at::kSparkCount; ++i) {
        unsigned char* const k = SparkAt(i);
        k[0] = static_cast<unsigned char>(sh::Half() ? 0 : PickOf(1, 1, 0x80, sh::Next()));
        k[2] = static_cast<unsigned char>(PickOf(1, 1, 2, 0, 0x10, sh::Next()));
    }
}
// The shards: the shade word at its clamps, the speed small and any.
void SeedShards() {
    for (unsigned i = 0; i < at::kShardCount; ++i) {
        unsigned char* const s = ShardAt(i);
        SetWord(s + 0x2A, PickOf(0, 0xFF, 0x100, 0xFFFF, 0x8000, 0x7FFF, 0x40, sh::Next()));
        SetWord(s + 0x28, PickOf(3, 5, 0, 0xFFFF, sh::Next()));
    }
}
// Every record's trail: the pull's x negative (0x100..0x4000), the head
// (+0x18) within 0x10000 ahead of and 0x80000 behind the point (+0x34) - so
// the dot loop ends inside a few hundred dots on both sides whichever record
// the harness makes current (docs/effect_4e.md section 4).
void SeedTrails() {
    for (unsigned r = 0; r < sh::at::kEffectCount; ++r) {
        unsigned char* const e = Rec(r);
        SetLong(e + 0x64, -static_cast<std::int32_t>(0x100 + sh::Next() % 0x3F00));
        SetLong(e + 0x34, Long(e + 0x18) + static_cast<std::int32_t>(sh::Next() % 0x90000) - 0x10000);
        if (sh::Half()) SetLong(e + 0x34, Long(e + 0x18) + PickOf(0, 1, 0xFFFFFFFFu, 0x10, 0x100));
    }
}

void Seed(unsigned k) {
    unsigned char* const s = Sprite_Current;
    s[9] = static_cast<unsigned char>(PickOf(1, 1, 0, 2, 0x3C, 0xFF, sh::Next()));
    s[6] = static_cast<unsigned char>(PickOf(0, 0, 4, 1, 2, 3, 5, 0xFC, sh::Next()));
    SetWord(s + 0x2E, PickOf(0, 0x10, 1, 0x11, 0xFFFF, 0xFFF0, 0x8000, 0x7FFF, 0x40, sh::Next()));
    s[0x5D] = static_cast<unsigned char>(PickOf(0, 8, 1, 7, 9, 0x80, 0x7F, 0xFF, sh::Next()));
    s[0x5E] = static_cast<unsigned char>(PickOf(0, 0xC0, 0x40, 0x80, sh::Next()));
    sh::Mem(at::kCounter)[0] = static_cast<unsigned char>(PickOf(0x14, 0x17, 0x13, 0x16, 0x18, sh::Next()));
    switch (k) {
    case k9BRise:
        // the height +0x14 after this step at, below and above 0x8000000
        SetLong(s + 0x20, static_cast<std::int32_t>(PickOf(0, 0x100000, 0x7F00000, sh::Next() & 0xFFFFFF, sh::Next())));
        if (sh::Often())
            SetLong(s + 0x14, static_cast<std::int32_t>(0x8000000u - (static_cast<U>(Long(s + 0x20)) + 0x100000u) +
                                                        PickOf(0, 0xFFFFFFFFu, 1, 0x100000, 0xFFF00000u)));
        break;
    case kA0Fly:
        // the head's x after this step at, below and above 0x2F8000
        if (sh::Often()) {
            const U speed = static_cast<U>(Long(s + 0xC)) + static_cast<U>(Long(s + 0x64));
            SetLong(s + 0x18, static_cast<std::int32_t>(0x2F8000u - speed + PickOf(0, 1, 0xFFFFFFFFu, 0x10000, 0xFFFF0000u)));
        }
        SeedSparks();
        break;
    case kA0Rise:
    case kA0Sparkle:
    case kA0Launch:
    case kA0FlyWait:
    case kA0Fade:
    case kA0Clear:
    case kA0Spawn:
    case kA0Step: SeedSparks(); break;
    case kA0Shards:
    case kA0Shard: SeedShards(); break;
    case kA0Trail: SeedTrails(); break;
    default: break;
    }
}

// The helpers with arguments: the record's own points (as the states pass
// them), a pool record, the sizes, shades and screen values over leftover upper
// bytes.
void Args(unsigned k, U* a) {
    const U s = Key(Sprite_Current);
    switch (k) {
    case k9BRing:
        a[0] = s + 0x34;
        a[1] = PickOf(0, 0x8000000, 0x100000, 0xFFF00000u, sh::Next());
        break;
    case k9CPanels: a[0] = (sh::Next() & 0xFFFFFF00u) | PickOf(0, 0xFF, 0x40, 0x80, 0xC0, sh::Next() & 0xFF); break;
    case k9CBeamsDraw: a[0] = (sh::Next() & 0xFFFF0000u) | PickOf(0, 0x40, 0x3C, 0xFFFC, sh::Next() & 0xFFFF); break;
    case k9CBeam:
        a[0] = s + 0x34;
        a[1] = s + 0xC;
        a[2] = (sh::Next() & 0xFFFF0000u) | PickOf(0, 0x40, 0x3C, 0xFFFC, sh::Next() & 0xFFFF);
        break;
    case k9EPlate:
    case k9ETexPlate:
    case k9EWall:
    case k9EShadePlate:
        a[0] = s + 0x34;
        a[1] = s + 0xC;
        break;
    case kA0Glow:
        a[0] = sh::Half() ? s + 0x64 : s + 0x34;
        a[1] = (sh::Next() & 0xFFFF0000u) | PickOf(0, 0x20, 0x100, 0x7FFF, 0x8000, 0xFFFA, sh::Next() & 0xFFFF);
        a[2] = (sh::Next() & 0xFFFFFF00u) | PickOf(6, 2, 0, 1, 3, 0xFF, sh::Next() & 0xFF);
        break;
    case kA0Trail:
        a[0] = s + 0x18;
        a[1] = s + 0x34;
        a[2] = (sh::Next() & 0xFFFFFF00u) | PickOf(0x40, 0x80, 0, 0xFF, sh::Next() & 0xFF);
        break;
    case kA0InitShard:
    case kA0Shard: a[0] = Key(ShardAt(sh::Next() % at::kShardCount)); break;
    case kA0Swap: {
        const U x = s + 0x18 + 4 * (sh::Next() % 3);
        a[0] = x + 0x1C;
        a[1] = sh::Next() % 8 == 0 ? a[0] : x;   // the same dword now and then: both 0
        break;
    }
    default: break;
    }
}

// What the functions read again after a call, moved by the group's case of the
// harness's disturbance (from its hash only): the record's counts and shades,
// the size word, the chapter's count. The trail's pull +0x64 is left alone
// (its dot loop's end depends on it, section 4).
void Disturb(U h) {
    const U v = h >> 8;
    unsigned char* const s = Sprite_Current;
    switch (h % 7) {
    case 0:
        if (sh::InRegions(s, 0x80)) s[9] = static_cast<unsigned char>((v & 1) ? 1u : v >> 1);
        break;
    case 1:
        if (sh::InRegions(s, 0x80)) s[6] = static_cast<unsigned char>(v);
        break;
    case 2:
        if (sh::InRegions(s, 0x80)) SetWord(s + 0x2E, (v & 1) ? 0x10u : v >> 1);
        break;
    case 3:
        if (sh::InRegions(s, 0x80)) s[0x5D] = static_cast<unsigned char>(v);
        break;
    case 4:
        if (sh::InRegions(s, 0x80)) s[0x5E] = static_cast<unsigned char>(v);
        break;
    case 5:
        if (sh::InRegions(s, 0x80)) s[3] = static_cast<unsigned char>(v);
        break;
    case 6: sh::Mem(at::kCounter)[0] = static_cast<unsigned char>((v & 1) ? 0x14u : (v & 2) ? 0x17u : v >> 2); break;
    default: break;
    }
}

}  // namespace

void SelfTest() {
    // BOF3X_E4E_ONLY: the clones whose name contains it (a control's run)
    static sh::Clone chosen[kCount];
    static unsigned index[kCount];
    const char* const only = std::getenv("BOF3X_E4E_ONLY");
    unsigned n = 0;
    for (unsigned k = 0; k < kCount; ++k)
        if (!only || !*only || std::strstr(kAll[k].name, only)) {
            index[n] = k;
            chosen[n++] = kAll[k];
        }
    if (n == 0) bof3::Fatal("effect_4e: BOF3X_E4E_ONLY=%s names no clone", only);
    static unsigned* s_index = index;
    sh::Group g = {"effect_4e", chosen, n, kCallees, sizeof kCallees / sizeof kCallees[0], kTables,
                   sizeof kTables / sizeof kTables[0], kRegions, sizeof kRegions / sizeof kRegions[0],
                   [](unsigned k) { Seed(s_index[k]); }, &Disturb, 4000};
    if (const char* r = std::getenv("BOF3X_E4E_ROUNDS")) g.rounds = static_cast<unsigned>(std::atoi(r));
    g.args = [](unsigned k, U* a) { Args(s_index[k], a); };
    g.effect = true;
    g.kinds = kKinds;
    g.n_kinds = sizeof kKinds;
    sh::Run(g);
}

}  // namespace effect_4e

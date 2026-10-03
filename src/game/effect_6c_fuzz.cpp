// BOF3X_SHADOW=effect_6c: group E6C's 50 functions through the scenario harness
// in effect mode (scenario_harness.h, docs/scenario_harness.md section 8), once
// at start-up. docs/effect_6c.md section 4. BOF3X_E6C_ONLY=<name> runs the
// clones whose name contains it (the controls' speed-up).
//
// The clone table is tools/band_rows.py --group E6C --clones --harness scenario
// (2026-10-03), each extent read again to its last instruction (capstone) and
// the names given; the cut's start 0x5124C0 is a case of 0x512490's switch (its
// jump table moved into that clone), and 0x512510, 0x512B20 and 0x513CD0 are
// added. Shapes: the states, the dispatchers, sub-kind 0x44's frame and sky
// and 0x59's move kEffect (Sprite_Current one of the 20 Effect_Objects records,
// +5 0x18, a dispatcher's +2 below its table's length); the helpers with
// arguments kCall; AreaMap_CornerHeight answers eax (ret_mask 0xFFFFFFFF).
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_6c.h"
#include "game/effect_6c_callees.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace effect_6c {
namespace {

namespace sh = scenario_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using U = std::uint32_t;

// tools/band_rows.py --group E6C --clones, 2026-10-03.
constexpr sh::CallSite kCalls510C90[] = {{0x14, 0x589330}, {0x33, 0x5893A0},  {0xF3, 0x5A8250},  {0x131, 0x5B9550},
                                         {0x146, 0x5B9550}, {0x173, 0x5B9550}, {0x1B4, 0x5A77C0}, {0x1BD, 0x461E50},
                                         {0x1C9, 0x5A7740}, {0x1D1, 0x5A7780}, {0x1F8, 0x5A9110}, {0x201, 0x461E50}};
constexpr sh::CallSite kCalls510EB0[] = {
    {0x3B, 0x5A77C0},  {0x44, 0x461E50},  {0x50, 0x5A75D0},  {0x58, 0x5A77A0},  {0xB0, 0x461E50},  {0xE9, 0x5A77C0},
    {0xF5, 0x461E50},  {0x109, 0x5A77C0}, {0x110, 0x511BB0}, {0x11C, 0x5A7740}, {0x124, 0x5A7780}, {0x14B, 0x511BB0},
    {0x2F0, 0x5718F0}, {0x32E, 0x5A75D0}, {0x335, 0x5A77A0}, {0x33D, 0x5A7780}, {0x422, 0x511BB0}, {0x463, 0x511740},
    {0x48D, 0x5A77C0}, {0x494, 0x511BB0}, {0x4B2, 0x5A7610}, {0x4BD, 0x5A7780}, {0x573, 0x511BB0}, {0x624, 0x5115A0},
    {0x633, 0x57C140}, {0x6B3, 0x5A7A70}, {0x6DC, 0x5115A0}};
constexpr sh::CallSite kCalls5115A0[] = {{0xC, 0x5A75D0},  {0x14, 0x5A77A0}, {0x1C, 0x5A7780}, {0x9D, 0x511BB0},
                                         {0xB9, 0x5A75D0}, {0xC1, 0x5A77A0}, {0xC9, 0x5A7780}, {0x16F, 0x511BB0}};
constexpr sh::CallSite kCalls511740[] = {
    {0x18, 0x5A77C0},  {0x1F, 0x511BB0},  {0x2B, 0x5A7750},  {0x33, 0x5A7780},  {0x7C, 0x5B93D2},  {0x93, 0x5B93D2},
    {0xAC, 0x511BB0},  {0xB2, 0x511D50},  {0x104, 0x5A7750}, {0x10C, 0x5A7780}, {0x138, 0x5B93D2}, {0x14F, 0x5B93D2},
    {0x168, 0x511BB0}, {0x16E, 0x511D50}, {0x216, 0x5A7750}, {0x21E, 0x5A7780}, {0x264, 0x5B93D2}, {0x282, 0x5B93D2},
    {0x2A0, 0x5B93D2}, {0x2C0, 0x511BB0}, {0x2C6, 0x511D50}, {0x3AE, 0x5A7750}, {0x3B6, 0x5A7780}, {0x3E8, 0x5A7A70},
    {0x414, 0x5B93D2}, {0x42B, 0x5B93D2}, {0x447, 0x511BB0}};
constexpr sh::CallSite kCalls511BB0[] = {{0x34, 0x5A7560}};
constexpr sh::CallSite kCalls511D50[] = {{0x13, 0x5A7750}, {0x1B, 0x5A7780}, {0x60, 0x511BB0}};
constexpr sh::CallSite kCalls511E20[] = {{0x38, 0x512040}};
constexpr sh::CallSite kCalls511E80[] = {{0x10E, 0x512040}, {0x11F, 0x512040}, {0x130, 0x512040}, {0x141, 0x512040},
                                         {0x14E, 0x512040}, {0x157, 0x512040}, {0x163, 0x512040}, {0x16C, 0x512040},
                                         {0x193, 0x572650}, {0x1B5, 0x589840}};
constexpr sh::CallSite kCalls512040[] = {
    {0x32, 0x5A75D0},  {0x3A, 0x5A77A0},  {0x51, 0x5B9550},  {0x6C, 0x5A7A00},  {0x87, 0x5B9550},  {0x94, 0x5A7A50},
    {0xB2, 0x5B9550},  {0xE7, 0x5B9550},  {0x102, 0x5A7A00}, {0x11B, 0x5B9550}, {0x128, 0x5A7A50}, {0x144, 0x5B9550},
    {0x198, 0x5A85F0}, {0x19E, 0x5A9290}, {0x1A6, 0x5A7780}, {0x1E5, 0x5A7A00}, {0x201, 0x5A7A50}, {0x232, 0x5A7A00},
    {0x253, 0x5A7A50}, {0x26F, 0x5A7A00}, {0x289, 0x5A7A50}, {0x2A3, 0x5A7A00}, {0x2BB, 0x5A7A50}, {0x2E3, 0x572FA0}};
constexpr sh::CallSite kCalls512370[] = {{0xA5, 0x572ED0}, {0xC2, 0x572A00}};
constexpr sh::CallSite kCalls512490[] = {{0x37, 0x57C140}, {0x53, 0x57C140}};
constexpr sh::JumpTable kTables512490[] = {{0x12, 0x68, 5}};   // the switch's five cases, 0x5124C0 the fourth
constexpr sh::CallSite kCalls512510[] = {{0xD6, 0x572ED0}, {0xFD, 0x572A00}, {0x144, 0x589840}};
constexpr sh::CallSite kCalls512660[] = {{0x18, 0x5720C0},  {0x85, 0x5A77C0},  {0x9B, 0x572FA0},  {0xF8, 0x5A7750},
                                         {0x100, 0x5A7780}, {0x124, 0x5A7A00}, {0x15F, 0x5A7A50}, {0x1BB, 0x5A8250},
                                         {0x1C4, 0x5A9110}, {0x1DA, 0x572FA0}, {0x225, 0x5A77C0}, {0x23B, 0x572FA0}};
constexpr sh::CallSite kCalls5128D0[] = {{0x7, 0x57C140}, {0x2B, 0x512960}};
constexpr sh::CallSite kCalls512910[] = {{0x18, 0x589840}, {0x3B, 0x512960}};
constexpr sh::CallSite kCalls512960[] = {{0x70, 0x5A8250},  {0xA5, 0x5A75D0},  {0xAD, 0x5A77A0},
                                         {0xB3, 0x5A92E0},  {0x12E, 0x5A7780}, {0x194, 0x572FA0}};
constexpr sh::CallSite kCalls512B40[] = {{0x1C, 0x512D30}, {0x24, 0x513410}};
constexpr sh::CallSite kCalls512B70[] = {{0x13, 0x512D30}};
constexpr sh::CallSite kCalls512B90[] = {{0x36, 0x512D30}, {0x54, 0x513410}};
constexpr sh::CallSite kCalls512BF0[] = {{0x1C, 0x512D30}, {0x3B, 0x513410}};
constexpr sh::CallSite kCalls512C30[] = {{0x78, 0x512D30}, {0x97, 0x513410}};
constexpr sh::CallSite kCalls512CF0[] = {{0x1A, 0x589840}, {0x2B, 0x512D30}};
constexpr sh::CallSite kCalls512D30[] = {
    {0x89, 0x5A7630},  {0x91, 0x5A7780},  {0x97, 0x5A7A00},  {0xB8, 0x5B9550},  {0xC4, 0x5A7A50},  {0xE5, 0x5B9550},
    {0x109, 0x5A7A00}, {0x12A, 0x5B9550}, {0x136, 0x5A7A50}, {0x157, 0x5B9550}, {0x17B, 0x5A7A00}, {0x19C, 0x5B9550},
    {0x1A8, 0x5A7A50}, {0x1C9, 0x5B9550}, {0x1ED, 0x5A7A00}, {0x20E, 0x5B9550}, {0x21A, 0x5A7A50}, {0x23B, 0x5B9550},
    {0x294, 0x5A85F0}, {0x29D, 0x5A93A0}, {0x354, 0x572FA0}, {0x360, 0x5A7630}, {0x368, 0x5A77A0}, {0x370, 0x5A7780},
    {0x47D, 0x572FA0}, {0x4D5, 0x5A7A00}, {0x4FA, 0x5B9550}, {0x506, 0x5A7A50}, {0x52B, 0x5B9550}, {0x564, 0x5A8250},
    {0x5B1, 0x5A75D0}, {0x5B9, 0x5A77A0}, {0x5BF, 0x5A92E0}, {0x63A, 0x5A7780}, {0x698, 0x572FA0}};
constexpr sh::CallSite kCalls513410[] = {{0x49, 0x5A7810}, {0x52, 0x461E50}};
constexpr sh::CallSite kCalls513470[] = {
    {0x52, 0x5A7A50},  {0xCC, 0x5A77C0},  {0xD5, 0x461E50},  {0xE1, 0x5A75D0},  {0xE9, 0x5A77A0},  {0xF1, 0x5A7780},
    {0x199, 0x461E50}, {0x1A5, 0x5A75D0}, {0x1AD, 0x5A77A0}, {0x1B8, 0x5A7780}, {0x233, 0x461E50}, {0x23F, 0x5A75D0},
    {0x247, 0x5A77A0}, {0x24F, 0x5A7780}, {0x2CA, 0x461E50}, {0x2D6, 0x5A75D0}, {0x2DE, 0x5A77A0}, {0x2E6, 0x5A7780},
    {0x357, 0x461E50}, {0x396, 0x5A77C0}, {0x39F, 0x461E50}, {0x3D9, 0x589840}};
constexpr sh::CallSite kCalls513880[] = {{0x13, 0x57C140}};
constexpr sh::CallSite kCalls5138E0[] = {{0x18, 0x513AD0}};
constexpr sh::CallSite kCalls513920[] = {{0xD, 0x57C0F0}, {0x22, 0x513C70}, {0x8D, 0x572ED0}, {0xAA, 0x572A00}};
constexpr sh::CallSite kCalls5139F0[] = {{0x5E, 0x513C70}};
constexpr sh::CallSite kCalls513A60[] = {{0x5A, 0x513C70}};
constexpr sh::CallSite kCalls513AD0[] = {{0xB2, 0x572ED0}, {0xE2, 0x572A00}, {0x15C, 0x572ED0}, {0x179, 0x572A00}};
constexpr sh::CallSite kCalls513C70[] = {{0x41, 0x5A7810}, {0x4A, 0x461E50}};
constexpr sh::CallSite kCalls513CF0[] = {{0xA, 0x57C140}};
constexpr sh::CallSite kCalls513D20[] = {{0x1F, 0x5140C0}, {0x26, 0x5140C0}, {0x2D, 0x5140C0}, {0x37, 0x587740}};
constexpr sh::CallSite kCalls513D70[] = {{0x2B, 0x5140C0}, {0x4D, 0x5140C0}, {0x5E, 0x5140C0}};
constexpr sh::CallSite kCalls513E00[] = {{0x16, 0x5140C0}, {0x27, 0x5140C0}, {0x38, 0x5140C0}, {0x5D, 0x587740}};
constexpr sh::CallSite kCalls513E80[] = {{0x2B, 0x5140C0}, {0x4D, 0x5140C0}, {0x5E, 0x5140C0}};
constexpr sh::CallSite kCalls513F10[] = {{0x17, 0x5140C0}, {0x28, 0x5140C0}, {0x55, 0x5140C0},
                                         {0x80, 0x57C0F0}, {0x89, 0x572650}, {0x91, 0x589840}};
constexpr sh::CallSite kCalls513FB0[] = {{0x13, 0x57C110}, {0x1D, 0x587740}, {0x38, 0x5140C0}, {0x49, 0x5140C0}, {0x64, 0x5140C0}};
constexpr sh::CallSite kCalls514030[] = {{0x2B, 0x5140C0}, {0x4D, 0x5140C0}, {0x68, 0x5140C0}};
constexpr sh::CallSite kCalls5140C0[] = {{0x45, 0x5A75D0},  {0x4D, 0x5A77A0},  {0xB0, 0x5B9550},  {0xCF, 0x5B9550},
                                         {0xF4, 0x5B9550},  {0x139, 0x5A85F0}, {0x13F, 0x5A9290}, {0x14B, 0x572A00},
                                         {0x16D, 0x5B9550}, {0x180, 0x572FA0}};

#define E6C_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define E6C_CALLS(a) a, E6C_N(a)
#define E6C_FN(name) reinterpret_cast<const void*>(&::name)
constexpr sh::Shape kEf = sh::Shape::kEffect;
constexpr sh::Shape kCa = sh::Shape::kCall;
constexpr U kScratch0 = sh::ArgAt(0, sh::Arg::kScratch);
// {name, base, size, calls, imms, tables, ours, ret_mask, calm, shape, pointers, state_span, sub_span, kind}
const sh::Clone kAll[] = {
    {"EffectKind18Sub44_Follow", 0x510C90, 0x215, E6C_CALLS(kCalls510C90), nullptr, 0, nullptr, 0, E6C_FN(EffectKind18Sub44_Follow), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub44_Draw", 0x510EB0, 0x6EB, E6C_CALLS(kCalls510EB0), nullptr, 0, nullptr, 0, E6C_FN(EffectKind18Sub44_Draw), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub44_DrawGlow", 0x5115A0, 0x195, E6C_CALLS(kCalls5115A0), nullptr, 0, nullptr, 0, E6C_FN(EffectKind18Sub44_DrawGlow), 0, false, kCa, 0, 0, 0, 0x18},
    {"EffectKind18Sub44_DrawStars", 0x511740, 0x46E, E6C_CALLS(kCalls511740), nullptr, 0, nullptr, 0, E6C_FN(EffectKind18Sub44_DrawStars), 0, false, kCa, 0, 0, 0, 0x18},
    {"EffectKind18Sub44_Commit", 0x511BB0, 0x5A, E6C_CALLS(kCalls511BB0), nullptr, 0, nullptr, 0, E6C_FN(EffectKind18Sub44_Commit), 0, false, kCa, 0, 0, 0, 0x18},
    {"AreaMap_CornerHeight", 0x511C10, 0x139, nullptr, 0, nullptr, 0, nullptr, 0, E6C_FN(AreaMap_CornerHeight), 0xFFFFFFFFu, false, kCa, 0, 0, 0, 0x18},
    {"EffectKind18Sub44_DrawTwinkle", 0x511D50, 0x7A, E6C_CALLS(kCalls511D50), nullptr, 0, nullptr, 0, E6C_FN(EffectKind18Sub44_DrawTwinkle), 0, false, kCa, kScratch0, 0, 0, 0x18},
    {"EffectKind18Sub45_Run", 0x511DD0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E6C_FN(EffectKind18Sub45_Run), 0, false, kEf, 0, 0, 3, 0x18},
    {"EffectKind18Sub45_Wait", 0x511DF0, 0x26, nullptr, 0, nullptr, 0, nullptr, 0, E6C_FN(EffectKind18Sub45_Wait), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub45_Open", 0x511E20, 0x5E, E6C_CALLS(kCalls511E20), nullptr, 0, nullptr, 0, E6C_FN(EffectKind18Sub45_Open), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub45_Spread", 0x511E80, 0x1BE, E6C_CALLS(kCalls511E80), nullptr, 0, nullptr, 0, E6C_FN(EffectKind18Sub45_Spread), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub45_DrawRing", 0x512040, 0x303, E6C_CALLS(kCalls512040), nullptr, 0, nullptr, 0, E6C_FN(EffectKind18Sub45_DrawRing), 0, false, kCa, 0, 0, 0, 0x18},
    {"EffectKind18Sub51_Run", 0x512350, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E6C_FN(EffectKind18Sub51_Run), 0, false, kEf, 0, 0, 3, 0x18},
    {"EffectKind18Sub51_Place", 0x512370, 0x115, E6C_CALLS(kCalls512370), nullptr, 0, nullptr, 0, E6C_FN(EffectKind18Sub51_Place), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub51_Wait", 0x512490, 0x7C, E6C_CALLS(kCalls512490), nullptr, 0, E6C_CALLS(kTables512490), E6C_FN(EffectKind18Sub51_Wait), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub51_Fade", 0x512510, 0x14D, E6C_CALLS(kCalls512510), nullptr, 0, nullptr, 0, E6C_FN(EffectKind18Sub51_Fade), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub53_Run", 0x512660, 0x24B, E6C_CALLS(kCalls512660), nullptr, 0, nullptr, 0, E6C_FN(EffectKind18Sub53_Run), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub55_Run", 0x5128B0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E6C_FN(EffectKind18Sub55_Run), 0, false, kEf, 0, 0, 3, 0x18},
    {"EffectKind18Sub55_Wait", 0x5128D0, 0x34, E6C_CALLS(kCalls5128D0), nullptr, 0, nullptr, 0, E6C_FN(EffectKind18Sub55_Wait), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub55_Fade", 0x512910, 0x44, E6C_CALLS(kCalls512910), nullptr, 0, nullptr, 0, E6C_FN(EffectKind18Sub55_Fade), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub55_Draw", 0x512960, 0x1C0, E6C_CALLS(kCalls512960), nullptr, 0, nullptr, 0, E6C_FN(EffectKind18Sub55_Draw), 0, false, kCa, 0, 0, 0, 0x18},
    {"EffectKind18Sub59_Run", 0x512B20, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E6C_FN(EffectKind18Sub59_Run), 0, false, kEf, 0, 0, 6, 0x18},
    {"EffectKind18Sub59_Start", 0x512B40, 0x29, E6C_CALLS(kCalls512B40), nullptr, 0, nullptr, 0, E6C_FN(EffectKind18Sub59_Start), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub59_Wait", 0x512B70, 0x1A, E6C_CALLS(kCalls512B70), nullptr, 0, nullptr, 0, E6C_FN(EffectKind18Sub59_Wait), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub59_Rise", 0x512B90, 0x59, E6C_CALLS(kCalls512B90), nullptr, 0, nullptr, 0, E6C_FN(EffectKind18Sub59_Rise), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub59_Hold", 0x512BF0, 0x40, E6C_CALLS(kCalls512BF0), nullptr, 0, nullptr, 0, E6C_FN(EffectKind18Sub59_Hold), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub59_Fade", 0x512C30, 0xB8, E6C_CALLS(kCalls512C30), nullptr, 0, nullptr, 0, E6C_FN(EffectKind18Sub59_Fade), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub59_Close", 0x512CF0, 0x32, E6C_CALLS(kCalls512CF0), nullptr, 0, nullptr, 0, E6C_FN(EffectKind18Sub59_Close), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub59_DrawRing", 0x512D30, 0x6DB, E6C_CALLS(kCalls512D30), nullptr, 0, nullptr, 0, E6C_FN(EffectKind18Sub59_DrawRing), 0, false, kCa, 0, 0, 0, 0x18},
    {"EffectKind18Sub59_Move", 0x513410, 0x5B, E6C_CALLS(kCalls513410), nullptr, 0, nullptr, 0, E6C_FN(EffectKind18Sub59_Move), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub66_Run", 0x513470, 0x3E2, E6C_CALLS(kCalls513470), nullptr, 0, nullptr, 0, E6C_FN(EffectKind18Sub66_Run), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub5A_Run", 0x513860, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E6C_FN(EffectKind18Sub5A_Run), 0, false, kEf, 0, 0, 6, 0x18},
    {"EffectKind18Sub5A_Start", 0x513880, 0x33, E6C_CALLS(kCalls513880), nullptr, 0, nullptr, 0, E6C_FN(EffectKind18Sub5A_Start), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub5A_Wait", 0x5138C0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E6C_FN(EffectKind18Sub5A_Wait), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub5A_Animate", 0x5138E0, 0x3C, E6C_CALLS(kCalls5138E0), nullptr, 0, nullptr, 0, E6C_FN(EffectKind18Sub5A_Animate), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub5A_Set", 0x513920, 0xC8, E6C_CALLS(kCalls513920), nullptr, 0, nullptr, 0, E6C_FN(EffectKind18Sub5A_Set), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub5A_Cycle", 0x5139F0, 0x6A, E6C_CALLS(kCalls5139F0), nullptr, 0, nullptr, 0, E6C_FN(EffectKind18Sub5A_Cycle), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub5A_CycleOpen", 0x513A60, 0x66, E6C_CALLS(kCalls513A60), nullptr, 0, nullptr, 0, E6C_FN(EffectKind18Sub5A_CycleOpen), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub5A_SetTiles", 0x513AD0, 0x193, E6C_CALLS(kCalls513AD0), nullptr, 0, nullptr, 0, E6C_FN(EffectKind18Sub5A_SetTiles), 0, false, kCa, 0, 0, 0, 0x18},
    {"EffectKind18Sub5A_Move", 0x513C70, 0x53, E6C_CALLS(kCalls513C70), nullptr, 0, nullptr, 0, E6C_FN(EffectKind18Sub5A_Move), 0, false, kCa, 0, 0, 0, 0x18},
    {"EffectKind18Sub5B_Run", 0x513CD0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E6C_FN(EffectKind18Sub5B_Run), 0, false, kEf, 0, 0, 8, 0x18},
    {"EffectKind18Sub5B_Start", 0x513CF0, 0x30, E6C_CALLS(kCalls513CF0), nullptr, 0, nullptr, 0, E6C_FN(EffectKind18Sub5B_Start), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub5B_Wait", 0x513D20, 0x48, E6C_CALLS(kCalls513D20), nullptr, 0, nullptr, 0, E6C_FN(EffectKind18Sub5B_Wait), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub5B_SlideOut", 0x513D70, 0x8C, E6C_CALLS(kCalls513D70), nullptr, 0, nullptr, 0, E6C_FN(EffectKind18Sub5B_SlideOut), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub5B_Hold", 0x513E00, 0x78, E6C_CALLS(kCalls513E00), nullptr, 0, nullptr, 0, E6C_FN(EffectKind18Sub5B_Hold), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub5B_SlideWide", 0x513E80, 0x8C, E6C_CALLS(kCalls513E80), nullptr, 0, nullptr, 0, E6C_FN(EffectKind18Sub5B_SlideWide), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub5B_Lift", 0x513F10, 0x98, E6C_CALLS(kCalls513F10), nullptr, 0, nullptr, 0, E6C_FN(EffectKind18Sub5B_Lift), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub5B_Reopen", 0x513FB0, 0x7E, E6C_CALLS(kCalls513FB0), nullptr, 0, nullptr, 0, E6C_FN(EffectKind18Sub5B_Reopen), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub5B_Close", 0x514030, 0x84, E6C_CALLS(kCalls514030), nullptr, 0, nullptr, 0, E6C_FN(EffectKind18Sub5B_Close), 0, false, kEf, 0, 0, 0, 0x18},
    {"EffectKind18Sub5B_DrawPart", 0x5140C0, 0x1AC, E6C_CALLS(kCalls5140C0), nullptr, 0, nullptr, 0, E6C_FN(EffectKind18Sub5B_DrawPart), 0, false, kCa, 0, 0, 0, 0x18},
};
#undef E6C_FN
#undef E6C_CALLS
#undef E6C_N

enum : unsigned {
    k44Follow, k44Draw, k44Glow, k44Stars, k44Commit, kCornerHeight, k44Twinkle,
    k45Run, k45Wait, k45Open, k45Spread, k45Ring,
    k51Run, k51Place, k51Wait, k51Fade,
    k53Run,
    k55Run, k55Wait, k55Fade, k55Draw,
    k59Run, k59Start, k59Wait, k59Rise, k59Hold, k59Fade, k59Close, k59Ring, k59Move,
    k66Run,
    k5ARun, k5AStart, k5AWait, k5AAnimate, k5ASet, k5ACycle, k5ACycleOpen, k5ASetTiles, k5AMove,
    k5BRun, k5BStart, k5BWait, k5BSlideOut, k5BHold, k5BSlideWide, k5BLift, k5BReopen, k5BClose, k5BDrawPart,
    kCount
};
static_assert(kCount == sizeof kAll / sizeof kAll[0], "one enum entry a clone, in order");

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
unsigned char* Mem(U a) { return sh::Mem(a); }
template <typename... T> U PickOf(T... v) {
    const U values[] = {static_cast<U>(v)...};
    return sh::Pick(values, sizeof...(v));
}

// --- the effects -----------------------------------------------------------------------

constexpr unsigned kPacketsSize = 0x800;   // the harness's packet buffer (scenario_harness.cpp g_packets)

// The group's helpers read Sprite_Current's record (its point, +8..+0xB): its
// address and those words logged, so a call on the wrong record, or before a
// state's write it should follow, shows.
U FxCurrent(const U*, U answer) {
    unsigned char* const s = Sprite_Current;
    const bool in = sh::InRegions(s, 0x80);
    sh::Note(Key(s), in ? static_cast<U>(Long(s + 8)) : 0u, in ? static_cast<U>(Long(s + 0x34)) : 0u,
             in ? static_cast<U>(Long(s + 0x38)) : 0u);
    return answer;
}
// Sub-kind 0x5B's parts read the two offsets the states set before each call.
U FxOffsets(const U* a, U answer) {
    FxCurrent(a, answer);
    sh::Note(static_cast<U>(Long(Mem(at::kOffsetX))), static_cast<U>(Long(Mem(at::kOffsetY))));
    return answer;
}
// Sub-kind 0x44's glows and stars read the two colour rows the sky wrote.
U FxRows(const U* a, U answer) {
    FxCurrent(a, answer);
    sh::NoteBytes(Mem(sh::at::kVertexScratch), 0x10);
    return answer;
}
// EffectKind18Sub44_Commit: the cursor += size & 0xFF while 0x160 bytes stay
// after it in the packet buffer (the sky lays four bands 0x44 apart from the
// cursor without reading it again).
U FxCommit44(const U* a, U answer) {
    unsigned char* const next = Gfx_PacketNext;
    unsigned char* const base = sh::Packets();
    const unsigned size = a[0] & 0xFF;
    if (next >= base && next + size + 0x160 <= base + kPacketsSize) Gfx_PacketNext = next + size;
    return answer;
}
// MapView_LinkPrimAt, as effect mode's row has it (the cursor + size & 0xFF two
// times in three, inside the packet buffer), its dy compared on the low byte
// only: the real one reads a signed byte, and EffectKind18Sub59_DrawRing pushes
// a byte over whatever its registers held.
U FxLink(const U* a, U answer) {
    if (sh::Noise() % 3 != 0) {
        unsigned char* const next = Gfx_PacketNext;
        const unsigned n = a[3] & 0xFF;
        if (next >= sh::Packets() && next + n + 0x40 <= sh::Packets() + kPacketsSize) Gfx_PacketNext = next + n;
    }
    return answer;
}

#define E6C_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
constexpr sh::Answer kG = sh::Answer::kGarbage, kPh = sh::Answer::kPhase;
constexpr U kW = 0xFFFFFFFFu, k8 = 0xFFu;
const sh::Callee kCallees[] = {
    // the group's own, called by name (directly or by a tail jump): each logs
    // what it is handed and the record it runs on; the arguments are whole
    // words the callers compute or push as immediates (the commit's size is
    // read as a byte)
    {E6C_OURS(EffectKind18Sub44_Commit), 1, {k8}, kG, 0, 0, {}, &FxCommit44},
    {E6C_OURS(EffectKind18Sub44_DrawStars), 1, {kW}, kG, 0, 0, {}, &FxRows},
    {E6C_OURS(EffectKind18Sub44_DrawGlow), 2, {kW, kW}, kG, 0, 0, {}, &FxRows},
    {E6C_OURS(EffectKind18Sub44_DrawTwinkle), 1, {kW}, kG, 0, 0, {16}, &FxCurrent, nullptr, true},
    {E6C_OURS(EffectKind18Sub45_DrawRing), 3, {kW, kW, kW}, kG, 0, 0, {}, &FxCurrent},
    {E6C_OURS(EffectKind18Sub55_Draw), 2, {kW, kW}, kG, 0, 0, {}, &FxCurrent},
    {E6C_OURS(EffectKind18Sub59_DrawRing), 1, {kW}, kG, 0, 0, {}, &FxCurrent},
    {E6C_OURS(EffectKind18Sub59_Move), 0, {}, kPh, 0, 0, {}, &FxCurrent},
    {E6C_OURS(EffectKind18Sub5A_SetTiles), 2, {kW, kW}, kG, 0, 0, {}, &FxCurrent},
    {E6C_OURS(EffectKind18Sub5A_Move), 2, {kW, kW}, kG, 0, 0, {}, &FxCurrent},
    {E6C_OURS(EffectKind18Sub5B_DrawPart), 1, {kW}, kG, 0, 0, {}, &FxOffsets},
    // a standard row re-listed: the link's dy compared on its byte
    {E6C_OURS(MapView_LinkPrimAt), 4, {kW, kW, k8, k8}, kG, 0, 0, {}, &FxLink, nullptr, true},
};
#undef E6C_OURS

// The sub-state tables the dispatchers jump through, read in place; each
// table's own length (symbols.toml [[data]], none bounded by a compare).
const sh::DataTable kTables[] = {
    {0x65EECC, 3}, {0x65EEF0, 3}, {0x65EF00, 3}, {0x65EF18, 6}, {0x65EF58, 6}, {0x65EF78, 8},
};
// Beyond effect mode's standard regions: Sprite_DrawList (sub-kind 0x44 adds its
// record), the list's two last pointers 0x802B34 (EffectKind18Sub44_Commit),
// 64 draw items (MapView_ItemAt's stand-in answers 1..0x3F), and sub-kind 0x59's
// twelve shade bytes in .data (its fade writes them).
const sh::Region kRegions[] = {
    {at::kDrawList, 4 * at::kDrawListRoom},
    {at::kLayerTails, 0x10},
    {0x905E80, 0x40 * at::kDrawItemStride},
    {at::kShades59, at::kShades59Size},
};
const std::uint8_t kKinds[] = {0x18};

// --- the seed ------------------------------------------------------------------------

unsigned char* Rec(unsigned r) { return sh::EffectRecord(r); }
// EffectKind18Sub44_Commit's size, drawn by the seed (which may put the cursor
// at the pool's room test for it) and handed by Args.
U g_size = 0;

// The area map as the cell-record functions index it, kept inside the area
// block's 8 KiB: a width and height 1..16, the cells' base 0..0x3F, every cell
// word of the plane they read (bytes 0x30..0x1000) 0..0x3F.
void SeedAreaMap() {
    unsigned char* const header = Mem(at::kAreaHeader);
    header[0] = static_cast<unsigned char>(1 + sh::Next() % 16);
    header[1] = static_cast<unsigned char>(1 + sh::Next() % 16);
    SetWord(header + 2, sh::Next() % 0x40);
    for (U offset = 0x30; offset < 0x1000; offset += 4) {
        const U n = sh::Next();
        SetWord(header + offset, n & 0x3F);
        SetWord(header + offset + 2, (n >> 16) & 0x3F);
    }
}
// The 64 draw items' link words (& 0xFFF) inside the pool's first 1,024 (the
// region holds 64; Prim_SetTexture's stand-in hashes a readable item past it).
void SeedItems() {
    for (U i = 0; i < 0x40; ++i)
        SetWord(Mem(0x905E80 + i * at::kDrawItemStride + at::kItemLink), (sh::Next() % 0x400) | (sh::Next() & 0xF000));
}
// AreaMap_CornerHeight's point: a cell whose block keeps the corner dword
// inside the area block (block & 0xF0 is 0, or 0x10 with the cell's z & 0xF at
// most 4), read from the image's block map; a fraction at the triangles' edge.
U CornerAxis(U cell) {
    const U frac = PickOf(0, 1, 0x7FFF, 0x8000, 0x8001, 0xFFFF, sh::Next() & 0xFFFF);
    return (sh::Next() & 0xFF000000u) + (cell << 16) + frac - 0x8000u;
}
void CornerPoint(U* a) {
    for (unsigned tries = 0; tries < 64; ++tries) {
        const U xi = sh::Next() & 0xFF, zi = sh::Next() & 0xFF;
        const U block = Mem(at::kBlockMap)[((xi >> 4) & 0xF) | (zi & 0xF0)];
        if ((block & 0xF0) == 0 || ((block & 0xF0) == 0x10 && (zi & 0xF) <= 4)) {
            a[0] = CornerAxis(xi);
            a[1] = CornerAxis(zi);
            return;
        }
    }
    a[0] = CornerAxis(0);
    a[1] = CornerAxis(0);
}

void Seed(unsigned k) {
    // every record the disturbance may move Sprite_Current to: the variant
    // +0x36 of sub-kind 0x51 inside its five, the counters about their bounds
    for (unsigned r = 0; r < 20; ++r) {
        unsigned char* const e = Rec(r);
        SetWord(e + 0x36, PickOf(0, 1, 2, 3, 4));
        e[9] = static_cast<unsigned char>(PickOf(0, 1, 3, 4, 5, 8, 9, 0xF, 0x10, 0x11, 0x12, 0x17, 0x18, 0x1E, 0x1F, 0x20,
                                                 0x21, 0x30, 0x31, 0x40, 0x5F, 0x60, 0x61, 0x7F, 0x80, 0x81, 0xBF, 0xC0,
                                                 0xCE, 0xCF, 0xD0, 0xFC, 0xFF, sh::Next()));
        // sub-kind 0x45's radius +0x3C about its ends (0x100 for the outer,
        // 0x11F for the inner and the last ring)
        SetLong(e + 0x3C, static_cast<std::int32_t>(PickOf(0, 0xF0, 0xF8, 0xF7, 0x110, 0x117, 0x118, 0x1F7, 0x1F8, 0x1FF, sh::Next() % 0x220, sh::Next())));
    }
    unsigned char* const s = Sprite_Current;
    Cond_ByteFE = static_cast<unsigned char>(PickOf(0, 1, 2, 3, sh::Next()));
    Gfx_BufferIndex = static_cast<unsigned char>(sh::Next() % 2);
    for (unsigned b = 0; b < 2; ++b)
        SetLong(Mem(at::kLayerTails + 8 * b), static_cast<std::int32_t>(sh::Half() ? Key(sh::Scratch(b) + (sh::Next() & 0x1C)) : sh::Next()));
    // the sky's frame word inside its wrap (Area189_StepArrive: 0..0x3BF), at
    // its four bands' edges
    SetWord(Mem(at::kWalkClock), PickOf(0, 0x1AF, 0x1B0, 0x1B3, 0x1DF, 0x1E0, 0x38F, 0x390, 0x3A8, 0x3BF, sh::Next() % 0x3C0));
    // the camera's cell about the glows' points (inside and past their reach),
    // the stars' clamps, sub-kind 0x53's record (within and past 20), or anywhere
    {
        U x = Word(Mem(at::kKind2XHigh)), z = Word(Mem(at::kKind2ZHigh));
        switch (sh::Next() % 7) {
        case 0: x = 0x1780 + sh::Next() % 0x301 - 0x180; z = 0x1380 + sh::Next() % 0x301 - 0x180; break;
        case 1: x = 0x1780 + sh::Next() % 0x401 - 0x200; z = 0xE00 + sh::Next() % 0x401 - 0x200; break;
        case 2: x = 0x1480 + sh::Next() % 0x1201 - 0x900; z = 0x1380 + sh::Next() % 0x801 - 0x400; break;
        case 3: x = Word(s + 0x36) + sh::Next() % 0x31 - 0x18; z = Word(s + 0x3A) + sh::Next() % 0x31 - 0x18; break;
        case 4: x = PickOf(0xAFF, 0xB00, 0x1E00, 0x1E01, 0x1400); z = PickOf(0xEFF, 0xF00, 0x1800, 0x1801, 0x1400); break;
        case 5: z = 85 * PickOf(0, 1, 90, 91, 92) + sh::Next() % 85; break;   // the second star's y at 0 / 90
        default: break;
        }
        SetWord(Mem(at::kKind2XHigh), x);
        SetWord(Mem(at::kKind2ZHigh), z);
    }
    // never the one distance the glow divides by zero at (ours aborts there)
    if (static_cast<std::int16_t>(Camera_Distance) == -0x1194) Camera_Distance = -0x1193;
    if (sh::Half()) Camera_Distance = static_cast<short>(PickOf(0, 0xEE6D, 0xEE6B, 0xEC00, 0x1000));
    Sprite_DrawListCount = static_cast<unsigned char>(PickOf(0, 0x27, 0x28, 0x29, sh::Next()));
    switch (k) {
    case k51Place:
    case k51Fade:
    case k5ASet:
    case k5AAnimate:
    case k5ASetTiles:
        SeedAreaMap();
        SeedItems();
        break;
    case k59Fade:
    case k59Ring:
    case k59Start: case k59Wait: case k59Rise: case k59Hold: case k59Close: {
        // the shade bytes at the cascade's steps: 0, 2, 0x60, 0x62 and others
        unsigned char* const shades = Mem(at::kShades59);
        const unsigned first = sh::Next() % 10;
        for (unsigned i = 0; i < at::kShades59Size; ++i)
            shades[i] = static_cast<unsigned char>(i < first ? 0 : PickOf(0, 2, 0x60, 0x61, 0x62, 0x64, 0x80, sh::Next()));
        break;
    }
    case k66Run:
        if (sh::Half()) Cond_ByteFE = 0;
        break;
    case k53Run:
        if (sh::Half()) s[2] = 0;   // the ground read once
        break;
    case k44Follow:
        // the leader in state 3 (Sprite_ScriptTick), moving or not, the
        // record on its animation or not
        if (sh::Half()) Mem(at::kLeaderState)[0] = 3;
        if (sh::Half()) SetLong(Mem(at::kLeaderMoving), 0);
        if (sh::Half()) s[0x4B] = Mem(at::kLeaderAnimation)[0];
        break;
    case k44Commit:
        g_size = PickOf(0xC, 0x14, 0x1C, 0x44, 0x48, 0, 0xFF, sh::Next());
        if (sh::Half()) {
            // the cursor at the pool's room test: the real pool's address, the
            // limit within a byte or two of cursor + size (nothing is written
            // at the cursor)
            const U limit = (static_cast<U>(Gfx_BufferIndex) << 16) + at::kPoolLimit;
            Gfx_PacketNext = Mem(limit - (g_size & 0xFF) + PickOf(0, 1, 0xFFFFFFFFu, 0x40, 0xFFFFFFC0u));
        }
        break;
    default: break;
    }
}

// The helpers' arguments at their callers' values and boundaries.
void Args(unsigned k, U* a) {
    switch (k) {
    case k44Glow:
        a[0] = PickOf(0, 1, 0x40, 0x7F, 0x80, sh::Next() & 0xFF);
        a[1] = PickOf(0xFFFFFFD8u, 0, 0x100, sh::Next() % 0x300 - 0x150, sh::Next());
        break;
    case k44Stars: a[0] = PickOf(0x80, 0x40, 1, 0x7F, sh::Next() & 0xFFFF, sh::Next()); break;
    case k44Commit: a[0] = g_size; break;
    case kCornerHeight: CornerPoint(a); break;
    case k45Ring:
        a[0] = PickOf(0, 0x20, 0x100, 0xFF, sh::Next() & 0x1FF);
        a[1] = PickOf(0, 0x20, 0xE0, 0x100, sh::Next() & 0x1FF);
        a[2] = sh::Next() % 2;
        break;
    case k55Draw:
        a[0] = PickOf(0x80, 0, 8, 0x78, sh::Next() & 0xFF);
        a[1] = PickOf(0, 8, 0x78, 0x80, sh::Next() & 0xFF);
        break;
    case k59Ring: a[0] = PickOf(0, 1, 0x20, 0x60, 0x7F, 0x80, sh::Next() & 0xFF); break;
    case k5ASetTiles:
        a[0] = sh::Next() % 0x20;
        a[1] = sh::Next() % 2;
        break;
    case k5AMove:
        a[0] = PickOf(0, 0x60, 0x90, 0xC0, sh::Next());
        a[1] = PickOf(0, 0xC0, sh::Next());
        break;
    case k5BDrawPart: a[0] = sh::Next() % 3; break;
    default: break;
    }
}

// What the states read again after a call, moved by the group's case of the
// harness's disturbance (from its hash only): +9, +0xA, Cond_ByteFE, the
// variant (inside its five), the camera's cell, the sky's frame word (inside
// its wrap), Camera_Distance (never -0x1194).
void Disturb(U h) {
    const U v = h >> 8;
    unsigned char* const s = Sprite_Current;
    switch (h % 7) {
    case 0:
        if (sh::InRegions(s, 0x80)) s[9] = static_cast<unsigned char>(v);
        break;
    case 1:
        if (sh::InRegions(s, 0x80)) s[0xA] = static_cast<unsigned char>(v);
        break;
    case 2: Cond_ByteFE = static_cast<unsigned char>(v % 5); break;
    case 3:
        if (sh::InRegions(s, 0x80)) SetWord(s + 0x36, v % 5);
        break;
    case 4: SetWord(Mem((v & 1) ? at::kKind2XHigh : at::kKind2ZHigh), v >> 1); break;
    case 5: SetWord(Mem(at::kWalkClock), v % 0x3C0); break;
    case 6: Camera_Distance = static_cast<short>((v & 0xFFFF) == 0xEE6C ? 0xEE6D : v); break;
    default: break;
    }
}

}  // namespace

void SelfTest() {
    // BOF3X_E6C_ONLY: the clones whose name contains it (a control's run)
    static sh::Clone chosen[kCount];
    static unsigned index[kCount];
    const char* const only = std::getenv("BOF3X_E6C_ONLY");
    unsigned n = 0;
    for (unsigned k = 0; k < kCount; ++k)
        if (!only || !*only || std::strstr(kAll[k].name, only)) {
            index[n] = k;
            chosen[n++] = kAll[k];
        }
    if (n == 0) bof3::Fatal("effect_6c: BOF3X_E6C_ONLY=%s names no clone", only);
    static unsigned* s_index = index;
    sh::Group g = {"effect_6c", chosen, n, kCallees, sizeof kCallees / sizeof kCallees[0], kTables,
                   sizeof kTables / sizeof kTables[0], kRegions, sizeof kRegions / sizeof kRegions[0],
                   [](unsigned k) { Seed(s_index[k]); }, &Disturb, 4000};
    g.args = [](unsigned k, U* a) { Args(s_index[k], a); };
    g.effect = true;
    g.kinds = kKinds;
    g.n_kinds = sizeof kKinds;
    sh::Run(g);
}

}  // namespace effect_6c

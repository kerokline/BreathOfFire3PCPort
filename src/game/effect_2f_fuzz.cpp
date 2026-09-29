// BOF3X_SHADOW=effect_2f: group E2F's 62 functions through the scenario harness
// in effect mode (scenario_harness.h, docs/scenario_harness.md section 8), once
// at start-up. docs/effect_2f.md section 4. BOF3X_E2F_ONLY=<name> runs the
// clones whose name contains it (the controls' speed-up).
//
// The clone table is tools/band_rows.py --group E2F --clones --harness scenario
// (2026-09-29; the ten rows the cut does not list by --function), each extent
// read again to its last instruction (capstone) and the names given. Shapes:
// every dispatcher and state kEffect (Sprite_Current one of the 20
// Effect_Objects records, +5 the kind, a dispatcher's +1 / +2 below its table's
// length); the helpers kCall.
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_2f.h"
#include "game/effect_2f_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace effect_2f {
namespace {

namespace sh = scenario_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using U = std::uint32_t;

// tools/band_rows.py --group E2F --clones (and --function for the ten more), 2026-09-29.
constexpr sh::CallSite kCalls47B7D0[] = {{0x13, 0x5A79A0}, {0x2A, 0x5A77C0}, {0x33, 0x461E50}, {0x53, 0x5A75F0},
                                         {0x5B, 0x5A7780}, {0x64, 0x5A9110}, {0xA6, 0x5A7A50}, {0xCA, 0x5A7A00},
                                         {0xF6, 0x5A7A50}, {0x11A, 0x5A7A00}, {0x16D, 0x461E50}};
constexpr sh::CallSite kCalls47B990[] = {{0x1, 0x57CD90}, {0x33, 0x57CD90}, {0x86, 0x57A010}, {0x9C, 0x57A010}, {0x199, 0x454CC0}, {0x1B9, 0x454CC0}};
constexpr sh::CallSite kCalls47BB90[] = {{0x0, 0x47C350}};
constexpr sh::CallSite kCalls47BBB0[] = {{0x9, 0x47C5F0}, {0xE, 0x47C370}};
constexpr sh::CallSite kCalls47BC00[] = {{0x0, 0x47C370}, {0x9, 0x589840}};
constexpr sh::CallSite kCalls47BC30[] = {{0x32, 0x5B93D2}, {0x4A, 0x47C7E0}, {0x65, 0x587740}};
constexpr sh::CallSite kCalls47BCB0[] = {{0x1, 0x47C810}, {0xE, 0x5B93D2}};
constexpr sh::CallSite kCalls47BD10[] = {{0x1, 0x47C810}, {0xE, 0x5B93D2}};
constexpr sh::CallSite kCalls47BD50[] = {{0x1, 0x47C810}, {0xE, 0x5B93D2}, {0x3C, 0x587740}};
constexpr sh::CallSite kCalls47BDC0[] = {{0xA, 0x47CBD0}, {0x3C, 0x587740}};
constexpr sh::CallSite kCalls47BE20[] = {{0xA, 0x47CBD0}, {0x32, 0x589840}};
constexpr sh::CallSite kCalls47BE60[] = {{0x32, 0x5B93D2}, {0x4A, 0x47C7E0}, {0x65, 0x587740}};
constexpr sh::CallSite kCalls47BEE0[] = {{0x1, 0x47C810}, {0xE, 0x5B93D2}};
constexpr sh::CallSite kCalls47BF40[] = {{0x1, 0x47C810}, {0xE, 0x5B93D2}};
constexpr sh::CallSite kCalls47BF80[] = {{0xB, 0x47CBD0}, {0x1C, 0x5B93D2}, {0x57, 0x589840}};
constexpr sh::CallSite kCalls47BFE0[] = {{0x3, 0x494060}, {0x2B, 0x494110}, {0x4F, 0x47C6E0}, {0x5F, 0x47CBD0}};
constexpr sh::CallSite kCalls47C050[] = {{0xA, 0x47CBD0}, {0x32, 0x589840}};
constexpr sh::CallSite kCalls47C0D0[] = {{0x0, 0x47CF00}};
constexpr sh::CallSite kCalls47C0F0[] = {{0x11, 0x587740}, {0x25, 0x47CF20}, {0x2F, 0x4790F0}, {0x37, 0x47CF40}};
constexpr sh::CallSite kCalls47C150[] = {{0x0, 0x47CF40}, {0x9, 0x589840}};
constexpr sh::CallSite kCalls47C160[] = {{0x17, 0x47D040}, {0x21, 0x47D220}};
constexpr sh::CallSite kCalls47C190[] = {{0x2F, 0x5720C0}, {0x59, 0x587740}};
constexpr sh::CallSite kCalls47C240[] = {{0x39, 0x5720C0}, {0x64, 0x587740}};
constexpr sh::CallSite kCalls47C320[] = {{0x21, 0x589840}};
constexpr sh::CallSite kCalls47C370[] = {{0x14, 0x5A79A0}, {0x2A, 0x5A77C0}, {0x33, 0x461E50}, {0x3B, 0x494060}, {0xC3, 0x47C4A0}};
constexpr sh::JumpTable kTables47C370[] = {{0x6D, 0x11C, 4}};
constexpr sh::CallSite kCalls47C4A0[] = {{0xC, 0x5A75D0}, {0x14, 0x5A7780}, {0x58, 0x4941E0}, {0x67, 0x494110},
                                         {0xEE, 0x5A79E0}, {0x105, 0x5A79A0}, {0x140, 0x461E50}};
constexpr sh::CallSite kCalls47C5F0[] = {{0x1, 0x47C6C0}, {0x47, 0x5A7A00}, {0x6F, 0x5A7A50}, {0x7B, 0x5A7A00}, {0x9F, 0x5A7A50}, {0xAB, 0x5A7A00}};
constexpr sh::CallSite kCalls47C6E0[] = {{0x13, 0x5B9550}, {0x40, 0x5B9550}, {0x6E, 0x5A7810}, {0x77, 0x461E50},
                                         {0x85, 0x5B9550}, {0xB2, 0x5B9550}, {0xE0, 0x5A7810}, {0xE9, 0x461E50}};
constexpr sh::CallSite kCalls47C810[] = {{0x3, 0x494060}, {0x2B, 0x494110}, {0x30, 0x47C6E0}, {0x3F, 0x47C7E0}, {0x53, 0x47C870}};
constexpr sh::CallSite kCalls47C870[] = {{0x48, 0x5A7A90}, {0x5E, 0x5A7A00}, {0x7B, 0x5A7630}, {0x83, 0x5A7780}, {0x1A5, 0x5A79A0},
                                         {0x1E2, 0x461E50}, {0x1EE, 0x5A7630}, {0x1F6, 0x5A7780}, {0x2E4, 0x5A79A0}, {0x324, 0x461E50}};
constexpr sh::CallSite kCalls47CBD0[] = {
    {0x2A, 0x5A7630},  {0x32, 0x5A7780},  {0x47, 0x5A7A50},  {0x6D, 0x5A7A00},  {0xA7, 0x5A7A50},  {0xD0, 0x5A7A00},
    {0x107, 0x5A7A50}, {0x11E, 0x5A7A00}, {0x132, 0x5A7A50}, {0x149, 0x5A7A00}, {0x19A, 0x5A79A0}, {0x1A7, 0x461E50},
    {0x1B6, 0x5A7630}, {0x1BE, 0x5A7780}, {0x1D0, 0x5A7A50}, {0x1F0, 0x5A7A00}, {0x21E, 0x5A7A50}, {0x23E, 0x5A7A00},
    {0x279, 0x5A7A50}, {0x288, 0x5A7A00}, {0x2A0, 0x5A7A50}, {0x2AF, 0x5A7A00}, {0x300, 0x5A79A0}, {0x30D, 0x461E50}};
constexpr sh::CallSite kCalls47CF40[] = {{0x12, 0x5A79A0}, {0x2A, 0x5A77C0}, {0x33, 0x461E50}, {0x3B, 0x494060}, {0x5D, 0x4792E0}};
constexpr sh::CallSite kCalls47D040[] = {{0x4B, 0x494060}, {0x5C, 0x494110}, {0x7D, 0x4941E0}, {0xAA, 0x5B9550},
                                         {0xBD, 0x5B9550}, {0xF6, 0x5A7A70}, {0x197, 0x479970}};
constexpr sh::CallSite kCalls47D220[] = {{0x14, 0x5A79A0},  {0x2C, 0x5A77C0},  {0x35, 0x461E50},  {0x5C, 0x47D4F0},  {0x94, 0x5A7610},
                                         {0x9C, 0x5A7780},  {0xB9, 0x5A7A50},  {0xE5, 0x5A7A00},  {0x126, 0x5A7A50}, {0x151, 0x5A7A00},
                                         {0x1AB, 0x461E50}, {0x1CF, 0x5A7A50}, {0x1FB, 0x5A7A00}, {0x227, 0x5A7A50}, {0x253, 0x5A7A00},
                                         {0x276, 0x461E50}, {0x2C0, 0x47D4F0}};
constexpr sh::CallSite kCalls47D4F0[] = {{0x26, 0x5A75F0}, {0x2E, 0x5A7780}, {0x3F, 0x5A7A50}, {0x58, 0x5A7A00},
                                         {0x86, 0x5A7A50}, {0x9F, 0x5A7A00}, {0xED, 0x461E50}};
constexpr sh::CallSite kCalls47D650[] = {{0x68, 0x47D8B0},  {0x74, 0x5A75D0},  {0x82, 0x578EB0},  {0xE3, 0x5A8250},  {0xEC, 0x5A92E0},
                                         {0x145, 0x5A79E0}, {0x15B, 0x5A79A0}, {0x1A6, 0x5B9550}, {0x1CF, 0x5B9550}, {0x1D9, 0x5A77A0},
                                         {0x219, 0x5A7780}, {0x22E, 0x572FA0}, {0x242, 0x47D8B0}};
constexpr sh::CallSite kCalls47D8B0[] = {{0x3B, 0x5A7840}, {0x51, 0x572FA0}};
constexpr sh::CallSite kCalls47D930[] = {{0x2, 0x47DA60}};
constexpr sh::CallSite kCalls47D980[] = {{0xE, 0x5A79A0}, {0x26, 0x5A77C0}, {0x2F, 0x461E50}, {0x36, 0x47DA60}, {0x70, 0x47DAC0}, {0xBE, 0x47DAC0}};
constexpr sh::CallSite kCalls47DA60[] = {{0x3F, 0x572570}};
constexpr sh::CallSite kCalls47DAC0[] = {{0xE, 0x5A7650},  {0x3A, 0x494110}, {0x5A, 0x494110}, {0x81, 0x5A7780}, {0x8A, 0x461E50},
                                         {0x96, 0x5A7650}, {0xB6, 0x494110}, {0xD6, 0x494110}, {0xF3, 0x461E50}, {0x101, 0x5A7780}};

#define E2F_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define E2F_CALLS(a) a, E2F_N(a)
#define E2F_FN(name) reinterpret_cast<const void*>(&::name)
constexpr sh::Shape kEf = sh::Shape::kEffect;
constexpr sh::Shape kCa = sh::Shape::kCall;
constexpr U kW = 0xFFFFFFFFu;
// {name, base, size, calls, imms, tables, ours, ret_mask, calm, shape, pointers, state_span, sub_span, kind}
const sh::Clone kAll[] = {
    {"EffectKind4E_DrawDisc", 0x47B7D0, 0x19D, E2F_CALLS(kCalls47B7D0), nullptr, 0, nullptr, 0, E2F_FN(EffectKind4E_DrawDisc), 0, false, kCa, 0, 0, 0, 0x4E},
    {"EffectKind4F_Run", 0x47B970, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E2F_FN(EffectKind4F_Run), 0, false, kEf, 0, 3, 0, 0x4F},
    {"EffectKind4F_Start", 0x47B990, 0x1D7, E2F_CALLS(kCalls47B990), nullptr, 0, nullptr, 0, E2F_FN(EffectKind4F_Start), 0, false, kEf, 0, 0, 0, 0x4F},
    {"EffectKind50_Run", 0x47BB70, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E2F_FN(EffectKind50_Run), 0, false, kEf, 0, 3, 0, 0x50},
    {"EffectKind50_Start", 0x47BB90, 0x1A, E2F_CALLS(kCalls47BB90), nullptr, 0, nullptr, 0, E2F_FN(EffectKind50_Start), 0, false, kEf, 0, 0, 0, 0x50},
    {"EffectKind50_Emit", 0x47BBB0, 0x47, E2F_CALLS(kCalls47BBB0), nullptr, 0, nullptr, 0, E2F_FN(EffectKind50_Emit), 0, false, kEf, 0, 0, 0, 0x50},
    {"EffectKind50_Drain", 0x47BC00, 0xF, E2F_CALLS(kCalls47BC00), nullptr, 0, nullptr, 0, E2F_FN(EffectKind50_Drain), 0, false, kEf, 0, 0, 0, 0x50},
    {"EffectKind50_ClearSpecks", 0x47C350, 0x14, nullptr, 0, nullptr, 0, nullptr, 0, E2F_FN(EffectKind50_ClearSpecks), 0, false, kCa, 0, 0, 0, 0x50},
    {"EffectKind50_MoveSpecks", 0x47C370, 0x12C, E2F_CALLS(kCalls47C370), nullptr, 0, E2F_CALLS(kTables47C370), E2F_FN(EffectKind50_MoveSpecks), 0xFF, false, kCa, 0, 0, 0, 0x50},
    {"EffectKind50_SpeckQuad", 0x47C4A0, 0x14E, E2F_CALLS(kCalls47C4A0), nullptr, 0, nullptr, 0, E2F_FN(EffectKind50_SpeckQuad), 0, false, kCa, 0, 0, 0, 0x50},
    {"EffectKind50_SpawnSpeck", 0x47C5F0, 0xC3, E2F_CALLS(kCalls47C5F0), nullptr, 0, nullptr, 0, E2F_FN(EffectKind50_SpawnSpeck), 0, false, kCa, 0, 0, 0, 0x50},
    {"EffectKind50_FreeSpeck", 0x47C6C0, 0x19, nullptr, 0, nullptr, 0, nullptr, 0, E2F_FN(EffectKind50_FreeSpeck), kW, false, kCa, 0, 0, 0, 0x50},
    {"EffectKind51_Run", 0x47BC10, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E2F_FN(EffectKind51_Run), 0, false, kEf, 0, 12, 0, 0x51},
    {"EffectKind51_Start", 0x47BC30, 0x7D, E2F_CALLS(kCalls47BC30), nullptr, 0, nullptr, 0, E2F_FN(EffectKind51_Start), 0, false, kEf, 0, 0, 0, 0x51},
    {"EffectKind51_Grow", 0x47BCB0, 0x58, E2F_CALLS(kCalls47BCB0), nullptr, 0, nullptr, 0, E2F_FN(EffectKind51_Grow), 0, false, kEf, 0, 0, 0, 0x51},
    {"EffectKind51_Wait", 0x47BD10, 0x3A, E2F_CALLS(kCalls47BD10), nullptr, 0, nullptr, 0, E2F_FN(EffectKind51_Wait), 0, false, kEf, 0, 0, 0, 0x51},
    {"EffectKind51_Shrink", 0x47BD50, 0x68, E2F_CALLS(kCalls47BD50), nullptr, 0, nullptr, 0, E2F_FN(EffectKind51_Shrink), 0, false, kEf, 0, 0, 0, 0x51},
    {"EffectKind51_RingIn", 0x47BDC0, 0x58, E2F_CALLS(kCalls47BDC0), nullptr, 0, nullptr, 0, E2F_FN(EffectKind51_RingIn), 0, false, kEf, 0, 0, 0, 0x51},
    {"EffectKind51_RingOut", 0x47BE20, 0x38, E2F_CALLS(kCalls47BE20), nullptr, 0, nullptr, 0, E2F_FN(EffectKind51_RingOut), 0, false, kEf, 0, 0, 0, 0x51},
    {"EffectKind51_Start2", 0x47BE60, 0x7D, E2F_CALLS(kCalls47BE60), nullptr, 0, nullptr, 0, E2F_FN(EffectKind51_Start2), 0, false, kEf, 0, 0, 0, 0x51},
    {"EffectKind51_Grow2", 0x47BEE0, 0x58, E2F_CALLS(kCalls47BEE0), nullptr, 0, nullptr, 0, E2F_FN(EffectKind51_Grow2), 0, false, kEf, 0, 0, 0, 0x51},
    {"EffectKind51_Wait2", 0x47BF40, 0x3A, E2F_CALLS(kCalls47BF40), nullptr, 0, nullptr, 0, E2F_FN(EffectKind51_Wait2), 0, false, kEf, 0, 0, 0, 0x51},
    {"EffectKind51_RingFade", 0x47BF80, 0x5D, E2F_CALLS(kCalls47BF80), nullptr, 0, nullptr, 0, E2F_FN(EffectKind51_RingFade), 0, false, kEf, 0, 0, 0, 0x51},
    {"EffectKind51_Burst", 0x47BFE0, 0x68, E2F_CALLS(kCalls47BFE0), nullptr, 0, nullptr, 0, E2F_FN(EffectKind51_Burst), 0, false, kEf, 0, 0, 0, 0x51},
    {"EffectKind51_RingClose", 0x47C050, 0x38, E2F_CALLS(kCalls47C050), nullptr, 0, nullptr, 0, E2F_FN(EffectKind51_RingClose), 0, false, kEf, 0, 0, 0, 0x51},
    {"EffectKind51_DrawMoves", 0x47C6E0, 0xF6, E2F_CALLS(kCalls47C6E0), nullptr, 0, nullptr, 0, E2F_FN(EffectKind51_DrawMoves), 0, false, kCa, 0, 0, 0, 0x51},
    {"EffectKind51_PushAngle", 0x47C7E0, 0x23, nullptr, 0, nullptr, 0, nullptr, 0, E2F_FN(EffectKind51_PushAngle), 0, false, kCa, 0, 0, 0, 0x51},
    {"EffectKind51_DrawFrame", 0x47C810, 0x5C, E2F_CALLS(kCalls47C810), nullptr, 0, nullptr, 0, E2F_FN(EffectKind51_DrawFrame), 0, false, kCa, 0, 0, 0, 0x51},
    {"EffectKind51_DrawColumn", 0x47C870, 0x359, E2F_CALLS(kCalls47C870), nullptr, 0, nullptr, 0, E2F_FN(EffectKind51_DrawColumn), 0, false, kCa, 0, 0, 0, 0x51},
    {"EffectKind51_DrawRing", 0x47CBD0, 0x330, E2F_CALLS(kCalls47CBD0), nullptr, 0, nullptr, 0, E2F_FN(EffectKind51_DrawRing), 0, false, kCa, 0, 0, 0, 0x51},
    {"EffectKind52_Run", 0x47C090, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E2F_FN(EffectKind52_Run), 0, false, kEf, 0, 2, 0, 0x52},
    {"EffectKind52_Sparks", 0x47C0B0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E2F_FN(EffectKind52_Sparks), 0, false, kEf, 0, 0, 3, 0x52},
    {"EffectKind52_SparksClear", 0x47C0D0, 0x19, E2F_CALLS(kCalls47C0D0), nullptr, 0, nullptr, 0, E2F_FN(EffectKind52_SparksClear), 0, false, kEf, 0, 0, 0, 0x52},
    {"EffectKind52_SparksEmit", 0x47C0F0, 0x55, E2F_CALLS(kCalls47C0F0), nullptr, 0, nullptr, 0, E2F_FN(EffectKind52_SparksEmit), 0, false, kEf, 0, 0, 0, 0x52},
    {"EffectKind52_SparksDrain", 0x47C150, 0xF, E2F_CALLS(kCalls47C150), nullptr, 0, nullptr, 0, E2F_FN(EffectKind52_SparksDrain), 0, false, kEf, 0, 0, 0, 0x52},
    {"EffectKind52_Trail", 0x47C160, 0x2A, E2F_CALLS(kCalls47C160), nullptr, 0, nullptr, 0, E2F_FN(EffectKind52_Trail), 0, false, kEf, 0, 0, 6, 0x52},
    {"EffectKind52_TrailStart", 0x47C190, 0x62, E2F_CALLS(kCalls47C190), nullptr, 0, nullptr, 0, E2F_FN(EffectKind52_TrailStart), 0, false, kEf, 0, 0, 0, 0x52},
    {"EffectKind52_TrailRise", 0x47C200, 0x37, nullptr, 0, nullptr, 0, nullptr, 0, E2F_FN(EffectKind52_TrailRise), 0, false, kEf, 0, 0, 0, 0x52},
    {"EffectKind52_TrailTurn", 0x47C240, 0x6D, E2F_CALLS(kCalls47C240), nullptr, 0, nullptr, 0, E2F_FN(EffectKind52_TrailTurn), 0, false, kEf, 0, 0, 0, 0x52},
    {"EffectKind52_TrailFall", 0x47C2B0, 0x37, nullptr, 0, nullptr, 0, nullptr, 0, E2F_FN(EffectKind52_TrailFall), 0, false, kEf, 0, 0, 0, 0x52},
    {"EffectKind52_TrailHold", 0x47C2F0, 0x26, nullptr, 0, nullptr, 0, nullptr, 0, E2F_FN(EffectKind52_TrailHold), 0, false, kEf, 0, 0, 0, 0x52},
    {"EffectKind52_TrailFade", 0x47C320, 0x27, E2F_CALLS(kCalls47C320), nullptr, 0, nullptr, 0, E2F_FN(EffectKind52_TrailFade), 0, false, kEf, 0, 0, 0, 0x52},
    {"EffectKind52_ClearSparks", 0x47CF00, 0x14, nullptr, 0, nullptr, 0, nullptr, 0, E2F_FN(EffectKind52_ClearSparks), 0, false, kCa, 0, 0, 0, 0x52},
    {"EffectSpark_FindFree", 0x47CF20, 0x19, nullptr, 0, nullptr, 0, nullptr, 0, E2F_FN(EffectSpark_FindFree), kW, false, kCa, 0, 0, 0, 0x52},
    {"EffectKind52_MoveSparks", 0x47CF40, 0x73, E2F_CALLS(kCalls47CF40), nullptr, 0, nullptr, 0, E2F_FN(EffectKind52_MoveSparks), 0xFF, false, kCa, 0, 0, 0, 0x52},
    {"EffectKind52_SparkGlow", 0x47CFC0, 0x26, nullptr, 0, nullptr, 0, nullptr, 0, E2F_FN(EffectKind52_SparkGlow), 0, false, kCa, 0, 0, 0, 0x52},
    {"EffectSpark_Wait", 0x47CFF0, 0x1B, nullptr, 0, nullptr, 0, nullptr, 0, E2F_FN(EffectSpark_Wait), 0, false, kCa, 0, 0, 0, 0x52},
    {"EffectKind52_SparkRise", 0x47D010, 0x2F, nullptr, 0, nullptr, 0, nullptr, 0, E2F_FN(EffectKind52_SparkRise), 0, false, kCa, 0, 0, 0, 0x52},
    {"EffectKind52_TrailUpdate", 0x47D040, 0x1D1, E2F_CALLS(kCalls47D040), nullptr, 0, nullptr, 0, E2F_FN(EffectKind52_TrailUpdate), 0, false, kCa, 0, 0, 0, 0x52},
    {"EffectKind52_TrailDraw", 0x47D220, 0x2D0, E2F_CALLS(kCalls47D220), nullptr, 0, nullptr, 0, E2F_FN(EffectKind52_TrailDraw), 0, false, kCa, 0, 0, 0, 0x52},
    {"EffectTrail_DrawCap", 0x47D4F0, 0x109, E2F_CALLS(kCalls47D4F0), nullptr, 0, nullptr, 0, E2F_FN(EffectTrail_DrawCap), 0, false, kCa, 0, 0, 0, 0x52},
    {"EffectKind53_Run", 0x47D600, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E2F_FN(EffectKind53_Run), 0, false, kEf, 0, 2, 0, 0x53},
    {"EffectKind53_Start", 0x47D620, 0x2D, nullptr, 0, nullptr, 0, nullptr, 0, E2F_FN(EffectKind53_Start), 0, false, kEf, 0, 0, 0, 0x53},
    {"EffectKind53_Beam", 0x47D650, 0x251, E2F_CALLS(kCalls47D650), nullptr, 0, nullptr, 0, E2F_FN(EffectKind53_Beam), 0, false, kEf, 0, 0, 0, 0x53},
    {"EffectKind53_TexWindow", 0x47D8B0, 0x5A, E2F_CALLS(kCalls47D8B0), nullptr, 0, nullptr, 0, E2F_FN(EffectKind53_TexWindow), 0, false, kCa, 0, 0, 0, 0x53},
    {"EffectKind56_Run", 0x47D910, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E2F_FN(EffectKind56_Run), 0, false, kEf, 0, 4, 0, 0x56},
    {"EffectKind56_Start", 0x47D930, 0x1E, E2F_CALLS(kCalls47D930), nullptr, 0, nullptr, 0, E2F_FN(EffectKind56_Start), 0, false, kEf, 0, 0, 0, 0x56},
    {"EffectKind56_Wait", 0x47D950, 0x13, nullptr, 0, nullptr, 0, nullptr, 0, E2F_FN(EffectKind56_Wait), 0, false, kEf, 0, 0, 0, 0x56},
    {"EffectKind56_Arm", 0x47D970, 0xA, nullptr, 0, nullptr, 0, nullptr, 0, E2F_FN(EffectKind56_Arm), 0, false, kEf, 0, 0, 0, 0x56},
    {"EffectKind56_Markers", 0x47D980, 0xDB, E2F_CALLS(kCalls47D980), nullptr, 0, nullptr, 0, E2F_FN(EffectKind56_Markers), 0, false, kEf, 0, 0, 0, 0x56},
    {"EffectKind56_Place", 0x47DA60, 0x57, E2F_CALLS(kCalls47DA60), nullptr, 0, nullptr, 0, E2F_FN(EffectKind56_Place), 0, false, kEf, 0, 0, 0, 0x56},
    {"EffectKind56_DrawCross", 0x47DAC0, 0x111, E2F_CALLS(kCalls47DAC0), nullptr, 0, nullptr, 0, E2F_FN(EffectKind56_DrawCross), 0, false, kCa, 0, 0, 0, 0x56},
};
#undef E2F_FN
#undef E2F_CALLS
#undef E2F_N

enum : unsigned {
    k4EDisc, k4FRun, k4FStart, k50Run, k50Start, k50Emit, k50Drain, k50Clear, k50Move, k50Quad, k50Spawn, k50Free,
    k51Run, k51Start, k51Grow, k51Wait, k51Shrink, k51RingIn, k51RingOut, k51Start2, k51Grow2, k51Wait2, k51RingFade,
    k51Burst, k51RingClose, k51Moves, k51Push, k51Frame, k51Column, k51Ring, k52Run, k52Sparks, k52SparksClear,
    k52SparksEmit, k52SparksDrain, k52Trail, k52TrailStart, k52TrailRise, k52TrailTurn, k52TrailFall, k52TrailHold,
    k52TrailFade, k52Clear, kSparkFind, k52Move, k52Glow, kSparkWait, k52Rise, k52TrailUpdate, k52TrailDraw, kCap,
    k53Run, k53Start, k53Beam, k53Window, k56Run, k56Start, k56Wait, k56Arm, k56Markers, k56Place, k56Cross, kCount
};
static_assert(kCount == sizeof kAll / sizeof kAll[0], "one enum entry a clone, in order");

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
unsigned char* P(U a) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a)); }
template <typename... T> U PickOf(T... v) {
    const U values[] = {static_cast<U>(v)...};
    return sh::Pick(values, sizeof...(v));
}
unsigned char* Speck(unsigned i) { return EffectKind30_Shards + (i % at::kSpecks) * at::kSpeckStride; }
unsigned char* Spark(unsigned i) { return EffectKind30_Shards + (i % at::kSparks) * at::kSparkStride; }
unsigned char* TrailAt() { return P(at::kTrail); }

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
// infinity): what a projected screen word or depth is.
void FillFloat(unsigned char* at) {
    const U n = sh::Noise();
    const U bits = (n & 0x807FFFFFu) | ((0x6Eu + (sh::Noise() % 0x24u)) << 23);
    std::memcpy(at, &bits, 4);
}
// EffectGte_ProjectPoint: out[0..2] the screen x, y and depth. A quarter of the
// time the three floats 0x20 below `out` (the trail's point before, whose steps
// kind 0x52 turns into angles) exactly, a quarter of the time those with x and
// y moved by -2..2, else fresh floats - so equal and near points happen.
U FxProjectPoint(const U* a, U answer) {
    if (!Writable(a[1], 12)) return answer;
    unsigned char* const out = P(a[1]);
    const U n = sh::Noise();
    // only where both lie in the regions: a stack out's neighbour differs
    // between the copy's frame and ours
    const bool before = sh::InRegions(out, 12) && sh::InRegions(out - 0x20, 12);
    // a world point equal to the one 0x20 below it (the trail's, seeded so)
    // projects to the same screen point: whole trails of zero steps happen
    const bool same = before && sh::InRegions(P(a[0]), 12) && sh::InRegions(P(a[0] - 0x20), 12) &&
                      std::memcmp(P(a[0]), P(a[0] - 0x20), 12) == 0;
    if (same || (before && n % 4 == 0)) {
        std::memcpy(out, out - 0x20, 12);
    } else if (before && n % 4 == 1) {
        std::memcpy(out, out - 0x20, 12);
        for (unsigned i = 0; i < 2; ++i) {
            float f;
            std::memcpy(&f, out + 4 * i, 4);
            f += static_cast<float>(static_cast<int>((n >> (2 + 3 * i)) % 5) - 2);
            std::memcpy(out + 4 * i, &f, 4);
        }
    } else {
        for (unsigned i = 0; i < 3; ++i) FillFloat(out + 4 * i);
    }
    return answer;
}
// EffectGte_ProjectSize: out[0..1], the scaled size - small half the time.
U FxProjectSize(const U* a, U answer) {
    if (!Writable(a[2], 4)) return answer;
    unsigned char* const out = P(a[2]);
    const U n = sh::Noise();
    SetWord(out, (n & 1) ? (n >> 1) % 0x40 : n >> 1);
    SetWord(out + 2, sh::Noise());
    return answer;
}
// Gte_RotTransPers (EffectKind53_Beam): the screen point, two floats the beam
// takes x +- 8.0f and y (compared with 240.0f, then ftol'd) from - fractions,
// y at and about 240 a third of the time, one time in eight a value _ftol
// answers with the integer indefinite; the depth-cue word filled; the answer (the
// depth the beam's shade and link depend on) at and about 0x1E0 and 0x220 half
// the time.
U FxRotTransPers(const U* a, U answer) {
    if (Writable(a[1], 8)) {
        unsigned char* const out = P(a[1]);
        for (unsigned i = 0; i < 2; ++i) {
            const U n = sh::Noise();
            float f;
            if (n % 8 == 0) {
                const U odd[] = {0x7FC00000u, 0xFFC00000u, 0x5F000000u, 0xDF000001u, 0x7F7FFFFFu, 0xFF7FFFFFu, 0x7F800000u};
                std::memcpy(&f, &odd[(n >> 3) % 7], 4);
            } else if (i == 1 && n % 3 == 0) {
                const float near[] = {239.0f, 239.5f, 239.99998f, 240.0f, 240.00002f, 240.5f, 241.0f};
                f = near[(n >> 3) % 7];
            } else {
                f = static_cast<float>(static_cast<int>((n >> 3) % 0x300) - 0x80) + static_cast<float>((n >> 13) % 64) / 64.0f;
            }
            std::memcpy(out + 4 * i, &f, 4);
        }
    }
    if (Writable(a[2], 4)) sh::FillBytes(P(a[2]), 4);
    // from the log's noise only (the harness's Next() stream differs between the passes)
    const U n = sh::Noise();
    const U near[] = {0x1DF, 0x1E0, 0x1E1, 0x1FF, 0x21F, 0x220, 0x221, 0x7FFFFFFF, 0x80000000u, 0};
    if (n % 2 == 0) return near[(n >> 1) % 10];
    return answer;
}
// EffectKind50_FreeSpeck: a quarter of the time none, else one of the 64 specks.
U FxFreeSpeck(const U*, U answer) { return answer % 4 == 0 ? 0 : Key(Speck(answer >> 2)); }
// EffectSpark_FindFree: a quarter of the time none, else one of the 8 sparks.
U FxFindSpark(const U*, U answer) { return answer % 4 == 0 ? 0 : Key(Spark(answer >> 2)); }

#define E2F_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
constexpr sh::Answer kG = sh::Answer::kGarbage;
constexpr sh::Answer kP = sh::Answer::kPhase;
constexpr sh::Answer kF = sh::Answer::kFlag;
constexpr U k16 = 0xFFFFu, k8 = 0xFFu;
const sh::Callee kCallees[] = {
    // the group's own called directly, by name: each read as the callee reads it
    // (the callers push whole registers; docs/effect_2f.md section 3)
    {E2F_OURS(EffectKind50_ClearSpecks), 0, {}, kP, 0, 0},
    {E2F_OURS(EffectKind50_MoveSpecks), 0, {}, kF, 0, 0},
    {E2F_OURS(EffectKind50_SpeckQuad), 1, {kW}, kG, 0, 0, {0x28}, nullptr, nullptr, true},
    {E2F_OURS(EffectKind50_SpawnSpeck), 0, {}, kP, 0, 0},
    {E2F_OURS(EffectKind50_FreeSpeck), 0, {}, kG, 0, 0, {}, &FxFreeSpeck, nullptr, true},
    {E2F_OURS(EffectKind51_PushAngle), 1, {k16}, kG, 0, 0},
    {E2F_OURS(EffectKind51_DrawFrame), 0, {}, kP, 0, 0},
    {E2F_OURS(EffectKind51_DrawMoves), 0, {}, kP, 0, 0},
    {E2F_OURS(EffectKind51_DrawColumn), 2, {k16, k16}, kG, 0, 0},
    {E2F_OURS(EffectKind51_DrawRing), 1, {k16}, kG, 0, 0},
    {E2F_OURS(EffectKind52_ClearSparks), 0, {}, kP, 0, 0},
    {E2F_OURS(EffectSpark_FindFree), 0, {}, kG, 0, 0, {}, &FxFindSpark, nullptr, true},
    {E2F_OURS(EffectKind52_MoveSparks), 0, {}, kF, 0, 0},
    {E2F_OURS(EffectKind52_TrailUpdate), 1, {kW}, kG, 0, 0},
    {E2F_OURS(EffectKind52_TrailDraw), 1, {kW}, kG, 0, 0},
    {E2F_OURS(EffectTrail_DrawCap), 4, {kW, k16, k16, k8}, kG, 0, 0, {12, 0, 0, 0}, nullptr, nullptr, true},
    {E2F_OURS(EffectKind53_TexWindow), 4, {k16, k16, k16, k16}, kG, 0, 0},
    {E2F_OURS(EffectKind56_Place), 0, {}, kP, 0, 0},
    {E2F_OURS(EffectKind56_DrawCross), 4, {kW, kW, kW, k8}, kG, 0, 0},
    // nobody's this round, by address: E2E's angle mean reads both words & 0xFFF
    // (0x479970: and eax, 0xFFF; and ecx, 0xFFF); the spark draw 0x4792E0 reads
    // the spark's shade +3 and point +0xC..+0x17
    {"0x479970", at::kAngleMean, at::kAngleMean, 2, {0xFFFu, 0xFFFu}, kG, 0, 0, {}, nullptr, nullptr, true},
    {"0x4792E0", at::kSparkDraw, at::kSparkDraw, 1, {kW}, kG, 0, 0, {0x18}, nullptr, nullptr, true},
    // standard rows re-listed: the point and the out are often stack locals
    // (their addresses differ between the copy and ours) - hashed, not logged
    {E2F_OURS(EffectGte_ProjectPoint), 2, {0, 0}, kG, 0, 0, {12, 0}, &FxProjectPoint, nullptr, true},
    // the size's second word is never written by EffectKind50_SpeckQuad's
    // original (a stack word): only the first hashed
    {E2F_OURS(EffectGte_ProjectSize), 3, {0, 0, 0}, kG, 0, 0, {12, 2, 0}, &FxProjectSize, nullptr, true},
    // the vertex's fourth word is not written by EffectKind53_Beam's original:
    // three hashed; the screen point is the packet (logged), p a local
    {E2F_OURS(Gte_RotTransPers), 3, {0, kW, 0}, kG, 0, 0, {6, 0, 0}, &FxRotTransPers, nullptr, true},
};
#undef E2F_OURS

// The eight state tables the dispatchers jump or call through, read in place;
// each table's length to the next table a dispatcher names (none bounded by a
// compare). EffectKind52_SparkStates (0x65472C, 3) is left in place: its
// handlers take the spark as an argument, which a handler recorder does not log,
// so both sides run Capcom's three (they write only the spark, compared).
const sh::DataTable kTables[] = {
    {0x6546A4, 3}, {0x6546C4, 3}, {0x6546D0, 12}, {0x654700, 2}, {0x654708, 3}, {0x654714, 6}, {0x654738, 2}, {0x654740, 4},
};
const std::uint8_t kKinds[] = {0x4F, 0x50, 0x51, 0x52, 0x53, 0x56};

// Beyond effect mode's standard regions: kind 0x50's 64 specks of 0x28 run past
// EffectKind30_Shards' standard 0x644; EffectKind51_Angles.
const sh::Region kRegions[] = {
    {0x92BF80 + 0x644, at::kSpecks * at::kSpeckStride - 0x644},
    {0x6761DC, 2 * at::kAngles},
};

// --- the seed ------------------------------------------------------------------------

// A float at p: half the time a whole number of -0x80..0x27F with a fraction of
// 1/64ths (a screen coordinate), else raw bits (NaN, infinities, huge).
void SeedFloat(unsigned char* p) {
    if (sh::Half()) {
        const U n = sh::Next();
        const float f = static_cast<float>(static_cast<int>(n % 0x300) - 0x80) + static_cast<float>((n >> 12) % 64) / 64.0f;
        std::memcpy(p, &f, 4);
    }
}
// The record's screen point +0x74 / +0x78 and depth +0x7C.
void SeedScreen(unsigned char* s) {
    for (unsigned i = 0; i < 3; ++i) SeedFloat(s + 0x74 + 4 * i);
}

// Kind 0x50's specks: half in use, the phase +2 0..4 mostly, the count +1 at 0
// and 1 often.
void SeedSpecks() {
    for (unsigned i = 0; i < at::kSpecks; ++i) {
        unsigned char* const k = Speck(i);
        k[0] = static_cast<unsigned char>(sh::Half() ? 0 : PickOf(1, 0x80, sh::Next() | 1));
        if (sh::Often()) k[2] = static_cast<unsigned char>(sh::Next() % 5);
        if (sh::Half()) k[1] = static_cast<unsigned char>(PickOf(0, 1, 2));
        SeedFloat(k + 0x10);
    }
}
// Kind 0x52's sparks: half in use, each state +1 below 3 (MoveSparks calls
// through EffectKind52_SparkStates by it, unbounded), the count +2 at 1 often.
void SeedSparks() {
    for (unsigned i = 0; i < at::kSparks; ++i) {
        unsigned char* const k = Spark(i);
        k[0] = static_cast<unsigned char>(sh::Half() ? 0 : PickOf(1, 0x80, sh::Next() | 1));
        k[1] = static_cast<unsigned char>(sh::Next() % 3);
        if (sh::Half()) k[2] = static_cast<unsigned char>(PickOf(0, 1, 2));
    }
}
// The trail at 0x92C060: its screen floats, half the pairs equal or near.
void SeedTrail() {
    unsigned char* const t = TrailAt();
    for (unsigned k = 0; k < 0x20; ++k) {
        unsigned char* const p = t + 0x20 * k + 0x10;
        SeedFloat(p);
        SeedFloat(p + 4);
        if (k > 0 && sh::Half()) {
            std::memcpy(p, p - 0x20, 8);
            if (sh::Half()) {
                float f;
                std::memcpy(&f, p + 4 * (sh::Next() % 2), 4);
                f += 1.0f;
                std::memcpy(p + 4 * (sh::Next() % 2), &f, 4);
            }
        }
    }
    // the angle words at +0x400: some 0x1000 (none), so the fill runs
    for (unsigned k = 0; k < 0x1F; ++k)
        if (sh::Half()) SetWord(t + 0x400 + 2 * k, 0x1000);
}
// Every record's ground +0x14 near its height +0x3C (kind 0x56's walk), never in
// the lowest 2^24 of the range, where the original's walk never ends.
void SeedGrounds() {
    for (unsigned r = 0; r < at::kEffects; ++r) {
        unsigned char* const e = sh::EffectRecord(r);
        U ground = static_cast<U>(Long(e + 0x14));
        if (sh::Often()) ground = static_cast<U>(Long(e + 0x3C)) - ((sh::Next() % 8) << 24) - (sh::Next() & 0xFFFFFFu);
        if ((ground >> 24) == 0x80) ground |= 0x01000000u;
        SetLong(e + 0x14, static_cast<std::int32_t>(ground));
    }
}

// +9 at every compare the states make: 1 (down to 0), 0 (down to 0xFF).
U FrameCount() { return PickOf(0, 1, 2, 0x20, 0x80, 0xFF, sh::Next()); }
// The timer word +0x5A at its compares: 1 (down to 0), 2, 4 (the sounds), the
// low two / three bits 0.
U Timer() { return PickOf(0, 1, 2, 3, 4, 5, 8, 0x10, 0x20, 0x96, 0xFFFF, sh::Next()); }

void Seed(unsigned k) {
    unsigned char* const s = Sprite_Current;
    s[9] = static_cast<unsigned char>(FrameCount());
    SetWord(s + 0x5A, Timer());
    SeedScreen(s);
    if (sh::Half()) sh::Mem(at::kCounter0)[0] = 0xB;
    sh::Mem(at::kCounter3)[0] = static_cast<unsigned char>(PickOf(0xA, 0xE, 0xB, sh::Next()));
    switch (k) {
    case k4FStart:
        // every record's sprite indices +3 / +4 inside the thirty: the
        // disturbance moves Sprite_Current between the two Sprite_FindFree calls,
        // and the original reads the new record's +3 (in the game it never moves)
        for (unsigned r = 0; r < at::kEffects; ++r) {
            sh::EffectRecord(r)[3] = static_cast<unsigned char>(sh::Next() % at::kSprites);
            sh::EffectRecord(r)[4] = static_cast<unsigned char>(sh::Next() % at::kSprites);
        }
        break;
    case k50Spawn: s[6] = static_cast<unsigned char>(sh::Half() ? 7 : sh::Next()); break;
    case k50Move:
    case k50Free:
    case k50Drain:
    case k50Emit:
        SeedSpecks();
        if (k == k50Free && sh::Half())
            for (unsigned i = 0; i < at::kSpecks; ++i) Speck(i)[0] |= 1;   // none free
        break;
    case k52Move:
    case kSparkFind:
    case k52SparksEmit:
    case k52SparksDrain:
        SeedSparks();
        if (k == kSparkFind && sh::Half())
            for (unsigned i = 0; i < at::kSparks; ++i) Spark(i)[0] |= 1;   // none free
        break;
    case k52Glow:
    case kSparkWait:
    case k52Rise: SeedSparks(); break;
    case k52TrailUpdate:
    case k52TrailDraw:
        SeedTrail();
        // a third of the time every world point the record's, so every step is
        // (0, 0) and no angle is found (all 0)
        if (k == k52TrailUpdate && sh::Next() % 3 == 0)
            for (unsigned p = 0; p < 0x20; ++p) std::memcpy(TrailAt() + 0x20 * p, s + 0x34, 12);
        break;
    case k53Start:
    case k53Beam:
        // the extra sprite +0x18 names: its +9 zero or not, its +0x14's sign
        SetLong(s + 0x18, static_cast<std::int32_t>(sh::Next() % at::kExtras));
        if (sh::Half()) sh::Mem(0x802000 + at::kSpriteStride * (Long(s + 0x18) & 3) + 9)[0] = 0;
        // its +0x14 at the sign's boundary (the record's +9 steps down below 0)
        SetLong(sh::Mem(0x802000 + at::kSpriteStride * (Long(s + 0x18) & 3) + 0x14),
                static_cast<std::int32_t>(PickOf(0, 1, 0xFFFFFFFFu, 0x80000000u, 0x7FFFFFFF, sh::Next())));
        break;
    case k56Markers: SeedGrounds(); break;
    default: break;
    }
}

void Args(unsigned k, U* a) {
    switch (k) {
    case k4EDisc:
        if (sh::Half()) a[0] = (sh::Next() & 0xFFFF0000u) | (sh::Next() % 0x4C);
        break;
    case k50Quad: a[0] = Key(Speck(sh::Next())); break;
    case k51Column:
        // the half-height: the states keep it 0..0x20 (2 h rows over 65 angles);
        // 0 and below draw nothing
        a[0] = (sh::Next() & 0xFFFF0000u) | PickOf(0, 1, 2, 0x10, 0x1F, 0x20, 0xFFFF, 0x8000, sh::Next() % 0x21);
        break;
    case k52Glow:
    case kSparkWait:
    case k52Rise: a[0] = Key(Spark(sh::Next())); break;
    case k52TrailUpdate:
    case k52TrailDraw: a[0] = at::kTrail; break;
    case kCap: a[0] = at::kTrail + 0x10 + 0x20 * (sh::Next() % 0x20); break;
    default: break;
    }
}

// What the states read again after a call, moved by the group's case of the
// harness's disturbance (from its hash only): the frame count, the timer word,
// the half-height, the counters kinds 0x51 and 0x56 wait on, the screen point.
void Disturb(U h) {
    const U v = h >> 8;
    unsigned char* const s = Sprite_Current;
    switch (h % 6) {
    case 0: s[9] = static_cast<unsigned char>((v & 1) ? 1u : v >> 1); break;
    case 1: SetWord(s + 0x5A, (v & 1) ? 1u + ((v >> 1) & 3) : v >> 1); break;
    case 2: SetWord(s + 0x32, (v >> 1) % 0x21); break;
    case 3: sh::Mem(at::kCounter0)[0] = static_cast<unsigned char>((v & 1) ? 0xB : v >> 1); break;
    case 4: sh::Mem(at::kCounter3)[0] = static_cast<unsigned char>((v & 1) ? 0xA + 4 * ((v >> 1) & 1) : v >> 2); break;
    case 5: {
        const float f = static_cast<float>(static_cast<int>(v % 0x300) - 0x80);
        std::memcpy(s + 0x74 + 4 * ((v >> 10) % 3), &f, 4);
        break;
    }
    default: break;
    }
}

}  // namespace

void SelfTest() {
    // BOF3X_E2F_ONLY: the clones whose name contains it (a control's run)
    static sh::Clone chosen[kCount];
    static unsigned index[kCount];
    const char* const only = std::getenv("BOF3X_E2F_ONLY");
    unsigned n = 0;
    for (unsigned k = 0; k < kCount; ++k)
        if (!only || !*only || std::strstr(kAll[k].name, only)) {
            index[n] = k;
            chosen[n++] = kAll[k];
        }
    if (n == 0) bof3::Fatal("effect_2f: BOF3X_E2F_ONLY=%s names no clone", only);
    static unsigned* s_index = index;
    sh::Group g = {"effect_2f", chosen, n, kCallees, sizeof kCallees / sizeof kCallees[0], kTables,
                   sizeof kTables / sizeof kTables[0], kRegions, sizeof kRegions / sizeof kRegions[0],
                   [](unsigned k) { Seed(s_index[k]); }, &Disturb, 6000};
    g.args = [](unsigned k, U* a) { Args(s_index[k], a); };
    g.effect = true;
    g.kinds = kKinds;
    g.n_kinds = sizeof kKinds;
    sh::Run(g);
}

}  // namespace effect_2f

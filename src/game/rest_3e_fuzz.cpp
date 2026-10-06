// BOF3X_SHADOW=rest_3e: group R3E's 50 functions through the scenario harness
// (scenario_harness.h, used unchanged) in effect mode, once at start-up.
// docs/rest_3e.md section 4. BOF3X_R3E_ONLY=<text> runs the clones whose name
// contains it (the controls' speed-up).
//
// The clone rows are tools/band_rows.py --group R3E --clones --harness scenario
// (2026-10-04), each extent read again to its last instruction (capstone); the
// cut's sizes are padding past them. Shapes: the states, the dispatchers and
// the void helpers kEffect (Sprite_Current one of the 20 records, +5 the
// clone's kind); the helpers handed a record, a pool entry or words kCall,
// their arguments set by Args. Three kinds' state tables are DataTables
// (recorders while the fuzz runs); EffectGlowSparks_States' three handlers
// take the spark, so the table holds typed stand-ins of this file's while the
// fuzz runs (a handler recorder logs no arguments) and is put back after.
// Every callee whose width the standard rows compare wider than the callee
// reads is re-listed here (the originals push whole registers whose upper
// bytes ours cannot hold).
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/rest_3e.h"
#include "game/rest_3e_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace rest_3e {
namespace {

namespace sh = scenario_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using U = std::uint32_t;

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
unsigned char* Mem(U a) { return sh::Mem(a); }
template <typename... T> U PickOf(T... v) {
    const U values[] = {static_cast<U>(v)...};
    return sh::Pick(values, sizeof...(v));
}

// tools/band_rows.py --group R3E --clones --harness scenario, 2026-10-04.
constexpr sh::CallSite kCalls46D5F0[] = {{0x20, 0x5B9380}, {0x34, 0x5A77C0}, {0x44, 0x461E50}, {0x7E, 0x5A79E0}, {0xCB, 0x5A7710}, {0xDB, 0x461E50}};
constexpr sh::CallSite kCalls46D710[] = {{0x3D, 0x46C200}, {0x42, 0x46C430}};
constexpr sh::CallSite kCalls46D770[] = {{0x0, 0x46C310}, {0x5, 0x46C4B0}};
constexpr sh::CallSite kCalls46E190[] = {{0x11, 0x494110}, {0x1D, 0x5A7750}, {0x25, 0x5A7780}, {0x3F, 0x5B93D2}, {0x5C, 0x461E50}};
constexpr sh::CallSite kCalls46F570[] = {{0x10, 0x5A7A50}, {0x26, 0x5A7A00}, {0x3C, 0x494180}, {0x4B, 0x5A7F80}, {0x5A, 0x5A7FF0}, {0x6E, 0x5A7CF0}, {0xCC, 0x5A7CF0}};
constexpr sh::CallSite kCalls46F690[] = {{0xE, 0x5A79A0}, {0x26, 0x5A77C0}, {0x2F, 0x461E50}, {0x34, 0x494060}, {0x44, 0x46F6F0}, {0x50, 0x46F6F0}};
constexpr sh::CallSite kCalls46F6F0[] = {{0x9, 0x5A75F0}, {0x11, 0x5A7780}, {0x30, 0x494110}, {0x4B, 0x494110}, {0x66, 0x494110}, {0x90, 0x461E50}};
constexpr sh::CallSite kCalls46FAE0[] = {{0x14, 0x5A79A0}, {0x2C, 0x5A77C0}, {0x35, 0x461E50}, {0x43, 0x5A7A50}, {0x4F, 0x5A7A00},
                                         {0x5F, 0x5A7A00}, {0x6B, 0x5A7A00}, {0x7B, 0x5A7A50}, {0x8A, 0x494060}, {0x9E, 0x5A76B0},
                                         {0xA6, 0x5A7780}, {0xFD, 0x494110}, {0x18A, 0x494110}, {0x1C9, 0x461E50}};
constexpr sh::CallSite kCalls4790F0[] = {{0x1B, 0x5B93D2}, {0x3B, 0x5B93D2}};
constexpr sh::CallSite kCalls479160[] = {{0x1F, 0x5B93D2}, {0x2C, 0x5B93D2}, {0x39, 0x5B93D2}, {0x50, 0x5B93D2}, {0x70, 0x5A7A00},
                                         {0x78, 0x5A7A50}, {0x9E, 0x5A7A00}, {0xAA, 0x5A7A00}, {0xCE, 0x5A7A50}};
constexpr sh::CallSite kCalls479260[] = {{0x12, 0x5A79A0}, {0x2A, 0x5A77C0}, {0x33, 0x461E50}, {0x3B, 0x494060}, {0x5D, 0x4792E0}};
constexpr sh::CallSite kCalls4792E0[] = {{0x20, 0x4941E0}, {0x2B, 0x494110}, {0x57, 0x5A75F0}, {0x5F, 0x5A7780}, {0x73, 0x5A7A50},
                                         {0x8E, 0x5A7A00}, {0xAF, 0x5A7A50}, {0xCA, 0x5A7A00}, {0x11F, 0x461E50}};
constexpr sh::CallSite kCalls479420[] = {{0x33, 0x587740}};
constexpr sh::CallSite kCalls479470[] = {{0x1A, 0x587740}};
constexpr sh::CallSite kCalls4794D0[] = {{0x4B, 0x494060}, {0x5C, 0x494110}, {0x7D, 0x4941E0}, {0xAA, 0x5B9550}, {0xBD, 0x5B9550},
                                         {0xF6, 0x5A7A70}, {0x197, 0x479970}};
constexpr sh::CallSite kCalls4796B0[] = {{0x14, 0x5A79A0}, {0x2C, 0x5A77C0}, {0x35, 0x461E50}, {0x5C, 0x47D4F0},  {0x94, 0x5A7610},
                                         {0x9C, 0x5A7780}, {0xB9, 0x5A7A50}, {0xE5, 0x5A7A00}, {0x11D, 0x5A7A50}, {0x148, 0x5A7A00},
                                         {0x199, 0x461E50}, {0x1BD, 0x5A7A50}, {0x1E9, 0x5A7A00}, {0x215, 0x5A7A50}, {0x241, 0x5A7A00},
                                         {0x264, 0x461E50}, {0x2AE, 0x47D4F0}};
constexpr sh::CallSite kCalls4799C0[] = {{0x7, 0x494060}, {0x2F, 0x5A8B60}, {0x6C, 0x5A7A00}, {0x74, 0x5A7A50}, {0x89, 0x5A7A00},
                                         {0x91, 0x5A7A00}, {0xA6, 0x5A7A50}, {0xDD, 0x5A7A90}, {0x12B, 0x494110}};
constexpr sh::CallSite kCalls479B70[] = {{0x4F, 0x494060}, {0x89, 0x5A7A00}, {0x91, 0x5A7A50}, {0xA6, 0x5A7A00}, {0xAE, 0x5A7A00},
                                         {0xC3, 0x5A7A50}, {0xFA, 0x5A7A90}, {0x148, 0x494110}, {0x18A, 0x5A79A0}, {0x1A2, 0x5A77C0},
                                         {0x1AB, 0x461E50}, {0x1EE, 0x5A7610}, {0x1F6, 0x5A7780}, {0x328, 0x461E50}};
constexpr sh::CallSite kCalls479EE0[] = {{0xA, 0x5A7A50}, {0x1D, 0x5A7A00}, {0x3F, 0x494110}, {0x5A, 0x494110}, {0x95, 0x5A7A50},
                                         {0xA6, 0x5A7A00}, {0xCE, 0x5A79A0}, {0xE7, 0x5A77C0}, {0xFA, 0x572FA0}, {0x106, 0x5A7610},
                                         {0x10E, 0x5A7780}, {0x15E, 0x494110}, {0x17A, 0x494110}, {0x207, 0x572FA0}};
constexpr sh::CallSite kCalls47A150[] = {{0x19, 0x5B93D2}, {0x30, 0x5B93D2}, {0x3F, 0x5B93D2}, {0x53, 0x5A7A50}, {0x6B, 0x5A7A00}, {0x89, 0x5720C0}};
constexpr sh::CallSite kCalls47A200[] = {{0x13, 0x5A79A0}, {0x2B, 0x5A77C0}, {0x34, 0x461E50}, {0x3C, 0x494060}, {0x56, 0x47A2B0}, {0x63, 0x47A3D0}};
constexpr sh::CallSite kCalls47A2B0[] = {{0xF, 0x5A75B0}, {0x17, 0x5A7780}, {0x47, 0x494110}, {0xB9, 0x494110}, {0x103, 0x461E50}};
constexpr sh::CallSite kCalls47A3D0[] = {{0x1D, 0x5720C0}, {0x36, 0x494110}, {0x40, 0x5A7A50}, {0x50, 0x5A7A00}, {0x66, 0x5720C0},
                                         {0x7F, 0x494110}, {0x9C, 0x5A75F0}, {0xA4, 0x5A7780}, {0xE8, 0x5A7A50}, {0xF8, 0x5A7A00},
                                         {0x10E, 0x5720C0}, {0x127, 0x494110}, {0x165, 0x461E50}};
constexpr sh::CallSite kCalls47F2D0[] = {{0x5, 0x589590}, {0x5A, 0x5891F0}};
constexpr sh::CallSite kCalls47F340[] = {{0x63, 0x595F60}};
constexpr sh::CallSite kCalls47F3E0[] = {{0x2C, 0x595C50}, {0x75, 0x5893A0}, {0x7A, 0x5890E0}, {0x95, 0x591680}, {0xA5, 0x5B9380},
                                         {0xC8, 0x516B30}, {0xDB, 0x5919B0}, {0xF0, 0x5B9380}, {0x117, 0x516B30}, {0x137, 0x47F7A0}};
constexpr sh::CallSite kCalls47F550[] = {{0x48, 0x595F60}};
constexpr sh::CallSite kCalls47F5F0[] = {{0x9, 0x47F2D0}};
constexpr sh::CallSite kCalls47F610[] = {{0x0, 0x47F9E0}, {0x33, 0x587740}, {0x55, 0x5891F0}, {0x6B, 0x587740}, {0x91, 0x587740},
                                         {0xB5, 0x5919B0}, {0xC7, 0x587740}, {0xF7, 0x591B60}, {0x101, 0x587740}};
constexpr sh::CallSite kCalls47F720[] = {{0x1C, 0x595C50}, {0x3A, 0x47F7A0}, {0x64, 0x587740}};
constexpr sh::CallSite kCalls47F7A0[] = {{0xC, 0x5A75D0}, {0x1F, 0x5A79A0}, {0x2F, 0x5A79E0}, {0xD1, 0x461E50}, {0x141, 0x47F910}};
constexpr sh::CallSite kCalls47F910[] = {{0x8, 0x5A75D0}, {0x1B, 0x5A79A0}, {0x2B, 0x5A79E0}, {0xBC, 0x461E50}};
constexpr sh::CallSite kCalls47F9E0[] = {{0x27, 0x57CF60}, {0x3B, 0x5B9380}, {0x5E, 0x516B30}, {0x63, 0x47FAF0}, {0x94, 0x5905D0},
                                         {0x9C, 0x47FBE0}, {0xF1, 0x595C50}, {0xF9, 0x5893A0}, {0xFE, 0x5890E0}};
constexpr sh::CallSite kCalls47FAF0[] = {{0x2E, 0x57CF60}, {0x56, 0x591680}, {0x62, 0x5919B0}, {0x7C, 0x57D800}, {0x90, 0x516B30},
                                         {0xAA, 0x5B9380}, {0xBE, 0x517090}};
constexpr sh::CallSite kCalls47FBE0[] = {{0x29, 0x57CF60}, {0x35, 0x5A75D0}, {0x48, 0x5A79A0}, {0x58, 0x5A79E0}, {0x188, 0x461E50}};
constexpr sh::CallSite kCalls47FDC0[] = {{0x53, 0x587740}, {0x5D, 0x587740}};
constexpr sh::CallSite kCalls47FE30[] = {{0x42, 0x480300}, {0x96, 0x587740}, {0xA0, 0x587740}};
constexpr sh::CallSite kCalls47FEE0[] = {{0x51, 0x480300}};
constexpr sh::CallSite kCalls47FF60[] = {{0x3A, 0x5720C0}, {0x92, 0x587740}, {0x9C, 0x587740}};
constexpr sh::CallSite kCalls480010[] = {{0x51, 0x480300}, {0x67, 0x589840}};
constexpr sh::CallSite kCalls480080[] = {{0x3F, 0x587740}, {0x49, 0x587740}};
constexpr sh::CallSite kCalls4800E0[] = {{0x42, 0x480300}, {0x58, 0x589840}};
constexpr sh::CallSite kCalls480140[] = {{0x3F, 0x587740}};
constexpr sh::CallSite kCalls480190[] = {{0x42, 0x480300}, {0x58, 0x589840}};

#define R3E_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define R3E_CALLS(a) a, R3E_N(a)
#define R3E_FN(name) reinterpret_cast<const void*>(&::name)
constexpr sh::Shape kEf = sh::Shape::kEffect;
constexpr sh::Shape kCa = sh::Shape::kCall;
constexpr U kRec0 = sh::ArgAt(0, sh::Arg::kEffect);
// {name, base, size, calls, imms, tables, ours, ret_mask, calm, shape, pointers, state_span, sub_span, kind}
const sh::Clone kAll[] = {
    {"EffectKind37_Run", 0x46A320, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, R3E_FN(EffectKind37_Run), 0, false, kEf, 0, EffectKind37_States_count, 0, 0x37},
    {"EffectKind17_Run", 0x46ABB0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, R3E_FN(EffectKind17_Run), 0, false, kEf, 0, EffectKind17_States_count, 0, 0x17},
    {"EffectKind1B_Run", 0x46B7A0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, R3E_FN(EffectKind1B_Run), 0, false, kEf, 0, EffectKind1B_States_count, 0, 0x1B},
    {"EffectKind41_DrawNumber", 0x46D5F0, 0x114, R3E_CALLS(kCalls46D5F0), nullptr, 0, nullptr, 0, R3E_FN(EffectKind41_DrawNumber), 0, false, kCa, 0, 0, 0, 0x41},
    {"EffectShards_LoadModel", 0x46D710, 0x52, R3E_CALLS(kCalls46D710), nullptr, 0, nullptr, 0, R3E_FN(EffectShards_LoadModel), 0, false, kEf, 0, 0, 0, 0x30},
    {"EffectShards_Step", 0x46D770, 0xA, R3E_CALLS(kCalls46D770), nullptr, 0, nullptr, 0, R3E_FN(EffectShards_Step), 0, false, kEf, 0, 0, 0, 0x30},
    {"EffectKind1D_DrawSpeck", 0x46E190, 0x69, R3E_CALLS(kCalls46E190), nullptr, 0, nullptr, 0, R3E_FN(EffectKind1D_DrawSpeck), 0, false, kCa, kRec0, 0, 0, 0x1D},
    {"EffectKind21_ArmPoints", 0x46F570, 0x11C, R3E_CALLS(kCalls46F570), nullptr, 0, nullptr, 0, R3E_FN(EffectKind21_ArmPoints), 0, false, kCa, kRec0, 0, 0, 0x21},
    {"EffectKind21_DrawArm", 0x46F690, 0x5A, R3E_CALLS(kCalls46F690), nullptr, 0, nullptr, 0, R3E_FN(EffectKind21_DrawArm), 0, false, kCa, kRec0, 0, 0, 0x21},
    {"EffectKind21_DrawArmTriangle", 0x46F6F0, 0x9B, R3E_CALLS(kCalls46F6F0), nullptr, 0, nullptr, 0, R3E_FN(EffectKind21_DrawArmTriangle), 0, false, kCa, kRec0, 0, 0, 0x21},
    {"EffectKind24_DrawRay", 0x46FAE0, 0x204, R3E_CALLS(kCalls46FAE0), nullptr, 0, nullptr, 0, R3E_FN(EffectKind24_DrawRay), 0, false, kCa, kRec0, 0, 0, 0x24},
    {"EffectGlowSparks_Clear", 0x4790C0, 0x21, nullptr, 0, nullptr, 0, nullptr, 0, R3E_FN(EffectGlowSparks_Clear), 0, false, kEf, 0, 0, 0, 0x49},
    {"EffectGlowSparks_StartRise", 0x4790F0, 0x68, R3E_CALLS(kCalls4790F0), nullptr, 0, nullptr, 0, R3E_FN(EffectGlowSparks_StartRise), 0, false, kCa, 0, 0, 0, 0x49},
    {"EffectGlowSparks_StartBurst", 0x479160, 0xF2, R3E_CALLS(kCalls479160), nullptr, 0, nullptr, 0, R3E_FN(EffectGlowSparks_StartBurst), 0, false, kCa, 0, 0, 0, 0x48},
    {"EffectGlowSparks_Run", 0x479260, 0x73, R3E_CALLS(kCalls479260), nullptr, 0, nullptr, 0, R3E_FN(EffectGlowSparks_Run), 0xFF, false, kEf, 0, 0, 0, 0x49},
    {"EffectGlowSparks_Draw", 0x4792E0, 0x13E, R3E_CALLS(kCalls4792E0), nullptr, 0, nullptr, 0, R3E_FN(EffectGlowSparks_Draw), 0, false, kCa, 0, 0, 0, 0x49},
    {"EffectGlowSparks_Glow", 0x479420, 0x43, R3E_CALLS(kCalls479420), nullptr, 0, nullptr, 0, R3E_FN(EffectGlowSparks_Glow), 0, false, kCa, 0, 0, 0, 0x49},
    {"EffectGlowSparks_Rise", 0x479470, 0x58, R3E_CALLS(kCalls479470), nullptr, 0, nullptr, 0, R3E_FN(EffectGlowSparks_Rise), 0, false, kCa, 0, 0, 0, 0x49},
    {"EffectGlowTrail_Update", 0x4794D0, 0x1D1, R3E_CALLS(kCalls4794D0), nullptr, 0, nullptr, 0, R3E_FN(EffectGlowTrail_Update), 0, false, kCa, 0, 0, 0, 0x49},
    {"EffectGlowTrail_Draw", 0x4796B0, 0x2BE, R3E_CALLS(kCalls4796B0), nullptr, 0, nullptr, 0, R3E_FN(EffectGlowTrail_Draw), 0, false, kCa, 0, 0, 0, 0x49},
    {"EffectSpiral_Init", 0x4799C0, 0x1A9, R3E_CALLS(kCalls4799C0), nullptr, 0, nullptr, 0, R3E_FN(EffectSpiral_Init), 0, false, kCa, 0, 0, 0, 0x48},
    {"EffectSpiral_StepDraw", 0x479B70, 0x370, R3E_CALLS(kCalls479B70), nullptr, 0, nullptr, 0, R3E_FN(EffectSpiral_StepDraw), 0, false, kCa, 0, 0, 0, 0x48},
    {"EffectRing_Draw", 0x479EE0, 0x229, R3E_CALLS(kCalls479EE0), nullptr, 0, nullptr, 0, R3E_FN(EffectRing_Draw), 0, false, kCa, kRec0, 0, 0, 0x48},
    {"EffectDust_Clear", 0x47A110, 0x14, nullptr, 0, nullptr, 0, nullptr, 0, R3E_FN(EffectDust_Clear), 0, false, kEf, 0, 0, 0, 0x49},
    {"EffectDust_FindFree", 0x47A130, 0x19, nullptr, 0, nullptr, 0, nullptr, 0, R3E_FN(EffectDust_FindFree), 0xFFFFFFFFu, false, kEf, 0, 0, 0, 0x49},
    {"EffectDust_Start", 0x47A150, 0xA1, R3E_CALLS(kCalls47A150), nullptr, 0, nullptr, 0, R3E_FN(EffectDust_Start), 0, false, kCa, 0, 0, 0, 0x49},
    {"EffectDust_Run", 0x47A200, 0xA7, R3E_CALLS(kCalls47A200), nullptr, 0, nullptr, 0, R3E_FN(EffectDust_Run), 0xFF, false, kEf, 0, 0, 0, 0x49},
    {"EffectDust_DrawColumn", 0x47A2B0, 0x114, R3E_CALLS(kCalls47A2B0), nullptr, 0, nullptr, 0, R3E_FN(EffectDust_DrawColumn), 0xFF, false, kCa, 0, 0, 0, 0x49},
    {"EffectDust_DrawFan", 0x47A3D0, 0x184, R3E_CALLS(kCalls47A3D0), nullptr, 0, nullptr, 0, R3E_FN(EffectDust_DrawFan), 0, false, kCa, 0, 0, 0, 0x49},
    {"EffectKind5D_Start", 0x47F2D0, 0x6D, R3E_CALLS(kCalls47F2D0), nullptr, 0, nullptr, 0, R3E_FN(EffectKind5D_Start), 0, false, kEf, 0, 0, 0, 0x5D},
    {"EffectKind5D_Open", 0x47F340, 0x9B, R3E_CALLS(kCalls47F340), nullptr, 0, nullptr, 0, R3E_FN(EffectKind5D_Open), 0, false, kEf, 0, 0, 0, 0x5D},
    {"EffectKind5D_Show", 0x47F3E0, 0x164, R3E_CALLS(kCalls47F3E0), nullptr, 0, nullptr, 0, R3E_FN(EffectKind5D_Show), 0, false, kEf, 0, 0, 0, 0x5D},
    {"EffectKind5D_Close", 0x47F550, 0x7D, R3E_CALLS(kCalls47F550), nullptr, 0, nullptr, 0, R3E_FN(EffectKind5D_Close), 0, false, kEf, 0, 0, 0, 0x5D},
    {"EffectKind5E_Start", 0x47F5F0, 0x19, R3E_CALLS(kCalls47F5F0), nullptr, 0, nullptr, 0, R3E_FN(EffectKind5E_Start), 0, false, kEf, 0, 0, 0, 0x5E},
    {"EffectKind5E_Pick", 0x47F610, 0x10A, R3E_CALLS(kCalls47F610), nullptr, 0, nullptr, 0, R3E_FN(EffectKind5E_Pick), 0, false, kEf, 0, 0, 0, 0x5E},
    {"EffectKind5E_Wait", 0x47F720, 0x76, R3E_CALLS(kCalls47F720), nullptr, 0, nullptr, 0, R3E_FN(EffectKind5E_Wait), 0, false, kEf, 0, 0, 0, 0x5E},
    {"EffectKind5D_DrawBoard", 0x47F7A0, 0x16A, R3E_CALLS(kCalls47F7A0), nullptr, 0, nullptr, 0, R3E_FN(EffectKind5D_DrawBoard), 0, false, kCa, 0, 0, 0, 0x5D},
    {"EffectKind5D_DrawMark", 0x47F910, 0xC6, R3E_CALLS(kCalls47F910), nullptr, 0, nullptr, 0, R3E_FN(EffectKind5D_DrawMark), 0, false, kCa, 0, 0, 0, 0x5D},
    {"EffectKind5E_DrawMenu", 0x47F9E0, 0x103, R3E_CALLS(kCalls47F9E0), nullptr, 0, nullptr, 0, R3E_FN(EffectKind5E_DrawMenu), 0, false, kEf, 0, 0, 0, 0x5E},
    {"EffectKind5E_DrawList", 0x47FAF0, 0xE2, R3E_CALLS(kCalls47FAF0), nullptr, 0, nullptr, 0, R3E_FN(EffectKind5E_DrawList), 0, false, kEf, 0, 0, 0, 0x5E},
    {"EffectKind5E_DrawPanel", 0x47FBE0, 0x193, R3E_CALLS(kCalls47FBE0), nullptr, 0, nullptr, 0, R3E_FN(EffectKind5E_DrawPanel), 0, false, kEf, 0, 0, 0, 0x5E},
    {"EffectKind5F_Launch", 0x47FDC0, 0x66, R3E_CALLS(kCalls47FDC0), nullptr, 0, nullptr, 0, R3E_FN(EffectKind5F_Launch), 0, false, kEf, 0, 0, 0, 0x5F},
    {"EffectKind5F_Hop", 0x47FE30, 0xA9, R3E_CALLS(kCalls47FE30), nullptr, 0, nullptr, 0, R3E_FN(EffectKind5F_Hop), 0, false, kEf, 0, 0, 0, 0x5F},
    {"EffectKind5F_Fly", 0x47FEE0, 0x74, R3E_CALLS(kCalls47FEE0), nullptr, 0, nullptr, 0, R3E_FN(EffectKind5F_Fly), 0, false, kEf, 0, 0, 0, 0x5F},
    {"EffectKind5F_Bounce", 0x47FF60, 0xA5, R3E_CALLS(kCalls47FF60), nullptr, 0, nullptr, 0, R3E_FN(EffectKind5F_Bounce), 0, false, kEf, 0, 0, 0, 0x5F},
    {"EffectKind5F_FlyAway", 0x480010, 0x6D, R3E_CALLS(kCalls480010), nullptr, 0, nullptr, 0, R3E_FN(EffectKind5F_FlyAway), 0, false, kEf, 0, 0, 0, 0x5F},
    {"EffectKind5F_PlaceHigh", 0x480080, 0x52, R3E_CALLS(kCalls480080), nullptr, 0, nullptr, 0, R3E_FN(EffectKind5F_PlaceHigh), 0, false, kEf, 0, 0, 0, 0x5F},
    {"EffectKind5F_Slide", 0x4800E0, 0x5E, R3E_CALLS(kCalls4800E0), nullptr, 0, nullptr, 0, R3E_FN(EffectKind5F_Slide), 0, false, kEf, 0, 0, 0, 0x5F},
    {"EffectKind5F_PlaceLow", 0x480140, 0x46, R3E_CALLS(kCalls480140), nullptr, 0, nullptr, 0, R3E_FN(EffectKind5F_PlaceLow), 0, false, kEf, 0, 0, 0, 0x5F},
    {"EffectKind5F_SlideOut", 0x480190, 0x5E, R3E_CALLS(kCalls480190), nullptr, 0, nullptr, 0, R3E_FN(EffectKind5F_SlideOut), 0, false, kEf, 0, 0, 0, 0x5F},
};
#undef R3E_FN
#undef R3E_CALLS
#undef R3E_N

enum : unsigned {
    k37Run, k17Run, k1BRun,
    kDrawNumber, kLoadModel, kShardsStep, kDrawSpeck, kArmPoints, kDrawArm, kArmTriangle, kDrawRay,
    kSparksClear, kStartRise, kStartBurst, kSparksRun, kSparkDraw, kSparkGlow, kSparkRise,
    kTrailUpdate, kTrailDraw, kSpiralInit, kSpiralStep, kRingDraw,
    kDustClear, kDustFind, kDustStart, kDustRun, kDustColumn, kDustFan,
    k5DStart, k5DOpen, k5DShow, k5DClose, k5EStart, k5EPick, k5EWait, k5DBoard, k5DMark, k5EMenu, k5EList, k5EPanel,
    k5FLaunch, k5FHop, k5FFly, k5FBounce, k5FFlyAway, k5FPlaceHigh, k5FSlide, k5FPlaceLow, k5FSlideOut,
    kCount
};
static_assert(kCount == sizeof kAll / sizeof kAll[0], "one enum entry a clone, in order");

// --- the stand-ins' effects (Noise() and the state only: both passes the same) ----------

// Crt_sprintf: one to seven characters - digits and spaces mostly, the number's
// draw skips a space - and a NUL, where the destination is ours to write (the
// standard row's FxSprintf can write none, which the number's loop does not
// end on: the real one always writes the number).
U FxSprintf(const U* a, U answer) {
    auto* const dst = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a[0]));
    if (!sh::InRegions(dst, 8)) return answer;
    const unsigned n = 1 + sh::Noise() % 7;
    for (unsigned i = 0; i < n; ++i) {
        const U r = sh::Noise();
        dst[i] = static_cast<unsigned char>(r % 4 == 0 ? 0x20 : '0' + (r >> 4) % 10);
    }
    dst[n] = 0;
    return n;
}
// Item_NamePtr: the harness's text buffer (the standard row's FxText).
U FxName(const U*, U answer) { return Key(sh::Text() + (answer & 0xF0)); }
// Inventory_Count: 0 a third of the time (the pick and the list test it), else
// small; garbage above the low word.
U FxCount(const U*, U answer) {
    const U n = sh::Noise();
    return (answer & 0xFFFF0000u) | (n % 3 == 0 ? 0u : 1u + (n >> 4) % 5);
}
// EffectGte_SetDiagonalOne: its 18 bytes filled, as the standard row; and the
// count of Gte_ApplyMatrixLV calls since started again - EffectKind21_ArmPoints
// calls it once before its first Gte_ApplyMatrixLV.
unsigned g_apply = 0;
U FxDiagonal(const U* a, U answer) {
    auto* const m = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a[0]));
    sh::FillBytes(m, 18);
    g_apply = 0;
    return answer;
}
// Gte_ApplyMatrixLV(matrix, vector, out): the matrix's nine shorts hashed; the
// vector's three longs hashed but on the first call after
// EffectGte_SetDiagonalOne, where the original hands a stack vector it never
// wrote (and drops the answer); the out filled.
U FxApplyLV(const U* a, U answer) {
    sh::NoteBytes(reinterpret_cast<const void*>(static_cast<std::uintptr_t>(a[0])), 18);
    if (g_apply++ != 0) sh::NoteBytes(reinterpret_cast<const void*>(static_cast<std::uintptr_t>(a[1])), 12);
    sh::FillBytes(reinterpret_cast<void*>(static_cast<std::uintptr_t>(a[2])), 12);
    return answer;
}
// Sound_PlayEffect (the standard row re-listed): a quarter of the time one of
// the two sound flags moved to 0 or 2..0xFF. EffectGlowSparks_Glow and _Rise
// set their flag after the sound; the group's case 3 alone moved it under that
// call in 0 and 3 rounds of 4000, too rarely to refuse the flag set before the
// sound (2026-10-05, round fourteen's review item 1).
U FxSoundFlags(const U*, U answer) {
    const U n = sh::Noise();
    if (n % 4 == 0)
        Mem((n & 4) ? at::kGlowSounded : at::kRiseSounded)[0] = static_cast<unsigned char>((n & 8) ? 0 : 2 + (n >> 8) % 0xFE);
    return answer;
}

// --- EffectGlowSparks_States' typed stand-ins -------------------------------------------
//
// EffectGlowSparks_Run calls the entry with the spark: each stand-in logs it
// under the handler's own address and moves the spark's shade, so the draw
// after it shows which spark it was handed.
constexpr U kSparkEntryAddress[EffectGlowSparks_States_count] = {bof3::addr::EffectGlowSparks_Glow, bof3::addr::EffectSpark_Wait,
                                                                  bof3::addr::EffectGlowSparks_Rise};
template <int I> void __cdecl SparkEntry(unsigned char* spark) {
    sh::Record(kSparkEntryAddress[I], Key(spark));
    sh::Stir();
    if (sh::InRegions(spark, at::kSparkStride)) spark[3] = static_cast<unsigned char>(spark[3] + 1 + I);
}
using SparkFn = void(__cdecl*)(unsigned char*);
const SparkFn kSparkEntries[EffectGlowSparks_States_count] = {&SparkEntry<0>, &SparkEntry<1>, &SparkEntry<2>};

#define R3E_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
constexpr sh::Answer kG = sh::Answer::kGarbage;
constexpr sh::Answer kP = sh::Answer::kPhase;
constexpr sh::Answer kF = sh::Answer::kFlag;
constexpr U kW = 0xFFFFFFFFu, k16 = 0xFFFFu, k8 = 0xFFu;
const sh::Callee kCallees[] = {
    // the group's own called directly, by name
    {R3E_OURS(EffectKind21_DrawArmTriangle), 4, {kW, kW, kW, kW}, kG, 0, 0},
    {R3E_OURS(EffectGlowSparks_Draw), 1, {kW}, kG, 0, 0, {0x18}, nullptr, nullptr, true},
    {R3E_OURS(EffectDust_DrawColumn), 1, {kW}, kF, 0, 0, {0x20}, nullptr, nullptr, true},
    {R3E_OURS(EffectDust_DrawFan), 1, {kW}, kG, 0, 0, {0x20}, nullptr, nullptr, true},
    {R3E_OURS(EffectKind5D_Start), 0, {}, kP, 0, 0},
    {R3E_OURS(EffectKind5E_DrawMenu), 0, {}, kP, 0, 0},
    {R3E_OURS(EffectKind5E_DrawList), 0, {}, kP, 0, 0},
    {R3E_OURS(EffectKind5E_DrawPanel), 0, {}, kP, 0, 0},
    // the board and the mark read the low words of their arguments (the callers
    // push whole registers: 0x47F3E0's y is `inc ax`)
    {R3E_OURS(EffectKind5D_DrawBoard), 4, {k16, k16, k16, k16}, kG, 0, 0},
    {R3E_OURS(EffectKind5D_DrawMark), 2, {k16, k16}, kG, 0, 0},
    // EffectGlowSparks_States' entries: the typed stand-ins' log slots, keyed on
    // each handler's address (no clone calls one by E8; the table holds them)
#define R3E_SPARK(i, name) {"EffectGlowSparks_States[" #i "] " #name, kSparkEntryAddress[i], kSparkEntryAddress[i], 1, {kW}, kG, 0, 0, \
                            {}, nullptr, reinterpret_cast<const void*>(kSparkEntries[i])}
    R3E_SPARK(0, Glow), R3E_SPARK(1, EffectSpark_Wait), R3E_SPARK(2, Rise),
#undef R3E_SPARK
    // FC2's kind 0x30 helpers, void (void) on Sprite_Current: not in the
    // standard set (their callers called them by name till now)
    {R3E_OURS(EffectKind30_ShardsInit), 0, {}, kP, 0, 0},
    {R3E_OURS(EffectKind30_SparksInit), 0, {}, kP, 0, 0},
    {R3E_OURS(EffectKind30_ShardsStep), 0, {}, kP, 0, 0},
    {R3E_OURS(EffectKind30_SparksDraw), 0, {}, kP, 0, 0},
    // E2F's trail cap (as E2F lists it: the point's 12 bytes, the size and angle
    // words, the shade byte - the last cap's shade is pushed as a dword whose
    // upper bytes the original never wrote); E2E's angle mean by address (the
    // trail update calls it so), each angle & 0xFFF
    {R3E_OURS(EffectTrail_DrawCap), 4, {kW, k16, k16, k8}, kG, 0, 0, {12, 0, 0, 0}, nullptr, nullptr, true},
    {"0x479970 EffectAngle_Mean", 0x479970, 0x479970, 2, {0xFFFu, 0xFFFu}, kG, 0, 0, {}, nullptr, nullptr, true},
    // R3F's, ours, keyed by address: (point, size, dy, last), the point
    // the record's +0x34 (the same address on both sides) and its 12 bytes
    {"0x480300 (R3F)", at::kR3FRing, at::kR3FRing, 4, {kW, kW, kW, kW}, kG, 0, 0, {12, 0, 0, 0}, nullptr, nullptr, true},
    // the arm's matrix calls (EffectKind21_ArmPoints): the matrix and the
    // vectors are stack locals - hashed, never logged by value
    {R3E_OURS(EffectGte_SetDiagonalOne), 1, {0}, kG, 0, 0, {}, &FxDiagonal, nullptr, true},
    {R3E_OURS(Gte_ApplyMatrixLV), 3, {0, 0, 0}, kG, 0, 0, {}, &FxApplyLV, nullptr, true},
    // standard rows re-listed at the width the callee reads: the windows read
    // x and y as signed words and w and h as bytes (window_task.cpp); the item
    // calls each argument's byte (R2E's reading)
    {R3E_OURS(Window_DrawFrame), 4, {k16, k16, k8, k8}, kG, 0, 0},
    {R3E_OURS(Window_DrawOutline), 4, {k16, k16, k8, k8}, kG, 0, 0},
    {R3E_OURS(Item_NamePtr), 2, {k8, k8}, kG, 0, 0, {}, &FxName, nullptr, true},
    {R3E_OURS(Inventory_Count), 3, {k8, k8, k8}, kG, 0, 0, {}, &FxCount, nullptr, true},
    {R3E_OURS(Inventory_Remove), 3, {k8, k8, k8}, kF, 0, 0},
    {"Crt_sprintf", 0x5B9380, KeyOf(&::Crt_sprintf), 3, {kW, kW, kW}, kG, 0, 0, {0, 16}, &FxSprintf, nullptr, true},
    // the standard row, louder: the two sound flags moved under it (FxSoundFlags)
    {R3E_OURS(Sound_PlayEffect), 1, {k16}, kG, 0, 0, {}, &FxSoundFlags},
};
#undef R3E_OURS

// Three kinds' state tables the dispatchers jump through, read in place (the
// counts symbols.toml gives, FC1's).
const sh::DataTable kTables[] = {
    {0x653F58, EffectKind37_States_count}, {0x653FBC, EffectKind17_States_count}, {0x653FD4, EffectKind1B_States_count},
};
const std::uint8_t kKinds[] = {0x17, 0x1B, 0x1D, 0x21, 0x24, 0x30, 0x37, 0x41, 0x48, 0x49, 0x5D, 0x5E, 0x5F};

// Beyond effect mode's standard regions: the sound flags and the pointers
// round them (0x6761C4..0x6761D3, E2D's); the spiral record past the shards'
// standard 0x644 and the dust records after it (0x92C5C4..0x92D9DC); the
// shards' model copy (24 faces of 0x28).
const sh::Region kRegions[] = {
    {0x6761C4, 0x10},
    {sh::at::kShards + sh::at::kShardsSize, at::kDust + at::kDustCount * at::kDustStride - (sh::at::kShards + sh::at::kShardsSize)},
    {at::kModelCopy, 24 * at::kModelFace},
};

// --- the seed ------------------------------------------------------------------------

unsigned g_k = 0;
unsigned char* Spark(unsigned i) { return EffectKind30_Shards + (i % at::kSparks) * at::kSparkStride; }
unsigned char* DustAt(unsigned i) { return Mem(at::kDust + (i % at::kDustCount) * at::kDustStride); }
constexpr U kTrail = 0x92C060;
constexpr U kSpiral = 0x92C4A4;

// A float at p: half the time a whole number of -0x80..0x27F with a fraction of
// 1/64ths (a screen coordinate), else raw bits.
void SeedFloat(unsigned char* p) {
    if (!sh::Half()) return;
    const U n = sh::Next();
    const float f = static_cast<float>(static_cast<int>(n % 0x300) - 0x80) + static_cast<float>((n >> 12) % 64) / 64.0f;
    std::memcpy(p, &f, 4);
}

void SeedSparks() {
    for (unsigned i = 0; i < at::kSparks; ++i) {
        unsigned char* const k = Spark(i);
        k[0] = static_cast<unsigned char>(sh::Half() ? 0 : PickOf(1, 0x80, sh::Next() | 1));
        k[1] = static_cast<unsigned char>(sh::Next() % EffectGlowSparks_States_count);
        if (sh::Half()) k[2] = static_cast<unsigned char>(PickOf(0, 1, 2));
    }
    Mem(at::kGlowSounded)[0] = static_cast<unsigned char>(PickOf(0, 0, 1, sh::Next()));
    Mem(at::kRiseSounded)[0] = static_cast<unsigned char>(PickOf(0, 0, 1, sh::Next()));
}
// The trail at 0x92C060: its screen floats, half the pairs equal or near.
void SeedTrail() {
    unsigned char* const t = Mem(kTrail);
    for (unsigned k = 0; k < 0x20; ++k) {
        unsigned char* const p = t + 0x20 * k + 0x10;
        SeedFloat(p);
        SeedFloat(p + 4);
        if (k > 0 && sh::Half()) {
            std::memcpy(p, p - 0x20, 8);
            if (sh::Half()) {
                float f;
                const unsigned w = 4 * (sh::Next() % 2);
                std::memcpy(&f, p + w, 4);
                f += 1.0f;
                std::memcpy(p + w, &f, 4);
            }
        }
    }
    for (unsigned k = 0; k < 0x1F; ++k)
        if (sh::Half()) SetWord(t + 0x400 + 2 * k, 0x1000);
}
void SeedDust() {
    for (unsigned i = 0; i < at::kDustCount; ++i) {
        unsigned char* const d = DustAt(i);
        d[0] = static_cast<unsigned char>(sh::Half() ? 0 : PickOf(1, 0x80, sh::Next() | 1));
        if (sh::Half()) d[2] = static_cast<unsigned char>(PickOf(0, 1, 2));
        // the bottom (height - spread << 8) about the ground
        if (sh::Half()) {
            const U spread = sh::Next() % 0x400;
            SetLong(d + 0x14, static_cast<std::int32_t>(spread));
            const U ground = static_cast<U>(Long(d + 0xC)) - (spread << 8) + PickOf(0, 1, 0xFFFFFFFFu, 0x10000, 0xFFFF0000u);
            SetLong(d + 0x1C, static_cast<std::int32_t>(ground));
        }
    }
}

// Kind 0x5D's windows: +9 so that n = +9 * 16 is at, below or past each
// window's sides, read from the layout words in place.
void SeedWindowStep(unsigned char* s) {
    const unsigned j = sh::Next() % 3;
    const unsigned side = Word(Mem(at::kWindows + 8 * j + 4 + 2 * (sh::Next() % 2)));
    const unsigned step = side >> 4;
    s[9] = static_cast<unsigned char>(PickOf(step, step + 1, step == 0 ? 0 : step - 1, 0, 1, 0x20, sh::Next()));
    if (sh::Half()) s[7] = static_cast<unsigned char>(sh::Next() & 7);
}
// Input_Pressed / Input_Held: nothing, one of the bits the states test, or any.
void SeedInput() {
    static const U kBits[] = {0, 0x10, 0x20, 0x40, 0x1000, 0x4000, 0x1010, 0x4020, 0x5000, 0x1040};
    SetWord(Mem(0x7E1BEC), sh::Half() ? kBits[sh::Next() % 10] : sh::Next());
    SetWord(Mem(0x7E1BE8), PickOf(0, 0, 1, sh::Next()));
}
// The counts at 0x903A10: 0..5 and about 100.
void SeedCounts() {
    for (unsigned i = 0; i < at::kItems; ++i)
        Mem(at::kCounts + i)[0] = static_cast<unsigned char>(PickOf(0, 1, 2, 3, 4, 5, 99, 100, 0xFF, sh::Next()));
}

void Seed(unsigned k) {
    g_k = k;
    unsigned char* const s = Sprite_Current;
    switch (k) {
    case kLoadModel: {
        // +0x54 at a count byte (-2..24 faces), +0x50 at the source, both in the
        // area block (a field region): the copy stays inside its region
        unsigned char* const count = Mem(sh::at::kAreaBlock + sh::Next() % 0x100);
        count[0] = static_cast<unsigned char>(static_cast<int>(sh::Next() % 27) - 2);
        SetLong(s + 0x54, static_cast<std::int32_t>(Key(count)));
        SetLong(s + 0x50, static_cast<std::int32_t>(sh::at::kAreaBlock + 0x100 + sh::Next() % 0x1000));
        break;
    }
    case kSparksClear: case kStartRise: case kStartBurst: case kSparksRun: case kSparkDraw: case kSparkGlow: case kSparkRise:
        SeedSparks();
        if (sh::Half()) s[6] = 0;
        break;
    case kTrailUpdate: case kTrailDraw: SeedTrail(); break;
    case kDustClear: case kDustFind: case kDustStart: case kDustRun: case kDustColumn: case kDustFan: SeedDust(); break;
    case k5DOpen: case k5DClose: SeedWindowStep(s); break;
    case k5DShow: case k5EWait: case k5EPick:
        if (sh::Half()) s[7] = static_cast<unsigned char>(sh::Next() & 7);
        s[0xA] = static_cast<unsigned char>(PickOf(0, 0, 1, sh::Next()));
        s[2] = static_cast<unsigned char>(PickOf(0, 7, sh::Next()));
        SeedInput();
        SeedCounts();
        break;
    case k5DBoard: case k5EMenu: SeedCounts(); break;
    case k5FLaunch: case k5FBounce: s[9] = static_cast<unsigned char>(PickOf(1, 1, 0, sh::Next())); break;
    case k5FHop: if (sh::Half()) SetLong(s + 0x34, 0x4A0000); break;
    case k5FFly: case k5FFlyAway:
        if (sh::Half()) {
            const U target = k == k5FFly ? 0x4B0000u : 0x360000u;
            const U by = sh::Next() % 0x40000;
            SetLong(s + 0x10, static_cast<std::int32_t>(by));
            SetLong(s + 0x38, static_cast<std::int32_t>(target - by));
        }
        break;
    case k5FSlide: if (sh::Half()) SetLong(s + 0x34, 0xF0000); break;
    case k5FSlideOut: if (sh::Half()) SetLong(s + 0x38, static_cast<std::int32_t>(PickOf(0x180000, 0x17FFFF, 0x190000, sh::Next()))); break;
    default: break;
    }
}

// The arguments: a spark, a dust record, the trail, the spiral; the arm
// triangle's points below 4 mostly; the number's clut a byte.
void Args(unsigned k, U* a) {
    switch (k) {
    case kStartRise: case kStartBurst: case kSparkDraw: case kSparkGlow: case kSparkRise: a[0] = Key(Spark(sh::Next())); break;
    case kDustStart: case kDustColumn: case kDustFan: a[0] = Key(DustAt(sh::Next())); break;
    case kTrailUpdate: case kTrailDraw: a[0] = kTrail; break;
    case kSpiralInit: case kSpiralStep: a[0] = kSpiral; break;
    case kArmTriangle:
        for (unsigned i = 1; i < 4; ++i) a[i] = sh::Often() ? sh::Next() % 4 : (a[i] & 0xFFFFFF00u) | (sh::Next() % 8);
        break;
    default: break;
    }
}

// What the states read again after a call, moved by the group's case of the
// harness's disturbance (from its hash only): +9, +0xA, +6, +2 of
// Sprite_Current, the sound flags, Input_Pressed.
void Disturb(U h) {
    const U v = h >> 8;
    unsigned char* const s = Sprite_Current;
    switch (sh::DisturbCase(h, 6)) {
    case 0: if (sh::InRegions(s, 0x80)) s[9] = static_cast<unsigned char>((v & 1) ? 1 : v >> 1); break;
    case 1: if (sh::InRegions(s, 0x80)) s[0xA] = static_cast<unsigned char>((v & 1) ? 0 : v >> 1); break;
    case 2: if (sh::InRegions(s, 0x80)) s[6] = static_cast<unsigned char>((v & 1) ? 0 : v >> 1); break;
    case 3: Mem((v & 1) ? at::kGlowSounded : at::kRiseSounded)[0] = static_cast<unsigned char>((v & 2) ? 0 : v >> 2); break;
    case 4: SetWord(Mem(0x7E1BEC), v & 0x5070u); break;
    case 5: if (sh::InRegions(s, 0x80)) s[2] = static_cast<unsigned char>(v); break;
    default: break;
    }
}

}  // namespace

void SelfTest() {
    // BOF3X_R3E_ONLY: the clones whose name contains it (a control's run)
    static sh::Clone chosen[kCount];
    static unsigned index[kCount];
    const char* const only = std::getenv("BOF3X_R3E_ONLY");
    unsigned n = 0;
    for (unsigned k = 0; k < kCount; ++k)
        if (!only || !*only || std::strstr(kAll[k].name, only)) {
            index[n] = k;
            chosen[n++] = kAll[k];
        }
    if (n == 0) bof3::Fatal("rest_3e: BOF3X_R3E_ONLY=%s names no clone", only);
    static unsigned* s_index = index;

    // EffectGlowSparks_States holds the typed stand-ins while the fuzz runs.
    unsigned char saved[4 * EffectGlowSparks_States_count];
    unsigned char* const table = move_script::At(Key(EffectGlowSparks_States));
    std::memcpy(saved, table, sizeof saved);
    for (unsigned i = 0; i < EffectGlowSparks_States_count; ++i) SetLong(table + 4 * i, static_cast<std::int32_t>(KeyOf(kSparkEntries[i])));

    sh::Group g = {"rest_3e", chosen, n, kCallees, sizeof kCallees / sizeof kCallees[0], kTables,
                   sizeof kTables / sizeof kTables[0], kRegions, sizeof kRegions / sizeof kRegions[0],
                   [](unsigned k) { Seed(s_index[k]); }, &Disturb, 4000};
    g.args = [](unsigned k, U* a) { Args(s_index[k], a); };
    g.effect = true;
    g.kinds = kKinds;
    g.n_kinds = sizeof kKinds;
    sh::Run(g);
    std::memcpy(table, saved, sizeof saved);
}

}  // namespace rest_3e

// BOF3X_SHADOW=area_w4b: world 4's areas 168..172 through the area round's
// shared harness (area_harness.h), once at start-up - one area_harness::Run
// per area, each Group setting its own area number, all under the one shadow
// name. docs/area_w4b.md section 3.
//
// The clone tables are tools/area_rows.py --clones's rows for AREA168..172
// (2026-09-28), each row read against the disassembly (every start, extent,
// call site and the five in-function jump tables agree); the shapes are the
// root table each function hangs from (docs/area_w4b.md section 1). The
// group's own callees (the rectangle searches, the slide step, the two draws)
// are recorders here like any other callee, so each function is fuzzed alone;
// area 172's state tables are swapped for recorders (DataTable).
#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w4b.h"
#include "game/area_w4b_callees.h"
#include "game/move_script_bytes.h"
#include "hook/log.h"

namespace area_w4b {
namespace {

namespace ah = area_harness;
using S = ah::Shape;
using U = std::uint32_t;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

#define AH_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define OURS(f) reinterpret_cast<const void*>(&::f)

// ---- area 168 ----
constexpr ah::CallSite kCalls426560[] = {{0x12, 0x57C7C0}};
constexpr ah::CallSite kCalls4265C0[] = {{0xB, 0x5918E0}};
constexpr ah::CallSite kCalls426610[] = {{0xB, 0x5918E0}};
constexpr ah::CallSite kCalls426660[] = {{0x1C, 0x57C0F0}, {0x24, 0x57C7C0}};
constexpr ah::CallSite kCalls4266A0[] = {{0x12, 0x57C7C0}};
constexpr ah::CallSite kCalls426700[] = {{0x48, 0x594E00}, {0x5F, 0x5734F0}, {0x7D, 0x587740}, {0x8C, 0x57C0F0}, {0xA5, 0x57C7A0}};
constexpr ah::JumpTable kTables426700[] = {{0x1B, 0xBC, 6}};
// ---- area 169 ----
constexpr ah::CallSite kCalls4267F0[] = {{0x10, 0x57C110}};
constexpr ah::CallSite kCalls426810[] = {{0x50, 0x4269F0}, {0x113, 0x589330}, {0x12D, 0x534610}, {0x136, 0x534710}, {0x18B, 0x52E140}, {0x198, 0x5893A0}};
constexpr ah::CallSite kCalls426A60[] = {{0x19, 0x495040}, {0x38, 0x587740}, {0x3D, 0x533E50}, {0x44, 0x495040}, {0x60, 0x4976D0}, {0x80, 0x57C7A0}, {0x8C, 0x57C0F0}};
constexpr ah::JumpTable kTables426A60[] = {{0x13, 0xA4, 4}};
constexpr ah::CallSite kCalls426B20[] = {{0x10, 0x57C140}, {0x1C, 0x57C7C0}};
// ---- area 170 ----
constexpr ah::CallSite kCalls426B60[] = {{0x12, 0x57C7C0}};
constexpr ah::CallSite kCalls426B90[] = {{0x12, 0x57C7C0}};
constexpr ah::CallSite kCalls426BC0[] = {{0x12, 0x57C7C0}};
constexpr ah::CallSite kCalls426C30[] = {{0x10, 0x486D60}, {0x2C, 0x587740}, {0x4B, 0x587740}};
constexpr ah::CallSite kCalls426C90[] = {
    {0x23, 0x531F90},  {0x49, 0x57C0F0},  {0x62, 0x594E00},  {0x76, 0x531F90},  {0x9C, 0x57C0F0},  {0xB5, 0x594E00},  {0xC9, 0x531F90},
    {0xEF, 0x57C0F0},  {0x108, 0x594E00}, {0x11C, 0x531F90}, {0x142, 0x57C0F0}, {0x15B, 0x594E00}, {0x16F, 0x531F90}, {0x195, 0x57C0F0},
    {0x1AE, 0x594E00}, {0x1C2, 0x531F90}, {0x1E8, 0x57C0F0}, {0x201, 0x594E00}, {0x230, 0x57C110}, {0x258, 0x57C110}, {0x264, 0x57C110},
    {0x270, 0x57C110}, {0x287, 0x589810}, {0x316, 0x589810}, {0x3F5, 0x589810}, {0x45F, 0x57C7A0}, {0x475, 0x4976D0}, {0x48E, 0x57C7A0},
    {0x495, 0x531F90}, {0x4F5, 0x57C7A0}, {0x50B, 0x57C7A0}};
constexpr ah::JumpTable kTables426C90[] = {{0x1D, 0x538, 26}};
constexpr ah::CallSite kCalls427270[] = {{0x72, 0x57C7C0}, {0x105, 0x57C7C0}, {0x148, 0x57C140}, {0x179, 0x57C7C0}, {0x1A4, 0x57C7C0}, {0x1BE, 0x57C0F0}};
constexpr ah::CallSite kCalls427470[] = {{0x0, 0x57C7C0}, {0x1A, 0x57C110}};
constexpr ah::CallSite kCalls4274A0[] = {{0x7, 0x57C0F0}};
constexpr ah::CallSite kCalls4274C0[] = {{0xE, 0x57C140},  {0x22, 0x590BB0}, {0x35, 0x57C0F0}, {0x3C, 0x5891F0},
                                         {0x43, 0x4976D0}, {0x4D, 0x587740}, {0x5D, 0x497710}, {0x67, 0x531F90}};
// ---- area 171 ----
constexpr ah::CallSite kCalls427540[] = {{0x12, 0x57C7C0}};
constexpr ah::CallSite kCalls427570[] = {{0x2D, 0x57C0F0}, {0x3B, 0x579F00}, {0x49, 0x579F00}, {0x54, 0x579F00}, {0x5F, 0x579F00}, {0x7C, 0x57C7C0}};
constexpr ah::CallSite kCalls427600[] = {{0x1, 0x589810}, {0x6E, 0x579F00}, {0x7C, 0x579F00}, {0x8A, 0x579F00}, {0x98, 0x579F00}};
constexpr ah::CallSite kCalls4276B0[] = {{0x10, 0x57C110}};
constexpr ah::CallSite kCalls4276D0[] = {{0x50, 0x4278B0}, {0x113, 0x589330}, {0x12D, 0x534610}, {0x136, 0x534710}, {0x18B, 0x52E140}, {0x198, 0x5893A0}};
constexpr ah::CallSite kCalls427920[] = {{0x2F, 0x495040}, {0x53, 0x587740}, {0x58, 0x533E50}, {0x5F, 0x495040}, {0x80, 0x4976D0}, {0xA5, 0x57C7A0},
                                         {0xB1, 0x57C0F0}, {0xD4, 0x57C7A0}, {0xDA, 0x531F90}, {0xF2, 0x4976D0}, {0x113, 0x57C7A0}};
constexpr ah::JumpTable kTables427920[] = {{0x1C, 0x128, 8}};
constexpr ah::CallSite kCalls427A80[] = {{0x14, 0x57C140}, {0x37, 0x536700}, {0x57, 0x536700}, {0x77, 0x536700}, {0x97, 0x536700}, {0xB6, 0x57C7C0}};
constexpr ah::CallSite kCalls427B50[] = {{0x10, 0x57C140}, {0x1C, 0x57C7C0}};
constexpr ah::CallSite kCalls427B90[] = {{0xB, 0x572650}, {0x15, 0x587740}};
// ---- area 172 ----
constexpr ah::CallSite kCalls427BB0[] = {{0x7, 0x57C110}};
constexpr ah::CallSite kCalls427CB0[] = {{0x10, 0x52E140}, {0x22, 0x572570}, {0x89, 0x5891F0}, {0x132, 0x5725F0}};
constexpr ah::CallSite kCalls427E10[] = {{0x3E, 0x427E60}};
constexpr ah::CallSite kCalls427FA0[] = {{0x1, 0x589810}};
constexpr ah::CallSite kCalls428090[] = {{0x12, 0x57C7C0}, {0x1E, 0x57C140}};
constexpr ah::CallSite kCalls4280E0[] = {{0x19, 0x531F90}, {0x39, 0x57C0F0}, {0x51, 0x531F90}, {0x71, 0x57C0F0}, {0x8A, 0x594E00}};
constexpr ah::JumpTable kTables4280E0[] = {{0x13, 0xA0, 7}};
constexpr ah::CallSite kCalls4281A0[] = {{0x31, 0x57C7C0}};
constexpr ah::CallSite kCalls4281F0[] = {{0x6, 0x579F00}};
constexpr ah::CallSite kCalls428240[] = {{0x33, 0x4282B0}, {0x47, 0x428370}};
constexpr ah::CallSite kCalls428290[] = {{0xA, 0x4282B0}};
constexpr ah::CallSite kCalls4282B0[] = {{0x8, 0x5A75D0}, {0x81, 0x5A79E0}, {0x98, 0x5A79A0}, {0xB0, 0x461E50}};
constexpr ah::CallSite kCalls428370[] = {{0xF, 0x5A79A0}, {0x27, 0x5A77C0}, {0x30, 0x461E50}, {0x3C, 0x5A7610}, {0xBC, 0x5A7780}, {0xC4, 0x5A77A0}, {0xCD, 0x461E50}};

const ah::Clone kClones168[] = {
    {"Area168_ChoiceFocusPair", 0x426560, 0x5F, kCalls426560, AH_N(kCalls426560), nullptr, 0, nullptr, 0, OURS(Area168_ChoiceFocusPair), 0, false, S::kChoice},
    {"Area168_ChoiceKeyItemC", 0x4265C0, 0x41, kCalls4265C0, AH_N(kCalls4265C0), nullptr, 0, nullptr, 0, OURS(Area168_ChoiceKeyItemC), 0, false, S::kChoice},
    {"Area168_ChoiceKeyItemD", 0x426610, 0x41, kCalls426610, AH_N(kCalls426610), nullptr, 0, nullptr, 0, OURS(Area168_ChoiceKeyItemD), 0, false, S::kChoice},
    {"Area168_ChoiceFlag8FArm59", 0x426660, 0x38, kCalls426660, AH_N(kCalls426660), nullptr, 0, nullptr, 0, OURS(Area168_ChoiceFlag8FArm59), 0, false, S::kChoice},
    {"Area168_ChoiceArm59At10", 0x4266A0, 0x26, kCalls4266A0, AH_N(kCalls4266A0), nullptr, 0, nullptr, 0, OURS(Area168_ChoiceArm59At10), 0, false, S::kChoice},
    {"Area168_ChoiceMessage12", 0x4266D0, 0x25, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area168_ChoiceMessage12), 0, false, S::kChoice},
    {"Area168_Tail59", 0x426700, 0xE1, kCalls426700, AH_N(kCalls426700), nullptr, 0, kTables426700, AH_N(kTables426700), OURS(Area168_Tail59), 0, false, S::kTail},
};
enum : unsigned { k168Focus, k168KeyC, k168KeyD, k168Flag8F, k168Arm10, k168Msg, k168Tail };
const ah::Clone kClones169[] = {
    {"Area169_InitClearFlag74", 0x4267F0, 0x19, kCalls4267F0, AH_N(kCalls4267F0), nullptr, 0, nullptr, 0, OURS(Area169_InitClearFlag74), 0, false, S::kInit},
    {"Area169_MembersFrame", 0x426810, 0x1D7, kCalls426810, AH_N(kCalls426810), nullptr, 0, nullptr, 0, OURS(Area169_MembersFrame), 0, false, S::kCallee},
    {"Area169_MemberRect", 0x4269F0, 0x6A, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area169_MemberRect), 0xFF, false, S::kCallee},
    {"Area169_TailHealParty", 0x426A60, 0xB4, kCalls426A60, AH_N(kCalls426A60), nullptr, 0, kTables426A60, AH_N(kTables426A60), OURS(Area169_TailHealParty), 0, false, S::kTail},
    {"Area169_ArriveHook", 0x426B20, 0x35, kCalls426B20, AH_N(kCalls426B20), nullptr, 0, nullptr, 0, OURS(Area169_ArriveHook), 0xFF, false, S::kHook},
};
enum : unsigned { k169Init, k169Frame, k169Rect, k169Tail, k169Arrive };
const ah::Clone kClones170[] = {
    {"Area170_ChoiceArm37At5", 0x426B60, 0x26, kCalls426B60, AH_N(kCalls426B60), nullptr, 0, nullptr, 0, OURS(Area170_ChoiceArm37At5), 0, false, S::kChoice},
    {"Area170_ChoiceArm37At15", 0x426B90, 0x26, kCalls426B90, AH_N(kCalls426B90), nullptr, 0, nullptr, 0, OURS(Area170_ChoiceArm37At15), 0, false, S::kChoice},
    {"Area170_ChoiceArm37At25", 0x426BC0, 0x26, kCalls426BC0, AH_N(kCalls426BC0), nullptr, 0, nullptr, 0, OURS(Area170_ChoiceArm37At25), 0, false, S::kChoice},
    {"Area170_ChoiceState52Or55", 0x426BF0, 0x1C, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area170_ChoiceState52Or55), 0, false, S::kChoice},
    {"Area170_ScriptFlagsSet1010", 0x426C10, 0xA, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area170_ScriptFlagsSet1010), 0, false, S::kHandler},
    {"Area170_ScriptFlagsClear1010", 0x426C20, 0xA, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area170_ScriptFlagsClear1010), 0, false, S::kHandler},
    {"Area170_Init", 0x426C30, 0x52, kCalls426C30, AH_N(kCalls426C30), nullptr, 0, nullptr, 0, OURS(Area170_Init), 0, false, S::kInit},
    {"Area170_Tail37", 0x426C90, 0x5DD, kCalls426C90, AH_N(kCalls426C90), nullptr, 0, kTables426C90, AH_N(kTables426C90), OURS(Area170_Tail37), 0, false, S::kTail},
    {"Area170_StepHook", 0x427270, 0x200, kCalls427270, AH_N(kCalls427270), nullptr, 0, nullptr, 0, OURS(Area170_StepHook), 0xFF, false, S::kHook},
    {"Area170_Trigger19", 0x427470, 0x25, kCalls427470, AH_N(kCalls427470), nullptr, 0, nullptr, 0, OURS(Area170_Trigger19), 0xFF, false, S::kCallee},
    {"Area170_Trigger20", 0x4274A0, 0x12, kCalls4274A0, AH_N(kCalls4274A0), nullptr, 0, nullptr, 0, OURS(Area170_Trigger20), 0xFF, false, S::kCallee},
    {"Area170_Trigger56", 0x4274C0, 0x80, kCalls4274C0, AH_N(kCalls4274C0), nullptr, 0, nullptr, 0, OURS(Area170_Trigger56), 0xFF, false, S::kCallee},
};
enum : unsigned { k170Arm5, k170Arm15, k170Arm25, k170State, k170Set, k170Clear, k170Init, k170Tail, k170Step, k170Trig19, k170Trig20, k170Trig56 };
const ah::Clone kClones171[] = {
    {"Area171_ChoiceArmTail10", 0x427540, 0x2D, kCalls427540, AH_N(kCalls427540), nullptr, 0, nullptr, 0, OURS(Area171_ChoiceArmTail10), 0, false, S::kChoice},
    {"Area171_Leader89Gate", 0x427570, 0x90, kCalls427570, AH_N(kCalls427570), nullptr, 0, nullptr, 0, OURS(Area171_Leader89Gate), 0, false, S::kHandler},
    {"Area171_SpawnEffect92", 0x427600, 0xA1, kCalls427600, AH_N(kCalls427600), nullptr, 0, nullptr, 0, OURS(Area171_SpawnEffect92), 0, false, S::kHandler},
    {"Area171_InitClearFlag74", 0x4276B0, 0x19, kCalls4276B0, AH_N(kCalls4276B0), nullptr, 0, nullptr, 0, OURS(Area171_InitClearFlag74), 0, false, S::kInit},
    {"Area171_MembersFrame", 0x4276D0, 0x1D7, kCalls4276D0, AH_N(kCalls4276D0), nullptr, 0, nullptr, 0, OURS(Area171_MembersFrame), 0, false, S::kCallee},
    {"Area171_MemberRect", 0x4278B0, 0x6A, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area171_MemberRect), 0xFF, false, S::kCallee},
    {"Area171_TailHealParty", 0x427920, 0x15E, kCalls427920, AH_N(kCalls427920), nullptr, 0, kTables427920, AH_N(kTables427920), OURS(Area171_TailHealParty), 0, false, S::kTail},
    {"Area171_StepHook", 0x427A80, 0xCC, kCalls427A80, AH_N(kCalls427A80), nullptr, 0, nullptr, 0, OURS(Area171_StepHook), 0xFF, false, S::kHook},
    {"Area171_ArriveHook", 0x427B50, 0x35, kCalls427B50, AH_N(kCalls427B50), nullptr, 0, nullptr, 0, OURS(Area171_ArriveHook), 0xFF, false, S::kHook},
    {"Area171_Trigger55", 0x427B90, 0x20, kCalls427B90, AH_N(kCalls427B90), nullptr, 0, nullptr, 0, OURS(Area171_Trigger55), 0xFF, false, S::kCallee},
};
enum : unsigned { k171Choice, k171Gate, k171Spawn, k171Init, k171Frame, k171Rect, k171Tail, k171Step, k171Arrive, k171Trig55 };
const ah::Clone kClones172[] = {
    {"Area172_ClearFlag4E", 0x427BB0, 0x10, kCalls427BB0, AH_N(kCalls427BB0), nullptr, 0, nullptr, 0, OURS(Area172_ClearFlag4E), 0, false, S::kHandler},
    {"Area172_SkipIfMember89Is4", 0x427BC0, 0x48, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area172_SkipIfMember89Is4), 0, false, S::kHandler},
    {"Area172_RunFall", 0x427C10, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area172_RunFall), 0, false, S::kHandler},
    {"Area172_FallStart", 0x427C30, 0x7D, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area172_FallStart), 0, false, S::kState},
    {"Area172_FallStep", 0x427CB0, 0x139, kCalls427CB0, AH_N(kCalls427CB0), nullptr, 0, nullptr, 0, OURS(Area172_FallStep), 0, false, S::kState},
    {"Area172_RunSlide", 0x427DF0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area172_RunSlide), 0, false, S::kHandler},
    {"Area172_SlideStart", 0x427E10, 0x4E, kCalls427E10, AH_N(kCalls427E10), nullptr, 0, nullptr, 0, OURS(Area172_SlideStart), 0, false, S::kState},
    {"Area172_SlideStep", 0x427E60, 0xB2, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area172_SlideStep), 0, false, S::kState},
    {"Area172_Drift", 0x427F20, 0x77, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area172_Drift), 0, false, S::kHandler},
    {"Area172_SpawnEffect92", 0x427FA0, 0x65, kCalls427FA0, AH_N(kCalls427FA0), nullptr, 0, nullptr, 0, OURS(Area172_SpawnEffect92), 0, false, S::kHandler},
    {"Area172_TintOn", 0x428010, 0x33, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area172_TintOn), 0, false, S::kHandler},
    {"Area172_TintOff", 0x428050, 0x33, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area172_TintOff), 0, false, S::kHandler},
    {"Area172_ChoiceFlag12", 0x428090, 0x48, kCalls428090, AH_N(kCalls428090), nullptr, 0, nullptr, 0, OURS(Area172_ChoiceFlag12), 0, false, S::kChoice},
    {"Area172_Tail35", 0x4280E0, 0xBC, kCalls4280E0, AH_N(kCalls4280E0), nullptr, 0, kTables4280E0, AH_N(kTables4280E0), OURS(Area172_Tail35), 0, false, S::kTail},
    {"Area172_StepHook", 0x4281A0, 0x4A, kCalls4281A0, AH_N(kCalls4281A0), nullptr, 0, nullptr, 0, OURS(Area172_StepHook), 0xFF, false, S::kHook},
    {"Area172_InitCell", 0x4281F0, 0xF, kCalls4281F0, AH_N(kCalls4281F0), nullptr, 0, nullptr, 0, OURS(Area172_InitCell), 0, false, S::kInit},
    {"Area172_EffectA5Run", 0x428200, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area172_EffectA5Run), 0, false, S::kCallee},
    {"Area172_EffectA5Start", 0x428220, 0x1F, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area172_EffectA5Start), 0, false, S::kState},
    {"Area172_EffectA5Grow", 0x428240, 0x50, kCalls428240, AH_N(kCalls428240), nullptr, 0, nullptr, 0, OURS(Area172_EffectA5Grow), 0, false, S::kState},
    {"Area172_EffectA5Hold", 0x428290, 0x13, kCalls428290, AH_N(kCalls428290), nullptr, 0, nullptr, 0, OURS(Area172_EffectA5Hold), 0, false, S::kState},
    {"Area172_DrawPanel", 0x4282B0, 0xBA, kCalls4282B0, AH_N(kCalls4282B0), nullptr, 0, nullptr, 0, OURS(Area172_DrawPanel), 0, false, S::kCallee},
    {"Area172_DrawShade", 0x428370, 0xD8, kCalls428370, AH_N(kCalls428370), nullptr, 0, nullptr, 0, OURS(Area172_DrawShade), 0, false, S::kCallee},
};
enum : unsigned {
    k172Clear4E, k172Skip, k172RunFall, k172FallStart, k172FallStep, k172RunSlide, k172SlideStart, k172SlideStep, k172Drift, k172Spawn,
    k172TintOn, k172TintOff, k172Choice, k172Tail, k172Step, k172Init, k172EffRun, k172EffStart, k172EffGrow, k172EffHold, k172Panel, k172Shade
};
#undef AH_N
#undef OURS

// Area 172's .data tables swapped for recorders: the fall table with the
// slide table it runs into (one swap of four entries covers both), and
// effect kind 0xA5's three states.
const ah::DataTable kTables172[] = {{at::kArea172FallStates, at::kArea172FallReach}, {at::kArea172EffectStates, at::kArea172EffectCount}};

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
unsigned char& B(U address) { return *ah::Mem(address); }
unsigned char* EffectRecord(unsigned slot) { return ah::Mem(at::kEffectObjects + slot % at::kEffectCount * at::kEffectStride); }

// ---- the fuzz's own memory ----

// The packet buffer Gfx_PacketNext points into while the fuzz runs (area
// 172's draws build 0xC + 0x44 or 0x48 bytes).
constexpr unsigned kPacketBytes = 0x400;
constexpr unsigned kPacketMargin = 0x80;
alignas(16) unsigned char g_packets[kPacketBytes];
void SetPacketNext(U v) { Gfx_PacketNext = g_packets + kPacketMargin + (v % 32) * 4; }
bool InPackets(U p, unsigned n) { return p >= Key(g_packets) && p + n <= Key(g_packets) + kPacketBytes; }

// Which area and function the round is running (set by the seeds).
int g_area = 0;
unsigned g_k = 0;
bool Running(int area, unsigned k) { return g_area == area && g_k == k; }

// ---- pointers the areas follow ----

// A record the active member pointer may name where the areas write through
// it (area 171's gate, area 172's slide): one of the four party objects
// (Sprite_ObjectsExtra), a field object, a party record, or the running
// object itself.
unsigned char* MemberRecord(U v) {
    switch (v % 4) {
    case 0: return ah::Mem(at::kSpriteObjectsExtra + (v >> 2) % 4 * at::kObjectStride);
    case 1: return ah::Object(v >> 2);
    case 2: return ah::PartyOf(static_cast<unsigned char>(v >> 2));
    default: return Sprite_Current;
    }
}
// A value of Field_ActiveMember where only its distance from Sprite_Objects
// is read (the spawns of kind 0x92): on a record or anywhere near one (the
// quotient truncates toward 0), below the array too.
U MemberValue(U v) {
    const U rec = (v >> 1) % 36;
    std::int32_t off = static_cast<std::int32_t>((v >> 7) % 0x149) - 0xA4;
    if ((v >> 17) % 3 == 0) off = 0;
    U base = at::kSpriteObjects + rec * at::kObjectStride;
    if (v & 1) base -= 3 * at::kObjectStride;
    return base + static_cast<U>(off);
}
unsigned char* ScriptRecord(U v) { return v & 1 ? ah::Object(v >> 1) : ah::PartyOf(static_cast<unsigned char>(v >> 1)); }
// The focus object: a field object, or one of Sprite_ObjectsExtra's four.
unsigned char* FocusRecord(U v) { return v % 5 == 0 ? ah::Mem(at::kSpriteObjectsExtra + (v >> 3) % 4 * at::kObjectStride) : ah::Object(v >> 3); }
bool WritesMember() { return Running(171, k171Gate) || Running(172, k172SlideStep); }
void SetMember(U v) {
    if (WritesMember() || (v & 0x10000)) ah::SetPointer(at::kActiveMember, MemberRecord(v));
    else SetLong(ah::Mem(at::kActiveMember), static_cast<std::int32_t>(MemberValue(v)));
}

// ---- the stand-ins the group lists ----

constexpr U kAll = 0xFFFFFFFFu, kU8 = 0xFFu, kU16 = 0xFFFFu;

// Louder than the real callees, on purpose (each only part of the time,
// from Noise): the callers read or write cells again after these calls.
void MoveCurrent(U n) { Sprite_Current = n & 0x100 ? ah::PartyOf(static_cast<unsigned char>(n >> 9)) : ah::Object((n >> 9) & 3); }
void MoveTail(U n) {
    if (n & 0x10) B(at::kTailState) = static_cast<unsigned char>(n >> 20);
    if (n & 0x20) B(at::kTailKind) = static_cast<unsigned char>(n >> 12);
}
U MovesCurrent(const U*, U answer) {
    const U n = ah::Noise();
    if (n & 1) MoveCurrent(n);
    return answer;
}
U MovesScriptObject(const U*, U answer) {
    const U n = ah::Noise();
    if (n & 1) ah::SetPointer(at::kScriptObject, ScriptRecord(n >> 8));
    if (n & 2) MoveCurrent(n >> 4);
    return answer;
}
// ScriptFlags_Set40: area 168's choice 1 reads the answer and the focus
// pointer after it; every armer writes the tail after it.
U Set40Effect(const U*, U answer) {
    const U n = ah::Noise();
    static const unsigned char kAnswers[] = {0, 1, 2, 3, 3, 0x80, 0xFF, 0x7F};
    if (n & 1) B(at::kChoiceAnswer) = kAnswers[(n >> 8) % 8];
    if (n & 2) ah::SetPointer(at::kFocusObject, FocusRecord(n >> 11));
    if (n & 4) B(at::kTailSub) = static_cast<unsigned char>(n >> 24);
    MoveTail(n);
    return answer;
}
// ScriptFlags_Clear40: the tails end (and area 168's clears counter 0) after it.
U Clear40Effect(const U*, U answer) {
    const U n = ah::Noise();
    if (n & 1) B(at::kCounter0) = static_cast<unsigned char>(n >> 8);
    MoveTail(n);
    return answer;
}
// The story-flag writers: the tails' state and the script flags are written
// after them (area 170's step hook and tail).
U FlagsEffect(const U*, U answer) {
    const U n = ah::Noise();
    if (n & 1) Field_ScriptFlags = static_cast<unsigned short>(n >> 8);
    MoveTail(n);
    return answer;
}
// Field_ChangeArea, Party_DropIn, Transition_Start, Msg_OpenScript: the tail
// state after (and Field_Request after the message); Party_DropIn also moves
// Sprite_Current (area 170's trigger 56 puts it back).
U TailEffect(const U*, U answer) {
    const U n = ah::Noise();
    if (n & 1) Field_Request = static_cast<unsigned char>(n & 2 ? 2 : n >> 8);
    if (n & 4) MoveCurrent(n >> 3);
    MoveTail(n);
    return answer;
}
// Effect_FindFree: a slot of the group's four or none (kByte); the active
// member, the message word, the camera yaw and the scratch cell read after it
// move now and then.
U FindFreeEffect(const U*, U answer) {
    const U n = ah::Noise();
    if (n & 1) SetMember(n >> 4);
    if (n & 2) SetWord(ah::Mem(at::kMessage), n >> 16);
    if (n & 4) SetWord(ah::Mem(at::kCameraYaw), n >> 12);
    if (n & 8) B(at::kScratch) = static_cast<unsigned char>(n >> 20);
    return answer;
}
// Sound_PlayEffect and the engine's 0x486D60: area 170's init reads the
// entry zone and Cond_ByteFD after them (and the tail kind before).
U ZoneEffect(const U*, U answer) {
    const U n = ah::Noise();
    static const unsigned char kZones[] = {2, 3, 2, 3, 1, 4, 0x82};
    if (n & 1) B(at::kEntryZone) = kZones[(n >> 8) % 7];
    if (n & 2) Cond_ByteFD = static_cast<unsigned char>(1 + (n >> 12) % 3);
    if (n & 4) MoveCurrent(n >> 3);
    if (n & 8) B(at::kTailKind) = static_cast<unsigned char>(n >> 20);
    return answer;
}
// AreaMap_SetByte: area 171's gate reads the leader's +0x89 and the active
// member again after its four.
U SetByteEffect(const U*, U answer) {
    const U n = ah::Noise();
    static const unsigned char k89[] = {5, 7, 7, 6, 0};
    if (n & 1) B(at::kLeader89) = k89[(n >> 8) % 5];
    if (n & 2) SetMember(n >> 4);
    return answer;
}
// AreaMap_ByteAt: half the time al 0x89 (area 171's step hook counts those);
// the scratch count it keeps in memory moves now and then.
U ByteAtEffect(const U*, U answer) {
    const U n = ah::Noise();
    if (n & 1) answer = (answer & 0xFFFFFF00u) | 0x89u;
    if ((n & 0x30) == 0x30) B(at::kScratch) = static_cast<unsigned char>(n >> 20);
    return answer;
}
// The rectangle searches: Field_State (read again when marking) and the
// flag words move now and then; Sprite_Current too.
U RectEffect(const U*, U answer) {
    const U n = ah::Noise();
    if (n & 1) ah::SetPointer(ah::at::kFieldState, ah::PartyOf(static_cast<unsigned char>(n >> 8)));
    if (n & 2) Field_ScriptFlags2 = static_cast<unsigned short>(n >> 12);
    if (n & 4) MoveCurrent(n >> 5);
    return answer;
}
// Sprite_ScriptTick: Field_ScriptFlags is or'd after it.
U TickEffect(const U*, U answer) {
    const U n = ah::Noise();
    if (n & 1) MoveCurrent(n);
    if (n & 2) Field_ScriptFlags = static_cast<unsigned short>(n >> 8);
    return answer;
}
// MapView_GroundAt: half the time the running object's word +0x3E or one
// either side (area 172's fall lands on the compare); Sprite_Current moved
// now and then (the fall reads it again after the call).
U GroundEffect(const U*, U answer) {
    const U n = ah::Noise();
    if (n & 1) MoveCurrent(n >> 4);
    if (n & 2) answer = (answer & 0xFFFF0000u) | ((Word(Sprite_Current + 0x3E) + (n >> 8) % 3 - 1) & 0xFFFFu);
    return answer;
}
// Sprite_SetAnimation: the script object (area 172's fall writes its +1
// after) and Sprite_Current.
U AnimationEffect(const U*, U answer) { return MovesScriptObject(nullptr, answer); }
// The link and commit stand-ins log the primitive at Gfx_PacketNext (every
// primitive of a draw is built at the pointer), then move it on by its size,
// kept in the buffer.
void Advance(U size) {
    size &= 0xFF;
    if (InPackets(Key(Gfx_PacketNext), size)) ah::NoteBytes(Gfx_PacketNext, size);
    const U next = Key(Gfx_PacketNext) + size;
    if (!InPackets(next, 0x80)) SetPacketNext(ah::Noise());
    else Gfx_PacketNext = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(next));
}
U CommitEffect(const U* a, U answer) {
    Advance(a[1]);
    return answer;
}
// The primitive setters write the primitive's bytes, so a store the caller
// makes before the call (where the original makes it after) shows.
void Scribble(U p, unsigned n) {
    if (!InPackets(p, n)) return;
    ah::FillBytes(ah::Mem(p), n);
}
U DrawModeEffect(const U* a, U answer) { Scribble(a[0], 0xC); return answer; }
U PolyG4Effect(const U* a, U) { Scribble(a[0], 0x44); return a[0]; }   // answers the primitive, as the callee
U PolyFT4Effect(const U* a, U answer) { Scribble(a[0], 0x48); return answer; }
U SemiEffect(const U* a, U answer) {
    if (InPackets(a[0], 8)) ah::Mem(a[0])[7] = static_cast<unsigned char>(a[1] ? ah::Mem(a[0])[7] | 2 : ah::Mem(a[0])[7] & 0xFD);
    return answer;
}
U ShadeEffect(const U* a, U answer) {
    if (InPackets(a[0], 8)) ah::Mem(a[0])[7] = static_cast<unsigned char>(a[1] ? ah::Mem(a[0])[7] | 1 : ah::Mem(a[0])[7] & 0xFE);
    return answer;
}

#define W4B_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
const ah::Callee kCallees[] = {
    {W4B_OURS(ScriptFlags_Set40), 0, {}, ah::Answer::kGarbage, 0, 0, {}, &Set40Effect},
    {W4B_OURS(ScriptFlags_Clear40), 0, {}, ah::Answer::kGarbage, 0, 0, {}, &Clear40Effect},
    {W4B_OURS(Flags_Set), 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &FlagsEffect},
    {W4B_OURS(Flags_Clear), 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &FlagsEffect},
    {W4B_OURS(Flags_Test), 2, {kAll, kAll}, ah::Answer::kFlag, 0, 0},
    {W4B_OURS(KeyItem_Has), 1, {kAll}, ah::Answer::kFlag, 0, 0},
    {W4B_OURS(Inventory_Add), 3, {kAll, kAll, kAll}, ah::Answer::kFlag, 0, 0},
    {W4B_OURS(Kind2_Place), 1, {kU8}, ah::Answer::kGarbage, 0, 0, {}, &TailEffect},
    {W4B_OURS(Field_ChangeArea), 4, {kAll, kAll, kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &TailEffect},
    {W4B_OURS(Party_DropIn), 1, {kAll}, ah::Answer::kGarbage, 0, 0, {}, &TailEffect},
    {W4B_OURS(Transition_Start), 1, {kU8}, ah::Answer::kGarbage, 0, 0, {}, &TailEffect},
    {W4B_OURS(Party_HealJoined), 0, {}, ah::Answer::kGarbage, 0, 0, {}, &TailEffect},
    {W4B_OURS(Msg_OpenScript), 1, {kU16}, ah::Answer::kGarbage, 0, 0, {}, &TailEffect},
    {W4B_OURS(Msg_OpenSystem), 1, {kAll}, ah::Answer::kGarbage, 0, 0, {}, &TailEffect},
    {W4B_OURS(Sound_PlayEffect), 1, {kU16}, ah::Answer::kGarbage, 0, 0, {}, &ZoneEffect},
    {"MapSetUp_486D60", at::kArea170MapSetUp, at::kArea170MapSetUp, 0, {}, ah::Answer::kGarbage, 0, 0, {}, &ZoneEffect},
    // slots inside the group's four effect records, or none
    {W4B_OURS(Effect_FindFree), 0, {}, ah::Answer::kByte, 0xFF, 0x03, {}, &FindFreeEffect},
    {W4B_OURS(Sprite_SetAnimation), 1, {kU8}, ah::Answer::kGarbage, 0, 0, {}, &AnimationEffect},
    {W4B_OURS(Sprite_EnsureAnimation), 1, {kU8}, ah::Answer::kFlag, 0, 0, {}, &MovesCurrent},
    {W4B_OURS(Field_JumpSetUp), 0, {}, ah::Answer::kGarbage, 0, 0, {}, &MovesCurrent},
    {W4B_OURS(Field_JumpCamera), 0, {}, ah::Answer::kGarbage, 0, 0, {}, &MovesCurrent},
    {W4B_OURS(Field_LeaderStepTick), 0, {}, ah::Answer::kFlag, 0, 0, {}, &MovesCurrent},
    {W4B_OURS(Sprite_ScriptTick), 0, {}, ah::Answer::kFlag, 0, 0, {}, &TickEffect},
    // the map: words x, z (and a value byte)
    {W4B_OURS(AreaMap_SetByte), 3, {kU16, kU16, kU8}, ah::Answer::kGarbage, 0, 0, {}, &SetByteEffect},
    {W4B_OURS(AreaMap_ByteAt), 2, {kU16, kU16}, ah::Answer::kGarbage, 0, 0, {}, &ByteAtEffect},
    {W4B_OURS(MoveCmd_TestFB), 2, {kU16, kU16}, ah::Answer::kFlag, 0, 0},
    {W4B_OURS(MapView_GroundAt), 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &GroundEffect},
    // the original pushes the height word with garbage above it; the callee
    // keeps the low word and a difference whose low 15 bits it uses
    {W4B_OURS(MapView_SetElevation), 1, {kU16}, ah::Answer::kGarbage, 0, 0},
    // the draws
    {W4B_OURS(Gpu_SetPolyFT4), 1, {kAll}, ah::Answer::kGarbage, 0, 0, {}, &PolyFT4Effect},
    {W4B_OURS(Gpu_GetClut), 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0},
    {W4B_OURS(Gpu_GetTPage), 4, {kAll, kAll, kAll, kAll}, ah::Answer::kGarbage, 0, 0},
    {W4B_OURS(Gfx_CommitPrim), 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &CommitEffect},
    {W4B_OURS(Gpu_SetDrawMode), 5, {kAll, kAll, kAll, kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &DrawModeEffect},
    {W4B_OURS(Gpu_SetPolyG4), 1, {kAll}, ah::Answer::kGarbage, 0, 0, {}, &PolyG4Effect},
    {W4B_OURS(Gpu_SetSemiTrans), 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &SemiEffect},
    {W4B_OURS(Gpu_SetShadeTex), 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &ShadeEffect},
    // the group's own, called directly: the rectangle searches (the member a
    // byte; 0xFF, 0 or 1), the slide step, the two draws (the low words)
    {W4B_OURS(Area169_MemberRect), 1, {kU8}, ah::Answer::kByte, 0xFF, 0x01, {}, &RectEffect},
    {W4B_OURS(Area171_MemberRect), 1, {kU8}, ah::Answer::kByte, 0xFF, 0x01, {}, &RectEffect},
    {W4B_OURS(Area172_SlideStep), 0, {}, ah::Answer::kGarbage, 0, 0, {}, &MovesCurrent},
    {W4B_OURS(Area172_DrawPanel), 2, {kU16, kU16}, ah::Answer::kGarbage, 0, 0, {}, &MovesCurrent},
    {W4B_OURS(Area172_DrawShade), 2, {kU16, kU16}, ah::Answer::kGarbage, 0, 0},
};
#undef W4B_OURS

// Beyond the field frame: MoveScript_TintRecords through Sprite_Kind2 to all
// twenty effect records (one run of 0x7E0700..0x7E1BE0: the tint index is a
// byte, Sprite_Kind2 +0x3C and Effect_Objects lie inside it), the held input,
// the packet pointer and buffer, the active member, script object and focus
// pointers, the gate byte, the DA wait word, Field_Kind2Hold, Camera_Angles,
// Field_MoveSpeeds, Cond_ByteFE.
ah::Region g_regions[] = {
    {at::kTintRecords, 0x7E1BE0 - at::kTintRecords},
    {at::kInputHeld, 4},
    {at::kPacketNext, 4},
    {0, kPacketBytes},   // g_packets, placed at SelfTest
    {at::kActiveMember, 4},
    {at::kScriptObject, 4},
    {at::kFocusObject, 4},
    {at::kGateKey, 1},
    {at::kWaitWord, 2},
    {at::kKind2Hold, 1},
    {at::kCameraAngles, 6},
    {at::kMoveSpeeds, 4},
    {at::kCondByteFE, 1},
};

// Every round: the pointers the areas follow put back inside the regions.
void Common(int area, unsigned k) {
    g_area = area;
    g_k = k;
    SetMember(ah::Next());
    ah::SetPointer(at::kScriptObject, ScriptRecord(ah::Next()));
    ah::SetPointer(at::kFocusObject, FocusRecord(ah::Next()));
    SetPacketNext(ah::Next());
}

// The group's cells, moved by the harness's disturbance about one call in
// 24 - drawn only from h (area_harness.h: a group disturb never draws Next).
void Disturb(U h) {
    const auto v = static_cast<unsigned char>(h >> 20);
    static const unsigned char kCounts[] = {0, 0x14, 0x24, 0x44, 0x64};
    static const unsigned char k89[] = {5, 7, 4};
    switch ((h >> 8) % 14) {
    case 0: B(at::kTailState) = static_cast<unsigned char>(h & 0x100 ? v % 62 : v); break;
    case 1: B(at::kCounter0) = static_cast<unsigned char>(h & 0x100 ? v % 3 : v); break;
    case 2: B(at::kCounter3) = h & 0x100 ? kCounts[v % 5] : v; break;
    case 3: SetMember(h >> 12); break;
    case 4: ah::SetPointer(at::kScriptObject, ScriptRecord(h >> 16)); break;
    case 5: ah::SetPointer(at::kFocusObject, FocusRecord(h >> 16)); break;
    case 6: SetWord(ah::Mem(at::kTailTimer), h & 0x100 ? v % 3 : v); break;
    case 7: SetWord(ah::Mem(at::kWaitWord), h & 0x100 ? 0 : v); break;
    case 8: B(at::kKind2Hold) = static_cast<unsigned char>(h & 0x100 ? 0 : v); break;
    case 9: B(at::kEntryZone) = static_cast<unsigned char>(h & 0x100 ? 2 + v % 2 : v); break;
    case 10: B(at::kScratch) = static_cast<unsigned char>(h & 0x100 ? v % 2 : v); break;
    case 11: B(at::kLeader89) = h & 0x100 ? k89[v % 3] : v; break;
    case 12: B(at::kTailKind) = static_cast<unsigned char>(h & 0x100 ? 0x33 : v); break;
    default: B(at::kChoiceAnswer) = static_cast<unsigned char>(v % 5); break;
    }
}

// A choice answer: each value a handler tests, its neighbours, a negative
// byte (area 168's choice 1 indexes by it signed), anything.
void SeedAnswer() {
    if (ah::Often()) B(at::kChoiceAnswer) = static_cast<unsigned char>(AH_PICK(0, 0, 1, 2, 3, 3, 4, 0xFF, 0x80, 0x81, 0x7F));
}
// A tail state from `states`, or anything; answered as a signed state.
int SeedState(const std::uint32_t* states, unsigned n) {
    B(at::kTailState) = static_cast<unsigned char>(ah::Often() ? ah::Pick(states, n) : ah::Next());
    return static_cast<signed char>(B(at::kTailState));
}
// A byte a state waits on: its value two times in three (`on`), else one
// either side of it or anything.
unsigned char Waited(unsigned char on) {
    if (ah::Often()) return on;
    switch (ah::Next() % 3) {
    case 0: return static_cast<unsigned char>(on + 1);
    case 1: return static_cast<unsigned char>(on - 1);
    default: return static_cast<unsigned char>(ah::Next());
    }
}
// A word timer on or beside `on`.
unsigned WaitedWord(unsigned on) {
    if (ah::Often()) return on;
    return ah::Half() ? on + (ah::Half() ? 1u : 0xFFFFu) : ah::Next() & 0xFFFF;
}
// Field_Request 2 or not, half and half.
void SeedRequest() { Field_Request = static_cast<unsigned char>(ah::Half() ? 2 : AH_PICK(0, 1, 3, 5, 0x82)); }
// The DA wait word 0 two times in three.
void SeedWait() { SetWord(ah::Mem(at::kWaitWord), ah::Often() ? 0 : AH_PICK(1, 0x100, 0xFFFF)); }
// A 16.16 word with the high word `high` and any low word.
U At16(U high, U low) { return (high & 0xFFFF) << 16 | (low & 0xFFFF); }
// A high word on or beside [lo, lo + 3): each inside, one either side, a
// high byte above it (the compares are 16-bit), anything.
U Around3(U lo) {
    switch (ah::Next() % 5) {
    case 0: case 1: return lo + ah::Next() % 3;
    case 2: return ah::Half() ? lo - 1 : lo + 3;
    case 3: return (lo + ah::Next() % 3) | 0x100u;
    default: return ah::Next();
    }
}
// An exact 16.16 value, one either side of it, or anything.
U Exact(U v) {
    switch (ah::Next() % 6) {
    case 0: case 1: case 2: return v;
    case 3: return v + 1;
    case 4: return v - 1;
    default: return v ^ 0x10000u;
    }
}

// The party records' positions around the rectangles of `rects` (their edges
// << 16 and one either side), the records' +8 on a facing or not, +9 0 or
// not; the caller's marks and the flag words' low bits.
void SeedMembers(U rects) {
    for (unsigned m = 0; m < 3; ++m) {
        unsigned char* const record = ah::PartyOf(static_cast<unsigned char>(m));
        const unsigned char* const r = ah::Mem(rects + (ah::Next() % 2) * at::kRectStride);
        if (ah::Often()) {
            const U edge = static_cast<U>(ah::Half() ? r[0] : r[2]) << 16;
            const U mid = (static_cast<U>(r[0] + r[2]) << 15) + (ah::Next() & 0xFFFF);
            SetLong(record + 0x34, static_cast<std::int32_t>(ah::Half() ? mid : edge + ah::Next() % 3 - 1));
        }
        if (ah::Often()) {
            const U edge = static_cast<U>(ah::Half() ? r[1] : r[3]) << 16;
            const U mid = (static_cast<U>(r[1] + r[3]) << 15) + (ah::Next() & 0xFFFF);
            SetLong(record + 0x38, static_cast<std::int32_t>(ah::Half() ? mid : edge + ah::Next() % 3 - 1));
        }
        if (ah::Half()) record[8] = r[4];
        if (ah::Half()) record[9] = 0;
    }
    if (ah::Half()) Sprite_Current[0xB] = static_cast<unsigned char>(ah::Next() & 7);
    if (ah::Often()) Field_ScriptFlags2 = static_cast<unsigned short>(ah::Next() & (ah::Half() ? 7 : 0xFFFF));
    if (ah::Often()) Field_ScriptFlags = static_cast<unsigned short>(ah::Next() & (ah::Half() ? 0x2007 : 0xFFFF));
    if (ah::Half()) Field_Request = static_cast<unsigned char>(AH_PICK(5, 5, 4, 6, 0));
}
// The rectangle search's member: 0..2 with garbage above the byte, or a record
// seeded as above.
void ArgsRect(std::uint32_t* a) { a[0] = (ah::Next() % 3) | (ah::Half() ? ah::Next() & 0xFFFFFF00u : 0); }
// An object trigger is called (a field object, 0x904030).
void ArgsTrigger(std::uint32_t* a) {
    a[0] = Key(ah::Object(a[0]));
    a[1] = at::kStoryFlags;
}

// ---- area 168 ----
void Seed168(unsigned k) {
    Common(168, k);
    switch (k) {
    case k168Focus: case k168KeyC: case k168KeyD: case k168Flag8F: case k168Arm10: case k168Msg: SeedAnswer(); break;
    case k168Tail: {
        static const std::uint32_t kStates[] = {0, 1, 10, 11, 12, 0, 1, 10, 11, 12, 2, 9, 13, 0xFF, 0x80, 0x8C};
        const int state = SeedState(kStates, sizeof kStates / sizeof kStates[0]);
        SeedRequest();
        if (state == 11) B(at::kCounter0) = Waited(1);
        else if (state == 12) B(at::kCounter0) = Waited(2);
        else if (ah::Half()) B(at::kCounter0) = static_cast<unsigned char>(AH_PICK(1, 2, 0));
        break;
    }
    default: break;
    }
}

// ---- area 169 ----
void Seed169(unsigned k) {
    Common(169, k);
    switch (k) {
    case k169Init: case k169Arrive:
        if (ah::Often()) Cond_ByteFD = static_cast<unsigned char>(AH_PICK(1, 1, 0, 2, 3, 0x81));
        break;
    case k169Frame: case k169Rect: SeedMembers(at::kRects169.rects); break;
    case k169Tail: {
        static const std::uint32_t kStates[] = {0, 1, 2, 3, 0, 1, 2, 3, 4, 0xFF, 0x80, 0x83};
        SeedState(kStates, sizeof kStates / sizeof kStates[0]);
        SeedWait();
        SeedRequest();
        break;
    }
    default: break;
    }
}
void Args169(unsigned k, std::uint32_t* a) {
    if (k == k169Rect) ArgsRect(a);
}

// ---- area 170 ----
void Seed170(unsigned k) {
    Common(170, k);
    switch (k) {
    case k170Arm5: case k170Arm15: case k170Arm25: case k170State: SeedAnswer(); break;
    case k170Init:
        if (ah::Often()) B(at::kTailKind) = static_cast<unsigned char>(AH_PICK(0x33, 0x33, 0x25, 0x32, 0x34, 0xB3));
        if (ah::Often()) B(at::kEntryZone) = static_cast<unsigned char>(AH_PICK(2, 3, 2, 3, 1, 4, 0x82));
        if (ah::Often()) Cond_ByteFD = static_cast<unsigned char>(AH_PICK(1, 2, 1, 2, 0, 3, 0x81));
        break;
    case k170Tail: {
        static const std::uint32_t kStates[] = {0,  1,  5,  6,  10, 11, 15, 16, 20, 21, 25, 26, 29, 30, 40, 41, 42, 43, 44, 45,
                                                50, 52, 53, 55, 60, 2,  31, 51, 54, 59, 61, 62, 0xFF, 0x80, 0xBC};
        const int state = SeedState(kStates, sizeof kStates / sizeof kStates[0]);
        // the counter each wait wants: 0x24 for 1 / 6, 0x44 for 11 / 16, 0x64
        // for 21 / 26, 0 for 29 / 30
        switch (state) {
        case 1: case 6: B(at::kCounter3) = Waited(0x24); break;
        case 11: case 16: B(at::kCounter3) = Waited(0x44); break;
        case 21: case 26: B(at::kCounter3) = Waited(0x64); break;
        case 29: case 30: B(at::kCounter3) = Waited(0); break;
        default: B(at::kCounter3) = static_cast<unsigned char>(AH_PICK(0x24, 0x44, 0x64, 0, 1)); break;
        }
        // the timer: 1 or 0xFF (state 41), 1 (43 / 44 count down to 0)
        if (state == 41) SetWord(ah::Mem(at::kTailTimer), ah::Half() ? WaitedWord(1) : WaitedWord(0xFF));
        else if (state == 43 || state == 44) SetWord(ah::Mem(at::kTailTimer), WaitedWord(1));
        else SetWord(ah::Mem(at::kTailTimer), AH_PICK(1, 0xFF, 0, 0x1E));
        B(at::kKind2Hold) = static_cast<unsigned char>(ah::Often() ? 0 : ah::Next() | 1);
        if (state == 53) Field_Request = static_cast<unsigned char>(ah::Often() ? 0 : AH_PICK(1, 2, 0x80));
        else SeedRequest();
        break;
    }
    case k170Step:
        if (ah::Often()) Cond_ByteFD = static_cast<unsigned char>(AH_PICK(4, 5, 4, 5, 3, 6, 0x84));
        if (ah::Often()) B(at::kLeaderPose) = static_cast<unsigned char>(AH_PICK(7, 3, 7, 3, 2, 6, 0x87));
        break;
    default: break;
    }
}
// Area 170's step hook's (x, z): one of its eight lines exact (or beside) with
// the other coordinate's high word in or beside its window; the pose squares
// (0x208000 / 0x218000, 0x900000); anything.
void Args170(unsigned k, std::uint32_t* a) {
    if (k == k170Trig19 || k == k170Trig20 || k == k170Trig56) {
        ArgsTrigger(a);
        return;
    }
    if (k != k170Step || !ah::Often()) return;
    U x = a[0], z = a[1];
    switch (ah::Next() % 7) {
    case 0: z = Exact(AH_PICK(0x708000, 0x738000)); x = At16(Around3(3), x); break;
    case 1: x = Exact(AH_PICK(0x28000, 0x58000)); z = At16(Around3(0x71), z); break;
    case 2: z = Exact(AH_PICK(0x8E8000, 0x918000)); x = At16(Around3(ah::Half() ? 9 : 0x10), x); break;
    case 3: x = Exact(AH_PICK(0x88000, 0xB8000, 0xF8000, 0x128000)); z = At16(Around3(0x8F), z); break;
    case 4: x = Exact(AH_PICK(0x208000, 0x218000)); z = Exact(0x900000); break;
    case 5: x = AH_PICK(0x208000, 0x218000); z = 0x900000; break;
    default: break;
    }
    a[0] = x;
    a[1] = z;
}

// ---- area 171 ----
void Seed171(unsigned k) {
    Common(171, k);
    switch (k) {
    case k171Choice: SeedAnswer(); break;
    case k171Gate:
        if (ah::Often()) B(at::kLeader89) = static_cast<unsigned char>(AH_PICK(5, 7, 5, 7, 4, 6, 0x85));
        if (ah::Often()) B(at::kGateKey) = static_cast<unsigned char>(AH_PICK(0x4B, 0x4B, 0x4A, 0x4C, 0xCB));
        break;
    case k171Init: case k171Arrive:
        if (ah::Often()) Cond_ByteFD = static_cast<unsigned char>(AH_PICK(3, 3, 2, 4, 0x83));
        break;
    case k171Frame: case k171Rect: SeedMembers(at::kRects171.rects); break;
    case k171Tail: {
        static const std::uint32_t kStates[] = {0, 1, 2, 3, 10, 20, 21, 0, 1, 2, 3, 10, 20, 21, 4, 9, 11, 19, 22, 0xFF, 0x80, 0x95};
        SeedState(kStates, sizeof kStates / sizeof kStates[0]);
        Cond_ByteFD = Waited(3);
        SeedWait();
        SeedRequest();
        B(at::kLeader137) = Waited(0);
        break;
    }
    case k171Step:
        if (ah::Often()) Cond_ByteFD = static_cast<unsigned char>(AH_PICK(2, 2, 1, 3, 0x82));
        break;
    default: break;
    }
}
// Area 171's step hook's (x, z): each fraction 0 or not.
void Args171(unsigned k, std::uint32_t* a) {
    if (k == k171Rect) {
        ArgsRect(a);
    } else if (k == k171Trig55) {
        ArgsTrigger(a);
    } else if (k == k171Step) {
        if (ah::Half()) a[0] &= 0xFFFF0000u;
        if (ah::Half()) a[1] &= 0xFFFF0000u;
        if (ah::Half()) a[0] |= 0xFFFFu;
        if (ah::Half()) a[1] |= 0xFFFFu;
    }
}

// ---- area 172 ----
void Seed172(unsigned k) {
    Common(172, k);
    switch (k) {
    case k172Skip:
        for (unsigned m = 0; m < 3; ++m)
            if (ah::Half()) ah::PartyOf(static_cast<unsigned char>(m))[0x89] = static_cast<unsigned char>(AH_PICK(4, 4, 3, 5, 0x84));
        if (ah::Next() % 8 == 0) Field_MemberCount = 0;
        break;
    case k172RunFall: Sprite_Current[4] = static_cast<unsigned char>(ah::Next() % at::kArea172FallReach); break;
    case k172RunSlide: Sprite_Current[4] = static_cast<unsigned char>(ah::Next() % at::kArea172SlideCount); break;
    case k172FallStep:
        if (ah::Often()) Sprite_Current[0x5D] = static_cast<unsigned char>(AH_PICK(0x40, 0x40, 0x3C, 0x44, 0xC0));
        break;
    case k172SlideStep:
        // each tint byte at 0xC0 or on either side of the signed bound
        for (unsigned f = 0x5D; f <= 0x5F; ++f)
            if (ah::Often()) Sprite_Current[f] = static_cast<unsigned char>(AH_PICK(0xC0, 0xC0, 0xBE, 0xBF, 0xC1, 0x80, 0x7F, 0));
        break;
    case k172Drift:
        if (ah::Often()) B(at::kCounter0) = static_cast<unsigned char>(AH_PICK(6, 7, 0, 8, 0x80, 0xFF));
        break;
    case k172Choice: SeedAnswer(); break;
    case k172Tail: {
        static const std::uint32_t kStates[] = {0, 1, 5, 6, 0, 1, 5, 6, 2, 3, 4, 7, 0xFF, 0x80};
        SeedState(kStates, sizeof kStates / sizeof kStates[0]);
        B(at::kCounter3) = Waited(0x14);
        break;
    }
    case k172Step:
        if (ah::Often()) Cond_ByteFD = static_cast<unsigned char>(AH_PICK(0, 0, 1, 0x80));
        if (ah::Often()) B(at::kLeaderPose) = static_cast<unsigned char>(AH_PICK(6, 7, 0, 5, 8, 1, 0x86));
        break;
    case k172EffRun:
        Sprite_Current = EffectRecord(ah::Next() % 4);
        Sprite_Current[1] = static_cast<unsigned char>(ah::Next() % at::kArea172EffectCount);
        break;
    case k172EffStart: case k172EffHold: Sprite_Current = EffectRecord(ah::Next() % 4); break;
    case k172EffGrow:
        Sprite_Current = EffectRecord(ah::Next() % 4);
        if (ah::Often()) SetWord(Sprite_Current + 0x2E, AH_PICK(0x186, 0x187, 0x188, 0x190, 0x17C, 0x7FF8, 0xFFF6, 0));
        break;
    default: break;
    }
}
// Area 172's step hook's (x, z): x exact 0x248000 or beside, z's high word
// on or beside 0x35..0x37.
void Args172(unsigned k, std::uint32_t* a) {
    if (k == k172Step && ah::Often()) {
        a[0] = Exact(0x248000);
        a[1] = At16(Around3(0x35), a[1]);
    }
}

void RunArea(int area, const ah::Clone* clones, unsigned n, const ah::DataTable* tables, unsigned n_tables, void (*seed)(unsigned),
             void (*args)(unsigned, std::uint32_t*), unsigned rounds) {
    ah::Group g{"area_w4b", clones, n, kCallees, sizeof kCallees / sizeof kCallees[0], tables, n_tables,
                g_regions, sizeof g_regions / sizeof g_regions[0], seed, &Disturb, rounds};
    g.args = args;
    g.area = area;
    ah::Run(g);
}

}  // namespace

void SelfTest() {
    constexpr unsigned kRounds = 6000;
    for (ah::Region& r : g_regions)
        if (r.at == 0) r.at = Key(g_packets);
    RunArea(168, kClones168, sizeof kClones168 / sizeof kClones168[0], nullptr, 0, &Seed168, nullptr, kRounds);
    RunArea(169, kClones169, sizeof kClones169 / sizeof kClones169[0], nullptr, 0, &Seed169, &Args169, kRounds);
    RunArea(170, kClones170, sizeof kClones170 / sizeof kClones170[0], nullptr, 0, &Seed170, &Args170, kRounds);
    RunArea(171, kClones171, sizeof kClones171 / sizeof kClones171[0], nullptr, 0, &Seed171, &Args171, kRounds);
    RunArea(172, kClones172, sizeof kClones172 / sizeof kClones172[0], kTables172, sizeof kTables172 / sizeof kTables172[0], &Seed172,
            &Args172, kRounds);
}

}  // namespace area_w4b

// BOF3X_SHADOW=area_w3f: world 3's areas 143..146 through the area round's
// shared harness (area_harness.h), once at start-up - one area_harness::Run
// per area, each Group setting its own area number, all under the one shadow
// name. docs/area_w3f.md section 3.
//
// The clone tables are tools/area_rows.py --clones's rows for AREA143..146
// (2026-09-28), each row read against the disassembly (every start, extent,
// call site and the one in-function jump table agree); the shapes are the
// root table each function hangs from (docs/area_w3f.md section 1). The
// group's own callees (the two glow cylinders, area 145's trails) are
// recorders here like any other callee, so each function is fuzzed alone;
// the three two-state tables are swapped for recorders (DataTable). Beyond
// the harness this file builds (area_harness.h is not edited): a packet
// buffer the draws build in, logged by the link and commit stand-ins; the
// projection 0x494110 writing a vertex; Item_NamePtr answering a name buffer
// of the fuzz's own.
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w3f.h"
#include "game/area_w3f_callees.h"
#include "game/move_script_bytes.h"
#include "hook/log.h"

namespace area_w3f {
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

// ---- area 143 ----
constexpr ah::CallSite kCalls4208E0[] = {{0x9, 0x57C7C0}};
constexpr ah::CallSite kCalls420940[] = {{0x11, 0x594E00}};
constexpr ah::CallSite kCalls420960[] = {{0x75, 0x4976D0}};
constexpr ah::CallSite kCalls420A10[] = {{0x3D, 0x531F90}};
constexpr ah::CallSite kCalls420A60[] = {{0x0, 0x57C7C0}};
constexpr ah::CallSite kCalls420B20[] = {{0x22, 0x420B50}};
constexpr ah::CallSite kCalls420B50[] = {{0x7, 0x494060}, {0xF, 0x5A7A50}, {0x27, 0x5A7A00}, {0x4C, 0x494110}, {0x67, 0x494110}, {0xC6, 0x5A7A50},
                                         {0xDA, 0x5A7A00}, {0xFF, 0x494110}, {0x11B, 0x494110}, {0x130, 0x5A79A0}, {0x149, 0x5A77C0}, {0x164, 0x572FA0},
                                         {0x170, 0x5A7610}, {0x178, 0x5A7780}, {0x21E, 0x572FA0}, {0x233, 0x5A79A0}, {0x24B, 0x5A77C0}, {0x256, 0x572FA0}};
// ---- area 144 ----
constexpr ah::CallSite kCalls420DD0[] = {{0x8, 0x57C140}, {0x3F, 0x57C140}, {0x7D, 0x57C0F0}, {0x9B, 0x57C140}, {0xC2, 0x57C0F0}, {0x111, 0x57C140}, {0x177, 0x57C140}, {0x19E, 0x57C0F0}};
constexpr ah::CallSite kCalls420FA0[] = {{0x4C, 0x578C10}};
constexpr ah::CallSite kCalls421040[] = {{0x36, 0x578C10}};
constexpr ah::CallSite kCalls421090[] = {{0x1, 0x589810}};
constexpr ah::CallSite kCalls4210F0[] = {{0x37, 0x573400}};
constexpr ah::CallSite kCalls421150[] = {{0x1B, 0x531F90}, {0x34, 0x531F90}};
// ---- area 145 ----
constexpr ah::CallSite kCalls421240[] = {{0x19, 0x572570}, {0x62, 0x5891F0}};
constexpr ah::CallSite kCalls4212F0[] = {{0x10, 0x52E140}, {0x22, 0x572570}, {0x59, 0x5891F0}};
constexpr ah::CallSite kCalls421370[] = {{0x75, 0x4976D0}};
constexpr ah::CallSite kCalls421400[] = {{0x75, 0x4976D0}};
constexpr ah::CallSite kCalls421490[] = {{0x75, 0x4976D0}};
constexpr ah::CallSite kCalls421520[] = {{0x4, 0x591680}, {0x2E, 0x591900}, {0x35, 0x497710}};
constexpr ah::CallSite kCalls421570[] = {{0x2, 0x5918E0}};
constexpr ah::CallSite kCalls421590[] = {{0x0, 0x57C7C0}};
constexpr ah::CallSite kCalls4215B0[] = {{0x35, 0x57C7C0}, {0x56, 0x57C140}, {0x96, 0x531F90}};
constexpr ah::CallSite kCalls421660[] = {{0x41, 0x57C140}, {0x4D, 0x57C7C0}};
constexpr ah::CallSite kCalls4216D0[] = {{0x43, 0x57C7A0}, {0x5D, 0x57C0F0}, {0x67, 0x587740}, {0x6E, 0x5734F0}, {0x76, 0x589810}, {0xF6, 0x587740},
                                         {0x10F, 0x421B20}, {0x12D, 0x587740}, {0x145, 0x57C140}, {0x156, 0x587740}, {0x160, 0x587740}, {0x171, 0x589810},
                                         {0x20A, 0x57C110}, {0x212, 0x57C7A0}, {0x25D, 0x57C7A0}, {0x276, 0x594E00}, {0x282, 0x57C140}, {0x293, 0x587740},
                                         {0x29F, 0x57C0F0}, {0x2B7, 0x495040}, {0x2EC, 0x495040}, {0x2F4, 0x589810}, {0x363, 0x57C0F0}, {0x37C, 0x594E00},
                                         {0x3C4, 0x594E00}};
constexpr ah::JumpTable kTables4216D0[] = {{0x21, 0x3E0, 18}};
constexpr ah::CallSite kCalls421B20[] = {{0x33, 0x421EA0}, {0x46, 0x587740}, {0x4F, 0x421B90}};
constexpr ah::CallSite kCalls421B90[] = {{0x9E, 0x421EA0}, {0xD7, 0x587740}, {0xE7, 0x421C90}};
constexpr ah::CallSite kCalls421C90[] = {{0xCF, 0x421EA0}, {0x101, 0x587740}, {0x10E, 0x421DB0}};
constexpr ah::CallSite kCalls421DB0[] = {{0xCF, 0x421EA0}};
constexpr ah::CallSite kCalls421EA0[] = {{0x0, 0x589810}};
constexpr ah::CallSite kCalls421F10[] = {{0x20, 0x5A77C0}, {0x29, 0x461E50}, {0x35, 0x5A7610}, {0x3C, 0x5A7780}, {0x91, 0x461E50}};
constexpr ah::CallSite kCalls421FB0[] = {{0x10, 0x454DC0}};
// ---- area 146 ----
constexpr ah::CallSite kCalls422000[] = {{0x10, 0x57C140}, {0x50, 0x531F90}};
constexpr ah::CallSite kCalls422060[] = {{0x0, 0x57C7C0}};
constexpr ah::CallSite kCalls4220A0[] = {{0x22, 0x4220D0}};
constexpr ah::CallSite kCalls4220D0[] = {{0x7, 0x494060}, {0xF, 0x5A7A50}, {0x27, 0x5A7A00}, {0x4C, 0x494110}, {0x67, 0x494110}, {0xC6, 0x5A7A50},
                                         {0xDA, 0x5A7A00}, {0xFF, 0x494110}, {0x11B, 0x494110}, {0x130, 0x5A79A0}, {0x149, 0x5A77C0}, {0x15C, 0x572FA0},
                                         {0x168, 0x5A7610}, {0x170, 0x5A7780}, {0x217, 0x572FA0}, {0x22C, 0x5A79A0}, {0x245, 0x5A77C0}, {0x258, 0x572FA0}};
constexpr ah::CallSite kCalls422350[] = {{0x7, 0x57C110}};

const ah::Clone kClones143[] = {
    {"Area143_ChoiceAsk52", 0x420800, 0x41, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area143_ChoiceAsk52), 0, false, S::kChoice},
    {"Area143_ChoiceYesMark3", 0x420850, 0x1A, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area143_ChoiceYesMark3), 0, false, S::kChoice},
    {"Area143_ChoiceYesMark5", 0x420870, 0x1A, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area143_ChoiceYesMark5), 0, false, S::kChoice},
    {"Area143_ChoiceAsk54", 0x420890, 0x24, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area143_ChoiceAsk54), 0, false, S::kChoice},
    {"Area143_ChoiceMessage49", 0x4208C0, 0x17, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area143_ChoiceMessage49), 0, false, S::kChoice},
    {"Area143_ChoiceRun8", 0x4208E0, 0x55, kCalls4208E0, AH_N(kCalls4208E0), nullptr, 0, nullptr, 0, OURS(Area143_ChoiceRun8), 0, false, S::kChoice},
    {"Area143_ChangeArea70", 0x420940, 0x1A, kCalls420940, AH_N(kCalls420940), nullptr, 0, nullptr, 0, OURS(Area143_ChangeArea70), 0, false, S::kHandler},
    {"Area143_MessageByMember", 0x420960, 0x8B, kCalls420960, AH_N(kCalls420960), nullptr, 0, nullptr, 0, OURS(Area143_MessageByMember), 0, false, S::kHandler},
    {"Area143_SkipIfLeader89Is7", 0x4209F0, 0x19, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area143_SkipIfLeader89Is7), 0, false, S::kHandler},
    {"Area143_StepHook", 0x420A10, 0x4B, kCalls420A10, AH_N(kCalls420A10), nullptr, 0, nullptr, 0, OURS(Area143_StepHook), 0xFF, false, S::kHook},
    {"Area143_Trigger50", 0x420A60, 0x2D, kCalls420A60, AH_N(kCalls420A60), nullptr, 0, nullptr, 0, OURS(Area143_Trigger50), 0xFF, false, S::kCallee},
    {"Area143_ClutShiftRight", 0x420A90, 0x6A, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area143_ClutShiftRight), 0xFF, false, S::kCallee},
    {"Area143_EffectB3Run", 0x420B00, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area143_EffectB3Run), 0, false, S::kCallee},
    {"Area143_EffectB3Cylinder", 0x420B20, 0x2B, kCalls420B20, AH_N(kCalls420B20), nullptr, 0, nullptr, 0, OURS(Area143_EffectB3Cylinder), 0, false, S::kState},
    {"Area143_DrawGlowCylinder", 0x420B50, 0x27B, kCalls420B50, AH_N(kCalls420B50), nullptr, 0, nullptr, 0, OURS(Area143_DrawGlowCylinder), 0, false, S::kCallee},
};
enum : unsigned { k143Ask52, k143Mark3, k143Mark5, k143Ask54, k143Msg49, k143Run8, k143Change, k143ByMember, k143Skip7, k143Step, k143Trig50, k143Clut,
                  k143EffRun, k143EffCyl, k143Cylinder };
const ah::Clone kClones144[] = {
    {"Area144_SceneByRowFlags", 0x420DD0, 0x1CA, kCalls420DD0, AH_N(kCalls420DD0), nullptr, 0, nullptr, 0, OURS(Area144_SceneByRowFlags), 0, false, S::kHandler},
    {"Area144_WalkToZ1D8", 0x420FA0, 0x55, kCalls420FA0, AH_N(kCalls420FA0), nullptr, 0, nullptr, 0, OURS(Area144_WalkToZ1D8), 0, false, S::kHandler},
    {"Area144_SkipIfMember3Is4", 0x421000, 0x14, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area144_SkipIfMember3Is4), 0, false, S::kHandler},
    {"Area144_SkipIfMember3Is2", 0x421020, 0x14, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area144_SkipIfMember3Is2), 0, false, S::kHandler},
    {"Area144_WalkToZ1C0", 0x421040, 0x46, kCalls421040, AH_N(kCalls421040), nullptr, 0, nullptr, 0, OURS(Area144_WalkToZ1C0), 0, false, S::kHandler},
    {"Area144_SpawnEffect85", 0x421090, 0x5B, kCalls421090, AH_N(kCalls421090), nullptr, 0, nullptr, 0, OURS(Area144_SpawnEffect85), 0, false, S::kHandler},
    {"Area144_MoveKind2Here", 0x4210F0, 0x3E, kCalls4210F0, AH_N(kCalls4210F0), nullptr, 0, nullptr, 0, OURS(Area144_MoveKind2Here), 0, false, S::kHandler},
    {"Area144_ChoiceCounter1E", 0x421130, 0x1C, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area144_ChoiceCounter1E), 0, false, S::kChoice},
    {"Area144_ChoiceDropIn", 0x421150, 0x3B, kCalls421150, AH_N(kCalls421150), nullptr, 0, nullptr, 0, OURS(Area144_ChoiceDropIn), 0, false, S::kChoice},
};
enum : unsigned { k144Scene, k144Walk1D8, k144Skip4, k144Skip2, k144Walk1C0, k144Spawn85, k144Kind2, k144Counter, k144DropIn };
const ah::Clone kClones145[] = {
    {"Area145_ChoiceRun3", 0x421190, 0x2E, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area145_ChoiceRun3), 0, false, S::kChoice},
    {"Area145_PartyRecord16", 0x4211C0, 0x58, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area145_PartyRecord16), 0, false, S::kHandler},
    {"Area145_RunDrop", 0x421220, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area145_RunDrop), 0, false, S::kHandler},
    {"Area145_DropStart", 0x421240, 0xAE, kCalls421240, AH_N(kCalls421240), nullptr, 0, nullptr, 0, OURS(Area145_DropStart), 0, false, S::kState},
    {"Area145_DropFall", 0x4212F0, 0x78, kCalls4212F0, AH_N(kCalls4212F0), nullptr, 0, nullptr, 0, OURS(Area145_DropFall), 0, false, S::kState},
    {"Area145_MessageByMember1", 0x421370, 0x8B, kCalls421370, AH_N(kCalls421370), nullptr, 0, nullptr, 0, OURS(Area145_MessageByMember1), 0, false, S::kHandler},
    {"Area145_MessageByMember5", 0x421400, 0x8B, kCalls421400, AH_N(kCalls421400), nullptr, 0, nullptr, 0, OURS(Area145_MessageByMember5), 0, false, S::kHandler},
    {"Area145_MessageByMemberB", 0x421490, 0x8B, kCalls421490, AH_N(kCalls421490), nullptr, 0, nullptr, 0, OURS(Area145_MessageByMemberB), 0, false, S::kHandler},
    {"Area145_GiveKeyItemB", 0x421520, 0x45, kCalls421520, AH_N(kCalls421520), nullptr, 0, nullptr, 0, OURS(Area145_GiveKeyItemB), 0, false, S::kHandler},
    {"Area145_SkipIfKeyItemB", 0x421570, 0x19, kCalls421570, AH_N(kCalls421570), nullptr, 0, nullptr, 0, OURS(Area145_SkipIfKeyItemB), 0, false, S::kHandler},
    {"Area145_Trigger40", 0x421590, 0x1D, kCalls421590, AH_N(kCalls421590), nullptr, 0, nullptr, 0, OURS(Area145_Trigger40), 0xFF, false, S::kCallee},
    {"Area145_StepHook", 0x4215B0, 0xA4, kCalls4215B0, AH_N(kCalls4215B0), nullptr, 0, nullptr, 0, OURS(Area145_StepHook), 0xFF, false, S::kHook},
    {"Area145_CellHook", 0x421660, 0x6F, kCalls421660, AH_N(kCalls421660), nullptr, 0, nullptr, 0, OURS(Area145_CellHook), 0xFF, false, S::kHook},
    {"Area145_Tail20", 0x4216D0, 0x44B, kCalls4216D0, AH_N(kCalls4216D0), nullptr, 0, kTables4216D0, AH_N(kTables4216D0), OURS(Area145_Tail20), 0, false, S::kTail},
    {"Area145_TrailStart", 0x421B20, 0x67, kCalls421B20, AH_N(kCalls421B20), nullptr, 0, nullptr, 0, OURS(Area145_TrailStart), 0xFF, false, S::kCallee},
    {"Area145_TrailLeg0", 0x421B90, 0xF5, kCalls421B90, AH_N(kCalls421B90), nullptr, 0, nullptr, 0, OURS(Area145_TrailLeg0), 0xFF, false, S::kCallee},
    {"Area145_TrailLeg1", 0x421C90, 0x11B, kCalls421C90, AH_N(kCalls421C90), nullptr, 0, nullptr, 0, OURS(Area145_TrailLeg1), 0xFF, false, S::kCallee},
    {"Area145_TrailLeg2", 0x421DB0, 0xEC, kCalls421DB0, AH_N(kCalls421DB0), nullptr, 0, nullptr, 0, OURS(Area145_TrailLeg2), 0xFF, false, S::kCallee},
    {"Area145_SpawnTrail", 0x421EA0, 0x68, kCalls421EA0, AH_N(kCalls421EA0), nullptr, 0, nullptr, 0, OURS(Area145_SpawnTrail), 0xFF, false, S::kCallee},
    {"Area145_DrawGradient", 0x421F10, 0x9C, kCalls421F10, AH_N(kCalls421F10), nullptr, 0, nullptr, 0, OURS(Area145_DrawGradient), 0, false, S::kState},
    {"Area145_ClearTint", 0x421FB0, 0x17, kCalls421FB0, AH_N(kCalls421FB0), nullptr, 0, nullptr, 0, OURS(Area145_ClearTint), 0, false, S::kHandler},
};
enum : unsigned { k145Run3, k145Record16, k145RunDrop, k145DropStart, k145DropFall, k145ByMember1, k145ByMember5, k145ByMemberB, k145GiveB, k145SkipB,
                  k145Trig40, k145Step, k145Cell, k145Tail, k145TrailStart, k145Leg0, k145Leg1, k145Leg2, k145Spawn, k145Gradient, k145Tint };
const ah::Clone kClones146[] = {
    {"Area146_LeaderBit138OtSlot", 0x421FD0, 0x23, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area146_LeaderBit138OtSlot), 0, false, S::kHandler},
    {"Area146_StepHook", 0x422000, 0x5E, kCalls422000, AH_N(kCalls422000), nullptr, 0, nullptr, 0, OURS(Area146_StepHook), 0xFF, false, S::kHook},
    {"Area146_Trigger32", 0x422060, 0x1D, kCalls422060, AH_N(kCalls422060), nullptr, 0, nullptr, 0, OURS(Area146_Trigger32), 0xFF, false, S::kCallee},
    {"Area146_EffectB4Run", 0x422080, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area146_EffectB4Run), 0, false, S::kCallee},
    {"Area146_EffectB4Cylinder", 0x4220A0, 0x2B, kCalls4220A0, AH_N(kCalls4220A0), nullptr, 0, nullptr, 0, OURS(Area146_EffectB4Cylinder), 0, false, S::kState},
    {"Area146_DrawGlowCylinder", 0x4220D0, 0x27A, kCalls4220D0, AH_N(kCalls4220D0), nullptr, 0, nullptr, 0, OURS(Area146_DrawGlowCylinder), 0, false, S::kCallee},
    {"Area146_ClearFlag46", 0x422350, 0x17, kCalls422350, AH_N(kCalls422350), nullptr, 0, nullptr, 0, OURS(Area146_ClearFlag46), 0, false, S::kHandler},
    {"Area146_ToExtraObject0", 0x422370, 0x2C, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area146_ToExtraObject0), 0, false, S::kHandler},
};
enum : unsigned { k146OtSlot, k146Step, k146Trig32, k146EffRun, k146EffCyl, k146Cylinder, k146Flag46, k146ToExtra };
#undef AH_N
#undef OURS

const ah::DataTable kTables143[] = {{at::kArea143EffectStates, at::kStateCount}};
const ah::DataTable kTables145[] = {{at::kArea145DropStates, at::kStateCount}};
const ah::DataTable kTables146[] = {{at::kArea146EffectStates, at::kStateCount}};

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
unsigned char& B(U address) { return *ah::Mem(address); }
unsigned char* EffectRecord(unsigned slot) { return ah::Mem(at::kEffectObjects + slot % at::kEffectCount * at::kEffectStride); }

// ---- the fuzz's own memory (regions filled in at SelfTest) ----

// The packet buffer Gfx_PacketNext points into while the fuzz runs: the
// cylinders build 16 x 0x5C bytes, the gradient 0x50.
constexpr unsigned kPacketBytes = 0x1000;
constexpr unsigned kPacketMargin = 0x80;
alignas(16) unsigned char g_packets[kPacketBytes];
// The name Item_NamePtr's stand-in answers, and the point a cylinder is
// handed when it is fuzzed alone (x, z, y dwords).
alignas(16) unsigned char g_name[16];
alignas(16) unsigned char g_point[12];

// Which area and function the round is running (set by the seeds).
int g_area = 0;
unsigned g_k = 0;

// ---- pointers the areas follow ----

// A record the script object pointer may name: a field object or a party
// record.
unsigned char* ScriptRecord(U v) { return v & 1 ? ah::Object(v >> 1) : ah::PartyOf(static_cast<unsigned char>(v >> 1)); }
// The focus object: a field object, or one of Sprite_ObjectsExtra's four.
unsigned char* FocusRecord(U v) { return v % 5 == 0 ? ah::Mem(at::kExtraObjects + (v >> 3) % 4 * at::kObjectStride) : ah::Object(v >> 3); }
// A value of Field_ActiveMember: only its distance from a base is read (area
// 144 from Sprite_Objects, area 145 from Sprite_ObjectsExtra), so any value
// near either, on a record or between (the quotient truncates toward 0).
U MemberValue(U v) {
    const U base = v & 1 ? 0x7DEE80u : at::kExtraObjects;
    const U rec = (v >> 1) % 7;   // area 145's records 0..6 stay inside its region
    std::int32_t off = static_cast<std::int32_t>((v >> 4) % 0x147) - 0xA3;   // -0xA3..0xA3
    if ((v >> 13) % 4 == 0) off = 0;
    return base + rec * at::kObjectStride + static_cast<U>(off);
}
void SetMember(U v) { SetLong(ah::Mem(at::kActiveMember), static_cast<std::int32_t>(MemberValue(v))); }
void SetPacketNext(U v) { Gfx_PacketNext = g_packets + kPacketMargin + (v % 32) * 4; }

// ---- the stand-ins the group lists ----

constexpr U kAll = 0xFFFFFFFFu, kU8 = 0xFFu, kU16 = 0xFFFFu;

void MoveCurrent(U n) { Sprite_Current = n & 0x100 ? ah::PartyOf(static_cast<unsigned char>(n >> 9)) : ah::Object((n >> 9) & 3); }
// Louder than the real callees, on purpose (each only part of the time,
// from Noise): the callers read cells again after these calls.
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
// ScriptFlags_Set40: area 143's choice 6 reads the message word after it
// (0x44 or not), its trigger 50 Cond_ByteFE.
U Set40Effect(const U*, U answer) {
    const U n = ah::Noise();
    static const unsigned short kMessages[] = {0x44, 0x44, 0x45, 0x144, 0x43, 0xFFFF};
    if (n & 1) SetWord(ah::Mem(at::kMessage), kMessages[(n >> 8) % 6]);
    if (n & 2) Cond_ByteFE = static_cast<unsigned char>(n & 0x400 ? 0 : n >> 16);
    if (n & 4) MoveCurrent(n >> 4);
    return answer;
}
// Effect_FindFree: area 144's spawn reads the active member after it.
U FindFreeEffect(const U*, U answer) {
    const U n = ah::Noise();
    if (n & 1) SetMember(n >> 4);
    return answer;
}
// MapView_GroundAt: half the time the running object's word +0x3E or one
// either side (area 145's fall lands on the compare), and the running
// object moved now and then (both states read it again after the call).
U GroundEffect(const U*, U answer) {
    const U n = ah::Noise();
    if (n & 1) MoveCurrent(n >> 4);
    if (n & 2) answer = (answer & 0xFFFF0000u) | ((Word(Sprite_Current + 0x3E) + (n >> 8) % 3 - 1) & 0xFFFFu);
    return answer;
}
// Item_NamePtr answers the fuzz's name buffer, filled.
U NameEffect(const U*, U) {
    ah::FillBytes(g_name, sizeof g_name);
    return Key(g_name);
}
// 0x494110 writes the projected vertex (two floats and a depth) at its
// second argument (the caller's local).
U ProjectEffect(const U* a, U answer) {
    ah::FillBytes(reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a[1])), 12);
    return answer;
}
bool InPackets(U p, unsigned n) { return p >= Key(g_packets) && p + n <= Key(g_packets) + kPacketBytes; }
// The link and commit stand-ins log the primitive at Gfx_PacketNext (every
// primitive of a draw is built at the pointer, so without the log only the
// last would be compared), then move it on by its size, kept in the buffer.
void Advance(U size) {
    size &= 0xFF;
    if (InPackets(Key(Gfx_PacketNext), size)) ah::NoteBytes(Gfx_PacketNext, size);
    const U next = Key(Gfx_PacketNext) + size;
    if (!InPackets(next, 0x80)) SetPacketNext(ah::Noise());
    else Gfx_PacketNext = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(next));
}
U LinkEffect(const U* a, U answer) {
    Advance(a[3]);
    return answer;
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
U PolyG4Effect(const U* a, U answer) { Scribble(a[0], 0x44); return answer; }
U SemiEffect(const U* a, U answer) {
    if (InPackets(a[0], 8)) ah::Mem(a[0])[7] = static_cast<unsigned char>(a[1] ? ah::Mem(a[0])[7] | 2 : ah::Mem(a[0])[7] & 0xFD);
    return answer;
}

#define W3F_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
#define W3F_THEIRS(name) #name, KeyOf(name), KeyOf(name)
const ah::Callee kCallees[] = {
    {W3F_OURS(ScriptFlags_Set40), 0, {}, ah::Answer::kGarbage, 0, 0, {}, &Set40Effect},
    {W3F_OURS(ScriptFlags_Clear40), 0, {}, ah::Answer::kGarbage, 0, 0},
    // slots inside the group's four effect records, or none
    {W3F_OURS(Effect_FindFree), 0, {}, ah::Answer::kByte, 0xFF, 0x03, {}, &FindFreeEffect},
    {W3F_OURS(KeyItem_Has), 1, {kAll}, ah::Answer::kFlag, 0, 0},
    {W3F_OURS(KeyItem_Add), 1, {kAll}, ah::Answer::kFlag, 0, 0},
    {W3F_OURS(Kind2_Place), 1, {kU8}, ah::Answer::kGarbage, 0, 0},
    {W3F_OURS(Transition_Start), 1, {kU8}, ah::Answer::kGarbage, 0, 0},
    {W3F_OURS(MoveCmd_MoveKind2), 1, {kU8}, ah::Answer::kGarbage, 0, 0},
    {W3F_THEIRS(MoveCmd_Move), 2, {kAll, kU8}, ah::Answer::kGarbage, 0, 0, {}, &MovesScriptObject},
    {W3F_OURS(Item_NamePtr), 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &NameEffect},
    {W3F_OURS(MapView_GroundAt), 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &GroundEffect},
    {W3F_OURS(Field_LeaderStepTick), 0, {}, ah::Answer::kFlag, 0, 0, {}, &MovesCurrent},
    {W3F_OURS(Sprite_SetAnimation), 1, {kU8}, ah::Answer::kGarbage, 0, 0, {}, &MovesScriptObject},
    {W3F_OURS(Sound_PlayEffect), 1, {kU16}, ah::Answer::kGarbage, 0, 0},
    // the draws
    {W3F_OURS(Gfx_CommitPrim), 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &CommitEffect},
    {W3F_OURS(MapView_LinkPrimAt), 4, {kAll, kAll, kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &LinkEffect},
    {W3F_OURS(Gpu_SetDrawMode), 5, {kAll, kAll, kAll, kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &DrawModeEffect},
    {W3F_OURS(Gpu_SetPolyG4), 1, {kAll}, ah::Answer::kGarbage, 0, 0, {}, &PolyG4Effect},
    {W3F_OURS(Gpu_SetSemiTrans), 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &SemiEffect},
    {W3F_OURS(Gpu_GetTPage), 4, {kAll, kAll, kAll, kAll}, ah::Answer::kGarbage, 0, 0},
    {W3F_OURS(Math_Sin), 1, {kAll}, ah::Answer::kGarbage, 0, 0},
    {W3F_OURS(Math_Cos), 1, {kAll}, ah::Answer::kGarbage, 0, 0},
    // Capcom's, unnamed: the map camera; a point projected (its vector a
    // local of the caller's: its twelve bytes; the vertex it writes, the
    // caller's local, not by address)
    {"SetMapCamera_494060", at::kSetMapCamera, at::kSetMapCamera, 0, {}, ah::Answer::kGarbage, 0, 0},
    {"ProjectPoint_494110", at::kProjectPoint, at::kProjectPoint, 2, {kAll, 0}, ah::Answer::kGarbage, 0, 0, {12}, &ProjectEffect},
    // the group's own, called directly: the cylinders (the point by its
    // twelve bytes), the trails (the timer's low word, the record's low
    // byte; answers 0..2)
    {W3F_OURS(Area143_DrawGlowCylinder), 1, {kAll}, ah::Answer::kGarbage, 0, 0, {12}},
    {W3F_OURS(Area146_DrawGlowCylinder), 1, {kAll}, ah::Answer::kGarbage, 0, 0, {12}},
    {W3F_OURS(Area145_TrailStart), 1, {kU16}, ah::Answer::kByte, 0x00, 0x02},
    {W3F_OURS(Area145_TrailLeg0), 1, {kU16}, ah::Answer::kByte, 0x00, 0x02},
    {W3F_OURS(Area145_TrailLeg1), 2, {kU16, kU8}, ah::Answer::kByte, 0x00, 0x02},
    {W3F_OURS(Area145_TrailLeg2), 2, {kU16, kU8}, ah::Answer::kByte, 0x00, 0x02},
    {W3F_OURS(Area145_SpawnTrail), 4, {kAll, kAll, kAll, kAll}, ah::Answer::kByte, 0x00, 0x01},
};
#undef W3F_OURS
#undef W3F_THEIRS

// ---- regions, per area (the harness holds 40 with its twenty) ----

ah::Region g_regions143[] = {
    {at::kEffectObjects, at::kEffectCount * at::kEffectStride},
    {at::kScriptObject, 4},
    {at::kFocusObject, 4},
    {at::kAnswerMark, 3},   // 0x9398CF .. 0x9398D1
    {at::kFlags904650, 4},
    {at::kCondByteFE, 1},
    {at::kPacketNext, 4},
    {0, kPacketBytes},      // g_packets
    {0, sizeof g_point},    // g_point
    {at::kClutSourceRows, at::kClutSourceEnd - at::kClutSourceRows},
    {at::kClutSourceRows + at::kClutToLive, at::kClutSourceEnd - at::kClutSourceRows},
};
const ah::Region kRegions144[] = {
    {at::kEffectObjects, at::kEffectCount * at::kEffectStride},
    {at::kActiveMember, 4},
    {at::kScriptObject, 4},
    {at::kFlagRow, 4},
    {at::kKind2, at::kObjectStride},
};
ah::Region g_regions145[] = {
    {at::kEffectObjects, at::kEffectCount * at::kEffectStride},
    {at::kActiveMember, 4},
    {at::kScriptObject, 4},
    {at::kPartyRecords, 0x60},
    {at::kTextRecords, 16},
    {at::kKind2Hold, 1},
    {at::kCameraAngle1 - 2, 6},   // Camera_Angles
    {at::kWaitWordDA, 2},
    {at::kCondByteFE, 1},
    {at::kPacketNext, 4},
    {0, kPacketBytes},      // g_packets
    {0, sizeof g_name},     // g_name
};
ah::Region g_regions146[] = {
    {at::kEffectObjects, at::kEffectCount * at::kEffectStride},
    {at::kScriptObject, 4},
    {at::kCondByteFE, 1},
    {at::kOtSlot, 1},
    {at::kPacketNext, 4},
    {0, kPacketBytes},      // g_packets
    {0, sizeof g_point},    // g_point
};

// Every round: the pointers the areas follow put back inside the regions.
void Common(int area, unsigned k) {
    g_area = area;
    g_k = k;
    ah::SetPointer(at::kScriptObject, ScriptRecord(ah::Next()));
    ah::SetPointer(at::kFocusObject, FocusRecord(ah::Next()));
    SetMember(ah::Next());
    ah::SetPointer(at::kFlagRow, ah::Mem(ah::at::kCondFlags + (ah::Next() % 24) * 8));
    SetPacketNext(ah::Next());
}

// The group's cells, moved by the harness's disturbance about one call in
// 24 - drawn only from h (area_harness.h: a group disturb never draws Next).
void Disturb(U h) {
    const auto v = static_cast<unsigned char>(h >> 20);
    switch ((h >> 8) % 10) {
    case 0: B(at::kTailState) = static_cast<unsigned char>(h & 0x100 ? v % 0x26 : v); break;
    case 1: B(at::kCounter3) = static_cast<unsigned char>(h & 0x100 ? (v & 1 ? 0 : 0x40) : v); break;
    case 2: SetWord(ah::Mem(at::kTailTimer), h & 0x100 ? v % 3 : v); break;
    case 3: ah::SetPointer(at::kScriptObject, ScriptRecord(h >> 16)); break;
    case 4: ah::SetPointer(at::kFocusObject, FocusRecord(h >> 16)); break;
    case 5: SetMember(h >> 12); break;
    case 6: Field_Kind2Hold = static_cast<unsigned char>(h & 0x100 ? 0 : v); break;
    case 7: MoveScript_WaitWordDA = static_cast<unsigned short>(h & 0x100 ? 0 : v); break;
    case 8: Cond_ByteFE = static_cast<unsigned char>(h & 0x100 ? 0 : v); break;
    default: B(at::kChoiceAnswer) = static_cast<unsigned char>(v % 3); break;
    }
}

// A choice answer: 0 often, 1, a negative byte, anything.
void SeedAnswer() {
    if (ah::Often()) B(at::kChoiceAnswer) = static_cast<unsigned char>(AH_PICK(0, 0, 1, 2, 0xFF, 0x80, 0x7F));
}
// A 16.16 word with the high word `high` and any low word.
U At16(U high, U low) { return (high & 0xFFFF) << 16 | (low & 0xFFFF); }
// A high word on or beside [lo, lo + n): each inside, one either side, a
// high byte above it (the compares are 16-bit), anything.
U Around(U lo, unsigned n) {
    switch (ah::Next() % 5) {
    case 0: case 1: return lo + ah::Next() % n;
    case 2: return ah::Half() ? lo - 1 : lo + n;
    case 3: return (lo + ah::Next() % n) | 0x100u;
    default: return ah::Next();
    }
}
// The leader's pose: each the hooks accept, and beside them.
void SeedPose() {
    if (ah::Often()) B(at::kLeaderPose) = static_cast<unsigned char>(AH_PICK(0, 7, 6, 0, 7, 6, 1, 5, 8, 0x80, 0x87));
}
// An object trigger is called (a field object, 0x904030).
void ArgsTrigger(std::uint32_t* a) {
    a[0] = Key(ah::Object(a[0]));
    a[1] = at::kStoryFlags;
}
// Each party record's +0x89 one of `keys` (or beside one), the member count
// 1..3 (0 a tenth of the time).
void SeedMemberKeys(U keys) {
    for (unsigned m = 0; m < 3; ++m) {
        unsigned char* const record = ah::PartyOf(static_cast<unsigned char>(m));
        if (ah::Often()) record[0x89] = static_cast<unsigned char>(B(keys + ah::Next() % 4) + (ah::Often() ? 0 : 1));
    }
    if (ah::Next() % 10 == 0) Field_MemberCount = 0;
}
// A running effect record (area 143's and 146's effect kinds).
void SeedEffectCurrent() { Sprite_Current = EffectRecord(ah::Next() % 4); }

// ---- area 143 ----
void Seed143(unsigned k) {
    Common(143, k);
    switch (k) {
    case k143Ask52:
        SeedAnswer();
        if (ah::Often()) SetLong(ah::Mem(at::kFlags904650), static_cast<std::int32_t>(AH_PICK(0x3FFFF, 0x3FFFF, 0x3FFFE, 0x7FFFF, 0x13FFFF, 0x8003FFFF, 0x2FFFF)));
        break;
    case k143Mark3: case k143Mark5: case k143Ask54: case k143Msg49: SeedAnswer(); break;
    case k143Run8:
        SeedAnswer();
        if (ah::Half()) SetWord(ah::Mem(at::kMessage), AH_PICK(0x44, 0x45, 0x144, 0xFFFF));
        break;
    case k143ByMember: SeedMemberKeys(at::kArea143MemberKeys); break;
    case k143Skip7:
        if (ah::Often()) Field_State[0x89] = static_cast<unsigned char>(AH_PICK(7, 7, 6, 8, 0x87));
        break;
    case k143Step:
        if (ah::Often()) Cond_ByteFD = static_cast<unsigned char>(AH_PICK(1, 1, 0, 2, 0x81));
        SeedPose();
        break;
    case k143Trig50:
        if (ah::Often()) Cond_ByteFE = static_cast<unsigned char>(AH_PICK(0, 0, 1, 0x80));
        break;
    case k143EffRun:
        SeedEffectCurrent();
        Sprite_Current[1] = static_cast<unsigned char>(ah::Next() % at::kStateCount);
        break;
    case k143EffCyl: SeedEffectCurrent(); break;
    default: break;
    }
}
void Args143(unsigned k, std::uint32_t* a) {
    switch (k) {
    case k143Step:
        if (!ah::Often()) return;
        a[0] = At16(Around(0x37, 3), a[0]);
        a[1] = At16(Around(0x64, 3), a[1]);
        break;
    case k143Trig50: ArgsTrigger(a); break;
    case k143Clut: a[0] = ah::Often() ? AH_PICK(1, 1, 0, 2, 3, 4, 5, 0x1F, 0x20, 0x21, 0x101) : a[0]; break;
    case k143Cylinder: a[0] = Key(g_point); break;
    default: break;
    }
}

// ---- area 144 ----
// The running object's z at or beside a whole step from `target` (d 0, +-1,
// many), or anything.
void SeedZ(U target) {
    if (!ah::Often()) return;
    static const std::int32_t kOff[] = {0, 1, 0x7FFF, 0x8000, -1, -0x8000, -0x8001, 0x10000, -0x10000, 0x7FFFFF};
    SetLong(Sprite_Current + 0x38, static_cast<std::int32_t>(target - static_cast<U>(kOff[ah::Next() % 10])));
}
void Seed144(unsigned k) {
    Common(144, k);
    switch (k) {
    case k144Scene:
        if (ah::Often()) B(at::kLeader89) = static_cast<unsigned char>(AH_PICK(2, 5, 7, 8, 2, 5, 7, 8, 3, 0x82));
        break;
    case k144Walk1D8: SeedZ(0x1D8000); break;
    case k144Walk1C0: SeedZ(0x1C0000); break;
    case k144Skip4: case k144Skip2:
        if (ah::Often()) B(at::kMember3_89) = static_cast<unsigned char>(AH_PICK(4, 2, 4, 2, 3, 5, 0x84, 0x82));
        break;
    case k144Counter: case k144DropIn: SeedAnswer(); break;
    default: break;
    }
}

// ---- area 145 ----
void Seed145(unsigned k) {
    Common(145, k);
    switch (k) {
    case k145Run3: SeedAnswer(); break;
    case k145RunDrop: Sprite_Current[4] = static_cast<unsigned char>(ah::Next() % at::kStateCount); break;
    case k145DropStart:
        // Field_State's value only is read: on a record, between two, or just
        // below the first (the quotient truncates toward 0)
        if (ah::Half()) {
            const std::int32_t off = static_cast<std::int32_t>(ah::Next() % 0x3E4) - (ah::Next() % 4 == 0 ? 0x14C : 0);
            ah::SetPointer(ah::at::kFieldState, ah::Mem(static_cast<U>(static_cast<std::int32_t>(ah::at::kParty) + off)));
        }
        break;
    case k145DropFall:
        if (ah::Half()) SetLong(Sprite_Current + 0x20, static_cast<std::int32_t>(AH_PICK(0, 1, 0xFFFFFFF8u, 0x80000000u, 0x7FFFFFFF)));
        break;
    case k145ByMember1: SeedMemberKeys(at::kArea145MemberKeys1); break;
    case k145ByMember5: SeedMemberKeys(at::kArea145MemberKeys5); break;
    case k145ByMemberB: SeedMemberKeys(at::kArea145MemberKeysB); break;
    case k145Step:
        if (ah::Often()) Cond_ByteFD = static_cast<unsigned char>(AH_PICK(0, 0, 4, 4, 1, 3, 5, 0x80, 0x84));
        if (ah::Often()) SetWord(ah::Mem(at::kLeaderXHigh), Around(0x1B, 2));
        SeedPose();
        break;
    case k145Cell:
        if (ah::Often()) B(at::kLeaderPose) = static_cast<unsigned char>(AH_PICK(1, 7, 7, 7, 0, 6, 0x17, 0x87));
        break;
    case k145Tail: {
        static const std::uint32_t kStates[] = {2, 4, 6, 8, 0xA, 0xB, 0xC, 0x14, 0x15, 0x16, 0x19, 0x1A, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24,
                                                2, 4, 6, 8, 0xA, 0xB, 0xC, 0x14, 0x15, 0x16, 0x19, 0x1A, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24,
                                                0, 1, 3, 5, 7, 9, 0xD, 0x13, 0x17, 0x18, 0x1B, 0x1D, 0x25, 0x26, 0x7F, 0x80, 0xFE, 0xFF, 0x82, 0x84};
        const auto state = static_cast<unsigned char>(ah::Pick(kStates, sizeof kStates / sizeof kStates[0]));
        B(at::kTailState) = state;
        const bool on = ah::Often();   // the cell the state waits on, at its value or beside it
        switch (state) {
        case 8: B(at::kCounter3) = static_cast<unsigned char>(on ? 0 : AH_PICK(1, 0x40, 0x80)); break;
        case 0xB: B(at::kCounter3) = static_cast<unsigned char>(on ? 0x40 : AH_PICK(0, 0x3F, 0x41, 0xC0)); break;
        case 0xC: SetWord(ah::Mem(at::kTailTimer), AH_PICK(0, 4, 5, 0x36, 0x37, 0xFFFF, 0x104)); break;
        case 0x15: case 0x20: Field_Kind2Hold = static_cast<unsigned char>(on ? 0 : AH_PICK(1, 0x80)); break;
        case 0x1A: case 0x21: case 0x24: SetWord(ah::Mem(at::kTailTimer), on ? 0 : AH_PICK(1, 2, 0x100, 0xFFFF)); break;
        case 0x1F: case 0x23: MoveScript_WaitWordDA = static_cast<unsigned short>(on ? 0 : AH_PICK(1, 0x100, 0xFFFF)); break;
        default: break;
        }
        break;
    }
    case k145Gradient:
        if (ah::Half()) Cond_ByteFE = 0;
        break;
    default: break;
    }
}
// The leg a trail function reads (a length of 2..13) and the timer at or
// beside its end: t - bias just below, at and past the length, small, big.
U TrailTimer(unsigned bias) {
    static const std::uint32_t kD[] = {0, 1, 2, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 0xFFFF, 0x8000};
    return static_cast<U>(bias + ah::Pick(kD, sizeof kD / sizeof kD[0])) | (ah::Half() ? 0 : ah::Next() & 0xFFFF0000u);
}
void Args145(unsigned k, std::uint32_t* a) {
    switch (k) {
    case k145Trig40: ArgsTrigger(a); break;
    case k145Step:
        if (!ah::Often()) return;
        if (Cond_ByteFD == 0) {
            a[0] = At16(Around(0x1B, 2), a[0]);
            a[1] = ah::Often() ? 0x98000 : AH_PICK(0x98001, 0x97FFF, 0x198000, 0x8000);
        } else {
            a[0] = At16(Around(9, 3), a[0]);
            a[1] = At16(Around(0x62, 3), a[1]);
        }
        break;
    case k145Cell: {
        if (!ah::Often()) return;
        // a record's x and z bytes (with any higher bytes), or one beside
        const U rec = at::kArea145CellRecords + (ah::Next() % at::kArea145CellRecordCount) * 4;
        a[0] = (ah::Next() & 0xFFFFFF00u) | static_cast<U>(B(rec) + (ah::Often() ? 0 : 1));
        a[1] = (ah::Next() & 0xFFFFFF00u) | static_cast<U>(B(rec + 1) + (ah::Often() ? 0 : 0xFF));
        if (ah::Often()) B(at::kLeaderPose) = static_cast<unsigned char>(B(rec + 2) & 0xF);
        break;
    }
    case k145TrailStart: a[0] = TrailTimer(0) | 0; break;
    case k145Leg0: a[0] = TrailTimer(5); break;
    case k145Leg1: case k145Leg2:
        a[0] = TrailTimer(k == k145Leg1 ? 5 : 10);
        a[1] = ah::Often() ? AH_PICK(1, 2, 1, 2, 0, 3, 0x101, 0x102) : a[1] & 0xFF;
        break;
    default: break;
    }
}

// ---- area 146 ----
void Seed146(unsigned k) {
    Common(146, k);
    switch (k) {
    case k146Step:
        if (ah::Often()) Cond_ByteFD = static_cast<unsigned char>(AH_PICK(1, 1, 0, 2, 0x81));
        SeedPose();
        break;
    case k146EffRun:
        SeedEffectCurrent();
        Sprite_Current[1] = static_cast<unsigned char>(ah::Next() % at::kStateCount);
        break;
    case k146EffCyl: SeedEffectCurrent(); break;
    default: break;
    }
}
void Args146(unsigned k, std::uint32_t* a) {
    switch (k) {
    case k146Step:
        if (!ah::Often()) return;
        a[0] = At16(Around(0x44, 3), a[0]);
        a[1] = At16(Around(0x2A, 3), a[1]);
        break;
    case k146Trig32: ArgsTrigger(a); break;
    case k146Cylinder: a[0] = Key(g_point); break;
    default: break;
    }
}

void RunArea(int area, const ah::Clone* clones, unsigned n, const ah::DataTable* tables, unsigned n_tables, const ah::Region* regions,
             unsigned n_regions, void (*seed)(unsigned), void (*args)(unsigned, std::uint32_t*), unsigned rounds) {
    ah::Group g{"area_w3f", clones, n, kCallees, sizeof kCallees / sizeof kCallees[0], tables, n_tables,
                regions, n_regions, seed, &Disturb, rounds};
    g.args = args;
    g.area = area;
    ah::Run(g);
}

// The runtime addresses of the fuzz's own buffers, in each region list.
void Place(ah::Region* regions, unsigned n) {
    for (unsigned i = 0; i < n; ++i) {
        if (regions[i].at != 0) continue;
        regions[i].at = regions[i].size == kPacketBytes ? Key(g_packets) : regions[i].size == sizeof g_name ? Key(g_name) : Key(g_point);
    }
}

}  // namespace

void SelfTest() {
    constexpr unsigned kRounds = 6000;
    Place(g_regions143, sizeof g_regions143 / sizeof g_regions143[0]);
    Place(g_regions145, sizeof g_regions145 / sizeof g_regions145[0]);
    Place(g_regions146, sizeof g_regions146 / sizeof g_regions146[0]);
    RunArea(143, kClones143, sizeof kClones143 / sizeof kClones143[0], kTables143, 1, g_regions143, sizeof g_regions143 / sizeof g_regions143[0],
            &Seed143, &Args143, kRounds);
    RunArea(144, kClones144, sizeof kClones144 / sizeof kClones144[0], nullptr, 0, kRegions144, sizeof kRegions144 / sizeof kRegions144[0], &Seed144,
            nullptr, kRounds);
    RunArea(145, kClones145, sizeof kClones145 / sizeof kClones145[0], kTables145, 1, g_regions145, sizeof g_regions145 / sizeof g_regions145[0],
            &Seed145, &Args145, kRounds);
    RunArea(146, kClones146, sizeof kClones146 / sizeof kClones146[0], kTables146, 1, g_regions146, sizeof g_regions146 / sizeof g_regions146[0],
            &Seed146, &Args146, kRounds);
}

}  // namespace area_w3f

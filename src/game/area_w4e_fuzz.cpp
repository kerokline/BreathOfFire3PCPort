// BOF3X_SHADOW=area_w4e: world 4's areas 188..191 through the area round's
// shared harness (area_harness.h), once at start-up - one area_harness::Run
// per area with code (188, 189, 191; 190 has none), each Group setting its own
// area number, all under the one shadow name. docs/area_w4e.md section 3.
//
// The clone tables are tools/area_rows.py --clones's rows for AREA188..191
// (2026-09-28), each row read against the disassembly (every start, extent,
// call site and the two in-function jump tables agree; the tool files area
// 189's leader states under AREA190 by their .data, section 7 of the doc);
// the shapes are the root table each function hangs from (docs/area_w4e.md
// section 1). The group's own callees are recorders here like any other
// callee, so each function is fuzzed alone; the two state tables are swapped
// for recorders (DataTable). Beyond the harness this file builds
// (area_harness.h is not edited): Gte_RotTransPers listed with its screen
// point written, Field_ChangeArea's and the other stale-bit arguments masked,
// louder stand-ins where a caller reads a cell again after a call, and a
// settle that keeps area 189's step divisor from being 0 (the original would
// fault there, as ours aborts).
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w4e.h"
#include "game/area_w4e_callees.h"
#include "game/move_script_bytes.h"
#include "hook/log.h"

namespace area_w4e {
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

// ---- area 188 ----
constexpr ah::CallSite kCalls42A320[] = {{0x12, 0x57C7C0}};
constexpr ah::CallSite kCalls42A350[] = {{0x12, 0x57C7C0}};
constexpr ah::CallSite kCalls42A380[] = {{0x40, 0x57C7C0}, {0x4D, 0x57C7C0}};
constexpr ah::CallSite kCalls42A3E0[] = {{0x55, 0x57C7C0}};
constexpr ah::CallSite kCalls42A450[] = {{0x3F, 0x578C10}};
constexpr ah::CallSite kCalls42A4C0[] = {{0x6, 0x579F00}, {0x11, 0x579F00}, {0x1C, 0x579F00}, {0x27, 0x579F00}};
constexpr ah::CallSite kCalls42A4F0[] = {{0x9, 0x579F00}, {0x17, 0x579F00}, {0x25, 0x579F00}, {0x33, 0x579F00}};
constexpr ah::CallSite kCalls42A530[] = {{0x6, 0x579F00}, {0x11, 0x579F00}, {0x1C, 0x579F00}, {0x27, 0x579F00}};
constexpr ah::CallSite kCalls42A560[] = {{0x9, 0x579F00}, {0x17, 0x579F00}, {0x25, 0x579F00}, {0x33, 0x579F00}};
constexpr ah::CallSite kCalls42A5A0[] = {{0x6, 0x579F00}, {0x11, 0x579F00}, {0x1C, 0x579F00}, {0x27, 0x579F00}, {0x32, 0x579F00}, {0x3D, 0x579F00}};
constexpr ah::CallSite kCalls42A5F0[] = {{0x1, 0x589810}};
constexpr ah::CallSite kCalls42A620[] = {{0x22, 0x5918E0}, {0x4D, 0x4976D0}, {0x84, 0x57C7A0}, {0x9F, 0x4976D0},
                                         {0xB1, 0x57C160}, {0xCD, 0x531F90}, {0x115, 0x594E00}, {0x131, 0x57C7A0}};
constexpr ah::JumpTable kTables42A620[] = {{0x1C, 0x14C, 8}};
// ---- area 189 ----
constexpr ah::CallSite kCalls42A7D0[] = {{0xA, 0x57C140}};
constexpr ah::CallSite kCalls42A7F0[] = {{0x12, 0x57C7C0}};
constexpr ah::CallSite kCalls42A820[] = {{0x12, 0x57C7C0}};
constexpr ah::CallSite kCalls42A850[] = {{0x9, 0x57C7A0}, {0x48, 0x594E00}};
constexpr ah::CallSite kCalls42A8D0[] = {{0xE, 0x5A7AE0}, {0x18, 0x5A7B00}, {0x79, 0x511C10}, {0x8D, 0x42B440}, {0xAE, 0x5B93D2},
                                         {0xBF, 0x531BB0}, {0xDC, 0x535310}, {0x10D, 0x535310}, {0x145, 0x57C110}, {0x151, 0x57C110},
                                         {0x160, 0x57C110}, {0x16F, 0x57C110}, {0x17E, 0x57C110}, {0x18D, 0x57C110}};
constexpr ah::CallSite kCalls42AA80[] = {{0x66, 0x5A8250}, {0x75, 0x5A9110}, {0x9C, 0x57C140}, {0xAF, 0x57C140}};
constexpr ah::CallSite kCalls42AB90[] = {{0x2D, 0x42B320}, {0x3A, 0x42B400}, {0x47, 0x42B460}, {0x68, 0x42ADB0}, {0xBD, 0x4976D0}, {0xCC, 0x42B2F0},
                                         {0xDE, 0x57C140}, {0x107, 0x4976D0}, {0x116, 0x42B2F0}, {0x129, 0x42AE30}, {0x18A, 0x42B2F0}};
constexpr ah::CallSite kCalls42AD30[] = {{0x19, 0x42AE80}, {0x23, 0x42AE30}};
constexpr ah::CallSite kCalls42ADB0[] = {{0x54, 0x511C10}};
constexpr ah::CallSite kCalls42AE80[] = {{0x0, 0x42B440}, {0x48, 0x4976D0}, {0x9E, 0x4976D0}, {0xF1, 0x4976D0}, {0x105, 0x5B93D2},
                                         {0x139, 0x42B4E0}, {0x14A, 0x587740}, {0x15B, 0x42B510}, {0x16C, 0x587740}, {0x191, 0x42B2F0},
                                         {0x19D, 0x57C140}, {0x1CD, 0x5658B0}, {0x1D2, 0x42B2F0}, {0x1DF, 0x56D750}, {0x1EB, 0x42B2F0},
                                         {0x2D9, 0x57C0F0}, {0x309, 0x57C0F0}, {0x33C, 0x57C0F0}, {0x36F, 0x57C0F0}, {0x3A2, 0x57C0F0},
                                         {0x3D3, 0x57C0F0}, {0x3FA, 0x5B93D2}, {0x436, 0x594E00}, {0x43E, 0x42B2F0}, {0x462, 0x42AB90},
                                         {0x467, 0x42B2F0}};
constexpr ah::CallSite kCalls42B2F0[] = {{0x0, 0x42B440}};
constexpr ah::CallSite kCalls42B320[] = {{0x86, 0x5B93D2}, {0xC3, 0x594E00}};
constexpr ah::CallSite kCalls42B400[] = {{0x1D, 0x587740}};
constexpr ah::CallSite kCalls42B4E0[] = {{0x14, 0x590660}};
// ---- area 191 ----
constexpr ah::CallSite kCalls42B5D0[] = {{0x12, 0x57C7C0}, {0x2A, 0x57C7C0}};
constexpr ah::CallSite kCalls42B610[] = {{0x0, 0x57C7C0}};
constexpr ah::CallSite kCalls42B640[] = {{0x0, 0x57C7C0}};
constexpr ah::CallSite kCalls42B7F0[] = {{0x22, 0x4976D0}, {0x49, 0x495040}, {0x6F, 0x587B80}, {0x74, 0x42C2D0}, {0x7A, 0x587910},
                                         {0x8B, 0x587A00}, {0xA2, 0x57C0F0}, {0xEF, 0x57C7A0}, {0x10B, 0x594E00}, {0x126, 0x57C110},
                                         {0x145, 0x57C7A0}, {0x163, 0x57C7A0}, {0x179, 0x594E00}, {0x194, 0x57C110}};
constexpr ah::JumpTable kTables42B7F0[] = {{0x1C, 0x1B8, 8}};
constexpr ah::CallSite kCalls42B9F0[] = {{0x10, 0x536700}, {0x2B, 0x536700}, {0x47, 0x536700}, {0x66, 0x536700}, {0x7C, 0x57C7C0}};
constexpr ah::CallSite kCalls42BA90[] = {{0x6D, 0x57C140}, {0x8D, 0x42BBB0}, {0xE2, 0x57C140}};
constexpr ah::CallSite kCalls42BBF0[] = {{0x11, 0x57C140}, {0x3F, 0x57C140}, {0x56, 0x57C140}, {0xAA, 0x57CD90}, {0xD1, 0x57A010}};
constexpr ah::CallSite kCalls42BCE0[] = {{0x7, 0x57C140}, {0x3F, 0x589840}};
constexpr ah::CallSite kCalls42BD30[] = {{0x12, 0x57C7C0}};

const ah::Clone kClones188[] = {
    {"Area188_ChoiceTail43A", 0x42A320, 0x2D, kCalls42A320, AH_N(kCalls42A320), nullptr, 0, nullptr, 0, OURS(Area188_ChoiceTail43A), 0, false, S::kChoice},
    {"Area188_ChoiceTail43B", 0x42A350, 0x2D, kCalls42A350, AH_N(kCalls42A350), nullptr, 0, nullptr, 0, OURS(Area188_ChoiceTail43B), 0, false, S::kChoice},
    {"Area188_ChoiceByChapter", 0x42A380, 0x5A, kCalls42A380, AH_N(kCalls42A380), nullptr, 0, nullptr, 0, OURS(Area188_ChoiceByChapter), 0, false, S::kChoice},
    {"Area188_ChoiceFocusPair", 0x42A3E0, 0x69, kCalls42A3E0, AH_N(kCalls42A3E0), nullptr, 0, nullptr, 0, OURS(Area188_ChoiceFocusPair), 0, false, S::kChoice},
    {"Area188_WalkWhileZUnder", 0x42A450, 0x53, kCalls42A450, AH_N(kCalls42A450), nullptr, 0, nullptr, 0, OURS(Area188_WalkWhileZUnder), 0, false, S::kHandler},
    {"Area188_ShiftCameraDown2", 0x42A4B0, 0x10, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area188_ShiftCameraDown2), 0, false, S::kHandler},
    {"Area188_PatchCellsA0", 0x42A4C0, 0x30, kCalls42A4C0, AH_N(kCalls42A4C0), nullptr, 0, nullptr, 0, OURS(Area188_PatchCellsA0), 0, false, S::kHandler},
    {"Area188_PatchCellsA1", 0x42A4F0, 0x3C, kCalls42A4F0, AH_N(kCalls42A4F0), nullptr, 0, nullptr, 0, OURS(Area188_PatchCellsA1), 0, false, S::kHandler},
    {"Area188_PatchCellsB0", 0x42A530, 0x30, kCalls42A530, AH_N(kCalls42A530), nullptr, 0, nullptr, 0, OURS(Area188_PatchCellsB0), 0, false, S::kHandler},
    {"Area188_PatchCellsB1", 0x42A560, 0x3C, kCalls42A560, AH_N(kCalls42A560), nullptr, 0, nullptr, 0, OURS(Area188_PatchCellsB1), 0, false, S::kHandler},
    {"Area188_ClearCellsC", 0x42A5A0, 0x46, kCalls42A5A0, AH_N(kCalls42A5A0), nullptr, 0, nullptr, 0, OURS(Area188_ClearCellsC), 0, false, S::kHandler},
    {"Area188_SpawnEffectB9", 0x42A5F0, 0x28, kCalls42A5F0, AH_N(kCalls42A5F0), nullptr, 0, nullptr, 0, OURS(Area188_SpawnEffectB9), 0, false, S::kHandler},
    {"Area188_Tail43", 0x42A620, 0x17A, kCalls42A620, AH_N(kCalls42A620), nullptr, 0, kTables42A620, AH_N(kTables42A620), OURS(Area188_Tail43), 0, false, S::kTail},
    {"Area188_Init", 0x42A7A0, 0x2F, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area188_Init), 0, false, S::kInit},
};
enum : unsigned { k188Tail43A, k188Tail43B, k188ByChapter, k188FocusPair, k188Walk, k188CameraDown, k188PatchA0, k188PatchA1, k188PatchB0,
                  k188PatchB1, k188ClearC, k188SpawnB9, k188Tail43, k188Init };
const ah::Clone kClones189[] = {
    {"Area189_Init", 0x42A7D0, 0x1E, kCalls42A7D0, AH_N(kCalls42A7D0), nullptr, 0, nullptr, 0, OURS(Area189_Init), 0, false, S::kInit},
    {"Area189_ChoiceTail50A", 0x42A7F0, 0x26, kCalls42A7F0, AH_N(kCalls42A7F0), nullptr, 0, nullptr, 0, OURS(Area189_ChoiceTail50A), 0, false, S::kChoice},
    {"Area189_ChoiceTail50B", 0x42A820, 0x26, kCalls42A820, AH_N(kCalls42A820), nullptr, 0, nullptr, 0, OURS(Area189_ChoiceTail50B), 0, false, S::kChoice},
    {"Area189_Tail50", 0x42A850, 0x5F, kCalls42A850, AH_N(kCalls42A850), nullptr, 0, nullptr, 0, OURS(Area189_Tail50), 0, false, S::kTail},
    {"Area189_LeaderRun", 0x42A8B0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area189_LeaderRun), 0, false, S::kState},
    {"Area189_LeaderStart", 0x42A8D0, 0x1A7, kCalls42A8D0, AH_N(kCalls42A8D0), nullptr, 0, nullptr, 0, OURS(Area189_LeaderStart), 0, false, S::kState},
    {"Area189_LeaderProject", 0x42AA80, 0x102, kCalls42AA80, AH_N(kCalls42AA80), nullptr, 0, nullptr, 0, OURS(Area189_LeaderProject), 0, false, S::kState},
    {"Area189_LeaderControl", 0x42AB90, 0x195, kCalls42AB90, AH_N(kCalls42AB90), nullptr, 0, nullptr, 0, OURS(Area189_LeaderControl), 0, false, S::kState},
    {"Area189_LeaderStep", 0x42AD30, 0x28, kCalls42AD30, AH_N(kCalls42AD30), nullptr, 0, nullptr, 0, OURS(Area189_LeaderStep), 0, false, S::kState},
    {"Area189_LeaderTurn", 0x42AD60, 0x4D, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area189_LeaderTurn), 0, false, S::kState},
    {"Area189_StepBegin", 0x42ADB0, 0x7A, kCalls42ADB0, AH_N(kCalls42ADB0), nullptr, 0, nullptr, 0, OURS(Area189_StepBegin), 0, false, S::kCallee},
    {"Area189_StepMove", 0x42AE30, 0x4A, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area189_StepMove), 0, false, S::kCallee},
    {"Area189_StepArrive", 0x42AE80, 0x46C, kCalls42AE80, AH_N(kCalls42AE80), nullptr, 0, nullptr, 0, OURS(Area189_StepArrive), 0, false, S::kCallee},
    {"Area189_LeaderHalt", 0x42B2F0, 0x26, kCalls42B2F0, AH_N(kCalls42B2F0), nullptr, 0, nullptr, 0, OURS(Area189_LeaderHalt), 0, false, S::kCallee},
    {"Area189_ExitButton", 0x42B320, 0xD1, kCalls42B320, AH_N(kCalls42B320), nullptr, 0, nullptr, 0, OURS(Area189_ExitButton), 0xFF, false, S::kCallee},
    {"Area189_MenuButton", 0x42B400, 0x3A, kCalls42B400, AH_N(kCalls42B400), nullptr, 0, nullptr, 0, OURS(Area189_MenuButton), 0xFF, false, S::kCallee},
    {"Area189_ZeroSpeeds", 0x42B440, 0x1E, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area189_ZeroSpeeds), 0, false, S::kCallee},
    {"Area189_TurnInput", 0x42B460, 0x77, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area189_TurnInput), 0xFF, false, S::kCallee},
    {"Area189_RaiseByte1E", 0x42B4E0, 0x2C, kCalls42B4E0, AH_N(kCalls42B4E0), nullptr, 0, nullptr, 0, OURS(Area189_RaiseByte1E), 0, false, S::kCallee},
    {"Area189_DrainHp", 0x42B510, 0x3F, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area189_DrainHp), 0, false, S::kCallee},
};
enum : unsigned { k189Init, k189Tail50A, k189Tail50B, k189Tail50, k189Run, k189Start, k189Project, k189Control, k189Step, k189Turn,
                  k189StepBegin, k189StepMove, k189Arrive, k189Halt, k189Exit, k189Menu, k189Zero, k189TurnInput, k189Raise, k189Drain };
const ah::Clone kClones191[] = {
    {"Area191_ChoiceMessage68", 0x42B550, 0x18, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area191_ChoiceMessage68), 0, false, S::kChoice},
    {"Area191_ChoiceFocusPair", 0x42B570, 0x3C, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area191_ChoiceFocusPair), 0, false, S::kChoice},
    {"Area191_ChoiceMessage6C", 0x42B5B0, 0x18, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area191_ChoiceMessage6C), 0, false, S::kChoice},
    {"Area191_ChoiceTail53", 0x42B5D0, 0x3E, kCalls42B5D0, AH_N(kCalls42B5D0), nullptr, 0, nullptr, 0, OURS(Area191_ChoiceTail53), 0, false, S::kChoice},
    {"Area191_MemberNext", 0x42B610, 0x26, kCalls42B610, AH_N(kCalls42B610), nullptr, 0, nullptr, 0, OURS(Area191_MemberNext), 0, false, S::kHandler},
    {"Area191_MemberNextRun", 0x42B640, 0x34, kCalls42B640, AH_N(kCalls42B640), nullptr, 0, nullptr, 0, OURS(Area191_MemberNextRun), 0, false, S::kHandler},
    {"Area191_RunScale", 0x42B680, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area191_RunScale), 0, false, S::kHandler},
    {"Area191_ScaleStart", 0x42B6A0, 0x6A, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area191_ScaleStart), 0, false, S::kState},
    {"Area191_ScaleGrow", 0x42B710, 0x68, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area191_ScaleGrow), 0, false, S::kState},
    {"Area191_ScaleShrink", 0x42B780, 0x68, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area191_ScaleShrink), 0, false, S::kState},
    {"Area191_Tail53", 0x42B7F0, 0x1F7, kCalls42B7F0, AH_N(kCalls42B7F0), nullptr, 0, kTables42B7F0, AH_N(kTables42B7F0), OURS(Area191_Tail53), 0, false, S::kTail},
    {"Area191_StepHook", 0x42B9F0, 0x95, kCalls42B9F0, AH_N(kCalls42B9F0), nullptr, 0, nullptr, 0, OURS(Area191_StepHook), 0xFF, false, S::kHook},
    {"Area191_TalkMessage", 0x42BA90, 0x11B, kCalls42BA90, AH_N(kCalls42BA90), nullptr, 0, nullptr, 0, OURS(Area191_TalkMessage), 0xFFFF, false, S::kCallee},
    {"Area191_TalkMessageB", 0x42BBB0, 0x3B, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area191_TalkMessageB), 0xFFFF, false, S::kCallee},
    {"Area191_Init", 0x42BBF0, 0xE4, kCalls42BBF0, AH_N(kCalls42BBF0), nullptr, 0, nullptr, 0, OURS(Area191_Init), 0, false, S::kInit},
    {"Area191_Kind18Flag77", 0x42BCE0, 0x45, kCalls42BCE0, AH_N(kCalls42BCE0), nullptr, 0, nullptr, 0, OURS(Area191_Kind18Flag77), 0, false, S::kState},
    {"Area191_ChoiceRun1", 0x42BD30, 0x26, kCalls42BD30, AH_N(kCalls42BD30), nullptr, 0, nullptr, 0, OURS(Area191_ChoiceRun1), 0, false, S::kChoice},
};
enum : unsigned { k191Msg68, k191FocusPair, k191Msg6C, k191Tail53Choice, k191MemberNext, k191MemberNextRun, k191RunScale, k191ScaleStart,
                  k191ScaleGrow, k191ScaleShrink, k191Tail53, k191Step, k191Talk, k191TalkB, k191Init, k191Kind18, k191Run1 };
#undef AH_N
#undef OURS

const ah::DataTable kTables189[] = {{at::kArea189LeaderStates, at::kArea189LeaderStateCount}};
const ah::DataTable kTables191[] = {{at::kArea191ScaleStates, at::kArea191ScaleStateCount}};

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
unsigned char& B(U address) { return *ah::Mem(address); }

// Which area and function the round is running (set by the seeds).
int g_area = 0;
unsigned g_k = 0;

// ---- pointers the areas follow ----

// A record the script object, the focus object or the active member may name:
// a field object, one of Sprite_ObjectsExtra's four, or a party record.
unsigned char* AnyRecord(U v) {
    switch (v % 3) {
    case 0: return ah::Object(v >> 2);
    case 1: return ah::Mem(ah::at::kObjectsExtra + (v >> 2) % 4 * ah::at::kObjectStride);
    default: return ah::PartyOf(static_cast<unsigned char>(v >> 2));
    }
}
void MoveCurrent(U n) { Sprite_Current = n & 0x100 ? ah::PartyOf(static_cast<unsigned char>(n >> 9)) : ah::Object((n >> 9) & 3); }

// ---- the stand-ins the group lists ----

constexpr U kAll = 0xFFFFFFFFu, kU8 = 0xFFu, kU16 = 0xFFFFu;

// Louder than the real callees, on purpose (each only part of the time, from
// Noise): the callers read cells again after these calls.
U MovesCurrent(const U*, U answer) {
    if (ah::Noise() & 1) MoveCurrent(ah::Noise());
    return answer;
}
U MovesScriptObject(const U*, U answer) {
    const U n = ah::Noise();
    if (n & 1) ah::SetPointer(at::kScriptObject, AnyRecord(n >> 8));
    if (n & 2) MoveCurrent(n >> 4);
    return answer;
}
// ScriptFlags_Set40: area 191's handlers 0 and 1 read Field_ActiveMember
// after it.
U Set40Effect(const U*, U answer) {
    const U n = ah::Noise();
    if (n & 1) ah::SetPointer(at::kActiveMember, AnyRecord(n >> 8));
    if (n & 2) MoveCurrent(n >> 4);
    return answer;
}
// ScriptFlags_Clear40: tail kind 50 reads Field_StatusBits and the state after
// it, tail kind 53 the saved place.
U Clear40Effect(const U*, U answer) {
    const U n = ah::Noise();
    if (n & 1) Field_StatusBits = static_cast<unsigned char>(n >> 8);
    if (n & 2) B(at::kTailState) = static_cast<unsigned char>(n & 0x400 ? 0 : n >> 16);
    if (n & 4) {
        SetLong(ah::Mem(at::kSavedX), static_cast<std::int32_t>(ah::Noise()));
        SetWord(ah::Mem(at::kSavedArea), n >> 12);
    }
    return answer;
}
// Msg_OpenScript: area 189's arrival reads its frame word again after message
// 5.
U MessageEffect(const U*, U answer) {
    const U n = ah::Noise();
    if (n % 4 == 0) SetWord(ah::Mem(at::kWalkFrames), n & 0x100 ? 0x3C0u : n >> 16);
    return answer;
}
// Sound_PlayEffect: area 189's arrival reads 0x9036D0 after the small event's
// sound, its menu button Sprite_Current.
U SoundEffect(const U*, U answer) {
    const U n = ah::Noise();
    if (n & 1) B(at::kEventsSince) = static_cast<unsigned char>(n >> 8);
    if (n & 2) MoveCurrent(n >> 4);
    return answer;
}
// Flags_Set: area 189's arrival reads Sprite_Current after each mark, tail
// kind 53 the walk count and Field_StatusBits after flag 0x82.
U FlagsSetEffect(const U*, U answer) {
    const U n = ah::Noise();
    if (n & 1) MoveCurrent(n >> 4);
    if (n & 2) B(at::kWalkCount) = static_cast<unsigned char>(n >> 16);
    if (n & 4) Field_StatusBits = static_cast<unsigned char>(n >> 24);
    return answer;
}
// Flags_Clear: tail kind 53's state 0x1E reads Field_StatusBits after it.
U FlagsClearEffect(const U*, U answer) {
    const U n = ah::Noise();
    if (n & 1) Field_StatusBits = static_cast<unsigned char>(n >> 8);
    if (n & 2) MoveCurrent(n >> 4);
    return answer;
}
// Scenario_ArriveHook: all of eax - 0, a low byte of 0 above a set bit, or
// any; Sprite_Current moved (the arrival reads it again after).
U ArriveAnswer(const U*, U answer) {
    const U n = ah::Noise();
    if (n & 0x10) MoveCurrent(n >> 8);
    const U m = n % 4;
    return m < 2 ? 0u : m == 2 ? 0x100u : (answer | 1u);
}
// Sound_StreamDone: all of eax tested - 0, a low byte of 0 above a set bit,
// or any.
U StreamAnswer(const U*, U answer) {
    const U m = ah::Noise() % 3;
    return m == 0 ? 0u : m == 1 ? 0x100u : (answer | 1u);
}
// AreaMap_ByteAt under area 191's step: 0xA6 often, its neighbours, or any.
U CellA6Answer(const U*, U answer) {
    const U n = ah::Noise();
    static const U kC[] = {0xA6, 0xA6, 0xA6, 0xA5, 0xA7, 0x26};
    return (answer & 0xFFFFFF00u) | (n % 5 == 0 ? (n >> 8) & 0xFF : kC[(n >> 4) % 6]);
}
// 0x511C10: the height in ax; Sprite_Current moved and its +9 changed now and
// then (area 189's step reads both after the call), +9 kept above 0 (the
// original divides by it).
U HeightEffect(const U*, U answer) {
    const U n = ah::Noise();
    if (n & 1) MoveCurrent(n >> 4);
    if (n & 2) Sprite_Current[9] = static_cast<unsigned char>(1 + (n >> 16) % 9);
    if (Sprite_Current[9] == 0) Sprite_Current[9] = 1;
    return answer;
}
// Gte_RotTransPers writes the screen point (two dwords) at its second
// argument: random, the y a signalling NaN a quarter of the time (the copy
// through the x87 quiets it); Sprite_Current moved now and then.
U ProjectEffect(const U* a, U answer) {
    const U n = ah::Noise();
    if (a[1] == at::kScreenXY) {
        ah::FillBytes(ah::Mem(at::kScreenXY), 8);
        if (n % 4 == 0) {
            static const U kNaN[] = {0x7F800001u, 0x7FBFFFFFu, 0xFF800001u, 0x7FC00000u, 0x7F800000u, 0x00000001u};
            SetLong(ah::Mem(at::kScreenY), static_cast<std::int32_t>(kNaN[(n >> 4) % 6]));
        }
    }
    if (n & 0x100) MoveCurrent(n >> 9);
    return answer;
}
// Area189_StepBegin sets +9 and the speeds the control reads after: the
// stand-in sets +9 to 0..8 and the speeds small, part of the time.
U StepBeginEffect(const U*, U answer) {
    const U n = ah::Noise();
    if (n & 1) Sprite_Current[9] = static_cast<unsigned char>((n >> 8) % 9);
    if (n & 2) {
        static const std::int32_t kV[] = {0, 0x2000, -0x2000, 0x10000, -0x10000, 0x800, -0x800};
        SetLong(Sprite_Current + 0xC, kV[(n >> 12) % 7]);
        SetLong(Sprite_Current + 0x10, kV[(n >> 16) % 7]);
    }
    if (n & 4) MoveCurrent(n >> 20);
    return answer;
}
// Area189_StepMove: the control writes Field_State +0x137 after it.
U StepMoveEffect(const U*, U answer) {
    const U n = ah::Noise();
    if (n & 1) ah::SetPointer(ah::at::kFieldState, ah::PartyOf(static_cast<unsigned char>(n >> 8)));
    if (n & 2) MoveCurrent(n >> 4);
    return answer;
}
// Char_RecalcStats: area 189's raise reads the next record's byte after it.
U RecalcEffect(const U*, U answer) {
    const U n = ah::Noise();
    if (n & 1) B(at::kCharByte1E + (n >> 8) % at::kCharWalked * at::kCharStride) = static_cast<unsigned char>(n >> 16);
    return answer;
}

#define W4E_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
#define W4E_THEIRS(name) #name, KeyOf(name), KeyOf(name)
const ah::Callee kCallees[] = {
    {W4E_OURS(ScriptFlags_Set40), 0, {}, ah::Answer::kGarbage, 0, 0, {}, &Set40Effect},
    {W4E_OURS(ScriptFlags_Clear40), 0, {}, ah::Answer::kGarbage, 0, 0, {}, &Clear40Effect},
    {W4E_OURS(Msg_OpenScript), 1, {kU16}, ah::Answer::kGarbage, 0, 0, {}, &MessageEffect},
    {W4E_OURS(Sound_PlayEffect), 1, {kU16}, ah::Answer::kGarbage, 0, 0, {}, &SoundEffect},
    {W4E_OURS(Flags_Set), 2, {kAll, kU8}, ah::Answer::kGarbage, 0, 0, {}, &FlagsSetEffect},
    {W4E_OURS(Flags_Clear), 2, {kAll, kU8}, ah::Answer::kGarbage, 0, 0, {}, &FlagsClearEffect},
    {W4E_OURS(Flags_Toggle), 2, {kAll, kU8}, ah::Answer::kGarbage, 0, 0},
    {W4E_OURS(KeyItem_Has), 1, {kAll}, ah::Answer::kFlag, 0, 0},
    // the area a word, the flags a byte (the pushes carry stale bits above them)
    {W4E_OURS(Field_ChangeArea), 4, {kU16, kAll, kAll, kU8}, ah::Answer::kGarbage, 0, 0},
    {W4E_THEIRS(MoveCmd_Move), 2, {kAll, kU8}, ah::Answer::kGarbage, 0, 0, {}, &MovesScriptObject},
    // slots inside the group's four effect records, or none
    {W4E_OURS(Effect_FindFree), 0, {}, ah::Answer::kByte, 0xFF, 0x03},
    {W4E_OURS(Party_Count), 1, {kAll}, ah::Answer::kByte, 0x00, 0x03},
    {W4E_OURS(Actor_EquipCount), 3, {kU8, kAll, kAll}, ah::Answer::kFlag, 0, 0},
    {W4E_OURS(Gte_RotTransPers), 3, {kAll, kAll, 0}, ah::Answer::kGarbage, 0, 0, {6}, &ProjectEffect},
    {W4E_OURS(Gte_StoreDepthF), 1, {kAll}, ah::Answer::kGarbage, 0, 0, {}, &MovesCurrent},
    {W4E_OURS(Scenario_ArriveHook), 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &ArriveAnswer},
    {W4E_OURS(Scena14_LeaveToC4), 0, {}, ah::Answer::kGarbage, 0, 0},
    {W4E_OURS(Char_RecalcStats), 1, {kAll}, ah::Answer::kGarbage, 0, 0, {}, &RecalcEffect},
    {W4E_OURS(Transition_Start), 1, {kU8}, ah::Answer::kGarbage, 0, 0},
    {W4E_THEIRS(Sound_StopMusic), 0, {}, ah::Answer::kGarbage, 0, 0},
    {W4E_OURS(Sound_LoadStream), 1, {kAll}, ah::Answer::kGarbage, 0, 0},
    {W4E_OURS(Sound_StreamDone), 0, {}, ah::Answer::kGarbage, 0, 0, {}, &StreamAnswer},
    {W4E_OURS(AreaMap_ByteAt), 2, {kU16, kU16}, ah::Answer::kGarbage, 0, 0, {}, &CellA6Answer},
    {W4E_OURS(Sprite_FindFree), 0, {}, ah::Answer::kByte, 0xFF, 0x1D},
    {W4E_OURS(EventOp_0x), 1, {kAll}, ah::Answer::kGarbage, 0, 0},
    {W4E_OURS(Effect_Release), 0, {}, ah::Answer::kGarbage, 0, 0},
    // Capcom's, unnamed: the height (x, z); area 192's record restore
    {"HeightAt_511C10", at::kHeightAt, at::kHeightAt, 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &HeightEffect},
    {"RestoreRecords_42C2D0", at::kRestoreRecords, at::kRestoreRecords, 0, {}, ah::Answer::kGarbage, 0, 0},
    // the group's own, called directly
    {W4E_OURS(Area189_ZeroSpeeds), 0, {}, ah::Answer::kGarbage, 0, 0, {}, &MovesCurrent},
    {W4E_OURS(Area189_LeaderHalt), 0, {}, ah::Answer::kGarbage, 0, 0},
    {W4E_OURS(Area189_ExitButton), 0, {}, ah::Answer::kFlag, 0, 0},
    {W4E_OURS(Area189_MenuButton), 0, {}, ah::Answer::kFlag, 0, 0},
    {W4E_OURS(Area189_TurnInput), 0, {}, ah::Answer::kByte, 0x00, 0x03},
    {W4E_OURS(Area189_StepBegin), 0, {}, ah::Answer::kGarbage, 0, 0, {}, &StepBeginEffect},
    {W4E_OURS(Area189_StepMove), 0, {}, ah::Answer::kGarbage, 0, 0, {}, &StepMoveEffect},
    {W4E_OURS(Area189_StepArrive), 0, {}, ah::Answer::kGarbage, 0, 0},
    {W4E_OURS(Area189_LeaderControl), 0, {}, ah::Answer::kGarbage, 0, 0},
    {W4E_OURS(Area189_RaiseByte1E), 0, {}, ah::Answer::kGarbage, 0, 0},
    {W4E_OURS(Area189_DrainHp), 0, {}, ah::Answer::kGarbage, 0, 0},
    // a, who, b, c: the three bytes read (the pushes carry stale bits), who
    // not at all
    {W4E_OURS(Area191_TalkMessageB), 4, {kU8, 0, kU8, kU8}, ah::Answer::kGarbage, 0, 0},
};
#undef W4E_OURS
#undef W4E_THEIRS

// ---- regions, per area (the harness holds 40 with its twenty) ----

const ah::Region kRegions188[] = {
    {at::kEffectObjects, at::kEffectCount * at::kEffectStride},
    {at::kCameraShiftY, 6},        // Camera_ShiftY 0x903802 and the focus pointer 0x903804
    {at::k929F0F, 1},
    {at::kScriptObject, 4},
};
const ah::Region kRegions189[] = {
    {at::kEffectObjects, at::kEffectCount * at::kEffectStride},
    {at::kPassFlags, 1},
    {at::kCondByteFF, 0xE},       // Cond_ByteFF 0x7E1BE2 .. Input_Held 0x7E1BE8 .. Input_Pressed 0x7E1BEC
    {at::kVertexScratch, 6},
    {at::kScreenXY, 8},
    {at::kButtonMap0, 8},          // the button words 0x903580 .. Field_MenuButton 0x903584
    {at::kStepsSince, 1},
    {at::kEventsSince, 1},
    {at::kCondByteFE, 1},
    {at::kCameraAngles, 8},        // Camera_Angles[0..1], Cond_AngleFB
    {at::kCharRecords, 8 * at::kCharStride},
    {at::kScriptObject, 4},
};
const ah::Region kRegions191[] = {
    {at::kActiveMember, 4},
    {at::kFlagRow, 4},
    {at::kWaitWordDA, 2},
    {at::kPassFlags, 1},
    {at::kStepsSince, 1},
    {at::kEventsSince, 1},
    {at::kCameraShiftY, 6},        // the focus pointer 0x903804
    {at::kCharRecords, 8 * at::kCharStride},
    {at::kScriptObject, 4},
};

// Every round: the pointers the areas follow put back inside the regions.
void Common(int area, unsigned k) {
    g_area = area;
    g_k = k;
    ah::SetPointer(at::kScriptObject, AnyRecord(ah::Next()));
    ah::SetPointer(at::kFocusObject, AnyRecord(ah::Next()));
    ah::SetPointer(at::kActiveMember, AnyRecord(ah::Next()));
    ah::SetPointer(at::kFlagRow, ah::Mem(ah::at::kCondFlags + (ah::Next() % 24) * 8));
}

// The group's cells, moved by the harness's disturbance about one call in
// 24 - drawn only from h (area_harness.h: a group disturb never draws Next).
void Disturb(U h) {
    const auto v = static_cast<unsigned char>(h >> 20);
    switch ((h >> 8) % 9) {
    case 0: B(at::kTailState) = static_cast<unsigned char>(h & 0x100 ? v % 0x20 : v); break;
    case 1: B(at::kCounter3) = static_cast<unsigned char>(h & 0x100 ? (v & 1 ? 0 : 0x28) : v); break;
    case 2: SetWord(ah::Mem(at::kWalkFrames), h & 0x100 ? (v & 1 ? 0x1DFu : 0x3BFu) : h >> 16); break;
    case 3: B(at::kWalkReserve) = static_cast<unsigned char>(h & 0x100 ? v % 4 : v); break;
    case 4: B(at::kStepsSince) = v; break;
    case 5: MoveScript_WaitWordDA = static_cast<unsigned short>(h & 0x100 ? 0 : v); break;
    case 6: B(at::kChoiceAnswer) = static_cast<unsigned char>(v % 3); break;
    case 7: Cond_ByteFF = static_cast<unsigned char>(h & 0x100 ? v & 1 : v); break;
    default: ah::SetPointer(at::kActiveMember, AnyRecord(h >> 12)); break;
    }
}
// After every disturbance: area 189's step divides by +9, read after its
// height call; the original faults on 0 as ours aborts, so the fuzz keeps it
// above 0 there.
void Settle() {
    if (g_area == 189 && g_k == k189StepBegin && Sprite_Current[9] == 0) Sprite_Current[9] = 1;
}

// A choice answer: 0 often, 1, 2, a negative byte, anything.
void SeedAnswer() {
    if (ah::Often()) B(at::kChoiceAnswer) = static_cast<unsigned char>(AH_PICK(0, 0, 1, 1, 2, 3, 5, 6, 0xFF, 0x80, 0x7F));
}
void SeedChapter() {
    if (ah::Often()) Cond_ByteFA = static_cast<signed char>(AH_PICK(11, 12, 12, 13, 13, 0, 0x7F, 0x80, 0xFF));
}
void SeedRequest() { Field_Request = static_cast<unsigned char>(ah::Next() % 3 == 0 ? 2 : AH_PICK(0, 0, 1, 3, 0x82)); }
// A 16.16 word with the high word `high` and any low word.
U At16(U high, U low) { return (high & 0xFFFF) << 16 | (low & 0xFFFF); }
// A block (high word with the low byte cleared) on or about `b`: the block
// with any low byte, the edges either side, a high bit.
U NearBlock(U b) {
    switch (ah::Next() % 6) {
    case 0: case 1: case 2: return b | (ah::Next() & 0xFF);
    case 3: return b - 1;
    case 4: return b + 0x100;
    default: return b | 0x8000;
    }
}
void SetHigh(unsigned char* p, U high) { SetWord(p + 2, high); }

// ---- area 188 ----
void Seed188(unsigned k) {
    Common(188, k);
    switch (k) {
    case k188Tail43A: case k188Tail43B: SeedAnswer(); break;
    case k188ByChapter: case k188FocusPair: SeedAnswer(); SeedChapter(); break;
    case k188Walk: {
        // the running object's z at, below and above the limit less the
        // direction record's dword
        if (ah::Often()) B(at::kTailSub) = static_cast<unsigned char>(AH_PICK(0, 1, 0, 1, 2, 0xFF));
        if (!ah::Often()) break;
        const U dir = static_cast<U>(Long(ah::Mem(at::kDirections + (Sprite_Current[8] & 7u) * 8)));
        const U limit = static_cast<U>(B(at::kArea188ZLimits + B(at::kTailSub))) << 16;
        static const U kOff[] = {0, 1, 0xFFFFFFFFu, 0x10000, 0xFFFF0000u, 0x80000000u, 2};
        SetLong(Sprite_Current + 0x38, static_cast<std::int32_t>(limit - dir + kOff[ah::Next() % 7]));
        break;
    }
    case k188Tail43: {
        B(at::kTailState) = static_cast<unsigned char>(AH_PICK(0, 1, 2, 0xA, 0xB, 0xC, 0xD, 0, 1, 2, 0xA, 0xB, 0xC, 0xD, 3, 9, 0xE, 0x80, 0xFF, 0x8A));
        SeedRequest();
        if (ah::Often()) B(at::kCounter3) = static_cast<unsigned char>(AH_PICK(0x28, 0, 0x28, 0, 0x27, 0x29, 1, 0xA8));
        if (ah::Often()) B(at::kTailSub) = static_cast<unsigned char>(AH_PICK(0, 1, 0, 1, 2, 0x80));
        break;
    }
    case k188Init:
        if (ah::Often()) B(at::kMode905E68) = static_cast<unsigned char>(AH_PICK(1, 4, 1, 4, 0, 2, 5, 0x81));
        if (ah::Often()) Cond_ByteFD = static_cast<unsigned char>(AH_PICK(4, 1, 4, 1, 0, 2, 5, 0x84));
        break;
    default: break;
    }
}

// ---- area 189 ----
void SeedFlags189() {
    if (ah::Often()) Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags & ~0x1E0u);
    if (ah::Often()) Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 & ~0x40u);
}
void Seed189(unsigned k) {
    Common(189, k);
    switch (k) {
    case k189Tail50A: case k189Tail50B: SeedAnswer(); break;
    case k189Tail50:
        SeedRequest();
        if (ah::Often()) B(at::kTailState) = static_cast<unsigned char>(AH_PICK(0, 1, 0, 1, 0x80, 0xFF));
        break;
    case k189Run: Sprite_Current[2] = static_cast<unsigned char>(ah::Next() % at::kArea189LeaderStateCount); break;
    case k189Start:
        if (ah::Often()) Cond_ByteFF = static_cast<unsigned char>(AH_PICK(0, 1, 0, 0x80));
        if (ah::Half()) B(at::kWalkFacing) = static_cast<unsigned char>(AH_PICK(0, 1, 2, 3, 0xF, 0x10, 0x12, 0xFF));
        break;
    case k189Project: {
        static const U kK[] = {0, 1, 0xFFFFFFFFu, 0x1000000, 0x1000001, 0xFFFFFF, 0x80000000u, 0x200, 0x1FF};
        if (ah::Half()) Field_Kind2X = static_cast<long>(kK[ah::Next() % 9] + (ah::Half() ? 0 : ah::Next() & 0xFF000000u));
        if (ah::Half()) Field_Kind2Z = static_cast<long>(kK[ah::Next() % 9] + (ah::Half() ? 0 : ah::Next() & 0xFF000000u));
        if (ah::Half()) SetWord(Sprite_Current + 0x3E, AH_PICK(0, 1, 0xFFFF, 0x8000, 0x7FFF, 3, 0xFFFD));
        break;
    }
    case k189Control: {
        SeedFlags189();
        if (ah::Often()) Field_Request = 0;
        // the point +9 steps on about the two boxes: speeds small, x and z
        // blocks about their edges
        unsigned char* const cur = Sprite_Current;
        if (ah::Often()) {
            cur[9] = static_cast<unsigned char>(ah::Next() % 9);
            static const std::int32_t kV[] = {0, 0x2000, -0x2000, 0x10000, -0x10000, 0x800, -0x800};
            SetLong(cur + 0xC, kV[ah::Next() % 7]);
            SetLong(cur + 0x10, kV[ah::Next() % 7]);
            SetHigh(cur + 0x34, NearBlock(AH_PICK(0x1000, 0x1400, 0x1200, 0x1500, 0x1900, 0x1700, 0xF00, 0x1A00)));
            SetHigh(cur + 0x38, NearBlock(AH_PICK(0x1800, 0x1700, 0x1A00, 0xE00, 0xF00, 0xC00, 0x1000)));
        }
        break;
    }
    case k189Step: if (ah::Half()) Sprite_Current[9] = static_cast<unsigned char>(AH_PICK(0, 0, 1, 2, 0xFF)); break;
    case k189Turn: {
        unsigned char* const cur = Sprite_Current;
        if (ah::Often()) cur[0xB] = static_cast<unsigned char>(AH_PICK(0x10, 0xF0, 0x10, 0xF0, 0, 0x80, 0x7F));
        if (ah::Often()) {
            // the angle one turn short of the facing's target, or on it
            const U target = ((2u - cur[8]) & 0xF) << 8;
            const U d = static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(cur[0xB])));
            const U base = (target - d + (ah::Half() ? 0 : AH_PICK(1, 0xFFF, 0x1000, 0x100))) & (ah::Half() ? 0xFFFu : 0xFFFFu);
            Cond_AngleFB = static_cast<unsigned long>(base | (ah::Half() ? 0 : ah::Next() & 0xFFFF0000u));
        }
        break;
    }
    case k189StepBegin:
        if (ah::Often()) Sprite_Current[8] = static_cast<unsigned char>(ah::Next() % 8);
        break;
    case k189Arrive: {
        SeedFlags189();
        if (ah::Often()) SetWord(ah::Mem(at::kWalkFrames), AH_PICK(0x1DF, 0x3BF, 0x1DF, 0x3BF, 0x1E0, 0x3C0, 0, 0xFFFF, 0x1DE));
        if (ah::Often()) B(at::kWalkReserve) = static_cast<unsigned char>(AH_PICK(0, 0, 0, 1, 2, 3, 4, 8, 0xF8));
        if (ah::Often()) Cond_ByteFF = static_cast<unsigned char>(AH_PICK(0, 1, 0, 1, 2, 0x80));
        if (ah::Often()) B(at::kStepsSince) = static_cast<unsigned char>(ah::Next() % 32);
        if (ah::Often()) B(at::kEventsSince) = static_cast<unsigned char>(AH_PICK(3, 4, 5, 0, 3, 4, 0xFF));
        if (ah::Often()) Field_Request = static_cast<unsigned char>(AH_PICK(0, 0, 1, 2));
        unsigned char* const cur = Sprite_Current;
        if (ah::Often()) {
            static const U kMarks[][2] = {{0xB00, 0xF00}, {0x1500, 0x1100}, {0xC00, 0x1400}, {0x1000, 0x1100}, {0x1D00, 0x1700}, {0x1400, 0x1400},
                                          {0x1600, 0x1000}, {0x1700, 0xF00}, {0x1600, 0x1100}, {0x900, 0xD00}, {0x1F00, 0x1900}, {0x800, 0x1A00}};
            const U m = ah::Next() % 12;
            SetWord(cur + 0x36, kMarks[m][0] | (ah::Half() ? 0 : ah::Next() & 0xFF));
            SetWord(cur + 0x3A, kMarks[m][1] | (ah::Half() ? 0 : ah::Next() & 0xFF));
            if (ah::Half()) SetWord(cur + 0x36, Word(cur + 0x36) + (ah::Half() ? 0x100u : 0xFF00u));
        }
        if (ah::Often()) {
            // the step count on and about the pace (the arrival counts one first)
            const U pace = Word(ah::Mem(at::kLeaderPace));
            Field_EdgeBits = static_cast<unsigned short>(pace + AH_PICK(0xFFFE, 0xFFFF, 0, 1, 0xFFFE));
        }
        if (ah::Half()) Field_InputHeld = 0;
        break;
    }
    case k189Exit:
        if (ah::Half()) Input_Pressed = static_cast<unsigned short>(Input_Pressed | 0x800);
        if (ah::Often()) Cond_ByteFF = static_cast<unsigned char>(AH_PICK(0, 1, 0, 0x80));
        if (ah::Half()) Game_AreaNumber = static_cast<unsigned short>(ah::Next());
        break;
    case k189Menu:
        if (ah::Half()) Input_Pressed = static_cast<unsigned short>(Input_Pressed & ~Field_MenuButton);
        if (ah::Half()) Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags & ~0x40u);
        break;
    case k189TurnInput:
        if (ah::Often()) Input_Held = static_cast<unsigned short>(AH_PICK(0x1000, 0x2000, 0x8000, 0x3000, 0xA000, 0xB000, 0, 0x4000, 0x0F00));
        if (ah::Half()) Sprite_Current[8] = static_cast<unsigned char>(AH_PICK(0, 0xF, 0x10, 0xFF, 7, 8));
        break;
    case k189Raise:
        for (unsigned r = 0; r < at::kCharWalked; ++r)
            if (ah::Often()) B(at::kCharByte1E + r * at::kCharStride) = static_cast<unsigned char>(AH_PICK(8, 9, 10, 0, 0xFF, 7));
        break;
    case k189Drain:
        for (unsigned r = 0; r < at::kCharWalked; ++r) {
            if (!ah::Often()) continue;
            unsigned char* const w = ah::Mem(at::kCharWord18 + r * at::kCharStride);
            const U bound = Word(w + 0x28);
            const U d = (bound * 2u + 0x32u) / 100u;
            SetWord(w, d + AH_PICK(0, 1, 0xFFFFFFFFu, 2, 0) - (ah::Half() ? 0 : d));
        }
        break;
    default: break;
    }
}

// ---- area 191 ----
void Seed191(unsigned k) {
    Common(191, k);
    switch (k) {
    case k191Msg68: case k191FocusPair: case k191Msg6C: case k191Tail53Choice: case k191Run1: SeedAnswer(); break;
    case k191RunScale: Sprite_Current[4] = static_cast<unsigned char>(ah::Next() % at::kArea191ScaleStateCount); break;
    case k191ScaleGrow: case k191ScaleShrink:
        if (ah::Often()) Sprite_Current[0xA] = static_cast<unsigned char>(AH_PICK(1, 2, 3, 4, 5, 8, 0, 0xFF));
        break;
    case k191Tail53:
        B(at::kTailState) = static_cast<unsigned char>(
            AH_PICK(0, 2, 3, 4, 0xA, 0x14, 0x1E, 0, 2, 3, 4, 0xA, 0x14, 0x1E, 1, 5, 9, 0xB, 0x13, 0x15, 0x1D, 0x1F, 0x80, 0xFF, 0x82));
        SeedRequest();
        if (ah::Often()) MoveScript_WaitWordDA = static_cast<unsigned short>(AH_PICK(0, 0, 1, 0x100));
        break;
    case k191Talk:
        if (ah::Often()) B(at::kWalkCount) = static_cast<unsigned char>(ah::Next() % 11);
        for (unsigned r = 0; r < 8; ++r)
            if (ah::Often()) B(at::kCharByte1E + r * at::kCharStride) = static_cast<unsigned char>(AH_PICK(4, 5, 7, 8, 9, 0, 0xFF));
        break;
    case k191Init:
        if (ah::Half()) Cond_ByteFA = static_cast<signed char>(AH_PICK(0xE, 0xE, 0xD, 0xF, 0x8E));
        if (ah::Often()) B(at::kCharByte1E) = static_cast<unsigned char>(AH_PICK(4, 5, 8, 9, 10, 0, 0xFF));
        if (ah::Often()) B(at::kWalkCount) = static_cast<unsigned char>(AH_PICK(4, 5, 6, 0, 0xFF));
        break;
    default: break;
    }
}
void Args191(unsigned k, std::uint32_t* a) {
    switch (k) {
    case k191Step:
        if (!ah::Often()) return;
        a[0] = At16(a[0] >> 16, ah::Half() ? 0 : a[0]);
        a[1] = At16(a[1] >> 16, ah::Half() ? 0 : a[1]);
        break;
    case k191Talk:
        // the five keys (with stale bits above), their neighbours, anything
        if (ah::Often()) a[0] = AH_PICK(8, 4, 2, 5, 6, 8, 4, 2, 5, 6, 0, 1, 3, 7, 9) | (ah::Half() ? 0 : a[0] & 0xFFFFFF00u);
        break;
    case k191TalkB:
        a[0] = (a[0] & 0xFFFFFF00u) | (ah::Next() % 54);
        if (ah::Often()) a[2] = (a[2] & 0xFFFFFF00u) | (ah::Next() % 9);
        if (ah::Often()) a[3] = (a[3] & 0xFFFFFF00u) | AH_PICK(4, 5, 0, 3, 6, 8, 0xFF);
        break;
    default: break;
    }
}

void RunArea(int area, const ah::Clone* clones, unsigned n, const ah::DataTable* tables, unsigned n_tables, const ah::Region* regions,
             unsigned n_regions, void (*seed)(unsigned), void (*args)(unsigned, std::uint32_t*), unsigned rounds) {
    ah::Group g{"area_w4e", clones, n, kCallees, sizeof kCallees / sizeof kCallees[0], tables, n_tables,
                regions, n_regions, seed, &Disturb, rounds};
    g.settle = &Settle;
    g.args = args;
    g.area = area;
    ah::Run(g);
}

}  // namespace

void SelfTest() {
    constexpr unsigned kRounds = 6000;
    RunArea(188, kClones188, sizeof kClones188 / sizeof kClones188[0], nullptr, 0, kRegions188, sizeof kRegions188 / sizeof kRegions188[0],
            &Seed188, nullptr, kRounds);
    RunArea(189, kClones189, sizeof kClones189 / sizeof kClones189[0], kTables189, 1, kRegions189, sizeof kRegions189 / sizeof kRegions189[0],
            &Seed189, nullptr, kRounds);
    RunArea(191, kClones191, sizeof kClones191 / sizeof kClones191[0], kTables191, 1, kRegions191, sizeof kRegions191 / sizeof kRegions191[0],
            &Seed191, &Args191, kRounds);
}

}  // namespace area_w4e

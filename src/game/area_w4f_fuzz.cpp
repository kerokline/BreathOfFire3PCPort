// BOF3X_SHADOW=area_w4f: world 4's areas 192..199 through the area round's
// shared harness (area_harness.h), once at start-up - one area_harness::Run
// per area with code (192, 193, 196, 197, 198, 199), each Group setting its
// own area number, all under the one shadow name. docs/area_w4f.md section 3.
//
// The clone tables are tools/area_rows.py --clones's rows for AREA192..199
// (2026-09-28), each row read against the disassembly; two differ from the
// tool: area 192's init is cloned from its body (0x42C210: Capcom's own jmp
// over eleven nops opens it, which CloneOriginal refuses as already patched),
// and the tool's 0x42D4D0 (0x21B) is two functions, 0x42D4D0 (0xB0, ending in
// a jmp of displacement 0) and 0x42D580 (0x16B), the two entries of
// Area198_EffectA6States. The shapes are the root table each function hangs
// from (docs/area_w4f.md section 1). The group's own callees (area 192's
// restore and second talk table, area 198's drop and effect state 1) are
// recorders here like any other callee, so each function is fuzzed alone; the
// two two-state tables are swapped for recorders (DataTable). Nothing beyond
// the harness's API is built here (area_harness.h is not edited).
#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w4f.h"
#include "game/area_w4f_callees.h"
#include "game/move_script_bytes.h"
#include "hook/log.h"

namespace area_w4f {
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

// ---- area 192 ----
constexpr ah::CallSite kCalls42BDD0[] = {{0x12, 0x57C7C0}, {0x2A, 0x57C7C0}};
constexpr ah::CallSite kCalls42BE10[] = {{0x22, 0x4976D0}, {0x49, 0x495040}, {0x6F, 0x587B80}, {0x74, 0x42C2D0}, {0x7A, 0x587910},
                                         {0x8B, 0x587A00}, {0xA2, 0x57C0F0}, {0xE3, 0x57C7A0}, {0xFF, 0x594E00}, {0x11A, 0x57C110},
                                         {0x139, 0x57C7A0}, {0x157, 0x57C7A0}, {0x16D, 0x594E00}, {0x188, 0x57C110}};
constexpr ah::JumpTable kTables42BE10[] = {{0x1C, 0x1AC, 8}};
constexpr ah::CallSite kCalls42C000[] = {{0x10, 0x536700}, {0x2B, 0x536700}, {0x47, 0x536700}, {0x66, 0x536700}, {0x7C, 0x57C7C0}};
constexpr ah::CallSite kCalls42C0A0[] = {{0x6D, 0x57C140}, {0x8D, 0x42C1C0}, {0xE2, 0x57C140}};
// the init's body, 0x42C210 (the tool's offsets less 0x10)
constexpr ah::CallSite kCalls42C210[] = {{0xF, 0x57C140}, {0x26, 0x57C140}, {0x7A, 0x57CD90}, {0xA1, 0x57A010}};
constexpr ah::CallSite kCalls42C350[] = {{0x7, 0x57C140}, {0x2F, 0x589840}};
// ---- area 193 ----
constexpr ah::CallSite kCalls42C3B0[] = {{0x1B, 0x57C7C0}};
constexpr ah::CallSite kCalls42C3E0[] = {{0x21, 0x5919B0}, {0x31, 0x590BB0}, {0x3B, 0x587740}};
constexpr ah::CallSite kCalls42C4D0[] = {{0x1D, 0x57C7C0}};
constexpr ah::CallSite kCalls42C500[] = {{0x1C, 0x4976D0}, {0x47, 0x57C110}, {0x4C, 0x57C7A0}, {0x62, 0x594E00}, {0xC4, 0x57C110},
                                         {0xC9, 0x57C7A0}, {0xDF, 0x594E00}, {0x108, 0x57C7A0}, {0x12E, 0x57C140}, {0x140, 0x4976D0},
                                         {0x160, 0x533E50}, {0x167, 0x587910}, {0x177, 0x587A00}, {0x18E, 0x587AE0}, {0x195, 0x495040},
                                         {0x1A8, 0x57C110}, {0x1B7, 0x57C110}};
constexpr ah::JumpTable kTables42C500[] = {{0x16, 0x1CC, 13}};
constexpr ah::CallSite kCalls42C700[] = {{0x29, 0x57C7C0}};
constexpr ah::CallSite kCalls42C740[] = {{0xA, 0x57C140}};
// ---- areas 196 and 197 ----
constexpr ah::CallSite kCallsSearch[] = {{0x75, 0x4976D0}};
constexpr ah::CallSite kCalls42CD60[] = {{0x1, 0x589810}};
constexpr ah::CallSite kCalls42CDD0[] = {{0x1F, 0x57C0F0}, {0x38, 0x594E00}, {0x4A, 0x531F90}};
constexpr ah::CallSite kCalls42CE30[] = {{0x68, 0x5918E0}, {0x74, 0x57C7C0}, {0xA0, 0x5918E0}, {0xAC, 0x57C7C0}};
// ---- area 198 ----
constexpr ah::CallSite kCalls42CF00[] = {{0x6, 0x454A80}, {0x17, 0x455290}, {0x28, 0x5891F0}};
constexpr ah::CallSite kCalls42CF40[] = {{0x37, 0x589200}};
constexpr ah::CallSite kCalls42D000[] = {{0x2, 0x589810}, {0x54, 0x589810}, {0xA5, 0x589810}};
constexpr ah::CallSite kCalls42D100[] = {{0x78, 0x589810}};
constexpr ah::CallSite kCalls42D200[] = {{0x3A, 0x578C10}};
constexpr ah::CallSite kCalls42D250[] = {{0xF, 0x454A80}};
constexpr ah::CallSite kCalls42D280[] = {{0x2C, 0x587740}, {0x34, 0x42D2E0}, {0x42, 0x42D2E0}};
constexpr ah::CallSite kCalls42D2E0[] = {{0xA, 0x57CD90}, {0x28, 0x57A010}, {0x5A, 0x5B93D2}, {0x80, 0x5B93D2}, {0x94, 0x5B93D2}};
constexpr ah::CallSite kCalls42D3F0[] = {{0x0, 0x589810}};
constexpr ah::CallSite kCalls42D470[] = {{0x6, 0x454A80}, {0x17, 0x455290}, {0x30, 0x5891F0}};
// state 0: its two calls and its tail jmp into state 1 (displacement 0)
constexpr ah::CallSite kCalls42D4D0[] = {{0x65, 0x589590}, {0x9A, 0x5891F0}, {0xAB, 0x42D580}};
// state 1: the tool's offsets for 0x42D4D0 less 0xB0
constexpr ah::CallSite kCalls42D580[] = {{0x22, 0x5893A0}, {0x27, 0x5890E0}, {0xEB, 0x441090}, {0x111, 0x441090}, {0x137, 0x441090},
                                         {0x15E, 0x441090}};

const ah::Clone kClones192[] = {
    {"Area192_ChoiceFocusPair", 0x42BD60, 0x3C, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area192_ChoiceFocusPair), 0, false, S::kChoice},
    {"Area192_ChoiceTailState", 0x42BDA0, 0x2A, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area192_ChoiceTailState), 0, false, S::kChoice},
    {"Area192_ChoiceArmTail54", 0x42BDD0, 0x3E, kCalls42BDD0, AH_N(kCalls42BDD0), nullptr, 0, nullptr, 0, OURS(Area192_ChoiceArmTail54), 0, false, S::kChoice},
    {"Area192_Tail54", 0x42BE10, 0x1EB, kCalls42BE10, AH_N(kCalls42BE10), nullptr, 0, kTables42BE10, AH_N(kTables42BE10), OURS(Area192_Tail54), 0, false, S::kTail},
    {"Area192_StepHook", 0x42C000, 0x95, kCalls42C000, AH_N(kCalls42C000), nullptr, 0, nullptr, 0, OURS(Area192_StepHook), 0xFF, false, S::kHook},
    {"Area192_TalkMessage", 0x42C0A0, 0x11B, kCalls42C0A0, AH_N(kCalls42C0A0), nullptr, 0, nullptr, 0, OURS(Area192_TalkMessage), 0xFFFF, false, S::kCallee},
    {"Area192_TalkMessageB", 0x42C1C0, 0x3B, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area192_TalkMessageB), 0xFFFF, false, S::kCallee},
    {"Area192_Init", 0x42C210, 0xB4, kCalls42C210, AH_N(kCalls42C210), nullptr, 0, nullptr, 0, OURS(Area192_Init), 0, false, S::kInit},
    {"Area192_RestoreCharacters", 0x42C2D0, 0x79, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area192_RestoreCharacters), 0, false, S::kCallee},
    {"Area192_Effect18Release77", 0x42C350, 0x35, kCalls42C350, AH_N(kCalls42C350), nullptr, 0, nullptr, 0, OURS(Area192_Effect18Release77), 0, false, S::kState},
};
enum : unsigned { k192Focus, k192TailState, k192Arm54, k192Tail, k192Step, k192Talk, k192TalkB, k192Init, k192Restore, k192Eff18 };
const ah::Clone kClones193[] = {
    {"Area193_ChoiceMessage", 0x42C390, 0x17, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area193_ChoiceMessage), 0, false, S::kChoice},
    {"Area193_ChoiceRunOnYes", 0x42C3B0, 0x2F, kCalls42C3B0, AH_N(kCalls42C3B0), nullptr, 0, nullptr, 0, OURS(Area193_ChoiceRunOnYes), 0, false, S::kChoice},
    {"Area193_ChoiceFill5B", 0x42C3E0, 0x44, kCalls42C3E0, AH_N(kCalls42C3E0), nullptr, 0, nullptr, 0, OURS(Area193_ChoiceFill5B), 0, false, S::kChoice},
    {"Area193_ChoiceFocusPair", 0x42C430, 0x3C, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area193_ChoiceFocusPair), 0, false, S::kChoice},
    {"Area193_ChoiceMessageState", 0x42C470, 0x51, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area193_ChoiceMessageState), 0, false, S::kChoice},
    {"Area193_MemberBit0Leader7", 0x42C4D0, 0x2F, kCalls42C4D0, AH_N(kCalls42C4D0), nullptr, 0, nullptr, 0, OURS(Area193_MemberBit0Leader7), 0, false, S::kChoice},
    {"Area193_Tail57", 0x42C500, 0x200, kCalls42C500, AH_N(kCalls42C500), nullptr, 0, kTables42C500, AH_N(kTables42C500), OURS(Area193_Tail57), 0, false, S::kTail},
    {"Area193_StepHook", 0x42C700, 0x3F, kCalls42C700, AH_N(kCalls42C700), nullptr, 0, nullptr, 0, OURS(Area193_StepHook), 0xFF, false, S::kHook},
    {"Area193_Init", 0x42C740, 0x34, kCalls42C740, AH_N(kCalls42C740), nullptr, 0, nullptr, 0, OURS(Area193_Init), 0, false, S::kInit},
};
enum : unsigned { k193Message, k193RunOnYes, k193Fill, k193Focus, k193MsgState, k193Member7, k193Tail, k193Step, k193Init };
const ah::Clone kClones196[] = {
    {"Area196_MessageByMember0", 0x42C780, 0x8B, kCallsSearch, AH_N(kCallsSearch), nullptr, 0, nullptr, 0, OURS(Area196_MessageByMember0), 0, false, S::kHandler},
    {"Area196_MessageByMember1", 0x42C810, 0x8B, kCallsSearch, AH_N(kCallsSearch), nullptr, 0, nullptr, 0, OURS(Area196_MessageByMember1), 0, false, S::kHandler},
    {"Area196_SetCondFE", 0x42C8A0, 0x8, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area196_SetCondFE), 0, false, S::kHandler},
};
enum : unsigned { k196Search0, k196Search1, k196SetFE };
const ah::Clone kClones197[] = {
    {"Area197_MessageByMember0", 0x42C8B0, 0x8B, kCallsSearch, AH_N(kCallsSearch), nullptr, 0, nullptr, 0, OURS(Area197_MessageByMember0), 0, false, S::kHandler},
    {"Area197_MessageByMember1", 0x42C940, 0x8B, kCallsSearch, AH_N(kCallsSearch), nullptr, 0, nullptr, 0, OURS(Area197_MessageByMember1), 0, false, S::kHandler},
    {"Area197_MessageByMember2", 0x42C9D0, 0x8B, kCallsSearch, AH_N(kCallsSearch), nullptr, 0, nullptr, 0, OURS(Area197_MessageByMember2), 0, false, S::kHandler},
    {"Area197_MessageByMember3", 0x42CA60, 0x8B, kCallsSearch, AH_N(kCallsSearch), nullptr, 0, nullptr, 0, OURS(Area197_MessageByMember3), 0, false, S::kHandler},
    {"Area197_MessageByMember4", 0x42CAF0, 0x8B, kCallsSearch, AH_N(kCallsSearch), nullptr, 0, nullptr, 0, OURS(Area197_MessageByMember4), 0, false, S::kHandler},
    {"Area197_MessageByMember5", 0x42CB80, 0x8B, kCallsSearch, AH_N(kCallsSearch), nullptr, 0, nullptr, 0, OURS(Area197_MessageByMember5), 0, false, S::kHandler},
    {"Area197_MessageByMember6", 0x42CC10, 0x8B, kCallsSearch, AH_N(kCallsSearch), nullptr, 0, nullptr, 0, OURS(Area197_MessageByMember6), 0, false, S::kHandler},
    {"Area197_RunShake", 0x42CCA0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area197_RunShake), 0, false, S::kHandler},
    {"Area197_ShakeStart", 0x42CCC0, 0x22, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area197_ShakeStart), 0, false, S::kState},
    {"Area197_ShakeStep", 0x42CCF0, 0x62, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area197_ShakeStep), 0, false, S::kState},
    {"Area197_SpawnEffect92", 0x42CD60, 0x67, kCalls42CD60, AH_N(kCalls42CD60), nullptr, 0, nullptr, 0, OURS(Area197_SpawnEffect92), 0, false, S::kHandler},
    {"Area197_Tail51", 0x42CDD0, 0x5A, kCalls42CDD0, AH_N(kCalls42CDD0), nullptr, 0, nullptr, 0, OURS(Area197_Tail51), 0, false, S::kTail},
    {"Area197_StepHook", 0x42CE30, 0xC2, kCalls42CE30, AH_N(kCalls42CE30), nullptr, 0, nullptr, 0, OURS(Area197_StepHook), 0xFF, false, S::kHook},
};
enum : unsigned { k197Search0, k197Search1, k197Search2, k197Search3, k197Search4, k197Search5, k197Search6, k197RunShake, k197ShakeStart,
                  k197ShakeStep, k197Spawn92, k197Tail, k197Step };
const ah::Clone kClones198[] = {
    {"Area198_StartSlotScript0", 0x42CF00, 0x31, kCalls42CF00, AH_N(kCalls42CF00), nullptr, 0, nullptr, 0, OURS(Area198_StartSlotScript0), 0, false, S::kHandler},
    {"Area198_SinkFade", 0x42CF40, 0xBD, kCalls42CF40, AH_N(kCalls42CF40), nullptr, 0, nullptr, 0, OURS(Area198_SinkFade), 0, false, S::kHandler},
    {"Area198_SpawnEffectsA6", 0x42D000, 0xF9, kCalls42D000, AH_N(kCalls42D000), nullptr, 0, nullptr, 0, OURS(Area198_SpawnEffectsA6), 0, false, S::kHandler},
    {"Area198_Shake", 0x42D100, 0xFD, kCalls42D100, AH_N(kCalls42D100), nullptr, 0, nullptr, 0, OURS(Area198_Shake), 0, false, S::kHandler},
    {"Area198_WalkToX190", 0x42D200, 0x43, kCalls42D200, AH_N(kCalls42D200), nullptr, 0, nullptr, 0, OURS(Area198_WalkToX190), 0, false, S::kHandler},
    {"Area198_ReleaseOnRequest5", 0x42D250, 0x24, kCalls42D250, AH_N(kCalls42D250), nullptr, 0, nullptr, 0, OURS(Area198_ReleaseOnRequest5), 0, false, S::kHandler},
    {"Area198_SpawnDrops", 0x42D280, 0x53, kCalls42D280, AH_N(kCalls42D280), nullptr, 0, nullptr, 0, OURS(Area198_SpawnDrops), 0, false, S::kHandler},
    {"Area198_SpawnDrop", 0x42D2E0, 0x105, kCalls42D2E0, AH_N(kCalls42D2E0), nullptr, 0, nullptr, 0, OURS(Area198_SpawnDrop), 0, false, S::kCallee},
    {"Area198_SpawnEffectA7", 0x42D3F0, 0x80, kCalls42D3F0, AH_N(kCalls42D3F0), nullptr, 0, nullptr, 0, OURS(Area198_SpawnEffectA7), 0, false, S::kHandler},
    {"Area198_StartSlotScript11", 0x42D470, 0x39, kCalls42D470, AH_N(kCalls42D470), nullptr, 0, nullptr, 0, OURS(Area198_StartSlotScript11), 0, false, S::kHandler},
    {"Area198_EffectA6Run", 0x42D4B0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area198_EffectA6Run), 0, false, S::kCallee},
    {"Area198_EffectA6Start", 0x42D4D0, 0xB0, kCalls42D4D0, AH_N(kCalls42D4D0), nullptr, 0, nullptr, 0, OURS(Area198_EffectA6Start), 0, false, S::kState},
    {"Area198_EffectA6Follow", 0x42D580, 0x16B, kCalls42D580, AH_N(kCalls42D580), nullptr, 0, nullptr, 0, OURS(Area198_EffectA6Follow), 0, false, S::kState},
};
enum : unsigned { k198Slot0, k198Sink, k198SpawnA6, k198Shake, k198Walk, k198Release, k198Drops, k198Drop, k198SpawnA7, k198Slot11,
                  k198EffRun, k198EffStart, k198EffFollow };
const ah::Clone kClones199[] = {
    {"Area199_ChoiceStepIfAnswer", 0x42D6F0, 0x19, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area199_ChoiceStepIfAnswer), 0, false, S::kChoice},
};
#undef AH_N
#undef OURS

const ah::DataTable kTables197[] = {{at::kArea197ShakeStates, at::kStateCount}};
const ah::DataTable kTables198[] = {{at::kArea198EffectA6States, at::kStateCount}};

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
unsigned char& B(U address) { return *ah::Mem(address); }
unsigned char* EffectRecord(unsigned slot) { return ah::Mem(at::kEffectObjects + slot % at::kEffectCount * at::kEffectStride); }

// ---- pointers the areas follow ----

// A record the script object pointer may name: a field object or a party
// record.
unsigned char* ScriptRecord(U v) { return v & 1 ? ah::Object(v >> 1) : ah::PartyOf(static_cast<unsigned char>(v >> 1)); }
// The focus object: a field object, or one of Sprite_ObjectsExtra's four.
unsigned char* FocusRecord(U v) { return v % 5 == 0 ? ah::Mem(0x802000 + (v >> 3) % 4 * at::kObjectStride) : ah::Object(v >> 3); }
// A value of Field_ActiveMember: on a field object's record (0..6) two times in
// three, else between two or just below the first (its distance from
// Sprite_Objects is a signed quotient; its +0x80 and +0x8A, which some
// handlers write, stay inside the message and object regions either way).
U MemberValue(U v) {
    const U rec = (v >> 1) % 7;
    std::int32_t off = static_cast<std::int32_t>((v >> 4) % 0x147) - 0xA3;   // -0xA3..0xA3
    if ((v >> 13) % 3 != 0) off = 0;
    return at::kSpriteObjects + rec * at::kObjectStride + static_cast<U>(off);
}
void SetMember(U v) { SetLong(ah::Mem(at::kActiveMember), static_cast<std::int32_t>(MemberValue(v))); }
void MoveCurrent(U n) { Sprite_Current = n & 0x100 ? ah::PartyOf(static_cast<unsigned char>(n >> 9)) : ah::Object((n >> 9) & 3); }

// ---- the stand-ins the group lists ----

constexpr U kAll = 0xFFFFFFFFu, kU8 = 0xFFu, kU16 = 0xFFFFu;

// Louder than the real callees, on purpose (each only part of the time,
// from Noise): the callers read cells again after these calls.
// ScriptFlags_Set40: area 193's handler 0 reads Field_ActiveMember after it.
U Set40Effect(const U*, U answer) {
    const U n = ah::Noise();
    if (n & 1) SetMember(n >> 4);
    return answer;
}
// Flags_Set: area 192's tail reads Field_StatusBits after it (state 4).
U FlagsSetEffect(const U*, U answer) {
    const U n = ah::Noise();
    if (n & 1) Field_StatusBits = static_cast<unsigned char>(n >> 8);
    return answer;
}
// Effect_FindFree: the spawns read the active member after it; a slot of the
// first four records or none.
U EffectSlotEffect(const U*, U answer) {
    const U n = ah::Noise();
    if (n & 1) SetMember(n >> 4);
    return (answer & 0xFFFFFF00u) | (n % 3 == 0 ? 0xFFu : (n >> 8) % 4);
}
// Sprite_FindFree: none a third of the time, else a field object's index.
U SpriteSlotEffect(const U*, U answer) {
    const U n = ah::Noise();
    return (answer & 0xFFFFFF00u) | (n % 3 == 0 ? 0xFFu : (n >> 4) % 30);
}
// EventOp_0x: the placement makes the new object the running one (area 198's
// drop writes it after).
U EventOpEffect(const U*, U answer) {
    const U n = ah::Noise();
    if (n & 1) MoveCurrent(n >> 4);
    return answer;
}
// AreaMap_ByteAt: 0xA6 (area 192's hook counts it) half the time.
U ByteAtEffect(const U*, U answer) {
    const U n = ah::Noise();
    return n & 1 ? (answer & 0xFFFFFF00u) | 0xA6u : answer;
}
// Sprite_SetAnimationAt: area 198's sink reads MoveScript_Object again after
// it.
U MovesScriptObject(const U*, U answer) {
    const U n = ah::Noise();
    if (n & 1) ah::SetPointer(at::kScriptObject, ScriptRecord(n >> 8));
    return answer;
}
// Sprite_ScriptTick / Sprite_QueueOverlay: effect kind 0xA6's state 1 reads
// Sprite_Current for every store after them.
U MovesCurrent(const U*, U answer) {
    const U n = ah::Noise();
    if (n & 1) Sprite_Current = n & 2 ? EffectRecord(n >> 8) : ah::Object((n >> 8) & 3);
    return answer;
}

#define W4F_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
#define W4F_THEIRS(name) #name, KeyOf(name), KeyOf(name)
const ah::Callee kCallees[] = {
    {W4F_OURS(ScriptFlags_Set40), 0, {}, ah::Answer::kGarbage, 0, 0, {}, &Set40Effect},
    {W4F_OURS(ScriptFlags_Clear40), 0, {}, ah::Answer::kGarbage, 0, 0},
    {W4F_OURS(Transition_Start), 1, {kU8}, ah::Answer::kGarbage, 0, 0},
    {W4F_THEIRS(Sound_StopMusic), 0, {}, ah::Answer::kGarbage, 0, 0},
    {W4F_OURS(Sound_LoadStream), 1, {kAll}, ah::Answer::kGarbage, 0, 0},
    // tested as a whole eax: a flag's garbage above a 0 in al tells an al test
    {W4F_OURS(Sound_StreamDone), 0, {}, ah::Answer::kFlag, 0, 0},
    // tested in al: kFlag's garbage above a 0 tells an eax test (the standard
    // set's kBool would not)
    {W4F_OURS(Flags_Test), 2, {kAll, kU8}, ah::Answer::kFlag, 0, 0},
    {W4F_OURS(Flags_Set), 2, {kAll, kU8}, ah::Answer::kGarbage, 0, 0, {}, &FlagsSetEffect},
    {W4F_OURS(KeyItem_Has), 1, {kAll}, ah::Answer::kFlag, 0, 0},
    // the area as a word, the flags as a byte (Field_ChangeArea reads no more;
    // area 192's tail pushes the return point's area from cx)
    {W4F_OURS(Field_ChangeArea), 4, {kU16, kAll, kAll, kU8}, ah::Answer::kGarbage, 0, 0},
    {W4F_OURS(AreaMap_ByteAt), 2, {kU16, kU16}, ah::Answer::kGarbage, 0, 0, {}, &ByteAtEffect},
    {W4F_OURS(Sprite_FindFree), 0, {}, ah::Answer::kGarbage, 0, 0, {}, &SpriteSlotEffect},
    {W4F_OURS(EventOp_0x), 1, {kAll}, ah::Answer::kGarbage, 0, 0, {}, &EventOpEffect},
    {W4F_OURS(Effect_FindFree), 0, {}, ah::Answer::kGarbage, 0, 0, {}, &EffectSlotEffect},
    {W4F_OURS(Effect_Release), 0, {}, ah::Answer::kGarbage, 0, 0},
    {W4F_OURS(Party_HealJoined), 0, {}, ah::Answer::kGarbage, 0, 0},
    {W4F_OURS(Sprite_SetAnimationAt), 2, {kU8, kU16}, ah::Answer::kGarbage, 0, 0, {}, &MovesScriptObject},
    {W4F_OURS(Sprite_ScriptTick), 0, {}, ah::Answer::kFlag, 0, 0, {}, &MovesCurrent},
    {W4F_OURS(Sprite_QueueOverlay), 0, {}, ah::Answer::kGarbage, 0, 0, {}, &MovesCurrent},
    {W4F_THEIRS(MoveCmd_Move), 2, {kAll, kU8}, ah::Answer::kGarbage, 0, 0, {}, &MovesScriptObject},
    // Capcom's, unnamed (area_w4f_callees.h)
    {"SlotsRelease_454A80", at::kSlotsReleaseFor, at::kSlotsReleaseFor, 1, {kAll}, ah::Answer::kGarbage, 0, 0},
    {"SlotStart_455290", at::kSlotStart, at::kSlotStart, 2, {kAll, kAll}, ah::Answer::kByte, 0xFF, 0x07},
    {"RoundHigh_441090", at::kRoundHigh, at::kRoundHigh, 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0},
    // the group's own, called directly: the second talk table (the index,
    // rank and level as bytes; who is passed and unread), the restore, the
    // drop, effect kind 0xA6's state 1 (also its table's entry 1)
    {W4F_OURS(Area192_TalkMessageB), 4, {kU8, 0, kU8, kU8}, ah::Answer::kGarbage, 0, 0},
    {W4F_OURS(Area192_RestoreCharacters), 0, {}, ah::Answer::kGarbage, 0, 0},
    {W4F_OURS(Area198_SpawnDrop), 0, {}, ah::Answer::kGarbage, 0, 0},
    {W4F_OURS(Area198_EffectA6Follow), 0, {}, ah::Answer::kGarbage, 0, 0},
};
#undef W4F_OURS
#undef W4F_THEIRS

// ---- regions, per area (the harness holds 40 with its twenty) ----

// Every area's list opens with the five pointer and word cells the seeds, the
// louder stand-ins and the disturbance write (the harness puts back only its
// regions after the run), then the area's own.
#define W4F_BASE_REGIONS {at::kFocusObject, 4}, {at::kFlagRow, 4}, {at::kActiveMember, 4}, {at::kScriptObject, 4}, {at::kWaitWordDA, 2}
const ah::Region kRegions192[] = {
    W4F_BASE_REGIONS,
    {at::kCharRecords, at::kCharCount * at::kCharStride},
    {at::kByte929EC1, 1},
    {at::kByte9036D0, 1},
    {at::kDrawPassFlags, 1},
};
const ah::Region kRegions193[] = {
    W4F_BASE_REGIONS,
    {at::kByte929EC1, 1},
    {at::kByte9036D0, 1},
    {at::kDrawPassFlags, 1},
    {at::kPendingKind, 1},
};
const ah::Region kRegions196[] = {
    W4F_BASE_REGIONS,
    {at::kCondByteFE, 1},
};
// Areas 197 and 198 also hold the record an unchecked slot of 0xFF would
// write (Effect_Objects + 0xFF << 7, 0x7E9160: past the pool), so a spawn
// that took "none" for a slot is seen.
const ah::Region kRegions197[] = {
    W4F_BASE_REGIONS,
    {at::kEffectObjects, at::kEffectCount * at::kEffectStride},
    {at::kEffectObjects + 0xFF * at::kEffectStride, at::kEffectStride},
};
const ah::Region kRegions198[] = {
    W4F_BASE_REGIONS,
    {at::kEffectObjects, at::kEffectCount * at::kEffectStride},
    {at::kEffectObjects + 0xFF * at::kEffectStride, at::kEffectStride},
};
const ah::Region kRegions199[] = {
    W4F_BASE_REGIONS,
};
#undef W4F_BASE_REGIONS

// The area whose run this is (the group's disturbance writes one cell that is
// a region of one area only).
int g_area = 0;

// Every round: the pointers the areas follow put back inside the regions.
void Common(int area) {
    g_area = area;
    ah::SetPointer(at::kScriptObject, ScriptRecord(ah::Next()));
    ah::SetPointer(at::kFocusObject, FocusRecord(ah::Next()));
    SetMember(ah::Next());
    ah::SetPointer(at::kFlagRow, ah::Mem(ah::at::kCondFlags + (ah::Next() % 24) * 8));
}

// The group's cells, moved by the harness's disturbance about one call in
// 24 - drawn only from h (area_harness.h: a group disturb never draws Next).
void Disturb(U h) {
    const auto v = static_cast<unsigned char>(h >> 20);
    switch ((h >> 8) % 13) {
    // cells read on both sides of a call: the status bits (areas 192 and
    // 193's tails), character record 0's level byte (area 192's init)
    case 11: Field_StatusBits = v; break;
    case 12:
        // the character records are a region of area 192's run only
        if (g_area == 192) B(at::kChar0Byte1E) = static_cast<unsigned char>(h & 0x100 ? 4 + (v & 1) + (v & 2) * 2 : v);
        break;
    case 0: B(at::kTailState) = static_cast<unsigned char>(h & 0x100 ? v % 0x20 : v); break;
    case 1: B(at::kCounter0) = static_cast<unsigned char>(h & 0x100 ? v % 5 : v); break;
    case 2: B(at::kCounter1) = static_cast<unsigned char>(h & 0x100 ? 0 : v); break;
    case 3: B(at::kCounter3) = static_cast<unsigned char>(h & 0x100 ? 0x24 : v); break;
    case 4: ah::SetPointer(at::kScriptObject, ScriptRecord(h >> 16)); break;
    case 5: ah::SetPointer(at::kFocusObject, FocusRecord(h >> 16)); break;
    case 6: SetMember(h >> 12); break;
    case 7: B(at::kByte90405E) = static_cast<unsigned char>(h & 0x100 ? v % 10 : v); break;
    case 8: MoveScript_WaitWordDA = static_cast<unsigned short>(h & 0x100 ? 0 : v); break;
    case 9: B(at::kLeader89) = static_cast<unsigned char>(h & 0x100 ? 7 : v); break;
    default: B(at::kChoiceAnswer) = static_cast<unsigned char>(v % 3); break;
    }
}

// A choice answer: each of 0..n-1 often, a negative byte, one past, anything.
void SeedAnswer(unsigned n) {
    if (!ah::Often()) return;
    const U r = ah::Next() % 8;
    B(at::kChoiceAnswer) = static_cast<unsigned char>(r < 5 ? ah::Next() % n : r == 5 ? n : r == 6 ? 0xFF : ah::Next());
}
// A 16.16 word with the high word `high` and any low word, or a low word 0.
U At16(U high, U low) { return (high & 0xFFFF) << 16 | (ah::Half() ? 0 : low & 0xFFFF); }
// A high word on or beside [lo, lo + n): each inside, one either side, a high
// byte above it (the compares are 16-bit), anything.
U Around(U lo, unsigned n) {
    switch (ah::Next() % 7) {
    case 0: case 1: case 2: case 3: return lo + ah::Next() % n;
    case 4: return ah::Half() ? lo - 1 : lo + n;
    case 5: return (lo + ah::Next() % n) | 0x100u;
    default: return ah::Next();
    }
}
// A running effect record (area 198's effect kind).
void SeedEffectCurrent() { Sprite_Current = EffectRecord(ah::Next() % 4); }
// Each party record's +0x89 one of `keys` (or beside one), the member count
// 1..3 (0 a tenth of the time).
void SeedMemberKeys(U keys) {
    for (unsigned m = 0; m < 3; ++m) {
        unsigned char* const record = ah::PartyOf(static_cast<unsigned char>(m));
        if (ah::Often()) record[0x89] = static_cast<unsigned char>(B(keys + ah::Next() % 4) + (ah::Often() ? 0 : 1));
    }
    if (ah::Next() % 10 == 0) Field_MemberCount = 0;
}
// A tail state from `states` (and its neighbours), Field_Request 2 or not.
void SeedTail(const U* states, unsigned n) {
    const U s = ah::Pick(states, n);
    B(at::kTailState) = static_cast<unsigned char>(ah::Often() ? s : s + (ah::Half() ? 1 : 0xFF));
    if (ah::Half()) Field_Request = static_cast<unsigned char>(AH_PICK(0, 2, 2, 1, 5, 0x82));
}

// ---- area 192 ----
void Seed192(unsigned k) {
    Common(192);
    switch (k) {
    case k192Focus: SeedAnswer(6); break;
    case k192TailState: SeedAnswer(3); break;
    case k192Arm54: SeedAnswer(4); break;
    case k192Tail: {
        static const U kStates[] = {0, 1, 2, 3, 4, 0xA, 0x14, 0x1E, 0, 2, 3, 4, 0xA, 0x14, 0x1E, 5, 9, 0xB, 0x13, 0x1F, 0x7F, 0x80, 0xFF};
        SeedTail(kStates, sizeof kStates / sizeof kStates[0]);
        if (ah::Half()) MoveScript_WaitWordDA = static_cast<unsigned short>(AH_PICK(0, 0, 1, 0x100));
        break;
    }
    case k192Talk:
    case k192TalkB:
    case k192Init: {
        // the rank byte around its compares (5, 7, 8), the level byte of
        // every record around its (5, 8, 9)
        if (ah::Often()) B(at::kByte90405E) = static_cast<unsigned char>(AH_PICK(4, 5, 6, 7, 8, 9, 0x80, 0xFF, 0));
        for (unsigned r = 0; r < at::kCharCount; ++r)
            if (ah::Often()) B(at::kCharRecords + r * at::kCharStride + 0x1E) = static_cast<unsigned char>(AH_PICK(3, 4, 5, 6, 7, 8, 9, 10, 0xFF));
        break;
    }
    case k192Restore:
        if (ah::Next() % 8 == 0) Field_MemberCount = 0;
        break;
    default: break;
    }
}
void Args192(unsigned k, std::uint32_t* a) {
    switch (k) {
    case k192Step:
        a[0] = At16(a[0] >> 16, a[0]);
        a[1] = At16(a[1] >> 16, a[1]);
        break;
    case k192Talk:
        // who: one of the five ids, an id beside, or any byte; garbage above
        if (ah::Often()) a[0] = (a[0] & 0xFFFFFF00u) | static_cast<U>(B(at::kArea192TalkWho + ah::Next() % at::kArea192TalkWhoCount) + (ah::Often() ? 0 : 1));
        break;
    case k192TalkB:
        a[0] = (a[0] & 0xFFFFFF00u) | (ah::Next() % 5) * 9;
        a[2] = (a[2] & 0xFFFFFF00u) | (ah::Often() ? ah::Next() % 9 : ah::Next() & 0xFF);
        a[3] = (a[3] & 0xFFFFFF00u) | AH_PICK(3, 4, 5, 6, 0x85, 0xFF);
        break;
    default: break;
    }
}

// ---- area 193 ----
void Seed193(unsigned k) {
    Common(193);
    switch (k) {
    case k193Message: SeedAnswer(6); break;
    case k193RunOnYes: case k193Fill: case k193Focus: SeedAnswer(4); break;
    // the stack table's three rows only (ours aborts past them)
    case k193MsgState: B(at::kChoiceAnswer) = static_cast<unsigned char>(ah::Next() % 3); break;
    case k193Member7:
        if (ah::Often()) B(at::kLeader89) = static_cast<unsigned char>(AH_PICK(7, 7, 6, 8, 0x87));
        break;
    case k193Tail: {
        static const U kStates[] = {0xA, 0xB, 0xC, 0xD, 0xE, 0xF, 0x10, 0x11, 0x14, 0x15, 0x16, 0xA, 0xC, 0xE, 0x10, 0x14, 0x15, 0x16, 9, 0x17, 0, 0x80, 0xFF};
        SeedTail(kStates, sizeof kStates / sizeof kStates[0]);
        break;
    }
    default: break;
    }
}
void Args193(unsigned k, std::uint32_t* a) {
    if (k != k193Step || !ah::Often()) return;
    static const U kCells[] = {0x18000, 0x338000, 0x18000, 0x338000, 0x17FFF, 0x18001, 0x337FFF, 0x338001, 0x1018000, 0};
    for (unsigned i = 0; i < 2; ++i)
        if (ah::Half()) a[i] = ah::Pick(kCells, sizeof kCells / sizeof kCells[0]);
}

// ---- areas 196 and 197 ----
void Seed196(unsigned k) {
    Common(196);
    if (k == k196Search0) SeedMemberKeys(at::kArea196Keys0);
    if (k == k196Search1) SeedMemberKeys(at::kArea196Keys1);
}
void Seed197(unsigned k) {
    Common(197);
    if (k <= k197Search6) {
        SeedMemberKeys(at::kArea197Keys0 + k * at::kKeysStride);
        return;
    }
    switch (k) {
    case k197RunShake: Sprite_Current[4] = static_cast<unsigned char>(ah::Next() % at::kStateCount); break;
    case k197ShakeStep:
        if (ah::Often()) Sprite_Current[0xA] = static_cast<unsigned char>(AH_PICK(0, 0, 1, 2, 3, 4, 5, 0x10, 0x14, 0xFF));
        break;
    case k197Tail:
        if (ah::Often()) B(at::kTailState) = static_cast<unsigned char>(AH_PICK(0, 1, 1, 0, 1, 2, 0xFF, 0x80, 0x1E));
        if (ah::Often()) B(at::kCounter3) = static_cast<unsigned char>(AH_PICK(0x24, 0x24, 0x23, 0x25, 0xA4));
        break;
    case k197Step:
        if (ah::Often()) Cond_ByteFD = static_cast<unsigned char>(AH_PICK(4, 4, 4, 3, 5, 0x84));
        break;
    default: break;
    }
}
void Args197(unsigned k, std::uint32_t* a) {
    if (k != k197Step || !ah::Often()) return;
    // one of the three shapes: a z row with x's high word, an x row with z's,
    // the key-item row - each on or beside its values
    static const U kZ[] = {0x708000, 0x738000, 0x708000, 0x738000, 0x708001, 0x738000 - 1, 0x1708000};
    static const U kX[] = {0x28000, 0x58000, 0xD8000, 0x28000, 0x58000, 0xD8000, 0x28001, 0xD7FFF, 0x10D8000};
    if (ah::Half()) {
        a[1] = ah::Pick(kZ, sizeof kZ / sizeof kZ[0]);
        a[0] = (Around(3, 3) & 0xFFFF) << 16 | (a[0] & 0xFFFF);
    } else {
        a[0] = ah::Pick(kX, sizeof kX / sizeof kX[0]);
        a[1] = (Around(0x71, 3) & 0xFFFF) << 16 | (a[1] & 0xFFFF);
    }
}

// ---- area 198 ----
void Seed198(unsigned k) {
    Common(198);
    switch (k) {
    case k198Sink: {
        unsigned char* const object = ah::Pointer(at::kScriptObject);
        if (ah::Often()) object[2] = static_cast<unsigned char>(AH_PICK(0, 4, 8, 0xC, 1, 2, 3, 0x7C, 0x80, 0x84, 0xFF));
        // the word +0x3E, after its - 0x10, on and beside each compare
        static const U kHeights[] = {0xFD80, 0xFD7F, 0xFD81, 0xF600, 0xF5FF, 0xF601, 0xF380, 0xF37F, 0xF381, 0, 0x8000, 0x7FFF};
        if (ah::Often()) SetWord(Sprite_Current + 0x3E, static_cast<unsigned>(ah::Pick(kHeights, sizeof kHeights / sizeof kHeights[0]) + 0x10));
        for (unsigned off = 0x5D; off <= 0x5F; ++off)
            if (ah::Half()) Sprite_Current[off] = static_cast<unsigned char>(AH_PICK(0x80, 0x81, 0x7F, 0, 0xFF));
        break;
    }
    case k198Shake:
        if (ah::Half()) B(at::kCounter1) = 0;
        for (unsigned off = 0x5D; off <= 0x5F; ++off)
            if (ah::Half()) Sprite_Current[off] = static_cast<unsigned char>(AH_PICK(0, 0, 8, 0xF8, 0xC0));
        break;
    case k198Walk: {
        static const std::int32_t kOff[] = {0, 1, 0x7FFF, 0x8000, -1, -0x8000, -0x8001, 0x10000, -0x10000, 0x7FFFFF};
        if (ah::Often()) SetLong(Sprite_Current + 0x34, static_cast<std::int32_t>(0x190000u - static_cast<U>(kOff[ah::Next() % 10])));
        break;
    }
    case k198Release:
        if (ah::Half()) Field_Request = static_cast<unsigned char>(AH_PICK(5, 5, 4, 6, 0x85));
        break;
    case k198Drops:
        if (ah::Often()) Frame_Counter = Frame_Counter & ~7u;
        if (ah::Often()) B(at::kCounter0) = static_cast<unsigned char>(AH_PICK(2, 3, 4, 3, 4, 0xFF, 0));
        if (ah::Half()) Sprite_Current[0xB] = static_cast<unsigned char>(Sprite_Current[0xB] & 0xF8);
        break;
    case k198EffRun:
        SeedEffectCurrent();
        Sprite_Current[1] = static_cast<unsigned char>(ah::Next() % at::kStateCount);
        break;
    case k198EffStart:
    case k198EffFollow:
        SeedEffectCurrent();
        if (ah::Often()) Sprite_Current[7] = static_cast<unsigned char>(ah::Next() % 30);
        if (ah::Often()) Sprite_Current[6] = static_cast<unsigned char>(ah::Next() % 3);
        if (ah::Half()) ah::Object(Sprite_Current[7])[0x48] = 0;
        break;
    default: break;
    }
}

// ---- area 199 ----
void Seed199(unsigned) {
    Common(199);
    SeedAnswer(3);
}

void RunArea(int area, const ah::Clone* clones, unsigned n, const ah::DataTable* tables, unsigned n_tables, const ah::Region* regions,
             unsigned n_regions, void (*seed)(unsigned), void (*args)(unsigned, std::uint32_t*), unsigned rounds) {
    ah::Group g{"area_w4f", clones, n, kCallees, sizeof kCallees / sizeof kCallees[0], tables, n_tables,
                regions, n_regions, seed, &Disturb, rounds};
    g.args = args;
    g.area = area;
    ah::Run(g);
}

}  // namespace

void SelfTest() {
    constexpr unsigned kRounds = 6000;
#define W4F_RUN(area, clones, tables, n_tables, regions, seed, args)                                                              \
    RunArea(area, clones, sizeof clones / sizeof clones[0], tables, n_tables, regions, sizeof regions / sizeof regions[0], seed, \
            args, kRounds)
    W4F_RUN(192, kClones192, nullptr, 0, kRegions192, &Seed192, &Args192);
    W4F_RUN(193, kClones193, nullptr, 0, kRegions193, &Seed193, &Args193);
    W4F_RUN(196, kClones196, nullptr, 0, kRegions196, &Seed196, nullptr);
    W4F_RUN(197, kClones197, kTables197, 1, kRegions197, &Seed197, &Args197);
    W4F_RUN(198, kClones198, kTables198, 1, kRegions198, &Seed198, nullptr);
    W4F_RUN(199, kClones199, nullptr, 0, kRegions199, &Seed199, nullptr);
#undef W4F_RUN
}

}  // namespace area_w4f

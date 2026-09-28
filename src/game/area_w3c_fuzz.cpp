// BOF3X_SHADOW=area_w3c: world 3's areas 124..125, 127..128 and 130..134
// through the area round's shared harness (area_harness.h), once at start-up -
// one area_harness::Run per area, each Group setting its own area number, all
// under the one shadow name. docs/area_w3c.md section 3.
//
// The clone tables are tools/area_rows.py --clones's rows for AREA124..134
// (2026-09-28), each row read against the disassembly (every start, extent,
// call site and jump table agrees); the shapes are the root table each
// function hangs from (docs/area_w3c.md section 1). The group's own callee
// (Area131_DisarmTail) is a recorder here like any other callee, so each
// function is fuzzed alone.
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w3c.h"
#include "game/area_w3c_callees.h"
#include "game/move_script_bytes.h"
#include "hook/log.h"

namespace area_w3c {
namespace {

namespace ah = area_harness;
using U = std::uint32_t;
using S = ah::Shape;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;

#define AH_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define AH_COUNT(a) static_cast<unsigned>(sizeof a / sizeof a[0])
#define W3C_C(name, base, size, calls, shape) \
    {#name, base, size, calls, AH_N(calls), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::name), 0, false, shape}
#define W3C_P(name, base, size, shape) \
    {#name, base, size, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::name), 0, false, shape}
// A clone with a jump table.
#define W3C_T(name, base, size, calls, tables, shape) \
    {#name, base, size, calls, AH_N(calls), nullptr, 0, tables, AH_N(tables), reinterpret_cast<const void*>(&::name), 0, false, shape}
// A clone answering in al (the object triggers).
#define W3C_A(name, base, size, calls, shape) \
    {#name, base, size, calls, AH_N(calls), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::name), 0xFF, false, shape}
#define W3C_AP(name, base, size, shape) \
    {#name, base, size, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::name), 0xFF, false, shape}

// Shared call-site lists.
constexpr ah::CallSite kCallsSpawn2C[] = {{0x2C, 0x57CE10}};
constexpr ah::CallSite kCallsSpawn30[] = {{0x30, 0x57CE10}};
constexpr ah::CallSite kCallsFindFree1[] = {{0x1, 0x589810}};
constexpr ah::CallSite kCallsSet40At0[] = {{0x0, 0x57C7C0}};

// ---- areas 124, 125 ----
// Each init's first instruction is a jmp over eleven nops to its body
// (0x41C8A0, 0x41C970), which bof3::CloneOriginal refuses as an entry that
// looks patched (E9); the clone is the body the jmp reaches, 0x10 bytes on,
// its call sites 0x10 less than the tool's rows (0x12, 0x4D, 0x98).
constexpr ah::CallSite kCallsPlace[] = {{0x2, 0x5B93D2}, {0x3D, 0x5B93D2}, {0x88, 0x5720C0}};
const ah::Clone kClones124[] = {W3C_C(Area124_PlaceRandomObject, 0x41C8A0, 0xBB, kCallsPlace, S::kInit)};
const ah::Clone kClones125[] = {W3C_C(Area125_PlaceRandomObject, 0x41C970, 0xBB, kCallsPlace, S::kInit)};
// ---- area 127 ----
constexpr ah::CallSite kCalls41CA30[] = {{0x24, 0x57C0F0}, {0x2D, 0x572650}};
const ah::Clone kClones127[] = {W3C_C(Area127_ClearActive80, 0x41CA30, 0x3E, kCalls41CA30, S::kHandler)};
// ---- area 128 ----
constexpr ah::CallSite kCalls41CAB0[] = {{0x9, 0x57C7C0}};
constexpr ah::CallSite kCalls41CB40[] = {{0x19, 0x57C7A0}, {0x28, 0x57C0F0}, {0x3B, 0x594E00}, {0x5A, 0x57C7A0}, {0x61, 0x531F90}, {0x70, 0x57C0F0}};
constexpr ah::CallSite kCalls41CC00[] = {{0x12, 0x57C0F0}, {0x1E, 0x57C110}, {0x43, 0x571110}, {0x6D, 0x57C140},
                                         {0x82, 0x579F00}, {0x90, 0x579F00}, {0x9E, 0x579F00}, {0xAC, 0x579F00}};
const ah::Clone kClones128[] = {
    W3C_P(Area128_ChoiceStep3, 0x41CA70, 0x37, S::kChoice),
    W3C_C(Area128_ChoiceArmTail56, 0x41CAB0, 0x30, kCalls41CAB0, S::kChoice),
    W3C_P(Area128_PlaceObject, 0x41CAE0, 0x56, S::kHandler),
    W3C_C(Area128_TailLeave56, 0x41CB40, 0x86, kCalls41CB40, S::kTail),
    W3C_A(Area128_Trigger61, 0x41CBD0, 0x21, kCallsSet40At0, S::kCallee),
    W3C_C(Area128_InitPatches, 0x41CC00, 0xB5, kCalls41CC00, S::kInit),
};
enum : unsigned { k128Step3, k128Arm, k128Place, k128Tail, k128Trigger, k128Init };
static_assert(k128Init + 1 == AH_COUNT(kClones128), "area 128's seeding indices");
// ---- area 130 ----
constexpr ah::CallSite kCallsDropped[] = {{0x2, 0x4DF820}};
constexpr ah::CallSite kCalls41D0B0[] = {{0x2, 0x591900}};
constexpr ah::CallSite kCalls41D0E0[] = {{0x25, 0x587740}, {0x34, 0x57C0F0}, {0x3C, 0x57C7A0}, {0x7A, 0x591680}, {0x88, 0x5B9450},
                                         {0xA0, 0x591680}, {0xAE, 0x5B9450}, {0xC6, 0x591680}, {0xD4, 0x5B9450}, {0xE9, 0x591680},
                                         {0xF7, 0x5B9450}, {0x10C, 0x591680}, {0x11A, 0x5B9450}, {0x12F, 0x591680}, {0x13D, 0x5B9450},
                                         {0x14A, 0x590BB0}, {0x15B, 0x4976D0}};
constexpr ah::JumpTable kTables41D0E0[] = {{0x70, 0x174, 6}};
const ah::Clone kClones130[] = {
    W3C_P(Area130_ChoiceRunStep, 0x41CCC0, 0x4D, S::kChoice),
    W3C_C(Area130_SpawnKind4AtMember0, 0x41CD10, 0x42, kCallsSpawn2C, S::kHandler),
    W3C_C(Area130_SpawnKind3AtMember0, 0x41CD60, 0x42, kCallsSpawn2C, S::kHandler),
    W3C_C(Area130_DroppedCall1, 0x41CDB0, 0x9, kCallsDropped, S::kHandler),
    W3C_C(Area130_DroppedCallTrack83, 0x41CDC0, 0x12, kCallsDropped, S::kHandler),
    W3C_C(Area130_SpawnKind3AtMember1, 0x41CDE0, 0x42, kCallsSpawn2C, S::kHandler),
    W3C_C(Area130_SpawnKind3AtMember2, 0x41CE30, 0x46, kCallsSpawn30, S::kHandler),
    W3C_C(Area130_Effect78AtObject, 0x41CE80, 0x4C, kCallsFindFree1, S::kHandler),
    W3C_C(Area130_SpawnKind1AtMember1, 0x41CED0, 0x42, kCallsSpawn2C, S::kHandler),
    W3C_C(Area130_SpawnKind1AtMember2, 0x41CF20, 0x46, kCallsSpawn30, S::kHandler),
    W3C_C(Area130_SpawnKind4AtMember1, 0x41CF70, 0x42, kCallsSpawn2C, S::kHandler),
    W3C_C(Area130_SpawnKind4AtMember2, 0x41CFC0, 0x46, kCallsSpawn30, S::kHandler),
    W3C_C(Area130_SpawnKind5AtMember1, 0x41D010, 0x42, kCallsSpawn2C, S::kHandler),
    W3C_C(Area130_SpawnKind5AtMember2, 0x41D060, 0x46, kCallsSpawn30, S::kHandler),
    W3C_C(Area130_GiveKeyItem10, 0x41D0B0, 0x9, kCalls41D0B0, S::kHandler),
    W3C_A(Area130_Trigger65, 0x41D0C0, 0x16, kCallsSet40At0, S::kCallee),
    W3C_T(Area130_TailGiveItem, 0x41D0E0, 0x18C, kCalls41D0E0, kTables41D0E0, S::kTail),
    W3C_P(Area130_ChoiceTailState2, 0x41D270, 0x1A, S::kChoice),
};
enum : unsigned {
    k130Choice, k130S4M0, k130S3M0, k130Drop1, k130Drop83, k130S3M1, k130S3M2, k130E78, k130S1M1, k130S1M2, k130S4M1,
    k130S4M2, k130S5M1, k130S5M2, k130Key10, k130Trigger, k130Tail, k130State2
};
static_assert(k130State2 + 1 == AH_COUNT(kClones130), "area 130's seeding indices");
// ---- area 131 ----
constexpr ah::CallSite kCalls41D290[] = {{0x9, 0x57C7C0}};
constexpr ah::CallSite kCalls41D300[] = {{0x12, 0x57C7C0}};
constexpr ah::CallSite kCalls41D390[] = {{0x13, 0x57C7C0}, {0x1A, 0x4976D0}, {0x3F, 0x41D430}, {0x5D, 0x41D430}, {0x70, 0x57C110}, {0x83, 0x594E00}};
constexpr ah::JumpTable kTables41D390[] = {{0xF, 0x8C, 4}};
constexpr ah::CallSite kCalls41D430[] = {{0x0, 0x57C7A0}};
const ah::Clone kClones131[] = {
    W3C_C(Area131_ChoiceFocusStepA, 0x41D290, 0x6A, kCalls41D290, S::kChoice),
    W3C_C(Area131_ChoiceFocusStep0, 0x41D300, 0x5E, kCalls41D300, S::kChoice),
    W3C_P(Area131_CameraShiftYLess1E, 0x41D360, 0x10, S::kHandler),
    W3C_P(Area131_CameraShiftYMore1E, 0x41D370, 0x10, S::kHandler),
    W3C_AP(Area131_Trigger15, 0x41D380, 0xA, S::kCallee),
    W3C_T(Area131_TailLeave34, 0x41D390, 0x9C, kCalls41D390, kTables41D390, S::kTail),
    W3C_C(Area131_DisarmTail, 0x41D430, 0x17, kCalls41D430, S::kCallee),
};
enum : unsigned { k131StepA, k131Step0, k131Less, k131More, k131Trigger, k131Tail, k131Disarm };
static_assert(k131Disarm + 1 == AH_COUNT(kClones131), "area 131's seeding indices");
// ---- area 132 ----
constexpr ah::CallSite kCalls41D450[] = {{0xD, 0x5891F0}, {0x18, 0x5891F0}};
constexpr ah::CallSite kCalls41D470[] = {{0x1, 0x589810}, {0x43, 0x5720C0}};
constexpr ah::CallSite kCalls41D4D0[] = {{0x2, 0x589810}, {0x47, 0x5720C0}, {0x69, 0x589810}, {0xAE, 0x5720C0}};
constexpr ah::CallSite kCalls41D5B0[] = {{0x1, 0x589810}, {0x3F, 0x5720C0}};
constexpr ah::CallSite kCalls41D620[] = {{0xC, 0x589840}, {0x2E, 0x5A77C0}, {0x37, 0x461E50}, {0x43, 0x5A7610}, {0x4A, 0x5A7780}, {0x9F, 0x461E50}};
const ah::Clone kClones132[] = {
    W3C_C(Area132_AnimByBit4, 0x41D450, 0x1F, kCalls41D450, S::kHandler),
    W3C_C(Area132_Effect73, 0x41D470, 0x57, kCalls41D470, S::kHandler),
    W3C_C(Area132_Effect73Pair, 0x41D4D0, 0xD3, kCalls41D4D0, S::kHandler),
    W3C_C(Area132_Effect74, 0x41D5B0, 0x53, kCalls41D5B0, S::kHandler),
    W3C_P(Area132_ClearFE, 0x41D610, 0x8, S::kHandler),
    W3C_C(Area132_EffectGradient, 0x41D620, 0xAA, kCalls41D620, S::kState),
};
enum : unsigned { k132Anim, k132E73, k132Pair, k132E74, k132ClearFE, k132Gradient };
static_assert(k132Gradient + 1 == AH_COUNT(kClones132), "area 132's seeding indices");
// ---- area 133 ----
constexpr ah::CallSite kCalls41D6D0[] = {{0x4E, 0x57C7C0}};
const ah::Clone kClones133[] = {
    W3C_C(Area133_ChoiceFocusPair, 0x41D6D0, 0x69, kCalls41D6D0, S::kChoice),
    W3C_C(Area133_SpawnKind3AtMember0, 0x41D740, 0x42, kCallsSpawn2C, S::kHandler),
};
enum : unsigned { k133Choice, k133S3M0 };
// ---- area 134 ----
const ah::Clone kClones134[] = {
    W3C_C(Area134_SpawnKind4AtMember0, 0x41D790, 0x42, kCallsSpawn2C, S::kHandler),
    W3C_P(Area134_CameraShiftXMore2, 0x41D7E0, 0x12, S::kHandler),
    W3C_P(Area134_CameraShiftXLess2, 0x41D800, 0x10, S::kHandler),
    W3C_P(Area134_CameraShiftXReset, 0x41D810, 0x11, S::kHandler),
    W3C_C(Area134_SpawnKind1AtMember0, 0x41D830, 0x42, kCallsSpawn2C, S::kHandler),
    W3C_P(Area134_CameraShiftYMore8, 0x41D880, 0x10, S::kHandler),
    W3C_P(Area134_CameraShiftYLess8, 0x41D890, 0x10, S::kHandler),
    W3C_C(Area134_SpawnKind3AtMember1, 0x41D8A0, 0x42, kCallsSpawn2C, S::kHandler),
    W3C_C(Area134_SpawnKind3AtMember2, 0x41D8F0, 0x46, kCallsSpawn30, S::kHandler),
    W3C_C(Area134_SpawnKind1AtMember1, 0x41D940, 0x42, kCallsSpawn2C, S::kHandler),
    W3C_C(Area134_SpawnKind1AtMember2, 0x41D990, 0x46, kCallsSpawn30, S::kHandler),
    W3C_C(Area134_Effect90AtObject, 0x41D9E0, 0x50, kCallsFindFree1, S::kHandler),
    W3C_C(Area134_Effect93AtObject, 0x41DA30, 0x4C, kCallsFindFree1, S::kHandler),
    W3C_C(Area134_Effect99AtObject, 0x41DA80, 0x4C, kCallsFindFree1, S::kHandler),
};
enum : unsigned {
    k134S4M0, k134XMore, k134XLess, k134XReset, k134S1M0, k134YMore, k134YLess, k134S3M1, k134S3M2, k134S1M1, k134S1M2,
    k134E90, k134E93, k134E99
};
static_assert(k134E99 + 1 == AH_COUNT(kClones134), "area 134's seeding indices");
#undef W3C_C
#undef W3C_P
#undef W3C_T
#undef W3C_A
#undef W3C_AP

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
unsigned char& B(U address) { return *ah::Mem(address); }
constexpr U kAll = 0xFFFFFFFFu, kU8 = 0xFFu, kU16 = 0xFFFFu;

// ===========================================================================
// Shared
// ===========================================================================

// A record a pointer the areas follow may name: a field object or a party
// record (both in the harness's regions).
unsigned char* SomeRecord(U v) { return v & 1 ? ah::Object(v >> 1) : ah::PartyOf(static_cast<unsigned char>(v >> 1)); }

// Louder than the real callees, on purpose (half the time, from Noise): after
// these calls the callers read Sprite_Current again (the spawns' +0xB store
// after Effect_Spawn, area 127's +0 store after MoveCmd_TestFB).
U MovesCurrent(const U*, U answer) {
    const U n = ah::Noise();
    if (n & 1) Sprite_Current = SomeRecord(n >> 8);
    return answer;
}
// ScriptFlags_Set40 moves the focus object pointer half the time (area 131's
// choices read it again after the call) and the tail state now and then.
U MovesFocus(const U*, U answer) {
    const U n = ah::Noise();
    if (n & 1) ah::SetPointer(at::kFocusObject, SomeRecord(n >> 8));
    if ((n & 6) == 2) B(at::kTailState) = static_cast<unsigned char>(n >> 16);
    return answer;
}
// Msg_OpenScript moves the tail state half the time (area 131's tail kind 34
// reads it again after the call to count it on).
U MovesTailState(const U*, U answer) {
    const U n = ah::Noise();
    if (n & 1) B(at::kTailState) = static_cast<unsigned char>(n >> 8);
    return answer;
}
// AreaMap_Elevation moves the effect records' x now and then (area 132's pair
// reads +0x34 again after the call); its answer any word, a sign edge often.
U ElevationEffect(const U*, U answer) {
    const U n = ah::Noise();
    if (n % 3 == 0) {
        unsigned char* const e = ah::Mem(at::kEffectObjects + ((n >> 4) % 4) * at::kEffectStride);
        SetLong(e + 0x34, static_cast<std::int32_t>(n));
    }
    if (n % 5 == 0) return (answer & 0xFFFF0000u) | AH_PICK(0x7FFF, 0x8000, 0xFFFF, 0, 0xFE00, 0x7E00);
    return answer;
}
// Effect_FindFree: a slot of the first four or none (0xFF); the calls after it
// read Sprite_Current again (the copies at the running object).
U FindFreeEffect(const U* a, U answer) { return MovesCurrent(a, answer); }

// A choice answer: each value a choice tests, its neighbours, a negative byte
// (read signed), anything.
void SeedAnswer() {
    B(at::kChoiceAnswer) = static_cast<unsigned char>(ah::Often() ? AH_PICK(0, 0, 0, 1, 1, 2, 2, 3, 4, 5, 0xFF, 0x80, 0x81, 0x7F) : ah::Next());
}
// A party list byte: inside the tables mostly, anything else now and then (the
// tables are read unchecked; .data either side).
void SeedList(U cell) { B(cell) = static_cast<unsigned char>(ah::Often() ? ah::Next() % 8 : ah::Next()); }
// Field_Request 2 (the tails wait) a third of the time, else its neighbours or
// any.
void SeedRequest() { Field_Request = static_cast<unsigned char>(ah::Next() % 3 == 0 ? 2 : AH_PICK(0, 1, 3, 0x82, 0xFF, 2)); }

// The object triggers' arguments: (a field object, the story flags).
void ArgsTrigger(std::uint32_t* a) {
    a[0] = Key(ah::Object(a[0]));
    a[1] = at::kStoryFlags;
}

#define W3C_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
#define W3C_THEIRS(name) #name, KeyOf(name), KeyOf(name)

// The spawns' callees: Effect_Spawn (the byte and the words pushed with stale
// high bits; slots 0..2 or none) moving Sprite_Current; Effect_FindFree.
#define W3C_SPAWN \
    {W3C_THEIRS(Effect_Spawn), 5, {kU8, kU8, kU8, kU16, kU16}, ah::Answer::kByte, 0xFE, 0x02, {}, &MovesCurrent}, \
    {W3C_OURS(Effect_FindFree), 0, {}, ah::Answer::kByte, 0xFF, 0x03, {}, &FindFreeEffect}

// Every effect record a slot byte can name (the stand-in answers 0..3 or none;
// a disturbed byte reaches past).
constexpr ah::Region kEffectRegion = {at::kEffectObjects, at::kEffectCount * at::kEffectStride};

// ===========================================================================
// Areas 124, 125
// ===========================================================================

const ah::Callee kCalleesPlace[] = {
    {W3C_OURS(AreaMap_Elevation), 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0},
};
void SeedPlace(unsigned) {
    // Rand's first answer near each cumulative weight edge a third of the time
    // (the hint), the leader's +0x134 around 5
    ah::SetRandHint(AH_PICK(0, 1, 7, 8, 15, 16, 31, 32, 47, 48, 62, 63, 64));
    if (ah::Often()) SetLong(ah::Mem(at::kLeaderZone), static_cast<std::int32_t>(AH_PICK(5, 4, 6, 0, 0x10005, 0xFFFF)));
}

// ===========================================================================
// Area 127
// ===========================================================================

const ah::Callee kCallees127[] = {
    {W3C_OURS(Flags_Set), 2, {kAll, kU8}, ah::Answer::kGarbage, 0, 0, {}, &MovesCurrent},
    {W3C_OURS(MoveCmd_TestFB), 2, {kU16, kU16}, ah::Answer::kFlag, 0, 0, {}, &MovesCurrent},
};
const ah::Region kRegions127[] = {{at::kActiveMember, 4}};
void Seed127(unsigned) {
    ah::SetPointer(at::kActiveMember, SomeRecord(ah::Next()));
    B(at::kLeader89) = static_cast<unsigned char>(ah::Often() ? AH_PICK(5, 5, 4, 6, 0x85, 0) : ah::Next());
}
void Disturb127(U h) { ah::SetPointer(at::kActiveMember, SomeRecord(h >> 8)); }

// ===========================================================================
// Area 128
// ===========================================================================

// AreaMap_ApplyPatch: the entry's step (its high word) rewritten half the
// time, 0..3 - the walk reads the dword again after the call; the chain the
// seed builds ends inside the area block whatever the steps.
U PatchEffect(const U* a, U answer) {
    const U n = ah::Noise();
    const U entry = a[0];
    if ((n & 1) && entry >= at::kMapHeader && entry + 4 <= at::kMapHeader + 0x2000) SetWord(ah::Mem(entry) + 2, (n >> 8) % 4);
    return answer;
}
// Flags_Set / Flags_Clear move the chapter byte now and then (the init reads
// it after the patch chain).
U MovesChapter(const U*, U answer) {
    const U n = ah::Noise();
    if (n % 4 == 0) Cond_ByteFA = static_cast<signed char>(AH_PICK(10, 11, 0, 0x7F, 0x80, 9, 12));
    return answer;
}
const ah::Callee kCallees128[] = {
    {W3C_OURS(ScriptFlags_Set40), 0, {}, ah::Answer::kGarbage, 0, 0},
    {W3C_OURS(ScriptFlags_Clear40), 0, {}, ah::Answer::kGarbage, 0, 0},
    {W3C_OURS(Flags_Test), 2, {kAll, kU8}, ah::Answer::kBool, 0, 0},
    {W3C_OURS(Flags_Set), 2, {kAll, kU8}, ah::Answer::kGarbage, 0, 0, {}, &MovesChapter},
    {W3C_OURS(Flags_Clear), 2, {kAll, kU8}, ah::Answer::kGarbage, 0, 0, {}, &MovesChapter},
    {W3C_OURS(AreaMap_SetByte), 3, {kU16, kU16, kU8}, ah::Answer::kGarbage, 0, 0},
    {W3C_OURS(AreaMap_ApplyPatch), 1, {kAll}, ah::Answer::kGarbage, 0, 0, {4}, &PatchEffect},
};
const ah::Region kRegions128[] = {
    {at::kCameraShiftX, 8},   // the focus object pointer at +4
    {at::kLastArea, 4},
};
// The patch chain: AreaMap_PatchBase at a dword of the block's first 7 KiB,
// then n (0..24) dwords not 0 with steps 0..3 in their high words (a zero now
// and then ends it early), then four zero dwords - so any step from any entry
// lands inside the chain or on a zero.
void SeedChain() {
    const U base = 0x20 + ah::Next() % 0x6C0;
    SetWord(ah::Mem(at::kPatchBase), base);
    const unsigned n = ah::Next() % 25;
    unsigned char* const p = ah::Mem(at::kMapHeader + base * 4);
    for (unsigned i = 0; i < n; ++i) {
        const U low = 1 + ah::Next() % 0xFFFF;
        const U step = ah::Next() % 4;
        SetLong(p + i * 4, static_cast<std::int32_t>(ah::Next() % 16 == 0 ? 0 : step << 16 | low));
    }
    for (unsigned i = 0; i < 4; ++i) SetLong(p + (n + i) * 4, 0);
}
void Seed128(unsigned k) {
    ah::SetPointer(at::kFocusObject, SomeRecord(ah::Next()));
    switch (k) {
    case k128Step3: case k128Arm: SeedAnswer(); break;
    case k128Place:
        Field_State[0x89] = static_cast<unsigned char>(ah::Often() ? AH_PICK(2, 2, 1, 3, 0x82, 0) : ah::Next());
        break;
    case k128Tail:
        B(at::kTailState) = static_cast<unsigned char>(ah::Often() ? AH_PICK(0, 0, 0xA, 0xA, 1, 9, 0xB, 0x8A, 0xFF) : ah::Next());
        SeedRequest();
        break;
    case k128Init:
        SetWord(ah::Mem(at::kLastArea), ah::Often() ? 0x79 : AH_PICK(0x78, 0x7A, 0x179, 0x7900, 0));
        SeedChain();
        Cond_ByteFA = static_cast<signed char>(ah::Often() ? AH_PICK(10, 11, 11, 12, 9, 0x7F, 0x80, 0) : ah::Next());
        break;
    default: break;
    }
}
void Disturb128(U h) {
    const auto v = static_cast<unsigned char>(h >> 20);
    switch ((h >> 8) % 3) {
    case 0: ah::SetPointer(at::kFocusObject, SomeRecord(h >> 12)); break;
    case 1: B(at::kChoiceAnswer) = static_cast<unsigned char>(v % 3); break;
    default: Field_Request = static_cast<unsigned char>(v & 1 ? 2 : v); break;
    }
}

// ===========================================================================
// Area 130
// ===========================================================================

// Item_NamePtr answers a name pointer; the copy that follows logs it.
const ah::Callee kCallees130[] = {
    W3C_SPAWN,
    {W3C_OURS(ScriptFlags_Set40), 0, {}, ah::Answer::kGarbage, 0, 0},
    {W3C_OURS(ScriptFlags_Clear40), 0, {}, ah::Answer::kGarbage, 0, 0},
    {W3C_OURS(Flags_Set), 2, {kAll, kU8}, ah::Answer::kGarbage, 0, 0},
    {W3C_OURS(Port_DroppedCall), 1, {kU8}, ah::Answer::kGarbage, 0, 0},
    {W3C_OURS(KeyItem_Add), 1, {kAll}, ah::Answer::kGarbage, 0, 0},
    {W3C_OURS(Item_NamePtr), 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0},
    {"strncpy", at::kStrncpy, at::kStrncpy, 3, {kAll, kAll, kAll}, ah::Answer::kGarbage, 0, 0},
    {W3C_OURS(Inventory_Add), 3, {kAll, kAll, kAll}, ah::Answer::kFlag, 0, 0},
    {W3C_OURS(Msg_OpenScript), 1, {kU16}, ah::Answer::kGarbage, 0, 0},
    {W3C_OURS(Sound_PlayEffect), 1, {kU16}, ah::Answer::kGarbage, 0, 0},
};
const ah::Region kRegions130[] = {
    kEffectRegion,
    {at::kItemPick, 1},
    {at::kTextRecords, 0x30},
};
void Seed130(unsigned k) {
    switch (k) {
    case k130Choice: case k130State2: SeedAnswer(); break;
    case k130S4M0: case k130S3M0: SeedList(at::kPartyList0); break;
    case k130S3M1: case k130S1M1: case k130S4M1: case k130S5M1: SeedList(at::kPartyList1); break;
    case k130S3M2: case k130S1M2: case k130S4M2: case k130S5M2: SeedList(at::kPartyList2); break;
    case k130Tail:
        B(at::kVar7Step) = static_cast<unsigned char>(ah::Often() ? AH_PICK(0, 0, 1, 1, 2, 0xFF, 0x80) : ah::Next());
        B(at::kItemPick) = static_cast<unsigned char>(ah::Often() ? ah::Next() % 6 : AH_PICK(6, 7, 0xFF, 0x80, 0x100 - 6));
        SeedRequest();
        break;
    default: break;
    }
}
void Args130(unsigned k, std::uint32_t* a) {
    if (k == k130Trigger) ArgsTrigger(a);
}
void Disturb130(U h) {
    const auto v = static_cast<unsigned char>(h >> 20);
    switch ((h >> 8) % 3) {
    case 0: B(at::kPartyList0 + (v % 3)) = static_cast<unsigned char>(v % 8); break;
    case 1: B(at::kVar7Step) = static_cast<unsigned char>(v % 3); break;
    default: Field_Request = static_cast<unsigned char>(v & 1 ? 2 : v); break;
    }
}

// ===========================================================================
// Area 131
// ===========================================================================

const ah::Callee kCallees131[] = {
    {W3C_OURS(ScriptFlags_Set40), 0, {}, ah::Answer::kGarbage, 0, 0, {}, &MovesFocus},
    {W3C_OURS(ScriptFlags_Clear40), 0, {}, ah::Answer::kGarbage, 0, 0},
    {W3C_OURS(Msg_OpenScript), 1, {kU16}, ah::Answer::kGarbage, 0, 0, {}, &MovesTailState},
    {W3C_OURS(Flags_Clear), 2, {kAll, kU8}, ah::Answer::kGarbage, 0, 0},
    {W3C_OURS(Field_ChangeArea), 4, {kAll, kAll, kAll, kAll}, ah::Answer::kGarbage, 0, 0},
    // the group's own, called by the tail (a call and a tail jmp)
    {W3C_OURS(Area131_DisarmTail), 0, {}, ah::Answer::kPhase, 0, 0},
};
const ah::Region kRegions131[] = {{at::kCameraShiftX, 8}};
void Seed131(unsigned k) {
    ah::SetPointer(at::kFocusObject, SomeRecord(ah::Next()));
    switch (k) {
    case k131StepA: case k131Step0: SeedAnswer(); break;
    case k131Tail:
        B(at::kTailState) = static_cast<unsigned char>(ah::Often() ? AH_PICK(0, 1, 2, 3, 0, 1, 2, 3, 4, 0xFF, 0x80, 0x7F) : ah::Next());
        SeedRequest();
        break;
    default: break;
    }
}
void Args131(unsigned k, std::uint32_t* a) {
    if (k == k131Trigger) ArgsTrigger(a);
}
void Disturb131(U h) {
    const auto v = static_cast<unsigned char>(h >> 20);
    switch ((h >> 8) % 4) {
    case 0: ah::SetPointer(at::kFocusObject, SomeRecord(h >> 12)); break;
    case 1: B(at::kTailState) = static_cast<unsigned char>(v % 5); break;
    case 2: B(at::kChoiceAnswer) = static_cast<unsigned char>(v % 3); break;
    default: Field_Request = static_cast<unsigned char>(v & 1 ? 2 : v); break;
    }
}

// ===========================================================================
// Area 132
// ===========================================================================

// The fuzz's own packet buffer: the gradient's draw mode and quad.
constexpr unsigned kPacketBytes = 0x400;
alignas(16) unsigned char g_packets[kPacketBytes];
bool InPackets(U p, unsigned n) { return p >= Key(g_packets) && p + n <= Key(g_packets) + kPacketBytes; }
// The primitive setters write the primitive's bytes, so a store the caller
// makes before the call (where the original makes it after) shows.
void Scribble(U at, unsigned n) {
    if (!InPackets(at, n)) return;
    unsigned char* const p = ah::Mem(at);
    for (unsigned i = 0; i < n; i += 4) SetLong(p + i, static_cast<std::int32_t>(ah::Noise()));
}
U DrawModeEffect(const U* a, U answer) { Scribble(a[0], 0xC); return answer; }
U PolyG4Effect(const U* a, U answer) { Scribble(a[0], 0x44); return answer; }
U SemiEffect(const U* a, U answer) {
    if (InPackets(a[0], 8)) ah::Mem(a[0])[7] = static_cast<unsigned char>(a[1] ? ah::Mem(a[0])[7] | 2 : ah::Mem(a[0])[7] & 0xFD);
    return answer;
}
// Gfx_CommitPrim moves the packet cursor on by the size, most of the time
// (the gradient reads the cursor again after the first commit), kept inside
// the fuzz's buffer.
U CommitEffect(const U* a, U answer) {
    const U n = ah::Noise();
    if (n % 5) {
        U next = Key(Gfx_PacketNext) + (a[1] & 0xFF);
        if (!InPackets(next, 0x48)) next = Key(g_packets) + ((n >> 8) % 8) * 4;
        Gfx_PacketNext = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(next));
    }
    return answer;
}
// Effect_Release moves Cond_ByteFE and the pass flags now and then (the
// gradient reads the flags after the call).
U ReleaseEffect(const U*, U answer) {
    const U n = ah::Noise();
    if (n % 3 == 0) Draw_PassFlags = static_cast<unsigned char>(n >> 8);
    return answer;
}
const ah::Callee kCallees132[] = {
    W3C_SPAWN,
    {W3C_OURS(Sprite_SetAnimation), 1, {kU8}, ah::Answer::kGarbage, 0, 0},
    {W3C_OURS(AreaMap_Elevation), 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &ElevationEffect},
    {W3C_OURS(Effect_Release), 0, {}, ah::Answer::kGarbage, 0, 0, {}, &ReleaseEffect},
    {W3C_OURS(Gpu_SetDrawMode), 5, {kAll, kAll, kAll, kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &DrawModeEffect},
    {W3C_OURS(Gpu_SetPolyG4), 1, {kAll}, ah::Answer::kGarbage, 0, 0, {}, &PolyG4Effect},
    {W3C_OURS(Gpu_SetSemiTrans), 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &SemiEffect},
    {W3C_OURS(Gfx_CommitPrim), 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &CommitEffect},
};
ah::Region g_regions132[] = {
    kEffectRegion,
    {at::kPacketNext, 4},
    {0, kPacketBytes},   // g_packets, filled in at Run
    {at::kPassFlags, 1},
    {at::kCondByteFE, 1},
};
void Seed132(unsigned k) {
    Gfx_PacketNext = g_packets + (ah::Next() % 8) * 4;
    switch (k) {
    case k132Anim:
        if (ah::Often()) Sprite_Current[8] = static_cast<unsigned char>(AH_PICK(4, 0, 0xFB, 0xFF, 5, 3, 0x84));
        break;
    case k132Gradient:
        Cond_ByteFE = static_cast<unsigned char>(ah::Next() % 3 == 0 ? 0 : AH_PICK(1, 0x80, 0xFF, 2));
        Draw_PassFlags = static_cast<unsigned char>(ah::Often() ? AH_PICK(4, 0, 0xFB, 0xFF, 5, 3, 0x1B) : ah::Next());
        break;
    default: break;
    }
}
void Disturb132(U h) {
    const auto v = static_cast<unsigned char>(h >> 20);
    switch ((h >> 8) % 3) {
    case 0: Gfx_PacketNext = g_packets + (v % 16) * 4; break;
    case 1: Draw_PassFlags = static_cast<unsigned char>(v & 1 ? v | 4 : v & 0xFB); break;
    default: Cond_ByteFE = static_cast<unsigned char>(v & 1); break;
    }
}

// ===========================================================================
// Area 133
// ===========================================================================

const ah::Callee kCallees133[] = {
    W3C_SPAWN,
    {W3C_OURS(ScriptFlags_Set40), 0, {}, ah::Answer::kGarbage, 0, 0},
};
const ah::Region kRegions133[] = {
    {at::kCameraShiftX, 8},
    {at::kLoad0F, 1},
    {at::kPassFlags, 1},
};
void Seed133(unsigned k) {
    ah::SetPointer(at::kFocusObject, SomeRecord(ah::Next()));
    if (k == k133Choice) {
        B(at::kChoiceAnswer) = static_cast<unsigned char>(ah::Often() ? AH_PICK(0, 1, 2, 2, 2, 3, 4, 5, 0xFF, 0x82) : ah::Next() % 6);
    } else {
        SeedList(at::kPartyList0);
    }
}
void Disturb133(U h) {
    const auto v = static_cast<unsigned char>(h >> 20);
    switch ((h >> 8) % 3) {
    case 0: ah::SetPointer(at::kFocusObject, SomeRecord(h >> 12)); break;
    case 1: B(at::kChoiceAnswer) = static_cast<unsigned char>(v % 6); break;
    default: B(at::kPartyList0) = static_cast<unsigned char>(v % 8); break;
    }
}

// ===========================================================================
// Area 134
// ===========================================================================

const ah::Callee kCallees134[] = {W3C_SPAWN};
const ah::Region kRegions134[] = {kEffectRegion, {at::kCameraShiftX, 8}};
void Seed134(unsigned k) {
    switch (k) {
    case k134S4M0: case k134S1M0: SeedList(at::kPartyList0); break;
    case k134S3M1: case k134S1M1: SeedList(at::kPartyList1); break;
    case k134S3M2: case k134S1M2: SeedList(at::kPartyList2); break;
    default: break;
    }
}
void Disturb134(U h) {
    const auto v = static_cast<unsigned char>(h >> 20);
    B(at::kPartyList0 + (v % 3)) = static_cast<unsigned char>(v % 8);
}
#undef W3C_SPAWN
#undef W3C_OURS
#undef W3C_THEIRS

void RunArea(int area, const ah::Clone* clones, unsigned n, const ah::Callee* callees, unsigned n_callees,
             const ah::Region* regions, unsigned n_regions, void (*seed)(unsigned), void (*disturb)(U),
             void (*args)(unsigned, std::uint32_t*), unsigned rounds) {
    ah::Group g{"area_w3c", clones, n, callees, n_callees, nullptr, 0, regions, n_regions, seed, disturb, rounds};
    g.args = args;
    g.area = area;
    ah::Run(g);
}

}  // namespace

void SelfTest() {
    constexpr unsigned kRounds = 6000;
    g_regions132[2].at = Key(g_packets);
    RunArea(124, kClones124, AH_COUNT(kClones124), kCalleesPlace, AH_COUNT(kCalleesPlace), nullptr, 0, &SeedPlace, nullptr,
            nullptr, kRounds);
    RunArea(125, kClones125, AH_COUNT(kClones125), kCalleesPlace, AH_COUNT(kCalleesPlace), nullptr, 0, &SeedPlace, nullptr,
            nullptr, kRounds);
    RunArea(127, kClones127, AH_COUNT(kClones127), kCallees127, AH_COUNT(kCallees127), kRegions127, AH_COUNT(kRegions127),
            &Seed127, &Disturb127, nullptr, kRounds);
    RunArea(128, kClones128, AH_COUNT(kClones128), kCallees128, AH_COUNT(kCallees128), kRegions128, AH_COUNT(kRegions128),
            &Seed128, &Disturb128, [](unsigned k, std::uint32_t* a) { if (k == k128Trigger) ArgsTrigger(a); }, kRounds);
    RunArea(130, kClones130, AH_COUNT(kClones130), kCallees130, AH_COUNT(kCallees130), kRegions130, AH_COUNT(kRegions130),
            &Seed130, &Disturb130, &Args130, kRounds);
    RunArea(131, kClones131, AH_COUNT(kClones131), kCallees131, AH_COUNT(kCallees131), kRegions131, AH_COUNT(kRegions131),
            &Seed131, &Disturb131, &Args131, kRounds);
    RunArea(132, kClones132, AH_COUNT(kClones132), kCallees132, AH_COUNT(kCallees132), g_regions132, AH_COUNT(g_regions132),
            &Seed132, &Disturb132, nullptr, kRounds);
    RunArea(133, kClones133, AH_COUNT(kClones133), kCallees133, AH_COUNT(kCallees133), kRegions133, AH_COUNT(kRegions133),
            &Seed133, &Disturb133, nullptr, kRounds);
    RunArea(134, kClones134, AH_COUNT(kClones134), kCallees134, AH_COUNT(kCallees134), kRegions134, AH_COUNT(kRegions134),
            &Seed134, &Disturb134, nullptr, kRounds);
}

}  // namespace area_w3c

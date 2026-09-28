// BOF3X_SHADOW=area_w1d: world 1's areas 53, 55..57 and 59..64 through the
// area round's shared harness (area_harness.h), once at start-up - one
// area_harness::Run per area, each Group setting its own area number, all
// under the one shadow name. docs/area_w1d.md section 3.
//
// The clone tables are tools/area_rows.py --clones's rows for AREA053..064
// (2026-09-28), each row read against the disassembly (every start, extent
// and call site agree; no jump table in .text); the shapes are the root table
// each function hangs from (docs/area_w1d.md section 1). The two dispatchers'
// .data state tables are DataTables (their entries recorders while the fuzz
// runs), so each state function is fuzzed alone.
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w1d.h"
#include "game/area_w1d_callees.h"
#include "game/move_script_bytes.h"
#include "hook/log.h"

namespace area_w1d {
namespace {

namespace ah = area_harness;

#define AH_N(a) static_cast<int>(sizeof a / sizeof a[0])

// ---- area 53 ----
constexpr ah::CallSite kCalls40AB40[] = {{0x0, 0x57C7C0}};
// ---- area 55 ----
constexpr ah::CallSite kCalls40AB60[] = {{0xF, 0x5919B0}, {0x24, 0x591B60}};
constexpr ah::CallSite kCalls40ABF0[] = {{0x0, 0x57C7C0}};
// ---- area 56 ----
constexpr ah::CallSite kCallsPoseFlag[] = {{0x36, 0x572650}, {0x4E, 0x57C0F0}, {0x64, 0x587740}};   // 0x40AC10..0x40AEB0, each alike
constexpr ah::CallSite kCalls40AFE0[] = {{0x36, 0x572650}};
constexpr ah::CallSite kCalls40B020[] = {{0x17, 0x587740}, {0x4F, 0x572650}};
constexpr ah::CallSite kCalls40B0A0[] = {{0x19, 0x572570}};
constexpr ah::CallSite kCalls40B120[] = {{0x10, 0x52E140}, {0x22, 0x572570}, {0x79, 0x5725F0}};
constexpr ah::CallSite kCallsGather[] = {{0x8, 0x57C0F0}, {0x1B, 0x57C140}, {0x34, 0x57C0F0}};   // 0x40B1D0, 0x40B2F0
// ---- area 57 ----
constexpr ah::CallSite kCalls40B230[] = {{0x2C, 0x57CE10}};
constexpr ah::CallSite kCalls40B280[] = {{0x2C, 0x57CE10}};
constexpr ah::CallSite kCalls40B2D0[] = {{0x0, 0x587B80}};
constexpr ah::CallSite kCalls40B2E0[] = {{0x0, 0x587B90}};
// ---- area 59 ----
constexpr ah::CallSite kCalls40B360[] = {{0x6, 0x579F00}, {0x11, 0x579F00}, {0x1C, 0x579F00}, {0x27, 0x579F00}, {0x32, 0x579F00}, {0x3D, 0x579F00}};
constexpr ah::CallSite kCalls40B3B0[] = {{0x9, 0x579F00}, {0x17, 0x579F00}, {0x25, 0x579F00}, {0x33, 0x579F00}, {0x41, 0x579F00}, {0x4F, 0x579F00}};
constexpr ah::CallSite kCalls40B410[] = {{0x10, 0x57C140}, {0x50, 0x531F90}};
constexpr ah::CallSite kCalls40B470[] = {{0x0, 0x57C7C0}};
constexpr ah::CallSite kCalls40B4F0[] = {{0xD, 0x5720C0}};
constexpr ah::CallSite kCalls40B520[] = {{0x22, 0x4220D0}};
// ---- area 61 ----
constexpr ah::CallSite kCalls40B5C0[] = {{0x0, 0x57C7C0}};
// ---- area 62 ----
constexpr ah::CallSite kCalls40B5E0[] = {{0x1D, 0x57C7C0}};
constexpr ah::CallSite kCalls40B620[] = {{0x29, 0x57C7A0}, {0x52, 0x591680}, {0x80, 0x590BB0}, {0x96, 0x587740},
                                         {0x9C, 0x497710}, {0xA8, 0x57C0F0}, {0xC0, 0x497710}, {0xE2, 0x587740}};
// ---- areas 63, 64 ----
// Each init's first instruction is Capcom's own `jmp +0x10` over eleven nops
// (the exe as shipped), which CloneOriginal reads as a patched entry: the
// clones start at the jump's target (0x40B730, 0x40B800), the body every
// call runs, and the offsets below are from there (the whole function's
// 0x12, 0x4D, 0x98 less 0x10).
constexpr ah::CallSite kCallsPlace[] = {{0x2, 0x5B93D2}, {0x3D, 0x5B93D2}, {0x88, 0x5720C0}};

#define W1D_CLONE(name, base, size, calls, n, ret, shape) \
    {#name, base, size, calls, n, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::name), ret, false, ah::Shape::shape}

const ah::Clone kClones53[] = {
    W1D_CLONE(Area53_ChoiceFocusPair, 0x40AB00, 0x3C, nullptr, 0, 0x0, kChoice),
    W1D_CLONE(Area53_Trigger42, 0x40AB40, 0x1B, kCalls40AB40, AH_N(kCalls40AB40), 0xFF, kCallee),
};
enum : unsigned { k53Choice, k53Trigger };
const ah::Clone kClones55[] = {
    W1D_CLONE(Area55_ChoiceTakeItem8, 0x40AB60, 0x58, kCalls40AB60, AH_N(kCalls40AB60), 0x0, kChoice),
    W1D_CLONE(Area55_ChoiceMark24, 0x40ABC0, 0x24, nullptr, 0, 0x0, kChoice),
    W1D_CLONE(Area55_Trigger23, 0x40ABF0, 0x16, kCalls40ABF0, AH_N(kCalls40ABF0), 0xFF, kCallee),
};
enum : unsigned { k55Take, k55Mark, k55Trigger };
const ah::Clone kClones56[] = {
    W1D_CLONE(Area56_ChoicePoseFlag8, 0x40AC10, 0x6B, kCallsPoseFlag, AH_N(kCallsPoseFlag), 0x0, kChoice),
    W1D_CLONE(Area56_ChoicePoseFlag9, 0x40AC80, 0x6B, kCallsPoseFlag, AH_N(kCallsPoseFlag), 0x0, kChoice),
    W1D_CLONE(Area56_ChoicePoseFlagA, 0x40ACF0, 0x6B, kCallsPoseFlag, AH_N(kCallsPoseFlag), 0x0, kChoice),
    W1D_CLONE(Area56_ChoicePoseFlagB, 0x40AD60, 0x6B, kCallsPoseFlag, AH_N(kCallsPoseFlag), 0x0, kChoice),
    W1D_CLONE(Area56_ChoicePoseFlagC, 0x40ADD0, 0x6B, kCallsPoseFlag, AH_N(kCallsPoseFlag), 0x0, kChoice),
    W1D_CLONE(Area56_ChoicePoseFlagD, 0x40AE40, 0x6B, kCallsPoseFlag, AH_N(kCallsPoseFlag), 0x0, kChoice),
    W1D_CLONE(Area56_ChoicePoseFlagE, 0x40AEB0, 0x6B, kCallsPoseFlag, AH_N(kCallsPoseFlag), 0x0, kChoice),
    W1D_CLONE(Area56_ChoiceMessage9, 0x40AF20, 0x18, nullptr, 0, 0x0, kChoice),
    W1D_CLONE(Area56_ChoiceMessageA, 0x40AF40, 0x18, nullptr, 0, 0x0, kChoice),
    W1D_CLONE(Area56_ChoiceMessageB, 0x40AF60, 0x18, nullptr, 0, 0x0, kChoice),
    W1D_CLONE(Area56_ChoiceMessageC, 0x40AF80, 0x1A, nullptr, 0, 0x0, kChoice),
    W1D_CLONE(Area56_ChoiceMessageD, 0x40AFA0, 0x1A, nullptr, 0, 0x0, kChoice),
    W1D_CLONE(Area56_ChoiceMessageE, 0x40AFC0, 0x1A, nullptr, 0, 0x0, kChoice),
    W1D_CLONE(Area56_ChoicePoseOn1, 0x40AFE0, 0x3F, kCalls40AFE0, AH_N(kCalls40AFE0), 0x0, kChoice),
    W1D_CLONE(Area56_ChoicePoseIfAllFlags, 0x40B020, 0x58, kCalls40B020, AH_N(kCalls40B020), 0x0, kChoice),
    W1D_CLONE(Area56_FallRun, 0x40B080, 0x12, nullptr, 0, 0x0, kHandler),
    W1D_CLONE(Area56_FallBegin, 0x40B0A0, 0x72, kCalls40B0A0, AH_N(kCalls40B0A0), 0x0, kState),
    W1D_CLONE(Area56_FallStep, 0x40B120, 0x90, kCalls40B120, AH_N(kCalls40B120), 0x0, kState),
    W1D_CLONE(Area56_FallEnd, 0x40B1B0, 0x13, nullptr, 0, 0x0, kState),
    W1D_CLONE(Area56_Trigger52, 0x40B1D0, 0x40, kCallsGather, AH_N(kCallsGather), 0xFF, kCallee),
};
enum : unsigned {
    k56Pose8, k56Pose9, k56PoseA, k56PoseB, k56PoseC, k56PoseD, k56PoseE, k56Msg9, k56MsgA, k56MsgB, k56MsgC, k56MsgD, k56MsgE,
    k56PoseOn1, k56PoseIfAll, k56FallRun, k56FallBegin, k56FallStep, k56FallEnd, k56Trigger
};
const ah::Clone kClones57[] = {
    W1D_CLONE(Area57_ChoiceMessage, 0x40B210, 0x17, nullptr, 0, 0x0, kChoice),
    W1D_CLONE(Area57_SpawnKind2AtLeader, 0x40B230, 0x42, kCalls40B230, AH_N(kCalls40B230), 0x0, kHandler),
    W1D_CLONE(Area57_SpawnKind4AtLeader, 0x40B280, 0x42, kCalls40B280, AH_N(kCalls40B280), 0x0, kHandler),
    W1D_CLONE(Area57_StopMusic, 0x40B2D0, 0x5, kCalls40B2D0, AH_N(kCalls40B2D0), 0x0, kHandler),
    W1D_CLONE(Area57_ResumeSound, 0x40B2E0, 0x5, kCalls40B2E0, AH_N(kCalls40B2E0), 0x0, kHandler),
    W1D_CLONE(Area57_Trigger53, 0x40B2F0, 0x40, kCallsGather, AH_N(kCallsGather), 0xFF, kCallee),
};
enum : unsigned { k57Choice, k57Spawn2, k57Spawn4, k57Stop, k57Resume, k57Trigger };
const ah::Clone kClones59[] = {
    W1D_CLONE(Area59_ChoiceMessage2or4, 0x40B330, 0x24, nullptr, 0, 0x0, kChoice),
    W1D_CLONE(Area59_ClearCells, 0x40B360, 0x46, kCalls40B360, AH_N(kCalls40B360), 0x0, kHandler),
    W1D_CLONE(Area59_SetCells, 0x40B3B0, 0x58, kCalls40B3B0, AH_N(kCalls40B3B0), 0x0, kHandler),
    W1D_CLONE(Area59_StepHook, 0x40B410, 0x5E, kCalls40B410, AH_N(kCalls40B410), 0xFF, kHook),
    W1D_CLONE(Area59_Trigger24, 0x40B470, 0x5D, kCalls40B470, AH_N(kCalls40B470), 0xFF, kCallee),
    W1D_CLONE(Area59_EffectRun, 0x40B4D0, 0x12, nullptr, 0, 0x0, kState),
    W1D_CLONE(Area59_EffectGround, 0x40B4F0, 0x2D, kCalls40B4F0, AH_N(kCalls40B4F0), 0x0, kState),
    W1D_CLONE(Area59_EffectStep, 0x40B520, 0x2B, kCalls40B520, AH_N(kCalls40B520), 0x0, kState),
};
enum : unsigned { k59Choice, k59Clear, k59Set, k59Hook, k59Trigger, k59Run, k59Ground, k59Step };
const ah::Clone kClones60[] = {
    W1D_CLONE(Area60_InitScriptFlag2000, 0x40B550, 0x8, nullptr, 0, 0x0, kInit),
};
const ah::Clone kClones61[] = {
    W1D_CLONE(Area61_ChoiceClearZenny, 0x40B560, 0x2E, nullptr, 0, 0x0, kChoice),
    W1D_CLONE(Area61_ChoiceMark4, 0x40B590, 0x24, nullptr, 0, 0x0, kChoice),
    W1D_CLONE(Area61_Trigger21, 0x40B5C0, 0x16, kCalls40B5C0, AH_N(kCalls40B5C0), 0xFF, kCallee),
};
enum : unsigned { k61Zenny, k61Mark, k61Trigger };
const ah::Clone kClones62[] = {
    W1D_CLONE(Area62_ArmTailGiveItem, 0x40B5E0, 0x3D, kCalls40B5E0, AH_N(kCalls40B5E0), 0x0, kHandler),
    W1D_CLONE(Area62_TailGiveItem59, 0x40B620, 0xF3, kCalls40B620, AH_N(kCalls40B620), 0x0, kTail),
};
enum : unsigned { k62Arm, k62Tail };
const ah::Clone kClones63[] = {
    W1D_CLONE(Area63_InitPlaceObject, 0x40B730, 0xBB, kCallsPlace, AH_N(kCallsPlace), 0x0, kInit),
};
const ah::Clone kClones64[] = {
    W1D_CLONE(Area64_InitPlaceObject, 0x40B800, 0xBB, kCallsPlace, AH_N(kCallsPlace), 0x0, kInit),
};
#undef W1D_CLONE
#undef AH_N

std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> std::uint32_t KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
unsigned char& B(std::uint32_t address) { return *ah::Mem(address); }

// A record the active member pointer may name: one of the four party objects
// (Sprite_ObjectsExtra), a field object, a party record, or the running
// object itself (the two alias in the game).
unsigned char* MemberRecord(std::uint32_t v) {
    switch (v % 4) {
    case 0: return ah::Mem(at::kSpriteObjectsExtra + (v >> 2) % 4 * at::kObjectStride);
    case 1: return ah::Object(v >> 2);
    case 2: return ah::PartyOf(static_cast<unsigned char>(v >> 2));
    default: return Sprite_Current;
    }
}
// What the focus pointer 0x903804 may name: a field object (its index
// exact), one of the four extra objects or a party record (far past the
// field objects), or a byte of the message box's cells just before
// Sprite_Objects (a negative offset: the division truncates toward 0).
unsigned char* FocusRecord(std::uint32_t v) {
    switch (v % 5) {
    case 0: case 1: return ah::Object(v >> 3);
    case 2: return ah::Mem(at::kSpriteObjectsExtra + (v >> 3) % 4 * at::kObjectStride);
    case 3: return ah::PartyOf(static_cast<unsigned char>(v >> 3));
    default: return ah::Mem(at::kSpriteObjects - 1 - (v >> 3) % 0x3C);
    }
}
unsigned char* EffectRecord(std::uint32_t v) { return ah::Mem(at::kEffectObjects + v % 4 * at::kEffectStride); }

// ---- the stand-ins the group lists ----

constexpr std::uint32_t kAll = 0xFFFFFFFFu, kU8 = 0xFFu, kU16 = 0xFFFFu;

// Louder than the real callees, on purpose (each only half the time, from
// Noise): after these calls the callers read a cell again - the active
// member and the focus pointer after ScriptFlags_Set40, the answer byte
// after MoveCmd_TestFB and Flags_Set (area 56's choices), counter 0 and Cond
// row 3's second byte after Sound_PlayEffect (area 62's tail stores the
// counter before the sound; area 56's choice 14 reads the byte after it),
// Sprite_Current after the ground, the step tick, the elevation and
// Effect_Spawn (area 56's fall, area 59's effect, area 57's spawns). The
// harness's own disturbance reaches a group cell about one call in 24.
std::uint32_t MovesAnswer(const std::uint32_t*, std::uint32_t answer) {
    const std::uint32_t n = ah::Noise();
    if (n & 1) B(at::kChoiceAnswer) = static_cast<unsigned char>((n >> 8) % 4);
    return answer;
}
std::uint32_t MovesCurrent(const std::uint32_t*, std::uint32_t answer) {
    const std::uint32_t n = ah::Noise();
    if (n & 1) Sprite_Current = n & 0x100 ? ah::PartyOf(static_cast<unsigned char>(n >> 9)) : ah::Object((n >> 9) & 3);
    return answer;
}
// ScriptFlags_Set40: area 62's handler reads the active member again after
// it, area 59's trigger 24 the focus pointer.
std::uint32_t Set40Effect(const std::uint32_t*, std::uint32_t answer) {
    const std::uint32_t n = ah::Noise();
    if (n & 1) ah::SetPointer(at::kActiveMember, MemberRecord(n >> 8));
    if (n & 2) ah::SetPointer(at::kFocusObject, FocusRecord(n >> 12));
    return answer;
}
std::uint32_t SoundEffect(const std::uint32_t*, std::uint32_t answer) {
    const std::uint32_t n = ah::Noise();
    if (n & 1) B(at::kCounter0) = static_cast<unsigned char>(n >> 8);
    if (n & 2) B(at::kCondRow3 + 1) = static_cast<unsigned char>(n & 4 ? (n >> 16) : ((n >> 16) & 1 ? 0x7F : 0xFF));
    return answer;
}
// The ground under the running object (after it may have moved): a third of
// the time its height +0x3E less one, equal or one more (FallStep's signed
// word compare), else the recorder's word.
std::uint32_t GroundEffect(const std::uint32_t* a, std::uint32_t answer) {
    MovesCurrent(a, answer);
    const std::uint32_t n = ah::Noise();
    if (n % 3 != 0) return answer;
    const auto height = move_script::Word(Sprite_Current + 0x3E);
    return (answer & 0xFFFF0000u) | static_cast<std::uint16_t>(height + (n >> 8) % 3 - 1);
}
// Inventory_Count answers a word its caller tests whole: 0 (garbage above) a
// third of the time.
std::uint32_t CountAnswer(const std::uint32_t*, std::uint32_t answer) {
    return ah::Noise() % 3 == 0 ? answer & 0xFFFF0000u : answer;
}
// Item_NamePtr answers a pointer the tail copies 16 bytes from: a buffer of
// noise the same on both passes.
unsigned char g_name[16];
std::uint32_t NameEffect(const std::uint32_t*, std::uint32_t) {
    ah::FillBytes(g_name, sizeof g_name);
    return Key(g_name);
}

#define W1D_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
#define W1D_THEIRS(name) #name, KeyOf(name), KeyOf(name)
const ah::Callee kCallees[] = {
    {W1D_OURS(ScriptFlags_Set40), 0, {}, ah::Answer::kGarbage, 0, 0, {}, &Set40Effect},
    {W1D_OURS(ScriptFlags_Clear40), 0, {}, ah::Answer::kGarbage, 0, 0},
    {W1D_OURS(MoveCmd_TestFB), 2, {kU16, kU16}, ah::Answer::kFlag, 0, 0, {}, &MovesAnswer},
    {W1D_OURS(Flags_Set), 2, {kAll, kU8}, ah::Answer::kGarbage, 0, 0, {}, &MovesAnswer},
    {W1D_OURS(Sound_PlayEffect), 1, {kU16}, ah::Answer::kGarbage, 0, 0, {}, &SoundEffect},
    {W1D_OURS(MapView_GroundAt), 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &GroundEffect},
    {W1D_OURS(Field_LeaderStepTick), 0, {}, ah::Answer::kFlag, 0, 0, {}, &MovesCurrent},
    // the elevation's low word only (it keeps the word sign-extended and adds (old - new) * 2 as a word)
    {W1D_OURS(MapView_SetElevation), 1, {kU16}, ah::Answer::kGarbage, 0, 0},
    {W1D_OURS(AreaMap_Elevation), 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &MovesCurrent},
    // a slot 0..2, or none (0xFF)
    {W1D_THEIRS(Effect_Spawn), 5, {kU8, kU8, kU8, kU16, kU16}, ah::Answer::kByte, 0xFE, 0x02, {}, &MovesCurrent},
    {W1D_THEIRS(Sound_StopMusic), 0, {}, ah::Answer::kGarbage, 0, 0},
    {W1D_OURS(Inventory_Count), 3, {kAll, kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &CountAnswer},
    {W1D_OURS(Inventory_Remove), 3, {kAll, kAll, kAll}, ah::Answer::kGarbage, 0, 0},
    {W1D_OURS(Item_NamePtr), 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &NameEffect},
    {"PositionHook_4220D0", at::kPositionHook, at::kPositionHook, 1, {kAll}, ah::Answer::kGarbage, 0, 0, {12}},
};
#undef W1D_OURS
#undef W1D_THEIRS

// The two dispatchers' .data state tables, swapped for recorders while the
// fuzz runs.
const ah::DataTable kTables56[] = {{at::kArea56FallStates, at::kArea56FallStateCount}};
const ah::DataTable kTables59[] = {{at::kArea59EffectStates, at::kArea59EffectStateCount}};

// Beyond the field frame: the effect records (slots 0..3, area 59's effect
// running in one), the focus pointer, the active member pointer, the mark,
// Text_Records' first 16 bytes, and areas 63 and 64's weights (.data, so the
// fuzz can draw weights the image does not hold; put back after the run).
const ah::Region kRegions[] = {
    {at::kEffectObjects, 4 * at::kEffectStride},
    {at::kFocusObject, 4},
    {at::kActiveMember, 4},
    {at::kAnswerMark, 1},
    {at::kTextRecords, 16},
    {at::kArea63Weights, 8},
    {at::kArea64Weights, 8},
};

// The image's weights, read before the first run (the regions are random
// every round).
unsigned char g_weights63[8], g_weights64[8];

// Every round: the pointers the areas follow put back inside the regions.
void Common() {
    ah::SetPointer(at::kActiveMember, MemberRecord(ah::Next()));
    ah::SetPointer(at::kFocusObject, FocusRecord(ah::Next()));
}

// The group's cells, moved by the harness's disturbance about one call in
// 24 - drawn only from h (area_harness.h: a group disturb never draws Next).
void Disturb(std::uint32_t h) {
    const auto v = static_cast<unsigned char>(h >> 20);
    switch ((h >> 8) % 7) {
    case 0: B(at::kTailState) = static_cast<unsigned char>(h & 0x100 ? v % 4 : v); break;
    case 1: B(at::kCounter0) = static_cast<unsigned char>(h & 0x100 ? 0xA + v % 3 : v); break;
    case 2: B(at::kChoiceAnswer) = static_cast<unsigned char>(v % 4); break;
    case 3: ah::SetPointer(at::kActiveMember, MemberRecord(h >> 16)); break;
    case 4: ah::SetPointer(at::kFocusObject, FocusRecord(h >> 16)); break;
    case 5: B(at::kCondRow3 + 1) = static_cast<unsigned char>(h & 0x100 ? 0x7F : v); break;
    default: B(at::kCounter3) = v; break;
    }
}

// A choice answer: each value a handler tests, its neighbours, a negative
// byte (the table indexes are signed), anything.
void SeedAnswer() {
    if (ah::Often()) B(at::kChoiceAnswer) = static_cast<unsigned char>(AH_PICK(0, 1, 2, 3, 0, 1, 2, 0xFF, 0x80, 0x81, 0x7F, 4, 5));
}
// An object trigger is called (a field object, 0x904030).
void ArgsTrigger(unsigned, std::uint32_t* a) {
    a[0] = Key(ah::Object(a[0]));
    a[1] = at::kStoryFlags;
}

// ---- area 53 ----
void Seed53(unsigned k) {
    Common();
    if (k == k53Choice) SeedAnswer();
}

// ---- area 55 ----
void Seed55(unsigned k) {
    Common();
    if (k != k55Trigger) SeedAnswer();
}

// ---- area 56 ----
void Seed56(unsigned k) {
    Common();
    switch (k) {
    case k56PoseIfAll:
        // the seven flags all set half the time (bit 7 either way), else one clear or anything
        if (ah::Half()) B(at::kCondRow3 + 1) = static_cast<unsigned char>(ah::Half() ? 0x7F : 0xFF);
        else if (ah::Half()) B(at::kCondRow3 + 1) = static_cast<unsigned char>(0x7F & ~(1u << ah::Next() % 7));
        SeedAnswer();
        break;
    case k56FallRun:
        Sprite_Current[4] = static_cast<unsigned char>(ah::Next() % at::kArea56FallStateCount);
        break;
    case k56FallBegin: break;
    case k56FallStep:
        // the height at the signed word's edges, +5 at 0 half the time, the two flags' bit 3 clear half the time
        if (ah::Half()) move_script::SetWord(Sprite_Current + 0x3E, AH_PICK(0x7FFF, 0x8000, 0xFFFF, 0, 0x7FF, 0x8001));
        if (ah::Half()) Sprite_Current[5] = 0;
        if (ah::Half()) Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 & ~8u);
        if (ah::Half()) Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags & ~8u);
        break;
    case k56FallEnd: case k56Trigger: break;
    default: SeedAnswer(); break;
    }
}

// ---- area 57 ----
void Seed57(unsigned k) {
    Common();
    switch (k) {
    case k57Choice: SeedAnswer(); break;
    case k57Spawn2: case k57Spawn4:
        if (ah::Often()) B(at::kPartyList0) = static_cast<unsigned char>(ah::Next() % 8);
        break;
    default: break;
    }
}

// ---- area 59 ----
void Seed59(unsigned k) {
    Common();
    switch (k) {
    case k59Choice: SeedAnswer(); break;
    case k59Hook:
        // half the rounds every test passes but at most one, drawn to fail
        if (ah::Half()) {
            Cond_ByteFD = 2;
            B(at::kLeaderPose) = static_cast<unsigned char>(AH_PICK(0, 6, 7));
            ah::Mem(at::kStoryFlags)[0x33 >> 3] |= 1u << (0x33 & 7);   // for the record: Flags_Test is a recorder
            if (ah::Half()) {
                if (ah::Half()) Cond_ByteFD = static_cast<unsigned char>(AH_PICK(1, 3, 0x82));
                else B(at::kLeaderPose) = static_cast<unsigned char>(AH_PICK(1, 5, 8, 0x80, 0x86));
            }
            break;
        }
        if (ah::Often()) Cond_ByteFD = static_cast<unsigned char>(AH_PICK(2, 1, 3, 0x82));
        if (ah::Often()) B(at::kLeaderPose) = static_cast<unsigned char>(AH_PICK(0, 6, 7, 1, 5, 8, 0x80));
        break;
    case k59Run:
        if (ah::Half()) Sprite_Current = EffectRecord(ah::Next());
        Sprite_Current[1] = static_cast<unsigned char>(ah::Next() % at::kArea59EffectStateCount);
        break;
    case k59Ground: case k59Step:
        if (ah::Half()) Sprite_Current = EffectRecord(ah::Next());
        break;
    default: break;
    }
}
// The hook's (x, z): 16.16 positions whose high words sit on and beside the
// 3 x 3 rectangle (0x46..0x48, 0x29..0x2B) two times in three; each side
// drawn alone so one edge can be off while the other is on.
void Args59(unsigned k, std::uint32_t* a) {
    if (k == k59Trigger) {
        ArgsTrigger(k, a);
        return;
    }
    if (k != k59Hook) return;
    if (ah::Half()) {
        a[0] = (0x46 + ah::Next() % 3) << 16 | (a[0] & 0xFFFF);
        a[1] = (0x29 + ah::Next() % 3) << 16 | (a[1] & 0xFFFF);
        if (ah::Half()) {
            if (ah::Half()) a[0] = static_cast<std::uint32_t>(AH_PICK(0x45, 0x49, 0x145, 0xFF46)) << 16 | (a[0] & 0xFFFF);
            else a[1] = static_cast<std::uint32_t>(AH_PICK(0x28, 0x2C, 0x129, 0xFF29)) << 16 | (a[1] & 0xFFFF);
        }
        return;
    }
    if (ah::Often()) a[0] = (0x45 + ah::Next() % 5) << 16 | (a[0] & 0xFFFF);
    if (ah::Often()) a[1] = (0x28 + ah::Next() % 5) << 16 | (a[1] & 0xFFFF);
}

// ---- area 60 ----
void Seed60(unsigned) { Common(); }

// ---- area 61 ----
void Seed61(unsigned k) {
    Common();
    if (k != k61Trigger) SeedAnswer();
}

// ---- area 62 ----
void Seed62(unsigned k) {
    Common();
    if (k == k62Arm) {
        if (ah::Half()) B(at::kLeaderByte89) = static_cast<unsigned char>(AH_PICK(6, 6, 5, 7, 0x86));
        return;
    }
    // the tail: each state, its neighbours and the out-of-range sides; then
    // the cell that state waits on, at its value two times in three
    const auto state = static_cast<unsigned char>(AH_PICK(0, 1, 2, 0, 1, 2, 3, 0xFF, 0x80, 0x7F));
    B(at::kTailState) = state;
    const bool on = ah::Often();
    if (state == 1) B(at::kCounter0) = static_cast<unsigned char>(on ? 0xB : AH_PICK(0xA, 0xC, 0x8B, 0));
    if (state == 2) Field_Request = static_cast<unsigned char>(on ? AH_PICK(0, 1, 3, 5) : 2);
}

// ---- areas 63, 64 ----
// The weights: the image's two times in three (the object picked by the
// draw), else all zero (none picked: the path the image's weights, which
// sum to 64, never take), one weight 64, or noise.
void SeedWeights(std::uint32_t at, const unsigned char* image) {
    unsigned char* const w = ah::Mem(at);
    switch (ah::Next() % 6) {
    case 0: std::memset(w, 0, 8); break;
    case 1: std::memset(w, 0, 8); w[ah::Next() % 8] = 0x40; break;
    case 2: break;   // the random fill
    default: std::memcpy(w, image, 8); break;
    }
}
void Seed63(unsigned) {
    Common();
    SeedWeights(at::kArea63Weights, g_weights63);
}
void Seed64(unsigned) {
    Common();
    SeedWeights(at::kArea64Weights, g_weights64);
}

void RunArea(int area, const ah::Clone* clones, unsigned n, const ah::DataTable* tables, unsigned n_tables,
             void (*seed)(unsigned), void (*args)(unsigned, std::uint32_t*), unsigned rounds) {
    ah::Group g{"area_w1d", clones, n, kCallees, sizeof kCallees / sizeof kCallees[0], tables, n_tables,
                kRegions, sizeof kRegions / sizeof kRegions[0], seed, &Disturb, rounds};
    g.args = args;
    g.area = area;
    ah::Run(g);
}

}  // namespace

void SelfTest() {
    constexpr unsigned kRounds = 6000;
    std::memcpy(g_weights63, ah::Mem(at::kArea63Weights), 8);
    std::memcpy(g_weights64, ah::Mem(at::kArea64Weights), 8);
    RunArea(53, kClones53, sizeof kClones53 / sizeof kClones53[0], nullptr, 0, &Seed53, &ArgsTrigger, kRounds);
    RunArea(55, kClones55, sizeof kClones55 / sizeof kClones55[0], nullptr, 0, &Seed55, &ArgsTrigger, kRounds);
    RunArea(56, kClones56, sizeof kClones56 / sizeof kClones56[0], kTables56, 1, &Seed56, &ArgsTrigger, kRounds);
    RunArea(57, kClones57, sizeof kClones57 / sizeof kClones57[0], nullptr, 0, &Seed57, &ArgsTrigger, kRounds);
    RunArea(59, kClones59, sizeof kClones59 / sizeof kClones59[0], kTables59, 1, &Seed59, &Args59, kRounds);
    RunArea(60, kClones60, 1, nullptr, 0, &Seed60, nullptr, kRounds);
    RunArea(61, kClones61, sizeof kClones61 / sizeof kClones61[0], nullptr, 0, &Seed61, &ArgsTrigger, kRounds);
    RunArea(62, kClones62, sizeof kClones62 / sizeof kClones62[0], nullptr, 0, &Seed62, nullptr, kRounds);
    RunArea(63, kClones63, 1, nullptr, 0, &Seed63, nullptr, kRounds);
    RunArea(64, kClones64, 1, nullptr, 0, &Seed64, nullptr, kRounds);
}

}  // namespace area_w1d

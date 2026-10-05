// BOF3X_SHADOW=rest_4a: group R4A's 48 functions through the scenario harness
// (scenario_harness.h, used unchanged) in field mode, once at start-up.
// docs/rest_4a.md section 4. BOF3X_R4A_ONLY=<name> runs the clones whose name
// contains it (the controls' speed-up).
//
// The clone rows are tools/band_rows.py --group R4A --clones --harness scenario
// (2026-10-05, through the round's band14.py wrapper), each extent read again
// to its last instruction (capstone); the cut's sizes are padding past them.
// Shapes: every function kCall (none is a state handler a chapter table
// reaches: the community's are called by CommuSim_AreaEnter and each other,
// the battle's by the battle's code, the triggers through Field_ObjectTriggers
// with (object, 0x904030)); ret_mask 0xFF where a caller reads al. Two groups:
// the 47, and Battle_RandomLiveEnemy alone, whose Battle_ActorIsOut stand-in
// levels the six frame bytes the original never writes (docs/rest_4a.md
// section 7, L1) on the original's side. Every callee the group's code calls
// that no standard set lists, or lists otherwise than the group needs, is
// listed here (registered before the standard rows: the group's listing
// stands).
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/rest_4a.h"
#include "game/rest_4a_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace rest_4a {
namespace {

namespace sh = scenario_harness;
using U = std::uint32_t;
using move_script::SetLong;
using move_script::SetWord;

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
unsigned char* Mem(U a) { return sh::Mem(a); }
template <typename... T> U PickOf(T... v) {
    const U values[] = {static_cast<U>(v)...};
    return sh::Pick(values, sizeof...(v));
}
void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }
U UL(const unsigned char* p) { return static_cast<U>(move_script::Long(p)); }

// --- the clone table (band_rows.py --group R4A --clones --harness scenario, 2026-10-05) ---
constexpr sh::CallSite kCalls452DD0[] = {{0x15, 0x4456C0}};
constexpr sh::CallSite kCalls452EB0[] = {{0x13, 0x4456C0}, {0x42, 0x5B93D2}};
constexpr sh::CallSite kCalls452F10[] = {{0x14, 0x4456C0}, {0x43, 0x5B93D2}};
constexpr sh::CallSite kCalls454290[] = {{0x27, 0x5B93D2}, {0x46, 0x454310}, {0x4D, 0x5B93D2}, {0x57, 0x445730}};
constexpr sh::CallSite kCalls454310[] = {{0x13, 0x4456C0}, {0x48, 0x5B93D2}};
constexpr sh::CallSite kCalls455300[] = {{0x69, 0x455380}};
constexpr sh::CallSite kCalls455450[] = {{0x16, 0x45E6B0}, {0x1F, 0x4560D0}, {0x45, 0x455F40}, {0x9F, 0x4561A0}, {0xA8, 0x455540},
                                         {0xB2, 0x455700}, {0xB7, 0x4557A0}, {0xBC, 0x455870}, {0xC2, 0x4555D0}, {0xCA, 0x455AA0},
                                         {0xCF, 0x455BE0}, {0xD4, 0x455950}, {0xD9, 0x455DF0}, {0xDE, 0x456080}, {0xE3, 0x4561A0}};
constexpr sh::CallSite kCalls4555D0[] = {{0x81, 0x455F40}, {0xE3, 0x455FF0}};
constexpr sh::CallSite kCalls455AA0[] = {{0x8C, 0x5B93D2}, {0x9F, 0x45E6B0}, {0xEF, 0x5B93D2}};
constexpr sh::CallSite kCalls455BE0[] = {{0x160, 0x5B93D2}, {0x188, 0x5B93D2}};
constexpr sh::CallSite kCalls455F40[] = {{0x1, 0x5B93D2}};
constexpr sh::CallSite kCalls455FF0[] = {{0x6, 0x5B93D2}};
constexpr sh::CallSite kCalls4560D0[] = {{0x4B, 0x5B93D2}, {0x5B, 0x5B93D2}};
constexpr sh::CallSite kCalls4561A0[] = {{0x31, 0x456680}, {0x38, 0x4562B0}, {0x4B, 0x4563B0}, {0x68, 0x5B93D2}, {0xA7, 0x5B93D2}, {0xCB, 0x4562B0}};
constexpr sh::CallSite kCalls4562B0[] = {{0xA5, 0x5720C0}, {0xDD, 0x589590}, {0xEB, 0x57C4C0}};
constexpr sh::CallSite kCalls4563B0[] = {{0xC0, 0x5720C0},  {0x10D, 0x589590}, {0x11B, 0x57C4C0}, {0x13D, 0x456680}, {0x14D, 0x4566C0},
                                         {0x16F, 0x456750}, {0x191, 0x4567F0}, {0x1AE, 0x456870}, {0x1C3, 0x4568A0}, {0x1EA, 0x456920},
                                         {0x20C, 0x456990}, {0x22E, 0x456A10}, {0x250, 0x456AB0}, {0x26D, 0x456AF0}};
constexpr sh::JumpTable kTables4563B0[] = {{0x138, 0x28C, 14}};
constexpr sh::CallSite kCalls456680[] = {{0x0, 0x5B93D2}};
constexpr sh::CallSite kCalls456A10[] = {{0x45, 0x57C140}};
constexpr sh::CallSite kCalls456B20[] = {{0x10, 0x57C7C0}};
constexpr sh::CallSite kCalls456B70[] = {{0x15, 0x5B9380}, {0x2D, 0x5B9380}, {0x43, 0x5B9380}, {0x58, 0x5B9380}, {0x5F, 0x4976D0}};
constexpr sh::CallSite kCalls456BF0[] = {{0x10, 0x5B9380}, {0x26, 0x5B9380}, {0x3C, 0x5B9380}, {0x43, 0x4976D0}};
constexpr sh::CallSite kCalls456C50[] = {{0x18, 0x57C7C0}};
constexpr sh::CallSite kCalls456C70[] = {{0x18, 0x57C7C0}};
constexpr sh::CallSite kCalls456C90[] = {{0x10, 0x57C7C0}};
constexpr sh::CallSite kCalls456CB0[] = {{0x17, 0x57C7C0}};
constexpr sh::CallSite kCalls456CD0[] = {{0x17, 0x57C7C0}};
constexpr sh::CallSite kCalls456CF0[] = {{0x18, 0x57C7C0}};
constexpr sh::CallSite kCalls456D10[] = {{0x18, 0x57C7C0}};
constexpr sh::CallSite kCalls456D30[] = {{0x18, 0x57C7C0}};

#define R4A_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define R4A_CALLS(a) a, R4A_N(a)
#define R4A_FN(name) reinterpret_cast<const void*>(&::name)
constexpr sh::Shape kCa = sh::Shape::kCall;
constexpr U kAl = 0xFF;
// The triggers' (object, 0x904030): the object one of the first four Sprite_Objects records.
constexpr U kObj = sh::ArgAt(0, sh::Arg::kSprite);
// {name, base, size, calls, imms, tables, ours, ret_mask, calm, shape, pointers}
const sh::Clone kAll47[] = {
    {"Battle_AutoTargetCheck", 0x452DD0, 0xD7, R4A_CALLS(kCalls452DD0), nullptr, 0, nullptr, 0, R4A_FN(Battle_AutoTargetCheck), kAl, false, kCa},
    {"Battle_RandomLiveMember", 0x452EB0, 0x5D, R4A_CALLS(kCalls452EB0), nullptr, 0, nullptr, 0, R4A_FN(Battle_RandomLiveMember), kAl, false, kCa},
    {"Battle_MemberActionIs0E", 0x454260, 0x2B, nullptr, 0, nullptr, 0, nullptr, 0, R4A_FN(Battle_MemberActionIs0E), kAl, false, kCa},
    {"Battle_MemberAutoFixed", 0x454290, 0x74, R4A_CALLS(kCalls454290), nullptr, 0, nullptr, 0, R4A_FN(Battle_MemberAutoFixed), 0, false, kCa},
    {"Battle_RandomOtherMember", 0x454310, 0x63, R4A_CALLS(kCalls454310), nullptr, 0, nullptr, 0, R4A_FN(Battle_RandomOtherMember), kAl, false, kCa},
    {"Field_RunSlot", 0x455300, 0x7B, R4A_CALLS(kCalls455300), nullptr, 0, nullptr, 0, R4A_FN(Field_RunSlot), 0, false, kCa},
    {"Field_SlotClutCopy", 0x455380, 0xC5, nullptr, 0, nullptr, 0, nullptr, 0, R4A_FN(Field_SlotClutCopy), 0, false, kCa},
    {"CommuSim_AreaEnter", 0x455450, 0xEC, R4A_CALLS(kCalls455450), nullptr, 0, nullptr, 0, R4A_FN(CommuSim_AreaEnter), 0, false, kCa},
    {"CommuSim_QueueAreas", 0x455540, 0x8D, nullptr, 0, nullptr, 0, nullptr, 0, R4A_FN(CommuSim_QueueAreas), 0, false, kCa},
    {"CommuSim_Population", 0x4555D0, 0x129, R4A_CALLS(kCalls4555D0), nullptr, 0, nullptr, 0, R4A_FN(CommuSim_Population), 0, false, kCa},
    {"CommuSim_Mood", 0x455700, 0x9C, nullptr, 0, nullptr, 0, nullptr, 0, R4A_FN(CommuSim_Mood), 0, false, kCa},
    {"CommuSim_LevelLit", 0x4557A0, 0xD0, nullptr, 0, nullptr, 0, nullptr, 0, R4A_FN(CommuSim_LevelLit), 0, false, kCa},
    {"CommuSim_LevelDark", 0x455870, 0xD4, nullptr, 0, nullptr, 0, nullptr, 0, R4A_FN(CommuSim_LevelDark), 0, false, kCa},
    {"CommuSim_TickKind5", 0x455950, 0x14B, nullptr, 0, nullptr, 0, nullptr, 0, R4A_FN(CommuSim_TickKind5), 0, false, kCa},
    {"CommuSim_TickKind9", 0x455AA0, 0x13B, R4A_CALLS(kCalls455AA0), nullptr, 0, nullptr, 0, R4A_FN(CommuSim_TickKind9), 0, false, kCa},
    {"CommuSim_TickKindD", 0x455BE0, 0x204, R4A_CALLS(kCalls455BE0), nullptr, 0, nullptr, 0, R4A_FN(CommuSim_TickKindD), 0, false, kCa},
    {"CommuSim_TickKindB", 0x455DF0, 0x143, nullptr, 0, nullptr, 0, nullptr, 0, R4A_FN(CommuSim_TickKindB), 0, false, kCa},
    {"CommuSim_AddRecord", 0x455F40, 0xA4, R4A_CALLS(kCalls455F40), nullptr, 0, nullptr, 0, R4A_FN(CommuSim_AddRecord), 0, false, kCa},
    {"CommuSim_RemoveRecord", 0x455FF0, 0x89, R4A_CALLS(kCalls455FF0), nullptr, 0, nullptr, 0, R4A_FN(CommuSim_RemoveRecord), 0, false, kCa},
    {"CommuSim_SumKindsAB", 0x456080, 0x4E, nullptr, 0, nullptr, 0, nullptr, 0, R4A_FN(CommuSim_SumKindsAB), 0, false, kCa},
    {"CommuSim_RollOffers", 0x4560D0, 0xC3, R4A_CALLS(kCalls4560D0), nullptr, 0, nullptr, 0, R4A_FN(CommuSim_RollOffers), 0, false, kCa},
    {"CommuSim_PlaceObjects", 0x4561A0, 0x110, R4A_CALLS(kCalls4561A0), nullptr, 0, nullptr, 0, R4A_FN(CommuSim_PlaceObjects), 0, false, kCa},
    {"CommuSim_PlaceLoose", 0x4562B0, 0xF6, R4A_CALLS(kCalls4562B0), nullptr, 0, nullptr, 0, R4A_FN(CommuSim_PlaceLoose), 0, false, kCa},
    {"CommuSim_PlaceResident", 0x4563B0, 0x2C4, R4A_CALLS(kCalls4563B0), nullptr, 0, R4A_CALLS(kTables4563B0), R4A_FN(CommuSim_PlaceResident), 0, false, kCa},
    {"CommuPose_Kind0", 0x456680, 0x38, R4A_CALLS(kCalls456680), nullptr, 0, nullptr, 0, R4A_FN(CommuPose_Kind0), 0, false, kCa},
    {"CommuPose_Kind4", 0x4566C0, 0x8B, nullptr, 0, nullptr, 0, nullptr, 0, R4A_FN(CommuPose_Kind4), 0, false, kCa},
    {"CommuPose_Kind5", 0x456750, 0x93, nullptr, 0, nullptr, 0, nullptr, 0, R4A_FN(CommuPose_Kind5), 0, false, kCa},
    {"CommuPose_Kind6", 0x4567F0, 0x7A, nullptr, 0, nullptr, 0, nullptr, 0, R4A_FN(CommuPose_Kind6), 0, false, kCa},
    {"CommuPose_Kind7", 0x456870, 0x23, nullptr, 0, nullptr, 0, nullptr, 0, R4A_FN(CommuPose_Kind7), 0, false, kCa},
    {"CommuPose_Kind8", 0x4568A0, 0x76, nullptr, 0, nullptr, 0, nullptr, 0, R4A_FN(CommuPose_Kind8), 0, false, kCa},
    {"CommuPose_Kind9", 0x456920, 0x6A, nullptr, 0, nullptr, 0, nullptr, 0, R4A_FN(CommuPose_Kind9), 0, false, kCa},
    {"CommuPose_KindA", 0x456990, 0x7C, nullptr, 0, nullptr, 0, nullptr, 0, R4A_FN(CommuPose_KindA), 0, false, kCa},
    {"CommuPose_KindB", 0x456A10, 0x9B, R4A_CALLS(kCalls456A10), nullptr, 0, nullptr, 0, R4A_FN(CommuPose_KindB), 0, false, kCa},
    {"CommuPose_KindC", 0x456AB0, 0x39, nullptr, 0, nullptr, 0, nullptr, 0, R4A_FN(CommuPose_KindC), 0, false, kCa},
    {"CommuPose_KindD", 0x456AF0, 0x23, nullptr, 0, nullptr, 0, nullptr, 0, R4A_FN(CommuPose_KindD), 0, false, kCa},
    {"FieldTrigger01", 0x456B20, 0x18, R4A_CALLS(kCalls456B20), nullptr, 0, nullptr, 0, R4A_FN(FieldTrigger01), kAl, false, kCa, kObj},
    {"FieldTrigger09", 0x456B40, 0x2D, nullptr, 0, nullptr, 0, nullptr, 0, R4A_FN(FieldTrigger09), kAl, false, kCa, kObj},
    {"FieldTrigger02", 0x456B70, 0x71, R4A_CALLS(kCalls456B70), nullptr, 0, nullptr, 0, R4A_FN(FieldTrigger02), kAl, false, kCa, kObj},
    {"FieldTrigger03", 0x456BF0, 0x55, R4A_CALLS(kCalls456BF0), nullptr, 0, nullptr, 0, R4A_FN(FieldTrigger03), kAl, false, kCa, kObj},
    {"FieldTrigger04", 0x456C50, 0x20, R4A_CALLS(kCalls456C50), nullptr, 0, nullptr, 0, R4A_FN(FieldTrigger04), kAl, false, kCa, kObj},
    {"FieldTrigger05", 0x456C70, 0x20, R4A_CALLS(kCalls456C70), nullptr, 0, nullptr, 0, R4A_FN(FieldTrigger05), kAl, false, kCa, kObj},
    {"FieldTrigger06", 0x456C90, 0x18, R4A_CALLS(kCalls456C90), nullptr, 0, nullptr, 0, R4A_FN(FieldTrigger06), kAl, false, kCa, kObj},
    {"FieldTrigger07", 0x456CB0, 0x1F, R4A_CALLS(kCalls456CB0), nullptr, 0, nullptr, 0, R4A_FN(FieldTrigger07), kAl, false, kCa, kObj},
    {"FieldTrigger08", 0x456CD0, 0x1F, R4A_CALLS(kCalls456CD0), nullptr, 0, nullptr, 0, R4A_FN(FieldTrigger08), kAl, false, kCa, kObj},
    {"FieldTrigger10", 0x456CF0, 0x20, R4A_CALLS(kCalls456CF0), nullptr, 0, nullptr, 0, R4A_FN(FieldTrigger10), kAl, false, kCa, kObj},
    {"FieldTrigger61", 0x456D10, 0x20, R4A_CALLS(kCalls456D10), nullptr, 0, nullptr, 0, R4A_FN(FieldTrigger61), kAl, false, kCa, kObj},
    {"FieldTrigger11", 0x456D30, 0x20, R4A_CALLS(kCalls456D30), nullptr, 0, nullptr, 0, R4A_FN(FieldTrigger11), kAl, false, kCa, kObj},
};
const sh::Clone kEnemy[] = {
    {"Battle_RandomLiveEnemy", 0x452F10, 0x5E, R4A_CALLS(kCalls452F10), nullptr, 0, nullptr, 0, R4A_FN(Battle_RandomLiveEnemy), kAl, false, kCa},
};
#undef R4A_FN
#undef R4A_CALLS
#undef R4A_N

enum : unsigned {
    kAutoCheck, kLiveMember, kActionIs0E, kAutoFixed, kOtherMember, kRunSlot, kClutCopy, kAreaEnter, kQueueAreas,
    kPopulation, kMood, kLevelLit, kLevelDark, kTick5, kTick9, kTickD, kTickB, kAddRecord, kRemoveRecord, kSumsAB,
    kRollOffers, kPlaceObjects, kPlaceLoose, kPlaceResident, kPose0, kPose4, kPose5, kPose6, kPose7, kPose8, kPose9,
    kPoseA, kPoseB, kPoseC, kPoseD, kTrig01, kTrig09, kTrig02, kTrig03, kTrig04, kTrig05, kTrig06, kTrig07, kTrig08,
    kTrig10, kTrig61, kTrig11, kCount
};
static_assert(kCount == sizeof kAll47 / sizeof kAll47[0], "one enum entry a clone, in order");
constexpr unsigned kLiveEnemy = kCount;   // the second group's one clone, its own seed index

// --- the stand-ins' effects -----------------------------------------------------

// The actor Battle_ActorIsOut answers "standing" for in the round (set by
// Args, the same for both passes): the last of the picker's range not
// excluded, so the pickers' count is never 0 (ours aborts there, the original
// faults - neither can be compared). 0x100: none.
U g_standing = 0x100;
void Disturb(U h);
// The louder stand-ins (the group's case runs after one call in some hundreds
// otherwise; the controls of section 6 that miss a re-read were 0 without
// these): each moves the cell its callers read again, from its answer only.
U IsOutEffect(const U* a, U answer) {
    if ((answer >> 9) % 4 == 0) Mem(at::kForcedActor)[0] = static_cast<unsigned char>((answer >> 12) % 11);   // read again after it
    return (a[0] & 0xFF) == g_standing ? answer & 0xFFFFFF00u : answer;
}
// Rand: the CRT's is 0..0x7FFF; the harness's garbage answers would take the
// pickers' idiv below their list (ours aborts there). One time in four the
// group's own disturbance.
U RandEffect(const U*, U answer) {
    if ((answer >> 16) % 4 == 0) Disturb(answer * 0x2545F491u);
    return answer & 0x7FFF;
}
// Field_SlotClutCopy: the slot's +1 (Field_RunSlot reads it after) half the time.
U SlotCopyEffect(const U* a, U answer) {
    unsigned char* const slot = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a[0]));
    if ((answer >> 9) & 1 && sh::InRegions(slot, 2)) slot[1] = static_cast<unsigned char>(answer >> 12);
    return answer;
}
// R4D's 0x45E6B0: the removed count (CommuSim_TickKind9 reads it after) half the time.
U CountEffect(const U*, U answer) {
    if ((answer >> 10) & 1) Mem(at::kRemovedCount)[0] = static_cast<unsigned char>((answer >> 12) % 0x20);
    return answer;
}
// AreaMap_Elevation: a resident count (CommuSim_PlaceResident reads it after) half the time.
U ElevationEffect(const U*, U answer) {
    if ((answer >> 9) & 1) Mem(at::kResidentCounts + (answer >> 12) % 8)[0] = static_cast<unsigned char>((answer >> 16) % 4);
    return answer;
}

// Battle_RandomLiveEnemy's Battle_ActorIsOut. On the original's side (the
// clone runs with the harness inactive) it first writes 0 to the six bytes of
// its caller's frame the original never writes - the count dword's upper three
// (frame bytes 5..7) and the loop dword's (9..11) - which ours holds as 0:
// what L1 levels (docs/rest_4a.md section 7). The caller pushed one dword and
// called: its frame byte 0 is 8 bytes above that argument, 0x10 above this
// function's frame pointer. Only at the first call (actor 3): later the list
// may have written those bytes. Standing three times in four, actor 10 always.
__attribute__((noinline)) unsigned char __cdecl LevelledIsOut(unsigned actor) {
    if (!sh::g_active && (actor & 0xFF) == 3) {
        unsigned char* const frame = static_cast<unsigned char*>(__builtin_frame_address(0)) + 0x10;
        frame[5] = frame[6] = frame[7] = 0;
        frame[9] = frame[10] = frame[11] = 0;
    }
    const U n = sh::Noise();
    U answer = n % 4 == 0 ? (n | 0x10) : (n & 0xFFFFFF00u);
    if ((actor & 0xFF) == 10) answer &= 0xFFFFFF00u;
    sh::Record(bof3::addr::Battle_ActorIsOut, actor & 0xFF, answer & 0xFF);
    sh::Stir();
    return static_cast<unsigned char>(answer);
}

#define R4A_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
constexpr sh::Answer kG = sh::Answer::kGarbage, kFl = sh::Answer::kFlag;
constexpr U kAll = 0xFFFFFFFFu;
const sh::Callee kCallees[] = {
    // the group's own, called directly by the group's
    {R4A_OURS(Battle_RandomOtherMember), 1, {0xFF}, kG, 0, 0},
    {R4A_OURS(Field_SlotClutCopy), 1, {kAll}, kG, 0, 0, {}, &SlotCopyEffect},
    {R4A_OURS(CommuSim_QueueAreas), 0, {}, kG, 0, 0},
    {R4A_OURS(CommuSim_Population), 1, {0xFF}, kG, 0, 0},
    {R4A_OURS(CommuSim_Mood), 1, {0xFF}, kG, 0, 0},
    {R4A_OURS(CommuSim_LevelLit), 0, {}, kG, 0, 0},
    {R4A_OURS(CommuSim_LevelDark), 0, {}, kG, 0, 0},
    {R4A_OURS(CommuSim_TickKind5), 0, {}, kG, 0, 0},
    {R4A_OURS(CommuSim_TickKind9), 0, {}, kG, 0, 0},
    {R4A_OURS(CommuSim_TickKindD), 0, {}, kG, 0, 0},
    {R4A_OURS(CommuSim_TickKindB), 0, {}, kG, 0, 0},
    {R4A_OURS(CommuSim_AddRecord), 0, {}, kG, 0, 0},
    {R4A_OURS(CommuSim_RemoveRecord), 0, {}, kG, 0, 0},
    {R4A_OURS(CommuSim_SumKindsAB), 0, {}, kG, 0, 0},
    {R4A_OURS(CommuSim_RollOffers), 0, {}, kG, 0, 0},
    {R4A_OURS(CommuSim_PlaceObjects), 0, {}, kG, 0, 0},
    {R4A_OURS(CommuSim_PlaceLoose), 2, {0xFF, 0xFF}, kG, 0, 0},
    {R4A_OURS(CommuSim_PlaceResident), 2, {0xFF, 0xFF}, kG, 0, 0},
    {R4A_OURS(CommuPose_Kind0), 1, {0xFF}, kG, 0, 0, {}, &ElevationEffect},
    {R4A_OURS(CommuPose_Kind4), 2, {0xFF, 0xFF}, kG, 0, 0, {}, &ElevationEffect},
    {R4A_OURS(CommuPose_Kind5), 2, {0xFF, 0xFF}, kG, 0, 0, {}, &ElevationEffect},
    {R4A_OURS(CommuPose_Kind6), 2, {0xFF, 0xFF}, kG, 0, 0, {}, &ElevationEffect},
    {R4A_OURS(CommuPose_Kind7), 1, {0xFF}, kG, 0, 0, {}, &ElevationEffect},
    {R4A_OURS(CommuPose_Kind8), 3, {0xFF, 0xFF, 0xFF}, kG, 0, 0, {}, &ElevationEffect},
    {R4A_OURS(CommuPose_Kind9), 2, {0xFF, 0xFF}, kG, 0, 0, {}, &ElevationEffect},   // CommuSim_PlaceResident pushes a third, unread
    {R4A_OURS(CommuPose_KindA), 2, {0xFF, 0xFF}, kG, 0, 0, {}, &ElevationEffect},
    {R4A_OURS(CommuPose_KindB), 2, {0xFF, 0xFF}, kG, 0, 0, {}, &ElevationEffect},
    {R4A_OURS(CommuPose_KindC), 2, {0xFF, 0xFF}, kG, 0, 0, {}, &ElevationEffect},
    {R4A_OURS(CommuPose_KindD), 1, {0xFF}, kG, 0, 0, {}, &ElevationEffect},
    // R4D's, ours by address until the round's rebinding: the records in use, al
    {"0x45E6B0 (R4D)", at::kCommuCount, at::kCommuCount, 0, {}, sh::Answer::kByte, 0, 2, {}, &CountEffect},
    // ours, no standard set lists them (or not as these callers need)
    {R4A_OURS(Battle_ActorIsOut), 1, {0xFF}, kFl, 0, 0, {}, &IsOutEffect},   // reads the low byte (its evidence)
    {R4A_OURS(Battle_DefaultTarget), 1, {0xFF}, kG, 0, 0},
    {R4A_OURS(AreaMap_Elevation), 2, {kAll, kAll}, kG, 0, 0, {}, &ElevationEffect},   // the standard row, louder                   // reads the low byte (its evidence)
    {"Rand", KeyOf(Rand), KeyOf(Rand), 0, {}, sh::Answer::kRand, 0, 0, {}, &RandEffect},
};
const sh::Callee kEnemyCallees[] = {
    {R4A_OURS(Battle_ActorIsOut), 1, {0xFF}, kG, 0, 0, {}, nullptr, reinterpret_cast<const void*>(&LevelledIsOut)},
    {"Rand", KeyOf(Rand), KeyOf(Rand), 0, {}, sh::Answer::kRand, 0, 0, {}, &RandEffect},
};
#undef R4A_OURS

// The group's own regions beyond field mode's standard ones.
const sh::Region kRegions[] = {
    {0x904700, 0x490},              // the community 0x9046B0.. past the save bytes' 0x904700, to the battle bytes' end 0x904B90
    {0x93B960, 0x940},              // the eight enemy records
    {0x9035C0, 0x80},               // Field_Slots
    {0x80F580, 0x4000},             // Gfx_ClutStrip
    {at::kEvents, 0x200},           // the event queue and Text_Records' first rows
    {at::kSpawned, 0x20},           // the spawned list's first bytes
    {0x9039A8, 0x5C},               // the removed list, the tail kind and sub-kind, to 0x903A04
    {at::kEventCount, 8},           // the event count
    {at::kResidentCounts, 0x1C},    // the resident counts and the offer words (.data)
    {at::kTriggerObject, 4},        // the trigger's object
    {at::kShopRecords, 0x480},      // Shop_Records' first 50 (.data)
    {at::kPrevArea, 4},             // the area the field came from
};

// --- the seed ------------------------------------------------------------------

unsigned char* Rec(U i) { return Mem(at::kRecords + 8 * i); }
unsigned char* Bld(U b) { return Mem(at::kBuildings + 8 * b); }

// The community, as the simulation leaves it: up to 24 records in use (the
// objects stay inside Sprite_Objects), each +1 a building 1..8 or a loose kind
// 0 / 9 / 0xA / 0xB, +2 small, +3 with the high nibble 1 often; the buildings
// of the kinds the ticks and the poses read, levels 0..2; stamps near the
// clock; the counts small.
void SeedCommunity() {
    const U clock = sh::Next() % 3 == 0 ? sh::Next() : sh::Next() % 0x400;
    SetUL(Mem(at::kClock), clock);
    const auto nearClock = [clock]() { return clock - PickOf(0, 1, 4, 5, 9, 10, 11, 19, 20, 21, 39, 40, 60, 120, sh::Next() % 0x80, sh::Next()); };
    for (U s = at::kStampGrow; s <= at::kStampLevelDark; s += 4) SetUL(Mem(s), nearClock());
    unsigned used = 0;
    for (U i = 0; i < at::kRecordCount; ++i) {
        unsigned char* const r = Rec(i);
        const bool in = used < 24 && sh::Next() % 3 == 0;
        r[0] = in ? static_cast<unsigned char>(PickOf(1, 1, 1, sh::Next() | 1)) : 0;
        if (in) ++used;
        r[1] = static_cast<unsigned char>(PickOf(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xA, 0xB, 1 + sh::Next() % 8, 1 + sh::Next() % 8));
        r[2] = static_cast<unsigned char>(PickOf(0, 1, 2, 3, sh::Next() % 0x40, sh::Next() % 0x100));
        r[3] = static_cast<unsigned char>(sh::Half() ? 0x10 | (sh::Next() % 4) : sh::Next());
        SetUL(r + 4, nearClock());
    }
    for (U b = 0; b < at::kBuildingCount; ++b) {
        unsigned char* const bl = Bld(b);
        bl[0] = static_cast<unsigned char>(PickOf(4, 4, 5, 5, 9, 9, 0xB, 0xB, 0xD, 0xD, 0, 6, 7, 8, 0xA, 0xC, sh::Next() % 0x10));
        bl[1] = static_cast<unsigned char>(sh::Next() % 3);
        bl[2] = static_cast<unsigned char>(PickOf(0, 1, 2, 3, sh::Next()));
        bl[3] = static_cast<unsigned char>(PickOf(0, 1, 4, 5, 6, 0xA, 0xB, 0xC, sh::Next() % 0x20));
        SetUL(bl + 4, nearClock());
    }
    // at least two in use (CommuSim_RemoveRecord walks forever without one; its one call can clear one)
    for (U i = 0; i < 2; ++i) Rec(i)[0] = 1;
    // no kind-0xB building with six residents (the original divides by zero there)
    for (U b = 0; b < at::kBuildingCount; ++b) {
        unsigned n = 0;
        for (U i = 0; i < at::kRecordCount; ++i)
            if (Rec(i)[0] != 0 && Rec(i)[1] == b + 1 && ++n == 6) {
                Rec(i)[1] = 0;
                --n;
            }
    }
    Mem(at::kMood)[0] = static_cast<unsigned char>(PickOf(0, 0, 1, 10, 0x30, 0x63, 0x64, 0x80, 0xFF, sh::Next()));
    Mem(at::kLevelLit)[0] = static_cast<unsigned char>(PickOf(0, 1, 6, 7, sh::Next() % 8));
    Mem(at::kLevelDark)[0] = static_cast<unsigned char>(PickOf(0, 1, 9, 0xA, sh::Next() % 0xB));
    Mem(at::kEventCount)[0] = static_cast<unsigned char>(sh::Next() % 0x20);
    Mem(at::kRemovedCount)[0] = static_cast<unsigned char>(sh::Next() % 0x10);
    Mem(at::kSpawnCount)[0] = static_cast<unsigned char>(sh::Next() % 0x10);
    const unsigned short area = static_cast<unsigned short>(PickOf(0xAF, 0xB0, 0xB2, 0xB5, 0xB6, 0xB7, 0xB9, 0xBA, 0x10, sh::Next() % 0x100));
    Game_AreaNumber = area;
    SetWord(Mem(at::kPrevArea), sh::Half() ? area : PickOf(0xAF, 0xB0, sh::Next() % 0x100));
    SetWord(Mem(at::kAreaSeen), area - PickOf(0, 1, 2, 5, 0x10, 0x40, 0xFFFF, sh::Next() % 0x80));
    for (unsigned k = 0; k < 8; ++k) Mem(at::kResidentCounts)[k] = static_cast<unsigned char>(PickOf(0, 0, 1, 2, 3));
}

// A Field_Slots record for the slot functions: its object one of the first
// four sprite records (+0x4B a pose 0..3, +0x28 a kind 0..4 - kinds 5..7 have
// no frames and fault - +0x24 either way), its table +4 four script pointers
// into scratch 1, its script +8 one of the steps there, an 0xFF step at the
// end leading back.
void SeedSlot(unsigned char* r) {
    unsigned char* const obj = sh::SpriteRecord(sh::Next());
    SetUL(r + 0xC, Key(obj));
    obj[0x4B] = static_cast<unsigned char>(sh::Next() % 4);
    obj[0x28] = static_cast<unsigned char>(PickOf(0, 1, 2, 3, 4));
    unsigned char* const table = sh::Scratch(0);
    unsigned char* const script = sh::Scratch(1);
    for (unsigned j = 0; j < 4; ++j) SetUL(table + 4 * j, Key(script + 4 * (sh::Next() % 12)));
    SetUL(r + 4, Key(table));
    for (unsigned j = 0; j < 12; ++j) {
        script[4 * j] = static_cast<unsigned char>(sh::Next() % 0xFF);   // never the 0xFF that leads back
        script[4 * j + 2] = static_cast<unsigned char>(PickOf(0, 1, 2, 8, 0x10, sh::Next() % 0x40));
        script[4 * j + 3] = static_cast<unsigned char>(PickOf(0, 1, 2, 3, sh::Next() % 0x20));
    }
    script[4 * 12] = 0xFF;
    script[4 * 12 + 1] = static_cast<unsigned char>(1 + sh::Next() % 12);
    if (sh::Half()) {
        script[4 * 13] = 0xFF;
        script[4 * 13 + 1] = 1;
    }
    SetUL(r + 8, Key(script + 4 * (sh::Next() % 12)));
    r[1] = static_cast<unsigned char>(PickOf(0, 0, 1, 2, sh::Next()));
    r[2] = static_cast<unsigned char>(sh::Half() ? obj[0x4B] : sh::Next() % 4);
    // the frame: below 15, so the row (at most 14, + 0x10) and the columns past it stay in the strip
    r[3] = static_cast<unsigned char>(PickOf(0, 1, 2, 3, 4, 7, 8, 0xE, sh::Next() % 15));
}

void SeedBattle() {
    SetUL(Mem(at::kBattleFlags), sh::Next() | (sh::Often() ? 0x4000u : 0));
    Mem(at::kForcedActor)[0] = static_cast<unsigned char>(PickOf(0, 1, 2, 3, 4, 10, sh::Next() % 11));
    Mem(at::kAutoMode)[0] = static_cast<unsigned char>(PickOf(4, 4, 3, 5, sh::Next()));
    Mem(at::kPartyPick)[0] = static_cast<unsigned char>(PickOf(0, 1, 1, 2, sh::Next()));
    for (U e = 0; e < 8; ++e) {
        unsigned char* const rec = Mem(0x93B960 + at::kEnemyStride * e);
        SetWord(rec + 0xBA, PickOf(0, 1, 2, sh::Next()));
        rec[0x92] = static_cast<unsigned char>(sh::Half() ? 0 : sh::Next());
        SetUL(rec + 0x114, sh::Half() ? 0 : sh::Next());
        SetWord(rec + 0x106, sh::Next() % 0x100);
    }
    for (U m = 0; m < 3; ++m) {
        SetWord(Mem(at::kMemberAction + at::kMemberStride * m), PickOf(0xE, 0x128, 0xF, 0x127, 0, sh::Next()));
        Mem(at::kMemberOdds + at::kMemberStride * m)[0] = static_cast<unsigned char>(PickOf(0, 1, 5, 6, 0x14, 0x80, 0xFF, sh::Next()));
    }
}

// Chosen in Seed, handed on by Args (an argument's memory is seeded before
// the state is captured; Args writes none): the slot of the slot functions,
// the record CommuSim_PlaceResident places.
U g_slot, g_resident;

void Seed(unsigned k) {
    if (k <= kOtherMember || k == kLiveEnemy) SeedBattle();
    g_slot = sh::Next() % 8;
    if (k == kRunSlot || k == kClutCopy) SeedSlot(Field_Slots + 16 * g_slot);
    if (k >= kAreaEnter && k <= kTrig11) SeedCommunity();
    g_resident = sh::Next() % at::kRecordCount;
    // the resident's building one of the eight (the counts are the group's region; past them the original writes .data)
    if (k == kPlaceResident) Rec(g_resident)[1] = static_cast<unsigned char>(1 + sh::Next() % 8);
    if (k >= kTrig01) {
        // the object's +5 a record, +0x1C a shop below 50
        for (unsigned j = 0; j < 4; ++j) {
            unsigned char* const o = sh::SpriteRecord(j);
            o[5] = static_cast<unsigned char>(sh::Next() % at::kRecordCount);
            SetUL(o + 0x1C, sh::Next() % 50);
        }
    }
    if (k >= kPlaceLoose && k <= kPoseD) {
        // Sprite_Current one of the first four records
        Sprite_Current = sh::SpriteRecord(sh::Next());
    }
}

void Args(unsigned k, U* a) {
    const auto byte = [a](unsigned i, U v) { a[i] = (a[i] & 0xFFFFFF00u) | (v & 0xFF); };
    g_standing = 0x100;
    switch (k) {
    case kAutoCheck: byte(0, PickOf(0, 1, 2, 3, 4, 7, 10, Mem(at::kForcedActor)[0], sh::Next() % 11)); break;
    case kLiveMember: g_standing = 2; break;
    case kActionIs0E: byte(0, sh::Next() % 3); break;
    case kAutoFixed: {
        const U m = sh::Next() % 3;
        byte(0, m);
        break;
    }
    case kOtherMember: {
        const U m = PickOf(0, 1, 2, 3, sh::Next() % 3);
        byte(0, m);
        g_standing = m == 2 ? 1 : 2;
        break;
    }
    case kRunSlot: byte(0, g_slot); break;
    case kClutCopy: a[0] = Key(Field_Slots + 16 * g_slot); break;
    case kPopulation:
    case kMood: byte(0, PickOf(0, 1, 2, 3, 5, 0x13, 0x14, 0x15, sh::Next() % 0x19)); break;
    case kPlaceLoose: byte(0, sh::Next() % at::kRecordCount); byte(1, sh::Next() % 30); break;
    case kPlaceResident: byte(0, g_resident); byte(1, sh::Next() % 30); break;
    case kPose0:
    case kPose7:
    case kPoseD: byte(0, sh::Next() % 30); break;
    case kPose9:
    case kPose8: byte(0, sh::Next() % at::kRecordCount); byte(1, sh::Next() % 30); byte(2, sh::Next() % 8); break;
    case kPose4:
    case kPose5:
    case kPose6:
    case kPoseA:
    case kPoseB:
    case kPoseC: byte(0, sh::Next() % 30); byte(1, sh::Next() % 8); break;
    default:
        if (k >= kTrig01 && k <= kTrig11) a[1] = at::kStoryFlags;
        break;
    }
}

// What the functions read again after a call, moved by the group's case of the
// harness's disturbance (from its hash only; the case by sh::DisturbCase).
void Disturb(U h) {
    const U v = h >> 3;
    switch (sh::DisturbCase(h, 10)) {
    case 0: Field_Slots[16 * (v % 8) + 1] = static_cast<unsigned char>(v >> 3); break;   // Field_RunSlot's +1 after its copy
    case 1: SetUL(Mem(at::kClock), UL(Mem(at::kClock)) + (v % 64)); break;                // the clock
    case 2: Game_AreaNumber = static_cast<unsigned short>((v & 1) ? 0xAF + (v >> 1) % 12 : v >> 1); break;
    case 3:   // a record's +2 / +3; half the time the +3 of the first in use with a clear high nibble (TickKindD's current)
        if (v & 0x100) {
            for (U i = 0; i < at::kRecordCount; ++i)
                if (Rec(i)[0] != 0 && (Rec(i)[3] & 0xF0) == 0) {
                    Rec(i)[3] = static_cast<unsigned char>(v >> 9);
                    break;
                }
        } else {
            Rec((v >> 2) % at::kRecordCount)[2 + (v & 1)] = static_cast<unsigned char>(v >> 8);
        }
        break;
    case 4: Bld((v >> 2) % 8)[1] = static_cast<unsigned char>((v >> 5) % 3); break;      // a building's level (0..2: kind 9's odds)
    case 5: Mem((v & 1) ? at::kEventCount : at::kRemovedCount)[0] = static_cast<unsigned char>((v >> 1) % 0x20); break;
    case 6:
        Mem(at::kForcedActor)[0] = static_cast<unsigned char>((v >> 1) % 11);
        Mem(at::kPartyPick)[0] = static_cast<unsigned char>(v & 1);
        break;
    case 7: Mem(at::kResidentCounts + (v % 8))[0] = static_cast<unsigned char>((v >> 3) % 4); break;
    case 8: SetWord(Mem(at::kOfferWords + 2 * (v % 9)), 0x9Bu + (v >> 4) % 26); break;
    case 9: Rec(v % at::kRecordCount)[0] = 0; break;   // a record out of use (never in: the objects stay inside Sprite_Objects)
    default: break;
    }
}

}  // namespace

void SelfTest() {
    // BOF3X_R4A_ONLY: the clones whose name contains it (a control's run)
    static sh::Clone chosen[kCount];
    static unsigned index[kCount];
    const char* const only = std::getenv("BOF3X_R4A_ONLY");
    unsigned n = 0;
    for (unsigned k = 0; k < kCount; ++k)
        if (!only || !*only || std::strstr(kAll47[k].name, only)) {
            index[n] = k;
            chosen[n++] = kAll47[k];
        }
    static unsigned* s_index = index;
    if (n != 0) {
        sh::Group g = {"rest_4a", chosen, n, kCallees, sizeof kCallees / sizeof kCallees[0], nullptr, 0, kRegions,
                       sizeof kRegions / sizeof kRegions[0], [](unsigned k) { Seed(s_index[k]); }, &Disturb, 4000};
        g.args = [](unsigned k, U* a) { Args(s_index[k], a); };
        g.field = true;
        sh::Run(g);
    }
    if (!only || !*only || std::strstr(kEnemy[0].name, only)) {
        sh::Group g = {"rest_4a enemy", kEnemy, 1, kEnemyCallees, sizeof kEnemyCallees / sizeof kEnemyCallees[0], nullptr, 0,
                       kRegions, sizeof kRegions / sizeof kRegions[0], [](unsigned) { Seed(kLiveEnemy); }, &Disturb, 6000};
        g.field = true;
        sh::Run(g);
        n = 1;
    }
    if (n == 0) bof3::Fatal("rest_4a: BOF3X_R4A_ONLY=%s names no clone", only);
}

}  // namespace rest_4a

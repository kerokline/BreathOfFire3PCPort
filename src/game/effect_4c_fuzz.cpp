// BOF3X_SHADOW=effect_4c: group E4C's 50 functions through the scenario harness
// in effect mode (scenario_harness.h, docs/scenario_harness.md section 8), once
// at start-up. docs/effect_4c.md section 4. BOF3X_E4C_ONLY=<name> runs the
// clones whose name contains it (the controls' speed-up).
//
// The clone table is tools/band_rows.py --group E4C --clones --harness scenario
// (2026-10-03), each extent read again to its last instruction (capstone) and
// the names given, with one start added: kind 0x8E's dispatcher 0x48B300
// (Effect_KindHandlers[0x8E]), which no list holds. Shapes: every dispatcher
// and state kEffect (Sprite_Current one of the 20 Effect_Objects records, +5
// the kind, a dispatcher's byte below its table's length); the helpers with
// arguments kCall; the four movers answering al and the finder answering a
// pointer with their ret_mask.
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_4c.h"
#include "game/effect_4c_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace effect_4c {
namespace {

namespace sh = scenario_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using U = std::uint32_t;

// tools/band_rows.py --group E4C --clones, 2026-10-03 (0x48B300 added by hand).
constexpr sh::CallSite kCalls48B280[] = {{0x0, 0x48CA90}};
constexpr sh::CallSite kCalls48B290[] = {{0x59, 0x48CA90}};
constexpr sh::CallSite kCalls48B2F0[] = {{0x6, 0x589840}};
constexpr sh::CallSite kCalls48B320[] = {{0x4, 0x48B640},  {0x9, 0x48B850},  {0x49, 0x48B660}, {0x5C, 0x48B870}, {0x97, 0x48B660},
                                         {0xAA, 0x48B870}, {0xEB, 0x48B660}, {0x101, 0x48B870}, {0x10B, 0x587740}};
constexpr sh::CallSite kCalls48B450[] = {{0x17, 0x572650}, {0x28, 0x494060}, {0x2D, 0x48B4A0}, {0x34, 0x48B730}};
constexpr sh::CallSite kCalls48B4A0[] = {{0x5B, 0x5B93D2}, {0x73, 0x48B530}};
constexpr sh::CallSite kCalls48B530[] = {{0x11, 0x5A79A0}, {0x29, 0x5A77C0}, {0x41, 0x572FA0}, {0x4D, 0x5A7570},
                                         {0x55, 0x5A7780}, {0x5F, 0x494110}, {0xF4, 0x572FA0}};
constexpr sh::CallSite kCalls48B660[] = {{0x3D, 0x5B93D2}, {0x59, 0x5B93D2}, {0x7E, 0x5B93D2}, {0x9C, 0x5B93D2}, {0xAC, 0x5B93D2}};
constexpr sh::CallSite kCalls48B730[] = {{0x6D, 0x48B7C0}};
constexpr sh::CallSite kCalls48B7C0[] = {{0x10, 0x5A79A0}, {0x28, 0x5A77C0}, {0x40, 0x572FA0}, {0x4C, 0x5A7750},
                                         {0x54, 0x5A7780}, {0x5E, 0x494110}, {0x7E, 0x572FA0}};
constexpr sh::CallSite kCalls48B870[] = {{0x3D, 0x5B93D2}, {0x59, 0x5B93D2}, {0x7E, 0x5B93D2}, {0x9C, 0x5B93D2}};
constexpr sh::CallSite kCalls48B950[] = {{0x0, 0x48BC40}};
constexpr sh::CallSite kCalls48B960[] = {{0x2, 0x48BB40}, {0x9, 0x48BB40}, {0x10, 0x48BB40}};
constexpr sh::CallSite kCalls48B990[] = {{0x0, 0x48BC60}};
constexpr sh::CallSite kCalls48B9E0[] = {{0x19, 0x48BBB0}, {0x21, 0x48BC60}};
constexpr sh::CallSite kCalls48BA30[] = {{0x0, 0x48BC60}};
constexpr sh::CallSite kCalls48BA70[] = {{0x6C, 0x48BBB0}, {0xAE, 0x48BC60}};
constexpr sh::CallSite kCalls48BB30[] = {{0x0, 0x48BC60}, {0x9, 0x589840}};
constexpr sh::CallSite kCalls48BB40[] = {{0x0, 0x4841C0}};
constexpr sh::CallSite kCalls48BBB0[] = {{0x19, 0x5B93D2}, {0x24, 0x5B93D2}, {0x70, 0x48BB40}};
constexpr sh::CallSite kCalls48BC60[] = {{0x4, 0x494060}, {0x81, 0x5720C0}, {0x90, 0x48BD10}};
constexpr sh::CallSite kCalls48BD10[] = {{0xD, 0x5A75D0},   {0x15, 0x5A7780},  {0x27, 0x494110}, {0x45, 0x4941E0},
                                         {0x198, 0x5A79E0}, {0x1AF, 0x5A79A0}, {0x1DE, 0x572FA0}};
constexpr sh::CallSite kCalls48BF20[] = {{0x9, 0x48C240}};
constexpr sh::CallSite kCalls48BF60[] = {{0xB, 0x48C1C0}, {0x2C, 0x48C260}};
constexpr sh::CallSite kCalls48BFA0[] = {{0x0, 0x48C260}, {0x9, 0x589840}};
constexpr sh::CallSite kCalls48BFD0[] = {{0x0, 0x48C240}, {0x60, 0x587740}};
constexpr sh::CallSite kCalls48C040[] = {{0x34, 0x48C550}};
constexpr sh::CallSite kCalls48C0A0[] = {{0x19, 0x589840}, {0x30, 0x48C550}};
constexpr sh::CallSite kCalls48C100[] = {{0x28, 0x5720C0}};
constexpr sh::CallSite kCalls48C160[] = {{0xE, 0x48C7A0}, {0x42, 0x587740}};
constexpr sh::CallSite kCalls48C1C0[] = {{0x0, 0x48C220}};
constexpr sh::CallSite kCalls48C260[] = {{0x5, 0x494060}, {0xC1, 0x5720C0}, {0xD0, 0x48C370}};
constexpr sh::JumpTable kTables48C260[] = {{0x41, 0xF4, 4}};
constexpr sh::CallSite kCalls48C370[] = {{0xD, 0x5A75D0},   {0x15, 0x5A7780},  {0x27, 0x494110}, {0x45, 0x4941E0},
                                         {0x198, 0x5A79E0}, {0x1AF, 0x5A79A0}, {0x1C8, 0x461E50}};
constexpr sh::CallSite kCalls48C550[] = {{0x7, 0x494060},   {0x38, 0x5A7A50},  {0x47, 0x5A7A00},  {0x82, 0x494110},
                                         {0xA9, 0x494110},  {0xCE, 0x5A75D0},  {0xD6, 0x5A7780},  {0x11A, 0x5A7A50},
                                         {0x12D, 0x5A7A00}, {0x166, 0x494110}, {0x18C, 0x494110}, {0x1F5, 0x5A79A0},
                                         {0x205, 0x5A79E0}, {0x21C, 0x572FA0}};
constexpr sh::CallSite kCalls48C7A0[] = {{0x4, 0x494060}, {0x2E, 0x48C7F0}};
constexpr sh::CallSite kCalls48C7F0[] = {{0x11, 0x494110},  {0x21, 0x5A7A50},  {0x39, 0x5A7A00}, {0x5E, 0x494110},
                                         {0x7E, 0x5A79A0},  {0x97, 0x5A77C0},  {0xB3, 0x572FA0}, {0xBF, 0x5A7570},
                                         {0xC7, 0x5A7780},  {0x10D, 0x5A7A50}, {0x127, 0x5A7A00}, {0x147, 0x494110},
                                         {0x176, 0x572FA0}};

#define E4C_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define E4C_CALLS(a) a, E4C_N(a)
#define E4C_FN(name) reinterpret_cast<const void*>(&::name)
constexpr sh::Shape kEf = sh::Shape::kEffect;
constexpr sh::Shape kCa = sh::Shape::kCall;
constexpr U kAl = 0xFFu, kEax = 0xFFFFFFFFu;
// InitChips / InitDots: the two corners in the harness's scratch (argument 2
// and 3's slots), seeded below.
constexpr U kCorners = sh::ArgAt(2, sh::Arg::kScratch) | sh::ArgAt(3, sh::Arg::kScratch);
// {name, base, size, calls, imms, tables, ours, ret_mask, calm, shape, pointers, state_span, sub_span, kind}
const sh::Clone kAll[] = {
    {"EffectKind8D_Run", 0x48B200, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E4C_FN(EffectKind8D_Run), 0, false, kEf, 0, 4, 0, 0x8D},
    {"EffectKind8D_Start", 0x48B220, 0x52, nullptr, 0, nullptr, 0, nullptr, 0, E4C_FN(EffectKind8D_Start), 0, false, kEf, 0, 0, 0, 0x8D},
    {"EffectKind8D_Hold", 0x48B280, 0x5, E4C_CALLS(kCalls48B280), nullptr, 0, nullptr, 0, E4C_FN(EffectKind8D_Hold), 0, false, kEf, 0, 0, 0, 0x8D},
    {"EffectKind8D_Pull", 0x48B290, 0x5E, E4C_CALLS(kCalls48B290), nullptr, 0, nullptr, 0, E4C_FN(EffectKind8D_Pull), 0, false, kEf, 0, 0, 0, 0x8D},
    {"EffectKind8D_End", 0x48B2F0, 0xB, E4C_CALLS(kCalls48B2F0), nullptr, 0, nullptr, 0, E4C_FN(EffectKind8D_End), 0, false, kEf, 0, 0, 0, 0x8D},
    {"EffectKind8E_Run", 0x48B300, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E4C_FN(EffectKind8E_Run), 0, false, kEf, 0, 3, 0, 0x8E},
    {"EffectKind8E_Start", 0x48B320, 0x129, E4C_CALLS(kCalls48B320), nullptr, 0, nullptr, 0, E4C_FN(EffectKind8E_Start), 0, false, kEf, 0, 0, 0, 0x8E},
    {"EffectKind8E_Fall", 0x48B450, 0x4B, E4C_CALLS(kCalls48B450), nullptr, 0, nullptr, 0, E4C_FN(EffectKind8E_Fall), 0, false, kEf, 0, 0, 0, 0x8E},
    {"EffectKind8E_MoveChips", 0x48B4A0, 0x88, E4C_CALLS(kCalls48B4A0), nullptr, 0, nullptr, 0, E4C_FN(EffectKind8E_MoveChips), kAl, false, kEf, 0, 0, 0, 0x8E},
    {"EffectKind8E_DrawChip", 0x48B530, 0x101, E4C_CALLS(kCalls48B530), nullptr, 0, nullptr, 0, E4C_FN(EffectKind8E_DrawChip), 0, false, kCa, 0, 0, 0, 0x8E},
    {"EffectKind8E_ClearChips", 0x48B640, 0x14, nullptr, 0, nullptr, 0, nullptr, 0, E4C_FN(EffectKind8E_ClearChips), 0, false, kEf, 0, 0, 0, 0x8E},
    {"EffectKind8E_InitChips", 0x48B660, 0xCD, E4C_CALLS(kCalls48B660), nullptr, 0, nullptr, 0, E4C_FN(EffectKind8E_InitChips), 0, false, kCa, kCorners, 0, 0, 0x8E},
    {"EffectKind8E_MoveDots", 0x48B730, 0x81, E4C_CALLS(kCalls48B730), nullptr, 0, nullptr, 0, E4C_FN(EffectKind8E_MoveDots), kAl, false, kEf, 0, 0, 0, 0x8E},
    {"EffectKind8E_DrawDot", 0x48B7C0, 0x8A, E4C_CALLS(kCalls48B7C0), nullptr, 0, nullptr, 0, E4C_FN(EffectKind8E_DrawDot), 0, false, kCa, 0, 0, 0, 0x8E},
    {"EffectKind8E_ClearDots", 0x48B850, 0x14, nullptr, 0, nullptr, 0, nullptr, 0, E4C_FN(EffectKind8E_ClearDots), 0, false, kEf, 0, 0, 0, 0x8E},
    {"EffectKind8E_InitDots", 0x48B870, 0xBF, E4C_CALLS(kCalls48B870), nullptr, 0, nullptr, 0, E4C_FN(EffectKind8E_InitDots), 0, false, kCa, kCorners, 0, 0, 0x8E},
    {"EffectKind8F_Run", 0x48B930, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E4C_FN(EffectKind8F_Run), 0, false, kEf, 0, 9, 0, 0x8F},
    {"EffectKind8F_Start", 0x48B950, 0xE, E4C_CALLS(kCalls48B950), nullptr, 0, nullptr, 0, E4C_FN(EffectKind8F_Start), 0, false, kEf, 0, 0, 0, 0x8F},
    {"EffectKind8F_Burst", 0x48B960, 0x2A, E4C_CALLS(kCalls48B960), nullptr, 0, nullptr, 0, E4C_FN(EffectKind8F_Burst), 0, false, kEf, 0, 0, 0, 0x8F},
    {"EffectKind8F_Rise", 0x48B990, 0x22, E4C_CALLS(kCalls48B990), nullptr, 0, nullptr, 0, E4C_FN(EffectKind8F_Rise), 0, false, kEf, 0, 0, 0, 0x8F},
    {"EffectKind8F_Arm", 0x48B9C0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E4C_FN(EffectKind8F_Arm), 0, false, kEf, 0, 0, 0, 0x8F},
    {"EffectKind8F_Trickle", 0x48B9E0, 0x43, E4C_CALLS(kCalls48B9E0), nullptr, 0, nullptr, 0, E4C_FN(EffectKind8F_Trickle), 0, false, kEf, 0, 0, 0, 0x8F},
    {"EffectKind8F_Settle", 0x48BA30, 0x12, E4C_CALLS(kCalls48BA30), nullptr, 0, nullptr, 0, E4C_FN(EffectKind8F_Settle), 0, false, kEf, 0, 0, 0, 0x8F},
    {"EffectKind8F_ResetClock", 0x48BA50, 0x1F, nullptr, 0, nullptr, 0, nullptr, 0, E4C_FN(EffectKind8F_ResetClock), 0, false, kEf, 0, 0, 0, 0x8F},
    {"EffectKind8F_Sequence", 0x48BA70, 0xB3, E4C_CALLS(kCalls48BA70), nullptr, 0, nullptr, 0, E4C_FN(EffectKind8F_Sequence), 0, false, kEf, 0, 0, 0, 0x8F},
    {"EffectKind8F_Fade", 0x48BB30, 0xF, E4C_CALLS(kCalls48BB30), nullptr, 0, nullptr, 0, E4C_FN(EffectKind8F_Fade), 0, false, kEf, 0, 0, 0, 0x8F},
    {"EffectKind8F_SpawnShard", 0x48BB40, 0x68, E4C_CALLS(kCalls48BB40), nullptr, 0, nullptr, 0, E4C_FN(EffectKind8F_SpawnShard), 0, false, kCa, 0, 0, 0, 0x8F},
    {"EffectKind8F_SpawnShuffled", 0x48BBB0, 0x83, E4C_CALLS(kCalls48BBB0), nullptr, 0, nullptr, 0, E4C_FN(EffectKind8F_SpawnShuffled), 0, false, kCa, 0, 0, 0, 0x8F},
    {"EffectKind8F_ClearShards", 0x48BC40, 0x14, nullptr, 0, nullptr, 0, nullptr, 0, E4C_FN(EffectKind8F_ClearShards), 0, false, kEf, 0, 0, 0, 0x8F},
    {"EffectKind8F_MoveShards", 0x48BC60, 0xA9, E4C_CALLS(kCalls48BC60), nullptr, 0, nullptr, 0, E4C_FN(EffectKind8F_MoveShards), kAl, false, kEf, 0, 0, 0, 0x8F},
    {"EffectKind8F_DrawShard", 0x48BD10, 0x1ED, E4C_CALLS(kCalls48BD10), nullptr, 0, nullptr, 0, E4C_FN(EffectKind8F_DrawShard), 0, false, kCa, 0, 0, 0, 0x8F},
    {"EffectKind90_Run", 0x48BF00, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E4C_FN(EffectKind90_Run), 0, false, kEf, 0, 3, 0, 0x90},
    {"EffectKind90_Start", 0x48BF20, 0x34, E4C_CALLS(kCalls48BF20), nullptr, 0, nullptr, 0, E4C_FN(EffectKind90_Start), 0, false, kEf, 0, 0, 0, 0x90},
    {"EffectKind90_Emit", 0x48BF60, 0x31, E4C_CALLS(kCalls48BF60), nullptr, 0, nullptr, 0, E4C_FN(EffectKind90_Emit), 0, false, kEf, 0, 0, 0, 0x90},
    {"EffectKind90_Fade", 0x48BFA0, 0xF, E4C_CALLS(kCalls48BFA0), nullptr, 0, nullptr, 0, E4C_FN(EffectKind90_Fade), 0, false, kEf, 0, 0, 0, 0x90},
    {"EffectKind93_Run", 0x48BFB0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E4C_FN(EffectKind93_Run), 0, false, kEf, 0, 4, 0, 0x93},
    {"EffectKind93_Start", 0x48BFD0, 0x67, E4C_CALLS(kCalls48BFD0), nullptr, 0, nullptr, 0, E4C_FN(EffectKind93_Start), 0, false, kEf, 0, 0, 0, 0x93},
    {"EffectKind93_Rise", 0x48C040, 0x5F, E4C_CALLS(kCalls48C040), nullptr, 0, nullptr, 0, E4C_FN(EffectKind93_Rise), 0, false, kEf, 0, 0, 0, 0x93},
    {"EffectKind93_Fade", 0x48C0A0, 0x35, E4C_CALLS(kCalls48C0A0), nullptr, 0, nullptr, 0, E4C_FN(EffectKind93_Fade), 0, false, kEf, 0, 0, 0, 0x93},
    {"EffectKind99_Run", 0x48C0E0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, E4C_FN(EffectKind99_Run), 0, false, kEf, 0, 4, 0, 0x99},
    {"EffectKind99_Start", 0x48C100, 0x54, E4C_CALLS(kCalls48C100), nullptr, 0, nullptr, 0, E4C_FN(EffectKind99_Start), 0, false, kEf, 0, 0, 0, 0x99},
    {"EffectKind99_Spread", 0x48C160, 0x53, E4C_CALLS(kCalls48C160), nullptr, 0, nullptr, 0, E4C_FN(EffectKind99_Spread), 0, false, kEf, 0, 0, 0, 0x99},
    {"EffectKind90_EmitOne", 0x48C1C0, 0x51, E4C_CALLS(kCalls48C1C0), nullptr, 0, nullptr, 0, E4C_FN(EffectKind90_EmitOne), 0, false, kEf, 0, 0, 0, 0x90},
    {"EffectKind90_FindShard", 0x48C220, 0x19, nullptr, 0, nullptr, 0, nullptr, 0, E4C_FN(EffectKind90_FindShard), kEax, false, kCa, 0, 0, 0, 0x90},
    {"EffectKind90_ClearShards", 0x48C240, 0x14, nullptr, 0, nullptr, 0, nullptr, 0, E4C_FN(EffectKind90_ClearShards), 0, false, kEf, 0, 0, 0, 0x90},
    {"EffectKind90_MoveShards", 0x48C260, 0x104, E4C_CALLS(kCalls48C260), nullptr, 0, E4C_CALLS(kTables48C260), E4C_FN(EffectKind90_MoveShards), kAl, false, kEf, 0, 0, 0, 0x90},
    {"EffectKind90_DrawShard", 0x48C370, 0x1D7, E4C_CALLS(kCalls48C370), nullptr, 0, nullptr, 0, E4C_FN(EffectKind90_DrawShard), 0, false, kCa, 0, 0, 0, 0x90},
    {"EffectKind93_Draw", 0x48C550, 0x242, E4C_CALLS(kCalls48C550), nullptr, 0, nullptr, 0, E4C_FN(EffectKind93_Draw), 0, false, kEf, 0, 0, 0, 0x93},
    {"EffectKind99_DrawDisc", 0x48C7A0, 0x45, E4C_CALLS(kCalls48C7A0), nullptr, 0, nullptr, 0, E4C_FN(EffectKind99_DrawDisc), 0, false, kCa, 0, 0, 0, 0x99},
    {"EffectKind99_DrawFan", 0x48C7F0, 0x195, E4C_CALLS(kCalls48C7F0), nullptr, 0, nullptr, 0, E4C_FN(EffectKind99_DrawFan), 0, false, kCa, 0, 0, 0, 0x99},
};
#undef E4C_FN
#undef E4C_CALLS
#undef E4C_N

enum : unsigned {
    k8DRun, k8DStart, k8DHold, k8DPull, k8DEnd,
    k8ERun, k8EStart, k8EFall, k8EMoveChips, k8EDrawChip, k8EClearChips, k8EInitChips, k8EMoveDots, k8EDrawDot,
    k8EClearDots, k8EInitDots,
    k8FRun, k8FStart, k8FBurst, k8FRise, k8FArm, k8FTrickle, k8FSettle, k8FResetClock, k8FSequence, k8FFade,
    k8FSpawnShard, k8FSpawnShuffled, k8FClearShards, k8FMoveShards, k8FDrawShard,
    k90Run, k90Start, k90Emit, k90Fade,
    k93Run, k93Start, k93Rise, k93Fade,
    k99Run, k99Start, k99Spread,
    k90EmitOne, k90FindShard, k90ClearShards, k90MoveShards, k90DrawShard,
    k93Draw, k99DrawDisc, k99DrawFan, kCount
};
static_assert(kCount == sizeof kAll / sizeof kAll[0], "one enum entry a clone, in order");

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
unsigned char* P(U a) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a)); }
unsigned char* Mem(U a) { return sh::Mem(a); }
template <typename... T> U PickOf(T... v) {
    const U values[] = {static_cast<U>(v)...};
    return sh::Pick(values, sizeof...(v));
}
U Shards() { return Key(EffectKind30_Shards); }

// --- the effects ---------------------------------------------------------------------

// The first record of a pool whose +0 is 0, or null - as the real finders; null
// also a quarter of the time (the answer's draw).
U FirstFree(U base, U stride, unsigned n, U answer) {
    if (answer % 4 == 0) return 0;
    for (unsigned i = 0; i < n; ++i)
        if (P(base + i * stride)[0] == 0) return base + i * stride;
    return 0;
}
// E3C's EffectKind6E_FindShard: sixteen of 0x28 at EffectKind30_Shards.
U FxFind16(const U*, U answer) { return FirstFree(Shards(), at::kShardStride, at::kShard8FCount, answer); }
// EffectKind90_FindShard: thirty-two of 0x28.
U FxFind32(const U*, U answer) { return FirstFree(Shards(), at::kShardStride, at::kShard90Count, answer); }
// E4D's tile reads Sprite_Current's +0x5D..+0x5F: its address and the dword
// +0x5C logged.
U FxTile(const U*, U answer) {
    unsigned char* const s = Sprite_Current;
    sh::Note(Key(s), static_cast<U>(Long(s + 0x5C)));
    return answer;
}

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
// infinity); one time in eight a NaN quiet or signalling or a value past
// 2^63 either way (E3C's FillFloat): the chip draw copies the depth through
// the FPU, which quiets a signalling NaN, and the others copy it by mov.
void FillFloat(U at) {
    if (!Writable(at, 4)) return;
    const U n = sh::Noise();
    U bits;
    if (n % 8 == 0) {
        static const U kOdd[] = {0x7FC00000u, 0xFFC00000u, 0x7FC00001u, 0x7F800001u, 0xFF800002u, 0x5F000000u, 0xDF000001u, 0x7F7FFFFFu};
        bits = kOdd[(n >> 3) % 8];
    } else {
        bits = (n & 0x807FFFFFu) | ((0x6Eu + (sh::Noise() % 0x24u)) << 23);
    }
    std::memcpy(P(at), &bits, 4);
}
// EffectGte_ProjectPoint: out[0..2] the screen x, y and depth, which every
// caller reads (into the packet, or a local it copies from); the point hashed.
U FxProjectPoint(const U* a, U answer) {
    for (unsigned i = 0; i < 3; ++i) FillFloat(a[1] + 4 * i);
    return answer;
}

#define E4C_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
constexpr sh::Answer kG = sh::Answer::kGarbage, kF = sh::Answer::kFlag, kPh = sh::Answer::kPhase;
constexpr U kW = 0xFFFFFFFFu, k16 = 0xFFFFu, k8 = 0xFFu;
const sh::Callee kCallees[] = {
    // the group's own called directly, by name
    {E4C_OURS(EffectKind8E_ClearChips), 0, {}, kPh, 0, 0},
    {E4C_OURS(EffectKind8E_ClearDots), 0, {}, kPh, 0, 0},
    // first and last read as their low words (`and ecx, 0xFFFF`, `cmp dx,
    // ax`); the two corners are the caller's locals: their 12 bytes hashed,
    // the pointers not compared
    {E4C_OURS(EffectKind8E_InitChips), 4, {k16, k16, 0, 0}, kG, 0, 0, {0, 0, 12, 12}, nullptr, nullptr, true},
    {E4C_OURS(EffectKind8E_InitDots), 4, {k16, k16, 0, 0}, kG, 0, 0, {0, 0, 12, 12}, nullptr, nullptr, true},
    {E4C_OURS(EffectKind8E_MoveChips), 0, {}, kF, 0, 0},
    {E4C_OURS(EffectKind8E_MoveDots), 0, {}, kF, 0, 0},
    {E4C_OURS(EffectKind8E_DrawChip), 1, {kW}, kG, 0, 0, {0x28}, nullptr, nullptr, true},
    {E4C_OURS(EffectKind8E_DrawDot), 1, {kW}, kG, 0, 0, {0x24}, nullptr, nullptr, true},
    {E4C_OURS(EffectKind8F_ClearShards), 0, {}, kPh, 0, 0},
    // the row a byte (`and ecx, 0xFF`): SpawnShuffled pushes a whole edx over
    // a byte of its local order
    {E4C_OURS(EffectKind8F_SpawnShard), 1, {k8}, kG, 0, 0},
    // the count a byte (`mov eax, [esp + 0x24]; test al, al`): Sequence pushes
    // a whole edx over a byte of 0x6550A4
    {E4C_OURS(EffectKind8F_SpawnShuffled), 1, {k8}, kG, 0, 0},
    {E4C_OURS(EffectKind8F_MoveShards), 0, {}, kF, 0, 0},
    {E4C_OURS(EffectKind8F_DrawShard), 1, {kW}, kG, 0, 0, {0x28}, nullptr, nullptr, true},
    {E4C_OURS(EffectKind90_ClearShards), 0, {}, kPh, 0, 0},
    {E4C_OURS(EffectKind90_EmitOne), 0, {}, kPh, 0, 0},
    {E4C_OURS(EffectKind90_FindShard), 0, {}, kG, 0, 0, {}, &FxFind32},
    {E4C_OURS(EffectKind90_MoveShards), 0, {}, kF, 0, 0},
    {E4C_OURS(EffectKind90_DrawShard), 1, {kW}, kG, 0, 0, {0x28}, nullptr, nullptr, true},
    {E4C_OURS(EffectKind93_Draw), 0, {}, kPh, 0, 0},
    // the point is the record's +0xC (hashed), the radius whole
    {E4C_OURS(EffectKind99_DrawDisc), 2, {0, kW}, kG, 0, 0, {12, 0}, nullptr, nullptr, true},
    // the angle read as its low word (`and esi, 0xFFFF`): DrawDisc's push of
    // edx has garbage above it
    {E4C_OURS(EffectKind99_DrawFan), 5, {0, kW, k16, kW, kW}, kG, 0, 0, {12}, nullptr, nullptr, true},
    // a group of an earlier wave's, ours by name, its stand-in the real one's answer
    {E4C_OURS(EffectKind6E_FindShard), 0, {}, kG, 0, 0, {}, &FxFind16},
    // another group's of this round, by address until it merges
    {"0x48CA90 (E4D)", at::kScreenTile, at::kScreenTile, 0, {}, kG, 0, 0, {}, &FxTile},
    // re-listed: the standard row's fractional floats never reach a NaN, which
    // the chip draw quiets in its FPU copy of the depth
    {E4C_OURS(EffectGte_ProjectPoint), 2, {0, 0}, kG, 0, 0, {12, 0}, &FxProjectPoint, nullptr, true},
};
#undef E4C_OURS

// The state tables the dispatchers jump through, read in place; each table's
// own length (symbols.toml [[data]], none bounded by a compare).
const sh::DataTable kTables[] = {
    {0x654FA4, 4}, {0x654FB4, 3}, {0x655080, 9}, {0x6550B4, 3}, {0x6550C0, 4}, {0x6550D0, 4},
};
const std::uint8_t kKinds[] = {0x8D, 0x8E, 0x8F, 0x90, 0x93, 0x99};

// Beyond effect mode's standard regions: kind 0x8F's stage byte and kind
// 0x90's tag (0x676290..0x676297), and kind 0x8E's two pools past the
// standard 0x644 bytes at EffectKind30_Shards (to 0x92E580).
const sh::Region kRegions[] = {
    {at::kStage, 8},
    {sh::at::kShards + sh::at::kShardsSize, at::kShardEnd - (sh::at::kShards + sh::at::kShardsSize)},
};

// --- the seed ------------------------------------------------------------------------

unsigned char* Rec(unsigned r) { return sh::EffectRecord(r); }

// A pool's records: +0 free a third of the time (some rounds all used), the
// life +2 at its boundaries.
void Pool(U base, U stride, unsigned n) {
    const bool full = sh::Next() % 5 == 0;
    for (unsigned i = 0; i < n; ++i) {
        unsigned char* const r = P(base + i * stride);
        r[0] = static_cast<unsigned char>(!full && sh::Next() % 3 == 0 ? 0 : PickOf(1, 1, 0x80, sh::Next() | 1));
        r[2] = static_cast<unsigned char>(PickOf(1, 1, 2, 0, 0x20, sh::Next()));
    }
}
// A particle's height near the floor 0xFE000000 and its speed round it.
void Floor(unsigned char* y, unsigned char* vy) {
    if (sh::Half()) return;
    SetLong(y, static_cast<std::int32_t>(0xFE000000u + PickOf(0, 1, 0x3FFFF, 0x40000, 0x40001, 0xFFFFFFFFu, sh::Next() % 0x100000u)));
    SetLong(vy, static_cast<std::int32_t>(PickOf(0, 0xFFFFFFFFu, 0xFFFC0000u, 0x40000, sh::Next())));
}
// The corners of a box (a scratch slot each): lo random, hi lo plus a span
// that is never 0 (the original's idiv faults there), either way.
void Corners() {
    unsigned char* const lo = sh::Scratch(2);
    unsigned char* const hi = sh::Scratch(3);
    for (unsigned i = 0; i < 3; ++i) {
        const U a = PickOf(0x20000, 0x50000, 0xA0000, 0xFF000000u, 0x770000, sh::Next());
        U span = PickOf(0x30000, 0x50000, 0x3000000, 1, 0xFFFFFFFFu, 0x100, sh::Next());
        if (span == 0) span = 1;
        SetLong(lo + 4 * i, static_cast<std::int32_t>(a));
        SetLong(hi + 4 * i, static_cast<std::int32_t>(a + span));
    }
}

void Seed(unsigned k) {
    unsigned char* const s = Sprite_Current;
    // every record the disturbance may move Sprite_Current to: +9 at its
    // boundaries, kind 0x90's tag +6 small (the shards match it)
    for (unsigned r = 0; r < 20; ++r) {
        unsigned char* const e = Rec(r);
        e[9] = static_cast<unsigned char>(PickOf(1, 2, 0, 3, 4, 5, 0xF, 0x10, 0x1E, 0x2D, 0x3C, 0x96, 0xFF, sh::Next()));
        if ((k >= k90Run && k <= k90Fade) || (k >= k90EmitOne && k <= k90DrawShard)) e[6] = static_cast<unsigned char>(PickOf(0, 1, 2, 3, sh::Next()));
    }
    switch (k) {
    case k8DPull:
        s[0x5D] = static_cast<unsigned char>(PickOf(0, 1, 0x7F, sh::Next()));
        break;
    case k8EFall:
        s[2] = static_cast<unsigned char>(PickOf(0, 0, 1, sh::Next()));
        Pool(Shards(), at::kChipStride, at::kChipCount);
        Pool(at::kDots, at::kDotStride, at::kDotCount);
        break;
    case k8EMoveChips:
        Pool(Shards(), at::kChipStride, at::kChipCount);
        for (unsigned i = 0; i < at::kChipCount; ++i) {
            unsigned char* const r = P(Shards() + i * at::kChipStride);
            Floor(r + 0x20, r + 0x10);
        }
        break;
    case k8EMoveDots:
        Pool(at::kDots, at::kDotStride, at::kDotCount);
        for (unsigned i = 0; i < at::kDotCount; ++i) {
            unsigned char* const r = P(at::kDots + i * at::kDotStride);
            Floor(r + 0x1C, r + 0xC);
        }
        break;
    case k8EDrawChip:
        for (unsigned i = 0; i < at::kChipCount; ++i)
            P(Shards() + i * at::kChipStride)[4] = static_cast<unsigned char>(sh::Next() % at::kChipShapeCount);
        break;
    case k8EInitChips:
    case k8EInitDots:
        Corners();
        break;
    case k8FSpawnShard:
    case k8FSpawnShuffled:
    case k8FBurst:
    case k8FMoveShards:
    case k8FRise:
    case k8FTrickle:
    case k8FSettle:
    case k8FFade:
        Pool(Shards(), at::kShardStride, at::kShard8FCount);
        for (unsigned i = 0; i < at::kShard8FCount; ++i) {
            unsigned char* const r = P(Shards() + i * at::kShardStride);
            r[1] = static_cast<unsigned char>(PickOf(0, 1, 0, 1, 2, sh::Next()));
            r[2] = static_cast<unsigned char>(PickOf(0xF, 0x1F, 0x10, 0x20, 0, sh::Next()));
        }
        break;
    case k8FSequence:
        SetWord(s + 0x2E, PickOf(0, 0x3B, 0x3C, 0xB3, 0xB4, 0x12B, 0x12C, 0x167, 0x168, 0x1A3, 0x1A4, 0xFFFF, 0x8000, 0x7FFF, sh::Next()));
        SetWord(s + 0x30, PickOf(0, 0, 1, 0xFFFF, sh::Next()));
        break;
    case k90Start:
        SetLong(Mem(at::kTag), static_cast<std::int32_t>(PickOf(0, 0, 1, 0xFF, 0xFFFFFFFFu, sh::Next())));
        break;
    case k90Emit:
    case k90Fade:
    case k90EmitOne:
    case k90FindShard:
    case k90MoveShards:
        for (unsigned i = 0; i < at::kShard90Count; ++i) {
            unsigned char* const r = P(Shards() + i * at::kShardStride);
            r[0] = static_cast<unsigned char>(PickOf(0, s[6], s[6], 1, 2, sh::Next()));
            r[1] = static_cast<unsigned char>(PickOf(0, 1, 2, 3, 4, 0xFF, sh::Next()));
            r[2] = static_cast<unsigned char>(PickOf(7, 8, 0xF, 0x10, 0x17, 0x18, 0x1F, 0x20, sh::Next()));
        }
        break;
    case k93Rise: {
        // the top at and around its cap, the foot + 0x8000000
        const U cap = static_cast<U>(Long(s + 0x14)) + 0x8000000u;
        SetLong(s + 0x1C, static_cast<std::int32_t>(cap - PickOf(0x1000000, 0x1000001, 0xFFFFFF, 0, 0x2000000, sh::Next())));
        break;
    }
    case k93Draw: {
        // the column a few steps tall on every record (Sprite_Current read
        // again for its bounds), the width and the foot round each other; the
        // height kept off the signed wrap, where the step count runs to 4,096
        // (E3C's seed of the twin)
        const U y0 = sh::Next() % 0x40000000u - 0x20000000u;
        for (unsigned r = 0; r < 20; ++r) {
            unsigned char* const e = Rec(r);
            const U foot = y0 + (sh::Next() % 5) * 0x80000u - 0x100000u;
            SetLong(e + 0x14, static_cast<std::int32_t>(foot));
            SetLong(e + 0x1C, static_cast<std::int32_t>(y0 + (sh::Next() % 0x14) * 0x100000u - 0x1000000u - PickOf(0, 1, 0xFFFFF)));
            SetLong(e + 0x24, static_cast<std::int32_t>(PickOf(0x1000000, 0x100000, 0, 0xF00000, sh::Next() % 0x2000000)));
        }
        SetLong(s + 0x14, static_cast<std::int32_t>(y0));
        break;
    }
    default: break;
    }
}

// The helpers with arguments: a record of the pool each draws (the callers
// hand them so), the box's slice over leftovers above the words, the rows and
// counts over leftovers above the byte, the disc's point the record's +0xC.
void Args(unsigned k, U* a) {
    switch (k) {
    case k8EDrawChip: a[0] = Shards() + at::kChipStride * (sh::Next() % at::kChipCount); break;
    case k8EDrawDot: a[0] = at::kDots + at::kDotStride * (sh::Next() % at::kDotCount); break;
    case k8EInitChips:
    case k8EInitDots: {
        const U first = PickOf(0, 0x20, 0x60, 0x7F, 0x80, sh::Next() % 0x81);
        const U last = PickOf(first, first + 1, 0x80, 0x20, 0x60, sh::Next() % 0x81);
        a[0] = (sh::Next() & 0xFFFF0000u) | first;
        a[1] = (sh::Next() & 0xFFFF0000u) | (last > 0x80 ? 0x80 : last);
        break;
    }
    case k8FSpawnShard: a[0] = (sh::Next() & 0xFFFFFF00u) | (sh::Next() % at::kShardRowCount); break;
    case k8FSpawnShuffled: a[0] = (sh::Next() & 0xFFFFFF00u) | PickOf(0, 1, 2, 4, 15, 16, sh::Next() % 17); break;
    case k8FDrawShard:
    case k90DrawShard: a[0] = Shards() + at::kShardStride * (sh::Next() % at::kShard90Count); break;
    case k99DrawDisc:
        a[0] = Key(Sprite_Current + 0xC);
        a[1] = PickOf(0, 0x1199, 0xAFFA, 0x7FFFFFFF, sh::Next());
        break;
    case k99DrawFan:
        a[0] = Key(Sprite_Current + 0xC);
        a[1] = PickOf(0, 0x1199, 0xAFFA, 0x7FFFFFFF, sh::Next());
        a[2] = (sh::Next() & 0xFFFF0000u) | PickOf(0, 0x400, 0xFF00, 0xFFFF, sh::Next());
        break;
    default: break;
    }
}

// What the states read again after a call, moved by the group's case of the
// harness's disturbance (from its hash only): the frame count +9, the tint
// +0x5D, the stage byte (inside its five rows), kind 0x90's tag +6 and its
// counter, kind 0x8F's two clock words, kind 0x93's top near its foot, kind
// 0x99's centre.
void Disturb(U h) {
    const U v = h >> 8;
    unsigned char* const s = Sprite_Current;
    if (!sh::InRegions(s, 0x80)) return;
    switch (h % 8) {
    case 0: s[9] = static_cast<unsigned char>((v & 1) ? 1u : v >> 1); break;
    case 1: s[0x5D + v % 3] = static_cast<unsigned char>(v >> 2); break;
    case 2: Mem(at::kStage)[0] = static_cast<unsigned char>(v % at::kStageCount); break;
    case 3: s[6] = static_cast<unsigned char>(v % 4); break;
    case 4: SetLong(Mem(at::kTag), static_cast<std::int32_t>(v % 3)); break;
    case 5: SetWord(s + 0x2E + 2 * (v & 1), (v >> 1) & 0x1FF); break;
    case 6: SetLong(s + 0x1C, static_cast<std::int32_t>(static_cast<U>(Long(s + 0x14)) + ((v >> 1) % 0x14) * 0x100000u)); break;
    case 7: SetLong(s + 0xC + 4 * (v & 1), static_cast<std::int32_t>(v >> 1)); break;
    default: break;
    }
}

}  // namespace

void SelfTest() {
    // BOF3X_E4C_ONLY: the clones whose name contains it (a control's run)
    static sh::Clone chosen[kCount];
    static unsigned index[kCount];
    const char* const only = std::getenv("BOF3X_E4C_ONLY");
    unsigned n = 0;
    for (unsigned k = 0; k < kCount; ++k)
        if (!only || !*only || std::strstr(kAll[k].name, only)) {
            index[n] = k;
            chosen[n++] = kAll[k];
        }
    if (n == 0) bof3::Fatal("effect_4c: BOF3X_E4C_ONLY=%s names no clone", only);
    static unsigned* s_index = index;
    sh::Group g = {"effect_4c", chosen, n, kCallees, sizeof kCallees / sizeof kCallees[0], kTables,
                   sizeof kTables / sizeof kTables[0], kRegions, sizeof kRegions / sizeof kRegions[0],
                   [](unsigned k) { Seed(s_index[k]); }, &Disturb, 4000};
    g.args = [](unsigned k, U* a) { Args(s_index[k], a); };
    g.effect = true;
    g.kinds = kKinds;
    g.n_kinds = sizeof kKinds;
    sh::Run(g);
}

}  // namespace effect_4c

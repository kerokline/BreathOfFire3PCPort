// BOF3X_SHADOW=magic_fx_reached: a differential fuzz of the magic effects the
// combat route casts through pointers, once at start-up.
// docs/magic_fx_reached.md section 5.
//
// Twenty-five byte-copies, every call and tail jmp out re-aimed at a
// recording stand-in (bof3::CloneCall with the callee each site was read to
// reach), the five stack tables' immediates checked and re-aimed at
// recorders in the copies, and the three .data tables the dispatches read in
// place (FxRing_Phases, StealClone_Types, FxDim_Phases) swapped for recorders
// and put back. One round: one function, random bytes in every region any of
// them touches (the task slots, the owner, Sprite_Current / Frame_Counter /
// Gfx_ClutStripDirty, the battle state bytes, the message-window byte, the
// party and enemy records, the steal and sparkle tables, the sparkle pool,
// the tint records, the CLUT row and its source, two sprite records of the
// fuzz's own), the pointers put back inside them, then that function's
// boundaries seeded; theirs, then from the same state ours; the regions and
// the stand-ins' log compared.
//
// The stand-ins are louder than the real callees: any call may move
// Sprite_Current, the owner, the source sprite, the current sparkle, the
// target and actor bytes, the message-window byte, the effect flags,
// Frame_Counter, or a field of the task, the owner, the sparkle or the target
// enemy - so a value ours keeps where the original reads memory again (or
// the other way round) shows. The ones that answer a byte answer garbage
// above it.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_fx_reached_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace magic_fx_reached {
namespace {

using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

std::uint32_t Address(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }

// --- the random source and the stand-ins' log ------------------------------

std::uint32_t g_rng = 0x3C6EF372u;
std::uint32_t Next() { g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5; return g_rng; }
bool Often() { return Next() % 3 != 0; }
bool Half() { return (Next() & 1) != 0; }

constexpr unsigned kLog = 1024;   // Sparkle_Spawn: up to 255 x (alloc, Rand), Sparkle_Task up to 129
struct Entry { std::uint32_t what, a, b, c, d; };
Entry g_log[kLog];
unsigned g_log_n;
std::uint32_t g_seed;        // the stand-ins' own stream: the same on both passes
std::uint32_t g_rand_hint;   // a value the seeding wants Rand & 0xFF to land around

std::uint32_t Hash() {
    std::uint32_t h = (g_seed + g_log_n * 0x2545F491u) * 0x9E3779B1u;
    h ^= h >> 15;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    return h;
}
void Record(std::uint32_t what, std::uint32_t a = 0, std::uint32_t b = 0, std::uint32_t c = 0, std::uint32_t d = 0) {
    if (g_log_n < kLog) g_log[g_log_n] = {what, a, b, c, d};
    ++g_log_n;
}

// --- the fuzz's own memory -------------------------------------------------

constexpr unsigned kRecordBytes = 0x140;
alignas(16) unsigned char g_records[2][kRecordBytes];

unsigned char* TaskAt(unsigned k) { return At(at::kTasks + (k % 4) * at::kTaskStride); }
unsigned char* SparkleAt(unsigned k) { return At(at::kSparklePool + (k % 4) * at::kSparkleStride); }
void SetPointer(std::uint32_t cell, const void* p) { SetLong(At(cell), static_cast<std::int32_t>(Address(p))); }
unsigned char* Pointer(std::uint32_t cell) {
    return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(At(cell)))));
}
unsigned char* OwnerFor(unsigned v) { return v & 4 ? g_records[v & 1] : TaskAt(v); }
unsigned char* TargetEnemy() {
    return At(at::kEnemies + static_cast<std::uint32_t>((static_cast<int>(At(at::kTarget)[0]) - 3) * 0x128));
}

// Every cell below is one some function reads again after a call.
void Disturb() {
    const std::uint32_t h = Hash();
    if (h % 3 == 0) return;
    const unsigned v = (h >> 12) & 0xFF;
    const auto b = static_cast<unsigned char>(h >> 20);
    switch ((h >> 4) % 16) {
    case 0: Sprite_Current = TaskAt(v); break;
    case 1: SetPointer(at::kOwner, OwnerFor(v)); break;
    case 2: SetPointer(at::kSource, g_records[v & 1]); break;
    case 3: SetPointer(at::kSparkleCurrent, SparkleAt(v)); break;
    case 4: At(at::kTarget)[0] = static_cast<unsigned char>(v % 11); break;
    case 5: At(at::kActor)[0] = static_cast<unsigned char>(v % 5); break;
    case 6: At(at::kMessageUp)[0] = b; break;
    case 7: At(at::kFlags)[0] = b; break;
    case 8: Frame_Counter = h >> 6; break;
    case 9: case 10: {
        static const unsigned kFields[] = {0, 1, 2, 4, 9, 0xA, 0xB, 0x2C, 0x2D, 0x30, 0x34, 8};
        Sprite_Current[kFields[v % 12]] = b;
        break;
    }
    case 11: {
        static const unsigned kFields[] = {0, 0xB, 8, 0x34, 0x38, 0x3C};
        Pointer(at::kOwner)[kFields[v % 6]] = b;
        break;
    }
    case 12: {
        static const unsigned kFields[] = {0, 2, 6, 0xC};
        Pointer(at::kSparkleCurrent)[kFields[v % 4]] = b;
        break;
    }
    case 13: {
        static const unsigned kFields[] = {at::kStealItem, at::kStealItem + 1, at::kStealRate, at::kEnemySpeed};
        TargetEnemy()[kFields[v % 4]] = b;
        break;
    }
    default: break;
    }
}

// --- the stand-ins ---------------------------------------------------------

std::uint32_t Cur() { return Address(Sprite_Current); }

template <unsigned N> void __cdecl StubHandler() {
    Record(100 + N, Cur(), Sprite_Current[1], Sprite_Current[2]);
    Disturb();
}
template <unsigned N> void __cdecl StubSparklePhase() {
    Record(140 + N, Address(Pointer(at::kSparkleCurrent)), Pointer(at::kSparkleCurrent)[2]);
    Disturb();
}
template <unsigned N> void __cdecl StubVoid() {
    Record(N, Cur());
    Disturb();
}
// BattleTask_Create answers a slot in al (0..47: 0xFF would write past the
// slots, outside the image), garbage above.
unsigned __cdecl StubTaskCreate(unsigned kind, unsigned parameter) {
    Record(20, kind & 0xFF, parameter & 0xFF, Cur());
    Disturb();
    const std::uint32_t h = Hash();
    return (h & 0xFFFFFF00u) | ((h >> 3) % at::kTaskCount);
}
void __cdecl StubTargetFlags(unsigned target, unsigned bits) {
    Record(21, target & 0xFF, bits & 0xFFFF);
    Disturb();
}
void __cdecl StubTargetFlag40(unsigned target) {
    Record(22, target & 0xFF);
    Disturb();
}
void __cdecl StubPlayById(unsigned id) {
    Record(23, id & 0xFFFF);
    Disturb();
}
void __cdecl StubPlayEffect(unsigned id) {
    Record(24, id & 0xFFFF);
    Disturb();
}
void __cdecl StubSetAnimation(unsigned offset, unsigned arg) {
    Record(25, offset & 0xFF, arg, Cur());
    Disturb();
}
// Rand: some values the CRT's never answers (negative); a third of the time
// near the value the seeding asked for.
int __cdecl StubRand() {
    Record(26);
    Disturb();
    const std::uint32_t h = Hash();
    if (h % 3 == 0) return static_cast<int>(((h >> 8) & 0x7F00u) | ((g_rand_hint + (h >> 4) % 3 - 1) & 0xFF));
    return h % 4 == 0 ? static_cast<int>(h) : static_cast<int>(h >> 1) & 0x7FFF;
}
unsigned __cdecl StubInventoryAdd(unsigned category, unsigned item, unsigned count) {
    const std::uint32_t h = Hash();
    const std::uint32_t answer = h % 3 == 0 ? h & 0xFFFFFF00u : h | 0x10;
    Record(27, category, item, count, answer & 0xFF);
    Disturb();
    return answer;
}
void __cdecl StubItemName(unsigned index, unsigned category) {
    Record(28, index & 0xFF, category & 0xFF);
    Disturb();
}
const unsigned char* __cdecl StubMsgSystem(unsigned id) {
    Record(29, id & 0xFFFF);
    Disturb();
    return reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(Hash()));
}
unsigned long __cdecl StubQueuePush(unsigned a, unsigned b, unsigned long value) {
    Record(30, a & 0xFF, b & 0xFF, value);
    Disturb();
    return Hash();
}
unsigned __cdecl StubTickOnce() {
    Record(31, Cur());
    Disturb();
    const std::uint32_t h = Hash();
    return h % 2 ? h & 0xFFFFFF00u : h | 1;
}
void __cdecl StubPlaySound(unsigned first, unsigned second) {
    Record(32, first & 0xFF, second & 0xFF);
    Disturb();
}
void __cdecl StubSparkleDispatch() {
    Record(33, Address(Pointer(at::kSparkleCurrent)), static_cast<std::uint32_t>(Long(At(at::kOwner))));
    Disturb();
}
// Sparkle_Alloc: a record index 0..127 in al, or 0xFF a quarter of the time.
unsigned __cdecl StubSparkleAlloc() {
    Record(34);
    Disturb();
    const std::uint32_t h = Hash();
    return (h & 0xFFFFFF00u) | (h % 4 == 0 ? 0xFFu : (h >> 8) % at::kSparkles);
}
void __cdecl StubReleaseTint(unsigned char* sprite) {
    Record(35, Address(sprite));
    Disturb();
}
void __cdecl StubFlash(unsigned index) {
    Record(36, index & 0xFF);
    Disturb();
}
// The rays' callees read their arguments' low words.
void __cdecl StubRaysG2(unsigned start, unsigned radius) {
    Record(37, start & 0xFFFF, radius & 0xFFFF, Address(Pointer(at::kSparkleCurrent)));
    Disturb();
}
void __cdecl StubRaysG3(unsigned start, unsigned radius) {
    Record(38, start & 0xFFFF, radius & 0xFFFF, Address(Pointer(at::kSparkleCurrent)));
    Disturb();
}
void __cdecl StubTintClut(int level) {
    Record(39, static_cast<std::uint32_t>(level));
    Disturb();
}
// The .data tables' entries.
template <unsigned N> void __cdecl StubDataEntry() {
    Record(160 + N, Cur(), Sprite_Current[1]);
    Disturb();
}

template <typename T, typename F> T As(F* f) { return reinterpret_cast<T>(reinterpret_cast<void*>(f)); }

const Callees kStubs = {
    {&StubHandler<0>, &StubHandler<1>, &StubHandler<2>},
    {&StubHandler<10>, &StubHandler<11>, &StubHandler<12>, &StubHandler<13>},
    {&StubHandler<20>, &StubHandler<21>, &StubHandler<22>, &StubHandler<23>},
    {&StubHandler<30>, &StubHandler<31>, &StubHandler<32>, &StubHandler<33>, &StubHandler<34>, &StubHandler<35>},
    {&StubSparklePhase<0>, &StubSparklePhase<1>, &StubSparklePhase<2>},
    &StubVoid<1>,    // update_screen_xy
    &StubVoid<2>,    // draw_disc
    &StubVoid<3>,    // push_matrix
    &StubVoid<4>,    // draw_fan
    &StubVoid<5>,    // pop_matrix
    &StubVoid<6>,    // draw_ring
    As<unsigned char (__cdecl*)(unsigned, unsigned)>(&StubTaskCreate),
    &StubVoid<7>,    // free_current
    StubTargetFlags,
    StubTargetFlag40,
    As<void (__cdecl*)(unsigned short)>(&StubPlayById),
    As<void (__cdecl*)(unsigned short)>(&StubPlayEffect),
    StubSetAnimation,
    StubRand,
    As<unsigned char (__cdecl*)(unsigned, unsigned, unsigned)>(&StubInventoryAdd),
    StubItemName,
    StubMsgSystem,
    StubQueuePush,
    As<unsigned char (__cdecl*)()>(&StubTickOnce),
    StubPlaySound,
    &StubVoid<8>,    // update_screen
    StubSparkleDispatch,
    As<unsigned char (__cdecl*)()>(&StubSparkleAlloc),
    StubReleaseTint,
    StubFlash,
    &StubVoid<9>,    // sparkle_disc
    StubRaysG2,
    StubRaysG3,
    StubTintClut,
};

const void* StubFor(std::uint32_t target) {
    const auto f = [](auto p) { return reinterpret_cast<const void*>(p); };
    switch (target) {
    case 0x4FBD10: return f(kStubs.update_screen_xy);
    case 0x4C54F0: return f(kStubs.draw_disc);
    case 0x4B7D40: return f(kStubs.push_matrix);
    case 0x4AD6F0: return f(kStubs.draw_fan);
    case 0x5A7BC0: return f(kStubs.pop_matrix);
    case 0x4C5150: return f(kStubs.draw_ring);
    case 0x435180: return f(kStubs.task_create);
    case 0x4351F0: return f(kStubs.free_current);
    case 0x452F70: return f(kStubs.set_target_flags);
    case 0x4530D0: return f(kStubs.set_target_flag40);
    case 0x587900: return f(kStubs.play_by_id);
    case 0x587740: return f(kStubs.play_effect);
    case 0x4FB830: return f(kStubs.set_animation);
    case 0x5B93D2: return f(kStubs.rand);
    case 0x590BB0: return f(kStubs.inventory_add);
    case 0x4B58F0: return f(kStubs.item_name);
    case 0x497740: return f(kStubs.msg_system);
    case 0x44A880: return f(kStubs.queue_push);
    case 0x589410: return f(kStubs.script_tick_once);
    case 0x4FC030: return f(kStubs.play_sound);
    case 0x588F20: return f(kStubs.update_screen);
    case 0x4B8FE0: return f(kStubs.sparkle_dispatch);
    case 0x4B98B0: return f(kStubs.sparkle_alloc);
    case 0x454DC0: return f(kStubs.release_tint);
    case 0x4FBDB0: return f(kStubs.flash);
    case 0x4B9680: return f(kStubs.sparkle_disc);
    case 0x4B9300: return f(kStubs.rays_g2);
    case 0x4B9490: return f(kStubs.rays_g3);
    case 0x573050: return f(kStubs.tint_clut);
    default: bof3::Fatal("magic_fx_reached: no stand-in for a call to 0x%X", static_cast<unsigned>(target)); return nullptr;
    }
}

// --- the twenty-five copies (capstone, 2026-09-25: every jump internal, no
// jump table; the calls and tail jmps below are every transfer that leaves) -

struct Call { std::uint32_t offset, target; };
struct Imm { std::uint32_t offset, value; };
struct Clone {
    const char* name;
    std::uint32_t base, size;
    const Call* calls;
    int n_calls;
    const Imm* imms;   // the stack table's immediates, or null
    const Handler* imm_to;
    int n_imms;
    const void* ours;
};

constexpr Call kFadeCalls[] = {{0x47, 0x4530D0}, {0x56, 0x4351F0}};
constexpr Call kRingFadeCalls[] = {{0x3D, 0x4351F0}};
constexpr Call kStealStartCalls[] = {{0x68, 0x4FB830}, {0x71, 0x435180}, {0x198, 0x5B93D2}, {0x1D8, 0x590BB0}};
constexpr Call kStealWaitCalls[] = {{0x16, 0x4FB830}};
constexpr Call kReportCalls[] = {{0x1E, 0x4B58F0}, {0x31, 0x497740}, {0x3B, 0x44A880}};
constexpr Call kCloneTaskCalls[] = {{0x3D, 0x588F20}};
constexpr Call kRunCalls[] = {{0x0, 0x589410}, {0x22, 0x4FC030}};
constexpr Call kFinishCalls[] = {{0x0, 0x589410}, {0xF, 0x4530D0}};
constexpr Call kSparkleTaskCalls[] = {{0x69, 0x4B8FE0}};
constexpr Call kSpawnCalls[] = {{0x5B, 0x4FBD10}, {0xA5, 0x4B98B0}, {0xDE, 0x5B93D2}, {0x143, 0x587740}};
constexpr Call kEndCalls[] = {{0x68, 0x454DC0}, {0x74, 0x4FBDB0}};
constexpr Call kUpdateCalls[] = {{0x3B, 0x4B9680}, {0x59, 0x4B9300}, {0x83, 0x4B9490}};
constexpr Call kDiscFanCalls[] = {{0x35, 0x4FBD10}, {0x44, 0x4C54F0}, {0x49, 0x4B7D40}, {0x4E, 0x4AD6F0}, {0x53, 0x5A7BC0}};
constexpr Call kStartCalls[] = {{0x47, 0x435180}, {0xBE, 0x452F70}, {0xC8, 0x587900}};
constexpr Call kRingTaskCalls[] = {{0x23, 0x4B7D40}, {0x28, 0x4C5150}, {0x2D, 0x5A7BC0}};
constexpr Call kIdleCalls[] = {{0x10, 0x4351F0}};
constexpr Call kDownCalls[] = {{0x2A, 0x573050}};
constexpr Call kUpCalls[] = {{0x19, 0x4351F0}, {0x29, 0x573050}};

constexpr Imm kDiscFanImm[] = {{0xF, 0x4C5020}, {0x17, 0x4AD130}, {0x22, 0x4AD160}};
constexpr Imm kStealImm[] = {{0xF, 0x4B54F0}, {0x17, 0x4B5770}, {0x22, 0x4B57C0}, {0x2A, 0x4F52D0}};
constexpr Imm kCloneImm[] = {{0xF, 0x4ED5C0}, {0x17, 0x4B5880}, {0x22, 0x4B58C0}, {0x2A, 0x4AEE90}};
constexpr Imm kSparkleTaskImm[] = {{0x15, 0x4B8E00}, {0x1D, 0x4B1E70}, {0x25, 0x4B1ED0}, {0x2D, 0x4EE8A0}, {0x35, 0x4B8F50}, {0x3D, 0x4F7350}};
constexpr Imm kUpdateImm[] = {{0xF, 0x4B9090}, {0x17, 0x4B9200}, {0x22, 0x4B9270}};

#define MF_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define MF_C(name, base, size, calls) {#name, base, size, calls, MF_N(calls), nullptr, nullptr, 0, reinterpret_cast<const void*>(&::name)}
#define MF_P(name, base, size) {#name, base, size, nullptr, 0, nullptr, nullptr, 0, reinterpret_cast<const void*>(&::name)}
#define MF_T(name, base, size, calls, imms, to) \
    {#name, base, size, calls, MF_N(calls), imms, to, MF_N(imms), reinterpret_cast<const void*>(&::name)}
#define MF_TP(name, base, size, imms, to) {#name, base, size, nullptr, 0, imms, to, MF_N(imms), reinterpret_cast<const void*>(&::name)}
const Clone kClones[] = {
    MF_T(FxDiscFan_Task, 0x4C4FC0, 0x5C, kDiscFanCalls, kDiscFanImm, kStubs.disc_fan),
    MF_C(FxDiscFan_Start, 0x4C5020, 0xEC, kStartCalls),
    MF_P(FxDiscFan_Grow, 0x4AD130, 0x29),
    MF_C(FxDiscFan_Fade, 0x4AD160, 0x5C, kFadeCalls),
    MF_C(FxRing_Task, 0x4C5110, 0x33, kRingTaskCalls),
    MF_P(FxRing_Wait, 0x4AD200, 0x6A),
    MF_P(FxRing_Rise, 0x4AD270, 0x38),
    MF_C(FxRing_Fade, 0x4AD2B0, 0x43, kRingFadeCalls),
    MF_TP(Steal_Task, 0x4B54B0, 0x36, kStealImm, kStubs.steal),
    MF_C(Steal_Start, 0x4B54F0, 0x271, kStealStartCalls),
    MF_C(Steal_Wait, 0x4B5770, 0x47, kStealWaitCalls),
    MF_C(Steal_Report, 0x4B57C0, 0x4C, kReportCalls),
    MF_C(MagicFx_EndWhenIdle, 0x4F52D0, 0x16, kIdleCalls),
    MF_P(StealClone_Dispatch, 0x4B5810, 0x12),
    MF_T(StealClone_Task, 0x4B5830, 0x46, kCloneTaskCalls, kCloneImm, kStubs.clone),
    MF_C(StealClone_Run, 0x4B5880, 0x33, kRunCalls),
    MF_C(StealClone_Finish, 0x4B58C0, 0x2D, kFinishCalls),
    MF_T(Sparkle_Task, 0x4B8D70, 0x81, kSparkleTaskCalls, kSparkleTaskImm, kStubs.sparkle_task),
    MF_C(Sparkle_Spawn, 0x4B8E00, 0x14E, kSpawnCalls),
    MF_C(Sparkle_End, 0x4B8F50, 0x85, kEndCalls),
    MF_T(Sparkle_Update, 0x4B9000, 0x8F, kUpdateCalls, kUpdateImm, kStubs.sparkle),
    MF_P(FxDim_Dispatch, 0x4FAFF0, 0x12),
    MF_C(FxDim_Down, 0x4FB010, 0x31, kDownCalls),
    MF_P(FxDim_Hold, 0x4FB050, 0x12),
    MF_C(FxDim_Up, 0x4FB070, 0x30, kUpCalls),
};
#undef MF_C
#undef MF_P
#undef MF_T
#undef MF_TP
#undef MF_N
constexpr unsigned kCount = sizeof kClones / sizeof kClones[0];

enum : unsigned {
    kDiscFanTask, kDiscFanStart, kGrow, kDiscFanFade, kRingTask, kRingWait, kRingRise, kRingFade, kStealTask,
    kStealStart, kStealWait, kReport, kIdle, kCloneDispatch, kCloneTask, kRun, kFinish, kSparkleTask, kSpawn, kEnd,
    kUpdate, kDimDispatch, kDown, kHold, kUp,
};
static_assert(kUp + 1 == kCount, "the seeding's indices");

// The .data tables swapped for recorders while the fuzz runs.
struct DataTable { std::uint32_t at; unsigned entries; const Handler* stubs; };
const Handler kRingStubs[4] = {&StubDataEntry<0>, &StubDataEntry<1>, &StubDataEntry<2>, &StubDataEntry<3>};
const Handler kCloneStubs[2] = {&StubDataEntry<10>, &StubDataEntry<11>};
const Handler kDimStubs[4] = {&StubDataEntry<20>, &StubDataEntry<21>, &StubDataEntry<22>, &StubDataEntry<23>};
const DataTable kDataTables[] = {
    {at::kRingPhases, 4, kRingStubs},
    {at::kCloneTypes, 2, kCloneStubs},
    {at::kDimPhases, 4, kDimStubs},
};

// --- the state both passes start from --------------------------------------

struct Region { std::uint32_t at, size; };
Region g_regions[] = {
    {at::kTasks, at::kTaskCount * at::kTaskStride},             // the task slots
    {0x93B8C0, 0xA0},                                           // 0x93B8C4 the current slot, 0x93B940 the owner, and
                                                                // the fields an enemy index of 2 reads (0x93B8E0..)
    {0x937F88, 0x10},                                           // Sprite_Current, Gfx_ClutStripDirty, Frame_Counter
    {at::kFlags, 0xA8},                                         // 0x904AA8..0x904B50: flags, actor, target, source
    {at::kMessageUp, 4},
    {at::kParty, 5 * at::kPartyStride},                         // actors 0..4
    {at::kEnemies, 8 * at::kEnemyStride},                       // targets 3..10
    {at::kStealRates, 8},
    {at::kSparkleCounts, 0x10},                                 // counts and delays
    {at::kSparklePool, at::kSparkles * at::kSparkleStride + 4}, // the pool and the current pointer
    {at::kTints, 0xC00},
    {at::kClutRow, 0x20},
    {at::kClutSource, 0x20},
    {0, 2 * kRecordBytes},                                      // g_records (filled in at start-up)
};
constexpr unsigned kRegionBytes = at::kTaskCount * at::kTaskStride + 0xA0 + 0x10 + 0xA8 + 4 + 5 * at::kPartyStride +
                                  8 * at::kEnemyStride + 8 + 0x10 + at::kSparkles * at::kSparkleStride + 4 + 0xC00 +
                                  0x20 + 0x20 + 2 * kRecordBytes;

struct State {
    unsigned char memory[kRegionBytes];
    Entry log[kLog];
    unsigned log_n;
};
void Capture(State& s) {
    unsigned n = 0;
    for (const Region& r : g_regions) { std::memcpy(s.memory + n, At(r.at), r.size); n += r.size; }
    std::memcpy(s.log, g_log, sizeof s.log);
    s.log_n = g_log_n;
}
void Apply(const State& s) {
    unsigned n = 0;
    for (const Region& r : g_regions) { std::memcpy(At(r.at), s.memory + n, r.size); n += r.size; }
    std::memset(g_log, 0, sizeof g_log);
    g_log_n = 0;
}

// Random bytes put back inside what the functions dereference.
void Fix() {
    Sprite_Current = TaskAt(Next());
    SetPointer(at::kOwner, OwnerFor(Next()));
    SetPointer(at::kSource, g_records[Next() & 1]);
    SetPointer(at::kSparkleCurrent, SparkleAt(Next()));
    At(at::kTarget)[0] = static_cast<unsigned char>(Next() % 11);
    At(at::kActor)[0] = static_cast<unsigned char>(Next() % 5);
    // Phases inside the tables (the seeding picks its function's own).
    for (unsigned k = 0; k < 4; ++k) {
        unsigned char* const t = TaskAt(k);
        t[1] = static_cast<unsigned char>(Next() % 3);
        t[2] = static_cast<unsigned char>(Next() % 2);
    }
    for (unsigned k = 0; k < 4; ++k) SparkleAt(k)[2] = static_cast<unsigned char>(Next() % 3);
    // Every sparkle's owner +0x28, which Sparkle_Task makes 0x93B940: a task
    // or a record, each its own (the stand-ins write through 0x93B940).
    for (unsigned i = 0; i < at::kSparkles; ++i)
        SetPointer(at::kSparklePool + i * at::kSparkleStride + 0x28, OwnerFor(Next()));
}

std::uint32_t Pick(const std::uint32_t* v, unsigned n) { return v[Next() % n]; }
#define MF_PICK(...) [] { static const std::uint32_t kV[] = {__VA_ARGS__}; return Pick(kV, sizeof kV / sizeof kV[0]); }()

void Seed(unsigned k) {
    unsigned char* const sc = Sprite_Current;
    unsigned char* const cur = Pointer(at::kSparkleCurrent);
    g_rand_hint = Next();
    switch (k) {
    case kDiscFanTask:
        sc[1] = static_cast<unsigned char>(Next() % 3);
        if (Half()) sc[0] = 0;
        break;
    case kGrow:
        if (Often()) sc[9] = static_cast<unsigned char>(MF_PICK(0xF, 0x10, 0xFF, 0x0E));
        break;
    case kDiscFanFade:
        if (Often()) sc[0xB] = static_cast<unsigned char>(MF_PICK(0, 2, 3, 4, 5, 0xFF));
        if (Often()) sc[9] = static_cast<unsigned char>(MF_PICK(0, 1, 2));
        if (Often()) sc[0xA] = static_cast<unsigned char>(MF_PICK(0, 1, 2));
        break;
    case kRingTask:
        sc[1] = static_cast<unsigned char>(Next() % 4);
        if (Half()) sc[0] = 0;
        break;
    case kRingWait: case kRingFade: case kRun: case kEnd:
        if (Often()) sc[9] = static_cast<unsigned char>(MF_PICK(0, 1, 2, 3));
        break;
    case kRingRise:
        if (Often()) sc[9] = static_cast<unsigned char>(MF_PICK(0x10, 0x11, 0x12, 0x0F));
        break;
    case kStealTask:
        sc[1] = static_cast<unsigned char>(Next() % 4);
        break;
    case kStealStart: {
        At(at::kTarget)[0] = static_cast<unsigned char>(Often() ? 3 + Next() % 8 : Next() % 3);
        unsigned char* const e = TargetEnemy();
        if (Often()) e[at::kStealRate] = static_cast<unsigned char>(Half() ? 1 + Next() % 7 : Next() % 8);
        if (Half()) SetWord(e + at::kStealItem, Half() ? 0 : Next() % 0x100);
        const int d = static_cast<int>(MF_PICK(49, 48, 29, 28, 19, 18, 9, 8, static_cast<std::uint32_t>(-10),
                                               static_cast<std::uint32_t>(-11), static_cast<std::uint32_t>(-20),
                                               static_cast<std::uint32_t>(-21), static_cast<std::uint32_t>(-30),
                                               static_cast<std::uint32_t>(-31), static_cast<std::uint32_t>(-50),
                                               static_cast<std::uint32_t>(-51), 0x3FFF, static_cast<std::uint32_t>(-0x4000)));
        const unsigned speed = Next() & 0x3FFF;
        unsigned m = static_cast<unsigned>(MF_PICK(4, 5, 6, 7, 8, 9, 10, 11, 12));
        if (Next() % 4) {
            // The speed difference at a threshold or one below, and Rand aimed
            // at the rate times the multiplier the original takes for it: a
            // threshold moved by one changes the answer at exactly that Rand.
            SetWord(e + at::kEnemySpeed, speed + 0x4000u);
            SetWord(At(at::kParty + At(at::kActor)[0] * at::kPartyStride + at::kThiefSpeed),
                    static_cast<unsigned>(static_cast<int>(speed + 0x4000u) + d));
            m = d >= 49 ? 12 : d >= 29 ? 11 : d >= 19 ? 10 : d >= 9 ? 9 : d >= -10 ? 8 : d >= -20 ? 7 : d >= -30 ? 6 : d >= -50 ? 5 : 4;
        }
        const unsigned rate = e[at::kStealRate] < 8 ? static_cast<unsigned>(static_cast<signed char>(At(at::kStealRates)[e[at::kStealRate]])) : 0;
        g_rand_hint = rate * m;
        break;
    }
    case kStealWait:
        if (Often()) sc[0xB] = static_cast<unsigned char>(MF_PICK(0, 0, 1));
        if (Often()) sc[9] = static_cast<unsigned char>(MF_PICK(0x1E, 0x1F, 1, 0, 2));
        break;
    case kReport:
        At(at::kMessageUp)[0] = static_cast<unsigned char>(Often() ? 0 : Next() | 1);
        if (Often()) sc[0xA] = static_cast<unsigned char>(MF_PICK(0x38, 0x39, 0x3A));
        break;
    case kIdle:
        At(at::kMessageUp)[0] = static_cast<unsigned char>(Half() ? 0 : Next() | 1);
        break;
    case kCloneDispatch:
        sc[1] = static_cast<unsigned char>(Next() % 2);
        break;
    case kCloneTask:
        sc[2] = static_cast<unsigned char>(Next() % 4);
        if (Half()) sc[0] = 0;
        break;
    case kSparkleTask:
        sc[1] = static_cast<unsigned char>(Next() % 6);
        break;
    case kSpawn:
        for (unsigned i = 0; i < 8; ++i)
            if (Often()) At(at::kSparkleCounts)[i] = static_cast<unsigned char>(MF_PICK(0, 1, 2, 5, 0x1E, 0x3C, 0xFF));
        break;
    case kUpdate:
        cur[2] = static_cast<unsigned char>(Next() % 3);
        if (Half()) cur[0] = static_cast<unsigned char>(cur[0] | 1);
        if (Half()) cur[0xC] = static_cast<unsigned char>(cur[0xC] & ~3u);
        break;
    case kDimDispatch:
        sc[1] = static_cast<unsigned char>(Next() % 4);
        break;
    case kDown:
        if (Often()) sc[9] = static_cast<unsigned char>(MF_PICK(0xFB, 0xFA, 0xF9, 0, 1, 0x80, 0x7F));
        break;
    case kHold:
        if (Half()) At(at::kFlags)[0] = static_cast<unsigned char>(At(at::kFlags)[0] ^ 4);
        break;
    case kUp:
        if (Often()) sc[9] = static_cast<unsigned char>(MF_PICK(0xFF, 0xFF, 0xFF, 0xFE, 0x7F, 0x80, 0));
        break;
    default:
        break;
    }
}
#undef MF_PICK

// What the rounds reached, from the original's side, for the log line.
struct Coverage {
    unsigned logged[200];
    unsigned stolen, bag_full, missed;
} g_cover;

void Cover(unsigned k, const State& in, const State& out) {
    for (unsigned i = 0; i < out.log_n && i < kLog; ++i)
        if (out.log[i].what < 200) ++g_cover.logged[out.log[i].what];
    (void)in;
    if (k == kStealStart) {
        // Inventory_Add's stand-in logs its answer: a theft, a full bag, or no
        // add at all (a failed roll or nothing to steal).
        int added = -1;
        for (unsigned i = 0; i < out.log_n && i < kLog; ++i)
            if (out.log[i].what == 27) added = out.log[i].d != 0;
        if (added == 1) ++g_cover.stolen;
        else if (added == 0) ++g_cover.bag_full;
        else ++g_cover.missed;
    }
}

void PatchImms(void* copy, const char* name, const Imm* imms, int n, const Handler* to) {
    auto* code = static_cast<std::uint8_t*>(copy);
    for (int i = 0; i < n; ++i) {
        std::uint32_t had;
        std::memcpy(&had, code + imms[i].offset, sizeof had);
        if (had != imms[i].value)
            bof3::Fatal("magic_fx_reached: %s +0x%X holds 0x%X, not the handler 0x%X", name, static_cast<unsigned>(imms[i].offset),
                        static_cast<unsigned>(had), static_cast<unsigned>(imms[i].value));
        const std::uint32_t target = Address(reinterpret_cast<const void*>(to[i]));
        std::memcpy(code + imms[i].offset, &target, sizeof target);
    }
}

using Fn3 = std::uint32_t (__cdecl*)(std::uint32_t, std::uint32_t, std::uint32_t);

}  // namespace

void SelfTest() {
    constexpr unsigned kPerFunction = 2000;
    g_regions[13].at = Address(g_records);
    unsigned region_bytes = 0;
    for (const Region& r : g_regions) region_bytes += r.size;
    if (region_bytes != kRegionBytes)
        bof3::Fatal("magic_fx_reached: the regions are %u bytes, the state holds %u", region_bytes, kRegionBytes);

    void* clones[kCount];
    for (unsigned k = 0; k < kCount; ++k) {
        const Clone& c = kClones[k];
        bof3::CloneCall calls[8];
        if (c.n_calls > 8) bof3::Fatal("magic_fx_reached: %s has %d calls", c.name, c.n_calls);
        for (int i = 0; i < c.n_calls; ++i) calls[i] = {c.calls[i].offset, StubFor(c.calls[i].target), c.calls[i].target};
        clones[k] = bof3::CloneOriginal(c.name, c.base, c.size, calls, c.n_calls);
        if (c.imms) PatchImms(clones[k], c.name, c.imms, c.n_imms, c.imm_to);
    }

    // The .data tables: their entries swapped for recorders, put back after.
    std::uint32_t kept[3][4];
    for (unsigned t = 0; t < 3; ++t)
        for (unsigned i = 0; i < kDataTables[t].entries; ++i) {
            kept[t][i] = static_cast<std::uint32_t>(Long(At(kDataTables[t].at + 4 * i)));
            SetPointer(kDataTables[t].at + 4 * i, reinterpret_cast<const void*>(kDataTables[t].stubs[i]));
        }

    static State saved, input, their_out, our_out;
    Capture(saved);
    g = kStubs;

    unsigned bad = 0, calls = 0, bad_per[kCount] = {};
    for (unsigned round = 0; round < kPerFunction * kCount; ++round) {
        const unsigned k = round % kCount;
        for (unsigned i = 0; i < kRegionBytes; i += 4) {
            const std::uint32_t v = Next();
            std::memcpy(input.memory + i, &v, kRegionBytes - i < 4 ? kRegionBytes - i : 4);
        }
        std::memset(input.log, 0, sizeof input.log);
        input.log_n = 0;
        Apply(input);
        Fix();
        g_seed = Next();
        Seed(k);
        Capture(input);

        for (int pass = 0; pass < 2; ++pass) {
            Apply(input);
            State& out = pass ? our_out : their_out;
            const void* const fn = pass ? kClones[k].ours : clones[k];
            reinterpret_cast<Fn3>(const_cast<void*>(fn))(Next(), Next(), Next());
            Capture(out);
            if (pass == 0) Cover(k, input, their_out);
        }
        calls += their_out.log_n;
        if (their_out.log_n > kLog)
            bof3::Fatal("magic_fx_reached: %s made %u calls, the log holds %u", kClones[k].name, their_out.log_n, kLog);
        if (std::memcmp(&their_out, &our_out, sizeof their_out) != 0) {
            ++bad_per[k];
            if (++bad <= 12) {
                unsigned first = 0;
                while (first < sizeof their_out &&
                       reinterpret_cast<const unsigned char*>(&their_out)[first] == reinterpret_cast<const unsigned char*>(&our_out)[first])
                    ++first;
                bof3::Log("shadow      magic_fx_reached self-test MISMATCH: round %u, %s, log %u / %u, first differing state byte %u",
                          round, kClones[k].name, their_out.log_n, our_out.log_n, first);
            }
        }
    }
    g = kOriginals;
    Apply(saved);
    for (unsigned t = 0; t < 3; ++t)
        for (unsigned i = 0; i < kDataTables[t].entries; ++i)
            SetLong(At(kDataTables[t].at + 4 * i), static_cast<std::int32_t>(kept[t][i]));

    bof3::Log("shadow      magic_fx_reached self-test: %u rounds over %u functions (%u each), %u calls to the stand-ins, "
              "%u MISMATCHES; the task slots, the battle state, the party and enemy records, the steal and sparkle tables, "
              "the sparkle pool, the tint records, the CLUT row, two sprite records and the stand-ins' log compared",
              kPerFunction * kCount, kCount, kPerFunction, calls, bad);
    if (bad)
        for (unsigned k = 0; k < kCount; ++k)
            if (bad_per[k]) bof3::Log("shadow      magic_fx_reached: %s mismatched in %u rounds", kClones[k].name, bad_per[k]);
    const Coverage& c = g_cover;
    bof3::Log("shadow      magic_fx_reached coverage: disc-fan phases %u / %u / %u, steal %u / %u / %u / %u, double %u / %u / "
              "%u / %u, herb %u / %u / %u / %u / %u / %u, sparkle %u / %u / %u; ring table %u / %u / %u / %u, double types "
              "%u / %u, dim table %u / %u / %u / %u; stolen %u, bag full %u, no add %u; creates %u, frees %u, "
              "allocs %u, dispatches %u, rays %u / %u, tints %u, messages %u",
              c.logged[100], c.logged[101], c.logged[102], c.logged[110], c.logged[111], c.logged[112], c.logged[113],
              c.logged[120], c.logged[121], c.logged[122], c.logged[123], c.logged[130], c.logged[131], c.logged[132],
              c.logged[133], c.logged[134], c.logged[135], c.logged[140], c.logged[141], c.logged[142], c.logged[160],
              c.logged[161], c.logged[162], c.logged[163], c.logged[170], c.logged[171], c.logged[180], c.logged[181],
              c.logged[182], c.logged[183], c.stolen, c.bag_full, c.missed, c.logged[20], c.logged[7],
              c.logged[34], c.logged[33], c.logged[37], c.logged[38], c.logged[39], c.logged[30]);
    if (bad) bof3::Fatal("the magic effects the fight casts differ from the original in %u self-test rounds", bad);
}

}  // namespace magic_fx_reached

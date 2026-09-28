// BOF3X_SHADOW=area_w1b: world 1's areas 42..47 through the area round's
// shared harness (area_harness.h), once at start-up: one area_harness::Run
// per area (Group::area its number, the real descriptor and tables in place,
// the area's own .data state tables swapped through DataTable).
// docs/area_w1b.md section "The fuzz".
//
// The clone rows are tools/area_rows.py's (--unit AREA<nnn> --clones,
// 2026-09-27), each read against the disassembly; area 45's are area 16's
// (area_w0b_fuzz.cpp) at area 45's addresses - the same offsets, a capstone
// compare of the pairs - and its group is area 16's with area 45's tables.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w1b.h"
#include "game/area_w1b_callees.h"
#include "game/move_script_bytes.h"
#include "hook/log.h"

namespace area_w1b {
namespace {

namespace ah = area_harness;
using U = std::uint32_t;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using ah::Mem;
using S = ah::Shape;

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
constexpr U kAll = 0xFFFFFFFFu, kU8 = 0xFFu, kU16 = 0xFFFFu;
#define AH_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define AH_COUNT(a) static_cast<unsigned>(sizeof a / sizeof a[0])
#define W1B_C(name, base, size, calls, shape) \
    {#name, base, size, calls, AH_N(calls), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::name), 0, false, shape}
#define W1B_P(name, base, size, shape) \
    {#name, base, size, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::name), 0, false, shape}
// A clone with a jump table inside it, and one answering in al.
#define W1B_T(name, base, size, calls, tables, shape) \
    {#name, base, size, calls, AH_N(calls), nullptr, 0, tables, AH_N(tables), reinterpret_cast<const void*>(&::name), 0, false, shape}
#define W1B_A(name, base, size, calls, shape) \
    {#name, base, size, calls, AH_N(calls), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::name), 0xFF, false, shape}
#define W1B_AP(name, base, size, shape) \
    {#name, base, size, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::name), 0xFF, false, shape}

// --- shared by the groups -------------------------------------------------------

constexpr U kActiveMember = 0x9035A4;     // Field_ActiveMember
constexpr U kEffectState = 0x66972C;      // MoveScript_EffectState (24)
constexpr U kKind2Hold = 0x929F12;        // Field_Kind2Hold
constexpr U kCameraAngles = 0x929EC8;     // Camera_Angles (3 words)
constexpr U kEffects = 0x7E11E0;          // Effect_Objects: the first eight records

// Effect_FindFree: a slot of the first eight or none (0xFF), garbage above.
const ah::Callee kFindFree = {"Effect_FindFree", bof3::addr::Effect_FindFree, KeyOf(&::Effect_FindFree), 0, {}, ah::Answer::kByte, 0xFF, 0x07};
const ah::Callee kSet40 = {"ScriptFlags_Set40", bof3::addr::ScriptFlags_Set40, KeyOf(&::ScriptFlags_Set40), 0, {}, ah::Answer::kGarbage, 0, 0};
const ah::Callee kClear40 = {"ScriptFlags_Clear40", bof3::addr::ScriptFlags_Clear40, KeyOf(&::ScriptFlags_Clear40), 0, {}, ah::Answer::kGarbage, 0, 0};

// Field_ActiveMember: one of the party records or of the first four field
// objects (every function here dereferences it).
unsigned char* SomeRecord(U h) { return (h & 1) ? ah::PartyOf(static_cast<unsigned char>(h >> 1)) : ah::TaskAt(h >> 1); }
void SeedActiveMember() { ah::SetPointer(kActiveMember, SomeRecord(ah::Next())); }

// A tail state byte: the switch's cases, its neighbours, and anything.
unsigned char TailState(std::initializer_list<unsigned> cases) {
    if (!ah::Often()) return static_cast<unsigned char>(ah::Next());
    const unsigned n = static_cast<unsigned>(cases.size());
    const unsigned pick = ah::Next() % (n + 4);
    if (pick < n) return static_cast<unsigned char>(cases.begin()[pick]);
    return static_cast<unsigned char>(AH_PICK(0xFF, 0x80, 0x7F, 0x15, 0xC, 2, 9));
}

// A 16.16 position for a step hook: a column of `lo..hi` (and one either
// side), any fraction, or anything.
U Column(unsigned lo, unsigned hi) {
    if (!ah::Often()) return ah::Next();
    const unsigned c = lo - 1 + ah::Next() % (hi - lo + 3);
    return (c << 16) | (ah::Half() ? 0x8000 : ah::Next() & 0xFFFF) | (ah::Half() ? 0 : 0x10000000u * (ah::Next() & 0xF));
}

// ===========================================================================
// Area 42
// ===========================================================================

// clones: tools/area_rows.py --unit AREA042 --clones
constexpr ah::CallSite kCalls406670[] = {{0x2B, 0x57C140}, {0x3E, 0x57C140}, {0x51, 0x57C0F0}};
constexpr ah::CallSite kCalls4066F0[] = {{0x14, 0x57C110}};
constexpr ah::CallSite kCalls406710[] = {{0x2B, 0x57C140}, {0x3E, 0x57C0F0}, {0x43, 0x4068D0}, {0x4F, 0x57C140}};
constexpr ah::CallSite kCalls406780[] = {{0x2B, 0x57C140}, {0x3E, 0x57C0F0}, {0x43, 0x4068D0}, {0x4F, 0x57C140}};
constexpr ah::CallSite kCalls4067F0[] = {{0x2B, 0x57C140}, {0x3E, 0x57C0F0}, {0x43, 0x4068D0}, {0x4F, 0x57C140}};
constexpr ah::CallSite kCalls406860[] = {{0x2B, 0x57C140}, {0x3E, 0x57C0F0}, {0x43, 0x4068D0}, {0x4F, 0x57C140}};
constexpr ah::CallSite kCalls4068D0[] = {{0x8, 0x57C140}, {0x1F, 0x57C140}, {0x36, 0x57C140}, {0x4D, 0x57C140}, {0x64, 0x57C140}, {0x74, 0x57C7C0}, {0xFB, 0x57C140}, {0x13C, 0x5891F0}};
constexpr ah::CallSite kCalls406A30[] = {{0x6F, 0x40E750}, {0x8E, 0x5B9380}, {0xAB, 0x516B30}, {0x101, 0x589810}, {0x17B, 0x57C0F0}, {0x18E, 0x589810}, {0x241, 0x57C110}, {0x249, 0x57C7A0}, {0x2AB, 0x40E750}, {0x2B0, 0x5B93D2}, {0x2D4, 0x5B9380}, {0x2FF, 0x516B30}, {0x322, 0x57C110}, {0x32E, 0x57C110}, {0x33A, 0x57C110}, {0x346, 0x57C110}, {0x352, 0x57C110}};
constexpr ah::JumpTable kTables406A30[] = {{0x16, 0x37C, 11}};
constexpr ah::CallSite kCalls406DE0[] = {{0x36, 0x57C110}, {0x42, 0x57C110}, {0x4E, 0x57C110}, {0x5A, 0x57C110}, {0x66, 0x57C110}, {0x72, 0x57C110}};
constexpr ah::CallSite kCalls406E90[] = {{0x11, 0x57C140}, {0x29, 0x57C140}, {0x50, 0x57CD90}, {0x7A, 0x57B530}, {0x8D, 0x589810}};
const ah::Clone kClones42[] = {
    W1B_P(Area42_ChoiceMessage, 0x406650, 0x17, S::kChoice),
    W1B_C(Area42_StartTimer, 0x406670, 0x78, kCalls406670, S::kHandler),
    W1B_C(Area42_ClearMemberFlag, 0x4066F0, 0x1D, kCalls4066F0, S::kHandler),
    W1B_C(Area42_Touch12, 0x406710, 0x68, kCalls406710, S::kHandler),
    W1B_C(Area42_Touch13, 0x406780, 0x68, kCalls406780, S::kHandler),
    W1B_C(Area42_Touch14, 0x4067F0, 0x68, kCalls4067F0, S::kHandler),
    W1B_C(Area42_Touch15, 0x406860, 0x68, kCalls406860, S::kHandler),
    W1B_A(Area42_CheckAll, 0x4068D0, 0x159, kCalls4068D0, S::kCallee),
    W1B_T(Area42_TimerTail, 0x406A30, 0x3A8, kCalls406A30, kTables406A30, S::kTail),
    W1B_A(Area42_StepHook, 0x406DE0, 0xA1, kCalls406DE0, S::kHook),
    W1B_C(Area42_Init, 0x406E90, 0xE3, kCalls406E90, S::kInit),
};
enum : unsigned { k42Choice, k42Start, k42ClearMember, k42Touch12, k42Touch13, k42Touch14, k42Touch15, k42CheckAll, k42Tail, k42Step, k42Init };
static_assert(k42Init + 1 == AH_COUNT(kClones42), "area 42's seeding indices");

// Flags_Test of the bank 0x9040CC moves Area42_Rank (a stand-in louder than
// the callee), so Area42_CheckAll's re-read of the rank after a miss shows.
U RankEffect(const U* a, U answer) {
    if (a[0] == at::kFlagsCC) Mem(at::kRank)[0] = static_cast<unsigned char>(ah::Noise() % 3);
    return answer;
}
const ah::Callee kCallees42[] = {
    {"Flags_Test", bof3::addr::Flags_Test, KeyOf(&::Flags_Test), 2, {kAll, kU8}, ah::Answer::kBool, 0, 0, {}, &RankEffect, nullptr},
    {"Area42_CheckAll", 0x4068D0, 0x4068D0, 0, {}, ah::Answer::kPhase, 0, 0},
    {"window 0x40E750", kDrawWindow, kDrawWindow, 5, {kAll, kAll, kAll, kAll, kAll}, ah::Answer::kGarbage, 0, 0},
    {"free object 0x57CD90", kFreeObject, kFreeObject, 0, {}, ah::Answer::kByte, 0xFF, 0x1D},
    {"Crt_sprintf", 0x5B9380, 0x5B9380, 4, {kAll, kAll, kAll, kAll}, ah::Answer::kGarbage, 0, 0},
    // its 13-byte operand logged
    {"EventOp_9x", bof3::addr::EventOp_9x, KeyOf(&::EventOp_9x), 1, {kAll}, ah::Answer::kGarbage, 0, 0, {13}},
    kFindFree, kSet40, kClear40,
};
const ah::Region kRegions42[] = {
    {kActiveMember, 4}, {kEffectState, 24}, {at::kRank, 1}, {kKind2Hold, 1}, {kCameraAngles, 6}, {kEffects, 8 * 0x80},
};

// The countdown's boundaries: seconds 5 / 6 / 10 (150, 180, 300 frames),
// hundredths (the remainder 0, 1, 29), the low byte of the seconds wrapping
// at 256 (7,680), the start 0x384, 0 and 1.
unsigned TimerWord() {
    if (!ah::Often()) return ah::Next() & 0xFFFF;
    return AH_PICK(0, 1, 2, 29, 30, 149, 150, 179, 180, 181, 299, 300, 301, 0x384, 7679, 7680, 7710, 0xFFFF, 0x8000);
}

void Seed42(unsigned k) {
    SeedActiveMember();
    // the leader's effect-state index inside the table, its entry 1 most of the time
    if (ah::Often()) Mem(at::kLeaderEffect)[0] = static_cast<unsigned char>(ah::Next() % 24);
    if (ah::Often()) Mem(kEffectState + Mem(at::kLeaderEffect)[0] % 24)[0] = static_cast<unsigned char>(ah::Often() ? 1 : AH_PICK(0, 2, 0x81));
    Mem(at::kRank)[0] = static_cast<unsigned char>(ah::Often() ? ah::Next() % 3 : ah::Next());
    SetWord(Mem(at::kTailTimer), TimerWord());
    Mem(at::kTailArg)[0] = static_cast<unsigned char>(ah::Often() ? AH_PICK(0, 1, 2, 0x3C, 0xFF) : ah::Next());
    Mem(at::kTailKind)[0] = static_cast<unsigned char>(ah::Often() ? AH_PICK(7, 7, 6, 8, 0x87) : ah::Next());
    switch (k) {
    case k42Choice:
        Mem(at::kChoice)[0] = static_cast<unsigned char>(ah::Often() ? AH_PICK(0, 1, 2, 3, 4, 5, 0xFF, 0x80, 0x7F) : ah::Next());
        break;
    case k42CheckAll:
        // the field objects that qualify: +0 bit 0, +6 == 9, +5 in 0x5C..0x5E
        for (unsigned i = 0; i < 30; ++i) {
            unsigned char* const o = ah::Object(i);
            if (ah::Half()) continue;
            o[0] = static_cast<unsigned char>(ah::Often() ? o[0] | 1 : o[0] & 0xFE);
            o[6] = static_cast<unsigned char>(ah::Often() ? 9 : AH_PICK(8, 0xA, 0x89));
            o[5] = static_cast<unsigned char>(ah::Often() ? 0x5C + ah::Next() % 3 : AH_PICK(0x5B, 0x5F, 0xDC));
        }
        break;
    case k42Tail:
        Mem(at::kTailState)[0] = TailState({0, 0, 1, 2, 3, 4, 5, 6, 7, 10, 10, 10});
        if (ah::Often()) Field_Kind2Hold = static_cast<unsigned char>(ah::Half() ? 0 : ah::Next() | 1);
        if (ah::Often()) Mem(at::kTailArg)[0] = static_cast<unsigned char>(ah::Next() % 8);   // a slot of the fuzz's records
        if (ah::Often()) {
            unsigned char* const e = Mem(kEffects + (Mem(at::kTailArg)[0] % 8) * 0x80);
            e[0] = static_cast<unsigned char>(ah::Half() ? e[0] & 0xFE : e[0] | 1);
        }
        break;
    case k42Init:
        Cond_ByteFD = static_cast<unsigned char>(ah::Often() ? AH_PICK(0, 0, 1, 1, 2, 0xFF) : ah::Next());
        break;
    default:
        break;
    }
}

// The step hook's (x, z): the two rows it tests (and one either side), the
// columns 0x7B..0x7C and 0x1D..0x1F.
void Args42(unsigned k, U* a) {
    if (k != k42Step) return;
    const bool first = ah::Half();
    a[0] = first ? Column(0x7B, 0x7C) : Column(0x1D, 0x1F);
    a[1] = ah::Often() ? (first ? 0x308000u : 0x368000u) + AH_PICK(0, 0, 0, 1, 0xFFFFFFFFu, 0x10000, 0x8000) : ah::Next();
}

// Area 42's disturbance: what its functions read again after a call - the
// rank (CheckAll's re-read), the tail bytes and the countdown word, the tail
// kind (the step hook reads it after its calls), Cond_ByteFD (the init reads
// it again), Field_ActiveMember (re-read after the calls).
void Disturb42(U h) {
    const unsigned v = (h >> 8) & 0xFF;
    switch ((h >> 16) % 7) {
    case 0: Mem(at::kRank)[0] = static_cast<unsigned char>(v % 3); break;
    case 1: Mem(at::kTailArg)[0] = static_cast<unsigned char>(v); break;
    case 2: SetWord(Mem(at::kTailTimer), (h >> 8) & 0xFFFF); break;
    case 3: Mem(at::kTailKind)[0] = static_cast<unsigned char>(v & 1 ? 7 : v); break;
    case 4: Cond_ByteFD = static_cast<unsigned char>(v % 3); break;
    case 5: ah::SetPointer(kActiveMember, SomeRecord(h >> 24)); break;
    case 6: Mem(at::kTailState)[0] = static_cast<unsigned char>(v % 11); break;
    default: break;
    }
}

// ===========================================================================
// Area 43
// ===========================================================================

// clones: tools/area_rows.py --unit AREA043 --clones
constexpr ah::CallSite kCalls406FA0[] = {{0xE, 0x454CC0}};
constexpr ah::CallSite kCalls407020[] = {{0x79, 0x454D60}};
constexpr ah::CallSite kCalls4070D0[] = {{0x0, 0x589810}};
constexpr ah::CallSite kCalls407140[] = {{0x6, 0x454DC0}, {0x10, 0x589590}, {0x17, 0x5891F0}, {0x4C, 0x454CC0}};
constexpr ah::CallSite kCalls4071E0[] = {{0x7, 0x57C0F0}, {0x10, 0x572650}, {0x1A, 0x587740}};
const ah::Clone kClones43[] = {
    W1B_P(Area43_ObjectRun, 0x406F80, 0x12, S::kHandler),
    W1B_C(Area43_TintStart, 0x406FA0, 0x77, kCalls406FA0, S::kState),
    W1B_C(Area43_TintSettle, 0x407020, 0xA7, kCalls407020, S::kState),
    W1B_C(Area43_SpawnEffect37, 0x4070D0, 0x68, kCalls4070D0, S::kHandler),
    W1B_C(Area43_SetUpObject, 0x407140, 0x92, kCalls407140, S::kHandler),
    W1B_C(Area43_Trigger58, 0x4071E0, 0x23, kCalls4071E0, S::kCallee),
};
enum : unsigned { k43Run, k43TintStart, k43TintSettle, k43Spawn, k43SetUp, k43Trigger };
const ah::Callee kCallees43[] = {
    {"Tint_Release", bof3::addr::Tint_Release, KeyOf(&::Tint_Release), 1, {kU8}, ah::Answer::kGarbage, 0, 0},
    {"MoveCmd_TestFB", bof3::addr::MoveCmd_TestFB, KeyOf(&::MoveCmd_TestFB), 2, {kU16, kU16}, ah::Answer::kFlag, 0, 0},
    kFindFree,
};
const ah::DataTable kTables43[] = {{at::kA43States, 2}};
const ah::Region kRegions43[] = {{kActiveMember, 4}, {kEffects, 8 * 0x80}};
void Seed43(unsigned k) {
    SeedActiveMember();
    unsigned char* const o = Sprite_Current;
    if (k == k43Run) o[4] = static_cast<unsigned char>(ah::Next() % 2);
    if (k == k43TintSettle) {
        // each tint byte at 0x80, one step above it, or anything; half the
        // time all three at 0x80 or 0x82 (the release)
        const bool settle = ah::Half();
        for (const U off : {0x5Du, 0x5Eu, 0x5Fu}) {
            if (settle) o[off] = static_cast<unsigned char>(ah::Half() ? 0x80 : 0x82);
            else if (ah::Often()) o[off] = static_cast<unsigned char>(AH_PICK(0x80, 0x80, 0x82, 0x81, 0x7F, 0x84, 0xC0));
        }
    }
}
// Area 43's disturbance: Field_ActiveMember (read after Sprite_SetTint and
// before Tint_Release).
void Disturb43(U h) {
    if ((h >> 16) % 2 == 0) ah::SetPointer(kActiveMember, SomeRecord(h >> 20));
}
// The trigger's (object, flags): a field object and the bank, never read.
void Args43(unsigned k, U* a) {
    if (k != k43Trigger) return;
    a[0] = Key(ah::Object(ah::Next()));
    a[1] = at::kStoryFlags;
}

// ===========================================================================
// Area 44
// ===========================================================================

// clones: tools/area_rows.py --unit AREA044 --clones
constexpr ah::CallSite kCalls407210[] = {{0xD6, 0x407320}, {0xF3, 0x57C110}, {0xFF, 0x57C110}};
constexpr ah::CallSite kCalls407320[] = {{0x29, 0x579F00}, {0x33, 0x579F00}, {0x40, 0x579F00}, {0x4A, 0x579F00}, {0x54, 0x579F00}, {0x5E, 0x579F00}, {0x85, 0x579F00}, {0x8F, 0x579F00}, {0x9C, 0x579F00}, {0xA6, 0x579F00}, {0xB0, 0x579F00}, {0xBA, 0x579F00}, {0xD9, 0x579F00}, {0xE3, 0x579F00}, {0xF0, 0x579F00}, {0xFA, 0x579F00}, {0x104, 0x579F00}, {0x10E, 0x579F00}, {0x133, 0x579F00}, {0x13D, 0x579F00}, {0x147, 0x579F00}, {0x151, 0x579F00}, {0x15E, 0x579F00}, {0x168, 0x579F00}, {0x18E, 0x579F00}, {0x198, 0x579F00}, {0x1A2, 0x579F00}, {0x1AC, 0x579F00}, {0x1B9, 0x579F00}, {0x1C3, 0x579F00}, {0x1E5, 0x579F00}, {0x1EF, 0x579F00}, {0x1FC, 0x579F00}, {0x206, 0x579F00}, {0x210, 0x579F00}, {0x21A, 0x579F00}};
constexpr ah::CallSite kCalls407550[] = {{0x4C, 0x57C160}, {0x56, 0x587740}, {0x5E, 0x57C7C0}};
constexpr ah::CallSite kCalls4075D0[] = {{0x3C, 0x57C140}, {0x4C, 0x4077F0}, {0x56, 0x587740}, {0x81, 0x57C140}, {0x91, 0x4077F0}, {0x9B, 0x587740}, {0xCE, 0x407320}, {0xDE, 0x57C140}, {0xF2, 0x4077F0}, {0x10F, 0x587740}, {0x13E, 0x57C140}, {0x14E, 0x4077F0}, {0x16B, 0x587740}, {0x19A, 0x587740}, {0x1B4, 0x407320}, {0x1BE, 0x587740}, {0x1DD, 0x57C7A0}};
constexpr ah::JumpTable kTables4075D0[] = {{0x2D, 0x1E4, 6}};
constexpr ah::CallSite kCalls4077F0[] = {{0x83, 0x589660}};
constexpr ah::CallSite kCalls407940[] = {{0x3A, 0x57C140}, {0x4A, 0x4077F0}, {0x54, 0x587740}, {0x7E, 0x57C140}, {0x8E, 0x4077F0}, {0x98, 0x587740}, {0xCB, 0x407320}, {0xDB, 0x57C140}, {0xEF, 0x4077F0}, {0x117, 0x587740}, {0x12D, 0x587740}, {0x14A, 0x57C140}, {0x15A, 0x4077F0}, {0x17F, 0x587740}, {0x199, 0x407320}, {0x1A3, 0x587740}, {0x1C2, 0x57C7A0}};
constexpr ah::JumpTable kTables407940[] = {{0x2B, 0x1CC, 6}};
const ah::Clone kClones44[] = {
    W1B_C(Area44_Init, 0x407210, 0x10B, kCalls407210, S::kInit),
    W1B_C(Area44_SetGates, 0x407320, 0x22E, kCalls407320, S::kCallee),
    W1B_A(Area44_SwitchHook, 0x407550, 0x7C, kCalls407550, S::kHook),
    W1B_T(Area44_GateTailA, 0x4075D0, 0x211, kCalls4075D0, kTables4075D0, S::kTail),
    W1B_C(Area44_PushParty, 0x4077F0, 0x143, kCalls4077F0, S::kCallee),
    W1B_T(Area44_GateTailB, 0x407940, 0x1F9, kCalls407940, kTables407940, S::kTail),
};
enum : unsigned { k44Init, k44SetGates, k44Switch, k44TailA, k44Push, k44TailB };
const ah::Callee kCallees44[] = {
    {"Area44_SetGates", 0x407320, 0x407320, 0, {}, ah::Answer::kPhase, 0, 0},
    {"Area44_PushParty", 0x4077F0, 0x4077F0, 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0},
    // the x pushed with the caller's esi / edi high half above it (read as an s16)
    {"AreaMap_SetByte", bof3::addr::AreaMap_SetByte, KeyOf(&::AreaMap_SetByte), 3, {kU16, kAll, kAll}, ah::Answer::kGarbage, 0, 0},
    // the flag byte pushed with a table pointer's high bytes above it
    {"flag toggle 0x57C160", kFlagsToggle, kFlagsToggle, 2, {kAll, kU8}, ah::Answer::kGarbage, 0, 0},
    // the member stands on extra object 0 or 1 (0x1E, 0x1F) now and then
    {"Sprite_FindNearby", bof3::addr::Sprite_FindNearby, KeyOf(&::Sprite_FindNearby), 0, {}, ah::Answer::kByte, 0x1C, 0x21},
    kSet40, kClear40,
};
// Area 44's gate and switch tables (0x5F7074..0x5F7095), randomised as a
// region and put back to the exe's bytes two rounds in three.
constexpr U kA44Tables = at::kA44Gate0, kA44TablesBytes = at::kA44SwitchesEnd - at::kA44Gate0;
unsigned char g_a44_tables[kA44TablesBytes];
const ah::Region kRegions44[] = {{kA44Tables, kA44TablesBytes}};
unsigned g_switch44 = 0;   // the switch record this round's hook arguments aim at
void Seed44(unsigned k) {
    if (ah::Often()) std::memcpy(Mem(kA44Tables), g_a44_tables, sizeof g_a44_tables);
    Mem(at::kCells44)[0] = static_cast<unsigned char>(ah::Next());
    switch (k) {
    case k44Init:
        Cond_ByteFD = static_cast<unsigned char>(ah::Often() ? AH_PICK(0, 1, 1, 2, 3, 0xFF) : ah::Next());
        break;
    case k44Switch: {
        g_switch44 = ah::Next() % 4;
        const unsigned char* const r = Mem(at::kA44Switches + g_switch44 * 5);
        Mem(at::kLeaderDir)[0] = static_cast<unsigned char>(ah::Often() ? r[2] & 0xF : ah::Next());
        break;
    }
    case k44TailA:
    case k44TailB:
        Mem(at::kTailState)[0] = TailState({0, 0, 1, 1, 10, 10, 11, 11, 20, 20});
        if (ah::Often()) Mem(at::kExtra0)[9] = 0;
        if (ah::Often()) Mem(at::kExtra1)[9] = 0;
        break;
    default:
        break;
    }
}
void Args44(unsigned k, U* a) {
    if (k == k44Switch) {
        const unsigned char* const r = Mem(at::kA44Switches + g_switch44 * 5);
        a[0] = ah::Often() ? r[0] | (ah::Half() ? 0 : ah::Next() & 0xFFFFFF00u) : ah::Next();
        a[1] = ah::Often() ? r[1] | (ah::Half() ? 0 : ah::Next() & 0xFFFFFF00u) : ah::Next();
        if (ah::Half()) a[ah::Next() & 1] ^= 1u << (ah::Next() % 8);
    } else if (k == k44Push) {
        // the directions and objects the tails pass, and every one in range
        a[0] = (ah::Often() ? AH_PICK(1, 3, 5, 7) : ah::Next() % 8) | (ah::Half() ? 0 : ah::Next() & 0xFFFFFF00u);
        a[1] = (ah::Often() ? ah::Next() % 2 : ah::Next() % 4) | (ah::Half() ? 0 : ah::Next() & 0xFFFFFF00u);
    }
}
// Area 44's disturbance: the gate byte and the tail's state (both read again
// after calls), Field_MemberCount (PushParty's loop bound, read each pass).
void Disturb44(U h) {
    const unsigned v = (h >> 8) & 0xFF;
    switch ((h >> 16) % 3) {
    case 0: Mem(at::kCells44)[0] = static_cast<unsigned char>(v); break;
    case 1: {
        static const unsigned char kStates[] = {1, 0xA, 0xB, 0x14, 0, 2};
        Mem(at::kTailState)[0] = kStates[v % 6];
        break;
    }
    case 2: Field_MemberCount = static_cast<unsigned char>(1 + v % 3); break;
    default: break;
    }
}

// ===========================================================================
// Area 45 (area 16's code over area 45's tables: area_w0b_fuzz.cpp's group)
// ===========================================================================

// --- the fuzz's own memory: a packet buffer, four map items, four names ------

constexpr unsigned kPacketBytes = 0x200, kItemBytes = 0x48;
alignas(16) unsigned char g_packets[kPacketBytes];
alignas(16) unsigned char g_items[4][kItemBytes];
alignas(16) unsigned char g_names[4][16];

// The exe's own bytes of the tables the fuzz randomises, put back two rounds
// in three (so the seeds see the shipped values most of the time).
unsigned char g_a45_tables[0x5BC];     // 0x5F7098..0x5F7653
unsigned char g_a45_dirs[0x18];        // 0x5F770C..0x5F7723
unsigned char g_a45_drift[0xC];        // 0x5F772C..0x5F7737

bool InPackets(U p, unsigned n) { return p >= Key(g_packets) && p + n <= Key(g_packets) + kPacketBytes; }

// The packet cursor moved on by `size`, kept inside the fuzz's buffer.
void Advance(U size) {
    U next = Key(Gfx_PacketNext) + size;
    if (!InPackets(next, 0x50)) next = Key(g_packets) + (ah::Noise() % 4) * 4;
    Gfx_PacketNext = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(next));
}
void Scribble(U at, unsigned n) {
    if (!InPackets(at, n)) return;
    unsigned char* const p = Mem(at);
    for (unsigned i = 0; i < n; i += 4) SetLong(p + i, static_cast<std::int32_t>(ah::Noise()));
}

// --- the effects: the callees that write what the caller reads after ---------

U CommitEffect(const U* a, U answer) { Advance(a[1] & 0xFF); return answer; }
U LinkEffect(const U* a, U answer) {
    if (ah::Noise() % 5) Advance(a[3] & 0xFF);
    return answer;
}
// AreaMap_ByteAt: the cell kinds the world map tests, mostly.
U ByteAtEffect(const U*, U answer) {
    static const U kCells[] = {0xA1, 0xA1, 0xA0, 0xAE, 0xA2, 0x9F, 0, 0x21};
    const U n = ah::Noise();
    return (answer & 0xFFFFFF00u) | (n % 5 == 0 ? (n >> 8) & 0xFF : kCells[(n >> 4) % 8]);
}
// Item_NamePtr: one of the fuzz's four 16-byte names.
U NameEffect(const U*, U answer) { return Key(g_names[(answer >> 4) % 4]); }
// MapView_ItemHalfAt: a third of the time none, else one of the fuzz's items.
U ItemEffect(const U*, U answer) { return answer % 3 == 0 ? 0u : Key(g_items[(answer >> 4) % 4]); }
// The primitive setters write the primitive's bytes, so a store the caller
// makes before the call (where the original makes it after) shows.
U PolyEffect(const U* a, U answer) {
    Scribble(a[0], 0x48);
    if (InPackets(a[0], 8)) Mem(a[0])[7] = 0x2C;
    return answer;
}
U SprtEffect(const U* a, U answer) { Scribble(a[0], 0x20); return answer; }
U DrawModeEffect(const U* a, U answer) { Scribble(a[0], 0xC); return answer; }
U ShadeEffect(const U* a, U answer) {
    if (InPackets(a[0], 8)) Mem(a[0])[7] = static_cast<unsigned char>(a[1] ? Mem(a[0])[7] | 1 : Mem(a[0])[7] & 0xFE);
    return answer;
}
U SemiEffect(const U* a, U answer) {
    if (InPackets(a[0], 8)) Mem(a[0])[7] = static_cast<unsigned char>(a[1] ? Mem(a[0])[7] | 2 : Mem(a[0])[7] & 0xFD);
    return answer;
}
// Gte_RotTransPers4 (ten words pushed): the four corners written; the two
// stack locals are the caller's and never read after.
U PersEffect(const U* a, U answer) {
    for (unsigned c = 4; c < 8; ++c) Scribble(a[c], 8);
    return answer;
}
U DepthEffect(const U* a, U answer) {
    for (U z = 0x10; z <= 0x40; z += 0x10) Scribble(a[0] + z, 4);
    return answer;
}
U TextureEffect(const U* a, U answer) {
    if (InPackets(a[1], 0x18)) Mem(a[1])[0x16] = static_cast<unsigned char>(a[0]);
    return answer;
}

// Gte_RotTransPers4's listing: the vectors' eight bytes each, the corners'
// addresses, the two locals not logged (stack addresses differ).
const ah::Callee kPers4 = {"Gte_RotTransPers4", bof3::addr::Gte_RotTransPers4, KeyOf(&::Gte_RotTransPers4), 10,
                               {kAll, kAll, kAll, kAll, kAll, kAll, kAll, kAll, 0, 0}, ah::Answer::kGarbage, 0, 0,
                               {8, 8, 8, 8}, &PersEffect, nullptr};

// ===========================================================================
// Area 45
// ===========================================================================

// clones: tools/area_rows.py --unit AREA045 --clones (area 16's rows at area 45's addresses: the same offsets)
constexpr ah::CallSite kCalls407B40[] = {{0x22, 0x57C7A0}, {0x3B, 0x57C7C0}, {0x4F, 0x536700}, {0x8E, 0x4976D0}, {0x113, 0x591680}, {0x154, 0x4976D0}};
constexpr ah::CallSite kCalls407CC0[] = {{0x55, 0x536700}, {0x6E, 0x536700}, {0x88, 0x536700}};
constexpr ah::CallSite kCalls407DA0[] = {{0xB, 0x589590}};
constexpr ah::CallSite kCalls407DF0[] = {{0x70, 0x5891F0}, {0xB2, 0x5891F0}, {0xF0, 0x5891F0}, {0x12F, 0x5891F0}};
constexpr ah::CallSite kCalls407F40[] = {{0x0, 0x4112A0}, {0x3C, 0x5890E0}};
constexpr ah::CallSite kCalls407F90[] = {{0x0, 0x4112A0}, {0x53, 0x5890E0}};
constexpr ah::CallSite kCalls407FF0[] = {{0x0, 0x4112A0}, {0x38, 0x589840}, {0x4B, 0x5890E0}};
constexpr ah::CallSite kCalls408060[] = {{0x0, 0x408070}, {0x5, 0x408140}};
constexpr ah::CallSite kCalls408090[] = {{0x1C, 0x4080C0}};
constexpr ah::CallSite kCalls4080C0[] = {{0x1F, 0x4082A0}};
constexpr ah::CallSite kCalls4080F0[] = {{0x38, 0x4082A0}};
constexpr ah::CallSite kCalls408160[] = {{0x59, 0x4086D0}};
constexpr ah::CallSite kCalls4081D0[] = {{0x65, 0x4086D0}};
constexpr ah::CallSite kCalls408240[] = {{0x4E, 0x4086D0}};
constexpr ah::CallSite kCalls4082A0[] = {{0x22, 0x5A77C0}, {0x2B, 0x461E50}, {0x3C, 0x408470}, {0x51, 0x536700}, {0x7B, 0x408470}, {0xDC, 0x408470}, {0xF8, 0x408470}, {0x15B, 0x408470}, {0x173, 0x531920}, {0x1A8, 0x408470}, {0x1B8, 0x408530}};
constexpr ah::CallSite kCalls408470[] = {{0x13, 0x5A77C0}, {0x1C, 0x461E50}, {0x28, 0x5A7710}, {0x3F, 0x5A7780}, {0xB1, 0x461E50}};
constexpr ah::CallSite kCalls4086D0[] = {{0x17, 0x408470}, {0x26, 0x408470}, {0x4D, 0x516B30}};
constexpr ah::CallSite kCalls408750[] = {{0x2, 0x589590}, {0x133, 0x5891F0}, {0x14F, 0x588F20}};
constexpr ah::CallSite kCalls4088D0[] = {{0x1C, 0x462A90}, {0x26, 0x589590}, {0x9B, 0x5891F0}, {0xB0, 0x589840}};
constexpr ah::CallSite kCalls408990[] = {{0x0, 0x5893A0}, {0x5, 0x588F00}};
constexpr ah::CallSite kCalls4089A0[] = {{0xB7, 0x5A75D0}, {0xBF, 0x5A77A0}, {0x199, 0x5A85F0}, {0x19F, 0x5A9290}, {0x1AF, 0x572A00}, {0x1BB, 0x461E50}, {0x21D, 0x572F70}, {0x236, 0x5A75D0}, {0x23E, 0x5A77A0}, {0x246, 0x5A7780}, {0x430, 0x572FA0}};

const ah::Clone kClones45[] = {
    W1B_C(Area45_PlaceMessage, 0x407B40, 0x174, kCalls407B40, S::kState),
    W1B_C(Area45_PlateRun, 0x407CC0, 0xD6, kCalls407CC0, S::kState),
    W1B_C(Area45_PlateStart, 0x407DA0, 0x4E, kCalls407DA0, S::kState),
    W1B_C(Area45_PlateShow, 0x407DF0, 0x142, kCalls407DF0, S::kState),
    W1B_C(Area45_PlateGrow, 0x407F40, 0x41, kCalls407F40, S::kState),
    W1B_C(Area45_PlateHold, 0x407F90, 0x58, kCalls407F90, S::kState),
    W1B_C(Area45_PlateShrink, 0x407FF0, 0x50, kCalls407FF0, S::kState),
    W1B_P(Area45_HudRun, 0x408040, 0x12, S::kState),
    W1B_C(Area45_HudFrame, 0x408060, 0xA, kCalls408060, S::kState),
    W1B_P(Area45_FrameStep, 0x408070, 0x12, S::kCallee),
    W1B_C(Area45_FrameSlideIn, 0x408090, 0x21, kCalls408090, S::kState),
    W1B_C(Area45_FrameHold, 0x4080C0, 0x28, kCalls4080C0, S::kState),
    W1B_C(Area45_FrameSlideOut, 0x4080F0, 0x41, kCalls4080F0, S::kState),
    W1B_P(Area45_BoxStep, 0x408140, 0x12, S::kCallee),
    W1B_C(Area45_BoxSlideIn, 0x408160, 0x62, kCalls408160, S::kState),
    W1B_C(Area45_BoxHold, 0x4081D0, 0x6E, kCalls4081D0, S::kState),
    W1B_C(Area45_BoxSlideOut, 0x408240, 0x57, kCalls408240, S::kState),
    W1B_C(Area45_DrawFrame, 0x4082A0, 0x1C5, kCalls4082A0, S::kCallee),
    W1B_C(Area45_DrawSprite, 0x408470, 0xBC, kCalls408470, S::kCallee),
    W1B_C(Area45_DrawHud, 0x4086D0, 0x58, kCalls4086D0, S::kCallee),
    W1B_P(Area45_Record8Run, 0x408730, 0x12, S::kState),
    W1B_C(Area45_Record8Place, 0x408750, 0x154, kCalls408750, S::kState),
    W1B_P(Area45_Record4Run, 0x4088B0, 0x12, S::kState),
    W1B_C(Area45_Record4MarkCell, 0x4088D0, 0xB5, kCalls4088D0, S::kState),
    W1B_C(Area45_DrawDrift, 0x4089A0, 0x462, kCalls4089A0, S::kState),
    W1B_C(Area45_Record4Tick, 0x408990, 0xA, kCalls408990, S::kState),
};
enum : unsigned {
    kPlaceMessage, kPlateRun, kPlateStart, kPlateShow, kPlateGrow, kPlateHold, kPlateShrink, kHudRun, kHudFrame,
    kFrameStep, kFrameSlideIn, kFrameHold, kFrameSlideOut, kBoxStep, kBoxSlideIn, kBoxHold, kBoxSlideOut, kDrawFrame,
    kDrawSprite, kDrawHud, kRecord8Run, kRecord8Place, kRecord4Run, kRecord4MarkCell, kDrawDrift, kRecord4Tick,
};
static_assert(kRecord4Tick + 1 == sizeof kClones45 / sizeof kClones45[0], "area 45's seeding indices");

const ah::Callee kCallees45[] = {
    // area 45's own, called directly
    {"Area45_FrameStep", 0x408070, 0x408070, 0, {}, ah::Answer::kPhase, 0, 0},
    {"Area45_BoxStep", 0x408140, 0x408140, 0, {}, ah::Answer::kPhase, 0, 0},
    {"Area45_DrawFrame", 0x4082A0, 0x4082A0, 2, {kAll, kU16}, ah::Answer::kGarbage, 0, 0},   // y pushed with a stale high half
    {"Area45_DrawSprite", 0x408470, 0x408470, 3, {kAll, kAll, kU8}, ah::Answer::kGarbage, 0, 0},   // a lit key's index carries the button word above its byte
    {"Area45_DrawHud", 0x4086D0, 0x4086D0, 2, {kAll, kU16}, ah::Answer::kGarbage, 0, 0},   // y pushed with the object pointer's high half
    // named, ours, beyond the standard set
    {"ScriptFlags_Set40", bof3::addr::ScriptFlags_Set40, KeyOf(&::ScriptFlags_Set40), 0, {}, ah::Answer::kGarbage, 0, 0},
    {"ScriptFlags_Clear40", bof3::addr::ScriptFlags_Clear40, KeyOf(&::ScriptFlags_Clear40), 0, {}, ah::Answer::kGarbage, 0, 0},
    {"Item_NamePtr", bof3::addr::Item_NamePtr, KeyOf(&::Item_NamePtr), 2, {kU8, kU8}, ah::Answer::kGarbage, 0, 0, {}, &NameEffect, nullptr},
    {"WorldMap_PinSprite", bof3::addr::WorldMap_PinSprite, KeyOf(&::WorldMap_PinSprite), 0, {}, ah::Answer::kGarbage, 0, 0},
    {"Effect_Release", bof3::addr::Effect_Release, KeyOf(&::Effect_Release), 0, {}, ah::Answer::kGarbage, 0, 0},
    {"Gfx_CommitPrim", bof3::addr::Gfx_CommitPrim, KeyOf(&::Gfx_CommitPrim), 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &CommitEffect, nullptr},
    {"WorldMap_DrawNeedle", bof3::addr::WorldMap_DrawNeedle, KeyOf(&::WorldMap_DrawNeedle), 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0},
    {"WorldMap_RecordIndex", bof3::addr::WorldMap_RecordIndex, KeyOf(&::WorldMap_RecordIndex), 0, {}, ah::Answer::kGarbage, 0, 0},
    {"Prim_SetTexture", bof3::addr::Prim_SetTexture, KeyOf(&::Prim_SetTexture), 3, {kAll, kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &TextureEffect, nullptr},
    // standard ones listed again with what the caller reads after
    {"AreaMap_ByteAt", bof3::addr::AreaMap_ByteAt, KeyOf(&::AreaMap_ByteAt), 2, {kU16, kU16}, ah::Answer::kGarbage, 0, 0, {}, &ByteAtEffect, nullptr},
    {"Field_CellHasEvent", bof3::addr::Field_CellHasEvent, KeyOf(&::Field_CellHasEvent), 2, {kU16, kU16}, ah::Answer::kFlag, 0, 0},   // words pushed with stale high halves
    {"MapView_ItemHalfAt", bof3::addr::MapView_ItemHalfAt, KeyOf(&::MapView_ItemHalfAt), 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &ItemEffect, nullptr},
    {"MapView_LinkPrimAt", bof3::addr::MapView_LinkPrimAt, KeyOf(&::MapView_LinkPrimAt), 4, {kAll, kAll, kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &LinkEffect, nullptr},
    {"Gpu_SetPolyFT4", bof3::addr::Gpu_SetPolyFT4, KeyOf(&::Gpu_SetPolyFT4), 1, {kAll}, ah::Answer::kGarbage, 0, 0, {}, &PolyEffect, nullptr},
    {"Gpu_SetShadeTex", bof3::addr::Gpu_SetShadeTex, KeyOf(&::Gpu_SetShadeTex), 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &ShadeEffect, nullptr},
    {"Gpu_SetSemiTrans", bof3::addr::Gpu_SetSemiTrans, KeyOf(&::Gpu_SetSemiTrans), 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &SemiEffect, nullptr},
    {"Gpu_SetSprt", bof3::addr::Gpu_SetSprt, KeyOf(&::Gpu_SetSprt), 1, {kAll}, ah::Answer::kGarbage, 0, 0, {}, &SprtEffect, nullptr},
    {"Gpu_SetDrawMode", bof3::addr::Gpu_SetDrawMode, KeyOf(&::Gpu_SetDrawMode), 5, {kAll, kAll, kAll, kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &DrawModeEffect, nullptr},
    {"Gte_PrimDepths4_10", bof3::addr::Gte_PrimDepths4_10, KeyOf(&::Gte_PrimDepths4_10), 1, {kAll}, ah::Answer::kGarbage, 0, 0, {}, &DepthEffect, nullptr},
    kPers4,
};

const ah::DataTable kTables45[] = {
    {at::kA45PlateStates, 5}, {at::kA45HudStates, 2}, {at::kA45FrameStates, 4},
    {at::kA45BoxStates, 4},   {at::kA45Record8States, 3}, {at::kA45Record4States, 2},
};

ah::Region g_regions45[] = {
    {at::kMapMode, 1},
    {0x7E0918, 1},                // Draw_PassFlags
    {0x7E0670, 4},                // Gfx_PacketNext
    {0, kPacketBytes},            // g_packets (set at start-up)
    {0x9037A0, 0x20},             // Prim_VertexScratch
    {0x66C7E8, 2},                // Game_Mode
    {at::kButtonMap0, 0x10},      // the button map's words 0..7
    {0, 4 * kItemBytes},          // g_items
    {0, sizeof g_names},          // g_names
    {at::kTextRecords, 0x80},     // Text_Records' first four rows
    {at::kAreaTextOffset45, 4},
    {at::kFlag3A79, 1},
    {at::kA45PlateAnims, 0x5BC},  // area 45's plate, cell, message and name tables
    {at::kA45Directions, 0x18},   // Area45_Directions, Area45_Record8Anims
    {at::kA45DriftUBase, 0xC},    // Area45_DriftUV
};

unsigned char* Obj() { return Sprite_Current; }

// The leader's cell words as bytes, and one Area45_Cells record planted with
// them (the search has no bound), a sentinel in the last record.
void PlantCell(bool random_place) {
    unsigned char* const cx = Mem(at::kLeaderCellWordX);
    unsigned char* const cz = Mem(at::kLeaderCellWordZ);
    cx[1] = 0;
    cz[1] = 0;
    const U n = (at::kA45CellsEnd - at::kA45Cells) / 4;
    unsigned char* const last = Mem(at::kA45CellsEnd - 4);
    last[0] = cx[0];
    last[1] = cz[0];
    // the sentinel's id follows the words, so a search with stale words (or
    // none again) lands on a record of another set
    last[3] = Mem(at::kA45NameSets + ((cx[0] ^ cz[0]) % 3) * 5)[0];
    if (random_place) {
        unsigned char* const r = Mem(at::kA45Cells + (ah::Next() % n) * 4);
        r[0] = cx[0];
        r[1] = cz[0];
    }
}

// Area 45's disturbance: the cells its functions read again after a call.
void Disturb45(U h) {
    const unsigned v = (h >> 8) & 0xFF;
    switch ((h >> 16) % 12) {
    case 0: Mem(at::kMapMode)[0] = static_cast<unsigned char>(v % 4 == 0 ? v : v % 3); break;
    case 1: Draw_PassFlags = static_cast<unsigned char>(Draw_PassFlags ^ (v & 1 ? 4 : 0x1B)); break;
    case 2: Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags ^ (v & 1 ? 0x100 : 0x4000)); break;
    case 3: Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 ^ 0x1000); break;
    case 4: Mem(at::kMsgState)[0] = static_cast<unsigned char>(v % 3); break;
    case 5: Game_Mode = static_cast<unsigned short>(Game_Mode ^ 1); break;
    case 6: Cond_ByteFA = static_cast<signed char>(v % 16); break;
    case 7: SetWord(Mem(at::kPlace), v); break;
    case 8: Mem(at::kLeaderCellX + (v & 1) * 2)[0] = static_cast<unsigned char>(v); break;
    case 9: Mem(at::kPartySet)[0] = static_cast<unsigned char>(v & 1 ? 0xC : v); break;
    case 10: case 11: Mem(v & 1 ? at::kLeaderCellWordX : at::kLeaderCellWordZ)[0] = static_cast<unsigned char>(h >> 24); break;   // the settle re-plants its record
    default: break;
    }
}
// After every disturbance: the dispatch bytes inside their tables (the plate
// run reads +1 after its calls), the leader's cell words bytes with their
// sentinel record (the place hook reads them again after its call).
void Settle45() {
    unsigned char* const o = Obj();
    if (o[1] >= 5) o[1] = static_cast<unsigned char>(o[1] % 5);
    PlantCell(false);
}

void Seed45(unsigned k) {
    unsigned char* const o = Obj();
    Gfx_PacketNext = g_packets + (ah::Next() % 4) * 4;
    if (ah::Often()) std::memcpy(Mem(at::kA45PlateAnims), g_a45_tables, sizeof g_a45_tables);
    if (ah::Often()) std::memcpy(Mem(at::kA45Directions), g_a45_dirs, sizeof g_a45_dirs);
    if (ah::Often()) std::memcpy(Mem(at::kA45DriftUBase), g_a45_drift, sizeof g_a45_drift);
    o[1] = static_cast<unsigned char>(o[1] % 5);
    PlantCell(true);
    const auto leave = [o] {
        Mem(at::kMapMode)[0] = static_cast<unsigned char>(ah::Often() ? AH_PICK(0, 0, 1, 2) : ah::Next());
        if (ah::Often()) o[0xB] = static_cast<unsigned char>(ah::Half() ? 0 : ah::Next() | 1);
        if (ah::Often()) Field_Request = static_cast<unsigned char>(AH_PICK(0, 2, 5, 1, 3));
        Field_ScriptFlags = static_cast<unsigned short>(ah::Often() ? Field_ScriptFlags & ~0x100u : Field_ScriptFlags | 0x100u);
    };
    switch (k) {
    case kPlaceMessage: {
        Mem(at::kMsgState)[0] = static_cast<unsigned char>(ah::Often() ? AH_PICK(0, 0, 0, 1, 1, 2, 0xFF, 0x80) : ah::Next());
        Cond_ByteFA = static_cast<signed char>(ah::Often() ? AH_PICK(1, 2, 15, 0, 0xFF, 0x80, 0x7F, 8) : ah::Next());
        if (ah::Often()) Field_Request = static_cast<unsigned char>(AH_PICK(2, 2, 0, 5, 3));
        // the place in one of the eleven rows, or in none
        if (ah::Often()) SetWord(Mem(at::kA45PlaceMessages + (ah::Next() % 11) * 0x20), Word(Mem(at::kPlace)));
        // the found cell's id one of the three sets', or none
        unsigned char* cell = Mem(at::kA45Cells);
        while (!(cell[0] == Mem(at::kLeaderCellWordX)[0] && cell[1] == Mem(at::kLeaderCellWordZ)[0])) cell += 4;
        if (ah::Often()) cell[3] = Mem(at::kA45NameSets + (ah::Next() % 3) * 5)[0];
        // items held or not, now and then a list ended early
        for (unsigned i = 0; i < 0x74; ++i)
            if (ah::Half()) Mem(at::kItemsHeld + i)[0] = static_cast<unsigned char>(ah::Half() ? 0 : ah::Next() | 1);
        if (ah::Half()) Mem(at::kA45NameSets + 1 + (ah::Next() % 3) * 5 + ah::Next() % 4)[0] = static_cast<unsigned char>(AH_PICK(0xFF, 0x16, 0x5E));
        break;
    }
    case kPlateRun:
        if (ah::Half()) Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 ^ 0x1000);
        if (ah::Half()) Mem(at::kLeaderSteps)[0] = static_cast<unsigned char>(AH_PICK(0, 1, 0xFF, 0x80));
        break;
    case kPlateShow:
        o[0xB] = static_cast<unsigned char>(ah::Often() ? AH_PICK(1, 1, 1, 2, 3, 4, 0, 5) : ah::Next());
        SetWord(Mem(at::kA45PlateAnims + (ah::Next() % 10) * 4), Word(Mem(at::kPlace)));
        break;
    case kPlateGrow:
    case kPlateShrink:
        if (ah::Often()) o[9] = static_cast<unsigned char>(AH_PICK(1, 1, 2, 0, 0xFF, 0x80));
        if (ah::Half()) SetLong(o + 0x40, static_cast<std::int32_t>(AH_PICK(0, 0xE000, 0x10000, 0xFFFFE000u, 0x7FFFF000)));
        if (ah::Often()) Field_Request = static_cast<unsigned char>(AH_PICK(5, 5, 2, 0));
        break;
    case kPlateHold:
        Game_Mode = static_cast<unsigned short>(ah::Often() ? AH_PICK(0, 2, 1, 0x101) : ah::Next());
        if (ah::Often()) o[7] = static_cast<unsigned char>(ah::Half() ? 1 : AH_PICK(0, 2, 3, 4));
        if (ah::Often()) o[0xB] = o[7];
        if (ah::Often()) SetLong(o + 0x18, static_cast<std::int32_t>(Word(Mem(at::kPlace)) | (ah::Half() ? 0 : 0x10000u)));
        if (ah::Often()) Field_Request = static_cast<unsigned char>(AH_PICK(0, 5, 2));
        break;
    case kHudRun: o[1] = static_cast<unsigned char>(ah::Next() % 2); break;
    case kFrameStep: o[2] = static_cast<unsigned char>(ah::Next() % 4); break;
    case kBoxStep: o[3] = static_cast<unsigned char>(ah::Next() % 4); break;
    case kRecord8Run: o[1] = static_cast<unsigned char>(ah::Next() % 3); break;
    case kRecord4Run: o[1] = static_cast<unsigned char>(ah::Next() % 2); break;
    case kFrameSlideIn:
        if (ah::Often()) SetWord(o + 0x2E, AH_PICK(0, 0xFFFF, 1, 0xFFF0, 0x7FF0, 0x7FFF, 0xFFD0));
        Mem(at::kMapMode)[0] = static_cast<unsigned char>(ah::Often() ? AH_PICK(2, 0, 1) : ah::Next());
        break;
    case kFrameHold:
        Mem(at::kMapMode)[0] = static_cast<unsigned char>(ah::Often() ? AH_PICK(2, 0, 1, 0x82) : ah::Next());
        break;
    case kFrameSlideOut:
        if (ah::Often()) SetWord(o + 0x2E, AH_PICK(0xFFE0, 0xFFE1, 0xFFDF, 0x8000, 0x800F, 0x8010, 0x10));
        Mem(at::kMapMode)[0] = static_cast<unsigned char>(ah::Often() ? AH_PICK(2, 0, 1, 0x82) : ah::Next());
        break;
    case kBoxSlideIn:
        if (ah::Often()) SetWord(o + 0x30, AH_PICK(0xD1, 0xD2, 0xD3, 0xF0, 0x800A, 0x8009, 0));
        leave();
        break;
    case kBoxHold:
        if (ah::Often()) o[0xB] = 0;
        if (ah::Often()) o[9] = static_cast<unsigned char>(AH_PICK(0x58, 0x59, 0x5A, 0xFF, 0, 0x7F));
        leave();
        break;
    case kBoxSlideOut:
        if (ah::Often()) SetWord(o + 0x30, AH_PICK(0xE5, 0xE6, 0xE7, 0xC8, 0x7FF6, 0x7FF5));
        leave();
        break;
    case kDrawFrame:
    case kDrawHud:
    case kDrawSprite:
        Draw_PassFlags = static_cast<unsigned char>(ah::Often() ? AH_PICK(1, 2, 8, 0x10, 0x1B, 4, 0x20, 0xE4) : ah::Next());
        // the button words with a bit of one entry's mask, or none
        if (ah::Often()) SetLong(Mem(at::kButtonMap0), static_cast<std::int32_t>(Word(Mem(at::kA45Buttons + (ah::Next() % 6) * 4)) | (ah::Next() & 0xFFFF0000u)));
        if (ah::Half()) {
            // a word only the eighth entry answers, when it has such bits
            U others = 0;
            for (U e = 0; e < 7; ++e) others |= Word(Mem(at::kA45Buttons + e * 4));
            const U only = Word(Mem(at::kA45Buttons + 7 * 4)) & ~others;
            if (only != 0) SetLong(Mem(at::kButtonMap6), static_cast<std::int32_t>(only | (ah::Next() & 0xFFFF0000u)));
        } else if (ah::Often()) SetLong(Mem(at::kButtonMap6), static_cast<std::int32_t>(Word(Mem(at::kA45Buttons + (ah::Half() ? 6 + ah::Next() % 2 : ah::Next() % 8) * 4)) | (ah::Next() & 0xFFFF0000u)));
        if (ah::Half()) SetLong(Mem(at::kButtonMap0), 0);
        if (ah::Half()) Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 & ~0x1000u);
        if (ah::Half()) Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags & ~0x4000u);
        if (ah::Half()) Mem(at::kPartySet)[0] = static_cast<unsigned char>(AH_PICK(0xC, 0x8C, 0xB, 0xFF));
        break;
    case kRecord8Place:
        if (ah::Often()) o[8] = static_cast<unsigned char>(ah::Next() % 4);
        if (ah::Often()) o[6] = static_cast<unsigned char>(AH_PICK(0, 1, 2, 0xFF, 1));
        break;
    case kRecord4MarkCell:
        Mem(at::kFlag3A79)[0] = static_cast<unsigned char>(ah::Often() ? AH_PICK(0, 9, 8, 0x89) : ah::Next());
        if (ah::Often()) Field_StatusBits = static_cast<unsigned char>(Field_StatusBits & ~1u);
        // the map's width so that any cell record lands in the block
        AreaMap_Header[0] = static_cast<unsigned char>(ah::Next() % 0x18);
        if (ah::Often()) o[0xB] = static_cast<unsigned char>(ah::Next() % 265);
        break;
    case kDrawDrift: {
        if (ah::Half()) o[2] = 0;
        if (ah::Often()) Draw_PassFlags = static_cast<unsigned char>(Draw_PassFlags | 4);
        o[0xB] = static_cast<unsigned char>(ah::Often() ? AH_PICK(2, 3, 2, 3, 0, 1, 4, 5) : ah::Next());
        const unsigned height = Mem(at::kMapHeight)[0];
        if (ah::Half()) SetWord(o + 0x3A, height + AH_PICK(8, 9, 7, 0, 0x7FFF));
        const auto near = [](unsigned char* cell, unsigned centre) {
            SetWord(cell, centre + AH_PICK(0, 25, 26, static_cast<U>(-25), static_cast<U>(-26), 1, 100));
        };
        if (ah::Often()) near(Mem(at::kLeaderCellX), Word(o + 0x36));
        if (ah::Often()) near(Mem(at::kLeaderCellZ), Word(o + 0x3A));
        if (ah::Half()) Frame_Counter = AH_PICK(0, 15, 16, 31, 0x2F, 0xFFFFFFFFu);
        break;
    }
    default: break;
    }
}

// The draws' arguments: x and y whole words (the dial's and the box's
// positions, and anything), the sprite's index a byte with bits above it.
void Args45(unsigned k, U* a) {
    if (k == kDrawFrame || k == kDrawHud || k == kDrawSprite) {
        a[0] = ah::Often() ? AH_PICK(0x10, 0x5C, 0, 0x7FF0) : ah::Next();
        a[1] = ah::Often() ? AH_PICK(0xFFD0, 0xFFFFFFD0u, 0xC8, 0xF0, 0x10) | (ah::Half() ? ah::Next() & 0xFFFF0000u : 0) : ah::Next();
        if (k == kDrawSprite) a[2] = ah::Often() ? AH_PICK(0, 1, 2, 3, 4, 5, 0x15, 0x100, 0x1FF) : ah::Next();
    }
}


// ===========================================================================
// Area 46
// ===========================================================================

// clones: tools/area_rows.py --unit AREA046 --clones
constexpr ah::CallSite kCalls408E10[] = {{0x2E, 0x57C7A0}, {0x4B, 0x531F90}, {0x67, 0x57C7A0}};
constexpr ah::JumpTable kTables408E10[] = {{0x18, 0x7C, 5}};
constexpr ah::CallSite kCalls408EB0[] = {{0x19, 0x5918E0}, {0x25, 0x57C7C0}};
constexpr ah::CallSite kCalls408F10[] = {{0x2, 0x5734F0}};
constexpr ah::CallSite kCalls408F20[] = {{0x8, 0x57C0F0}, {0x1B, 0x57C140}, {0x34, 0x57C0F0}};
const ah::Clone kClones46[] = {
    W1B_T(Area46_DropTail, 0x408E10, 0x9C, kCalls408E10, kTables408E10, S::kTail),
    W1B_A(Area46_StepHook, 0x408EB0, 0x55, kCalls408EB0, S::kHook),
    W1B_C(Area46_PlaceKind2At0, 0x408F10, 0x9, kCalls408F10, S::kHandler),
    W1B_A(Area46_Trigger54, 0x408F20, 0x40, kCalls408F20, S::kCallee),
};
enum : unsigned { k46Tail, k46Step, k46Place, k46Trigger };
const ah::Callee kCallees46[] = {
    {"KeyItem_Has", bof3::addr::KeyItem_Has, KeyOf(&::KeyItem_Has), 1, {kU8}, ah::Answer::kFlag, 0, 0},
    {"Kind2_Place", bof3::addr::Kind2_Place, KeyOf(&::Kind2_Place), 1, {kU8}, ah::Answer::kGarbage, 0, 0},
    kSet40, kClear40,
};
void Seed46(unsigned k) {
    Mem(at::kDrop46)[0] = static_cast<unsigned char>(ah::Often() ? AH_PICK(0, 1, 2, 3, 0xFF) : ah::Next());
    if (k == k46Tail) Mem(at::kTailState)[0] = TailState({0, 0, 1, 1, 10, 10, 11, 11});
    if (k == k46Step) Cond_ByteFA = static_cast<signed char>(ah::Often() ? AH_PICK(7, 8, 9, 0, 0xFF, 0x80, 0x7F) : ah::Next());
}
// The step hook's (x, z): z at the bound 0x218000 (one either side, a
// negative, anything), the columns 0xE..0x10.
void Args46(unsigned k, U* a) {
    if (k == k46Step) {
        a[0] = Column(0xE, 0x10);
        a[1] = ah::Often() ? AH_PICK(0x218000, 0x217FFF, 0x218001, 0x200000, 0x80000000u, 0xFFFFFFFFu, 0x7FFFFFFF) : ah::Next();
    } else if (k == k46Trigger) {
        a[0] = Key(ah::Object(ah::Next()));
        a[1] = at::kStoryFlags;
    }
}
// Area 46's disturbance: Cond_ByteFA (the step hook reads it after its calls).
void Disturb46(U h) {
    static const unsigned char kChapters[] = {7, 8, 9, 0, 0xFF, 0x80};
    if ((h >> 16) % 2 == 0) Cond_ByteFA = static_cast<signed char>(kChapters[(h >> 8) % 6]);
}

// ===========================================================================
// Area 47
// ===========================================================================

// clones: tools/area_rows.py --unit AREA047 --clones
constexpr ah::CallSite kCalls408F60[] = {{0x26, 0x56F670}, {0x41, 0x5725F0}};
constexpr ah::CallSite kCalls408FC0[] = {{0x1, 0x589810}};
const ah::Clone kClones47[] = {
    W1B_C(Area47_PlaceAtStart, 0x408F60, 0x5C, kCalls408F60, S::kHandler),
    W1B_C(Area47_SpawnEffect3D, 0x408FC0, 0x28, kCalls408FC0, S::kHandler),
};
const ah::Callee kCallees47[] = {
    // the word pushed with Field_ViewReset's edx above it: its low 16 bits are all it reads
    {"MapView_SetElevation", bof3::addr::MapView_SetElevation, KeyOf(&::MapView_SetElevation), 1, {kU16}, ah::Answer::kGarbage, 0, 0},
    kFindFree,
};
const ah::Region kRegions47[] = {{kCameraAngles, 6}, {kEffects, 8 * 0x80}};

// ===========================================================================

template <std::size_t N> constexpr unsigned Count(const ah::Clone (&)[N]) { return N; }

void RunArea(int area, const ah::Clone* clones, unsigned n, const ah::Callee* callees, unsigned n_callees,
             const ah::DataTable* tables, unsigned n_tables, const ah::Region* regions, unsigned n_regions,
             void (*seed)(unsigned), void (*disturb)(U), void (*args)(unsigned, U*), unsigned rounds) {
    ah::Group g{"area_w1b", clones, n, callees, n_callees, tables, n_tables, regions, n_regions, seed, disturb, rounds};
    g.args = args;
    g.area = area;
    ah::Run(g);
}

// BOF3X_AR1B_AREA=n runs area n's group alone (the controls script's
// shortcut); unset, every area runs.
bool Wants(int area) {
    const char* const only = std::getenv("BOF3X_AR1B_AREA");
    return only == nullptr || *only == 0 || std::atoi(only) == area;
}

}  // namespace

void SelfTest() {
    g_regions45[3].at = Key(g_packets);
    g_regions45[7].at = Key(g_items);
    g_regions45[8].at = Key(g_names);
    std::memcpy(g_a45_tables, Mem(at::kA45PlateAnims), sizeof g_a45_tables);
    std::memcpy(g_a45_dirs, Mem(at::kA45Directions), sizeof g_a45_dirs);
    std::memcpy(g_a45_drift, Mem(at::kA45DriftUBase), sizeof g_a45_drift);
    std::memcpy(g_a44_tables, Mem(kA44Tables), sizeof g_a44_tables);

    if (Wants(42)) RunArea(42, kClones42, Count(kClones42), kCallees42, AH_COUNT(kCallees42), nullptr, 0, kRegions42, AH_COUNT(kRegions42),
            &Seed42, &Disturb42, &Args42, 6000);
    if (Wants(43)) RunArea(43, kClones43, Count(kClones43), kCallees43, AH_COUNT(kCallees43), kTables43, AH_COUNT(kTables43), kRegions43,
            AH_COUNT(kRegions43), &Seed43, &Disturb43, &Args43, 4000);
    if (Wants(44)) RunArea(44, kClones44, Count(kClones44), kCallees44, AH_COUNT(kCallees44), nullptr, 0, kRegions44, AH_COUNT(kRegions44), &Seed44, &Disturb44,
            &Args44, 6000);
    if (Wants(45)) {
        ah::Group g{"area_w1b", kClones45, Count(kClones45), kCallees45, AH_COUNT(kCallees45), kTables45, AH_COUNT(kTables45),
                    g_regions45, AH_COUNT(g_regions45), &Seed45, &Disturb45, 4000};
        g.settle = &Settle45;
        g.phase_span = 5;
        g.args = &Args45;
        g.area = 45;
        ah::Run(g);
    }
    if (Wants(46)) RunArea(46, kClones46, Count(kClones46), kCallees46, AH_COUNT(kCallees46), nullptr, 0, nullptr, 0, &Seed46, &Disturb46,
            &Args46, 4000);
    if (Wants(47)) RunArea(47, kClones47, Count(kClones47), kCallees47, AH_COUNT(kCallees47), nullptr, 0, kRegions47, AH_COUNT(kRegions47),
            nullptr, nullptr, nullptr, 4000);
}

}  // namespace area_w1b

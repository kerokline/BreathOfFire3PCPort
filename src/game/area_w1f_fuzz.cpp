// BOF3X_SHADOW=area_w1f: world 1's areas 68..69 and 71..75 through the area
// round's shared harness (area_harness.h), once at start-up - one
// area_harness::Run per area, each Group setting its own area number, all
// under the one shadow name. docs/area_w1f.md section 3.
//
// The clone tables are tools/area_rows.py --clones's rows for AREA068..075
// (2026-09-28), each row read against the disassembly (every start, extent,
// call site and the tail's jump table agree); the shapes are the root table
// each function hangs from (docs/area_w1f.md section 1). The group's own
// callees (area 75's reset, phase run, presses, counters and window draw) are
// recorders here like any other callee, so each function is fuzzed alone.
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w1f.h"
#include "game/area_w1f_callees.h"
#include "game/move_script_bytes.h"
#include "hook/log.h"

namespace area_w1f {
namespace {

namespace ah = area_harness;
using U = std::uint32_t;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

#define AH_N(a) static_cast<int>(sizeof a / sizeof a[0])

// ---- area 68 ----
constexpr ah::CallSite kCalls40CF50[] = {{0xB, 0x591A80}};
constexpr ah::CallSite kCalls40CFD0[] = {{0x2C, 0x57CE10}};
constexpr ah::CallSite kCalls40D020[] = {{0x30, 0x57CE10}};
constexpr ah::CallSite kCalls40D1C0[] = {{0x2C, 0x57CE10}};
constexpr ah::CallSite kCalls40D210[] = {{0x30, 0x57CE10}};
constexpr ah::CallSite kCalls40D260[] = {{0x2C, 0x57CE10}};
constexpr ah::CallSite kCalls40D2B0[] = {{0x30, 0x57CE10}};
constexpr ah::CallSite kCalls40D300[] = {{0x0, 0x57C7C0}};
// ---- area 71 ----
constexpr ah::CallSite kCalls40D420[] = {{0x0, 0x589810}, {0x53, 0x587740}};
constexpr ah::CallSite kCalls40D480[] = {{0x0, 0x589810}};
// ---- areas 72, 73 ----
// Each init's first instruction is a jmp over eleven nops to its body
// (0x40D4C0, 0x40D590), which bof3::CloneOriginal refuses as an entry that
// looks patched (E9); the clone is the body the jmp reaches, 0x10 bytes on,
// its call sites 0x10 less than the tool's rows (0x12, 0x4D, 0x98).
constexpr ah::CallSite kCalls40D4C0[] = {{0x2, 0x5B93D2}, {0x3D, 0x5B93D2}, {0x88, 0x5720C0}};
constexpr ah::CallSite kCalls40D590[] = {{0x2, 0x5B93D2}, {0x3D, 0x5B93D2}, {0x88, 0x5720C0}};
// ---- area 74 ----
constexpr ah::CallSite kCalls40D6C0[] = {{0x53, 0x518080}, {0x77, 0x578C10}, {0x8B, 0x57C140}, {0x9E, 0x57C110}, {0xAC, 0x57C0F0}, {0xC1, 0x587740}};
constexpr ah::CallSite kCalls40D7A0[] = {{0x0, 0x57C7C0}};
// ---- area 75 ----
constexpr ah::CallSite kCalls40D7C0[] = {{0x2, 0x531F90}};
constexpr ah::CallSite kCalls40D7D0[] = {{0x0, 0x57C7C0}};
constexpr ah::CallSite kCalls40D8A0[] = {{0x17, 0x57CD90}, {0x2D, 0x57A010}};
constexpr ah::CallSite kCalls40DA80[] = {{0x5, 0x587900}};
constexpr ah::CallSite kCalls40DAE0[] = {{0xE, 0x587740}};
constexpr ah::CallSite kCalls40DB90[] = {{0x24, 0x495040}, {0x62, 0x57C0F0}, {0x78, 0x594E00}, {0x8B, 0x57C140}, {0xC3, 0x495040}, {0x107, 0x40E180}, {0x14E, 0x589330}, {0x202, 0x4976D0}, {0x26B, 0x589330}, {0x305, 0x40E180}, {0x33E, 0x40E180}, {0x379, 0x589330}, {0x3A4, 0x589330}, {0x3B7, 0x587740}, {0x3C9, 0x40E1C0}, {0x3E1, 0x40E1C0}, {0x41A, 0x495040}, {0x421, 0x587BE0}, {0x446, 0x587B40}, {0x44D, 0x587910}, {0x454, 0x4976D0}, {0x47D, 0x587A00}, {0x49E, 0x57C110}, {0x4B4, 0x594E00}, {0x4E1, 0x495040}, {0x4F0, 0x57C7A0}};
constexpr ah::JumpTable kTables40DB90[] = {{0x1E, 0x50C, 24}};
constexpr ah::CallSite kCalls40E120[] = {{0x7, 0x57C140}, {0x2A, 0x57C7C0}};
constexpr ah::CallSite kCalls40E160[] = {{0x0, 0x57C7C0}};
constexpr ah::CallSite kCalls40E1C0[] = {{0x11, 0x40E5F0}};
constexpr ah::CallSite kCalls40E1E0[] = {{0x1A, 0x587740}, {0x52, 0x589330}, {0x7A, 0x5B93D2}, {0xAD, 0x5B93D2}, {0x125, 0x589330}, {0x13B, 0x587740}};
constexpr ah::CallSite kCalls40E330[] = {{0x12, 0x40E480}, {0x17, 0x40E520}, {0x1E, 0x40E480}, {0x23, 0x40E5A0}};
constexpr ah::CallSite kCalls40E3C0[] = {{0x7, 0x57C0F0}};
constexpr ah::CallSite kCalls40E3E0[] = {{0x7, 0x57C110}, {0x21, 0x57C0F0}, {0x81, 0x531F90}};
constexpr ah::CallSite kCalls40E480[] = {{0x2F, 0x587740}, {0x37, 0x5B93D2}, {0x90, 0x5893A0}};
constexpr ah::CallSite kCalls40E520[] = {{0xE, 0x587740}, {0x16, 0x5B93D2}, {0x60, 0x5893A0}};
constexpr ah::CallSite kCalls40E5A0[] = {{0x3C, 0x5893A0}};
constexpr ah::CallSite kCalls40E5F0[] = {{0x84, 0x40E750}, {0xA8, 0x5B9380}, {0xC4, 0x516B30}, {0x108, 0x40E750}, {0x12C, 0x5B9380}, {0x147, 0x516B30}};
constexpr ah::CallSite kCalls40E750[] = {{0x62, 0x5A77C0}, {0x6B, 0x461E50}, {0x89, 0x5A75D0}, {0x91, 0x5A7780}, {0x170, 0x5A79E0}, {0x191, 0x461E50}, {0x1C1, 0x5A75D0}, {0x1C9, 0x5A7780}, {0x249, 0x5A79E0}, {0x25A, 0x461E50}, {0x266, 0x5A75D0}, {0x26E, 0x5A7780}, {0x2F5, 0x5A79E0}, {0x306, 0x461E50}, {0x312, 0x5A75D0}, {0x31A, 0x5A7780}, {0x3AE, 0x5A79E0}, {0x3C2, 0x461E50}, {0x400, 0x5A77C0}, {0x409, 0x461E50}, {0x42F, 0x57D420}};

#define W1F_CLONE(name, base, size, calls, n, tables, nt, ret, shape) \
    {#name, base, size, calls, n, nullptr, 0, tables, nt, reinterpret_cast<const void*>(&::name), ret, false, ah::Shape::shape}

const ah::Clone kClones68[] = {
    W1F_CLONE(Area68_ChoiceMessageA, 0x40CEF0, 0x2E, nullptr, 0, nullptr, 0, 0, kChoice),
    W1F_CLONE(Area68_ChoiceMessageB, 0x40CF20, 0x2E, nullptr, 0, nullptr, 0, 0, kChoice),
    W1F_CLONE(Area68_ChoiceAskCount, 0x40CF50, 0x43, kCalls40CF50, AH_N(kCalls40CF50), nullptr, 0, 0, kChoice),
    W1F_CLONE(Area68_ChoiceAnswer84, 0x40CFA0, 0x24, nullptr, 0, nullptr, 0, 0, kChoice),
    W1F_CLONE(Area68_SpawnKind4AtMember1, 0x40CFD0, 0x42, kCalls40CFD0, AH_N(kCalls40CFD0), nullptr, 0, 0, kHandler),
    W1F_CLONE(Area68_SpawnKind4AtMember2, 0x40D020, 0x46, kCalls40D020, AH_N(kCalls40D020), nullptr, 0, 0, kHandler),
    W1F_CLONE(Area68_GlideRunA, 0x40D070, 0x12, nullptr, 0, nullptr, 0, 0, kHandler),
    W1F_CLONE(Area68_GlideBegin10, 0x40D090, 0x22, nullptr, 0, nullptr, 0, 0, kState),
    W1F_CLONE(Area68_GlideStepA, 0x40D0C0, 0x62, nullptr, 0, nullptr, 0, 0, kState),
    W1F_CLONE(Area68_GlideRunB, 0x40D130, 0x12, nullptr, 0, nullptr, 0, 0, kHandler),
    W1F_CLONE(Area68_GlideStepB, 0x40D150, 0x62, nullptr, 0, nullptr, 0, 0, kState),
    W1F_CLONE(Area68_SpawnKind1AtMember1, 0x40D1C0, 0x42, kCalls40D1C0, AH_N(kCalls40D1C0), nullptr, 0, 0, kHandler),
    W1F_CLONE(Area68_SpawnKind1AtMember2, 0x40D210, 0x46, kCalls40D210, AH_N(kCalls40D210), nullptr, 0, 0, kHandler),
    W1F_CLONE(Area68_SpawnKind3AtMember1, 0x40D260, 0x42, kCalls40D260, AH_N(kCalls40D260), nullptr, 0, 0, kHandler),
    W1F_CLONE(Area68_SpawnKind3AtMember2, 0x40D2B0, 0x46, kCalls40D2B0, AH_N(kCalls40D2B0), nullptr, 0, 0, kHandler),
    W1F_CLONE(Area68_Trigger25, 0x40D300, 0x5E, kCalls40D300, AH_N(kCalls40D300), nullptr, 0, 0xFF, kCallee),
};
enum : unsigned {
    k68MessageA, k68MessageB, k68AskCount, k68Answer84, k68Spawn4M1, k68Spawn4M2, k68RunA, k68Begin10, k68StepA, k68RunB,
    k68StepB, k68Spawn1M1, k68Spawn1M2, k68Spawn3M1, k68Spawn3M2, k68Trigger25
};
static_assert(k68Trigger25 + 1 == sizeof kClones68 / sizeof kClones68[0], "area 68's seeding indices");
const ah::Clone kClones69[] = {
    W1F_CLONE(Area69_GlideRun, 0x40D360, 0x12, nullptr, 0, nullptr, 0, 0, kHandler),
    W1F_CLONE(Area69_GlideBegin40, 0x40D380, 0x22, nullptr, 0, nullptr, 0, 0, kState),
    W1F_CLONE(Area69_GlideStep, 0x40D3B0, 0x62, nullptr, 0, nullptr, 0, 0, kState),
};
enum : unsigned { k69Run, k69Begin40, k69Step };
const ah::Clone kClones71[] = {
    W1F_CLONE(Area71_SpawnEffect3C, 0x40D420, 0x5A, kCalls40D420, AH_N(kCalls40D420), nullptr, 0, 0, kHandler),
    W1F_CLONE(Area71_SpawnEffect25, 0x40D480, 0x2A, kCalls40D480, AH_N(kCalls40D480), nullptr, 0, 0, kHandler),
};
const ah::Clone kClones72[] = {
    W1F_CLONE(Area72_PlaceRandomObject, 0x40D4C0, 0xBB, kCalls40D4C0, AH_N(kCalls40D4C0), nullptr, 0, 0, kInit),
};
const ah::Clone kClones73[] = {
    W1F_CLONE(Area73_PlaceRandomObject, 0x40D590, 0xBB, kCalls40D590, AH_N(kCalls40D590), nullptr, 0, 0, kInit),
};
const ah::Clone kClones74[] = {
    W1F_CLONE(Area74_ChoiceAsk92, 0x40D650, 0x3E, nullptr, 0, nullptr, 0, 0, kChoice),
    W1F_CLONE(Area74_ChoiceAnswer94, 0x40D690, 0x24, nullptr, 0, nullptr, 0, 0, kChoice),
    W1F_CLONE(Area74_PushToggleRow7, 0x40D6C0, 0xCC, kCalls40D6C0, AH_N(kCalls40D6C0), nullptr, 0, 0, kHandler),
    W1F_CLONE(Area74_ClearMemberBit0, 0x40D790, 0xD, nullptr, 0, nullptr, 0, 0, kHandler),
    W1F_CLONE(Area74_Trigger28, 0x40D7A0, 0x16, kCalls40D7A0, AH_N(kCalls40D7A0), nullptr, 0, 0xFF, kCallee),
};
enum : unsigned { k74Ask92, k74Answer94, k74Push, k74ClearBit, k74Trigger28 };
const ah::Clone kClones75[] = {
    W1F_CLONE(Area75_DropIn0, 0x40D7C0, 0x9, kCalls40D7C0, AH_N(kCalls40D7C0), nullptr, 0, 0, kHandler),
    W1F_CLONE(Area75_ArmTail30, 0x40D7D0, 0x14, kCalls40D7D0, AH_N(kCalls40D7D0), nullptr, 0, 0, kHandler),
    W1F_CLONE(Area75_MarkObjectB, 0x40D7F0, 0x24, nullptr, 0, nullptr, 0, 0, kHandler),
    W1F_CLONE(Area75_MarkObjectA, 0x40D820, 0x24, nullptr, 0, nullptr, 0, 0, kHandler),
    W1F_CLONE(Area75_ShowEffects, 0x40D850, 0x26, nullptr, 0, nullptr, 0, 0, kHandler),
    W1F_CLONE(Area75_PoseObject5, 0x40D880, 0x1F, nullptr, 0, nullptr, 0, 0, kHandler),
    W1F_CLONE(Area75_SpawnFive, 0x40D8A0, 0x80, kCalls40D8A0, AH_N(kCalls40D8A0), nullptr, 0, 0, kHandler),
    W1F_CLONE(Area75_FlyRun, 0x40D920, 0x12, nullptr, 0, nullptr, 0, 0, kHandler),
    W1F_CLONE(Area75_FlyBegin, 0x40D940, 0x34, nullptr, 0, nullptr, 0, 0, kState),
    W1F_CLONE(Area75_FlyStep, 0x40D980, 0x6B, nullptr, 0, nullptr, 0, 0, kState),
    W1F_CLONE(Area75_DrainEffects, 0x40D9F0, 0x64, nullptr, 0, nullptr, 0, 0, kHandler),
    W1F_CLONE(Area75_FollowRun, 0x40DA60, 0x12, nullptr, 0, nullptr, 0, 0, kHandler),
    W1F_CLONE(Area75_FollowBegin, 0x40DA80, 0x22, kCalls40DA80, AH_N(kCalls40DA80), nullptr, 0, 0, kState),
    W1F_CLONE(Area75_FollowStep, 0x40DAB0, 0x2E, nullptr, 0, nullptr, 0, 0, kState),
    W1F_CLONE(Area75_FollowKind2Z, 0x40DAE0, 0x31, kCalls40DAE0, AH_N(kCalls40DAE0), nullptr, 0, 0, kHandler),
    W1F_CLONE(Area75_ChoiceStart14, 0x40DB20, 0x2B, nullptr, 0, nullptr, 0, 0, kChoice),
    W1F_CLONE(Area75_ChoiceStartF, 0x40DB50, 0x34, nullptr, 0, nullptr, 0, 0, kChoice),
    W1F_CLONE(Area75_TailPresses, 0x40DB90, 0x590, kCalls40DB90, AH_N(kCalls40DB90), kTables40DB90, AH_N(kTables40DB90), 0, kTail),
    W1F_CLONE(Area75_StepHook, 0x40E120, 0x3C, kCalls40E120, AH_N(kCalls40E120), nullptr, 0, 0xFF, kHook),
    W1F_CLONE(Area75_Trigger41, 0x40E160, 0x1D, kCalls40E160, AH_N(kCalls40E160), nullptr, 0, 0xFF, kCallee),
    W1F_CLONE(Area75_ResetPresses, 0x40E180, 0x32, nullptr, 0, nullptr, 0, 0, kCallee),
    W1F_CLONE(Area75_PhaseRun, 0x40E1C0, 0x16, kCalls40E1C0, AH_N(kCalls40E1C0), nullptr, 0, 0, kCallee),
    W1F_CLONE(Area75_PhaseDeal, 0x40E1E0, 0x14C, kCalls40E1E0, AH_N(kCalls40E1E0), nullptr, 0, 0, kState),
    W1F_CLONE(Area75_PhasePlay, 0x40E330, 0x8D, kCalls40E330, AH_N(kCalls40E330), nullptr, 0, 0, kState),
    W1F_CLONE(Area75_PhaseMissed, 0x40E3C0, 0x17, kCalls40E3C0, AH_N(kCalls40E3C0), nullptr, 0, 0, kState),
    W1F_CLONE(Area75_PhaseReached, 0x40E3E0, 0x92, kCalls40E3E0, AH_N(kCalls40E3E0), nullptr, 0, 0, kState),
    W1F_CLONE(Area75_OtherPress, 0x40E480, 0x96, kCalls40E480, AH_N(kCalls40E480), nullptr, 0, 0, kCallee),
    W1F_CLONE(Area75_PlayerPress, 0x40E520, 0x74, kCalls40E520, AH_N(kCalls40E520), nullptr, 0, 0, kCallee),
    W1F_CLONE(Area75_EarlyPress, 0x40E5A0, 0x48, kCalls40E5A0, AH_N(kCalls40E5A0), nullptr, 0, 0, kCallee),
    W1F_CLONE(Area75_DrawCounters, 0x40E5F0, 0x155, kCalls40E5F0, AH_N(kCalls40E5F0), nullptr, 0, 0, kCallee),
    W1F_CLONE(Area75_DrawWindow, 0x40E750, 0x43F, kCalls40E750, AH_N(kCalls40E750), nullptr, 0, 0, kCallee),
};
enum : unsigned {
    k75DropIn, k75ArmTail, k75MarkB, k75MarkA, k75ShowEffects, k75Pose5, k75SpawnFive, k75FlyRun, k75FlyBegin, k75FlyStep,
    k75Drain, k75FollowRun, k75FollowBegin, k75FollowStep, k75FollowKind2Z, k75Choice14, k75ChoiceF, k75Tail, k75Step,
    k75Trigger41, k75Reset, k75PhaseRun, k75Deal, k75Play, k75Missed, k75Reached, k75OtherPress, k75PlayerPress,
    k75EarlyPress, k75DrawCounters, k75DrawWindow
};
static_assert(k75DrawWindow + 1 == sizeof kClones75 / sizeof kClones75[0], "area 75's seeding indices");
#undef W1F_CLONE
#undef AH_N

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
unsigned char& B(U address) { return *ah::Mem(address); }

// --- the fuzz's own memory: a packet buffer for the window draw ----------------

constexpr unsigned kPacketBytes = 0x200;
alignas(16) unsigned char g_packets[kPacketBytes];
// The exe's own weights of areas 72 and 73, put back two rounds in three (the
// shipped weights sum to 0x40, so the "none chosen" path needs others).
unsigned char g_weights72[8], g_weights73[8];

bool InPackets(U p, unsigned n) { return p >= Key(g_packets) && p + n <= Key(g_packets) + kPacketBytes; }
// The packet cursor moved on by `size`, kept inside the fuzz's buffer.
void Advance(U size) {
    U next = Key(Gfx_PacketNext) + size;
    if (!InPackets(next, 0x50)) next = Key(g_packets) + (ah::Noise() % 4) * 4;
    Gfx_PacketNext = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(next));
}
void Scribble(U at, unsigned n) {
    if (!InPackets(at, n)) return;
    unsigned char* const p = ah::Mem(at);
    for (unsigned i = 0; i < n; i += 4) SetLong(p + i, static_cast<std::int32_t>(ah::Noise()));
}

// Records the areas' pointers may name: a field object (0..29), a party
// record, one of the four party objects.
unsigned char* AnyRecord(U v) {
    switch (v % 4) {
    case 0: return ah::Mem(at::kSpriteObjectsExtra + (v >> 2) % 4 * 0xA4);
    case 1: return ah::PartyOf(static_cast<unsigned char>(v >> 2));
    default: return ah::Object(v >> 2);
    }
}
// The running object as the harness puts it: a party record or one of the
// first four field objects.
unsigned char* RunningRecord(U v) { return v & 1 ? ah::PartyOf(static_cast<unsigned char>(v >> 1)) : ah::Object((v >> 1) % 4); }

// --- the effects: the callees that move what the caller reads after, and the
// answers the recorders cannot give ------------------------------------------

constexpr U kAll = 0xFFFFFFFFu, kU8 = 0xFFu, kU16 = 0xFFFFu;

// Gfx_CommitPrim moves the cursor by the size; one call in five by another
// amount (louder than the real one, so a cursor cached across it shows).
U CommitEffect(const U* a, U answer) {
    Advance(ah::Noise() % 5 ? a[1] & 0xFF : (a[1] & 0xFF) + 4 * (1 + ah::Noise() % 4));
    return answer;
}
// The primitive setters write the primitive's bytes, so a store the caller
// makes before the call (where the original makes it after) shows.
U PolyEffect(const U* a, U answer) {
    Scribble(a[0], 0x48);
    if (InPackets(a[0], 8)) ah::Mem(a[0])[7] = 0x2C;
    return answer;
}
U DrawModeEffect(const U* a, U answer) { Scribble(a[0], 0xC); return answer; }
U SemiEffect(const U* a, U answer) {
    if (InPackets(a[0], 8)) ah::Mem(a[0])[7] = static_cast<unsigned char>(a[1] ? ah::Mem(a[0])[7] | 2 : ah::Mem(a[0])[7] & 0xFD);
    return answer;
}
// Louder than the real callees, each only half the time (from Noise): the
// running object after Sprite_EnsureAnimation (the tail and the deal write its
// +0x2A after) and after EventOp_0x (area 75's spawns write the new one);
// the active member after Field_ObjectBlockedAhead and Effect_FindFree; the
// focus object after ScriptFlags_Set40 (area 68's trigger reads it four
// times); area 75's cells after the calls its functions read them again
// after.
void MoveCell(U n);
U MovesCurrent(const U*, U answer) {
    const U n = ah::Noise();
    if (n & 1) Sprite_Current = RunningRecord(n >> 8);
    if (n & 2) MoveCell(n >> 12);
    return answer;
}
U MovesMember(const U*, U answer) {
    const U n = ah::Noise();
    if (n & 1) ah::SetPointer(at::kActiveMember, AnyRecord(n >> 8));
    return answer;
}
U EventOpEffect(const U*, U answer) {
    const U n = ah::Noise();
    if (n & 1) Sprite_Current = n & 2 ? ah::Object(n >> 8) : RunningRecord(n >> 8);
    if (n & 4) ah::SetPointer(at::kActiveMember, AnyRecord(n >> 12));
    return answer;
}
U Set40Effect(const U*, U answer) {
    const U n = ah::Noise();
    if (n & 1) ah::SetPointer(at::kFocusObject, ah::Object(n >> 8));
    return answer;
}
// Area 75's cells, kept to what indexes a table or a record.
void MoveCell(U n) {
    switch ((n >> 4) % 10) {
    case 0: B(at::kObjectA) = static_cast<unsigned char>((n >> 8) % 30); break;
    case 1: B(at::kObjectB) = static_cast<unsigned char>((n >> 8) % 30); break;
    case 2: B(at::kOtherEffect) = static_cast<unsigned char>((n >> 8) % 4); break;
    case 3: B(at::kPlayerEffect) = static_cast<unsigned char>((n >> 8) % 4); break;
    case 4: B(at::kPressFlags) = static_cast<unsigned char>(n >> 8); break;
    case 5: B(at::kMove) = static_cast<unsigned char>((n >> 8) % 4); break;
    case 6: B(at::kList) = static_cast<unsigned char>((n >> 8) % 3); break;
    case 7: SetWord(ah::Mem(at::kPlayerCounter), n >> 12); break;
    case 8: SetWord(ah::Mem(at::kOtherCounter), n >> 12); break;
    default: B(at::kListPos) = static_cast<unsigned char>((n >> 8) % 8); break;
    }
}
U MovesCells(const U*, U answer) {
    const U n = ah::Noise();
    if (n & 1) MoveCell(n);
    return answer;
}
// Sprite_ScriptTick: the two press counts the callers bump after it.
U TickEffect(const U*, U answer) {
    const U n = ah::Noise();
    if (n & 1) B(n & 2 ? at::kEarlyPresses : at::kIdleFrames) = static_cast<unsigned char>(n >> 8);
    return answer;
}
// Area75_ResetPresses: object A and the flags byte the tail reads after it.
U ResetEffect(const U*, U answer) {
    const U n = ah::Noise();
    if (n & 1) B(at::kObjectA) = static_cast<unsigned char>((n >> 8) % 30);
    if (n & 2) B(at::kPressFlags) = static_cast<unsigned char>(n >> 16);
    return answer;
}
// Area75_PhaseRun: the tail state its caller reads again (the real phases 3
// and 4 set it to 0x1E and 0).
U PhaseRunEffect(const U*, U answer) {
    static const unsigned char kStates[] = {0x1E, 0, 6, 0xE, 0x15, 0xF};
    const U n = ah::Noise();
    if (n & 1) B(at::kTailState) = kStates[(n >> 8) % 6];
    return answer;
}
// Msg_OpenScript: the flags byte the tail's state 9 reads after it.
U MsgEffect(const U*, U answer) {
    const U n = ah::Noise();
    if (n & 1) B(at::kPressFlags) = static_cast<unsigned char>(n >> 8);
    return answer;
}
// Inventory_CountUsed: a count around the choice's 0xF.
U CountEffect(const U*, U answer) {
    static const U kCounts[] = {0xE, 0xF, 0x10, 0, 0xFF, 0x8F};
    const U n = ah::Noise();
    return (answer & 0xFFFFFF00u) | (n % 7 == 0 ? (n >> 8) & 0xFF : kCounts[(n >> 4) % 6]);
}
// Sprite_FindFree / Effect_FindFree: none a third of the time; the effect
// slots stay inside the group's four records.
U SpriteSlotEffect(const U*, U answer) {
    const U n = ah::Noise();
    return (answer & 0xFFFFFF00u) | (n % 3 == 0 ? 0xFFu : (n >> 4) % 30);
}
U EffectSlotEffect(const U* a, U answer) {
    MovesMember(a, answer);
    const U n = ah::Noise();
    return (answer & 0xFFFFFF00u) | (n % 3 == 0 ? 0xFFu : (n >> 4) % 4);
}

#define W1F_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
#define W1F_THEIRS(name) #name, KeyOf(name), KeyOf(name)
const ah::Callee kCallees[] = {
    // the group's own, called directly
    // kGarbage, not kPhase, where an effect is listed: the harness's kPhase
    // recorder returns before it runs a callee's effect
    {W1F_OURS(Area75_ResetPresses), 0, {}, ah::Answer::kGarbage, 0, 0, {}, &ResetEffect},
    {W1F_OURS(Area75_PhaseRun), 0, {}, ah::Answer::kGarbage, 0, 0, {}, &PhaseRunEffect},
    {W1F_OURS(Area75_DrawCounters), 0, {}, ah::Answer::kPhase, 0, 0},
    {W1F_OURS(Area75_OtherPress), 0, {}, ah::Answer::kGarbage, 0, 0, {}, &MovesCells},
    {W1F_OURS(Area75_PlayerPress), 0, {}, ah::Answer::kGarbage, 0, 0, {}, &MovesCells},
    {W1F_OURS(Area75_EarlyPress), 0, {}, ah::Answer::kPhase, 0, 0},
    {W1F_OURS(Area75_DrawWindow), 5, {kAll, kAll, kAll, kAll, kAll}, ah::Answer::kGarbage, 0, 0},
    // named, beyond the standard set
    {W1F_OURS(ScriptFlags_Set40), 0, {}, ah::Answer::kGarbage, 0, 0, {}, &Set40Effect},
    {W1F_OURS(ScriptFlags_Clear40), 0, {}, ah::Answer::kGarbage, 0, 0},
    {W1F_OURS(Effect_FindFree), 0, {}, ah::Answer::kGarbage, 0, 0, {}, &EffectSlotEffect},
    {W1F_OURS(Sprite_FindFree), 0, {}, ah::Answer::kGarbage, 0, 0, {}, &SpriteSlotEffect},
    {W1F_OURS(EventOp_0x), 1, {kAll}, ah::Answer::kGarbage, 0, 0, {}, &EventOpEffect},
    {W1F_OURS(Transition_Start), 1, {kU8}, ah::Answer::kGarbage, 0, 0},
    {W1F_OURS(Music_FadeOut), 1, {kAll}, ah::Answer::kGarbage, 0, 0},
    {W1F_OURS(Sound_LoadStream), 1, {kAll}, ah::Answer::kGarbage, 0, 0},
    {W1F_OURS(Sound_StreamDone), 0, {}, ah::Answer::kFlag, 0, 0},
    {W1F_OURS(Menu_DrawOutline), 5, {kAll, kAll, kAll, kAll, kAll}, ah::Answer::kGarbage, 0, 0},
    {W1F_OURS(Gfx_CommitPrim), 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &CommitEffect},
    {W1F_THEIRS(Crt_sprintf), 4, {kAll, kAll, kAll, kAll}, ah::Answer::kGarbage, 0, 0},
    {W1F_THEIRS(MoveCmd_Move), 2, {kAll, kU8}, ah::Answer::kGarbage, 0, 0},
    // standard ones listed again: what the callee reads, or what the caller reads after
    {W1F_OURS(Text_DrawAt), 5, {kAll, kAll, kU8, kAll, kAll}, ah::Answer::kGarbage, 0, 0},   // the colour's upper bytes are the original's stack
    {W1F_OURS(Inventory_CountUsed), 1, {kAll}, ah::Answer::kGarbage, 0, 0, {}, &CountEffect},
    {W1F_OURS(Sprite_EnsureAnimation), 1, {kU8}, ah::Answer::kFlag, 0, 0, {}, &MovesCurrent},
    {W1F_OURS(Sprite_ScriptTick), 0, {}, ah::Answer::kFlag, 0, 0, {}, &TickEffect},
    {W1F_OURS(Field_ObjectBlockedAhead), 1, {kAll}, ah::Answer::kFlag, 0, 0, {}, &MovesMember},
    {W1F_OURS(Msg_OpenScript), 1, {kU16}, ah::Answer::kGarbage, 0, 0, {}, &MsgEffect},
    {W1F_OURS(Sound_PlayEffect), 1, {kU16}, ah::Answer::kGarbage, 0, 0, {}, &MovesCells},
    {W1F_OURS(Flags_Set), 2, {kAll, kU8}, ah::Answer::kGarbage, 0, 0, {}, &MovesCells},
    {W1F_THEIRS(Rand), 0, {}, ah::Answer::kRand, 0, 0, {}, &MovesCells},
    {W1F_THEIRS(Effect_Spawn), 5, {kU8, kU8, kU8, kU16, kU16}, ah::Answer::kByte, 0xFE, 0x02},
    {W1F_OURS(Gpu_SetPolyFT4), 1, {kAll}, ah::Answer::kGarbage, 0, 0, {}, &PolyEffect},
    {W1F_OURS(Gpu_SetSemiTrans), 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &SemiEffect},
    {W1F_OURS(Gpu_SetDrawMode), 5, {kAll, kAll, kAll, kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &DrawModeEffect},
};
#undef W1F_OURS
#undef W1F_THEIRS

const ah::DataTable kTables68[] = {{at::kArea68GlideStatesA, 2}, {at::kArea68GlideStatesB, 2}};
const ah::DataTable kTables69[] = {{at::kArea69GlideStates, 2}};
const ah::DataTable kTables75[] = {{at::kArea75FlyStates, 2}, {at::kArea75FollowStates, 2}, {at::kArea75Phases, at::kArea75PhaseCount}};

// Beyond the field frame: the effect records (slots 0..3), the active member,
// script object and focus object pointers, the mark, area 75's cells,
// Input_Pressed, Draw_PassFlags, the wait word, the music byte, the packet
// cursor and the fuzz's packet buffer, areas 72 and 73's weights (.data).
ah::Region g_regions[] = {
    {at::kEffectObjects, 4 * at::kEffectStride},
    {at::kActiveMember, 4},
    {at::kScriptObject, 4},
    {at::kFocusObject, 4},
    {at::kAnswerMark, 1},
    {at::kArea75Cells, at::kArea75CellBytes},
    {at::kInputPressed, 2},
    {at::kPassFlags, 1},
    {at::kWaitWord, 2},
    {at::kMusicCurrent, 1},
    {at::kPacketNext, 4},
    {0, kPacketBytes},   // g_packets (set at start-up)
    {at::kArea72Weights, 8},
    {at::kArea73Weights, 8},
};
constexpr unsigned kPacketRegion = 11;

// Every round: the pointers the areas follow put back inside the regions, and
// every cell of area 75's that indexes a table or a record kept inside it.
void Common() {
    ah::SetPointer(at::kActiveMember, AnyRecord(ah::Next()));
    ah::SetPointer(at::kScriptObject, ah::Next() & 1 ? ah::Object(ah::Next()) : ah::PartyOf(static_cast<unsigned char>(ah::Next())));
    ah::SetPointer(at::kFocusObject, ah::Object(ah::Next()));
    B(at::kObjectA) = static_cast<unsigned char>(ah::Next() % 30);
    B(at::kObjectB) = static_cast<unsigned char>(ah::Next() % 30);
    B(at::kOtherEffect) = static_cast<unsigned char>(ah::Next() % 4);
    B(at::kPlayerEffect) = static_cast<unsigned char>(ah::Next() % 4);
    B(at::kList) = static_cast<unsigned char>(ah::Next() % 3);
    B(at::kPhase) = static_cast<unsigned char>(ah::Next() % at::kArea75PhaseCount);
    Gfx_PacketNext = g_packets + (ah::Next() % 4) * 4;
    if (ah::Often()) std::memcpy(ah::Mem(at::kArea72Weights), g_weights72, sizeof g_weights72);
    if (ah::Often()) std::memcpy(ah::Mem(at::kArea73Weights), g_weights73, sizeof g_weights73);
}

// The group's cells, moved by the harness's disturbance about one call in
// 24 - drawn only from h (area_harness.h: a group disturb never draws Next).
void Disturb(U h) {
    const auto v = static_cast<unsigned char>(h >> 20);
    switch ((h >> 8) % 8) {
    case 0: B(at::kTailState) = static_cast<unsigned char>(h & 0x100 ? v % 0x24 : v); break;
    case 1: B(at::kCounter0) = v; break;
    case 2: ah::SetPointer(at::kActiveMember, AnyRecord(h >> 16)); break;
    case 3: ah::SetPointer(at::kScriptObject, ah::Object(h >> 16)); break;
    case 4: MoveCell(h >> 4); break;
    case 5: SetWord(ah::Mem(h & 0x100 ? at::kPlayerCounter : at::kOtherCounter), h >> 12); break;
    case 6: B(at::kInputPressed) = v; break;
    default: SetWord(ah::Mem(at::kWaitWord), h & 0x100 ? 0 : v); break;
    }
}

// A choice answer: each value a handler tests, its neighbours, a negative
// byte (the table choices read it signed), anything.
void SeedAnswer() {
    if (ah::Often()) B(at::kChoiceAnswer) = static_cast<unsigned char>(AH_PICK(0, 1, 2, 0, 1, 0xFF, 0x80, 0x7F, 3));
}
// The running object's state byte for the two-state dispatchers (a state of
// 2 or more aborts ours and faults the original: never seeded).
void SeedState() { Sprite_Current[4] = static_cast<unsigned char>(ah::Next() & 1); }
// An object trigger is called (a field object, 0x904030).
void ArgsTrigger(U* a) {
    a[0] = Key(ah::Object(a[0]));
    a[1] = at::kStoryFlags;
}

// ---- area 68 ----
void Seed68(unsigned k) {
    Common();
    switch (k) {
    case k68MessageA: case k68MessageB: case k68AskCount: case k68Answer84: SeedAnswer(); break;
    case k68RunA: case k68RunB: SeedState(); break;
    case k68StepA: case k68StepB:
        if (ah::Often()) Sprite_Current[0xA] = static_cast<unsigned char>(AH_PICK(0, 0, 1, 0x10, 0x11, 0x0F, 0x80));
        break;
    case k68Spawn4M1: case k68Spawn4M2: case k68Spawn1M1: case k68Spawn1M2: case k68Spawn3M1: case k68Spawn3M2:
        for (unsigned m = 1; m < 3; ++m)
            if (ah::Often()) B(at::kPartyList0 + m) = static_cast<unsigned char>(ah::Next() % 12);
        break;
    default: break;
    }
}
void Args68(unsigned k, U* a) {
    if (k == k68Trigger25) ArgsTrigger(a);
}

// ---- area 69 ----
void Seed69(unsigned k) {
    Common();
    if (k == k69Run) SeedState();
    if (k == k69Step && ah::Often()) Sprite_Current[0xA] = static_cast<unsigned char>(AH_PICK(0, 0, 1, 0x40, 0x41, 0x0F, 0x80));
}

// ---- area 71 ----
void Seed71(unsigned) { Common(); }

// ---- areas 72, 73 ----
// Two rounds in three the shipped weights (Common); else the harness's random
// bytes, so the walk also runs off the table's end ("none chosen").
void Seed72(unsigned) { Common(); }

// ---- area 74 ----
void Seed74(unsigned k) {
    Common();
    switch (k) {
    case k74Ask92:
    case k74Answer94:
        SeedAnswer();
        if (k == k74Ask92 && ah::Half()) B(at::kFlagByte9C) = static_cast<unsigned char>(B(at::kFlagByte9C) ^ 0x20);
        break;
    case k74Push:
        // each gate passed four times in five, so the move is reached often
        if (ah::Next() % 5) B(at::kLeaderByte89) = static_cast<unsigned char>(AH_PICK(2, 2, 2, 1, 3, 0x82));
        if (ah::Next() % 5) Sprite_Current[9] = 0;
        if (ah::Next() % 5) B(at::kLeaderDir) = static_cast<unsigned char>((B(at::kLeaderDir) & 0xF8) | AH_PICK(5, 1, 5, 1, 5, 1, 0, 3, 4, 6));
        break;
    default: break;
    }
}
void Args74(unsigned k, U* a) {
    if (k == k74Trigger28) ArgsTrigger(a);
}

// ---- area 75 ----

// The two counters apart by `gap` (either way round), each else any word
// inside 0..0x5DC or anything.
void SeedGap(U gap) {
    const U base = ah::Next() % 0x5DD;
    const U other = ah::Half() ? base + gap : base - gap;
    const bool swap = ah::Half();
    SetWord(ah::Mem(swap ? at::kPlayerCounter : at::kOtherCounter), base);
    SetWord(ah::Mem(swap ? at::kOtherCounter : at::kPlayerCounter), other);
}
void SeedCounterAt(U counter, U edge) {
    SetWord(ah::Mem(counter), ah::Often() ? edge + AH_PICK(0, 0, 1, 0xFFFFFFFFu, 2) : ah::Next());
}

void SeedTail() {
    // each state the byte table reaches, the out-of-table and out-of-range
    // sides; then the cell that state waits on, at its value two times in
    // three and beside it otherwise (step-paired)
    static const U kStates[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xA, 0xB, 0xC, 0xD, 0xF, 0x14, 0x15, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23,
                                0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xA, 0xB, 0xC, 0xD, 0xF, 0x14, 0x15, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23,
                                0xE, 0x10, 0x13, 0x16, 0x1D, 0x24, 0x7F, 0x80, 0xFF, 0xFA};
    const auto state = static_cast<unsigned char>(ah::Pick(kStates, sizeof kStates / sizeof kStates[0]));
    B(at::kTailState) = state;
    const bool on = ah::Often();
    switch (state) {
    case 1: case 3: case 4: case 0x20: case 0x23:
        SetWord(ah::Mem(at::kWaitWord), on ? 0 : AH_PICK(1, 0x100, 0xFFFF));
        break;
    case 5: B(at::kCounter0) = static_cast<unsigned char>(on ? 3 : AH_PICK(2, 4, 0x83)); break;
    case 7: B(at::kCounter0) = static_cast<unsigned char>(on ? 6 : AH_PICK(5, 7, 0x86)); break;
    case 9: B(at::kCounter0) = static_cast<unsigned char>(on ? 9 : AH_PICK(8, 0xA, 0x89)); break;
    case 0xD: B(at::kCounter0) = static_cast<unsigned char>(on ? 0xC : AH_PICK(0xB, 0xD, 0x8C)); break;
    case 0x1F: B(at::kCounter0) = static_cast<unsigned char>(on ? 0x1A : AH_PICK(0x19, 0x1B, 0x9A)); break;
    case 6: SeedCounterAt(at::kOtherCounter, 0x578); break;
    case 8: SeedCounterAt(at::kPlayerCounter, 0x578); break;
    case 0xC: SeedCounterAt(at::kOtherCounter, 0x47E); break;
    case 0xB: SeedGap(AH_PICK(0x96, 0x97, 0x95, 0x200, 0)); break;
    case 0xA: case 0x14: case 0x21:
        Field_Request = static_cast<unsigned char>(on ? AH_PICK(0, 1, 5, 3) : 2);
        break;
    case 0xF: SetWord(ah::Mem(at::kTailTimer), on ? 1 : AH_PICK(0, 2, 0x101)); break;
    default: break;
    }
}

// The rhythm row and the move's frames on a press of the table (half the time).
void SeedRhythm() {
    B(at::kRow) = static_cast<unsigned char>(ah::Often() ? ah::Next() % 2 : ah::Next() % 16);
    if (!ah::Half()) return;
    const unsigned char* const row = ah::Mem(at::kArea75Rhythm + B(at::kRow) * 15u);
    for (unsigned tries = 0; tries < 15; ++tries) {
        const unsigned k = ah::Next() % 15;
        if (row[k] != 0) {
            SetWord(ah::Mem(at::kMoveFrames), k + 15 * (ah::Next() % 8));
            return;
        }
    }
}

void Seed75(unsigned k) {
    Common();
    switch (k) {
    case k75FlyRun: case k75FollowRun: SeedState(); break;
    case k75FlyStep: if (ah::Half()) Sprite_Current[0x5D] = 0x80; break;
    case k75Drain: if (ah::Often()) B(at::kCounter0) = static_cast<unsigned char>(AH_PICK(0x1F, 0x20, 0x21, 0, 0x9F)); break;
    case k75FollowStep: if (ah::Half()) Field_Request = 0; break;
    case k75FollowKind2Z: if (ah::Half()) Field_Request = static_cast<unsigned char>(AH_PICK(5, 5, 4, 6)); break;
    case k75Choice14: case k75ChoiceF: SeedAnswer(); break;
    case k75Tail: SeedTail(); break;
    case k75Deal: {
        SeedCounterAt(at::kPlayerCounter, 0x1F4);
        if (ah::Often()) B(at::kMove) = static_cast<unsigned char>(AH_PICK(0, 1, 2, 3, 0xFF));
        // the list position at its count less one (the list ends here) or beside
        const unsigned char count = B(at::kArea75Lists + 4 + B(at::kList) * 8u);
        if (ah::Often()) B(at::kListPos) = static_cast<unsigned char>(count - AH_PICK(1, 1, 2, 0, 3));
        break;
    }
    case k75Play:
        B(at::kMove) = static_cast<unsigned char>(AH_PICK(0, 1, 2, 0, 1, 2, 3, 0xFF));
        if (ah::Often()) SetWord(ah::Mem(at::kMoveFrames), AH_PICK(1, 1, 0, 2));
        if (ah::Half()) B(at::kPressFlags) = static_cast<unsigned char>(B(at::kPressFlags) & ~8u);
        if (ah::Often()) B(at::kEarlyPresses) = static_cast<unsigned char>(AH_PICK(3, 4, 0, 0x83));
        if (ah::Often()) B(at::kIdleFrames) = static_cast<unsigned char>(AH_PICK(0x1E, 0x1F, 0, 0x9E));
        if (ah::Often()) SeedGap(AH_PICK(0xC8, 0xC9, 0xC7, 0));
        SeedRhythm();
        break;
    case k75Reached: if (ah::Often()) Cond_ByteFA = static_cast<signed char>(AH_PICK(0xA, 0xA, 9, 0xB, 0x8A)); break;
    case k75OtherPress: SeedRhythm(); break;
    case k75PlayerPress: case k75EarlyPress:
        if (ah::Half()) B(at::kPressFlags) = static_cast<unsigned char>(B(at::kPressFlags) & ~8u);
        break;
    case k75DrawCounters:
        if (ah::Half()) B(at::kPressFlags) = static_cast<unsigned char>(B(at::kPressFlags) & ~1u);
        if (ah::Often()) SeedGap(AH_PICK(0x96, 0x97, 0x95, 0));
        break;
    default: break;
    }
}
void Args75(unsigned k, U* a) {
    switch (k) {
    case k75Step: {
        // x at 0x1B0000 and beside (signed), z's high word at 0x38..0x3B and beside
        if (ah::Often()) a[0] = AH_PICK(0x1B0000, 0x1B0001, 0x1AFFFF, 0, 0x80000000u, 0x1B0000 + 0x10000);
        if (ah::Often()) a[1] = (AH_PICK(0x38, 0x3B, 0x37, 0x3C, 0x39, 0x138) << 16) | (a[1] & 0xFFFF);
        break;
    }
    case k75Trigger41: ArgsTrigger(a); break;
    case k75DrawWindow: {
        // x, y words (a high half now and then), w and h small or any, flag 0,
        // 1 or any
        a[0] = ah::Often() ? a[0] % 0x140 : a[0];
        a[1] = ah::Often() ? a[1] % 0xF0 : a[1];
        a[2] = ah::Often() ? AH_PICK(0x54, 0x48, 0x10, 3, 4, 5, 0x55, 0, 1, 2, 0xFFFF) : a[2];
        a[3] = ah::Often() ? AH_PICK(0x11, 0x10, 2, 3, 0xFF) : a[3];
        a[4] = ah::Often() ? AH_PICK(0, 0, 1, 2) : a[4];
        break;
    }
    default: break;
    }
}

void RunArea(int area, const ah::Clone* clones, unsigned n, const ah::DataTable* tables, unsigned n_tables,
             void (*seed)(unsigned), void (*args)(unsigned, U*), unsigned rounds) {
    ah::Group g{"area_w1f", clones, n, kCallees, sizeof kCallees / sizeof kCallees[0], tables, n_tables,
                g_regions, sizeof g_regions / sizeof g_regions[0], seed, &Disturb, rounds};
    g.args = args;
    g.area = area;
    ah::Run(g);
}

}  // namespace

void SelfTest() {
    constexpr unsigned kRounds = 6000;
    g_regions[kPacketRegion].at = Key(g_packets);
    std::memcpy(g_weights72, ah::Mem(at::kArea72Weights), sizeof g_weights72);
    std::memcpy(g_weights73, ah::Mem(at::kArea73Weights), sizeof g_weights73);
    RunArea(68, kClones68, sizeof kClones68 / sizeof kClones68[0], kTables68, 2, &Seed68, &Args68, kRounds);
    RunArea(69, kClones69, sizeof kClones69 / sizeof kClones69[0], kTables69, 1, &Seed69, nullptr, kRounds);
    RunArea(71, kClones71, sizeof kClones71 / sizeof kClones71[0], nullptr, 0, &Seed71, nullptr, kRounds);
    RunArea(72, kClones72, 1, nullptr, 0, &Seed72, nullptr, kRounds);
    RunArea(73, kClones73, 1, nullptr, 0, &Seed72, nullptr, kRounds);
    RunArea(74, kClones74, sizeof kClones74 / sizeof kClones74[0], nullptr, 0, &Seed74, &Args74, kRounds);
    RunArea(75, kClones75, sizeof kClones75 / sizeof kClones75[0], kTables75, 3, &Seed75, &Args75, kRounds);
}

}  // namespace area_w1f

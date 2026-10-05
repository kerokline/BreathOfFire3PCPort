// BOF3X_SHADOW=area_w3d: world 3's area 135 through the area round's shared
// harness (area_harness.h), once at start-up - one area_harness::Run with
// Group::area 135. docs/area_w3d.md section 3.
//
// The clone table is tools/area_rows.py --clones's rows for AREA135
// (2026-09-28), each row read against the disassembly (every start, extent,
// call site and the tail's jump table agree); the shapes are the root table
// each function hangs from (docs/area_w3d.md section 1). The group's own
// callees (the fall, the cell scans, the cell stamp and restore, the marker
// and box tests, the camera glide, the message helper) are recorders here like
// any other callee, so each function is fuzzed alone.
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w3d.h"
#include "game/area_w3d_callees.h"
#include "game/move_script_bytes.h"
#include "hook/log.h"

namespace area_w3d {
namespace {

namespace ah = area_harness;
using U = std::uint32_t;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

#define AH_N(a) static_cast<int>(sizeof a / sizeof a[0])

constexpr ah::CallSite kCalls41DAD0[] = {{0x12, 0x57C7C0}};
constexpr ah::CallSite kCalls41DB00[] = {{0x24, 0x5B9380}};
constexpr ah::CallSite kCalls41DB40[] = {{0x16, 0x57C7C0}, {0x29, 0x589810}};
constexpr ah::CallSite kCalls41DBB0[] = {{0x19, 0x57C0F0}, {0x22, 0x57C110}};
constexpr ah::CallSite kCalls41DBE0[] = {{0x7, 0x57C110}};
constexpr ah::CallSite kCalls41DC60[] = {{0x68, 0x594E00}};
constexpr ah::CallSite kCalls41DCE0[] = {{0x9F, 0x578D10}};
constexpr ah::CallSite kCalls41DE60[] = {{0x39, 0x5891F0}};
constexpr ah::CallSite kCalls41DEA0[] = {{0x32, 0x578C10}};
constexpr ah::CallSite kCalls41DF00[] = {{0x32, 0x578C10}};
constexpr ah::CallSite kCalls41DF60[] = {{0x53, 0x572570}, {0x87, 0x41EC40}, {0xA5, 0x5720C0}, {0xB0, 0x41EBE0}, {0xC7, 0x41EBE0}};
constexpr ah::CallSite kCalls41E040[] = {{0x5, 0x41EBE0}};
constexpr ah::CallSite kCalls41E050[] = {{0x0, 0x41ECF0}};
constexpr ah::CallSite kCalls41E060[] = {{0x0, 0x41EDE0}};
constexpr ah::CallSite kCalls41E070[] = {{0x9, 0x41EDE0}};
constexpr ah::CallSite kCalls41E090[] = {{0x46, 0x41EC40}};
constexpr ah::CallSite kCalls41E130[] = {{0x1, 0x41ECB0}, {0x74, 0x578C10}};
constexpr ah::CallSite kCalls41E1E0[] = {{0x1, 0x41ECB0}, {0x5C, 0x578C10}};
constexpr ah::CallSite kCalls41E280[] = {{0x5B, 0x578C10}};
constexpr ah::CallSite kCalls41E310[] = {{0x34, 0x578C10}};
constexpr ah::CallSite kCalls41E440[] = {{0xB, 0x57CD90}, {0x21, 0x41EDE0}, {0x3E, 0x57B530}, {0x6F, 0x46D710}};
constexpr ah::CallSite kCalls41E4E0[] = {{0x38, 0x46D770}};
constexpr ah::CallSite kCalls41E530[] = {{0x2C, 0x57CE10}};
constexpr ah::CallSite kCalls41E580[] = {{0x3F, 0x57C7C0}};
constexpr ah::CallSite kCalls41E5E0[] = {{0x18, 0x57C140}, {0x2E, 0x41EE70}, {0x3C, 0x531F90}};
constexpr ah::CallSite kCalls41E630[] = {
    {0x34, 0x591900}, {0x3E, 0x4976D0}, {0x49, 0x587B80}, {0x4F, 0x587910}, {0x60, 0x587A00}, {0x7A, 0x587B90},
    {0x7F, 0x57C7A0}, {0x94, 0x41EEF0}, {0xA2, 0x4976D0}, {0xC1, 0x57C160}, {0xCB, 0x587740}, {0xF0, 0x57C7A0},
    {0x113, 0x57C140}, {0x132, 0x5B9380}, {0x13C, 0x4976D0}, {0x159, 0x4976D0}, {0x165, 0x57C0F0}, {0x1A8, 0x41EF60},
    {0x1D4, 0x41EFA0}, {0x21F, 0x41EFA0}, {0x23D, 0x587740}, {0x257, 0x41EF60}, {0x271, 0x41EFA0}, {0x2C0, 0x41EFA0},
    {0x2E0, 0x587740}, {0x2FA, 0x41EF60}, {0x32F, 0x41EF60}, {0x3AC, 0x41EF60}, {0x402, 0x41EFA0}, {0x415, 0x41EF60},
    {0x478, 0x57C7A0}};
constexpr ah::JumpTable kTables41E630[] = {{0x1C, 0x498, 19}};
constexpr ah::CallSite kCalls41EB40[] = {{0x19, 0x57B130}, {0x25, 0x57C110}, {0x5D, 0x589810}};
constexpr ah::CallSite kCalls41ECB0[] = {{0x1F, 0x41EC40}};
constexpr ah::CallSite kCalls41ECF0[] = {{0x59, 0x572620}, {0x6D, 0x536700}, {0x8C, 0x579F00}, {0x98, 0x572620}, {0xAE, 0x536700}, {0xD1, 0x579F00}};
constexpr ah::CallSite kCalls41EDE0[] = {{0x32, 0x572620}, {0x47, 0x579F00}, {0x53, 0x572620}, {0x6C, 0x579F00}};
constexpr ah::CallSite kCalls41EFA0[] = {{0x7, 0x4976D0}, {0x19, 0x59E310}};

#define W3D_CLONE(name, base, size, calls, n, tables, nt, ret, shape) \
    {#name, base, size, calls, n, nullptr, 0, tables, nt, reinterpret_cast<const void*>(&::name), ret, false, ah::Shape::shape}

const ah::Clone kClones[] = {
    W3D_CLONE(Area135_ChoiceStartTail, 0x41DAD0, 0x26, kCalls41DAD0, AH_N(kCalls41DAD0), nullptr, 0, 0, kChoice),
    W3D_CLONE(Area135_ChoiceCount, 0x41DB00, 0x3C, kCalls41DB00, AH_N(kCalls41DB00), nullptr, 0, 0, kChoice),
    W3D_CLONE(Area135_ChoiceStartTail14, 0x41DB40, 0x63, kCalls41DB40, AH_N(kCalls41DB40), nullptr, 0, 0, kChoice),
    W3D_CLONE(Area135_ChoiceFlag37, 0x41DBB0, 0x2B, kCalls41DBB0, AH_N(kCalls41DBB0), nullptr, 0, 0, kChoice),
    W3D_CLONE(Area135_ClearFlagC, 0x41DBE0, 0x17, kCalls41DBE0, AH_N(kCalls41DBE0), nullptr, 0, 0, kHandler),
    W3D_CLONE(Area135_RiseInZone4, 0x41DC00, 0x22, nullptr, 0, nullptr, 0, 0, kHandler),
    W3D_CLONE(Area135_RiseElsewhere, 0x41DC30, 0x22, nullptr, 0, nullptr, 0, 0, kHandler),
    W3D_CLONE(Area135_LeaveByExit, 0x41DC60, 0x7E, kCalls41DC60, AH_N(kCalls41DC60), nullptr, 0, 0, kHandler),
    W3D_CLONE(Area135_MarkerRoute, 0x41DCE0, 0x11A, kCalls41DCE0, AH_N(kCalls41DCE0), nullptr, 0, 0, kHandler),
    W3D_CLONE(Area135_ToMarker, 0x41DE00, 0x53, nullptr, 0, nullptr, 0, 0, kHandler),
    W3D_CLONE(Area135_FaceLeaderDir, 0x41DE60, 0x40, kCalls41DE60, AH_N(kCalls41DE60), nullptr, 0, 0, kHandler),
    W3D_CLONE(Area135_HeldMove1, 0x41DEA0, 0x53, kCalls41DEA0, AH_N(kCalls41DEA0), nullptr, 0, 0, kHandler),
    W3D_CLONE(Area135_HeldMove7, 0x41DF00, 0x53, kCalls41DF00, AH_N(kCalls41DF00), nullptr, 0, 0, kHandler),
    W3D_CLONE(Area135_FallToFloor, 0x41DF60, 0xD7, kCalls41DF60, AH_N(kCalls41DF60), nullptr, 0, 0, kHandler),
    W3D_CLONE(Area135_Fall780, 0x41E040, 0xC, kCalls41E040, AH_N(kCalls41E040), nullptr, 0, 0, kHandler),
    W3D_CLONE(Area135_StampCellsRun, 0x41E050, 0x5, kCalls41E050, AH_N(kCalls41E050), nullptr, 0, 0, kHandler),
    W3D_CLONE(Area135_RestoreCellsRun, 0x41E060, 0x5, kCalls41E060, AH_N(kCalls41E060), nullptr, 0, 0, kHandler),
    W3D_CLONE(Area135_RestoreOnRequest5, 0x41E070, 0x1A, kCalls41E070, AH_N(kCalls41E070), nullptr, 0, 0, kHandler),
    W3D_CLONE(Area135_JumpByOccupied, 0x41E090, 0x79, kCalls41E090, AH_N(kCalls41E090), nullptr, 0, 0, kHandler),
    W3D_CLONE(Area135_QueueRun, 0x41E110, 0x12, nullptr, 0, nullptr, 0, 0, kHandler),
    W3D_CLONE(Area135_QueueStepZ, 0x41E130, 0xA3, kCalls41E130, AH_N(kCalls41E130), nullptr, 0, 0, kState),
    W3D_CLONE(Area135_QueueStepX, 0x41E1E0, 0x7C, kCalls41E1E0, AH_N(kCalls41E1E0), nullptr, 0, 0, kState),
    W3D_CLONE(Area135_WalkRun, 0x41E260, 0x12, nullptr, 0, nullptr, 0, 0, kHandler),
    W3D_CLONE(Area135_WalkToX, 0x41E280, 0x88, kCalls41E280, AH_N(kCalls41E280), nullptr, 0, 0, kState),
    W3D_CLONE(Area135_WalkToZ, 0x41E310, 0x51, kCalls41E310, AH_N(kCalls41E310), nullptr, 0, 0, kState),
    W3D_CLONE(Area135_LiftRun, 0x41E370, 0x12, nullptr, 0, nullptr, 0, 0, kHandler),
    W3D_CLONE(Area135_LiftUp, 0x41E390, 0x2D, nullptr, 0, nullptr, 0, 0, kState),
    W3D_CLONE(Area135_LiftDown, 0x41E3C0, 0x2D, nullptr, 0, nullptr, 0, 0, kState),
    W3D_CLONE(Area135_LiftDone, 0x41E3F0, 0x2A, nullptr, 0, nullptr, 0, 0, kState),
    W3D_CLONE(Area135_SpawnRun, 0x41E420, 0x12, nullptr, 0, nullptr, 0, 0, kHandler),
    W3D_CLONE(Area135_SpawnCopy, 0x41E440, 0x93, kCalls41E440, AH_N(kCalls41E440), nullptr, 0, 0, kState),
    W3D_CLONE(Area135_SpawnCountdown, 0x41E4E0, 0x47, kCalls41E4E0, AH_N(kCalls41E4E0), nullptr, 0, 0, kState),
    W3D_CLONE(Area135_SpawnKind1AtLeader, 0x41E530, 0x42, kCalls41E530, AH_N(kCalls41E530), nullptr, 0, 0, kHandler),
    W3D_CLONE(Area135_CellHook, 0x41E580, 0x5C, kCalls41E580, AH_N(kCalls41E580), nullptr, 0, 0xFF, kHook),
    W3D_CLONE(Area135_StepHook, 0x41E5E0, 0x4A, kCalls41E5E0, AH_N(kCalls41E5E0), nullptr, 0, 0xFF, kHook),
    W3D_CLONE(Area135_Tail18, 0x41E630, 0x504, kCalls41E630, AH_N(kCalls41E630), kTables41E630, AH_N(kTables41E630), 0, kTail),
    W3D_CLONE(Area135_Init, 0x41EB40, 0x99, kCalls41EB40, AH_N(kCalls41EB40), nullptr, 0, 0, kInit),
    W3D_CLONE(Area135_FallTo, 0x41EBE0, 0x54, nullptr, 0, nullptr, 0, 0, kCallee),
    W3D_CLONE(Area135_ObjectAtCell, 0x41EC40, 0x68, nullptr, 0, nullptr, 0, 0xFF, kCallee),
    W3D_CLONE(Area135_CountQueue, 0x41ECB0, 0x3B, kCalls41ECB0, AH_N(kCalls41ECB0), nullptr, 0, 0xFF, kCallee),
    W3D_CLONE(Area135_StampCells, 0x41ECF0, 0xEF, kCalls41ECF0, AH_N(kCalls41ECF0), nullptr, 0, 0, kCallee),
    W3D_CLONE(Area135_RestoreCells, 0x41EDE0, 0x86, kCalls41EDE0, AH_N(kCalls41EDE0), nullptr, 0, 0, kCallee),
    W3D_CLONE(Area135_NearMarker, 0x41EE70, 0x71, nullptr, 0, nullptr, 0, 0xFF, kCallee),
    W3D_CLONE(Area135_PartyInBox, 0x41EEF0, 0x61, nullptr, 0, nullptr, 0, 0xFF, kCallee),
    W3D_CLONE(Area135_Kind2ToObject0, 0x41EF60, 0x3E, nullptr, 0, nullptr, 0, 0, kCallee),
    W3D_CLONE(Area135_OpenMessage, 0x41EFA0, 0x31, kCalls41EFA0, AH_N(kCalls41EFA0), nullptr, 0, 0, kCallee),
};
enum : unsigned {
    kChoiceStart, kChoiceCount, kChoiceStart14, kChoiceFlag37, kClearFlagC, kRiseZone4, kRiseElse, kLeave, kRoute,
    kToMarker, kFaceLeader, kHeld1, kHeld7, kFallFloor, kFall780, kStampRun, kRestoreRun, kRestore5, kJump,
    kQueueRun, kQueueZ, kQueueX, kWalkRun, kWalkX, kWalkZ, kLiftRun, kLiftUp, kLiftDown, kLiftDone, kSpawnRun,
    kSpawnCopy, kSpawnCount, kSpawnLeader, kCellHook, kStepHook, kTail, kInit, kFallTo, kObjectAt, kCountQueue,
    kStamp, kRestore, kNearMarker, kPartyInBox, kKind2To0, kOpenMessage
};
static_assert(kOpenMessage + 1 == sizeof kClones / sizeof kClones[0], "area 135's seeding indices");
#undef W3D_CLONE
#undef AH_N

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
unsigned char& B(U address) { return *ah::Mem(address); }

// The running object as the harness puts it: a party record or one of the
// first four field objects.
unsigned char* RunningRecord(U v) { return v & 1 ? ah::PartyOf(static_cast<unsigned char>(v >> 1)) : ah::Object((v >> 1) % 4); }
// The script object: a field object, a party record, or the running object
// itself (as the movement script often has it).
unsigned char* ScriptRecord(U v) {
    switch (v % 3) {
    case 0: return Sprite_Current;
    case 1: return ah::PartyOf(static_cast<unsigned char>(v >> 2));
    default: return ah::Object(v >> 2);
    }
}

// --- the effects: the callees that move what the caller reads after, and the
// answers the recorders cannot give ------------------------------------------

constexpr U kAll = 0xFFFFFFFFu, kU8 = 0xFFu, kU16 = 0xFFFFu;

// Louder than the real callees, each only half the time (from Noise): the
// running object and the script object after a call its caller reads them
// again after.
U MovesCurrent(const U*, U answer) {
    const U n = ah::Noise();
    if (n & 1) Sprite_Current = RunningRecord(n >> 8);
    if (n & 2) ah::SetPointer(at::kScriptObject, ScriptRecord(n >> 12));
    return answer;
}
// AreaMap_ByteAt: the dword +0x20 of the running object (the caller or's
// into it through a pointer taken before the call), and the running object.
U ByteAtEffect(const U* a, U answer) {
    const U n = ah::Noise();
    if (n & 4) SetLong(Sprite_Current + 0x20, static_cast<std::int32_t>(ah::Noise()));
    return MovesCurrent(a, answer);
}
// Crt_sprintf: the choice answer choice 1 reads again after it.
U SprintfEffect(const U*, U answer) {
    const U n = ah::Noise();
    if (n & 1) B(at::kChoiceAnswer) = static_cast<unsigned char>(n >> 8);
    return answer;
}
// EventOp_Bx / Flags_Clear: the zone the init reads again after them.
U ZoneEffect(const U*, U answer) {
    static const unsigned char kZones[] = {0, 4, 6, 7, 1, 5};
    const U n = ah::Noise();
    if (n & 1) Cond_ByteFD = kZones[(n >> 8) % 6];
    return answer;
}
// Sound_StreamDone: Field_Request, which state 1 reads after it.
U StreamEffect(const U*, U answer) {
    const U n = ah::Noise();
    if (n & 1) Field_Request = static_cast<unsigned char>(n & 2 ? 2 : n >> 8);
    return answer;
}
// Area135_Kind2ToObject0: counter 3 and Field_Kind2Hold (the tail tests
// counter 3 right after it), to a tested value half the time.
U Kind2Effect(const U*, U answer) {
    static const unsigned char kCounts[] = {0xB, 0xD, 0x10, 0x12, 0x15, 0x11, 0};
    const U n = ah::Noise();
    if (n & 1) B(at::kCounter3) = kCounts[(n >> 8) % 7];
    if (n & 2) B(at::kKind2Hold) = static_cast<unsigned char>(n >> 16);
    return answer;
}
// Area135_OpenMessage: the effect slot and the speed byte the tail reads
// after it (the slot kept inside the 20 records).
U MessageEffect(const U*, U answer) {
    const U n = ah::Noise();
    if (n & 1) B(at::kEffectSlot) = static_cast<unsigned char>((n >> 8) % at::kEffectCount);
    if (n & 2) B(at::kMoveSpeed3) = static_cast<unsigned char>(n >> 16);
    return answer;
}
// Effect_FindFree: a slot of the 20 records or none (a third of the time).
U EffectSlotEffect(const U*, U answer) {
    const U n = ah::Noise();
    return (answer & 0xFFFFFF00u) | (n % 3 == 0 ? 0xFFu : (n >> 4) % at::kEffectCount);
}
// MapView_GroundAt's answers this round (the seed picks): 0 garbage, 1 every
// ground 0x8000 as s16, 2 near the s16 edges.
unsigned g_groundMode;
// MapView_GroundAt: the round's mode (g_groundMode): garbage, every ground
// 0x8000 as s16 (so the highest keeps its start), or near the s16 edges.
U GroundEffect(const U* a, U answer) {
    MovesCurrent(a, answer);
    const U n = ah::Noise();
    switch (g_groundMode) {
    case 1: return (answer & 0xFFFF0000u) | 0x8000u;
    case 2: {
        static const U kEdges[] = {0x8000, 0x8001, 0x7FFF, 0xFFFF, 0, 0x8002};
        return (answer & 0xFFFF0000u) | (n % 7 == 6 ? n >> 16 : kEdges[n % 7]);
    }
    default: return answer;
    }
}
// Sprite_FindFree: 0..29 or none (a third of the time).
U SpriteSlotEffect(const U*, U answer) {
    const U n = ah::Noise();
    return (answer & 0xFFFFFF00u) | (n % 3 == 0 ? 0xFFu : (n >> 4) % 30);
}

#define W3D_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
#define W3D_THEIRS(name) #name, KeyOf(name), KeyOf(name)
const ah::Callee kCallees[] = {
    // the group's own, called directly (kGarbage, not kPhase, where an effect
    // is listed: the harness's kPhase recorder returns before the effect)
    {W3D_OURS(Area135_FallTo), 1, {kU16}, ah::Answer::kGarbage, 0, 0},
    {W3D_OURS(Area135_ObjectAtCell), 2, {kAll, kAll}, ah::Answer::kByte, 0xFF, 0x1D, {}, &MovesCurrent},
    {W3D_OURS(Area135_CountQueue), 0, {}, ah::Answer::kByte, 0, 5, {}, &MovesCurrent},
    {W3D_OURS(Area135_StampCells), 0, {}, ah::Answer::kPhase, 0, 0},
    {W3D_OURS(Area135_RestoreCells), 0, {}, ah::Answer::kGarbage, 0, 0, {}, &MovesCurrent},
    {W3D_OURS(Area135_NearMarker), 2, {kAll, kAll}, ah::Answer::kFlag, 0, 0},
    {W3D_OURS(Area135_PartyInBox), 0, {}, ah::Answer::kFlag, 0, 0},
    {W3D_OURS(Area135_Kind2ToObject0), 1, {kU16}, ah::Answer::kGarbage, 0, 0, {}, &Kind2Effect},
    {W3D_OURS(Area135_OpenMessage), 1, {kU8}, ah::Answer::kGarbage, 0, 0, {}, &MessageEffect},
    // engine callees nobody owns (raw)
    {"Engine_46D710", kEngine46D710, kEngine46D710, 0, {}, ah::Answer::kGarbage, 0, 0, {}, &MovesCurrent},
    {"Engine_46D770", kEngine46D770, kEngine46D770, 0, {}, ah::Answer::kGarbage, 0, 0, {}, &MovesCurrent},
    // named, beyond the standard set
    {W3D_OURS(ScriptFlags_Set40), 0, {}, ah::Answer::kGarbage, 0, 0},
    {W3D_OURS(ScriptFlags_Clear40), 0, {}, ah::Answer::kGarbage, 0, 0},
    {W3D_OURS(Effect_FindFree), 0, {}, ah::Answer::kGarbage, 0, 0, {}, &EffectSlotEffect},
    {W3D_OURS(Sprite_FindFree), 0, {}, ah::Answer::kGarbage, 0, 0, {}, &SpriteSlotEffect},
    {W3D_OURS(EventOp_9x), 1, {kAll}, ah::Answer::kGarbage, 0, 0, {}, &MovesCurrent},
    {W3D_OURS(EventOp_Bx), 1, {kAll}, ah::Answer::kGarbage, 0, 0, {}, &ZoneEffect},
    {W3D_OURS(MoveCmd_OpF7), 4, {kAll, kAll, kU8, kAll}, ah::Answer::kGarbage, 0, 0, {}, &MovesCurrent},
    {W3D_THEIRS(MoveCmd_Move), 2, {kAll, kU8}, ah::Answer::kGarbage, 0, 0, {}, &MovesCurrent},
    {W3D_THEIRS(Crt_sprintf), 3, {kAll, kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &SprintfEffect},
    {W3D_OURS(KeyItem_Add), 1, {kAll}, ah::Answer::kGarbage, 0, 0},
    {W3D_THEIRS(Sound_StopMusic), 0, {}, ah::Answer::kGarbage, 0, 0},
    {W3D_OURS(Sound_LoadStream), 1, {kAll}, ah::Answer::kGarbage, 0, 0},
    {W3D_OURS(Sound_StreamDone), 0, {}, ah::Answer::kFlag, 0, 0, {}, &StreamEffect},
    {W3D_OURS(Flags_Toggle), 2, {kAll, kU8}, ah::Answer::kGarbage, 0, 0},
    {W3D_OURS(Window_FreeCurrent), 0, {}, ah::Answer::kGarbage, 0, 0},
    {W3D_OURS(AreaMap_SetHeight), 3, {kU16, kU16, kU8}, ah::Answer::kGarbage, 0, 0, {}, &MovesCurrent},
    // standard ones listed again: what the callee reads, or what the caller reads after
    {W3D_OURS(AreaMap_ByteAt), 2, {kU16, kU16}, ah::Answer::kGarbage, 0, 0, {}, &ByteAtEffect},
    {W3D_OURS(AreaMap_SetByte), 3, {kU16, kU16, kU8}, ah::Answer::kGarbage, 0, 0, {}, &MovesCurrent},
    {W3D_OURS(MapView_GroundAt), 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &GroundEffect},
    {W3D_OURS(Flags_Clear), 2, {kAll, kU8}, ah::Answer::kGarbage, 0, 0, {}, &ZoneEffect},
    {W3D_OURS(Effect_Spawn), 5, {kU8, kU8, kU8, kU16, kU16}, ah::Answer::kByte, 0xFE, 0x02, {}, &MovesCurrent},
};
#undef W3D_OURS
#undef W3D_THEIRS

// The four state tables, contiguous and not overlapping as the fuzz swaps
// them (each dispatcher reading on past its own reads the next one's
// recorders, as the original reads the next table's handlers).
const ah::DataTable kTables[] = {
    {at::kArea135QueueStates, 2}, {at::kArea135WalkStates, 2}, {at::kArea135LiftStates, 9}, {at::kArea135SpawnStates, 2}};

// Beyond the field frame: the effect records, the script object pointer, the
// effect slot, Sprite_Kind2's record, the input words, the height scale, the
// kind-2 hold, the speed byte, the map flags byte, the init's byte, the
// window pool words.
const ah::Region kRegions[] = {
    {at::kEffectObjects, at::kEffectCount * at::kEffectStride},
    {at::kScriptObject, 4},
    {at::kEffectSlot, 1},
    {at::kSpriteKind2, at::kObjectStride},
    {at::kInputHeld, 6},
    {at::kHeightScale, 1},
    {at::kKind2Hold, 1},
    {at::kMoveSpeed3, 1},
    {at::kArea135MapFlags, 1},
    {at::kInitBit, 1},
    {at::kPoolWords, 4},
    {at::kArea135Routes, at::kArea135RouteCount * at::kArea135RouteStride},
};

// The exe's own routes, put back two rounds in three (the shipped table has
// 0xFF in both target cells or neither, and no no-target route shares its
// cell with a later one: the others reach those cases).
unsigned char g_routes[at::kArea135RouteCount * at::kArea135RouteStride];


// Every round: the script object inside the regions, the effect slot inside
// the 20 records, the tail's sub byte a field object's index.
void Common() {
    ah::SetPointer(at::kScriptObject, ScriptRecord(ah::Next()));
    B(at::kEffectSlot) = static_cast<unsigned char>(ah::Next() % at::kEffectCount);
    B(at::kTailSub) = static_cast<unsigned char>(ah::Next() % at::kObjectCount);
    if (ah::Often()) Cond_ByteFD = static_cast<unsigned char>(AH_PICK(0, 4, 6, 7, 4, 1, 5, 3, 0x84));
    if (ah::Often()) std::memcpy(ah::Mem(at::kArea135Routes), g_routes, sizeof g_routes);
    g_groundMode = 0;
}

// The group's cells, moved by the harness's disturbance about one call in
// 24 - drawn only from h (area_harness.h: a group disturb never draws Next).
void Disturb(U h) {
    const auto v = static_cast<unsigned char>(h >> 20);
    switch ((h >> 8) % 9) {
    case 0: B(at::kTailState) = static_cast<unsigned char>(h & 0x100 ? v % 0x20 : v); break;
    case 1: B(at::kCounter3) = v; break;
    case 2: ah::SetPointer(at::kScriptObject, h & 0x10000 ? ah::Object(h >> 17) : ah::PartyOf(static_cast<unsigned char>(h >> 17))); break;
    case 3: B(at::kTailSub) = static_cast<unsigned char>(v % at::kObjectCount); break;
    case 4: B(at::kEffectSlot) = static_cast<unsigned char>(v % at::kEffectCount); break;
    case 5: Cond_ByteFD = v; break;
    case 6: B(h & 0x100 ? at::kInputHeld : at::kInputPressed) = v; break;
    case 7: B(at::kKind2Hold) = h & 0x100 ? 0 : v; break;
    default: SetWord(ah::Mem(h & 0x100 ? at::kMarkerCellX : at::kMarkerCellZ), v); break;
    }
}

// A choice answer: each value a handler tests, its neighbours, a negative
// byte (choice 2 reads it signed), anything.
void SeedAnswer() {
    if (ah::Often()) B(at::kChoiceAnswer) = static_cast<unsigned char>(AH_PICK(0, 1, 2, 0, 1, 0xFF, 0x80, 0x7F, 3, 0x81));
}
// A dispatcher's state byte: inside what it reaches (past that ours aborts
// and the original jumps through a zero or data: never seeded).
void SeedState(unsigned reach) { Sprite_Current[4] = static_cast<unsigned char>(ah::Next() % reach); }
// A word near `edge` (step-paired), or anything.
U Near(U edge, U step) {
    if (!ah::Often()) return ah::Next();
    return edge + AH_PICK(0, 0, 1, 0xFFFFFFFFu, 2, 0xFFFFFFFEu) * step;
}

// Values the seeds hand to the args hook (the args hook cannot write memory).
U g_cellX, g_cellZ;

void SeedTail() {
    static const U kStates[] = {0, 1, 5, 6, 0xA, 0xB, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
                                0, 1, 5, 6, 0xA, 0xB, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
                                2, 3, 4, 7, 8, 9, 0xC, 0x13, 0x20, 0x7F, 0x80, 0xFF, 0xE0};
    const auto state = static_cast<unsigned char>(ah::Pick(kStates, sizeof kStates / sizeof kStates[0]));
    B(at::kTailState) = state;
    const bool on = ah::Often();
    switch (state) {
    case 0: case 1: case 0xB: case 0x14:
        Field_Request = static_cast<unsigned char>(on ? AH_PICK(0, 1, 3, 5) : 2);
        break;
    case 6: SetWord(ah::Mem(at::kTailTimer), on ? 0 : AH_PICK(1, 2, 0x100, 0xFFFF)); break;
    case 0x15: case 0x1F: B(at::kKind2Hold) = static_cast<unsigned char>(on ? 0 : AH_PICK(1, 0x80, 0xFF)); break;
    case 0x16: case 0x19:
        B(at::kInputHeld) = static_cast<unsigned char>(on ? B(at::kInputHeld) | 0x20 : B(at::kInputHeld) & ~0x20u);
        break;
    case 0x18:
        B(at::kInputPressed) = static_cast<unsigned char>(on ? B(at::kInputPressed) | 0x20 : B(at::kInputPressed) & ~0x20u);
        break;
    case 0x17: B(at::kCounter3) = static_cast<unsigned char>(on ? 0xB : AH_PICK(0xA, 0xC, 0x8B)); break;
    case 0x1A: B(at::kCounter3) = static_cast<unsigned char>(on ? 0xD : AH_PICK(0xC, 0xE, 0x8D)); break;
    case 0x1B:
        B(at::kCounter3) = static_cast<unsigned char>(on ? 0x10 : AH_PICK(0xF, 0x11, 0x90));
        if (ah::Half()) B(at::kTailSub) = 0xFF;
        break;
    case 0x1C: B(at::kCounter3) = static_cast<unsigned char>(on ? 0x11 : AH_PICK(0x10, 0x12, 0x91)); break;
    case 0x1D: B(at::kCounter3) = static_cast<unsigned char>(on ? 0x12 : AH_PICK(0x11, 0x13, 0x92)); break;
    case 0x1E: B(at::kCounter3) = static_cast<unsigned char>(on ? 0x15 : AH_PICK(0x14, 0x16, 0x95)); break;
    default: break;
    }
}

void Seed(unsigned k) {
    Common();
    unsigned char* const cur = Sprite_Current;
    switch (k) {
    case kChoiceStart: case kChoiceCount: case kChoiceStart14: case kChoiceFlag37: SeedAnswer(); break;
    case kRiseZone4: case kRiseElse: case kStepHook:
        Cond_ByteFD = static_cast<unsigned char>(AH_PICK(4, 4, 6, 7, 0, 5, 3, 0x84));
        break;
    case kLeave:
        if (ah::Often()) MoveScript_Object[3] = static_cast<unsigned char>(6 + ah::Next() % 4);
        Cond_ByteFD = static_cast<unsigned char>(ah::Half() ? 4 : ah::Next());
        break;
    case kRoute:
        // the marker's cell at a route's (read from the image's table) two
        // rounds in three, its target cells sometimes 0xFF
        if (ah::Often()) {
            const U i = ah::Next() % at::kArea135RouteCount;
            unsigned char* const r = ah::Mem(at::kArea135Routes + i * at::kArea135RouteStride);
            if (ah::Half()) {
                // one target cell 0xFF or the other, and a later route at the
                // same cell
                if (ah::Half()) r[ah::Half() ? 3 : 5] = 0xFF;
                if (i + 1 < at::kArea135RouteCount && ah::Half()) {
                    unsigned char* const later =
                        ah::Mem(at::kArea135Routes + (i + 1 + ah::Next() % (at::kArea135RouteCount - 1 - i)) * at::kArea135RouteStride);
                    later[0] = r[0];
                    later[1] = r[1];
                }
            }
            // the cell words with a high byte now and then (read as words)
            SetWord(ah::Mem(at::kMarkerCellX), r[0] | (ah::Next() % 4 == 0 ? 0x100u << (ah::Next() % 8) : 0u));
            SetWord(ah::Mem(at::kMarkerCellZ), ah::Often() ? r[1] : r[1] + 1u);
        }
        break;
    case kToMarker:
        if (ah::Half()) Sprite_Current = ah::Mem(at::kSpriteKind2);
        break;
    case kHeld1: case kHeld7:
        SetWord(cur + (k == kHeld1 ? 0x3A : 0x36), static_cast<unsigned>(AH_PICK(7, 6, 5, 8, 0x8000, 0x7FFF, 0)));
        if (ah::Often()) B(at::kInputHeld) = static_cast<unsigned char>(B(at::kInputHeld) | 0x20);
        break;
    case kRestore5: case kStamp: Field_Request = static_cast<unsigned char>(AH_PICK(5, 5, 4, 6, 0)); break;
    case kJump:
        // every record the running object can become (the party records, the
        // first four field objects) on a jump cell half the time, so a
        // stand-in that moves it lands on another
        for (unsigned m = 0; m < 7; ++m) {
            if (ah::Half()) continue;
            unsigned char* const o = m < 3 ? ah::PartyOf(static_cast<unsigned char>(m)) : ah::Object(m - 3);
            const U i = ah::Next() % 4;
            SetLong(o + 0x34, static_cast<std::int32_t>(static_cast<U>(B(at::kArea135JumpCells + i * 2)) << 16 | 0x8000));
            SetLong(o + 0x38, static_cast<std::int32_t>(static_cast<U>(B(at::kArea135JumpCells + i * 2 + 1)) << 16 | 0x8000));
        }
        // the running object on one of the four cells two rounds in three
        if (ah::Often()) {
            const U i = ah::Next() % 4;
            SetLong(cur + 0x34, static_cast<std::int32_t>(static_cast<U>(B(at::kArea135JumpCells + i * 2)) << 16 | 0x8000));
            SetLong(cur + 0x38, static_cast<std::int32_t>(static_cast<U>(B(at::kArea135JumpCells + i * 2 + 1)) << 16 |
                                                          (ah::Often() ? 0x8000 : 0)));
        }
        break;
    case kQueueRun: SeedState(at::kArea135QueueReach); break;
    case kWalkRun: SeedState(at::kArea135WalkReach); break;
    case kLiftRun: SeedState(at::kArea135LiftReach); break;
    case kSpawnRun: SeedState(at::kArea135SpawnReach); break;
    case kQueueZ:
        SetLong(cur + 0x38, static_cast<std::int32_t>(0xC8000 + (ah::Next() % 6 << 17) + AH_PICK(0, 0x4000, 0x7FFF, 0x8000, 0xFFFFC000u, 0xFFFF8000u, 0x123456)));
        break;
    case kQueueX:
        SetLong(cur + 0x34, static_cast<std::int32_t>(Near(0x208000, 0x4000)));
        break;
    case kWalkX: SetLong(cur + 0x34, static_cast<std::int32_t>(Near(0x1C8000, 0x4000))); break;
    case kWalkZ: SetLong(cur + 0x38, static_cast<std::int32_t>(Near(0x148000, 0x4000))); break;
    case kLiftDone:
        if (ah::Often()) SetLong(cur + 0x34, static_cast<std::int32_t>(ah::Often() ? 0x168000 : AH_PICK(0x168001, 0x167FFF, 0x178000)));
        if (ah::Often()) SetLong(cur + 0x38, static_cast<std::int32_t>(ah::Often() ? 0x128000 : AH_PICK(0x128001, 0x127FFF, 0x138000)));
        break;
    case kSpawnCopy: B(at::kTailSub) = static_cast<unsigned char>(ah::Often() ? ah::Next() % 4 : ah::Next()); break;
    case kSpawnCount: cur[0xA] = static_cast<unsigned char>(AH_PICK(0, 0, 1, 2, 0x80, 0xFF)); break;
    case kSpawnLeader: if (ah::Often()) B(at::kPartyList1) = static_cast<unsigned char>(ah::Next() % 9); break;
    case kCellHook: {
        const unsigned char* const e = ah::Mem(at::kArea135CellEntries + (ah::Next() & 1) * 4);
        g_cellX = ah::Often() ? e[0] | (ah::Half() ? ah::Next() & 0xFFFFFF00u : 0) : ah::Next();
        g_cellZ = ah::Often() ? e[1] | (ah::Half() ? ah::Next() & 0xFFFFFF00u : 0) : ah::Next();
        B(at::kLeaderDir) = static_cast<unsigned char>(ah::Often() ? e[2] & 0xF : AH_PICK(0x17, 0x11, 0, 3, 0xF7));
        break;
    }
    case kFallFloor: g_groundMode = AH_PICK(0, 1, 1, 2); break;
    case kTail: SeedTail(); break;
    case kInit:
        Cond_ByteFD = static_cast<unsigned char>(AH_PICK(0, 4, 4, 6, 7, 1, 5, 0x84));
        B(at::kEntryZone) = static_cast<unsigned char>(ah::Half() ? 1 : AH_PICK(0, 2, 4, 0x81));
        break;
    case kObjectAt: {
        // (x, z) a cell some objects stand on; the running object among them
        // half the time (it is skipped)
        g_cellX = ah::Next() & 0x3FF8000u;
        g_cellZ = ah::Next() & 0x3FF8000u;
        if (ah::Half()) {
            // one object alone at the cell (object 29 half the time), in use
            // and of kind 0xA
            unsigned char* const o = ah::Object(ah::Half() ? 29 : ah::Next() % at::kObjectCount);
            o[0] = static_cast<unsigned char>(o[0] | 1);
            o[6] = 0xA;
            SetLong(o + 0x34, static_cast<std::int32_t>(g_cellX));
            SetLong(o + 0x38, static_cast<std::int32_t>(g_cellZ));
            break;
        }
        for (unsigned i = 0; i < at::kObjectCount; ++i) {
            if (ah::Next() % 5 == 0) continue;
            unsigned char* const o = ah::Object(i);
            o[0] = static_cast<unsigned char>(ah::Next() % 4 ? o[0] | 1 : o[0] & 0xFE);
            o[6] = static_cast<unsigned char>(ah::Next() % 4 ? 0xA : AH_PICK(0xB, 0x8A, 0));
            SetLong(o + 0x34, static_cast<std::int32_t>(ah::Next() % 4 ? g_cellX : g_cellX + 0x8000));
            SetLong(o + 0x38, static_cast<std::int32_t>(ah::Next() % 4 ? g_cellZ : g_cellZ ^ 0x10000));
            if (ah::Next() % 3) break;
        }
        break;
    }
    case kNearMarker: {
        B(at::kMarker9) = static_cast<unsigned char>(AH_PICK(0, 1, 2, 0x10, 0xFF));
        SetLong(ah::Mem(at::kMarkerStepX), static_cast<std::int32_t>(ah::Half() ? 0 : ah::Next() % 0x20000 - 0x10000));
        SetLong(ah::Mem(at::kMarkerStepZ), static_cast<std::int32_t>(ah::Half() ? 0 : ah::Next() % 0x20000 - 0x10000));
        break;
    }
    case kPartyInBox:
        Field_MemberCount = static_cast<unsigned char>(AH_PICK(1, 2, 3, 3, 0, 4));
        for (unsigned m = 0; m < 3; ++m) {
            if (ah::Often()) SetWord(ah::Mem(at::kPartyCellX + m * at::kPartyStride), static_cast<unsigned>(AH_PICK(0x2C, 0x2D, 0x2B, 0x2E, 0x102C)));
            if (ah::Often()) SetWord(ah::Mem(at::kPartyCellZ + m * at::kPartyStride), static_cast<unsigned>(AH_PICK(0x30, 0x37, 0x2F, 0x38, 0x33, 0xFFB0)));
        }
        break;
    default: break;
    }
}

// r = (the running object's +0x70 byte + the marker's + 2) << 15, as
// Area135_NearMarker computes it.
U MarkerReach() {
    return ((static_cast<U>(Sprite_Current[0x70]) + ah::Mem(at::kMarker70)[0] + 2) << 15);
}
// A coordinate at the marker's (less its step times +9) plus `delta` times
// the reach, or anything.
U NearMarkerAt(U position, U step, U frames) {
    const U centre = position - frames * step;
    if (!ah::Often()) return ah::Next();
    const U r = MarkerReach();
    switch (ah::Next() % 7) {
    case 0: return centre;
    case 1: return centre + r - 1;
    case 2: return centre + r;
    case 3: return centre - r + 1;
    case 4: return centre - r;
    case 5: return centre + ah::Next() % (r ? r : 1);
    default: return centre + 0x80000000u;
    }
}

void Args(unsigned k, U* a) {
    switch (k) {
    case kCellHook:
        a[0] = g_cellX;
        a[1] = g_cellZ;
        break;
    case kFallTo: {
        // a height whose difference from +0x3E gives a fall count not 0 (a 0
        // faults the original's idiv and aborts ours: never seeded)
        U q = 1 + ah::Next() % 255;
        if ((q & 0xF) == 0) q += 1 + ah::Next() % 15;
        if (q > 255) q = 255;
        const U d = q * 128 + ah::Next() % 128;
        const U word = Word(Sprite_Current + 0x3E) + (ah::Half() ? d : 0u - d);
        a[0] = (ah::Half() ? ah::Next() & 0xFFFF0000u : 0) | (word & 0xFFFF);
        break;
    }
    case kObjectAt:
        a[0] = ah::Often() ? g_cellX : ah::Next();
        a[1] = ah::Often() ? g_cellZ : ah::Next();
        break;
    case kNearMarker: {
        const U frames = ah::Mem(at::kMarker9)[0];
        a[0] = NearMarkerAt(static_cast<U>(Long(ah::Mem(at::kMarkerX))), static_cast<U>(Long(ah::Mem(at::kMarkerStepX))), frames);
        a[1] = NearMarkerAt(static_cast<U>(Long(ah::Mem(at::kMarkerZ))), static_cast<U>(Long(ah::Mem(at::kMarkerStepZ))), frames);
        break;
    }
    case kKind2To0: a[0] = ah::Half() ? a[0] & 0xFFFF0000u : a[0]; break;
    default: break;
    }
}

}  // namespace

void SelfTest() {
    constexpr unsigned kRounds = 6000;
    ah::Group g{"area_w3d", kClones, sizeof kClones / sizeof kClones[0], kCallees, sizeof kCallees / sizeof kCallees[0],
                kTables, sizeof kTables / sizeof kTables[0], kRegions, sizeof kRegions / sizeof kRegions[0], &Seed,
                &Disturb, kRounds};
    std::memcpy(g_routes, ah::Mem(at::kArea135Routes), sizeof g_routes);
    g.args = &Args;
    g.area = 135;
    ah::Run(g);
}

}  // namespace area_w3d

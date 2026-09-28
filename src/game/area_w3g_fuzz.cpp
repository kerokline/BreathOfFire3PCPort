// BOF3X_SHADOW=area_w3g: world 3's areas 148..151 through the area round's
// shared harness (area_harness.h), once at start-up: five area_harness::Run
// calls under the one shadow name (area 148 twice: its hooks and tails, then
// its searchlight effect), each with Group::area its area number, the real
// descriptors and tables in place, the .data state tables swapped through
// DataTable. docs/area_w3g.md section "The fuzz".
//
// The clone rows are tools/area_rows.py's (--unit AREA148..151 --clones,
// 2026-09-28), each read against the disassembly. Area 151's group is area
// 115's (area_w3a_fuzz.cpp, itself area 88's and area 45's) over area 151's
// tables, with the cell and name-set plants gone (its place hook has neither
// search).
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w3g.h"
#include "game/area_w3g_callees.h"
#include "game/move_script_bytes.h"
#include "hook/log.h"

namespace area_w3g {
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
#define W3G_C(name, base, size, calls, shape) \
    {#name, base, size, calls, AH_N(calls), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::name), 0, false, shape}
#define W3G_P(name, base, size, shape) \
    {#name, base, size, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::name), 0, false, shape}
// A clone answering in al.
#define W3G_A(name, base, size, calls, shape) \
    {#name, base, size, calls, AH_N(calls), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::name), 0xFF, false, shape}
// A clone with a jump table.
#define W3G_T(name, base, size, calls, tables, shape) \
    {#name, base, size, calls, AH_N(calls), nullptr, 0, tables, AH_N(tables), reinterpret_cast<const void*>(&::name), 0, false, shape}

// The fuzz's own literal addresses (read off the exe, docs/area_w3g.md): never
// the constants ours reads, so a wrong one in ours shows.
constexpr U kFzCursor = 0x7DEE67;
constexpr U kFzLeaderDir = 0x802D48;
constexpr U kFzTailKind = 0x9039F3;
constexpr U kFzTailState = 0x9039F4;
constexpr U kFzTailArg = 0x9039F5;
constexpr U kFzTailTimer = 0x9039F6;
constexpr U kFzScriptVar = 0x90384B;
constexpr U kFzEffects = 0x7E11E0;
constexpr U kFzDrawPass = 0x7E0918;
constexpr U kFzPacketNext = 0x7E0670;
constexpr U kFzAngle1 = 0x929ECA;
constexpr U kFzCameFrom = 0x802290;
constexpr U kFzExtra0 = 0x802000;
constexpr U kFzKind2Sprite = 0x7E0974;   // Sprite_Kind2 + 0x34 .. + 0x3F
constexpr U kFzFocusX = 0x929F14;
constexpr U kFzOrigin = 0x7E0688;
constexpr U kFzScriptObject = 0x929E80;  // MoveScript_Object
constexpr U kFzCondFE = 0x905E20;
constexpr U kFzCameraDistance = 0x903840;
constexpr U kFzKind2X = 0x905E64, kFzKind2Z = 0x905E60;
constexpr U kFzMemberId = 0x802DC9;
constexpr U kFzSwitch = 0x6353F0;

// BOF3X_AR3G_GROUP=n runs one group alone (the controls script's shortcut):
// 1480 area 148's hooks and tails, 1481 its beam, 149, 150, 151; unset, all.
bool Wants(int group) {
    const char* const only = std::getenv("BOF3X_AR3G_GROUP");
    return only == nullptr || *only == 0 || std::atoi(only) == group;
}

// A cursor (the choices' answer, s8): its cases, their edges, the sign's.
unsigned char SomeCursor() {
    return static_cast<unsigned char>(ah::Often() ? AH_PICK(0, 0, 1, 1, 2, 3, 0xFF, 0x80, 0x7F, 0xFE) : ah::Next());
}
// A 16.16 position whose high word is at and about `lo` (the hooks' boxes).
U BoxWord(U lo, U span) {
    U high;
    if (ah::Often()) high = lo - 1 + ah::Next() % (span + 2);                  // lo - 1 .. lo + span
    else if (ah::Half()) high = lo + AH_PICK(0x100, 0x8000, 0x200, 0x4000);   // the box's word with bits above a byte
    else high = ah::Next();
    return (high << 16) | (ah::Next() & 0xFFFF);
}
// An object trigger is called (a field object, 0x904030).
void ArgsTrigger(U* a) {
    a[0] = Key(ah::Object(a[0]));
    a[1] = 0x904030;
}

const ah::Callee kSet40 = {"ScriptFlags_Set40", bof3::addr::ScriptFlags_Set40, KeyOf(&::ScriptFlags_Set40), 0, {}, ah::Answer::kGarbage, 0, 0};
const ah::Callee kClear40 = {"ScriptFlags_Clear40", bof3::addr::ScriptFlags_Clear40, KeyOf(&::ScriptFlags_Clear40), 0, {}, ah::Answer::kGarbage, 0, 0};
// Effect_FindFree answers a slot 0..19 or 0xFF for none.
const ah::Callee kFindFree = {"Effect_FindFree", bof3::addr::Effect_FindFree, KeyOf(&::Effect_FindFree), 0, {}, ah::Answer::kByte, 0xFF, 0x13};

// ===========================================================================
// Area 148: its hooks, choices, handlers and tail kind 31 (group 1480)
// ===========================================================================

// clones: tools/area_rows.py --unit AREA148 --clones
constexpr ah::CallSite kCallsMsgBy[] = {{0x75, 0x4976D0}};
constexpr ah::CallSite kCalls4224E0[] = {{0x19, 0x57C0F0}};
constexpr ah::CallSite kCalls422510[] = {{0x22, 0x531F90}, {0x55, 0x57C0F0}, {0x6E, 0x594E00}, {0x78, 0x589810}, {0xEC, 0x5734F0},
                                         {0x116, 0x57C0F0}, {0x13B, 0x589810}, {0x1A4, 0x57C7A0}, {0x1BE, 0x57C160}, {0x1C8, 0x587740},
                                         {0x1D4, 0x57C140}, {0x1FA, 0x5718F0}, {0x22A, 0x57C7A0}};
constexpr ah::JumpTable kTables422510[] = {{0x1C, 0x240, 10}};
constexpr ah::CallSite kCalls422790[] = {{0x15, 0x57C7C0}};
constexpr ah::CallSite kCalls4227C0[] = {{0x0, 0x57C7C0}};
constexpr ah::CallSite kCalls4227E0[] = {{0x3F, 0x57C7C0}};
const ah::Clone kClones1480[] = {
    W3G_C(Area148_MemberMessageA, 0x4223A0, 0x8B, kCallsMsgBy, S::kHandler),
    W3G_C(Area148_MemberMessageB, 0x422430, 0x8B, kCallsMsgBy, S::kHandler),
    W3G_P(Area148_ChoiceMessage, 0x4224C0, 0x16, S::kChoice),
    W3G_C(Area148_ChoiceSetFlag59, 0x4224E0, 0x2C, kCalls4224E0, S::kChoice),
    W3G_T(Area148_Tail31, 0x422510, 0x27E, kCalls422510, kTables422510, S::kTail),
    W3G_A(Area148_ArriveHook, 0x422790, 0x2E, kCalls422790, S::kHook),
    W3G_A(Area148_Trigger17, 0x4227C0, 0x16, kCalls4227C0, S::kCallee),
    W3G_A(Area148_SwitchHook, 0x4227E0, 0x5C, kCalls4227E0, S::kHook),
};
enum : unsigned { k148MsgA, k148MsgB, k148Choice0, k148Choice1, k148Tail, k148Arrive, k148Trigger, k148Switch };
static_assert(k148Switch + 1 == AH_COUNT(kClones1480), "area 148's seeding indices");

const ah::Callee kCallees1480[] = {
    kSet40, kClear40, kFindFree,
    {"Flags_Toggle", bof3::addr::Flags_Toggle, KeyOf(&::Flags_Toggle), 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0},
    {"Kind2_Place", bof3::addr::Kind2_Place, KeyOf(&::Kind2_Place), 1, {kU8}, ah::Answer::kGarbage, 0, 0},
    {"Gfx_ClutAdjust", bof3::addr::Gfx_ClutAdjust, KeyOf(&::Gfx_ClutAdjust), 5, {kAll, kAll, kAll, kAll, kAll}, ah::Answer::kGarbage, 0, 0},
};
const ah::Region kRegions1480[] = {{kFzEffects, 20 * 0x80}, {kFzDrawPass, 1}, {kFzAngle1, 2}};

void SeedMembers() {
    static const U kIds[] = {5, 2, 8, 4, 0, 1, 3, 6, 7, 9, 0xFF};
    for (U m = 0; m < 3; ++m)
        if (ah::Often()) Mem(kFzMemberId + m * 0x14C)[0] = static_cast<unsigned char>(kIds[ah::Next() % AH_COUNT(kIds)]);
    if (ah::Half()) Field_MemberCount = static_cast<unsigned char>(AH_PICK(0, 1, 2, 3, 3, 4));
}

void Seed1480(unsigned k) {
    switch (k) {
    case k148MsgA: case k148MsgB: SeedMembers(); break;
    case k148Choice0: case k148Choice1: Mem(kFzCursor)[0] = SomeCursor(); break;
    case k148Tail: {
        Mem(kFzTailState)[0] = static_cast<unsigned char>(
            ah::Often() ? AH_PICK(0, 1, 1, 0xA, 0xB, 0xC, 0xD, 0xE, 0x14, 0x15, 0xD, 0x15, 2, 9, 0xF, 0x13, 0x16, 0x80, 0xFF, 0x7F)
                        : ah::Next());
        Mem(kFzScriptVar)[0] = static_cast<unsigned char>(ah::Often() ? AH_PICK(0x18, 0x20, 0, 0x19, 0x1F, 0x21, 0x17, 1) : ah::Next());
        SetWord(Mem(kFzTailTimer), ah::Often() ? AH_PICK(1, 1, 2, 0, 0x100, 0x101, 0xFFFF) : ah::Next());
        // the slot the flash is kept in: one of the twenty (its +0 bit 0 either way), or any byte
        const U slot = ah::Often() ? ah::Next() % 20 : ah::Next() & 0xFF;
        Mem(kFzTailArg)[0] = static_cast<unsigned char>(slot);
        if (slot < 20) Mem(kFzEffects + slot * 0x80)[0] = static_cast<unsigned char>(ah::Half() ? ah::Next() & 0xFE : ah::Next() | 1);
        break;
    }
    case k148Switch:
        Mem(kFzLeaderDir)[0] = static_cast<unsigned char>(ah::Often() ? AH_PICK(1, 1, 1, 0x11, 0x81, 0, 2, 9) : ah::Next());
        break;
    default: break;
    }
}
void Args1480(unsigned k, U* a) {
    if (k == k148Arrive) {
        a[0] = BoxWord(0x12, 3);
        a[1] = ah::Often() ? (AH_PICK(0x1C, 0x1C, 0x1B, 0x1D, 0x11C, 0x1C0) << 16) | (ah::Next() & 0xFFFF) : ah::Next();
    } else if (k == k148Trigger) {
        ArgsTrigger(a);
    } else if (k == k148Switch) {
        // the record's bytes (x 0x4A, z 0x37) with bits above, one off, or anything
        const unsigned char* const r = Mem(kFzSwitch);
        if (ah::Often()) {
            a[0] = r[0] | (ah::Half() ? 0 : ah::Next() & 0xFFFFFF00u);
            a[1] = r[1] | (ah::Half() ? 0 : ah::Next() & 0xFFFFFF00u);
            if (ah::Half()) a[ah::Next() & 1] += ah::Half() ? 1 : 0xFFFFFFFFu;
        } else {
            a[0] = ah::Next();
            a[1] = ah::Next();
        }
    }
}

// ===========================================================================
// Area 148's searchlight: effect kind 0x7E and its draws (group 1481)
// ===========================================================================

// The fuzz's packet buffer: every primitive a draw builds is at
// Gfx_PacketNext, which the link stand-in moves on (kept with room for the
// band's second quad, built at the first's + 0x44).
constexpr unsigned kPacketBytes = 0x300, kPacketRoom = 0xA0;
alignas(16) unsigned char g_packets[kPacketBytes];

bool InPackets(U p, unsigned n) { return p >= Key(g_packets) && p + n <= Key(g_packets) + kPacketBytes; }
void Scribble(U at, unsigned n) {
    if (!InPackets(at, n)) return;
    unsigned char* const p = Mem(at);
    for (unsigned i = 0; i < n; i += 4) SetLong(p + i, static_cast<std::int32_t>(ah::Noise()));
}
void SetPacketNext(U offset) { Gfx_PacketNext = g_packets + (offset % 0x40) * 4; }
// The link stand-in logs the primitive it links (each draw builds several at
// the pointer), writes its tag dword (the link is into the prim), then
// moves the pointer on by the size, kept with room in the buffer.
U LinkEffect(const U* a, U answer) {
    const U size = a[3] & 0xFF;
    const U at = Key(Gfx_PacketNext);
    if (InPackets(at, size)) {
        ah::NoteBytes(Gfx_PacketNext, size);
        SetLong(Gfx_PacketNext, static_cast<std::int32_t>(ah::Noise()));
    }
    const U next = at + size;
    if (!InPackets(next, kPacketRoom)) SetPacketNext(ah::Noise());
    else Gfx_PacketNext = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(next));
    return answer;
}
// The primitive setters write the primitive's bytes, so a store the caller
// makes before the call (where the original makes it after) shows.
U PolyEffect(const U* a, U answer) { Scribble(a[0], 0x44); return answer; }
U DrawModeEffect(const U* a, U answer) { Scribble(a[0], 0xC); return answer; }
U SemiEffect(const U* a, U answer) {
    if (InPackets(a[0], 8)) Mem(a[0])[7] = static_cast<unsigned char>(a[1] ? Mem(a[0])[7] | 2 : Mem(a[0])[7] & 0xFD);
    return answer;
}
// 0x494110 writes the projected vertex (two floats and a depth) at its
// second argument: random bits (NaNs and huge values included) half the
// time, else small floats about the screen (so the _ftol differences are
// ordinary numbers).
U ProjectEffect(const U* a, U answer) {
    if (!InPackets(a[1], 12)) return answer;
    unsigned char* const v = Mem(a[1]);
    const U n = ah::Noise();
    if (n & 1) {
        ah::FillBytes(v, 12);
    } else {
        for (unsigned c = 0; c < 3; ++c) {
            const U m = ah::Noise();
            float f = static_cast<float>(static_cast<std::int32_t>(m % 0x400) - 0x200);
            if (m & 0x400) f += static_cast<float>((m >> 12) & 0xFF) / 256.0f;
            if (m & 0x800) f = -f;
            std::memcpy(v + c * 4, &f, 4);
        }
    }
    return answer;
}
// 0x4941E0 writes two shorts (the screen size) at its third argument.
U SizeEffect(const U* a, U answer) {
    const U n = ah::Noise();
    unsigned char* const out = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a[2]));
    const U lo = n & 1 ? (n >> 4) & 0xFFFF : (n >> 4) % 0x60;
    SetWord(out, lo);
    SetWord(out + 2, (n >> 20) & 0xFFFF);
    return answer;
}
// Party_MemberAt: the running object's +0x3C (the beam sets it for the asks)
// logged with each ask; the answer a member or none.
U MemberAtEffect(const U*, U answer) {
    ah::Note(static_cast<U>(Long(Sprite_Current + 0x3C)));
    return answer;
}

// clones: tools/area_rows.py --unit AREA148 --clones
constexpr ah::CallSite kCalls422860[] = {{0x16, 0x4229C0}, {0x2C, 0x422A20}};
constexpr ah::CallSite kCalls4228B0[] = {{0x16, 0x422B50}, {0x54, 0x422B50}, {0x5C, 0x422A50}, {0x79, 0x422A20}, {0x88, 0x5B93D2}};
constexpr ah::CallSite kCalls422970[] = {{0xD, 0x422B50}, {0x15, 0x422A50}};
constexpr ah::CallSite kCalls4229C0[] = {{0x45, 0x5720C0}};
constexpr ah::CallSite kCalls422A50[] = {{0x79, 0x531F10}, {0xD5, 0x57C8A0}};
constexpr ah::CallSite kCalls422B50[] = {{0x17, 0x57C140}, {0x38, 0x5A79A0}, {0x50, 0x5A77C0}, {0x66, 0x572FA0}, {0x6B, 0x494060},
                                         {0x77, 0x5A7650}, {0x7F, 0x5A7780}, {0x8D, 0x494110}, {0x9B, 0x494110}, {0xF3, 0x572FA0},
                                         {0xFC, 0x5B9550}, {0x115, 0x5B9550}, {0x126, 0x5A7A70}, {0x14A, 0x4941E0}, {0x170, 0x4941E0},
                                         {0x192, 0x5B9550}, {0x19A, 0x5B9550}, {0x1A0, 0x422FF0}, {0x1B4, 0x5B9550}, {0x1BC, 0x5B9550},
                                         {0x1D1, 0x5B9550}, {0x1D9, 0x5B9550}, {0x1DF, 0x422D90}, {0x1F4, 0x5A79A0}, {0x20C, 0x5A77C0},
                                         {0x222, 0x572FA0}};
constexpr ah::CallSite kCalls422D90[] = {{0xC, 0x5A7610}, {0x14, 0x5A7780}, {0x69, 0x5A7A50}, {0x82, 0x5A7A00}, {0xB7, 0x5A7A50},
                                         {0xD4, 0x5A7A00}, {0x197, 0x572FA0}, {0x1B2, 0x5A7A50}, {0x1D4, 0x5A7A00}, {0x1F5, 0x5A7A50},
                                         {0x217, 0x5A7A00}, {0x244, 0x572FA0}};
constexpr ah::CallSite kCalls422FF0[] = {{0x49, 0x5A75F0}, {0x51, 0x5A7780}, {0x65, 0x5A7A50}, {0x82, 0x5A7A00}, {0xB3, 0x5A7A50},
                                         {0xD0, 0x5A7A00}, {0x14D, 0x572FA0}};
const ah::Clone kClones1481[] = {
    W3G_P(Area148_BeamRun, 0x422840, 0x12, S::kCallee),
    W3G_C(Area148_BeamStart, 0x422860, 0x4F, kCalls422860, S::kState),
    W3G_C(Area148_BeamSweep, 0x4228B0, 0xBC, kCalls4228B0, S::kState),
    W3G_C(Area148_BeamPause, 0x422970, 0x46, kCalls422970, S::kState),
    W3G_C(Area148_BeamAhead, 0x4229C0, 0x5D, kCalls4229C0, S::kCallee),
    W3G_P(Area148_BeamVelocity, 0x422A20, 0x2D, S::kCallee),
    W3G_C(Area148_BeamTurnMembers, 0x422A50, 0xF9, kCalls422A50, S::kCallee),
    W3G_C(Area148_DrawBeam, 0x422B50, 0x232, kCalls422B50, S::kCallee),
    W3G_C(Area148_DrawBeamBand, 0x422D90, 0x252, kCalls422D90, S::kCallee),
    W3G_C(Area148_DrawBeamFan, 0x422FF0, 0x16C, kCalls422FF0, S::kCallee),
};
enum : unsigned { kBeamRun, kBeamStart, kBeamSweep, kBeamPause, kBeamAhead, kBeamVelocity, kBeamTurn, kDrawBeam, kDrawBand, kDrawFan };
static_assert(kDrawFan + 1 == AH_COUNT(kClones1481), "the beam's seeding indices");

const ah::Callee kCallees1481[] = {
    {"Area148_BeamAhead", 0x4229C0, KeyOf(&::Area148_BeamAhead), 0, {}, ah::Answer::kPhase, 0, 0},
    {"Area148_BeamVelocity", 0x422A20, KeyOf(&::Area148_BeamVelocity), 0, {}, ah::Answer::kPhase, 0, 0},
    {"Area148_BeamTurnMembers", 0x422A50, KeyOf(&::Area148_BeamTurnMembers), 0, {}, ah::Answer::kPhase, 0, 0},
    {"Area148_DrawBeam", 0x422B50, KeyOf(&::Area148_DrawBeam), 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0},
    {"Area148_DrawBeamFan", 0x422FF0, KeyOf(&::Area148_DrawBeamFan), 4, {kU16, kU16, kU16, kU16}, ah::Answer::kGarbage, 0, 0},
    {"Area148_DrawBeamBand", 0x422D90, KeyOf(&::Area148_DrawBeamBand), 8, {kU16, kU16, kU16, kU16, kU16, kU16, kU16, kU16},
     ah::Answer::kGarbage, 0, 0},
    {"Party_MemberAt", bof3::addr::Party_MemberAt, KeyOf(&::Party_MemberAt), 3, {kAll, kAll, kAll}, ah::Answer::kByte, 0xFF, 0x02,
     {}, &MemberAtEffect, nullptr},
    {"Member_SetState2_8", bof3::addr::Member_SetState2_8, KeyOf(&::Member_SetState2_8), 2, {kU8, kU8}, ah::Answer::kGarbage, 0, 0},
    {"Math_Ratan2", bof3::addr::Math_Ratan2, KeyOf(&::Math_Ratan2), 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0},
    {"Ftol_5B9550", 0x5B9550, 0x5B9550, 0, {}, ah::Answer::kThrough, 0, 0},
    {"SetMapCamera_494060", kSetMapCamera, kSetMapCamera, 0, {}, ah::Answer::kGarbage, 0, 0},
    {"ProjectPoint_494110", kProjectPoint, kProjectPoint, 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0, {12}, &ProjectEffect, nullptr},
    // the offset pair is a stack local of the caller's: logged through the pointer
    {"ScreenSize_4941E0", kScreenSize, kScreenSize, 3, {kAll, 0, 0}, ah::Answer::kGarbage, 0, 0, {12, 4}, &SizeEffect, nullptr},
    {"MapView_LinkPrimAt", bof3::addr::MapView_LinkPrimAt, KeyOf(&::MapView_LinkPrimAt), 4, {kAll, kAll, kAll, kAll}, ah::Answer::kGarbage, 0, 0,
     {}, &LinkEffect, nullptr},
    {"Gpu_SetDrawMode", bof3::addr::Gpu_SetDrawMode, KeyOf(&::Gpu_SetDrawMode), 5, {kAll, kAll, kAll, kAll, kAll}, ah::Answer::kGarbage, 0, 0,
     {}, &DrawModeEffect, nullptr},
    {"Gpu_SetLineF2", bof3::addr::Gpu_SetLineF2, KeyOf(&::Gpu_SetLineF2), 1, {kAll}, ah::Answer::kGarbage, 0, 0, {}, &PolyEffect, nullptr},
    {"Gpu_SetPolyG3", bof3::addr::Gpu_SetPolyG3, KeyOf(&::Gpu_SetPolyG3), 1, {kAll}, ah::Answer::kGarbage, 0, 0, {}, &PolyEffect, nullptr},
    {"Gpu_SetPolyG4", bof3::addr::Gpu_SetPolyG4, KeyOf(&::Gpu_SetPolyG4), 1, {kAll}, ah::Answer::kGarbage, 0, 0, {}, &PolyEffect, nullptr},
    {"Gpu_SetSemiTrans", bof3::addr::Gpu_SetSemiTrans, KeyOf(&::Gpu_SetSemiTrans), 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &SemiEffect, nullptr},
};
const ah::DataTable kTables1481[] = {{0x6353F4, 3}};   // Area148_BeamStates
enum : unsigned { kRegBeamPackets = 2 };
ah::Region g_regions1481[] = {{kFzDrawPass, 1}, {kFzPacketNext, 4}, {0, kPacketBytes}};

// Beam arguments the args hook hands the draws: planted by the seed.
U g_beam_args[8];

void SeedBeam(unsigned k) {
    unsigned char* const o = Sprite_Current;
    SetPacketNext(ah::Next());
    if (ah::Often()) o[6] = static_cast<unsigned char>(ah::Often() ? AH_PICK(2, 2, 0, 1, 3) : ah::Next());
    if (ah::Often()) o[8] = static_cast<unsigned char>(ah::Often() ? ah::Next() % 8 : ah::Next());
    switch (k) {
    case kBeamRun: o[1] = static_cast<unsigned char>(ah::Next() % 3); break;
    case kBeamSweep:
    case kBeamPause:
        if (ah::Often()) Field_Request = 0;
        o[9] = static_cast<unsigned char>(ah::Often() ? AH_PICK(1, 1, 2, 0, 0x50, 0xFF) : ah::Next());
        o[0xA] = static_cast<unsigned char>(ah::Often() ? AH_PICK(1, 1, 2, 0, 0xFF) : ah::Next());
        // Rand's first answer a multiple of 45 (no pause), one off, or any
        if (ah::Half()) ah::SetRandFirst(static_cast<int>(45 * (ah::Next() % 700) + AH_PICK(0, 0, 1, 44, 0xFFFFFFFFu)));
        if (ah::Half()) Draw_PassFlags = static_cast<unsigned char>(Draw_PassFlags | 1);
        break;
    case kBeamTurn:
        o[9] = static_cast<unsigned char>(ah::Often() ? (ah::Next() % 0x50) : ah::Next());
        if (ah::Often()) Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 & ~0x1400u);
        if (ah::Half()) Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 | AH_PICK(0x400, 0x1000, 0x800, 0x200, 0x2000));
        if (ah::Often()) Field_Request = static_cast<unsigned char>(ah::Often() ? 0 : AH_PICK(1, 2, 5, 0x80));
        for (unsigned m = 0; m < 3; ++m) {
            unsigned char* const p = ah::PartyOf(static_cast<unsigned char>(m));
            if (ah::Often()) p[1] = static_cast<unsigned char>(ah::Often() ? 1 : AH_PICK(0, 2, 0x81, 0xFF));
            if (ah::Often()) p[8] = static_cast<unsigned char>(ah::Often() ? ah::Next() % 8 : ah::Next());
        }
        break;
    case kDrawBeam:
        Draw_PassFlags = static_cast<unsigned char>(ah::Often() ? AH_PICK(1, 0x1F, 4, 0x80, 0) : ah::Next());
        if (ah::Half()) Frame_Counter = AH_PICK(0, 1, 2, 3, 0xFFFFFFFFu);
        break;
    case kDrawBand:
    case kDrawFan: {
        // coordinates about the screen, radii small and huge, angles about the turns
        for (unsigned i = 0; i < 8; ++i) {
            const U n = ah::Next();
            U v;
            switch (i % 4) {
            case 2: v = ah::Often() ? n % 0x80 : n; break;                               // a radius
            case 3: v = ah::Often() ? AH_PICK(0, 0x400, 0x800, 0xC00, 0xFFFF, 0xF800, 0x10C00) + (n % 3) : n; break;   // an angle
            default: v = ah::Often() ? (n % 0x400) - 0x200 : n; break;                   // x, y
            }
            g_beam_args[i] = v;
        }
        break;
    }
    default: break;
    }
}
void ArgsBeam(unsigned k, U* a) {
    unsigned char* const o = Sprite_Current;
    if (k == kDrawBeam) {
        a[0] = Key(o + 0x34);
        a[1] = Key(o + 0x18);
    } else if (k == kDrawBand || k == kDrawFan) {
        for (unsigned i = 0; i < 8; ++i) a[i] = g_beam_args[i];
    }
}
// The beam's disturbance: the facing, the counters, the draw flags, the
// packet pointer (every draw reads it again), Field_Request.
void DisturbBeam(U h) {
    const unsigned v = (h >> 8) & 0xFF;
    unsigned char* const o = Sprite_Current;
    switch ((h >> 16) % 6) {
    case 0: o[8] = static_cast<unsigned char>(v % 8); break;
    case 1: o[9] = static_cast<unsigned char>(v); break;
    case 2: Draw_PassFlags = static_cast<unsigned char>(v & 1 ? 0 : v); break;
    case 3: SetPacketNext(h >> 24); break;
    case 4: Field_Request = static_cast<unsigned char>(v & 1 ? 0 : v); break;
    case 5: o[6] = static_cast<unsigned char>(v % 4); break;
    default: break;
    }
}
// Gfx_PacketNext kept in the buffer (the disturbance moves pointers about).
void SettleBeam() {
    if (!InPackets(Key(Gfx_PacketNext), kPacketRoom)) SetPacketNext(0);
}

// ===========================================================================
// Area 149 (group 149)
// ===========================================================================

// clones: tools/area_rows.py --unit AREA149 --clones
constexpr ah::CallSite kCalls423180[] = {{0x1B, 0x587740}, {0x2D, 0x4232B0}};
constexpr ah::CallSite kCalls4231D0[] = {{0x14, 0x587740}, {0x41, 0x4232B0}};
constexpr ah::CallSite kCalls423230[] = {{0x13, 0x587740}, {0x40, 0x4232B0}};
constexpr ah::CallSite kCalls423290[] = {{0x2, 0x4232B0}};
constexpr ah::CallSite kCalls4232B0[] = {{0x18, 0x589810}, {0x8A, 0x5720C0}};
constexpr ah::CallSite kCalls423380[] = {{0x4D, 0x5720C0}, {0x59, 0x5725F0}, {0x61, 0x56FCA0}, {0x8D, 0x5720C0}, {0xB9, 0x5720C0}};
constexpr ah::CallSite kCalls423450[] = {{0x66, 0x5720C0}, {0x72, 0x5725F0}, {0x77, 0x56FCA0}, {0x93, 0x5720C0}, {0xDD, 0x594E00},
                                         {0x12D, 0x5720C0}, {0x139, 0x5725F0}, {0x13E, 0x56FCA0}, {0x15A, 0x5720C0}, {0x1A0, 0x594E00}};
constexpr ah::JumpTable kTables423450[] = {{0x1B, 0x1AC, 5}};
constexpr ah::CallSite kCalls423620[] = {{0x30, 0x5720C0}, {0x49, 0x57C140}};
const ah::Clone kClones149[] = {
    W3G_P(Area149_DustRun, 0x423160, 0x12, S::kHandler),
    W3G_C(Area149_DustNear880, 0x423180, 0x41, kCalls423180, S::kState),
    W3G_C(Area149_DustNear680, 0x4231D0, 0x55, kCalls4231D0, S::kState),
    W3G_C(Area149_DustAtZero, 0x423230, 0x54, kCalls423230, S::kState),
    W3G_C(Area149_DustHold, 0x423290, 0x16, kCalls423290, S::kState),
    W3G_C(Area149_SpawnDust, 0x4232B0, 0xCC, kCalls4232B0, S::kCallee),
    W3G_C(Area149_ViewShiftBack, 0x423380, 0xCA, kCalls423380, S::kCallee),
    W3G_T(Area149_Tail33, 0x423450, 0x1CD, kCalls423450, kTables423450, S::kTail),
    W3G_C(Area149_Init, 0x423620, 0xA4, kCalls423620, S::kInit),
};
enum : unsigned { k149Run, k149S0, k149S1, k149S2, k149S3, k149Spawn, k149Shift, k149Tail, k149Init };
static_assert(k149Init + 1 == AH_COUNT(kClones149), "area 149's seeding indices");

// AreaMap_Elevation's answer a height (all of eax: MapView_SetElevation is
// handed it whole).
const ah::Callee kCallees149[] = {
    kFindFree,
    {"Area149_SpawnDust", 0x4232B0, KeyOf(&::Area149_SpawnDust), 1, {kU8}, ah::Answer::kGarbage, 0, 0},
    {"ViewShift_56FCA0", kViewShift, kViewShift, 0, {}, ah::Answer::kGarbage, 0, 0},
};
const ah::DataTable kTables149[] = {{0x6361FC, 4}};   // Area149_DustStates
const ah::Region kRegions149[] = {{kFzEffects, 20 * 0x80}, {kFzFocusX, 4}, {kFzOrigin, 2}, {kFzKind2Sprite, 0xC},
                                  {kFzCameFrom, 2}, {kFzScriptObject, 4}, {kFzCondFE, 1}};

// A 16.16 x at and about a cell edge.
std::int32_t Edge(U v) { return static_cast<std::int32_t>(v + AH_PICK(0, 0, 1, 0xFFFFFFFFu, 0x10000, 0xFFFF0000u, 0x8000)); }

void Seed149(unsigned k) {
    // the running script's pointer a field object (the states move its +0xA)
    ah::SetPointer(kFzScriptObject, ah::Object(ah::Next()));
    switch (k) {
    case k149Run: Sprite_Current[4] = static_cast<unsigned char>(ah::Next() % 4); break;
    case k149S0: case k149S1: case k149S2: case k149S3:
        SetWord(Mem(kFzCameraDistance),
                ah::Often() ? AH_PICK(0x87F, 0x880, 0x881, 0x67F, 0x680, 0x681, 0, 0, 1, 0xFFFF, 0x8000, 0x7FFF) : ah::Next());
        // extra record 0's x about the kind-2 x + 2 cells
        if (ah::Often()) SetLong(Mem(kFzExtra0 + 0x34), Edge(static_cast<U>(Long(Mem(kFzKind2X))) + 0x20000u));
        break;
    case k149Spawn:
        if (ah::Often()) Frame_Counter = Frame_Counter & ~3u;
        break;
    case k149Shift:
    case k149Tail: {
        SetLong(Mem(kFzFocusX), static_cast<std::int32_t>(ah::Often() ? AH_PICK(0x63FF, 0x77FF, 0x63FF, 0x77FF, 0x63FE, 0x7800, 0x163FF) : ah::Next()));
        Mem(kFzTailState)[0] = static_cast<unsigned char>(
            ah::Often() ? AH_PICK(0, 0, 2, 0xA, 0xA, 0xC, 1, 3, 9, 0xB, 0xD, 0x80, 0xFF) : ah::Next());
        SetWord(Mem(kFzTailTimer), ah::Often() ? AH_PICK(1, 1, 2, 0, 0x5A) : ah::Next());
        // the dust records: live kind 0x37 ones, and near misses
        for (U e = 0; e < 20; ++e) {
            if (!ah::Half()) continue;
            unsigned char* const r = Mem(kFzEffects + e * 0x80);
            r[0] = static_cast<unsigned char>(ah::Often() ? ah::Next() | 1 : ah::Next() & 0xFE);
            r[5] = static_cast<unsigned char>(ah::Often() ? 0x37 : AH_PICK(0x36, 0x38, 0x137 & 0xFF, 0));
        }
        break;
    }
    case k149Init:
        SetWord(Mem(kFzCameFrom), ah::Often() ? AH_PICK(0x94, 0x94, 0xA7, 0xA7, 0x95, 0xA6, 0x194, 0x1A7) : ah::Next());
        break;
    default: break;
    }
}
void Args149(unsigned k, U* a) {
    if (k == k149Spawn) a[0] = (ah::Often() ? AH_PICK(0xFC, 0xFD, 0xFE, 0xFF, 0, 1, 0x7F, 0x80) : ah::Next() & 0xFF) | (ah::Next() & 0xFFFFFF00u);
}
// The disturbance: the focus, the timer, the came-from word, a dust record.
void Disturb149(U h) {
    const unsigned v = (h >> 8) & 0xFF;
    switch ((h >> 16) % 5) {
    case 0: SetLong(Mem(kFzFocusX), v & 1 ? 0x63FF : 0x77FF); break;
    case 1: SetWord(Mem(kFzTailTimer), v % 3); break;
    case 2: SetWord(Mem(kFzCameFrom), v & 1 ? 0x94 : 0xA7); break;
    case 3: Mem(kFzEffects + (v % 20) * 0x80)[0] ^= 1; break;
    case 4: Frame_Counter = Frame_Counter ^ (v & 3); break;
    default: break;
    }
}
// MoveScript_Object kept a field object (the disturbance may move pointers).
void Settle149() {
    const U p = static_cast<U>(Long(Mem(kFzScriptObject)));
    if (p < Key(ah::Object(0)) || p >= Key(ah::Object(0)) + 30 * 0xA4) ah::SetPointer(kFzScriptObject, ah::Object(0));
}

// ===========================================================================
// Area 150 (group 150)
// ===========================================================================

// clones: tools/area_rows.py --unit AREA150 --clones
constexpr ah::CallSite kCalls4236D0[] = {{0x21, 0x5919B0}, {0x31, 0x590BB0}, {0x42, 0x587740}};
constexpr ah::CallSite kCalls423760[] = {{0x24, 0x4976D0}, {0x48, 0x57C7A0}, {0x5E, 0x594E00}, {0xB9, 0x57C7A0}, {0xDF, 0x57C140},
                                         {0xF1, 0x4976D0}, {0x111, 0x533E50}, {0x118, 0x587910}, {0x128, 0x587A00}, {0x13F, 0x587AE0},
                                         {0x146, 0x495040}, {0x159, 0x57C110}, {0x168, 0x57C110}};
constexpr ah::JumpTable kTables423760[] = {{0x1E, 0x180, 7}};
constexpr ah::CallSite kCalls423910[] = {{0x1A, 0x57C7C0}};
const ah::Clone kClones150[] = {
    W3G_C(Area150_ChoiceFill5B, 0x4236D0, 0x49, kCalls4236D0, S::kChoice),
    W3G_P(Area150_ChoiceMessage, 0x423720, 0x17, S::kChoice),
    W3G_P(Area150_ChoiceTailState, 0x423740, 0x1C, S::kChoice),
    W3G_T(Area150_Tail58, 0x423760, 0x1A9, kCalls423760, kTables423760, S::kTail),
    W3G_A(Area150_StepHook, 0x423910, 0x33, kCalls423910, S::kHook),
};
enum : unsigned { k150Fill, k150Message, k150TailState, k150Tail, k150Step };
static_assert(k150Step + 1 == AH_COUNT(kClones150), "area 150's seeding indices");

const ah::Callee kCallees150[] = {
    kSet40, kClear40,
    {"Party_HealJoined", bof3::addr::Party_HealJoined, KeyOf(&::Party_HealJoined), 0, {}, ah::Answer::kGarbage, 0, 0},
    {"Sound_LoadStream", bof3::addr::Sound_LoadStream, KeyOf(&::Sound_LoadStream), 1, {kAll}, ah::Answer::kGarbage, 0, 0},
    // all of eax is tested: 0, al 0 with bits above, or not 0
    {"Sound_StreamDone", bof3::addr::Sound_StreamDone, KeyOf(&::Sound_StreamDone), 0, {}, ah::Answer::kFlag, 0, 0},
    {"Transition_Start", bof3::addr::Transition_Start, KeyOf(&::Transition_Start), 1, {kU8}, ah::Answer::kGarbage, 0, 0},
    // the count's byte is what the caller subtracts from 16
    {"Inventory_Count", bof3::addr::Inventory_Count, KeyOf(&::Inventory_Count), 3, {kAll, kAll, kAll}, ah::Answer::kByte, 0x00, 0x12},
};
const ah::Region kRegions150[] = {{kFzDrawPass, 1}, {0x929EC1, 1}, {0x9036D0, 1}};

void Seed150(unsigned k) {
    switch (k) {
    case k150Fill: case k150Message: case k150TailState: Mem(kFzCursor)[0] = SomeCursor(); break;
    case k150Tail:
        Mem(kFzTailState)[0] = static_cast<unsigned char>(
            ah::Often() ? AH_PICK(0xA, 0xC, 0xE, 0x14, 0x15, 0x16, 0x16, 0xC, 0xB, 0xD, 0xF, 0x13, 9, 0x17, 0x80, 0xFF) : ah::Next());
        Field_Request = static_cast<unsigned char>(ah::Often() ? AH_PICK(0, 2, 0, 2, 1, 3, 0x82) : ah::Next());
        break;
    default: break;
    }
}
void Args150(unsigned k, U* a) {
    if (k == k150Step) {
        a[0] = BoxWord(0x17, 6);
        a[1] = BoxWord(0x33, 6);
    }
}

// ===========================================================================
// Area 151: the world-map copy (area 115's group, area_w3a_fuzz.cpp; area
// 88's, area_w2b_fuzz.cpp; area 45's, area_w1b_fuzz.cpp) (group 151)
// ===========================================================================

constexpr unsigned kItemBytes = 0x48;
alignas(16) unsigned char g_items[4][kItemBytes];
alignas(16) unsigned char g_names[4][16];

void WmAdvance(U size) {
    U next = Key(Gfx_PacketNext) + size;
    if (!InPackets(next, 0x50)) next = Key(g_packets) + (ah::Noise() % 4) * 4;
    Gfx_PacketNext = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(next));
}
U WmCommitEffect(const U* a, U answer) { WmAdvance(a[1] & 0xFF); return answer; }
U WmLinkEffect(const U* a, U answer) {
    if (ah::Noise() % 5) WmAdvance(a[3] & 0xFF);
    return answer;
}
U ByteAtEffect(const U*, U answer) {
    static const U kCells[] = {0xA1, 0xA1, 0xA0, 0xAE, 0xA2, 0x9F, 0, 0x21};
    const U n = ah::Noise();
    return (answer & 0xFFFFFF00u) | (n % 5 == 0 ? (n >> 8) & 0xFF : kCells[(n >> 4) % 8]);
}
U NameEffect(const U*, U answer) { return Key(g_names[(answer >> 4) % 4]); }
U ItemEffect(const U*, U answer) { return answer % 3 == 0 ? 0u : Key(g_items[(answer >> 4) % 4]); }
U WmPolyEffect(const U* a, U answer) {
    Scribble(a[0], 0x48);
    if (InPackets(a[0], 8)) Mem(a[0])[7] = 0x2C;
    return answer;
}
U SprtEffect(const U* a, U answer) { Scribble(a[0], 0x20); return answer; }
U ShadeEffect(const U* a, U answer) {
    if (InPackets(a[0], 8)) Mem(a[0])[7] = static_cast<unsigned char>(a[1] ? Mem(a[0])[7] | 1 : Mem(a[0])[7] & 0xFE);
    return answer;
}
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

// clones: tools/area_rows.py --unit AREA151 --clones (area 115's rows at area
// 151's addresses, less PlateStart - its plate state 0 is 0x424BA0, AR4A's -
// and with the place hook's own sites)
constexpr ah::CallSite kCalls423950[] = {{0x22, 0x57C7A0}, {0x3B, 0x57C7C0}, {0x4F, 0x536700}, {0x8E, 0x4976D0}, {0xCD, 0x591680}, {0x10C, 0x4976D0}};
constexpr ah::CallSite kCalls423A80[] = {{0x55, 0x536700}, {0x6E, 0x536700}, {0x88, 0x536700}};
constexpr ah::CallSite kCalls423B60[] = {{0x70, 0x5891F0}, {0xB2, 0x5891F0}, {0xF0, 0x5891F0}, {0x12F, 0x5891F0}};
constexpr ah::CallSite kCalls423CB0[] = {{0x0, 0x4112A0}, {0x3C, 0x5890E0}};
constexpr ah::CallSite kCalls423D00[] = {{0x0, 0x4112A0}, {0x53, 0x5890E0}};
constexpr ah::CallSite kCalls423D60[] = {{0x0, 0x4112A0}, {0x38, 0x589840}, {0x4B, 0x5890E0}};
constexpr ah::CallSite kCalls423DD0[] = {{0x0, 0x423DE0}, {0x5, 0x423EB0}};
constexpr ah::CallSite kCalls423E00[] = {{0x1C, 0x423E30}};
constexpr ah::CallSite kCalls423E30[] = {{0x1F, 0x424010}};
constexpr ah::CallSite kCalls423E60[] = {{0x38, 0x424010}};
constexpr ah::CallSite kCalls423ED0[] = {{0x59, 0x4242A0}};
constexpr ah::CallSite kCalls423F40[] = {{0x65, 0x4242A0}};
constexpr ah::CallSite kCalls423FB0[] = {{0x4E, 0x4242A0}};
constexpr ah::CallSite kCalls424010[] = {{0x22, 0x5A77C0}, {0x2B, 0x461E50}, {0x3C, 0x4241E0}, {0x51, 0x536700}, {0x7B, 0x4241E0}, {0xDC, 0x4241E0},
                                         {0xF8, 0x4241E0}, {0x15B, 0x4241E0}, {0x173, 0x531920}, {0x1A8, 0x4241E0}, {0x1B8, 0x408530}};
constexpr ah::CallSite kCalls4241E0[] = {{0x13, 0x5A77C0}, {0x1C, 0x461E50}, {0x28, 0x5A7710}, {0x3F, 0x5A7780}, {0xB1, 0x461E50}};
constexpr ah::CallSite kCalls4242A0[] = {{0x17, 0x4241E0}, {0x26, 0x4241E0}, {0x4D, 0x516B30}};
constexpr ah::CallSite kCalls424320[] = {{0x2, 0x589590}, {0x133, 0x5891F0}, {0x14F, 0x588F20}};
constexpr ah::CallSite kCalls4244A0[] = {{0x1C, 0x462A90}, {0x26, 0x589590}, {0x9B, 0x5891F0}, {0xB0, 0x589840}};
constexpr ah::CallSite kCalls424560[] = {{0xB7, 0x5A75D0}, {0xBF, 0x5A77A0}, {0x199, 0x5A85F0}, {0x19F, 0x5A9290}, {0x1AF, 0x572A00}, {0x1BB, 0x461E50},
                                         {0x21D, 0x572F70}, {0x236, 0x5A75D0}, {0x23E, 0x5A77A0}, {0x246, 0x5A7780}, {0x430, 0x572FA0}};

const ah::Clone kClones151[] = {
    W3G_C(Area151_PlaceMessage, 0x423950, 0x12B, kCalls423950, S::kState),
    W3G_C(Area151_PlateRun, 0x423A80, 0xD6, kCalls423A80, S::kState),
    W3G_C(Area151_PlateShow, 0x423B60, 0x142, kCalls423B60, S::kState),
    W3G_C(Area151_PlateGrow, 0x423CB0, 0x41, kCalls423CB0, S::kState),
    W3G_C(Area151_PlateHold, 0x423D00, 0x58, kCalls423D00, S::kState),
    W3G_C(Area151_PlateShrink, 0x423D60, 0x50, kCalls423D60, S::kState),
    W3G_P(Area151_HudRun, 0x423DB0, 0x12, S::kState),
    W3G_C(Area151_HudFrame, 0x423DD0, 0xA, kCalls423DD0, S::kState),
    W3G_P(Area151_FrameStep, 0x423DE0, 0x12, S::kCallee),
    W3G_C(Area151_FrameSlideIn, 0x423E00, 0x21, kCalls423E00, S::kState),
    W3G_C(Area151_FrameHold, 0x423E30, 0x28, kCalls423E30, S::kState),
    W3G_C(Area151_FrameSlideOut, 0x423E60, 0x41, kCalls423E60, S::kState),
    W3G_P(Area151_BoxStep, 0x423EB0, 0x12, S::kCallee),
    W3G_C(Area151_BoxSlideIn, 0x423ED0, 0x62, kCalls423ED0, S::kState),
    W3G_C(Area151_BoxHold, 0x423F40, 0x6E, kCalls423F40, S::kState),
    W3G_C(Area151_BoxSlideOut, 0x423FB0, 0x57, kCalls423FB0, S::kState),
    W3G_C(Area151_DrawFrame, 0x424010, 0x1C5, kCalls424010, S::kCallee),
    W3G_C(Area151_DrawSprite, 0x4241E0, 0xBC, kCalls4241E0, S::kCallee),
    W3G_C(Area151_DrawHud, 0x4242A0, 0x58, kCalls4242A0, S::kCallee),
    W3G_P(Area151_Record8Run, 0x424300, 0x12, S::kState),
    W3G_C(Area151_Record8Place, 0x424320, 0x154, kCalls424320, S::kState),
    W3G_P(Area151_Record4Run, 0x424480, 0x12, S::kState),
    W3G_C(Area151_Record4MarkCell, 0x4244A0, 0xB5, kCalls4244A0, S::kState),
    W3G_C(Area151_DrawDrift, 0x424560, 0x462, kCalls424560, S::kState),
};
enum : unsigned {
    kPlaceMessage, kPlateRun, kPlateShow, kPlateGrow, kPlateHold, kPlateShrink, kHudRun, kHudFrame,
    kFrameStep, kFrameSlideIn, kFrameHold, kFrameSlideOut, kBoxStep, kBoxSlideIn, kBoxHold, kBoxSlideOut, kDrawFrame,
    kDrawSprite, kDrawHud, kRecord8Run, kRecord8Place, kRecord4Run, kRecord4MarkCell, kDrawDrift,
};
static_assert(kDrawDrift + 1 == AH_COUNT(kClones151), "the copy's seeding indices");

const ah::Callee kPers4 = {"Gte_RotTransPers4", bof3::addr::Gte_RotTransPers4, KeyOf(&::Gte_RotTransPers4), 10,
                           {kAll, kAll, kAll, kAll, kAll, kAll, kAll, kAll, 0, 0}, ah::Answer::kGarbage, 0, 0,
                           {8, 8, 8, 8}, &PersEffect, nullptr};
const ah::Callee kCallees151[] = {
    {"Area151_FrameStep", at::kWm151.fn_frame_step, at::kWm151.fn_frame_step, 0, {}, ah::Answer::kPhase, 0, 0},
    {"Area151_BoxStep", at::kWm151.fn_box_step, at::kWm151.fn_box_step, 0, {}, ah::Answer::kPhase, 0, 0},
    {"Area151_DrawFrame", at::kWm151.fn_draw_frame, at::kWm151.fn_draw_frame, 2, {kAll, kU16}, ah::Answer::kGarbage, 0, 0},
    {"Area151_DrawSprite", at::kWm151.fn_draw_sprite, at::kWm151.fn_draw_sprite, 3, {kAll, kAll, kU8}, ah::Answer::kGarbage, 0, 0},
    {"Area151_DrawHud", at::kWm151.fn_draw_hud, at::kWm151.fn_draw_hud, 2, {kAll, kU16}, ah::Answer::kGarbage, 0, 0},
    kSet40, kClear40,
    {"Item_NamePtr", bof3::addr::Item_NamePtr, KeyOf(&::Item_NamePtr), 2, {kU8, kU8}, ah::Answer::kGarbage, 0, 0, {}, &NameEffect, nullptr},
    {"WorldMap_PinSprite", bof3::addr::WorldMap_PinSprite, KeyOf(&::WorldMap_PinSprite), 0, {}, ah::Answer::kGarbage, 0, 0},
    {"Effect_Release", bof3::addr::Effect_Release, KeyOf(&::Effect_Release), 0, {}, ah::Answer::kGarbage, 0, 0},
    {"Gfx_CommitPrim", bof3::addr::Gfx_CommitPrim, KeyOf(&::Gfx_CommitPrim), 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &WmCommitEffect, nullptr},
    {"WorldMap_DrawNeedle", bof3::addr::WorldMap_DrawNeedle, KeyOf(&::WorldMap_DrawNeedle), 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0},
    {"WorldMap_RecordIndex", bof3::addr::WorldMap_RecordIndex, KeyOf(&::WorldMap_RecordIndex), 0, {}, ah::Answer::kGarbage, 0, 0},
    {"Prim_SetTexture", bof3::addr::Prim_SetTexture, KeyOf(&::Prim_SetTexture), 3, {kAll, kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &TextureEffect, nullptr},
    {"AreaMap_ByteAt", bof3::addr::AreaMap_ByteAt, KeyOf(&::AreaMap_ByteAt), 2, {kU16, kU16}, ah::Answer::kGarbage, 0, 0, {}, &ByteAtEffect, nullptr},
    {"Field_CellHasEvent", bof3::addr::Field_CellHasEvent, KeyOf(&::Field_CellHasEvent), 2, {kU16, kU16}, ah::Answer::kFlag, 0, 0},
    {"MapView_ItemHalfAt", bof3::addr::MapView_ItemHalfAt, KeyOf(&::MapView_ItemHalfAt), 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &ItemEffect, nullptr},
    {"MapView_LinkPrimAt", bof3::addr::MapView_LinkPrimAt, KeyOf(&::MapView_LinkPrimAt), 4, {kAll, kAll, kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &WmLinkEffect, nullptr},
    {"Gpu_SetPolyFT4", bof3::addr::Gpu_SetPolyFT4, KeyOf(&::Gpu_SetPolyFT4), 1, {kAll}, ah::Answer::kGarbage, 0, 0, {}, &WmPolyEffect, nullptr},
    {"Gpu_SetShadeTex", bof3::addr::Gpu_SetShadeTex, KeyOf(&::Gpu_SetShadeTex), 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &ShadeEffect, nullptr},
    {"Gpu_SetSemiTrans", bof3::addr::Gpu_SetSemiTrans, KeyOf(&::Gpu_SetSemiTrans), 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &SemiEffect, nullptr},
    {"Gpu_SetSprt", bof3::addr::Gpu_SetSprt, KeyOf(&::Gpu_SetSprt), 1, {kAll}, ah::Answer::kGarbage, 0, 0, {}, &SprtEffect, nullptr},
    {"Gpu_SetDrawMode", bof3::addr::Gpu_SetDrawMode, KeyOf(&::Gpu_SetDrawMode), 5, {kAll, kAll, kAll, kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &DrawModeEffect, nullptr},
    {"Gte_PrimDepths4_10", bof3::addr::Gte_PrimDepths4_10, KeyOf(&::Gte_PrimDepths4_10), 1, {kAll}, ah::Answer::kGarbage, 0, 0, {}, &DepthEffect, nullptr},
    kPers4,
};

const ah::DataTable kTables151[] = {
    {at::kWm151.plate_states, 5}, {at::kWm151.hud_states, 2}, {at::kWm151.frame_states, 4},
    {at::kWm151.box_states, 4},   {at::kWm151.record8_states, 3}, {at::kWm151.record4_states, 2},
};

// The fuzz's own literal table addresses (read off the exe, docs/area_w3g.md
// section 5): the seed plants and the regions are built from these, never
// from the kWm151 ours reads, so a wrong constant in ours shows.
constexpr U kFzAnims = 0x636A68, kFzAnimsEnd = 0x636A84, kFzCells = 0x636A84;
constexpr U kFzRows = 0x6370BC, kFzRowsEnd = 0x63719C, kFzNameSet = 0x63719C;
constexpr U kFzDirections = 0x63725C, kFzDriftU = 0x63727C, kFzButtons = 0x637238;
constexpr unsigned kFzAnimCount = 7, kFzRowCount = 7;
constexpr U kFzPlace = 0x937F82, kFzMapMode = 0x9045FA, kFzItemsHeld = 0x9040EC, kFzTextRecords = 0x904CE0;
constexpr U kFzCellWordX = 0x802D76, kFzCellWordZ = 0x802D7A, kFzCellX = 0x905E66, kFzCellZ = 0x905E62;
constexpr U kFzButtonMap0 = 0x903580, kFzButtonMap6 = 0x90358C, kFzPartySet = 0x90412C, kFzMapHeight = 0x8CB581;
constexpr U kFzFlag3A79 = 0x903A79, kFzLeaderSteps = 0x802D49, kFzAreaText = 0x803580;

enum : unsigned { kRegPackets151 = 3, kRegItems = 7, kRegNames = 8 };
ah::Region g_regions151[] = {
    {kFzMapMode, 1}, {kFzDrawPass, 1}, {kFzPacketNext, 4}, {0, kPacketBytes}, {0x9037A0, 0x20}, {0x66C7E8, 2},
    {kFzButtonMap0, 0x10}, {0, 4 * kItemBytes}, {0, sizeof g_names}, {kFzTextRecords, 0xA0},
    {kFzAreaText, 4}, {kFzFlag3A79, 1},
    {kFzAnims, kFzAnimsEnd - kFzAnims}, {kFzCells, 4}, {kFzRows, kFzRowsEnd - kFzRows}, {kFzNameSet, 6},
    {kFzDirections, 0x18}, {kFzDriftU, 0xC},
};

// The exe's own bytes of the tables the fuzz randomises, put back two rounds
// in three (so the seeds see the shipped values most of the time).
struct Saved {
    unsigned char anims[0x1C], cells[4], rows[0xE0], names[6], dirs[0x18], drift[0xC];
};
Saved g_saved;

void SaveTables() {
    std::memcpy(g_saved.anims, Mem(kFzAnims), sizeof g_saved.anims);
    std::memcpy(g_saved.cells, Mem(kFzCells), sizeof g_saved.cells);
    std::memcpy(g_saved.rows, Mem(kFzRows), sizeof g_saved.rows);
    std::memcpy(g_saved.names, Mem(kFzNameSet), sizeof g_saved.names);
    std::memcpy(g_saved.dirs, Mem(kFzDirections), sizeof g_saved.dirs);
    std::memcpy(g_saved.drift, Mem(kFzDriftU), sizeof g_saved.drift);
}

void DisturbWm(U h) {
    const unsigned v = (h >> 8) & 0xFF;
    switch ((h >> 16) % 12) {
    case 0: Mem(kFzMapMode)[0] = static_cast<unsigned char>(v % 4 == 0 ? v : v % 3); break;
    case 1: Draw_PassFlags = static_cast<unsigned char>(Draw_PassFlags ^ (v & 1 ? 4 : 0x1B)); break;
    case 2: Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags ^ (v & 1 ? 0x100 : 0x4000)); break;
    case 3: Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 ^ 0x1000); break;
    case 4: Mem(kFzTailState)[0] = static_cast<unsigned char>(v % 3); break;
    case 5: Game_Mode = static_cast<unsigned short>(Game_Mode ^ 1); break;
    case 6: Cond_ByteFA = static_cast<signed char>(v % 16); break;
    case 7: SetWord(Mem(kFzPlace), v); break;     // the settle plants it in the last plate entry
    case 8: Mem(kFzCellX + (v & 1) * 2)[0] = static_cast<unsigned char>(v); break;
    case 9: Mem(kFzPartySet)[0] = static_cast<unsigned char>(v & 1 ? 0xC : v); break;
    case 10: case 11: Mem(v & 1 ? kFzCellWordX : kFzCellWordZ)[0] = static_cast<unsigned char>(h >> 24); break;
    default: break;
    }
}
// +1 kept inside the plate table; the place kept in the plate animations'
// last entry (the search has no bound).
void SettleWm() {
    unsigned char* const o = Sprite_Current;
    if (o[1] >= 5) o[1] = static_cast<unsigned char>(o[1] % 5);
    SetWord(Mem(kFzAnims + (kFzAnimCount - 1) * 4), Word(Mem(kFzPlace)));
}

void SeedWm(unsigned k) {
    unsigned char* const o = Sprite_Current;
    Gfx_PacketNext = g_packets + (ah::Next() % 4) * 4;
    if (ah::Often()) std::memcpy(Mem(kFzAnims), g_saved.anims, sizeof g_saved.anims);
    if (ah::Often()) std::memcpy(Mem(kFzCells), g_saved.cells, sizeof g_saved.cells);
    if (ah::Often()) std::memcpy(Mem(kFzRows), g_saved.rows, sizeof g_saved.rows);
    if (ah::Often()) std::memcpy(Mem(kFzNameSet), g_saved.names, sizeof g_saved.names);
    if (ah::Often()) std::memcpy(Mem(kFzDirections), g_saved.dirs, sizeof g_saved.dirs);
    if (ah::Often()) std::memcpy(Mem(kFzDriftU), g_saved.drift, sizeof g_saved.drift);
    o[1] = static_cast<unsigned char>(o[1] % 5);
    SetWord(Mem(kFzAnims + (kFzAnimCount - 1) * 4), Word(Mem(kFzPlace)));
    const auto leave = [o] {
        Mem(kFzMapMode)[0] = static_cast<unsigned char>(ah::Often() ? AH_PICK(0, 0, 1, 2) : ah::Next());
        if (ah::Often()) o[0xB] = static_cast<unsigned char>(ah::Half() ? 0 : ah::Next() | 1);
        if (ah::Often()) Field_Request = static_cast<unsigned char>(AH_PICK(0, 2, 5, 1, 3));
        Field_ScriptFlags = static_cast<unsigned short>(ah::Often() ? Field_ScriptFlags & ~0x100u : Field_ScriptFlags | 0x100u);
    };
    switch (k) {
    case kPlaceMessage: {
        Mem(kFzTailState)[0] = static_cast<unsigned char>(ah::Often() ? AH_PICK(0, 0, 0, 1, 1, 2, 0xFF, 0x80) : ah::Next());
        Cond_ByteFA = static_cast<signed char>(ah::Often() ? AH_PICK(1, 2, 15, 0, 0xFF, 0x80, 0x7F, 8) : ah::Next());
        if (ah::Often()) Field_Request = static_cast<unsigned char>(AH_PICK(2, 2, 0, 5, 3));
        // the place in one of the rows, or in none
        if (ah::Often()) SetWord(Mem(kFzRows + (ah::Next() % kFzRowCount) * 0x20), Word(Mem(kFzPlace)));
        // items held or not, now and then a list ended early
        for (unsigned i = 0; i < 0x74; ++i)
            if (ah::Half()) Mem(kFzItemsHeld + i)[0] = static_cast<unsigned char>(ah::Half() ? 0 : ah::Next() | 1);
        if (ah::Half()) Mem(kFzNameSet + 1 + ah::Next() % 5)[0] = static_cast<unsigned char>(AH_PICK(0xFF, 0x16, 0x5E, 0xFF));
        break;
    }
    case kPlateRun:
        if (ah::Half()) Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 ^ 0x1000);
        if (ah::Half()) Mem(kFzLeaderSteps)[0] = static_cast<unsigned char>(AH_PICK(0, 1, 0xFF, 0x80));
        break;
    case kPlateShow:
        o[0xB] = static_cast<unsigned char>(ah::Often() ? AH_PICK(1, 1, 1, 2, 3, 4, 0, 5) : ah::Next());
        SetWord(Mem(kFzAnims + (ah::Next() % kFzAnimCount) * 4), Word(Mem(kFzPlace)));
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
        if (ah::Often()) SetLong(o + 0x18, static_cast<std::int32_t>(Word(Mem(kFzPlace)) | (ah::Half() ? 0 : 0x10000u)));
        if (ah::Often()) Field_Request = static_cast<unsigned char>(AH_PICK(0, 5, 2));
        break;
    case kHudRun: o[1] = static_cast<unsigned char>(ah::Next() % 2); break;
    case kFrameStep: o[2] = static_cast<unsigned char>(ah::Next() % 4); break;
    case kBoxStep: o[3] = static_cast<unsigned char>(ah::Next() % 4); break;
    case kRecord8Run: o[1] = static_cast<unsigned char>(ah::Next() % 3); break;
    case kRecord4Run: o[1] = static_cast<unsigned char>(ah::Next() % 2); break;
    case kFrameSlideIn:
        if (ah::Often()) SetWord(o + 0x2E, AH_PICK(0, 0xFFFF, 1, 0xFFF0, 0x7FF0, 0x7FFF, 0xFFD0));
        Mem(kFzMapMode)[0] = static_cast<unsigned char>(ah::Often() ? AH_PICK(2, 0, 1) : ah::Next());
        break;
    case kFrameHold:
        Mem(kFzMapMode)[0] = static_cast<unsigned char>(ah::Often() ? AH_PICK(2, 0, 1, 0x82) : ah::Next());
        break;
    case kFrameSlideOut:
        if (ah::Often()) SetWord(o + 0x2E, AH_PICK(0xFFE0, 0xFFE1, 0xFFDF, 0x8000, 0x800F, 0x8010, 0x10));
        Mem(kFzMapMode)[0] = static_cast<unsigned char>(ah::Often() ? AH_PICK(2, 0, 1, 0x82) : ah::Next());
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
        if (ah::Often()) SetLong(Mem(kFzButtonMap0), static_cast<std::int32_t>(Word(Mem(kFzButtons + (ah::Next() % 6) * 4)) | (ah::Next() & 0xFFFF0000u)));
        if (ah::Half()) {
            // a word only the eighth entry answers, when it has such bits
            U others = 0;
            for (U e = 0; e < 7; ++e) others |= Word(Mem(kFzButtons + e * 4));
            const U only = Word(Mem(kFzButtons + 7 * 4)) & ~others;
            if (only != 0) SetLong(Mem(kFzButtonMap6), static_cast<std::int32_t>(only | (ah::Next() & 0xFFFF0000u)));
        } else if (ah::Often()) SetLong(Mem(kFzButtonMap6), static_cast<std::int32_t>(Word(Mem(kFzButtons + (ah::Half() ? 6 + ah::Next() % 2 : ah::Next() % 8) * 4)) | (ah::Next() & 0xFFFF0000u)));
        if (ah::Half()) SetLong(Mem(kFzButtonMap0), 0);
        if (ah::Half()) Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 & ~0x1000u);
        if (ah::Half()) Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags & ~0x4000u);
        if (ah::Half()) Mem(kFzPartySet)[0] = static_cast<unsigned char>(AH_PICK(0xC, 0x8C, 0xB, 0xFF));
        break;
    case kRecord8Place:
        if (ah::Often()) o[8] = static_cast<unsigned char>(ah::Next() % 4);
        if (ah::Often()) o[6] = static_cast<unsigned char>(AH_PICK(0, 1, 2, 0xFF, 1));
        break;
    case kRecord4MarkCell:
        Mem(kFzFlag3A79)[0] = static_cast<unsigned char>(ah::Often() ? AH_PICK(0, 9, 8, 0x89) : ah::Next());
        if (ah::Often()) Field_StatusBits = static_cast<unsigned char>(Field_StatusBits & ~1u);
        // the map's width so that any cell record lands in the block
        AreaMap_Header[0] = static_cast<unsigned char>(ah::Next() % 0x18);
        // the one record mostly; the next records are the descriptor's data (read unchecked)
        o[0xB] = static_cast<unsigned char>(ah::Often() ? 0 : ah::Next() % 4);
        break;
    case kDrawDrift: {
        if (ah::Half()) o[2] = 0;
        if (ah::Often()) Draw_PassFlags = static_cast<unsigned char>(Draw_PassFlags | 4);
        o[0xB] = static_cast<unsigned char>(ah::Often() ? AH_PICK(2, 3, 2, 3, 0, 1, 4, 5) : ah::Next());
        const unsigned height = Mem(kFzMapHeight)[0];
        if (ah::Half()) SetWord(o + 0x3A, height + AH_PICK(8, 9, 7, 0, 0x7FFF));
        const auto near = [](unsigned char* cell, unsigned centre) {
            SetWord(cell, centre + AH_PICK(0, 25, 26, static_cast<U>(-25), static_cast<U>(-26), 1, 100));
        };
        if (ah::Often()) near(Mem(kFzCellX), Word(o + 0x36));
        if (ah::Often()) near(Mem(kFzCellZ), Word(o + 0x3A));
        if (ah::Half()) Frame_Counter = AH_PICK(0, 15, 16, 31, 0x2F, 0xFFFFFFFFu);
        break;
    }
    default: break;
    }
}

// The draws' arguments: x and y whole words (the dial's and the box's
// positions, and anything), the sprite's index a byte with bits above it.
void ArgsWm(unsigned k, U* a) {
    if (k == kDrawFrame || k == kDrawHud || k == kDrawSprite) {
        a[0] = ah::Often() ? AH_PICK(0x10, 0x5C, 0, 0x7FF0) : ah::Next();
        a[1] = ah::Often() ? AH_PICK(0xFFD0, 0xFFFFFFD0u, 0xC8, 0xF0, 0x10) | (ah::Half() ? ah::Next() & 0xFFFF0000u : 0) : ah::Next();
        if (k == kDrawSprite) a[2] = ah::Often() ? AH_PICK(0, 1, 2, 3, 4, 5, 0x15, 0x100, 0x1FF) : ah::Next();
    }
}

}  // namespace

void SelfTest() {
    SaveTables();

    if (Wants(1480)) {
        ah::Group g{"area_w3g", kClones1480, AH_COUNT(kClones1480), kCallees1480, AH_COUNT(kCallees1480), nullptr, 0,
                    kRegions1480, AH_COUNT(kRegions1480), &Seed1480, nullptr, 6000};
        g.args = &Args1480;
        g.area = 148;
        ah::Run(g);
    }
    if (Wants(1481)) {
        g_regions1481[kRegBeamPackets].at = Key(g_packets);
        ah::Group g{"area_w3g", kClones1481, AH_COUNT(kClones1481), kCallees1481, AH_COUNT(kCallees1481), kTables1481,
                    AH_COUNT(kTables1481), g_regions1481, AH_COUNT(g_regions1481), &SeedBeam, &DisturbBeam, 6000};
        g.settle = &SettleBeam;
        g.args = &ArgsBeam;
        g.area = 148;
        ah::Run(g);
    }
    if (Wants(149)) {
        ah::Group g{"area_w3g", kClones149, AH_COUNT(kClones149), kCallees149, AH_COUNT(kCallees149), kTables149, AH_COUNT(kTables149),
                    kRegions149, AH_COUNT(kRegions149), &Seed149, &Disturb149, 6000};
        g.settle = &Settle149;
        g.args = &Args149;
        g.area = 149;
        ah::Run(g);
    }
    if (Wants(150)) {
        ah::Group g{"area_w3g", kClones150, AH_COUNT(kClones150), kCallees150, AH_COUNT(kCallees150), nullptr, 0,
                    kRegions150, AH_COUNT(kRegions150), &Seed150, nullptr, 6000};
        g.args = &Args150;
        g.area = 150;
        ah::Run(g);
    }
    if (Wants(151)) {
        g_regions151[kRegPackets151].at = Key(g_packets);
        g_regions151[kRegItems].at = Key(g_items);
        g_regions151[kRegNames].at = Key(g_names);
        ah::Group g{"area_w3g", kClones151, AH_COUNT(kClones151), kCallees151, AH_COUNT(kCallees151), kTables151, AH_COUNT(kTables151),
                    g_regions151, AH_COUNT(g_regions151), &SeedWm, &DisturbWm, 4000};
        g.settle = &SettleWm;
        g.phase_span = 5;
        g.args = &ArgsWm;
        g.area = 151;
        ah::Run(g);
    }
}

}  // namespace area_w3g

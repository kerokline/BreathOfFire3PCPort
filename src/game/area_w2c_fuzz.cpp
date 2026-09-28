// BOF3X_SHADOW=area_w2c: world 2's areas 90, 91, 92 and 94 through the area
// round's shared harness (area_harness.h), once at start-up - one
// area_harness::Run per area, each Group setting its own area number, all
// under the one shadow name. docs/area_w2c.md section 3.
//
// The clone tables are tools/area_rows.py --clones's rows for AREA090..094
// (2026-09-28), each row read against the disassembly (every start, extent and
// call site agrees); the shapes are the root table each function hangs from
// (docs/area_w2c.md section 1). The group's own callee (Area91_DrawGlow) is a
// recorder here like any other callee, so each function is fuzzed alone.
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w2c.h"
#include "game/area_w2c_callees.h"
#include "game/move_script_bytes.h"
#include "hook/log.h"

namespace area_w2c {
namespace {

namespace ah = area_harness;
using U = std::uint32_t;
using S = ah::Shape;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;

#define AH_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define AH_COUNT(a) static_cast<unsigned>(sizeof a / sizeof a[0])
#define W2C_C(name, base, size, calls, shape) \
    {#name, base, size, calls, AH_N(calls), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::name), 0, false, shape}
#define W2C_P(name, base, size, shape) \
    {#name, base, size, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::name), 0, false, shape}
// A clone answering in al (the object triggers).
#define W2C_A(name, base, size, calls, shape) \
    {#name, base, size, calls, AH_N(calls), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::name), 0xFF, false, shape}

// ---- area 90 ----
const ah::Clone kClones90[] = {
    W2C_P(Area90_ChoiceFocusPair, 0x411F10, 0x3C, S::kChoice),
};
// ---- area 91 ----
constexpr ah::CallSite kCalls411F50[] = {{0xF, 0x5919B0}, {0x24, 0x591B60}};
constexpr ah::CallSite kCalls412000[] = {{0x20, 0x5919B0}, {0x45, 0x587740}, {0x4E, 0x57C7C0}};
constexpr ah::CallSite kCalls412060[] = {{0xE, 0x587740}};
constexpr ah::CallSite kCalls4120A0[] = {{0x21, 0x57C140}, {0x34, 0x57C0F0}, {0x3B, 0x531F90}, {0x47, 0x531F90}, {0x90, 0x412140}};
constexpr ah::CallSite kCalls412140[] = {{0x7, 0x5A7B90}, {0x41, 0x5A8200}, {0x50, 0x5A8060}, {0x64, 0x5A7D70}, {0x6E, 0x5A8DE0}, {0x78, 0x5A8E00},
                                         {0x92, 0x5A77C0}, {0x9D, 0x572FA0}, {0xCA, 0x5A7A50}, {0xDC, 0x5A7A00}, {0xFB, 0x5A7A50}, {0x10D, 0x5A7A00},
                                         {0x12C, 0x5A75F0}, {0x134, 0x5A7780}, {0x15E, 0x5A84A0}, {0x164, 0x5A9310}, {0x19E, 0x572FA0}, {0x1B4, 0x5A7BC0}};
constexpr ah::CallSite kCalls412310[] = {{0xB0, 0x412140}};
constexpr ah::CallSite kCalls4123D0[] = {{0x11A, 0x412140}};
constexpr ah::CallSite kCalls412500[] = {{0x44, 0x57C7A0}, {0x5E, 0x412140}};
constexpr ah::CallSite kCalls412570[] = {{0x0, 0x57C7C0}};
constexpr ah::CallSite kCalls412590[] = {{0x5C, 0x5A8250}, {0x73, 0x5A77C0}, {0x89, 0x572FA0}, {0xFF, 0x5A7650}, {0x107, 0x5A7780}, {0x11D, 0x5A9110},
                                         {0x129, 0x5A7A00}, {0x146, 0x5A7A50}, {0x166, 0x5A7A00}, {0x183, 0x5A7A50}, {0x1B0, 0x572FA0}, {0x1E0, 0x5A7750},
                                         {0x1E8, 0x5A7780}, {0x1FA, 0x5A9110}, {0x21C, 0x5A7A00}, {0x255, 0x5A7A50}, {0x286, 0x572FA0}, {0x2D9, 0x5A77C0},
                                         {0x2EF, 0x572FA0}};
const ah::Clone kClones91[] = {
    W2C_C(Area91_ChoiceTakeItem59, 0x411F50, 0x58, kCalls411F50, S::kChoice),
    W2C_P(Area91_ChoiceMessage7, 0x411FB0, 0x24, S::kChoice),
    W2C_P(Area91_ObjectRun, 0x411FE0, 0x12, S::kHandler),
    W2C_C(Area91_State0CheckItem57, 0x412000, 0x5C, kCalls412000, S::kState),
    W2C_C(Area91_State1Arm, 0x412060, 0x32, kCalls412060, S::kState),
    W2C_C(Area91_State2Grow, 0x4120A0, 0x9A, kCalls4120A0, S::kState),
    W2C_C(Area91_DrawGlow, 0x412140, 0x1C1, kCalls412140, S::kCallee),
    W2C_C(Area91_State3Place, 0x412310, 0xBA, kCalls412310, S::kState),
    W2C_C(Area91_State4RevealClut, 0x4123D0, 0x126, kCalls4123D0, S::kState),
    W2C_C(Area91_State5Shrink, 0x412500, 0x67, kCalls412500, S::kState),
    W2C_A(Area91_Trigger49, 0x412570, 0x16, kCalls412570, S::kCallee),
    W2C_C(Area91_EffectRings, 0x412590, 0x30A, kCalls412590, S::kState),
};
enum : unsigned { k91Take, k91Msg7, k91Run, k91S0, k91S1, k91S2, k91Glow, k91S3, k91S4, k91S5, k91Trigger, k91Rings };
static_assert(k91Rings + 1 == AH_COUNT(kClones91), "area 91's seeding indices");
// ---- area 92 ----
constexpr ah::CallSite kCalls4128A0[] = {{0x21, 0x57C7C0}, {0x4B, 0x57C7C0}, {0x75, 0x57C7C0}};
constexpr ah::CallSite kCalls412940[] = {{0x1E, 0x57C7C0}};
constexpr ah::CallSite kCalls412990[] = {{0x1E, 0x57C7C0}};
constexpr ah::CallSite kCalls4129E0[] = {{0x2D, 0x57C140}, {0x39, 0x57C7C0}, {0x66, 0x57C7C0}, {0x93, 0x57C7C0}};
constexpr ah::CallSite kCallsFindFree[] = {{0xA, 0x589810}};
constexpr ah::CallSite kCalls412F90[] = {{0x4, 0x587AE0}};
constexpr ah::CallSite kCalls412FA0[] = {{0x0, 0x57C7C0}};
const ah::Clone kClones92[] = {
    W2C_C(Area92_ChoiceRunStep, 0x4128A0, 0x9F, kCalls4128A0, S::kChoice),
    W2C_C(Area92_ChoiceRun0, 0x412940, 0x4A, kCalls412940, S::kChoice),
    W2C_C(Area92_ChoiceRun28, 0x412990, 0x4B, kCalls412990, S::kChoice),
    W2C_C(Area92_ChoiceRunByFlag1B, 0x4129E0, 0xC0, kCalls4129E0, S::kChoice),
    W2C_C(Area92_SpawnKind4AtMember0, 0x412AA0, 0xAD, kCallsFindFree, S::kHandler),
    W2C_C(Area92_SpawnKind3AtMember0, 0x412B50, 0xAD, kCallsFindFree, S::kHandler),
    W2C_C(Area92_SpawnKind3AtMember1, 0x412C00, 0xAD, kCallsFindFree, S::kHandler),
    W2C_C(Area92_SpawnKind3AtMember2, 0x412CB0, 0xB1, kCallsFindFree, S::kHandler),
    W2C_C(Area92_SpawnKind1AtMember0, 0x412D70, 0xAD, kCallsFindFree, S::kHandler),
    W2C_C(Area92_SpawnKind1AtMember1, 0x412E20, 0xAD, kCallsFindFree, S::kHandler),
    W2C_C(Area92_SpawnKind1AtMember2, 0x412ED0, 0xB1, kCallsFindFree, S::kHandler),
    W2C_C(Area92_PlayMusic3D, 0x412F90, 0xD, kCalls412F90, S::kHandler),
    W2C_A(Area92_Trigger37, 0x412FA0, 0x1D, kCalls412FA0, S::kCallee),
};
enum : unsigned { k92Step, k92Run0, k92Run28, k92Flag1B, k92Spawn4M0, k92Spawn3M0, k92Spawn3M1, k92Spawn3M2, k92Spawn1M0, k92Spawn1M1, k92Spawn1M2, k92Music, k92Trigger };
static_assert(k92Trigger + 1 == AH_COUNT(kClones92), "area 92's seeding indices");
// ---- area 94 ----
constexpr ah::CallSite kCalls413000[] = {{0x22, 0x57C0F0}};
constexpr ah::CallSite kCalls413030[] = {{0x26, 0x57C140}, {0x3A, 0x57C0F0}, {0x42, 0x57C7C0}, {0x73, 0x57C140}, {0x7F, 0x57C7C0}, {0xA9, 0x57C7C0}};
constexpr ah::CallSite kCalls413110[] = {{0x29, 0x57C140}, {0x35, 0x57C7C0}, {0x5D, 0x57C7C0}, {0x87, 0x57C7C0}};
constexpr ah::CallSite kCallsSpawn2C[] = {{0x2C, 0x57CE10}};
constexpr ah::CallSite kCallsSpawn30[] = {{0x30, 0x57CE10}};
constexpr ah::CallSite kCalls413310[] = {{0x6, 0x579F00}, {0x11, 0x579F00}, {0x1C, 0x579F00}};
constexpr ah::CallSite kCalls4134E0[] = {{0x2, 0x591900}, {0xF, 0x57C0F0}};
constexpr ah::CallSite kCalls413550[] = {{0x12, 0x57C0F0}, {0x1E, 0x57C110}, {0x43, 0x571110}};
const ah::Clone kClones94[] = {
    W2C_P(Area94_ChoiceMessage0, 0x412FC0, 0x17, S::kChoice),
    W2C_P(Area94_ChoiceMessage1, 0x412FE0, 0x17, S::kChoice),
    W2C_C(Area94_ChoiceSetFlag15, 0x413000, 0x2B, kCalls413000, S::kChoice),
    W2C_C(Area94_ChoiceRunByFlags16, 0x413030, 0xD3, kCalls413030, S::kChoice),
    W2C_C(Area94_ChoiceRunByFlag20, 0x413110, 0xB1, kCalls413110, S::kChoice),
    W2C_C(Area94_SpawnKind2AtMember0, 0x4131D0, 0x42, kCallsSpawn2C, S::kHandler),
    W2C_C(Area94_SpawnKind3AtMember1, 0x413220, 0x42, kCallsSpawn2C, S::kHandler),
    W2C_C(Area94_SpawnKind3AtMember2, 0x413270, 0x46, kCallsSpawn30, S::kHandler),
    W2C_C(Area94_SpawnKind4AtMember0, 0x4132C0, 0x42, kCallsSpawn2C, S::kHandler),
    W2C_C(Area94_ClearCells, 0x413310, 0x25, kCalls413310, S::kHandler),
    W2C_C(Area94_SpawnKind1AtMember1, 0x413340, 0x42, kCallsSpawn2C, S::kHandler),
    W2C_C(Area94_SpawnKind1AtMember2, 0x413390, 0x46, kCallsSpawn30, S::kHandler),
    W2C_P(Area94_Counter1FromLeaderPose, 0x4133E0, 0xB, S::kHandler),
    W2C_C(Area94_SpawnKind3AtMember0, 0x4133F0, 0x42, kCallsSpawn2C, S::kHandler),
    W2C_C(Area94_SpawnKind4AtMember1, 0x413440, 0x42, kCallsSpawn2C, S::kHandler),
    W2C_C(Area94_SpawnKind4AtMember2, 0x413490, 0x46, kCallsSpawn30, S::kHandler),
    W2C_C(Area94_GiveKeyItem7, 0x4134E0, 0x18, kCalls4134E0, S::kHandler),
    W2C_C(Area94_SpawnKind4AtMember0E, 0x413500, 0x42, kCallsSpawn2C, S::kHandler),
    W2C_C(Area94_InitPatches, 0x413550, 0x5E, kCalls413550, S::kInit),
};
enum : unsigned {
    k94Msg0, k94Msg1, k94Flag15, k94Flags16, k94Flag20, k94S2M0, k94S3M1, k94S3M2, k94S4M0, k94Clear, k94S1M1, k94S1M2,
    k94Pose, k94S3M0, k94S4M1, k94S4M2, k94Key7, k94S4M0E, k94Init
};
static_assert(k94Init + 1 == AH_COUNT(kClones94), "area 94's seeding indices");
#undef W2C_C
#undef W2C_P
#undef W2C_A

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

// Louder than the real callees, on purpose (half the time or less, from
// Noise): after these calls the callers read Sprite_Current again (area 91's
// states after ScriptFlags_Set40 / Clear40, Sound_PlayEffect and
// Party_DropIn, which the grow state then puts back; the spawns after
// Effect_FindFree / Effect_Spawn; the rings before each link).
U MovesCurrent(const U*, U answer) {
    const U n = ah::Noise();
    if (n & 1) Sprite_Current = SomeRecord(n >> 8);
    return answer;
}
// The chapter row pointer, moved (the choices and area 94's handler 11 read it
// again after Flags_Test and KeyItem_Add).
U MovesRow(const U*, U answer) {
    const U n = ah::Noise();
    if (n & 1) SetLong(ah::Mem(at::kFlagRow), static_cast<std::int32_t>(n & 2 ? at::kStoryFlags + (n >> 8) % 0x20 : n));
    return answer;
}
U MovesRowBool(const U*, U answer) {
    const U n = ah::Noise();
    if (n & 1) SetLong(ah::Mem(at::kFlagRow), static_cast<std::int32_t>(at::kStoryFlags + (n >> 8) % 0x20));
    return answer;
}

// A choice answer: each value a choice tests, its neighbours, a negative byte
// (read signed), anything.
void SeedAnswer() {
    B(at::kChoiceAnswer) = static_cast<unsigned char>(ah::Often() ? AH_PICK(0, 0, 1, 1, 2, 3, 4, 5, 0xFF, 0x80, 0x81, 0x7F) : ah::Next());
}
// A party list byte: inside the eight-byte tables mostly, anything else now
// and then (the tables are read unchecked; .data either side).
void SeedList(U cell) { B(cell) = static_cast<unsigned char>(ah::Often() ? ah::Next() % 8 : ah::Next()); }

// The object triggers' arguments: (a field object, the story flags).
void ArgsTrigger(std::uint32_t* a) {
    a[0] = Key(ah::Object(a[0]));
    a[1] = at::kStoryFlags;
}

// ===========================================================================
// Area 90
// ===========================================================================

const ah::Region kRegions90[] = {
    {at::kFocusObject, 4},
};
void Seed90(unsigned) {
    ah::SetPointer(at::kFocusObject, SomeRecord(ah::Next()));
    SeedAnswer();
}
void Disturb90(U h) { ah::SetPointer(at::kFocusObject, SomeRecord(h >> 8)); }

// ===========================================================================
// Area 91
// ===========================================================================

// The fuzz's own packet buffer: every primitive the glow and the rings build.
constexpr unsigned kPacketBytes = 0x1000;
alignas(16) unsigned char g_packets[kPacketBytes];
bool InPackets(U p, unsigned n) { return p >= Key(g_packets) && p + n <= Key(g_packets) + kPacketBytes; }
// The packet cursor moved on by `size`, kept inside the fuzz's buffer.
void Advance(U size) {
    U next = Key(Gfx_PacketNext) + size;
    if (!InPackets(next, 0x40)) next = Key(g_packets) + (ah::Noise() % 8) * 4;
    Gfx_PacketNext = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(next));
}
// The primitive setters write the primitive's bytes, so a store the caller
// makes before the call (where the original makes it after) shows.
void Scribble(U at, unsigned n) {
    if (!InPackets(at, n)) return;
    unsigned char* const p = ah::Mem(at);
    for (unsigned i = 0; i < n; i += 4) SetLong(p + i, static_cast<std::int32_t>(ah::Noise()));
}
U LinkEffect(const U* a, U answer) {
    if (ah::Noise() % 5) Advance(a[3] & 0xFF);
    return answer;
}
U DrawModeEffect(const U* a, U answer) { Scribble(a[0], 0xC); return answer; }
U PolyG3Effect(const U* a, U answer) { Scribble(a[0], 0x34); return answer; }
U LineF2Effect(const U* a, U answer) { Scribble(a[0], 0x20); return answer; }
U Tile1Effect(const U* a, U answer) { Scribble(a[0], 0x14); return answer; }
U SemiEffect(const U* a, U answer) {
    if (InPackets(a[0], 8)) ah::Mem(a[0])[7] = static_cast<unsigned char>(a[1] ? ah::Mem(a[0])[7] | 2 : ah::Mem(a[0])[7] & 0xFD);
    return answer;
}
// A float the screen could hold: a small whole number mostly, any bits now
// and then.
float SomeFloat(U n) {
    if (n % 7 == 0) {
        float f;
        const U bits = ah::Noise();
        std::memcpy(&f, &bits, sizeof f);
        return f;
    }
    return static_cast<float>(static_cast<std::int32_t>((n >> 4) % 1200) - 400) + ((n >> 16) & 1 ? 0.5f : 0.0f);
}
// Gte_RotTransPers: the screen point written (its two floats), the depth word
// the stack local takes never read.
U PersEffect(const U* a, U answer) {
    if (a[1] == at::kScreenX) {
        const float x = SomeFloat(ah::Noise()), y = SomeFloat(ah::Noise());
        std::memcpy(ah::Mem(at::kScreenX), &x, sizeof x);
        std::memcpy(ah::Mem(at::kScreenY), &y, sizeof y);
    }
    return answer;
}
// Gte_StoreDepthF: a float into the primitive (the rings copy it to +0x1C).
U DepthEffect(const U* a, U answer) {
    if (InPackets(a[0], 4)) {
        const float f = SomeFloat(ah::Noise());
        std::memcpy(ah::Mem(a[0]), &f, sizeof f);
    }
    return answer;
}
// Math_Sin moves Sprite_Current now and then, Math_Cos its byte +9 or the
// frame counter: the rings read each again after these calls.
U SinEffect(const U* a, U answer) { return MovesCurrent(a, answer); }
U CosEffect(const U*, U answer) {
    const U n = ah::Noise();
    if (n % 8 == 0) Sprite_Current[9] = static_cast<unsigned char>(n >> 8);
    else if (n % 8 == 1) Frame_Counter = n >> 8;
    return answer;
}
// Inventory_Count answers a word the callers test whole.
U CountAnswer(const U*, U answer) {
    const U n = ah::Noise();
    return n % 3 == 0 ? answer & 0xFFFF0000u : n % 3 == 1 ? answer : (answer & 0xFFFF0000u) | ((n >> 8) & 0xFF00);
}

#define W2C_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
#define W2C_THEIRS(name) #name, KeyOf(name), KeyOf(name)
const ah::Callee kCallees91[] = {
    {W2C_OURS(Inventory_Count), 3, {kAll, kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &CountAnswer},
    {W2C_OURS(Inventory_Remove), 3, {kAll, kAll, kAll}, ah::Answer::kGarbage, 0, 0},
    {W2C_OURS(Sound_PlayEffect), 1, {kU16}, ah::Answer::kGarbage, 0, 0, {}, &MovesCurrent},
    {W2C_OURS(ScriptFlags_Set40), 0, {}, ah::Answer::kGarbage, 0, 0, {}, &MovesCurrent},
    {W2C_OURS(ScriptFlags_Clear40), 0, {}, ah::Answer::kGarbage, 0, 0, {}, &MovesCurrent},
    {W2C_OURS(Flags_Test), 2, {kAll, kU8}, ah::Answer::kBool, 0, 0},
    {W2C_OURS(Flags_Set), 2, {kAll, kU8}, ah::Answer::kGarbage, 0, 0},
    {W2C_OURS(Party_DropIn), 1, {kAll}, ah::Answer::kGarbage, 0, 0, {}, &MovesCurrent},
    // the group's own: the radius word pushed with a stale high half
    {W2C_OURS(Area91_DrawGlow), 2, {kU16, kU8}, ah::Answer::kGarbage, 0, 0},
    // the glow's matrix set-up and projection run for real on both sides
    // (they read and write the stack MATRIX and the primitive; nothing of the
    // GTE's state outlives the pair Push / Pop)
    {W2C_OURS(Gte_PushMatrix), 0, {}, ah::Answer::kThrough, 0, 0},
    {W2C_OURS(Gte_PopMatrix), 0, {}, ah::Answer::kThrough, 0, 0},
    {W2C_OURS(Gte_RotTrans), 2, {0, 0}, ah::Answer::kThrough, 0, 0},
    {W2C_OURS(Gte_RotMatrix), 2, {0, 0}, ah::Answer::kThrough, 0, 0},
    {W2C_OURS(Gte_MulMatrix0), 3, {0, 0, 0}, ah::Answer::kThrough, 0, 0},
    {W2C_OURS(Gte_SetRotMatrix), 1, {0}, ah::Answer::kThrough, 0, 0},
    {W2C_OURS(Gte_SetTransMatrix), 1, {0}, ah::Answer::kThrough, 0, 0},
    {W2C_OURS(Gte_RotTransPers3), 7, {0, 0, 0, 0, 0, 0, 0}, ah::Answer::kThrough, 0, 0},
    {W2C_OURS(Gte_PrimDepths3_10B), 1, {0}, ah::Answer::kThrough, 0, 0},
    // the rings' projection and depths recorded: the vertex logged, a screen
    // point and a depth written
    {W2C_OURS(Gte_RotTransPers), 3, {kAll, kAll, 0}, ah::Answer::kGarbage, 0, 0, {8}, &PersEffect},
    {W2C_OURS(Gte_StoreDepthF), 1, {kAll}, ah::Answer::kGarbage, 0, 0, {}, &DepthEffect},
    {W2C_OURS(Math_Sin), 1, {kAll}, ah::Answer::kGarbage, 0, 0, {}, &SinEffect},
    {W2C_OURS(Math_Cos), 1, {kAll}, ah::Answer::kGarbage, 0, 0, {}, &CosEffect},
    {W2C_OURS(Gpu_SetDrawMode), 5, {kAll, kAll, kAll, kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &DrawModeEffect},
    {W2C_OURS(Gpu_SetPolyG3), 1, {kAll}, ah::Answer::kGarbage, 0, 0, {}, &PolyG3Effect},
    {W2C_OURS(Gpu_SetLineF2), 1, {kAll}, ah::Answer::kGarbage, 0, 0, {}, &LineF2Effect},
    {W2C_OURS(Gpu_SetTile1), 1, {kAll}, ah::Answer::kGarbage, 0, 0, {}, &Tile1Effect},
    {W2C_OURS(Gpu_SetSemiTrans), 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &SemiEffect},
    {W2C_OURS(MapView_LinkPrimAt), 4, {kAll, kAll, kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &LinkEffect},
};

// Area 91's state beyond the field frame: the active member pointer,
// Cond_ByteFE, the mark, Field_Kind2Hold, the CLUT strips (both, with 0x200
// bytes either side for a row index past its row), the packet cursor and
// buffer, the vertex scratch, the screen point, the camera's and the GTE's
// matrices.
ah::Region g_regions91[] = {
    {at::kActiveMember, 4},
    {at::kCondByteFE, 1},
    {at::kAnswerMark, 1},
    {at::kKind2Hold, 1},
    {at::kClutStripSource - 0x200, 0x8400},
    {at::kPacketNext, 4},
    {0, kPacketBytes},   // g_packets, filled in at Run
    {at::kVertex0, 0x20},
    {at::kScreenX, 8},
    {at::kCameraMatrix, 0x20},
    {at::kGteMatrix, 0x20},
    {at::kGteNearZ, 4},
    {at::kGteProjDistance, 4},
    {at::kGteOffsetY, 8},
};
const ah::DataTable kTables91[] = {{at::kArea91States, at::kArea91StateCount}};

// A rotation matrix the camera could hold: nine s16 within +-0x1000 (the
// random fill's are up to eight times that, which throws every point of the
// glow to one pixel).
void SeedRotation(unsigned char* m) {
    for (unsigned i = 0; i < 9; ++i) SetWord(m + i * 2, static_cast<U>(static_cast<std::int32_t>(ah::Next() % 0x2001) - 0x1000));
}
// The glow's projection: a camera matrix and the GTE's as the field leaves
// them (rotations within +-0x1000, the GTE's translation putting the glow's
// centre a few thousand units in front), a projection distance, a near plane
// and a screen offset - two rounds in three; the random fill else.
void SeedProjection() {
    if (!ah::Often()) return;
    SeedRotation(ah::Mem(at::kCameraMatrix));
    SeedRotation(ah::Mem(at::kGteMatrix));
    for (unsigned i = 0; i < 3; ++i)
        SetLong(ah::Mem(at::kGteMatrix + 0x14 + i * 4), static_cast<std::int32_t>(ah::Next() % 0x8000) - 0x4000 + (i == 2 ? 0x6000 : 0));
    SetLong(ah::Mem(at::kGteProjDistance), static_cast<std::int32_t>(0x100 + ah::Next() % 0x300));
    SetLong(ah::Mem(at::kGteNearZ), static_cast<std::int32_t>(ah::Next() % 0x100));
    SetLong(ah::Mem(at::kGteOffsetY), static_cast<std::int32_t>(ah::Next() % 0x100));
    SetLong(ah::Mem(at::kGteOffsetY + 4), static_cast<std::int32_t>(ah::Next() % 0x200));
}

void Seed91(unsigned k) {
    ah::SetPointer(at::kActiveMember, SomeRecord(ah::Next()));
    Gfx_PacketNext = g_packets + (ah::Next() % 8) * 4;
    unsigned char* const o = Sprite_Current;
    switch (k) {
    case k91Take: case k91Msg7:
        SeedAnswer();
        if (ah::Often()) B(at::kChoiceAnswer) = 0;
        break;
    case k91Run: o[4] = static_cast<unsigned char>(ah::Next() % at::kArea91StateCount); break;
    case k91S0:
        B(at::kPartyList0) = static_cast<unsigned char>(ah::Often() ? AH_PICK(6, 3, 6, 3, 2, 4, 5, 7, 0, 0xFF) : ah::Next());
        break;
    case k91S1:
        if (ah::Often()) B(at::kLeader137) = 0;
        break;
    case k91S2:
        o[0xA] = static_cast<unsigned char>(ah::Often() ? AH_PICK(0x1D, 0x1D, 0x1C, 0x1E, 0x1F, 0xFF, 0x9D) : ah::Next());
        break;
    case k91S3:
        if (ah::Often()) Field_Kind2Hold = 0;
        break;
    case k91S4:
        o[0xA] = static_cast<unsigned char>(ah::Often() ? AH_PICK(1, 1, 2, 0, 3) : ah::Next());
        o[0xB] = static_cast<unsigned char>(ah::Often() ? AH_PICK(0xF, 0xF, 0, 1, 0xE, 0x10, 0x1F, 0x11) : ah::Next());
        break;
    case k91S5:
        o[0xA] = static_cast<unsigned char>(ah::Often() ? AH_PICK(1, 1, 2, 0) : ah::Next());
        break;
    case k91Glow: SeedProjection(); break;
    case k91Rings:
        // Cond_ByteFE 0 a quarter of the time; +9 at each ring's radius and
        // colour edges (+4 on the way in), the last colour's 0 above 0xE4
        if (ah::Next() % 4 == 0) Cond_ByteFE = 0;
        else if (Cond_ByteFE == 0) Cond_ByteFE = 1;
        o[9] = static_cast<unsigned char>(ah::Often() ? AH_PICK(0xE4, 0xE3, 0xE5, 0xFB, 0xFC, 0xFF, 0x1C, 0x1B, 0x3C, 0x3B, 0x5C, 0x5B,
                                                             0xC4, 0xC3, 0xD4, 0xD3, 0, 0x80)
                                            : ah::Next());
        break;
    default: break;
    }
}
void Args91(unsigned k, std::uint32_t* a) {
    if (k == k91Trigger) {
        ArgsTrigger(a);
    } else if (k == k91Glow) {
        // the radius word as the states push it (a tenth of 0..0x1E, 0x12C) or
        // any, the colour byte 0xFF or any
        if (ah::Often()) a[0] = (a[0] & 0xFFFF0000u) | AH_PICK(0x12C, 0, 10, 0x12C, 0x118, 0x1E * 10, 0xFFFF, 0x8000);
        if (ah::Often()) a[1] = (a[1] & 0xFFFFFF00u) | 0xFF;
    }
}
// Area 91's disturbance (from h only): the cells its states read again after
// a call.
void Disturb91(U h) {
    const auto v = static_cast<unsigned char>(h >> 20);
    switch ((h >> 8) % 6) {
    case 0: Cond_ByteFE = static_cast<unsigned char>(v & 1); break;
    case 1: ah::SetPointer(at::kActiveMember, SomeRecord(h >> 16)); break;
    case 2: Field_Kind2Hold = static_cast<unsigned char>(v & 1 ? 0 : v); break;
    case 3: Gfx_PacketNext = g_packets + (v % 16) * 4; break;
    case 4: B(at::kLeader137) = static_cast<unsigned char>(v & 1 ? 0 : v); break;
    default: B(at::kPartyList0) = static_cast<unsigned char>(v & 1 ? 6 : v); break;
    }
}
// ===========================================================================
// Area 92
// ===========================================================================

// Effect_FindFree: a slot of the first four or none (0xFF); the spawns read
// Sprite_Current again after it.
U FindFreeEffect(const U* a, U answer) { return MovesCurrent(a, answer); }
const ah::Callee kCallees92[] = {
    {W2C_OURS(ScriptFlags_Set40), 0, {}, ah::Answer::kGarbage, 0, 0},
    {W2C_OURS(Flags_Test), 2, {kAll, kU8}, ah::Answer::kBool, 0, 0},
    {W2C_OURS(Effect_FindFree), 0, {}, ah::Answer::kByte, 0xFF, 0x03, {}, &FindFreeEffect},
    {W2C_OURS(Music_Play), 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0},
};
// Beyond the field frame: every effect record a slot byte can name (a
// disturbed +0xB reaches past the four the stand-in answers), the row pointer.
const ah::Region kRegions92[] = {
    {at::kEffectObjects, 0xFF * at::kEffectStride},
    {at::kFlagRow, 4},
};
void Seed92(unsigned k) {
    switch (k) {
    case k92Step: case k92Run0: case k92Run28: case k92Flag1B: SeedAnswer(); break;
    case k92Spawn4M0: case k92Spawn3M0: case k92Spawn1M0: SeedList(at::kPartyList0); break;
    case k92Spawn3M1: case k92Spawn1M1: SeedList(at::kPartyList1); break;
    case k92Spawn3M2: case k92Spawn1M2: SeedList(at::kPartyList2); break;
    default: break;
    }
}
void Args92(unsigned k, std::uint32_t* a) {
    if (k == k92Trigger) ArgsTrigger(a);
}
void Disturb92(U h) {
    const auto v = static_cast<unsigned char>(h >> 20);
    switch ((h >> 8) % 3) {
    case 0: B(at::kPartyList0 + (v % 3)) = static_cast<unsigned char>(v % 8); break;
    case 1: B(at::kChoiceAnswer) = static_cast<unsigned char>(v % 5); break;
    default: SetLong(ah::Mem(at::kFlagRow), static_cast<std::int32_t>(at::kStoryFlags + v % 0x20)); break;
    }
}

// ===========================================================================
// Area 94
// ===========================================================================

// AreaMap_ApplyPatch: the entry's step (its high word) rewritten half the
// time, 0..3 - the walk reads the dword again after the call; the chain the
// seed builds ends inside the area block whatever the steps.
U PatchEffect(const U* a, U answer) {
    const U n = ah::Noise();
    const U entry = a[0];
    if ((n & 1) && entry >= at::kMapHeader && entry + 4 <= at::kMapHeader + 0x2000) {
        unsigned char* const p = ah::Mem(entry);
        SetWord(p + 2, (n >> 8) % 4);
    }
    return answer;
}
const ah::Callee kCallees94[] = {
    {W2C_OURS(ScriptFlags_Set40), 0, {}, ah::Answer::kGarbage, 0, 0},
    {W2C_OURS(Flags_Test), 2, {kAll, kU8}, ah::Answer::kBool, 0, 0, {}, &MovesRowBool},
    {W2C_OURS(Flags_Set), 2, {kAll, kU8}, ah::Answer::kGarbage, 0, 0},
    {W2C_OURS(Flags_Clear), 2, {kAll, kU8}, ah::Answer::kGarbage, 0, 0},
    {W2C_OURS(KeyItem_Add), 1, {kAll}, ah::Answer::kGarbage, 0, 0, {}, &MovesRow},
    {W2C_OURS(AreaMap_SetByte), 3, {kU16, kU16, kU8}, ah::Answer::kGarbage, 0, 0},
    {W2C_OURS(AreaMap_ApplyPatch), 1, {kAll}, ah::Answer::kGarbage, 0, 0, {4}, &PatchEffect},
    // the byte and the words pushed with stale high bits; slots 0..2 or none
    {W2C_THEIRS(Effect_Spawn), 5, {kU8, kU8, kU8, kU16, kU16}, ah::Answer::kByte, 0xFE, 0x02, {}, &MovesCurrent},
};
const ah::Region kRegions94[] = {
    {at::kFlagRow, 4},
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
void Seed94(unsigned k) {
    switch (k) {
    case k94Msg0: case k94Msg1: case k94Flag15: case k94Flags16: case k94Flag20: SeedAnswer(); break;
    case k94S2M0: case k94S4M0: case k94S3M0: case k94S4M0E: SeedList(at::kPartyList0); break;
    case k94S3M1: case k94S1M1: case k94S4M1: SeedList(at::kPartyList1); break;
    case k94S3M2: case k94S1M2: case k94S4M2: SeedList(at::kPartyList2); break;
    case k94Init:
        SetWord(ah::Mem(at::kLastArea), ah::Often() ? 0x79 : AH_PICK(0x78, 0x7A, 0x179, 0x7900, 0));
        SeedChain();
        break;
    default: break;
    }
}
void Disturb94(U h) {
    const auto v = static_cast<unsigned char>(h >> 20);
    switch ((h >> 8) % 3) {
    case 0: B(at::kPartyList0 + (v % 3)) = static_cast<unsigned char>(v % 8); break;
    case 1: B(at::kChoiceAnswer) = static_cast<unsigned char>(v % 5); break;
    default: SetLong(ah::Mem(at::kFlagRow), static_cast<std::int32_t>(at::kStoryFlags + v % 0x20)); break;
    }
}
#undef W2C_OURS
#undef W2C_THEIRS

void RunArea(int area, const ah::Clone* clones, unsigned n, const ah::Callee* callees, unsigned n_callees,
             const ah::DataTable* tables, unsigned n_tables, const ah::Region* regions, unsigned n_regions,
             void (*seed)(unsigned), void (*disturb)(U), void (*args)(unsigned, std::uint32_t*), void (*settle)(),
             unsigned rounds) {
    ah::Group g{"area_w2c", clones, n, callees, n_callees, tables, n_tables, regions, n_regions, seed, disturb, rounds};
    g.args = args;
    g.settle = settle;
    g.area = area;
    ah::Run(g);
}

}  // namespace

void SelfTest() {
    constexpr unsigned kRounds = 6000;
    g_regions91[6].at = Key(g_packets);
    RunArea(90, kClones90, AH_COUNT(kClones90), nullptr, 0, nullptr, 0, kRegions90, AH_COUNT(kRegions90), &Seed90, &Disturb90,
            nullptr, nullptr, kRounds);
    RunArea(91, kClones91, AH_COUNT(kClones91), kCallees91, AH_COUNT(kCallees91), kTables91, AH_COUNT(kTables91), g_regions91,
            AH_COUNT(g_regions91), &Seed91, &Disturb91, &Args91, nullptr, kRounds);
    RunArea(92, kClones92, AH_COUNT(kClones92), kCallees92, AH_COUNT(kCallees92), nullptr, 0, kRegions92, AH_COUNT(kRegions92),
            &Seed92, &Disturb92, &Args92, nullptr, kRounds);
    RunArea(94, kClones94, AH_COUNT(kClones94), kCallees94, AH_COUNT(kCallees94), nullptr, 0, kRegions94, AH_COUNT(kRegions94),
            &Seed94, &Disturb94, nullptr, nullptr, kRounds);
}

}  // namespace area_w2c

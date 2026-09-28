// BOF3X_SHADOW=area_w1a: world 1's areas 38..41 through the area round's
// shared harness (area_harness.h), once at start-up - one area_harness::Run
// per area, each Group setting its own area number, all under the one shadow
// name, the real descriptors and tables in place, the areas' own .data state
// tables swapped through DataTable. docs/area_w1a.md section "The fuzz".
//
// The clone rows are tools/area_rows.py's (--unit AREA038..AREA041 --clones,
// 2026-09-28), each read against the disassembly; the shapes are the root
// table each function hangs from (docs/area_w1a.md section 1).
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w1a.h"
#include "game/area_w1a_callees.h"
#include "game/move_script_bytes.h"
#include "hook/log.h"

namespace area_w1a {
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

unsigned char& B(U address) { return *Mem(address); }

// --- the fuzz's own memory: a packet buffer for area 40's quads ----------------

constexpr unsigned kPacketBytes = 0x200;
alignas(16) unsigned char g_packets[kPacketBytes];

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

// A record a pointer the areas follow may name: one of the four party objects
// (Sprite_ObjectsExtra), a field object, a party record, or the running
// object itself.
unsigned char* MemberRecord(U v) {
    switch (v % 4) {
    case 0: return Mem(0x802000 + (v >> 2) % 4 * 0xA4);
    case 1: return ah::Object(v >> 2);
    case 2: return ah::PartyOf(static_cast<unsigned char>(v >> 2));
    default: return Sprite_Current;
    }
}
unsigned char* FlagRowAt(U v) { return Mem(0x903F90 + 8 * (v % 0x3A)); }

// ===========================================================================
// The effects: the callees whose callers read a cell again after the call
// move that cell half the time (from Noise), louder than the real callees on
// purpose - the harness's own disturbance reaches a group cell about one call
// in 24.
// ===========================================================================

// Effect_FindFree: area 38 reads Sprite_Current and its party-list byte again
// after it, area 39 Field_ActiveMember.
U FindFreeEffect(const U*, U answer) {
    const U n = ah::Noise();
    if (n & 1) Sprite_Current = MemberRecord(n >> 8);
    if (n & 2) B(n & 4 ? at::kPartyList1 : at::kPartyList2) = static_cast<unsigned char>(n & 8 ? 0 : n >> 16);
    if (n & 0x10) ah::SetPointer(at::kActiveMember, MemberRecord(n >> 12));
    return answer;
}
// Party_DropIn: area 41's choice reads the pointer 0x903804 after it, area
// 40's tail Field_ScriptFlags' high byte and (its way out) the state, the
// area, Cond_ByteFD.
U DropInEffect(const U*, U answer) {
    const U n = ah::Noise();
    if (n & 1) ah::SetPointer(at::kFocusObject, ah::Object(n >> 8));
    if (n & 2) B(at::kScriptFlagsHigh) = static_cast<unsigned char>(B(at::kScriptFlagsHigh) ^ 0x10);
    if (n & 4) B(at::kTailState) = static_cast<unsigned char>(n & 8 ? 0xA : 1);
    if (n & 0x10) Cond_ByteFD = static_cast<unsigned char>(n & 0x20 ? 6 : 5);
    return answer;
}
// Flags_Test: area 40's init reads Cond_ByteFD and the entry zone after it,
// its tail the state, area 41's choice the row pointer.
U TestEffect(const U*, U answer) {
    const U n = ah::Noise();
    if (n & 1) Cond_ByteFD = static_cast<unsigned char>(n & 2 ? 2 : 6);
    if (n & 4) B(at::kEntryZone) = static_cast<unsigned char>(n & 8 ? 4 : 6);
    if (n & 0x10) ah::SetPointer(at::kFlagRow, FlagRowAt(n >> 8));
    if (n & 0x20) B(at::kTailState) = static_cast<unsigned char>(n & 0x40 ? 0xA : 0);
    return answer;
}
// Flags_Set: area 40's choice 0 reads Field_StatusBits after it.
U SetEffect(const U*, U answer) {
    const U n = ah::Noise();
    if (n & 1) Field_StatusBits = static_cast<unsigned char>(Field_StatusBits ^ (n >> 8));
    return answer;
}
// Flags_Clear: area 40's init reads the entry zone after it.
U ClearEffect(const U*, U answer) {
    const U n = ah::Noise();
    if (n & 1) B(at::kEntryZone) = static_cast<unsigned char>(n & 2 ? 4 : 6);
    return answer;
}
// AreaMap_SetByte: area 40's init reads Cond_ByteFD after its gate loop.
U SetByteEffect(const U*, U answer) {
    const U n = ah::Noise();
    if (n % 8 == 0) Cond_ByteFD = static_cast<unsigned char>(n & 0x100 ? 6 : 1);
    return answer;
}
// Inventory_Add: area 41's choice reads the row pointer again after it.
U AddEffect(const U*, U answer) {
    const U n = ah::Noise();
    if (n & 1) ah::SetPointer(at::kFlagRow, FlagRowAt(n >> 8));
    return answer;
}
// 0x57CD90: area 40's handler reads MoveScript_Object after it.
U FreeObjectEffect(const U*, U answer) {
    const U n = ah::Noise();
    if (n & 1) ah::SetPointer(at::kScriptObject, ah::Object(n >> 8));
    return answer;
}
// Area40_DrawGrid (a stand-in in the tail's fuzz): the tail's way out reads
// the area, the state and Cond_ByteFD after it.
U DrawGridEffect(const U*, U answer) {
    const U n = ah::Noise();
    switch (n % 4) {
    case 0: B(at::kTailState) = static_cast<unsigned char>(n & 0x100 ? 0xA : 2); break;
    case 1: Game_AreaNumber = static_cast<unsigned short>(n & 0x100 ? 0x28 : 0x29); break;
    case 2: Cond_ByteFD = static_cast<unsigned char>(n & 0x100 ? 6 : 7); break;
    default: break;
    }
    return answer;
}
// Area40_TileLit (a stand-in in the hook's and Area40_PuzzleSolved's fuzz):
// in the grid, the pattern's byte for the cell 31 times in 32 (so the "all
// sixteen equal" path runs about half the time), else the other value or any
// byte.
U TileLitEffect(const U* a, U answer) {
    const U x = (a[0] & 0xFFFF) - at::kGridX, z = (a[1] & 0xFFFF) - at::kGridZ;
    const U n = ah::Noise();
    if (x >= at::kGridSide || z >= at::kGridSide || n % 32 == 0) return answer;
    const U want = B(at::kArea40Pattern + z * 4 + x);
    return (answer & 0xFFFFFF00u) | (n % 64 == 1 ? want ^ 1 : want);
}
// AreaMap_ByteAt: the grid's lit byte 0x50 three times in four.
U ByteAtEffect(const U*, U answer) {
    return (answer & 0xFFFFFF00u) | (ah::Noise() % 4 ? 0x50u : (answer >> 8) & 0xFF);
}
// MapView_ItemAt: no item a third of the time, else item 1..3 (DrawItems'
// first eight halves are the group's region).
U ItemEffect(const U*, U answer) { return answer % 3 == 0 ? 0u : 1 + (answer >> 4) % 3; }
// The primitive setters write the primitive's bytes, so a store the caller
// makes before the call (where the original makes it after) shows.
U PolyEffect(const U* a, U answer) {
    Scribble(a[0], 0x34);
    return answer;
}
U SemiEffect(const U* a, U answer) {
    if (InPackets(a[0], 8)) Mem(a[0])[7] = static_cast<unsigned char>(a[1] ? Mem(a[0])[7] | 2 : Mem(a[0])[7] & 0xFD);
    return answer;
}
U LinkEffect(const U* a, U answer) {
    if (ah::Noise() % 5) Advance(a[3] & 0xFF);
    return answer;
}
// Inventory_Count: its word 0 a quarter of the time (garbage above).
U CountEffect(const U*, U answer) { return (answer & 3) == 0 ? answer & 0xFFFF0000u : answer; }

#define W1A_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
const ah::Callee kCallees[] = {
    // named, ours, beyond the standard set
    {W1A_OURS(Effect_FindFree), 0, {}, ah::Answer::kByte, 0xFF, 0x03, {}, &FindFreeEffect},
    {W1A_OURS(ScriptFlags_Set40), 0, {}, ah::Answer::kGarbage, 0, 0},
    {W1A_OURS(ScriptFlags_Clear40), 0, {}, ah::Answer::kGarbage, 0, 0},
    {W1A_OURS(EventOp_0x), 1, {kAll}, ah::Answer::kGarbage, 0, 0},
    {W1A_OURS(MoveCmd_TestFB), 2, {kU16, kU16}, ah::Answer::kGarbage, 0, 0},   // cells pushed with stale high halves
    {W1A_OURS(MoveCmd_TestFC), 2, {kU16, kU16}, ah::Answer::kGarbage, 0, 0},
    {W1A_OURS(Gpu_SetPolyF4), 1, {kAll}, ah::Answer::kGarbage, 0, 0, {}, &PolyEffect},
    {W1A_OURS(Kind2_Place), 1, {kU8}, ah::Answer::kGarbage, 0, 0},
    // standard ones listed again with what the callee reads or the caller reads after
    {W1A_OURS(AreaMap_SetByte), 3, {kU16, kU16, kU8}, ah::Answer::kGarbage, 0, 0, {}, &SetByteEffect},   // s16 x, s16 z, the byte
    {W1A_OURS(Party_DropIn), 1, {kAll}, ah::Answer::kGarbage, 0, 0, {}, &DropInEffect},
    {W1A_OURS(Flags_Test), 2, {kAll, kU8}, ah::Answer::kBool, 0, 0, {}, &TestEffect},
    {W1A_OURS(Flags_Set), 2, {kAll, kU8}, ah::Answer::kGarbage, 0, 0, {}, &SetEffect},
    {W1A_OURS(Flags_Clear), 2, {kAll, kU8}, ah::Answer::kGarbage, 0, 0, {}, &ClearEffect},
    {W1A_OURS(Inventory_Add), 3, {kAll, kAll, kAll}, ah::Answer::kFlag, 0, 0, {}, &AddEffect},
    {W1A_OURS(Inventory_Count), 3, {kAll, kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &CountEffect},
    {W1A_OURS(AreaMap_ByteAt), 2, {kU16, kU16}, ah::Answer::kGarbage, 0, 0, {}, &ByteAtEffect},
    {W1A_OURS(MapView_ItemAt), 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &ItemEffect},
    {W1A_OURS(Gpu_SetSemiTrans), 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &SemiEffect},
    {W1A_OURS(MapView_LinkPrimAt), 4, {kAll, kAll, kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &LinkEffect},
    // the group's own, called directly
    {W1A_OURS(Area40_SetGate), 0, {}, ah::Answer::kGarbage, 0, 0},
    {W1A_OURS(Area40_DrawGrid), 0, {}, ah::Answer::kGarbage, 0, 0, {}, &DrawGridEffect},
    {W1A_OURS(Area40_ClearGrid), 0, {}, ah::Answer::kPhase, 0, 0},
    {W1A_OURS(Area40_TileLit), 2, {kU16, kU16}, ah::Answer::kFlag, 0, 0, {}, &TileLitEffect},   // words, pushed with stale high halves
    {W1A_OURS(Area40_MarkBehind), 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0},
    {W1A_OURS(Area40_PuzzleSolved), 0, {}, ah::Answer::kFlag, 0, 0},
    {W1A_OURS(Area41_TintUp), 0, {}, ah::Answer::kPhase, 0, 0},
    // group SX's, by raw address
    {"FreeObject_57CD90", at::kFreeObject, at::kFreeObject, 0, {}, ah::Answer::kByte, 0xFF, 0x1D, {}, &FreeObjectEffect},
    {"KeyItemAdd_591900", at::kKeyItemAdd, at::kKeyItemAdd, 1, {kAll}, ah::Answer::kGarbage, 0, 0},
    {"InventoryTake_591B60", at::kInventoryTake, at::kInventoryTake, 4, {kAll, kAll, kAll, kAll}, ah::Answer::kGarbage, 0, 0},
};
#undef W1A_OURS

// Beyond the field frame: the effect records the spawns write (slots 0..3),
// the pointers the areas follow, the key items, the button word, area 40's
// view (origin, ring offsets, the ring's cells), its draw (the packet cursor
// and buffer, DrawItems' first eight halves) and area 41's tint records (232
// of them: with Effect_Objects after them every byte index lands in a region).
ah::Region g_regions[] = {
    {at::kEffectObjects, 4 * at::kEffectStride},
    {at::kActiveMember, 4},
    {at::kScriptObject, 4},
    {at::kFlagRow, 4},
    {at::kFocusObject, 4},
    {at::kAnswerMark, 1},
    {at::kKeyItems, at::kKeyItemCount},
    {at::kInputHeld, 4},
    {at::kPacketNext, 4},
    {0, kPacketBytes},            // g_packets (set at start-up)
    {at::kOrigin, 4},
    {at::kViewColumn, 6},
    {at::kViewCells, at::kViewCellCount * 2},
    {at::kDrawItems, 8 * at::kDrawItemStride},
    {at::kTintRecords, 0xAE0},
};

void Common() {
    ah::SetPointer(at::kActiveMember, MemberRecord(ah::Next()));
    ah::SetPointer(at::kScriptObject, ah::Half() ? ah::Object(ah::Next()) : ah::PartyOf(static_cast<unsigned char>(ah::Next())));
    ah::SetPointer(at::kFocusObject, ah::Object(ah::Next()));
    ah::SetPointer(at::kFlagRow, FlagRowAt(ah::Next()));
    Gfx_PacketNext = g_packets + (ah::Next() % 4) * 4;
}

// The group's cells, moved by the harness's disturbance about one call in 24
// - drawn only from h (area_harness.h: a group disturb never draws Next).
void Disturb(U h) {
    const auto v = static_cast<unsigned char>(h >> 20);
    switch ((h >> 8) % 12) {
    case 0: B(at::kTailState) = static_cast<unsigned char>(h & 0x10000 ? v : (h & 0x20000 ? 0xA : v % 3)); break;
    case 1: Cond_ByteFD = static_cast<unsigned char>(h & 0x10000 ? v : (h & 0x20000 ? 6 : 2)); break;
    case 2: B(at::kEntryZone) = static_cast<unsigned char>(h & 0x10000 ? v : (h & 0x20000 ? 4 : 6)); break;
    case 3: Game_AreaNumber = static_cast<unsigned short>(h & 0x10000 ? 0x28 : 0x29); break;
    case 4: Field_StatusBits = static_cast<unsigned char>(Field_StatusBits ^ v); break;
    case 5: ah::SetPointer(at::kFlagRow, FlagRowAt(h >> 16)); break;
    case 6: ah::SetPointer(at::kFocusObject, ah::Object(h >> 16)); break;
    case 7: ah::SetPointer(at::kActiveMember, MemberRecord(h >> 16)); break;
    case 8: B(at::kPartyList0 + (h >> 16) % 3) = static_cast<unsigned char>(h & 0x40000 ? v : (h & 0x80000 ? 0 : 4)); break;
    case 9: Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags ^ (h & 0x10000 ? 0x1000 : 0x8)); break;
    case 10: ah::SetPointer(at::kScriptObject, ah::Object(h >> 16)); break;
    default: Gfx_BufferIndex = static_cast<unsigned char>(h & 0x10000 ? Gfx_BufferIndex ^ 1 : v); break;
    }
}

// A choice answer: each value a handler tests, its neighbours, a negative
// byte, anything.
void SeedAnswer() {
    if (ah::Often()) B(at::kChoiceAnswer) = static_cast<unsigned char>(AH_PICK(0, 0, 1, 2, 0xFF, 0x80, 0x7F, 3));
}

// ===========================================================================
// Area 38
// ===========================================================================

constexpr ah::CallSite kCalls4053B0[] = {{0x6, 0x579F00}, {0x11, 0x579F00}, {0x1C, 0x579F00}};
constexpr ah::CallSite kCalls4053E0[] = {{0x9, 0x579F00}, {0x17, 0x579F00}, {0x25, 0x579F00}};
constexpr ah::CallSite kCalls405410[] = {{0x17, 0x589810}};
constexpr ah::CallSite kCalls4054D0[] = {{0x17, 0x589810}};

const ah::Clone kClones38[] = {
    {"Area38_ClearCells", 0x4053B0, 0x25, kCalls4053B0, AH_N(kCalls4053B0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area38_ClearCells), 0, false, S::kHandler},
    {"Area38_SetCellsC0", 0x4053E0, 0x2E, kCalls4053E0, AH_N(kCalls4053E0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area38_SetCellsC0), 0, false, S::kHandler},
    {"Area38_SpawnEffectMember1", 0x405410, 0xBA, kCalls405410, AH_N(kCalls405410), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area38_SpawnEffectMember1), 0, false, S::kHandler},
    {"Area38_SpawnEffectMember2", 0x4054D0, 0xBE, kCalls4054D0, AH_N(kCalls4054D0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area38_SpawnEffectMember2), 0, false, S::kHandler},
};
enum : unsigned { k38Clear, k38Set, k38Member1, k38Member2 };

void Seed38(unsigned k) {
    Common();
    if (k == k38Member1 || k == k38Member2) {
        const U list = k == k38Member1 ? at::kPartyList1 : at::kPartyList2;
        if (ah::Often()) B(list) = 0;
        else if (ah::Half()) B(list) = static_cast<unsigned char>(AH_PICK(1, 7, 0x80, 0xFF));
    }
}

// ===========================================================================
// Area 39
// ===========================================================================

constexpr ah::CallSite kCalls405650[] = {{0x1, 0x589810}};
constexpr ah::CallSite kCalls4056A0[] = {{0x6, 0x579F00}, {0x11, 0x579F00}};
constexpr ah::CallSite kCalls4056C0[] = {{0x6, 0x579F00}, {0x11, 0x579F00}};
constexpr ah::CallSite kCalls4056E0[] = {{0x6, 0x579F00}, {0x11, 0x579F00}};
constexpr ah::CallSite kCalls405700[] = {{0x7, 0x587AE0}};
constexpr ah::CallSite kCalls405720[] = {{0x16, 0x591900}};
constexpr ah::CallSite kCalls405750[] = {{0x2, 0x587B40}};

const ah::Clone kClones39[] = {
    {"Area39_Run", 0x405590, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area39_Run), 0, false, S::kHandler},
    {"Area39_DriftStart", 0x4055B0, 0x22, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area39_DriftStart), 0, false, S::kState},
    {"Area39_DriftStep", 0x4055E0, 0x62, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area39_DriftStep), 0, false, S::kState},
    {"Area39_SpawnEffect36", 0x405650, 0x41, kCalls405650, AH_N(kCalls405650), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area39_SpawnEffect36), 0, false, S::kHandler},
    {"Area39_ClearCells", 0x4056A0, 0x1A, kCalls4056A0, AH_N(kCalls4056A0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area39_ClearCells), 0, false, S::kHandler},
    {"Area39_SetCells51", 0x4056C0, 0x1A, kCalls4056C0, AH_N(kCalls4056C0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area39_SetCells51), 0, false, S::kHandler},
    {"Area39_SetCells50", 0x4056E0, 0x1A, kCalls4056E0, AH_N(kCalls4056E0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area39_SetCells50), 0, false, S::kHandler},
    {"Area39_PlayMusicA3", 0x405700, 0x10, kCalls405700, AH_N(kCalls405700), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area39_PlayMusicA3), 0, false, S::kHandler},
    {"Area39_SetScriptFlag8", 0x405710, 0x8, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area39_SetScriptFlag8), 0, false, S::kHandler},
    {"Area39_SwapKeyItemE", 0x405720, 0x21, kCalls405720, AH_N(kCalls405720), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area39_SwapKeyItemE), 0, false, S::kHandler},
    {"Area39_FadeOutMusic", 0x405750, 0x9, kCalls405750, AH_N(kCalls405750), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area39_FadeOutMusic), 0, false, S::kHandler},
    {"Area39_Counter1Not4", 0x405760, 0x10, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area39_Counter1Not4), 0, false, S::kHandler},
};
enum : unsigned { k39Run, k39DriftStart, k39DriftStep, k39Spawn, k39Clear, k39Set51, k39Set50, k39Music, k39Flag8, k39KeyItem, k39Fade, k39Counter };
const ah::DataTable kTables39[] = {{at::kArea39States, at::kArea39StateCount}};

void Seed39(unsigned k) {
    Common();
    switch (k) {
    case k39Run: Sprite_Current[4] = static_cast<unsigned char>(ah::Next() % at::kArea39StateCount); break;
    case k39DriftStep:
        if (ah::Half()) Sprite_Current[0xA] = static_cast<unsigned char>(AH_PICK(0, 0, 1, 0x40, 0x10, 0x11));
        break;
    case k39KeyItem:
        // no 0xE a third of the time, else one or two planted
        for (unsigned i = 0; i < at::kKeyItemCount; ++i)
            if (B(at::kKeyItems + i) == 0xE) B(at::kKeyItems + i) = 0xF;
        if (ah::Often()) B(at::kKeyItems + ah::Next() % at::kKeyItemCount) = 0xE;
        if (ah::Half()) B(at::kKeyItems + ah::Next() % at::kKeyItemCount) = 0xE;
        break;
    case k39Counter:
        if (ah::Often()) B(at::kPartyList0) = static_cast<unsigned char>(AH_PICK(4, 4, 3, 5, 0x84, 0));
        break;
    default: break;
    }
}

// ===========================================================================
// Area 40
// ===========================================================================

constexpr ah::CallSite kCalls405770[] = {{0x0, 0x57CD90}, {0x1A, 0x57A010}};
constexpr ah::CallSite kCalls4057C0[] = {{0x24, 0x57C0F0}};
constexpr ah::CallSite kCalls405810[] = {{0x19, 0x57C0F0}, {0x1E, 0x405910}, {0x28, 0x587740}};
constexpr ah::CallSite kCalls405850[] = {{0x19, 0x57C0F0}, {0x1E, 0x405910}, {0x28, 0x587740}};
constexpr ah::CallSite kCalls405890[] = {{0x19, 0x57C0F0}, {0x1E, 0x405910}, {0x28, 0x587740}};
constexpr ah::CallSite kCalls4058D0[] = {{0x19, 0x57C0F0}, {0x1E, 0x405910}, {0x28, 0x587740}};
constexpr ah::CallSite kCalls405910[] = {{0x1C, 0x579F00}, {0x26, 0x579F00}, {0x30, 0x579F00}, {0x3A, 0x579F00}, {0x60, 0x579F00}, {0x6D, 0x579F00}, {0x7A, 0x579F00}, {0x87, 0x579F00}};
constexpr ah::CallSite kCalls4059B0[] = {{0x24, 0x57C7C0}};
constexpr ah::CallSite kCalls405A10[] = {{0x54, 0x405ED0}, {0x7B, 0x57C7A0}, {0x87, 0x57C140}, {0x95, 0x531F90}};
constexpr ah::CallSite kCalls405B00[] = {{0x22, 0x57C140}, {0x37, 0x405E30}, {0x5B, 0x405BF0}, {0x69, 0x572650}, {0x70, 0x572790}, {0x82, 0x405E80}, {0x8C, 0x587740}, {0x99, 0x405D30}, {0xBF, 0x572790}};
constexpr ah::CallSite kCalls405D30[] = {{0x34, 0x405BF0}, {0x73, 0x57C0F0}, {0x84, 0x579F00}, {0x95, 0x579F00}, {0xA6, 0x579F00}, {0xB7, 0x579F00}, {0xCA, 0x572650}, {0xD7, 0x587740}};
constexpr ah::CallSite kCalls405E30[] = {{0x21, 0x579F00}};
constexpr ah::CallSite kCalls405E80[] = {{0x45, 0x579F00}};
constexpr ah::CallSite kCalls405ED0[] = {{0x4A, 0x536700}, {0x64, 0x572ED0}, {0x7D, 0x5A75B0}, {0x85, 0x5A7780}, {0x192, 0x572FA0}};
constexpr ah::CallSite kCalls4060C0[] = {{0x2A, 0x579F00}, {0x37, 0x579F00}, {0x44, 0x579F00}, {0x51, 0x579F00}, {0x72, 0x57C140}, {0x87, 0x579F00}, {0x95, 0x579F00}, {0xA3, 0x579F00}, {0xB1, 0x579F00}, {0xDA, 0x57C110}, {0x119, 0x572650}};

const ah::Clone kClones40[] = {
    {"Area40_PlaceObject", 0x405770, 0x2F, kCalls405770, AH_N(kCalls405770), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area40_PlaceObject), 0, false, S::kHandler},
    {"Area40_NudgeObject", 0x4057A0, 0x1B, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area40_NudgeObject), 0, false, S::kHandler},
    {"Area40_ChoiceFlag5", 0x4057C0, 0x42, kCalls4057C0, AH_N(kCalls4057C0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area40_ChoiceFlag5), 0, false, S::kChoice},
    {"Area40_ChoiceLeverB", 0x405810, 0x31, kCalls405810, AH_N(kCalls405810), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area40_ChoiceLeverB), 0, false, S::kChoice},
    {"Area40_ChoiceLeverA", 0x405850, 0x31, kCalls405850, AH_N(kCalls405850), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area40_ChoiceLeverA), 0, false, S::kChoice},
    {"Area40_ChoiceLever9", 0x405890, 0x31, kCalls405890, AH_N(kCalls405890), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area40_ChoiceLever9), 0, false, S::kChoice},
    {"Area40_ChoiceLever8", 0x4058D0, 0x31, kCalls4058D0, AH_N(kCalls4058D0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area40_ChoiceLever8), 0, false, S::kChoice},
    {"Area40_SetGate", 0x405910, 0x9B, kCalls405910, AH_N(kCalls405910), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area40_SetGate), 0xFF, false, S::kCallee},
    {"Area40_ChoiceTail16", 0x4059B0, 0x5A, kCalls4059B0, AH_N(kCalls4059B0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area40_ChoiceTail16), 0, false, S::kChoice},
    {"Area40_TailPuzzle", 0x405A10, 0xE8, kCalls405A10, AH_N(kCalls405A10), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area40_TailPuzzle), 0, false, S::kTail},
    {"Area40_ArriveHook", 0x405B00, 0xE2, kCalls405B00, AH_N(kCalls405B00), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area40_ArriveHook), 0xFF, false, S::kHook},
    {"Area40_TileLit", 0x405BF0, 0x138, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area40_TileLit), 0xFF, false, S::kCallee},
    {"Area40_PuzzleSolved", 0x405D30, 0xF1, kCalls405D30, AH_N(kCalls405D30), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area40_PuzzleSolved), 0xFF, false, S::kCallee},
    {"Area40_ClearGrid", 0x405E30, 0x42, kCalls405E30, AH_N(kCalls405E30), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area40_ClearGrid), 0, false, S::kCallee},
    {"Area40_MarkBehind", 0x405E80, 0x4E, kCalls405E80, AH_N(kCalls405E80), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area40_MarkBehind), 0, false, S::kCallee},
    {"Area40_DrawGrid", 0x405ED0, 0x1E4, kCalls405ED0, AH_N(kCalls405ED0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area40_DrawGrid), 0, false, S::kCallee},
    {"Area40_Init", 0x4060C0, 0x13B, kCalls4060C0, AH_N(kCalls4060C0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area40_Init), 0, false, S::kInit},
};
enum : unsigned {
    k40Place, k40Nudge, k40Flag5, k40LeverB, k40LeverA, k40Lever9, k40Lever8, k40Gate, k40Tail16, k40Tail,
    k40Hook, k40TileLit, k40Solved, k40ClearGrid, k40Behind, k40Draw, k40Init
};

// Area40_TileLit's view and runs: the ring offsets inside their range (one
// wrap, as the original assumes), four runs of area-block records planted at
// dword indexes 0x140, 0x180, 0x1C0, 0x200 (their counts, kinds 0x23..0x25 or
// another, steps of 1 or 2 landing exactly on the run's last dword), their
// patch entries (0x8000 under 0xF000 half the time), and every ring cell
// either 0 or one of the four runs less AreaMap_CellBase.
void PlantRuns() {
    MapView_Row = static_cast<short>(static_cast<int>(ah::Next() % 0x38) - 1);
    MapView_Column = static_cast<short>(static_cast<int>(ah::Next() % 0x1C) - 1);
    const U base = ah::Next() % 0x40;
    SetLong(Mem(at::kCellBase), static_cast<std::int32_t>((ah::Next() & 0xFFFF0000u) | base));
    const U patch = 0x600 + ah::Next() % 0x40;
    SetLong(Mem(at::kPatchBase), static_cast<std::int32_t>((ah::Next() & 0xFFFF0000u) | patch));
    static constexpr U kRuns[4] = {0x140, 0x180, 0x1C0, 0x200};
    for (U run : kRuns) {
        const U n = ah::Next() % 7;
        unsigned char* const head = AreaMap_Header + (run - 1) * 4;
        SetLong(head, static_cast<std::int32_t>((ah::Next() & 0xFFFF) | (n + 1) << 16));
        for (U at_ = 0; at_ < n;) {
            const U step = n - at_ == 1 || ah::Half() ? 1 : 2;
            const U kind = AH_PICK(0x23, 0x24, 0x25, 0x23, 0x22, 0x26, 0xA3, 0x03);
            const U lo = ah::Next() % 0x40;
            SetLong(AreaMap_Header + (run + at_) * 4, static_cast<std::int32_t>(kind << 24 | step << 16 | lo));
            unsigned char* const entry = AreaMap_Header + (patch + lo) * 4;
            if (ah::Half()) SetLong(entry, static_cast<std::int32_t>((Long(entry) & 0xFFFF0FFF) | 0x8000));
            at_ += step;
        }
    }
    for (unsigned i = 0; i < at::kViewCellCount; ++i)
        SetWord(Mem(at::kViewCells + i * 2), ah::Next() % 4 == 0 ? 0u : kRuns[ah::Next() % 4] - base);
}

void Seed40(unsigned k) {
    Common();
    switch (k) {
    case k40Nudge:
        if (ah::Often()) Field_State[0x89] = static_cast<unsigned char>(AH_PICK(2, 2, 1, 3, 0x82));
        break;
    case k40Flag5: case k40LeverB: case k40LeverA: case k40Lever9: case k40Lever8: SeedAnswer(); break;
    case k40Gate:
        if (ah::Often()) B(at::kLeverNibble) = static_cast<unsigned char>((ah::Next() & 0xF0) | AH_PICK(5, 5, 4, 6, 0xD, 0x7));
        break;
    case k40Tail16:
        SeedAnswer();
        if (ah::Often()) B(at::kLeaderByte89) = static_cast<unsigned char>(AH_PICK(2, 1, 3, 0x82));
        if (ah::Often()) B(at::kTailKind) = static_cast<unsigned char>(AH_PICK(0, 0, 0x10, 1));
        break;
    case k40Tail:
        if (ah::Often()) B(at::kTailState) = static_cast<unsigned char>(AH_PICK(0, 1, 2, 2, 3, 0xFF, 0xA));
        if (ah::Often()) B(at::kCounter0) = static_cast<unsigned char>(AH_PICK(0x63, 0x63, 0x62, 0x64, 0xE3));
        if (ah::Often()) Field_Request = static_cast<unsigned char>(AH_PICK(0, 0, 1, 2));
        if (ah::Half()) Game_AreaNumber = static_cast<unsigned short>(AH_PICK(0x29, 0x128, 0x27));
        if (ah::Often()) Cond_ByteFD = static_cast<unsigned char>(AH_PICK(6, 6, 5, 7, 0x86));
        break;
    case k40Hook:
        // half the rounds every gate passes but at most one, drawn to fail
        if (ah::Half()) {
            Cond_ByteFD = 6;
            Field_State[0x2B] = 0;
            B(at::kTailKind) = 0x10;
            if (ah::Half()) {
                switch (ah::Next() % 3) {
                case 0: Cond_ByteFD = static_cast<unsigned char>(AH_PICK(5, 7, 0x86)); break;
                case 1: Field_State[0x2B] = static_cast<unsigned char>(AH_PICK(1, 0x80)); break;
                default: B(at::kTailKind) = static_cast<unsigned char>(AH_PICK(0xF, 0x11, 0x90)); break;
                }
            }
        }
        break;
    case k40TileLit: PlantRuns(); break;
    case k40Behind:
        if (ah::Often()) B(at::kLeaderByte8) = static_cast<unsigned char>(ah::Next() % 8 | (ah::Half() ? 0 : ah::Next() & 0xF8));
        break;
    case k40Draw:
        if (ah::Half()) Gfx_BufferIndex = static_cast<unsigned char>(ah::Next() % 2);
        break;
    case k40Init:
        if (ah::Often()) Cond_ByteFD = static_cast<unsigned char>(AH_PICK(1, 2, 6, 6, 5, 0x86));
        if (ah::Often()) B(at::kEntryZone) = static_cast<unsigned char>(AH_PICK(4, 4, 6, 6, 5, 0x84));
        break;
    default: break;
    }
}

// A 16.16 position whose high word is `cell` (or beside it).
U Position(U cell) { return cell << 16 | (ah::Next() & 0xFFFF); }

void Args40(unsigned k, U* a) {
    switch (k) {
    case k40Hook:
    case k40Behind:
        // on the grid, one edge just off, or anywhere near it
        if (ah::Half()) {
            a[0] = Position(at::kGridX + ah::Next() % 4);
            a[1] = Position(at::kGridZ + ah::Next() % 4);
            if (ah::Half()) {
                if (ah::Half()) a[0] = Position(AH_PICK(0x93, 0x98, 0x194, 0xFF94));
                else a[1] = Position(AH_PICK(0x1F, 0x24, 0x120, 0xFF20));
            }
        } else {
            a[0] = Position(at::kGridX - 3 + ah::Next() % 10);
            a[1] = Position(at::kGridZ - 3 + ah::Next() % 10);
        }
        break;
    case k40TileLit: {
        // (x, z) less the origin: a row and a diagonal of the same parity,
        // inside the ring two times in three, else at or past its edges
        const bool inside = ah::Often();
        const int row = inside ? static_cast<int>(ah::Next() % 0x38) : static_cast<int>(AH_PICK(0xFFFFFFFF, 0x38, 0x39, 0x100));
        int diagonal = inside ? static_cast<int>(ah::Next() % 0x38) : static_cast<int>(AH_PICK(0xFFFFFFFF, 0x38, 0, 0x37));
        if ((diagonal ^ row) & 1) diagonal += diagonal > 0 ? -1 : 1;
        const int dx = (row + diagonal) / 2, dz = (row - diagonal) / 2;
        a[0] = (a[0] & 0xFFFF0000u) | static_cast<std::uint16_t>(MapView_Origin[0] + dx);
        a[1] = (a[1] & 0xFFFF0000u) | static_cast<std::uint16_t>(MapView_Origin[1] + dz);
        break;
    }
    default: break;
    }
}

// ===========================================================================
// Area 41
// ===========================================================================

constexpr ah::CallSite kCalls406200[] = {{0x14, 0x531F90}, {0x26, 0x531F90}};
constexpr ah::CallSite kCalls406240[] = {{0x14, 0x531F90}, {0x26, 0x531F90}};
constexpr ah::CallSite kCalls406280[] = {{0x23, 0x57C140}, {0x34, 0x587740}, {0x41, 0x590BB0}, {0x4F, 0x57C0F0}};
constexpr ah::CallSite kCalls4062E0[] = {{0x13, 0x5919B0}, {0x26, 0x5919B0}, {0x39, 0x5919B0}, {0x4C, 0x5919B0}, {0x61, 0x591B60}, {0x6E, 0x591B60}, {0x7B, 0x591B60}, {0x88, 0x591B60}};
constexpr ah::CallSite kCalls4063F0[] = {{0xC, 0x406560}};
constexpr ah::CallSite kCalls406430[] = {{0x0, 0x406560}};
constexpr ah::CallSite kCalls4065D0[] = {{0x6, 0x579F00}, {0x11, 0x579F00}};
constexpr ah::CallSite kCalls4065F0[] = {{0x2, 0x5734F0}};
constexpr ah::CallSite kCalls406600[] = {{0x0, 0x57C7C0}};
constexpr ah::CallSite kCalls406620[] = {{0x18, 0x57C0F0}, {0x22, 0x587740}};

const ah::Clone kClones41[] = {
    {"Area41_ChoiceDropIn12", 0x406200, 0x36, kCalls406200, AH_N(kCalls406200), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area41_ChoiceDropIn12), 0, false, S::kChoice},
    {"Area41_ChoiceDropIn34", 0x406240, 0x3E, kCalls406240, AH_N(kCalls406240), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area41_ChoiceDropIn34), 0, false, S::kChoice},
    {"Area41_ChoiceGiveItem", 0x406280, 0x58, kCalls406280, AH_N(kCalls406280), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area41_ChoiceGiveItem), 0, false, S::kChoice},
    {"Area41_ChoiceTrade", 0x4062E0, 0xBC, kCalls4062E0, AH_N(kCalls4062E0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area41_ChoiceTrade), 0, false, S::kChoice},
    {"Area41_ChoiceConfirm64", 0x4063A0, 0x24, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area41_ChoiceConfirm64), 0, false, S::kChoice},
    {"Area41_Run", 0x4063D0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area41_Run), 0, false, S::kHandler},
    {"Area41_TintStart", 0x4063F0, 0x34, kCalls4063F0, AH_N(kCalls4063F0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area41_TintStart), 0, false, S::kState},
    {"Area41_TintRise", 0x406430, 0x3A, kCalls406430, AH_N(kCalls406430), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area41_TintRise), 0, false, S::kState},
    {"Area41_Slide", 0x406470, 0x46, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area41_Slide), 0, false, S::kState},
    {"Area41_TintFall", 0x4064C0, 0x99, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area41_TintFall), 0, false, S::kState},
    {"Area41_TintUp", 0x406560, 0x64, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area41_TintUp), 0, false, S::kCallee},
    {"Area41_ClearCells", 0x4065D0, 0x1A, kCalls4065D0, AH_N(kCalls4065D0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area41_ClearCells), 0, false, S::kHandler},
    {"Area41_PlaceKind2", 0x4065F0, 0x9, kCalls4065F0, AH_N(kCalls4065F0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area41_PlaceKind2), 0, false, S::kHandler},
    {"Area41_Trigger29", 0x406600, 0x16, kCalls406600, AH_N(kCalls406600), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area41_Trigger29), 0xFF, false, S::kCallee},
    {"Area41_TriggerFlag", 0x406620, 0x2D, kCalls406620, AH_N(kCalls406620), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area41_TriggerFlag), 0xFF, false, S::kCallee},
};
enum : unsigned {
    k41DropIn12, k41DropIn34, k41Give, k41Trade, k41Confirm, k41Run, k41TintStart, k41TintRise, k41Slide, k41TintFall,
    k41TintUp, k41Clear, k41Kind2, k41Trigger29, k41TriggerFlag
};
const ah::DataTable kTables41[] = {{at::kArea41States, at::kArea41StateCount}};

// The field object a trigger is called with, drawn by the seed (the
// harness captures the state before it asks for the arguments).
unsigned g_trigger_object;

void Seed41(unsigned k) {
    Common();
    if (k == k41Trigger29 || k == k41TriggerFlag) {
        g_trigger_object = ah::Next() % 30;
        if (k == k41TriggerFlag && ah::Often()) ah::Object(g_trigger_object)[0x86] = static_cast<unsigned char>(AH_PICK(59, 60));
    }
    switch (k) {
    case k41DropIn12: case k41DropIn34: case k41Give: case k41Trade: case k41Confirm: SeedAnswer(); break;
    case k41Run: Sprite_Current[4] = static_cast<unsigned char>(ah::Next() % at::kArea41StateCount); break;
    case k41TintRise: case k41Slide: case k41TintFall:
        if (ah::Often()) Sprite_Current[0xA] = static_cast<unsigned char>(AH_PICK(1, 1, 2, 0, 8));
        break;
    default: break;
    }
    // the tint record index (Field_State +0x149) inside the group's regions
    if (k == k41TintFall || k == k41TintUp) Field_State[0x149] = static_cast<unsigned char>(ah::Next() % 232);
}

// Area 41's object triggers are called (the seed's field object, the story
// flags); its id is most often one the trigger is registered for.
void Args41(unsigned k, U* a) {
    if (k != k41Trigger29 && k != k41TriggerFlag) return;
    a[0] = Key(ah::Object(g_trigger_object));
    a[1] = 0x904030;
}

void RunArea(int area, const ah::Clone* clones, unsigned n, const ah::DataTable* tables, unsigned n_tables,
             void (*seed)(unsigned), void (*args)(unsigned, U*), unsigned rounds) {
    ah::Group g{"area_w1a", clones, n, kCallees, AH_COUNT(kCallees), tables, n_tables, g_regions, AH_COUNT(g_regions),
                seed, &Disturb, rounds};
    g.args = args;
    g.area = area;
    ah::Run(g);
}

}  // namespace

void SelfTest() {
    g_regions[9].at = Key(g_packets);
    constexpr unsigned kRounds = 6000;
    RunArea(38, kClones38, AH_COUNT(kClones38), nullptr, 0, &Seed38, nullptr, kRounds);
    RunArea(39, kClones39, AH_COUNT(kClones39), kTables39, AH_COUNT(kTables39), &Seed39, nullptr, kRounds);
    RunArea(40, kClones40, AH_COUNT(kClones40), nullptr, 0, &Seed40, &Args40, kRounds);
    RunArea(41, kClones41, AH_COUNT(kClones41), kTables41, AH_COUNT(kTables41), &Seed41, &Args41, kRounds);
}

}  // namespace area_w1a

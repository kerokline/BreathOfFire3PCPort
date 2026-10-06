// The area round's shared harness (area_harness.h; docs/area_harness.md).
//
// A copy of magic_harness.cpp adapted to the area overlays (round ten, group
// ARH). What is the same: one pool of recording stand-ins, assigned at
// start-up to every callee a group's clones call (the standard set below,
// plus the group's own) and to every handler they dispatch to (stack-table
// immediates, .data table entries) - ours reaches the same recorders through
// AH_CALL / AH_AT / Phase while g_active is set; theirs (the copy) then ours
// from the same state, the regions and the recorders' log compared. What is
// different:
//
//   - the state is one field frame: the party records and Field_State, the
//     field objects and Sprite_Current, the message word, the flags, the
//     area block, the field cells (section 4 of the doc);
//   - Group::area is written into Game_AreaNumber every round, and the real
//     descriptors and tables are left in place;
//   - each clone is called as its shape says (Clone::shape), and what the
//     engine reads after it is logged;
//   - a .data table may hold pairs (DataTable::stride) and hooks
//     (DataTable::nargs);
//   - the disturbance moves field cells, not battle cells.
//
// It keeps its own recorders and its own g_active: magic_harness is not
// touched, and both can run in one process (BOF3X_SHADOW='*').
#include "game/area_harness.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <utility>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace area_harness {

bool g_active = false;

namespace {

std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> std::uint32_t KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }

// --- the random source and the recorders' log ---------------------------------

std::uint32_t g_rng = 0xA4EA7001u;

constexpr unsigned kLog = 32768;
struct Entry { std::uint32_t what, a, b, c, d; };
Entry g_log[kLog];
unsigned g_log_n;
std::uint32_t g_seed;
std::uint32_t g_salt;
std::uint32_t g_rand_hint;
int g_rand_first = -1;
int g_rand_pending = -1;

constexpr std::uint32_t kPhaseTag = 1000;   // + slot: a handler's or a kPhase callee's
constexpr std::uint32_t kMoreTag = 2000;    // + slot: a call's arguments past the fourth
constexpr std::uint32_t kNoteTag = 3000;    // Note / NoteBytes
constexpr std::uint32_t kReturnTag = 4000;  // a clone's answer (Clone::ret_mask)
constexpr std::uint32_t kShapeTag = 5000;   // + shape: what the engine reads after the root

std::uint32_t Hash() {
    std::uint32_t h = (g_seed + g_log_n * 0x2545F491u) * 0x9E3779B1u;
    h ^= h >> 15;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    return h;
}
void Log5(std::uint32_t what, std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t d) {
    if (g_log_n < kLog) g_log[g_log_n] = {what, a, b, c, d};
    ++g_log_n;
}

bool g_calm = false;

// --- the harness's own memory -------------------------------------------------

constexpr unsigned kRecordBytes = 0x140;
alignas(16) unsigned char g_records[2][kRecordBytes];

const Group* g_group = nullptr;

// --- the regions (the state both passes start from) ----------------------------

constexpr unsigned kMaxRegions = 40;
Region g_regions[kMaxRegions];
unsigned g_region_n;
constexpr unsigned kMaxBytes = 0x10000;

// Whether [p, p + n) lies in the compared state: a pointer ours or a
// recorder moved is only read through when it does.
bool InRegions(const unsigned char* p, unsigned n) {
    const std::uint32_t a = Key(p);
    for (unsigned i = 0; i < g_region_n; ++i)
        if (a >= g_regions[i].at && a + n <= g_regions[i].at + g_regions[i].size) return true;
    return false;
}
std::uint32_t ByteAt(const unsigned char* p, unsigned off) { return InRegions(p + off, 1) ? p[off] : 0x100u; }

unsigned char* ObjectOrMember(unsigned v) { return v & 8 ? PartyOf(static_cast<unsigned char>(v)) : Object(v & 3); }

// Every cell below is one some area function or its engine callee reads
// again after a call.
void Disturb() {
    const std::uint32_t h = Hash();
    if (g_calm || h % 3 == 0) return;
    const unsigned v = (h >> 12) & 0xFF;
    const auto b = static_cast<unsigned char>(h >> 20);
    switch ((h >> 4) % 16) {
    case 0: Sprite_Current = ObjectOrMember(v); break;
    case 1: SetPointer(at::kFieldState, PartyOf(static_cast<unsigned char>(v))); break;
    case 2: case 3: {
        static const unsigned kFields[] = {0, 1, 2, 4, 8, 9, 0xA, 0xB, 0xC, 0x2E, 0x30, 0x34, 0x36, 0x38, 0x3A};
        const unsigned f = kFields[v % 15];
        const unsigned span = g_group ? g_group->phase_span : 0;
        if (InRegions(Sprite_Current + f, 1))
            Sprite_Current[f] = (f == 1 || f == 2) && span ? static_cast<unsigned char>(b % span) : b;
        break;
    }
    case 4: {
        unsigned char* const leader = Pointer(at::kFieldState) + (h >> 20) % at::kPartyStride;
        if (InRegions(leader, 1)) *leader = static_cast<unsigned char>(v);
        break;
    }
    case 5: Frame_Counter = h >> 6; break;
    case 6: move_script::SetWord(Mem(at::kMessage), h & 0x100 ? 0xFFFFu : h >> 16); break;
    case 7: Mem(at::kFieldRequest)[0] = b; break;
    case 8: Mem(at::kCondFlags + (h >> 20) % 0x1D0)[0] = static_cast<unsigned char>(v); break;
    case 9: Mem(at::kMemberCount)[0] = static_cast<unsigned char>(1 + v % 3); break;
    case 10: Object(v & 3)[(h >> 20) % at::kObjectStride] = b; break;
    case 11: Mem(0x903840 + (h >> 20) % 0x40)[0] = static_cast<unsigned char>(v); break;
    case 12:
        if (g_group && g_group->disturb) g_group->disturb(h);
        break;
    default: break;
    }
    if (g_group && g_group->settle) g_group->settle();
}

// --- the stand-ins ------------------------------------------------------------

struct Slot {
    const char* name;
    std::uint32_t address;
    std::uint32_t key;
    unsigned nargs;
    std::uint32_t masks[kArgs];
    Answer answer;
    std::uint8_t lo, hi;
    std::uint8_t deref[kArgs];
    Effect effect;
    const void* custom;
    bool handler;            // a phase or a hook: logs the object (and a hook's arguments)
    unsigned calls;
};
constexpr unsigned kSlots = 256;
Slot g_slots[kSlots];
unsigned g_slot_n;

std::uint32_t Cur() { return Key(Sprite_Current); }
std::uint32_t Leader() { return static_cast<std::uint32_t>(move_script::Long(Mem(at::kFieldState))); }
std::uint32_t Phases() {
    return ByteAt(Sprite_Current, 1) | ByteAt(Sprite_Current, 2) << 9;
}

std::uint32_t Remixed(std::uint32_t h) {
    h *= 0x2C1B3C6Du;
    h ^= h >> 12;
    h *= 0x297A2D39u;
    h ^= h >> 15;
    return h;
}

std::uint32_t Answering(const Slot& s) {
    const std::uint32_t h = s.answer == Answer::kFlag || s.answer == Answer::kBool ? Remixed(Hash()) : Hash();
    switch (s.answer) {
    case Answer::kByte: {
        const unsigned span = ((static_cast<unsigned>(s.hi) - s.lo) & 0xFFu) + 1;
        return (h & 0xFFFFFF00u) | ((s.lo + (h >> 8) % span) & 0xFFu);
    }
    case Answer::kFlag: return h % 3 == 0 ? (h & 0x100 ? 0u : h & 0xFFFFFF00u) : h | 0x10;
    case Answer::kBool: return h % 3 == 0 ? 0u : 1u;
    case Answer::kRand:
        if (g_rand_pending >= 0) {
            const int v = g_rand_pending;
            g_rand_pending = -1;
            return ((h >> 8) & 0x7F00u) | static_cast<std::uint32_t>(v);
        }
        if (h % 3 == 0) return ((h >> 8) & 0x7F00u) | ((g_rand_hint + (h >> 4) % 3 - 1) & 0xFF);
        return h % 4 == 0 ? h : (h >> 1) & 0x7FFF;
    case Answer::kGarbage:
    default: return h;
    }
}

template <unsigned I>
std::uint32_t __cdecl Stub(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2, std::uint32_t a3, std::uint32_t a4,
                           std::uint32_t a5, std::uint32_t a6, std::uint32_t a7, std::uint32_t a8, std::uint32_t a9) {
    const Slot& s = g_slots[I];
    if (s.handler) {
        // a phase logs the object it ran for; a hook (nargs) its arguments too
        if (s.nargs) Log5(kPhaseTag + I, Cur(), a0 & s.masks[0], s.nargs > 1 ? a1 & s.masks[1] : 0, Phases());
        else Log5(kPhaseTag + I, Cur(), Leader(), Phases(), 0);
        Disturb();
        return Hash();
    }
    if (s.answer == Answer::kPhase) {
        Log5(I, Cur(), Leader(), Phases(), s.masks[0] ? static_cast<std::uint32_t>(move_script::Long(Mem(s.masks[0]))) : 0);
        Disturb();
        return Hash();
    }
    const std::uint32_t a[kArgs] = {a0, a1, a2, a3, a4, a5, a6, a7, a8, a9};
    std::uint32_t r[kArgs] = {};
    for (unsigned i = 0; i < s.nargs && i < kArgs; ++i)
        r[i] = s.deref[i] ? HashBytes(reinterpret_cast<const void*>(static_cast<std::uintptr_t>(a[i])), s.deref[i])
                          : a[i] & s.masks[i];
    const unsigned entry = g_log_n;
    Log5(I, r[0], r[1], r[2], r[3]);
    if (s.nargs > 4) Log5(kMoreTag + I, r[4], r[5], r[6], r[7]);
    if (s.nargs > 8) Log5(kMoreTag + I, r[8], r[9], 0, 0);
    Disturb();
    std::uint32_t answer = Answering(s);
    if (s.effect) answer = s.effect(a, answer);
    if (s.answer != Answer::kGarbage && entry < kLog) g_log[entry].d ^= answer & 0xFF;
    return answer;
}

using StubFn = std::uint32_t (__cdecl*)(std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t,
                                        std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t);
template <std::size_t... I> constexpr auto MakeStubs(std::index_sequence<I...>) {
    struct T { StubFn f[sizeof...(I)]; };
    return T{{&Stub<I>...}};
}
constexpr auto kStubs = MakeStubs(std::make_index_sequence<kSlots>{});

// --- the standard callees -------------------------------------------------------
//
// The area band's frontier (docs/takeover-queue-areas.md section 1.3): every
// function the walk of 2026-09-26 found the band calling outside itself whose
// name is AreaMap_*, Field_*, Flags_*, Gte_*, Gpu_*, MapView_*, Msg_Open*,
// Sprite_*, Party_*, Inventory_*, Sound_*, Music_*, Text_Draw* or Rand, with
// a signature in symbols.toml - plus Effect_Spawn and Math_Sin / Math_Cos.
// AH_OURS for a callee that is ours, AH_THEIRS for one that is Capcom's, as
// symbols.toml says on 2026-09-27; one that changes hands is caught at
// start-up (Register) and moves line. Masks from the parameter types (a
// byte's 0xFF, a short's 0xFFFF); answers kFlag for a callee answering a
// byte, kBool for Flags_Test (all of eax is 0 or 1), garbage otherwise. A
// callee whose answer is a pointer the caller follows (Text_DrawAt, the GPU
// and GTE setters that answer their packet) wants the group's own listing
// with an effect.
#define AH_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
#define AH_THEIRS(name) #name, KeyOf(name), KeyOf(name)
constexpr std::uint32_t kAll = 0xFFFFFFFFu, kU8 = 0xFFu, kU16 = 0xFFFFu;

const Callee kStandard[] = {
    {AH_OURS(Sprite_SetTint), 5, {kAll, kU8, kU8, kU8, kU8}, Answer::kFlag, 0, 0},
    {AH_OURS(Sprite_ReleaseTint), 1, {kAll}, Answer::kGarbage, 0, 0},
    {AH_OURS(Msg_OpenScript), 1, {kU16}, Answer::kGarbage, 0, 0},
    {AH_OURS(Msg_OpenSystem), 1, {kAll}, Answer::kGarbage, 0, 0},
    {AH_OURS(Text_DrawAt), 5, {kAll, kAll, kAll, kAll, kAll}, Answer::kGarbage, 0, 0},   // answers a pointer: a caller that follows it wants an effect
    {AH_OURS(Text_DrawSmall), 5, {kAll, kAll, kAll, kAll, kAll}, Answer::kGarbage, 0, 0},   // answers a pointer: a caller that follows it wants an effect
    {AH_OURS(Field_ObjectBlockedAhead), 1, {kAll}, Answer::kFlag, 0, 0},
    {AH_OURS(Field_LeaderStepTick), 0, {}, Answer::kFlag, 0, 0},
    {AH_OURS(Field_LeaderStepTarget), 0, {}, Answer::kFlag, 0, 0},
    {AH_OURS(Field_LeaderTalkTest), 0, {}, Answer::kFlag, 0, 0},
    {AH_OURS(Field_CellHasEvent), 2, {kAll, kAll}, Answer::kFlag, 0, 0},
    {AH_OURS(Field_LeaderCellEvent), 0, {}, Answer::kFlag, 0, 0},
    {AH_OURS(Party_Count), 1, {kAll}, Answer::kGarbage, 0, 0},
    {AH_OURS(Sprite_ObjectAt), 3, {kAll, kAll, kAll}, Answer::kFlag, 0, 0},
    {AH_OURS(Field_LeaderPushObjects), 0, {}, Answer::kFlag, 0, 0},
    {AH_OURS(Party_MemberAt), 3, {kAll, kAll, kAll}, Answer::kFlag, 0, 0},
    {AH_OURS(Party_DropIn), 1, {kAll}, Answer::kGarbage, 0, 0},
    {AH_OURS(Field_MemberSprite), 2, {kAll, kAll}, Answer::kGarbage, 0, 0},
    {AH_OURS(Sprite_ShadeFadeBegin), 0, {}, Answer::kGarbage, 0, 0},
    {AH_OURS(Field_JumpStart), 0, {}, Answer::kGarbage, 0, 0},
    {AH_OURS(Sprite_ShadeLower), 1, {kAll}, Answer::kFlag, 0, 0},
    {AH_OURS(Field_FloorDamage), 0, {}, Answer::kGarbage, 0, 0},
    {AH_OURS(Field_Bit80Tick), 0, {}, Answer::kGarbage, 0, 0},
    {AH_OURS(Field_Bit20Tick), 0, {}, Answer::kGarbage, 0, 0},
    {AH_OURS(Field_JumpCheckHeight), 0, {}, Answer::kGarbage, 0, 0},
    {AH_OURS(Sprite_LoadPalette), 2, {kAll, kAll}, Answer::kGarbage, 0, 0},
    {AH_OURS(AreaMap_ByteAt), 2, {kU16, kU16}, Answer::kFlag, 0, 0},
    {AH_OURS(Field_ViewReset), 0, {}, Answer::kGarbage, 0, 0},
    {AH_OURS(AreaMap_ApplyPatch), 1, {kAll}, Answer::kGarbage, 0, 0},
    {AH_OURS(AreaMap_Elevation), 2, {kAll, kAll}, Answer::kGarbage, 0, 0},
    {AH_OURS(AreaMap_Slope), 3, {kAll, kAll, kAll}, Answer::kGarbage, 0, 0},
    {AH_OURS(MapView_GroundAt), 2, {kAll, kAll}, Answer::kGarbage, 0, 0},
    {AH_OURS(MapView_SetElevation), 1, {kAll}, Answer::kGarbage, 0, 0},
    {AH_OURS(MapView_ItemAt), 2, {kAll, kAll}, Answer::kGarbage, 0, 0},
    {AH_OURS(MapView_ItemHalfAt), 2, {kAll, kAll}, Answer::kGarbage, 0, 0},   // answers a pointer: a caller that follows it wants an effect
    {AH_OURS(MapView_LinkPrimAt), 4, {kAll, kAll, kAll, kAll}, Answer::kGarbage, 0, 0},
    {AH_OURS(AreaMap_SetByte), 3, {kAll, kAll, kAll}, Answer::kGarbage, 0, 0},
    {AH_OURS(Sprite_UpdateScreenA), 0, {}, Answer::kGarbage, 0, 0},
    {AH_OURS(Sprite_InitFromEntry), 1, {kAll}, Answer::kGarbage, 0, 0},
    {AH_OURS(Flags_Set), 2, {kAll, kU8}, Answer::kGarbage, 0, 0},
    {AH_OURS(Flags_Clear), 2, {kAll, kU8}, Answer::kGarbage, 0, 0},
    {AH_OURS(Flags_Test), 2, {kAll, kU8}, Answer::kBool, 0, 0},
    {AH_OURS(Sprite_FaceDirection), 1, {kU8}, Answer::kGarbage, 0, 0},
    {AH_OURS(Sound_PlayEffect), 1, {kU16}, Answer::kGarbage, 0, 0},
    {AH_OURS(Sound_PlayById), 1, {kU16}, Answer::kGarbage, 0, 0},
    {AH_OURS(Music_Play), 2, {kAll, kAll}, Answer::kGarbage, 0, 0},
    {AH_OURS(Music_FadeOutStop), 1, {kAll}, Answer::kGarbage, 0, 0},
    {AH_OURS(Sound_ResumeAll), 0, {}, Answer::kGarbage, 0, 0},
    {AH_OURS(Sprite_UpdateScreenSlot), 0, {}, Answer::kGarbage, 0, 0},
    {AH_OURS(Sprite_UpdateScreen), 0, {}, Answer::kGarbage, 0, 0},
    {AH_OURS(Sprite_QueueOverlay), 0, {}, Answer::kGarbage, 0, 0},
    {AH_OURS(Sprite_SetAnimation), 1, {kU8}, Answer::kGarbage, 0, 0},
    {AH_OURS(Sprite_SetAnimationAt), 2, {kU8, kU16}, Answer::kGarbage, 0, 0},
    {AH_OURS(Sprite_EnsureAnimation), 1, {kU8}, Answer::kFlag, 0, 0},
    {AH_OURS(Sprite_ScriptTick), 0, {}, Answer::kFlag, 0, 0},
    {AH_OURS(Sprite_SetAnimationBank), 1, {kU16}, Answer::kFlag, 0, 0},
    {AH_OURS(Inventory_Add), 3, {kAll, kAll, kAll}, Answer::kFlag, 0, 0},
    {AH_OURS(Inventory_Count), 3, {kAll, kAll, kAll}, Answer::kGarbage, 0, 0},
    {AH_OURS(Inventory_CountUsed), 1, {kAll}, Answer::kFlag, 0, 0},
    {AH_OURS(Field_ChangeArea), 4, {kAll, kAll, kAll, kAll}, Answer::kGarbage, 0, 0},
    {AH_OURS(Gpu_SetPolyFT4), 1, {kAll}, Answer::kGarbage, 0, 0},
    {AH_OURS(Gpu_SetPolyG3), 1, {kAll}, Answer::kGarbage, 0, 0},
    {AH_OURS(Gpu_SetPolyG4), 1, {kAll}, Answer::kGarbage, 0, 0},   // answers a pointer: a caller that follows it wants an effect
    {AH_OURS(Gpu_SetLineF2), 1, {kAll}, Answer::kGarbage, 0, 0},
    {AH_OURS(Gpu_SetSprt), 1, {kAll}, Answer::kGarbage, 0, 0},
    {AH_OURS(Gpu_SetSemiTrans), 2, {kAll, kAll}, Answer::kGarbage, 0, 0},
    {AH_OURS(Gpu_SetShadeTex), 2, {kAll, kAll}, Answer::kGarbage, 0, 0},
    {AH_OURS(Gpu_SetDrawMode), 5, {kAll, kAll, kAll, kAll, kAll}, Answer::kGarbage, 0, 0},
    {AH_OURS(Gpu_GetTPage), 4, {kAll, kAll, kAll, kAll}, Answer::kGarbage, 0, 0},
    {AH_OURS(Gpu_GetClut), 2, {kAll, kAll}, Answer::kGarbage, 0, 0},
    {AH_OURS(Gte_SetGeomOffset), 2, {kAll, kAll}, Answer::kGarbage, 0, 0},
    {AH_OURS(Gte_SetGeomScreen), 1, {kAll}, Answer::kGarbage, 0, 0},
    {AH_OURS(Gte_PushMatrix), 0, {}, Answer::kGarbage, 0, 0},
    {AH_OURS(Gte_PopMatrix), 0, {}, Answer::kGarbage, 0, 0},
    {AH_OURS(Gte_MulMatrix0), 3, {kAll, kAll, kAll}, Answer::kGarbage, 0, 0},   // answers a pointer: a caller that follows it wants an effect
    {AH_OURS(Gte_RotMatrix), 2, {kAll, kAll}, Answer::kGarbage, 0, 0},   // answers a pointer: a caller that follows it wants an effect
    {AH_OURS(Gte_RotTrans), 2, {kAll, kAll}, Answer::kGarbage, 0, 0},
    {AH_OURS(Gte_RotTransPers), 3, {kAll, kAll, kAll}, Answer::kGarbage, 0, 0},
    {AH_OURS(Gte_RotTransPers3), 7, {kAll, kAll, kAll, kAll, kAll, kAll, kAll}, Answer::kGarbage, 0, 0},
    {AH_OURS(Gte_RotTransPers4), 9, {kAll, kAll, kAll, kAll, kAll, kAll, kAll, kAll, kAll}, Answer::kGarbage, 0, 0},
    {AH_OURS(Gte_RotAverage3), 7, {kAll, kAll, kAll, kAll, kAll, kAll, kAll}, Answer::kGarbage, 0, 0},
    {AH_OURS(Gte_SetRotMatrix), 1, {kAll}, Answer::kGarbage, 0, 0},
    {AH_OURS(Gte_SetTransMatrix), 1, {kAll}, Answer::kGarbage, 0, 0},
    {AH_OURS(Gte_StoreDepthF), 1, {kAll}, Answer::kGarbage, 0, 0},
    {AH_OURS(Gte_PrimDepths4_10), 1, {kAll}, Answer::kGarbage, 0, 0},
    {AH_OURS(Gte_PrimDepths3_10B), 1, {kAll}, Answer::kGarbage, 0, 0},
    {AH_OURS(Gte_PrimDepths4_10B), 1, {kAll}, Answer::kGarbage, 0, 0},
    {AH_THEIRS(Rand), 0, {}, Answer::kRand, 0, 0},
    {AH_OURS(Effect_Spawn), 5, {kU8, kU8, kU8, kU16, kU16}, Answer::kFlag, 0, 0},
    {AH_OURS(Math_Sin), 1, {kAll}, Answer::kGarbage, 0, 0},
    {AH_OURS(Math_Cos), 1, {kAll}, Answer::kGarbage, 0, 0},
};
#undef AH_OURS
#undef AH_THEIRS

constexpr std::uint32_t kImageLo = 0x401000, kImageHi = 0x5C3000;   // .text

void Register(const Callee& c) {
    if (c.key == c.address) {
        if (c.key < kImageLo || c.key >= kImageHi)
            bof3::Fatal("area_harness: callee %s at 0x%X is not Capcom's code", c.name, (unsigned)c.key);
    } else if (c.key >= kImageLo && c.key < kImageHi) {
        bof3::Fatal("area_harness: callee %s is Capcom's now (0x%X): list it as such", c.name, (unsigned)c.key);
    }
    if (c.nargs > kArgs) bof3::Fatal("area_harness: callee %s takes %u arguments, the stand-ins %u", c.name, c.nargs, kArgs);
    for (unsigned i = 0; i < g_slot_n; ++i)
        if (g_slots[i].address == c.address) return;
    if (g_slot_n == kSlots) bof3::Fatal("area_harness: more than %u stand-ins", kSlots);
    Slot& s = g_slots[g_slot_n++];
    s = {};
    s.name = c.name;
    s.address = c.address;
    s.key = c.key;
    s.nargs = c.nargs;
    std::memcpy(s.masks, c.masks, sizeof s.masks);
    s.answer = c.answer;
    s.lo = c.lo;
    s.hi = c.hi;
    std::memcpy(s.deref, c.deref, sizeof s.deref);
    s.effect = c.effect;
    s.custom = c.custom;
}
void RegisterHandler(std::uint32_t address, unsigned nargs) {
    for (unsigned i = 0; i < g_slot_n; ++i)
        if (g_slots[i].address == address) return;
    if (g_slot_n == kSlots) bof3::Fatal("area_harness: more than %u stand-ins", kSlots);
    Slot& s = g_slots[g_slot_n++];
    s = {};
    s.name = "handler";
    s.address = s.key = address;
    s.handler = true;
    s.nargs = nargs;
    for (unsigned i = 0; i < kArgs; ++i) s.masks[i] = kAll;
}
const void* StubOf(unsigned i) {
    const Slot& s = g_slots[i];
    if (s.custom) return s.custom;
    if (s.answer == Answer::kThrough) return reinterpret_cast<const void*>(static_cast<std::uintptr_t>(s.address));
    return reinterpret_cast<const void*>(kStubs.f[i]);
}
unsigned SlotFor(std::uint32_t address, const char* who) {
    for (unsigned i = 0; i < g_slot_n; ++i)
        if (g_slots[i].address == address) return i;
    bof3::Fatal("area_harness: %s calls 0x%X, which no stand-in covers: list it in the group's callees", who,
                (unsigned)address);
}
const void* StubFor(std::uint32_t address, const char* who) { return StubOf(SlotFor(address, who)); }

// --- the state ------------------------------------------------------------------

struct State {
    unsigned char memory[kMaxBytes];
    Entry log[kLog];
    unsigned log_n;
};
unsigned g_bytes;

unsigned Used(unsigned log_n) { return log_n < kLog ? log_n : kLog; }

void Capture(State& s) {
    unsigned n = 0;
    for (unsigned i = 0; i < g_region_n; ++i) {
        std::memcpy(s.memory + n, Mem(g_regions[i].at), g_regions[i].size);
        n += g_regions[i].size;
    }
    s.log_n = g_log_n;
    std::memcpy(s.log, g_log, Used(g_log_n) * sizeof(Entry));
}
void Apply(const State& s) {
    unsigned n = 0;
    for (unsigned i = 0; i < g_region_n; ++i) {
        std::memcpy(Mem(g_regions[i].at), s.memory + n, g_regions[i].size);
        n += g_regions[i].size;
    }
    g_log_n = 0;
    g_salt = 0;
    g_rand_pending = g_rand_first;
}
bool Same(const State& a, const State& b) {
    return a.log_n == b.log_n && std::memcmp(a.memory, b.memory, g_bytes) == 0 &&
           std::memcmp(a.log, b.log, Used(a.log_n) * sizeof(Entry)) == 0;
}
unsigned FirstDifference(const State& a, const State& b) {
    for (unsigned i = 0; i < g_bytes; ++i)
        if (a.memory[i] != b.memory[i]) return i;
    return g_bytes;
}
unsigned FirstLogDifference(const State& a, const State& b) {
    const unsigned n = Used(a.log_n < b.log_n ? a.log_n : b.log_n);
    for (unsigned i = 0; i < n; ++i)
        if (std::memcmp(&a.log[i], &b.log[i], sizeof(Entry)) != 0) return i;
    return n;
}
void Where(unsigned byte, std::uint32_t& region, std::uint32_t& offset) {
    unsigned n = 0;
    for (unsigned i = 0; i < g_region_n; ++i) {
        if (byte < n + g_regions[i].size) {
            region = g_regions[i].at;
            offset = byte - n;
            return;
        }
        n += g_regions[i].size;
    }
    region = offset = 0;
}

// Random bytes put back inside what every area function dereferences: the
// running object (a party record or one of the first four field objects),
// the leader's record, the map bytes (into the area block), a member count
// of 1..3, the message word "none" two times in three.
void Fix() {
    Sprite_Current = ObjectOrMember(Next());
    SetPointer(at::kFieldState, PartyOf(static_cast<unsigned char>(Half() ? 0 : Next())));
    SetPointer(at::kMapBytes, Mem(at::kMapHeader + 0x800));
    Mem(at::kMemberCount)[0] = static_cast<unsigned char>(1 + Next() % 3);
    if (Often()) move_script::SetWord(Mem(at::kMessage), 0xFFFF);
}

void PatchImms(void* copy, const Clone& c) {
    auto* code = static_cast<std::uint8_t*>(copy);
    for (int i = 0; i < c.n_imms; ++i) {
        std::uint32_t had;
        std::memcpy(&had, code + c.imms[i].offset, sizeof had);
        if (had != c.imms[i].value)
            bof3::Fatal("area_harness: %s +0x%X holds 0x%X, not the handler 0x%X", c.name, (unsigned)c.imms[i].offset,
                        (unsigned)had, (unsigned)c.imms[i].value);
        const std::uint32_t to = Key(StubFor(had, c.name));
        std::memcpy(code + c.imms[i].offset, &to, sizeof to);
    }
    for (int i = 0; i < c.n_tables; ++i)
        move_script::Relocate(copy, c.base, c.size, {c.tables[i].jmp_disp, c.tables[i].table, c.tables[i].entries});
}

using Fn3 = std::uint32_t (__cdecl*)(std::uint32_t, std::uint32_t, std::uint32_t);
using FnArgs = std::uint32_t (__cdecl*)(std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t,
                                        std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t);

// A cell coordinate of the harness's draw, for a hook without the group's args.
std::uint32_t Cell() { return Half() ? Next() & 0xFF : Next(); }

}  // namespace

// --- the public helpers -------------------------------------------------------

std::uint32_t Next() { g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5; return g_rng; }
bool Often() { return Next() % 3 != 0; }
bool Half() { return (Next() & 1) != 0; }
std::uint32_t Pick(const std::uint32_t* v, unsigned n) { return v[Next() % n]; }
unsigned char* Mem(std::uint32_t address) { return move_script::At(address); }
unsigned char* Object(unsigned k) { return Mem(at::kObjects + (k % at::kObjectCount) * at::kObjectStride); }
unsigned char* TaskAt(unsigned k) { return Object(k % 4); }
unsigned char* SpriteRecord(unsigned k) { return g_records[k & 1]; }
unsigned char* EnemyOf(unsigned char target) {
    return Mem(at::kEnemies + static_cast<std::uint32_t>((static_cast<int>(target) - 3) * static_cast<int>(at::kEnemyStride)));
}
unsigned char* PartyOf(unsigned char member) { return Mem(at::kParty + (member % at::kPartyCount) * at::kPartyStride); }
const unsigned char* Descriptor(unsigned area) {
    return reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(
        static_cast<std::uint32_t>(move_script::Long(Mem(at::kDescriptors + 4 * area)))));
}
void SetPointer(std::uint32_t cell, const void* p) { move_script::SetLong(Mem(cell), static_cast<std::int32_t>(Key(p))); }
unsigned char* Pointer(std::uint32_t cell) {
    return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(move_script::Long(Mem(cell)))));
}
void SetRandHint(std::uint32_t hint) { g_rand_hint = hint; }
void SetRandFirst(int first) { g_rand_first = first; }

void Record(std::uint32_t address, std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t d) {
    for (unsigned i = 0; i < g_slot_n; ++i)
        if (g_slots[i].address == address && !g_slots[i].handler) {
            Log5(i, a, b, c, d);
            return;
        }
    bof3::Fatal("area_harness: a custom stand-in records 0x%X, which no callee lists", (unsigned)address);
}
void Stir() { Disturb(); }
std::uint32_t Noise() {
    std::uint32_t h = Hash() ^ (++g_salt * 0x9E3779B9u);
    h ^= h >> 16;
    h *= 0x7FEB352Du;
    h ^= h >> 15;
    return h;
}
void Note(std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t d) { Log5(kNoteTag, a, b, c, d); }
void NoteBytes(const void* p, unsigned n) {
    std::uint32_t w[4] = {};
    if (n <= sizeof w) {
        std::memcpy(w, p, n);
    } else {
        w[0] = HashBytes(p, n);
        w[1] = n;
    }
    Log5(kNoteTag + 1, w[0], w[1], w[2], w[3]);
}
void FillBytes(void* p, unsigned n) {
    for (unsigned i = 0; i < n; ++i) static_cast<unsigned char*>(p)[i] = static_cast<unsigned char>(Noise() >> 7);
}
std::uint32_t HashBytes(const void* p, unsigned n) {
    std::uint32_t h = 0x811C9DC5u;
    for (unsigned i = 0; i < n; ++i) h = (h ^ static_cast<const unsigned char*>(p)[i]) * 0x01000193u;
    return h;
}

const void* ForOurs(unsigned i, std::uint32_t key) {
    return g_slots[i].answer == Answer::kThrough ? reinterpret_cast<const void*>(static_cast<std::uintptr_t>(key)) : StubOf(i);
}

const void* StandIn(std::uint32_t key) {
    for (unsigned i = 0; i < g_slot_n; ++i)
        if (g_slots[i].key == key) return ForOurs(i, key);
    std::uint32_t address = key;
    for (const Callee& c : kStandard)
        if (c.key == key) address = c.address;
    for (unsigned i = 0; i < g_slot_n; ++i)
        if (g_slots[i].address == address) return ForOurs(i, key);
    bof3::Fatal("area_harness: ours calls 0x%X, which no stand-in covers: list it in the group's callees", (unsigned)key);
}

void Run(const Group& group) {
    const unsigned per = group.rounds ? group.rounds : 2000;
    g_group = &group;
    g_slot_n = 0;
    for (unsigned i = 0; i < group.n_callees; ++i) Register(group.callees[i]);
    for (const Callee& c : kStandard) Register(c);
    for (unsigned k = 0; k < group.n_clones; ++k)
        for (int i = 0; i < group.clones[k].n_imms; ++i) RegisterHandler(group.clones[k].imms[i].value, 0);
    for (unsigned t = 0; t < group.n_data_tables; ++t) {
        const DataTable& d = group.data_tables[t];
        for (unsigned i = 0; i < d.entries; ++i)
            RegisterHandler(static_cast<std::uint32_t>(move_script::Long(Mem(d.at + d.stride * i))), d.nargs);
    }

    // the regions: one field frame (docs/area_harness.md section 4), then the group's
    g_region_n = 0;
    const Region standard[] = {
        {0x7DEE40, 0x40},                                   // the message word 0x7DEE48 and the message box's cells
        {at::kObjects, at::kObjectCount * at::kObjectStride},   // Sprite_Objects
        {at::kObjectsExtra, 4 * at::kObjectStride},         // Sprite_ObjectsExtra
        {at::kParty, at::kPartyCount * at::kPartyStride},   // ObjTrio: the leader and the party
        {0x8034E0, 0x14},                                   // Cond_ByteFA .. MoveScript_Var7 ..
        {0x903840, 0x40},                                   // Camera_Distance, the pending cells, the scratch words
        {0x9039A0, 0x58},                                   // Field_ScriptFlags .. 0x9039F7 (the mode bytes)
        {at::kCondFlags, 0x1D0},                            // Cond_Flags, the story flags, the party lists
        {at::kAreaNumber, 4},                               // Game_AreaNumber
        {0x905B80, 0x30},                                   // Field_EdgeBits .. Field_ScriptFlags2 ..
        {at::kMapBytes, 8},                                 // AreaMap_Bytes, Field_State
        {0x905E60, 0x10},                                   // MapView_Redraw ..
        {at::kMemberCount, 1},                              // Field_MemberCount
        {0x929F1C, 4},                                      // MapView_Elevation
        {0x92BEE2, 2},                                      // MapView_ElevationOffset
        {0x937F80, 4},                                      // the pending area word 0x937F82
        {0x937F88, 0x10},                                   // Sprite_Current, MoveScript_F3Divisor, Gfx_ClutStripDirty, Frame_Counter
        {at::kFieldRequest, 1},                             // Field_Request
        {at::kMapHeader, at::kMapHeaderBytes},              // AreaMap_Header: the area block, its header cells
        {Key(g_records), sizeof g_records},
    };
    for (const Region& r : standard) g_regions[g_region_n++] = r;
    for (unsigned i = 0; i < group.n_regions; ++i) {
        if (g_region_n == kMaxRegions) bof3::Fatal("area_harness: %s: more than %u regions", group.shadow, kMaxRegions);
        g_regions[g_region_n++] = group.regions[i];
    }
    g_bytes = 0;
    for (unsigned i = 0; i < g_region_n; ++i) g_bytes += g_regions[i].size;
    if (g_bytes > kMaxBytes) bof3::Fatal("area_harness: %s: the regions are %u bytes, the state holds %u", group.shadow, g_bytes, kMaxBytes);

    static void* clones[256];
    if (group.n_clones > 256) bof3::Fatal("area_harness: %s: more than 256 clones", group.shadow);
    for (unsigned k = 0; k < group.n_clones; ++k) {
        const Clone& c = group.clones[k];
        bof3::CloneCall calls[64];
        if (c.n_calls > 64) bof3::Fatal("area_harness: %s has %d calls", c.name, c.n_calls);
        for (int i = 0; i < c.n_calls; ++i) {
            const unsigned slot = SlotFor(c.calls[i].target, c.name);
            calls[i] = {c.calls[i].offset, g_slots[slot].answer == Answer::kThrough ? nullptr : StubOf(slot), c.calls[i].target};
        }
        clones[k] = bof3::CloneOriginal(c.name, c.base, c.size, calls, c.n_calls);
        PatchImms(clones[k], c);
    }

    // the .data tables' entries swapped for their recorders, put back after
    constexpr unsigned kTableMax = 64, kEntryMax = 64;
    static std::uint32_t kept[kTableMax][kEntryMax];
    if (group.n_data_tables > kTableMax) bof3::Fatal("area_harness: %s: more than %u .data tables", group.shadow, kTableMax);
    for (unsigned t = 0; t < group.n_data_tables; ++t) {
        const DataTable& d = group.data_tables[t];
        if (d.entries > kEntryMax) bof3::Fatal("area_harness: %s: a .data table of more than %u", group.shadow, kEntryMax);
        for (unsigned i = 0; i < d.entries; ++i) {
            const std::uint32_t cell = d.at + d.stride * i;
            kept[t][i] = static_cast<std::uint32_t>(move_script::Long(Mem(cell)));
            move_script::SetLong(Mem(cell), static_cast<std::int32_t>(Key(StubFor(kept[t][i], group.shadow))));
        }
    }

    static State saved, input, theirs, ours;
    Capture(saved);
    for (unsigned i = 0; i < g_slot_n; ++i) g_slots[i].calls = 0;

    unsigned bad = 0, calls = 0;
    static unsigned bad_per[256];
    std::memset(bad_per, 0, sizeof bad_per);
    for (unsigned round = 0; round < per * group.n_clones; ++round) {
        const unsigned k = round % group.n_clones;
        const Clone& c = group.clones[k];
        for (unsigned i = 0; i < g_bytes; i += 4) {
            const std::uint32_t v = Next();
            std::memcpy(input.memory + i, &v, g_bytes - i < 4 ? g_bytes - i : 4);
        }
        input.log_n = 0;
        g_rand_first = -1;
        Apply(input);
        Fix();
        if (group.area >= 0) Game_AreaNumber = static_cast<unsigned short>(group.area);
        g_seed = Next();
        g_rand_hint = Next();
        if (group.seed) group.seed(k);
        Capture(input);

        // three words for a root (a hook's first two a cell), or the group's arguments
        std::uint32_t a[kArgs] = {Next(), Next(), Next()};
        if (c.shape == Shape::kHook) {
            a[0] = Cell();
            a[1] = Cell();
        }
        if (group.args) {
            for (unsigned i = 3; i < kArgs; ++i) a[i] = Next();
            group.args(k, a);
        }
        const std::uint32_t ret_mask = c.ret_mask ? c.ret_mask : c.shape == Shape::kHook ? 0xFFu : 0u;
        g_calm = c.calm;
        for (int pass = 0; pass < 2; ++pass) {
            Apply(input);
            State& out = pass ? ours : theirs;
            const void* const fn = pass ? c.ours : clones[k];
            g_active = pass == 1;
            const std::uint32_t answer =
                group.args ? reinterpret_cast<FnArgs>(const_cast<void*>(fn))(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8], a[9])
                           : reinterpret_cast<Fn3>(const_cast<void*>(fn))(a[0], a[1], a[2]);
            g_active = false;
            if (ret_mask) Log5(kReturnTag, answer & ret_mask, 0, 0, 0);
            // what the engine reads after the root (area_harness.h, Shape)
            if (c.shape == Shape::kHandler) Log5(kShapeTag, Cur(), ByteAt(Sprite_Current, 8), 0, 0);
            else if (c.shape == Shape::kChoice) Log5(kShapeTag + 1, move_script::Word(Mem(at::kMessage)), 0, 0, 0);
            Capture(out);
        }
        g_calm = false;
        for (unsigned i = 0; i < Used(theirs.log_n); ++i) {
            const std::uint32_t w = theirs.log[i].what;
            const unsigned s = w >= kPhaseTag ? w - kPhaseTag : w;
            if (w < kMoreTag && s < g_slot_n) {
                ++g_slots[s].calls;
                ++calls;
            }
        }
        if (theirs.log_n > kLog)
            bof3::Fatal("area_harness: %s made %u calls, the log holds %u", c.name, theirs.log_n, kLog);
        if (!Same(theirs, ours)) {
            ++bad_per[k];
            if (++bad <= 12) {
                std::uint32_t region, offset;
                const unsigned first = FirstDifference(theirs, ours);
                Where(first, region, offset);
                bof3::Log("shadow      %s self-test MISMATCH: round %u, %s, log %u / %u (first differing entry %u), first "
                          "differing byte %u (0x%X + 0x%X)",
                          group.shadow, round, c.name, theirs.log_n, ours.log_n, FirstLogDifference(theirs, ours), first,
                          (unsigned)region, (unsigned)offset);
            }
        }
    }
    Apply(saved);
    for (unsigned t = 0; t < group.n_data_tables; ++t) {
        const DataTable& d = group.data_tables[t];
        for (unsigned i = 0; i < d.entries; ++i)
            move_script::SetLong(Mem(d.at + d.stride * i), static_cast<std::int32_t>(kept[t][i]));
    }

    bof3::Log("shadow      %s self-test: %u rounds over %u functions (%u each), %u calls to the stand-ins, %u MISMATCHES; "
              "%u bytes of state (%u regions) and the stand-ins' log compared",
              group.shadow, per * group.n_clones, group.n_clones, per, calls, bad, g_bytes, g_region_n);
    char line[900];
    unsigned n = 0;
    bool more = false;
    for (unsigned i = 0; i < g_slot_n; ++i) {
        if (g_slots[i].calls == 0) continue;
        if (n + 64 >= sizeof line) {
            bof3::Log("shadow      %s coverage%s: %s,", group.shadow, more ? ", continued" : " (calls the originals made)", line);
            n = 0;
            more = true;
        }
        const int w = g_slots[i].handler
                          ? std::snprintf(line + n, sizeof line - n, "%sphase 0x%X %u", n ? ", " : "",
                                          (unsigned)g_slots[i].address, g_slots[i].calls)
                          : std::snprintf(line + n, sizeof line - n, "%s%s %u", n ? ", " : "", g_slots[i].name,
                                          g_slots[i].calls);
        if (w > 0) n += static_cast<unsigned>(w);
    }
    if (n || !more)
        bof3::Log("shadow      %s coverage%s: %s", group.shadow, more ? ", continued" : " (calls the originals made)", n ? line : "none");
    if (bad) {
        for (unsigned k = 0; k < group.n_clones; ++k)
            if (bad_per[k]) bof3::Log("shadow      %s: %s mismatched in %u rounds", group.shadow, group.clones[k].name, bad_per[k]);
        bof3::Fatal("%s differs from the original in %u self-test rounds", group.shadow, bad);
    }
    g_group = nullptr;
}

}  // namespace area_harness

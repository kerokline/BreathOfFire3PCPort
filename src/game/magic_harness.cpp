// The spell round's shared harness (magic_harness.h; docs/magic_harness.md).
//
// Generalises what battle_fx_tasks_fuzz.cpp and magic_fx_reached_fuzz.cpp
// do for one fixed set of functions:
//
//   - one pool of recording stand-ins, assigned at start-up to every callee a
//     group's clones call (the standard set below, plus the group's own) and
//     to every handler they dispatch to (stack-table immediates, .data table
//     entries) - ours reaches the same recorders through MH_CALL / MH_AT /
//     Phase while g_active is set;
//   - one frame of state: the task slots, the current slot and owner, the
//     battle bytes, the message-window byte, the party and enemy records,
//     Sprite_Current / Gfx_ClutStripDirty / Frame_Counter, two sprite records
//     of the harness's own, and the group's regions - random bytes, the
//     pointers put back inside, then the group's seed;
//   - theirs (the copy) then ours from the same state, the regions and the
//     recorders' log compared.
//
// The recorders are louder than the real callees: after recording, any call
// may move Sprite_Current, the current slot, the owner, the source, the
// target and actor bytes, the message-window byte, the effect flags,
// Frame_Counter, or a field of the task, the owner or the target enemy (and
// a group's own cells, through its disturb) - so a value ours keeps where
// the original reads memory again, or the other way round, shows.
#include "game/magic_harness.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <utility>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace magic_harness {

bool g_active = false;

namespace {

std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> std::uint32_t KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }

// --- the random source and the recorders' log ---------------------------------

std::uint32_t g_rng = 0x2545F491u;

constexpr unsigned kLog = 1024;
struct Entry { std::uint32_t what, a, b, c, d; };
Entry g_log[kLog];
unsigned g_log_n;
std::uint32_t g_seed;        // the recorders' own stream: the same on both passes
std::uint32_t g_rand_hint;
int g_rand_first = -1;
int g_rand_pending = -1;

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
// Several values within one call (Hash is the same until the log grows): a
// salt that counts up, reset with the log so both passes see the same.
std::uint32_t g_salt;

// --- the harness's own memory -------------------------------------------------

constexpr unsigned kRecordBytes = 0x140;
alignas(16) unsigned char g_records[2][kRecordBytes];

unsigned char* OwnerFor(unsigned v) { return v & 4 ? g_records[v & 1] : TaskAt(v); }
unsigned char* TargetEnemy() { return EnemyOf(Mem(at::kTarget)[0]); }

const Group* g_group = nullptr;

// Every cell below is one some effect reads again after a call.
void Disturb() {
    const std::uint32_t h = Hash();
    if (h % 3 == 0) return;
    const unsigned v = (h >> 12) & 0xFF;
    const auto b = static_cast<unsigned char>(h >> 20);
    switch ((h >> 4) % 16) {
    case 0: Sprite_Current = TaskAt(v); break;
    case 1: SetPointer(at::kOwner, OwnerFor(v)); break;
    case 2: SetPointer(at::kSource, g_records[v & 1]); break;
    case 3: SetPointer(at::kCurrentSlot, TaskAt(v >> 2)); break;
    case 4: Mem(at::kTarget)[0] = static_cast<unsigned char>(v % 11); break;
    case 5: Mem(at::kActor)[0] = static_cast<unsigned char>(v % 5); break;
    case 6: Mem(at::kMessageUp)[0] = b; break;
    case 7: Mem(at::kFlags)[0] = b; break;
    case 8: Frame_Counter = h >> 6; break;
    case 9: case 10: {
        static const unsigned kFields[] = {0, 1, 2, 4, 8, 9, 0xA, 0xB, 0xC, 0x2C, 0x2D, 0x30, 0x34, 0x38, 0x3C};
        Sprite_Current[kFields[v % 15]] = b;
        break;
    }
    case 11: {
        static const unsigned kFields[] = {0, 8, 9, 0xA, 0xB, 0x34, 0x38, 0x3C};
        Pointer(at::kOwner)[kFields[v % 8]] = b;
        break;
    }
    case 12: case 13:
        TargetEnemy()[(h >> 20) % at::kEnemyStride] = static_cast<unsigned char>(v);
        break;
    case 14:
        if (g_group && g_group->disturb) g_group->disturb(h);
        break;
    default: break;
    }
}

// --- the stand-ins ------------------------------------------------------------

struct Slot {
    const char* name;
    std::uint32_t address;   // what a clone's call site or table holds
    std::uint32_t key;       // what ours passes to Call
    unsigned nargs;
    std::uint32_t masks[4];
    Answer answer;
    std::uint8_t lo, hi;
    bool handler;            // a phase: logs the slot's phase bytes, answers nothing
    unsigned calls;          // the original's side, for the coverage line
    Effect effect;           // what it does through its pointers, if the group says
};
constexpr unsigned kSlots = 160;
Slot g_slots[kSlots];
unsigned g_slot_n;

std::uint32_t Cur() { return Key(Sprite_Current); }

std::uint32_t Answering(const Slot& s) {
    const std::uint32_t h = Hash();
    switch (s.answer) {
    case Answer::kByte: {
        const unsigned span = static_cast<unsigned>(s.hi) - s.lo + 1;
        return (h & 0xFFFFFF00u) | (s.lo + (h >> 8) % span);
    }
    case Answer::kFlag: return h % 3 == 0 ? h & 0xFFFFFF00u : h | 0x10;
    case Answer::kRand:
        // some values the CRT's never answers (negative); a third of the time
        // near the value the seeding asked for; the round's first exactly.
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

// Ten arguments: the most a callee takes (Gte_RotTransPers4). A caller that
// pushes fewer leaves its own frame in the rest, which only an Effect that
// knows the callee's arity reads.
template <unsigned I>
std::uint32_t __cdecl Stub(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2, std::uint32_t a3, std::uint32_t a4,
                           std::uint32_t a5, std::uint32_t a6, std::uint32_t a7, std::uint32_t a8, std::uint32_t a9) {
    const Slot& s = g_slots[I];
    if (s.handler) {
        Log5(1000 + I, Cur(), Sprite_Current[1], Sprite_Current[2], 0);
        Disturb();
        return Hash();
    }
    const std::uint32_t a[10] = {a0, a1, a2, a3, a4, a5, a6, a7, a8, a9};
    std::uint32_t r[4] = {};
    for (unsigned i = 0; i < s.nargs && i < 4; ++i) r[i] = a[i] & s.masks[i];
    Log5(I, r[0], r[1], r[2], r[3]);
    std::uint32_t answer = 0;
    if (s.effect) {
        answer = s.effect(a, Answering(s));
        Disturb();
    } else {
        Disturb();
        answer = Answering(s);
    }
    if (s.answer != Answer::kGarbage && g_log_n <= kLog) g_log[g_log_n - 1].d ^= answer & 0xFF;   // the answer in the log
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
// Every callee the effects read so far call (magic_fx_reached_callees.h,
// battle_fx_tasks_callees.h, battle_items_callees.h). MH_OURS for a callee that
// is ours (its name is our function; `address` is the one it displaced),
// MH_THEIRS for one that is Capcom's (its name is the address). A callee that
// changes hands is caught at start-up (Register) and moves line.
#define MH_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
#define MH_THEIRS(name) #name, KeyOf(name), KeyOf(name)
constexpr std::uint32_t kAll = 0xFFFFFFFFu, kU8 = 0xFFu, kU16 = 0xFFFFu;

const Callee kStandard[] = {
    {MH_OURS(BattleTask_Create), 2, {kU8, kU8}, Answer::kByte, 0, at::kTaskCount - 1},
    {MH_OURS(BattleTask_FreeCurrent), 0, {}, Answer::kGarbage, 0, 0},
    {MH_OURS(Battle_SetTargetFlags), 2, {kU8, kU16}, Answer::kGarbage, 0, 0},
    {MH_OURS(Battle_SetTargetFlag40), 1, {kU8}, Answer::kGarbage, 0, 0},
    {MH_OURS(Sound_PlayById), 1, {kU16}, Answer::kGarbage, 0, 0},
    {MH_OURS(Sound_PlayEffect), 1, {kU16}, Answer::kGarbage, 0, 0},
    {MH_OURS(BattleActor_SetAnimation), 2, {kU8, kAll}, Answer::kGarbage, 0, 0},
    {MH_OURS(BattleActor_UpdateScreenXY), 0, {}, Answer::kGarbage, 0, 0},
    {MH_OURS(BattleActor_PlaySound), 2, {kU8, kU8}, Answer::kGarbage, 0, 0},
    {MH_OURS(BattleActor_Flash), 1, {kU8}, Answer::kGarbage, 0, 0},
    {MH_OURS(BattleActor_FxSize), 0, {}, Answer::kFlag, 0, 0},
    {MH_OURS(Sprite_ScriptTickOnce), 0, {}, Answer::kFlag, 0, 0},
    {MH_OURS(Sprite_UpdateScreen), 0, {}, Answer::kGarbage, 0, 0},
    {MH_OURS(Sprite_ReleaseTint), 1, {kAll}, Answer::kGarbage, 0, 0},
    {MH_OURS(MagicFx_PushActorMatrix), 0, {}, Answer::kGarbage, 0, 0},
    {MH_OURS(MagicFx_DrawDisc), 0, {}, Answer::kGarbage, 0, 0},
    {MH_OURS(MagicFx_DrawFan), 0, {}, Answer::kGarbage, 0, 0},
    {MH_OURS(MagicFx_DrawRing), 0, {}, Answer::kGarbage, 0, 0},
    {MH_OURS(Gte_PopMatrix), 0, {}, Answer::kGarbage, 0, 0},
    {MH_OURS(AreaMap_TintClut), 1, {kAll}, Answer::kGarbage, 0, 0},
    {MH_OURS(Inventory_Add), 3, {kAll, kAll, kAll}, Answer::kFlag, 0, 0},
    {MH_OURS(Msg_SystemPtr), 1, {kU16}, Answer::kGarbage, 0, 0},
    {MH_OURS(BattleQueue_Push), 3, {kU8, kU8, kAll}, Answer::kGarbage, 0, 0},
    {MH_THEIRS(Rand), 0, {}, Answer::kRand, 0, 0},
    // an item's name into Text_Records by (index, category) - unnamed, in no
    // group (docs/magic_fx_reached.md section 10)
    {"0x4B58F0", 0x4B58F0, 0x4B58F0, 2, {kU8, kU8}, Answer::kGarbage, 0, 0},
};
#undef MH_OURS
#undef MH_THEIRS

constexpr std::uint32_t kImageLo = 0x401000, kImageHi = 0x5C3000;   // .text

void Register(const Callee& c) {
    if (c.key == c.address) {
        if (c.key < kImageLo || c.key >= kImageHi)
            bof3::Fatal("magic_harness: callee %s at 0x%X is not Capcom's code", c.name, (unsigned)c.key);
    } else if (c.key >= kImageLo && c.key < kImageHi) {
        bof3::Fatal("magic_harness: callee %s is Capcom's now (0x%X): list it as such", c.name, (unsigned)c.key);
    }
    for (unsigned i = 0; i < g_slot_n; ++i)
        if (g_slots[i].address == c.address) return;   // listed twice: the first stands
    if (g_slot_n == kSlots) bof3::Fatal("magic_harness: more than %u stand-ins", kSlots);
    Slot& s = g_slots[g_slot_n++];
    s = {c.name, c.address, c.key, c.nargs, {c.masks[0], c.masks[1], c.masks[2], c.masks[3]}, c.answer, c.lo, c.hi, false, 0,
         c.effect};
}
void RegisterHandler(std::uint32_t address) {
    for (unsigned i = 0; i < g_slot_n; ++i)
        if (g_slots[i].address == address) return;
    if (g_slot_n == kSlots) bof3::Fatal("magic_harness: more than %u stand-ins", kSlots);
    Slot& s = g_slots[g_slot_n++];
    s = {"handler", address, address, 0, {}, Answer::kGarbage, 0, 0, true, 0, nullptr};
}
const void* StubFor(std::uint32_t address, const char* who) {
    for (unsigned i = 0; i < g_slot_n; ++i)
        if (g_slots[i].address == address) return reinterpret_cast<const void*>(kStubs.f[i]);
    bof3::Fatal("magic_harness: %s calls 0x%X, which no stand-in covers: list it in the group's callees", who,
                (unsigned)address);
}

// --- the state both passes start from ------------------------------------------

constexpr unsigned kMaxRegions = 32;
Region g_regions[kMaxRegions];
unsigned g_region_n;
constexpr unsigned kMaxBytes = 0x10000;

struct State {
    unsigned char memory[kMaxBytes];
    Entry log[kLog];
    unsigned log_n;
};
unsigned g_bytes;

void Capture(State& s) {
    unsigned n = 0;
    for (unsigned i = 0; i < g_region_n; ++i) {
        std::memcpy(s.memory + n, Mem(g_regions[i].at), g_regions[i].size);
        n += g_regions[i].size;
    }
    std::memcpy(s.log, g_log, sizeof s.log);
    s.log_n = g_log_n;
}
void Apply(const State& s) {
    unsigned n = 0;
    for (unsigned i = 0; i < g_region_n; ++i) {
        std::memcpy(Mem(g_regions[i].at), s.memory + n, g_regions[i].size);
        n += g_regions[i].size;
    }
    std::memset(g_log, 0, sizeof g_log);
    g_log_n = 0;
    g_salt = 0;
    g_rand_pending = g_rand_first;
}
bool Same(const State& a, const State& b) {
    return a.log_n == b.log_n && std::memcmp(a.memory, b.memory, g_bytes) == 0 &&
           std::memcmp(a.log, b.log, sizeof a.log) == 0;
}
unsigned FirstDifference(const State& a, const State& b) {
    for (unsigned i = 0; i < g_bytes; ++i)
        if (a.memory[i] != b.memory[i]) return i;
    return g_bytes;
}
// The region and offset a state byte belongs to, for the log.
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

// Random bytes put back inside what every effect dereferences.
void Fix() {
    Sprite_Current = TaskAt(Next());
    SetPointer(at::kCurrentSlot, Half() ? Sprite_Current : TaskAt(Next()));
    SetPointer(at::kOwner, OwnerFor(Next()));
    SetPointer(at::kSource, g_records[Next() & 1]);
    Mem(at::kTarget)[0] = static_cast<unsigned char>(Next() % 11);
    Mem(at::kActor)[0] = static_cast<unsigned char>(Next() % 5);
    for (unsigned k = 0; k < 4; ++k) {
        unsigned char* const t = TaskAt(k);
        t[1] = static_cast<unsigned char>(Next() % 3);
        t[2] = static_cast<unsigned char>(Next() % 3);
        SetPointer(at::kTasks + k * at::kTaskStride + 0x80, OwnerFor(Next()));
    }
}

void PatchImms(void* copy, const Clone& c) {
    auto* code = static_cast<std::uint8_t*>(copy);
    for (int i = 0; i < c.n_imms; ++i) {
        std::uint32_t had;
        std::memcpy(&had, code + c.imms[i].offset, sizeof had);
        if (had != c.imms[i].value)
            bof3::Fatal("magic_harness: %s +0x%X holds 0x%X, not the handler 0x%X", c.name, (unsigned)c.imms[i].offset,
                        (unsigned)had, (unsigned)c.imms[i].value);
        const std::uint32_t to = Key(StubFor(had, c.name));
        std::memcpy(code + c.imms[i].offset, &to, sizeof to);
    }
    for (int i = 0; i < c.n_tables; ++i)
        move_script::Relocate(copy, c.base, c.size, {c.tables[i].jmp_disp, c.tables[i].table, c.tables[i].entries});
}

using Fn3 = std::uint32_t (__cdecl*)(std::uint32_t, std::uint32_t, std::uint32_t);

}  // namespace

// --- the public helpers -------------------------------------------------------

std::uint32_t Next() { g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5; return g_rng; }
bool Often() { return Next() % 3 != 0; }
bool Half() { return (Next() & 1) != 0; }
std::uint32_t Pick(const std::uint32_t* v, unsigned n) { return v[Next() % n]; }
unsigned char* Mem(std::uint32_t address) { return move_script::At(address); }
unsigned char* TaskAt(unsigned k) { return Mem(at::kTasks + (k % 4) * at::kTaskStride); }
unsigned char* SpriteRecord(unsigned k) { return g_records[k & 1]; }
unsigned char* EnemyOf(unsigned char target) {
    return Mem(at::kEnemies + static_cast<std::uint32_t>((static_cast<int>(target) - 3) * static_cast<int>(at::kEnemyStride)));
}
unsigned char* PartyOf(unsigned char actor) { return Mem(at::kParty + actor * at::kPartyStride); }
void SetPointer(std::uint32_t cell, const void* p) { move_script::SetLong(Mem(cell), static_cast<std::int32_t>(Key(p))); }
unsigned char* Pointer(std::uint32_t cell) {
    return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(move_script::Long(Mem(cell)))));
}
void SetRandHint(std::uint32_t hint) { g_rand_hint = hint; }
void LogValue(std::uint32_t v) { Log5(3000, v, 0, 0, 0); }
void LogBytes(const void* p, unsigned n) {
    std::uint32_t h = 0x811C9DC5u;
    for (unsigned i = 0; i < n; ++i) h = (h ^ static_cast<const unsigned char*>(p)[i]) * 0x01000193u;
    Log5(3001, h, n, 0, 0);
}
std::uint32_t Salted() {
    std::uint32_t h = Hash() ^ (++g_salt * 0x9E3779B9u);
    h ^= h >> 16;
    h *= 0x7FEB352Du;
    h ^= h >> 15;
    return h;
}
void FillBytes(void* p, unsigned n) {
    for (unsigned i = 0; i < n; ++i) static_cast<unsigned char*>(p)[i] = static_cast<unsigned char>(Salted() >> 7);
}
void SetRandFirst(int first) { g_rand_first = first; }

const void* StandIn(std::uint32_t key) {
    for (unsigned i = 0; i < g_slot_n; ++i)
        if (g_slots[i].key == key) return reinterpret_cast<const void*>(kStubs.f[i]);
    bof3::Fatal("magic_harness: ours calls 0x%X, which no stand-in covers: list it in the group's callees", (unsigned)key);
}

void Run(const Group& group) {
    const unsigned per = group.rounds ? group.rounds : 2000;
    g_group = &group;
    g_slot_n = 0;
    for (const Callee& c : kStandard) Register(c);
    for (unsigned i = 0; i < group.n_callees; ++i) Register(group.callees[i]);
    for (unsigned k = 0; k < group.n_clones; ++k)
        for (int i = 0; i < group.clones[k].n_imms; ++i) RegisterHandler(group.clones[k].imms[i].value);
    for (unsigned t = 0; t < group.n_data_tables; ++t)
        for (unsigned i = 0; i < group.data_tables[t].entries; ++i)
            RegisterHandler(static_cast<std::uint32_t>(move_script::Long(Mem(group.data_tables[t].at + 4 * i))));

    // the regions: the standard ones, then the group's
    g_region_n = 0;
    const Region standard[] = {
        {at::kTasks, at::kTaskCount * at::kTaskStride},
        {0x93B8C0, 0xA0},                                  // the current slot, the owner, and 0x93B8E0.. (an enemy index of 2)
        {0x937F88, 0x10},                                  // Sprite_Current, Gfx_ClutStripDirty, Frame_Counter
        {at::kBattle, at::kBattleSize},
        {at::kMessageUp, 4},
        {at::kParty, 5 * at::kPartyStride},
        {at::kEnemies, 8 * at::kEnemyStride},
        {Key(g_records), sizeof g_records},
    };
    for (const Region& r : standard) g_regions[g_region_n++] = r;
    for (unsigned i = 0; i < group.n_regions; ++i) {
        if (g_region_n == kMaxRegions) bof3::Fatal("magic_harness: %s: more than %u regions", group.shadow, kMaxRegions);
        g_regions[g_region_n++] = group.regions[i];
    }
    g_bytes = 0;
    for (unsigned i = 0; i < g_region_n; ++i) g_bytes += g_regions[i].size;
    if (g_bytes > kMaxBytes) bof3::Fatal("magic_harness: %s: the regions are %u bytes, the state holds %u", group.shadow, g_bytes, kMaxBytes);

    // the copies, every call out re-aimed at its recorder, the stack tables' immediates too
    static void* clones[256];
    if (group.n_clones > 256) bof3::Fatal("magic_harness: %s: more than 256 clones", group.shadow);
    for (unsigned k = 0; k < group.n_clones; ++k) {
        const Clone& c = group.clones[k];
        bof3::CloneCall calls[32];
        if (c.n_calls > 32) bof3::Fatal("magic_harness: %s has %d calls", c.name, c.n_calls);
        for (int i = 0; i < c.n_calls; ++i) calls[i] = {c.calls[i].offset, StubFor(c.calls[i].target, c.name), c.calls[i].target};
        clones[k] = bof3::CloneOriginal(c.name, c.base, c.size, calls, c.n_calls);
        PatchImms(clones[k], c);
    }

    // the .data tables' entries swapped for their recorders, put back after
    static std::uint32_t kept[64][16];
    if (group.n_data_tables > 64) bof3::Fatal("magic_harness: %s: more than 64 .data tables", group.shadow);
    for (unsigned t = 0; t < group.n_data_tables; ++t) {
        if (group.data_tables[t].entries > 16) bof3::Fatal("magic_harness: %s: a .data table of more than 16", group.shadow);
        for (unsigned i = 0; i < group.data_tables[t].entries; ++i) {
            const std::uint32_t cell = group.data_tables[t].at + 4 * i;
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
        for (unsigned i = 0; i < g_bytes; i += 4) {
            const std::uint32_t v = Next();
            std::memcpy(input.memory + i, &v, g_bytes - i < 4 ? g_bytes - i : 4);
        }
        std::memset(input.log, 0, sizeof input.log);
        input.log_n = 0;
        g_rand_first = -1;
        Apply(input);
        Fix();
        g_seed = Next();
        g_rand_hint = Next();
        if (group.seed) group.seed(k);
        Capture(input);

        const std::uint32_t a0 = Next(), a1 = Next(), a2 = Next();   // the task runner's, ignored
        const unsigned width = group.clones[k].answer_bytes;
        const std::uint32_t answer_mask = width >= 4 ? 0xFFFFFFFFu : (1u << (8 * width)) - 1;
        std::uint32_t answers[2] = {};
        for (int pass = 0; pass < 2; ++pass) {
            Apply(input);
            State& out = pass ? ours : theirs;
            const void* const fn = pass ? group.clones[k].ours : clones[k];
            g_active = pass == 1;
            answers[pass] = reinterpret_cast<Fn3>(const_cast<void*>(fn))(a0, a1, a2) & answer_mask;
            g_active = false;
            Capture(out);
        }

        calls += theirs.log_n;
        for (unsigned i = 0; i < theirs.log_n && i < kLog; ++i) {
            const std::uint32_t w = theirs.log[i].what;
            const unsigned s = w >= 1000 ? w - 1000 : w;
            if (s < g_slot_n) ++g_slots[s].calls;
        }
        if (theirs.log_n > kLog)
            bof3::Fatal("magic_harness: %s made %u calls, the log holds %u", group.clones[k].name, theirs.log_n, kLog);
        if (!Same(theirs, ours) || answers[0] != answers[1]) {
            ++bad_per[k];
            if (++bad <= 12) {
                std::uint32_t region, offset;
                const unsigned first = FirstDifference(theirs, ours);
                Where(first, region, offset);
                bof3::Log("shadow      %s self-test MISMATCH: round %u, %s, log %u / %u, first differing byte %u (0x%X + 0x%X)",
                          group.shadow, round, group.clones[k].name, theirs.log_n, ours.log_n, first, (unsigned)region,
                          (unsigned)offset);
            }
        }
    }
    Apply(saved);
    for (unsigned t = 0; t < group.n_data_tables; ++t)
        for (unsigned i = 0; i < group.data_tables[t].entries; ++i)
            move_script::SetLong(Mem(group.data_tables[t].at + 4 * i), static_cast<std::int32_t>(kept[t][i]));

    bof3::Log("shadow      %s self-test: %u rounds over %u functions (%u each), %u calls to the stand-ins, %u MISMATCHES; "
              "%u bytes of state (%u regions) and the stand-ins' log compared",
              group.shadow, per * group.n_clones, group.n_clones, per, calls, bad, g_bytes, g_region_n);
    char line[900];
    unsigned n = 0;
    for (unsigned i = 0; i < g_slot_n && n + 64 < sizeof line; ++i) {
        if (g_slots[i].calls == 0) continue;
        const int w = g_slots[i].handler
                          ? std::snprintf(line + n, sizeof line - n, "%sphase 0x%X %u", n ? ", " : "",
                                          (unsigned)g_slots[i].address, g_slots[i].calls)
                          : std::snprintf(line + n, sizeof line - n, "%s%s %u", n ? ", " : "", g_slots[i].name,
                                          g_slots[i].calls);
        if (w > 0) n += static_cast<unsigned>(w);
    }
    bof3::Log("shadow      %s coverage (calls the originals made): %s", group.shadow, n ? line : "none");
    if (bad) {
        for (unsigned k = 0; k < group.n_clones; ++k)
            if (bad_per[k]) bof3::Log("shadow      %s: %s mismatched in %u rounds", group.shadow, group.clones[k].name, bad_per[k]);
        bof3::Fatal("%s differs from the original in %u self-test rounds", group.shadow, bad);
    }
    g_group = nullptr;
}

}  // namespace magic_harness

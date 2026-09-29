// The boss round's shared harness (boss_harness.h; docs/boss_harness.md).
//
// A copy of magic_harness.cpp and area_harness.cpp adapted to the boss band
// (round eleven, group BH). What is the same: one pool of recording
// stand-ins, assigned at start-up to every callee a group's clones call (the
// standard set below, plus the group's own) and to every handler they
// dispatch to (stack-table immediates, .data table entries) - ours reaches the
// same recorders through BH_CALL / BH_AT / Phase while g_active is set; theirs
// (the copy) then ours from the same state, the regions and the recorders' log
// compared. What is different:
//
//   - the state is one battle frame with the event battle's cells: the task
//     slots, the enemies' objects and their working records, the party, the
//     battle bytes to 0x904BA0 (the three hooks among them), the current
//     sprite and enemy, the field's objects (the actors the set-ups find by
//     tag), the chapter bytes, the chapter's flag bits, the loaded area's
//     enemy rows and records, a packet buffer (section 4 of the doc);
//   - four recorders of the harness's own sit in the hook cells 0x904B64 /
//     68 / 6C and every enemy's +0xF4 at each round's start, so a function
//     that calls through a hook logs it;
//   - Group::fight is written into 0x904AAA and Group::kind into the current
//     enemy's +0x100 every round, and the real tables are left in place;
//   - each clone is called as its shape says (Clone::shape): the words a
//     hook is called with, a task slot or an enemy as Sprite_Current, a
//     dispatcher's state byte drawn below its table (Clone::states), or the
//     unit's own dispatcher with the clone planted in its table (Clone::via);
//     the three hooks are logged after every call;
//   - the disturbance moves battle cells: the current sprite and enemy, the
//     slot and owner, target and actor, the flags, the state bytes, a byte of
//     the current enemy's record (never its hook and table pointers), the
//     chapter bytes, the battle-end byte, the fight byte (among the values
//     boss code compares it with).
//
// It keeps its own recorders and its own g_active: magic_harness,
// scenario_harness and area_harness are not touched, and all can run in one
// process (BOF3X_SHADOW='*').
#include "game/boss_harness.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <utility>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace boss_harness {

bool g_active = false;

namespace {

std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> std::uint32_t KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }

// --- the random source and the recorders' log ---------------------------------

std::uint32_t g_rng = 0xB0551101u;

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
constexpr std::uint32_t kShapeTag = 5000;   // + shape: the hooks read back after the call

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
// Gfx_PacketNext points in here at each round's start; Gfx_CommitPrim's
// stand-in advances it as the real one does, wrapping before the end.
constexpr unsigned kPacketBytes = 0x4000;
alignas(16) unsigned char g_packets[kPacketBytes];
// The engine frame's text buffer (Group::engine): the strings TextPtrEffect
// answers, a region, a NUL every 16th byte each round.
constexpr unsigned kTextBytes = 0x200;
alignas(16) unsigned char g_text[kTextBytes];

const Group* g_group = nullptr;
const Clone* g_clone = nullptr;   // the clone being fuzzed (its shape)
std::uint32_t g_via_fn = 0;       // the function planted in a Via's cell for this pass

// --- the regions (the state both passes start from) ----------------------------

constexpr unsigned kMaxRegions = 64;       // 40 before round twelve (capacity only)
Region g_regions[kMaxRegions];
unsigned g_region_n;
constexpr unsigned kMaxBytes = 0x20000;    // 0x10000 before round twelve (capacity only)

bool InRegions(const unsigned char* p, unsigned n) {
    const std::uint32_t a = Key(p);
    for (unsigned i = 0; i < g_region_n; ++i)
        if (a >= g_regions[i].at && a + n <= g_regions[i].at + g_regions[i].size) return true;
    return false;
}
std::uint32_t ByteAt(const unsigned char* p, unsigned off) { return InRegions(p + off, 1) ? p[off] : 0x100u; }

bool TaskShape() { return g_clone && g_clone->shape == Shape::kTask; }
// The round-twelve shapes (boss_harness.h): Sprite_Current a party member for
// kMember, a member or an enemy for the other three.
bool EngineShape() { return g_clone && g_clone->shape >= Shape::kStep; }
bool MemberShape() { return g_clone && g_clone->shape == Shape::kMember; }
unsigned char* SpriteFor(unsigned v) {
    if (EngineShape()) return MemberShape() || (v & 0x10) ? PartyOf(static_cast<unsigned char>(v % at::kPartyCount)) : EnemyAt(v);
    return TaskShape() ? TaskAt(v) : EnemyAt(v);
}
unsigned char* OwnerFor(unsigned v) { return v & 4 ? g_records[v & 1] : TaskAt(v); }
unsigned char* CurrentEnemy() { return Pointer(at::kEnemyCurrent); }

// The values boss code compares the fight byte with (the plan's section 2),
// and the group's own.
unsigned char FightValue(std::uint32_t h) {
    static const unsigned char kCompared[] = {0x10, 0x19, 0x1A, 0x25};
    if (g_group && g_group->fight >= 0 && (h & 1)) return static_cast<unsigned char>(g_group->fight);
    return kCompared[(h >> 1) % 4];
}

// The engine frame's cells a caller reads again after a call (round twelve,
// an engine group's case 12): the window record 0x905B84 names and a byte of
// it (+2 / +3 below phase_span when the group sets it: a window's stack-table
// indices), Field_State, the menu actor with its command record, the step
// bytes 0x904AA1..0x904AA4 (below phase_span likewise), the result record.
void EngineDisturb(std::uint32_t h) {
    const unsigned v = (h >> 12) & 0xFF;
    const auto b = static_cast<unsigned char>(h >> 20);
    const unsigned span = g_group ? g_group->phase_span : 0;
    switch ((h >> 9) % 6) {
    case 0: SetPointer(at::kWindowCurrent, WindowAt(v)); break;
    case 1: {
        unsigned off = (h >> 24) % at::kWindowStride;
        unsigned char* const w = CurrentWindow();
        if (InRegions(w + off, 1)) w[off] = (off == 2 || off == 3) && span ? static_cast<unsigned char>(v % span) : b;
        break;
    }
    case 2: SetPointer(at::kMemberCurrent, PartyOf(static_cast<unsigned char>(v % at::kPartyCount))); break;
    case 3: {
        unsigned char* const m = PartyOf(static_cast<unsigned char>(v % at::kPartyCount));
        SetPointer(at::kMenuActor, m);
        SetPointer(at::kMenuCommand, m + at::kMenuCommandAt);
        break;
    }
    case 4: Mem(at::kStep + (h >> 24) % 4)[0] = span ? static_cast<unsigned char>(v % span) : b; break;
    default:
        SetPointer(at::kResultRecord, (v & 1 ? PartyOf(static_cast<unsigned char>(v % at::kPartyCount)) : EnemyAt(v)) + at::kResultAt);
        break;
    }
}

// Every cell below is one some boss function or its engine callee reads
// again after a call.
void Disturb() {
    const std::uint32_t h = Hash();
    if (g_calm || h % 3 == 0) return;
    const unsigned v = (h >> 12) & 0xFF;
    const auto b = static_cast<unsigned char>(h >> 20);
    switch ((h >> 4) % 16) {
    case 0: Sprite_Current = SpriteFor(v); break;
    case 1: SetPointer(at::kEnemyCurrent, EnemyAt(v)); break;
    case 2: SetPointer(at::kOwner, OwnerFor(v)); break;
    case 3: SetPointer(at::kCurrentSlot, TaskAt(v >> 2)); break;
    case 4: Mem(at::kTarget)[0] = static_cast<unsigned char>(v % 11); break;
    case 5: Mem(at::kActor)[0] = static_cast<unsigned char>(v % 3); break;
    case 6: Mem(at::kMessageUp)[0] = b; break;
    case 7: Mem(at::kFlags)[0] = b; break;
    case 8: Frame_Counter = h >> 6; break;
    case 9: case 10: {
        static const unsigned kFields[] = {0, 1, 2, 3, 4, 5, 8, 9, 0xA, 0xB, 0x2E, 0x30, 0x48, 0x4A, 0x4B, 0x92};
        const unsigned f = kFields[v % 16];
        const unsigned span = g_group ? g_group->phase_span : 0;
        if (InRegions(Sprite_Current + f, 1))
            Sprite_Current[f] = f >= 1 && f <= 4 && span ? static_cast<unsigned char>(b % span) : b;
        break;
    }
    case 11: {
        // a byte of the current enemy's record - never its hook and table
        // pointers +0xF4..+0xFF or its kind +0x100 (a dispatcher's index)
        unsigned off = (h >> 20) % at::kEnemyStride;
        if (off >= 0xF4 && off <= 0x100) off -= 0x80;
        unsigned char* const e = CurrentEnemy();
        // a state byte +1..+4 stays below phase_span, as in cases 9 and 10
        // (round 11 doc section 5.4: BSD's kind 26 saw a real Fatal here)
        const unsigned span = g_group ? g_group->phase_span : 0;
        if (InRegions(e + off, 1))
            e[off] = static_cast<unsigned char>(off >= 1 && off <= 4 && span ? v % span : v);
        break;
    }
    case 12:
        // the chapter bytes for a boss group; the engine cells for an engine
        // group (round twelve: EngineDisturb, below), which has no chapter
        if (g_group && g_group->engine) EngineDisturb(h);
        else Mem(h & 0x100 ? at::kChapterRun : at::kChapterStep)[0] = b;
        break;
    case 13: Mem(at::kBattleEnd)[0] = b; break;
    case 14:
        if (g_group && g_group->disturb) g_group->disturb(h);
        break;
    case 15: Mem(at::kFight)[0] = FightValue(h >> 8); break;
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
    bool handler;            // a table entry or a hook: logs the sprite (and a hook's argument)
    unsigned calls;
};
constexpr unsigned kSlots = 512;   // 256 before round twelve (capacity only: the engine set adds ~110)
Slot g_slots[kSlots];
unsigned g_slot_n;

std::uint32_t Cur() { return Key(Sprite_Current); }
std::uint32_t Enemy() { return static_cast<std::uint32_t>(move_script::Long(Mem(at::kEnemyCurrent))); }
std::uint32_t States() {
    return ByteAt(Sprite_Current, 1) | ByteAt(Sprite_Current, 2) << 9 | ByteAt(Sprite_Current, 3) << 18 |
           (ByteAt(Sprite_Current, 4) & 0xFF) << 27;
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
        // a table entry logs the sprite it ran for; a hook (nargs) its argument too
        Log5(kPhaseTag + I, Cur(), Enemy(), States(), s.nargs ? a0 & s.masks[0] : 0);
        Disturb();
        return Hash();
    }
    if (s.answer == Answer::kPhase) {
        Log5(I, Cur(), Enemy(), States(), s.masks[0] ? static_cast<std::uint32_t>(move_script::Long(Mem(s.masks[0]))) : 0);
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

// --- the standard callees' effects ---------------------------------------------

// Gfx_CommitPrim(slot, size): Gfx_PacketNext += size (a byte), as the real one
// does when the pool has room - its callers read the pointer again for the
// next primitive. Wrapped to the buffer's start before its end, the same on
// both passes.
std::uint32_t CommitEffect(const std::uint32_t* a, std::uint32_t answer) {
    unsigned char* next = Pointer(at::kPacketNext) + (a[1] & 0xFF);
    if (next < g_packets || next + 0x100 > g_packets + kPacketBytes) next = g_packets;
    SetPointer(at::kPacketNext, next);
    return answer;
}
// BossActor_Find(tag): a pointer to one of the first four field objects (in
// the compared state), never null - its callers write through it without a
// test (Boss01's exit hook 0x437E10); a caller that tests for null lists the
// callee again with an effect of its own.
std::uint32_t ActorFindEffect(const std::uint32_t*, std::uint32_t) { return Key(Object(Noise() % 4)); }
// BossActor_ClearBit40(tag): the real one clears bit 0x40 of the tagged
// actor's +0; this one flips it in one of the first four, so a caller that
// reads the actor again after sees a change.
std::uint32_t ActorBitEffect(const std::uint32_t*, std::uint32_t answer) {
    Object(Noise() % 4)[0] ^= 0x40;
    return answer;
}

}  // namespace

// --- the stage-B groups' louder stand-ins, folded (boss_harness.h) -------------

// Battle_RemoveFromTurnOrder: the end hooks of set-ups 26, 27, 32, 34 and 52
// read the member's +0x90 and +8 after it, and the next member's +0 - moved
// in a random member (BSE's RemoveEffect, then BSF, BSG, BSH, BSJ's copies).
std::uint32_t TurnOrderEffect(const std::uint32_t*, std::uint32_t answer) {
    const std::uint32_t n = Noise();
    unsigned char* const p = PartyOf(static_cast<unsigned char>(n % 3));
    switch ((n >> 4) % 3) {
    case 0: p[0x91] ^= 0x40; break;
    case 1: p[8] = static_cast<unsigned char>(n >> 8); break;
    default: p[0] ^= 1; break;
    }
    return answer;
}
// 0x446DE0 (the end phase's step 1): the step as the caller left it is
// logged first - set-ups 35 and 36 store it before the call, and Boss34_End
// increments it after - then moved (BSH's StepEffect; BSC's form moved the
// move counter instead, MoveCounterEffect).
std::uint32_t EndWinEffect(const std::uint32_t*, std::uint32_t answer) {
    Note(Mem(at::kChapterStep)[0]);
    Mem(at::kChapterStep)[0] = static_cast<unsigned char>(Noise());
    return answer;
}
// Msg_OpenScript: set-up 27's hooks read the script bits 0x904AAD again after
// it (BSC's BitsEffect). In the battle frame, so every group's.
std::uint32_t ScriptBitsEffect(const std::uint32_t*, std::uint32_t answer) {
    if (Noise() & 1) Mem(0x904AAD)[0] = static_cast<unsigned char>(Noise());
    return answer;
}
// Scenario_CallA: the move counter 0x903848, half the time only - the end
// hooks store it before the call, and an effect that always overwrote it would
// hide the store (BSC's SceneEffect). The cell is a group region: outside one
// the effect draws nothing, so a group that does not list it fuzzes as before.
std::uint32_t MoveCounterEffect(const std::uint32_t*, std::uint32_t answer) {
    if (!InRegions(Mem(0x903848), 1)) return answer;
    const std::uint32_t n = Noise();
    if (n & 2) Mem(0x903848)[0] = static_cast<unsigned char>(n & 1 ? 0x14 : n >> 8);
    return answer;
}
// Battle_OpenMsgWindow: the banner's character 0x66972D, read after by the
// event hooks (BSC's CharEffect). A group region, as above.
std::uint32_t BannerCharEffect(const std::uint32_t*, std::uint32_t answer) {
    if (!InRegions(Mem(0x66972D), 1)) return answer;
    Mem(0x66972D)[0] = static_cast<unsigned char>(Noise());
    return answer;
}
// BattleTask_Create answering none (BSG's CreateEffect): F6's strike tests for
// it; the seven that do not (D163) abort in ours, so this is opt-in.
std::uint32_t CreateMayFail(const std::uint32_t*, std::uint32_t answer) {
    const std::uint32_t n = Noise();
    return (answer & 0xFFFFFF00u) | (n % 3 == 0 ? 0xFFu : (n >> 8) % 48);
}

// --- the engine's typed stand-ins (round twelve, group EH) ------------------------

namespace {

// Where a string argument may be read: the compared state, the harness's
// text buffer, the loaded image (its .data strings and tables) or this
// thread's stack (a caller's buffer). Anything else - a garbage pointer a
// quieter stand-in answered - is noted as unreadable, never followed.
bool Readable(std::uint32_t p) {
    if (p >= Key(g_text) && p < Key(g_text) + kTextBytes) return true;
    if (InRegions(reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(p)), 1)) return true;
    const std::uint32_t image = 0x400000;
    const std::uint32_t pe = image + *reinterpret_cast<const std::uint32_t*>(static_cast<std::uintptr_t>(image + 0x3C));
    const std::uint32_t size = *reinterpret_cast<const std::uint32_t*>(static_cast<std::uintptr_t>(pe + 24 + 56));   // SizeOfImage
    if (p >= image + 0x1000 && p < image + size) return true;
    std::uint32_t base, limit;
    asm("movl %%fs:4, %0" : "=r"(base));
    asm("movl %%fs:8, %0" : "=r"(limit));
    return p >= limit && p < base;
}
// Where an effect may write a string: the compared state, the text buffer,
// this thread's stack - never the rest of the image (a static buffer outside
// the regions would keep what the self-test wrote).
bool Writable(std::uint32_t p, unsigned n) {
    if (p >= Key(g_text) && p + n <= Key(g_text) + kTextBytes) return true;
    if (InRegions(reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(p)), n)) return true;
    std::uint32_t base, limit;
    asm("movl %%fs:4, %0" : "=r"(base));
    asm("movl %%fs:8, %0" : "=r"(limit));
    return p >= limit && p + n <= base;
}
// The string at p to its NUL, 64 bytes at most, noted (its hash and length),
// or "unreadable" (0xFFFFFFFF). Returns its length.
unsigned NoteString(std::uint32_t p) {
    unsigned n = 0;
    std::uint32_t hash = 0x811C9DC5u;
    if (!Readable(p)) {
        Note(0xFFFFFFFFu);
        return 0;
    }
    const auto* s = reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(p));
    while (n < 64 && Readable(p + n) && s[n] != 0) hash = (hash ^ s[n++]) * 0x01000193u;
    Note(hash, n);
    return n;
}

}  // namespace

// Window_Alloc(slot, kind): claims record `slot` when its byte 0 is free (1,
// +1 the kind, +2 / +3 0) and answers the slot, else 0xFF - the real one
// (symbols.toml), so a caller that reads the record after sees what it would.
// A slot past the 22 is noted and answered 0xFF, the record left alone.
std::uint32_t WindowAllocEffect(const std::uint32_t* a, std::uint32_t answer) {
    const unsigned slot = a[0] & 0xFF;
    if (slot >= at::kWindowCount) {
        Note(slot, 0xFF);
        return answer | 0xFFu;
    }
    unsigned char* const w = WindowAt(slot);
    if (w[0] != 0) return answer | 0xFFu;
    w[0] = 1;
    w[1] = static_cast<unsigned char>(a[1]);
    w[2] = w[3] = 0;
    return (answer & 0xFFFFFF00u) | slot;
}
// Window_FreeCurrent: bytes +0, +2 and +3 of the record 0x905B84 names zeroed
// (+1 kept), as the real one - its callers read the record again.
std::uint32_t WindowFreeEffect(const std::uint32_t*, std::uint32_t answer) {
    unsigned char* const w = CurrentWindow();
    if (InRegions(w, 4)) w[0] = w[2] = w[3] = 0;
    return answer;
}
// A string of the text buffer (16 bytes apart, each NUL-ended by Fix): for a
// stand-in answering a text pointer its caller follows (Msg_SystemPtr,
// Item_NamePtr in the engine set).
std::uint32_t TextPtrEffect(const std::uint32_t*, std::uint32_t) { return Key(g_text) + 16 * (Noise() % (kTextBytes / 16)); }
// Crt_sprintf(dst, fmt, ...): the format's first 15 bytes (to its NUL) and a
// NUL written into dst, when dst is a buffer of the compared state or the
// stack, and noted - the caller draws dst after (BE1's and BE7's windows), so
// dst holds the same string on both passes. A static buffer outside the
// regions (BATE's 0x904BA0) is left alone and only the format noted: a group
// that draws it lists the buffer as a region.
std::uint32_t SprintfEffect(const std::uint32_t* a, std::uint32_t answer) {
    if (!Writable(a[0], 16)) {
        NoteString(a[1]);
        return answer;
    }
    auto* d = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a[0]));
    const auto* f = reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(a[1]));
    unsigned n = 0;
    if (Readable(a[1]))
        while (n < 15 && Readable(a[1] + n) && f[n] != 0) {
            d[n] = f[n];
            ++n;
        }
    d[n] = 0;
    NoteString(a[0]);
    return (answer & 0xFFFFFF00u) | n;
}
std::uint32_t TextArg0Effect(const std::uint32_t* a, std::uint32_t answer) {
    NoteString(a[0]);
    return answer;
}
std::uint32_t TextArg3Effect(const std::uint32_t* a, std::uint32_t answer) {
    NoteString(a[3]);
    return answer;
}
// Text_DrawAt / Text_DrawSmall answer where the text stopped: the argument
// past its string (a caller continues from it), not garbage.
std::uint32_t TextArg4Effect(const std::uint32_t* a, std::uint32_t) {
    const unsigned n = NoteString(a[4]);
    return Readable(a[4]) ? a[4] + n : a[4];
}
std::uint32_t BannerTextEffect(const std::uint32_t* a, std::uint32_t answer) {
    NoteString(a[4]);
    return answer;
}

namespace {

// --- the standard callees -------------------------------------------------------
// The louder forms (above) are the standard since round eleven's cleanup; the
// groups that listed their own keep them (a group's listing stands).
//
// The boss band's frontier (tools/boss_rows.py, 2026-09-28: 117 functions, the
// callees the helpers reach added): every one with a signature in
// symbols.toml, called by a rel32 from the band, plus the spawn helpers
// (boss_spawn.cpp, group BH) and the three battle-end setters every hook
// tail-jumps to. BH_OURS for a callee that is ours, BH_THEIRS for one that is
// Capcom's; one that changes hands is caught at start-up (Register) and moves
// line. The .data table entries the kinds reach (EnemyOp_*, 0x4365D0, ...) are
// not here: a group lists its tables (DataTable) and their entries get
// handler recorders. Masks from the parameter types (a byte's 0xFF, a short's
// 0xFFFF); answers kFlag for a byte a caller tests, kByte for an index,
// garbage otherwise. A callee whose answer is a pointer the caller follows
// wants the group's own listing with an effect (BossActor_Find has one here).
#define BH_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
#define BH_THEIRS(name) #name, KeyOf(name), KeyOf(name)
constexpr std::uint32_t kAll = 0xFFFFFFFFu, kU8 = 0xFFu, kU16 = 0xFFFFu;

const Callee kStandard[] = {
    // the battle engine
    {BH_OURS(BattleTask_Create), 2, {kU8, kU8}, Answer::kByte, 0, at::kTaskCount - 1},
    {BH_OURS(BattleTask_FreeCurrent), 0, {}, Answer::kGarbage, 0, 0},
    {BH_OURS(BattleEnemy_SetAnimation), 1, {kU8}, Answer::kGarbage, 0, 0},
    {BH_OURS(BattleEnemy_ScriptTick), 0, {}, Answer::kFlag, 0, 0},
    {BH_OURS(BattleEnemy_ScriptTickOnce), 0, {}, Answer::kFlag, 0, 0},
    {BH_OURS(Battle_EnemyDefeated), 0, {}, Answer::kGarbage, 0, 0},
    {BH_OURS(BattleWin_DrawMediumBox), 2, {kAll, kAll}, Answer::kGarbage, 0, 0},
    {BH_OURS(Battle_OpenMsgWindow), 0, {}, Answer::kGarbage, 0, 0, {}, &BannerCharEffect},
    {BH_OURS(BattleWin_DrawTileRgb), 5, {kAll, kAll, kAll, kAll, kAll}, Answer::kGarbage, 0, 0},
    {BH_OURS(Battle_ActorIsOut), 1, {kU8}, Answer::kFlag, 0, 0},
    {BH_OURS(Battle_RemoveFromTurnOrder), 1, {kU8}, Answer::kGarbage, 0, 0, {}, &TurnOrderEffect},
    {BH_OURS(Battle_LoadSoundByKey), 2, {kAll, kAll}, Answer::kFlag, 0, 0},
    {BH_OURS(Battle_ClearActorBit), 1, {kU8}, Answer::kGarbage, 0, 0},
    {BH_OURS(BattleBanner_Add), 5, {kAll, kAll, kAll, kAll, kAll}, Answer::kGarbage, 0, 0},
    {BH_OURS(Battle_SetTargetFlag40), 1, {kU8}, Answer::kGarbage, 0, 0},
    {BH_OURS(Battle_CopyEnemyData), 2, {kAll, kAll}, Answer::kGarbage, 0, 0},
    {BH_OURS(BattleFx_FreeTask), 0, {}, Answer::kGarbage, 0, 0},
    {BH_OURS(Transition_Start), 1, {kU8}, Answer::kGarbage, 0, 0},
    {BH_OURS(LoadDatFile), 1, {kAll}, Answer::kGarbage, 0, 0},
    {BH_OURS(File_LoadDone), 0, {}, Answer::kFlag, 0, 0},
    {BH_OURS(Port_DroppedCall), 1, {kU8}, Answer::kGarbage, 0, 0},
    // the battle's way out, the hooks' tail jumps (engine code nobody owns):
    // 0x904AA0 = 5 and 0x904AA2 = 0, with 0x904AA1 = 1 (the win), 2, 3
    {"0x446DE0", 0x446DE0, 0x446DE0, 0, {}, Answer::kGarbage, 0, 0, {}, &EndWinEffect},
    {"0x446E00", 0x446E00, 0x446E00, 0, {}, Answer::kGarbage, 0, 0},
    {"0x446E20", 0x446E20, 0x446E20, 0, {}, Answer::kGarbage, 0, 0},
    // the spawn helpers (boss_spawn.cpp): the field actors a set-up finds by tag
    {BH_OURS(EnemyData_FindByTag), 1, {kU8}, Answer::kByte, 0xFF, 7},
    {BH_OURS(BossActor_Find), 1, {kU8}, Answer::kGarbage, 0, 0, {}, &ActorFindEffect},
    {BH_OURS(BossActor_Index), 1, {kU8}, Answer::kByte, 0xFF, at::kObjectCount - 1},
    {BH_OURS(BossActor_ClearBit40), 1, {kU8}, Answer::kGarbage, 0, 0, {}, &ActorBitEffect},
    {BH_OURS(BossActor_CopyFrom), 3, {kU8, kAll, kU8}, Answer::kGarbage, 0, 0},
    {BH_OURS(BossActor_Clear), 1, {kU8}, Answer::kGarbage, 0, 0},
    // messages, text, the field
    {BH_OURS(Msg_OpenScript), 1, {kU16}, Answer::kGarbage, 0, 0, {}, &ScriptBitsEffect},
    {BH_OURS(Msg_SystemPtr), 1, {kU16}, Answer::kGarbage, 0, 0},   // answers a pointer: a caller that follows it wants an effect
    {BH_OURS(Text_DrawAt), 5, {kAll, kAll, kAll, kAll, kAll}, Answer::kGarbage, 0, 0},   // answers a pointer
    {BH_OURS(Text_DrawFont12), 4, {kAll, kAll, kAll, kAll}, Answer::kGarbage, 0, 0},
    {BH_OURS(Str_CopyN), 3, {kAll, kAll, kAll}, Answer::kGarbage, 0, 0},
    {BH_OURS(Field_MemberSprite), 2, {kAll, kAll}, Answer::kGarbage, 0, 0},
    {BH_OURS(Scenario_CallA), 1, {kAll}, Answer::kGarbage, 0, 0, {}, &MoveCounterEffect},
    {BH_OURS(AreaMap_Elevation), 2, {kAll, kAll}, Answer::kGarbage, 0, 0},
    {BH_OURS(MoveCmd_TestFB), 2, {kU16, kU16}, Answer::kFlag, 0, 0},
    {BH_OURS(Flags_Set), 2, {kAll, kU8}, Answer::kGarbage, 0, 0},
    {BH_OURS(Flags_Clear), 2, {kAll, kU8}, Answer::kGarbage, 0, 0},
    {BH_OURS(Flags_Test), 2, {kAll, kU8}, Answer::kBool, 0, 0},
    {BH_OURS(AbilityList_Add), 4, {kAll, kAll, kAll, kAll}, Answer::kFlag, 0, 0},
    {BH_OURS(MoveCmd_OpE9), 7, {kAll, kU8, kU8, kU16, kU16, kU8, kU8}, Answer::kFlag, 0, 0},
    {BH_THEIRS(Crt_sprintf), 4, {kAll, kAll, kAll, kAll}, Answer::kGarbage, 0, 0},
    // sound and music
    {BH_OURS(Sound_PlayEffect), 1, {kU16}, Answer::kGarbage, 0, 0},
    {BH_OURS(Sound_PlayById), 1, {kU16}, Answer::kGarbage, 0, 0},
    // sprites
    {BH_OURS(Sprite_UpdateScreen), 0, {}, Answer::kGarbage, 0, 0},
    {BH_OURS(Sprite_QueueOverlay), 0, {}, Answer::kGarbage, 0, 0},
    {BH_OURS(Sprite_PoseFromSet), 3, {kAll, kAll, kAll}, Answer::kGarbage, 0, 0},
    {BH_OURS(Sprite_SetAnimation), 1, {kU8}, Answer::kGarbage, 0, 0},
    {BH_OURS(Sprite_SetAnimationAt), 2, {kU8, kU16}, Answer::kGarbage, 0, 0},
    {BH_OURS(Sprite_EnsureAnimation), 1, {kU8}, Answer::kFlag, 0, 0},
    {BH_OURS(Sprite_ScriptTick), 0, {}, Answer::kFlag, 0, 0},
    {BH_OURS(Sprite_ScriptTickOnce), 0, {}, Answer::kFlag, 0, 0},
    {BH_OURS(Sprite_SetAnimationBank), 1, {kU16}, Answer::kFlag, 0, 0},
    {BH_OURS(Effect_Release), 0, {}, Answer::kGarbage, 0, 0},
    // the effect library
    {BH_OURS(MagicFx_StepToward), 2, {kAll, kU16}, Answer::kGarbage, 0, 0},
    {BH_OURS(MagicFx_NearSprite3D), 2, {kAll, kAll}, Answer::kFlag, 0, 0},
    {BH_OURS(MagicFx_CenterOnSide), 0, {}, Answer::kGarbage, 0, 0},
    // drawing
    {BH_OURS(Gfx_ClearRect), 4, {kAll, kAll, kAll, kAll}, Answer::kGarbage, 0, 0},
    {BH_OURS(Gfx_CommitPrim), 2, {kU8, kU8}, Answer::kGarbage, 0, 0, {}, &CommitEffect},
    {BH_OURS(Gpu_SetPolyFT4), 1, {kAll}, Answer::kGarbage, 0, 0},
    {BH_OURS(Gpu_SetPolyG3), 1, {kAll}, Answer::kGarbage, 0, 0},
    {BH_OURS(Gpu_SetSprt), 1, {kAll}, Answer::kGarbage, 0, 0},
    {BH_OURS(Gpu_SetSemiTrans), 2, {kAll, kU8}, Answer::kGarbage, 0, 0},
    {BH_OURS(Gpu_SetDrawMode), 5, {kAll, kAll, kAll, kAll, kAll}, Answer::kGarbage, 0, 0},
    {BH_OURS(Gpu_GetTPage), 4, {kU8, kU8, kAll, kAll}, Answer::kGarbage, 0, 0},
    {BH_OURS(Gte_PushMatrix), 0, {}, Answer::kGarbage, 0, 0},
    {BH_OURS(Gte_PopMatrix), 0, {}, Answer::kGarbage, 0, 0},
    {BH_OURS(Gte_RotMatrixYXZ), 2, {kAll, kAll}, Answer::kGarbage, 0, 0},   // answers a pointer
    {BH_OURS(Gte_TransMatrix), 2, {kAll, kAll}, Answer::kGarbage, 0, 0},
    {BH_OURS(Gte_RotTrans), 2, {kAll, kAll}, Answer::kGarbage, 0, 0},
    {BH_OURS(Gte_SetRotMatrix), 1, {kAll}, Answer::kGarbage, 0, 0},
    {BH_OURS(Gte_SetTransMatrix), 1, {kAll}, Answer::kGarbage, 0, 0},
    {BH_OURS(Math_Sin), 1, {kAll}, Answer::kGarbage, 0, 0},
    {BH_OURS(Math_Cos), 1, {kAll}, Answer::kGarbage, 0, 0},
    {BH_THEIRS(Rand), 0, {}, Answer::kRand, 0, 0},
};

// The battle engine's standard callees (round twelve, group EH;
// docs/boss_harness.md section 10.4): the frontier of the 308 functions of
// BE1..BE7 (every rel32 call or tail jmp from their extents to a function
// outside them, analysis/round12_cut.tsv at 430f34b, capstone) that
// kStandard lacks, plus the louder forms of five kStandard callees that the
// engine calls with a stack buffer or follows the answer of (Crt_sprintf,
// Msg_SystemPtr, Text_DrawAt, Text_DrawFont12, BattleBanner_Add). Registered
// for an engine group only, and before kStandard, so these stand there;
// a boss group never sees them. Masks: a byte or a short where the callee
// reads only that (its first read of the argument, capstone), a string
// argument 0 with its content noted by the effect (a caller's stack buffer
// is at a different address in the copy and in ours), everything else whole.
// kThrough for the pure ones (both sides run the real code: 0x446F20 /
// 0x446F50 / 0x446F80 are (a * b) / 100 clamped to 999 / 9999 / 100,
// Battle_WrapIndex, 0x5B9450 is the CRT's memcpy) and the GTE family BE6's
// BMAGIC slots read results of through pointers (Gte_* below; the five GTE
// matrix calls kStandard records stay recorders).
const Callee kEngineStandard[] = {
    // text and messages: strings noted, pointers answered into the text buffer
    {BH_OURS(Msg_SystemPtr), 1, {kU16}, Answer::kGarbage, 0, 0, {}, &TextPtrEffect},
    {BH_OURS(Item_NamePtr), 2, {kU8, kU8}, Answer::kGarbage, 0, 0, {}, &TextPtrEffect},
    // the destination, the format and one value: a word past what the caller pushed is its own
    // frame, different in the copy and in ours (self-test); a group formatting more lists its own
    {BH_THEIRS(Crt_sprintf), 3, {0, kAll, kAll}, Answer::kGarbage, 0, 0, {}, &SprintfEffect},
    {BH_OURS(Text_DrawAt), 5, {kAll, kAll, kAll, kAll, 0}, Answer::kGarbage, 0, 0, {}, &TextArg4Effect},
    {BH_OURS(Text_DrawSmall), 5, {kAll, kAll, kAll, kU8, 0}, Answer::kGarbage, 0, 0, {}, &TextArg4Effect},
    {BH_OURS(Text_DrawFont12), 4, {kAll, kAll, kAll, 0}, Answer::kGarbage, 0, 0, {}, &TextArg3Effect},
    {BH_OURS(Text_DrawFont8), 4, {kAll, kAll, kAll, 0}, Answer::kGarbage, 0, 0, {}, &TextArg3Effect},
    {BH_OURS(Text_CharCount), 1, {0}, Answer::kByte, 0, 17, {}, &TextArg0Effect},
    {BH_OURS(BattleBanner_Add), 5, {kAll, kAll, kAll, kAll, 0}, Answer::kGarbage, 0, 0, {}, &BannerTextEffect},
    {BH_OURS(BattleBanner_Set), 6, {kAll, kU8, kU8, kU8, kU8, kAll}, Answer::kGarbage, 0, 0},
    {BH_OURS(BattleBanner_ShowName), 1, {kAll}, Answer::kGarbage, 0, 0},
    {BH_OURS(BattleBanner_ClearAll), 0, {}, Answer::kGarbage, 0, 0},
    {BH_OURS(Item_HelpMessage), 2, {kU8, kU8}, Answer::kGarbage, 0, 0},
    // windows: the records claimed and freed as the real ones do
    {BH_OURS(Window_Alloc), 2, {kAll, kU8}, Answer::kGarbage, 0, 0, {}, &WindowAllocEffect},
    {BH_OURS(Window_FreeCurrent), 0, {}, Answer::kGarbage, 0, 0, {}, &WindowFreeEffect},
    {BH_OURS(Window_ResetAll), 0, {}, Answer::kGarbage, 0, 0},
    {BH_OURS(ItemMenu_FreeWindows), 0, {}, Answer::kGarbage, 0, 0},
    // menu and window drawing
    {BH_OURS(Menu_DrawPiece), 4, {kU16, kU16, kAll, kAll}, Answer::kGarbage, 0, 0},
    {BH_OURS(Menu_DrawPieces), 4, {kAll, kAll, kAll, kAll}, Answer::kGarbage, 0, 0},
    {BH_OURS(Menu_DrawBox), 6, {kAll, kAll, kAll, kAll, kU8, kU8}, Answer::kGarbage, 0, 0},   // the colour: a byte its callers load into al only (0x42D8C0), self-test
    {BH_OURS(Menu_DrawBorder), 4, {kAll, kAll, kU8, kU8}, Answer::kGarbage, 0, 0},
    {BH_OURS(Menu_DrawBackdrop), 1, {kAll}, Answer::kGarbage, 0, 0},
    {BH_OURS(Menu_DrawHand), 3, {kAll, kAll, 0}, Answer::kGarbage, 0, 0},
    {BH_OURS(Menu_DrawIcon), 6, {kAll, kAll, kAll, kAll, kAll, kAll}, Answer::kGarbage, 0, 0},
    {BH_OURS(Menu_DrawIcon8), 4, {kAll, kAll, kAll, kAll}, Answer::kGarbage, 0, 0},
    {BH_OURS(Menu_DrawScrollBar), 7, {kAll, kAll, kAll, kAll, kU8, kAll, kAll}, Answer::kGarbage, 0, 0},
    {BH_OURS(BattleWin_DrawCommandLabel), 1, {kAll}, Answer::kGarbage, 0, 0},
    {BH_OURS(BattleWin_DrawCommandCross), 2, {kAll, kAll}, Answer::kGarbage, 0, 0},
    {BH_OURS(BattleWin_DrawPartyStatus), 2, {kAll, kAll}, Answer::kGarbage, 0, 0},
    {BH_OURS(BattleWin_DrawQuadF4), 4, {kAll, kU16, kAll, kAll}, Answer::kGarbage, 0, 0},
    {BH_OURS(BattleWin_DrawLineAdd), 7, {kU16, kU16, kU16, kU16, kAll, kU8, kU8}, Answer::kGarbage, 0, 0},
    {BH_OURS(BattleWin_DrawLineHalf), 7, {kU16, kU16, kU16, kU16, kAll, kU8, kU8}, Answer::kGarbage, 0, 0},
    {BH_OURS(Gpu_GetClut), 2, {kAll, kAll}, Answer::kGarbage, 0, 0},
    {BH_OURS(Gpu_SetLineF2), 1, {kAll}, Answer::kGarbage, 0, 0},
    {BH_OURS(Gpu_SetLineF3), 1, {kAll}, Answer::kGarbage, 0, 0},
    {BH_OURS(Gpu_SetLineF4), 1, {kAll}, Answer::kGarbage, 0, 0},
    {BH_OURS(Gpu_SetPolyG4), 1, {kAll}, Answer::kGarbage, 0, 0},
    {BH_OURS(Gpu_SetTile), 1, {kAll}, Answer::kGarbage, 0, 0},
    {BH_OURS(Gpu_SetShadeTex), 2, {kAll, kAll}, Answer::kGarbage, 0, 0},
    {BH_OURS(Prim_SetTexture), 3, {kAll, kAll, kAll}, Answer::kGarbage, 0, 0},
    // input
    {BH_OURS(Input_AutoRepeat), 1, {kU16}, Answer::kGarbage, 0, 0},
    // the battle engine
    {BH_OURS(BattleObj_ScriptTick), 0, {}, Answer::kFlag, 0, 0},
    {BH_OURS(BattleObj_ScriptTickOnce), 0, {}, Answer::kFlag, 0, 0},
    {BH_OURS(BattleObj_PickPose), 0, {}, Answer::kGarbage, 0, 0},
    {BH_OURS(BattleObj_EndAction), 0, {}, Answer::kGarbage, 0, 0},
    {BH_OURS(BattleQueue_Push), 3, {kU8, kU8, kAll}, Answer::kGarbage, 0, 0},
    {BH_OURS(BattleTask_ClearAll), 0, {}, Answer::kGarbage, 0, 0},
    {BH_OURS(Battle_ApplyDamage), 2, {kAll, kAll}, Answer::kGarbage, 0, 0},
    {BH_OURS(Battle_CalcDamage), 3, {kAll, kAll, kU16}, Answer::kGarbage, 0, 0},
    {BH_OURS(Battle_BuildTurnOrder), 0, {}, Answer::kGarbage, 0, 0},
    {BH_OURS(Battle_ClearActingFlags), 0, {}, Answer::kGarbage, 0, 0},
    {BH_OURS(Battle_ClearStatus), 2, {kAll, kAll}, Answer::kGarbage, 0, 0},
    {BH_OURS(Battle_DefaultTarget), 1, {kAll}, Answer::kByte, 0xFF, 10},   // an actor 0..10, or 0xFF for none
    {BH_OURS(Battle_PlayActorCue), 1, {kU8}, Answer::kGarbage, 0, 0},
    {BH_OURS(Battle_PlayHitSound), 0, {}, Answer::kGarbage, 0, 0},
    {BH_OURS(Battle_ReturnQueuedItem), 1, {kAll}, Answer::kGarbage, 0, 0},
    {BH_OURS(Battle_ReturnTrue), 0, {}, Answer::kFlag, 0, 0},
    {BH_OURS(Battle_RollPendingFlag), 0, {}, Answer::kFlag, 0, 0},
    {BH_OURS(Battle_SetActorBit), 1, {kU8}, Answer::kGarbage, 0, 0},
    {BH_OURS(Battle_SetDamagePopup), 2, {kU16, kAll}, Answer::kGarbage, 0, 0},
    {BH_OURS(Battle_SetHitPopup), 0, {}, Answer::kGarbage, 0, 0},
    {BH_OURS(Battle_StatusTint), 1, {kU8}, Answer::kGarbage, 0, 0},
    {BH_OURS(Battle_WrapIndex), 3, {kAll, kAll, kAll}, Answer::kThrough, 0, 0},
    {BH_OURS(Effect_ApplyResult), 0, {}, Answer::kGarbage, 0, 0},
    {BH_OURS(EnemyAI_RowDone), 2, {kAll, kU8}, Answer::kFlag, 0, 0},
    {BH_OURS(Formation_ApplyStatMods), 0, {}, Answer::kGarbage, 0, 0},
    {BH_OURS(Char_RecalcStats), 1, {kAll}, Answer::kGarbage, 0, 0},
    {BH_OURS(Area_TestCondition), 1, {kAll}, Answer::kFlag, 0, 0},
    {BH_OURS(Field_SlotRelease), 1, {kU8}, Answer::kGarbage, 0, 0},
    {BH_OURS(Gfx_ClutStripCopyRow), 1, {kAll}, Answer::kGarbage, 0, 0},
    {BH_OURS(MapView_SetElevation), 1, {kAll}, Answer::kGarbage, 0, 0},
    {"0x446F20", 0x446F20, 0x446F20, 2, {kAll, kAll}, Answer::kThrough, 0, 0},   // (a * b) / 100, 0..999
    {"0x446F50", 0x446F50, 0x446F50, 2, {kAll, kAll}, Answer::kThrough, 0, 0},   // ..., 0..9999
    {"0x446F80", 0x446F80, 0x446F80, 2, {kAll, kAll}, Answer::kThrough, 0, 0},   // ..., 0..100
    {"0x494E70", 0x494E70, 0x494E70, 0, {}, Answer::kThrough, 0, 0},             // the eight enemies' +0..+3 zeroed (compared)
    {"0x42E0E0", 0x42E0E0, 0x42E0E0, 0, {}, Answer::kGarbage, 0, 0},             // BATE's, unlisted in the cut (part 7)
    {"0x42E250", 0x42E250, 0x42E250, 0, {}, Answer::kGarbage, 0, 0},
    {"0x42E2F0", 0x42E2F0, 0x42E2F0, 0, {}, Answer::kFlag, 0, 0},
    {"0x437230", 0x437230, 0x437230, 0, {}, Answer::kGarbage, 0, 0},             // no start list has it (after 0x437200's padding)
    {"0x441510", 0x441510, 0x441510, 0, {}, Answer::kGarbage, 0, 0},             // no start list has it (after BattleObj_PickPose's table)
    {"0x44F1D0", 0x44F1D0, 0x44F1D0, 2, {kAll, kAll}, Answer::kGarbage, 0, 0},
    {"0x44F6A0", 0x44F6A0, 0x44F6A0, 2, {0, kAll}, Answer::kFlag, 0, 0},         // reads its second word only
    {"0x44FB30", 0x44FB30, 0x44FB30, 0, {}, Answer::kGarbage, 0, 0},
    {"0x452DD0", 0x452DD0, 0x452DD0, 1, {kAll}, Answer::kFlag, 0, 0},
    {"0x452EB0", 0x452EB0, 0x452EB0, 0, {}, Answer::kFlag, 0, 0},
    {"0x452F10", 0x452F10, 0x452F10, 0, {}, Answer::kFlag, 0, 0},
    {"0x4CF4B0", 0x4CF4B0, 0x4CF4B0, 2, {kAll, kAll}, Answer::kGarbage, 0, 0},
    {"0x590E80", 0x590E80, 0x590E80, 3, {kAll, kAll, kAll}, Answer::kGarbage, 0, 0},   // (u16 *stat, cap, delta): a stat add
    {"0x591810", 0x591810, 0x591810, 2, {kU8, kU8}, Answer::kFlag, 0, 0},         // no start list has it (after 0x5917D0's)
    {"0x59DB70", 0x59DB70, 0x59DB70, 6, {kAll, kAll, kU8, kU8, kU16, kU8}, Answer::kGarbage, 0, 0},
    {"0x5B9450", 0x5B9450, 0x5B9450, 3, {kAll, kAll, kAll}, Answer::kThrough, 0, 0},   // the CRT's memcpy
    // items, stats, the party
    {BH_OURS(Inventory_Add), 3, {kAll, kU8, kAll}, Answer::kFlag, 0, 0},
    {BH_OURS(Inventory_Remove), 3, {kAll, kU8, kU8}, Answer::kFlag, 0, 0},
    {BH_OURS(Item_CanUse), 4, {kU8, kAll, kAll, kAll}, Answer::kFlag, 0, 0},
    {BH_OURS(Item_EquipMask), 2, {kU8, kU8}, Answer::kGarbage, 0, 0},
    {BH_OURS(Item_IconKind), 2, {kU8, kAll}, Answer::kGarbage, 0, 0},
    {BH_OURS(Equip_PreviewSet), 4, {kAll, kAll, 0, 0}, Answer::kGarbage, 0, 0},   // marks, values: the caller's buffers (the group's)
    {BH_OURS(Stat_AddClamped), 2, {kAll, kAll}, Answer::kGarbage, 0, 0},
    {BH_OURS(Stat_AddCap999), 2, {kAll, kAll}, Answer::kFlag, 0, 0},
    {BH_OURS(PartySet_Select), 2, {kAll, kAll}, Answer::kGarbage, 0, 0},
    // sprites, sound, tasks
    {BH_OURS(Sprite_ReleaseTint), 1, {kAll}, Answer::kGarbage, 0, 0},
    {BH_OURS(Sprite_SetTint), 5, {kAll, kU8, kU8, kU8, kU8}, Answer::kFlag, 0, 0},
    {BH_OURS(Sprite_AnimFromSet), 4, {kAll, kAll, kAll, kAll}, Answer::kGarbage, 0, 0},
    {BH_OURS(Sprite_LoadPalette), 2, {kAll, kU8}, Answer::kGarbage, 0, 0},
    {BH_OURS(Sprite_SetClutStp), 0, {}, Answer::kGarbage, 0, 0},
    {BH_OURS(Sound_StopChannels), 0, {}, Answer::kGarbage, 0, 0},
    {BH_OURS(Task_Restart), 1, {kAll}, Answer::kGarbage, 0, 0},
    // the GTE BMAGIC reads back through pointers: both sides run the real one
    {BH_OURS(Gte_LoadVertex), 1, {kAll}, Answer::kThrough, 0, 0},
    {BH_OURS(Gte_LoadVertices3), 1, {kAll}, Answer::kThrough, 0, 0},
    {BH_OURS(Gte_Rtps), 0, {}, Answer::kThrough, 0, 0},
    {BH_OURS(Gte_Rtpt), 0, {}, Answer::kThrough, 0, 0},
    {BH_OURS(Gte_StoreScreenXY), 1, {kAll}, Answer::kThrough, 0, 0},
    {BH_OURS(Gte_StoreScreenXY3), 3, {kAll, kAll, kAll}, Answer::kThrough, 0, 0},
    {BH_OURS(Gte_StoreDepthF4), 4, {kAll, kAll, kAll, kAll}, Answer::kThrough, 0, 0},
    {BH_OURS(Gte_PrimDepths4_10), 1, {kAll}, Answer::kThrough, 0, 0},
    {BH_OURS(Gte_PrimDepthFlat4_10), 1, {kAll}, Answer::kThrough, 0, 0},
    {BH_OURS(Gte_RotMatrix), 2, {kAll, kAll}, Answer::kThrough, 0, 0},
    {BH_OURS(Gte_MulMatrix0), 3, {kAll, kAll, kAll}, Answer::kThrough, 0, 0},
    {BH_OURS(Gte_RotTransPers), 3, {kAll, kAll, kAll}, Answer::kThrough, 0, 0},
    {BH_OURS(Gte_RotTransPers4), 9, {kAll, kAll, kAll, kAll, kAll, kAll, kAll, kAll, kAll}, Answer::kThrough, 0, 0},
};
#undef BH_OURS
#undef BH_THEIRS

constexpr std::uint32_t kImageLo = 0x401000, kImageHi = 0x5C3000;   // .text

void Register(const Callee& c) {
    if (c.key == c.address) {
        if (c.key < kImageLo || c.key >= kImageHi)
            bof3::Fatal("boss_harness: callee %s at 0x%X is not Capcom's code", c.name, (unsigned)c.key);
    } else if (c.key >= kImageLo && c.key < kImageHi) {
        bof3::Fatal("boss_harness: callee %s is Capcom's now (0x%X): list it as such", c.name, (unsigned)c.key);
    }
    if (c.nargs > kArgs) bof3::Fatal("boss_harness: callee %s takes %u arguments, the stand-ins %u", c.name, c.nargs, kArgs);
    for (unsigned i = 0; i < g_slot_n; ++i)
        if (g_slots[i].address == c.address) return;
    if (g_slot_n == kSlots) bof3::Fatal("boss_harness: more than %u stand-ins", kSlots);
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
unsigned RegisterHandler(std::uint32_t address, unsigned nargs, const char* name = "handler") {
    for (unsigned i = 0; i < g_slot_n; ++i)
        if (g_slots[i].address == address) {
            // the first listing stands (boss_harness.h, DataTable); said once
            if (g_slots[i].handler && g_slots[i].nargs != nargs)
                bof3::Log("shadow      %s: handler 0x%X in tables with nargs %u and %u: the first stands", g_group->shadow,
                          (unsigned)address, g_slots[i].nargs, nargs);
            return i;
        }
    if (g_slot_n == kSlots) bof3::Fatal("boss_harness: more than %u stand-ins", kSlots);
    Slot& s = g_slots[g_slot_n++];
    s = {};
    s.name = name;
    s.address = s.key = address;
    s.handler = true;
    s.nargs = nargs;
    for (unsigned i = 0; i < kArgs; ++i) s.masks[i] = kAll;
    return g_slot_n - 1;
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
    bof3::Fatal("boss_harness: %s calls 0x%X, which no stand-in covers: list it in the group's callees", who,
                (unsigned)address);
}
const void* StubFor(std::uint32_t address, const char* who) { return StubOf(SlotFor(address, who)); }

// The hook recorders: four handler slots under keys no code holds (they are
// only ever reached through the cells), registered by every Run. The event
// and enemy hooks log their argument.
constexpr std::uint32_t kEndKey = 0xB0550064u, kExitKey = 0xB0550068u, kEventKey = 0xB055006Cu, kEnemyKey = 0xB05500F4u;
std::uint32_t g_hook_end, g_hook_exit, g_hook_event, g_hook_enemy;   // the stubs' addresses

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

// The engine frame's pointers put back (Group::engine only, after the boss
// frame's, so a boss group draws exactly what it did): 0x905B84 at one of the
// 22 window records; Field_State at a party member - for kMember the member
// Sprite_Current is three times in four; the menu actor at a member and its
// command record at that member's +0x124; the acting sprites 0x904B3C /
// 0x904B40 at the harness's records; the result record 0x904B60 at a member's
// or an enemy's +0x104; a NUL every 16th byte of the text buffer.
void FixEngine() {
    SetPointer(at::kWindowCurrent, WindowAt(Next()));
    unsigned char* const member = MemberShape() && Next() % 4 != 0 ? Sprite_Current : PartyOf(static_cast<unsigned char>(Next() % 3));
    SetPointer(at::kMemberCurrent, member);
    unsigned char* const actor = Often() ? member : PartyOf(static_cast<unsigned char>(Next() % 3));
    SetPointer(at::kMenuActor, actor);
    SetPointer(at::kMenuCommand, actor + at::kMenuCommandAt);
    SetPointer(at::kActingSprite, g_records[Next() & 1]);
    SetPointer(at::kActingSprite2, g_records[Next() & 1]);
    SetPointer(at::kResultRecord, (Half() ? PartyOf(static_cast<unsigned char>(Next() % 3)) : EnemyAt(Next())) + at::kResultAt);
    for (unsigned i = 15; i < kTextBytes; i += 16) g_text[i] = 0;
}

// Random bytes put back inside what every boss function dereferences: the
// running sprite (an enemy, or a task slot for an effect task) and the
// current enemy (the same two times in three), the slot and its owner, a
// target of 0..10 and an actor of 0..2, the four slots' state bytes and
// owners, every enemy's hook and tables (the recorder, the harness's two
// records), the three hooks (their recorders), the chapter's flag bits
// inside Cond_Flags, the packet pointer at the harness's buffer.
void Fix() {
    Sprite_Current = SpriteFor(Next());
    if (EngineShape()) SetPointer(at::kEnemyCurrent, EnemyAt(Next()));   // never a member (round twelve)
    else SetPointer(at::kEnemyCurrent, TaskShape() || !Often() ? EnemyAt(Next()) : Sprite_Current);
    SetPointer(at::kCurrentSlot, TaskShape() && Often() ? Sprite_Current : TaskAt(Next()));
    SetPointer(at::kOwner, OwnerFor(Next()));
    SetPointer(at::kSource, g_records[Next() & 1]);
    Mem(at::kTarget)[0] = static_cast<unsigned char>(Next() % 11);
    Mem(at::kActor)[0] = static_cast<unsigned char>(Next() % 3);
    for (unsigned k = 0; k < 4; ++k) {
        unsigned char* const t = TaskAt(k);
        t[1] = static_cast<unsigned char>(Next() % 3);
        t[2] = static_cast<unsigned char>(Next() % 3);
        SetPointer(at::kTasks + k * at::kTaskStride + 0x80, OwnerFor(Next()));
    }
    for (unsigned k = 0; k < at::kEnemyCount; ++k) {
        unsigned char* const e = EnemyAt(k);
        move_script::SetLong(e + 0xF4, static_cast<std::int32_t>(g_hook_enemy));
        move_script::SetLong(e + 0xF8, static_cast<std::int32_t>(Key(g_records[0])));
        move_script::SetLong(e + 0xFC, static_cast<std::int32_t>(Key(g_records[1])));
    }
    move_script::SetLong(Mem(at::kHookEnd), static_cast<std::int32_t>(g_hook_end));
    move_script::SetLong(Mem(at::kHookExit), static_cast<std::int32_t>(g_hook_exit));
    move_script::SetLong(Mem(at::kHookEvent), static_cast<std::int32_t>(g_hook_event));
    SetPointer(at::kFlagBits, Mem(at::kCondFlags + 8 * (Next() % 40)));
    SetPointer(at::kPacketNext, g_packets + 4 * (Next() % 64));
    if (g_group && g_group->engine) FixEngine();
}

// The byte a clone's state draw writes: its state_cell when set (round
// twelve), the window record's byte for a kWindow, else Sprite_Current's -
// the only form before round twelve, so a boss clone draws where it did.
unsigned char* StateByte(const Clone& c) {
    if (c.state_cell) return Mem(c.state_cell);
    if (c.shape == Shape::kWindow) return CurrentWindow() + c.state_at;
    return Sprite_Current + c.state_at;
}
unsigned char* ViaByte(const Clone& c) {
    if (c.via.state_cell) return Mem(c.via.state_cell);
    if (c.shape == Shape::kWindow) return CurrentWindow() + c.via.state_at;
    return Sprite_Current + c.via.state_at;
}

void PatchImms(void* copy, const Clone& c) {
    auto* code = static_cast<std::uint8_t*>(copy);
    for (int i = 0; i < c.n_imms; ++i) {
        std::uint32_t had;
        std::memcpy(&had, code + c.imms[i].offset, sizeof had);
        if (had != c.imms[i].value)
            bof3::Fatal("boss_harness: %s +0x%X holds 0x%X, not the handler 0x%X", c.name, (unsigned)c.imms[i].offset,
                        (unsigned)had, (unsigned)c.imms[i].value);
        const std::uint32_t to = Key(StubFor(had, c.name));
        std::memcpy(code + c.imms[i].offset, &to, sizeof to);
    }
    for (int i = 0; i < c.n_tables; ++i)
        move_script::Relocate(copy, c.base, c.size, {c.tables[i].jmp_disp, c.tables[i].table, c.tables[i].entries});
}

using FnArgs = std::uint32_t (__cdecl*)(std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t,
                                        std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t);

std::uint32_t CallTen(const void* fn, const std::uint32_t* a) {
    return reinterpret_cast<FnArgs>(const_cast<void*>(fn))(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8], a[9]);
}

}  // namespace

// --- the public helpers -------------------------------------------------------

std::uint32_t Next() { g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5; return g_rng; }
bool Often() { return Next() % 3 != 0; }
bool Half() { return (Next() & 1) != 0; }
std::uint32_t Pick(const std::uint32_t* v, unsigned n) { return v[Next() % n]; }
unsigned char* Mem(std::uint32_t address) { return move_script::At(address); }
unsigned char* EnemyAt(unsigned k) { return Mem(at::kEnemies + (k % at::kEnemyCount) * at::kEnemyStride); }
unsigned char* EnemyOf(unsigned char target) {
    return Mem(at::kEnemies + static_cast<std::uint32_t>((static_cast<int>(target) - 3) * static_cast<int>(at::kEnemyStride)));
}
unsigned char* PartyOf(unsigned char member) { return Mem(at::kParty + (member % at::kPartyCount) * at::kPartyStride); }
unsigned char* TaskAt(unsigned k) { return Mem(at::kTasks + (k % 4) * at::kTaskStride); }
unsigned char* Object(unsigned k) { return Mem(at::kObjects + (k % at::kObjectCount) * at::kObjectStride); }
unsigned char* SpriteRecord(unsigned k) { return g_records[k & 1]; }
unsigned char* Packets() { return g_packets; }
bool InEngineBands(std::uint32_t address) {
    for (const at::Band& b : at::kEngineBands)
        if (address >= b.lo && address < b.hi) return true;
    return false;
}
unsigned char* WindowAt(unsigned k) { return Mem(at::kWindows + (k % at::kWindowCount) * at::kWindowStride); }
unsigned char* CurrentWindow() { return Pointer(at::kWindowCurrent); }
unsigned char* TextBuffer() { return g_text; }
std::uint32_t HookStub(std::uint32_t cell) {
    switch (cell) {
    case at::kHookEnd: return g_hook_end;
    case at::kHookExit: return g_hook_exit;
    case at::kHookEvent: return g_hook_event;
    default: return g_hook_enemy;
    }
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
    bof3::Fatal("boss_harness: a custom stand-in records 0x%X, which no callee lists", (unsigned)address);
}
void Stir() { Disturb(); }
void OtherStates(unsigned drawn, unsigned below) {
    unsigned char* const s = Sprite_Current;
    for (unsigned b = 1; b <= 4; ++b)
        if (b != drawn) s[b] = static_cast<unsigned char>(Next() % below);
}
void OtherStates(unsigned at, unsigned n1, unsigned n2, unsigned n3) {
    unsigned char* const s = Sprite_Current;
    const unsigned n[4] = {0, n1, n2, n3};
    for (unsigned b = 1; b <= 3; ++b)
        if (b != at && n[b]) s[b] = static_cast<unsigned char>(Next() % n[b]);
}
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
    // a Via's planted function, reached through a dispatcher that is ours by
    // now (a later group's): it is the function under test, not a callee
    if (key == g_via_fn && key != 0) return reinterpret_cast<const void*>(static_cast<std::uintptr_t>(key));
    for (unsigned i = 0; i < g_slot_n; ++i)
        if (g_slots[i].key == key) return ForOurs(i, key);
    std::uint32_t address = key;
    for (const Callee& c : kStandard)
        if (c.key == key) address = c.address;
    for (const Callee& c : kEngineStandard)
        if (c.key == key) address = c.address;
    for (unsigned i = 0; i < g_slot_n; ++i)
        if (g_slots[i].address == address) return ForOurs(i, key);
    bof3::Fatal("boss_harness: ours calls 0x%X, which no stand-in covers: list it in the group's callees", (unsigned)key);
}

void Run(const Group& group) {
    const unsigned per = group.rounds ? group.rounds : 2000;
    g_group = &group;
    g_slot_n = 0;
    for (unsigned i = 0; i < group.n_callees; ++i) Register(group.callees[i]);
    // an engine group's louder standard forms first, so they stand over kStandard's
    if (group.engine)
        for (const Callee& c : kEngineStandard) Register(c);
    for (const Callee& c : kStandard) Register(c);
    g_hook_end = Key(StubOf(RegisterHandler(kEndKey, 0, "hook 0x904B64")));
    g_hook_exit = Key(StubOf(RegisterHandler(kExitKey, 0, "hook 0x904B68")));
    g_hook_event = Key(StubOf(RegisterHandler(kEventKey, 1, "hook 0x904B6C")));
    g_hook_enemy = Key(StubOf(RegisterHandler(kEnemyKey, 1, "hook +0xF4")));
    for (unsigned k = 0; k < group.n_clones; ++k)
        for (int i = 0; i < group.clones[k].n_imms; ++i) RegisterHandler(group.clones[k].imms[i].value, 0);
    for (unsigned t = 0; t < group.n_data_tables; ++t) {
        const DataTable& d = group.data_tables[t];
        for (unsigned i = 0; i < d.entries; ++i)
            RegisterHandler(static_cast<std::uint32_t>(move_script::Long(Mem(d.at + d.stride * i))), d.nargs);
    }

    // the regions: one battle frame (docs/boss_harness.md section 4), then the group's
    g_region_n = 0;
    const Region standard[] = {
        {at::kTasks, at::kTaskCount * at::kTaskStride},             // the 48 task slots
        {0x93B8C0, 0xA0},                                           // the current slot, the owner
        {at::kEnemies, at::kEnemyCount * at::kEnemyStride + 0x80},  // the enemies' objects, their working records' tail
        {0x937F80, 0x18},                                           // Sprite_Current, Gfx_ClutStripDirty, Frame_Counter
        {0x939AD0, 0x50},                                           // the current enemy 0x939AD8 .. 0x939B1C
        {at::kBattle, at::kBattleSize},                             // the battle bytes and the three hooks
        {at::kMessageUp, 4},
        {at::kParty, at::kPartyCount * at::kPartyStride},           // ObjTrio
        {0x8034E0, 0x10},                                           // Cond_ByteFA .., the chapter's run and step
        {at::kObjects, at::kObjectCount * at::kObjectStride},       // Sprite_Objects: the field's actors
        {at::kFlagBits, 4},                                         // the chapter's flag bits (a pointer)
        {at::kCondFlags, 0x1D0},                                    // Cond_Flags, which it points into
        {at::kEnemyRows, 0x48 + 8 * at::kEnemyDataStride},          // the area's enemy rows and records
        {at::kPacketNext, 4},                                       // Gfx_PacketNext
        {Key(g_records), sizeof g_records},
        {Key(g_packets), sizeof g_packets},
    };
    for (const Region& r : standard) g_regions[g_region_n++] = r;
    // the engine frame's (round twelve; docs/boss_harness.md section 10.3):
    // the state three or more of BE1..BE7 touch beyond the boss frame
    const Region engine[] = {
        {at::kWindows, at::kWindowCount * at::kWindowStride},       // WindowRecords: BE1..BE6
        {at::kWindowCurrent, 4},                                    // the record a window handler runs for: BE7 (every kWindow)
        {at::kMemberCurrent, 4},                                    // Field_State: BE2, BE3, BE4
        {0x903A50, 0xD4},                                           // DrawItemPool_Top's tail, CharacterRecords' head, Field_ActorStates[0]: BE1..BE4, BE6, BE7
        {0x9045FC, 0x58},                                           // the 18 dwords at 0x904608 and their neighbours: BE2, BE5, BE6, BE7
        {0x939EC0, 0xA0},                                           // the menu actor 0x939EC4, the transformation's cells 0x939EE0..: BE3..BE7
        {0x939F64, 0x9C},                                           // 0x939F86 .. the command record 0x939FA0 .. 0x939FFC: BE3, BE4, BE5
        {at::kModeBytes, 0x20},                                     // BATE's mode bytes, 0x929F04 / 06, the kind-2 cells: BE1, BE4, BE7
        {0x7E1BE8, 8},                                              // Input_Held, Input_Pressed: BE1, BE4, BE5
        {0x90358C, 8},                                              // Field_ConfirmButtons, Field_CancelButtons: BE1, BE4, BE5
        {Key(g_text), sizeof g_text},                               // the harness's strings (TextPtrEffect)
    };
    if (group.engine)
        for (const Region& r : engine) g_regions[g_region_n++] = r;
    for (unsigned i = 0; i < group.n_regions; ++i) {
        if (g_region_n == kMaxRegions) bof3::Fatal("boss_harness: %s: more than %u regions", group.shadow, kMaxRegions);
        g_regions[g_region_n++] = group.regions[i];
    }
    g_bytes = 0;
    for (unsigned i = 0; i < g_region_n; ++i) g_bytes += g_regions[i].size;
    if (g_bytes > kMaxBytes) bof3::Fatal("boss_harness: %s: the regions are %u bytes, the state holds %u", group.shadow, g_bytes, kMaxBytes);

    static void* clones[256];
    if (group.n_clones > 256) bof3::Fatal("boss_harness: %s: more than 256 clones", group.shadow);
    for (unsigned k = 0; k < group.n_clones; ++k) {
        const Clone& c = group.clones[k];
        bof3::CloneCall calls[64];
        if (c.n_calls > 64)
            bof3::Fatal("boss_harness: %s has %d calls: copy it in the group's file and hand the harness a jmp (docs/boss_harness.md section 6)",
                        c.name, c.n_calls);
        for (int i = 0; i < c.n_calls; ++i) {
            const unsigned slot = SlotFor(c.calls[i].target, c.name);
            calls[i] = {c.calls[i].offset, g_slots[slot].answer == Answer::kThrough ? nullptr : StubOf(slot), c.calls[i].target};
        }
        // round twelve: the engine's shapes need the engine frame, and an
        // engine group's functions lie in the battle runs (a function outside
        // them is a cut mistake; the harness's own self-test hands it a jmp
        // wrapper of its copies, which lies in our DLL and is checked there)
        if (c.shape >= Shape::kStep && !group.engine)
            bof3::Fatal("boss_harness: %s: %s's shape is the battle engine's: set Group::engine", group.shadow, c.name);
        if (group.engine && !InEngineBands(c.base) && (c.base >= kImageLo && c.base < kImageHi))
            bof3::Fatal("boss_harness: %s: %s at 0x%X lies outside the battle engine's runs", group.shadow, c.name, (unsigned)c.base);
        clones[k] = bof3::CloneOriginal(c.name, c.base, c.size, calls, c.n_calls);
        PatchImms(clones[k], c);
        if (c.via.dispatcher && (c.via.cell == 0 || (c.via.state_at == 0 && c.via.state_cell == 0 && c.shape != Shape::kEnemyHook &&
                                                     c.shape != Shape::kEvent && c.shape != Shape::kCallee && c.shape != Shape::kHelper)))
            bof3::Fatal("boss_harness: %s: a via needs a cell, and a state byte unless its dispatcher reads the argument", c.name);
    }

    // the .data tables' entries swapped for their recorders, put back after
    constexpr unsigned kTableMax = 64, kEntryMax = 64;
    static std::uint32_t kept[kTableMax][kEntryMax];
    if (group.n_data_tables > kTableMax) bof3::Fatal("boss_harness: %s: more than %u .data tables", group.shadow, kTableMax);
    for (unsigned t = 0; t < group.n_data_tables; ++t) {
        const DataTable& d = group.data_tables[t];
        if (d.entries > kEntryMax) bof3::Fatal("boss_harness: %s: a .data table of more than %u", group.shadow, kEntryMax);
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
        g_clone = &c;
        for (unsigned i = 0; i < g_bytes; i += 4) {
            const std::uint32_t v = Next();
            std::memcpy(input.memory + i, &v, g_bytes - i < 4 ? g_bytes - i : 4);
        }
        input.log_n = 0;
        g_rand_first = -1;
        Apply(input);
        Fix();
        Mem(at::kFight)[0] = static_cast<unsigned char>(group.fight >= 0 ? group.fight : 1 + Next() % 55);
        if (group.kind >= 0 && InRegions(CurrentEnemy() + 0x100, 1)) CurrentEnemy()[0x100] = static_cast<unsigned char>(group.kind);
        if (c.shape == Shape::kDispatch && c.states && InRegions(StateByte(c), 1))
            *StateByte(c) = static_cast<unsigned char>(Next() % c.states);
        else if (c.shape >= Shape::kStep && c.shape != Shape::kHelper && c.states && InRegions(StateByte(c), 1))
            *StateByte(c) = static_cast<unsigned char>(Next() % c.states);   // round twelve: kStep, kWindow, kMember
        g_seed = Next();
        g_rand_hint = Next();
        if (group.seed) group.seed(k);
        if (c.shape == Shape::kTask && c.states && InRegions(StateByte(c), 1))
            *StateByte(c) = static_cast<unsigned char>(Next() % c.states);
        if (c.via.dispatcher && (c.via.state_at || c.via.state_cell) && InRegions(ViaByte(c), 1))
            *ViaByte(c) = c.via.state;
        Capture(input);

        // the shape's words, or the group's arguments
        std::uint32_t a[kArgs] = {Next(), Next(), Next()};
        for (unsigned i = 3; i < kArgs; ++i) a[i] = Next();
        if (c.shape == Shape::kEvent) a[0] = Next() % 7;
        else if (c.shape == Shape::kEnemyHook) a[0] = Next() % 3;
        if (group.args) group.args(k, a);
        if (c.via.dispatcher && c.via.state_at == 0 && c.via.state_cell == 0) a[0] = c.via.state;
        const std::uint32_t ret_mask = c.ret_mask ? c.ret_mask : c.shape == Shape::kEvent ? 0xFFu : 0u;
        g_calm = c.calm;
        for (int pass = 0; pass < 2; ++pass) {
            Apply(input);
            State& out = pass ? ours : theirs;
            const void* const fn = pass ? c.ours : clones[k];
            std::uint32_t answer;
            if (c.via.dispatcher) {
                const std::int32_t was = move_script::Long(Mem(c.via.cell));
                move_script::SetLong(Mem(c.via.cell), static_cast<std::int32_t>(Key(fn)));
                g_via_fn = Key(fn);
                g_active = pass == 1;
                answer = CallTen(reinterpret_cast<const void*>(static_cast<std::uintptr_t>(c.via.dispatcher)), a);
                g_active = false;
                g_via_fn = 0;
                move_script::SetLong(Mem(c.via.cell), was);
            } else {
                g_active = pass == 1;
                answer = CallTen(fn, a);
                g_active = false;
            }
            if (ret_mask) Log5(kReturnTag, answer & ret_mask, 0, 0, 0);
            // the hooks read back after every call (boss_harness.h, Shape)
            Log5(kShapeTag + static_cast<unsigned>(c.shape), static_cast<std::uint32_t>(move_script::Long(Mem(at::kHookEnd))),
                 static_cast<std::uint32_t>(move_script::Long(Mem(at::kHookExit))),
                 static_cast<std::uint32_t>(move_script::Long(Mem(at::kHookEvent))), Cur());
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
            bof3::Fatal("boss_harness: %s made %u calls, the log holds %u", c.name, theirs.log_n, kLog);
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
                // round twelve: the differing entry itself, the first three times -
                // which recorder (slot, or 1000 + slot a handler, 2000 + slot the
                // words past the fourth, 3000 a Note, 4000 the answer, 5000 +
                // shape the hooks) and its words on each side
                const unsigned e = FirstLogDifference(theirs, ours);
                if (bad <= 3 && e < Used(theirs.log_n) && e < Used(ours.log_n)) {
                    const Entry& t = theirs.log[e];
                    const Entry& o = ours.log[e];
                    const unsigned s = t.what >= kPhaseTag ? t.what % 1000 : t.what;
                    bof3::Log("shadow      %s   entry %u (%s): theirs %u 0x%X 0x%X 0x%X 0x%X, ours %u 0x%X 0x%X 0x%X 0x%X", group.shadow,
                              e, s < g_slot_n ? g_slots[s].name : "-", (unsigned)t.what, (unsigned)t.a, (unsigned)t.b,
                              (unsigned)t.c, (unsigned)t.d, (unsigned)o.what, (unsigned)o.a, (unsigned)o.b, (unsigned)o.c,
                              (unsigned)o.d);
                }
            }
        }
    }
    g_clone = nullptr;
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
        const bool named = g_slots[i].handler && std::strcmp(g_slots[i].name, "handler") != 0;
        const int w = named ? std::snprintf(line + n, sizeof line - n, "%s%s %u", n ? ", " : "", g_slots[i].name, g_slots[i].calls)
                      : g_slots[i].handler
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

}  // namespace boss_harness

// BOF3X_SHADOW=sprite_pose: the start-up differential fuzz of the ten
// functions in sprite_pose.cpp against byte-copies of the originals
// (docs/sprite-pose.md §5).
//
// Two phases.
//   1. Alone: each of the ten copied on its own, every call out of the copy
//      re-aimed at a recording stand-in, and ours routed to the same
//      stand-ins through sprite_pose::g.
//   2. Chained: the copies call each OTHER - Sprite_FaceDirection's copy
//      reaches the copies of Sprite_EnsureAnimation, Sprite_SetAnimation,
//      Sprite_SetAnimationAt, Sprite_SetAnimationBank and
//      Sprite_SetFrameQueueUpload, Effect_ClearAll's the copy of
//      Effect_ReleaseAt - while ours call ours; only Field_MemberSprite and
//      Sprite_ScriptStart, which belong to other files, stay stand-ins.
// Each round: random state over four sprite objects of 0xA4 bytes, the effect
// pool, the upload queue and the globals the ten read, each branch's
// boundaries seeded; theirs, then ours from the same state; the objects, the
// pool, the queue's arrays and count, Sprite_Current, the result (as much of
// eax as the function defines) and the stand-ins' log compared.
//
// The tables the ten index are swapped for buffers of ours for the fuzz's
// duration (the member-animation pointers, the party-set rows, the frame
// offsets, the bank records and their count, the active member) and put back
// after. The direction pairs and the bank bytes are only read, and are read
// where they are. The inputs are bounded - not the functions: the party row
// index stays inside the 0xC0 bytes swapped, a member inside the 11 pointers,
// an effect index below 0x40 - but the upload count takes every value up to
// 0xFF, so the unbounded append (known-defects D4) writes where it writes in
// the game, inside regions this snapshots and restores.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/sprite_pose.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace sprite_pose {
namespace {

using namespace move_script;

std::uint32_t Address(const volatile void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }

constexpr unsigned kSprites = 4, kSpriteBytes = 0xA4;
constexpr unsigned kMembers = 11, kMemberRecords = 128;
constexpr unsigned kRowBytes = 0xC0;
constexpr unsigned kColumns = 4, kColumnWords = 256;
constexpr unsigned kBankRecords = 256;
constexpr unsigned kImages = 0x10, kWords = 0x410, kEntries = 0x610, kAnimBytes = 0x910;
constexpr unsigned kEffectSpan = 0x40 * 0x80;
constexpr std::uint32_t kQueueFrom = at::kQueueX, kQueueSpan = at::kQueueY + 0x200 - at::kQueueX;
constexpr unsigned kRecordSpan = 0x400;

unsigned char g_sprites[kSprites * kSpriteBytes];
unsigned char g_member_records[kMembers][kMemberRecords * 4];
unsigned char g_offsets[kColumns * 4 + kColumns * kColumnWords * 2];
unsigned char g_banks[kBankRecords * 8];
unsigned char g_active[0xA4];
unsigned char g_anim[kSprites][kAnimBytes];

unsigned char* Sprite(unsigned k) { return g_sprites + (k % kSprites) * kSpriteBytes; }

// --- the recording stand-ins --------------------------------------------------

constexpr unsigned kLog = 64;
struct Entry { std::uint32_t what, current, a, b; };
Entry g_log[kLog];
unsigned g_log_n;
std::uint32_t g_seed;

std::uint32_t Hash() {
    std::uint32_t h = (g_seed + g_log_n) * 0x9E3779B1u;
    h ^= h >> 15;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    return h;
}
void Record(std::uint32_t what, std::uint32_t a = 0, std::uint32_t b = 0) {
    if (g_log_n < kLog) g_log[g_log_n] = {what, Address(Sprite_Current), a, b};
    ++g_log_n;
}

// What a callee may change that the caller reads after it: which sprite is
// current, and that sprite's +0x24 flags, +0x2C column, +0x4B animation,
// +0x64 member - each kept inside what the fuzz's tables can index.
void Disturb() {
    const std::uint32_t h = Hash();
    if (h % 3 == 0) Sprite_Current = Sprite(h >> 8);
    unsigned char* const s = Sprite_Current;
    if (h % 4 == 0) s[0x24] = static_cast<unsigned char>(h >> 11);
    if (h % 5 == 0) SetWord(s + 0x2C, (h >> 12) % kColumns);
    if (h % 6 == 0) s[0x4B] = static_cast<unsigned char>(h >> 16);
    if (h % 7 == 0) SetLong(s + 0x64, static_cast<std::int32_t>((h >> 9) % kMembers));
}

// Each records what its callee reads of its arguments - the low byte, or 16
// bits - since the copies push whole dwords whose upper bytes are whatever
// the register held.
void __cdecl StubSetAnimationAt(std::uint32_t animation, std::uint32_t start) {
    Record(1, animation & 0xFF, start & 0xFFFF);
    Disturb();
}
void __cdecl StubSetAnimation(std::uint32_t animation) {
    Record(2, animation & 0xFF);
    Disturb();
}
std::uint32_t __cdecl StubEnsure(std::uint32_t animation) {
    Record(3, animation & 0xFF);
    Disturb();
    return Hash() & 1;
}
std::uint32_t __cdecl StubSetBank(std::uint32_t bank) {
    Record(4, bank & 0xFFFF);
    Disturb();
    return Hash() & 1;
}
void __cdecl StubMemberSprite(std::uint32_t member, std::uint32_t slot) {
    Record(5, member & 0xFF, slot & 0xFF);
    Disturb();
}
std::uint32_t __cdecl StubQueue(std::uint32_t frame) {
    Record(6, frame & 0xFF);
    Disturb();
    return Hash();
}
void __cdecl StubScriptStart(std::uint32_t position) {
    Record(7, position & 0xFFFF);
    Disturb();
}
void __cdecl StubReleaseAt(std::uint32_t index) {
    Record(8, index & 0xFF);
    Disturb();
}

template <class T> T Cast(const void* p) { return reinterpret_cast<T>(const_cast<void*>(p)); }
using Raw = const void*;

const Callees kStubs = {
    Cast<void (__cdecl*)(unsigned char, unsigned short)>(reinterpret_cast<Raw>(&StubSetAnimationAt)),
    Cast<void (__cdecl*)(unsigned char)>(reinterpret_cast<Raw>(&StubSetAnimation)),
    Cast<unsigned char (__cdecl*)(unsigned char)>(reinterpret_cast<Raw>(&StubEnsure)),
    Cast<unsigned char (__cdecl*)(unsigned short)>(reinterpret_cast<Raw>(&StubSetBank)),
    Cast<void (__cdecl*)(unsigned, unsigned)>(reinterpret_cast<Raw>(&StubMemberSprite)),
    Cast<unsigned char* (__cdecl*)(unsigned)>(reinterpret_cast<Raw>(&StubQueue)),
    Cast<void (__cdecl*)(unsigned short)>(reinterpret_cast<Raw>(&StubScriptStart)),
    Cast<void (__cdecl*)(unsigned char)>(reinterpret_cast<Raw>(&StubReleaseAt)),
};

Raw StubFor(std::uint32_t target) {
    switch (target) {
    case 0x589200: return reinterpret_cast<Raw>(&StubSetAnimationAt);
    case 0x5891F0: return reinterpret_cast<Raw>(&StubSetAnimation);
    case 0x589330: return reinterpret_cast<Raw>(&StubEnsure);
    case 0x589590: return reinterpret_cast<Raw>(&StubSetBank);
    case 0x533BA0: return reinterpret_cast<Raw>(&StubMemberSprite);
    case 0x5894D0: return reinterpret_cast<Raw>(&StubQueue);
    case 0x589350: return reinterpret_cast<Raw>(&StubScriptStart);
    case 0x589870: return reinterpret_cast<Raw>(&StubReleaseAt);
    default: bof3::Fatal("sprite_pose: no stand-in for a call to 0x%X", (unsigned)target);
    }
}

// --- the copies ------------------------------------------------------------------
// Extents and calls out listed by capstone 2026-09-22; every jump in each stays
// inside it and none has a jump table or an indirect call. Offsets are of the
// E8 byte.

struct Call { std::uint32_t offset, target; };
struct Clone {
    const char* name;
    std::uint32_t base, size;
    Call calls[5];
    int n_calls;
    std::uint32_t result_mask;   // how much of eax the function defines
    int args;
};
enum : unsigned { kSetAnimation, kSetAnimationAt, kEnsure, kQueue, kBank, kFace, kFindFree, kRelease, kReleaseAt, kClearAll, kCount };
const Clone kClones[kCount] = {
    {"Sprite_SetAnimation", 0x5891F0, 0x10, {{0x07, 0x589200}}, 1, 0, 1},
    {"Sprite_SetAnimationAt", 0x589200, 0x12F,
     {{0x69, 0x589590}, {0xB4, 0x533BA0}, {0xE6, 0x5894D0}, {0xF3, 0x589350}, {0x124, 0x589350}}, 5, 0, 2},
    {"Sprite_EnsureAnimation", 0x589330, 0x1E, {{0x10, 0x5891F0}}, 1, 0xFF, 1},
    {"Sprite_SetFrameQueueUpload", 0x5894D0, 0xBD, {}, 0, 0xFFFFFFFFu, 1},
    {"Sprite_SetAnimationBank", 0x589590, 0xCE, {}, 0, 0xFF, 1},
    {"Sprite_FaceDirection", 0x57C4C0, 0x84, {{0x54, 0x5891F0}, {0x63, 0x589200}, {0x73, 0x589330}, {0x7D, 0x589330}}, 4, 0, 1},
    {"Effect_FindFree", 0x589810, 0x2C, {}, 0, 0xFF, 0},
    {"Effect_Release", 0x589840, 0x2F, {}, 0, 0, 0},
    {"Effect_ReleaseAt", 0x589870, 0x2D, {}, 0, 0, 1},
    {"Effect_ClearAll", 0x5898A0, 0x23, {{0x0D, 0x589870}}, 1, 0, 0},
};
const Raw kOurs[kCount] = {
    reinterpret_cast<Raw>(&Sprite_SetAnimation), reinterpret_cast<Raw>(&Sprite_SetAnimationAt),
    reinterpret_cast<Raw>(&Sprite_EnsureAnimation), reinterpret_cast<Raw>(&Sprite_SetFrameQueueUpload),
    reinterpret_cast<Raw>(&Sprite_SetAnimationBank), reinterpret_cast<Raw>(&Sprite_FaceDirection),
    reinterpret_cast<Raw>(&Effect_FindFree), reinterpret_cast<Raw>(&Effect_Release),
    reinterpret_cast<Raw>(&Effect_ReleaseAt), reinterpret_cast<Raw>(&Effect_ClearAll),
};

using Fn = std::uint32_t (__cdecl*)(std::uint32_t, std::uint32_t);

// --- the state ---------------------------------------------------------------------

struct Region { std::uint32_t at, size; };
const Region kRegions[] = {
    {Address(g_sprites), sizeof g_sprites},
    {Address(Effect_Objects), kEffectSpan},
    {kQueueFrom, kQueueSpan},
    {at::kQueueRecord, kRecordSpan},
    {Address(&Gfx_UploadQueueCount), 1},
};
constexpr unsigned kRegionBytes = sizeof g_sprites + kEffectSpan + kQueueSpan + kRecordSpan + 1;

struct State {
    unsigned char memory[kRegionBytes];
    unsigned char* current;
    std::uint32_t result;
    Entry log[kLog];
    unsigned log_n;
};
void Capture(State& s) {
    unsigned at = 0;
    for (const Region& r : kRegions) { std::memcpy(s.memory + at, At(r.at), r.size); at += r.size; }
    s.current = Sprite_Current;
    std::memcpy(s.log, g_log, sizeof s.log);
    s.log_n = g_log_n;
}
void Apply(const State& s) {
    unsigned at = 0;
    for (const Region& r : kRegions) { std::memcpy(At(r.at), s.memory + at, r.size); at += r.size; }
    Sprite_Current = s.current;
    std::memset(g_log, 0, sizeof g_log);
    g_log_n = 0;
}

// What the fuzz swaps in, and what it puts back.
struct Saved {
    unsigned char rows[kRowBytes];
    unsigned char members[kMembers * 4];
    unsigned char offsets[4], bank_table[4], bank_count[4], party_set;
    unsigned char* active;
};
void Swap(Saved& saved) {
    std::memcpy(saved.rows, At(at::kPartySetRows), kRowBytes);
    std::memcpy(saved.members, At(at::kMemberAnimations), sizeof saved.members);
    std::memcpy(saved.offsets, At(at::kFrameOffsets), 4);
    std::memcpy(saved.bank_table, At(at::kBankTable), 4);
    std::memcpy(saved.bank_count, At(at::kBankCount), 4);
    saved.party_set = At(at::kPartySetCurrent)[0];
    saved.active = Field_ActiveMember;
    for (unsigned m = 0; m < kMembers; ++m)
        SetLong(At(at::kMemberAnimations + m * 4), static_cast<std::int32_t>(Address(g_member_records[m])));
    SetLong(At(at::kFrameOffsets), static_cast<std::int32_t>(Address(g_offsets)));
    SetLong(At(at::kBankTable), static_cast<std::int32_t>(Address(g_banks)));
    Field_ActiveMember = g_active;
}
void Restore(const Saved& saved) {
    std::memcpy(At(at::kPartySetRows), saved.rows, kRowBytes);
    std::memcpy(At(at::kMemberAnimations), saved.members, sizeof saved.members);
    std::memcpy(At(at::kFrameOffsets), saved.offsets, 4);
    std::memcpy(At(at::kBankTable), saved.bank_table, 4);
    std::memcpy(At(at::kBankCount), saved.bank_count, 4);
    At(at::kPartySetCurrent)[0] = saved.party_set;
    Field_ActiveMember = saved.active;
}

// --- the seeding -------------------------------------------------------------------

std::uint32_t g_rng = 0x2545F491u;
std::uint32_t Next() { g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5; return g_rng; }
bool Half() { return Next() % 2 == 0; }
bool Often() { return Next() % 3 == 0; }
template <class T, std::size_t N> T Pick(const T (&values)[N]) { return values[Next() % N]; }
std::uint32_t High() { return Next() & 0xFFFFFF00u; }   // what a caller's register may hold above the byte

// A bank number: most from a few values the records repeat, so that a search
// finds the first of several; some anything.
std::uint16_t Bank() { return static_cast<std::uint16_t>(Next() % 4 ? Next() % 12 : Next()); }

void SeedTablesOnce() {
    for (auto& records : g_member_records)
        for (unsigned i = 0; i < kMemberRecords; ++i) {
            SetWord(records + i * 4, Bank());
            records[i * 4 + 2] = static_cast<unsigned char>(Next());
            records[i * 4 + 3] = static_cast<unsigned char>(Next());
        }
    for (unsigned c = 0; c < kColumns; ++c)
        SetLong(g_offsets + c * 4, static_cast<std::int32_t>(kColumns * 4 + c * kColumnWords * 2));
    for (unsigned i = kColumns * 4; i < sizeof g_offsets; ++i) g_offsets[i] = static_cast<unsigned char>(Next());
    for (unsigned k = 0; k < kSprites; ++k) {
        unsigned char* const a = g_anim[k];
        for (unsigned i = 0; i < kAnimBytes; ++i) a[i] = static_cast<unsigned char>(Next());
        SetLong(a, kImages);
        SetLong(a + 4, static_cast<std::int32_t>(Next()));
        SetLong(a + 8, kWords);
        SetLong(a + 0xC, kEntries);
    }
}

void SeedBanks() {
    for (unsigned i = 0; i < kBankRecords; ++i) {
        SetWord(g_banks + i * 8, Bank());
        for (unsigned j = 2; j < 8; ++j) g_banks[i * 8 + j] = static_cast<unsigned char>(Next());
        g_banks[i * 8 + 7] = static_cast<unsigned char>(Next() % kColumns);   // becomes +0x2C, which indexes g_offsets
    }
    static const unsigned char kCounts[] = {0, 1, 2, 3, 8, 0x7F, 0x80, 0xFF};
    const unsigned count = Half() ? Pick(kCounts) : Next() % 256;
    SetLong(At(at::kBankCount), static_cast<std::int32_t>(High() | count));
}

void SeedSprite(unsigned char* s, unsigned k) {
    for (unsigned i = 0; i < kSpriteBytes; ++i) s[i] = static_cast<unsigned char>(Next());
    static const unsigned char kTypes[] = {0, 1, 5, 6, 6, 7, 8, 9, 0xFF};
    s[6] = Next() % 4 ? static_cast<unsigned char>(Next() % 8) : Half() ? Pick(kTypes) : static_cast<unsigned char>(Next());
    static const unsigned char kSlots[] = {0, 1, 0x0F, 0x10, 0x11, 0x1F, 0x20, 0xFF};
    s[0x25] = Half() ? Pick(kSlots) : static_cast<unsigned char>(Next());
    SetWord(s + 0x2C, Next() % kColumns);
    SetLong(s + 0x4C, static_cast<std::int32_t>(Address(g_anim[k])));
    SetLong(s + 0x64, static_cast<std::int32_t>(Next() % kMembers));
    static const std::uint32_t kRows[] = {0, 1, 79, 80, 81, 159, 160, 0xFFFFFFFFu, 0xFFFFFFB1u, 0xFFFFFFB0u,
                                          0xFFFFFFAFu, 0x7FFFFFFFu, 0x80000000u, 0x50, 0xA0, 0xF0};
    SetLong(s + 0x68, static_cast<std::int32_t>(Half() ? Pick(kRows) : Next()));
}

void SeedRound() {
    for (unsigned i = 0; i < kRowBytes; ++i) At(at::kPartySetRows)[i] = static_cast<unsigned char>(Next() % kMembers);
    At(at::kPartySetCurrent)[0] = static_cast<unsigned char>(Next() % (kRowBytes / 3 - 1) | (Half() ? 0x80 : 0));
    SeedBanks();
    for (unsigned i = 0; i < sizeof g_active; ++i) g_active[i] = static_cast<unsigned char>(Next());
    if (Half()) g_active[0xA1] = 0xFF;
    for (unsigned k = 0; k < kSprites; ++k) SeedSprite(Sprite(k), k);
    for (unsigned k = 0; k < kSprites; ++k)                      // the frame entries' bytes
        for (unsigned i = 0; i < 24; ++i) g_anim[k][kEntries + Next() % 0x300] = static_cast<unsigned char>(Next());
    // The effect pool's in-use bytes: often all taken, the first free anywhere.
    for (unsigned i = 0; i < 0x40; ++i) Effect_Objects[i * 0x80] = static_cast<unsigned char>(Next() % 4 ? Next() | 1 : 0);
    if (Half()) {
        for (unsigned i = 0; i < 20; ++i) Effect_Objects[i * 0x80] = static_cast<unsigned char>(Next() | 0x10);
        static const unsigned kOnlyFree[] = {0, 1, 18, 19};   // or none free at all
        if (Half()) Effect_Objects[(Half() ? Pick(kOnlyFree) : Next() % 20) * 0x80] = 0;
    }
    static const unsigned char kCounts[] = {0, 1, 0x13, 0x14, 0x27, 0x28, 0x7F, 0xFE, 0xFF};
    Gfx_UploadQueueCount = Half() ? Pick(kCounts) : static_cast<unsigned char>(Next());
    Sprite_Current = Sprite(Next());
}

// The arguments of function k, as whole dwords.
void Arguments(unsigned k, std::uint32_t& a, std::uint32_t& b) {
    a = Next();
    b = Next();
    switch (k) {
    case kSetAnimation: case kSetAnimationAt: case kEnsure: {
        const unsigned char current = Sprite_Current[0x4B];
        unsigned animation = Half() ? Next() % 0x100 : (Next() % 16) | (Half() ? 0x80 : 0);
        if (Often()) animation = current;
        a = High() | animation;
        break;
    }
    case kQueue: a = High() | (Next() % 256); break;
    case kBank: {
        const unsigned count = static_cast<std::uint32_t>(Long(At(at::kBankCount))) & 0xFF;
        const unsigned pick = Next() % 4;
        std::uint32_t bank = Bank();
        if (pick == 0 && count) bank = Word(g_banks + (count - 1) * 8);    // the last one searched
        else if (pick == 1 && count < kBankRecords) bank = Word(g_banks + count * 8);   // the first one not
        a = (Next() & 0xFFFF0000u) | bank;
        break;
    }
    case kFace: {
        // Directions 0..15 are the pairs the game uses (animations 0..6, 5
        // and 6 among them); past them the index reads on unchecked.
        const unsigned d = Next() % 4 ? Next() % 16 : Next() % 256;
        a = High() | d;
        if (Often()) Sprite_Current[0x4B] = At(at::kDirectionAnimations)[d * 2];   // already facing that way
        break;
    }
    case kRelease: if (Often()) Sprite_Current = Effect_Objects + (Next() % 20) * 0x80; break;
    case kReleaseAt: a = High() | (Next() % 0x40); break;
    default: break;
    }
}

// --- the rounds --------------------------------------------------------------------

struct Coverage {
    unsigned bank_path, member_set_up, pending_member, uploads;
    unsigned found, not_found, queue_skipped, queue_wrapped, high_slot;
    unsigned no_turn, type6, no_start, same, turned, start_path;
    unsigned none_free;
};

// Which branches a round took, read off its input and our side's output.
void Count(Coverage& c, unsigned k, const State& input, std::uint32_t a, const State& ours) {
    const unsigned char* const s = input.current;
    const unsigned char count_in = input.memory[kRegionBytes - 1], count_out = ours.memory[kRegionBytes - 1];
    if (count_in != count_out) ++c.uploads;
    switch (k) {
    case kSetAnimationAt:
        if (a & 0x80) { ++c.bank_path; if (!(s[0x24] & 2)) ++c.member_set_up; }
        else if (s[0x24] & 2) ++c.pending_member;
        break;
    case kBank: if (ours.result & 0xFF) ++c.not_found; else ++c.found; break;
    case kQueue:
        if (s[0] & 2) ++c.queue_skipped;
        else {
            if (count_in == 0xFF) ++c.queue_wrapped;
            if (s[0x25] > 0x10) ++c.high_slot;
        }
        break;
    case kFace:
        if (s[6] >= 8) ++c.no_turn;
        else if (s[6] == 6) ++c.type6;
        else if (g_active[0xA1] == 0xFF) ++c.no_start;
        else if (s[0x4B] == At(at::kDirectionAnimations)[(a & 0xFF) * 2]) ++c.same;
        else ++c.turned;
        for (unsigned i = 0; i < ours.log_n && i < kLog; ++i)
            if (ours.log[i].what == 1) ++c.start_path;
        break;
    case kFindFree: if ((ours.result & 0xFF) == 0xFF) ++c.none_free; break;
    default: break;
    }
}

// Runs `rounds` rounds of each function in `which` (theirs[k] against
// kOurs[k]) and returns the number of mismatches.
unsigned Run(const char* phase, void* const (&theirs)[kCount], const bool (&which)[kCount], unsigned rounds,
             unsigned& calls, Coverage& cover) {
    static State input, their_out, our_out;
    unsigned bad = 0;
    for (unsigned k = 0; k < kCount; ++k) {
        if (!which[k]) continue;
        for (unsigned round = 0; round < rounds; ++round) {
            SeedRound();
            g_seed = Next();
            std::uint32_t a, b;
            Arguments(k, a, b);
            std::memset(g_log, 0, sizeof g_log);
            g_log_n = 0;
            Capture(input);
            input.result = 0;
            for (int pass = 0; pass < 2; ++pass) {
                Apply(input);
                const Fn f = reinterpret_cast<Fn>(const_cast<void*>(pass ? kOurs[k] : theirs[k]));
                const std::uint32_t result = f(a, b) & kClones[k].result_mask;
                State& out = pass ? our_out : their_out;
                Capture(out);
                out.result = result;
            }
            calls += their_out.log_n;
            Count(cover, k, input, a, our_out);
            if (std::memcmp(&their_out, &our_out, sizeof their_out) != 0 && ++bad <= 12) {
                unsigned first = 0;
                const auto* x = reinterpret_cast<const unsigned char*>(&their_out);
                const auto* y = reinterpret_cast<const unsigned char*>(&our_out);
                while (first < sizeof their_out && x[first] == y[first]) ++first;
                bof3::Log("shadow      sprite_pose self-test MISMATCH (%s): round %u, %s(0x%X, 0x%X), result 0x%X / 0x%X, log %u / %u, "
                          "first difference at state byte 0x%X", phase, round, kClones[k].name, a, b, their_out.result,
                          our_out.result, their_out.log_n, our_out.log_n, first);
            }
        }
    }
    return bad;
}

}  // namespace

void SelfTest() {
    unsigned region_bytes = 0;
    for (const Region& r : kRegions) region_bytes += r.size;
    if (region_bytes != kRegionBytes) bof3::Fatal("sprite_pose: regions are %u bytes, the state holds %u", region_bytes, kRegionBytes);

    // Phase 1's copies: every call out re-aimed at its stand-in.
    void* alone[kCount];
    for (unsigned k = 0; k < kCount; ++k) {
        const Clone& c = kClones[k];
        bof3::CloneCall calls[5];
        for (int i = 0; i < c.n_calls; ++i) calls[i] = {c.calls[i].offset, StubFor(c.calls[i].target), c.calls[i].target};
        alone[k] = bof3::CloneOriginal(c.name, c.base, c.size, calls, c.n_calls);
        if (!alone[k]) bof3::Fatal("sprite_pose: could not copy %s", c.name);
    }
    // Phase 2's: callees first, each caller's calls into the ten re-aimed at
    // the callee's copy.
    void* chained[kCount] = {};
    auto copy = [&chained](unsigned k) {
        const Clone& c = kClones[k];
        bof3::CloneCall calls[5];
        for (int i = 0; i < c.n_calls; ++i) {
            const std::uint32_t t = c.calls[i].target;
            const void* to = StubFor(t);
            for (unsigned j = 0; j < kCount; ++j)
                if (kClones[j].base == t) to = chained[j];
            if (!to) bof3::Fatal("sprite_pose: %s's callee 0x%X is not copied yet", c.name, (unsigned)t);
            calls[i] = {c.calls[i].offset, to, t};
        }
        chained[k] = bof3::CloneOriginal(c.name, c.base, c.size, calls, c.n_calls);
        if (!chained[k]) bof3::Fatal("sprite_pose: could not copy %s", c.name);
    };
    for (unsigned k : {kQueue, kBank, kSetAnimationAt, kSetAnimation, kEnsure, kFace, kFindFree, kRelease, kReleaseAt, kClearAll})
        copy(k);

    unsigned char* const saved_current = Sprite_Current;
    static State saved_state;
    Capture(saved_state);
    Saved saved;
    Swap(saved);
    SeedTablesOnce();

    constexpr unsigned kAlone = 1500, kChained = 1500;
    unsigned calls_alone = 0, calls_chained = 0;
    Coverage cover_alone = {}, cover_chained = {};
    const bool all[kCount] = {true, true, true, true, true, true, true, true, true, true};
    g = kStubs;
    const unsigned bad_alone = Run("alone", alone, all, kAlone, calls_alone, cover_alone);
    bof3::Log("shadow      sprite_pose: the copies alone done, %u mismatches; chained next", bad_alone);

    // Chained: ours call ours, both sides call the same two stand-ins.
    g = kOriginals;
    g.member_sprite = kStubs.member_sprite;
    g.script_start = kStubs.script_start;
    const bool entries[kCount] = {true, true, true, false, false, true, false, false, false, true};
    const unsigned bad_chained = Run("chained", chained, entries, kChained, calls_chained, cover_chained);
    g = kOriginals;

    Restore(saved);
    Apply(saved_state);
    Sprite_Current = saved_current;

    const Coverage& c = cover_alone;
    bof3::Log("shadow      sprite_pose self-test: alone %u rounds (%u per function), %u calls to the stand-ins, %u MISMATCHES; "
              "chained %u rounds over 5 entries, %u calls to the two stand-ins, %u MISMATCHES; the four sprites, the effect pool, "
              "the upload queue's arrays and count, Sprite_Current, the result and the stand-ins' log compared",
              kAlone * kCount, kAlone, calls_alone, bad_alone, kChained * 5, calls_chained, bad_chained);
    const Coverage& d = cover_chained;
    bof3::Log("shadow      sprite_pose coverage (alone): SetAnimationAt bank path %u (member worked out %u), pending member %u; "
              "queue skipped %u, wrapped from 0xFF %u, high slot %u; bank found %u / not %u; face: no turn %u, type 6 %u, no start %u, "
              "already %u, turned %u (5 or 6 with a start %u); all effects taken %u",
              c.bank_path, c.member_set_up, c.pending_member, c.queue_skipped, c.queue_wrapped, c.high_slot, c.found, c.not_found,
              c.no_turn, c.type6, c.no_start, c.same, c.turned, c.start_path, c.none_free);
    bof3::Log("shadow      sprite_pose coverage (chained): %u rounds queued an upload; SetAnimationAt bank path %u (member worked out %u), "
              "pending member %u; face: no turn %u, type 6 %u, no start %u, already %u, turned %u",
              d.uploads, d.bank_path, d.member_set_up, d.pending_member, d.no_turn, d.type6, d.no_start, d.same, d.turned);
    const unsigned bad = bad_alone + bad_chained;
    if (bad) bof3::Fatal("the sprite pose functions differ from the originals in %u self-test rounds", bad);
}

}  // namespace sprite_pose

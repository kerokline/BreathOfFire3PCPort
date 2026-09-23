// BOF3X_SHADOW=area_entry: a differential fuzz of the five functions against
// byte-copies of Capcom's, once at start-up (docs/area-entry.md section 3).
// Every call out of a copy is re-aimed at a recording stand-in - the calls
// between the five included, so each function is tested alone. The area
// descriptors 0..3 are pointed at descriptors of the fuzz's own for the
// duration, and every region either side writes is compared.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/area_entry_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace area_entry {
namespace {

using namespace move_script;

std::uint32_t Address(const volatile void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
unsigned char* Rec(unsigned slot) { return ObjTrio + slot * kRecord; }

// --- The fuzz's own area data ------------------------------------------------
// Four descriptors (areas 0..3), each with its own script table so that a
// descriptor looked up once where the original looks it up afresh shows.
constexpr unsigned kLinks = 17, kEntries = 16, kChoices = 8, kPlace = 48;
struct Fake {
    unsigned char desc[4][0x48];
    std::uint32_t table[kEntries];             // desc +0x14: the placement entries
    unsigned char place[kEntries][kPlace];
    std::uint32_t scripts[4][256];             // desc +0x18, one per descriptor
    unsigned char links[kLinks * 12];          // desc +0x20
    std::uint32_t choice_ptr[kEntries];        // desc +0x24
    unsigned char choices[kEntries][kChoices * 10];
    unsigned char choice_n[kEntries];          // desc +0x28
    unsigned char list[64];                    // Party_SetUpMembers' list
};
Fake g_fake;

constexpr unsigned kLog = 32;
struct Entry { std::uint32_t what, a, b, c; };
Entry g_log[kLog];
unsigned g_log_n, g_seed;

std::uint32_t Hash() {
    std::uint32_t h = (g_seed + g_log_n) * 0x9E3779B1u;
    h ^= h >> 15;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    return h;
}
void Record(std::uint32_t what, std::uint32_t a = 0, std::uint32_t b = 0, std::uint32_t c = 0) {
    if (g_log_n < kLog) g_log[g_log_n] = {what, a, b, c};
    ++g_log_n;
}

// --- The stand-ins -------------------------------------------------------
// Each records what the real callee reads, and several change what the caller
// reads after them, so that a value kept across a call shows.
std::uint32_t __cdecl StubFlagsTest(const unsigned char* bits, std::uint32_t index) {
    Record(1, Address(bits), index & 0xFF);
    const std::uint32_t h = Hash();
    // The caller tests al: 0x100 is "false" to it.
    return h % 3 == 0 ? 1u : (h % 3 == 1 ? 0u : 0x100u);
}
// Area_ClassifyPending writes none of these; the caller reads them after it.
void __cdecl StubClassify() {
    Record(2, Word(At(kPendingArea)), static_cast<std::uint32_t>(Long(At(kPendingX))), static_cast<std::uint32_t>(Long(At(kPendingZ))));
    const std::uint32_t h = Hash();
    if (h % 3 == 0) SetLong(At(kPendingX), static_cast<std::int32_t>(h * 7));
    if (h % 5 == 0) SetLong(At(kPendingZ), static_cast<std::int32_t>(h * 13));
    if (h % 7 == 0) SetWord(At(kPendingArea), h >> 9);
}
// Area_PickMusic masks the area to a word (the original pushes it stale).
void __cdecl StubPickMusic(unsigned x, unsigned z, unsigned area) { Record(3, x, z, area & 0xFFFF); }
// Kind2_Place reads its byte. Returns 0 in eax, so that the caller's result
// after it is defined on both sides.
std::uint32_t __cdecl StubKind2Place(std::uint32_t b) {
    Record(4, b & 0xFF);
    return 0;
}
void __cdecl StubMemberClear(unsigned i) {
    Record(5, i, Address(Sprite_Current));
    const std::uint32_t h = Hash();
    if (h % 3 == 0) Sprite_Current = Rec((h >> 8) % 6);
    if (h % 5 == 0) Game_AreaNumber = static_cast<unsigned short>((h >> 12) % 4);
}
// Like ScriptContext_Reset, it writes the context; then moves Sprite_Current
// or its palette byte, which the caller reads next.
void __cdecl StubContextReset(unsigned char* context) {
    Record(6, Address(context));
    const std::uint32_t h = Hash();
    SetWord(context + 0xA, 0);
    context[7] = context[2] = context[5] = context[0] = context[1] = context[3] = 0;
    context[4] = 3;
    if (h % 3 == 0) Sprite_Current = Rec((h >> 8) % 6);
    if (h % 5 == 0) Sprite_Current[kRecPalette] = static_cast<unsigned char>(h >> 16);
}
void __cdecl StubLoadPalette(unsigned short* dst, unsigned index) { Record(7, Address(dst), index); }
// Field_MemberSprite masks both; it may move Field_State or the member byte
// the caller reads after it.
void __cdecl StubMemberSprite(unsigned member, unsigned slot) {
    Record(8, member & 0xFF, slot & 0xFF, Address(Sprite_Current));
    const std::uint32_t h = Hash();
    if (h % 3 == 0) Sprite_Current[kRecMember] = static_cast<unsigned char>((h >> 8) % 24);
    if (h % 5 == 0) Field_State = Rec((h >> 16) % 6);
}
void __cdecl StubSetUp(unsigned count, const unsigned char* list) { Record(9, count & 0xFF, Address(list), list[0]); }
// Party_SwapMembers: the caller reads the slot list and the member count
// after it.
void __cdecl StubSwapMembers(unsigned a, unsigned b, unsigned keep) {
    Record(10, a, b & 0xFF, keep);
    const std::uint32_t h = Hash();
    if (h % 3 == 0) At(kPickSlot)[(h >> 4) % 4] = static_cast<unsigned char>((h >> 8) % 6);
    if (h % 7 == 0) Field_MemberCount = static_cast<unsigned char>((h >> 16) % 6);
}
// ObjTrio_SwapFields swaps the member bytes the caller reads next.
void __cdecl StubSwapFields(unsigned a, unsigned b, unsigned keep) {
    Record(11, a & 0xFF, b & 0xFF, keep & 0xFF);
    const std::uint32_t h = Hash();
    Rec(a & 0xFF)[kRecMember] = static_cast<unsigned char>(h % 24);
    Rec(b & 0xFF)[kRecMember] = static_cast<unsigned char>((h >> 8) % 24);
}

template <class T, class U> T As(U u) { return reinterpret_cast<T>(reinterpret_cast<void*>(u)); }
const Callees kStubs = {
    As<unsigned char (__cdecl*)(const unsigned char*, unsigned)>(&StubFlagsTest),
    StubClassify,
    StubPickMusic,
    As<void (__cdecl*)(unsigned char)>(&StubKind2Place),
    StubMemberClear,
    StubContextReset,
    StubLoadPalette,
    StubMemberSprite,
    StubSetUp,
    StubSwapMembers,
    StubSwapFields,
};

const void* StubFor(std::uint32_t target) {
    const auto f = [](auto p) { return reinterpret_cast<const void*>(p); };
    switch (target) {
    case 0x57C140: return f(&StubFlagsTest);
    case 0x595350: return f(&StubClassify);
    case 0x5953B0: return f(&StubPickMusic);
    case 0x5734F0: return f(&StubKind2Place);
    case 0x536730: return f(&StubMemberClear);
    case 0x5322B0: return f(&StubContextReset);
    case 0x5366A0: return f(&StubLoadPalette);
    case 0x533BA0: return f(&StubMemberSprite);
    case 0x5321C0: return f(&StubSetUp);
    case 0x5322D0: return f(&StubSwapMembers);
    case 0x5323E0: return f(&StubSwapFields);
    default: bof3::Fatal("area_entry: no stand-in for a call to 0x%X", (unsigned)target); return nullptr;
    }
}

// The copies and their calls out, by capstone 2026-09-22; every jump stays
// inside. Sizes are each body to its last instruction.
struct Call { std::uint32_t offset, target; };
struct Clone { const char* name; std::uint32_t base, size; const Call* calls; int n_calls; };
constexpr Call kLinkCalls[] = {{0xBC, 0x57C140}, {0x148, 0x595350}, {0x168, 0x5953B0}};
constexpr Call kDropCalls[] = {{0x189, 0x5322D0}, {0x1C5, 0x5321C0}, {0x1F0, 0x5734F0}};
constexpr Call kSetUpCalls[] = {{0x4E, 0x536730}, {0x9A, 0x5322B0}, {0xB4, 0x5366A0}};
constexpr Call kSwapCalls[] = {{0x12, 0x5323E0}, {0x40, 0x533BA0}, {0xB7, 0x533BA0}};

enum Fn : unsigned { kLinkAt, kDropIn, kSetUp, kSwapMembers, kSwapFields, kCount };
const Clone kClones[kCount] = {
    {"Area_LinkAt", 0x5951D0, 0x177, kLinkCalls, 3},
    {"Party_DropIn", 0x531F90, 0x227, kDropCalls, 3},
    {"Party_SetUpMembers", 0x5321C0, 0xE6, kSetUpCalls, 3},
    {"Party_SwapMembers", 0x5322D0, 0x10C, kSwapCalls, 3},
    {"ObjTrio_SwapFields", 0x5323E0, 0x16A, nullptr, 0},
};

const void* Ours(unsigned k) {
    const auto f = [](auto p) { return reinterpret_cast<const void*>(p); };
    switch (k) {
    case kLinkAt: return f(&Area_LinkAt);
    case kDropIn: return f(&Party_DropIn);
    case kSetUp: return f(&Party_SetUpMembers);
    case kSwapMembers: return f(&Party_SwapMembers);
    default: return f(&ObjTrio_SwapFields);
    }
}

// --- The state a round compares -------------------------------------------
struct Region { std::uint32_t at, size; };
constexpr unsigned kObjBytes = 6 * kRecord;   // ObjTrio's three records and three past them (D18's reach)
const Region kRegions[] = {
    {Address(ObjTrio), kObjBytes},
    {0x903840, 0x40},                           // Camera_Distance.. the pending x / z, the scratch lists
    {0x904060, 0x20},                           // the party list and what follows
    {kCharacterRecords, 8 * kCharacterBytes},
    {kPendingArea, 2},
    {kPendingFlags, 1},
    {Address(&Field_ScriptFlags), 2},
    {Address(&Field_ScriptFlags2), 2},
    {Address(&Sprite_Current), 4},
    {Address(&Field_State), 4},
    {Address(&Field_MemberCount), 1},
    {Address(&Game_AreaNumber), 2},
    {Address(Area_Descriptors), 16},
};
constexpr unsigned kRegionBytes = kObjBytes + 0x40 + 0x20 + 8 * kCharacterBytes + 2 + 1 + 2 + 2 + 4 + 4 + 1 + 2 + 16;

struct State {
    unsigned char memory[kRegionBytes];
    Fake fake;
    std::uint32_t result;
    Entry log[kLog];
    unsigned log_n;
};
void Capture(State& s) {
    unsigned at = 0;
    for (const Region& r : kRegions) { std::memcpy(s.memory + at, At(r.at), r.size); at += r.size; }
    s.fake = g_fake;
    std::memcpy(s.log, g_log, sizeof s.log);
    s.log_n = g_log_n;
}
void Apply(const State& s) {
    unsigned at = 0;
    for (const Region& r : kRegions) { std::memcpy(At(r.at), s.memory + at, r.size); at += r.size; }
    g_fake = s.fake;
    std::memset(g_log, 0, sizeof g_log);
    g_log_n = 0;
}

std::uint32_t g_rng = 0x6A09E667u;
std::uint32_t Next() { g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5; return g_rng; }
bool Often() { return Next() % 3 == 0; }
// A byte argument with stale upper bytes, as Capcom's callers push them.
std::uint32_t Stale(unsigned b) { return Next() % 2 ? (Next() & 0xFFFFFF00u) | (b & 0xFF) : b & 0xFF; }
unsigned Pick(std::initializer_list<unsigned> seeds) {
    const unsigned i = Next() % (seeds.size() + 1);
    return i < seeds.size() ? seeds.begin()[i] : Next() & 0xFF;
}
void SetPtr(unsigned char* at, const void* p) { SetLong(at, static_cast<std::int32_t>(Address(p))); }

using Fn1 = std::uint32_t (__cdecl*)(std::uint32_t);
using Fn2 = std::uint32_t (__cdecl*)(std::uint32_t, std::uint32_t);
using Fn3 = std::uint32_t (__cdecl*)(std::uint32_t, std::uint32_t, std::uint32_t);
struct Args { std::uint32_t a[3]; };

std::uint32_t Run(const void* fn, unsigned k, const Args& x) {
    void* const p = const_cast<void*>(fn);
    switch (k) {
    case kLinkAt: return reinterpret_cast<Fn2>(p)(x.a[0], x.a[1]) & 0xFF;   // every caller tests al
    case kDropIn: return reinterpret_cast<Fn1>(p)(x.a[0]);
    case kSetUp: reinterpret_cast<Fn2>(p)(x.a[0], x.a[1]); return 0;
    default: reinterpret_cast<Fn3>(p)(x.a[0], x.a[1], x.a[2]); return 0;
    }
}

// Every descriptor points at the shared lists; each its own script table.
void WireDescriptors() {
    for (unsigned d = 0; d < 4; ++d) {
        unsigned char* const desc = g_fake.desc[d];
        Area_Descriptors[d] = desc;
        SetPtr(desc + 0x14, g_fake.table);
        SetPtr(desc + 0x18, g_fake.scripts[d]);
        SetPtr(desc + 0x20, g_fake.links);
        SetPtr(desc + 0x24, g_fake.choice_ptr);
        SetPtr(desc + 0x28, g_fake.choice_n);
    }
    for (unsigned e = 0; e < kEntries; ++e) {
        g_fake.table[e] = Address(g_fake.place[e]);
        g_fake.choice_ptr[e] = Address(g_fake.choices[e]);
    }
}

// One round's state and arguments for function k.
Args Generate(unsigned k, State& input) {
    auto* bytes = reinterpret_cast<unsigned char*>(&input);
    for (unsigned i = 0; i < offsetof(State, result); ++i) bytes[i] = static_cast<unsigned char>(Next());
    g_seed = Next();
    Apply(input);
    WireDescriptors();
    Game_AreaNumber = static_cast<unsigned short>(Next() % 4);
    Sprite_Current = Rec(Next() % 3);
    Field_State = Rec(Next() % 3);
    for (unsigned r = 0; r < 6; ++r) Rec(r)[kRecMember] = static_cast<unsigned char>(Next() % 3 ? Next() % 6 : Next() % 24);
    Args x{};
    for (auto& a : x.a) a = Next();
    switch (k) {
    case kLinkAt: {
        const unsigned n = Pick({0, 1, 2, 5, 16});
        unsigned char* const desc = g_fake.desc[Game_AreaNumber];
        desc[0x30] = static_cast<unsigned char>(n > 16 ? 16 : n);
        for (unsigned i = 0; i < kLinks; ++i) {
            unsigned char* const l = g_fake.links + i * 12;
            l[0] = static_cast<unsigned char>(Next() % 4 ? Next() % 12 : Pick({0xFE, 0xFF, 0xF0}));
            l[1] = static_cast<unsigned char>(Next() % 4 ? Next() % 12 : Pick({0xFE, 0xFF, 0xF0}));
            l[8] = static_cast<unsigned char>(Next() % 2 ? 0 : Pick({1, 0x80, 0xFF}));
            l[9] = static_cast<unsigned char>(Pick({0, 1, 2, 3, 4, 8}));
            if (Next() % 3 == 0) {
                SetWord(l + 2, 0xFFFF);
                SetWord(l + 4, Next() % kEntries);
            } else if (Next() % 8 == 0) {
                SetWord(l + 2, 0xFFFE);
            }
        }
        for (unsigned e = 0; e < kEntries; ++e) g_fake.choice_n[e] = static_cast<unsigned char>(Pick({0, 1, 2, 3, 7}) % kChoices);
        // The cell: on some link's run (its end, one past it, or the start
        // minus one), or anywhere.
        const unsigned char* const l = g_fake.links + (Next() % (desc[0x30] + 1u)) * 12;
        const int i = static_cast<int>(Next() % 3 ? Next() % (l[9] + 1u) : l[9]) - (Next() % 6 == 0 ? 1 : 0);
        unsigned cx = l[0], cz = l[1];
        if (l[8]) cz += static_cast<unsigned>(i);
        else cx += static_cast<unsigned>(i);
        if (Next() % 6 == 0) { cx = Next(); cz = Next(); }
        x.a[0] = Stale(cx);
        x.a[1] = Stale(cz);
        break;
    }
    case kDropIn: {
        const unsigned entry = Next() % kEntries;
        x.a[0] = Stale(entry);
        unsigned char* const p = g_fake.place[entry];
        const unsigned kind = Pick({0, 0, 0, 0x40, 0x80, 0x80, 0xC0}) & 0xC0;
        const unsigned n = Next() % 8 ? Pick({0, 1, 2, 3, 3, 4}) % 6 : Pick({5, 6, 15}) & 0xF;
        p[0] = static_cast<unsigned char>(kind | (Next() & 0x30) | n);
        unsigned members;
        if (kind == 0) members = Next() % 4 ? n : n + Next() % 3 - 1;
        else members = n + Next() % 3 - 1;
        Field_MemberCount = static_cast<unsigned char>(members & 0xFF);
        for (unsigned i = 0; i < 6; ++i)
            if (Often()) At(kPartyList)[i] = 0xFF;
        for (unsigned i = 0; i < n; ++i) {
            const unsigned r = Next() % 4;
            if (r == 0) p[1 + i] = 0x80;
            else if (r < 3 && members != 0) p[1 + i] = Rec(Next() % (members < 6 ? members : 6))[kRecMember];
            else p[1 + i] = static_cast<unsigned char>(Next() % 8);
        }
        break;
    }
    case kSetUp:
        x.a[0] = Stale(Pick({0, 1, 2, 3, 3, 4, 5}) % 6);
        x.a[1] = Address(g_fake.list);
        break;
    case kSwapMembers:
    case kSwapFields: {
        const unsigned a = Next() % 4 ? Next() % 3 : Next() % 6, b = Next() % 4 ? Next() % 3 : Next() % 6;
        x.a[0] = Stale(a);
        x.a[1] = Stale(Next() % 5 == 0 ? a : b);
        x.a[2] = Next() % 2 ? 0 : (Next() % 2 ? Next() & 0xFFFFFF00u : Next());
        break;
    }
    }
    return x;
}

}  // namespace

void SelfTest() {
    void* clones[kCount];
    for (unsigned k = 0; k < kCount; ++k) {
        const Clone& c = kClones[k];
        bof3::CloneCall calls[4];
        for (int i = 0; i < c.n_calls; ++i) calls[i] = {c.calls[i].offset, StubFor(c.calls[i].target), c.calls[i].target};
        clones[k] = bof3::CloneOriginal(c.name, c.base, c.size, calls, c.n_calls);
    }
    unsigned region_bytes = 0;
    for (const Region& r : kRegions) region_bytes += r.size;
    if (region_bytes != kRegionBytes) bof3::Fatal("area_entry: regions are %u bytes, the state holds %u", region_bytes, kRegionBytes);

    constexpr unsigned kRounds = 40000;
    static State saved, input, theirs, ours;
    Capture(saved);
    g = kStubs;
    unsigned bad = 0, calls = 0, per[kCount] = {}, bad_per[kCount] = {};
    for (unsigned round = 0; round < kRounds; ++round) {
        const unsigned k = round % kCount;
        ++per[k];
        const Args x = Generate(k, input);
        Capture(input);
        for (int pass = 0; pass < 2; ++pass) {
            Apply(input);
            State& out = pass ? ours : theirs;
            const std::uint32_t result = Run(pass ? Ours(k) : clones[k], k, x);
            Capture(out);
            out.result = result;
        }
        calls += theirs.log_n;
        if (std::memcmp(&theirs, &ours, sizeof theirs) != 0) {
            ++bad_per[k];
            if (++bad <= 12) {
                const auto* t = reinterpret_cast<const unsigned char*>(&theirs);
                const auto* o = reinterpret_cast<const unsigned char*>(&ours);
                unsigned first = 0;
                while (t[first] == o[first]) ++first;
                bof3::Log("shadow      area_entry self-test MISMATCH: round %u, %s, result %08X / %08X, log %u / %u, "
                          "first difference at state +0x%X: %02X / %02X",
                          round, kClones[k].name, theirs.result, ours.result, theirs.log_n, ours.log_n, first, t[first], o[first]);
            }
        }
    }
    g = kOriginals;
    Apply(saved);
    bof3::Log("shadow      area_entry self-test: %u rounds (%u per function, 5 functions), %u calls to the stand-ins, %u MISMATCHES",
              kRounds, per[0], calls, bad);
    for (unsigned k = 0; k < kCount; ++k)
        if (bad_per[k]) bof3::Log("shadow      area_entry self-test: %s %u of %u rounds differ", kClones[k].name, bad_per[k], per[k]);
    if (bad) bof3::Fatal("the area entry functions differ from the original in %u of %u self-test rounds", bad, kRounds);
}

}  // namespace area_entry

// BOF3X_SHADOW=move_cmds: a differential fuzz of the fifteen movement commands
// against byte-copies of Capcom's, once at start-up (docs/move-cmds.md section
// 3). Every call out of a copy is re-aimed at a recording stand-in - the calls
// between the fifteen included, so each function is tested alone - and
// MoveScript_Variable's jump table is relocated into its copy. Math_Ratan2 is
// fuzzed on its own, its copy keeping the tail jump to the CRT's _ftol (plain
// x87, safe before the CRT is up), under three x87 control words.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/move_cmds_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace move_cmds {
namespace {

using namespace move_script;

std::uint32_t Address(const volatile void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }

constexpr unsigned kBuf = 0x100, kLog = 24, kScript = 0x10100;
unsigned char g_sprite[kBuf], g_context[kBuf];
unsigned char g_script[kScript];   // the label search's script, the peek's frame script
long g_out[4], g_alt[4];            // MoveCmd_HandlePosition's out; what its stand-in may return instead
std::uint32_t g_flags;              // the dword [0x929ED0] points at
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
std::uint32_t Bits(float f) {
    std::uint32_t v;
    std::memcpy(&v, &f, sizeof v);
    return v;
}
// A callee may change what the caller reads, or writes, after it.
void Disturb() {
    const std::uint32_t h = Hash();
    if (h % 3 == 0) Sprite_Current[1] = static_cast<unsigned char>(h >> 8);
    if (h % 5 == 0) Sprite_Current[2] = static_cast<unsigned char>(h >> 16);
    if (h % 7 == 0) Sprite_Current[0x24] = static_cast<unsigned char>(h >> 24);
    if (h % 11 == 0) g_context[7] = static_cast<unsigned char>(h >> 4);
}

long __cdecl StubElevation(long x, long z) {
    Record(1, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(z));
    const std::uint32_t h = Hash();
    switch (h % 3) {   // a high half of 0, sign-extended, or anything
    case 0: return static_cast<long>((h >> 8) & 0xFFFF);
    case 1: return static_cast<short>(h >> 8);
    default: return static_cast<long>(h);
    }
}
int __cdecl StubRatan2(float y, float x) {
    Record(2, Bits(y), Bits(x));
    return static_cast<int>(Hash() % 4096) - 2048;
}
// Like ScriptContext_Reset, it writes bytes the caller writes again after it
// (+0x83 +0x84) or reads (+0x85): a random value, so a store moved before the
// call shows.
void __cdecl StubContextReset(unsigned char* context) {
    Record(3, context == Sprite_Kind2 + kK2Context);
    const std::uint32_t h = Hash();
    SetPos(context, h);
    context[3] = static_cast<unsigned char>(h >> 8);
    context[4] = static_cast<unsigned char>(h >> 16);
    context[5] = static_cast<unsigned char>(h >> 24);
}
void __cdecl StubSetElevation(int v) { Record(4, static_cast<std::uint32_t>(v)); }
// The buffer is a local on both sides: its contents are what the caller uses.
void __cdecl StubAttachOffset(long* out, unsigned char b) {
    Record(5, b);
    for (unsigned i = 0; i < 3; ++i) out[i] = static_cast<long>(Hash() * (2 * i + 3));
}
int __cdecl StubKind() {
    Record(6);
    const std::uint32_t h = Hash();
    switch (h % 8) {
    case 0: case 1: case 2: return 1;
    case 3: return 0;
    case 4: return 2;
    case 5: return 3;
    default: return static_cast<int>(Address(Sprite_Current));
    }
}
void __cdecl StubDetach() { Record(7, Sprite_Current[1], Sprite_Current[2]); Disturb(); }
void __cdecl StubAttachMove(unsigned char t) { Record(8, t); Disturb(); }
// Returns its out, or on one call in four another buffer: the caller must read
// the position through the pointer it gets back.
long* __cdecl StubHandlePosition(long* out, unsigned char handle, unsigned char offset) {
    Record(9, handle, offset);
    const std::uint32_t h = Hash();
    long* const at = h % 4 == 0 ? g_alt : out;
    for (unsigned i = 0; i < 4; ++i) at[i] = static_cast<long>(Hash() * (2 * i + 5) + (h >> 7));
    return at;
}
// The two party calls rewrite what the caller writes around them.
void __cdecl StubPartySetLoad(unsigned a, unsigned b, unsigned c, unsigned mode) {
    Record(10, a, b << 16 | (c & 0xFFFF), mode);
    const std::uint32_t h = Hash();
    if (h % 2) At(kPartyFlags)[0] = static_cast<unsigned char>(h >> 8);
    if (h % 3 == 0) At(kPartyLists)[(h >> 16) % 6] = static_cast<unsigned char>(h >> 20);
    if (h % 5 == 0) Field_MemberCount = static_cast<unsigned char>(h >> 24);
}
unsigned char __cdecl StubPartyJoin(unsigned member) {
    Record(11, member, At(kPartyFlags)[0], Field_MemberCount);
    const std::uint32_t h = Hash();
    if (h % 3 == 0) At(kPartyLists)[(h >> 8) % 6] = static_cast<unsigned char>(h >> 12);
    if (h % 5 == 0) Field_MemberCount = static_cast<unsigned char>(h >> 20);
    return static_cast<unsigned char>(h >> 24);
}

const Callees kStubs = {StubElevation, StubRatan2, StubContextReset, StubSetElevation, StubAttachOffset, StubKind,
                        StubDetach, StubAttachMove, StubHandlePosition, StubPartySetLoad, StubPartyJoin};

const void* StubFor(std::uint32_t target) {
    const auto f = [](auto p) { return reinterpret_cast<const void*>(p); };
    switch (target) {
    case 0x5720C0: return f(&StubElevation);
    case 0x5A7A70: return f(&StubRatan2);
    case 0x5322B0: return f(&StubContextReset);
    case 0x5725F0: return f(&StubSetElevation);
    case 0x578EB0: return f(&StubAttachOffset);
    case 0x57C840: return f(&StubKind);
    case 0x579390: return f(&StubDetach);
    case 0x5793B0: return f(&StubAttachMove);
    case 0x578DC0: return f(&StubHandlePosition);
    case 0x5367E0: return f(&StubPartySetLoad);
    case 0x533EF0: return f(&StubPartyJoin);
    case 0x5B9550: return nullptr;   // the CRT's _ftol: the copy keeps it
    default: bof3::Fatal("move_cmds: no stand-in for a call to 0x%X", (unsigned)target); return nullptr;
    }
}

// The copies and their calls out, by capstone 2026-09-22; every jump stays
// inside. Sizes are each body's extent to its last instruction (and, for the
// variables, the jump table after it).
struct Call { std::uint32_t offset, target; };
struct Clone { const char* name; std::uint32_t base, size; const Call* calls; int n_calls; };
constexpr Call kMoveMemberCalls[] = {{0x7D, 0x5720C0}, {0xBD, 0x5A7A70}, {0xF2, 0x5A7A70}};
constexpr Call kMoveKind2Calls[] = {{0xA3, 0x5720C0}};
constexpr Call kPlaceCalls[] = {{0x5, 0x5322B0}, {0x3E, 0x5720C0}, {0x4A, 0x5725F0}};
constexpr Call kOpF7Calls[] = {{0x24, 0x5720C0}};
constexpr Call kHandleCalls[] = {{0x93, 0x578EB0}};
constexpr Call kAttachCalls[] = {{0x2, 0x57C840}, {0x1C, 0x579390}, {0x6B, 0x5793B0}, {0x85, 0x579390}, {0xCE, 0x5793B0}};
constexpr Call kDetachCalls[] = {{0x9, 0x57C840}};
constexpr Call kAttachMoveCalls[] = {{0x28, 0x578DC0}};
constexpr Call kPartyResetCalls[] = {{0xC, 0x5367E0}, {0x4D, 0x533EF0}};
constexpr Call kRatan2Calls[] = {{0x16, 0x5B9550}};

enum Fn : unsigned {
    kMoveMember, kPoseWait, kMoveKind2, kPlace, kOpF7, kHandle, kAttach, kDetach, kAttachMove, kFindLabel, kVariable,
    kObjectKind, kPeek, kPartyReset, kRatan2, kCount
};
constexpr unsigned kFuzzed = kRatan2;   // the round-robin ones; Math_Ratan2 has its own loop
const Clone kClones[kCount] = {
    {"Party_MoveMember", 0x5190A0, 0x1F9, kMoveMemberCalls, 3},
    {"Sprite_SetPoseWait", 0x518B00, 0x16, nullptr, 0},
    {"MoveCmd_MoveKind2", 0x573400, 0xED, kMoveKind2Calls, 1},
    {"Kind2_Place", 0x5734F0, 0x66, kPlaceCalls, 3},
    {"MoveCmd_OpF7", 0x578D10, 0xA4, kOpF7Calls, 1},
    {"MoveCmd_HandlePosition", 0x578DC0, 0xEA, kHandleCalls, 1},
    {"MoveCmd_Attach", 0x5792A0, 0xE8, kAttachCalls, 5},
    {"MoveCmd_Detach", 0x579390, 0x1D, kDetachCalls, 1},
    {"MoveCmd_AttachMove", 0x5793B0, 0x92, kAttachMoveCalls, 1},
    {"MoveScript_FindLabel", 0x579450, 0x78, nullptr, 0},
    {"MoveScript_Variable", 0x57C310, 0xD0, nullptr, 0},
    {"MoveScript_ObjectKind", 0x57C840, 0x52, nullptr, 0},
    {"Sprite_ScriptPeek", 0x57CDC0, 0x4E, nullptr, 0},
    {"Scena16_PartyReset", 0x519890, 0x57, kPartyResetCalls, 2},
    {"Math_Ratan2", 0x5A7A70, 0x1B, kRatan2Calls, 1},
};
// MoveScript_Variable's `jmp [eax*4 + 0x57C3A4]` at +0xE (its operand at
// +0x11) and the 15 entries at +0x94.
constexpr Table kVariableTable = {0x11, 0x94, 15};

const void* Ours(unsigned k) {
    const auto f = [](auto p) { return reinterpret_cast<const void*>(p); };
    switch (k) {
    case kMoveMember: return f(&Party_MoveMember);
    case kPoseWait: return f(&Sprite_SetPoseWait);
    case kMoveKind2: return f(&MoveCmd_MoveKind2);
    case kPlace: return f(&Kind2_Place);
    case kOpF7: return f(&MoveCmd_OpF7);
    case kHandle: return f(&MoveCmd_HandlePosition);
    case kAttach: return f(&MoveCmd_Attach);
    case kDetach: return f(&MoveCmd_Detach);
    case kAttachMove: return f(&MoveCmd_AttachMove);
    case kFindLabel: return f(&MoveScript_FindLabel);
    case kVariable: return f(&MoveScript_Variable);
    case kObjectKind: return f(&MoveScript_ObjectKind);
    case kPeek: return f(&Sprite_ScriptPeek);
    case kPartyReset: return f(&Scena16_PartyReset);
    default: return f(&Math_Ratan2);
    }
}

// --- The state a round compares -------------------------------------------
struct Region { std::uint32_t at, size; };
const Region kRegions[] = {
    {Address(Sprite_Kind2), Sprite_Kind2_count},
    {Address(Sprite_Objects), Sprite_Objects_count},
    {Address(Sprite_ObjectsExtra), Sprite_ObjectsExtra_count},
    {Address(MoveScript_PartyRecords), MoveScript_PartyRecords_count},
    {Address(&MoveScript_F3Divisor), 2},
    {Address(&Game_AreaNumber), 4},             // with MoveScript_FAWord after it
    {Address(&Field_Kind2Z), 8},                // and Field_Kind2X
    {kSavedSprite, 4},
    {kPartyFlags, 7},                           // and both party lists
    {Address(&Field_MemberCount), 1},
    {Address(&Cond_ByteFA), 0x12},              // 0x8034E0..0x8034F1: Field_StatusBits, MoveScript_Var7, Cond_ByteFD
    {kCounters, 4},
    {kNameIndex, 1},
    {kStoryFlags, 4},
};
constexpr unsigned kRegionBytes = Sprite_Kind2_count + Sprite_Objects_count + Sprite_ObjectsExtra_count +
                                  MoveScript_PartyRecords_count + 2 + 4 + 8 + 4 + 7 + 1 + 0x12 + 4 + 1 + 4;

struct State {
    unsigned char memory[kRegionBytes];
    unsigned char sprite[kBuf], context[kBuf];
    long out[4], alt[4];
    std::uint32_t flags;
    unsigned char* current;
    std::uint32_t flags_ptr;
    std::uint32_t result;
    Entry log[kLog];
    unsigned log_n;
};
void Capture(State& s) {
    unsigned at = 0;
    for (const Region& r : kRegions) { std::memcpy(s.memory + at, At(r.at), r.size); at += r.size; }
    std::memcpy(s.sprite, g_sprite, kBuf);
    std::memcpy(s.context, g_context, kBuf);
    std::memcpy(s.out, g_out, sizeof s.out);
    std::memcpy(s.alt, g_alt, sizeof s.alt);
    s.flags = g_flags;
    s.current = Sprite_Current;
    std::memcpy(&s.flags_ptr, At(kFlagsPtr), 4);
    std::memcpy(s.log, g_log, sizeof s.log);
    s.log_n = g_log_n;
}
void Apply(const State& s) {
    unsigned at = 0;
    for (const Region& r : kRegions) { std::memcpy(At(r.at), s.memory + at, r.size); at += r.size; }
    std::memcpy(g_sprite, s.sprite, kBuf);
    std::memcpy(g_context, s.context, kBuf);
    std::memcpy(g_out, s.out, sizeof g_out);
    std::memcpy(g_alt, s.alt, sizeof g_alt);
    g_flags = s.flags;
    Sprite_Current = s.current;
    std::memcpy(At(kFlagsPtr), &s.flags_ptr, 4);
    std::memset(g_log, 0, sizeof g_log);
    g_log_n = 0;
}

std::uint32_t g_rng = 0x2C1B3C6Du;
std::uint32_t Next() { g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5; return g_rng; }
bool Often() { return Next() % 3 == 0; }
// A byte argument with stale upper bytes, as Capcom's callers push them.
std::uint32_t Stale(unsigned char b) { return (Next() & 0xFFFFFF00u) | b; }
unsigned char Pick(std::initializer_list<unsigned> seeds) {
    const unsigned i = Next() % (seeds.size() + 1);
    return static_cast<unsigned char>(i < seeds.size() ? seeds.begin()[i] : Next());
}

// The pointers MoveScript_ObjectKind tells apart, each edge and its neighbour.
std::uint32_t KindPointer() {
    const std::uint32_t k2 = Address(Sprite_Kind2), trio = Address(ObjTrio);
    const std::uint32_t objects = Address(Sprite_Objects), extra = Address(Sprite_ObjectsExtra);
    const std::uint32_t seeds[] = {k2, k2 - 1, k2 + 1, trio, trio + 1, trio + 0x14C, trio + 0x14B, trio + 0x298, trio + 0x299,
                                   objects, objects - 1, objects + Sprite_Objects_count - 1, objects + Sprite_Objects_count,
                                   extra, extra - 1, extra + Sprite_ObjectsExtra_count - 1, extra + Sprite_ObjectsExtra_count,
                                   Address(g_sprite), objects + (Next() % 30) * 0xA4, extra + (Next() % 4) * 0xA4, Next()};
    return seeds[Next() % (sizeof seeds / sizeof seeds[0])];
}

// A byte the walk may land on without stopping or finding a label: any op of
// non-zero length but 0A.
unsigned char SafeOp() {
    unsigned char op;
    do op = static_cast<unsigned char>(Next());
    while (op == 0x0A || MoveScript_OpLengths[op] == 0);
    return op;
}
// A script the label search walks to its label: ops of every step size, other
// labels, F8 with and without 07, 0E / 0F with and without mask 2 - and never
// an op of length 0 where the walk lands. Every operand byte is itself an op
// of non-zero length, and the label is followed by a sled of the same label,
// so that a walk put out of step (a negative control) still ends - on another
// position, a mismatch the fuzz counts, rather than a hang. So the label byte
// is an op of ODD length: a walk that lands on one in the sled steps to the
// other parity, onto a 0A (an even length would keep it on the label bytes,
// past the sled). The compare itself takes any byte; 0A is 2 bytes, so the
// label is never 0A.
void BuildScript(unsigned char label) {
    unsigned at = 0;
    const unsigned ops = Next() % 3 ? Next() % 24 : 0;
    for (unsigned i = 0; i < ops; ++i) {
        unsigned char* const p = g_script + at;
        for (unsigned j = 0; j < 6; ++j) p[j] = SafeOp();
        switch (Next() % 6) {
        case 0:   // another label
            p[0] = 0x0A;
            if (p[1] == label) p[1] = static_cast<unsigned char>(label == 0x0B ? 0x0C : 0x0B);
            at += MoveScript_OpLengths[0x0A];
            break;
        case 1:
            p[0] = 0xF8;
            if (Next() % 2) p[1] = 7;
            at += p[1] == 7 ? 5 : 2;
            break;
        case 2:
            p[0] = static_cast<unsigned char>(0x0E + Next() % 2);
            at += (p[3] & 2) ? 5 : 4;
            break;
        default:
            p[0] = SafeOp();
            at += MoveScript_OpLengths[p[0]];
            break;
        }
    }
    for (unsigned i = 0; i < 24; ++i) {
        g_script[at + 2 * i] = 0x0A;
        g_script[at + 2 * i + 1] = label;
    }
}

using Fn0 = std::uint32_t (__cdecl*)();
using Fn1 = std::uint32_t (__cdecl*)(std::uint32_t);
using Fn2 = std::uint32_t (__cdecl*)(std::uint32_t, std::uint32_t);
using Fn3 = std::uint32_t (__cdecl*)(std::uint32_t, std::uint32_t, std::uint32_t);
using Fn4 = std::uint32_t (__cdecl*)(std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t);
using Fn5 = std::uint32_t (__cdecl*)(std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t);
using RatanFn = int (__cdecl*)(float, float);

struct Args { std::uint32_t a[5]; };

std::uint32_t Run(const void* fn, unsigned k, const Args& x) {
    void* const p = const_cast<void*>(fn);
    switch (k) {
    case kMoveMember: reinterpret_cast<Fn2>(p)(x.a[0], x.a[1]); return 0;
    case kPoseWait: reinterpret_cast<Fn1>(p)(x.a[0]); return 0;
    case kMoveKind2: case kPlace: case kAttachMove: reinterpret_cast<Fn1>(p)(x.a[0]); return 0;
    case kOpF7: reinterpret_cast<Fn4>(p)(x.a[0], x.a[1], x.a[2], x.a[3]); return 0;
    case kHandle: {
        const std::uint32_t r = reinterpret_cast<Fn3>(p)(x.a[0], x.a[1], x.a[2]);
        return r == Address(g_out) ? 1 : r;
    }
    case kAttach: reinterpret_cast<Fn5>(p)(x.a[0], x.a[1], x.a[2], x.a[3], x.a[4]); return 0;
    case kDetach: case kPartyReset: reinterpret_cast<Fn0>(p)(); return 0;
    case kFindLabel: return reinterpret_cast<Fn3>(p)(x.a[0], x.a[1], x.a[2]) & 0xFFFF;
    case kVariable: return reinterpret_cast<Fn1>(p)(x.a[0]);
    case kObjectKind: return reinterpret_cast<Fn0>(p)();
    case kPeek: return reinterpret_cast<Fn0>(p)() & 0xFFFF;
    default: bof3::Fatal("move_cmds: no runner for %u", k); return 0;
    }
}

// One round's state and arguments for function k; Sprite_Current is g_sprite
// unless the function is the object kind.
Args Generate(unsigned k, State& input) {
    auto* bytes = reinterpret_cast<unsigned char*>(&input);
    for (unsigned i = 0; i < offsetof(State, current); ++i) bytes[i] = static_cast<unsigned char>(Next());
    input.current = g_sprite;
    input.flags_ptr = Address(&g_flags);
    g_seed = Next();
    Apply(input);
    unsigned char* const s = g_sprite;
    unsigned char* const script = g_script;
    std::memcpy(s + 0x50, &script, sizeof script);
    Args x{};
    for (auto& a : x.a) a = Next();
    switch (k) {
    case kMoveMember: {
        // The slot: one of the eight inside the records, below the objects
        // (0 just below, then negative), or 0x100 + n (the low byte counts).
        const std::uint32_t base = Address(Sprite_ObjectsExtra);
        const unsigned choice = Next() % 10, n = Next() % 8;
        std::uint32_t object;
        if (choice < 7) object = base + n * 0xA4 + Next() % 0xA4;
        else if (choice == 7) object = base - 1 - Next() % 0x200;
        else if (choice == 8) object = base + (0x100 + n) * 0xA4 + Next() % 0xA4;
        else object = base + (0x80 + Next() % 0x80) * 0xA4;
        x.a[0] = object;
        const unsigned char direction = static_cast<unsigned char>(Next() % 4 ? Next() % 8 : Next());
        x.a[1] = Stale(direction);
        std::int32_t slot = static_cast<signed char>((static_cast<std::int32_t>(object - base) / 0xA4) & 0xFF);
        if (slot < 0) slot = 4;
        unsigned char* const record = MoveScript_PartyRecords + slot * 16;
        record[0] = static_cast<unsigned char>(Next() % 4 ? record[0] & ~1u : record[0] | 1u);
        if (Often()) SetLong(s + 0x10, -Long(s + 0xC));
        s[0xA] = Pick({0, 1, 2, 0x3F, 0x40, 0x7F, 0x80, 0xFF});
        const std::uint16_t angle3 = Word(At(Address(Sprite_DirectionAngles) + 6));
        if (Often()) SetLong(s + 0x6C, static_cast<std::int32_t>((Next() & ~0xFFFu) | ((angle3 + Next() % 3 - 1) & 0xFFF)));
        break;
    }
    case kPoseWait:
        x.a[0] = Address(g_context);
        break;
    case kMoveKind2: {
        Sprite_Kind2[kK2Speed] = static_cast<unsigned char>(1 + Next() % 5);
        Sprite_Kind2[kK2Steps] = static_cast<unsigned char>(Pick({1, 2, 0x7F, 0x80, 0xFF}) | 1);
        // A stale divisor that divides: a store of it dropped then shows as a
        // count, not a fault.
        MoveScript_F3Divisor = static_cast<short>(1 + Next() % 0x80);
        const unsigned char direction = Pick({0, 1, 2, 3, 4, 5, 6, 7, 2, 6, 25});
        x.a[0] = Stale(direction);
        break;
    }
    case kPlace:
        x.a[0] = Stale(static_cast<unsigned char>(Next()));
        break;
    case kOpF7:
        if (Often()) x.a[0] = static_cast<std::uint32_t>(Long(s + 0x34)) + Next() % 5 - 2;
        if (Often()) x.a[1] = static_cast<std::uint32_t>(Long(s + 0x38)) + Next() % 5 - 2;
        x.a[2] = Stale(Pick({0, 1, 2, 0x7F, 0x80, 0xFF}));
        x.a[3] = Address(g_context);
        if (Next() % 2) g_context[4] = 0;
        break;
    case kHandle: {
        for (unsigned n = 0; n < 30; ++n)
            if (Often()) Sprite_Objects[n * 0xA4u + 6] = 0x0A;
        const unsigned char low = static_cast<unsigned char>(Next() % 3 ? Next() % 12 : Next() & 0x7F);
        x.a[0] = Address(g_out);
        x.a[1] = Stale(static_cast<unsigned char>(Next() % 2 ? low | 0x80 : low | (Next() & 0x40)));
        x.a[2] = Stale(static_cast<unsigned char>(Next()));
        break;
    }
    case kAttach:
        if (Often()) s[1] = 7;
        if (Often()) s[2] = Next() % 2 ? 1 : 7;
        x.a[0] = Address(g_context);
        x.a[1] = Stale(Next() % 2 ? 7 : Pick({0, 1, 6, 8, 0x87}));
        x.a[2] = Stale(static_cast<unsigned char>(Next()));
        x.a[3] = Stale(static_cast<unsigned char>(Next()));
        x.a[4] = Stale(static_cast<unsigned char>(Next()));
        if (Next() % 2) g_context[4] = 0;
        break;
    case kDetach:
        break;
    case kAttachMove:
        x.a[0] = Stale(Pick({0, 1, 2, 0x7F, 0x80, 0x81, 0xFE, 0xFF}));
        break;
    case kFindLabel: {
        unsigned char label;
        do label = static_cast<unsigned char>(Next());
        while (MoveScript_OpLengths[label] % 2 == 0);
        BuildScript(label);
        std::uint16_t position = static_cast<std::uint16_t>(Next());
        position = static_cast<std::uint16_t>(Next() % 4 ? position | 0x4000 : position & ~0x4000u);
        SetPos(g_context, position);
        x.a[0] = Address(g_context);
        x.a[1] = Stale(label);
        x.a[2] = Address(g_script);
        break;
    }
    case kVariable:
        x.a[0] = Stale(static_cast<unsigned char>(Next() % 4 ? Next() % 16 : Next()));
        break;
    case kObjectKind:
        Sprite_Current = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(KindPointer()));
        break;
    case kPeek: {
        s[0x4A] = Pick({1, 1, 1, 0, 2});
        std::uint16_t position = static_cast<std::uint16_t>(Next() % 3 ? Next() % 0x200 : Next());
        SetWord(s + 0x58, position);
        if (Next() % 2) s[0x49] = static_cast<unsigned char>(position >> 1);
        else if (Often()) s[0x49] = static_cast<unsigned char>((position >> 1) + 1);
        g_script[position] = Pick({0x7F, 0x80, 0xFF, 0});
        g_script[position + 1] = static_cast<unsigned char>(Next());
        break;
    }
    case kPartyReset:
        break;
    }
    return x;
}

// Math_Ratan2 alone: the copy (with the real _ftol) and ours under the same
// control word - 0x027F as in game, 0x037F and 0x007F too - on quadrant
// boundaries, signed zeros, infinities, NaNs, denormals and random floats.
unsigned short ControlWord() {
    unsigned short cw;
    __asm__ volatile("fnstcw %0" : "=m"(cw));
    return cw;
}
void SetControlWord(unsigned short cw) { __asm__ volatile("fldcw %0" : : "m"(cw)); }
float Float(std::uint32_t bits) {
    float f;
    std::memcpy(&f, &bits, sizeof f);
    return f;
}
unsigned FuzzRatan2(void* clone, unsigned rounds) {
    const auto theirs = reinterpret_cast<RatanFn>(clone);
    const std::uint32_t seeds[] = {0x00000000u, 0x80000000u, 0x3F800000u, 0xBF800000u, 0x43000000u, 0xC3000000u,
                                   0x7F800000u, 0xFF800000u, 0x7FC00000u, 0xFFC00000u, 0x7FA00000u, 0x00000001u,
                                   0x80000001u, 0x007FFFFFu, 0x7F7FFFFFu, 0xFF7FFFFFu, 0x47000000u, 0xC7000000u,
                                   0x46FFFE00u, 0xC7000100u, 0x3F000000u, 0x34000000u};
    constexpr unsigned kSeeds = sizeof seeds / sizeof seeds[0];
    const unsigned short words[] = {0x027F, 0x037F, 0x007F};
    const unsigned short saved = ControlWord();
    unsigned bad = 0;
    for (unsigned round = 0; round < rounds; ++round) {
        std::uint32_t by, bx;
        switch (round % 4) {
        case 0: by = seeds[Next() % kSeeds]; bx = seeds[Next() % kSeeds]; break;
        case 1: by = Bits(static_cast<float>(static_cast<short>(Next()))); bx = Next() % 2 ? 0x43000000u : seeds[Next() % kSeeds]; break;
        case 2: by = Next(); bx = Next(); break;
        default: by = Bits(static_cast<float>(static_cast<int>(Next() % 2001) - 1000)); bx = Bits(static_cast<float>(static_cast<int>(Next() % 2001) - 1000)); break;
        }
        const unsigned short cw = words[round % 7 == 0 ? 1 + (round / 7) % 2 : 0];
        SetControlWord(cw);
        const int a = theirs(Float(by), Float(bx));
        const unsigned short after_a = ControlWord();
        SetControlWord(cw);
        const int b = Math_Ratan2(Float(by), Float(bx));
        const unsigned short after_b = ControlWord();
        SetControlWord(saved);
        if ((a != b || after_a != after_b) && ++bad <= 8)
            bof3::Log("shadow      move_cmds self-test MISMATCH: Math_Ratan2(%08X, %08X) cw %04X: %d / %d, cw after %04X / %04X",
                      by, bx, cw, a, b, after_a, after_b);
    }
    return bad;
}

}  // namespace

void SelfTest() {
    // The two doubles ours holds, against the image's.
    double scale, inverse_pi;
    std::memcpy(&scale, At(kScaleAt), sizeof scale);
    std::memcpy(&inverse_pi, At(kInversePiAt), sizeof inverse_pi);
    std::uint64_t bits;
    std::memcpy(&bits, &inverse_pi, sizeof bits);
    if (scale != 2048.0 || bits != 0x3FD461D59AE78A99ull) bof3::Fatal("move_cmds: Math_Ratan2's constants are not the image's");

    void* clones[kCount];
    for (unsigned k = 0; k < kCount; ++k) {
        const Clone& c = kClones[k];
        bof3::CloneCall calls[8];
        for (int i = 0; i < c.n_calls; ++i) calls[i] = {c.calls[i].offset, StubFor(c.calls[i].target), c.calls[i].target};
        clones[k] = bof3::CloneOriginal(c.name, c.base, c.size, calls, c.n_calls);
    }
    Relocate(clones[kVariable], kClones[kVariable].base, kClones[kVariable].size, kVariableTable);

    constexpr unsigned kRounds = 28000, kRatanRounds = 20000;
    unsigned region_bytes = 0;
    for (const Region& r : kRegions) region_bytes += r.size;
    if (region_bytes != kRegionBytes) bof3::Fatal("move_cmds: regions are %u bytes, the state holds %u", region_bytes, kRegionBytes);
    static State saved, input, theirs, ours;
    Capture(saved);
    g = kStubs;
    unsigned bad = 0, calls = 0, per[kFuzzed] = {}, bad_per[kFuzzed] = {};
    for (unsigned round = 0; round < kRounds; ++round) {
        const unsigned k = round % kFuzzed;
        ++per[k];
        const Args x = Generate(k, input);
        Capture(input);
        for (int pass = 0; pass < 2; ++pass) {
            Apply(input);
            State& out = pass ? ours : theirs;
            const std::uint32_t result = Run(pass ? Ours(k) : clones[k], k, x);
            // out[3] is an uninitialised stack dword of the original's
            // (docs/move-cmds.md section 2): not compared.
            if (k == kHandle) g_out[3] = 0;
            Capture(out);
            out.result = result;
        }
        calls += theirs.log_n;
        if (std::memcmp(&theirs, &ours, sizeof theirs) != 0) {
            ++bad_per[k];
            if (++bad <= 12)
                bof3::Log("shadow      move_cmds self-test MISMATCH: round %u, %s, result %08X / %08X, log %u / %u", round,
                          kClones[k].name, theirs.result, ours.result, theirs.log_n, ours.log_n);
        }
    }
    g = kOriginals;
    Apply(saved);
    const unsigned ratan_bad = FuzzRatan2(clones[kRatan2], kRatanRounds);
    bof3::Log("shadow      move_cmds self-test: %u rounds (%u per function, 14 functions), %u calls to the stand-ins, %u MISMATCHES; "
              "Math_Ratan2 %u rounds under three control words, %u MISMATCHES",
              kRounds, per[0], calls, bad, kRatanRounds, ratan_bad);
    for (unsigned k = 0; k < kFuzzed; ++k)
        if (bad_per[k]) bof3::Log("shadow      move_cmds self-test: %s %u of %u rounds differ", kClones[k].name, bad_per[k], per[k]);
    if (bad || ratan_bad)
        bof3::Fatal("the movement commands differ from the original in %u of %u self-test rounds, Math_Ratan2 in %u of %u",
                    bad, kRounds, ratan_bad, kRatanRounds);
}

}  // namespace move_cmds

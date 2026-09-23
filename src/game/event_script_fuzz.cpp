// BOF3X_SHADOW=event_script: the start-up differential fuzz of event_script.cpp's
// 28 functions (docs/event-script.md, "The fuzz").
//
// Each original is byte-copied with EVERY call re-aimed at a recording
// stand-in - the calls between the interpreter's own functions included, so
// each function is tested alone - and ours runs with the same stand-ins
// through event_script::g. The copies' jump tables are moved into the copies;
// their calls through EventScript_Conditions are re-aimed at a table of 256
// numbered stand-ins (the switch reads it signed, from -128). A round: random
// state with each branch's boundaries seeded, theirs, the same state again,
// ours; every byte of the state, the stand-ins' log (a count, a hash of every
// entry and the first 48 kept) and the result compared. The stand-ins give
// back what the real callee would leave for the caller to read - a moved
// script position, a depth or else byte, Sprite_Current +0x54 - and now and
// then disturb what the caller reads again after the call (the count,
// Sprite_Current, the op's own byte). Scripts are generated so that each loop
// ends: a body of the bytes the function under test does not hang on, then a
// tail of its terminator.
#include <windows.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <utility>

#include "bof3/symbols.gen.h"
#include "game/event_script_callees.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace event_script {
namespace {

std::uint32_t Address(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
unsigned char* At(std::uint32_t a) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a)); }
void SetWord(unsigned char* p, unsigned v) {
    const auto w = static_cast<std::uint16_t>(v);
    std::memcpy(p, &w, sizeof w);
}

std::uint32_t g_rng;
std::uint32_t Next() {
    g_rng ^= g_rng << 13;
    g_rng ^= g_rng >> 17;
    g_rng ^= g_rng << 5;
    return g_rng;
}
bool OneIn(unsigned n) { return Next() % n == 0; }

// --- the stand-ins' log ----------------------------------------------------------

constexpr unsigned kKeep = 48, kIds = 512;
struct Log {
    std::uint32_t n, hash;
    std::uint32_t keep[kKeep][4];
    unsigned counts[kIds];
};
Log g_log;
std::uint32_t g_seed;

void Record(std::uint32_t what, std::uint32_t a = 0, std::uint32_t b = 0, std::uint32_t c = 0) {
    if (g_log.n < kKeep) {
        g_log.keep[g_log.n][0] = what;
        g_log.keep[g_log.n][1] = a;
        g_log.keep[g_log.n][2] = b;
        g_log.keep[g_log.n][3] = c;
    }
    for (const std::uint32_t v : {what, a, b, c}) g_log.hash = (g_log.hash ^ v) * 0x01000193u + 0x9E3779B9u;
    ++g_log.n;
    ++g_log.counts[what % kIds];
}
// Deterministic in the call's position: the same on both sides while the
// calls agree.
std::uint32_t Hash(unsigned salt = 0) {
    std::uint32_t h = (g_seed + g_log.n * 0x10001u + salt * 0x3C6EF372u) * 0x9E3779B1u;
    h ^= h >> 15;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    h *= 0xC2B2AE35u;
    h ^= h >> 16;
    return h;
}

// --- the buffers ------------------------------------------------------------------

constexpr unsigned kScript = 0x300;
alignas(4) unsigned char g_script[kScript];     // the script under test
alignas(4) unsigned char g_entries[0x100];      // readable descriptor +8 entries
alignas(4) unsigned char g_bits[0x40];          // Flags_* arrays
alignas(4) unsigned char g_arm[2];              // an if's else and depth bytes
alignas(4) std::uint32_t g_result;              // a function's result, as a region

constexpr int kFirstObject = -4, kObjects = 38;   // Sprite_Objects -4..33: counts a stand-in may leave
unsigned char* Object(int n) { return Sprite_Objects + n * 0xA4; }
short Count() {
    short v;
    std::memcpy(&v, At(bof3::addr::DamageScratch), sizeof v);
    return v;
}
void SetCount(int v) { SetWord(At(bof3::addr::DamageScratch), static_cast<unsigned>(v)); }

// A count as a stand-in may leave it: most often in range, sometimes at the
// edges of the placements' test, never outside the objects the fuzz owns.
int PickCount(std::uint32_t h) {
    static const int kEdge[] = {29, 30, 31, 0, -1, 28};
    return h % 3 == 0 ? kEdge[(h >> 4) % 6] : static_cast<int>((h >> 8) % 34) - 2;
}

// --- the interpreter's stand-ins --------------------------------------------------------

unsigned Advance() { return 1 + Hash(7) % 4; }
const unsigned char* __cdecl StubOp(const unsigned char* at) {
    Record(1, Address(at), *at);
    return at + Advance();
}
const unsigned char* __cdecl StubControl(const unsigned char* at) {
    Record(2, Address(at), *at);
    return at + Advance();
}
const unsigned char* __cdecl StubIf(const unsigned char* at) {
    Record(3, Address(at));
    return at + Advance();
}
const unsigned char* __cdecl StubIfNot(const unsigned char* at) {
    Record(4, Address(at));
    return at + Advance();
}
const unsigned char* __cdecl StubSwitch(const unsigned char* at) {
    Record(5, Address(at));
    return at + Advance();
}
// An arm's step: FE counts the depth down and stays (as the real ones do - the
// if ends on it only at depth 0); F0 / F1 sometimes count it up (a nested if
// the real step would have run whole); FD sets the else byte, and now and then
// a step flips it, which the if must read afresh.
const unsigned char* ArmStep(std::uint32_t id, const unsigned char* at, unsigned char* else_seen, unsigned char* depth) {
    Record(id, Address(at), *at, static_cast<std::uint32_t>(*else_seen) << 8 | *depth);
    const std::uint32_t h = Hash(1);
    if (*at == 0xFE) {
        --*depth;
        return at;
    }
    if ((*at == 0xF0 || *at == 0xF1) && h % 2) ++*depth;
    if (*at == 0xFD) *else_seen = 1;
    else if (h % 7 == 0) *else_seen = static_cast<unsigned char>(*else_seen ? 0 : 1 + (h >> 8) % 2);
    return at + Advance();
}
const unsigned char* __cdecl StubRunStep(const unsigned char* at, unsigned char* e, unsigned char* d) { return ArmStep(6, at, e, d); }
const unsigned char* __cdecl StubSkipStep(const unsigned char* at, unsigned char* e, unsigned char* d) { return ArmStep(7, at, e, d); }
const unsigned char* __cdecl StubSkipControl(const unsigned char* at) {
    Record(8, Address(at), *at);
    return at + Advance();
}
const unsigned char* __cdecl StubSkipIf(const unsigned char* at) {
    Record(9, Address(at));
    return at + Advance();
}
const unsigned char* __cdecl StubSkipSwitch(const unsigned char* at) {
    Record(10, Address(at));
    return at + Advance();
}
// A case body, run or skipped: to the next F5, F6 or F7 at or after `at` -
// the switch's own bytes, which the fuzz keeps out of the bodies - so that
// the switch always meets one of them next.
const unsigned char* NextMarker(const unsigned char* at) {
    while (*at < 0xF5 || *at > 0xF7) ++at;
    return at;
}
const unsigned char* __cdecl StubCaseRun(const unsigned char* at) {
    Record(11, Address(at));
    return NextMarker(at);
}
const unsigned char* __cdecl StubCaseSkip(const unsigned char* at) {
    Record(12, Address(at));
    return NextMarker(at);
}
// The conditions: numbered 0x20 + the table index + 128. The table is 512
// long so that an index the switch read unsigned still lands on a stand-in.
// They may move the position
// (none of the reached ones does; the callers read it back), and answer in al
// with the rest of eax full of other bits, as Capcom's do.
unsigned CondBody(unsigned index, const unsigned char** at) {
    Record(0x20 + index, Address(*at), **at);
    const std::uint32_t h = Hash(2);
    if (h % 4 == 0) *at += 1 + (h >> 8) % 2;
    const std::uint32_t junk = Hash(3) & 0xFFFFFF00u;
    return junk | ((h >> 12) % 2 ? 0u : 1u + (h >> 16) % 0xFF);
}
template <std::size_t N>
unsigned __cdecl StubCondition(const unsigned char** at) { return CondBody(N, at); }
template <std::size_t... I>
constexpr std::array<event_script::Condition, sizeof...(I)> MakeConditions(std::index_sequence<I...>) {
    return {{&StubCondition<I>...}};
}
const std::array<event_script::Condition, 512> kConditions = MakeConditions(std::make_index_sequence<512>());

// EventScript_Op's handlers, numbered 0x120 + the nibble whose jump-table
// slot calls them (0 for EventOp_0x). Now and then one rewrites the op's first
// byte, which EventScript_Op reads again for the length.
void Handler(unsigned n, const unsigned char* at) {
    Record(0x120 + n, Address(at), *at);
    const std::uint32_t h = Hash(4);
    if (h % 5 == 0) *const_cast<unsigned char*>(at) = static_cast<unsigned char>(h >> 8);
}
template <unsigned N>
void __cdecl StubHandler(const unsigned char* at) { Handler(N, at); }

// --- the placements' stand-ins -------------------------------------------------------------

// What a callee might move that the placements read again after it: the
// count, and Sprite_Current (to another of the fuzz's objects).
// A moved Sprite_Current's +0x54 is a readable entry, as EventOp_Bx reads
// byte 2 through it.
void MoveCurrent(std::uint32_t h) {
    Sprite_Current = Object(static_cast<int>(h % 30));
    unsigned char* const e = g_entries + (h >> 5) % 30 * 8;
    std::memcpy(Sprite_Current + 0x54, &e, sizeof e);
}
void Disturb(unsigned salt) {
    const std::uint32_t h = Hash(salt);
    if (h % 7 == 0) SetCount(PickCount(h >> 4));
    if (h % 11 == 0) MoveCurrent(h >> 8);
}
void __cdecl StubReset() {
    Record(0x130, Address(Sprite_Current), Address(Field_ActiveMember));
    Disturb(10);
}
// Sprite_SetAnimationBank: the low 16 bits are what the original pushes as a
// word (its register's other bits are whatever they were).
unsigned char __cdecl StubSetBank(unsigned short bank) {   // the ops ignore the result
    Record(0x131, bank);
    Disturb(11);
    return 0;
}
long __cdecl StubElevation(long x, long z) {
    Record(0x132, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(z));
    Disturb(12);
    return static_cast<long>(Hash(13));
}
void __cdecl StubSetFlags(const unsigned char* flags) {
    Record(0x133, Address(flags), *flags);
    Disturb(14);
}
void __cdecl StubFace() {
    Record(0x134, Address(Sprite_Current));
    Disturb(15);
}
// PartyRecord_Clear and Kind2_Place use the low byte of the index they are
// pushed (the rest is register garbage in the original's pushes).
void __cdecl StubClearRecord(unsigned index) { Record(0x135, index & 0xFF); }
// Sprite_InitFromEntry: its side effect the caller reads - Sprite_Current
// +0x54, from which EventOp_Bx takes byte 2 - is set to a readable entry.
void __cdecl StubInitEntry(unsigned char* entry) {
    Record(0x136, Address(entry));
    const std::uint32_t h = Hash(16);
    unsigned char* const e = g_entries + (h % 30) * 8;
    std::memcpy(Sprite_Current + 0x54, &e, sizeof e);
    if (h % 9 == 0) MoveCurrent(h >> 8);
}
void __cdecl StubKind2Place(unsigned char arg) { Record(0x137, arg); }
int g_run_count;   // what the run stand-in left as the count, for the coverage line
void __cdecl StubRun(const unsigned char* script) {
    Record(0x138, Address(script));
    const std::uint32_t h = Hash(17);
    g_run_count = h % 4 == 0 ? static_cast<int>(30 + (h >> 4) % 3) : PickCount(h >> 4);
    SetCount(g_run_count);
    SetWord(At(bof3::addr::DamageScratch) + 2, h >> 16);
}
unsigned char __cdecl StubFlagsTest(const unsigned char* bits, unsigned index) {
    Record(0x139, Address(bits), index & 0xFF);
    return static_cast<unsigned char>(Hash(18));
}

const void* StubFor(std::uint32_t target) {
    const auto f = [](auto p) { return reinterpret_cast<const void*>(p); };
    switch (target) {
    case 0x5797C0: return f(&StubRun);
    case 0x579F30: return f(&StubOp);
    case 0x579800: return f(&StubControl);
    case 0x579890: return f(&StubIf);
    case 0x579950: return f(&StubIfNot);
    case 0x579B00: return f(&StubSwitch);
    case 0x579A10: return f(&StubRunStep);
    case 0x579A50: return f(&StubSkipStep);
    case 0x579AA0: return f(&StubSkipControl);
    case 0x579CA0: return f(&StubSkipIf);
    case 0x579CF0: return f(&StubSkipSwitch);
    case 0x579BA0: return f(&StubCaseRun);
    case 0x579C20: return f(&StubCaseSkip);
    case 0x57A010: return f(&StubHandler<0>);
    case 0x57A1E0: return f(&StubHandler<1>);
    case 0x57A5E0: return f(&StubHandler<2>);
    case 0x57A7C0: return f(&StubHandler<3>);
    case 0x57A990: return f(&StubHandler<4>);
    case 0x57B310: return f(&StubHandler<5>);
    case 0x57AD10: return f(&StubHandler<6>);
    case 0x57AB50: return f(&StubHandler<7>);
    case 0x57A3A0: return f(&StubHandler<8>);
    case 0x57B530: return f(&StubHandler<9>);
    case 0x57AEC0: return f(&StubHandler<10>);
    case 0x57B130: return f(&StubHandler<11>);
    case 0x57B4E0: return f(&StubHandler<12>);
    case 0x57B500: return f(&StubHandler<13>);
    case 0x5898D0: return f(&StubHandler<14>);
    case 0x579E30: return f(&StubReset);
    case 0x589590: return f(&StubSetBank);
    case 0x5720C0: return f(&StubElevation);
    case 0x579DB0: return f(&StubSetFlags);
    case 0x579D70: return f(&StubFace);
    case 0x57B100: return f(&StubClearRecord);
    case 0x57BA60: return f(&StubInitEntry);
    case 0x5734F0: return f(&StubKind2Place);
    case 0x57C140: return f(&StubFlagsTest);
    default: bof3::Fatal("event_script: no stand-in for a call to 0x%X", (unsigned)target); return nullptr;
    }
}

const Callees kStubs = {
    StubOp, StubControl, StubIf, StubIfNot, StubSwitch, StubRunStep, StubSkipStep, StubSkipControl, StubSkipIf,
    StubSkipSwitch, StubCaseRun, StubCaseSkip, kConditions.data() + 128,
    {StubHandler<0>, StubHandler<1>, StubHandler<2>, StubHandler<3>, StubHandler<4>, StubHandler<5>, StubHandler<6>,
     StubHandler<7>, StubHandler<8>, StubHandler<9>, StubHandler<10>, StubHandler<11>, StubHandler<12>,
     StubHandler<13>, StubHandler<14>, StubHandler<0>},
    StubReset, StubSetBank, StubElevation, StubSetFlags, StubFace, StubClearRecord, StubInitEntry, StubKind2Place,
    StubRun, StubFlagsTest,
};

// --- the copies ------------------------------------------------------------------------------

// Every function's range - its last instruction, and the jump table after it
// where it has one - and every call out, by capstone 2026-09-22; no jump
// leaves any of them. `table` / `disp`: a jump table's offset and entries and
// the offset of its `jmp [reg*4 + table]` operand. `cond`: the offset of the
// operand 0x663B30 in the call (or load) through EventScript_Conditions.
struct Call { std::uint32_t offset, target; };
struct Clone {
    const char* name;
    std::uint32_t base, size;
    Call calls[15];   // to the first empty entry
    std::uint32_t table, entries, disp, cond;
};
enum Fn {
    kPlacement, kRun, kControl, kIf, kIfNot, kRunStep, kSkipStep, kSkipControl, kSwitch, kCaseRun, kCaseSkip,
    kSetFlags, kReset, kOp, kOp1, kOp2, kClearRecord, kOpB, kOpC, kInitEntry, kFlagsSet, kFlagsClear, kFlagsTest,
    kCondFA, kCondFlag, kCondFD, kSet40, kSetBit40, kFns
};
const Clone kClones[kFns] = {
    {"Area_RunPlacement", 0x579740, 0x78, {{0x32, 0x5797C0}}, 0, 0, 0, 0},
    {"EventScript_Run", 0x5797C0, 0x36, {{0x1F, 0x579F30}, {0x26, 0x579800}}, 0, 0, 0, 0},
    {"EventScript_Control", 0x579800, 0x87, {{0x24, 0x579890}, {0x2F, 0x579950}, {0x3A, 0x579B00}}, 0x64, 6, 0x1E, 0},
    {"EventScript_If", 0x579890, 0xBB, {{0x61, 0x579A10}, {0x6B, 0x579A50}, {0xA4, 0x579A10}, {0xAE, 0x579A50}}, 0, 0, 0, 0x21},
    {"EventScript_IfNot", 0x579950, 0xBB, {{0x61, 0x579A10}, {0x6B, 0x579A50}, {0xA4, 0x579A10}, {0xAE, 0x579A50}}, 0, 0, 0, 0x21},
    {"EventScript_IfRunStep", 0x579A10, 0x3E, {{0x11, 0x579800}, {0x35, 0x579F30}}, 0, 0, 0, 0},
    {"EventScript_IfSkipStep", 0x579A50, 0x48, {{0x11, 0x579AA0}}, 0, 0, 0, 0},
    {"EventScript_SkipControl", 0x579AA0, 0x5B, {{0x23, 0x579CA0}, {0x2D, 0x579CF0}}, 0x3C, 5, 0x1E, 0},
    {"EventScript_Switch", 0x579B00, 0x9C, {{0x4A, 0x579BA0}, {0x60, 0x579C20}, {0x7B, 0x579BA0}, {0x8B, 0x579C20}}, 0, 0, 0, 0x35},
    {"EventScript_CaseRun", 0x579BA0, 0x74, {{0x27, 0x579800}, {0x39, 0x579F30}}, 0x48, 11, 0x22, 0},
    {"EventScript_CaseSkip", 0x579C20, 0x7C, {{0x27, 0x579AA0}}, 0x50, 11, 0x22, 0},
    {"EventObj_SetFlags", 0x579DB0, 0x7A, {}, 0, 0, 0, 0},
    {"EventObj_Reset", 0x579E30, 0xC5, {}, 0, 0, 0, 0},
    {"EventScript_Op", 0x579F30, 0xE0,
     {{0x1A, 0x5898D0}, {0x22, 0x57B500}, {0x2A, 0x57B4E0}, {0x32, 0x57B310}, {0x3A, 0x57B130}, {0x42, 0x57AEC0},
      {0x4A, 0x57AD10}, {0x52, 0x57AB50}, {0x5A, 0x57B530}, {0x62, 0x57A990}, {0x6A, 0x57A7C0}, {0x72, 0x57A5E0},
      {0x7A, 0x57A3A0}, {0x82, 0x57A1E0}, {0x8A, 0x57A010}},
     0xA8, 14, 0x15, 0},
    {"EventOp_1x", 0x57A1E0, 0x1BA, {{0x3F, 0x579E30}, {0x4B, 0x589590}, {0xDE, 0x5720C0}, {0x16B, 0x579DB0}, {0x1AB, 0x579D70}}, 0, 0, 0, 0},
    {"EventOp_2x", 0x57A5E0, 0x1DE, {{0x3F, 0x579E30}, {0x4B, 0x589590}, {0xDE, 0x5720C0}, {0x18F, 0x579DB0}, {0x1CF, 0x579D70}}, 0, 0, 0, 0},
    {"PartyRecord_Clear", 0x57B100, 0x27, {}, 0, 0, 0, 0},
    {"EventOp_Bx", 0x57B130, 0x1DC, {{0x57, 0x579E30}, {0x60, 0x57B100}, {0x66, 0x57BA60}, {0xB8, 0x5720C0}, {0x1AA, 0x579DB0}}, 0, 0, 0, 0},
    {"EventOp_Cx", 0x57B4E0, 0x1A, {{0x13, 0x5734F0}}, 0, 0, 0, 0},
    {"Sprite_InitFromEntry", 0x57BA60, 0x78, {}, 0, 0, 0, 0},
    {"Flags_Set", 0x57C0F0, 0x1F, {}, 0, 0, 0, 0},
    {"Flags_Clear", 0x57C110, 0x25, {}, 0, 0, 0, 0},
    {"Flags_Test", 0x57C140, 0x20, {}, 0, 0, 0, 0},
    {"EventCond_ByteFA", 0x57C180, 0x12, {}, 0, 0, 0, 0},
    {"EventCond_Flag", 0x57C1C0, 0x2E, {{0x25, 0x57C140}}, 0, 0, 0, 0},
    {"EventCond_ByteFD", 0x57C210, 0x12, {}, 0, 0, 0, 0},
    {"ScriptFlags_Set40", 0x57C7C0, 0x14, {}, 0, 0, 0, 0},
    {"ObjTrio_SetBit40", 0x57C810, 0x2D, {}, 0, 0, 0, 0},
};

void* g_theirs[kFns];

void PatchDword(unsigned char* code, std::uint32_t at, std::uint32_t expected, std::uint32_t value, const char* what,
                const Clone& c) {
    std::uint32_t v;
    std::memcpy(&v, code + at, sizeof v);
    if (v != expected) bof3::Fatal("event_script: %s's %s at +0x%X is 0x%X, not 0x%X", c.name, what, (unsigned)at, (unsigned)v, (unsigned)expected);
    std::memcpy(code + at, &value, sizeof value);
}

void MakeClones() {
    for (unsigned k = 0; k < kFns; ++k) {
        const Clone& c = kClones[k];
        bof3::CloneCall calls[16];
        int n = 0;
        for (const Call& call : c.calls)
            if (call.target) calls[n++] = {call.offset, StubFor(call.target)};
        void* const copy = bof3::CloneOriginal(c.name, c.base, c.size, calls, n);
        auto* code = static_cast<unsigned char*>(copy);
        const std::uint32_t moved = Address(code) - c.base;
        if (c.entries) {   // a jump table holds absolute addresses into the original
            for (std::uint32_t i = 0; i < c.entries; ++i) {
                std::uint32_t target;
                std::memcpy(&target, code + c.table + 4 * i, sizeof target);
                if (target < c.base || target >= c.base + c.size)
                    bof3::Fatal("event_script: %s's jump table entry %u is 0x%X", c.name, (unsigned)i, (unsigned)target);
                PatchDword(code, c.table + 4 * i, target, target + moved, "jump table entry", c);
            }
            PatchDword(code, c.disp, c.base + c.table, c.base + c.table + moved, "jump table operand", c);
        }
        if (c.cond) PatchDword(code, c.cond, Address(EventScript_Conditions), Address(kStubs.conditions), "condition table operand", c);
        FlushInstructionCache(GetCurrentProcess(), copy, c.size);
        g_theirs[k] = copy;
    }
}
template <class F>
F Theirs(Fn k) { return reinterpret_cast<F>(g_theirs[k]); }

// --- the state and one round ----------------------------------------------------------------

struct Region { unsigned char* at; unsigned size; };
template <class T> Region R(T& v) { return {reinterpret_cast<unsigned char*>(&v), sizeof v}; }
Region R(void* at, unsigned size) { return {static_cast<unsigned char*>(at), size}; }

constexpr unsigned kStateMax = 0x3000;
unsigned char g_saved[kStateMax], g_input[kStateMax], g_out[2][kStateMax];
Log g_logs[2];

unsigned Total(const Region* r, unsigned n) {
    unsigned total = 0;
    for (unsigned i = 0; i < n; ++i) total += r[i].size;
    if (total > kStateMax) bof3::Fatal("event_script self-test: state of 0x%X bytes", total);
    return total;
}
void Capture(const Region* r, unsigned n, unsigned char* out) {
    for (unsigned i = 0; i < n; out += r[i].size, ++i) std::memcpy(out, r[i].at, r[i].size);
}
void Apply(const Region* r, unsigned n, const unsigned char* in) {
    for (unsigned i = 0; i < n; in += r[i].size, ++i) std::memcpy(r[i].at, in, r[i].size);
}
void Randomize(const Region* r, unsigned n) {
    for (unsigned i = 0; i < n; ++i)
        for (unsigned k = 0; k < r[i].size; ++k) r[i].at[k] = static_cast<unsigned char>(Next());
}

// Runs `theirs` then `ours` from the state as it stands; true if the state,
// the log or the result differ.
template <class Theirs, class Ours>
bool Pair(const char* name, unsigned round, const Region* r, unsigned n, Theirs&& theirs, Ours&& ours, unsigned& bad) {
    const unsigned total = Total(r, n);
    Capture(r, n, g_input);
    g_seed = Next();
    for (int pass = 0; pass < 2; ++pass) {
        Apply(r, n, g_input);
        std::memset(&g_log, 0, sizeof g_log);
        if (pass == 0) theirs();
        else ours();
        Capture(r, n, g_out[pass]);
        g_logs[pass] = g_log;
    }
    const bool log_differs = g_logs[0].n != g_logs[1].n || g_logs[0].hash != g_logs[1].hash;
    const bool state_differs = std::memcmp(g_out[0], g_out[1], total) != 0;
    if (!log_differs && !state_differs) return false;
    if (++bad <= 8) {
        if (state_differs) {
            unsigned at = 0;
            while (g_out[0][at] == g_out[1][at]) ++at;
            unsigned region = 0, base = 0;
            while (at >= base + r[region].size) base += r[region++].size;
            bof3::Log("shadow      event_script %s MISMATCH: round %u, state region %u (0x%08X) +0x%X: %02X / %02X", name,
                      round, region, static_cast<unsigned>(Address(r[region].at)), at - base, g_out[0][at], g_out[1][at]);
        } else {
            unsigned at = 0;
            const unsigned kept = g_logs[0].n < kKeep ? g_logs[0].n : kKeep;
            while (at < kept && std::memcmp(g_logs[0].keep[at], g_logs[1].keep[at], 16) == 0) ++at;
            const std::uint32_t* a = at < kKeep ? g_logs[0].keep[at] : g_logs[0].keep[0];
            const std::uint32_t* b = at < kKeep ? g_logs[1].keep[at] : g_logs[1].keep[0];
            bof3::Log("shadow      event_script %s MISMATCH: round %u, log of %u / %u calls, first difference at entry "
                      "%u: %X(%X, %X, %X) / %X(%X, %X, %X)", name, round, g_logs[0].n, g_logs[1].n, at, a[0], a[1],
                      a[2], a[3], b[0], b[1], b[2], b[3]);
        }
    }
    return true;
}

unsigned g_total_bad, g_total_rounds, g_total_calls;
void Report(const char* name, unsigned rounds, unsigned bad, const char* detail) {
    bof3::Log("shadow      event_script %s self-test: %u rounds (%s), %u MISMATCHES", name, rounds, detail, bad);
    g_total_bad += bad;
    g_total_rounds += rounds;
}
unsigned Calls() { return g_logs[0].n; }

// --- script generation ------------------------------------------------------------------------

// A body of `n` bytes drawn from `allowed` (a 256-entry mask), boundary bytes
// often, then `tail` repeated to the end of the buffer.
void Script(const bool* allowed, unsigned n, unsigned char tail) {
    static const unsigned char kEdge[] = {0x00, 0x0F, 0x10, 0x1F, 0xEF, 0xF0, 0xF1, 0xF4, 0xF5, 0xF6, 0xF7,
                                          0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF, 0x80, 0x7F, 0xBF, 0xC0};
    for (unsigned i = 0; i < n; ++i) {
        unsigned char b;
        do b = OneIn(2) ? kEdge[Next() % sizeof kEdge] : static_cast<unsigned char>(Next());
        while (!allowed[b]);
        g_script[i] = b;
    }
    for (unsigned i = n; i < kScript; ++i) g_script[i] = tail;
}
struct Mask {
    bool b[256];
    explicit Mask(std::initializer_list<unsigned char> banned) {
        for (bool& v : b) v = true;
        for (unsigned char x : banned) b[x] = false;
    }
};

// --- the interpreter's tests -----------------------------------------------------------------

using StepFn = const unsigned char* (__cdecl*)(const unsigned char*);
using ArmFn = const unsigned char* (__cdecl*)(const unsigned char*, unsigned char*, unsigned char*);

void SelfTestInterpreter() {
    constexpr unsigned kRounds = 20000;
    const Region r[] = {R(g_script, kScript), R(&EventScript_FlagBank, 2), R(&Cond_ByteFA, 1), R(g_arm), R(g_result)};
    constexpr unsigned n = sizeof r / sizeof r[0];
    Capture(r, n, g_saved);
    const Mask any({});
    // What each loop hangs on without a call: the switch's case bodies and
    // skips treat F2 F3 F5 and FB..FF as nothing, and do not move.
    const Mask body({0xF2, 0xF3, 0xF5, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF, 0xF8});
    const Mask junk({0xF5, 0xF6, 0xF7});
    const char* const names[] = {"EventScript_Run", "EventScript_Control", "EventScript_If", "EventScript_IfNot",
                                 "EventScript_IfRunStep", "EventScript_IfSkipStep", "EventScript_SkipControl",
                                 "EventScript_Switch", "EventScript_CaseRun", "EventScript_CaseSkip", "EventScript_Op"};
    constexpr unsigned kTests = sizeof names / sizeof names[0];
    for (unsigned t = 0; t < kTests; ++t) {
        g_rng = 0x57970001u + t * 0x1000193u;
        unsigned bad = 0, calls = 0, seen[16] = {}, arms[2] = {}, cases[2] = {}, signed_cond = 0;
        for (unsigned round = 0; round < kRounds; ++round) {
            Randomize(r, n);
            const unsigned len = OneIn(8) ? 0 : 1 + Next() % 48;
            const unsigned char* const s = g_script;
            switch (t) {
            case 0: Script(any.b, len, 0xFF); break;
            case 1: case 4: case 5: case 6: Script(any.b, 8, static_cast<unsigned char>(Next())); break;
            case 2: case 3: {   // c x, one byte, then the arms
                Script(any.b, len + 3, 0xFE);
                static const unsigned char kC[] = {0x00, 0x02, 0x08, 0x1F, 0x20, 0x22, 0xFF, 0xE0};
                g_script[0] = OneIn(2) ? kC[Next() % 8] : static_cast<unsigned char>(Next());
                break;
            }
            case 7: {   // c, then cases
                unsigned at = 0;
                g_script[at++] = static_cast<unsigned char>(OneIn(2) ? (OneIn(2) ? 0x80 + Next() % 3 : 0x7D + Next() % 3) : Next());
                const unsigned cases = Next() % 7;
                for (unsigned k = 0; k < cases; ++k) {
                    if (OneIn(5)) g_script[at++] = 0xF7;
                    else { g_script[at++] = 0xF6; g_script[at++] = static_cast<unsigned char>(Next()); }
                    for (unsigned j = Next() % 6; j; --j) {
                        unsigned char b;
                        do b = static_cast<unsigned char>(Next()); while (!junk.b[b]);
                        g_script[at++] = b;
                    }
                }
                for (unsigned i = at; i < kScript; ++i) g_script[i] = 0xF5;
                break;
            }
            case 8: case 9: Script(body.b, len, OneIn(4) && t == 9 ? 0xF6 : 0xF8); break;
            case 10: Script(any.b, 4, static_cast<unsigned char>(Next())); break;
            }
            // Where the byte under test is chosen, seed every control byte and nibble.
            if (t == 1 || t == 4 || t == 5 || t == 6) g_script[0] = static_cast<unsigned char>(OneIn(2) ? 0xEE + Next() % 18 : Next());
            if (t == 10) g_script[0] = static_cast<unsigned char>((round % 16) << 4 | (Next() & 0xF));
            g_arm[0] = static_cast<unsigned char>(OneIn(2) ? 0 : Next());
            g_arm[1] = static_cast<unsigned char>(OneIn(2) ? 1 + Next() % 3 : Next());
            if (OneIn(3)) Cond_ByteFA = static_cast<signed char>(OneIn(2) ? -1 : 0x7F);
            auto run = [&](auto fn) -> std::uint32_t {
                switch (t) {
                case 0: reinterpret_cast<void (__cdecl*)(const unsigned char*)>(fn)(s); return 0;
                case 4: case 5: return Address(reinterpret_cast<ArmFn>(fn)(s, &g_arm[0], &g_arm[1]));
                default: return Address(reinterpret_cast<StepFn>(fn)(s));
                }
            };
            static const Fn kFn[] = {kRun, kControl, kIf, kIfNot, kRunStep, kSkipStep, kSkipControl, kSwitch, kCaseRun, kCaseSkip, kOp};
            static void* const kOurs[] = {
                reinterpret_cast<void*>(&EventScript_Run), reinterpret_cast<void*>(&EventScript_Control),
                reinterpret_cast<void*>(&EventScript_If), reinterpret_cast<void*>(&EventScript_IfNot),
                reinterpret_cast<void*>(&EventScript_IfRunStep), reinterpret_cast<void*>(&EventScript_IfSkipStep),
                reinterpret_cast<void*>(&EventScript_SkipControl), reinterpret_cast<void*>(&EventScript_Switch),
                reinterpret_cast<void*>(&EventScript_CaseRun), reinterpret_cast<void*>(&EventScript_CaseSkip),
                reinterpret_cast<void*>(&EventScript_Op)};
            Pair(names[t], round, r, n, [&] { g_result = run(g_theirs[kFn[t]]); }, [&] { g_result = run(kOurs[t]); }, bad);
            calls += Calls();
            ++seen[g_script[0] >> 4];
            arms[0] += g_logs[0].counts[6];
            arms[1] += g_logs[0].counts[7];
            cases[0] += g_logs[0].counts[11];
            cases[1] += g_logs[0].counts[12];
            for (unsigned i = 0x20; i < 0x20 + 128; ++i) signed_cond += g_logs[0].counts[i];
        }
        Apply(r, n, g_saved);
        unsigned least = seen[0];
        for (const unsigned v : seen) least = v < least ? v : least;
        char detail[220];
        if (t == 2 || t == 3)
            std::snprintf(detail, sizeof detail,
                          "%u calls to the stand-ins; the condition's nibble %u..%u times each, %u arm steps run and "
                          "%u skipped", calls, least, kRounds, arms[0], arms[1]);
        else if (t == 7)
            std::snprintf(detail, sizeof detail,
                          "%u calls to the stand-ins; %u case bodies run and %u skipped, %u rounds whose condition "
                          "index was negative", calls, cases[0], cases[1], signed_cond);
        else
            std::snprintf(detail, sizeof detail, "%u calls to the stand-ins; the first byte's high nibble %u..%u times each",
                          calls, least, kRounds);
        g_total_calls += calls;
        Report(names[t], kRounds, bad, detail);
    }
}

// --- the placements -------------------------------------------------------------------------

// Field_MoveSpeeds' indices that do not divide by zero: EventObj_SetFlags'
// idiv faults on the others, as the original's does.
unsigned char g_speeds[256];
unsigned g_n_speeds;
unsigned char PickSpeed() {
    static const unsigned char kSeed[] = {0, 1, 2, 3, 4, 5};
    return OneIn(2) ? kSeed[Next() % 6] : g_speeds[Next() % g_n_speeds];
}

void SeedObject(unsigned char* o) {
    o[0x84] = PickSpeed();
    unsigned char* const e = g_entries + (Next() % 30) * 8;   // +0x54, which EventOp_Bx reads byte 2 through
    std::memcpy(o + 0x54, &e, sizeof e);
}

void SelfTestPlacements() {
    constexpr unsigned kRounds = 20000;
    unsigned char* const objects = Object(kFirstObject);
    const Region r[] = {R(objects, kObjects * 0xA4), R(Sprite_ObjectsExtra, 8 * 0xA4), R(MoveScript_PartyRecords, 0x1000),
                        R(At(bof3::addr::DamageScratch), 4), R(&Sprite_Current, 4), R(&Field_ActiveMember, 4),
                        R(Sprite_Kind2, Sprite_Kind2_count), R(&Game_AreaNumber, 2), R(g_script, 0x40),
                        R(g_entries), R(g_result)};
    constexpr unsigned n = sizeof r / sizeof r[0];
    Capture(r, n, g_saved);
    const Fn kFnsHere[] = {kPlacement, kOp1, kOp2, kOpB, kOpC, kReset, kSetFlags, kInitEntry, kClearRecord};
    for (const Fn fn : kFnsHere) {
        g_rng = 0x57A1E001u + fn * 0x9E3779B9u;
        unsigned bad = 0, calls = 0, placed = 0, full = 0, divided = 0, cleared = 0, before = 0;
        for (unsigned round = 0; round < kRounds; ++round) {
            Randomize(r, n);
            for (int i = kFirstObject; i < kFirstObject + kObjects; ++i) SeedObject(Object(i));
            for (unsigned i = 0; i < 8; ++i) SeedObject(Sprite_ObjectsExtra + i * 0xA4);
            static const int kCount[] = {29, 30, 31, 0, 1, 0x7FFF, 100};
            SetCount(OneIn(3) ? kCount[Next() % 7] : static_cast<int>(Next() % 32));
            Game_AreaNumber = static_cast<unsigned short>(Next() % Area_Descriptors_count);
            // The op: the placements' own bytes, with the speed index, the
            // extra object's number and the flags byte's bits seeded.
            g_script[0] = static_cast<unsigned char>((fn == kOpB ? 0xB0 : fn == kOp2 ? 0x20 : fn == kOpC ? 0xC0 : 0x10) |
                                                     (OneIn(3) ? (OneIn(2) ? 7 : 8) : Next() & 0xF));
            g_script[0xB] = fn == kOpB ? static_cast<unsigned char>(Next() % 8) : PickSpeed();
            if (fn == kOpB) g_script[6] = PickSpeed();
            static const unsigned char kFlags[] = {0x00, 0x01, 0x04, 0x40, 0x45, 0xFF, 0xBA};
            g_script[fn == kOpB ? 8 : 0xD] = OneIn(2) ? kFlags[Next() % 7] : static_cast<unsigned char>(Next());
            for (const unsigned at : {1u, 3u, 6u, 8u}) if (OneIn(3)) g_script[at] = 0;
            if (OneIn(2)) g_script[3] = static_cast<unsigned char>(Next() & 0x7F);
            // Sprite_Current and Field_ActiveMember: an object of the fuzz's,
            // the same one, or a few bytes apart (the order of the stores).
            Sprite_Current = Object(static_cast<int>(Next() % 30));
            static const int kApart[] = {0, 0, 0, 1, -1, 4, -4, 8, -8, 2};
            Field_ActiveMember = OneIn(3) ? Object(static_cast<int>(Next() % 30)) : Sprite_Current + kApart[Next() % 10];
            Field_ActiveMember[0x84] = PickSpeed();   // the divisor's index: never one that faults
            const unsigned char* const op = g_script;
            unsigned char* flags = g_script + 0xD;
            // Where EventObj_SetFlags' own stores can change the byte it
            // reads again: its +7 (the same value), and +0, +0x48 and the
            // context's +0x80, which it changes between the reads.
            if (fn == kSetFlags && OneIn(2)) {
                unsigned char* const kAlias[] = {Sprite_Current + 7, Sprite_Current, Sprite_Current + 0x48,
                                                 Field_ActiveMember + 0x80};
                flags = kAlias[Next() % 4];
            }
            unsigned char* entry = g_entries + (Next() % 30) * 8;
            if (fn == kInitEntry && OneIn(4)) entry = Sprite_Current + (OneIn(2) ? 0 : 0x40);   // read after the stores
            if (fn == kInitEntry && OneIn(3)) Sprite_Current[8] = static_cast<unsigned char>(OneIn(2) ? Next() % 8 : Next());
            const unsigned index = (Next() & 0xFFFFFF00u) | (OneIn(2) ? Next() % 8 : Next() & 0xFF);
            const bool room = Count() < 30;
            const unsigned char speed = fn == kSetFlags ? Field_ActiveMember[0x84] : 0;
            switch (fn) {
            case kPlacement:
                Pair("Area_RunPlacement", round, r, n, [&] { Theirs<void (__cdecl*)(const unsigned char*)>(fn)(op); },
                     [&] { Area_RunPlacement(op); }, bad);
                break;
            case kOp1:
                Pair("EventOp_1x", round, r, n, [&] { Theirs<void (__cdecl*)(const unsigned char*)>(fn)(op); }, [&] { EventOp_1x(op); }, bad);
                break;
            case kOp2:
                Pair("EventOp_2x", round, r, n, [&] { Theirs<void (__cdecl*)(const unsigned char*)>(fn)(op); }, [&] { EventOp_2x(op); }, bad);
                break;
            case kOpB:
                Pair("EventOp_Bx", round, r, n, [&] { Theirs<void (__cdecl*)(const unsigned char*)>(fn)(op); }, [&] { EventOp_Bx(op); }, bad);
                break;
            case kOpC:
                Pair("EventOp_Cx", round, r, n, [&] { Theirs<void (__cdecl*)(const unsigned char*)>(fn)(op); }, [&] { EventOp_Cx(op); }, bad);
                break;
            case kReset:
                Pair("EventObj_Reset", round, r, n, [&] { Theirs<void (__cdecl*)()>(fn)(); }, [&] { EventObj_Reset(); }, bad);
                break;
            case kSetFlags:
                Pair("EventObj_SetFlags", round, r, n, [&] { Theirs<void (__cdecl*)(const unsigned char*)>(fn)(flags); },
                     [&] { EventObj_SetFlags(flags); }, bad);
                break;
            case kInitEntry:
                Pair("Sprite_InitFromEntry", round, r, n, [&] { Theirs<void (__cdecl*)(unsigned char*)>(fn)(entry); },
                     [&] { Sprite_InitFromEntry(entry); }, bad);
                break;
            case kClearRecord:
                Pair("PartyRecord_Clear", round, r, n, [&] { Theirs<void (__cdecl*)(unsigned)>(fn)(index); },
                     [&] { PartyRecord_Clear(index); }, bad);
                break;
            default: break;
            }
            calls += Calls();
            placed += room && g_logs[0].counts[0x130] != 0;
            full += !room;
            divided += speed != 0;
            cleared += fn == kPlacement && g_run_count < 30;
            before += fn == kPlacement && g_run_count < 0;
        }
        Apply(r, n, g_saved);
        char detail[240];
        if (fn == kPlacement)
            std::snprintf(detail, sizeof detail,
                          "%u calls to the stand-ins; the script left the count below 30 in %u rounds (%u of them "
                          "negative, clearing objects before the array)", calls, cleared, before);
        else if (fn == kSetFlags)
            std::snprintf(detail, sizeof detail, "%u rounds with a non-zero speed index (a division)", divided);
        else if (fn == kOp1 || fn == kOp2 || fn == kOpB)
            std::snprintf(detail, sizeof detail, "%u calls to the stand-ins; %u placed an object, %u found the count at 30 or more",
                          calls, placed, full);
        else
            std::snprintf(detail, sizeof detail, "%u calls to the stand-ins", calls);
        g_total_calls += calls;
        Report(kClones[fn].name, kRounds, bad, detail);
    }
}

// --- the flags --------------------------------------------------------------------------------

void SelfTestFlags() {
    constexpr unsigned kRounds = 20000;
    const Region r[] = {R(g_bits), R(g_script, 0x10), R(MoveScript_PartyRecords, 0x80), R(&EventScript_FlagBank, 2),
                        R(Cond_Flags, 0x820), R(&Field_ScriptFlags, 2), R(ObjTrio, 0x299), R(g_result)};
    constexpr unsigned n = sizeof r / sizeof r[0];
    Capture(r, n, g_saved);
    const Fn kFnsHere[] = {kFlagsSet, kFlagsClear, kFlagsTest, kCondFA, kCondFlag, kCondFD, kSet40, kSetBit40};
    for (const Fn fn : kFnsHere) {
        g_rng = 0x57C0F001u + fn * 0x9E3779B9u;
        unsigned bad = 0, set = 0;
        for (unsigned round = 0; round < kRounds; ++round) {
            Randomize(r, n);
            static const unsigned kIndex[] = {0, 7, 8, 0xFF, 0x100, 0x1FF, 0xF8, 0x80};
            const unsigned index = OneIn(3) ? kIndex[Next() % 8] : Next();
            if (OneIn(2)) g_script[0] = static_cast<unsigned char>(Cond_ByteFA);
            if (OneIn(2)) Cond_ByteFD = g_script[0];
            const unsigned char* position = g_script;
            if (OneIn(3)) EventScript_FlagBank = static_cast<unsigned short>(Next() % 4);
            switch (fn) {
            case kFlagsSet:
                Pair("Flags_Set", round, r, n, [&] { Theirs<void (__cdecl*)(unsigned char*, unsigned)>(fn)(g_bits, index); },
                     [&] { Flags_Set(g_bits, index); }, bad);
                break;
            case kFlagsClear:
                Pair("Flags_Clear", round, r, n, [&] { Theirs<void (__cdecl*)(unsigned char*, unsigned)>(fn)(g_bits, index); },
                     [&] { Flags_Clear(g_bits, index); }, bad);
                break;
            case kFlagsTest:
                // All of eax: the original's is exactly 0 or 1, and some of its
                // callers read it whole (and eax, 0xFF).
                Pair("Flags_Test", round, r, n,
                     [&] { g_result = Theirs<std::uint32_t (__cdecl*)(const unsigned char*, unsigned)>(fn)(g_bits, index); },
                     [&] { g_result = reinterpret_cast<std::uint32_t (__cdecl*)(const unsigned char*, unsigned)>(reinterpret_cast<void*>(&Flags_Test))(g_bits, index); }, bad);
                set += g_out[0][sizeof g_bits + 0x10 + 0x80 + 2 + 0x820 + 2 + 0x299] != 0;
                break;
            case kCondFA:
            case kCondFlag:
            case kCondFD: {
                using Cond = std::uint32_t (__cdecl*)(const unsigned char**);
                void* const ours = fn == kCondFA ? reinterpret_cast<void*>(&EventCond_ByteFA)
                                 : fn == kCondFlag ? reinterpret_cast<void*>(&EventCond_Flag) : reinterpret_cast<void*>(&EventCond_ByteFD);
                // al is the answer; the rest of the original's eax is the
                // position's bytes (or Flags_Test's), which no caller reads.
                Pair(kClones[fn].name, round, r, n,
                     [&] { const unsigned char* p = position; g_result = Theirs<Cond>(fn)(&p) & 0xFF; g_result |= Address(p) != Address(position) ? 0x100 : 0; },
                     [&] { const unsigned char* p = position; g_result = reinterpret_cast<Cond>(ours)(&p) & 0xFF; g_result |= Address(p) != Address(position) ? 0x100 : 0; }, bad);
                set += (g_out[0][sizeof g_bits + 0x10 + 0x80 + 2 + 0x820 + 2 + 0x299] & 1) != 0;
                break;
            }
            case kSet40:
                Pair("ScriptFlags_Set40", round, r, n, [&] { Theirs<void (__cdecl*)()>(fn)(); }, [&] { ScriptFlags_Set40(); }, bad);
                break;
            case kSetBit40:
                Pair("ObjTrio_SetBit40", round, r, n, [&] { Theirs<void (__cdecl*)()>(fn)(); }, [&] { ObjTrio_SetBit40(); }, bad);
                break;
            default: break;
            }
        }
        Apply(r, n, g_saved);
        char detail[120];
        if (fn == kFlagsTest || fn == kCondFA || fn == kCondFlag || fn == kCondFD)
            std::snprintf(detail, sizeof detail, "%u answered 1", set);
        else
            std::snprintf(detail, sizeof detail, "every bit of the index seeded");
        Report(kClones[fn].name, kRounds, bad, detail);
    }
}

}  // namespace

void SelfTest() {
    for (unsigned i = 0; i < 256; ++i)
        if (i == 0 || Field_MoveSpeeds[i] != 0) g_speeds[g_n_speeds++] = static_cast<unsigned char>(i);
    MakeClones();
    g = kStubs;
    SelfTestInterpreter();
    SelfTestPlacements();
    SelfTestFlags();
    g = kOriginals;
    bof3::Log("shadow      event_script self-test: %u functions, %u rounds, %u calls to the stand-ins, %u MISMATCHES",
              static_cast<unsigned>(kFns), g_total_rounds, g_total_calls, g_total_bad);
    if (g_total_bad) bof3::Fatal("the event script differs from the original in %u self-test rounds", g_total_bad);
}

}  // namespace event_script

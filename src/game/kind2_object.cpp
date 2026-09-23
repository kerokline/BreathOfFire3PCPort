// The kind-2 object's script runner, once a field frame: originals 0x573080
// (Kind2_Run), 0x573090 (Kind2_Dispatch) and the five states it dispatches
// to - 0x5730A0 (Kind2_Script), 0x5732B0 (Kind2_Move), 0x573320
// (Kind2_WaitRequest), 0x573340 (Kind2_Travel), 0x5733B0 (Kind2_Lift).
// docs/kind2-object.md.
//
// "k +n" in the comments is byte n of Sprite_Kind2. Its script context is
// k +0x80: +0x80 the context flags, +0x81 a wait in frames, +0x83 which of the
// area's kind-2 scripts, +0x84 the speed index, +0x87 a timed move's steps.
// Its own bytes: +1 the run byte (0 idle, 1 running), +2 the state, +8 the
// direction the script returned, +9 a count of frames for state 4, the long
// +0x14 a per-frame elevation step, +0x34 / +0x38 x and z, and the long +0x3C
// the elevation in 16.16 - whose high word +0x3E is what MapView_SetElevation
// is given.
#include "game/kind2_object.h"

#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

using namespace move_script;

std::uint32_t Address(const volatile void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }

using Handler = void (__cdecl*)();

// Every call out, through pointers so that the start-up fuzz can stand
// recording functions in for them - for the original's copies and for ours
// alike. The two tables are the original's own, in BOF3.exe's .data: the
// dispatchers index them with a whole byte, as the original does, so a run
// byte or a state out of range reaches exactly what it reached in 2001 (see
// Kind2_Run). Their entries are the original addresses, which Inject turns
// into jumps to ours; entry 0 of the run table, 0x437CC0, is Capcom's bare ret.
struct Callees {
    const Handler* run_table;
    const Handler* state_table;
    unsigned char (__cdecl* step)(unsigned char*, const unsigned char*);
    long (__cdecl* elevation)(long, long);
    void (__cdecl* set_elevation)(int);
    void (__cdecl* view_reset)();
    void (__cdecl* script)();   // Kind2_Move's tail jump
};
const Callees kOriginals = {
    reinterpret_cast<const Handler*>(Kind2_RunTable),
    reinterpret_cast<const Handler*>(Kind2_StateTable),
    MoveScript_Step, AreaMap_Elevation, MapView_SetElevation, Field_ViewReset, Kind2_Script,
};
Callees g = kOriginals;

// `cdq` / `idiv`: the original's signed division, faults included - a zero
// divisor, or INT_MIN by -1, raises the same #DE at the same point of the
// same computation, which a C++ `/` does not promise (it is undefined there).
std::int32_t Idiv(std::int32_t dividend, std::int32_t divisor) {
    std::int32_t quotient;
    __asm__ volatile("cltd\n\tidivl %2" : "=a"(quotient) : "a"(dividend), "r"(divisor) : "edx", "cc");
    return quotient;
}

// `cdq` / `xor` / `sub`: an absolute value in which INT_MIN stays INT_MIN.
std::int32_t Abs32(std::uint32_t v) {
    const auto s = static_cast<std::int32_t>(v);
    return s < 0 ? static_cast<std::int32_t>(0u - v) : s;
}

}  // namespace

// original 0x573080: the kind-2 object's frame (PSX FUN_801C67EC) - a jump
// through Kind2_RunTable by the run byte k +1.
//
// As the original has it: the index is the whole byte and nothing checks it.
// The run table is two entries (0x437CC0, a bare ret, and Kind2_Dispatch) and
// Kind2_StateTable follows it directly, so a run byte of 2..6 would run the
// states 0..4 without the dispatch, 7 would jump to address 0 (the dword at
// 0x6632C8) and anything larger to what follows. The exe stores only 0 and 1
// there directly (docs/kind2-object.md section 3); op E5 copies k +3 into it
// when the kind-2 object's own script runs it.
extern "C" void __cdecl Kind2_Run(void) {
    g.run_table[Sprite_Kind2[1]]();
}

// original 0x573090: the state k +2 through Kind2_StateTable (the PSX
// 0x801C6830). As the original has it: unchecked, a whole byte - a state of 5
// jumps to address 0.
extern "C" void __cdecl Kind2_Dispatch(void) {
    g.state_table[Sprite_Kind2[2]]();
}

// original 0x5730A0, state 0: one step of the kind-2 object's movement script
// (PSX FUN_801C686C).
//
// As the original has it:
//  - Sprite_Current is left pointing at Sprite_Kind2 (the step needs it; it is
//    not put back).
//  - The glide (result 0xFF with context bit 4) takes its distance as the
//    larger of |dx| and |dz| in wrapping 32-bit arithmetic, where |INT_MIN| is
//    INT_MIN; and its frame count as the HIGH word of twice that distance,
//    signed - 16.16 whole units, so a distance of 0x40000000 or more counts
//    negative frames.
//  - The two divisions fault as the original's do (Idiv): 0x80 by
//    MoveScript_F3Divisor, which is 0 for speed index 0 (and past the table's
//    six bytes, 6 and 7); then by quotient x frames, which is 0 for a
//    distance under 0x8000 - half a unit - and for a speed above 16 (index 8
//    reads 0x40). docs/kind2-object.md section 5 has these as defects.
//  - MoveScript_F3Divisor, the elevation long and MapView_Elevation are read
//    back after AreaMap_Elevation, not kept from before it.
extern "C" void __cdecl Kind2_Script(void) {
    unsigned char* const k = Sprite_Kind2;
    if (k[0x81] != 0) {   // a wait: count it down
        --k[0x81];
        return;
    }
    const unsigned char* const descriptor = Area_Descriptors[Game_AreaNumber];
    const unsigned char* const* scripts;
    std::memcpy(&scripts, descriptor + 0x1C, sizeof scripts);
    Sprite_Current = k;
    const unsigned char result = g.step(k + 0x80, scripts[k[0x83]]);
    const unsigned char flags = k[0x80];
    k[8] = result;
    if (flags & 8) {   // the script ended: idle, at speed 3
        k[0x80] = static_cast<unsigned char>(flags & 0xF7);
        k[1] = 0;
        k[2] = 0;
        k[3] = 0;
        k[0x84] = 3;
        return;
    }
    if (result == 0xFF) {
        if (flags & 4) {   // a glide toward Field_Kind2X / Z
            const long x = Field_Kind2X, z = Field_Kind2Z;
            const std::int32_t dx = Abs32(static_cast<std::uint32_t>(x) - static_cast<std::uint32_t>(Long(k + 0x34)));
            const std::int32_t dz = Abs32(static_cast<std::uint32_t>(z) - static_cast<std::uint32_t>(Long(k + 0x38)));
            const std::int32_t distance = dx >= dz ? dx : dz;
            if (distance == 0) {   // already there
                k[0x80] = static_cast<unsigned char>(flags & 0xFB);
                k[2] = 0;
                return;
            }
            const std::uint32_t twice = static_cast<std::uint32_t>(distance) * 2u;
            const unsigned char speed = Field_MoveSpeeds[k[0x84]];   // an unchecked byte index, as the original's
            MoveScript_F3Divisor = static_cast<short>(static_cast<std::uint16_t>(speed << 3));
            const long ground = g.elevation(x, z);
            const auto rest = static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(k + 0x3C)) -
                                                        (static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(ground))) << 16));
            const std::int32_t per_speed = Idiv(0x80, MoveScript_F3Divisor);
            MoveScript_FAWord = 0;
            k[2] = 3;
            const auto frames = static_cast<std::int32_t>(static_cast<std::int16_t>(twice >> 16));
            const std::int32_t slope = Idiv(rest, static_cast<std::int32_t>(static_cast<std::uint32_t>(per_speed) * static_cast<std::uint32_t>(frames)));
            Field_Kind2X = Long(k + 0x34);
            Field_Kind2Z = Long(k + 0x38);
            SetLong(k + 0x14, slope);
            SetWord(k + 0x3E, static_cast<std::uint16_t>(MapView_Elevation));
            return;
        }
        // No glide: the view is reset on the object where it stands.
        Field_Kind2X = Long(k + 0x34);
        Field_Kind2Z = Long(k + 0x38);
        g.view_reset();
        k[8] = 0;
        k[2] = 0;
        return;
    }
    if (flags & 0x20) {
        k[2] = 2;
        return;
    }
    if (flags & 0x80) {
        k[2] = 4;
        return;
    }
    if (k[0x87] == 0) {
        k[2] = 0;
        return;
    }
    // A timed move began: its first frame of elevation, then state 1.
    SetLong(k + 0x3C, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(k + 0x3C)) + static_cast<std::uint32_t>(Long(k + 0x14))));
    g.set_elevation(Word(k + 0x3E));
    k[2] = 1;
}

// original 0x5732B0, state 1: the timed move (PSX 0x801C6BB0). While
// Field_Kind2Hold, one more frame of elevation; when it clears, the view is
// grounded where Field_Kind2X / Z stand (unless context bit 0x40), and the
// script steps again the same frame.
//
// As the original has it: MapView_SetElevation is given the high word of the
// elevation long (the original pushes a dword whose high half is that word
// again; the callee uses the low 16 bits only). The context flags are read
// after the two calls. The script step is a tail jump (the PSX calls it).
extern "C" void __cdecl Kind2_Move(void) {
    unsigned char* const k = Sprite_Kind2;
    if (Field_Kind2Hold != 0) {
        SetLong(k + 0x3C, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(k + 0x3C)) + static_cast<std::uint32_t>(Long(k + 0x14))));
        g.set_elevation(Word(k + 0x3E));
        return;
    }
    if (!(k[0x80] & 0x40)) g.set_elevation(static_cast<int>(g.elevation(Field_Kind2X, Field_Kind2Z)));
    const unsigned char flags = k[0x80];
    k[0x87] = 0;
    k[2] = 0;
    k[0x80] = static_cast<unsigned char>(flags & 0xBF);
    g.script();
}

// original 0x573320, state 2: wait while Field_Request is 2 (PSX 0x801C6C74).
extern "C" void __cdecl Kind2_WaitRequest(void) {
    if (Field_Request == 2) return;
    unsigned char* const k = Sprite_Kind2;
    const unsigned char flags = k[0x80];
    k[2] = 0;
    k[0x80] = static_cast<unsigned char>(flags & 0xDF);
}

// original 0x573340, state 3: the glide Kind2_Script set up (PSX 0x801C6CAC).
// While Field_Kind2Hold, the slope k +0x14 is added to the elevation - if it
// is not 0; when it clears, the view is grounded where Field_Kind2X / Z stand
// and the glide ends. As the original has it: the context flags are read after
// the two calls.
extern "C" void __cdecl Kind2_Travel(void) {
    unsigned char* const k = Sprite_Kind2;
    if (Field_Kind2Hold != 0) {
        const std::int32_t slope = Long(k + 0x14);
        if (slope == 0) return;
        SetLong(k + 0x3C, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(k + 0x3C)) + static_cast<std::uint32_t>(slope)));
        g.set_elevation(Word(k + 0x3E));
        return;
    }
    g.set_elevation(static_cast<int>(g.elevation(Field_Kind2X, Field_Kind2Z)));
    const unsigned char flags = k[0x80];
    SetLong(k + 0x14, 0);
    k[0x80] = static_cast<unsigned char>(flags & 0xFB);
    k[8] = 0;
    k[2] = 0;
}

// original 0x5733B0, state 4: k +9 frames of rising (PSX FUN_801C6D60).
//
// As the original has it: the rise is 16-bit - the elevation's high word plus
// the LOW word of k +0x14, wrapping - where state 1 and 3 add whole longs; and
// the frame count is read again after MapView_SetElevation.
extern "C" void __cdecl Kind2_Lift(void) {
    unsigned char* const k = Sprite_Kind2;
    if (k[9] == 0) {
        const unsigned char flags = k[0x80];
        k[2] = 0;
        k[0x80] = static_cast<unsigned char>(flags & 0x7F);
        return;
    }
    const auto risen = static_cast<std::uint16_t>(Word(k + 0x3E) + Word(k + 0x14));
    SetWord(k + 0x3E, risen);
    g.set_elevation(risen);
    --k[9];
}

namespace {

// --- BOF3X_SHADOW=kind2_object: a differential fuzz, once at start-up -------
// Seven byte-copies. The five states' calls are re-aimed at the recorders
// below (Kind2_Move's tail jump included, so each state is tested alone); the
// two dispatchers' `jmp [eax*4 + table]` operands are re-aimed at a table of
// seven recorders, laid out as the original's two tables are - the state
// table two entries into the run table - and ours are given the same. One
// round: one of the seven; random Sprite_Kind2, Field_Kind2X / Z, hold, the
// divisor, FAWord, MapView_Elevation, Field_Request, area; each branch's
// boundaries seeded; theirs, then from the same state ours; all of it,
// Sprite_Current and the recorders' log compared.
//
// Never generated, because the original faults on them (and a fault in a
// start-up self-test hangs the start-up): a speed index whose speed is 0 or
// above 16, a glide distance of 1..0x7FFF, one of 0x7FFF8000 or more (frames
// of -1, where INT_MIN / -1 faults) and both axes INT_MIN. The recorders
// that may change the divisor keep it in 1..0x80 for the same reason.

constexpr unsigned kLog = 32;
struct Entry { std::uint32_t what, a, b, c; };
Entry g_log[kLog];
unsigned g_log_n, g_seed;
unsigned char g_other[0xA4];   // what Sprite_Current points at before the round
unsigned char g_descriptor[0x44];
const unsigned char* g_scripts[256];
struct AfterStep { unsigned char flags, result, steps; std::uint32_t distance; };
AfterStep g_after_step;

std::uint32_t Hash(std::uint32_t salt = 0) {
    std::uint32_t h = (g_seed + g_log_n * 0x10001u + salt * 0x3C6EF372u) * 0x9E3779B1u;
    h ^= h >> 15;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    return h;
}
void Record(std::uint32_t what, std::uint32_t a = 0, std::uint32_t b = 0, std::uint32_t c = 0) {
    if (g_log_n < kLog) g_log[g_log_n] = {what, a, b, c};
    ++g_log_n;
}
std::uint32_t Id(const void* p) {
    if (p == Sprite_Kind2 + 0x80) return 1;
    return Address(p);
}

// Shifts the object and Field_Kind2X / Z by the same amounts: what the glide
// reads after the step moves, but its distance - which must stay clear of the
// faulting values - does not.
void Shift(std::uint32_t h) {
    unsigned char* const k = Sprite_Kind2;
    const std::uint32_t by_x = Hash(h), by_z = Hash(h + 1);
    SetLong(k + 0x34, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(k + 0x34)) + by_x));
    SetLong(k + 0x38, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(k + 0x38)) + by_z));
    Field_Kind2X = static_cast<long>(static_cast<std::uint32_t>(Field_Kind2X) + by_x);
    Field_Kind2Z = static_cast<long>(static_cast<std::uint32_t>(Field_Kind2Z) + by_z);
}

// The step: the result (0xFF often), and what the caller reads after it - the
// context flags, the wait, the timed move's steps, the speed (kept to 1..5),
// the elevation and its slope, the position.
unsigned char __cdecl StubStep(unsigned char* context, const unsigned char* script) {
    Record(1, Id(context), Id(script), Address(Sprite_Current));
    unsigned char* const k = Sprite_Kind2;
    const std::uint32_t h = Hash();
    if (h % 2) k[0x80] = static_cast<unsigned char>(Hash(1) & (h % 4 == 1 ? 0xF7 : 0xFF));   // often not ended
    if (h % 3 == 0) k[0x87] = static_cast<unsigned char>(Hash(2) % 3);
    if (h % 5 == 0) k[0x84] = static_cast<unsigned char>(1 + Hash(3) % 5);
    if (h % 7 == 0) Shift(4);
    if (h % 11 == 0) SetLong(k + 0x14, static_cast<std::int32_t>(Hash(6)));
    if (h % 13 == 0) SetLong(k + 0x3C, static_cast<std::int32_t>(Hash(7)));
    if (h % 17 == 0) { k[1] = static_cast<unsigned char>(Hash(8)); k[2] = static_cast<unsigned char>(Hash(9)); k[3] = static_cast<unsigned char>(Hash(10)); }
    const unsigned char result = (h >> 28) < 7 ? 0xFF : static_cast<unsigned char>(Hash(11));
    // For the coverage count: what the caller branches on after this.
    const std::int32_t dx = Abs32(static_cast<std::uint32_t>(Field_Kind2X) - static_cast<std::uint32_t>(Long(k + 0x34)));
    const std::int32_t dz = Abs32(static_cast<std::uint32_t>(Field_Kind2Z) - static_cast<std::uint32_t>(Long(k + 0x38)));
    g_after_step = {k[0x80], result, k[0x87], static_cast<std::uint32_t>(dx >= dz ? dx : dz)};
    return result;
}
// The ground: what the callers read after it - the elevation long, the view's
// elevation, the divisor (kept to 1..0x80), the context flags, the position.
long __cdecl StubElevation(long x, long z) {
    Record(2, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(z));
    unsigned char* const k = Sprite_Kind2;
    const std::uint32_t h = Hash();
    if (h % 3 == 0) SetLong(k + 0x3C, static_cast<std::int32_t>(Hash(1)));
    if (h % 4 == 0) MapView_Elevation = static_cast<long>(Hash(2));
    if (h % 5 == 0) MoveScript_F3Divisor = static_cast<short>(1 + Hash(3) % 0x80);
    if (h % 7 == 0) k[0x80] = static_cast<unsigned char>(Hash(4));
    if (h % 11 == 0) Shift(5);
    if (h % 13 == 0) { k[8] = static_cast<unsigned char>(Hash(7)); k[2] = static_cast<unsigned char>(Hash(8)); k[0x87] = static_cast<unsigned char>(Hash(9)); }
    return static_cast<long>(Hash(10));
}
// MapView_SetElevation uses the low 16 bits of its argument only (its
// symbols.toml entry), and the original pushes words with whatever high half
// the register held: so the low 16 bits are what is compared.
void __cdecl StubSetElevation(int value) {
    Record(3, static_cast<std::uint16_t>(value));
    unsigned char* const k = Sprite_Kind2;
    const std::uint32_t h = Hash();
    if (h % 3 == 0) k[9] = static_cast<unsigned char>(Hash(1) % 4);
    if (h % 5 == 0) k[0x80] = static_cast<unsigned char>(Hash(2));
    if (h % 7 == 0) { k[2] = static_cast<unsigned char>(Hash(3)); k[0x87] = static_cast<unsigned char>(Hash(4)); }
}
void __cdecl StubViewReset() {
    Record(4, static_cast<std::uint32_t>(Field_Kind2X), static_cast<std::uint32_t>(Field_Kind2Z));
    unsigned char* const k = Sprite_Kind2;
    const std::uint32_t h = Hash();
    if (h % 2) { k[8] = static_cast<unsigned char>(Hash(1)); k[2] = static_cast<unsigned char>(Hash(2)); }
    if (h % 3 == 0) k[0x80] = static_cast<unsigned char>(Hash(3));
}
void __cdecl StubScript() { Record(5, Sprite_Kind2[2], Sprite_Kind2[0x80], Sprite_Kind2[0x87]); }
template <unsigned N> void __cdecl StubEntry() { Record(0x10 + N, Sprite_Kind2[1], Sprite_Kind2[2]); }
// As the original's .data: the run table's two entries, then the state table's five.
const Handler kTable[7] = {StubEntry<0>, StubEntry<1>, StubEntry<2>, StubEntry<3>, StubEntry<4>, StubEntry<5>, StubEntry<6>};
const Callees kStubs = {kTable, kTable + 2, StubStep, StubElevation, StubSetElevation, StubViewReset, StubScript};

const void* StubFor(std::uint32_t target) {
    const auto f = [](auto p) { return reinterpret_cast<const void*>(p); };
    switch (target) {
    case 0x576B50: return f(&StubStep);
    case 0x5720C0: return f(&StubElevation);
    case 0x5725F0: return f(&StubSetElevation);
    case 0x56F670: return f(&StubViewReset);
    case 0x5730A0: return f(&StubScript);
    default: bof3::Fatal("kind2_object: no stand-in for a call to 0x%X", (unsigned)target); return nullptr;
    }
}

// The seven copies and their calls out, by capstone 2026-09-22; every other
// jump stays inside. The dispatchers' one instruction each is `jmp [eax*4 +
// table]` at +7, its disp32 at +0xA.
struct Call { std::uint32_t offset, target; };
struct Clone { const char* name; std::uint32_t base, size; const Call* calls; int n_calls; std::uint32_t table; };
constexpr Call kScriptCalls[] = {{0x46, 0x576b50}, {0x109, 0x5720c0}, {0x191, 0x56f670}, {0x1f3, 0x5725f0}};
constexpr Call kMoveCalls[] = {{0x1f, 0x5720c0}, {0x25, 0x5725f0}, {0x47, 0x5730a0}, {0x65, 0x5725f0}};
constexpr Call kTravelCalls[] = {{0x16, 0x5720c0}, {0x1c, 0x5725f0}, {0x68, 0x5725f0}};
constexpr Call kLiftCalls[] = {{0x1d, 0x5725f0}};
constexpr std::uint32_t kTableDisp = 0xA;
const Clone kClones[] = {
    {"Kind2_Run", 0x573080, 0xE, nullptr, 0, 0x6632AC},
    {"Kind2_Dispatch", 0x573090, 0xE, nullptr, 0, 0x6632B4},
    {"Kind2_Script", 0x5730A0, 0x207, kScriptCalls, 4, 0},
    {"Kind2_Move", 0x5732B0, 0x6C, kMoveCalls, 4, 0},
    {"Kind2_WaitRequest", 0x573320, 0x1D, nullptr, 0, 0},
    {"Kind2_Travel", 0x573340, 0x6F, kTravelCalls, 3, 0},
    {"Kind2_Lift", 0x5733B0, 0x46, kLiftCalls, 1, 0},
};
constexpr unsigned kCount = 7;

struct Region { std::uint32_t at, size; };
const Region kRegions[] = {
    {Address(Sprite_Kind2), Sprite_Kind2_count},
    {Address(&Field_Kind2Z), 8},   // Field_Kind2Z, then Field_Kind2X
    {Address(&Field_Kind2Hold), 1},
    {Address(&MoveScript_F3Divisor), 2},
    {Address(&MoveScript_FAWord), 2},
    {Address(&MapView_Elevation), 4},
    {Address(&Field_Request), 1},
};
constexpr unsigned kRegionBytes = Sprite_Kind2_count + 8 + 1 + 2 + 2 + 4 + 1;

struct State {
    unsigned char memory[kRegionBytes];
    unsigned char* current;
    std::uint16_t area;
    Entry log[kLog];
    unsigned log_n;
};
void Capture(State& s) {
    unsigned at = 0;
    for (const Region& r : kRegions) { std::memcpy(s.memory + at, At(r.at), r.size); at += r.size; }
    s.current = Sprite_Current;
    s.area = Game_AreaNumber;
    std::memcpy(s.log, g_log, sizeof s.log);
    s.log_n = g_log_n;
}
void Apply(const State& s) {
    unsigned at = 0;
    for (const Region& r : kRegions) { std::memcpy(At(r.at), s.memory + at, r.size); at += r.size; }
    Sprite_Current = s.current;
    Game_AreaNumber = s.area;
    std::memset(g_log, 0, sizeof g_log);
    g_log_n = 0;
}

std::uint32_t g_rng = 0x2545F491u;
std::uint32_t Next() { g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5; return g_rng; }
bool Often() { return Next() % 3 == 0; }

// The glide's distance: 0, or 0x8000..0x7FFF7FFF on the larger axis - the
// edges of the frame count's high word seeded (0x8000 and 0xFFFF / 0x10000
// frames of 1 and 2, 0x3FFFFFFF / 0x40000000 where it turns negative) - with
// the smaller axis 0, equal, or anything below.
void SeedDistance() {
    unsigned char* const k = Sprite_Kind2;
    std::uint32_t major = 0, minor = 0;
    if (Next() % 4) {
        static const std::uint32_t kEdges[] = {0x8000, 0x8001, 0xFFFF, 0x10000, 0x17FFF, 0x18000, 0x3FFFFFFF, 0x40000000, 0x7FFF7FFF};
        switch (Next() % 3) {
        case 0: major = kEdges[Next() % (sizeof kEdges / sizeof kEdges[0])]; break;
        case 1: major = 0x8000 + Next() % 0x400000; break;
        default: major = 0x8000 + Next() % (0x7FFF8000u - 0x8000u); break;
        }
        switch (Next() % 4) {
        case 0: minor = 0; break;
        case 1: minor = major; break;
        default: minor = Next() % (major + 1); break;
        }
    }
    std::uint32_t dx = major, dz = minor;
    if (Next() % 2) { dx = minor; dz = major; }
    if (Next() % 2) dx = 0u - dx;
    if (Next() % 2) dz = 0u - dz;
    Field_Kind2X = static_cast<long>(static_cast<std::uint32_t>(Long(k + 0x34)) + dx);
    Field_Kind2Z = static_cast<long>(static_cast<std::uint32_t>(Long(k + 0x38)) + dz);
}

// A dispatcher's copy: its `jmp [eax*4 + table]` re-aimed at `table`.
void AimTable(void* copy, const Clone& c, const Handler* table) {
    auto* code = static_cast<std::uint8_t*>(copy);
    std::uint32_t disp;
    std::memcpy(&disp, code + kTableDisp, sizeof disp);
    if (code[7] != 0xFF || code[8] != 0x24 || code[9] != 0x85 || disp != c.table)
        bof3::Fatal("kind2_object: %s has no jmp [eax*4 + 0x%X] at +7", c.name, (unsigned)c.table);
    disp = Address(table);
    std::memcpy(code + kTableDisp, &disp, sizeof disp);
    FlushInstructionCache(GetCurrentProcess(), copy, c.size);
}

void SelfTest(void* const (&theirs)[kCount]) {
    constexpr unsigned kRounds = 40000;
    unsigned region_bytes = 0;
    for (const Region& r : kRegions) region_bytes += r.size;
    if (region_bytes != kRegionBytes) bof3::Fatal("kind2_object: regions are %u bytes, the state holds %u", region_bytes, kRegionBytes);
    static State saved, input, their_out, our_out;
    Capture(saved);
    const unsigned char* const* const scripts = g_scripts;
    std::memcpy(g_descriptor + 0x1C, &scripts, sizeof scripts);
    for (unsigned i = 0; i < 256; ++i) g_scripts[i] = reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(0x20000u + i * 0x40u));
    g = kStubs;

    // Kind2_Script's branches as the original took them: wait, end, glide
    // (and with negative frames), already there, view reset, states 2 / 4 / 1 / 0.
    enum { kWait, kEnd, kGlide, kNegative, kThere, kReset, kTo2, kTo4, kTo1, kTo0, kBranches };
    unsigned branch[kBranches] = {};
    unsigned bad = 0, calls = 0, per[kCount] = {};
    for (unsigned round = 0; round < kRounds; ++round) {
        const unsigned k = round % 10 < kCount ? round % 10 : 2;   // Kind2_Script, the most branches, four rounds in ten
        ++per[k];
        auto* bytes = reinterpret_cast<unsigned char*>(&input);
        for (unsigned i = 0; i < offsetof(State, log); ++i) bytes[i] = static_cast<unsigned char>(Next());
        input.current = Next() % 2 ? g_other : Sprite_Kind2;
        input.area = static_cast<std::uint16_t>(Next() % Area_Descriptors_count);
        g_seed = Next();
        Apply(input);
        unsigned char* const o = Sprite_Kind2;
        // The boundaries: every run byte and state in range; the wait 0 or
        // not; a speed with a non-zero divisor; the glide's distance; the
        // hold, bit 0x40, a zero slope, a zero frame count, Field_Request 2.
        o[1] = static_cast<unsigned char>(Next() % 7);
        o[2] = static_cast<unsigned char>(Next() % 5);
        o[0x81] = static_cast<unsigned char>(Next() % 4 ? 0 : Next() % 3);
        o[0x84] = static_cast<unsigned char>(1 + Next() % 5);
        o[0x87] = static_cast<unsigned char>(Next() % 2 ? 0 : Next());
        o[9] = static_cast<unsigned char>(Next() % 2 ? 0 : 1 + Next() % 3);
        if (Often()) SetLong(o + 0x14, 0);
        if (Next() % 2) Field_Kind2Hold = 0;
        if (Next() % 2) Field_Request = 2;
        MoveScript_F3Divisor = static_cast<short>(Next());
        SeedDistance();
        Capture(input);
        unsigned char* const saved_descriptor = Area_Descriptors[input.area];
        Area_Descriptors[input.area] = g_descriptor;
        for (int pass = 0; pass < 2; ++pass) {
            Apply(input);
            if (pass) {
                const Handler ours[kCount] = {Kind2_Run, Kind2_Dispatch, Kind2_Script, Kind2_Move, Kind2_WaitRequest, Kind2_Travel, Kind2_Lift};
                ours[k]();
            } else {
                g_after_step = {};
                reinterpret_cast<Handler>(theirs[k])();
                if (k == 2) {
                    const AfterStep& a = g_after_step;
                    if (input.memory[0x81] != 0) ++branch[kWait];
                    else if (a.flags & 8) ++branch[kEnd];
                    else if (a.result == 0xFF && (a.flags & 4) && a.distance == 0) ++branch[kThere];
                    else if (a.result == 0xFF && (a.flags & 4)) ++branch[a.distance >= 0x40000000u ? kNegative : kGlide];
                    else if (a.result == 0xFF) ++branch[kReset];
                    else if (a.flags & 0x20) ++branch[kTo2];
                    else if (a.flags & 0x80) ++branch[kTo4];
                    else ++branch[a.steps ? kTo1 : kTo0];
                }
            }
            Capture(pass ? our_out : their_out);
        }
        Area_Descriptors[input.area] = saved_descriptor;
        calls += their_out.log_n;
        if (std::memcmp(&their_out, &our_out, sizeof their_out) != 0 && ++bad <= 12)
            bof3::Log("shadow      kind2_object self-test MISMATCH: round %u, %s, log %u / %u", round, kClones[k].name,
                      their_out.log_n, our_out.log_n);
    }
    g = kOriginals;
    Apply(saved);
    bof3::Log("shadow      kind2_object self-test: %u rounds (%u of Kind2_Script, %u of each other), %u calls to the stand-ins, Kind2_Script's "
              "branches wait %u / end %u / glide %u (negative frames %u) / there %u / view reset %u / to state 2 %u, 4 %u, "
              "1 %u, 0 %u; %u MISMATCHES; Sprite_Kind2, Field_Kind2X / Z, the hold, the divisor, FAWord, MapView_Elevation, "
              "Field_Request, Sprite_Current and the stand-ins' log compared",
              kRounds, per[2], per[0], calls, branch[kWait], branch[kEnd], branch[kGlide], branch[kNegative], branch[kThere],
              branch[kReset], branch[kTo2], branch[kTo4], branch[kTo1], branch[kTo0], bad);
    if (bad) bof3::Fatal("the kind-2 object's script runner differs from the original in %u of %u self-test rounds", bad, kRounds);
}

}  // namespace

void Kind2Object_Inject() {
    if (bof3::WantsShadow("kind2_object")) {
        void* clones[kCount];
        for (unsigned i = 0; i < kCount; ++i) {
            const Clone& c = kClones[i];
            bof3::CloneCall calls[4];
            if (c.n_calls > 4) bof3::Fatal("kind2_object: %s has %d calls", c.name, c.n_calls);
            for (int j = 0; j < c.n_calls; ++j) calls[j] = {c.calls[j].offset, StubFor(c.calls[j].target)};
            clones[i] = bof3::CloneOriginal(c.name, c.base, c.size, calls, c.n_calls);
        }
        AimTable(clones[0], kClones[0], kStubs.run_table);
        AimTable(clones[1], kClones[1], kStubs.state_table);
        SelfTest(clones);
    }
    BOF3_INJECT(Kind2_Run);
    BOF3_INJECT(Kind2_Dispatch);
    BOF3_INJECT(Kind2_Script);
    BOF3_INJECT(Kind2_Move);
    BOF3_INJECT(Kind2_WaitRequest);
    BOF3_INJECT(Kind2_Travel);
    BOF3_INJECT(Kind2_Lift);
}

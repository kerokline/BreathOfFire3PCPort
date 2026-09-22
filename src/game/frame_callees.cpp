// The field frame loop's unread callees (docs/frame-callees.md): what
// Field_Frame / Field_FrameScripted (0x517200 / 0x517240) call every field
// frame and nobody had read - originals 0x494030 (the 20 effect objects, run
// through their kind's handler), 0x469E30 (effect kind 0x13, the camera turn)
// with its three states 0x469E50 / 0x469EF0 / 0x469F70, 0x455250 (the 8 slot
// records), 0x454AD0 (the tint records' CLUT rebuild) and 0x531B60 (the party
// members' screen updates).
#include "game/frame_callees.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

using namespace move_script;

std::uint32_t Address(const volatile void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }

using Fn = void (__cdecl*)();
Fn Handler(unsigned long entry) { return reinterpret_cast<Fn>(static_cast<std::uintptr_t>(entry)); }

constexpr unsigned kEffects = 20, kEffectBytes = 0x80;
constexpr unsigned kSlots = 8, kSlotBytes = 0x10;
constexpr unsigned kTints = 32, kTintBytes = 12;
constexpr unsigned kMemberBytes = 0x14C;

// `cdq` / `idiv`: the original's signed division, faults included - a zero
// divisor raises the same #DE at the same point of the same computation,
// which a C++ `/` does not promise (as kind2_object.cpp's).
struct QuotRem { std::int32_t quot, rem; };
QuotRem Idiv(std::int32_t dividend, std::int32_t divisor) {
    std::int32_t quot, rem;
    __asm__ volatile("cltd\n\tidivl %%ecx" : "=a"(quot), "=&d"(rem) : "a"(dividend), "c"(divisor) : "cc");
    return {quot, rem};
}

// Every direct callee, through pointers so that the start-up fuzz can stand
// recording functions in for them. The indirect ones are the two tables,
// Effect_KindHandlers and CameraTurn_States, which the fuzz fills with
// recorders instead.
struct Callees {
    void (__cdecl* run_slot)(unsigned char);   // 0x455300
    void (__cdecl* update_screen_slot)();      // 0x588F00
    void (__cdecl* update_screen)();           // 0x588F20
    void (__cdecl* release)();                 // 0x589840
};
const Callees kOriginals = {Field_RunSlot, Sprite_UpdateScreenSlot, Sprite_UpdateScreen, Effect_Release};
Callees g = kOriginals;

// A colour component plus a tint byte, in 8 bits as the original adds them:
// a result that is negative as a signed byte is 0, one above 31 is 31 - so a
// large positive tint (0x61 and up on a component of 31) wraps to negative
// and comes out 0, not 31.
unsigned Tinted(unsigned component, unsigned char tint) {
    const auto sum = static_cast<signed char>(static_cast<unsigned char>(component + tint));
    if (sum < 0) return 0;
    if (sum > 0x1F) return 0x1F;
    return static_cast<unsigned>(sum);
}

}  // namespace

// original 0x494030: the effect objects' frame (PSX FUN_8019B0EC, term for
// term). Each of the 20 whose byte +0 is non-zero - any bit, read afresh each
// turn, so an object a handler spawns further on runs this frame - becomes
// Sprite_Current and runs Effect_KindHandlers[+5], unbounded as the
// original's.
extern "C" void __cdecl Effect_RunObjects(void) {
    unsigned char* object = Effect_Objects;
    for (unsigned n = kEffects; n != 0; --n, object += kEffectBytes) {
        if (object[0] == 0) continue;
        Sprite_Current = object;
        Handler(Effect_KindHandlers[object[5]])();
    }
}

// original 0x469E30: effect kind 0x13, the camera turn (PSX FUN_8019BC3C):
// its state +1 through CameraTurn_States, unbounded - states 3 and up are the
// next kind's - then MapView_Redraw 2, whatever the state did to it.
extern "C" void __cdecl Effect_CameraTurn(void) {
    Handler(CameraTurn_States[Sprite_Current[1]])();
    MapView_Redraw = 2;
}

// original 0x469E50: the turn's first frame (PSX FUN_8019B90C, term for
// term). The three angles to 16.16, and a step for each toward the dwords
// +0x64 / +0x68 / +0x6C over +9 frames: ((target - angle) << 16, in 32 bits)
// / frames, signed and truncating. A +9 of 0 divides by zero, as on both
// platforms (the spawners at 0x406B56 / 0x406BE3 give 16). MapView_Redraw
// takes the frame count - which Effect_CameraTurn overwrites with 2 on return.
extern "C" void __cdecl CameraTurn_Start(void) {
    const std::int32_t x = Camera_Angles[0], z = Camera_Angles[2];
    unsigned char* const object = Sprite_Current;
    const std::int32_t y = Camera_Angles[1];
    CameraTurn_Angles[0] = static_cast<long>(static_cast<std::uint32_t>(x) << 16);
    CameraTurn_Angles[1] = static_cast<long>(static_cast<std::uint32_t>(y) << 16);
    CameraTurn_Angles[2] = static_cast<long>(static_cast<std::uint32_t>(z) << 16);
    const auto step = [object](unsigned at, std::int32_t angle) {
        const std::uint32_t diff = static_cast<std::uint32_t>(Long(object + at)) - static_cast<std::uint32_t>(angle);
        return static_cast<long>(Idiv(static_cast<std::int32_t>(diff << 16), object[9]).quot);
    };
    CameraTurn_Steps[0] = step(0x64, x);
    CameraTurn_Steps[1] = step(0x68, y);
    CameraTurn_Steps[2] = step(0x6C, z);
    const unsigned char bits = Field_StatusBits;
    MapView_Redraw = object[9];
    Field_StatusBits = static_cast<unsigned char>(bits | 0x20);
    object[1] = 1;
}

// original 0x469EF0: each later frame (PSX FUN_8019BA4C). With frames left,
// each 16.16 angle takes its step (wrapping in 32 bits) and its high half goes
// to Camera_Angles; with none, the state is 2.
extern "C" void __cdecl CameraTurn_Step(void) {
    unsigned char* const object = Sprite_Current;
    if (object[9] == 0) {
        object[1] = 2;
        return;
    }
    const std::uint32_t x = static_cast<std::uint32_t>(CameraTurn_Angles[0]) + static_cast<std::uint32_t>(CameraTurn_Steps[0]);
    const std::uint32_t y = static_cast<std::uint32_t>(CameraTurn_Angles[1]) + static_cast<std::uint32_t>(CameraTurn_Steps[1]);
    const std::uint32_t z = static_cast<std::uint32_t>(CameraTurn_Angles[2]) + static_cast<std::uint32_t>(CameraTurn_Steps[2]);
    CameraTurn_Angles[0] = static_cast<long>(x);
    CameraTurn_Angles[1] = static_cast<long>(y);
    CameraTurn_Angles[2] = static_cast<long>(z);
    Camera_Angles[0] = static_cast<short>(x >> 16);
    Camera_Angles[1] = static_cast<short>(y >> 16);
    Camera_Angles[2] = static_cast<short>(z >> 16);
    object[9] = static_cast<unsigned char>(object[9] - 1);
}

// original 0x469F70: the last (PSX FUN_8019BB04). The angles exactly to the
// targets' low words, the turning bit cleared, and the object released - a
// tail jmp in the original.
extern "C" void __cdecl CameraTurn_End(void) {
    unsigned char* const object = Sprite_Current;
    Camera_Angles[0] = static_cast<short>(Word(object + 0x64));
    Camera_Angles[1] = static_cast<short>(Word(object + 0x68));
    Camera_Angles[2] = static_cast<short>(Word(object + 0x6C));
    Field_StatusBits = static_cast<unsigned char>(Field_StatusBits & 0xDF);
    g.release();
}

// original 0x455250: the slot records (PSX FUN_80197C8C, term for term). Each
// of the 8 whose byte +0 has bit 0 - read afresh each turn - runs
// Field_RunSlot(index). The original pushes the index as a whole stack dword
// of which it stored only the low byte (the upper three are the caller's
// ecx); Field_RunSlot masks it to a byte, so the clean byte here is the same.
extern "C" void __cdecl Field_RunSlots(void) {
    const unsigned char* slot = Field_Slots;
    for (unsigned index = 0; index < kSlots; ++index, slot += kSlotBytes)
        if (slot[0] & 1) g.run_slot(static_cast<unsigned char>(index));
}

// original 0x454AD0: the tint records' frame (PSX FUN_8019725C, term for
// term). For each of the 32 MoveScript_TintRecords with bit 0 of byte +0:
//   - byte +5 is a CLUT depth, indexing Clut_Sixteens, Clut_PerRow and
//     Clut_Stride unbounded - depths 5..7 have 0 per row and divide by zero,
//     as the original does;
//   - CLUT number +1 (the source) and +6 (the target) each become a word
//     index into Gfx_ClutStrip: (n / per_row) << 8 plus the column
//     (n % per_row) * stride, kept to 8 bits;
//   - sixteens * 16 words are copied source to target in order, each
//     component plus a tint byte (+2 red, +3 green, +4 blue) in 8 bits and
//     clamped (Tinted), bit 15 forced by bit 1 of +0 or else kept; the
//     target's first word is 0 instead, whatever the source. Words are read
//     one at a time after the previous store, so overlapping source and
//     target rows copy forward, as the original's;
// then Gfx_ClutStripDirty is 1, and with bit 6 of +0 it is cleared and the
// sprite at dword +8 takes +6 as its CLUT number +0x27 - and with bit 7 its
// +0x24 bit 2. The original captures +0's bit 1, +3 and +4 before the
// copy and reads +2 in it; nothing the copy writes can reach the record, so
// that order is kept but unobservable. The tail re-reads +0 and the pointer
// after each store, in the original's order.
extern "C" void __cdecl MoveScript_TintFrame(void) {
    for (unsigned char* record = MoveScript_TintRecords; record < MoveScript_TintRecords + kTints * kTintBytes;
         record += kTintBytes) {
        if (!(record[0] & 1)) continue;
        const unsigned depth = record[5];
        const std::int32_t per_row = Clut_PerRow[depth];
        const unsigned stride = Clut_Stride[depth];
        const QuotRem from = Idiv(record[1], per_row);
        const QuotRem to = Idiv(record[6], per_row);
        const unsigned count = static_cast<unsigned>(Clut_Sixteens[depth]) << 4;
        if (count != 0) {
            const std::uint32_t source = (static_cast<std::uint32_t>(from.quot & 0xFF) << 8) +
                                         static_cast<unsigned char>(static_cast<unsigned>(from.rem) * stride);
            const std::uint32_t target = (static_cast<std::uint32_t>(to.quot & 0xFF) << 8) +
                                         static_cast<unsigned char>(static_cast<unsigned>(to.rem) * stride);
            const bool force = (record[0] & 2) != 0;
            const unsigned char green = record[3], blue = record[4];
            for (unsigned i = 0; i < count; ++i) {
                const std::uint16_t word = Gfx_ClutStrip[source + i];
                const unsigned top = force ? 1 : word >> 15;
                const unsigned r = Tinted(word & 0x1F, record[2]);
                const unsigned gr = Tinted((word >> 5) & 0x1F, green);
                const unsigned b = Tinted((word >> 10) & 0x1F, blue);
                Gfx_ClutStrip[target + i] = i == 0 ? 0 : static_cast<unsigned short>(top << 15 | b << 10 | gr << 5 | r);
            }
        }
        const unsigned char flags = record[0];
        Gfx_ClutStripDirty = 1;
        if (!(flags & 0x40)) continue;
        const unsigned char clut = record[6];
        record[0] = static_cast<unsigned char>(flags & 0xBF);
        At(static_cast<std::uint32_t>(Long(record + 8)))[0x27] = clut;
        if (!(record[0] & 0x80)) continue;
        At(static_cast<std::uint32_t>(Long(record + 8)))[0x24] |= 4;
    }
}

// original 0x531B60: the party members' screen updates (PSX FUN_801BEAB0,
// term for term). Field_MemberCount read once; from the last member down,
// each without bit 6 of +0 becomes Sprite_Current and is updated - through
// Sprite_UpdateScreenSlot when Field_Request (read each time) is 3. A count
// above 3 walks on past ObjTrio's three at the same stride, as the original.
extern "C" void __cdecl Party_UpdateScreens(void) {
    const int last = static_cast<int>(Field_MemberCount) - 1;
    if (last < 0) return;
    unsigned char* member = ObjTrio + last * kMemberBytes;
    for (int left = last + 1; left != 0; --left, member -= kMemberBytes) {
        if (member[0] & 0x40) continue;
        Sprite_Current = member;
        if (Field_Request == 3) g.update_screen_slot();
        else g.update_screen();
    }
}

namespace {

// --- BOF3X_SHADOW=frame_callees: a differential fuzz, once at start-up ------
// Eight byte-copies, every call out of them re-aimed at the recorders below;
// the two tables' first entries (32 of Effect_KindHandlers, 10 of
// CameraTurn_States) are recorders for the fuzz's duration, for both sides.
// One round: one of the eight; random bytes in the effect objects, slots,
// members, tint records, the camera words and flags (and for the tint frame
// every word of Gfx_ClutStrip it can reach), each branch's boundaries seeded;
// theirs, then from the same state ours; all of it and the recorders' log
// compared.

constexpr unsigned kLog = 512, kKinds = 32, kStates = 10, kSprites = 4, kSpriteBytes = 0x40;
constexpr unsigned kMembersSpan = 5 * kMemberBytes + 4;   // a count of up to 5 reads byte +0 of five
constexpr std::uint32_t kTintFrom = 0x7E06F0;              // records less a margin: sprite pointers aim into them
constexpr unsigned kTintSpan = kTints * kTintBytes + (0x7E0700 - kTintFrom);
constexpr unsigned kStripWords = 0xFFFF + 0xFF0 + 1;      // the highest index: row 255, column 255, 4079 on

unsigned char g_sprites[kSprites * kSpriteBytes];
struct Entry { std::uint32_t what, current, a; };
Entry g_log[kLog];
unsigned g_log_n, g_seed;

std::uint32_t Hash() {
    std::uint32_t h = (g_seed + g_log_n) * 0x9E3779B1u;
    h ^= h >> 15;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    return h;
}
void Record(std::uint32_t what, std::uint32_t a = 0) {
    if (g_log_n < kLog) g_log[g_log_n] = {what, Address(Sprite_Current), a};
    ++g_log_n;
}
unsigned char* Effect(unsigned k) { return Effect_Objects + (k % kEffects) * kEffectBytes; }
unsigned char* Member(unsigned k) { return ObjTrio + (k % 5) * kMemberBytes; }

// A callee may change what the caller reads again after it: which objects
// are in use and their kinds, the slots' and members' flag bytes, the
// request byte, the member count (read once), the current object, the
// redraw byte.
void Disturb() {
    const std::uint32_t h = Hash();
    if (h % 3 == 0) {
        unsigned char* const o = Effect(h >> 8);
        o[0] = static_cast<unsigned char>((h >> 13) % 3 == 0 ? 0 : (h >> 16) | 0x02);
        o[5] = static_cast<unsigned char>((h >> 24) % kKinds);
    }
    if (h % 5 == 0) Field_Slots[((h >> 8) % kSlots) * kSlotBytes] = static_cast<unsigned char>(h >> 16);
    if (h % 7 == 0) Member(h >> 8)[0] = static_cast<unsigned char>(h >> 16);
    if (h % 4 == 0) Field_Request = static_cast<unsigned char>((h >> 12) % 2 ? 3 : h >> 20);
    if (h % 6 == 0) Field_MemberCount = static_cast<unsigned char>((h >> 8) % 6);
    if (h % 8 == 0) Sprite_Current = Effect(h >> 12);
    if (h % 9 == 0) MapView_Redraw = static_cast<unsigned char>(h >> 4);
}

template <unsigned K> void __cdecl StubKind() { Record(100 + K); Disturb(); }
template <unsigned S> void __cdecl StubState() { Record(200 + S); Disturb(); }
// The index arrives as a whole pushed dword from the copy: masked, as
// Field_RunSlot masks it.
void __cdecl StubRunSlot(std::uint32_t index) { Record(1, index & 0xFF); Disturb(); }
void __cdecl StubScreenSlot() { Record(2); Disturb(); }
void __cdecl StubScreen() { Record(3); Disturb(); }
// Effect_Release's own effect: bytes 0..4 of the current object to 0.
void __cdecl StubRelease() {
    Record(4);
    for (unsigned i = 0; i < 5; ++i) Sprite_Current[i] = 0;
    Disturb();
}

using Raw = const void*;
template <std::size_t... K> void FillKinds(unsigned long* table, std::index_sequence<K...>) {
    ((table[K] = Address(reinterpret_cast<Raw>(&StubKind<K>))), ...);
}
template <std::size_t... S> void FillStates(unsigned long* table, std::index_sequence<S...>) {
    ((table[S] = Address(reinterpret_cast<Raw>(&StubState<S>))), ...);
}
const Callees kStubs = {
    reinterpret_cast<void (__cdecl*)(unsigned char)>(reinterpret_cast<std::uintptr_t>(&StubRunSlot)),
    StubScreenSlot, StubScreen, StubRelease,
};

// The eight copies and their calls out, listed by capstone 2026-09-22; every
// other jump stays inside, and the only indirect calls are the two tables'.
// Offsets are of the E8 or E9 byte.
struct Call { std::uint32_t offset, target; };
struct Clone { const char* name; std::uint32_t base, size; Call calls[2]; int n_calls; };
constexpr unsigned kCount = 8;
constexpr unsigned kStart = 2, kStep = 3, kTint = 6;   // indexes of kClones
const Clone kClones[kCount] = {
    {"Effect_RunObjects", 0x494030, 0x2F, {}, 0},
    {"Effect_CameraTurn", 0x469E30, 0x1A, {}, 0},
    {"CameraTurn_Start", 0x469E50, 0x99, {}, 0},
    {"CameraTurn_Step", 0x469EF0, 0x77, {}, 0},
    {"CameraTurn_End", 0x469F70, 0x36, {{0x31, 0x589840}}, 1},   // the tail jmp
    {"Field_RunSlots", 0x455250, 0x32, {{0x18, 0x455300}}, 1},
    {"MoveScript_TintFrame", 0x454AD0, 0x1EB, {}, 0},
    {"Party_UpdateScreens", 0x531B60, 0x4B, {{0x33, 0x588F00}, {0x3A, 0x588F20}}, 2},
};
Raw StubFor(std::uint32_t target) {
    switch (target) {
    case 0x455300: return reinterpret_cast<Raw>(&StubRunSlot);
    case 0x588F00: return reinterpret_cast<Raw>(&StubScreenSlot);
    case 0x588F20: return reinterpret_cast<Raw>(&StubScreen);
    case 0x589840: return reinterpret_cast<Raw>(&StubRelease);
    default: bof3::Fatal("frame_callees: no stand-in for a call to 0x%X", (unsigned)target);
    }
}

struct Region { std::uint32_t at, size; };
const Region kRegions[] = {
    {Address(Effect_Objects), kEffects * kEffectBytes},
    {Address(Field_Slots), kSlots * kSlotBytes},
    {Address(ObjTrio), kMembersSpan},
    {kTintFrom, kTintSpan},
    {Address(g_sprites), sizeof g_sprites},
    {Address(Camera_Angles), 6},
    {Address(CameraTurn_Angles), 12},
    {Address(CameraTurn_Steps), 12},
    {Address(&MapView_Redraw), 1},
    {Address(&Field_StatusBits), 1},
    {Address(&Field_Request), 1},
    {Address(&Field_MemberCount), 1},
    {Address(&Gfx_ClutStripDirty), 1},
};
constexpr unsigned kRegionBytes = kEffects * kEffectBytes + kSlots * kSlotBytes + kMembersSpan + kTintSpan +
                                  kSprites * kSpriteBytes + 6 + 12 + 12 + 5;

struct State {
    unsigned char memory[kRegionBytes];
    unsigned char* current;
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

std::uint32_t g_rng = 0x6D2B79F5u;
std::uint32_t Next() { g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5; return g_rng; }
bool Often() { return Next() % 3 == 0; }
bool Half() { return Next() % 2 == 0; }
template <class T, std::size_t N> T Pick(const T (&values)[N]) { return values[Next() % N]; }

// Byte +0 as the loops test it: 0, bit 0 alone, other bits without bit 0.
unsigned char InUse() {
    static const unsigned char kBytes[] = {0, 1, 2, 0x80, 0x40, 0xFE, 0xFF, 0x41};
    return Half() ? Pick(kBytes) : static_cast<unsigned char>(Next());
}

// The camera turn's words: its boundaries are the s16 edges of the angles,
// the 32-bit wrap of the 16.16 sums and of (target - angle) << 16, and the
// division's sign and truncation.
void SeedCamera() {
    static const std::uint16_t kAngles[] = {0, 1, 0x7FFF, 0x8000, 0xFFFF, 0x200, 0xFCFE, 0xFD56};
    for (unsigned i = 0; i < 3; ++i) Camera_Angles[i] = static_cast<short>(Half() ? Pick(kAngles) : Next());
    static const std::uint32_t kLongs[] = {0x7FFF0000u, 0x80000000u, 0xFFFF0000u, 0x00010000u, 0x7FFFFFFFu, 0xFFFFFFFFu, 0};
    for (unsigned i = 0; i < 3; ++i) {
        CameraTurn_Angles[i] = static_cast<long>(Half() ? Pick(kLongs) : Next());
        CameraTurn_Steps[i] = static_cast<long>(Half() ? Pick(kLongs) : Next() >> (Next() % 32));
    }
}
void SeedEffect(unsigned char* o) {
    o[0] = InUse();
    o[1] = static_cast<unsigned char>(Next() % 4 ? Next() % 3 : Next() % kStates);
    o[5] = static_cast<unsigned char>(Often() ? 0x13 : Next() % kKinds);
    static const unsigned char kFrames[] = {0, 1, 2, 0x10, 0x7F, 0x80, 0xFF};
    o[9] = Half() ? Pick(kFrames) : static_cast<unsigned char>(Next());
    for (unsigned i = 0; i < 3; ++i) {
        const std::int32_t angle = Camera_Angles[i];
        static const std::uint32_t kTargets[] = {0x7FFFFFFFu, 0x80000000u, 0xFFFFFCFEu, 0xFFFFFD56u, 0, 0x8000u, 0xFFFF7FFFu};
        std::uint32_t target = Pick(kTargets);
        if (Often()) target = static_cast<std::uint32_t>(angle) + Next() % 64 - 32;   // near: rounding toward zero
        else if (Often()) target = Next();
        SetLong(o + 0x64 + 4 * i, static_cast<std::int32_t>(target));
    }
}

// A tint record: its depth one whose row count is not 0 (the others divide
// by zero), mostly 0..4; source and target CLUT numbers apart, equal or a
// step apart, so that the copy meets its own stores; tints at the clamp's
// edges, 8-bit wrap included; a sprite pointer into the fuzz's sprites or
// into the records themselves, so that the tail's re-reads show. A pointer
// into the records aims its +0x27 at this record or an earlier one (and +0x24
// three bytes before): never at a later record's depth or pointer, which a
// later turn would divide by or write through, nor at this record's own
// pointer, which the tail reads again before the +0x24 store - both would
// fault the original as much as ours.
unsigned char g_depths[256];
unsigned g_depth_n;
std::uint32_t SpriteInRecords(unsigned index) {
    if (Often()) return 0x7E0700u + index * kTintBytes - 0x27;   // +0x27 on its own +0: the bit-7 test's re-read
    for (;;) {
        const unsigned at = Next() % ((index + 1) * kTintBytes);   // where +0x27 lands, from record 0
        if (at / kTintBytes == index && at % kTintBytes >= 8) continue;
        return 0x7E0700u + at - 0x27;
    }
}
void SeedTint(unsigned char* r, unsigned index) {
    r[0] = static_cast<unsigned char>(Next());
    if (Next() % 4) r[0] |= 1;
    r[5] = Next() % 4 ? static_cast<unsigned char>(Next() % 5) : g_depths[Next() % g_depth_n];
    r[1] = static_cast<unsigned char>(Next());
    if (Half()) r[1] = static_cast<unsigned char>(Next() % 40);
    const unsigned step = Next() % 5;
    r[6] = step == 0 ? r[1] : step == 1 ? static_cast<unsigned char>(r[1] + 1) : step == 2 ? static_cast<unsigned char>(r[1] - 1)
                                                                                      : static_cast<unsigned char>(Next());
    static const unsigned char kTintValues[] = {0, 1, 0xFF, 0x1F, 0x20, 0x60, 0x61, 0x7F, 0x80, 0x81, 0xE1, 0xE0, 0xC0};
    for (unsigned i = 2; i < 5; ++i) r[i] = Half() ? Pick(kTintValues) : static_cast<unsigned char>(Next());
    const std::uint32_t sprite = Often() ? SpriteInRecords(index) : Address(g_sprites + (Next() % kSprites) * kSpriteBytes);
    SetLong(r + 8, static_cast<std::int32_t>(sprite));
}

std::uint16_t g_strip_saved[kStripWords], g_strip_in[kStripWords], g_strip_theirs[kStripWords], g_strip_ours[kStripWords];

void SelfTest(void* const (&theirs)[kCount]) {
    constexpr unsigned kRounds = 24000;
    unsigned region_bytes = 0;
    for (const Region& r : kRegions) region_bytes += r.size;
    if (region_bytes != kRegionBytes) bof3::Fatal("frame_callees: regions are %u bytes, the state holds %u", region_bytes, kRegionBytes);
    for (unsigned d = 0; d < 256; ++d)
        if (Clut_PerRow[d] != 0) g_depths[g_depth_n++] = static_cast<unsigned char>(d);
    static State saved, input, their_out, our_out;
    Capture(saved);
    std::memcpy(g_strip_saved, Gfx_ClutStrip, sizeof g_strip_saved);
    unsigned long saved_kinds[kKinds], saved_states[kStates];
    std::memcpy(saved_kinds, Effect_KindHandlers, sizeof saved_kinds);
    std::memcpy(saved_states, CameraTurn_States, sizeof saved_states);
    FillKinds(Effect_KindHandlers, std::make_index_sequence<kKinds>());
    FillStates(CameraTurn_States, std::make_index_sequence<kStates>());
    g = kStubs;
    const Raw ours[kCount] = {
        reinterpret_cast<Raw>(&Effect_RunObjects), reinterpret_cast<Raw>(&Effect_CameraTurn),
        reinterpret_cast<Raw>(&CameraTurn_Start), reinterpret_cast<Raw>(&CameraTurn_Step),
        reinterpret_cast<Raw>(&CameraTurn_End), reinterpret_cast<Raw>(&Field_RunSlots),
        reinterpret_cast<Raw>(&MoveScript_TintFrame), reinterpret_cast<Raw>(&Party_UpdateScreens),
    };

    unsigned bad = 0, calls = 0, per[kCount] = {}, active_tints = 0, done_turns = 0, words_moved = 0;
    for (unsigned round = 0; round < kRounds; ++round) {
        const unsigned k = round % kCount;
        ++per[k];
        auto* bytes = reinterpret_cast<unsigned char*>(&input);
        for (unsigned i = 0; i < offsetof(State, log); ++i) bytes[i] = static_cast<unsigned char>(Next());
        input.current = Effect(Next());
        Apply(input);
        g_seed = Next();
        SeedCamera();
        for (unsigned i = 0; i < kEffects; ++i) SeedEffect(Effect(i));
        for (unsigned i = 0; i < kSlots; ++i) Field_Slots[i * kSlotBytes] = InUse();
        for (unsigned i = 0; i < 5; ++i) {
            Member(i)[0] = static_cast<unsigned char>(Next());
            if (Half()) Member(i)[0] &= static_cast<unsigned char>(~0x40u);
        }
        static const unsigned char kCounts[] = {0, 1, 2, 3, 3, 3, 4, 5, 0x80, 0xFF};
        Field_MemberCount = Pick(kCounts);
        Field_Request = static_cast<unsigned char>(Half() ? 3 : Next() % 5);
        for (unsigned i = 0; i < kTints; ++i) SeedTint(MoveScript_TintRecords + i * kTintBytes, i);
        if (k == kStart && Sprite_Current[9] == 0) Sprite_Current[9] = static_cast<unsigned char>(Half() ? 1 : 0x80);
        Capture(input);
        const bool ending = k == kStep && Sprite_Current[9] == 0;
        if (k == kTint)
            for (unsigned i = 0; i < kStripWords; ++i) g_strip_in[i] = static_cast<std::uint16_t>(Next());
        for (int pass = 0; pass < 2; ++pass) {
            Apply(input);
            if (k == kTint) std::memcpy(Gfx_ClutStrip, g_strip_in, sizeof g_strip_in);
            reinterpret_cast<Fn>(const_cast<void*>(pass ? ours[k] : theirs[k]))();
            Capture(pass ? our_out : their_out);
            if (k == kTint) std::memcpy(pass ? g_strip_ours : g_strip_theirs, Gfx_ClutStrip, sizeof g_strip_ours);
        }
        calls += their_out.log_n;
        done_turns += ending;
        if (k == kTint)
            for (unsigned i = 0; i < kStripWords; ++i) words_moved += g_strip_ours[i] != g_strip_in[i];
        const bool differ = std::memcmp(&their_out, &our_out, sizeof their_out) != 0 ||
                            (k == kTint && std::memcmp(g_strip_theirs, g_strip_ours, sizeof g_strip_ours) != 0);
        if (differ && ++bad <= 12) {
            unsigned first = 0;
            while (first < sizeof their_out && reinterpret_cast<unsigned char*>(&their_out)[first] == reinterpret_cast<unsigned char*>(&our_out)[first]) ++first;
            bof3::Log("shadow      frame_callees self-test MISMATCH: round %u, %s, log %u / %u, first difference at state byte 0x%X", round,
                      kClones[k].name, their_out.log_n, our_out.log_n, first);
        }
        if (k == kTint)
            for (unsigned i = 0; i < kTints; ++i) active_tints += input.memory[kEffects * kEffectBytes + kSlots * kSlotBytes + kMembersSpan +
                                                                               (0x7E0700 - kTintFrom) + i * kTintBytes] & 1;
    }
    g = kOriginals;
    std::memcpy(Effect_KindHandlers, saved_kinds, sizeof saved_kinds);
    std::memcpy(CameraTurn_States, saved_states, sizeof saved_states);
    std::memcpy(Gfx_ClutStrip, g_strip_saved, sizeof g_strip_saved);
    Apply(saved);
    bof3::Log("shadow      frame_callees self-test: %u rounds (%u per function), %u calls to the stand-ins, %u tint records "
              "run, %u strip words changed, %u turns ended, %u MISMATCHES; the effect objects, slots, members, tint records, camera words, "
              "flags, the strip and the stand-ins' log compared",
              kRounds, per[0], calls, active_tints, words_moved, done_turns, bad);
    if (bad) bof3::Fatal("the frame loop's callees differ from the original in %u of %u self-test rounds", bad, kRounds);
}

}  // namespace

void FrameCallees_Inject() {
    if (bof3::WantsShadow("frame_callees")) {
        void* clones[kCount];
        for (unsigned k = 0; k < kCount; ++k) {
            const Clone& c = kClones[k];
            bof3::CloneCall calls[2];
            for (int i = 0; i < c.n_calls; ++i) calls[i] = {c.calls[i].offset, StubFor(c.calls[i].target)};
            clones[k] = bof3::CloneOriginal(c.name, c.base, c.size, calls, c.n_calls);
        }
        SelfTest(clones);
    }
    BOF3_INJECT(Effect_RunObjects);
    BOF3_INJECT(Effect_CameraTurn);
    BOF3_INJECT(CameraTurn_Start);
    BOF3_INJECT(CameraTurn_Step);
    BOF3_INJECT(CameraTurn_End);
    BOF3_INJECT(Field_RunSlots);
    BOF3_INJECT(MoveScript_TintFrame);
    BOF3_INJECT(Party_UpdateScreens);
}

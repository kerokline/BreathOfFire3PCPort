// BOF3X_SHADOW=battle_win_states: a differential fuzz of the battle windows'
// states, once at start-up. docs/battle_win_states.md section 3.
//
// Nineteen byte-copies, every call out re-aimed at a recording stand-in (for
// ours through battle_win_states::g alike): the relative calls and tail
// jumps through CloneCall, and every immediate of the seven stack tables
// (`mov [esp + 4 i], imm32`, and 0x5971B0's `mov eax, imm32`) aimed at a
// numbered recording handler. One round: one function, random bytes in every
// region any of them reads or writes, then that function's branch boundaries
// seeded; the copy, then from the same state ours; the regions, our window
// records and target bytes and the stand-ins' log compared. None returns a
// value its callers read (Field_RunTaskRecords and the dispatchers ignore
// eax), so no answer is compared.
//
// The stand-ins do what their callers read back: most disturb a cell some
// caller reads again after the call - the current window pointer and its
// fields (the slot byte +0xA only within the round's range, so no address
// computed from it leaves the regions), the target pointer and its byte, the
// choosing byte, the selection and growths, the enemies' and members' HP and
// status words, the banner pool, the label gate.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/battle_win_states_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"
#include <windows.h>

namespace battle_win_states {
namespace {

using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

std::uint32_t Address(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }

// --- the random source, our buffers, the stand-ins' log ---------------------

std::uint32_t g_rng = 0x6A09E667u;
std::uint32_t Next() { g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5; return g_rng; }
bool Half() { return (Next() & 1) != 0; }
bool Often() { return Next() % 3 != 0; }

constexpr unsigned kWindows = 3, kWindowBytes = 0x40;
constexpr unsigned kTargets = 8;

struct Buffers {
    unsigned char windows[kWindows][kWindowBytes];
    unsigned char targets[kTargets];
};
Buffers g_buf;

constexpr unsigned kLog = 48;
struct Entry { std::uint32_t what, a, b, c, d, e, f, h, i; };
Entry g_log[kLog];
unsigned g_log_n;
std::uint32_t g_seed;   // the stand-ins' own stream: the same on both passes
unsigned g_slot_lo, g_slot_n;   // the round's range for the windows' slot byte +0xA

std::uint32_t Hash() {
    std::uint32_t h = (g_seed + g_log_n * 0x2545F491u) * 0x9E3779B1u;
    h ^= h >> 15;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    return h;
}
bool g_tracing;
void Record(std::uint32_t what, std::uint32_t a = 0, std::uint32_t b = 0, std::uint32_t c = 0, std::uint32_t d = 0,
            std::uint32_t e = 0, std::uint32_t f = 0, std::uint32_t h = 0, std::uint32_t i = 0) {
    if (g_tracing) bof3::Log("battle_win_states trace:   %u (%X %X %X %X %X %X %X %X)", what, a, b, c, d, e, f, h, i);
    if (g_log_n < kLog) g_log[g_log_n] = {what, a, b, c, d, e, f, h, i};
    ++g_log_n;
}

// A pointer as both passes can compare it: an offset inside our buffers,
// else the address.
std::uint32_t Id(const void* p) {
    const std::uint32_t at = Address(p), base = Address(&g_buf);
    if (at >= base && at < base + sizeof g_buf) return 0x1000000u + (at - base);
    return at;
}
std::uint32_t IdOf(std::uint32_t a) { return Id(reinterpret_cast<const void*>(static_cast<std::uintptr_t>(a))); }

unsigned char* Current() { return At(static_cast<std::uint32_t>(Long(At(at::kCurrent)))); }
unsigned char* Enemy(unsigned e) { return At(at::kEnemies + at::kEnemyStride * e); }
unsigned char* Member(unsigned m) { return At(at::kMembers + at::kMemberStride * m); }
unsigned char Slot(std::uint32_t v) { return static_cast<unsigned char>(g_slot_lo + v % g_slot_n); }

// A target byte: this window's slot, with or without a side bit, a side
// alone, or anything.
unsigned char TargetByte(std::uint32_t v, std::uint32_t h) {
    const unsigned char s = Slot(v);
    switch (h % 7) {
    case 0: case 1: return s;
    case 2: return 0x40;
    case 3: return 0x80;
    case 4: return static_cast<unsigned char>(s | ((h & 0x100) ? 0x40 : 0x80));
    case 5: return Slot(v + 1);
    default: return static_cast<unsigned char>(h >> 16);
    }
}

// --- the cells the functions read again after a call ------------------------

void Disturb() {
    const std::uint32_t h = Hash();
    if (h % 4 == 0) return;
    const unsigned v = (h >> 12) & 0xFF;
    const auto byte = static_cast<unsigned char>(h >> 24);
    switch ((h >> 4) % 14) {
    case 0: SetLong(At(at::kCurrent), static_cast<std::int32_t>(Address(g_buf.windows[v % kWindows]))); break;
    case 1:
    case 2: {
        unsigned char* const w = g_buf.windows[v % kWindows];
        const unsigned field = 2 + (h >> 20) % 0x1E;
        w[field] = field == 0xA ? Slot(h >> 8) : byte;
        break;
    }
    case 3: At(at::kChoosing)[0] = static_cast<unsigned char>((h & 0x40000000u) ? 0 : byte | 1); break;
    case 4: {
        unsigned char* const t = At(static_cast<std::uint32_t>(Long(At(at::kTarget))));
        *t = TargetByte(v, h >> 8);
        break;
    }
    case 5: SetLong(At(at::kTarget), static_cast<std::int32_t>(Address(g_buf.targets + v % kTargets))); break;
    case 6: At(at::kCrossSel)[0] = static_cast<unsigned char>(v % 8); break;
    case 7: At(at::kCrossGrow + v % 7)[0] = static_cast<unsigned char>((h & 0x40000000u) ? 0 : byte); break;
    case 8: {
        static const unsigned kFields[] = {0x12, 0x13, 0x24, 0x25, 0x30, 0x31};
        Enemy(v % 6)[kFields[(h >> 20) % 6]] = byte;
        break;
    }
    case 9: {
        static const unsigned kFields[] = {0x90, 0x91, 0x98, 0x99, 0x9A, 0x9B, 0xA0, 0xA1, 0xA2, 0xA3};
        Member(v % 5)[kFields[(h >> 20) % 10]] = byte;
        break;
    }
    case 10: At(at::kBanners + (h >> 20) % 0x60)[0] = byte; break;
    case 11: At(at::kLabelGate)[0] = static_cast<unsigned char>((h & 0x40000000u) ? 0 : byte); break;
    default: break;
    }
}

// --- the stand-ins ----------------------------------------------------------
// Each records what the real callee reads of its arguments: coordinates their
// low words, slots, icons and sizes their low bytes; what ours and the
// original both compute whole (the banner text's colour, the counts, the
// icons' shade, the gauge steps' pointers) whole.

std::uint32_t Lo16(int v) { return static_cast<std::uint32_t>(v) & 0xFFFF; }

void __cdecl StubPartyStatus(int x, int y) { Record(1, Lo16(x), Lo16(y)); Disturb(); }
void __cdecl StubCross(int x, int y) { Record(2, Lo16(x), Lo16(y)); Disturb(); }
void __cdecl StubLabel(unsigned k) { Record(3, k & 0xFF); Disturb(); }
void __cdecl StubSmallBox(int x, int y) { Record(4, Lo16(x), Lo16(y)); Disturb(); }
void __cdecl StubMediumBox(int x, int y) { Record(5, Lo16(x), Lo16(y)); Disturb(); }
void __cdecl StubTargetEnemy(int x, int y, unsigned t) { Record(6, Lo16(x), Lo16(y), t & 0xFF); Disturb(); }
void __cdecl StubEnemyStatus(int x, int y, unsigned t) { Record(7, Lo16(x), Lo16(y), t & 0xFF); Disturb(); }
void __cdecl StubTargetMember(int x, int y, unsigned m) { Record(8, Lo16(x), Lo16(y), m & 0xFF); Disturb(); }
// The target byte is read again after the first advance: moved half the time.
void __cdecl StubFlagAdvance() {
    Record(9, Id(Current()));
    Disturb();
    const std::uint32_t h = Hash() * 0x27D4EB2Fu;
    if (h & 0x10000) *At(static_cast<std::uint32_t>(Long(At(at::kTarget)))) = TargetByte(h >> 8, h >> 20);
}
void __cdecl StubRestoreBack() { Record(10, Id(Current())); Disturb(); }
unsigned __cdecl StubDispatchKind(unsigned a0, unsigned a1, unsigned a2, unsigned a3, unsigned a4, unsigned a5,
                                  unsigned a6, unsigned a7) {
    Record(11, IdOf(a0), IdOf(a1), IdOf(a2), IdOf(a3), IdOf(a4), IdOf(a5), IdOf(a6), IdOf(a7));
    Disturb();
    return Hash();
}
unsigned char __cdecl StubGlyphCount(const unsigned char* text) {
    Record(12, Id(text));   // never read: the pointer is a random word of the pool
    Disturb();
    const std::uint32_t h = Hash();
    return static_cast<unsigned char>(h % 3 != 0 ? h % 20 : h >> 8);   // the stand-ins' stream only
}
const unsigned char* __cdecl StubDrawAt(int x, int y, int colour, int count, const unsigned char* text) {
    Record(13, Lo16(x), Lo16(y), static_cast<std::uint32_t>(colour), static_cast<std::uint32_t>(count), Id(text));
    Disturb();
    return text;
}
void __cdecl StubIcon(unsigned icon, unsigned x, unsigned y, unsigned w, unsigned h, unsigned shade) {
    Record(14, icon & 0xFF, x & 0xFFFF, y & 0xFFFF, w & 0xFF, h & 0xFF, shade);
    Disturb();
}
unsigned char __cdecl StubActorIsOut(unsigned actor) {
    Record(15, actor & 0xFF);
    Disturb();
    const std::uint32_t h = Hash();
    return static_cast<unsigned char>(h % 3 == 0 ? 0 : h | 1);
}
unsigned char __cdecl StubReturnTrue() {
    Record(16);
    Disturb();
    const std::uint32_t h = Hash();
    return static_cast<unsigned char>(h % 2 == 0 ? 0 : h | 1);
}

// The stack tables' handlers: one numbered stand-in per slot (0x5971B0's
// slots 0 and 2 share one immediate, so one stand-in).
constexpr unsigned kSlots = 26;
template <unsigned N> void __cdecl StubSlot() { Record(200 + N, Id(Current())); Disturb(); }
using SlotFn = void (__cdecl*)();
template <unsigned... I> struct SlotList { static constexpr SlotFn f[sizeof...(I)] = {&StubSlot<I>...}; };
using Slots = SlotList<0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25>;
std::uint32_t SlotAt(unsigned n) { return Address(reinterpret_cast<const void*>(Slots::f[n])); }

const void* StubFor(std::uint32_t target) {
    const auto f = [](auto p) { return reinterpret_cast<const void*>(p); };
    switch (target) {
    case 0x442FA0: return f(&StubPartyStatus);
    case 0x4432F0: return f(&StubCross);
    case 0x4439A0: return f(&StubLabel);
    case 0x443740: return f(&StubSmallBox);
    case 0x443870: return f(&StubMediumBox);
    case 0x443B10: return f(&StubTargetEnemy);
    case 0x443D90: return f(&StubEnemyStatus);
    case 0x443F60: return f(&StubTargetMember);
    case 0x5979E0: return f(&StubFlagAdvance);
    case 0x597A00: return f(&StubRestoreBack);
    case 0x597A30: return f(&StubDispatchKind);
    case 0x597F40: return f(&StubGlyphCount);
    case 0x516B30: return f(&StubDrawAt);
    case 0x5903F0: return f(&StubIcon);
    case 0x4456C0: return f(&StubActorIsOut);
    case 0x449E00: return f(&StubReturnTrue);
    default: bof3::Fatal("battle_win_states: no stand-in for a call to 0x%X", static_cast<unsigned>(target)); return nullptr;
    }
}

Callees Stubs() {
    Callees s{};
    s.party_status = StubPartyStatus;
    s.command_cross = StubCross;
    s.command_label = StubLabel;
    s.small_box = StubSmallBox;
    s.medium_box = StubMediumBox;
    s.target_enemy = StubTargetEnemy;
    s.enemy_status = StubEnemyStatus;
    s.target_member = StubTargetMember;
    s.flag_advance = StubFlagAdvance;
    s.restore_back = StubRestoreBack;
    s.dispatch_kind = StubDispatchKind;
    s.glyph_count = StubGlyphCount;
    s.text_draw_at = StubDrawAt;
    s.icon = StubIcon;
    s.actor_is_out = StubActorIsOut;
    s.return_true = StubReturnTrue;
    for (unsigned i = 0; i < 8; ++i) s.kinds[i] = SlotAt(i);
    for (unsigned i = 0; i < 3; ++i) s.party[i] = SlotAt(8 + i);
    for (unsigned i = 0; i < 3; ++i) s.cross[i] = SlotAt(11 + i);
    s.label[0] = SlotAt(14);
    s.label[1] = SlotAt(15);
    s.label[2] = SlotAt(14);
    for (unsigned i = 0; i < 3; ++i) s.banner[i] = SlotAt(16 + i);
    for (unsigned i = 0; i < 4; ++i) s.enemy[i] = SlotAt(19 + i);
    for (unsigned i = 0; i < 3; ++i) s.member[i] = SlotAt(23 + i);
    return s;
}

// --- the nineteen copies (capstone, 2026-09-25: every jump internal but the
// listed calls and tail jumps; the stack tables' immediates below) -----------

struct Call { std::uint32_t offset, target; };
struct Imm { std::uint32_t offset, value; unsigned slot; };
struct Clone {
    const char* name;
    std::uint32_t base, size;
    const Call* calls;
    int n_calls;
    const Imm* imms;
    int n_imms;
    const void* ours;
};

constexpr Imm kI596FA0[] = {{0xF, 0x597000, 0}, {0x17, 0x597090, 1}, {0x22, 0x5971B0, 2}, {0x2A, 0x597200, 3},
                            {0x32, 0x597320, 4}, {0x3A, 0x5975D0, 5}, {0x42, 0x597C70, 6}, {0x4A, 0x597D50, 7}};
constexpr Imm kI597000[] = {{0xF, 0x597030, 8}, {0x17, 0x597070, 9}, {0x22, 0x437CC0, 10}};
constexpr Imm kI597090[] = {{0xF, 0x5970C0, 11}, {0x17, 0x597160, 12}, {0x22, 0x437CC0, 13}};
constexpr Imm kI5971B0[] = {{0xA, 0x437CC0, 14}, {0x1C, 0x5971E0, 15}};
constexpr Imm kI597200[] = {{0xF, 0x437CC0, 16}, {0x17, 0x597230, 17}, {0x22, 0x437CC0, 18}};
constexpr Imm kI597320[] = {{0xF, 0x597400, 19}, {0x17, 0x5974C0, 20}, {0x22, 0x597510, 21}, {0x2A, 0x437CC0, 22}};
constexpr Imm kI5975D0[] = {{0xF, 0x5976D0, 23}, {0x17, 0x597850, 24}, {0x22, 0x5978B0, 25}};

constexpr Call k597030[] = {{0x36, 0x442FA0}};
constexpr Call k597070[] = {{0xF, 0x442FA0}};
constexpr Call k5970C0[] = {{0x4C, 0x5903F0}};
constexpr Call k597160[] = {{0x39, 0x4432F0}};
constexpr Call k5971E0[] = {{0xF, 0x4439A0}};
constexpr Call k597230[] = {{0x17, 0x597F40}, {0x47, 0x443870}, {0x8B, 0x516B30}, {0x96, 0x443740}, {0xDB, 0x516B30}};
constexpr Call k597320[] = {{0x6E, 0x597A30}, {0x7D, 0x4456C0}};
constexpr Call k5974C0[] = {{0x13, 0x443D90}, {0x38, 0x5979E0}, {0x48, 0x5979E0}};
constexpr Call k597510[] = {{0x83, 0x443B10}, {0xAC, 0x597A00}, {0xB7, 0x597A00}};
constexpr Call k5975D0[] = {{0x66, 0x597A30}, {0xA7, 0x597A30}};
constexpr Call k597850[] = {{0x12, 0x4456C0}, {0x1E, 0x449E00}, {0x27, 0x449E00}, {0x45, 0x5979E0}, {0x55, 0x5979E0}};
constexpr Call k5978B0[] = {{0x116, 0x443F60}};

#define N_(a) static_cast<int>(sizeof a / sizeof a[0])
#define CW_D(name, base, size, imms) {#name, base, size, nullptr, 0, imms, N_(imms), reinterpret_cast<const void*>(&::name)}
#define CW_C(name, base, size, calls) {#name, base, size, calls, N_(calls), nullptr, 0, reinterpret_cast<const void*>(&::name)}
#define CW_B(name, base, size, calls, imms) {#name, base, size, calls, N_(calls), imms, N_(imms), reinterpret_cast<const void*>(&::name)}
#define CW_P(name, base, size) {#name, base, size, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::name)}
const Clone kClones[] = {
    CW_D(BattleWin_Run, 0x596FA0, 0x56, kI596FA0),
    CW_D(BattleWin_PartyRowStates, 0x597000, 0x2E, kI597000),
    CW_C(BattleWin_PartyRowSlideIn, 0x597030, 0x3F, k597030),
    CW_C(BattleWin_PartyRowDraw, 0x597070, 0x18, k597070),
    CW_D(BattleWin_CrossStates, 0x597090, 0x2E, kI597090),
    CW_C(BattleWin_CrossGrow, 0x5970C0, 0x92, k5970C0),
    CW_C(BattleWin_CrossFrame, 0x597160, 0x42, k597160),
    CW_D(BattleWin_LabelStates, 0x5971B0, 0x2B, kI5971B0),
    CW_C(BattleWin_LabelFrame, 0x5971E0, 0x16, k5971E0),
    CW_D(BattleWin_BannerStates, 0x597200, 0x2E, kI597200),
    CW_C(BattleWin_BannerFrame, 0x597230, 0xE5, k597230),
    CW_B(BattleWin_EnemyGaugeStates, 0x597320, 0xDC, k597320, kI597320),
    CW_P(BattleWin_EnemyGaugeOpen, 0x597400, 0xB8),
    CW_C(BattleWin_EnemyGaugeFrame, 0x5974C0, 0x4E, k5974C0),
    CW_C(BattleWin_EnemyTargetFrame, 0x597510, 0xBE, k597510),
    CW_B(BattleWin_MemberGaugeStates, 0x5975D0, 0xF2, k5975D0, kI5975D0),
    CW_P(BattleWin_MemberGaugeOpen, 0x5976D0, 0x172),
    CW_C(BattleWin_MemberGaugeWait, 0x597850, 0x5B, k597850),
    CW_C(BattleWin_MemberTargetFrame, 0x5978B0, 0x124, k5978B0),
};
#undef CW_D
#undef CW_C
#undef CW_B
#undef CW_P
#undef N_
constexpr unsigned kCount = sizeof kClones / sizeof kClones[0];

enum : unsigned {
    kRun, kPartyStates, kSlideIn, kRowDraw, kCrossStates, kCrossGrow, kCrossFrame, kLabelStates, kLabelFrame,
    kBannerStates, kBannerFrame, kEnemyStates, kEnemyOpen, kEnemyFrame, kEnemyTarget, kMemberStates, kMemberOpen,
    kMemberWait, kMemberTarget,
};
static_assert(kMemberTarget + 1 == kCount, "the index list and the clone list disagree");

// --- the state both passes start from ---------------------------------------

struct Region { std::uint32_t at, size; };
const Region kRegions[] = {
    {0x802D40, 0x700},              // the party objects (five)
    {0x903A50, 0x10},               // the label gate 0x903A5D
    {0x904AA0, 0x50},               // the battle's bytes: choosing, the cross's selection and growths
    {0x905B80, 8},                  // the current window record pointer
    {0x939FA0, 4},                  // the target pointer
    {0x93B8E0, 0x60},               // the banner pool, eight records
    {0x93B960, 0x80 + 6 * 0x128},   // the enemy objects' bytes before the first record, six records
};
constexpr unsigned kRegionBytes = 0x700 + 0x10 + 0x50 + 8 + 4 + 0x60 + 0x80 + 6 * 0x128;

struct State {
    unsigned char memory[kRegionBytes];
    Buffers buf;
    Entry log[kLog];
    unsigned log_n;
};
void Capture(State& s) {
    unsigned at = 0;
    for (const Region& r : kRegions) {
        std::memcpy(s.memory + at, At(r.at), r.size);
        at += r.size;
    }
    std::memcpy(&s.buf, &g_buf, sizeof g_buf);
    std::memcpy(s.log, g_log, sizeof s.log);
    s.log_n = g_log_n;
}
void Apply(const State& s) {
    unsigned at = 0;
    for (const Region& r : kRegions) {
        std::memcpy(At(r.at), s.memory + at, r.size);
        at += r.size;
    }
    std::memcpy(&g_buf, &s.buf, sizeof g_buf);
    std::memset(g_log, 0, sizeof g_log);
    g_log_n = 0;
}

// The slot byte's range a function's addresses stay inside the regions for.
void SlotRange(unsigned k) {
    switch (k) {
    case kBannerFrame: g_slot_lo = 0; g_slot_n = 8; break;                  // the eight banner records
    case kEnemyStates: case kEnemyOpen: case kEnemyFrame: case kEnemyTarget:
        g_slot_lo = 3; g_slot_n = 6; break;                                  // enemies 0..5
    case kMemberStates: case kMemberOpen: case kMemberWait: case kMemberTarget:
        g_slot_lo = 0; g_slot_n = 5; break;                                  // the five objects
    default: g_slot_lo = 0; g_slot_n = 9; break;                             // not used as an index
    }
}

void Fix(unsigned k) {
    SlotRange(k);
    for (auto& w : g_buf.windows) w[0xA] = Slot(Next());
    SetLong(At(at::kCurrent), static_cast<std::int32_t>(Address(g_buf.windows[Next() % kWindows])));
    SetLong(At(at::kTarget), static_cast<std::int32_t>(Address(g_buf.targets + Next() % kTargets)));
    for (auto& t : g_buf.targets) t = TargetByte(Next(), Next());
    At(at::kChoosing)[0] = static_cast<unsigned char>(Half() ? 0 : Next() | 1);
    // no gauge top of 0: the original faults on it (section 4), ours aborts
    for (unsigned e = 0; e < 6; ++e)
        if (Word(Enemy(e) + 0x30) == 0) SetWord(Enemy(e) + 0x30, 1 + Next() % 0xFFFF);
    for (unsigned m = 0; m < 5; ++m)
        if (Word(Member(m) + 0xA0) == 0) SetWord(Member(m) + 0xA0, 1 + Next() % 0xFFFF);
}

// A value against its top: 0, the gauge's floor (55 v < top), a quarter
// either side, the top, past it, or anything.
unsigned Against(unsigned top) {
    static const int kNear[] = {-1, 0, 1};
    switch (Next() % 8) {
    case 0: return 0;
    case 1: return Next() % (top / 55 + 1);
    case 2: case 3: return static_cast<unsigned>(static_cast<int>(top >> 2) + kNear[Next() % 3]) & 0xFFFF;
    case 4: return top;
    case 5: return (top + 1 + Next() % 0x100) & 0xFFFF;
    default: return Next() & 0xFFFF;
    }
}
unsigned Top() { return Often() ? 1 + Next() % 999 : Half() ? 1 + Next() % 4 : 1 + Next() % 0xFFFF; }

void SeedEnemy(unsigned e) {
    unsigned char* const r = Enemy(e);
    const unsigned top = Top();
    SetWord(r + 0x30, top);
    SetWord(r + 0x24, Against(top));
    SetWord(r + 0x12, Next() & 0xFFFF);
    At(at::kEnemies + at::kEnemyStride * e - 0x78)[0] = static_cast<unsigned char>(Often() ? Next() % 5 : Next());
}
void SeedMember(unsigned m, bool ap_top_zero_often) {
    unsigned char* const r = Member(m);
    const unsigned top = Top();
    SetWord(r + 0xA0, top);
    SetWord(r + 0x98, Against(top));
    const unsigned ap = ap_top_zero_often && Half() ? 0 : Top();
    SetWord(r + 0xA2, ap);
    SetWord(r + 0x9A, ap ? Against(ap) : Next() & 0xFFFF);
    SetWord(r + 0x90, Next() & 0xFFFF);
    r[8] = static_cast<unsigned char>(Often() ? Next() % 4 : Next() % 6);
    r[0x89] = static_cast<unsigned char>(Often() ? Next() % 11 : Next());
}

void Seed(unsigned k) {
    unsigned char* const w = Current();
    switch (k) {
    case kRun: w[2] = static_cast<unsigned char>(Next() % 8); break;
    case kPartyStates: case kCrossStates: case kLabelStates: case kBannerStates: case kMemberStates:
        w[3] = static_cast<unsigned char>(Next() % 3);
        break;
    case kEnemyStates: w[3] = static_cast<unsigned char>(Next() % 4); break;
    default: break;
    }
    switch (k) {
    case kSlideIn: {
        static const unsigned kY[] = {0xC4, 0xC7, 0xC8, 0xC9, 0xCB, 0xCC, 0xCD, 0x100, 0x7FFF, 0x8000, 0xFFFF, 0};
        SetWord(w + 6, Often() ? kY[Next() % 12] : Next() & 0xFFFF);
        break;
    }
    case kCrossGrow:
        for (unsigned j = 0; j < 7; ++j)
            At(at::kCrossGrow + j)[0] = static_cast<unsigned char>(Half() ? 0 : Often() ? Next() % 0x12 : Next());
        break;
    case kCrossFrame: {
        static const unsigned char kGrow[] = {0, 1, 2, 6, 7, 8, 9, 10};
        At(at::kCrossSel)[0] = static_cast<unsigned char>(Often() ? Next() % 8 : Next());
        for (unsigned j = 0; j < 7; ++j)
            At(at::kCrossGrow + j)[0] = Often() ? kGrow[Next() % 8] : static_cast<unsigned char>(Next());
        break;
    }
    case kLabelFrame: At(at::kLabelGate)[0] = static_cast<unsigned char>(Half() ? 0 : Next()); break;
    case kBannerFrame:
        for (unsigned b = 0; b < 8; ++b) At(at::kBanners + at::kBannerStride * b + 2)[0] = static_cast<unsigned char>(Half() ? 0 : Next());
        break;
    case kEnemyStates: case kEnemyOpen: case kEnemyFrame: case kEnemyTarget:
        for (unsigned e = 0; e < 6; ++e) SeedEnemy(e);
        for (auto& x : g_buf.windows) x[0xD] = static_cast<unsigned char>(Half() ? 0 : Next());
        if (k == kEnemyOpen) w[0xB] = static_cast<unsigned char>(Next());
        break;
    case kMemberStates: case kMemberOpen: case kMemberWait: case kMemberTarget:
        for (unsigned m = 0; m < 5; ++m) SeedMember(m, k == kMemberOpen);
        break;
    default: break;
    }
}

struct Coverage { unsigned logged[256]; } g_cover;

}  // namespace

void SelfTest() {
    constexpr unsigned kPerFunction = 3000;
    unsigned region_bytes = 0;
    for (const Region& r : kRegions) region_bytes += r.size;
    if (region_bytes != kRegionBytes)
        bof3::Fatal("battle_win_states: the regions are %u bytes, the state holds %u", region_bytes, kRegionBytes);

    void* clones[kCount];
    for (unsigned k = 0; k < kCount; ++k) {
        const Clone& c = kClones[k];
        bof3::CloneCall calls[8];
        if (c.n_calls > 8) bof3::Fatal("battle_win_states: %s has %d calls", c.name, c.n_calls);
        for (int i = 0; i < c.n_calls; ++i) calls[i] = {c.calls[i].offset, StubFor(c.calls[i].target), c.calls[i].target};
        clones[k] = bof3::CloneOriginal(c.name, c.base, c.size, calls, c.n_calls);
        auto* const code = static_cast<unsigned char*>(clones[k]);
        for (int i = 0; i < c.n_imms; ++i) {
            std::uint32_t had;
            std::memcpy(&had, code + c.imms[i].offset, sizeof had);
            if (had != c.imms[i].value)
                bof3::Fatal("battle_win_states: %s +0x%X holds 0x%X, not the handler 0x%X", c.name,
                            static_cast<unsigned>(c.imms[i].offset), static_cast<unsigned>(had),
                            static_cast<unsigned>(c.imms[i].value));
            const std::uint32_t to = SlotAt(c.imms[i].slot);
            std::memcpy(code + c.imms[i].offset, &to, sizeof to);
        }
    }

    static State saved, input, their_out, our_out;
    Capture(saved);
    const Buffers saved_buf = g_buf;
    const Callees kept = g;
    g = Stubs();
    char trace_env[16] = {};
    unsigned trace_round = ~0u;
    if (GetEnvironmentVariableA("BOF3X_BWS_TRACE", trace_env, sizeof trace_env) != 0) {
        trace_round = 0;
        for (const char* p = trace_env; *p >= '0' && *p <= '9'; ++p) trace_round = trace_round * 10 + (*p - '0');
    }

    unsigned bad = 0, calls = 0, bad_per[kCount] = {};
    for (unsigned round = 0; round < kPerFunction * kCount; ++round) {
        const unsigned k = round % kCount;
        for (unsigned i = 0; i < kRegionBytes; i += 4) {
            const std::uint32_t v = Next();
            std::memcpy(input.memory + i, &v, kRegionBytes - i < 4 ? kRegionBytes - i : 4);
        }
        for (unsigned i = 0; i < sizeof input.buf; i += 4) {
            const std::uint32_t v = Next();
            std::memcpy(reinterpret_cast<unsigned char*>(&input.buf) + i, &v, sizeof input.buf - i < 4 ? sizeof input.buf - i : 4);
        }
        std::memset(input.log, 0, sizeof input.log);
        input.log_n = 0;
        Apply(input);
        Fix(k);
        g_seed = Next();
        Seed(k);
        Capture(input);

        for (int pass = 0; pass < 2; ++pass) {
            g_tracing = round == trace_round;
            if (g_tracing) bof3::Log("battle_win_states trace: round %u %s pass %d", round, kClones[k].name, pass);
            Apply(input);
            State& out = pass ? our_out : their_out;
            const void* const fn = pass ? kClones[k].ours : clones[k];
            reinterpret_cast<void (__cdecl*)()>(const_cast<void*>(fn))();
            Capture(out);
        }
        calls += their_out.log_n;
        for (unsigned i = 0; i < their_out.log_n && i < kLog; ++i) ++g_cover.logged[their_out.log[i].what & 0xFF];
        if (std::memcmp(&their_out, &our_out, sizeof their_out) != 0) {
            ++bad_per[k];
            if (++bad <= 12) {
                unsigned first = 0;
                while (first < sizeof their_out &&
                       reinterpret_cast<const unsigned char*>(&their_out)[first] ==
                           reinterpret_cast<const unsigned char*>(&our_out)[first])
                    ++first;
                bof3::Log("shadow      battle_win_states self-test MISMATCH: round %u, %s, log %u / %u, first differing "
                          "state byte %u",
                          round, kClones[k].name, their_out.log_n, our_out.log_n, first);
                if (first < kRegionBytes) {
                    unsigned at = 0;
                    for (const Region& r : kRegions) {
                        if (first < at + r.size) {
                            bof3::Log("shadow      battle_win_states:   memory 0x%X: 0x%02X / 0x%02X", r.at + (first - at),
                                      their_out.memory[first], our_out.memory[first]);
                            break;
                        }
                        at += r.size;
                    }
                } else if (first < offsetof(State, log)) {
                    const unsigned o = first - static_cast<unsigned>(offsetof(State, buf));
                    bof3::Log("shadow      battle_win_states:   buffer +0x%X: 0x%02X / 0x%02X", o,
                              reinterpret_cast<const unsigned char*>(&their_out.buf)[o],
                              reinterpret_cast<const unsigned char*>(&our_out.buf)[o]);
                } else if (first < offsetof(State, log_n)) {
                    const unsigned e = (first - static_cast<unsigned>(offsetof(State, log))) / sizeof(Entry);
                    const Entry& a = their_out.log[e];
                    const Entry& b = our_out.log[e];
                    bof3::Log("shadow      battle_win_states:   log %u: %u (%X %X %X %X %X %X %X %X) / %u (%X %X %X %X %X %X %X %X)",
                              e, a.what, a.a, a.b, a.c, a.d, a.e, a.f, a.h, a.i, b.what, b.a, b.b, b.c, b.d, b.e, b.f,
                              b.h, b.i);
                }
            }
        }
    }
    g = kept;
    Apply(saved);
    g_buf = saved_buf;

    bof3::Log("shadow      battle_win_states self-test: %u rounds over %u functions (%u each), %u calls to the "
              "stand-ins, %u MISMATCHES; the party objects, the label gate, the battle's bytes, the current record "
              "and target pointers, the banner pool, the enemies, our window records and target bytes and the "
              "stand-ins' log compared",
              kPerFunction * kCount, kCount, kPerFunction, calls, bad);
    if (bad)
        for (unsigned k = 0; k < kCount; ++k)
            if (bad_per[k]) bof3::Log("shadow      battle_win_states: %s mismatched in %u rounds", kClones[k].name, bad_per[k]);
    const Coverage& c = g_cover;
    char slots[kSlots * 7 + 1] = {};
    unsigned n = 0;
    for (unsigned i = 0; i < kSlots; ++i) {
        unsigned v = c.logged[200 + i];
        char digits[8];
        unsigned d = 0;
        do { digits[d++] = static_cast<char>('0' + v % 10); v /= 10; } while (v && d < 7);
        while (d && n < sizeof slots - 2) slots[n++] = digits[--d];
        if (n < sizeof slots - 1) slots[n++] = i + 1 < kSlots ? ',' : '\0';
    }
    bof3::Log("shadow      battle_win_states coverage: party row %u, cross %u, label %u, small / medium box %u / %u, "
              "target enemy %u, enemy status %u, target member %u, advance %u, back %u, gauge steps %u, glyph counts "
              "%u, texts %u, icons %u, out tests %u, true %u; table slots %s",
              c.logged[1], c.logged[2], c.logged[3], c.logged[4], c.logged[5], c.logged[6], c.logged[7], c.logged[8],
              c.logged[9], c.logged[10], c.logged[11], c.logged[12], c.logged[13], c.logged[14], c.logged[15],
              c.logged[16], slots);
    if (bad) bof3::Fatal("the battle windows' states differ from the original in %u self-test rounds", bad);
}

}  // namespace battle_win_states

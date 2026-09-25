// BOF3X_SHADOW=window_kinds: a differential fuzz of the window-kind handlers,
// once at start-up.
//
// Twelve byte-copies, every call out re-aimed at a recording stand-in (the two
// tail jumps included) and every stack-built dispatch table's immediates
// re-aimed in the copy - `mov [esp + k], imm32` carries an absolute address,
// so an unpatched copy runs the ORIGINAL states, which are what is under test.
// One round: one of the twelve, random bytes over every region any of them
// touches, the indices put back inside their tables, each branch's boundaries
// seeded; theirs, then from the same state ours; the regions, the gauge's two
// buffers and the stand-ins' log compared. docs/window_kinds.md section 4.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/window_kinds_callees.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace window_kinds {
namespace {

using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

std::uint32_t Address(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }

// --- the gauge's arguments -------------------------------------------------
// The eight pointers the enemy target window hands Window_DispatchKind point
// into a window record and an enemy record; here, into two buffers of ours at
// the same offsets.
unsigned char g_win[0x24];
unsigned char g_enemy[0x40];
struct GaugeArgs { std::uint32_t a[8]; };
GaugeArgs Gauge() {
    return {{Address(g_win + 0xB), Address(g_enemy + 0x30), Address(g_win + 0x1C), Address(g_win + 0x14),
             Address(g_enemy + 0x24), Address(g_win + 0xD), Address(g_win + 0x18), Address(g_win + 8)}};
}

// --- the stand-ins' log ----------------------------------------------------

constexpr unsigned kLog = 32;
struct Entry { std::uint32_t what, a, b, c, d; };
Entry g_log[kLog];
unsigned g_log_n, g_seed;

std::uint32_t Hash() {
    std::uint32_t h = (g_seed + g_log_n * 0x2545F491u) * 0x9E3779B1u;
    h ^= h >> 15;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    return h;
}
void Record(std::uint32_t what, std::uint32_t a = 0, std::uint32_t b = 0, std::uint32_t c = 0, std::uint32_t d = 0) {
    if (g_log_n < kLog) g_log[g_log_n] = {what, a, b, c, d};
    ++g_log_n;
}

// Every byte below is one some function reads again after a call, so a read
// moved across a call shows. Once in 23 the current record itself is
// repointed - the thing these functions re-read after every call out.
const std::uint32_t kWatch[] = {
    at::kMsgHead, at::kMsgTail, at::kMsgOpen, at::kBannerMask, 0x7E1BEC /* Input_Pressed */,
    at::kMsgRing, at::kMsgRing + 1, at::kMsgRing + 8, at::kMsgRing + 9, at::kScratch,
};
void Disturb() {
    const std::uint32_t h = Hash();
    if (h % 3 == 0) return;
    if (h % 23 == 0) {
        const std::uint32_t record = at::kRecords + ((h >> 8) % 22u) * 0x24u;
        SetLong(At(at::kCurrent), static_cast<std::int32_t>(record));
        return;
    }
    const std::uint32_t where = kWatch[(h >> 4) % (sizeof kWatch / sizeof kWatch[0])];
    std::uint32_t v = (h >> 12) & 0xFF;
    if (where == at::kMsgHead || where == at::kMsgTail) v &= 0xF;   // the ring's indices stay inside it
    At(where)[0] = static_cast<unsigned char>(v);
}

// --- the stand-ins ---------------------------------------------------------
// BattleWin_DrawMessageBox and Text_DrawAt keep only the low 16 bits of x and
// y (docs/battle_windows.md section 2; Text_DrawAt stores both as words), so
// that is what is recorded of them; the colour, the count and the text go on
// whole.

void __cdecl StubBox(int x, int y) {
    Record(1, static_cast<std::uint16_t>(x), static_cast<std::uint16_t>(y));
    Disturb();
}
const unsigned char* __cdecl StubText(int x, int y, int colour, int count, const unsigned char* text) {
    Record(2, static_cast<std::uint16_t>(x) | static_cast<std::uint32_t>(static_cast<std::uint16_t>(y)) << 16,
           static_cast<std::uint32_t>(colour), static_cast<std::uint32_t>(count), Address(text));
    Disturb();
    return text;
}
void __cdecl StubDrawMessage() {
    Record(3);
    Disturb();
}
// BattleMsg_Advance as it is: the read index on, 1 when it meets the write
// index - so that the caller's re-read of the index after it sees a change.
unsigned char __cdecl StubAdvance() {
    const auto head = static_cast<unsigned char>((At(at::kMsgHead)[0] + 1) & 0xF);
    At(at::kMsgHead)[0] = head;
    const unsigned char r = At(at::kMsgTail)[0] == head ? 1 : 0;
    Record(4, r);
    Disturb();
    return r;
}
// Window_FreeCurrent as it is: bytes 0, +2 and +3 of the current record.
void __cdecl StubFree() {
    Record(5);
    unsigned char* const w = At(static_cast<std::uint32_t>(Long(At(at::kCurrent))));
    w[0] = 0;
    w[2] = 0;
    w[3] = 0;
    Disturb();
}
template <unsigned Id_> void __cdecl StubHandler() {
    Record(Id_);
    Disturb();
}

const Callees kStubs = {
    StubBox,
    StubText,
    StubDrawMessage,
    StubAdvance,
    StubFree,
    {&StubHandler<10>, &StubHandler<11>, &StubHandler<12>},
    {&StubHandler<20>, &StubHandler<21>, &StubHandler<22>, &StubHandler<23>},
    {&StubHandler<30>, &StubHandler<31>, nullptr, nullptr, &StubHandler<34>, &StubHandler<35>},
};

// --- the copies ------------------------------------------------------------

const void* StubFor(std::uint32_t target) {
    const auto f = [](auto p) { return reinterpret_cast<const void*>(p); };
    switch (target) {
    case bof3::addr::BattleWin_DrawMessageBox: return f(&StubBox);
    case bof3::addr::Text_DrawAt:              return f(&StubText);
    case bof3::addr::BattleWin_DrawMessage:    return f(&StubDrawMessage);
    case bof3::addr::BattleMsg_Advance:        return f(&StubAdvance);
    case bof3::addr::Window_FreeCurrent:       return f(&StubFree);
    default: bof3::Fatal("window_kinds: no stand-in for a call to 0x%X", static_cast<unsigned>(target)); return nullptr;
    }
}

struct Imm { std::uint32_t offset, expected; Handler replacement; };
void PatchImm(void* copy, const char* name, const Imm* imms, int n) {
    auto* code = static_cast<std::uint8_t*>(copy);
    for (int i = 0; i < n; ++i) {
        std::uint32_t had;
        std::memcpy(&had, code + imms[i].offset, sizeof had);
        if (had != imms[i].expected)
            bof3::Fatal("window_kinds: %s +0x%X holds 0x%X, not the handler 0x%X", name,
                        static_cast<unsigned>(imms[i].offset), static_cast<unsigned>(had),
                        static_cast<unsigned>(imms[i].expected));
        const std::uint32_t to = Address(reinterpret_cast<const void*>(imms[i].replacement));
        std::memcpy(code + imms[i].offset, &to, sizeof to);
    }
}

struct Call { std::uint32_t offset, target; };
struct Clone {
    const char* name;
    std::uint32_t base, size;
    const Call* calls; int n_calls;
    const Imm* imms; int n_imms;
};

constexpr Call kBannerRunCalls[] = {{0x39, bof3::addr::BattleWin_DrawMessageBox}, {0x74, bof3::addr::Text_DrawAt}};
constexpr Call kBannerOutCalls[] = {{0x26, bof3::addr::Window_FreeCurrent}};
constexpr Call kMsgInCalls[] = {{0x22, bof3::addr::BattleWin_DrawMessage}};
constexpr Call kMsgShowCalls[] = {
    {0x00, bof3::addr::BattleWin_DrawMessage}, {0x20, bof3::addr::BattleMsg_Advance}, {0x5B, bof3::addr::BattleMsg_Advance}};
constexpr Call kMsgOutCalls[] = {{0x33, bof3::addr::BattleWin_DrawMessageBox}, {0x67, bof3::addr::Text_DrawAt}};

const Imm kBannerImms[] = {
    {0x0F, bof3::addr::BattleWin_BannerSlideIn, &StubHandler<10>},
    {0x17, kNop, &StubHandler<11>},
    {0x22, bof3::addr::BattleWin_BannerSlideOut, &StubHandler<12>},
};
const Imm kMessageImms[] = {
    {0x0F, bof3::addr::BattleWin_MessageWait, &StubHandler<20>},
    {0x17, bof3::addr::BattleWin_MessageSlideIn, &StubHandler<21>},
    {0x22, bof3::addr::BattleWin_MessageShow, &StubHandler<22>},
    {0x2A, bof3::addr::BattleWin_MessageSlideOut, &StubHandler<23>},
};
const Imm kResultImms[] = {
    {0x1A, kResultKinds[0], &StubHandler<30>},
    {0x22, kResultKinds[1], &StubHandler<31>},
    {0x2A, kResultKinds[4], &StubHandler<34>},
    {0x32, kResultKinds[5], &StubHandler<35>},
};

enum : unsigned {
    kTrack, kDrain, kFill, kBannerRun, kBannerIn, kBannerOut,
    kMsgRun, kMsgWait, kMsgIn, kMsgShow, kMsgOut, kHandler4, kCount
};

#define WK_C(name, base, size, calls) {name, base, size, calls, static_cast<int>(sizeof calls / sizeof calls[0]), nullptr, 0}
#define WK_P(name, base, size) {name, base, size, nullptr, 0, nullptr, 0}
const Clone kClones[kCount] = {
    WK_P("Window_HpGaugeTrack", 0x597A80, 0x14C),
    WK_P("Window_HpGaugeDrain", 0x597BD0, 0x40),
    WK_P("Window_HpGaugeFill", 0x597C10, 0x5F),
    {"BattleWin_BannerRun", 0x597C70, 0x7D, kBannerRunCalls, 2, kBannerImms, 3},
    WK_P("BattleWin_BannerSlideIn", 0x597CF0, 0x1B),
    WK_C("BattleWin_BannerSlideOut", 0x597D10, 0x33, kBannerOutCalls),
    {"BattleWin_MessageRun", 0x597D50, 0x36, nullptr, 0, kMessageImms, 4},
    WK_P("BattleWin_MessageWait", 0x597D90, 0x24),
    WK_C("BattleWin_MessageSlideIn", 0x597DC0, 0x27, kMsgInCalls),
    WK_C("BattleWin_MessageShow", 0x597DF0, 0x6F, kMsgShowCalls),
    WK_C("BattleWin_MessageSlideOut", 0x597E60, 0x70, kMsgOutCalls),
    {"Window_Handler4Kinds", 0x597F60, 0x3E, nullptr, 0, kResultImms, 4},
};
#undef WK_C
#undef WK_P

// --- the state both passes start from --------------------------------------

struct Region { std::uint32_t at, size; };
const Region kRegions[] = {
    {at::kRecords, 0x318},       // the 22 window records
    {at::kCurrent, 4},           // the record running (a pointer: Fix sets it)
    {at::kMsgHead, 2},           // the ring's read and write indices
    {at::kMsgRing, 0x80},        // the ring's 16 entries
    {at::kMsgOpen, 1},
    {at::kBannerCurrent, 4},     // a pointer: Fix sets it inside the pool
    {at::kBannerPool, 0x60},     // the banner pool's 8 entries
    {at::kBannerMask, 1},
    {0x7E1BEC, 2},               // Input_Pressed
    {at::kScratch, 1},
};
constexpr unsigned kRegionBytes = 0x318 + 4 + 2 + 0x80 + 1 + 4 + 0x60 + 1 + 2 + 1;

struct State {
    unsigned char memory[kRegionBytes];
    unsigned char win[sizeof g_win];
    unsigned char enemy[sizeof g_enemy];
    Entry log[kLog];
    unsigned log_n;
};
void Capture(State& s) {
    unsigned at = 0;
    for (const Region& r : kRegions) { std::memcpy(s.memory + at, At(r.at), r.size); at += r.size; }
    std::memcpy(s.win, g_win, sizeof g_win);
    std::memcpy(s.enemy, g_enemy, sizeof g_enemy);
    std::memcpy(s.log, g_log, sizeof s.log);
    s.log_n = g_log_n;
}
void Apply(const State& s) {
    unsigned at = 0;
    for (const Region& r : kRegions) { std::memcpy(At(r.at), s.memory + at, r.size); at += r.size; }
    std::memcpy(g_win, s.win, sizeof g_win);
    std::memcpy(g_enemy, s.enemy, sizeof g_enemy);
    std::memset(g_log, 0, sizeof g_log);
    g_log_n = 0;
}

std::uint32_t g_rng = 0x6D2B79F5u;
std::uint32_t Next() { g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5; return g_rng; }
bool Often() { return Next() % 3 != 0; }
bool Half() { return (Next() & 1) != 0; }

unsigned char* Current() { return At(static_cast<std::uint32_t>(Long(At(at::kCurrent)))); }

// The indices put back inside the ring, the pool pointer inside the pool.
void Fix() {
    SetLong(At(at::kCurrent), static_cast<std::int32_t>(at::kRecords + (Next() % 22u) * 0x24u));
    At(at::kMsgHead)[0] = static_cast<unsigned char>(Next() & 0xF);
    At(at::kMsgTail)[0] = static_cast<unsigned char>(Half() ? At(at::kMsgHead)[0] : Next() & 0xF);
    SetLong(At(at::kBannerCurrent), static_cast<std::int32_t>(at::kBannerPool + (Next() % 8u) * 0xCu));
}

// A y the slides compare, at and around both of their ends.
std::uint16_t SlideY() {
    static const std::uint16_t kY[] = {0x12, 0x0A, 0x1A, 0x02, 0xFFEA, 0xFFE2, 0xFFF2, 0xFFEB, 0x13, 0};
    return Often() ? kY[Next() % 10] : static_cast<std::uint16_t>(Next());
}

// A max HP: never 0 (the original divides by it), mostly an HP-like value.
std::uint16_t MaxHp() {
    static const std::uint16_t kMax[] = {1, 2, 55, 56, 100, 999, 9999, 0x7FFF, 0x8000, 0xFFFF};
    std::uint16_t v = Often() ? static_cast<std::uint16_t>(1 + Next() % 9999) : kMax[Next() % 10];
    return v == 0 ? 1 : v;
}
std::uint16_t Near(std::uint16_t v) {
    static const int kD[] = {0, 0, 1, -1, 2, -2, 55, -55};
    switch (Next() % 4) {
    case 0: return static_cast<std::uint16_t>(v + kD[Next() % 8]);
    case 1: return static_cast<std::uint16_t>(Next() % (static_cast<unsigned>(static_cast<short>(v) > 0 ? v : 1) + 1u));
    case 2: return 0;
    default: return static_cast<std::uint16_t>(Next());
    }
}

void SeedGauge(unsigned k) {
    const std::uint16_t max = MaxHp();
    SetWord(g_enemy + 0x30, max);
    const std::uint16_t hp = Near(max);
    SetWord(g_enemy + 0x24, hp);
    SetWord(g_win + 0x1C, Often() ? max : Near(max));
    SetWord(g_win + 0x14, Often() ? Near(hp) : hp);
    static const unsigned char kGauge[] = {0, 1, 2, 0x36, 0x37, 0x38, 0xFF, 0x10};
    g_win[0xB] = Often() ? static_cast<unsigned char>(Next() % 0x38) : kGauge[Next() % 8];
    g_win[0xD] = static_cast<unsigned char>(Next());
    static const std::uint16_t kStep[] = {0, 1, 2, 0x10, 0x37, 0xFF, 0x100, 0x7FFF, 0x8000, 0xFFFF};
    std::uint16_t step = Often() ? static_cast<std::uint16_t>(Next() % 0x20) : kStep[Next() % 10];
    if (k == kDrain && Half()) step = static_cast<std::uint16_t>(g_win[0xD] + static_cast<int>(Next() % 3) - 1);
    if (k == kFill && Half()) {
        // the step right at the edge of the fill test: target - step against the gauge
        const int target = static_cast<short>(hp) * 55 / static_cast<short>(max);
        step = static_cast<std::uint16_t>(target - g_win[0xB] + static_cast<int>(Next() % 3) - 1);
    }
    SetWord(g_win + 0x18, step);
    g_win[8] = static_cast<unsigned char>(Next() % 3);
}

void Seed(unsigned k) {
    unsigned char* const r = Current();
    switch (k) {
    case kTrack:
    case kDrain:
    case kFill:
        SeedGauge(k);
        break;
    case kBannerRun:
        r[3] = static_cast<unsigned char>(Next() % 3);
        // mostly one of the pool's eight; sometimes far past it, where the
        // colour argument carries 0xC n's upper bits
        r[0xA] = static_cast<unsigned char>(Often() ? Next() % 8 : Next());
        break;
    case kBannerIn:
    case kBannerOut:
    case kMsgIn:
    case kMsgOut:
        SetWord(r + 6, SlideY());
        break;
    case kMsgRun:
        r[3] = static_cast<unsigned char>(Next() % 4);
        break;
    case kMsgWait:
        if (Half()) At(at::kMsgTail)[0] = At(at::kMsgHead)[0];
        break;
    case kMsgShow: {
        const unsigned head = At(at::kMsgHead)[0];
        static const unsigned char kTimer[] = {0xFF, 1, 0, 2, 0xFE};
        for (unsigned i = 0; i < 2; ++i) {
            unsigned char* const e = At(at::kMsgRing + 8 * ((head + i) & 0xF));
            e[0] = static_cast<unsigned char>((e[0] & ~3u) | (Next() & 3));
            e[1] = Often() ? kTimer[Next() % 5] : static_cast<unsigned char>(Next());
        }
        SetWord(At(0x7E1BEC), Half() ? 0 : static_cast<std::uint16_t>(1u << (Next() % 16)));
        if (Half()) At(at::kMsgTail)[0] = static_cast<unsigned char>((head + 1 + Next() % 2) & 0xF);
        break;
    }
    case kHandler4: {
        static const unsigned char kKinds[] = {0, 1, 4, 5};
        r[2] = kKinds[Next() % 4];
        break;
    }
    default:
        break;
    }
}

// What the rounds reached, from the original's side, for the log line.
struct Coverage {
    unsigned max_changed, clamped, capped, to_drain, to_fill, settled, emptied;
    unsigned drain_on, drain_done, fill_on, fill_done;
    unsigned arrived, freed, wide_colour, opened, button, timer, closed;
} g_cover;
bool Logged(const State& s, std::uint32_t what) {
    for (unsigned i = 0; i < s.log_n && i < kLog; ++i)
        if (s.log[i].what == what) return true;
    return false;
}
unsigned CurrentSlot(const State& s) {
    const std::uint32_t record = static_cast<std::uint32_t>(Long(s.memory + 0x318));
    return (record - at::kRecords) / 0x24u;
}
void Cover(unsigned k, const State& in, const State& out) {
    const unsigned slot = CurrentSlot(in);
    const unsigned char* const rin = slot < 22 ? in.memory + slot * 0x24 : nullptr;
    const unsigned char* const rout = slot < 22 ? out.memory + slot * 0x24 : nullptr;
    switch (k) {
    case kTrack:
        if (Word(in.win + 0x1C) != Word(in.enemy + 0x30)) ++g_cover.max_changed;
        if (Word(out.enemy + 0x24) != Word(in.enemy + 0x24)) ++g_cover.clamped;
        if (out.win[0xB] == 0x37 && Word(out.win + 0x14) != Word(in.win + 0x14)) ++g_cover.capped;
        if (out.win[8] == 1 && in.win[8] != 1) ++g_cover.to_drain;
        else if (out.win[8] == 2 && in.win[8] != 2) ++g_cover.to_fill;
        else ++g_cover.settled;
        if (Word(out.enemy + 0x24) == 0 && out.win[8] == 1) ++g_cover.emptied;
        break;
    case kDrain:
        if (out.win[8] == 0 && in.win[8] != 0) ++g_cover.drain_done; else ++g_cover.drain_on;
        break;
    case kFill:
        if (out.win[8] == 0 && in.win[8] != 0) ++g_cover.fill_done; else ++g_cover.fill_on;
        break;
    case kBannerRun:
        for (unsigned i = 0; i < out.log_n && i < kLog; ++i)
            if (out.log[i].what == 2 && (out.log[i].b & ~0xFFu) != 0) { ++g_cover.wide_colour; break; }
        break;
    case kBannerIn:
    case kMsgIn:
        if (rin && rout && rout[3] != rin[3]) ++g_cover.arrived;
        break;
    case kBannerOut:
        if (Logged(out, 5)) ++g_cover.freed;
        break;
    case kMsgWait:
        if (rin && rout && rout[3] != rin[3]) ++g_cover.opened;
        break;
    case kMsgShow: {
        unsigned advances = 0;
        for (unsigned i = 0; i < out.log_n && i < kLog; ++i)
            if (out.log[i].what == 4) ++advances;
        const unsigned head = in.memory[0x318 + 4];
        const unsigned char* const e = in.memory + 0x318 + 4 + 2 + 8 * head;
        if (advances && (e[0] & 1) && Word(in.memory + kRegionBytes - 3) != 0) ++g_cover.button;
        else if (advances) ++g_cover.timer;
        break;
    }
    case kMsgOut:
        if (rin && Word(rin + 6) == 0xFFEA) ++g_cover.closed;
        break;
    default:
        break;
    }
}

using Fn8 = void (__cdecl*)(std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t,
                            std::uint32_t, std::uint32_t);

}  // namespace

void SelfTest() {
    constexpr unsigned kRounds = 24000;   // 2,000 rounds per function
    unsigned region_bytes = 0;
    for (const Region& r : kRegions) region_bytes += r.size;
    if (region_bytes != kRegionBytes)
        bof3::Fatal("window_kinds: the regions are %u bytes, the state holds %u", region_bytes, kRegionBytes);

    void* clones[kCount];
    for (unsigned k = 0; k < kCount; ++k) {
        const Clone& c = kClones[k];
        bof3::CloneCall calls[4];
        if (c.n_calls > 4) bof3::Fatal("window_kinds: %s has %d calls", c.name, c.n_calls);
        for (int i = 0; i < c.n_calls; ++i) calls[i] = {c.calls[i].offset, StubFor(c.calls[i].target)};
        clones[k] = bof3::CloneOriginal(c.name, c.base, c.size, calls, c.n_calls);
        if (c.n_imms) PatchImm(clones[k], c.name, c.imms, c.n_imms);
    }

    const void* const ours[kCount] = {
        reinterpret_cast<const void*>(&Window_HpGaugeTrack),       reinterpret_cast<const void*>(&Window_HpGaugeDrain),
        reinterpret_cast<const void*>(&Window_HpGaugeFill),        reinterpret_cast<const void*>(&BattleWin_BannerRun),
        reinterpret_cast<const void*>(&BattleWin_BannerSlideIn),   reinterpret_cast<const void*>(&BattleWin_BannerSlideOut),
        reinterpret_cast<const void*>(&BattleWin_MessageRun),      reinterpret_cast<const void*>(&BattleWin_MessageWait),
        reinterpret_cast<const void*>(&BattleWin_MessageSlideIn),  reinterpret_cast<const void*>(&BattleWin_MessageShow),
        reinterpret_cast<const void*>(&BattleWin_MessageSlideOut), reinterpret_cast<const void*>(&Window_Handler4Kinds)};

    static State saved, input, their_out, our_out;
    Capture(saved);
    g = kStubs;

    unsigned bad = 0, calls = 0, per[kCount] = {}, bad_per[kCount] = {};
    for (unsigned round = 0; round < kRounds; ++round) {
        const unsigned k = round % kCount;
        ++per[k];
        for (unsigned i = 0; i < kRegionBytes; ++i) input.memory[i] = static_cast<unsigned char>(Next());
        for (unsigned char& b : input.win) b = static_cast<unsigned char>(Next());
        for (unsigned char& b : input.enemy) b = static_cast<unsigned char>(Next());
        std::memset(input.log, 0, sizeof input.log);
        input.log_n = 0;
        Apply(input);
        Fix();
        Seed(k);
        g_seed = Next();
        Capture(input);
        const GaugeArgs args = Gauge();

        for (int pass = 0; pass < 2; ++pass) {
            Apply(input);
            State& out = pass ? our_out : their_out;
            const void* const fn = pass ? ours[k] : clones[k];
            reinterpret_cast<Fn8>(const_cast<void*>(fn))(args.a[0], args.a[1], args.a[2], args.a[3], args.a[4],
                                                         args.a[5], args.a[6], args.a[7]);
            Capture(out);
        }
        calls += their_out.log_n;
        Cover(k, input, their_out);
        if (std::memcmp(&their_out, &our_out, sizeof their_out) != 0) {
            ++bad_per[k];
            if (++bad <= 12)
                bof3::Log("shadow      window_kinds self-test MISMATCH: round %u, %s, log %u / %u", round,
                          kClones[k].name, their_out.log_n, our_out.log_n);
        }
    }
    g = kOriginals;
    Apply(saved);

    bof3::Log("shadow      window_kinds self-test: %u rounds over %u functions (%u each), %u calls to the stand-ins, "
              "%u MISMATCHES; the 22 window records, the current record, the message ring and its indices, 0x939F60, "
              "the banner pool, its pointer and mask, Input_Pressed, DamageScratch's byte, the gauge's two records "
              "and the stand-ins' log compared",
              kRounds, static_cast<unsigned>(kCount), per[0], calls, bad);
    if (bad)
        for (unsigned k = 0; k < kCount; ++k)
            if (bad_per[k]) bof3::Log("shadow      window_kinds: %s mismatched in %u rounds", kClones[k].name, bad_per[k]);
    const Coverage& c = g_cover;
    bof3::Log("shadow      window_kinds coverage: gauge max changed %u, HP clamped %u, capped at 0x37 %u; to drain %u, "
              "to fill %u, settled %u, emptied %u; drain on %u done %u; fill on %u done %u; slid in %u, banner freed %u, "
              "colour with upper bits %u; message opened %u, advanced by a button %u, by the timer %u, closed %u",
              c.max_changed, c.clamped, c.capped, c.to_drain, c.to_fill, c.settled, c.emptied, c.drain_on,
              c.drain_done, c.fill_on, c.fill_done, c.arrived, c.freed, c.wide_colour, c.opened, c.button, c.timer,
              c.closed);
    if (bad) bof3::Fatal("the window-kind handlers differ from the original in %u of %u self-test rounds", bad, kRounds);
}

}  // namespace window_kinds

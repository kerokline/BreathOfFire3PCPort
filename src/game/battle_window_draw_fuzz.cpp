// BOF3X_SHADOW=battle_window_draw: a differential fuzz of the battle windows'
// draw helpers, once at start-up. docs/battle_window_draw.md section 3.
//
// Nineteen byte-copies, every call out re-aimed at a recording stand-in (for
// ours through battle_window_draw::g alike). One round: one function, random
// bytes in every region any of them reads or writes, then that function's
// branch boundaries seeded; the copy, then from the same state ours, both
// under the game's x87 control word 0x027F; the regions, our packet pool and
// strings, the answer (at the width the original defines) and the stand-ins'
// log compared.
//
// The stand-ins do what their callers read back: Window_Alloc claims a free
// record as the real one does, the commit moves the packet cursor, the
// primitive setters write their code bytes and z floats, the draw mode its
// words, sprintf the print buffer (a small %Nd formatter of its own: nothing
// here may reach the CRT, the self-test runs before it is initialised); and
// most disturb a cell some caller reads again after the call (the party size,
// the window colour, the print buffer, the working and character records' AP
// and flags, the ability table's bytes, the four-byte flags, 0x904AAA).
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/battle_window_draw_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"
#include <windows.h>

namespace battle_window_draw {
namespace {

using move_script::At;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

constexpr unsigned short kGameControlWord = 0x027F;   // measured: psx_gte_float.cpp
unsigned short GetControlWord() {
    unsigned short cw;
    __asm__("fnstcw %0" : "=m"(cw));
    return cw;
}
void SetControlWord(unsigned short cw) { __asm__("fldcw %0" : : "m"(cw)); }

std::uint32_t Address(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }

// --- the random source, our buffers, the stand-ins' log ---------------------

std::uint32_t g_rng = 0xBB67AE85u;
std::uint32_t Next() { g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5; return g_rng; }
bool Half() { return (Next() & 1) != 0; }
bool Often() { return Next() % 3 != 0; }
std::uint32_t Garbage(std::uint32_t low_mask, std::uint32_t value) { return (Next() & ~low_mask) | (value & low_mask); }
template <unsigned N> std::uint32_t Pick(const std::uint32_t (&v)[N]) { return v[Next() % N]; }

constexpr unsigned kPool = 0x800;
constexpr unsigned kNames = 4, kNameBytes = 0x20;

struct Buffers {
    unsigned char pool[kPool];
    unsigned char names[kNames][kNameBytes];
};
Buffers g_buf;

constexpr unsigned kLog = 96;
struct Entry { std::uint32_t what, a, b, c, d, e, f; };
Entry g_log[kLog];
unsigned g_log_n;
std::uint32_t g_seed;   // the stand-ins' own stream: the same on both passes

std::uint32_t Hash() {
    std::uint32_t h = (g_seed + g_log_n * 0x2545F491u) * 0x9E3779B1u;
    h ^= h >> 15;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    return h;
}
void Record(std::uint32_t what, std::uint32_t a = 0, std::uint32_t b = 0, std::uint32_t c = 0, std::uint32_t d = 0,
            std::uint32_t e = 0, std::uint32_t f = 0) {
    if (g_log_n < kLog) g_log[g_log_n] = {what, a, b, c, d, e, f};
    ++g_log_n;
}

// A pointer as both passes can compare it: an offset inside our buffers,
// else the address (the game's own memory is at the same place on both).
std::uint32_t Id(const void* p) {
    const std::uint32_t at = Address(p), base = Address(&g_buf);
    if (at >= base && at < base + sizeof g_buf) return 0x1000000u + (at - base);
    return at;
}
std::uint32_t TextHash(const unsigned char* s, unsigned n) {
    std::uint32_t h = 0x811C9DC5u;
    for (unsigned i = 0; i < n && i < 40 && s[i]; ++i) h = (h ^ s[i]) * 0x01000193u;
    return h;
}

// --- the cells the functions read again after a call ------------------------

void Disturb() {
    const std::uint32_t h = Hash();
    if (h % 4 == 0) return;
    const unsigned v = (h >> 12) & 0xFF;
    switch ((h >> 4) % 12) {
    case 0: At(at::kPartyCount)[0] = static_cast<unsigned char>(v); break;
    case 1: At(at::kColour)[0] = static_cast<unsigned char>(static_cast<signed char>(v % 17) - 8); break;
    case 2: {
        // a character of the print buffer, never its tail (Fix keeps 0x30.. 0)
        unsigned char* const b = At(at::kPrintBuf);
        b[(h >> 20) % 8] = static_cast<unsigned char>(v % 3 == 0 ? 0x20 : 0x30 + v % 11);
        break;
    }
    case 3: {
        unsigned char* const w = At(at::kWorking + ((h >> 20) % 4) * at::kWorkingStride);
        static const unsigned kFields[] = {0x10, 0x1A, 0x1B, 0x1E, 0x16, 0x17};
        w[kFields[(h >> 24) % 6]] = static_cast<unsigned char>(v % 3 == 0 ? v : v % 8);
        break;
    }
    case 4: {
        unsigned char* const r = At(at::kCharRecords + ((h >> 20) % 10) * at::kCharStride);
        static const unsigned kFields[] = {0x1A, 0x1B, 0x16, 0x17};
        r[kFields[(h >> 24) % 4]] = static_cast<unsigned char>(v % 3 == 0 ? v : v % 8);
        break;
    }
    case 5: {
        unsigned char* const a = At(at::kAbilities + ((h >> 20) % 0x100) * at::kAbilityStride);
        static const unsigned kFields[] = {0x10, 0x12, 0x14, 0x15};
        a[kFields[(h >> 28) % 4]] = static_cast<unsigned char>(v);
        break;
    }
    case 6: At(at::kFlags15 + (v & 3))[0] = static_cast<unsigned char>(v & 4 ? h >> 24 : 0); break;
    case 7: At(at::kFlags3E + (v & 3))[0] = static_cast<unsigned char>(v & 4 ? h >> 24 : 0); break;
    case 8: At(at::kBattle97)[0] = static_cast<unsigned char>(0x24 + v % 3); break;
    default: break;
    }
}

// --- the stand-ins ----------------------------------------------------------

constexpr std::uint32_t kPointZeroOne = 0x3C23D70Au;
void PutDword(unsigned char* at, std::uint32_t v) { std::memcpy(at, &v, sizeof v); }
std::uint32_t Lo16(int v) { return static_cast<std::uint32_t>(v) & 0xFFFF; }
std::uint32_t Lo8(unsigned v) { return v & 0xFF; }

// Window_Alloc as msgbox.cpp has it: claims the record when its first byte is 0.
unsigned __cdecl StubAlloc(unsigned slot, unsigned kind) {
    Record(1, slot, kind);
    unsigned char* const rec = At(at::kWindows + (slot & 0xFF) * at::kWindowStride);
    unsigned answer = slot | 0xFFu;
    if (rec[0] == 0) {
        rec[0] = 1;
        rec[1] = static_cast<unsigned char>(kind);
        rec[2] = 0;
        rec[3] = 0;
        answer = slot;
    }
    Disturb();
    return answer;
}
// sprintf: %[width]d written in decimal, right-aligned in spaces; every other
// byte copied; no more than 0x20 characters.
int __cdecl StubSprintf(char* dst, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    std::uint32_t args[2] = {};
    unsigned n = 0, out = 0;
    for (const char* p = fmt; *p && out < 0x20; ++p) {
        if (p[0] != '%') { dst[out++] = *p; continue; }
        unsigned width = 0;
        const char* q = p + 1;
        while (*q >= '0' && *q <= '9') width = width * 10 + static_cast<unsigned>(*q++ - '0');
        if (*q != 'd') { dst[out++] = *p; continue; }
        const std::int32_t v = va_arg(ap, std::int32_t);
        if (n < 2) args[n] = static_cast<std::uint32_t>(v);
        ++n;
        char digits[12];
        unsigned k = 0;
        std::uint32_t mag = v < 0 ? 0u - static_cast<std::uint32_t>(v) : static_cast<std::uint32_t>(v);
        do { digits[k++] = static_cast<char>('0' + mag % 10); mag /= 10; } while (mag && k < 10);
        if (v < 0) digits[k++] = '-';
        for (unsigned pad = k; pad < width && out < 0x20; ++pad) dst[out++] = ' ';
        while (k && out < 0x20) dst[out++] = digits[--k];
        p = q;
    }
    va_end(ap);
    dst[out] = 0;
    Record(2, Id(dst), Id(fmt), n, args[0], args[1]);
    Disturb();
    return static_cast<int>(out);
}
unsigned __cdecl StubGetTPage(unsigned tp, unsigned abr, int x, int y) {
    Record(3, tp, abr, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y));
    Disturb();
    return Hash();
}
void __cdecl StubDrawMode(unsigned char* prim, int dfe, int dtd, unsigned tpage, unsigned long tw) {
    Record(4, Id(prim), static_cast<std::uint32_t>(dfe), static_cast<std::uint32_t>(dtd), tpage, static_cast<std::uint32_t>(tw));
    PutDword(prim + 4, 0xE8000000u | (tpage & 0xFFFFu));
    if (dfe) prim[6] = static_cast<unsigned char>(prim[6] | 1);
    if (dtd) prim[6] = static_cast<unsigned char>(prim[6] | 2);
    PutDword(prim + 8, static_cast<std::uint32_t>(tw));
    Disturb();
}
// The commit moves the packet cursor, as the real one does, so that a cursor
// read too early shows; it wraps in the first 0x200 bytes of our pool, and
// stamps the committed primitive's tag.
void __cdecl StubCommit(unsigned slot, unsigned size) {
    unsigned char* const at = Gfx_PacketNext;
    Record(5, slot, size, Id(at));
    PutDword(at, 0xC0000000u | (Hash() & 0xFFFFFF));
    const unsigned offset = static_cast<unsigned>(at - g_buf.pool);
    Gfx_PacketNext = g_buf.pool + (offset + (size & 0xFF) + (Hash() % 3 == 0 ? 4 : 0)) % 0x200u;
    Disturb();
}
unsigned __cdecl StubGetClut(int x, int y) {
    Record(6, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y));
    Disturb();
    return Hash() >> 3;
}
void __cdecl StubSetSprt(unsigned char* prim) { Record(7, Id(prim)); prim[7] = 0x64; PutDword(prim + 0x10, kPointZeroOne); Disturb(); }
unsigned char* __cdecl StubPolyF4(unsigned char* prim) {
    Record(8, Id(prim));
    prim[7] = 0x28;
    for (unsigned z = 0x10; z <= 0x34; z += 0xC) PutDword(prim + z, kPointZeroOne);
    Disturb();
    return prim;
}
void __cdecl StubSetSemi(unsigned char* prim, unsigned abe) {
    Record(9, Id(prim), abe & 0xFF);
    prim[7] = static_cast<unsigned char>((abe & 1) ? prim[7] | 2 : prim[7] & 0xFD);
    Disturb();
}
void __cdecl StubTile(unsigned char* prim) { Record(10, Id(prim)); prim[7] = 0x60; PutDword(prim + 0x10, kPointZeroOne); Disturb(); }
void __cdecl StubPolyFT4(unsigned char* prim) {
    Record(11, Id(prim));
    prim[7] = 0x2C;
    for (unsigned z = 0x10; z <= 0x40; z += 0x10) PutDword(prim + z, kPointZeroOne);
    Disturb();
}
void __cdecl StubLineF2(unsigned char* prim) {
    Record(12, Id(prim));
    prim[7] = 0x40;
    PutDword(prim + 0x10, kPointZeroOne);
    PutDword(prim + 0x1C, kPointZeroOne);
    Disturb();
}
void __cdecl StubIcon8(int x, int y, int icon, int dim) {
    Record(13, Lo16(x), Lo16(y), Lo8(static_cast<unsigned>(icon)), Lo8(static_cast<unsigned>(dim)));
    Disturb();
}
unsigned char __cdecl StubCharCount(const unsigned char* text) {
    Record(14, Id(text), TextHash(text, 17));
    Disturb();
    return static_cast<unsigned char>(Hash() % 11);
}
const unsigned char* __cdecl StubDrawAt(int x, int y, int colour, int count, const unsigned char* text) {
    Record(15, Lo16(x), Lo16(y), Lo8(static_cast<unsigned>(colour)), Lo8(static_cast<unsigned>(count)), Id(text),
           TextHash(text, static_cast<unsigned>(count) & 0xFF));
    Disturb();
    return text + (Hash() & 7);
}
void __cdecl StubFont8(int x, int y, int colour, const unsigned char* text) {
    Record(16, Lo16(x), Lo16(y), Lo8(static_cast<unsigned>(colour)), Id(text), TextHash(text, 40));
    Disturb();
}
// The cost: around the AP the seeds put in the records (0..20), sometimes far.
unsigned char __cdecl StubApCost(unsigned member, unsigned id, unsigned battle) {
    Record(17, member & 0xFF, id & 0xFF, battle & 0xFF);
    Disturb();
    const std::uint32_t h = Hash();
    return static_cast<unsigned char>(h % 5 == 0 ? h >> 8 : h % 5 == 1 ? 0 : (h >> 8) % 21);
}

const Callees kStubs = {
    StubAlloc,   StubSprintf, StubGetTPage, StubDrawMode,  StubCommit, StubGetClut,  StubSetSprt, StubPolyF4, StubSetSemi,
    StubTile,    StubPolyFT4, StubLineF2,   StubIcon8,     StubCharCount, StubDrawAt, StubFont8,  StubApCost,
};

const void* StubFor(std::uint32_t target) {
    const auto f = [](auto p) { return reinterpret_cast<const void*>(p); };
    switch (target) {
    case 0x59E2D0: return f(&StubAlloc);
    case 0x5B9380: return f(&StubSprintf);
    case 0x5A79A0: return f(&StubGetTPage);
    case 0x5A77C0: return f(&StubDrawMode);
    case 0x461E50: return f(&StubCommit);
    case 0x5A79E0: return f(&StubGetClut);
    case 0x5A7710: return f(&StubSetSprt);
    case 0x5A75B0: return f(&StubPolyF4);
    case 0x5A7780: return f(&StubSetSemi);
    case 0x5A7740: return f(&StubTile);
    case 0x5A75D0: return f(&StubPolyFT4);
    case 0x5A7650: return f(&StubLineF2);
    case 0x57D360: return f(&StubIcon8);
    case 0x57D800: return f(&StubCharCount);
    case 0x516B30: return f(&StubDrawAt);
    case 0x517090: return f(&StubFont8);
    case 0x591DB0: return f(&StubApCost);
    default: bof3::Fatal("battle_window_draw: no stand-in for a call to 0x%X", static_cast<unsigned>(target)); return nullptr;
    }
}

// --- the nineteen copies (capstone, 2026-09-23: every jump internal, no jump
// table; the calls below are every call that leaves) ------------------------

struct Call { std::uint32_t offset, target; };
// ret: the answer's width the original defines - 0 none, 1 al, 4 eax.
struct Clone {
    const char* name;
    std::uint32_t base, size;
    const Call* calls;
    int n_calls;
    unsigned ret;
    const void* ours;
};

constexpr Call k444230[] = {{0x4, 0x59E2D0}};
constexpr Call k444290[] = {{0x4, 0x59E2D0}};
constexpr Call k4442C0[] = {{0x4, 0x59E2D0}};
constexpr Call k4442E0[] = {{0x4, 0x59E2D0}};
constexpr Call k444340[] = {{0x1A, 0x5B9380}, {0x34, 0x5B9380}, {0x4D, 0x5A79A0}, {0x65, 0x5A77C0}, {0x6E, 0x461E50}, {0xA4, 0x5A79E0}, {0xF0, 0x5A7710}, {0xF9, 0x461E50}};
constexpr Call k4447B0[] = {{0x9, 0x5A75B0}, {0x13B, 0x5A7780}, {0x144, 0x461E50}};
constexpr Call k444900[] = {{0x8, 0x5A7740}, {0xC8, 0x5A7780}, {0xD1, 0x461E50}};
constexpr Call k4449E0[] = {{0x8, 0x5A7740}, {0x95, 0x5A7780}, {0x9E, 0x461E50}};
constexpr Call k444A90[] = {{0xA, 0x5A75D0}, {0x1A, 0x5A79A0}, {0x3F, 0x5A79E0}, {0xF9, 0x461E50}, {0x105, 0x5A75D0}, {0x115, 0x5A79A0}, {0x128, 0x5A79E0}, {0x19D, 0x461E50}};
constexpr Call k444C40[] = {{0xD, 0x5A79A0}, {0x25, 0x5A77C0}, {0x2E, 0x461E50}, {0x8E, 0x5A7710}, {0x97, 0x461E50}};
constexpr Call k444CE0[] = {{0x8, 0x5A7650}, {0x66, 0x461E50}};
constexpr Call k444D50[] = {{0xE, 0x5A79A0}, {0x26, 0x5A77C0}, {0x2F, 0x461E50}, {0x3B, 0x5A7650}, {0x98, 0x5A7780}, {0xA1, 0x461E50}};
constexpr Call k444E00[] = {{0xE, 0x5A79A0}, {0x26, 0x5A77C0}, {0x2F, 0x461E50}, {0x3B, 0x5A7650}, {0x98, 0x5A7780}, {0xA1, 0x461E50}};
constexpr Call k57DA70[] = {{0x5A, 0x591DB0}, {0x13E, 0x591DB0}};
constexpr Call k57DC90[] = {{0x27, 0x57D360}, {0x35, 0x57D800}, {0x48, 0x516B30}, {0x62, 0x5B9380}, {0x72, 0x517090}};

#define BD_C(name, base, size, calls, ret) {#name, base, size, calls, static_cast<int>(sizeof calls / sizeof calls[0]), ret, reinterpret_cast<const void*>(&::name)}
#define BD_P(name, base, size, ret) {#name, base, size, nullptr, 0, ret, reinterpret_cast<const void*>(&::name)}
const Clone kClones[] = {
    BD_C(BattleWin_OpenStatus, 0x444230, 0x51, k444230, 0),
    BD_C(BattleWin_OpenSub1, 0x444290, 0x2F, k444290, 0),
    BD_C(BattleWin_OpenSub2, 0x4442C0, 0x1D, k4442C0, 0),
    BD_C(BattleWin_OpenSub3, 0x4442E0, 0x2F, k4442E0, 0),
    BD_C(BattleWin_DrawNumber, 0x444340, 0x132, k444340, 0),
    BD_C(BattleWin_DrawQuadF4, 0x4447B0, 0x14F, k4447B0, 0),
    BD_C(BattleWin_DrawTile, 0x444900, 0xDB, k444900, 0),
    BD_C(BattleWin_DrawTileRgb, 0x4449E0, 0xA8, k4449E0, 0),
    BD_C(BattleWin_DrawBar, 0x444A90, 0x1A9, k444A90, 0),
    BD_C(BattleWin_DrawCell16, 0x444C40, 0xA0, k444C40, 0),
    BD_C(BattleWin_DrawLine, 0x444CE0, 0x70, k444CE0, 0),
    BD_C(BattleWin_DrawLineHalf, 0x444D50, 0xAB, k444D50, 0),
    BD_C(BattleWin_DrawLineAdd, 0x444E00, 0xAB, k444E00, 0),
    BD_P(BattleWin_FirstOfKind, 0x444EB0, 0x84, 1),
    BD_C(Skill_CanUse, 0x57DA70, 0x176, k57DA70, 1),
    BD_C(Menu_DrawSkillRow, 0x57DC90, 0x7F, k57DC90, 0),
    BD_P(Skill_FlagIndex, 0x5918A0, 0x3A, 1),
    BD_P(Skill_ApCost, 0x591DB0, 0x9D, 1),
    BD_P(Char_AbilityList, 0x591E50, 0x67, 4),
};
#undef BD_C
#undef BD_P
constexpr unsigned kCount = sizeof kClones / sizeof kClones[0];

enum : unsigned {
    kOpenStatus, kOpenSub1, kOpenSub2, kOpenSub3, kNumber, kQuad, kTile, kTileRgb, kBar, kCell, kLine, kLineHalf,
    kLineAdd, kFirstOfKind, kCanUse, kSkillRow, kFlagIndex, kApCost, kAbilityList,
};
static_assert(kAbilityList + 1 == kCount, "the index list and the clone list disagree");

// --- the state both passes start from ---------------------------------------

struct Region { std::uint32_t at, size; };
const Region kRegions[] = {
    {at::kWindows, 22 * at::kWindowStride},                 // WindowRecords
    {0x7E0670, 4},                                          // Gfx_PacketNext (into our pool)
    {at::kWorking, 4 * at::kWorkingStride},                 // the party's working records
    {at::kCharRecords, 10 * at::kCharStride},               // CharacterRecords
    {0x903A50, 0x20},                                       // the window colour 0x903A5A
    {0x904040, 0x40},                                       // the party lists 0x904062
    {0x904650, 0x20},                                       // the flag words 0x90465C, 0x904660
    {0x904AA0, 0x20},                                       // 0x904AAA, the party size 0x904AB0
    {at::kPrintBuf, 0x40},                                  // the sprintf buffer
    {at::kMemberRecord, 0x18},                              // character -> record
    {at::kAbilities, 0x100 * at::kAbilityStride},           // the ability table
    {at::kClutShadow - 0x200, 0x500},                       // the CLUT shadow around the rows seeded
    {at::kEnemies - 3 * at::kEnemyStride, 11 * at::kEnemyStride},  // enemies -3 .. 7
    // the shapes, the tile sizes, window 1's x table - around the ":" format
    // at 0x64E320, which a random byte could turn into a conversion that
    // reads an argument never passed
    {at::kQuadShapes, 0x1D8},
    {at::kFmtNone + 4, 0xE4},
    {at::kSkillIcons, 0x100},                               // the skill row's icons
};
constexpr unsigned kRegionBytes = 22 * at::kWindowStride + 4 + 4 * at::kWorkingStride + 10 * at::kCharStride + 0x20 +
                                  0x40 + 0x20 + 0x20 + 0x40 + 0x18 + 0x100 * at::kAbilityStride + 0x500 +
                                  11 * at::kEnemyStride + 0x1D8 + 0xE4 + 0x100;

struct State {
    unsigned char memory[kRegionBytes];
    Buffers buf;
    std::uint32_t result;
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

unsigned char* Window(unsigned slot) { return At(at::kWindows + slot * at::kWindowStride); }
unsigned char* Ability(unsigned id) { return At(at::kAbilities + (id & 0xFF) * at::kAbilityStride); }
unsigned char* Enemy(std::int32_t index) {
    return At(at::kEnemies + static_cast<std::uint32_t>(index * static_cast<std::int32_t>(at::kEnemyStride)));
}

// Random bytes put back inside what the functions can safely be handed: the
// packet cursor in our pool, the print buffer ended, the window colour and
// the party's indexes inside the regions (mostly), the strings ended.
void Fix() {
    Gfx_PacketNext = g_buf.pool + Next() % 0x40;
    unsigned char* const buf = At(at::kPrintBuf);
    std::memset(buf + 0x20, 0, 0x20);
    if (Often()) At(at::kColour)[0] = static_cast<unsigned char>(static_cast<signed char>(Next() % 17) - 8);
    for (unsigned k = 0; k < 8; ++k)
        if (Often()) At(at::kPartyLists + k)[0] = static_cast<unsigned char>(Next() % 0x18);
    for (unsigned k = 0; k < 0x18; ++k)
        if (Often()) At(at::kMemberRecord + k)[0] = static_cast<unsigned char>(Next() % 10);
    for (unsigned k = 0; k < kNames; ++k) {
        const unsigned len = Next() % (kNameBytes - 1);
        for (unsigned i = 0; i < len; ++i) g_buf.names[k][i] = static_cast<unsigned char>(0x21 + Next() % 0xDE);
        g_buf.names[k][len] = 0;
    }
    // AP and cost bytes near each other, the accessory bytes near 0x19 / 0x1A
    for (unsigned m = 0; m < 10; ++m) {
        unsigned char* const r = At(at::kCharRecords + m * at::kCharStride);
        if (Often()) SetWord(r + 0x1A, Next() % 21);
        for (unsigned s = 0x16; s <= 0x17; ++s)
            if (Often()) r[s] = static_cast<unsigned char>(0x18 + Next() % 4);
    }
    for (unsigned m = 0; m < 4; ++m) {
        unsigned char* const w = At(at::kWorking + m * at::kWorkingStride);
        if (Often()) SetWord(w + 0x1A, Half() ? 0 : Next() % 21);
        if (Often()) w[0x1E] = static_cast<unsigned char>(3 + Next() % 4);
        for (unsigned s = 0x16; s <= 0x17; ++s)
            if (Often()) w[s] = static_cast<unsigned char>(0x18 + Next() % 4);
    }
    for (unsigned k = 0; k < 4; ++k) {
        if (Often()) At(at::kFlags15 + k)[0] = 0;
        if (Often()) At(at::kFlags3E + k)[0] = 0;
    }
    if (Half()) At(at::kBattle97)[0] = static_cast<unsigned char>(0x24 + Next() % 3);
    // windows 5..12: actors 0..10, a state of 0..4, kinds of 0..2
    for (unsigned k = 5; k <= 12; ++k) {
        Window(k)[0xA] = static_cast<unsigned char>(Next() % 11);
        Window(k)[3] = static_cast<unsigned char>(Next() % 5);
    }
    for (std::int32_t e = -3; e < 8; ++e) Enemy(e)[at::kEnemyKind] = static_cast<unsigned char>(Next() % 3);
}

struct Args { std::uint32_t a[8]; };

std::uint32_t Coord() {
    static const std::uint32_t kEdges[] = {0, 1, 0x7FFF, 0x8000, 0xFFFF, 0x10000, 0xFFFFFFF0u, 0x140, 0xF0, 0xFFFFFFFFu};
    return Often() ? Garbage(0xFFFF, Next() % 0x180) : Half() ? Pick(kEdges) : Next();
}

Args Seed(unsigned k) {
    Args args;
    for (auto& a : args.a) a = Next();
    switch (k) {
    case kOpenStatus:
    case kOpenSub1:
    case kOpenSub2:
    case kOpenSub3:
        for (unsigned s = 1; s <= 4; ++s)
            if (Half()) Window(s)[0] = 0;
        if (Half()) At(at::kPartyCount)[0] = static_cast<unsigned char>(Next() % 4);
        break;
    case kNumber: {
        static const std::uint32_t kValues[] = {0xFFFF, 0x1FFFF, 0xFFFFFFFFu, 0xFFFE, 0, 9, 10, 99, 100, 999, 1000, 9999, 10000, 0x8000, 0x7FFF};
        args.a[0] = Coord();
        args.a[1] = Coord();
        args.a[3] = Often() ? Garbage(0xFFFF, Pick(kValues)) : Next();
        break;
    }
    case kQuad:
    case kTile:
        args.a[0] = Coord();
        args.a[1] = Coord();
        args.a[2] = Often() ? Garbage(0xFF, Next() % 0x20) : Next();
        args.a[3] = Often() ? Garbage(0xFF, Next() % 4) : Next();
        break;
    case kTileRgb:
        args.a[0] = Coord();
        args.a[1] = Coord();
        args.a[2] = Often() ? Garbage(0xFF, Next() % 0x20) : Next();
        break;
    case kBar: {
        static const std::uint32_t kLit[] = {0, 1, 0x100, 0xFF, 0x80, 0xFFFFFF00u};
        static const std::uint32_t kBytes[] = {0, 1, 0x7F, 0x80, 0xFF, 0x100, 0x1FF};
        args.a[0] = Coord();
        args.a[1] = Coord();
        args.a[2] = Often() ? Pick(kBytes) : Next();
        args.a[3] = Often() ? Pick(kBytes) : Next();
        args.a[4] = Often() ? Pick(kLit) : Next();
        break;
    }
    case kCell:
        args.a[0] = Coord();
        args.a[1] = Coord();
        break;
    case kLine:
    case kLineHalf:
    case kLineAdd:
        for (unsigned i = 0; i < 4; ++i) args.a[i] = Coord();
        break;
    case kFirstOfKind: {
        const unsigned e = Next() % 8;
        args.a[0] = Half() ? e : Garbage(0xFF, e);
        // the enemy in one of the windows 5..12 most of the time, and more of
        // its kind above it
        if (Often()) Window(5 + Next() % 8)[0xA] = static_cast<unsigned char>(e + 3);
        if (Often()) Window(5 + Next() % 8)[0xA] = static_cast<unsigned char>(e + 3);
        if (Half()) Enemy(static_cast<std::int32_t>(Next() % 8))[at::kEnemyKind] = Enemy(static_cast<std::int32_t>(e))[at::kEnemyKind];
        break;
    }
    case kCanUse: {
        static const std::uint32_t kModes[] = {0, 1, 2, 3, 0x101, 0x102, 0xFF, 0x201};
        static const std::uint32_t kIds[] = {0, 0x14, 0x15, 0x3E, 0x8C, 0x97, 0x100, 0x114, 0x13, 0x16, 0x3D, 0x3F, 0x8B, 0x8D, 0x96, 0x98, 1};
        args.a[0] = Often() ? Pick(kModes) : Next();
        args.a[1] = Often() ? Garbage(0xFF, Next() % 4) : Garbage(0xFF, Next() % 8);
        args.a[2] = Often() ? Garbage(Half() ? 0xFFFFFFFFu : 0xFF, Pick(kIds)) : Next();
        unsigned char* const a = Ability(args.a[2]);
        a[0x10] = static_cast<unsigned char>(Next() % 4 | (Next() & 0xFC));
        if (Half()) a[0x15] = static_cast<unsigned char>(Half() ? 4 : Next() & ~4u);
        unsigned char* const w = At(at::kWorking + (args.a[1] & 3) * at::kWorkingStride);
        if (Half()) w[0x10] = static_cast<unsigned char>(Half() ? 0x10 : Next() & ~0x10u);
        break;
    }
    case kSkillRow:
        args.a[0] = Coord();
        args.a[1] = Coord();
        args.a[4] = Address(g_buf.names[Next() % kNames]);
        break;
    case kFlagIndex: {
        static const std::uint32_t kFlags[] = {0, 0x200, 0x100, 0x1, 0x2, 0x80, 0x180, 0xFE00, 0xFFFF, 0x1FF, 0x400};
        args.a[0] = Half() ? Next() % 0x100 : Next();
        if (Often()) SetWord(Ability(args.a[0]) + 0x14, Half() ? Pick(kFlags) : Next());
        break;
    }
    case kApCost: {
        static const std::uint32_t kBattle[] = {0, 1, 0x100, 0xFF, 0x80};
        static const std::uint32_t kCosts[] = {0, 1, 2, 3, 4, 5, 7, 8, 0xFE, 0xFF, 0x55, 0x56};
        args.a[2] = Often() ? Pick(kBattle) : Next();
        args.a[0] = (args.a[2] & 0xFF) == 0 ? Garbage(0xFF, Next() % 10) : Garbage(0xFF, Next() % 4);
        args.a[1] = Next();
        if (Often()) Ability(args.a[1])[0x12] = static_cast<unsigned char>(Pick(kCosts));
        break;
    }
    case kAbilityList: {
        static const std::uint32_t kTypes[] = {0, 1, 2, 3, 4, 0x101, 0xFF, 0x100};
        static const std::uint32_t kBattle[] = {0, 1, 0x100, 0xFF};
        args.a[0] = Garbage(0xFF, Next() % 8);
        args.a[1] = Often() ? Pick(kTypes) : Next();
        args.a[2] = Often() ? Pick(kBattle) : Next();
        break;
    }
    default: break;
    }
    return args;
}

// What the rounds reached, from the original's side, for the log line.
struct Coverage {
    unsigned logged[20];
    unsigned first_of_kind[2], can_use[2], flag_index[11];
    unsigned cost_half, cost_quarter, cost_plain, allocs_taken, colon;
} g_cover;

void Cover(unsigned k, const State& in, const State& out, std::uint32_t result, const Args& args) {
    for (unsigned i = 0; i < out.log_n && i < kLog; ++i)
        if (out.log[i].what < 20) ++g_cover.logged[out.log[i].what];
    if (k == kFirstOfKind) ++g_cover.first_of_kind[result ? 1 : 0];
    if (k == kCanUse) ++g_cover.can_use[result ? 1 : 0];
    if (k == kFlagIndex && result <= 10) ++g_cover.flag_index[result];
    if (k == kNumber && (args.a[3] & 0xFFFF) == 0xFFFF) ++g_cover.colon;
    (void)in;
}

// Which of Skill_ApCost's three answers the input asks for, read from the
// live input before the passes run.
void CoverApCost(const Args& args) {
    const unsigned m = args.a[0] & 0xFF;
    const unsigned char* const r = (args.a[2] & 0xFF) == 0 ? At(at::kCharRecords + m * at::kCharStride)
                                                           : At(at::kWorking + m * at::kWorkingStride);
    if (r[0x16] == 0x1A || r[0x17] == 0x1A) ++g_cover.cost_half;
    else if (r[0x16] == 0x19 || r[0x17] == 0x19) ++g_cover.cost_quarter;
    else ++g_cover.cost_plain;
}

using Fn8 = std::uint32_t (__cdecl*)(std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t,
                                      std::uint32_t, std::uint32_t, std::uint32_t);

}  // namespace

void SelfTest() {
    constexpr unsigned kPerFunction = 2000;
    unsigned region_bytes = 0;
    for (const Region& r : kRegions) region_bytes += r.size;
    if (region_bytes != kRegionBytes)
        bof3::Fatal("battle_window_draw: the regions are %u bytes, the state holds %u", region_bytes, kRegionBytes);

    void* clones[kCount];
    for (unsigned k = 0; k < kCount; ++k) {
        const Clone& c = kClones[k];
        bof3::CloneCall calls[8];
        if (c.n_calls > 8) bof3::Fatal("battle_window_draw: %s has %d calls", c.name, c.n_calls);
        for (int i = 0; i < c.n_calls; ++i) calls[i] = {c.calls[i].offset, StubFor(c.calls[i].target), c.calls[i].target};
        clones[k] = bof3::CloneOriginal(c.name, c.base, c.size, calls, c.n_calls);
    }

    static State saved, input, their_out, our_out;
    Capture(saved);
    const Callees kept = g;
    g = kStubs;
    const unsigned short saved_cw = GetControlWord();

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
        Fix();
        g_seed = Next();
        const Args args = Seed(k);
        Capture(input);
        if (k == kApCost) CoverApCost(args);
        if (k >= kOpenStatus && k <= kOpenSub3 && Window(k + 1)[0] != 0) ++g_cover.allocs_taken;

        std::uint32_t result[2];
        for (int pass = 0; pass < 2; ++pass) {
            Apply(input);
            State& out = pass ? our_out : their_out;
            const void* const fn = pass ? kClones[k].ours : clones[k];
            SetControlWord(kGameControlWord);
            const std::uint32_t r = reinterpret_cast<Fn8>(const_cast<void*>(fn))(
                args.a[0], args.a[1], args.a[2], args.a[3], args.a[4], args.a[5], args.a[6], args.a[7]);
            SetControlWord(saved_cw);
            Capture(out);
            const unsigned w = kClones[k].ret;
            result[pass] = w == 0 ? 0u : w == 1 ? (r & 0xFFu) : r;
            out.result = result[pass];
        }
        calls += their_out.log_n;
        Cover(k, input, their_out, result[0], args);
        if (std::memcmp(&their_out, &our_out, sizeof their_out) != 0) {
            ++bad_per[k];
            if (++bad <= 12) {
                unsigned first = 0;
                while (first < sizeof their_out &&
                       reinterpret_cast<const unsigned char*>(&their_out)[first] ==
                           reinterpret_cast<const unsigned char*>(&our_out)[first])
                    ++first;
                bof3::Log("shadow      battle_window_draw self-test MISMATCH: round %u, %s, log %u / %u, result 0x%X / "
                          "0x%X, first differing state byte %u",
                          round, kClones[k].name, their_out.log_n, our_out.log_n, their_out.result, our_out.result, first);
                if (first < kRegionBytes) {
                    unsigned at = 0;
                    for (const Region& r : kRegions) {
                        if (first < at + r.size) {
                            bof3::Log("shadow      battle_window_draw:   memory 0x%X: 0x%02X / 0x%02X", r.at + (first - at),
                                      their_out.memory[first], our_out.memory[first]);
                            break;
                        }
                        at += r.size;
                    }
                } else if (first < offsetof(State, result)) {
                    const unsigned o = first - static_cast<unsigned>(offsetof(State, buf));
                    bof3::Log("shadow      battle_window_draw:   buffer +0x%X (pool %u): 0x%02X / 0x%02X", o, kPool,
                              reinterpret_cast<const unsigned char*>(&their_out.buf)[o],
                              reinterpret_cast<const unsigned char*>(&our_out.buf)[o]);
                } else if (first >= offsetof(State, log) && first < offsetof(State, log_n)) {
                    const unsigned e = (first - static_cast<unsigned>(offsetof(State, log))) / sizeof(Entry);
                    const Entry& a = their_out.log[e];
                    const Entry& b = our_out.log[e];
                    bof3::Log("shadow      battle_window_draw:   log %u: %u (%X %X %X %X %X %X) / %u (%X %X %X %X %X %X)", e,
                              a.what, a.a, a.b, a.c, a.d, a.e, a.f, b.what, b.a, b.b, b.c, b.d, b.e, b.f);
                }
            }
        }
    }
    g = kept;
    Apply(saved);

    bof3::Log("shadow      battle_window_draw self-test: %u rounds over %u functions (%u each), %u calls to the "
              "stand-ins, %u MISMATCHES; the window records, the packet cursor and our pool, the working and character "
              "records, the colour, the party lists, the flag words, the party size, the print buffer, the record map, "
              "the ability table, the CLUT shadow, the enemies, the shape tables, the skill icons, the answer and the "
              "stand-ins' log compared",
              kPerFunction * kCount, kCount, kPerFunction, calls, bad);
    if (bad)
        for (unsigned k = 0; k < kCount; ++k)
            if (bad_per[k]) bof3::Log("shadow      battle_window_draw: %s mismatched in %u rounds", kClones[k].name, bad_per[k]);
    const Coverage& c = g_cover;
    bof3::Log("shadow      battle_window_draw coverage: first of kind no %u, yes %u; can use no %u, yes %u; flag index "
              "0 %u, 1 %u, 9 %u; AP cost halved %u, three quarters %u, whole %u; openers on a taken record %u; the ':' "
              "number %u; calls: window allocs %u, sprintf %u, commits %u, cluts %u, semi %u, AP costs %u, text %u",
              c.first_of_kind[0], c.first_of_kind[1], c.can_use[0], c.can_use[1], c.flag_index[0], c.flag_index[1],
              c.flag_index[9], c.cost_half, c.cost_quarter, c.cost_plain, c.allocs_taken, c.colon, c.logged[1],
              c.logged[2], c.logged[5], c.logged[6], c.logged[9], c.logged[17], c.logged[15]);
    if (bad) bof3::Fatal("the battle windows' draw helpers differ from the original in %u self-test rounds", bad);
}

}  // namespace battle_window_draw

// BOF3X_SHADOW=battle_draw: a differential fuzz of group BJ's six functions
// against byte-copies of Capcom's, once at start-up (docs/battle_draw.md
// section 4). Every call out of a copy is re-aimed at a recording stand-in -
// the tail jump of AreaMap_TintClut included - and ours is put on the same
// stand-ins through battle_draw::g. Per round: the state a function reads,
// random with its boundaries seeded; Capcom's copy, then ours from the same
// state; the calls out with their arguments and every region either could
// write compared.
//
// The three handlers use the vertex-block harness (d3d_fuzz.h: its log, its
// fake device and the DrawPrimitive snapshots), under the three x87 control
// words d3d_draw_fuzz.cpp uses. The two list draws make up to a hundred calls
// a round - more than that log holds - so they and the tint log into one of
// this file's own, 320 calls deep.
//
// The stand-ins record what the real callee reads of its arguments (a 16-bit
// coordinate, a byte colour, id or count - docs/battle_draw.md section 3), write
// what the real callee writes where the caller reads it again (the scroll
// step's offset and moving flag, sprintf's buffer, the colour pair), and now
// and then change something the caller reads after the call: a byte of the
// window record, the window colour, the menu member, a party-list byte, an
// inventory byte, a byte of the skill list; for the handlers a byte of the
// primitive, the vertex block, the scales or Gfx_DrawTpage.
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <utility>

#include "bof3/symbols.gen.h"
#include "game/battle_draw_callees.h"
#include "game/d3d_fuzz.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace battle_draw {
namespace {

using d3d_fuzz::Next;

unsigned char* At(U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }
U Addr(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
U GetLong(const unsigned char* p) {
    U v;
    std::memcpy(&v, p, sizeof v);
    return v;
}
void PutLong(unsigned char* p, U v) { std::memcpy(p, &v, sizeof v); }
void PutWord(unsigned char* p, U v) {
    const auto w = static_cast<std::uint16_t>(v);
    std::memcpy(p, &w, sizeof w);
}
template <typename T, typename F> T As(F f) { return reinterpret_cast<T>(reinterpret_cast<void*>(f)); }
U Pick(std::initializer_list<U> seeds) { return seeds.begin()[Next() % seeds.size()]; }
U Lo16(U v) { return v & 0xFFFF; }
U Lo8(U v) { return v & 0xFF; }

// --- our buffers -------------------------------------------------------------------
unsigned char g_prim[0x60];          // a handler's primitive
unsigned char g_window[0x40];        // a list's window record
unsigned char g_list[0x140];         // the skill list the stand-in hands out (+ top + rows past it)
constexpr U kInvFrom = 0x904154, kInvBytes = 0x5B0;   // the five categories and what top + rows reach
constexpr U kPartyFrom = 0x904062, kPartyBytes = 6;

// --- regions ---------------------------------------------------------------------
struct Region {
    U at, size;
};
constexpr U kMaxState = 0x1000;
struct State {
    unsigned char bytes[kMaxState];
};
U RegionBytes(const Region* r, int n) {
    U total = 0;
    for (int i = 0; i < n; ++i) total += r[i].size;
    return total;
}
void Capture(const Region* r, int n, State& s) {
    U at = 0;
    for (int i = 0; i < n; ++i) {
        std::memcpy(s.bytes + at, At(r[i].at), r[i].size);
        at += r[i].size;
    }
}
void Restore(const Region* r, int n, const State& s) {
    U at = 0;
    for (int i = 0; i < n; ++i) {
        std::memcpy(At(r[i].at), s.bytes + at, r[i].size);
        at += r[i].size;
    }
}
bool FirstDifference(const Region* r, int n, const State& a, const State& b, U* where) {
    U at = 0;
    for (int i = 0; i < n; ++i) {
        for (U k = 0; k < r[i].size; ++k)
            if (a.bytes[at + k] != b.bytes[at + k]) {
                *where = r[i].at + k;
                return true;
            }
        at += r[i].size;
    }
    return false;
}

// Every region any test touches, saved once and put back at the end.
const Region g_all[] = {
    {kVertices, 0x80}, {kScaleY, 8},          {kDrawTpage, 4},         {kColour, 1},
    {kMenuMember, 1},  {kPartyFrom, kPartyBytes}, {kInvFrom, kInvBytes}, {kPrintBuf, 0x60},
    {kStatusBits, 1},
};

// --- the logs --------------------------------------------------------------------
// The handlers' calls go to d3d_fuzz's log; the lists' and the tint's to this.
constexpr unsigned kCalls = 320;
struct Entry {
    U what, a[7];
};
struct Log {
    Entry e[kCalls];
    unsigned n;
};
Log g_theirs_log, g_ours_log;
Log* g_log;
bool g_lists;   // which log the stand-ins write
unsigned LogCount() { return g_lists ? g_log->n : d3d_fuzz::g_log->n; }
void Rec(U what, U a0 = 0, U a1 = 0, U a2 = 0, U a3 = 0, U a4 = 0, U a5 = 0, U a6 = 0) {
    if (g_log->n < kCalls) g_log->e[g_log->n] = {what, {a0, a1, a2, a3, a4, a5, a6}};
    ++g_log->n;
}
bool SameOwnLog(const Log& ours, const Log& theirs, char* why) {
    if (ours.n != theirs.n) {
        std::snprintf(why, 200, "%u calls, the original %u", ours.n, theirs.n);
        return false;
    }
    for (unsigned i = 0; i < ours.n && i < kCalls; ++i)
        if (std::memcmp(&ours.e[i], &theirs.e[i], sizeof(Entry)) != 0) {
            const Entry &o = ours.e[i], &t = theirs.e[i];
            std::snprintf(why, 200, "call %u: %u(%X %X %X %X %X %X %X), the original %u(%X %X %X %X %X %X %X)", i,
                          o.what, o.a[0], o.a[1], o.a[2], o.a[3], o.a[4], o.a[5], o.a[6], t.what, t.a[0], t.a[1],
                          t.a[2], t.a[3], t.a[4], t.a[5], t.a[6]);
            return false;
        }
    return true;
}

// A pointer as both passes can compare it: an offset inside our buffers, else
// the address (the game's tables are at the same place on both).
U Id(const void* p) {
    const U at = Addr(p);
    for (const auto& b : {std::pair<const unsigned char*, U>{g_window, sizeof g_window},
                          std::pair<const unsigned char*, U>{g_list, sizeof g_list}})
        if (at >= Addr(b.first) && at < Addr(b.first) + b.second)
            return (b.first == g_window ? 0x1000000u : 0x2000000u) + (at - Addr(b.first));
    return at;
}
U TextHash(const unsigned char* s, unsigned n) {
    U h = 0x811C9DC5u;
    for (unsigned i = 0; i < n && s[i]; ++i) h = (h ^ s[i]) * 0x01000193u;
    return h;
}

U g_round;
U Mix(U salt) {
    U h = (g_round * 0x9E3779B1u) ^ (salt * 0x85EBCA6Bu) ^ (LogCount() * 0xC2B2AE35u);
    h ^= h >> 16;
    h *= 0x7FEB352Du;
    h ^= h >> 15;
    h *= 0x846CA68Bu;
    h ^= h >> 16;
    return h;
}

// --- the lists' disturbances -----------------------------------------------------
bool g_skill;   // which list is under test: the fields that must stay in range differ
// The rows being drawn: the top the scroll step answered and the skill list
// handed out, so that a disturbance can land on the very id or count the list
// reads again after a call (a random byte of the whole inventory hardly ever
// did - control I23, docs/battle_draw.md section 4).
unsigned char g_last_top;
const unsigned char* g_last_list = g_list;

// A category the item list can index. A 4 stays a 4: key items have no counts
// array (0x656B24 is 0) and the list takes its pointers from the category once
// but tests it again per row, so a key-item list turned into another category
// under it reads through that null - in the original too (Menu_DrawItemList's
// fuzz keeps the same rule, docs/menu-windows.md section 4).
void SetCategory(unsigned char v) {
    if (g_window[0xA] != 4) g_window[0xA] = static_cast<unsigned char>(v % 4);
}
void Disturb(U salt) {
    const U h = Mix(salt ^ 0x3C6EF372u);
    if (h % 4 == 0) return;
    const auto v = static_cast<unsigned char>(h >> 24);
    switch ((h >> 4) % 12) {
    case 0: At(kColour)[0] = v; break;
    case 1: At(kMenuMember)[0] = static_cast<unsigned char>(v % 3 == 0 ? v : v % 5); break;
    case 2:
    case 3:
    case 4: {
        const unsigned field = 4 + (h >> 12) % 0x12;
        if (!g_skill && field == 0xA) SetCategory(v);
        else if (g_skill && field == 0xB) g_window[0xB] = static_cast<unsigned char>(v % 5);   // the title index
        else g_window[field] = v;
        break;
    }
    case 5:
        if ((h >> 8) % 2 && g_window[0xA] < 4) {
            // an id or a count of a row in view
            const U base = GetLong(At((((h >> 9) % 2) ? kCounts : kInventory) + g_window[0xA] * 4u));
            At(base + g_last_top + (h >> 10) % 10)[0] = static_cast<unsigned char>(v % 2 ? 0 : v);
        } else {
            At(kInvFrom + (h >> 8) % kInvBytes)[0] = static_cast<unsigned char>(v % 2 ? 0 : v);
        }
        break;
    case 6: At(kPartyFrom + (h >> 8) % kPartyBytes)[0] = static_cast<unsigned char>(v % 9); break;
    case 7:
        if ((h >> 8) % 2) const_cast<unsigned char*>(g_last_list)[g_last_top + (h >> 9) % 10] = static_cast<unsigned char>(v % 2 ? 0 : v);
        else g_list[(h >> 8) % sizeof g_list] = static_cast<unsigned char>(v % 2 ? 0 : v);
        break;
    case 8: g_window[0xC + (h >> 8) % 2] = static_cast<unsigned char>(v % 12); break;   // a mark or the cursor near the rows
    case 9: PutWord(g_window + ((h >> 8) % 2 ? 0x10 : 0x14), v % 2 ? 0 : v); break;
    default: break;
    }
}

// --- the stand-ins: the lists ----------------------------------------------------
void __cdecl StubBox(U x, U y, U w, U h, U flags, U colour) {
    Rec(1, Lo16(x), Lo16(y), Lo16(w), Lo16(h), Lo8(flags), Lo8(colour));
    Disturb(1);
}
// The scroll step writes its two outputs every time, and now and then the top
// and the state it was handed. Moving is 0 or 1 from the real step; the row
// counter is a byte compared with moving + 7, so 249 or more would never end
// (in the original too) - a 2 now and then is as far as the stand-in goes.
unsigned char __cdecl StubListScroll(unsigned char* top, unsigned char* offset, unsigned char* moving,
                                     unsigned char* state) {
    Rec(2, Id(top), Id(state), *top, *state);
    const U h = Mix(2);
    *offset = static_cast<unsigned char>(h % 3 == 0 ? 0 : (h >> 8) % 3 == 0 ? h >> 16 : 0x100 - (h >> 8) % 12);
    *moving = static_cast<unsigned char>((h >> 20) % 7 == 0 ? 2 : (h >> 20) & 1);
    if (h % 5 == 0) *top = static_cast<unsigned char>(h >> 12);
    if (h % 7 == 0) *state = static_cast<unsigned char>(h >> 4);
    Disturb(3);
    const U r = Mix(4);
    g_last_top = static_cast<unsigned char>(r % 3 == 0 ? r >> 8 : *top);
    return g_last_top;
}
unsigned char __cdecl StubCanUse(U mode, U member, U category, U item) {
    Rec(3, Lo8(mode), Lo8(member), Lo8(category), Lo8(item));
    Disturb(5);
    const U h = Mix(6);
    return static_cast<unsigned char>(h % 3 == 0 ? 0 : h % 3 == 1 ? 1 : h >> 8);
}
void __cdecl StubItemRow(U x, U y, U colour, U category, U id, U count, U dim) {
    Rec(4, Lo16(x), Lo16(y), Lo8(colour), Lo8(category), Lo8(id), Lo8(count), Lo8(dim));
    Disturb(7);
}
unsigned char __cdecl StubCharCount(const unsigned char* text) {
    Rec(5, Id(text), TextHash(text, 17));
    Disturb(8);
    return static_cast<unsigned char>(Mix(9) % 20);   // above 13 now and then: the centring goes negative
}
U __cdecl StubDrawAt(U x, U y, U colour, U count, const unsigned char* text) {
    Rec(6, Lo16(x), Lo16(y), Lo8(colour), Lo8(count), Id(text), TextHash(text, Lo8(count)));
    Disturb(10);
    return Mix(11);
}
unsigned char __cdecl StubCountUsed(U category) {
    Rec(7, Lo8(category));
    Disturb(12);
    return static_cast<unsigned char>(Mix(13));
}
// sprintf: the two conversions, written into the buffer as hex - nothing of
// the CRT (the self-test runs before it is initialised).
int __cdecl StubSprintf(char* dst, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    const U a = va_arg(ap, U);
    const U b = va_arg(ap, U);
    va_end(ap);
    Rec(8, Id(dst), Id(fmt), a, b);
    char* out = dst;
    for (U v : {a, b}) {
        for (int k = 7; k >= 0; --k) *out++ = "0123456789ABCDEF"[(v >> (4 * k)) & 0xF];
        *out++ = ' ';
    }
    *out = 0;
    Disturb(14);
    return static_cast<int>(out - dst);
}
void __cdecl StubFont8(U x, U y, U colour, const unsigned char* text) {
    Rec(9, Lo16(x), Lo16(y), Lo8(colour), Id(text), TextHash(text, 40));
    Disturb(15);
}
void __cdecl StubPieces(U x, U y, const unsigned char* list, U flags) {
    Rec(10, Lo16(x), Lo16(y), Id(list), Lo8(flags));
    Disturb(16);
}
void __cdecl StubPiece(U x, U y, U id, U flags) {
    Rec(11, Lo16(x), Lo16(y), Lo8(id), Lo8(flags));
    Disturb(17);
}
void __cdecl StubScrollBar(const unsigned char* items, U top, U x, U y, U rows, U total, U height) {
    Rec(12, Id(items), Lo8(top), Lo16(x), Lo16(y), Lo8(rows), Lo8(total), Lo8(height));
    Disturb(18);
}
// Group BD's: what each reads of its arguments, from their first instructions
// (docs/battle_draw.md section 3).
U __cdecl StubSkillUsable(U mode, U member, U id) {
    Rec(13, Lo8(mode), Lo8(member), Lo8(id));
    Disturb(19);
    const U h = Mix(20);
    return (h & 0xFFFFFF00u) | (h % 3 == 0 ? 0 : h % 3 == 1 ? 1 : (h >> 8) & 0xFF);
}
void __cdecl StubSkillRow(U x, U y, U colour, U icon, const unsigned char* record, U cost, U dim) {
    Rec(14, Lo16(x), Lo16(y), Lo8(colour), Lo8(icon), Id(record), Lo8(cost), Lo8(dim));
    Disturb(21);
}
U __cdecl StubSkillIcon(U id) {
    Rec(15, Lo8(id));
    Disturb(22);
    return Mix(23);
}
U __cdecl StubSkillCost(U member, U id, U battle) {
    Rec(16, Lo8(member), Lo8(id), battle);
    Disturb(24);
    return Mix(25);
}
const unsigned char* __cdecl StubSkillList(U member, U kind, U battle) {
    Rec(17, Lo8(member), Lo8(kind), battle);
    Disturb(26);
    g_last_list = g_list + Mix(27) % 8;
    return g_last_list;
}
// The tint's two.
U __cdecl StubClutAdjust(U columns, U rows, U red, U green, U blue) {
    Rec(18, columns, rows, red, green, blue);
    return Mix(28);
}
void __cdecl StubClutCycle() { Rec(19); }

// --- the stand-ins: the handlers (as field_misc_fuzz.cpp's) ------------------------
unsigned char* g_prim_now;
U g_prim_bytes;
void DisturbDraw(U salt) {
    const U h = Mix(salt ^ 0x5BD1E995u);
    if (h % 4) return;
    const auto value = static_cast<unsigned char>(h >> 24);
    switch ((h >> 2) % 5) {
    case 0:
    case 1:
        if (g_prim_now) g_prim_now[(h >> 8) % g_prim_bytes] = value;
        break;
    case 2: At(kVertices)[(h >> 8) % 0x80] = value; break;
    case 3: At(kScaleY)[(h >> 8) % 8] = value; break;
    default: At(kDrawTpage)[(h >> 8) % 2] = value; break;
    }
}
void __cdecl StubPrimColor(U r, U gg, U b, U code, U mode, unsigned long* diffuse, unsigned long* specular) {
    d3d_fuzz::Record(1, r, gg, b, code, mode, specular != nullptr);
    DisturbDraw(30);
    *diffuse = Mix(31);
    if (specular) *specular = Mix(32);
}
void __cdecl StubRet(U a) {
    d3d_fuzz::Record(2, a);
    DisturbDraw(33);
}
void __cdecl StubBlend(U code, U mode) {
    d3d_fuzz::Record(3, code, mode);
    DisturbDraw(34);
}
void __cdecl StubShade(U mode) {
    d3d_fuzz::Record(4, mode);
    DisturbDraw(35);
}

const Callees kStandIns = {
    As<void (__cdecl*)(unsigned, unsigned, unsigned, unsigned, unsigned, unsigned long*, unsigned long*)>(
        &StubPrimColor),
    As<void (__cdecl*)(unsigned)>(&StubRet),
    As<void (__cdecl*)(unsigned, unsigned)>(&StubBlend),
    As<void (__cdecl*)(unsigned)>(&StubShade),
    As<void (__cdecl*)(int, int, int, int, int, int)>(&StubBox),
    As<unsigned char (__cdecl*)(unsigned char*, unsigned char*, unsigned char*, unsigned char*)>(&StubListScroll),
    As<unsigned char (__cdecl*)(unsigned, unsigned, unsigned, unsigned)>(&StubCanUse),
    As<void (__cdecl*)(int, int, int, unsigned, unsigned, unsigned, int)>(&StubItemRow),
    As<unsigned char (__cdecl*)(const unsigned char*)>(&StubCharCount),
    As<const unsigned char* (__cdecl*)(int, int, int, int, const unsigned char*)>(&StubDrawAt),
    As<unsigned char (__cdecl*)(unsigned)>(&StubCountUsed),
    StubSprintf,
    As<void (__cdecl*)(int, int, int, const unsigned char*)>(&StubFont8),
    As<void (__cdecl*)(int, int, const unsigned char*, int)>(&StubPieces),
    As<void (__cdecl*)(int, int, unsigned, unsigned)>(&StubPiece),
    As<void (__cdecl*)(const unsigned char*, unsigned, int, int, unsigned, unsigned, unsigned)>(&StubScrollBar),
    As<unsigned (__cdecl*)(unsigned, unsigned, unsigned)>(&StubSkillUsable),
    As<void (__cdecl*)(int, int, int, unsigned, const unsigned char*, unsigned, int)>(&StubSkillRow),
    As<unsigned (__cdecl*)(unsigned)>(&StubSkillIcon),
    As<unsigned (__cdecl*)(unsigned, unsigned, unsigned)>(&StubSkillCost),
    As<const unsigned char* (__cdecl*)(unsigned, unsigned, unsigned)>(&StubSkillList),
    As<long (__cdecl*)(int, int, int, int, int)>(&StubClutAdjust),
    As<void (__cdecl*)()>(&StubClutCycle),
};

// --- the copies --------------------------------------------------------------------
// Every E8 (and AreaMap_TintClut's E9) of each body, by capstone 2026-09-23
// (docs/battle_draw.md section 1); every other jump stays inside, and the COM
// calls go through D3d_Device, which the fuzz replaces.

const void* StubFor(U target) {
    switch (target) {
    case 0x59FBA0: return As<const void*>(&StubPrimColor);
    case 0x437CC0: return As<const void*>(&StubRet);
    case 0x59FCA0: return As<const void*>(&StubBlend);
    case 0x59FD80: return As<const void*>(&StubShade);
    case 0x57CF60: return As<const void*>(&StubBox);
    case 0x57DF00: return As<const void*>(&StubListScroll);
    case 0x57D9A0: return As<const void*>(&StubCanUse);
    case 0x57DBF0: return As<const void*>(&StubItemRow);
    case 0x57D800: return As<const void*>(&StubCharCount);
    case 0x516B30: return As<const void*>(&StubDrawAt);
    case 0x591A80: return As<const void*>(&StubCountUsed);
    case 0x5B9380: return As<const void*>(&StubSprintf);
    case 0x517090: return As<const void*>(&StubFont8);
    case 0x57D910: return As<const void*>(&StubPieces);
    case 0x57D860: return As<const void*>(&StubPiece);
    case 0x57DD10: return As<const void*>(&StubScrollBar);
    case kSkillUsable: return As<const void*>(&StubSkillUsable);
    case kSkillRow: return As<const void*>(&StubSkillRow);
    case kSkillIcon: return As<const void*>(&StubSkillIcon);
    case kSkillCost: return As<const void*>(&StubSkillCost);
    case kSkillList: return As<const void*>(&StubSkillList);
    case 0x5718F0: return As<const void*>(&StubClutAdjust);
    case 0x5717B0: return As<const void*>(&StubClutCycle);
    default: bof3::Fatal("battle_draw: no stand-in for a call to 0x%X", (unsigned)target);
    }
}

struct Site {
    U offset, target;
};
constexpr Site kPolyG3Sites[] = {{0x34, 0x59FBA0},  {0x65, 0x59FBA0},  {0x96, 0x59FBA0}, {0x17A, 0x437CC0},
                                 {0x181, 0x437CC0}, {0x199, 0x59FCA0}, {0x1A0, 0x59FD80}};
constexpr Site kLineG2Sites[] = {{0x34, 0x59FBA0},  {0x65, 0x59FBA0},  {0xFF, 0x437CC0},
                                 {0x106, 0x437CC0}, {0x11E, 0x59FCA0}, {0x125, 0x59FD80}};
constexpr Site kLineG3Sites[] = {{0x34, 0x59FBA0},  {0x65, 0x59FBA0},  {0x96, 0x59FBA0}, {0x170, 0x437CC0},
                                 {0x177, 0x437CC0}, {0x18F, 0x59FCA0}, {0x196, 0x59FD80}};
constexpr Site kItemListSites[] = {
    {0x30, 0x57CF60},  {0x47, 0x57DF00},  {0xD5, 0x57D9A0},  {0x175, 0x57DBF0}, {0x19C, 0x57DBF0},
    {0x200, 0x57CF60}, {0x229, 0x57CF60}, {0x261, 0x57D800}, {0x27F, 0x516B30}, {0x29B, 0x591A80},
    {0x2B3, 0x5B9380}, {0x2DA, 0x517090}, {0x300, 0x57D910}, {0x326, 0x57D910}, {0x361, 0x57D860},
    {0x38C, 0x57D860}, {0x3B2, 0x57D860}, {0x3DB, 0x57D860}, {0x400, 0x57D860}, {0x417, 0x57D860},
    {0x43F, 0x57D860}, {0x464, 0x57D860}, {0x48C, 0x57D860}, {0x4B1, 0x57D860}, {0x4E3, 0x57DD10}};
constexpr Site kSkillListSites[] = {
    {0x2E, 0x57CF60},  {0x45, 0x57DF00},  {0x58, 0x591E50},  {0xB3, 0x57DA70},  {0xEB, 0x591DB0},
    {0xF7, 0x5918A0},  {0x179, 0x57DC90}, {0x1A3, 0x57DC90}, {0x204, 0x57CF60}, {0x250, 0x516B30},
    {0x276, 0x57D910}, {0x29C, 0x57D910}, {0x2D2, 0x57D860}, {0x2FD, 0x57D860}, {0x323, 0x57D860},
    {0x34C, 0x57D860}, {0x371, 0x57D860}, {0x388, 0x57D860}, {0x3B0, 0x57D860}, {0x3D8, 0x57D860},
    {0x3EF, 0x57D860}, {0x41E, 0x591E50}, {0x427, 0x57DD10}};
constexpr Site kTintSites[] = {{0x1A, 0x5718F0}, {0x22, 0x5717B0}};

void* Clone(const char* name, U base, U size, const Site* sites, int n) {
    bof3::CloneCall calls[32];
    if (n > 32) bof3::Fatal("battle_draw: %s has %d calls", name, n);
    for (int i = 0; i < n; ++i) calls[i] = {sites[i].offset, StubFor(sites[i].target), sites[i].target};
    void* code = bof3::CloneOriginal(name, base, size, calls, n);
    if (!code) bof3::Fatal("battle_draw: CloneOriginal(%s) returned null", name);
    return code;
}

// --- the comparison ------------------------------------------------------------------

State g_start, g_after_theirs, g_after_ours;
d3d_fuzz::Log g_theirs, g_ours;

struct Tally {
    const char* name;
    unsigned rounds, bad, calls, hits;   // hits: a coverage count each test defines
};

void Mismatch(Tally& t, const char* why) {
    if (t.bad < 4) bof3::Log("shadow      battle_draw MISMATCH: %s round %u: %s", t.name, t.rounds - 1, why);
    ++t.bad;
}

// One round of a list or the tint: the state already generated.
template <typename Theirs, typename Ours>
void PassOwn(Tally& t, const Region* rs, int nr, Theirs theirs, Ours ours) {
    Capture(rs, nr, g_start);
    g_lists = true;
    g_theirs_log.n = 0;
    g_log = &g_theirs_log;
    g_last_top = 0;
    g_last_list = g_list;
    theirs();
    Capture(rs, nr, g_after_theirs);
    Restore(rs, nr, g_start);
    g_ours_log.n = 0;
    g_log = &g_ours_log;
    g_last_top = 0;
    g_last_list = g_list;
    ours();
    Capture(rs, nr, g_after_ours);
    g_log = nullptr;
    ++t.rounds;
    t.calls += g_theirs_log.n;
    char why[200] = "";
    bool same = SameOwnLog(g_ours_log, g_theirs_log, why);
    U where = 0;
    if (same && FirstDifference(rs, nr, g_after_ours, g_after_theirs, &where)) {
        std::snprintf(why, sizeof why, "memory at 0x%X", (unsigned)where);
        same = false;
    }
    if (!same) Mismatch(t, why);
}

// One round of a handler: the result compared as well.
template <typename Theirs, typename Ours>
void PassDraw(Tally& t, const Region* rs, int nr, Theirs theirs, Ours ours) {
    Capture(rs, nr, g_start);
    g_lists = false;
    g_theirs.Clear();
    d3d_fuzz::g_log = &g_theirs;
    const U ret_theirs = theirs();
    Capture(rs, nr, g_after_theirs);
    Restore(rs, nr, g_start);
    g_ours.Clear();
    d3d_fuzz::g_log = &g_ours;
    const U ret_ours = ours();
    Capture(rs, nr, g_after_ours);
    d3d_fuzz::g_log = nullptr;
    ++t.rounds;
    t.calls += g_theirs.n;
    char why[200] = "";
    bool same = d3d_fuzz::SameLog(g_ours, g_theirs, why);
    if (same && ret_ours != ret_theirs) {
        std::snprintf(why, sizeof why, "the result %08X, the original %08X", (unsigned)ret_ours, (unsigned)ret_theirs);
        same = false;
    }
    U where = 0;
    if (same && FirstDifference(rs, nr, g_after_ours, g_after_theirs, &where)) {
        std::snprintf(why, sizeof why, "memory at 0x%X", (unsigned)where);
        same = false;
    }
    if (!same) Mismatch(t, why);
}

// --- the handlers ----------------------------------------------------------------------

unsigned short GetControlWord() {
    unsigned short cw;
    __asm__ volatile("fnstcw %0" : "=m"(cw));
    return cw;
}
void SetControlWord(unsigned short cw) { __asm__ volatile("fldcw %0" : : "m"(cw)); }
const unsigned short kControlWords[] = {0x027F, 0x007F, 0x037F};

// group R's float seeds (d3d-draw.md section 5) and scales
const U kFloats[] = {
    0x00000000, 0x80000000, 0x3F800000, 0x3F000000, 0x43200000, 0xC2000000, 0x3C23D70A,  // 0 -0 1 .5 160 -32 .01
    0x7149F2CA, 0x7F7FFFFF, 0x00800000, 0x007FFFFF, 0x00000001, 0x7F800000, 0xFF800000,  // 1e30 max min-normal denormals inf
    0x7FC00000, 0x7FA00000, 0xFF800001, 0x3F7D70A4, 0x4B7FFFFF, 0x3EAAAAAB, 0xBF800000,  // qNaN sNaN sNaN .99 2^24-1 1/3 -1
};
const U kScales[] = {0x40000000, 0x3F800000, 0x3FC00000, 0x40400000, 0x3F000000, 0xC0000000, 0x40100000,
                     0x0DA24260, 0x7F000000, 0x3F800001};

struct Handler {
    const char* name;
    U base, size;
    const Site* sites;
    int n_sites;
    const void* ours;
    U prim_bytes;
    U corners;   // Gouraud: colours at +4 + i * 0x10, float corners from +8 + i * 0x10
};

void FillRandom(U at, U bytes) {
    for (U i = 0; i < bytes; i += 4) PutLong(At(at + i), Next());
}

void FuzzHandler(Tally& t, const Handler& h, void* clone, unsigned rounds) {
    using Fn = U(__cdecl*)(unsigned char*);
    const Region regions[] = {{kVertices, 0x80}, {kScaleY, 8}, {kDrawTpage, 4}, {Addr(g_prim), sizeof g_prim}};
    const unsigned short saved = GetControlWord();
    for (unsigned r = 0; r < rounds; ++r) {
        g_round = (h.base << 4) + r;
        for (U a : {kScaleX, kScaleY})
            PutLong(At(a), Next() % 5 == 0 ? Next() : Next() % 5 == 1 ? 0x3F800000u + Next() % 0x2000000u
                                                                         : kScales[Next() % 10]);
        FillRandom(kVertices, 0x80);
        PutLong(At(kDrawTpage), Next() % 2 ? Next() : (Next() & 0xFFFF0000u) | Pick({0, 0x20, 0x40, 0x60, 0xFFFF}));
        for (U i = 0; i < sizeof g_prim; ++i) g_prim[i] = static_cast<unsigned char>(Next());
        for (U c = 0; c < h.corners; ++c) {
            for (U f = 0; f < 3; ++f)
                if (Next() % 4) PutLong(g_prim + 8 + c * 0x10 + f * 4, Next() % 3 ? kFloats[Next() % 21] : Next());
            for (U i = 0; i < 3; ++i)
                if (Next() % 2) g_prim[4 + c * 0x10 + i] = static_cast<unsigned char>(Pick({0, 1, 0x7F, 0x80, 0xFF}));
        }
        if (Next() % 2) g_prim[7] = static_cast<unsigned char>(Pick({0x30, 0x31, 0x32, 0x33, 0x50, 0x52, 0x58, 0x5A}));
        g_prim_now = g_prim;
        g_prim_bytes = h.prim_bytes;
        const unsigned short cw = kControlWords[Next() % 3];
        if (cw == 0x007F) ++t.hits;
        PassDraw(t, regions, 4,
                 [&] {
                     SetControlWord(cw);
                     const U ret = reinterpret_cast<Fn>(clone)(g_prim);
                     SetControlWord(saved);
                     return ret;
                 },
                 [&] {
                     SetControlWord(cw);
                     const U ret = reinterpret_cast<Fn>(const_cast<void*>(h.ours))(g_prim);
                     SetControlWord(saved);
                     return ret;
                 });
        g_prim_now = nullptr;
    }
}

// --- the lists -------------------------------------------------------------------------

const Region kListRegions[] = {
    {Addr(g_window), sizeof g_window}, {Addr(g_list), sizeof g_list}, {kColour, 1}, {kMenuMember, 1},
    {kPartyFrom, kPartyBytes},         {kInvFrom, kInvBytes},         {kPrintBuf, 0x60},
};

// The state both lists read: the window record with each branch's boundaries
// seeded, the inventory (a third of its ids 0), the party lists, the menu
// member, the window colour, the skill list.
void SeedList(bool skill) {
    g_skill = skill;
    for (U i = 0; i < sizeof g_window; ++i) g_window[i] = static_cast<unsigned char>(Next());
    PutWord(g_window + 4, Next() % 3 ? Next() % 0x140 : Pick({0, 0xFFFF, 0x7FFF, 0x8000, Next() & 0xFFFF}));
    PutWord(g_window + 6, Next() % 3 ? Next() % 0xF0 : Pick({0, 0xFFFF, 0xFFE6, 0x8000, Next() & 0xFFFF}));
    g_window[9] = static_cast<unsigned char>(Next() % 2 ? 0 : Pick({1, 2, 3, 0x10, 0x13, 0xF0, 0xFF, Next() & 0xFF}));
    const unsigned char top = static_cast<unsigned char>(Next() % 3 ? Next() % 4 : Next() & 0xFF);
    if (skill) {
        g_window[0xB] = static_cast<unsigned char>(Next() % 5);
        g_window[0x10] = top;
        g_window[0x11] = static_cast<unsigned char>(Next() % 3 ? 0 : Pick({0xFF, 1, 0x80}));
        PutWord(g_window + 0x14, Next() % 2 ? 0 : Pick({1, 0x100, 0x8000, Next() & 0xFFFF}));
        // the cursor as a row index and as top + row: the two tests the original makes
        g_window[0xD] = static_cast<unsigned char>(Next() % 2 ? Next() % 9 : top + Next() % 9);
        g_window[0xC] = static_cast<unsigned char>(Next() % 2 ? Next() % 9 : top + Next() % 9);
    } else {
        g_window[0xA] = static_cast<unsigned char>(Next() % 5);
        g_window[0xB] = top;
        g_window[0xD] = static_cast<unsigned char>(Next() % 3 ? top + Next() % 9 : Next() & 0xFF);
        g_window[0xC] = static_cast<unsigned char>(Next() % 3 ? top + Next() % 9 : Next() & 0xFF);
        PutWord(g_window + 0x10, Next() % 2 ? Next() & 3 : Next() & 0xFFFF);
    }
    g_window[0x12] = static_cast<unsigned char>(Next() % 2 ? 0 : Next() & 0xFF);
    for (U i = 0; i < kInvBytes; ++i)
        At(kInvFrom + i)[0] = static_cast<unsigned char>(Next() % 3 == 0 ? 0 : Pick({1, 2, 99, 0x80, 0xFF, Next() & 0xFF}));
    for (U i = 0; i < sizeof g_list; ++i) g_list[i] = static_cast<unsigned char>(Next() % 3 == 0 ? 0 : Next() & 0xFF);
    for (U i = 0; i < kPartyBytes; ++i) At(kPartyFrom + i)[0] = static_cast<unsigned char>(Next() % 9);
    At(kMenuMember)[0] = static_cast<unsigned char>(Next() % 4 ? Next() % 3 : Next() & 0xFF);
    At(kColour)[0] = static_cast<unsigned char>(Next() % 2 ? Next() % 8 : Next() & 0xFF);
    std::memset(At(kPrintBuf), 0x5A, 0x60);
}

void FuzzList(Tally& t, void* clone, const void* ours, bool skill, unsigned rounds) {
    using Fn = void(__cdecl*)(unsigned char*);
    const int nr = static_cast<int>(sizeof kListRegions / sizeof kListRegions[0]);
    for (unsigned r = 0; r < rounds; ++r) {
        g_round = (skill ? 0x700000u : 0x600000u) + r;
        SeedList(skill);
        // coverage: a round whose window is scrolled with its cursor or mark
        // in view (the raised rows)
        if ((skill ? g_window[0x10] : g_window[0xB]) != 0) ++t.hits;
        PassOwn(t, kListRegions, nr, [&] { reinterpret_cast<Fn>(clone)(g_window); },
                [&] { reinterpret_cast<Fn>(const_cast<void*>(ours))(g_window); });
    }
}

// --- the tint -----------------------------------------------------------------------------

void FuzzTint(Tally& t, void* clone, unsigned rounds) {
    using Fn = void(__cdecl*)(int);
    const Region regions[] = {{kStatusBits, 1}};
    for (unsigned r = 0; r < rounds; ++r) {
        g_round = 0x800000u + r;
        At(kStatusBits)[0] = static_cast<unsigned char>(Next() % 2 ? Next() & 0xFE : Next() | 1);
        if (!(At(kStatusBits)[0] & 1)) ++t.hits;
        const int level = static_cast<int>(Next() % 2 ? Pick({0, 1, 0xFFFFFFFFu, 0x7F, 0xFFFFFF80u, 0x1F, 0xFFFFFFE1u})
                                                      : Next());
        PassOwn(t, regions, 1, [&] { reinterpret_cast<Fn>(clone)(level); },
                [&] { As<Fn>(&AreaMap_TintClut)(level); });
    }
}

}  // namespace

void SelfTest() {
    // The copies, before BattleDraw_Inject patches anything.
    const Handler handlers[] = {
        {"D3d_DrawPolyG3", 0x5A0E80, 0x1C8, kPolyG3Sites, 7, As<const void*>(&D3d_DrawPolyG3), 0x34, 3},
        {"D3d_DrawLineG2", 0x5A18B0, 0x14D, kLineG2Sites, 6, As<const void*>(&D3d_DrawLineG2), 0x24, 2},
        {"D3d_DrawLineG3", 0x5A1B50, 0x1BE, kLineG3Sites, 7, As<const void*>(&D3d_DrawLineG3), 0x34, 3},
    };
    void* draws[3];
    for (int i = 0; i < 3; ++i)
        draws[i] = Clone(handlers[i].name, handlers[i].base, handlers[i].size, handlers[i].sites, handlers[i].n_sites);
    void* item_list = Clone("BattleMenu_DrawItemList", 0x59CD00, 0x4F2, kItemListSites, 25);
    void* skill_list = Clone("BattleMenu_DrawSkillList", 0x59D200, 0x436, kSkillListSites, 23);
    void* tint = Clone("AreaMap_TintClut", 0x573050, 0x28, kTintSites, 2);

    const int n_all = static_cast<int>(sizeof g_all / sizeof g_all[0]);
    static State saved;
    if (RegionBytes(g_all, n_all) > kMaxState || RegionBytes(kListRegions, 7) > kMaxState)
        bof3::Fatal("battle_draw: the saved regions outgrow the state buffer");
    Capture(g_all, n_all, saved);
    const Callees saved_callees = g;
    g = kStandIns;
    d3d_fuzz::Seed(0x424A424Au);

    Tally tallies[] = {
        {"D3d_DrawPolyG3", 0, 0, 0, 0},          {"D3d_DrawLineG2", 0, 0, 0, 0},
        {"D3d_DrawLineG3", 0, 0, 0, 0},          {"BattleMenu_DrawItemList", 0, 0, 0, 0},
        {"BattleMenu_DrawSkillList", 0, 0, 0, 0}, {"AreaMap_TintClut", 0, 0, 0, 0},
    };
    {
        d3d_fuzz::DeviceSwap swap;
        for (int i = 0; i < 3; ++i) FuzzHandler(tallies[i], handlers[i], draws[i], 20000);
    }
    FuzzList(tallies[3], item_list, As<const void*>(&BattleMenu_DrawItemList), false, 20000);
    FuzzList(tallies[4], skill_list, As<const void*>(&BattleMenu_DrawSkillList), true, 20000);
    FuzzTint(tallies[5], tint, 2000);

    g = saved_callees;
    Restore(g_all, n_all, saved);

    unsigned bad = 0, rounds = 0;
    for (const Tally& t : tallies) {
        bad += t.bad;
        rounds += t.rounds;
        bof3::Log("shadow      battle_draw self-test: %s %u rounds, %u calls out, %u covered, %u MISMATCHES", t.name,
                  t.rounds, t.calls, t.hits, t.bad);
    }
    bof3::Log("shadow      battle_draw self-test: %u rounds over 6 functions, %u MISMATCHES", rounds, bad);
    if (bad) bof3::Fatal("group BJ's functions differ from the original in %u of %u self-test rounds", bad, rounds);
}

}  // namespace battle_draw

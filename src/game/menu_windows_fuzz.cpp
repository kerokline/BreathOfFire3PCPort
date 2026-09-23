// BOF3X_SHADOW=menu_windows: a differential fuzz of the menu and shop windows,
// once at start-up. docs/menu-windows.md section 4.
//
// Thirty-seven byte-copies, every call out re-aimed at a recording stand-in
// (for ours through menu_windows::g alike), the two jump tables relocated in
// the copies. One round: one function, random bytes in every region any of
// them reads or writes, then that function's branch boundaries seeded; the
// copy, then from the same state ours, both under the game's x87 control word
// 0x027F; the regions, the packet pool and the packet cursor, our own window
// records, lists and texts, the answer (at the width the original defines)
// and the stand-ins' log compared.
//
// The stand-ins do what their callers read back: the commit moves the packet
// cursor, the primitive setters write their code bytes, the draw mode its
// words, sprintf the print buffer (nothing here may reach the CRT: the
// self-test runs before it is initialised), the name and message lookups
// answer strings of ours, the scroll step writes its outputs, 0x590960 its
// eight; and most disturb a cell some caller reads again after the call (the
// window colour, the selection, the pad, the window record's fields, the list
// count, the current record pointer, the inventory, the gold).
#include <cstdarg>
#include <initializer_list>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/menu_windows_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"
#include <windows.h>

namespace menu_windows {
namespace {

using move_script::At;
using move_script::Long;
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

std::uint32_t g_rng = 0x6A09E667u;
std::uint32_t Next() { g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5; return g_rng; }
bool Half() { return (Next() & 1) != 0; }
bool Often() { return Next() % 3 != 0; }
std::uint32_t Garbage(std::uint32_t low_mask, std::uint32_t value) { return (Next() & ~low_mask) | (value & low_mask); }

constexpr unsigned kPool = 0x800;
constexpr unsigned kWindows = 4, kWindowBytes = 0x40;
constexpr unsigned kTexts = 4, kTextBytes = 0x100;
constexpr unsigned kNames = 8, kNameBytes = 0x20;
constexpr unsigned kListBytes = 0x210;
constexpr unsigned kMisc = 0x200;   // piece rectangles, piece lists, scroll items, the scroll step's outputs

struct Buffers {
    unsigned char pool[kPool];
    unsigned char windows[kWindows][kWindowBytes];
    unsigned char texts[kTexts][kTextBytes];
    unsigned char names[kNames][kNameBytes];
    unsigned char list[kListBytes];
    unsigned char misc[kMisc];
};
Buffers g_buf;

constexpr unsigned kLog = 160;
struct Entry { std::uint32_t what, a, b, c, d, e, f, h; };
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
// BOF3X_MW_TRACE=<round>: every record of that round into the log (debugging
// a fault inside a round, which otherwise leaves nothing).
bool g_tracing;
void Record(std::uint32_t what, std::uint32_t a = 0, std::uint32_t b = 0, std::uint32_t c = 0, std::uint32_t d = 0,
            std::uint32_t e = 0, std::uint32_t f = 0, std::uint32_t h = 0) {
    if (g_tracing) bof3::Log("menu_windows trace:   %u (%X %X %X %X %X %X %X)", what, a, b, c, d, e, f, h);
    if (g_log_n < kLog) g_log[g_log_n] = {what, a, b, c, d, e, f, h};
    ++g_log_n;
}

// A pointer as both passes can compare it: an offset inside our buffers,
// else the address (the game's own tables are at the same place on both).
std::uint32_t Id(const void* p) {
    const std::uint32_t at = Address(p), base = Address(&g_buf);
    if (at >= base && at < base + sizeof g_buf) return 0x1000000u + (at - base);
    return at;
}
// A string's bytes, to its NUL or n of them, as a hash - so that what a draw
// was handed is compared, not only where it was.
std::uint32_t TextHash(const unsigned char* s, unsigned n) {
    std::uint32_t h = 0x811C9DC5u;
    for (unsigned i = 0; i < n && i < 40 && s[i]; ++i) h = (h ^ s[i]) * 0x01000193u;
    return h;
}

// --- the cells the functions read again after a call ------------------------

unsigned char* Window(unsigned k) { return g_buf.windows[k]; }

void Disturb() {
    const std::uint32_t h = Hash();
    if (h % 4 == 0) return;
    const unsigned v = (h >> 12) & 0xFF;
    switch ((h >> 4) % 16) {
    case 0: At(at::kColour)[0] = static_cast<unsigned char>(v); break;
    case 1: At(at::kYesNo)[0] = static_cast<unsigned char>(v % 3 == 0 ? v : v & 1); break;
    case 2: At(at::kInputPressed + (v & 1))[0] = static_cast<unsigned char>(h >> 20); break;
    case 3: At(at::kListCount)[0] = static_cast<unsigned char>(static_cast<signed char>(v % 9) - 1); break;
    case 4: At(at::kListSet)[0] = static_cast<unsigned char>(v % 4); break;
    case 5: SetLong(At(at::kCurrent), static_cast<std::int32_t>(Address(Window(v % kWindows)))); break;
    case 6:
    case 7: {
        // a byte of the window records' fields. +0xA stays a category the
        // item list can index (0..4) - and a 4 stays a 4: key items have no
        // counts array (0x656B24 is 0), and the list takes its pointers from
        // the category once but tests it again per row, so a key-item list
        // turned into another category under it reads through that null in
        // the original too (docs/menu-windows.md section 4).
        unsigned char* const w = Window(v % kWindows);
        const unsigned field = 4 + (h >> 20) % 0x10;
        if (field == 0xA) {
            if (w[0xA] != 4) w[0xA] = static_cast<unsigned char>((h >> 24) % 4);
        } else {
            w[field] = static_cast<unsigned char>(h >> 24);
        }
        break;
    }
    case 8: g_buf.list[0] = static_cast<unsigned char>(v % 14); break;
    case 9: {
        unsigned char* const r = At(at::kCharRecords + (v % 8) * at::kCharStride);
        static const unsigned kFields[] = {9, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29};
        r[kFields[(h >> 20) % 13]] = static_cast<unsigned char>(h >> 24);
        break;
    }
    case 10: At(0x904154 + (h >> 16) % 0x480)[0] = static_cast<unsigned char>(v); break;
    case 11: At(at::kGold + (v & 3))[0] = static_cast<unsigned char>(h >> 24); break;
    case 12: At(at::kInputFlags)[0] = static_cast<unsigned char>(v); break;
    case 13: At(at::kCondFA)[0] = static_cast<unsigned char>(v % 3 == 0 ? v : 6 + v % 4); break;
    default: break;
    }
}

// --- the stand-ins ----------------------------------------------------------
// Each records what the real callee reads of its arguments (a 16-bit
// coordinate, a colour byte ...), so what the original leaves in the upper
// bits of a register it pushes is masked exactly as the real callee masks it.

constexpr std::uint32_t kPointZeroOne = 0x3C23D70Au;
void PutDword(unsigned char* at, std::uint32_t v) { std::memcpy(at, &v, sizeof v); }
std::uint32_t Lo16(int v) { return static_cast<std::uint32_t>(v) & 0xFFFF; }
std::uint32_t Lo8(unsigned v) { return v & 0xFF; }

const unsigned char* __cdecl StubDrawAt(int x, int y, int colour, int count, const unsigned char* text) {
    Record(1, Lo16(x), Lo16(y), Lo8(static_cast<unsigned>(colour)), Lo8(static_cast<unsigned>(count)), Id(text),
           TextHash(text, static_cast<unsigned>(count) & 0xFF));
    Disturb();
    return text + (Hash() & 7);
}
const unsigned char* __cdecl StubVerbLabel(int x, int y, int colour, int count, const unsigned char* text) {
    Record(2, Lo16(x), Lo16(y), Lo8(static_cast<unsigned>(colour)), Lo8(static_cast<unsigned>(count)), Id(text),
           TextHash(text, static_cast<unsigned>(count) & 0xFF));
    Disturb();
    return text;
}
const unsigned char* __cdecl StubImmediate(int x, int y, const unsigned char* text) {
    Record(3, Lo16(x), Lo16(y), Id(text), TextHash(text, 16));
    Disturb();
    return text + 1 + Hash() % 8;
}
const unsigned char* __cdecl StubSystemPtr(unsigned id) {
    Record(4, id & 0xFFFF);
    Disturb();
    return g_buf.texts[3] + Hash() % 0x40;
}
const unsigned char* __cdecl StubYesNoLine(unsigned id) {
    Record(5, id & 0xFFFF);
    Disturb();
    return g_buf.texts[2] + Hash() % 0x40;
}
void __cdecl StubReset() { Record(6); Disturb(); }
void __cdecl StubSound(unsigned id) { Record(7, id & 0xFFFF); Disturb(); }
void __cdecl StubDrawMode(unsigned char* prim, int dfe, int dtd, unsigned tpage, unsigned long tw) {
    Record(8, Id(prim), dfe != 0, dtd != 0, tpage, Id(reinterpret_cast<const void*>(tw)));
    PutDword(prim + 4, 0xE8000000u | (tpage & 0xFFFFu));
    if (dfe) prim[6] = static_cast<unsigned char>(prim[6] | 1);
    if (dtd) prim[6] = static_cast<unsigned char>(prim[6] | 2);
    PutDword(prim + 8, static_cast<std::uint32_t>(tw));
    Disturb();
}
// The commit moves the packet cursor, as the real one does, so that a cursor
// read too early shows; it wraps in the first 0x200 bytes of our pool, and
// stamps the committed primitive's tag (the real one links it).
void __cdecl StubCommit(unsigned slot, unsigned size) {
    unsigned char* const at = Gfx_PacketNext;
    Record(9, slot & 0xFF, size & 0xFF, Id(at));
    PutDword(at, 0xC0000000u | (Hash() & 0xFFFFFF));
    const unsigned offset = static_cast<unsigned>(at - g_buf.pool);
    Gfx_PacketNext = g_buf.pool + (offset + (size & 0xFF) + (Hash() % 3 == 0 ? 4 : 0)) % 0x200u;
    Disturb();
}
void __cdecl StubSetSprt(unsigned char* prim) { Record(10, Id(prim)); prim[7] = 0x64; PutDword(prim + 0x10, kPointZeroOne); }
void __cdecl StubSetSprt8(unsigned char* prim) { Record(11, Id(prim)); prim[7] = 0x74; PutDword(prim + 0x10, kPointZeroOne); }
void __cdecl StubSetSemi(unsigned char* prim, unsigned abe) {
    Record(12, Id(prim), abe & 0xFF);
    prim[7] = static_cast<unsigned char>((abe & 1) ? prim[7] | 2 : prim[7] & 0xFD);
}
unsigned __cdecl StubGetTPage(unsigned tp, unsigned abr, int x, int y) {
    Record(13, tp & 0xFF, abr & 0xFF, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y));
    return Hash();
}
unsigned __cdecl StubGetClut(int x, int y) {
    Record(14, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y));
    return Hash() >> 3;
}
void __cdecl StubPolyFT4(unsigned char* prim) {
    Record(15, Id(prim));
    prim[7] = 0x2C;
    for (unsigned at = 0x10; at <= 0x40; at += 0x10) PutDword(prim + at, kPointZeroOne);
}
void __cdecl StubLineF2(unsigned char* prim) { Record(16, Id(prim)); prim[7] = 0x40; PutDword(prim + 0x10, kPointZeroOne); PutDword(prim + 0x1C, kPointZeroOne); }
void __cdecl StubLineF3(unsigned char* prim) {
    Record(17, Id(prim));
    prim[7] = 0x48;
    for (unsigned at = 0x10; at <= 0x28; at += 0xC) PutDword(prim + at, kPointZeroOne);
}
void __cdecl StubLineF4(unsigned char* prim) {
    Record(18, Id(prim));
    prim[7] = 0x4C;
    for (unsigned at = 0x10; at <= 0x34; at += 0xC) PutDword(prim + at, kPointZeroOne);
}
void __cdecl StubTile(unsigned char* prim) { Record(19, Id(prim)); prim[7] = 0x60; PutDword(prim + 0x10, kPointZeroOne); }
unsigned char __cdecl StubAreaByte(int x, int y) {
    Record(20, Lo16(x), Lo16(y));
    Disturb();
    const std::uint32_t h = Hash();
    return static_cast<unsigned char>(h % 2 ? 0xAE : h >> 8);
}
// sprintf: the conversions the format names (1 or 2 here), written into the
// buffer as hex - nothing of the CRT.
int __cdecl StubSprintf(char* dst, const char* fmt, ...) {
    unsigned n = 0;
    for (const char* p = fmt; *p; ++p)
        if (p[0] == '%' && p[1] != '%') ++n;
    va_list ap;
    va_start(ap, fmt);
    const std::uint32_t a = n > 0 ? va_arg(ap, std::uint32_t) : 0;
    const std::uint32_t b = n > 1 ? va_arg(ap, std::uint32_t) : 0;
    va_end(ap);
    Record(21, Id(dst), Id(fmt), n, a, b);
    char* out = dst;
    for (std::uint32_t v : {a, b, Address(fmt) & 0xFF}) {
        for (int k = 7; k >= 0; --k) *out++ = "0123456789ABCDEF"[(v >> (4 * k)) & 0xF];
        *out++ = ' ';
    }
    *out = 0;
    Disturb();
    return static_cast<int>(out - dst);
}
void __cdecl StubStatIcon(unsigned kind, int x, int y, unsigned w, unsigned h, unsigned shade) {
    Record(22, kind & 0xFF, Lo16(x), Lo16(y), w & 0xFF, h & 0xFF, shade & 0xFF);
    Disturb();
}
void __cdecl StubHand(int x, int y, int) { Record(23, Lo16(x), Lo16(y)); Disturb(); }
void __cdecl StubCompare(unsigned member, unsigned slot, unsigned item, unsigned char* colours, unsigned short* stats) {
    Record(24, member & 0xFF, slot & 0xFF, item & 0xFF);
    static const unsigned char kColours[] = {0, 2, 3};
    for (unsigned i = 0; i < 4; ++i) {
        colours[i] = kColours[(Hash() >> (3 * i)) % 3];
        stats[i] = static_cast<unsigned short>(Hash() >> (4 * i));
    }
    Disturb();
}
const unsigned char* __cdecl StubItemName(unsigned category, unsigned id) {
    Record(25, category & 0xFF, id & 0xFF);
    Disturb();
    return g_buf.names[Hash() % kNames];
}
unsigned __cdecl StubItemKind(unsigned category, unsigned id) {
    Record(26, category & 0xFF, id & 0xFF);
    Disturb();
    return (Hash() & 0xFFFFFF00u) | (Hash() >> 8 & 0xF);
}
unsigned __cdecl StubEquipMask(unsigned category, unsigned id) {
    Record(27, category & 0xFF, id & 0xFF);
    Disturb();
    return Hash();
}
unsigned __cdecl StubItemFlags(unsigned category, unsigned id) {
    Record(28, category & 0xFF, id & 0xFF);
    Disturb();
    return Hash();
}
unsigned __cdecl StubHasKeyItem(unsigned id) {
    Record(29, id & 0xFF);
    Disturb();
    const std::uint32_t h = Hash();
    return h % 2 ? (h & 0xFFFFFF00u) : h;
}
unsigned __cdecl StubCountOwned(unsigned category, unsigned id, unsigned where) {
    Record(30, category & 0xFF, id & 0xFF, where & 0xFF);
    Disturb();
    const std::uint32_t h = Hash();
    return h % 2 ? (h & 0xFFFF0000u) : h;
}
unsigned __cdecl StubCountCategory(unsigned category) {
    Record(31, category & 0xFF);
    Disturb();
    return Hash();
}
unsigned __cdecl StubPriceScale(unsigned price, unsigned percent) {
    Record(32, price, percent & 0xFFFF);
    Disturb();
    const std::uint32_t h = Hash();
    // around the money seeded (Seed puts the gold near 1000)
    return h % 3 == 0 ? h : 990 + h % 20;
}
unsigned __cdecl StubSellPrice(unsigned category, unsigned id, unsigned flag) {
    Record(33, category & 0xFF, id & 0xFF, flag & 0xFF);
    Disturb();
    return Hash();
}
void __cdecl StubFont12(int x, int y, int colour, const unsigned char* text) {
    Record(34, Lo16(x), Lo16(y), Lo8(static_cast<unsigned>(colour)), Id(text), TextHash(text, 40));
    Disturb();
}
void __cdecl StubFont8(int x, int y, int colour, const unsigned char* text) {
    Record(35, Lo16(x), Lo16(y), Lo8(static_cast<unsigned>(colour)), Id(text), TextHash(text, 40));
    Disturb();
}
void __cdecl StubPanel(int x, int y, int w, int h) {
    Record(36, Lo16(x), Lo16(y), Lo8(static_cast<unsigned>(w)), Lo8(static_cast<unsigned>(h)));
    Disturb();
}
void __cdecl StubBox(int x, int y, int w, int h, int flags, int colour) {
    Record(37, Lo16(x), Lo16(y), Lo16(w), Lo16(h), Lo8(static_cast<unsigned>(flags)), Lo8(static_cast<unsigned>(colour)));
    Disturb();
}
void __cdecl StubIcon8(int x, int y, int icon, int dim) {
    Record(38, Lo16(x), Lo16(y), Lo8(static_cast<unsigned>(icon)), Lo8(static_cast<unsigned>(dim)));
    Disturb();
}
unsigned char __cdecl StubCharCount(const unsigned char* text) {
    Record(39, Id(text), TextHash(text, 17));
    Disturb();
    return static_cast<unsigned char>(Hash() % 11);
}
void __cdecl StubPieces(int x, int y, const unsigned char* list, int flags) {
    Record(40, Lo16(x), Lo16(y), Id(list), Lo8(static_cast<unsigned>(flags)));
    Disturb();
}
void __cdecl StubPiece(int x, int y, unsigned id, unsigned flags) {
    Record(41, Lo16(x), Lo16(y), id & 0xFF, flags & 0xFF);
    Disturb();
}
const unsigned char* __cdecl StubPieceRect(unsigned id, unsigned flags) {
    Record(42, id & 0xFF, flags & 0xFF);
    Disturb();
    return g_buf.misc + (Hash() % 0x40) * 4;
}
void __cdecl StubLine(int x0, int y0, int x1, int y1, int r, int gr, int b, int abr) {
    Record(43, Lo16(x0), Lo16(y0), Lo16(x1), Lo16(y1),
           Lo8(static_cast<unsigned>(r)) | Lo8(static_cast<unsigned>(gr)) << 8 | Lo8(static_cast<unsigned>(b)) << 16,
           static_cast<unsigned>(abr) & 3);
    Disturb();
}
void __cdecl StubOutline(int x, int y, int w, int h, int flags) {
    Record(44, Lo16(x), Lo16(y), Lo16(w), Lo16(h), static_cast<unsigned>(flags) & 1);
    Disturb();
}
void __cdecl StubItemIcon(int x, int y, int icon, int shade) {
    Record(45, Lo16(x), Lo16(y), Lo8(static_cast<unsigned>(icon)), Lo8(static_cast<unsigned>(shade)));
    Disturb();
}
void __cdecl StubIcon(int x, int y, int icon, int shade) {
    Record(46, Lo16(x), Lo16(y), Lo8(static_cast<unsigned>(icon)), Lo8(static_cast<unsigned>(shade)));
    Disturb();
}
int __cdecl StubExp(unsigned member, unsigned level) {
    Record(47, member & 0xFF, level & 0xFF);
    Disturb();
    const std::uint32_t h = Hash();
    // equal answers for the two calls often enough: one span of 0 in four
    return static_cast<int>(h % 4 == 0 ? 5000u : h % 4 == 1 ? 0xFFFFFFFFu : (h >> 4) % 20000);
}
unsigned __cdecl StubPrice(unsigned category, unsigned id) {
    Record(48, category & 0xFF, id & 0xFF);
    Disturb();
    return Hash();
}
unsigned char __cdecl StubCanUse(unsigned mode, unsigned member, unsigned category, unsigned item) {
    Record(49, mode & 0xFF, member & 0xFF, category & 0xFF, item & 0xFF);
    Disturb();
    const std::uint32_t h = Hash();
    return static_cast<unsigned char>(h % 3 == 0 ? 0 : h % 3 == 1 ? 1 : h >> 8);
}
void __cdecl StubItemRow(int x, int y, int colour, unsigned category, unsigned id, unsigned count, int dim) {
    Record(50, Lo16(x), Lo16(y), Lo8(static_cast<unsigned>(colour)), category & 0xFF, id & 0xFF, count & 0xFF,
           Lo8(static_cast<unsigned>(dim)));
    Disturb();
}
void __cdecl StubScrollBar(const unsigned char* items, unsigned top, int x, int y, unsigned rows, unsigned total,
                           unsigned height) {
    Record(51, Id(items), top & 0xFF, Lo16(x), Lo16(y), rows & 0xFF, total & 0xFF, height & 0xFF);
    Disturb();
}
// The scroll step writes its two outputs every time, and sometimes the top and
// the state it was handed.
unsigned char __cdecl StubListScroll(unsigned char* top, unsigned char* offset, unsigned char* moving,
                                     unsigned char* state) {
    Record(52, Id(top), Id(state), *top, *state);
    const std::uint32_t h = Hash();
    *offset = static_cast<unsigned char>(h % 3 == 0 ? 0 : (h >> 8) % 3 == 0 ? h >> 16 : 0x100 - (h >> 8) % 12);
    // moving is 0 or 1 from the real step; the list's row counter is a byte
    // compared with moving + 9, so 247 or more would never end (in the
    // original too) - a 2 now and then is as far as the stand-in goes
    *moving = static_cast<unsigned char>((h >> 20) % 7 == 0 ? 2 : (h >> 20) & 1);
    if (h % 5 == 0) *top = static_cast<unsigned char>(h >> 12);
    if (h % 7 == 0) *state = static_cast<unsigned char>(h >> 4);
    Disturb();
    return static_cast<unsigned char>(Hash() % 3 == 0 ? Hash() : *top);
}
void __cdecl StubBorder(int x, int y, int w, int h) {
    Record(53, Lo16(x), Lo16(y), Lo8(static_cast<unsigned>(w)), Lo8(static_cast<unsigned>(h)));
    Disturb();
}

const Callees kStubs = {
    StubDrawAt, StubVerbLabel, StubImmediate, StubSystemPtr, StubYesNoLine, StubReset, StubSound, StubDrawMode,
    StubCommit, StubSetSprt, StubSetSemi, StubGetTPage, StubGetClut, StubPolyFT4, StubLineF2, StubLineF3,
    StubLineF4, StubTile, StubAreaByte, StubSprintf, StubStatIcon, StubHand, StubCompare, StubItemName,
    StubItemKind, StubEquipMask, StubItemFlags, StubHasKeyItem, StubCountOwned, StubCountCategory, StubPriceScale,
    StubSellPrice, StubFont12, StubFont8, StubPanel, StubBox, StubIcon8, StubCharCount, StubPieces, StubPiece,
    StubPieceRect, StubLine, StubOutline, StubItemIcon, StubIcon, StubExp, StubPrice, StubCanUse, StubItemRow,
    StubScrollBar, StubListScroll, StubBorder, StubSetSprt8,
};

const void* StubFor(std::uint32_t target, std::uint32_t caller, std::uint32_t offset) {
    const auto f = [](auto p) { return reinterpret_cast<const void*>(p); };
    // the two sites a divergence's module may have re-aimed (checked apart)
    if (caller == 0x5747D0 && offset == 0x2) return f(&StubYesNoLine);
    if (caller == 0x574890 && offset == 0x10B) return f(&StubVerbLabel);
    switch (target) {
    case 0x516B30: return f(&StubDrawAt);
    case 0x5961C0: return f(&StubImmediate);
    case 0x497740: return f(&StubSystemPtr);
    case 0x497770: return f(&StubReset);
    case 0x587740: return f(&StubSound);
    case 0x5A77C0: return f(&StubDrawMode);
    case 0x461E50: return f(&StubCommit);
    case 0x5A7710: return f(&StubSetSprt);
    case 0x5A7720: return f(&StubSetSprt8);
    case 0x5A7780: return f(&StubSetSemi);
    case 0x5A79A0: return f(&StubGetTPage);
    case 0x5A79E0: return f(&StubGetClut);
    case 0x5A75D0: return f(&StubPolyFT4);
    case 0x5A7650: return f(&StubLineF2);
    case 0x5A7670: return f(&StubLineF3);
    case 0x5A7690: return f(&StubLineF4);
    case 0x5A7740: return f(&StubTile);
    case 0x536700: return f(&StubAreaByte);
    case 0x5B9380: return f(&StubSprintf);
    case kStatIcon: return f(&StubStatIcon);
    case kDrawHand: return f(&StubHand);
    case kEquipCompare: return f(&StubCompare);
    case kItemName: return f(&StubItemName);
    case kItemKind: return f(&StubItemKind);
    case kEquipMask: return f(&StubEquipMask);
    case kItemFlagsOf: return f(&StubItemFlags);
    case kHasKeyItem: return f(&StubHasKeyItem);
    case kCountOwned: return f(&StubCountOwned);
    case kCountCategory: return f(&StubCountCategory);
    case kPriceScale: return f(&StubPriceScale);
    case kSellPrice: return f(&StubSellPrice);
    case 0x516F60: return f(&StubFont12);
    case 0x517090: return f(&StubFont8);
    case 0x575830: return f(&StubPanel);
    case 0x57CF60: return f(&StubBox);
    case 0x57D360: return f(&StubIcon8);
    case 0x57D800: return f(&StubCharCount);
    case 0x57D910: return f(&StubPieces);
    case 0x57D860: return f(&StubPiece);
    case 0x57D830: return f(&StubPieceRect);
    case 0x57D760: return f(&StubLine);
    case 0x57D420: return f(&StubOutline);
    case 0x573F30: return f(&StubItemIcon);
    case 0x573E50: return f(&StubIcon);
    case 0x574A60: return f(&StubExp);
    case 0x5749F0: return f(&StubPrice);
    case 0x57D9A0: return f(&StubCanUse);
    case 0x57DBF0: return f(&StubItemRow);
    case 0x57DD10: return f(&StubScrollBar);
    case 0x57DF00: return f(&StubListScroll);
    case 0x5762D0: return f(&StubBorder);
    default: bof3::Fatal("menu_windows: no stand-in for a call to 0x%X", static_cast<unsigned>(target)); return nullptr;
    }
}

// --- the thirty-seven copies (capstone, 2026-09-23: every jump internal but
// the two jump tables; the calls below are every call that leaves) -----------

struct Call { std::uint32_t offset, target; };
struct Table { std::uint32_t jmp_disp, table, entries; };
// ret: the answer's width the original defines - 0 none, 1 al, 4 eax.
struct Clone {
    const char* name;
    std::uint32_t base, size;
    const Call* calls;
    int n_calls;
    Table table;
    unsigned ret;
    const void* ours;
};

constexpr Call k497710[] = {{0x6, 0x497740}, {0x1F, 0x497770}};
constexpr Call k498D20[] = {{0x1B, 0x5A79A0}, {0x33, 0x5A77C0}, {0x3C, 0x461E50}, {0x48, 0x5A7710}, {0x57, 0x5A79E0}, {0xA3, 0x5A7780}, {0xAC, 0x461E50}};
constexpr Call k516F60[] = {{0x1B, 0x5A77C0}, {0x24, 0x461E50}, {0xF0, 0x5A7710}, {0xF8, 0x5A7780}, {0x101, 0x461E50}};
constexpr Call k517090[] = {{0x12, 0x5A77C0}, {0x1B, 0x461E50}, {0xD0, 0x5A7720}, {0xD8, 0x5A7780}, {0xE1, 0x461E50}};
constexpr Call k596020[] = {{0x53, 0x5961C0}};
constexpr Call k573A80[] = {{0x15, 0x575830}, {0x44, 0x516B30}, {0x5C, 0x5903F0}, {0x11C, 0x57D360}, {0x12C, 0x57D800}, {0x148, 0x516B30}};
constexpr Call k573CE0[] = {{0x62, 0x5A7670}, {0xF1, 0x461E50}, {0xFD, 0x5A7690}, {0x156, 0x461E50}};
constexpr Call k573E50[] = {{0x21, 0x5A77C0}, {0x2A, 0x461E50}, {0x36, 0x5A7710}, {0xD3, 0x461E50}};
constexpr Call k573F30[] = {{0x29, 0x573E50}};
constexpr Call k574530[] = {{0xE, 0x5A7650}, {0x16, 0x5A7780}, {0x56, 0x574A60}, {0x65, 0x574A60}, {0xC4, 0x461E50}};
constexpr Call k574610[] = {{0x1F, 0x57CF60}, {0x33, 0x5B9380}, {0x47, 0x516F60}, {0x5D, 0x516B30}, {0x6E, 0x57D910}, {0x89, 0x57D910}, {0xA1, 0x57D910}};
constexpr Call k5747D0[] = {{0x2, 0x497740}, {0x13, 0x516B30}, {0x32, 0x5905D0}, {0x4D, 0x587740}, {0x76, 0x587740}, {0x86, 0x587740}, {0x9B, 0x587740}};
constexpr Call k574890[] = {{0x55, 0x57CF60}, {0xA9, 0x5918E0}, {0xBB, 0x5919B0}, {0xE9, 0x57D800}, {0x10B, 0x516B30}, {0x12E, 0x57D910}};
constexpr Call k574AB0[] = {{0x3D, 0x5A77C0}, {0x46, 0x461E50}, {0x60, 0x5A75D0}, {0x68, 0x5A7780}, {0x141, 0x5A79E0}, {0x154, 0x461E50}, {0x181, 0x5A75D0}, {0x189, 0x5A7780}, {0x20E, 0x5A79E0}, {0x221, 0x461E50}, {0x22D, 0x5A75D0}, {0x235, 0x5A7780}, {0x2B7, 0x5A79E0}, {0x2CA, 0x461E50}, {0x2D6, 0x5A75D0}, {0x2DE, 0x5A7780}, {0x376, 0x5A79E0}, {0x38C, 0x461E50}, {0x3C8, 0x5A77C0}, {0x3D1, 0x461E50}, {0x3F4, 0x57D420}};
constexpr Call k575430[] = {{0x6E, 0x5917A0}, {0xA2, 0x57CF60}, {0xCD, 0x590960}, {0x101, 0x5B9380}, {0x127, 0x517090}, {0x15D, 0x5B9380}, {0x171, 0x517090}, {0x187, 0x5B9380}, {0x198, 0x517090}, {0x1AE, 0x5B9380}, {0x1C2, 0x517090}, {0x1DB, 0x573F30}, {0x1F7, 0x516B30}, {0x20C, 0x516B30}, {0x221, 0x516B30}, {0x232, 0x57D910}};
constexpr Call k575690[] = {{0x17, 0x5A79A0}, {0x2F, 0x5A77C0}, {0x38, 0x461E50}, {0x44, 0x5A79E0}, {0x55, 0x5A79E0}, {0x66, 0x5A79E0}, {0x77, 0x5A79E0}, {0xE1, 0x5A7710}, {0xE9, 0x5A7780}, {0x14A, 0x461E50}};
constexpr Call k575830[] = {{0x40, 0x57CF60}, {0x58, 0x5A79A0}, {0x71, 0x5A77C0}, {0x7A, 0x461E50}, {0x85, 0x57D860}, {0xA6, 0x57D860}, {0xC6, 0x57D860}, {0xD9, 0x57D860}, {0x10A, 0x57D860}, {0x142, 0x57D860}, {0x151, 0x57D860}, {0x16F, 0x57D860}, {0x17E, 0x57D860}};
constexpr Call k5759C0[] = {{0x30, 0x57CF60}, {0x4B, 0x57DF00}, {0xE4, 0x57D9A0}, {0x1B5, 0x57DBF0}, {0x1E1, 0x57DBF0}, {0x24D, 0x57CF60}, {0x276, 0x57CF60}, {0x2AD, 0x57D800}, {0x2CB, 0x516B30}, {0x2E7, 0x591A80}, {0x2FF, 0x5B9380}, {0x326, 0x517090}, {0x34C, 0x57D910}, {0x372, 0x57D910}, {0x3AD, 0x57D860}, {0x3D8, 0x57D860}, {0x3FE, 0x57D860}, {0x427, 0x57D860}, {0x44D, 0x57D860}, {0x465, 0x57D860}, {0x48D, 0x57D860}, {0x4B2, 0x57D860}, {0x4DB, 0x57D860}, {0x501, 0x57D860}, {0x522, 0x57D860}, {0x53A, 0x57D860}, {0x57D, 0x57DD10}};
constexpr Call k5762D0[] = {{0x14, 0x5A79A0}, {0x2C, 0x5A77C0}, {0x35, 0x461E50}, {0x6C, 0x57D860}, {0x7B, 0x57D860}, {0xB4, 0x57D860}, {0xC3, 0x57D860}, {0xDB, 0x57D860}, {0xF0, 0x57D860}, {0x105, 0x57D860}, {0x110, 0x57D860}};
constexpr Call k57CF60[] = {{0x54, 0x5A77C0}, {0x5D, 0x461E50}, {0x69, 0x5A75D0}, {0x87, 0x5A7780}, {0x17A, 0x5A79E0}, {0x18D, 0x461E50}, {0x21A, 0x461E50}, {0x28F, 0x461E50}, {0x2E9, 0x461E50}, {0x3A4, 0x461E50}, {0x3DF, 0x5A77C0}, {0x3E8, 0x461E50}};
constexpr Call k57D360[] = {{0xF, 0x5A77C0}, {0x18, 0x461E50}, {0x24, 0x5A7720}, {0x9A, 0x461E50}, {0xAE, 0x461E50}};
constexpr Call k57D420[] = {{0x87, 0x57D760}, {0xAE, 0x57D760}, {0xD1, 0x57D760}, {0xEF, 0x57D760}};
constexpr Call k57D760[] = {{0x1B, 0x5A77C0}, {0x24, 0x461E50}, {0x30, 0x5A7650}, {0x8D, 0x5A7780}, {0x96, 0x461E50}};
constexpr Call k57D860[] = {{0x11, 0x57D830}, {0x1F, 0x5A7710}, {0x27, 0x5A7780}, {0x87, 0x5A79E0}, {0xA3, 0x461E50}};
constexpr Call k57D910[] = {{0x21, 0x5A79A0}, {0x3A, 0x5A77C0}, {0x43, 0x461E50}, {0x74, 0x57D860}};
constexpr Call k57D9A0[] = {{0x5D, 0x536700}, {0x72, 0x591810}, {0x87, 0x591810}, {0x9C, 0x5917A0}};
constexpr Call k57DBF0[] = {{0x1A, 0x591720}, {0x34, 0x57D360}, {0x3F, 0x591680}, {0x49, 0x57D800}, {0x5C, 0x516B30}, {0x7C, 0x5B9380}, {0x8C, 0x517090}};
constexpr Call k57DD10[] = {{0x3B, 0x5A7740}, {0xB6, 0x461E50}, {0xD5, 0x5A75D0}, {0x1A6, 0x5A79A0}, {0x1B9, 0x5A79E0}, {0x1D2, 0x461E50}};
constexpr Call k59B580[] = {{0x30, 0x57CF60}, {0x8D, 0x5749F0}, {0x9B, 0x5830D0}, {0xA6, 0x591720}, {0xB1, 0x591680}, {0xF5, 0x5B9380}, {0x12F, 0x57D360}, {0x139, 0x57D800}, {0x152, 0x516B30}, {0x168, 0x517090}, {0x19A, 0x57D360}, {0x1A4, 0x57D800}, {0x1C0, 0x516B30}, {0x1D5, 0x517090}, {0x21B, 0x5762D0}};
constexpr Call k59B820[] = {{0x31, 0x5749F0}, {0x3F, 0x5830D0}, {0x4A, 0x591680}, {0x73, 0x57CF60}, {0x7F, 0x591720}, {0xA5, 0x57D360}, {0xAF, 0x57D800}, {0xCC, 0x516B30}, {0xE6, 0x5B9380}, {0x104, 0x517090}, {0x119, 0x5B9380}, {0x137, 0x517090}, {0x141, 0x497740}, {0x160, 0x516B30}, {0x169, 0x5919B0}, {0x17E, 0x5B9380}, {0x19D, 0x517090}, {0x1A7, 0x497740}, {0x1C6, 0x516B30}, {0x1CF, 0x5919B0}, {0x1E4, 0x5B9380}, {0x203, 0x517090}, {0x21B, 0x5A79A0}, {0x233, 0x5A77C0}, {0x23C, 0x461E50}, {0x263, 0x57D860}, {0x27A, 0x57D860}, {0x2A8, 0x57D860}, {0x2C0, 0x57D860}, {0x2DD, 0x57D860}, {0x2F4, 0x57D860}, {0x30B, 0x57D860}, {0x327, 0x57D860}};
constexpr Call k59BBC0[] = {{0x25, 0x583100}, {0x2E, 0x591680}, {0x58, 0x57CF60}, {0x64, 0x591720}, {0x8A, 0x57D360}, {0x98, 0x57D800}, {0xB5, 0x516B30}, {0xC5, 0x5B9380}, {0xE3, 0x517090}, {0xFB, 0x5B9380}, {0x119, 0x517090}, {0x131, 0x5B9380}, {0x14F, 0x517090}, {0x167, 0x5A79A0}, {0x180, 0x5A77C0}, {0x189, 0x461E50}, {0x1B0, 0x57D860}, {0x1C7, 0x57D860}, {0x1F5, 0x57D860}, {0x20C, 0x57D860}, {0x229, 0x57D860}, {0x241, 0x57D860}, {0x258, 0x57D860}, {0x274, 0x57D860}};

constexpr Table kNoTable = {0, 0, 0};
#define MW_C(name, base, size, calls, ret) {#name, base, size, calls, static_cast<int>(sizeof calls / sizeof calls[0]), kNoTable, ret, reinterpret_cast<const void*>(&::name)}
#define MW_T(name, base, size, calls, table, ret) {#name, base, size, calls, static_cast<int>(sizeof calls / sizeof calls[0]), table, ret, reinterpret_cast<const void*>(&::name)}
#define MW_P(name, base, size, ret) {#name, base, size, nullptr, 0, kNoTable, ret, reinterpret_cast<const void*>(&::name)}
const Clone kClones[] = {
    MW_C(Msg_OpenSystem, 0x497710, 0x26, k497710, 0),
    MW_C(MsgBox_DrawArrow, 0x498D20, 0xB6, k498D20, 0),
    MW_C(Text_DrawFont12, 0x516F60, 0x123, k516F60, 0),
    MW_C(Text_DrawFont8, 0x517090, 0x103, k517090, 0),
    MW_C(Window_Kind2List, 0x596020, 0x6A, k596020, 0),
    MW_C(Menu_DrawEquipPanel, 0x573A80, 0x16B, k573A80, 0),
    MW_C(Menu_DrawCursorBox, 0x573CE0, 0x163, k573CE0, 0),
    MW_C(Menu_DrawIcon, 0x573E50, 0xDE, k573E50, 0),
    MW_C(Menu_DrawItemIcon, 0x573F30, 0x32, k573F30, 0),
    MW_C(Menu_DrawExpBar, 0x574530, 0xD4, k574530, 0),
    MW_C(Menu_DrawMoneyBox, 0x574610, 0xAD, k574610, 0),
    MW_C(Menu_YesNo, 0x5747D0, 0xB2, k5747D0, 1),
    MW_C(Menu_DrawButtonRow, 0x574890, 0x156, k574890, 0),
    MW_P(Item_Price, 0x5749F0, 0x6E, 4),
    MW_P(Char_ExpForLevel, 0x574A60, 0x45, 4),
    MW_C(Menu_DrawTitleBox, 0x574AB0, 0x404, k574AB0, 0),
    MW_T(Shop_DrawMemberStats, 0x575430, 0x25F, k575430, (Table{0x46, 0x244, 4}), 0),
    MW_C(Menu_DrawBackdrop, 0x575690, 0x195, k575690, 0),
    MW_C(Menu_DrawPanel, 0x575830, 0x18C, k575830, 0),
    MW_C(Menu_DrawItemList, 0x5759C0, 0x58C, k5759C0, 0),
    MW_C(Menu_DrawBorder, 0x5762D0, 0x11D, k5762D0, 0),
    MW_C(Menu_DrawBox, 0x57CF60, 0x3F8, k57CF60, 0),
    MW_C(Menu_DrawIcon8, 0x57D360, 0xB8, k57D360, 0),
    MW_C(Menu_DrawOutline, 0x57D420, 0xFF, k57D420, 0),
    MW_C(Menu_DrawLine, 0x57D760, 0xA0, k57D760, 0),
    MW_P(Text_CharCount, 0x57D800, 0x27, 1),
    MW_P(Menu_PieceRect, 0x57D830, 0x2B, 4),
    MW_C(Menu_DrawPiece, 0x57D860, 0xAF, k57D860, 0),
    MW_C(Menu_DrawPieces, 0x57D910, 0x8B, k57D910, 0),
    MW_T(Item_CanUse, 0x57D9A0, 0xCC, k57D9A0, (Table{0x21, 0xBC, 4}), 1),
    MW_C(Menu_DrawItemRow, 0x57DBF0, 0x99, k57DBF0, 0),
    MW_C(Menu_DrawScrollBar, 0x57DD10, 0x1E2, k57DD10, 0),
    MW_P(Menu_ListScroll, 0x57DF00, 0xED, 1),
    MW_C(Shop_DrawBuyList, 0x59B580, 0x22A, k59B580, 0),
    MW_C(Shop_DrawBuyDetail, 0x59B820, 0x335, k59B820, 0),
    MW_C(Shop_DrawSellDetail, 0x59BBC0, 0x282, k59BBC0, 0),
    MW_P(Gpu_SetSprt8, 0x5A7720, 0x10, 0),
};
#undef MW_C
#undef MW_T
#undef MW_P
constexpr unsigned kCount = sizeof kClones / sizeof kClones[0];

enum : unsigned {
    kOpenSystem, kArrow, kFont12, kFont8, kKind2List, kEquipPanel, kCursorBox, kIcon, kItemIcon, kExpBar, kMoneyBox,
    kYesNo, kButtonRow, kPrice, kExp, kTitleBox, kMemberStats, kBackdrop, kPanel, kItemList, kBorder, kBox, kIcon8,
    kOutline, kLine, kCharCount, kPieceRect, kPiece, kPieces, kCanUse, kItemRow, kScrollBar, kListScroll, kBuyList,
    kBuyDetail, kSellDetail, kSetSprt8,
};
static_assert(kSetSprt8 + 1 == kCount, "the index list and the clone list disagree");

// --- the state both passes start from ---------------------------------------

struct Region { std::uint32_t at, size; };
const Region kRegions[] = {
    {0x7DEE40, 0x40},                              // MsgBoxState
    {0x7E0670, 4},                                 // Gfx_PacketNext (into our pool)
    {0x7E1BE0, 0x10},                              // the pad words, Input_Pressed
    {0x802D70, 0x10},                              // the leader's x and z
    {0x8034E0, 4},                                 // Cond_ByteFA
    {0x903580, 0x14},                              // the menu, confirm and cancel buttons
    {0x903A50, 0x20},                              // the window colour 0x903A5A
    {at::kCharRecords, 10 * at::kCharStride},      // the records (and two past the eight)
    {0x904030, 0x40},                              // the gold, the party lists
    {0x904150, 0x490},                             // the inventory's ids and counts
    {at::kPrintBuf, 0x40},                         // the sprintf buffer
    {0x905B80, 0x30},                              // the current record pointer, 0x905BA2
    {0x929F00, 0x10},                              // the menu state, 0x929F06, 0x929F0B
    {0x937F90, 8},                                 // Frame_Counter
};
constexpr unsigned kRegionBytes =
    0x40 + 4 + 0x10 + 0x10 + 4 + 0x14 + 0x20 + 10 * at::kCharStride + 0x40 + 0x490 + 0x40 + 0x30 + 0x10 + 8;

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

// Random bytes put back inside what the functions can safely be handed: the
// pointers ours, the counts short, the strings ended.
void Text(unsigned char* s, unsigned size, bool controls) {
    const unsigned len = Next() % (size - 2);
    for (unsigned i = 0; i < len; ++i) {
        std::uint32_t c = Next();
        switch (c % 8) {
        case 0: c = controls ? (Half() ? 0x0A : 0x20) : 0x41 + (c >> 8) % 26; break;
        case 1: c = 0x80 | (c >> 8); break;
        case 2: c = 1 + (c >> 8) % 0x1F; break;
        default: c = 0x21 + (c >> 8) % 0x5E; break;
        }
        s[i] = static_cast<unsigned char>(c);
    }
    s[len] = 0;
    s[len + 1] = static_cast<unsigned char>(Half() ? 0 : 0x41);
    s[size - 1] = 0;
}
void Fix() {
    Gfx_PacketNext = g_buf.pool + Next() % 0x40;
    SetLong(At(at::kCurrent), static_cast<std::int32_t>(Address(Window(Next() % kWindows))));
    SetLong(At(at::kListText), static_cast<std::int32_t>(Address(g_buf.texts[Next() % kTexts])));
    At(at::kListSet)[0] = static_cast<unsigned char>(Next() % 4);
    At(at::kListCount)[0] = static_cast<unsigned char>(static_cast<signed char>(Next() % 10) - 2);
    for (unsigned k = 0; k < kWindows; ++k) {
        unsigned char* const w = Window(k);
        w[0xA] = static_cast<unsigned char>(Next() % 5);
        const std::uint32_t list = Address(g_buf.list);
        std::memcpy(w + 0x20, &list, sizeof list);
    }
    g_buf.list[0] = static_cast<unsigned char>(Next() % 14);
    for (unsigned k = 0; k < kTexts; ++k) Text(g_buf.texts[k], kTextBytes, true);
    for (unsigned k = 0; k < kNames; ++k) Text(g_buf.names[k], kNameBytes, false);
    // the gold near the prices the scale stand-in answers
    if (Often()) SetLong(At(at::kGold), static_cast<std::int32_t>(990 + Next() % 20));
}

struct Args { std::uint32_t a[8]; };

std::uint32_t Coord() {
    static const std::uint32_t kEdges[] = {0, 1, 0x7FFF, 0x8000, 0xFFFF, 0x10000, 0xFFFFFFF0u, 0x140, 0xF0};
    return Often() ? Garbage(0xFFFF, Next() % 0x180) : Half() ? kEdges[Next() % 9] : Next();
}

Args Seed(unsigned k) {
    Args args;
    for (auto& a : args.a) a = Next();
    switch (k) {
    case kOpenSystem: break;
    case kArrow:
        At(at::kFrameCounter)[0] = static_cast<unsigned char>(Half() ? 0x20 | Next() : Next() & ~0x20u);
        args.a[0] = Coord();
        args.a[1] = Coord();
        break;
    case kFont12:
    case kFont8:
        args.a[0] = Coord();
        args.a[1] = Coord();
        args.a[3] = Address(g_buf.texts[Next() % kTexts]);
        if (Next() % 8 == 0) g_buf.texts[0][0] = 0;
        break;
    case kKind2List: break;
    case kEquipPanel: args.a[2] = Garbage(0xFF, Next() % 8); break;
    case kCursorBox: {
        args.a[0] = Coord();
        args.a[1] = Coord();
        args.a[2] = Coord();
        args.a[3] = Coord();
        args.a[4] = Half() ? Garbage(0xFF, 0) : Next();
        At(at::kFrameCounter)[0] = static_cast<unsigned char>(Next());
        break;
    }
    case kIcon:
    case kItemIcon: {
        static const std::uint32_t kShades[] = {0, 1, 2, 0xFF, 0x100, 0x101};
        args.a[0] = Coord();
        args.a[1] = Coord();
        args.a[2] = Half() ? Garbage(0xFF, 4) : Garbage(0xFF, Next() % 0x18);
        args.a[3] = Often() ? Garbage(0xFF, kShades[Next() % 6]) : Next();
        static const unsigned char kFA[] = {7, 8, 9, 0x7F, 0x80, 0xFF, 0};
        At(at::kCondFA)[0] = kFA[Next() % 7];
        break;
    }
    case kExpBar: {
        static const std::uint32_t kLevels[] = {0, 1, 0x62, 0x63, 0x64, 0xFF, 0x1FF};
        args.a[3] = Half() ? kLevels[Next() % 7] : Next();
        args.a[4] = Half() ? Next() % 30000 : Next();
        break;
    }
    case kMoneyBox: args.a[0] = Coord(); args.a[1] = Coord(); break;
    case kYesNo: {
        static const unsigned char kSel[] = {0, 1, 0xFF, 0x80, 2};
        At(at::kYesNo)[0] = kSel[Next() % 5];
        const std::uint32_t buttons = Next();
        SetWord(At(at::kCancelButtons), Half() ? 0 : 1u << (buttons % 16));
        SetWord(At(at::kConfirmButtons), Half() ? 0 : 1u << ((buttons >> 4) % 16));
        static const std::uint32_t kPressed[] = {0, 0x2000, 0x8000, 0xA000, 0x4000, 0x1000, 0x10000, 0xFFFF0000u};
        SetLong(At(at::kInputPressed),
                static_cast<std::int32_t>(Often() ? kPressed[Next() % 8] | (Half() ? 0 : 1u << (Next() % 16)) : Next()));
        break;
    }
    case kButtonRow: {
        args.a[0] = Coord();
        args.a[1] = Coord();
        args.a[2] = Garbage(0xFF, Next() % 9);
        static const std::uint32_t kSel[] = {0, 1, 2, 3, 4, 0xFF};
        args.a[3] = Garbage(0xFF, kSel[Next() % 6]);
        break;
    }
    case kPrice: args.a[0] = Garbage(0xFF, Next() % 6); break;
    case kExp: {
        static const std::uint32_t kLevels[] = {0, 1, 0x62, 0x63, 0x64, 0xFF};
        args.a[1] = Garbage(0xFF, Half() ? kLevels[Next() % 6] : Next() % 0x64);
        break;
    }
    case kTitleBox: {
        static const std::uint32_t kW[] = {0, 1, 2, 3, 4, 5, 6, 0x40, 0x41, 0xFFFF, 0xFFFE};
        args.a[0] = Coord();
        args.a[1] = Coord();
        args.a[2] = Often() ? Garbage(0xFFFF, kW[Next() % 11]) : Next();
        args.a[3] = Often() ? Garbage(0xFFFF, kW[Next() % 11]) : Next();
        break;
    }
    case kMemberStats:
        args.a[0] = Coord();
        args.a[1] = Coord();
        args.a[2] = Garbage(0xFF, Next() % 10);
        args.a[3] = Garbage(0xFF, Next() % 13);
        break;
    case kBackdrop: args.a[0] = Garbage(0xFF, Next() % 4); break;
    case kPanel:
        args.a[0] = Coord();
        args.a[1] = Coord();
        args.a[2] = Garbage(0xFF, Half() ? Next() % 16 : Next() % 251);
        args.a[3] = Garbage(0xFF, Next() % 32);
        break;
    case kItemList:
    case kBuyList:
    case kBuyDetail:
    case kSellDetail: {
        unsigned char* const w = Window(0);
        args.a[0] = Address(w);
        static const unsigned char kState[] = {0x10, 0x11, 0x17, 0x18, 0x1B, 0xF0, 0xF1, 0xF4, 0xF5, 0xFB, 0, 0x20};
        w[0x12] = kState[Next() % 12];
        w[0x13] = static_cast<unsigned char>(Half() ? 0 : Next());
        w[0x9] = static_cast<unsigned char>(Half() ? 0 : Next());
        if (k == kBuyList || k == kBuyDetail) {
            w[0xA] = static_cast<unsigned char>(Half() ? 0 : Next() % 5);
            g_buf.list[0] = static_cast<unsigned char>(Next() % 14);
            w[0xB] = static_cast<unsigned char>(Next() % 14);
            w[0xC] = static_cast<unsigned char>(Next() % 14);
        } else {
            w[0xB] = static_cast<unsigned char>(Half() ? Next() % 16 : Next());
            w[0xC] = static_cast<unsigned char>(w[0xB] + Next() % 12);
            w[0xD] = static_cast<unsigned char>(w[0xB] + Next() % 12);
        }
        At(at::kMenuMember)[0] = static_cast<unsigned char>(Next() % 4);
        break;
    }
    case kBorder:
        args.a[0] = Coord();
        args.a[1] = Coord();
        args.a[2] = Garbage(0xFF, Next() % 24);
        args.a[3] = Garbage(0xFF, Next() % 24);
        break;
    case kBox: {
        static const std::uint32_t kW[] = {0, 8, 0xFF, 0x100, 0x101, 0x1FF, 0x200, 0xFFFF, 0x10000, 0x10100};
        static const std::uint32_t kKind[] = {0, 1, 2, 3, 0x11, 0x12, 0xF1};
        args.a[0] = Coord();
        args.a[1] = Coord();
        args.a[2] = Often() ? Garbage(0xFFFFF, kW[Next() % 10]) : Next();
        args.a[3] = Coord();
        args.a[4] = Half() ? Garbage(0xFF, kKind[Next() % 7] | (Next() & 0xF0)) : Next();
        break;
    }
    case kIcon8: {
        static const std::uint32_t kIcons[] = {1, 3, 15, 0, 2, 14, 16, 0x101, 0xF9};
        args.a[0] = Coord();
        args.a[1] = Coord();
        args.a[2] = Often() ? kIcons[Next() % 9] : Next();
        args.a[3] = Half() ? Garbage(0xFF, 0) : Next();
        break;
    }
    case kOutline:
        args.a[0] = Coord(); args.a[1] = Coord(); args.a[2] = Coord(); args.a[3] = Coord();
        break;
    case kLine: args.a[0] = Coord(); args.a[1] = Coord(); args.a[2] = Coord(); args.a[3] = Coord(); break;
    case kCharCount: {
        unsigned char* const s = g_buf.texts[Next() % kTexts];
        if (Half()) s[14 + Next() % 3] = static_cast<unsigned char>(0x80 | Next());
        args.a[0] = Address(s);
        break;
    }
    case kPieceRect: break;
    case kPiece: args.a[0] = Coord(); args.a[1] = Coord(); break;
    case kPieces: {
        unsigned char* const list = g_buf.misc + 0x100;
        const unsigned n = Next() % 7;
        for (unsigned i = 0; i < n; ++i) {
            list[3 * i] = static_cast<unsigned char>(Next());
            list[3 * i + 1] = static_cast<unsigned char>(Next());
            list[3 * i + 2] = static_cast<unsigned char>(Next() % 0xFF);
        }
        list[3 * n + 2] = 0xFF;
        args.a[0] = Coord();
        args.a[1] = Coord();
        args.a[2] = Address(list);
        break;
    }
    case kCanUse: {
        static const std::uint32_t kItems[] = {0, 0x57, 1, 2, 0x46, 0x100, 0x157};
        args.a[0] = Garbage(0xFF, Next() % 6);
        args.a[1] = Half() ? Garbage(0xFF, Next() % 10) : Next();
        args.a[2] = Half() ? Garbage(0xFF, 0) : Next();
        args.a[3] = Half() ? kItems[Next() % 7] : Next();
        break;
    }
    case kItemRow: {
        static const std::uint32_t kCounts[] = {0, 1, 2, 0x100, 0x101, 0xFF};
        args.a[0] = Coord();
        args.a[1] = Coord();
        args.a[5] = Often() ? kCounts[Next() % 6] : Next();
        break;
    }
    case kScrollBar: {
        args.a[0] = Address(g_buf.misc);
        args.a[1] = Half() ? Next() % 0x80 : Next();
        args.a[2] = Coord();
        args.a[3] = Coord();
        // a total byte of 0 divides by zero in both: never seeded
        std::uint32_t total = Next();
        if ((total & 0xFF) == 0) total |= 1 + Next() % 0x80;
        args.a[5] = Half() ? Garbage(0xFF, Half() ? 0x20 : 0x80) : total;
        break;
    }
    case kListScroll: {
        unsigned char* const m = g_buf.misc + 0x1C0;
        static const unsigned char kState[] = {0x10, 0x11, 0x13, 0x14, 0x17, 0x18, 0x1B, 0x1F, 0xF0, 0xF1, 0xF3,
                                               0xF4, 0xF5, 0xFB, 0xFF, 0, 0x20, 0xE4};
        m[3] = Often() ? kState[Next() % 18] : static_cast<unsigned char>(Next());
        args.a[0] = Address(m);
        args.a[1] = Address(m + 1);
        args.a[2] = Address(m + 2);
        args.a[3] = Address(m + 3);
        break;
    }
    case kSetSprt8: args.a[0] = Address(g_buf.pool + Next() % 0x100); break;
    default: break;
    }
    return args;
}

// What the rounds reached, from the original's side, for the log line.
struct Coverage {
    unsigned logged[64];
    unsigned yes_no[3];           // answers 0 / 1 / other
    unsigned can_use[2];
    unsigned wide_boxes, grey_rows;
} g_cover;

void Cover(unsigned k, const State& in, const State& out, std::uint32_t result) {
    for (unsigned i = 0; i < out.log_n && i < kLog; ++i)
        if (out.log[i].what < 64) ++g_cover.logged[out.log[i].what];
    if (k == kYesNo) ++g_cover.yes_no[result > 1 ? 2 : result];
    if (k == kCanUse) ++g_cover.can_use[result ? 1 : 0];
    if (k == kBox) {
        unsigned commits = 0;
        for (unsigned i = 0; i < out.log_n && i < kLog; ++i) commits += out.log[i].what == 9 ? 1 : 0;
        if (commits == 6) ++g_cover.wide_boxes;
    }
    (void)in;
    if (k == kItemList)
        for (unsigned i = 0; i < out.log_n && i < kLog; ++i)
            if (out.log[i].what == 50 && out.log[i].c == 7) ++g_cover.grey_rows;
}

using Fn8 = std::uint32_t (__cdecl*)(std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t,
                                      std::uint32_t, std::uint32_t, std::uint32_t);

}  // namespace

void SelfTest() {
    constexpr unsigned kPerFunction = 2000;
    unsigned region_bytes = 0;
    for (const Region& r : kRegions) region_bytes += r.size;
    if (region_bytes != kRegionBytes)
        bof3::Fatal("menu_windows: the regions are %u bytes, the state holds %u", region_bytes, kRegionBytes);

    void* clones[kCount];
    for (unsigned k = 0; k < kCount; ++k) {
        const Clone& c = kClones[k];
        bof3::CloneCall calls[40];
        if (c.n_calls > 40) bof3::Fatal("menu_windows: %s has %d calls", c.name, c.n_calls);
        for (int i = 0; i < c.n_calls; ++i) {
            // the two sites a divergence may have re-aimed are not checked
            // against their original target (their stand-ins are the same
            // either way: the divergence is what the callee does)
            const bool div_site = (c.base == 0x5747D0 && c.calls[i].offset == 0x2) ||
                                  (c.base == 0x574890 && c.calls[i].offset == 0x10B);
            calls[i] = {c.calls[i].offset, StubFor(c.calls[i].target, c.base, c.calls[i].offset),
                        div_site ? 0u : c.calls[i].target};
        }
        clones[k] = bof3::CloneOriginal(c.name, c.base, c.size, calls, c.n_calls);
        if (c.table.entries) move_script::Relocate(clones[k], c.base, c.size, {c.table.jmp_disp, c.table.table, c.table.entries});
    }

    static State saved, input, their_out, our_out;
    Capture(saved);
    const Callees kept = g;
    g = kStubs;
    const unsigned short saved_cw = GetControlWord();
    char trace_env[16] = {};
    unsigned trace_round = ~0u;
    if (GetEnvironmentVariableA("BOF3X_MW_TRACE", trace_env, sizeof trace_env) != 0) {
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
        Fix();
        g_seed = Next();
        const Args args = Seed(k);
        Capture(input);

        std::uint32_t result[2];
        for (int pass = 0; pass < 2; ++pass) {
            g_tracing = round == trace_round;
            if (g_tracing) bof3::Log("menu_windows trace: round %u %s pass %d", round, kClones[k].name, pass);
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
        Cover(k, input, their_out, result[0]);
        if (std::memcmp(&their_out, &our_out, sizeof their_out) != 0) {
            ++bad_per[k];
            if (++bad <= 12) {
                unsigned first = 0;
                while (first < sizeof their_out &&
                       reinterpret_cast<const unsigned char*>(&their_out)[first] ==
                           reinterpret_cast<const unsigned char*>(&our_out)[first])
                    ++first;
                bof3::Log("shadow      menu_windows self-test MISMATCH: round %u, %s, log %u / %u, result 0x%X / 0x%X, "
                          "first differing state byte %u",
                          round, kClones[k].name, their_out.log_n, our_out.log_n, their_out.result, our_out.result,
                          first);
                if (first < kRegionBytes) {
                    unsigned at = 0;
                    for (const Region& r : kRegions) {
                        if (first < at + r.size) {
                            bof3::Log("shadow      menu_windows:   memory 0x%X: 0x%02X / 0x%02X", r.at + (first - at),
                                      their_out.memory[first], our_out.memory[first]);
                            break;
                        }
                        at += r.size;
                    }
                } else if (first < offsetof(State, result)) {
                    const unsigned o = first - static_cast<unsigned>(offsetof(State, buf));
                    bof3::Log("shadow      menu_windows:   buffer +0x%X (pool %u): 0x%02X / 0x%02X", o, kPool,
                              reinterpret_cast<const unsigned char*>(&their_out.buf)[o],
                              reinterpret_cast<const unsigned char*>(&our_out.buf)[o]);
                } else if (first >= offsetof(State, log) && first < offsetof(State, log_n)) {
                    const unsigned e = (first - static_cast<unsigned>(offsetof(State, log))) / sizeof(Entry);
                    const Entry& a = their_out.log[e];
                    const Entry& b = our_out.log[e];
                    bof3::Log("shadow      menu_windows:   log %u: %u (%X %X %X %X %X %X %X) / %u (%X %X %X %X %X %X %X)", e,
                              a.what, a.a, a.b, a.c, a.d, a.e, a.f, a.h, b.what, b.a, b.b, b.c, b.d, b.e, b.f, b.h);
                }
            }
        }
    }
    g = kept;
    Apply(saved);

    bof3::Log("shadow      menu_windows self-test: %u rounds over %u functions (%u each), %u calls to the stand-ins, "
              "%u MISMATCHES; MsgBoxState, the packet cursor and our pool, the pad, the leader, the buttons, the "
              "colour, ten records, the gold and party, the inventory, the print buffer, the current record, the menu "
              "state, Frame_Counter, our window records, lists and strings, the answer and the stand-ins' log compared",
              kPerFunction * kCount, kCount, kPerFunction, calls, bad);
    if (bad)
        for (unsigned k = 0; k < kCount; ++k)
            if (bad_per[k]) bof3::Log("shadow      menu_windows: %s mismatched in %u rounds", kClones[k].name, bad_per[k]);
    const Coverage& c = g_cover;
    bof3::Log("shadow      menu_windows coverage: Yes/No answered 0 %u, 1 %u; Item_CanUse no %u, yes %u; wide boxes %u; "
              "grey item rows %u; calls: text %u, verb labels %u, lists %u, sounds %u, commits %u, sprintf %u, "
              "0x590960 %u, can-use %u, scroll steps %u, item rows %u, area bytes %u, key-item tests %u, owned counts %u",
              c.yes_no[0], c.yes_no[1], c.can_use[0], c.can_use[1], c.wide_boxes, c.grey_rows, c.logged[1], c.logged[2],
              c.logged[3], c.logged[7], c.logged[9], c.logged[21], c.logged[24], c.logged[49], c.logged[52],
              c.logged[50], c.logged[20], c.logged[29], c.logged[30]);
    if (bad) bof3::Fatal("the menu and shop windows differ from the original in %u self-test rounds", bad);
}

}  // namespace menu_windows

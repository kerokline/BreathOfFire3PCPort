// BOF3X_SHADOW=battle_windows: a differential fuzz of the battle windows,
// once at start-up. docs/battle_windows.md section 3.
//
// Twenty-two byte-copies, every call out re-aimed at a recording stand-in
// (for ours through battle_windows::g alike): the relative calls and tail
// jumps through CloneCall, the state table's operand in 0x4411E0's copy and
// the three handler immediates in 0x597A30's at tables of stand-ins, and
// 0x4412B0's switch table relocated in its copy. One round: one function,
// random bytes in every region any of them reads or writes, then that
// function's branch boundaries seeded; the copy, then from the same state
// ours, both under the game's x87 control word 0x027F; the regions, our
// pool, objects, window records and strings, the answer (at the width the
// original defines) and the stand-ins' log compared.
//
// The stand-ins do what their callers read back: the commit moves the packet
// cursor, the primitive setters write their code bytes, sprintf the print
// buffer (nothing here may reach the CRT: the self-test runs before it is
// initialised); and most disturb a cell some caller reads again after the
// call (the window colour, the party count, the cross's selection and
// growths, the party objects' flags and values, the bar bytes, the enemies'
// gauge flag, the current window record and its fields, the message ring's
// index, the acting member's flag).
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>

#include "bof3/symbols.gen.h"
#include "game/battle_windows_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"
#include <windows.h>

namespace battle_windows {
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

std::uint32_t g_rng = 0x3C6EF372u;
std::uint32_t Next() { g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5; return g_rng; }
bool Half() { return (Next() & 1) != 0; }
bool Often() { return Next() % 3 != 0; }
std::uint32_t Garbage(std::uint32_t low_mask, std::uint32_t value) { return (Next() & ~low_mask) | (value & low_mask); }

constexpr unsigned kPool = 0x800;
constexpr unsigned kObjects = 4, kObjectBytes = 0x180;
constexpr unsigned kWindows = 3, kWindowBytes = 0x40;
constexpr unsigned kTexts = 3, kTextBytes = 0x40;

struct Buffers {
    unsigned char pool[kPool];
    unsigned char objects[kObjects][kObjectBytes];
    unsigned char windows[kWindows][kWindowBytes];
    unsigned char texts[kTexts][kTextBytes];
};
Buffers g_buf;

constexpr unsigned kLog = 96;
struct Entry { std::uint32_t what, a, b, c, d, e, f, h, i; };
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
bool g_tracing;
void Record(std::uint32_t what, std::uint32_t a = 0, std::uint32_t b = 0, std::uint32_t c = 0, std::uint32_t d = 0,
            std::uint32_t e = 0, std::uint32_t f = 0, std::uint32_t h = 0, std::uint32_t i = 0) {
    if (g_tracing) bof3::Log("battle_windows trace:   %u (%X %X %X %X %X %X %X %X)", what, a, b, c, d, e, f, h, i);
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
std::uint32_t TextHash(const unsigned char* s, unsigned n) {
    std::uint32_t h = 0x811C9DC5u;
    for (unsigned i = 0; i < n && s[i]; ++i) h = (h ^ s[i]) * 0x01000193u;
    return h;
}

// --- the cells the functions read again after a call ------------------------

unsigned char* Member(unsigned i) { return At(at::kPartyObjects + at::kObjectStride * i); }
// A colour row whose CLUT shadow word is inside the randomised region.
unsigned char SmallRow(std::uint32_t v) { return static_cast<unsigned char>(static_cast<signed char>(v % 12) - 2); }

void Disturb() {
    const std::uint32_t h = Hash();
    if (h % 4 == 0) return;
    const unsigned v = (h >> 12) & 0xFF;
    const auto byte = static_cast<unsigned char>(h >> 24);
    switch ((h >> 4) % 18) {
    case 0: At(at::kColour)[0] = SmallRow(v); break;
    case 1: At(at::kPartyCount)[0] = static_cast<unsigned char>(v % 6); break;
    case 2: At(at::kCrossSel)[0] = static_cast<unsigned char>(v % 8); break;
    case 3: At(at::kCrossGrow + v % 7)[0] = static_cast<unsigned char>((h & 0x40000000u) ? 0 : byte); break;
    case 4:
    case 5: {
        static const unsigned kFields[] = {0x90, 0x91, 0x98, 0x99, 0x9A, 0x9B, 0xA0, 0xA1, 0xA2, 0xA3};
        Member(v % 5)[kFields[(h >> 20) % 10]] = byte;
        break;
    }
    case 6: At(at::kPartyBars + 36 * (v % 5) + (h >> 20) % 4)[0] = static_cast<unsigned char>((h & 0x40000000u) ? 0 : byte); break;
    case 7: At(at::kEnemies + at::kEnemyStride * (v % 6) + 0xF)[0] = static_cast<unsigned char>((h & 0x40000000u) ? 1 : byte); break;
    case 8: {
        unsigned char* const w = g_buf.windows[v % kWindows];
        w[3 + (h >> 20) % 0x10] = byte;
        break;
    }
    case 9: SetLong(At(at::kCurrent), static_cast<std::int32_t>(Address(g_buf.windows[v % kWindows]))); break;
    case 10: At(at::kMsgHead)[0] = static_cast<unsigned char>(v % 0x14); break;
    case 11: g_buf.objects[v % kObjects][0x134] = byte; break;
    case 12: At(at::kClutShadow + 64 * (static_cast<int>(v % 12) - 2) + (h >> 20) % 2)[0] = byte; break;
    case 13: At(at::kMsgRing + 8 * ((h >> 20) % 16))[0] = byte; break;
    // the first members' extra charge byte, read again after the first line
    case 14: At(at::kPartyBars + 36 * (v % 3) + 3)[0] = static_cast<unsigned char>((h & 0x40000000u) ? 0 : byte); break;
    default: break;
    }
}

// --- the stand-ins ----------------------------------------------------------
// Each records what the real callee reads of its arguments (docs/battle_windows.md
// section 3 has each callee's reads), so what the original leaves in the upper
// bits of a register it pushes is masked exactly as the real callee masks it.

constexpr std::uint32_t kPointZeroOne = 0x3C23D70Au;
void PutDword(unsigned char* at, std::uint32_t v) { std::memcpy(at, &v, sizeof v); }
std::uint32_t Lo16(int v) { return static_cast<std::uint32_t>(v) & 0xFFFF; }

unsigned __cdecl StubGetTPage(unsigned tp, unsigned abr, int x, int y) {
    Record(1, tp, abr, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y));
    return Hash();
}
void __cdecl StubDrawMode(unsigned char* prim, int dfe, int dtd, unsigned tpage, unsigned long tw) {
    Record(2, Id(prim), static_cast<std::uint32_t>(dfe), static_cast<std::uint32_t>(dtd), tpage,
           static_cast<std::uint32_t>(tw));
    PutDword(prim + 4, 0xE1000000u | (tpage & 0xFFFFu));
    Disturb();
}
void __cdecl StubCommit(unsigned slot, unsigned size) {
    unsigned char* const at = Gfx_PacketNext;
    Record(3, slot, size, Id(at));
    PutDword(at, 0xC0000000u | (Hash() & 0xFFFFFF));
    const unsigned offset = static_cast<unsigned>(at - g_buf.pool);
    Gfx_PacketNext = g_buf.pool + (offset + (size & 0xFF) + (Hash() % 3 == 0 ? 4 : 0)) % 0x400u;
    Disturb();
}
void __cdecl StubPolyFT4(unsigned char* prim) {
    Record(4, Id(prim));
    prim[7] = 0x2C;
    for (unsigned at = 0x10; at <= 0x40; at += 0x10) PutDword(prim + at, kPointZeroOne);
}
const unsigned char* __cdecl StubDrawAt(int x, int y, int colour, int count, const unsigned char* text) {
    Record(5, Lo16(x), Lo16(y), static_cast<std::uint32_t>(colour) & 0xFF, static_cast<std::uint32_t>(count) & 0xFF,
           Id(text));
    Disturb();
    return text + (Hash() & 7);
}
void __cdecl StubFont8(int x, int y, int colour, const unsigned char* text) {
    Record(6, Lo16(x), Lo16(y), static_cast<std::uint32_t>(colour) & 0xFF, Id(text), TextHash(text, 40));
    Disturb();
}
// sprintf: nothing of the CRT - the format's first bytes and a value of the
// stand-ins' own, written into the buffer.
int __cdecl StubSprintf(char* dst, const char* fmt, ...) {
    Record(7, Id(dst), Id(fmt), TextHash(reinterpret_cast<const unsigned char*>(fmt), 8));
    const std::uint32_t h = Hash();
    unsigned n = 0;
    for (; n < 4 && fmt[n]; ++n) dst[n] = fmt[n];
    for (unsigned k = 0; k < 4; ++k) dst[n++] = "0123456789ABCDEF"[(h >> (4 * k)) & 0xF];
    dst[n] = 0;
    Disturb();
    return static_cast<int>(n);
}
void __cdecl StubIcon(unsigned icon, unsigned x, unsigned y, unsigned w, unsigned h, unsigned shade) {
    Record(8, icon & 0xFF, x & 0xFFFF, y & 0xFFFF, w & 0xFF, h & 0xFF, shade & 0xFF);
    Disturb();
}
void __cdecl StubUpdateScreen() {
    Record(9, Id(Field_State), Id(Sprite_Current));
    Disturb();
}
unsigned __cdecl StubTick() { Record(10, Id(Field_State), Id(Sprite_Current)); Disturb(); return Hash(); }
unsigned __cdecl StubTickOnce() { Record(11, Id(Field_State), Id(Sprite_Current)); Disturb(); return Hash(); }
unsigned __cdecl StubEnsurePose(unsigned pose) { Record(12, pose & 0xFF, Id(Sprite_Current)); Disturb(); return Hash(); }
unsigned __cdecl StubPoseFrom(unsigned pose, std::uint32_t set, unsigned size) {
    Record(13, pose & 0xFF, set, size);
    Disturb();
    return Hash();
}
void __cdecl StubDrawValue(int x, int y, unsigned colour, unsigned value) {
    Record(14, Lo16(x), Lo16(y), colour & 0xFF, value & 0xFFFF);
    Disturb();
}
void __cdecl StubDrawEdge(int x, int y, unsigned piece, unsigned semi) {
    Record(15, Lo16(x), Lo16(y), piece & 0xFF, semi & 0xFF);
    Disturb();
}
void __cdecl StubDrawTile(int x, int y, unsigned size, unsigned semi) {
    Record(16, Lo16(x), Lo16(y), size & 0xFF, semi & 0xFF);
    Disturb();
}
void __cdecl StubDrawTileRgb(int x, int y, unsigned size, unsigned colour, unsigned semi) {
    Record(17, Lo16(x), Lo16(y), size & 0xFF, colour & 0x7FFF, semi & 0xFF);
    Disturb();
}
void __cdecl StubDrawBar(int x, int y, unsigned width, unsigned fill, unsigned flag) {
    Record(18, Lo16(x), Lo16(y), width & 0xFF, fill & 0xFF, flag & 0xFF);
    Disturb();
}
void __cdecl StubDrawDigit(int x, int y, unsigned n) {
    Record(19, Lo16(x), Lo16(y), n & 0xF);
    Disturb();
}
void __cdecl StubLinePlain(int x0, int y0, int x1, int y1, unsigned r, unsigned gr, unsigned b) {
    Record(20, Lo16(x0), Lo16(y0), Lo16(x1), Lo16(y1), r & 0xFF, gr & 0xFF, b & 0xFF);
    Disturb();
}
void __cdecl StubLineSemi0(int x0, int y0, int x1, int y1, unsigned r, unsigned gr, unsigned b) {
    Record(21, Lo16(x0), Lo16(y0), Lo16(x1), Lo16(y1), r & 0xFF, gr & 0xFF, b & 0xFF);
    Disturb();
}
void __cdecl StubLineSemi1(int x0, int y0, int x1, int y1, unsigned r, unsigned gr, unsigned b) {
    Record(22, Lo16(x0), Lo16(y0), Lo16(x1), Lo16(y1), r & 0xFF, gr & 0xFF, b & 0xFF);
    Disturb();
}
unsigned __cdecl StubNameShown(unsigned e) {
    Record(23, e & 0xFF);
    Disturb();
    const std::uint32_t h = Hash();
    return h % 3 == 0 ? (h & 0xFFFFFF00u) : h;
}
// 0x516E70 hands x and y whole to its glyph draw: recorded whole.
const unsigned char* __cdecl StubTinyFont(int x, int y, unsigned colour, unsigned count, const unsigned char* text) {
    Record(24, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y), colour & 0xFF, count & 0xFF, Id(text));
    Disturb();
    return text + 1;
}
// Ours in this module, stood in when their callers are fuzzed.
unsigned __cdecl StubRunState() { Record(25, Id(Field_State), Id(Sprite_Current)); Disturb(); return Hash(); }
void __cdecl StubCommandIcon(unsigned k, unsigned x, unsigned y, unsigned w, unsigned h, unsigned shade) {
    Record(26, k, x & 0xFFFF, y & 0xFFFF, w & 0xFF, h & 0xFF, shade & 0xFF);
    Disturb();
}
void __cdecl StubMessageBox(int x, int y) {
    Record(27, Lo16(x), Lo16(y));
    Disturb();
}

// The state handlers: 32 distinct stand-ins, the table of 256 cycling them
// (the original's state byte is unbounded).
template <unsigned N> unsigned __cdecl StubState() {
    Record(100 + N, Id(Field_State), Id(Sprite_Current));
    Disturb();
    return Hash();
}
using StateFn = unsigned (__cdecl*)();
template <std::size_t... I> constexpr auto MakeStates(std::index_sequence<I...>) {
    struct T { StateFn f[sizeof...(I)]; };
    return T{{&StubState<static_cast<unsigned>(I)>...}};
}
constexpr auto kStates = MakeStates(std::make_index_sequence<32>{});
std::uint32_t g_state_table[256];

// The window kinds' three handlers: eight arguments, the answer theirs.
template <unsigned N>
unsigned __cdecl StubKind(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2, std::uint32_t a3, std::uint32_t a4,
                          std::uint32_t a5, std::uint32_t a6, std::uint32_t a7) {
    Record(60 + N, a0, a1, a2, a3, a4, a5, a6, Id(reinterpret_cast<const void*>(a7)));
    Disturb();
    return Hash();
}

const void* StubFor(std::uint32_t target) {
    const auto f = [](auto p) { return reinterpret_cast<const void*>(p); };
    switch (target) {
    case 0x5A79A0: return f(&StubGetTPage);
    case 0x5A77C0: return f(&StubDrawMode);
    case 0x461E50: return f(&StubCommit);
    case 0x5A75D0: return f(&StubPolyFT4);
    case 0x516B30: return f(&StubDrawAt);
    case 0x517090: return f(&StubFont8);
    case 0x5B9380: return f(&StubSprintf);
    case 0x5903F0: return f(&StubIcon);
    case 0x588F20: return f(&StubUpdateScreen);
    case 0x5893A0: return f(&StubTick);
    case 0x589410: return f(&StubTickOnce);
    case 0x589330: return f(&StubEnsurePose);
    case kPoseFrom: return f(&StubPoseFrom);
    case kDrawValue: return f(&StubDrawValue);
    case kDrawEdge: return f(&StubDrawEdge);
    case kDrawTile: return f(&StubDrawTile);
    case kDrawTileRgb: return f(&StubDrawTileRgb);
    case kDrawBar: return f(&StubDrawBar);
    case kDrawDigit: return f(&StubDrawDigit);
    case kLinePlain: return f(&StubLinePlain);
    case kLineSemi0: return f(&StubLineSemi0);
    case kLineSemi1: return f(&StubLineSemi1);
    case kEnemyNameShown: return f(&StubNameShown);
    case kTinyFont: return f(&StubTinyFont);
    case 0x4411E0: return f(&StubRunState);
    case 0x4434C0: return f(&StubCommandIcon);
    case 0x443610: return f(&StubMessageBox);
    default: bof3::Fatal("battle_windows: no stand-in for a call to 0x%X", static_cast<unsigned>(target)); return nullptr;
    }
}

Callees Stubs() {
    Callees s{};
    s.get_tpage = StubGetTPage;
    s.draw_mode = StubDrawMode;
    s.commit = StubCommit;
    s.set_poly_ft4 = StubPolyFT4;
    s.text_draw_at = StubDrawAt;
    s.font8 = StubFont8;
    s.sprintf_ = StubSprintf;
    s.icon = StubIcon;
    s.update_screen = StubUpdateScreen;
    s.script_tick = StubTick;
    s.script_tick_once = StubTickOnce;
    s.ensure_pose = StubEnsurePose;
    s.pose_from = StubPoseFrom;
    s.draw_value = StubDrawValue;
    s.draw_edge = StubDrawEdge;
    s.draw_tile = StubDrawTile;
    s.draw_tile_rgb = StubDrawTileRgb;
    s.draw_bar = StubDrawBar;
    s.draw_digit = StubDrawDigit;
    s.line_plain = StubLinePlain;
    s.line_semi0 = StubLineSemi0;
    s.line_semi1 = StubLineSemi1;
    s.enemy_name_shown = StubNameShown;
    s.tiny_font = StubTinyFont;
    s.state_table = g_state_table;
    s.kind_handlers[0] = Address(reinterpret_cast<const void*>(&StubKind<0>));
    s.kind_handlers[1] = Address(reinterpret_cast<const void*>(&StubKind<1>));
    s.kind_handlers[2] = Address(reinterpret_cast<const void*>(&StubKind<2>));
    s.run_state = StubRunState;
    s.command_icon = StubCommandIcon;
    s.message_box = StubMessageBox;
    return s;
}

// --- the twenty-two copies (capstone, 2026-09-23: every jump internal but
// the listed calls and tail jumps; 0x4411E0's table jump and 0x597A30's
// handler immediates patched below) ------------------------------------------

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

constexpr Call k441100[] = {{0xF, 0x4411E0}, {0x23, 0x4411E0}, {0x37, 0x4411E0}};
constexpr Call k441140[] = {{0x22, 0x588F20}};
constexpr Call k441180[] = {{0x21, 0x5893A0}};
constexpr Call k4411B0[] = {{0x21, 0x589410}};
constexpr Call k4412B0[] = {{0x4D, 0x589110}, {0x5F, 0x589110}, {0x9A, 0x589110}, {0xAF, 0x589110}, {0xD7, 0x589330}, {0xF1, 0x589330}, {0x114, 0x589330}, {0x136, 0x589330}, {0x153, 0x589330}, {0x189, 0x589330}, {0x19E, 0x589330}, {0x1B3, 0x589330}, {0x1C9, 0x589330}, {0x1E7, 0x589330}, {0x212, 0x589330}, {0x227, 0x589330}, {0x23D, 0x589330}};
constexpr Call k442FA0[] = {{0x12, 0x5A79A0}, {0x2A, 0x5A77C0}, {0x33, 0x461E50}, {0x50, 0x4447B0}, {0x68, 0x4447B0}, {0x82, 0x444900}, {0x114, 0x4449E0}, {0x13B, 0x444A90}, {0x176, 0x444E00}, {0x1B0, 0x444E00}, {0x1EE, 0x516E70}, {0x1FE, 0x444C40}, {0x20A, 0x444C40}, {0x244, 0x444340}, {0x28B, 0x444340}, {0x2AD, 0x444E00}, {0x2CD, 0x444E00}, {0x2F0, 0x444D50}, {0x30C, 0x444D50}};
constexpr Call k4432F0[] = {{0x81, 0x5903F0}, {0x105, 0x5903F0}, {0x13B, 0x4434C0}, {0x17A, 0x4434C0}, {0x1B9, 0x4434C0}};
constexpr Call k4434C0[] = {{0x35, 0x5A79A0}, {0x4D, 0x5A77C0}, {0x56, 0x461E50}, {0x62, 0x5A75D0}, {0x116, 0x5A79A0}, {0x13F, 0x461E50}};
constexpr Call k443610[] = {{0x14, 0x5A79A0}, {0x2C, 0x5A77C0}, {0x35, 0x461E50}, {0x89, 0x444900}, {0x94, 0x4447B0}, {0xA2, 0x4447B0}, {0xC6, 0x444E00}, {0xE8, 0x444E00}, {0x104, 0x444D50}, {0x11F, 0x444D50}};
constexpr Call k443740[] = {{0x14, 0x5A79A0}, {0x2C, 0x5A77C0}, {0x35, 0x461E50}, {0x89, 0x444900}, {0x94, 0x4447B0}, {0xA2, 0x4447B0}, {0xC3, 0x444E00}, {0xE5, 0x444E00}, {0x101, 0x444D50}, {0x11C, 0x444D50}};
constexpr Call k443870[] = {{0x14, 0x5A79A0}, {0x2C, 0x5A77C0}, {0x35, 0x461E50}, {0x89, 0x444900}, {0x94, 0x4447B0}, {0xA2, 0x4447B0}, {0xC3, 0x444E00}, {0xE5, 0x444E00}, {0x101, 0x444D50}, {0x11C, 0x444D50}};
constexpr Call k4439A0[] = {{0x14, 0x5A79A0}, {0x2C, 0x5A77C0}, {0x35, 0x461E50}, {0x9B, 0x444900}, {0xA6, 0x4447B0}, {0xB4, 0x4447B0}, {0xD5, 0x444E00}, {0xFE, 0x444E00}, {0x119, 0x444D50}, {0x13B, 0x444D50}, {0x155, 0x516B30}};
constexpr Call k443B10[] = {{0x14, 0x5A79A0}, {0x2C, 0x5A77C0}, {0x35, 0x461E50}, {0x8E, 0x444900}, {0x9C, 0x4447B0}, {0xAA, 0x4447B0}, {0xBF, 0x5A79A0}, {0xD7, 0x5A77C0}, {0xE0, 0x461E50}, {0xEE, 0x444900}, {0x101, 0x4449E0}, {0x10F, 0x4447B0}, {0x11D, 0x4447B0}, {0x15B, 0x444A90}, {0x174, 0x444340}, {0x194, 0x516E70}, {0x1B9, 0x444E00}, {0x1D0, 0x444E00}, {0x250, 0x444CE0}, {0x264, 0x444CE0}};
constexpr Call k443D90[] = {{0x14, 0x5A79A0}, {0x2C, 0x5A77C0}, {0x35, 0x461E50}, {0x93, 0x444900}, {0xA6, 0x4449E0}, {0xB4, 0x4447B0}, {0xC2, 0x4447B0}, {0xFC, 0x444A90}, {0x115, 0x444340}, {0x122, 0x444EB0}, {0x141, 0x516E70}, {0x162, 0x444E00}, {0x184, 0x444E00}, {0x1A0, 0x444D50}, {0x1BB, 0x444D50}};
constexpr Call k443F60[] = {{0x14, 0x5A79A0}, {0x2C, 0x5A77C0}, {0x35, 0x461E50}, {0x83, 0x444900}, {0x91, 0x4447B0}, {0x9F, 0x4447B0}, {0xB4, 0x5A79A0}, {0xCC, 0x5A77C0}, {0xD5, 0x461E50}, {0xE3, 0x444900}, {0xEE, 0x4447B0}, {0xFC, 0x4447B0}, {0x14E, 0x516E70}, {0x180, 0x444C40}, {0x19A, 0x444340}, {0x1B9, 0x5B9380}, {0x1D0, 0x517090}, {0x1EA, 0x444340}, {0x213, 0x444E00}, {0x22D, 0x444E00}, {0x2A1, 0x444CE0}, {0x2B5, 0x444CE0}};
constexpr Call k597ED0[] = {{0xF, 0x443610}, {0x41, 0x516B30}};

constexpr Table kNoTable = {0, 0, 0};
#define BW_C(name, base, size, calls, ret) {#name, base, size, calls, static_cast<int>(sizeof calls / sizeof calls[0]), kNoTable, ret, reinterpret_cast<const void*>(&::name)}
#define BW_T(name, base, size, calls, table, ret) {#name, base, size, calls, static_cast<int>(sizeof calls / sizeof calls[0]), table, ret, reinterpret_cast<const void*>(&::name)}
#define BW_P(name, base, size, ret) {#name, base, size, nullptr, 0, kNoTable, ret, reinterpret_cast<const void*>(&::name)}
const Clone kClones[] = {
    BW_C(BattleParty_RunStates, 0x441100, 0x3C, k441100, 4),
    BW_C(BattleParty_UpdateScreens, 0x441140, 0x33, k441140, 0),
    BW_C(BattleObj_ScriptTick, 0x441180, 0x29, k441180, 1),
    BW_C(BattleObj_ScriptTickOnce, 0x4411B0, 0x29, k4411B0, 1),
    BW_P(BattleObj_RunState, 0x4411E0, 0x17, 4),
    BW_T(BattleObj_PickPose, 0x4412B0, 0x260, k4412B0, (Table{0x170, 0x248, 6}), 4),
    BW_C(BattleWin_DrawPartyStatus, 0x442FA0, 0x342, k442FA0, 0),
    BW_C(BattleWin_DrawCommandCross, 0x4432F0, 0x1C9, k4432F0, 0),
    BW_C(BattleWin_DrawCommandIcon, 0x4434C0, 0x14D, k4434C0, 0),
    BW_C(BattleWin_DrawMessageBox, 0x443610, 0x12F, k443610, 0),
    BW_C(BattleWin_DrawSmallBox, 0x443740, 0x12C, k443740, 0),
    BW_C(BattleWin_DrawMediumBox, 0x443870, 0x12C, k443870, 0),
    BW_C(BattleWin_DrawCommandLabel, 0x4439A0, 0x165, k4439A0, 0),
    BW_C(BattleWin_DrawTargetEnemy, 0x443B10, 0x274, k443B10, 0),
    BW_C(BattleWin_DrawEnemyStatus, 0x443D90, 0x1CB, k443D90, 0),
    BW_C(BattleWin_DrawTargetMember, 0x443F60, 0x2C5, k443F60, 0),
    BW_P(Window_FlagAndAdvance, 0x5979E0, 0x12, 0),
    BW_P(Window_RestoreAndBack, 0x597A00, 0x2C, 0),
    BW_P(Window_DispatchKind, 0x597A30, 0x4F, 4),
    BW_C(BattleWin_DrawMessage, 0x597ED0, 0x4A, k597ED0, 0),
    BW_P(BattleMsg_Advance, 0x597F20, 0x1A, 1),
    BW_P(Text_GlyphCount, 0x597F40, 0x1D, 1),
};
#undef BW_C
#undef BW_T
#undef BW_P
constexpr unsigned kCount = sizeof kClones / sizeof kClones[0];

enum : unsigned {
    kRunStates, kUpdateScreens, kTick, kTickOnce, kRunState, kPickPose, kPartyStatus, kCross, kCommandIcon,
    kMessageBox, kSmallBox, kMediumBox, kLabel, kTargetEnemy, kEnemyStatus, kTargetMember, kFlagAdvance,
    kRestoreBack, kDispatch, kMessage, kMsgAdvance, kGlyphCount,
};
static_assert(kGlyphCount + 1 == kCount, "the index list and the clone list disagree");

// --- the state both passes start from ---------------------------------------

struct Region { std::uint32_t at, size; };
const Region kRegions[] = {
    {0x7E0670, 4},                 // Gfx_PacketNext (into our pool)
    {0x802D40, 0x700},             // the party objects (five), the bar bytes 0x80333F + 36 i
    {0x80B728, 0x300},             // the CLUT shadow rows -2..9
    {0x903A50, 0x10},              // the window colour 0x903A5A
    {0x904AA0, 0x50},              // the battle's bytes: kind, count, cross, pose flags
    {0x904B80, 0x60},              // the tick gate 0x904B8E, the print buffer
    {0x905B80, 8},                 // the current window record pointer
    {0x905D98, 4},                 // Field_State
    {0x937F88, 4},                 // Sprite_Current
    {0x939EC4, 4},                 // the acting member's record pointer
    {0x93B9E0, 6 * 0x128},         // six enemy records
    {0x93C2A0, 0xA8},              // the message ring
};
constexpr unsigned kRegionBytes = 4 + 0x700 + 0x300 + 0x10 + 0x50 + 0x60 + 8 + 4 + 4 + 4 + 6 * 0x128 + 0xA8;

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

void Text(unsigned char* s, unsigned size) {
    const unsigned len = Next() % (size - 4);
    for (unsigned i = 0; i < len; ++i) {
        std::uint32_t c = Next();
        s[i] = static_cast<unsigned char>(c % 3 == 0 ? 0x80 | (c >> 8) : 0x20 + (c >> 8) % 0x5F);
    }
    s[len] = 0;
    // a lead byte right before the end takes the NUL with it (the count reads on)
    if (len && Half()) s[len - 1] = static_cast<unsigned char>(0x80 | Next());
    s[len + 1] = static_cast<unsigned char>(Half() ? 0 : 0x41);
    s[len + 2] = 0;
    s[size - 1] = 0;
}
void Fix() {
    Gfx_PacketNext = g_buf.pool + Next() % 0x40;
    SetLong(At(at::kCurrent), static_cast<std::int32_t>(Address(g_buf.windows[Next() % kWindows])));
    Field_State = g_buf.objects[Next() % kObjects];
    Sprite_Current = g_buf.objects[Next() % kObjects];
    SetLong(At(at::kActor), static_cast<std::int32_t>(Address(g_buf.objects[Next() % kObjects])));
    At(at::kColour)[0] = SmallRow(Next());
    At(at::kPartyCount)[0] = static_cast<unsigned char>(Next() % 6);
    for (unsigned k = 0; k < kTexts; ++k) Text(g_buf.texts[k], kTextBytes);
}

struct Args { std::uint32_t a[8]; };

// Skills (0..0x13F) whose flag byte 0x65C4DD + 24 skill has bit 3 set with
// bit 2 or 4 clear, or bit 3 clear with either set: .data the fuzz cannot
// vary, so the seeds pick from it (filled at the start of SelfTest).
std::uint16_t g_skills[0x140];
unsigned g_skills_n;

std::uint32_t Coord() {
    static const std::uint32_t kEdges[] = {0, 1, 0x7FFF, 0x8000, 0xFFFF, 0x10000, 0xFFFFFFF0u, 0x140, 0xF0};
    return Often() ? Garbage(0xFFFF, Next() % 0x180) : Half() ? kEdges[Next() % 9] : Next();
}
// A CLUT shadow word near the darker lines' clamp: components 0..3 often.
void SeedLineColour() {
    const int row = static_cast<signed char>(At(at::kColour)[0]);
    unsigned w = Next();
    if (Often()) w = (Next() % 4) | (Next() % 4) << 5 | (Next() % 4) << 10 | (Next() & 0x8000);
    SetWord(At(at::kClutShadow + static_cast<std::uint32_t>(row * 64)), w);
}
void SeedMember(unsigned char* m) {
    static const unsigned kFlags[] = {0, 0x4000, 0x0004, 0x0800, 0x2000, 0x0080, 0x0040, 0x0BFC, 0x0400, 0x6000,
                                      0x20C0, 0x1000};
    SetWord(m + 0x90, Often() ? kFlags[Next() % 12] | (Half() ? 0 : Next() & 0xFFFF) : Next());
    const unsigned top = Half() ? 0 : Next() % 1000;
    SetWord(m + 0xA2, top);
    const unsigned quarter = top >> 2;
    static const int kNear[] = {-1, 0, 1};
    SetWord(m + 0x9A, Half() ? (Half() ? 0 : static_cast<unsigned>(static_cast<int>(quarter) + kNear[Next() % 3])) : Next());
}

Args Seed(unsigned k) {
    Args args;
    for (auto& a : args.a) a = Next();
    switch (k) {
    case kRunStates:
        for (unsigned i = 0; i < 3; ++i) Member(i)[0] = static_cast<unsigned char>(Next());
        break;
    case kUpdateScreens:
        for (unsigned i = 0; i < 3; ++i) Member(i)[0] = static_cast<unsigned char>(Next() & 0x41);
        break;
    case kTick:
    case kTickOnce: {
        unsigned char* const f = Field_State;
        f[0x90] = static_cast<unsigned char>(Half() ? Next() & ~4u : Next());
        f[0x134] = static_cast<unsigned char>(Half() ? Next() & ~0x10u : Next());
        At(at::kTickGate)[0] = static_cast<unsigned char>(Half() ? 0 : Next());
        break;
    }
    case kRunState: {
        unsigned char* const s = Sprite_Current;
        s[0] = static_cast<unsigned char>(Often() ? Next() | 1 : Next());
        s[1] = static_cast<unsigned char>(Often() ? Next() % 27 : Next());
        break;
    }
    case kPickPose: {
        static const unsigned char kKinds[] = {5, 5, 4, 6, 0};
        At(at::kBattleKind)[0] = Often() ? kKinds[Next() % 5] : static_cast<unsigned char>(Next());
        At(at::kKindFlags)[0] = static_cast<unsigned char>(Next() & 3);
        At(at::kPoseFlags)[0] = static_cast<unsigned char>(Next() & 0x30);
        At(at::kPoseMode)[0] = static_cast<unsigned char>(Next() & 0x18);
        unsigned char* const f = Field_State;
        SeedMember(f);
        f[0x91] = static_cast<unsigned char>(Half() ? f[0x91] & ~0x48u : f[0x91]);
        f[0x130] = static_cast<unsigned char>(Half() ? Next() & ~2u : Next());
        f[0x125] = static_cast<unsigned char>(Often() ? (Half() ? 4 : Next() % 8) : Next());
        f[0x134] = static_cast<unsigned char>(Next());
        // a skill whose flag byte tells bit 3 from its neighbours half the time
        SetWord(f + 0x126, g_skills_n && Half() ? g_skills[Next() % g_skills_n] : Often() ? Next() % 0x140 : Next());
        break;
    }
    case kPartyStatus:
        args.a[0] = Coord();
        args.a[1] = Coord();
        for (unsigned i = 0; i < 5; ++i) {
            SeedMember(Member(i));
            Member(i)[0x91] = static_cast<unsigned char>(Next());
            if (Half()) At(at::kPartyBars + 36 * i + 3)[0] = 0;
        }
        break;
    case kCross: {
        args.a[0] = Coord();
        args.a[1] = Coord();
        static const unsigned char kSel[] = {0, 1, 2, 3, 4, 5, 6, 7, 2, 3};
        At(at::kCrossSel)[0] = Often() ? kSel[Next() % 10] : static_cast<unsigned char>(Next());
        for (unsigned j = 0; j < 7; ++j) At(at::kCrossGrow + j)[0] = static_cast<unsigned char>(Half() ? 0 : Next());
        break;
    }
    case kCommandIcon: {
        // 7 reads a byte the original never wrote: never seeded (section 4)
        std::uint32_t i = Often() ? Next() % 7 : 8 + Next() % 36;
        args.a[0] = Garbage(0xFF, i);
        args.a[1] = Coord();
        args.a[2] = Coord();
        break;
    }
    case kMessageBox:
    case kSmallBox:
    case kMediumBox:
        args.a[0] = Coord();
        args.a[1] = Coord();
        break;
    case kLabel:
        args.a[0] = Garbage(0xFF, Often() ? Next() % 7 : Next());
        break;
    case kTargetEnemy:
    case kEnemyStatus:
        args.a[0] = Coord();
        args.a[1] = Coord();
        args.a[2] = Garbage(0xFF, 3 + (Often() ? Next() % 6 : Next() % 11));
        for (unsigned e = 0; e < 6; ++e)
            At(at::kEnemies + at::kEnemyStride * e + 0xF)[0] = static_cast<unsigned char>(Half() ? 1 : Next());
        SeedLineColour();
        break;
    case kTargetMember:
        args.a[0] = Coord();
        args.a[1] = Coord();
        args.a[2] = Garbage(0xFF, Next() % 5);
        SeedMember(Member(args.a[2] & 0xFF));
        SeedLineColour();
        break;
    case kFlagAdvance:
    case kRestoreBack:
        break;
    case kDispatch: {
        unsigned char* const t = g_buf.texts[Next() % kTexts];
        t[0] = static_cast<unsigned char>(Next() % 3);
        args.a[7] = Address(t);
        break;
    }
    case kMessage:
        At(at::kMsgHead)[0] = static_cast<unsigned char>(Often() ? Next() % 16 : Next());
        break;
    case kMsgAdvance: {
        const unsigned head = Next() % 16;
        At(at::kMsgHead)[0] = static_cast<unsigned char>(Often() ? head : Next());
        At(at::kMsgTail)[0] = static_cast<unsigned char>(Often() ? (head + 1) & 0xF : Half() ? head + 1 : Next());
        break;
    }
    case kGlyphCount: args.a[0] = Address(g_buf.texts[Next() % kTexts]); break;
    default: break;
    }
    return args;
}

// What the rounds reached, from the original's side, for the log line.
struct Coverage {
    unsigned logged[160];
    unsigned members_drawn, names_hidden, icon_past_table, pose_default, msg_wrapped;
} g_cover;

void Cover(unsigned k, const Args& args, const State& out, std::uint32_t result) {
    for (unsigned i = 0; i < out.log_n && i < kLog; ++i)
        if (out.log[i].what < 160) ++g_cover.logged[out.log[i].what];
    if (k == kPartyStatus)
        for (unsigned i = 0; i < out.log_n && i < kLog; ++i) g_cover.members_drawn += out.log[i].what == 24 ? 1 : 0;
    if (k == kEnemyStatus) {
        bool shown = false;
        for (unsigned i = 0; i < out.log_n && i < kLog; ++i) shown = shown || out.log[i].what == 24;
        g_cover.names_hidden += shown ? 0 : 1;
    }
    if (k == kCommandIcon && (args.a[0] & 0xFF) >= 7) ++g_cover.icon_past_table;
    if (k == kPickPose) {
        bool called = false;
        for (unsigned i = 0; i < out.log_n && i < kLog; ++i) called = called || out.log[i].what == 12 || out.log[i].what == 13;
        g_cover.pose_default += called ? 0 : 1;
    }
    if (k == kMsgAdvance && result == 1) ++g_cover.msg_wrapped;
}

using Fn8 = std::uint32_t (__cdecl*)(std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t,
                                      std::uint32_t, std::uint32_t, std::uint32_t);

}  // namespace

void SelfTest() {
    constexpr unsigned kPerFunction = 3000;
    unsigned region_bytes = 0;
    for (const Region& r : kRegions) region_bytes += r.size;
    if (region_bytes != kRegionBytes)
        bof3::Fatal("battle_windows: the regions are %u bytes, the state holds %u", region_bytes, kRegionBytes);

    for (unsigned i = 0; i < 256; ++i) g_state_table[i] = Address(reinterpret_cast<const void*>(kStates.f[i % 32]));
    g_skills_n = 0;
    for (unsigned i = 0; i < 0x140; ++i) {
        const unsigned b = At(at::kSkillFlags + 24 * i)[0];
        if (((b >> 3) & 1) != ((b >> 2) & 1) || ((b >> 3) & 1) != ((b >> 4) & 1)) g_skills[g_skills_n++] = static_cast<std::uint16_t>(i);
    }

    void* clones[kCount];
    for (unsigned k = 0; k < kCount; ++k) {
        const Clone& c = kClones[k];
        bof3::CloneCall calls[32];
        if (c.n_calls > 32) bof3::Fatal("battle_windows: %s has %d calls", c.name, c.n_calls);
        for (int i = 0; i < c.n_calls; ++i) calls[i] = {c.calls[i].offset, StubFor(c.calls[i].target), c.calls[i].target};
        clones[k] = bof3::CloneOriginal(c.name, c.base, c.size, calls, c.n_calls);
        if (c.table.entries) move_script::Relocate(clones[k], c.base, c.size, {c.table.jmp_disp, c.table.table, c.table.entries});
    }
    // 0x4411E0: `jmp [ecx*4 + 0x64DFE0]` - its operand at +0x12 aimed at our table.
    {
        auto* const code = static_cast<unsigned char*>(clones[kRunState]);
        std::uint32_t disp;
        std::memcpy(&disp, code + 0x12, 4);
        if (disp != at::kStateTable) bof3::Fatal("battle_windows: no state table operand in 0x4411E0's copy");
        const std::uint32_t ours = Address(g_state_table);
        std::memcpy(code + 0x12, &ours, 4);
    }
    // 0x597A30: `mov dword ptr [esp + 0x20 + 4 i], handler` - the three
    // immediates at +0x33, +0x3B, +0x43 aimed at the stand-ins.
    const Callees stubs = Stubs();
    {
        auto* const code = static_cast<unsigned char*>(clones[kDispatch]);
        for (unsigned i = 0; i < 3; ++i) {
            std::uint32_t imm;
            std::memcpy(&imm, code + 0x33 + 8 * i, 4);
            if (imm != at::kKindHandlers[i]) bof3::Fatal("battle_windows: no handler immediate %u in 0x597A30's copy", i);
            std::memcpy(code + 0x33 + 8 * i, &stubs.kind_handlers[i], 4);
        }
    }

    static State saved, input, their_out, our_out;
    Capture(saved);
    const Callees kept = g;
    g = stubs;
    const unsigned short saved_cw = GetControlWord();
    char trace_env[16] = {};
    unsigned trace_round = ~0u;
    if (GetEnvironmentVariableA("BOF3X_BW_TRACE", trace_env, sizeof trace_env) != 0) {
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
            if (g_tracing) bof3::Log("battle_windows trace: round %u %s pass %d", round, kClones[k].name, pass);
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
        Cover(k, args, their_out, result[0]);
        if (std::memcmp(&their_out, &our_out, sizeof their_out) != 0) {
            ++bad_per[k];
            if (++bad <= 12) {
                unsigned first = 0;
                while (first < sizeof their_out &&
                       reinterpret_cast<const unsigned char*>(&their_out)[first] ==
                           reinterpret_cast<const unsigned char*>(&our_out)[first])
                    ++first;
                bof3::Log("shadow      battle_windows self-test MISMATCH: round %u, %s, log %u / %u, result 0x%X / 0x%X, "
                          "first differing state byte %u",
                          round, kClones[k].name, their_out.log_n, our_out.log_n, their_out.result, our_out.result,
                          first);
                if (first < kRegionBytes) {
                    unsigned at = 0;
                    for (const Region& r : kRegions) {
                        if (first < at + r.size) {
                            bof3::Log("shadow      battle_windows:   memory 0x%X: 0x%02X / 0x%02X", r.at + (first - at),
                                      their_out.memory[first], our_out.memory[first]);
                            break;
                        }
                        at += r.size;
                    }
                } else if (first < offsetof(State, result)) {
                    const unsigned o = first - static_cast<unsigned>(offsetof(State, buf));
                    bof3::Log("shadow      battle_windows:   buffer +0x%X (pool %u): 0x%02X / 0x%02X", o, kPool,
                              reinterpret_cast<const unsigned char*>(&their_out.buf)[o],
                              reinterpret_cast<const unsigned char*>(&our_out.buf)[o]);
                } else if (first >= offsetof(State, log) && first < offsetof(State, log_n)) {
                    const unsigned e = (first - static_cast<unsigned>(offsetof(State, log))) / sizeof(Entry);
                    const Entry& a = their_out.log[e];
                    const Entry& b = our_out.log[e];
                    bof3::Log("shadow      battle_windows:   log %u: %u (%X %X %X %X %X %X %X %X) / %u (%X %X %X %X %X %X %X %X)",
                              e, a.what, a.a, a.b, a.c, a.d, a.e, a.f, a.h, a.i, b.what, b.a, b.b, b.c, b.d, b.e, b.f,
                              b.h, b.i);
                }
            }
        }
    }
    g = kept;
    Apply(saved);

    bof3::Log("shadow      battle_windows self-test: %u rounds over %u functions (%u each), %u calls to the stand-ins, "
              "%u MISMATCHES; the packet cursor and our pool, the party objects and bars, the CLUT shadow, the colour, "
              "the battle's bytes, the print buffer, the current record, Field_State, Sprite_Current, the actor, the "
              "enemies, the message ring, our objects, window records and strings, the answer and the stand-ins' log "
              "compared",
              kPerFunction * kCount, kCount, kPerFunction, calls, bad);
    if (bad)
        for (unsigned k = 0; k < kCount; ++k)
            if (bad_per[k]) bof3::Log("shadow      battle_windows: %s mismatched in %u rounds", kClones[k].name, bad_per[k]);
    const Coverage& c = g_cover;
    unsigned states = 0, kinds = 0;
    for (unsigned i = 100; i < 132; ++i) states += c.logged[i];
    for (unsigned i = 60; i < 63; ++i) kinds += c.logged[i];
    bof3::Log("shadow      battle_windows coverage: members drawn %u, enemy names hidden %u, icons past the table %u, "
              "poses left alone %u, message ring caught up %u; calls: state handlers %u, window kinds %u, icons %u, "
              "command icons %u, poses %u / %u, gauges %u, values %u, tiny font %u, darker lines %u, "
              "screen updates %u, ticks %u / %u",
              c.members_drawn, c.names_hidden, c.icon_past_table, c.pose_default, c.msg_wrapped, states, kinds,
              c.logged[8], c.logged[26], c.logged[12], c.logged[13], c.logged[18], c.logged[14], c.logged[24],
              c.logged[20], c.logged[9], c.logged[10], c.logged[11]);
    if (bad) bof3::Fatal("the battle windows differ from the original in %u self-test rounds", bad);
}

}  // namespace battle_windows

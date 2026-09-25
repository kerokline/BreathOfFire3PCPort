// BOF3X_SHADOW=map_field_objects: a differential fuzz of group DD's sixteen
// functions against byte-copies of Capcom's, once at start-up
// (docs/map_field_objects.md section 3). Every call out of a copy is re-aimed
// at a recording stand-in, and ours is put on the same stand-ins through
// map_field_objects::g; EventScript_SkipSwitch's jump table is relocated into
// its copy.
//
// Per round: the state a function reads, random with each branch's boundaries
// seeded; Capcom's copy, then ours, from the same state, both under the
// game's x87 control word 0x027F (MapCell_DrawAnimated's corners are x87
// arithmetic, compared bit for bit); the calls out with their arguments (as
// the callee reads them), eax where a caller reads it, and every region either
// side could write compared.
//
// The stand-ins answer as the real callees would leave things for the caller
// to read - screen points on and around every cull bound, NaNs and
// infinities, depths of 0, a packet cursor that moves or (one time in four, a
// full pool) does not, printed text in the buffer - and they disturb, now and
// then, what the caller reads after the call: the scratch dwords, the record
// or entry, the member record, the clock, Sprite_Current and the object
// index. A value read or a store made on the wrong side of a call shows.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/map_field_objects_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

// Calls fn() with eax set on entry; returns eax (ScriptFlags_Clear40's
// caller's eax passes through it).
extern "C" __attribute__((naked)) std::uint32_t __cdecl MapFieldObjects_CallEax(const void*, std::uint32_t) {
    asm("pushl %ebp\n\t"
        "movl %esp, %ebp\n\t"
        "pushl %ebx\n\t"
        "pushl %esi\n\t"
        "pushl %edi\n\t"
        "movl 12(%ebp), %eax\n\t"
        "call *8(%ebp)\n\t"
        "popl %edi\n\t"
        "popl %esi\n\t"
        "popl %ebx\n\t"
        "movl %ebp, %esp\n\t"
        "popl %ebp\n\t"
        "ret");
}

namespace map_field_objects {
namespace {

unsigned char* At(U a) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a)); }
U Addr(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
U Dword(const unsigned char* p) {
    U v;
    std::memcpy(&v, p, sizeof v);
    return v;
}
U Word(const unsigned char* p) {
    std::uint16_t v;
    std::memcpy(&v, p, sizeof v);
    return v;
}
void SetDword(unsigned char* p, U v) { std::memcpy(p, &v, sizeof v); }
void SetWord(unsigned char* p, U v) {
    const auto w = static_cast<std::uint16_t>(v);
    std::memcpy(p, &w, sizeof w);
}
U FloatBits(float f) {
    U v;
    std::memcpy(&v, &f, sizeof v);
    return v;
}
template <typename T, typename F> T As(F f) { return reinterpret_cast<T>(reinterpret_cast<void*>(f)); }

constexpr unsigned short kGameControlWord = 0x027F;   // measured: psx_gte_float.cpp
unsigned short GetControlWord() {
    unsigned short cw;
    __asm__ volatile("fnstcw %0" : "=m"(cw));
    return cw;
}
void SetControlWord(unsigned short cw) { __asm__ volatile("fldcw %0" : : "m"(cw)); }

// --- randomness -----------------------------------------------------------------------
U g_rng;
U Next() {
    g_rng ^= g_rng << 13;
    g_rng ^= g_rng >> 17;
    g_rng ^= g_rng << 5;
    return g_rng;
}
bool OneIn(U n) { return Next() % n == 0; }
U Pick(std::initializer_list<U> seeds) { return seeds.begin()[Next() % seeds.size()]; }
U PickH(U h, std::initializer_list<U> seeds) { return seeds.begin()[h % seeds.size()]; }

// --- the stand-ins' log ------------------------------------------------------------------
constexpr unsigned kKeep = 48, kIds = 40;
struct Log {
    U n, hash;
    U keep[kKeep][6];
    unsigned counts[kIds];
};
Log g_log;
U g_seed;

void Record(U what, U a = 0, U b = 0, U c = 0, U d = 0, U e = 0) {
    if (g_log.n < kKeep) {
        U* k = g_log.keep[g_log.n];
        k[0] = what, k[1] = a, k[2] = b, k[3] = c, k[4] = d, k[5] = e;
    }
    for (const U v : {what, a, b, c, d, e}) g_log.hash = (g_log.hash ^ v) * 0x01000193u + 0x9E3779B9u;
    ++g_log.n;
    ++g_log.counts[what % kIds];
}
// Deterministic in the call's position: the same on both sides while the
// calls agree.
U Hash(U salt = 0) {
    U h = (g_seed + g_log.n * 0x2545F491u + salt * 0x9E3779B1u) * 0x9E3779B1u;
    h ^= h >> 15;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    h *= 0xC2B2AE35u;
    h ^= h >> 16;
    return h;
}

// --- the fuzz's own memory -----------------------------------------------------------------
constexpr U kPacketBytes = 0xA000;
alignas(16) unsigned char g_packets[kPacketBytes];
alignas(16) unsigned char g_record[0x200];   // a map-cell record, a set-up entry, an op, a script
alignas(16) unsigned char g_bytes[0x1000];   // AreaMap_Bytes, from +0x200
U Packet(const void* p) { return Addr(p) - Addr(g_packets); }
U Rec(const void* p) { return Addr(p) - Addr(g_record); }

// Regions the fuzz randomises and compares.
constexpr U kHeaderBytes = 0x2000;
constexpr U kUprightTables = 0x663280 - 0x66313C;   // MapCell_UprightCounts .. MapCell_WallTextures
constexpr U kObjectsFrom = kObjects - 3 * kObjectStride, kObjectsBytes = 33 * kObjectStride;
constexpr U kMembersBytes = 16 * kCharStride;

// --- disturbances ------------------------------------------------------------------------
// What the test in hand lets a stand-in change after its call, one of them a
// third of the time.
struct Watch {
    U at, size;
};
Watch g_watch[8];
int g_watches;
bool g_move_sprite;   // EventOp_8x: Sprite_Current and the object index move too
void SetWatch(std::initializer_list<Watch> w) {
    g_watches = 0;
    for (const Watch& x : w) g_watch[g_watches++] = x;
}
U SomeObject(U h) { return kObjects + (h % 33 - 3) * kObjectStride; }
void Disturb(U salt) {
    const U h = Hash(salt + 0x51);
    if (h % 3 != 0) return;
    if (g_move_sprite && (h >> 4) % 3 == 0) {
        if ((h >> 6) % 2) SetDword(At(kSpriteCurrent), SomeObject(h >> 8));
        else SetWord(At(kScratchX), (h >> 8) % 33 - 3);
        return;
    }
    if (g_watches == 0) return;
    const Watch& w = g_watch[(h >> 8) % static_cast<U>(g_watches)];
    const U byte = (h >> 12) % w.size;
    At(w.at + byte)[0] = static_cast<unsigned char>(h >> 20);
}

// Screen coordinates on and around a cull bound: the bound, a float step
// either side, far inside, far outside, NaNs (quiet and signalling, as bits),
// infinities, and random bits.
U ScreenBits(U h, float lo, float hi) {
    switch (h % 16) {
    case 0: return FloatBits(lo);
    case 1: return FloatBits(hi);
    case 2: return FloatBits(lo) + 1u;   // a float step past the bound (lo is negative)
    case 3: return FloatBits(lo) - 1u;   // a float step inside it
    case 4: return FloatBits(hi) + 1u;
    case 5: return FloatBits(hi) - 1u;
    case 6: return 0x7FC00000u;                                              // quiet NaN
    case 7: return 0x7F800001u | ((h >> 8) & 0x3FFFFFu);                     // signalling NaN
    case 8: return 0xFF800000u;                                              // -inf
    case 9: return 0x7F800000u;                                              // +inf
    case 10: return (h >> 4) ^ 0x5A5A5A5Au;
    default: return FloatBits(lo + (hi - lo) * static_cast<float>((h >> 8) % 1000) / 1000.0f);
    }
}

// --- the stand-ins -------------------------------------------------------------------------
float g_lo_x = -80, g_hi_x = 400, g_lo_y = -20, g_hi_y = 260;   // the bounds the test in hand culls on

long __cdecl StubElevation(long x, long y) {
    Record(1, static_cast<U>(x), static_cast<U>(y));
    Disturb(1);
    const U h = Hash(2);
    const U low = (h >> 3) % 2 ? PickH(h >> 5, {0, 1, 2, 3, 0xFFFF, 0xFFFE, 0x7FFF, 0x8000, 0x8001, 0x100}) : (h >> 5);
    return static_cast<long>((Hash(3) & 0xFFFF0000u) | (low & 0xFFFFu));
}
void ScreenPoint(float* s, U salt) {
    const U h = Hash(salt);
    SetDword(reinterpret_cast<unsigned char*>(s), ScreenBits(h, g_lo_x, g_hi_x));
    SetDword(reinterpret_cast<unsigned char*>(s) + 4, ScreenBits(h >> 5, g_lo_y, g_hi_y));
}
U VertexWords(const short* v) { return (static_cast<U>(static_cast<std::uint16_t>(v[0]))) | (static_cast<U>(static_cast<std::uint16_t>(v[1])) << 16); }
U VertexZ(const short* v) { return static_cast<std::uint16_t>(v[2]); }
long __cdecl StubRtp3(const short* v0, const short* v1, const short* v2, float* s0, float* s1, float* s2, long* p) {
    Record(2, VertexWords(v0), VertexZ(v0), VertexWords(v1), VertexZ(v1), VertexWords(v2));
    Record(2, VertexZ(v2), Addr(s0), Addr(s1), Addr(s2));
    ScreenPoint(s0, 4);
    ScreenPoint(s1, 5);
    ScreenPoint(s2, 6);
    *p = static_cast<long>(Hash(7));
    return static_cast<long>(Hash(8));
}
void __cdecl StubDepthF3(float* a, float* b, float* c) {
    Record(3);
    SetDword(reinterpret_cast<unsigned char*>(a), Hash(9));
    SetDword(reinterpret_cast<unsigned char*>(b), Hash(10));
    SetDword(reinterpret_cast<unsigned char*>(c), Hash(11));
}
void __cdecl StubSetPolyFT4(unsigned char* prim) {
    Record(4, Packet(prim));
    const U h = Hash(12);
    for (U i = 4; i < 0x48; ++i) prim[i] = static_cast<unsigned char>(h >> (i % 24));
    prim[7] = 0x2C;
    Disturb(13);
}
void __cdecl StubSetShadeTex(unsigned char* prim, unsigned tge) {
    Record(5, Packet(prim), tge);
    prim[7] ^= 1;
    Disturb(14);
}
U PrimHash(const unsigned char* p, U bytes) {
    U h = 0x811C9DC5u;
    for (U i = 0; i < bytes; ++i) h = (h ^ p[i]) * 0x01000193u;
    return h;
}
// The real one reads both arguments as bytes (draw_emit.cpp); the primitive
// at the cursor is hashed as the caller left it; the cursor moves by the size
// three times in four.
U __cdecl StubCommit(unsigned slot, unsigned size) {
    unsigned char* const at = At(Dword(At(at::PacketNextAt())));
    const bool inside = Addr(at) >= Addr(g_packets) && Addr(at) + 0x48 <= Addr(g_packets) + kPacketBytes;
    Record(6, slot & 0xFFu, size & 0xFFu, Packet(at), inside ? PrimHash(at, 0x48) : 0);
    if (Hash(15) % 4 != 0) SetDword(At(at::PacketNextAt()), Addr(at) + (size & 0xFFu));
    Disturb(16);
    return Hash(17);
}
long __cdecl StubRtp4(const short* v0, const short* v1, const short* v2, const short* v3, float* s0, float* s1,
                      float* s2, float* s3, long* p) {
    Record(7, VertexWords(v0), VertexZ(v0), VertexWords(v1), VertexZ(v1), VertexWords(v2));
    Record(7, VertexZ(v2), VertexWords(v3), VertexZ(v3), Packet(s0), Packet(s1));
    Record(7, Packet(s2), Packet(s3));
    ScreenPoint(s0, 18);
    ScreenPoint(s1, 19);
    ScreenPoint(s2, 20);
    ScreenPoint(s3, 21);
    *p = static_cast<long>(Hash(22));
    return static_cast<long>(Hash(23));
}
void __cdecl StubDepthF4(float* a, float* b, float* c, float* d) {
    Record(8, Packet(a), Packet(b), Packet(c), Packet(d));
    SetDword(reinterpret_cast<unsigned char*>(a), Hash(24));
    SetDword(reinterpret_cast<unsigned char*>(b), Hash(25));
    SetDword(reinterpret_cast<unsigned char*>(c), Hash(26));
    SetDword(reinterpret_cast<unsigned char*>(d), Hash(27));
}
void __cdecl StubDepths4(void* prim) {
    Record(9, Packet(prim));
    auto* p = static_cast<unsigned char*>(prim);
    for (U z = 0x10; z <= 0x40; z += 0x10) SetDword(p + z, Hash(28 + z));
}
void __cdecl StubSetTexture(unsigned long texture, unsigned char* prim, int count) {
    Record(10, static_cast<U>(texture), Packet(prim), static_cast<U>(count), PrimHash(prim, 0x48));
    prim[0x16] ^= static_cast<unsigned char>(Hash(29));
    Disturb(30);
}
void __cdecl StubFlatOverlay(int faces, unsigned item) { Record(11, static_cast<U>(faces), item); }
void __cdecl StubApplyPatch(const unsigned char* entry) {
    Record(12, Addr(entry) - kHeader, Dword(g_record));
}
// Only the low 16 bits reach the real one (map_cells.cpp): the originals push
// ax with stale bits above.
unsigned char __cdecl StubTest(unsigned long code) {
    Record(13, static_cast<U>(code) & 0xFFFFu);
    Disturb(31);
    const U h = Hash(32);
    return static_cast<unsigned char>(PickH(h, {0, 0, 1, 1, 0x80, 0xFF, 0x7F, 2}));
}
long __cdecl StubRtp(const short* v, unsigned long* sxy, long* p) {
    Record(14, VertexWords(v), VertexZ(v), Addr(sxy));
    ScreenPoint(reinterpret_cast<float*>(sxy), 33);
    *p = static_cast<long>(Hash(34));
    const U h = Hash(35);
    const U depth = (h % 3 == 0) ? PickH(h >> 3, {0, 1, 0xFFFFFFFFu, 2, 0xFFFFFFFEu, 7, 100, 0x7FFFFFFFu, 0x80000000u,
                                                  0x10000, 0xFFFF0000u, 3, 1125, 0xFFFFFB9Bu})
                                 : (h >> 3) % 0x4000 + 1;
    return static_cast<long>(depth);
}
void __cdecl StubDepthFlat4(void* prim) {
    Record(15, Packet(prim));
    SetDword(static_cast<unsigned char*>(prim) + 0x10, Hash(36));
    Disturb(37);
}
long __cdecl StubClutAdjust(int c, int r, int red, int green, int blue) {
    Record(16, static_cast<U>(c), static_cast<U>(r), static_cast<U>(red), static_cast<U>(green), static_cast<U>(blue));
    return static_cast<long>(Hash(38));
}
// The menu draws. Menu_DrawBox reads the colour's byte (menu_windows.cpp);
// Text_DrawAt hands the colour to Text_DrawString, which reads its low byte;
// Text_DrawFont8 / 12 read 6 bits of theirs: so the byte is what is logged.
void __cdecl StubDrawBox(int x, int y, int w, int h, int flags, int colour) {
    Record(17, static_cast<U>(x), static_cast<U>(y), static_cast<U>(w) | static_cast<U>(h) << 16, static_cast<U>(flags),
           static_cast<U>(colour) & 0xFFu);
    Disturb(39);
}
void __cdecl StubItemIcon(int x, int y, int icon, int shade) {
    Record(18, static_cast<U>(x), static_cast<U>(y), static_cast<U>(icon), static_cast<U>(shade));
    Disturb(40);
}
const unsigned char* __cdecl StubTextAt(int x, int y, int colour, int count, const unsigned char* text) {
    Record(19, static_cast<U>(x), static_cast<U>(y), static_cast<U>(colour) & 0xFFu, static_cast<U>(count), Addr(text));
    Disturb(41);
    return At(Hash(42));
}
int __cdecl StubSprintf(char* buffer, const char* format, unsigned value) {
    Record(20, Addr(buffer), Addr(format), value);
    const U h = Hash(43);
    for (U i = 0; i < 7; ++i) buffer[i] = static_cast<char>(0x30 + ((h >> (i * 4)) & 0xF) + (value & 7));
    buffer[7] = 0;
    Disturb(44);
    return static_cast<int>(Hash(45));
}
void __cdecl StubFont8(int x, int y, int colour, const unsigned char* text) {
    Record(21, static_cast<U>(x), static_cast<U>(y), static_cast<U>(colour) & 0xFFu, Addr(text), Dword(text) ^ Dword(text + 4));
    Disturb(46);
}
const unsigned char* __cdecl StubTinyFont(int x, int y, unsigned colour, unsigned count, const unsigned char* text) {
    Record(22, static_cast<U>(x), static_cast<U>(y), colour, count, Addr(text));
    Disturb(47);
    return At(Hash(48));
}
// Its eax, which both callers hand on (one into the next x, one into the
// level): the high word 0 half the time.
U __cdecl StubPieces(int x, int y, const unsigned char* list, int flags) {
    Record(23, static_cast<U>(x), static_cast<U>(y), Addr(list), static_cast<U>(flags));
    Disturb(49);
    const U h = Hash(50);
    return (h % 2) ? (h & 0xFFFFu) : h;
}
U __cdecl StubExpBar(int x, int y, unsigned member, unsigned level, unsigned exp) {
    Record(24, static_cast<U>(x), static_cast<U>(y), member, level, exp);
    return Hash(51);
}
void __cdecl StubSetSprt8(unsigned char* prim) {
    Record(25, Packet(prim));
    const U h = Hash(52);
    for (U i = 4; i < 0x18; ++i) prim[i] = static_cast<unsigned char>(h >> (i % 24));
    prim[7] = 0x74;
}
void __cdecl StubSetSemi(unsigned char* prim, unsigned abe) {
    Record(26, Packet(prim), abe, PrimHash(prim, 0x18));
    prim[7] ^= 2;
}
void __cdecl StubFont12(int x, int y, int colour, const unsigned char* text) {
    Record(27, static_cast<U>(x), static_cast<U>(y), static_cast<U>(colour) & 0xFFu, Addr(text), Dword(text) ^ Dword(text + 4));
    Disturb(53);
}
// The skip of a nested control op: the position it is handed and one a few
// bytes on (the script is built so any landing is on a byte the switch skip
// handles, and it runs into F5s before its end).
const unsigned char* __cdecl StubSkipControl(const unsigned char* at) {
    Record(28, Rec(at), at[0]);
    return at + 1 + Hash(54) % 5;
}
void __cdecl StubObjReset() {
    Record(29, Word(At(kScratchBank)), Dword(At(kSpriteCurrent)));
    Disturb(55);
    const U h = Hash(56);
    if (h % 4 == 0) SetWord(At(kScratchBank), h >> 8);
}
U __cdecl StubSetBank(U bank) {
    Record(30, bank & 0xFFFFu, Dword(At(kSpriteCurrent)));
    Disturb(57);
    return Hash(58);
}
void __cdecl StubSetFlags(const unsigned char* flags) {
    Record(31, Rec(flags), Dword(At(kSpriteCurrent)));
    Disturb(59);
}
// Sprite_SetAnimation takes a byte; the original pushes ecx with
// Sprite_Current's upper bits above it.
void __cdecl StubSetAnimation(U animation) {
    Record(32, animation & 0xFFu, Dword(At(kSpriteCurrent)));
    Disturb(60);
}
// AreaMap_SetByte (ours) reads the low words of x and z, the value's byte.
U __cdecl StubSetByte(U x, U z, U value) {
    Record(33, x & 0xFFFFu, z & 0xFFFFu, value & 0xFFu, Dword(At(kSpriteCurrent)));
    Disturb(61);
    return Hash(62);
}

const Callees kStubs = {
    StubElevation, StubRtp3,     StubDepthF3,      StubSetPolyFT4, StubSetShadeTex, StubCommit,     StubRtp4,
    StubDepthF4,   StubDepths4,  StubSetTexture,   StubFlatOverlay, StubApplyPatch, StubTest,       StubRtp,
    StubDepthFlat4, StubClutAdjust, StubDrawBox,   StubItemIcon,   StubTextAt,      StubSprintf,    StubFont8,
    StubTinyFont,  StubPieces,   StubExpBar,       StubSetSprt8,   StubSetSemi,     StubFont12,     StubSkipControl,
    StubObjReset,  StubSetBank,  StubSetFlags,     StubSetAnimation, StubSetByte,
};

const void* StubFor(U target) {
    const auto f = [](auto p) { return reinterpret_cast<const void*>(p); };
    switch (target) {
    case 0x5720C0: return f(kStubs.elevation);
    case 0x5A84A0: return f(kStubs.rtp3);
    case 0x5A9130: return f(kStubs.depth_f3);
    case 0x5A75D0: return f(kStubs.set_poly_ft4);
    case 0x5A77A0: return f(kStubs.set_shade_tex);
    case 0x461E50: return f(kStubs.commit);
    case 0x5A85F0: return f(kStubs.rtp4);
    case 0x5A9170: return f(kStubs.depth_f4);
    case 0x5A9290: return f(kStubs.depths4);
    case 0x572A00: return f(kStubs.set_texture);
    case 0x570AB0: return f(kStubs.flat_overlay);
    case 0x571110: return f(kStubs.apply_patch);
    case 0x56FF00: return f(kStubs.test);
    case 0x5A8250: return f(kStubs.rtp);
    case 0x5A92E0: return f(kStubs.depth_flat4);
    case 0x5718F0: return f(kStubs.clut_adjust);
    case 0x57CF60: return f(kStubs.draw_box);
    case 0x573F30: return f(kStubs.item_icon);
    case 0x516B30: return f(kStubs.text_at);
    case 0x5B9380: return f(kStubs.sprintf);
    case 0x517090: return f(kStubs.font8);
    case 0x516E70: return f(kStubs.tiny_font);
    case 0x57D910: return f(kStubs.pieces);
    case 0x574530: return f(kStubs.exp_bar);
    case 0x5A7720: return f(kStubs.set_sprt8);
    case 0x5A7780: return f(kStubs.set_semi);
    case 0x516F60: return f(kStubs.font12);
    case 0x579AA0: return f(kStubs.skip_control);
    case 0x579E30: return f(kStubs.obj_reset);
    case 0x589590: return f(kStubs.set_bank);
    case 0x579DB0: return f(kStubs.set_flags);
    case 0x5891F0: return f(kStubs.set_animation);
    case 0x579F00: return f(kStubs.set_byte);
    default: bof3::Fatal("map_field_objects: no stand-in for a call to 0x%X", static_cast<unsigned>(target));
    }
}

// --- the copies ---------------------------------------------------------------------------
// Every E8 of each body, by capstone 2026-09-25 (docs/map_field_objects.md
// section 1); no body has a tail jmp out, every other jump stays inside.
struct Call {
    U offset, target;
};
constexpr Call kUprightCalls[] = {{0x5F, 0x5720C0}, {0xE8, 0x5A84A0}, {0xFC, 0x5A9130}, {0x19C, 0x5A75D0},
                                  {0x1A4, 0x5A77A0}, {0x23E, 0x461E50}, {0x2D3, 0x5A85F0}, {0x2E8, 0x5A9170}};
constexpr Call kWallCalls[] = {{0xBE, 0x5A75D0}, {0xC6, 0x5A77A0}, {0xF9, 0x5A85F0},
                               {0xFF, 0x5A9290}, {0x10C, 0x572A00}, {0x117, 0x461E50}};
constexpr Call kFlatCalls[] = {{0x9B, 0x570AB0}};
constexpr Call kPatchCalls[] = {{0x2D, 0x571110}};
constexpr Call kAnimatedCalls[] = {{0xC, 0x56FF00},   {0x79, 0x5A8250},  {0x126, 0x5A75D0}, {0x12E, 0x5A77A0},
                                   {0x134, 0x5A92E0}, {0x1DF, 0x572A00}, {0x1F7, 0x461E50}, {0x20B, 0x461E50}};
constexpr Call kTintCalls[] = {{0xC, 0x56FF00}, {0x4A, 0x5718F0}, {0x62, 0x5718F0}};
constexpr Call kMemberCalls[] = {{0x34, 0x57CF60},  {0x7B, 0x573F30},  {0x95, 0x516B30},  {0xAA, 0x5B9380},
                                 {0xC9, 0x517090},  {0x19F, 0x516E70}, {0x1FE, 0x5B9380}, {0x216, 0x517090},
                                 {0x24C, 0x5B9380}, {0x261, 0x517090}, {0x2AD, 0x5B9380}, {0x2C8, 0x517090},
                                 {0x2DE, 0x5B9380}, {0x2F2, 0x517090}, {0x304, 0x57D910}, {0x322, 0x574530}};
constexpr Call kCellCalls[] = {{0x8, 0x5A7720}, {0x63, 0x5A7780}, {0x6C, 0x461E50}};
constexpr Call kClockCalls[] = {{0x1F, 0x57CF60}, {0x39, 0x5B9380}, {0x4D, 0x516F60}, {0x65, 0x5B9380},
                                {0x79, 0x516F60}, {0x9E, 0x516E70}, {0xB4, 0x516E70}, {0xC5, 0x57D910},
                                {0xE1, 0x57D910}, {0xFC, 0x57D910}};
constexpr Call kSwitchCalls[] = {{0x2E, 0x579AA0}};
constexpr Call kOp8Calls[] = {{0x3F, 0x579E30},  {0x4B, 0x589590},  {0xDE, 0x5720C0},  {0x14D, 0x579DB0}, {0x1A6, 0x5891F0},
                              {0x1BC, 0x579F00}, {0x1DD, 0x579F00}, {0x1FE, 0x579F00}, {0x227, 0x579F00}};

void* Clone(const char* name, U base, U size, const Call* calls, int n) {
    bof3::CloneCall re_aimed[16];
    if (n > 16) bof3::Fatal("map_field_objects: %s has %d calls", name, n);
    for (int i = 0; i < n; ++i) re_aimed[i] = {calls[i].offset, StubFor(calls[i].target), calls[i].target};
    void* code = bof3::CloneOriginal(name, base, size, re_aimed, n);
    if (!code) bof3::Fatal("map_field_objects: CloneOriginal(%s) returned null", name);
    return code;
}
template <int N> void* Clone(const char* name, U base, U size, const Call (&calls)[N]) {
    return Clone(name, base, size, calls, N);
}

// --- the state and one round -------------------------------------------------------------
struct Region {
    U at, size;
};
constexpr U kStateMax = 0x10000;
unsigned char g_input[kStateMax], g_out[2][kStateMax];
Log g_logs[2];
U g_eax[2];

U Total(const Region* r, int n) {
    U total = 0;
    for (int i = 0; i < n; ++i) total += r[i].size;
    if (total > kStateMax) bof3::Fatal("map_field_objects self-test: a state of 0x%X bytes", static_cast<unsigned>(total));
    return total;
}
void Capture(const Region* r, int n, unsigned char* out) {
    for (int i = 0; i < n; out += r[i].size, ++i) std::memcpy(out, At(r[i].at), r[i].size);
}
void Apply(const Region* r, int n, const unsigned char* in) {
    for (int i = 0; i < n; in += r[i].size, ++i) std::memcpy(At(r[i].at), in, r[i].size);
}
void Randomize(const Region* r, int n) {
    for (int i = 0; i < n; ++i)
        for (U k = 0; k < r[i].size; ++k) At(r[i].at)[k] = static_cast<unsigned char>(Next());
}

struct Tally {
    const char* name;
    unsigned rounds, bad, calls;
    unsigned cover[4];
};

// Theirs then ours from the state as it stands, both under the game's control
// word; the log, eax (when `eax` is set) and every region compared.
template <class Theirs, class Ours>
void Pair(Tally& t, const Region* r, int n, bool eax, Theirs&& theirs, Ours&& ours) {
    const U total = Total(r, n);
    Capture(r, n, g_input);
    g_seed = Next();
    const unsigned short saved_word = GetControlWord();
    for (int pass = 0; pass < 2; ++pass) {
        Apply(r, n, g_input);
        std::memset(&g_log, 0, sizeof g_log);
        SetControlWord(kGameControlWord);
        g_eax[pass] = pass == 0 ? theirs() : ours();
        SetControlWord(saved_word);
        Capture(r, n, g_out[pass]);
        g_logs[pass] = g_log;
    }
    ++t.rounds;
    t.calls += g_logs[0].n;
    const bool log_differs = g_logs[0].n != g_logs[1].n || g_logs[0].hash != g_logs[1].hash;
    const bool eax_differs = eax && g_eax[0] != g_eax[1];
    const bool state_differs = std::memcmp(g_out[0], g_out[1], total) != 0;
    if (!log_differs && !eax_differs && !state_differs) return;
    if (++t.bad <= 6) {
        if (state_differs) {
            U at = 0;
            while (g_out[0][at] == g_out[1][at]) ++at;
            int region = 0;
            U base = 0;
            while (at >= base + r[region].size) base += r[region++].size;
            bof3::Log("shadow      map_field_objects %s MISMATCH: round %u, memory 0x%08X: %02X / %02X", t.name,
                      t.rounds - 1, static_cast<unsigned>(r[region].at + at - base), g_out[0][at], g_out[1][at]);
        } else if (log_differs) {
            U at = 0;
            const U kept = g_logs[0].n < kKeep ? g_logs[0].n : kKeep;
            while (at < kept && std::memcmp(g_logs[0].keep[at], g_logs[1].keep[at], sizeof g_logs[0].keep[0]) == 0) ++at;
            const U* a = g_logs[0].keep[at < kKeep ? at : 0];
            const U* b = g_logs[1].keep[at < kKeep ? at : 0];
            bof3::Log("shadow      map_field_objects %s MISMATCH: round %u, %u / %u calls, entry %u: %X(%X %X %X %X %X) / "
                      "%X(%X %X %X %X %X)",
                      t.name, t.rounds - 1, g_logs[0].n, g_logs[1].n, at, a[0], a[1], a[2], a[3], a[4], a[5], b[0], b[1],
                      b[2], b[3], b[4], b[5]);
        } else {
            bof3::Log("shadow      map_field_objects %s MISMATCH: round %u, eax %08X / %08X", t.name, t.rounds - 1,
                      static_cast<unsigned>(g_eax[0]), static_cast<unsigned>(g_eax[1]));
        }
    }
}

// True if a kept entry of the log is `what` with first argument `a`.
bool Logged(const Log& log, U what, U a) {
    for (U i = 0; i < log.n && i < kKeep; ++i)
        if (log.keep[i][0] == what && log.keep[i][1] == a) return true;
    return false;
}

// Common regions.
const Region kScratch = {kScratchX, 0x10};
const Region kVertices = {kVertex0, 0x20};
const Region kScreen = {kScreen0, 0x18};
Region PacketRegion() { return {Addr(g_packets), kPacketBytes}; }
Region RecordRegion() { return {Addr(g_record), sizeof g_record}; }
const Region kPacketNext = {0x7E0670, 4};
const Region kOtSlot = {0x92BF19, 1};
const Region kHeaderRegion = {kHeader, kHeaderBytes};
const Region kUprights = {0x66313C, kUprightTables};

void PlacePacket() { SetDword(At(at::PacketNextAt()), Addr(g_packets) + (OneIn(4) ? 4 * (Next() % 16) : 0)); }

// --- the map-cell handlers -------------------------------------------------------------------

// A record's top byte: one of the handler's own kinds most of the time.
U KindFor(std::initializer_list<U> kinds) { return OneIn(8) ? Next() & 0xFF : kinds.begin()[Next() % kinds.size()]; }
U CellByte() { return OneIn(8) ? Next() : Pick({0, 1, 0x7F, 0x80, 0x81, 0xFF, Next() & 0xFF, Next() & 0xFF}); }

void TestUprights(Tally& t, void* clone, unsigned rounds) {
    using Fn = void(__cdecl*)(const unsigned char*, unsigned, unsigned);
    const Fn theirs = reinterpret_cast<Fn>(clone);
    const Region r[] = {kScratch, kVertices, kScreen, PacketRegion(), RecordRegion(), kPacketNext, kOtSlot, kUprights};
    const int n = sizeof r / sizeof r[0];
    SetWatch({{kScratchX, 8}});
    g_lo_x = -80, g_hi_x = 400, g_lo_y = -20, g_hi_y = 260;
    static unsigned char real_tables[kUprightTables];
    std::memcpy(real_tables, At(0x66313C), kUprightTables);
    for (unsigned round = 0; round < rounds; ++round) {
        Randomize(r, n);
        // The real tables most rounds; random ones (counts kept small) the rest.
        if (!OneIn(3)) std::memcpy(At(0x66313C), real_tables, kUprightTables);
        else
            for (U k = 0; k < 12; ++k) At(0x66313C + k)[0] = static_cast<unsigned char>(Pick({0, 1, 2, 3, 4, 9}));
        const U kind = KindFor({1, 2, 3, 5, 6, 7, 8, 9, 0xA, 0xB, 0xC, 0x31, 0x32, 0x33, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3D, 0xD});
        SetDword(g_record, (kind << 24) | (Next() & 0xFFFFFFu));
        PlacePacket();
        const U b1 = CellByte(), b0 = CellByte();
        Pair(t, r, n, false, [&] { theirs(g_record, b1, b0); return 0u; },
             [&] { MapCell_DrawUprights(g_record, b1, b0); return 0u; });
        t.cover[0] += g_logs[0].counts[4] != 0;                 // drawn
        t.cover[1] += g_logs[0].counts[4] >= 4;                 // two pairs or more
        t.cover[2] += g_logs[0].counts[2] != 0 && g_logs[0].counts[4] == 0;   // culled or no count
    }
    std::memcpy(At(0x66313C), real_tables, kUprightTables);
}

void RandomCorners(U& b1, U& b0) {
    const U width = 1 + Next() % 32;
    At(kHeader)[0] = static_cast<unsigned char>(width);
    b1 = OneIn(6) ? Pick({0, 0x7F, 0x80, 0x81}) : Next() % 64;
    const U rows = (kHeaderBytes - 0x40 - 64 * 4) / (width * 4);
    b0 = Next() % (rows < 256 ? rows : 256);
}

void TestDiagonalWall(Tally& t, void* clone, unsigned rounds) {
    using Fn = void(__cdecl*)(const unsigned char*, unsigned, unsigned);
    const Fn theirs = reinterpret_cast<Fn>(clone);
    const Region r[] = {kHeaderRegion, PacketRegion(), RecordRegion(), kPacketNext, kOtSlot};
    const int n = sizeof r / sizeof r[0];
    SetWatch({});
    g_lo_x = -100, g_hi_x = 420, g_lo_y = -150, g_hi_y = 300;
    for (unsigned round = 0; round < rounds; ++round) {
        Randomize(r, n);
        U b1, b0;
        RandomCorners(b1, b0);
        SetDword(g_record, (KindFor({4, 0x34}) << 24) | (Next() & 0xFFFFFFu));
        PlacePacket();
        Pair(t, r, n, false, [&] { theirs(g_record, b1, b0); return 0u; },
             [&] { MapCell_DrawDiagonalWall(g_record, b1, b0); return 0u; });
        t.cover[0] += (Dword(g_record) >> 28) != 0;
    }
}

void TestFlatFaces(Tally& t, void* clone, unsigned rounds) {
    using Fn = void(__cdecl*)(const unsigned char*, unsigned, unsigned);
    const Fn theirs = reinterpret_cast<Fn>(clone);
    const Region r[] = {{at::OriginAt(), 4}, {at::RowAt(), 2}, {at::ColumnAt(), 2}, {0x937FA0, 56 * 28 * 4}, RecordRegion()};
    const int n = sizeof r / sizeof r[0];
    SetWatch({});
    for (unsigned round = 0; round < rounds; ++round) {
        Randomize(r, n);
        // Cell items mostly zero or small, so both outcomes of the item test show.
        for (U k = 0; k < 56 * 28; ++k)
            if (Next() % 3 == 0) SetWord(At(0x937FA2 + k * 4), Next() & 0xF000u);
        const auto ox = static_cast<std::int32_t>(Next() % 0x100) - 0x40, oz = static_cast<std::int32_t>(Next() % 0x100) - 0x40;
        SetWord(At(at::OriginAt()), static_cast<U>(ox));
        SetWord(At(at::OriginAt() + 2), static_cast<U>(oz));
        SetWord(At(at::RowAt()), static_cast<U>(static_cast<std::int32_t>(Next() % 0x3D) - 2));
        SetWord(At(at::ColumnAt()), static_cast<U>(static_cast<std::int32_t>(Next() % 0x21) - 2));
        // (x', z') near the diamond's edges: sum and difference at -1, 0, 0x37, 0x38.
        const std::int32_t s = static_cast<std::int32_t>(Pick({0xFFFFFFFFu, 0, 1, 0x36, 0x37, 0x38, Next() % 0x38, Next() % 0x38}));
        const std::int32_t d = static_cast<std::int32_t>(Pick({0xFFFFFFFFu, 0, 1, 0x36, 0x37, 0x38, Next() % 0x38, Next() % 0x38}));
        std::int32_t dx = (s + d) / 2, dz = s - dx;
        if (OneIn(4)) dx += 1;
        const U b1 = static_cast<U>(ox + dx), b0 = static_cast<U>(oz + dz);
        SetDword(g_record, (KindFor({0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x19, 0x1A, 0x1F}) << 24) | (Next() & 0xFFFFFFu));
        Pair(t, r, n, false, [&] { theirs(g_record, b1, b0); return 0u; },
             [&] { MapCell_DrawFlatFaces(g_record, b1, b0); return 0u; });
        t.cover[0] += g_logs[0].counts[11] != 0;
    }
}

void TestPatch(Tally& t, void* clone, unsigned rounds, void(__cdecl* ours)(unsigned char*, unsigned, unsigned)) {
    using Fn = void(__cdecl*)(unsigned char*, unsigned, unsigned);
    const Fn theirs = reinterpret_cast<Fn>(clone);
    const Region r[] = {{kPatchBase, 4}, RecordRegion()};
    const int n = sizeof r / sizeof r[0];
    SetWatch({});
    for (unsigned round = 0; round < rounds; ++round) {
        Randomize(r, n);
        if (OneIn(3)) SetDword(At(kPatchBase), Pick({0, 0xFFFF, 0x10000, 0xFFFFFFFFu}));
        if (OneIn(3)) SetDword(g_record, Pick({0x24000000u, 0x24FFFFFFu, 0x2500FFFFu, 0xFF000000u, 0x00FFFFFFu}));
        const U b1 = Next(), b0 = Next();
        Pair(t, r, n, false, [&] { theirs(g_record, b1, b0); return 0u; }, [&] { ours(g_record, b1, b0); return 0u; });
    }
}

void TestAnimated(Tally& t, void* clone, unsigned rounds) {
    using Fn = void(__cdecl*)(const unsigned char*, unsigned, unsigned);
    const Fn theirs = reinterpret_cast<Fn>(clone);
    const Region r[] = {kVertices, kScreen, PacketRegion(), RecordRegion(), kPacketNext, kOtSlot, {at::FrameCounterAt(), 4}};
    const int n = sizeof r / sizeof r[0];
    // The period byte +8 is never disturbed (a 0 divides by zero on both sides).
    SetWatch({{kScreen0, 8}, {Addr(g_record) + 4, 4}, {Addr(g_record) + 9, 1}, {Addr(g_record) + 0x18, 0x158}});
    g_lo_x = -60, g_hi_x = 380, g_lo_y = -150, g_hi_y = 300;
    for (unsigned round = 0; round < rounds; ++round) {
        Randomize(r, n);
        const U period = OneIn(3) ? Pick({1, 2, 3, 0xFE, 0xFF}) : 1 + Next() % 0xFF;
        g_record[8] = static_cast<unsigned char>(period);
        g_record[9] = static_cast<unsigned char>(OneIn(3) ? Pick({0, 3, 4, 0xFB, 0xFC, 0xFF}) : Next());
        // Thresholds ascending (or not), one of 0xFF within reach, which no
        // frame reaches (t < period <= 0xFF).
        const U stop = Next() % 7;
        for (U k = 0; k < stop; ++k) g_record[0xA + k] = static_cast<unsigned char>(OneIn(2) ? Next() % (period + 1) : Next());
        g_record[0xA + stop] = 0xFF;
        if (OneIn(4)) SetDword(At(at::FrameCounterAt()), Pick({0, period - 1, period, period + 1, 0xFFFFFFFFu}));
        PlacePacket();
        const U b1 = CellByte(), b0 = CellByte();
        Pair(t, r, n, false, [&] { theirs(g_record, b1, b0); return 0u; },
             [&] { MapCell_DrawAnimated(g_record, b1, b0); return 0u; });
        t.cover[0] += g_logs[0].counts[14] != 0;   // the test passed
        t.cover[1] += g_logs[0].counts[10] != 0;   // drawn
        t.cover[2] += Logged(g_logs[0], 6, 6);     // committed to slot 6
    }
}

// --- the set-up entries ----------------------------------------------------------------------

void TestTint(Tally& t, void* clone, unsigned rounds) {
    using Fn = void(__cdecl*)(const unsigned char*);
    const Fn theirs = reinterpret_cast<Fn>(clone);
    const Region r[] = {RecordRegion()};
    SetWatch({{Addr(g_record), 12}});
    for (unsigned round = 0; round < rounds; ++round) {
        Randomize(r, 1);
        Pair(t, r, 1, false, [&] { theirs(g_record); return 0u; }, [&] { AreaMap_SetupTint(g_record); return 0u; });
        t.cover[0] += g_logs[0].counts[16] != 0 && g_logs[0].keep[1][3] != 0;
    }
}

void TestFlatColours(Tally& t, void* clone, unsigned rounds) {
    using Fn = void(__cdecl*)(const unsigned char*);
    const Fn theirs = reinterpret_cast<Fn>(clone);
    const Region r[] = {RecordRegion(), {at::CondByteFFAt(), 1}, {kFlatColour0, 4}};
    const int n = sizeof r / sizeof r[0];
    for (unsigned round = 0; round < rounds; ++round) {
        Randomize(r, n);
        At(at::CondByteFFAt())[0] = static_cast<unsigned char>(Pick({0, 0, 1, 0x80, 0xFF, Next()}));
        Pair(t, r, n, false, [&] { theirs(g_record); return 0u; }, [&] { AreaMap_SetupFlatColours(g_record); return 0u; });
        t.cover[0] += At(at::CondByteFFAt())[0] != 0;
    }
}

void TestSetupTexture(Tally& t, void* clone, unsigned rounds) {
    using Fn = void(__cdecl*)(const unsigned char*);
    const Fn theirs = reinterpret_cast<Fn>(clone);
    const Region r[] = {RecordRegion(), {at::CondByteFFAt(), 1}, kHeaderRegion};
    const int n = sizeof r / sizeof r[0];
    for (unsigned round = 0; round < rounds; ++round) {
        Randomize(r, n);
        At(at::CondByteFFAt())[0] = static_cast<unsigned char>(Pick({0, 0, 1, 0x80, 0xFF, Next()}));
        const U width = 1 + Next() % 32, height = Next() % 33, offset = Next() % 0x101;
        At(kHeader)[0] = static_cast<unsigned char>(width);
        At(kHeader)[1] = static_cast<unsigned char>(height);
        SetWord(At(kHeaderOffset), offset);
        const U y = Next() % 32, x = Next() % 32;
        g_record[0] = static_cast<unsigned char>(y);
        g_record[1] = static_cast<unsigned char>(x);
        const U tile_at = kHeader + (y * width + x + offset * 2) * 2;
        if (tile_at < kHeader + 0x40) continue;   // the tile would overlap the header's own bytes: redraw
        SetWord(At(tile_at), OneIn(4) ? Pick({0, 1, 0xFF, 0x100}) : Next() % 0x101);
        Pair(t, r, n, false, [&] { theirs(g_record); return 0u; }, [&] { AreaMap_SetupTexture(g_record); return 0u; });
        t.cover[0] += At(at::CondByteFFAt())[0] != 0;
    }
}

// --- the menu draws ---------------------------------------------------------------------------

int SomeCoordinate() { return static_cast<int>(OneIn(4) ? Next() : Next() % 0x200 - 0x40); }

void TestMemberStatus(Tally& t, void* clone, unsigned rounds) {
    using Fn = U(__cdecl*)(int, int, unsigned, unsigned);
    const Fn theirs = reinterpret_cast<Fn>(clone);
    const Region r[] = {{kCharRecords, kMembersBytes}, {kPrintBuf, 0x20}, {kWindowColour, 1}, {at::FrameCounterAt(), 4}};
    const int n = sizeof r / sizeof r[0];
    for (unsigned round = 0; round < rounds; ++round) {
        Randomize(r, n);
        const U member = (OneIn(4) ? Next() & 0xFFFFFF00u : 0) | (OneIn(6) ? 8 + Next() % 8 : Next() % 8);
        unsigned char* const rec = At(kCharRecords + (member & 0xFF) * kCharStride);
        // HP and AP on the quarter tests' edges.
        const U max_hp = Pick({0, 1, 3, 4, 5, 8, 9, 100, 999, 0xFFFF, Next() & 0xFFFF});
        const U hp = Pick({0, 1, 2, max_hp / 4, max_hp / 4 - 1, max_hp / 4 + 1, max_hp, Next() & 0xFFFF});
        SetWord(rec + 0x20, max_hp);
        SetWord(rec + 0x18, hp);
        const U max_ap = Pick({0, 1, 3, 4, 5, 8, 99, 0xFFFF, Next() & 0xFFFF});
        const U ap = Pick({0, 1, max_ap / 4, max_ap / 4 - 1, max_ap / 4 + 1, max_ap, Next() & 0xFFFF});
        SetWord(rec + 0x22, max_ap);
        SetWord(rec + 0x1A, ap);
        if (OneIn(2)) rec[0x1E] = 0;
        const U highlight = OneIn(2) ? (Next() & 0xFFFFFF00u) : Next();
        SetWatch({{Addr(rec) + 9, 0x1A}, {at::FrameCounterAt(), 1}, {kWindowColour, 1}});
        const int x = SomeCoordinate(), y = SomeCoordinate();
        Pair(t, r, n, true, [&] { return theirs(x, y, member, highlight); },
             [&] { return static_cast<U>(Menu_DrawMemberStatus(x, y, member, highlight)); });
        t.cover[0] += (highlight & 0xFF) == 0;
        t.cover[1] += g_logs[0].counts[22] != 0;   // a status word
        t.cover[2] += (Word(rec + 0x10) & 0x2000u) != 0;
    }
}

void TestCell8(Tally& t, void* clone, unsigned rounds) {
    using Fn = U(__cdecl*)(unsigned, unsigned, unsigned, unsigned, unsigned, unsigned);
    const Fn theirs = reinterpret_cast<Fn>(clone);
    const Region r[] = {PacketRegion(), kPacketNext};
    const int n = sizeof r / sizeof r[0];
    SetWatch({});
    for (unsigned round = 0; round < rounds; ++round) {
        Randomize(r, n);
        PlacePacket();
        const U x = OneIn(3) ? Pick({0, 0xFFFF, 0x10000, 0x8000, 0xFFFFFFFFu}) : Next(), y = Next();
        const U u = Next(), v = Next(), clut = Next(), shade = Next();
        Pair(t, r, n, true, [&] { return theirs(x, y, u, v, clut, shade); },
             [&] { return static_cast<U>(Menu_DrawCell8(x, y, u, v, clut, shade)); });
    }
}

void TestPlayTime(Tally& t, void* clone, unsigned rounds) {
    using Fn = U(__cdecl*)(int, int);
    const Fn theirs = reinterpret_cast<Fn>(clone);
    const Region r[] = {{kClockHours, 4}, {kPrintBuf, 0x20}, {kWindowColour, 1}};
    const int n = sizeof r / sizeof r[0];
    SetWatch({{kClockHours, 4}, {kWindowColour, 1}});
    for (unsigned round = 0; round < rounds; ++round) {
        Randomize(r, n);
        At(kClockFrames)[0] = static_cast<unsigned char>(Pick({0, 14, 15, 16, 29, 0x80, 0xFF, Next()}));
        const int x = SomeCoordinate(), y = SomeCoordinate();
        Pair(t, r, n, true, [&] { return theirs(x, y); }, [&] { return static_cast<U>(Menu_DrawPlayTime(x, y)); });
        t.cover[0] += g_logs[0].counts[22] != 0;   // the colon drawn
    }
}

// --- the event script ---------------------------------------------------------------------------

// A switch body being skipped: ops below F0 and the control bytes the skip
// handles, never F2, F3 or FB..FF (which the original re-reads for ever);
// F5s more and more often, and a run of F5s at the end.
void RandomScript() {
    for (U i = 0; i < sizeof g_record; ++i) {
        U b;
        const U roll = Next() % 16;
        if (roll < 9) b = Next() % 0xF0;
        else if (roll < 13) b = Pick({0xF0, 0xF1, 0xF4, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA});
        else b = (i > 0x20 || OneIn(3)) ? 0xF5 : Next() % 0xF0;
        g_record[i] = static_cast<unsigned char>(b);
    }
    for (U i = sizeof g_record - 0x28; i < sizeof g_record; ++i) g_record[i] = 0xF5;
}

void TestSkipSwitch(Tally& t, void* clone, unsigned rounds) {
    using Fn = const unsigned char*(__cdecl*)(const unsigned char*);
    const Fn theirs = reinterpret_cast<Fn>(clone);
    const Region r[] = {RecordRegion()};
    for (unsigned round = 0; round < rounds; ++round) {
        RandomScript();
        Pair(t, r, 1, true, [&] { return Addr(theirs(g_record)); }, [&] { return Addr(EventScript_SkipSwitch(g_record)); });
        t.cover[0] += g_logs[0].counts[28] != 0;
    }
}

void TestSetByte(Tally& t, void* clone, unsigned rounds) {
    using Fn = U(__cdecl*)(U, U, U);
    const Fn theirs = reinterpret_cast<Fn>(clone);
    const Region r[] = {{Addr(g_bytes), sizeof g_bytes}, {kHeader, 4}, {at::BytesAt(), 4}};
    const int n = sizeof r / sizeof r[0];
    for (unsigned round = 0; round < rounds; ++round) {
        Randomize(r, n);
        const U width = OneIn(8) ? 0 : 1 + Next() % 32;
        At(kHeader)[0] = static_cast<unsigned char>(width);
        SetDword(At(at::BytesAt()), Addr(g_bytes) + 0x200);
        const U z = (Next() & 0xFFFF0000u) | static_cast<std::uint16_t>(static_cast<std::int32_t>(Next() % 0x44) - 2);
        const U x = (Next() & 0xFFFF0000u) | static_cast<std::uint16_t>(static_cast<std::int32_t>(Next() % 0x108) - 8);
        const U v = Next();
        Pair(t, r, n, true, [&] { return theirs(x, z, v); }, [&] { return static_cast<U>(AreaMap_SetByte(x, z, v)); });
    }
}

void TestOp8x(Tally& t, void* clone, unsigned rounds) {
    using Fn = void(__cdecl*)(const unsigned char*);
    const Fn theirs = reinterpret_cast<Fn>(clone);
    const Region r[] = {RecordRegion(), {kObjectsFrom, kObjectsBytes}, kScratch, {kSpriteCurrent, 4}, {kActiveMember, 4}};
    const int n = sizeof r / sizeof r[0];
    SetWatch({{kScratchBank, 2}, {kObjectsFrom, kObjectsBytes}});
    g_move_sprite = true;
    for (unsigned round = 0; round < rounds; ++round) {
        Randomize(r, n);
        const U index = OneIn(4) ? Pick({0x1D, 0x1E, 0x1F, 0x7FFF, 0xFFFF, 0xFFFD, 0}) : Next() % 0x1E;
        SetWord(At(kScratchX), index);
        SetDword(At(kSpriteCurrent), SomeObject(Next()));
        for (U k = 4; k <= 7; ++k)
            if (OneIn(2)) g_record[k] = 0;
        Pair(t, r, n, false, [&] { theirs(g_record); return 0u; }, [&] { EventOp_8x(g_record); return 0u; });
        t.cover[0] += g_logs[0].counts[29] != 0;    // placed
        t.cover[1] += g_logs[0].counts[33] == 4;    // all four cells stamped
    }
    g_move_sprite = false;
}

void TestClear40(Tally& t, void* clone, unsigned rounds) {
    const Region r[] = {{at::StatusBitsAt(), 1}, {at::ScriptFlagsAt(), 2}};
    const int n = sizeof r / sizeof r[0];
    for (unsigned round = 0; round < rounds; ++round) {
        Randomize(r, n);
        const U eax = Next();
        Pair(t, r, n, true, [&] { return MapFieldObjects_CallEax(clone, eax); },
             [&] { return MapFieldObjects_CallEax(reinterpret_cast<const void*>(&ScriptFlags_Clear40), eax); });
    }
}

}  // namespace

void SelfTest() {
    // The copies, before MapFieldObjects_Inject patches anything.
    void* const uprights = Clone("MapCell_DrawUprights", 0x570210, 0x311, kUprightCalls);
    void* const wall = Clone("MapCell_DrawDiagonalWall", 0x570530, 0x125, kWallCalls);
    void* const flat = Clone("MapCell_DrawFlatFaces", 0x570A00, 0xA6, kFlatCalls);
    void* const step = Clone("MapCell_PatchThenStep", 0x571090, 0x34, kPatchCalls);
    void* const stop = Clone("MapCell_PatchThenStop", 0x5710D0, 0x34, kPatchCalls);
    void* const animated = Clone("MapCell_DrawAnimated", 0x5712E0, 0x218, kAnimatedCalls);
    void* const tint = Clone("AreaMap_SetupTint", 0x571880, 0x6F, kTintCalls);
    void* const colours = Clone("AreaMap_SetupFlatColours", 0x571A30, 0x3F, nullptr, 0);
    void* const texture = Clone("AreaMap_SetupTexture", 0x571A70, 0x71, nullptr, 0);
    void* const member = Clone("Menu_DrawMemberStatus", 0x573560, 0x332, kMemberCalls);
    void* const cell = Clone("Menu_DrawCell8", 0x5744B0, 0x76, kCellCalls);
    void* const clock = Clone("Menu_DrawPlayTime", 0x5746C0, 0x108, kClockCalls);
    void* const skip = Clone("EventScript_SkipSwitch", 0x579CF0, 0x7C, kSwitchCalls);
    move_script::Relocate(skip, 0x579CF0, 0x7C, {0x26, 0x50, 11});
    void* const set_byte = Clone("AreaMap_SetByte", 0x579F00, 0x27, nullptr, 0);
    void* const op8 = Clone("EventOp_8x", 0x57A3A0, 0x239, kOp8Calls);
    void* const clear40 = Clone("ScriptFlags_Clear40", 0x57C7A0, 0x16, nullptr, 0);

    // Everything any test touches, saved once and put back at the end.
    const Region all[] = {
        kScratch, kVertices, kScreen, kPacketNext, kOtSlot, kUprights, kHeaderRegion,
        {at::OriginAt(), 4}, {at::RowAt(), 2}, {at::ColumnAt(), 2}, {0x937FA0, 56 * 28 * 4},
        {at::FrameCounterAt(), 4}, {at::CondByteFFAt(), 1}, {kCharRecords, kMembersBytes}, {kPrintBuf, 0x20},
        {kWindowColour, 1}, {kClockHours, 4}, {at::BytesAt(), 4}, {kObjectsFrom, kObjectsBytes},
        {kSpriteCurrent, 4}, {kActiveMember, 4}, {at::StatusBitsAt(), 1}, {at::ScriptFlagsAt(), 2},
    };
    const int n_all = sizeof all / sizeof all[0];
    static unsigned char saved[kStateMax * 2];
    U total = 0;
    for (const Region& r : all) total += r.size;
    if (total > sizeof saved) bof3::Fatal("map_field_objects: the saved regions outgrow the buffer");
    Capture(all, n_all, saved);
    const Callees saved_callees = g;
    g = kStubs;
    g_rng = 0x44440DD1u;

    Tally tallies[] = {
        {"MapCell_DrawUprights", 0, 0, 0, {}},     {"MapCell_DrawDiagonalWall", 0, 0, 0, {}},
        {"MapCell_DrawFlatFaces", 0, 0, 0, {}},    {"MapCell_PatchThenStep", 0, 0, 0, {}},
        {"MapCell_PatchThenStop", 0, 0, 0, {}},    {"MapCell_DrawAnimated", 0, 0, 0, {}},
        {"AreaMap_SetupTint", 0, 0, 0, {}},        {"AreaMap_SetupFlatColours", 0, 0, 0, {}},
        {"AreaMap_SetupTexture", 0, 0, 0, {}},     {"Menu_DrawMemberStatus", 0, 0, 0, {}},
        {"Menu_DrawCell8", 0, 0, 0, {}},           {"Menu_DrawPlayTime", 0, 0, 0, {}},
        {"EventScript_SkipSwitch", 0, 0, 0, {}},   {"AreaMap_SetByte", 0, 0, 0, {}},
        {"EventOp_8x", 0, 0, 0, {}},               {"ScriptFlags_Clear40", 0, 0, 0, {}},
    };
    TestUprights(tallies[0], uprights, 8000);
    TestDiagonalWall(tallies[1], wall, 5000);
    TestFlatFaces(tallies[2], flat, 3000);
    TestPatch(tallies[3], step, 3000, MapCell_PatchThenStep);
    TestPatch(tallies[4], stop, 3000, MapCell_PatchThenStop);
    TestAnimated(tallies[5], animated, 20000);
    TestTint(tallies[6], tint, 5000);
    TestFlatColours(tallies[7], colours, 3000);
    TestSetupTexture(tallies[8], texture, 5000);
    TestMemberStatus(tallies[9], member, 10000);
    TestCell8(tallies[10], cell, 3000);
    TestPlayTime(tallies[11], clock, 5000);
    TestSkipSwitch(tallies[12], skip, 5000);
    TestSetByte(tallies[13], set_byte, 5000);
    TestOp8x(tallies[14], op8, 10000);
    TestClear40(tallies[15], clear40, 2000);

    g = saved_callees;
    Apply(all, n_all, saved);
    SetWatch({});

    unsigned bad = 0, rounds = 0, calls = 0;
    for (const Tally& t : tallies) {
        bad += t.bad;
        rounds += t.rounds;
        calls += t.calls;
        bof3::Log("shadow      map_field_objects self-test: %s %u rounds, %u calls out, covered %u %u %u, %u MISMATCHES",
                  t.name, t.rounds, t.calls, t.cover[0], t.cover[1], t.cover[2], t.bad);
    }
    bof3::Log("shadow      map_field_objects self-test: %u rounds over 16 functions, %u calls to the stand-ins, %u MISMATCHES",
              rounds, calls, bad);
    if (bad) bof3::Fatal("group DD's functions differ from the original in %u of %u self-test rounds", bad, rounds);
}

}  // namespace map_field_objects

// BOF3X_SHADOW=battle_items: a differential fuzz of the battle's item and
// ability effects, the battle actor's helpers, four primitive setters and
// the SND stream's play and stop, once at start-up. docs/battle_items.md
// section 6.
//
// Twenty-four byte-copies, every call out re-aimed at a recording stand-in,
// Sparkle_Types' first four entries swapped for recorders and the SND
// stream's buffer a fake IDirectSoundBuffer whose methods record. One round:
// one function, random bytes in every region any of them touches (the
// sparkle pool, the scratch words, two sprite records, two actor records,
// the party and enemy records, a primitive buffer, the constant tables the
// sparkles read), the pointers among them put back inside the regions, then
// that function's branch boundaries seeded; theirs, then from the same state
// ours; the regions, the answer (at the width the original defines) and the
// stand-ins' log compared.
//
// The stand-ins are as loud as the real callees where the caller reads after
// the call, and louder: any call may move the sparkle being updated, the
// effect's actor, Sprite_Current, Gfx_PacketNext, the stream's buffer, a
// scratch word or a field of the records the callers read again - so a
// value ours keeps in a register where the original reads memory again (or
// the other way round) shows.
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <utility>

#include "bof3/symbols.gen.h"
#include "game/battle_items_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace battle_items {
namespace {

using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

std::uint32_t Address(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }

// --- the random source and the stand-ins' log ------------------------------

std::uint32_t g_rng = 0x6A09E667u;
std::uint32_t Next() { g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5; return g_rng; }
bool Often() { return Next() % 3 != 0; }
bool Half() { return (Next() & 1) != 0; }

constexpr unsigned kLog = 640;   // MagicFx_DrawRing makes 5 + 32 x 16 records
struct Entry { std::uint32_t what, a, b, c, d, e; };
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
            std::uint32_t e = 0) {
    if (g_log_n < kLog) g_log[g_log_n] = {what, a, b, c, d, e};
    ++g_log_n;
}

// --- the fuzz's own memory -------------------------------------------------

constexpr unsigned kPrimBytes = 0x2000;
constexpr unsigned kRecordBytes = 0x140;
alignas(16) unsigned char g_prims[kPrimBytes];
alignas(16) unsigned char g_sprites[2][kRecordBytes];
alignas(16) unsigned char g_actors[2][kRecordBytes];
alignas(16) unsigned char g_cues[0x40];
alignas(16) unsigned char g_wave[0x40];

// Two fake IDirectSoundBuffers: a vtable pointer each, then nothing. A
// stand-in may swap one for the other, never for null - the original reads
// the cell again and calls through it.
struct Fake { const void* const* vtable; };
Fake g_fakes[2];
Fake& g_fake = g_fakes[0];

unsigned char* PrimAt(unsigned k) { return g_prims + 4 * (k % 32); }
void SetPointer(std::uint32_t cell, const void* p) { SetLong(At(cell), static_cast<std::int32_t>(Address(p))); }
unsigned char* Pointer(std::uint32_t cell) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(Long(At(cell)))); }
unsigned char* Sparkle(unsigned k) { return At(at::kSparklePool + (k % 4) * at::kSparkleStride); }

// Every cell below is one some function reads again after a call.
void Disturb() {
    const std::uint32_t h = Hash();
    if (h % 3 == 0) return;
    const unsigned v = (h >> 12) & 0xFF;
    switch ((h >> 4) % 13) {
    case 0: SetPointer(at::kSparkleCurrent, Sparkle(v)); break;
    case 1: SetPointer(at::kFxActor, g_actors[v & 1]); break;
    case 2: Sprite_Current = g_sprites[v & 1]; break;
    case 3: Gfx_PacketNext = PrimAt(v); break;
    case 4: SetPointer(at::kStreamBuffer, &g_fakes[v & 1]); break;
    case 5: SetWord(At(at::kScratch + 2 * (v % 8)), h >> 16); break;
    case 6: SetWord(At(at::kVertex + 2 * (v % 16)), h >> 16); break;
    case 7: {
        static const unsigned kFields[] = {5, 6, 7, 3, 4, 8, 9, 0xA, 0xC, 0x20, 0x21, 0x22, 0x14, 0x18};
        Pointer(at::kSparkleCurrent)[kFields[v % 14]] = static_cast<unsigned char>(h >> 20);
        break;
    }
    case 8: {
        static const unsigned kFields[] = {9, 0xA, 0xB, 0x2E, 0x2F, 0x30, 0x31, 0x34, 0x35, 0x38, 0x3A};
        Sprite_Current[kFields[v % 11]] = static_cast<unsigned char>(h >> 20);
        break;
    }
    case 9: Frame_Counter = h >> 8; break;
    case 10: Pointer(at::kFxActor)[0xB] = static_cast<unsigned char>(h >> 20); break;
    default: break;
    }
}

// --- the stand-ins ---------------------------------------------------------

// A value in the range the real sine / cosine answer, a quarter of the time
// anything.
int Trig() {
    const std::uint32_t h = Hash();
    if (h % 4 == 0) return static_cast<int>(h);
    return static_cast<int>((h >> 8) % 8193) - 4096;
}
// A float's bits for a projected coordinate: ordinary, fractional negative,
// huge, a NaN, an infinity.
std::uint32_t FloatBitsFor(std::uint32_t h) {
    switch (h % 8) {
    case 0: return 0x7FC00000u;                       // NaN
    case 1: return 0xFF800000u;                       // -inf
    case 2: return 0x5F000000u + (h >> 12 & 0xFFFF);  // around 2^63
    case 3: return 0xBF7FFFFFu;                       // -0.99999994
    default: {
        const float f = static_cast<float>(static_cast<int>(h >> 8) % 100000) / 7.0f;
        std::uint32_t bits;
        std::memcpy(&bits, &f, sizeof bits);
        return bits;
    }
    }
}
void PutFloatBits(void* at) {
    const std::uint32_t bits = FloatBitsFor(Hash());
    std::memcpy(at, &bits, sizeof bits);
}
std::uint32_t Shorts3(const short* v) {
    return (static_cast<std::uint32_t>(static_cast<std::uint16_t>(v[0])) * 0x9E3779B1u) ^
           (static_cast<std::uint32_t>(static_cast<std::uint16_t>(v[1])) << 16) ^ static_cast<std::uint16_t>(v[2]);
}
std::uint32_t Shorts9(const short* m) {
    std::uint32_t h = 0;
    for (int i = 0; i < 9; ++i) h = h * 0x01000193u ^ static_cast<std::uint16_t>(m[i]);
    return h;
}

void __cdecl StubDrawMode(unsigned char* prim, int dfe, int dtd, unsigned tpage, unsigned long tw) {
    Record(1, Address(prim), static_cast<std::uint32_t>(dfe), static_cast<std::uint32_t>(dtd), tpage, tw);
    if (Hash() % 2) SetLong(prim + 4, static_cast<std::int32_t>(0xE8000000u | tpage));
    Disturb();
}
void Advance(unsigned size) {
    if (Hash() % 2 && Gfx_PacketNext + (size & 0xFF) < g_prims + kPrimBytes / 2) Gfx_PacketNext += size & 0xFF;
}
void __cdecl StubCommit(unsigned slot, unsigned size) {
    Record(2, slot, size);
    Advance(size);
    Disturb();
}
void __cdecl StubCommitSorted(unsigned x, unsigned z, unsigned slot, unsigned size) {
    Record(3, x, z, slot, size);
    Advance(size);
    Disturb();
}
int __cdecl StubSin(int a) {
    Record(4, static_cast<std::uint32_t>(a));
    Disturb();
    return Trig();
}
int __cdecl StubCos(int a) {
    Record(5, static_cast<std::uint32_t>(a));
    Disturb();
    return Trig();
}
template <unsigned Code> void __cdecl StubSetPrim(unsigned char* prim) {
    Record(6, Code, Address(prim));
    prim[7] = static_cast<unsigned char>(Code);
    Disturb();
}
void __cdecl StubSemi(unsigned char* prim, unsigned abe) {
    Record(7, Address(prim), abe);
    Disturb();
}
unsigned __cdecl StubTPage(unsigned tp, unsigned abr, int x, int y) {
    Record(8, tp, abr, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y));
    Disturb();
    return Hash();
}
unsigned __cdecl StubClut(int x, int y) {
    Record(9, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y));
    Disturb();
    return Hash();
}
long __cdecl StubRtp(const short* v, float* sxy, long* depth, long*) {
    Record(10, Shorts3(v), Address(sxy));
    PutFloatBits(sxy);
    PutFloatBits(sxy + 1);
    *depth = static_cast<long>(Hash());
    Disturb();
    return static_cast<long>(Hash());
}
long __cdecl StubRtp3(const short* v0, const short* v1, const short* v2, float* s0, float* s1, float* s2, long* depth,
                      long*) {
    Record(11, Address(v0), Address(v1), Address(v2), Address(s0), Address(s1) ^ (Address(s2) << 8));
    Record(12, Shorts3(v0), Shorts3(v1), Shorts3(v2));
    for (float* s : {s0, s1, s2}) {
        PutFloatBits(s);
        PutFloatBits(s + 1);
    }
    *depth = static_cast<long>(Hash());
    Disturb();
    return static_cast<long>(Hash());
}
long __cdecl StubRtp4(const short* v0, const short* v1, const short* v2, const short* v3, float* s0, float* s1,
                      float* s2, float* s3, long* depth, long*) {
    Record(13, Address(v0), Address(v1), Address(v2), Address(v3));
    Record(14, Address(s0), Address(s1), Address(s2), Address(s3));
    Record(15, Shorts3(v0), Shorts3(v1), Shorts3(v2), Shorts3(v3));
    for (float* s : {s0, s1, s2, s3}) {
        PutFloatBits(s);
        PutFloatBits(s + 1);
    }
    *depth = static_cast<long>(Hash());
    Disturb();
    return static_cast<long>(Hash());
}
void __cdecl StubDepths3(unsigned char* prim) {
    Record(16, Address(prim));
    for (unsigned k = 0x10; k <= 0x30; k += 0x10) if (Hash() % 2) PutFloatBits(prim + k);
    Disturb();
}
void __cdecl StubDepths4(unsigned char* prim) {
    Record(17, Address(prim));
    for (unsigned k = 0x14; k <= 0x50; k += 0x14) if (Hash() % 2) PutFloatBits(prim + k);
    Disturb();
}
void __cdecl StubStoreDepth(float* out) {
    Record(18, Address(out));
    PutFloatBits(out);
    Disturb();
}

// The matrix calls work on the caller's stack: what is recorded is the
// content, and where each block lies relative to the one before - the
// original's MATRIX layout, the translation RotTrans fills at +0x14 of the
// block RotMatrix and MulMatrix0 then fill, has to be ours too.
const unsigned char* g_trans;
const unsigned char* g_matrix;
void __cdecl StubPushMatrix() {
    Record(19);
    Disturb();
}
void __cdecl StubRotTrans(const short* v, long* t, long*) {
    Record(20, Shorts3(v));
    g_trans = reinterpret_cast<const unsigned char*>(t);
    for (int i = 0; i < 3; ++i) t[i] = static_cast<long>(Hash() + static_cast<std::uint32_t>(i));
    Disturb();
}
short* __cdecl StubRotMatrix(const short* angles, short* m) {
    g_matrix = reinterpret_cast<const unsigned char*>(m);
    Record(21, Shorts3(angles), Address(g_trans) - Address(m));
    for (int i = 0; i < 9; ++i) m[i] = static_cast<short>(Hash() >> i);
    Disturb();
    return m;
}
short* __cdecl StubMulMatrix0(const short* a, const short* b, short* out) {
    Record(22, Address(a), Address(b) - Address(g_matrix), Address(out) - Address(g_matrix), Shorts9(b));
    for (int i = 0; i < 9; ++i) out[i] = static_cast<short>(Hash() >> (i + 3));
    Disturb();
    return out;
}
void __cdecl StubSetRot(const unsigned long* m) {
    Record(23, Address(m) - Address(g_matrix), Shorts9(reinterpret_cast<const short*>(m)));
    Disturb();
}
void __cdecl StubSetTrans(const unsigned long* m) {
    Record(24, Address(m) - Address(g_matrix), m[5], m[6], m[7]);
    Disturb();
}
// Rand: some the CRT's rand never answers (negative), so that a sign
// mistake in a mask shows.
int __cdecl StubRand() {
    Record(25);
    Disturb();
    const std::uint32_t h = Hash();
    return h % 4 == 0 ? static_cast<int>(h) : static_cast<int>(h >> 1) & 0x7FFF;
}
void __cdecl StubSetAnimation(unsigned v) {
    Record(26, v, Address(Sprite_Current));
    Disturb();
}
// 0x435A20 reads the index's low byte only, and passes the second dword on.
void __cdecl StubEnemyAnimation(unsigned index, unsigned arg) {
    Record(27, index & 0xFF, arg, Address(Sprite_Current));
    Disturb();
}
unsigned char __cdecl StubIsOut(unsigned index) {
    Record(28, index);
    Disturb();
    const std::uint32_t h = Hash();
    return static_cast<unsigned char>(h % 3 == 0 ? 0 : h % 3 == 1 ? 1 : (h >> 8) & 0xF0);
}
unsigned char __cdecl StubSetTint(unsigned char* sprite, unsigned r, unsigned gg, unsigned b, unsigned a) {
    Record(29, Address(sprite), r, gg, b, a);
    Disturb();
    return static_cast<unsigned char>(Hash());
}
// 0x446A50 masks its argument to a byte; 0x437450 and Sound_PlayById read a word.
void __cdecl StubActorSound(unsigned v) {
    Record(30, v & 0xFF, Address(Field_State));
    Disturb();
}
void __cdecl StubEnemySound(unsigned v) {
    Record(31, v & 0xFFFF);
    Disturb();
}
void __cdecl StubPlayById(unsigned v) {
    Record(32, v & 0xFFFF);
    Disturb();
}
void __cdecl StubStreamStop() {
    Record(33, Address(Pointer(at::kStreamBuffer)));
    Disturb();
}
void* __cdecl StubCreate(const unsigned char* wave) {
    Record(34, Address(wave));
    Disturb();
    const std::uint32_t h = Hash();
    return h % 4 == 0 ? nullptr : &g_fakes[h & 1];
}
unsigned __cdecl StubFindData(const unsigned char* wave, const unsigned char** data) {
    Record(35, Address(wave));
    *data = g_wave + Hash() % 16;
    Disturb();
    return Hash();
}
long __cdecl StubWrite(void* buffer, const void* src, unsigned offset, unsigned size) {
    Record(36, Address(buffer), Address(src), offset, size);
    Disturb();
    return static_cast<long>(Hash());
}
void __cdecl StubFree() {
    Record(37, Address(Pointer(at::kSparkleCurrent)));
    Disturb();
}
template <unsigned N> void __cdecl StubType() {
    Record(40 + N, Address(Pointer(at::kSparkleCurrent)));
    Disturb();
}

// The fake buffer's methods (__stdcall, `this` first).
long __stdcall FakeSetPosition(void* self, unsigned long position) {
    Record(50, Address(self), position);
    Disturb();
    return static_cast<long>(Hash());
}
long __stdcall FakePlay(void* self, unsigned long a, unsigned long b, unsigned long c) {
    Record(51, Address(self), a, b, c);
    Disturb();
    const std::uint32_t h = Hash();
    return h % 2 ? static_cast<long>(0x88780096u) : h % 3 == 0 ? static_cast<long>(0x88780097u) : 0;
}
long __stdcall FakeGetStatus(void* self, unsigned long* status) {
    Record(52, Address(self));
    *status = Hash();
    Disturb();
    return 0;
}
long __stdcall FakeStop(void* self) {
    Record(53, Address(self));
    Disturb();
    return 0;
}
unsigned long __stdcall FakeRelease(void* self) {
    Record(54, Address(self));
    Disturb();
    return Hash();
}
long __stdcall FakeOther(void* self) {
    Record(55, Address(self));
    return 0;
}
const void* g_vtable[0x60 / 4];

const Callees kStubs = {
    StubDrawMode, StubCommit, StubCommitSorted, StubSin, StubCos,
    StubSetPrim<0x30>, StubSetPrim<0x50>, StubSetPrim<0x58>, StubSetPrim<0x68>, StubSetPrim<0x3C>,
    StubSemi, StubTPage, StubClut, StubRtp, StubRtp3, StubRtp4, StubDepths3, StubDepths4, StubStoreDepth,
    StubPushMatrix, StubRotTrans, StubRotMatrix, StubMulMatrix0, StubSetRot, StubSetTrans, StubRand,
    StubSetAnimation, StubEnemyAnimation, StubIsOut, StubSetTint, StubActorSound, StubEnemySound, StubPlayById,
    StubStreamStop, StubCreate, StubFindData, StubWrite, StubFree,
};

const void* StubFor(std::uint32_t target) {
    const auto f = [](auto p) { return reinterpret_cast<const void*>(p); };
    switch (target) {
    case 0x5A77C0: return f(kStubs.set_draw_mode);
    case 0x461E50: return f(kStubs.commit);
    case 0x572FA0: return f(kStubs.commit_sorted);
    case 0x5A7A00: return f(kStubs.sin);
    case 0x5A7A50: return f(kStubs.cos);
    case 0x5A75F0: return f(kStubs.set_poly_g3);
    case 0x5A76B0: return f(kStubs.set_line_g2);
    case 0x5A76D0: return f(kStubs.set_line_g3);
    case 0x5A7750: return f(kStubs.set_tile1);
    case 0x5A7630: return f(kStubs.set_poly_gt4);
    case 0x5A7780: return f(kStubs.set_semi);
    case 0x5A79A0: return f(kStubs.get_tpage);
    case 0x5A79E0: return f(kStubs.get_clut);
    case 0x5A8250: return f(kStubs.rtp);
    case 0x5A84A0: return f(kStubs.rtp3);
    case 0x5A85F0: return f(kStubs.rtp4);
    case 0x5A9310: return f(kStubs.depths3);
    case 0x5A93A0: return f(kStubs.depths4);
    case 0x5A9110: return f(kStubs.store_depth);
    case 0x5A7B90: return f(kStubs.push_matrix);
    case 0x5A8200: return f(kStubs.rot_trans);
    case 0x5A8060: return f(kStubs.rot_matrix);
    case 0x5A7D70: return f(kStubs.mul_matrix0);
    case 0x5A8DE0: return f(kStubs.set_rot);
    case 0x5A8E00: return f(kStubs.set_trans);
    case 0x5B93D2: return f(kStubs.rand);
    case 0x5891F0: return f(kStubs.set_animation);
    case 0x435A20: return f(kStubs.enemy_animation);
    case 0x4456C0: return f(kStubs.is_out);
    case 0x454CC0: return f(kStubs.set_tint);
    case 0x446A50: return f(kStubs.actor_sound);
    case 0x437450: return f(kStubs.enemy_sound);
    case 0x587900: return f(kStubs.play_by_id);
    case 0x5A71C0: return f(kStubs.stream_stop);
    case 0x5A69E0: return f(kStubs.create_from_wave);
    case 0x5A6AA0: return f(kStubs.find_data);
    case 0x5A6AF0: return f(kStubs.buffer_write);
    case 0x4B9900: return f(kStubs.sparkle_free);
    case 0x5B9550: return nullptr;   // the CRT's _ftol: pure x87, the copy keeps it
    default: bof3::Fatal("battle_items: no stand-in for a call to 0x%X", static_cast<unsigned>(target)); return nullptr;
    }
}

// --- the twenty-four copies (capstone, 2026-09-23: every jump internal, no
// jump table; the calls below are every call that leaves) --------------------

struct Call { std::uint32_t offset, target; };
// ret: the answer's width the original defines - 0 none, 1 al.
struct Clone { const char* name; std::uint32_t base, size; const Call* calls; int n_calls; unsigned ret; const void* ours; };

constexpr Call kLaunch[] = {{0x8C, 0x5B93D2}, {0xB0, 0x5B93D2}, {0xD7, 0x5B93D2}, {0x103, 0x5B93D2}, {0x115, 0x5B93D2}, {0x124, 0x5B93D2}};
constexpr Call kRise[] = {{0x21, 0x5A7A00}};
constexpr Call kFade[] = {{0x21, 0x5A7A00}, {0x7F, 0x4B9900}};
constexpr Call kRaysG2[] = {{0x2B, 0x5A77C0}, {0x41, 0x572FA0}, {0xA8, 0x5A76B0}, {0xB0, 0x5A7780}, {0xF0, 0x5A7A50}, {0x117, 0x5A7A00}, {0x16D, 0x572FA0}};
constexpr Call kRaysG3[] = {{0x2B, 0x5A77C0}, {0x41, 0x572FA0}, {0xB1, 0x5A76D0}, {0xB9, 0x5A7780}, {0xF9, 0x5A7A50}, {0x120, 0x5A7A00}, {0x147, 0x5A7A50}, {0x16E, 0x5A7A00}, {0x1D0, 0x572FA0}};
constexpr Call kDisc[] = {{0x11, 0x5A77C0}, {0x27, 0x572FA0}, {0x2F, 0x5B93D2}, {0xE5, 0x5A75F0}, {0xED, 0x5A7780}, {0x117, 0x5A7A00}, {0x13E, 0x5A7A50}, {0x16B, 0x5A7A00}, {0x192, 0x5A7A50}, {0x217, 0x572FA0}};
constexpr Call kFan[] = {{0x19, 0x5A77C0}, {0x22, 0x461E50}, {0x49, 0x5A7A00}, {0x62, 0x5A7A50}, {0x9E, 0x5A75F0}, {0xA5, 0x5A7780}, {0xD3, 0x5A7A00}, {0xEC, 0x5A7A50}, {0x129, 0x5A84A0}, {0x12F, 0x5A9310}, {0x164, 0x461E50}, {0x18A, 0x5A77C0}, {0x193, 0x461E50}};
constexpr Call kPush[] = {{0x3, 0x5A7B90}, {0x5F, 0x5A8200}, {0x6E, 0x5A8060}, {0x82, 0x5A7D70}, {0x8C, 0x5A8DE0}, {0x96, 0x5A8E00}};
constexpr Call kRing[] = {{0x54, 0x5A7A50}, {0x6D, 0x5A7A00}, {0x8D, 0x5A7A00}, {0xAE, 0x5A7A50}, {0xC7, 0x5A7A00}, {0x13E, 0x5A7A50}, {0x157, 0x5A7A00}, {0x177, 0x5A7A00}, {0x1B9, 0x5A7A50}, {0x1D2, 0x5A7A00}, {0x218, 0x5A77C0}, {0x223, 0x572FA0}, {0x22F, 0x5A7630}, {0x237, 0x5A7780}, {0x24D, 0x5A79A0}, {0x25D, 0x5A79E0}, {0x35D, 0x5A85F0}, {0x366, 0x5A93A0}, {0x371, 0x572FA0}};
constexpr Call kFxDisc[] = {{0x2D, 0x5A77C0}, {0x43, 0x572FA0}, {0x4F, 0x5A75F0}, {0x56, 0x5A7780}, {0x85, 0x5A7A00}, {0xAF, 0x5A7A50}, {0xDF, 0x5A7A00}, {0x109, 0x5A7A50}, {0x16D, 0x572FA0}};
constexpr Call kSetAnim[] = {{0x26, 0x5891F0}, {0x3C, 0x435A20}};
constexpr Call kScreen[] = {{0x43, 0x5A7750}, {0x5B, 0x5A8250}, {0x64, 0x5A9110}, {0x6E, 0x5B9550}, {0x80, 0x5B9550}};
constexpr Call kFlash[] = {{0x6, 0x4456C0}, {0x41, 0x454CC0}, {0x75, 0x454CC0}};
constexpr Call kSound[] = {{0x38, 0x446A50}, {0x49, 0x446A50}, {0x79, 0x437450}, {0xA2, 0x587900}};
constexpr Call kPlay[] = {{0xB, 0x5A71C0}, {0x15, 0x5A69E0}, {0x4C, 0x5A6AA0}, {0x60, 0x5A6AF0}};

#define BI_C(name, base, size, calls, ret) {#name, base, size, calls, static_cast<int>(sizeof calls / sizeof calls[0]), ret, reinterpret_cast<const void*>(&::name)}
#define BI_P(name, base, size, ret) {#name, base, size, nullptr, 0, ret, reinterpret_cast<const void*>(&::name)}
const Clone kClones[] = {
    BI_P(Gpu_SetPolyG3, 0x5A75F0, 0x17, 0),
    BI_P(Gpu_SetLineG2, 0x5A76B0, 0x14, 0),
    BI_P(Gpu_SetLineG3, 0x5A76D0, 0x17, 0),
    BI_P(Gpu_SetTile1, 0x5A7750, 0x10, 0),
    BI_P(Sparkle_Dispatch, 0x4B8FE0, 0x12, 0),
    BI_C(Sparkle_Launch, 0x4B9090, 0x161, kLaunch, 0),
    BI_C(Sparkle_Rise, 0x4B9200, 0x6C, kRise, 0),
    BI_C(Sparkle_Fade, 0x4B9270, 0x85, kFade, 0),
    BI_C(Sparkle_DrawRaysG2, 0x4B9300, 0x185, kRaysG2, 0),
    BI_C(Sparkle_DrawRaysG3, 0x4B9490, 0x1ED, kRaysG3, 0),
    BI_C(Sparkle_DrawDisc, 0x4B9680, 0x22F, kDisc, 0),
    BI_P(Sparkle_Alloc, 0x4B98B0, 0x4A, 1),
    BI_P(Sparkle_Free, 0x4B9900, 0x2F, 0),
    BI_C(MagicFx_DrawFan, 0x4AD6F0, 0x1A3, kFan, 0),
    BI_C(MagicFx_PushActorMatrix, 0x4B7D40, 0x9F, kPush, 0),
    BI_C(MagicFx_DrawRing, 0x4C5150, 0x39C, kRing, 0),
    BI_C(MagicFx_DrawDisc, 0x4C54F0, 0x186, kFxDisc, 0),
    BI_C(BattleActor_SetAnimation, 0x4FB830, 0x4C, kSetAnim, 0),
    BI_C(BattleActor_UpdateScreenXY, 0x4FBD10, 0x95, kScreen, 0),
    BI_C(BattleActor_Flash, 0x4FBDB0, 0x7F, kFlash, 0),
    BI_C(BattleActor_PlaySound, 0x4FC030, 0xAC, kSound, 0),
    BI_P(BattleActor_FxSize, 0x4FC1F0, 0x6B, 1),
    BI_C(SndStream_Play, 0x5A7140, 0x7C, kPlay, 0),
    BI_P(SndStream_Stop, 0x5A71C0, 0x3E, 0),
};
#undef BI_C
#undef BI_P
constexpr unsigned kCount = sizeof kClones / sizeof kClones[0];

// Indices the seeding singles out.
enum : unsigned {
    kPolyG3, kLineG2, kLineG3, kTile1, kDispatch, kLaunchK, kRiseK, kFadeK, kRaysG2K, kRaysG3K, kDiscK, kAllocK,
    kFreeK, kFanK, kPushK, kRingK, kFxDiscK, kSetAnimK, kScreenK, kFlashK, kSoundK, kFxSizeK, kPlayK, kStopK,
};
static_assert(kStopK + 1 == kCount, "the seeding's indices");

// --- the state both passes start from --------------------------------------

struct Region { std::uint32_t at, size; };
Region g_regions[] = {
    {at::kSparklePool, at::kSparkles * at::kSparkleStride},   // the pool
    {at::kSparkleCurrent, 4},
    {at::kFxActor, 4},
    {at::kScratch, 0x10},
    {at::kVertex, 0x20},
    {at::kSparkleOffsets, 0x104},                             // 0x65AD34..0x65AE38: the sparkles' tables, Sparkle_Types[0..3]
    {0x937F88, 0x10},                                         // Sprite_Current, Frame_Counter (0x937F94)
    {0x7E0670, 4},                                            // Gfx_PacketNext
    {at::kActorIndex, 4},
    {at::kFxSizeSelect, 1},
    {at::kEnemySoundMode, 1},
    {at::kPartyRecords, 3 * at::kPartyStride},
    {at::kEnemyRecords, 8 * at::kEnemyStride},
    {0x905D98, 4},                                            // Field_State
    {0x7DE3BC, 4},                                            // Snd_Device
    {at::kStreamBuffer, 4},
    {0, kPrimBytes},                                          // g_prims (filled in at start-up)
    {0, 2 * kRecordBytes},                                    // g_sprites
    {0, 2 * kRecordBytes},                                    // g_actors
    {0, 0x40},                                                // g_cues
};
constexpr unsigned kRegionBytes = at::kSparkles * at::kSparkleStride + 4 + 4 + 0x10 + 0x20 + 0x104 + 0x10 + 4 + 4 + 1 + 1 +
                                  3 * at::kPartyStride + 8 * at::kEnemyStride + 4 + 4 + 4 + kPrimBytes +
                                  2 * kRecordBytes + 2 * kRecordBytes + 0x40;

struct State {
    unsigned char memory[kRegionBytes];
    std::uint32_t result;
    Entry log[kLog];
    unsigned log_n;
};
void Capture(State& s) {
    unsigned n = 0;
    for (const Region& r : g_regions) { std::memcpy(s.memory + n, At(r.at), r.size); n += r.size; }
    std::memcpy(s.log, g_log, sizeof s.log);
    s.log_n = g_log_n;
}
void Apply(const State& s) {
    unsigned n = 0;
    for (const Region& r : g_regions) { std::memcpy(At(r.at), s.memory + n, r.size); n += r.size; }
    std::memset(g_log, 0, sizeof g_log);
    g_log_n = 0;
    g_trans = g_matrix = nullptr;
}

// Random bytes put back inside what the functions dereference: the pointers
// among the regions, the actor index to a party member or one of the eight
// enemy records (upper bytes random: 0x4FC1F0 reads a dword), the enemies'
// cue pointers into g_cues, the dispatch table's first four entries.
void Fix(const void* const* types) {
    SetPointer(at::kSparkleCurrent, Sparkle(Next()));
    SetPointer(at::kFxActor, g_actors[Next() & 1]);
    Sprite_Current = g_sprites[Next() & 1];
    Gfx_PacketNext = PrimAt(Next());
    At(at::kActorIndex)[0] = static_cast<unsigned char>(Next() % 11);
    for (unsigned j = 0; j < 8; ++j)
        SetPointer(at::kEnemyRecords + j * at::kEnemyStride + 0xF8, g_cues + Next() % 0x20);
    for (unsigned t = 0; t < 4; ++t) SetPointer(at::kSparkleTypes + 4 * t, types[t]);
    SetPointer(0x7DE3BC, Next() % 4 == 0 ? nullptr : &g_fake);
    SetPointer(at::kStreamBuffer, Next() % 4 == 0 ? nullptr : &g_fake);
}

struct Args { std::uint32_t a[3]; };

// A dword whose low byte is `low`, the rest stale half the time.
std::uint32_t Stale(std::uint32_t low_bits, std::uint32_t value) { return Half() ? value : (Next() & ~low_bits) | value; }

Args Seed(unsigned k) {
    Args args{{Next(), Next(), Next()}};
    unsigned char* const cur = Pointer(at::kSparkleCurrent);
    unsigned char* const sc = Sprite_Current;
    switch (k) {
    case kPolyG3: case kLineG2: case kLineG3: case kTile1:
        args.a[0] = Address(PrimAt(Next()));
        break;
    case kDispatch:
        cur[at::kType] = static_cast<unsigned char>(Next() % 4);
        break;
    case kLaunchK: {
        static const unsigned char kTimer[] = {0, 1, 2, 0xFF};
        if (Often()) cur[at::kTimer] = kTimer[Next() % 4];
        static const std::uint16_t kOffsets[] = {0, 1, 0xFFFF, 0x7FFF, 0x8000, 7, 0xFFF8};
        const unsigned row = cur[at::kLimit] & 0x1F;
        if (Often()) SetWord(At(at::kSparkleOffsets + row * 4), kOffsets[Next() % 7]);
        if (Half()) SetWord(At(at::kSparkleOffsets + row * 4 + 2), kOffsets[Next() % 7]);
        break;
    }
    case kRiseK:
        if (Half()) cur[at::kCount] = static_cast<unsigned char>(cur[at::kLimit] - 1);
        if (Half()) SetWord(cur + at::kWave, Half() ? 0xFFFF : 0x3F);
        break;
    case kFadeK:
        if (Half()) Frame_Counter &= ~3u;
        if (Half()) cur[at::kTimer] = static_cast<unsigned char>(Next() % 3);
        break;
    case kRaysG2K: case kRaysG3K: {
        static const std::uint32_t kStart[] = {0, 0xFFFF, 0xFFE0, 0x1F, 0x10000, 0xFFFFFFFFu};
        args.a[0] = Often() ? kStart[Next() % 6] : Next();
        static const std::uint32_t kRadius[] = {0, 1, 0x10, 0xFFFF, 0x10000, 0x8001, 0xFFFFFFFFu};
        args.a[1] = Often() ? kRadius[Next() % 7] : Next();
        break;
    }
    case kAllocK:
        for (unsigned i = 0; i < at::kSparkles; ++i) {
            unsigned char* const s = At(at::kSparklePool + i * at::kSparkleStride);
            s[0] = static_cast<unsigned char>(s[0] | 1);
        }
        if (Next() % 5) {
            static const unsigned kFree[] = {0, 1, 0x7E, 0x7F};
            const unsigned f = Half() ? kFree[Next() % 4] : Next() % at::kSparkles;
            unsigned char* const s = At(at::kSparklePool + f * at::kSparkleStride);
            s[0] = static_cast<unsigned char>(s[0] & 0xFE);
        }
        break;
    case kPushK: case kScreenK: {
        static const std::uint16_t kHeight[] = {0, 1, 0xFFFF, 0xFFFE, 0x8000, 0x7FFF, 3, 0xFFFD};
        if (Often()) SetWord(sc + 0x3E, kHeight[Next() % 8]);
        if (Half()) SetLong(sc + 0x34, static_cast<std::int32_t>(Half() ? 0x80000000u : 0x7FFFFFFFu));
        break;
    }
    case kSetAnimK:
    case kFlashK:
    case kSoundK:
    case kFxSizeK: {
        static const unsigned char kIndex[] = {0, 2, 3, 4, 10, 1};
        At(at::kActorIndex)[0] = kIndex[Next() % 6];
        const unsigned index = At(at::kActorIndex)[0];
        if (k == kSetAnimK) args.a[0] = Stale(0xFF, Half() ? 0xFF : Next() & 0xFF);
        if (k == kFlashK) {
            args.a[0] = Stale(0xFF, kIndex[Next() % 6]);
            const unsigned i = args.a[0] & 0xFF;
            unsigned char* const r = i < 3 ? At(at::kPartyRecords + i * at::kPartyStride)
                                           : At(at::kEnemyRecords + (i - 3) * at::kEnemyStride);
            if (Half()) r[i < 3 ? 0x90 : 0x92] ^= 0x80;
        }
        if (k == kSoundK) {
            static const std::uint32_t kSounds[] = {0, 0xFF, 1, 0xFE};
            args.a[0] = Stale(0xFF, Often() ? kSounds[Next() % 4] : Next() & 0xFF);
            args.a[1] = Stale(0xFF, Often() ? kSounds[Next() % 4] : Next() & 0xFF);
            At(at::kEnemySoundMode)[0] = static_cast<unsigned char>(Half() ? 0 : Next() | 1);
        }
        if (k == kFxSizeK && index < 3) {
            unsigned char* const r = At(at::kPartyRecords + index * at::kPartyStride);
            if (Half()) r[0x134] ^= 2;
        }
        break;
    }
    case kPlayK:
        args.a[0] = Address(g_wave);
        break;
    default:
        break;
    }
    return args;
}

// What the rounds reached, from the original's side, for the log line.
struct Coverage {
    unsigned logged[64];
    unsigned alloc_full, alloc_found, launched, rise_done, freed, party, enemy, lost;
} g_cover;

void Cover(unsigned k, const State& in, const State& out, std::uint32_t result) {
    for (unsigned i = 0; i < out.log_n && i < kLog; ++i)
        if (out.log[i].what < 64) ++g_cover.logged[out.log[i].what];
    const auto count = [&](std::uint32_t what) {
        unsigned n = 0;
        for (unsigned i = 0; i < out.log_n && i < kLog; ++i) n += out.log[i].what == what ? 1u : 0u;
        return n;
    };
    (void)in;
    switch (k) {
    case kAllocK: (result & 0xFF) == 0xFF ? ++g_cover.alloc_full : ++g_cover.alloc_found; break;
    case kLaunchK: if (count(25)) ++g_cover.launched; break;
    case kFadeK: if (count(37)) ++g_cover.freed; break;
    case kSetAnimK: count(26) ? ++g_cover.party : ++g_cover.enemy; break;
    case kPlayK: if (count(35)) ++g_cover.lost; break;
    default: break;
    }
}

using Fn3 = std::uint32_t (__cdecl*)(std::uint32_t, std::uint32_t, std::uint32_t);

}  // namespace

void SelfTest() {
    constexpr unsigned kPerFunction = 2000;
    // The fuzz's own buffers, and the fake buffer's vtable.
    g_regions[16].at = Address(g_prims);
    g_regions[17].at = Address(g_sprites);
    g_regions[18].at = Address(g_actors);
    g_regions[19].at = Address(g_cues);
    unsigned region_bytes = 0;
    for (const Region& r : g_regions) region_bytes += r.size;
    if (region_bytes != kRegionBytes)
        bof3::Fatal("battle_items: the regions are %u bytes, the state holds %u", region_bytes, kRegionBytes);
    for (auto& m : g_vtable) m = reinterpret_cast<const void*>(&FakeOther);
    g_vtable[8 / 4] = reinterpret_cast<const void*>(&FakeRelease);
    g_vtable[0x24 / 4] = reinterpret_cast<const void*>(&FakeGetStatus);
    g_vtable[0x30 / 4] = reinterpret_cast<const void*>(&FakePlay);
    g_vtable[0x34 / 4] = reinterpret_cast<const void*>(&FakeSetPosition);
    g_vtable[0x48 / 4] = reinterpret_cast<const void*>(&FakeStop);
    g_fakes[0].vtable = g_fakes[1].vtable = g_vtable;

    void* clones[kCount];
    for (unsigned k = 0; k < kCount; ++k) {
        const Clone& c = kClones[k];
        bof3::CloneCall calls[24];
        if (c.n_calls > 24) bof3::Fatal("battle_items: %s has %d calls", c.name, c.n_calls);
        for (int i = 0; i < c.n_calls; ++i) calls[i] = {c.calls[i].offset, StubFor(c.calls[i].target), c.calls[i].target};
        clones[k] = bof3::CloneOriginal(c.name, c.base, c.size, calls, c.n_calls);
    }
    static const void* const kTypes[4] = {
        reinterpret_cast<const void*>(&StubType<0>), reinterpret_cast<const void*>(&StubType<1>),
        reinterpret_cast<const void*>(&StubType<2>), reinterpret_cast<const void*>(&StubType<3>)};

    static State saved, input, their_out, our_out;
    Capture(saved);
    g = kStubs;

    unsigned bad = 0, calls = 0, bad_per[kCount] = {};
    for (unsigned round = 0; round < kPerFunction * kCount; ++round) {
        const unsigned k = round % kCount;
        for (unsigned i = 0; i < kRegionBytes; i += 4) {
            const std::uint32_t v = Next();
            std::memcpy(input.memory + i, &v, kRegionBytes - i < 4 ? kRegionBytes - i : 4);
        }
        std::memset(input.log, 0, sizeof input.log);
        input.log_n = 0;
        Apply(input);
        Fix(kTypes);
        g_seed = Next();
        const Args args = Seed(k);
        Capture(input);

        std::uint32_t result[2];
        for (int pass = 0; pass < 2; ++pass) {
            Apply(input);
            State& out = pass ? our_out : their_out;
            const void* const fn = pass ? kClones[k].ours : clones[k];
            const std::uint32_t r = reinterpret_cast<Fn3>(const_cast<void*>(fn))(args.a[0], args.a[1], args.a[2]);
            Capture(out);
            const unsigned w = kClones[k].ret;
            result[pass] = w == 0 ? 0u : (r & 0xFFu);
            out.result = result[pass];
        }
        calls += their_out.log_n;
        if (their_out.log_n > kLog) bof3::Fatal("battle_items: %s made %u calls, the log holds %u", kClones[k].name, their_out.log_n, kLog);
        Cover(k, input, their_out, result[0]);
        if (std::memcmp(&their_out, &our_out, sizeof their_out) != 0) {
            ++bad_per[k];
            if (++bad <= 12) {
                unsigned first = 0;
                while (first < sizeof their_out &&
                       reinterpret_cast<const unsigned char*>(&their_out)[first] == reinterpret_cast<const unsigned char*>(&our_out)[first])
                    ++first;
                bof3::Log("shadow      battle_items self-test MISMATCH: round %u, %s, log %u / %u, result 0x%X / 0x%X, "
                          "first differing state byte %u",
                          round, kClones[k].name, their_out.log_n, our_out.log_n, their_out.result, our_out.result, first);
            }
        }
    }
    g = kOriginals;
    Apply(saved);

    bof3::Log("shadow      battle_items self-test: %u rounds over %u functions (%u each), %u calls to the stand-ins, "
              "%u MISMATCHES; the sparkle pool, the scratch words, the sparkles' tables, the party and enemy records, "
              "two sprite and two actor records, a primitive buffer, the answer and the stand-ins' log compared",
              kPerFunction * kCount, kCount, kPerFunction, calls, bad);
    if (bad)
        for (unsigned k = 0; k < kCount; ++k)
            if (bad_per[k]) bof3::Log("shadow      battle_items: %s mismatched in %u rounds", kClones[k].name, bad_per[k]);
    const Coverage& c = g_cover;
    bof3::Log("shadow      battle_items coverage: dispatched %u / %u / %u / %u; alloc found %u, full %u; launched %u; "
              "freed %u; party animations %u, enemy %u; stream data rewritten %u; sorted commits %u, sin %u, cos %u, "
              "tints %u, actor sounds %u, enemy cues %u, sounds by id %u, lost-buffer plays %u",
              c.logged[40], c.logged[41], c.logged[42], c.logged[43], c.alloc_found, c.alloc_full, c.launched, c.freed,
              c.party, c.enemy, c.lost, c.logged[3], c.logged[4], c.logged[5], c.logged[29], c.logged[30], c.logged[31],
              c.logged[32], c.logged[51]);
    if (bad) bof3::Fatal("the battle's item effects differ from the original in %u self-test rounds", bad);
}

}  // namespace battle_items

// The battle's item and ability effects, the battle actor's small helpers and
// four PSX primitive setters. docs/battle_items.md.
//
//   - The Healing Herb's sparkles (MAGIC070.EMI on the PSX, compiled into the
//     exe at 0x4B8BD0..0x4B992F): the type dispatch 0x4B8FE0, the three
//     phases 0x4B9090 / 0x4B9200 / 0x4B9270 that 0x4B9000 (left Capcom's)
//     picks, the three draws 0x4B9300 / 0x4B9490 / 0x4B9680, the pool's
//     alloc and free 0x4B98B0 / 0x4B9900.
//   - Three effect draws shared by several MAGIC overlays (the PSX copies
//     read are Sacrifice's, MAGIC 6bdc2d45): 0x4AD6F0, 0x4C5150, 0x4C54F0, and
//     the actor-matrix push 0x4B7D40 they are drawn under.
//   - The battle actor's helpers 0x4FB830..0x4FC1F0 (BATTLE.EMI section 15).
//   - libgpu's SetPolyG3, SetLineG2, SetLineG3 and SetTile1, the port's wider
//     primitives, next to psx_gpu.cpp's.
//   - The SND stream's play and stop 0x5A7140 / 0x5A71C0.
//
// Every call goes through battle_items::g (battle_items_callees.h), so that
// the start-up fuzz can stand recorders in for them - for ours and for the
// originals' copies alike. No divergence: each is a faithful replacement.
#include "game/battle_items.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/battle_items_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace battle_items {

using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

namespace {
template <typename T> T Fn(std::uint32_t address) { return reinterpret_cast<T>(static_cast<std::uintptr_t>(address)); }
template <typename T, typename F> T As(F* f) { return reinterpret_cast<T>(reinterpret_cast<void*>(f)); }
}  // namespace

const Callees kOriginals = {
    Gpu_SetDrawMode,
    Gfx_CommitPrim,
    Fn<void (__cdecl*)(unsigned, unsigned, unsigned, unsigned)>(kCommitSorted),
    Math_Sin,
    Math_Cos,
    Gpu_SetPolyG3,
    Gpu_SetLineG2,
    Gpu_SetLineG3,
    Gpu_SetTile1,
    Gpu_SetPolyGT4,
    Gpu_SetSemiTrans,
    Gpu_GetTPage,
    Gpu_GetClut,
    As<long (__cdecl*)(const short*, float*, long*, long*)>(&Gte_RotTransPers),
    As<long (__cdecl*)(const short*, const short*, const short*, float*, float*, float*, long*, long*)>(&Gte_RotTransPers3),
    As<long (__cdecl*)(const short*, const short*, const short*, const short*, float*, float*, float*, float*, long*,
                       long*)>(&Gte_RotTransPers4),
    As<void (__cdecl*)(unsigned char*)>(&Gte_PrimDepths3_10B),
    As<void (__cdecl*)(unsigned char*)>(&Gte_PrimDepths4_14),
    Gte_StoreDepthF,
    Gte_PushMatrix,
    As<void (__cdecl*)(const short*, long*, long*)>(&Gte_RotTrans),
    Gte_RotMatrix,
    Gte_MulMatrix0,
    Gte_SetRotMatrix,
    Gte_SetTransMatrix,
    Rand,
    As<void (__cdecl*)(unsigned)>(&Sprite_SetAnimation),
    Fn<void (__cdecl*)(unsigned, unsigned)>(kEnemyAnimation),
    Fn<unsigned char (__cdecl*)(unsigned)>(kActorIsOut),
    Fn<unsigned char (__cdecl*)(unsigned char*, unsigned, unsigned, unsigned, unsigned)>(kSetTint),
    Fn<void (__cdecl*)(unsigned)>(kActorSound),
    Fn<void (__cdecl*)(unsigned)>(kEnemySound),
    Fn<void (__cdecl*)(unsigned)>(kPlayById),
    SndStream_Stop,
    As<void* (__cdecl*)(const unsigned char*)>(&SndBuf_CreateFromWave),
    Wave_FindData,
    SndBuf_Write,
    Sparkle_Free,
};
Callees g = kOriginals;

}  // namespace battle_items

using namespace battle_items;

namespace {

// What the setters leave at +0x10 and every 0x10 after it: the float 0.01, as
// psx_gpu.cpp's do (docs/psx-library-layer.md section 1).
constexpr std::uint32_t kPointZeroOne = 0x3C23D70Au;

void PutDword(unsigned char* at, std::uint32_t v) { std::memcpy(at, &v, sizeof v); }

// The sparkle being updated, and the battle actor the effect plays on - each
// read from its cell every time the original reads it.
unsigned char* Cur() { return *reinterpret_cast<unsigned char**>(At(at::kSparkleCurrent)); }
unsigned char* Actor() { return *reinterpret_cast<unsigned char**>(At(at::kFxActor)); }

// The scratch words: DamageScratch + k, and the vertex scratch + k.
unsigned char* Scr(unsigned k) { return At(at::kScratch + k); }
short S16(const unsigned char* at) { return static_cast<short>(Word(at)); }
unsigned char* Vtx(unsigned k) { return At(at::kVertex + k); }
const short* VtxP(unsigned k) { return reinterpret_cast<const short*>(Vtx(k)); }

// `imul` then `sar 0xC`: a 32-bit product that wraps, shifted arithmetically.
int Mul12(int a, int b) {
    return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12;
}

// `fild dword` then `fstp dword` of an int: the original's integer vertex as
// a float.
void PutFloat(unsigned char* at, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(at, &f, sizeof f);
}

// `fld dword` then the CRT's _ftol 0x5B9550: truncation through a 64-bit
// fistp, whose NaN and out-of-range answer is the integer indefinite
// 0x8000000000000000 - so 0 in the low word the caller keeps. As
// field_frame.cpp's.
std::uint16_t Ftol16(std::uint32_t bits) {
    float v;
    std::memcpy(&v, &bits, sizeof v);
    if (!(v > -9.2233720368547758e18f && v < 9.2233720368547758e18f)) return 0;   // NaN included
    return static_cast<std::uint16_t>(static_cast<std::uint64_t>(static_cast<std::int64_t>(v)));
}

// `mov al, [eax + 8]; add al, dl; push eax`: the actor's animation byte plus
// the offset, in the low byte of the actor's own pointer. In inline assembly
// because clang 22.1.8 folds the C++ spelling of a pointer's high bytes
// merged with a byte read through it (docs/event-ops.md section 8).
std::uint32_t AnimationInPointer(const unsigned char* actor, unsigned offset) {
    std::uint32_t v = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(actor));
    __asm__ volatile("movb 8(%1), %b0\n\taddb %b2, %b0" : "+q"(v) : "r"(actor), "q"(offset) : "memory", "cc");
    return v;
}

// A method of the IDirectSoundBuffer the SND stream holds, by vtable offset.
template <typename F> F Method(void* object, unsigned offset) {
    F f;
    std::memcpy(&f, *reinterpret_cast<unsigned char**>(object) + offset, sizeof f);
    return f;
}
void*& StreamBuffer() { return *reinterpret_cast<void**>(At(at::kStreamBuffer)); }
constexpr long kBufferLost = static_cast<long>(0x88780096u);   // DSERR_BUFFERLOST

}  // namespace

// ===========================================================================
// The PSX primitive setters (libgpu's, on the port's wider primitives)

// original 0x5A75F0 (PSX SetPolyG3 0x8017B2F4): code 0x30, 0.01 per vertex.
extern "C" void __cdecl Gpu_SetPolyG3(unsigned char* prim) {
    prim[7] = 0x30;
    for (unsigned k = 0x10; k <= 0x30; k += 0x10) PutDword(prim + k, kPointZeroOne);
}
// original 0x5A76B0 (PSX SetLineG2 0x8017B40C): code 0x50, two vertices.
extern "C" void __cdecl Gpu_SetLineG2(unsigned char* prim) {
    prim[7] = 0x50;
    PutDword(prim + 0x10, kPointZeroOne);
    PutDword(prim + 0x20, kPointZeroOne);
}
// original 0x5A76D0 (PSX SetLineG3 0x8017B440): code 0x58, three vertices.
// The PSX's also stores the polyline's terminator 0x55555555; the port's
// primitive has none.
extern "C" void __cdecl Gpu_SetLineG3(unsigned char* prim) {
    prim[7] = 0x58;
    for (unsigned k = 0x10; k <= 0x30; k += 0x10) PutDword(prim + k, kPointZeroOne);
}
// original 0x5A7750 (PSX SetTile1 0x8017B3A8): code 0x68, one vertex.
extern "C" void __cdecl Gpu_SetTile1(unsigned char* prim) {
    prim[7] = 0x68;
    PutDword(prim + 0x10, kPointZeroOne);
}

// ===========================================================================
// The Healing Herb's sparkles (MAGIC070.EMI)

// original 0x4B8FE0: the type byte +1 through Sparkle_Types, a jmp. As the
// original has it: the table's one real entry is type 0 (0x4B9000), the words
// after it are data, and nothing checks the type - 0x4B8D70 sets it to 0.
extern "C" void __cdecl Sparkle_Dispatch(void) {
    const unsigned type = Cur()[at::kType];
    reinterpret_cast<void (__cdecl*)()>(static_cast<std::uintptr_t>(Long(At(at::kSparkleTypes + 4 * type))))();
}

// original 0x4B9090 (PSX MAGIC070 0x801EF47C's slot): the launch. The delay +5
// counts down; at 0 the sparkle takes the actor's position (+0x34..+0x3C) and
// screen point (+0x2E / +0x30), is thrown by the offset row +7 (x by the
// row's dx pushed 0..7 further from 0, y up by dy + 0..7), gets a random sway
// and drift, a rise length from its kind, brightness 0x10 and phase 1.
// The x pointer is taken before the Rand, the sparkle read again after each
// call, as the original does.
extern "C" void __cdecl Sparkle_Launch(void) {
    unsigned char* c = Cur();
    c[at::kTimer] = static_cast<unsigned char>(c[at::kTimer] - 1);
    c = Cur();
    if (c[at::kTimer] != 0) return;
    SetLong(c + 0x14, Long(Actor() + 0x34));
    SetLong(Cur() + 0x18, Long(Actor() + 0x38));
    SetLong(Cur() + 0x1C, Long(Actor() + 0x3C));
    SetWord(Cur() + at::kX, Word(Actor() + 0x2E));
    SetWord(Cur() + at::kY, Word(Actor() + 0x30));
    c = Cur();
    unsigned char* const x = c + at::kX;
    if (S16(At(at::kSparkleOffsets + (c[at::kLimit] & 0x1F) * 4)) < 0) {
        const int r = g.rand();
        const unsigned char* const c2 = Cur();
        const auto dx = static_cast<std::uint16_t>(Word(At(at::kSparkleOffsets + (c2[at::kLimit] & 0x1F) * 4)) - (r & 7));
        SetWord(x, Word(x) + dx);
    } else {
        const int r = g.rand();
        const unsigned char* const c2 = Cur();
        const auto dx = static_cast<std::uint16_t>((r & 7) + Word(At(at::kSparkleOffsets + (c2[at::kLimit] & 0x1F) * 4)));
        SetWord(x, Word(x) + dx);
    }
    unsigned char* const y = Cur() + at::kY;
    {
        const int r = g.rand();
        const unsigned char* const c2 = Cur();
        const auto dy = static_cast<std::uint16_t>((r & 7) + Word(At(at::kSparkleOffsets + 2 + (c2[at::kLimit] & 0x1F) * 4)));
        SetWord(y, Word(y) - dy);
    }
    c = Cur();
    SetWord(c + at::kBaseX, Word(c + at::kX));
    {
        const int r = g.rand();
        SetWord(Cur() + at::kDriftY, static_cast<unsigned>(r & 1));
    }
    {
        const int r = g.rand();
        SetWord(Cur() + at::kWave, static_cast<unsigned>(r));
    }
    {
        const int r = g.rand();
        c = Cur();
        c[at::kLimit] = static_cast<unsigned char>((r & 6) + At(at::kSparkleLife)[c[at::kKind]]);
    }
    Cur()[at::kTimer] = 0x10;
    Cur()[at::kCount] = 0;
    c = Cur();
    c[at::kPhase] = static_cast<unsigned char>(c[at::kPhase] + 1);
}

namespace {
// The sway both later phases share: the wave +0xC steps, x = base +
// sin((wave & 0x3F) << 6) * 24 / 4096 (the angle kept in scratch +6), y
// drifts by +0xA.
void Sway() {
    unsigned char* c = Cur();
    SetWord(c + at::kWave, Word(c + at::kWave) + 1);
    const int angle = (Cur()[at::kWave] & 0x3F) << 6;
    SetWord(Scr(6), static_cast<unsigned>(angle));
    const int s = g.sin(angle);
    c = Cur();
    SetWord(c + at::kX, static_cast<unsigned>(Mul12(s, 24)) + Word(c + at::kBaseX));
    c = Cur();
    SetWord(c + at::kY, Word(c + at::kY) + Word(c + at::kDriftY));
}
}  // namespace

// original 0x4B9200 (PSX MAGIC070 0x801EF67C): the rise. The sway, then the
// count +6 up to the rise length +7, which moves the phase on.
extern "C" void __cdecl Sparkle_Rise(void) {
    Sway();
    unsigned char* c = Cur();
    c[at::kCount] = static_cast<unsigned char>(c[at::kCount] + 1);
    c = Cur();
    if (c[at::kCount] == c[at::kLimit]) c[at::kPhase] = static_cast<unsigned char>(c[at::kPhase] + 1);
}

// original 0x4B9270 (PSX MAGIC070 0x801EF72C): the fade. The sway; the count
// down every fourth frame; the brightness +5 down, and at 0 the actor's
// sparkle count +0xB down and the sparkle freed (a tail jmp in the original).
extern "C" void __cdecl Sparkle_Fade(void) {
    Sway();
    if ((Frame_Counter & 3) == 0) {
        unsigned char* const c = Cur();
        c[at::kCount] = static_cast<unsigned char>(c[at::kCount] - 1);
    }
    unsigned char* c = Cur();
    c[at::kTimer] = static_cast<unsigned char>(c[at::kTimer] - 1);
    if (Cur()[at::kTimer] != 0) return;
    unsigned char* const actor = Actor();
    actor[0xB] = static_cast<unsigned char>(actor[0xB] - 1);
    g.sparkle_free();
}

namespace {
// The three draws' common start: the sparkle's screen point to scratch +0 /
// +2 (the rays), the draw mode, and a sorted commit of it at the sparkle's
// position.
void DrawModeAtSparkle() {
    g.set_draw_mode(Gfx_PacketNext, 0, 1, 0x35, 0);
    const unsigned char* const c = Cur();
    g.commit_sorted(static_cast<unsigned>(Long(c + 0x14)), static_cast<unsigned>(Long(c + 0x18)), 2, 0xC);
}
void CommitAtSparkle(unsigned size) {
    const unsigned char* const c = Cur();
    g.commit_sorted(static_cast<unsigned>(Long(c + 0x14)), static_cast<unsigned>(Long(c + 0x18)), 2, size);
}
// Scratch +8 / +0xA / +0xC: the brightness +5 times 6, three times.
void RayShade() {
    const unsigned char* const c = Cur();
    SetWord(Scr(8), c[at::kTimer] * 6u);
    SetWord(Scr(0xA), c[at::kTimer] * 6u);
    SetWord(Scr(0xC), c[at::kTimer] * 6u);
}
}  // namespace

// original 0x4B9300 (PSX MAGIC070 0x801EF810): four gouraud lines from the
// sparkle's point outward, `radius` long, at angles ((start + 8k) & 0x1F) <<
// 7, bright at the centre (+5 * 6) and dark at the tip. Both arguments are
// read as their low words, as the original does (its caller pushes stale
// upper halves).
extern "C" void __cdecl Sparkle_DrawRaysG2(unsigned start, unsigned radius) {
    const unsigned char* const c = Cur();
    SetWord(Scr(0), Word(c + at::kX));
    SetWord(Scr(2), Word(c + at::kY));
    DrawModeAtSparkle();
    RayShade();
    int n = static_cast<int>(start & 0xFFFF);
    const int end = n + 0x20;
    const int r = static_cast<int>(radius & 0xFFFF);
    do {
        unsigned char* const p = Gfx_PacketNext;
        g.set_line_g2(p);
        g.set_semi(p, 1);
        PutFloat(p + 8, S16(Scr(0)));
        SetWord(Scr(6), static_cast<unsigned>((n & 0x1F) << 7));
        PutFloat(p + 0xC, S16(Scr(2)));
        const int co = g.cos(S16(Scr(6)));
        PutFloat(p + 0x18, Mul12(co, r) + S16(Scr(0)));
        const int si = g.sin(S16(Scr(6)));
        PutFloat(p + 0x1C, Mul12(si, r) + S16(Scr(2)));
        p[4] = Scr(8)[0];
        p[5] = Scr(0xA)[0];
        p[6] = Scr(0xC)[0];
        p[0x14] = 1;
        p[0x15] = 1;
        p[0x16] = 1;
        CommitAtSparkle(0x34);
        n += 8;
    } while (n < end);
}

// original 0x4B9490 (PSX MAGIC070 0x801EFA44): the same four rays as
// three-point lines - dark at the centre, bright (+5 * 6) at half the
// radius, dark at the tip.
extern "C" void __cdecl Sparkle_DrawRaysG3(unsigned start, unsigned radius) {
    const unsigned char* const c = Cur();
    SetWord(Scr(0), Word(c + at::kX));
    SetWord(Scr(2), Word(c + at::kY));
    DrawModeAtSparkle();
    RayShade();
    int n = static_cast<int>(start & 0xFFFF);
    const int end = n + 0x20;
    const int full = static_cast<int>(radius & 0xFFFF);
    const int half = full >> 1;
    do {
        unsigned char* const p = Gfx_PacketNext;
        g.set_line_g3(p);
        g.set_semi(p, 1);
        PutFloat(p + 8, S16(Scr(0)));
        SetWord(Scr(6), static_cast<unsigned>((n & 0x1F) << 7));
        PutFloat(p + 0xC, S16(Scr(2)));
        const int co = g.cos(S16(Scr(6)));
        PutFloat(p + 0x18, Mul12(co, half) + S16(Scr(0)));
        const int si = g.sin(S16(Scr(6)));
        PutFloat(p + 0x1C, Mul12(si, half) + S16(Scr(2)));
        const int co2 = g.cos(S16(Scr(6)));
        PutFloat(p + 0x28, Mul12(co2, full) + S16(Scr(0)));
        const int si2 = g.sin(S16(Scr(6)));
        const int y2 = Mul12(si2, full) + S16(Scr(2));
        p[4] = 1;
        p[5] = 1;
        p[6] = 1;
        PutFloat(p + 0x2C, y2);
        p[0x14] = Scr(8)[0];
        p[0x15] = Scr(0xA)[0];
        p[0x16] = Scr(0xC)[0];
        p[0x24] = 1;
        p[0x25] = 1;
        p[0x26] = 1;
        CommitAtSparkle(0x34);
        n += 8;
    } while (n < end);
}

// original 0x4B9680 (PSX MAGIC070 0x801EFCDC): the sparkle's body - eight
// gouraud triangles round its point, radius +6 + Rand & 3, the centre in the
// colour row (+3 + 4 * +4) of 0x65ADD4 times the brightness +5, the rim
// +5 grey.
extern "C" void __cdecl Sparkle_DrawDisc(void) {
    DrawModeAtSparkle();
    const int r = g.rand();
    const unsigned char* const c = Cur();
    SetWord(Scr(4), static_cast<unsigned>((r & 3) + c[at::kCount]));
    SetWord(Scr(0), Word(c + at::kX));
    SetWord(Scr(2), Word(c + at::kY));
    const unsigned char* const rgb = At(at::kSparkleColours + (c[at::kShade] + c[at::kKind] * 4u) * 3u);
    SetWord(Scr(8), rgb[0] * static_cast<unsigned>(c[at::kTimer]));
    SetWord(Scr(0xA), rgb[1] * static_cast<unsigned>(c[at::kTimer]));
    SetWord(Scr(0xC), rgb[2] * static_cast<unsigned>(c[at::kTimer]));
    SetWord(Scr(0xE), c[at::kTimer]);
    int a = 0;
    do {
        unsigned char* const p = Gfx_PacketNext;
        g.set_poly_g3(p);
        g.set_semi(p, 1);
        PutFloat(p + 8, S16(Scr(0)));
        PutFloat(p + 0xC, S16(Scr(2)));
        int s = g.sin(a);
        PutFloat(p + 0x18, Mul12(s, S16(Scr(4))) + S16(Scr(0)));
        int co = g.cos(a);
        PutFloat(p + 0x1C, Mul12(co, S16(Scr(4))) + S16(Scr(2)));
        a += 0x200;
        s = g.sin(a);
        PutFloat(p + 0x28, Mul12(s, S16(Scr(4))) + S16(Scr(0)));
        co = g.cos(a);
        PutFloat(p + 0x2C, Mul12(co, S16(Scr(4))) + S16(Scr(2)));
        p[4] = Scr(8)[0];
        p[5] = Scr(0xA)[0];
        p[6] = Scr(0xC)[0];
        for (unsigned k : {0x14u, 0x15u, 0x16u, 0x24u, 0x25u, 0x26u}) p[k] = Scr(0xE)[0];
        CommitAtSparkle(0x34);
    } while (a < 0x1000);
}

// original 0x4B98B0 (PSX MAGIC070 0x801F0000): the first of the 128 sparkles
// without bit 0, marked and its index answered; 0xFF when all are in use.
// Only al is defined in the original's answer (its upper bytes are the
// caller's eax); every caller keeps al.
extern "C" unsigned char __cdecl Sparkle_Alloc(void) {
    unsigned char i = 0;
    do {
        unsigned char* const s = At(at::kSparklePool + i * at::kSparkleStride);
        if ((s[at::kFlags] & 1) == 0) {
            s[at::kFlags] = static_cast<unsigned char>(s[at::kFlags] | 1);
            return i;
        }
        ++i;
    } while (i < at::kSparkles);
    return 0xFF;
}

// original 0x4B9900 (PSX MAGIC070 0x801F006C): bytes +0..+4 of the sparkle
// cleared, the pointer read again for each.
extern "C" void __cdecl Sparkle_Free(void) {
    for (unsigned k = 0; k < 5; ++k) Cur()[k] = 0;
}

// ===========================================================================
// The shared effect draws

// original 0x4AD6F0 (PSX Sacrifice / Fireblast 0x801EF9F8's shape): a fan of
// eight gouraud triangles of radius 0x120 round the origin of the current
// matrix (MagicFx_PushActorMatrix's), the centre +0xA * 12 grey, the rim
// dark. The PSX picks tpage 0x55 or 0x125 by the video mode; the port 0x55.
extern "C" void __cdecl MagicFx_DrawFan(void) {
    g.set_draw_mode(Gfx_PacketNext, 0, 1, 0x55, 0);
    g.commit(5, 0xC);
    const unsigned char* const sc = Sprite_Current;
    SetWord(Scr(0), 0x120);
    SetWord(Scr(0xA), sc[0xA] * 12u);
    int s = g.sin(0);
    SetWord(Vtx(0x10), static_cast<unsigned>(Mul12(s, S16(Scr(0)))));
    int co = g.cos(0);
    SetWord(Vtx(0x14), 0);
    SetWord(Vtx(0x12), static_cast<unsigned>(Mul12(co, S16(Scr(0)))));
    SetWord(Vtx(0xC), 0);
    SetWord(Vtx(4), 0);
    for (int a = 0x200; a < 0x1200; a += 0x200) {
        unsigned char* const p = Gfx_PacketNext;
        g.set_poly_g3(p);
        g.set_semi(p, 1);
        const std::uint16_t x = Word(Vtx(0x10)), y = Word(Vtx(0x12));
        SetWord(Vtx(0), 0);
        SetWord(Vtx(2), 0);
        SetWord(Vtx(8), x);
        SetWord(Vtx(0xA), y);
        s = g.sin(a);
        SetWord(Vtx(0x10), static_cast<unsigned>(Mul12(s, S16(Scr(0)))));
        co = g.cos(a);
        SetWord(Vtx(0x12), static_cast<unsigned>(Mul12(co, S16(Scr(0)))));
        long depth, flag;
        g.rtp3(VtxP(0), VtxP(8), VtxP(0x10), reinterpret_cast<float*>(p + 8), reinterpret_cast<float*>(p + 0x18),
               reinterpret_cast<float*>(p + 0x28), &depth, &flag);
        g.depths3(p);
        p[4] = Scr(0xA)[0];
        p[5] = Scr(0xA)[0];
        p[6] = Scr(0xA)[0];
        for (unsigned k : {0x14u, 0x15u, 0x16u, 0x24u, 0x25u, 0x26u}) p[k] = 1;
        g.commit(5, 0x34);
    }
    g.set_draw_mode(Gfx_PacketNext, 0, 1, 0x15, 0);
    g.commit(5, 0xC);
}

// original 0x4B7D40 (PSX Sacrifice 0x801EF700): pushes the matrix and loads
// Camera_Matrix x the actor's: translation RotTrans of (x >> 9 - 0x4000,
// z >> 9 - 0x4000, -(height / 2)) from Sprite_Current, no rotation. Pops
// nothing - its callers do. The MATRIX is one block (nine shorts, a pad, the
// translation at +0x14), as the original lays it out on its stack: RotTrans
// writes straight into its translation.
extern "C" void __cdecl MagicFx_PushActorMatrix(void) {
    g.push_matrix();
    const short rot[4] = {0, 0, 0, 0};
    const unsigned char* const sc = Sprite_Current;
    short v[4];
    v[0] = static_cast<short>((Long(sc + 0x34) >> 9) - 0x4000);
    v[1] = static_cast<short>((Long(sc + 0x38) >> 9) - 0x4000);
    v[2] = static_cast<short>(-(S16(sc + 0x3E) / 2));
    v[3] = 0;
    struct Matrix {
        short m[10];
        long t[3];
    } m;
    static_assert(offsetof(Matrix, t) == 0x14 && sizeof(Matrix) == 0x20, "MATRIX layout");
    long flag;
    g.rot_trans(v, m.t, &flag);
    g.rot_matrix(rot, m.m);
    g.mul_matrix0(Camera_Matrix, m.m, m.m);
    g.set_rot(reinterpret_cast<const unsigned long*>(&m));
    g.set_trans(reinterpret_cast<const unsigned long*>(&m));
}

// original 0x4C5150 (PSX Sacrifice 0x801EF240's loop): a band of 32 textured
// gouraud quads round the actor - inner radius +9 * 4 lifted by
// sin(((+0xB + i) & 0xF) << 8) of +0xA * 4 + 0x40, outer radius +9 * 3 on the
// ground - the texture a strip of page (0x340, 0x100), clut (0, 0x1FA), its
// columns i * 4 and rows (+0xB + 8) * 4 / (+0xB + 12) * 4, the rim shade
// 0x68 - +0xB * 3. Each quad sorted at the actor's position moved by its
// inner x << 9 on both axes, as on the PSX. Tpage 0x35 where the PSX picks
// 0x55 or 0x125.
extern "C" void __cdecl MagicFx_DrawRing(void) {
    const unsigned char* sc = Sprite_Current;
    SetWord(Scr(0), sc[9] * 4u);
    SetWord(Scr(4), sc[9] * 3u);
    SetWord(Scr(6), sc[0xA] * 4u + 0x40);
    SetWord(Scr(8), static_cast<unsigned>((sc[0xB] & 0xF) << 8));
    int v = g.cos(0);
    SetWord(Vtx(8), static_cast<unsigned>(Mul12(v, S16(Scr(0)))));
    v = g.sin(0);
    SetWord(Vtx(0xA), static_cast<unsigned>(Mul12(v, S16(Scr(0)))));
    v = g.sin(S16(Scr(8)));
    {
        const std::uint16_t lift = Word(Scr(6));
        SetWord(Vtx(0xC), static_cast<unsigned>(Mul12(v, static_cast<short>(lift))) - lift);
    }
    v = g.cos(0);
    SetWord(Vtx(0x18), static_cast<unsigned>(Mul12(v, S16(Scr(4)))));
    v = g.sin(0);
    SetWord(Vtx(0x1C), 0);
    SetWord(Vtx(0x1A), static_cast<unsigned>(Mul12(v, S16(Scr(4)))));
    unsigned i = 1;
    for (int a = 0x80; a < 0x1080; a += 0x80, ++i) {
        sc = Sprite_Current;
        SetWord(Vtx(0), Word(Vtx(8)));
        SetWord(Scr(8), static_cast<unsigned>(((sc[0xB] + static_cast<unsigned char>(i)) & 0xF) << 8));
        SetWord(Vtx(2), Word(Vtx(0xA)));
        SetWord(Vtx(4), Word(Vtx(0xC)));
        v = g.cos(a);
        SetWord(Vtx(8), static_cast<unsigned>(Mul12(v, S16(Scr(0)))));
        v = g.sin(a);
        SetWord(Vtx(0xA), static_cast<unsigned>(Mul12(v, S16(Scr(0)))));
        v = g.sin(S16(Scr(8)));
        {
            const std::uint16_t lift = Word(Scr(6));
            SetWord(Vtx(0xC), static_cast<unsigned>(Mul12(v, static_cast<short>(lift))) - lift);
        }
        SetWord(Vtx(0x10), Word(Vtx(0x18)));
        SetWord(Vtx(0x12), Word(Vtx(0x1A)));
        SetWord(Vtx(0x14), 0);
        v = g.cos(a);
        SetWord(Vtx(0x18), static_cast<unsigned>(Mul12(v, S16(Scr(4)))));
        v = g.sin(a);
        sc = Sprite_Current;
        unsigned char* p = Gfx_PacketNext;
        SetWord(Vtx(0x1A), static_cast<unsigned>(Mul12(v, S16(Scr(4)))));
        const std::uint32_t lean = static_cast<std::uint32_t>(static_cast<int>(S16(Vtx(8)))) << 9;
        SetWord(Vtx(0x1C), 0);
        const unsigned x = static_cast<std::uint32_t>(Long(sc + 0x34)) + lean;
        const unsigned z = static_cast<std::uint32_t>(Long(sc + 0x38)) + lean;
        g.set_draw_mode(p, 0, 1, 0x35, 0);
        g.commit_sorted(x, z, 2, 0xC);
        p = Gfx_PacketNext;
        g.set_poly_gt4(p);
        g.set_semi(p, 1);
        SetWord(p + 0x2A, g.get_tpage(0, 1, 0x340, 0x100));
        SetWord(p + 0x16, g.get_clut(0, 0x1FA));
        const auto u0 = static_cast<unsigned char>(i << 2);
        const auto u1 = static_cast<unsigned char>((i + 1) << 2);
        p[0x14] = u0;
        p[0x15] = static_cast<unsigned char>((Sprite_Current[0xB] + 8) << 2);
        p[0x28] = u1;
        p[0x29] = static_cast<unsigned char>((Sprite_Current[0xB] + 8) << 2);
        p[0x3C] = u0;
        p[0x50] = u1;
        p[0x3D] = static_cast<unsigned char>((Sprite_Current[0xB] + 0xC) << 2);
        p[0x51] = static_cast<unsigned char>((Sprite_Current[0xB] + 0xC) << 2);
        SetWord(Scr(0xA), 0x68u - Sprite_Current[0xB] * 3u);
        for (unsigned k : {4u, 5u, 6u, 0x18u, 0x19u, 0x1Au}) p[k] = 1;
        for (unsigned k : {0x2Cu, 0x2Du, 0x2Eu, 0x40u, 0x41u, 0x42u}) p[k] = Scr(0xA)[0];
        long depth, flag;
        g.rtp4(VtxP(0), VtxP(8), VtxP(0x10), VtxP(0x18), reinterpret_cast<float*>(p + 8),
               reinterpret_cast<float*>(p + 0x1C), reinterpret_cast<float*>(p + 0x30), reinterpret_cast<float*>(p + 0x44),
               &depth, &flag);
        g.depths4(p);
        g.commit_sorted(x, z, 2, 0x54);
    }
}

// original 0x4C54F0 (PSX Sacrifice 0x801EF7B0): eight gouraud triangles round
// the actor's screen point (+0x2E / +0x30), radius +9 * 2 + 0x20, the centre
// red +9 * 10, each sorted at the actor's position. Tpage 0x35 where the
// PSX picks 0x35 or 0xA5.
extern "C" void __cdecl MagicFx_DrawDisc(void) {
    SetWord(Scr(0), Sprite_Current[9] * 2u + 0x20);
    int a = 0;
    do {
        g.set_draw_mode(Gfx_PacketNext, 0, 1, 0x35, 0);
        const unsigned char* sc = Sprite_Current;
        g.commit_sorted(static_cast<unsigned>(Long(sc + 0x34)), static_cast<unsigned>(Long(sc + 0x38)), 2, 0xC);
        unsigned char* const p = Gfx_PacketNext;
        g.set_poly_g3(p);
        g.set_semi(p, 1);
        PutFloat(p + 8, S16(Sprite_Current + 0x2E));
        PutFloat(p + 0xC, S16(Sprite_Current + 0x30));
        int v = g.sin(a);
        PutFloat(p + 0x18, Mul12(v, S16(Scr(0))) + S16(Sprite_Current + 0x2E));
        v = g.cos(a);
        PutFloat(p + 0x1C, Mul12(v, S16(Scr(0))) + S16(Sprite_Current + 0x30));
        a += 0x200;
        v = g.sin(a);
        PutFloat(p + 0x28, Mul12(v, S16(Scr(0))) + S16(Sprite_Current + 0x2E));
        v = g.cos(a);
        PutFloat(p + 0x2C, Mul12(v, S16(Scr(0))) + S16(Sprite_Current + 0x30));
        p[4] = static_cast<unsigned char>(Sprite_Current[9] * 10u);
        for (unsigned k : {5u, 6u, 0x14u, 0x15u, 0x16u, 0x24u, 0x25u, 0x26u}) p[k] = 1;
        sc = Sprite_Current;
        g.commit_sorted(static_cast<unsigned>(Long(sc + 0x34)), static_cast<unsigned>(Long(sc + 0x38)), 2, 0x34);
    } while (a < 0x1000);
}

// ===========================================================================
// The battle actor's helpers (BATTLE.EMI section 15 on the PSX)

// original 0x4FB830 (PSX 0x800ABEC0): with Sprite_Current the effect's actor
// (0x93B940) for the call and put back after, a party member (0x904B34 below
// 3) gets Sprite_SetAnimation(its +8 + offset), an enemy 0x435A20(the index,
// arg). As the original has it: the animation byte goes in the low byte of
// the actor's own pointer, pushed whole; the enemy's index in the low byte of
// the caller's ecx (0x435A20 reads only that byte).
extern "C" void __cdecl BattleActor_SetAnimation(unsigned offset, unsigned arg) {
    const unsigned char index = At(at::kActorIndex)[0];
    unsigned char* const actor = Actor();
    unsigned char* const saved = Sprite_Current;
    Sprite_Current = actor;
    if (index < 3) g.set_animation(AnimationInPointer(actor, offset));
    else g.enemy_animation(index, arg);
    Sprite_Current = saved;
}

// original 0x4FBD10 (PSX 0x800AC68C): Sprite_Current's screen point. The
// actor's (x >> 9 - 0x4000, z >> 9 - 0x4000, -(height / 2)) projected into a
// TILE_1 at Gfx_PacketNext - never committed, used for its vertex - and the
// float x / y truncated (the CRT's _ftol) to +0x2E / +0x30. The PSX reads the
// integer screen point back the same way.
extern "C" void __cdecl BattleActor_UpdateScreenXY(void) {
    const unsigned char* const sc = Sprite_Current;
    unsigned char* const p = Gfx_PacketNext;
    short v[4];
    v[0] = static_cast<short>((Long(sc + 0x34) >> 9) - 0x4000);
    v[1] = static_cast<short>((Long(sc + 0x38) >> 9) - 0x4000);
    v[2] = static_cast<short>(-(S16(sc + 0x3E) / 2));
    v[3] = 0;
    g.set_tile1(p);
    long depth, flag;
    g.rtp(v, reinterpret_cast<float*>(p + 8), &depth, &flag);
    g.store_depth(reinterpret_cast<float*>(p + 0x10));
    SetWord(Sprite_Current + 0x2E, Ftol16(static_cast<std::uint32_t>(Long(p + 8))));
    SetWord(Sprite_Current + 0x30, Ftol16(static_cast<std::uint32_t>(Long(p + 0xC))));
}

// original 0x4FBDB0 (PSX 0x800AC73C): unless actor `index` is out
// (0x4456C0), a party member with +0x90 bit 7 or an enemy with +0x92 bit 7
// is tinted (-6, -10, 0, 0).
extern "C" void __cdecl BattleActor_Flash(unsigned index) {
    if (g.is_out(index) != 0) return;
    const unsigned i = index & 0xFF;
    if (i < 3) {
        unsigned char* const r = At(at::kPartyRecords + i * at::kPartyStride);
        if (r[0x90] & 0x80) g.set_tint(r, 0xFFFFFFFAu, 0xFFFFFFF6u, 0, 0);
        return;
    }
    unsigned char* const r = At(at::kEnemyRecords + (i - 3) * at::kEnemyStride);
    if (r[0x92] & 0x80) g.set_tint(r, 0xFFFFFFFAu, 0xFFFFFFF6u, 0, 0);
}

// original 0x4FC030 (PSX 0x800ACBC0): the acting actor's sound. A party
// member becomes Field_State, then its sound `first` + 1 (unless first is 0)
// and `second` (unless 0) through 0x446A50; an enemy plays its record's cue
// word (+0xF8's) through 0x437450 when 0x904AAA is set, else Sound_PlayById
// 0x600 + 2 * its type byte +0xF0. The arguments' low bytes are what is read.
extern "C" void __cdecl BattleActor_PlaySound(unsigned first, unsigned second) {
    const unsigned char index = At(at::kActorIndex)[0];
    if (index < 3) {
        Field_State = At(at::kPartyRecords + index * at::kPartyStride);
        const auto a = static_cast<unsigned char>(first);
        if (a != 0) g.actor_sound(static_cast<unsigned char>(a + 1));
        if (static_cast<unsigned char>(second) != 0) g.actor_sound(second);
        return;
    }
    const unsigned char* const r = At(at::kEnemyRecords + (index - 3u) * at::kEnemyStride);
    if (At(at::kEnemySoundMode)[0] != 0) {
        const auto* const cue = reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(Long(r + 0xF8)));
        g.enemy_sound(Word(cue));
    } else {
        g.play_by_id(r[0xF0] * 2u + 0x600);
    }
}

// original 0x4FC1F0: the acting actor's effect size (callers store it to
// Sprite_Current +9, which the effect draws scale by). A party member: the
// byte of 0x65C3F8 by 0x904B89 when its +0x134 has bit 1, else of 0x65C3EC by
// its +0x89; an enemy: the byte at 0x8C5652 + 0x8C * its type. Only al is
// defined in the original's answer; all ten callers store al.
extern "C" unsigned char __cdecl BattleActor_FxSize(void) {
    const unsigned char index = At(at::kActorIndex)[0];
    if (index < 3) {
        const unsigned char* const r = At(at::kPartyRecords + index * at::kPartyStride);
        if (r[0x134] & 2) return At(at::kFxSizeAltTable)[At(at::kFxSizeSelect)[0]];
        return At(at::kFxSizeTable)[r[0x89]];
    }
    const unsigned char type = At(at::kEnemyRecords + (index - 3u) * at::kEnemyStride + 0xF0)[0];
    return At(at::kEnemyFxSize + type * 0x8Cu)[0];
}

// ===========================================================================
// The SND stream

// original 0x5A7140: nothing without Snd_Device. Else the playing stream
// stopped, a buffer made of the WAV image and kept at 0x7DE3C8,
// SetCurrentPosition(0) and Play; a lost buffer (DSERR_BUFFERLOST) gets the
// wave's data written again (SndBuf_Write restores it) and a second Play.
extern "C" void __cdecl SndStream_Play(const unsigned char* wave) {
    if (Snd_Device == nullptr) return;
    g.stream_stop();
    void* const buffer = g.create_from_wave(wave);
    StreamBuffer() = buffer;
    if (buffer == nullptr) return;
    Method<long (__stdcall*)(void*, unsigned long)>(buffer, 0x34)(buffer, 0);
    void* b = StreamBuffer();
    if (Method<long (__stdcall*)(void*, unsigned long, unsigned long, unsigned long)>(b, 0x30)(b, 0, 0, 0) != kBufferLost)
        return;
    const unsigned char* data = nullptr;
    const unsigned size = g.find_data(wave, &data);
    g.buffer_write(StreamBuffer(), data, 0, size);
    b = StreamBuffer();
    Method<long (__stdcall*)(void*, unsigned long, unsigned long, unsigned long)>(b, 0x30)(b, 0, 0, 0);
}

// original 0x5A71C0: the stream's buffer, if any, stopped when GetStatus says
// it plays (bit 0), released, and forgotten. The original's status word is
// an uninitialised local that GetStatus fills; ours starts at 0.
extern "C" void __cdecl SndStream_Stop(void) {
    void* b = StreamBuffer();
    if (b == nullptr) return;
    unsigned long status = 0;
    Method<long (__stdcall*)(void*, unsigned long*)>(b, 0x24)(b, &status);
    if (status & 1) {
        b = StreamBuffer();
        Method<long (__stdcall*)(void*)>(b, 0x48)(b);
    }
    b = StreamBuffer();
    Method<unsigned long (__stdcall*)(void*)>(b, 8)(b);
    StreamBuffer() = nullptr;
}

// ===========================================================================

void BattleItems_Inject() {
    if (bof3::WantsShadow("battle_items")) battle_items::SelfTest();
    BOF3_INJECT(Gpu_SetPolyG3);
    BOF3_INJECT(Gpu_SetLineG2);
    BOF3_INJECT(Gpu_SetLineG3);
    BOF3_INJECT(Gpu_SetTile1);
    BOF3_INJECT(Sparkle_Dispatch);
    BOF3_INJECT(Sparkle_Launch);
    BOF3_INJECT(Sparkle_Rise);
    BOF3_INJECT(Sparkle_Fade);
    BOF3_INJECT(Sparkle_DrawRaysG2);
    BOF3_INJECT(Sparkle_DrawRaysG3);
    BOF3_INJECT(Sparkle_DrawDisc);
    BOF3_INJECT(Sparkle_Alloc);
    BOF3_INJECT(Sparkle_Free);
    BOF3_INJECT(MagicFx_DrawFan);
    BOF3_INJECT(MagicFx_PushActorMatrix);
    BOF3_INJECT(MagicFx_DrawRing);
    BOF3_INJECT(MagicFx_DrawDisc);
    BOF3_INJECT(BattleActor_SetAnimation);
    BOF3_INJECT(BattleActor_UpdateScreenXY);
    BOF3_INJECT(BattleActor_Flash);
    BOF3_INJECT(BattleActor_PlaySound);
    BOF3_INJECT(BattleActor_FxSize);
    BOF3_INJECT(SndStream_Play);
    BOF3_INJECT(SndStream_Stop);
}

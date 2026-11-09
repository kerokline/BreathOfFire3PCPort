// Moved here by tools/move_funcs.py from magic_s21.cpp (docs/source-layout-round.md).
#include "game/magic/iceblast.h"

#include <cstdint>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s21.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/records_gpu.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = magic_harness::at;

}  // namespace

namespace {

using magic_harness::Mem;

using magic_harness::Pointer;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

// The PC's copy of the PSX scratchpad (SharedScratch) and Prim_VertexScratch:
// four SVECTORs 8 bytes apart, A0 / A8 / B0 / B8.
constexpr ::bof3::image::Va kScratch{SharedScratch_addr};

// kVertex: now Prim_VertexScratch (symbols.toml).
unsigned char* ScrMagicS21(unsigned off) { return Mem(::bof3::Img(kScratch.v) + off); }

unsigned char* Vtx(unsigned off) { return Mem(::bof3::Img(Prim_VertexScratch_addr) + off); }

const short* VtxP(unsigned off) { return reinterpret_cast<const short*>(Vtx(off)); }

short S16(const unsigned char* p) { return static_cast<short>(Word(p)); }

std::int32_t L(std::uint32_t address) { return Long(Mem(address)); }

// Unnamed callees, called by their addresses, never bound (S37's
// MagicFx_FreeCurrentRecord, a pool slot's free, is ours now and by name):
//   0x5A76F0  a POLY_G4-shaped setter, code 0x5C, the four z at +0x10 ..;
//   0x5A7570  POLY_F3's setter, code 0x20, the three z at +0x10, +0x1C, +0x28.
constexpr ::bof3::image::CodeVa kSetPoly5C{bof3::addr::Gpu_SetLineG4};

constexpr ::bof3::image::CodeVa kSetPolyF3{bof3::addr::Gpu_SetPolyF3};

using PrimFn = void (__cdecl*)(unsigned char*);

// imul r32, r/m32: the low 32 bits, then an arithmetic shift.
std::int32_t Mul(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b));
}

void BumpMagicS21(unsigned char& b, int d = 1) { b = static_cast<unsigned char>(b + d); }

unsigned char U8(int v) { return static_cast<unsigned char>(v); }

float* F(unsigned char* prim, unsigned off) { return reinterpret_cast<float*>(prim + off); }

std::uint32_t Addr(const void* p) { return Low32(p); }

// An actor's record as the originals index them: 0..2 the party (stride
// 0x14C), else an enemy (index - 3, stride 0x128), none of it checked.
unsigned char* PartyRecord(unsigned i) { return Mem(::bof3::Img(ObjTrio_addr) + i * at::kPartyStride); }

unsigned char* EnemyRecord(unsigned i) { return Mem(::bof3::Img(Battle_Enemies_addr) + i * at::kEnemyStride); }

// The target's record by its byte: the original reads the byte, then the
// dword at 0x904B44 again and indexes by its low byte.
unsigned char* TargetRecord() {
    const unsigned char t = Mem(::bof3::Img(at::kTarget))[0];
    const unsigned low = static_cast<std::uint32_t>(L(::bof3::Img(at::kTarget))) & 0xFF;
    return t < 3 ? PartyRecord(low) : EnemyRecord(low - 3);
}

void CallCell(std::uint32_t table, unsigned index) {
    reinterpret_cast<magic_harness::Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(L(table + 4 * index))))();
}

// The link of a primitive at the task's position: MapView_LinkPrimAt(+0x34,
// +0x38, slot, size), Sprite_Current read once.
void LinkAtTask(int slot, unsigned size) {
    const unsigned char* const sc = Sprite_Current;
    const records::BattleTask* const tsc = rv::AsConst<records::BattleTask>(sc);
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(tsc->x), static_cast<unsigned long>(tsc->z),
                                slot, size);
}

}  // namespace

#define MS21_EXPORT extern "C" __attribute__((disable_tail_calls))

// The spires' shared column: vertex A0 at the jitter byte j of row i (radius
// 0x100 - 41 i + j[0], height -72 i - j[0]) and A8 one row up (radius 0xD7 -
// 41 i + j[8], height -72 (i + 1) - j[8]), at angle a; the top row (i 5)
// closes to the axis (A8 x, z 0). The scratch dwords 0x903850 (the radius)
// and 0x903854 (the row's base) as the original leaves them.
struct Spire {
    int base0, base1, h0, h1;
};

static Spire SpireRow(int i) {
    const int b = -41 * i;
    return {b + 0x100, b + 0xD7, -72 * i, -72 * (i + 1)};
}

static void SpireColumn(int i, const Spire& s, int a, unsigned idx) {
    SetLong(ScrMagicS21(4), s.base0);
    SetLong(ScrMagicS21(0), static_cast<signed char>(Mem(::bof3::Img(Iceblast_Jitter_addr) + idx)[0]) + s.base0);
    SetWord(Vtx(0), static_cast<unsigned>(Mul(MH_CALL(Math_Sin)(a), L(::bof3::Img(kScratch.v))) >> 12));
    SetWord(Vtx(2), static_cast<unsigned>(Mul(MH_CALL(Math_Cos)(a), L(::bof3::Img(kScratch.v))) >> 12));
    SetWord(Vtx(4), static_cast<unsigned>(s.h0 - static_cast<signed char>(Mem(::bof3::Img(Iceblast_Jitter_addr) + idx)[0])));
    SetLong(ScrMagicS21(4), s.base1);
    SetLong(ScrMagicS21(0), static_cast<signed char>(Mem(::bof3::Img(Iceblast_Jitter_addr) + idx + 8)[0]) + s.base1);
    SetWord(Vtx(8), static_cast<unsigned>(Mul(MH_CALL(Math_Sin)(a), L(::bof3::Img(kScratch.v))) >> 12));
    const int c = MH_CALL(Math_Cos)(a);
    const signed char j1 = static_cast<signed char>(Mem(::bof3::Img(Iceblast_Jitter_addr) + idx + 8)[0]);
    SetWord(Vtx(0xA), static_cast<unsigned>(Mul(c, L(::bof3::Img(kScratch.v))) >> 12));
    SetWord(Vtx(0xC), static_cast<unsigned>(s.h1 - j1));
    if (i == 5) {
        SetWord(Vtx(0xA), 0);
        SetWord(Vtx(8), 0);
    }
}

// original 0x4C7CD0 (Magic_Rows row 22): the kind-2 task under the actor's
// matrix, its phase +1 through a five-entry stack table - Iceblast_Start,
// _Chill, _Rise, _Sink, _End. Ours aborts past 4.
MS21_EXPORT void __cdecl Iceblast_Task(void) {
    static constexpr std::uint32_t kPhases[5] = {bof3::addr::Iceblast_Start, bof3::addr::Iceblast_Chill,
                                                 bof3::addr::Iceblast_Rise, bof3::addr::Iceblast_Sink,
                                                 bof3::addr::Iceblast_End};
    MH_CALL(MagicFx_PushActorMatrix)();
    const unsigned phase = Sprite_Current[1];
    if (phase >= 5) bof3::Fatal("Iceblast_Task: phase %u, past the five-entry table", phase);
    magic_harness::Phase(kPhases[phase])();
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4C7D20: the source sprite's (read once) screen point and
// position to the task; the 56 jitter bytes at 0x695958 each Rand & 0xF,
// negated when a first Rand is odd; twelve shard children (kind 1, 0x17)
// owned by it with the source's actor byte and the task's position, +9
// 0x30 - 4 i (the delay), +0xA 0, +0xB i; sound 0x100; +9, +0xA 0; on.
MS21_EXPORT void __cdecl Iceblast_Start(void) {
    const unsigned char* const src = Pointer(::bof3::Img(BattleOdds_ActorObject_addr));
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->screen_x, static_cast<std::uint16_t>(rv::As<records::SpriteHead>(src)->screen_x));
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->screen_y, static_cast<std::uint16_t>(rv::As<records::SpriteHead>(src)->screen_y));
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(src)->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(src)->z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(src)->height);
    for (unsigned i = 0; i < 0x38; ++i) {
        unsigned char v;
        if (MH_CALL(Rand)() & 1)
            v = U8(-(MH_CALL(Rand)() & 0xF));
        else
            v = U8(MH_CALL(Rand)() & 0xF);
        Mem(::bof3::Img(Iceblast_Jitter_addr) + i)[0] = v;
    }
    for (unsigned i = 0; i < 12; ++i) {
        const unsigned k = MH_CALL(BattleTask_Create)(1, 0x17);
        unsigned char* const child = Mem(::bof3::Img(BattleTask_Slots_addr) + k * at::kTaskStride);
        const unsigned char* const sc = Sprite_Current;
        const records::BattleTask* const tsc = rv::AsConst<records::BattleTask>(sc);
        SetLong(child + 0x80, static_cast<std::int32_t>(Addr(sc)));
        child[8] = src[8];
        SetLong(child + 0x34, tsc->x);
        SetLong(child + 0x38, tsc->z);
        rv::Store(rv::As<records::BattleTask>(child)->height, tsc->height);
        child[9] = U8(0x30 - U8(i << 2));
        child[0xA] = 0;
        child[0xB] = U8(i);
    }
    MH_CALL(Sound_PlayById)(0x100);
    Sprite_Current[9] = 0;
    Sprite_Current[0xA] = 0;
    BumpMagicS21(Sprite_Current[1]);
}

// original 0x4C7E30: the disc; at +0xA 0xC the target's tint released and
// set to (6, 4, 8, 0) and its flags 0x200; +0xA up, at 0x10 the phase on.
// The task is read again only after the tint.
MS21_EXPORT void __cdecl Iceblast_Chill(void) {
    MH_CALL(Iceblast_DrawDisc)();
    unsigned char* sc = Sprite_Current;
    if (sc[0xA] == 0xC) {
        unsigned char* const r = TargetRecord();
        MH_CALL(Sprite_ReleaseTint)(r);
        MH_CALL(Sprite_SetTint)(r, 6, 4, 8, 0);
        MH_CALL(Battle_SetTargetFlags)(Mem(::bof3::Img(at::kTarget))[0], 0x200);
        sc = Sprite_Current;
    }
    BumpMagicS21(sc[0xA]);
    unsigned char* const s2 = Sprite_Current;
    if (s2[0xA] == 0x10) BumpMagicS21(s2[1]);
}

// original 0x4C7EC0: the spires, the inner spires, the disc; +9 up, at 0x40
// sounds 0x101 and 0x102, +9 0x30, the phase on.
MS21_EXPORT void __cdecl Iceblast_Rise(void) {
    MH_CALL(Iceblast_DrawSpires)();
    MH_CALL(Iceblast_DrawSpiresInner)();
    MH_CALL(Iceblast_DrawDisc)();
    BumpMagicS21(Sprite_Current[9]);
    if (Sprite_Current[9] != 0x40) return;
    MH_CALL(Sound_PlayById)(0x101);
    MH_CALL(Sound_PlayById)(0x102);
    Sprite_Current[9] = 0x30;
    BumpMagicS21(Sprite_Current[1]);
}

// original 0x4C7F20: the three draws; +9 down, at 0 the phase on.
MS21_EXPORT void __cdecl Iceblast_Sink(void) {
    MH_CALL(Iceblast_DrawSpires)();
    MH_CALL(Iceblast_DrawSpiresInner)();
    MH_CALL(Iceblast_DrawDisc)();
    BumpMagicS21(Sprite_Current[9], -1);
    unsigned char* const sc = Sprite_Current;
    if (sc[9] == 0) BumpMagicS21(sc[1]);
}

// original 0x4C7F50: the disc; +0xA down, at 0 the target's tint released,
// it flashed, its flag 0x40, the done flag, free.
MS21_EXPORT void __cdecl Iceblast_End(void) {
    MH_CALL(Iceblast_DrawDisc)();
    BumpMagicS21(Sprite_Current[0xA], -1);
    if (Sprite_Current[0xA] != 0) return;
    MH_CALL(Sprite_ReleaseTint)(TargetRecord());
    MH_CALL(BattleActor_Flash)(Mem(::bof3::Img(at::kTarget))[0]);
    MH_CALL(Battle_SetTargetFlag40)(Mem(::bof3::Img(at::kTarget))[0]);
    Mem(::bof3::Img(at::kFlags))[0] = static_cast<unsigned char>(Mem(::bof3::Img(at::kFlags))[0] | 4);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4C7FE0: six rows of eight quads (a POLY_G4-shaped primitive,
// 0x5A76F0's code 0x5C) round the actor, each row's column at angle 0x200 k
// jittered by the row's bytes (k & 7); the quad's far edge the column before
// it, swapped (B0 = the old A8, B8 = the old A0). Shade by row: (+9 - 7 i) *
// 6 at most 0x30, nothing drawn at 0 or below; odd columns lit on the near
// edge (A0 / B0), even on the far (A8 / B8), the rest (1, 1, 1).
MS21_EXPORT void __cdecl Iceblast_DrawSpires(void) {
    int i = 0;
    do {
        const Spire s = SpireRow(i);
        SpireColumn(i, s, 0, static_cast<unsigned>(i * 8));
        int k = 1;
        for (int a = 0x200; a < 0x1200; a += 0x200, ++k) {
            const std::uint16_t ax = Word(Vtx(8)), az = Word(Vtx(0xA)), ah = Word(Vtx(0xC));
            SetWord(Vtx(0x14), ah);
            const std::uint16_t cz = Word(Vtx(2));
            SetWord(Vtx(0x10), ax);
            const std::uint16_t ch = Word(Vtx(4));
            SetWord(Vtx(0x1A), cz);
            SetWord(Vtx(0x12), az);
            const std::uint16_t cx = Word(Vtx(0));
            SetWord(Vtx(0x1C), ch);
            SetWord(Vtx(0x18), cx);
            SpireColumn(i, s, a, static_cast<unsigned>((k & 7) + i * 8));
            const unsigned char* const sc = Sprite_Current;
            const records::BattleTask* const tsc = rv::AsConst<records::BattleTask>(sc);
            const unsigned long x = static_cast<unsigned long>((static_cast<std::uint32_t>(S16(Vtx(0))) << 9) +
                                                               static_cast<std::uint32_t>(tsc->x));
            const unsigned long z = static_cast<unsigned long>((static_cast<std::uint32_t>(S16(Vtx(2))) << 9) +
                                                               static_cast<std::uint32_t>(tsc->z));
            MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x35, 0);
            MH_CALL(MapView_LinkPrimAt)(x, z, 0, 0xC);
            unsigned char* const p = Gfx_PacketNext;
            MH_AT(PrimFn, kSetPoly5C.v)(p);
            MH_CALL(Gpu_SetSemiTrans)(p, 1);
            long depth;
            MH_CALL(Gte_RotTransPers4)(VtxP(0), VtxP(8), VtxP(0x10), VtxP(0x18), F(p, 8), F(p, 0x18), F(p, 0x28), F(p, 0x38),
                                       &depth);
            MH_CALL(Gte_PrimDepths4_10C)(p);
            int shade = Sprite_Current[9] - 7 * i;
            if (shade <= 0) continue;
            shade *= 6;
            SetLong(ScrMagicS21(8), shade);
            if (shade > 0x30) {
                shade = 0x30;
                SetLong(ScrMagicS21(8), 0x30);
            }
            if (k & 1) {
                p[4] = U8(shade);
                p[5] = ScrMagicS21(8)[0];
                p[6] = ScrMagicS21(8)[0];
                p[0x14] = 1;
                p[0x15] = 1;
                p[0x16] = 1;
                p[0x24] = ScrMagicS21(8)[0];
                p[0x25] = ScrMagicS21(8)[0];
                p[0x26] = ScrMagicS21(8)[0];
                p[0x34] = 1;
                p[0x35] = 1;
                p[0x36] = 1;
            } else {
                p[4] = 1;
                p[5] = 1;
                p[6] = 1;
                p[0x14] = ScrMagicS21(8)[0];
                p[0x15] = ScrMagicS21(8)[0];
                p[0x16] = ScrMagicS21(8)[0];
                p[0x24] = 1;
                p[0x25] = 1;
                p[0x26] = 1;
                p[0x34] = ScrMagicS21(8)[0];
                p[0x35] = ScrMagicS21(8)[0];
                p[0x36] = ScrMagicS21(8)[0];
            }
            MH_CALL(MapView_LinkPrimAt)(x, z, 0, 0x44);
        }
        ++i;
    } while (i < 6);
}

// original 0x4C83E0: the same rows as Iceblast_DrawSpires drawn as POLY_G4
// (Gpu_SetPolyG4, Gte_PrimDepths4_10B), the far edge the column before it
// unswapped (B0 = the old A0, B8 = the old A8); shade d = +9 - 8 i: nothing
// at 0 or below, below 5 the blue of every vertex 48 d, else the first and
// third (48 d - 192 at most 0xC0) with the others' 0xC0.
MS21_EXPORT void __cdecl Iceblast_DrawSpiresInner(void) {
    int i = 0;
    do {
        const Spire s = SpireRow(i);
        SpireColumn(i, s, 0, static_cast<unsigned>(i * 8));
        int k = 1;
        for (int a = 0x200; a < 0x1200; a += 0x200, ++k) {
            const std::uint16_t fh = Word(Vtx(0xC)), fz = Word(Vtx(0xA));
            const std::uint16_t nx = Word(Vtx(0));
            SetWord(Vtx(0x1C), fh);
            SetWord(Vtx(0x10), nx);
            SetWord(Vtx(0x12), Word(Vtx(2)));
            SetWord(Vtx(0x14), Word(Vtx(4)));
            const std::uint16_t fx = Word(Vtx(8));
            SetWord(Vtx(0x18), fx);
            SetWord(Vtx(0x1A), fz);
            SpireColumn(i, s, a, static_cast<unsigned>((k & 7) + i * 8));
            const unsigned char* const sc = Sprite_Current;
            const records::BattleTask* const tsc = rv::AsConst<records::BattleTask>(sc);
            const unsigned long x = static_cast<unsigned long>((static_cast<std::uint32_t>(S16(Vtx(0))) << 9) +
                                                               static_cast<std::uint32_t>(tsc->x));
            const unsigned long z = static_cast<unsigned long>((static_cast<std::uint32_t>(S16(Vtx(2))) << 9) +
                                                               static_cast<std::uint32_t>(tsc->z));
            MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x35, 0);
            MH_CALL(MapView_LinkPrimAt)(x, z, 0, 0xC);
            unsigned char* const p = Gfx_PacketNext;
            MH_CALL(Gpu_SetPolyG4)(p);
            MH_CALL(Gpu_SetSemiTrans)(p, 1);
            long depth;
            MH_CALL(Gte_RotTransPers4)(VtxP(0), VtxP(8), VtxP(0x10), VtxP(0x18), F(p, 8), F(p, 0x18), F(p, 0x28), F(p, 0x38),
                                       &depth);
            MH_CALL(Gte_PrimDepths4_10B)(p);
            const int d = Sprite_Current[9] - (i << 3);
            if (d <= 0) continue;
            if (d < 5) {
                SetLong(ScrMagicS21(8), d * 48);
                if (d * 48 > 0xC0) SetLong(ScrMagicS21(8), 0xC0);   // the original's compare; d <= 4 never meets it
                p[4] = 1;
                p[5] = 1;
                p[0x14] = 1;
                p[6] = ScrMagicS21(8)[0];
                p[0x15] = 1;
                p[0x16] = 1;
                p[0x24] = 1;
                p[0x25] = 1;
                p[0x26] = ScrMagicS21(8)[0];
                p[0x34] = 1;
                p[0x35] = 1;
                p[0x36] = 1;
            } else {
                const int v = (d * 3 - 0xC) << 4;
                SetLong(ScrMagicS21(8), v);
                if (v > 0xC0) SetLong(ScrMagicS21(8), 0xC0);
                p[4] = 1;
                p[5] = 1;
                p[6] = 0xC0;
                p[0x14] = 1;
                p[0x15] = 1;
                p[0x16] = ScrMagicS21(8)[0];
                p[0x24] = 1;
                p[0x25] = 1;
                p[0x26] = 0xC0;
                p[0x34] = 1;
                p[0x35] = 1;
                p[0x36] = ScrMagicS21(8)[0];
            }
            MH_CALL(MapView_LinkPrimAt)(x, z, 0, 0x44);
        }
        ++i;
    } while (i < 6);
}

// original 0x4C87E0: a disc of sixteen gouraud triangles, radius 0x1C0 (dword
// 0x903850), round the origin; the centre (+0xA * 8) grey, the rim (1, 1,
// 1); each linked at the task's position.
MS21_EXPORT void __cdecl Iceblast_DrawDisc(void) {
    SetLong(ScrMagicS21(0), 0x1C0);
    SetWord(Vtx(0x10), static_cast<unsigned>(Mul(MH_CALL(Math_Sin)(0), L(::bof3::Img(kScratch.v))) >> 12));
    SetWord(Vtx(0x12), static_cast<unsigned>(Mul(MH_CALL(Math_Cos)(0), L(::bof3::Img(kScratch.v))) >> 12));
    for (int a = 0x100; a < 0x1100; a += 0x100) {
        const std::uint16_t y = Word(Vtx(0x12)), x = Word(Vtx(0x10));
        SetWord(Vtx(0), 0);
        SetWord(Vtx(2), 0);
        SetWord(Vtx(4), 0);
        SetWord(Vtx(8), x);
        SetWord(Vtx(0xA), y);
        SetWord(Vtx(0xC), 0);
        SetWord(Vtx(0x10), static_cast<unsigned>(Mul(MH_CALL(Math_Sin)(a), L(::bof3::Img(kScratch.v))) >> 12));
        const int c = MH_CALL(Math_Cos)(a);
        unsigned char* const mode = Gfx_PacketNext;
        SetWord(Vtx(0x12), static_cast<unsigned>(Mul(c, L(::bof3::Img(kScratch.v))) >> 12));
        SetWord(Vtx(0x14), 0);
        MH_CALL(Gpu_SetDrawMode)(mode, 0, 1, 0x35, 0);
        LinkAtTask(2, 0xC);
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        const std::int32_t grey = Sprite_Current[0xA] << 3;
        SetLong(ScrMagicS21(8), grey);
        p[4] = U8(grey);
        p[5] = ScrMagicS21(8)[0];
        p[6] = ScrMagicS21(8)[0];
        for (unsigned k : {0x14u, 0x15u, 0x16u, 0x24u, 0x25u, 0x26u}) p[k] = 1;
        long depth;
        MH_CALL(Gte_RotTransPers3)(VtxP(0), VtxP(8), VtxP(0x10), F(p, 8), F(p, 0x18), F(p, 0x28), &depth);
        MH_CALL(Gte_PrimDepths3_10B)(p);
        LinkAtTask(2, 0x34);
    }
}

// original 0x4C8970: under the actor's matrix, its phase through [0x65B64C
// + 4 * +1] - IceShard_Wait, IceShard_Run.
MS21_EXPORT void __cdecl IceShard_Task(void) {
    MH_CALL(MagicFx_PushActorMatrix)();
    CallCell(::bof3::Img(IceShard_Phases_addr), Sprite_Current[1]);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4C8990: held until the owner's phase is 3 (Iceblast_Sink); then
// +9 (its delay) down, at 0 the phase on.
MS21_EXPORT void __cdecl IceShard_Wait(void) {
    if (Pointer(::bof3::Img(BattleE2_Owner_addr))[1] != 3) return;
    BumpMagicS21(Sprite_Current[9], -1);
    unsigned char* const sc = Sprite_Current;
    if (sc[9] == 0) BumpMagicS21(sc[1]);
}

// original 0x4C89C0: the shards; +9 up, at 0x20 free.
MS21_EXPORT void __cdecl IceShard_Run(void) {
    MH_CALL(IceShard_Draw)();
    BumpMagicS21(Sprite_Current[9]);
    if (Sprite_Current[9] == 0x20) MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4C89F0: 32 flat triangles (POLY_F3, 0x5A7570) on a ring of row
// h = +0xB >> 1 of the jitter bytes at angle 0x80 k: radius 0x100 - 41 h +
// j[(k / 8) & 7] + 4 (+0xA, or +9 while +0xA is 0), height -72 h - j (0x24
// higher for odd +0xB), each point moved by the jitter byte j[h + k]; the
// triangle's three corners a small circle (radius 0x21 - +9) round that
// point at (+9 + 0x10, +9 - 0x18, +9 - 0xF) & 0x3F << 6, their height
// + +9 * 4 - clamped to 0 when above, which also latches +0xA = +9 once.
// Colour (0x40, 0x40, 0x40 + Rand & 0x7F); linked at the point.
MS21_EXPORT void __cdecl IceShard_Draw(void) {
    const unsigned char* sc = Sprite_Current;
    const int h = sc[0xB] >> 1;
    const std::int32_t base = 0x100 - 41 * h;
    const std::int32_t lift = -72 * h;
    int k = 1;
    for (int a = 0x80; a < 0x1080; a += 0x80, ++k) {
        sc = Sprite_Current;
        SetLong(ScrMagicS21(4), base);
        const int col = (k / 8) & 7;
        const unsigned char* const jit = Mem(::bof3::Img(Iceblast_Jitter_addr) + static_cast<std::uint32_t>(col + h * 8));
        std::int32_t radius = static_cast<signed char>(jit[0]) + base;
        SetLong(ScrMagicS21(0), radius);
        if (sc[0xA] != 0)
            SetLong(ScrMagicS21(0), radius + sc[0xA] * 4);
        else
            SetLong(ScrMagicS21(0), radius + sc[9] * 4);
        const int s = MH_CALL(Math_Sin)(a);
        std::uint16_t px = static_cast<std::uint16_t>(Mul(s, L(::bof3::Img(kScratch.v))) >> 12);
        const int c = MH_CALL(Math_Cos)(a);
        std::uint16_t pz = static_cast<std::uint16_t>(Mul(c, L(::bof3::Img(kScratch.v))) >> 12);
        std::uint16_t py = static_cast<std::uint16_t>(lift - static_cast<signed char>(jit[0]));
        const unsigned char* const s2 = Sprite_Current;
        const records::BattleTask* const ts2 = rv::AsConst<records::BattleTask>(s2);
        if (s2[0xB] & 1) py = static_cast<std::uint16_t>(py - 0x24);
        const std::uint16_t j = static_cast<std::uint16_t>(static_cast<signed char>(Mem(::bof3::Img(Iceblast_Jitter_addr) + static_cast<std::uint32_t>(h + k))[0]));
        py = static_cast<std::uint16_t>(py + j);
        px = static_cast<std::uint16_t>(px + j);
        pz = static_cast<std::uint16_t>(pz + j);
        const unsigned long x = static_cast<unsigned long>((static_cast<std::uint32_t>(static_cast<short>(px)) << 9) +
                                                           static_cast<std::uint32_t>(ts2->x));
        const unsigned long z = static_cast<unsigned long>((static_cast<std::uint32_t>(static_cast<short>(pz)) << 9) +
                                                           static_cast<std::uint32_t>(ts2->z));
        MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x35, 0);
        MH_CALL(MapView_LinkPrimAt)(x, z, 0, 0xC);
        unsigned char* const p = Gfx_PacketNext;
        MH_AT(PrimFn, kSetPolyF3.v)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        SetLong(ScrMagicS21(0), 0x21 - Sprite_Current[9]);
        int v = MH_CALL(Math_Sin)(((Sprite_Current[9] + 0x10) & 0x3F) << 6);
        SetWord(Vtx(0), static_cast<unsigned>((Mul(v, L(::bof3::Img(kScratch.v))) >> 12) + px));
        v = MH_CALL(Math_Cos)(((Sprite_Current[9] + 0x10) & 0x3F) << 6);
        SetWord(Vtx(2), static_cast<unsigned>((Mul(v, L(::bof3::Img(kScratch.v))) >> 12) + pz));
        v = MH_CALL(Math_Sin)(((Sprite_Current[9] - 0x18) & 0x3F) << 6);
        SetWord(Vtx(8), static_cast<unsigned>((Mul(v, L(::bof3::Img(kScratch.v))) >> 12) + px));
        v = MH_CALL(Math_Cos)(((Sprite_Current[9] - 0x18) & 0x3F) << 6);
        SetWord(Vtx(0xA), static_cast<unsigned>((Mul(v, L(::bof3::Img(kScratch.v))) >> 12) + pz));
        v = MH_CALL(Math_Sin)(((Sprite_Current[9] - 0xF) & 0x3F) << 6);
        SetWord(Vtx(0x10), static_cast<unsigned>((Mul(v, L(::bof3::Img(kScratch.v))) >> 12) + px));
        v = MH_CALL(Math_Cos)(((Sprite_Current[9] - 0xF) & 0x3F) << 6);
        unsigned char* const s3 = Sprite_Current;
        SetWord(Vtx(0x12), static_cast<unsigned>((Mul(v, L(::bof3::Img(kScratch.v))) >> 12) + pz));
        const std::uint16_t y = static_cast<std::uint16_t>(py + s3[9] * 4u);
        SetWord(Vtx(0x14), y);
        SetWord(Vtx(0xC), y);
        SetWord(Vtx(4), y);
        if (static_cast<short>(y) > 0) {
            SetWord(Vtx(0x14), 0);
            SetWord(Vtx(0xC), 0);
            SetWord(Vtx(4), 0);
            if (s3[0xA] == 0) s3[0xA] = s3[9];
        }
        p[4] = 0x40;
        p[5] = 0x40;
        p[6] = U8((MH_CALL(Rand)() & 0x7F) + 0x40);
        long depth;
        MH_CALL(Gte_RotTransPers3)(VtxP(0), VtxP(8), VtxP(0x10), F(p, 8), F(p, 0x14), F(p, 0x20), &depth);
        MH_CALL(Gte_PrimDepths3_0C)(p);
        MH_CALL(MapView_LinkPrimAt)(x, z, 0, 0x2C);
    }
}

void Iceblast_Inject() {
    BOF3_INJECT(Iceblast_Task);
    BOF3_INJECT(Iceblast_Start);
    BOF3_INJECT(Iceblast_Chill);
    BOF3_INJECT(Iceblast_Rise);
    BOF3_INJECT(Iceblast_Sink);
    BOF3_INJECT(Iceblast_End);
    BOF3_INJECT(Iceblast_DrawSpires);
    BOF3_INJECT(Iceblast_DrawSpiresInner);
    BOF3_INJECT(Iceblast_DrawDisc);
    BOF3_INJECT(IceShard_Task);
    BOF3_INJECT(IceShard_Wait);
    BOF3_INJECT(IceShard_Run);
    BOF3_INJECT(IceShard_Draw);
}

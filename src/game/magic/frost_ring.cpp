// Moved here by tools/move_funcs.py from magic_s21.cpp (docs/source-layout-round.md).
#include "game/magic/frost_ring.h"

#include <cstdint>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
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

// imul r32, r/m32: the low 32 bits, then an arithmetic shift.
std::int32_t Mul(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b));
}

void BumpMagicS21(unsigned char& b, int d = 1) { b = static_cast<unsigned char>(b + d); }

unsigned char U8(int v) { return static_cast<unsigned char>(v); }

float* F(unsigned char* prim, unsigned off) { return reinterpret_cast<float*>(prim + off); }

void PutF(unsigned char* prim, unsigned off, std::int32_t v) { *F(prim, off) = static_cast<float>(v); }

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

// original 0x4C7430 (Magic_Rows row 20): the kind-2 task, its phase +1
// through a two-entry stack table - Frost_Start, Frost_Wait. Ours aborts
// past 1.
MS21_EXPORT void __cdecl Frost_Task(void) {
    static constexpr std::uint32_t kPhases[2] = {bof3::addr::Frost_Start, bof3::addr::Frost_Wait};
    const unsigned phase = Sprite_Current[1];
    if (phase >= 2) bof3::Fatal("Frost_Task: phase %u, past the two-entry table", phase);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x4C7460: the source sprite's (0x904B4C, read once) position and
// screen point (y 16 up) to the task; a ring child (kind 1, 0x15) owned by
// it with the source's actor byte and position; +0xB 0; the phase on; sound
// 0x100.
MS21_EXPORT void __cdecl Frost_Start(void) {
    unsigned char* const src = Pointer(::bof3::Img(BattleOdds_ActorObject_addr));
    rv::Store(rv::Current<records::BattleTask>()->x, Long(src + 0x34));
    rv::Store(rv::Current<records::BattleTask>()->z, Long(src + 0x38));
    rv::Store(rv::Current<records::BattleTask>()->height, Long(src + 0x3C));
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->screen_x, static_cast<std::uint16_t>(rv::As<records::SpriteHead>(src)->screen_x));
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->screen_y, Word(src + 0x30) - 0x10u);
    const unsigned k = MH_CALL(BattleTask_Create)(1, 0x15);
    unsigned char* const child = Mem(::bof3::Img(BattleTask_Slots_addr) + k * at::kTaskStride);
    unsigned char* const sc = Sprite_Current;
    SetLong(child + 0x80, static_cast<std::int32_t>(Addr(sc)));
    child[8] = src[8];
    SetLong(child + 0x34, rv::As<records::SpriteHead>(src)->x);
    SetLong(child + 0x38, rv::As<records::SpriteHead>(src)->z);
    rv::Store(rv::As<records::BattleTask>(child)->height, rv::As<records::SpriteHead>(src)->height);
    sc[0xB] = 0;
    BumpMagicS21(Sprite_Current[1]);
    MH_CALL(Sound_PlayById)(0x100);
}

// original 0x4C7510: the screen point from the source again; once the ring
// has set +0xB to 0xFF, the target's tint released, it flashed, its flag
// 0x40, the done flag, free.
MS21_EXPORT void __cdecl Frost_Wait(void) {
    const unsigned char* const src = Pointer(::bof3::Img(BattleOdds_ActorObject_addr));
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->screen_x, static_cast<std::uint16_t>(rv::As<records::SpriteHead>(src)->screen_x));
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->screen_y, Word(src + 0x30) - 0x10u);
    if (Sprite_Current[0xB] != 0xFF) return;
    MH_CALL(Sprite_ReleaseTint)(TargetRecord());
    MH_CALL(BattleActor_Flash)(Mem(::bof3::Img(at::kTarget))[0]);
    MH_CALL(Battle_SetTargetFlag40)(Mem(::bof3::Img(at::kTarget))[0]);
    Mem(::bof3::Img(at::kFlags))[0] = static_cast<unsigned char>(Mem(::bof3::Img(at::kFlags))[0] | 4);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4C75B0: its phase through [0x65B63C + 4 * +1]; unless the
// owner's +0xB is 0xFF, the shards and the disc under its own matrix; then
// always a draw mode (0x15) committed to slot 3.
MS21_EXPORT void __cdecl FrostRing_Task(void) {
    CallCell(::bof3::Img(FrostRing_Phases_addr), Sprite_Current[1]);
    if (Pointer(::bof3::Img(BattleE2_Owner_addr))[0xB] != 0xFF) {
        MH_CALL(FrostRing_PushMatrix)();
        MH_CALL(FrostRing_DrawShards)();
        MH_CALL(FrostRing_DrawDisc)();
        MH_CALL(Gte_PopMatrix)();
    }
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x15, 0);
    MH_CALL(Gfx_CommitPrim)(3, 0xC);
}

// original 0x4C7610: the target's flags 0x20; +9, +0xA 0; the phase on.
MS21_EXPORT void __cdecl FrostRing_Mark(void) {
    MH_CALL(Battle_SetTargetFlags)(Mem(::bof3::Img(at::kTarget))[0], 0x20);
    Sprite_Current[9] = 0;
    Sprite_Current[0xA] = 0;
    BumpMagicS21(Sprite_Current[1]);
}

// original 0x4C7640: +0xA and +9 up; at +9 0x10 the target's tint released
// and set to (6, 4, 8, 0), +9 0, the phase on.
MS21_EXPORT void __cdecl FrostRing_Grow(void) {
    BumpMagicS21(Sprite_Current[0xA]);
    BumpMagicS21(Sprite_Current[9]);
    if (Sprite_Current[9] != 0x10) return;
    unsigned char* const r = TargetRecord();
    MH_CALL(Sprite_ReleaseTint)(r);
    MH_CALL(Sprite_SetTint)(r, 6, 4, 8, 0);
    Sprite_Current[9] = 0;
    BumpMagicS21(Sprite_Current[1]);
}

// original 0x4C76D0: +0xA up to 0x10; +9 up, at 0x10 back to 0 and the phase
// on. The task is read again only after +0xA moves.
MS21_EXPORT void __cdecl FrostRing_Spin(void) {
    unsigned char* sc = Sprite_Current;
    if (sc[0xA] < 0x10) {
        BumpMagicS21(sc[0xA]);
        sc = Sprite_Current;
    }
    BumpMagicS21(sc[9]);
    unsigned char* const s2 = Sprite_Current;
    if (s2[9] != 0x10) return;
    s2[9] = 0;
    BumpMagicS21(Sprite_Current[1]);
}

// original 0x4C7710: +9 up; at 0x20 the owner's +0xB 0xFF (Frost_Wait's
// signal) and free.
MS21_EXPORT void __cdecl FrostRing_Fade(void) {
    BumpMagicS21(Sprite_Current[9]);
    if (Sprite_Current[9] != 0x20) return;
    Pointer(::bof3::Img(BattleE2_Owner_addr))[0xB] = 0xFF;
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4C7740: six rings (i 0..5) of eight gouraud triangles each, one
// point on the axis (radius (i + 1) << 4) and two on a ring round it; the
// ring's radius and height by the phase +1 and +9:
//   phase 2: rim (i + 1) << 6, height -80 * (8 - (i ^ 7));
//   phase 3: rim ((i + 1) << 6) - +9, height 2 * (+9 - 40 * ((i ^ 7) + 1));
//   else:    rim 4 * +9 * (i + 1), height -5 * +9 * ((i ^ 7) + 1).
// The rim shade (r, r, 0xC0) with r = (i + 1) << 4 - in phase 3 less (+9 >> 1)
// * (i + 1), at least 1 when that goes negative, and blue 0xC1 - +9 * 6.
// Scratch dwords 0x903850 / 0x903854 / 0x903858 / 0x90385C, each read again.
MS21_EXPORT void __cdecl FrostRing_DrawShards(void) {
    int i = 0;
    do {
        const unsigned char* const sc = Sprite_Current;
        const int n = i + 1;
        const int r16 = n << 4;
        const int j = i ^ 7;
        if (sc[1] == 2) {
            SetLong(ScrMagicS21(0), r16);
            SetLong(ScrMagicS21(4), n << 6);
            SetLong(ScrMagicS21(8), (-1 - j) * 80);
        } else if (sc[1] == 3) {
            SetLong(ScrMagicS21(0), r16);
            SetLong(ScrMagicS21(4), (n << 6) - sc[9]);
            SetLong(ScrMagicS21(8), (sc[9] - (j + 1) * 40) * 2);
        } else {
            SetLong(ScrMagicS21(0), r16);
            SetLong(ScrMagicS21(4), Mul(sc[9], n) << 2);
            SetLong(ScrMagicS21(8), -5 * Mul(j + 1, sc[9]));
        }
        SetWord(Vtx(0x10), static_cast<unsigned>(Mul(MH_CALL(Math_Cos)(0), L(::bof3::Img(kScratch.v))) >> 12));
        SetWord(Vtx(0x12), static_cast<unsigned>(Mul(MH_CALL(Math_Sin)(0), L(::bof3::Img(kScratch.v))) >> 12));
        SetWord(Vtx(0x14), 0);
        int a1 = 0x100;
        for (int a2 = 0x200; a2 < 0x1200; a2 += 0x200, a1 += 0x200) {
            SetWord(Vtx(0), static_cast<unsigned>(Mul(MH_CALL(Math_Cos)(a1), L(::bof3::Img(kScratch.v) + 4)) >> 12));
            const int s1 = MH_CALL(Math_Sin)(a1);
            const std::uint16_t h = Word(ScrMagicS21(8)), px = Word(Vtx(0x10));
            SetWord(Vtx(2), static_cast<unsigned>(Mul(s1, L(::bof3::Img(kScratch.v) + 4)) >> 12));
            const std::uint16_t pz = Word(Vtx(0x12));
            SetWord(Vtx(4), h);
            SetWord(Vtx(8), px);
            SetWord(Vtx(0xA), pz);
            SetWord(Vtx(0xC), 0);
            SetWord(Vtx(0x10), static_cast<unsigned>(Mul(MH_CALL(Math_Cos)(a2), L(::bof3::Img(kScratch.v))) >> 12));
            const int s2 = MH_CALL(Math_Sin)(a2);
            const int ox = S16(Vtx(0x10));
            const unsigned char* const s = Sprite_Current;
            const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
            SetWord(Vtx(0x14), 0);
            const std::int32_t oz = Mul(s2, L(::bof3::Img(kScratch.v))) >> 12;
            SetWord(Vtx(0x12), static_cast<unsigned>(oz));
            const unsigned long x = static_cast<unsigned long>((static_cast<std::uint32_t>(ox) << 9) +
                                                               static_cast<std::uint32_t>(ts->x));
            const unsigned long z = static_cast<unsigned long>((static_cast<std::uint32_t>(static_cast<short>(oz)) << 9) +
                                                               static_cast<std::uint32_t>(ts->z));
            MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x35, 0);
            MH_CALL(MapView_LinkPrimAt)(x, z, 2, 0xC);
            unsigned char* const p = Gfx_PacketNext;
            MH_CALL(Gpu_SetPolyG3)(p);
            MH_CALL(Gpu_SetSemiTrans)(p, 1);
            const unsigned char* const s3 = Sprite_Current;
            if (s3[1] == 3) {
                SetLong(ScrMagicS21(0xC), r16);
                const int less = -(r16 / 16) * (s3[9] >> 1);
                const std::int32_t v = L(::bof3::Img(kScratch.v) + 0xC) + less;
                SetLong(ScrMagicS21(0xC), v);
                if (v < 0) SetLong(ScrMagicS21(0xC), 1);
                p[4] = 1;
                p[5] = 1;
                p[6] = 1;
                p[0x14] = ScrMagicS21(0xC)[0];
                p[0x15] = ScrMagicS21(0xC)[0];
                const unsigned char blue = U8(0xC1 - U8(Sprite_Current[9] * 6));
                p[0x16] = blue;
                p[0x24] = p[0x14];
                p[0x25] = p[0x15];
                p[0x26] = blue;
            } else {
                SetLong(ScrMagicS21(0xC), r16);
                p[4] = 1;
                p[5] = 1;
                p[6] = 1;
                p[0x14] = ScrMagicS21(0xC)[0];
                p[0x15] = ScrMagicS21(0xC)[0];
                p[0x16] = 0xC0;
                p[0x24] = ScrMagicS21(0xC)[0];
                p[0x25] = ScrMagicS21(0xC)[0];
                p[0x26] = 0xC0;
            }
            long depth;
            MH_CALL(Gte_RotTransPers3)(VtxP(0), VtxP(8), VtxP(0x10), F(p, 8), F(p, 0x18), F(p, 0x28), &depth);
            MH_CALL(Gte_PrimDepths3_10B)(p);
            MH_CALL(MapView_LinkPrimAt)(x, z, 2, 0x34);
        }
        i = n;
    } while (i < 6);
}

// original 0x4C7A80: pushes the matrix and loads Camera_Matrix x the ring's:
// translation RotTrans of (+0x34 >> 9 - 0x4000, +0x38 >> 9 - 0x4000,
// -(+0x3E / 2)), rotation (0, 0, +0xA << 4). Pops nothing - its caller does.
// MagicFx_PushActorMatrix's shape with a turn; the MATRIX one block with the
// translation at +0x14, as the original's stack lays it out.
MS21_EXPORT void __cdecl FrostRing_PushMatrix(void) {
    MH_CALL(Gte_PushMatrix)();
    const unsigned char* const sc = Sprite_Current;
    const records::BattleTask* const tsc = rv::AsConst<records::BattleTask>(sc);
    short rot[4];
    rot[0] = 0;
    rot[1] = 0;
    rot[2] = static_cast<short>(sc[0xA] << 4);
    rot[3] = 0;
    short v[4];
    v[0] = static_cast<short>((tsc->x >> 9) - 0x4000);
    v[1] = static_cast<short>((tsc->z >> 9) - 0x4000);
    v[2] = static_cast<short>(-(S16(sc + 0x3E) / 2));
    v[3] = 0;
    struct Matrix {
        short m[10];
        long t[3];
    } m;
    static_assert(sizeof(Matrix) == 0x20, "MATRIX layout");
    MH_CALL(Gte_RotTrans)(v, m.t);
    MH_CALL(Gte_RotMatrix)(rot, m.m);
    MH_CALL(Gte_MulMatrix0)(Camera_Matrix, m.m, m.m);
    MH_CALL(Gte_SetRotMatrix)(reinterpret_cast<const unsigned long*>(&m));
    MH_CALL(Gte_SetTransMatrix)(reinterpret_cast<const unsigned long*>(&m));
}

// original 0x4C7B30: 32 gouraud triangles in a disc at the owner's screen
// point (+0x2E, +0x30), radius by the phase: 0x40 in 2, (0x20 - +9) * 2 in
// 3, else +9 * 3 + 0x10 (dword 0x903850); the centre (0x20, 0x20, 0xC0),
// the rim (1, 1, 1); each linked at the ring's position.
MS21_EXPORT void __cdecl FrostRing_DrawDisc(void) {
    const unsigned char* const sc = Sprite_Current;
    if (sc[1] == 2) {
        SetLong(ScrMagicS21(0), 0x40);
    } else if (sc[1] == 3) {
        SetLong(ScrMagicS21(0), (0x20 - sc[9]) * 2);
    } else {
        SetLong(ScrMagicS21(0), sc[9] * 3 + 0x10);
    }
    int a = 0;
    do {
        MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x35, 0);
        LinkAtTask(2, 0xC);
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        PutF(p, 8, S16(Pointer(::bof3::Img(BattleE2_Owner_addr)) + 0x2E));
        PutF(p, 0xC, S16(Pointer(::bof3::Img(BattleE2_Owner_addr)) + 0x30));
        int v = MH_CALL(Math_Sin)(a);
        PutF(p, 0x18, (Mul(v, L(::bof3::Img(kScratch.v))) >> 12) + S16(Pointer(::bof3::Img(BattleE2_Owner_addr)) + 0x2E));
        v = MH_CALL(Math_Cos)(a);
        a += 0x80;
        PutF(p, 0x1C, (Mul(v, L(::bof3::Img(kScratch.v))) >> 12) + S16(Pointer(::bof3::Img(BattleE2_Owner_addr)) + 0x30));
        v = MH_CALL(Math_Sin)(a);
        PutF(p, 0x28, (Mul(v, L(::bof3::Img(kScratch.v))) >> 12) + S16(Pointer(::bof3::Img(BattleE2_Owner_addr)) + 0x2E));
        v = MH_CALL(Math_Cos)(a);
        const std::int32_t y2 = (Mul(v, L(::bof3::Img(kScratch.v))) >> 12) + S16(Pointer(::bof3::Img(BattleE2_Owner_addr)) + 0x30);
        p[4] = 0x20;
        p[5] = 0x20;
        p[6] = 0xC0;
        for (unsigned k : {0x14u, 0x15u, 0x16u}) p[k] = 1;
        PutF(p, 0x2C, y2);
        for (unsigned k : {0x24u, 0x25u, 0x26u}) p[k] = 1;
        LinkAtTask(2, 0x34);
    } while (a < 0x1000);
}

void FrostRing_Inject() {
    BOF3_INJECT(Frost_Task);
    BOF3_INJECT(Frost_Start);
    BOF3_INJECT(Frost_Wait);
    BOF3_INJECT(FrostRing_Task);
    BOF3_INJECT(FrostRing_Mark);
    BOF3_INJECT(FrostRing_Grow);
    BOF3_INJECT(FrostRing_Spin);
    BOF3_INJECT(FrostRing_Fade);
    BOF3_INJECT(FrostRing_DrawShards);
    BOF3_INJECT(FrostRing_PushMatrix);
    BOF3_INJECT(FrostRing_DrawDisc);
}

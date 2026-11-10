// Moved here by tools/move_funcs.py from magic_s19.cpp (docs/source-layout-round.md).
#include "game/magic/barrier.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = magic_harness::at;

}  // namespace

#define MS19_EXPORT extern "C" __attribute__((disable_tail_calls))

namespace {

using magic_harness::Mem;

using magic_harness::Pointer;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

// The GTE's input vectors (four SVECTORs) and the effects' scratch words.
// kV0: now Prim_VertexScratch (symbols.toml).
// kV1: now Prim_VertexScratch[1] (symbols.toml).
// kV2: now Prim_VertexScratch[2] (symbols.toml).
// kV3: now Prim_VertexScratch[3] (symbols.toml).
constexpr ::bof3::image::Va kRadius{SharedScratch_addr}; // s16 (dword for MAGIC086's draws)

using Handler = void (__cdecl*)();

void Bump(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Drop(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

unsigned char* Task(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + (slot & 0xFF) * at::kTaskStride); }

unsigned char* Tint(unsigned i) { return Mem(::bof3::Img(MoveScript_TintRecords_addr) + 12 * i); }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

// A .data handler table's cell, read in place.
Handler Cell(std::uint32_t table, unsigned index) {
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(table + 4 * index)))));
}

void Put16(std::uint32_t a, std::uint32_t v) { SetWord(Mem(a), v & 0xFFFF); }

// imul then sar, as the original's 32-bit registers wrap.
std::int32_t MulSar(std::int32_t a, std::int32_t b, int n) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> n;
}

const short* Vec(std::uint32_t a) { return reinterpret_cast<const short*>(Mem(a)); }

float* At(unsigned char* prim, unsigned off) { return reinterpret_cast<float*>(prim + off); }

unsigned char* Prim() { return Gfx_PacketNext; }

void DrawModeAndCommit(unsigned tpage) {
    MH_CALL(Gpu_SetDrawMode)(Prim(), 0, 1, tpage, 0);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
}

// The sin / cos helpers at radius r: (f(angle) * r) >> 12, as a word.
std::int32_t Sin(std::int32_t angle) { return MH_CALL(Math_Sin)(angle); }

std::int32_t Cos(std::int32_t angle) { return MH_CALL(Math_Cos)(angle); }

// A part (kind 1 parameter 0x1D) of type `type` at the effect's point, owned by
// it, +9 and +0xA 0 (the ring's +0xB 0x30 first). The writes in the
// original's order: the slot may be the effect's own.
void SpawnPart(unsigned char type) {
    const unsigned char slot = MH_CALL(BattleTask_Create)(1, 0x1D);
    unsigned char* const t = Task(slot);
    records::BattleTask* const tt = rv::As<records::BattleTask>(t);
    const unsigned char* const cur = Sprite_Current;
    const records::BattleTask* const tcur = rv::AsConst<records::BattleTask>(cur);
    tt->owner = static_cast<std::int32_t>(Low32(cur));
    t[1] = type;
    if (type == 1) t[0xB] = 0x30;
    t[9] = 0;
    t[0xA] = 0;
    tt->x = tcur->x;
    tt->z = tcur->z;
    rv::Store<unsigned>(tt->height_whole, tcur->height_whole);
}

}  // namespace

// original 0x4C27F0: the kind-2 task (Magic_Rows row 28). Its phase +1 through
// a six-entry table the original builds on its stack - Barrier_Start,
// Barrier_Tint, BattleFx_Brighten, Barrier_WaitRings, Barrier_Fade and
// 0x4BDC10 (MAGIC078's: once +0xB is 0xFF, Battle_SetTargetFlag40 on the
// target, the done flag, free). Unchecked by the original; ours aborts.
MS19_EXPORT void __cdecl Barrier_Task(void) {
    static constexpr std::uint32_t kPhases[6] = {bof3::addr::Barrier_Start,     bof3::addr::Barrier_Tint,
                                                 bof3::addr::BattleFx_Brighten, bof3::addr::Barrier_WaitRings,
                                                 bof3::addr::Barrier_Fade,      bof3::addr::Leech_WaitOrbs};
    const unsigned phase = Sprite_Current[1];
    if (phase >= 6) bof3::Fatal("Barrier_Task: phase %u, past the six-entry table", phase);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x4C2840: Barrier_Task's entry 0. The source sprite's (0x904B4C,
// read once) position to the task; two parts at it - the disc (+1 0) and the
// ring (+1 1, +0xB 0x30) - with +9 and +0xA 0; sound 0x100; +0xB and +9 0;
// the phase on.
MS19_EXPORT void __cdecl Barrier_Start(void) {
    const unsigned char* const source = Pointer(::bof3::Img(BattleOdds_ActorObject_addr));
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(source)->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(source)->z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(source)->height);
    SpawnPart(0);
    SpawnPart(1);
    MH_CALL(Sound_PlayById)(0x100);
    Sprite_Current[0xB] = 0;
    Sprite_Current[9] = 0;
    Bump(Sprite_Current[1]);
}

// original 0x4C2960: entry 1. Once the ring has set +0xB to 1, the source
// sprite's tint released and a new one set (1, 1, 1, 0), its record in +0xA;
// +9 0x10; the phase on (to BattleFx_Brighten, which raises it).
MS19_EXPORT void __cdecl Barrier_Tint(void) {
    const unsigned char* const cur = Sprite_Current;
    unsigned char* const source = Pointer(::bof3::Img(BattleOdds_ActorObject_addr));
    if (cur[0xB] != 1) return;
    MH_CALL(Sprite_ReleaseTint)(source);
    const unsigned char tint = MH_CALL(Sprite_SetTint)(source, 1, 1, 1, 0);
    Sprite_Current[0xA] = tint;
    Sprite_Current[9] = 0x10;
    Bump(Sprite_Current[1]);
}

// original 0x4C29B0: entry 3. The phase on once the ring has set +0xB to 2.
MS19_EXPORT void __cdecl Barrier_WaitRings(void) {
    unsigned char* const cur = Sprite_Current;
    if (cur[0xB] == 2) Bump(cur[1]);
}

// original 0x4C29C0: entry 4. The tint record's three levels down by one
// (unguarded); when the first reaches 0, the source's tint released, the
// target (0x904B44) flashed, the phase on.
MS19_EXPORT void __cdecl Barrier_Fade(void) {
    const unsigned char* const cur = Sprite_Current;
    Drop(Tint(cur[0xA])[2]);
    Drop(Tint(cur[0xA])[3]);
    Drop(Tint(cur[0xA])[4]);
    if (Tint(cur[0xA])[2] != 0) return;
    MH_CALL(Sprite_ReleaseTint)(Pointer(::bof3::Img(BattleOdds_ActorObject_addr)));
    MH_CALL(BattleActor_Flash)(Mem(::bof3::Img(at::kTarget))[0]);
    Bump(Sprite_Current[1]);
}

// original 0x4C2A50: kind 1 parameter 0x1D - jmp [BarrierPart_Types + 4 * +1].
MS19_EXPORT void __cdecl BarrierPart_Dispatch(void) { Cell(::bof3::Img(BarrierPart_Types_addr), Sprite_Current[1])(); }

// original 0x4C2A70: BarrierPart_Types[0], the disc. Its phase +2 through
// BarrierDisc_Phases; while live, the disc under the actor's matrix.
MS19_EXPORT void __cdecl BarrierDisc_Task(void) {
    Cell(::bof3::Img(BarrierDisc_Phases_addr), Sprite_Current[2])();
    if (Sprite_Current[0] == 0) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    MH_CALL(BarrierDisc_Draw)();
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4C2AA0: BarrierDisc_Phases[0]. +9 (its brightness) and +0xA (its
// radius) up; the phase on at +9 0x10.
MS19_EXPORT void __cdecl BarrierDisc_Grow(void) {
    Bump(Sprite_Current[9]);
    Bump(Sprite_Current[0xA]);
    unsigned char* const cur = Sprite_Current;
    if (cur[9] == 0x10) Bump(cur[2]);
}

// original 0x4C2AD0: BarrierDisc_Phases[1]. The phase on once the owner's +0xB
// is 2 (the ring has settled).
MS19_EXPORT void __cdecl BarrierDisc_Wait(void) {
    if (Owner()[0xB] == 2) Bump(Sprite_Current[2]);
}

// original 0x4C2AF0: BarrierDisc_Phases[2]. +9 down; at 0 freed (a tail jump).
MS19_EXPORT void __cdecl BarrierDisc_Shrink(void) {
    Drop(Sprite_Current[9]);
    if (Sprite_Current[9] != 0) return;
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4C2B10: the disc - 32 POLY_G3s about the origin, radius +0xA * 12
// (the dword 0x903850), semi-transparent; the centre (+9 * 6, +9 * 6, +9 *
// 4) and the rim (+9 * 4, +9 * 4, +9 * 2), bytes; between draw modes 0x35 and
// 0x15. The rim's first two bytes are read back from the primitive.
MS19_EXPORT void __cdecl BarrierDisc_Draw(void) {
    DrawModeAndCommit(0x35);
    SetLong(Mem(::bof3::Img(kRadius.v)), static_cast<std::int32_t>(Sprite_Current[0xA] * 12u));
    Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x10, static_cast<std::uint32_t>(MulSar(Sin(0), Long(Mem(::bof3::Img(kRadius.v))), 12)));
    Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x10 + 2, static_cast<std::uint32_t>(MulSar(Cos(0), Long(Mem(::bof3::Img(kRadius.v))), 12)));
    for (std::int32_t a = 0x80; a < 0x1080; a += 0x80) {
        Put16(::bof3::Img(Prim_VertexScratch_addr), 0);
        Put16(::bof3::Img(Prim_VertexScratch_addr) + 2, 0);
        Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x8, Word(Mem(::bof3::Img(Prim_VertexScratch_addr) + 0x10)));
        Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 2, Word(Mem(::bof3::Img(Prim_VertexScratch_addr) + 0x10 + 2)));
        Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x10, static_cast<std::uint32_t>(MulSar(Sin(a), Long(Mem(::bof3::Img(kRadius.v))), 12)));
        Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x10 + 2, static_cast<std::uint32_t>(MulSar(Cos(a), Long(Mem(::bof3::Img(kRadius.v))), 12)));
        unsigned char* const prim = Prim();
        Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x10 + 4, 0);
        Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 4, 0);
        Put16(::bof3::Img(Prim_VertexScratch_addr) + 4, 0);
        MH_CALL(Gpu_SetPolyG3)(prim);
        MH_CALL(Gpu_SetSemiTrans)(prim, 1);
        long depth;
        MH_CALL(Gte_RotTransPers3)(Vec(::bof3::Img(Prim_VertexScratch_addr)), Vec(::bof3::Img(Prim_VertexScratch_addr) + 0x8), Vec(::bof3::Img(Prim_VertexScratch_addr) + 0x10), At(prim, 8), At(prim, 0x18), At(prim, 0x28), &depth);
        MH_CALL(Gte_PrimDepths3_10B)(prim);
        prim[4] = static_cast<unsigned char>(Sprite_Current[9] * 6);
        prim[5] = static_cast<unsigned char>(Sprite_Current[9] * 6);
        prim[6] = static_cast<unsigned char>(Sprite_Current[9] << 2);
        prim[0x14] = static_cast<unsigned char>(Sprite_Current[9] << 2);
        prim[0x15] = static_cast<unsigned char>(Sprite_Current[9] << 2);
        const unsigned char blue = static_cast<unsigned char>(Sprite_Current[9] << 1);
        const unsigned char red = prim[0x14];
        const unsigned char green = prim[0x15];
        prim[0x16] = blue;
        prim[0x24] = red;
        prim[0x25] = green;
        prim[0x26] = blue;
        MH_CALL(Gfx_CommitPrim)(5, 0x34);
    }
    DrawModeAndCommit(0x15);
}

void Barrier_Inject() {
    BOF3_INJECT(Barrier_Task);
    BOF3_INJECT(Barrier_Start);
    BOF3_INJECT(Barrier_Tint);
    BOF3_INJECT(Barrier_WaitRings);
    BOF3_INJECT(Barrier_Fade);
    BOF3_INJECT(BarrierPart_Dispatch);
    BOF3_INJECT(BarrierDisc_Task);
    BOF3_INJECT(BarrierDisc_Grow);
    BOF3_INJECT(BarrierDisc_Wait);
    BOF3_INJECT(BarrierDisc_Shrink);
    BOF3_INJECT(BarrierDisc_Draw);
}

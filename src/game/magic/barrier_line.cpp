// Moved here by tools/move_funcs.py from magic_s19.cpp (docs/source-layout-round.md).
#include "game/magic/barrier_line.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s19.h"
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

using move_script::SetWord;

using Handler = void (__cdecl*)();

void Bump(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Drop(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

// A .data handler table's cell, read in place.
Handler Cell(std::uint32_t table, unsigned index) {
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(table + 4 * index)))));
}

void Put16(std::uint32_t a, std::uint32_t v) { SetWord(Mem(a), v & 0xFFFF); }

std::int32_t ShlSar(std::int32_t a, int l, int r) { return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) << l) >> r; }

std::int32_t Add(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b));
}

const short* Vec(std::uint32_t a) { return reinterpret_cast<const short*>(Mem(a)); }

float* At(unsigned char* prim, unsigned off) { return reinterpret_cast<float*>(prim + off); }

unsigned char* Prim() { return Gfx_PacketNext; }

// The sin / cos helpers at radius r: (f(angle) * r) >> 12, as a word.
std::int32_t Sin(std::int32_t angle) { return MH_CALL(Math_Sin)(angle); }

std::int32_t Cos(std::int32_t angle) { return MH_CALL(Math_Cos)(angle); }

}  // namespace

// original 0x4C3290: BarrierPart_Types[2], a line. Its phase +2 through
// BarrierLine_Phases; while live and past its first phase, the line under the
// actor's matrix. Nothing read here creates one (a part with +1 2).
MS19_EXPORT void __cdecl BarrierLine_Task(void) {
    Cell(::bof3::Img(BarrierLine_Phases_addr), Sprite_Current[2])();
    const unsigned char* const cur = Sprite_Current;
    if (cur[0] == 0 || cur[2] == 0) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    MH_CALL(BarrierLine_Draw)();
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4C32D0: BarrierLine_Phases[0] (a body 14 files' tables share). The
// phase on once the owner's +0xB is 1.
MS19_EXPORT void __cdecl BarrierLine_Wait(void) {
    if (Owner()[0xB] == 1) Bump(Sprite_Current[2]);
}

// original 0x4C32F0: BarrierLine_Phases[1]. +9 down; at 0 +0x14 0x20 and +0x20
// -1; the phase on.
MS19_EXPORT void __cdecl BarrierLine_Delay(void) {
    Drop(Sprite_Current[9]);
    unsigned char* const cur = Sprite_Current;
    if (cur[9] != 0) return;
    rv::Store(rv::As<records::BattleTask>(cur)->step_y, 0x20);
    rv::Store(rv::Current<records::BattleTask>()->work_20, -1);
    Bump(Sprite_Current[2]);
}

// original 0x4C3330: BarrierLine_Phases[2]. The lift (as the crystal's); freed
// (a tail jump) once +0x14 is below -16 (signed).
MS19_EXPORT void __cdecl BarrierLine_Fly(void) {
    unsigned char* const cur = Sprite_Current;
    rv::Store(rv::As<records::BattleTask>(cur)->step_y, Add(rv::As<records::BattleTask>(cur)->step_y, rv::As<records::BattleTask>(cur)->work_20));
    unsigned char* const now = Sprite_Current;
    rv::Store<unsigned>(rv::As<records::BattleTask>(now)->height_whole, rv::As<records::BattleTask>(now)->height_whole + static_cast<std::uint16_t>(rv::As<records::BattleTask>(now)->step_y));
    if (rv::Current<records::BattleTask>()->step_y >= -0x10) return;
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4C3360: the line - one LINE_G3 of three equal points in the plane,
// (Math_Sin, Math_Cos)(+0xB << 9) * 128 >> 12, at heights 0x20, 0x18 and 0,
// shaded (0x40, 0x40, 0x40), (0xC0, 0x80, 0x80), (1, 1, 1), after a draw mode
// 0x35; both linked at the line's map cell, one row up (dy 2).
MS19_EXPORT void __cdecl BarrierLine_Draw(void) {
    const std::int32_t s = ShlSar(Sin(static_cast<std::int32_t>(static_cast<std::uint32_t>(Sprite_Current[0xB]) << 9)), 7, 12);
    Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x10, static_cast<std::uint32_t>(s));
    Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x8, static_cast<std::uint32_t>(s));
    Put16(::bof3::Img(Prim_VertexScratch_addr), static_cast<std::uint32_t>(s));
    const std::int32_t cosine = Cos(static_cast<std::int32_t>(static_cast<std::uint32_t>(Sprite_Current[0xB]) << 9));
    unsigned char* const mode = Prim();
    const std::int32_t c = ShlSar(cosine, 7, 12);
    Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x10 + 2, static_cast<std::uint32_t>(c));
    Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 2, static_cast<std::uint32_t>(c));
    Put16(::bof3::Img(Prim_VertexScratch_addr) + 2, static_cast<std::uint32_t>(c));
    Put16(::bof3::Img(Prim_VertexScratch_addr) + 4, 0x20);
    Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 4, 0x18);
    Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x10 + 4, 0);
    MH_CALL(Gpu_SetDrawMode)(mode, 0, 1, 0x35, 0);
    {
        const unsigned char* const cur = Sprite_Current;
        const records::BattleTask* const tcur = rv::AsConst<records::BattleTask>(cur);
        MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(tcur->x),
                                    static_cast<unsigned long>(tcur->z), 2, 0xC);
    }
    unsigned char* const prim = Prim();
    MH_CALL(Gpu_SetLineG3)(prim);
    MH_CALL(Gpu_SetSemiTrans)(prim, 1);
    long depth;
    MH_CALL(Gte_RotTransPers3)(Vec(::bof3::Img(Prim_VertexScratch_addr)), Vec(::bof3::Img(Prim_VertexScratch_addr) + 0x8), Vec(::bof3::Img(Prim_VertexScratch_addr) + 0x10), At(prim, 8), At(prim, 0x18), At(prim, 0x28), &depth);
    MH_CALL(Gte_PrimDepths3_10C)(prim);
    prim[0x14] = 0xC0;
    prim[4] = 0x40;
    prim[5] = 0x40;
    prim[6] = 0x40;
    prim[0x15] = 0x80;
    prim[0x16] = 0x80;
    prim[0x24] = 1;
    prim[0x25] = 1;
    prim[0x26] = 1;
    const unsigned char* const cur = Sprite_Current;
    const records::BattleTask* const tcur = rv::AsConst<records::BattleTask>(cur);
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(tcur->x),
                                static_cast<unsigned long>(tcur->z), 2, 0x34);
}

void BarrierLine_Inject() {
    BOF3_INJECT(BarrierLine_Task);
    BOF3_INJECT(BarrierLine_Wait);
    BOF3_INJECT(BarrierLine_Delay);
    BOF3_INJECT(BarrierLine_Fly);
    BOF3_INJECT(BarrierLine_Draw);
}

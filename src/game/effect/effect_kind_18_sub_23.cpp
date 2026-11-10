// Moved here by tools/move_funcs.py from effect_5e.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_18_sub_23.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_5e_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_5e::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

using move_script::Word;

using Handler = scenario_harness::Handler;

unsigned char* S() { return Sprite_Current; }

U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }

U UL(U a) { return UL(At(a)); }

U W(const unsigned char* p) { return Word(p); }

U AddressOf(const void* p) { return Low32(p); }

unsigned char* Vtx() { return reinterpret_cast<unsigned char*>(Prim_VertexScratch); }

const short* Vector(U offset) { return reinterpret_cast<const short*>(Vtx() + offset); }

float* F(unsigned char* p) { return reinterpret_cast<float*>(p); }

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 2]; jmp [table + eax
// * 4]: the table's `entries` handlers read in place (the fuzz swaps the cells
// for recorders); a Fatal past them, where the original jumps through the dword
// after - the next sub-kind's table or data.
void Dispatch(const char* who, U table, unsigned entries) {
    const unsigned state = Sprite_Current[2];
    if (state >= entries)
        bof3::Fatal("%s: sub-state byte +2 is %u, past the %u entries of 0x%X - the original jumps through the dword "
                    "after (docs/effect_5e.md section 6)",
                    who, state, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(table + 4 * state)))();
}

// +2 up, Sprite_Current read afresh.
void Step() {
    unsigned char* const s = S();
    s[2] = static_cast<unsigned char>(s[2] + 1);
}

// Gte_RotTransPers4 of Prim_VertexScratch's four vectors (0x9037A0, A8, B0, B8)
// into the quad at p; the original's depth and flag are locals of its own.
void Project4(unsigned char* p) {
    long depth = 0;
    SH_CALL(Gte_RotTransPers4)(Vector(0), Vector(8), Vector(0x10), Vector(0x18), F(p + 8), F(p + 0x18), F(p + 0x28),
                               F(p + 0x38), &depth);
}

// A POLY_FT4 at the cursor, opaque shading (Gpu_SetShadeTex 0): answers it.
unsigned char* TexturedQuad() {
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyFT4)(p);
    SH_CALL(Gpu_SetShadeTex)(p, 0);
    return p;
}

// MapView_LinkPrimAt at the record's point (+0x34, +0x38), Sprite_Current read
// for it.
void LinkAtRecord(int dy, unsigned size) {
    unsigned char* const s = S();
    auto* const so = rv::As<records::EffectObject>(s);
    SH_CALL(MapView_LinkPrimAt)(static_cast<std::uint32_t>(so->x), static_cast<std::uint32_t>(so->z), dy, size);
}

}  // namespace

// original 0x506A10 (EffectKind18_States[35]): jmp [EffectKind18Sub23_States +
// +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub23_Run(void) {
    Dispatch("EffectKind18Sub23_Run", AddressOf(EffectKind18Sub23_States), EffectKind18Sub23_States_count);
}

// original 0x506A30 (sub-state 0): +0x30 = 0; story flag 0x28 already set: a
// tail jump to Effect_Release; else +2 up and a tail jump to the draw.
extern "C" void __cdecl EffectKind18Sub23_Start(void) {
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_30, 0);
    if (SH_CALL(Flags_Test)(At(::bof3::Img(Cond_Flags_addr) + 0xa0), at::kSub23Flag)) {
        SH_CALL(Effect_Release)();
        return;
    }
    Step();
    SH_CALL(EffectKind18Sub23_Draw)();
}

// original 0x506A60 (sub-state 1): once story flag 0x28 is set, +2 up; the draw.
extern "C" void __cdecl EffectKind18Sub23_WaitFlag(void) {
    if (SH_CALL(Flags_Test)(At(::bof3::Img(Cond_Flags_addr) + 0xa0), at::kSub23Flag)) Step();
    SH_CALL(EffectKind18Sub23_Draw)();
}

// original 0x506A80 (sub-state 2): +0x30 up 4; at 0x100 or more (signed word)
// Effect_Release - and the draw all the same.
extern "C" void __cdecl EffectKind18Sub23_Slide(void) {
    unsigned char* s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_30, static_cast<std::uint32_t>(rv::As<records::EffectWork>(s)->work_30) + 4);
    s = S();
    if (static_cast<std::int32_t>(static_cast<std::int16_t>(rv::As<records::EffectWork>(s)->work_30)) >= 0x100) SH_CALL(Effect_Release)();
    SH_CALL(EffectKind18Sub23_Draw)();
}

// original 0x506AB0 (the tail the three sub-states jump to; inside 0x506A80's
// tool extent, its own frame and ret): a draw mode (tpage 0x95, dtd 0) at the
// cursor linked at the record's point (0xC bytes); a POLY_FT4 at the cursor
// (read after the link): x +0x30 - 0x37C0 and +0x30 - 0x36C0, y 0x16C0, z 0x140
// and 0x280 in Prim_VertexScratch, Gte_RotTransPers4, Gte_PrimDepths4_10,
// texture 0x2378013D, linked at the record's point (0x48 bytes).
extern "C" void __cdecl EffectKind18Sub23_Draw(void) {
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x95, 0);
    LinkAtRecord(0, 0xC);
    unsigned char* const p = TexturedQuad();
    unsigned char* const v = Vtx();
    unsigned char* const s = S();
    const U near = W(s + 0x30) - 0x37C0u, far = static_cast<std::uint32_t>(rv::As<records::EffectWork>(s)->work_30) - 0x36C0u;
    rv::Store<std::uint32_t>(rv::As<records::SVector>(v)[2].x, near);
    rv::Store<std::uint32_t>(rv::As<records::SVector>(v)->x, near);
    rv::Store<std::uint32_t>(rv::As<records::SVector>(v)[3].x, far);
    rv::Store<std::uint32_t>(rv::As<records::SVector>(v)[1].x, far);
    rv::Store<std::uint32_t>(rv::As<records::SVector>(v)[3].y, 0x16C0);
    rv::Store<std::uint32_t>(rv::As<records::SVector>(v)[2].y, 0x16C0);
    rv::Store<std::uint32_t>(rv::As<records::SVector>(v)[1].y, 0x16C0);
    rv::Store<std::uint32_t>(rv::As<records::SVector>(v)->y, 0x16C0);
    rv::Store<std::uint32_t>(rv::As<records::SVector>(v)[1].z, 0x140);
    rv::Store<std::uint32_t>(rv::As<records::SVector>(v)->z, 0x140);
    rv::Store<std::uint32_t>(rv::As<records::SVector>(v)[3].z, 0x280);
    rv::Store<std::uint32_t>(rv::As<records::SVector>(v)[2].z, 0x280);
    Project4(p);
    SH_CALL(Gte_PrimDepths4_10)(p);
    SH_CALL(Prim_SetTexture)(0x2378013D, p, 1);
    LinkAtRecord(0, 0x48);
}

void EffectKind18Sub23_Inject() {
    BOF3_INJECT(EffectKind18Sub23_Run);
    BOF3_INJECT(EffectKind18Sub23_Start);
    BOF3_INJECT(EffectKind18Sub23_WaitFlag);
    BOF3_INJECT(EffectKind18Sub23_Slide);
    BOF3_INJECT(EffectKind18Sub23_Draw);
}

// Round thirteen group E5A (docs/effect_5a.md): 51 functions of
// analysis/round13_cut.tsv's group E5A (its other two rows, 0x4FD350 and
// 0x4FD3E0, are area_backdrop.cpp's already) and four starts the cut does not
// list - sub-kind 4's state 1 0x4FD4C0 and state 3 0x4FD630, sub-kind 0x1A's
// draw 0x4FDCC0 and sub-kind 7's top draw 0x4FE640 - each read with capstone
// to its last instruction. Effect_RunObjects (ours) makes each live record of
// Effect_Objects (20 of 0x80 bytes) Sprite_Current and calls
// Effect_KindHandlers[+5]; kind 0x18's EffectKind18_Run jumps through
// EffectKind18_States by +1 (the sub-kind EffectKind18_Start copied from +0xB),
// and each sub-kind here is a dispatcher by +2 through its own table (none
// bounded by a compare) or a single state. What each is, as far as the code
// says:
//
//   sub-kind 1     the sunset sky by Cond_ByteFE: the gradient climbing a step
//                  word +0x2E to 0xBF, the glow over it (area 23's cutscene)
//   sub-kind 4     two textured panels at fixed map cells, shut until flag 0x17
//                  of Cond_Flags row 2, then turned apart by +0xC to 0x400
//   sub-kind 5     CLUT slots of two strip rows cycled by a nibble test of the
//                  word +0x3A against the story-flag dword: fade in, pulse, out
//   sub-kind 6     a part set (+0x36) whose height +0x3E steps through four
//                  levels each time its story flag is set, the map's bytes and
//                  heights under it rewritten as it moves
//   sub-kind 7     a two-panel door (+0x36 picks one of three) slid open by
//                  its flag, the map's row 0x29 cells blocked and freed
//   sub-kind 8     the same panels shaken, pushed, toppled (a turning draw)
//                  and crashed into six floor tiles under puffs
//   sub-kind 9     three story flags as a pattern 1..8: a height, a 3 x 4 block
//                  of map bytes and Cond_ByteFE by it
//   sub-kind 0xA   a pair of doors on Cond_ByteFE: swung, held, slid apart
//   sub-kind 0x1A  a block slid one cell out and back on Cond_ByteFE
//   sub-kind 0x1F  CLUT slots of rows 3 and 6 brightened, then cycled, on
//                  story flag 7
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies. No divergence:
// each is a faithful replacement. Where the original jumps through a state
// table past its end or indexes one of the image's tables past its end by a
// byte of the record, ours aborts with a message (docs/effect_5a.md section 7).
#include "game/effect_5a.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/effect_5a_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_5a::at;
using U = std::uint32_t;
using move_script::At;
using move_script::Long;
using move_script::SetWord;
using move_script::Word;

unsigned char T(U a) { return *At(a); }
std::int32_t TS(U a) { return static_cast<signed char>(*At(a)); }
U TL(U a) { return static_cast<U>(Long(At(a))); }
std::int32_t Sar(U v, unsigned n) { return static_cast<std::int32_t>(v) >> n; }
U AddressOf(const void* p) { return Low32(p); }

using Handler = scenario_harness::Handler;

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 2]; jmp [table + eax * 4]:
// the table's `entries` handlers read in place (the fuzz swaps the cells for
// recorders); a Fatal past them, where the original jumps through the dword
// after - the next table, bytes or a null.
void Dispatch(const char* who, U table, unsigned entries) {
    const unsigned sub = Sprite_Current[2];
    if (sub >= entries)
        bof3::Fatal("%s: sub-state byte +2 is %u, past the %u entries of 0x%X - the original jumps through the dword "
                    "after (docs/effect_5a.md section 7)",
                    who, sub, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(TL(table + 4 * sub)))();
}

// An index into one of the image's tables by a byte or word of the record,
// unchecked by the original (it reads whatever follows).
std::int32_t Index(const char* who, std::int32_t i, unsigned n, U table) {
    if (i < 0 || static_cast<unsigned>(i) >= n)
        bof3::Fatal("%s: index %d into 0x%X, past its %u entries - the original reads what follows "
                    "(docs/effect_5a.md section 7)",
                    who, (int)i, (unsigned)table, n);
    return i;
}

// Prim_VertexScratch's four SVECTORs (x, y, z words at +0, +2, +4; the pads
// +6 left as they are).
// kV: now Prim_VertexScratch (symbols.toml).
void Vertex(unsigned k, U x, U y, U z) {
    SetWord(At(::bof3::Img(Prim_VertexScratch_addr) + 8 * k), x & 0xFFFF);
    SetWord(At(::bof3::Img(Prim_VertexScratch_addr) + 8 * k + 2), y & 0xFFFF);
    SetWord(At(::bof3::Img(Prim_VertexScratch_addr) + 8 * k + 4), z & 0xFFFF);
}
// Gte_RotTransPers4 of the four into the POLY_FT4's corners (the tenth word
// Capcom pushes, a second local, is read by no Gte_RotTransPers4 of ours),
// then Gte_PrimDepths4_10.
void Project4(unsigned char* p) {
    long depth;
    SH_CALL(Gte_RotTransPers4)(reinterpret_cast<const short*>(At(::bof3::Img(Prim_VertexScratch_addr))), reinterpret_cast<const short*>(At(::bof3::Img(Prim_VertexScratch_addr) + 8)),
                               reinterpret_cast<const short*>(At(::bof3::Img(Prim_VertexScratch_addr) + 0x10)), reinterpret_cast<const short*>(At(::bof3::Img(Prim_VertexScratch_addr) + 0x18)),
                               reinterpret_cast<float*>(p + 8), reinterpret_cast<float*>(p + 0x18),
                               reinterpret_cast<float*>(p + 0x28), reinterpret_cast<float*>(p + 0x38), &depth);
    SH_CALL(Gte_PrimDepths4_10)(p);
}
// A POLY_FT4 begun at Gfx_PacketNext: Gpu_SetPolyFT4, Gpu_SetShadeTex(0).
unsigned char* BeginQuad() {
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyFT4)(p);
    SH_CALL(Gpu_SetShadeTex)(p, 0);
    return p;
}
// The GTE matrix pushed, then a MATRIX on the stack: its translation
// Gte_RotTrans(point) (+0x14), its rotation Gte_RotMatrix(angles), multiplied
// by Camera_Matrix (Gte_MulMatrix0(camera, m, m)), set as the rotation and
// the translation. Capcom's third word to Gte_RotTrans (a flag out) is read by
// no Gte_RotTrans of ours.
void PushMatrix(const short* point, const short* angles) {
    SH_CALL(Gte_PushMatrix)();
    alignas(4) unsigned char m[0x20];
    SH_CALL(Gte_RotTrans)(point, reinterpret_cast<long*>(m + 0x14));
    SH_CALL(Gte_RotMatrix)(angles, reinterpret_cast<short*>(m));
    SH_CALL(Gte_MulMatrix0)(Camera_Matrix, reinterpret_cast<short*>(m), reinterpret_cast<short*>(m));
    SH_CALL(Gte_SetRotMatrix)(reinterpret_cast<const unsigned long*>(m));
    SH_CALL(Gte_SetTransMatrix)(reinterpret_cast<const unsigned long*>(m));
}

// (v << 12) / 256 as the original computes it (cdq; and edx, 0xFF; add; sar 8).
U Div256(U v) {
    const std::int32_t x = static_cast<std::int32_t>(v << 12);
    return static_cast<U>((x + ((x >> 31) & 0xFF)) >> 8);
}

}  // namespace

// ===========================================================================
// Sub-kind 1 (EffectKind18_States[1])
// ===========================================================================

// ===========================================================================
// Sub-kind 4: EffectKind18_04_States (4)
// ===========================================================================

// ===========================================================================
// Sub-kind 5: EffectKind18_05_States (5)
// ===========================================================================

// ===========================================================================
// Sub-kind 0x1A: EffectKind18_1A_States (5)
// ===========================================================================

// ===========================================================================
// Sub-kind 0x1F: EffectKind18_1F_States (4)
// ===========================================================================

// ===========================================================================
// Sub-kind 6: EffectKind18_06_States (3)
// ===========================================================================

// ===========================================================================
// Sub-kind 7: EffectKind18_07_States (4)
// ===========================================================================

// ===========================================================================
// Sub-kind 8: EffectKind18_08_States (7)
// ===========================================================================

// ===========================================================================
// Sub-kind 9 (EffectKind18_States[9], one state)
// ===========================================================================

// ===========================================================================
// Sub-kind 0xA: EffectKind18_0A_States (5)
// ===========================================================================

void Effect5A_Inject() {
    if (bof3::WantsShadow("effect_5a")) effect_5a::SelfTest();
}

// Round thirteen group E5B (docs/effect_5b.md): the 48 functions of
// analysis/round13_cut.tsv's group E5B and two starts the cut does not list
// (sub-kind 0x13's states 1 and 3, 0x5003A0 and 0x5004A0), each read with
// capstone to its last instruction. Effect_RunObjects (ours) makes each live
// record of Effect_Objects (20 of 0x80 bytes) Sprite_Current and calls
// Effect_KindHandlers[+5]; kind 0x18's EffectKind18_Run jumps by +1 through
// EffectKind18_States, whose entries 11..15, 19 and 79 are the dispatchers here,
// each by +2 through its own table (none bounded by a compare). What each
// sub-kind is, as far as the code says:
//
//   0x0D  four VRAM columns animated by Gpu_SetDrawMove and a glow over a cell
//         (story flag 0xD for column 0, area 48's count 0x92BEE7 for the rest;
//         sound 0x20D)
//   0x0B  eighteen ground tiles that rise with story flag 0x11 and fade out,
//         leaving 0x9039F4 = 4 (sound 0x202)
//   0x0C  sub-kind 0x0D's columns again (other VRAM, story flags by column, no
//         glow; sound 0x201)
//   0x0E  a gate at a fixed place (cells (1, 21), (1, 22)) by row 9's flag 0xC,
//         lowered and raised as the leader comes and goes (sounds 0x200, 0x201)
//   0x13  a wall panel of four variants by row 9's flags and a flickering line
//   0x0F  gates of eight variants (+0x36), lowered by Cond_ByteFE or the
//         leader, their cells 0x52 / 0xA1 in AreaMap_Bytes
//   0x4F  the same gates for the other axis, opened by Cond_ByteFE only
//
// Every call goes through the harness (SH_CALL), so the start-up fuzz can stand
// recorders in for ours as for the originals' copies. No divergence: each is a
// faithful replacement. Where the original jumps through a state table past its
// end or reads one of its byte arrays past its extent by a byte of the record,
// ours aborts with a message (docs/effect_5b.md section 7).
#include "game/effect_5b.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_5b_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_5b::at;
using U = std::uint32_t;
using I = std::int32_t;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

unsigned char* S() { return Sprite_Current; }
U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }
void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<I>(v)); }
unsigned char& B(U a) { return *At(a); }
U AddressOf(const void* p) { return Low32(p); }

using Handler = scenario_harness::Handler;

// Prim_VertexScratch's four SVECTORs (0x9037A0, A8, B0, B8): word k of vertex v.
// kScratch: now Prim_VertexScratch (symbols.toml).
U VW(unsigned v, unsigned k) { return Word(At(::bof3::Img(Prim_VertexScratch_addr) + 8 * v + 2 * k)); }
I VS(unsigned v, unsigned k) { return static_cast<std::int16_t>(VW(v, k)); }
void SetV(unsigned v, unsigned k, U value) { SetWord(At(::bof3::Img(Prim_VertexScratch_addr) + 8 * v + 2 * k), value); }
const short* VertexEffect5b(unsigned v) { return reinterpret_cast<const short*>(At(::bof3::Img(Prim_VertexScratch_addr) + 8 * v)); }

// cdq; xor eax, edx; sub eax, edx (INT_MIN stays itself)
I Abs(I v) {
    const U sign = static_cast<U>(v >> 31);
    return static_cast<I>((static_cast<U>(v) ^ sign) - sign);
}
// movsx eax, ax; cdq; sub eax, edx; sar eax, 1: an elevation's low word halved
// toward zero
I Half(long e) {
    const I w = static_cast<std::int16_t>(static_cast<U>(e));
    return (w - (w >> 31)) >> 1;
}
// cdq; sub eax, edx; sar eax, 1 on a whole dword
I Halve(I v) { return static_cast<I>(static_cast<U>(v) - static_cast<U>(v >> 31)) >> 1; }

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 2]; jmp [table + eax * 4]:
// the table's `entries` handlers read in place (the fuzz swaps the cells for
// recorders); a Fatal past them, where the original jumps through the dword
// after - the next table, a data dword or a null.
void DispatchEffect5b(const char* who, U table, unsigned entries) {
    const unsigned sub = Sprite_Current[2];
    if (sub >= entries)
        bof3::Fatal("%s: sub-state byte +2 is %u, past the %u entries of 0x%X - the original jumps through the dword "
                    "after (docs/effect_5b.md section 7)",
                    who, sub, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(At(table + 4 * sub))))();
}

// One of the image's byte arrays by an index the record supplies (unchecked by
// the original, which reads the next array or table past its extent).
unsigned Byte(const char* who, U array, unsigned extent, U index) {
    if (index >= extent)
        bof3::Fatal("%s: index %d past the %u bytes of 0x%X - the original reads what follows "
                    "(docs/effect_5b.md section 7)",
                    who, static_cast<int>(index), extent, (unsigned)::bof3::VaOf(array));
    return B(array + index);
}

// --- the CRT's _ftol 0x5B9550 on st(0) (truncation set in a copy of the control
// word for one fistp to 64 bits, the word put back), the low dword -------------

unsigned char* Prim() { return Gfx_PacketNext; }
unsigned char* Flags() { return At(::bof3::Img(Cond_Flags_addr) + 0xa0); }
unsigned char* Row9() { return At(::bof3::Img(Cond_Flags_addr) + 0x48); }

// The four-vertex projection every quad here makes of Prim_VertexScratch into a
// primitive whose vertices are `stride` apart from +8 (0x10 textured, 0xC flat).
void Project4Effect5b(unsigned char* prim, unsigned stride) {
    long p;
    SH_CALL(Gte_RotTransPers4)(VertexEffect5b(0), VertexEffect5b(1), VertexEffect5b(2), VertexEffect5b(3), reinterpret_cast<float*>(prim + 8),
                               reinterpret_cast<float*>(prim + 8 + stride), reinterpret_cast<float*>(prim + 8 + 2 * stride),
                               reinterpret_cast<float*>(prim + 8 + 3 * stride), &p);
}

// AreaMap_Bytes + z * width + x (width the area header's byte 0), as the
// original's 32-bit sums: no bound, as Capcom's.
unsigned char* Cell(I x, I z) {
    const U width = AreaMap_Header[0];
    return const_cast<unsigned char*>(AreaMap_Bytes.Native()) + (static_cast<U>(z) * width + static_cast<U>(x));
}

// A VRAM move of an 8 x 0x78 column: the rect at (frame + 1) * 8 + `from`,
// 0x160, onto (`to`, 0x160); committed to slot 6.
void MoveColumn(U from, U to) {
    unsigned char rect[8];
    SetWord(rect, from);
    SetWord(rect + 2, 0x160);
    SetWord(rect + 4, 8);
    SetWord(rect + 6, 0x78);
    SH_CALL(Gpu_SetDrawMove)(Prim(), rect, to, 0x160);
    SH_CALL(Gfx_CommitPrim)(6, 0x18);
}

// +9 up one; when past `last`, +9 0 (when `clear`) and +2 to `next` (or up one
// when next is negative).
void Step(unsigned last, bool clear, int next) {
    unsigned char* const s = S();
    s[9] = static_cast<unsigned char>(s[9] + 1);
    if (S()[9] <= last) return;
    if (clear) S()[9] = 0;
    if (next < 0) S()[2] = static_cast<unsigned char>(S()[2] + 1);
    else S()[2] = static_cast<unsigned char>(next);
}

}  // namespace

// ===========================================================================
// Sub-kind 0x0D (EffectKind18_States[13])
// ===========================================================================

// ===========================================================================
// Sub-kind 0x0B (EffectKind18_States[11])
// ===========================================================================

// ===========================================================================
// Sub-kind 0x0C (EffectKind18_States[12])
// ===========================================================================

// ===========================================================================
// Sub-kind 0x0E (EffectKind18_States[14])
// ===========================================================================

// ===========================================================================
// Sub-kind 0x13 (EffectKind18_States[19])
// ===========================================================================

// ===========================================================================
// Sub-kinds 0x0F and 0x4F (EffectKind18_States[15], [79]): the gates
// ===========================================================================

void Effect5B_Inject() {
    if (bof3::WantsShadow("effect_5b")) effect_5b::SelfTest();
}

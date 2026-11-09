// Round thirteen group E5E (docs/effect_5e.md): the 51 functions of
// analysis/round13_cut.tsv's group E5E, 0x506A10..0x508CBB, and three starts
// the cut does not list (sub-kind 0x23's shared draw 0x506AB0, the dispatchers
// 0x506BD0 and 0x507620 of EffectKind18_States 36 and 63), each read with
// capstone to its last instruction (2026-10-03). Effect_RunObjects (ours)
// makes each live record of Effect_Objects (20 of 0x80 bytes) Sprite_Current
// and calls Effect_KindHandlers[+5]; kind 0x18's handler jumps through
// EffectKind18_States by +1 (EffectKind18_Start sets +1 from the record's
// +0xB, the sub-kind), and each sub-kind here is a dispatcher by +2 through its
// own table (none bounded by a compare) and the states it names. What each
// sub-kind is, as far as the code says:
//
//   0x23 (35)  a textured panel at the record's map point: it waits for story
//              flag 0x28 and then slides 4 a frame until +0x30 reaches 0x100
//              (released) - or is released at once when the flag is already set
//   0x24 (36)  at the counter 0x903848 = 5 (story row 0x50 bit 4 set): four
//              textured quads round the record's point, three beats of 16
//              frames with sounds, seven shaded trails drawn away and four
//              drawn out, then a spin that speeds up (random jitter, the
//              counter 6 to 7) and fades; the counter raised twice
//   0x25 (37)  a lid of two leaves over a map cell (eight variants by +0x36):
//              it opens (+0x30 to 0x80) while the leader stands near, waits for
//              the leader to leave, closes; sounds 0x200 / 0x201 when no
//              message is up; a textured cap when the variant has one
//   0x26 (38)  the same with five variants of its own (the texture word in
//              +0x20, the height less +0x2E)
//   0x39 (57)  three textured quads in two halves that part (+0x30 to 0x80)
//              once story flag 0x5A is set, then hold with Cond_ByteFE 1
//   0x3F (63)  a scene on the counter's cues 0x26..0x3B: CLUT row 4 dimmed, a
//              sky gradient over the frame that warms and fades, a trail,
//              sub-kind 0x24's spin, and a white frame to end (Cond row 4 at
//              full, the leader's shade cleared, released)
//
// Every call goes through the harness (SH_CALL), so the start-up fuzz can stand
// recorders in for ours as for the originals' copies. Sprite_Current is read
// again wherever the original reads [0x937F88] again after a call. The two
// full-frame fills (0x507BC0's white POLY_F4, 0x507CB0's gradient POLY_G4) are
// DIV-0041's listed sites 0x507BDC and 0x507CE3 (section 3c): they span
// Widescreen_FillX() .. Widescreen_FillX() + Widescreen_FillWidth(), which is the
// original's 0 .. 320 until Widescreen_ArmFills has run and whenever the
// picture is narrow. Otherwise no divergence: each is a faithful replacement.
// Where the original jumps through a state table past its end or indexes one of
// its .data tables past its entries, ours aborts with a message
// (docs/effect_5e.md section 6).
#include "game/effect_5e.h"

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
using U = std::uint32_t;
using move_script::At;
using move_script::Long;
using move_script::SetWord;
using move_script::Word;
using Handler = scenario_harness::Handler;

unsigned char* S() { return Sprite_Current; }
U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }
U UL(U a) { return UL(At(a)); }
U W(const unsigned char* p) { return Word(p); }
std::int32_t SW(const unsigned char* p) { return static_cast<std::int16_t>(Word(p)); }
// movsx of a .data byte, as the 32 bits the original holds
U SB(U a) { return static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(*At(a)))); }
unsigned char& B(U a) { return *At(a); }
U AddressOf(const void* p) { return Low32(p); }
U Sar(U v, unsigned n) { return static_cast<U>(static_cast<std::int32_t>(v) >> n); }
// cdq; xor eax, edx; sub eax, edx: |v| as the original's signed compare sees
// it (0x80000000 stays negative).
std::int32_t Abs(U v) {
    const U m = Sar(v, 31);
    return static_cast<std::int32_t>((v ^ m) - m);
}
unsigned char* Vtx() { return reinterpret_cast<unsigned char*>(Prim_VertexScratch); }
const short* Vector(U offset) { return reinterpret_cast<const short*>(Vtx() + offset); }
float* F(unsigned char* p) { return reinterpret_cast<float*>(p); }

// libgte's MATRIX: nine s16 of rotation, two bytes of padding, a translation
// of three s32 (the originals' 0x20-byte local).
struct Matrix {
    short m[9];
    short pad;
    long t[3];
};
static_assert(sizeof(Matrix) == 0x20, "a MATRIX is 32 bytes");

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

// ===========================================================================
// Sub-kind 0x23: EffectKind18_States[35] (0x6540F8), EffectKind18Sub23_States
// (three) by +2
// ===========================================================================

// ===========================================================================
// Sub-kind 0x24: EffectKind18_States[36] (0x6540FC), EffectKind18Sub24_States
// (eight) by +2
// ===========================================================================

// ===========================================================================
// Sub-kind 0x3F: EffectKind18_States[63] (0x654168), EffectKind18Sub3F_States
// (sixteen) by +2
// ===========================================================================

// ===========================================================================
// Sub-kinds 0x25 and 0x26: EffectKind18_States[37] (0x654100) and [38]
// (0x654104), EffectKind18Sub25_States and EffectKind18Sub26_States (five
// each) by +2; sub-state 3 of both (and of E5F's table 0x65E9A8) is one function
// ===========================================================================

namespace {

// Whether the leader (ObjTrio record 0's x, z) stands at the record's cell:
// with +8 set (the lid lies along x) its z within `across` of the cell's
// middle row (+0x3A + 1) << 16 | 0x8000, and its x within `along` of the cell
// +0x36 << 16 or the next; with +8 clear the same with x and z swapped.
bool LeaderAt(const unsigned char* s, std::int32_t across, std::int32_t along) {
    const bool lengthwise = s[8] != 0;
    const U first = lengthwise ? static_cast<std::uint32_t>(ObjTrio[0].z) : static_cast<std::uint32_t>(ObjTrio[0].x);
    const U middle = ((static_cast<U>(SW(s + (lengthwise ? 0x3A : 0x36))) + 1u) << 16) | 0x8000u;
    if (Abs(first - middle) > across) return false;
    const U second = (lengthwise ? static_cast<std::uint32_t>(ObjTrio[0].x) : static_cast<std::uint32_t>(ObjTrio[0].z)) - (static_cast<U>(SW(s + (lengthwise ? 0x36 : 0x3A))) << 16);
    if (Abs(second) <= along) return true;
    return Abs(second - 0x10000u) <= along;
}

// The start of both: +8 = (+0x3A == 0); the variant +0x36 picks the cell
// (+0x36, +0x3A), the height +0x3E, the page +0x2E, the lid +0x32 (and for 0x26
// the texture word +0x20); +0x30 0; +2 up; when the leader is at the cell,
// open at once: +0x30 0x80, +2 = 3.
struct Variants {
    const char* who;
    U cells, heights, pages, lids, words;
    unsigned count;
};
void StartLid(const Variants& t) {
    unsigned char* s = S();
    const std::int32_t v = static_cast<std::int32_t>(static_cast<std::int16_t>(rv::As<records::EffectObject>(s)->x_whole));
    s[8] = static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->z_whole) == 0 ? 1 : 0;
    if (v < 0 || v >= static_cast<std::int32_t>(t.count))
        bof3::Fatal("%s: the variant +0x36 is %d, past the %u of its tables - the original reads the .data after "
                    "(docs/effect_5e.md section 6)",
                    t.who, v, t.count);
    const U i = static_cast<U>(v);
    s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->x_whole, B(t.cells + 2 * i));
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->z_whole, B(t.cells + 2 * i + 1));
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->height_whole, W(At(t.heights + 2 * i)));
    if (t.words) rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->work_20, UL(t.words + 4 * i));
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_2e, W(At(t.pages + 2 * i)));
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_32, B(t.lids + i));
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_30, 0);
    Step();
    s = S();
    if (LeaderAt(s, 0x8000, 0x10000)) {
        rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_30, 0x80);
        S()[2] = 3;
    }
}

// Sub-state 1 of both: when the leader is at the cell, sound 0x200 unless
// Field_Request is set, +2 up.
void WaitNear() {
    unsigned char* s = S();
    if (!LeaderAt(s, 0x8000, 0x10000)) return;
    if (Field_Request == 0) {
        SH_CALL(Sound_PlayEffect)(0x200);
        s = S();
    }
    s[2] = static_cast<unsigned char>(s[2] + 1);
}
// Sub-state 2 of both: +0x30 up 0x10; at 0x80, +2 up. Answers the draw's dy:
// 0 with a lid (+0x32), else -1.
int Open() {
    unsigned char* s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_30, static_cast<std::uint32_t>(rv::As<records::EffectWork>(s)->work_30) + 0x10);
    s = S();
    if (static_cast<std::int32_t>(static_cast<std::int16_t>(rv::As<records::EffectWork>(s)->work_30)) >= 0x80) {
        s[2] = static_cast<unsigned char>(s[2] + 1);
        s = S();
    }
    return static_cast<std::uint32_t>(rv::As<records::EffectWork>(s)->work_32) != 0 ? 0 : -1;
}
// Sub-state 4 of both: +0x30 down 0x10; at 0 or below, sound 0x201 unless
// Field_Request is set and +2 = 1.
int Close() {
    unsigned char* s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_30, static_cast<std::uint32_t>(rv::As<records::EffectWork>(s)->work_30) - 0x10);
    s = S();
    if (static_cast<std::int32_t>(static_cast<std::int16_t>(rv::As<records::EffectWork>(s)->work_30)) <= 0) {
        if (Field_Request == 0) SH_CALL(Sound_PlayEffect)(0x201);
        S()[2] = 1;
    }
    return static_cast<std::uint32_t>(rv::As<records::EffectWork>(S())->work_32) != 0 ? 0 : -1;
}

// The lid's texture: the dword `index` of the table (entry 0 is the leaves'
// signs, which the lid never asks for: the callers skip a lid of 0).
U LidTexture(const char* who, U table, unsigned count, std::int32_t index) {
    if (index < 0 || index >= static_cast<std::int32_t>(count))
        bof3::Fatal("%s: the lid +0x32 is %d, past the %u textures of 0x%X - the original reads the .data after "
                    "(docs/effect_5e.md section 6)",
                    who, index, count, (unsigned)::bof3::VaOf(table));
    return UL(table + 4 * static_cast<U>(index));
}

// The two leaves of both draws: each leaf i (0, 1) moves by +0x30 times the
// sign byte i of the table (1, -1) across the cell; along x when +8 is set
// (the leaf's x from (i + +0x36) << 7, its z edge -0x4040 / -0x3FC0), else
// along z ((+0x3A - i) << 7, -0x3F40 / -0x3FC0).
void Leaf(U i, U signs) {
    unsigned char* const v = Vtx();
    unsigned char* const s = S();
    if (s[8]) {
        const U sign = SB(signs + i);
        const U base = (i + static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->x_whole)) << 7;
        const U a = base - static_cast<std::uint32_t>(rv::As<records::EffectWork>(s)->work_30) * sign - 0x4040u;
        rv::Store<std::uint32_t>(rv::As<records::SVector>(v)[2].x, a);
        rv::Store<std::uint32_t>(rv::As<records::SVector>(v)->x, a);
        const U b = base - static_cast<std::uint32_t>(rv::As<records::EffectWork>(s)->work_30) * sign - 0x3FC0u;
        rv::Store<std::uint32_t>(rv::As<records::SVector>(v)[3].x, b);
        rv::Store<std::uint32_t>(rv::As<records::SVector>(v)[1].x, b);
    } else {
        const U sign = SB(signs + i);
        const U base = (static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->z_whole) - i) << 7;
        const U a = base + static_cast<std::uint32_t>(rv::As<records::EffectWork>(s)->work_30) * sign - 0x3F40u;
        rv::Store<std::uint32_t>(rv::As<records::SVector>(v)[2].y, a);
        rv::Store<std::uint32_t>(rv::As<records::SVector>(v)->y, a);
        const U b = base + static_cast<std::uint32_t>(rv::As<records::EffectWork>(s)->work_30) * sign - 0x3FC0u;
        rv::Store<std::uint32_t>(rv::As<records::SVector>(v)[3].y, b);
        rv::Store<std::uint32_t>(rv::As<records::SVector>(v)[1].y, b);
    }
}

// The lid's corners over the cell.
void LidCorners(const unsigned char* s) {
    unsigned char* const v = Vtx();
    if (s[8]) {
        const U a = (W(s + 0x36) << 7) - 0x40C0u, b = (W(s + 0x36) << 7) - 0x4040u;
        rv::Store<std::uint32_t>(rv::As<records::SVector>(v)[2].x, a);
        rv::Store<std::uint32_t>(rv::As<records::SVector>(v)->x, a);
        rv::Store<std::uint32_t>(rv::As<records::SVector>(v)[3].x, b);
        rv::Store<std::uint32_t>(rv::As<records::SVector>(v)[1].x, b);
    } else {
        const U a = (W(s + 0x3A) << 7) - 0x4040u, b = (W(s + 0x3A) << 7) - 0x40C0u;
        rv::Store<std::uint32_t>(rv::As<records::SVector>(v)[2].y, a);
        rv::Store<std::uint32_t>(rv::As<records::SVector>(v)->y, a);
        rv::Store<std::uint32_t>(rv::As<records::SVector>(v)[3].y, b);
        rv::Store<std::uint32_t>(rv::As<records::SVector>(v)[1].y, b);
    }
}

// The fixed edge of the cell both draws start with: with +8 set the z
// (+0x3A << 7) - 0x3FC0 in all four vectors' y... (the record's cell row),
// else the x.
void CellEdge(const unsigned char* s) {
    unsigned char* const v = Vtx();
    if (s[8]) {
        const U y = (W(s + 0x3A) << 7) - 0x3FC0u;
        rv::Store<std::uint32_t>(rv::As<records::SVector>(v)[3].y, y);
        rv::Store<std::uint32_t>(rv::As<records::SVector>(v)[2].y, y);
        rv::Store<std::uint32_t>(rv::As<records::SVector>(v)[1].y, y);
        rv::Store<std::uint32_t>(rv::As<records::SVector>(v)->y, y);
    } else {
        const U x = (W(s + 0x36) << 7) - 0x3FC0u;
        rv::Store<std::uint32_t>(rv::As<records::SVector>(v)[3].x, x);
        rv::Store<std::uint32_t>(rv::As<records::SVector>(v)[2].x, x);
        rv::Store<std::uint32_t>(rv::As<records::SVector>(v)[1].x, x);
        rv::Store<std::uint32_t>(rv::As<records::SVector>(v)->x, x);
    }
}

}  // namespace

// ===========================================================================
// Sub-kind 0x39: EffectKind18_States[57] (0x654150), EffectKind18Sub39_States
// (four) by +2
// ===========================================================================

void Effect5E_Inject() {
    if (bof3::WantsShadow("effect_5e")) effect_5e::SelfTest();
}

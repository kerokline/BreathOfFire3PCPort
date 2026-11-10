// Moved here by tools/move_funcs.py from effect_5e.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_18_sub_26.h"

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

const Variants kSub26 = {"EffectKind18Sub26_Start", ::bof3::Img(at::kSub26Cells.v), ::bof3::Img(at::kSub26Heights.v),
                         ::bof3::Img(at::kSub26Lifts.v), ::bof3::Img(at::kSub26Lids.v), ::bof3::Img(at::kSub26Pages.v),
                         at::kSub26Variants};

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

// original 0x5083B0 (EffectKind18_States[38]): jmp [EffectKind18Sub26_States +
// +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub26_Run(void) {
    Dispatch("EffectKind18Sub26_Run", AddressOf(EffectKind18Sub26_States), EffectKind18Sub26_States_count);
}

// original 0x5083D0 (sub-state 0): as sub-kind 0x25's with its own five
// variants (and the texture word +0x20). The draw (dy 0).
extern "C" void __cdecl EffectKind18Sub26_Start(void) {
    StartLid(kSub26);
    SH_CALL(EffectKind18Sub26_Draw)(0);
}

// original 0x508540 (sub-state 1): waits for the leader at the cell. The draw.
extern "C" void __cdecl EffectKind18Sub26_WaitNear(void) {
    WaitNear();
    SH_CALL(EffectKind18Sub26_Draw)(0);
}

// original 0x508630 (sub-state 2): opens. The draw.
extern "C" void __cdecl EffectKind18Sub26_Open(void) {
    const int dy = Open();
    SH_CALL(EffectKind18Sub26_Draw)(dy);
}

// original 0x508670 (sub-state 3 of sub-kinds 0x25 and 0x26, and of E5F's table
// 0x65E9A8): +2 up once the leader is away from the cell - more than 0x20000
// across, or along it more than 0x20000 from both the cell and the next.
// Nothing drawn.
extern "C" void __cdecl EffectKind18Sub25_WaitAway(void) {
    unsigned char* const s = S();
    if (!LeaderAt(s, 0x20000, 0x20000)) s[2] = static_cast<unsigned char>(s[2] + 1);
}

// original 0x508730 (sub-state 4): closes. The draw.
extern "C" void __cdecl EffectKind18Sub26_Close(void) {
    const int dy = Close();
    SH_CALL(EffectKind18Sub26_Draw)(dy);
}

// original 0x508790 (cdecl (dy)): as sub-kind 0x25's draw, but the leaves from
// +0x3E - +0x2E to +0x3E, their texture word (+0x20 + i) | +8 << 21, and the
// lid's height left as the leaves had it, its texture from kSub26Textures.
extern "C" void __cdecl EffectKind18Sub26_Draw(int dy) {
    unsigned char* s = S();
    unsigned char* const v = Vtx();
    CellEdge(s);
    const U low = static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->height_whole) - static_cast<std::uint32_t>(rv::As<records::EffectWork>(s)->work_2e);
    rv::Store<std::uint32_t>(rv::As<records::SVector>(v)[1].z, low);
    rv::Store<std::uint32_t>(rv::As<records::SVector>(v)->z, low);
    const U high = static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->height_whole);
    rv::Store<std::uint32_t>(rv::As<records::SVector>(v)[3].z, high);
    rv::Store<std::uint32_t>(rv::As<records::SVector>(v)[2].z, high);
    for (U i = 0; i < 2; ++i) {
        unsigned char* const p = TexturedQuad();
        Leaf(i, ::bof3::Img(at::kSub26Textures.v));
        Project4(p);
        SH_CALL(Gte_PrimDepths4_10)(p);
        s = S();
        const U word = (static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->work_20) + i) | (static_cast<U>(s[8]) << 21);
        SH_CALL(Prim_SetTexture)(word, p, 1);
        LinkAtRecord(dy, 0x48);
    }
    s = S();
    if (static_cast<std::uint32_t>(rv::As<records::EffectWork>(s)->work_32) == 0) return;
    unsigned char* const p = TexturedQuad();
    LidCorners(S());
    Project4(p);
    SH_CALL(Gte_PrimDepths4_10)(p);
    const U texture = LidTexture("EffectKind18Sub26_Draw", ::bof3::Img(at::kSub26Textures.v), at::kSub26TextureCount, static_cast<std::int32_t>(static_cast<std::int16_t>(rv::As<records::EffectWork>(S())->work_32)));
    SH_CALL(Prim_SetTexture)(texture, p, 1);
    LinkAtRecord(0, 0x48);
}

// original 0x508A80 (EffectKind18_States[57]): jmp [EffectKind18Sub39_States +
// +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub39_Run(void) {
    Dispatch("EffectKind18Sub39_Run", AddressOf(EffectKind18Sub39_States), EffectKind18Sub39_States_count);
}

// original 0x508AA0 (sub-state 0): story flag 0x5A set: +0x30 0x80, +2 = 3, the
// draw of the halves (0, -2); clear: +0x30 0, +2 up, the draws (0, 1), (1, 1).
extern "C" void __cdecl EffectKind18Sub39_Start(void) {
    if (SH_CALL(Flags_Test)(At(::bof3::Img(Cond_Flags_addr) + 0xa0), at::kSub39Flag)) {
        rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_30, 0x80);
        S()[2] = 3;
        SH_CALL(EffectKind18Sub39_Draw)(0, -2);
        return;
    }
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_30, 0);
    Step();
    SH_CALL(EffectKind18Sub39_Draw)(0, 1);
    SH_CALL(EffectKind18Sub39_Draw)(1, 1);
}

// original 0x508B00 (sub-state 1): once story flag 0x5A is set, sound 0x208
// unless Field_Request is set, +2 up. The draws (0, 1), (1, 1).
extern "C" void __cdecl EffectKind18Sub39_WaitFlag(void) {
    if (SH_CALL(Flags_Test)(At(::bof3::Img(Cond_Flags_addr) + 0xa0), at::kSub39Flag)) {
        if (Field_Request == 0) SH_CALL(Sound_PlayEffect)(0x208);
        Step();
    }
    SH_CALL(EffectKind18Sub39_Draw)(0, 1);
    SH_CALL(EffectKind18Sub39_Draw)(1, 1);
}

// original 0x508B50 (sub-state 2): +0x30 up 8; at 0x80, +2 up. The draws (0,
// -1), (1, 1).
extern "C" void __cdecl EffectKind18Sub39_Open(void) {
    unsigned char* s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_30, static_cast<std::uint32_t>(rv::As<records::EffectWork>(s)->work_30) + 8);
    s = S();
    if (static_cast<std::int32_t>(static_cast<std::int16_t>(rv::As<records::EffectWork>(s)->work_30)) >= 0x80) s[2] = static_cast<unsigned char>(s[2] + 1);
    SH_CALL(EffectKind18Sub39_Draw)(0, -1);
    SH_CALL(EffectKind18Sub39_Draw)(1, 1);
}

// original 0x508B80 (sub-state 3): Cond_ByteFE 1; the draw (2, 0).
extern "C" void __cdecl EffectKind18Sub39_Hold(void) {
    Cond_ByteFE = 1;
    SH_CALL(EffectKind18Sub39_Draw)(2, 0);
}

// original 0x508BA0 (cdecl (variant, dy)): three POLY_FT4s from kSub39Vertices
// (four (x, y, z) each), x moved by +0x30 times the sign byte (variant & 1) of
// kSub39Signs and by (variant & 1) << 7, projected, textured with dword quad +
// 3 * variant of kSub39Textures (nine; past them ours aborts), each linked at
// the record's point with dy (0x48 bytes).
extern "C" void __cdecl EffectKind18Sub39_Draw(U variant, int dy) {
    const U sign_at = ::bof3::Img(at::kSub39Signs.v) + (variant & 1);
    const U shift = (variant & 1) << 7;
    unsigned char* const v = Vtx();
    for (U q = 0; q < at::kSub39Quads; ++q) {
        unsigned char* const p = TexturedQuad();
        const U sign = SB(sign_at);
        const unsigned char* const s = S();
        for (U k = 0; k < 4; ++k) {
            const U vertex = ::bof3::Img(at::kSub39Vertices.v) + 0x18 * q + 6 * k;
            const U x = W(At(vertex)) - W(s + 0x30) * sign + shift;
            SetWord(v + 8 * k + 2, W(At(vertex + 2)));
            SetWord(v + 8 * k, x);
            SetWord(v + 8 * k + 4, W(At(vertex + 4)));
        }
        Project4(p);
        SH_CALL(Gte_PrimDepths4_10)(p);
        const U index = q + 3 * variant;
        if (index >= at::kSub39TextureCount)
            bof3::Fatal("EffectKind18Sub39_Draw: variant %d asks for texture %u of the %u at 0x%X - the original reads "
                        "the .data after (docs/effect_5e.md section 6)",
                        static_cast<int>(variant), index, at::kSub39TextureCount, at::kSub39Textures.v);
        SH_CALL(Prim_SetTexture)(UL(::bof3::Img(at::kSub39Textures.v) + 4 * index), p, 1);
        LinkAtRecord(dy, 0x48);
    }
}

void EffectKind18Sub26_Inject() {
    BOF3_INJECT(EffectKind18Sub26_Run);
    BOF3_INJECT(EffectKind18Sub26_Start);
    BOF3_INJECT(EffectKind18Sub26_WaitNear);
    BOF3_INJECT(EffectKind18Sub26_Open);
    BOF3_INJECT(EffectKind18Sub25_WaitAway);
    BOF3_INJECT(EffectKind18Sub26_Close);
    BOF3_INJECT(EffectKind18Sub26_Draw);
    BOF3_INJECT(EffectKind18Sub39_Run);
    BOF3_INJECT(EffectKind18Sub39_Start);
    BOF3_INJECT(EffectKind18Sub39_WaitFlag);
    BOF3_INJECT(EffectKind18Sub39_Open);
    BOF3_INJECT(EffectKind18Sub39_Hold);
    BOF3_INJECT(EffectKind18Sub39_Draw);
}

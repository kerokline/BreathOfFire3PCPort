// Moved here by tools/move_funcs.py from effect_5g.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_18_sub_2b.h"

#include <bit>
#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/effect_5g.h"
#include "game/effect_5g_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_5g::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

using Handler = scenario_harness::Handler;

unsigned char* S() { return Sprite_Current; }

U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }

void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }

std::int32_t S16(const unsigned char* p) { return static_cast<std::int16_t>(Word(p)); }

U AddressOf(const void* p) { return Low32(p); }

unsigned char* Vertices() { return reinterpret_cast<unsigned char*>(Prim_VertexScratch); }

const short* Vertex(unsigned i) { return reinterpret_cast<const short*>(Vertices() + 8 * i); }

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 2]; jmp [table + eax
// * 4]: the table's `entries` handlers read in place (the fuzz swaps the cells
// for recorders); a Fatal past them, where the original jumps through the dword
// after - another table or data.
void Dispatch(const char* who, U table, unsigned entries) {
    const unsigned sub = Sprite_Current[2];
    if (sub >= entries)
        bof3::Fatal("%s: sub-state byte +2 is %u, past the %u entries of 0x%X - the original jumps through the dword "
                    "after (docs/effect_5g.md section 6)",
                    who, sub, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(At(table + 4 * sub))))();
}

// The variant the spawn's x cell (+0x36, movsx) names, checked against the
// room its tables have.
U Variant(const char* who, const unsigned char* s, unsigned room) {
    const std::int32_t v = S16(s + 0x36);
    if (v < 0 || v >= static_cast<std::int32_t>(room))
        bof3::Fatal("%s: the variant +0x36 is %d, past the %u its tables have room for - the original reads on into "
                    "the next table (docs/effect_5g.md section 6)",
                    who, (int)v, room);
    return static_cast<U>(v);
}

// cdq; xor eax, edx; sub eax, edx; cmp eax, n; jg: |d| as the original
// computes it - 0x80000000 stays itself, negative, and so counts as near.
std::int32_t Abs(U d) {
    const U m = static_cast<std::int32_t>(d) < 0 ? 0xFFFFFFFFu : 0u;
    return static_cast<std::int32_t>((d ^ m) - m);
}

// A point against a cell's centre on one axis: point - ((cell + 1) << 16 |
// 0x8000) (inc; shl 16; or dh, 0x80).
U FromCentre(U point, std::int32_t cell) { return point - ((static_cast<U>(cell + 1) << 16) | 0x8000u); }

// ... and against its edge: point - (cell << 16).
U FromEdge(U point, std::int32_t cell) { return point - (static_cast<U>(cell) << 16); }

// The tests every waiting state makes: |a| within `first`, then |b| or |b -
// 0x10000| within `second` (the cell or the one past it).
bool Within(U a, std::int32_t first, U b, std::int32_t second) {
    if (Abs(a) > first) return false;
    if (Abs(b) <= second) return true;
    return Abs(b - 0x10000u) <= second;
}

U LeaderX() { return static_cast<std::uint32_t>(ObjTrio[0].x); }

U LeaderZ() { return static_cast<std::uint32_t>(ObjTrio[0].z); }

// The leader at the record's cell: across the x axis first (the centre), then
// z - or, for +8 set (sub-kinds 0x2C / 0x4A), z first, then x.
bool LeaderAt(const unsigned char* s, bool z_first, std::int32_t first, std::int32_t second) {
    const std::int32_t cx = S16(s + 0x36), cz = S16(s + 0x3A);
    if (z_first) return Within(FromCentre(LeaderZ(), cz), first, FromEdge(LeaderX(), cx), second);
    return Within(FromCentre(LeaderX(), cx), first, FromEdge(LeaderZ(), cz), second);
}

// A vertex's ground point: (s16 + 0x4000) << 9, the 16.16 the map reads.
long Ground(const unsigned char* word) { return static_cast<long>(static_cast<U>(S16(word) + 0x4000) << 9); }

// -(height / 2) - +0x3E, the low word (movsx; cdq; sub; sar 1; neg; sub ax).
U Lift(long height, const unsigned char* s) {
    const std::int32_t half = static_cast<std::int16_t>(static_cast<U>(height) & 0xFFFFu) / 2;
    return 0u - static_cast<U>(half) - Word(s + 0x3E);
}

// The four corners' heights from the ground under them, as every panel draw
// lifts them: corners 0 and 2 at vertex 0's point (two reads, the top 0x180
// above), corners 1 and 3 at vertex 1's, Sprite_Current read after each call.
void Grounded() {
    unsigned char* const v = Vertices();
    const long x0 = Ground(v), y0 = Ground(v + 2);
    long h = SH_CALL(AreaMap_Elevation)(x0, y0);
    rv::Store<std::uint32_t>(rv::As<records::SVector>(v)->z, Lift(h, S()) - 0x180u);
    h = SH_CALL(AreaMap_Elevation)(x0, y0);
    const long x1 = Ground(v + 8), y1 = Ground(v + 0xA);
    rv::Store<std::uint32_t>(rv::As<records::SVector>(v)[2].z, Lift(h, S()));
    h = SH_CALL(AreaMap_Elevation)(x1, y1);
    rv::Store<std::uint32_t>(rv::As<records::SVector>(v)[1].z, Lift(h, S()) - 0x180u);
    h = SH_CALL(AreaMap_Elevation)(x1, y1);
    rv::Store<std::uint32_t>(rv::As<records::SVector>(v)[3].z, Lift(h, S()));
}

// The quad's four corners projected into the POLY_FT4 at p, its depths set.
// The original also pushes a tenth pointer (a flag local) the projection
// does not read.
void Project(unsigned char* p) {
    long depth;
    SH_CALL(Gte_RotTransPers4)(Vertex(0), Vertex(1), Vertex(2), Vertex(3), reinterpret_cast<float*>(p + 8),
                               reinterpret_cast<float*>(p + 0x18), reinterpret_cast<float*>(p + 0x28),
                               reinterpret_cast<float*>(p + 0x38), &depth);
    SH_CALL(Gte_PrimDepths4_10)(p);
}

// The draw mode the panels put first: Gpu_SetDrawMode(prim, 0, 0, 0x95, 0).
void PanelMode(unsigned char* prim) { SH_CALL(Gpu_SetDrawMode)(prim, 0, 0, 0x95, 0); }

// A slide of +0x30 by `by`; answers whether it has reached `end` (>= when
// opening, <= 0 when closing; signed words).
bool Opened(std::int32_t end) {
    unsigned char* const s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_30, rv::As<records::EffectWork>(s)->work_30 + 0x10u);
    return S16(S() + 0x30) >= end;
}

bool Closed() {
    unsigned char* const s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_30, rv::As<records::EffectWork>(s)->work_30 - 0x10u);
    return S16(S() + 0x30) <= 0;
}

// Closed: the sound unless a message is up (Field_Request), +2 = 1 (read after
// the sound).
void Shut(unsigned sound) {
    if (Field_Request == 0) SH_CALL(Sound_PlayEffect)(static_cast<unsigned short>(sound));
    S()[2] = 1;
}

}  // namespace

// original 0x50AF90 (hidden in E5F's 0x50AD70): jmp [EffectKind18Sub2B_States +
// +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub2B_Run(void) {
    Dispatch("EffectKind18Sub2B_Run", AddressOf(EffectKind18Sub2B_States), EffectKind18Sub2B_States_count);
}

// original 0x50AFB0 (sub-state 0): the variant the spawn's x cell names: the
// cell into +0x36 / +0x3A, the lift -height - ground / 2 into +0x3E (the ground
// at the record's point, read after the cell is written), the height into
// +0x32, the texture byte << 24 into +0xC, the slide 0, +2 up. The leader
// already at the cell's front: the slide 0xC0 and +2 = 3. Then the draw on the
// ground.
extern "C" void __cdecl EffectKind18Sub2B_Place(void) {
    unsigned char* s = S();
    const U v = Variant("EffectKind18Sub2B_Place", s, at::kSub2BVariants);
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->x_whole, At(::bof3::Img(at::kSub2BCells.v) + 2 * v)[0]);
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->z_whole, At(::bof3::Img(at::kSub2BCells.v) + 2 * v + 1)[0]);
    const long h = SH_CALL(AreaMap_Elevation)(rv::As<records::EffectObject>(s)->x, rv::As<records::EffectObject>(s)->z);
    const U height = Word(At(::bof3::Img(at::kSub2BHeights.v) + 2 * v));
    const std::int32_t half = static_cast<std::int16_t>(static_cast<U>(h) & 0xFFFFu) / 2;
    s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->height_whole, (0u - height) - static_cast<U>(half));
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_32, height);
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_0c, static_cast<U>(At(::bof3::Img(at::kSub2BTextures.v) + v)[0]) << 24);
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_30, 0);
    s[2] = static_cast<unsigned char>(s[2] + 1);
    if (LeaderAt(s, false, 0x8000, 0x10000)) {
        rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_30, 0xC0);
        s[2] = 3;
    }
    SH_CALL(EffectKind18Sub2B_Draw)(1);
}

// original 0x50B0C0 (sub-state 1): the leader at the cell's front (within half
// a cell of the centre across x, on the cell or the next along z): sound 0x200
// unless a message is up, +2 up (Sprite_Current read after the sound). The draw
// on the ground.
extern "C" void __cdecl EffectKind18Sub2B_WaitNear(void) {
    unsigned char* s = S();
    if (LeaderAt(s, false, 0x8000, 0x10000)) {
        if (Field_Request == 0) {
            SH_CALL(Sound_PlayEffect)(at::kSoundOpen);
            s = S();
        }
        s[2] = static_cast<unsigned char>(s[2] + 1);
    }
    SH_CALL(EffectKind18Sub2B_Draw)(1);
}

// original 0x50B150 (sub-state 2): the slide up 0x10; at 0xC0 +2 up. The draw
// flat.
extern "C" void __cdecl EffectKind18Sub2B_Open(void) {
    if (Opened(0xC0)) S()[2] = static_cast<unsigned char>(S()[2] + 1);
    SH_CALL(EffectKind18Sub2B_Draw)(0);
}

// original 0x50B180 (sub-state 3): once the leader is two cells away (the same
// test at 0x20000 both ways fails), +2 up. Nothing drawn.
extern "C" void __cdecl EffectKind18Sub2B_WaitFar(void) {
    unsigned char* const s = S();
    if (!LeaderAt(s, false, 0x20000, 0x20000)) s[2] = static_cast<unsigned char>(s[2] + 1);
}

// original 0x50B1E0 (sub-state 4): the slide down 0x10; at 0 or below sound
// 0x201 unless a message is up and +2 = 1. The draw flat.
extern "C" void __cdecl EffectKind18Sub2B_Close(void) {
    if (Closed()) Shut(at::kSoundShut);
    SH_CALL(EffectKind18Sub2B_Draw)(0);
}

// original 0x50B220 (sub-kind 0x2B's draw; called by its five states but the
// fourth): the draw mode (page 0x95) committed at slot 6 (0xC); two textured
// quads, i = 0 and 1, one cell (0x100) wide at x (+0x36 << 7) - 0x3FC0 (the
// four corners alike), y (+0x3A << 7) - 0x3F40 / - 0x4040 plus the slide +0x30
// times the side's sign (kSub2BSides), every word the low sixteen bits. With
// `follow` the corners' heights come from the ground (Grounded); without, flat
// at +0x32 (the top 0x180 above). Projected, the texture +0xC | (0x130 - i) |
// 0x1690000, committed at slot 6 (0x48).
extern "C" void __cdecl EffectKind18Sub2B_Draw(int follow) {
    unsigned char* const v = Vertices();
    unsigned char* const mode = Gfx_PacketNext;
    const U x = (static_cast<U>(rv::As<records::EffectObject>(S())->x_whole) << 7) - 0x3FC0u;
    rv::Store<std::uint32_t>(rv::As<records::SVector>(v)[3].x, x);
    rv::Store<std::uint32_t>(rv::As<records::SVector>(v)[2].x, x);
    rv::Store<std::uint32_t>(rv::As<records::SVector>(v)[1].x, x);
    SetWord(v, x);
    PanelMode(mode);
    SH_CALL(Gfx_CommitPrim)(6, 0xC);
    U texture = 0x130;
    for (unsigned i = 0; i < 2; ++i, --texture) {
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyFT4)(p);
        SH_CALL(Gpu_SetShadeTex)(p, 0);
        unsigned char* const s = S();
        const U sign = static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(At(::bof3::Img(at::kSub2BSides.v))[i])));
        const U slide = rv::As<records::EffectWork>(s)->work_30 * sign;
        const U z = static_cast<U>(rv::As<records::EffectObject>(s)->z_whole) << 7;
        rv::Store<std::uint32_t>(rv::As<records::SVector>(v)[2].y, slide + z - 0x3F40u);
        rv::Store<std::uint32_t>(rv::As<records::SVector>(v)->y, slide + z - 0x3F40u);
        rv::Store<std::uint32_t>(rv::As<records::SVector>(v)[3].y, slide + z - 0x4040u);
        rv::Store<std::uint32_t>(rv::As<records::SVector>(v)[1].y, slide + z - 0x4040u);
        if (follow != 0) {
            Grounded();
        } else {
            rv::Store<std::uint32_t>(rv::As<records::SVector>(v)[1].z, rv::As<records::EffectWork>(s)->work_32 - 0x180u);
            rv::Store<std::uint32_t>(rv::As<records::SVector>(v)->z, rv::As<records::EffectWork>(s)->work_32 - 0x180u);
            SetWord(v + 0x1C, rv::As<records::EffectWork>(s)->work_32);
            rv::Store<std::uint32_t>(rv::As<records::SVector>(v)[2].z, rv::As<records::EffectWork>(s)->work_32);
        }
        Project(p);
        SH_CALL(Prim_SetTexture)(rv::As<records::EffectWork>(S())->work_0c | texture | 0x1690000u, p, 1);
        SH_CALL(Gfx_CommitPrim)(6, 0x48);
    }
}

// original 0x50B480 (hidden in 0x50B220): a draw mode (page 0xB5, additive)
// committed at slot 3 (0xC); a semi-transparent POLY_F4 over the frame -
// DIV-0041's fill, (Widescreen_FillX(), 0)..(320 + Widescreen_Fill(), 240), the
// original's (0, 0)..(320, 240) narrow or unarmed - coloured (0, 0x30,
// kSub36Blues[+2] << 3), committed at slot 3 (0x38); every eighth frame
// (Frame_Counter & 7, read after the commit) +2 = (+2 + 1) & 3.
extern "C" void __cdecl EffectKind18Sub36_Pulse(void) {
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0xB5, 0);
    SH_CALL(Gfx_CommitPrim)(3, 0xC);
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyF4)(p);
    SH_CALL(Gpu_SetSemiTrans)(p, 1);
    const U left = std::bit_cast<U>(Widescreen_FillX());                                 // DIV-0041: -53 wide, 0 narrow
    const U right = std::bit_cast<U>(320.0f + static_cast<float>(Widescreen_Fill()));    // 0x43A00000, 320.0f narrow
    SetUL(p + 8, left);
    SetUL(p + 0xC, 0);
    SetUL(p + 0x14, right);
    SetUL(p + 0x18, 0);
    SetUL(p + 0x20, left);
    SetUL(p + 0x24, 0x43700000u);   // 240.0f
    SetUL(p + 0x2C, right);
    SetUL(p + 0x30, 0x43700000u);
    p[4] = 0;
    p[5] = 0x30;
    const unsigned step = S()[2];
    if (step >= at::kSub36Steps)
        bof3::Fatal("EffectKind18Sub36_Pulse: +2 is %u, past the %u steps of 0x%X - the original reads on into "
                    "sub-kind 0x2C's heights (docs/effect_5g.md section 6)",
                    step, at::kSub36Steps, at::kSub36Blues.v);
    p[6] = static_cast<unsigned char>(At(::bof3::Img(at::kSub36Blues.v))[step] << 3);
    SH_CALL(Gfx_CommitPrim)(3, 0x38);
    if ((Frame_Counter & 7) == 0) {
        unsigned char* const s = S();
        s[2] = static_cast<unsigned char>((s[2] + 1) & 3);
    }
}

void EffectKind18Sub2b_Inject() {
    BOF3_INJECT(EffectKind18Sub2B_Run);
    BOF3_INJECT(EffectKind18Sub2B_Place);
    BOF3_INJECT(EffectKind18Sub2B_WaitNear);
    BOF3_INJECT(EffectKind18Sub2B_Open);
    BOF3_INJECT(EffectKind18Sub2B_WaitFar);
    BOF3_INJECT(EffectKind18Sub2B_Close);
    BOF3_INJECT(EffectKind18Sub2B_Draw);
    BOF3_INJECT(EffectKind18Sub36_Pulse);
}

// Moved here by tools/move_funcs.py from effect_6b.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_18_sub_3b.h"

#include <bit>
#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/effect_6b_callees.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_6b::at;

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

U SignedByte(unsigned char b) { return static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(b))); }

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 2]; jmp [table + eax
// * 4]: the table's `entries` handlers read in place (the fuzz swaps the cells
// for recorders); a Fatal past them, where the original jumps through the dword
// after - another table or data.
void Dispatch(const char* who, U table, unsigned entries) {
    const unsigned sub = Sprite_Current[2];
    if (sub >= entries)
        bof3::Fatal("%s: sub-state byte +2 is %u, past the %u entries of 0x%X - the original jumps through the dword "
                    "after (docs/effect_6b.md section 7)",
                    who, sub, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(At(table + 4 * sub))))();
}

// The variant the spawn's x cell (+0x36, movsx) names, checked against the
// room its table has.
U Variant(const char* who, const unsigned char* s, U table, unsigned room) {
    const std::int32_t v = S16(s + 0x36);
    if (v < 0 || v >= static_cast<std::int32_t>(room))
        bof3::Fatal("%s: the variant +0x36 is %d, past the %u its table 0x%X has room for - the original reads on "
                    "into the data after (docs/effect_6b.md section 7)",
                    who, (int)v, room, (unsigned)table);
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
// z - or, z_first, z first, then x.
bool LeaderAt(const unsigned char* s, bool z_first, std::int32_t first, std::int32_t second) {
    const std::int32_t cx = S16(s + 0x36), cz = S16(s + 0x3A);
    if (z_first) return Within(FromCentre(LeaderZ(), cz), first, FromEdge(LeaderX(), cx), second);
    return Within(FromCentre(LeaderX(), cx), first, FromEdge(LeaderZ(), cz), second);
}

// The quad's four corners projected into the POLY_FT4 at p, its depths set.
// The original also pushes a tenth pointer (a flag local) the projection does
// not read.
void Project(unsigned char* p) {
    long depth;
    SH_CALL(Gte_RotTransPers4)(Vertex(0), Vertex(1), Vertex(2), Vertex(3), reinterpret_cast<float*>(p + 8),
                               reinterpret_cast<float*>(p + 0x18), reinterpret_cast<float*>(p + 0x28),
                               reinterpret_cast<float*>(p + 0x38), &depth);
    SH_CALL(Gte_PrimDepths4_10)(p);
}

void Draw3B() { SH_CALL(EffectKind18Sub3B_Draw)(); }

using Draw = void (*)();

// The place state sub-kinds 0x33, 0x34, 0x35 and 0x3B share (0x50E420): +8 =
// whether the spawn's z cell is 0; the variant's cell into +0x36 / +0x3A (and,
// with `heights`, its lift into +0x3E); the slide +0x30 0, +2 up. The leader
// already at the cell's front (z first when +8): the slide `open` and +2 = 3.
// Then the draw.
void Place(const char* who, U cells, U heights, unsigned room, U open, Draw draw) {
    unsigned char* s = S();
    const U v = Variant(who, s, cells, room);
    s[8] = Word(s + 0x3A) == 0 ? 1 : 0;
    SetWord(S() + 0x36, At(cells + 2 * v)[0]);
    SetWord(S() + 0x3A, At(cells + 2 * v + 1)[0]);
    if (heights != 0) SetWord(S() + 0x3E, Word(At(heights + 2 * v)));
    SetWord(S() + 0x30, 0);
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
    s = S();
    if (LeaderAt(s, s[8] != 0, 0x8000, 0x10000)) {
        SetWord(s + 0x30, open);
        S()[2] = 3;
    }
    draw();
}

// The leader at the cell's front: sound 0x200 unless a message is up, +2 up
// (Sprite_Current read after the sound). The draw.
void WaitNear(Draw draw) {
    unsigned char* s = S();
    if (LeaderAt(s, s[8] != 0, 0x8000, 0x10000)) {
        if (Field_Request == 0) {
            SH_CALL(Sound_PlayEffect)(at::kSoundOpen);
            s = S();
        }
        s[2] = static_cast<unsigned char>(s[2] + 1);
    }
    draw();
}

// Once the leader is two cells away (the front test at 0x20000 both ways
// fails), +2 up. The draw.
void WaitFar(Draw draw) {
    unsigned char* const s = S();
    if (!LeaderAt(s, s[8] != 0, 0x20000, 0x20000)) s[2] = static_cast<unsigned char>(s[2] + 1);
    draw();
}

// The slide +0x30 moved by `step` (a word add); answers it as a signed word.
std::int32_t Slide(U step) {
    SetWord(S() + 0x30, Word(S() + 0x30) + step);
    return S16(S() + 0x30);
}

// Shut: the sound 0x201 unless a message is up, then +2 = 1 (read after the
// sound).
void Shut() {
    if (Field_Request == 0) SH_CALL(Sound_PlayEffect)(at::kSoundShut);
    S()[2] = 1;
}

}  // namespace

// original 0x50F230 (hidden in 0x50EED0).
extern "C" void __cdecl EffectKind18Sub3B_Run(void) {
    Dispatch("EffectKind18Sub3B_Run", AddressOf(EffectKind18Sub3B_States), EffectKind18Sub3B_States_count);
}

// original 0x50F250 (sub-state 0): Place, the variant of 0x65EC58 (room 2) and
// its lift from 0x65EC54 into +0x3E, open 0xC0.
extern "C" void __cdecl EffectKind18Sub3B_Place(void) {
    Place("EffectKind18Sub3B_Place", ::bof3::Img(at::kSub3BCells.v), ::bof3::Img(at::kSub3BHeights.v), at::kRoom2, 0xC0, &Draw3B);
}

// original 0x50F380 (sub-state 1).
extern "C" void __cdecl EffectKind18Sub3B_WaitNear(void) { WaitNear(&Draw3B); }

// original 0x50F460 (sub-state 2): the slide up 0x10; at 0xC0 +2 up. A tail
// jump to the draw.
extern "C" void __cdecl EffectKind18Sub3B_Open(void) {
    if (Slide(0x10) >= 0xC0) S()[2] = static_cast<unsigned char>(S()[2] + 1);
    Draw3B();
}

// original 0x50F480 (sub-state 3).
extern "C" void __cdecl EffectKind18Sub3B_WaitFar(void) { WaitFar(&Draw3B); }

// original 0x50F550 (sub-state 4): the slide down 0x10; at 0 or below Shut. A
// tail jump to the draw.
extern "C" void __cdecl EffectKind18Sub3B_Close(void) {
    if (Slide(0xFFF0u) <= 0) Shut();
    Draw3B();
}

// original 0x50F590 (sub-kind 0x3B's draw, called or tail-jumped to by its
// five states): with +8 the corners' z (+0x3A << 7) - 0x3FC0, else their x
// (+0x36 << 7) - 0x3FC0; corners 0 and 1 at +0x3E - 0x180, 2 and 3 at +0x3E
// (all read before any call). A draw mode (page 0x95) committed at slot 6
// (0xC); two POLY_FT4s, side 0 and 1: the other axis from the cell (<< 7) -
// 0x3F40 / - 0x4040 plus the slide +0x30 times the s8 0x65EC5C[side] (a 16-bit
// product); projected, the texture (+8 << 21) | (0x10C - side) | 0x12510000,
// committed at slot 6 (0x48).
extern "C" void __cdecl EffectKind18Sub3B_Draw(void) {
    unsigned char* const v = Vertices();
    {
        const unsigned char* const s = S();
        if (s[8] != 0) {
            const U z = (static_cast<U>(Word(s + 0x3A)) << 7) - 0x3FC0u;
            SetWord(v + 0x1A, z);
            SetWord(v + 0x12, z);
            SetWord(v + 0xA, z);
            SetWord(v + 2, z);
        } else {
            const U x = (static_cast<U>(Word(s + 0x36)) << 7) - 0x3FC0u;
            SetWord(v + 0x18, x);
            SetWord(v + 0x10, x);
            SetWord(v + 8, x);
            SetWord(v, x);
        }
        SetWord(v + 0xC, Word(s + 0x3E) - 0x180u);
        SetWord(v + 4, Word(s + 0x3E) - 0x180u);
        SetWord(v + 0x1C, Word(s + 0x3E));
        SetWord(v + 0x14, Word(s + 0x3E));
    }
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x95, 0);
    SH_CALL(Gfx_CommitPrim)(6, 0xC);
    U texture = 0x10C;
    for (unsigned side = 0; side < 2; ++side, --texture) {
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyFT4)(p);
        SH_CALL(Gpu_SetShadeTex)(p, 0);
        const unsigned char* const s = S();
        const U sign = SignedByte(At(::bof3::Img(at::kSub3BSides.v))[side]);
        if (s[8] != 0) {
            const U x0 = Word(s + 0x30) * sign + (static_cast<U>(Word(s + 0x36)) << 7) - 0x3F40u;
            SetWord(v + 0x10, x0);
            SetWord(v, x0);
            const U x1 = Word(s + 0x30) * sign + (static_cast<U>(Word(s + 0x36)) << 7) - 0x4040u;
            SetWord(v + 0x18, x1);
            SetWord(v + 8, x1);
        } else {
            const U z0 = Word(s + 0x30) * sign + (static_cast<U>(Word(s + 0x3A)) << 7) - 0x3F40u;
            SetWord(v + 0x12, z0);
            SetWord(v + 2, z0);
            const U z1 = Word(s + 0x30) * sign + (static_cast<U>(Word(s + 0x3A)) << 7) - 0x4040u;
            SetWord(v + 0x1A, z1);
            SetWord(v + 0xA, z1);
        }
        Project(p);
        SH_CALL(Prim_SetTexture)((static_cast<U>(S()[8]) << 21) | texture | 0x12510000u, p, 1);
        SH_CALL(Gfx_CommitPrim)(6, 0x48);
    }
}

// original 0x50F780 (hidden in 0x50F590): a draw mode (page 0xB5, additive)
// committed at slot 3 (0xC); a semi-transparent POLY_F4 over the frame -
// DIV-0041's fill (0x50F7B5 is the original's mov ecx, 320.0f),
// (Widescreen_FillX(), 0)..(320 + Widescreen_Fill(), 240), the original's (0,
// 0)..(320, 240) narrow or unarmed - coloured (0, 0x30, 0x65EC60[+2] << 3),
// committed at slot 3 (0x38); every eighth frame (Frame_Counter & 7, read after
// the commit) +2 = (+2 + 1) & 3. E5G's EffectKind18Sub36_Pulse with its own
// table.
extern "C" void __cdecl EffectKind18Sub54_Pulse(void) {
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
    if (step >= at::kSub54Steps)
        bof3::Fatal("EffectKind18Sub54_Pulse: +2 is %u, past the %u steps of 0x%X - the original reads on into "
                    "EffectKind18Sub40_States (docs/effect_6b.md section 7)",
                    step, at::kSub54Steps, at::kSub54Blues.v);
    p[6] = static_cast<unsigned char>(At(::bof3::Img(at::kSub54Blues.v))[step] << 3);
    SH_CALL(Gfx_CommitPrim)(3, 0x38);
    if ((Frame_Counter & 7) == 0) {
        unsigned char* const s = S();
        s[2] = static_cast<unsigned char>((s[2] + 1) & 3);
    }
}

// original 0x50F820 (hidden in 0x50F590): nothing while Cond_ByteFE is 0.
// Otherwise 35 rows from the leader's cell row (the high word of Field_Kind2Z)
// - 0x14, k = 0, 3, .. 0x66 the row's phase: a = (sin(((F + k) << 7) - 0x80) >>
// 10) .. (sin((F + k) << 7) >> 10) subtracted the other way, b = (sin((F + k +
// 3) << 7) >> 10) - (sin((F + k + 2) << 7) >> 10), each angle & 0xFFF and F
// Frame_Counter read before each Math_Sin; then 35 columns from the leader's
// cell column - 0x14: every cell inside the map (AreaMap_Header's width and
// height bytes, signed tests) has its AreaMap_Corners bytes 0 and 1 raised by
// a, 2 and 3 by b (byte adds). Then the leader's height word ObjTrio +0x3E from
// AreaMap_Elevation at its point, and MapView_Redraw = 2. Sprite_Current is
// not read.
extern "C" void __cdecl EffectKind18Sub3D_Ripple(void) {
    if (Cond_ByteFE == 0) return;
    const std::int32_t column = S16(At(::bof3::Img(at::kLeaderCellX.v))) - 0x14;
    std::int32_t row = S16(At(::bof3::Img(at::kLeaderCellZ.v))) - 0x14;
    unsigned char* const corners = reinterpret_cast<unsigned char*>(&AreaMap_Corners);
    for (U k = 0; k < 0x69; k += 3, ++row) {
        const std::int32_t s0 = SH_CALL(Math_Sin)(static_cast<int>((((Frame_Counter + k) << 7) - 0x80u) & 0xFFFu)) >> 10;
        const std::int32_t s1 = SH_CALL(Math_Sin)(static_cast<int>(((Frame_Counter + k) << 7) & 0xFFFu)) >> 10;
        const unsigned char a = static_cast<unsigned char>(s1 - s0);
        const std::int32_t s2 = SH_CALL(Math_Sin)(static_cast<int>(((Frame_Counter + k + 3) << 7) & 0xFFFu)) >> 10;
        const std::int32_t s3 = SH_CALL(Math_Sin)(static_cast<int>(((Frame_Counter + k + 2) << 7) & 0xFFFu)) >> 10;
        const unsigned char b = static_cast<unsigned char>(s2 - s3);
        for (std::int32_t i = 0; i < 0x23; ++i) {
            const std::int32_t width = AreaMap_Header[0], height = AreaMap_Header[1];
            if (row < 0 || row >= height) continue;
            const std::int32_t col = i + column;
            if (col < 0 || col >= width) continue;
            unsigned char* const c = corners + 4 * static_cast<U>(row * width + col);
            c[0] = static_cast<unsigned char>(c[0] + a);
            c[1] = static_cast<unsigned char>(c[1] + a);
            c[2] = static_cast<unsigned char>(c[2] + b);
            c[3] = static_cast<unsigned char>(c[3] + b);
        }
    }
    const long h = SH_CALL(AreaMap_Elevation)(ObjTrio[0].x, ObjTrio[0].z);
    ObjTrio[0].height_whole = static_cast<U>(h);
    MapView_Redraw = 2;
}

void EffectKind18Sub3b_Inject() {
    BOF3_INJECT(EffectKind18Sub3B_Run);
    BOF3_INJECT(EffectKind18Sub3B_Place);
    BOF3_INJECT(EffectKind18Sub3B_WaitNear);
    BOF3_INJECT(EffectKind18Sub3B_Open);
    BOF3_INJECT(EffectKind18Sub3B_WaitFar);
    BOF3_INJECT(EffectKind18Sub3B_Close);
    BOF3_INJECT(EffectKind18Sub3B_Draw);
    BOF3_INJECT(EffectKind18Sub54_Pulse);
    BOF3_INJECT(EffectKind18Sub3D_Ripple);
}

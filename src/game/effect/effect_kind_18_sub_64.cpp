// Moved here by tools/move_funcs.py from effect_6d.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_18_sub_64.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_6d_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_6d::at;

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

std::int32_t S16(const unsigned char* p) { return static_cast<std::int16_t>(Word(p)); }

std::int32_t S16(U a) { return S16(At(a)); }

std::int32_t I(U v) { return static_cast<std::int32_t>(v); }

U AddressOf(const void* p) { return Low32(p); }

unsigned char* Vertices() { return reinterpret_cast<unsigned char*>(Prim_VertexScratch); }

const short* Vertex(unsigned i) { return reinterpret_cast<const short*>(Vertices() + 8 * i); }

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 2]; jmp [table + eax
// * 4]: the table's `entries` handlers read in place (the fuzz swaps the cells
// for recorders); a Fatal past them, where the original jumps through the dword
// after - another table or data.
void DispatchEffect6d(const char* who, U table, unsigned entries) {
    const unsigned sub = Sprite_Current[2];
    if (sub >= entries)
        bof3::Fatal("%s: sub-state byte +2 is %u, past the %u entries of 0x%X - the original jumps through the dword "
                    "after (docs/effect_6d.md section 7)",
                    who, sub, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(table + 4 * sub)))();
}

// The variant the spawn's x cell (+0x36, movsx) names, checked against the
// room its tables have.
U Variant(const char* who, const unsigned char* s, unsigned room) {
    const std::int32_t v = S16(s + 0x36);
    if (v < 0 || v >= static_cast<std::int32_t>(room))
        bof3::Fatal("%s: the variant +0x36 is %d, past the %u its tables have room for - the original reads on into "
                    "the next table (docs/effect_6d.md section 7)",
                    who, (int)v, room);
    return static_cast<U>(v);
}

// cdq; xor eax, edx; sub eax, edx: |d| as the original computes it -
// 0x80000000 stays itself, negative.
std::int32_t Abs(U d) {
    const U m = I(d) < 0 ? 0xFFFFFFFFu : 0u;
    return I((d ^ m) - m);
}

// movsx eax, ax; cdq; sub eax, edx; sar 1: half a height, toward zero.
U Half(long height) { return static_cast<U>(static_cast<std::int16_t>(static_cast<U>(height) & 0xFFFFu) / 2); }

// The quad's four corners projected into the POLY_FT4 / G4 at p, its depths
// set. The original also pushes a tenth pointer (a flag local) the projection
// does not read.
void Project(unsigned char* p) {
    long depth;
    SH_CALL(Gte_RotTransPers4)(Vertex(0), Vertex(1), Vertex(2), Vertex(3), reinterpret_cast<float*>(p + 8),
                               reinterpret_cast<float*>(p + 0x18), reinterpret_cast<float*>(p + 0x28),
                               reinterpret_cast<float*>(p + 0x38), &depth);
    SH_CALL(Gte_PrimDepths4_10)(p);
}

// The draw mode the panels put first: Gpu_SetDrawMode(prim, 0, 0, 0x95, 0).
void PanelMode() { SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x95, 0); }

void Link(int dy, unsigned size) {
    unsigned char* const s = S();
    auto* const so = rv::As<records::EffectObject>(s);
    SH_CALL(MapView_LinkPrimAt)(static_cast<std::uint32_t>(so->x), static_cast<std::uint32_t>(so->z), dy, size);
}

void StepUp() { S()[2] = static_cast<unsigned char>(S()[2] + 1); }

// The enemy record +0xC holds (Place stores it): its bytes +1 and +2, read
// again for every test.
const unsigned char* Watched() { return reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(rv::As<records::EffectWork>(S())->work_0c)); }

bool WatchedIs(unsigned char a, unsigned char b) {
    const unsigned char* const r = Watched();
    return r[1] == a && r[2] == b;
}

void Sub64Plain() { SH_CALL(EffectKind18Sub64_Draw)(0x80, 0x40, 1, 0); }

}  // namespace

// original 0x516480 (hidden in 0x515CC0): jmp [EffectKind18Sub64_States + +2 *
// 4], unbounded.
extern "C" void __cdecl EffectKind18Sub64_Run(void) {
    DispatchEffect6d("EffectKind18Sub64_Run", AddressOf(EffectKind18Sub64_States), EffectKind18Sub64_States_count);
}

// original 0x5164A0 (sub-state 0): the variant v the spawn's x cell names: its
// cell (0x65F578) << 16 into +0x34 / +0x38, +0xC the enemy record v + 1
// (0x93BA88 + 0x128 v), +9 and +0xA 0, +2 up. The draw (0x80, 0x40, 1, 0).
extern "C" void __cdecl EffectKind18Sub64_Place(void) {
    unsigned char* const s = S();
    auto* const so = rv::As<records::EffectObject>(s);
    const U v = Variant("EffectKind18Sub64_Place", s, at::kSub64Variants);
    so->x = static_cast<U>(At(::bof3::Img(at::kSub64Cells.v) + 2 * v)[0]) << 16;
    so->z = static_cast<U>(At(::bof3::Img(at::kSub64Cells.v) + 2 * v + 1)[0]) << 16;
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_0c, ::bof3::Img(Battle_Enemies_addr) + 0x128 + at::kEnemyStride * v);
    s[9] = 0;
    s[0xA] = 0;
    s[2] = static_cast<unsigned char>(s[2] + 1);
    Sub64Plain();
}

// original 0x516520 (sub-state 1): Game_Mode and Game_Step both 5: +2 up. The
// draw.
extern "C" void __cdecl EffectKind18Sub64_WaitBattle(void) {
    if (Game_Mode == 5 && Game_Step == 5) StepUp();
    Sub64Plain();
}

// original 0x516560 (sub-state 2): Game_Mode 5 and Game_Step not: +2 down. Then
// by the watched record's bytes +1 / +2: (8, 2) +2 = 4; (6, 1) and (6, 2) 6;
// (6, 3) 5; (6, 4) 3 - each test after the last's write. (3, 1): the draw at
// scale ((0x904AC8 * 6 + 0x3F) & 0xF8); else the plain draw.
extern "C" void __cdecl EffectKind18Sub64_Watch(void) {
    if (Game_Mode == 5 && Game_Step != 5) S()[2] = static_cast<unsigned char>(S()[2] - 1);
    if (WatchedIs(8, 2)) S()[2] = 4;
    if (WatchedIs(6, 1)) S()[2] = 6;
    if (WatchedIs(6, 2)) S()[2] = 6;
    if (WatchedIs(6, 3)) S()[2] = 5;
    if (WatchedIs(6, 4)) S()[2] = 3;
    if (WatchedIs(3, 1)) {
        const U scale = (UL(::bof3::Img(at::kBattleWord.v)) * 6 + 0x3Fu) & 0xF8u;
        SH_CALL(EffectKind18Sub64_Draw)(scale, 0x40, 1, 0);
        return;
    }
    Sub64Plain();
}

// original 0x516640 (sub-state 3): +9 up 2; the draw at (2a, a, 1, 0), a = 0x40 -
// +9; past 0x3F a tail jump to Effect_Release.
extern "C" void __cdecl EffectKind18Sub64_Fade(void) {
    S()[9] = static_cast<unsigned char>(S()[9] + 2);
    const U a = 0x40u - S()[9];
    SH_CALL(EffectKind18Sub64_Draw)(a + a, I(a), 1, 0);
    if (S()[9] > 0x3F) SH_CALL(Effect_Release)();
}

// original 0x516690 (sub-state 4): unless the watched record is (8, 2), +2 = 2.
// The draw at (0xE0, 0x70, 2, 0).
extern "C" void __cdecl EffectKind18Sub64_Hold(void) {
    unsigned char* const s = S();
    if (!WatchedIs(8, 2)) s[2] = 2;
    SH_CALL(EffectKind18Sub64_Draw)(0xE0, 0x70, 2, 0);
}

// original 0x5166C0 (sub-state 5, and sub-state 6's tail jump; no list held it):
// +9 up; e = 8 - |8 - +9|; past 0xF +9 0 and +2 = 2. The draw lifted by -e <<
// 4.
extern "C" void __cdecl EffectKind18Sub64_Pulse(void) {
    S()[9] = static_cast<unsigned char>(S()[9] + 1);
    unsigned char* const s = S();
    const U c = s[9];
    const std::int32_t e = 8 - Abs(8u - c);
    if (c > 0xF) {
        s[9] = 0;
        S()[2] = 2;
    }
    SH_CALL(EffectKind18Sub64_Draw)(0x80, 0x40, 1, I(static_cast<U>(-e) << 4));
}

// original 0x516730 (sub-state 6): the watched record (6, 3): +9 0, +2 = 5 and a
// tail jump to the pulse. Else +9 up 8; the draw at (|0x40 - +9| + 0x40, |0x20 -
// +9 / 2| + 0x20, 1, 0), and past 0x7F +9 0 and +2 = 2 first.
extern "C" void __cdecl EffectKind18Sub64_Swell(void) {
    if (WatchedIs(6, 3)) {
        S()[9] = 0;
        S()[2] = 5;
        SH_CALL(EffectKind18Sub64_Pulse)();
        return;
    }
    S()[9] = static_cast<unsigned char>(S()[9] + 8);
    unsigned char* const s = S();
    const U b = s[9];
    const std::int32_t scale = Abs(0x40u - b) + 0x40;
    const std::int32_t width = Abs(0x20u - (b >> 1)) + 0x20;
    if (b > 0x7F) {
        s[9] = 0;
        S()[2] = 2;
    }
    SH_CALL(EffectKind18Sub64_Draw)(static_cast<U>(scale), width, 1, 0);
}

// original 0x5167C0 (cdecl (scale, width, step, lift); every state's draw): +0xA
// up by the step byte, less 12 past 11; frames f0 = +0xA / 2 and f1 = f0 + 2
// (f0 - 4 past 5), each + ((scale << 16) | 0xBF009111) a texture; two crossed
// textured quads at the record's cell ((+0x36 << 7) - 0x3FF0, (+0x3A << 7) -
// 0x3FF0): i 0 across z, i 1 across x, width either side, the top lift above
// the near edge; the top corners -(2 width + 0xC0) - ground / 2, the bottom
// -0xC0 - ground / 2 (the ground at the record's point, twice). Each after a
// draw mode (page 0x95) linked at the point with dy 1 (0xC); projected, the
// frame's texture, linked (0x48).
extern "C" void __cdecl EffectKind18Sub64_Draw(unsigned scale, int width, unsigned step, int lift) {
    {
        unsigned char* const s = S();
        s[0xA] = static_cast<unsigned char>(s[0xA] + static_cast<unsigned char>(step));
    }
    {
        unsigned char* const s = S();
        U f = s[0xA];
        if (f > 0xB) f -= 0xC;
        s[0xA] = static_cast<unsigned char>(f);
    }
    unsigned char* const s = S();
    const U f0 = static_cast<U>(s[0xA]) >> 1;
    const U f1 = f0 + 2 > 5 ? f0 - 4 : f0 + 2;
    const U base = (scale << 16) | 0xBF009111u;
    const U textures[2] = {f0 + base, f1 + base};
    const U x = (static_cast<U>(S16(s + 0x36)) << 7) - 0x3FF0u;
    const U z = (static_cast<U>(S16(s + 0x3A)) << 7) - 0x3FF0u;
    const U w = static_cast<U>(width), l = static_cast<U>(lift);
    unsigned char* const v = Vertices();
    for (U i = 0; i < 2; ++i) {
        PanelMode();
        Link(1, 0xC);
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyFT4)(p);
        SH_CALL(Gpu_SetShadeTex)(p, 0);
        const U across = w * i;
        rv::Store<std::uint32_t>(rv::As<records::SVector>(v)[2].x, across + x);
        SetWord(v, across + x);
        rv::Store<std::uint32_t>(rv::As<records::SVector>(v)[3].x, x - across);
        rv::Store<std::uint32_t>(rv::As<records::SVector>(v)[1].x, x - across);
        const U along = i == 0 ? w : 0u;
        rv::Store<std::uint32_t>(rv::As<records::SVector>(v)->y, along - l + z);
        rv::Store<std::uint32_t>(rv::As<records::SVector>(v)[2].y, along + z);
        rv::Store<std::uint32_t>(rv::As<records::SVector>(v)[3].y, z - along);
        rv::Store<std::uint32_t>(rv::As<records::SVector>(v)[1].y, z - along - l);
        {
            unsigned char* const r = S();
            const long h = SH_CALL(AreaMap_Elevation)(rv::As<records::EffectObject>(r)->x, rv::As<records::EffectObject>(r)->z);
            const U top = (0u - (2 * w + 0xC0u)) - Half(h);
            rv::Store<std::uint32_t>(rv::As<records::SVector>(v)[1].z, top);
            rv::Store<std::uint32_t>(rv::As<records::SVector>(v)->z, top);
        }
        {
            unsigned char* const r = S();
            const long h = SH_CALL(AreaMap_Elevation)(rv::As<records::EffectObject>(r)->x, rv::As<records::EffectObject>(r)->z);
            const U bottom = 0xFFFFFF40u - Half(h);
            rv::Store<std::uint32_t>(rv::As<records::SVector>(v)[3].z, bottom);
            rv::Store<std::uint32_t>(rv::As<records::SVector>(v)[2].z, bottom);
        }
        Project(p);
        SH_CALL(Prim_SetTexture)(textures[i], p, 1);
        Link(1, 0x48);
    }
}

void EffectKind18Sub64_Inject() {
    BOF3_INJECT(EffectKind18Sub64_Run);
    BOF3_INJECT(EffectKind18Sub64_Place);
    BOF3_INJECT(EffectKind18Sub64_WaitBattle);
    BOF3_INJECT(EffectKind18Sub64_Watch);
    BOF3_INJECT(EffectKind18Sub64_Fade);
    BOF3_INJECT(EffectKind18Sub64_Hold);
    BOF3_INJECT(EffectKind18Sub64_Pulse);
    BOF3_INJECT(EffectKind18Sub64_Swell);
    BOF3_INJECT(EffectKind18Sub64_Draw);
}

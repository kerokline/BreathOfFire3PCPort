// Moved here by tools/move_funcs.py from effect_6d.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_18_sub_5d.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_6d.h"
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

U S8U(unsigned char b) { return static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(b))); }

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

// A point against a cell's centre on one axis: point - ((cell + 1) << 16 |
// 0x8000) (inc; shl 16; or dh, 0x80).
U FromCentre(U point, std::int32_t cell) { return point - ((static_cast<U>(cell + 1) << 16) | 0x8000u); }

// ... and against its edge: point - (cell << 16).
U FromEdge(U point, std::int32_t cell) { return point - (static_cast<U>(cell) << 16); }

// |a| within `first`, then |b| or |b - 0x10000| within `second` (the cell or
// the one past it).
bool Within(U a, std::int32_t first, U b, std::int32_t second) {
    if (Abs(a) > first) return false;
    if (Abs(b) <= second) return true;
    return Abs(b - 0x10000u) <= second;
}

U LeaderX() { return static_cast<std::uint32_t>(ObjTrio[0].x); }

U LeaderZ() { return static_cast<std::uint32_t>(ObjTrio[0].z); }

// The leader at the record's cell, z across the centre first (sub-kind 0x5E,
// and 0x5D with +8 set), or x first (0x5D without).
bool LeaderAt(const unsigned char* s, bool z_first, std::int32_t first, std::int32_t second) {
    const std::int32_t cx = S16(s + 0x36), cz = S16(s + 0x3A);
    if (z_first) return Within(FromCentre(LeaderZ(), cz), first, FromEdge(LeaderX(), cx), second);
    return Within(FromCentre(LeaderX(), cx), first, FromEdge(LeaderZ(), cz), second);
}

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

// A slide of +0x30 by `by`; answers +0x30 (signed, read again).
std::int32_t Slide(U by) {
    unsigned char* const s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_30, rv::As<records::EffectWork>(s)->work_30 + by);
    return S16(S() + 0x30);
}

// Closed: the sound unless a message is up (Field_Request), +2 = 1 (read after
// the sound).
void Shut() {
    if (Field_Request == 0) SH_CALL(Sound_PlayEffect)(static_cast<unsigned short>(at::kSoundShut));
    S()[2] = 1;
}

void StepUp() { S()[2] = static_cast<unsigned char>(S()[2] + 1); }

}  // namespace

// original 0x514A70 (hidden in 0x514880): jmp [EffectKind18Sub5D_States + +2 *
// 4], unbounded.
extern "C" void __cdecl EffectKind18Sub5D_Run(void) {
    DispatchEffect6d("EffectKind18Sub5D_Run", AddressOf(EffectKind18Sub5D_States), EffectKind18Sub5D_States_count);
}

// original 0x514A90 (sub-state 0): +8 = whether the spawn's z cell is 0; the
// variant the x cell names: the cell (0x65F448), +0x3E (0x65F438), +0x20
// (0x65F440), +0x2E (0x65F43C); +0x30 0, +2 up; the leader already at the
// cell (the axis +8 names first): +0x30 0x80, +2 = 3. The draw.
extern "C" void __cdecl EffectKind18Sub5D_Place(void) {
    unsigned char* const s = S();
    s[8] = rv::As<records::EffectObject>(s)->z_whole == 0 ? 1 : 0;
    const U v = Variant("EffectKind18Sub5D_Place", s, at::kSub5DVariants);
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->x_whole, At(::bof3::Img(at::kSub5DCells.v) + 2 * v)[0]);
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->z_whole, At(::bof3::Img(at::kSub5DCells.v) + 2 * v + 1)[0]);
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->height_whole, Word(At(::bof3::Img(at::kSub5DLifts.v) + 2 * v)));
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->work_20, UL(::bof3::Img(at::kSub5DTextures.v) + 4 * v));
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_2e, Word(At(::bof3::Img(at::kSub5DHeights.v) + 2 * v)));
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_30, 0);
    s[2] = static_cast<unsigned char>(s[2] + 1);
    if (LeaderAt(s, s[8] != 0, 0x8000, 0x10000)) {
        rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_30, 0x80);
        s[2] = 3;
    }
    SH_CALL(EffectKind18Sub5D_Draw)();
}

// original 0x514BE0 (sub-state 1): the leader at the cell (the axis +8 names
// first): sound 0x200 unless a message is up, +2 up (read after the sound).
// The draw.
extern "C" void __cdecl EffectKind18Sub5D_WaitNear(void) {
    unsigned char* s = S();
    if (LeaderAt(s, s[8] != 0, 0x8000, 0x10000)) {
        if (Field_Request == 0) {
            SH_CALL(Sound_PlayEffect)(static_cast<unsigned short>(at::kSoundOpen));
            s = S();
        }
        s[2] = static_cast<unsigned char>(s[2] + 1);
    }
    SH_CALL(EffectKind18Sub5D_Draw)();
}

// original 0x514CC0 (sub-state 2): the slide up 0x10; at 0x80 +2 up. A tail jump
// to the draw.
extern "C" void __cdecl EffectKind18Sub5D_Open(void) {
    if (Slide(0x10) >= 0x80) StepUp();
    SH_CALL(EffectKind18Sub5D_Draw)();
}

// original 0x514CE0 (sub-state 3): the leader two cells off: +2 up. The draw.
extern "C" void __cdecl EffectKind18Sub5D_WaitFar(void) {
    unsigned char* const s = S();
    if (!LeaderAt(s, s[8] != 0, 0x20000, 0x20000)) s[2] = static_cast<unsigned char>(s[2] + 1);
    SH_CALL(EffectKind18Sub5D_Draw)();
}

// original 0x514DB0 (sub-state 4): the slide down 0x10; at 0 or below sound
// 0x201 unless a message is up and +2 = 1. A tail jump to the draw.
extern "C" void __cdecl EffectKind18Sub5D_Close(void) {
    if (Slide(0xFFF0u) <= 0) Shut();
    SH_CALL(EffectKind18Sub5D_Draw)();
}

// original 0x514DF0 (sub-kind 0x5D's draw: called by its states 0, 1, 3, tail-
// jumped to by 2 and 4): with +8 the corners' y (+0x3A << 7) - 0x3FC0 and the
// panels slide along x, else x (+0x36 << 7) - 0x3FC0 and they slide along z;
// the top corners at +0x3E - +0x2E, the bottom ones at +0x3E. Two sides: a
// draw mode (page 0x95) linked at the record's point with dy -1 (0xC), the
// corners slid by +0x30 times the side's sign (0x65F44C), projected, the
// texture (+0x20 + side) | (+8 << 21), linked likewise (0x48).
extern "C" void __cdecl EffectKind18Sub5D_Draw(void) {
    unsigned char* const v = Vertices();
    {
        unsigned char* const s = S();
        if (s[8] != 0) {
            const U y = (static_cast<U>(rv::As<records::EffectObject>(s)->z_whole) << 7) - 0x3FC0u;
            SetWord(v + 0x1A, y);
            SetWord(v + 0x12, y);
            SetWord(v + 0xA, y);
            SetWord(v + 2, y);
        } else {
            const U x = (static_cast<U>(rv::As<records::EffectObject>(s)->x_whole) << 7) - 0x3FC0u;
            SetWord(v + 0x18, x);
            SetWord(v + 0x10, x);
            SetWord(v + 8, x);
            SetWord(v, x);
        }
        const U top = rv::As<records::EffectObject>(s)->height_whole - rv::As<records::EffectWork>(s)->work_2e;
        rv::Store<std::uint32_t>(rv::As<records::SVector>(v)[1].z, top);
        rv::Store<std::uint32_t>(rv::As<records::SVector>(v)->z, top);
        SetWord(v + 0x1C, rv::As<records::EffectObject>(s)->height_whole);
        rv::Store<std::uint32_t>(rv::As<records::SVector>(v)[2].z, rv::As<records::EffectObject>(s)->height_whole);
    }
    for (U side = 0; side < 2; ++side) {
        PanelMode();
        Link(-1, 0xC);
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyFT4)(p);
        SH_CALL(Gpu_SetShadeTex)(p, 0);
        unsigned char* const s = S();
        const U sign = S8U(At(::bof3::Img(at::kSub5DSides.v))[side]);
        const U slide = rv::As<records::EffectWork>(s)->work_30 * sign;
        if (s[8] != 0) {
            const U x = (side + rv::As<records::EffectObject>(s)->x_whole) << 7;
            SetWord(v + 0x10, x - slide - 0x4040u);
            SetWord(v, x - slide - 0x4040u);
            SetWord(v + 0x18, x - slide - 0x3FC0u);
            SetWord(v + 8, x - slide - 0x3FC0u);
        } else {
            const U y = (rv::As<records::EffectObject>(s)->z_whole - side) << 7;
            SetWord(v + 0x12, y + slide - 0x3F40u);
            SetWord(v + 2, y + slide - 0x3F40u);
            SetWord(v + 0x1A, y + slide - 0x3FC0u);
            SetWord(v + 0xA, y + slide - 0x3FC0u);
        }
        Project(p);
        {
            unsigned char* const r = S();
            SH_CALL(Prim_SetTexture)((static_cast<std::uint32_t>(rv::As<records::EffectObject>(r)->work_20) + side) | (static_cast<U>(r[8]) << 21), p, 1);
        }
        Link(-1, 0x48);
    }
}

void EffectKind18Sub5d_Inject() {
    BOF3_INJECT(EffectKind18Sub5D_Run);
    BOF3_INJECT(EffectKind18Sub5D_Place);
    BOF3_INJECT(EffectKind18Sub5D_WaitNear);
    BOF3_INJECT(EffectKind18Sub5D_Open);
    BOF3_INJECT(EffectKind18Sub5D_WaitFar);
    BOF3_INJECT(EffectKind18Sub5D_Close);
    BOF3_INJECT(EffectKind18Sub5D_Draw);
}

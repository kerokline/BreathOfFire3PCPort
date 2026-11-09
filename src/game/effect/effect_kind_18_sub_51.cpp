// Moved here by tools/move_funcs.py from effect_6c.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_18_sub_51.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/draw_pool.h"
#include "game/effect_6c.h"
#include "game/effect_6c_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_6c::at;

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

U ULEffect6c(const unsigned char* p) { return static_cast<U>(Long(p)); }

U ULEffect6c(U a) { return ULEffect6c(At(a)); }

void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }

void SetUL(U a, U v) { SetUL(At(a), v); }

std::int32_t S16(const unsigned char* p) { return static_cast<std::int16_t>(Word(p)); }

std::int32_t S16(U a) { return S16(At(a)); }

std::int32_t S8(U a) { return static_cast<signed char>(*At(a)); }

std::int32_t I(U v) { return static_cast<std::int32_t>(v); }

U AddressOf(const void* p) { return Low32(p); }

// sar: an arithmetic right shift of the 32 bits as the original holds them.
U Sar(U v, unsigned n) { return static_cast<U>(static_cast<std::int32_t>(v) >> n); }

// cdq; and edx, 2^n - 1; add eax, edx; sar eax, n: a signed divide by 2^n,
// toward zero.
U DivP2(U v, unsigned n) { return Sar(v + (I(v) < 0 ? (1u << n) - 1u : 0u), n); }

// cdq; xor eax, edx; sub eax, edx.
U Abs(U v) { return I(v) < 0 ? 0u - v : v; }

float F(const unsigned char* p) {
    float f;
    std::memcpy(&f, p, sizeof f);
    return f;
}

float F(U a) { return F(At(a)); }

unsigned char* Vertices() { return reinterpret_cast<unsigned char*>(Prim_VertexScratch); }

void SetV(unsigned offset, U v) { SetWord(Vertices() + offset, v); }

const short* Vx(unsigned i) { return reinterpret_cast<const short*>(Vertices() + 8 * i); }

float* Fp(unsigned char* p) { return reinterpret_cast<float*>(p); }

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 2]; jmp [table + eax
// * 4]: the table's `entries` handlers read in place (the fuzz swaps the cells
// for recorders); a Fatal past them, where the original jumps through the dword
// after - another table or data.
void Dispatch(const char* who, U table, unsigned entries) {
    const unsigned sub = Sprite_Current[2];
    if (sub >= entries)
        bof3::Fatal("%s: sub-state byte +2 is %u, past the %u entries of 0x%X - the original jumps through the dword "
                    "after (docs/effect_6c.md section 7)",
                    who, sub, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(ULEffect6c(At(table + 4 * sub))))();
}

#define E6C_TABLE(name) AddressOf(name), name##_count

// A draw item by index: the pool's (DIV-0062), a Fatal past its count where the
// original reads past the array.
unsigned char* Item(const char* who, U item) {
    if (item >= draw_pool::Count())
        bof3::Fatal("%s: draw item %u, past the pool's %u - the original reads past DrawItems (docs/effect_6c.md "
                    "section 7)",
                    who, (unsigned)item, draw_pool::Count());
    return draw_pool::Items() + item * at::kDrawItemStride;
}

// The dword record of the map cell (x, z) at `run` records into its run, as
// the original indexes it: the cell's word at AreaMap_Header + (x + width * z
// + 2 * base) * 2, plus base, plus (height * width + 1) / 2 - every byte and
// word re-read where the original re-reads them.
unsigned char* CellRecord(U x, U z, U run) {
    const U header = ULEffect6c(::bof3::Img(at::kAreaHeader.v));
    const U width = header & 0xFF;
    const U base = Word(At(::bof3::Img(AreaMap_Header_addr) + 0x2));
    const U word = Word(At(::bof3::Img(AreaMap_Header_addr) + 2 * (x + width * z + 2 * base)));
    const U height = (header >> 8) & 0xFF;
    const U half = (height * width + 1) / 2;   // cdq; sub eax, edx; sar 1 - never negative here
    return At(::bof3::Img(AreaMap_Header_addr) + 4 * (base + word + half + run));
}

// The variant +0x36 (s16) of the record now current, checked against the five
// rectangles of kRect51.
U Variant51(const char* who) {
    const std::int32_t v = S16(S() + 0x36);
    if (v < 0 || v >= static_cast<std::int32_t>(at::kRect51Variants))
        bof3::Fatal("%s: the variant +0x36 is %d, past the %u rectangles of 0x%X - the original reads on into "
                    "EffectKind18Sub51_States (docs/effect_6c.md section 7)",
                    who, (int)v, at::kRect51Variants, at::kRect51.v);
    return static_cast<U>(v);
}

U Rect51(const char* who, unsigned field) { return At(::bof3::Img(at::kRect51.v) + 4 * Variant51(who) + field)[0]; }

// Each cell of the variant's rectangle, z rows outer, x inner, every bound read
// again from the record current at that point (as the original re-reads +0x36):
// `cell(x, z)`.
template <typename F> void ForCells(const char* who, F cell) {
    for (U z = Rect51(who, 1); I(z) <= I(Rect51(who, 3)); ++z)
        for (U x = Rect51(who, 0); I(x) <= I(Rect51(who, 2)); ++x) cell(x, z);
}

}  // namespace

// original 0x512350 (EffectKind18_States[0x51], hidden in 0x512040): jmp
// [EffectKind18Sub51_States + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub51_Run(void) {
    Dispatch("EffectKind18Sub51_Run", E6C_TABLE(EffectKind18Sub51_States));
}

// original 0x512370 (sub-state 0): +9 0; each cell of the variant's rectangle:
// its first record's bits 19..23 cleared and, where MapView_ItemAt answers a
// draw item, Prim_SetTexture(the record, the item, 2) - DIV-0062's site
// 0x51242A; +2 up.
extern "C" void __cdecl EffectKind18Sub51_Place(void) {
    S()[9] = 0;
    ForCells("EffectKind18Sub51_Place", [](U x, U z) {
        unsigned char* const record = CellRecord(x, z, 0);
        SetUL(record, ULEffect6c(record) & 0xFF07FFFFu);
        const U item = static_cast<U>(SH_CALL(MapView_ItemAt)(I(x), I(z)));
        if (item != 0) SH_CALL(Prim_SetTexture)(ULEffect6c(record), Item("EffectKind18Sub51_Place", item), 2);
    });
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
}

// original 0x512490 (sub-state 1; its switch on the variant +0x36, 0..4, has
// five cases, the fourth the cut's start 0x5124C0): +2 up once - variant 0
// flag 0x14 of 0x904000, 1 Cond_ByteFE 1, 2..4 story flags 0x7A..0x7C - is set.
// A variant past 4 does nothing.
extern "C" void __cdecl EffectKind18Sub51_Wait(void) {
    unsigned char* const s = S();
    const U v = static_cast<U>(S16(s + 0x36));
    if (v > 4) return;
    if (v == 1) {
        if (Cond_ByteFE == 1) s[2] = static_cast<unsigned char>(s[2] + 1);
        return;
    }
    const bool set = v == 0 ? SH_CALL(Flags_Test)(At(::bof3::Img(Cond_Flags_addr) + 0x70), 0x14) != 0
                            : SH_CALL(Flags_Test)(At(::bof3::Img(Cond_Flags_addr) + 0xa0), 0x7A + (v - 2)) != 0;
    if (set) S()[2] = static_cast<unsigned char>(S()[2] + 1);
}

// original 0x512510 (sub-state 2; a start no list had, reached by
// EffectKind18Sub51_States[2]): +9 up; each cell of the rectangle: its first
// record's bits 19..23 = min(+9, 0x10) and, where MapView_ItemAt answers an
// item, Prim_SetTexture(the record, the item's half for this buffer, 1) -
// DIV-0062's site 0x512607; once +9 is past 0x11 the record released.
extern "C" void __cdecl EffectKind18Sub51_Fade(void) {
    S()[9] = static_cast<unsigned char>(S()[9] + 1);
    const U step = S()[9] > 0x10 ? 0x10u : S()[9];
    ForCells("EffectKind18Sub51_Fade", [step](U x, U z) {
        unsigned char* const record = CellRecord(x, z, 0);
        SetUL(record, (ULEffect6c(record) & 0xFF07FFFFu) | (step << 19));
        const U item = static_cast<U>(SH_CALL(MapView_ItemAt)(I(x), I(z)));
        if (item == 0) return;
        const U half = Gfx_BufferIndex + item * 2;
        if (half >= 2 * draw_pool::Count())
            bof3::Fatal("EffectKind18Sub51_Fade: the item half %u, past the pool's %u items - the original reads past "
                        "DrawItems (docs/effect_6c.md section 7)",
                        (unsigned)half, draw_pool::Count());
        SH_CALL(Prim_SetTexture)(ULEffect6c(record), draw_pool::Items() + half * at::kItemHalf, 1);
    });
    if (S()[9] > 0x11) SH_CALL(Effect_Release)();
}

// original 0x512660 (hidden in 0x512040): on +2 = 0 +0x3C = the ground at the
// record's point (AreaMap_Elevation, an s16) and +2 up. Each frame, while the
// camera's cell is within 20 (|dx| + |dz|) of the record's (+0x36, +0x3A): an
// additive draw mode linked at the record (dy 1); +9 up 2; sixteen rings k of
// sixteen TILE_1 dots, shaded 0xFF - ((+9 - 0x10 k) & 0xFF), each at angle
// ((Frame_Counter & 0xF) << 4) * kSpin53[k & 1] + 0x100 i round the record's
// point (3 * (sin >> 6) about (+0x34 >> 9) - 0x4000), lifted -(+0x3C / 2) -
// 2 ((+9 - 0x10 k) & 0xFF); projected, linked at the record (dy 1, 0x14); a
// draw mode closing it.
extern "C" void __cdecl EffectKind18Sub53_Run(void) {
    if (S()[2] == 0) {
        const unsigned char* const s = S();
        const long h = SH_CALL(AreaMap_Elevation)(Long(s + 0x34), Long(s + 0x38));
        rv::Store<U>(rv::Current<records::EffectObject>()->height, static_cast<U>(static_cast<std::int16_t>(static_cast<U>(h) & 0xFFFF)));
        S()[2] = static_cast<unsigned char>(S()[2] + 1);
    }
    {
        const unsigned char* const s = S();
        const U near = Abs(static_cast<U>(S16(::bof3::Img(at::kKind2ZHigh.v)) - S16(s + 0x3A))) + Abs(static_cast<U>(S16(::bof3::Img(at::kKind2XHigh.v)) - S16(s + 0x36)));
        if (I(near) > 0x14) return;
    }
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0xB5, 0);
    {
        unsigned char* const s = S();
        SH_CALL(MapView_LinkPrimAt)(ULEffect6c(s + 0x34), ULEffect6c(s + 0x38), 1, 0xC);
    }
    S()[9] = static_cast<unsigned char>(S()[9] + 2);
    for (U k = 0, ring = 0; I(ring) < 0x100; ++k, ring += 0x10) {
        const unsigned char shade = static_cast<unsigned char>(0xFFu - ((S()[9] - ring) & 0xFF));
        const std::int32_t spin = S8(::bof3::Img(at::kSpin53.v) + (k & 1));
        for (U angle = 0; I(angle) < 0x1000; angle += 0x100) {
            unsigned char* const p = Gfx_PacketNext;
            SH_CALL(Gpu_SetTile1)(p);
            SH_CALL(Gpu_SetSemiTrans)(p, 1);
            p[4] = p[5] = p[6] = shade;
            const U sine = static_cast<U>(SH_CALL(Math_Sin)(I(((Frame_Counter & 0xF) << 4) * static_cast<U>(spin) + angle)));
            SetV(0, Sar(sine, 6) * 3 + Sar(static_cast<std::uint32_t>(rv::Current<records::EffectObject>()->x), 9) - 0x4000u);
            const U cosine = static_cast<U>(SH_CALL(Math_Cos)(I(((Frame_Counter & 0xF) << 4) * static_cast<U>(spin) + angle)));
            {
                const unsigned char* const s = S();
                SetV(2, Sar(cosine, 6) * 3 + Sar(ULEffect6c(s + 0x38), 9) - 0x4000u);
                const U lift = (static_cast<U>(s[9]) - (k << 4)) & 0xFF;
                SetV(4, (0u - DivP2(ULEffect6c(s + 0x3C), 1)) - (lift << 1));
            }
            long a;
            SH_CALL(Gte_RotTransPers)(Vx(0), reinterpret_cast<unsigned long*>(p + 8), &a);
            SH_CALL(Gte_StoreDepthF)(Fp(p + 0x10));
            unsigned char* const s = S();
            SH_CALL(MapView_LinkPrimAt)(ULEffect6c(s + 0x34), ULEffect6c(s + 0x38), 1, 0x14);
        }
    }
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0xB5, 0);
    unsigned char* const s = S();
    SH_CALL(MapView_LinkPrimAt)(ULEffect6c(s + 0x34), ULEffect6c(s + 0x38), 1, 0xC);
}

void EffectKind18Sub51_Inject() {
    BOF3_INJECT(EffectKind18Sub51_Run);
    BOF3_INJECT(EffectKind18Sub51_Place);
    BOF3_INJECT(EffectKind18Sub51_Wait);
    BOF3_INJECT(EffectKind18Sub51_Fade);
    BOF3_INJECT(EffectKind18Sub53_Run);
}

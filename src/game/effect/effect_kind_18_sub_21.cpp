// Moved here by tools/move_funcs.py from effect_5d.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_18_sub_21.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_5d_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_5d::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

unsigned char* S() { return Sprite_Current; }

U ULEffect5d(const unsigned char* p) { return static_cast<U>(Long(p)); }

U ULEffect5d(U a) { return ULEffect5d(At(a)); }

void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }

U W(const unsigned char* p) { return Word(p); }

U W(U a) { return Word(At(a)); }

unsigned char& B(U a) { return *At(a); }

std::int32_t I(U v) { return static_cast<std::int32_t>(v); }

std::int32_t S16(U v) { return static_cast<std::int16_t>(v & 0xFFFFu); }

std::int32_t S8(unsigned char v) { return static_cast<signed char>(v); }

U Abs(std::int32_t v) { return static_cast<U>(v < 0 ? -v : v); }   // cdq; xor; sub (small values here)

U AddressOf(const void* p) { return Low32(p); }

U FC() { return Frame_Counter; }

void StoreFloat(unsigned char* p, long double v) {
    const float f = static_cast<float>(v);   // fstp dword
    std::memcpy(p, &f, sizeof f);
}

unsigned char* Vertex() { return reinterpret_cast<unsigned char*>(Prim_VertexScratch); }

void SetV(unsigned offset, U v) { SetWord(Vertex() + offset, v); }

const short* Vx(unsigned i) { return reinterpret_cast<const short*>(Vertex() + 8 * i); }

float* Fp(unsigned char* p) { return reinterpret_cast<float*>(p); }

using Handler = scenario_harness::Handler;

// A sub-kind's state table: its address and its own length (symbols.toml [[data]]).
#define E5D_TABLE(name) AddressOf(name), name##_count

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 2]; jmp (or call)
// [table + eax * 4]: the table's `entries` handlers read in place (the fuzz
// swaps the cells for recorders); a Fatal past them, where the original jumps
// through the dword after - the next table or data.
void DispatchEffect5d(const char* who, U table, unsigned entries) {
    const unsigned state = Sprite_Current[2];
    if (state >= entries)
        bof3::Fatal("%s: sub-state byte +2 is %u, past the %u entries of 0x%X - the original jumps through the dword "
                    "after (docs/effect_5d.md section 7)",
                    who, state, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(ULEffect5d(table + 4 * state)))();
}

}  // namespace

// original 0x505E20 (EffectKind18_States[33], hidden in 0x505BF0): call
// [EffectKind18Sub21_States + +2 * 4] (unbounded), then the spiral at +9.
extern "C" void __cdecl EffectKind18Sub21_Run(void) {
    DispatchEffect5d("EffectKind18Sub21_Run", E5D_TABLE(EffectKind18Sub21_States));
    SH_CALL(EffectKind18Sub21_DrawSpiral)(S()[9]);
}

// original 0x505E50 (sub-state 1): +9 up two.
extern "C" void __cdecl EffectKind18Sub21_Step(void) { S()[9] = static_cast<unsigned char>(S()[9] + 2); }

// original 0x505E60 (cdecl): with Draw_PassFlags bit 2 set: a draw mode, then
// sixteen steps of a spiral about the screen's centre (160, 120) - a POLY_G3
// from the centre and two POLY_G4s outward, their corners Math_Cos / Math_Sin
// of the step's angle (from 0x200 by 0x100) at three radii (x / 16, / 8, / 4
// over 3; y half that) - grey |0x80 - n| / 2 inside, the rim's red and blue
// from Math_Cos of n's phases; committed to slot 7; a draw mode after. The
// centre is the 320 x 240 screen's (DIV-0041 not applied here).
extern "C" void __cdecl EffectKind18Sub21_DrawSpiral(U n) {
    if ((Draw_PassFlags & 4) == 0) return;
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x95, 0);
    SH_CALL(Gfx_CommitPrim)(7, 0xC);
    const unsigned char grey = static_cast<unsigned char>(Abs(static_cast<std::int32_t>(0x80 - n)) >> 1);
    U angle = 0x200, phase = n << 4;
    const U lag = n << 4;
    auto x_at = [](U a, unsigned shift) {
        const std::int32_t c = static_cast<std::int32_t>(SH_CALL(Math_Cos)(static_cast<int>(a)));
        return static_cast<long double>((c >> shift) / 3 + 0xA0);
    };
    auto y_at = [](U a, unsigned shift) {
        const std::int32_t c = static_cast<std::int32_t>(SH_CALL(Math_Sin)(static_cast<int>(a)));
        return static_cast<long double>((c >> shift) / 3 + 0x78);
    };
    auto rim = [](U a) {
        const std::int32_t c = static_cast<std::int32_t>(SH_CALL(Math_Cos)(static_cast<int>(a)));
        return static_cast<unsigned char>(((c >> 6) + 0x80) >> 1);
    };
    for (U k = 0x900; k < 0x1900; k += 0x100) {
        unsigned char* const g3 = Gfx_PacketNext;
        unsigned char* const g4 = g3 + 0x34;
        unsigned char* const g5 = g3 + 0x78;
        SH_CALL(Gpu_SetPolyG3)(g3);
        SH_CALL(Gpu_SetPolyG4)(g4);
        SH_CALL(Gpu_SetPolyG4)(g5);
        SetUL(g3 + 8, 0x43200000u);
        SetUL(g3 + 0xC, 0x42F00000u);
        long double v = x_at(angle - 0x200, 4);
        StoreFloat(g4 + 8, v);
        StoreFloat(g3 + 0x18, v);
        v = y_at(angle - 0x200, 5);
        StoreFloat(g4 + 0xC, v);
        StoreFloat(g3 + 0x1C, v);
        v = x_at(angle - 0x100, 4);
        StoreFloat(g4 + 0x18, v);
        StoreFloat(g3 + 0x28, v);
        v = y_at(angle - 0x100, 5);
        StoreFloat(g4 + 0x1C, v);
        StoreFloat(g3 + 0x2C, v);
        v = x_at(angle - 0x100, 3);
        StoreFloat(g5 + 8, v);
        StoreFloat(g4 + 0x28, v);
        v = y_at(angle - 0x100, 4);
        StoreFloat(g5 + 0xC, v);
        StoreFloat(g4 + 0x2C, v);
        v = x_at(angle, 3);
        StoreFloat(g5 + 0x18, v);
        StoreFloat(g4 + 0x38, v);
        v = y_at(angle, 4);
        StoreFloat(g5 + 0x1C, v);
        StoreFloat(g4 + 0x3C, v);
        StoreFloat(g5 + 0x28, x_at(angle, 2));
        StoreFloat(g5 + 0x2C, y_at(angle, 3));
        angle += 0x100;
        StoreFloat(g5 + 0x38, x_at(angle, 2));
        StoreFloat(g5 + 0x3C, y_at(angle, 3));
        g3[6] = grey;
        g3[5] = grey;
        g3[4] = grey;
        g5[0x36] = grey;
        g5[0x35] = grey;
        g5[0x34] = grey;
        g5[0x26] = grey;
        g5[0x25] = grey;
        g5[0x24] = grey;
        unsigned char c = rim(phase);
        g5[6] = c;
        g4[0x26] = c;
        g4[6] = c;
        g3[0x16] = c;
        c = rim(phase + 0x100);
        g5[0x16] = c;
        g4[0x36] = c;
        g4[0x16] = c;
        g3[0x26] = c;
        c = rim(k - lag - 0x100);
        g5[4] = c;
        g4[0x24] = c;
        g4[4] = c;
        g3[0x14] = c;
        c = rim(k - lag);
        g5[0x14] = c;
        g4[0x34] = c;
        g4[0x14] = c;
        g3[0x24] = c;
        g3[0x25] = 0;
        g3[0x15] = 0;
        g4[0x15] = 0;
        g4[5] = 0;
        g4[0x35] = 0;
        g4[0x25] = 0;
        g5[0x15] = 0;
        g5[5] = 0;
        SH_CALL(Gfx_CommitPrim)(7, 0x34);
        SH_CALL(Gfx_CommitPrim)(7, 0x44);
        SH_CALL(Gfx_CommitPrim)(7, 0x44);
        phase += 0x100;
    }
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x95, 0);
    SH_CALL(Gfx_CommitPrim)(7, 0xC);
}

// original 0x506290 (EffectKind18_States[82], hidden in 0x505E60): the camera's
// cell within 20 (|dx| + |dz|) of the record's: a draw mode (0xB5) linked on
// the record's row; +9 up two; sixteen rings (i) of sixteen semi-transparent
// dots about the record's point, turning by (Frame_Counter & 0xF) << 4 one way
// or the other (0x65E330 by the ring's parity), radius 3 (Math_Sin / Math_Cos
// >> 6), grey 0xFF - (+9 - 16 i), height (-0xC0 - (+9 - 16 i)) * 2, each
// projected (Gte_RotTransPers to the dot) and linked (dy 1); a draw mode after.
extern "C" void __cdecl EffectKind18Sub52_Draw(void) {
    unsigned char* s = S();
    const U near = Abs(S16(W(::bof3::Img(at::kCameraZ.v))) - S16(static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->z_whole))) + Abs(S16(W(::bof3::Img(at::kCameraX.v))) - S16(static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->x_whole)));
    if (I(near) > 0x14) return;
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0xB5, 0);
    s = S();
    SH_CALL(MapView_LinkPrimAt)(static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->x), static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->z), 1, 0xC);
    s = S();
    s[9] = static_cast<unsigned char>(s[9] + 2);
    for (U ring = 0; ring < 0x10; ++ring) {
        s = S();
        const U grey = 0xFFu - ((s[9] - ring * 0x10) & 0xFF);
        const std::int32_t sign = S8(B(::bof3::Img(at::kSub52Signs.v) + (ring & 1)));
        for (U turn = 0; turn < 0x1000; turn += 0x100) {
            unsigned char* p = Gfx_PacketNext;
            SH_CALL(Gpu_SetTile1)(p);
            SH_CALL(Gpu_SetSemiTrans)(p, 1);
            p[4] = static_cast<unsigned char>(grey);
            p[5] = static_cast<unsigned char>(grey);
            p[6] = static_cast<unsigned char>(grey);
            const U sn = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(((FC() & 0xF) << 4) * static_cast<U>(sign) + turn)));
            s = S();
            SetV(0, static_cast<U>((I(sn) >> 6) * 3 + (rv::As<records::EffectObject>(s)->x >> 9) - 0x4000));
            const U cs = static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(((FC() & 0xF) << 4) * static_cast<U>(sign) + turn)));
            s = S();
            SetV(2, static_cast<U>((I(cs) >> 6) * 3 + (rv::As<records::EffectObject>(s)->z >> 9) - 0x4000));
            const U fade = (s[9] - static_cast<unsigned char>(ring * 0x10)) & 0xFF;
            SetV(4, (0xFFFFFF40u - fade) << 1);
            long depth[2];
            SH_CALL(Gte_RotTransPers)(Vx(0), reinterpret_cast<unsigned long*>(p + 8), depth);
            p += 0x10;
            SH_CALL(Gte_StoreDepthF)(Fp(p));
            s = S();
            SH_CALL(MapView_LinkPrimAt)(static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->x), static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->z), 1, 0x14);
        }
    }
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0xB5, 0);
    s = S();
    SH_CALL(MapView_LinkPrimAt)(static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->x), static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->z), 1, 0xC);
}

void EffectKind18Sub21_Inject() {
    BOF3_INJECT(EffectKind18Sub21_Run);
    BOF3_INJECT(EffectKind18Sub21_Step);
    BOF3_INJECT(EffectKind18Sub21_DrawSpiral);
    BOF3_INJECT(EffectKind18Sub52_Draw);
}

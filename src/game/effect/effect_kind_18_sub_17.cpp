// Moved here by tools/move_funcs.py from effect_5c.cpp, effect_5d.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_18_sub_17.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect/effect_kind_18_sub_17_callees.h"
#include "game/effect_5c_callees.h"
#include "game/effect_5d_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/draw_order.h"
#include "hook/log.h"

namespace {

namespace at = effect_kind_18_sub_17::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

using Handler = scenario_harness::Handler;

using StepFn = void(__cdecl*)(unsigned);

using QuadsFn = void(__cdecl*)(unsigned);

using MovesFn = void(__cdecl*)(void);

unsigned char* S() { return Sprite_Current; }

U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }

std::int32_t SWEffect5c(U v) { return static_cast<std::int16_t>(v & 0xFFFFu); }

U AddressOf(const void* p) { return Low32(p); }

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 2]; jmp [table +
// eax * 4]: the table's `entries` handlers read in place (the fuzz swaps the
// cells for recorders); a Fatal past them, where the original jumps through the
// dword after - the next table or data.
void DispatchEffect5c(const char* who, U table, unsigned entries) {
    const unsigned state = Sprite_Current[2];
    if (state >= entries)
        bof3::Fatal("%s: sub-state byte +2 is %u, past the %u entries of 0x%X - the original jumps through the dword "
                    "after (docs/effect_5c.md section 6)",
                    who, state, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(At(table + 4 * state))))();
}

// Effect_FindFree's record (answer & 0xFF, as the originals widen it): a Fatal
// on an answer past the twenty (the callee answers 0xFF or 0..19; the original
// would write past the pool).
unsigned char* NewRecord(const char* who, unsigned index) {
    if (index >= 20)
        bof3::Fatal("%s: Effect_FindFree answered %u, past the 20 records - the original writes past the pool "
                    "(docs/effect_5c.md section 6)",
                    who, index);
    return rv::Bytes(Effect_Objects) + index * 0x80u;
}

// E5D's three (raw until E5D merges).
void Step(unsigned step) { SH_AT(StepFn, at::kDrawMoveStep.v)(step); }

void Quads(unsigned variant) { SH_AT(QuadsFn, at::kDrawQuads.v)(variant); }

void Moves() { SH_AT(MovesFn, at::kDrawMoves.v)(); }

// The tail most of its sub-states share: every fifth frame E5D's draw-move
// step (Frame_Counter / 5) & 3; E5D's quads variants 0 and 1; a tail jump to
// E5D's four draw-moves.
void Sub17Tail() {
    const U f = Frame_Counter;
    if (f % 5u == 0) Step((f / 5u) & 3u);
    Quads(0);
    Quads(1);
    Moves();
}

// The enemy wait: in game mode 5 step 5 with bit 14 of the dword at `bits`, +2
// up. Then +9 at 0xF wraps to 0; on every fifth +9 E5D's step +9 / 5 + 7 (the
// record read again after it). Answers the record +9 is stepped on.
unsigned char* EnemyWait(U bits) {
    if (Game_Mode == 5 && Game_Step == 5 && (UL(At(bits)) & 0x4000u) != 0) S()[2] = static_cast<unsigned char>(S()[2] + 1);
    unsigned char* s = S();
    if (s[9] == 0xF) {
        s[9] = 0;
        s = S();
    }
    const unsigned t = s[9];
    if (t % 5 == 0) {
        Step(t / 5 + 7);
        s = S();
    }
    return s;
}

// The settle: +9 at 0xF wraps to 0; +0x32 (s16) above 0 counts down, else at
// +9 / 5 = 0 +2 up; every fifth +9 E5D's step +9 / 5 + 7; +9 up.
void Settle() {
    unsigned char* s = S();
    if (s[9] == 0xF) {
        s[9] = 0;
        s = S();
    }
    const U w = rv::As<records::EffectWork>(s)->work_32;
    if (SWEffect5c(w) > 0) {
        rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_32, w - 1u);
    } else if (s[9] / 5 == 0) {
        s[2] = static_cast<unsigned char>(s[2] + 1);
    }
    s = S();
    const unsigned t = s[9];
    if (t % 5 == 0) {
        Step(t / 5 + 7);
        s = S();
    }
    s[9] = static_cast<unsigned char>(s[9] + 1);
}

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

U ULEffect5d(const unsigned char* p) { return static_cast<U>(Long(p)); }

U ULEffect5d(U a) { return ULEffect5d(At(a)); }

void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }

U W(const unsigned char* p) { return Word(p); }

U W(U a) { return Word(At(a)); }

unsigned char& B(U a) { return *At(a); }

std::int32_t I(U v) { return static_cast<std::int32_t>(v); }

std::int32_t S16(U v) { return static_cast<std::int16_t>(v & 0xFFFFu); }

U Abs(std::int32_t v) { return static_cast<U>(v < 0 ? -v : v); }   // cdq; xor; sub (small values here)

U FC() { return Frame_Counter; }

float Fl(U a) {
    float f;
    std::memcpy(&f, At(a), sizeof f);
    return f;
}

long double SX() { return Fl(AddressOf(MapView_ScreenXY)); }   // fld / fadd dword [0x903820]

long double SY() { return MapView_ScreenXY[1]; }

void StoreFloat(unsigned char* p, long double v) {
    const float f = static_cast<float>(v);   // fstp dword
    std::memcpy(p, &f, sizeof f);
}

// The CRT's _ftol 0x5B9550 on the value x87 holds: truncation through a 64-bit
// fistp, whose NaN and out-of-range answer is the integer indefinite
// 0x8000000000000000; the callers keep eax's low word.
U Ftol(long double v) {
    if (!(v > -9.2233720368547758e18L && v < 9.2233720368547758e18L)) return 0;
    return static_cast<U>(static_cast<std::uint64_t>(static_cast<std::int64_t>(v)));
}

unsigned char* Vertex() { return reinterpret_cast<unsigned char*>(Prim_VertexScratch); }

void SetV(unsigned offset, U v) { SetWord(Vertex() + offset, v); }

const short* Vx(unsigned i) { return reinterpret_cast<const short*>(Vertex() + 8 * i); }

float* Fp(unsigned char* p) { return reinterpret_cast<float*>(p); }

unsigned long* ScreenXY() { return reinterpret_cast<unsigned long*>(MapView_ScreenXY); }

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

// An index into one of the image's tables by a record word or byte, checked:
// past it the original reads the data after (another table or code pointers).
U Index(const char* who, std::int32_t index, unsigned count, U table) {
    if (index < 0 || static_cast<unsigned>(index) >= count)
        bof3::Fatal("%s: index %d, past the %u entries of the table at 0x%X - the original reads the data after "
                    "(docs/effect_5d.md section 7)",
                    who, (int)index, count, (unsigned)table);
    return static_cast<U>(index);
}

// 0x5049D0 (jumped to by 0x504900 only; its own frame, the function's tail):
// in the camera's frame turned about z by 0x40 - |0x80 - 2 (Frame_Counter &
// 0x7F)| and moved to the record's point (Gte_RotTrans), sixteen POLY_GT4
// strips side by side (x -12 i .. -12 (i + 1)), each edge waved by Math_Sin of
// the frame, shaded by Math_Cos of it; between two draw modes (slot 4), the
// GTE matrix pushed and popped.
void Sub19Draw() {
    SH_CALL(Gte_PushMatrix)();
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x95, 0);
    SH_CALL(Gfx_CommitPrim)(4, 0xC);
    const std::int32_t turn = 0x40 - static_cast<std::int32_t>(Abs(0x80 - static_cast<std::int32_t>((FC() & 0x7F) << 1)));
    alignas(4) unsigned char angles[8] = {};
    alignas(4) unsigned char point[8] = {};
    alignas(4) unsigned char matrix[0x20] = {};
    SetWord(angles + 4, static_cast<U>(turn));
    unsigned char* const s = S();
    SetWord(point, static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->x_frac));
    SetWord(point + 2, static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->z_frac));
    SetWord(point + 4, static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->height_frac));
    SH_CALL(Gte_RotTrans)(reinterpret_cast<const short*>(point), reinterpret_cast<long*>(matrix + 0x14));
    SH_CALL(Gte_RotMatrix)(reinterpret_cast<const short*>(angles), reinterpret_cast<short*>(matrix));
    SH_CALL(Gte_MulMatrix0)(Camera_Matrix, reinterpret_cast<const short*>(matrix),
                            reinterpret_cast<short*>(matrix));
    SH_CALL(Gte_SetRotMatrix)(reinterpret_cast<const unsigned long*>(matrix));
    SH_CALL(Gte_SetTransMatrix)(reinterpret_cast<const unsigned long*>(matrix));
    auto shade = [](U angle, U times) {
        const U c = static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(angle)));
        return static_cast<unsigned char>((I(c * times) >> 11) + 0x80);
    };
    for (U i = 0; i < 0x10; ++i) {
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyGT4)(p);
        SH_CALL(Gpu_SetShadeTex)(p, 0);
        const unsigned char u0 = static_cast<unsigned char>(0x20 - 2 * i), u1 = static_cast<unsigned char>(0x1E - 2 * i);
        rv::Store<std::uint32_t>(rv::As<records::GpuPolyGT4>(p)->tpage, 0x1B);
        rv::Store<std::uint32_t>(rv::As<records::GpuPolyGT4>(p)->clut, 0x78CF);
        p[0x14] = u0;
        p[0x15] = 0xC0;
        p[0x28] = u1;
        p[0x29] = 0xC0;
        p[0x3C] = u0;
        p[0x3D] = 0xD8;
        p[0x50] = u1;
        p[0x51] = 0xD8;
        const U before = i != 0 ? i - 1 : 0;
        const unsigned char c0 = shade(((before + FC()) << 9) & 0xFFF, before);
        p[6] = c0;
        p[5] = c0;
        p[4] = c0;
        const unsigned char c1 = shade(((FC() + i) << 9) & 0xFFF, i);
        p[0x2E] = c1;
        p[0x2D] = c1;
        p[0x2C] = c1;
        p[0x1A] = c1;
        p[0x19] = c1;
        p[0x18] = c1;
        const U after = i + 1;
        const unsigned char c2 = shade(((FC() + i + 1) << 9) & 0xFFF, after);
        p[0x42] = c2;
        p[0x41] = c2;
        p[0x40] = c2;
        const U near_x = static_cast<U>(I(0u - ((3 * i) << 6)) / 16);
        SetV(0x10, near_x);
        SetV(0, near_x);
        const U far_x = static_cast<U>(I(((0xFFFFFFFFu - i) * 3) << 6) / 16);
        SetV(0x18, far_x);
        SetV(8, far_x);
        auto wave = [](U angle, U times) {
            const U v = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(angle)));
            return static_cast<U>(I(v * times) >> 13);
        };
        SetV(2, wave(((FC() - before) << 9) & 0xFFF, before));
        const U mid = wave(((FC() - i) << 9) & 0xFFF, i);
        SetV(0x12, mid);
        SetV(0xA, mid);
        SetV(0x1A, wave(((FC() - i - 1) << 9) & 0xFFF, after));
        SetV(0xC, 0xFF70);
        SetV(4, 0xFF70);
        SetV(0x1C, 0);
        SetV(0x14, 0);
        long depth[2];
        SH_CALL(Gte_RotTransPers4)(Vx(0), Vx(1), Vx(2), Vx(3), Fp(p + 8), Fp(p + 0x1C), Fp(p + 0x30), Fp(p + 0x44), depth);
        SH_CALL(Gte_PrimDepths4_14)(p);
        SH_CALL(Gfx_CommitPrim)(4, 0x54);
    }
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x95, 0);
    SH_CALL(Gfx_CommitPrim)(4, 0xC);
    SH_CALL(Gte_PopMatrix)();
}

}  // namespace

// original 0x503660 (EffectKind18_States[23], hidden in 0x5032C0):
// jmp [EffectKind18Sub17_States + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub17_Run(void) {
    DispatchEffect5c("EffectKind18Sub17_Run", AddressOf(EffectKind18Sub17_States), EffectKind18Sub17_States_count);
}

// original 0x503680 (sub-state 0): the point (+0x34, +0x38) = (0x570000,
// 0x140000), +0x3E = 0xFF00, +0x32 = 0, +9 = 0, +0xB = 0; +2 up.
extern "C" void __cdecl EffectKind18Sub17_Start(void) {
    unsigned char* const s = S();
    auto* const so = rv::As<records::EffectObject>(s);
    so->x = 0x570000;
    so->z = 0x140000;
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->height_whole, 0xFF00);
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_32, 0);
    s[9] = 0;
    s[0xB] = 0;
    s[2] = static_cast<unsigned char>(s[2] + 1);
}

// original 0x5036D0 (sub-state 1): at the counter 0x903848 = 0xC: +2 up,
// Cond_ByteFE = 0x23, and the record Effect_FindFree answers made kind 0x18,
// sub-kind 0x17, sub-state 0xA - unchecked: on none free (0xFF) the original
// writes 0x7F80 past the pool, ours aborts. Then the shared tail.
extern "C" void __cdecl EffectKind18Sub17_WaitCue(void) {
    if (At(::bof3::Img(effect_5c::at::kCounter.v))[0] == 0xC) {
        S()[2] = static_cast<unsigned char>(S()[2] + 1);
        Cond_ByteFE = 0x23;
        unsigned char* const rec = NewRecord("EffectKind18Sub17_WaitCue", SH_CALL(Effect_FindFree)() & 0xFFu);
        rec[0] = 1;
        rec[5] = 0x18;
        rec[1] = 0x17;
        rec[2] = 0xA;
    }
    Sub17Tail();
}

// original 0x503760 (sub-state 2): +0x32 (s16) below 0x80 up one; else +2 up
// and the counter 0x903848 = 0xD. The shared tail.
extern "C" void __cdecl EffectKind18Sub17_Rise(void) {
    unsigned char* const s = S();
    const U w = rv::As<records::EffectWork>(s)->work_32;
    if (SWEffect5c(w) < 0x80) {
        rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_32, w + 1u);
    } else {
        s[2] = static_cast<unsigned char>(s[2] + 1);
        At(::bof3::Img(effect_5c::at::kCounter.v))[0] = 0xD;
    }
    Sub17Tail();
}

// original 0x5037D0 (sub-state 3): the z cell +0x3A below 0x19: +0x38 up 0x1000;
// else +2 up. The shared tail.
extern "C" void __cdecl EffectKind18Sub17_MoveZ(void) {
    unsigned char* const s = S();
    auto* const so = rv::As<records::EffectObject>(s);
    if (SWEffect5c(rv::As<records::EffectObject>(s)->z_whole) < 0x19) so->z = static_cast<std::uint32_t>(so->z) + 0x1000u;
    else s[2] = static_cast<unsigned char>(s[2] + 1);
    Sub17Tail();
}

// original 0x503830 (sub-state 4): +0x3A below 0x1C: +0x34 down 0x800, +0x38 up
// 0x800; else +2 up. The shared tail.
extern "C" void __cdecl EffectKind18Sub17_MoveXZ(void) {
    unsigned char* const s = S();
    auto* const so = rv::As<records::EffectObject>(s);
    if (SWEffect5c(rv::As<records::EffectObject>(s)->z_whole) < 0x1C) {
        so->x = static_cast<std::uint32_t>(so->x) + 0xFFFFF800u;
        so->z = static_cast<std::uint32_t>(so->z) + 0x800u;
    } else {
        s[2] = static_cast<unsigned char>(s[2] + 1);
    }
    Sub17Tail();
}

// original 0x5038A0 (sub-state 5): the x cell +0x36 at least 0x4F: +0x34 down
// 0x1000; else with (Frame_Counter / 5) & 3 = 3 +2 up. The shared tail.
extern "C" void __cdecl EffectKind18Sub17_MoveX(void) {
    unsigned char* const s = S();
    auto* const so = rv::As<records::EffectObject>(s);
    if (SWEffect5c(rv::As<records::EffectObject>(s)->x_whole) >= 0x4F) so->x = static_cast<std::uint32_t>(so->x) + 0xFFFFF000u;
    else if (((Frame_Counter / 5u) & 3u) == 3) s[2] = static_cast<unsigned char>(s[2] + 1);
    Sub17Tail();
}

// original 0x503920 (sub-state 6): every fifth +9 E5D's step +9 / 5 + 4; +9 up;
// E5D's quads variant 1; at +9 / 5 = 3: Cond_ByteFE = 0, +9 = 1, +2 up.
extern "C" void __cdecl EffectKind18Sub17_Count(void) {
    unsigned char* s = S();
    const unsigned t = s[9];
    if (t % 5 == 0) {
        Step(t / 5 + 4);
        s = S();
    }
    s[9] = static_cast<unsigned char>(s[9] + 1);
    Quads(1);
    s = S();
    if (s[9] / 5 != 3) return;
    Cond_ByteFE = 0;
    s[9] = 1;
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
}

// original 0x5039B0 (sub-state 7): the enemy wait on EnemyWorkingRecords record
// 0 (0x93B9F2); +9 up; E5D's quads variant 1.
extern "C" void __cdecl EffectKind18Sub17_WaitEnemy0(void) {
    unsigned char* const s = EnemyWait(::bof3::Img(Battle_Enemies_addr) + 0x92);
    s[9] = static_cast<unsigned char>(s[9] + 1);
    Quads(1);
}

// original 0x503A40 (sub-state 8): the settle; E5D's quads variant 1.
extern "C" void __cdecl EffectKind18Sub17_Settle(void) {
    Settle();
    Quads(1);
}

// original 0x503AE0 (sub-state 9): at +9 / 5 = 4 Effect_Release (and nothing
// more); else every fifth +9 E5D's step 7 - +9 / 5; +9 up; E5D's quads variant 1.
extern "C" void __cdecl EffectKind18Sub17_End(void) {
    unsigned char* s = S();
    const unsigned t = s[9];
    const unsigned q = t / 5;
    if (q == 4) {
        SH_CALL(Effect_Release)();
        return;
    }
    if (t % 5 == 0) {
        Step(7 - q);
        s = S();
    }
    s[9] = static_cast<unsigned char>(s[9] + 1);
    Quads(1);
}

// original 0x503B50 (sub-state 10, where sub-state 1's spawn starts): the point
// (+0x34, +0x38) = (0x430000, 0x1A0000), +0x3E = 0xFF00, +0x32 = 0x80, +9 = 0,
// +0xB = 1; MoveCmd_TestFB(0x55, 0x19) (al unread); +2 up.
extern "C" void __cdecl EffectKind18Sub17_Start10(void) {
    unsigned char* const s = S();
    auto* const so = rv::As<records::EffectObject>(s);
    so->x = 0x430000;
    so->z = 0x1A0000;
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->height_whole, 0xFF00);
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_32, 0x80);
    s[9] = 0;
    s[0xB] = 1;
    SH_CALL(MoveCmd_TestFB)(0x55, 0x19);
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
}

// original 0x503BB0 (sub-state 11): at the counter 0x903848 = 0xF with
// (Frame_Counter / 5) & 3 = 3: E5D's step 4, MoveCmd_TestFB(0x43, 0x1A), +2
// up. Else every fifth frame E5D's step (Frame_Counter / 5) & 3, and E5D's
// quads variant 2.
extern "C" void __cdecl EffectKind18Sub17_WaitCue11(void) {
    const unsigned char cue = At(::bof3::Img(effect_5c::at::kCounter.v))[0];
    const U f = Frame_Counter;
    if (cue == 0xF && ((f / 5u) & 3u) == 3) {
        Step(4);
        SH_CALL(MoveCmd_TestFB)(0x43, 0x1A);
        S()[2] = static_cast<unsigned char>(S()[2] + 1);
        return;
    }
    if (f % 5u == 0) Step((f / 5u) & 3u);
    Quads(2);
}

// original 0x503C20 (sub-state 12): every fifth +9 E5D's step +9 / 5 + 4; +9
// up; at +9 / 5 = 3: +2 up, +9 = 1.
extern "C" void __cdecl EffectKind18Sub17_Count12(void) {
    unsigned char* s = S();
    const unsigned t = s[9];
    if (t % 5 == 0) {
        Step(t / 5 + 4);
        s = S();
    }
    s[9] = static_cast<unsigned char>(s[9] + 1);
    s = S();
    if (s[9] / 5 != 3) return;
    s[2] = static_cast<unsigned char>(s[2] + 1);
    S()[9] = 1;
}

// original 0x503CA0 (sub-state 13): the enemy wait on EnemyWorkingRecords record
// 1 (0x93BB1A); +9 up.
extern "C" void __cdecl EffectKind18Sub17_WaitEnemy1(void) {
    unsigned char* const s = EnemyWait(::bof3::Img(BossH_Enemy1Status_addr));
    s[9] = static_cast<unsigned char>(s[9] + 1);
}

// original 0x503D30 (sub-state 14): the settle; MoveCmd_TestFB(0x42, +0x32 / 6
// + 0x18) (al unread).
extern "C" void __cdecl EffectKind18Sub17_Settle14(void) {
    Settle();
    const std::int32_t lean = SWEffect5c(rv::As<records::EffectWork>(S())->work_32) / 6;
    SH_CALL(MoveCmd_TestFB)(0x42, static_cast<short>(lean + 0x18));
}

// original 0x503DE0 (0x65E1A8's entry 15, hidden in E5C's 0x5032C0): q = +9 / 5;
// at 4, MoveCmd_TestFC(0x43, 0x1A) (al unread) and Effect_Release; else on
// every fifth frame EffectKind18Sub17_CopyFrame(7 - q); +9 up one.
extern "C" void __cdecl EffectKind18Sub17_Close(void) {
    unsigned char* s = S();
    const U n = s[9];
    const U q = n / 5;
    if (q == 4) {
        SH_CALL(MoveCmd_TestFC)(0x43, 0x1A);
        SH_CALL(Effect_Release)();
        return;
    }
    if (n % 5 == 0) {
        SH_CALL(EffectKind18Sub17_CopyFrame)(7u - q);
        s = S();
    }
    s[9] = static_cast<unsigned char>(s[9] + 1);
}

// original 0x503E50 (code no list has, after 0x503DE0; E5C's five states
// 0x5036D0..0x5038A0 tail-jump to it - its own frame and ret): a 16 x 32 block
// of VRAM at (0x260, 0x100) scrolled by the record's point - u = bits 13..16 of
// +0x34 with bit 16 flipped, v = bits 12..16 of +0x38 - as four
// Gpu_SetDrawMove copies from (0x170, 0x1A0), each committed to slot 6 (0x18).
extern "C" void __cdecl EffectKind18Sub17_ScrollTexture(void) {
    unsigned char* const s = S();
    auto* const so = rv::As<records::EffectObject>(s);
    const U u = (static_cast<U>(I(static_cast<std::uint32_t>(so->x) ^ 0xFFFF1FFFu) >> 13)) & 0xF;
    const U v = static_cast<U>(so->z >> 12) & 0x1F;
    unsigned char rect[8];
    auto copy = [&rect](U x, U y, U w, U h, U to_x, U to_y) {
        unsigned char* const p = Gfx_PacketNext;
        SetWord(rect, x);
        SetWord(rect + 2, y);
        SetWord(rect + 4, w);
        SetWord(rect + 6, h);
        SH_CALL(Gpu_SetDrawMove)(p, rect, to_x, to_y);
        SH_CALL(Gfx_CommitPrim)(6, 0x18);
    };
    copy(0x170, 0x1A0, u, v, 0x270 - u, 0x120 - v);
    copy(u + 0x170, 0x1A0, 0x10 - u, v, 0x260, 0x120 - v);
    copy(0x170, v + 0x1A0, u, 0x20 - v, 0x270 - u, 0x100);
    copy(u + 0x170, v + 0x1A0, 0x10 - u, 0x20 - v, 0x260, 0x100);
}

// original 0x503FA0 (cdecl; E5C's states call it 15 times with 0, 1 or 2):
// with Draw_PassFlags bit 2 set (nothing otherwise), MapView_ScreenXY the
// record's point less 0x4000 cells (>> 9), and sixteen textured quads in a
// 4 x 4 grid of 128-unit cells about it, each corner on the ground
// (AreaMap_Elevation, halved) less the record's +0x32 scaled by a height byte
// of the quad's record in 0x65E1E8; its flag byte picks the corner order and
// the depth (Gte_StoreDepthF4 or Gte_PrimDepths4_10); the texture its flag,
// the variant's byte and page 0x2180.. (variant 0) or 0x2580..; a draw mode
// and the quad linked on the record's row (dy (i & 3) + (i >> 2) - 1).
extern "C" void __cdecl EffectKind18Sub17_DrawPatch(U variant) {
    if ((Draw_PassFlags & 4) == 0) return;
    unsigned char* s = S();
    const std::int32_t x0 = (rv::As<records::EffectObject>(s)->x >> 9) - 0x4000;
    StoreFloat(reinterpret_cast<unsigned char*>(MapView_ScreenXY), static_cast<long double>(x0));
    const std::int32_t z0 = (rv::As<records::EffectObject>(s)->z >> 9) - 0x4000;
    StoreFloat(At(::bof3::Img(MapView_ScreenXY_addr) + 0x4), static_cast<long double>(z0));
    if (variant >= at::kPatchVariants)
        bof3::Fatal("EffectKind18Sub17_DrawPatch: variant %u, past the three bytes of each 0x65E1E8 record - the original "
                    "reads the next record's (docs/effect_5d.md section 7)",
                    (unsigned)variant);
    const U page = variant == 0 ? 0x21800000u : 0x25800000u;
    const long double half = Fl(at::kHalfCell);
    for (U i = 0; i < at::kPatchCount; ++i) {
        const U rec = ::bof3::Img(at::kPatch.v) + at::kPatchStride * i;
        SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x95, 0);
        const U row = i >> 2;
        const U dy = ((i & 3) + row - 1) & 0xFF;
        s = S();
        SH_CALL(MapView_LinkPrimAt)(static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->x), static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->z), static_cast<int>(dy), 0xC);
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyFT4)(p);
        SH_CALL(Gpu_SetShadeTex)(p, 0);
        const long double fx = static_cast<float>(static_cast<std::int32_t>((i & 3) << 7));
        const long double fy = static_cast<float>(static_cast<std::int32_t>(row << 7));
        // each corner: x and y about the cell's centre, z the ground less the height byte's share
        auto corner = [&](unsigned k, long double sx, long double sy, U height_byte) {
            const U x = Ftol(sx) & 0xFFFF;
            SetV(8 * k, x);
            const U y = Ftol(sy) & 0xFFFF;
            SetV(8 * k + 2, y);
            const U e = static_cast<U>(SH_CALL(AreaMap_Elevation)(static_cast<long>((static_cast<U>(S16(x) + 0x4000)) << 9),
                                                                  static_cast<long>((static_cast<U>(S16(y) + 0x4000)) << 9)));
            const std::int32_t h = S16(static_cast<std::uint32_t>(rv::As<records::EffectWork>(S())->work_32));
            const std::int32_t lift = I(B(height_byte) * static_cast<U>(h)) / 128;
            SetV(8 * k + 4, static_cast<U>(-(S16(e) / 2) - lift));
        };
        corner(0, fx + SX() - half, fy + SY() - half, rec);
        corner(1, fx + SX() + half, fy + SY() - half, rec + 1);
        corner(2, fx + SX() - half, fy + SY() + half, rec + 2);
        corner(3, fx + SX() + half, fy + SY() + half, rec + 3);
        const unsigned char flag = B(rec + 4);
        long depth[2];
        if (flag != 0) {
            SH_CALL(Gte_RotTransPers4)(Vx(0), Vx(1), Vx(2), Vx(3), Fp(p + 0x18), Fp(p + 8), Fp(p + 0x38), Fp(p + 0x28), depth);
            SH_CALL(Gte_StoreDepthF4)(Fp(p + 0x20), Fp(p + 0x10), Fp(p + 0x40), Fp(p + 0x30));
        } else {
            SH_CALL(Gte_RotTransPers4)(Vx(0), Vx(1), Vx(2), Vx(3), Fp(p + 8), Fp(p + 0x18), Fp(p + 0x28), Fp(p + 0x38), depth);
            SH_CALL(Gte_PrimDepths4_10)(p);
        }
        SH_CALL(Prim_SetTexture)((static_cast<U>(B(rec + 4)) << 16) | B(rec + 5 + variant) | page, p, 1);
        s = S();
        SH_CALL(MapView_LinkPrimAt)(static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->x), static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->z), static_cast<int>(dy), 0x48);
    }
}

// original 0x5043B0 (cdecl; 0x503DE0 and E5C's states): a 32 x 64 block of VRAM
// copied from (0x240, 0x100) + frame `frame`'s (u, v) byte pair (0x65E268, ten)
// to ((+0xB + 12) * 48, 0x100), committed to slot 6 (0x18). The whole word is
// the index: past the ten (or below 0) the original reads the data after.
extern "C" void __cdecl EffectKind18Sub17_CopyFrame(U frame) {
    unsigned char rect[8];
    SetWord(rect + 4, 0x20);
    SetWord(rect + 6, 0x40);
    const U f = Index("EffectKind18Sub17_CopyFrame", I(frame), at::kFrameCount, at::kFrames.v);
    SetWord(rect, B(::bof3::Img(at::kFrames.v) + 2 * f) + 0x240u);
    SetWord(rect + 2, B(::bof3::Img(at::kFrames.v) + 2 * f + 1) + 0x100u);
    const U to_x = (S()[0xB] + 0xCu) * 0x30u;
    SH_CALL(Gpu_SetDrawMove)(Gfx_PacketNext, rect, to_x, 0x100);
    SH_CALL(Gfx_CommitPrim)(6, 0x18);
}

// original 0x504430 (EffectKind18_States[24], hidden in 0x5043B0):
// jmp [EffectKind18Sub18_States + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub18_Run(void) {
    DispatchEffect5d("EffectKind18Sub18_Run", E5D_TABLE(EffectKind18Sub18_States));
}

// original 0x504450 (sub-state 0): the column at +9 (0x80), the ring at +9; +9
// up one; past 7, +2 up.
extern "C" void __cdecl EffectKind18Sub18_Grow(void) {
    SH_CALL(EffectKind18Sub18_DrawColumn)(S()[9], 0x80);
    SH_CALL(EffectKind18Sub18_DrawRing)(S()[9]);
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] + 1);
    s = S();
    if (s[9] > 7) s[2] = static_cast<unsigned char>(s[2] + 1);
}

// original 0x5044A0 (sub-state 1): the column at 7; below 0x10 the ring at +9;
// +9 up one; past 0x1F, +9 0 and +2 up.
extern "C" void __cdecl EffectKind18Sub18_Hold(void) {
    SH_CALL(EffectKind18Sub18_DrawColumn)(7, 0x80);
    unsigned char* s = S();
    const unsigned char n = s[9];
    if (n < 0x10) {
        SH_CALL(EffectKind18Sub18_DrawRing)(n);
        s = S();
    }
    s[9] = static_cast<unsigned char>(s[9] + 1);
    s = S();
    if (s[9] > 0x1F) {
        s[9] = 0;
        S()[2] = static_cast<unsigned char>(S()[2] + 1);
    }
}

// original 0x5044F0 (sub-state 2): t = 0x1F - +9: the column at (t / 4, 4 t);
// +9 up one; past 0x18, +9 0 and +2 up.
extern "C" void __cdecl EffectKind18Sub18_Shrink(void) {
    const std::int32_t t = 0x1F - static_cast<std::int32_t>(S()[9]);
    SH_CALL(EffectKind18Sub18_DrawColumn)(static_cast<U>(t / 4), static_cast<U>(t) * 4);
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] + 1);
    s = S();
    if (s[9] > 0x18) {
        s[9] = 0;
        S()[2] = static_cast<unsigned char>(S()[2] + 1);
    }
}

// original 0x504550 (sub-state 3): +9 up one; past 0x1C a tail jump to
// Effect_Release.
extern "C" void __cdecl EffectKind18Sub18_End(void) {
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] + 1);
    s = S();
    if (s[9] > 0x1C) SH_CALL(Effect_Release)();
}

// original 0x504570 (cdecl): eight textured quads stacked over the record's
// cell (x, z = (cell - 0x80) << 7, y half the ground's height up), each level
// 8 `grow` higher and 2 `grow` wider than the last (the start 8 + grow
// (Frame_Counter & 1) wider still), texture 0xBA009124 | (level) << 19, linked
// on the record's row (dy 2). The second argument is read only to compute a
// value the code stores back into its own stack slot and reads nowhere else:
// it changes nothing (docs/effect_5d.md section 1.2).
extern "C" void __cdecl EffectKind18Sub18_DrawColumn(U grow, U unused) {
    (void)unused;
    unsigned char* s = S();
    const U x = static_cast<U>(S16(static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->x_whole)) - 0x80) << 7;
    const U z = static_cast<U>(S16(static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->z_whole)) - 0x80) << 7;
    const U e = static_cast<U>(SH_CALL(AreaMap_Elevation)(rv::As<records::EffectObject>(s)->x, rv::As<records::EffectObject>(s)->z));
    const U y = static_cast<U>(-(S16(e) / 2));
    const U odd = (FC() & 1) << 2;
    U b = static_cast<U>(I(odd * grow) / 4) + 8;
    U level = 0, shade = 0x3F;
    std::int32_t c = 0x1F80000;
    do {
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyFT4)(p);
        SH_CALL(Gpu_SetShadeTex)(p, 0);
        SetV(0, x - b);
        SetV(2, z - b);
        SetV(4, y + (((2 - level) << 3) - odd) * grow);
        const U low = y - (odd + level * 8) * grow;
        SetV(8, x + b);
        SetV(0xA, z - b);
        SetV(0xC, low);
        SetV(0x10, x - b);
        SetV(0x12, z + b);
        SetV(0x14, low);
        SetV(0x18, x + b);
        SetV(0x1A, z + b);
        SetV(0x1C, y - (odd + level * 8 + 0x10) * grow);
        long depth[2];
        SH_CALL(Gte_RotTransPers4)(Vx(1), Vx(2), Vx(3), Vx(0), Fp(p + 0x18), Fp(p + 0x28), Fp(p + 0x38), Fp(p + 8), depth);
        SH_CALL(Gte_StoreDepthF4)(Fp(p + 0x20), Fp(p + 0x30), Fp(p + 0x40), Fp(p + 0x10));
        SH_CALL(Prim_SetTexture)((static_cast<U>(I(shade - odd) / 4) << 19) | 0xBA009124u, p, 1);
        s = S();
        SH_CALL(MapView_LinkPrimAt)(static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->x), static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->z), 2, 0x48);
        b += grow * 2;
        ++level;
        c -= 0x400000;
        shade -= 8;
    } while (c > static_cast<std::int32_t>(0xFFF80000u));
}

// original 0x5047A0 (cdecl): MapView_ScreenXY the record's point (its cell,
// half the ground's height up) projected; a draw mode (0xB5) linked on the
// record's row (dy 1); then 64 semi-transparent dots on a ring of radius
// 4 `radius` (x by Math_Sin >> 14, y by Math_Cos >> 15, 8 down), coloured
// (c, c on odd frames, (c >> 1) on odd frames) with c = ((4 radius) ^ 0x3F)
// << 1 (bytes), each linked on the record's row (dy 2).
extern "C" void __cdecl EffectKind18Sub18_DrawRing(U radius) {
    unsigned char* s = S();
    const U x = static_cast<U>(S16(static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->x_whole)) - 0x80) << 7;
    const U z = static_cast<U>(S16(static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->z_whole)) - 0x80) << 7;
    const U e = static_cast<U>(SH_CALL(AreaMap_Elevation)(rv::As<records::EffectObject>(s)->x, rv::As<records::EffectObject>(s)->z));
    SetV(4, static_cast<U>(-(S16(e) / 2)));
    SetV(0, x);
    SetV(2, z);
    long depth[2];
    SH_CALL(Gte_RotTransPers)(Vx(0), ScreenXY(), depth);
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0xB5, 0);
    s = S();
    SH_CALL(MapView_LinkPrimAt)(static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->x), static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->z), 1, 0xC);
    const U r = radius << 2;
    const unsigned char blue = static_cast<unsigned char>((r & 0xFF) ^ 0x3F);
    const unsigned char red = static_cast<unsigned char>(blue << 1);
    for (U j = 0; j < 0x40; ++j) {
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetTile1)(p);
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        SH_CALL(Gte_StoreDepthF)(Fp(p + 0x10));
        const U angle = j << 7;
        const U sn = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(angle)));
        const std::int32_t dx = I(sn * r) >> 14;
        StoreFloat(p + 8, static_cast<long double>(dx) + SX());
        const U cs = static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(angle)));
        const std::int32_t dy = I(cs * r) >> 15;
        p[4] = red;
        StoreFloat(p + 0xC, static_cast<long double>(dy) + SY() + Fl(at::kRingDrop));
        p[5] = static_cast<unsigned char>((FC() & 1) * red);
        p[6] = static_cast<unsigned char>((FC() & 1) * blue);
        s = S();
        SH_CALL(MapView_LinkPrimAt)(static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->x), static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->z), 2, 0x14);
    }
}

// original 0x504900 (EffectKind18_States[25], hidden in 0x5047A0): with +2 0,
// +0xB the x cell's low byte (+0x36) and the point placed from the tables
// 0x65E28C / 0x65E290 / 0x65E294 by it (x (t << 7) - 0x3FC8, z (t - 0x80) << 7,
// the height a word) and +2 up. Then, with Draw_PassFlags bit 2 set and the
// camera's cell within 10 of the place's (less 6, both axes), the fan
// (0x5049D0). The tables hold two places; +0xB past them reads the data after.
extern "C" void __cdecl EffectKind18Sub19_Run(void) {
    static const char kWho[] = "EffectKind18Sub19_Run";
    unsigned char* s = S();
    if (s[2] == 0) {
        s[0xB] = s[0x36];
        s = S();
        U k = Index(kWho, s[0xB], at::kSub19Count, at::kSub19X.v);
        rv::As<records::EffectObject>(s)->x = (static_cast<U>(B(::bof3::Img(at::kSub19X.v) + k)) << 7) - 0x3FC8u;
        s = S();
        k = Index(kWho, s[0xB], at::kSub19Count, at::kSub19Z.v);
        rv::As<records::EffectObject>(s)->z = (static_cast<U>(B(::bof3::Img(at::kSub19Z.v) + k)) - 0x80u) << 7;
        s = S();
        k = Index(kWho, s[0xB], at::kSub19Count, at::kSub19Height.v);
        rv::As<records::EffectObject>(s)->height = static_cast<U>(S16(W(::bof3::Img(at::kSub19Height.v) + 2 * k)));
        s = S();
        s[2] = static_cast<unsigned char>(s[2] + 1);
        s = S();
    }
    if ((Draw_PassFlags & 4) == 0) return;
    const U k = Index(kWho, s[0xB], at::kSub19Count, at::kSub19X.v);
    if (Abs(S16(W(::bof3::Img(at::kCameraX.v))) - static_cast<std::int32_t>(B(::bof3::Img(at::kSub19X.v) + k)) + 6) > 10) return;
    if (Abs(S16(W(::bof3::Img(at::kCameraZ.v))) - static_cast<std::int32_t>(B(::bof3::Img(at::kSub19Z.v) + k)) + 6) > 10) return;
    Sub19Draw();
}

// original 0x504CE0 (EffectKind18_States[104], hidden in 0x5047A0): with
// Draw_PassFlags bit 2 set and the camera within 10 cells of (0x2D, 0x23):
// MapView_ScreenXY (-10624.0, -11904.0); then eight arcs (angle a / 8 - 0x100,
// a = 0..0x7000 by 0x1000) of 32 semi-transparent dots each, at distance
// ((Frame_Counter & 0xF) + 16 k) * 5 / 65536 of Math_Sin / Math_Cos (unsigned)
// from it, z -0x140 less Math_Sin(5 that) >> 5, through the GTE's own
// projection (Gte_LoadVertex, Gte_Rtps), shaded by k and the frame, each
// linked on the record's row (dy 1); a draw mode (0xB5) before every fourth arc.
extern "C" void __cdecl EffectKind18Sub68_Draw(void) {
    if ((Draw_PassFlags & 4) == 0) return;
    if (Abs(S16(W(::bof3::Img(at::kCameraX.v))) - 0x2D) > 10) return;
    if (Abs(S16(W(::bof3::Img(at::kCameraZ.v))) - 0x23) > 10) return;
    SetUL(reinterpret_cast<unsigned char*>(MapView_ScreenXY), 0xC6260000u);
    SetUL(At(::bof3::Img(MapView_ScreenXY_addr) + 0x4), 0xC63A0000u);
    U arc = 0;
    for (U a = 0; a < 0x8000; a += 0x1000, ++arc) {
        const U first = (arc & 0xFFFFFFF8u) == 0 ? 1 : 0;
        if ((arc & 3) == 0) {
            SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0xB5, 0);
            unsigned char* const s = S();
            SH_CALL(MapView_LinkPrimAt)(static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->x), static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->z), static_cast<int>(first), 0xC);
        }
        const U angle = static_cast<U>(I(a) / 8) + 0xFFFFFF00u;
        U k = 0;
        for (std::int32_t step = 0; step > -0x100; step -= 8, k += 0x10) {
            unsigned char* p = Gfx_PacketNext;
            SH_CALL(Gpu_SetTile1)(p);
            SH_CALL(Gpu_SetSemiTrans)(p, 1);
            const U blue = (static_cast<U>(step) - (FC() & 7) + 0x17F) >> 1;
            p[6] = static_cast<unsigned char>(blue);
            const unsigned char other = static_cast<unsigned char>(I(blue) / 4);
            p[4] = other;
            p[5] = other;
            const U sn = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(angle)));
            const U frame = FC();
            const U reach = (frame & 0xF) + k;
            const U dx = (sn * reach * 5) >> 16;
            SetV(0, Ftol(static_cast<long double>(dx) + SX()) & 0xFFFF);
            const U cs = static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(angle)));
            const U dz = (cs * reach * 5) >> 16;
            SetV(2, Ftol(static_cast<long double>(dz) + SY()) & 0xFFFF);
            const U lift = static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(reach * 5)));
            SetV(4, 0xFFFFFEC0u - static_cast<U>(I(lift) >> 5));
            SH_CALL(Gte_LoadVertex)(reinterpret_cast<const unsigned long*>(Vertex()));
            SH_CALL(Gte_Rtps)();
            SH_CALL(Gte_StoreScreenXY)(reinterpret_cast<unsigned long*>(p + 8));
            p += 0x10;
            SH_CALL(Gte_StoreDepthF)(Fp(p));
            unsigned char* const s = S();
            SH_CALL(MapView_LinkPrimAt)(static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->x), static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->z), static_cast<int>(first), 0x14);
        }
    }
}

// original 0x504F00 (EffectKind18_States[27], hidden in 0x5047A0): +9 up one;
// at 1, MoveCmd_TestFB(cell x + 1, cell z); at 5, MoveCmd_TestFB(cell x, cell
// z) (their al unread); at 0xF, story flag 0x1C cleared and a tail jump to
// Effect_Release.
extern "C" void __cdecl EffectKind18Sub1B_Run(void) {
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] + 1);
    s = S();
    if (s[9] == 1) {
        SH_CALL(MoveCmd_TestFB)(static_cast<short>(static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->x_whole) + 1), static_cast<short>(static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->z_whole)));
        s = S();
    }
    if (s[9] == 5) {
        SH_CALL(MoveCmd_TestFB)(static_cast<short>(static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->x_whole)), static_cast<short>(static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->z_whole)));
        s = S();
    }
    if (s[9] == 0xF) {
        SH_CALL(Flags_Clear)(At(::bof3::Img(Cond_Flags_addr) + 0xa0), 0x1C);
        SH_CALL(Effect_Release)();
    }
}

// original 0x504F70 (EffectKind18_States[67], hidden in 0x5047A0):
// jmp [EffectKind18Sub43_States + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub43_Run(void) {
    DispatchEffect5d("EffectKind18Sub43_Run", E5D_TABLE(EffectKind18Sub43_States));
}

// original 0x504F90 (sub-state 1): while the counter 0x903848 is 0xE, the quad
// at (+9, +9); +9 up one; past 0x6C, Cond_ByteFE 1, the counter 0xF and +2 up.
extern "C" void __cdecl EffectKind18Sub43_Glow(void) {
    if (B(::bof3::Img(effect_5d::at::kCounter.v)) != 0xE) return;
    const unsigned char n = S()[9];
    SH_CALL(EffectKind18Sub43_DrawQuad)(n, n);
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] + 1);
    s = S();
    if (s[9] > 0x6C) {
        Cond_ByteFE = 1;
        B(::bof3::Img(effect_5d::at::kCounter.v)) = 0xF;
        s[2] = static_cast<unsigned char>(s[2] + 1);
    }
}

// original 0x504FE0 (sub-state 3): the counter at 0x1C: Cond_ByteFE 0 and a tail
// jump to Effect_Release.
extern "C" void __cdecl EffectKind18Sub43_End(void) {
    if (B(::bof3::Img(effect_5d::at::kCounter.v)) != 0x1C) return;
    Cond_ByteFE = 0;
    SH_CALL(Effect_Release)();
}

// original 0x505000 (cdecl): one textured quad (x (-0xD40 - g) * 4 .. 4 g -
// 0x3500, y (-0x7C0 - g) * 4 .. 4 g - 0x1F00, z -0x80, low words), depths
// Gte_PrimDepths4_10, texture 0x25100100 + ((shade & 0xF8) << 16), committed to
// slot 5 (0x48).
extern "C" void __cdecl EffectKind18Sub43_DrawQuad(U grow, U shade) {
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyFT4)(p);
    SH_CALL(Gpu_SetShadeTex)(p, 0);
    const U x0 = (0xFFFFF2C0u - grow) << 2, x1 = grow * 4 - 0x3500;
    const U y0 = (0xFFFFF840u - grow) << 2, y1 = grow * 4 - 0x1F00;
    SetV(0x10, x0);
    SetV(0, x0);
    SetV(0x18, x1);
    SetV(8, x1);
    SetV(0xA, y0);
    SetV(2, y0);
    SetV(0x1A, y1);
    SetV(0x12, y1);
    SetV(0x1C, 0xFF80);
    SetV(0x14, 0xFF80);
    SetV(0xC, 0xFF80);
    SetV(4, 0xFF80);
    long depth[2];
    SH_CALL(Gte_RotTransPers4)(Vx(0), Vx(1), Vx(2), Vx(3), Fp(p + 8), Fp(p + 0x18), Fp(p + 0x28), Fp(p + 0x38), depth);
    SH_CALL(Gte_PrimDepths4_10)(p);
    SH_CALL(Prim_SetTexture)(((shade & 0xF8) << 16) + 0x25100100u, p, 1);
    SH_CALL(Gfx_CommitPrim)(5, 0x48);
}

void EffectKind18Sub17_Inject() {
    BOF3_INJECT(EffectKind18Sub17_Run);
    BOF3_INJECT(EffectKind18Sub17_Start);
    BOF3_INJECT(EffectKind18Sub17_WaitCue);
    BOF3_INJECT(EffectKind18Sub17_Rise);
    BOF3_INJECT(EffectKind18Sub17_MoveZ);
    BOF3_INJECT(EffectKind18Sub17_MoveXZ);
    BOF3_INJECT(EffectKind18Sub17_MoveX);
    BOF3_INJECT(EffectKind18Sub17_Count);
    BOF3_INJECT(EffectKind18Sub17_WaitEnemy0);
    BOF3_INJECT(EffectKind18Sub17_Settle);
    BOF3_INJECT(EffectKind18Sub17_End);
    BOF3_INJECT(EffectKind18Sub17_Start10);
    BOF3_INJECT(EffectKind18Sub17_WaitCue11);
    BOF3_INJECT(EffectKind18Sub17_Count12);
    BOF3_INJECT(EffectKind18Sub17_WaitEnemy1);
    BOF3_INJECT(EffectKind18Sub17_Settle14);
    BOF3_INJECT(EffectKind18Sub17_Close);
    BOF3_INJECT(EffectKind18Sub17_ScrollTexture);
    BOF3_INJECT(EffectKind18Sub17_DrawPatch);
    BOF3_INJECT(EffectKind18Sub17_CopyFrame);
    BOF3_INJECT(EffectKind18Sub18_Run);
    BOF3_INJECT(EffectKind18Sub18_Grow);
    BOF3_INJECT(EffectKind18Sub18_Hold);
    BOF3_INJECT(EffectKind18Sub18_Shrink);
    BOF3_INJECT(EffectKind18Sub18_End);
    BOF3_INJECT(EffectKind18Sub18_DrawColumn);
    BOF3_INJECT(EffectKind18Sub18_DrawRing);
    BOF3_INJECT(EffectKind18Sub19_Run);
    BOF3_INJECT(EffectKind18Sub68_Draw);
    BOF3_INJECT(EffectKind18Sub1B_Run);
    BOF3_INJECT(EffectKind18Sub43_Run);
    BOF3_INJECT(EffectKind18Sub43_Glow);
    BOF3_INJECT(EffectKind18Sub43_End);
    BOF3_INJECT(EffectKind18Sub43_DrawQuad);
}

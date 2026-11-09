// Moved here by tools/move_funcs.py from effect_4d.cpp, effect_4e.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_9a.h"

#include <bit>
#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/effect/effect_kind_9a_callees.h"
#include "game/effect_4d.h"
#include "game/effect_4d_callees.h"
#include "game/effect_4e.h"
#include "game/effect_4e_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_kind_9a::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

using move_script::SetLong;

using Handler = scenario_harness::Handler;

unsigned char* S() { return Sprite_Current; }

U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }

void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }

U AddressOf(const void* p) { return Low32(p); }

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 1]; jmp [table + eax
// * 4]: the table's `entries` handlers read in place (the fuzz swaps the cells
// for recorders); a Fatal past them, where the original jumps through the dword
// after - the next kind's table or data.
void Dispatch(const char* who, U table, unsigned entries) {
    const unsigned state = Sprite_Current[1];
    if (state >= entries)
        bof3::Fatal("%s: state byte +1 is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/effect_4d.md section 6)",
                    who, state, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(At(table + 4 * state))))();
}

// mov al, [ecx + 9]; mov dl, al; dec dl; test al, al; mov [ecx + 9], dl: +9
// down one; answers whether it WAS 0 (kind 0x9A's holds).
bool WasZero() {
    unsigned char* const s = S();
    const unsigned char v = s[9];
    s[9] = static_cast<unsigned char>(v - 1);
    return v == 0;
}

}  // namespace

// Sprite_Objects record 1 made current (Sprite_Current kept), its word at
// `offset` (+0x38 or +0x34) moved by `step` four times through Sprite_Current
// read again, the kept record's +1 and +9 set, Sprite_Current put back.
void Kind9ANudge(unsigned offset, U step, unsigned char state, unsigned char count) {
    unsigned char* const kept = Sprite_Current;
    Sprite_Current = At(::bof3::Img(Sprite_Objects_addr) + 0xa4);
    for (unsigned n = 4; n != 0; --n) {
        unsigned char* const o = Sprite_Current;
        SetUL(o + offset, UL(o + offset) + step);
    }
    kept[1] = state;
    Sprite_Current = kept;
    kept[9] = count;
}

namespace {

std::int32_t SL(const unsigned char* p) { return Long(p); }

std::int32_t S16(U v) { return static_cast<std::int16_t>(v); }

// sar: an arithmetic right shift of the 32 bits as the original holds them.
U Sar(U v, unsigned n) { return static_cast<U>(static_cast<std::int32_t>(v) >> n); }

// Three dwords moved as the original moves them (mov, not the FPU).
void Copy12(unsigned char* to, const unsigned char* from) {
    for (unsigned i = 0; i < 12; i += 4) SetUL(to + i, UL(from + i));
}

long* Point(unsigned char* p) { return reinterpret_cast<long*>(p); }

float* Out(unsigned char* p) { return reinterpret_cast<float*>(p); }

const unsigned char* Bytes(const long* p) { return reinterpret_cast<const unsigned char*>(p); }

unsigned char* Bytes(long* p) { return reinterpret_cast<unsigned char*>(p); }

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 1]; jmp (or call)
// [table + eax * 4]: the table's `entries` handlers read in place (the fuzz
// swaps the cells for recorders); a Fatal past them, where the original
// jumps through the dword after - the next kind's table or data.
void DispatchEffect4e(const char* who, const void* table, unsigned entries) {
    const unsigned state = S()[1];
    if (state >= entries)
        bof3::Fatal("%s: state byte +1 is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/effect_4e.md section 6)",
                    who, state, entries, (unsigned)AddressOf(table));
    reinterpret_cast<scenario_harness::Handler>(static_cast<std::uintptr_t>(UL(static_cast<const unsigned char*>(table) + 4 * state)))();
}

// Gpu_GetTPage(0, abr, x, y), then Gpu_SetDrawMode(cursor, 0, dtd, the page's
// low word, 0 - the fifth word the leftover of GetTPage's five pushes).
void SetMode(U abr, int x, int y, int dtd) {
    const U tp = SH_CALL(Gpu_GetTPage)(0, abr, x, y);
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, dtd, tp & 0xFFFFu, 0);
}

// +1 up, Sprite_Current read afresh.
void NextState() {
    unsigned char* const s = S();
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

}  // namespace

// original 0x48DDE0 (Effect_KindHandlers[0x9A], hidden in 0x48DC40):
// jmp [EffectKind9A_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind9A_Run(void) {
    Dispatch("EffectKind9A_Run", AddressOf(EffectKind9A_States), EffectKind9A_States_count);
}

// original 0x48DE00 (state 0): +9 0; +1 = EffectKind9A_Branches[Rand & 7] (1,
// the y nudge, or 4, the x nudge).
extern "C" void __cdecl EffectKind9A_Start(void) {
    S()[9] = 0;
    const unsigned pick = static_cast<U>(SH_CALL(Rand)()) & 7u;
    S()[1] = EffectKind9A_Branches[pick];
}

// original 0x48DE30 (state 1): record 1's +0x38 up 0x1000 (0x400 four times);
// +1 = 2, +9 = 4.
extern "C" void __cdecl EffectKind9A_NudgeY(void) { Kind9ANudge(0x38, 0x400, 2, 4); }

// original 0x48DE70 (state 2): +9 down; when it was 0, +1 = 3.
extern "C" void __cdecl EffectKind9A_HoldY(void) {
    if (WasZero()) S()[1] = 3;
}

// original 0x48DE90 (state 3): record 1's +0x38 back down 0x1000; +1 = 7,
// +9 0.
extern "C" void __cdecl EffectKind9A_BackY(void) { Kind9ANudge(0x38, 0xFFFFFC00u, 7, 0); }

// original 0x48DED0 (state 4): record 1's +0x34 up 0x1000; +1 = 5, +9 = 4.
extern "C" void __cdecl EffectKind9A_NudgeX(void) { Kind9ANudge(0x34, 0x400, 5, 4); }

// original 0x48DF10 (state 5): +9 down; when it was 0, +1 = 6.
extern "C" void __cdecl EffectKind9A_HoldX(void) {
    if (WasZero()) S()[1] = 6;
}

// original 0x48DF30 (state 6): record 1's +0x34 back down 0x1000; +1 = 7,
// +9 0.
extern "C" void __cdecl EffectKind9A_BackX(void) { Kind9ANudge(0x34, 0xFFFFFC00u, 7, 0); }

// original 0x48DF70 (state 7): at the counter 0x903848 = 0x28 a tail jump to
// Effect_Release; else +1 = 0 (another nudge).
extern "C" void __cdecl EffectKind9A_Repeat(void) {
    if (At(::bof3::Img(effect_4d::at::kCounter.v))[0] == at::kKind9AEnd) {
        SH_CALL(Effect_Release)();
        return;
    }
    S()[1] = 0;
}

// original 0x48DF90 (hidden in E4D's 0x48DC40): jmp [EffectKind9B_States +
// +1 * 4], unbounded.
extern "C" void __cdecl EffectKind9B_Run(void) {
    DispatchEffect4e("EffectKind9B_Run", EffectKind9B_States, EffectKind9B_States_count);
}

// original 0x48DFB0 (state 0): +0x3C = AreaMap_Elevation(+0x34, +0x38) as an
// s16 << 16; the height +0x14 and its speed +0x20 = 0; +1 up.
extern "C" void __cdecl EffectKind9B_Start(void) {
    unsigned char* s = S();
    const long elevation = SH_CALL(AreaMap_Elevation)(rv::As<records::EffectObject>(s)->x, rv::As<records::EffectObject>(s)->z);
    s = S();
    rv::As<records::EffectObject>(s)->height = static_cast<U>(S16(static_cast<U>(elevation))) << 16;
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->step_y, 0);
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->work_20, 0);
    NextState();
}

// original 0x48E000 (state 1): the speed +0x20 up 0x100000, the height +0x14
// up by it; the ring drawn at (+0x34, +0x38, +0x3C) that high; at 0x8000000
// or more (signed) +1 up.
extern "C" void __cdecl EffectKind9B_Rise(void) {
    unsigned char* s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->work_20, static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->work_20) + 0x100000u);
    s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->step_y, static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->step_y) + static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->work_20));
    s = S();
    alignas(4) unsigned char q[12];
    Copy12(q, s + 0x34);
    SH_CALL(EffectKind9B_DrawRing)(Point(q), rv::As<records::EffectObject>(s)->step_y);
    if (SL(S() + 0x14) >= 0x8000000) NextState();
}

// original 0x48E070 (state 2): the ring drawn at (+0x34, +0x38, +0x3C),
// 0x8000000 high. Never leaves.
extern "C" void __cdecl EffectKind9B_Hold(void) {
    alignas(4) unsigned char q[12];
    Copy12(q, S() + 0x34);
    SH_CALL(EffectKind9B_DrawRing)(Point(q), 0x8000000);
}

// original 0x48E0A0: EffectGte_LoadMapCamera; sixteen semi-transparent
// POLY_G4 round `point`, radius (cos << 9) sar 4 on x and (sin << 9) sar 4 on
// z (angles 0x100 apart): each quad's bottom edge at the point's height, its
// top `height` above, both projected (EffectGte_ProjectPoint) for the angle
// before and this one. Each quad: a draw mode (Gpu_GetTPage(0, 1, 0x3C0, 0),
// dtd 1) linked 0xC at the corner (MapView_LinkPrimAt), the quad linked 0x44,
// a second draw mode (dtd 0) linked 0xC. The shade starts at ((Frame_Counter
// & 1) + 8) << 2, the first edge's; each quad's second edge 4 more for quads
// 0..3 and 12..15, 4 less for 4..11, carried on.
extern "C" void __cdecl EffectKind9B_DrawRing(const long* point, long height) {
    SH_CALL(EffectGte_LoadMapCamera)();
    const unsigned char* const o = Bytes(point);
    alignas(4) unsigned char a[12], p0[12], q0[12], p1[12], q1[12];
    // the corner at angle t: a = (x + cos * 32, z + sin * 32, y); p0 its bottom
    // projected, q0 its top
    const auto corner = [&](int t) {
        const int c = SH_CALL(Math_Cos)(t);
        SetUL(a, Sar(static_cast<U>(c) << 9, 4) + UL(o));
        const int sn = SH_CALL(Math_Sin)(t);
        SetUL(a + 4, Sar(static_cast<U>(sn) << 9, 4) + UL(o + 4));
        SetUL(a + 8, UL(o + 8));
        SH_CALL(EffectGte_ProjectPoint)(Point(a), Out(p0));
        SetUL(a + 8, static_cast<U>(height) + UL(o + 8));
        SH_CALL(EffectGte_ProjectPoint)(Point(a), Out(q0));
    };
    corner(0);
    unsigned char shade = static_cast<unsigned char>(((Frame_Counter & 1u) + 8u) << 2);
    U angle = 0;
    for (unsigned i = 0; i < 0x10; ++i) {
        Copy12(p1, p0);
        Copy12(q1, q0);
        angle += 0x100;
        corner(static_cast<int>(angle & 0xFFFFu));
        SetMode(1, 0x3C0, 0, 1);
        SH_CALL(MapView_LinkPrimAt)(UL(a), UL(a + 4), 0, 0xC);
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG4)(p);
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        Copy12(p + 8, p1);
        Copy12(p + 0x18, q1);
        Copy12(p + 0x28, p0);
        Copy12(p + 0x38, q0);
        p[4] = p[5] = p[6] = shade;
        p[0x14] = p[0x15] = p[0x16] = shade;
        shade = static_cast<unsigned char>(i < 4 || i >= 0xC ? shade + 4 : shade - 4);
        p[0x24] = p[0x25] = p[0x26] = shade;
        p[0x34] = p[0x35] = p[0x36] = shade;
        SH_CALL(MapView_LinkPrimAt)(UL(a), UL(a + 4), 0, 0x44);
        SetMode(1, 0x3C0, 0, 0);
        SH_CALL(MapView_LinkPrimAt)(UL(a), UL(a + 4), 0, 0xC);
    }
}

void EffectKind9aFromEffect4d_Inject() {
    BOF3_INJECT(EffectKind9A_Run);
    BOF3_INJECT(EffectKind9A_Start);
    BOF3_INJECT(EffectKind9A_NudgeY);
    BOF3_INJECT(EffectKind9A_HoldY);
    BOF3_INJECT(EffectKind9A_BackY);
    BOF3_INJECT(EffectKind9A_NudgeX);
    BOF3_INJECT(EffectKind9A_HoldX);
    BOF3_INJECT(EffectKind9A_BackX);
    BOF3_INJECT(EffectKind9A_Repeat);
}

void EffectKind9aFromEffect4e_Inject() {
    BOF3_INJECT(EffectKind9B_Run);
    BOF3_INJECT(EffectKind9B_Start);
    BOF3_INJECT(EffectKind9B_Rise);
    BOF3_INJECT(EffectKind9B_Hold);
    BOF3_INJECT(EffectKind9B_DrawRing);
}

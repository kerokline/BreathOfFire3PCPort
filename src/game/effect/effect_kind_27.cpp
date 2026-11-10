// Moved here by tools/move_funcs.py from effect_1d.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_27.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_1d.h"
#include "game/effect_1d_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_1d::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

unsigned char* S() { return Sprite_Current; }

U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }

U AddressOf(const void* p) { return Low32(p); }

// sar: an arithmetic right shift of the 32 bits as the original holds them.
U Sar(U v, unsigned n) { return static_cast<U>(static_cast<std::int32_t>(v) >> n); }

// cdq; and edx, 0xFF; add eax, edx; sar eax, 8: a signed divide by 256, toward zero.
U Div256(U v) { return Sar(v + (static_cast<std::int32_t>(v) < 0 ? 0xFFu : 0u), 8); }

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 1]; jmp [table + eax * 4]:
// the table's `entries` handlers read in place (the fuzz swaps the cells for
// recorders); a Fatal past them, where the original jumps through the dword
// after - the next kind's table or data.
void Dispatch(const char* who, U table, unsigned entries) {
    const unsigned state = Sprite_Current[1];
    if (state >= entries)
        bof3::Fatal("%s: state byte +1 is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/effect_1d.md section 2)",
                    who, state, entries, (unsigned)table);
    reinterpret_cast<scenario_harness::Handler>(static_cast<std::uintptr_t>(UL(At(table + 4 * state))))();
}

// Effect_FindFree's record (answer & 0xFF, as the originals widen it): a Fatal
// on an answer past the twenty (the callee answers 0xFF or 0..19; the original
// would write past the pool).
unsigned char* NewRecord(const char* who, unsigned index) {
    if (index >= at::kEffects)
        bof3::Fatal("%s: Effect_FindFree answered %u, past the 20 records - the original writes past the pool "
                    "(docs/effect_1d.md section 2)",
                    who, index);
    return rv::Bytes(Effect_Objects) + index * at::kEffectStride;
}

// +1 up, Sprite_Current read afresh.
void NextState() {
    unsigned char* const s = S();
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

}  // namespace

// original 0x46FEE0: jmp [EffectKind27_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind27_Run(void) { Dispatch("EffectKind27_Run", AddressOf(EffectKind27_States), EffectKind27_States_count); }

// original 0x46FF00 (state 0): the dword +0xC = 0x1000 frames, +1 up.
extern "C" void __cdecl EffectKind27_Start(void) {
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_0c, 0x1000);
    NextState();
}

// original 0x46FF20 (state 1): when the low four bits of +0xC are 0,
// Effect_FindFree; a record found gets +0 = 1, +5 = 0x26 (a band), +0x34 /
// +0x38 = Sprite_Current's, and +0x3C = AreaMap_Elevation(Sprite_Current's
// +0x34, +0x38) (its low word, sign-extended) << 16 - the ground under it.
// Then the dword +0xC down; at 0, +1 up.
extern "C" void __cdecl EffectKind27_Emit(void) {
    if ((S()[0xC] & 0xF) == 0) {
        const unsigned char found = SH_CALL(Effect_FindFree)();
        if (found != 0xFF) {
            unsigned char* const e = NewRecord("EffectKind27_Emit", found);
            auto* const eo = rv::As<records::EffectObject>(e);
            e[0] = 1;
            e[5] = 0x26;
            eo->x = static_cast<std::uint32_t>(rv::Current<records::EffectObject>()->x);
            eo->z = static_cast<std::uint32_t>(rv::Current<records::EffectObject>()->z);
            const unsigned char* const s = S();
            auto* const so = rv::AsConst<records::EffectObject>(s);
            const long ground = SH_CALL(AreaMap_Elevation)(so->x, so->z);
            eo->height = static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(ground))) << 16;
        }
    }
    unsigned char* t = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(t)->work_0c, rv::As<records::EffectWork>(t)->work_0c - 1u);
    t = S();
    if (rv::As<records::EffectWork>(t)->work_0c != 0) return;
    t[1] = static_cast<unsigned char>(t[1] + 1);
}

// original 0x46FFB0 (0x346 bytes): a draw mode (as EffectKind25_DrawDisc's,
// committed 0xC) and EffectGte_LoadMapCamera; then, of t's low word: the outer
// angle ro = t, at most 0x400, the outer shade 0x80 - or, past 0x400, ro =
// 0x400 and the shade (0x500 - t) * 0x80 / 0x100 (a signed divide, its low
// byte); the inner angle ri = t - 0x100 (low word) and the inner shade 0 - or,
// when that is below 0 (s16), ri = 0 and the shade (0x100 - t) * 0x80 / 0x100's
// low byte. For a = 0, 0x100, .. 0xF00 (b = a + 0x100, each as its low word): a
// Gouraud quad at the packet cursor (read afresh), Gpu_SetPolyG4,
// Gpu_SetSemiTrans(1), its four vertices EffectGte_ProjectPoint of the world
// points (x + ((3 cos(u) sin(p)) sar 8 with the low four bits cleared), z +
// ((3 sin(u) sin(p)) sar 8, cleared likewise), height + (3 cos(p) << 12)) for
// (u, p) = (a, ro), (a, ri), (b, ro), (b, ri), shaded outer, inner, outer,
// inner; committed 0x44. Each Math_* is called in the original's order (below).
extern "C" void __cdecl EffectKind26_DrawBand(long x, long z, long height, unsigned t) {
    const unsigned tpage = SH_CALL(Gpu_GetTPage)(0, 1, 0x3C0, 0);
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage & 0xFFFFu, 0);
    SH_CALL(Gfx_CommitPrim)(1, 0xC);
    SH_CALL(EffectGte_LoadMapCamera)();
    const U tw = t & 0xFFFFu;
    U outer = 0x80;   // bl: only its low byte is stored
    U ro = tw;
    if (tw > 0x400) {
        ro = 0x400;
        outer = Div256((0x500u - tw) << 7);
    }
    U ri = (t - 0x100u) & 0xFFFFu;
    U inner = 0;
    if (static_cast<std::int16_t>(ri) < 0) {
        inner = Div256(static_cast<U>(-static_cast<std::int32_t>(static_cast<std::int16_t>(ri))) << 7);
        ri = 0;
    }
    const auto outer_shade = static_cast<unsigned char>(outer);
    const auto inner_shade = static_cast<unsigned char>(inner);
    const U ux = static_cast<U>(x), uz = static_cast<U>(z), uh = static_cast<U>(height);
    // The original's point: three dwords on its stack (x, z, height), rewritten
    // before each projection.
    long point[3];
    auto horizontal = [](U trig, U polar) { return Sar(trig * polar * 3u, 8) & 0xFFFFFFF0u; };
    auto Cos = [](U angle) { return static_cast<U>(SH_CALL(Math_Cos)(static_cast<int>(angle))); };
    auto Sin = [](U angle) { return static_cast<U>(SH_CALL(Math_Sin)(static_cast<int>(angle))); };
    U a = 0;
    for (unsigned n = 0; n < 16; ++n) {
        unsigned char* const prim = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG4)(prim);
        SH_CALL(Gpu_SetSemiTrans)(prim, 1);
        const U a16 = a & 0xFFFFu;
        // vertex 0: (a, ro), outer
        {
            const U cu = Cos(a16);
            const U sp = Sin(ro);
            point[0] = static_cast<long>(horizontal(cu, sp) + ux);
            const U su = Sin(a16);
            const U sp2 = Sin(ro);
            point[1] = static_cast<long>(horizontal(su, sp2) + uz);
            point[2] = static_cast<long>((Cos(ro) * 3u << 12) + uh);
            SH_CALL(EffectGte_ProjectPoint)(point, reinterpret_cast<float*>(prim + 8));
            prim[4] = prim[5] = prim[6] = outer_shade;
        }
        // vertex 1: (a, ri), inner
        {
            const U sp = Sin(ri);
            const U cu = Cos(a16);
            point[0] = static_cast<long>(horizontal(cu, sp) + ux);
            const U sp2 = Sin(ri);
            const U su = Sin(a16);
            point[1] = static_cast<long>(horizontal(su, sp2) + uz);
            point[2] = static_cast<long>((Cos(ri) * 3u << 12) + uh);
            SH_CALL(EffectGte_ProjectPoint)(point, reinterpret_cast<float*>(prim + 0x18));
            prim[0x14] = prim[0x15] = prim[0x16] = inner_shade;
        }
        a += 0x100;
        const U b16 = a & 0xFFFFu;
        // vertex 2: (b, ro), outer
        {
            const U cu = Cos(b16);
            const U sp = Sin(ro);
            point[0] = static_cast<long>(horizontal(cu, sp) + ux);
            const U su = Sin(b16);
            const U sp2 = Sin(ro);
            point[1] = static_cast<long>(horizontal(su, sp2) + uz);
            point[2] = static_cast<long>((Cos(ro) * 3u << 12) + uh);
            SH_CALL(EffectGte_ProjectPoint)(point, reinterpret_cast<float*>(prim + 0x28));
            prim[0x24] = prim[0x25] = prim[0x26] = outer_shade;
        }
        // vertex 3: (b, ri), inner
        {
            const U cu = Cos(b16);
            const U sp = Sin(ri);
            point[0] = static_cast<long>(horizontal(cu, sp) + ux);
            const U su = Sin(b16);
            const U sp2 = Sin(ri);
            point[1] = static_cast<long>(horizontal(su, sp2) + uz);
            point[2] = static_cast<long>((Cos(ri) * 3u << 12) + uh);
            SH_CALL(EffectGte_ProjectPoint)(point, reinterpret_cast<float*>(prim + 0x38));
            prim[0x34] = prim[0x35] = prim[0x36] = inner_shade;
        }
        SH_CALL(Gfx_CommitPrim)(1, 0x44);
    }
}

void EffectKind27_Inject() {
    BOF3_INJECT(EffectKind27_Run);
    BOF3_INJECT(EffectKind27_Start);
    BOF3_INJECT(EffectKind27_Emit);
    BOF3_INJECT(EffectKind26_DrawBand);
}

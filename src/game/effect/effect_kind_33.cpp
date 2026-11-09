// Moved here by tools/move_funcs.py from effect_2b.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_33.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_2b.h"
#include "game/effect_2b_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_2b::at;

}  // namespace

namespace {

using move_script::At;

using move_script::Long;

using U = std::uint32_t;

using Handler = void (__cdecl*)();

unsigned char* Cur() { return Sprite_Current; }

U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }

U Key(const void* p) { return Low32(p); }

U AddressOf(const void* p) { return Key(p); }

// imul of two dwords (the product's low 32 bits), then sar: how every point
// here is placed round its centre (>> 12 for the fixed-point trig, >> 4 for
// kind 0x3B's spirals).
std::int32_t MulSar(int t, U v, unsigned shift) { return static_cast<std::int32_t>(static_cast<U>(t) * v) >> shift; }

// jmp / call [table + 4 * index]: the table's `entries` handlers, read in place
// (the fuzz swaps the cells for recorders); a Fatal past them, where the
// original goes through the dword after - the next kind's table or data.
Handler Entry(U table, unsigned index, unsigned entries, const char* who) {
    if (index >= entries)
        bof3::Fatal("%s: index %u past the %u entries of the table at 0x%X (the original jumps through 0x%X)", who,
                    index, entries, (unsigned)table, (unsigned)(table + 4u * index));
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(At(table + 4u * index))));
}

// Gpu_GetTPage(0, abr, x, y) and a draw mode of it at Gfx_PacketNext (dfe 0,
// dtd `dtd`); the fifth zero the original pushed for Gpu_GetTPage and left on
// the stack is the mode's texture window.
void Mode(unsigned abr, int x, int y, int dtd) {
    const unsigned tpage = SH_CALL(Gpu_GetTPage)(0, abr, x, y) & 0xFFFFu;
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, dtd, tpage, 0);
}

}  // namespace

// original 0x4734F0 (Effect_KindHandlers[0x33], hidden in 0x473460's recorded
// extent): a tail jump through EffectKind33_States 0x6543E0 by +1.
extern "C" void __cdecl EffectKind33_Run(void) {
    Entry(AddressOf(EffectKind33_States), Cur()[1], 4, "EffectKind33_Run")();
}

// original 0x473510 (EffectKind33_States[0]): the disc's centre +0xC..+0x14
// the record's point +0x34..+0x3C, its radius +0x1C 0; +9 0xA, +1 up.
extern "C" void __cdecl EffectKind33_Start(void) {
    unsigned char* const s = Cur();
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_0c, static_cast<std::uint32_t>(rv::Current<records::EffectObject>()->x));
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_10, static_cast<std::uint32_t>(rv::Current<records::EffectObject>()->z));
    const U height = static_cast<std::uint32_t>(rv::Current<records::EffectObject>()->height);
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->work_1c, 0);
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->step_y, height);
    Cur()[9] = 0xA;
    unsigned char* const t = Cur();
    t[1] = static_cast<unsigned char>(t[1] + 1);
}

// original 0x473550 (EffectKind33_States[1]): the disc drawn
// (EffectKind33_DrawDisc(+0xC, +0x1C)), its radius +0x1C up 0x1199 (the record
// read before the call); +9 down 1 - at 0, Sound_PlayEffect(0x20D) and +1 up.
extern "C" void __cdecl EffectKind33_Grow(void) {
    unsigned char* const s = Cur();
    SH_CALL(EffectKind33_DrawDisc)(reinterpret_cast<const long*>(s + 0xC), static_cast<long>(static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->work_1c)));
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->work_1c, static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->work_1c) + 0x1199u);
    unsigned char* t = Cur();
    t[9] = static_cast<unsigned char>(t[9] - 1);
    if (Cur()[9] != 0) return;
    SH_CALL(Sound_PlayEffect)(0x20D);
    t = Cur();
    t[1] = static_cast<unsigned char>(t[1] + 1);
}

// original 0x4735B0: the map camera loaded; four quarters of the disc
// (EffectKind33_DrawQuarter) at the angles 0x400 i, each linked a step off the
// centre by EffectKind33_SignX[i] / _SignZ[i].
extern "C" void __cdecl EffectKind33_DrawDisc(const long* centre, long radius) {
    SH_CALL(EffectGte_LoadMapCamera)();
    for (U i = 0; i < 4; ++i) {
        const auto dz = static_cast<long>(UL(At(AddressOf(EffectKind33_SignZ) + 4 * i)));
        const auto dx = static_cast<long>(UL(At(AddressOf(EffectKind33_SignX) + 4 * i)));
        SH_CALL(EffectKind33_DrawQuarter)(centre, radius, i << 10, dx, dz);
    }
}

// original 0x473600: the centre projected; the point at `angle` (its low 16
// bits) and `radius` round it (the height the centre's, read once) projected;
// then four times: a draw mode (the page at (0x380, 0x100), abr 2, dtd 0)
// linked at the centre's x + dx, z + dz (MapView_LinkPrimAt(.., 0, 0x38)); a
// semi-transparent POLY_F3 (0x5A7570) at Gfx_PacketNext (read after the link)
// from the centre's projection to the last point's and the point 0x100 further
// round (the angle kept whole in its argument's slot), grey 0x80; linked the
// same.
extern "C" void __cdecl EffectKind33_DrawQuarter(const long* centre, long radius, unsigned angle, long dx, long dz) {
    float mid[3], edge[3];
    SH_CALL(EffectGte_ProjectPoint)(centre, mid);
    const U r = static_cast<U>(radius);
    long q[3];
    U a = angle & 0xFFFFu;
    const int c = SH_CALL(Math_Cos)(static_cast<int>(a));
    q[0] = static_cast<long>(static_cast<U>(MulSar(c, r, 12)) + static_cast<U>(centre[0]));
    const int n = SH_CALL(Math_Sin)(static_cast<int>(a));
    q[1] = static_cast<long>(static_cast<U>(MulSar(n, r, 12)) + static_cast<U>(centre[1]));
    q[2] = centre[2];
    SH_CALL(EffectGte_ProjectPoint)(q, edge);
    U slot = angle;
    for (unsigned k = 0; k < 4; ++k) {
        Mode(2, 0x380, 0x100, 0);
        const auto x = static_cast<unsigned long>(static_cast<U>(centre[0]) + static_cast<U>(dx));
        const auto z = static_cast<unsigned long>(static_cast<U>(centre[1]) + static_cast<U>(dz));
        SH_CALL(MapView_LinkPrimAt)(x, z, 0, 0x38);
        unsigned char* const prim = Gfx_PacketNext;
        SH_AT(void (__cdecl*)(unsigned char*), at::kSetPolyF3.v)(prim);
        SH_CALL(Gpu_SetSemiTrans)(prim, 1);
        std::memcpy(prim + 8, mid, 12);
        std::memcpy(prim + 0x14, edge, 12);
        slot += 0x100;
        a = slot & 0xFFFFu;
        const int c2 = SH_CALL(Math_Cos)(static_cast<int>(a));
        q[0] = static_cast<long>(static_cast<U>(MulSar(c2, r, 12)) + static_cast<U>(centre[0]));
        const int n2 = SH_CALL(Math_Sin)(static_cast<int>(a));
        q[1] = static_cast<long>(static_cast<U>(MulSar(n2, r, 12)) + static_cast<U>(centre[1]));
        SH_CALL(EffectGte_ProjectPoint)(q, edge);
        std::memcpy(prim + 0x20, edge, 12);
        prim[4] = prim[5] = prim[6] = 0x80;
        SH_CALL(MapView_LinkPrimAt)(x, z, 0, 0x38);
    }
}

void EffectKind33_Inject() {
    BOF3_INJECT(EffectKind33_Run);
    BOF3_INJECT(EffectKind33_Start);
    BOF3_INJECT(EffectKind33_Grow);
    BOF3_INJECT(EffectKind33_DrawDisc);
    BOF3_INJECT(EffectKind33_DrawQuarter);
}

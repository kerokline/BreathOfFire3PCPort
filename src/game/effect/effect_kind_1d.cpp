// Moved here by tools/move_funcs.py from effect_1c.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_1d.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_1c.h"
#include "game/effect_1c_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_1c::at;

}  // namespace

namespace {

using move_script::At;

using move_script::Long;

using move_script::SetLong;

using move_script::Word;

using U = std::uint32_t;

using Handler = void (__cdecl*)();

unsigned char* Cur() { return Sprite_Current; }

U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }

void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }

short SW(const unsigned char* p) { return static_cast<short>(Word(p)); }

U Key(const void* p) { return Low32(p); }

U AddressOf(const void* p) { return Key(p); }

// jmp / call [table + 4 * index]: the table's `entries` handlers, read in place
// (the fuzz swaps the cells for recorders); a Fatal past them, where the
// original goes through the dword after - the next kind's table or data.
Handler Entry(U table, unsigned index, unsigned entries, const char* who) {
    if (index >= entries)
        bof3::Fatal("%s: index %u past the %u entries of the table at 0x%X (the original jumps through 0x%X)", who,
                    index, entries, (unsigned)table, (unsigned)(table + 4u * index));
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(At(table + 4u * index))));
}

// (trig * radius) >> 4 + base, as the rings and the specks place a point: imul
// of the whole answer by the s16 radius, sar 4, add.
U Arm(int trig, short radius, U base) {
    return static_cast<U>(static_cast<std::int32_t>(static_cast<U>(trig) * static_cast<U>(static_cast<int>(radius))) >> 4) +
           base;
}

// The draw mode both rings put before each quad: Gpu_GetTPage(0, 1, 0x3C0, 0)
// (its fifth pushed zero left on the stack becomes the mode's texture window).
void RingMode(int dtd) {
    const unsigned tpage = SH_CALL(Gpu_GetTPage)(0, 1, 0x3C0, 0) & 0xFFFFu;
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, dtd, tpage, 0);
}

// The two projections of a ring's point: at the height +0x3C (out1), then at
// +0x3C + (+0x30 << 16) (out2), the point (x, z) at angle `angle` and radius
// +0x2E round (+0x34, +0x38). Sprite_Current read after each call, as the
// originals read it.
void RingPoints(U angle, long* point, float* out1, float* out2) {
    const int c = SH_CALL(Math_Cos)(static_cast<int>(angle));
    const unsigned char* s = Cur();
    point[0] = static_cast<long>(Arm(c, static_cast<std::int16_t>(rv::As<records::EffectWork>(s)->work_2e), static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->x)));
    const int n = SH_CALL(Math_Sin)(static_cast<int>(angle));
    s = Cur();
    point[1] = static_cast<long>(Arm(n, static_cast<std::int16_t>(rv::As<records::EffectWork>(s)->work_2e), static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->z)));
    point[2] = rv::As<records::EffectObject>(s)->height;
    SH_CALL(EffectGte_ProjectPoint)(point, out1);
    s = Cur();
    point[2] = static_cast<long>((static_cast<U>(static_cast<int>(SW(s + 0x30))) << 16) + static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->height));
    SH_CALL(EffectGte_ProjectPoint)(point, out2);
}

// A quad's four vertices of three dwords (x, y, depth: floats) at +8, +0x18,
// +0x28, +0x38: the previous projections, then the new ones.
void RingVertices(unsigned char* prim, const U* prev1, const U* prev2, const float* out1, const float* out2) {
    for (unsigned i = 0; i < 3; ++i) {
        SetUL(prim + 0x08 + 4 * i, prev1[i]);
        SetUL(prim + 0x18 + 4 * i, prev2[i]);
        std::memcpy(prim + 0x28 + 4 * i, &out1[i], 4);
        std::memcpy(prim + 0x38 + 4 * i, &out2[i], 4);
    }
}

void Shade(unsigned char* prim, U at1, U at2, unsigned char v) {
    prim[at1] = v;
    prim[at1 + 1] = v;
    prim[at1 + 2] = v;
    prim[at2] = v;
    prim[at2 + 1] = v;
    prim[at2 + 2] = v;
}

}  // namespace

// original 0x46DD50: a tail jump through EffectKind1D_States 0x654224 by +1.
extern "C" void __cdecl EffectKind1D_Run(void) { Entry(AddressOf(EffectKind1D_States), Cur()[1], 5, "EffectKind1D_Run")(); }

// original 0x46DD70 (EffectKind1D_States[0]): the ring's radius +0x2E 0x280
// and height +0x30 0x600 (the point where the spawner put it); +9 0; the
// specks cleared; +1 up.
extern "C" void __cdecl EffectKind1D_Start(void) {
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(Cur())->work_2e, 0x280);
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(Cur())->work_30, 0x600);
    Cur()[9] = 0;
    SH_CALL(EffectShards_Clear)();
    unsigned char* const s = Cur();
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x46DDA0 (EffectKind1D_States[1]): the ring at brightness +9, a
// speck spawned (EffectSpecks_Spawn) on odd frames, the specks moved; +9 up 4 -
// at 0, +9 0x20 and +1 up.
extern "C" void __cdecl EffectKind1D_Rise(void) {
    SH_CALL(EffectKind1D_DrawRing)(Cur()[9]);
    if (Frame_Counter & 1) SH_AT(unsigned char (__cdecl*)(void), at::kShardSpawn.v)();
    SH_CALL(EffectKind1D_MoveShards)();
    unsigned char* s = Cur();
    s[9] = static_cast<unsigned char>(s[9] + 4);
    s = Cur();
    if (s[9] == 0) {
        s[9] = 0x20;
        s = Cur();
        s[1] = static_cast<unsigned char>(s[1] + 1);
    }
}

// original 0x46DDF0 (EffectKind1D_States[2]): the ring at full brightness, a
// speck on odd frames, the specks moved; held until the counter byte 0x903848
// is 0x1F - then +9 0xFC and +1 up.
extern "C" void __cdecl EffectKind1D_Hold(void) {
    SH_CALL(EffectKind1D_DrawRing)(0xFF);
    if (Frame_Counter & 1) SH_AT(unsigned char (__cdecl*)(void), at::kShardSpawn.v)();
    SH_CALL(EffectKind1D_MoveShards)();
    if (At(::bof3::Img(0x903848))[0] == 0x1F) {
        Cur()[9] = 0xFC;
        unsigned char* const s = Cur();
        s[1] = static_cast<unsigned char>(s[1] + 1);
    }
}

// original 0x46DE30 (EffectKind1D_States[3]): the ring at brightness +9, the
// specks moved; +9 down 4 - at 0, +1 up.
extern "C" void __cdecl EffectKind1D_Fade(void) {
    SH_CALL(EffectKind1D_DrawRing)(Cur()[9]);
    SH_CALL(EffectKind1D_MoveShards)();
    unsigned char* s = Cur();
    s[9] = static_cast<unsigned char>(s[9] + 0xFC);
    s = Cur();
    if (s[9] == 0) s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x46DE70 (EffectKind1D_States[4]): the specks moved; none in use,
// Effect_Release (a tail jump).
extern "C" void __cdecl EffectKind1D_End(void) {
    if (!SH_CALL(EffectKind1D_MoveShards)()) SH_CALL(Effect_Release)();
}

// original 0x46DE80: kind 0x1C's ring with 32 steps of 0x80: the draw mode
// (dtd 1) and the quad committed at slot 1 (Gfx_CommitPrim, 0xC and 0x44)
// only for steps below 0xB or from 0x1C - the others are built at the same
// Gfx_PacketNext and overwritten, a gap in the ring; the shade down 8 for steps
// 8..0x17, up 8 for the others. No second draw mode.
extern "C" void __cdecl EffectKind1D_DrawRing(unsigned brightness) {
    SH_CALL(EffectGte_LoadMapCamera)();
    long point[3];
    float out1[3], out2[3];
    RingPoints(0, point, out1, out2);
    unsigned char shade = static_cast<unsigned char>(((Frame_Counter & 1) + 0x10) << 2);
    const U bright = brightness & 0xFFu;
    U angle = 0;
    for (unsigned k = 0; k < 0x20; ++k) {
        U prev1[3], prev2[3];
        std::memcpy(prev1, out1, sizeof prev1);
        std::memcpy(prev2, out2, sizeof prev2);
        angle += 0x80;
        RingPoints(angle & 0xFFFF, point, out1, out2);
        RingMode(1);
        const bool drawn = k < 0xB || k >= 0x1C;
        if (drawn) SH_CALL(Gfx_CommitPrim)(1, 0xC);
        unsigned char* const prim = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG4)(prim);
        SH_CALL(Gpu_SetSemiTrans)(prim, 1);
        RingVertices(prim, prev1, prev2, out1, out2);
        Shade(prim, 4, 0x14, static_cast<unsigned char>((shade * bright) >> 8));
        shade = static_cast<unsigned char>(k >= 8 && k < 0x18 ? shade - 8 : shade + 8);
        Shade(prim, 0x24, 0x34, static_cast<unsigned char>((shade * bright) >> 8));
        if (drawn) SH_CALL(Gfx_CommitPrim)(1, 0x44);
    }
}

void EffectKind1d_Inject() {
    BOF3_INJECT(EffectKind1D_Run);
    BOF3_INJECT(EffectKind1D_Start);
    BOF3_INJECT(EffectKind1D_Rise);
    BOF3_INJECT(EffectKind1D_Hold);
    BOF3_INJECT(EffectKind1D_Fade);
    BOF3_INJECT(EffectKind1D_End);
    BOF3_INJECT(EffectKind1D_DrawRing);
}

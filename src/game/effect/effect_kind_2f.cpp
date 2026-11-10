// Moved here by tools/move_funcs.py from effect_2b.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_2f.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
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

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

using U = std::uint32_t;

using Handler = void (__cdecl*)();

unsigned char* Cur() { return Sprite_Current; }

U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }

void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }

U Key(const void* p) { return Low32(p); }

U AddressOf(const void* p) { return Key(p); }

void Copy12(unsigned char* to, const void* from) { std::memcpy(to, from, 12); }

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

// The byte a G2 line's end and a spiral's quad take: a word clamped to 0..0xFF
// as the originals clamp it (a 16-bit compare).
unsigned char ClampUnsigned(U v) { return static_cast<unsigned char>((v & 0xFFFFu) > 0xFFu ? 0xFFu : v); }

// Kind 0x2F's head: the point at the record's angle word (zero-extended) and
// radius +0x10 round its centre (+0, +4), the height +8 << 8; the map camera
// loaded; projected into the first of the trail's points (+0x18).
void TrailHead(unsigned char* r) {
    long point[3];
    const int c = SH_CALL(Math_Cos)(static_cast<int>(Word(r + 0x14)));
    point[0] = static_cast<long>(static_cast<U>(MulSar(c, UL(r + 0x10), 12)) + UL(r + 0));
    const int n = SH_CALL(Math_Sin)(static_cast<int>(Word(r + 0x14)));
    point[1] = static_cast<long>(static_cast<U>(MulSar(n, UL(r + 0x10), 12)) + UL(r + 4));
    point[2] = static_cast<long>(UL(r + 8) << 8);
    SH_CALL(EffectGte_LoadMapCamera)();
    SH_CALL(EffectGte_ProjectPoint)(point, reinterpret_cast<float*>(r + 0x18));
}

}  // namespace

// original 0x4731A0 (Effect_KindHandlers[0x2F], hidden in E2A's 0x473100's
// recorded extent): a tail jump through EffectKind2F_States 0x6543D8 by +1.
extern "C" void __cdecl EffectKind2F_Run(void) {
    Entry(AddressOf(EffectKind2F_States), Cur()[1], 2, "EffectKind2F_Run")();
}

// original 0x4731C0 (EffectKind2F_States[0]): the three trail records at
// EffectKind30_Shards (0x198 apart): centre (0x20000, 0xC0000), height
// 0xB0000, radius 0x20000, the angle word 0x1000 * i / 3; each trail set up
// (EffectKind2F_InitTrail); Sound_PlayEffect(0x206); +9 0x40, +1 up.
extern "C" void __cdecl EffectKind2F_Start(void) {
    for (U i = 0; i < at::kTrailCount; ++i) {
        unsigned char* const r = At(::bof3::Img(EffectKind30_Shards_addr) + at::kTrailStride * i);
        SetUL(r + 0, 0x20000);
        SetUL(r + 4, 0xC0000);
        SetUL(r + 8, 0xB0000);
        SetUL(r + 0x10, 0x20000);
        SetWord(r + 0x14, 0x1000u * i / 3u);
        SH_CALL(EffectKind2F_InitTrail)(r);
    }
    SH_CALL(Sound_PlayEffect)(0x206);
    Cur()[9] = 0x40;
    unsigned char* const s = Cur();
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x473240 (EffectKind2F_States[1]): each trail four times: the
// height down 0x400, the radius down 0x200, the angle word up 0x40, the head
// moved (EffectKind2F_StepTrail); then drawn (EffectKind2F_DrawTrail). +9 down
// 1 - at 0, Effect_Release.
extern "C" void __cdecl EffectKind2F_Spin(void) {
    for (U i = 0; i < at::kTrailCount; ++i) {
        unsigned char* const r = At(::bof3::Img(EffectKind30_Shards_addr) + at::kTrailStride * i);
        for (unsigned k = 0; k < 4; ++k) {
            const U height = UL(r + 8), radius = UL(r + 0x10);
            SetWord(r + 0x14, Word(r + 0x14) + 0x40u);
            SetUL(r + 8, height - 0x400u);
            SetUL(r + 0x10, radius - 0x200u);
            SH_CALL(EffectKind2F_StepTrail)(r);
        }
        SH_CALL(EffectKind2F_DrawTrail)(r);
    }
    unsigned char* const s = Cur();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    if (Cur()[9] == 0) SH_CALL(Effect_Release)();
}

// original 0x4732D0: a trail's 32 points (three floats from +0x18) moved down
// one, from the last (dword by dword); the head projected into the first
// (TrailHead).
extern "C" void __cdecl EffectKind2F_StepTrail(unsigned char* trail) {
    for (U k = 31; k >= 1; --k) {
        unsigned char* const to = trail + 0x18 + 0xC * k;
        const unsigned char* const from = to - 0xC;
        SetUL(to, UL(from));
        SetUL(to + 4, UL(from + 4));
        SetUL(to + 8, UL(from + 8));
    }
    TrailHead(trail);
}

// original 0x473360: a draw mode (the page at (0x380, 0x100), abr 1, dtd 0)
// committed at slot 1; then 31 semi-transparent G2 lines at Gfx_PacketNext,
// point j to point j + 1 (floats as they are), red from 0xFF (the first end
// (0x1F00 - 0x100 j + 0x100) / 32, the second (0x1F00 - 0x100 j) / 32, each
// clamped to 0xFF), green and blue 0; each committed at slot 1 (0x24 bytes).
extern "C" void __cdecl EffectKind2F_DrawTrail(unsigned char* trail) {
    Mode(1, 0x380, 0x100, 0);
    SH_CALL(Gfx_CommitPrim)(1, 0xC);
    std::int32_t shade = 0x1F00;
    for (U j = 0; j < 31; ++j) {
        unsigned char* const prim = Gfx_PacketNext;
        SH_CALL(Gpu_SetLineG2)(prim);
        SH_CALL(Gpu_SetSemiTrans)(prim, 1);
        const unsigned char* const e = trail + 0x18 + 0xC * j;
        Copy12(prim + 8, e);
        Copy12(prim + 0x18, e + 0xC);
        prim[4] = ClampUnsigned(static_cast<U>((shade + 0x100) / 32));
        prim[5] = 0;
        prim[6] = 0;
        prim[0x14] = ClampUnsigned(static_cast<U>(shade / 32));
        prim[0x15] = 0;
        prim[0x16] = 0;
        SH_CALL(Gfx_CommitPrim)(1, 0x24);
        shade -= 0x100;
    }
}

// original 0x473460: the head projected into the first point (TrailHead), then
// copied into the other 31.
extern "C" void __cdecl EffectKind2F_InitTrail(unsigned char* trail) {
    TrailHead(trail);
    for (U k = 1; k < 32; ++k) {
        unsigned char* const to = trail + 0x18 + 0xC * k;
        SetUL(to, UL(trail + 0x18));
        SetUL(to + 4, UL(trail + 0x1C));
        SetUL(to + 8, UL(trail + 0x20));
    }
}

void EffectKind2f_Inject() {
    BOF3_INJECT(EffectKind2F_Run);
    BOF3_INJECT(EffectKind2F_Start);
    BOF3_INJECT(EffectKind2F_Spin);
    BOF3_INJECT(EffectKind2F_StepTrail);
    BOF3_INJECT(EffectKind2F_DrawTrail);
    BOF3_INJECT(EffectKind2F_InitTrail);
}

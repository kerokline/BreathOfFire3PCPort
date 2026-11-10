// Moved here by tools/move_funcs.py from effect_2b.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_38.h"

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

using U = std::uint32_t;

using Handler = void (__cdecl*)();

unsigned char* Cur() { return Sprite_Current; }

U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }

void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }

U Key(const void* p) { return Low32(p); }

U AddressOf(const void* p) { return Key(p); }

void StoreFloat(unsigned char* at, double v) {
    const auto f = static_cast<float>(v);
    std::memcpy(at, &f, sizeof f);
}

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

// The tile's three colour bytes stepped by `by` (the record read for each).
static void TintStep(unsigned char* s, unsigned char by) {
    s[0x5D] = static_cast<unsigned char>(s[0x5D] + by);
    s = Cur();
    s[0x5E] = static_cast<unsigned char>(s[0x5E] + by);
    s = Cur();
    s[0x5F] = static_cast<unsigned char>(s[0x5F] + by);
}

// The fan at the record's screen point (+0x2E, +0x30) with the radius +9 (each
// pushed with the upper half the register held: only the words are read).
static void Kind39Disc() {
    const unsigned char* const s = Cur();
    SH_CALL(EffectKind39_DrawDisc)(rv::As<records::EffectWork>(s)->work_2e, rv::As<records::EffectWork>(s)->work_30, s[9]);
}

// A screen fan's rim point: (trig * radius >> 12) + the centre as a dword, then
// a float (fild of it).
static float Rim(int trig, U radius, int centre) {
    return static_cast<float>(static_cast<std::int32_t>(static_cast<U>(MulSar(trig, radius, 12)) + static_cast<U>(centre)));
}

// original 0x473DD0 (Effect_KindHandlers[0x38], hidden in 0x473CE0's recorded
// extent): a tail jump through EffectKind38_States 0x654424 by +1.
extern "C" void __cdecl EffectKind38_Run(void) {
    Entry(AddressOf(EffectKind38_States), Cur()[1], 5, "EffectKind38_Run")();
}

// original 0x473DF0 (EffectKind38_States[0]): +9 0x10, +1 1, the tile's
// colour +0x5D..+0x5F 0.
extern "C" void __cdecl EffectKind38_Start(void) {
    Cur()[9] = 0x10;
    Cur()[1] = 1;
    Cur()[0x5F] = 0;
    Cur()[0x5E] = 0;
    Cur()[0x5D] = 0;
}

// original 0x473E30 (EffectKind38_States[1]): +9 down 1 - not yet 0, the tile
// brighter by 6; at 0, message 0x1F opened (Msg_OpenScript), Field_Request 2,
// +1 2. The tile drawn (a tail jump to EffectKind38_DrawTint).
extern "C" void __cdecl EffectKind38_FadeIn(void) {
    unsigned char* s = Cur();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    s = Cur();
    if (s[9] != 0) {
        TintStep(s, 6);
    } else {
        SH_CALL(Msg_OpenScript)(0x1F);
        unsigned char* const t = Cur();
        Field_Request = 2;
        t[1] = 2;
    }
    SH_CALL(EffectKind38_DrawTint)();
}

// original 0x473E90 (EffectKind38_States[2]): Field_Request no longer 2 (the
// message closed), +9 0x10 and +1 3. The tile drawn (a tail jump).
extern "C" void __cdecl EffectKind38_WaitMessage(void) {
    if (Field_Request != 2) {
        Cur()[9] = 0x10;
        Cur()[1] = 3;
    }
    SH_CALL(EffectKind38_DrawTint)();
}

// original 0x473EC0 (EffectKind38_States[3]): +9 down 1 - not yet 0, the tile
// dimmer by 6; at 0, +1 4 (Effect_StateRelease). The tile drawn (0x473F10 is
// this function's own tail: run here in place, not through the recorder).
extern "C" void __cdecl EffectKind38_FadeOut(void) {
    unsigned char* s = Cur();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    s = Cur();
    if (s[9] != 0)
        TintStep(s, 0xFA);
    else
        s[1] = 4;
    EffectKind38_DrawTint();
}

// original 0x473F10 (the tail of 0x473EC0, which 0x473E30 and 0x473E90 jump
// to; no list had it): a draw mode (page word 0x4F, dtd 1) committed at slot
// 2; a semi-transparent TILE at Gfx_PacketNext (read after the commit),
// untextured, at (0, 0) and 320.0 x 240.0 (floats), coloured +0x5D, +0x5E,
// +0x5F (Sprite_Current read for each); committed at slot 2 (0x1C bytes).
extern "C" void __cdecl EffectKind38_DrawTint(void) {
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x4F, 0);
    SH_CALL(Gfx_CommitPrim)(2, 0xC);
    unsigned char* const prim = Gfx_PacketNext;
    SH_CALL(Gpu_SetTile)(prim);
    prim[4] = Cur()[0x5D];
    prim[5] = Cur()[0x5E];
    const unsigned char blue = Cur()[0x5F];
    SetUL(prim + 8, std::bit_cast<std::uint32_t>(Widescreen_FillX()));   // DIV-0041: (-53, 0) 426 wide under the wide picture
    prim[6] = blue;
    SetUL(prim + 0xC, 0);
    SetUL(prim + 0x14, std::bit_cast<std::uint32_t>(Widescreen_FillWidth()));   // 0x43A00000, 320.0f narrow
    SetUL(prim + 0x18, 0x43700000u);   // 240.0f
    SH_CALL(Gpu_SetSemiTrans)(prim, 1);
    SH_CALL(Gpu_SetShadeTex)(prim, 0);
    SH_CALL(Gfx_CommitPrim)(2, 0x1C);
}

// original 0x473FA0 (Effect_KindHandlers[0x39], hidden in 0x473CE0's recorded
// extent): a tail jump through EffectKind39_States 0x654438 by +1.
extern "C" void __cdecl EffectKind39_Run(void) {
    Entry(AddressOf(EffectKind39_States), Cur()[1], 4, "EffectKind39_Run")();
}

// original 0x473FC0 (EffectKind39_States[0]): +9 0, +1 up;
// Sound_PlayEffect(0x20E).
extern "C" void __cdecl EffectKind39_Start(void) {
    Cur()[9] = 0;
    unsigned char* const s = Cur();
    s[1] = static_cast<unsigned char>(s[1] + 1);
    SH_CALL(Sound_PlayEffect)(0x20E);
}

// original 0x473FE0 (EffectKind39_States[1]): the fan drawn; +9 up 1 - past
// 0x1E, +9 0x1E and +1 up.
extern "C" void __cdecl EffectKind39_Grow(void) {
    Kind39Disc();
    unsigned char* s = Cur();
    s[9] = static_cast<unsigned char>(s[9] + 1);
    s = Cur();
    if (s[9] > 0x1E) {
        s[9] = 0x1E;
        s = Cur();
        s[1] = static_cast<unsigned char>(s[1] + 1);
    }
}

// original 0x474030 (EffectKind39_States[2]): the fan drawn; +9 down 1 - at
// 0, +1 up (Effect_StateRelease).
extern "C" void __cdecl EffectKind39_Shrink(void) {
    Kind39Disc();
    unsigned char* s = Cur();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    s = Cur();
    if (s[9] == 0) s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x474070: a draw mode (the page at (0x3C0, 0), abr 1, dtd 1)
// committed at slot 1; 32 semi-transparent G3 triangles at Gfx_PacketNext
// fanned round the screen point (x, y) (s16 each, the centre as floats), the
// rim points at `radius` (s16) and the angles 0x80 k (whole) and 0x80 (k + 1)
// (its low 16 bits), each (trig * radius >> 12) + the centre as an int then a
// float; the centre grey 0x80, the rim black; no depth written; each committed
// at slot 1 (0x34 bytes).
extern "C" void __cdecl EffectKind39_DrawDisc(int x, int y, int radius) {
    Mode(1, 0x3C0, 0, 1);
    SH_CALL(Gfx_CommitPrim)(1, 0xC);
    const int cx = static_cast<short>(x), cy = static_cast<short>(y);
    const U r = static_cast<U>(static_cast<int>(static_cast<short>(radius)));
    const auto fx = static_cast<float>(cx);
    const auto fy = static_cast<float>(cy);
    U a1 = 0, next = 0x80;
    do {
        unsigned char* const prim = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG3)(prim);
        SH_CALL(Gpu_SetSemiTrans)(prim, 1);
        std::memcpy(prim + 8, &fx, 4);
        std::memcpy(prim + 0xC, &fy, 4);
        StoreFloat(prim + 0x18, Rim(SH_CALL(Math_Cos)(static_cast<int>(a1)), r, cx));
        StoreFloat(prim + 0x1C, Rim(SH_CALL(Math_Sin)(static_cast<int>(a1)), r, cy));
        const U a2 = next & 0xFFFFu;
        StoreFloat(prim + 0x28, Rim(SH_CALL(Math_Cos)(static_cast<int>(a2)), r, cx));
        StoreFloat(prim + 0x2C, Rim(SH_CALL(Math_Sin)(static_cast<int>(a2)), r, cy));
        prim[4] = prim[5] = prim[6] = 0x80;
        prim[0x14] = prim[0x15] = prim[0x16] = 0;
        prim[0x24] = prim[0x25] = prim[0x26] = 0;
        SH_CALL(Gfx_CommitPrim)(1, 0x34);
        next += 0x80;
        a1 += 0x80;
    } while (((next - 0x80u) & 0xFFFFu) < 0x1000u);
}

void EffectKind38_Inject() {
    BOF3_INJECT(EffectKind38_Run);
    BOF3_INJECT(EffectKind38_Start);
    BOF3_INJECT(EffectKind38_FadeIn);
    BOF3_INJECT(EffectKind38_WaitMessage);
    BOF3_INJECT(EffectKind38_FadeOut);
    BOF3_INJECT(EffectKind38_DrawTint);
    BOF3_INJECT(EffectKind39_Run);
    BOF3_INJECT(EffectKind39_Start);
    BOF3_INJECT(EffectKind39_Grow);
    BOF3_INJECT(EffectKind39_Shrink);
    BOF3_INJECT(EffectKind39_DrawDisc);
}

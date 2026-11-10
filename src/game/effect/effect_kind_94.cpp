// Moved here by tools/move_funcs.py from effect_4d.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_94.h"

#include <bit>
#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/effect_4d_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_4d::at;

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

// mov eax, [S]; mov dl, [eax + 9]; dec dl; mov [eax + 9], dl; mov eax, [S];
// test [eax + 9]: +9 down one; answers whether it is now 0.
bool CountDown() {
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    s = S();
    return s[9] == 0;
}

// The draw mode most of these put first: Gpu_GetTPage(0, abr, x, y) and
// Gpu_SetDrawMode at the cursor (read after the page), dtd 1 - its fifth
// argument is the first of the page's five pushes, a zero, left on the stack.
void DrawMode(unsigned abr, int x, int y) {
    const unsigned tpage = SH_CALL(Gpu_GetTPage)(0, abr, x, y) & 0xFFFFu;
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0);
}

// The full-screen TILE 0x48CA90 and 0x48CC90 build after their draw mode: at
// the cursor (read after the commit), its colour Sprite_Current's +0x5D, +0x5E,
// +0x5F (read for each), at (0, 0) 320.0 x 240.0 - DIV-0041's fill - semi-
// transparent, untextured, committed at slot 5 (0x1C bytes).
void TintTile() {
    unsigned char* const prim = Gfx_PacketNext;
    SH_CALL(Gpu_SetTile)(prim);
    prim[4] = S()[0x5D];
    prim[5] = S()[0x5E];
    prim[6] = S()[0x5F];
    SetUL(prim + 8, std::bit_cast<std::uint32_t>(Widescreen_FillX()));   // DIV-0041: (-53, 0) 426 wide under the wide picture
    SetUL(prim + 0xC, 0);
    SetUL(prim + 0x14, std::bit_cast<std::uint32_t>(Widescreen_FillWidth()));   // 0x43A00000, 320.0f narrow
    SetUL(prim + 0x18, 0x43700000u);   // 240.0f
    SH_CALL(Gpu_SetSemiTrans)(prim, 1);
    SH_CALL(Gpu_SetShadeTex)(prim, 0);
    SH_CALL(Gfx_CommitPrim)(5, 0x1C);
}

}  // namespace

// original 0x48CB30 (Effect_KindHandlers[0x94], hidden in 0x48CA90):
// jmp [EffectKind94_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind94_Run(void) {
    Dispatch("EffectKind94_Run", AddressOf(EffectKind94_States), EffectKind94_States_count);
}

// original 0x48CB50 (state 0): the colour 0; +9 = 0x5A; +1 = 1.
extern "C" void __cdecl EffectKind94_Start(void) {
    S()[0x5F] = 0;
    S()[0x5E] = 0;
    S()[0x5D] = 0;
    S()[9] = 0x5A;
    S()[1] = 1;
}

// original 0x48CB90 (state 1): +9 down (Sprite_Current read again for the
// test, and kept); not 0: each colour byte 0xFF - (+9 * 0xFF / 0x5A) (the
// quotient's low byte: a fade in), +0x5F through the kept pointer, +0x5E and
// +0x5D read afresh; at 0: +1 = 2. Then the tint at blend 2 and the redraw.
extern "C" void __cdecl EffectKind94_Fade(void) {
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    unsigned char* const kept = S();
    const unsigned v = kept[9];
    if (v != 0) {
        const auto level = static_cast<unsigned char>(0xFFu - ((v * 0xFFu) / 0x5Au));
        kept[0x5F] = level;
        S()[0x5E] = level;
        S()[0x5D] = level;
    } else {
        kept[1] = 2;
    }
    SH_CALL(Effect_DrawScreenTintMode)(2);
    SH_CALL(EffectKind94_RedrawSprites)();
}

// original 0x48CC10 (state 2): the tint at blend 2; a tail jump to the redraw.
extern "C" void __cdecl EffectKind94_Hold(void) {
    SH_CALL(Effect_DrawScreenTintMode)(2);
    SH_CALL(EffectKind94_RedrawSprites)();
}

// original 0x48CC20 (state 3, where Scena14_Run6 spawns it): the colour 0xFF
// (white), the tint at blend 1; +9 = 4, +1 = 4.
extern "C" void __cdecl EffectKind94_Flash(void) {
    S()[0x5F] = 0xFF;
    S()[0x5E] = 0xFF;
    S()[0x5D] = 0xFF;
    SH_CALL(Effect_DrawScreenTintMode)(1);
    S()[9] = 4;
    S()[1] = 4;
}

// original 0x48CC60 (state 4): +9 down; at 0 Effect_Release; the tint at
// blend 1 either way.
extern "C" void __cdecl EffectKind94_FlashOut(void) {
    if (CountDown()) SH_CALL(Effect_Release)();
    SH_CALL(Effect_DrawScreenTintMode)(1);
}

// original 0x48CC90 (blend): 0x48CA90 with the page's blend mode the argument's
// low byte (and eax, 0xFF).
extern "C" void __cdecl Effect_DrawScreenTintMode(unsigned blend) {
    DrawMode(blend & 0xFFu, 0x3C0, 0);
    SH_CALL(Gfx_CommitPrim)(5, 0xC);
    TintTile();
}

// original 0x48CD40: Sprite_Current kept; the first six Sprite_Objects records
// each made current, +0x29 = 2 and drawn again (Sprite_UpdateScreen) - over
// the tint; then for each of Field_MemberCount (read again after each draw)
// party members whose ObjTrio +0x89 is 7, the same; Sprite_Current put back.
// A member index past ObjTrio's three records aborts (docs/effect_4d.md
// section 6).
extern "C" void __cdecl EffectKind94_RedrawSprites(void) {
    unsigned char* const saved = Sprite_Current;
    unsigned char* o = rv::Bytes(Sprite_Objects);
    for (unsigned n = 6; n != 0; --n, o += 0xA4) {
        Sprite_Current = o;
        o[0x29] = 2;
        SH_CALL(Sprite_UpdateScreen)();
    }
    for (unsigned m = 0; m < Field_MemberCount; m = (m + 1) & 0xFFu) {
        if (m >= at::kTrioCount)
            bof3::Fatal("EffectKind94_RedrawSprites: Field_MemberCount is %u, past ObjTrio's three records - the original "
                        "reads and draws what follows (docs/effect_4d.md section 6)",
                        (unsigned)Field_MemberCount);
        unsigned char* const member = rv::Bytes(&ObjTrio[m]);
        if (member[at::kTrioBusy] == 7) {
            Sprite_Current = member;
            member[0x29] = 2;
            SH_CALL(Sprite_UpdateScreen)();
        }
    }
    Sprite_Current = saved;
}

void EffectKind94_Inject() {
    BOF3_INJECT(EffectKind94_Run);
    BOF3_INJECT(EffectKind94_Start);
    BOF3_INJECT(EffectKind94_Fade);
    BOF3_INJECT(EffectKind94_Hold);
    BOF3_INJECT(EffectKind94_Flash);
    BOF3_INJECT(EffectKind94_FlashOut);
    BOF3_INJECT(Effect_DrawScreenTintMode);
    BOF3_INJECT(EffectKind94_RedrawSprites);
}

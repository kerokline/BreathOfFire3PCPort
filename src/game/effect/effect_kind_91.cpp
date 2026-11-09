// Moved here by tools/move_funcs.py from effect_4d.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_91.h"

#include <bit>
#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/effect_4d.h"
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

// original 0x48C990 (Effect_KindHandlers[0x91], hidden in E4C's 0x48C7F0):
// jmp [EffectKind91_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind91_Run(void) {
    Dispatch("EffectKind91_Run", AddressOf(EffectKind91_States), EffectKind91_States_count);
}

// original 0x48C9B0 (state 0): the tint's colour +0x5F, +0x5E, +0x5D 0; +9 =
// 0x3C; +1 = 1.
extern "C" void __cdecl EffectKind91_Start(void) {
    S()[0x5F] = 0;
    S()[0x5E] = 0;
    S()[0x5D] = 0;
    S()[9] = 0x3C;
    S()[1] = 1;
}

// original 0x48C9F0 (state 1): +9 down; not 0: each colour byte up 1; at 0:
// Msg_OpenScript(0x86), Field_Request 2, +1 = 2 (Sprite_Current read after the
// call). Then a tail jump to the tint.
extern "C" void __cdecl EffectKind91_Brighten(void) {
    if (!CountDown()) {
        unsigned char* s = S();
        s[0x5D] = static_cast<unsigned char>(s[0x5D] + 1);
        s = S();
        s[0x5E] = static_cast<unsigned char>(s[0x5E] + 1);
        s = S();
        s[0x5F] = static_cast<unsigned char>(s[0x5F] + 1);
    } else {
        SH_CALL(Msg_OpenScript)(0x86);
        unsigned char* const s = S();
        Field_Request = 2;
        s[1] = 2;
    }
    SH_CALL(Effect_DrawScreenTint)();
}

// original 0x48CA50 (state 2): once Field_Request is not 2 (the message
// closed), the chapter's step byte 0x8034E5 up 1 and +1 = 3. Then a tail jump
// to the tint.
extern "C" void __cdecl EffectKind91_WaitMessage(void) {
    if (Field_Request != 2) {
        MoveScript_PartyRecords[0x65] = static_cast<unsigned char>(MoveScript_PartyRecords[0x65] + 1);
        S()[1] = 3;
    }
    SH_CALL(Effect_DrawScreenTint)();
}

// original 0x48CA80 (state 3): a tail jump to the tint.
extern "C" void __cdecl EffectKind91_Hold(void) { SH_CALL(Effect_DrawScreenTint)(); }

// original 0x48CA90 (E3B's kScreenTint, E3C's kScreenTile): a draw mode (page
// (0x3C0, 0), blend 2, dtd 1) committed at slot 5 (0xC); a full-screen semi-
// transparent TILE coloured Sprite_Current's +0x5D..+0x5F, committed at slot 5.
extern "C" void __cdecl Effect_DrawScreenTint(void) {
    DrawMode(2, 0x3C0, 0);
    SH_CALL(Gfx_CommitPrim)(5, 0xC);
    TintTile();
}

void EffectKind91_Inject() {
    BOF3_INJECT(EffectKind91_Run);
    BOF3_INJECT(EffectKind91_Start);
    BOF3_INJECT(EffectKind91_Brighten);
    BOF3_INJECT(EffectKind91_WaitMessage);
    BOF3_INJECT(EffectKind91_Hold);
    BOF3_INJECT(Effect_DrawScreenTint);
}

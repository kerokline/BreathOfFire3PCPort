// Moved here by tools/move_funcs.py from effect_6d.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_18_sub_65.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_6d.h"
#include "game/effect_6d_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_6d::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

using move_script::SetWord;

using move_script::Word;

using Handler = scenario_harness::Handler;

unsigned char* S() { return Sprite_Current; }

U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }

U UL(U a) { return UL(At(a)); }

std::int32_t I(U v) { return static_cast<std::int32_t>(v); }

U AddressOf(const void* p) { return Low32(p); }

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 2]; jmp [table + eax
// * 4]: the table's `entries` handlers read in place (the fuzz swaps the cells
// for recorders); a Fatal past them, where the original jumps through the dword
// after - another table or data.
void DispatchEffect6d(const char* who, U table, unsigned entries) {
    const unsigned sub = Sprite_Current[2];
    if (sub >= entries)
        bof3::Fatal("%s: sub-state byte +2 is %u, past the %u entries of 0x%X - the original jumps through the dword "
                    "after (docs/effect_6d.md section 7)",
                    who, sub, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(table + 4 * sub)))();
}

void StepUp() { S()[2] = static_cast<unsigned char>(S()[2] + 1); }

}  // namespace

// original 0x5169F0 (hidden in 0x5167C0): jmp [EffectKind18Sub65_States + +2 *
// 4], unbounded.
extern "C" void __cdecl EffectKind18Sub65_Run(void) {
    DispatchEffect6d("EffectKind18Sub65_Run", AddressOf(EffectKind18Sub65_States), EffectKind18Sub65_States_count);
}

// original 0x516A10 (sub-state 0): the rows black (level 0), +2 up.
extern "C" void __cdecl EffectKind18Sub65_Start(void) {
    SH_CALL(EffectKind18Sub65_ShadeCluts)(0);
    StepUp();
}

// original 0x516A30 (sub-state 1): the counter byte 0x903848 0xD: +2 up.
extern "C" void __cdecl EffectKind18Sub65_Wait(void) {
    if (At(::bof3::Img(at::kCounter48.v))[0] == 0xD) StepUp();
}

// original 0x516A50 (sub-state 2): +9 up; the rows at level +9; from 0x80 a tail
// jump to Effect_Release.
extern "C" void __cdecl EffectKind18Sub65_FadeIn(void) {
    S()[9] = static_cast<unsigned char>(S()[9] + 1);
    SH_CALL(EffectKind18Sub65_ShadeCluts)(S()[9]);
    if (S()[9] >= 0x80) SH_CALL(Effect_Release)();
}

// original 0x516A90 (cdecl (level); called by sub-kind 0x65's states 0 and 2):
// each of CLUT rows 4 and 5's 0x200 colours as loaded (Gfx_ClutStripSource +
// 0x800) with its three 5-bit channels times level / 128 (signed, toward zero)
// packed back unclamped (a channel past 31 runs into the next); 0 from a colour
// that was not becomes 0x8000; written to the live strip (Gfx_ClutStrip +
// 0x800). Gfx_ClutStripDirty 1.
extern "C" void __cdecl EffectKind18Sub65_ShadeCluts(int level) {
    const U l = static_cast<U>(level);
    for (U k = 0; k < at::kClutWords; ++k) {
        const U colour = Word(At(::bof3::Img(Gfx_ClutStripSource_addr) + 0x800 + 2 * k));
        U out = static_cast<U>(I(((colour >> 10) & 0x1F) * l) / 128);
        out = (out << 5) | static_cast<U>(I(((colour >> 5) & 0x1F) * l) / 128);
        out = (out << 5) | static_cast<U>(I((colour & 0x1F) * l) / 128);
        if (out == 0 && colour != 0) out = 0x8000;
        SetWord(At(::bof3::Img(Gfx_ClutStrip_addr) + 0x800 + 2 * k), out);
    }
    Gfx_ClutStripDirty = 1;
}

void EffectKind18Sub65_Inject() {
    BOF3_INJECT(EffectKind18Sub65_Run);
    BOF3_INJECT(EffectKind18Sub65_Start);
    BOF3_INJECT(EffectKind18Sub65_Wait);
    BOF3_INJECT(EffectKind18Sub65_FadeIn);
    BOF3_INJECT(EffectKind18Sub65_ShadeCluts);
}

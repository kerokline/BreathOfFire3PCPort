// Moved here by tools/move_funcs.py from effect_4f.cpp, rest_3g.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_ac.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/cheats.h"
#include "game/draw_pool.h"
#include "game/effect/effect_kind_ac_callees.h"
#include "game/effect_4f.h"
#include "game/effect_4f_callees.h"
#include "game/effect_gte.h"
#include "game/map_rim.h"
#include "game/move_script_bytes.h"
#include "game/psp_code.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_3g.h"
#include "game/rest_3g_callees.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/code_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_kind_ac::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

unsigned char* S() { return Sprite_Current; }

U ULEffect4f(const unsigned char* p) { return static_cast<U>(Long(p)); }

U ULEffect4f(U a) { return ULEffect4f(At(a)); }

U AddressOf(const void* p) { return Low32(p); }

using Handler = scenario_harness::Handler;

// The entry `state` of a kind's table (read in place: the fuzz swaps the cells
// for recorders); a Fatal past the table's own length, where the original jumps
// (or calls) through the dword after - the next kind's table.
Handler Entry(const char* who, U table, unsigned entries) {
    const unsigned state = S()[1];
    if (state >= entries)
        bof3::Fatal("%s: state byte +1 is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/effect_4f.md section 6)",
                    who, state, entries, (unsigned)table);
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(ULEffect4f(table + 4 * state)));
}

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 1]; jmp [table + eax * 4]
void DispatchEffect4f(const char* who, U table, unsigned entries) { Entry(who, table, entries)(); }

std::int32_t S8Rest3g(unsigned char b) { return static_cast<signed char>(b); }

}  // namespace

// original 0x4925A0 (hidden in 0x492400): jmp [EffectKindAC_States + +1 * 4],
// unbounded. Its three states are catalog part 6 rows (0x4925C0, 0x4925E0,
// 0x492620).
extern "C" void __cdecl EffectKindAC_Run(void) {
    DispatchEffect4f("EffectKindAC_Run", AddressOf(EffectKindAC_States), EffectKindAC_States_count);
}

// original 0x4925C0 (state 0; hidden in R3F's 0x492400): +9 = 8, +1 up.
extern "C" void __cdecl EffectKindAC_Start(void) {
    S()[9] = 8;
    S()[1] = static_cast<unsigned char>(S()[1] + 1);
}

// original 0x4925E0 (state 1): R3F's fill 0x492400 (EffectKindAA_DrawFill,
// ours, widened under DIV-0041 - so this fade is too) with the shade
// s8(+9) * -0x20 (imul cl: ax; the push carries Sprite_Current's upper half
// above it), +9 down; at 0 +9 = 8 and +1 up. +9 runs 8..1, so the shade 0x00,
// 0x20, .., 0xE0.
extern "C" void __cdecl EffectKindAC_FadeIn(void) {
    {
        const unsigned char* const s = S();
        const U product = static_cast<U>(S8Rest3g(s[9]) * -0x20) & 0xFFFFu;
        SH_AT(void(__cdecl*)(U), at::kFadeDraw.v)((AddressOf(s) & 0xFFFF0000u) | product);
    }
    S()[9] = static_cast<unsigned char>(S()[9] - 1);
    if (S()[9] != 0) return;
    S()[9] = 8;
    S()[1] = static_cast<unsigned char>(S()[1] + 1);
}

// original 0x492620 (state 2): the gradient with 0xFF while +9 is 8, else
// +9 << 5 (a byte, Sprite_Current's upper 24 bits above it); +9 down; at 0 a
// tail jump to Effect_Release.
extern "C" void __cdecl EffectKindAC_FadeOut(void) {
    {
        const unsigned char* const s = S();
        const unsigned char n = s[9];
        const U shade = n == 8 ? 0xFFu : (AddressOf(s) & 0xFFFFFF00u) | static_cast<unsigned char>(n << 5);
        SH_AT(void(__cdecl*)(U), at::kFadeDraw.v)(shade);
    }
    S()[9] = static_cast<unsigned char>(S()[9] - 1);
    if (S()[9] == 0) SH_CALL(Effect_Release)();
}

void EffectKindAc_Inject() {
    BOF3_INJECT(EffectKindAC_Run);
    BOF3_INJECT(EffectKindAC_Start);
    BOF3_INJECT(EffectKindAC_FadeIn);
    BOF3_INJECT(EffectKindAC_FadeOut);
}

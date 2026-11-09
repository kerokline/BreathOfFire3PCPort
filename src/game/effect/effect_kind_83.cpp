// Moved here by tools/move_funcs.py from effect_4a.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_83.h"

#include <cstdint>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/effect_4a_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_4a::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::Long;

using move_script::SetLong;

unsigned char* S() { return Sprite_Current; }

U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }

void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }

unsigned char* P(U a) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a)); }

// Sprite_Current pointed at `object` for `count` adds of `step` to its dword
// `offset`, then +1 `next` (and +9 `wait` when given) on the effect record and
// Sprite_Current put back, in the original's order.
void Nudge(unsigned char* object, unsigned offset, int count, U step, unsigned next, int wait) {
    unsigned char* const saved = Sprite_Current;
    Sprite_Current = object;
    for (int n = count; n != 0; --n) SetUL(Sprite_Current + offset, UL(Sprite_Current + offset) + step);
    saved[1] = static_cast<unsigned char>(next);
    Sprite_Current = saved;
    if (wait >= 0) saved[9] = static_cast<unsigned char>(wait);
}

// +9 down one; +1 `next` when it was 0.
void Hold(unsigned next) {
    unsigned char* const s = S();
    const unsigned char was = s[9];
    s[9] = static_cast<unsigned char>(was - 1);
    if (was == 0) S()[1] = static_cast<unsigned char>(next);
}

}  // namespace

// original 0x488690 (state 14 of kinds 0x82 and 0x83): +9 down; +1 0xF when it
// was 0.
extern "C" void __cdecl EffectKind83_Hold14(void) { Hold(0xF); }

// original 0x4886B0 (state 15): record 1's z - 0x4000 four times, +1 0x10, +9 8.
extern "C" void __cdecl EffectKind83_Nudge15(void) { Nudge(P(::bof3::Img(Sprite_Objects_addr) + 0xa4), 0x38, 4, 0xFFFFC000u, 0x10, 8); }

void EffectKind83_Inject() {
    BOF3_INJECT(EffectKind83_Hold14);
    BOF3_INJECT(EffectKind83_Nudge15);
}

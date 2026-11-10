// Moved here by tools/move_funcs.py from effect_4f.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_b1.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_4f_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_4f::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

using move_script::Word;

unsigned char* S() { return Sprite_Current; }

U ULEffect4f(const unsigned char* p) { return static_cast<U>(Long(p)); }

U ULEffect4f(U a) { return ULEffect4f(At(a)); }

U SW(const unsigned char* p) { return static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(Word(p)))); }

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

}  // namespace

// original 0x493430 (hidden in 0x493370): call [EffectKindB1_States + +1 * 4],
// unbounded; then MapView_Redraw = 2.
extern "C" void __cdecl EffectKindB1_Run(void) {
    Entry("EffectKindB1_Run", AddressOf(EffectKindB1_States), EffectKindB1_States_count)();
    MapView_Redraw = 2;
}

// original 0x493450 (state 0): CameraTurn_Angles = Camera_Angles (each s16 <<
// 16), CameraTurn_Steps' three 0; MapView_Redraw = +9 (the dispatcher sets 2
// after); +1 = 1.
extern "C" void __cdecl EffectKindB1_Start(void) {
    const U a0 = SW(reinterpret_cast<const unsigned char*>(Camera_Angles));
    const U a1 = SW(reinterpret_cast<const unsigned char*>(Camera_Angles + 1));
    const U a2 = SW(reinterpret_cast<const unsigned char*>(Camera_Angles + 2));
    CameraTurn_Angles[0] = static_cast<long>(a0 << 16);
    CameraTurn_Steps[0] = 0;
    CameraTurn_Steps[1] = 0;
    CameraTurn_Steps[2] = 0;
    CameraTurn_Angles[1] = static_cast<long>(a1 << 16);
    CameraTurn_Angles[2] = static_cast<long>(a2 << 16);
    unsigned char* const s = S();
    MapView_Redraw = s[9];
    s[1] = 1;
}

// original 0x4934B0 (state 1): Camera_Angles[1] up 4; MapView_SetElevation(it +
// 0xF0, a word); past 0xF0 (an s16, read again): Sound_PlayEffect(0x203) unless
// Field_Request is set, +1 = 2.
extern "C" void __cdecl EffectKindB1_Sway(void) {
    const U tilt = (static_cast<std::uint16_t>(Camera_Angles[1]) + 4u) & 0xFFFFu;
    Camera_Angles[1] = tilt;
    SH_CALL(MapView_SetElevation)(static_cast<int>((tilt + 0xF0u) & 0xFFFFu));
    if (Camera_Angles[1] <= 0xF0) return;
    if (Field_Request == 0) SH_CALL(Sound_PlayEffect)(0x203);
    S()[1] = 2;
}

// original 0x493500 (state 2): Camera_Angles[1] down 4; MapView_SetElevation(it
// + 0xF0, a word); below -0xF0 (an s16, read again): Sound_PlayEffect(0x203)
// unless Field_Request is set, +1 = 1.
extern "C" void __cdecl EffectKindB1_SwayBack(void) {
    const U tilt = (static_cast<std::uint16_t>(Camera_Angles[1]) + 0xFFFCu) & 0xFFFFu;
    Camera_Angles[1] = tilt;
    SH_CALL(MapView_SetElevation)(static_cast<int>((tilt + 0xF0u) & 0xFFFFu));
    if (Camera_Angles[1] >= -0xF0) return;
    if (Field_Request == 0) SH_CALL(Sound_PlayEffect)(0x203);
    S()[1] = 1;
}

void EffectKindB1_Inject() {
    BOF3_INJECT(EffectKindB1_Run);
    BOF3_INJECT(EffectKindB1_Start);
    BOF3_INJECT(EffectKindB1_Sway);
    BOF3_INJECT(EffectKindB1_SwayBack);
}

// Moved here by tools/move_funcs.py from effect_4f.cpp, rest_3f.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_ab.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect/effect_kind_ab_callees.h"
#include "game/effect_4f.h"
#include "game/effect_4f_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_3f.h"
#include "game/rest_3f_callees.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_kind_ab::at;

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

// +1 up, Sprite_Current read afresh.
void NextState() {
    unsigned char* const s = S();
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

}  // namespace

// original 0x492510 (hidden in 0x492400): jmp [EffectKindAB_States + +1 * 4],
// unbounded. States: EffectKindAB_Start, EffectKindAB_Drops (part 6: Draw_PassFlags 0 -
// +1 up; else EffectKindAB_Emit and a tail jmp to EffectKindAB_MoveDrops),
// Effect_StateRelease.
extern "C" void __cdecl EffectKindAB_Run(void) {
    DispatchEffect4f("EffectKindAB_Run", AddressOf(EffectKindAB_States), EffectKindAB_States_count);
}

// original 0x492530 (state 0; hidden in 0x492400, catalog part 6): the record's
// point +0x34 / +0x38 / +0x3C the leader's (ObjTrio +0x34..); the drops cleared
// (EffectKind81_ClearDrops), the eight sources placed; +9 = 0x78; +1 up.
extern "C" void __cdecl EffectKindAB_Start(void) {
    rv::Store<U>(rv::Current<records::EffectObject>()->x, static_cast<std::uint32_t>(ObjTrio[0].x));
    rv::Store<U>(rv::Current<records::EffectObject>()->z, static_cast<std::uint32_t>(ObjTrio[0].z));
    rv::Store<U>(rv::Current<records::EffectObject>()->height, static_cast<std::uint32_t>(ObjTrio[0].height));
    SH_CALL(EffectKind81_ClearDrops)();
    SH_CALL(EffectKindAB_PlaceSources)();
    S()[9] = 0x78;
    NextState();
}

// original 0x492580 (state 1, hidden in 0x492400): Draw_PassFlags 0 - +1 up;
// else EffectKindAB_Emit and a tail jmp to EffectKindAB_MoveDrops.
extern "C" void __cdecl EffectKindAB_Drops(void) {
    if (Draw_PassFlags == 0) {
        NextState();
        return;
    }
    SH_CALL(EffectKindAB_Emit)();
    SH_CALL(EffectKindAB_MoveDrops)();
}

void EffectKindAbFromEffect4f_Inject() {
    BOF3_INJECT(EffectKindAB_Run);
    BOF3_INJECT(EffectKindAB_Start);
}

void EffectKindAbFromRest3f_Inject() {
    BOF3_INJECT(EffectKindAB_Drops);
}

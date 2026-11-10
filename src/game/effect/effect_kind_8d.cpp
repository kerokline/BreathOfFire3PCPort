// Moved here by tools/move_funcs.py from effect_4c.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_8d.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_4c_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_4c::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

using Handler = scenario_harness::Handler;

using VoidFn = void (__cdecl*)();

unsigned char* S() { return Sprite_Current; }

U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }

U AddressOf(const void* p) { return Low32(p); }

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 1]; jmp [table +
// eax * 4]: the table's `entries` handlers read in place (the fuzz swaps the
// cells for recorders); a Fatal past them, where the original jumps through
// the dword after - the next kind's table or data.
void Dispatch(const char* who, U table, unsigned entries) {
    const unsigned state = Sprite_Current[1];
    if (state >= entries)
        bof3::Fatal("%s: state byte +1 is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/effect_4c.md section 6)",
                    who, state, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(At(table + 4 * state))))();
}

}  // namespace

// original 0x48B200 (Effect_KindHandlers[0x8D], hidden in E4B's 0x48A8E0):
// jmp [EffectKind8D_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind8D_Run(void) {
    Dispatch("EffectKind8D_Run", AddressOf(EffectKind8D_States), EffectKind8D_States_count);
}

// original 0x48B220 (state 0): Camera_Distance 0x1C80, MapView_Redraw 2, the
// byte +0x48 of the three party records (ObjTrio) 1; the tint +0x5F / +0x5E
// / +0x5D 0x7F (+0x5F through the record read first); +9 = 0x96, +1 = 1.
extern "C" void __cdecl EffectKind8D_Start(void) {
    unsigned char* const s = S();
    Camera_Distance = 0x1C80;
    MapView_Redraw = 2;
    for (unsigned i = 0; i < 3; ++i) At(::bof3::Img(ObjTrio_addr) + 0x48 + at::kObjStride * i)[0] = 1;
    s[0x5F] = 0x7F;
    S()[0x5E] = 0x7F;
    S()[0x5D] = 0x7F;
    S()[9] = 0x96;
    S()[1] = 1;
}

// original 0x48B280 (state 1): a tail jump to E4D's screen tint 0x48CA90 (the
// tint held; nothing here moves +1 on).
extern "C" void __cdecl EffectKind8D_Hold(void) { SH_AT(VoidFn, at::kScreenTile.v)(); }

// original 0x48B290 (state 2): +9 down; not 0: Camera_Distance - 0x2A and,
// while +0x5D is not 0, the tint +0x5D / +0x5E / +0x5F each down one (+0x5D
// through the record read after the count); at 0: Camera_Distance 0x400, +1 =
// 3. Then MapView_Redraw 2 and a tail jump to the tint.
extern "C" void __cdecl EffectKind8D_Pull(void) {
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    s = S();
    if (s[9] != 0) {
        Camera_Distance = static_cast<short>(Camera_Distance - 0x2A);
        const unsigned char red = s[0x5D];
        if (red != 0) {
            s[0x5D] = static_cast<unsigned char>(red - 1);
            unsigned char* t = S();
            t[0x5E] = static_cast<unsigned char>(t[0x5E] - 1);
            t = S();
            t[0x5F] = static_cast<unsigned char>(t[0x5F] - 1);
        }
    } else {
        Camera_Distance = 0x400;
        s[1] = 3;
    }
    MapView_Redraw = 2;
    SH_AT(VoidFn, at::kScreenTile.v)();
}

// original 0x48B2F0 (state 3): the byte after MoveScript_Var7 (0x8034E5) up;
// a tail jump to Effect_Release.
extern "C" void __cdecl EffectKind8D_End(void) {
    unsigned char* const step = At(::bof3::Img(MoveScript_PartyRecords_addr) + 0x65);
    step[0] = static_cast<unsigned char>(step[0] + 1);
    SH_CALL(Effect_Release)();
}

void EffectKind8d_Inject() {
    BOF3_INJECT(EffectKind8D_Run);
    BOF3_INJECT(EffectKind8D_Start);
    BOF3_INJECT(EffectKind8D_Hold);
    BOF3_INJECT(EffectKind8D_Pull);
    BOF3_INJECT(EffectKind8D_End);
}

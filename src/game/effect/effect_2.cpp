// Moved here by tools/move_funcs.py from field_hidden.cpp, sprite_pose.cpp (docs/source-layout-round.md).
#include "game/effect/effect_2.h"

#include <cstdint>
#include <cstring>
#include <windows.h>

#include "bof3/symbols.gen.h"
#include "game/field_hidden_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/sprite_pose.h"
#include "hook/detour.h"
#include "hook/log.h"

using namespace field_hidden;

namespace sprite_pose {

using namespace move_script;

}  // namespace sprite_pose

using namespace sprite_pose;

namespace {

unsigned char Al(std::uint32_t eax) { return static_cast<unsigned char>(eax); }

// An effect object by its index: Effect_Objects + index * 0x80, unchecked.
unsigned char* Effect(unsigned index) { return rv::Bytes(Effect_Objects) + (index << 7); }

}  // namespace

#define FH_EXPORT extern "C" __attribute__((disable_tail_calls))

namespace sprite_pose {

namespace {

constexpr unsigned kEffects = 20, kEffectBytes = 0x80;

}  // namespace

}  // namespace sprite_pose

// original 0x589810 (PSX 0x8019701C): the first of the 20 Effect_Objects whose
// byte +0 is 0, or 0xFF when all are in use.
extern "C" unsigned char __cdecl Effect_FindFree(void) {
    for (unsigned i = 0; i < kEffects; ++i)
        if (Effect_Objects[i].flags == 0) return static_cast<unsigned char>(i);
    return 0xFF;
}

// original 0x589840 (PSX 0x80197070): bytes 0..4 of the current object to 0,
// the pointer re-read before each store as the original does.
extern "C" void __cdecl Effect_Release(void) {
    for (unsigned i = 0; i < 5; ++i) Sprite_Current[i] = 0;
}

// original 0x589870 (PSX 0x801970C0): bytes 0..4 of effect object `index` to
// 0. As the original has it: the index is not checked against the 20 - up to
// 255 reaches 0x7F80 bytes past the pool.
extern "C" void __cdecl Effect_ReleaseAt(unsigned char index) {
    unsigned char* const object = rv::Bytes(&Effect_Objects[index]);
    for (unsigned i = 0; i < 5; ++i) object[i] = 0;
}

// original 0x5898A0 (PSX 0x8019711C): Effect_ReleaseAt for each of the 20.
extern "C" void __cdecl Effect_ClearAll(void) {
    for (unsigned i = 0; i < kEffects; ++i) sprite_pose::g.release_at(static_cast<unsigned char>(i));
}

// original 0x5898D0 (EventScript_Op's handler for Ex, 7 bytes): an effect
// object from Effect_FindFree, none if there is none free: +0 = 1, kind +5 =
// op[1], the position +0x34 / +0x38 as 16.16 - the cell words op[3] / op[5]
// and the halves 0x8000 when op[2] / op[4] are non-zero - its ground +0x3E =
// AreaMap_Elevation(+0x34, +0x38) and +0xB = op[6].
//
// As the original has it: the words are stored before the dwords are read
// back for the call; the op is read after each store.
FH_EXPORT void __cdecl EventOp_Ex(const unsigned char* op) {
    const unsigned char i = Al(field_hidden::g.effect_free());
    if (i == 0xFF) return;
    records::EffectObject* const e = rv::As<records::EffectObject>(Effect(i));
    e->flags = 1;
    e->kind = op[1];
    e->x_whole = op[3];
    e->x_frac = op[2] != 0 ? 0x8000 : 0;
    e->z_whole = op[5];
    e->z_frac = op[4] != 0 ? 0x8000 : 0;
    const long h = field_hidden::g.elevation(e->x, e->z);
    e->height_whole = static_cast<std::uint16_t>(h);
    rv::Bytes(e)[0xB] = op[6];
}

void Effect2_Inject() {
    BOF3_INJECT(Effect_FindFree);
    BOF3_INJECT(Effect_Release);
    BOF3_INJECT(Effect_ReleaseAt);
    BOF3_INJECT(Effect_ClearAll);
    BOF3_INJECT(EventOp_Ex);
}

// Moved here by tools/move_funcs.py from area_w0c.cpp (docs/source-layout-round.md).
#include "game/area/area_28.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w0c.h"
#include "game/area_w0c_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w0c::at;

}  // namespace

namespace {

// Effect_Objects record `slot` (0x80 bytes).
unsigned char* EffectAt(unsigned slot) { return rv::Bytes(Effect_Objects) + slot * at::kEffectStride; }

// Areas 28 and 34's effects at a fixed cell (0x403750, 0x404E00): +0x38 z,
// +0x34 x, +0 = 1, +5 the kind, (area 34) +1 = 6, and +0x3C the ground's
// elevation there, as a word, in the high half.
void SpawnAtCell(unsigned slot, std::int32_t x, std::int32_t z, unsigned char kind, bool state6) {
    unsigned char* const e = EffectAt(slot);
    auto* const tE = rv::As<records::EffectObject>(e);
    tE->z = z;
    tE->x = x;
    e[0] = 1;
    e[5] = kind;
    if (state6) e[1] = 6;
    const auto ground = static_cast<std::int16_t>(AH_CALL(AreaMap_Elevation)(tE->x, tE->z));
    tE->height = static_cast<std::int32_t>(static_cast<std::uint32_t>(static_cast<std::int32_t>(ground)) << 16);
}

}  // namespace

// original 0x403750 (Area28_Handlers[2]; PSX 0x801F3424): a free effect
// slot gets kind 0x40 at cell (0x15, 0x5B), on the ground.
extern "C" void __cdecl Area28_SpawnEffect40(void) {
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    if (slot == 0xFF) return;
    SpawnAtCell(slot, 0x150000, 0x5B0000, 0x40, false);
}

void Area28_Inject() {
    BOF3_INJECT(Area28_SpawnEffect40);
}

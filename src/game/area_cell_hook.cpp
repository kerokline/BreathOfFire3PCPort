// original 0x56E670, Area_CellHook: the per-area table at 0x662F28 (group
// ART names it and its layout) read for Game_AreaNumber. docs/area_harness.md
// section 8. A faithful replacement: no DIVERGENCE.md entry is owed.
#include "game/area_cell_hook.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace area_cell_hook {
namespace at {

// 0x662F28: pairs of (area, handler) dwords. The loop runs while its cursor
// is below 0x663008 (MapCell_Handlers), so the table is 28 pairs - not the
// 100 docs/takeover-queue-areas.md counted from the bytes that follow.
constexpr std::uint32_t kPairs = 0x662F28;
constexpr std::uint32_t kPairStride = 8;
constexpr unsigned kPairCount = (0x663008 - kPairs) / kPairStride;

}  // namespace at
}  // namespace area_cell_hook

// original 0x56E670 (called only by 0x56D7A0, when the chapter's +0x10 is
// null or answers below 0): the first pair whose area byte - the low byte of
// its dword, zero-extended - equals the whole u16 Game_AreaNumber (so an area
// of 0x100 or more matches none); none, 0; else that pair's handler (x, z),
// its al sign-extended. The table is read in place, as the original's, so
// the fuzz's recorders stand in for its handlers.
extern "C" int __cdecl Area_CellHook(unsigned x, unsigned z) {
    namespace at = area_cell_hook::at;
    const unsigned area = Game_AreaNumber;
    unsigned i = 0;
    while (i < at::kPairCount && area_harness::Mem(at::kPairs + at::kPairStride * i)[0] != area) ++i;
    if (i == at::kPairCount) return 0;
    const auto handler = reinterpret_cast<area_harness::Hook>(static_cast<std::uintptr_t>(
        static_cast<std::uint32_t>(move_script::Long(area_harness::Mem(at::kPairs + at::kPairStride * i + 4)))));
    return static_cast<signed char>(handler(x, z));
}

void AreaCellHook_Inject() {
    if (bof3::WantsShadow("area_cell_hook")) area_cell_hook::SelfTest();
    BOF3_INJECT(Area_CellHook);
}

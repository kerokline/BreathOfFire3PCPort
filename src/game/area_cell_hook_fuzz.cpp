// BOF3X_SHADOW=area_cell_hook: Area_CellHook (0x56E670) through the area
// harness, its table's 28 handlers swapped for recorders (a DataTable of
// pairs: stride 8, two arguments each). docs/area_harness.md section 8.
#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_cell_hook.h"
#include "game/area_harness.h"
#include "game/move_script_bytes.h"

namespace area_cell_hook {
namespace {

namespace ah = area_harness;

constexpr std::uint32_t kPairs = 0x662F28;
constexpr unsigned kPairCount = 28;

// 0x56E670: 0x44 bytes; no call out but the one through the table (+0x36)
const ah::Clone kClones[] = {
    {"Area_CellHook", 0x56E670, 0x44, nullptr, 0, nullptr, 0, nullptr, 0,
     reinterpret_cast<const void*>(&::Area_CellHook), 0xFFFFFFFFu, false, ah::Shape::kHook},
};
// the handlers: the second dword of each pair
const ah::DataTable kTables[] = {{kPairs + 4, kPairCount, 8, 2}};

// Game_AreaNumber: an area the table lists (its byte read from the image),
// that byte with a high byte (matches none), the areas beside, or any.
void Seed(unsigned) {
    const unsigned pick = ah::Next() % 8;
    const unsigned listed = ah::Mem(kPairs + 8 * (ah::Next() % kPairCount))[0];
    unsigned area;
    if (pick < 4) area = listed;
    else if (pick == 4) area = listed | (1 + ah::Next() % 0xFF) << 8;
    else if (pick == 5) area = (listed + (ah::Half() ? 1 : 0xFFFF)) & 0xFFFF;
    else if (pick == 6) area = ah::Next() & 0xFF;
    else area = ah::Next() & 0xFFFF;
    Game_AreaNumber = static_cast<unsigned short>(area);
}

}  // namespace

void SelfTest() {
    ah::Group g{"area_cell_hook", kClones, 1, nullptr, 0, kTables, 1, nullptr, 0, &Seed, nullptr, 20000};
    ah::Run(g);
}

}  // namespace area_cell_hook

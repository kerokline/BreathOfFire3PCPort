// BOF3X_SHADOW=scenario_harness_fh: the scenario harness's field mode proved
// on Capcom's code (round twelve group FH, docs/scenario_harness.md section
// 7.7). Every round-twelve shape is driven with a function of the field runs
// on both sides - the harness's copy of the original against the original in
// place - so any difference is the harness's own: a region not put back, a
// pointer argument drawn differently per pass, a data table not restored, a
// span drawn outside its table. No function is taken and nothing is injected.
//
// The thirteen are leaves of the cut table analysis/round12_cut.tsv (no E8 /
// E9 out of them; the original in place would reach the real callee where the
// copy reaches a recorder), or reach out only through a .data table, which the
// harness swaps for recorders on both sides alike. Extents read to the last
// instruction (capstone, 2026-09-28); which group owns each is the cut's.
#include "game/scenario_harness_fh.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"

namespace {

namespace sh = scenario_harness;
namespace at = scenario_harness::at;
using sh::Arg;
using sh::ArgAt;
using sh::Shape;

const void* InPlace(std::uint32_t address) { return reinterpret_cast<const void*>(static_cast<std::uintptr_t>(address)); }

// base, size: the extent to the last instruction; the original in place is "ours".
#define FH_CLONE(name, base, size) name, base, size, nullptr, 0, nullptr, 0, nullptr, 0, InPlace(base)
const sh::Clone kClones[] = {
    // kSprite: FE1 0x52F980 jumps through 0x6609AC by Sprite_Current +3; FC3
    // 0x525CC0 through 0x66017C by +4 (sprite_span 5 keeps both inside)
    {FH_CLONE("FE1 0x52F980", 0x52F980, 0x12), 0, false, Shape::kSprite},
    {FH_CLONE("FC3 0x525CC0", 0x525CC0, 0x12), 0, false, Shape::kSprite},
    // kMenu: FS 0x5837E0 jumps through 0x6641BC by the menu state 0x929F01
    // (menu_span 8); 0x5811B0 opens the menu block when the wait word is 0;
    // 0x5845E0 counts the menu timer down and steps on at 0
    {FH_CLONE("FS 0x5837E0", 0x5837E0, 0xE), 0, false, Shape::kMenu},
    {FH_CLONE("FS 0x5811B0", 0x5811B0, 0x29), 0, false, Shape::kMenu},
    {FH_CLONE("FS 0x5845E0", 0x5845E0, 0x15), 0, false, Shape::kMenu},
    // kCursor: FO's event-script conditions (EventScript_Conditions entries 1
    // and 9): the operand byte against Game_AreaNumber, and against 1 with
    // Field_StatusBits' bit 0; al compared
    {FH_CLONE("FO 0x57C1A0", 0x57C1A0, 0x15), 0, false, Shape::kCursor},
    {FH_CLONE("FO 0x57C230", 0x57C230, 0x16), 0, false, Shape::kCursor},
    // kScript: FE2 0x56E020 is handed a pointer and reads its byte +0x86, then
    // calls through 0x662E1C with (pointer, 0x904030). Its argument is an
    // object record in the game, not an op - no leaf of the 323 is handed an
    // op (the five EventOp_* call out) - so this proves the shape's mechanics:
    // a[0] the script cursor's op, read through, compared as a region
    {FH_CLONE("FE2 0x56E020", 0x56E020, 0x1D), 0, false, Shape::kScript},
    // kCall: FO MoveCmd_OpE9 0x57C8E0, seven words (the object a sprite
    // record), forwarded through 0x663B84 by Sprite_Current +4, al answered;
    // FE2 0x5343C0 (a byte, a counter it increments - a scratch pointer);
    // FE2 0x534420 (a byte); FE1 0x52D880 (none, al answered); FE2 0x5728D0
    // (x, z: a cell of the area block through AreaMap_Bytes, then MapView_Cells)
    {FH_CLONE("FO 0x57C8E0", 0x57C8E0, 0x39), 0xFF, false, Shape::kCall, ArgAt(0, Arg::kSprite)},
    {FH_CLONE("FE2 0x5343C0", 0x5343C0, 0x55), 0, false, Shape::kCall, ArgAt(1, Arg::kScratch)},
    {FH_CLONE("FE2 0x534420", 0x534420, 0x57), 0, false, Shape::kCall},
    {FH_CLONE("FE1 0x52D880", 0x52D880, 0x39), 0xFF, false, Shape::kCall},
    {FH_CLONE("FE2 0x5728D0", 0x5728D0, 0x128), 0, false, Shape::kCall},
};
#undef FH_CLONE
constexpr unsigned kOpE9 = 8, kCounter = 9, kCount = 10, kCells = 12, kScript = 7, kArea = 5, kStatus = 6;

// The tables these reach out through, swapped for recorders on both sides.
// Their lengths: to the next table's start (0x6609C0, 0x66019C, 0x6641DC; the
// four MoveCmd_OpE9 handlers symbols.toml names), and twelve of 0x662E1C's.
const sh::DataTable kTables[] = {
    {0x6609AC, 5}, {0x66017C, 8}, {0x6641BC, 8}, {0x663B84, 4}, {0x662E1C, 12},
};

// Beyond field mode's standard regions: the save block's words 0x904160..
// (0x52D880's two runs of 0x80 at 0x904154 / 0x904354) and 0x904700..0x904900
// (0x5343C0 / 0x534420's eight records at 0x9048B0 and the pairs 0x9046D0..),
// and MapView_Cells, which 0x5728D0 reads after its write (a group's region,
// section 7.3).
const sh::Region kRegions[] = {{0x904160, 0x280}, {0x904700, 0x200}, {at::kMapCells, 1568 * 2}};

void Seed(unsigned k) {
    unsigned char* const op = sh::Script();
    auto* const sprite = static_cast<unsigned char*>(Sprite_Current);
    switch (k) {
    case kArea:
        // the operand the area number half the time
        if (sh::Half()) sh::Mem(at::kArea)[0] = op[0], sh::Mem(at::kArea)[1] = 0;
        break;
    case kStatus: if (sh::Half()) op[0] = 1; break;
    case kScript: op[0x86] = static_cast<unsigned char>(sh::Next() % 12); break;   // inside 0x662E1C's twelve
    case kOpE9: sprite[4] &= 3; break;                                             // inside 0x663B84's four
    case kCells:
        // every view cell empty: past the write, a cell word would walk the
        // area block's records from AreaMap_CellBase, which the random block
        // does not bound - this proof stops at the cell test
        for (unsigned i = 0; i < 1568 * 2; ++i) sh::Mem(at::kMapCells)[i] = 0;
        break;
    case kCounter:
    case kCount:
        // some of the eight records the kind (4) and the argument's byte they look for
        for (unsigned i = 0; i < 8; ++i)
            if (sh::Half()) sh::Mem(0x9048B0 + 8 * i)[0] = k == kCounter ? 4 : static_cast<unsigned char>(i);
        break;
    default: break;
    }
}

void Args(unsigned k, std::uint32_t* a) {
    switch (k) {
    case kCounter: a[0] = sh::Mem(0x9048B1 + 8 * (sh::Next() % 8))[0]; break;   // a record's byte, often a match
    case kCount: a[0] = sh::Next() % 10; break;
    case kCells:
        // x, z inside the area block: AreaMap_Bytes + width * z + x, width below 0x20
        a[0] = sh::Next() & 0x3F;
        a[1] = sh::Next() & 0x3F;
        break;
    default: break;
    }
}

}  // namespace

void ScenarioHarnessFh_Inject() {
    if (!bof3::WantsShadow("scenario_harness_fh")) return;
    sh::Group g = {"scenario_harness_fh", kClones, sizeof kClones / sizeof kClones[0], nullptr, 0,
                   kTables, sizeof kTables / sizeof kTables[0], kRegions, sizeof kRegions / sizeof kRegions[0],
                   Seed, nullptr, 0};
    g.args = Args;
    g.field = true;
    g.sprite_span = 5;
    g.menu_span = 8;
    sh::Run(g);
}

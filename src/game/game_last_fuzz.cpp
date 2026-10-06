// BOF3X_SHADOW=game_last: group TWO's two functions through the scenario
// harness (scenario_harness.h, used unchanged), once at start-up.
// docs/game-last.md section 3.
//
// The clone rows: capstone 2026-10-06, each extent read to its last
// instruction. Neither has an E8 or E9 leaving it: Item_UseFlags returns from
// each of its five cases and jumps only through its own table (moved into the
// copy); ItemTrade_Dispatch's one way out is the indirect jump through
// ItemTrade_States, whose three cells are a DataTable, swapped for recorders
// while the fuzz runs. Shapes: Item_UseFlags kCall, its whole eax compared;
// ItemTrade_Dispatch kState.
#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/game_last.h"
#include "game/game_last_callees.h"
#include "game/scenario_harness.h"
#include "hook/log.h"

namespace game_last {
namespace {

namespace sh = scenario_harness;
using U = std::uint32_t;

template <typename... T> U PickOf(T... v) {
    const U values[] = {static_cast<U>(v)...};
    return sh::Pick(values, sizeof...(v));
}

// --- the clone table (capstone, 2026-10-06) -----------------------------------------
// Item_UseFlags: jmp [eax*4 + 0x591888] at +0xF, its disp32 at +0x12; the
// table at +0x78, four entries (0x591826, 0x591840, 0x591857, 0x59186B).
constexpr sh::JumpTable kTables591810[] = {{0x12, 0x78, 4}};

#define GL_FN(name) reinterpret_cast<const void*>(&::name)
constexpr U kAll = 0xFFFFFFFFu;
// {name, base, size, calls, imms, tables, ours, ret_mask, calm, shape}
const sh::Clone kClones[] = {
    {"Item_UseFlags", 0x591810, 0x88, nullptr, 0, nullptr, 0, kTables591810, 1, GL_FN(Item_UseFlags), kAll, false,
     sh::Shape::kCall},
    {"ItemTrade_Dispatch", 0x593950, 0xE, nullptr, 0, nullptr, 0, nullptr, 0, GL_FN(ItemTrade_Dispatch), 0, false,
     sh::Shape::kState},
};
#undef GL_FN

enum : unsigned { kUseFlags, kDispatch, kCount };
static_assert(kCount == sizeof kClones / sizeof kClones[0], "one enum entry a clone, in order");

const sh::DataTable kTables[] = {
    {0x66A470, 3},   // ItemTrade_States (ItemTrade_States_count)
};

// The group's own regions: the item tables as far as item 255 of any category
// reaches (random every round, so a wrong record, stride or byte shows), and
// the trade bytes 0x93985C..0x93985F.
const sh::Region kRegions[] = {
    {at::kConsumableFlags, at::kFlagsReachEnd - at::kConsumableFlags},
    {at::kTradeState, 4},
};

// --- the seed ------------------------------------------------------------------------

// ItemTrade_Dispatch: every state inside the table, each a third of the time.
// Past the table ours aborts by design (section 4's control, not a seed).
void Seed(unsigned k) {
    if (k == kDispatch) sh::Mem(at::kTradeState)[0] = static_cast<unsigned char>(sh::Next() % ItemTrade_States_count);
}

// Item_UseFlags' two words: every category value 0..5 and around the low byte's
// wrap, any byte; the item at each table's first and last record, one past it,
// the byte's edges, any byte - each with random upper bytes half the time (the
// callers push whole registers).
void Args(unsigned k, std::uint32_t* a) {
    if (k != kUseFlags) return;
    const U category = PickOf(0, 1, 2, 3, 4, 5, 6, 0x7F, 0x80, 0xFE, 0xFF, sh::Next() & 0xFF);
    // records: consumables 92, weapons 83, armour 68, accessories 52
    const U item = PickOf(0, 1, 51, 52, 53, 67, 68, 69, 82, 83, 84, 91, 92, 93, 0x7F, 0x80, 0xFE, 0xFF, sh::Next() & 0xFF);
    a[0] = sh::Half() ? category : (sh::Next() & 0xFFFFFF00u) | category;
    a[1] = sh::Half() ? item : (sh::Next() & 0xFFFFFF00u) | item;
}

}  // namespace

void SelfTest() {
    sh::Group g = {"game_last", kClones, kCount, nullptr, 0, kTables, sizeof kTables / sizeof kTables[0], kRegions,
                   sizeof kRegions / sizeof kRegions[0], &Seed, nullptr, 4000};
    g.args = &Args;
    sh::Run(g);
}

}  // namespace game_last

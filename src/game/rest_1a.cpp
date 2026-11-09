// Group R1A of round fourteen, wave one (docs/rest_1a.md): the 49 functions
// 0x51BA80..0x51D70C of the cut (analysis/round14_cut.tsv) - three
// party-member states and the field actions of party sets 0, 1 and 2, with the
// dispatchers that reach them. Each read with capstone to its last instruction.
//
// Every call out goes through the scenario harness (SH_CALL), so the start-up
// fuzz can stand recorders in for ours as for the originals' copies; every
// table dispatch reads the table's word in place (CodeAt), which the fuzz
// swaps for recorders. No divergence: each is a faithful replacement.
// Sprite_Current and Field_State are read afresh at every use, as the
// original reads [0x937F88] / [0x905D98] (a callee may move either). Where the
// original would jump through a table word that is not code, or write an
// object record by an index its callee never answers, ours aborts with a
// message (docs/rest_1a.md section 5).
#include "game/rest_1a.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_0a.h"
#include "game/rest_1a_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = rest_1a::at;
using URest1a = std::uint32_t;
using move_script::At;
using move_script::Long;

std::int32_t L(URest1a a) { return move_script::Long(move_script::At(a)); }
unsigned char* Sc() { return Sprite_Current; }
unsigned char* Fs() { return Field_State; }

// `cdq; xor eax, edx; sub eax, edx`: the absolute value with the original's
// wrap (the most negative stays negative).
std::int32_t Abs32(std::int32_t v) {
    const auto u = static_cast<URest1a>(v);
    const URest1a s = static_cast<URest1a>(v >> 31);
    return static_cast<std::int32_t>((u ^ s) - s);
}

// A .data dispatch table read in place. The original's index is unchecked:
// past the table it jumps through the next table's cell, or into data. Ours
// aborts at the table's own count (its symbols.toml count), before the read -
// the rule all seven of wave one's groups share (docs/rest_1b.md section 6).
// While the fuzz runs, the entries are its recorders.
using HandlerRest1a = void (__cdecl*)();
HandlerRest1a CodeAt(URest1a table, unsigned count, unsigned index, const char* who) {
    const URest1a cell = table + 4u * index;
    if (index >= count)
        bof3::Fatal("%s: index %u is past its table's %u entries at 0x%X; the original jumps through 0x%X", who, index,
                    count, static_cast<unsigned>(::bof3::VaOf(table)), static_cast<unsigned>(::bof3::VaOf(cell)));
    return reinterpret_cast<HandlerRest1a>(static_cast<std::uintptr_t>(static_cast<URest1a>(L(cell))));
}

// --- the shared bodies of the three sets' identical copies -------------------------

}  // namespace

// ===========================================================================
// The party members' states (Member_States 0x65F960, Field_MemberFrame's by +1)
// ===========================================================================

// ===========================================================================
// The dispatchers: a table's word by the form (u16 +0x2C), the state (+2) or
// the step (+3), jumped to, the index unchecked
// ===========================================================================

// ===========================================================================
// The form actions' states (shared: each sits in several sets' tables)
// ===========================================================================

// ===========================================================================
// The actions' states
// ===========================================================================

void Rest1A_Inject() {
    if (bof3::WantsShadow("rest_1a")) rest_1a::SelfTest();
}

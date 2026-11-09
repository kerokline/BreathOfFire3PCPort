// World 0's areas 27..29 and 32..37: the PSX's BIN/WORLD00/AREA027..037.EMI
// compiled into the exe at 0x403400..0x4053B0 (Area_Descriptors entries 27,
// 28, 29, 32..37; areas 30 and 31 have no code). Round ten, group AR0C: the
// 52 functions of the band not already ours, each read to its last
// instruction with capstone (2026-09-27) and taken through the area harness
// (area_harness.h). Round eight's DA took the band's 20 world-map functions
// (worldmap_area.cpp) and round seven's world_map.cpp the map's frame and
// draws; they are called here by name. docs/area_w0c.md.
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. The
// five dispatchers through an area's .data state table abort past the table
// where the original would call whatever the next dwords hold (the owner's
// rule for an unchecked index, round9 doc section 6; no route reaches it).
// Every call goes through the harness (AH_CALL / AH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies.
#include "game/area_w0c.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w0c_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w0c::at;
using area_harness::Mem;
using move_script::Long;
using move_script::SetWord;
using move_script::Word;

using Handler = void (__cdecl*)();

unsigned char& B(std::uint32_t address) { return *Mem(address); }
void AddWord(unsigned char* p, int v) { SetWord(p, static_cast<unsigned>(Word(p) + v)); }
// Effect_Objects record `slot` (0x80 bytes).
unsigned char* EffectAt(unsigned slot) { return rv::Bytes(Effect_Objects) + slot * at::kEffectStride; }

// A state handler read from an area's .data table in place, as the
// originals' `jmp [index * 4 + table]`: the index is not checked there. Ours
// aborts past the table (what follows is the next table, or data).
Handler StateEntry(const char* who, std::uint32_t table, unsigned count, unsigned index) {
    if (index >= count)
        bof3::Fatal("%s: state %u is past its %u-entry table 0x%X", who, index, count, static_cast<unsigned>(table));
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(table + index * 4u)))));
}

// Choice handlers read the message box's answer; most set the message word
// to 0xFFFF (no new message) first.
signed char Answer() { return static_cast<signed char>(B(::bof3::Img(AreaW0c_ChoiceAnswer_addr))); }
void SetMessage(unsigned id) { SetWord(Mem(::bof3::Img(at::kMessage.v)), id); }

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

// ===========================================================================
// Area 27 (descriptor 0x5ED848): seven choices, two handlers (the choices'
// last two), two mode-tail phases.
// ===========================================================================

// ===========================================================================
// Area 28 (descriptor 0x5EE1A0): one handler of its own (handlers 0 and 1 are
// the shared `ret` 0x437CC0).
// ===========================================================================

// ===========================================================================
// Area 32 (descriptor 0x5EF2C8): eight choices, six handlers (choices 2..7),
// two two-state machines through Area32_StatesA / Area32_StatesB.
// ===========================================================================

// ===========================================================================
// Area 33 (descriptor 0x5EF4D8, the Yraall world map): world-map record 1's
// slots +8 (effect kind 0x16) and +4 (effect kind 0xE), and their starts.
// The rest of the area is round eight's (worldmap_area.cpp).
// ===========================================================================

// ===========================================================================
// Area 34 (descriptor 0x5F01A0): three choices, two handlers (choices 1 and
// 2), an object trigger.
// ===========================================================================

// ===========================================================================
// Area 35 (descriptor 0x5F0330): four handlers.
// ===========================================================================

// ===========================================================================
// Area 36 (descriptor 0x5F0540): the step hook, and effect kind 0xB5's two
// states (its handlers are shared bodies of other groups).
// ===========================================================================

// ===========================================================================
// Area 37 (descriptor 0x5F14A0): 22 choices (six of them shared bodies of
// another group's, several named twice), one handler, a mode-tail phase,
// four object triggers.
// ===========================================================================

void AreaW0c_Inject() {
    if (bof3::WantsShadow("area_w0c")) area_w0c::SelfTest();
}

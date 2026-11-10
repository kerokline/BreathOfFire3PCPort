// World 3's area 135: the PSX's BIN/WORLD03/AREA135.EMI compiled into the exe
// at 0x41DAD0..0x41EFE0 (Area_Descriptors entry 135, descriptor 0x62D940).
// Round ten, group AR3D: the band's 46 functions, none ours before, each read
// to its last instruction with capstone (2026-09-28) and taken through the
// area harness (area_harness.h). docs/area_w3d.md.
//
// Every one is a faithful replacement but one: Area135_Init's DIV-0180 (the
// marker placed when it is to be shown and was never placed). Reads
// by an unchecked index into .data are kept (they stay in .data); where the
// original jumps through a pointer read past what its dispatcher's table
// reaches, divides by a byte that can be 0, or writes through an index past
// the 30 field objects or the 20 effect records, ours aborts with a message
// instead (round nine section 6: no ledger entry). Every call goes through
// the harness (AH_CALL / AH_AT), so the start-up fuzz can stand recorders in
// for ours as for the originals' copies; the group's own callees are called
// the same way, so each function is fuzzed alone.
#include "game/area_w3d.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w3d_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace {

namespace at = area_w3d::at;
using area_harness::Mem;
using move_script::SetWord;
using U = std::uint32_t;

unsigned char& B(U address) { return *Mem(address); }

// Effect record `slot` (Effect_Objects, 20 of 0x80 bytes). Choice 2 stores
// Effect_FindFree's "none", 0xFF, as the slot the tail later writes through,
// unchecked: past the 20 records ours aborts.
unsigned char* EffectAtAreaW3d(const char* who, unsigned slot) {
    if (slot >= at::kEffectCount)
        bof3::Fatal("%s: the effect slot %u is past the 20 effect records (the original writes 0x%X, past "
                    "Effect_Objects)",
                    who, slot, static_cast<unsigned>(::bof3::Img(Effect_Objects_addr) + slot * at::kEffectStride));
    return Mem(::bof3::Img(Effect_Objects_addr) + slot * at::kEffectStride);
}

}  // namespace

// ===========================================================================
// Choices 0..3 (Area135_Choices 0x62D8DC; choices 4..23 are the handlers).
// ===========================================================================

// ===========================================================================
// Handlers 0..19 (Area135_Handlers 0x62D8EC = choices 4..23).
// ===========================================================================

// ===========================================================================
// The hooks, the tail and the init.
// ===========================================================================

// ===========================================================================
// The helpers.
// ===========================================================================

void AreaW3d_Inject() {
    if (bof3::WantsShadow("area_w3d")) area_w3d::SelfTest();
}

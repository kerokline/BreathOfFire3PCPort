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

// original 0x41EB40 (Area_Descriptors[135] +0x40; PSX 0x801F5A44): zone 4
// (Cond_ByteFD): entered from zone 1 (0x905E68): EventOp_Bx(Area135_InitOp),
// story flag 0xC cleared, the zone read again, the tail's sub byte 0; then
// the byte 0x8034B0 |= 1. Zone (as last read) 4, 6 or 7: the marker's +0 = 1.
// Zone 0: Effect_FindFree; a slot: +0 = 1, kind +5 = 0x53, dwords +0x18 = 0,
// +0x1C = 6; counter 3 = 0.
//
// DIVERGENCE DIV-0180 (1 through PatchBytes("Area135MarkerPlaced") in
// AreaW3d_Inject, after the self-test): the marker (Sprite_ObjectsExtra[3])
// is placed only by the zone-1 branch's op Bx, and kept in .bss across the
// area's own reloads; zones 4, 6 and 7 only set its +0 again. Arrived in one
// of them with the marker never placed in this process - a save made there
// (F12 writes one anywhere in the field) loaded after a launch - the
// original shows an object whose record pointer +0x54 is 0, and the next
// field frame's Sprite_UpdateScreenA (0x57B830) reads address 3: an access
// violation in Capcom's port as in ours (the PlayStation's read of 3 is RAM
// and does not fault). With the switch on, an unplaced marker (+0x54 0) is
// placed first by the zone-1 branch itself, as a player entering zone 4 from
// zone 1 has it.
namespace area_w3d { std::uint8_t g_place_marker = 0; }

// ===========================================================================
// The helpers.
// ===========================================================================

void AreaW3d_Inject() {
    if (bof3::WantsShadow("area_w3d")) area_w3d::SelfTest();
    {
        // DIV-0180: after the self-test, which compared Capcom's init.
        const std::uint8_t was = 0, is = 1;
        bof3::PatchBytes("Area135MarkerPlaced", Low32(&area_w3d::g_place_marker), &was, &is, 1);
        bof3::Log("DIV-0180    area 135 places its marker before showing it when it was never placed (on unless the line above says OFF)");
    }
}

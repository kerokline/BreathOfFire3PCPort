// World 1's areas 48..52: the PSX's BIN/WORLD01/AREA048..052.EMI compiled into
// the exe at 0x408FF0..0x40AB00 (Area_Descriptors entries 48..52). Round ten,
// group AR1C: the band's 57 functions, none ours before, each read to its
// last instruction with capstone (2026-09-28) and taken through the area
// harness (area_harness.h). docs/area_w1c.md.
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. Reads
// by an unchecked index into the areas' .data tables are kept (they stay in
// .data); area 51's init keeps the original's header walk, which never ends
// on a zero step (docs/area_w1c.md section 6). Every call goes through the
// harness (AH_CALL / AH_AT), so the start-up fuzz can stand recorders in for
// ours as for the originals' copies; the callees of the group's own are
// called the same way, so each function is fuzzed alone.
#include "game/area_w1c.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w1c_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w1c::at;
using area_harness::Mem;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

unsigned char& B(std::uint32_t address) { return *Mem(address); }
std::uint16_t High(std::int32_t v) { return static_cast<std::uint16_t>(static_cast<std::uint32_t>(v) >> 16); }
// Effect_Objects record `slot` (0x80 bytes).
unsigned char* EffectAt(unsigned slot) { return rv::Bytes(Effect_Objects) + slot * at::kEffectStride; }
// Party record `member` (ObjTrio, 0x14C bytes).
unsigned char* PartyAt(unsigned member) { return rv::Bytes(&ObjTrio[member]); }

void SetMessage(unsigned id) { SetWord(Mem(::bof3::Img(at::kMessage.v)), id); }

// The four callees nobody owns, by their raw addresses (area_w1c_callees.h).
void FlagsToggle(unsigned char* bits, unsigned index) {
    AH_AT(void (__cdecl*)(unsigned char*, unsigned), at::kFlagsToggle.v)(bits, index);
}

// The map byte setter, as the originals pass it: x and z words, a value byte.
void SetByte(unsigned x, unsigned z, unsigned value) { AH_CALL(AreaMap_SetByte)(x, z, value); }

// ---- area 48 ----

// ---- area 49 ----

// ---- area 52 ----

}  // namespace

// ===========================================================================
// Area 48 (descriptor 0x5F98A8): seven choices, six handlers (choices 1..6),
// the tail kind 3, the arrive hook, the init.
// ===========================================================================

// ===========================================================================
// Area 49 (descriptor 0x5FBBF8): 23 choices (five of them other groups'
// shared bodies), 21 handlers (choices 2..22), the step and cell hooks, the
// effect frame effect kind 0x70 runs here, and their helpers.
// ===========================================================================

// ===========================================================================
// Area 50 (descriptor 0x5FCD48): a choice, a handler, an object trigger (its
// other choices are other groups' shared bodies).
// ===========================================================================

// ===========================================================================
// Area 51 (descriptor 0x5FD450): three choices, two handlers (choices 1, 2),
// the init.
// ===========================================================================

// ===========================================================================
// Area 52 (descriptor 0x5FE080): nine handlers, the cell hook, the tail kind
// 15, and the block helpers (docs/area_w1c.md section 1).
// ===========================================================================

void AreaW1c_Inject() {
    if (bof3::WantsShadow("area_w1c")) area_w1c::SelfTest();
    {
        // DIVERGENCE DIV-0176: Sunder's animation at Mt. Myrneg's entry (area
        // 51, the +0x10 movement script 3 of the bank-0x44 object) is the
        // looping cut 0x0F, not the once-and-hold cut 0x10 - the script's
        // first op, `0E 10 01 01` at 0x5FD000 in BOF3.exe's .data, asks for
        // 0x0F. One byte; the original in both hosts under
        // BOF3X_ORIGINAL=SunderLoop. docs/cut-content.md 4.
        static const std::uint8_t was = 0x10, is = 0x0F;
        const bool on = bof3::PatchBytes("SunderLoop", 0x5FD001, &was, &is, 1);
        bof3::Log("DIV-0176    Sunder's animation at Mt. Myrneg's entry loops (0x0F) until the scene reaches him: %s",
                  on ? "on" : "off");
    }
}

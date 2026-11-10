// The inn's, save point's and rest's last states, the save block's builder,
// the shop's browse and sell modes, the master's talk and its panels, the
// figure record's moves - round fourteen, wave two, group R2C: the 60
// functions 0x57F340..0x58699F of the cut (analysis/round14_cut.tsv) and
// ShopMode_States[9] 0x583350, which no list had; each read with capstone to
// its last instruction (docs/rest_2c.md section 1).
//
// Every call out goes through the scenario harness (SH_CALL / SH_AT), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies;
// a table's handler is called by the address the table holds. No divergence:
// each is a faithful replacement. Where the original jumps through a table by
// a byte past the table's own count, or writes a slot's summary past the
// sixteen, ours aborts with a message (section 5); no state or caller writes
// such a byte. Reads past a table (a scale, a face, a master's message, a
// record by an id byte) read the image in place, as the original does.
#include "game/rest_2c.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_2c_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace {

namespace at = rest_2c::at;
using U = std::uint32_t;
constexpr auto& AtRest2c = move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

unsigned char& BRest2c(U address) { return *AtRest2c(address); }
U LRest2c(U address) { return static_cast<U>(Long(AtRest2c(address))); }
void PutL(U address, U v) { SetLong(AtRest2c(address), static_cast<std::int32_t>(v)); }
void PutW(U address, unsigned v) { SetWord(AtRest2c(address), v); }
void PutF(unsigned char* at, float v) { std::memcpy(at, &v, sizeof v); }
U Key(const void* p) { return Low32(p); }

// A state table's handler, jumped to as the original's `jmp [table + 4 *
// index]` does: the index must lie inside the table's own count (its reader's
// reach, docs/rest_2c.md section 3). Past it the original jumps through the
// next table's cell; no state writes such an index (section 5), so ours
// aborts. The entry is called as the cell holds it: Capcom's address in the
// game (Inject's jmp to ours where it is ours), a recorder while the fuzz runs.
using Handler = void (__cdecl*)();
void Jump(U table, unsigned count, unsigned index, const char* who) {
    if (index >= count)
        bof3::Fatal("%s: state %u is past its table's %u entries at 0x%X; the original jumps through 0x%X", who, index, count,
                    static_cast<unsigned>(::bof3::VaOf(table)), static_cast<unsigned>(::bof3::VaOf(table + 4 * index)));
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(LRest2c(table + 4 * index)))();
}

// The window record n's byte / word at +off.
U Win(unsigned n, unsigned off) { return ::bof3::Img(WindowRecords_addr) + n * rest_2c::at::kWindowStride + off; }

// A party id's form: id 4 is drawn and saved as 0xB unless the story byte's
// low seven bits are 5 or 0xC (Save_BuildBlock's and MasterPanel_DrawFace's
// test, the same instructions).
unsigned char FormOf(unsigned char id) {
    if (id != 4) return id;
    const unsigned story = AreaW0b_PartySet & 0x7F;
    return story == 5 || story == 0xC ? id : static_cast<unsigned char>(0xB);
}

}  // namespace

#pragma clang attribute push(__attribute__((disable_tail_calls)), apply_to = function)

// ===========================================================================
// The figure record 0x9398E0 (MasterFigure_States 0x663E28, by +1)
// ===========================================================================

// ===========================================================================
// The inn and the save point (shop_states.cpp has their other states)
// ===========================================================================

// ===========================================================================
// The rest (ShopMode_States[10], Rest_States)
// ===========================================================================

// ===========================================================================
// The save block (FieldSave_Write's and Save_QuickWrite's)
// ===========================================================================

// ===========================================================================
// The shop's modes: the dispatchers of ShopMode_States 5..9
// ===========================================================================

// ===========================================================================
// ShopMode 9, the browse (ShopBrowse_States 0x664190 and its steps)
// ===========================================================================

// ===========================================================================
// The master's talk (MasterTalk_States 0x66450C, by 0x9398CF)
// ===========================================================================

// ===========================================================================
// The panels
// ===========================================================================

#pragma clang attribute pop

void Rest2C_Inject() {
    if (bof3::WantsShadow("rest_2c")) rest_2c::SelfTest();
}

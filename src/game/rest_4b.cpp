// The community band's tail kinds and its board - round fourteen, wave four,
// group R4B: the 60 functions 0x456D50..0x459EDA of the cut
// (analysis/round14_cut.tsv), each read with capstone to its last instruction
// (docs/rest_4b.md section 1). Field_ModeTailKinds 14, 21..26 and 60 and the
// states their tables hold; the board those states run (CommuBoard_States, by
// 0x939A3E): a grid cursor over eight slots and three more, the 60 community
// records placed in them, the lists a slot's kind is picked from, and the
// panel, cards, bars and sprites it draws.
//
// Every call out goes through the scenario harness (SH_CALL / SH_AT), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies;
// a table's handler is called by the address the table holds. No divergence:
// each is a faithful replacement. Where the original jumps through a table by
// an index past the table's own count, writes a record or a slot past its
// table, or reads a stack table past its end, ours aborts with a message
// (docs/rest_4b.md section 5, one policy for the group); reads of the .data
// tables by an index the code computes read the image in place, as the
// original does.
#include "game/rest_4b.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/faerie_name.h"
#include "game/labels.h"
#include "game/lang_layout.h"
#include "game/move_script_bytes.h"
#include "game/rest_4b_callees.h"
#include "game/scenario_harness.h"
#include "game/yes_no_layout.h"
#include "hook/detour.h"
#include "hook/log.h"

// DIVERGENCE DIV-0027 (amended 2026-10-26, issue 152): the faerie board's
// "Do you accept?  Yes No" (help 0x3D) marks its answer with a flashing tile,
// not a hand, and the original places the tile at 0xD0 + 36 * the answer,
// 0x19 + 12 * the answer wide - the Chinese line's words. The English line the
// overlay carries puts Yes at 245 and No at 277, so the tile on Yes sat in the
// gap before it and on No covered Yes (the owner's screenshots). The US disc's
// twin (COMMU01.EMI, 0x801D3038, read 2026-10-26) draws the same line at the
// same place and its tile at 0xF4 + 32 * the answer, 0x19 - 8 * the answer
// wide: one unit left of each word, one wider than it. With the flag on
// (Rest4B_Inject, a Latin overlay, after the self-test, which compares the
// original's draw) the line is re-spaced and the tile measured from it as the
// other prompts' hands are (yes_no_layout.cpp's YesNoLayout_Tail): the
// word's pen place less one, the word's width plus one, in any language.
unsigned char g_board_yes_no_layout = 0;

// DIVERGENCE DIV-0200 (faerie_name.h): 1 through PatchBytes("FaerieNameEnd")
// in Rest4B_Inject, after the self-test.
namespace rest_4b { std::uint8_t g_faerie_name_end = 0; }

namespace {

namespace at = rest_4b::at;
using U = std::uint32_t;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

unsigned char& B(U address) { return *At(address); }
unsigned W(U address) { return Word(At(address)); }
U L(U address) { return static_cast<U>(move_script::Long(move_script::At(address))); }
void PutW(U address, unsigned v) { SetWord(At(address), v); }
void PutL(U address, U v) { SetLong(At(address), static_cast<std::int32_t>(v)); }
void PutW(unsigned char* p, unsigned v) { SetWord(p, v); }
U Key(const void* p) { return Low32(p); }

// A state table's handler, jumped to as the original's `jmp [table + 4 *
// index]` does: the index must lie inside the table's own count (the run of
// code words from the reader's cell, docs/rest_4b.md section 3). Past it the
// original jumps through whatever follows; no state writes such an index, so
// ours aborts (section 5). The entry is called as the cell holds it: Capcom's
// address in the game (Inject's jmp to ours where it is ours), a recorder
// while the fuzz runs.
using Handler = void (__cdecl*)();
void Jump(U table, unsigned count, int index, const char* who) {
    if (index < 0 || static_cast<unsigned>(index) >= count)
        bof3::Fatal("%s: state %d is outside its table's %u entries at 0x%X; the original jumps through 0x%X", who, index, count,
                    static_cast<unsigned>(::bof3::VaOf(table)), static_cast<unsigned>(::bof3::VaOf(table + 4 * static_cast<U>(index))));
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(L(table + 4 * static_cast<U>(index))))();
}

// The community record k (0x9046D0 + 8k): ours writes only the 60; past them
// the original writes on into the save block (section 5).
U Member(unsigned k, const char* who) {
    if (k >= 60)
        bof3::Fatal("%s: community record %u is past the 60 at 0x9046D0; the original writes at 0x%X", who, k,
                    static_cast<unsigned>(Community_Records_addr + 8 * k));
    return ::bof3::Img(Community_Records_addr) + 8 * k;
}

// A slot's bytes (0x9048A8 + 8 * slot, the slots 1..8 at 0x9048B0): read in
// place for any slot byte, as the original reads them.
U Slot(unsigned slot) { return ::bof3::Img(at::kSlots.v) + 8 * slot; }

// The field tail's state byte, moved by one as the original's inc / dec do.
void StateBy(int d) { AreaW0c_TailState = static_cast<unsigned char>(static_cast<unsigned char>(AreaW0c_TailState) + d); }

}  // namespace

#pragma clang attribute push(__attribute__((disable_tail_calls)), apply_to = function)

// ===========================================================================
// The tail kinds (Field_ModeTailKinds 14, 21..26, 60) and their states
// ===========================================================================

// ===========================================================================
// The board (CommuBoard_States, by 0x939A3E)
// ===========================================================================

// ===========================================================================
// The board's draws
// ===========================================================================

#pragma clang attribute pop

void Rest4B_Inject() {
    if (bof3::WantsShadow("rest_4b")) rest_4b::SelfTest();
    {
        // DIV-0200, after the self-test (which compares Capcom's draw).
        const std::uint8_t was = 0, is = 1;
        bof3::PatchBytes("FaerieNameEnd", static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&rest_4b::g_faerie_name_end)), &was, &is, 1);
    }
    // DIVERGENCE DIV-0027 (amended 2026-10-26): after the self-test, which
    // compares the original's panel; a Latin overlay only (DIV-0056).
    if (Lang_Latin()) {
        static const std::uint8_t was = 0, is = 1;
        bof3::PatchBytes("BoardYesNoLayout", Low32(&g_board_yes_no_layout),
                         &was, &is, 1);
        bof3::Log("DIV-0027    the faerie board's \"Do you accept?\": answers re-spaced, the tile over each");
    }
}

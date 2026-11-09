// Two of the community's games and the board helpers a third draws with -
// round fourteen, wave four, group R4D: the cut's 60 functions
// 0x45C400..0x45E86E (analysis/round14_cut.tsv), each read with capstone to its
// last instruction (docs/rest_4d.md section 1).
//
// Every call out goes through the scenario harness (SH_CALL / SH_AT), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies; a
// table's handler is called by the address the table holds. No divergence: each
// is a faithful replacement. Where the original jumps through a table by a byte
// past the table's own count, ours aborts with a message (section 3): no state
// writes such a byte. Where it dereferences a name pointer past
// CommuName_RecordNames' seven, or loops forever on a limit of 0
// (CommuDraw_RandBelow), ours aborts before the read or the loop (section 5).
// Byte reads past a table (a slot's name, a slot's default, the draw's rows
// with a negative row) read the image in place, as the original does.
//
// Coordinates: the original builds several in a 16-bit register whose upper
// half is the previous call's eax (`movzx ax, byte`); every callee reads those
// arguments' low word (cited in rest_4d_fuzz.cpp's callee rows), so ours passes
// the value with a clean upper half.
#include "game/rest_4d.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/rest_4d_callees.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = rest_4d::at;
using U = std::uint32_t;
using move_script::At;
using move_script::SetWord;
using move_script::Word;

unsigned char& B(U address) { return *At(address); }
const unsigned char* Text(U address) { return At(address); }

}  // namespace
// DIV-0075, set by Rest4D_Inject after the self-test: the two entry steps move
// the step's step on, where the original's never does.
namespace rest_4d { unsigned char g_entry_abandons = 0; }
namespace {

char* Scratch() { return reinterpret_cast<char*>(At(::bof3::Img(at::kTextScratch.v))); }

// A message's text: MessagePools' word at `words + 2 * index` from 0x803580.
const unsigned char* PoolText(U words, int index) {
    return Text(::bof3::Img(at::kPoolWords.v) + Word(At(words + 2u * static_cast<U>(index))));
}

}  // namespace

#pragma clang attribute push(__attribute__((disable_tail_calls)), apply_to = function)

// ===========================================================================
// The board (R4C's states draw it: 0x675F98 five bytes a row, 0x675F8C the row)
// ===========================================================================

// ===========================================================================
// CommuDraw: 0x652A70[7] (CommuDraw_States 0x652DC4, by 0x939A3E)
// ===========================================================================

// ===========================================================================
// CommuName: 0x652A70[8] (CommuName_States 0x652E48, by 0x939A3E)
// ===========================================================================

#pragma clang attribute pop

void Rest4D_Inject() {
    if (bof3::WantsShadow("rest_4d")) rest_4d::SelfTest();
    // DIVERGENCE DIV-0075: after the self-test, which compares Capcom's entry steps.
    {
        static const std::uint8_t was = 0, is = 1;
        bof3::PatchBytes("CommuNameEntryAbandons",
                         Low32(&rest_4d::g_entry_abandons), &was, &is, 1);
        bof3::Log("DIV-0075    the community's name entry ends unanswered where the original never leaves its step");
    }
    if (bof3::WantsShadow("rest_4d")) rest_4d::EntryAbandonTest();   // DIV-0075's row: the switch on, before the inject
}

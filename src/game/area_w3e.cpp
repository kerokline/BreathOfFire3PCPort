// World 3's areas 136 and 139..142: the PSX's BIN/WORLD03/AREA136 and
// AREA139..142.EMI compiled into the exe at 0x41EFE0..0x420800
// (Area_Descriptors entries 136..142; 137 and 138 have no code). Round ten,
// group AR3E: the band's 52 functions, none ours before, each read to its last
// instruction with capstone (2026-09-28) and taken through the area harness
// (area_harness.h). docs/area_w3e.md.
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. Reads
// by an unchecked index into the areas' .data tables are kept where they stay
// in .data; the dispatchers through the state tables of areas 140, 141 and 142
// abort past the table (what follows is data or another table, which the
// original would jump through), docs/area_w3e.md section 6. Every call goes
// through the harness (AH_CALL), so the start-up fuzz can stand recorders in
// for ours as for the originals' copies; the callees of the group's own are
// called the same way, so each function is fuzzed alone.
#include "game/area_w3e.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w3e_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w3e::at;
using area_harness::Handler;
using area_harness::Mem;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using U = std::uint32_t;

unsigned char& B(U address) { return *Mem(address); }
void AddWord(unsigned char* p, int v) { SetWord(p, static_cast<unsigned>(Word(p) + v)); }

void SetMessage(unsigned id) { SetWord(Mem(::bof3::Img(at::kMessage.v)), id); }
unsigned char* StoryFlags() { return Mem(::bof3::Img(Cond_Flags_addr) + 0xa0); }
// The movement script's position (MoveScript_Object's word +0xA) moved by `v`.
void ScriptStep(int v) { AddWord(MoveScript_Object + 0xA, v); }

// A state handler read from an area's .data table in place, as the
// originals' `jmp [index * 4 + table]`: the index is not checked there. Ours
// aborts past the table (what follows is data or the next table).
Handler StateEntry(const char* who, U table, unsigned count, unsigned index) {
    if (index >= count)
        bof3::Fatal("%s: state %u is past its %u-entry table 0x%X", who, index, count, static_cast<unsigned>(table));
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(table + index * 4u)))));
}

// The first of `count` entries of `stride` bytes at `table` whose x byte is
// x's low byte, whose z byte is z's, and whose direction (the third byte's low
// nibble) is the leader's direction byte (the whole byte: 0x10 or more matches
// none); `count` when none - the search the three cell hooks open with.
unsigned FindCell(U table, unsigned count, unsigned stride, long x, long z) {
    const unsigned char pose = B(::bof3::Img(ObjTrio_addr) + 0x8);
    for (unsigned i = 0; i < count; ++i) {
        const unsigned char* const e = Mem(table + i * stride);
        if (e[0] == static_cast<unsigned char>(x) && e[1] == static_cast<unsigned char>(z) && (e[2] & 0xF) == pose) return i;
    }
    return count;
}

}  // namespace

// ===========================================================================
// Area 136 (descriptor 0x62EA30; PSX 0x801F43B4): fifteen choices, eleven
// handlers (choice 4 + n is handler n), tail kind 19, the init. Choice 13 =
// handler 9 is 0x42A4B0, another block's.
// ===========================================================================

// ===========================================================================
// Area 139 (descriptor 0x62ED78; PSX 0x801F45A0): one handler, the cell hook.
// ===========================================================================

// ===========================================================================
// Area 140 (descriptor 0x62EF98; PSX 0x801F4A28): one choice (= handler 2),
// three handlers, tail kind 41, the step hook, the cell hook, object trigger
// 18, effect kind 0x71 and its three states.
// ===========================================================================

// ===========================================================================
// Area 141 (descriptor 0x62F5A0; PSX 0x801F4D50): six choices (choice 2 + n
// is handler n; choice 1 is 0x403050, world 0's), four handlers, two state
// tables, the init, tail kind 52, the cell hook, object trigger 57, the
// cell-run painter, and five object placers chapter 14's code calls.
// ===========================================================================

// ===========================================================================
// Area 142 (descriptor 0x62FC98; PSX 0x801F3B38): one handler and its two
// states.
// ===========================================================================

void AreaW3e_Inject() {
    if (bof3::WantsShadow("area_w3e")) area_w3e::SelfTest();
}

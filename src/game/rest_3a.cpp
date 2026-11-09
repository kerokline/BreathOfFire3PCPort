// Round fourteen group R3A (wave three; docs/takeover-queue-round14.md,
// analysis/round14_cut.tsv's R3A rows): 45 functions of the band
// 0x404180..0x4378AA - the cut's 44 and one start it lacks (0x4041B0) - each
// read to its last instruction with capstone (2026-10-04). docs/rest_3a.md has
// every function one row each.
//
//   - area 33's world-map frame states 1..3 (WorldMap33_FrameStates), which
//     our WorldMap_FrameStep carries inline: the table still holds them;
//   - BATE's root (game mode 9's 0x42D710 and its two sub-dispatchers by
//     0x929F00..02, its start, its way out, the transition step) and the
//     equipment screen's three helpers (its windows, the commit, the frame's
//     refresh of the six chosen bytes);
//   - the battle's end: the loss way's dispatcher and two steps
//     (BattleEnd_Steps[2]), the third way's dispatcher and two steps
//     (BattleEnd_Steps[3]), BattleEnd_ExitSteps[3];
//   - battle-task kind 0's slots 2 (a flash), 4 (a tint), 5 (an animation),
//     11 and 12 (a member shrunk, its DAT loaded, grown back) with their
//     states, and the actor watch's states 1 and 4; kind 3's dispatcher
//     (BattleBossFx_Dispatch);
//   - the two engine helpers that run BattleEnemy_SetAnimation on an enemy
//     by its battle index, and New Game's character records.
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. The
// dispatchers abort past their tables where the original jumps or calls
// through whatever follows (round9 doc section 6); the writes through an
// unchecked index abort past what they index (docs/rest_3a.md section 7); the
// reads through one are kept. Every call goes through the boss harness
// (BH_CALL / BH_AT / Phase), so the start-up fuzz can stand recorders in.
#include "game/rest_3a.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/boss_harness.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_3a_callees.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = rest_3a::at;
using U = std::uint32_t;
using boss_harness::Phase;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

unsigned char& B(U address) { return move_script::At(address)[0]; }
U LRest3a(U address) { return static_cast<U>(move_script::Long(move_script::At(address))); }
// Sprite_Current, the running task slot (0x93B8C4) and its owner (0x93B940),
// each read afresh where the original reads it.
unsigned char* Sc() { return Sprite_Current; }
unsigned char* Slot() { return rv::Bytes(BattleFlow_TaskCurrent.Native()); }

// jmp [table + 4 * index]: the entry as read (the fuzz swaps the table's
// cells for its recorders), a Fatal past `entries` (the original jumps
// through the dword after). The jmp leaves the caller's stack word and the
// entry's eax in place: ours hands the word on and answers the entry's eax
// (round 11 doc section 5.1).
using Entry = unsigned long (__cdecl*)(unsigned long);
unsigned long DispatchRest3a(const char* who, U table, unsigned index, unsigned entries, unsigned long through) {
    if (index >= entries)
        bof3::Fatal("%s: the step byte is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/rest_3a.md section 7)",
                    who, index, entries, (unsigned)::bof3::VaOf(table));
    return reinterpret_cast<Entry>(static_cast<std::uintptr_t>(LRest3a(table + 4 * index)))(through);
}

}  // namespace

// ============================================================================
// Area 33's world-map frame (WorldMap33_FrameStates 1..3)
// ============================================================================

// ============================================================================
// BATE (game mode 9): the root and its sub-dispatchers
// ============================================================================

// ============================================================================
// BATE's equipment screen: the three helpers BE1's steps call
// ============================================================================

// ============================================================================
// The battle's end: BattleEnd_Steps[2] (the loss) and [3], ExitSteps[3]
// ============================================================================

// ============================================================================
// Battle-task kind 0, slot 2: a flash over the screen
// ============================================================================

// ============================================================================
// Kind 0, slot 4: a tint over the screen
// ============================================================================

// ============================================================================
// Kind 0, slot 5: an animation of bank 0x18
// ============================================================================

// ============================================================================
// The actor watch's states 1 and 4 (BattleFx_ActorWatch's stack table)
// ============================================================================

// ============================================================================
// Kind 0, slots 11 and 12: a member shrunk, its DAT loaded, grown back
// ============================================================================

// ============================================================================
// Kind 3: BattleBossFx_Dispatch
// ============================================================================

// ============================================================================
// An enemy's animation by its battle index
// ============================================================================

// ============================================================================
// New Game
// ============================================================================

// ============================================================================

void Rest3A_Inject() {
    if (bof3::WantsShadow("rest_3a")) rest_3a::SelfTest();
}

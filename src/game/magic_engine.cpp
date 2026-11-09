// The engine-side rows of Magic_Rows (docs/takeover-queue-round9-spells.md
// section 3): the five rows whose file is 0xFFFF have their handler in the
// battle engine, not in a BMAGIC overlay. Read one id down (the names are
// hypotheses, docs/cut-content.md section 2):
//
//   - rows 0 and 126, 0x4378D0 (row 126: id 0x7F, TCRF's Assault): a hold of
//     60 frames, then flag 0x40 on the target and the effect done;
//   - row 108, 0x4525F0 (id 0xD9, Restore Form): the acting member's state
//     byte set to 6 (the reaction state) with sub-states 4 and 4, then a wait
//     until no actor is pending;
//   - row 123, 0x43F3B0 (id 0x8B, TCRF's Paralyzer): the current enemy's cue,
//     the caster's animation 2 to its end, flag 0x40 on the target;
//   - row 128, 0x43FC80 (id 0x80, TCRF's Head Cracker): the caster's
//     animation 2, a sound, then three rocks dropped on the target (the kind-1
//     child 0x43FE90, parameter 0x5D), each followed by a wait for the target
//     to leave state 6.
//
// Faithful, latent defects included (docs/magic_engine.md section 6): row
// 128's waits have no limit, kept as Capcom wrote them (a fix is a divergence
// for the owner to choose). Row 123 read a word through the current enemy's
// +0xF8 unchecked (only the event-battle set-ups write that pointer) and
// crashed in an ordinary battle - the spell sweep reached it 2026-10-26 - so
// since DIV-0183 a null pointer plays no cue and the skill goes on; a written
// pointer takes Capcom's path unchanged.
//
// Every call goes through the harness (MH_CALL / MH_AT / Phase), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies.
// One divergence, DIV-0183 above; otherwise each is a faithful replacement,
// except that a phase past a stack table aborts where the original would
// call through its own stack (docs/magic_fx_reached.md section 3, the
// precedent).
#include "game/magic_engine.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = magic_harness::at;
using magic_harness::EnemyOf;
using magic_harness::Mem;
using magic_harness::PartyOf;
using magic_harness::Pointer;
using move_script::Long;
using move_script::SetWord;
using move_script::Word;

constexpr unsigned char kReacting = 6;

void Bump(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

// The effect-done bit: `or byte [0x904AA8], 4`.
void SetDone() { Mem(::bof3::Img(magic_harness::at::kFlags))[0] = static_cast<unsigned char>(Mem(::bof3::Img(magic_harness::at::kFlags))[0] | 4); }

// The task slot's owner (0x93B8C4's +0x80), read at the point of the call.
unsigned char* SlotOwner() { return Pointer(static_cast<std::uint32_t>(Long(Mem(::bof3::Img(BattleFlow_TaskCurrent_addr)))) + 0x80); }

// A stack-table dispatcher: the phase byte through the table the original
// builds on its stack, unchecked there; past the table ours aborts.
void DispatchMagicEngine(const char* who, const std::uint32_t* phases, unsigned n, unsigned phase) {
    if (phase >= n) bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, n);
    magic_harness::Phase(phases[phase])();
}

}  // namespace

#define MENGINE_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// Rows 0 and 126: the hold
// ===========================================================================

// ===========================================================================
// Row 108: Restore Form (read one id down)
// ===========================================================================

// ===========================================================================
// Row 123: Paralyzer (TCRF's name for id 0x8B)
// ===========================================================================

// ===========================================================================
// Row 128: Head Cracker (TCRF's name for id 0x80)
// ===========================================================================

// ===========================================================================
// The rock: BattleMagicFx_Dispatch's kind 1, parameter 0x5D
// ===========================================================================

void MagicEngine_Inject() {
    if (bof3::WantsShadow("magic_engine")) magic_engine::SelfTest();
}

// Group S27's calls into code and cells that are not ours and not named:
// other units' phases and an engine helper, called by their raw addresses
// (docs/magic_s27.md section 3). Whoever takes one of these gives it a name;
// until then the fuzz re-aims each at a recorder by this address.
#pragma once

#include <cstdint>

namespace magic_s27::raw {

// MAGIC041's phase (group S08, not yet ours): +1 on once +0xB is 1 - entry 2
// of Burn_Task's stack table (Burn waits for its last flame).
constexpr std::uint32_t kWaitOneChild = 0x4A6640;

// The engine's turn of a task's dx / dz pair (+0xC / +0x10) by its direction
// byte +8 (unnamed, in no group; docs/magic_s22.md section 3).
constexpr std::uint32_t kTurnOffset = 0x446770;

// MAGIC122's phases (group S28): the last three entries of
// WhelpBreathBeam_Phases, one body the linker kept for MAGIC120, 121 and 122.
constexpr std::uint32_t kBeamSweep = 0x4DDC30;    // +0xB down, +9 up; at 0x18 the target flags 0x10, +2 on
constexpr std::uint32_t kBeamHold = 0x4DDC70;     // +9 up; at 0x78 +2 on
constexpr std::uint32_t kBeamFade = 0x4DDC90;     // +0xB up, +4 down, +9 up; at 0x88 the owner's +0xB 0xFF, freed

// The sprite bank the Sprite_* functions read their animations from
// (0x9039D8, a pointer: 0x8B3580 the default, 0x8E3580 the battle effects'
// bank - battle_fx_tasks_callees.h, magic_s24_callees.h).
constexpr std::uint32_t kSpriteBank = 0x9039D8;
constexpr std::uint32_t kBankEffects = 0x8E3580;
constexpr std::uint32_t kBankDefault = 0x8B3580;

}  // namespace magic_s27::raw

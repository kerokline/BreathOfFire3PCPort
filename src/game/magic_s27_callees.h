// Group S27's calls into code and cells that are not ours and not named: an
// engine helper, called by its raw address, and cells (docs/magic_s27.md
// section 3). The other units' phases once here are ours now and named in the
// tables: S08's Berserk_WaitChildren (entry 2 of Burn_Task's stack table) and
// S28's BreathBeam_Hit / _Hold / _Fade (the last three entries of
// WhelpBreathBeam_Phases).
#pragma once

#include <cstdint>

namespace magic_s27::raw {

// The engine's turn of a task's dx / dz pair (+0xC / +0x10) by its direction
// byte +8 (unnamed, in no group; docs/magic_s22.md section 3).
constexpr std::uint32_t kTurnOffset = 0x446770;

// The sprite bank the Sprite_* functions read their animations from
// (0x9039D8, a pointer: 0x8B3580 the default, 0x8E3580 the battle effects'
// bank - battle_fx_tasks_callees.h, magic_s24_callees.h).
constexpr std::uint32_t kSpriteBank = 0x9039D8;
constexpr std::uint32_t kBankEffects = 0x8E3580;
constexpr std::uint32_t kBankDefault = 0x8B3580;

}  // namespace magic_s27::raw

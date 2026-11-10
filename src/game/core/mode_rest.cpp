// Group PM of the platform round (step 2, 2026-10-05; docs/mode-rest.md): game
// modes 3..6 - GameMode_Handlers' four entries no catalogue held - with the
// steps of modes 3 and 5 not already ours, and the jump 0x587C20 under mode 3.
// Each read with capstone to its last instruction (2026-10-05):
//
//   GameMode3_Run    0x495BB0  mode 3, the field menu: GameMode3_Steps by Game_Step
//   GameMode3_Enter  0x495BC0  its step 0
//   GameMode3_Leave  0x495C50  its step 2 (step 1 is Menu_Frame, ours)
//   GameMode4_Run    0x495E60  mode 4: the scripted field frame until the message bit
//   GameMode5_Run    0x495E90  mode 5, the battle: GameMode5_Steps by Game_Step
//   GameMode5_TurnSense .. GameMode5_Leave  0x495EA0 .. 0x496150  its steps 0..4, 6, 7
//                              (step 5 is Battle_Frame, ours)
//   GameMode6_Run    0x496230  mode 6: Look_PadControl or GameMode_LookEnd, a loading frame
//   (Sound_MusicPlaying 0x587C20, a jmp to Music_IsPlaying, is group PS's - sound_rest.cpp)
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies; a memory cell
// the original reads after a call is read after it here. No divergence: each
// is a faithful replacement. Where the original jumps through a step table
// past its end, ours aborts with a message (docs/mode-rest.md section 6).
#include "game/mode_rest.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/mode_flow.h"
#include "game/mode_rest_callees.h"
#include "game/window_task.h"
#include "game/music_resume.h"
#include "game/sound_rest.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = mode_rest::at;
using move_script::SetLong;

}  // namespace
// ===========================================================================
// Mode 3, the field menu (Field_Request 1, GameMode_Field)
// ===========================================================================

// ===========================================================================
// Mode 4 (Field_Request 2: a message open)
// ===========================================================================

// ===========================================================================
// Mode 5, the battle (Field_Request 3)
// ===========================================================================

// ===========================================================================
// Mode 6 (Field_Request 4)
// ===========================================================================

// ===========================================================================
// 0x587C20: jmp Music_IsPlaying
// ===========================================================================

// Where the jump goes: Music_IsPlaying's address (Capcom's 0x5A7020, or the
// jump Inject put there to ours), or its recorder while the fuzz runs ours.
// Sound_MusicPlaying 0x587C20 (a five-byte jmp to Music_IsPlaying) is group PS's,
// src/game/sound_rest.cpp, taken the same night; GameMode3_Enter / _Leave call it
// through SH_CALL by name.

void ModeRest_Inject() {
    if (bof3::WantsShadow("mode_rest")) mode_rest::SelfTest();
}

// The sound-layer round's group ST (docs/sound-layer-round.md): repairs of
// the port's DirectSound layer where the PlayStation cuts sound the port lets
// run on.
//
//   DIV-0174 (StopCuts): Music_FadeOutStop's twin on both PlayStation discs is
//   a stop (SsSepStop, the voices released at once), at every one of its 131
//   call sites; the port fades. With the switch, the stream is stopped at the
//   call; every word the game can read is left as Capcom's fade leaves it.
//
// All state is the DLL's: nothing here writes game memory, so neither the
// state hash nor a save file sees it.
#pragma once

#include <cstdint>

namespace sound_st {

// DIVERGENCE DIV-0174: 1 through PatchBytes("StopCuts") at the end of
// Sound_Inject, after every self-test there (they compare Capcom's fade).
extern std::uint8_t g_stop_cuts;

// Music_FadeOutStop's cut, at the end of its body with the switch set and
// frames above 0: the stream stopped now (Music_Stop, through the sound
// layer's callees - on the sequenced path the synth's SsSepStop with it).
// Capcom's fade state is left exactly as the fade set it, so Music_Track
// still goes 0xFF at the first Sound_Tick and the fade still ends with its
// own stop (a second Music_Stop, which finds nothing playing).
void CutStop(int frames);

// Sound_Inject's end: the switch armed (BOF3X_ORIGINAL=StopCuts leaves it).
void ArmStopCuts();

}  // namespace sound_st

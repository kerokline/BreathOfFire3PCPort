// The sound layer's last seven - the music stopped, resumed and asked about,
// everything paused, a voice's volume, the SND stream asked about - and the
// sound set-up Game_Init calls, Snd_Init. Group PS of the platform round.
// docs/sound-rest.md.
#pragma once

void SoundRest_Inject();

// The nine, for callers by name (the game-mode steps of mode_rest call
// Sound_MusicPlaying): naked thunks and cdecl bodies in sound_rest.cpp.
extern "C" void __cdecl Sound_StopMusic(void);
extern "C" void __cdecl Sound_ResumeAll(void);
extern "C" int __cdecl Sound_MusicPlaying(void);
extern "C" void __cdecl Sound_PauseAll(void);

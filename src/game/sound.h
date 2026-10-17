// The sound layer: the sound effects (Sound_PlayEffect and the DirectSound
// buffer helpers under it), the music (Music_Play, the fades, the streaming
// MP3 buffer and its pump) and the per-loop tick WinMain spins on. The PC
// port's own driver over DirectSound; the MP3 decoder under it stays
// Capcom's. docs/sound.md.
#pragma once

void Sound_Inject();

namespace sound {

// DIVERGENCE DIV-0132: the PlayStation's duck - SsSepSetDecrescendo(0, 0,
// 0x14, 0x10), the song lowered 16 of its 97 sequence-volume units, about
// -3.1 dB - as a fade over `frames` logic frames (DIV-0028's clock) to the
// same loudness in whichever volume law the music plays under (the MP3's
// linear-in-decibels level, the sequencer's libsnd square law). The volume
// before it is kept for UnduckMusic.
void DuckMusic(int frames);
// The duck's undo - the PlayStation's SsSepSetCrescendo(0, 0, 0x14, 0x10) -
// a fade over `frames` back to the volume DuckMusic found; with no duck
// outstanding, Music_FadeIn's fade to 127.
void UnduckMusic(int frames);

}  // namespace sound

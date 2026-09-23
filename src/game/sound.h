// The sound layer: the sound effects (Sound_PlayEffect and the DirectSound
// buffer helpers under it), the music (Music_Play, the fades, the streaming
// MP3 buffer and its pump) and the per-loop tick WinMain spins on. The PC
// port's own driver over DirectSound; the MP3 decoder under it stays
// Capcom's. docs/sound.md.
#pragma once

void Sound_Inject();

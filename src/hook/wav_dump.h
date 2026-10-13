// BOF3X_WAV_DIR: the game's audio mix written to a WAV file, one per run
// (docs/wav-dump.md). Instrumentation, off unless the variable is set:
// nothing is patched without it, and with it nothing the game computes or
// hears changes - DirectSound still plays every buffer; the dump only watches
// the calls the game makes on them and mixes its own copy.
//
// The mix is DirectSound's, not ours: the game hands each sound effect and
// the music ring to DirectSound as separate secondary buffers and Windows
// sums them (src/game/sound.cpp, docs/sound.md). So the tap is the hand-over
// itself - every buffer's data at Unlock and its Play / Stop /
// SetCurrentPosition / SetVolume / SetPan / SetFrequency / Release - and the
// dump sums those buffers as DirectSound does, clocked by the game's logic
// frames (one frame of the pace in force, DIV-0047's 1001 / 30 ms or the
// port's 33.334 under Capcom's WinMain) so two runs of a recipe line up frame
// for frame. Output: 44,100 Hz, 16-bit, stereo PCM.
#pragma once

namespace bof3 {

// Reads BOF3X_WAV_DIR (and BOF3X_WAV_LABEL); when set, puts the dump on the
// DirectSoundCreate import slot 0x5C4014. Call after InjectAll and before
// InputScript_Start - its latch decides whether Capcom's WinMain needs the
// dump's own latch (InputScript_Start reads WavDump_On).
void WavDump_Start();

// True when BOF3X_WAV_DIR is set.
bool WavDump_On();

// Once a pass of WinMain's loop, from the input latch: every logic frame
// before Frame_Counter's current one is mixed and written.
void WavDump_Tick();

// DLL_PROCESS_DETACH: the frames reached so far, and the header. The header
// is rewritten after every write, so a run ended by TerminateProcess (the
// runners' way) leaves a valid file too, short only the last few frames.
void WavDump_Stop();

}  // namespace bof3

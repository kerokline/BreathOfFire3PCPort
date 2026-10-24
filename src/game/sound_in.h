// DIVERGENCE DIV-0169 (the sound-layer round's group IN, known-defects.md
// D343): the inn's night pauses the area's song and resumes it where it
// stopped, as the PlayStation's SHOP.EMI#0 does, instead of letting the
// jingle's stream release it for good.
//
// The PlayStation (US SHOP.EMI#0; JP the same, every masked word): night
// state 0's last frame, after Transition_Start(0), tests SEP slots 0..3 for
// playing, records each answer at 0x801E6100 + 2i and SsSepPauses each that
// plays (US 0x801D41EC); night 2 plays the jingle on the XA stream beside the
// paused sequence; the restore state (US 0x801D446C, JP 0x801D447C) ends,
// after Party_RestoreAll(1), the answer byte, the repeat latch and the
// message, with SsSepReplay(0, i) for each slot recorded as playing.
//
// The PC (InnNight_Message 0x57FB70 .. InnNight_Restore 0x57FCB0,
// shop_states.cpp) plays its jingle (SND\OYASUMI, kind 0) through Music_Start
// on the music's own ring, which releases the area's stream; Music_Track
// stays the area's, so nothing plays it again until an area with another
// track (measured on shop.txt, 2026-10-23: no music call after the jingle's
// end at f2714 to the recipe's end at f3157).
//
// Ours, with the switch: at night 0's end the song heard is taken
// (music_resume::Heard - DLL state only) and the ring halted (Sound_StopMusic,
// Music_Halt: DirectSound's Stop, position kept); the jingle plays as before;
// at the restore's end, when the area's track is still Music_Track and still
// the file in Music_File, the stream is started again from that file with the
// position pending (music_resume::Pend, the DIV-0092 resume's fast-forward:
// the MP3 decoded from the top and dropped, or the sequenced song's
// FastForward), at the volume the game holds (Music_Volume, which the jingle's
// Music_SetVolume(127) never stored). No fade either side, as SsSepPause and
// SsSepReplay have none. Nothing is waited on: both are calls inside the
// existing states' frames.
#pragma once

#include <cstdint>

namespace sound_in {

// 1 through PatchBytes("InnNightPause") at the end of sound_in::Inject, after
// the shop_states fuzz and this file's self-test (both run with it 0).
extern std::uint8_t g_inn_pause;

// InnNight_Message (night 0), after Transition_Start(0): the song taken and
// halted. Nothing with the switch off or no song playing.
void NightFalls();

// InnNight_Restore (night 4), after the message: the song taken at nightfall
// started again at its position. Nothing with the switch off, nothing taken,
// or the area's track changed meanwhile. The taken song is forgotten either way.
void MorningComes();

// ShopStates_Inject's tail: under BOF3X_SHADOW=shop_states or sound_in the
// self-test (recorders for the five calls out, both branches and the refusals),
// then the switch armed and logged.
void Inject();

}  // namespace sound_in

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

// DIVERGENCE DIV-0177 (known-defects.md D356): the area's song comes back
// after a jingle, as on the PlayStation, where the 11 kind-0 jingles are XA
// from the CD beside the sequence: the stream start (JP 0x80164090) calls the
// CD library only, never SsSepPause, SsSepStop or the stop wrappers, so the
// song plays on under the jingle. The PC plays the jingle through Music_Start
// on the music's own ring, which releases the song for good while Music_Track
// still names it - so every later Music_Play of that track is a no-op and the
// area stays silent until an area with another track (issue 120: Genmel after
// the Flier, ITEM01).
//
// Most of the PC's jingle sites bracket the jingle themselves: Sound_StopMusic
// (Music_Halt, the song stopped with its position kept), Sound_LoadStream,
// the wait on Sound_StreamDone, Sound_ResumeAll (Music_Resume) - Capcom's
// intent that the song comes back. It cannot: the jingle's Music_Start
// replaced the song on the one ring, so Music_Resume plays the jingle's spent
// buffer again (measured on genmelAndLift, 2026-10-24: Music_Halt and the
// stream at f5719, Music_Resume at the jingle's end f5947, the pump's stop
// again at f5997). A few sites play a jingle with the song still playing.
//
// Ours, with the switch: as the jingle starts, the song on the ring is taken
// (music_resume::Kept: track and sample, halted or playing; DLL state only).
// A halted song is started again when the game's own Music_Resume comes and
// the jingle has ended - in place of the Play on the jingle's buffer - so the
// scene's timing is Capcom's. A song still playing is started again the
// first logic frame Sound_Tick finds the jingle stopped, and from then until
// the next stream Sound_StreamDone answers 1 for kind 0, so a wait on the
// jingle ends as it did and never waits on the resumed song. Either way the
// song restarts from Music_File at the taken sample, at the game's volume,
// the way DIV-0169's morning does - when the area's track is still
// Music_Track and still the file in Music_File. The song comes back after the
// jingle, not under it: the PC has one music ring.
// Nothing is taken while DIV-0169's night holds the song (its morning
// resumes it) or with no song heard; nothing is resumed when the game has
// played or stopped other music meanwhile. A quicksave mid-jingle keeps no
// taken song (DLL state), so its load leaves the area silent, as before.
extern std::uint8_t g_jingle_resume;

// Sound_LoadStream, any kind, as it starts: the last jingle's state forgotten.
void StreamLoads();

// Sound_LoadStream, kind 0, once its file is open, before a fade is dropped
// and before Music_Start: the song on the ring taken, halted or playing.
// Nothing with the switch off.
void JingleStarts();

// Music_Resume (Sound_ResumeAll's jump), first: for a song the game halted
// before the jingle, once the jingle has ended, the song started again at its
// sample - true, and Music_Resume plays nothing more. False otherwise, and
// Music_Resume is Capcom's.
bool JingleResume();

// Sound_Tick, after the pump: once a logic frame while a song taken playing
// waits, the jingle's end found and the song started again at its sample.
void JingleFrame();

// Sound_StreamDone, kind 0: true from a playing song's resume at the jingle's
// end until the next stream.
bool JingleOver();

// ShopStates_Inject's tail: under BOF3X_SHADOW=shop_states or sound_in the
// self-tests (recorders for the calls out, both branches and the refusals),
// then both switches armed and logged.
void Inject();

}  // namespace sound_in

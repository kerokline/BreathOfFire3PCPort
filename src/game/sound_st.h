// The sound-layer round's group ST (docs/sound-layer-round.md): two repairs
// of the port's DirectSound layer where the PlayStation cuts sound the port
// lets run on.
//
//   DIV-0174 (StopCuts): Music_FadeOutStop's twin on both PlayStation discs is
//   a stop (SsSepStop, the voices released at once), at every one of its 131
//   call sites; the port fades. With the switch, the stream is stopped at the
//   call; every word the game can read is left as Capcom's fade leaves it.
//
//   DIV-0175 (OneShotKeyOff): the PlayStation's SsUtKeyOffV on voices 16..23
//   at a battle action's effect preload releases a one-shot effect still
//   sounding, which DIV-0167 (looping voices only) cannot reach. With the
//   switch, the buffer each effect channel last played as a one-shot is
//   remembered here, DLL-side, and stopped at that point.
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

// DIVERGENCE DIV-0175: 1 through PatchBytes("OneShotKeyOff") in
// BattleActions_Inject, after its self-test (it compares Capcom's preload).
extern std::uint8_t g_one_shot_key_off;

// One remembered one-shot: the bank's voice entry it was played from and the
// buffer that entry held just after SndBuf_Play. 0 / 0: none.
struct OneShot {
    std::uint32_t entry;
    std::uint32_t buffer;
};
constexpr unsigned kChannels = 23;  // Sound_Channels' count
extern OneShot g_one_shots[kChannels];

// Sound_PlayEffect, after a voice that does not loop has been played on
// `channel` (0..22) from the voice entry at `entry`: with the switch set, the
// channel remembers it, replacing what it remembered. Nothing otherwise.
void NoteOneShot(std::uint32_t channel, std::uint32_t entry);

// BattleAction_EffectPreload, with the switch set: for voices 16..23 in order
// (channels 16..22 and 0, Sound_PlayEffect's wrap), the one-shot each channel
// remembers is forgotten and its buffer stopped (SndBuf_Stop: only if it is
// playing) when its voice entry still holds that buffer - a bank reloaded
// since has released it - and no channel of Sound_Channels holds it, since a
// looping voice is DIV-0167's to stop.
void KeyOffOneShots();

// BattleActions_Inject, after its self-test: the switch armed.
void ArmOneShotKeyOff();

}  // namespace sound_st

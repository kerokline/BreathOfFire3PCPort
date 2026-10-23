// The PlayStation's positional sound on the PC's flat effects (known-defects
// D345; the sound-layer round's group SP, docs/sound-layer-round.md): three
// hooks the port emptied, each a divergence of its own, each restored here
// on DLL-side state only - no game word but `Music_Track` (DIV-0170, below)
// is written that the PC did not write.
//
//   DIVERGENCE DIV-0170 (switch MoveB8Restart): the movement script's op B8
//     restarts the song from its top at the script's level, no fade.
//   DIVERGENCE DIV-0171 (switch SpriteVoicePosition): Sound_PlayById's cue
//     follows the sprite that played it - its voices' volume and pan set by
//     that sprite's place, each frame the sprite is drawn, until they end.
//   DIVERGENCE DIV-0172 (switch SoundLevelGate): the field cues that carry a
//     level (op BC, the scenario's three) play at it, and the ones gated by
//     distance (ops BD and D1) play only near their object, quieter further.
//
// Each switch is a byte, 0 at build and during every module's self-test,
// flipped by Arm() through bof3::PatchBytes after this module's own
// self-test (BOF3X_SHADOW=sound_sp); BOF3X_ORIGINAL=<switch> keeps the PC's.
#pragma once

#include <cstdint>

namespace sound_sp {

extern std::uint8_t g_b8;        // DIV-0170
extern std::uint8_t g_position;  // DIV-0171
extern std::uint8_t g_level;     // DIV-0172

// DIV-0170: op B8 b1 b2 b3 (MoveScript_GroupB 0x577280, where the PC calls
// Port_DroppedCall 0x4DF820 with b2).
void OpB8(unsigned char group, unsigned char sequence, unsigned char level);

// DIV-0171: Sound_PlayById's cue as the PlayStation's 0x8015D71C plays it -
// flat, then remembered under Sprite_Current for UpdatePositional.
void PlayPositional(unsigned short id);
// DIV-0171: the PlayStation's 0x80161454 for Sprite_Current, where the PC's
// Sprite_UpdateScreen and Sprite_QueueOverlay call Port_DroppedCall.
void UpdatePositional();

// DIV-0171 / DIV-0172: Sound_PlayEffect is about to play `buffer` (a bank
// entry's DirectSound buffer, shared by every cue that names the voice). A
// buffer whose volume or pan this module changed gets back what it had; a
// level play in progress (PlayLevel, PlayNear) puts its own on it instead.
void BeforePlay(void* buffer);

// DIV-0172 (falls back to the PC's flat Sound_PlayEffect with the switch
// off): the PlayStation's 0x8015D7C8 (op BC and Scena01_Scene09's three
// calls) - the cue at the level table's entries `left` and `right` - and
// 0x8015D820 (ops BD and D1) - the cue only when the leader is within 22
// cells of object `index`, at the level of the nearest ring.
void PlayLevel(unsigned short id, unsigned left, unsigned right);
void PlayNear(unsigned short id, unsigned index);
// Whether the movement script's BC / BD / D1 and the scenario's three calls
// go through PlayLevel / PlayNear rather than Sound_PlayById: either switch
// on (so that DIV-0171 never makes a level cue positional).
bool Routed();

// After every module's self-test (inject_all.cpp): this module's own
// (BOF3X_SHADOW=sound_sp), then the three switches.
void Arm();

}  // namespace sound_sp

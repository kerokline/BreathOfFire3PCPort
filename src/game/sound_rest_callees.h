// Internal to sound_rest.cpp and sound_rest_fuzz.cpp: the addresses the last
// seven of the sound layer and its set-up touch that are not named in
// symbols.toml, and every call they make - through SoundRest_g, so that the
// start-up fuzz can stand recorders in for them, for the originals' copies and
// for ours alike. The DirectSound vtable offsets and Method<> are the sound
// module's (sound_callees.h). docs/sound-rest.md.
#pragma once

#include <cstddef>
#include <cstdint>

namespace sound_rest {

// --- Addresses without a symbol --------------------------------------------
// DirectSoundCreate's import thunk, `jmp [0x5C4014]` (Imp_DirectSoundCreate,
// DSOUND.dll ordinal 1 by the import directory, 2026-10-05). It sits among
// the MP3 decoder's starts by address only: it is the linker's thunk.
constexpr std::uint32_t kDirectSoundCreate = 0x5ACBBC;
// Sound_LoadStream's kind of the last stream (id >> 12), the dword
// save_menu_callees.h calls kStreamKind: non-zero, the SND stream's buffer
// SndStream_Buffer holds a voice Sound_PauseAll stops.
constexpr std::uint32_t kStreamKind = 0x6BDE40;
// The end of Sound_Channels' 23 dwords, Sound_PauseAll's bound.
constexpr std::uint32_t kChannelsEnd = 0x6BC924;

// --- DirectSound, beyond what sound_callees.h names -------------------------
constexpr unsigned kSetCooperativeLevel = 0x18;   // IDirectSound
constexpr unsigned kSetFormat = 0x38;             // IDirectSoundBuffer
constexpr unsigned long kExclusive = 3;           // DSSCL_EXCLUSIVE
constexpr unsigned long kPrimaryBuffer = 1;       // DSBCAPS_PRIMARYBUFFER
constexpr unsigned long kLooping = 1;             // DSBPLAY_LOOPING
// Snd_Init's answers.
constexpr int kNoDevice = 100, kNoCooperation = 101, kNoPrimary = 102, kNoFormat = 103;

using ComCooperate = long (__stdcall*)(void*, void*, unsigned long);
using ComFormat = long (__stdcall*)(void*, const void*);

struct Callees {
    void (__cdecl* sndbuf_stop)(void*);                    // SndBuf_Stop (ours, sound.cpp)
    void (__cdecl* stream_stop)();                         // SndStream_Stop (ours, battle_items.cpp)
    int (__cdecl* halt)();                                 // Music_Halt: Sound_StopMusic's and Sound_PauseAll's jmp
    int (__cdecl* resume)();                               // Music_Resume: Sound_ResumeAll's jmp
    int (__cdecl* music_playing)();                        // Music_IsPlaying (ours, save_menu.cpp): 0x587C20's jmp
    long (__stdcall* create)(const void*, void**, void*);  // DirectSoundCreate, through its thunk
};
// The thunks' jumps read these three by offset (sound_rest.cpp).
static_assert(offsetof(Callees, halt) == 8 && offsetof(Callees, resume) == 12 && offsetof(Callees, music_playing) == 16,
              "the thunks' jmp operands");
extern const Callees kOriginals;

// BOF3X_SHADOW=sound_rest: the start-up fuzz, sound_rest_fuzz.cpp. Clones
// every original before SoundRest_Inject patches it.
void SelfTest();

}  // namespace sound_rest

// C linkage: the naked thunks jump through it by its assembler name.
extern "C" sound_rest::Callees SoundRest_g;

// The sound layer's last seven and its set-up - group PS of the platform
// round (docs/sound-rest.md). The seven are the music stopped and resumed
// (Sound_StopMusic and Sound_ResumeAll, five-byte jumps to Music_Halt and
// Music_Resume), everything paused (Sound_PauseAll), a voice's volume
// (SndBuf_SetVolume), the SND stream asked whether it plays
// (SndStream_IsPlaying), and 0x587C20 (Sound_MusicPlaying, a jump to
// Music_IsPlaying); the set-up is Snd_Init, Game_Init's DirectSound device
// and primary buffer. Every call out goes through SoundRest_g, which the
// start-up fuzz (sound_rest_fuzz.cpp) points at recorders. The DirectSound
// objects are Capcom's own, made here, not our backend's.
#include "game/sound_rest.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "common/asm_name.h"
#include "game/sound_callees.h"
#include "game/sound_in.h"
#include "game/sound_rest_callees.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace sound {
bool MusicLogOn();  // sound.cpp: BOF3X_MUSIC_LOG
}

using sound::ComCall;
using sound::ComStatus;
// sound::Method is named in full below (source-layout B0, analysis/tu_renames.tsv).

namespace sound_rest {
namespace {

unsigned char* At(std::uint32_t address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }
std::uint32_t Address(const volatile void* p) { return Low32(p); }

}  // namespace

const Callees kOriginals = {
    SndBuf_Stop, SndStream_Stop, Music_Halt, Music_Resume, Music_IsPlaying,
    reinterpret_cast<long (__stdcall*)(const void*, void**, void*)>(static_cast<std::uintptr_t>(::bof3::Code(kDirectSoundCreate.v))),
};

}  // namespace sound_rest

using namespace sound_rest;

extern "C" Callees SoundRest_g = kOriginals;

// Sound_PauseAll's eax: Music_Halt's, which its tail jump hands back. Defined
// under the assembler name of the void declaration symbols.gen.h makes, as
// sound.cpp's five are (common/asm_name.h); the one caller (Game_WndProc,
// ours) does not read it.
extern "C" int __cdecl Sound_PauseAllEax(void) BOF3_ASM_NAME(Sound_PauseAll);

// original 0x5A6FF0 (Music_Stop 0x5A7050 byte for byte): the music stopped if
// it plays. GetStatus into a local nothing initialises - the slot the entry's
// `push ecx` made - so a GetStatus that does not write it leaves the caller's
// ecx to be tested: the naked entry below passes it. eax 0 without a buffer,
// else GetStatus's, or Stop's when it played; Music_Buffer read again for
// Stop.
extern "C" int __cdecl SoundRest_HaltBody(std::uint32_t ecx) {
    if (sound::MusicLogOn()) bof3::Log("music_log   f%u Music_Halt track %u", Frame_Counter, static_cast<unsigned>(Music_Track));
    void* const buffer = Music_Buffer;
    if (buffer == nullptr) return 0;
    unsigned long status = ecx;
    const long got = sound::Method<ComStatus>(buffer, sound::kGetStatus)(buffer, &status);
    if ((status & 1) == 0) return static_cast<int>(got);
    void* const playing = Music_Buffer;
    return static_cast<int>(sound::Method<ComCall>(playing, sound::kStop)(playing));
}

// original 0x5A7200 (Music_IsPlaying 0x5A7020 on the other buffer): 1 if the
// SND stream's buffer plays. 0 without it; else GetStatus into the slot of the
// entry's `push ecx` - the caller's ecx for a GetStatus that does not write
// it, as Music_Halt - and bit 0 of its low byte.
extern "C" int __cdecl SoundRest_StreamPlayingBody(std::uint32_t ecx) {
    void* const buffer = SndStream_Buffer;
    if (buffer == nullptr) return 0;
    unsigned long status = ecx;
    sound::Method<ComStatus>(buffer, sound::kGetStatus)(buffer, &status);
    return (status & 1) ? 1 : 0;
}

void SoundRest_Inject() {
    if (bof3::WantsShadow("sound_rest")) sound_rest::SelfTest();
}

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
#include "game/sound_callees.h"
#include "game/sound_rest_callees.h"
#include "hook/detour.h"
#include "hook/log.h"

using sound::ComCall;
using sound::ComCreate;
using sound::ComPlay;
using sound::ComStatus;
using sound::ComValue;
using sound::Method;

namespace sound_rest {
namespace {

unsigned char* At(std::uint32_t address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }
std::uint32_t Address(const volatile void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
std::uint32_t Dword(std::uint32_t address) {
    std::uint32_t v;
    std::memcpy(&v, At(address), sizeof v);
    return v;
}
void SetDword(std::uint32_t address, std::uint32_t v) { std::memcpy(At(address), &v, sizeof v); }
void SetWord(std::uint32_t address, std::uint16_t v) { std::memcpy(At(address), &v, sizeof v); }

void Release(void* object) { Method<ComCall>(object, sound::kRelease)(object); }

}  // namespace

const Callees kOriginals = {
    SndBuf_Stop, SndStream_Stop, Music_Halt, Music_Resume, Music_IsPlaying,
    reinterpret_cast<long (__stdcall*)(const void*, void**, void*)>(static_cast<std::uintptr_t>(kDirectSoundCreate)),
};

}  // namespace sound_rest

using namespace sound_rest;

extern "C" Callees SoundRest_g = kOriginals;

// Sound_PauseAll's eax: Music_Halt's, which its tail jump hands back. Defined
// under the assembler name of the void declaration symbols.gen.h makes, as
// sound.cpp's five are; the one caller (Game_WndProc, ours) does not read it.
extern "C" int __cdecl Sound_PauseAllEax(void) __asm__("_Sound_PauseAll");

// original 0x587B80: `jmp Music_Halt`. The caller's ecx and stack reach
// Music_Halt as they were: a jump through SoundRest_g, not a call.
extern "C" __attribute__((naked)) void __cdecl Sound_StopMusic(void) { __asm__ volatile("jmp *(_SoundRest_g+8)"); }

// original 0x587B90: `jmp Music_Resume`, the same way.
extern "C" __attribute__((naked)) void __cdecl Sound_ResumeAll(void) { __asm__ volatile("jmp *(_SoundRest_g+12)"); }

// original 0x587C20: `jmp Music_IsPlaying`, the same way - Music_IsPlaying
// reads the ecx it is handed (save_menu.cpp). Its callers are Game_Mode 3's
// steps 0x495BC0 and 0x495C50, still Capcom's (docs/sound-rest.md section 5).
extern "C" __attribute__((naked)) int __cdecl Sound_MusicPlaying(void) { __asm__ volatile("jmp *(_SoundRest_g+16)"); }

// original 0x587C30: everything paused - WndProc's on a deactivation and on
// F9. Each of the 23 Sound_Channels dwords that is not 0, SndBuf_Stop (the
// dwords kept, unlike Sound_StopChannels'); with a stream kind above 0, the
// SND stream's buffer SndStream_Stop stops and releases; then the music,
// Music_Halt, whose eax is the answer.
// As the original has it: each dword read afresh, after the call before it;
// the kind read after the walk. The original's tail jump hands Music_Halt the
// ecx its last callee left (or its own caller's): ours hands it what ours
// leaves - a value no one set either way, read only where DirectSound's
// GetStatus does not write its out (section 3).
int Sound_PauseAllEax(void) {
    for (std::uint32_t p = Address(Sound_Channels); p < kChannelsEnd; p += 4) {
        const std::uint32_t buffer = Dword(p);
        if (buffer != 0) SoundRest_g.sndbuf_stop(At(buffer));
    }
    if (Dword(kStreamKind) != 0) SoundRest_g.stream_stop();
    return SoundRest_g.halt();
}

// original 0x5A6C60: a voice's volume - level 0..127 as DirectSound
// hundredths of a decibel, fild(level) * (1 / 127.0) * 10000.0 - 10000.0,
// truncated as the CRT's _ftol 0x5B9550 does (round toward zero in a copy of
// the control word for one fistp to 64 bits; the low dword). The x87 as the
// original, on the image's own floats, so it rounds as the original under any
// control word. Nothing for a null buffer.
// As the original has it: the level is not clamped - above 127 asks for more
// than 0, below 0 for less than -10000, which DirectSound refuses
// (DSERR_INVALIDPARAM) and the original does not look at.
extern "C" void __cdecl SndBuf_SetVolume(void* buffer, int level) {
    if (buffer == nullptr) return;
    std::int64_t value;
    unsigned short saved, truncating;
    __asm__ volatile(
        "fildl %[level]\n\t"
        "fmuls %[inverse]\n\t"
        "fmuls %[scale]\n\t"
        "fsubs %[scale]\n\t"
        "fnstcw %[saved]\n\t"
        "movw %[saved], %%ax\n\t"
        "orb $0x0C, %%ah\n\t"
        "movw %%ax, %[truncating]\n\t"
        "fldcw %[truncating]\n\t"
        "fistpll %[value]\n\t"
        "fldcw %[saved]\n\t"
        : [value] "=m"(value), [saved] "=m"(saved), [truncating] "=m"(truncating)
        : [level] "m"(level), [inverse] "m"(*reinterpret_cast<const float*>(sound::kInverse127At)),
          [scale] "m"(*reinterpret_cast<const float*>(sound::k10000At))
        : "eax", "st");
    Method<ComValue>(buffer, sound::kSetVolume)(buffer, static_cast<unsigned long>(static_cast<std::uint64_t>(value)));
}

// original 0x5A6FF0 (Music_Stop 0x5A7050 byte for byte): the music stopped if
// it plays. GetStatus into a local nothing initialises - the slot the entry's
// `push ecx` made - so a GetStatus that does not write it leaves the caller's
// ecx to be tested: the naked entry below passes it. eax 0 without a buffer,
// else GetStatus's, or Stop's when it played; Music_Buffer read again for
// Stop.
extern "C" int __cdecl SoundRest_HaltBody(std::uint32_t ecx) {
    void* const buffer = Music_Buffer;
    if (buffer == nullptr) return 0;
    unsigned long status = ecx;
    const long got = Method<ComStatus>(buffer, sound::kGetStatus)(buffer, &status);
    if ((status & 1) == 0) return static_cast<int>(got);
    void* const playing = Music_Buffer;
    return static_cast<int>(Method<ComCall>(playing, sound::kStop)(playing));
}

// original 0x5A7080: the music played again, looping, from where it stopped
// (Play(0, 0, DSBPLAY_LOOPING); no rewind). eax Play's, or 0 without a buffer.
extern "C" int __cdecl Music_Resume(void) {
    void* const buffer = Music_Buffer;
    if (buffer == nullptr) return 0;
    return static_cast<int>(Method<ComPlay>(buffer, sound::kPlay)(buffer, 0, 0, kLooping));
}

// original 0x5A7200 (Music_IsPlaying 0x5A7020 on the other buffer): 1 if the
// SND stream's buffer plays. 0 without it; else GetStatus into the slot of the
// entry's `push ecx` - the caller's ecx for a GetStatus that does not write
// it, as Music_Halt - and bit 0 of its low byte.
extern "C" int __cdecl SoundRest_StreamPlayingBody(std::uint32_t ecx) {
    void* const buffer = SndStream_Buffer;
    if (buffer == nullptr) return 0;
    unsigned long status = ecx;
    Method<ComStatus>(buffer, sound::kGetStatus)(buffer, &status);
    return (status & 1) ? 1 : 0;
}

// The two entries whose originals read the ecx their caller left: each passes
// it to its body, above, as it found it.
extern "C" __attribute__((naked)) int __cdecl Music_Halt(void) {
    __asm__ volatile(
        "push %ecx\n\t"
        "call _SoundRest_HaltBody\n\t"
        "add $4, %esp\n\t"
        "ret");
}
extern "C" __attribute__((naked)) int __cdecl SndStream_IsPlaying(void) {
    __asm__ volatile(
        "push %ecx\n\t"
        "call _SoundRest_StreamPlayingBody\n\t"
        "add $4, %esp\n\t"
        "ret");
}

// original 0x5A6830: the sound set-up, Game_Init's (hwnd Game_Hwnd). The
// DirectSound device, made once - nothing if Snd_Device is already set - in
// exclusive cooperation with the window, and its primary buffer set to
// 22,050 Hz 8-bit stereo PCM. Nothing more: the voices' buffers come with
// each DAT's bank (Snd_LoadBank), the music's stream and the MP3 decoder with
// each track (Music_Start), the SND stream with each stream. 0, or 100..103
// for the step that failed, with what that step had made let go and cleared;
// Game_Init does not read it, and a game without a device plays silent
// (SndBuf_FromWave and Music_Start do nothing without one).
// As the original has it: any non-zero result is a failure (S_FALSE too);
// DirectSoundCreate writes Snd_Device directly, and Snd_Device and Snd_Primary
// are read again after every call; on the format's failure the primary is
// read after the device's Release and before Snd_Device is cleared. The
// format's words are stored around the dwords as the original orders them.
extern "C" int __cdecl Snd_Init(void* hwnd) {
    if (Snd_Device != nullptr) return 0;
    if (SoundRest_g.create(nullptr, &Snd_Device, nullptr) != 0) {
        Snd_Device = nullptr;
        return kNoDevice;
    }
    void* device = Snd_Device;
    if (Method<ComCooperate>(device, kSetCooperativeLevel)(device, hwnd, kExclusive) != 0) {
        Release(Snd_Device);
        Snd_Device = nullptr;
        return kNoCooperation;
    }
    const std::uint32_t desc = Address(Snd_BufferDesc);
    std::memset(At(desc), 0, 0x24);
    SetDword(desc + 0x00, 0x24);             // dwSize
    SetDword(desc + 0x04, kPrimaryBuffer);   // dwFlags
    SetDword(desc + 0x08, 0);                // dwBufferBytes
    SetDword(desc + 0x0C, 0);                // dwReserved
    SetDword(desc + 0x10, 0);                // lpwfxFormat
    device = Snd_Device;
    if (Method<ComCreate>(device, sound::kCreateSoundBuffer)(device, At(desc), &Snd_Primary, nullptr) != 0) {
        Release(Snd_Device);
        Snd_Device = nullptr;
        return kNoPrimary;
    }
    const std::uint32_t format = Address(Snd_WaveFormat);
    SetWord(format + 0x00, 1);        // wFormatTag, PCM
    SetWord(format + 0x02, 2);        // nChannels
    SetWord(format + 0x0C, 2);        // nBlockAlign
    SetDword(format + 0x04, 22050);   // nSamplesPerSec
    SetDword(format + 0x08, 44100);   // nAvgBytesPerSec
    SetWord(format + 0x0E, 8);        // wBitsPerSample
    SetWord(format + 0x10, 0);        // cbSize
    void* const primary = Snd_Primary;
    if (Method<ComFormat>(primary, kSetFormat)(primary, At(format)) != 0) {
        Release(Snd_Device);
        void* const made = Snd_Primary;
        Snd_Device = nullptr;
        Release(made);
        Snd_Primary = nullptr;
        return kNoFormat;
    }
    return 0;
}

void SoundRest_Inject() {
    if (bof3::WantsShadow("sound_rest")) sound_rest::SelfTest();
    BOF3_INJECT(Sound_StopMusic);
    BOF3_INJECT(Sound_ResumeAll);
    BOF3_INJECT(Sound_MusicPlaying);
    BOF3_INJECT(Sound_PauseAll);
    BOF3_INJECT(SndBuf_SetVolume);
    BOF3_INJECT(Music_Halt);
    BOF3_INJECT(Music_Resume);
    BOF3_INJECT(SndStream_IsPlaying);
    BOF3_INJECT(Snd_Init);
}

// Internal to sound.cpp and sound_fuzz.cpp: the addresses the sound layer
// touches that are not named in symbols.toml, the DirectSound calls it makes
// through vtables, and every call the 23 functions make - through pointers, so
// that the start-up fuzz can stand recording functions in for them, for the
// originals' copies and for ours alike. Most callees are ours in this module
// (the music start under Music_Play, the buffer helpers under
// Sound_PlayEffect); through the pointers each function is still tested
// alone. docs/sound.md.
#pragma once

#include <cstdint>
#include <cstring>

namespace sound {

// --- Addresses without a symbol --------------------------------------------
// The Win32 import slots the layer calls through (the IAT, by name from the
// import directory 2026-09-22).
constexpr std::uint32_t kImportCreateEvent = 0x5C4080;   // KERNEL32 CreateEventA
constexpr std::uint32_t kImportCloseHandle = 0x5C4084;   // KERNEL32 CloseHandle
constexpr std::uint32_t kImportMsgWait = 0x5C4164;       // USER32 MsgWaitForMultipleObjects
// WinMain's frame deadline, a float in milliseconds, advanced once per logic
// frame at 0x4FCF0F (known-defects D5, DIV-0022). Read by DIV-0028.
constexpr std::uint32_t kFrameDeadline = 0x6BC628;
// IID_IDirectSoundNotify {B0210783-89CD-11D0-AF08-00A0C925CD16}, 16 bytes.
constexpr std::uint32_t kIidNotify = 0x5C45B8;
// The floats the music volume is built from: 1 / 127 (0x3C010204), 10000.0
// (0x461C4000), and the full volume 127.0 (0x42FE0000).
constexpr std::uint32_t kInverse127At = 0x5C464C;
constexpr std::uint32_t k10000At = 0x5C4648;
constexpr std::uint32_t k127At = 0x5C427C;
// The format strings: BGM\%03d.DAT (loops), BGM\%03dN.DAT (plays once), and
// the decoder's memory-stream name %X@%X (size, address).
constexpr std::uint32_t kLoopingName = 0x666FB8;
constexpr std::uint32_t kOnceName = 0x666FA8;
constexpr std::uint32_t kMemoryName = 0x66BC24;
// Where a cue's four voice words and a bank's 64 voice entries start: bank n
// (1..6 in the shipped data) is Sound_Banks + (n - 1) * 0x384, its cues 16
// bytes each from +0, its voice entries 8 bytes each (sample, buffer) from
// +0x180.
constexpr std::uint32_t kBankBytes = 0x384;
constexpr std::uint32_t kVoices = 0x180;
// Mp3_Decode's end of stream.
constexpr int kEndOfStream = static_cast<int>(0xFFFFFDFEu);
// The streaming buffer: two halves of 0x12000 bytes, a notification at each.
constexpr unsigned kHalf = 0x12000;

// --- DirectSound, as the original calls it: through the vtable each time -
constexpr unsigned kQueryInterface = 0x00, kRelease = 0x08;
constexpr unsigned kCreateSoundBuffer = 0x0C;          // IDirectSound
constexpr unsigned kSetNotificationPositions = 0x0C;   // IDirectSoundNotify
constexpr unsigned kGetStatus = 0x24, kLock = 0x2C, kPlay = 0x30, kSetCurrentPosition = 0x34, kSetVolume = 0x3C,
                   kSetFrequency = 0x44, kStop = 0x48, kUnlock = 0x4C, kRestore = 0x50;   // IDirectSoundBuffer
constexpr long kBufferLost = static_cast<long>(0x88780096u);   // DSERR_BUFFERLOST

using ComCall = long (__stdcall*)(void*);
using ComStatus = long (__stdcall*)(void*, unsigned long*);
using ComValue = long (__stdcall*)(void*, unsigned long);
using ComPlay = long (__stdcall*)(void*, unsigned long, unsigned long, unsigned long);
using ComLock = long (__stdcall*)(void*, unsigned long, unsigned long, void**, unsigned long*, void**, unsigned long*,
                                  unsigned long);
using ComUnlock = long (__stdcall*)(void*, void*, unsigned long, void*, unsigned long);
using ComCreate = long (__stdcall*)(void*, void*, void**, void*);
using ComQuery = long (__stdcall*)(void*, const void*, void**);
using ComPositions = long (__stdcall*)(void*, unsigned long, const void*);

template <class Fn>
inline Fn Method(void* object, unsigned offset) {
    void* const* vtable;
    std::memcpy(&vtable, object, sizeof vtable);
    return reinterpret_cast<Fn>(vtable[offset / 4]);
}

struct Callees {
    // The C runtime and the file layer (File_* are ours, file_io.cpp)
    int (__cdecl* sprintf)(char*, const char*, ...);                          // Crt_sprintf
    void* (__cdecl* malloc)(unsigned);                                       // Crt_malloc
    void (__cdecl* free)(void*);                                             // Crt_free
    int (__cdecl* file_open)(const char*, int, int);                          // File_Open
    int (__cdecl* file_size)(int);                                            // File_Size
    unsigned (__cdecl* file_read)(int, void*, unsigned);                      // File_Read
    void (__cdecl* file_close)(int);                                          // File_Close
    // The MP3 decoder, out of scope and stdcall throughout
    int (__stdcall* mp3_create)(void**);                                      // Mp3_Create
    int (__stdcall* mp3_memory_io)(void*);                                    // Mp3_MemoryIo
    int (__stdcall* mp3_set_io)(void*, void*);                                // Mp3_SetIo
    int (__stdcall* mp3_open)(void*, const char*, int);                       // Mp3_Open
    int (__stdcall* mp3_start)(void*);                                        // Mp3_Start
    int (__stdcall* mp3_decode)(void*, void*);                                // Mp3_Decode
    int (__stdcall* mp3_seek)(void*, int);                                    // Mp3_Seek
    int (__stdcall* mp3_destroy)(void*);                                      // Mp3_Destroy
    // Win32, through the game's own import slots
    void* (__stdcall* create_event)(void*, int, int, const char*);            // CreateEventA
    int (__stdcall* close_handle)(void*);                                     // CloseHandle
    unsigned (__stdcall* msg_wait)(unsigned, const void*, int, unsigned, unsigned);   // MsgWaitForMultipleObjects
    // This module's own
    int (__cdecl* music_load_file)(unsigned);                                 // Music_LoadFile
    void (__cdecl* music_set_volume)(float);                                  // Music_SetVolume
    void (__cdecl* music_start)(const void*, unsigned, int);                  // Music_Start
    int (__cdecl* music_fade_in)(int);                                        // Music_FadeIn
    void (__cdecl* music_pump)();                                             // Music_Pump
    void (__cdecl* music_stop)();                                             // Music_Stop
    unsigned (__cdecl* wave_find_data)(const unsigned char*, const unsigned char**);   // Wave_FindData
    long (__cdecl* sndbuf_write)(void*, const void*, unsigned, unsigned);     // SndBuf_Write
    void (__cdecl* sndbuf_play)(std::uint32_t, void*, unsigned);              // SndBuf_Play
    void (__cdecl* sndbuf_stop)(void*);                                       // SndBuf_Stop
    void* (__cdecl* sndbuf_create)(const unsigned char*);                     // SndBuf_CreateFromWave
    void (__cdecl* music_release)();                                          // Music_Release
    void* (__cdecl* music_open_decoder)(const void*, unsigned);               // Music_OpenDecoder
    void* (__cdecl* music_create_buffer)();                                   // Music_CreateBuffer
    long (__cdecl* music_set_notify)(void*);                                  // Music_SetNotify
    void (__cdecl* music_decode)(unsigned char*, int);                        // Music_Decode
};
extern const Callees kOriginals;
extern Callees g;

// BOF3X_SHADOW=sound: the start-up fuzz, sound_fuzz.cpp. Clones every original
// before Sound_Inject patches it.
void SelfTest();

}  // namespace sound

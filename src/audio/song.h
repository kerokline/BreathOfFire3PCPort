// The cache's music containers in memory: one song (base/bgm/NNN.DAT) and one
// sound bank (base/bgm/bank/NAME.DAT), as docs/seq-format.md lays them out,
// and the loaders that read them. A malformed file aborts loudly through the
// hook below; nothing is guessed.
//
// Portable and standalone: no Windows, no game addresses.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace psx {

// Called with a message before std::abort() when a music file is malformed or
// the player meets something it does not implement (the repo's hard rule 4).
// The default prints to stderr. Shared by song.cpp and seq.cpp.
using MusicAbortHook = void (*)(const char* message);
void SetMusicAbortHook(MusicAbortHook hook);
[[noreturn]] void MusicFatal(const char* format, ...);

struct SongEvent {
    std::uint32_t tick;   // absolute, from the song's start
    std::uint8_t status;  // 0x80..0xEF with the channel, or 0xFF (meta)
    std::uint8_t d1;      // note / controller / program / bend LSB / meta type
    std::uint8_t d2;      // velocity / value / bend MSB
    std::uint32_t meta;   // 0x51: tempo in microseconds per quarter note
};

struct Song {
    std::uint32_t flags = 0;           // bit 0: carries loop markers
    std::string bank;                  // base/bgm/bank/<bank>.DAT
    std::uint16_t number = 0;
    std::uint16_t sub = 0;
    std::uint16_t resolution = 0;      // ticks per quarter note
    std::uint16_t rhythm = 0;
    std::uint32_t tempo = 0;           // microseconds per quarter note
    std::uint32_t loop_start_tick = 0xFFFFFFFFu;
    std::uint32_t loop_end_tick = 0xFFFFFFFFu;
    std::uint32_t end_tick = 0;
    std::vector<SongEvent> events;
};

struct BankProgram {
    std::uint8_t tones = 0;
    std::uint8_t mvol = 0;
    std::uint8_t prior = 0;
    std::uint8_t mode = 0;
    std::uint8_t mpan = 0;
    std::uint8_t block = 0xFF;  // tone block, 0xFF for a program without tones
    std::uint16_t attr = 0;
};

struct BankTone {
    std::uint8_t prior, mode, vol, pan, center, shift, min, max;
    std::uint8_t vibw, vibt, porw, port, pbmin, pbmax;
    std::uint16_t adsr1, adsr2, prog, vag;  // vag 1-based, 0 = none
};

struct BankSample {
    std::uint32_t offset;  // from the start of the bodies
    std::uint32_t size;
};

struct Bank {
    std::string name;
    std::uint16_t ps = 0, ts = 0, vs = 0;
    std::uint8_t mvol = 0, pan = 0, attr1 = 0, attr2 = 0;
    BankProgram programs[128];
    std::vector<BankTone> tones;       // ps * 16, block-major
    std::vector<BankSample> samples;   // samples[i - 1] is VAB sample i
    std::vector<std::uint8_t> body;    // the ADPCM bodies, back to back
};

// Parse a whole file held in memory. Abort on anything malformed.
void LoadSong(const std::uint8_t* data, std::size_t size, Song* out);
void LoadBank(const std::uint8_t* data, std::size_t size, Bank* out);

} // namespace psx

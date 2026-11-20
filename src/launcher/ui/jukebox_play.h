// The drawn launcher's Jukebox player (docs/launcher-portable.md, stage P5):
// what plays - a disc song through the engine's sequencer, a PC MP3 or a disc
// jingle kept as an XA clip - and the sound out through SDL's audio stream in
// place of the Win32 Jukebox's waveOut (music_player.cpp, Stream, Mp3Stream,
// SeqStream and Output, ported). The page is page_jukebox.cpp; this header
// names no library and no operating system.
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace bof3x::ui {

// What plays: 44.1 kHz 16-bit stereo, the rate of every source.
class JukeStream {
public:
    virtual ~JukeStream() = default;
    virtual void Render(std::int16_t* stereo, int frames) = 0;
    // Nothing more will sound: a once-only song past its end.
    virtual bool Ended() const = 0;
};

// A PC MP3 (or a disc jingle kept as an XA clip, DIV-0215), decoded whole and
// looped at DIV-0081's measured points where `loops` and music_loops' table
// has a row for `track` and this file, else rewound to its start (`loops`) or
// played once. `*measured`: the row was found.
std::unique_ptr<JukeStream> JukeMp3(const std::wstring& path, unsigned track, bool loops, bool* measured);
// A disc song from the cache through the engine's sequencer, with the bank of
// its name from `bank_dir` (the song's own folder's bank\, DIV-0109).
std::unique_ptr<JukeStream> JukeSeq(const std::wstring& song_path, const std::wstring& bank_dir);

// The whole of a file, as the Win32 player's ReadWhole: a file that does not
// open, is empty or larger than any song ends the launcher, naming it.
std::vector<std::uint8_t> JukeReadWhole(const std::wstring& path);
// The file a song's parser reads now, for the message of a malformed one; empty for none.
void JukeReading(const std::wstring& path);
// Before the lists are read: the sequencer's parser refuses a malformed song
// or bank with a message that names JukeReading's file, then aborts.
void JukeArmAbort();
// A malformed file ends the launcher, naming it (CLAUDE.md rule 4).
[[noreturn]] void JukeFatal(const std::wstring& message);

// The sound out: one device stream, fed in blocks of 2048 frames on SDL's
// audio thread as the Win32 player's thread fed waveOut's four buffers.
bool JukeOutOpen();    // false: no sound device could be opened
void JukeOutClose();   // stops the sound; the stream playing goes
// The stream from the next block on; null for silence.
void JukeOutSet(std::unique_ptr<JukeStream> stream);
// The stream playing has ended (a song that plays once); the window is woken
// when it does (WakeFrame).
bool JukeOutEnded();

// A check (main.cpp --check): the sound goes to SDL's dummy driver whatever
// the environment says, so a check never sounds; and with `samples`, every
// block a stream renders is also written to that file as it is handed to the
// stream (16-bit little-endian stereo, 44,100 Hz, no header), for the proof
// that the samples equal the Win32 player's.
void JukeOutCheck(const std::string& samples);
// The frames written to the check's file so far.
std::uint64_t JukeOutSamples();
// The name of SDL's sound driver in use ("dummy", "wasapi", ...), "" with no
// device open: a check's report says it.
std::string JukeOutDriver();

}  // namespace bof3x::ui

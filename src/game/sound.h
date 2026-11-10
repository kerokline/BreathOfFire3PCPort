// The sound layer: the sound effects (Sound_PlayEffect and the DirectSound
// buffer helpers under it), the music (Music_Play, the fades, the streaming
// MP3 buffer and its pump) and the per-loop tick WinMain spins on. The PC
// port's own driver over DirectSound; the MP3 decoder under it stays
// Capcom's. docs/sound.md.
#pragma once

#include <cstdint>

#include "game/music_loops.h"

void Sound_Inject();

namespace sound {
std::uint32_t ResumeSkip(std::uint32_t target, int loops, int loop_track, unsigned size);   // defined in sound.cpp (source-layout B0: promoted, one definition)
extern std::uint32_t g_stop_unseen;   // defined in sound.cpp (named here for a moved reader: source-layout B0)
extern std::uint32_t g_fade_frame;   // defined in sound.cpp (named here for a moved reader: source-layout B0)
extern std::uint32_t g_fade_per_frame;   // defined in sound.cpp (named here for a moved reader: source-layout B0)

// DIVERGENCE DIV-0132: the PlayStation's duck - SsSepSetDecrescendo(0, 0,
// 0x14, 0x10), the song lowered 16 of its 97 sequence-volume units, about
// -3.1 dB - as a fade over `frames` logic frames (DIV-0028's clock) to the
// same loudness in whichever volume law the music plays under (the MP3's
// linear-in-decibels level, the sequencer's libsnd square law). The volume
// before it is kept for UnduckMusic.
void DuckMusic(int frames);
// The duck's undo - the PlayStation's SsSepSetCrescendo(0, 0, 0x14, 0x10) -
// a fade over `frames` back to the volume DuckMusic found; with no duck
// outstanding, Music_FadeIn's fade to 127.
void UnduckMusic(int frames);

// DIVERGENCE DIV-0167: the PlayStation's key-off of its sound-effect voices
// 16..23, on the PC's channels (16..22 and 0, by Sound_PlayEffect's wrap):
// every buffer those channels remember is stopped and forgotten. Called by
// BattleAction_EffectPreload under its switch; it touches only
// Sound_Channels and those buffers' play (through Callees::sndbuf_stop).
void KeyOffEffectVoices();

// BOF3X_MUSIC_LOG=1 (sound.cpp): the music log is on. Read by the voice
// stream's SndStream_Play (battle_items.cpp) for its own line; no behaviour.
bool MusicLogOn();

// DIV-0168's switch (sound.cpp): Music_Play refuses a selection whose low
// byte is 0xFF. 0 at build; sound_fuzz.cpp sets it for its own check only.
extern std::uint8_t g_play_refuses_none;

// BOF3X_MUSIC_LOG=1's ring line (sound.cpp RingLog, from Sound_Tick): held
// off while Sound_Inject's self-tests drive Sound_Tick on stand-in buffers,
// and for the whole of a BOF3X_SELFTEST_ONLY run.
extern std::uint8_t g_ring_log_hold;

}  // namespace sound

namespace sound {
// moved from sound.cpp with the state that has the type (source-layout B0)
struct Loop {
    const music_loops::Row* row;  // nullptr: the original's rewind
    std::uint32_t pos;            // samples decoded from the file's first frame
    unsigned char frame[0x4000];
    std::uint32_t carry_at, carry_len;  // bytes of `frame` still to hand out
    unsigned logged[8];                 // tracks whose first loop was logged (a bitmap of 256)
};
struct Ring {
    std::uint32_t written = 0;        // stream samples written into the ring since Music_Start
    std::uint32_t half_base[2] = {};  // the stream sample each half starts with
    bool track_stream = false;        // the stream is the BGM file Music_Play started
    int track = -1;                   // Music_LoadedTrack then
};
struct Resume {
    bool remembered = false;
    unsigned track = 0;
    std::uint32_t sample = 0;
    bool pending = false;
    unsigned pending_track = 0;
    std::uint32_t pending_sample = 0;
};
struct RestoreSkip {
    bool armed = false;
    std::uint32_t sample = 0;
    std::uint32_t reached = 0;  // where the skip landed (the restore's log)
};
extern Loop g_loop;   // defined in sound.cpp (source-layout B0: promoted for a moved reader)
extern Ring g_ring;   // defined in sound.cpp (source-layout B0: promoted for a moved reader)
extern Resume g_resume;   // defined in sound.cpp (source-layout B0: promoted for a moved reader)
extern RestoreSkip g_restore_skip;   // defined in sound.cpp (source-layout B0: promoted for a moved reader)
}  // namespace sound

namespace sound {
void FinishStoppingFade();   // defined in sound.cpp (named here for a moved reader: source-layout B6)
void RingLog();   // defined in sound.cpp (named here for a moved reader: source-layout B6)
}  // namespace sound

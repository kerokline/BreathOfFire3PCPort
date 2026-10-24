// DIVERGENCE DIV-0169: the inn night's pause and resume of the area's song
// (sound_in.h; known-defects.md D343; docs/sound-layer-round.md, group IN).
// DIVERGENCE DIV-0177: the area's song resumed after a jingle (sound_in.h;
// known-defects.md D356).
#include "game/sound_in.h"

#include <cstdint>
#include <cstring>
#include <string>

#include "bof3/symbols.gen.h"
#include "game/music_resume.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace sound {
bool MusicLogOn();  // sound.cpp: BOF3X_MUSIC_LOG
}

extern "C" int __cdecl Music_IsPlaying(void);  // save_menu.cpp, original 0x5A7020

namespace sound_in {

std::uint8_t g_inn_pause = 0;
std::uint8_t g_jingle_resume = 0;

namespace {

// The seven calls out, so the self-tests can stand recorders in.
struct Calls {
    bool (*heard)(unsigned* track, unsigned* sample);
    void (__cdecl* halt)(void);
    void (*pend)(unsigned track, unsigned sample);
    void (__cdecl* start)(const void* file, unsigned size, int loops);
    void (__cdecl* volume)(float volume);
    int (__cdecl* playing)(void);
    bool (*kept)(unsigned* track, unsigned* sample, bool* playing);
};
const Calls kCalls = {music_resume::Heard, Sound_StopMusic, music_resume::Pend, Music_Start, Music_SetVolume,
                      Music_IsPlaying, music_resume::Kept};
Calls g = kCalls;

// The song taken at nightfall - the DLL's own state, never game memory, so
// neither a save nor the state hash sees it (as music_resume's).
struct Taken {
    bool valid = false;
    unsigned track = 0;
    unsigned sample = 0;
};
Taken g_taken;

// DIV-0177: the song taken as a jingle started; whether the game had halted
// it first (its own Music_Resume is then the moment) or it was still playing
// (the jingle's end is); for the latter, the frame last asked about; and
// whether the song was started again - DLL state as g_taken.
struct Jingle {
    Taken song;
    bool halted = false;
    bool playing = false;
    bool over = false;
    std::uint32_t frame = 0xFFFFFFFFu;
};
Jingle g_jingle;

// The song taken is still the one the game holds as playing, and still the
// file Music_Play loaded: else something chose other music meanwhile.
bool StillTheSong(const Taken& song) {
    return Music_Track == song.track && Music_LoadedTrack == static_cast<int>(song.track) && Music_File != nullptr;
}

// Music_Start from the song's file at the taken sample, at the game's volume.
void StartAgain(const Taken& song) {
    g.pend(song.track, song.sample);
    g.start(Music_File, static_cast<unsigned>(Music_FileSize), Music_FileLoops);
    g.volume(Music_Volume);
}

}  // namespace

// The PlayStation's pause (US SHOP.EMI#0 0x801D41EC, SsSepPause for each SEP
// slot playing) at night state 0's end, after Transition_Start(0).
void NightFalls() {
    if (!g_inn_pause) return;
    g_taken = Taken{};
    unsigned track = 0, sample = 0;
    if (!g.heard(&track, &sample)) {  // no slot playing: nothing paused, nothing replayed
        if (sound::MusicLogOn()) bof3::Log("music_log   f%u DIV-0169 nightfall: no song playing, nothing halted", Frame_Counter);
        return;
    }
    g_taken.valid = true;
    g_taken.track = track;
    g_taken.sample = sample;
    g.halt();
    if (sound::MusicLogOn())
        bof3::Log("music_log   f%u DIV-0169 nightfall: track %u halted at sample %u", Frame_Counter, track, sample);
}

// The PlayStation's replay (US 0x801D446C's tail, SsSepReplay(0, i) for each
// slot recorded as playing), last in the restore state, after the message.
void MorningComes() {
    if (!g_inn_pause) return;
    const Taken taken = g_taken;
    g_taken = Taken{};
    if (!taken.valid) return;
    if (!StillTheSong(taken)) {
        if (sound::MusicLogOn())
            bof3::Log("music_log   f%u DIV-0169 morning: track %u not resumed (Music_Track %u, loaded %d)", Frame_Counter,
                      taken.track, static_cast<unsigned>(Music_Track), Music_LoadedTrack);
        return;
    }
    StartAgain(taken);
    if (sound::MusicLogOn())
        bof3::Log("music_log   f%u DIV-0169 morning: track %u resumed at sample %u", Frame_Counter, taken.track, taken.sample);
}

void StreamLoads() { g_jingle = Jingle{}; }

void JingleStarts() {
    g_jingle = Jingle{};
    if (!g_jingle_resume) return;
    if (g_taken.valid) return;  // DIV-0169's night holds the song; its morning resumes it
    unsigned track = 0, sample = 0;
    bool playing = false;
    if (!g.kept(&track, &sample, &playing)) {
        if (sound::MusicLogOn()) bof3::Log("music_log   f%u DIV-0177 jingle: no song on the ring, nothing taken", Frame_Counter);
        return;
    }
    g_jingle.song = Taken{true, track, sample};
    g_jingle.halted = !playing;
    g_jingle.playing = playing;
    if (sound::MusicLogOn())
        bof3::Log("music_log   f%u DIV-0177 jingle: track %u taken at sample %u, %s", Frame_Counter, track, sample,
                  playing ? "playing (resumed at the jingle's end)" : "halted (resumed at the game's Music_Resume)");
}

bool JingleResume() {
    if (!g_jingle.song.valid || !g_jingle.halted) return false;
    if (g.playing() != 0) return false;  // the jingle still sounds: Capcom's Play, the song kept for later
    const Taken song = g_jingle.song;
    g_jingle = Jingle{};
    if (!StillTheSong(song)) {
        if (sound::MusicLogOn())
            bof3::Log("music_log   f%u DIV-0177 Music_Resume: track %u not resumed (Music_Track %u, loaded %d)",
                      Frame_Counter, song.track, static_cast<unsigned>(Music_Track), Music_LoadedTrack);
        return false;
    }
    StartAgain(song);
    if (sound::MusicLogOn())
        bof3::Log("music_log   f%u DIV-0177 Music_Resume: track %u started again at sample %u", Frame_Counter, song.track,
                  song.sample);
    return true;
}

void JingleFrame() {
    if (!g_jingle.playing) return;
    const std::uint32_t frame = Frame_Counter;
    if (frame == g_jingle.frame) return;
    g_jingle.frame = frame;
    if (g.playing() != 0) return;  // the jingle still sounds
    g_jingle.playing = false;
    const Taken song = g_jingle.song;
    if (!StillTheSong(song)) {
        if (sound::MusicLogOn())
            bof3::Log("music_log   f%u DIV-0177 jingle over: track %u not resumed (Music_Track %u, loaded %d)", frame,
                      song.track, static_cast<unsigned>(Music_Track), Music_LoadedTrack);
        return;
    }
    StartAgain(song);
    g_jingle.over = true;
    if (sound::MusicLogOn())
        bof3::Log("music_log   f%u DIV-0177 jingle over: track %u resumed at sample %u", frame, song.track, song.sample);
}

bool JingleOver() { return g_jingle.over; }

namespace {

// BOF3X_SHADOW=shop_states or sound_in: the two hooks against recorders, with
// the switch set for the test alone and every game word it sets put back.
std::string g_trace;
bool g_heard_answer = false;
unsigned g_heard_track = 0, g_heard_sample = 0;

bool RecHeard(unsigned* track, unsigned* sample) {
    g_trace += "heard ";
    if (!g_heard_answer) return false;
    *track = g_heard_track;
    *sample = g_heard_sample;
    return true;
}
void __cdecl RecHalt(void) { g_trace += "halt "; }
void RecPend(unsigned track, unsigned sample) { g_trace += "pend(" + std::to_string(track) + "," + std::to_string(sample) + ") "; }
void __cdecl RecStart(const void* file, unsigned size, int loops) {
    g_trace += "start(" + std::to_string(reinterpret_cast<std::uintptr_t>(file)) + "," + std::to_string(size) + "," +
               std::to_string(loops) + ") ";
}
void __cdecl RecVolume(float volume) { g_trace += "volume(" + std::to_string(static_cast<int>(volume)) + ") "; }
int g_playing_answer = 0;
int __cdecl RecPlaying(void) {
    g_trace += "playing ";
    return g_playing_answer;
}
bool g_kept_playing = false;  // the song on the ring: playing (true) or halted
bool RecKept(unsigned* track, unsigned* sample, bool* playing) {
    g_trace += "kept ";
    if (!g_heard_answer) return false;
    *track = g_heard_track;
    *sample = g_heard_sample;
    *playing = g_kept_playing;
    return true;
}

void SelfTest() {
    const std::uint8_t saved_switch = g_inn_pause;
    const std::uint8_t saved_jingle_switch = g_jingle_resume;
    const std::uint32_t saved_frame = Frame_Counter;
    const unsigned char saved_track = Music_Track;
    const int saved_loaded = Music_LoadedTrack;
    void* const saved_file = Music_File;
    const int saved_size = Music_FileSize;
    const int saved_loops = Music_FileLoops;
    const float saved_volume = Music_Volume;
    g = Calls{RecHeard, RecHalt, RecPend, RecStart, RecVolume, RecPlaying, RecKept};
    alignas(4) static unsigned char file[16];
    const std::string file_id = std::to_string(reinterpret_cast<std::uintptr_t>(file));

    int failures = 0;
    auto check = [&](const char* what, const std::string& want) {
        if (g_trace != want) {
            ++failures;
            bof3::Log("shadow      sound_in self-test MISMATCH %s: \"%s\", want \"%s\"", what, g_trace.c_str(), want.c_str());
        }
        g_trace.clear();
    };
    auto area = [&](unsigned track, int loaded) {
        Music_Track = static_cast<unsigned char>(track);
        Music_LoadedTrack = loaded;
        Music_File = file;
        Music_FileSize = 16;
        Music_FileLoops = 1;
        Music_Volume = 100.0f;
    };

    // the switch off: Capcom's night, nothing called
    g_inn_pause = 0;
    g_heard_answer = true, g_heard_track = 7, g_heard_sample = 1234;
    area(7, 7);
    NightFalls();
    MorningComes();
    check("switch off", "");

    g_inn_pause = 1;
    // a song playing: halted at nightfall, resumed at its sample in the morning, once
    NightFalls();
    check("nightfall", "heard halt ");
    MorningComes();
    check("morning", "pend(7,1234) start(" + file_id + ",16,1) volume(100) ");
    MorningComes();
    check("a second morning", "");

    // nothing playing at nightfall: nothing halted, nothing resumed
    g_heard_answer = false;
    NightFalls();
    check("nightfall, silent", "heard ");
    MorningComes();
    check("morning after a silent night", "");

    // the game's track changed meanwhile, or the file no longer holds it
    g_heard_answer = true;
    NightFalls();
    area(9, 9);
    MorningComes();
    check("another track", "heard halt ");
    area(7, 7);
    NightFalls();
    Music_LoadedTrack = 9;
    MorningComes();
    check("another file", "heard halt ");
    area(7, 7);
    NightFalls();
    Music_Track = 0xFF;
    MorningComes();
    check("the track stopped", "heard halt ");

    // DIV-0177, the jingle. Each JingleFrame a new logic frame unless said.
    g_inn_pause = 0;
    g_taken = Taken{};
    std::uint32_t frame = 100;
    auto next_frame = [&]() { Frame_Counter = ++frame; };
    auto jingle = [&](bool kept, bool playing) {
        g_heard_answer = kept;
        g_kept_playing = playing;
        area(7, 7);
        StreamLoads();
        JingleStarts();
    };
    // the switch off: nothing taken, nothing asked, the resume and the wait Capcom's
    g_jingle_resume = 0;
    jingle(true, false);
    g_playing_answer = 0;
    next_frame();
    JingleFrame();
    if (JingleResume()) check("jingle, switch off, resume", "taken");
    check("jingle, switch off", "");

    g_jingle_resume = 1;
    // halted first (Capcom's bracket): taken; Sound_Tick leaves it; the game's
    // Music_Resume while the jingle sounds is Capcom's Play; once it has
    // ended, the song started again, once
    jingle(true, false);
    check("halted jingle starts", "kept ");
    g_playing_answer = 1;
    next_frame();
    JingleFrame();
    check("halted, the tick", "");
    if (JingleResume()) check("halted, resume during the jingle", "resumed");
    check("halted, resume during the jingle", "playing ");
    g_playing_answer = 0;
    if (!JingleResume()) check("halted, resume after the jingle", "not resumed");
    check("halted, resume after the jingle", "playing pend(7,1234) start(" + file_id + ",16,1) volume(100) ");
    if (JingleResume()) check("halted, a second resume", "resumed");
    if (JingleOver()) check("halted, over", "over");
    check("halted, a second resume", "");

    // playing (no bracket): taken; asked once a frame while the jingle plays;
    // at its end started again at its sample, once; the wait over from then
    jingle(true, true);
    check("playing jingle starts", "kept ");
    g_playing_answer = 1;
    next_frame();
    JingleFrame();
    JingleFrame();  // the same frame: not asked again
    check("playing, jingle sounds", "playing ");
    g_playing_answer = 0;
    next_frame();
    JingleFrame();
    check("playing, jingle ends", "playing pend(7,1234) start(" + file_id + ",16,1) volume(100) ");
    if (!JingleOver()) check("playing, over", "not over");
    if (JingleResume()) check("playing, the game's resume after", "resumed");
    next_frame();
    JingleFrame();
    check("after the resume", "");
    StreamLoads();
    if (JingleOver()) check("the next stream, over", "over");

    // no song on the ring: nothing taken, nothing resumed
    jingle(false, false);
    next_frame();
    JingleFrame();
    if (JingleResume()) check("silent, resume", "resumed");
    check("jingle, silent", "kept ");

    // the game's music changed meanwhile: nothing resumed
    jingle(true, false);
    area(9, 9);
    if (JingleResume()) check("halted, another track", "resumed");
    check("halted, another track", "kept playing ");
    jingle(true, true);
    Music_Track = 0xFF;
    next_frame();
    JingleFrame();
    if (JingleOver()) check("playing, the track stopped, over", "over");
    check("playing, the track stopped", "kept playing ");

    // the inn's night holds the song: the jingle takes nothing
    g_inn_pause = 1;
    g_heard_answer = true;
    area(7, 7);
    NightFalls();
    check("inn nightfall", "heard halt ");
    StreamLoads();
    JingleStarts();
    next_frame();
    JingleFrame();
    if (JingleResume()) check("inn, resume", "resumed");
    check("inn jingle", "");
    MorningComes();
    check("inn morning", "pend(7,1234) start(" + file_id + ",16,1) volume(100) ");
    g = kCalls;
    g_taken = Taken{};
    g_jingle = Jingle{};
    g_jingle_resume = saved_jingle_switch;
    Frame_Counter = saved_frame;
    g_inn_pause = saved_switch;
    Music_Track = saved_track;
    Music_LoadedTrack = saved_loaded;
    Music_File = saved_file;
    Music_FileSize = saved_size;
    Music_FileLoops = saved_loops;
    Music_Volume = saved_volume;
    if (failures) bof3::Fatal("sound_in self-test: %d MISMATCHES (the log names them)", failures);
    bof3::Log("shadow      DIV-0169 self-test: 9 checks, 0 MISMATCHES (off, nightfall, morning once, silent night, another "
              "track, another file, track stopped)");
    bof3::Log("shadow      DIV-0177 self-test: 16 traces and 12 answers, 0 MISMATCHES (off; halted: kept, Capcom's Play "
              "during the jingle, resumed once after; playing: asked once a frame, resumed once, over until the next "
              "stream; silent, another track, track stopped, the inn's night)");
}

}  // namespace

void Inject() {
    if (bof3::WantsShadow("shop_states") || bof3::WantsShadow("sound_in")) SelfTest();
    const std::uint8_t was = 0, is = 1;
    bof3::PatchBytes("InnNightPause",
                     static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&g_inn_pause)), &was, &is, 1);
    bof3::Log("DIV-0169    the inn's night halts the area's song as the fade starts and resumes it at its position "
              "after the morning's message, as the PlayStation does (on unless the line above says OFF)");
    // DIV-0177: after the sound, save_menu and shop_states fuzzes (Inject_All
    // runs them first), which drive Sound_LoadStream and Sound_Tick as Capcom's.
    bof3::PatchBytes("JingleResume",
                     static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&g_jingle_resume)), &was, &is, 1);
    bof3::Log("DIV-0177    the area's song resumes where it stopped once a jingle ends, as the PlayStation's plays on "
              "under its XA jingles (on unless the line above says OFF)");
}

}  // namespace sound_in

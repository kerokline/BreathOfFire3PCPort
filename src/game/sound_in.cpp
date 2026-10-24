// DIVERGENCE DIV-0169: the inn night's pause and resume of the area's song
// (sound_in.h; known-defects.md D343; docs/sound-layer-round.md, group IN).
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

namespace sound_in {

std::uint8_t g_inn_pause = 0;

namespace {

// The five calls out, so the self-test can stand recorders in.
struct Calls {
    bool (*heard)(unsigned* track, unsigned* sample);
    void (__cdecl* halt)(void);
    void (*pend)(unsigned track, unsigned sample);
    void (__cdecl* start)(const void* file, unsigned size, int loops);
    void (__cdecl* volume)(float volume);
};
const Calls kCalls = {music_resume::Heard, Sound_StopMusic, music_resume::Pend, Music_Start, Music_SetVolume};
Calls g = kCalls;

// The song taken at nightfall - the DLL's own state, never game memory, so
// neither a save nor the state hash sees it (as music_resume's).
struct Taken {
    bool valid = false;
    unsigned track = 0;
    unsigned sample = 0;
};
Taken g_taken;

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
    // The area's song is still the one the game holds as playing, and still
    // the file Music_Play loaded: else something chose other music meanwhile.
    if (Music_Track != taken.track || Music_LoadedTrack != static_cast<int>(taken.track) || Music_File == nullptr) {
        if (sound::MusicLogOn())
            bof3::Log("music_log   f%u DIV-0169 morning: track %u not resumed (Music_Track %u, loaded %d)", Frame_Counter,
                      taken.track, static_cast<unsigned>(Music_Track), Music_LoadedTrack);
        return;
    }
    g.pend(taken.track, taken.sample);
    g.start(Music_File, static_cast<unsigned>(Music_FileSize), Music_FileLoops);
    g.volume(Music_Volume);
    if (sound::MusicLogOn())
        bof3::Log("music_log   f%u DIV-0169 morning: track %u resumed at sample %u", Frame_Counter, taken.track, taken.sample);
}

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

void SelfTest() {
    const std::uint8_t saved_switch = g_inn_pause;
    const unsigned char saved_track = Music_Track;
    const int saved_loaded = Music_LoadedTrack;
    void* const saved_file = Music_File;
    const int saved_size = Music_FileSize;
    const int saved_loops = Music_FileLoops;
    const float saved_volume = Music_Volume;
    g = Calls{RecHeard, RecHalt, RecPend, RecStart, RecVolume};
    alignas(4) static unsigned char file[16];
    const std::string file_id = std::to_string(reinterpret_cast<std::uintptr_t>(file));

    int failures = 0;
    auto check = [&](const char* what, const std::string& want) {
        if (g_trace != want) {
            ++failures;
            bof3::Log("shadow      DIV-0169 self-test MISMATCH %s: \"%s\", want \"%s\"", what, g_trace.c_str(), want.c_str());
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

    g = kCalls;
    g_taken = Taken{};
    g_inn_pause = saved_switch;
    Music_Track = saved_track;
    Music_LoadedTrack = saved_loaded;
    Music_File = saved_file;
    Music_FileSize = saved_size;
    Music_FileLoops = saved_loops;
    Music_Volume = saved_volume;
    if (failures) bof3::Fatal("DIV-0169 self-test: %d MISMATCHES (the log names them)", failures);
    bof3::Log("shadow      DIV-0169 self-test: 9 checks, 0 MISMATCHES (off, nightfall, morning once, silent night, another "
              "track, another file, track stopped)");
}

}  // namespace

void Inject() {
    if (bof3::WantsShadow("shop_states") || bof3::WantsShadow("sound_in")) SelfTest();
    const std::uint8_t was = 0, is = 1;
    bof3::PatchBytes("InnNightPause",
                     static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&g_inn_pause)), &was, &is, 1);
    bof3::Log("DIV-0169    the inn's night halts the area's song as the fade starts and resumes it at its position "
              "after the morning's message, as the PlayStation does (on unless the line above says OFF)");
}

}  // namespace sound_in

// The sound-layer round's group ST: DIV-0174 (StopCuts) and DIV-0175
// (OneShotKeyOff). sound_st.h says what each does; docs/DIVERGENCE.md has the
// entries, and DIV-0174's the table of every Music_FadeOutStop call site read
// beside its PlayStation twin.
#include "game/sound_st.h"

#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/sound_callees.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace sound {
bool MusicLogOn();  // sound.cpp: BOF3X_MUSIC_LOG
}

namespace sound_st {
namespace {

std::uint32_t Address(const volatile void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
std::uint32_t Dword(std::uint32_t address) {
    std::uint32_t v;
    std::memcpy(&v, reinterpret_cast<const void*>(static_cast<std::uintptr_t>(address)), sizeof v);
    return v;
}

}  // namespace

// ---------------------------------------------------------------------------
// DIVERGENCE DIV-0174: the PlayStation's stop where the port fades.
//
// Music_FadeOutStop (original 0x587B40) is the twin of the JP disc's
// 0x801626C0 (US 0x80161CD0): the selection's SEP through 0x8015DF44 (US
// 0x8015D554), which sets a decrescendo and calls SsSepStop (JP 0x8016E03C,
// US 0x8016D6C4) at once - the voices released, the decrescendo never heard
// (docs/libsnd-reading.md 5.2). Its 131 PC call sites pair with that, with
// the plain stop 0x8015DDF4 (US 0x8015D404) or the stop-every-SEP 0x8015DFC8;
// none with the decrescendo alone (0x8015DEE8, US 0x8015D4F8), whose twin is
// Music_FadeOut. So the class is the function, and the cut is made here.
std::uint8_t g_stop_cuts = 0;

void CutStop(int frames) {
    if (sound::MusicLogOn())
        bof3::Log("music_log   f%u StopCuts: Music_FadeOutStop(%d) cut, track %u", Frame_Counter, frames,
                  static_cast<unsigned>(Music_Track));
    sound::g.music_stop();
}

void ArmStopCuts() {
    const std::uint8_t was = 0, is = 1;
    bof3::PatchBytes("StopCuts", Address(&g_stop_cuts), &was, &is, 1);
    bof3::Log("DIV-0174    a music stop cuts the song, as the PlayStation's SsSepStop does, instead of fading it "
              "(on unless the line above says OFF)");
}

// ---------------------------------------------------------------------------
// DIVERGENCE DIV-0175: the PlayStation's key-off reaching a one-shot.
//
// BattleAction_EffectPreload's twin keys off voices 0x10..0x17 (SsUtKeyOffV
// 0x8016F10C, JP 0x801D3E68..0x801D3E84; US 0x801D3E88..), whatever each is
// sounding. Sound_PlayEffect (original 0x587740) remembers a voice's buffer in
// Sound_Channels only when it loops; a one-shot plays the bank entry's buffer,
// shared by every cue naming that voice, and nothing remembers it.
std::uint8_t g_one_shot_key_off = 0;
OneShot g_one_shots[kChannels] = {};

void NoteOneShot(std::uint32_t channel, std::uint32_t entry) {
    if (!g_one_shot_key_off) return;
    if (channel >= kChannels) bof3::Fatal("DIV-0175: Sound_PlayEffect filed a voice under channel %u", static_cast<unsigned>(channel));
    g_one_shots[channel] = {entry, Dword(entry + 4)};
}

void KeyOffOneShots() {
    for (std::uint32_t voice = 16; voice <= 23; ++voice) {
        const std::uint32_t channel = voice >= 0x17 ? voice - 0x17 : voice;
        const OneShot shot = g_one_shots[channel];
        g_one_shots[channel] = {0, 0};
        if (shot.entry == 0 || shot.buffer == 0 || Dword(shot.entry + 4) != shot.buffer) continue;
        bool looping = false;
        for (unsigned c = 0; c < kChannels; ++c) looping = looping || Sound_Channels[c] == shot.buffer;
        if (looping) continue;
        sound::g.sndbuf_stop(reinterpret_cast<void*>(static_cast<std::uintptr_t>(shot.buffer)));
    }
}

void ArmOneShotKeyOff() {
    const std::uint8_t was = 0, is = 1;
    bof3::PatchBytes("OneShotKeyOff", Address(&g_one_shot_key_off), &was, &is, 1);
    bof3::Log("DIV-0175    a battle action's effect stops the one-shot sound effects the last one left sounding "
              "(on unless the line above says OFF)");
}

}  // namespace sound_st

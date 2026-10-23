// The sound-layer round's group ST: DIV-0174 (StopCuts). sound_st.h says what
// it does; docs/DIVERGENCE.md has the entry, with the table of every
// Music_FadeOutStop call site read beside its PlayStation twin.
#include "game/sound_st.h"

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

}  // namespace sound_st

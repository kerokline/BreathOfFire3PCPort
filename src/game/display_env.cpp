// The PSX library's display calls as the port implements them over DirectDraw
// and Direct3D, and the sound bank load - group S of the fifth takeover round
// (docs/takeover-queue-round5.md). Each function read to its last instruction
// with capstone against bof3/BOF3.exe, 2026-09-23 (docs/display-env.md):
//
//   Gpu_PutDispEnv    0x5A7860  0x26    Gfx_Present       0x59EDF0  0x60
//   Gpu_PutDrawEnv    0x5A7890  0x4D    Gfx_ClearPresent  0x59ECE0  0x107
//   Gpu_SetDefDispEnv 0x5A78E0  0x28    D3d_SetBackColor  0x5A5050  0xE0
//   Gpu_SetDefDrawEnv 0x5A7910  0x4A    Snd_LoadBank      0x587CD0  0xDE
//
// Faithful: no divergence. Every call out goes through display_env::g, so the
// start-up fuzz can stand recorders in for the callees, and every COM call
// through the object's vtable as the original makes it.
#include "game/display_env.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/display_env_callees.h"
#include "game/sound_sp.h"
#include "game/sound_st.h"
#include "hook/detour.h"

namespace display_env {

const Callees kOriginals = {
    Gfx_Present, Gfx_ClearPresent, D3d_SetBackColor, Crt_malloc, Crt_free, SndBuf_Release, SndBuf_FromWave,
};
Callees g = kOriginals;

namespace {

unsigned char* At(U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }
U Addr(const volatile void* p) { return Low32(p); }
U Long(U address) {
    U v;
    std::memcpy(&v, At(address), sizeof v);
    return v;
}
void PutLong(U address, U v) { std::memcpy(At(address), &v, sizeof v); }

// `rep movsd`: n dwords, first to last, each read just before it is stored -
// so a source that overlaps the destination from below repeats itself, as the
// original's does.
void CopyDwords(U dst, U src, U n) {
    for (U i = 0; i < n; ++i) PutLong(dst + i * 4, Long(src + i * 4));
}

}  // namespace
}  // namespace display_env

using namespace display_env;

void DisplayEnv_Inject() {
    if (bof3::WantsShadow("display_env")) display_env::SelfTest();
}

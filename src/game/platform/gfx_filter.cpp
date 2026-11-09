#include "game/gfx_filter.h"

#include <windows.h>

#include <cstdint>
#include <cstring>

#include "hook/detour.h"
#include "hook/log.h"

// DIV-0012: texture filtering as a choice. Off unless BOF3X_FILTER=point.
//
// The renderer's set-up 0x5A5160 sets stage 0's filters once, through the
// device at 0x7CC350 (IDirect3DDevice3::SetTextureStageState, vtable +0xA0):
//
//   0x5A5B20  push 2 / push 0x11 / push esi(0)    D3DTSS_MINFILTER = D3DTFN_LINEAR
//   0x5A5B33  push 2 / push 0x10 / push esi(0)    D3DTSS_MAGFILTER = D3DTFG_LINEAR
//
// and alpha testing as GREATER than 8 of 255 (render states 0x0F = 1,
// 0x18 = 8, 0x19 = 5, at 0x5A5B4D..0x5A5B6B). So every texture is drawn
// bilinearly at 2x, and a glyph's edge - white blended towards the colour
// key's transparent black - passes the alpha test down to 3% and is drawn as a
// grey fringe: the "glow" round PC text that the PlayStation, which does not
// filter, does not have (owner, 2026-09-20).
//
// BOF3X_FILTER=point turns both immediates into 1 (D3DTFN_POINT,
// D3DTFG_POINT): hard 2x pixels, no fringe. With DIV-0010's far edge a sprite
// of w texels then covers its 2w pixels two to a texel exactly; with the
// original's far edge the last texel would get one.
//
// An environment variable, not a key, because the game's input is unread; a
// live toggle would call SetTextureStageState itself (docs/IDEAS.md).

namespace {

constexpr std::uint32_t kMinFilterImm = 0x5A5B21;   // the 02 of `push 2` at 0x5A5B20
constexpr std::uint32_t kMagFilterImm = 0x5A5B34;   // the 02 of `push 2` at 0x5A5B33

}  // namespace

void GfxFilter_Inject() {
    char mode[16];
    const DWORD n = GetEnvironmentVariableA("BOF3X_FILTER", mode, sizeof mode);
    if (n == 0) return;
    if (n >= sizeof mode || (std::strcmp(mode, "point") != 0 && std::strcmp(mode, "linear") != 0))
        bof3::Fatal("BOF3X_FILTER must be 'point' or 'linear'");
    if (std::strcmp(mode, "linear") == 0) return;   // the original's
    // `6A 02 6A 11` and `6A 02 6A 10`: the immediate with the state after it,
    // so that a wrong address cannot pass for the right one.
    const std::uint8_t min_was[] = {0x02, 0x6A, 0x11}, min_is[] = {0x01, 0x6A, 0x11};
    const std::uint8_t mag_was[] = {0x02, 0x6A, 0x10}, mag_is[] = {0x01, 0x6A, 0x10};
    bof3::PatchBytes("Gfx_FilterPoint", kMinFilterImm, min_was, min_is, 3);
    bof3::PatchBytes("Gfx_FilterPoint", kMagFilterImm, mag_was, mag_is, 3);
    bof3::Log("DIV-0012    texture filter: point (BOF3X_FILTER)");
}

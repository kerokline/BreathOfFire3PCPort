// The SatPixie CRT look (DIV-0043, docs/crt-look.md section 7): a port to
// HLSL of Conkwer's "CRT-SatPixie" (github.com/Conkwer/satpixie-crt-shader),
// itself a fork of Mattias Gustavsson's "newpixie" CRT, both offered under
// the MIT licence or as public domain - the one third-party look this
// project carries (docs/THIRD_PARTY.md has the notice). Chosen by the owner,
// 2026-09-23.
//
// Four passes, as the RetroArch preset has them: accumulate (this frame
// against the previous frame's horizontal blur, faded), blur across, blur
// down, then the picture - chromatic aberration, ghosting, rolling
// scanlines, a vignette, an optional shadow mask, filmic tone mapping,
// noise and flicker. The parameters are the preset's, read from
// BOF3X_SATPIXIE="name=value,..." (the launcher's Look options dialog
// writes it).
#pragma once

#include <d3d11.h>

#include "render/render_shim.h"

namespace render {

// Compiles the passes and makes the intermediate textures of the target's
// size. `k` is the target's scale (unused by the look itself; kept for the
// contract CrtInit has).
void SatpixieInit(ID3D11Device* device, U target_w, U target_h, U k);

// DIV-0042: the target changed size; the textures follow.
void SatpixieResize(ID3D11Device* device, U target_w, U target_h, U k);

// Draws the target onto `window` inside `picture`. `frame` counts presents,
// for the look's animation. Leaves the render target and viewport set to
// the window's.
void SatpixieDraw(ID3D11DeviceContext* ctx, ID3D11ShaderResourceView* target, ID3D11RenderTargetView* window,
                  const D3D11_VIEWPORT& picture, unsigned frame);

}  // namespace render

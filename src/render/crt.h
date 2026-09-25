// The CRT look (DIV-0037): scanlines and halation, drawn by
// the present from the render target onto the window (docs/crt-look.md).
//
// Our own shaders, written for this project from the general technique - a
// Gaussian beam per source line whose width follows its brightness and a
// blurred linear-light glow added back - and not a
// port of any published shader (CLAUDE.md rule 5: the well-known libretro
// CRT shaders are GPL).
#pragma once

#include <d3d11.h>

#include "render/render_shim.h"

namespace render {

// Compiles the four passes and makes the two glow textures at the game's own
// size, target / k: 320 x 240, or 426 wide (DIV-0041). `target_w` / `target_h`
// are the render target's size, 320k x 240k. BOF3X_CRT overrides the look's
// numbers (crt.cpp).
void CrtInit(ID3D11Device* device, U target_w, U target_h, U k);

// DIV-0042: the target changed size; the constants and the glow textures follow.
void CrtResize(ID3D11Device* device, U target_w, U target_h, U k);

// Draws the target onto `window` inside `picture` (the present's centred,
// scaled rectangle, already cleared around). Leaves the render target set to
// `window`, the viewport to `picture`, and no shader resources bound.
void CrtDraw(ID3D11DeviceContext* ctx, ID3D11ShaderResourceView* target, ID3D11RenderTargetView* window,
             const D3D11_VIEWPORT& picture);

}  // namespace render

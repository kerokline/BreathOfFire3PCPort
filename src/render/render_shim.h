// The render backend's game-facing half: DirectDraw- and Direct3D-shaped COM
// objects that the game's draw code talks to through their vtables exactly as
// it talked to DirectX 6 (docs/display-overhaul.md section 2, docs/render-backend.md).
//
// Every draw handler, texture builder and the present are ours already, and
// all of them reach DirectX only through the objects the set-up 0x5A5160 put
// in the globals - Dd_DirectDraw 0x7CC334, DDraw_Primary 0x7CC338,
// DDraw_BackBuffer 0x7CC33C, Dd_StageSurface 0x7CC344, D3d_Device 0x7CC350,
// D3d_Viewport 0x7CC354, D3d_BackMaterial 0x7CC358 - and the surfaces made
// through IDirectDraw4::CreateSurface. So the backend is those objects, with
// the method set the start-up fuzzes already enumerated (d3d_fuzz.h,
// ddraw_fuzz.h): an IDirectDraw4 that makes surfaces, surfaces that lock,
// blit and answer QueryInterface with an IDirect3DTexture2, a device that
// takes TLVERTEX DrawPrimitive calls, a viewport that clears, a material that
// holds the clear colour. Any other method ends the process naming its slot
// (rule 4: no quiet stubs).
//
// Nothing here touches the GPU. A surface is pixels in memory; a
// DrawPrimitive is appended to the frame's command list; the present hands
// that list to render_d3d11.cpp, which runs it. This is deliberate: draw
// handlers run wherever the game calls them, including the 16 KB task stacks
// (docs/SCAFFOLDING.md section 3), and the GPU work happens only under the
// present - which the game also calls from a task stack, so render_d3d11.cpp
// runs it on a fiber with a 1 MB stack of its own (docs/render-backend.md
// sections 2 and 4).
//
// A texture that a recorded draw uses and that the game then rewrites (a page
// texture refreshed for a new CLUT, a glyph slot reused) keeps its old pixels
// for that draw: the rewrite snapshots them first (TexVersion). So the
// command list replays the frame exactly as DirectDraw would have drawn it.
#pragma once

#include <cstdint>

namespace render {

using U = std::uint32_t;

// --- what the set-up hands the game ------------------------------------------

struct Surface;

// One immutable view of a surface's pixels for the draws that were recorded
// against it. `pixels` null means "the surface's own buffer at render time".
struct TexVersion {
    Surface* surface;
    const unsigned char* pixels;   // a snapshot in the frame arena, or null
    U serial;                      // unique per version, for the GPU-side cache
    // With a snapshot: the colour key as it stood when the snapshot was taken,
    // since SetColorKey can change the surface's own afterwards.
    U color_key;
    bool has_color_key;
};

struct Surface {
    void* const* vtable;           // IDirectDrawSurface4-shaped
    void* const* texture_vtable;   // the IDirect3DTexture2 that QueryInterface answers: this + 4
    U refs;
    U width, height, bpp, pitch;   // bpp 16 or 32; pitch in bytes
    U caps, caps2;                 // DDSCAPS as created
    U color_key;                   // DDCKEY_SRCBLT low colour, valid when has_color_key
    bool has_color_key;
    bool is_primary, is_back, locked;
    bool dirty;                    // pixels or key changed since the GPU copy was made
    unsigned char* pixels;         // HeapAlloc'd, height * pitch bytes; null for the primary and once released
    TexVersion* version;           // the version pending draws see; replaced on a rewrite
    U pending_draws;               // draws recorded against `version` since it was made
    bool snapshot_held;            // released with a snapshot the current frame still draws; not reusable until ResetFrame
    void* gpu;                     // render_d3d11's per-surface object, or null
    U gpu_serial;                  // the TexVersion serial the GPU texture holds
    // DDPIXELFORMAT as GetSurfaceDesc reports it
    U pf_flags, pf_bits, pf_rmask, pf_gmask, pf_bmask, pf_amask;
};

// Command stream, one frame at a time.
enum class Cmd : U { kClear, kDraw };

struct PipeState {
    TexVersion* texture;     // null: untextured
    U src_blend, dst_blend;  // D3DBLEND values; alpha blend on when blend_enable
    bool blend_enable;
    bool alpha_test;         // ALPHATESTENABLE
    U alpha_ref, alpha_func; // ALPHAREF, ALPHAFUNC (D3DCMP)
    bool color_key;          // COLORKEYENABLE: keyed texels discarded
    bool specular;           // SPECULARENABLE: specular added after texturing
    bool alpha_modulate;     // ALPHAOP MODULATE (else SELECTARG2, the diffuse alpha)
    bool point_filter;       // MAGFILTER / MINFILTER 1 (point); else linear
    bool flat;               // SHADEMODE 1 (flat): the first vertex's colour over the face
    U topology;              // 0 triangles, 1 lines, 2 points
};

struct Command {
    Cmd kind;
    PipeState state;   // kDraw
    U first, count;    // kDraw: vertices in the frame's vertex array (a list: triangles as 3, lines as 2)
    U color;           // kClear: ARGB
};

// A D3DTLVERTEX: sx, sy, sz, rhw, diffuse, specular, tu, tv - 0x20 bytes.
struct Vertex {
    float x, y, z, rhw;
    U diffuse, specular;
    float u, v;
};

struct Frame {
    Vertex* vertices;
    U n_vertices, max_vertices;
    Command* commands;
    U n_commands, max_commands;
    unsigned char* arena;      // the frame's TexVersions and pixel snapshots; emptied by ResetFrame
    U arena_used, arena_size;
    U next_serial;
};

// --- the objects ---------------------------------------------------------------

// Creates the IDirectDraw4-shaped object. One per process.
void* DirectDrawObject();
// The IDirect3DDevice3-, IDirect3DViewport3- and IDirect3DMaterial3-shaped objects.
void* DeviceObject();
void* ViewportObject();
void* MaterialObject();

// Makes a surface the way IDirectDraw4::CreateSurface would from a
// DDSURFACEDESC2 (dwFlags, dwWidth, dwHeight, ddpfPixelFormat, ddsCaps), or
// straight from its dimensions. bpp 16 (5-6-5) or 32 (X-8-8-8); pitch rounds
// the row up to 16 bytes. Returns null when out of memory or when a
// dimension is 0 or above 4096.
Surface* MakeSurface(U width, U height, U bpp, U caps, U caps2, bool primary, bool back);

// DIV-0042: the primary or back surface at a new size - width, height and
// pitch as GetSurfaceDesc reports them, the back buffer's pixels reallocated.
// Only for those two, which no draw is ever recorded against. False when out
// of memory (the surface then keeps its old size).
bool ResizeSurface(Surface* s, U width, U height);

// The state the device holds between draws, for the set-up to seed and for
// tests to read.
PipeState& CurrentState();
// Sets the material's colour (the viewport's clear colour) directly.
void SetClearColor(U argb);

// The frame being recorded. The present takes it (render_d3d11.cpp) and
// Reset starts the next.
Frame& CurrentFrame();
void ResetFrame();

// Called by the present path: Flip or Blt on the primary. Set by
// render_d3d11.cpp at start-up. Receives the frame; must not keep pointers
// into it past its return.
using PresentFn = void (*)(Frame& frame);
void SetPresentHook(PresentFn fn);

// Start-up: allocates the frame's arrays. Sizes are the limits a frame can
// hold; a frame that outgrows them ends the process naming the limit.
void InitShim(U max_vertices, U max_commands, U arena_bytes);

// The current pixel-source of a surface for a draw: registers the draw
// against the surface's version. Used by the device's DrawPrimitive.
TexVersion* Use(Surface* s);
// Called before a surface's pixels change: marks it dirty for the GPU side and
// snapshots the pixels for any draw already recorded against them.
void BeforeWrite(Surface* s);

// Every surface ever made, live (refs > 0) or released (refs 0, pixels null,
// `gpu` still to be freed by the GPU side).
Surface* const* AllSurfaces(U* n);

}  // namespace render

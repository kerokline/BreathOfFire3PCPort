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
    // DIV-0125 (fb_grab.h): a page texture of direct colour, as
    // D3d_BindTexture last bound it - the VRAM cell its texel (0, 0) comes
    // from and the texture window its 256 x 256 texels tile.
    bool direct_page;
    int page_x, page_y, tile_w, tile_h;
};

// Command stream, one frame at a time. kGrab (DIV-0125): copy part of
// the picture drawn so far into a grab slot's texture.
enum class Cmd : U { kClear, kDraw, kGrab };

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
    // DIV-0125: 0, or 1 + the grab slot whose texture this draw samples
    // where its texels fall inside the grab. grab_off is the texture's VRAM
    // origin less the grab's (cells), grab_tile the texture window the 256
    // texels tile, grab_size the grab's cells.
    U grab_slot;
    float grab_off[2], grab_tile[2], grab_size[2];
};

struct Command {
    Cmd kind;
    PipeState state;   // kDraw
    U first, count;    // kDraw: vertices in the frame's vertex array (a list: triangles as 3, lines as 2)
    U color;           // kClear: ARGB
    int grab_x, grab_y, grab_w, grab_h;   // kGrab: the source in the picture's coordinates (game pixels)
    U grab_slot;                          // kGrab: the slot it fills
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

// DIV-0041 (the narrow areas pillarboxed, game/pillarbox.h): an opaque,
// untextured rectangle [x0, x1) x [y0, y1) appended to the frame being
// recorded, in the draws' own coordinates (target pixels from the 320-wide
// view's left edge - game x times D3d_ScaleX - the band's columns negative).
// Called from the present, after the frame hash, so BOF3X_FRAMEHASH hashes
// the game's frame as the game drew it.
void AppendFill(float x0, float y0, float x1, float y1, U argb);

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

// --- bit 15 in the textures (DIV-0160, docs/stp-blend.md) ------------------
// The PlayStation's semi-transparency bit, carried in the A1R5G5B5 textures
// the game builds, in the one combination Capcom's converters never write:
// alpha set - a colour with bit 15 clear (drawn opaque by a semi-transparent
// primitive); alpha clear and colour 0 - transparent; alpha clear and colour
// non-zero - a colour with bit 15 set (blended). The converters
// (Gfx_ConvertRow, Tex_Convert16) write it only while StpTexels(): the switch
// armed (StpTexels, after the start-up fuzzes, by TexPage_Inject) and the
// backend ours (InitShim has run). Under Capcom's DirectX 6 objects an
// alpha-clear texel is transparent, so there the converters keep Capcom's rule.
extern std::uint32_t g_stp_texels;
bool StpTexels();
// Step 3: a blended draw of a texture that carries bit 15 blends only its
// bit-15 texels and draws the others opaque (render_d3d11.cpp, PSStp). The
// switch StpBlend, armed with StpTexels; nothing without it.
extern std::uint32_t g_stp_blend;
bool StpBlend();

// --- the frame-buffer grab (DIV-0125, fb_grab.h) ---------------------------
// Each does nothing unless the backend is the one recording frames
// (InitShim has run) and BOF3X_FBGRAB is not 0, so Capcom's DirectX 6 path
// and the start-up self-tests are untouched.

// The walk's move case: a move of (x, y, w, h) - the rectangle as the game
// built it, before Gfx_MoveImage clips it - to VRAM (to_x, to_y). Records a
// grab when the source lies in a display buffer.
void MoveFromFrame(int x, int y, int w, int h, int to_x, int to_y);
// Gfx_InvalidateTextures: the shadow was written over (x, y, w, h).
void ShadowWritten(int x, int y, int w, int h);
// D3d_BindTexture: `texture` (an IDirect3DTexture2) holds a direct-colour
// page whose texel (0, 0) is VRAM cell (x, y), tiling a w x h window.
void NoteDirectPage(void* texture, int x, int y, int w, int h);

// --- the quicksave (SP-G, docs/quicksave.md, the graphics participant) -----------------------
// Called at a quick-load, between two logic frames.

// Releases a surface or texture interface (vtable slot 2) right after the next
// present: the frame drawn before the load still names it. Capcom's DirectX 6
// objects (BOF3X_ORIGINAL) are released at once.
void ReleaseAfterPresent(void* object);
U DeferredReleases();      // still waiting for that present
void ReleaseDeferredNow(); // the self-test's: no present comes
// The pipe state as a record holds it: no texture (every textured draw binds
// its own), no grab (a quick-load forgets the grabs).
PipeState PipeForSnapshot();
void RestorePipe(const PipeState& state, U clear_color);
U ClearColor();
// DIV-0125: every frame-buffer grab forgotten - the pictures they hold are the
// live process's, not the quicksave's.
void ForgetGrabs();

}  // namespace render

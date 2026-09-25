// The DirectDraw- and Direct3D-shaped objects (render_shim.h). Slot numbers
// are DirectX 6's, from ddraw.h and d3d.h, and agree with the ones the
// start-up fuzzes measured against the real objects (d3d_fuzz.h,
// ddraw_fuzz.h). Every method is stdcall with `this` first, as COM is on x86.
//
// Nothing here calls the GPU, allocates from the game's CRT, or keeps a
// large local: these methods run on whatever stack the game calls them from.
#include "render/render_shim.h"

#include <windows.h>

#include <cstddef>
#include <cstdlib>
#include <cstring>

#include "hook/log.h"

namespace render {
namespace {

// --- helpers -------------------------------------------------------------------

using Rect = long[4];   // left, top, right, bottom

constexpr U kDdOk = 0;
constexpr U kInvalidParams = 0x80070057u;  // DDERR_INVALIDPARAMS
constexpr U kNoInterface = 0x80004002u;    // E_NOINTERFACE
constexpr U kSurfaceBusy = 0x887601AEu;    // DDERR_SURFACEBUSY
constexpr U kNotLocked = 0x887601E8u;      // DDERR_NOTLOCKED
constexpr U kUnsupported = 0x88760180u;    // DDERR_UNSUPPORTED

constexpr U kDescBytes = 0x7C;
// DDSURFACEDESC2 offsets
constexpr U kDescFlags = 0x04, kDescHeight = 0x08, kDescWidth = 0x0C, kDescPitch = 0x10, kDescSurface = 0x24,
            kDescPixelFormat = 0x48, kDescCaps = 0x68, kDescCaps2 = 0x6C;
// DDPIXELFORMAT offsets within it
constexpr U kPfSize = 0, kPfFlags = 4, kPfBits = 0xC, kPfR = 0x10, kPfG = 0x14, kPfB = 0x18, kPfA = 0x1C;
// DDSD_* flags
constexpr U kSdCaps = 1, kSdHeight = 2, kSdWidth = 4, kSdPitch = 8, kSdPixelFormat = 0x1000, kSdSurface = 0x800;
// DDBLT_* flags
constexpr U kBltColorFill = 0x400, kBltKeySrc = 0x8000;
// DDBLTFX
constexpr U kFxFillColor = 0x50;
// DDPF_*
constexpr U kPfRgb = 0x40;

void* Heap() { return GetProcessHeap(); }
void* Alloc(U bytes) { return HeapAlloc(Heap(), HEAP_ZERO_MEMORY, bytes); }
void Free(void* p) {
    if (p) HeapFree(Heap(), 0, p);
}

U Long(const void* p) {
    U v;
    std::memcpy(&v, p, sizeof v);
    return v;
}
void PutLong(void* p, U v) { std::memcpy(p, &v, sizeof v); }
unsigned char* Bytes(void* p) { return static_cast<unsigned char*>(p); }
const unsigned char* Bytes(const void* p) { return static_cast<const unsigned char*>(p); }

// Every surface made, for the frame reset and for validating pointers the
// game hands back.
constexpr U kMaxSurfaces = 1024;
Surface* g_surfaces[kMaxSurfaces];
U g_n_surfaces;

Frame g_frame;
PipeState g_state;
U g_clear_color;
PresentFn g_present;

// The material's handle (what GetHandle answers, what SetBackground takes).
constexpr U kMaterialHandle = 1;

// A method nobody implemented: ends the process naming the object and slot.
template <int kObject, int kSlot> long __stdcall Unsupported(void*) {
    static const char* const kNames[] = {"IDirectDraw4", "IDirectDrawSurface4", "IDirect3DTexture2",
                                         "IDirect3DDevice3", "IDirect3DViewport3", "IDirect3DMaterial3"};
    bof3::Fatal("render: %s slot %d (+0x%X) is not implemented by the backend", kNames[kObject], kSlot, kSlot * 4);
}

template <int kObject, int... kSlots> void FillUnsupported(void** table) {
    ((table[kSlots] = reinterpret_cast<void*>(&Unsupported<kObject, kSlots>)), ...);
}
template <int kObject> void FillAll(void** table) {
    FillUnsupported<kObject, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
                    25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47>(table);
}

// Slots per table; FillAll's list above must run 0..kSlots-1 to match.
constexpr U kSlots = 48;
void* g_dd_vtable[kSlots];
void* g_surface_vtable[kSlots];
void* g_texture_vtable[kSlots];
void* g_device_vtable[kSlots];
void* g_viewport_vtable[kSlots];
void* g_material_vtable[kSlots];

// The DirectDraw, device, viewport and material objects are static
// singletons: their AddRef and Release answer 1 and free nothing.
struct Object {
    void* const* vtable;
};
Object g_dd = {g_dd_vtable};
Object g_device = {g_device_vtable};
Object g_viewport = {g_viewport_vtable};
Object g_material = {g_material_vtable};

// The IDirect3DTexture2 pointer of a surface is the surface's second word.
Surface* SurfaceOfTexture(void* texture) {
    return reinterpret_cast<Surface*>(Bytes(texture) - offsetof(Surface, texture_vtable));
}
void* TextureOf(Surface* s) { return &s->texture_vtable; }

bool IsSurface(const void* p) {
    for (U i = 0; i < g_n_surfaces; ++i)
        if (g_surfaces[i] == p) return true;
    return false;
}
bool IsTexture(const void* p) {
    for (U i = 0; i < g_n_surfaces; ++i)
        if (TextureOf(g_surfaces[i]) == p) return true;
    return false;
}

// --- pixels ----------------------------------------------------------------------

// The shift and width of a channel mask.
void MaskBits(U mask, U* shift, U* bits) {
    *shift = 0;
    *bits = 0;
    if (mask == 0) return;
    while (!((mask >> *shift) & 1)) ++*shift;
    while (*shift + *bits < 32 && ((mask >> (*shift + *bits)) & 1)) ++*bits;
}
// A channel of `raw` under `mask`, widened to 8 bits (the high bits repeated
// below, as DirectDraw expanded them).
U Channel(U raw, U mask) {
    U shift, bits;
    MaskBits(mask, &shift, &bits);
    if (bits == 0) return 0;
    const U v = (raw & mask) >> shift;
    if (bits >= 8) return v >> (bits - 8);
    return (v << (8 - bits)) | (v >> (bits - (8 - bits) > 0 ? (2 * bits - 8) : 0));
}
// A pixel as 32-bit A8R8G8B8, whatever the surface's format: alpha 255 unless
// the surface has an alpha mask and the pixel's is clear.
U ReadPixel(const Surface* s, U x, U y) {
    const unsigned char* row = s->pixels + y * s->pitch;
    U raw;
    if (s->bpp == 32) {
        raw = Long(row + x * 4);
    } else {
        std::uint16_t v;
        std::memcpy(&v, row + x * 2, 2);
        raw = v;
    }
    const U a = (s->pf_amask && !(raw & s->pf_amask)) ? 0 : 0xFF;
    return (a << 24) | (Channel(raw, s->pf_rmask) << 16) | (Channel(raw, s->pf_gmask) << 8) | Channel(raw, s->pf_bmask);
}
// The top bits of an 8-bit channel placed under `mask`.
U Pack(U c8, U mask) {
    U shift, bits;
    MaskBits(mask, &shift, &bits);
    if (bits == 0) return 0;
    const U v = bits >= 8 ? (c8 << (bits - 8)) : (c8 >> (8 - bits));
    return (v << shift) & mask;
}
void WritePixel(Surface* s, U x, U y, U argb) {
    U raw = Pack((argb >> 16) & 0xFF, s->pf_rmask) | Pack((argb >> 8) & 0xFF, s->pf_gmask) | Pack(argb & 0xFF, s->pf_bmask);
    if (s->pf_amask && (argb >> 24) >= 0x80) raw |= s->pf_amask;
    unsigned char* row = s->pixels + y * s->pitch;
    if (s->bpp == 32) {
        PutLong(row + x * 4, raw);
        return;
    }
    const std::uint16_t v = static_cast<std::uint16_t>(raw);
    std::memcpy(row + x * 2, &v, 2);
}
// A raw pixel value in the surface's own format (a colour key, a fill colour).
U RawPixel(const Surface* s, U x, U y) {
    const unsigned char* row = s->pixels + y * s->pitch;
    if (s->bpp == 32) return Long(row + x * 4);
    std::uint16_t v;
    std::memcpy(&v, row + x * 2, 2);
    return v;
}
void WriteRaw(Surface* s, U x, U y, U raw) {
    unsigned char* row = s->pixels + y * s->pitch;
    if (s->bpp == 32) {
        PutLong(row + x * 4, raw);
        return;
    }
    const std::uint16_t v = static_cast<std::uint16_t>(raw);
    std::memcpy(row + x * 2, &v, 2);
}

void RectOf(const long* r, const Surface* s, long* out) {
    if (r) {
        out[0] = r[0];
        out[1] = r[1];
        out[2] = r[2];
        out[3] = r[3];
    } else {
        out[0] = 0;
        out[1] = 0;
        out[2] = static_cast<long>(s->width);
        out[3] = static_cast<long>(s->height);
    }
}
bool RectInside(const long* r, const Surface* s) {
    return r[0] >= 0 && r[1] >= 0 && r[2] <= static_cast<long>(s->width) && r[3] <= static_cast<long>(s->height) &&
           r[0] <= r[2] && r[1] <= r[3];
}

void FillRect(Surface* s, const long* r, U raw) {
    for (long y = r[1]; y < r[3]; ++y)
        for (long x = r[0]; x < r[2]; ++x) WriteRaw(s, static_cast<U>(x), static_cast<U>(y), raw);
}

// Copies src's rectangle onto dst's, stretching by nearest neighbour when the
// sizes differ, converting the depth when the surfaces differ, and skipping
// src's keyed texels when `key_src`.
void CopyRect(Surface* dst, const long* dr, const Surface* src, const long* sr, bool key_src) {
    const long dw = dr[2] - dr[0], dh = dr[3] - dr[1];
    const long sw = sr[2] - sr[0], sh = sr[3] - sr[1];
    if (dw <= 0 || dh <= 0 || sw <= 0 || sh <= 0) return;
    const bool same = dst->bpp == src->bpp;
    for (long y = 0; y < dh; ++y) {
        const long sy = sr[1] + (dh == sh ? y : y * sh / dh);
        for (long x = 0; x < dw; ++x) {
            const long sx = sr[0] + (dw == sw ? x : x * sw / dw);
            if (same) {
                const U raw = RawPixel(src, static_cast<U>(sx), static_cast<U>(sy));
                if (key_src && src->has_color_key && raw == src->color_key) continue;
                WriteRaw(dst, static_cast<U>(dr[0] + x), static_cast<U>(dr[1] + y), raw);
            } else {
                if (key_src && src->has_color_key && RawPixel(src, static_cast<U>(sx), static_cast<U>(sy)) == src->color_key)
                    continue;
                WritePixel(dst, static_cast<U>(dr[0] + x), static_cast<U>(dr[1] + y),
                           ReadPixel(src, static_cast<U>(sx), static_cast<U>(sy)));
            }
        }
    }
}

// --- the frame -------------------------------------------------------------------

TexVersion* NewVersion(Surface* s) {
    if (g_frame.arena_used + sizeof(TexVersion) > g_frame.arena_size)
        bof3::Fatal("render: the frame arena (%u bytes) is full of texture versions", g_frame.arena_size);
    auto* v = reinterpret_cast<TexVersion*>(g_frame.arena + g_frame.arena_used);
    g_frame.arena_used += sizeof(TexVersion);
    v->surface = s;
    v->pixels = nullptr;
    v->serial = g_frame.next_serial++;
    return v;
}

void Present() {
    if (g_present) g_present(g_frame);
    ResetFrame();
}

void AppendCommand(const Command& c) {
    if (g_frame.n_commands >= g_frame.max_commands)
        bof3::Fatal("render: a frame holds more than %u commands", g_frame.max_commands);
    g_frame.commands[g_frame.n_commands++] = c;
}

Vertex* AppendVertices(U n) {
    if (g_frame.n_vertices + n > g_frame.max_vertices)
        bof3::Fatal("render: a frame holds more than %u vertices", g_frame.max_vertices);
    Vertex* v = g_frame.vertices + g_frame.n_vertices;
    g_frame.n_vertices += n;
    return v;
}

// --- IDirectDrawSurface4 -----------------------------------------------------------

long __stdcall Surface_QueryInterface(void* self, const void* iid, void** out) {
    // IID_IDirect3DTexture2 {93281502-8CF8-11D0-89AB-00A0C9054129}
    static const unsigned char kTexture2[16] = {0x02, 0x15, 0x28, 0x93, 0xF8, 0x8C, 0xD0, 0x11,
                                                0x89, 0xAB, 0x00, 0xA0, 0xC9, 0x05, 0x41, 0x29};
    // IID_IUnknown {00000000-0000-0000-C000-000000000046}
    static const unsigned char kUnknown[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0xC0, 0, 0, 0, 0, 0, 0, 0x46};
    // IID_IDirectDrawSurface4 {0B2B8630-AD35-11D0-8EA6-00609797EA5B}
    static const unsigned char kSurface4[16] = {0x30, 0x86, 0x2B, 0x0B, 0x35, 0xAD, 0xD0, 0x11,
                                                0x8E, 0xA6, 0x00, 0x60, 0x97, 0x97, 0xEA, 0x5B};
    auto* s = static_cast<Surface*>(self);
    if (!out) return static_cast<long>(kInvalidParams);
    if (std::memcmp(iid, kTexture2, 16) == 0) {
        s->refs++;
        *out = TextureOf(s);
        return kDdOk;
    }
    if (std::memcmp(iid, kUnknown, 16) == 0 || std::memcmp(iid, kSurface4, 16) == 0) {
        s->refs++;
        *out = s;
        return kDdOk;
    }
    *out = nullptr;
    return static_cast<long>(kNoInterface);
}

unsigned long __stdcall Surface_AddRef(void* self) { return ++static_cast<Surface*>(self)->refs; }

unsigned long __stdcall Surface_Release(void* self) {
    auto* s = static_cast<Surface*>(self);
    if (s->refs == 0) bof3::Fatal("render: Release of a surface with no references");
    if (--s->refs > 0) return s->refs;
    // Pending draws keep their own snapshot; the GPU object goes at the next
    // present, after those draws have run (SweepReleased). The dimensions
    // and format stay: the snapshot is drawn through them
    // (docs/render-backend.md, "Released surfaces with pending draws").
    if (s->version && s->pending_draws) BeforeWrite(s);
    Free(s->pixels);
    s->pixels = nullptr;
    s->refs = 0;
    // Left in the registry with `gpu` for render_d3d11 to release; the slot is
    // reused once that is done (SweepReleased).
    return 0;
}

long __stdcall Surface_Blt(void* self, const long* dst_rect, void* src_surface, const long* src_rect, U flags, void* fx) {
    auto* dst = static_cast<Surface*>(self);
    if (dst->is_primary) {
        // The present: the back buffer onto the primary.
        Present();
        return kDdOk;
    }
    if (flags & kBltColorFill) {
        if (!fx) return static_cast<long>(kInvalidParams);
        if (dst->is_back) {
            // A fill of the back buffer is a whole-target clear: the rectangle
            // is not read, and the fill colour is taken as ARGB - the back
            // buffer's own X-8-8-8 raw value (Display_Setup makes it 32-bit).
            Command c = {};
            c.kind = Cmd::kClear;
            c.color = Long(Bytes(fx) + kFxFillColor);
            AppendCommand(c);
            return kDdOk;
        }
        long r[4];
        RectOf(dst_rect, dst, r);
        if (!RectInside(r, dst)) return static_cast<long>(kInvalidParams);
        BeforeWrite(dst);
        FillRect(dst, r, Long(Bytes(fx) + kFxFillColor));
        return kDdOk;
    }
    auto* src = static_cast<Surface*>(src_surface);
    if (!src || !IsSurface(src)) return static_cast<long>(kInvalidParams);
    if (src->is_primary || src->is_back)
        bof3::Fatal("render: a Blt reads the %s surface back - not built (docs/render-backend.md)",
                    src->is_primary ? "primary" : "back");
    if (dst->is_back) bof3::Fatal("render: a Blt onto the back buffer from a surface - not built");
    long dr[4], sr[4];
    RectOf(dst_rect, dst, dr);
    RectOf(src_rect, src, sr);
    if (!RectInside(dr, dst) || !RectInside(sr, src)) return static_cast<long>(kInvalidParams);
    BeforeWrite(dst);
    CopyRect(dst, dr, src, sr, (flags & kBltKeySrc) != 0);
    return kDdOk;
}

long __stdcall Surface_BltFast(void* self, U x, U y, void* src_surface, const long* src_rect, U trans) {
    auto* dst = static_cast<Surface*>(self);
    auto* src = static_cast<Surface*>(src_surface);
    if (!src || !IsSurface(src)) return static_cast<long>(kInvalidParams);
    if (dst->is_primary || dst->is_back || src->is_primary || src->is_back)
        bof3::Fatal("render: BltFast touching the primary or back buffer - not built");
    long sr[4];
    RectOf(src_rect, src, sr);
    long dr[4] = {static_cast<long>(x), static_cast<long>(y), static_cast<long>(x) + (sr[2] - sr[0]),
                  static_cast<long>(y) + (sr[3] - sr[1])};
    if (!RectInside(dr, dst) || !RectInside(sr, src)) return static_cast<long>(kInvalidParams);
    BeforeWrite(dst);
    CopyRect(dst, dr, src, sr, (trans & 1) != 0);   // DDBLTFAST_SRCCOLORKEY
    return kDdOk;
}

long __stdcall Surface_Flip(void* self, void*, U) {
    auto* s = static_cast<Surface*>(self);
    if (!s->is_primary) return static_cast<long>(kUnsupported);
    Present();
    return kDdOk;
}

void FillDesc(const Surface* s, unsigned char* d, bool with_surface) {
    std::memset(d + 4, 0, kDescBytes - 4);
    PutLong(d + kDescFlags, kSdCaps | kSdHeight | kSdWidth | kSdPitch | kSdPixelFormat | (with_surface ? kSdSurface : 0));
    PutLong(d + kDescHeight, s->height);
    PutLong(d + kDescWidth, s->width);
    PutLong(d + kDescPitch, s->pitch);
    if (with_surface) PutLong(d + kDescSurface, static_cast<U>(reinterpret_cast<std::uintptr_t>(s->pixels)));
    unsigned char* pf = d + kDescPixelFormat;
    PutLong(pf + kPfSize, 0x20);
    PutLong(pf + kPfFlags, s->pf_flags);
    PutLong(pf + kPfBits, s->pf_bits);
    PutLong(pf + kPfR, s->pf_rmask);
    PutLong(pf + kPfG, s->pf_gmask);
    PutLong(pf + kPfB, s->pf_bmask);
    PutLong(pf + kPfA, s->pf_amask);
    PutLong(d + kDescCaps, s->caps);
    PutLong(d + kDescCaps2, s->caps2);
}

long __stdcall Surface_GetSurfaceDesc(void* self, void* desc) {
    if (!desc || Long(desc) != kDescBytes) return static_cast<long>(kInvalidParams);
    FillDesc(static_cast<Surface*>(self), Bytes(desc), false);
    return kDdOk;
}

long __stdcall Surface_IsLost(void*) { return kDdOk; }
long __stdcall Surface_Restore(void*) { return kDdOk; }

long __stdcall Surface_Lock(void* self, const long* rect, void* desc, U, void*) {
    auto* s = static_cast<Surface*>(self);
    if (!desc || Long(desc) != kDescBytes) return static_cast<long>(kInvalidParams);
    if (s->is_primary || s->is_back) bof3::Fatal("render: Lock of the %s surface - not built", s->is_primary ? "primary" : "back");
    if (s->locked) return static_cast<long>(kSurfaceBusy);
    if (rect) bof3::Fatal("render: Lock of a sub-rectangle - not built");
    BeforeWrite(s);
    s->locked = true;
    FillDesc(s, Bytes(desc), true);
    return kDdOk;
}

long __stdcall Surface_Unlock(void* self, const void*) {
    auto* s = static_cast<Surface*>(self);
    if (!s->locked) return static_cast<long>(kNotLocked);
    s->locked = false;
    return kDdOk;
}

long __stdcall Surface_SetColorKey(void* self, U flags, const U* key) {
    auto* s = static_cast<Surface*>(self);
    if (flags != 8) return static_cast<long>(kUnsupported);   // DDCKEY_SRCBLT only
    if (!key) {
        s->has_color_key = false;
        return kDdOk;
    }
    BeforeWrite(s);   // the key is part of how the pixels upload
    s->color_key = key[0];
    s->has_color_key = true;
    return kDdOk;
}

// --- IDirect3DTexture2 --------------------------------------------------------------

long __stdcall Texture_QueryInterface(void* self, const void* iid, void** out) {
    return Surface_QueryInterface(SurfaceOfTexture(self), iid, out);
}
unsigned long __stdcall Texture_AddRef(void* self) { return Surface_AddRef(SurfaceOfTexture(self)); }
unsigned long __stdcall Texture_Release(void* self) { return Surface_Release(SurfaceOfTexture(self)); }
long __stdcall Texture_GetHandle(void* self, void*, U* handle) {
    if (!handle) return static_cast<long>(kInvalidParams);
    // The interface pointer itself; nothing here takes a handle back
    // (SetTexture takes the interface).
    *handle = static_cast<U>(reinterpret_cast<std::uintptr_t>(self));
    return kDdOk;
}
long __stdcall Texture_PaletteChanged(void*, U, U) { return kDdOk; }
long __stdcall Texture_Load(void* self, void* from) {
    auto* dst = SurfaceOfTexture(self);
    if (!from || !IsTexture(from)) return static_cast<long>(kInvalidParams);
    auto* src = SurfaceOfTexture(from);
    long dr[4], sr[4];
    RectOf(nullptr, dst, dr);
    RectOf(nullptr, src, sr);
    BeforeWrite(dst);
    CopyRect(dst, dr, src, sr, false);
    return kDdOk;
}

// --- IDirectDraw4 ---------------------------------------------------------------------

long __stdcall Dd_QueryInterface(void*, const void*, void** out) {
    if (out) *out = nullptr;
    return static_cast<long>(kNoInterface);
}
unsigned long __stdcall Dd_AddRef(void*) { return 1; }
unsigned long __stdcall Dd_Release(void*) { return 1; }

long __stdcall Dd_CreateSurface(void*, const void* desc, Surface** out, void*) {
    if (!desc || !out || Long(desc) != kDescBytes) return static_cast<long>(kInvalidParams);
    const unsigned char* d = Bytes(desc);
    const U flags = Long(d + kDescFlags);
    if (!(flags & kSdWidth) || !(flags & kSdHeight)) return static_cast<long>(kInvalidParams);
    U bpp = 32, rmask = 0xFF0000, gmask = 0xFF00, bmask = 0xFF, amask = 0;
    if (flags & kSdPixelFormat) {
        const unsigned char* pf = d + kDescPixelFormat;
        if (Long(pf + kPfSize) != 0x20) return static_cast<long>(kInvalidParams);
        bpp = Long(pf + kPfBits);
        if (bpp != 16 && bpp != 32) bof3::Fatal("render: CreateSurface with %u bits a pixel - not built", bpp);
        rmask = Long(pf + kPfR);
        gmask = Long(pf + kPfG);
        bmask = Long(pf + kPfB);
        amask = (Long(pf + kPfFlags) & 1) ? Long(pf + kPfA) : 0;   // DDPF_ALPHAPIXELS
        if (!(Long(pf + kPfFlags) & kPfRgb) || rmask == 0 || gmask == 0 || bmask == 0)
            bof3::Fatal("render: CreateSurface with a non-RGB format (flags 0x%X) - not built", Long(pf + kPfFlags));
    }
    Surface* s = MakeSurface(Long(d + kDescWidth), Long(d + kDescHeight), bpp, (flags & kSdCaps) ? Long(d + kDescCaps) : 0,
                             (flags & kSdCaps) ? Long(d + kDescCaps2) : 0, false, false);
    if (!s) return static_cast<long>(0x8007000Eu);   // DDERR_OUTOFMEMORY
    s->pf_rmask = rmask;
    s->pf_gmask = gmask;
    s->pf_bmask = bmask;
    s->pf_amask = amask;
    s->pf_flags = kPfRgb | (amask ? 1 : 0);
    *out = s;
    return kDdOk;
}

// --- IDirect3DDevice3 -------------------------------------------------------------

long __stdcall Device_QueryInterface(void*, const void*, void** out) {
    if (out) *out = nullptr;
    return static_cast<long>(kNoInterface);
}
unsigned long __stdcall Device_AddRef(void*) { return 1; }
unsigned long __stdcall Device_Release(void*) { return 1; }
long __stdcall Device_BeginScene(void*) { return kDdOk; }
long __stdcall Device_EndScene(void*) { return kDdOk; }

// D3DRENDERSTATETYPE
constexpr U kRsTexturePerspective = 4, kRsZEnable = 7, kRsFillMode = 8, kRsShadeMode = 9, kRsZWrite = 14,
            kRsAlphaTest = 15, kRsSrcBlend = 19, kRsDestBlend = 20, kRsTextureMapBlend = 21, kRsCullMode = 22,
            kRsZFunc = 23, kRsAlphaRef = 24, kRsAlphaFunc = 25, kRsDither = 26, kRsAlphaBlend = 27, kRsFog = 28,
            kRsSpecular = 29, kRsColorKey = 41, kRsLighting = 137;
// D3DTEXTURESTAGESTATETYPE
constexpr U kTsColorOp = 1, kTsColorArg1 = 2, kTsColorArg2 = 3, kTsAlphaOp = 4, kTsAlphaArg1 = 5, kTsAlphaArg2 = 6,
            kTsMagFilter = 16, kTsMinFilter = 17, kTsMipFilter = 18;

long __stdcall Device_SetRenderState(void*, U state, U value) {
    switch (state) {
    case kRsShadeMode: g_state.flat = (value == 1); break;
    case kRsAlphaTest: g_state.alpha_test = value != 0; break;
    case kRsSrcBlend: g_state.src_blend = value; break;
    case kRsDestBlend: g_state.dst_blend = value; break;
    case kRsAlphaRef: g_state.alpha_ref = value & 0xFF; break;
    case kRsAlphaFunc: g_state.alpha_func = value; break;
    case kRsAlphaBlend: g_state.blend_enable = value != 0; break;
    case kRsSpecular: g_state.specular = value != 0; break;
    case kRsColorKey: g_state.color_key = value != 0; break;
    case kRsZEnable:
    case kRsZWrite:
        if (value != 0) bof3::Fatal("render: SetRenderState(%u, %u): a depth test - not built", state, value);
        break;
    case kRsCullMode:
        if (value != 1) bof3::Fatal("render: SetRenderState(CULLMODE, %u): culling - not built", value);
        break;
    case kRsFog:
    case kRsLighting:
        if (value != 0) bof3::Fatal("render: SetRenderState(%u, %u): fog or lighting - not built", state, value);
        break;
    case kRsFillMode:
        if (value != 3) bof3::Fatal("render: SetRenderState(FILLMODE, %u) - not built", value);
        break;
    case kRsTexturePerspective:
    case kRsTextureMapBlend:
    case kRsZFunc:
    case kRsDither:
        break;   // no effect on pre-transformed, unlit, undithered output
    default: {
        static U seen[16];
        static U n_seen;
        bool known = false;
        for (U i = 0; i < n_seen; ++i) known |= seen[i] == state;
        if (!known) {
            if (n_seen < 16) seen[n_seen++] = state;
            bof3::Log("render: SetRenderState(%u, %u) ignored (first time)", state, value);
        }
    }
    }
    return kDdOk;
}

constexpr float kZNearest = 1.0f / 4096.0f;      // the smallest depth Gte_PrimDepths4_10B hands the handlers
constexpr float kRhwNearest = 0.1f / kZNearest;  // 409.6, the port's rhw at that depth

long __stdcall Device_DrawPrimitive(void*, U type, U fvf, const void* vertices, U count, U) {
    if (fvf != 0x1C4) bof3::Fatal("render: DrawPrimitive with vertex format 0x%X (not TLVERTEX)", fvf);
    if (!vertices || count == 0) return static_cast<long>(kInvalidParams);
    const auto* in = static_cast<const Vertex*>(vertices);
    // BOF3X_DRAWLOG_RGB=RRGGBB logs every draw with a vertex of that diffuse colour
    // (the first 64): the way to see what the game handed us for one primitive.
    {
        static U want = 0xFFFFFFFF, logged = 0;
        if (want == 0xFFFFFFFF) {
            char text[16];
            want = GetEnvironmentVariableA("BOF3X_DRAWLOG_RGB", text, sizeof text) > 0 ? std::strtoul(text, nullptr, 16) & 0xFFFFFF : 0x1000000;
        }
        if (want < 0x1000000 && logged < 64) {
            for (U i = 0; i < count; ++i) {
                if ((in[i].diffuse & 0xFFFFFF) != want) continue;
                ++logged;
                bof3::Log("drawlog: type %u count %u flat %u blend_enable %u", type, count, (unsigned)g_state.flat, (unsigned)g_state.blend_enable);
                for (U k = 0; k < count; ++k)
                    bof3::Log("drawlog:   v%u x %g y %g z %g rhw %g diffuse %08X specular %08X uv %g %g", k, in[k].x, in[k].y, in[k].z, in[k].rhw, in[k].diffuse, in[k].specular, in[k].u, in[k].v);
                break;
            }
        }
    }
    Command c = {};
    c.kind = Cmd::kDraw;
    c.state = g_state;
    c.first = g_frame.n_vertices;
    // D3DPT_: 1 points, 2 line list, 3 line strip, 4 triangle list, 5 triangle strip, 6 triangle fan
    switch (type) {
    case 1: {
        c.state.topology = 2;
        Vertex* out = AppendVertices(count);
        std::memcpy(out, in, count * sizeof(Vertex));
        c.count = count;
        break;
    }
    case 2:
    case 3: {
        c.state.topology = 1;
        const U lines = type == 2 ? count / 2 : (count >= 2 ? count - 1 : 0);
        if (lines == 0) return kDdOk;
        Vertex* out = AppendVertices(lines * 2);
        for (U i = 0; i < lines; ++i) {
            const U a = type == 2 ? i * 2 : i;
            out[i * 2] = in[a];
            out[i * 2 + 1] = in[a + 1];
            if (c.state.flat) out[i * 2 + 1].diffuse = out[i * 2].diffuse, out[i * 2 + 1].specular = out[i * 2].specular;
        }
        c.count = lines * 2;
        break;
    }
    case 4:
    case 5:
    case 6: {
        c.state.topology = 0;
        const U tris = type == 4 ? count / 3 : (count >= 3 ? count - 2 : 0);
        if (tris == 0) return kDdOk;
        Vertex* out = AppendVertices(tris * 3);
        for (U i = 0; i < tris; ++i) {
            U a, b, d;
            if (type == 4) a = i * 3, b = a + 1, d = a + 2;
            else if (type == 5) a = i, b = i + 1, d = i + 2;
            else a = 0, b = i + 1, d = i + 2;
            out[i * 3] = in[a];
            out[i * 3 + 1] = in[b];
            out[i * 3 + 2] = in[d];
            if (c.state.flat) {
                // Flat shading: the face takes the colour of its first vertex
                // (for a strip, vertex i of triangle i; for a fan, the shared vertex 0).
                for (U k = 1; k < 3; ++k) {
                    out[i * 3 + k].diffuse = out[i * 3].diffuse;
                    out[i * 3 + k].specular = out[i * 3].specular;
                }
            }
        }
        c.count = tris * 3;
        break;
    }
    default:
        bof3::Fatal("render: DrawPrimitive of type %u - not built", type);
    }
    // DIV-0044: a corner nearer than the nearest depth the game otherwise uses,
    // 1/4096 (rhw 409.6) - in practice depth 0, where the port's rhw = 0.1 / z
    // is infinite - is drawn at that depth instead of vanishing. Capcom's
    // Direct3D 6 device dropped such a primitive; the world map's compass
    // needle (0x408530, D41 in docs/known-defects.md) is the one seen.
    // Dividing by the infinity ourselves collapsed those corners to the screen
    // centre - the purple sliver of the world-map A/B.
    {
        static U logged = 0;
        Vertex* v = g_frame.vertices + c.first;
        for (U i = 0; i < c.count; ++i) {
            if (v[i].rhw <= kRhwNearest) continue;   // false for inf and NaN too
            if (logged++ < 4)
                bof3::Log("render: DrawPrimitive corner %u at z %g rhw %g - drawn at the nearest depth (DIV-0044)", i, v[i].z, v[i].rhw);
            v[i].rhw = kRhwNearest;
            v[i].z = kZNearest;
        }
    }
    if (c.state.texture) c.state.texture->surface->pending_draws++;
    AppendCommand(c);
    return kDdOk;
}

long __stdcall Device_SetTexture(void*, U stage, void* texture) {
    if (stage != 0) bof3::Fatal("render: SetTexture on stage %u", stage);
    if (!texture) {
        g_state.texture = nullptr;
        return kDdOk;
    }
    if (!IsTexture(texture)) {
        // known-defects D18: with all glyph slots in use the game hands vertex
        // bits here. DirectX refused it; so do we, and the draw goes untextured.
        static U logged;
        if (logged++ < 8) bof3::Log("render: SetTexture(0, 0x%08X) is not a texture - refused", static_cast<U>(reinterpret_cast<std::uintptr_t>(texture)));
        g_state.texture = nullptr;
        return static_cast<long>(kInvalidParams);
    }
    g_state.texture = Use(SurfaceOfTexture(texture));
    return kDdOk;
}

long __stdcall Device_SetTextureStageState(void*, U stage, U type, U value) {
    if (stage != 0) bof3::Fatal("render: SetTextureStageState on stage %u", stage);
    switch (type) {
    case kTsAlphaOp: g_state.alpha_modulate = (value == 4); break;   // D3DTOP_MODULATE 4, SELECTARG2 3
    case kTsMagFilter:
    case kTsMinFilter: g_state.point_filter = (value == 1); break;    // D3DTFG_POINT 1, LINEAR 2
    case kTsColorOp:
    case kTsColorArg1:
    case kTsColorArg2:
    case kTsAlphaArg1:
    case kTsAlphaArg2:
    case kTsMipFilter:
        break;
    default:
        bof3::Log("render: SetTextureStageState(0, %u, %u) ignored", type, value);
    }
    return kDdOk;
}

// --- IDirect3DViewport3 --------------------------------------------------------------

long __stdcall Viewport_QueryInterface(void*, const void*, void** out) {
    if (out) *out = nullptr;
    return static_cast<long>(kNoInterface);
}
unsigned long __stdcall Viewport_AddRef(void*) { return 1; }
unsigned long __stdcall Viewport_Release(void*) { return 1; }
long __stdcall Viewport_SetBackground(void*, U handle) {
    if (handle != kMaterialHandle) bof3::Fatal("render: SetBackground with handle %u", handle);
    return kDdOk;
}
long __stdcall Viewport_Clear(void*, U count, const long* rects, U flags) {
    if (!(flags & 1)) return kDdOk;   // D3DCLEAR_TARGET
    if (count != 1 || !rects) bof3::Fatal("render: Clear of %u rectangles - not built", count);
    // The one rectangle is not read: the clear covers the whole target (RunFrame).
    Command c = {};
    c.kind = Cmd::kClear;
    c.color = g_clear_color;
    AppendCommand(c);
    return kDdOk;
}

// --- IDirect3DMaterial3 --------------------------------------------------------------

long __stdcall Material_QueryInterface(void*, const void*, void** out) {
    if (out) *out = nullptr;
    return static_cast<long>(kNoInterface);
}
unsigned long __stdcall Material_AddRef(void*) { return 1; }
unsigned long __stdcall Material_Release(void*) { return 1; }
long __stdcall Material_SetMaterial(void*, const void* material) {
    if (!material || Long(material) != 0x50) return static_cast<long>(kInvalidParams);   // D3DMATERIAL's dwSize
    float rgba[4];
    std::memcpy(rgba, Bytes(material) + 4, sizeof rgba);   // dcvDiffuse
    auto channel = [](float f) -> U {
        if (!(f > 0.0f)) return 0;
        if (f >= 1.0f) return 255;
        return static_cast<U>(f * 255.0f + 0.5f);
    };
    g_clear_color = (channel(rgba[3]) << 24) | (channel(rgba[0]) << 16) | (channel(rgba[1]) << 8) | channel(rgba[2]);
    return kDdOk;
}
long __stdcall Material_GetHandle(void*, void*, U* handle) {
    if (!handle) return static_cast<long>(kInvalidParams);
    *handle = kMaterialHandle;
    return kDdOk;
}

void BuildTables() {
    static bool built;
    if (built) return;
    built = true;
    FillAll<0>(g_dd_vtable);
    FillAll<1>(g_surface_vtable);
    FillAll<2>(g_texture_vtable);
    FillAll<3>(g_device_vtable);
    FillAll<4>(g_viewport_vtable);
    FillAll<5>(g_material_vtable);
    auto put = [](void** t, U slot, auto fn) { t[slot] = reinterpret_cast<void*>(fn); };
    put(g_dd_vtable, 0, &Dd_QueryInterface);
    put(g_dd_vtable, 1, &Dd_AddRef);
    put(g_dd_vtable, 2, &Dd_Release);
    put(g_dd_vtable, 6, &Dd_CreateSurface);
    put(g_surface_vtable, 0, &Surface_QueryInterface);
    put(g_surface_vtable, 1, &Surface_AddRef);
    put(g_surface_vtable, 2, &Surface_Release);
    put(g_surface_vtable, 5, &Surface_Blt);
    put(g_surface_vtable, 7, &Surface_BltFast);
    put(g_surface_vtable, 11, &Surface_Flip);
    put(g_surface_vtable, 22, &Surface_GetSurfaceDesc);
    put(g_surface_vtable, 24, &Surface_IsLost);
    put(g_surface_vtable, 25, &Surface_Lock);
    put(g_surface_vtable, 27, &Surface_Restore);
    put(g_surface_vtable, 29, &Surface_SetColorKey);
    put(g_surface_vtable, 32, &Surface_Unlock);
    put(g_texture_vtable, 0, &Texture_QueryInterface);
    put(g_texture_vtable, 1, &Texture_AddRef);
    put(g_texture_vtable, 2, &Texture_Release);
    put(g_texture_vtable, 3, &Texture_GetHandle);
    put(g_texture_vtable, 4, &Texture_PaletteChanged);
    put(g_texture_vtable, 5, &Texture_Load);
    put(g_device_vtable, 0, &Device_QueryInterface);
    put(g_device_vtable, 1, &Device_AddRef);
    put(g_device_vtable, 2, &Device_Release);
    put(g_device_vtable, 9, &Device_BeginScene);
    put(g_device_vtable, 10, &Device_EndScene);
    put(g_device_vtable, 22, &Device_SetRenderState);
    put(g_device_vtable, 28, &Device_DrawPrimitive);
    put(g_device_vtable, 38, &Device_SetTexture);
    put(g_device_vtable, 40, &Device_SetTextureStageState);
    put(g_viewport_vtable, 0, &Viewport_QueryInterface);
    put(g_viewport_vtable, 1, &Viewport_AddRef);
    put(g_viewport_vtable, 2, &Viewport_Release);
    put(g_viewport_vtable, 8, &Viewport_SetBackground);
    put(g_viewport_vtable, 12, &Viewport_Clear);
    put(g_material_vtable, 0, &Material_QueryInterface);
    put(g_material_vtable, 1, &Material_AddRef);
    put(g_material_vtable, 2, &Material_Release);
    put(g_material_vtable, 3, &Material_SetMaterial);
    put(g_material_vtable, 5, &Material_GetHandle);
}

}  // namespace

// --- the public face --------------------------------------------------------------------

void* DirectDrawObject() {
    BuildTables();
    return &g_dd;
}
void* DeviceObject() {
    BuildTables();
    return &g_device;
}
void* ViewportObject() {
    BuildTables();
    return &g_viewport;
}
void* MaterialObject() {
    BuildTables();
    return &g_material;
}

Surface* MakeSurface(U width, U height, U bpp, U caps, U caps2, bool primary, bool back) {
    BuildTables();
    if (bpp != 16 && bpp != 32) bof3::Fatal("render: MakeSurface with %u bits a pixel", bpp);
    if (width == 0 || height == 0 || width > 4096 || height > 4096) return nullptr;
    // Reuse a released slot whose GPU object is gone; else append.
    Surface* s = nullptr;
    for (U i = 0; i < g_n_surfaces; ++i) {
        if (g_surfaces[i]->refs == 0 && g_surfaces[i]->gpu == nullptr && g_surfaces[i]->pixels == nullptr) {
            s = g_surfaces[i];
            break;
        }
    }
    if (!s) {
        if (g_n_surfaces >= kMaxSurfaces) bof3::Fatal("render: more than %u surfaces", kMaxSurfaces);
        s = static_cast<Surface*>(Alloc(sizeof(Surface)));
        if (!s) return nullptr;
        g_surfaces[g_n_surfaces++] = s;
    }
    std::memset(s, 0, sizeof *s);
    s->vtable = g_surface_vtable;
    s->texture_vtable = g_texture_vtable;
    s->refs = 1;
    s->width = width;
    s->height = height;
    s->bpp = bpp;
    s->pitch = ((width * (bpp / 8)) + 15) & ~15u;
    s->caps = caps;
    s->caps2 = caps2;
    s->is_primary = primary;
    s->is_back = back;
    s->dirty = true;
    s->pf_flags = kPfRgb;
    s->pf_bits = bpp;
    if (bpp == 16) {
        s->pf_rmask = 0xF800;
        s->pf_gmask = 0x07E0;
        s->pf_bmask = 0x001F;
    } else {
        s->pf_rmask = 0xFF0000;
        s->pf_gmask = 0x00FF00;
        s->pf_bmask = 0x0000FF;
    }
    if (!primary) {
        s->pixels = static_cast<unsigned char*>(Alloc(s->pitch * height));
        if (!s->pixels) {
            s->refs = 0;
            return nullptr;
        }
    }
    return s;
}

bool ResizeSurface(Surface* s, U width, U height) {
    if (!s->is_primary && !s->is_back) bof3::Fatal("render: ResizeSurface of a texture surface");
    const U pitch = ((width * (s->bpp / 8)) + 15) & ~15u;
    if (s->pixels) {
        unsigned char* pixels = static_cast<unsigned char*>(Alloc(pitch * height));
        if (!pixels) return false;
        Free(s->pixels);
        s->pixels = pixels;
    }
    s->width = width;
    s->height = height;
    s->pitch = pitch;
    s->dirty = true;
    return true;
}

PipeState& CurrentState() { return g_state; }
void SetClearColor(U argb) { g_clear_color = argb; }
Frame& CurrentFrame() { return g_frame; }

void ResetFrame() {
    g_frame.n_vertices = 0;
    g_frame.n_commands = 0;
    g_frame.arena_used = 0;
    for (U i = 0; i < g_n_surfaces; ++i) {
        g_surfaces[i]->version = nullptr;
        g_surfaces[i]->pending_draws = 0;
    }
    // The state's texture pointed into the arena.
    if (g_state.texture) g_state.texture = Use(g_state.texture->surface);
}

void SetPresentHook(PresentFn fn) { g_present = fn; }

void InitShim(U max_vertices, U max_commands, U arena_bytes) {
    BuildTables();
    g_frame.vertices = static_cast<Vertex*>(Alloc(max_vertices * sizeof(Vertex)));
    g_frame.commands = static_cast<Command*>(Alloc(max_commands * sizeof(Command)));
    g_frame.arena = static_cast<unsigned char*>(Alloc(arena_bytes));
    if (!g_frame.vertices || !g_frame.commands || !g_frame.arena) bof3::Fatal("render: out of memory for the frame");
    g_frame.max_vertices = max_vertices;
    g_frame.max_commands = max_commands;
    g_frame.arena_size = arena_bytes;
    g_frame.next_serial = 1;
    std::memset(&g_state, 0, sizeof g_state);
    g_state.src_blend = 2;   // D3DBLEND_ONE
    g_state.dst_blend = 1;   // D3DBLEND_ZERO
    g_state.alpha_func = 8;  // D3DCMP_ALWAYS
    g_state.point_filter = false;
    g_state.flat = false;
}

TexVersion* Use(Surface* s) {
    if (!s->version) s->version = NewVersion(s);
    return s->version;
}

void BeforeWrite(Surface* s) {
    s->dirty = true;
    if (!s->version || s->pending_draws == 0 || !s->pixels) {
        // Nothing drawn against it this frame: the GPU copy is simply stale.
        return;
    }
    const U bytes = s->pitch * s->height;
    if (g_frame.arena_used + bytes > g_frame.arena_size)
        bof3::Fatal("render: the frame arena (%u bytes) cannot hold a %u-byte texture snapshot", g_frame.arena_size, bytes);
    unsigned char* copy = g_frame.arena + g_frame.arena_used;
    g_frame.arena_used += bytes;
    std::memcpy(copy, s->pixels, bytes);
    s->version->pixels = copy;
    s->version = nullptr;
    s->pending_draws = 0;
    // The device's bound texture moves to a fresh version, so later draws see the new pixels.
    if (g_state.texture && g_state.texture->surface == s) g_state.texture = Use(s);
}

// For render_d3d11.cpp: every surface, live or released.
Surface* const* AllSurfaces(U* n) {
    *n = g_n_surfaces;
    return g_surfaces;
}

}  // namespace render

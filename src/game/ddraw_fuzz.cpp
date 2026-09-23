// The surface fuzz's fake DirectDraw. See ddraw_fuzz.h and docs/tex-page.md
// section 5.
#include "game/ddraw_fuzz.h"

#include <windows.h>

#include <cstdio>
#include <cstring>
#include <utility>

#include "hook/log.h"

namespace ddraw_fuzz {

Log* g_log = nullptr;

void Log::Clear() { std::memset(this, 0, sizeof *this); }

void Record(U what, U a0, U a1, U a2, U a3, U a4, U a5, U a6, U a7, U a8, U a9, U a10, U a11) {
    if (!g_log) bof3::Fatal("ddraw_fuzz: a call was recorded with no log set");
    if (g_log->n < kMaxCalls) g_log->calls[g_log->n] = {what, {a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11}};
    ++g_log->n;
}

bool SameLog(const Log& ours, const Log& theirs, char* why) {
    if (ours.n != theirs.n) {
        std::snprintf(why, 200, "%u calls out, the original %u", ours.n, theirs.n);
        return false;
    }
    const unsigned n = ours.n < kMaxCalls ? ours.n : kMaxCalls;
    for (unsigned i = 0; i < n; ++i) {
        const Call& a = ours.calls[i];
        const Call& b = theirs.calls[i];
        if (a.what != b.what || std::memcmp(a.a, b.a, sizeof a.a) != 0) {
            unsigned k = 0;
            while (k < 12 && a.a[k] == b.a[k]) ++k;
            std::snprintf(why, 200, "call %u: 0x%X(%X %X %X %X ..) argument %u 0x%X, the original 0x%X(%X %X %X %X ..) 0x%X",
                          i, (unsigned)a.what, (unsigned)a.a[0], (unsigned)a.a[1], (unsigned)a.a[2], (unsigned)a.a[3],
                          k, k < 12 ? (unsigned)a.a[k] : 0u, (unsigned)b.what, (unsigned)b.a[0], (unsigned)b.a[1],
                          (unsigned)b.a[2], (unsigned)b.a[3], k < 12 ? (unsigned)b.a[k] : 0u);
            return false;
        }
    }
    if (ours.n_descs != theirs.n_descs) {
        std::snprintf(why, 200, "%u surface descriptors, the original %u", ours.n_descs, theirs.n_descs);
        return false;
    }
    for (unsigned i = 0; i < ours.n_descs && i < kMaxDescs; ++i)
        for (unsigned k = 0; k < kDescBytes; ++k)
            if (ours.descs[i][k] != theirs.descs[i][k]) {
                std::snprintf(why, 200, "surface descriptor %u differs at +0x%X: 0x%02X, the original 0x%02X", i, k,
                              ours.descs[i][k], theirs.descs[i][k]);
                return false;
            }
    return true;
}

namespace {

Disturb g_disturb = nullptr;

void After(U what) {
    if (g_disturb) g_disturb(what);
}

U Get(const void* p, U offset) {
    U v;
    std::memcpy(&v, static_cast<const unsigned char*>(p) + offset, 4);
    return v;
}
void Put(void* p, U offset, U v) { std::memcpy(static_cast<unsigned char*>(p) + offset, &v, 4); }

// --- the objects -----------------------------------------------------------------

struct Surface {
    void** vtbl;
    unsigned index;
    U width, height, bpp, pitch, caps;
    bool locked;
    unsigned char format[0x20];   // the DDPIXELFORMAT it was made with
};
struct Texture {
    void** vtbl;
    unsigned index;
};
struct DirectDraw {
    void** vtbl;
};

constexpr int kSlots = 64;
void* g_surface_vtbl[kSlots];
void* g_texture_vtbl[kSlots];
void* g_dd_vtbl[kSlots];
Surface g_surfaces[kMaxSurfaces];
Texture g_textures[kMaxSurfaces];
DirectDraw g_dd = {g_dd_vtbl};

// What MakeSurface made, to put back at each BeginPass.
struct Made {
    U width, height, bpp, pitch;
};
Made g_made_list[kMaxSurfaces];
unsigned g_made = 0;        // surfaces MakeSurface made this round
unsigned g_count = 0;       // surfaces that exist this pass
unsigned g_count_of[2] = {0, 0};
bool g_in_pass = false;
int g_pass = 0;
U g_seed = 0;
unsigned char* g_arena[2] = {nullptr, nullptr};

U g_pitches[kMaxSurfaces];
unsigned g_n_pitches = 0;

struct Plan {
    U what;
    unsigned nth;
    U result;
    unsigned seen;
};
Plan g_plans[4];
unsigned g_n_plans = 0;

// True (and the result) if this call of `what` is one a plan fails.
bool Planned(U what, U* result) {
    bool fail = false;
    for (unsigned i = 0; i < g_n_plans; ++i) {
        Plan& p = g_plans[i];
        if (p.what != what) continue;
        if (p.seen == p.nth && !fail) {
            fail = true;
            *result = p.result;
        }
        ++p.seen;
    }
    return fail;
}

unsigned char* Buffer(unsigned index, int pass) { return g_arena[pass] + static_cast<std::size_t>(index) * kSlotBytes; }

// The bytes a surface of this shape can have written: 256 rows (every builder
// writes 256 whatever the surface's height), each row up to 256 texels of 4
// bytes, and a margin; never beyond the slot.
U Span(const Surface& s) {
    const U rows = s.height > 256 ? s.height : 256;
    const U row = (s.width > 256 ? s.width : 256) * 4;
    U span = s.pitch * rows + row + 64;
    if (span > kSlotBytes) span = kSlotBytes;
    return span;
}

unsigned char FillByte(unsigned index) {
    U h = (g_seed ^ (index * 0x9E3779B1u)) * 0x85EBCA6Bu;
    return static_cast<unsigned char>(h >> 24);
}

void Shape(Surface& s, unsigned index, U width, U height, U bpp, U pitch) {
    s.vtbl = g_surface_vtbl;
    s.index = index;
    s.width = width;
    s.height = height;
    s.bpp = bpp;
    s.pitch = pitch ? pitch : width * bpp;
    s.caps = 0;
    s.locked = false;
    std::memset(s.format, 0, sizeof s.format);
    Put(s.format, 0, 0x20);
    Put(s.format, 0xC, bpp * 8);
    if (s.pitch > 4096 || s.width > 1024)
        bof3::Fatal("ddraw_fuzz: a surface of width %u pitch %u does not fit its buffer", (unsigned)s.width,
                    (unsigned)s.pitch);
    g_textures[index] = {g_texture_vtbl, index};
    std::memset(Buffer(index, g_pass), FillByte(index), Span(s));
}

Surface* AsSurface(const void* p) {
    for (unsigned i = 0; i < kMaxSurfaces; ++i)
        if (p == &g_surfaces[i]) return &g_surfaces[i];
    return nullptr;
}
Texture* AsTexture(const void* p) {
    for (unsigned i = 0; i < kMaxSurfaces; ++i)
        if (p == &g_textures[i]) return &g_textures[i];
    return nullptr;
}

struct Rect {
    std::int32_t l, t, r, b;
};
// A RECT *'s four values for the log, or four kNullRect.
void RectArgs(const void* p, U* out) {
    if (!p) {
        out[0] = out[1] = out[2] = out[3] = kNullRect;
        return;
    }
    for (int i = 0; i < 4; ++i) out[i] = Get(p, 4 * i);
}
// The rectangle a RECT * names on a surface: the whole surface for null. False
// if it is empty or does not lie inside the surface.
bool RectOn(const Surface& s, const void* p, Rect* r) {
    if (p) std::memcpy(r, p, sizeof *r);
    else *r = {0, 0, static_cast<std::int32_t>(s.width), static_cast<std::int32_t>(s.height)};
    return r->l >= 0 && r->t >= 0 && r->l < r->r && r->t < r->b && static_cast<U>(r->r) <= s.width &&
           static_cast<U>(r->b) <= s.height;
}

// Copies a w x h block between two surfaces of the same depth (for Blt, BltFast
// and Load); the fake's copy has no stretching, colour keying or effects.
void Copy(const Surface& to, int tx, int ty, const Surface& from, const Rect& r) {
    const U w = static_cast<U>(r.r - r.l) * from.bpp;
    for (int y = 0; y < r.b - r.t; ++y)
        std::memmove(Buffer(to.index, g_pass) + static_cast<U>(ty + y) * to.pitch + static_cast<U>(tx) * to.bpp,
                     Buffer(from.index, g_pass) + static_cast<U>(r.t + y) * from.pitch + static_cast<U>(r.l) * from.bpp,
                     w);
}

void FillDesc(const Surface& s, unsigned char* d, U surface) {
    Put(d, 4, 0x100F);            // DDSD_CAPS | HEIGHT | WIDTH | PITCH | PIXELFORMAT
    Put(d, 8, s.height);
    Put(d, 0xC, s.width);
    Put(d, 0x10, s.pitch);
    Put(d, 0x24, surface);
    std::memcpy(d + 0x48, s.format, 0x20);
    Put(d, 0x68, s.caps);
}

constexpr U kTexture2[4] = {0x93281502u, 0x11D08CF8u, 0xA000AB89u, 0x294105C9u};   // IID_IDirect3DTexture2
bool IsTexture2(const void* iid) {
    for (int i = 0; i < 4; ++i)
        if (Get(iid, 4 * i) != kTexture2[i]) return false;
    return true;
}

// --- IDirectDrawSurface4 --------------------------------------------------------------

long __stdcall SQueryInterface(Surface* s, const void* iid, void** out) {
    Record(kSurfaceCall + kQueryInterface, Id(s), Get(iid, 0), Get(iid, 4), Get(iid, 8), Get(iid, 12),
           static_cast<U>(reinterpret_cast<std::uintptr_t>(out)));
    U hr = kOk;
    if (!Planned(kSurfaceCall + kQueryInterface, &hr)) {
        if (IsTexture2(iid)) *out = &g_textures[s->index];
        else {
            *out = nullptr;
            hr = kNoInterface;
        }
    }
    After(kSurfaceCall + kQueryInterface);
    return static_cast<long>(hr);
}
U __stdcall SAddRef(Surface* s) {
    Record(kSurfaceCall + kAddRef, Id(s));
    After(kSurfaceCall + kAddRef);
    return 2;
}
U __stdcall SRelease(Surface* s) {
    Record(kSurfaceCall + kRelease, Id(s));
    After(kSurfaceCall + kRelease);
    return 0;
}
long __stdcall SBlt(Surface* s, const void* to_rect, void* from, const void* from_rect, U flags, void* fx) {
    U a[4], b[4];
    RectArgs(to_rect, a);
    RectArgs(from_rect, b);
    Record(kSurfaceCall + kBlt, Id(s), a[0], a[1], a[2], a[3], Id(from), b[0], b[1], b[2], b[3], flags,
           static_cast<U>(reinterpret_cast<std::uintptr_t>(fx)));
    U hr = kOk;
    if (!Planned(kSurfaceCall + kBlt, &hr)) {
        const Surface* src = AsSurface(from);
        Rect rt, rf;
        if (s->locked || (src && src->locked)) hr = kSurfaceBusy;
        else if (src && RectOn(*s, to_rect, &rt) && RectOn(*src, from_rect, &rf) && rt.r - rt.l == rf.r - rf.l &&
                 rt.b - rt.t == rf.b - rf.t && s->bpp == src->bpp)
            Copy(*s, rt.l, rt.t, *src, rf);
    }
    After(kSurfaceCall + kBlt);
    return static_cast<long>(hr);
}
long __stdcall SBltFast(Surface* s, U x, U y, void* from, const void* from_rect, U trans) {
    U b[4];
    RectArgs(from_rect, b);
    Record(kSurfaceCall + kBltFast, Id(s), x, y, Id(from), b[0], b[1], b[2], b[3], trans);
    U hr = kOk;
    if (!Planned(kSurfaceCall + kBltFast, &hr)) {
        const Surface* src = AsSurface(from);
        Rect rf;
        if (s->locked || (src && src->locked)) hr = kSurfaceBusy;
        else if (src && RectOn(*src, from_rect, &rf) && s->bpp == src->bpp && x + static_cast<U>(rf.r - rf.l) <= s->width &&
                 y + static_cast<U>(rf.b - rf.t) <= s->height)
            Copy(*s, static_cast<int>(x), static_cast<int>(y), *src, rf);
    }
    After(kSurfaceCall + kBltFast);
    return static_cast<long>(hr);
}
long __stdcall SFlip(Surface* s, void* target, U flags) {
    Record(kSurfaceCall + kFlip, Id(s), Id(target), flags);
    U hr = kOk;
    Planned(kSurfaceCall + kFlip, &hr);
    After(kSurfaceCall + kFlip);
    return static_cast<long>(hr);
}
long __stdcall SGetSurfaceDesc(Surface* s, unsigned char* d) {
    Record(kSurfaceCall + kGetSurfaceDesc, Id(s), d ? Get(d, 0) : kNullRect);
    U hr = kOk;
    if (!Planned(kSurfaceCall + kGetSurfaceDesc, &hr)) {
        if (!d || Get(d, 0) != kDescBytes) hr = kInvalidParams;
        else FillDesc(*s, d, 0);
    }
    After(kSurfaceCall + kGetSurfaceDesc);
    return static_cast<long>(hr);
}
long __stdcall SIsLost(Surface* s) {
    Record(kSurfaceCall + kIsLost, Id(s));
    U hr = kOk;
    Planned(kSurfaceCall + kIsLost, &hr);
    After(kSurfaceCall + kIsLost);
    return static_cast<long>(hr);
}
long __stdcall SLock(Surface* s, const void* rect, unsigned char* d, U flags, U event) {
    U a[4];
    RectArgs(rect, a);
    Record(kSurfaceCall + kLock, Id(s), a[0], a[1], a[2], a[3], flags, event, d ? Get(d, 0) : kNullRect);
    U hr = kOk;
    if (!Planned(kSurfaceCall + kLock, &hr)) {
        Rect r;
        if (!d || Get(d, 0) != kDescBytes) hr = kInvalidParams;
        else if (s->locked) hr = kSurfaceBusy;
        else if (!RectOn(*s, rect, &r)) hr = kInvalidParams;
        else {
            unsigned char* p = Buffer(s->index, g_pass) + static_cast<U>(r.t) * s->pitch + static_cast<U>(r.l) * s->bpp;
            FillDesc(*s, d, static_cast<U>(reinterpret_cast<std::uintptr_t>(p)));
            s->locked = true;
        }
    }
    After(kSurfaceCall + kLock);
    return static_cast<long>(hr);
}
long __stdcall SRestore(Surface* s) {
    Record(kSurfaceCall + kRestore, Id(s));
    U hr = kOk;
    Planned(kSurfaceCall + kRestore, &hr);
    After(kSurfaceCall + kRestore);
    return static_cast<long>(hr);
}
long __stdcall SSetColorKey(Surface* s, U flags, const void* key) {
    Record(kSurfaceCall + kSetColorKey, Id(s), flags, key ? Get(key, 0) : kNullRect, key ? Get(key, 4) : kNullRect);
    U hr = kOk;
    Planned(kSurfaceCall + kSetColorKey, &hr);
    After(kSurfaceCall + kSetColorKey);
    return static_cast<long>(hr);
}
long __stdcall SUnlock(Surface* s, const void* rect) {
    U a[4];
    RectArgs(rect, a);
    Record(kSurfaceCall + kUnlock, Id(s), a[0], a[1], a[2], a[3]);
    U hr = kOk;
    if (!Planned(kSurfaceCall + kUnlock, &hr)) {
        if (!s->locked) hr = kNotLocked;
        else s->locked = false;
    }
    After(kSurfaceCall + kUnlock);
    return static_cast<long>(hr);
}

// --- IDirect3DTexture2 ------------------------------------------------------------------

long __stdcall TQueryInterface(Texture* t, const void* iid, void** out) {
    Record(kTextureCall + kQueryInterface, Id(t), Get(iid, 0), Get(iid, 4), Get(iid, 8), Get(iid, 12),
           static_cast<U>(reinterpret_cast<std::uintptr_t>(out)));
    U hr = kOk;
    if (!Planned(kTextureCall + kQueryInterface, &hr)) {
        if (IsTexture2(iid)) *out = t;
        else {
            *out = nullptr;
            hr = kNoInterface;
        }
    }
    After(kTextureCall + kQueryInterface);
    return static_cast<long>(hr);
}
U __stdcall TAddRef(Texture* t) {
    Record(kTextureCall + kAddRef, Id(t));
    After(kTextureCall + kAddRef);
    return 2;
}
U __stdcall TRelease(Texture* t) {
    Record(kTextureCall + kRelease, Id(t));
    After(kTextureCall + kRelease);
    return 0;
}
long __stdcall TGetHandle(Texture* t, void* device, U* handle) {
    Record(kTextureCall + kGetHandle, Id(t), static_cast<U>(reinterpret_cast<std::uintptr_t>(device)),
           static_cast<U>(reinterpret_cast<std::uintptr_t>(handle)));
    U hr = kOk;
    if (!Planned(kTextureCall + kGetHandle, &hr)) *handle = 0x7E000000u + t->index;
    After(kTextureCall + kGetHandle);
    return static_cast<long>(hr);
}
long __stdcall TPaletteChanged(Texture* t, U start, U count) {
    Record(kTextureCall + kPaletteChanged, Id(t), start, count);
    U hr = kOk;
    Planned(kTextureCall + kPaletteChanged, &hr);
    After(kTextureCall + kPaletteChanged);
    return static_cast<long>(hr);
}
long __stdcall TLoad(Texture* t, Texture* from) {
    Record(kTextureCall + kLoad, Id(t), Id(from));
    U hr = kOk;
    if (!Planned(kTextureCall + kLoad, &hr)) {
        const Texture* src = AsTexture(from);
        if (!src) hr = kInvalidParams;
        else {
            const Surface& a = g_surfaces[t->index];
            const Surface& b = g_surfaces[src->index];
            if (a.width == b.width && a.height == b.height && a.bpp == b.bpp && !a.locked && !b.locked)
                Copy(a, 0, 0, b, {0, 0, static_cast<std::int32_t>(b.width), static_cast<std::int32_t>(b.height)});
            else hr = kInvalidParams;
        }
    }
    After(kTextureCall + kLoad);
    return static_cast<long>(hr);
}

// --- IDirectDraw4 ---------------------------------------------------------------------------

long __stdcall DCreateSurface(DirectDraw*, const unsigned char* d, void** out, void* outer) {
    U snap = kNullRect;
    if (d && g_log->n_descs < kMaxDescs) {
        snap = g_log->n_descs++;
        std::memcpy(g_log->descs[snap], d, kDescBytes);
    }
    Record(kDirectDrawCall + kCreateSurface, snap, static_cast<U>(reinterpret_cast<std::uintptr_t>(out)),
           static_cast<U>(reinterpret_cast<std::uintptr_t>(outer)));
    U hr = kOk;
    if (!Planned(kDirectDrawCall + kCreateSurface, &hr)) {
        if (!d || Get(d, 0) != kDescBytes) hr = kInvalidParams;
        else {
            if (g_count >= kMaxSurfaces) bof3::Fatal("ddraw_fuzz: more than %u surfaces in one pass", kMaxSurfaces);
            const U flags = Get(d, 4);
            const U width = flags & 4 ? Get(d, 0xC) : 0;
            const U height = flags & 2 ? Get(d, 8) : 0;
            U bits = flags & 0x1000 ? Get(d, 0x54) : 16;
            U bpp = (bits + 7) / 8;
            if (bpp < 1 || bpp > 4) bpp = 2;
            const unsigned k = g_count - g_made;
            const U pitch = k < g_n_pitches ? g_pitches[k] : 0;
            if (width > 1024 || height > 1024) hr = kInvalidParams;
            else {
                Surface& s = g_surfaces[g_count];
                Shape(s, g_count, width, height, bpp, pitch);
                if (flags & 0x1000) std::memcpy(s.format, d + 0x48, 0x20);
                s.caps = Get(d, 0x68);
                ++g_count;
                *out = &s;
            }
        }
    }
    After(kDirectDrawCall + kCreateSurface);
    return static_cast<long>(hr);
}

template <U Base, int S> long __stdcall Trap(void*) {
    bof3::Fatal("ddraw_fuzz: slot %d (vtable +0x%X) of a fake %s was called, and nothing records it", S, S * 4,
                Base == kSurfaceCall ? "surface" : Base == kTextureCall ? "texture" : "IDirectDraw4");
}
template <U Base, int... S> void FillTraps(void** vtbl, std::integer_sequence<int, S...>) {
    ((vtbl[S] = reinterpret_cast<void*>(&Trap<Base, S>)), ...);
}

void* F(auto fn) { return reinterpret_cast<void*>(fn); }

void Build() {
    static bool built = false;
    if (built) return;
    built = true;
    FillTraps<kSurfaceCall>(g_surface_vtbl, std::make_integer_sequence<int, kSlots>{});
    FillTraps<kTextureCall>(g_texture_vtbl, std::make_integer_sequence<int, kSlots>{});
    FillTraps<kDirectDrawCall>(g_dd_vtbl, std::make_integer_sequence<int, kSlots>{});
    g_surface_vtbl[kQueryInterface] = F(&SQueryInterface);
    g_surface_vtbl[kAddRef] = F(&SAddRef);
    g_surface_vtbl[kRelease] = F(&SRelease);
    g_surface_vtbl[kBlt] = F(&SBlt);
    g_surface_vtbl[kBltFast] = F(&SBltFast);
    g_surface_vtbl[kFlip] = F(&SFlip);
    g_surface_vtbl[kGetSurfaceDesc] = F(&SGetSurfaceDesc);
    g_surface_vtbl[kIsLost] = F(&SIsLost);
    g_surface_vtbl[kLock] = F(&SLock);
    g_surface_vtbl[kRestore] = F(&SRestore);
    g_surface_vtbl[kSetColorKey] = F(&SSetColorKey);
    g_surface_vtbl[kUnlock] = F(&SUnlock);
    g_texture_vtbl[kQueryInterface] = F(&TQueryInterface);
    g_texture_vtbl[kAddRef] = F(&TAddRef);
    g_texture_vtbl[kRelease] = F(&TRelease);
    g_texture_vtbl[kGetHandle] = F(&TGetHandle);
    g_texture_vtbl[kPaletteChanged] = F(&TPaletteChanged);
    g_texture_vtbl[kLoad] = F(&TLoad);
    g_dd_vtbl[kCreateSurface] = F(&DCreateSurface);
    for (int pass = 0; pass < 2; ++pass) {
        g_arena[pass] = static_cast<unsigned char*>(
            VirtualAlloc(nullptr, static_cast<SIZE_T>(kMaxSurfaces) * kSlotBytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        if (!g_arena[pass]) bof3::Fatal("ddraw_fuzz: VirtualAlloc of the pixel arena failed, error %lu", GetLastError());
    }
}

}  // namespace

void SetDisturb(Disturb d) { g_disturb = d; }

void Reset(U seed) {
    Build();
    g_seed = seed;
    g_made = g_count = 0;
    g_count_of[0] = g_count_of[1] = 0;
    g_n_pitches = 0;
    g_n_plans = 0;
    g_in_pass = false;
    g_pass = 0;
}

void* MakeSurface(U width, U height, U bytes_per_pixel, U pitch) {
    Build();
    if (g_in_pass) bof3::Fatal("ddraw_fuzz: MakeSurface inside a pass");
    if (g_made >= kMaxSurfaces) bof3::Fatal("ddraw_fuzz: more than %u surfaces made", kMaxSurfaces);
    g_made_list[g_made] = {width, height, bytes_per_pixel, pitch};
    Shape(g_surfaces[g_made], g_made, width, height, bytes_per_pixel, pitch);
    g_count = ++g_made;
    return &g_surfaces[g_made - 1];
}

void SetPitches(const U* pitches, unsigned n) {
    if (n > kMaxSurfaces) n = kMaxSurfaces;
    for (unsigned i = 0; i < n; ++i) g_pitches[i] = pitches[i];
    g_n_pitches = n;
}

void FailAt(U what, unsigned nth, U result) {
    if (g_n_plans >= 4) bof3::Fatal("ddraw_fuzz: more than four failure plans");
    g_plans[g_n_plans++] = {what, nth, result, 0};
}

void BeginPass(int pass) {
    Build();
    if (pass != 0 && pass != 1) bof3::Fatal("ddraw_fuzz: pass %d", pass);
    if (g_in_pass) g_count_of[g_pass] = g_count;   // how many surfaces the pass that ends made
    g_pass = pass;
    g_in_pass = true;
    for (unsigned i = 0; i < g_made; ++i) {
        const Made& m = g_made_list[i];
        Shape(g_surfaces[i], i, m.width, m.height, m.bpp, m.pitch);
    }
    g_count = g_made;
    for (unsigned i = 0; i < g_n_plans; ++i) g_plans[i].seen = 0;
}

bool SamePixels(char* why) {
    // Pass 1 is the current one; pass 0's count was kept when pass 1 began.
    if (g_pass != 1) bof3::Fatal("ddraw_fuzz: SamePixels outside pass 1");
    const unsigned n0 = g_count_of[0], n1 = g_count;
    if (n0 != n1) {
        std::snprintf(why, 200, "%u surfaces, the original %u", n1, n0);
        return false;
    }
    for (unsigned i = 0; i < n1; ++i) {
        const U span = Span(g_surfaces[i]);
        const unsigned char* a = Buffer(i, 1);
        const unsigned char* b = Buffer(i, 0);
        if (std::memcmp(a, b, span) != 0) {
            U at = 0;
            while (a[at] == b[at]) ++at;
            std::snprintf(why, 200, "surface %u (pitch %u) differs at byte 0x%X (row %u): 0x%02X, the original 0x%02X", i,
                          (unsigned)g_surfaces[i].pitch, (unsigned)at,
                          g_surfaces[i].pitch ? (unsigned)(at / g_surfaces[i].pitch) : 0u, a[at], b[at]);
            return false;
        }
    }
    return true;
}

void* FakeDirectDraw() {
    Build();
    return &g_dd;
}

U Id(const void* p) {
    if (const Surface* s = AsSurface(p)) return kSurfaceId + s->index;
    if (const Texture* t = AsTexture(p)) return kTextureId + t->index;
    return static_cast<U>(reinterpret_cast<std::uintptr_t>(p));
}

bool Locate(const void* p, U* surface, U* offset) {
    const auto* b = static_cast<const unsigned char*>(p);
    for (unsigned i = 0; i < g_count; ++i) {
        const unsigned char* base = Buffer(i, g_pass);
        if (b >= base && b < base + kSlotBytes) {
            *surface = i;
            *offset = static_cast<U>(b - base);
            return true;
        }
    }
    return false;
}

unsigned char* Pixels(unsigned index) { return Buffer(index, g_pass); }

unsigned SurfaceCount() { return g_count; }

GlobalSwap::GlobalSwap(U address, const void* value) : address_(address) {
    auto* slot = reinterpret_cast<U*>(static_cast<std::uintptr_t>(address));
    saved_ = *slot;
    *slot = static_cast<U>(reinterpret_cast<std::uintptr_t>(value));
}

GlobalSwap::~GlobalSwap() { *reinterpret_cast<U*>(static_cast<std::uintptr_t>(address_)) = saved_; }

}  // namespace ddraw_fuzz

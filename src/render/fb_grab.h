// The frame-buffer grab (DIV-0125, docs/depress-wobble.md section 6).
//
// On the PlayStation a DR_MOVE whose source lies in the display buffer copies
// the picture drawn so far into VRAM, and later primitives texture from it:
// Depress's swirl (MAGIC109), and three other users of Gpu_SetDrawMove. The
// PC port moves cells of its VRAM shadow instead, which never holds the
// rendered frame, so those primitives sample nothing. With the grab on, the
// walk's move case still moves the shadow (game logic that reads it is
// unchanged) and also records a grab: the render backend copies that part of
// its render target, at the window's resolution, into a texture of its own,
// and direct-colour page textures that cover the destination sample it.
//
// This file is the part with no GPU in it: the switch, the display-buffer
// test and the registry of VRAM rectangles that hold a grab. The registry is a
// plain class so the start-up check (BOF3X_SHADOW=fb_grab) can test it alone.
#pragma once

namespace render {
namespace fb_grab {

struct Rect {
    int x, y, w, h;
};

// BOF3X_FBGRAB: unset or 1 grabs (the default, the owner's call 2026-10-13);
// 0 is Capcom's port - the shadow move only. Read once.
bool Enabled();

// A move's source in game coordinates: true when it lies in a display buffer
// (rows 0..239 or 240..479, the whole rectangle in one of them; columns
// starting left of 320 and reaching past 0). `local` is the rectangle in the
// picture's coordinates: the same x, y less the buffer's 240 rows. Either
// buffer is the frame being drawn: the PC has one render target.
bool SourceInDisplay(int x, int y, int w, int h, Rect* local);

constexpr int kSlots = 32;

// The VRAM rectangles that hold a grab, each in a slot the backend keeps a
// texture for. A grab stays until something writes the shadow over any of
// its rectangle (Gfx_InvalidateTextures sees every such write), as VRAM
// keeps a moved picture on the PlayStation until it is overwritten.
class Registry {
public:
    void Clear();
    // A grab into `dst`: the slot it lives in (a live grab of the same
    // rectangle keeps its slot), or -1 when every slot is live.
    int Record(const Rect& dst);
    // A write of the shadow: every grab whose rectangle it meets is dropped.
    void Written(const Rect& r);
    // The most recent live grab whose rectangle meets `area`: its slot, and
    // its rectangle in *dst. -1 when none.
    int Find(const Rect& area, Rect* dst) const;
    int Live() const;

private:
    struct Entry {
        bool live;
        Rect dst;
        unsigned order;
    };
    Entry entries_[kSlots] = {};
    unsigned next_order_ = 1;
};

bool Meets(const Rect& a, const Rect& b);

// BOF3X_SHADOW=fb_grab: property checks of the above. Fatal on a failure.
void SelfTest();

}  // namespace fb_grab
}  // namespace render

// The frame-buffer grab's GPU-free half (fb_grab.h; DIV-0125).
#include "render/fb_grab.h"

#include <windows.h>

#include <cstdint>

#include "hook/log.h"

namespace render {
namespace fb_grab {

bool Enabled() {
    static int state = -1;
    if (state < 0) {
        char text[8];
        const DWORD n = GetEnvironmentVariableA("BOF3X_FBGRAB", text, sizeof text);
        if (n == 0) state = 1;
        else if (n == 1 && text[0] == '1') state = 1;
        else if (n == 1 && text[0] == '0') state = 0;
        else bof3::Fatal("BOF3X_FBGRAB=%s: 0 or 1", n < sizeof text ? text : "...");
        bof3::Log("fb_grab     the frame-buffer grab (DIV-0125): %s", state ? "on" : "off (BOF3X_FBGRAB=0, Capcom's port)");
    }
    return state != 0;
}

bool SourceInDisplay(int x, int y, int w, int h, Rect* local) {
    if (w <= 0 || h <= 0) return false;
    if (y < 0 || y + h > 480) return false;
    const int buffer = y / 240;
    if ((y + h - 1) / 240 != buffer) return false;
    if (x >= 320 || x + w <= 0) return false;
    *local = {x, y - buffer * 240, w, h};
    return true;
}

bool Meets(const Rect& a, const Rect& b) {
    return a.w > 0 && a.h > 0 && b.w > 0 && b.h > 0 && a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h &&
           b.y < a.y + a.h;
}

void Registry::Clear() {
    for (Entry& e : entries_) e = {};
    next_order_ = 1;
}

int Registry::Record(const Rect& dst) {
    int slot = -1;
    for (int i = 0; i < kSlots; ++i) {
        const Entry& e = entries_[i];
        if (e.live && e.dst.x == dst.x && e.dst.y == dst.y && e.dst.w == dst.w && e.dst.h == dst.h) {
            slot = i;
            break;
        }
    }
    if (slot < 0)
        for (int i = 0; i < kSlots; ++i)
            if (!entries_[i].live) {
                slot = i;
                break;
            }
    if (slot < 0) return -1;
    entries_[slot] = {true, dst, next_order_++};
    return slot;
}

void Registry::Written(const Rect& r) {
    for (Entry& e : entries_)
        if (e.live && Meets(e.dst, r)) e.live = false;
}

int Registry::Find(const Rect& area, Rect* dst) const {
    int best = -1;
    for (int i = 0; i < kSlots; ++i) {
        const Entry& e = entries_[i];
        if (!e.live || !Meets(e.dst, area)) continue;
        if (best < 0 || e.order > entries_[best].order) best = i;
    }
    if (best >= 0) *dst = entries_[best].dst;
    return best;
}

int Registry::Live() const {
    int n = 0;
    for (const Entry& e : entries_) n += e.live ? 1 : 0;
    return n;
}

// --- BOF3X_SHADOW=fb_grab --------------------------------------------------------
// The display test against a cell-by-cell model; the registry against a list
// that remembers every grab in order; fixed cases from the four users of
// Gpu_SetDrawMove that move from the frame and the ones that do not.

namespace {

std::uint32_t g_seed = 0x2545F491u;
std::uint32_t Next() {
    g_seed ^= g_seed << 13;
    g_seed ^= g_seed >> 17;
    g_seed ^= g_seed << 5;
    return g_seed;
}
int Range(int lo, int hi) { return lo + static_cast<int>(Next() % static_cast<std::uint32_t>(hi - lo + 1)); }

// The model: every cell of the rectangle in one buffer's rows, and at least
// one cell in the 320 columns.
bool ModelInDisplay(int x, int y, int w, int h) {
    if (w <= 0 || h <= 0) return false;
    int buffer = -1;
    for (int row = y; row < y + h; ++row) {
        if (row < 0 || row >= 480) return false;
        if (buffer < 0) buffer = row / 240;
        else if (row / 240 != buffer) return false;
    }
    for (int col = x; col < x + w; ++col)
        if (col >= 0 && col < 320) return true;
    return false;
}

bool ModelMeets(const Rect& a, const Rect& b) {
    for (int yy = a.y; yy < a.y + a.h; ++yy)
        for (int xx = a.x; xx < a.x + a.w; ++xx)
            if (xx >= b.x && xx < b.x + b.w && yy >= b.y && yy < b.y + b.h) return true;
    return false;
}

unsigned g_failures;
void Expect(bool ok, const char* what, int a = 0, int b = 0, int c = 0, int d = 0) {
    if (ok) return;
    if (g_failures++ < 16) bof3::Log("shadow      fb_grab FAILED: %s (%d, %d, %d, %d)", what, a, b, c, d);
}

}  // namespace

void SelfTest() {
    g_failures = 0;
    // 1. The display test.
    unsigned in_display = 0;
    for (int round = 0; round < 20000; ++round) {
        const int x = Range(-200, 1100), y = Range(-50, 530), w = Range(-2, 330), h = Range(-2, 250);
        Rect local = {};
        const bool ours = SourceInDisplay(x, y, w, h, &local);
        Expect(ours == ModelInDisplay(x, y, w, h), "display test against the model", x, y, w, h);
        if (ours) {
            ++in_display;
            Expect(local.x == x && local.w == w && local.h == h && local.y == y % 240 && local.y + h <= 240,
                   "the local rectangle", x, y, w, h);
        }
    }
    // Fixed cases: Depress's box (both buffers), the fishing strip, the whole
    // frame of Scena17, an effect column move from x 0x170 (not the frame),
    // a rectangle across the buffers' seam.
    Rect l = {};
    Expect(SourceInDisplay(8, 8, 168, 168, &l) && l.y == 8, "Depress, buffer 0");
    Expect(SourceInDisplay(8, 248, 168, 168, &l) && l.y == 8, "Depress, buffer 1");
    Expect(SourceInDisplay(0, 240 + 0x50, 0x40, 0x78, &l) && l.y == 0x50, "the fishing strip");
    Expect(SourceInDisplay(0, 240, 320, 240, &l) && l.y == 0 && l.w == 320, "the whole frame");
    Expect(!SourceInDisplay(0x170, 0x1A0, 0x10, 0x10, &l), "a VRAM move right of the frame");
    Expect(!SourceInDisplay(0, 200, 16, 80, &l), "across the seam");
    Expect(!SourceInDisplay(10, 10, 0, 10, &l), "an empty rectangle");

    // 2. The registry against a model list of every grab and write in order.
    unsigned finds = 0, hits = 0, full = 0;
    for (int run = 0; run < 200; ++run) {
        Registry reg;
        struct M {
            Rect r;
            bool live;
        } model[256];
        int n = 0;
        for (int op = 0; op < 200; ++op) {
            const Rect r = {Range(700, 1023), Range(0, 511), Range(1, 96), Range(1, 96)};
            const int kind = Range(0, 9);
            if (kind < 4) {
                int live = 0;
                bool same = false;
                for (int i = 0; i < n; ++i)
                    if (model[i].live) {
                        ++live;
                        same |= model[i].r.x == r.x && model[i].r.y == r.y && model[i].r.w == r.w && model[i].r.h == r.h;
                    }
                const int slot = reg.Record(r);
                if (!same && live >= kSlots) {
                    Expect(slot < 0, "a full registry refuses");
                    ++full;
                    continue;
                }
                Expect(slot >= 0 && slot < kSlots, "a grab gets a slot", slot);
                for (int i = 0; i < n; ++i)   // the same rectangle replaces its older grab
                    if (model[i].live && model[i].r.x == r.x && model[i].r.y == r.y && model[i].r.w == r.w && model[i].r.h == r.h)
                        model[i].live = false;
                if (n < 256) model[n++] = {r, true};
            } else if (kind < 6) {
                reg.Written(r);
                for (int i = 0; i < n; ++i)
                    if (model[i].live && ModelMeets(model[i].r, r)) model[i].live = false;
            } else {
                Rect got = {};
                const int slot = reg.Find(r, &got);
                int want = -1;
                for (int i = n - 1; i >= 0; --i)
                    if (model[i].live && ModelMeets(model[i].r, r)) {
                        want = i;
                        break;
                    }
                ++finds;
                if (want < 0) {
                    Expect(slot < 0, "find: none live meets", r.x, r.y, r.w, r.h);
                } else {
                    ++hits;
                    Expect(slot >= 0 && got.x == model[want].r.x && got.y == model[want].r.y && got.w == model[want].r.w &&
                               got.h == model[want].r.h,
                           "find: the latest live grab that meets", r.x, r.y, r.w, r.h);
                }
            }
            int live = 0;
            for (int i = 0; i < n; ++i) live += model[i].live ? 1 : 0;
            Expect(reg.Live() == live, "live count", reg.Live(), live);
        }
    }
    // Fixed: Depress's grab is dropped by its own next move's invalidation
    // and recorded again in the slot that freed.
    {
        Registry reg;
        const Rect dst = {0x340, 0x100, 168, 168};
        const int a = reg.Record(dst);
        reg.Written(dst);
        const int b = reg.Record(dst);
        Rect got = {};
        Expect(a == b && reg.Live() == 1, "a re-grab reuses its slot");
        Expect(reg.Find({0x340, 0x100, 256, 256}, &got) == b && got.w == 168, "the page texture finds it");
        Expect(reg.Find({0x300, 0x100, 64, 256}, &got) < 0, "the page to the left does not");
    }
    bof3::Log("shadow      fb_grab self-test: 20000 display rounds (%u in a buffer), 200 registry runs of 200 operations "
              "(%u finds, %u that found a grab, %u refused full), %u FAILURES",
              in_display, finds, hits, full, g_failures);
    if (g_failures) bof3::Fatal("fb_grab: %u self-test failures", g_failures);
}

}  // namespace fb_grab
}  // namespace render

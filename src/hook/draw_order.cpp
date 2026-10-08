// BOF3X_DRAWORDER - see draw_order.h and docs/sprite-draw-order.md section 18.
#include "hook/draw_order.h"

#include <windows.h>

#include <cstdio>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/draw_pool.h"
#include "hook/input_script.h"
#include "hook/log.h"

namespace draw_order {

bool g_on = false;
Context g_ctx = {};

namespace {

unsigned g_f0 = 0, g_f1 = 0;
float g_x0 = 0, g_y0 = 0, g_x1 = 0, g_y1 = 0;

struct Tag {
    std::uint32_t addr, stamp;
    Kind kind;
    std::uint8_t slot;
    std::uint16_t layer;
    std::uint32_t a, b, c, d, texture, caller;
};
constexpr unsigned kTags = 1u << 15;   // power of two
Tag g_tags[kTags];

unsigned Now() {
    const unsigned f = bof3::InputScript_Frame();
    return f ? f : static_cast<unsigned>(Frame_Counter);
}

unsigned Hash(std::uint32_t a) { return ((a >> 2) * 2654435761u) >> 17; }

Tag* Slot(std::uint32_t addr) {
    const std::uint32_t now = Frame_Counter;
    unsigned h = Hash(addr) & (kTags - 1);
    for (unsigned probe = 0; probe < 64; ++probe, h = (h + 1) & (kTags - 1)) {
        Tag& t = g_tags[h];
        // A tag from four or more logic frames back is free: the buffer it
        // was built for has been drawn since.
        if (t.addr == addr || t.addr == 0 || now - t.stamp >= 4) return &t;
    }
    return &g_tags[Hash(addr) & (kTags - 1)];   // a crowded run: overwrite the home slot
}
const Tag* Find(std::uint32_t addr) {
    unsigned h = Hash(addr) & (kTags - 1);
    for (unsigned probe = 0; probe < 64; ++probe, h = (h + 1) & (kTags - 1)) {
        const Tag& t = g_tags[h];
        if (t.addr == addr) return &t;
        if (t.addr == 0) return nullptr;
    }
    return nullptr;
}

void Put(std::uint32_t addr, Kind kind, unsigned slot, unsigned layer, unsigned a, unsigned b, unsigned c, unsigned d,
         std::uint32_t texture, std::uint32_t caller) {
    Tag* t = Slot(addr);
    *t = {addr, static_cast<std::uint32_t>(Frame_Counter), kind, static_cast<std::uint8_t>(slot),
          static_cast<std::uint16_t>(layer), a, b, c, d, texture, caller};
}

template <class T> T Get(std::uint32_t addr) {
    T v;
    std::memcpy(&v, reinterpret_cast<const void*>(static_cast<std::uintptr_t>(addr)), sizeof v);
    return v;
}

struct Box { float x0, y0, x1, y1; };
void Grow(Box& b, float x, float y) {
    if (x < b.x0) b.x0 = x;
    if (x > b.x1) b.x1 = x;
    if (y < b.y0) b.y0 = y;
    if (y > b.y1) b.y1 = y;
}
Box Corners(std::uint32_t p, unsigned first, unsigned stride, unsigned n) {
    Box b = {1e30f, 1e30f, -1e30f, -1e30f};
    for (unsigned k = 0; k < n; ++k) Grow(b, Get<float>(p + first + k * stride), Get<float>(p + first + k * stride + 4));
    return b;
}

// The screen box of a primitive by its code, in the primitives' own
// coordinates; false for a code with no box here (draw modes, glyphs, the
// codes no handler of interest uses).
bool BoxOf(std::uint32_t p, unsigned code, Box& out) {
    switch (code & 0xFC) {
    case 0x2C: out = Corners(p, 8, 0x10, 4); return true;     // POLY_FT4
    case 0x38: out = Corners(p, 8, 0x10, 4); return true;     // POLY_G4
    case 0x3C: out = Corners(p, 8, 0x14, 4); return true;     // POLY_GT4
    case 0x40: out = Corners(p, 8, 0xC, 2); return true;      // LINE_F2
    case 0x4C: out = Corners(p, 8, 0xC, 4); return true;      // LINE_F4
    case 0x60: {                                              // TILE
        const float x = Get<float>(p + 8), y = Get<float>(p + 0xC);
        out = {x, y, x + Get<float>(p + 0x14), y + Get<float>(p + 0x18)};
        return true;
    }
    case 0x64: case 0x74: case 0x7C: {                        // SPRT, SPRT_8, SPRT_16
        const float x = Get<float>(p + 8), y = Get<float>(p + 0xC);
        const unsigned c = code & 0xFC;
        const float w = c == 0x74 ? 8.0f : c == 0x7C ? 16.0f : static_cast<float>(Get<std::uint16_t>(p + 0x18));
        const float h = c == 0x74 ? 8.0f : c == 0x7C ? 16.0f : static_cast<float>(Get<std::uint16_t>(p + 0x1A));
        out = {x, y, x + w, y + h};
        return true;
    }
    case 0x84: {                                              // the port's cell sprite
        // The extent as D3d_BuildCellTexture grows it: from the origin, every
        // cell's (x, y)..(x + w, y + h).
        const unsigned first = Get<std::uint16_t>(p + 0x18), count = Get<std::uint16_t>(p + 0x1A);
        int l = 0, r = 0, t = 0, b = 0;
        for (unsigned i = 0; i < count && i < 256; ++i) {
            const std::uint32_t rec = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(SpriteCell_Table)) + (first + i) * 8u;
            const int x = Get<std::int16_t>(rec), y = Get<std::int8_t>(rec + 2);
            const unsigned size = Get<std::uint8_t>(rec + 3);
            const int w = static_cast<int>(size & 0xF) * 8, h = static_cast<int>(size >> 4) * 8;
            if (x < l) l = x;
            if (y < t) t = y;
            if (x + w > r) r = x + w;
            if (y + h > b) b = y + h;
        }
        const float x = Get<float>(p + 8), y = Get<float>(p + 0xC);
        const float sx = Get<float>(p + 0x10), sy = Get<float>(p + 0x14);
        const bool flip = (Get<std::uint16_t>(p + 0x1E) & 0x400) != 0;
        out.x0 = flip ? x - r * sx : x + l * sx;
        out.x1 = flip ? x - l * sx : x + r * sx;
        out.y0 = y + t * sy;
        out.y1 = y + b * sy;
        return true;
    }
    default: return false;
    }
}

// A draw item of MapView_Build's (draw_pool's 0x90-byte items, two 0x48-byte
// halves): the view cell whose word names it, found by search.
void DescribeItem(std::uint32_t p, char* out, std::size_t n) {
    const std::uint32_t items = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(draw_pool::Items()));
    if (p < items || p >= items + draw_pool::Count() * 0x90u) {
        std::snprintf(out, n, "not a draw item");
        return;
    }
    const unsigned index = (p - items) / 0x90u, half = (p - items) % 0x90u / 0x48u;
    for (unsigned c = 0; c < 1568; ++c) {
        const std::uint32_t cell = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(MapView_CellItems)) + c * 4u;
        const unsigned word = Get<std::uint16_t>(cell + 2);
        if ((word & 0xFFFu) != index) continue;
        std::snprintf(out, n, "item %u half %u, view cell %u (map %u,%u) word %04X", index, half, c,
                      Get<std::uint8_t>(cell), Get<std::uint8_t>(cell + 1), word);
        return;
    }
    // Not a cell's own quad: one of the side triangles (+0x7E / +0x8E of some
    // cell's item), or an item released since.
    std::snprintf(out, n, "item %u half %u, no view cell names it (a side triangle?)", index, half);
}

void Describe(std::uint32_t p, char* out, std::size_t n) {
    const Tag* t = Find(p);
    if (!t) {
        std::snprintf(out, n, "untagged");
        return;
    }
    char item[96];
    switch (t->kind) {
    case kCell:
        std::snprintf(out, n, "cell layer %u row %u col %u handler %02X b1b0 %04X texture %08X (bit14 %u bit30 %u)",
                      t->layer, t->a, t->b, t->c, t->d, t->texture, (t->texture >> 14) & 1, (t->texture >> 30) & 1);
        return;
    case kSprite:
        std::snprintf(out, n, "sprite layer %u key %04X object %08X", t->layer, t->a, t->b);
        return;
    case kList0:
    case kList1:
    case kFrameNode:
        DescribeItem(p, item, sizeof item);
        std::snprintf(out, n, "%s layer %u - %s",
                      t->kind == kList0 ? "list0" : t->kind == kList1 ? "list1" : "framenode", t->layer, item);
        return;
    case kTable:
        DescribeItem(p, item, sizeof item);
        std::snprintf(out, n, "table layer %u key %04X - %s", t->layer, t->a, item);
        return;
    case kRecord:
        std::snprintf(out, n, "record layer %u object %08X", t->layer, t->a);
        return;
    case kCommit:
        std::snprintf(out, n, "commit from %08X", t->caller);
        return;
    default:
        std::snprintf(out, n, "tag %u", static_cast<unsigned>(t->kind));
        return;
    }
}

}  // namespace

void Start() {
    char text[128];
    const DWORD n = GetEnvironmentVariableA("BOF3X_DRAWORDER", text, sizeof text);
    if (n == 0 || n >= sizeof text) return;
    unsigned f0, f1;
    float x0, y0, x1, y1;
    if (std::sscanf(text, "%u-%u:%f,%f,%f,%f", &f0, &f1, &x0, &y0, &x1, &y1) != 6 || f1 < f0)
        bof3::Fatal("BOF3X_DRAWORDER=%s: want F0-F1:X0,Y0,X1,Y1", text);
    g_f0 = f0; g_f1 = f1;
    g_x0 = x0 < x1 ? x0 : x1; g_x1 = x0 < x1 ? x1 : x0;
    g_y0 = y0 < y1 ? y0 : y1; g_y1 = y0 < y1 ? y1 : y0;
    g_on = true;
    bof3::Log("draworder   on: frames %u..%u (%s), rectangle (%g, %g)-(%g, %g)", f0, f1,
              bof3::InputScript_Frame() || GetEnvironmentVariableA("BOF3X_INPUT", nullptr, 0) ? "recipe frames" : "Frame_Counter",
              g_x0, g_y0, g_x1, g_y1);
}

bool Tagging() {
    if (!g_on) return false;
    const unsigned f = Now();
    return f + 2 >= g_f0 && f <= g_f1;
}

void Set(Kind kind, unsigned layer, unsigned a, unsigned b, unsigned c, unsigned d) {
    g_ctx = {kind, layer, a, b, c, d, 0};
}
void Clear() { g_ctx = {}; }

void TagCommit(std::uint32_t prim, unsigned slot, std::uint32_t caller) {
    if (!Tagging()) return;
    if (g_ctx.kind == kCell || g_ctx.kind == kSprite)
        Put(prim, g_ctx.kind, slot, g_ctx.layer, g_ctx.a, g_ctx.b, g_ctx.c, g_ctx.d, g_ctx.texture, caller);
    else
        Put(prim, kCommit, slot, 0, 0, 0, 0, 0, 0, caller);
}

void TagList(std::uint32_t first, std::uint32_t last, Kind kind, unsigned layer, unsigned slot) {
    if (!Tagging()) return;
    std::uint32_t p = first;
    for (unsigned steps = 0; p != 0 && steps < 4096; ++steps) {
        Put(p, kind, slot, layer, 0, 0, 0, 0, 0, 0);
        if (p == last) return;
        p = Get<std::uint32_t>(p) & 0xFFFFFFu;
    }
}

void TagOne(std::uint32_t prim, Kind kind, unsigned layer, unsigned slot, unsigned a, unsigned b) {
    if (!Tagging()) return;
    Put(prim, kind, slot, layer, a, b, 0, 0, 0, 0);
}

void LogWalk(const unsigned long* ot) {
    if (!g_on) return;
    const unsigned f = Now();
    if (f < g_f0 || f > g_f1) return;
    {
        // The packet pool's fill at the walk: Gfx_PacketNext against this
        // buffer's 64 KB (Gfx_PacketPools + (buffer << 16), draw_emit.cpp's limit).
        const std::uint32_t base = 0x7E1C00u + (static_cast<std::uint32_t>(Gfx_BufferIndex) << 16);
        const std::uint32_t next = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(Gfx_PacketNext));
        bof3::Log("draworder   frame %u: packet pool at %u of 65536 bytes (buffer %u)", f, next - base,
                  static_cast<unsigned>(Gfx_BufferIndex));
    }
    // The walk starts at entry 7 of the environment's eight heads (Gfx_DrawOTag
    // is handed +0x8C, the heads were cleared in reverse) and passes each head
    // on its way down to entry 0: a node inside them says which slot the
    // primitives after it were committed to. Slot 7 is drawn first, 0 last.
    const std::uint32_t env = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(ot)) - 7u * 4u;
    bof3::Log("draworder   frame %u (Frame_Counter %u, buffer %u): the walk, primitives over (%g, %g)-(%g, %g)", f,
              static_cast<unsigned>(Frame_Counter), static_cast<unsigned>(Gfx_BufferIndex), g_x0, g_y0, g_x1, g_y1);
    int slot = 7;
    unsigned index = 0, shown = 0, unboxed = 0, outside = 0;
    std::uint32_t node = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(ot));
    for (unsigned steps = 0; steps < 200000; ++steps) {
        const std::uint32_t link = Get<std::uint32_t>(node);
        if (link == 0xFFFFFFFFu) break;
        std::uint32_t p = link;
        const bool skipped = (p & 0xFF000000u) != 0;
        if (skipped) p &= 0xFFFFFFu;
        // Either kind of head names the slot: the environment's eight, or the
        // eight list heads per buffer that Gfx_OtPointers start the frame at
        // (0x9037C0 + (buffer << 5) + 4 * slot, Gfx_BeginFrame) - whichever
        // the splice of Gfx_LinkOTags leaves on the path.
        constexpr std::uint32_t kListHeads = 0x9037C0;
        const bool env_head = p >= env && p < env + 0x20u;
        const bool list_head = p >= kListHeads && p < kListHeads + 0x40u;
        const bool head = env_head || list_head;
        if (env_head) slot = static_cast<int>((p - env) / 4u);
        if (list_head) slot = static_cast<int>((p - kListHeads) % 0x20u / 4u);
        if (!skipped && !head) {
            ++index;
            const unsigned code = Get<std::uint8_t>(p + 7);
            Box b;
            if (!BoxOf(p, code, b)) {
                ++unboxed;
            } else if (b.x1 < g_x0 || b.x0 > g_x1 || b.y1 < g_y0 || b.y0 > g_y1) {
                ++outside;
            } else if (shown < 600) {
                ++shown;
                char who[192];
                Describe(p, who, sizeof who);
                bof3::Log("draworder   #%u slot %d code %02X box (%.1f, %.1f)-(%.1f, %.1f) at %08X: %s", index, slot,
                          code, b.x0, b.y0, b.x1, b.y1, p, who);
                if (b.y1 - b.y0 > 100.0f && (code == 0x2C || code == 0x2D || code == 0x2E || code == 0x2F)) {
                    // A tall textured quad's 0x48 bytes, for comparing runs (2026-10-07).
                    char hex[0x48 * 3 + 1];
                    for (unsigned k = 0; k < 0x48; ++k) std::snprintf(hex + k * 3, 4, "%02X ", Get<std::uint8_t>(p + k));
                    bof3::Log("draworder     bytes %s", hex);
                }
            }
        }
        node = p;
    }
    bof3::Log("draworder   frame %u: %u drawn, %u over the rectangle (%u logged), %u outside it, %u with no box here", f,
              index, index - outside - unboxed, shown, outside, unboxed);
}

}  // namespace draw_order

// PROTOTYPE: the in-between frame's matcher (interp.h).
#include "render/interp.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

#include "os/os.h"

namespace render::interp {
namespace {

using Key = unsigned long long;

struct Fnv {
    Key h = 0xCBF29CE484222325ull;
    void Bytes(const void* p, std::size_t n) {
        const auto* b = static_cast<const unsigned char*>(p);
        for (std::size_t i = 0; i < n; ++i) h = (h ^ b[i]) * 0x100000001B3ull;
    }
    template <class T> void Put(const T& v) { Bytes(&v, sizeof v); }
};

struct Entry {
    Key exact, loose;
    U first, count;
    bool used;
};

// The kept frame.
std::vector<Vertex> g_vertices;
std::vector<Entry> g_entries;
std::vector<U> g_by_exact, g_by_loose;   // indices into g_entries, sorted by key
bool g_kept;

void StateInto(Fnv& f, const Command& c) {
    f.Put(c.state.topology);
    f.Put(c.count);
    f.Put(c.state.blend_enable);
    f.Put(c.state.src_blend);
    f.Put(c.state.dst_blend);
    f.Put(c.state.alpha_test);
    f.Put(c.state.color_key);
}

Entry EntryOf(const Command& c, const Vertex* v) {
    Entry e = {};
    e.first = c.first;
    e.count = c.count;
    const void* surface = c.state.texture ? static_cast<const void*>(c.state.texture->surface) : nullptr;
    Fnv x;
    StateInto(x, c);
    x.Put(surface);
    float x0 = v[0].x, x1 = v[0].x, y0 = v[0].y, y1 = v[0].y;
    for (U i = 0; i < c.count; ++i) {
        if (surface) {
            x.Put(v[i].u);
            x.Put(v[i].v);
        } else {
            x.Put(v[i].diffuse);
        }
        x0 = std::min(x0, v[i].x);
        x1 = std::max(x1, v[i].x);
        y0 = std::min(y0, v[i].y);
        y1 = std::max(y1, v[i].y);
    }
    e.exact = x.h;
    // Shape: the state and the box's size in steps of 4 units, so a cel a
    // pixel or two larger still pairs.
    Fnv l;
    StateInto(l, c);
    l.Put(surface != nullptr);
    l.Put(static_cast<int>((x1 - x0) / 4.0f));
    l.Put(static_cast<int>((y1 - y0) / 4.0f));
    e.loose = l.h;
    return e;
}

// The nearest unused kept entry with `key` in `order` (sorted by `member`),
// by the first vertex's distance; -1 when none.
int Nearest(const std::vector<U>& order, Key Entry::*member, Key key, const Vertex& at) {
    auto lo = std::lower_bound(order.begin(), order.end(), key,
                               [&](U i, Key k) { return g_entries[i].*member < k; });
    int best = -1;
    float best_d = 0;
    for (; lo != order.end() && g_entries[*lo].*member == key; ++lo) {
        const Entry& e = g_entries[*lo];
        if (e.used) continue;
        const Vertex& p = g_vertices[e.first];
        const float dx = p.x - at.x, dy = p.y - at.y, d = dx * dx + dy * dy;
        if (best < 0 || d < best_d) {
            best = static_cast<int>(*lo);
            best_d = d;
        }
    }
    return best;
}

}  // namespace

bool On() {
    static int on = -1;
    if (on < 0) {
        char v[8] = "";
        on = (bof3::os::GetEnv("BOF3X_INTERP", v, sizeof v) > 0 && v[0] != '0') ? 1 : 0;
    }
    return on != 0;
}

void Keep(const Frame& frame) {
    g_vertices.assign(frame.vertices, frame.vertices + frame.n_vertices);
    g_entries.clear();
    for (U i = 0; i < frame.n_commands; ++i) {
        const Command& c = frame.commands[i];
        if (c.kind != Cmd::kDraw || c.count == 0) continue;
        g_entries.push_back(EntryOf(c, frame.vertices + c.first));
    }
    const U n = static_cast<U>(g_entries.size());
    g_by_exact.resize(n);
    g_by_loose.resize(n);
    for (U i = 0; i < n; ++i) g_by_exact[i] = g_by_loose[i] = i;
    std::sort(g_by_exact.begin(), g_by_exact.end(), [](U a, U b) { return g_entries[a].exact < g_entries[b].exact; });
    std::sort(g_by_loose.begin(), g_by_loose.end(), [](U a, U b) { return g_entries[a].loose < g_entries[b].loose; });
    g_kept = true;
}

bool Build(const Frame& frame, Vertex* out, float max_move, Stats* stats) {
    Stats s = {};
    if (!g_kept) return false;
    std::memcpy(out, frame.vertices, frame.n_vertices * sizeof(Vertex));
    for (Entry& e : g_entries) e.used = false;
    const float cap = max_move * max_move;
    // Halfway, when every vertex is within the cap; says whether it was.
    const auto Pair = [&](const Command& c, Entry& e) {
        const Vertex* now = frame.vertices + c.first;
        const Vertex* was = g_vertices.data() + e.first;
        bool differs = false;
        // Every vertex within the cap, and all of them moving nearly alike:
        // a draw clipped at the picture's edge, or paired with another of
        // the same look, has vertices going different ways, and halfway
        // between is then a shape neither frame drew.
        const float dx0 = was[0].x - now[0].x, dy0 = was[0].y - now[0].y;
        for (U i = 0; i < c.count; ++i) {
            const float dx = was[i].x - now[i].x, dy = was[i].y - now[i].y;
            if (dx * dx + dy * dy > cap) return false;
            if ((dx - dx0) * (dx - dx0) + (dy - dy0) * (dy - dy0) > cap / 64) return false;
            if (dx != 0 || dy != 0) differs = true;
        }
        e.used = true;
        if (differs) {
            ++s.moved;
            Vertex* o = out + c.first;
            for (U i = 0; i < c.count; ++i) {
                o[i].x = (was[i].x + now[i].x) * 0.5f;
                o[i].y = (was[i].y + now[i].y) * 0.5f;
                o[i].z = (was[i].z + now[i].z) * 0.5f;
                o[i].rhw = (was[i].rhw + now[i].rhw) * 0.5f;
            }
        }
        return true;
    };
    // 0 unmatched, 1 a candidate too far, 2 matched
    std::vector<unsigned char> done(frame.n_commands, 0);
    std::vector<Key> loose(frame.n_commands, 0);
    for (U i = 0; i < frame.n_commands; ++i) {
        const Command& c = frame.commands[i];
        if (c.kind != Cmd::kDraw || c.count == 0) continue;
        ++s.draws;
        const Entry e = EntryOf(c, frame.vertices + c.first);
        loose[i] = e.loose;
        const int k = Nearest(g_by_exact, &Entry::exact, e.exact, frame.vertices[c.first]);
        if (k < 0) continue;
        if (Pair(c, g_entries[k])) {
            done[i] = 2;
            ++s.exact;
        } else {
            done[i] = 1;
        }
    }
    for (U i = 0; i < frame.n_commands; ++i) {
        const Command& c = frame.commands[i];
        if (c.kind != Cmd::kDraw || c.count == 0 || done[i] == 2) continue;
        const int k = Nearest(g_by_loose, &Entry::loose, loose[i], frame.vertices[c.first]);
        if (k >= 0 && Pair(c, g_entries[k])) {
            done[i] = 2;
            ++s.loose;
        } else if (k >= 0) {
            done[i] = 1;
        }
        if (done[i] == 1) ++s.far_;
        if (done[i] == 0) ++s.unmatched;
    }
    if (stats) *stats = s;
    return true;
}

}  // namespace render::interp

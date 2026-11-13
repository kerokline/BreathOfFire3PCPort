// PROTOTYPE: the in-between frame's matcher (interp.h).
#include "render/interp.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

#include "hook/log.h"
#include "os/os.h"

namespace render::interp {
extern bool g_debug;
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

bool g_debug;

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
    // What a matched draw's first vertex moves back by in the in-between
    // picture, for its unmatched neighbours to follow.
    std::vector<float> half_dx(frame.n_commands, 0.0f), half_dy(frame.n_commands, 0.0f);
    const auto Pair = [&](U index, Entry& e) {
        const Command& c = frame.commands[index];
        const Vertex* now = frame.vertices + c.first;
        const Vertex* was = g_vertices.data() + e.first;
        bool differs = false;
        // Every vertex within the cap, and all of them moving nearly alike:
        // a draw clipped at the picture's edge, or paired with another of
        // the same look, has vertices going different ways, and halfway
        // between is then a shape neither frame drew.
        const float dx0 = was[0].x - now[0].x, dy0 = was[0].y - now[0].y;
        // "Nearly alike" is to the draw's own size: a large polygon close to
        // the view turns and grows as it moves.
        float extent = 0;
        for (U i = 1; i < c.count; ++i)
            extent = std::max(extent, (now[i].x - now[0].x) * (now[i].x - now[0].x) + (now[i].y - now[0].y) * (now[i].y - now[0].y));
        const float alike = std::max(cap / 64, extent / 16);
        for (U i = 0; i < c.count; ++i) {
            const float dx = was[i].x - now[i].x, dy = was[i].y - now[i].y;
            if (dx * dx + dy * dy > cap) return false;
            if ((dx - dx0) * (dx - dx0) + (dy - dy0) * (dy - dy0) > alike) return false;
            if (dx != 0 || dy != 0) differs = true;
        }
        if (g_debug)
            bof3::Log("interp-dbg  cmd %u n %u tex %p blend %d at (%.1f, %.1f) size (%.1f, %.1f) uv (%.4f, %.4f) from (%.1f, %.1f) kept uv (%.4f, %.4f) d (%.1f, %.1f)",
                      index, c.count, c.state.texture ? static_cast<void*>(c.state.texture->surface) : nullptr, c.state.blend_enable,
                      now[0].x, now[0].y, now[c.count - 1].x - now[0].x, now[c.count - 1].y - now[0].y, now[0].u, now[0].v,
                      was[0].x, was[0].y, was[0].u, was[0].v, dx0, dy0);
        e.used = true;
        half_dx[index] = dx0 * 0.5f;
        half_dy[index] = dy0 * 0.5f;
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
    // Pairing is led by motion, not by the nearest draw of the same look: a
    // field is a grid of tiles that look alike, and its animated ones trade
    // looks with their neighbours from frame to frame (the sea), so the
    // nearest look-alike is often the tile next door, and one wrong pair
    // takes a neighbour's and the fault runs down the row (the owner,
    // 2026-11-13: holes in a chequer pattern over the sea).
    //
    // First each surface's commonest motion, voted by its draws' nearest
    // look-alikes (the sky moves apart from the ground). Then, in list order,
    // each draw takes the kept draw of its look or of its shape whose motion
    // is closest to that vote or to what the last paired draw of its surface
    // did - within kAgree. A
    // draw that agrees with nothing but has a look nearly its own (a sprite
    // walking against the scroll) takes the nearest of that look.
    constexpr float kAgree = 5.0f;       // target pixels
    constexpr float kShapeOnly = 1.0f;   // what a pair by shape alone gives away to one by look
    const auto Each = [&](const std::vector<U>& order, Key Entry::*member, Key key, const auto& fn) {
        auto lo = std::lower_bound(order.begin(), order.end(), key, [&](U i, Key k) { return g_entries[i].*member < k; });
        U count = 0;
        for (; lo != order.end() && g_entries[*lo].*member == key; ++lo, ++count)
            if (!g_entries[*lo].used) fn(static_cast<int>(*lo));
        return count;
    };
    struct Vote {
        const void* surface;
        long long cell;
        float dx, dy;
    };
    std::vector<Vote> votes;   // sorted by surface; after the count, one a surface
    {
        for (U i = 0; i < frame.n_commands; ++i) {
            const Command& c = frame.commands[i];
            if (c.kind != Cmd::kDraw || c.count == 0) continue;
            const Vertex& at = frame.vertices[c.first];
            const int k = Nearest(g_by_exact, &Entry::exact, EntryOf(c, frame.vertices + c.first).exact, at);
            if (k < 0) continue;
            const Vertex& p = g_vertices[g_entries[k].first];
            const long long cx = std::llround((p.x - at.x) / 2), cy = std::llround((p.y - at.y) / 2);
            votes.push_back({c.state.texture ? static_cast<const void*>(c.state.texture->surface) : nullptr, cx * 100000 + cy,
                             static_cast<float>(cx) * 2, static_cast<float>(cy) * 2});
        }
        std::sort(votes.begin(), votes.end(), [](const Vote& a, const Vote& b) {
            return a.surface != b.surface ? a.surface < b.surface : a.cell < b.cell;
        });
        std::vector<Vote> winners;
        std::size_t run = 0, best = 0;
        for (std::size_t i = 0; i < votes.size(); ++i) {
            const bool same_surface = i > 0 && votes[i].surface == votes[i - 1].surface;
            run = (same_surface && votes[i].cell == votes[i - 1].cell) ? run + 1 : 1;
            if (!same_surface) {
                winners.push_back(votes[i]);
                best = 0;
            }
            if (run > best) {
                best = run;
                winners.back() = votes[i];
            }
        }
        votes.swap(winners);
    }
    struct Last {
        const void* surface;
        float dx, dy;
    };
    std::vector<Last> last;   // a frame binds tens of surfaces: a linear search
    for (U i = 0; i < frame.n_commands; ++i) {
        const Command& c = frame.commands[i];
        if (c.kind != Cmd::kDraw || c.count == 0) continue;
        ++s.draws;
        const Vertex& at = frame.vertices[c.first];
        const Entry e = EntryOf(c, frame.vertices + c.first);
        const void* surface = c.state.texture ? static_cast<const void*>(c.state.texture->surface) : nullptr;
        // What to agree with: the surface's vote, and what its last paired draw did.
        float want[2][2] = {};
        int wants = 0;
        for (const Vote& v : votes)
            if (v.surface == surface) want[wants][0] = v.dx, want[wants][1] = v.dy, wants = 1;
        Last* mine = nullptr;
        for (Last& l : last)
            if (l.surface == surface) mine = &l;
        if (mine) want[wants][0] = mine->dx, want[wants][1] = mine->dy, ++wants;
        int best = -1, nearest_look = -1;
        float best_cost = 0, second_cost = 1e9f, nearest_d = 0;
        bool best_is_look = false;
        const auto Try = [&](int k, bool look) {
            const Vertex& p = g_vertices[g_entries[k].first];
            const float dx = p.x - at.x, dy = p.y - at.y;
            float cost = 1e9f;
            for (int w = 0; w < wants; ++w)
                cost = std::min(cost, std::sqrt((dx - want[w][0]) * (dx - want[w][0]) + (dy - want[w][1]) * (dy - want[w][1])));
            cost += look ? 0 : kShapeOnly;
            if (best < 0 || cost < best_cost) {
                if (best >= 0) second_cost = best_cost;
                best = k, best_cost = cost, best_is_look = look;
            } else if (cost < second_cost) {
                second_cost = cost;
            }
            const float d = dx * dx + dy * dy;
            if (look && (nearest_look < 0 || d < nearest_d)) nearest_look = k, nearest_d = d;
        };
        const U looks = Each(g_by_exact, &Entry::exact, e.exact, [&](int k) { Try(k, true); });
        Each(g_by_loose, &Entry::loose, e.loose, [&](int k) {
            if (g_entries[k].exact != e.exact) Try(k, false);
        });
        const bool any = best >= 0;
        // Agreeing, or standing well clear of the next best: a part of the
        // scene with a motion of its own on a surface it shares (the bridge
        // over the sea) agrees with neither the vote nor its neighbour, and
        // has one candidate near and the rest a piece away.
        const bool clear = best_cost <= kAgree * 4 && best_cost * 2.5f < second_cost;
        if (best >= 0 && (best_cost <= kAgree || clear) && Pair(i, g_entries[best])) {
            done[i] = 2;
            ++(best_is_look ? s.exact : s.loose);
            if (mine) mine->dx = half_dx[i] * 2, mine->dy = half_dy[i] * 2;
            else last.push_back({surface, half_dx[i] * 2, half_dy[i] * 2});
        } else if (nearest_look >= 0 && looks <= 2 && Pair(i, g_entries[nearest_look])) {
            done[i] = 2;
            ++s.exact;
        } else if (any) {
            done[i] = 1;
            ++s.far_;
            if (g_debug) {
                const Vertex& p = g_vertices[g_entries[best].first];
                bof3::Log("interp-dbg  FAIL cmd %u n %u tex %p at (%.1f, %.1f) ext (%.1f, %.1f) looks %u best cost %.1f look %d d (%.1f, %.1f) wants %d (%.1f, %.1f) (%.1f, %.1f)",
                          i, c.count, surface, at.x, at.y, frame.vertices[c.first + c.count - 1].x - at.x,
                          frame.vertices[c.first + c.count - 1].y - at.y, looks, best_cost, best_is_look, p.x - at.x, p.y - at.y,
                          wants, want[0][0], want[0][1], want[1][0], want[1][1]);
            }
        } else {
            ++s.unmatched;
        }
    }
    // A draw with no pair - one that came into the picture this frame, or
    // one cut by the picture's edge - goes with its neighbours: the nearest
    // matched draw of the same surface and state within kReach commands of it
    // in the list and within the cap of it in the picture. Standing still
    // beside draws that moved half a step, it opened a gap one picture in two
    // (the owner, 2026-11-13: a chequered look at the edges).
    constexpr int kReach = 48;
    const int n = static_cast<int>(frame.n_commands);
    for (int i = 0; i < n; ++i) {
        const Command& c = frame.commands[i];
        if (c.kind != Cmd::kDraw || c.count == 0 || done[i] == 2) continue;
        const Vertex& at = frame.vertices[c.first];
        int best = -1;
        float best_d = cap;
        for (int j = std::max(0, i - kReach); j < std::min(n, i + kReach + 1); ++j) {
            const Command& o = frame.commands[j];
            if (done[j] != 2 || o.kind != Cmd::kDraw) continue;
            const bool same_surface = (c.state.texture ? c.state.texture->surface : nullptr) ==
                                      (o.state.texture ? o.state.texture->surface : nullptr);
            if (!same_surface || o.state.blend_enable != c.state.blend_enable || o.state.topology != c.state.topology) continue;
            const Vertex& p = frame.vertices[o.first];
            const float dx = p.x - at.x, dy = p.y - at.y, d = dx * dx + dy * dy;
            if (d < best_d) {
                best = j;
                best_d = d;
            }
        }
        if (best < 0) continue;
        ++s.followed;
        if (half_dx[best] == 0 && half_dy[best] == 0) continue;
        Vertex* o = out + c.first;
        for (U k = 0; k < c.count; ++k) {
            o[k].x += half_dx[best];
            o[k].y += half_dy[best];
        }
    }
    if (stats) *stats = s;
    return true;
}

}  // namespace render::interp

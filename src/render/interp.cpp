// PROTOTYPE: the in-between frame's matcher (interp.h).
#include "render/interp.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

#include "hook/log.h"
#include "os/os.h"

namespace render::interp {
extern bool g_debug;
namespace {

using Key = std::uint64_t;

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

// A draw the matcher looks at: kDraw with vertices.
bool IsDraw(const Command& c) { return c.kind == Cmd::kDraw && c.count != 0; }

// The surface a draw samples; null for an untextured one.
const void* SurfaceOf(const Command& c) {
    return c.state.texture ? static_cast<const void*>(c.state.texture->surface) : nullptr;
}

Entry EntryOf(const Command& c, const Vertex* v) {
    Entry e = {};
    e.first = c.first;
    e.count = c.count;
    const void* surface = SurfaceOf(c);
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

// Each unused kept entry with `key` in `order` (sorted by `member`) to
// fn(index); returns how many entries have the key, used or not.
template <class Fn> U EachUnused(const std::vector<U>& order, Key Entry::*member, Key key, const Fn& fn) {
    auto lo = std::lower_bound(order.begin(), order.end(), key, [&](U i, Key k) { return g_entries[i].*member < k; });
    U count = 0;
    for (; lo != order.end() && g_entries[*lo].*member == key; ++lo, ++count)
        if (!g_entries[*lo].used) fn(static_cast<int>(*lo));
    return count;
}

// The nearest unused kept entry with `key` in `order` (sorted by `member`),
// by the first vertex's distance; -1 when none.
int Nearest(const std::vector<U>& order, Key Entry::*member, Key key, const Vertex& at) {
    int best = -1;
    float best_d = 0;
    EachUnused(order, member, key, [&](int k) {
        const Vertex& p = g_vertices[g_entries[k].first];
        const float dx = p.x - at.x, dy = p.y - at.y, d = dx * dx + dy * dy;
        if (best < 0 || d < best_d) {
            best = k;
            best_d = d;
        }
    });
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
        if (!IsDraw(c)) continue;
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
    // The frame's own entries, once: the vote, the order and the pairing read them.
    std::vector<Entry> cur(frame.n_commands);
    for (U i = 0; i < frame.n_commands; ++i)
        if (IsDraw(frame.commands[i])) cur[i] = EntryOf(frame.commands[i], frame.vertices + frame.commands[i].first);
    // For the debug lines: a surface by its ordinal among the frame's, in
    // the order met (an address is not the same from run to run).
    std::vector<const void*> surface_ids;
    const auto SurfaceId = [&](const void* surface) {
        for (std::size_t k = 0; k < surface_ids.size(); ++k)
            if (surface_ids[k] == surface) return static_cast<U>(k);
        surface_ids.push_back(surface);
        return static_cast<U>(surface_ids.size() - 1);
    };
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
            bof3::Log("interp-dbg  cmd %u n %u tex %u blend %d at (%.1f, %.1f) size (%.1f, %.1f) uv (%.4f, %.4f) from (%.1f, %.1f) kept uv (%.4f, %.4f) d (%.1f, %.1f)",
                      index, c.count, SurfaceId(SurfaceOf(c)), c.state.blend_enable,
                      now[0].x, now[0].y, now[c.count - 1].x - now[0].x, now[c.count - 1].y - now[0].y, now[0].u, now[0].v,
                      was[0].x, was[0].y, was[0].u, was[0].v, dx0, dy0);
        if (g_debug)
            for (U i = 0; i < c.count && i < 6; ++i)
                bof3::Log("interp-dbg  VERT cmd %u v%u now (%.1f, %.1f) uv (%.4f, %.4f) was (%.1f, %.1f) uv (%.4f, %.4f)", index, i,
                          now[i].x, now[i].y, now[i].u, now[i].v, was[i].x, was[i].y, was[i].u, was[i].v);
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
    struct Vote {
        const void* surface;
        std::int64_t cell;
        float dx, dy;
    };
    std::vector<Vote> votes;   // sorted by surface; after the count, one a surface
    {
        for (U i = 0; i < frame.n_commands; ++i) {
            const Command& c = frame.commands[i];
            if (!IsDraw(c)) continue;
            const Vertex& at = frame.vertices[c.first];
            const int k = Nearest(g_by_exact, &Entry::exact, cur[i].exact, at);
            if (k < 0) continue;
            const Vertex& p = g_vertices[g_entries[k].first];
            const std::int64_t cx = std::llround((p.x - at.x) / 2), cy = std::llround((p.y - at.y) / 2);
            votes.push_back({SurfaceOf(c), cx * 100000 + cy,
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
    // The draw list's own order: the longest run of looks the two lists
    // share, in order. A window's text is drawn in the same order every
    // frame, where the nearest letter of the same look is often another
    // word's (the sweep of 2026-11-13: a sliding window's digits traded
    // places). align[command] is the kept entry, -1 for none.
    std::vector<int> align(frame.n_commands, -1);
    {
        std::vector<U> cmd;
        std::vector<Key> a;
        for (U i = 0; i < frame.n_commands; ++i) {
            const Command& c = frame.commands[i];
            if (!IsDraw(c)) continue;
            cmd.push_back(i);
            a.push_back(cur[i].exact);
        }
        const std::size_t n = a.size(), m = g_entries.size();
        std::size_t pre = 0, suf = 0;
        while (pre < n && pre < m && a[pre] == g_entries[pre].exact) align[cmd[pre]] = static_cast<int>(pre), ++pre;
        while (suf < n - pre && suf < m - pre && a[n - 1 - suf] == g_entries[m - 1 - suf].exact)
            align[cmd[n - 1 - suf]] = static_cast<int>(m - 1 - suf), ++suf;
        const std::size_t N = n - pre - suf, M = m - pre - suf;
        if (N > 0 && M > 0 && N * M <= 8000000) {
            static std::vector<unsigned short> t;
            t.assign((N + 1) * (M + 1), 0);
            const auto T = [&](std::size_t i, std::size_t j) -> unsigned short& { return t[i * (M + 1) + j]; };
            // The table runs on the game's thread inside the frame's wait: past
            // kOrderBudgetMs it is dropped, and the middle of the list pairs
            // without an order, so the next frame is not made late.
            constexpr double kOrderBudgetMs = 8.0;
            const std::uint64_t start = bof3::os::Counter();
            const std::uint64_t budget =
                static_cast<std::uint64_t>(kOrderBudgetMs * static_cast<double>(bof3::os::CounterFrequency()) / 1000.0);
            bool timed_out = false;
            for (std::size_t i = N; i-- > 0 && !timed_out;) {
                for (std::size_t j = M; j-- > 0;)
                    T(i, j) = a[pre + i] == g_entries[pre + j].exact ? static_cast<unsigned short>(T(i + 1, j + 1) + 1)
                                                                     : std::max(T(i + 1, j), T(i, j + 1));
                if ((i & 15) == 0 && bof3::os::Counter() - start > budget) timed_out = true;
            }
            if (timed_out) {
                ++s.order_dropped;
                static bool said;
                if (!said) {
                    said = true;
                    bof3::Log("interp      an order table of %u x %u draws ran past %.0f ms and was dropped (logged once)",
                              static_cast<U>(N), static_cast<U>(M), kOrderBudgetMs);
                }
            }
            for (std::size_t i = 0, j = 0; !timed_out && i < N && j < M;) {
                if (a[pre + i] == g_entries[pre + j].exact) align[cmd[pre + i]] = static_cast<int>(pre + j), ++i, ++j;
                else if (T(i + 1, j) >= T(i, j + 1)) ++i;
                else ++j;
            }
        }
    }
    // The motion draw i's partner by order gives it; false when it has none.
    const auto AlignMotion = [&](U i, float* dx, float* dy) {
        const Command& c = frame.commands[i];
        if (!IsDraw(c) || align[i] < 0) return false;
        const Vertex& p = g_vertices[g_entries[align[i]].first];
        *dx = p.x - frame.vertices[c.first].x;
        *dy = p.y - frame.vertices[c.first].y;
        return true;
    };
    // A partner by order counts when a neighbour in the list, with its own
    // partner by order, moves the same way - of any surface: a window's
    // letters are a texture each (the fourth sweep, a battle window's text).
    const auto AlignAgrees = [&](U i) {
        float dx, dy, ox, oy;
        if (!AlignMotion(i, &dx, &dy) || g_entries[align[i]].used) return false;
        for (int dir = -1; dir <= 1; dir += 2) {
            int seen = 0;
            for (std::int64_t j = static_cast<std::int64_t>(i) + dir; j >= 0 && j < static_cast<std::int64_t>(frame.n_commands) && seen < 6; j += dir) {
                const Command& o = frame.commands[j];
                if (!IsDraw(o)) continue;
                ++seen;
                if (!AlignMotion(static_cast<U>(j), &ox, &oy)) continue;
                if ((ox - dx) * (ox - dx) + (oy - dy) * (oy - dy) <= 2.0f * 2.0f) return true;
                break;
            }
        }
        return false;
    };
    struct Last {
        const void* surface;
        float dx, dy;
    };
    std::vector<Last> last;   // a frame binds tens of surfaces: a linear search
    for (U i = 0; i < frame.n_commands; ++i) {
        const Command& c = frame.commands[i];
        if (!IsDraw(c)) continue;
        ++s.draws;
        const Vertex& at = frame.vertices[c.first];
        const Entry& e = cur[i];
        const void* surface = SurfaceOf(c);
        // What to agree with: the surface's vote, and what its last paired draw did.
        float want[3][2] = {};
        int wants = 0;
        for (const Vote& v : votes)
            if (v.surface == surface) want[wants][0] = v.dx, want[wants][1] = v.dy, wants = 1;
        Last* mine = nullptr;
        for (Last& l : last)
            if (l.surface == surface) mine = &l;
        if (mine) want[wants][0] = mine->dx, want[wants][1] = mine->dy, ++wants;
        // The draw before this one in the list, when it was paired.
        int prev_paired = -1;
        for (std::int64_t j = static_cast<std::int64_t>(i) - 1; j >= 0; --j) {
            const Command& o = frame.commands[j];
            if (!IsDraw(o)) continue;
            if (done[j] == 2) prev_paired = static_cast<int>(j);
            break;
        }
        // ... and what it did: a thing's pieces are drawn together, whatever
        // their textures (a window's letters), and a far plank of the bridge
        // within kAgree of the vote was still a row off (the owner,
        // 2026-11-13: thin seams along the bridge).
        if (prev_paired >= 0) want[wants][0] = half_dx[prev_paired] * 2, want[wants][1] = half_dy[prev_paired] * 2, ++wants;

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
        const U looks = EachUnused(g_by_exact, &Entry::exact, e.exact, [&](int k) { Try(k, true); });
        EachUnused(g_by_loose, &Entry::loose, e.loose, [&](int k) {
            if (g_entries[k].exact != e.exact) Try(k, false);
        });
        const bool any = best >= 0;
        // Agreeing, or standing well clear of the next best: a part of the
        // scene with a motion of its own on a surface it shares (the bridge
        // over the sea) agrees with neither the vote nor its neighbour, and
        // has one candidate near and the rest a piece away.
        const bool clear = best_cost <= kAgree * 4 && best_cost * 2.5f < second_cost;
        const bool agrees = best >= 0 && best_cost <= kAgree;
        if (g_debug) {
            float ax = 0, ay = 0;
            const bool am = AlignMotion(i, &ax, &ay);
            bof3::Log("interp-dbg  DECIDE cmd %u tex %u at (%.1f, %.1f) looks %u best %d cost %.1f second %.1f look %d wants %d (%.1f, %.1f) (%.1f, %.1f) align %d d (%.1f, %.1f) used %d agrees %d",
                      i, SurfaceId(surface), at.x, at.y, looks, best, best_cost, second_cost, best_is_look, wants, want[0][0], want[0][1],
                      want[1][0], want[1][1], align[i], ax, ay, am && g_entries[align[i]].used, am && AlignAgrees(i));
        }
        // The partner by order first, where its neighbours bear it out, over
        // a candidate that merely agrees with the vote: a list's letter took
        // the same letter of the line above (the fifth sweep, the shop's
        // list). A field of look-alike tiles that gained a row pairs by
        // order a whole row off, and every neighbour bears that out too -
        // the test below on a common look is what stops that. Two windows of one page
        // sliding opposite ways (a menu changing screens) have two motions
        // and one vote, and the vote paired half of one window's letters
        // with the other's (the sweep of 2026-11-13).
        bool by_order = AlignAgrees(i);
        if (by_order && looks > 2) {
            // A common look (a field's tiles) by order only as far as its
            // surface is known to move: by order a carpet's tiles paired a
            // tile off and left holes (the third sweep, bossAndFlash).
            float ax = 0, ay = 0, known = 0;
            AlignMotion(i, &ax, &ay);
            for (int w = 0; w < wants; ++w) known = std::max(known, std::sqrt(want[w][0] * want[w][0] + want[w][1] * want[w][1]));
            if (prev_paired >= 0)
                known = std::max(known, 2 * std::sqrt(half_dx[prev_paired] * half_dx[prev_paired] + half_dy[prev_paired] * half_dy[prev_paired]));
            if (std::sqrt(ax * ax + ay * ay) > known + kAgree) by_order = false;
        }
        if (by_order && agrees && align[i] != best) {
            // ... and gives way to a candidate that agrees better with what
            // is known than it does itself.
            float ax = 0, ay = 0, cost = 1e9f;
            AlignMotion(i, &ax, &ay);
            for (int w = 0; w < wants; ++w)
                cost = std::min(cost, std::sqrt((ax - want[w][0]) * (ax - want[w][0]) + (ay - want[w][1]) * (ay - want[w][1])));
            if (cost > best_cost + 1.0f) by_order = false;
        }
        // A paired draw's motion is what the next draw of its surface agrees with.
        const auto Remember = [&] {
            done[i] = 2;
            if (mine) mine->dx = half_dx[i] * 2, mine->dy = half_dy[i] * 2;
            else last.push_back({surface, half_dx[i] * 2, half_dy[i] * 2});
        };
        if (by_order && Pair(i, g_entries[align[i]])) {
            Remember();
            ++s.exact;
        } else if (best >= 0 && (agrees || clear) && Pair(i, g_entries[best])) {
            Remember();
            ++(best_is_look ? s.exact : s.loose);
        } else if (nearest_look >= 0 && looks <= 2 && nearest_d <= cap / 16 && Pair(i, g_entries[nearest_look])) {
            // Within a quarter of the cap: a line of text that changed has
            // letters whose only look-alike is a letter of the old line, and
            // they slid in from where those stood (the sixth sweep, the
            // shop's message bar).
            Remember();
            ++s.exact;
        } else if (any) {
            done[i] = 1;
            ++s.far_;
            if (g_debug) {
                const Vertex& p = g_vertices[g_entries[best].first];
                bof3::Log("interp-dbg  FAIL cmd %u n %u tex %u at (%.1f, %.1f) ext (%.1f, %.1f) looks %u best cost %.1f look %d d (%.1f, %.1f) wants %d (%.1f, %.1f) (%.1f, %.1f)",
                          i, c.count, SurfaceId(surface), at.x, at.y, frame.vertices[c.first + c.count - 1].x - at.x,
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
    struct Follow {
        U i;
        float dx, dy;
    };
    std::vector<Follow> follow;
    const int n = static_cast<int>(frame.n_commands);
    for (int i = 0; i < n; ++i) {
        const Command& c = frame.commands[i];
        if (!IsDraw(c) || done[i] == 2) continue;
        // Textured draws alone: a flat one with no pair is a fade or a tint
        // whose colour changed, and following a window's ground it uncovered
        // a strip of the picture (the sweep of 2026-11-13, the world map).
        if (!c.state.texture) continue;
        // Vertex by vertex: each takes the motion of the vertex it shares with
        // a paired draw of its surface near it in the list - a mesh's pieces
        // are drawn together, and share their corners. Moved as one block by
        // a neighbour's first vertex, a plank entering at the far end of the
        // swaying bridge (its rows of planks bob in turn) no longer met the
        // plank beside it (the owner, 2026-11-13: a seam at the upper right).
        bool any = false;
        float first_dx = 0, first_dy = 0;
        Vertex* o = out + c.first;
        for (U k = 0; k < c.count; ++k) {
            const Vertex& v = frame.vertices[c.first + k];
            // Only a corner it shares (within half a pixel): the rest of a
            // piece that came in at the picture's edge stays at the edge. A
            // new sliver of sky at the right edge, moved whole with the sky
            // beside it, left the last column bare one in-between in sixteen
            // (the owner, 2026-11-13: a black line at the upper right).
            float best_d = 0.26f, mx = 0, my = 0;
            bool found = false;
            for (int j = std::max(0, i - kReach); j < std::min(n, i + kReach + 1); ++j) {
                const Command& q = frame.commands[j];
                if (done[j] != 2 || q.kind != Cmd::kDraw) continue;
                const bool same_surface = SurfaceOf(c) == SurfaceOf(q);
                if (!same_surface || q.state.blend_enable != c.state.blend_enable || q.state.topology != c.state.topology) continue;
                for (U m = 0; m < q.count; ++m) {
                    const Vertex& p = frame.vertices[q.first + m];
                    const float dx = p.x - v.x, dy = p.y - v.y, d = dx * dx + dy * dy;
                    if (d < best_d) {
                        best_d = d;
                        mx = out[q.first + m].x - p.x;
                        my = out[q.first + m].y - p.y;
                        found = true;
                    }
                }
            }
            if (!found) continue;
            o[k].x += mx;
            o[k].y += my;
            if (!any) first_dx = mx, first_dy = my;
            any = true;
        }
        if (!any) continue;
        ++s.followed;
        follow.push_back({static_cast<U>(i), first_dx, first_dy});
    }
    for (const Follow& f : follow) half_dx[f.i] = f.dx, half_dy[f.i] = f.dy;
    // The last word: a surface whose draws touch in the picture and follow
    // each other in the list is a mesh or a line of text, and its pieces
    // move alike. Where several neighbours go different ways the pairing is
    // wrong somewhere, and halfway shows holes or scrambled letters (the
    // sweep of 2026-11-13: a spell's sphere, the desert floor, a dragon's
    // breath). Such a run of the list is shown where the frame has it, whole.
    struct Run {
        U n, breaks;
    };
    std::vector<Run> runs;
    std::vector<U> run_of(frame.n_commands, 0);
    {
        const void* surface = nullptr;
        bool blend = false, open = false;
        float px0 = 0, py0 = 0, px1 = 0, py1 = 0, pmx = 0, pmy = 0;
        for (U i = 0; i < frame.n_commands; ++i) {
            const Command& c = frame.commands[i];
            if (!IsDraw(c)) continue;
            const Vertex* v = frame.vertices + c.first;
            float x0 = v[0].x, x1 = v[0].x, y0 = v[0].y, y1 = v[0].y;
            for (U k = 1; k < c.count; ++k) {
                x0 = std::min(x0, v[k].x), x1 = std::max(x1, v[k].x);
                y0 = std::min(y0, v[k].y), y1 = std::max(y1, v[k].y);
            }
            const float mx = half_dx[i] * 2, my = half_dy[i] * 2;
            const bool same = open && SurfaceOf(c) == surface && (c.state.blend_enable != 0) == blend;
            if (!same) {
                runs.push_back({0, 0});
                surface = SurfaceOf(c), blend = c.state.blend_enable != 0, open = true;
            } else if (x0 <= px1 + 2 && px0 <= x1 + 2 && y0 <= py1 + 2 && py0 <= y1 + 2) {
                const float d = std::sqrt((mx - pmx) * (mx - pmx) + (my - pmy) * (my - pmy));
                const float big = std::sqrt(std::max(mx * mx + my * my, pmx * pmx + pmy * pmy));
                if (d > std::max(4.0f, big * 0.5f)) ++runs.back().breaks;
            }
            ++runs.back().n;
            run_of[i] = static_cast<U>(runs.size() - 1);
            px0 = x0, px1 = x1, py0 = y0, py1 = y1, pmx = mx, pmy = my;
        }
    }
    // A cut - another view, another place: the two frames are not the same
    // scene a step apart, and whatever pairs between them pairs by chance
    // (the owner, 2026-11-13: the ground shifting on the way into town).
    // When a quarter of the draws found no pair, or the runs' breaks are a
    // tenth of them, the frame is shown as it is.
    {
        U lost = 0, breaks = 0;
        for (U i = 0; i < frame.n_commands; ++i) {
            const Command& c = frame.commands[i];
            if (c.kind == Cmd::kDraw && c.count != 0 && done[i] != 2) ++lost;
        }
        for (const Run& r : runs) breaks += r.breaks;
        if (g_debug) bof3::Log("interp-dbg  CUT? draws %u lost %u breaks %u", s.draws, lost, breaks);
        if (s.draws >= 16 && (lost * 4 > s.draws || breaks * 10 > s.draws)) {
            std::memcpy(out, frame.vertices, frame.n_vertices * sizeof(Vertex));
            s.snapped = s.draws;
            if (stats) *stats = s;
            return true;
        }
    }
    for (U i = 0; i < frame.n_commands; ++i) {
        const Command& c = frame.commands[i];
        if (!IsDraw(c)) continue;
        const Run& r = runs[run_of[i]];
        if (g_debug) bof3::Log("interp-dbg  RUN cmd %u run %u n %u breaks %u half (%.1f, %.1f)", i, run_of[i], r.n, r.breaks, half_dx[i], half_dy[i]);
        // A long run takes a few breaks (a field's layer has the scene's
        // parts in it); a short one - a window's text - takes one.
        const bool broken = (r.breaks >= 3 && r.breaks * 10 > r.n) || (r.n <= 48 && r.breaks >= 2);
        if (!broken) continue;
        std::memcpy(out + c.first, frame.vertices + c.first, c.count * sizeof(Vertex));
        ++s.snapped;
    }
    if (stats) *stats = s;
    return true;
}

}  // namespace render::interp

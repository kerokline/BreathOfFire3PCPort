// BOF3X_TASKCENSUS (hook/task_census.h; docs/state-by-name-round.md 2).
#include "hook/task_census.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "bof3/symbols.gen.h"
#include "game/task_sched_callees.h"   // the records' layout; game/task_switch.h's slice and entry slot
#include "hook/log.h"
#include "hook/task_yield.h"
#include "os/os.h"

namespace task_census {

namespace {

using U = std::uint32_t;
using Chain = std::vector<std::uintptr_t>;   // innermost first: Task_Sleep's return address, then its callers'

constexpr std::size_t kMaxChain = 24;
constexpr U kEvery = 60;    // frames between two writes of the file

struct Site {
    int id = 0;
    U count = 0;
    U first_frame = 0;
    U min_depth = ~U{0}, max_depth = 0;   // bytes of the slice in use, from its top to the saved sp
    U fresh = 0;                          // frames the task stood created and not yet run (state 2)
    std::set<unsigned> modes;             // Game_Mode on those frames (task 0's word, the game's mode)
};

bool g_read = false, g_on = false;
std::string g_path;
bof3::os::ModuleImageInfo g_module;
// A row is a chain and the yield word the task carried with it (task_yield.h:
// the site id, 0 for none), so that a chain seen with two words is two rows.
using Key = std::pair<Chain, U>;
std::array<std::map<Key, Site>, task_sched::kTasks> g_sites;
std::map<std::array<int, task_sched::kTasks>, U> g_frames;   // each task's site id (-1: free) -> frames
U g_seen = 0;

// The frames as runs: consecutive frames (by Frame_Counter) on which the four
// tasks stood at the same rows and carried the same yield words. -1: free.
struct Run {
    U first = 0, frames = 0;
    std::array<int, task_sched::kTasks> rows{}, yields{};
};
std::vector<Run> g_runs;

// A native address (a task slice, our code, the image where it lies): not a VA.
const unsigned char* Native(std::uintptr_t a) { return reinterpret_cast<const unsigned char*>(a); }

bool Executable(std::uintptr_t a) {
    for (const bof3::os::ModulePart& p : g_module.parts)
        if (p.executable && a >= p.lo && a < p.hi) return true;
    return false;
}

// Whether the bytes before `a` are a call instruction that ends at `a`: E8
// rel32, or FF /2 in one of its encodings of 2, 3, 6 or 7 bytes (with an
// optional REX prefix the lengths 3, 4, 7, 8 cover the same). A word that is
// not a return address passes this by chance about once in the E8 case's 256.
bool AfterCall(std::uintptr_t a) {
    if (!Executable(a - 8)) return false;
    const unsigned char* p = Native(a);
    if (p[-5] == 0xE8) return true;
    for (int n : {2, 3, 4, 6, 7, 8})
        if (p[-n] == 0xFF && ((p[-n + 1] >> 3) & 7) == 2) return true;
    return false;
}

void Write() {
    std::FILE* f = std::fopen(g_path.c_str(), "wb");
    if (!f) {
        bof3::Log("taskcensus  cannot write %s", g_path.c_str());
        return;
    }
    std::fprintf(f, "# BOF3X_TASKCENSUS (src/hook/task_census.cpp): where each task stood at the frame boundary. "
                    "Addresses of our code, counts and stack depths only.\n");
    std::fprintf(f, "#module\t0x%llX\t0x%llX\tframes\t%u\tslice\t0x%X\n", static_cast<unsigned long long>(g_module.base),
                 static_cast<unsigned long long>(g_module.extent), g_seen, task_sched::kStackSize);
    for (const bof3::task_yield::Site& y : bof3::task_yield::kSites)
        std::fprintf(f, "#yieldsite\t%u\t%s\t%u\n", y.id, y.name, y.bytes);
    std::fprintf(f, "#rec\ttask\tsite\tframes\tfirst_frame\tmin_depth\tmax_depth\tfresh\tmodes\tchain\tyield\n");
    for (U k = 0; k < task_sched::kTasks; ++k)
        for (const auto& [key, s] : g_sites[k]) {
            const Chain& chain = key.first;
            std::fprintf(f, "site\t%u\t%d\t%u\t%u\t0x%X\t0x%X\t%u\t", k, s.id, s.count, s.first_frame, s.min_depth, s.max_depth,
                         s.fresh);
            bool first = true;
            for (unsigned m : s.modes) {
                std::fprintf(f, "%s%u", first ? "" : ",", m);
                first = false;
            }
            std::fputc('\t', f);
            first = true;
            for (std::uintptr_t a : chain) {
                std::fprintf(f, "%s0x%llX", first ? "" : " ", static_cast<unsigned long long>(a));
                first = false;
            }
            std::fprintf(f, "\t%u\n", key.second);
        }
    for (const auto& [ids, n] : g_frames) std::fprintf(f, "frame\t%d\t%d\t%d\t%d\t%u\n", ids[0], ids[1], ids[2], ids[3], n);
    for (const Run& r : g_runs)
        std::fprintf(f, "run\t%u\t%u\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n", r.first, r.frames, r.rows[0], r.rows[1], r.rows[2],
                     r.rows[3], r.yields[0], r.yields[1], r.yields[2], r.yields[3]);
    std::fclose(f);
}

}  // namespace

void Frame() {
    if (!g_read) {
        g_read = true;
        char buf[1024];
        const U n = bof3::os::GetEnv("BOF3X_TASKCENSUS", buf, sizeof buf);
        if (n == 0 || n >= sizeof buf) return;
        g_path.assign(buf, n);
        if (!bof3::os::ModuleImage(reinterpret_cast<const void*>(&Frame), &g_module))
            bof3::Fatal("BOF3X_TASKCENSUS: the module that holds our code does not read as one");
        g_on = true;
        bof3::Log("taskcensus  BOF3X_TASKCENSUS=%s: our code at 0x%llX, 0x%llX bytes", g_path.c_str(),
                  static_cast<unsigned long long>(g_module.base), static_cast<unsigned long long>(g_module.extent));
    }
    if (!g_on) return;

    using namespace task_sched;
    const unsigned char* records = Native(::bof3::Img(bof3::addr::Task_Records));
    const U top0 = static_cast<U>(Task_StackTop);
    const unsigned mode = static_cast<std::uint16_t>(Game_Mode);   // task 0's own word +0x18 (docs/task_sched.md 1)
    std::array<int, kTasks> ids{}, yields{};
    for (U k = 0; k < kTasks; ++k) {
        ids[k] = -1;
        yields[k] = -1;
        const unsigned char* r = records + k * kRecordSize;
        std::uint16_t state;
        std::memcpy(&state, r + kState, sizeof state);
        if (state == 0) continue;
        U sp;
        std::memcpy(&sp, r + kSavedEsp, sizeof sp);
        const U top = top0 - k * kStackSize;
        if (sp >= top || top - sp > kStackSize)
            bof3::Fatal("BOF3X_TASKCENSUS: task %u's saved sp 0x%08X is outside its slice under 0x%08X", k, sp, top);
        Chain chain;
        std::uintptr_t word;
        std::memcpy(&word, Native(sp + kEntrySlot), sizeof word);
        chain.push_back(word);   // what the scheduler's ret pops: Task_Sleep's return address, or a fresh task's entry
        for (U at = sp + kEntrySlot + sizeof word; at + sizeof word <= top && chain.size() < kMaxChain; at += sizeof word) {
            std::memcpy(&word, Native(at), sizeof word);
            if (Executable(word) && AfterCall(word)) chain.push_back(word);
        }
        // The yield word at this frame (task_yield.h): what a take would write
        // for this task, read beside the chain so that the two can be checked
        // against each other (tools/task_census.py).
        const U yield = bof3::task_yield::g_entries[k].site;
        yields[k] = static_cast<int>(yield);
        std::map<Key, Site>& sites = g_sites[k];
        const Key key{chain, yield};
        auto it = sites.find(key);
        if (it == sites.end()) {
            it = sites.emplace(key, Site{}).first;
            it->second.id = static_cast<int>(sites.size()) - 1;
            it->second.first_frame = Frame_Counter;
        }
        Site& s = it->second;
        s.count += 1;
        const U depth = top - sp;
        if (depth < s.min_depth) s.min_depth = depth;
        if (depth > s.max_depth) s.max_depth = depth;
        if (state != kSleeping) s.fresh += 1;
        s.modes.insert(mode);
        ids[k] = s.id;
    }
    g_frames[ids] += 1;
    const U now = Frame_Counter;
    if (!g_runs.empty() && g_runs.back().rows == ids && g_runs.back().yields == yields &&
        g_runs.back().first + g_runs.back().frames == now)
        g_runs.back().frames += 1;
    else
        g_runs.push_back(Run{now, 1, ids, yields});
    g_seen += 1;
    if (g_seen % kEvery == 0) Write();
}

}  // namespace task_census

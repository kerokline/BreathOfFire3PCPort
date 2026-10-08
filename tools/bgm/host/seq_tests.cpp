// Host-side tests for psx::MusicSynth (src/audio/seq.cpp) on a synthetic song
// and bank built here - no game data. What they pin: the VSync carry (a
// render in odd-sized pieces equals one in a single call), the loop period in
// VSyncs from libsnd's tick arithmetic, note2pitch's table and octave shift,
// the voice volume formula, and the aborts. docs/libsnd-reading.md is the
// reading each expectation comes from.
#include "audio/seq.h"
#include "audio/song.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace {

int g_failures = 0;
int g_checks = 0;

#define CHECK(cond)                                                                  \
    do {                                                                             \
        ++g_checks;                                                                  \
        if (!(cond)) {                                                               \
            ++g_failures;                                                            \
            std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);            \
        }                                                                            \
    } while (0)

#define CHECK_EQ(a, b)                                                               \
    do {                                                                             \
        ++g_checks;                                                                  \
        long long va_ = static_cast<long long>(a), vb_ = static_cast<long long>(b);  \
        if (va_ != vb_) {                                                            \
            ++g_failures;                                                            \
            std::printf("  FAIL %s:%d: %s == %lld, expected %s == %lld\n", __FILE__, \
                        __LINE__, #a, va_, #b, vb_);                                 \
        }                                                                            \
    } while (0)

// One program (0) with one tone over the whole keyboard, centre 60, and one
// looping sample: a silent first block, then two blocks of a triangle wave at
// shift 0, filter 0 (loop start on the first, end + repeat on the second).
psx::Bank MakeBank(std::uint16_t adsr2 = 0x1FC0) {
    psx::Bank b;
    b.name = "TEST";
    b.ps = 1;
    b.ts = 1;
    b.vs = 1;
    b.mvol = 127;
    b.pan = 64;
    psx::BankProgram& p = b.programs[0];
    p.tones = 1;
    p.mvol = 127;
    p.mpan = 64;
    p.block = 0;
    b.tones.assign(16, psx::BankTone{});
    psx::BankTone& t = b.tones[0];
    t = psx::BankTone{};
    t.vol = 127;
    t.pan = 64;
    t.center = 60;
    t.min = 0;
    t.max = 127;
    t.mode = 4;  // reverb on
    t.adsr1 = 0x00FF;
    t.adsr2 = adsr2;
    t.vag = 1;
    std::vector<std::uint8_t> body(48, 0);
    for (int k = 0; k < 2; ++k) {
        std::uint8_t* blk = &body[16 + 16 * static_cast<std::size_t>(k)];
        blk[0] = 0;  // filter 0, shift 0: nibble << 12
        blk[1] = k == 0 ? 0x04 : 0x03;
        for (int i = 0; i < 28; ++i) {
            const int x = k * 28 + i;  // 0..55, triangle -8..7
            const int v = (x < 28 ? x / 2 : (55 - x) / 2) - 7;
            const std::uint8_t nib = static_cast<std::uint8_t>(v & 0xF);
            blk[2 + i / 2] |= static_cast<std::uint8_t>(i & 1 ? nib << 4 : nib);
        }
    }
    b.samples.push_back(psx::BankSample{0, static_cast<std::uint32_t>(body.size())});
    b.body = body;
    return b;
}

void Ev(psx::Song& s, std::uint32_t tick, std::uint8_t st, std::uint8_t d1, std::uint8_t d2, std::uint32_t meta = 0) {
    s.events.push_back(psx::SongEvent{tick, st, d1, d2, meta});
}

// Resolution 48, 120 bpm: 48 * 120 * 10 / 3600 = 16 tenths of a tick a VSync,
// exactly. The loop body is 96 ticks = 960 tenths = 60 VSyncs.
psx::Song MakeSong(std::uint8_t extra_cc = 0) {
    psx::Song s;
    s.flags = 1;
    s.bank = "TEST";
    s.resolution = 48;
    s.tempo = 500000;
    Ev(s, 0, 0xC0, 0, 0);
    Ev(s, 0, 0xB0, 7, 100);
    Ev(s, 0, 0xB0, 99, 20);
    Ev(s, 0, 0xB0, 6, 127);
    if (extra_cc) Ev(s, 0, 0xB0, extra_cc, 1);
    Ev(s, 0, 0x90, 60, 100);
    Ev(s, 24, 0x90, 60, 0);
    Ev(s, 48, 0x90, 64, 100);
    Ev(s, 60, 0x90, 72, 90);
    Ev(s, 72, 0x90, 64, 0);
    Ev(s, 84, 0x90, 72, 0);
    Ev(s, 96, 0xB0, 99, 30);
    Ev(s, 96, 0xFF, 0x2F, 0);
    s.loop_start_tick = 0;
    s.loop_end_tick = 96;
    s.end_tick = 96;
    return s;
}

std::vector<std::int16_t> RenderSong(const std::vector<int>& pieces, int total) {
    auto m = std::make_unique<psx::MusicSynth>();
    m->LoadBank(MakeBank());
    m->Play(MakeSong(), 100, 8);
    std::vector<std::int16_t> out(2 * static_cast<std::size_t>(total));
    int done = 0;
    std::size_t k = 0;
    while (done < total) {
        int n = pieces[k++ % pieces.size()];
        if (n > total - done) n = total - done;
        m->Render(out.data() + 2 * done, n);
        done += n;
    }
    return out;
}

void TestPieces() {
    const int total = 44100 * 4;
    const auto whole = RenderSong({total}, total);
    const auto odd = RenderSong({1, 7, 735, 736, 737, 738, 18432, 3}, total);
    bool same = whole == odd;
    CHECK(same);
    long long energy = 0;
    for (std::int16_t v : whole) energy += static_cast<long long>(v) * v;
    CHECK(energy > 0);
}

struct Kons {
    std::vector<std::uint64_t> ticks;
    std::vector<std::uint16_t> pitch;
};

void Record(const psx::MusicSynth::TickTrace& t, void* user) {
    Kons* k = static_cast<Kons*>(user);
    for (int v = 0; v < 24; ++v)
        if (t.key_on & (1u << v)) {
            k->ticks.push_back(t.tick);
            k->pitch.push_back(t.pitch[v]);
        }
}

void TestLoopAndPitch() {
    auto m = std::make_unique<psx::MusicSynth>();
    m->LoadBank(MakeBank());
    m->Play(MakeSong(), 100, 8);
    Kons k;
    m->SetTickTrace(Record, &k);
    std::vector<std::int16_t> out(2 * 44100 * 3);
    m->Render(out.data(), 44100 * 3);  // VSyncs 1..180 (179.48 of them)
    // VSync m covers tenths (16(m-1), 16m]; an event exactly on 16m, the first
    // of its call, fires in VSync m (libsnd-reading.md 1.5). The loop end,
    // tick 96 = 960 tenths, fires in VSync 60, then every 60 VSyncs.
    CHECK_EQ(m->LoopJumps(), 3);
    CHECK_EQ(m->LastLoopJumpTick(), 180u);
    // Key ons reach the SPU at the flush of the VSync after their events:
    // note 60 at VSync 1 -> flush 2; note 64 (tick 48 = 480 = 16 x 30) in
    // VSync 30 -> 31; note 72 (tick 60, 37.5) in VSync 38 -> 39; the loop's
    // note 60 in VSync 60 -> 61.
    CHECK(k.ticks.size() >= 4);
    if (k.ticks.size() >= 4) {
        CHECK_EQ(k.ticks[0], 2u);
        CHECK_EQ(k.ticks[1], 31u);
        CHECK_EQ(k.ticks[2], 39u);
        CHECK_EQ(k.ticks[3], 61u);
        // note2pitch: x = note + 60 - centre; T[i] = floor(4096 * 2^(i/192)).
        CHECK_EQ(k.pitch[0], 0x1000);  // note 60: T[0], octave 5
        CHECK_EQ(k.pitch[1], 5160);    // note 64: T[64]
        CHECK_EQ(k.pitch[2], 0x2000);  // note 72: T[0] << 1
    }
}

bool Aborts(void (*fn)()) {
    std::fflush(stdout);
    pid_t pid = fork();
    if (pid == 0) {
        psx::SetMusicAbortHook([](const char*) {});
        psx::SetSpuAbortHook([](const char*) {});
        fn();
        _exit(0);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    return WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT;
}

void TestAborts() {
    // Controller 0 (bank select) is not implemented.
    CHECK(Aborts([] {
        auto m = std::make_unique<psx::MusicSynth>();
        m->LoadBank(MakeBank());
        m->Play(MakeSong(0), 100, 8);
        psx::Song s = MakeSong();
        s.events.insert(s.events.begin() + 1, psx::SongEvent{0, 0xB0, 0, 1, 0});
        m->Play(s, 100, 8);
        std::int16_t out[2 * 1000];
        m->Render(out, 1000);
    }));
    // Controller 98 (NRPN) is not implemented.
    CHECK(Aborts([] {
        auto m = std::make_unique<psx::MusicSynth>();
        m->LoadBank(MakeBank());
        m->Play(MakeSong(98), 100, 8);
        std::int16_t out[2 * 1000];
        m->Render(out, 1000);
    }));
    // A crescendo over 0 frames divides by zero on the PSX.
    CHECK(Aborts([] {
        auto m = std::make_unique<psx::MusicSynth>();
        m->LoadBank(MakeBank());
        m->Play(MakeSong(), 100, 0);
    }));
    // A song for another bank.
    CHECK(Aborts([] {
        auto m = std::make_unique<psx::MusicSynth>();
        psx::Bank b = MakeBank();
        b.name = "OTHER";
        m->LoadBank(b);
        m->Play(MakeSong(), 100, 8);
    }));
    // A controller libsnd ignores (here 1, modulation) is no abort.
    CHECK(!Aborts([] {
        auto m = std::make_unique<psx::MusicSynth>();
        m->LoadBank(MakeBank());
        m->Play(MakeSong(1), 100, 8);
        std::int16_t out[2 * 4000];
        m->Render(out, 4000);
    }));
}

// The loaders reject what is not the format.
void TestLoaders() {
    CHECK(Aborts([] {
        const std::uint8_t junk[64] = {'B', 'F', '3', 'S', 2};
        psx::Song s;
        psx::LoadSong(junk, sizeof junk, &s);
    }));
    CHECK(Aborts([] {
        const std::uint8_t junk[64] = {'B', 'F', '3', 'B', 1};
        psx::Bank b;
        psx::LoadBank(junk, sizeof junk, &b);
    }));
}

} // namespace

int main() {
    struct {
        const char* name;
        void (*fn)();
    } tests[] = {
        {"render in pieces", TestPieces},
        {"loop period and pitch", TestLoopAndPitch},
        {"aborts", TestAborts},
        {"loaders", TestLoaders},
    };
    for (auto& t : tests) {
        const int before = g_failures;
        std::printf("%s\n", t.name);
        t.fn();
        std::printf(g_failures == before ? "  ok\n" : "  FAILED\n");
    }
    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}

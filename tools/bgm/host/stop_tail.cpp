// stop_tail: what the synth still holds after a song is stopped - the sound a
// song started next would begin over (DIV-0250, known-defects D441).
//
//   stop_tail CACHE [--run-out]            the table, every song of the cache
//   stop_tail CACHE A T B OUT [--run-out]  song A stopped at T seconds, then
//                                          song B: OUT_frozen.wav (B at once)
//                                          and OUT_ran.wav (the SPU rendered
//                                          for 2 s in between)
//
// The table: each song played as the game plays it (Play 1 over 1, volume
// 127) and stopped at 8, 16, 24, 32 and 40 s on a copy of the synth; the next
// 3 s rendered with nothing playing, once on the song's own bank (keep) and
// once after LoadBank of another song's (swap). Per case: the song's level
// over its last second, the leftover's peak, its level over the first 100 ms,
// and how long it stays above -50 dBFS. --run-out calls StopAndRunOut (10 s
// limit) where the game's Music_Start does, in place of the plain stop.
// CACHE is the cache root (base/bgm). The WAVs are derived from game data:
// scratch only, never the repo.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>
#include "audio/seq.h"
#include "audio/song.h"
using namespace psx;
static std::vector<std::uint8_t> ReadFile(const std::string& p, bool must = true) {
    std::vector<std::uint8_t> d; FILE* f = fopen(p.c_str(), "rb");
    if (!f) { if (must) { fprintf(stderr, "no %s\n", p.c_str()); exit(2);} return d; }
    std::uint8_t b[65536]; size_t n; while ((n = fread(b, 1, sizeof b, f)) > 0) d.insert(d.end(), b, b + n); fclose(f); return d;
}
static bool LoadS(const std::string& c, int n, Song* s) { char nm[16]; snprintf(nm, sizeof nm, "%03d.DAT", n);
    auto d = ReadFile(c + "/base/bgm/" + nm, false); if (d.empty()) return false; LoadSong(d.data(), d.size(), s); return true; }
static void LoadB(const std::string& c, const std::string& name, Bank* b) { auto d = ReadFile(c + "/base/bgm/bank/" + name + ".DAT"); LoadBank(d.data(), d.size(), b); }
static void Rend(MusicSynth& s, std::vector<std::int16_t>& out, double sec) {
    long total = (long)(sec * 44100); size_t at = out.size(); out.resize(at + total * 2);
    for (long d = 0; d < total;) { long n = total - d < 4096 ? total - d : 4096; s.Render(out.data() + at + 2 * d, (int)n); d += n; }
}
static double Db(double a) { return a <= 0 ? -120 : 20 * log10(a / 32768.0); }
static void Wav(const std::string& p, const std::vector<std::int16_t>& pcm) {
    FILE* f = fopen(p.c_str(), "wb"); std::uint32_t bytes = pcm.size() * 2, v;
    fwrite("RIFF", 1, 4, f); v = 36 + bytes; fwrite(&v, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f); v = 16; fwrite(&v, 4, 1, f);
    std::uint16_t h; h = 1; fwrite(&h, 2, 1, f); h = 2; fwrite(&h, 2, 1, f); v = 44100; fwrite(&v, 4, 1, f); v = 176400; fwrite(&v, 4, 1, f);
    h = 4; fwrite(&h, 2, 1, f); h = 16; fwrite(&h, 2, 1, f); fwrite("data", 1, 4, f); fwrite(&bytes, 4, 1, f); fwrite(pcm.data(), 2, pcm.size(), f); fclose(f);
}
struct Stat { double peak, rms100, len; };
static Stat Measure(const std::vector<std::int16_t>& x) {
    Stat s{0, 0, 0}; size_t n = x.size() / 2; double e = 0; long last = -1;
    for (size_t i = 0; i < n; ++i) { double a = std::fabs((double)x[2*i]), b = std::fabs((double)x[2*i+1]); double m = a > b ? a : b;
        if (m > s.peak) s.peak = m; if (i < 4410) e += m * m; if (m > 104) last = (long)i; }  // 104 = -50 dBFS
    s.rms100 = std::sqrt(e / 4410); s.len = (last + 1) / 44100.0; return s;
}
static bool g_run_out = false;
static void StopIt(MusicSynth& s) { if (g_run_out) s.StopAndRunOut(10 * 44100); else s.Stop(); }
int main(int argc, char** argv) {
    if (argc >= 2 && std::string(argv[argc - 1]) == "--run-out") { g_run_out = true; --argc; }
    if (argc != 2 && argc != 6) { fprintf(stderr, "usage: stop_tail CACHE [A T B OUT] [--run-out]\n"); return 2; }
    std::string cache = argv[1];
    if (argc >= 6) {
        int a = atoi(argv[2]); double t = atof(argv[3]); int b = atoi(argv[4]); std::string out = argv[5];
        Song sa, sb; LoadS(cache, a, &sa); LoadS(cache, b, &sb); Bank ba, bb; LoadB(cache, sa.bank, &ba); LoadB(cache, sb.bank, &bb);
        for (int ran = 0; ran < 2; ++ran) {
            auto s = std::make_unique<MusicSynth>(); s->LoadBank(ba); s->Play(sa, 1, 1); s->SetVolume(127, 127);
            std::vector<std::int16_t> pre, pcm; Rend(*s, pre, t); StopIt(*s);
            if (ran) { std::vector<std::int16_t> gap; Rend(*s, gap, 2.0); }
            pcm.assign(pre.end() - 2 * 44100, pre.end());  // the last second of A, then B
            if (sb.bank != sa.bank) s->LoadBank(bb);
            s->Play(sb, 1, 1); s->SetVolume(127, 127); Rend(*s, pcm, 4.0);
            Wav(out + (ran ? "_ran.wav" : "_frozen.wav"), pcm);
        }
        return 0;
    }
    std::vector<int> nums; std::vector<Song> songs;
    for (int n = 0; n < 400; ++n) { Song s; if (LoadS(cache, n, &s)) { nums.push_back(n); songs.push_back(s); } }
    printf("song\tbank\tt\tplay_rms_dB\tkeep_peak_dB\tkeep_first100ms_rms_dB\tkeep_len_s\tswap_bank\tswap_peak_dB\tswap_first100ms_rms_dB\tswap_len_s\n");
    for (size_t i = 0; i < songs.size(); ++i) {
        const Song& sa = songs[i]; Bank ba; LoadB(cache, sa.bank, &ba);
        std::string other; for (size_t k = 1; k < songs.size(); ++k) { const Song& o = songs[(i + k) % songs.size()]; if (o.bank != sa.bank) { other = o.bank; break; } }
        Bank bo; LoadB(cache, other, &bo);
        auto s = std::make_unique<MusicSynth>(); s->LoadBank(ba); s->Play(sa, 1, 1); s->SetVolume(127, 127);
        for (int t = 8; t <= 40; t += 8) {
            std::vector<std::int16_t> seg; Rend(*s, seg, 8.0);
            double e = 0; size_t n = seg.size(); for (size_t j = n - 88200; j < n; ++j) e += (double)seg[j] * seg[j];
            double play = std::sqrt(e / 88200);
            auto k = std::make_unique<MusicSynth>(*s); StopIt(*k); std::vector<std::int16_t> kt; Rend(*k, kt, 3.0); Stat ks = Measure(kt);
            auto c = std::make_unique<MusicSynth>(*s); StopIt(*c); c->LoadBank(bo); std::vector<std::int16_t> ct; Rend(*c, ct, 3.0); Stat cs = Measure(ct);
            printf("%d\t%s\t%d\t%.1f\t%.1f\t%.1f\t%.2f\t%s\t%.1f\t%.1f\t%.2f\n", nums[i], sa.bank.c_str(), t, Db(play), Db(ks.peak), Db(ks.rms100), ks.len,
                   other.c_str(), Db(cs.peak), Db(cs.rms100), cs.len);
            fflush(stdout);
        }
    }
}

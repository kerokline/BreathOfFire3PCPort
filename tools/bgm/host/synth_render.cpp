// synth_render: render one song of the cache to a WAV through psx::MusicSynth.
//
//   synth_render --cache <dir> --song N --out file.wav [--seconds S]
//                [--volume V] [--frames F] [--loops L] [--pre S] [--trace]
//                [--ticks FILE] [--envx] [--phase P] [--offsets FILE] [--solo V]
//
// <dir> is the cache root holding base/bgm/NNN.DAT and base/bgm/bank/NAME.DAT.
// --volume / --frames are Music_Play's crescendo (the title: 100 over 8);
// --pre renders S seconds of the idle SPU before Play (default 0). --trace
// prints the VSync tick of every loop-end jump to stderr. --ticks writes one
// line per VSync with a key on or off: tick, sample, KON mask, KOFF mask,
// voice register writes, then voice:PITCH:note:sample for each voice keyed on;
// --envx writes every VSync's line and appends, after a '|', voices 0..15's
// ENVX as that flush read it ('*' when keyed) - the allocator's view for that
// tick's events, whose key ons are on the next line. --phase is
// where the first VSync falls in the first sample, in 1/256 sample;
// --offsets reads "tick delay" lines (delay in 1/256 sample) and delays those
// VSyncs (an experiment on the interrupt latency, libsnd-reading.md 9);
// --solo mixes only voice V (0..23), the others running unheard (a stem). The WAV is 44,100 Hz
// 16-bit stereo. Game data stays where the cache is; the WAV is derived from it
// and belongs in scratch, never the repo.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "audio/seq.h"
#include "audio/song.h"

namespace {

std::vector<std::uint8_t> ReadFile(const std::string& path) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) {
        std::fprintf(stderr, "synth_render: cannot open %s\n", path.c_str());
        std::exit(2);
    }
    std::vector<std::uint8_t> data;
    std::uint8_t buf[65536];
    std::size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) data.insert(data.end(), buf, buf + n);
    std::fclose(f);
    return data;
}

void Put32(std::FILE* f, std::uint32_t v) {
    const std::uint8_t b[4] = {static_cast<std::uint8_t>(v), static_cast<std::uint8_t>(v >> 8),
                               static_cast<std::uint8_t>(v >> 16), static_cast<std::uint8_t>(v >> 24)};
    std::fwrite(b, 1, 4, f);
}
void Put16(std::FILE* f, std::uint16_t v) {
    const std::uint8_t b[2] = {static_cast<std::uint8_t>(v), static_cast<std::uint8_t>(v >> 8)};
    std::fwrite(b, 1, 2, f);
}

bool g_envx = false;
void WriteTick(const psx::MusicSynth::TickTrace& t, void* user) {
    if (!t.key_on && !t.key_off && !g_envx) return;
    std::FILE* f = static_cast<std::FILE*>(user);
    std::fprintf(f, "%llu %llu %06X %06X %d", static_cast<unsigned long long>(t.tick),
                 static_cast<unsigned long long>(t.sample), t.key_on, t.key_off, t.voice_writes);
    for (int v = 0; v < 24; ++v)
        if (t.key_on & (1u << v)) std::fprintf(f, " %d:%04X:n%d:s%d", v, t.pitch[v], t.note[v], t.vag[v]);
    if (g_envx) {
        // what the allocator reads for this tick's events (their key ons are
        // the next line's): ENVX as this flush read it, '*' after a keyed voice
        std::fprintf(f, " |");
        for (int v = 0; v < 16; ++v) std::fprintf(f, " %04X%s", t.envx[v], t.keyed[v] ? "*" : "");
    }
    std::fprintf(f, "\n");
}

std::vector<int> g_offsets;
int TickOffset(std::uint64_t tick, void*) {
    return tick < g_offsets.size() ? g_offsets[static_cast<std::size_t>(tick)] : 0;
}

void Usage() {
    std::fprintf(stderr,
                 "usage: synth_render --cache DIR --song N --out FILE.wav [--seconds S] [--volume V] [--frames F]\n"
                 "                    [--loops L] [--pre S] [--trace] [--ticks FILE] [--phase P] [--offsets FILE]\n"
                 "                    [--solo V]\n");
    std::exit(2);
}

} // namespace

int main(int argc, char** argv) {
    std::string cache, out, ticks, offsets;
    int song_no = -1, volume = 100, frames = 8, loops = 1, phase = 0, solo = -1;
    double seconds = 60.0, pre = 0.0;
    bool trace = false;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&]() -> const char* {
            if (i + 1 >= argc) Usage();
            return argv[++i];
        };
        if (a == "--cache") cache = next();
        else if (a == "--song") song_no = std::atoi(next());
        else if (a == "--out") out = next();
        else if (a == "--seconds") seconds = std::atof(next());
        else if (a == "--volume") volume = std::atoi(next());
        else if (a == "--frames") frames = std::atoi(next());
        else if (a == "--loops") loops = std::atoi(next());
        else if (a == "--pre") pre = std::atof(next());
        else if (a == "--trace") trace = true;
        else if (a == "--ticks") ticks = next();
        else if (a == "--phase") phase = std::atoi(next());
        else if (a == "--offsets") offsets = next();
        else if (a == "--solo") solo = std::atoi(next());
        else if (a == "--envx") g_envx = true;
        else Usage();
    }
    if (cache.empty() || out.empty() || song_no < 0 || seconds <= 0) Usage();

    char name[32];
    std::snprintf(name, sizeof name, "%03d.DAT", song_no);
    const std::vector<std::uint8_t> song_bytes = ReadFile(cache + "/base/bgm/" + name);
    psx::Song song;
    psx::LoadSong(song_bytes.data(), song_bytes.size(), &song);
    const std::vector<std::uint8_t> bank_bytes = ReadFile(cache + "/base/bgm/bank/" + song.bank + ".DAT");
    psx::Bank bank;
    psx::LoadBank(bank_bytes.data(), bank_bytes.size(), &bank);

    static psx::MusicSynth synth;  // 1 MiB of SPU RAM: not on the stack
    synth.LoadBank(bank);
    synth.SetTickPhase(phase);
    if (solo >= 0) synth.spu().SetDiagnosticMixMask(1u << solo);
    if (!offsets.empty()) {
        std::FILE* of = std::fopen(offsets.c_str(), "r");
        if (!of) {
            std::fprintf(stderr, "synth_render: cannot open %s\n", offsets.c_str());
            return 2;
        }
        unsigned long long t;
        int d;
        while (std::fscanf(of, "%llu %d", &t, &d) == 2) {
            if (t >= g_offsets.size()) g_offsets.resize(static_cast<std::size_t>(t) + 1, 0);
            g_offsets[static_cast<std::size_t>(t)] = d;
        }
        std::fclose(of);
        synth.SetTickOffset(TickOffset, nullptr);
    }
    std::FILE* tick_file = nullptr;
    if (!ticks.empty()) {
        tick_file = std::fopen(ticks.c_str(), "w");
        if (!tick_file) {
            std::fprintf(stderr, "synth_render: cannot write %s\n", ticks.c_str());
            return 2;
        }
        synth.SetTickTrace(WriteTick, tick_file);
    }

    const int rate = 44100;
    const long total = static_cast<long>(seconds * rate);
    const long pre_frames = static_cast<long>(pre * rate);
    std::vector<std::int16_t> pcm(static_cast<std::size_t>(total) * 2);
    long done = 0;
    if (pre_frames > 0) {
        const long n = pre_frames < total ? pre_frames : total;
        synth.Render(pcm.data(), static_cast<int>(n));
        done = n;
    }
    synth.Play(song, volume, frames, loops);
    int jumps = 0;
    const long chunk = 4096;
    while (done < total) {
        const long n = total - done < chunk ? total - done : chunk;
        synth.Render(pcm.data() + 2 * done, static_cast<int>(n));
        done += n;
        if (trace && synth.LoopJumps() != jumps) {
            jumps = synth.LoopJumps();
            std::fprintf(stderr, "loop jump %d at tick %llu\n", jumps,
                         static_cast<unsigned long long>(synth.LastLoopJumpTick()));
        }
    }

    if (tick_file) std::fclose(tick_file);
    std::FILE* f = std::fopen(out.c_str(), "wb");
    if (!f) {
        std::fprintf(stderr, "synth_render: cannot write %s\n", out.c_str());
        return 2;
    }
    const std::uint32_t bytes = static_cast<std::uint32_t>(pcm.size() * 2);
    std::fwrite("RIFF", 1, 4, f);
    Put32(f, 36 + bytes);
    std::fwrite("WAVEfmt ", 1, 8, f);
    Put32(f, 16);
    Put16(f, 1);
    Put16(f, 2);
    Put32(f, rate);
    Put32(f, rate * 4);
    Put16(f, 4);
    Put16(f, 16);
    std::fwrite("data", 1, 4, f);
    Put32(f, bytes);
    for (std::int16_t s : pcm) Put16(f, static_cast<std::uint16_t>(s));
    std::fclose(f);
    std::printf("song %d (bank %s, sub %u): %ld frames, %llu ticks, %d loop jumps%s\n", song_no, song.bank.c_str(), song.sub,
                total, static_cast<unsigned long long>(synth.Ticks()), synth.LoopJumps(), synth.Playing() ? "" : ", ended");
    return 0;
}

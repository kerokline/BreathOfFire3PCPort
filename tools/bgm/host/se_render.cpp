// se_render: one sound-effect tone through the SPU model twice - with its
// tone's ADSR envelope, and with an envelope that is full at once and holds
// (as the PC's banks play every sample, sound-import.md 2.3) - and the two
// energies. The measurement of docs/sound-import.md section 10: what the PC's
// flat effects drop of the envelope alone. tools/bgm/se_census.py writes the
// jobs and reads the results.
//
//   se_render JOBS
//
// JOBS: one line a tone, "id path adsr1 adsr2 pitch max_frames", the ADSR
// words and the pitch in hex; path holds the sample's ADPCM blocks as the VAB
// body has them. Prints per line: id, then for the enveloped and the flat
// render each: frames until the voice is off (or max_frames), energy (the sum
// of the squared left samples, VOLL 0x3FFF), and the frame at which the
// enveloped one's level is first seen at 0x7F00 or above (the attack, to 16
// frames), -1 if never. Game data
// stays in the job files (scratch, never the repo).
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "audio/spu.h"

namespace {

struct Result {
    long frames = 0;
    double energy = 0;
    long attack = -1;
};

Result Run(psx::Spu& s, const std::vector<std::uint8_t>& adpcm, std::uint16_t a1, std::uint16_t a2, std::uint16_t pitch,
           long max_frames) {
    s.Reset();
    s.SetControl(0xC000);  // enabled, unmuted
    s.SetMainVolumeLeft(0x3FFF);
    s.SetMainVolumeRight(0x3FFF);
    const std::uint32_t base = 0x1000;
    s.WriteRam(base, adpcm.data(), static_cast<std::uint32_t>(adpcm.size()));
    s.SetVoiceStartAddress(0, static_cast<std::uint16_t>(base / 8));
    s.SetVoicePitch(0, pitch);
    s.SetVoiceVolumeLeft(0, 0x3FFF);
    s.SetVoiceVolumeRight(0, 0);
    s.SetVoiceAdsr1(0, a1);
    s.SetVoiceAdsr2(0, a2);
    s.KeyOn(1);
    Result r;
    std::int16_t out[2 * 16];
    bool started = false;
    while (r.frames < max_frames) {
        s.Render(out, 16);
        for (int i = 0; i < 16; ++i) r.energy += static_cast<double>(out[2 * i]) * out[2 * i];
        r.frames += 16;
        if (s.VoiceEnvelope(0) > 0) started = true;
        if (r.attack < 0 && s.VoiceEnvelope(0) >= 0x7F00) r.attack = r.frames;
        if (started && s.VoiceAdsrPhase(0) == 4) break;  // off: released to zero, or ended
    }
    return r;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: se_render JOBS\n");
        return 2;
    }
    std::FILE* jobs = std::fopen(argv[1], "r");
    if (!jobs) {
        std::fprintf(stderr, "se_render: cannot open %s\n", argv[1]);
        return 2;
    }
    static psx::Spu spu;  // 512 KiB of SPU RAM: not on the stack
    char id[64], path[1024];
    unsigned a1, a2, pitch;
    long max_frames;
    while (std::fscanf(jobs, "%63s %1023s %x %x %x %ld", id, path, &a1, &a2, &pitch, &max_frames) == 6) {
        std::FILE* f = std::fopen(path, "rb");
        if (!f) {
            std::fprintf(stderr, "se_render: cannot open %s\n", path);
            return 2;
        }
        std::vector<std::uint8_t> adpcm;
        for (int c; (c = std::fgetc(f)) != EOF;) adpcm.push_back(static_cast<std::uint8_t>(c));
        std::fclose(f);
        if (adpcm.empty() || adpcm.size() > 0x70000) {
            std::fprintf(stderr, "se_render: %s: %zu bytes\n", path, adpcm.size());
            return 2;
        }
        const Result e = Run(spu, adpcm, static_cast<std::uint16_t>(a1), static_cast<std::uint16_t>(a2),
                             static_cast<std::uint16_t>(pitch), max_frames);
        // full at once and held: the envelope the PC's flat WAV amounts to
        const Result fl = Run(spu, adpcm, 0x00FF, 0x0000, static_cast<std::uint16_t>(pitch), max_frames);
        std::printf("%s %ld %.6e %ld %.6e %ld\n", id, e.frames, e.energy, fl.frames, fl.energy, e.attack);
    }
    std::fclose(jobs);
    return 0;
}

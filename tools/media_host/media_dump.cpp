// media_dump: the oracle checks' side of src/media (docs/psx-media-round.md M1).
//   media_dump str FILE OUT         OUT.rgb: every frame, the undoubled decode, RGB24;
//                                   OUT.pcm: the soundtrack, s16le, the stream's own rate;
//                                   prints the frame count, rate and audio form
//   media_dump xa  CLIP OUT.pcm [--no-resample]
//                                   the clip as s16le stereo, 44,100 Hz; --no-resample gives
//                                   the ADPCM decode alone at the clip's own rate (a mono
//                                   clip doubled), for holding it to tools/xa.py's decode
// Files only; game data stays in the scratch folder it is read from.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "media/str_movie.h"
#include "media/xa_adpcm.h"
#include "media/xa_clip.h"

namespace {

bool ReadFile(const char* path, std::vector<std::uint8_t>& out) {
    std::FILE* f = std::fopen(path, "rb");
    if (f == nullptr) return false;
    std::fseek(f, 0, SEEK_END);
    const long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    out.resize(static_cast<std::size_t>(n));
    const bool ok = n == 0 || std::fread(out.data(), 1, out.size(), f) == out.size();
    std::fclose(f);
    return ok;
}

void WritePcm(std::FILE* f, const std::int16_t* p, std::size_t n) {
    // s16le whatever the host's order is
    for (std::size_t i = 0; i < n; ++i) {
        const unsigned char b[2] = {static_cast<unsigned char>(p[i] & 0xFF), static_cast<unsigned char>((p[i] >> 8) & 0xFF)};
        std::fwrite(b, 1, 2, f);
    }
}

int Str(const char* path, const char* out) {
    std::vector<std::uint8_t> file;
    if (!ReadFile(path, file)) { std::fprintf(stderr, "cannot read %s\n", path); return 1; }
    media::StrMovie m;
    char why[256];
    if (!m.Open(file.data(), file.size(), why, sizeof(why))) { std::fprintf(stderr, "refused: %s\n", why); return 1; }
    std::FILE* rgb = std::fopen((std::string(out) + ".rgb").c_str(), "wb");
    if (rgb == nullptr) return 1;
    const std::uint32_t w = m.Width() / 2, h = m.Height() / 2;
    std::vector<std::uint8_t> row(static_cast<std::size_t>(w) * 3);
    while (m.NextFrame(why, sizeof(why))) {
        const std::uint32_t* p = m.RawFrame320();
        for (std::uint32_t y = 0; y < h; ++y) {
            for (std::uint32_t x = 0; x < w; ++x) {
                const std::uint32_t v = p[y * w + x];
                row[3 * x] = static_cast<std::uint8_t>(v >> 16);
                row[3 * x + 1] = static_cast<std::uint8_t>(v >> 8);
                row[3 * x + 2] = static_cast<std::uint8_t>(v);
            }
            std::fwrite(row.data(), 1, row.size(), rgb);
        }
    }
    std::fclose(rgb);
    if (why[0]) { std::fprintf(stderr, "decode fault after %u frames: %s\n", m.FramesDecoded(), why); return 1; }
    std::printf("frames %u of %u, %ux%u, %.6f s a frame (%.3f fps)\n", m.FramesDecoded(), m.FrameCount(), w, h,
                m.FrameSeconds(), 1.0 / m.FrameSeconds());
    if (m.HasAudio()) {
        std::printf("audio %u Hz, %u channels, %zu frames\n", m.AudioRate(), m.AudioChannels(), m.AudioFrames());
        std::FILE* pcm = std::fopen((std::string(out) + ".pcm").c_str(), "wb");
        if (pcm == nullptr) return 1;
        WritePcm(pcm, m.Audio(), m.AudioFrames() * m.AudioChannels());
        std::fclose(pcm);
    } else {
        std::printf("no audio\n");
    }
    return 0;
}

int Xa(const char* path, const char* out, bool resample) {
    std::vector<std::uint8_t> file;
    if (!ReadFile(path, file)) { std::fprintf(stderr, "cannot read %s\n", path); return 1; }
    media::XaClip clip;
    char why[256];
    if (!clip.Open(file.data(), file.size(), why, sizeof(why))) { std::fprintf(stderr, "refused: %s\n", why); return 1; }
    std::FILE* f = std::fopen(out, "wb");
    if (f == nullptr) return 1;
    if (resample) {
        std::vector<std::int16_t> buf(2 * 4096);
        std::uint64_t total = 0;
        for (;;) {
            const std::size_t n = clip.Read(buf.data(), 4096);
            if (n == 0) break;
            WritePcm(f, buf.data(), 2 * n);
            total += n;
        }
        std::printf("%llu frames at 44100 Hz (TotalFrames %llu)\n", static_cast<unsigned long long>(total),
                    static_cast<unsigned long long>(clip.TotalFrames()));
    } else {
        // The header is read here again: the tool wants the groups, not the clip class's output.
        const std::uint8_t* d = file.data();
        const bool stereo = d[12] == 2;
        const std::uint32_t groups = d[16] | d[17] << 8 | d[18] << 16 | static_cast<std::uint32_t>(d[19]) << 24;
        media::XaHistory hist;
        std::int16_t l[media::kXaMonoSamples], r[media::kXaMonoSamples];
        std::uint64_t total = 0;
        for (std::uint32_t g = 0; g < groups; ++g) {
            const std::size_t n = media::XaDecodeGroup(d + 24 + static_cast<std::size_t>(g) * media::kXaGroupBytes, stereo, hist, l, r);
            for (std::size_t i = 0; i < n; ++i) {
                const std::int16_t pair[2] = {l[i], stereo ? r[i] : l[i]};
                WritePcm(f, pair, 2);
            }
            total += n;
        }
        std::printf("%llu frames at the clip's own rate\n", static_cast<unsigned long long>(total));
    }
    std::fclose(f);
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc >= 4 && std::strcmp(argv[1], "str") == 0) return Str(argv[2], argv[3]);
    if (argc >= 4 && std::strcmp(argv[1], "xa") == 0) {
        return Xa(argv[2], argv[3], !(argc >= 5 && std::strcmp(argv[4], "--no-resample") == 0));
    }
    std::fprintf(stderr, "usage: media_dump str FILE OUT | media_dump xa CLIP OUT.pcm [--no-resample]\n");
    return 2;
}

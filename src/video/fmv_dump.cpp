// bof3x-fmvdump: decodes an AVI, or a PlayStation STR (DIV-0214), with the
// game's own decoders (movie.h) and writes every frame raw, for
// tools/fmv_compare.py to hold against a reference decode by ffmpeg
// (DIV-0205's verification).
//
//   bof3x-fmvdump IN.avi|IN.STR OUT.rgb [--raw OUT.raw] [--pcm OUT.pcm]
//
// OUT.rgb: every frame as width x height x 3 bytes R, G, B, rows top-down
// (ffmpeg's -pix_fmt rgb24). --raw: the Video 1 frames in the stream's own
// depth, 16-bit little-endian words a pixel (rgb555le, or the palette
// index in the low byte; AVI alone). --pcm: the audio stream's bytes as they
// are (an STR's: its decoded 16-bit little-endian PCM). The stream's
// description goes to stdout. Exit 0, 1 on an open failure, 2 on a decode
// fault (the frames before it are written).
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "video/movie.h"

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: bof3x-fmvdump IN.avi OUT.rgb [--raw OUT.raw] [--pcm OUT.pcm]\n");
        return 1;
    }
    const char* raw_path = nullptr;
    const char* pcm_path = nullptr;
    for (int i = 3; i + 1 < argc; i += 2) {
        if (std::strcmp(argv[i], "--raw") == 0) raw_path = argv[i + 1];
        else if (std::strcmp(argv[i], "--pcm") == 0) pcm_path = argv[i + 1];
    }
    FILE* in = std::fopen(argv[1], "rb");
    if (!in) {
        std::fprintf(stderr, "cannot open %s\n", argv[1]);
        return 1;
    }
    std::fseek(in, 0, SEEK_END);
    const long size = std::ftell(in);
    std::fseek(in, 0, SEEK_SET);
    auto* data = static_cast<std::uint8_t*>(std::malloc(size > 0 ? static_cast<std::size_t>(size) : 1));
    if (!data || std::fread(data, 1, static_cast<std::size_t>(size), in) != static_cast<std::size_t>(size)) {
        std::fprintf(stderr, "cannot read %s\n", argv[1]);
        return 1;
    }
    std::fclose(in);
    video::Movie movie;
    char why[256];
    if (!movie.Open(data, static_cast<std::size_t>(size), why, sizeof why)) {
        std::fprintf(stderr, "%s: %s\n", argv[1], why);
        return 1;
    }
    if (const video::AviFile* f = movie.Avi()) {
        std::printf("codec %s handler %s compression %.4s %u x %u %u bits, %u/%u fps, %u frames (strh says %u), idx1 %u, "
                    "audio %s\n",
                    movie.Codec(), f->video.handler, f->video.compression, f->video.width, f->video.height,
                    f->video.bits, f->video.rate, f->video.scale, movie.FrameCount(), f->video.length, f->idx1_entries,
                    movie.HasPcm() ? "pcm" : "none");
        if (movie.HasPcm())
            std::printf("audio tag %u, %u channels, %u Hz, %u bits, %u chunks, %u bytes\n", f->audio.format_tag,
                        f->audio.channels, f->audio.rate, f->audio.bits, f->audio_chunks, movie.AudioBytes());
    } else {
        std::printf("codec %s %u x %u, %.4f s a frame, %u frames, audio %s\n", movie.Codec(), movie.Width(),
                    movie.Height(), movie.FrameSeconds(), movie.FrameCount(), movie.HasPcm() ? "pcm" : "none");
        if (movie.HasPcm())
            std::printf("audio %u channels, %u Hz, %u bits, %u bytes\n", movie.AudioChannels(), movie.AudioRate(),
                        movie.AudioBits(), movie.AudioBytes());
    }
    FILE* out = std::fopen(argv[2], "wb");
    FILE* raw = raw_path ? std::fopen(raw_path, "wb") : nullptr;
    if (!out) {
        std::fprintf(stderr, "cannot write %s\n", argv[2]);
        return 1;
    }
    const std::uint32_t w = movie.Width(), h = movie.Height();
    auto* row = static_cast<std::uint8_t*>(std::malloc(static_cast<std::size_t>(w) * 3));
    int rc = 0;
    for (;;) {
        const bool ok = movie.NextFrame(why, sizeof why);
        if (!ok && !why[0]) break;
        if (!ok) {
            std::fprintf(stderr, "frame %u: %s\n", movie.FramesDecoded(), why);
            rc = 2;
        }
        const std::uint32_t* px = movie.Frame();
        for (std::uint32_t y = 0; y < h; ++y) {
            for (std::uint32_t x = 0; x < w; ++x) {
                const std::uint32_t c = px[y * w + x];
                row[x * 3] = static_cast<std::uint8_t>(c >> 16);
                row[x * 3 + 1] = static_cast<std::uint8_t>(c >> 8);
                row[x * 3 + 2] = static_cast<std::uint8_t>(c);
            }
            std::fwrite(row, 1, static_cast<std::size_t>(w) * 3, out);
        }
        if (raw && movie.RawFrame()) std::fwrite(movie.RawFrame(), 2, static_cast<std::size_t>(w) * h, raw);
        if (!ok) break;
    }
    std::fclose(out);
    if (raw) std::fclose(raw);
    if (pcm_path && movie.HasPcm()) {
        FILE* pcm = std::fopen(pcm_path, "wb");
        if (pcm) {
            std::uint32_t n;
            while (const std::uint8_t* chunk = movie.NextAudio(&n)) std::fwrite(chunk, 1, n, pcm);
            std::fclose(pcm);
        }
    }
    std::printf("decoded %u frames\n", movie.FramesDecoded());
    return rc;
}

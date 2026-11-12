// A movie: an AVI's bytes, its chunk list, and the decoder its video stream
// names - Cinepak ("cvid") or Microsoft Video 1 ("MSVC" / "CRAM"), the two
// the game ships (docs/replacing-mci.md "As built"; DIV-0205) - or a
// PlayStation STR (media/str_movie.h; DIV-0214), the disc's own CAPCOM30.STR
// decoded by our MDEC. The frames come out in order, each over the last, as
// 0x00RRGGBB rows top-down; the PCM chunks of the audio stream, when there is
// one, in order too. The accessors name no container: a caller asks for the
// frame time, the codec's name and the audio's rate, channels and bits.
#pragma once

#include <cstddef>
#include <cstdint>

#include "video/avi_reader.h"
#include "video/cinepak.h"
#include "video/msvideo1.h"

namespace media { class StrMovie; }

namespace video {

class Movie {
public:
    Movie();
    ~Movie();
    Movie(const Movie&) = delete;
    Movie& operator=(const Movie&) = delete;

    // Takes the file's bytes (kept alive by the caller) and readies the
    // decoder: an STR when its first sector carries the CD sync
    // (media::IsStr), an AVI otherwise. False with the reason in `why`: not
    // an AVI, a video stream of a codec other than the two, an 8-bit Video 1
    // stream without a palette, an STR that media::StrMovie refuses.
    bool Open(const std::uint8_t* data, std::size_t size, char* why, std::size_t why_n);

    std::uint32_t Width() const;
    std::uint32_t Height() const;
    // The video stream's frame count: the chunks in the movi list (strh's
    // dwLength is what the writer claimed; the chunks are what is there); an
    // STR's frames.
    std::uint32_t FrameCount() const;
    std::uint32_t FramesDecoded() const;
    // Seconds a frame: dwScale / dwRate; an STR's is media::StrMovie::FrameSeconds.
    double FrameSeconds() const;

    // Decodes the next video frame. False at the end of the stream, or on a
    // decode fault with the reason in `why` (the frame then holds what was
    // decoded before it; FramesDecoded still counts it).
    bool NextFrame(char* why, std::size_t why_n);
    const std::uint32_t* Frame() const;
    // The Video 1 frame in the stream's own depth (msvideo1.h Raw), null for
    // Cinepak and for an STR.
    const std::uint16_t* RawFrame() const { return msvc_ ? msvc_->Raw() : nullptr; }

    // The audio's data in chunks, in order: the next one's bytes and size,
    // or null at the end. An AVI's is each audio chunk of a PCM stream
    // (format tag 1, 8 or 16 bits; any other audio stream is reported
    // absent); an STR's is its decoded soundtrack in pieces of 16 KB, 16-bit
    // little-endian.
    bool HasPcm() const { return pcm_; }
    const std::uint8_t* NextAudio(std::uint32_t* size);
    // The audio's bytes summed, for the stream's length.
    std::uint32_t AudioBytes() const { return audio_bytes_; }
    // The audio's form: samples a second a channel, channels, bits a sample
    // (0 each when there is no PCM).
    std::uint32_t AudioRate() const { return pcm_ ? audio_rate_ : 0; }
    unsigned AudioChannels() const { return pcm_ ? audio_channels_ : 0; }
    unsigned AudioBits() const { return pcm_ ? audio_bits_ : 0; }
    // The audio's length in seconds.
    double AudioSeconds() const;
    const char* Codec() const { return codec_; }
    // "AVI" or "STR": the container the bytes were.
    const char* Container() const { return str_ ? "STR" : "AVI"; }

    // The AVI's stream description, for the dump tool alone; null for an STR.
    const AviFile* Avi() const { return str_ ? nullptr : &avi_; }

private:
    AviFile avi_{};
    media::StrMovie* str_ = nullptr;
    std::uint32_t str_audio_at_ = 0;     // the STR audio walk: bytes handed out
    Cinepak* cvid_ = nullptr;
    MsVideo1* msvc_ = nullptr;
    std::uint32_t next_chunk_ = 0;       // the video walk
    std::uint32_t next_audio_chunk_ = 0; // the audio walk
    std::uint32_t decoded_ = 0;
    std::uint32_t audio_bytes_ = 0;
    bool pcm_ = false;
    std::uint32_t audio_rate_ = 0;
    unsigned audio_channels_ = 0, audio_bits_ = 0;
    const char* codec_ = "";
};

}  // namespace video

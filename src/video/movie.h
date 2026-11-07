// A movie: an AVI's bytes, its chunk list, and the decoder its video stream
// names - Cinepak ("cvid") or Microsoft Video 1 ("MSVC" / "CRAM"), the two
// the game ships (docs/replacing-mci.md "As built"; DIV-0205). The frames
// come out in order, each over the last, as 0x00RRGGBB rows top-down; the
// PCM chunks of the audio stream, when there is one, in order too.
#pragma once

#include <cstddef>
#include <cstdint>

#include "video/avi_reader.h"
#include "video/cinepak.h"
#include "video/msvideo1.h"

namespace video {

class Movie {
public:
    Movie();
    ~Movie();
    Movie(const Movie&) = delete;
    Movie& operator=(const Movie&) = delete;

    // Takes the AVI's bytes (kept alive by the caller), parses them and
    // readies the decoder. False with the reason in `why`: not an AVI, a
    // video stream of a codec other than the two, an 8-bit Video 1 stream
    // without a palette.
    bool Open(const std::uint8_t* data, std::size_t size, char* why, std::size_t why_n);

    const AviFile& File() const { return avi_; }
    std::uint32_t Width() const { return avi_.video.width; }
    std::uint32_t Height() const { return avi_.video.height; }
    // The video stream's frame count: the chunks in the movi list (strh's
    // dwLength is what the writer claimed; the chunks are what is there).
    std::uint32_t FrameCount() const { return avi_.video_chunks; }
    std::uint32_t FramesDecoded() const { return decoded_; }
    // Seconds a frame: dwScale / dwRate.
    double FrameSeconds() const { return static_cast<double>(avi_.video.scale) / avi_.video.rate; }

    // Decodes the next video frame. False at the end of the stream, or on a
    // decode fault with the reason in `why` (the frame then holds what was
    // decoded before it; FramesDecoded still counts it).
    bool NextFrame(char* why, std::size_t why_n);
    const std::uint32_t* Frame() const;
    // The Video 1 frame in the stream's own depth (msvideo1.h Raw), null for
    // Cinepak.
    const std::uint16_t* RawFrame() const { return msvc_ ? msvc_->Raw() : nullptr; }

    // The audio stream's data chunks, in order: the next one's bytes and
    // size, or null at the end. Only for a PCM stream (format tag 1, 8 or
    // 16 bits); any other audio stream is reported absent.
    bool HasPcm() const { return pcm_; }
    const std::uint8_t* NextAudio(std::uint32_t* size);
    // Every audio chunk's bytes summed, for the stream's length.
    std::uint32_t AudioBytes() const { return audio_bytes_; }
    const char* Codec() const { return codec_; }

private:
    AviFile avi_{};
    Cinepak* cvid_ = nullptr;
    MsVideo1* msvc_ = nullptr;
    std::uint32_t next_chunk_ = 0;       // the video walk
    std::uint32_t next_audio_chunk_ = 0; // the audio walk
    std::uint32_t decoded_ = 0;
    std::uint32_t audio_bytes_ = 0;
    bool pcm_ = false;
    const char* codec_ = "";
};

}  // namespace video

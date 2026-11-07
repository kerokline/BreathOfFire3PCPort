// See movie.h.
#include "video/movie.h"

#include <cstdio>
#include <cstring>

namespace video {

Movie::Movie() = default;

Movie::~Movie() {
    delete cvid_;
    delete msvc_;
    Close(&avi_);
}

bool Movie::Open(const std::uint8_t* data, std::size_t size, char* why, std::size_t why_n) {
    if (!Parse(&avi_, data, size, why, why_n)) return false;
    const VideoStream& v = avi_.video;
    const bool cinepak = std::strcmp(v.handler, "CVID") == 0 || std::memcmp(v.compression, "cvid", 4) == 0;
    const bool video1 = std::strcmp(v.handler, "MSVC") == 0 || std::strcmp(v.handler, "CRAM") == 0 ||
                        std::memcmp(v.compression, "CRAM", 4) == 0 || std::memcmp(v.compression, "MSVC", 4) == 0 ||
                        std::memcmp(v.compression, "msvc", 4) == 0 || std::memcmp(v.compression, "cram", 4) == 0;
    if (cinepak) {
        cvid_ = new Cinepak;
        if (!cvid_->Init(v.width, v.height)) {
            std::snprintf(why, why_n, "no memory for a %u x %u Cinepak frame", v.width, v.height);
            Close(&avi_);
            return false;
        }
        codec_ = "Cinepak";
    } else if (video1) {
        if (v.bits == 8 && v.palette_n == 0) {
            std::snprintf(why, why_n, "an 8-bit Video 1 stream without a palette");
            Close(&avi_);
            return false;
        }
        msvc_ = new MsVideo1;
        if (!msvc_->Init(v.width, v.height, v.bits, v.palette, v.palette_n)) {
            std::snprintf(why, why_n, "a %u-bit Video 1 stream (8 or 16 are read), or no memory for a %u x %u frame",
                          v.bits, v.width, v.height);
            Close(&avi_);
            return false;
        }
        codec_ = "Microsoft Video 1";
    } else {
        std::snprintf(why, why_n, "the video stream is '%s' (%.4s), not Cinepak or Microsoft Video 1", v.handler,
                      v.compression);
        Close(&avi_);
        return false;
    }
    const AudioStream& a = avi_.audio;
    pcm_ = a.present && a.format_tag == 1 && (a.bits == 8 || a.bits == 16) && a.channels >= 1 && a.channels <= 2 &&
           a.rate >= 4000 && a.rate <= 192000 && avi_.audio_chunks > 0;
    if (pcm_)
        for (std::uint32_t i = 0; i < avi_.n_chunks; ++i)
            if (!avi_.chunks[i].video && avi_.chunks[i].stream == avi_.audio_stream) audio_bytes_ += avi_.chunks[i].size;
    return true;
}

bool Movie::NextFrame(char* why, std::size_t why_n) {
    while (next_chunk_ < avi_.n_chunks) {
        const Chunk& c = avi_.chunks[next_chunk_++];
        if (!c.video || c.stream != avi_.video_stream) continue;
        ++decoded_;
        const std::uint8_t* p = avi_.data + c.offset;
        if (c.size == 0) return true;   // an empty chunk: the frame repeats
        if (cvid_) return cvid_->Decode(p, c.size, why, why_n);
        return msvc_->Decode(p, c.size, why, why_n);
    }
    why[0] = 0;
    return false;
}

const std::uint32_t* Movie::Frame() const { return cvid_ ? cvid_->Frame() : msvc_ ? msvc_->Frame() : nullptr; }

const std::uint8_t* Movie::NextAudio(std::uint32_t* size) {
    if (!pcm_) return nullptr;
    while (next_audio_chunk_ < avi_.n_chunks) {
        const Chunk& c = avi_.chunks[next_audio_chunk_++];
        if (c.video || c.stream != avi_.audio_stream) continue;
        *size = c.size;
        return avi_.data + c.offset;
    }
    return nullptr;
}

}  // namespace video

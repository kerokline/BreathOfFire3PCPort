// See movie.h.
#include "video/movie.h"

#include <cstdio>
#include <cstring>

#include "media/str_movie.h"

namespace video {

Movie::Movie() = default;

Movie::~Movie() {
    delete cvid_;
    delete msvc_;
    delete str_;
    Close(&avi_);
}

std::uint32_t Movie::Width() const { return str_ ? str_->Width() : avi_.video.width; }
std::uint32_t Movie::Height() const { return str_ ? str_->Height() : avi_.video.height; }
std::uint32_t Movie::FrameCount() const { return str_ ? str_->FrameCount() : avi_.video_chunks; }
std::uint32_t Movie::FramesDecoded() const { return str_ ? str_->FramesDecoded() : decoded_; }
double Movie::FrameSeconds() const {
    return str_ ? str_->FrameSeconds() : static_cast<double>(avi_.video.scale) / avi_.video.rate;
}
double Movie::AudioSeconds() const {
    if (!pcm_) return 0;
    return static_cast<double>(audio_bytes_) / (static_cast<double>(audio_rate_) * audio_channels_ * (audio_bits_ / 8));
}

bool Movie::Open(const std::uint8_t* data, std::size_t size, char* why, std::size_t why_n) {
    if (media::IsStr(data, size)) {
        // DIVERGENCE DIV-0214: the PlayStation disc's own CAPCOM30.STR, decoded
        // by our MDEC and XA decoders, where an AVI would otherwise be played.
        str_ = new media::StrMovie;
        if (!str_->Open(data, size, why, why_n)) {
            delete str_;
            str_ = nullptr;
            return false;
        }
        codec_ = "PlayStation STR (MDEC)";
        pcm_ = str_->HasAudio() && str_->AudioChannels() >= 1 && str_->AudioChannels() <= 2 && str_->AudioFrames() > 0;
        if (pcm_) {
            audio_rate_ = str_->AudioRate();
            audio_channels_ = str_->AudioChannels();
            audio_bits_ = 16;
            audio_bytes_ = static_cast<std::uint32_t>(str_->AudioFrames() * str_->AudioChannels() * 2);
        }
        return true;
    }
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
    audio_rate_ = a.rate;
    audio_channels_ = a.channels;
    audio_bits_ = a.bits;
    if (pcm_)
        for (std::uint32_t i = 0; i < avi_.n_chunks; ++i)
            if (!avi_.chunks[i].video && avi_.chunks[i].stream == avi_.audio_stream) audio_bytes_ += avi_.chunks[i].size;
    return true;
}

bool Movie::NextFrame(char* why, std::size_t why_n) {
    if (str_) return str_->NextFrame(why, why_n);
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

const std::uint32_t* Movie::Frame() const { return str_ ? str_->Frame() : cvid_ ? cvid_->Frame() : msvc_ ? msvc_->Frame() : nullptr; }

const std::uint8_t* Movie::NextAudio(std::uint32_t* size) {
    if (!pcm_) return nullptr;
    if (str_) {
        constexpr std::uint32_t kChunk = 16384;
        if (str_audio_at_ >= audio_bytes_) return nullptr;
        const std::uint32_t left = audio_bytes_ - str_audio_at_;
        *size = left < kChunk ? left : kChunk;
        const std::uint8_t* at = reinterpret_cast<const std::uint8_t*>(str_->Audio()) + str_audio_at_;
        str_audio_at_ += *size;
        return at;
    }
    while (next_audio_chunk_ < avi_.n_chunks) {
        const Chunk& c = avi_.chunks[next_audio_chunk_++];
        if (c.video || c.stream != avi_.audio_stream) continue;
        *size = c.size;
        return avi_.data + c.offset;
    }
    return nullptr;
}

}  // namespace video

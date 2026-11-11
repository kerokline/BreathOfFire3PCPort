// The "XA clip": a jingle kept as the disc's own sound groups, read out as
// 44,100 Hz 16-bit stereo (docs/psx-media-round.md 2.2).
//
//   offset size  field
//        0    8  magic "BOF3XA\0\1"
//        8    4  u32 sample rate: 37800 or 18900
//       12    2  u16 channels: 1 or 2
//       14    2  u16 bits a sample: 4
//       16    4  u32 sound groups n
//       20    4  u32 0
//       24 128n  the sound groups, in play order
// Little-endian throughout. Each clip decodes from a fresh ADPCM history
// (xa_adpcm.h), then is resampled by 7/6 (37,800 Hz) or 7/3 (18,900 Hz)
// (resample.h); a mono clip is doubled to both channels. TotalFrames() is
// ResampleOutputs(samples a side, 6 or 3) - see resample.h for the rule.
#pragma once

#include <cstddef>
#include <cstdint>

#include "media/resample.h"
#include "media/xa_adpcm.h"

namespace media {

// True when the first bytes are the magic.
bool IsXaClip(const std::uint8_t* data, std::size_t size);

class XaClip {
public:
    // Validates the header and the size and refuses with the reason in `why`
    // (a rate other than 37,800 / 18,900, channels not 1 / 2, bits not 4, no
    // groups, a size that is not 24 + 128 n). `data` stays the caller's and
    // must outlive the clip.
    bool Open(const std::uint8_t* data, std::size_t size, char* why, std::size_t why_n);
    // Output stereo frames at 44,100 Hz, exactly.
    std::uint64_t TotalFrames() const { return total_; }
    // Up to `frames` interleaved L, R frames; returns the frames written, 0 at
    // the end.
    std::size_t Read(std::int16_t* out, std::size_t frames);
    // Back to frame 0 with the decoder and filter reset: the same samples again.
    void Rewind();

private:
    void PushNext();   // one input sample (a zero past the end) into the filters, outputs into the queue

    const std::uint8_t* groups_ = nullptr;
    std::uint32_t group_count_ = 0;
    bool stereo_ = false;
    unsigned down_ = 6;
    std::uint64_t total_ = 0;

    XaHistory hist_;
    Resampler rs_[2];
    std::int16_t left_[kXaMonoSamples] = {};
    std::int16_t right_[kXaMonoSamples] = {};
    std::size_t group_samples_ = 0;      // the current group's samples a side
    std::size_t group_pos_ = 0;
    std::uint32_t next_group_ = 0;
    std::uint64_t made_ = 0;             // output frames handed out
    std::int16_t queue_[2][8] = {};      // outputs waiting (left, right)
    std::size_t queued_ = 0;
    std::size_t queue_pos_ = 0;
};

}  // namespace media

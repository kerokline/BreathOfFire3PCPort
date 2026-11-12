#include "media/resample.h"

#include "media/resample_table.h"

namespace media {

bool Resampler::Reset(unsigned down) {
    if (down != 6 && down != 3) return false;
    for (std::int16_t& h : hist_) h = 0;
    in_ = 0;
    next_ = 0;
    down_ = down;
    return true;
}

std::size_t Resampler::Push(std::int16_t x, std::int16_t* out) {
    const std::uint64_t i = in_++;
    hist_[i & 63] = x;
    std::size_t made = 0;
    // Output k needs the inputs i0 - H + 1 .. i0 + H, i0 = the integer part of
    // k * M / 7, its phase r = k * M mod 7; it is complete once input i0 + H
    // is in. The 2H = 48 inputs it reads are within the last 64, so the ring
    // never mixes old data in; the slots of inputs before 0 are still zero.
    for (;;) {
        const std::uint64_t pos = next_ * down_;
        const std::uint64_t i0 = pos / kResampleUp;
        if (i0 + kResampleHalf > i) break;
        const std::int32_t* taps = kResampleTaps[pos % kResampleUp];
        const std::uint64_t first = i0 + 1 - kResampleHalf;   // wraps below 0; the ring index is mod 64
        std::int64_t acc = 0;
        for (int t = 0; t < 2 * kResampleHalf; ++t) {
            acc += static_cast<std::int64_t>(taps[t]) * hist_[(first + static_cast<std::uint64_t>(t)) & 63];
        }
        std::int64_t v = (acc + kResampleOne / 2) >> 16;
        v = v < -32768 ? -32768 : (v > 32767 ? 32767 : v);
        out[made++] = static_cast<std::int16_t>(v);
        ++next_;
    }
    return made;
}

}  // namespace media

// A streaming polyphase resampler to 44,100 Hz, 16-bit, integer arithmetic
// only (so the 32-bit and 64-bit builds give the same samples).
//
// The ratio is 7 / M with M = 6 (37,800 -> 44,100 Hz) or M = 3 (18,900 ->
// 44,100 Hz). One low-pass prototype, 7 phases of 48 taps (resample_table.h,
// made by tools/gen_resample.py), serves both: its cut-off is a fixed fraction
// of the input's Nyquist rate.
//
// Output length rule. The input is `n` samples; before the first and after
// the last the signal is zero. Output k lies at input position k * M / 7 and
// is the filter centred there (no delay: output 0 is centred on input 0). The
// outputs are those with k * M < 7 * n, so there are ceil(7 * n / M) of them
// (0 for n = 0): the clip's duration is kept, and the tail rings out into
// the zeros past the end. `Push` makes the outputs as the input arrives; the
// caller flushes by pushing zeros (at most 24 of them are needed for the last
// output) and takes the first ResampleOutputs(n, M) outputs.
//
// Each output is the sum of the taps times the inputs in Q16, rounded to
// nearest (half up), clamped to 16 bits. A constant input comes out the same
// constant once the filter is full (each phase's taps sum to exactly 1.0).
#pragma once

#include <cstddef>
#include <cstdint>

namespace media {

// ceil(7 * n / M), the output length rule.
constexpr std::uint64_t ResampleOutputs(std::uint64_t n, unsigned down) {
    return (7 * n + down - 1) / down;
}

class Resampler {
public:
    // `down` is 6 or 3. False for another value.
    bool Reset(unsigned down);
    // Takes one input sample and writes the outputs it completes into `out`
    // (room for 3). Returns how many (0..3).
    std::size_t Push(std::int16_t x, std::int16_t* out);

private:
    std::int16_t hist_[64] = {};   // the last 64 inputs, a ring
    std::uint64_t in_ = 0;         // inputs taken
    std::uint64_t next_ = 0;       // the next output's index k
    unsigned down_ = 6;
};

}  // namespace media

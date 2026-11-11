#include "media/xa_clip.h"

#include <cstring>

#include "media/why.h"

namespace media {

namespace {
const std::uint8_t kMagic[8] = {'B', 'O', 'F', '3', 'X', 'A', 0, 1};
constexpr std::size_t kHeader = 24;

std::uint32_t U32(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(p[0]) | static_cast<std::uint32_t>(p[1]) << 8 |
           static_cast<std::uint32_t>(p[2]) << 16 | static_cast<std::uint32_t>(p[3]) << 24;
}
std::uint32_t U16(const std::uint8_t* p) { return static_cast<std::uint32_t>(p[0]) | static_cast<std::uint32_t>(p[1]) << 8; }
}  // namespace

bool IsXaClip(const std::uint8_t* data, std::size_t size) {
    return data != nullptr && size >= sizeof(kMagic) && std::memcmp(data, kMagic, sizeof(kMagic)) == 0;
}

bool XaClip::Open(const std::uint8_t* data, std::size_t size, char* why, std::size_t why_n) {
    groups_ = nullptr;
    group_count_ = 0;
    total_ = 0;
    if (!IsXaClip(data, size)) return Fail(why, why_n, "not an XA clip: the magic is missing");
    if (size < kHeader) return Fail(why, why_n, "XA clip: %zu bytes, the header is %zu", size, kHeader);
    const std::uint32_t rate = U32(data + 8);
    const std::uint32_t channels = U16(data + 12);
    const std::uint32_t bits = U16(data + 14);
    const std::uint32_t n = U32(data + 16);
    if (rate != 37800 && rate != 18900) return Fail(why, why_n, "XA clip: sample rate %u, only 37800 and 18900 are read", rate);
    if (channels != 1 && channels != 2) return Fail(why, why_n, "XA clip: %u channels, only 1 and 2 are read", channels);
    if (bits != 4) return Fail(why, why_n, "XA clip: %u bits a sample, only 4 is read", bits);
    if (n == 0) return Fail(why, why_n, "XA clip: no sound groups");
    const std::uint64_t want = kHeader + static_cast<std::uint64_t>(n) * kXaGroupBytes;
    if (want != size) {
        return Fail(why, why_n, "XA clip: %zu bytes do not hold exactly the %u sound groups its header says (24 + 128 x %u)",
                    size, n, n);
    }
    groups_ = data + kHeader;
    group_count_ = n;
    stereo_ = channels == 2;
    down_ = rate == 37800 ? 6 : 3;
    const std::uint64_t a_side = static_cast<std::uint64_t>(n) * (stereo_ ? kXaStereoSamples : kXaMonoSamples);
    total_ = ResampleOutputs(a_side, down_);
    Rewind();
    return true;
}

void XaClip::Rewind() {
    hist_.Reset();
    rs_[0].Reset(down_);
    rs_[1].Reset(down_);
    group_samples_ = 0;
    group_pos_ = 0;
    next_group_ = 0;
    made_ = 0;
    queued_ = 0;
    queue_pos_ = 0;
}

void XaClip::PushNext() {
    std::int16_t l = 0, r = 0;   // zeros once the groups are used up: the tail of the filter
    if (group_pos_ == group_samples_ && next_group_ < group_count_) {
        group_samples_ = XaDecodeGroup(groups_ + static_cast<std::size_t>(next_group_) * kXaGroupBytes, stereo_, hist_,
                                       left_, right_);
        group_pos_ = 0;
        ++next_group_;
    }
    if (group_pos_ < group_samples_) {
        l = left_[group_pos_];
        r = stereo_ ? right_[group_pos_] : l;
        ++group_pos_;
    }
    queued_ = rs_[0].Push(l, queue_[0]);
    if (stereo_) {
        rs_[1].Push(r, queue_[1]);
    } else {
        for (std::size_t i = 0; i < queued_; ++i) queue_[1][i] = queue_[0][i];
    }
    queue_pos_ = 0;
}

std::size_t XaClip::Read(std::int16_t* out, std::size_t frames) {
    std::size_t done = 0;
    while (done < frames && made_ < total_) {
        while (queue_pos_ == queued_) PushNext();
        out[2 * done] = queue_[0][queue_pos_];
        out[2 * done + 1] = queue_[1][queue_pos_];
        ++queue_pos_;
        ++made_;
        ++done;
    }
    return done;
}

}  // namespace media

// See cinepak.h.
#include "video/cinepak.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace video {
namespace {

std::uint32_t Be16(const std::uint8_t* p) { return (std::uint32_t{p[0]} << 8) | p[1]; }
std::uint32_t Be24(const std::uint8_t* p) { return (std::uint32_t{p[0]} << 16) | (std::uint32_t{p[1]} << 8) | p[2]; }
std::uint32_t Be32(const std::uint8_t* p) { return (Be16(p) << 16) | Be16(p + 2); }

std::uint32_t Clip(int v) { return v < 0 ? 0u : v > 255 ? 255u : static_cast<std::uint32_t>(v); }

std::uint32_t Pixel(int y, int u, int v) {
    return (Clip(y + 2 * v) << 16) | (Clip(y - u / 2 - v) << 8) | Clip(y + 2 * u);
}

void Fail(char* why, std::size_t why_n, const char* what) { std::snprintf(why, why_n, "%s", what); }

}  // namespace

Cinepak::Cinepak() = default;

Cinepak::~Cinepak() {
    std::free(frame_);
    std::free(strips_);
}

bool Cinepak::Init(std::uint32_t width, std::uint32_t height) {
    if (width == 0 || height == 0 || width > 4096 || height > 4096) return false;
    std::free(frame_);
    std::free(strips_);
    frame_ = static_cast<std::uint32_t*>(std::calloc(static_cast<std::size_t>(width) * height, sizeof(std::uint32_t)));
    strips_ = static_cast<Strip*>(std::calloc(kMaxStrips, sizeof(Strip)));
    if (!frame_ || !strips_) {
        std::free(frame_);
        std::free(strips_);
        frame_ = nullptr;
        strips_ = nullptr;
        return false;
    }
    width_ = width;
    height_ = height;
    return true;
}

bool Cinepak::Decode(const std::uint8_t* data, std::size_t size, char* why, std::size_t why_n) {
    if (!frame_) {
        Fail(why, why_n, "Decode before Init");
        return false;
    }
    if (size < 10) {
        Fail(why, why_n, "a frame shorter than its 10-byte header");
        return false;
    }
    const unsigned flags = data[0];
    const std::uint32_t length = Be24(data + 1);
    const std::uint32_t strips = Be16(data + 8);
    const std::uint8_t* end = data + (length < size && length >= 10 ? length : size);
    const std::uint8_t* p = data + 10;
    if (strips > kMaxStrips) {
        Fail(why, why_n, "more than 32 strips in a frame");
        return false;
    }
    std::uint32_t bottom = 0;
    for (unsigned i = 0; i < strips; ++i) {
        if (end - p < 12) {
            Fail(why, why_n, "a strip header past the frame's end");
            return false;
        }
        const unsigned id = p[0];
        std::uint32_t strip_size = Be24(p + 1);
        std::uint32_t top = Be16(p + 4);
        const std::uint32_t y1 = Be16(p + 8);
        if (top == 0) top = bottom;   // "under the previous strip"; the bottom word is then the height
        std::uint32_t strip_bottom = top + y1;
        if (strip_bottom > height_) strip_bottom = height_;
        if (id != 0x10 && id != 0x11) {
            std::snprintf(why, why_n, "strip %u has id 0x%02X, not 0x10 or 0x11", i, id);
            return false;
        }
        if (strip_size < 12) {
            Fail(why, why_n, "a strip whose size is under its 12-byte header");
            return false;
        }
        if (static_cast<std::size_t>(end - p) < strip_size) strip_size = static_cast<std::uint32_t>(end - p);
        if (i > 0 && !(flags & 1)) std::memcpy(&strips_[i], &strips_[i - 1], sizeof(Strip));
        if (!DecodeStrip(i, p + 12, strip_size - 12, top, strip_bottom, why, why_n)) return false;
        p += strip_size;
        bottom = strip_bottom;
    }
    return true;
}

bool Cinepak::DecodeStrip(unsigned index, const std::uint8_t* data, std::size_t size, std::uint32_t top, std::uint32_t bottom,
                          char* why, std::size_t why_n) {
    Strip& s = strips_[index];
    const std::uint8_t* end = data + size;
    for (const std::uint8_t* p = data; end - p >= 4;) {
        const unsigned id = p[0];
        std::uint32_t chunk_size = Be24(p + 1);
        if (chunk_size < 4) {
            Fail(why, why_n, "a chunk whose size is under its 4-byte header");
            return false;
        }
        if (static_cast<std::size_t>(end - p) < chunk_size) chunk_size = static_cast<std::uint32_t>(end - p);
        const std::uint8_t* body = p + 4;
        const std::size_t body_size = chunk_size - 4;
        switch (id) {
        case 0x20: case 0x21: case 0x24: case 0x25:
            ReadCodebook(id, s.v4, body, body_size);
            break;
        case 0x22: case 0x23: case 0x26: case 0x27:
            ReadCodebook(id, s.v1, body, body_size);
            break;
        case 0x30: case 0x31: case 0x32:
            return DecodeVectors(id, s, body, body_size, top, bottom, why, why_n);
        default:
            std::snprintf(why, why_n, "strip %u has a chunk of id 0x%02X", index, id);
            return false;
        }
        p += chunk_size;
    }
    return true;   // a strip of codebooks alone changes no pixel
}

// Codebook chunk ids: bit 0 partial, bit 2 greyscale. The chunk may end
// before 256 entries; the entries it has are taken.
void Cinepak::ReadCodebook(unsigned id, std::uint32_t (*book)[4], const std::uint8_t* data, std::size_t size) {
    const bool partial = id & 1, grey = id & 4;
    const std::size_t entry = grey ? 4 : 6;
    const std::uint8_t* p = data;
    const std::uint8_t* end = data + size;
    std::uint32_t mask = 0;
    for (unsigned i = 0; i < 256; ++i) {
        if (partial && (i & 31) == 0) {
            if (end - p < 4) return;
            mask = Be32(p);
            p += 4;
        }
        if (partial && !(mask & (0x80000000u >> (i & 31)))) continue;
        if (static_cast<std::size_t>(end - p) < entry) return;
        if (grey) {
            for (unsigned k = 0; k < 4; ++k) book[i][k] = p[k] * 0x010101u;
        } else {
            const int u = static_cast<std::int8_t>(p[4]), v = static_cast<std::int8_t>(p[5]);
            for (unsigned k = 0; k < 4; ++k) book[i][k] = Pixel(p[k], u, v);
        }
        p += entry;
    }
}

bool Cinepak::DecodeVectors(unsigned id, const Strip& s, const std::uint8_t* data, std::size_t size, std::uint32_t top,
                            std::uint32_t bottom, char* why, std::size_t why_n) {
    const bool inter = id == 0x31, all_v1 = id == 0x32;
    // The choice bits and the index bytes share one stream: a 32-bit
    // big-endian word of bits is read, from wherever the stream has reached,
    // whenever the previous word's bits are used up.
    const std::uint8_t* p = data;
    const std::uint8_t* end = data + size;
    std::uint32_t word = 0, mask = 0;
    bool short_bits = false;
    auto next_bit = [&]() -> bool {
        if (!mask) {
            if (end - p < 4) {
                short_bits = true;
                return false;
            }
            word = Be32(p);
            p += 4;
            mask = 0x80000000u;
        }
        const bool bit = (word & mask) != 0;
        mask >>= 1;
        return bit;
    };
    for (std::uint32_t y = top; y < bottom; y += 4) {
        for (std::uint32_t x = 0; x < width_; x += 4) {
            if (inter && !next_bit()) {
                if (short_bits) {
                    Fail(why, why_n, "the vectors chunk ends inside its choice bits");
                    return false;
                }
                continue;   // the previous frame's block stays
            }
            const bool v4 = all_v1 ? false : next_bit();
            if (short_bits) {
                Fail(why, why_n, "the vectors chunk ends inside its choice bits");
                return false;
            }
            const std::uint32_t w = width_ - x < 4 ? width_ - x : 4;
            const std::uint32_t h = height_ - y < 4 ? height_ - y : 4;
            if (v4) {
                if (end - p < 4) {
                    Fail(why, why_n, "the vectors chunk ends inside a V4 block's indices");
                    return false;
                }
                // Four entries: the top-left, top-right, bottom-left and
                // bottom-right 2 x 2 quarters, each entry's four pixels in
                // the same order within its quarter.
                for (unsigned q = 0; q < 4; ++q) {
                    const std::uint32_t* e = s.v4[p[q]];
                    const std::uint32_t qx = x + (q & 1) * 2, qy = y + (q >> 1) * 2;
                    for (unsigned k = 0; k < 4; ++k) {
                        const std::uint32_t px = qx + (k & 1), py = qy + (k >> 1);
                        if (px < x + w && py < y + h) frame_[py * width_ + px] = e[k];
                    }
                }
                p += 4;
            } else {
                if (end - p < 1) {
                    Fail(why, why_n, "the vectors chunk ends inside a V1 block's index");
                    return false;
                }
                // One entry over the block: each of its four pixels doubled
                // into a 2 x 2 quarter.
                const std::uint32_t* e = s.v1[p[0]];
                for (std::uint32_t dy = 0; dy < h; ++dy)
                    for (std::uint32_t dx = 0; dx < w; ++dx) frame_[(y + dy) * width_ + x + dx] = e[(dy >> 1) * 2 + (dx >> 1)];
                p += 1;
            }
        }
    }
    return true;
}

}  // namespace video

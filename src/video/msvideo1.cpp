// See msvideo1.h.
#include "video/msvideo1.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace video {
namespace {

std::uint32_t Widen(std::uint32_t rgb555) {
    const std::uint32_t r = (rgb555 >> 10) & 31, g = (rgb555 >> 5) & 31, b = rgb555 & 31;
    return (((r << 3) | (r >> 2)) << 16) | (((g << 3) | (g >> 2)) << 8) | ((b << 3) | (b >> 2));
}

}  // namespace

MsVideo1::MsVideo1() = default;

MsVideo1::~MsVideo1() {
    std::free(frame_);
    std::free(raw_);
}

bool MsVideo1::Init(std::uint32_t width, std::uint32_t height, std::uint32_t bits, const std::uint32_t* palette,
                    std::uint32_t palette_n) {
    if (width == 0 || height == 0 || width > 4096 || height > 4096) return false;
    if (bits != 8 && bits != 16) return false;
    std::free(frame_);
    std::free(raw_);
    const std::size_t n = static_cast<std::size_t>(width) * height;
    frame_ = static_cast<std::uint32_t*>(std::calloc(n, sizeof(std::uint32_t)));
    raw_ = static_cast<std::uint16_t*>(std::calloc(n, sizeof(std::uint16_t)));
    if (!frame_ || !raw_) {
        std::free(frame_);
        std::free(raw_);
        frame_ = nullptr;
        raw_ = nullptr;
        return false;
    }
    width_ = width;
    height_ = height;
    bits_ = bits;
    std::memset(palette_, 0, sizeof palette_);
    if (palette && palette_n) std::memcpy(palette_, palette, (palette_n > 256 ? 256 : palette_n) * sizeof(std::uint32_t));
    return true;
}

// One pixel at DIB coordinates (x from the left, dib_y from the bottom).
void MsVideo1::Put(std::uint32_t x, std::uint32_t dib_y, std::uint32_t value) {
    if (x >= width_ || dib_y >= height_) return;
    const std::size_t at = static_cast<std::size_t>(height_ - 1 - dib_y) * width_ + x;
    raw_[at] = static_cast<std::uint16_t>(value);
    frame_[at] = bits_ == 8 ? palette_[value & 0xFF] : Widen(value);
}

bool MsVideo1::Decode(const std::uint8_t* data, std::size_t size, char* why, std::size_t why_n) {
    if (!frame_) {
        std::snprintf(why, why_n, "Decode before Init");
        return false;
    }
    const std::uint8_t* p = data;
    const std::uint8_t* end = data + size;
    const std::uint32_t blocks_wide = (width_ + 3) / 4, blocks_high = (height_ + 3) / 4;
    std::uint32_t total = blocks_wide * blocks_high;
    std::uint32_t skip = 0;
    auto word = [&](std::uint32_t* out) -> bool {
        if (end - p < 2) return false;
        *out = p[0] | (std::uint32_t{p[1]} << 8);
        p += 2;
        return true;
    };
    for (std::uint32_t by = 0; by < blocks_high; ++by) {
        for (std::uint32_t bx = 0; bx < blocks_wide; ++bx, --total) {
            if (skip) {
                --skip;
                continue;
            }
            if (end - p < 2) return true;   // the frame ends early: the rest keeps the previous frame
            const std::uint32_t a = p[0], b = p[1];
            p += 2;
            if (a == 0 && b == 0 && total == 0) return true;
            const std::uint32_t x0 = bx * 4, y0 = by * 4;
            if ((b & 0xFC) == 0x84) {
                skip = ((b - 0x84) << 8) + a;   // this block and skip - 1 more
                if (skip) --skip;
                continue;
            }
            std::uint32_t flags = (b << 8) | a;
            std::uint32_t colours[8];
            unsigned n_colours;
            if (b < 0x80) {
                if (bits_ == 8) {
                    if (end - p < 2) goto short_block;
                    colours[0] = p[0];
                    colours[1] = p[1];
                    p += 2;
                } else {
                    if (!word(&colours[0]) || !word(&colours[1])) goto short_block;
                }
                n_colours = 2;
                if (bits_ == 16 && (colours[0] & 0x8000)) {
                    for (unsigned k = 2; k < 8; ++k)
                        if (!word(&colours[k])) goto short_block;
                    n_colours = 8;
                }
            } else if (bits_ == 8 && b >= 0x90) {
                if (end - p < 8) goto short_block;
                for (unsigned k = 0; k < 8; ++k) colours[k] = p[k];
                p += 8;
                n_colours = 8;
            } else {
                // one colour: the index a (8-bit form) or the word itself (16-bit)
                const std::uint32_t c = bits_ == 8 ? a : flags;
                for (std::uint32_t dy = 0; dy < 4; ++dy)
                    for (std::uint32_t dx = 0; dx < 4; ++dx) Put(x0 + dx, y0 + dy, c);
                continue;
            }
            for (std::uint32_t dy = 0; dy < 4; ++dy) {
                for (std::uint32_t dx = 0; dx < 4; ++dx, flags >>= 1) {
                    const unsigned pair = n_colours == 8 ? ((dy & 2) << 1) + (dx & 2) : 0;
                    Put(x0 + dx, y0 + dy, colours[pair + ((flags & 1) ^ 1)]);
                }
            }
        }
    }
    return true;
short_block:
    std::snprintf(why, why_n, "the frame ends inside a block's colours");
    return false;
}

}  // namespace video

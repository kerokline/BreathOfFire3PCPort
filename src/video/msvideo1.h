// A Microsoft Video 1 ("MSVC" / "CRAM") decoder, written from the public
// description of the format (Microsoft's "Video 1" compression notes and the
// MultimediaWiki's "Microsoft Video 1" page; DIV-0205,
// docs/replacing-mci.md "As built"). No vendored code.
//
// The format, as implemented:
//   The picture is a bottom-up DIB of 4 x 4 blocks, coded left to right from
//   the bottom row of blocks up; within a block the pixels go left to right
//   from its bottom row up. Every block starts with a 16-bit little-endian
//   word (bytes a, b). In both forms b in 0x84..0x87 is a skip code: this
//   block and the next ((b - 0x84) * 256 + a - 1) keep the previous frame's
//   pixels. A word of 0 after the last block ends the frame; a frame that
//   ends early keeps the rest.
//   The 8-bit form (palettised): b under 0x80 - the word is 16 mask bits,
//   two palette indices follow, each pixel takes the first where its mask
//   bit (bit 0 the first pixel) is set, else the second; b of 0x90 or over -
//   the word is 16 mask bits, eight indices follow, a pair a 2 x 2 quarter
//   (the first pair the bottom-left quarter in DIB order, then bottom-right,
//   top-left, top-right), each pixel choosing within its quarter's pair as
//   above; else (b in 0x80..0x8F outside the skip range) a one-colour block
//   of index a.
//   The 16-bit form (RGB 5-5-5): b under 0x80 - 16 mask bits and two
//   colours follow; when the first colour has bit 15 set the block has eight
//   colours (six more words), a pair a quarter as in the 8-bit form, else
//   the two colours over the block; b of 0x80 or over outside the skip range
//   - the word itself is the block's one colour.
#pragma once

#include <cstddef>
#include <cstdint>

namespace video {

class MsVideo1 {
public:
    MsVideo1();
    ~MsVideo1();
    MsVideo1(const MsVideo1&) = delete;
    MsVideo1& operator=(const MsVideo1&) = delete;

    // Allocates the frame. `bits` is 8 (with the stream's palette, up to 256
    // entries of 0x00RRGGBB) or 16. False when out of memory, on another
    // bit depth, or on a size of 0 or over 4096 a side.
    bool Init(std::uint32_t width, std::uint32_t height, std::uint32_t bits, const std::uint32_t* palette, std::uint32_t palette_n);
    // Decodes one frame's chunk over the previous frame. False, with the
    // reason in `why`, when the chunk ends inside a block's data.
    bool Decode(const std::uint8_t* data, std::size_t size, char* why, std::size_t why_n);
    // The frame: width x height pixels of 0x00RRGGBB, rows top-down. A
    // 16-bit colour's channels are widened to 8 bits as (c << 3) | (c >> 2).
    const std::uint32_t* Frame() const { return frame_; }
    // The frame's pixels as decoded, before the widening: palette indices
    // (8-bit form) or RGB 5-5-5 words (16-bit form), rows top-down. For a
    // comparison with a reference decode in the stream's own depth.
    const std::uint16_t* Raw() const { return raw_; }
    std::uint32_t Width() const { return width_; }
    std::uint32_t Height() const { return height_; }

private:
    void Put(std::uint32_t x, std::uint32_t dib_y, std::uint32_t value);

    std::uint32_t width_ = 0, height_ = 0, bits_ = 0;
    std::uint32_t* frame_ = nullptr;
    std::uint16_t* raw_ = nullptr;
    std::uint32_t palette_[256] = {};
};

}  // namespace video

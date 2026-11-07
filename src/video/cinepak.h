// A Cinepak ("cvid") decoder, written from the public description of the
// format (the MultimediaWiki's "Cinepak" page and Apple's QuickTime notes of
// the codec; DIV-0205, docs/replacing-mci.md "As built"). No vendored code.
//
// The format, as implemented:
//   A frame is a 10-byte header - flags, a 24-bit length, width, height, the
//   strip count (big-endian from the length on) - then its strips. A strip
//   is a 12-byte header - id 0x10 (intra) or 0x11 (inter), a 24-bit size
//   that includes the header, then the strip's top, left, bottom and right
//   as 16-bit words - and chunks: a one-byte id, a 24-bit size that includes
//   the 4-byte chunk header, then the chunk. A strip's top of 0 means "under
//   the previous strip"; its bottom word is then its height.
//   Chunks 0x20..0x27 carry the two codebooks of 256 entries: bit 1 of the id
//   picks V1 (set) or V4 (clear), bit 2 greyscale (4 bytes an entry: four
//   luma values) or colour (6 bytes: four luma, then U and V as signed
//   bytes), bit 0 a partial update (a 32-bit big-endian mask before each
//   group of 32 entries, a set bit meaning that entry follows) or a full one.
//   Each entry is a 2 x 2 patch; a V1 entry covers a 4 x 4 block with its
//   luma values doubled, a V4 block takes four entries, one a 2 x 2 quarter.
//   Chunks 0x30 (intra), 0x31 (inter) and 0x32 (all V1) code the blocks in
//   raster order, 4 x 4 each, their choices as a stream of 32-bit big-endian
//   words read from the top bit down: intra - one bit a block, clear V1 (one
//   codebook index) or set V4 (four); inter - one bit a block, clear "keep
//   the previous frame's block", set then another bit as intra's; all-V1 -
//   no bits, one index a block.
//   Codebooks persist per strip index from frame to frame; within a frame a
//   strip after the first starts from the previous strip's books unless bit
//   0 of the frame's flags is set. A colour entry's pixels: R = Y + 2V,
//   G = Y - U/2 - V, B = Y + 2U, each clipped to 0..255.
#pragma once

#include <cstddef>
#include <cstdint>

namespace video {

class Cinepak {
public:
    Cinepak();
    ~Cinepak();
    Cinepak(const Cinepak&) = delete;
    Cinepak& operator=(const Cinepak&) = delete;

    // Allocates the frame for a width x height picture. False when out of
    // memory or on a size of 0 or over 4096 a side.
    bool Init(std::uint32_t width, std::uint32_t height);
    // Decodes one frame's chunk into the frame, over the previous one (an
    // inter frame keeps the blocks it does not code). False, with the reason
    // in `why`, on a chunk the description does not cover or that ends
    // early; the frame then holds what was decoded before the fault.
    bool Decode(const std::uint8_t* data, std::size_t size, char* why, std::size_t why_n);
    // The frame: width x height pixels of 0x00RRGGBB, rows top-down.
    const std::uint32_t* Frame() const { return frame_; }
    std::uint32_t Width() const { return width_; }
    std::uint32_t Height() const { return height_; }

private:
    struct Strip {
        std::uint32_t v4[256][4];   // each entry's four pixels, as 0x00RRGGBB
        std::uint32_t v1[256][4];
    };
    static constexpr unsigned kMaxStrips = 32;

    bool DecodeStrip(unsigned index, const std::uint8_t* data, std::size_t size, std::uint32_t top, std::uint32_t bottom,
                     char* why, std::size_t why_n);
    void ReadCodebook(unsigned id, std::uint32_t (*book)[4], const std::uint8_t* data, std::size_t size);
    bool DecodeVectors(unsigned id, const Strip& s, const std::uint8_t* data, std::size_t size, std::uint32_t top,
                       std::uint32_t bottom, char* why, std::size_t why_n);

    std::uint32_t width_ = 0, height_ = 0;
    std::uint32_t* frame_ = nullptr;
    Strip* strips_ = nullptr;   // kMaxStrips of them
};

}  // namespace video

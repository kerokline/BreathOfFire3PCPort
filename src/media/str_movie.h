// The PlayStation STR v2 movie (docs/psx-media-round.md 2.1): raw 2,352-byte
// CD sectors, a BS v2 picture in the Form 1 video sectors and XA ADPCM in the
// Form 2 audio sectors.
//
// Sources: psx-spx ("CDROM File Video STR Streaming and BS Picture
// Compression", "MDEC") for the bitstream and the decoder, the owner's
// psx-machine crate (MIT, player/psx-machine/src/mdec.rs) for the MDEC
// dequantisation and inverse DCT, whose steps are ported here, and the bof3
// wiki's formats/str.md for CAPCOM30's chunk header.
//
// A video sector's data (at +24 of the raw sector) opens with a 32-byte
// header: 0x80010160, chunk number u16, chunk count u16, frame number u32,
// bytes used in the frame u32, width u16, height u16, then the BS header
// (MDEC word count u16, id 0x3800 u16, quantiser scale u16, version u16) and
// four zero bytes; 2,016 bytes of the frame follow. The frame is its chunks
// joined, cut to the bytes used; it opens with the BS header again, then the
// bitstream: 16-bit little-endian words read from the top bit, macroblocks in
// column order, each six blocks (Cr, Cb, Y0..Y3 of a 2 x 2 square), a block a
// 10-bit signed DC (v2) followed by AC codes of MPEG-1 table B.14. The BS
// escape is 000001, then run (6 bits) and level (10 bits, signed). The end of
// block code is 10. Coefficients are dequantised with the STR default table,
// inverse-DCT'd and converted to RGB as the MDEC does (unsigned output).
// The IDCT is psx-spx's reference one, so a pixel can differ from the
// console's by a step or two.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace media {

// A size that is a multiple of 2,352 and the first sector's CD sync.
bool IsStr(const std::uint8_t* data, std::size_t size);

class StrMovie {
public:
    // Scans every sector. Refuses, with the reason in `why`: a sector without
    // the sync; a video header with a version other than 2 or an id other than
    // 0x3800; a picture size that changes, is not a multiple of 16 or is above
    // 2,048; a frame that lacks a chunk; audio that is not four-bit or that
    // changes its coding; a file with no video. Decodes the audio whole.
    // `data` stays the caller's and must outlive the movie.
    bool Open(const std::uint8_t* data, std::size_t size, char* why, std::size_t why_n);

    // The doubled picture: twice the stream's width and height (640 x 480).
    std::uint32_t Width() const { return width_ * 2; }
    std::uint32_t Height() const { return height_ * 2; }
    std::uint32_t FrameCount() const { return static_cast<std::uint32_t>(frames_.size()); }
    // Seconds a frame: total sectors / (150 * frames). The disc reads 150
    // sectors a second.
    double FrameSeconds() const { return frame_seconds_; }
    std::uint32_t FramesDecoded() const { return decoded_; }
    // Decodes the next frame. False at the end (`why` empty) or on a fault in
    // the bitstream (`why` says what; the picture is then half made).
    bool NextFrame(char* why, std::size_t why_n);
    // The last frame, doubled: Width() x Height() of 0x00RRGGBB, rows top-down.
    const std::uint32_t* Frame() const { return doubled_.data(); }
    // The same without the doubling: the stream's width x height.
    const std::uint32_t* RawFrame320() const { return raw_.data(); }

    bool HasAudio() const { return audio_rate_ != 0; }
    std::uint32_t AudioRate() const { return audio_rate_; }
    unsigned AudioChannels() const { return audio_channels_; }
    // The whole soundtrack, interleaved, at the stream's own rate.
    const std::int16_t* Audio() const { return audio_.data(); }
    std::size_t AudioFrames() const { return audio_channels_ ? audio_.size() / audio_channels_ : 0; }

private:
    struct FrameRef {
        std::uint32_t chunks;               // chunks in the frame
        std::uint32_t sector[64];           // raw sector index of each chunk
        std::uint32_t used;                 // bytes used in the frame
        std::uint32_t qscale;
    };
    bool DecodeFrame(const FrameRef& f, char* why, std::size_t why_n);

    const std::uint8_t* data_ = nullptr;
    std::vector<FrameRef> frames_;
    std::uint32_t width_ = 0, height_ = 0;
    double frame_seconds_ = 0;
    std::uint32_t decoded_ = 0;
    std::vector<std::uint8_t> bits_;
    std::vector<std::uint32_t> raw_, doubled_;
    std::uint32_t audio_rate_ = 0;
    unsigned audio_channels_ = 0;
    std::vector<std::int16_t> audio_;
};

}  // namespace media

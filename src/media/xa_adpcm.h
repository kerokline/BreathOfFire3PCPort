// CD-ROM XA ADPCM, four bits a sample (psx-spx, "CDROM XA Audio ADPCM
// Compression"; the same decode as tools/xa.py's `decode`, which the importer
// holds the port's clips to, and the psx-formats crate of frost's
// (raulcorreia7) bof3-player, MIT: docs/THIRD_PARTY.md section 5).
//
// A sound group is 128 bytes: 16 parameter bytes, then 28 four-byte words.
// Units 0..7 take their parameter from bytes 4..11 (the other eight are
// copies): the shift in the low nibble, the filter (0..3) in bits 4..5. Sample
// j of unit u is the low nibble (even u) or high nibble (odd u) of byte u / 2
// of word j. A mono group plays its units one after another (8 x 28 = 224
// samples); a stereo group gives the even units to the left side and the odd
// to the right (4 x 28 = 112 samples a side). A sample is the nibble, signed,
// put in the top of a 16-bit word and shifted right by `shift`, plus
// (s1 * k0 + s2 * k1) >> 6 (an arithmetic shift: floored) from the side's last
// two samples, clamped to 16 bits. The filter pairs are the SPU's first four.
// The shift is used as given, up to 15.
//
// Eight-bit groups are a different layout; the callers refuse them.
#pragma once

#include <cstddef>
#include <cstdint>

namespace media {

constexpr std::size_t kXaGroupBytes = 128;
constexpr std::size_t kXaMonoSamples = 224;     // a mono group's samples
constexpr std::size_t kXaStereoSamples = 112;   // a stereo group's samples a side

// Each side's last two samples. A clip starts from zeros.
struct XaHistory {
    std::int32_t s1[2] = {0, 0};
    std::int32_t s2[2] = {0, 0};
    void Reset() { s1[0] = s1[1] = s2[0] = s2[1] = 0; }
};

// Decodes one 128-byte group, advancing `h`. Mono: 224 samples into `left`
// (`right` is not touched and may be null). Stereo: 112 samples into each.
// Returns the samples a side.
std::size_t XaDecodeGroup(const std::uint8_t* group, bool stereo, XaHistory& h, std::int16_t* left,
                          std::int16_t* right);

}  // namespace media

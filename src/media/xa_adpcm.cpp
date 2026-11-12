#include "media/xa_adpcm.h"

namespace media {

namespace {
// The SPU's first four filter pairs.
constexpr int kFilter[4][2] = {{0, 0}, {60, 0}, {115, -52}, {98, -55}};
}  // namespace

std::size_t XaDecodeGroup(const std::uint8_t* group, bool stereo, XaHistory& h, std::int16_t* left,
                          std::int16_t* right) {
    std::size_t fill[2] = {0, 0};
    for (int u = 0; u < 8; ++u) {
        const int side = stereo ? (u & 1) : 0;
        std::int16_t* dst = (side == 0) ? left : right;
        std::int32_t s1 = h.s1[side];
        std::int32_t s2 = h.s2[side];
        const int param = group[4 + u];
        const int shift = param & 0x0F;
        const int k0 = kFilter[(param >> 4) & 3][0];
        const int k1 = kFilter[(param >> 4) & 3][1];
        const int high = u & 1;
        const std::uint8_t* at = group + 16 + (u >> 1);
        for (int j = 0; j < 28; ++j) {
            const int b = at[4 * j];
            int n = high ? (b >> 4) : (b & 0x0F);
            if (n & 8) n -= 16;
            std::int32_t v = ((n * 4096) >> shift) + ((s1 * k0 + s2 * k1) >> 6);
            v = v < -32768 ? -32768 : (v > 32767 ? 32767 : v);
            dst[fill[side]++] = static_cast<std::int16_t>(v);
            s2 = s1;
            s1 = v;
        }
        h.s1[side] = s1;
        h.s2[side] = s2;
    }
    return fill[0];
}

}  // namespace media

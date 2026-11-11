#include "media/str_movie.h"

#include <cstdlib>
#include <cstring>

#include "media/why.h"
#include "media/xa_adpcm.h"

namespace media {

namespace {

constexpr std::size_t kSector = 2352;
constexpr std::size_t kData = 24;            // user data starts here in a raw sector
constexpr std::size_t kVideoHeader = 32;
constexpr std::size_t kChunkBytes = 2048 - kVideoHeader;
constexpr std::uint32_t kMagic = 0x80010160u;
constexpr std::uint32_t kMaxChunks = 64;
constexpr std::uint32_t kMaxSide = 2048;

const std::uint8_t kSync[12] = {0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00};

std::uint32_t U32(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(p[0]) | static_cast<std::uint32_t>(p[1]) << 8 |
           static_cast<std::uint32_t>(p[2]) << 16 | static_cast<std::uint32_t>(p[3]) << 24;
}
std::uint32_t U16(const std::uint8_t* p) { return static_cast<std::uint32_t>(p[0]) | static_cast<std::uint32_t>(p[1]) << 8; }

// ---- the bitstream's codes -------------------------------------------------

// MPEG-1 table B.14, the AC codes without their sign bit (a sign follows each).
// The end of block (10) and the escape (000001) are handled apart.
struct VlcRow {
    const char* code;
    int run;
    int level;
};
const VlcRow kVlc[] = {
    {"11", 0, 1}, {"011", 1, 1}, {"0100", 0, 2},
    {"0101", 2, 1}, {"00101", 0, 3}, {"00111", 3, 1},
    {"00110", 4, 1}, {"000110", 1, 2}, {"000111", 5, 1},
    {"000101", 6, 1}, {"000100", 7, 1}, {"0000110", 0, 4},
    {"0000100", 2, 2}, {"0000111", 8, 1}, {"0000101", 9, 1},
    {"00100110", 0, 5}, {"00100001", 0, 6}, {"00100101", 1, 3},
    {"00100100", 3, 2}, {"00100111", 10, 1}, {"00100011", 11, 1},
    {"00100010", 12, 1}, {"00100000", 13, 1}, {"0000001010", 0, 7},
    {"0000001100", 1, 4}, {"0000001011", 2, 3}, {"0000001111", 4, 2},
    {"0000001001", 5, 2}, {"0000001110", 14, 1}, {"0000001101", 15, 1},
    {"0000001000", 16, 1}, {"000000011101", 0, 8}, {"000000011000", 0, 9},
    {"000000010011", 0, 10}, {"000000010000", 0, 11}, {"000000011011", 1, 5},
    {"000000010100", 2, 4}, {"000000011100", 3, 3}, {"000000010010", 4, 3},
    {"000000011110", 6, 2}, {"000000010101", 7, 2}, {"000000010001", 8, 2},
    {"000000011111", 17, 1}, {"000000011010", 18, 1}, {"000000011001", 19, 1},
    {"000000010111", 20, 1}, {"000000010110", 21, 1}, {"0000000011010", 0, 12},
    {"0000000011001", 0, 13}, {"0000000011000", 0, 14}, {"0000000010111", 0, 15},
    {"0000000010110", 1, 6}, {"0000000010101", 1, 7}, {"0000000010100", 2, 5},
    {"0000000010011", 3, 4}, {"0000000010010", 5, 3}, {"0000000010001", 9, 2},
    {"0000000010000", 10, 2}, {"0000000011111", 22, 1}, {"0000000011110", 23, 1},
    {"0000000011101", 24, 1}, {"0000000011100", 25, 1}, {"0000000011011", 26, 1},
    {"00000000011111", 0, 16}, {"00000000011110", 0, 17}, {"00000000011101", 0, 18},
    {"00000000011100", 0, 19}, {"00000000011011", 0, 20}, {"00000000011010", 0, 21},
    {"00000000011001", 0, 22}, {"00000000011000", 0, 23}, {"00000000010111", 0, 24},
    {"00000000010110", 0, 25}, {"00000000010101", 0, 26}, {"00000000010100", 0, 27},
    {"00000000010011", 0, 28}, {"00000000010010", 0, 29}, {"00000000010001", 0, 30},
    {"00000000010000", 0, 31}, {"000000000011000", 0, 32}, {"000000000010111", 0, 33},
    {"000000000010110", 0, 34}, {"000000000010101", 0, 35}, {"000000000010100", 0, 36},
    {"000000000010011", 0, 37}, {"000000000010010", 0, 38}, {"000000000010001", 0, 39},
    {"000000000010000", 0, 40}, {"000000000011111", 1, 8}, {"000000000011110", 1, 9},
    {"000000000011101", 1, 10}, {"000000000011100", 1, 11}, {"000000000011011", 1, 12},
    {"000000000011010", 1, 13}, {"000000000011001", 1, 14}, {"0000000000010011", 1, 15},
    {"0000000000010010", 1, 16}, {"0000000000010001", 1, 17}, {"0000000000010000", 1, 18},
    {"0000000000010100", 6, 3}, {"0000000000011010", 11, 2}, {"0000000000011001", 12, 2},
    {"0000000000011000", 13, 2}, {"0000000000010111", 14, 2}, {"0000000000010110", 15, 2},
    {"0000000000010101", 16, 2}, {"0000000000011111", 27, 1}, {"0000000000011110", 28, 1},
    {"0000000000011101", 29, 1}, {"0000000000011100", 30, 1}, {"0000000000011011", 31, 1},
};

struct VlcEntry {
    std::uint8_t len;   // 0: no such code
    std::uint8_t run;
    std::uint8_t level;
};

// Indexed by the next 16 bits. Built once; a table that is not a prefix code
// aborts (the rows are constants, so that is a bug here, not a bad file).
const VlcEntry* VlcTable() {
    static VlcEntry* table = [] {
        VlcEntry* t = static_cast<VlcEntry*>(std::calloc(65536, sizeof(VlcEntry)));
        if (t == nullptr) std::abort();
        for (const VlcRow& row : kVlc) {
            const unsigned len = static_cast<unsigned>(std::strlen(row.code));
            unsigned code = 0;
            for (unsigned i = 0; i < len; ++i) code = code << 1 | static_cast<unsigned>(row.code[i] - '0');
            const unsigned first = code << (16 - len);
            for (unsigned i = 0; i < (1u << (16 - len)); ++i) {
                if (t[first + i].len != 0) std::abort();
                t[first + i] = {static_cast<std::uint8_t>(len), static_cast<std::uint8_t>(row.run),
                                static_cast<std::uint8_t>(row.level)};
            }
        }
        return t;
    }();
    return table;
}

// 16-bit little-endian words, read from the top bit.
class BitReader {
public:
    BitReader(const std::uint8_t* p, std::size_t n) : p_(p), words_(n / 2) {}
    // The next 16 bits without taking them (zeros past the end).
    std::uint32_t Peek16() const {
        const std::size_t w = pos_ >> 4;
        const unsigned off = static_cast<unsigned>(pos_ & 15);
        std::size_t i = w;
        const std::uint32_t hi = Word(i);
        ++i;
        const std::uint32_t v = hi << 16 | Word(i);
        return (v << off) >> 16;
    }
    std::uint32_t Take(unsigned n) {   // n <= 16
        const std::uint32_t v = Peek16() >> (16 - n);
        pos_ += n;
        return v;
    }
    void Skip(unsigned n) { pos_ += n; }
    bool Overrun() const { return pos_ > words_ * 16; }

private:
    std::uint32_t Word(std::size_t i) const {
        return i < words_ ? (static_cast<std::uint32_t>(p_[2 * i]) | static_cast<std::uint32_t>(p_[2 * i + 1]) << 8) : 0;
    }
    const std::uint8_t* p_;
    std::size_t words_;
    std::size_t pos_ = 0;
};

// ---- the MDEC ---------------------------------------------------------------

// Raster position of each zigzag index.
const int kZagZig[64] = {0,  1,  8,  16, 9,  2,  3,  10, 17, 24, 32, 25, 18, 11, 4,  5,  12, 19, 26, 33, 40, 48,
                         41, 34, 27, 20, 13, 6,  7,  14, 21, 28, 35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23,
                         30, 37, 44, 51, 58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63};

// The quantisation table STR playback loads (psx-spx: the MPEG-1 intra
// matrix with the DC term 2), in raster order.
const std::uint8_t kQuantRaster[64] = {
    2,  16, 19, 22, 26, 27, 29, 34, 16, 16, 22, 24, 27, 29, 34, 37, 19, 22, 26, 27, 29, 34, 34, 38, 22, 22,
    26, 27, 29, 34, 37, 40, 22, 26, 27, 29, 32, 35, 40, 48, 26, 27, 29, 32, 35, 40, 48, 58, 26, 27, 29, 34,
    38, 46, 56, 69, 27, 29, 35, 38, 46, 56, 69, 83};

// The same in the zigzag order the stream's coefficients come in: coefficient
// k is multiplied by Quant()[k].
const std::uint8_t* Quant() {
    static const struct Zz {
        std::uint8_t v[64];
        Zz() {
            for (int k = 0; k < 64; ++k) v[k] = kQuantRaster[kZagZig[k]];
        }
    } zz;
    return zz.v;
}

// The MDEC's scale table (psx-spx "MDEC Decompression"): the cosine basis,
// row z the frequency, column x the position, row 0 at 1/sqrt 2, in 1.15
// fixed point, each value floored.
const std::int16_t kScale[64] = {
    23170, 23170,  23170,  23170,  23170,  23170,  23170,  23170,  32138,  27245,  18204,  6392,   -6393,
    -18205, -27246, -32139, 30273, 12539,  -12540, -30274, -30274, -12540, 12539,  30273,  27245,  -6393,
    -32139, -18205, 18204,  32138, 6392,   -27246, 23170,  -23171, -23171, 23170,  23170,  -23171, -23171,
    23170,  18204,  -32139, 6392,  27245,  -27246, -6393,  32138,  -18205, 12539,  -30274, 30273,  -12540,
    -12540, 30273,  -30274, 12539, 6392,   -18205, 27245,  -32139, 32138,  -27246, 18204,  -6393};

std::int32_t Signed10(std::uint32_t v) { return static_cast<std::int32_t>(v & 0x3FF) - static_cast<std::int32_t>((v & 0x200) << 1); }

// The inverse DCT of psx-spx: two passes over the scale table, each rounded
// to 13 fraction bits.
void Idct(const std::int32_t* block, std::int32_t* out) {
    std::int32_t a[64], b[64];
    std::memcpy(a, block, sizeof(a));
    std::int32_t* source = a;
    std::int32_t* target = b;
    for (int pass = 0; pass < 2; ++pass) {
        for (int x = 0; x < 8; ++x) {
            for (int y = 0; y < 8; ++y) {
                std::int64_t sum = 0;
                for (int z = 0; z < 8; ++z) sum += static_cast<std::int64_t>(source[y + z * 8]) * (kScale[x + z * 8] >> 3);
                target[x + y * 8] = static_cast<std::int32_t>((sum + 0xFFF) >> 13);
            }
        }
        std::int32_t* t = source;
        source = target;
        target = t;
    }
    std::memcpy(out, source, 64 * sizeof(std::int32_t));
}

// One block from the bitstream: the DC, the AC codes up to the end of block,
// dequantised into the raster order, then inverse-DCT'd. False when the
// bitstream is bad (`why` says how).
bool Block(BitReader& br, std::uint32_t qscale, std::int32_t* out, char* why, std::size_t why_n) {
    const VlcEntry* vlc = VlcTable();
    const std::uint8_t* quant = Quant();
    std::int32_t coef[64] = {};
    auto clamp = [](std::int32_t v) { return v < -0x400 ? -0x400 : (v > 0x3FF ? 0x3FF : v); };
    const std::int32_t dc = Signed10(br.Take(10));
    coef[0] = clamp(qscale == 0 ? dc * 2 : dc * quant[0]);
    int k = 0;
    for (;;) {
        const std::uint32_t peek = br.Peek16();
        int run, level;
        if ((peek >> 14) == 2) {            // 10: end of block
            br.Skip(2);
            break;
        }
        if ((peek >> 10) == 1) {            // 000001: the escape
            br.Skip(6);
            run = static_cast<int>(br.Take(6));
            level = Signed10(br.Take(10));
        } else {
            const VlcEntry e = vlc[peek];
            if (e.len == 0) return Fail(why, why_n, "bad AC code %04X", static_cast<unsigned>(peek));
            br.Skip(e.len);
            run = e.run;
            level = br.Take(1) ? -e.level : e.level;
        }
        if (br.Overrun()) return Fail(why, why_n, "the bitstream ends inside a block");
        k += run + 1;
        if (k > 63) continue;               // past the block: read to the end code, keep nothing
        if (qscale == 0) {
            coef[k] = clamp(level * 2);
        } else {
            coef[kZagZig[k]] = clamp((level * static_cast<std::int32_t>(quant[k]) * static_cast<std::int32_t>(qscale) + 4) >> 3);
        }
    }
    if (br.Overrun()) return Fail(why, why_n, "the bitstream ends inside a block");
    Idct(coef, out);
    return true;
}

// Luma plus a chroma term, to the unsigned byte the MDEC gives.
std::uint32_t Channel(std::int32_t luma, std::int32_t term) {
    std::int32_t v = luma + term;
    v = v < -128 ? -128 : (v > 127 ? 127 : v);
    return static_cast<std::uint32_t>(v + 128);
}

// One 8 x 8 luma block at (x0, y0) of the macroblock, converted with the
// macroblock's chroma. The factors are the MDEC's 1.402, 0.3437 / 0.7143 and
// 1.772, the product cut toward zero, in integers.
void Colour(std::uint32_t* pixels, std::uint32_t stride, const std::int32_t* y, const std::int32_t* cr,
            const std::int32_t* cb, int x0, int y0) {
    for (int row = 0; row < 8; ++row) {
        for (int col = 0; col < 8; ++col) {
            const int c = (x0 + col) / 2 + ((y0 + row) / 2) * 8;
            const std::int32_t r = cr[c], b = cb[c];
            const std::int32_t rt = (1402 * r) / 1000;
            const std::int32_t gt = -(3437 * b + 7143 * r) / 10000;
            const std::int32_t bt = (1772 * b) / 1000;
            const std::int32_t l = y[col + row * 8];
            pixels[static_cast<std::size_t>(y0 + row) * stride + static_cast<std::size_t>(x0 + col)] =
                Channel(l, rt) << 16 | Channel(l, gt) << 8 | Channel(l, bt);
        }
    }
}

}  // namespace

bool IsStr(const std::uint8_t* data, std::size_t size) {
    return data != nullptr && size >= kSector && size % kSector == 0 && std::memcmp(data, kSync, sizeof(kSync)) == 0;
}

bool StrMovie::Open(const std::uint8_t* data, std::size_t size, char* why, std::size_t why_n) {
    frames_.clear();
    width_ = height_ = 0;
    decoded_ = 0;
    audio_rate_ = 0;
    audio_channels_ = 0;
    audio_.clear();
    data_ = nullptr;
    if (data == nullptr || size < kSector || size % kSector != 0) {
        return Fail(why, why_n, "STR: %zu bytes is not a whole number of 2352-byte sectors", size);
    }
    const std::size_t sectors = size / kSector;
    std::uint32_t last_frame = 0;
    bool have_frame = false;
    bool have_audio = false;
    std::uint8_t audio_file = 0, audio_channel = 0, audio_coding = 0;
    XaHistory hist;
    std::int16_t left[kXaMonoSamples], right[kXaMonoSamples];
    for (std::size_t s = 0; s < sectors; ++s) {
        const std::uint8_t* raw = data + s * kSector;
        if (std::memcmp(raw, kSync, sizeof(kSync)) != 0) return Fail(why, why_n, "STR: sector %zu has no sync", s);
        const std::uint8_t submode = raw[18];
        if (submode & 0x04) {   // submode bit 2: an audio sector
            if (!have_audio) {
                have_audio = true;
                audio_file = raw[16];
                audio_channel = raw[17];
                audio_coding = raw[19];
                if (audio_coding & 0x30) {
                    return Fail(why, why_n, "STR: the audio (sector %zu) is coded 0x%02X: only four-bit is read", s, audio_coding);
                }
                audio_rate_ = (audio_coding & 0x04) ? 18900 : 37800;
                audio_channels_ = (audio_coding & 1) ? 2 : 1;
            } else if (raw[16] != audio_file || raw[17] != audio_channel) {
                continue;   // another channel (the trailing mono one): not played
            }
            if (raw[19] != audio_coding) {
                return Fail(why, why_n, "STR: the audio changes its coding (0x%02X, then 0x%02X at sector %zu)", audio_coding, raw[19], s);
            }
            const bool stereo = audio_channels_ == 2;
            for (int g = 0; g < 18; ++g) {
                const std::size_t n = XaDecodeGroup(raw + kData + static_cast<std::size_t>(g) * kXaGroupBytes, stereo, hist, left, right);
                for (std::size_t i = 0; i < n; ++i) {
                    audio_.push_back(left[i]);
                    if (stereo) audio_.push_back(right[i]);
                }
            }
            continue;
        }
        // Video sectors are Form 1 (submode bit 5 clear) and open with the magic.
        if ((submode & 0x20) != 0 || U32(raw + kData) != kMagic) continue;
        const std::uint8_t* h = raw + kData;
        const std::uint32_t chunk = U16(h + 4), count = U16(h + 6), frame = U32(h + 8), used = U32(h + 12);
        const std::uint32_t w = U16(h + 16), ht = U16(h + 18);
        const std::uint32_t id = U16(h + 22), qscale = U16(h + 24), version = U16(h + 26);
        if (id != 0x3800) return Fail(why, why_n, "STR: sector %zu has id 0x%04X, only 0x3800 is read", s, id);
        if (version != 2) return Fail(why, why_n, "STR: sector %zu is version %u, only version 2 is read", s, version);
        if (width_ == 0) {
            if (w == 0 || ht == 0 || w > kMaxSide || ht > kMaxSide || w % 16 != 0 || ht % 16 != 0) {
                return Fail(why, why_n, "STR: the picture is %u x %u: a multiple of 16, up to %u, is read", w, ht, kMaxSide);
            }
            width_ = w;
            height_ = ht;
        } else if (w != width_ || ht != height_) {
            return Fail(why, why_n, "STR: sector %zu changes the picture size to %u x %u", s, w, ht);
        }
        if (count == 0 || count > kMaxChunks || chunk >= count) {
            return Fail(why, why_n, "STR: sector %zu is chunk %u of %u", s, chunk, count);
        }
        if (!have_frame || frame != last_frame) {
            FrameRef f = {};
            f.chunks = count;
            f.used = used;
            f.qscale = qscale;
            for (std::uint32_t& x : f.sector) x = 0xFFFFFFFFu;
            frames_.push_back(f);
            last_frame = frame;
            have_frame = true;
        }
        FrameRef& f = frames_.back();
        if (count != f.chunks || f.sector[chunk] != 0xFFFFFFFFu) {
            return Fail(why, why_n, "STR: frame %u: sector %zu is chunk %u of %u, a repeat or another count", frame, s, chunk, count);
        }
        f.sector[chunk] = static_cast<std::uint32_t>(s);
    }
    if (frames_.empty()) return Fail(why, why_n, "STR: no video sector (0x80010160) found");
    for (std::size_t i = 0; i < frames_.size(); ++i) {
        for (std::uint32_t c = 0; c < frames_[i].chunks; ++c) {
            if (frames_[i].sector[c] == 0xFFFFFFFFu) return Fail(why, why_n, "STR: frame %zu lacks chunk %u of %u", i, c, frames_[i].chunks);
        }
    }
    if (audio_channels_ == 2 && audio_.size() % 2 != 0) std::abort();
    if (!have_audio) {
        audio_rate_ = 0;
        audio_channels_ = 0;
    }
    frame_seconds_ = static_cast<double>(sectors) / (150.0 * static_cast<double>(frames_.size()));
    raw_.assign(static_cast<std::size_t>(width_) * height_, 0);
    doubled_.assign(static_cast<std::size_t>(width_) * height_ * 4, 0);
    data_ = data;
    return true;
}

bool StrMovie::DecodeFrame(const FrameRef& f, char* why, std::size_t why_n) {
    bits_.clear();
    for (std::uint32_t c = 0; c < f.chunks; ++c) {
        const std::uint8_t* raw = data_ + static_cast<std::size_t>(f.sector[c]) * kSector + kData + kVideoHeader;
        bits_.insert(bits_.end(), raw, raw + kChunkBytes);
    }
    if (f.used < bits_.size()) bits_.resize(f.used);
    if (bits_.size() < 8) return Fail(why, why_n, "STR: frame %u has %zu bytes", decoded_ + 1, bits_.size());
    BitReader br(bits_.data() + 8, bits_.size() - 8);   // past the BS header, which the chunk header repeats
    const std::uint32_t mbw = width_ / 16, mbh = height_ / 16;
    std::int32_t cr[64], cb[64], y[4][64];
    for (std::uint32_t mx = 0; mx < mbw; ++mx) {
        for (std::uint32_t my = 0; my < mbh; ++my) {
            if (!Block(br, f.qscale, cr, why, why_n) || !Block(br, f.qscale, cb, why, why_n) ||
                !Block(br, f.qscale, y[0], why, why_n) || !Block(br, f.qscale, y[1], why, why_n) ||
                !Block(br, f.qscale, y[2], why, why_n) || !Block(br, f.qscale, y[3], why, why_n)) {
                return false;
            }
            std::uint32_t* at = raw_.data() + static_cast<std::size_t>(my) * 16 * width_ + static_cast<std::size_t>(mx) * 16;
            Colour(at, width_, y[0], cr, cb, 0, 0);
            Colour(at, width_, y[1], cr, cb, 8, 0);
            Colour(at, width_, y[2], cr, cb, 0, 8);
            Colour(at, width_, y[3], cr, cb, 8, 8);
        }
    }
    return true;
}

bool StrMovie::NextFrame(char* why, std::size_t why_n) {
    if (why != nullptr && why_n > 0) why[0] = 0;
    if (data_ == nullptr || decoded_ >= frames_.size()) return false;
    if (!DecodeFrame(frames_[decoded_], why, why_n)) return false;
    ++decoded_;
    const std::size_t w = width_, h = height_;
    for (std::size_t row = 0; row < h; ++row) {
        const std::uint32_t* src = raw_.data() + row * w;
        std::uint32_t* d0 = doubled_.data() + row * 2 * (w * 2);
        std::uint32_t* d1 = d0 + w * 2;
        for (std::size_t x = 0; x < w; ++x) d0[2 * x] = d0[2 * x + 1] = src[x];
        std::memcpy(d1, d0, w * 2 * sizeof(std::uint32_t));
    }
    return true;
}

}  // namespace media

// Tests of src/media on synthetic data the tests make themselves - no game data.
//   media_tests            exit 0 when every check holds
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <string>
#include <vector>

#include "media/resample.h"
#include "media/str_movie.h"
#include "media/xa_adpcm.h"
#include "media/xa_clip.h"

namespace {

int g_checks = 0;
int g_failed = 0;

#define CHECK(cond)                                                              \
    do {                                                                         \
        ++g_checks;                                                              \
        if (!(cond)) {                                                           \
            ++g_failed;                                                          \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);          \
        }                                                                        \
    } while (0)

using Bytes = std::vector<std::uint8_t>;

bool Has(const char* why, const char* word) { return std::strstr(why, word) != nullptr; }

// ---- XA ADPCM ---------------------------------------------------------------

// Puts the 4-bit value `nib` (0..15) in sample `j` of unit `u`.
void SetNibble(std::uint8_t* group, int u, int j, int nib) {
    std::uint8_t& b = group[16 + 4 * j + (u >> 1)];
    b = (u & 1) ? static_cast<std::uint8_t>((b & 0x0F) | (nib << 4)) : static_cast<std::uint8_t>((b & 0xF0) | nib);
}

void SetParam(std::uint8_t* group, int u, int shift, int filter) {
    const std::uint8_t p = static_cast<std::uint8_t>(shift | (filter << 4));
    group[4 + u] = p;
    group[u < 4 ? u : 12 + (u - 4)] = p;   // the copies
}

// The group the hand-computed samples below come from.
Bytes HandGroup() {
    Bytes g(128, 0);
    // Unit 0: shift 12, filter 0: a sample is the nibble itself. 1, -1, 7, -8.
    SetParam(g.data(), 0, 12, 0);
    SetNibble(g.data(), 0, 0, 1);
    SetNibble(g.data(), 0, 1, 15);
    SetNibble(g.data(), 0, 2, 7);
    SetNibble(g.data(), 0, 3, 8);
    // Unit 1: shift 12, filter 1 (k0 = 60), nibble 7 every sample.
    SetParam(g.data(), 1, 12, 1);
    for (int j = 0; j < 28; ++j) SetNibble(g.data(), 1, j, 7);
    // Unit 2: shift 0, filter 0: 7 -> 28672, -8 -> -32768.
    SetParam(g.data(), 2, 0, 0);
    SetNibble(g.data(), 2, 0, 7);
    SetNibble(g.data(), 2, 1, 8);
    // Unit 3: shift 0, filter 1: 7 then more 7s climb past 32767 and clamp.
    SetParam(g.data(), 3, 0, 1);
    for (int j = 0; j < 28; ++j) SetNibble(g.data(), 3, j, 7);
    // Units 4..7: shift 12, filter 0, nibble 0 (silence).
    for (int u = 4; u < 8; ++u) SetParam(g.data(), u, 12, 0);
    return g;
}

void TestAdpcm() {
    const Bytes g = HandGroup();
    media::XaHistory h;
    std::int16_t mono[media::kXaMonoSamples];
    CHECK(media::XaDecodeGroup(g.data(), false, h, mono, nullptr) == 224);
    // unit 0
    CHECK(mono[0] == 1 && mono[1] == -1 && mono[2] == 7 && mono[3] == -8 && mono[4] == 0);
    // unit 1 at 28..: 7, 13, 19, 24, 29 (7 + (s * 60 >> 6) each time)
    CHECK(mono[28] == 7 && mono[29] == 13 && mono[30] == 19 && mono[31] == 24 && mono[32] == 29);
    // unit 2 at 56..
    CHECK(mono[56] == 28672 && mono[57] == -32768 && mono[58] == 0);
    // unit 3 at 84..: 28672, then 28672 + 26880 clamps to 32767 and stays there
    CHECK(mono[84] == 28672 && mono[85] == 32767 && mono[86] == 32767);
    // unit 4 at 112..: silence, filter 0: zeros whatever the history
    CHECK(mono[112] == 0 && mono[140] == 0);
    // the prediction floors: shift 12, filter 1, nibble -1 from a fresh history gives
    // -1, then -1 + (-60 >> 6) = -2, then -1 + (-120 >> 6) = -3, then -4
    {
        Bytes f(128, 0);
        SetParam(f.data(), 0, 12, 1);
        for (int j = 0; j < 28; ++j) SetNibble(f.data(), 0, j, 15);
        media::XaHistory hf;
        std::int16_t o[media::kXaMonoSamples];
        media::XaDecodeGroup(f.data(), false, hf, o, nullptr);
        CHECK(o[0] == -1 && o[1] == -2 && o[2] == -3 && o[3] == -4);
    }
    // a clip starts from zeros: the same group again from a fresh history is the same
    media::XaHistory h2;
    std::int16_t again[media::kXaMonoSamples];
    media::XaDecodeGroup(g.data(), false, h2, again, nullptr);
    CHECK(std::memcmp(mono, again, sizeof(mono)) == 0);
    // the history carries over: a second group starts from the first's last two samples
    std::int16_t second[media::kXaMonoSamples];
    media::XaDecodeGroup(g.data(), false, h, second, nullptr);
    CHECK(second[0] == 1);   // unit 0's filter is 0: the history is not read there

    // Stereo: the even units to the left (0, 2, 4, 6), the odd to the right.
    media::XaHistory hs;
    std::int16_t l[media::kXaMonoSamples], r[media::kXaMonoSamples];
    CHECK(media::XaDecodeGroup(g.data(), true, hs, l, r) == 112);
    CHECK(l[0] == 1 && l[1] == -1 && l[2] == 7 && l[3] == -8);   // unit 0
    CHECK(l[28] == 28672 && l[29] == -32768);                    // unit 2
    CHECK(r[0] == 7 && r[1] == 13 && r[2] == 19 && r[3] == 24);  // unit 1
}

// ---- the resampler ----------------------------------------------------------

std::vector<std::int16_t> Run(const std::vector<std::int16_t>& in, unsigned down, std::size_t flush = 0) {
    media::Resampler rs;
    CHECK(rs.Reset(down));
    std::vector<std::int16_t> out;
    std::int16_t tmp[3];
    for (std::size_t i = 0; i < in.size() + flush; ++i) {
        const std::size_t n = rs.Push(i < in.size() ? in[i] : 0, tmp);
        CHECK(n <= 3);
        for (std::size_t k = 0; k < n; ++k) out.push_back(tmp[k]);
    }
    return out;
}

void TestResampler() {
    media::Resampler rs;
    CHECK(!rs.Reset(5));
    CHECK(media::ResampleOutputs(0, 6) == 0);
    CHECK(media::ResampleOutputs(6, 6) == 7);
    CHECK(media::ResampleOutputs(7, 3) == 17);   // ceil(49 / 3)
    // Output length: after n inputs and 24 zeros, the first ceil(7n/M) outputs exist, and
    // the next one (k * M >= 7n) is not owed.
    for (unsigned down : {6u, 3u}) {
        for (std::size_t n = 1; n < 300; ++n) {
            const std::vector<std::int16_t> in(n, 1000);
            const std::size_t want = static_cast<std::size_t>(media::ResampleOutputs(n, down));
            const std::size_t made = Run(in, down, 24).size();
            CHECK(made >= want);
            if (made < want) break;
            // one push fewer and the last owed output is not yet made
            CHECK(Run(in, down, 23).size() < want || n == 0);
        }
    }
    // DC gain: a constant comes out the same constant wherever the filter is full.
    for (std::int16_t dc : {10000, -12345, 32767, -32768}) {
        for (unsigned down : {6u, 3u}) {
            const std::vector<std::int16_t> in(400, dc);
            const std::vector<std::int16_t> out = Run(in, down, 24);
            bool flat = true;
            const std::size_t lo = 64, hi = static_cast<std::size_t>(media::ResampleOutputs(400 - 30, down));
            for (std::size_t k = lo; k < hi; ++k) flat = flat && out[k] == dc;
            CHECK(flat);
        }
    }
    // One impulse: 16384 at input 6 is output 7 of the 7/6 ratio (7 * 6 / 6), centred on tap
    // 23 of phase 0, 60559 / 65536 of it, rounded.
    {
        std::vector<std::int16_t> in(60, 0);
        in[6] = 16384;
        const std::vector<std::int16_t> out = Run(in, 6, 24);
        CHECK(out.size() > 8);
        CHECK(out[7] == ((16384 * 60559 + 32768) >> 16));
    }
    // Determinism: the same input, the same samples, and a checksum of a noise run that
    // every build and every host must reach.
    {
        std::vector<std::int16_t> in(5000);
        std::uint32_t seed = 12345;
        for (std::int16_t& v : in) {
            seed = seed * 1664525u + 1013904223u;
            v = static_cast<std::int16_t>(seed >> 16);
        }
        for (unsigned down : {6u, 3u}) {
            const std::vector<std::int16_t> a = Run(in, down, 24), b = Run(in, down, 24);
            CHECK(a == b);
            std::uint32_t sum = 2166136261u;
            for (std::int16_t v : a) sum = (sum ^ static_cast<std::uint16_t>(v)) * 16777619u;
            CHECK(a.size() == (down == 6 ? 5834u : 11667u));
            CHECK(sum == (down == 6 ? 0xD309D953u : 0x8182C5D7u));
        }
    }
    // A tone inside the pass band keeps its level. The tone is a triangle wave
    // of period 36 samples (1,050 Hz at 37,800 Hz), peak 9,999: integer-only, and
    // its fundamental is well below the cut-off.
    {
        std::vector<std::int16_t> in(4000);
        for (std::size_t i = 0; i < in.size(); ++i) {
            const int p = static_cast<int>(i % 36);
            const int tri = p < 18 ? p : 36 - p;   // 0..18..0
            in[i] = static_cast<std::int16_t>((tri - 9) * 1111);
        }
        const std::vector<std::int16_t> out = Run(in, 6, 24);
        int pk_in = 0, pk_out = 0;
        for (std::size_t i = 500; i < 3000; ++i) pk_in = std::max<int>(pk_in, in[i]);
        for (std::size_t k = 600; k < 3500; ++k) pk_out = std::max<int>(pk_out, out[k]);
        CHECK(pk_out > pk_in * 98 / 100 && pk_out < pk_in * 104 / 100);
    }
}

// ---- the XA clip ------------------------------------------------------------

Bytes ClipHeader(std::uint32_t rate, std::uint16_t channels, std::uint16_t bits, std::uint32_t groups) {
    Bytes b = {'B', 'O', 'F', '3', 'X', 'A', 0, 1};
    auto u32 = [&](std::uint32_t v) { for (int i = 0; i < 4; ++i) b.push_back(static_cast<std::uint8_t>(v >> (8 * i))); };
    auto u16 = [&](std::uint16_t v) { b.push_back(static_cast<std::uint8_t>(v)); b.push_back(static_cast<std::uint8_t>(v >> 8)); };
    u32(rate);
    u16(channels);
    u16(bits);
    u32(groups);
    u32(0);
    return b;
}

Bytes MakeClip(std::uint32_t rate, std::uint16_t channels, std::uint32_t groups) {
    Bytes b = ClipHeader(rate, channels, 4, groups);
    std::uint32_t seed = 99;
    for (std::uint32_t g = 0; g < groups; ++g) {
        Bytes grp(128);
        for (std::uint8_t& v : grp) {
            seed = seed * 1664525u + 1013904223u;
            v = static_cast<std::uint8_t>(seed >> 24);
        }
        for (int u = 0; u < 8; ++u) grp[4 + u] = static_cast<std::uint8_t>((grp[4 + u] & 0x30) | 8);   // shift 8, any filter
        b.insert(b.end(), grp.begin(), grp.end());
    }
    return b;
}

std::vector<std::int16_t> ReadAll(media::XaClip& c, std::size_t step) {
    std::vector<std::int16_t> out;
    std::vector<std::int16_t> buf(2 * step);
    for (;;) {
        const std::size_t n = c.Read(buf.data(), step);
        if (n == 0) break;
        out.insert(out.end(), buf.begin(), buf.begin() + static_cast<std::ptrdiff_t>(2 * n));
    }
    return out;
}

void TestClip() {
    char why[200];
    media::XaClip c;
    const Bytes good = MakeClip(37800, 2, 3);
    CHECK(media::IsXaClip(good.data(), good.size()));
    const std::uint8_t other[8] = {'R', 'I', 'F', 'F', 0, 0, 0, 0};
    CHECK(!media::IsXaClip(other, sizeof(other)));
    CHECK(!c.Open(other, sizeof(other), why, sizeof(why)) && Has(why, "magic"));

    Bytes bad = MakeClip(44100, 2, 1);
    CHECK(!c.Open(bad.data(), bad.size(), why, sizeof(why)) && Has(why, "44100"));
    bad = MakeClip(37800, 3, 1);
    CHECK(!c.Open(bad.data(), bad.size(), why, sizeof(why)) && Has(why, "3 channels"));
    bad = MakeClip(37800, 2, 1);
    bad[14] = 8;
    CHECK(!c.Open(bad.data(), bad.size(), why, sizeof(why)) && Has(why, "8 bits"));
    bad = MakeClip(37800, 2, 2);
    bad.pop_back();
    CHECK(!c.Open(bad.data(), bad.size(), why, sizeof(why)) && Has(why, "bytes"));
    bad = MakeClip(37800, 2, 2);
    bad.push_back(0);
    CHECK(!c.Open(bad.data(), bad.size(), why, sizeof(why)));
    bad = ClipHeader(37800, 2, 4, 0);
    CHECK(!c.Open(bad.data(), bad.size(), why, sizeof(why)) && Has(why, "no sound groups"));
    CHECK(!c.Open(good.data(), 10, why, sizeof(why)));
    CHECK(c.TotalFrames() == 0 && c.Read(nullptr, 0) == 0);

    // stereo 37,800 Hz: 3 groups x 112 = 336 a side -> 392 frames
    CHECK(c.Open(good.data(), good.size(), why, sizeof(why)));
    CHECK(c.TotalFrames() == 392);
    const std::vector<std::int16_t> all = ReadAll(c, 4096);
    CHECK(all.size() == 2 * 392);
    CHECK(c.Read(nullptr, 5) == 0);
    c.Rewind();
    CHECK(ReadAll(c, 4096) == all);
    c.Rewind();
    CHECK(ReadAll(c, 7) == all);   // reads of any size give the same samples
    CHECK(ReadAll(c, 1) == std::vector<std::int16_t>());
    c.Rewind();
    CHECK(ReadAll(c, 1) == all);
    // the left side equals the resampled ADPCM decode of the left side
    {
        media::XaHistory h;
        std::vector<std::int16_t> l;
        for (int g = 0; g < 3; ++g) {
            std::int16_t a[media::kXaMonoSamples], b[media::kXaMonoSamples];
            media::XaDecodeGroup(good.data() + 24 + 128 * g, true, h, a, b);
            l.insert(l.end(), a, a + 112);
        }
        const std::vector<std::int16_t> want = Run(l, 6, 24);
        bool same = want.size() >= 392;
        for (std::size_t k = 0; same && k < 392; ++k) same = want[k] == all[2 * k];
        CHECK(same);
    }

    // mono 18,900 Hz: 2 groups x 224 = 448 -> ceil(7 * 448 / 3) = 1046 frames, doubled
    const Bytes mono = MakeClip(18900, 1, 2);
    CHECK(c.Open(mono.data(), mono.size(), why, sizeof(why)));
    CHECK(c.TotalFrames() == 1046);
    const std::vector<std::int16_t> m = ReadAll(c, 100);
    CHECK(m.size() == 2 * 1046);
    bool doubled = true;
    for (std::size_t k = 0; k < 1046; ++k) doubled = doubled && m[2 * k] == m[2 * k + 1];
    CHECK(doubled);
}

// ---- the STR movie ----------------------------------------------------------

constexpr std::size_t kSector = 2352;

// Bits into the stream's form: 16-bit little-endian words, top bit first.
struct BitWriter {
    Bytes out;
    std::uint32_t acc = 0;
    int n = 0;
    void Put(std::uint32_t value, int bits) {
        for (int i = bits - 1; i >= 0; --i) {
            acc = acc << 1 | ((value >> i) & 1);
            if (++n == 16) {
                out.push_back(static_cast<std::uint8_t>(acc & 0xFF));
                out.push_back(static_cast<std::uint8_t>(acc >> 8));
                acc = 0;
                n = 0;
            }
        }
    }
    void Pad() { while (n != 0) Put(0, 1); }
};

Bytes Sector(std::uint8_t file, std::uint8_t channel, std::uint8_t submode, std::uint8_t coding) {
    Bytes s(kSector, 0);
    s[0] = 0;
    for (int i = 1; i <= 10; ++i) s[i] = 0xFF;
    s[11] = 0;
    s[15] = 2;
    s[16] = s[20] = file;
    s[17] = s[21] = channel;
    s[18] = s[22] = submode;
    s[19] = s[23] = coding;
    return s;
}

// A one-chunk video sector for a frame of two macroblocks (32 x 16), each block a DC and the end
// of block. `dc[mb][block]`, blocks in the order Cr, Cb, Y0..Y3.
Bytes VideoSector(std::uint32_t frame, const int dc[2][6], std::uint16_t version = 2, std::uint16_t id = 0x3800) {
    BitWriter bw;
    for (int mb = 0; mb < 2; ++mb) {
        for (int b = 0; b < 6; ++b) {
            bw.Put(static_cast<std::uint32_t>(dc[mb][b]) & 0x3FF, 10);
            bw.Put(2, 2);   // 10: end of block
        }
    }
    bw.Pad();
    Bytes frame_bytes = {0x10, 0x00, static_cast<std::uint8_t>(id & 0xFF), static_cast<std::uint8_t>(id >> 8), 4, 0,
                         static_cast<std::uint8_t>(version), 0};   // the BS header: words, id, qscale, version
    frame_bytes.insert(frame_bytes.end(), bw.out.begin(), bw.out.end());
    Bytes s = Sector(1, 0, 0x42, 0);
    std::uint8_t* d = s.data() + 24;
    const std::uint32_t head[4] = {0x80010160u, 0u | (1u << 16), frame, static_cast<std::uint32_t>(frame_bytes.size())};
    for (int i = 0; i < 4; ++i) for (int k = 0; k < 4; ++k) d[4 * i + k] = static_cast<std::uint8_t>(head[i] >> (8 * k));
    d[16] = 32; d[18] = 16;                                   // width 32, height 16
    std::memcpy(d + 20, frame_bytes.data(), 8);               // the BS header, as the chunk header holds it
    std::memcpy(d + 32, frame_bytes.data(), frame_bytes.size());
    return s;
}

Bytes Cat(std::initializer_list<Bytes> parts) {
    Bytes out;
    for (const Bytes& p : parts) out.insert(out.end(), p.begin(), p.end());
    return out;
}

Bytes AudioSector(std::uint8_t channel, std::uint8_t coding, std::uint8_t fill) {
    Bytes s = Sector(1, channel, 0x64, coding);
    for (int g = 0; g < 18; ++g) {
        for (int i = 0; i < 128; ++i) s[24 + 128 * g + i] = static_cast<std::uint8_t>(fill + i * 7 + g);
        for (int u = 0; u < 8; ++u) s[24 + 128 * g + 4 + u] &= 0x3F;   // filters 0..3 as they come, shifts as they come
    }
    return s;
}

void TestStr() {
    char why[200];
    // Frame 1: grey (luma DC 100 -> 25 -> 153). Frame 2: macroblock 0 with Cr DC 40 (+10), macroblock 1 Cb DC -40 (-10).
    const int flat[2][6] = {{0, 0, 100, 100, 100, 100}, {0, 0, 100, 100, 100, 100}};
    const int tint[2][6] = {{40, 0, 100, 100, 100, 100}, {0, -40, 100, 100, 100, 100}};
    const Bytes audio = AudioSector(1, 1, 3);
    const Bytes other = AudioSector(2, 0, 9);   // another channel, mono: not played
    const Bytes str = Cat({VideoSector(1, flat), audio, VideoSector(2, tint), other});
    CHECK(media::IsStr(str.data(), str.size()));

    media::StrMovie m;
    CHECK(m.Open(str.data(), str.size(), why, sizeof(why)));
    CHECK(m.Width() == 64 && m.Height() == 32);   // doubled
    CHECK(m.FrameCount() == 2);
    CHECK(m.FrameSeconds() > 4.0 / 300 - 1e-9 && m.FrameSeconds() < 4.0 / 300 + 1e-9);   // 4 sectors, 2 frames
    CHECK(m.FramesDecoded() == 0);
    CHECK(m.HasAudio() && m.AudioRate() == 37800 && m.AudioChannels() == 2 && m.AudioFrames() == 18 * 112);
    {
        // the audio is the first channel's groups through the ADPCM decoder
        media::XaHistory h;
        std::vector<std::int16_t> want;
        for (int g = 0; g < 18; ++g) {
            std::int16_t l[media::kXaMonoSamples], r[media::kXaMonoSamples];
            media::XaDecodeGroup(audio.data() + 24 + 128 * g, true, h, l, r);
            for (int i = 0; i < 112; ++i) { want.push_back(l[i]); want.push_back(r[i]); }
        }
        CHECK(want.size() == m.AudioFrames() * 2 && std::memcmp(want.data(), m.Audio(), want.size() * 2) == 0);
    }
    CHECK(m.NextFrame(why, sizeof(why)));
    CHECK(m.FramesDecoded() == 1);
    const std::uint32_t grey = 153u << 16 | 153u << 8 | 153u;
    bool flat_ok = true;
    for (int i = 0; i < 32 * 16; ++i) flat_ok = flat_ok && m.RawFrame320()[i] == grey;
    CHECK(flat_ok);
    bool doubled_ok = true;
    for (int y = 0; y < 32; ++y) for (int x = 0; x < 64; ++x) doubled_ok = doubled_ok && m.Frame()[y * 64 + x] == grey;
    CHECK(doubled_ok);
    CHECK(m.NextFrame(why, sizeof(why)));
    // Cr +10: R + 14, G - 7. Cb -10: G + 3, B - 17 (the products cut toward zero).
    const std::uint32_t red = (153u + 14) << 16 | (153u - 7) << 8 | 153u;
    const std::uint32_t blue = 153u << 16 | (153u + 3) << 8 | (153u - 17);
    CHECK(m.RawFrame320()[0] == red && m.RawFrame320()[15 * 32 + 15] == red);
    CHECK(m.RawFrame320()[16] == blue && m.RawFrame320()[15 * 32 + 31] == blue);
    CHECK(m.Frame()[1] == red && m.Frame()[64 + 33] == blue && m.Frame()[31 * 64 + 63] == blue);   // doubled
    CHECK(!m.NextFrame(why, sizeof(why)) && why[0] == 0);   // the end, not a fault
    CHECK(m.FramesDecoded() == 2);

    // refusals
    Bytes v3 = Cat({VideoSector(1, flat, 3)});
    CHECK(!m.Open(v3.data(), v3.size(), why, sizeof(why)) && Has(why, "version 3"));
    Bytes id = Cat({VideoSector(1, flat, 2, 0x3801)});
    CHECK(!m.Open(id.data(), id.size(), why, sizeof(why)) && Has(why, "0x3801"));
    Bytes sync = str;
    sync[5] = 0;
    CHECK(!media::IsStr(sync.data(), sync.size()));
    CHECK(!m.Open(sync.data(), sync.size(), why, sizeof(why)) && Has(why, "sync"));
    Bytes sync2 = str;
    sync2[kSector * 2 + 3] = 0;   // a later sector
    CHECK(media::IsStr(sync2.data(), sync2.size()));
    CHECK(!m.Open(sync2.data(), sync2.size(), why, sizeof(why)) && Has(why, "sync"));
    CHECK(!media::IsStr(str.data(), str.size() - 1));
    Bytes eight = Cat({VideoSector(1, flat), AudioSector(1, 0x11, 0)});
    CHECK(!m.Open(eight.data(), eight.size(), why, sizeof(why)) && Has(why, "four-bit"));
    Bytes novideo = Cat({audio});
    CHECK(!m.Open(novideo.data(), novideo.size(), why, sizeof(why)) && Has(why, "no video"));

    // A bad code in the bitstream is a fault with a reason, not a crash: a zero word where a code is due.
    Bytes broken = Cat({VideoSector(1, flat)});
    for (std::size_t i = 24 + 32 + 8 + 3; i < 24 + 32 + 8 + 12; ++i) broken[i] = 0;
    CHECK(m.Open(broken.data(), broken.size(), why, sizeof(why)));
    CHECK(!m.NextFrame(why, sizeof(why)) && why[0] != 0);
}

}  // namespace

int main() {
    TestAdpcm();
    TestResampler();
    TestClip();
    TestStr();
    std::printf("%d checks, %d failed\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}

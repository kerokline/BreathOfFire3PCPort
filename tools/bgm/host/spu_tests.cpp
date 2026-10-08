// Host-side unit tests for psx::Spu (src/audio/spu.cpp). Each test computes its
// expectation independently - by hand in the comments, or by a small
// reference routine written here from the psx-spx description - and compares
// the model against it. docs/spu-model.md says what each test establishes.
#include "audio/spu.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace {

int g_failures = 0;
int g_checks = 0;

#define CHECK(cond)                                                                  \
    do {                                                                             \
        ++g_checks;                                                                  \
        if (!(cond)) {                                                               \
            ++g_failures;                                                            \
            std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);            \
        }                                                                            \
    } while (0)

#define CHECK_EQ(a, b)                                                               \
    do {                                                                             \
        ++g_checks;                                                                  \
        long long va_ = static_cast<long long>(a), vb_ = static_cast<long long>(b);  \
        if (va_ != vb_) {                                                            \
            ++g_failures;                                                            \
            std::printf("  FAIL %s:%d: %s == %lld, expected %s == %lld\n", __FILE__, \
                        __LINE__, #a, va_, #b, vb_);                                 \
        }                                                                            \
    } while (0)

using psx::Spu;

// A 16-byte ADPCM block: header byte (filter << 4 | range), flags, 28 nibbles.
struct Block {
    std::uint8_t b[16];
};
Block MakeBlock(int filter, int range, int flags, const int nibbles[28]) {
    Block blk;
    blk.b[0] = static_cast<std::uint8_t>((filter << 4) | range);
    blk.b[1] = static_cast<std::uint8_t>(flags);
    for (int j = 0; j < 14; ++j)
        blk.b[2 + j] = static_cast<std::uint8_t>((nibbles[2 * j] & 0xF) | ((nibbles[2 * j + 1] & 0xF) << 4));
    return blk;
}
Block MakeBlockFill(int filter, int range, int flags, int nibble) {
    int n[28];
    for (int& x : n) x = nibble;
    return MakeBlock(filter, range, flags, n);
}

// Reference ADPCM decode, written from psx-spx's decode_28_nibbles for this
// test (not shared with the model).
void RefDecode(const Block& blk, std::vector<int>& out, int& old, int& older) {
    static const int pos[5] = {0, 60, 115, 98, 122};
    static const int neg[5] = {0, 0, -52, -55, -60};
    int range = blk.b[0] & 15;
    if (range > 12) range = 9;
    int shift = 12 - range;
    int filter = blk.b[0] >> 4;
    for (int j = 0; j < 28; ++j) {
        int nib = (blk.b[2 + j / 2] >> (4 * (j % 2))) & 15;
        int t = nib >= 8 ? nib - 16 : nib;
        long long p = static_cast<long long>(old) * pos[filter] + static_cast<long long>(older) * neg[filter] + 32;
        long long q = p >= 0 ? p / 64 : -((-p + 63) / 64); // floor division
        long long s = static_cast<long long>(t) * (1 << shift) + q;
        if (s > 32767) s = 32767;
        if (s < -32768) s = -32768;
        out.push_back(static_cast<int>(s));
        older = old;
        old = static_cast<int>(s);
    }
}

long long FloorShift15(long long x) { return x >= 0 ? x / 32768 : -((-x + 32767) / 32768); }

// Reference Gaussian, from the formula, through the model's table.
int RefGauss(int oldest, int older, int old, int nw, int i) {
    return static_cast<int>(FloorShift15(static_cast<long long>(Spu::GaussTable(0x0FF - i)) * oldest) +
                            FloorShift15(static_cast<long long>(Spu::GaussTable(0x1FF - i)) * older) +
                            FloorShift15(static_cast<long long>(Spu::GaussTable(0x100 + i)) * old) +
                            FloorShift15(static_cast<long long>(Spu::GaussTable(0x000 + i)) * nw));
}

std::unique_ptr<Spu> NewSpu() {
    std::unique_ptr<Spu> s(new Spu());
    s->SetControl(0xC000); // enabled, unmuted
    s->SetMainVolumeLeft(0x3FFF);
    s->SetMainVolumeRight(0x3FFF);
    return s;
}

void Put(Spu& s, std::uint32_t address, const Block& blk) { s.WriteRam(address, blk.b, 16); }

std::int16_t Capture(const Spu& s, std::uint32_t base, std::uint32_t tick) {
    std::uint8_t b[2];
    s.ReadRam(base + 2 * (tick & 0x1FF), b, 2);
    return static_cast<std::int16_t>(b[0] | (b[1] << 8));
}

// ---------------------------------------------------------------------------

void TestGaussTable() {
    // psx-spx: "each four values (gauss[000h+i], gauss[0FFh-i], gauss[100h+i],
    // gauss[1FFh-i]) sum up to 7F80h ... spreads the actual sums over
    // 7F7Fh..7F81h". A transcription error in any entry breaks its sum.
    for (int i = 0; i < 256; ++i) {
        int sum = Spu::GaussTable(i) + Spu::GaussTable(0xFF - i) + Spu::GaussTable(0x100 + i) + Spu::GaussTable(0x1FF - i);
        CHECK(sum >= 0x7F7F && sum <= 0x7F81);
    }
    for (int i = 0; i < 16; ++i) CHECK_EQ(Spu::GaussTable(i), -1);
    CHECK_EQ(Spu::GaussTable(0x0FF), 0x12C7);
    CHECK_EQ(Spu::GaussTable(0x100), 0x1307);
    CHECK_EQ(Spu::GaussTable(0x1FF), 0x59B3);
    // Monotonic rise over the whole table (the window's half).
    for (int i = 16; i < 512; ++i) CHECK(Spu::GaussTable(i) >= Spu::GaussTable(i - 1));
}

void TestDecodeByHand() {
    // Range nibble 8: t << 4. Nibbles 1, -2, 0...; history old 1000, older 500.
    int n[28] = {1, 0xE};
    struct Case {
        int filter;
        int s0, s1;
    } cases[] = {
        {0, 16, -32},     // no prediction
        {1, 954, 862},    // 16 + (60000+32)>>6 = 16+938; -32 + (57240+32)>>6 = -32+894
        {2, 1407, 1684},  // 16 + (115000-26000+32)>>6 = 16+1391; -32 + (161805-52000+32)>>6
        {3, 1118, 821},   // 16 + (98000-27500+32)>>6 = 16+1102; -32 + (109564-55000+32)>>6
        {4, 1454, 1802},  // 16 + (122000-30000+32)>>6 = 16+1438; -32 + (177388-60000+32)>>6
    };
    for (const Case& c : cases) {
        Block blk = MakeBlock(c.filter, 8, 0, n);
        std::int16_t out[28];
        std::int32_t hist[2] = {1000, 500};
        Spu::DecodeBlock(blk.b, out, hist);
        CHECK_EQ(out[0], c.s0);
        CHECK_EQ(out[1], c.s1);
        CHECK_EQ(hist[0], out[27]);
        CHECK_EQ(hist[1], out[26]);
    }
    {
        // The >> 6 floors: filter 1, old -1001: (-60060+32) >> 6 = -938 (truncation would give -937).
        int z[28] = {0};
        Block blk = MakeBlock(1, 12, 0, z);
        std::int16_t out[28];
        std::int32_t hist[2] = {-1001, 0};
        Spu::DecodeBlock(blk.b, out, hist);
        CHECK_EQ(out[0], -938);
    }
    {
        // Clamping high: range 0, t 7 = 28672, filter 4, old 32767:
        // 28672 + (3997574+32)>>6 = 28672 + 62462 -> 32767. The clamped value
        // is the history: next (t 0) = (32767*122 - 32767*60 + 32) >> 6 = 31743.
        int m[28] = {7, 0};
        Block blk = MakeBlock(4, 0, 0, m);
        std::int16_t out[28];
        std::int32_t hist[2] = {32767, 0};
        Spu::DecodeBlock(blk.b, out, hist);
        CHECK_EQ(out[0], 32767);
        CHECK_EQ(out[1], 31743);
    }
    {
        // Clamping low: t -8 = -32768, filter 4, old -32768: -32768 + (-3997696+32)>>6 -> -32768.
        int m[28] = {8, 0};
        Block blk = MakeBlock(4, 0, 0, m);
        std::int16_t out[28];
        std::int32_t hist[2] = {-32768, 0};
        Spu::DecodeBlock(blk.b, out, hist);
        CHECK_EQ(out[0], -32768);
    }
    {
        // Range 13..15 act as 9 (CDROM XA-ADPCM header bytes): t 1 -> 8.
        // Range 12: t 1 -> 1, t -1 -> -1.
        int m[28] = {1, 0xF};
        for (int range = 13; range <= 15; ++range) {
            Block blk = MakeBlock(0, range, 0, m);
            std::int16_t out[28];
            std::int32_t hist[2] = {0, 0};
            Spu::DecodeBlock(blk.b, out, hist);
            CHECK_EQ(out[0], 8);
            CHECK_EQ(out[1], -8);
        }
        Block blk = MakeBlock(0, 12, 0, m);
        std::int16_t out[28];
        std::int32_t hist[2] = {0, 0};
        Spu::DecodeBlock(blk.b, out, hist);
        CHECK_EQ(out[0], 1);
        CHECK_EQ(out[1], -1);
    }
    {
        // The model against the reference routine over random blocks of every filter.
        std::uint32_t seed = 12345;
        auto rnd = [&]() { seed = seed * 1103515245u + 12345u; return (seed >> 16) & 0x7FFF; };
        int old = 0, older = 0;
        std::int32_t hist[2] = {0, 0};
        for (int k = 0; k < 2000; ++k) {
            int nib[28];
            for (int& x : nib) x = static_cast<int>(rnd() & 15);
            Block blk = MakeBlock(static_cast<int>(rnd() % 5), static_cast<int>(rnd() % 16), 0, nib);
            std::vector<int> ref;
            RefDecode(blk, ref, old, older);
            std::int16_t out[28];
            Spu::DecodeBlock(blk.b, out, hist);
            for (int j = 0; j < 28; ++j) CHECK_EQ(out[j], ref[j]);
        }
    }
}

void TestGaussianByHand() {
    const std::int16_t s[4] = {1000, 2000, 3000, 4000};
    // i = 0: (0x12C7*1000)>>15 + (0x59B3*2000)>>15 + (0x1307*3000)>>15 + (-1*4000)>>15
    //      = 146 + 1401 + 445 - 1
    CHECK_EQ(Spu::Interpolate(s, 0x00), 1991);
    // i = 0x80: g[7F]=019C, g[17F]=3DEF, g[180]=3E4C, g[80]=01A8: 12 + 967 + 1460 + 51
    CHECK_EQ(Spu::Interpolate(s, 0x80), 2490);
    // i = 0xFF: g[0]=-1, g[100]=1307, g[1FF]=59B3, g[FF]=12C7: -1 + 297 + 2102 + 586
    CHECK_EQ(Spu::Interpolate(s, 0xFF), 2984);
    // A constant input comes out at 255/256 of itself, within the table's rounding.
    const std::int16_t c[4] = {32767, 32767, 32767, 32767};
    for (int i = 0; i < 256; ++i) {
        int v = Spu::Interpolate(c, i);
        CHECK(v >= 32635 && v <= 32642);
    }
}

// A looped sample, A then B (loop start) then C (end + repeat): plays A B C B C
// B C ... The decoder's history runs on across the jump C -> B, so the second
// pass of B differs from the first; the model must match a reference that
// carries the history, at a pitch with a fractional step.
void TestLoopHistory() {
    std::uint32_t seed = 777;
    auto rnd = [&]() { seed = seed * 1103515245u + 12345u; return static_cast<int>((seed >> 16) & 15); };
    int na[28], nb[28], nc[28];
    for (int j = 0; j < 28; ++j) {
        na[j] = rnd();
        nb[j] = rnd();
        nc[j] = rnd();
    }
    const Block A = MakeBlock(2, 6, 0x00, na);
    const Block B = MakeBlock(3, 5, 0x04, nb);
    const Block C = MakeBlock(4, 7, 0x03, nc);
    const std::uint32_t base = 0x2000;

    for (std::uint16_t pitch : {std::uint16_t(0x1000), std::uint16_t(0x0C37), std::uint16_t(0x2345)}) {
        auto s = NewSpu();
        Put(*s, base, A);
        Put(*s, base + 16, B);
        Put(*s, base + 32, C);
        s->SetVoiceStartAddress(0, base / 8);
        s->SetVoicePitch(0, pitch);
        s->SetVoiceVolumeLeft(0, 0x3FFF);
        s->SetVoiceVolumeRight(0, 0);
        s->SetVoiceAdsr1(0, 0x00FF); // linear attack, shift 0 step 0; decay F; sustain level F
        s->SetVoiceAdsr2(0, 0x0000); // sustain linear increase, shift 0
        s->KeyOn(1);

        // The reference stream: A B C, then (B C) again and again, history carried;
        // and the same with the history reset at each jump, which must differ.
        std::vector<int> carried, reset;
        {
            int old = 0, older = 0;
            RefDecode(A, carried, old, older);
            RefDecode(B, carried, old, older);
            RefDecode(C, carried, old, older);
            for (int k = 0; k < 40; ++k) {
                RefDecode(B, carried, old, older);
                RefDecode(C, carried, old, older);
            }
            int o2 = 0, p2 = 0;
            RefDecode(A, reset, o2, p2);
            RefDecode(B, reset, o2, p2);
            RefDecode(C, reset, o2, p2);
            for (int k = 0; k < 40; ++k) {
                o2 = p2 = 0;
                RefDecode(B, reset, o2, p2);
                RefDecode(C, reset, o2, p2);
            }
        }
        bool second_pass_differs = false;
        for (int j = 0; j < 28; ++j) second_pass_differs |= carried[28 + j] != carried[84 + j];
        CHECK(second_pass_differs);

        auto at = [&](const std::vector<int>& v, long long k) { return k < 0 ? 0 : v[static_cast<size_t>(k)]; };
        long long g = 0; // the reference pitch counter, never wrapped
        bool reset_differs = false;
        const int frames = 28 * 60;
        for (int f = 0; f < frames; ++f) {
            const long long k = g >> 12;
            if (k + 1 >= static_cast<long long>(carried.size())) break;
            const int i = static_cast<int>((g >> 4) & 0xFF);
            const int env = s->VoiceEnvelope(0);
            const int interp = RefGauss(at(carried, k - 3), at(carried, k - 2), at(carried, k - 1), at(carried, k), i);
            const int interp_reset = RefGauss(at(reset, k - 3), at(reset, k - 2), at(reset, k - 1), at(reset, k), i);
            auto final_left = [&](int sample) {
                long long outx = FloorShift15(static_cast<long long>(sample) * env);
                long long left = FloorShift15(outx * 0x7FFE);
                return static_cast<int>(FloorShift15(left * 0x7FFE));
            };
            std::int16_t out[2];
            s->Render(out, 1);
            CHECK_EQ(out[0], final_left(interp));
            CHECK_EQ(out[1], 0);
            if (final_left(interp) != final_left(interp_reset)) reset_differs = true;
            g += pitch;
        }
        CHECK(reset_differs);
        CHECK_EQ(s->VoiceLoopAddress(0), (base + 16) / 8); // set by B's loop-start flag
        CHECK_EQ(s->Endx() & 1, 1u);
    }
}

// A one-shot: A then B with flags 1 (end, no repeat): at B's end ENDX is set,
// the envelope forced to zero and the voice to release; key on clears ENDX.
void TestOneShotEnd() {
    auto s = NewSpu();
    const std::uint32_t base = 0x3000;
    Put(*s, base, MakeBlockFill(0, 4, 0x00, 3));
    Put(*s, base + 16, MakeBlockFill(0, 4, 0x01, 3));
    Put(*s, base + 32, MakeBlockFill(0, 0, 0x07, 0)); // a silent self-loop, unused here
    s->SetVoiceStartAddress(5, base / 8);
    s->SetVoiceLoopAddress(5, (base + 32) / 8);
    s->SetVoicePitch(5, 0x1000);
    s->SetVoiceAdsr1(5, 0x00FF);
    s->SetVoiceAdsr2(5, 0x0000);
    s->KeyOn(1u << 5);
    std::int16_t out[2];
    for (int t = 0; t < 55; ++t) {
        s->Render(out, 1);
        CHECK_EQ(s->Endx(), 0u);
    }
    s->Render(out, 1); // the 56th sample ends block B
    CHECK_EQ(s->Endx(), 1u << 5);
    CHECK_EQ(s->VoiceEnvelope(5), 0);
    CHECK(s->VoiceAdsrPhase(5) >= 3);
    CHECK_EQ(s->VoiceLoopAddress(5), (base + 32) / 8);
    s->KeyOn(1u << 5);
    s->Render(out, 1);
    CHECK_EQ(s->Endx(), 0u);
    CHECK_EQ(s->VoiceAdsrPhase(5), 0);
}

// Envelope timings, each worked out from the envelope operation of psx-spx
// ("Envelope Operation depending on Shift/Step/Mode/Direction").
int TicksToFull(std::uint16_t adsr1) {
    auto s = NewSpu();
    Put(*s, 0x1000, MakeBlockFill(0, 0, 0x07, 0));
    s->SetVoiceStartAddress(0, 0x1000 / 8);
    s->SetVoiceAdsr1(0, adsr1);
    s->SetVoiceAdsr2(0, 0x1F1F);
    s->KeyOn(1);
    std::int16_t out[2];
    for (int t = 1; t < 200000; ++t) {
        s->Render(out, 1);
        if (s->VoiceEnvelope(0) == 0x7FFF) return t;
    }
    return -1;
}

void TestAdsr() {
    // Linear attack, shift 0 step 0 (+7 << 11 = 14336 a sample): 14336, 28672, 32767.
    {
        auto s = NewSpu();
        Put(*s, 0x1000, MakeBlockFill(0, 0, 0x07, 0));
        s->SetVoiceStartAddress(0, 0x1000 / 8);
        s->SetVoiceAdsr1(0, 0x00FF);
        s->SetVoiceAdsr2(0, 0x1F1F);
        s->KeyOn(1);
        std::int16_t out[2];
        const int expect[3] = {14336, 28672, 32767};
        for (int e : expect) {
            s->Render(out, 1);
            CHECK_EQ(s->VoiceEnvelope(0), e);
        }
        CHECK_EQ(s->VoiceAdsrPhase(0), 1); // decay
        // Key off; exponential release shift 0 (step -8 << 11, times level / 8000h)
        // halves the level each sample, flooring: 2^k - 1 down to 0 in 15 samples.
        s->SetVoiceAdsr2(0, 0x0020);
        s->KeyOff(1);
        for (int k = 14; k >= 0; --k) {
            s->Render(out, 1);
            CHECK_EQ(s->VoiceEnvelope(0), (1 << k) - 1);
        }
        CHECK_EQ(s->VoiceAdsrPhase(0), 4);
    }
    // Linear attack shift 11 step 0: +7 every sample, 7 x 4681 = 32767.
    CHECK_EQ(TicksToFull(0x2C00 | 0x00FF), 4681);
    // Linear attack shift 13 step 3: +4 every 4 samples (counter += 8000h >> 2): 8192 steps.
    CHECK_EQ(TicksToFull((13 << 10) | (3 << 8) | 0x00FF), 32768);
    // Exponential attack shift 11 step 0: +7 a sample until the level passes
    // 6000h (3511 samples, level 24577), then +7 every 4 samples (counter
    // increment / 4): 8190 / 7 = 1170 steps, 4680 samples. 8191 in all.
    CHECK_EQ(TicksToFull(0x8000 | (11 << 10) | 0x00FF), 8191);
    // Exponential attack shift 2 step 0: +3584 (7 << 9) for 7 samples (25088),
    // then +896 (step / 4): 9 more. 16 in all.
    CHECK_EQ(TicksToFull(0x8000 | (2 << 10) | 0x00FF), 16);
    // Exponential attack shift 10 step 0: +14 for 1756 samples (24584), then +7
    // every 2 samples (both halved): 1169 steps, 2338 samples. 4094 in all.
    CHECK_EQ(TicksToFull(0x8000 | (10 << 10) | 0x00FF), 4094);
    // Shifts past 26 floor the counter increment at 1 (psx-spx: "a rate of 0x76
    // behaves like 0x6A"): both take one +5 step every 8000h samples.
    {
        for (int shift : {26, 29}) {
            auto s = NewSpu();
            Put(*s, 0x1000, MakeBlockFill(0, 0, 0x07, 0));
            s->SetVoiceStartAddress(0, 0x1000 / 8);
            s->SetVoiceAdsr1(0, static_cast<std::uint16_t>((shift << 10) | (2 << 8) | 0xFF));
            s->KeyOn(1);
            std::vector<std::int16_t> buf(2 * 0x8000);
            s->Render(buf.data(), 0x7FFF);
            CHECK_EQ(s->VoiceEnvelope(0), 0);
            s->Render(buf.data(), 1);
            CHECK_EQ(s->VoiceEnvelope(0), 5);
        }
    }
    // Linear release shift 11: -8 a sample from 32767: 4096 samples to 0.
    // Decay exponential shift 0 to sustain level 3 (4 x 800h = 8192):
    // 32767 -> 16383 -> 8191, then sustain.
    {
        auto s = NewSpu();
        Put(*s, 0x1000, MakeBlockFill(0, 0, 0x07, 0));
        s->SetVoiceStartAddress(0, 0x1000 / 8);
        s->SetVoiceAdsr1(0, 0x0003);              // attack shift 0, decay shift 0, sustain level 3
        s->SetVoiceAdsr2(0, (0x1F << 8) | (3 << 6) | 11); // sustain rate 7Fh (never steps), linear release shift 11
        s->KeyOn(1);
        std::int16_t out[2];
        s->Render(out, 3);
        CHECK_EQ(s->VoiceEnvelope(0), 32767);
        s->Render(out, 1);
        CHECK_EQ(s->VoiceEnvelope(0), 16383);
        CHECK_EQ(s->VoiceAdsrPhase(0), 1);
        s->Render(out, 1);
        CHECK_EQ(s->VoiceEnvelope(0), 8191);
        CHECK_EQ(s->VoiceAdsrPhase(0), 2);
        std::vector<std::int16_t> buf(2 * 1000);
        s->Render(buf.data(), 1000);
        CHECK_EQ(s->VoiceEnvelope(0), 8191); // rate 7Fh: "the volume never steps"
        s->KeyOff(1);
        int t = 0;
        while (s->VoiceEnvelope(0) != 0 && t < 10000) {
            s->Render(out, 1);
            ++t;
        }
        CHECK_EQ(t, 1024); // 8191 / 8 rounded up
        auto s2 = NewSpu();
        Put(*s2, 0x1000, MakeBlockFill(0, 0, 0x07, 0));
        s2->SetVoiceStartAddress(0, 0x1000 / 8);
        s2->SetVoiceAdsr1(0, 0x00FF);
        s2->SetVoiceAdsr2(0, 11);
        s2->KeyOn(1);
        s2->Render(out, 3);
        s2->KeyOff(1);
        t = 0;
        while (s2->VoiceEnvelope(0) != 0 && t < 10000) {
            s2->Render(out, 1);
            ++t;
        }
        CHECK_EQ(t, 4096);
    }
    // Release shift 1Fh: the field all ones, the level never steps (reading R6).
    {
        auto s = NewSpu();
        Put(*s, 0x1000, MakeBlockFill(0, 0, 0x07, 0));
        s->SetVoiceStartAddress(0, 0x1000 / 8);
        s->SetVoiceAdsr1(0, 0x00FF);
        s->SetVoiceAdsr2(0, 0x001F);
        s->KeyOn(1);
        std::int16_t out[2];
        s->Render(out, 3);
        s->KeyOff(1);
        std::vector<std::int16_t> buf(2 * 70000);
        s->Render(buf.data(), 70000);
        CHECK_EQ(s->VoiceEnvelope(0), 32767);
    }
}

void TestVolumeSweep() {
    auto s = NewSpu();
    s->SetVoiceVolumeLeft(0, 0x0000);
    s->SetVoiceVolumeLeft(0, 0x8000); // sweep: linear increase, shift 0 step 0
    s->SetVoiceVolumeRight(0, 0x2000);
    CHECK_EQ(s->VoiceCurrentVolumeRight(0), 0x4000); // fixed: bits 0..14 are volume / 2
    s->SetVoiceVolumeRight(0, 0x7FFF);
    CHECK_EQ(static_cast<std::int16_t>(s->VoiceCurrentVolumeRight(0)), -2); // -1 x 2
    std::int16_t out[2];
    const int expect[3] = {14336, 28672, 32767};
    for (int e : expect) {
        s->Render(out, 1);
        CHECK_EQ(s->VoiceCurrentVolumeLeft(0), e);
    }
    // Linear decrease shift 0 step 0 (-8 << 11): 32767 -> 16383 -> 0.
    s->SetVoiceVolumeLeft(0, 0xA000);
    s->Render(out, 1);
    CHECK_EQ(s->VoiceCurrentVolumeLeft(0), 16383);
    s->Render(out, 1);
    CHECK_EQ(s->VoiceCurrentVolumeLeft(0), 0);
}

// Reference noise generator, from psx-spx's "SPU Noise Generator".
struct RefNoise {
    int timer = 0;
    unsigned level = 0;
    void Tick(int shift, int step) {
        timer -= step + 4;
        unsigned parity = 1 ^ ((level >> 15) & 1) ^ ((level >> 12) & 1) ^ ((level >> 11) & 1) ^ ((level >> 10) & 1);
        if (timer < 0) {
            level = ((level * 2) + parity) & 0xFFFF;
            timer += 0x20000 >> shift;
            if (timer < 0) timer += 0x20000 >> shift;
        }
    }
};

void TestNoise() {
    struct Setting {
        int shift, step;
    } settings[] = {{0, 0}, {6, 1}, {10, 2}, {15, 3}, {15, 0}, {12, 3}};
    for (const Setting& st : settings) {
        auto s = NewSpu();
        s->SetControl(static_cast<std::uint16_t>(0xC000 | (st.shift << 10) | (st.step << 8)));
        RefNoise ref;
        std::vector<std::int16_t> out(2);
        int changes = 0;
        unsigned prev = 0;
        const int ticks = st.shift == 0 ? 0x40000 : 20000;
        for (int t = 0; t < ticks; ++t) {
            s->Render(out.data(), 1);
            ref.Tick(st.shift, st.step);
            CHECK_EQ(static_cast<std::uint16_t>(s->NoiseLevel()), ref.level);
            if (ref.level != prev) ++changes;
            prev = ref.level;
        }
        // The clock: one shift per (20000h >> shift) / (step + 4) samples.
        if (st.shift == 0 && st.step == 0) CHECK_EQ(changes, 0x40000 / 0x8000); // every 8000h samples
        if (st.shift == 15 && st.step == 3) CHECK(changes > ticks * 9 / 10);    // every sample (reload 4 < step 7)
    }
    // The LFSR's own period from zero (16-bit state, shifted left with the parity).
    unsigned level = 0, start = 0;
    std::vector<int> seen(0x10000, -1);
    int n = 0;
    while (seen[level] < 0) {
        seen[level] = n++;
        unsigned parity = 1 ^ ((level >> 15) & 1) ^ ((level >> 12) & 1) ^ ((level >> 11) & 1) ^ ((level >> 10) & 1);
        level = ((level << 1) | parity) & 0xFFFF;
    }
    start = static_cast<unsigned>(seen[level]);
    std::printf("  noise LFSR from 0: enters a cycle of %d states after %u\n", n - static_cast<int>(start), start);
    CHECK(n - static_cast<int>(start) > 1000);
}

// Pitch modulation: voice 1 (captured to SPU RAM 800h..BFFh after its
// envelope) modulates voice 2. The reference replays psx-spx's "Pitch Counter"
// from the captured amplitudes and predicts the sample at which voice 2 ends
// its sample (ENDX).
void TestPitchModulation() {
    auto s = NewSpu();
    Put(*s, 0x1000, MakeBlockFill(0, 0, 0x07, 7)); // DC 28672, looping on itself
    for (int k = 0; k < 6; ++k) Put(*s, 0x2000 + 16 * static_cast<std::uint32_t>(k), MakeBlockFill(0, 0, k == 5 ? 0x03 : 0x00, 1));
    s->SetVoiceStartAddress(1, 0x1000 / 8);
    s->SetVoiceAdsr1(1, (4 << 10) | 0xFF); // linear attack over ~37 samples: the modulator rises
    s->SetVoiceAdsr2(1, 0x1FDF);
    s->SetVoicePitch(1, 0x1000);
    s->SetVoiceStartAddress(2, 0x2000 / 8);
    s->SetVoiceLoopAddress(2, 0x2000 / 8);
    s->SetVoiceAdsr1(2, 0x00FF);
    s->SetVoicePitch(2, 0x1000);
    s->SetPitchModulation(1u << 2);
    s->KeyOn((1u << 1) | (1u << 2));
    long long counter = 0;
    int predicted = -1, actual = -1;
    std::int16_t out[2];
    for (int t = 0; t < 400 && (predicted < 0 || actual < 0); ++t) {
        s->Render(out, 1);
        const int amp = Capture(*s, 0x800, static_cast<std::uint32_t>(t));
        long long step = static_cast<std::int16_t>(0x1000);
        step = FloorShift15(step * (amp + 0x8000)) & 0xFFFF;
        if (step > 0x3FFF) step = 0x4000;
        counter += step;
        if (predicted < 0 && counter >= (6LL * 28) << 12) predicted = t;
        if (actual < 0 && (s->Endx() & (1u << 2))) actual = t;
    }
    CHECK(predicted > 0);
    CHECK_EQ(actual, predicted);
    CHECK(predicted < 6 * 28 - 20); // modulated up: it ends well before 168 samples
}

// Reverb. The comb taps read what the same-side reflection wrote D 8-byte
// units earlier in the work area: 4D steps at 22,050 Hz, 8D samples; the two
// all-pass stages with zero volume are pure delays of dAPF units; the two
// 39-tap resamplers add 19 samples each (psx-spx, "Reverb Precision").
void TestReverbStructure() {
    auto s = NewSpu();
    s->SetControl(0xC080);
    // A short impulse: one nonzero sample (range 0, nibble 7) then silence.
    int imp[28] = {7};
    Put(*s, 0x1000, MakeBlock(0, 0, 0x00, imp));
    Put(*s, 0x1010, MakeBlockFill(0, 0, 0x07, 0));
    s->SetVoiceStartAddress(0, 0x1000 / 8);
    s->SetVoicePitch(0, 0x1000);
    s->SetVoiceAdsr1(0, 0x00FF);
    s->SetVoiceVolumeLeft(0, 0x3FFF);
    s->SetVoiceVolumeRight(0, 0x3FFF);
    s->SetReverbMode(1);
    s->SetReverbBase((0x80000 - 0x10000) / 8);
    const int D = 0x40;
    std::uint16_t r[32] = {};
    r[0] = 1;            // dAPF1
    r[1] = 1;            // dAPF2
    r[2] = 0x7FFF;       // vIIR
    r[3] = 0x7FFF;       // vCOMB1
    r[4] = 0x4000;       // vCOMB2
    r[10] = 0x200;       // mLSAME
    r[11] = 0x600;       // mRSAME
    r[12] = 0x200 - D;   // mLCOMB1
    r[13] = 0x600 - D;   // mRCOMB1
    r[14] = 0x200 - 2 * D; // mLCOMB2
    r[15] = 0x600 - 2 * D; // mRCOMB2
    r[18] = 0x300;       // mLDIFF
    r[19] = 0x700;       // mRDIFF
    r[20] = r[22] = 0x380; // mLCOMB3/4 (volume 0)
    r[21] = r[23] = 0x780;
    r[26] = 0x400;       // mLAPF1
    r[27] = 0x800;       // mRAPF1
    r[28] = 0x500;       // mLAPF2
    r[29] = 0x900;       // mRAPF2
    r[30] = 0x7FFF;      // vLIN
    r[31] = 0;           // vRIN: the right side hears nothing
    for (int i = 0; i < 32; ++i) s->SetReverbRegister(i, r[i]);
    s->SetReverbOutputVolume(0x7FFF, 0x7FFF);
    s->KeyOn(1);
    const int frames = 3000;
    std::vector<std::int16_t> out(2 * frames);
    s->Render(out.data(), frames);
    // The dry peak: the impulse through the Gaussian (3 samples late at i=0).
    int dry_peak = 0;
    for (int t = 0; t < 20; ++t)
        if (out[2 * t] > out[2 * dry_peak]) dry_peak = t;
    auto peak_in = [&](int from, int to) {
        int best = from;
        for (int t = from; t < to; ++t)
            if (out[2 * t] > out[2 * best]) best = t;
        return best;
    };
    const int delay1 = 38 + 8 * (D + 1 + 1);
    const int delay2 = 38 + 8 * (2 * D + 1 + 1);
    const int p1 = peak_in(dry_peak + delay1 - 20, dry_peak + delay1 + 20);
    const int p2 = peak_in(dry_peak + delay2 - 20, dry_peak + delay2 + 20);
    std::printf("  reverb: dry peak %d, echoes at %d (expected %d +-1) and %d (expected %d +-1), heights %d %d\n",
                dry_peak, p1, dry_peak + delay1, p2, dry_peak + delay2, out[2 * p1], out[2 * p2]);
    CHECK(p1 >= dry_peak + delay1 - 1 && p1 <= dry_peak + delay1 + 1);
    CHECK(p2 >= dry_peak + delay2 - 1 && p2 <= dry_peak + delay2 + 1);
    CHECK(out[2 * p1] > 1000);
    // vCOMB2 is half vCOMB1: the second echo about half the first.
    CHECK(out[2 * p2] * 2 > out[2 * p1] * 9 / 10 && out[2 * p2] * 2 < out[2 * p1] * 11 / 10);
    // Nothing between the dry sound and the first echo but the dry tail.
    // (The two resamplers spread an echo over +-38 samples around its peak.)
    for (int t = dry_peak + 10; t < dry_peak + delay1 - 38; ++t) CHECK(out[2 * t] == 0);
    // The right side: vRIN = 0, so only the dry sound.
    for (int t = dry_peak + 10; t < frames; ++t) CHECK(out[2 * t + 1] == 0);
}

// The standard presets: an impulse into each gives a non-zero, decaying tail
// that never reaches full scale (no runaway).
void TestReverbPresetsStable() {
    for (int p = 0; p < psx::kSpuReverbPresetCount; ++p) {
        const psx::SpuReverbPreset& preset = psx::kSpuReverbPresets[p];
        auto s = NewSpu();
        s->SetControl(0xC080);
        int imp[28] = {7, 7, 7, 7, 9, 9, 9, 9}; // a short burst
        Put(*s, 0x1000, MakeBlock(0, 0, 0x00, imp));
        Put(*s, 0x1010, MakeBlockFill(0, 0, 0x07, 0));
        s->SetVoiceStartAddress(0, 0x1000 / 8);
        s->SetVoicePitch(0, 0x1000);
        s->SetVoiceAdsr1(0, 0x00FF);
        s->SetVoiceVolumeLeft(0, 0x3FFF);
        s->SetVoiceVolumeRight(0, 0x3FFF);
        s->SetReverbMode(1);
        s->ApplyReverbPreset(preset);
        s->SetReverbOutputVolume(0x3FFF, 0x3FFF);
        s->KeyOn(1);
        const int frames = 44100 * 6;
        std::vector<std::int16_t> out(2 * static_cast<size_t>(frames));
        s->Render(out.data(), frames);
        auto peak = [&](int from, int to) {
            int m = 0;
            for (int t = from; t < to; ++t)
                for (int c = 0; c < 2; ++c) m = std::abs(out[2 * t + c]) > m ? std::abs(out[2 * t + c]) : m;
            return m;
        };
        const int wet_early = peak(100, 44100);
        const int late = peak(frames - 44100, frames);
        std::printf("  preset %-13s wet peak after the dry burst %5d, peak in the last second %5d\n", preset.name, wet_early, late);
        CHECK(peak(0, frames) < 32767);
        if (std::strcmp(preset.name, "Off") == 0) {
            CHECK_EQ(wet_early, 0);
        } else if (std::strcmp(preset.name, "Chaos Echo") == 0) {
            CHECK(wet_early > 0); // "almost infinite": no decay demanded
        } else {
            CHECK(wet_early > 50);
            CHECK(late * 4 < wet_early);
        }
    }
}

// Reverb master enable off: nothing written to the work area.
void TestReverbWriteDisable() {
    auto s = NewSpu();
    int imp[28] = {7, 7, 7, 7};
    Put(*s, 0x1000, MakeBlock(0, 0, 0x00, imp));
    Put(*s, 0x1010, MakeBlockFill(0, 0, 0x07, 0));
    s->SetVoiceStartAddress(0, 0x1000 / 8);
    s->SetVoicePitch(0, 0x1000);
    s->SetVoiceAdsr1(0, 0x00FF);
    s->SetVoiceVolumeLeft(0, 0x3FFF);
    s->SetReverbMode(1);
    s->ApplyReverbPreset(psx::kSpuReverbPresets[0]);
    s->KeyOn(1);
    std::vector<std::int16_t> out(2 * 20000);
    s->Render(out.data(), 20000);
    std::vector<std::uint8_t> area(psx::kSpuReverbPresets[0].size);
    s->ReadRam(Spu::kRamBytes - psx::kSpuReverbPresets[0].size, area.data(), static_cast<std::uint32_t>(area.size()));
    bool all_zero = true;
    for (std::uint8_t b : area) all_zero &= b == 0;
    CHECK(all_zero);
}

// The model aborts on what it does not implement.
bool Aborts(void (*fn)()) {
    std::fflush(stdout);
    pid_t pid = fork();
    if (pid == 0) {
        psx::SetSpuAbortHook([](const char*) {});
        fn();
        _exit(0);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    return WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT;
}

void TestAborts() {
    CHECK(Aborts([] {
        int z[28] = {0};
        Block blk = MakeBlock(5, 0, 0, z);
        std::int16_t out[28];
        std::int32_t hist[2] = {0, 0};
        Spu::DecodeBlock(blk.b, out, hist);
    }));
    CHECK(Aborts([] { Spu s; s.SetControl(0xC001); }));
    CHECK(Aborts([] { Spu s; s.SetControl(0xC040); }));
    CHECK(Aborts([] { Spu s; std::int16_t o[2]; s.Render(o, 1); }));
    CHECK(Aborts([] { Spu s; s.WriteRegister(0x1A4, 0); }));
    CHECK(Aborts([] {
        Spu s;
        s.SetControl(0xC080);
        s.SetReverbRegister(2, 0x8000);
        std::int16_t o[2];
        s.Render(o, 1);
    }));
    CHECK(!Aborts([] { Spu s; s.SetControl(0xC000); std::int16_t o[2]; s.Render(o, 1); }));
}

} // namespace

int main() {
    struct {
        const char* name;
        void (*fn)();
    } tests[] = {
        {"gauss table", TestGaussTable},
        {"adpcm decode", TestDecodeByHand},
        {"gaussian interpolation", TestGaussianByHand},
        {"loop history", TestLoopHistory},
        {"one-shot end", TestOneShotEnd},
        {"adsr", TestAdsr},
        {"volume sweep", TestVolumeSweep},
        {"noise", TestNoise},
        {"pitch modulation", TestPitchModulation},
        {"reverb structure", TestReverbStructure},
        {"reverb presets", TestReverbPresetsStable},
        {"reverb write disable", TestReverbWriteDisable},
        {"aborts", TestAborts},
    };
    for (const auto& t : tests) {
        const int before = g_failures;
        std::printf("%s\n", t.name);
        t.fn();
        std::printf("  %s\n", g_failures == before ? "ok" : "FAILED");
    }
    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}

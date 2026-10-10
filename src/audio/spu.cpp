// The SPU model. docs/spu-model.md is the companion: each block below names
// the psx-spx section it follows ("spec: <section>"), and the readings taken
// where the description leaves a choice are listed there as R1, R2, ...
#include "audio/spu.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace psx {
namespace {

void DefaultAbort(const char* message) { std::fprintf(stderr, "psx::Spu: %s\n", message); }
SpuAbortHook g_abort_hook = DefaultAbort;

[[noreturn]] void Fatal(const char* format, ...) {
    char message[256];
    va_list args;
    va_start(args, format);
    std::vsnprintf(message, sizeof message, format, args);
    va_end(args);
    g_abort_hook(message);
    std::abort();
}

inline std::int32_t Clamp16(std::int32_t x) { return x < -0x8000 ? -0x8000 : (x > 0x7FFF ? 0x7FFF : x); }
inline std::int32_t Mul15(std::int32_t a, std::int32_t b) { return Clamp16((a * b) >> 15); }

// spec: SPU ADPCM Pitch, "4-Point Gaussian Interpolation" - the 512 entries
// as listed (hardware data).
const std::int16_t kGauss[512] = {
    -0x001, -0x001, -0x001, -0x001, -0x001, -0x001, -0x001, -0x001,
    -0x001, -0x001, -0x001, -0x001, -0x001, -0x001, -0x001, -0x001,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0001,
    0x0001, 0x0001, 0x0001, 0x0002, 0x0002, 0x0002, 0x0003, 0x0003,
    0x0003, 0x0004, 0x0004, 0x0005, 0x0005, 0x0006, 0x0007, 0x0007,
    0x0008, 0x0009, 0x0009, 0x000A, 0x000B, 0x000C, 0x000D, 0x000E,
    0x000F, 0x0010, 0x0011, 0x0012, 0x0013, 0x0015, 0x0016, 0x0018,
    0x0019, 0x001B, 0x001C, 0x001E, 0x0020, 0x0021, 0x0023, 0x0025,
    0x0027, 0x0029, 0x002C, 0x002E, 0x0030, 0x0033, 0x0035, 0x0038,
    0x003A, 0x003D, 0x0040, 0x0043, 0x0046, 0x0049, 0x004D, 0x0050,
    0x0054, 0x0057, 0x005B, 0x005F, 0x0063, 0x0067, 0x006B, 0x006F,
    0x0074, 0x0078, 0x007D, 0x0082, 0x0087, 0x008C, 0x0091, 0x0096,
    0x009C, 0x00A1, 0x00A7, 0x00AD, 0x00B3, 0x00BA, 0x00C0, 0x00C7,
    0x00CD, 0x00D4, 0x00DB, 0x00E3, 0x00EA, 0x00F2, 0x00FA, 0x0101,
    0x010A, 0x0112, 0x011B, 0x0123, 0x012C, 0x0135, 0x013F, 0x0148,
    0x0152, 0x015C, 0x0166, 0x0171, 0x017B, 0x0186, 0x0191, 0x019C,
    0x01A8, 0x01B4, 0x01C0, 0x01CC, 0x01D9, 0x01E5, 0x01F2, 0x0200,
    0x020D, 0x021B, 0x0229, 0x0237, 0x0246, 0x0255, 0x0264, 0x0273,
    0x0283, 0x0293, 0x02A3, 0x02B4, 0x02C4, 0x02D6, 0x02E7, 0x02F9,
    0x030B, 0x031D, 0x0330, 0x0343, 0x0356, 0x036A, 0x037E, 0x0392,
    0x03A7, 0x03BC, 0x03D1, 0x03E7, 0x03FC, 0x0413, 0x042A, 0x0441,
    0x0458, 0x0470, 0x0488, 0x04A0, 0x04B9, 0x04D2, 0x04EC, 0x0506,
    0x0520, 0x053B, 0x0556, 0x0572, 0x058E, 0x05AA, 0x05C7, 0x05E4,
    0x0601, 0x061F, 0x063E, 0x065C, 0x067C, 0x069B, 0x06BB, 0x06DC,
    0x06FD, 0x071E, 0x0740, 0x0762, 0x0784, 0x07A7, 0x07CB, 0x07EF,
    0x0813, 0x0838, 0x085D, 0x0883, 0x08A9, 0x08D0, 0x08F7, 0x091E,
    0x0946, 0x096F, 0x0998, 0x09C1, 0x09EB, 0x0A16, 0x0A40, 0x0A6C,
    0x0A98, 0x0AC4, 0x0AF1, 0x0B1E, 0x0B4C, 0x0B7A, 0x0BA9, 0x0BD8,
    0x0C07, 0x0C38, 0x0C68, 0x0C99, 0x0CCB, 0x0CFD, 0x0D30, 0x0D63,
    0x0D97, 0x0DCB, 0x0E00, 0x0E35, 0x0E6B, 0x0EA1, 0x0ED7, 0x0F0F,
    0x0F46, 0x0F7F, 0x0FB7, 0x0FF1, 0x102A, 0x1065, 0x109F, 0x10DB,
    0x1116, 0x1153, 0x118F, 0x11CD, 0x120B, 0x1249, 0x1288, 0x12C7,
    0x1307, 0x1347, 0x1388, 0x13C9, 0x140B, 0x144D, 0x1490, 0x14D4,
    0x1517, 0x155C, 0x15A0, 0x15E6, 0x162C, 0x1672, 0x16B9, 0x1700,
    0x1747, 0x1790, 0x17D8, 0x1821, 0x186B, 0x18B5, 0x1900, 0x194B,
    0x1996, 0x19E2, 0x1A2E, 0x1A7B, 0x1AC8, 0x1B16, 0x1B64, 0x1BB3,
    0x1C02, 0x1C51, 0x1CA1, 0x1CF1, 0x1D42, 0x1D93, 0x1DE5, 0x1E37,
    0x1E89, 0x1EDC, 0x1F2F, 0x1F82, 0x1FD6, 0x202A, 0x207F, 0x20D4,
    0x2129, 0x217F, 0x21D5, 0x222C, 0x2282, 0x22DA, 0x2331, 0x2389,
    0x23E1, 0x2439, 0x2492, 0x24EB, 0x2545, 0x259E, 0x25F8, 0x2653,
    0x26AD, 0x2708, 0x2763, 0x27BE, 0x281A, 0x2876, 0x28D2, 0x292E,
    0x298B, 0x29E7, 0x2A44, 0x2AA1, 0x2AFF, 0x2B5C, 0x2BBA, 0x2C18,
    0x2C76, 0x2CD4, 0x2D33, 0x2D91, 0x2DF0, 0x2E4F, 0x2EAE, 0x2F0D,
    0x2F6C, 0x2FCC, 0x302B, 0x308B, 0x30EA, 0x314A, 0x31AA, 0x3209,
    0x3269, 0x32C9, 0x3329, 0x3389, 0x33E9, 0x3449, 0x34A9, 0x3509,
    0x3569, 0x35C9, 0x3629, 0x3689, 0x36E8, 0x3748, 0x37A8, 0x3807,
    0x3867, 0x38C6, 0x3926, 0x3985, 0x39E4, 0x3A43, 0x3AA2, 0x3B00,
    0x3B5F, 0x3BBD, 0x3C1B, 0x3C79, 0x3CD7, 0x3D35, 0x3D92, 0x3DEF,
    0x3E4C, 0x3EA9, 0x3F05, 0x3F62, 0x3FBD, 0x4019, 0x4074, 0x40D0,
    0x412A, 0x4185, 0x41DF, 0x4239, 0x4292, 0x42EB, 0x4344, 0x439C,
    0x43F4, 0x444C, 0x44A3, 0x44FA, 0x4550, 0x45A6, 0x45FC, 0x4651,
    0x46A6, 0x46FA, 0x474E, 0x47A1, 0x47F4, 0x4846, 0x4898, 0x48E9,
    0x493A, 0x498A, 0x49D9, 0x4A29, 0x4A77, 0x4AC5, 0x4B13, 0x4B5F,
    0x4BAC, 0x4BF7, 0x4C42, 0x4C8D, 0x4CD7, 0x4D20, 0x4D68, 0x4DB0,
    0x4DF7, 0x4E3E, 0x4E84, 0x4EC9, 0x4F0E, 0x4F52, 0x4F95, 0x4FD7,
    0x5019, 0x505A, 0x509A, 0x50DA, 0x5118, 0x5156, 0x5194, 0x51D0,
    0x520C, 0x5247, 0x5281, 0x52BA, 0x52F3, 0x532A, 0x5361, 0x5397,
    0x53CC, 0x5401, 0x5434, 0x5467, 0x5499, 0x54CA, 0x54FA, 0x5529,
    0x5558, 0x5585, 0x55B2, 0x55DE, 0x5609, 0x5632, 0x565B, 0x5684,
    0x56AB, 0x56D1, 0x56F6, 0x571B, 0x573E, 0x5761, 0x5782, 0x57A3,
    0x57C3, 0x57E2, 0x57FF, 0x581C, 0x5838, 0x5853, 0x586D, 0x5886,
    0x589E, 0x58B5, 0x58CB, 0x58E0, 0x58F4, 0x5907, 0x5919, 0x592A,
    0x593A, 0x5949, 0x5958, 0x5965, 0x5971, 0x597C, 0x5986, 0x598F,
    0x5997, 0x599E, 0x59A4, 0x59A9, 0x59AD, 0x59B0, 0x59B2, 0x59B3,
};

// spec: SPU Reverb Formula, "Reverb Buffer Resampling" - the 39-tap FIR.
const std::int32_t kReverbFir[39] = {
    -0x0001, 0x0000, 0x0002, 0x0000, -0x000A, 0x0000, 0x0023, 0x0000,
    -0x0067, 0x0000, 0x010A, 0x0000, -0x0268, 0x0000, 0x0534, 0x0000,
    -0x0B90, 0x0000, 0x2806, 0x4000, 0x2806, 0x0000, -0x0B90, 0x0000,
    0x0534, 0x0000, -0x0268, 0x0000, 0x010A, 0x0000, -0x0067, 0x0000,
    0x0023, 0x0000, -0x000A, 0x0000, 0x0002, 0x0000, -0x0001,
};

// spec: CDROM Format, "Pos/neg Tables" (SPU-ADPCM has five filters, 0..4).
const std::int32_t kFilterPos[5] = {0, 60, 115, 98, 122};
const std::int32_t kFilterNeg[5] = {0, 0, -52, -55, -60};

// The envelope operation shared by ADSR and the volume sweeps. spec: SPU
// Volume and ADSR Generator, "Envelope Operation depending on
// Shift/Step/Mode/Direction". `all_ones` is that section's ALL_BITS test,
// decided by the caller for the field the phase reads (R6).
struct EnvRate {
    bool exponential;
    bool decreasing;
    bool negative;   // sweep phase bit; ADSR is always positive
    int shift;       // 0..31
    int step;        // 0..3, the 2-bit field ("+7,+6,+5,+4")
    bool all_ones;
};

// Returns true when a step was applied this tick.
bool EnvTick(std::int32_t& level, std::uint32_t& counter, const EnvRate& r) {
    std::int32_t step = 7 - r.step;
    if (r.decreasing != r.negative) step = ~step;
    if (r.shift < 11) step *= 1 << (11 - r.shift);
    std::int32_t increment = r.shift > 11 ? (0x8000 >> (r.shift - 11)) : 0x8000;
    if (r.exponential && !r.decreasing && level > 0x6000) {
        if (r.shift < 10) {
            step >>= 2;
        } else if (r.shift >= 11) {
            increment >>= 2;
        } else {
            step >>= 1;
            increment >>= 1;
        }
    } else if (r.exponential && r.decreasing) {
        step = (step * level) >> 15; // R5: an arithmetic shift, floor
    }
    if (!r.all_ones && increment < 1) increment = 1;
    counter += static_cast<std::uint32_t>(increment);
    if ((counter & 0x8000) == 0) return false;
    counter &= 0x7FFF; // R7
    level += step;
    if (!r.decreasing) {
        level = Clamp16(level);
    } else if (r.negative) {
        level = level < -0x8000 ? -0x8000 : (level > 0 ? 0 : level);
    } else if (level < 0) {
        level = 0;
    }
    return true;
}

enum Phase { kAttack = 0, kDecay = 1, kSustain = 2, kRelease = 3, kOff = 4 };

} // namespace

void SetSpuAbortHook(SpuAbortHook hook) { g_abort_hook = hook ? hook : DefaultAbort; }

// spec: SPU Reverb Examples. The register order is 0x1F801DC0..0x1F801DFE.
const SpuReverbPreset kSpuReverbPresets[] = {
    {"Room", 0x26C0,
     {0x007D, 0x005B, 0x6D80, 0x54B8, 0xBED0, 0x0000, 0x0000, 0xBA80,
      0x5800, 0x5300, 0x04D6, 0x0333, 0x03F0, 0x0227, 0x0374, 0x01EF,
      0x0334, 0x01B5, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
      0x0000, 0x0000, 0x01B4, 0x0136, 0x00B8, 0x005C, 0x8000, 0x8000}},
    {"Studio Small", 0x1F40,
     {0x0033, 0x0025, 0x70F0, 0x4FA8, 0xBCE0, 0x4410, 0xC0F0, 0x9C00,
      0x5280, 0x4EC0, 0x03E4, 0x031B, 0x03A4, 0x02AF, 0x0372, 0x0266,
      0x031C, 0x025D, 0x025C, 0x018E, 0x022F, 0x0135, 0x01D2, 0x00B7,
      0x018F, 0x00B5, 0x00B4, 0x0080, 0x004C, 0x0026, 0x8000, 0x8000}},
    {"Studio Medium", 0x4840,
     {0x00B1, 0x007F, 0x70F0, 0x4FA8, 0xBCE0, 0x4510, 0xBEF0, 0xB4C0,
      0x5280, 0x4EC0, 0x0904, 0x076B, 0x0824, 0x065F, 0x07A2, 0x0616,
      0x076C, 0x05ED, 0x05EC, 0x042E, 0x050F, 0x0305, 0x0462, 0x02B7,
      0x042F, 0x0265, 0x0264, 0x01B2, 0x0100, 0x0080, 0x8000, 0x8000}},
    {"Studio Large", 0x6FE0,
     {0x00E3, 0x00A9, 0x6F60, 0x4FA8, 0xBCE0, 0x4510, 0xBEF0, 0xA680,
      0x5680, 0x52C0, 0x0DFB, 0x0B58, 0x0D09, 0x0A3C, 0x0BD9, 0x0973,
      0x0B59, 0x08DA, 0x08D9, 0x05E9, 0x07EC, 0x04B0, 0x06EF, 0x03D2,
      0x05EA, 0x031D, 0x031C, 0x0238, 0x0154, 0x00AA, 0x8000, 0x8000}},
    {"Hall", 0xADE0,
     {0x01A5, 0x0139, 0x6000, 0x5000, 0x4C00, 0xB800, 0xBC00, 0xC000,
      0x6000, 0x5C00, 0x15BA, 0x11BB, 0x14C2, 0x10BD, 0x11BC, 0x0DC1,
      0x11C0, 0x0DC3, 0x0DC0, 0x09C1, 0x0BC4, 0x07C1, 0x0A00, 0x06CD,
      0x09C2, 0x05C1, 0x05C0, 0x041A, 0x0274, 0x013A, 0x8000, 0x8000}},
    {"Half Echo", 0x3C00,
     {0x0017, 0x0013, 0x70F0, 0x4FA8, 0xBCE0, 0x4510, 0xBEF0, 0x8500,
      0x5F80, 0x54C0, 0x0371, 0x02AF, 0x02E5, 0x01DF, 0x02B0, 0x01D7,
      0x0358, 0x026A, 0x01D6, 0x011E, 0x012D, 0x00B1, 0x011F, 0x0059,
      0x01A0, 0x00E3, 0x0058, 0x0040, 0x0028, 0x0014, 0x8000, 0x8000}},
    {"Space Echo", 0xF6C0,
     {0x033D, 0x0231, 0x7E00, 0x5000, 0xB400, 0xB000, 0x4C00, 0xB000,
      0x6000, 0x5400, 0x1ED6, 0x1A31, 0x1D14, 0x183B, 0x1BC2, 0x16B2,
      0x1A32, 0x15EF, 0x15EE, 0x1055, 0x1334, 0x0F2D, 0x11F6, 0x0C5D,
      0x1056, 0x0AE1, 0x0AE0, 0x07A2, 0x0464, 0x0232, 0x8000, 0x8000}},
    {"Chaos Echo", 0x18040,
     {0x0001, 0x0001, 0x7FFF, 0x7FFF, 0x0000, 0x0000, 0x0000, 0x8100,
      0x0000, 0x0000, 0x1FFF, 0x0FFF, 0x1005, 0x0005, 0x0000, 0x0000,
      0x1005, 0x0005, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
      0x0000, 0x0000, 0x1004, 0x1002, 0x0004, 0x0002, 0x8000, 0x8000}},
    {"Delay", 0x18040,
     {0x0001, 0x0001, 0x7FFF, 0x7FFF, 0x0000, 0x0000, 0x0000, 0x0000,
      0x0000, 0x0000, 0x1FFF, 0x0FFF, 0x1005, 0x0005, 0x0000, 0x0000,
      0x1005, 0x0005, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
      0x0000, 0x0000, 0x1004, 0x1002, 0x0004, 0x0002, 0x8000, 0x8000}},
    {"Off", 0x10,
     {0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
      0x0000, 0x0000, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001,
      0x0000, 0x0000, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001,
      0x0000, 0x0000, 0x0001, 0x0001, 0x0001, 0x0001, 0x0000, 0x0000}},
};
const int kSpuReverbPresetCount = static_cast<int>(sizeof kSpuReverbPresets / sizeof kSpuReverbPresets[0]);

// Reverb register indices (0x1F801DC0 + 2 * index). spec: SPU Reverb Registers.
namespace {
enum ReverbReg {
    dAPF1, dAPF2, vIIR, vCOMB1, vCOMB2, vCOMB3, vCOMB4, vWALL,
    vAPF1, vAPF2, mLSAME, mRSAME, mLCOMB1, mRCOMB1, mLCOMB2, mRCOMB2,
    dLSAME, dRSAME, mLDIFF, mRDIFF, mLCOMB3, mRCOMB3, mLCOMB4, mRCOMB4,
    dLDIFF, dRDIFF, mLAPF1, mRAPF1, mLAPF2, mRAPF2, vLIN, vRIN,
};
} // namespace

Spu::Spu() { Reset(); }

void Spu::Reset() {
    std::memset(ram_, 0, sizeof ram_);
    for (Voice& voice : voices_) voice = Voice();
    pending_key_on_ = pending_key_off_ = 0;
    std::memset(key_on_wait_, 0, sizeof key_on_wait_);
    pmon_ = non_ = eon_ = endx_ = 0;
    mix_mask_ = 0xFFFFFF;
    main_vol_[0] = main_vol_[1] = Volume();
    evol_[0] = evol_[1] = 0;
    esa_ = 0;
    std::memset(reverb_, 0, sizeof reverb_);
    attr_ = 0;
    reverb_address_ = 0;
    noise_level_ = 0;
    noise_timer_ = 0;
    tick_ = 0;
    capture_index_ = 0;
    std::memset(rev_in_, 0, sizeof rev_in_);
    std::memset(rev_out_, 0, sizeof rev_out_);
}

void Spu::CheckVoice(int v) const {
    if (v < 0 || v >= kVoices) Fatal("voice %d out of range 0..23", v);
}

void Spu::WriteRam(std::uint32_t address, const void* data, std::uint32_t size) {
    if (address >= kRamBytes || size > kRamBytes - address)
        Fatal("WriteRam 0x%X+0x%X past the 512 KiB of SPU RAM", address, size);
    const std::uint8_t* src = static_cast<const std::uint8_t*>(data);
    for (std::uint32_t i = 0; i < size; ++i) {
        std::uint32_t a = address + i;
        std::uint16_t& half = ram_[a >> 1];
        if (a & 1)
            half = static_cast<std::uint16_t>((half & 0x00FF) | (src[i] << 8));
        else
            half = static_cast<std::uint16_t>((half & 0xFF00) | src[i]);
    }
}

void Spu::ReadRam(std::uint32_t address, void* data, std::uint32_t size) const {
    if (address >= kRamBytes || size > kRamBytes - address)
        Fatal("ReadRam 0x%X+0x%X past the 512 KiB of SPU RAM", address, size);
    std::uint8_t* dst = static_cast<std::uint8_t*>(data);
    for (std::uint32_t i = 0; i < size; ++i) {
        std::uint32_t a = address + i;
        dst[i] = static_cast<std::uint8_t>(ram_[a >> 1] >> ((a & 1) * 8));
    }
}

// --- registers -------------------------------------------------------------

// spec: SPU Volume and ADSR Generator, VOLL/VOLR/MVOLL/MVOLR. Bit 15 clear:
// fixed volume, bits 0..14 are volume/2. Bit 15 set: sweep from the current
// level (R10).
void Spu::WriteVolume(Volume& vol, std::uint16_t value) {
    vol.reg = value;
    vol.env.counter = 0;
    if ((value & 0x8000) == 0) vol.env.level = static_cast<std::int16_t>(value << 1);
}

void Spu::SetVoiceVolumeLeft(int v, std::uint16_t value) { CheckVoice(v); WriteVolume(voices_[v].vol[0], value); }
void Spu::SetVoiceVolumeRight(int v, std::uint16_t value) { CheckVoice(v); WriteVolume(voices_[v].vol[1], value); }
void Spu::SetVoicePitch(int v, std::uint16_t value) { CheckVoice(v); voices_[v].pitch = value; }
void Spu::SetVoiceStartAddress(int v, std::uint16_t value) { CheckVoice(v); voices_[v].ssa = value; }
void Spu::SetVoiceAdsr1(int v, std::uint16_t value) { CheckVoice(v); voices_[v].adsr1 = value; }
void Spu::SetVoiceAdsr2(int v, std::uint16_t value) { CheckVoice(v); voices_[v].adsr2 = value; }
void Spu::SetVoiceLoopAddress(int v, std::uint16_t value) { CheckVoice(v); voices_[v].lsa = value; }

std::uint16_t Spu::VoiceEnvelope(int v) const { CheckVoice(v); return static_cast<std::uint16_t>(voices_[v].adsr.level); }
std::uint16_t Spu::VoiceLoopAddress(int v) const { CheckVoice(v); return voices_[v].lsa; }
std::uint16_t Spu::VoiceCurrentVolumeLeft(int v) const { CheckVoice(v); return static_cast<std::uint16_t>(voices_[v].vol[0].env.level); }
std::uint16_t Spu::VoiceCurrentVolumeRight(int v) const { CheckVoice(v); return static_cast<std::uint16_t>(voices_[v].vol[1].env.level); }
int Spu::VoiceAdsrPhase(int v) const { CheckVoice(v); return voices_[v].phase; }

void Spu::KeyOn(std::uint32_t mask) { pending_key_on_ |= mask & 0xFFFFFF; }
void Spu::KeyOff(std::uint32_t mask) { pending_key_off_ |= mask & 0xFFFFFF; }
void Spu::SetPitchModulation(std::uint32_t mask) { pmon_ = mask & 0xFFFFFE; }
void Spu::SetNoiseMode(std::uint32_t mask) { non_ = mask & 0xFFFFFF; }
void Spu::SetReverbMode(std::uint32_t mask) { eon_ = mask & 0xFFFFFF; }
std::uint32_t Spu::Endx() const { return endx_; }

void Spu::SetMainVolumeLeft(std::uint16_t value) { WriteVolume(main_vol_[0], value); }
void Spu::SetMainVolumeRight(std::uint16_t value) { WriteVolume(main_vol_[1], value); }
std::uint16_t Spu::MainCurrentVolumeLeft() const { return static_cast<std::uint16_t>(main_vol_[0].env.level); }
std::uint16_t Spu::MainCurrentVolumeRight() const { return static_cast<std::uint16_t>(main_vol_[1].env.level); }

void Spu::SetReverbOutputVolume(std::uint16_t left, std::uint16_t right) {
    evol_[0] = static_cast<std::int16_t>(left);
    evol_[1] = static_cast<std::int16_t>(right);
}

// spec: SPU Reverb Registers - "Writing a value to ESA does additionally set
// the current buffer address to that value."
void Spu::SetReverbBase(std::uint16_t esa) {
    esa_ = esa;
    reverb_address_ = static_cast<std::uint32_t>(esa) * 8;
}

void Spu::SetReverbRegister(int index, std::uint16_t value) {
    if (index < 0 || index >= 32) Fatal("reverb register %d out of range 0..31", index);
    reverb_[index] = value;
}

std::uint16_t Spu::ReverbRegister(int index) const {
    if (index < 0 || index >= 32) Fatal("reverb register %d out of range 0..31", index);
    return reverb_[index];
}

// spec: SPU Control and Status Register, ATTR.
void Spu::SetControl(std::uint16_t value) {
    if (value & 0x000F)
        Fatal("ATTR 0x%04X: the CD (I2SA) and external (I2SB) inputs and their reverb (bits 0..3) are not modelled", value);
    if (value & 0x0040)
        Fatal("ATTR 0x%04X: the SPU interrupt (IRQ9, bit 6) is not modelled", value);
    attr_ = value; // bits 4..5 (transfer mode) have no audible effect: WriteRam is the transfer
}

std::uint16_t Spu::Control() const { return attr_; }

void Spu::ApplyReverbPreset(const SpuReverbPreset& preset) {
    for (int i = 0; i < 32; ++i) reverb_[i] = preset.regs[i];
    SetReverbBase(static_cast<std::uint16_t>((kRamBytes - preset.size) / 8));
}

void Spu::WriteRegister(std::uint32_t offset, std::uint16_t value) {
    if (offset & 1) Fatal("WriteRegister: odd offset 0x%X", offset);
    if (offset < 0x180) {
        int v = static_cast<int>(offset >> 4);
        switch (offset & 0xF) {
        case 0x0: SetVoiceVolumeLeft(v, value); return;
        case 0x2: SetVoiceVolumeRight(v, value); return;
        case 0x4: SetVoicePitch(v, value); return;
        case 0x6: SetVoiceStartAddress(v, value); return;
        case 0x8: SetVoiceAdsr1(v, value); return;
        case 0xA: SetVoiceAdsr2(v, value); return;
        case 0xC: Fatal("WriteRegister: a write to ENVX (voice %d) is not modelled", v);
        case 0xE: SetVoiceLoopAddress(v, value); return;
        }
    }
    if (offset >= 0x1C0 && offset < 0x200) {
        SetReverbRegister(static_cast<int>((offset - 0x1C0) >> 1), value);
        return;
    }
    auto lo = [&](std::uint32_t& mask) { mask = (mask & 0xFF0000) | value; };
    auto hi = [&](std::uint32_t& mask) { mask = (mask & 0x00FFFF) | ((value & 0xFFu) << 16); };
    switch (offset) {
    case 0x180: SetMainVolumeLeft(value); return;
    case 0x182: SetMainVolumeRight(value); return;
    case 0x184: evol_[0] = static_cast<std::int16_t>(value); return;
    case 0x186: evol_[1] = static_cast<std::int16_t>(value); return;
    case 0x188: KeyOn(value); return;
    case 0x18A: KeyOn((value & 0xFFu) << 16); return;
    case 0x18C: KeyOff(value); return;
    case 0x18E: KeyOff((value & 0xFFu) << 16); return;
    case 0x190: lo(pmon_); pmon_ &= 0xFFFFFE; return;
    case 0x192: hi(pmon_); return;
    case 0x194: lo(non_); return;
    case 0x196: hi(non_); return;
    case 0x198: lo(eon_); return;
    case 0x19A: hi(eon_); return;
    case 0x1A2: SetReverbBase(value); return;
    case 0x1AA: SetControl(value); return;
    case 0x1AC:
        if (value != 0x0004) Fatal("WriteRegister: RAM_CTRL 0x%04X, only 0x0004 (one 512 KiB bank) is modelled", value);
        return;
    }
    Fatal("WriteRegister: offset 0x%03X (0x1F801%03X) is not modelled", offset, 0xC00 + offset);
}

// --- voices ----------------------------------------------------------------

// spec: SPU ADPCM Samples, "Sample Data", and CDROM Format,
// "decode_28_nibbles": shift = 12 - (header & 0x0F) with 13..15 acting as 9
// (R2), the filter in bits 4..6 and bit 7 ignored (R21), s = (t << shift) + ((old * f0 + older * f1)
// >> 6), clamped to 16 bits (R1: no +32 - the renders settled it, docs/music-open-ends.md 2).
void Spu::DecodeBlock(const std::uint8_t block[16], std::int16_t out[28], std::int32_t hist[2]) {
    int range = block[0] & 0x0F;
    if (range > 12) range = 9;
    int filter = (block[0] >> 4) & 0x07;
    if (filter > 4) Fatal("ADPCM block header 0x%02X: filter %d (only 0..4 are described)", block[0], filter);
    const std::int32_t f0 = kFilterPos[filter], f1 = kFilterNeg[filter];
    std::int32_t old = hist[0], older = hist[1];
    for (int j = 0; j < 28; ++j) {
        std::int32_t t = (block[2 + (j >> 1)] >> ((j & 1) * 4)) & 0x0F;
        t = static_cast<std::int16_t>(t << 12) >> range;
        std::int32_t s = Clamp16(t + ((old * f0 + older * f1) >> 6));
        out[j] = static_cast<std::int16_t>(s);
        older = old;
        old = s;
    }
    hist[0] = old;
    hist[1] = older;
}

std::int16_t Spu::GaussTable(int index) { return kGauss[index & 0x1FF]; }

// spec: SPU ADPCM Pitch, "4-Point Gaussian Interpolation" - each product
// shifted separately, as written.
std::int32_t Spu::Interpolate(const std::int16_t s[4], int i) {
    std::int32_t out = (kGauss[0x0FF - i] * s[0]) >> 15;
    out += (kGauss[0x1FF - i] * s[1]) >> 15;
    out += (kGauss[0x100 + i] * s[2]) >> 15;
    out += (kGauss[0x000 + i] * s[3]) >> 15;
    return out;
}

// Reads and decodes the block at the voice's current address. A loop-start
// flag (bit 2) copies the address to LSAX as the block is read (spec: SPU
// ADPCM Samples, LSAX).
void Spu::LoadBlock(Voice& voice) {
    std::uint8_t block[16];
    std::uint32_t base = (voice.address * 8) & (kRamBytes - 1);
    for (int i = 0; i < 16; i += 2) {
        std::uint16_t half = ram_[((base + static_cast<std::uint32_t>(i)) & (kRamBytes - 1)) >> 1];
        block[i] = static_cast<std::uint8_t>(half);
        block[i + 1] = static_cast<std::uint8_t>(half >> 8);
    }
    voice.flags = block[1];
    if (voice.flags & 0x04) voice.lsa = static_cast<std::uint16_t>(voice.address);
    // The filter as DecodeBlock reads it, bits 4..6; bit 7 is not part of it
    // (R21).
    if (((block[0] >> 4) & 0x07) > 4) {
        // R20: a voice that is silent for good (released or never keyed on,
        // envelope at zero) goes on reading SPU RAM wherever its address has
        // walked - the capture buffers, the reverb area - and meets headers
        // that are not ADPCM.
        // Unheard (its OUTX is zero, and key on resets the decoder, R3): the
        // block decodes to silence. Heard, it is the abort below.
        if (voice.phase >= kRelease && voice.adsr.level == 0) {
            for (int i = 0; i < 28; ++i) voice.buf[3 + i] = 0;
            voice.hist[0] = voice.hist[1] = 0;
            return;
        }
        Fatal("voice %d: ADPCM block at 0x%05X, header 0x%02X: filter %d (only 0..4 are described); start 0x%05X, loop 0x%05X",
              static_cast<int>(&voice - voices_), base, block[0], (block[0] >> 4) & 0x07, voice.ssa * 8u, voice.lsa * 8u);
    }
    DecodeBlock(block, voice.buf + 3, voice.hist);
}

// The end of the block just played. spec: SPU ADPCM Samples, "Flag Bits":
// bit 0 sets ENDX and jumps to LSAX; with bit 1 clear as well the voice is
// forced to release with the envelope at zero (R4). Otherwise the next block.
void Spu::FinishBlock(Voice& voice, int v) {
    if (voice.flags & 0x01) {
        endx_ |= 1u << v;
        voice.address = voice.lsa;
        if ((voice.flags & 0x02) == 0) {
            voice.phase = kRelease;
            voice.adsr.level = 0;
            voice.adsr.counter = 0;
        }
    } else {
        voice.address = (voice.address + 2) & 0xFFFF;
    }
    voice.buf[0] = voice.buf[28];
    voice.buf[1] = voice.buf[29];
    voice.buf[2] = voice.buf[30];
    LoadBlock(voice);
}

// spec: SPU Voice Flags, KON - "Starts the ADSR Envelope, and automatically
// initializes ADSR Volume to zero"; SSA "is copied to the current address
// upon Key On". The decoder and interpolation histories and the pitch counter
// start from zero (R3).
void Spu::KeyOnVoice(Voice& voice) {
    voice.address = voice.ssa;
    // R18: the voice starts with the pitch counter at sample 3 of the block,
    // so that the interpolation's newest sample is three source samples
    // ahead of where R11 alone puts it (measured against the renders,
    // docs/spu-model.md R18).
    voice.counter = 3u << 12;
    voice.phase = kAttack;
    voice.adsr.level = 0;
    voice.adsr.counter = 0;
    voice.hist[0] = voice.hist[1] = 0;
    voice.buf[0] = voice.buf[1] = voice.buf[2] = 0;
    LoadBlock(voice);
}

// spec: SPU Volume and ADSR Generator, ADSR1/ADSR2 and the envelope
// operation. Phase changes are taken after the step that reaches the target
// (R8); each phase change restarts the rate counter (R7).
void Spu::StepAdsr(Voice& voice) {
    const std::uint16_t a1 = voice.adsr1, a2 = voice.adsr2;
    EnvRate r;
    switch (voice.phase) {
    case kAttack: {
        int shift = (a1 >> 10) & 0x1F, step = (a1 >> 8) & 3;
        r = {(a1 & 0x8000) != 0, false, false, shift, step, shift == 0x1F && step == 3};
        break;
    }
    case kDecay: {
        int shift = (a1 >> 4) & 0x0F;
        r = {true, true, false, shift, 0, false};
        break;
    }
    case kSustain: {
        int shift = (a2 >> 8) & 0x1F, step = (a2 >> 6) & 3;
        r = {(a2 & 0x8000) != 0, (a2 & 0x4000) != 0, false, shift, step, shift == 0x1F && step == 3};
        break;
    }
    case kRelease: {
        int shift = a2 & 0x1F;
        r = {(a2 & 0x20) != 0, true, false, shift, 0, shift == 0x1F};
        break;
    }
    default:
        return;
    }
    if (!EnvTick(voice.adsr.level, voice.adsr.counter, r)) return;
    switch (voice.phase) {
    case kAttack:
        if (voice.adsr.level >= 0x7FFF) {
            voice.phase = kDecay;
            voice.adsr.counter = 0;
        }
        break;
    case kDecay:
        if (voice.adsr.level <= ((a1 & 0x0F) + 1) * 0x800) {
            voice.phase = kSustain;
            voice.adsr.counter = 0;
        }
        break;
    case kRelease:
        if (voice.adsr.level <= 0) voice.phase = kOff;
        break;
    }
}

// spec: SPU Volume and ADSR Generator, "Sweep Volume Mode" through the same
// envelope operation: bit 14 exponential, 13 decrease, 12 negative phase,
// 6..2 shift, 1..0 step.
void Spu::StepVolume(Volume& vol) {
    const std::uint16_t reg = vol.reg;
    if ((reg & 0x8000) == 0) return;
    int shift = (reg >> 2) & 0x1F, step = reg & 3;
    EnvRate r = {(reg & 0x4000) != 0, (reg & 0x2000) != 0, (reg & 0x1000) != 0, shift, step,
                 shift == 0x1F && step == 3};
    EnvTick(vol.env.level, vol.env.counter, r);
}

// spec: SPU Noise Generator.
void Spu::StepNoise() {
    const int shift = (attr_ >> 10) & 0x0F;
    const int step = ((attr_ >> 8) & 3) + 4;
    const std::uint32_t lvl = noise_level_;
    const std::uint32_t parity = ((lvl >> 15) ^ (lvl >> 12) ^ (lvl >> 11) ^ (lvl >> 10) ^ 1) & 1;
    noise_timer_ -= step;
    if (noise_timer_ < 0) {
        noise_level_ = ((lvl << 1) | parity) & 0xFFFF;
        noise_timer_ += 0x20000 >> shift;
        if (noise_timer_ < 0) noise_timer_ += 0x20000 >> shift;
    }
}

// --- reverb ----------------------------------------------------------------

// spec: SPU Reverb Formula, "Notes": addresses relative to the current buffer
// address, wrapped within ESA..0x7FFFE.
std::uint32_t Spu::ReverbAddress(std::int32_t offset_bytes) const {
    const std::int32_t base = static_cast<std::int32_t>(esa_) * 8;
    const std::int32_t size = static_cast<std::int32_t>(kRamBytes) - base;
    std::int32_t rel = (static_cast<std::int32_t>(reverb_address_) - base + offset_bytes) % size;
    if (rel < 0) rel += size;
    return static_cast<std::uint32_t>(base + rel) & 0x7FFFE;
}

std::int16_t Spu::ReverbRead(std::int32_t offset_bytes) const { return RamHalf(ReverbAddress(offset_bytes)); }

// "When the Reverb Master Enable flag is cleared, the SPU stops to write any
// data to the Reverb buffer" (spec: SPU Reverb Registers, ATTR bits).
void Spu::ReverbWrite(std::int32_t offset_bytes, std::int32_t value) {
    if ((attr_ & 0x0080) == 0) return;
    ram_[ReverbAddress(offset_bytes) >> 1] = static_cast<std::uint16_t>(Clamp16(value));
}

// One channel's 22,050 Hz step, in the order of spec: SPU Internal State
// Machine, "Reverb Computation Order"; the arithmetic of SPU Reverb Formula
// with every product and sum saturated to 16 bits (R12).
void Spu::StepReverb(int ch, std::int32_t input) {
    auto reg = [this](int i) { return static_cast<std::int32_t>(reverb_[i]); };
    auto vol = [this](int i) { return static_cast<std::int32_t>(static_cast<std::int16_t>(reverb_[i])); };
    if (reverb_[vIIR] == 0x8000) Fatal("reverb vIIR = -8000h: the negation the spec describes under \"Bug\" is not modelled");

    const int mSAME = ch ? mRSAME : mLSAME;
    const int dSAME = ch ? dRSAME : dLSAME;
    const int mDIFF = ch ? mRDIFF : mLDIFF;
    const int dDIFF = ch ? dLDIFF : dRDIFF; // the other side's
    const int mCOMB1 = ch ? mRCOMB1 : mLCOMB1, mCOMB2 = ch ? mRCOMB2 : mLCOMB2;
    const int mCOMB3 = ch ? mRCOMB3 : mLCOMB3, mCOMB4 = ch ? mRCOMB4 : mLCOMB4;
    const int mAPF1 = ch ? mRAPF1 : mLAPF1, mAPF2 = ch ? mRAPF2 : mLAPF2;

    const std::int32_t in = Mul15(vol(ch ? vRIN : vLIN), input);
    const std::int32_t wall = vol(vWALL), iir = vol(vIIR);

    const std::int32_t d_same = ReverbRead(reg(dSAME) * 8);
    const std::int32_t same_prev = ReverbRead(reg(mSAME) * 8 - 2);
    const std::int32_t d_diff = ReverbRead(reg(dDIFF) * 8);
    {
        std::int32_t x = Clamp16(Clamp16(in + Mul15(d_same, wall)) - same_prev);
        ReverbWrite(reg(mSAME) * 8, Clamp16(Mul15(x, iir) + same_prev));
    }
    const std::int32_t diff_prev = ReverbRead(reg(mDIFF) * 8 - 2);
    const std::int32_t c1 = ReverbRead(reg(mCOMB1) * 8);
    {
        std::int32_t x = Clamp16(Clamp16(in + Mul15(d_diff, wall)) - diff_prev);
        ReverbWrite(reg(mDIFF) * 8, Clamp16(Mul15(x, iir) + diff_prev));
    }
    const std::int32_t c2 = ReverbRead(reg(mCOMB2) * 8);
    const std::int32_t c3 = ReverbRead(reg(mCOMB3) * 8);
    const std::int32_t c4 = ReverbRead(reg(mCOMB4) * 8);
    const std::int32_t a1 = ReverbRead((reg(mAPF1) - reg(dAPF1)) * 8);
    const std::int32_t a2 = ReverbRead((reg(mAPF2) - reg(dAPF2)) * 8);

    std::int32_t out = Mul15(vol(vCOMB1), c1);
    out = Clamp16(out + Mul15(vol(vCOMB2), c2));
    out = Clamp16(out + Mul15(vol(vCOMB3), c3));
    out = Clamp16(out + Mul15(vol(vCOMB4), c4));

    out = Clamp16(out - Mul15(vol(vAPF1), a1));
    ReverbWrite(reg(mAPF1) * 8, out);
    out = Clamp16(Mul15(out, vol(vAPF1)) + a1);

    out = Clamp16(out - Mul15(vol(vAPF2), a2));
    ReverbWrite(reg(mAPF2) * 8, out);
    out = Clamp16(Mul15(out, vol(vAPF2)) + a2);

    rev_out_[ch][tick_ & 63] = static_cast<std::int16_t>(out);
}

// --- the sample loop -------------------------------------------------------

void Spu::Render(std::int16_t* stereo, int frames) {
    if ((attr_ & 0x8000) == 0) Fatal("Render with the SPU disabled (ATTR bit 15 clear) is not modelled");
    for (int f = 0; f < frames; ++f) {
        // Key off, then key on (R9), latched from the writes since the last
        // sample; a key on acts kKeyOnLatency samples after its write (R19).
        if (pending_key_on_) {
            for (int v = 0; v < kVoices; ++v)
                if ((pending_key_on_ >> v) & 1) key_on_wait_[v] = kKeyOnLatency + 1;
            pending_key_on_ = 0;
        }
        if (pending_key_off_) {
            for (int v = 0; v < kVoices; ++v) {
                if ((pending_key_off_ >> v) & 1) {
                    Voice& voice = voices_[v];
                    if (voice.phase < kRelease) {
                        voice.phase = kRelease;
                        voice.adsr.counter = 0;
                    }
                }
            }
            pending_key_off_ = 0;
        }
        for (int v = 0; v < kVoices; ++v) {
            if (key_on_wait_[v] == 0 || --key_on_wait_[v] != 0) continue;
            KeyOnVoice(voices_[v]);
            endx_ &= ~(1u << v);
        }

        StepNoise(); // R13: once per sample, before the voices read it

        std::int32_t dry[2] = {0, 0};
        std::int32_t wet[2] = {0, 0};
        std::int32_t prev_out = 0;
        for (int v = 0; v < kVoices; ++v) {
            Voice& voice = voices_[v];

            // The sample: noise, or the Gaussian over the four most recent
            // ADPCM samples at the pitch counter (R11). OUTX = sample x ENVX.
            std::int32_t out = 0;
            if (voice.adsr.level != 0) {
                std::int32_t sample;
                if ((non_ >> v) & 1) {
                    sample = static_cast<std::int16_t>(noise_level_);
                } else {
                    const int index = static_cast<int>(voice.counter >> 12);
                    sample = Interpolate(voice.buf + index, static_cast<int>((voice.counter >> 4) & 0xFF));
                }
                out = (sample * voice.adsr.level) >> 15;
            }
            voice.out = out;
            StepAdsr(voice); // R14: the level used above is the one before this tick's step

            const std::int32_t left = (out * voice.vol[0].env.level) >> 15;
            const std::int32_t right = (out * voice.vol[1].env.level) >> 15;
            StepVolume(voice.vol[0]);
            StepVolume(voice.vol[1]);
            if ((mix_mask_ >> v) & 1) {
                dry[0] += left;
                dry[1] += right;
                if ((eon_ >> v) & 1) {
                    wet[0] += left;
                    wet[1] += right;
                }
            }

            // spec: SPU ADPCM Pitch, "Pitch Counter".
            std::int32_t step = voice.pitch;
            if ((pmon_ >> v) & 1) {
                const std::int32_t factor = prev_out + 0x8000;
                step = static_cast<std::int16_t>(voice.pitch);
                step = (step * factor) >> 15;
                step &= 0xFFFF;
            }
            if (step > 0x3FFF) step = 0x4000;
            voice.counter += static_cast<std::uint32_t>(step);
            if (voice.counter >= (28u << 12)) {
                voice.counter -= 28u << 12;
                FinishBlock(voice, v);
            }
            prev_out = out;
        }

        // spec: SPU Memory layout - the capture buffers: CD left/right (no CD
        // input is modelled: silence) and voices 1 and 3 after the envelope.
        ram_[(0x000 >> 1) + capture_index_] = 0;
        ram_[(0x400 >> 1) + capture_index_] = 0;
        ram_[(0x800 >> 1) + capture_index_] = static_cast<std::uint16_t>(Clamp16(voices_[1].out));
        ram_[(0xC00 >> 1) + capture_index_] = static_cast<std::uint16_t>(Clamp16(voices_[3].out));
        capture_index_ = (capture_index_ + 1) & 0x1FF;

        // Reverb: the input resampled 44.1 -> 22.05 kHz by the 39-tap FIR,
        // left on even samples and right on odd ones (spec: Reverb Formula,
        // "Notes"; R15), the output zero-stuffed and filtered back to 44.1 kHz.
        const unsigned t = tick_;
        rev_in_[0][t & 63] = static_cast<std::int16_t>(Clamp16(wet[0]));
        rev_in_[1][t & 63] = static_cast<std::int16_t>(Clamp16(wet[1]));
        const int ch = static_cast<int>(t & 1);
        rev_out_[ch ^ 1][t & 63] = 0;
        {
            std::int32_t acc = 0;
            for (int k = 0; k < 39; ++k) acc += kReverbFir[k] * rev_in_[ch][(t - static_cast<unsigned>(k)) & 63];
            StepReverb(ch, Clamp16(acc >> 15));
        }
        if (ch == 1) {
            // spec: "BufferAddress = MAX(ESA, (BufferAddress+2) AND 7FFFEh)".
            std::uint32_t next = (reverb_address_ + 2) & 0x7FFFE;
            const std::uint32_t base = static_cast<std::uint32_t>(esa_) * 8;
            reverb_address_ = next < base ? base : next;
        }
        std::int32_t rev[2];
        for (int c = 0; c < 2; ++c) {
            std::int32_t acc = 0;
            for (int k = 0; k < 39; ++k) acc += kReverbFir[k] * rev_out_[c][(t - static_cast<unsigned>(k)) & 63];
            rev[c] = Mul15(Clamp16(acc >> 14), evol_[c]);
        }

        // The mix (R16): voices + reverb return, clamped, times the main
        // volume, clamped. Mute (ATTR bit 14 clear) silences the output.
        for (int c = 0; c < 2; ++c) {
            std::int32_t mixed = Clamp16(Clamp16(dry[c]) + rev[c]);
            mixed = Mul15(mixed, main_vol_[c].env.level);
            if ((attr_ & 0x4000) == 0) mixed = 0;
            stereo[2 * f + c] = static_cast<std::int16_t>(mixed);
            StepVolume(main_vol_[c]);
        }
        ++tick_;
    }
}

} // namespace psx

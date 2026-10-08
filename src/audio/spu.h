// A model of the PlayStation's sound processor (SPU): 24 ADPCM voices with
// Gaussian interpolation, ADSR envelopes, volume sweeps, the noise generator,
// pitch modulation, the reverb unit and the final mix, at 44,100 Hz, from the
// public register-level description (nocash's psx-spx, the SPU chapter).
// docs/spu-model.md says what each part follows, and every reading taken where
// that description is ambiguous.
//
// Portable and standalone: no Windows, no game addresses, no project helpers.
// What the model does not implement aborts through the hook below, never a
// silent skip (the repo's hard rule 4).
#pragma once

#include <cstdint>

namespace psx {

// Called with a message before std::abort() when the client asks for something
// the model does not implement. The default prints to stderr.
using SpuAbortHook = void (*)(const char* message);
void SetSpuAbortHook(SpuAbortHook hook);

// One reverb register set as psx-spx's "SPU Reverb Examples" lists it: the 32
// registers 0x1F801DC0..0x1F801DFE in order, and the work area size in bytes
// (ESA = (0x80000 - size) / 8).
struct SpuReverbPreset {
    const char* name;
    std::uint32_t size;
    std::uint16_t regs[32];
};
extern const SpuReverbPreset kSpuReverbPresets[];
extern const int kSpuReverbPresetCount;

class Spu {
public:
    static constexpr int kVoices = 24;
    static constexpr std::uint32_t kRamBytes = 0x80000;

    Spu();
    // Power-on state: RAM zero, every register zero, every voice idle.
    void Reset();

    // SPU RAM, by byte address (0..0x7FFFF), as DMA would write it.
    void WriteRam(std::uint32_t address, const void* data, std::uint32_t size);
    void ReadRam(std::uint32_t address, void* data, std::uint32_t size) const;

    // Voice registers 0x1F801C00 + 0x10 * v.
    void SetVoiceVolumeLeft(int v, std::uint16_t value);   // VOLL (+0)
    void SetVoiceVolumeRight(int v, std::uint16_t value);  // VOLR (+2)
    void SetVoicePitch(int v, std::uint16_t value);        // PITCH (+4)
    void SetVoiceStartAddress(int v, std::uint16_t value); // SSA (+6), 8-byte units
    void SetVoiceAdsr1(int v, std::uint16_t value);        // ADSR1 (+8)
    void SetVoiceAdsr2(int v, std::uint16_t value);        // ADSR2 (+A)
    void SetVoiceLoopAddress(int v, std::uint16_t value);  // LSAX (+E), 8-byte units

    std::uint16_t VoiceEnvelope(int v) const;          // ENVX (+C)
    std::uint16_t VoiceLoopAddress(int v) const;       // LSAX as the hardware leaves it
    std::uint16_t VoiceCurrentVolumeLeft(int v) const; // VOLXL 0x1F801E00 + 4 * v
    std::uint16_t VoiceCurrentVolumeRight(int v) const;// VOLXR 0x1F801E02 + 4 * v

    // Not a hardware register: which envelope phase the voice is in, for
    // tests and for a host tool's traces. 0 attack, 1 decay, 2 sustain,
    // 3 release, 4 off (released to zero).
    int VoiceAdsrPhase(int v) const;

    // Flag registers, bit n = voice n (the hardware splits each into two
    // halfwords: voices 0..15 and 16..23).
    void KeyOn(std::uint32_t mask);               // KON, acted on at the next sample
    void KeyOff(std::uint32_t mask);              // KOFF, acted on at the next sample
    void SetPitchModulation(std::uint32_t mask);  // PMON (bit 0 ignored)
    void SetNoiseMode(std::uint32_t mask);        // NON
    void SetReverbMode(std::uint32_t mask);       // EON
    std::uint32_t Endx() const;                   // ENDX

    void SetMainVolumeLeft(std::uint16_t value);   // MVOLL 0x1F801D80
    void SetMainVolumeRight(std::uint16_t value);  // MVOLR 0x1F801D82
    std::uint16_t MainCurrentVolumeLeft() const;   // MVOLXL 0x1F801DB8
    std::uint16_t MainCurrentVolumeRight() const;  // MVOLXR 0x1F801DBA
    void SetReverbOutputVolume(std::uint16_t left, std::uint16_t right); // EVOLL/EVOLR 0x1F801D84/86
    void SetReverbBase(std::uint16_t esa);        // ESA 0x1F801DA2, 8-byte units
    void SetReverbRegister(int index, std::uint16_t value); // 0x1F801DC0 + 2 * index, 0..31
    std::uint16_t ReverbRegister(int index) const;
    void SetControl(std::uint16_t value);         // ATTR / SPUCNT 0x1F801DAA
    std::uint16_t Control() const;

    // The register file by I/O address offset from 0x1F801C00 (0x000..0x1FE),
    // for replaying a register trace. Registers the model does not implement
    // abort.
    void WriteRegister(std::uint32_t offset, std::uint16_t value);

    // Writes the preset's 32 registers and ESA = (0x80000 - size) / 8.
    void ApplyReverbPreset(const SpuReverbPreset& preset);

    // Advances the SPU `frames` samples at 44,100 Hz, writing interleaved
    // left/right 16-bit samples.
    void Render(std::int16_t* stereo, int frames);

    // The noise generator's current output level (not a hardware register).
    std::int16_t NoiseLevel() const { return static_cast<std::int16_t>(noise_level_); }

    // The hardware's building blocks, exposed for the unit tests.
    // Decodes one 16-byte ADPCM block into 28 samples; hist[0] is the most
    // recent previous output, hist[1] the one before, both updated.
    static void DecodeBlock(const std::uint8_t block[16], std::int16_t out[28], std::int32_t hist[2]);
    // 4-point Gaussian interpolation: s[0] oldest .. s[3] newest, i = 0..255.
    static std::int32_t Interpolate(const std::int16_t s[4], int i);
    static std::int16_t GaussTable(int index);

private:
    struct Envelope {
        std::int32_t level = 0;
        std::uint32_t counter = 0;
    };
    // A volume register (VOLL, VOLR, MVOLL, MVOLR) and its current level.
    struct Volume {
        std::uint16_t reg = 0;
        Envelope env;
    };
    struct Voice {
        Volume vol[2];
        std::uint16_t pitch = 0;
        std::uint16_t ssa = 0;
        std::uint16_t adsr1 = 0;
        std::uint16_t adsr2 = 0;
        std::uint16_t lsa = 0;
        std::uint32_t address = 0;   // current block, 8-byte units
        std::uint32_t counter = 0;   // pitch counter: bits 12+ sample in block
        int phase = 4;
        Envelope adsr;
        std::uint8_t flags = 0;      // flag byte of the block being played
        std::int32_t hist[2] = {0, 0};
        std::int16_t buf[3 + 28] = {}; // last three of the previous block, then this block
        std::int32_t out = 0;        // OUTX of the current sample (after the envelope)
    };

    void CheckVoice(int v) const;
    void KeyOnVoice(Voice& voice);
    void LoadBlock(Voice& voice);
    void FinishBlock(Voice& voice, int v);
    void StepAdsr(Voice& voice);
    static void StepVolume(Volume& vol);
    void WriteVolume(Volume& vol, std::uint16_t value);
    void StepNoise();
    void StepReverb(int channel, std::int32_t input);
    std::int16_t RamHalf(std::uint32_t byte_address) const { return static_cast<std::int16_t>(ram_[(byte_address & 0x7FFFE) >> 1]); }
    std::uint32_t ReverbAddress(std::int32_t offset_bytes) const;
    std::int16_t ReverbRead(std::int32_t offset_bytes) const;
    void ReverbWrite(std::int32_t offset_bytes, std::int32_t value);

    std::uint16_t ram_[kRamBytes / 2];
    Voice voices_[kVoices];
    std::uint32_t pending_key_on_ = 0;
    std::uint32_t pending_key_off_ = 0;
    std::uint32_t pmon_ = 0;
    std::uint32_t non_ = 0;
    std::uint32_t eon_ = 0;
    std::uint32_t endx_ = 0;
    Volume main_vol_[2];
    std::int16_t evol_[2] = {0, 0};
    std::uint16_t esa_ = 0;
    std::uint16_t reverb_[32] = {};
    std::uint16_t attr_ = 0;
    std::uint32_t reverb_address_ = 0; // bytes
    std::uint32_t noise_level_ = 0;
    std::int32_t noise_timer_ = 0;
    std::uint32_t tick_ = 0;           // samples since reset
    std::uint32_t capture_index_ = 0;  // 0..0x1FF
    // Reverb resampler histories at 44.1 kHz, indexed by tick & 63.
    std::int16_t rev_in_[2][64] = {};
    std::int16_t rev_out_[2][64] = {};
};

} // namespace psx

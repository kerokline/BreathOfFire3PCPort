// The music player: Sony's libsnd 3.7 sequencer and voice manager, as the
// PlayStation boot EXE of Breath of Fire III runs them, driving the SPU model
// (spu.h) through the same register values in the same order at the same
// tick. docs/libsnd-reading.md is the companion: every rule below cites the
// function it reproduces there, and the readings that decide each detail.
//
// Portable and standalone: no Windows, no game addresses. What the player
// does not implement aborts through psx::MusicFatal (song.h), never a silent
// skip (the repo's hard rule 4).
#pragma once

#include <cstdint>

#include "audio/song.h"
#include "audio/spu.h"

namespace psx {

class MusicSynth {
public:
    // SPU byte address the game's sound layouts give the music bank (VAB slot
    // 0 of every layout); docs/libsnd-reading.md section 7.
    static constexpr std::uint32_t kBankAddress = 0x1010;
    static constexpr std::uint32_t kBankLimit = 0x6C6D0;
    // The sequencer's voices (SsSetReservedVoice(16)); 16..23 are the effects'.
    static constexpr int kSeqVoices = 16;
    // One VSync in SPU samples, as a fraction: 263 lines x 3412.5 GPU clocks
    // at Mednafen's NTSC GPU/CPU ratio 103896/65536, over 768 CPU clocks a
    // sample = 737.137137... (libsnd-reading.md section 1.4).
    static constexpr std::uint64_t kTickNum = 263ull * 6825ull * 65536ull;
    static constexpr std::uint64_t kTickDen = 103896ull * 768ull * 2ull;

    MusicSynth();

    // The SPU and libsnd as the game's sound start-up leaves them: SsInit,
    // SsSetTickMode(SS_TICK60), SsUtSetReverbType(1), SsUtReverbOn,
    // SsSetReservedVoice(16), SsUtSetReverbDepth(40, 40), SsSetRVol(40, 40),
    // SsSetMVol(127, 127). Called by the constructor.
    void Reset();

    // SsVabOpenHeadSticky + SsVabTransBody at kBankAddress. The bank is copied.
    // A bank of another name first keys the sequencer's voices off and clears
    // their records' references into the old one (libsnd-reading.md 3.11).
    void LoadBank(const Bank& bank);
    // What LoadBank refuses, without loading: a bank past the game's largest
    // music slot, a master volume above 127. Aborts through MusicFatal.
    static void CheckBank(const Bank& bank);

    // The game's Music_Play: SsSepStop, SsSepSetVol(0, 0), SsSepPlay(SSPLAY_PLAY,
    // loop_count), SsSepSetCrescendo(volume, frames). The game always passes
    // loop_count 1 (the loop itself is in the data); the title passes volume
    // 100 over 8 frames. The song is copied; its bank must be loaded.
    void Play(const Song& song, int volume, int frames, int loop_count = 1);
    // SsSepSetVol(left, right): immediate (the game's 0x8015DE28 wrapper).
    void SetVolume(int left, int right);
    // SsSepSetCrescendo / SsSepSetDecrescendo(amount, frames).
    void Crescendo(int amount, int frames);
    void Decrescendo(int amount, int frames);
    // SsSetMono / SsSetStereo (0x8017413C / 0x80174150, called from the
    // options screens START.EMI and STATUS.EMI): from the next volume write
    // on, both sides of every voice take the louder side's level.
    void SetMono(bool mono);
    // SsSepPause / SsSepReplay (the game's 0x80162710 / 0x80162764).
    void Pause();
    void Replay();
    // SsSepStop.
    void Stop();

    // Where in the first sample the first VSync falls, in 1/256 sample (the
    // VSync and the SPU's sample clock run free of each other; a render's
    // phase is fitted, the engine's is arbitrary). Before the first Render.
    void SetTickPhase(int sub_samples);

    // Advance in VSync ticks of exactly kTickNum / kTickDen samples (the
    // fraction carried) and mix: interleaved left/right.
    void Render(std::int16_t* stereo, int frames);

    bool Playing() const { return (score_.flags & kPlay) != 0; }
    // A song that plays once (no loop markers) has reached its end of track
    // (_SsSeqGetEof stopped it and keyed its voices off) and every sequencer
    // voice's envelope has released to zero: nothing more will sound.
    // False while playing, paused, or after Stop() of a song that had not
    // ended.
    bool Ended() const;

    // SsSetMVol(left, right) (0x8016CA38): the SPU's main volume, fixed mode,
    // value * 129 (127 -> 0x3FFF, the start-up level). Effects and music
    // alike; the game calls it from two scenario scripts' fades (SCENA00,
    // SCENA16; libsnd-reading.md 6.1). 0..127.
    void SetMasterVolume(int left, int right);
    std::uint64_t Ticks() const { return ticks_; }
    // The tick (VSync count since Play, 1-based) of each loop-end jump so far.
    int LoopJumps() const { return loop_jumps_; }
    std::uint64_t LastLoopJumpTick() const { return last_loop_jump_tick_; }
    Spu& spu() { return spu_; }

    // Diagnostics (host tools, self-tests): called at every VSync tick after
    // _SsVmFlush with what the flush wrote.
    struct TickTrace {
        std::uint64_t tick;        // VSync ticks since Reset, 1-based
        std::uint64_t sample;      // samples rendered before this tick
        std::uint32_t key_on, key_off;
        int voice_writes;          // voice register writes (VOL pairs count 2)
        std::uint16_t pitch[24];   // each voice's PITCH as last written
        std::int16_t note[24];     // each voice's note and sample (libsnd's
        std::int16_t vag[24];      // record; vag 0 after a key off)
    };
    using TickTraceFn = void (*)(const TickTrace& trace, void* user);
    void SetTickTrace(TickTraceFn fn, void* user) { trace_fn_ = fn; trace_user_ = user; }
    // Diagnostics: an extra delay, in 1/256 sample, of VSync `tick` (1-based)
    // - for experiments on the interrupt latency the renders show
    // (libsnd-reading.md section 9).
    using TickOffsetFn = int (*)(std::uint64_t tick, void* user);
    void SetTickOffset(TickOffsetFn fn, void* user) { offset_fn_ = fn; offset_user_ = user; }

private:
    enum : std::uint32_t { kPlay = 1, kPause = 2, kStop = 4, kReplay = 8, kCres = 0x10, kDecres = 0x20,
                           kAccel = 0x40, kRitard = 0x80, kEnded = 0x200 };

    // The libsnd score fields this player uses (the 0xAC-byte struct; offsets
    // in libsnd-reading.md section 2).
    struct Score {
        std::uint32_t pos = 0, start_pos = 0, loop_pos = 0;  // +04 / +08 / +0C, event indices
        std::int32_t delta = 0;          // +88, ticks x 10 to the next event
        std::int32_t first_delta = 0;    // +7C
        std::int16_t step = 0;           // +70, ticks x 10 per VSync
        std::int16_t step_init = 0;      // +72
        std::int16_t countdown = 0;      // +6E, -1 unless a slow tempo
        std::int32_t bpm_init = 0;       // +84
        std::int32_t bpm = 0;            // +8C
        std::int16_t resolution = 0;     // +4A
        std::int16_t vab = 0;            // +4C
        std::uint8_t ch = 0;             // +12, the channel of the last event read
        std::uint8_t f10 = 0, f13 = 0, f14 = 0, f15 = 0, nrpn = 0;  // +10 +13 +14 +15 +16
        std::uint8_t f27 = 0, loop_count = 0, f29 = 0, f2a = 0, f2b = 0;  // +27 +28 +29 +2A +2B
        std::uint8_t prog[16] = {};      // +2C
        std::uint8_t pan[16] = {};       // +17
        std::int16_t vol[16] = {};       // +4E
        std::uint16_t seq_l = 127, seq_r = 127;  // +74 / +76
        std::int16_t loops_req = 0, plays = 0;   // +46 / +48
        std::uint32_t flags = 0;         // +90
        std::int16_t v_target = 0, v_count = 0, v_step = 0;  // +3E +40 +42
        std::uint32_t v_total = 0, v_left = 0;                // +94 +98
        std::uint16_t mute = 0;          // +A8
    };

    // libsnd's per-voice record (_svm_voice, 0x34 bytes; section 3).
    struct Voice {
        std::int16_t vag = 0xFF;      // +00
        std::int16_t age = 24;        // +02
        std::uint16_t pitch = 0;      // +04
        std::uint16_t envx = 0;       // +06, ENVX as the last flush read it
        std::int16_t vel = 0;         // +08, velocity x channel volume / 127
        std::uint8_t pan = 64;        // +0A, the channel's pan at key on
        std::int16_t note = 0;        // +0C
        std::int16_t seqid = -1;      // +0E
        std::int16_t block = 0;       // +10, the program's tone block
        std::int16_t prog = 0;        // +12
        std::int16_t tone = 0xFF;     // +14
        std::int16_t vab = 0;         // +16
        std::int16_t prior = 0;       // +18
        std::uint8_t keyed = 0;       // +13 (0 free, 1 keyed)
    };

    // The shadow registers _SsVmFlush writes (0x8018F180, 16 bytes a voice).
    struct ShadowRegs {
        std::uint16_t vol_l = 0, vol_r = 0, pitch = 0, ssa = 0, adsr1 = 0, adsr2 = 0;
    };

    // libsnd's _svm_cur, the note being keyed (0x8018F150..).
    struct Current {
        std::int16_t seqid = 0;
        std::uint8_t note = 0, vel = 0, pan = 0, block = 0, tones = 0;
        std::uint8_t pmvol = 0, pmpan = 0;
        std::uint8_t tone = 0, tvol = 0, tpan = 0, tprior = 0, center = 0, shift = 0, tmode = 0;
        std::int16_t vag = 0;
        int voice = 0;
    };

    void TickOnce();                       // SsSeqCalledTbyT
    void Flush();                          // _SsVmFlush
    void SeqPlay();                        // _SsSeqPlay
    void GetSeqData();                     // _SsGetSeqData
    std::int32_t ReadDelta() const;        // _SsReadDeltaValue
    void ControlChange(std::uint8_t cc, std::uint8_t value);
    void DataEntry(std::uint8_t value);    // _SsContDataEntry
    void Nrpn2(std::uint8_t value);        // _SsContNrpn2
    void Eof();                            // _SsSeqGetEof
    void SndStop();                        // _SsSndStop
    void SetVolData(int amount, int frames);
    void SndCrescendo();
    void SndDecrescendo();
    void InitScore(const Song& song);      // _SsInitSoundSep
    std::int16_t StepFor(std::int32_t bpm) const;
    std::uint64_t SpeedOf(std::int32_t bpm) const;  // resolution * bpm * 10

    bool VSetUp(int vab, int prog);        // _SsVmVSetUp
    void VmKeyOn(int prog, int note, int vel, int pan);
    void VmKeyOff(int prog, int note);
    void VmSeqKeyOff();
    void VmPitchBend(int prog, int msb);
    void VmSetVol(int prog, int vol, int pan);
    void VmSetSeqVol(int l, int r, bool apply);
    int Alloc();                           // _SsVmAlloc
    void DoAllocate();                     // _SsVmDoAllocate
    std::uint16_t NoteToPitch() const;     // note2pitch
    std::uint16_t NoteToPitch2(int note, int fine, const BankTone& tone) const;  // note2pitch2
    void KeyOnNow(std::uint16_t pitch);    // _SsVmKeyOnNow
    void KeyOffNow(int v);                 // _SsVmKeyOffNow
    const BankTone& ToneAt(int block, int tone) const;  // aborts outside the bank
    const BankTone& ToneOf(const Voice& v) const;
    void PanVolumes(std::uint32_t base, int tpan, int mpan, int cpan, std::uint32_t* l, std::uint32_t* r) const;

    Spu spu_;
    Bank bank_;
    bool bank_loaded_ = false;
    Song song_;
    bool song_loaded_ = false;
    Score score_;
    Voice voices_[24];
    ShadowRegs sreg_[24];
    std::uint8_t dirty_[24] = {};
    Current cur_;
    std::uint32_t kon_ = 0, koff_ = 0, eon_ = 0;  // 0x8018E4C8, 0x801915D0, 0x8018E4CC
    std::uint32_t envx_zero_[16] = {};            // 0x80191590
    std::uint32_t envx_index_ = 0;                // 0x8018F470
    std::uint16_t damper_ = 0;                    // 0x8018EB58
    bool mono_ = false;                           // 0x8018EBBE
    bool reached_end_ = false;                    // _SsSeqGetEof stopped the song
    std::int16_t seqid_ = 0;
    std::uint64_t phase_ = 0;                     // samples x kTickDen before VSync 0
    std::uint64_t next_tick_sample_ = 0;
    bool next_tick_valid_ = false;
    std::uint64_t ticks_ = 0;
    std::uint64_t ticks_since_play_ = 0;
    int loop_jumps_ = 0;
    std::uint64_t last_loop_jump_tick_ = 0;
    std::uint64_t samples_ = 0;
    int writes_ = 0;
    TickOffsetFn offset_fn_ = nullptr;
    void* offset_user_ = nullptr;
    TickTraceFn trace_fn_ = nullptr;
    void* trace_user_ = nullptr;
};

} // namespace psx

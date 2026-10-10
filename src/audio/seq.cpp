// libsnd 3.7's sequencer and voice manager as BoF3's boot EXE runs them.
// Each function names the libsnd function it reproduces and its address in
// SLPS_009.90; docs/libsnd-reading.md holds the reading (the instructions each
// rule rests on). Integer arithmetic is the library's: every division below
// is the one its magic-number multiply computes (checked in the reading,
// section 3.6), and the order of operations is the code's.
#include "audio/seq.h"

#include <cmath>
#include <cstring>

namespace psx {
namespace {

// note2pitch's table at 0x80184DD4: 192 halfwords, entry i = 4096 x 2^(i/192)
// rounded down (all 192 checked against the EXE; libsnd-reading.md 3.4).
struct PitchTable {
    std::uint16_t v[192];
    PitchTable() {
        // Every entry's fractional part lies between 0.007 and 0.996, so the
        // double's exp2 (error ~1e-12) floors to the table's value exactly.
        for (int i = 0; i < 192; ++i) v[i] = static_cast<std::uint16_t>(std::exp2(12.0 + i / 192.0));
    }
};
const PitchTable& Pitches() {
    static const PitchTable t;
    return t;
}

// The tick rate SsSetTickMode(SS_TICK60) leaves in _snd_seq_tick (0x80190C78).
constexpr std::int32_t kSeqTick = 60;

// libspu's reverb mode 1 (SPU_REV_MODE_ROOM) - its table at 0x80184954 + 68
// equals psx-spx's "Room" preset register for register, ESA 0xFB28 at
// 0x80184908 (libsnd-reading.md 6.2); kSpuReverbPresets[0] is that preset.
constexpr int kRoomPreset = 0;

// SsSetRVol(40, 40) / SsUtSetReverbDepth(40, 40): 40 * 0x7FFF / 127.
constexpr std::uint16_t kReverbDepth = 40 * 0x7FFF / 127;

} // namespace

MusicSynth::MusicSynth() { Reset(); }

void MusicSynth::Reset() {
    spu_.Reset();
    // _spu_init (0x80168CEC): the 16 bytes at 0x80184468 (all 0x07) to SPU
    // address 0x1000; every voice VOL 0, PITCH 0x3FFF, SSA 0x200, ADSR 0;
    // key on then key off all; ATTR 0xC000.
    std::uint8_t block[16];
    std::memset(block, 0x07, sizeof block);
    spu_.WriteRam(0x1000, block, sizeof block);
    spu_.SetControl(0xC000);
    // _SsInit (0x8016BB3C): the eight voice registers from 0x80184D88 for all
    // 24 voices (VOL 0/0, PITCH 0x1000, SSA 0x3000, ADSR 0x00BF/0, ENVX 0,
    // LSAX 0 - the ENVX write is left out: the voices are idle at level 0),
    // then 0x1F801D80.. from 0x80184D98 (MVOL 0x3FFF/0x3FFF, the rest 0).
    for (int v = 0; v < Spu::kVoices; ++v) {
        spu_.SetVoiceVolumeLeft(v, 0);
        spu_.SetVoiceVolumeRight(v, 0);
        spu_.SetVoicePitch(v, 0x1000);
        spu_.SetVoiceStartAddress(v, 0x3000);
        spu_.SetVoiceAdsr1(v, 0x00BF);
        spu_.SetVoiceAdsr2(v, 0);
        spu_.SetVoiceLoopAddress(v, 0);
    }
    spu_.SetMainVolumeLeft(0x3FFF);
    spu_.SetMainVolumeRight(0x3FFF);
    spu_.SetReverbOutputVolume(0, 0);
    spu_.SetPitchModulation(0);
    spu_.SetNoiseMode(0);
    spu_.SetReverbMode(0);
    // SsUtSetReverbType(1) -> SpuSetReverbModeParam: the depth to 0, the 32
    // registers, ESA; SsUtReverbOn -> ATTR bit 7. Then (Boot_Init)
    // SsUtSetReverbDepth(40, 40) and SsSetRVol(40, 40): EVOL 10320 both;
    // SsSetMVol(127, 127): MVOL 127 * 129 = 0x3FFF.
    spu_.ApplyReverbPreset(kSpuReverbPresets[kRoomPreset]);
    spu_.SetControl(0xC080);
    spu_.SetReverbOutputVolume(kReverbDepth, kReverbDepth);
    spu_.SetMainVolumeLeft(0x3FFF);
    spu_.SetMainVolumeRight(0x3FFF);
    // SsSetSerialAttr(SS_SERIAL_A, SS_MIX, SS_SON) would set ATTR bit 0 (CD
    // audio into the mix). No CD audio plays under the sequenced music; the
    // model does not take a CD input, so the bit stays clear and the input is
    // the silence it would carry (libsnd-reading.md 6.4).

    for (int v = 0; v < 24; ++v) {
        voices_[v] = Voice();
        sreg_[v] = ShadowRegs();
        dirty_[v] = 0;
    }
    score_ = Score();
    cur_ = Current();
    kon_ = koff_ = eon_ = 0;
    std::memset(envx_zero_, 0, sizeof envx_zero_);
    envx_index_ = 0;
    damper_ = 0;
    mono_ = false;
    reached_end_ = false;
    seqid_ = 0;
    phase_ = 0;
    next_tick_valid_ = false;
    next_tick_sample_ = 0;
    ticks_ = 0;
    ticks_since_play_ = 0;
    loop_jumps_ = 0;
    last_loop_jump_tick_ = 0;
    samples_ = 0;
    song_loaded_ = false;
    bank_loaded_ = false;
}

void MusicSynth::CheckBank(const Bank& bank) {
    // The game's layouts give the music VAB 0x1010..0x3E0A0 (layouts 0, 1)
    // or 0x1010..0x6C6D0 (layout 2, the opening and ending banks); the
    // reverb work area of mode 1 starts at 0x7D940 (libsnd-reading.md 6.3).
    if (bank.body.size() > kBankLimit - kBankAddress)
        MusicFatal("bank %s: 0x%zX bytes of samples, the game's largest music slot holds 0x%X", bank.name.c_str(),
                   bank.body.size(), kBankLimit - kBankAddress);
    if (bank.mvol > 127) MusicFatal("bank %s: master volume %u", bank.name.c_str(), bank.mvol);
}

void MusicSynth::LoadBank(const Bank& bank) {
    CheckBank(bank);
    // A different bank. SsVabClose (0x80174190) frees the VAB's SPU memory
    // and its slot and leaves libsnd's voice records alone, so on the PSX a
    // record still names a tone of the closed VAB, and the next SsSepSetVol
    // or pitch bend of its sequence reads that index in the new VAB's tone
    // table - past its end when the new bank has fewer programs. Here every
    // sequencer voice is keyed off and its record's references into the bank
    // go back to _SsVmInit's values (seqid -1, tone 0xFF, vel 0), so nothing
    // reaches a voice through the old bank's indices; ENVX, age and priority
    // (what the allocator reads of the SPU) stay. The release tails go on, as
    // on the PSX, over the new bank's samples (libsnd-reading.md 3.11).
    if (bank_loaded_ && bank.name != bank_.name) {
        for (int v = 0; v < kSeqVoices; ++v) {
            Voice& vo = voices_[v];
            if (vo.keyed) KeyOffNow(v);
            vo.seqid = -1;
            vo.block = 0;
            vo.prog = 0;
            vo.tone = 0xFF;
            vo.note = 0;
            vo.vel = 0;
        }
    }
    bank_ = bank;
    spu_.WriteRam(kBankAddress, bank_.body.data(), static_cast<std::uint32_t>(bank_.body.size()));
    bank_loaded_ = true;
}

// ---------------------------------------------------------------------------
// The score: _SsInitSoundSep (0x8016BE30), the tick arithmetic.

std::int16_t MusicSynth::StepFor(std::int32_t bpm) const {
    // _SsInitSoundSep 0x8016C0C4.. / _SsGetMetaEvent 0x8016B1D4..: ticks x 10
    // per call = resolution * bpm * 10 / (60 * tick), rounded up when the
    // remainder exceeds 30 * tick. Below one (x10) per call libsnd counts calls
    // per tick instead (the "slow" mode, +6E >= 0).
    const std::uint64_t x = SpeedOf(bpm);
    const std::uint64_t den = 60u * kSeqTick;
    std::uint64_t q = x / den;
    if (x % den > 30u * kSeqTick) ++q;
    // the step is a halfword (+70): above it the PSX's would wrap
    if (q > 0x7FFF) MusicFatal("song %u: resolution %d at %d bpm, %llu tenths of a tick a VSync (above 0x7FFF)",
                               song_.number, score_.resolution, bpm, static_cast<unsigned long long>(q));
    return static_cast<std::int16_t>(q);
}

std::uint64_t MusicSynth::SpeedOf(std::int32_t bpm) const {
    // resolution * bpm * 10, in 64 bits: LoadSong bounds the resolution to
    // 0x7FFF and the tempo is at least 1 (bpm at most 60,000,000), so this
    // cannot wrap where libsnd's 32-bit multiply would for such a song.
    if (score_.resolution <= 0 || bpm <= 0) MusicFatal("song %u: resolution %d, %d bpm", song_.number, score_.resolution, bpm);
    return static_cast<std::uint64_t>(score_.resolution) * static_cast<std::uint64_t>(bpm) * 10u;
}

void MusicSynth::InitScore(const Song& song) {
    Score& s = score_;
    const std::uint16_t seq_l = s.seq_l, seq_r = s.seq_r;
    s = Score();
    s.seq_l = seq_l;
    s.seq_r = seq_r;
    s.vab = 0;
    for (int c = 0; c < 16; ++c) {
        s.pan[c] = 64;
        s.prog[c] = static_cast<std::uint8_t>(c);
        s.vol[c] = 127;
    }
    s.resolution = static_cast<std::int16_t>(song.resolution);
    // bpm = 60,000,000 / tempo, rounded up when the remainder exceeds half
    // the tempo (0x8016BFEC..0x8016C03C).
    const std::uint32_t q = 60000000u / song.tempo, r = 60000000u % song.tempo;
    s.bpm_init = static_cast<std::int32_t>((song.tempo >> 1) < r ? q + 1 : q);
    s.bpm = s.bpm_init;
    s.pos = 0;
    s.start_pos = 0;
    s.loop_pos = 0;
    s.first_delta = static_cast<std::int32_t>(song.events[0].tick) * 10;
    s.delta = s.first_delta;
    const std::uint64_t x = SpeedOf(s.bpm);
    if (x < 60u * kSeqTick) {
        s.countdown = static_cast<std::int16_t>(600u * kSeqTick / static_cast<std::uint32_t>(s.resolution * s.bpm));
        s.step = s.countdown;
    } else {
        s.countdown = -1;
        s.step = StepFor(s.bpm);
    }
    s.step_init = s.step;
}

std::int32_t MusicSynth::ReadDelta() const {
    // _SsReadDeltaValue (0x8016B44C): the variable-length delta after the
    // event just read, times 10. In the event table that is the next event's
    // tick minus this one's.
    const auto& ev = song_.events;
    if (score_.pos == 0 || score_.pos >= ev.size())
        MusicFatal("song %u: a delta read past the end of the events", song_.number);
    return static_cast<std::int32_t>(ev[score_.pos].tick - ev[score_.pos - 1].tick) * 10;
}

// ---------------------------------------------------------------------------
// SsSeqCalledTbyT (0x8016CEC0), once a VSync.

void MusicSynth::TickOnce() {
    ++ticks_;
    Flush();
    Score& s = score_;
    if (s.flags & kPlay) {
        ++ticks_since_play_;
        SeqPlay();
        if (s.flags & kCres) SndCrescendo();
        if (s.flags & kDecres) SndDecrescendo();
        if (s.flags & (kAccel | kRitard)) MusicFatal("SsSeqCalledTbyT: accelerando / ritardando (_SsSndTempo) are not implemented");
    }
    if (s.flags & kPause) {  // _SsSndPause (0x8016D6B4)
        VmSeqKeyOff();
        s.f2b = 0;
        s.flags &= ~kPause;
    }
    if (s.flags & kReplay) {  // _SsSndReplay (0x8016DE44)
        s.f2b = 1;
        s.flags &= ~kReplay;
    }
    if (s.flags & kStop) {
        SndStop();
        s.flags = 0;
    }
}

void MusicSynth::SeqPlay() {
    // _SsSeqPlay (0x8016D784).
    Score& s = score_;
    const std::int16_t step = s.step;
    const std::int32_t left = s.delta - step;
    if (left > 0) {
        if (s.countdown > 0) {
            --s.countdown;
        } else if (s.countdown == 0) {
            s.countdown = step;
            --s.delta;
        } else {
            s.delta = left;
        }
        return;
    }
    if (step < s.delta) return;
    std::int32_t acc = s.delta;
    for (;;) {
        GetSeqData();
        if (s.delta == 0) continue;
        acc += s.delta;
        if (acc < step) continue;
        s.delta = acc - step;
        return;
    }
}

void MusicSynth::GetSeqData() {
    // _SsGetSeqData (0x8016DA58): one event and the delta after it. The
    // event table has running status resolved; libsnd's handlers for 0x90,
    // 0xB0, 0xC0, 0xE0 and 0xFF are the only ones (any other status is read
    // as nothing and would desynchronise the stream).
    Score& s = score_;
    if (s.pos >= song_.events.size()) MusicFatal("song %u: read past the end of track", song_.number);
    const SongEvent e = song_.events[s.pos++];
    s.ch = static_cast<std::uint8_t>(e.status & 0x0F);
    switch (e.status & 0xF0) {
    case 0x90:  // _SsNoteOn (0x8016B2EC), the delta read first
        s.delta = ReadDelta();
        if (s.seq_l == 0) break;
        if (e.d2 != 0) {
            if ((s.mute >> s.ch) & 1) break;
            VmKeyOn(s.prog[s.ch], e.d1, e.d2, s.pan[s.ch]);
        } else {
            VmKeyOff(s.prog[s.ch], e.d1);
        }
        break;
    case 0xB0:
        ControlChange(e.d1, e.d2);
        break;
    case 0xC0:  // _SsSetProgramChange (0x8016B3D4)
        s.prog[s.ch] = e.d1;
        s.delta = ReadDelta();
        break;
    case 0xE0:  // _SsSetPitchBend (0x8016AE1C): the MSB only
        VmPitchBend(s.prog[s.ch], e.d2);
        s.delta = ReadDelta();
        break;
    case 0xF0:
        if (e.d1 == 0x2F) {
            Eof();
        } else if (e.d1 == 0x51) {
            // _SsGetMetaEvent (0x8016B114): bpm = 60e6 / tempo, truncated
            // here (unlike the header's), and the step again.
            if (e.meta == 0) MusicFatal("song %u: tempo 0", song_.number);
            s.bpm = static_cast<std::int32_t>(60000000u / e.meta);
            const std::uint64_t x = SpeedOf(s.bpm);
            if (x < 60u * kSeqTick)
                MusicFatal("song %u: a tempo change into libsnd's slow mode is not implemented", song_.number);
            s.countdown = -1;
            s.step = StepFor(s.bpm);
            s.delta = ReadDelta();
        } else {
            MusicFatal("song %u: meta event 0x%02X (libsnd reads only 0x2F and, as a tempo, any other)", song_.number, e.d1);
        }
        break;
    default:
        MusicFatal("song %u: status 0x%02X, which libsnd's _SsGetSeqData does not read", song_.number, e.status);
    }
}

void MusicSynth::ControlChange(std::uint8_t cc, std::uint8_t value) {
    // _SsSetControlChange (0x8016AED4) and the handlers the game installs at
    // 0x801627B8 (libsnd-reading.md 2.4).
    Score& s = score_;
    switch (cc) {
    case 0:
        MusicFatal("song %u: controller 0 (bank select: libsnd switches the score's VAB) is not implemented", song_.number);
    case 6:
        DataEntry(value);
        return;
    case 7:  // _SsContMainVol (0x8016A860): the voices first, then the channel
        VmSetVol(s.prog[s.ch], value, s.pan[s.ch]);
        s.vol[s.ch] = value;
        s.delta = ReadDelta();
        return;
    case 10:  // _SsContPanpot (0x8016A938)
        VmSetVol(s.prog[s.ch], s.vol[s.ch], value);
        s.pan[s.ch] = value;
        s.delta = ReadDelta();
        return;
    case 11:
    case 64:  // both go to _SsContDamper (0x8016AA08) in this build
        damper_ = value < 64 ? 0 : 2;
        s.delta = ReadDelta();
        return;
    case 91:
    case 121:
        MusicFatal("song %u: controller %u - the game installs no handler (a call to address 0 on the PSX)", song_.number, cc);
    case 98:
    case 100:
    case 101:
        MusicFatal("song %u: controller %u (NRPN / RPN data entry) is not implemented (no song uses it)", song_.number, cc);
    case 99:
        Nrpn2(value);
        return;
    default:  // every other controller: libsnd reads the delta and nothing else
        s.delta = ReadDelta();
        return;
    }
}

void MusicSynth::Nrpn2(std::uint8_t value) {
    // _SsContNrpn2 (0x8016ABF0): 20 marks the loop start (the position after
    // the marker's delta, so the end jumps straight to the next event), 30 the
    // loop end.
    Score& s = score_;
    if (value == 20) {
        s.nrpn = value;
        s.f27 = 1;
        s.delta = ReadDelta();
        s.loop_pos = s.pos;
        return;
    }
    if (value == 30) {
        s.nrpn = value;
        if (s.loop_count == 0) {
            s.f10 = 0;
            s.delta = ReadDelta();
            return;
        }
        if (s.loop_count < 127) {
            --s.loop_count;
            s.delta = ReadDelta();
            if (s.loop_count != 0) {
                s.pos = s.loop_pos;
                ++loop_jumps_;
                last_loop_jump_tick_ = ticks_since_play_;
            } else {
                s.f10 = 0;
            }
            return;
        }
        ReadDelta();  // read and dropped
        s.pos = s.loop_pos;
        s.delta = 0;
        ++loop_jumps_;
        last_loop_jump_tick_ = ticks_since_play_;
        return;
    }
    s.nrpn = value;
    ++s.f2a;
    s.delta = ReadDelta();
}

void MusicSynth::DataEntry(std::uint8_t value) {
    // _SsContDataEntry (0x8016A430).
    Score& s = score_;
    if (s.f27 == 1 && s.f10 == 0) {
        s.loop_count = value;
        s.f27 = 0;
        s.f10 = 1;
        s.delta = ReadDelta();
        return;
    }
    if (s.f29 == 2) MusicFatal("song %u: RPN data entry (tone attributes) is not implemented", song_.number);
    if (s.f2a == 2) MusicFatal("song %u: NRPN data entry - the game installs no handlers (a call to address 0)", song_.number);
    s.delta = ReadDelta();
}

void MusicSynth::Eof() {
    // _SsSeqGetEof (0x8016D894).
    Score& s = score_;
    ++s.plays;
    if (s.loops_req == 0 || s.plays < s.loops_req) {
        s.pos = s.start_pos;
        s.f27 = 0;
        s.delta = 0;
        if (s.loops_req != 0) s.loop_pos = s.start_pos;
        return;
    }
    s.flags &= ~(kPlay | kReplay | kPause);
    s.flags |= kEnded | kStop;
    reached_end_ = true;
    s.f2b = 0;
    s.loop_pos = s.start_pos;
    VmSeqKeyOff();
    s.delta = s.step;
}

void MusicSynth::SndStop() {
    // _SsSndStop (0x8016DEAC).
    Score& s = score_;
    s.flags &= ~(kPlay | kPause | kReplay);
    s.flags |= kStop;
    VmSeqKeyOff();
    s.f2b = 0;
    s.f27 = 0; s.f13 = 0; s.f14 = 0; s.f29 = 0; s.f15 = 0; s.nrpn = 0; s.f2a = 0;
    s.ch = 0;
    s.plays = 0;
    s.loop_count = 0;
    s.f10 = 0;
    s.delta = s.first_delta;
    s.bpm = s.bpm_init;
    s.step = s.step_init;
    s.pos = s.start_pos;
    s.loop_pos = s.start_pos;
    for (int c = 0; c < 16; ++c) {
        s.prog[c] = static_cast<std::uint8_t>(c);
        s.pan[c] = 64;
        s.vol[c] = 127;
    }
}

// ---------------------------------------------------------------------------
// Volume ramps: _SsSndSetVolData (0x8016E764), _SsSndCrescendo (0x8016D134),
// _SsSndDecrescendo (0x8016D414).

void MusicSynth::SetVolData(int amount, int frames) {
    Score& s = score_;
    if (s.flags & (kStop | 0x100)) return;
    if (amount == 0) return;
    const std::uint32_t mag = static_cast<std::uint32_t>(amount < 0 ? -amount : amount);
    const std::uint32_t f = static_cast<std::uint32_t>(frames);
    s.v_target = static_cast<std::int16_t>(amount);
    s.v_total = f;
    s.v_count = static_cast<std::int16_t>(amount);
    s.v_left = f;
    if (f < mag) {
        if (f == 0) MusicFatal("a volume ramp over 0 frames (a division by zero on the PSX)");
        s.v_step = static_cast<std::int16_t>(-static_cast<std::int32_t>(mag / f));
    } else {
        s.v_step = static_cast<std::int16_t>(f / mag);
    }
}

void MusicSynth::SndCrescendo() {
    Score& s = score_;
    --s.v_left;
    const std::int16_t step = s.v_step;
    if (step > 0) {
        if (s.v_left % static_cast<std::uint32_t>(step) != 0) return;
        s.v_count = static_cast<std::int16_t>(s.v_count - 1);
        if (s.v_count < 0) {
            VmSetSeqVol(127, 127, true);
            s.flags &= ~kCres;
        } else {
            const int l = s.seq_l, r = s.seq_r;
            if (!(l + s.v_count < l + 1)) VmSetSeqVol(l + 1, r + 1, true);
        }
    } else if (step < 0) {
        s.v_count = static_cast<std::int16_t>(s.v_count + step);
        if (s.v_count < 0) {
            VmSetSeqVol(127, 127, true);
            s.flags &= ~kCres;
        } else {
            const int l = s.seq_l, r = s.seq_r;
            if (l - step >= 127 && r - step >= 127) VmSetSeqVol(127, 127, true);
            const std::uint32_t done = (s.v_total - s.v_left) * static_cast<std::uint32_t>(-step);
            if (done < static_cast<std::uint32_t>(static_cast<std::int32_t>(s.v_target)))
                VmSetSeqVol(l - step, r - step, true);
        }
    } else {
        return;
    }
    if (s.v_left == 0 || s.v_count == 0) s.flags &= ~kCres;
}

void MusicSynth::SndDecrescendo() {
    Score& s = score_;
    --s.v_left;
    const std::int16_t step = s.v_step;
    if (step > 0) {
        if (s.v_left % static_cast<std::uint32_t>(step) != 0) return;
        s.v_count = static_cast<std::int16_t>(s.v_count - 1);
        if (s.v_count <= 0) {
            s.flags &= ~kDecres;
        } else {
            const int l = s.seq_l, r = s.seq_r;
            if (l - s.v_count <= 0 || r - s.v_count <= 0 || l == 1) VmSetSeqVol(1, 1, true);
            else VmSetSeqVol(l - 1, r - 1, true);
        }
    } else {
        s.v_count = static_cast<std::int16_t>(s.v_count + step);
        if (s.v_count <= 0) {
            s.flags &= ~kDecres;
        } else {
            const int l = s.seq_l, r = s.seq_r;
            const std::uint32_t done = (s.v_total - s.v_left) * static_cast<std::uint32_t>(-step);
            if (static_cast<std::uint32_t>(static_cast<std::int32_t>(s.v_target)) < done) VmSetSeqVol(1, 1, true);
            else if (-step < l) VmSetSeqVol(l + step, r + step, true);
            else VmSetSeqVol(1, 1, true);
        }
    }
    if (s.v_left == 0 || s.v_count == 0) s.flags &= ~kDecres;
}

// ---------------------------------------------------------------------------
// The game-level calls.

void MusicSynth::Play(const Song& song, int volume, int frames, int loop_count) {
    if (!bank_loaded_) MusicFatal("Play(song %u): no bank loaded", song.number);
    if (song.bank != bank_.name)
        MusicFatal("Play(song %u): the song wants bank %s, %s is loaded", song.number, song.bank.c_str(), bank_.name.c_str());
    if (volume < 1 || volume > 127) MusicFatal("Play: volume %d outside 1..127", volume);
    if (frames < 1) MusicFatal("Play: a crescendo over %d frames (the PSX divides by it)", frames);
    if (loop_count < 0) MusicFatal("Play: loop count %d", loop_count);
    // SsSepOpenJ when the file loaded (_SsInitSoundSep), then Music_Play
    // (0x80162610): 0x8015DCF0 and 0x8015DE8C.
    song_ = song;
    song_loaded_ = true;
    seqid_ = static_cast<std::int16_t>(song.sub << 8);
    InitScore(song_);
    SndStop();                       // SsSepStop
    VmSetSeqVol(0, 0, true);         // SsSepSetVol(0, 0) -> 1, 1
    // SsSepPlay(SSPLAY_PLAY, loop_count) -> Snd_SetPlayMode (0x8016C510)
    Score& s = score_;
    s.flags &= ~(kEnded | kStop);
    s.loops_req = static_cast<std::int16_t>(loop_count);
    s.flags |= kPlay;
    s.plays = 0;
    s.f2b = 1;
    VmSetSeqVol(s.seq_l, s.seq_r, true);
    Crescendo(volume, frames);       // 0x8015DE8C -> SsSepSetCrescendo
    ticks_since_play_ = 0;
    loop_jumps_ = 0;
    last_loop_jump_tick_ = 0;
    reached_end_ = false;
}

void MusicSynth::SetVolume(int left, int right) {
    if (left < 0 || right < 0) MusicFatal("SetVolume(%d, %d)", left, right);
    VmSetSeqVol(left, right, true);
}

void MusicSynth::Crescendo(int amount, int frames) {
    SetVolData(amount, frames);
    score_.flags |= kCres;
    score_.flags &= ~kDecres;
}

void MusicSynth::Decrescendo(int amount, int frames) {
    SetVolData(amount, frames);
    score_.flags |= kDecres;
    score_.flags &= ~kCres;
}

void MusicSynth::SetMono(bool mono) { mono_ = mono; }

bool MusicSynth::Ended() const {
    if (!reached_end_) return false;
    for (int v = 0; v < kSeqVoices; ++v)
        if (spu_.VoiceEnvelope(v) != 0) return false;
    return true;
}

void MusicSynth::SetMasterVolume(int left, int right) {
    if (left < 0 || left > 127 || right < 0 || right > 127) MusicFatal("SetMasterVolume(%d, %d): 0..127", left, right);
    spu_.SetMainVolumeLeft(static_cast<std::uint16_t>((left * 129) & 0x7FFF));
    spu_.SetMainVolumeRight(static_cast<std::uint16_t>((right * 129) & 0x7FFF));
}

void MusicSynth::Pause() {
    // SsSepPause -> _SsSndSetPauseMode (0x8016C1E0); the voices are keyed off
    // at the next VSync.
    if (!song_loaded_) return;
    score_.flags &= ~(kPlay | kReplay);
    score_.flags |= kPause;
}

void MusicSynth::Replay() {
    // SsSepReplay -> _SsSndSetReplayMode (0x8016C418): from where it paused;
    // the notes that were sounding are not keyed again.
    if (!song_loaded_) return;
    if (score_.flags & (kStop | kEnded | 0x100)) return;
    score_.flags &= ~kPause;
    score_.flags |= kReplay | kPlay;
}

void MusicSynth::Stop() {
    if (song_loaded_) SndStop();
}

// ---------------------------------------------------------------------------
// The voice manager.

bool MusicSynth::VSetUp(int vab, int prog) {
    // _SsVmVSetUp (0x80174068): one VAB (id 0) is open; 128 programs (VAB
    // version 7). Sets the current VAB, program and its tone block.
    if (vab != 0) MusicFatal("song %u: VAB %d (only the music bank, VAB 0, is loaded)", song_.number, vab);
    if (prog < 0 || prog >= 128) return false;
    const std::uint8_t block = bank_.programs[prog].block;
    // libsnd stores, for a program with no tones, the next program's block
    // (SsVabOpenHeadWithMode 0x80174804); such a program selects no tone, so
    // 0xFF (>= ps) gives the same: no voice.
    cur_.block = block;
    return true;
}

const BankTone& MusicSynth::ToneAt(int block, int tone) const {
    if (block < 0 || tone < 0 || tone >= 16 ||
        static_cast<std::size_t>(block) * 16 + static_cast<std::size_t>(tone) >= bank_.tones.size())
        MusicFatal("bank %s: block %d tone %d, the bank has %zu tones", bank_.name.c_str(), block, tone,
                   bank_.tones.size());
    return bank_.tones[static_cast<std::size_t>(block) * 16 + static_cast<std::size_t>(tone)];
}

const BankTone& MusicSynth::ToneOf(const Voice& v) const { return ToneAt(v.block, v.tone); }

void MusicSynth::PanVolumes(std::uint32_t base, int tpan, int mpan, int cpan, std::uint32_t* l, std::uint32_t* r) const {
    // The pan and square law shared by _SsVmKeyOnNow, _SsVmSetVol and
    // _SsVmSetSeqVol: base is the volume before the sequence volume.
    const std::uint32_t sl = score_.seq_l, sr = score_.seq_r;
    std::uint32_t left = base * sl / 127, right = base * sr / 127;
    if (tpan < 64) right = right * static_cast<std::uint32_t>(tpan) / 63;
    else left = left * static_cast<std::uint32_t>(127 - tpan) / 63;
    if (mpan < 64) right = right * static_cast<std::uint32_t>(mpan) / 63;
    else left = left * static_cast<std::uint32_t>(127 - mpan) / 63;
    if (cpan < 64) right = right * static_cast<std::uint32_t>(cpan) / 63;
    else left = left * static_cast<std::uint32_t>(127 - cpan) / 63;
    // SsSetMono (0x8017413C, the options screen): both sides the louder.
    if (mono_) {
        if (left < right) left = right;
        else right = left;
    }
    *l = left * left / 16383;
    *r = right * right / 16383;
}

void MusicSynth::VmKeyOn(int prog, int note, int vel, int pan) {
    // _SsVmKeyOn (0x801719A4).
    if (!VSetUp(score_.vab, prog)) return;
    cur_.seqid = seqid_;
    cur_.note = static_cast<std::uint8_t>(note);
    // velocity x the channel's volume / 127 (the seqid 0x21 case is the
    // effects', never a song's)
    cur_.vel = static_cast<std::uint8_t>(vel * score_.vol[score_.ch] / 127);
    cur_.pan = static_cast<std::uint8_t>(pan);
    const BankProgram& pg = bank_.programs[prog];
    cur_.pmvol = pg.mvol;
    cur_.pmpan = pg.mpan;
    cur_.tones = pg.tones;
    if (cur_.block >= bank_.ps) return;
    // _SsVmSelectToneAndVag (0x80173A30): every tone whose range holds the note
    std::uint8_t sel_tone[16], sel_vag[16];
    int n = 0;
    for (int t = 0; t < cur_.tones; ++t) {
        const BankTone& tn = bank_.tones[static_cast<std::size_t>(cur_.block) * 16 + static_cast<std::size_t>(t)];
        if (cur_.note < tn.min || cur_.note > tn.max) continue;
        sel_vag[n] = static_cast<std::uint8_t>(tn.vag);
        sel_tone[n] = static_cast<std::uint8_t>(t);
        ++n;
    }
    for (int i = 0; i < n; ++i) {
        cur_.vag = sel_vag[i];
        cur_.tone = sel_tone[i];
        const BankTone& tn = bank_.tones[static_cast<std::size_t>(cur_.block) * 16 + cur_.tone];
        cur_.tprior = tn.prior;
        cur_.tvol = tn.vol;
        cur_.tpan = tn.pan;
        cur_.center = tn.center;
        cur_.shift = tn.shift;
        cur_.tmode = tn.mode;
        const int v = Alloc();
        cur_.voice = v;
        if (v >= kSeqVoices) continue;
        Voice& vo = voices_[v];
        vo.keyed = 1;
        vo.age = 0;
        vo.seqid = seqid_;
        vo.vab = 0;
        vo.block = cur_.block;
        vo.prog = static_cast<std::int16_t>(prog);
        vo.vel = cur_.vel;
        vo.pan = cur_.pan;
        vo.tone = cur_.tone;
        vo.note = cur_.note;
        vo.prior = cur_.tprior;
        vo.vag = cur_.vag;
        DoAllocate();
        if (cur_.vag == 0xFF)
            MusicFatal("song %u: a noise tone (sample 255, vmNoiseOn) is not implemented (no music bank has one)", song_.number);
        KeyOnNow(NoteToPitch());
    }
}

int MusicSynth::Alloc() {
    // _SsVmAlloc (0x80172168): the first voice that is free (not keyed and
    // ENVX 0 at the last flush); else among the voices of priority <= the
    // tone's, the lowest; then the lowest ENVX; then the oldest.
    int chosen = 99;
    std::uint16_t best_env = 0xFFFF;
    std::int16_t best_age = 0;
    int best = 99, candidates = 0;
    std::int16_t best_prior = cur_.tprior;
    for (int v = 0; v < kSeqVoices; ++v) {
        const Voice& vo = voices_[v];
        if (vo.keyed == 0 && vo.envx == 0) {
            chosen = v;
            break;
        }
        if (vo.prior < best_prior) {
            best_prior = vo.prior;
            best = v;
            best_env = vo.envx;
            best_age = vo.age;
            candidates = 1;
        } else if (vo.prior == best_prior) {
            ++candidates;
            if (vo.envx < best_env) {
                best_age = vo.age;
                best_env = vo.envx;
                best = v;
            } else if (vo.envx == best_env && best_age < vo.age) {
                best_age = vo.age;
                best = v;
            }
        }
    }
    if (chosen == 99) chosen = candidates ? best : kSeqVoices;
    if (chosen < kSeqVoices) {
        for (int v = 0; v < kSeqVoices; ++v) voices_[v].age = static_cast<std::int16_t>(voices_[v].age + 1);
        voices_[chosen].age = 0;
        voices_[chosen].prior = cur_.tprior;
        if (voices_[chosen].keyed == 2) MusicFatal("a noise voice (key state 2) is not implemented");
    }
    return chosen;
}

void MusicSynth::DoAllocate() {
    // _SsVmDoAllocate (0x801703F4): ENVX taken as full until the next flush,
    // the voice's zero-ENVX history cleared; SSA, ADSR1, ADSR2 (+ the damper)
    // into the shadow registers.
    const int v = cur_.voice;
    voices_[v].envx = 0x7FFF;
    for (auto& h : envx_zero_) h &= ~(1u << v);
    const int vag = cur_.vag;
    if (vag < 1 || vag > static_cast<int>(bank_.samples.size()))
        MusicFatal("song %u: tone sample %d of %zu", song_.number, vag, bank_.samples.size());
    sreg_[v].ssa = static_cast<std::uint16_t>((kBankAddress + bank_.samples[static_cast<std::size_t>(vag - 1)].offset) >> 3);
    dirty_[v] |= 8;
    const BankTone& tn = bank_.tones[static_cast<std::size_t>(cur_.block) * 16 + cur_.tone];
    sreg_[v].adsr1 = tn.adsr1;
    sreg_[v].adsr2 = static_cast<std::uint16_t>(tn.adsr2 + damper_);
    dirty_[v] |= 0x30;
}

std::uint16_t MusicSynth::NoteToPitch() const {
    // note2pitch (0x801723D4): 16 fine steps a semitone from shift >> 3.
    int fine = cur_.shift >> 3;
    if (fine > 15) fine = 15;
    const int x = cur_.note + 60 - cur_.center;
    const int oct = x / 12, semi = x - oct * 12;
    if (semi < 0) MusicFatal("song %u: note %d below its tone's centre %d by more than 60 (note2pitch reads before its table)",
                             song_.number, cur_.note, cur_.center);
    std::uint32_t p = Pitches().v[semi * 16 + fine];
    const int sh = oct - 5;
    if (sh > 0) p <<= sh;
    else if (sh < 0) p >>= -sh;
    return static_cast<std::uint16_t>(p);
}

std::uint16_t MusicSynth::NoteToPitch2(int note, int fine, const BankTone& tone) const {
    // note2pitch2 (0x80172498): the fine part carries into the next semitone.
    int f = (fine + tone.shift) >> 3;
    int carry = 0;
    if (f >= 16) {
        carry = 1;
        f -= 16;
    }
    const int x = static_cast<std::int16_t>(note + 60 - tone.center + carry);
    const int oct = x / 12, semi = x - oct * 12;
    if (semi < 0) MusicFatal("song %u: bent note %d below its tone's centre %d by more than 60", song_.number, note, tone.center);
    std::uint32_t p = Pitches().v[semi * 16 + f];
    const int sh = oct - 5;
    if (sh > 0) p <<= sh;
    else if (sh < 0) p >>= -sh;
    return static_cast<std::uint16_t>(p);
}

void MusicSynth::KeyOnNow(std::uint16_t pitch) {
    // _SsVmKeyOnNow (0x80172B74).
    const int v = cur_.voice;
    std::uint32_t a = static_cast<std::uint32_t>(cur_.vel) * (static_cast<std::uint32_t>(bank_.mvol) * 0x3FFF) / 16129;
    a = a * cur_.pmvol * cur_.tvol / 16129;
    std::uint32_t l, r;
    PanVolumes(a, cur_.tpan, cur_.pmpan, cur_.pan, &l, &r);
    sreg_[v].vol_l = static_cast<std::uint16_t>(l);
    sreg_[v].vol_r = static_cast<std::uint16_t>(r);
    sreg_[v].pitch = pitch;
    dirty_[v] |= 7;
    voices_[v].pitch = pitch;
    voices_[v].keyed = 1;
    const std::uint32_t bit = 1u << v;
    if (cur_.tmode & 4) eon_ |= bit;
    else eon_ &= ~bit;
    kon_ |= bit;
    koff_ &= ~kon_;
}

void MusicSynth::KeyOffNow(int v) {
    // _SsVmKeyOffNow (0x80172AA4).
    const std::uint32_t bit = 1u << v;
    voices_[v].keyed = 0;
    voices_[v].pitch = 0;
    voices_[v].vag = 0;
    koff_ |= bit;
    kon_ &= ~koff_;
}

void MusicSynth::VmKeyOff(int prog, int note) {
    // _SsVmKeyOff (0x80171EF4): every voice of this sequence with the note,
    // the program and the VAB.
    for (int v = 0; v < kSeqVoices; ++v) {
        const Voice& vo = voices_[v];
        if (vo.note != note || vo.prog != prog || vo.seqid != seqid_ || vo.vab != score_.vab) continue;
        if (vo.vag == 0xFF) MusicFatal("song %u: noise key off (vmNoiseOff) is not implemented", song_.number);
        KeyOffNow(v);
    }
}

void MusicSynth::VmSeqKeyOff() {
    // _SsVmSeqKeyOff (0x80173994).
    for (int v = 0; v < kSeqVoices; ++v)
        if (voices_[v].seqid == seqid_) KeyOffNow(v);
}

void MusicSynth::VmPitchBend(int prog, int msb) {
    // _SsVmPitchBend (0x80173258) -> _SsVmPBVoice (0x80173058) on every voice
    // of this sequence with the program and VAB - whatever its channel, also
    // in release. The bend is not remembered for later notes.
    VSetUp(score_.vab, prog);
    const int bend = msb - 64;
    for (int v = 0; v < kSeqVoices; ++v) {
        Voice& vo = voices_[v];
        if (vo.seqid != seqid_ || vo.vab != score_.vab || vo.prog != prog) continue;
        // the current program's block (_SsVmPBVoice reads _svm_cur's), the
        // voice's tone; checked - a stale record must not index past the bank
        const BankTone& tn = ToneAt(cur_.block, vo.tone);
        int note = vo.note, fine = 0;
        if (bend > 0) {
            const int a = bend * tn.pbmax;
            const int q = a / 63;
            note += q;
            fine = (a - q * 63) * 2;
        } else if (bend < 0) {
            const int a = bend * tn.pbmin;
            const int q = a / 64;  // truncation toward zero, as the code's +63 / sra 6
            note = note + q - 1;
            fine = (a - q * 64) * 2 + 127;
        }
        sreg_[v].pitch = NoteToPitch2(note, fine, tn);
        dirty_[v] |= 4;
    }
}

void MusicSynth::VmSetVol(int prog, int vol, int pan) {
    // _SsVmSetVol (0x80173AE4): the voices of this sequence, program and VAB
    // re-volumed with the new channel volume over the velocity they were keyed
    // with (which already holds the old channel volume).
    VSetUp(score_.vab, prog);
    if (pan == 0) pan = 1;
    if (vol == 0) vol = 1;
    for (int v = 0; v < kSeqVoices; ++v) {
        Voice& vo = voices_[v];
        if (vo.seqid != seqid_ || vo.prog != prog || vo.vab != score_.vab) continue;
        std::int16_t& chvol = score_.vol[score_.ch];
        if (chvol != vol && chvol == 0) chvol = 1;
        std::uint32_t a = static_cast<std::uint32_t>(vo.vel * vol / 127);
        a = a * (static_cast<std::uint32_t>(bank_.mvol) * 0x3FFF) / 16129;
        const BankTone& tn = ToneOf(vo);
        a = a * bank_.programs[prog].mvol * tn.vol / 16129;
        std::uint32_t l, r;
        PanVolumes(a, tn.pan, bank_.programs[vo.prog].mpan, pan, &l, &r);
        sreg_[v].vol_l = static_cast<std::uint16_t>(l);
        sreg_[v].vol_r = static_cast<std::uint16_t>(r);
        dirty_[v] |= 3;
    }
}

void MusicSynth::VmSetSeqVol(int l, int r, bool apply) {
    // _SsVmSetSeqVol (0x80173348): 0 becomes 1, above 127 becomes 127; the
    // voices of the sequence re-volumed - over the keyed velocity times the
    // volume of the channel of the last event read, whatever the voice's.
    if (l == 0) l = 1;
    if (r == 0) r = 1;
    if (l > 127) l = 127;
    if (r > 127) r = 127;
    if (l < 0 || r < 0) MusicFatal("sequence volume %d, %d", l, r);
    score_.seq_l = static_cast<std::uint16_t>(l);
    score_.seq_r = static_cast<std::uint16_t>(r);
    if (!apply || !bank_loaded_) return;
    for (int v = 0; v < kSeqVoices; ++v) {
        Voice& vo = voices_[v];
        if (vo.seqid != seqid_) continue;
        std::uint32_t a = static_cast<std::uint32_t>(vo.vel * score_.vol[score_.ch] / 127);
        a = a * (static_cast<std::uint32_t>(bank_.mvol) * 0x3FFF) / 16129;
        if (vo.tone == 0xFF) continue;  // never keyed: _SsVmInit's tone 0xFF, vel 0 (volume 0 either way)
        const BankTone& tn = ToneOf(vo);
        a = a * bank_.programs[vo.prog].mvol * tn.vol / 16129;
        std::uint32_t lv, rv;
        PanVolumes(a, tn.pan, bank_.programs[vo.prog].mpan, vo.pan, &lv, &rv);
        sreg_[v].vol_l = static_cast<std::uint16_t>(lv);
        sreg_[v].vol_r = static_cast<std::uint16_t>(rv);
        dirty_[v] |= 3;
    }
}

void MusicSynth::Flush() {
    // _SsVmFlush (0x8017101C), at the start of every SsSeqCalledTbyT.
    envx_index_ = (envx_index_ + 1) & 15;
    envx_zero_[envx_index_] = 0;
    for (int v = 0; v < kSeqVoices; ++v) {
        voices_[v].envx = spu_.VoiceEnvelope(v);
        if (voices_[v].envx == 0) envx_zero_[envx_index_] |= 1u << v;
    }
    // A voice whose ENVX read 0 in history slots 0..14 is freed (slot 15 is
    // not in the AND - the code's loop bound).
    std::uint32_t all = 0xFFFFFFFFu;
    for (int i = 0; i < 15; ++i) all &= envx_zero_[i];
    for (int v = 0; v < kSeqVoices; ++v) {
        if (!(all & (1u << v))) continue;
        if (voices_[v].keyed == 2) MusicFatal("a noise voice (key state 2) is not implemented");
        voices_[v].keyed = 0;
    }
    kon_ &= ~koff_;
    // (the per-voice auto-volume / auto-pan hooks at +1C / +28 are the
    // effects' SsUtAutoVol / SsUtAutoPan, never set for a sequenced voice)
    writes_ = 0;
    for (int v = 0; v < 24; ++v) {
        const std::uint8_t f = dirty_[v];
        writes_ += ((f & 1) ? 2 : 0) + ((f & 4) ? 1 : 0) + ((f & 8) ? 1 : 0) + ((f & 0x10) ? 2 : 0);
        if (f & 1) {
            spu_.SetVoiceVolumeLeft(v, sreg_[v].vol_l);
            spu_.SetVoiceVolumeRight(v, sreg_[v].vol_r);
        }
        if (f & 4) spu_.SetVoicePitch(v, sreg_[v].pitch);
        if (f & 8) spu_.SetVoiceStartAddress(v, sreg_[v].ssa);
        if (f & 0x10) {
            spu_.SetVoiceAdsr1(v, sreg_[v].adsr1);
            spu_.SetVoiceAdsr2(v, sreg_[v].adsr2);
        }
        dirty_[v] = 0;
    }
    const std::uint32_t koff = koff_, kon = kon_;
    koff_ = 0;
    kon_ = 0;
    spu_.KeyOff(koff);
    spu_.KeyOn(kon);
    spu_.SetReverbMode(eon_);
    if (trace_fn_) {
        TickTrace t{ticks_, samples_, kon, koff, writes_, {}, {}, {}};
        for (int v = 0; v < 24; ++v) {
            t.pitch[v] = sreg_[v].pitch;
            t.note[v] = voices_[v].note;
            t.vag[v] = voices_[v].vag;
        }
        trace_fn_(t, trace_user_);
    }
}

// ---------------------------------------------------------------------------

void MusicSynth::SetTickPhase(int sub_samples) {
    if (sub_samples < 0 || sub_samples > 255 || ticks_ != 0) MusicFatal("SetTickPhase(%d) after the first tick or out of 0..255", sub_samples);
    phase_ = static_cast<std::uint64_t>(sub_samples) * kTickDen / 256;
}

void MusicSynth::Render(std::int16_t* stereo, int frames) {
    // VSync k (k = 0, 1, ...) falls at time phase + k * kTickNum / kTickDen
    // samples (+ the diagnostic offset); its flush acts from the first sample
    // computed after it, sample ceil(time).
    while (frames > 0) {
        if (!next_tick_valid_) {
            std::int64_t t = static_cast<std::int64_t>(phase_ + ticks_ * kTickNum);
            if (offset_fn_) t += static_cast<std::int64_t>(offset_fn_(ticks_ + 1, offset_user_)) * static_cast<std::int64_t>(kTickDen) / 256;
            if (t < 0) t = 0;
            const std::uint64_t ut = static_cast<std::uint64_t>(t);
            next_tick_sample_ = (ut + kTickDen - 1) / kTickDen;
            if (next_tick_sample_ < samples_) next_tick_sample_ = samples_;
            next_tick_valid_ = true;
        }
        if (next_tick_sample_ <= samples_) {
            next_tick_valid_ = false;
            TickOnce();
            continue;
        }
        std::uint64_t n = next_tick_sample_ - samples_;
        if (n > static_cast<std::uint64_t>(frames)) n = static_cast<std::uint64_t>(frames);
        spu_.Render(stereo, static_cast<int>(n));
        samples_ += n;
        stereo += 2 * n;
        frames -= static_cast<int>(n);
    }
}

} // namespace psx

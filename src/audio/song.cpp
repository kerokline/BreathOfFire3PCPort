// Loaders for the two music containers of docs/seq-format.md.
#include "audio/song.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace psx {
namespace {

void DefaultAbort(const char* message) { std::fprintf(stderr, "psx music: %s\n", message); }
MusicAbortHook g_abort_hook = DefaultAbort;

class Reader {
public:
    Reader(const std::uint8_t* data, std::size_t size, const char* what) : data_(data), size_(size), what_(what) {}
    void Need(std::size_t offset, std::size_t bytes) const {
        if (offset > size_ || bytes > size_ - offset)
            MusicFatal("%s: truncated (needs 0x%zX bytes at 0x%zX, the file has 0x%zX)", what_, bytes, offset, size_);
    }
    std::uint8_t U8(std::size_t o) const { Need(o, 1); return data_[o]; }
    std::uint16_t U16(std::size_t o) const { Need(o, 2); return static_cast<std::uint16_t>(data_[o] | data_[o + 1] << 8); }
    std::uint32_t U32(std::size_t o) const {
        Need(o, 4);
        return static_cast<std::uint32_t>(data_[o]) | static_cast<std::uint32_t>(data_[o + 1]) << 8 |
               static_cast<std::uint32_t>(data_[o + 2]) << 16 | static_cast<std::uint32_t>(data_[o + 3]) << 24;
    }
    std::string Name(std::size_t o, std::size_t n) const {
        Need(o, n);
        std::size_t len = 0;
        while (len < n && data_[o + len]) ++len;
        for (std::size_t i = len; i < n; ++i)
            if (data_[o + i]) MusicFatal("%s: name field at 0x%zX is not NUL-padded", what_, o);
        return std::string(reinterpret_cast<const char*>(data_ + o), len);
    }
    const std::uint8_t* Ptr(std::size_t o) const { return data_ + o; }
    std::size_t Size() const { return size_; }

private:
    const std::uint8_t* data_;
    std::size_t size_;
    const char* what_;
};

} // namespace

void SetMusicAbortHook(MusicAbortHook hook) { g_abort_hook = hook ? hook : DefaultAbort; }

[[noreturn]] void MusicFatal(const char* format, ...) {
    char message[320];
    va_list args;
    va_start(args, format);
    std::vsnprintf(message, sizeof message, format, args);
    va_end(args);
    g_abort_hook(message);
    std::abort();
}

void LoadSong(const std::uint8_t* data, std::size_t size, Song* out) {
    Reader r(data, size, "song");
    r.Need(0, 56);
    if (std::memcmp(data, "BF3S", 4) != 0) MusicFatal("song: bad magic (not BF3S)");
    if (r.U32(4) != 1) MusicFatal("song: version %u, this player reads version 1", r.U32(4));
    Song s;
    s.flags = r.U32(8);
    if (s.flags & ~1u) MusicFatal("song: unknown flags 0x%X", s.flags);
    s.bank = r.Name(12, 16);
    s.number = r.U16(28);
    s.sub = r.U16(30);
    s.resolution = r.U16(32);
    s.rhythm = r.U16(34);
    s.tempo = r.U32(36);
    const std::uint32_t count = r.U32(40);
    s.loop_start_tick = r.U32(44);
    s.loop_end_tick = r.U32(48);
    s.end_tick = r.U32(52);
    if (s.resolution == 0 || s.tempo == 0) MusicFatal("song %u: resolution %u, tempo %u", s.number, s.resolution, s.tempo);
    if (count == 0) MusicFatal("song %u: no events", s.number);
    r.Need(56, static_cast<std::size_t>(count) * 12);
    if (size != 56 + static_cast<std::size_t>(count) * 12)
        MusicFatal("song %u: 0x%zX bytes, the header's %u events make 0x%zX", s.number, size, count,
                   56 + static_cast<std::size_t>(count) * 12);
    s.events.resize(count);
    std::uint32_t last = 0;
    for (std::uint32_t i = 0; i < count; ++i) {
        const std::size_t o = 56 + static_cast<std::size_t>(i) * 12;
        SongEvent& e = s.events[i];
        e.tick = r.U32(o);
        e.status = r.U8(o + 4);
        e.d1 = r.U8(o + 5);
        e.d2 = r.U8(o + 6);
        e.meta = r.U32(o + 8);
        if (r.U8(o + 7) != 0) MusicFatal("song %u event %u: reserved byte set", s.number, i);
        if (e.tick < last) MusicFatal("song %u event %u: tick %u before %u", s.number, i, e.tick, last);
        last = e.tick;
        if (e.status < 0x80 || (e.status >= 0xF0 && e.status != 0xFF))
            MusicFatal("song %u event %u: status 0x%02X", s.number, i, e.status);
        if (e.status != 0xFF && (e.d1 > 0x7F || e.d2 > 0x7F))
            MusicFatal("song %u event %u: data byte above 0x7F", s.number, i);
    }
    const SongEvent& end = s.events.back();
    if (end.status != 0xFF || end.d1 != 0x2F) MusicFatal("song %u: the last event is not the end of track", s.number);
    if (end.tick != s.end_tick) MusicFatal("song %u: end tick %u, header %u", s.number, end.tick, s.end_tick);
    *out = std::move(s);
}

void LoadBank(const std::uint8_t* data, std::size_t size, Bank* out) {
    Reader r(data, size, "bank");
    r.Need(0, 36);
    if (std::memcmp(data, "BF3B", 4) != 0) MusicFatal("bank: bad magic (not BF3B)");
    if (r.U32(4) != 2) MusicFatal("bank: version %u, this player reads version 2", r.U32(4));
    Bank b;
    b.name = r.Name(8, 16);
    b.ps = r.U16(24);
    b.ts = r.U16(26);
    b.vs = r.U16(28);
    b.mvol = r.U8(30);
    b.pan = r.U8(31);
    b.attr1 = r.U8(32);
    b.attr2 = r.U8(33);
    if (b.ps > 128) MusicFatal("bank %s: %u programs", b.name.c_str(), b.ps);
    int with_tones = 0;
    for (int p = 0; p < 128; ++p) {
        const std::size_t o = 36 + static_cast<std::size_t>(p) * 8;
        BankProgram& g = b.programs[p];
        g.tones = r.U8(o);
        g.mvol = r.U8(o + 1);
        g.prior = r.U8(o + 2);
        g.mode = r.U8(o + 3);
        g.mpan = r.U8(o + 4);
        g.block = r.U8(o + 5);
        g.attr = r.U16(o + 6);
        if (g.tones > 16) MusicFatal("bank %s program %d: %u tones", b.name.c_str(), p, g.tones);
        if (g.tones == 0 && g.block != 0xFF) MusicFatal("bank %s program %d: no tones but block %u", b.name.c_str(), p, g.block);
        if (g.tones != 0) {
            if (g.block != with_tones) MusicFatal("bank %s program %d: block %u, expected %d", b.name.c_str(), p, g.block, with_tones);
            ++with_tones;
        }
    }
    if (with_tones != b.ps) MusicFatal("bank %s: %d programs with tones, header ps %u", b.name.c_str(), with_tones, b.ps);
    const std::size_t tone_at = 1060;
    const std::size_t sample_at = tone_at + 384 * static_cast<std::size_t>(b.ps);
    const std::size_t body_at = sample_at + 8 * static_cast<std::size_t>(b.vs);
    r.Need(tone_at, body_at - tone_at);
    b.tones.resize(static_cast<std::size_t>(b.ps) * 16);
    for (std::size_t i = 0; i < b.tones.size(); ++i) {
        const std::size_t o = tone_at + i * 24;
        BankTone& t = b.tones[i];
        t.prior = r.U8(o); t.mode = r.U8(o + 1); t.vol = r.U8(o + 2); t.pan = r.U8(o + 3);
        t.center = r.U8(o + 4); t.shift = r.U8(o + 5); t.min = r.U8(o + 6); t.max = r.U8(o + 7);
        t.vibw = r.U8(o + 8); t.vibt = r.U8(o + 9); t.porw = r.U8(o + 10); t.port = r.U8(o + 11);
        t.pbmin = r.U8(o + 12); t.pbmax = r.U8(o + 13);
        t.adsr1 = r.U16(o + 16); t.adsr2 = r.U16(o + 18); t.prog = r.U16(o + 20); t.vag = r.U16(o + 22);
        if (t.vag > b.vs && t.vag != 0xFF) MusicFatal("bank %s tone %zu: sample %u of %u", b.name.c_str(), i, t.vag, b.vs);
    }
    b.samples.resize(b.vs);
    std::uint32_t next = 0;
    for (std::size_t i = 0; i < b.samples.size(); ++i) {
        const std::size_t o = sample_at + i * 8;
        b.samples[i].offset = r.U32(o);
        b.samples[i].size = r.U32(o + 4);
        // libsnd's SsVabOpenHead places sample i at the bank's address plus
        // the sum of the sizes before it; the cache must say the same.
        if (b.samples[i].offset != next)
            MusicFatal("bank %s sample %zu: offset 0x%X, the sizes before it sum to 0x%X", b.name.c_str(), i + 1,
                       b.samples[i].offset, next);
        if (b.samples[i].size % 16) MusicFatal("bank %s sample %zu: size 0x%X is not whole blocks", b.name.c_str(), i + 1, b.samples[i].size);
        next += b.samples[i].size;
    }
    if (size - body_at != next)
        MusicFatal("bank %s: 0x%zX bytes of bodies, the samples sum to 0x%X", b.name.c_str(), size - body_at, next);
    b.body.assign(r.Ptr(body_at), r.Ptr(body_at) + next);
    *out = std::move(b);
}

} // namespace psx

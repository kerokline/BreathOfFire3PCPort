// DIV-0091: minimp3 behind the music seam (mp3.h; docs/mp3-decoder.md).
#include "audio/mp3.h"

#include <windows.h>

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <vector>

#include "bof3/symbols.gen.h"
#include "game/sound_callees.h"
#include "hook/log.h"

// third_party/minimp3/minimp3.h, verbatim (docs/THIRD_PARTY.md section 3).
// 16-bit output, the x86 SSE2 path when the CPU has it (its own test).
#define MINIMP3_IMPLEMENTATION
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsign-compare"
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wimplicit-fallthrough"
#include "minimp3/minimp3.h"
#pragma clang diagnostic pop

namespace mp3dec {
namespace {

// The seam's shape: the frame's byte count at [[decoder + 0x14] + 8], where
// Capcom's decoder keeps it and every caller of ours reads it (sound.cpp's
// Music_Decode and LoopFrame, sound_fuzz.cpp's stubs, the loop self-test's
// fake). Nothing else of the layout is read from outside.
struct Frame {
    std::uint32_t pad[2];
    std::uint32_t bytes;
};
struct Decoder {
    std::uint32_t pad[5];
    Frame* frame;
    const std::uint8_t* data;  // the file in memory (Music_Data, the caller's)
    std::uint32_t size;
    std::uint32_t pos;         // the next byte to decode
    bool opened, started;
    Frame frame_store;
    mp3dec_t dec;
};
static_assert(offsetof(Decoder, frame) == 0x14, "the frame pointer is read at decoder + 0x14");
static_assert(offsetof(Frame, bytes) == 8, "the frame's bytes are read at frame + 8");

// What the frame's byte count holds after the end of the stream is reported:
// Capcom's decoder writes nothing to `out` and leaves the last frame's count,
// 4,608, in place (measured on all 177 files, docs/mp3-decoder.md 2), so
// Music_Decode's `filled` steps over 1,152 samples it never wrote at every
// rewind and before a once-only track's zeros (known-defects.md D240). Ours
// stores the same, so the stream's logic is unchanged by the swap; the
// defect is the owner's call, not this entry's.
constexpr std::uint32_t kEosBytes = 1152 * 4;

// 44,100 Hz stereo, 1,152 samples a frame: what every frame of the game's
// files decodes to (docs/bgm-comparison.md 6.1).
constexpr std::uint32_t kFrameBytes = 1152 * 4;

Decoder* D(void* decoder) { return static_cast<Decoder*>(decoder); }

// "%X@%X": the size, then the address, upper-case hex as Crt_sprintf writes
// it (sound.cpp's kMemoryName); either case read.
bool ParseName(const char* name, std::uint32_t* size, std::uint32_t* address) {
    std::uint32_t v[2] = {0, 0};
    const char* p = name;
    for (int i = 0; i < 2; ++i) {
        int digits = 0;
        for (;; ++p, ++digits) {
            const char c = *p;
            unsigned d;
            if (c >= '0' && c <= '9') d = static_cast<unsigned>(c - '0');
            else if (c >= 'A' && c <= 'F') d = static_cast<unsigned>(c - 'A' + 10);
            else if (c >= 'a' && c <= 'f') d = static_cast<unsigned>(c - 'a' + 10);
            else break;
            if (digits >= 8) return false;
            v[i] = (v[i] << 4) | d;
        }
        if (digits == 0) return false;
        if (i == 0) {
            if (*p != '@') return false;
            ++p;
        }
    }
    if (*p != 0) return false;
    *size = v[0];
    *address = v[1];
    return true;
}

bool g_active;

}  // namespace

int __stdcall Create(void** decoder) {
    Decoder* d = new Decoder();
    std::memset(d, 0, sizeof *d);
    d->frame = &d->frame_store;
    mp3dec_init(&d->dec);
    *decoder = d;
    return 0;
}

// Capcom's fills a 0x2C-byte table of its memory I/O callbacks for SetIo to
// copy; ours reads the file through its own fields, so the table is zeros.
int __stdcall MemoryIo(void* io) {
    std::memset(io, 0, 0x2C);
    return 0;
}

int __stdcall SetIo(void*, void*) { return 0; }

int __stdcall Open(void* decoder, const char* name, int mode) {
    if (mode != -1) bof3::Fatal("DIV-0091: Mp3_Open with mode %d (the game passes -1)", mode);
    Decoder* d = D(decoder);
    std::uint32_t size, address;
    if (!ParseName(name, &size, &address)) bof3::Fatal("DIV-0091: Mp3_Open's name \"%s\" is not size@address", name);
    d->data = reinterpret_cast<const std::uint8_t*>(static_cast<std::uintptr_t>(address));
    d->size = size;
    d->pos = 0;
    d->opened = true;
    d->started = false;
    return 0;
}

int __stdcall Start(void* decoder) {
    Decoder* d = D(decoder);
    if (!d->opened) return -1;
    mp3dec_init(&d->dec);
    d->pos = 0;
    d->started = true;
    d->frame->bytes = 0;
    return 0;
}

// One frame to `out`; its bytes stored; kEndOfStream once the file holds no
// more frames. minimp3 reports a frame with no samples for bytes it skipped
// (none in the game's files, which are bare streams); those are passed over.
//
// On a stack of its own. mp3dec_decode_frame keeps its scratch
// (mp3dec_scratch_t: the granule buffers, the synthesis window, the bit
// reservoir - some 13 KB) on the stack, where Capcom's decoder kept its state
// in heap blocks; and the game's tasks run on 16 KB slices of the main
// thread's stack below WinMain's frame (Task_SetStackBase, docs/task_sched.md),
// the title's Music_Play among them. A decode there overran the slice into
// the next task's saved context (2026-10-10, the owner's crash at a float
// bit pattern with every register a float). So DecodeFrame runs with esp on
// a 256 KB block of ours, switched in and out around the one call.
namespace {

constexpr std::uint32_t kStackBytes = 256 * 1024;
unsigned char* g_stack;

int __stdcall DecodeFrame(void* decoder, void* out);

}  // namespace

int __stdcall Decode(void* decoder, void* out) {
    if (!g_stack) {
        g_stack = static_cast<unsigned char*>(VirtualAlloc(nullptr, kStackBytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        if (!g_stack) bof3::Fatal("DIV-0091: no memory for the decoder's stack");
        DWORD old;
        VirtualProtect(g_stack, 4096, PAGE_NOACCESS, &old);  // a guard page under it: an overrun faults, never wanders
    }
    const std::uint32_t top = (reinterpret_cast<std::uintptr_t>(g_stack) + kStackBytes - 16) & ~15u;
    int result;
    asm volatile(
        "movl %%esp, %%esi\n\t"
        "movl %[top], %%esp\n\t"
        "pushl %[out]\n\t"
        "pushl %[dec]\n\t"
        "call *%[fn]\n\t"
        "movl %%esi, %%esp\n\t"
        : "=a"(result)
        : [top] "r"(top), [out] "r"(out), [dec] "r"(decoder), [fn] "r"(&DecodeFrame)
        : "esi", "ecx", "edx", "memory", "cc");
    return result;
}

namespace {

int __stdcall DecodeFrame(void* decoder, void* out) {
    Decoder* d = D(decoder);
    if (!d->started) bof3::Fatal("DIV-0091: Mp3_Decode before Mp3_Start");
    for (;;) {
        if (d->pos >= d->size) {
            d->frame->bytes = kEosBytes;
            return sound::kEndOfStream;
        }
        mp3dec_frame_info_t info;
        const int samples = mp3dec_decode_frame(&d->dec, d->data + d->pos, static_cast<int>(d->size - d->pos),
                                                static_cast<mp3d_sample_t*>(out), &info);
        if (info.frame_bytes <= 0) {
            d->pos = d->size;
            d->frame->bytes = kEosBytes;
            return sound::kEndOfStream;
        }
        d->pos += static_cast<std::uint32_t>(info.frame_bytes);
        if (samples == 0) continue;
        if (info.channels != 2 || info.hz != 44100)
            bof3::Fatal("DIV-0091: a frame of %d channels at %d Hz at byte %u (the stream is 44,100 Hz stereo)",
                        info.channels, info.hz, d->pos - static_cast<std::uint32_t>(info.frame_bytes));
        d->frame->bytes = static_cast<std::uint32_t>(samples) * 4;
        return 0;
    }
}

}  // namespace

// Position 0 only: the file's first frame with the decoder's state reset,
// which is decoding from the start again (mp3-decoder-choice.md 3's rule).
int __stdcall Seek(void* decoder, int position) {
    if (position != 0) bof3::Fatal("DIV-0091: Mp3_Seek to %d (the game seeks to 0 alone)", position);
    Decoder* d = D(decoder);
    if (!d->started) bof3::Fatal("DIV-0091: Mp3_Seek before Mp3_Start");
    mp3dec_init(&d->dec);
    d->pos = 0;
    return 0;
}

int __stdcall Destroy(void* decoder) {
    delete D(decoder);
    return 0;
}

bool Active() { return g_active; }

// --- One file through one decoder, driven as the game drives the seam --------
namespace {

struct Entries {
    int (__stdcall* create)(void**);
    int (__stdcall* memory_io)(void*);
    int (__stdcall* set_io)(void*, void*);
    int (__stdcall* open)(void*, const char*, int);
    int (__stdcall* start)(void*);
    int (__stdcall* decode)(void*, void*);
    int (__stdcall* seek)(void*, int);
    int (__stdcall* destroy)(void*);
};

Entries Ours() { return {Create, MemoryIo, SetIo, Open, Start, Decode, Seek, Destroy}; }
Entries Capcoms() {
    const sound::Callees& o = sound::kOriginals;
    return {o.mp3_create, o.mp3_memory_io, o.mp3_set_io, o.mp3_open, o.mp3_start, o.mp3_decode, o.mp3_seek, o.mp3_destroy};
}

struct Run {
    bool opened;
    std::uint32_t frames;         // frames before the end of the stream
    std::uint32_t frame_bytes;    // every frame's byte count, 0 if they differ
    bool uniform;                 // all frames the same size
    std::uint32_t eos_bytes;      // the frame's byte count after the end was reported
    std::uint32_t eos_written;    // bytes of `out` the end-of-stream call changed
    char eos_kind[12];            // what it wrote: "nothing", "zeros", "lastframe", "other"
    std::uint32_t replay_frames;  // frames decoded again after Seek(0)
    bool replay_identical;        // those frames byte for byte the first pass's
    std::vector<std::int16_t> pcm;
};

std::uint32_t FrameBytes(void* decoder) {
    std::uint32_t frame, bytes;
    std::memcpy(&frame, static_cast<const unsigned char*>(decoder) + 0x14, 4);
    std::memcpy(&bytes, reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(frame)) + 8, 4);
    return bytes;
}

// As Music_OpenDecoder opens, Music_Decode decodes and rewinds, Music_Release
// destroys. `replay` frames are decoded again after Seek(0) and compared.
void DecodeAll(const Entries& e, const std::uint8_t* data, std::uint32_t size, std::uint32_t replay, Run* run) {
    *run = Run();
    void* decoder = nullptr;
    if (e.create(&decoder) != 0) return;
    unsigned char io[0x2C];
    e.memory_io(io);
    e.set_io(decoder, io);
    char name[0x20];
    std::snprintf(name, sizeof name, "%X@%X", size, static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(data)));
    if (e.open(decoder, name, -1) != 0) return;
    if (e.start(decoder) != 0) return;
    run->opened = true;
    run->uniform = true;
    static unsigned char out[0x4000];
    static unsigned char last[0x4000];
    for (;;) {
        std::memset(out, 0xA5, sizeof out);  // a sentinel, so what the end-of-stream call writes is visible
        const int result = e.decode(decoder, out);
        const std::uint32_t bytes = FrameBytes(decoder);
        if (result == sound::kEndOfStream) {
            run->eos_bytes = bytes;
            std::uint32_t written = 0, zeros = 0, same = 0;
            for (std::uint32_t i = 0; i < sizeof out; ++i) {
                if (out[i] != 0xA5) ++written;
                if (out[i] == 0) ++zeros;
                if (out[i] == last[i]) ++same;
            }
            run->eos_written = written;
            std::strcpy(run->eos_kind, written == 0 ? "nothing" : zeros >= run->frame_bytes && written <= run->frame_bytes ? "zeros"
                                       : same >= run->frame_bytes ? "lastframe" : "other");
            break;
        }
        std::memcpy(last, out, sizeof out);
        if (result != 0) bof3::Fatal("DIV-0091 compare: Mp3_Decode returned %d at frame %u", result, run->frames);
        if (bytes > sizeof out || (bytes & 3)) bof3::Fatal("DIV-0091 compare: a frame of %u bytes", bytes);
        if (run->frames == 0) run->frame_bytes = bytes;
        else if (bytes != run->frame_bytes) run->uniform = false;
        const std::size_t at = run->pcm.size();
        run->pcm.resize(at + bytes / 2);
        std::memcpy(run->pcm.data() + at, out, bytes);
        ++run->frames;
    }
    run->replay_identical = true;
    if (e.seek(decoder, 0) != 0) bof3::Fatal("DIV-0091 compare: Mp3_Seek(0) failed");
    std::size_t at = 0;
    for (std::uint32_t i = 0; i < replay && i < run->frames; ++i) {
        std::memset(out, 0, sizeof out);
        const int result = e.decode(decoder, out);
        const std::uint32_t bytes = FrameBytes(decoder);
        if (result != 0 || bytes != run->frame_bytes) {
            run->replay_identical = false;
            break;
        }
        if (std::memcmp(run->pcm.data() + at, out, bytes) != 0) run->replay_identical = false;
        at += bytes / 2;
        ++run->replay_frames;
    }
    e.destroy(decoder);
}

struct Diff {
    std::uint32_t samples;      // compared (the shorter of the two)
    std::uint32_t differing;
    std::uint32_t max_abs;
    std::uint32_t first_diff;   // sample index of the first difference, or ~0u
    double rms;
    std::uint32_t frames_differing, worst_frame, worst_frame_max;  // by 1,152-sample frames
};

Diff Compare(const Run& a, const Run& b) {
    Diff d = {};
    d.first_diff = ~0u;
    const std::size_t n = a.pcm.size() < b.pcm.size() ? a.pcm.size() : b.pcm.size();
    double sq = 0;
    for (std::size_t i = 0; i < n; ++i) {
        const int delta = static_cast<int>(a.pcm[i]) - static_cast<int>(b.pcm[i]);
        if (delta == 0) continue;
        const std::uint32_t abs = static_cast<std::uint32_t>(delta < 0 ? -delta : delta);
        ++d.differing;
        if (abs > d.max_abs) d.max_abs = abs;
        if (d.first_diff == ~0u) d.first_diff = static_cast<std::uint32_t>(i);
        sq += static_cast<double>(delta) * delta;
    }
    d.samples = static_cast<std::uint32_t>(n);
    d.rms = n ? std::sqrt(sq / static_cast<double>(n)) : 0.0;
    for (std::size_t f = 0; f * 2304 < n; ++f) {
        std::uint32_t worst = 0;
        for (std::size_t i = f * 2304; i < (f + 1) * 2304 && i < n; ++i) {
            const int delta = static_cast<int>(a.pcm[i]) - static_cast<int>(b.pcm[i]);
            const std::uint32_t abs = static_cast<std::uint32_t>(delta < 0 ? -delta : delta);
            if (abs > worst) worst = abs;
        }
        if (worst == 0) continue;
        ++d.frames_differing;
        if (worst > d.worst_frame_max) {
            d.worst_frame_max = worst;
            d.worst_frame = static_cast<std::uint32_t>(f);
        }
    }
    return d;
}

bool ReadWhole(const char* path, std::vector<std::uint8_t>* out) {
    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    const DWORD size = GetFileSize(h, nullptr);
    out->resize(size);
    DWORD got = 0;
    const bool ok = size == 0 || (ReadFile(h, out->data(), size, &got, nullptr) && got == size);
    CloseHandle(h);
    return ok;
}

bool LooksMp3(const std::uint8_t* b, std::size_t n) { return n >= 4 && b[0] == 0xFF && (b[1] & 0xE0) == 0xE0; }

// An independent count of the file's frames: the MPEG-1 layer III headers
// walked by their own lengths (144 * bit rate / 44,100 + padding), nothing
// decoded. The game's files are bare streams of such frames (bgm-comparison.md
// 6.1); a byte the walk cannot read as a header ends it.
std::uint32_t WalkFrames(const std::uint8_t* b, std::size_t n) {
    static const unsigned kBitrate[16] = {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0};
    static const unsigned kRate[4] = {44100, 48000, 32000, 0};
    std::uint32_t frames = 0;
    std::size_t pos = 0;
    while (pos + 4 <= n) {
        const std::uint8_t* h = b + pos;
        if (h[0] != 0xFF || (h[1] & 0xFE) != 0xFA) break;  // MPEG-1, layer III
        const unsigned bitrate = kBitrate[h[2] >> 4], rate = kRate[(h[2] >> 2) & 3], padding = (h[2] >> 1) & 1;
        if (bitrate == 0 || rate == 0) break;
        const std::size_t length = 144000u * bitrate / rate + padding;
        if (pos + length > n) break;
        pos += length;
        ++frames;
    }
    return frames;
}

// BOF3X_MP3_DUMP=<dir> with BOF3X_MP3_DUMP_FILES=<name,name,...>: the two
// decoders' PCM of the named files written there, raw 16-bit stereo, for a
// third decoder's verdict (docs/mp3-decoder.md 2).
void DumpPcm(const char* file_name, const Run& capcom, const Run& ours) {
    char dir[MAX_PATH], names[1024];
    if (GetEnvironmentVariableA("BOF3X_MP3_DUMP", dir, sizeof dir) == 0) return;
    const DWORD n = GetEnvironmentVariableA("BOF3X_MP3_DUMP_FILES", names, sizeof names);
    if (n == 0 || n >= sizeof names) return;
    bool listed = false;
    const std::size_t len = std::strlen(file_name);
    for (const char* p = names; *p;) {
        const char* end = p;
        while (*end && *end != ',') ++end;
        if (static_cast<std::size_t>(end - p) == len && _strnicmp(p, file_name, len) == 0) listed = true;
        p = *end ? end + 1 : end;
    }
    if (!listed) return;
    const Run* runs[2] = {&capcom, &ours};
    const char* tags[2] = {"capcom", "ours"};
    for (int i = 0; i < 2; ++i) {
        char path[MAX_PATH];
        std::snprintf(path, sizeof path, "%s\\%s.%s.pcm", dir, file_name, tags[i]);
        std::FILE* f = std::fopen(path, "wb");
        if (!f) bof3::Fatal("DIV-0091: cannot write %s", path);
        std::fwrite(runs[i]->pcm.data(), 2, runs[i]->pcm.size(), f);
        std::fclose(f);
    }
}

// One file both ways; the TSV row when `tsv` is given. True when the two
// agree in shape (opened, frames, frame bytes, end-of-stream store, replay).
bool CompareFile(const char* path, std::FILE* tsv, Diff* diff, Run* capcom, Run* ours) {
    std::vector<std::uint8_t> data;
    if (!ReadWhole(path, &data)) bof3::Fatal("DIV-0091 compare: cannot read %s", path);
    DecodeAll(Capcoms(), data.data(), static_cast<std::uint32_t>(data.size()), 16, capcom);
    DecodeAll(Ours(), data.data(), static_cast<std::uint32_t>(data.size()), 16, ours);
    *diff = Compare(*capcom, *ours);
    const std::uint32_t walked = WalkFrames(data.data(), data.size());
    const bool shape = capcom->opened == ours->opened && capcom->frames == ours->frames &&
                       capcom->frame_bytes == ours->frame_bytes && capcom->uniform == ours->uniform &&
                       capcom->eos_bytes == ours->eos_bytes && capcom->replay_identical && ours->replay_identical &&
                       capcom->frames == walked;
    if (tsv)
        std::fprintf(tsv, "%s\t%u\t%u\t%u\t%u\t%u\t%u\t%s\t%u\t%s\t%d\t%d\t%u\t%u\t%u\t%.4f\t%u\t%u\t%u\t%s\n",
                     path, static_cast<unsigned>(data.size()), walked, capcom->frames, ours->frames, capcom->frame_bytes,
                     capcom->eos_bytes, capcom->eos_kind, ours->eos_bytes, ours->eos_kind, capcom->replay_identical ? 1 : 0,
                     ours->replay_identical ? 1 : 0, diff->samples, diff->differing, diff->max_abs, diff->rms,
                     diff->frames_differing, diff->worst_frame, diff->worst_frame_max, shape ? "same" : "DIFFERENT");
    const char* base = std::strrchr(path, '\\');
    DumpPcm(base ? base + 1 : path, *capcom, *ours);
    return shape;
}

void CompareCorpus(const char* tsv_path) {
    std::FILE* tsv = std::fopen(tsv_path, "w");
    if (!tsv) bof3::Fatal("DIV-0091: cannot write BOF3X_MP3_COMPARE=%s", tsv_path);
    std::fprintf(tsv, "file\tbytes\tframes_walked\tframes_capcom\tframes_ours\tframe_bytes\teos_bytes_capcom\t"
                      "eos_kind_capcom\teos_bytes_ours\teos_kind_ours\treplay_capcom\treplay_ours\tsamples\tdiffering\t"
                      "max_abs\trms\tframes_differing\tworst_frame\tworst_frame_max\tshape\n");
    const char* dirs[2] = {"BGM", "SND"};
    unsigned files = 0, same = 0;
    std::uint32_t max_abs = 0;
    double worst_rms = 0;
    const DWORD t0 = GetTickCount();
    for (const char* dir : dirs) {
        char pattern[MAX_PATH];
        std::snprintf(pattern, sizeof pattern, "%s\\*", dir);
        WIN32_FIND_DATAA f;
        HANDLE h = FindFirstFileA(pattern, &f);
        if (h == INVALID_HANDLE_VALUE) continue;
        do {
            if (f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            char path[MAX_PATH];
            std::snprintf(path, sizeof path, "%s\\%s", dir, f.cFileName);
            std::uint8_t first[4] = {0, 0, 0, 0};
            DWORD got = 0;
            HANDLE fh = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
            if (fh == INVALID_HANDLE_VALUE) continue;
            ReadFile(fh, first, 4, &got, nullptr);
            CloseHandle(fh);
            if (!LooksMp3(first, got)) continue;
            ++files;
            Diff d;
            Run a, b;
            if (CompareFile(path, tsv, &d, &a, &b)) ++same;
            if (d.max_abs > max_abs) max_abs = d.max_abs;
            if (d.rms > worst_rms) worst_rms = d.rms;
        } while (FindNextFileA(h, &f));
        FindClose(h);
    }
    std::fclose(tsv);
    bof3::Log("DIV-0091    compare: %u MP3 files of BGM\\ and SND\\ through both decoders in %lu ms, %u the same shape; "
              "max |delta| %u, worst RMS %.4f (16-bit units); %s",
              files, static_cast<unsigned long>(GetTickCount() - t0), same, max_abs, worst_rms, tsv_path);
}

}  // namespace

void Arm() {
    char mode[16];
    const DWORD n = GetEnvironmentVariableA("BOF3X_MP3", mode, sizeof mode);
    if (n >= sizeof mode || (n != 0 && std::strcmp(mode, "minimp3") != 0 && std::strcmp(mode, "original") != 0))
        bof3::Fatal("DIV-0091: BOF3X_MP3 must be minimp3 or original");
    if (n != 0 && std::strcmp(mode, "original") == 0) {
        bof3::Log("DIV-0091    OFF (BOF3X_MP3=original): the MP3s decode through Capcom's decoder");
        return;
    }
    sound::g.mp3_create = Create;
    sound::g.mp3_memory_io = MemoryIo;
    sound::g.mp3_set_io = SetIo;
    sound::g.mp3_open = Open;
    sound::g.mp3_start = Start;
    sound::g.mp3_decode = Decode;
    sound::g.mp3_seek = Seek;
    sound::g.mp3_destroy = Destroy;
    g_active = true;
    bof3::Log("DIV-0091    the MP3s decode through minimp3 (BOF3X_MP3=original for Capcom's decoder)");
}

// Capcom's decoder allocates from the runtime's heap, which exists only once
// the executable's entry point has run - after every start-up self-test. So
// the comparison against it runs from WinMain, the first code of ours with
// the runtime under it, and ends the process there.
void CompareAtStart() {
    char tsv[MAX_PATH];
    const DWORD t = GetEnvironmentVariableA("BOF3X_MP3_COMPARE", tsv, sizeof tsv);
    if (t >= sizeof tsv) bof3::Fatal("DIV-0091: BOF3X_MP3_COMPARE is %lu characters", static_cast<unsigned long>(t));
    if (t == 0) return;
    CompareCorpus(tsv);
    bof3::Log("DIV-0091    compare only: done, ending the process");
    bof3::LogClose();
    TerminateProcess(GetCurrentProcess(), 0);
}

// --- BOF3X_SHADOW=sound -------------------------------------------------------

// Ours alone (Capcom's cannot run here, see CompareAtStart): BGM\001.DAT's
// frames counted by the header walk and by decoding, every frame 1,152
// stereo samples, the store at the end of the stream, the replay after
// Seek(0) identical over the whole file, and the name's round trip.
void SelfTest() {
    std::uint32_t size = 0, address = 0;
    char name[0x20];
    std::snprintf(name, sizeof name, "%X@%X", 0x1A2B3Cu, 0xDEADBEEFu);
    if (!ParseName(name, &size, &address) || size != 0x1A2B3C || address != 0xDEADBEEF || ParseName("12@", &size, &address) ||
        ParseName("@12", &size, &address) || ParseName("12@34x", &size, &address) || ParseName("123456789@1", &size, &address))
        bof3::Fatal("DIV-0091 self-test: the size@address name does not round-trip");
    const char* const path = "BGM\\001.DAT";
    std::vector<std::uint8_t> data;
    if (!ReadWhole(path, &data)) bof3::Fatal("DIV-0091 self-test: cannot read %s", path);
    const std::uint32_t walked = WalkFrames(data.data(), data.size());
    Run ours;
    DecodeAll(Ours(), data.data(), static_cast<std::uint32_t>(data.size()), ~0u, &ours);
    if (!ours.opened) bof3::Fatal("DIV-0091 self-test: %s did not open", path);
    if (ours.frames != walked || ours.frame_bytes != kFrameBytes || !ours.uniform || ours.eos_bytes != kEosBytes ||
        !ours.replay_identical || ours.replay_frames != ours.frames)
        bof3::Fatal("DIV-0091 self-test: %s - %u frames decoded against %u walked, %u bytes a frame%s, %u stored at the "
                    "end, the replay after Seek(0) %s over %u frames",
                    path, ours.frames, walked, ours.frame_bytes, ours.uniform ? "" : " (uneven)", ours.eos_bytes,
                    ours.replay_identical ? "identical" : "DIFFERENT", ours.replay_frames);
    bof3::Log("shadow      DIV-0091 self-test: %s - %u frames of %u bytes, as many as the header walk counts; %u stored "
              "at the end of the stream; the replay after Seek(0) identical over all %u frames; the size@address name "
              "round-trips",
              path, ours.frames, ours.frame_bytes, ours.eos_bytes, ours.replay_frames);
}

}  // namespace mp3dec

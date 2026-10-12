// The launcher's music player (docs/music-player.md). A Win32 dialog from
// launcher.rc over the settings dialog; the sound goes out through waveOut
// (winmm), since the launcher's SDL is built without its audio subsystem and
// that build is the DLL's too.
//
// Three sources, each a radio button greyed when the install lacks it (the
// Config screen's rule): the PlayStation disc's songs as tools/seq.py wrote
// them into the cache (base\bgm\NNN.DAT and its bank), played by the engine's
// own sequencer (src/audio/seq.h, DIV-0087's player); the PSP disc's, when the
// cache was built from a PSP disc alone (tools/ppms.py, docs/psp-music.md: the
// same form, the same player; the manifest's bgm rows name the build); and the PC
// port's MP3s in BGM\, decoded by minimp3 and looped at DIV-0081's measured
// points where the table has a row, else rewound to the file's start as the
// port does. A song number is the game's: song N is the PC's file N and the
// PSX song table's entry N (docs/bgm-comparison.md 6.2, 6.3), so a list holds
// each song once - the 38 battle, boss and demo bundles are copies of seven
// BGM files and never appear (docs/seq-import.md).
//
// Songs loop until stopped (the owner's call, 2026-10-11); a song that plays
// once in the game plays once here.
#include "launcher/music_player.h"

#include <windows.h>

#include <mmsystem.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <memory>
#include <string>
#include <vector>

#include "audio/seq.h"
#include "audio/song.h"
#include "game/music_loops.h"
#include "launcher/resource.h"

// third_party/minimp3/minimp3.h, verbatim (docs/THIRD_PARTY.md section 3): the
// launcher's own copy of the implementation, as src/audio/mp3.cpp is the DLL's.
#define MINIMP3_IMPLEMENTATION
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsign-compare"
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wimplicit-fallthrough"
#include "minimp3/minimp3.h"
#pragma clang diagnostic pop

namespace bof3x {
namespace {

constexpr wchar_t kTitle[] = L"bof3x-launcher - music";

// ---- a malformed file ends the launcher, naming it (CLAUDE.md rule 4) -------

wchar_t g_reading[MAX_PATH];  // the file being parsed, for the message

[[noreturn]] void Fatal(const std::wstring& message) {
    MessageBoxW(nullptr, message.c_str(), kTitle, MB_OK | MB_ICONERROR);
    std::fwprintf(stderr, L"bof3x-launcher: music: %ls\n", message.c_str());
    ExitProcess(1);
}

void MusicAbort(const char* message) {
    wchar_t text[1024];
    std::swprintf(text, 1024, L"%hs\n\nwhile reading %ls", message, g_reading[0] ? g_reading : L"(no file)");
    MessageBoxW(nullptr, text, kTitle, MB_OK | MB_ICONERROR);
    std::fwprintf(stderr, L"bof3x-launcher: music: %ls\n", text);
}

std::vector<std::uint8_t> ReadWhole(const std::wstring& path) {
    const HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                 FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) Fatal(path + L" does not open");
    LARGE_INTEGER size;
    constexpr LONGLONG kFileMax = 64ll << 20;  // far above any song, bank or MP3
    if (!GetFileSizeEx(h, &size) || size.QuadPart <= 0 || size.QuadPart > kFileMax) {
        CloseHandle(h);
        Fatal(path + L" is empty or too large to be a song");
    }
    std::vector<std::uint8_t> out(static_cast<std::size_t>(size.QuadPart));
    DWORD got = 0;
    const BOOL ok = ReadFile(h, out.data(), static_cast<DWORD>(out.size()), &got, nullptr);
    CloseHandle(h);
    if (!ok || got != out.size()) Fatal(path + L" could not be read whole");
    return out;
}

// ---- what plays: 44.1 kHz 16-bit stereo, the rate of both sources ----------

class Stream {
public:
    virtual ~Stream() = default;
    virtual void Render(std::int16_t* stereo, int frames) = 0;
    // Nothing more will sound: a once-only song past its end.
    virtual bool Ended() const = 0;
};

// A PC MP3, decoded whole. Sample positions count from the file's first
// decoded frame, as music_loops' rows and the game's LoopDecode count them.
class Mp3Stream : public Stream {
public:
    Mp3Stream(const std::wstring& path, unsigned track, bool loops) : loops_(loops) {
        const std::vector<std::uint8_t> file = ReadWhole(path);
        mp3dec_t dec;
        mp3dec_init(&dec);
        std::vector<mp3d_sample_t> frame(MINIMP3_MAX_SAMPLES_PER_FRAME);
        std::size_t at = 0;
        while (at < file.size()) {
            mp3dec_frame_info_t info;
            const int samples = mp3dec_decode_frame(&dec, file.data() + at, static_cast<int>(file.size() - at),
                                                    frame.data(), &info);
            if (info.frame_bytes <= 0) break;
            at += static_cast<std::size_t>(info.frame_bytes);
            if (samples == 0) continue;
            if (info.channels != 2 || info.hz != 44100)
                Fatal(path + L" has a frame that is not 44,100 Hz stereo");
            pcm_.insert(pcm_.end(), frame.begin(), frame.begin() + 2 * samples);
        }
        frames_ = static_cast<std::uint32_t>(pcm_.size() / 2);
        if (frames_ == 0) Fatal(path + L" decodes to no samples");
        if (loops_) {
            for (const music_loops::Row& r : kRows) {
                if (r.track != track || r.file_bytes != file.size()) continue;
                if (r.start >= r.end || r.end + r.fade > frames_ || r.start + r.fade > frames_)
                    Fatal(path + L": its loop row runs past the decoded stream");
                row_ = &r;
            }
        }
    }

    void Render(std::int16_t* out, int frames) override {
        for (int i = 0; i < frames; ++i, out += 2) {
            if (ended_) {
                out[0] = out[1] = 0;
                continue;
            }
            const std::uint32_t stop = row_ ? row_->end : frames_;
            if (pos_ >= stop) {
                if (!loops_) {
                    ended_ = true;
                    out[0] = out[1] = 0;
                    continue;
                }
                // DIV-0081's measured loop, or the port's rewind to the file's start
                pos_ = row_ ? row_->start : 0;
                fade_ = 0;
                fading_ = row_ && row_->fade > 0;
            }
            const std::int16_t* s = &pcm_[2 * static_cast<std::size_t>(pos_)];
            if (fading_) {
                // the game's crossfade (sound.cpp LoopDecode): the samples after
                // the end, faded out over the start's
                const std::int16_t* a = &pcm_[2 * static_cast<std::size_t>(row_->end + fade_)];
                const std::int32_t w = static_cast<std::int32_t>(2 * fade_ + 1);
                const std::int32_t f2 = static_cast<std::int32_t>(2 * row_->fade);
                for (int c = 0; c < 2; ++c) out[c] = static_cast<std::int16_t>((a[c] * (f2 - w) + s[c] * w) / f2);
                if (++fade_ == row_->fade) fading_ = false;
            } else {
                out[0] = s[0];
                out[1] = s[1];
            }
            ++pos_;
        }
    }

    bool Ended() const override { return ended_; }
    bool Measured() const { return row_ != nullptr; }

private:
    static constexpr music_loops::Row kRows[] = {
#include "game/music_loops_table.inc"
    };

    std::vector<std::int16_t> pcm_;
    std::uint32_t frames_ = 0;
    std::uint32_t pos_ = 0;
    const music_loops::Row* row_ = nullptr;
    bool loops_;
    bool ended_ = false;
    bool fading_ = false;
    std::uint32_t fade_ = 0;
};

// A disc song from the cache through the engine's sequencer, started as the
// game's music_seq.cpp Begin starts one: Play at volume 1 over one frame,
// then SsSepSetVol(127, 127). The loop is in the song's data.
class SeqStream : public Stream {
public:
    SeqStream(const std::wstring& song_path, const std::wstring& bank_dir) : synth_(new psx::MusicSynth) {
        const std::vector<std::uint8_t> song_bytes = ReadWhole(song_path);
        lstrcpynW(g_reading, song_path.c_str(), MAX_PATH);
        psx::LoadSong(song_bytes.data(), song_bytes.size(), &song_);
        const std::wstring bank_path = bank_dir + std::wstring(song_.bank.begin(), song_.bank.end()) + L".DAT";
        const std::vector<std::uint8_t> bank_bytes = ReadWhole(bank_path);
        lstrcpynW(g_reading, bank_path.c_str(), MAX_PATH);
        psx::LoadBank(bank_bytes.data(), bank_bytes.size(), &bank_);
        psx::MusicSynth::CheckBank(bank_);
        g_reading[0] = 0;
        synth_->LoadBank(bank_);
        synth_->Play(song_, 1, 1);
        synth_->SetVolume(127, 127);
    }

    void Render(std::int16_t* out, int frames) override { synth_->Render(out, frames); }
    bool Ended() const override { return synth_->Ended(); }

private:
    std::unique_ptr<psx::MusicSynth> synth_;
    psx::Song song_;
    psx::Bank bank_;
};

// ---- waveOut: four buffers refilled by a thread of ours --------------------

class Output {
public:
    bool Open() {
        event_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        WAVEFORMATEX fmt{};
        fmt.wFormatTag = WAVE_FORMAT_PCM;
        fmt.nChannels = 2;
        fmt.nSamplesPerSec = 44100;
        fmt.wBitsPerSample = 16;
        fmt.nBlockAlign = 4;
        fmt.nAvgBytesPerSec = 44100 * 4;
        if (waveOutOpen(&out_, WAVE_MAPPER, &fmt, reinterpret_cast<DWORD_PTR>(event_), 0, CALLBACK_EVENT) !=
            MMSYSERR_NOERROR) {
            out_ = nullptr;
            return false;
        }
        InitializeCriticalSection(&lock_);
        for (int i = 0; i < kBuffers; ++i) {
            buffers_[i].assign(2 * kFrames, 0);
            headers_[i] = WAVEHDR{};
            headers_[i].lpData = reinterpret_cast<LPSTR>(buffers_[i].data());
            headers_[i].dwBufferLength = kFrames * 4;
            waveOutPrepareHeader(out_, &headers_[i], sizeof(WAVEHDR));
            Fill(i);
        }
        thread_ = CreateThread(nullptr, 0, &Output::Main, this, 0, nullptr);
        return true;
    }

    void Close() {
        if (!out_) return;
        quit_ = 1;
        SetEvent(event_);
        WaitForSingleObject(thread_, INFINITE);
        CloseHandle(thread_);
        waveOutReset(out_);
        for (WAVEHDR& h : headers_) waveOutUnprepareHeader(out_, &h, sizeof(WAVEHDR));
        waveOutClose(out_);
        out_ = nullptr;
        stream_.reset();
        DeleteCriticalSection(&lock_);
        CloseHandle(event_);
    }

    // The stream from the next buffer on; nullptr for silence.
    void Set(std::unique_ptr<Stream> s) {
        EnterCriticalSection(&lock_);
        stream_.swap(s);
        ended_ = 0;
        LeaveCriticalSection(&lock_);
        // the old stream is destroyed here, outside the lock
    }

    bool Ended() const { return ended_ != 0; }

private:
    static constexpr int kBuffers = 4;
    static constexpr int kFrames = 2048;  // 46 ms a buffer

    static DWORD WINAPI Main(void* self) {
        auto* o = static_cast<Output*>(self);
        while (!o->quit_) {
            WaitForSingleObject(o->event_, 100);
            for (int i = 0; i < kBuffers && !o->quit_; ++i)
                if (o->headers_[i].dwFlags & WHDR_DONE) o->Fill(i);
        }
        return 0;
    }

    void Fill(int i) {
        EnterCriticalSection(&lock_);
        if (stream_) {
            stream_->Render(buffers_[i].data(), kFrames);
            if (stream_->Ended()) ended_ = 1;
        } else {
            std::memset(buffers_[i].data(), 0, kFrames * 4);
        }
        LeaveCriticalSection(&lock_);
        headers_[i].dwFlags &= ~static_cast<DWORD>(WHDR_DONE);
        waveOutWrite(out_, &headers_[i], sizeof(WAVEHDR));
    }

    HWAVEOUT out_ = nullptr;
    HANDLE event_ = nullptr;
    HANDLE thread_ = nullptr;
    CRITICAL_SECTION lock_;
    WAVEHDR headers_[kBuffers];
    std::vector<std::int16_t> buffers_[kBuffers];
    std::unique_ptr<Stream> stream_;
    volatile LONG quit_ = 0;
    volatile LONG ended_ = 0;
};

// ---- the sources and their song lists --------------------------------------

enum Source { kPsx, kPsp, kPc, kSources };

struct SourceInfo {
    int radio;
    const wchar_t* label;
};
const SourceInfo kSourceInfo[kSources] = {
    {IDC_MP_PSX, L"PlayStation disc"},
    {IDC_MP_PSP, L"PSP disc"},
    {IDC_MP_PC, L"PC port"},
};

struct Entry {
    unsigned number;
    std::wstring path;   // the song file
    std::wstring where;  // the disc's own name for it
    bool loops;
};

struct State {
    std::wstring game_dir, cache;
    std::vector<Entry> songs[kSources];
    std::wstring why[kSources];  // why a source is greyed
    std::vector<std::wstring> names;  // music-names.tsv: index = song number
    std::vector<bool> hidden;         // music-names.tsv rows marked same-notes
    int source = -1;
    Output out;
    bool open = false;
    bool playing = false;
};

// NNN.DAT, NNNN.DAT (the PC's once-only files): the song number, or -1.
int SongNumber(const wchar_t* name, bool* once) {
    if (!(name[0] >= L'0' && name[0] <= L'9' && name[1] >= L'0' && name[1] <= L'9' && name[2] >= L'0' &&
          name[2] <= L'9'))
        return -1;
    const wchar_t* rest = name + 3;
    *once = *rest == L'N' || *rest == L'n';
    if (*once) ++rest;
    if (lstrcmpiW(rest, L".DAT") != 0) return -1;
    return (name[0] - L'0') * 100 + (name[1] - L'0') * 10 + (name[2] - L'0');
}

template <typename F>
void EachFile(const std::wstring& dir, F f) {
    WIN32_FIND_DATAW found;
    const HANDLE find = FindFirstFileW((dir + L"*").c_str(), &found);
    if (find == INVALID_HANDLE_VALUE) return;
    do {
        if (!(found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) f(found.cFileName);
    } while (FindNextFileW(find, &found));
    FindClose(find);
}

void Sort(std::vector<Entry>& v) {
    for (std::size_t i = 1; i < v.size(); ++i)
        for (std::size_t j = i; j > 0 && v[j].number < v[j - 1].number; --j) std::swap(v[j], v[j - 1]);
}

std::wstring CacheMusicRoot(const std::wstring& cache) {
    // the game reads BOF3X_CACHE before the ini's cache= (config.cpp)
    wchar_t env[MAX_PATH];
    const DWORD n = GetEnvironmentVariableW(L"BOF3X_CACHE", env, MAX_PATH);
    if (n > 0 && n < MAX_PATH) return env;
    return cache;
}

// The build the cache's base\bgm came from: the build id that opens the source
// of the manifest's first `bgm` row (`["base/bgm/000.DAT", "psp-jp:ppms:...", ...]`,
// tools/importer.py write_manifest), or "" when the manifest has no such row.
// A PSP build's songs are tools/ppms.py's (docs/psp-music.md); any other's are
// a PlayStation disc's (tools/seq.py). A cache holds one base\bgm.
std::string CacheBgmBuild(const std::wstring& root) {
    const std::wstring path = root + L"\\manifest.toml";
    if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) return "";
    const std::vector<std::uint8_t> bytes = ReadWhole(path);
    const std::string text(bytes.begin(), bytes.end());
    std::size_t at = text.find("\nbgm = [");
    if (at == std::string::npos) return "";
    at = text.find('\n', at + 1);
    if (at == std::string::npos) return "";
    // the row: its first string is the file, its second the source
    const std::size_t open1 = text.find('"', at);
    const std::size_t close1 = open1 == std::string::npos ? open1 : text.find('"', open1 + 1);
    const std::size_t q = close1 == std::string::npos ? close1 : text.find('"', close1 + 1);
    if (q == std::string::npos) return "";
    const std::size_t colon = text.find(':', q + 1), close = text.find('"', q + 1);
    if (colon == std::string::npos || close == std::string::npos || colon > close) return "";
    return text.substr(q + 1, colon - q - 1);
}

bool IsPspBuild(const std::string& build) { return build.compare(0, 4, "psp-") == 0; }

// The cache's base\bgm\NNN.DAT songs into `source`'s list.
void ListCacheSongs(State& s, const std::wstring& root, int source) {
    const std::wstring dir = root + L"\\base\\bgm\\";
    EachFile(dir, [&](const wchar_t* name) {
        bool once = false;
        const int n = SongNumber(name, &once);
        if (n < 0 || once) return;
        Entry e;
        e.number = static_cast<unsigned>(n);
        e.path = dir + name;
        const std::vector<std::uint8_t> bytes = ReadWhole(e.path);
        lstrcpynW(g_reading, e.path.c_str(), MAX_PATH);
        psx::Song song;
        psx::LoadSong(bytes.data(), bytes.size(), &song);
        g_reading[0] = 0;
        if (song.number != e.number) Fatal(e.path + L" holds another song's number");
        e.loops = (song.flags & 1) != 0;
        wchar_t where[64];
        std::swprintf(where, 64, L"%hs.EMI sub %u", song.bank.c_str(), song.sub);
        e.where = where;
        s.songs[source].push_back(e);
    });
}

void ListPsx(State& s) {
    const std::wstring root = CacheMusicRoot(s.cache);
    if (root.empty()) {
        s.why[kPsx] = L"No cache folder is set: the importer writes the disc's songs into one (docs/seq-import.md).";
        return;
    }
    if (IsPspBuild(CacheBgmBuild(root))) {
        s.why[kPsx] = L"The cache's songs are the PSP disc's: build the cache with a PlayStation disc for these.";
        return;
    }
    ListCacheSongs(s, root, kPsx);
    if (s.songs[kPsx].empty())
        s.why[kPsx] = L"The cache folder holds no songs (base\\bgm\\): run tools/seq.py with a PlayStation disc.";
}

void ListPsp(State& s) {
    // docs/psp-music.md: a cache built from a PSP disc and no PlayStation disc
    // holds the PSP's songs (tools/ppms.py, pPMS inverted) and banks
    // (vag.psp_music_vab, the volume / pan merge inverted) in base\bgm, in the
    // PlayStation's form, so the same sequencer plays them. A cache holds one
    // base\bgm: with a PlayStation disc among the sources it is the disc's.
    const std::wstring root = CacheMusicRoot(s.cache);
    if (root.empty()) {
        s.why[kPsp] = L"No cache folder is set: the importer writes a PSP disc's songs into one (docs/psp-music.md).";
        return;
    }
    if (!IsPspBuild(CacheBgmBuild(root))) {
        s.why[kPsp] = L"The cache's songs are not the PSP disc's: a cache built from a PSP disc alone holds them "
                      L"(docs/psp-music.md).";
        return;
    }
    ListCacheSongs(s, root, kPsp);
    if (s.songs[kPsp].empty()) s.why[kPsp] = L"The cache's manifest names the PSP's songs, but base\\bgm\\ holds none.";
}

void ListPc(State& s) {
    const std::wstring dir = s.game_dir + L"\\BGM\\";
    EachFile(dir, [&](const wchar_t* name) {
        bool once = false;
        const int n = SongNumber(name, &once);
        if (n < 0) return;
        Entry e;
        e.number = static_cast<unsigned>(n);
        e.path = dir + name;
        e.where = std::wstring(L"BGM\\") + name;
        e.loops = !once;
        s.songs[kPc].push_back(e);
    });
    if (s.songs[kPc].empty()) s.why[kPc] = L"The game folder has no BGM\\ songs.";
}

// <launcher folder>\music-names.tsv, copied there from data/music-names.tsv
// by the build: the catalogue of names (docs/music-player.md section 4).
// UTF-8, tab-separated, # comments; the columns are
//   song  name  box_disc  box_track  box_title  source  note
// and a song shows its box title where it has one, else its name; a song
// whose source is same-notes is not listed (the owner's mark, 2026-10-12, for
// one too like another). A line whose first field is not a three-digit song
// number is skipped.
std::wstring Wide(const std::string& u8) {
    const int w = MultiByteToWideChar(CP_UTF8, 0, u8.c_str(), static_cast<int>(u8.size()), nullptr, 0);
    std::wstring wide(static_cast<std::size_t>(w), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, u8.c_str(), static_cast<int>(u8.size()), wide.data(), w);
    return wide;
}

void LoadNames(State& s) {
    wchar_t self[MAX_PATH];
    const DWORD n = GetModuleFileNameW(nullptr, self, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return;
    std::wstring path(self);
    path = path.substr(0, path.find_last_of(L"\\/") + 1) + L"music-names.tsv";
    const HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    CloseHandle(h);
    const std::vector<std::uint8_t> bytes = ReadWhole(path);
    std::string text(bytes.begin(), bytes.end());
    if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF) text.erase(0, 3);  // a BOM
    std::size_t at = 0;
    while (at < text.size()) {
        std::size_t end = text.find('\n', at);
        if (end == std::string::npos) end = text.size();
        std::string line = text.substr(at, end - at);
        at = end + 1;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::vector<std::string> field;
        for (std::size_t from = 0;;) {
            const std::size_t tab = line.find('\t', from);
            field.push_back(line.substr(from, tab == std::string::npos ? std::string::npos : tab - from));
            if (tab == std::string::npos) break;
            from = tab + 1;
        }
        const std::string& song = field[0];
        if (song.size() != 3 || song.find_first_not_of("0123456789") != std::string::npos) continue;
        const unsigned number = static_cast<unsigned>(std::strtoul(song.c_str(), nullptr, 10));
        // the owner's mark for a song too like another to list (2026-10-12)
        if (field.size() > 5 && field[5] == "same-notes") {
            if (number >= s.hidden.size()) s.hidden.resize(number + 1);
            s.hidden[number] = true;
        }
        std::string name;
        if (field.size() > 4 && !field[4].empty()) name = field[4];  // the box's title
        else if (field.size() > 1) name = field[1];
        const std::size_t b = name.find_first_not_of(' '), e = name.find_last_not_of(' ');
        if (b == std::string::npos) continue;
        name = name.substr(b, e - b + 1);
        if (number >= s.names.size()) s.names.resize(number + 1);
        s.names[number] = Wide(name);
    }
}

// ---- the dialog ------------------------------------------------------------

void Status(HWND dlg, const wchar_t* text) { SetDlgItemTextW(dlg, IDC_MP_STATUS, text); }

void FillSongs(HWND dlg, State& s, unsigned keep) {
    const HWND box = GetDlgItem(dlg, IDC_MP_SONG);
    SendMessageW(box, CB_RESETCONTENT, 0, 0);
    int select = 0;
    const std::vector<Entry>& list = s.songs[s.source];
    for (std::size_t i = 0; i < list.size(); ++i) {
        const Entry& e = list[i];
        const bool named = e.number < s.names.size() && !s.names[e.number].empty();
        wchar_t label[256];
        std::swprintf(label, 256, L"%03u%ls%ls   (%ls%ls)", e.number, named ? L"   " : L"",
                      named ? s.names[e.number].c_str() : L"", e.where.c_str(), e.loops ? L"" : L", plays once");
        SendMessageW(box, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label));
        if (e.number == keep) select = static_cast<int>(i);
    }
    SendMessageW(box, CB_SETCURSEL, static_cast<WPARAM>(select), 0);
}

const Entry* Chosen(HWND dlg, const State& s) {
    const LRESULT i = SendDlgItemMessageW(dlg, IDC_MP_SONG, CB_GETCURSEL, 0, 0);
    if (s.source < 0 || i < 0 || static_cast<std::size_t>(i) >= s.songs[s.source].size()) return nullptr;
    return &s.songs[s.source][static_cast<std::size_t>(i)];
}

void Play(HWND dlg, State& s) {
    const Entry* e = Chosen(dlg, s);
    if (!e || !s.open) return;
    HCURSOR was = SetCursor(LoadCursorA(nullptr, IDC_WAIT));
    std::unique_ptr<Stream> stream;
    std::wstring how;
    if (s.source == kPsx || s.source == kPsp) {
        // both are the cache's base\bgm in the PlayStation's form (docs/psp-music.md)
        const std::wstring root = CacheMusicRoot(s.cache);
        stream.reset(new SeqStream(e->path, root + L"\\base\\bgm\\bank\\"));
        how = e->loops ? L"looping as the disc's song does" : L"once, as in the game";
    } else {
        auto* mp3 = new Mp3Stream(e->path, e->number, e->loops);
        stream.reset(mp3);
        how = !e->loops ? L"once, as in the game"
              : mp3->Measured() ? L"looping at its measured points (DIV-0081)"
                                : L"looping from the file's start, as the port does";
    }
    s.out.Set(std::move(stream));
    SetCursor(was);
    s.playing = true;
    wchar_t text[256];
    std::swprintf(text, 256, L"Playing %03u from the %ls: %ls.", e->number, kSourceInfo[s.source].label, how.c_str());
    Status(dlg, text);
}

void Stop(HWND dlg, State& s) {
    s.out.Set(nullptr);
    s.playing = false;
    Status(dlg, L"Stopped.");
}

void ChooseSource(HWND dlg, State& s, int source) {
    const Entry* was = Chosen(dlg, s);
    const unsigned keep = was ? was->number : 0;
    s.source = source;
    CheckRadioButton(dlg, IDC_MP_PSX, IDC_MP_PC, kSourceInfo[source].radio);
    FillSongs(dlg, s, keep);
    if (s.playing) Play(dlg, s);
}

constexpr UINT_PTR kEndTimer = 2;

INT_PTR CALLBACK Proc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_INITDIALOG: {
        auto* s = reinterpret_cast<State*>(lp);
        SetWindowLongPtrW(dlg, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(s));
        std::wstring notes;
        int first = -1;
        for (int i = 0; i < kSources; ++i) {
            const bool have = !s->songs[i].empty();
            EnableWindow(GetDlgItem(dlg, kSourceInfo[i].radio), have);
            if (have && first < 0) first = i;
            if (!have) notes += std::wstring(kSourceInfo[i].label) + L": " + s->why[i] + L"\r\n";
        }
        SetDlgItemTextW(dlg, IDC_MP_NOTE, notes.c_str());
        s->open = s->out.Open();
        if (first < 0) {
            Status(dlg, L"No source has songs.");
        } else {
            ChooseSource(dlg, *s, first);
            Status(dlg, s->open ? L"Stopped." : L"No sound device could be opened.");
        }
        EnableWindow(GetDlgItem(dlg, IDC_MP_PLAY), first >= 0 && s->open);
        EnableWindow(GetDlgItem(dlg, IDC_MP_STOP), first >= 0 && s->open);
        SetTimer(dlg, kEndTimer, 250, nullptr);
        return TRUE;
    }
    case WM_TIMER:
        if (wp == kEndTimer) {
            auto* s = reinterpret_cast<State*>(GetWindowLongPtrW(dlg, GWLP_USERDATA));
            if (s->playing && s->out.Ended()) {
                s->playing = false;
                Status(dlg, L"Ended (a song that plays once).");
            }
            return TRUE;
        }
        break;
    case WM_COMMAND: {
        auto* s = reinterpret_cast<State*>(GetWindowLongPtrW(dlg, GWLP_USERDATA));
        switch (LOWORD(wp)) {
        case IDC_MP_PSX:
        case IDC_MP_PSP:
        case IDC_MP_PC:
            if (HIWORD(wp) == BN_CLICKED)
                for (int i = 0; i < kSources; ++i)
                    if (kSourceInfo[i].radio == LOWORD(wp) && i != s->source) ChooseSource(dlg, *s, i);
            return TRUE;
        case IDC_MP_SONG:
            if (HIWORD(wp) == CBN_SELCHANGE && s->playing) Play(dlg, *s);
            return TRUE;
        case IDC_MP_PLAY:
            Play(dlg, *s);
            return TRUE;
        case IDC_MP_STOP:
            Stop(dlg, *s);
            return TRUE;
        case IDOK:
        case IDCANCEL:
            KillTimer(dlg, kEndTimer);
            s->out.Close();
            EndDialog(dlg, 0);
            return TRUE;
        default:
            break;
        }
        break;
    }
    default:
        break;
    }
    return FALSE;
}

}  // namespace

void MusicPlayerRun(HWND owner, const std::wstring& game_dir, const std::wstring& cache) {
    psx::SetMusicAbortHook(MusicAbort);
    State s;
    s.game_dir = game_dir;
    s.cache = cache;
    HCURSOR was = SetCursor(LoadCursorA(nullptr, IDC_WAIT));
    ListPsx(s);
    ListPsp(s);
    ListPc(s);
    for (std::vector<Entry>& v : s.songs) Sort(v);
    LoadNames(s);
    for (std::vector<Entry>& v : s.songs) {
        std::size_t kept = 0;
        for (const Entry& e : v)
            if (!(e.number < s.hidden.size() && s.hidden[e.number])) v[kept++] = e;
        v.resize(kept);
    }
    SetCursor(was);
    DialogBoxParamW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDD_MUSIC), owner, Proc, reinterpret_cast<LPARAM>(&s));
}

}  // namespace bof3x

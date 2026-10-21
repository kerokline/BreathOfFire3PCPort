// DIV-0087: the cache's songs through psx::MusicSynth (music_seq.h,
// docs/music-seq-engine.md). Ours entirely: no byte of Capcom's here, and no
// byte of game data - the self-test builds its own song and bank.
#include "game/music_seq.h"

#include <windows.h>

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "audio/seq.h"
#include "audio/song.h"
#include "bof3/symbols.gen.h"
#include "common/cache_manifest.h"
#include "game/dat_cache.h"
#include "game/music_resume.h"
#include "game/sound_callees.h"
#include "hook/log.h"

namespace music_seq {
namespace {

using sound::g;

// The options screen's Sound row (Stereo 0 / Mono 1): ConfigScreen_Rows
// 0x461070 flips it, task 0 at 0x4FCF.. zeroes it (symbols.toml; docs/
// config-screen.md section 1, row 3). The PC's driver never reads it; on the
// PSX the same row calls SsSetMono / SsSetStereo (libsnd-reading.md 8).
// kSoundMono: now Rest4f_Setting3 (symbols.toml).

// The CRT's fopen takes a path of up to MAX_PATH - 1 characters. File_Open
// (0x5A7380, ours in file_io.cpp) retries a failed open with File_CdRoot in
// front in a 0x50-byte buffer; it skips that retry for a path that would not
// fit (DIV-0087), so a cache path may be as long as fopen allows.
constexpr std::size_t kPathMax = MAX_PATH - 1;
// The longest path built: <root>\base\bgm\bank\ + a 15-character bank name +
// .DAT (a song's, <root>\base\bgm\ + ten digits + .DAT, is shorter).
constexpr std::size_t kLongestTail = sizeof "\\base\\bgm\\bank\\" - 1 + 15 + sizeof ".DAT" - 1;
constexpr std::size_t kRootMax = kPathMax - kLongestTail;
// Decode renders in pieces of this many frames and looks for a once-only
// song's end after each; 0x12000 bytes is 18 of them.
constexpr int kPiece = 1024;

bool FileExists(const char* path) {
    const DWORD a = GetFileAttributesA(path);
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

struct State {
    unsigned char armed = 0;       // BOF3X_MUSIC seq and a cache with base\bgm (or a named layer's bgm)
    char root[kRootMax + 1] = {};  // BOF3X_CACHE, no trailing separator
    // The bgm directories a song is looked for in, the first first; a song's
    // bank is its own directory's bank\. DIV-0109: with no source chosen, the
    // bgm directories of the layers BOF3X_OPT names that have one
    // (<root>\opt\<layer>\bgm), the last named first, then base\bgm.
    // DIV-0150: with one chosen, the chosen disc's songs, then the other's.
    std::vector<std::string> dirs;
    // DIV-0150: the source BOF3X_MUSIC chose - 0 none (seq, or unset: the order
    // above), 'x' the PlayStation's, 'p' the PSP's, 'c' the PC's MP3s first.
    char source = 0;
    // DIV-0150: under a chosen source, where Sound_LoadStream's 11 kind-0
    // streams are looked for, the first first: a cache directory, or "" for
    // the install's SND\ (the original's own name).
    std::vector<std::string> stream_dirs;
    bool (*exists)(const char*) = FileExists;
    // The cache song LoadFile put in Music_File (nullptr: Music_File is not one).
    const void* song_file = nullptr;
    unsigned song_size = 0;
    unsigned track = 0;
    bool active = false;           // the stream Music_Start began is the synth's
    psx::Song* song = nullptr;     // the song LoadFile parsed
    psx::Bank* bank = nullptr;     // the bank it wants, parsed
    std::string bank_path;         // the file `bank` was read from ("" none): a layer's bank may share a base bank's name
    std::string song_from;         // where the song came from, for the log (base\bgm or opt\<layer>\bgm)
    std::string synth_bank;        // the bank file the synth holds ("" none)
    psx::MusicSynth* synth = nullptr;
};
State g_s;

// The cache file being parsed, for the abort's message (nullptr: none).
const char* g_parsing = nullptr;

void MusicAbort(const char* message) {
    if (g_parsing) bof3::Fatal("DIV-0087: %s: %s", g_parsing, message);
    bof3::Fatal("DIV-0087: %s", message);
}
void SpuAbort(const char* message) { bof3::Fatal("DIV-0087: SPU model: %s", message); }

void Objects() {
    if (g_s.synth) return;
    psx::SetMusicAbortHook(MusicAbort);
    psx::SetSpuAbortHook(SpuAbort);
    g_s.song = new psx::Song;
    g_s.bank = new psx::Bank;
    g_s.synth = new psx::MusicSynth;
}

bool Mono() { return *reinterpret_cast<const volatile unsigned char*>(static_cast<std::uintptr_t>(Rest4f_Setting3_addr)) != 0; }

// The whole of `path` through the sound layer's file callees into `out`
// (the bank; the song is read in LoadFile as the original reads an MP3).
void ReadBank(const char* path, std::vector<std::uint8_t>* out) {
    const int handle = g.file_open(path, 0, 0);
    if (handle == -1) bof3::Fatal("DIV-0087: %s is there but does not open", path);
    const int size = g.file_size(handle);
    if (size <= 0) bof3::Fatal("DIV-0087: %s has %d bytes", path, size);
    out->resize(static_cast<std::size_t>(size));
    const unsigned got = g.file_read(handle, out->data(), static_cast<unsigned>(size));
    g.file_close(handle);
    if (got != static_cast<unsigned>(size)) bof3::Fatal("DIV-0087: %s: read %u of %d bytes", path, got, size);
}

// What a song file must be beyond its format (song.cpp): the song its name
// says, and a bank name that is a file name.
void CheckSong(const char* path, unsigned track, const psx::Song& song) {
    if (song.number != track) bof3::Fatal("DIV-0087: %s holds song %u", path, song.number);
    for (const char c : song.bank)
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-'))
            bof3::Fatal("DIV-0087: %s names bank \"%s\"", path, song.bank.c_str());
}

// `bytes` (from `path`) parsed into `out`: the format, then what the synth
// refuses, then the name the song asked for. Any failure is fatal and names
// the file.
void ParseBank(const char* path, const std::vector<std::uint8_t>& bytes, const std::string& want, psx::Bank* out) {
    g_parsing = path;
    psx::LoadBank(bytes.data(), bytes.size(), out);
    psx::MusicSynth::CheckBank(*out);
    g_parsing = nullptr;
    if (out->name != want) bof3::Fatal("DIV-0087: %s is bank %s", path, out->name.c_str());
}

// The whole of `path` by Win32 (start-up only: the cache's check).
void ReadWhole(const char* path, std::vector<std::uint8_t>* out) {
    const HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) bof3::Fatal("DIV-0087: %s does not open (error %lu)", path, GetLastError());
    LARGE_INTEGER size;
    // far above any song or bank (a bank's samples fit 0x6B6C0 bytes)
    constexpr LONGLONG kFileMax = 64ll << 20;
    if (!GetFileSizeEx(h, &size) || size.QuadPart <= 0 || size.QuadPart > kFileMax) {
        CloseHandle(h);
        bof3::Fatal("DIV-0087: %s has %lld bytes", path, static_cast<long long>(size.QuadPart));
    }
    out->resize(static_cast<std::size_t>(size.QuadPart));
    DWORD got = 0;
    const BOOL ok = ReadFile(h, out->data(), static_cast<DWORD>(out->size()), &got, nullptr);
    CloseHandle(h);
    if (!ok || got != out->size()) bof3::Fatal("DIV-0087: %s: read %lu of %zu bytes", path, got, out->size());
}

// Every song file the seam could be asked for in one bgm directory (base\bgm
// or a layer's, NNN.DAT, "%03u" of a track) parsed and checked as LoadFile
// checks it, and each bank they name once, from that directory's bank\: a
// damaged cache stops the game at start-up, naming the file, rather than at
// the scene change that first plays it. A track with no file is not an error
// - it plays the song under it, or its MP3.
void CheckCache(const char* dir) {
    char pattern[kPathMax + 1];
    std::snprintf(pattern, sizeof pattern, "%s\\*.DAT", dir);
    WIN32_FIND_DATAA found;
    const HANDLE find = FindFirstFileA(pattern, &found);
    if (find == INVALID_HANDLE_VALUE) {
        const DWORD e = GetLastError();
        if (e != ERROR_FILE_NOT_FOUND) bof3::Fatal("DIV-0087: cannot list %s (error %lu)", pattern, e);
        bof3::Log("DIV-0087    %s holds no song", dir);
        return;
    }
    std::vector<std::string> banks;
    std::vector<std::uint8_t> bytes;
    unsigned songs = 0;
    do {
        if (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        const char* name = found.cFileName;
        std::size_t digits = 0;
        while (name[digits] >= '0' && name[digits] <= '9') ++digits;
        if (digits == 0 || digits > 9) continue;
        const unsigned track = static_cast<unsigned>(std::strtoul(name, nullptr, 10));
        char expect[16];
        std::snprintf(expect, sizeof expect, "%03u.DAT", track);
        if (lstrcmpiA(expect, name) != 0) continue;  // not a name LoadFile builds
        char path[kPathMax + 1];
        std::snprintf(path, sizeof path, "%s\\%s", dir, expect);
        ReadWhole(path, &bytes);
        psx::Song song;
        g_parsing = path;
        psx::LoadSong(bytes.data(), bytes.size(), &song);
        g_parsing = nullptr;
        CheckSong(path, track, song);
        ++songs;
        bool seen = false;
        for (const std::string& b : banks) seen = seen || b == song.bank;
        if (seen) continue;
        char bank_path[kPathMax + 1];
        std::snprintf(bank_path, sizeof bank_path, "%s\\bank\\%s.DAT", dir, song.bank.c_str());
        if (!FileExists(bank_path)) bof3::Fatal("DIV-0087: %s wants %s, which is not there", path, bank_path);
        ReadWhole(bank_path, &bytes);
        psx::Bank bank;
        ParseBank(bank_path, bytes, song.bank, &bank);
        banks.push_back(song.bank);
    } while (FindNextFileA(find, &found));
    const DWORD e = GetLastError();
    FindClose(find);
    if (e != ERROR_NO_MORE_FILES) bof3::Fatal("DIV-0087: listing %s stopped (error %lu)", pattern, e);
    bof3::Log("DIV-0087    the cache checked: %s, %u songs and %u banks parse", dir, songs,
              static_cast<unsigned>(banks.size()));
}

}  // namespace

// The cache file for `track` into path and its directory into dir; false
// when the cache has none. State::dirs in order (DIV-0109, DIV-0150).
bool Find(unsigned track, char (&path)[kPathMax + 1], std::string& dir) {
    if (!g_s.armed) return false;
    for (const std::string& d : g_s.dirs) {
        std::snprintf(path, sizeof path, "%s\\%03u.DAT", d.c_str(), track);
        if (g_s.exists(path)) {
            dir = d;
            return true;
        }
    }
    dir.clear();
    return false;
}

bool Probe(unsigned track, int* size) {
    char path[kPathMax + 1];
    std::string dir;
    if (!Find(track, path, dir)) return false;
    const int handle = g.file_open(path, 0, 0);
    if (handle == -1) bof3::Fatal("DIV-0087: %s is there but does not open", path);
    *size = g.file_size(handle);
    g.file_close(handle);
    return true;
}

bool LoadFile(unsigned track) {
    if (!g_s.armed) return false;
    char path[kPathMax + 1];
    std::string dir;
    if (!Find(track, path, dir)) {
        bof3::Log("DIV-0087    track %03u: not in the cache, its MP3", track);
        return false;
    }
    Objects();
    const int handle = g.file_open(path, 0, 0);
    if (handle == -1) bof3::Fatal("DIV-0087: %s is there but does not open", path);
    // as the original's MP3: the old file freed once the new one has opened
    if (Music_File) g.free(Music_File);
    const int size = g.file_size(handle);
    if (size <= 0) bof3::Fatal("DIV-0087: %s has %d bytes", path, size);
    Music_FileSize = size;
    void* const file = g.malloc(static_cast<unsigned>(size));
    Music_File = file;
    const unsigned got = g.file_read(handle, file, static_cast<unsigned>(size));
    g.file_close(handle);
    if (got != static_cast<unsigned>(size)) bof3::Fatal("DIV-0087: %s: read %u of %d bytes", path, got, size);
    // Arm checked every song and bank of the cache; these are the same checks
    // again, for a cache changed while the game runs.
    g_parsing = path;
    psx::LoadSong(static_cast<const std::uint8_t*>(file), static_cast<std::size_t>(size), g_s.song);
    g_parsing = nullptr;
    CheckSong(path, track, *g_s.song);
    // The song's bank from its own directory. A layer's bank may bear a base
    // bank's name (the PSP's are named as the PSX's), so the file, not the
    // name, says whether the bank held is the one wanted.
    std::snprintf(path, sizeof path, "%s\\bank\\%s.DAT", dir.c_str(), g_s.song->bank.c_str());
    if (g_s.bank_path != path) {
        if (!g_s.exists(path)) bof3::Fatal("DIV-0087: song %u wants %s, which is not there", track, path);
        std::vector<std::uint8_t> bytes;
        ReadBank(path, &bytes);
        g_s.bank_path.clear();
        ParseBank(path, bytes, g_s.song->bank, g_s.bank);
        g_s.bank_path = path;
    }
    g_s.song_from = dir.substr(std::strlen(g_s.root) + 1);
    Music_FileLoops = static_cast<int>(g_s.song->flags & 1);
    Music_LoadedTrack = static_cast<int>(track);
    g_s.song_file = file;
    g_s.song_size = static_cast<unsigned>(size);
    g_s.track = track;
    return true;
}

void Forget(const void* file) {
    if (file && file == g_s.song_file) g_s.song_file = nullptr;
}

bool IsSong(const void* file, unsigned size) {
    return g_s.song_file && file == g_s.song_file && size == g_s.song_size;
}

void Begin() {
    Objects();
    if (g_s.synth_bank != g_s.bank_path) {
        g_s.synth->LoadBank(*g_s.bank);
        g_s.synth_bank = g_s.bank_path;
    }
    g_s.synth->SetMono(Mono());
    // At sequence volume 127 from the first tick: a ramp to 1 over one frame
    // (Play's crescendo, which then does nothing), then SsSepSetVol(127, 127).
    // The game's fades act on the DirectSound buffer (Level), not here: the
    // ring renders 0.4 to 0.8 s ahead of what is heard.
    g_s.synth->Play(*g_s.song, 1, 1);
    g_s.synth->SetVolume(127, 127);
    g_s.active = true;
    bof3::Log("DIV-0087    track %03u from the cache: %s\\%03u.DAT, bank %s, %s", g_s.track, g_s.song_from.c_str(),
              g_s.track, g_s.song->bank.c_str(), (g_s.song->flags & 1) ? "looping" : "once");
}

bool Active() { return g_s.active; }

void Decode(unsigned char* dst, int size) {
    if (size <= 0) return;
    g_s.synth->SetMono(Mono());
    const int frames = size / 4;
    std::int16_t piece[2 * kPiece];
    int done = 0;
    while (done < frames) {
        const int n = frames - done < kPiece ? frames - done : kPiece;
        g_s.synth->Render(piece, n);
        std::memcpy(dst + static_cast<std::size_t>(done) * 4, piece, static_cast<std::size_t>(n) * 4);
        done += n;
        if (g_s.synth->Ended()) {
            std::memset(dst + static_cast<std::size_t>(done) * 4, 0, static_cast<std::size_t>(size - done * 4));
            Music_Finished = 1;
            return;
        }
    }
    std::memset(dst + static_cast<std::size_t>(done) * 4, 0, static_cast<std::size_t>(size - done * 4));
}

std::uint32_t Fold(std::uint32_t heard) {
    if (!g_s.synth) return heard;
    const int jumps = g_s.synth->LoopJumps();
    const std::uint64_t first = g_s.synth->FirstLoopJumpSample(), last = g_s.synth->LastLoopJumpSample();
    if (jumps < 2 || heard < first || last <= first) return heard;
    const std::uint64_t period = (last - first) / static_cast<std::uint64_t>(jumps - 1);
    if (period == 0) return heard;
    return static_cast<std::uint32_t>(first + (heard - first) % period);
}

bool FastForward(std::uint32_t frames) {
    g_s.synth->SetMono(Mono());
    return g_s.synth->FastForward(frames);
}

long Level(float volume) {
    int v;
    if (!(volume >= 1.0f)) v = 1;  // NaN too: the original's _ftol gives 0 for it, which libsnd makes 1
    else if (volume >= 127.0f) v = 127;
    else v = static_cast<int>(volume);
    return std::lround(4000.0 * std::log10(v / 127.0));
}

void Release() {
    if (g_s.synth) g_s.synth->Stop();
    g_s.active = false;
}

void Stop() {
    if (g_s.synth && g_s.active) g_s.synth->Stop();
}

bool PcFirst() { return g_s.armed && g_s.source == 'c'; }

bool StreamPath(const char* name, char* out, std::size_t cap) {
    if (!g_s.source || g_s.stream_dirs.empty()) return false;
    for (const std::string& d : g_s.stream_dirs) {
        if (d.empty()) {
            std::snprintf(out, cap, "SND\\%s.DAT", name);
            if (g_s.exists(out)) {
                bof3::Log("sound       DIV-0150: stream %s from %s", name, out);
                out[0] = 0;  // the install's: the caller formats the original's own name
                return true;
            }
        } else {
            std::snprintf(out, cap, "%s\\%s.DAT", d.c_str(), name);
            if (g_s.exists(out)) {
                bof3::Log("sound       DIV-0150: stream %s from %s", name, out);
                return true;
            }
        }
    }
    // in none: the first choice's, whose failed open the caller reports (DIV-0129)
    const std::string& d = g_s.stream_dirs.front();
    if (d.empty()) out[0] = 0;
    else std::snprintf(out, cap, "%s\\%s.DAT", d.c_str(), name);
    return true;
}

namespace {

// DIV-0150: the directories a chosen source puts in order, for the songs
// (part "bgm") or the streams ("snd"): its disc's, then the other disc's; the
// PC's MP3s are the install's, outside these lists. The PlayStation's are
// base\ on a PlayStation base; the PSP's are base\ on a PSP base (a PSP-only
// cache, DIV-0103), else the PSP layer of that part (DIV-0109's psp-music,
// DIV-0110's psp-snd; recipes/suites.toml) when the cache offers it. The
// PlayStation's streams beside a PC install are base\psx-jingles (the importer
// keeps them out of base\snd there), so its stream directories are that, then
// base\snd.
std::vector<std::string> SourceDirs(const char* root, bool psp_base, bool psp_first, const char* part) {
    const bool bgm = std::strcmp(part, "bgm") == 0;
    const auto is_dir = [](const std::string& d) {
        const DWORD a = GetFileAttributesA(d.c_str());
        return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
    };
    std::vector<std::string> psx, psp;
    const std::string base = std::string(root) + "\\base\\" + part;
    const char* const layer = bgm ? "psp-music" : "psp-snd";
    if (psp_base) {
        if (is_dir(base)) psp.push_back(base);
    } else {
        const std::string jingles = std::string(root) + "\\base\\psx-jingles";
        if (!bgm && is_dir(jingles)) psx.push_back(jingles);
        if (is_dir(base)) psx.push_back(base);
        if (dat_cache::HasLayerPart(layer, part)) psp.push_back(std::string(root) + "\\opt\\" + layer + "\\" + part);
    }
    std::vector<std::string> out = psp_first ? psp : psx;
    const std::vector<std::string>& second = psp_first ? psx : psp;
    out.insert(out.end(), second.begin(), second.end());
    return out;
}

const char* SourceName(char source) {
    return source == 'x' ? "the PlayStation's" : source == 'p' ? "the PSP's" : "the PC's MP3s";
}

}  // namespace

void Arm() {
    char mode[8];
    const DWORD m = GetEnvironmentVariableA("BOF3X_MUSIC", mode, sizeof mode);
    // DIV-0150: psx, psp, pc choose a source; seq and mp3 (DIV-0087) as before
    if (m >= sizeof mode || (m != 0 && std::strcmp(mode, "seq") != 0 && std::strcmp(mode, "mp3") != 0 &&
                             std::strcmp(mode, "psx") != 0 && std::strcmp(mode, "psp") != 0 &&
                             std::strcmp(mode, "pc") != 0))
        bof3::Fatal("DIV-0150: BOF3X_MUSIC must be psx, psp, pc, seq or mp3");
    const char source = m == 0 ? 0 : std::strcmp(mode, "psx") == 0 ? 'x' : std::strcmp(mode, "psp") == 0 ? 'p'
                                                                   : std::strcmp(mode, "pc") == 0    ? 'c'
                                                                                                      : 0;
    char root[MAX_PATH];
    const DWORD n = GetEnvironmentVariableA("BOF3X_CACHE", root, sizeof root);
    if (n >= sizeof root) bof3::Fatal("DIV-0087: BOF3X_CACHE is %lu characters", static_cast<unsigned long>(n));
    if (n == 0) {
        bof3::Log("DIV-0087    no cache (BOF3X_CACHE unset): every track plays its MP3");
        return;
    }
    std::size_t len = n;
    while (len > 1 && (root[len - 1] == '\\' || root[len - 1] == '/')) root[--len] = 0;
    const DWORD a = GetFileAttributesA(root);
    if (a == INVALID_FILE_ATTRIBUTES || !(a & FILE_ATTRIBUTE_DIRECTORY))
        bof3::Fatal("DIV-0087: BOF3X_CACHE=%s is not a directory", root);
    if (len > kRootMax)
        bof3::Fatal("DIV-0087: BOF3X_CACHE=%s is %u characters; its longest path (a bank) must stay within MAX_PATH, "
                    "so at most %u",
                    root, static_cast<unsigned>(len), static_cast<unsigned>(kRootMax));
    char bgm[MAX_PATH + 16];
    std::snprintf(bgm, sizeof bgm, "%s\\base\\bgm", root);
    const DWORD b = GetFileAttributesA(bgm);
    const bool base = b != INVALID_FILE_ATTRIBUTES && (b & FILE_ATTRIBUTE_DIRECTORY);
    // DIV-0109: the layers BOF3X_OPT names with a bgm part (DatLoad_Inject read
    // and checked them; dat_cache answers for the same root, BOF3X_CACHE)
    std::vector<std::string> layers;
    for (const std::string& l : dat_cache::LayersWith("bgm")) {
        // its longest path: <root>\opt\<layer>\bgm\bank\ + a 15-character bank + .DAT
        if (len + sizeof "\\opt\\" - 1 + l.size() + sizeof "\\bgm\\bank\\" - 1 + 15 + sizeof ".DAT" - 1 > kPathMax)
            bof3::Fatal("DIV-0109: BOF3X_CACHE=%s with layer %s: its banks' paths would pass MAX_PATH; a shorter cache "
                        "path, or leave %s out of BOF3X_OPT", root, l.c_str(), l.c_str());
        layers.push_back(std::string(root) + "\\opt\\" + l + "\\bgm");
    }
    if (source) {
        // DIV-0150: the chosen source's order, BOF3X_OPT's bgm layers aside
        const std::wstring wroot(root, root + len);
        const bool psp_base = bof3x::ManifestPspBase(bof3x::ManifestRead(wroot));
        const std::vector<std::string> dirs = SourceDirs(root, psp_base, source == 'p', "bgm");
        for (const std::string& d : dirs)
            if (d.size() + sizeof "\\bank\\" - 1 + 15 + sizeof ".DAT" - 1 > kPathMax)
                bof3::Fatal("DIV-0150: BOF3X_CACHE=%s: the banks' paths under %s would pass MAX_PATH; a shorter cache "
                            "path", root, d.c_str());
        std::memcpy(g_s.root, root, len + 1);
        g_s.source = source;
        g_s.dirs = dirs;
        // the streams: under the cache's data only (dat_cache configured), as SoundPath
        g_s.stream_dirs.clear();
        if (dat_cache::Root()) g_s.stream_dirs = SourceDirs(root, psp_base, source == 'p', "snd");
        g_s.stream_dirs.insert(source == 'c' ? g_s.stream_dirs.begin() : g_s.stream_dirs.end(), std::string());
        if (!dirs.empty()) {
            Objects();
            for (const std::string& d : dirs) CheckCache(d.c_str());
            g_s.armed = 1;
        }
        std::string order = source == 'c' ? "BGM\\" : "";
        for (const std::string& d : dirs) order += (order.empty() ? "" : ", ") + d.substr(len + 1);
        if (source != 'c') order += (order.empty() ? "" : ", ") + std::string("BGM\\");
        bof3::Log("DIV-0150    music from %s first (BOF3X_MUSIC=%s): songs from %s, the first that has the track",
                  SourceName(source), mode, order.c_str());
        std::string streams;
        for (const std::string& d : g_s.stream_dirs)
            streams += (streams.empty() ? "" : ", ") + (d.empty() ? std::string("SND\\") : d.substr(len + 1));
        bof3::Log("DIV-0150    the 11 recorded streams from %s, the first that has the file", streams.c_str());
        return;
    }
    if (!base && layers.empty()) {
        bof3::Log("DIV-0087    BOF3X_CACHE=%s has no base\\bgm: every track plays its MP3", root);
        return;
    }
    if (m != 0 && std::strcmp(mode, "mp3") == 0) {
        bof3::Log("DIV-0087    BOF3X_MUSIC=mp3: every track plays its MP3 (the cache %s not used for music%s)", root,
                  layers.empty() ? "" : "; the named layers' songs neither: music= chooses the MP3s over every song");
        return;
    }
    std::memcpy(g_s.root, root, len + 1);
    g_s.dirs.assign(layers.rbegin(), layers.rend());
    if (base) g_s.dirs.push_back(bgm);
    Objects();
    for (const std::string& d : layers) CheckCache(d.c_str());
    if (base) CheckCache(bgm);
    g_s.armed = 1;
    for (const std::string& d : layers)
        bof3::Log("DIV-0109    songs from %s first, where it has the song (the layer named in BOF3X_OPT)", d.c_str());
    bof3::Log("DIV-0087    music from the cache where it has the song: %s\\base\\bgm (BOF3X_MUSIC=mp3 for the MP3s)",
              root);
}

// --- BOF3X_SHADOW=sound -------------------------------------------------------
namespace {

constexpr char kTestRoot[] = "T:\\bof3x-selftest";
constexpr unsigned kHalf = sound::kHalf;

void Put16(std::vector<std::uint8_t>& b, unsigned v) {
    b.push_back(static_cast<std::uint8_t>(v));
    b.push_back(static_cast<std::uint8_t>(v >> 8));
}
void Put32(std::vector<std::uint8_t>& b, std::uint32_t v) {
    Put16(b, v & 0xFFFF);
    Put16(b, v >> 16);
}
void PutName(std::vector<std::uint8_t>& b, const char* s) {
    for (std::size_t i = 0; i < 16; ++i) b.push_back(i < std::strlen(s) ? static_cast<std::uint8_t>(s[i]) : 0);
}

// docs/seq-format.md section 2, version 2: `programs` programs (0, 1, ...)
// of one tone each over the keyboard, centre 60, reverb on (off when
// `reverb` is false), and one looping
// sample - a zero block, then two blocks of a triangle at shift 0, filter 0
// (the host tests' bank, tools/bgm/host/seq_tests.cpp MakeBank).
std::vector<std::uint8_t> TestBank(const char* name = "SELFTEST", int programs = 1, bool reverb = true) {
    std::vector<std::uint8_t> b;
    b.insert(b.end(), {'B', 'F', '3', 'B'});
    Put32(b, 2);
    PutName(b, name);
    Put16(b, static_cast<unsigned>(programs)); Put16(b, static_cast<unsigned>(programs)); Put16(b, 1);  // ps, ts, vs
    b.insert(b.end(), {127, 64, 0, 0, 0, 0});      // mvol, pan, attr1, attr2, 0
    for (int p = 0; p < 128; ++p) {
        if (p < programs) b.insert(b.end(), {1, 127, 0, 0, 64, static_cast<std::uint8_t>(p), 0, 0});
        else b.insert(b.end(), {0, 0, 0, 0, 0, 0xFF, 0, 0});
    }
    for (int t = 0; t < 16 * programs; ++t) {
        if (t % 16 == 0) {
            b.insert(b.end(), {0, static_cast<std::uint8_t>(reverb ? 4 : 0), 127, 64, 60, 0, 0, 127, 0, 0, 0, 0, 0, 0, 0, 0});
            Put16(b, 0x00FF); Put16(b, 0x1FC0); Put16(b, static_cast<unsigned>(t / 16)); Put16(b, 1);
        } else {
            b.insert(b.end(), 24, 0);
        }
    }
    Put32(b, 0); Put32(b, 48);
    std::vector<std::uint8_t> body(48, 0);
    for (int k = 0; k < 2; ++k) {
        std::uint8_t* blk = &body[16 + 16 * static_cast<std::size_t>(k)];
        blk[1] = k == 0 ? 0x04 : 0x03;  // loop start; loop end + repeat
        for (int i = 0; i < 28; ++i) {
            const int x = k * 28 + i;
            const int v = (x < 28 ? x / 2 : (55 - x) / 2) - 7;
            const std::uint8_t nib = static_cast<std::uint8_t>(v & 0xF);
            blk[2 + i / 2] |= static_cast<std::uint8_t>(i & 1 ? nib << 4 : nib);
        }
    }
    b.insert(b.end(), body.begin(), body.end());
    return b;
}

// docs/seq-format.md section 1: resolution 48 at 120 bpm (16 tenths of a tick
// a VSync), four notes over 96 ticks; `loops` with the loop markers (the whole
// song the body), else without them, once; on program `program` of `bank`.
std::vector<std::uint8_t> TestSong(unsigned number, bool loops, const char* bank = "SELFTEST", std::uint8_t program = 0) {
    struct E { std::uint32_t tick; std::uint8_t st, d1, d2; };
    std::vector<E> ev = {{0, 0xC0, program, 0}, {0, 0xB0, 7, 100}};
    if (loops) ev.insert(ev.end(), {{0, 0xB0, 99, 20}, {0, 0xB0, 6, 127}});
    ev.insert(ev.end(), {{0, 0x90, 60, 100}, {24, 0x90, 60, 0}, {48, 0x90, 64, 100}, {60, 0x90, 72, 90},
                         {72, 0x90, 64, 0}, {84, 0x90, 72, 0}});
    if (loops) ev.push_back({96, 0xB0, 99, 30});
    ev.push_back({96, 0xFF, 0x2F, 0});
    std::vector<std::uint8_t> b;
    b.insert(b.end(), {'B', 'F', '3', 'S'});
    Put32(b, 1);
    Put32(b, loops ? 1 : 0);
    PutName(b, bank);
    Put16(b, number); Put16(b, 0); Put16(b, 48); Put16(b, 0);
    Put32(b, 500000);
    Put32(b, static_cast<std::uint32_t>(ev.size()));
    Put32(b, loops ? 0 : 0xFFFFFFFFu);
    Put32(b, loops ? 96 : 0xFFFFFFFFu);
    Put32(b, 96);
    for (const E& e : ev) {
        Put32(b, e.tick);
        b.insert(b.end(), {e.st, e.d1, e.d2, 0});
        Put32(b, 0);
    }
    return b;
}

// The file layer, the heap, the decoder and the buffer, stood in for.
struct TestFile { std::string path; const std::vector<std::uint8_t>* bytes; unsigned opens; };
std::vector<TestFile>* g_files;
const std::vector<std::uint8_t>* g_handles[4];
std::vector<std::string>* g_names;   // every path file_open was asked for
int g_decoders_opened;
std::vector<std::uint8_t>* g_capture; // what Music_CreateBuffer's stand-in decoded
std::vector<long>* g_levels;          // what SetVolume was given

TestFile* Lookup(const char* path) {
    for (TestFile& f : *g_files)
        if (f.path == path) return &f;
    return nullptr;
}
bool TestExists(const char* path) { return Lookup(path) != nullptr; }
int __cdecl TestSprintf(char* out, const char* format, ...) {
    va_list args;
    va_start(args, format);
    const int n = std::vsprintf(out, format, args);
    va_end(args);
    return n;
}
void* __cdecl TestMalloc(unsigned n) { return std::malloc(n ? n : 1); }
void __cdecl TestFree(void* p) { std::free(p); }
int __cdecl TestOpen(const char* path, int, int) {
    g_names->push_back(path);
    TestFile* f = Lookup(path);
    if (!f) return -1;
    for (int h = 0; h < 4; ++h)
        if (!g_handles[h]) {
            g_handles[h] = f->bytes;
            ++f->opens;
            return h;
        }
    bof3::Fatal("DIV-0087 self-test: more than four files open");
}
int __cdecl TestSize(int h) { return static_cast<int>(g_handles[h]->size()); }
unsigned __cdecl TestRead(int h, void* dst, unsigned n) {
    const unsigned have = static_cast<unsigned>(g_handles[h]->size());
    if (n > have) n = have;
    std::memcpy(dst, g_handles[h]->data(), n);
    return n;
}
void __cdecl TestClose(int h) { g_handles[h] = nullptr; }
void* __cdecl TestOpenDecoder(const void*, unsigned) {
    ++g_decoders_opened;
    return nullptr;
}
// Music_CreateBuffer's work as far as the stream goes: its first half decoded
// (through g.music_decode, ours); no DirectSound buffer.
void* __cdecl TestCreateBuffer() {
    const std::size_t at = g_capture->size();
    g_capture->resize(at + kHalf);
    g.music_decode(g_capture->data() + at, static_cast<int>(kHalf));
    return nullptr;
}
long __stdcall TestSetVolume(void*, unsigned long level) {
    g_levels->push_back(static_cast<long>(level));
    return 0;
}
long __stdcall TestUnexpected(void*) { bof3::Fatal("DIV-0087 self-test: a DirectSound method other than SetVolume"); }
const void* g_vtable[0x54 / 4];
struct FakeBuffer { const void* const* vtable; } g_buffer = {g_vtable};

// `m` started as Begin starts it, rendered in Decode's pieces; `ended` (when
// given) the first piece boundary after which Ended() held, or -1, and zeros
// after it. The synth
// is the caller's and lives across songs, as the game's does: a song that
// follows another starts over the first's release tails and reverb, as on the
// PlayStation's SPU, and its end waits for every voice's envelope.
std::vector<std::int16_t> Reference(psx::MusicSynth* m, const std::vector<std::uint8_t>& song_bytes, int frames,
                                    int* ended = nullptr) {
    psx::Song song;
    psx::LoadSong(song_bytes.data(), song_bytes.size(), &song);
    m->SetMono(Mono());
    m->Play(song, 1, 1);
    m->SetVolume(127, 127);
    std::vector<std::int16_t> out(2 * static_cast<std::size_t>(frames));
    if (ended) {
        *ended = -1;
        // rendering stops at the end, as Decode's does, so the synth is left
        // where the game's is for the song that follows
        for (int done = 0; done < frames && *ended < 0; done += kPiece) {
            const int n = frames - done < kPiece ? frames - done : kPiece;
            m->Render(out.data() + 2 * static_cast<std::size_t>(done), n);
            if (m->Ended()) *ended = done + n;
        }
    } else {
        m->Render(out.data(), frames);
    }
    return out;
}

bool Opened(const char* name) {
    for (const std::string& n : *g_names)
        if (n == name) return true;
    return false;
}

}  // namespace

void SelfTest() {
    Objects();
    const sound::Callees saved_g = g;
    void* const file = Music_File;
    const int file_size = Music_FileSize, file_loops = Music_FileLoops, loaded = Music_LoadedTrack;
    void* const data = Music_Data;
    void* const decoder = Music_Decoder;
    void* const buffer = Music_Buffer;
    void* const notify = Music_Notify;
    void* const device = Snd_Device;
    const int finished = Music_Finished, loops = Music_Loops;
    const State saved_s = g_s;

    const std::vector<std::uint8_t> bank_bytes = TestBank(), looping = TestSong(7, true), once = TestSong(8, false);
    // a bank of two programs, song 10 on its second, and song 11 back on the
    // one-program bank
    const std::vector<std::uint8_t> big_bytes = TestBank("SELFTST2", 2), on_big = TestSong(10, true, "SELFTST2", 1),
                                    after_big = TestSong(11, true);
    psx::Bank bank, big;
    psx::LoadBank(bank_bytes.data(), bank_bytes.size(), &bank);
    psx::LoadBank(big_bytes.data(), big_bytes.size(), &big);
    psx::MusicSynth* const ref = new psx::MusicSynth;  // the oracle, one synth across the songs as g_s.synth is
    ref->LoadBank(bank);
    std::vector<TestFile> files = {{std::string(kTestRoot) + "\\base\\bgm\\007.DAT", &looping, 0},
                                   {std::string(kTestRoot) + "\\base\\bgm\\008.DAT", &once, 0},
                                   {std::string(kTestRoot) + "\\base\\bgm\\bank\\SELFTEST.DAT", &bank_bytes, 0},
                                   {std::string(kTestRoot) + "\\base\\bgm\\010.DAT", &on_big, 0},
                                   {std::string(kTestRoot) + "\\base\\bgm\\011.DAT", &after_big, 0},
                                   {std::string(kTestRoot) + "\\base\\bgm\\bank\\SELFTST2.DAT", &big_bytes, 0}};
    std::vector<std::string> names;
    std::vector<std::uint8_t> capture;
    std::vector<long> levels;
    g_files = &files;
    g_names = &names;
    g_capture = &capture;
    g_levels = &levels;
    g_decoders_opened = 0;
    for (auto& v : g_vtable) v = reinterpret_cast<const void*>(&TestUnexpected);
    g_vtable[sound::kSetVolume / 4] = reinterpret_cast<const void*>(&TestSetVolume);

    g.sprintf = TestSprintf;
    g.malloc = TestMalloc;
    g.free = TestFree;
    g.file_open = TestOpen;
    g.file_size = TestSize;
    g.file_read = TestRead;
    g.file_close = TestClose;
    g.music_open_decoder = TestOpenDecoder;
    g.music_create_buffer = TestCreateBuffer;
    Music_File = nullptr;
    Music_Data = nullptr;
    Music_Decoder = nullptr;
    Music_Buffer = nullptr;
    Music_Notify = nullptr;
    Music_LoadedTrack = -1;
    static int device_stand_in;
    Snd_Device = &device_stand_in;  // Music_Start returns at once without one; never called through
    g_s.armed = 1;
    std::memcpy(g_s.root, kTestRoot, sizeof kTestRoot);
    g_s.exists = TestExists;
    g_s.song_file = nullptr;
    g_s.active = false;
    g_s.synth_bank.clear();
    g_s.bank->name.clear();
    g_s.bank_path.clear();
    const std::string base_dir = std::string(kTestRoot) + "\\base\\bgm";
    g_s.dirs = {base_dir};
    g_s.source = 0;
    g_s.stream_dirs.clear();

    // 1. A looping song: loaded, started, the first half by Music_CreateBuffer
    // and five more by Music_Decode in 0x12000-byte calls; equal to one Render.
    const int r7 = Music_LoadFile(7);
    const bool load7 = r7 == 0 && Music_LoadedTrack == 7 && Music_FileLoops == 1 &&
                       Music_FileSize == static_cast<int>(looping.size()) &&
                       std::memcmp(Music_File, looping.data(), looping.size()) == 0;
    Music_Start(Music_File, static_cast<unsigned>(Music_FileSize), Music_FileLoops);
    const bool start7 = Active() && !Music_Decoder && g_decoders_opened == 0 && Music_Loops == 1 && Music_Finished == 0;
    for (int i = 0; i < 5; ++i) {
        const std::size_t at = capture.size();
        capture.resize(at + kHalf);
        Music_Decode(capture.data() + at, static_cast<int>(kHalf));
    }
    const int frames7 = static_cast<int>(capture.size() / 4);
    const std::vector<std::int16_t> ref7 = Reference(ref, looping, frames7);
    long long energy = 0;
    for (std::int16_t v : ref7) energy += static_cast<long long>(v) * v;
    const bool same7 = std::memcmp(capture.data(), ref7.data(), capture.size()) == 0 && energy > 0 && Music_Finished == 0;

    // 2. The buffer's volume by libsnd's law, the synth left at 127.
    Music_Buffer = &g_buffer;
    Music_SetVolume(0.0f);
    Music_SetVolume(63.9f);
    Music_SetVolume(127.0f);
    Music_SetVolume(300.0f);
    Music_Buffer = nullptr;
    const bool volume = levels.size() == 4 && levels[0] == -8415 && levels[1] == -1218 && levels[2] == 0 && levels[3] == 0;

    // 3. A once-only song on the same bank (not read again), started over the
    // looping song's release tails as the game starts one: its end makes
    // Music_Finished with zeros after it, the samples before it the Render's.
    capture.clear();
    const int r8 = Music_LoadFile(8);
    const bool load8 = r8 == 0 && Music_LoadedTrack == 8 && Music_FileLoops == 0 && files[2].opens == 1;
    Music_Start(Music_File, static_cast<unsigned>(Music_FileSize), Music_FileLoops);
    for (int i = 0; i < 12 && !Music_Finished; ++i) {
        const std::size_t at = capture.size();
        capture.resize(at + kHalf);
        Music_Decode(capture.data() + at, static_cast<int>(kHalf));
    }
    const int frames8 = static_cast<int>(capture.size() / 4);
    int cut = -1;
    const std::vector<std::int16_t> ref8 = Reference(ref, once, frames8, &cut);
    const std::int16_t* got8 = reinterpret_cast<const std::int16_t*>(capture.data());
    bool match = cut > 0 && std::memcmp(got8, ref8.data(), static_cast<std::size_t>(cut) * 4) == 0;
    for (std::size_t i = 2 * static_cast<std::size_t>(cut > 0 ? cut : 0); i < 2 * static_cast<std::size_t>(frames8); ++i)
        match = match && got8[i] == 0;
    const bool end8 = Music_Finished == 1 && cut > 0 && cut < frames8 && match;

    // 4. A change of bank to a smaller one on the same synth (the review of
    // 2026-10-10): song 10 on program 1 (tone block 1) of the two-program
    // bank, then song 11 on the one-program bank. Each bank is read for its
    // song, and each song's samples are the oracle's, which changes bank with
    // it. Before LoadBank cleared the voice records, song 11's Play aborted in
    // SsSepSetVol(0, 0) on song 10's voices (block 1, tone 0 of a bank of 16
    // tones).
    const auto change = [&](unsigned track, const std::vector<std::uint8_t>& song, const psx::Bank& want) {
        capture.clear();
        if (Music_LoadFile(track) != 0 || Music_LoadedTrack != static_cast<int>(track)) return false;
        Music_Start(Music_File, static_cast<unsigned>(Music_FileSize), Music_FileLoops);
        for (int i = 0; i < 3; ++i) {
            const std::size_t at = capture.size();
            capture.resize(at + kHalf);
            Music_Decode(capture.data() + at, static_cast<int>(kHalf));
        }
        ref->LoadBank(want);
        const std::vector<std::int16_t> r = Reference(ref, song, static_cast<int>(capture.size() / 4));
        long long e = 0;
        for (std::int16_t v : r) e += static_cast<long long>(v) * v;
        return Active() && e > 0 && std::memcmp(capture.data(), r.data(), capture.size()) == 0;
    };
    const bool to_big = change(10, on_big, big) && files[5].opens == 1;
    const bool to_small = change(11, after_big, bank) && files[2].opens == 2;

    // 5. A track the cache lacks, and BOF3X_MUSIC=mp3: the MP3's names, the
    // cache's song untouched.
    names.clear();
    const int r9 = Music_LoadFile(9);
    const bool fallback = r9 == -1 && Opened("BGM\\009.DAT") && Opened("BGM\\009N.DAT") && Music_LoadedTrack == 11;
    names.clear();
    g_s.armed = 0;
    const int r7b = Music_LoadFile(7);
    const bool mp3 = r7b == -1 && Opened("BGM\\007.DAT") && Opened("BGM\\007N.DAT") && files[0].opens == 1;

    // 6. DIV-0092's resume on the sequenced path. On a synth of its own (so
    // the oracle's history is the same: nothing before), a looping song on a
    // bank without reverb - FastForward clears the reverb unit, the one part
    // of the SPU it does not step - pending at a sample where two notes sound
    // (tick 66 of 96): Music_Start fast-forwards to it, and what Music_Decode
    // renders from there is the oracle's render from the top at that sample,
    // bit for bit. A once-only song pending past its end is run to its end
    // and started again from the top (the oracle does the same).
    // And Fold on a synth after six seconds of the song (a loop a second): a
    // point past the last jump goes back by whole periods into the first loop.
    const std::vector<std::uint8_t> dry_bytes = TestBank("SELFTST3", 1, false), dry_song = TestSong(12, true, "SELFTST3"),
                                    dry_once = TestSong(13, false, "SELFTST3");
    files.push_back({std::string(kTestRoot) + "\\base\\bgm\\012.DAT", &dry_song, 0});
    files.push_back({std::string(kTestRoot) + "\\base\\bgm\\013.DAT", &dry_once, 0});
    files.push_back({std::string(kTestRoot) + "\\base\\bgm\\bank\\SELFTST3.DAT", &dry_bytes, 0});
    g_s.armed = 1;
    psx::Bank dry;
    psx::LoadBank(dry_bytes.data(), dry_bytes.size(), &dry);
    psx::MusicSynth* const game_synth = g_s.synth;
    const std::string game_synth_bank = g_s.synth_bank;
    const auto resumed = [&](unsigned track, const std::vector<std::uint8_t>& song, std::uint32_t at, std::uint32_t expect,
                             long long* energy_out) {
        g_s.synth = new psx::MusicSynth;
        g_s.synth_bank.clear();
        capture.clear();
        music_resume::Pend(track, at);
        bool ok = Music_LoadFile(track) == 0;
        Music_Start(Music_File, static_cast<unsigned>(Music_FileSize), Music_FileLoops);
        for (int i = 0; i < 2; ++i) {
            const std::size_t n = capture.size();
            capture.resize(n + kHalf);
            Music_Decode(capture.data() + n, static_cast<int>(kHalf));
        }
        psx::MusicSynth* const oracle = new psx::MusicSynth;
        oracle->LoadBank(dry);
        if (expect != at) {
            // asked past the end: the seam's synth ran the song to its end
            // without a sample, then started it again - so the oracle's does,
            // and its render from the top follows on the same synth
            psx::Song parsed;
            psx::LoadSong(song.data(), song.size(), &parsed);
            oracle->SetMono(Mono());
            oracle->Play(parsed, 1, 1);
            oracle->SetVolume(127, 127);
            ok = ok && !oracle->FastForward(at);
        }
        const int frames = static_cast<int>(expect + capture.size() / 4);
        const std::vector<std::int16_t> r = Reference(oracle, song, frames);
        long long e = 0;
        for (std::size_t i = 2 * static_cast<std::size_t>(expect); i < r.size(); ++i) e += static_cast<long long>(r[i]) * r[i];
        *energy_out = e;
        ok = ok && Active() && std::memcmp(capture.data(), r.data() + 2 * static_cast<std::size_t>(expect), capture.size()) == 0;
        delete oracle;
        delete g_s.synth;
        g_s.synth = game_synth;
        return ok;
    };
    const std::uint32_t mid = 44100u * 66 / 96;
    long long resume_energy = 0, past_energy = 0;
    const bool resume_mid = resumed(12, dry_song, mid, mid, &resume_energy) && resume_energy > 0;
    const bool resume_past = resumed(13, dry_once, 44100u * 30, 0, &past_energy) && past_energy > 0;
    g_s.synth = game_synth;
    g_s.synth_bank = game_synth_bank;
    psx::MusicSynth* const looper = new psx::MusicSynth;
    looper->LoadBank(dry);
    (void)Reference(looper, dry_song, 44100 * 6);
    g_s.synth = looper;
    const std::uint64_t first_jump = looper->FirstLoopJumpSample(), last_jump = looper->LastLoopJumpSample();
    const int jumps = looper->LoopJumps();
    const std::uint64_t period = jumps >= 2 ? (last_jump - first_jump) / static_cast<std::uint64_t>(jumps - 1) : 0;
    const std::uint32_t far_point = static_cast<std::uint32_t>(last_jump + 1234), near_point = static_cast<std::uint32_t>(first_jump / 2);
    const std::uint32_t folded = Fold(far_point);
    const bool fold = jumps >= 4 && period > 40000 && period < 48000 && folded >= first_jump &&
                      folded < first_jump + period && (far_point - folded) % period == 0 && Fold(near_point) == near_point;
    g_s.synth = game_synth;
    delete looper;

    // 7. DIV-0109, a layer's bgm (BOF3X_OPT naming it): song 7 is in the layer as
    // well as in base\bgm, on a bank of the same name in the layer's own
    // bank\ (the bank without reverb, under base's name) - the layer's song
    // and the layer's bank are read and played, the oracle's on that bank;
    // base's 007.DAT not opened (base first would open it). Then song 11,
    // which only base has, on base's SELFTEST: the bank held bears the same
    // name, and a seam that kept a bank by its name (as before DIV-0109) would
    // play the layer's - base's must be read again and the samples be the
    // oracle's on it. Then song 10, base's on its own bank.
    const std::vector<std::uint8_t> lay_bank = TestBank("SELFTEST", 1, false), lay_song = TestSong(7, true, "SELFTEST");
    const std::string lay_dir = std::string(kTestRoot) + "\\opt\\lay-m\\bgm";
    files.push_back({lay_dir + "\\007.DAT", &lay_song, 0});
    files.push_back({lay_dir + "\\bank\\SELFTEST.DAT", &lay_bank, 0});
    const std::size_t lay_song_at = files.size() - 2, lay_bank_at = files.size() - 1;
    psx::Bank lay;
    psx::LoadBank(lay_bank.data(), lay_bank.size(), &lay);
    g_s.dirs = {lay_dir, base_dir};
    names.clear();
    const unsigned base7_opens = files[0].opens;
    const bool layer_song = change(7, lay_song, lay) && files[lay_song_at].opens == 1 && files[lay_bank_at].opens == 1 &&
                            files[0].opens == base7_opens && g_s.song_from == "opt\\lay-m\\bgm";
    const unsigned big_opens = files[5].opens, small_opens = files[2].opens;
    // song 11 names SELFTEST: base's, read again, not the layer's of the same name
    const bool layer_name_twin = change(11, after_big, bank) && files[2].opens == small_opens + 1 &&
                                 files[lay_bank_at].opens == 1 && g_s.song_from == "base\\bgm";
    const bool layer_under = change(10, on_big, big) && files[5].opens == big_opens + 1 && g_s.song_from == "base\\bgm";

    // 8. DIV-0150. The PC's MP3s first: song 10's MP3 names are opened before
    // the cache's (neither MP3 is there, so the cache's song is loaded after
    // them). The PSP's first: the layer's song 7 over base's. The streams: the
    // first directory with the file, the install's SND\ as "" (the caller's
    // own name), and the first choice's path when none has it.
    g_s.source = 'c';
    g_s.dirs = {base_dir, lay_dir};
    names.clear();
    const int r10 = Music_LoadFile(10);
    const bool pc_first = r10 == 0 && names.size() == 3 && names[0] == "BGM\\010.DAT" && names[1] == "BGM\\010N.DAT" &&
                          names[2] == base_dir + "\\010.DAT" && Music_LoadedTrack == 10;
    g_s.source = 'p';
    g_s.dirs = {lay_dir, base_dir};
    names.clear();
    const bool psp_first = change(7, lay_song, lay) && !Opened("BGM\\007.DAT") && g_s.song_from == "opt\\lay-m\\bgm";
    const std::vector<std::uint8_t> jingle(16, 0);
    const std::string snd_dir = std::string(kTestRoot) + "\\base\\snd";
    files.push_back({snd_dir + "\\ABC.DAT", &jingle, 0});
    files.push_back({"SND\\DEF.DAT", &jingle, 0});
    char sp[kPathMax + 1];
    g_s.stream_dirs = {snd_dir, std::string()};
    const bool stream_cache = StreamPath("ABC", sp, sizeof sp) && snd_dir + "\\ABC.DAT" == sp;
    const bool stream_install = StreamPath("DEF", sp, sizeof sp) && sp[0] == 0;
    const bool stream_none = StreamPath("GHI", sp, sizeof sp) && snd_dir + "\\GHI.DAT" == sp;
    g_s.stream_dirs = {std::string(), snd_dir};
    const bool stream_pc = StreamPath("DEF", sp, sizeof sp) && sp[0] == 0 && StreamPath("ABC", sp, sizeof sp) &&
                           snd_dir + "\\ABC.DAT" == sp;
    g_s.source = 0;
    const bool stream_legacy = !StreamPath("ABC", sp, sizeof sp);

    Music_Release();
    const bool released = !Active() && !Music_Data;
    delete ref;
    std::free(Music_File);

    g = saved_g;
    Music_File = file;
    Music_FileSize = file_size;
    Music_FileLoops = file_loops;
    Music_LoadedTrack = loaded;
    Music_Data = data;
    Music_Decoder = decoder;
    Music_Buffer = buffer;
    Music_Notify = notify;
    Snd_Device = device;
    Music_Finished = finished;
    Music_Loops = loops;
    g_s.armed = saved_s.armed;
    std::memcpy(g_s.root, saved_s.root, sizeof g_s.root);
    g_s.dirs = saved_s.dirs;
    g_s.source = saved_s.source;
    g_s.stream_dirs = saved_s.stream_dirs;
    g_s.exists = saved_s.exists;
    g_s.song_file = saved_s.song_file;
    g_s.song_size = saved_s.song_size;
    g_s.track = saved_s.track;
    g_s.active = saved_s.active;
    g_s.synth_bank.clear();  // the synth holds the test's bank: the game's first song loads its own
    g_s.bank->name.clear();
    g_s.bank_path.clear();
    g_s.song_from = saved_s.song_from;
    g_files = nullptr;
    g_names = nullptr;
    g_capture = nullptr;
    g_levels = nullptr;

    if (!load7 || !start7 || !same7 || !volume || !load8 || !end8 || !to_big || !to_small || !fallback || !mp3 ||
        !released || !resume_mid || !resume_past || !fold)
        bof3::Fatal("DIV-0092 resume (sequenced): at sample %u %s, past a once-only end %s, fold %s (%d jumps, period "
                    "%llu); DIV-0087 self-test: looping song load %s start %s samples %s; volume %s; once-only load %s end %s "
                    "(cut at %d of %d); to a larger bank %s, back to a smaller %s; missing track %s; mp3 switch %s; "
                    "release %s",
                    mid, resume_mid ? "ok" : "WRONG", resume_past ? "ok" : "WRONG", fold ? "ok" : "WRONG", jumps,
                    static_cast<unsigned long long>(period), load7 ? "ok" : "WRONG", start7 ? "ok" : "WRONG", same7 ? "ok" : "WRONG", volume ? "ok" : "WRONG",
                    load8 ? "ok" : "WRONG", end8 ? "ok" : "WRONG", cut, frames8, to_big ? "ok" : "WRONG",
                    to_small ? "ok" : "WRONG", fallback ? "ok" : "WRONG", mp3 ? "ok" : "WRONG", released ? "ok" : "WRONG");
    if (!layer_song || !layer_name_twin || !layer_under)
        bof3::Fatal("DIV-0109 self-test: a layer's song and bank %s; a base song on a bank named as the layer's %s; a base "
                    "song under the layer %s",
                    layer_song ? "ok" : "WRONG", layer_name_twin ? "ok" : "WRONG", layer_under ? "ok" : "WRONG");
    bof3::Log("shadow      DIV-0109 self-test: a named layer's song and its own bank played before base\\bgm's, equal to "
              "the oracle's; a base song on a bank bearing the layer bank's name read from base again; a song only "
              "base has from base");
    if (!pc_first || !psp_first || !stream_cache || !stream_install || !stream_none || !stream_pc || !stream_legacy)
        bof3::Fatal("DIV-0150 self-test: the PC's MP3s tried first %s; the PSP's song first %s; a stream from the cache "
                    "%s, from the install %s, in neither %s, the install first %s, no source chosen %s",
                    pc_first ? "ok" : "WRONG", psp_first ? "ok" : "WRONG", stream_cache ? "ok" : "WRONG",
                    stream_install ? "ok" : "WRONG", stream_none ? "ok" : "WRONG", stream_pc ? "ok" : "WRONG",
                    stream_legacy ? "ok" : "WRONG");
    bof3::Log("shadow      DIV-0150 self-test: the PC's MP3 names opened before the cache's song; the PSP's song before "
              "the PlayStation's; the streams from the first place that has the file");
    bof3::Log("shadow      DIV-0087 self-test: a synthetic looping song through Music_LoadFile / Music_Start / "
              "Music_Decode, %d samples equal to one MusicSynth::Render; the buffer volume by libsnd's law; a once-only "
              "song ended at sample %d with Music_Finished and zeros after, its bank kept; a song on a two-program "
              "bank, then one on the one-program bank, each equal to the oracle's; the MP3 names for a missing track "
              "and under BOF3X_MUSIC=mp3",
              frames7, cut);
    bof3::Log("shadow      DIV-0092 resume self-test (sequenced path): a looping song pending at sample %u fast-forwarded "
              "by Music_Start, its next %u samples equal to the oracle's render from the top; a once-only song pending "
              "past its end run to it and started again from the top; a point %llu samples on folded to %u (%d loop jumps, period %llu samples)",
              mid, 3 * kHalf / 4, static_cast<unsigned long long>(far_point), folded, jumps,
              static_cast<unsigned long long>(period));
}

}  // namespace music_seq

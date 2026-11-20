// The Jukebox (launcher.rc IDD_MUSIC; docs/music-player.md): the game's songs
// and recorded streams from each source the install has, one at a time.
// music_player.cpp's lists, names and dialog procedure, less the windows; the
// sound is jukebox_play.cpp's. The texts are the Win32 window's, word for word.
//
// Three sources, each a radio button greyed when the install lacks it (the
// Config screen's rule): the PlayStation disc's songs in the cache's
// base\bgm\, played by the engine's sequencer; the PSP disc's, from a cache
// built from a PSP disc alone (base\bgm\ again, the manifest's bgm rows name
// the build) or from the psp-music layer's opt\psp-music\bgm\ beside a
// PlayStation disc's (DIV-0109); and the PC port's MP3s in BGM\, looped at
// DIV-0081's measured points where the table has a row, else rewound to the
// file's start. A song number is the game's, so a list holds each song once.
// After the songs, each source lists the game's 11 recorded streams, each the
// file the game's lookup lands on with that source's version in play
// (DIV-0089, DIV-0110, DIV-0129, DIV-0150, DIV-0106, DIV-0215); a stream a
// source lacks is listed grey with its reason, and can still be chosen: Play
// then says why it does not play. Songs loop until stopped (the owner's call,
// 2026-10-11); a song that plays once in the game plays once here.
//
// Shown in place of the title page, from its Jukebox button, as the Sources
// page is (the Win32 launcher shows a window of its own); Close, Escape and
// the pad's circle (the Win32 window's IDCANCEL) stop the sound and go back.
// The window fits the page area as launcher.rc lays it out (320 x 186 dialog
// units against about 350 x 214): every place is launcher.rc's less the
// window's margin of 7 units. The source note and the status are areas of
// launcher.rc's size that scroll where the Win32 labels cut a long text.
//
// DIV-0113: the battle-chord box, over the settings' opt= as read. The
// player's choice goes into Pages::chord (-1 untouched, 0 off, 1 on), which
// the Game Options page's Apply writes with the suites on Play and on Back
// (SuitesOptEdit), as the Win32 window's DialogState::chord: a box ticked and
// unticked again writes the line back byte for byte.
#include "launcher/ui/pages.h"

#include "audio/song.h"
#include "launcher/start.h"
#include "launcher/ui/jukebox_play.h"
#include "launcher/ui/ui.h"

#include "os/wide_file.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>

namespace bof3x::ui {

namespace {

enum Source { kPsx, kPsp, kPc, kSources };
const char* const kSourceLabel[kSources] = {"PlayStation disc", "PSP disc", "PC port"};

struct Entry {
    unsigned number = 0;    // the song number, or the stream's id (Sound_LoadStream kind 0)
    bool stream = false;
    std::wstring why;       // a stream the source lacks: why (it is listed grey); empty when it plays
    std::wstring path;      // the song file
    std::wstring bank_dir;  // its bank\ (the cache's songs): the directory's own, base\bgm or a layer's
    std::wstring where;     // the disc's own name for it
    bool loops = false;
};

struct State {
    std::vector<Entry> songs[kSources];
    std::wstring why[kSources];        // why a source is grey
    std::vector<std::string> names;    // music-names.tsv: index = song number
    std::vector<bool> hidden;          // music-names.tsv rows marked same-notes
    std::vector<std::string> stream_names;   // music-names.tsv rows keyed by a stream's name: index = its id
    ChordState chord;
    bool have[kSources] = {};          // a source with a song or a stream it plays: its radio button usable
    int first = -1;                    // the first such source
    int source = -1;
    std::vector<std::string> items;    // the song box's entries for `source`
    std::vector<bool> grey;
    int chosen = 0;
    std::string note, status;
    bool open = false;                 // the sound device opened
    bool playing = false;
    bool focus = false;                // the frame the page opened: Close takes the focus
};
State g_s;
bool g_shown = false;   // between JukeboxBegin and its Close (JukeboxText says it)

// ---- the sources and their song lists ------------------------------------------

// NNN.DAT, NNNN.DAT (the PC's once-only files): the song number, or -1.
int SongNumber(const wchar_t* name, bool* once) {
    if (!(name[0] >= L'0' && name[0] <= L'9' && name[1] >= L'0' && name[1] <= L'9' && name[2] >= L'0' &&
          name[2] <= L'9'))
        return -1;
    const wchar_t* rest = name + 3;
    *once = *rest == L'N' || *rest == L'n';
    if (*once) ++rest;
    // lstrcmpiW(rest, L".DAT"): ASCII case not counted
    const wchar_t* dat = L".DAT";
    for (; *rest && *dat; ++rest, ++dat)
        if ((*rest >= L'a' && *rest <= L'z' ? *rest - L'a' + L'A' : *rest) != *dat) return -1;
    if (*rest || *dat) return -1;
    return (name[0] - L'0') * 100 + (name[1] - L'0') * 10 + (name[2] - L'0');
}

// Each file (not a folder) of `dir`, in the system's order (FindFirstFileW's on Windows).
template <typename F>
void EachFile(const std::wstring& dir, F f) {
    for (const bof3::os::MatchW& m : bof3::os::MatchesW(dir + L"*"))
        if (!m.directory) f(m.name.c_str());
}

void Sort(std::vector<Entry>& v) {
    for (std::size_t i = 1; i < v.size(); ++i)
        for (std::size_t j = i; j > 0 && v[j].number < v[j - 1].number; --j) std::swap(v[j], v[j - 1]);
}

// The build the cache's base\bgm came from: the build id that opens the source
// of the manifest's first `bgm` row, or "" when the manifest has no such row
// (common/cache_manifest.h). A PSP build's songs are tools/ppms.py's
// (docs/psp-music.md); any other's are a PlayStation disc's (tools/seq.py).
std::string CacheBgmBuild(const std::wstring& root) { return ManifestRead(root).bgm_build; }

bool IsPspBuild(const std::string& build) { return build.compare(0, 4, "psp-") == 0; }

// The songs of one bgm directory (the cache's base\bgm\, or a layer's
// opt\<layer>\bgm\) into `source`'s list, each with that directory's bank\.
void ListCacheSongs(State& s, const std::wstring& dir, int source) {
    EachFile(dir, [&](const wchar_t* name) {
        bool once = false;
        const int n = SongNumber(name, &once);
        if (n < 0 || once) return;
        Entry e;
        e.number = static_cast<unsigned>(n);
        e.path = dir + name;
        e.bank_dir = dir + L"bank\\";
        const std::vector<std::uint8_t> bytes = JukeReadWhole(e.path);
        JukeReading(e.path);
        psx::Song song;
        psx::LoadSong(bytes.data(), bytes.size(), &song);
        JukeReading(std::wstring());
        if (song.number != e.number) JukeFatal(e.path + L" holds another song's number");
        e.loops = (song.flags & 1) != 0;
        wchar_t where[64];
        std::swprintf(where, 64, L"%hs.EMI sub %u", song.bank.c_str(), song.sub);
        e.where = where;
        s.songs[source].push_back(e);
    });
}

void ListPsx(State& s, const std::wstring& root) {
    if (root.empty()) {
        s.why[kPsx] = L"No cache folder is set. The importer puts the disc songs into a cache folder.";
        return;
    }
    if (IsPspBuild(CacheBgmBuild(root))) {
        s.why[kPsx] = L"The cache has the PSP songs. For these songs, build the cache with a PlayStation disc.";
        return;
    }
    ListCacheSongs(s, root + L"\\base\\bgm\\", kPsx);
    if (s.songs[kPsx].empty())
        s.why[kPsx] = L"The cache folder has no songs (base\\bgm\\). Add a PlayStation disc on Data, Sources.";
}

void ListPsp(State& s, const std::wstring& root) {
    // docs/psp-music.md: a cache built from a PSP disc and no PlayStation disc
    // holds the PSP's songs and banks in base\bgm, in the PlayStation's form,
    // so the same sequencer plays them. A cache holds one base\bgm: with a
    // PlayStation disc among the sources it is the disc's.
    if (root.empty()) {
        s.why[kPsp] = L"No cache folder is set. The importer puts the PSP disc songs into a cache folder.";
        return;
    }
    if (!IsPspBuild(CacheBgmBuild(root))) {
        // DIV-0109: beside a PlayStation disc's songs, the PSP's are the music
        // layer's tree (recipes/psp-layers.toml's bgm layer), the same form,
        // played from its own bank\. The layer only when the manifest lists it
        // (or there is none), as the game takes it (common/cache_manifest.h).
        const char* name = nullptr;
        for (const suites_gen::LayerPart& l : suites_gen::kLayerParts)
            if (!name && std::strcmp(l.part, "bgm") == 0) name = l.layer;
        std::wstring layer;
        for (const char* c = name ? name : ""; *c; ++c) layer += static_cast<wchar_t>(*c);
        if (name && ManifestOffersLayer(ManifestRead(root), name))
            ListCacheSongs(s, root + L"\\opt\\" + layer + L"\\bgm\\", kPsp);
        if (s.songs[kPsp].empty())
            s.why[kPsp] = L"The cache does not have the PSP songs. Build it from a PSP disc only, "
                          L"or with --opt " + layer + L" and a PlayStation disc.";
        return;
    }
    ListCacheSongs(s, root + L"\\base\\bgm\\", kPsp);
    if (s.songs[kPsp].empty()) s.why[kPsp] = L"The cache manifest lists the PSP songs, but base\\bgm\\ has no songs.";
}

void ListPc(State& s, const std::wstring& game_dir) {
    const std::wstring dir = game_dir + L"\\BGM\\";
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

// ---- the recorded streams --------------------------------------------------------

// Sound_LoadStream's kind-0 names, by id: kind 0's list in the name tables at
// 0x6653B0, each opened as "SND\%s.DAT" (music_player.cpp's kStreams, the same
// 11). Names only (CLAUDE.md rule 1). Each plays once (Music_Start's 0).
constexpr const char* kStreams[] = {
    "OYASUMI", "AKURYOU", "ITEM01", "NAKAMA02", "MIMI01", "WIN", "SIPPAI01", "GAAN01", "PURE", "DRAGON", "KARA",
};
constexpr unsigned kStreamCount = sizeof kStreams / sizeof kStreams[0];

// The psp-snd layer's name: the audio suite's snd member (recipes/suites.toml).
std::wstring PspSndLayer() {
    for (const suites_gen::MemberInfo& m : suites_gen::kMembers)
        if (m.kind == suites_gen::Kind::kSnd && m.layer) return std::wstring(m.layer, m.layer + std::strlen(m.layer));
    return std::wstring();
}

// The 11 streams into each source's list, after its songs. Where the game
// would open each (save_menu.cpp Sound_LoadStream, dat_cache.cpp SoundPath):
// with the cache armed, the opt= layers' snd\ (the last first), then
// base\snd\, then the game folder's SND\. A source here is one of those
// places, as the game reaches it with that source's version in play.
void ListStreams(State& s, const std::wstring& root, const std::wstring& game_dir) {
    const CacheManifest m = ManifestRead(root);
    const std::wstring layer = PspSndLayer();
    const bool psp_base = IsPspBuild(m.snd_build);
    const bool psx_jingles = !m.snd_jingles_build.empty() && !IsPspBuild(m.snd_jingles_build);
    std::wstring jingle_dir = L"base\\snd";   // DIV-0150: the manifest's [snd_jingles] dir
    if (!m.snd_jingles_dir.empty()) {
        jingle_dir.clear();
        for (char c : m.snd_jingles_dir) jingle_dir += c == '/' ? L'\\' : static_cast<wchar_t>(c);
    }
    const bool layer_offered =
        !root.empty() && !layer.empty() && ManifestOffersLayer(m, std::string(layer.begin(), layer.end()));
    for (unsigned id = 0; id < kStreamCount; ++id) {
        const std::wstring file = ConfigFromUtf8(kStreams[id]) + L".DAT";
        for (int source = 0; source < kSources; ++source) {
            Entry e;
            e.number = id;
            e.stream = true;
            e.loops = false;
            if (source == kPc) {
                e.path = game_dir + L"\\SND\\" + file;
                e.where = L"SND\\" + file;
                if (!bof3::os::IsFileW(e.path)) e.why = L"The game folder has no SND\\" + file + L".";
            } else if (root.empty()) {
                e.where = file;
                e.why = L"No cache folder is set. The importer puts the disc streams into a cache folder.";
            } else if (source == kPsx) {
                // DIV-0129, DIV-0150: the disc's S_XA00.STR clips, in base\snd on a
                // PSX-only cache and base\psx-jingles beside a PC install
                e.path = root + L"\\" + jingle_dir + L"\\" + file;
                e.where = jingle_dir + L"\\" + file + L", from S_XA00.STR";
                if (!psx_jingles)
                    e.why = L"The cache has no disc streams. It is from an older release. Build the install again.";
                else if (!bof3::os::IsFileW(e.path))
                    e.why = L"The cache's " + jingle_dir + L"\\ has no " + file + L".";
            } else if (psp_base) {
                // DIV-0106: a PSP-only cache's base\snd is the PSP's
                e.path = root + L"\\base\\snd\\" + file;
                e.where = L"base\\snd\\" + file + L", the PSP's";
                if (!bof3::os::IsFileW(e.path)) e.why = L"The cache's base\\snd\\ (the PSP's) has no " + file + L".";
            } else {
                // DIV-0110: beside another base, the psp-snd layer's
                e.path = root + L"\\opt\\" + layer + L"\\snd\\" + file;
                e.where = L"opt\\" + layer + L"\\snd\\" + file;
                if (!layer_offered || !bof3::os::IsFileW(e.path))
                    e.why = L"The cache has no " + layer + L" layer holding " + file +
                            L". Build the cache from a PSP disc only, or with --opt " + layer + L".";
            }
            s.songs[source].push_back(e);
        }
    }
}

// <launcher folder>/music-names.tsv, copied there from data/music-names.tsv
// by the build: the catalogue of names (docs/music-player.md section 4).
// UTF-8, tab-separated, # comments; the columns are
//   song  name  box_disc  box_track  box_title  source  note
// and a song shows its box title where it has one, else its name; a song
// whose source is same-notes is not listed (the owner's mark, 2026-10-12, for
// one too like another). A line whose first field is not a three-digit song
// number or a stream's name (kStreams) is skipped. A stream's `name` is its
// file's, so it shows its box title where it has one, else the stream's name.
void LoadNames(State& s) {
    const std::wstring path = StartOwnDirectory() + StartSep(L"\\music-names.tsv");
    if (!bof3::os::IsFileW(path)) return;
    const std::vector<std::uint8_t> bytes = JukeReadWhole(path);
    std::string text(bytes.begin(), bytes.end());
    if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF) text.erase(0, 3);   // a BOM
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
        int stream = -1;
        for (unsigned id = 0; id < kStreamCount; ++id)
            if (song == kStreams[id]) stream = static_cast<int>(id);
        if (stream < 0 && (song.size() != 3 || song.find_first_not_of("0123456789") != std::string::npos)) continue;
        const unsigned number = stream >= 0 ? 0u : static_cast<unsigned>(std::strtoul(song.c_str(), nullptr, 10));
        // the owner's mark for a song too like another to list (2026-10-12)
        if (stream < 0 && field.size() > 5 && field[5] == "same-notes") {
            if (number >= s.hidden.size()) s.hidden.resize(number + 1);
            s.hidden[number] = true;
        }
        std::string name;
        if (field.size() > 4 && !field[4].empty()) name = field[4];   // the box's title
        else if (field.size() > 1 && stream < 0) name = field[1];     // a stream's name is its file's
        const std::size_t b = name.find_first_not_of(' '), e = name.find_last_not_of(' ');
        if (b == std::string::npos) continue;
        name = name.substr(b, e - b + 1);
        if (stream >= 0) {
            s.stream_names.resize(kStreamCount);
            s.stream_names[static_cast<unsigned>(stream)] = name;
            continue;
        }
        if (number >= s.names.size()) s.names.resize(number + 1);
        s.names[number] = name;
    }
}

// ---- the window ----------------------------------------------------------------

// An entry's key across the sources' lists: the song number, or 4096 + a
// stream's id, so a choice survives a change of source.
unsigned KeyOf(const Entry& e) { return e.stream ? 4096u + e.number : e.number; }

// The song box's entries for the source chosen; a stream the source lacks is
// grey (the Win32 box is owner-drawn for that) and can still be chosen.
void FillSongs(State& s, unsigned keep) {
    s.items.clear();
    s.grey.clear();
    int select = 0;
    const std::vector<Entry>& list = s.songs[s.source];
    for (std::size_t i = 0; i < list.size(); ++i) {
        const Entry& e = list[i];
        const std::string where = ConfigToUtf8(e.where);
        char label[512];
        if (e.stream) {
            const bool named = e.number < s.stream_names.size() && !s.stream_names[e.number].empty();
            std::snprintf(label, sizeof label, "stream %u   %s   (%s%s)", e.number,
                          named ? s.stream_names[e.number].c_str() : kStreams[e.number], where.c_str(),
                          e.why.empty() ? ", plays once" : ", not here");
        } else {
            const bool named = e.number < s.names.size() && !s.names[e.number].empty();
            std::snprintf(label, sizeof label, "%03u%s%s   (%s%s)", e.number, named ? "   " : "",
                          named ? s.names[e.number].c_str() : "", where.c_str(), e.loops ? "" : ", plays once");
        }
        s.items.push_back(label);
        s.grey.push_back(!e.why.empty());
        if (KeyOf(e) == keep) select = static_cast<int>(i);
    }
    s.chosen = select;
}

const Entry* Chosen(const State& s) {
    if (s.source < 0 || s.chosen < 0 || static_cast<std::size_t>(s.chosen) >= s.songs[s.source].size()) return nullptr;
    return &s.songs[s.source][static_cast<std::size_t>(s.chosen)];
}

void Play(State& s) {
    const Entry* e = Chosen(s);
    if (!e || !s.open) return;
    char text[1024];
    if (!e->why.empty()) {
        // grey: nothing to play; a song that was playing stops
        JukeOutSet(nullptr);
        s.playing = false;
        std::snprintf(text, sizeof text, "%s is not in the %s source: %s", kStreams[e->number], kSourceLabel[s.source],
                      ConfigToUtf8(e->why).c_str());
        s.status = text;
        return;
    }
    std::unique_ptr<JukeStream> stream;
    const char* how = "";
    if (e->stream) {
        // every source's stream is an MP3 in the PC's form (DIV-0106, DIV-0129)
        // or a disc jingle's XA clip (DIV-0215); kind 0 plays once
        // (Sound_LoadStream's Music_Start(..., 0))
        stream = JukeMp3(e->path, 0, false, nullptr);
        how = "once, as in the game";
    } else if (s.source == kPsx || s.source == kPsp) {
        // both are the cache's songs in the PlayStation's form (docs/psp-music.md)
        stream = JukeSeq(e->path, e->bank_dir);   // the song's own directory's bank (DIV-0109)
        how = e->loops ? "looping as on the disc" : "once, as in the game";
    } else {
        bool measured = false;
        stream = JukeMp3(e->path, e->number, e->loops, &measured);
        how = !e->loops ? "once, as in the game"
              : measured ? "looping at its measured points (DIV-0081)"
                         : "looping from the start, as in the PC port";
    }
    JukeOutSet(std::move(stream));
    s.playing = true;
    if (e->stream)
        std::snprintf(text, sizeof text, "Playing stream %u (%s) from the %s: %s.", e->number, kStreams[e->number],
                      kSourceLabel[s.source], how);
    else
        std::snprintf(text, sizeof text, "Playing %03u from the %s: %s.", e->number, kSourceLabel[s.source], how);
    s.status = text;
}

void Stop(State& s) {
    JukeOutSet(nullptr);
    s.playing = false;
    s.status = "Stopped.";
}

void ChooseSource(State& s, int source) {
    const Entry* was = Chosen(s);
    const unsigned keep = was ? KeyOf(*was) : 0;
    s.source = source;
    FillSongs(s, keep);
    if (s.playing) Play(s);
}

void Close(Pages& p) {
    JukeOutClose();
    g_s.playing = false;
    g_s.open = false;
    g_shown = false;
    p.jukebox_open = false;
}

// launcher.rc's dialog units, less the window's margin of 7.
constexpr float X(float units) { return Dx(units - 7); }
constexpr float Y(float units) { return Dy(units - 7); }

}  // namespace

void JukeboxBegin(Pages& p) {
    JukeArmAbort();
    g_s = State{};
    State& s = g_s;
    const std::wstring root = PagesCacheRoot(p), data = PagesDataRoot(p);
    // The settings' opt= as read (MusicPlayerRun's `opt`), and the choice made
    // the last time the Jukebox was open.
    s.chord = ChordEvaluate(ManifestRead(root), ConfigOptInstalled(p.game_dir, kChordLayer, data), p.cfg->opt,
                            ConfigOptDefaultInstalled(p.game_dir, data));
    if (p.chord >= 0 && s.chord.enabled) s.chord.checked = p.chord == 1;
    ListPsx(s, root);
    ListPsp(s, root);
    ListPc(s, p.game_dir);
    for (std::vector<Entry>& v : s.songs) Sort(v);
    LoadNames(s);
    for (std::vector<Entry>& v : s.songs) {
        std::size_t kept = 0;
        for (const Entry& e : v)
            if (!(e.number < s.hidden.size() && s.hidden[e.number])) v[kept++] = e;
        v.resize(kept);
    }
    ListStreams(s, root, p.game_dir);   // after the songs, and never hidden

    // WM_INITDIALOG
    for (int i = 0; i < kSources; ++i) {
        // a source with a song or a stream it plays
        bool have = false, songs = false;
        for (const Entry& e : s.songs[i]) {
            have = have || e.why.empty();
            songs = songs || !e.stream;
        }
        s.have[i] = have;
        if (have && s.first < 0) s.first = i;
        // a source listed for its streams alone says why it has no songs
        if (!have) s.note += std::string(kSourceLabel[i]) + ": " + ConfigToUtf8(s.why[i]) + "\n";
        else if (!songs) s.note += std::string(kSourceLabel[i]) + ", songs: " + ConfigToUtf8(s.why[i]) + "\n";
    }
    s.open = JukeOutOpen();
    if (s.first < 0) {
        s.status = "No source has songs.";
    } else {
        ChooseSource(s, s.first);
        s.status = s.open ? "Stopped." : "No sound device could be opened.";
    }
    s.focus = true;
    g_shown = true;
    p.jukebox_open = true;
}

void JukeboxPage(Pages& p) {
    State& s = g_s;
    // the Win32 window's timer: a song that plays once has ended
    if (s.playing && JukeOutEnded()) {
        s.playing = false;
        s.status = "Ended (a song that plays once).";
    }

    GroupBegin("Source", X(7), Y(7), Dx(306), Dy(64));
    const float radio_x[kSources] = {14, 110, 206};
    for (int i = 0; i < kSources; ++i) {
        int source = s.source;
        Place(Dx(radio_x[i] - 7), Dy(20 - 7));
        Size(Dx(90));
        if (!s.have[i]) Grey(ConfigToUtf8(s.why[i]).c_str());
        if (Radio(kSourceLabel[i], &source, i) && source != s.source) ChooseSource(s, source);
    }
    AreaBegin("##note", Dx(14 - 7), Dy(34 - 7), Dx(296), Dy(34));
    Place(0, 0);
    Size(Dx(296));
    Label(s.note.c_str());
    AreaEnd();
    GroupEnd();

    Place(X(7), Y(80));
    Label("Song:");
    Place(X(40), Y(78));
    Size(Dx(273));
    if (ComboGreyText("##song", &s.chosen, s.items, s.grey) && s.playing) Play(s);
    AreaBegin("##status", X(7), Y(98), Dx(306), Dy(18));
    Place(0, 0);
    Size(Dx(306));
    Label(s.status.c_str());
    AreaEnd();

    GroupBegin("In the game (a sound effect, not a song)", X(7), Y(118), Dx(306), Dy(42));
    Place(Dx(14 - 7), Dy(130 - 118));
    Size(Dx(290));
    if (!s.chord.enabled) Grey(ConfigToUtf8(s.chord.why).c_str());
    bool on = s.chord.checked;
    if (Check("Battle chord: PSP version (one tone)##chord", &on) && s.chord.enabled) {
        // a choice, not an edit: opt= is made once on Play from the line as
        // read (SuitesOptEdit), so ticking and unticking writes it back byte
        // for byte
        p.chord = on ? 1 : 0;
        s.chord.checked = on;
    }
    Place(Dx(14 - 7), Dy(142 - 118));
    Size(Dx(296));
    Label(s.chord.enabled ? "Off: two tones, as on the PlayStation. The launcher saves this when you click Play."
                          : ConfigToUtf8(s.chord.why).c_str());
    GroupEnd();

    const bool can_play = s.first >= 0 && s.open;
    Place(X(7), Y(165));
    Size(Dx(52), Dy(14));
    if (!can_play) Grey(s.first < 0 ? "No source has songs." : "No sound device could be opened.");
    if (Button("Play")) Play(s);
    Place(X(63), Y(165));
    Size(Dx(52), Dy(14));
    if (!can_play) Grey(s.first < 0 ? "No source has songs." : "No sound device could be opened.");
    if (Button("Stop")) Stop(s);
    Place(X(261), Y(165));
    Size(Dx(52), Dy(14));
    const bool close = Button("Close");
    if (s.focus) DefaultFocus();   // the Win32 window's default button: Enter closes it
    s.focus = false;
    if (close || Pressed(Press::kBack)) Close(p);
}

std::string JukeboxText() {
    const State& s = g_s;
    std::string out = g_shown ? "the Jukebox is shown\n" : "the Jukebox is not shown (as it was last)\n";
    for (int i = 0; i < kSources; ++i)
        out += std::string("source ") + kSourceLabel[i] + ": " + (s.have[i] ? "usable" : "grey") +
               (i == s.source ? ", chosen" : "") + (s.why[i].empty() ? "" : " (" + ConfigToUtf8(s.why[i]) + ")") + "\n";
    std::size_t from = 0;
    while (from < s.note.size()) {
        std::size_t end = s.note.find('\n', from);
        if (end == std::string::npos) end = s.note.size();
        out += "note: " + s.note.substr(from, end - from) + "\n";
        from = end + 1;
    }
    for (std::size_t i = 0; i < s.items.size(); ++i) {
        out += "song " + std::to_string(i) + ": " + s.items[i] + (s.grey[i] ? "   [grey]" : "") +
               (static_cast<int>(i) == s.chosen ? "   [chosen]" : "") + "\n";
        if (s.grey[i]) out += "  why: " + ConfigToUtf8(s.songs[s.source][i].why) + "\n";
    }
    out += "status: " + s.status + "\n";
    out += std::string("battle chord: ") + (s.chord.checked ? "on" : "off") +
           (s.chord.enabled ? "" : ", grey (" + ConfigToUtf8(s.chord.why) + ")") + "\n";
    out += std::string("play and stop: ") + (s.first >= 0 && s.open ? "usable" : "grey") +
           "; sound device: " + (s.open ? "open" : "not open") + "; playing: " + (s.playing ? "yes" : "no") + "\n";
    out += "samples written: " + std::to_string(JukeOutSamples()) + " frames\n";
    return out;
}

void JukeboxCheck(const std::string& samples) { JukeOutCheck(samples); }

}  // namespace bof3x::ui

// Music: the song source (launcher.rc IDD_PAGE_MUSIC; DIV-0150).
// config_dialog.cpp's MusicRefresh, kMusicSources and the Music box's part of
// ReadBack, less the windows. The texts are the Win32 page's, word for word.
//
// The source the game plays first (music=, ConfigMusicSource), its index the
// box's: the PlayStation's, the PSP's, the PC's MP3s. Every source is listed
// (the Config screen's rule); one this install lacks says so, as the next in
// its order then plays (music_seq.cpp Arm). The box is grey when the preset
// sets music= when the game starts; the player's choice stays in bof3x.ini.
#include "launcher/ui/pages.h"

#include "launcher/start.h"
#include "launcher/ui/ui.h"
#include "os/wide_file.h"

namespace bof3x::ui {

namespace {

const char* const kMusicSources[] = {"psx", "psp", "pc"};

int MusicIndex(const std::string& source) {
    for (int i = 0; i < 3; ++i)
        if (source == kMusicSources[i]) return i;
    return 0;
}

struct Boxes {
    int music = 0;                    // the box's entry
    std::vector<std::string> items;   // its entries as last made, at this stamp
    unsigned stamp = 0;
};
Boxes g_boxes;

// The box's entries for the cache as the Data tab's box names it; the entry
// chosen stays.
void MusicRefresh(Pages& p) {
    Boxes& b = g_boxes;
    if (b.stamp == p.stamp) return;
    b.stamp = p.stamp;
    const std::wstring cache = PagesCacheRoot(p), data = PagesDataRoot(p);
    const CacheManifest m = ManifestRead(cache);
    const bool psp_base = ManifestPspBase(m);
    const bool songs = !cache.empty() && bof3::os::IsDirectoryW(cache + StartSep(L"\\base\\bgm"));
    const bool mp3s = bof3::os::AnyMatchW(p.game_dir + StartSep(L"\\BGM\\*.DAT"));
    const bool have[3] = {songs && !psp_base,
                          psp_base ? songs : ConfigOptInstalled(p.game_dir, "psp-music", data, cache), mp3s};
    const char* const names[3] = {"PlayStation", "PSP", "PC (MP3)"};
    const char* const lacks[3] = {"PlayStation - not in the cache (plays PSP, then PC)",
                                  "PSP - not in the cache (plays PlayStation, then PC)",
                                  "PC (MP3) - no BGM\\ folder (plays PlayStation, then PSP)"};
    b.items.clear();
    for (int i = 0; i < 3; ++i) b.items.push_back(have[i] ? names[i] : lacks[i]);
}

}  // namespace

void MusicOpen(Pages& p) {
    g_boxes = Boxes{};
    g_boxes.music = MusicIndex(ConfigMusicSource(p.cfg->music, p.cfg->opt));
}

// The source music= already means keeps the line as written (an empty
// music=, seq or mp3 too); another writes its name. Read against opt= as it
// was read: PagesApply runs this before VersionsApply edits opt=.
void MusicApply(Pages& p) {
    Config& cfg = *p.cfg;
    const int music = g_boxes.music;
    if (music >= 0 && music < 3 && music != MusicIndex(ConfigMusicSource(cfg.music, cfg.opt)))
        cfg.music = kMusicSources[music];
}

void MusicPage(Pages& p) {
    MusicRefresh(p);
    GroupBegin("Music", 0, 0, Dx(276), Dy(56));
    Place(Dx(9), Dy(17));
    Label("Music:");
    Place(Dx(77), Dy(15));
    Size(Dx(186));
    if (PagesPresetSets(p, "music")) Grey(kPresetGreyReason);
    Combo("music", &g_boxes.music, g_boxes.items);
    Place(Dx(9), Dy(34));
    Size(Dx(260));
    Label("The game plays its music from this source. If this source does not have a song, the game plays that song "
          "from a different source.");
    GroupEnd();
}

}  // namespace bof3x::ui

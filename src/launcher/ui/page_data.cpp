// Data: the cache folder and the Sources page's button (launcher.rc
// IDD_PAGE_DATA; DIV-0087). The cache box's text is Pages::cache, since the
// suites, the music sources and the row above the text read the cache it
// names as it stands. The texts are the Win32 page's.
#include "launcher/ui/pages.h"

#include "common/home.h"
#include "launcher/ui/ui.h"
#include "os/file.h"
#include "os/text.h"

namespace bof3x::ui {

namespace {

constexpr int kAskCache = 1;   // this page's question to the folder picker

// What the game will make of the folder, said under the box: the DLL's own
// tests (music_seq.cpp Arm), so the note never promises what the game
// refuses. The 226 is its kRootMax: 259 less the longest path it builds
// (\base\bgm\bank\ + a 15-character bank name + .DAT), counted as the DLL
// counts it - in the UTF-8 bytes it receives.
const char* CacheNote(const Pages& p) {
    std::wstring w = bof3::os::ToWide(p.cache.c_str());
    const std::size_t b = w.find_first_not_of(L" \t"), e = w.find_last_not_of(L" \t");
    w = b == std::wstring::npos ? std::wstring() : w.substr(b, e - b + 1);   // ConfigLoad trims
    if (w.empty())
        return HomeCacheDefault().empty() ? "No cache folder: the game plays the PC MP3 music."
                                          : "Empty: the game uses the cache\\ folder of this install.";
    while (w.size() > 1 && (w.back() == L'\\' || w.back() == L'/')) w.pop_back();
    const std::string a = ConfigToUtf8(w);
    if (!bof3::os::IsDirectory(w.c_str())) return "This is not a folder. The game cannot start.";
    if (a.size() > 259 - (sizeof "\\base\\bgm\\bank\\" - 1 + 15 + sizeof ".DAT" - 1))
        return "The path is too long (more than 226 bytes as UTF-8). The game cannot start.";
    return bof3::os::IsDirectory((w + L"/base/bgm").c_str()) ? "Has base\\bgm: the game plays the disc music."
                                                              : "No base\\bgm: the game plays the PC MP3 music.";
}

}  // namespace

void DataOpen(Pages& p) { p.cache = p.cfg->cache; }

// The cache box as the ini's UTF-8 bytes, trimmed as ConfigLoad trims a line.
void DataApply(Pages& p) {
    const std::size_t b = p.cache.find_first_not_of(" \t"), e = p.cache.find_last_not_of(" \t");
    p.cfg->cache = b == std::string::npos ? std::string() : p.cache.substr(b, e - b + 1);
}

void DataPage(Pages& p) {
    GroupBegin("Cache folder", 0, 0, Dx(276), Dy(52));
    Place(Dx(9), Dy(17));
    Label("Cache folder:");
    Place(Dx(77), Dy(15));
    Size(Dx(132));
    if (Edit("cache", &p.cache)) ++p.stamp;   // another cache: what the pages made from it is made again
    Place(Dx(213), Dy(14));
    Size(Dx(50), 23);
    if (Button("Browse...")) PickFolder(kAskCache);
    if (std::string picked; Picked(kAskCache, &picked) && !picked.empty()) {
        p.cache = picked;
        ++p.stamp;
    }
    Place(Dx(77), Dy(32));
    Size(Dx(196));
    Label(CacheNote(p));
    GroupEnd();

    GroupBegin("Sources", 0, Dy(58), Dx(276), Dy(52));
    Place(Dx(9), Dy(13));
    Size(Dx(260));
    Label("Your copies of the game. The importer checks them and builds this install from them.");
    Place(Dx(9), Dy(32));
    Size(Dx(60), 23);
    Grey("The Sources page is not in this launcher yet. To change the sources, use bof3x-launcher.");
    Button("Sources...");
    GroupEnd();
}

}  // namespace bof3x::ui

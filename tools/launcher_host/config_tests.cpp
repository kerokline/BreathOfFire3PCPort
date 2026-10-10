// Host tests for the launcher's settings (src/launcher/config.cpp): the ini's
// round trip through ConfigLoad / ConfigSave, and ConfigOptEdit - what the
// settings dialog's "PSP extras" boxes make of an `opt=` line. No game data;
// a Windows host (config.cpp is Win32). docs/launcher-settings.md section 3.
//
//   cmake -S tools/launcher_host -B <scratch>/lh -G Ninja -DCMAKE_CXX_COMPILER=clang++
//   cmake --build <scratch>/lh && <scratch>/lh/config_tests.exe
#include "launcher/config.h"

#include <windows.h>

#include <cstdio>
#include <string>
#include <vector>

namespace {

int g_failures = 0, g_checks = 0;

void Check(bool ok, const char* what, const std::string& got = std::string()) {
    ++g_checks;
    if (!ok) {
        ++g_failures;
        std::printf("  FAIL %s%s%s\n", what, got.empty() ? "" : ": got ", got.c_str());
    }
}

std::wstring Temp(const wchar_t* name) {
    wchar_t dir[MAX_PATH];
    GetTempPathW(MAX_PATH, dir);
    return std::wstring(dir) + name;
}

std::string ReadAll(const std::wstring& path) {
    std::string out;
    if (FILE* f = _wfopen(path.c_str(), L"rb")) {
        char buf[4096];
        size_t n;
        while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) out.append(buf, n);
        std::fclose(f);
    }
    return out;
}

void WriteAll(const std::wstring& path, const std::string& text) {
    FILE* f = _wfopen(path.c_str(), L"wb");
    std::fwrite(text.data(), 1, text.size(), f);
    std::fclose(f);
}

void TestOptEdit() {
    using Boxes = std::vector<std::pair<std::string, bool>>;
    const std::vector<std::string> walls{"area4-walls"}, none;
    struct Case {
        const char* what;
        std::string opt;
        std::vector<std::string> defaults;
        Boxes boxes;
        std::string want;
    } cases[] = {
        {"untouched, empty", "", walls, {{"psp-art", false}, {"psp-maps", false}}, ""},
        {"untouched, a list kept byte for byte", "psp-tiles,psp-art", walls, {{"psp-art", true}, {"psp-tiles", true}},
         "psp-tiles,psp-art"},
        {"untouched, none", "none", walls, {{"psp-art", false}}, "none"},
        {"no box at all", "psp-art", walls, {}, "psp-art"},
        {"a box on over empty keeps the default walls", "", walls, {{"psp-art", true}}, "psp-art,area4-walls"},
        {"a box on over empty, no walls installed", "", none, {{"psp-art", true}}, "psp-art"},
        {"a box on over none: the walls stay off", "none", walls, {{"psp-art", true}}, "psp-art"},
        {"the last box off: none", "psp-art", walls, {{"psp-art", false}}, "none"},
        {"a box off, the walls listed stay", "area4-walls,psp-art", walls, {{"psp-art", false}}, "area4-walls"},
        {"kPspLayers' order, a name the boxes lack kept after", "foo,psp-maps", none,
         {{"psp-art", true}, {"psp-maps", true}}, "psp-art,psp-maps,foo"},
        {"a layer not offered (not installed) kept", "psp-names-ja-JP,psp-maps", none, {{"psp-maps", false}},
         "psp-names-ja-JP"},
        {"names layer on with the art", "psp-art", none, {{"psp-art", true}, {"psp-names-en-150", true}},
         "psp-art,psp-names-en-150"},
    };
    for (const Case& c : cases) {
        const std::string got = bof3x::ConfigOptEdit(c.opt, c.defaults, c.boxes);
        Check(got == c.want, c.what, got);
        std::string why;
        Check(bof3x::ConfigOptValid(got, why), "the result is a valid opt=", got + " " + why);
    }
}

// An ini as a player might hand-edit it: what ConfigLoad reads, ConfigSave
// writes back the same, and a second load and save is byte-identical.
void TestRoundTrip() {
    const std::wstring a = Temp(L"bof3x_config_tests_a.ini"), b = Temp(L"bof3x_config_tests_b.ini");
    WriteAll(a,
             "[bof3x]\r\n"
             "language=original\r\n"
             "opt=psp-art,area4-walls\r\n"
             "cache=  D:\\Games\\bof3 cache\\  \r\n"
             "music=seq\r\n"
             "filter=point\r\n");
    bof3x::Config c1;
    std::string err;
    Check(bof3x::ConfigLoad(a, c1, err) && err.empty(), "load the hand-edited ini");
    Check(c1.cache == "D:\\Games\\bof3 cache\\", "cache= trimmed as a line is, the path kept", c1.cache);
    Check(c1.music == "seq", "music=seq kept as seq", c1.music);
    Check(c1.opt == "psp-art,area4-walls", "opt= kept", c1.opt);
    Check(bof3x::ConfigSave(b, c1), "save");
    const std::string first = ReadAll(b);
    Check(first.find("\r\ncache=D:\\Games\\bof3 cache\\\r\n") != std::string::npos, "the cache= line written");
    Check(first.find("\r\nmusic=seq\r\n") != std::string::npos, "the music= line written");
    Check(first.find("\r\nopt=psp-art,area4-walls\r\n") != std::string::npos, "the opt= line written");
    bof3x::Config c2;
    Check(bof3x::ConfigLoad(b, c2, err) && err.empty(), "load the saved ini");
    Check(c2.cache == c1.cache && c2.music == c1.music && c2.opt == c1.opt, "the three survive a second load");
    Check(bof3x::ConfigSave(b, c2), "save again");
    Check(ReadAll(b) == first, "a second save is byte-identical");
    // An empty music= and cache= stay empty, and music= that is neither seq nor
    // mp3 is dropped to the default as before.
    WriteAll(a, "[bof3x]\r\nmusic=\r\ncache=\r\nopt=\r\n");
    bof3x::Config c3;
    Check(bof3x::ConfigLoad(a, c3, err) && c3.music.empty() && c3.cache.empty() && c3.opt.empty(), "empty lines stay empty");
    WriteAll(a, "[bof3x]\r\nmusic=MP3\r\n");
    bof3x::Config c4;
    Check(bof3x::ConfigLoad(a, c4, err) && c4.music.empty(), "music=MP3 (not a value) is the default", c4.music);
    DeleteFileW(a.c_str());
    DeleteFileW(b.c_str());
}

}  // namespace

int main() {
    std::printf("opt edit\n");
    TestOptEdit();
    std::printf("ini round trip\n");
    TestRoundTrip();
    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}

// Host tests for the launcher's settings (src/launcher/config.cpp, suites.cpp):
// the ini's round trip through ConfigLoad / ConfigSave, ConfigOptEdit, and the
// three suites - what the settings dialog makes of a cache's manifest.toml and
// of an `opt=` line (docs/launcher-suites.md). No game data; a Windows host
// (config.cpp is Win32). docs/launcher-settings.md section 3.
//
//   cmake -S tools/launcher_host -B <scratch>/lh -G Ninja -DCMAKE_CXX_COMPILER=clang++
//   cmake --build <scratch>/lh && <scratch>/lh/config_tests.exe
#include "common/home.h"
#include "launcher/config.h"
#include "launcher/suites.h"

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
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

// The installed-layer lookup the dialog's boxes and an empty `opt=` rest on
// (ConfigOptInstalled, ConfigOptDefaultInstalled, ConfigOptWanted): a game
// folder with DAT\<layer>.*.DAT, and a cache with opt\<layer>\dat\*.DAT (DIV-0089).
// A merge on 2026-10-10 lost one of the pattern's backslashes; dialog_test.py
// caught it and this keeps it caught on the host.
void TestInstalled() {
    const std::wstring game = Temp(L"bof3x_cfg_game"), cache = Temp(L"bof3x_cfg_cache");
    CreateDirectoryW(game.c_str(), nullptr);
    CreateDirectoryW((game + L"\\DAT").c_str(), nullptr);
    for (const wchar_t* part : {L"", L"\\opt", L"\\opt\\psp-tiles", L"\\opt\\psp-tiles\\dat"})
        CreateDirectoryW((cache + part).c_str(), nullptr);
    const std::wstring art = game + L"\\DAT\\psp-art.AREA067.DAT", walls = game + L"\\DAT\\area4-walls.AREA004.DAT",
                       tiles = cache + L"\\opt\\psp-tiles\\dat\\AREA000.DAT";
    DeleteFileW(art.c_str()); DeleteFileW(walls.c_str()); DeleteFileW(tiles.c_str());
    Check(!bof3x::ConfigOptInstalled(game, "psp-art"), "nothing installed: psp-art absent");
    Check(bof3x::ConfigOptDefaultInstalled(game).empty(), "nothing installed: no default layer");
    Check(bof3x::ConfigOptWanted(game, "").empty(), "nothing installed: empty opt= plays nothing");
    WriteAll(art, "");
    Check(bof3x::ConfigOptInstalled(game, "psp-art"), "DAT\\psp-art.*.DAT: installed");
    Check(!bof3x::ConfigOptInstalled(game, "psp-tiles"), "psp-tiles not installed, no cache");
    WriteAll(tiles, "");
    Check(!bof3x::ConfigOptInstalled(game, "psp-tiles"), "psp-tiles in a cache not named: not installed");
    Check(bof3x::ConfigOptInstalled(game, "psp-tiles", cache), "psp-tiles in the cache's opt: installed (DIV-0089)");
    WriteAll(walls, "");
    const std::vector<std::string> def = bof3x::ConfigOptDefaultInstalled(game);
    Check(def.size() == 1 && def[0] == "area4-walls", "area4-walls installed: the default list");
    Check(bof3x::ConfigOptWanted(game, "") == "area4-walls", "empty opt= plays area4-walls", bof3x::ConfigOptWanted(game, ""));
    Check(bof3x::ConfigOptWanted(game, "none").empty(), "opt=none plays nothing");
    DeleteFileW(art.c_str()); DeleteFileW(walls.c_str()); DeleteFileW(tiles.c_str());
    // TG: a layer of songs, sound files or .data ranges counts as the DLL counts it (dat_cache::HasLayerPart)
    struct Part { const char* layer; const wchar_t* sub; const wchar_t* file; } parts[] = {
        {"psp-music", L"bgm", L"000.DAT"}, {"psp-snd", L"snd", L"ABC.DAT"}, {"psp-tables", L"exe", L"place.toml"}};
    for (const Part& pt : parts) {
        std::wstring dir = cache + L"\\opt\\";
        for (const char* c = pt.layer; *c; ++c) dir += static_cast<wchar_t>(*c);
        CreateDirectoryW(dir.c_str(), nullptr);
        dir += std::wstring(L"\\") + pt.sub;
        CreateDirectoryW(dir.c_str(), nullptr);
        Check(!bof3x::ConfigOptInstalled(game, pt.layer, cache), (std::string(pt.layer) + ": an empty part is no layer").c_str());
        WriteAll(dir + L"\\" + pt.file, "");
        Check(bof3x::ConfigOptInstalled(game, pt.layer, cache), (std::string(pt.layer) + ": the cache's part counts").c_str());
        DeleteFileW((dir + L"\\" + pt.file).c_str());
    }
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
        {"the suites' order, a name the boxes lack kept after", "foo,psp-maps", none,
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
             "psp=myria-item\r\n"
             "cache=  D:\\Games\\bof3 cache\\  \r\n"
             "music=seq\r\n"
             "filter=point\r\n");
    bof3x::Config c1;
    std::string err;
    Check(bof3x::ConfigLoad(a, c1, err) && err.empty(), "load the hand-edited ini");
    Check(c1.cache == "D:\\Games\\bof3 cache\\", "cache= trimmed as a line is, the path kept", c1.cache);
    Check(c1.music == "seq", "music=seq kept as seq", c1.music);
    Check(c1.opt == "psp-art,area4-walls", "opt= kept", c1.opt);
    Check(c1.psp == "myria-item", "psp= kept (DIV-0108)", c1.psp);
    Check(bof3x::ConfigSave(b, c1), "save");
    const std::string first = ReadAll(b);
    Check(first.find("\r\ncache=D:\\Games\\bof3 cache\\\r\n") != std::string::npos, "the cache= line written");
    Check(first.find("\r\nmusic=seq\r\n") != std::string::npos, "the music= line written");
    Check(first.find("\r\nopt=psp-art,area4-walls\r\n") != std::string::npos, "the opt= line written");
    Check(first.find("\r\npsp=myria-item\r\n") != std::string::npos, "the psp= line written");
    bof3x::Config c2;
    Check(bof3x::ConfigLoad(b, c2, err) && err.empty(), "load the saved ini");
    Check(c2.cache == c1.cache && c2.music == c1.music && c2.opt == c1.opt && c2.psp == c1.psp,
          "the four survive a second load");
    Check(bof3x::ConfigSave(b, c2), "save again");
    Check(ReadAll(b) == first, "a second save is byte-identical");
    // An empty music= and cache= stay empty, and music= that is neither seq nor
    // mp3 is dropped to the default as before.
    WriteAll(a, "[bof3x]\r\nmusic=\r\ncache=\r\nopt=\r\npsp=\r\n");
    bof3x::Config c3;
    Check(bof3x::ConfigLoad(a, c3, err) && c3.music.empty() && c3.cache.empty() && c3.opt.empty() && c3.psp.empty(),
          "empty lines stay empty");
    WriteAll(a, "[bof3x]\r\nmusic=MP3\r\n");
    bof3x::Config c4;
    Check(bof3x::ConfigLoad(a, c4, err) && c4.music.empty(), "music=MP3 (not a value) is the default", c4.music);
    DeleteFileW(a.c_str());
    DeleteFileW(b.c_str());
}

// The ini's cache= is bytes in the ANSI code page (the dialog's ToAnsi; the
// DLL reads BOF3X_CACHE with GetEnvironmentVariableA). The launcher's own
// lookups must widen it through the same code page - they widened it a byte a
// character, so a folder named outside Latin-1 was looked for elsewhere and
// its layers went unseen (docs/code-page-audit.md).
void TestCacheCodePage() {
    SetEnvironmentVariableW(L"BOF3X_CACHE", nullptr);
    SetEnvironmentVariableW(L"BOF3X_CACHE_DATA", nullptr);
    // A character this code page holds whose bytes are not its Latin-1 widening.
    std::wstring name;
    std::string bytes;
    for (const wchar_t* c : {L"€", L"中", L"Ж", L"あ"}) {
        const std::wstring w = std::wstring(L"bof3x_cfg_cp_") + c;
        BOOL lossy = FALSE;
        char a[64];
        const int n = WideCharToMultiByte(CP_ACP, 0, w.c_str(), -1, a, sizeof a, nullptr, &lossy);
        if (n <= 0 || lossy) continue;
        std::wstring latin1;
        for (const char* p = a; *p; ++p) latin1 += static_cast<wchar_t>(static_cast<unsigned char>(*p));
        if (latin1 == w) continue;
        name = w;
        bytes = a;
        break;
    }
    if (name.empty()) {
        std::printf("  (code page %u holds none of the candidates outside Latin-1: not checked)\n", GetACP());
        return;
    }
    wchar_t tmp[MAX_PATH];
    const DWORD tn = GetTempPathW(MAX_PATH, tmp);
    char atmp[MAX_PATH * 2];
    BOOL lossy = FALSE;
    if (tn == 0 || WideCharToMultiByte(CP_ACP, 0, tmp, -1, atmp, sizeof atmp, nullptr, &lossy) <= 0 || lossy) {
        std::printf("  (the temporary folder is outside code page %u: not checked)\n", GetACP());
        return;
    }
    const std::wstring game = Temp(L"bof3x_cfg_game"), cache = Temp(name.c_str());
    CreateDirectoryW(game.c_str(), nullptr);
    CreateDirectoryW((game + L"\\DAT").c_str(), nullptr);
    for (const wchar_t* part : {L"", L"\\opt", L"\\opt\\psp-tiles", L"\\opt\\psp-tiles\\dat"})
        CreateDirectoryW((cache + part).c_str(), nullptr);
    const std::wstring tiles = cache + L"\\opt\\psp-tiles\\dat\\AREA000.DAT";
    WriteAll(tiles, "");
    bof3x::Config cfg;
    // Through ConfigLoad's reading of the line (ConfigCacheFromIni): since state 3
    // Config::cache holds UTF-8, and an older ini's ANSI bytes are read once as
    // the code page. (Until 2026-10-12 this set the ANSI bytes straight into
    // Config::cache, which state 3 made UTF-8 - two stale failures since.)
    cfg.cache = bof3x::ConfigCacheFromIni(std::string(atmp) + bytes + "\\");
    Check(bof3x::ConfigCacheDataRoot(cfg) == cache, "cache= widened through the ANSI code page");
    Check(bof3x::ConfigOptInstalled(game, "psp-tiles", bof3x::ConfigCacheDataRoot(cfg)),
          "a layer in a cache named outside Latin-1: installed (DIV-0089)");
    DeleteFileW(tiles.c_str());
}

// ---- The three suites (src/launcher/suites.cpp, docs/launcher-suites.md) -----------
//
// Three fake manifests as tools/importer.py writes them - PSP-only, PSX-only,
// and PC + PSX + PSP - with the [build] keys the round's contract fixes
// (docs/psp-only-round.md section 4: base_policy from SI, exe_tables_from from
// IM). The [build] table sits right after [meta], as SI's
// psp_standins.manifest_lines writes it (psp/si 1d72d999, standins_sha256 under
// psp); exe_tables_from in it is IM's, not written yet. Builds, names and
// made-up hashes only.

const char kManifestPspOnly[] =
    "# The cache's provenance\n[meta]\ntarget = \"pc-zh\"\nbuilt = \"2026-10-12T00:00:00\"\n\n"
    "[build]\nbase_policy = \"psp\"\nstandins_sha256 = \"66\"\nexe_tables_from = \"psp-jp\"\n\n"
    "[[source]]\nbuild = \"psp-jp\"\npath = \"Breath of Fire III (PSP) (Japan).iso\"\n\n"
    "[cache]\nassets = [\n  [\"AREA000.DAT\", 0, \"base\", \"psp-jp:own:WORLD00/AREA000.EMI#7\", \"00\"],\n]\n"
    "bgm = [\n  [\"base/bgm/000.DAT\", \"psp-jp:ppms:SOUND/BGM.BIN#0\", \"11\"],\n]\n\n"
    "[snd]\nbuild = \"psp-jp\"\nhow = \"psp-at3\"\nffmpeg = \"ffmpeg version 9.0.1\"\nfiles = [\n  [\"MAGIC/000_00.WAV\", \"22\"],\n]\n\n"
    "[exe]\nbuild = \"psp-jp\"\nfile = \"base/exe/data.bin\"\nsha256 = \"33\"\n";
const char kManifestPsxOnly[] =
    "[meta]\ntarget = \"pc-zh\"\n\n[build]\nbase_policy = \"psx\"\nexe_tables_from = \"psx-us\"\n\n"
    "[[source]]\nbuild = \"psx-us\"\npath = \"Breath of Fire III (USA).cue\"\n\n"
    "[cache]\nassets = [\n  [\"AREA000.DAT\", 0, \"base\", \"psx-us:copy\", \"00\"],\n]\n"
    "bgm = [\n  [\"base/bgm/000.DAT\", \"psx-us:seq:BGM000.EMI#1\", \"11\"],\n]\n\n"
    "[exe]\nbuild = \"psx-us\"\nfile = \"base/exe/data.bin\"\nsha256 = \"33\"\n";
const char kManifestAll[] =
    "[meta]\ntarget = \"pc-zh\"\n\n[build]\nbase_policy = \"psx\"\nexe_tables_from = [\"pc-zh\"]\n\n"
    "[[source]]\nbuild = \"pc-zh\"\npath = \"DAT\"\n\n[[source]]\nbuild = \"pc-zh\"\npath = \"BOF3.exe\"\n\n"
    "[[source]]\nbuild = \"psx-us\"\npath = \"Breath of Fire III (USA).cue\"\n\n"
    "[[source]]\nbuild = \"psp-eu\"\npath = \"Breath of Fire III (PSP) (Europe).iso\"\n\n"
    "[[source]]\nbuild = \"psp-jp\"\npath = \"Breath of Fire III (PSP) (Japan).iso\"\n\n"
    "[cache]\nassets = [\n  [\"AREA000.DAT\", 0, \"base\", \"pc-zh:chunk\", \"00\"],\n]\n"
    "opt_recipe_sha256 = \"44\"\nopt = [\n  [\"AREA067.DAT\", \"opt/psp-art\", 0, \"psp-eu:copy:WORLD01/AREA067.EMI#6@192\", \"55\"],\n]\n"
    "bgm = [\n  [\"base/bgm/000.DAT\", \"psx-us:seq:BGM000.EMI#1\", \"11\"],\n]\n\n"
    "[exe]\nbuild = \"pc-zh\"\nfile = \"base/exe/data.bin\"\nsha256 = \"33\"\n";

std::string Narrow(const std::wstring& w) {
    std::string s;
    for (wchar_t c : w) s += c < 128 ? static_cast<char>(c) : '?';
    return s;
}

const bof3x::SuiteState& Suite(const std::vector<bof3x::SuiteState>& v, const char* id) {
    for (const bof3x::SuiteState& s : v)
        if (std::string(s.info->id) == id) return s;
    std::printf("  no suite %s\n", id);
    std::exit(2);
}

const bof3x::SuiteMemberState& Member(const bof3x::SuiteState& s, const char* id) {
    for (const bof3x::SuiteMemberState& m : s.members)
        if (std::string(m.info->id) == id) return m;
    std::printf("  no member %s\n", id);
    std::exit(2);
}

bof3x::SuiteInputs Inputs(const char* manifest, std::vector<std::string> installed, std::string language,
                          std::string opt, std::vector<std::string> defaults) {
    bof3x::SuiteInputs in;
    if (manifest) in.manifest = bof3x::ManifestParse(manifest);
    in.installed = [installed](const char* layer) {
        return std::find(installed.begin(), installed.end(), layer) != installed.end();
    };
    in.language = language;
    in.opt = opt;
    in.defaults = defaults;
    return in;
}

void TestManifest() {
    const bof3x::CacheManifest a = bof3x::ManifestParse(kManifestAll);
    Check(a.present && a.error.empty(), "the PC + PSX + PSP manifest reads");
    Check(a.sources == std::vector<std::string>{"pc-zh", "psx-us", "psp-eu", "psp-jp"}, "its sources, distinct, in order");
    Check(a.base_policy == "psx", "base_policy psx", a.base_policy);
    Check(a.exe_tables_from == std::vector<std::string>{"pc-zh"}, "exe_tables_from as a list");
    Check(a.bgm_build == "psx-us", "base/bgm's build from bgm's first row", a.bgm_build);
    Check(a.snd_build.empty(), "no [snd]");
    Check(a.snd_jingles_build.empty(), "no [snd_jingles]");
    // DIV-0129's jingles, read by the Jukebox's PlayStation streams
    const bof3x::CacheManifest j = bof3x::ManifestParse(
        "[[source]]\nbuild = \"psx-us\"\n[snd_jingles]\nbuild = \"psx-us\"\nhow = \"psx-xa-mp3\"\n");
    Check(j.snd_jingles_build == "psx-us" && j.snd_build.empty() && j.sources == std::vector<std::string>{"psx-us"},
          "[snd_jingles] build: the jingles' build, not [snd]'s, not a source", j.snd_jingles_build);
    const bof3x::CacheManifest p = bof3x::ManifestParse(kManifestPspOnly);
    Check(p.base_policy == "psp" && p.exe_tables_from == std::vector<std::string>{"psp-jp"} && p.snd_build == "psp-jp" &&
              p.bgm_build == "psp-jp",
          "the PSP-only manifest: policy, tables, snd, bgm");
    // Before this round: no [build] table. The policy is absent (read as psx),
    // the tables are [exe]'s build.
    const bof3x::CacheManifest old =
        bof3x::ManifestParse("[[source]]\nbuild = \"psx-us\"\n[exe]\nbuild = \"psx-us\"\nfile = \"base/exe/data.bin\"\n");
    Check(old.error.empty() && old.base_policy.empty() && old.exe_tables_from == std::vector<std::string>{"psx-us"},
          "a manifest without [build]: no policy, the tables [exe]'s build");
    Check(!bof3x::ManifestParse("[build]\nbase_policy = \"pc\"\n").error.empty(), "base_policy \"pc\": an error");
    Check(!bof3x::ManifestParse("[build]\nbase_policy = psp\n").error.empty(), "base_policy unquoted: an error");
    Check(!bof3x::ManifestParse("[build]\nexe_tables_from = 3\n").error.empty(), "exe_tables_from = 3: an error");
    Check(!bof3x::ManifestParse("[build]\nexe_tables_from = [\"psp-jp\", 3]\n").error.empty(),
          "exe_tables_from with a number in its list: an error");
    // A [[source]] build key elsewhere is not a source.
    Check(bof3x::ManifestParse("[exe]\nbuild = \"pc-zh\"\n[snd]\nbuild = \"psp-eu\"\n").sources.empty(),
          "[exe] and [snd] builds are not sources");
    Check(!bof3x::ManifestRead(L"").present, "no cache: no manifest");
}

void TestSuites() {
    using bof3x::SuiteSide;
    const std::vector<std::string> walls{"area4-walls"};

    // 1. PSP-only: every suite's base is the PSP's; the PlayStation's is greyed, naming a PSX disc.
    std::vector<bof3x::SuiteState> v = bof3x::SuitesEvaluate(Inputs(kManifestPspOnly, {}, "ja-JP", "", {}));
    Check(v.size() == 3, "three suites");
    for (const bof3x::SuiteState& s : v) {
        Check(s.psp_base && s.psp_enabled && !s.psx_enabled && s.side == SuiteSide::kPsp,
              (std::string("PSP-only: ") + s.info->id + " is the PSP's, the PlayStation's greyed").c_str());
        Check(Narrow(s.psx_why).find("needs a PlayStation disc (psx-jp, psx-us, psx-fr, psx-de or psx-eu-en)") !=
                  std::string::npos,
              "  the reason names the PlayStation discs", Narrow(s.psx_why));
    }
    Check(Member(Suite(v, "audio"), "psp-music").available && Member(Suite(v, "audio"), "psp-effects").available,
          "PSP-only: the cache's songs and effects are the PSP's");
    Check(Member(Suite(v, "data"), "psp-tables").available, "PSP-only: exe_tables_from psp-jp, the tables the PSP's");
    Check(Member(Suite(v, "data"), "psp-names-ja-JP").available && !Member(Suite(v, "data"), "psp-names-en-150").available &&
              Narrow(Member(Suite(v, "data"), "psp-names-en-150").note) == "needs psp-eu",
          "PSP-JP base: the Japanese renames the base's, the English ones need psp-eu");
    // Before MS stage B: a PSP-only cache with no base/bgm.
    std::string no_bgm = kManifestPspOnly;
    no_bgm.replace(no_bgm.find("bgm = ["), no_bgm.find("]\n\n[snd]") + 2 - no_bgm.find("bgm = ["), "");
    v = bof3x::SuitesEvaluate(Inputs(no_bgm.c_str(), {}, "", "", {}));
    Check(!Member(Suite(v, "audio"), "psp-music").available &&
              Narrow(Member(Suite(v, "audio"), "psp-music").note) == "no songs in this cache",
          "PSP-only without base/bgm: the music member says so", Narrow(Member(Suite(v, "audio"), "psp-music").note));
    // An untouched page, or a choice, never edits opt= on a PSP base.
    Check(bof3x::SuitesOptEdit("", {}, v, {0, 1, 0}) == "", "PSP-only: a choice writes nothing");

    // 2. PSX-only: the PlayStation's is the base; every PSP side greyed, naming a PSP disc.
    v = bof3x::SuitesEvaluate(Inputs(kManifestPsxOnly, {}, "en-US", "", walls));
    for (const bof3x::SuiteState& s : v) {
        Check(!s.psp_base && s.psx_enabled && !s.psp_enabled && s.side == SuiteSide::kPsx,
              (std::string("PSX-only: ") + s.info->id + " the PlayStation's, the PSP's greyed").c_str());
        Check(Narrow(s.psp_why).find("needs a PSP disc (psp-jp or psp-eu)") != std::string::npos,
              "  the reason names the PSP discs", Narrow(s.psp_why));
    }
    Check(Narrow(Member(Suite(v, "data"), "psp-names-en-150").note) == "needs psp-eu", "  the English renames need psp-eu",
          Narrow(Member(Suite(v, "data"), "psp-names-en-150").note));
    Check(bof3x::SuitesOptEdit("", walls, v, {1, 1, 1}) == "", "PSX-only: a greyed PSP side writes nothing");

    // 3. PC + PSX + PSP, the cache holding psp-art, psp-tiles and the English renames; Chinese played.
    const std::vector<std::string> have{"psp-art", "psp-tiles", "psp-names-en-150"};
    v = bof3x::SuitesEvaluate(Inputs(kManifestAll, have, "original", "", walls));
    const bof3x::SuiteState& g = Suite(v, "graphics");
    Check(g.psx_enabled && g.psp_enabled && g.side == SuiteSide::kPsx, "all three: graphics offered, the PlayStation's on");
    Check(g.layers_on == std::vector<std::string>{"psp-art", "psp-tiles"}, "  the PSP's adds the two layers built");
    Check(!Member(g, "psp-maps").available &&
              Narrow(Member(g, "psp-maps").note) == "not in the cache (build with --opt psp-maps)",
          "  psp-maps greyed: not built", Narrow(Member(g, "psp-maps").note));
    const bof3x::SuiteState& au = Suite(v, "audio");
    Check(au.psx_enabled && !au.psp_enabled, "all three: audio's PSP side greyed (its layers not built)");
    Check(Narrow(Member(au, "psp-music").note) == "not in the cache (build with --opt psp-music)", "  with the reason",
          Narrow(Member(au, "psp-music").note));
    Check(Narrow(au.psp_why) == "Nothing of the PSP's plays beside this base yet.", "  and the side's", Narrow(au.psp_why));
    const bof3x::SuiteState& d = Suite(v, "data");
    Check(d.psp_enabled && d.layers_on == std::vector<std::string>{"psp-names-en-150"},
          "all three: data offered for the English renames alone");
    Check(!Member(d, "psp-tables").available && !Member(d, "psp-ryu-forms").available &&
              Narrow(Member(d, "psp-ryu-forms").note) == "not in the cache (build with --opt psp-ryu-forms)",
          "  the tables and Ryu's forms greyed: not built", Narrow(Member(d, "psp-ryu-forms").note));
    Check(Narrow(Member(d, "psp-names-en-150").note) == "plays under English text only",
          "  the renames say they play under English text only", Narrow(Member(d, "psp-names-en-150").note));
    Check(Narrow(Member(d, "psp-names-ja-JP").note) == "not in the cache (build with --opt psp-names-ja-JP)",
          "  the Japanese renames: not built");
    // The ini: untouched, the PSP's graphics, the PSP's data, back to the PlayStation's.
    Check(bof3x::SuitesOptEdit("", walls, v, {-1, -1, -1}) == "", "untouched: opt= empty stays empty");
    Check(bof3x::SuitesOptEdit("", walls, v, {1, -1, -1}) == "psp-art,psp-tiles,area4-walls",
          "the PSP's graphics over an empty opt=: its layers, the walls kept", bof3x::SuitesOptEdit("", walls, v, {1, -1, -1}));
    Check(bof3x::SuitesOptEdit("", walls, v, {1, 1, 1}) == "psp-art,psp-tiles,psp-names-en-150,area4-walls",
          "the PSP's graphics and data (audio greyed)", bof3x::SuitesOptEdit("", walls, v, {1, 1, 1}));
    Check(bof3x::SuitesOptEdit("", walls, v, {0, 0, 0}) == "", "the PlayStation's chosen where it already is: unchanged");
    v = bof3x::SuitesEvaluate(Inputs(kManifestAll, have, "en-US", "psp-art,psp-tiles,area4-walls", walls));
    Check(Suite(v, "graphics").side == SuiteSide::kPsp, "opt= with both graphics layers: the PSP's");
    Check(Member(Suite(v, "data"), "psp-names-en-150").note.empty(), "under en-US the renames play, no note");
    Check(bof3x::SuitesOptEdit("psp-art,psp-tiles,area4-walls", walls, v, {0, -1, -1}) == "area4-walls",
          "back to the PlayStation's graphics: the walls stay");
    Check(bof3x::SuitesOptEdit("psp-art,psp-tiles", walls, v, {0, -1, -1}) == "none",
          "the PlayStation's graphics over a list without the walls: none");
    // A list from the PSP extras boxes: one layer of the suite is a partial suite, kept as written.
    v = bof3x::SuitesEvaluate(Inputs(kManifestAll, have, "original", "psp-art,area4-walls", walls));
    Check(Suite(v, "graphics").side == SuiteSide::kMixed, "opt=psp-art alone: graphics partial");
    Check(bof3x::SuitesOptEdit("psp-art,area4-walls", walls, v, {-1, -1, -1}) == "psp-art,area4-walls",
          "  untouched, kept byte for byte");
    Check(bof3x::SuitesOptEdit("psp-art,area4-walls", walls, v, {1, -1, -1}) == "psp-art,psp-tiles,area4-walls",
          "  the PSP's chosen: the suite whole");
    // A layer named in opt= but not installed (psp-maps) is removed by the PlayStation's choice, kept otherwise.
    v = bof3x::SuitesEvaluate(Inputs(kManifestAll, have, "original", "psp-maps,mod-x", walls));
    Check(Suite(v, "graphics").side == SuiteSide::kMixed, "opt= naming an uninstalled member: partial");
    Check(bof3x::SuitesOptEdit("psp-maps,mod-x", walls, v, {-1, 1, -1}) == "psp-maps,mod-x",
          "  another suite's greyed choice leaves the line alone");
    Check(bof3x::SuitesOptEdit("psp-maps,mod-x", walls, v, {0, -1, -1}) == "mod-x",
          "  the PlayStation's graphics removes it, a mod's layer kept");

    // 4. No cache: the layers installed in DAT\ are their own evidence (the PSP extras' install path).
    v = bof3x::SuitesEvaluate(Inputs(nullptr, {"psp-art"}, "", "", walls));
    Check(Suite(v, "graphics").psp_enabled && Suite(v, "graphics").layers_on == std::vector<std::string>{"psp-art"},
          "no cache, psp-art in DAT\\: graphics offered for it");
    Check(!Suite(v, "audio").psp_enabled &&
              Narrow(Suite(v, "audio").psp_why).find("in a cache named in Cache folder") != std::string::npos,
          "no cache: audio's PSP side names the PSP discs and the cache", Narrow(Suite(v, "audio").psp_why));

    // 5. A manifest the page cannot read: every side greyed with the reason.
    v = bof3x::SuitesEvaluate(Inputs("[build]\nbase_policy = \"pc\"\n", have, "", "", walls));
    for (const bof3x::SuiteState& s : v)
        Check(!s.psx_enabled && !s.psp_enabled && Narrow(s.psx_why).find("base_policy") != std::string::npos,
              (std::string("bad base_policy: ") + s.info->id + " greyed with the reason").c_str());

    // 6. TG: every engine-half layer in the cache (recipes/psp-layers.toml) beside the PlayStation's base.
    const std::vector<std::string> tg{"psp-art", "psp-tiles", "psp-maps", "psp-music", "psp-snd", "psp-tables",
                                      "psp-names-en-150", "psp-ryu-forms"};
    v = bof3x::SuitesEvaluate(Inputs(kManifestAll, tg, "en-US", "", walls));
    Check(Suite(v, "audio").psp_enabled && Suite(v, "audio").layers_on == std::vector<std::string>{"psp-music", "psp-snd"},
          "TG: audio offered, its two layers");
    Check(Suite(v, "data").layers_on == std::vector<std::string>{"psp-tables", "psp-names-en-150", "psp-ryu-forms"},
          "TG: data offered for the tables, the renames and Ryu's forms");
    const std::string all = bof3x::SuitesOptEdit("", walls, v, {1, 1, 1});
    Check(all == "psp-art,psp-tiles,psp-maps,psp-music,psp-snd,psp-tables,psp-names-en-150,psp-ryu-forms,area4-walls",
          "TG: every suite's PSP side, in the catalogue's order, the walls last", all);
    std::string why;
    Check(bof3x::ConfigOptValid(all, why), "  a valid opt= of nine layers (kOptMax 16 since TG)", why);
    Check(bof3x::ConfigOptValid(all + ",psp-names-ja-JP,psp-chord-one-tone", why), "  eleven layers valid", why);
    Check(!bof3x::ConfigOptValid("a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q", why) && why == "more than 16 layers",
          "  seventeen refused", why);
    Check(bof3x::SuitesOptEdit(all, walls, v, {-1, 0, -1}) ==
              "psp-art,psp-tiles,psp-maps,psp-tables,psp-names-en-150,psp-ryu-forms,area4-walls",
          "TG: back to the PlayStation's audio removes its two layers");
    v = bof3x::SuitesEvaluate(Inputs(kManifestPspOnly, {}, "ja-JP", "", {}));
    Check(Member(Suite(v, "data"), "psp-ryu-forms").available, "TG, PSP-only: Ryu's forms the base's");

    // The order a written list takes: the suites' layers as recipes/suites.toml has them.
    Check(bof3x::SuiteLayers() == std::vector<std::string>{"psp-art", "psp-tiles", "psp-maps", "psp-music", "psp-snd",
                                                         "psp-tables", "psp-names-en-150", "psp-names-ja-JP",
                                                         "psp-ryu-forms"},
          "SuiteLayers: the catalogue's order");
}

// DIV-0113: the Jukebox's battle-chord box (ChordEvaluate).
void TestChord() {
    const std::vector<std::string> walls{"area4-walls"};
    bof3x::ChordState c = bof3x::ChordEvaluate(bof3x::ManifestParse(kManifestAll), true, "", walls);
    Check(c.enabled && !c.checked, "chord: in the cache, opt= empty: offered, off (the restored chord)");
    c = bof3x::ChordEvaluate(bof3x::ManifestParse(kManifestAll), true, "psp-chord-one-tone,area4-walls", walls);
    Check(c.enabled && c.checked, "chord: opt= names it: checked");
    c = bof3x::ChordEvaluate(bof3x::ManifestParse(kManifestAll), false, "", walls);
    Check(!c.enabled && Narrow(c.why).find("build it again") != std::string::npos, "chord: a PSP source, not built",
          Narrow(c.why));
    c = bof3x::ChordEvaluate(bof3x::ManifestParse(kManifestPsxOnly), false, "", walls);
    Check(!c.enabled && Narrow(c.why).find("Needs a PSP disc (psp-jp or psp-eu)") != std::string::npos,
          "chord: PSX-only greyed, naming the PSP discs", Narrow(c.why));
    c = bof3x::ChordEvaluate(bof3x::CacheManifest{}, false, "", walls);
    Check(!c.enabled && Narrow(c.why).find("Cache folder") != std::string::npos, "chord: no cache", Narrow(c.why));
    c = bof3x::ChordEvaluate(bof3x::ManifestParse(kManifestPspOnly), true, "none", {});
    Check(c.enabled && !c.checked, "chord: PSP-only, opt=none: offered, off");
    using Boxes = std::vector<std::pair<std::string, bool>>;
    Check(bof3x::ConfigOptEdit("", walls, Boxes{{bof3x::kChordLayer, true}}) == "area4-walls,psp-chord-one-tone",
          "chord on over an empty opt=: the walls kept, the chord after them");
    Check(bof3x::ConfigOptEdit("area4-walls,psp-chord-one-tone", walls, Boxes{{bof3x::kChordLayer, false}}) ==
              "area4-walls",
          "chord off again");
}

}  // namespace

// DIV-0108: the data suite's two code members, the PSP's battle rules,
// played through psp= rather than opt=.
void TestCodeMembers() {
    using bof3x::SuiteSide;
    const std::vector<std::string> walls{"area4-walls"};
    const std::vector<std::string> codes{"duel-loss", "myria-item"};
    // PSP-only (PSP-JP): both the base's, on by default; psp=none notes them off; a choice writes nothing.
    std::vector<bof3x::SuiteState> v = bof3x::SuitesEvaluate(Inputs(kManifestPspOnly, {}, "ja-JP", "", {}));
    const bof3x::SuiteState& d0 = Suite(v, "data");
    Check(Member(d0, "psp-duel-loss").available && Member(d0, "psp-duel-loss").note.empty() &&
              Member(d0, "psp-myria-item").available && Member(d0, "psp-myria-item").note.empty(),
          "PSP-only: the two battle rules the base's, on (psp= empty)");
    Check(d0.codes_all == codes && d0.codes_on.empty(), "  every code member listed; none to add on a PSP base");
    Check(bof3x::SuitesPspEdit("", v, {0, 1, 0}) == "" && bof3x::SuitesPspEdit("none", v, {0, 0, 0}) == "none",
          "PSP-only: a choice writes nothing to psp=");
    bof3x::SuiteInputs off = Inputs(kManifestPspOnly, {}, "ja-JP", "", {});
    off.psp = "none";
    v = bof3x::SuitesEvaluate(off);
    Check(Narrow(Member(Suite(v, "data"), "psp-duel-loss").note) == "off in bof3x.ini's psp=",
          "PSP-only with psp=none: the members say they are off", Narrow(Member(Suite(v, "data"), "psp-duel-loss").note));

    // PSX-only: greyed, naming the PSP discs; the PSP side stays greyed.
    v = bof3x::SuitesEvaluate(Inputs(kManifestPsxOnly, {}, "en-US", "", walls));
    Check(!Member(Suite(v, "data"), "psp-duel-loss").available &&
              Narrow(Member(Suite(v, "data"), "psp-duel-loss").note) == "needs psp-jp or psp-eu" &&
              !Suite(v, "data").psp_enabled,
          "PSX-only: the battle rules greyed, needing a PSP disc", Narrow(Member(Suite(v, "data"), "psp-duel-loss").note));
    Check(bof3x::SuitesPspEdit("", v, {1, 1, 1}) == "", "  a greyed side writes nothing");

    // PC + PSX + PSP: offered beside the PlayStation base, off by default.
    const std::vector<std::string> have{"psp-art", "psp-tiles", "psp-names-en-150"};
    v = bof3x::SuitesEvaluate(Inputs(kManifestAll, have, "original", "", walls));
    const bof3x::SuiteState& d = Suite(v, "data");
    Check(d.psp_enabled && d.codes_on == codes && d.side == SuiteSide::kPsx &&
              Member(d, "psp-duel-loss").available && Member(d, "psp-myria-item").available,
          "all three: the battle rules playable beside a PlayStation base, the PlayStation's on");
    Check(Member(d, "psp-duel-loss").note.empty(), "  off by default beside a PlayStation base, no note",
          Narrow(Member(d, "psp-duel-loss").note));
    Check(bof3x::SuitesPspEdit("", v, {-1, -1, -1}) == "", "untouched: psp= empty stays empty");
    Check(bof3x::SuitesPspEdit("", v, {1, -1, 1}) == "duel-loss,myria-item", "the PSP's data: both rules",
          bof3x::SuitesPspEdit("", v, {1, -1, 1}));
    Check(bof3x::SuitesPspEdit("", v, {1, -1, -1}) == "", "the PSP's graphics alone: psp= untouched");
    Check(bof3x::SuitesPspEdit("", v, {-1, -1, 0}) == "", "the PlayStation's data over an empty psp=: still empty");
    Check(bof3x::SuitesOptEdit("", walls, v, {-1, -1, 1}) == "psp-names-en-150,area4-walls",
          "  and opt= gets the data suite's layer as before", bof3x::SuitesOptEdit("", walls, v, {-1, -1, 1}));
    // Both lines read: the layer and both rules on is the PSP's; a rule off is partial.
    bof3x::SuiteInputs in = Inputs(kManifestAll, have, "en-US", "psp-names-en-150,area4-walls", walls);
    in.psp = "myria-item,duel-loss";
    v = bof3x::SuitesEvaluate(in);
    Check(Suite(v, "data").side == SuiteSide::kPsp && Member(Suite(v, "data"), "psp-duel-loss").note.empty(),
          "opt= with the renames and psp= with both rules: the PSP's data");
    Check(bof3x::SuitesPspEdit("myria-item,duel-loss", v, {-1, -1, 1}) == "myria-item,duel-loss",
          "  the PSP's chosen where it is: psp= byte for byte");
    Check(bof3x::SuitesPspEdit("myria-item,duel-loss", v, {-1, -1, 0}) == "none", "  the PlayStation's: none");
    in.psp = "duel-loss";
    v = bof3x::SuitesEvaluate(in);
    Check(Suite(v, "data").side == SuiteSide::kMixed, "psp= with one rule: data partial");
    Check(bof3x::SuitesPspEdit("duel-loss", v, {-1, -1, 1}) == "duel-loss,myria-item", "  the PSP's: both, in order");
    Check(bof3x::SuitesPspEdit("duel-loss,mod-y", v, {-1, -1, 0}) == "mod-y", "  a name of no member is kept");

    // No cache: no evidence of a PSP disc for code; greyed.
    v = bof3x::SuitesEvaluate(Inputs(nullptr, {"psp-art"}, "", "", walls));
    Check(!Member(Suite(v, "data"), "psp-myria-item").available && Suite(v, "data").codes_on.empty(),
          "no cache: the battle rules greyed");

    Check(bof3x::SuitesPspPlayed("", true) == codes && bof3x::SuitesPspPlayed("", false).empty() &&
              bof3x::SuitesPspPlayed("none", true).empty() &&
              bof3x::SuitesPspPlayed("myria-item", false) == std::vector<std::string>{"myria-item"},
          "SuitesPspPlayed: the default by the base, none, a list");
}

// ---- the code review of 2026-10-12 (psp/review-fixes) ----------------------

// A directory and its parents.
void MakeDirs(const std::wstring& path) {
    for (size_t at = path.find(L'\\', 3); at != std::wstring::npos; at = path.find(L'\\', at + 1))
        CreateDirectoryW(path.substr(0, at).c_str(), nullptr);
    CreateDirectoryW(path.c_str(), nullptr);
}

std::wstring WideOf(const char* s) {
    std::wstring w;
    for (const char* c = s; *c; ++c) w += static_cast<wchar_t>(*c);
    return w;
}

// A cache on disk: `trees` as opt\<layer>\<part>\<file> (one file each), and a
// manifest.toml when `manifest` is not null.
std::wstring FakeCache(const wchar_t* name, const std::vector<std::pair<const char*, const wchar_t*>>& trees,
                       const char* manifest) {
    const std::wstring cache = Temp(name);
    MakeDirs(cache);
    DeleteFileW((cache + L"\\manifest.toml").c_str());
    for (const auto& [layer, part] : trees) {
        const std::wstring dir = cache + L"\\opt\\" + WideOf(layer) + L"\\" + part;
        MakeDirs(dir);
        WriteAll(dir + (std::wcscmp(part, L"exe") == 0 ? L"\\place.toml" : L"\\001.DAT"), "");
    }
    if (manifest) WriteAll(cache + L"\\manifest.toml", manifest);
    return cache;
}

std::string Env(const char* name) {
    char buf[1024];
    const DWORD n = GetEnvironmentVariableA(name, buf, sizeof buf);
    return n == 0 || n >= sizeof buf ? std::string() : std::string(buf, n);
}

void ClearEnv() {
    for (const char* v : {"BOF3X_OPT", "BOF3X_CACHE", "BOF3X_CACHE_DATA", "BOF3X_LANG", "BOF3X_PSP", "BOF3X_MUSIC"})
        SetEnvironmentVariableA(v, nullptr);
}

// The PC + US + PSP-EU cache as importer.py build --opt writes it: base_policy
// psx, an opt.toml layer in [cache] opt, and the suites' engine half in
// [psp_layers]. Rows abridged to what the reader takes.
const char kManifestLayers[] =
    "[meta]\ntarget = \"pc-zh\"\n\n[build]\nbase_policy = \"psx\"\nexe_tables_from = \"pc-zh\"\n\n"
    "[[source]]\nbuild = \"pc-zh\"\npath = \"DAT\"\n\n[[source]]\nbuild = \"psx-us\"\npath = \"us.cue\"\n\n"
    "[[source]]\nbuild = \"psp-eu\"\npath = \"eu.iso\"\n\n"
    "[cache]\nassets = [\n  [\"A.DAT\", 0, \"base\", \"pc-zh:chunk\", \"h\"],\n]\n"
    "opt = [\n  [\"AREA004.DAT\", \"opt/area4-walls\", 0, \"psx-us:copy:WORLD00/AREA004.EMI#8@0\", \"h\"],\n"
    "  [\"ITEM.DAT\", \"opt/psp-names-en-150\", 0, \"psp-eu:name:items#1\", \"h\"],\n]\n"
    "bgm = [\n  [\"base/bgm/000.DAT\", \"psx-us:seq:BGM000.EMI#1\", \"h\"],\n]\n\n"
    "[exe]\nbuild = \"pc-zh\"\n\n[psp_layers]\nrecipe_sha256 = \"h\"\nrows = [\n"
    "  [\"psp-music\", \"opt/psp-music/bgm/001.DAT\", \"psp-eu:ppms:BGM000.EMI#1\", \"h\"],\n"
    "  [\"psp-snd\", \"opt/psp-snd/snd/001.DAT\", \"psp-eu:psp-at3\", \"h\"],\n"
    "  [\"psp-tables\", \"opt/psp-tables/exe/place.toml\", \"psp-eu:exe:h\", \"h\"],\n"
    "  [\"psp-ryu-forms\", \"opt/psp-ryu-forms/dat/001.DAT\", \"psp-eu:type1:RYUD#1\", \"h\"],\n]\n";

// Finding 1: a hosted start with the data suite on the PSP's side. The suite
// writes psp-tables into opt=, and the hosted build refuses that layer at
// start (dat_load.cpp ReadOptLayers): the launcher leaves it out, by part.
void TestHostedOpt() {
    const std::wstring game = Temp(L"bof3x_rf_game");
    MakeDirs(game + L"\\DAT");
    const std::wstring cache = FakeCache(L"bof3x_rf_cache_all",
                                         {{"psp-music", L"bgm"}, {"psp-snd", L"snd"}, {"psp-tables", L"exe"},
                                          {"psp-ryu-forms", L"dat"}, {"psp-names-en-150", L"dat"}},
                                         kManifestLayers);
    // the data suite's PSP side, as the dialog writes it over an empty opt=
    bof3x::SuiteInputs in;
    in.manifest = bof3x::ManifestRead(cache);
    in.installed = [&](const char* layer) { return bof3x::ConfigOptInstalled(game, layer, cache); };
    in.language = "en-150";
    const std::vector<bof3x::SuiteState> v = bof3x::SuitesEvaluate(in);
    bof3x::Config cfg;
    cfg.cache = bof3x::ConfigToUtf8(cache);
    cfg.opt = bof3x::SuitesOptEdit("", {}, v, {-1, -1, 1});
    Check(cfg.opt == "psp-tables,psp-names-en-150,psp-ryu-forms", "hosted: the data suite's PSP side in opt=", cfg.opt);

    ClearEnv();
    bof3x::ConfigApplyEnvironment(game, cfg, false);
    Check(Env("BOF3X_OPT") == "psp-tables,psp-ryu-forms", "hosted: bof3x.exe gets the tables (the text layer left off "
                                                          "under the original text)", Env("BOF3X_OPT"));
    ClearEnv();
    bof3x::ConfigApplyEnvironment(game, cfg, true);
    Check(Env("BOF3X_OPT") == "psp-ryu-forms", "hosted: the hosted build does not get psp-tables (an exe part)",
          Env("BOF3X_OPT"));
    ClearEnv();
    SetEnvironmentVariableA("BOF3X_OPT", "psp-tables");
    bof3x::ConfigApplyEnvironment(game, cfg, true);
    Check(Env("BOF3X_OPT") == "psp-tables", "hosted: a hand-set BOF3X_OPT is the DLL's to refuse, untouched");
    ClearEnv();

    std::vector<std::string> dropped;
    Check(bof3x::ConfigOptHosted("psp-art,psp-tables,area4-walls", std::wstring(), dropped) == "psp-art,area4-walls" &&
              dropped == std::vector<std::string>{"psp-tables"},
          "ConfigOptHosted: psp-tables dropped by its recipe part, no cache needed");
    // a layer no recipe names, whose cache tree has an exe part: the DLL's test
    const std::wstring mod = FakeCache(L"bof3x_rf_cache_mod", {{"mod-x", L"exe"}, {"mod-y", L"dat"}}, nullptr);
    dropped.clear();
    Check(bof3x::ConfigOptHosted("mod-y,mod-x", mod, dropped) == "mod-y" && dropped == std::vector<std::string>{"mod-x"},
          "ConfigOptHosted: a mod's layer with exe\\place.toml dropped by the cache's tree");
    dropped.clear();
    Check(bof3x::ConfigOptHosted("psp-music,psp-snd,psp-ryu-forms,psp-chord-one-tone", cache, dropped) ==
                  "psp-music,psp-snd,psp-ryu-forms,psp-chord-one-tone" && dropped.empty(),
          "ConfigOptHosted: the bgm, snd and dat layers kept (the hosted build takes them)");
}

// Finding 2: a layer tree the manifest does not list (a build that stopped
// part-way) is not offered; with no manifest, the tree is its own evidence.
void TestListedLayers() {
    const std::wstring game = Temp(L"bof3x_rf_game");
    const char partial[] =  // only psp-music's rows were written
        "[build]\nbase_policy = \"psx\"\n[[source]]\nbuild = \"psp-eu\"\n[psp_layers]\nrows = [\n"
        "  [\"psp-music\", \"opt/psp-music/bgm/001.DAT\", \"psp-eu:ppms:B#1\", \"h\"],\n]\n";
    const std::wstring cache = FakeCache(L"bof3x_rf_cache_partial", {{"psp-music", L"bgm"}, {"psp-snd", L"snd"},
                                                                     {"psp-tables", L"exe"}, {"psp-art", L"dat"}},
                                         partial);
    const bof3x::CacheManifest m = bof3x::ManifestParse(partial);
    Check(m.opt_layers == std::vector<std::string>{"psp-music"}, "listing: [psp_layers] rows read");
    Check(bof3x::ManifestParse(kManifestLayers).opt_layers ==
              std::vector<std::string>{"area4-walls", "psp-names-en-150", "psp-music", "psp-snd", "psp-tables",
                                       "psp-ryu-forms"},
          "listing: [cache] opt and [psp_layers] rows, distinct, in order");
    Check(bof3x::ConfigOptInstalled(game, "psp-music", cache), "listing: a listed layer is offered");
    for (const char* l : {"psp-snd", "psp-tables", "psp-art"})
        Check(!bof3x::ConfigOptInstalled(game, l, cache), (std::string("listing: unlisted ") + l + " not offered").c_str());
    const std::string played = bof3x::ConfigOptPlayable(game, "psp-music,psp-snd,psp-tables", "", cache);
    Check(played == "psp-music", "listing: ConfigOptPlayable drops the unlisted layers", played);
    DeleteFileW((cache + L"\\manifest.toml").c_str());
    Check(bof3x::ConfigOptInstalled(game, "psp-snd", cache), "no manifest: the tree is its own evidence, as before");
}

// Finding 3: BOF3X_CACHE_DATA=0, the cache for the music only. A layer's bgm
// part is still the music's; its snd part and the chord's banks are the data's.
void TestMusicOnly() {
    const std::wstring game = Temp(L"bof3x_rf_game");
    const std::wstring cache = FakeCache(L"bof3x_rf_cache_all2",
                                         {{"psp-music", L"bgm"}, {"psp-snd", L"snd"}, {"psp-chord-one-tone", L"dat"}},
                                         nullptr);
    Check(bof3x::ConfigOptInstalled(game, "psp-music", std::wstring(), cache),
          "music only: psp-music's bgm from the music's cache");
    Check(!bof3x::ConfigOptInstalled(game, "psp-snd", std::wstring(), cache) &&
              !bof3x::ConfigOptInstalled(game, "psp-chord-one-tone", std::wstring(), cache),
          "music only: psp-snd and the chord's banks are the data's, not offered");
    bof3x::Config cfg;
    cfg.cache = bof3x::ConfigToUtf8(cache);
    cfg.opt = "psp-music,psp-snd,psp-chord-one-tone";
    ClearEnv();
    SetEnvironmentVariableA("BOF3X_CACHE_DATA", "0");
    Check(bof3x::ConfigCacheDataRoot(cfg).empty() && bof3x::ConfigCacheRoot(cfg) == cache,
          "music only: no data root, the cache root kept");
    bof3x::ConfigApplyEnvironment(game, cfg, false);
    Check(Env("BOF3X_OPT") == "psp-music", "music only: BOF3X_OPT names the music layer alone", Env("BOF3X_OPT"));
    ClearEnv();
    bof3x::ConfigApplyEnvironment(game, cfg, false);
    Check(Env("BOF3X_OPT") == "psp-music,psp-snd,psp-chord-one-tone", "the data too: all three", Env("BOF3X_OPT"));
    ClearEnv();
}

// Finding 4: one manifest reader. Good, absent and misspelled values; the
// refusal text the launcher stops on and the DLL Fatals with
// (common/cache_manifest.h ManifestRefusal, both sides).
void TestOneReader() {
    for (const char* good : {"psp", "psx"}) {
        const bof3x::CacheManifest m =
            bof3x::ManifestParse(std::string("[build]\nbase_policy = \"") + good + "\"\n");
        Check(m.error.empty() && m.base_policy == good, (std::string("base_policy \"") + good + "\" read").c_str());
    }
    Check(bof3x::ManifestPspBase(bof3x::ManifestParse("[build]\nbase_policy = \"psp\"\n")) &&
              !bof3x::ManifestPspBase(bof3x::ManifestParse("[meta]\ntarget = \"pc-zh\"\n")) &&
              !bof3x::ManifestPspBase(bof3x::CacheManifest{}),
          "absent base_policy (and no manifest) is psx");
    for (const char* bad : {"\"PSP\"", "\"ps\"", "psp", "\"psp \"", "\"\"", "[\"psp\"]", "\"psp\", \"psx\""}) {
        const bof3x::CacheManifest m = bof3x::ManifestParse(std::string("[build]\nbase_policy = ") + bad + "\n");
        const std::string r = bof3x::ManifestRefusal(m, "C:\\c\\manifest.toml");
        Check(!m.error.empty() && !bof3x::ManifestPspBase(m) &&
                  r.find(std::string("manifest.toml's [build] base_policy is ") + bad +
                         ", neither \"psp\" nor \"psx\" (C:\\c\\manifest.toml)") == 0,
              (std::string("base_policy = ") + bad + " refused, the same sentence for both").c_str(), r);
    }
    Check(bof3x::ManifestRefusal(bof3x::ManifestParse("[build]\nbase_policy = \"psp\"\n"), "x").empty(),
          "no refusal for a good manifest");
    // The bgm row's build and [snd] as the Jukebox and the page read them.
    const bof3x::CacheManifest l = bof3x::ManifestParse(kManifestLayers);
    Check(l.bgm_build == "psx-us" && l.snd_build.empty() && l.exe_tables_from == std::vector<std::string>{"pc-zh"},
          "the bgm row's build, [snd], exe_tables_from from one reader");
    // The launcher stops up front on a bad manifest: ManifestRead of the file.
    const std::wstring cache = FakeCache(L"bof3x_rf_cache_bad", {}, "[build]\nbase_policy = \"PSP\"\n");
    Check(!bof3x::ManifestRead(cache).error.empty(), "ManifestRead: the file's misspelling is the error");
}

// Finding 6: the Jukebox's chord box ticked and unticked writes opt= back byte
// for byte (LT's rule: an untouched page writes the line as read).
void TestChordRoundTrip() {
    const std::vector<std::string> walls{"area4-walls"};
    const std::vector<bof3x::SuiteState> v = bof3x::SuitesEvaluate(Inputs(kManifestAll, {}, "original", "", walls));
    using Boxes = std::vector<std::pair<std::string, bool>>;
    // what the box did before: two edits in turn, the second over the first's line
    const std::string twice = bof3x::ConfigOptEdit(bof3x::ConfigOptEdit("", walls, Boxes{{bof3x::kChordLayer, true}}),
                                                   walls, Boxes{{bof3x::kChordLayer, false}});
    Check(twice == "area4-walls", "the old way: on then off over an empty line wrote the default (the bug)", twice);
    // now a choice, made once on Play from the line as read
    for (const char* line : {"", "none", "area4-walls", "psp-art,area4-walls"}) {
        const std::string back = bof3x::SuitesOptEdit(line, walls, v, {-1, -1, -1}, Boxes{{bof3x::kChordLayer, false}});
        Check(back == line, (std::string("chord on then off: opt=") + line + " byte for byte").c_str(), back);
    }
    Check(bof3x::SuitesOptEdit("", walls, v, {-1, -1, -1}, Boxes{{bof3x::kChordLayer, true}}) ==
              "area4-walls,psp-chord-one-tone",
          "chord on over an empty line: the walls kept, the chord after them");
    Check(bof3x::SuitesOptEdit("area4-walls,psp-chord-one-tone", walls, v, {-1, -1, -1},
                               Boxes{{bof3x::kChordLayer, true}}) == "area4-walls,psp-chord-one-tone",
          "chord left on where the line had it: byte for byte");
}

// DIV-0041, the 21:9 picture (docs/ultrawide-frame.md): wide= 0, 1 or 2 reads,
// writes back the same, and goes out as BOF3X_WIDE (nothing for 0); a value
// the DLL would refuse is dropped to the default, as every unknown value is;
// a BOF3X_WIDE already set (the run tools' --wide) wins over the ini.
void TestWide() {
    const std::wstring a = Temp(L"bof3x_config_tests_wide.ini"), game = Temp(L"bof3x_wide_game");
    MakeDirs(game + L"\\DAT");
    std::string err;
    for (int v = 0; v <= 2; ++v) {
        WriteAll(a, "[bof3x]\r\nwide=" + std::to_string(v) + "\r\n");
        bof3x::Config c;
        Check(bof3x::ConfigLoad(a, c, err) && err.empty() && c.wide == v, "wide= read", std::to_string(c.wide));
        Check(bof3x::ConfigSave(a, c), "save");
        Check(ReadAll(a).find("\r\nwide=" + std::to_string(v) + "\r\n") != std::string::npos, "the wide= line written");
        SetEnvironmentVariableA("BOF3X_WIDE", nullptr);
        bof3x::ConfigApplyEnvironment(game, c, false);
        Check(Env("BOF3X_WIDE") == (v == 0 ? "" : std::to_string(v)), "BOF3X_WIDE from wide=", Env("BOF3X_WIDE"));
        SetEnvironmentVariableA("BOF3X_WIDE", "0");
        bof3x::ConfigApplyEnvironment(game, c, false);
        Check(Env("BOF3X_WIDE") == "0", "a BOF3X_WIDE already set is kept", Env("BOF3X_WIDE"));
    }
    for (const char* bad : {"3", "-1", "21:9", "true"}) {
        WriteAll(a, std::string("[bof3x]\r\nwide=") + bad + "\r\n");
        bof3x::Config c;
        Check(bof3x::ConfigLoad(a, c, err) && c.wide == 0, (std::string("wide=") + bad + " is the default").c_str(),
              std::to_string(c.wide));
    }
    SetEnvironmentVariableA("BOF3X_WIDE", nullptr);
    ClearEnv();
    DeleteFileW(a.c_str());
}

// Finding 7: psp= checked up front by the DLL's rule, as opt= is.
void TestPspValid() {
    std::string why;
    for (const char* good : {"", "none", "duel-loss", "myria-item", "myria-item,duel-loss"})
        Check(bof3x::ConfigPspValid(good, why), (std::string("psp=") + good + " valid").c_str(), why);
    for (const char* bad : {"duel-los", "Duel-loss", "duel-loss,", ",duel-loss", "duel-loss,,myria-item",
                            "duel-loss,duel-loss", "none,duel-loss", "duel-loss myria-item", "original", " "}) {
        why.clear();
        Check(!bof3x::ConfigPspValid(bad, why) && !why.empty(), (std::string("psp=") + bad + " refused").c_str());
    }
    why.clear();
    bof3x::ConfigPspValid("duel-los", why);
    Check(why == "\"duel-los\" is not one of the PSP's changes (duel-loss, myria-item)", "the reason names the items", why);
}

// docs/install-sources.md: the home's own game\ and cache\ are defaults only -
// BOF3X_CACHE and the ini's cache= still win, an empty home offers neither, and
// the default is never written to the ini. ffmpeg= round-trips.
void TestHomeDefaults() {
    const std::wstring home = Temp(L"bof3x_home_test");
    SetEnvironmentVariableW(L"BOF3X_HOME", home.c_str());
    // a previous run's folders, removed so the home starts empty
    DeleteFileW((home + L"\\cache\\manifest.toml").c_str());
    DeleteFileW((home + L"\\bof3x.ini").c_str());
    for (const wchar_t* dir : {L"\\cache", L"\\game\\DAT", L"\\game"}) RemoveDirectoryW((home + dir).c_str());
    CreateDirectoryW(home.c_str(), nullptr);
    ClearEnv();
    bof3x::Config cfg;
    Check(bof3x::HomeDir() == home, "BOF3X_HOME names the home", bof3x::ConfigToUtf8(bof3x::HomeDir()));
    Check(bof3x::HomeCacheDefault().empty() && bof3x::HomeGameDefault().empty(), "an empty home: no default");
    Check(bof3x::ConfigCacheRoot(cfg).empty(), "an empty home: no cache");
    CreateDirectoryW((home + L"\\cache").c_str(), nullptr);
    Check(bof3x::HomeCacheDefault().empty(), "cache\\ without manifest.toml: no default");
    WriteAll(home + L"\\cache\\manifest.toml", "[meta]\n");
    CreateDirectoryW((home + L"\\game").c_str(), nullptr);
    CreateDirectoryW((home + L"\\game\\DAT").c_str(), nullptr);
    Check(bof3x::HomeGameDefault() == home + L"\\game", "game\\DAT: the home's game folder");
    Check(bof3x::ConfigCacheRoot(cfg) == home + L"\\cache", "no env, no ini: the home's cache");
    const std::wstring game = Temp(L"bof3x_home_game");
    bof3x::ConfigApplyEnvironment(game, cfg, false);
    Check(Env("BOF3X_CACHE") == bof3x::ConfigToUtf8(home + L"\\cache"), "the game is given the home's cache",
          Env("BOF3X_CACHE"));
    ClearEnv();
    cfg.cache = "C:\\elsewhere";
    Check(bof3x::ConfigCacheRoot(cfg) == L"C:\\elsewhere", "the ini's cache= wins");
    SetEnvironmentVariableW(L"BOF3X_CACHE", L"D:\\env");
    Check(bof3x::ConfigCacheRoot(cfg) == L"D:\\env", "BOF3X_CACHE wins");
    ClearEnv();
    cfg.cache.clear();
    cfg.ffmpeg = "C:\\tools\\ffmpeg.exe";
    const std::wstring ini = home + L"\\bof3x.ini";
    Check(bof3x::ConfigSave(ini, cfg), "save");
    const std::string text = ReadAll(ini);
    Check(text.find("\r\ncache=\r\n") != std::string::npos, "the home's cache is not written to the ini");
    bof3x::Config back;
    std::string why;
    bof3x::ConfigLoad(ini, back, why);
    Check(back.ffmpeg == cfg.ffmpeg, "ffmpeg= round-trips", back.ffmpeg);
    // The first run's save (the Sources page, before any language is chosen)
    // must not name "original": the disc-only install it builds would refuse it.
    Check(text.find("\r\nlanguage=") == std::string::npos && !back.language_named,
          "an unchosen language is not written");
    SetEnvironmentVariableW(L"BOF3X_HOME", nullptr);
}

int main() {
    std::printf("home defaults (docs/install-sources.md)\n");
    TestHomeDefaults();
    std::printf("cache manifest\n");
    TestManifest();
    std::printf("review fixes: hosted opt=, listed layers, music only, one reader, chord round trip, psp=\n");
    TestHostedOpt();
    TestListedLayers();
    TestMusicOnly();
    TestOneReader();
    TestChordRoundTrip();
    TestPspValid();
    std::printf("suites\n");
    TestSuites();
    std::printf("code members (DIV-0108)\n");
    TestCodeMembers();
    std::printf("jukebox chord\n");
    TestChord();
    std::printf("opt edit\n");
    TestOptEdit();
    std::printf("installed layers\n");
    TestInstalled();
    std::printf("ini round trip\n");
    TestRoundTrip();
    std::printf("wide= (DIV-0041: off, 16:9, 21:9)\n");
    TestWide();
    std::printf("cache path code page\n");
    TestCacheCodePage();
    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}

// Host tests for the launcher's settings (src/launcher/config.cpp, suites.cpp):
// the ini's round trip through ConfigLoad / ConfigSave, ConfigOptEdit, and the
// three suites - what the settings dialog makes of a cache's manifest.toml and
// of an `opt=` line (docs/launcher-suites.md). No game data; a Windows host
// (config.cpp is Win32). docs/launcher-settings.md section 3.
//
//   cmake -S tools/launcher_host -B <scratch>/lh -G Ninja -DCMAKE_CXX_COMPILER=clang++
//   cmake --build <scratch>/lh && <scratch>/lh/config_tests.exe
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

// TG-5: the Jukebox's battle-chord box (ChordEvaluate).
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

int main() {
    std::printf("cache manifest\n");
    TestManifest();
    std::printf("suites\n");
    TestSuites();
    std::printf("jukebox chord\n");
    TestChord();
    std::printf("opt edit\n");
    TestOptEdit();
    std::printf("installed layers\n");
    TestInstalled();
    std::printf("ini round trip\n");
    TestRoundTrip();
    std::printf("cache path code page\n");
    TestCacheCodePage();
    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}

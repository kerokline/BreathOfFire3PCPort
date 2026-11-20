// The importer's cache manifest (tools/importer.py write_manifest, then
// tools/psp_layers.py's [psp_layers] table), as far as the engine and the
// launcher read its keys: ONE reader for both (the code review of 2026-10-12,
// docs/psp-only-round.md; it was three - the launcher's suites page, the
// DLL's battle-rule default and the Jukebox - and two of them read
// base_policy by different rules).
//
// What it reads:
//   [[source]] build          the builds the cache was made from, distinct, in order
//   [build] base_policy       exactly "psp" or "psx"; absent is psx (a cache from
//                             before the PSP-only round); anything else is an error
//   [build] exe_tables_from   a build id or a one-line list of them; absent: [exe] build
//   [build] install_kind      exactly "pc" or "disc" (docs/exe-retire-round.md, X1): "pc"
//                             when the PC game's DAT\ was a source - the cache's base\ and
//                             loc\zh-CN\ are its chunks, and setup lays the game folder from
//                             that PC folder; "disc" when it was not - the base is a disc's
//                             (PlayStation or PSP) and setup lays the game folder from the
//                             cache (importer.py install --base). Absent: a cache from before
//                             the key, answered from its [cache] assets (ManifestInstallKind);
//                             anything else is an error
//   [cache] assets            whether a row's layer is loc/zh-CN (only the PC's DAT\ carries
//                             that layer): the kind of a cache from before install_kind
//   [cache] bgm               the first row's source build (base/bgm/)
//   [snd] build               base/snd/'s build
//   [snd_jingles] build       the build of the PlayStation disc's 11 jingles (DIV-0129)
//   [snd_jingles] dir         their folder: base/snd on a PSX-only install, base/psx-jingles
//                             beside a PC install (DIV-0150); absent is base/snd (a cache
//                             from before DIV-0150)
//   [cache] opt, [psp_layers] rows
//                             the opt/ layers the manifest lists: an opt/<layer>/
//                             tree the manifest does not list is not offered
//                             (a build that stopped part-way, docs/launcher-suites.md)
//
// Pure apart from ManifestRead, ManifestReadEnvironment and InstallGameFolder;
// no game data (keys, ids and layer names).
// The DLL (src/game/psp_code.cpp, dat_cache.cpp) and the launcher
// (src/launcher/suites.cpp, config.cpp, ui/page_jukebox.cpp) both link it.
#pragma once

#include <string>
#include <vector>

namespace bof3x {

struct CacheManifest {
    bool present = false;                        // a manifest was read
    std::vector<std::string> sources;            // [[source]] build, distinct, in order
    std::string base_policy;                     // [build] base_policy: "psp", "psx", or "" (absent: psx)
    std::vector<std::string> exe_tables_from;    // [build] exe_tables_from (a build or a list); else [exe] build
    std::string bgm_build;                       // the build of bgm's first row (base/bgm/), or ""
    std::string snd_build;                       // [snd] build (base/snd/), or ""
    std::string snd_jingles_build;               // [snd_jingles] build (the PSX's jingles, DIV-0129), or ""
    std::string snd_jingles_dir;                 // [snd_jingles] dir (DIV-0150), or "" (base/snd)
    std::vector<std::string> opt_layers;         // the opt/ layers listed ([cache] opt, [psp_layers] rows), distinct
    std::string install_kind;                    // [build] install_kind: "pc", "disc", or "" (absent: an older cache)
    bool zh_rows = false;                        // [cache] assets holds a row of the loc/zh-CN layer
    // A value the reader cannot honour, as one sentence naming the key and
    // its value; empty when none. The launcher refuses to start on it and
    // the DLL Fatals with the same text (ManifestRefusal); the settings page
    // greys every suite with it.
    std::string error;
};

// The subset of TOML the importer writes: table headers, `key = "string"`,
// `key = ["a", "b"]` on one line, multi-line arrays of rows.
CacheManifest ManifestParse(const std::string& text);

// <cache>\manifest.toml; `present` false when `cache` is empty or the file
// is not there.
CacheManifest ManifestRead(const std::wstring& cache);

// The policy as a bool: true for "psp". Absent (or no manifest) is psx.
inline bool ManifestPspBase(const CacheManifest& m) { return m.error.empty() && m.base_policy == "psp"; }

// Whether `layer` may be offered from the cache's opt/: with no manifest, as
// before (the tree is its own evidence); with one, only a layer it lists.
bool ManifestOffersLayer(const CacheManifest& m, const std::string& layer);

// The refusal both sides give for a manifest with an error: the error and
// the file it is in (`where`, UTF-8). Empty when there is no error.
std::string ManifestRefusal(const CacheManifest& m, const std::string& where);

// ---- The install's kind (docs/exe-retire-round.md, X1) ----------------------
//
// "A PC install" was inferred from BOF3.exe beside the game; it is a fact the
// install records: [build] install_kind in the cache's manifest. The four
// questions that leaned on the file's presence are not one question, and each
// has its own call below. The cache and the game folder are two folders:
// where they disagree (a PC folder used with a cache a disc built), the
// cache's record decides: the game folder records nothing, and once
// BOF3.exe is no source the game's .data is the cache's image
// (docs/exe-retire-round.md section 6, call 3).

enum class InstallKind {
    kNone,   // no manifest: no cache, or a cache with no manifest.toml
    kPc,     // the PC game's DAT\ was a source of the cache
    kDisc,   // it was not: a disc (PlayStation or PSP) is the base
};

// The kind the cache records, or for a manifest from before install_kind
// what that manifest holds: a [cache] assets row of the loc/zh-CN layer is
// the PC's (no disc carries that layer), none is a disc's.
InstallKind ManifestInstallKind(const CacheManifest& m);

// Whether the kind was answered from an older manifest's rows: a manifest
// is there and has no install_kind.
inline bool ManifestKindFromRows(const CacheManifest& m) { return m.present && m.install_kind.empty(); }

// The line a caller logs when the kind was answered from an older
// manifest's rows (ManifestKindFromRows), the first time this process asks;
// empty every other time, and when the kind is recorded.
std::string ManifestKindNoteOnce(const CacheManifest& m);

// What a caller may know of BOF3.exe, and what it answers with no manifest.
//   developer    BOF3X_EXEFILE=1, or the hosted build (the engine asks
//                bof3::Host(); the launcher passes the switch). Only then is
//                BOF3.exe asked: today's test, `exe_present`.
//   exe_present  the file is beside the game (the launcher) or the engine
//                opened it (ExeFile_Present). Not read unless `developer`.
//   no_cache     the answer when there is no manifest. The launcher passes
//                true: with no cache only a PC folder can start (a disc-built
//                install has no .data without its cache), and a player's
//                start sets such a folder up first. The engine passes
//                ExeFile_Present(): with no cache the file it opened is the
//                only source of .data, so that is what it asks, not the folder.
struct InstallAsk {
    bool developer = false;
    bool exe_present = false;
    bool no_cache = false;
};

// Whether the environment holds the developer's switch, BOF3X_EXEFILE=1.
bool InstallExeFileSwitch();

// The engine's ask: `hosted` (!bof3::IsExeHost(): the build that runs inside
// BOF3.exe) or the switch make it the developer's; `exe_opened` is
// ExeFile_Present(), both what the developer's test reads and the answer
// with no cache.
inline InstallAsk InstallAskEngine(bool hosted, bool exe_opened) {
    InstallAsk ask;
    ask.developer = hosted || InstallExeFileSwitch();
    ask.exe_present = exe_opened;
    ask.no_cache = exe_opened;
    return ask;
}

// Rows 12 and 13 (BOF3X_CACHE_DATA=0): the game folder's DAT\ is the PC's
// own - the disc probe is Capcom's to decide (DIV-0100), the game folder can
// be read in place of the cache.
bool InstallPcDat(const CacheManifest& m, const InstallAsk& ask);

// Row 11 (DIV-0102): the port's Chinese text can be read - the PC's DAT
// folder (InstallPcDat), or the cache's loc\zh-CN layer, `zh_layer`, the caller's
// own test of the cache's folder (dat_cache::HasLayer, CacheHasLayer).
bool InstallChineseText(const CacheManifest& m, bool zh_layer, const InstallAsk& ask);

// Row 13 (BOF3X_EXEIMAGE_STRICT): .data has no gap - the cache's image was
// built from BOF3.exe (exe_tables_from pc-zh). A cache with no image recorded
// (from before [exe]) answers as InstallPcDat.
bool InstallImageWhole(const CacheManifest& m, const InstallAsk& ask);

// Row 1: `dir` is a game folder - it holds DAT\FIRST.DAT, the test
// importer.py install makes (a PC folder and a disc-built game folder alike).
// Not a test of whose DAT\ it is: that is InstallPcDat.
bool InstallGameFolder(const std::wstring& dir);

// The engine's cache: the manifest of BOF3X_CACHE, else of BOF3X_EXEIMAGE
// (a disc-built image named alone), and its path into `where` (UTF-8, for a
// refusal). `present` false when neither is set or names no manifest.
CacheManifest ManifestReadEnvironment(std::string* where);

}  // namespace bof3x

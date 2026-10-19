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
//   [cache] bgm               the first row's source build (base/bgm/)
//   [snd] build               base/snd/'s build
//   [snd_jingles] build       the build of base/snd/'s 11 jingles on a PSX-only install (DIV-0129)
//   [cache] opt, [psp_layers] rows
//                             the opt/ layers the manifest lists: an opt/<layer>/
//                             tree the manifest does not list is not offered
//                             (a build that stopped part-way, docs/launcher-suites.md)
//
// Pure apart from ManifestRead; no game data (keys, ids and layer names).
// The DLL (src/game/psp_code.cpp, dat_cache.cpp) and the launcher
// (src/launcher/suites.cpp, config.cpp, music_player.cpp) both link it.
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
    std::string snd_jingles_build;               // [snd_jingles] build (base/snd/'s jingles, DIV-0129), or ""
    std::vector<std::string> opt_layers;         // the opt/ layers listed ([cache] opt, [psp_layers] rows), distinct
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

}  // namespace bof3x

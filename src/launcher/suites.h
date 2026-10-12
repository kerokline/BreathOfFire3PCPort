// The settings dialog's three suites - graphics, audio, data - each the
// PlayStation's version of a set of differences or the PSP's, all together
// (the owner's calls of 2026-10-10, docs/psp-only-build.md section 7;
// docs/launcher-suites.md). The catalogue is recipes/suites.toml, compiled in
// as suites.gen.h (tools/suites.py gen). What an install can offer comes from
// the importer's cache: its manifest.toml's sources, and its [build] keys
// base_policy and exe_tables_from (docs/psp-only-round.md section 4).
//
// The ini has no line of its own for a suite: a suite is the opt/ layers it
// moves, so the dialog writes it as opt= (DIV-0086) through ConfigOptEdit,
// and an untouched page writes opt= back byte for byte. music= stays the
// choice between the cache's songs and the PC's MP3s (DIV-0087).
#pragma once

#include <functional>
#include <string>
#include <vector>

#include "launcher/suites.gen.h"

namespace bof3x {

// The cache's manifest.toml, as far as the suites read it. Absent keys stay
// empty. A value the page cannot honour sets `error`; the page then greys every
// suite with it rather than guess.
struct CacheManifest {
    bool present = false;                        // the file was read
    std::vector<std::string> sources;            // [[source]] build, distinct, in order
    std::string base_policy;                     // [build] base_policy: "psp", "psx", or "" (absent: psx)
    std::vector<std::string> exe_tables_from;    // [build] exe_tables_from (a build or a list); else [exe] build
    std::string bgm_build;                       // the build of bgm's first row (base/bgm/), or ""
    std::string snd_build;                       // [snd] build (base/snd/), or ""
    std::string error;
};

// The subset of TOML the importer writes (tools/importer.py write_manifest):
// table headers, `key = "string"`, `key = ["a", "b"]` on one line, multi-line
// arrays of rows. Pure, for the host tests.
CacheManifest ManifestParse(const std::string& text);
// <cache>\manifest.toml; `present` false when there is no cache or no file.
CacheManifest ManifestRead(const std::wstring& cache);

// fixtures.toml's role of a build id, through the generated table; false for
// an id it does not hold.
bool SuiteBuildRole(const std::string& id, suites_gen::Role& role);

enum class SuiteSide { kPsx, kPsp, kMixed };

struct SuiteMemberState {
    const suites_gen::MemberInfo* info = nullptr;
    // Beside a PlayStation base: whether choosing the PSP's version plays it.
    // On a PSP base: whether the cache holds it (it is the base either way).
    bool available = false;
    std::wstring note;   // why it is greyed, or a qualifier ("plays under en only")
};

struct SuiteState {
    const suites_gen::SuiteInfo* info = nullptr;
    bool psp_base = false;               // base_policy = "psp": the PSP's version is the base
    bool psx_enabled = false, psp_enabled = false;
    SuiteSide side = SuiteSide::kPsx;    // what opt= plays now; kPsp on a PSP base
    std::wstring psx_why, psp_why;       // a greyed side's reason, naming the discs that would enable it
    std::vector<SuiteMemberState> members;
    std::vector<std::string> layers_on;   // choosing the PSP's version adds these (the members installed)
    std::vector<std::string> layers_all;  // choosing the PlayStation's removes these (every member layer)
};

struct SuiteInputs {
    CacheManifest manifest;
    // Whether an opt/ layer can be played: DAT\<layer>.*.DAT or the cache's
    // opt\<layer>\dat\ (ConfigOptInstalled).
    std::function<bool(const char* layer)> installed;
    std::string language;                // the Language box's tag; "" or "original" for none
    std::string opt;                     // the ini's opt= as read
    std::vector<std::string> defaults;   // ConfigOptDefaultInstalled
};

// Every suite of the catalogue, in its order, whatever the install.
std::vector<SuiteState> SuitesEvaluate(const SuiteInputs& in);

// opt= after the page. `choice[i]` is -1 for a suite left as it was, 0 for
// the PlayStation's version, 1 for the PSP's. Only a choice beside a
// PlayStation base, on an enabled side, changes anything; the rest is
// ConfigOptEdit's (an untouched page returns `opt` byte for byte).
std::string SuitesOptEdit(const std::string& opt, const std::vector<std::string>& defaults,
                          const std::vector<SuiteState>& suites, const std::vector<int>& choice);

// The opt/ layers of the catalogue, in its order: the order a list the dialog
// writes puts them in (ConfigOptEdit).
std::vector<std::string> SuiteLayers();

}  // namespace bof3x

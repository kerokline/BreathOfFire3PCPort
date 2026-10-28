#include "input/sigma.h"

#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "common/cache_manifest.h"
#include "common/image_base.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/bindings.h"

// docs/button-sigma.md. The pure half (ReleaseMap, DefaultWords,
// ReleaseOfSources, Build, Apply) holds no game data: the buttons are named by
// input::Action and the two releases' maps are the permutations
// button-roles-by-release.md 2 measured, stated as rules (which line moves to
// which button), not copied tables. The live half reads and writes the nine
// pad words at 0x903580 (save data, docs/controls.md 2.1).

namespace bof3x::input::sigma {

const unsigned char kLineWord[kLines] = {0, 1, 6, 2, 3, 4};   // 0x6536F0's line-to-slot table
const char* const kLineName[kLines] = {"Speak", "Move", "Action", "Menu", "View", "Change"};

namespace {

constexpr ::bof3::image::Va kWords{0x903580};   // the nine pad words (word 7 Field_ConfirmButtons, 8 Field_CancelButtons)
constexpr ::bof3::image::Va kBootDefaults{0x656AEC};   // Boot_Task's source, the engine's rule (exe_own.cpp)

// Japan's button for each line, in kLineWord's order (Init 0x460FDE, the rule
// FieldButtonDefaults in exe_own.cpp, SLPS_009.90's Boot_Init copy).
constexpr std::uint16_t kJapanLine[kLines] = {kCircle, kSquare, kCross, kTriangle, kR1, kL1};

// The maps from Japan's buttons (button-roles-by-release.md 2), each as the
// four face buttons' images; R1 and L1 are the same on every release.
//   P (US, EU, FR, DE): circle -> cross -> triangle -> square -> circle.
//   S (PSP-EU): circle <-> cross, triangle <-> square.
std::uint16_t MapP(std::uint16_t b) {
    switch (b) {
        case kCircle: return kCross;
        case kCross: return kTriangle;
        case kTriangle: return kSquare;
        case kSquare: return kCircle;
        default: return b;
    }
}
std::uint16_t MapS(std::uint16_t b) {
    switch (b) {
        case kCircle: return kCross;
        case kCross: return kCircle;
        case kTriangle: return kSquare;
        case kSquare: return kTriangle;
        default: return b;
    }
}

bool OneOfSix(std::uint16_t b) { return b != 0 && (b & (b - 1)) == 0 && (b & kSixBits) == b; }
unsigned Index(std::uint16_t b) {   // a single bit's index, b one bit
    unsigned i = 0;
    while ((b >> i) != 1) ++i;
    return i;
}

// The PC counts as a JP source: its code and its defaults are Japan's
// (button-roles-by-release.md 8, answer 2; confirmed by the owner 2026-10-17:
// a PC install with a Western layer or a Western disc starts with Japan's
// buttons). The one line to change for the other reading.
constexpr bool kPcIsJapanSource = true;

// --- the live state ---

// DIVERGENCE: DIV-0133 (docs/button-sigma.md 5) - 1 after every
// module's self-test (PatchBytes "ButtonSigma").
unsigned char g_sigma_on = 0;
// DIVERGENCE: DIV-0134 - 1 after every module's self-test
// (PatchBytes "StartConfigBySource").
unsigned char g_by_source = 0;
bool g_armed = false;
Release g_release = Release::kJapan;
Table g_table;
// The release's starting words stand: set where WriteStart puts them in,
// cleared when a save's words are loaded (RecipeToPad, sigma.h).
bool g_release_words = false;

std::uint16_t* Words() { return reinterpret_cast<std::uint16_t*>(static_cast<std::uintptr_t>(::bof3::Img(kWords.v))); }

// Words 7 and 8 the Speak line's and the Action line's, then the table; a
// Config that is not a permutation of the six bits a Fatal naming the writer.
void Follow(const char* writer, const char* remedy) {
    std::uint16_t* const w = Words();
    w[7] = w[0];
    w[8] = w[6];
    std::string why;
    g_table = Build(w, why);
    if (!g_table.valid)
        bof3::Fatal("ButtonSigma: after %s the Config's six lines are not one button each (%s); not guessed at (%s, or "
              "BOF3X_ORIGINAL=ButtonSigma for the original's flows)", writer, why.c_str(), remedy);
}

void WriteStart(bool reset) {
    std::uint16_t words[9];
    DefaultWords(g_release, reset, words);
    std::memcpy(Words(), words, sizeof words);
    if (!g_release_words)
        bof3::Log("sigma       %s starting words stand: a recipe's buttons pressed as %s from here (input-script.md 2a)",
                  ReleaseName(g_release), ReleaseName(g_release));
    g_release_words = true;
}

std::uint16_t MapMask(std::uint16_t mask, bool inverse) {
    std::uint16_t out = static_cast<std::uint16_t>(mask & ~kSixBits);
    for (unsigned i = 2; i < 8; ++i) {
        const auto b = static_cast<std::uint16_t>(1u << i);
        if (mask & b) out = static_cast<std::uint16_t>(out | (inverse ? ReleaseMapInverse(g_release, b) : ReleaseMap(g_release, b)));
    }
    return out;
}

// The cache named by BOF3X_CACHE, read through the one manifest reader the
// launcher shares; no cache or no manifest: a PC install, Japan.
Release ReadInstallRelease(std::string& how) {
    wchar_t root[1024];
    const DWORD n = GetEnvironmentVariableW(L"BOF3X_CACHE", root, 1024);
    if (n == 0 || n >= 1024) {
        how = "no cache named: the PC's own data";
        return Release::kJapan;
    }
    std::wstring path(root, n);
    while (!path.empty() && (path.back() == L'\\' || path.back() == L'/')) path.pop_back();
    const std::string where = bof3::Utf8Path((path + L"\\manifest.toml").c_str()).text;
    const CacheManifest m = ManifestRead(path);
    if (!m.present) {
        how = "the cache has no manifest: the PC's own data";
        return Release::kJapan;
    }
    if (!m.error.empty()) bof3::Fatal("StartConfigBySource: %s", ManifestRefusal(m, where).c_str());
    std::string error;
    const Release r = ReleaseOfSources(m.sources, error);
    if (!error.empty()) bof3::Fatal("StartConfigBySource: %s (%s)", error.c_str(), where.c_str());
    how = "the cache's sources:";
    for (const std::string& s : m.sources) how += " " + s;
    return r;
}

// --- the self-test (BOF3X_SHADOW=sigma) ---

unsigned g_bad = 0;
void Check(bool ok, const char* what, unsigned a = 0, unsigned b = 0) {
    if (ok) return;
    if (g_bad < 20) bof3::Log("shadow      sigma MISMATCH: %s (0x%X, 0x%X)", what, a, b);
    ++g_bad;
}

// σ a permutation of the six bits: onto, and its inverse undoes it.
void CheckPermutation(const Table& t) {
    std::uint16_t seen = 0;
    for (unsigned i = 2; i < 8; ++i) {
        const std::uint16_t b = static_cast<std::uint16_t>(1u << i);
        const std::uint16_t s = Apply(t, b);
        Check(OneOfSix(s), "sigma leaves the six bits", b, s);
        seen = static_cast<std::uint16_t>(seen | s);
        Check(ApplyInverse(t, s) == b, "the inverse does not undo sigma", b, s);
        Check(Apply(t, ApplyInverse(t, b)) == b, "sigma does not undo the inverse", b, ApplyInverse(t, b));
    }
    Check(seen == kSixBits, "sigma is not onto the six bits", seen);
    for (std::uint32_t m = 0; m < 0x10000; m += 0x0107)   // masks: the others pass through
        Check((Apply(t, static_cast<std::uint16_t>(m)) & ~kSixBits) == (m & ~kSixBits), "a bit outside the six moved", m);
}

void SelfTest() {
    g_bad = 0;
    unsigned tables = 0, refusals = 0;
    // 1. Every release's defaults: words 7 / 8 as the originals', σ = M_R,
    //    SigmaFrom(R) the identity. Japan's against the engine's region
    //    Boot_Task copies (exe_own.cpp, equal to BOF3.exe's .data).
    std::uint16_t jp[9];
    DefaultWords(Release::kJapan, false, jp);
    Check(std::memcmp(jp, reinterpret_cast<const void*>(static_cast<std::uintptr_t>(::bof3::Img(kBootDefaults.v))), sizeof jp) == 0,
          "Japan's words differ from 0x656AEC's", jp[0]);
    // The two other releases as button-roles-by-release.md 2 read them from
    // the boot executables (Speak, Move, Menu, Action; confirm = Speak, cancel
    // = Action; R1, L1, select the same): stated by name.
    {
        std::uint16_t w[9];
        DefaultWords(Release::kWest, false, w);
        Check(w[0] == (kCross | kL2 | kR2) && w[1] == kCircle && w[2] == kSquare && w[6] == kTriangle && w[3] == kR1 &&
                  w[4] == kL1 && w[5] == kSelect,
              "the West's words", w[0], w[6]);
        DefaultWords(Release::kPspEu, false, w);
        Check(w[0] == (kCross | kL2 | kR2) && w[1] == kTriangle && w[2] == kSquare && w[6] == kCircle && w[3] == kR1 &&
                  w[4] == kL1 && w[5] == kSelect,
              "PSP-EU's words", w[0], w[6]);
    }
    for (Release r : {Release::kJapan, Release::kWest, Release::kPspEu})
        for (bool reset : {false, true}) {
            std::uint16_t w[9], followed[9];
            DefaultWords(r, reset, w);
            std::memcpy(followed, w, sizeof w);
            followed[7] = followed[0];
            followed[8] = followed[6];
            Check(std::memcmp(followed, w, sizeof w) == 0, "words 7 / 8 at the defaults differ from Speak's / Action's",
                  w[7], w[8]);
            Check((w[0] & kL2R2) == (reset ? 0 : kL2R2), "L2 / R2 in word 0", w[0]);
            std::string why;
            const Table t = Build(w, why);
            Check(t.valid, "a release's defaults refused");
            if (!t.valid) continue;
            ++tables;
            CheckPermutation(t);
            for (unsigned i = 2; i < 8; ++i) {
                const std::uint16_t b = static_cast<std::uint16_t>(1u << i);
                Check(Apply(t, b) == ReleaseMap(r, b), "sigma at R's defaults is not M_R", b, Apply(t, b));
                Check(Apply(t, ReleaseMapInverse(r, b)) == b, "SigmaFrom(R) at R's defaults is not the identity", b);
                Check(ReleaseMap(r, ReleaseMapInverse(r, b)) == b, "M_R's inverse", b);
            }
        }
    // 2. Every Config the rebind can make - all 720 assignments of the six
    //    lines to the six bits, from each release's words - a permutation;
    //    a swap of two lines exchanges their images and nothing else.
    const std::uint16_t six[kLines] = {kCircle, kSquare, kCross, kTriangle, kR1, kL1};
    for (Release r : {Release::kJapan, Release::kWest, Release::kPspEu}) {
        std::uint16_t perm[kLines];
        std::memcpy(perm, six, sizeof perm);
        std::sort(perm, perm + kLines);
        do {
            std::uint16_t w[9];
            DefaultWords(r, false, w);
            for (int l = 0; l < kLines; ++l)
                w[kLineWord[l]] = static_cast<std::uint16_t>(perm[l] | (kLineWord[l] == 0 ? kL2R2 : 0));
            std::string why;
            const Table t = Build(w, why);
            Check(t.valid, "a permutation refused", perm[0], perm[1]);
            if (!t.valid) continue;
            ++tables;
            CheckPermutation(t);
            for (int l = 0; l < kLines; ++l)
                Check(Apply(t, kJapanLine[l]) == perm[l], "sigma of a line's Japanese button is not its button now",
                      kJapanLine[l], perm[l]);
        } while (std::next_permutation(perm, perm + kLines));
        for (int a = 0; a < kLines; ++a)
            for (int b = a + 1; b < kLines; ++b) {
                std::uint16_t w[9];
                DefaultWords(r, false, w);
                std::string why;
                const Table before = Build(w, why);
                const std::uint16_t wa = w[kLineWord[a]] & kSixBits, wb = w[kLineWord[b]] & kSixBits;
                w[kLineWord[a]] = static_cast<std::uint16_t>((w[kLineWord[a]] & ~kSixBits) | wb);
                w[kLineWord[b]] = static_cast<std::uint16_t>((w[kLineWord[b]] & ~kSixBits) | wa);
                const Table t = Build(w, why);
                Check(t.valid, "a swap refused", a, b);
                if (!t.valid) continue;
                ++tables;
                for (int l = 0; l < kLines; ++l) {
                    const std::uint16_t want =
                        l == a ? Apply(before, kJapanLine[b]) : l == b ? Apply(before, kJapanLine[a]) : Apply(before, kJapanLine[l]);
                    Check(Apply(t, kJapanLine[l]) == want, "a swap moved a third line", a * 8 + b, l);
                }
            }
    }
    // 3. Not a permutation: refused, with a reason - two lines on one button,
    //    a line on nothing, on two buttons, on a bit outside the six.
    {
        struct Bad {
            int word;
            std::uint16_t value;
        };
        const Bad bad[] = {{1, kCircle}, {6, kCircle}, {2, kR1}, {3, 0}, {6, 0}, {1, kSquare | kCross}, {4, kSelect},
                           {2, kStart}, {1, kL2}, {6, kUp}, {0, kL2 | kR2}};
        for (const Bad& b : bad) {
            std::uint16_t w[9];
            DefaultWords(Release::kJapan, false, w);
            w[b.word] = b.value;
            std::string why;
            const Table t = Build(w, why);
            Check(!t.valid && !why.empty(), "a non-permutation accepted", b.word, b.value);
            refusals += t.valid ? 0u : 1u;
        }
    }
    // 4. The release by source, the order of answer 2.
    {
        struct Case {
            std::vector<std::string> sources;
            Release want;
            bool error;
        };
        const Case cases[] = {
            {{"pc-zh"}, Release::kJapan, false},
            {{"pc-zh", "psx-us", "psx-jp", "psx-eu-en", "psx-fr", "psx-de", "psp-eu", "psp-jp"}, Release::kJapan, false},
            {{"pc-zh", "psx-us"}, kPcIsJapanSource ? Release::kJapan : Release::kWest, false},
            {{"psx-us"}, Release::kWest, false},
            {{"psx-fr", "psp-eu"}, Release::kWest, false},
            {{"psp-eu", "psx-jp"}, Release::kJapan, false},
            {{"psp-eu"}, Release::kPspEu, false},
            {{"psp-jp"}, Release::kJapan, false},
            {{"psp-eu", "psp-jp"}, Release::kJapan, false},
            {{"psx-de"}, Release::kWest, false},
            {{"psx-eu-en"}, Release::kWest, false},
            {{}, Release::kJapan, true},
            {{"psx-us", "psx-xx"}, Release::kJapan, true},
        };
        for (const Case& c : cases) {
            std::string error;
            const Release r = ReleaseOfSources(c.sources, error);
            Check(r == c.want && error.empty() != c.error, "the release by source", static_cast<unsigned>(r),
                  static_cast<unsigned>(c.sources.size()));
        }
    }
    bof3::Log("shadow      sigma self-test: %u tables (each release's defaults and reset, 3 x 720 assignments, 3 x 15 swaps), "
              "each a permutation of the six bits with its inverse, M_R at R's defaults, words 7 / 8 equal to the "
              "originals at every default; %u non-permutations refused; the release by source over 13 cases; %u MISMATCHES",
              tables, refusals, g_bad);
    if (g_bad) bof3::Fatal("sigma: %u mismatches against docs/button-sigma.md", g_bad);
    if (tables != 6 + 3 * 720 + 3 * 15 || refusals != 11)
        bof3::Fatal("sigma self-test: %u tables, %u refusals (2211, 11 wanted)", tables, refusals);
}

}  // namespace

const char* ReleaseName(Release r) {
    switch (r) {
        case Release::kJapan: return "Japan's";
        case Release::kWest: return "the PlayStation West's (P)";
        case Release::kPspEu: return "PSP-EU's (S)";
    }
    bof3::Fatal("sigma: release %u", static_cast<unsigned>(r));
}

std::uint16_t ReleaseMap(Release r, std::uint16_t bit) {
    switch (r) {
        case Release::kJapan: return bit;
        case Release::kWest: return MapP(bit);
        case Release::kPspEu: return MapS(bit);
    }
    bof3::Fatal("sigma: release %u", static_cast<unsigned>(r));
}

std::uint16_t ReleaseMapInverse(Release r, std::uint16_t bit) {
    switch (r) {
        case Release::kJapan: return bit;
        case Release::kWest: return MapP(MapP(MapP(bit)));   // a 4-cycle: its inverse is its cube
        case Release::kPspEu: return MapS(bit);              // two swaps: its own inverse
    }
    bof3::Fatal("sigma: release %u", static_cast<unsigned>(r));
}

void DefaultWords(Release r, bool reset, std::uint16_t out[9]) {
    const std::uint16_t l2r2 = reset ? 0 : static_cast<std::uint16_t>(kL2 | kR2);
    std::uint16_t line[kLines];
    for (int l = 0; l < kLines; ++l) line[l] = ReleaseMap(r, kJapanLine[l]);
    out[0] = static_cast<std::uint16_t>(line[0] | l2r2);   // Speak
    out[1] = line[1];                                      // Move
    out[2] = line[3];                                      // Menu, Field_MenuButton
    out[3] = line[4];                                      // View
    out[4] = line[5];                                      // Change
    out[5] = kSelect;                                      // no line
    out[6] = line[2];                                      // Action
    out[7] = out[0];                                       // confirm, Field_ConfirmButtons
    out[8] = out[6];                                       // cancel, Field_CancelButtons
}

Release ReleaseOfSources(const std::vector<std::string>& sources, std::string& error) {
    if (sources.empty()) {
        error = "the cache's manifest names no [[source]] build";
        return Release::kJapan;
    }
    bool jp = false, west = false, psp_jp = false, psp_eu = false;
    for (const std::string& s : sources) {
        if (s == "pc-zh") jp = jp || kPcIsJapanSource;
        else if (s == "psx-jp") jp = true;
        else if (s == "psx-us" || s == "psx-eu-en" || s == "psx-fr" || s == "psx-de") west = true;
        else if (s == "psp-jp") psp_jp = true;
        else if (s == "psp-eu") psp_eu = true;
        else {
            error = "the cache's manifest names a source build \"" + s + "\" whose button defaults are not known "
                    "(docs/button-sigma.md 3)";
            return Release::kJapan;
        }
    }
    if (jp) return Release::kJapan;
    if (west) return Release::kWest;
    if (psp_jp) return Release::kJapan;
    if (psp_eu) return Release::kPspEu;
    // Only pc-zh with kPcIsJapanSource false: the PC's data and no disc.
    return Release::kJapan;
}

Table Build(const std::uint16_t words[9], std::string& why) {
    Table t;
    std::uint16_t seen = 0;
    char buf[160];
    for (int l = 0; l < kLines; ++l) {
        const std::uint16_t raw = words[kLineWord[l]];
        const std::uint16_t b = kLineWord[l] == 0 ? static_cast<std::uint16_t>(raw & ~kL2R2) : raw;
        if (!OneOfSix(b)) {
            std::snprintf(buf, sizeof buf, "%s's word %u is 0x%04X, not one of the six buttons", kLineName[l],
                          kLineWord[l], raw);
            why = buf;
            return Table();
        }
        if (seen & b) {
            std::snprintf(buf, sizeof buf, "%s's word %u is 0x%04X, a button another line is on", kLineName[l],
                          kLineWord[l], raw);
            why = buf;
            return Table();
        }
        seen = static_cast<std::uint16_t>(seen | b);
        t.now[Index(kJapanLine[l])] = b;
        t.back[Index(b)] = kJapanLine[l];
    }
    t.valid = true;
    return t;
}

std::uint16_t Apply(const Table& t, std::uint16_t mask) {
    std::uint16_t out = static_cast<std::uint16_t>(mask & ~kSixBits);
    for (unsigned i = 2; i < 8; ++i)
        if (mask & (1u << i)) out = static_cast<std::uint16_t>(out | t.now[i]);
    return out;
}

std::uint16_t ApplyInverse(const Table& t, std::uint16_t mask) {
    std::uint16_t out = static_cast<std::uint16_t>(mask & ~kSixBits);
    for (unsigned i = 2; i < 8; ++i)
        if (mask & (1u << i)) out = static_cast<std::uint16_t>(out | t.back[i]);
    return out;
}

std::uint16_t Sigma(std::uint16_t bits) {
    if (!g_armed || g_sigma_on == 0) return bits;   // the original: Japan's bit itself
    if (!g_table.valid) bof3::Fatal("Sigma(0x%04X) asked before any writer built the table", bits);
    return Apply(g_table, bits);
}

std::uint16_t SigmaInverse(std::uint16_t bits) {
    if (!g_armed || g_sigma_on == 0) return bits;
    if (!g_table.valid) bof3::Fatal("SigmaInverse(0x%04X) asked before any writer built the table", bits);
    return ApplyInverse(g_table, bits);
}

std::uint16_t SigmaFrom(Release r, std::uint16_t bits) {
    std::uint16_t jp = static_cast<std::uint16_t>(bits & ~kSixBits);
    for (unsigned i = 2; i < 8; ++i)
        if (bits & (1u << i)) jp = static_cast<std::uint16_t>(jp | ReleaseMapInverse(r, static_cast<std::uint16_t>(1u << i)));
    return Sigma(jp);
}

Release InstallRelease() { return g_release; }

std::uint16_t RecipeToPad(std::uint16_t mask) { return g_release_words ? MapMask(mask, false) : mask; }
std::uint16_t PadToRecipe(std::uint16_t mask) { return g_release_words ? MapMask(mask, true) : mask; }
bool RecipeTranslating() { return g_release_words; }

void BootDefaultsWritten() {
    if (!g_armed) return;
    if (g_by_source != 0 && g_release != Release::kJapan) WriteStart(false);
    if (g_sigma_on != 0) Follow("Boot_Task's defaults", "the defaults themselves: a fault of ours");
}

void LoadApplied(bool reset) {
    if (!g_armed) return;
    if (reset && g_by_source != 0 && g_release != Release::kJapan) WriteStart(true);
    else if (g_release_words) {
        g_release_words = false;   // the save's words: the same save, the same words, on every install
        bof3::Log("sigma       the save's words loaded: a recipe's buttons pressed as recorded from here");
    }
    if (g_sigma_on != 0) Follow("loading the save", "hold Start while the save loads to reset the buttons");
}

void Rebound() {
    if (!g_armed) return;
    if (g_sigma_on != 0) Follow("the Config screen's rebind", "the rebind only swaps: a fault of ours");
}

void SnapshotRestored(bool release_words) {
    g_release_words = release_words;
    if (!g_armed) return;
    std::string why;
    g_table = Build(Words(), why);
    if (g_sigma_on != 0 && !g_table.valid)
        bof3::Fatal("ButtonSigma: after a quickload the Config's six lines are not one button each (%s)", why.c_str());
}

void Arm() {
    if (bof3::WantsShadow("sigma")) SelfTest();
    std::string how;
    g_release = ReadInstallRelease(how);
    static const std::uint8_t was = 0, is = 1;
    bof3::PatchBytes("ButtonSigma", static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&g_sigma_on)), &was, &is, 1);
    bof3::PatchBytes("StartConfigBySource", static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&g_by_source)), &was,
                     &is, 1);
    g_armed = true;
    // A table now if the words already hold a Config (an inject after boot is
    // not a path the game takes, but the words are read, not assumed).
    std::string why;
    g_table = Build(Words(), why);
    bof3::Log("ButtonSigma the Config's lines as one permutation, confirm on Speak's button and cancel on Action's: %s",
              g_sigma_on ? "on" : "OFF (BOF3X_ORIGINAL=ButtonSigma)");
    bof3::Log("StartConfig the starting Config by source: %s, %s (%s)", ReleaseName(g_release),
              g_by_source ? "on" : "OFF (BOF3X_ORIGINAL=StartConfigBySource)", how.c_str());
}

}  // namespace bof3x::input::sigma

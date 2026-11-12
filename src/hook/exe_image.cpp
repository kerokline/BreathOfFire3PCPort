// BOF3X_EXEIMAGE (exe_image.h; docs/exe-import-engine.md section 4.4).
#include "hook/exe_image.h"

#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "common/image_base.h"
#include "common/sha256.h"
#include "hook/exe_host.h"
#include "hook/exe_layout.h"
#include "hook/exe_own.h"
#include "hook/log.h"
#include "os/os.h"

namespace bof3 {
namespace {

struct Range {
    std::uint32_t lo, hi;
    std::string how, from;
};

// One row of data.toml's `accepted` list (tools/exe_tables.py write_exe since
// the exe-gap round's group ST): the answer recipes/exe-gaps-accepted.tsv
// gives the `gap` range [lo, hi) - layer, port-own, unread or pending.
struct Accepted {
    std::uint32_t lo, hi;
    std::string answer;
};

struct Image {
    std::uint32_t va = 0, size = 0, bss_end = 0;
    std::string build, file, gaps;
    std::vector<Range> ranges;
    bool has_accepted = false;   // the `accepted = [` line: absent in an image written before the list
    std::vector<Accepted> accepted;
};

// data.toml's classes (tools/exe_tables.py write_exe), what the engine does
// with each in a disc-built image, and whether the bytes must then equal the
// PC's .data (the comparison when BOF3.exe is there too):
//   map, place, table, widen       copied (a disc's numbers may differ: logged)
//   file                           a tables.toml table: zero in data.bin, laid from tables\<key>.bin
//                                  by ComposeBase before the copy; then as `table`
//   exe                            copied; must equal the file's when it is there (only a pc-zh image has it)
//   rebuilt, layout                copied; the PC's pointer by construction (must equal)
//   own                            filled by ExeOwn_Fill after the copy (must equal)
//   keys                           Key_TableDefault, written by Shell_MapDefaultKeys (must equal)
//   none                           zero; recipes/exe-gaps.tsv says the PC's are zero too (must equal)
//   language                       zero; a language layer's handlers write it, or ours read around it
//   unread                         zero; nothing the exe host runs reads it
//   gap                            zero where the PC has a value no disc carries: logged, each range
//                                  once with its answer from data.toml's `accepted` list; a gap with
//                                  no answer is a Fatal (BOF3X_EXEIMAGE_STRICT, StrictMode below)
//   pointer                        a pointer word left unfilled: a Fatal (no disc build has one)
struct Class {
    const char* how;
    bool must_equal;
};
constexpr Class kClasses[] = {{"exe", true},     {"map", false},     {"place", false},   {"table", false},
                              {"widen", false},  {"rebuilt", true},  {"layout", true},   {"own", true},
                              {"keys", true},    {"none", true},     {"language", false}, {"unread", false},
                              {"gap", false},    {"pointer", true},  {"file", false}};

const Class* ClassOf(const Range& r) {
    for (const Class& c : kClasses)
        if (r.how == c.how) return &c;
    Fatal("BOF3X_EXEIMAGE: data.toml range 0x%X has an unknown how \"%s\"", (unsigned)r.lo, r.how.c_str());
}

// Bytes every disc build carries since an importer fix, which an image written
// before that fix left as a `gap` - zero where the engine now reads a value it
// cannot do without. Such an image is refused by name rather than mapped: a
// player's cache from an older release would otherwise run into the defect the
// fix removed (D305: a 2026-10-12 afternoon PSX-US cache crashing on entering
// area 16). A row per fix, when one changes what a disc build must hold;
// `effect` is what the player would see without it, for the refusal's message.
struct Since {
    std::uint32_t lo, hi;
    const char* what;
    const char* effect;
};
std::vector<Since> FilledSince() {
    std::vector<Since> out;
    // tools/psp_exe.py wmrec (2026-10-12, docs/visual-findings-1012.md section 8):
    // WorldMap_Records 0x653910, eleven 0x1C-byte records, the world map's area
    // word at +0x18. Zero, WorldMap_RecordIndex finds no world map and answers
    // the "none" record, whose effect kind 1 calls MoveCmd_AttachOffset with
    // area 16's or 45's null vector table.
    for (std::uint32_t k = 0; k < 11; ++k) {
        const std::uint32_t at = 0x653910 + 0x1C * k + 0x18;
        out.push_back({at, at + 4, "WorldMap_Records' area words (the importer's wmrec rule of 2026-10-12)",
                       "it would crash on entering a world map area"});
    }
    // tools/exe_tables.py's places by the pointers into a region (2026-10-16,
    // docs/exe-import-engine.md 7.7, IDEAS.md I43): party members 7 and 9's
    // animation records, which the member table 0x669884 points at. Zero, every
    // pose of theirs reads bank 0 and animation 0 (sprite_pose.cpp
    // Sprite_SetAnimationAt): the forest sprite after the mine drawn in pieces.
    out.push_back({0x669868, 0x669883, "two party members' animation records (the importer's places of 2026-10-16)",
                   "a character's field sprite would be drawn in pieces"});
    return out;
}

// The disc-built image ExeImage_Source mapped, kept for ExeImage_CompareWithFile.
Image g_mapped;
bool g_disc_mapped = false;

// BOF3X_EXEIMAGE's directory, read with the wide API as the launcher sets it
// and held as UTF-8 (the log's and Fatal's text); "" when unset. A path
// outside the ANSI code page reads whole (docs/disc-only-round-review.md 2).
std::string ImageDir() {
    wchar_t w[MAX_PATH];
    const DWORD n = bof3::os::GetEnvW(L"BOF3X_EXEIMAGE", w, MAX_PATH);
    if (n == 0) return std::string();
    if (n >= MAX_PATH) Fatal("BOF3X_EXEIMAGE: the path is longer than %u", (unsigned)MAX_PATH);
    const int len = WideCharToMultiByte(CP_UTF8, 0, w, static_cast<int>(n), nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<std::size_t>(len > 0 ? len : 0), '\0');
    if (len <= 0 || WideCharToMultiByte(CP_UTF8, 0, w, static_cast<int>(n), &out[0], len, nullptr, nullptr) != len)
        Fatal("BOF3X_EXEIMAGE: the path does not convert to UTF-8 (error %lu)", static_cast<unsigned long>(bof3::os::LastError()));
    return out;
}

// A file under ImageDir() (UTF-8) opened for reading through the wide API.
FILE* OpenUtf8(const std::string& path) {
    const int len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path.c_str(), -1, nullptr, 0);
    if (len <= 0) Fatal("BOF3X_EXEIMAGE: %s is not UTF-8", path.c_str());
    std::wstring w(static_cast<std::size_t>(len), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path.c_str(), -1, &w[0], len);
    return _wfopen(w.c_str(), L"rb");
}

// data.toml as tools/exe_tables.py writes it: `key = value` lines in [image],
// then one `[0x.., 0x.., "how", "from"],` line a range, then on a disc build
// the `accepted = [` list, one `["answer", 0x.., 0x..],` line a gap range it
// answers. Only that shape; `path` names it in a Fatal.
Image ParseToml(const std::string& text, const std::string& path) {
    Image im;
    std::size_t at = 0;
    while (at < text.size()) {
        std::size_t end = text.find('\n', at);
        if (end == std::string::npos) end = text.size();
        const std::string line = text.substr(at, end - at);
        at = end + 1;
        if (line.size() >= 1024)
            Fatal("BOF3X_EXEIMAGE: %s has a line of %u characters", path.c_str(), (unsigned)line.size());
        unsigned a = 0, b = 0;
        char s[64] = {0}, from[256] = {0};
        const char* p = line.c_str();
        while (*p == ' ') ++p;
        const int got = std::sscanf(p, "[0x%x, 0x%x, \"%63[^\"]\", \"%255[^\"]\"", &a, &b, s, from);
        if (std::strncmp(p, "accepted = [", 12) == 0) {
            im.has_accepted = true;
        } else if (std::sscanf(p, "[\"%63[^\"]\", 0x%x, 0x%x]", s, &a, &b) == 3) {
            if (!im.has_accepted) Fatal("BOF3X_EXEIMAGE: %s has an answer line outside its accepted list", path.c_str());
            im.accepted.push_back({a, b, s});
        } else if (got >= 3) {
            if (im.has_accepted) Fatal("BOF3X_EXEIMAGE: %s has a range after its accepted list", path.c_str());
            im.ranges.push_back({a, b, s, got == 4 ? from : ""});
        } else if (std::sscanf(p, "va = 0x%x", &a) == 1) {
            im.va = a;
        } else if (std::sscanf(p, "size = %u", &a) == 1) {
            im.size = a;
        } else if (std::sscanf(p, "bss_end = 0x%x", &a) == 1) {
            im.bss_end = a;
        } else if (std::sscanf(p, "build = \"%63[^\"]\"", s) == 1) {
            im.build = s;
        } else if (std::sscanf(p, "file = \"%63[^\"]\"", s) == 1) {
            im.file = s;
        } else if (std::sscanf(p, "gaps = \"%63[^\"]\"", s) == 1) {
            im.gaps = s;
        }
    }
    if (!im.va || !im.size || !im.bss_end || im.build.empty() || im.file.empty() || im.ranges.empty())
        Fatal("BOF3X_EXEIMAGE: %s is not a data.toml exe_tables.py wrote", path.c_str());
    return im;
}

std::string ReadWhole(const std::string& path) {
    FILE* f = OpenUtf8(path);
    if (!f) Fatal("BOF3X_EXEIMAGE: cannot open %s", path.c_str());
    std::string out;
    char buf[4096];
    std::size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) out.append(buf, n);
    std::fclose(f);
    return out;
}

Image ReadToml(const std::string& path) { return ParseToml(ReadWhole(path), path); }

// BOF3X_EXEIMAGE_STRICT: what a disc-built image's `gap` ranges may be
// (DIV-0101, amended by the exe-gap round's group ST, docs/exe-gap-round.md 4).
//   unset   the default: a gap with an answer in data.toml's accepted list
//           (recipes/exe-gaps-accepted.tsv) runs, logged; a gap with none, or
//           an image written before the list, is a Fatal that says to rebuild
//   0       every gap runs, logged, answered or not (the leniency before the
//           round: a developer's switch for an image under a new gap list)
//   1       every gap is a Fatal, answered or not: no game value of the PC's
//           runs as zero (the entry's off value, which the presets send)
// Any other value is a Fatal: a misspelt switch must not pick a mode.
enum class Strict { kAccepted, kLenient, kEvery };

Strict StrictMode() {
    char v[8] = {};
    const DWORD n = bof3::os::GetEnv("BOF3X_EXEIMAGE_STRICT", v, sizeof v);
    if (n == 0) return Strict::kAccepted;
    if (n < sizeof v && std::strcmp(v, "0") == 0) return Strict::kLenient;
    if (n < sizeof v && std::strcmp(v, "1") == 0) return Strict::kEvery;
    Fatal("BOF3X_EXEIMAGE_STRICT=%s: the switch takes 0 (every gap runs, logged) or 1 (every gap refused); unset, "
          "a gap recipes/exe-gaps-accepted.tsv does not answer is refused",
          n < sizeof v ? v : "(too long)");
}

const char* StrictName(Strict m) {
    return m == Strict::kAccepted  ? " unset: an unanswered gap refused"
           : m == Strict::kLenient ? "=0: every gap runs"
                                   : "=1: every gap refused";
}

// The answer data.toml's accepted list gives the gap range `r`, or null.
const char* AnswerOf(const Image& im, const Range& r) {
    for (const Accepted& a : im.accepted)
        if (a.lo == r.lo && a.hi == r.hi) return a.answer.c_str();
    return nullptr;
}

// The accepted list as exe_tables.py writes it: each row a known answer, on a
// `gap` range of the image, in the ranges' order, once. Empty when it is,
// else why not (a hand-edited or a damaged data.toml: a Fatal in every mode).
std::string AcceptedProblem(const Image& im) {
    char msg[256];
    std::uint32_t last = 0;
    for (const Accepted& a : im.accepted) {
        const bool known =
            a.answer == "layer" || a.answer == "port-own" || a.answer == "unread" || a.answer == "pending";
        bool on_gap = false;
        for (const Range& r : im.ranges) on_gap = on_gap || (r.how == "gap" && r.lo == a.lo && r.hi == a.hi);
        if (!known || !on_gap || a.lo < last) {
            std::snprintf(msg, sizeof msg, "the accepted row \"%s\" 0x%06X..0x%06X %s", a.answer.c_str(),
                          (unsigned)a.lo, (unsigned)a.hi,
                          !known    ? "is not an answer (layer, port-own, unread, pending)"
                          : !on_gap ? "is on no gap range of the image"
                                    : "is out of order or twice");
            return msg;
        }
        last = a.hi;
    }
    return std::string();
}

// Why a disc-built image is refused for its gaps under `mode`, or empty when
// it runs: the text of the Fatal, a player's lines first (as FilledSince()'s),
// then the engine's. `where` is the cache's base\exe\ directory.
std::string GapRefusal(const Image& im, Strict mode, const std::string& where) {
    if (mode == Strict::kLenient) return std::string();
    std::vector<char> msg(2048);
    std::uint32_t gaps = 0;
    for (const Range& r : im.ranges) gaps += r.how == "gap";
    if (mode == Strict::kAccepted && !im.has_accepted) {
        std::snprintf(msg.data(), msg.size(),
                      "This install's game data was built by an older release. This release must know which gaps of "
                      "the data it accepts, and the older release did not record them. Rebuild the install: in the "
                      "launcher's Sources page add the disc again, or run tools/importer.py build.\n\n"
                      "BOF3X_EXEIMAGE: %s (%s) has %u gap ranges and no accepted list (its data.toml was written "
                      "before recipes/exe-gaps-accepted.tsv); BOF3X_EXEIMAGE_STRICT=0 runs it with every gap logged",
                      where.c_str(), im.build.c_str(), (unsigned)gaps);
        return msg.data();
    }
    for (const Range& r : im.ranges) {
        if (r.how != "gap") continue;
        const char* answer = AnswerOf(im, r);
        if (mode == Strict::kEvery) {
            std::snprintf(msg.data(), msg.size(),
                          "BOF3X_EXEIMAGE_STRICT=1: 0x%06X..0x%06X (%u bytes, %s) of the %s image is a gap - the PC "
                          "has a value there that no disc carries - and the strict switch refuses an image with one%s%s%s",
                          (unsigned)r.lo, (unsigned)r.hi, (unsigned)(r.hi - r.lo), r.from.c_str(), im.build.c_str(),
                          answer ? " (accepted as " : "", answer ? answer : "", answer ? ")" : "");
            return msg.data();
        }
        if (answer) continue;
        std::snprintf(msg.data(), msg.size(),
                      "This install's game data has a gap that this release does not accept: some bytes are zero "
                      "where the PC has a value that no disc carries. Rebuild the install: in the launcher's Sources "
                      "page add the disc again, or run tools/importer.py build. If the problem stays after the "
                      "rebuild, the gap list of this release has a new row (tools/exe_scan.py --accepted names it).\n\n"
                      "BOF3X_EXEIMAGE: %s (%s) leaves 0x%06X..0x%06X (%u bytes, %s) a gap that its accepted list "
                      "does not answer (recipes/exe-gaps-accepted.tsv); BOF3X_EXEIMAGE_STRICT=0 runs it, logged",
                      where.c_str(), im.build.c_str(), (unsigned)r.lo, (unsigned)r.hi, (unsigned)(r.hi - r.lo),
                      r.from.c_str());
        return msg.data();
    }
    return std::string();
}

// The gate's negative control (BOF3X_SELFTEST_ONLY, the exe host): the parser
// and GapRefusal on a synthetic data.toml, then, when `mapped_text` is the
// disc-built image's own data.toml, on a copy of it with one gap's answer
// taken out - which the default must refuse, naming that range.
void StrictSelfTest(const std::string& mapped_text) {
    const std::string head =
        "[image]\nfile = \"data.bin\"\nva = 0x001000\nsize = 64\nbss_end = 0x001080\n"
        "build = \"psx-xx\"\ngaps = \"recipes/exe-gaps.tsv\"\n\nranges = [\n"
        "  [0x001000, 0x001010, \"map\", \"A@0x80000000\"],\n"
        "  [0x001010, 0x001018, \"gap\", \"game-value Sym_A\"],\n"
        "  [0x001018, 0x001020, \"none\", \"\"],\n"
        "  [0x001020, 0x001028, \"gap\", \"game-value Sym_B+0x4\"],\n"
        "  [0x001028, 0x001040, \"map\", \"A@0x80000028\"],\n]\n";
    const std::string full =
        head + "\naccepted = [\n  [\"layer\", 0x001010, 0x001018],\n  [\"pending\", 0x001020, 0x001028],\n]\n";
    const std::string planted = head + "\naccepted = [\n  [\"layer\", 0x001010, 0x001018],\n]\n";
    const std::string w = "selftest\\base\\exe\\";
    unsigned ok = 0, n = 0;
    const auto check = [&](bool good, const char* what) {
        ++n;
        if (good) ++ok;
        else Log("exe_image   STRICT self-test: WRONG - %s", what);
    };
    const Image a = ParseToml(full, "synthetic"), b = ParseToml(planted, "synthetic"), c = ParseToml(head, "synthetic");
    check(a.has_accepted && a.accepted.size() == 2 && a.ranges.size() == 5 && AcceptedProblem(a).empty(),
          "the synthetic image parsed: 5 ranges, 2 answers");
    check(!c.has_accepted && c.accepted.empty() && c.ranges.size() == 5, "an image without the list parsed as old");
    check(GapRefusal(a, Strict::kAccepted, w).empty(), "every gap answered: runs under the default");
    const std::string refused = GapRefusal(b, Strict::kAccepted, w);
    check(refused.find("0x001020..0x001028") != std::string::npos && refused.find("Sym_B+0x4") != std::string::npos &&
              refused.find("psx-xx") != std::string::npos && refused.find("Rebuild the install") != std::string::npos,
          "the planted gap refused under the default, by range, symbol and build, with the rebuild line");
    check(GapRefusal(c, Strict::kAccepted, w).find("older release") != std::string::npos,
          "an image written before the list refused under the default");
    check(GapRefusal(b, Strict::kLenient, w).empty() && GapRefusal(c, Strict::kLenient, w).empty(),
          "BOF3X_EXEIMAGE_STRICT=0 runs the planted and the old image");
    check(GapRefusal(a, Strict::kEvery, w).find("0x001010..0x001018") != std::string::npos,
          "BOF3X_EXEIMAGE_STRICT=1 refuses an answered gap");
    Image d = a;
    d.accepted[1].answer = "maybe";
    Image e = a;
    e.accepted[1].lo = 0x1018;
    e.accepted[1].hi = 0x1020;
    Image f = a;
    f.accepted.push_back(a.accepted[0]);
    check(!AcceptedProblem(d).empty() && !AcceptedProblem(e).empty() && !AcceptedProblem(f).empty(),
          "an unknown answer, an answer on a range that is not a gap, an answer twice: each refused");
    std::string mapped = "no disc-built image mapped";
    if (!mapped_text.empty()) {
        // A copy of the image's own data.toml with its first answer line taken
        // out - when the image itself runs under the default; one that does not
        // is refused by ExeImage_Source right after this test, by its own range.
        const Image m = ParseToml(mapped_text, "the mapped data.toml");
        const std::size_t list = mapped_text.find("\naccepted = [\n");
        const std::size_t row = list == std::string::npos ? list : mapped_text.find("\n  [\"", list + 1);
        if (!GapRefusal(m, Strict::kAccepted, w).empty()) {
            mapped = "the mapped " + m.build + " image is itself refused under the default, so nothing is planted in it";
        } else if (row != std::string::npos) {
            const std::size_t row_end = mapped_text.find('\n', row + 1);
            unsigned lo = 0, hi = 0;
            char answer[64] = {};
            std::sscanf(mapped_text.c_str() + row + 3, "[\"%63[^\"]\", 0x%x, 0x%x]", answer, &lo, &hi);
            std::string copy = mapped_text;
            copy.erase(row, row_end - row);
            const Image p = ParseToml(copy, "a copy of the mapped data.toml");
            char range[32];
            std::snprintf(range, sizeof range, "0x%06X..0x%06X", lo, hi);
            const std::string why = GapRefusal(p, Strict::kAccepted, w);
            check(p.accepted.size() + 1 == m.accepted.size() && why.find(range) != std::string::npos &&
                      why.find(m.build) != std::string::npos,
                  "a copy of the mapped image with one answer taken out refused, naming that range");
            mapped = "a copy of the mapped " + m.build + " image's data.toml with the answer of " + range + " (" +
                     answer + ") taken out refused, naming it";
        } else {
            mapped = "the mapped " + m.build + " image has no answered gap to plant";
        }
    }
    if (ok != n) Fatal("exe_image STRICT self-test: %u of %u checks right (each WRONG line above)", ok, n);
    Log("exe_image   STRICT self-test: %u checks right - a synthetic data.toml parsed with its accepted list; an "
        "unanswered gap and an image without the list refused under the default and run under =0; an answered gap "
        "refused under =1; three malformed lists refused; %s",
        n, mapped.c_str());
}

std::vector<unsigned char> ReadBin(const std::string& path, std::uint32_t size) {
    FILE* f = OpenUtf8(path);
    if (!f) Fatal("BOF3X_EXEIMAGE: cannot open %s", path.c_str());
    std::vector<unsigned char> out(size + 1);
    const std::size_t n = std::fread(out.data(), 1, out.size(), f);
    std::fclose(f);
    if (n != size) Fatal("BOF3X_EXEIMAGE: %s holds %u bytes, data.toml says %u", path.c_str(), (unsigned)n, (unsigned)size);
    out.resize(size);
    return out;
}

// --- The base's table files (docs/image-layer-round.md 3.1) ----------
//
// tools/exe_tables.py cuts every tables.toml table that can be a file out of
// data.bin: the range is class `file`, zero in data.bin, `from` the table's
// key, and the bytes are base\exe\tables\<key>.bin, one row each in
// tables\tables.toml: ["key", 0xPC, 0xEND, count, stride, "sha256", "what it was"].
// ComposeBase lays them back before anything else reads the image, so the
// bytes the engine maps are the ones the image held whole.
struct TableRow {
    std::string key, sha256;
    std::uint32_t lo, hi, count, stride;
};

bool ParseTableRows(const std::string& text, std::vector<TableRow>* out, std::string* why) {
    out->clear();
    std::size_t at = 0;
    while (at < text.size()) {
        std::size_t end = text.find('\n', at);
        if (end == std::string::npos) end = text.size();
        std::string line = text.substr(at, end - at);
        at = end + 1;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const char* p = line.c_str();
        while (*p == ' ') ++p;
        unsigned a = 0, b = 0, c = 0, s = 0;
        char key[128] = {}, h[128] = {};
        if (std::sscanf(p, "[\"%127[^\"]\", 0x%x, 0x%x, %u, %u, \"%127[^\"]\"", key, &a, &b, &c, &s, h) != 6) continue;
        if (b <= a || !c || !s || static_cast<std::uint64_t>(c) * s != b - a || std::strlen(h) != 64) {
            *why = std::string("the row of table ") + key + " is not one tools/exe_tables.py wrote";
            return false;
        }
        out->push_back({key, h, a, b, c, s});
    }
    if (out->empty()) {
        *why = "no table rows";
        return false;
    }
    return true;
}

// `body`, the file of `row`, laid at the image's `file` range `r` (`img` holds
// va's byte at img[0]). False with `why`, laying nothing, unless the range is
// inside the row's table, the file the row's, and data.bin zero there.
// DIV-0212 as amended 2026-11-11: a table with holes (tables.toml `holes`)
// has one `file` range for each run of it the file owns; the bytes between
// are another range's (a name the language layer writes) and are not laid,
// so only the range's own part of the file goes down.
bool LayBaseTable(const TableRow& row, const std::vector<unsigned char>& body, const Range& r, unsigned char* img,
                  std::uint32_t va, std::string* why) {
    char msg[256];
    if (r.lo < row.lo || r.hi > row.hi || r.hi <= r.lo) {
        std::snprintf(msg, sizeof msg, "tables.toml has %s at 0x%X..0x%X, data.toml's file range is 0x%X..0x%X",
                      row.key.c_str(), (unsigned)row.lo, (unsigned)row.hi, (unsigned)r.lo, (unsigned)r.hi);
        *why = msg;
        return false;
    }
    if (body.size() != row.hi - row.lo) {
        std::snprintf(msg, sizeof msg, "%s.bin holds %u bytes, the table %u", row.key.c_str(), (unsigned)body.size(),
                      (unsigned)(row.hi - row.lo));
        *why = msg;
        return false;
    }
    std::string hex;
    if (!bof3x::Sha256HexBytes(body.data(), body.size(), hex) || hex != row.sha256) {
        *why = row.key + ".bin is not the file tables.toml's sha256 names";
        return false;
    }
    for (std::uint32_t a = r.lo; a < r.hi; ++a)
        if (img[a - va]) {
            std::snprintf(msg, sizeof msg, "data.bin is not zero at 0x%X, inside the file range of %s", (unsigned)a,
                          row.key.c_str());
            *why = msg;
            return false;
        }
    std::memcpy(img + (r.lo - va), body.data() + (r.lo - row.lo), r.hi - r.lo);
    return true;
}

// The rows of the base ComposeBase last laid in ExeImage_Source: a layer's
// table must be the same table at the same place.
std::vector<TableRow> g_base_tables;

// Every `file` range of `im` filled in `bin` from exe_dir's tables\. An image
// written before the table files has no such range: nothing to do. Anything
// missing or wrong is a Fatal that names the file.
std::vector<TableRow> ComposeBase(const std::string& exe_dir, const Image& im, std::vector<unsigned char>& bin) {
    std::vector<TableRow> rows;
    std::uint32_t files = 0, bytes = 0;
    for (const Range& r : im.ranges) files += r.how == "file";
    if (!files) return rows;
    const std::string dir = exe_dir + "tables\\";
    std::string why;
    if (!ParseTableRows(ReadWhole(dir + "tables.toml"), &rows, &why))
        Fatal("BOF3X_EXEIMAGE: %stables.toml: %s. Rebuild the cache (tools/importer.py build).", dir.c_str(), why.c_str());
    for (const Range& r : im.ranges) {
        if (r.how != "file") continue;
        const TableRow* row = nullptr;
        for (const TableRow& t : rows)
            if (t.key == r.from) row = &t;
        if (!row || r.from.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_") != std::string::npos)
            Fatal("BOF3X_EXEIMAGE: %stables.toml has no row for the table \"%s\" that data.toml's range 0x%X..0x%X "
                  "names. Rebuild the cache (tools/importer.py build).",
                  dir.c_str(), r.from.c_str(), (unsigned)r.lo, (unsigned)r.hi);
        const std::string path = dir + r.from + ".bin";
        FILE* f = OpenUtf8(path);
        if (!f) Fatal("BOF3X_EXEIMAGE: cannot open %s, a table of this install's game data. Rebuild the cache "
                      "(tools/importer.py build).", path.c_str());
        std::vector<unsigned char> body(row->hi - row->lo + 1);   // the whole table, one byte more to see a long file
        body.resize(std::fread(body.data(), 1, body.size(), f));
        std::fclose(f);
        if (!LayBaseTable(*row, body, r, bin.data(), im.va, &why))
            Fatal("BOF3X_EXEIMAGE: %s: %s. Rebuild the cache (tools/importer.py build).", path.c_str(), why.c_str());
        bytes += r.hi - r.lo;
    }
    Log("BOF3X_EXEIMAGE: %u table files, %u bytes, laid into the image from %s (docs/image-layer-round.md)", (unsigned)files,
        (unsigned)bytes, dir.c_str());
    return rows;
}

// BOF3.exe's .data: (address, initialised size, virtual size) - its section
// table's values (exe_layout.h), where this read the running image's header
// before the exe host existed; the exe host checks them against the
// player's file before copying (exe_host.cpp).
void LiveData(std::uint32_t& va, std::uint32_t& raw, std::uint32_t& vsz) {
    va = exe_layout::kDataVa;
    raw = exe_layout::kDataBytes;
    vsz = exe_layout::kBssEnd - exe_layout::kDataVa;
}

}  // namespace

void ExeImage_Check() {
    const std::string dir = ImageDir();
    if (dir.empty()) return;
    const std::string exe_dir = dir + "\\base\\exe\\";
    const Image im = ReadToml(exe_dir + "data.toml");
    std::uint32_t va = 0, raw = 0, vsz = 0;
    LiveData(va, raw, vsz);
    if (im.va != va || im.size != raw || im.bss_end != va + vsz)
        Fatal("BOF3X_EXEIMAGE: data.toml's image is 0x%X + 0x%X (zero to 0x%X), the running .data 0x%X + 0x%X (to 0x%X)",
              (unsigned)im.va, (unsigned)im.size, (unsigned)im.bss_end, (unsigned)va, (unsigned)raw, (unsigned)(va + vsz));
    std::vector<unsigned char> bin = ReadBin(exe_dir + im.file, im.size);
    ComposeBase(exe_dir, im, bin);   // the table files, laid where data.bin leaves them out
    auto* live = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(Img(va)));

    // .bss: zero-initialised by the loader, as state 3's engine must do itself.
    for (std::uint32_t a = va + raw; a < im.bss_end; ++a)
        if (live[a - va] != 0) Fatal("BOF3X_EXEIMAGE: .bss is not zero at 0x%X before the game starts", (unsigned)a);

    // Every byte, counted by the range that produced it.
    std::uint32_t equal = 0, first_diff = 0;
    struct Count { const char* how; std::uint32_t equal, total; };
    Count by[] = {{"exe", 0, 0},     {"map", 0, 0},    {"place", 0, 0},    {"table", 0, 0},  {"widen", 0, 0},
                  {"rebuilt", 0, 0}, {"layout", 0, 0}, {"own", 0, 0},      {"keys", 0, 0},   {"none", 0, 0},
                  {"language", 0, 0}, {"unread", 0, 0}, {"gap", 0, 0},     {"pointer", 0, 0}, {"file", 0, 0}};
    std::uint32_t covered = va;
    for (const Range& r : im.ranges) {
        if (r.lo != covered || r.hi <= r.lo || r.hi > va + raw)
            Fatal("BOF3X_EXEIMAGE: data.toml's ranges do not cover the image in order at 0x%X", (unsigned)r.lo);
        covered = r.hi;
        Count* c = nullptr;
        for (Count& k : by)
            if (r.how == k.how) c = &k;
        if (!c) Fatal("BOF3X_EXEIMAGE: data.toml range 0x%X has an unknown how \"%s\"", (unsigned)r.lo, r.how.c_str());
        for (std::uint32_t a = r.lo; a < r.hi; ++a) {
            const bool same = bin[a - va] == live[a - va];
            c->total += 1;
            c->equal += same;
            equal += same;
            if (!same && !first_diff) first_diff = a;
        }
    }
    if (covered != va + raw) Fatal("BOF3X_EXEIMAGE: data.toml's ranges end at 0x%X, not 0x%X", (unsigned)covered, (unsigned)(va + raw));
    for (const Count& c : by)
        if (c.total) Log("BOF3X_EXEIMAGE  %-8s %7u of %7u bytes equal to the running .data", c.how, (unsigned)c.equal, (unsigned)c.total);

    if (im.build == "pc-zh") {
        if (equal != im.size)
            Fatal("BOF3X_EXEIMAGE: base/exe/data.bin is built from BOF3.exe but %u of its %u bytes differ from the "
                  "running .data, the first at 0x%X", (unsigned)(im.size - equal), (unsigned)im.size, (unsigned)first_diff);
        std::memcpy(live, bin.data(), im.size);   // the same bytes: the map step, changing nothing
        Log("BOF3X_EXEIMAGE: base/exe/data.bin (pc-zh) laid over .data at 0x%X: %u bytes, byte-identical; "
            ".bss zero to 0x%X", (unsigned)va, (unsigned)im.size, (unsigned)im.bss_end);
    } else {
        Log("BOF3X_EXEIMAGE: base/exe/data.bin (%s) compared, not mapped: %u of %u bytes equal to the running .data "
            "(the hosted build lays only a pc-zh image over Capcom's; bof3x.exe maps a disc-built one)",
            im.build.c_str(), (unsigned)equal, (unsigned)im.size);
    }
}

bool ExeImage_Source(void* to, std::uint32_t va, std::uint32_t size, std::uint32_t bss_end) {
    const bool selftest = bof3::os::GetEnv("BOF3X_SELFTEST_ONLY", nullptr, 0) > 0;
    const Strict mode = StrictMode();   // read on every install, so that a misspelt value stops a PC install too
    const std::string dir = ImageDir();
    if (dir.empty()) {
        if (selftest) StrictSelfTest(std::string());
        return false;
    }
    const std::string exe_dir = dir + "\\base\\exe\\";
    const std::string toml_text = ReadWhole(exe_dir + "data.toml");
    const Image im = ParseToml(toml_text, exe_dir + "data.toml");
    if (im.va != va || im.size != size || im.bss_end != bss_end)
        Fatal("BOF3X_EXEIMAGE: data.toml's image is 0x%X + 0x%X (zero to 0x%X), BOF3.exe's .data 0x%X + 0x%X (to 0x%X)",
              (unsigned)im.va, (unsigned)im.size, (unsigned)im.bss_end, (unsigned)va, (unsigned)size, (unsigned)bss_end);
    std::uint32_t covered = va;
    for (const Range& r : im.ranges) {
        if (r.lo != covered || r.hi <= r.lo || r.hi > va + size)
            Fatal("BOF3X_EXEIMAGE: data.toml's ranges do not cover the image in order at 0x%X", (unsigned)r.lo);
        covered = r.hi;
        ClassOf(r);
    }
    if (covered != va + size)
        Fatal("BOF3X_EXEIMAGE: data.toml's ranges end at 0x%X, not 0x%X", (unsigned)covered, (unsigned)(va + size));
    std::vector<unsigned char> bin = ReadBin(exe_dir + im.file, im.size);
    g_base_tables = ComposeBase(exe_dir, im, bin);   // the table files, laid where data.bin leaves them out

    if (im.build == "pc-zh") {
        // DIV-0212: a PC install reads this image in place of the file's
        // .data. With the file there, the two must be the same bytes: an image
        // that is not this BOF3.exe's (a damaged cache, a table file changed
        // by hand) is refused, never run. A change belongs in a layer.
        if (ExeFile_Present()) {
            std::vector<unsigned char> pc(im.size);
            ExeFile_Read(im.va, pc.data(), im.size);
            for (std::uint32_t i = 0; i < im.size; ++i)
                if (pc[i] != bin[i]) {
                    const char* in = "data.bin";
                    std::string table;
                    for (const Range& r : im.ranges)
                        if (r.how == "file" && r.lo <= im.va + i && im.va + i < r.hi) {
                            table = "tables\\" + r.from + ".bin";
                            in = table.c_str();
                        }
                    Fatal("This install's game data is not the data of this BOF3.exe. Rebuild the install: in the "
                          "launcher's Sources page add the game again, or run tools/importer.py build. To change a "
                          "table, use a mod; the files of base\\exe are checked against the game.\n\n"
                          "BOF3X_EXEIMAGE: %s%s differs from BOF3.exe's .data at 0x%06X (the image 0x%02X, the "
                          "file 0x%02X); BOF3X_EXEFILE=1 reads the file",
                          exe_dir.c_str(), in, (unsigned)(im.va + i), (unsigned)bin[i], (unsigned)pc[i]);
                }
            Log("BOF3X_EXEIMAGE: base/exe (pc-zh) is BOF3.exe's .data, byte for byte: %u bytes compared (DIV-0212)",
                (unsigned)im.size);
        }
        std::memcpy(to, bin.data(), im.size);
        // The bytes no disc carries that are the engine's own (exe_own.h):
        // checked against the image before they are written, so that a pc-zh
        // image proves them; over a pc-zh image the fill changes nothing.
        ExeOwn_Verify(bin.data(), "base/exe/data.bin (pc-zh)");
        ExeOwn_Fill(static_cast<std::uint8_t*>(to));
        Log("BOF3X_EXEIMAGE: .data at 0x%X from base/exe/data.bin (pc-zh), %u bytes: the source in the exe host",
            (unsigned)va, (unsigned)im.size);
        if (selftest) StrictSelfTest(std::string());
        return true;
    }

    // A disc-built image (docs/exe-import-engine.md section 4.4): every range
    // a class the engine accounts for. An image written before
    // recipes/exe-gaps.tsv named what no disc carries has `none` ranges that
    // hide the PC's values: refused, as the no-stub rule wants.
    if (im.gaps.empty())
        Fatal("BOF3X_EXEIMAGE: %s (%s) was built before recipes/exe-gaps.tsv classed the bytes no disc carries: its "
              "`none` ranges may stand where the PC has values. Rebuild the cache (tools/importer.py build).",
              exe_dir.c_str(), im.build.c_str());
    for (const Range& r : im.ranges)
        if (r.how == "pointer")
            Fatal("BOF3X_EXEIMAGE: %s (%s) leaves the pointer words 0x%X..0x%X unfilled; every pointer word of a disc "
                  "build is rebuilt or the layout's (recipes/exe-layout.tsv). Rebuild the cache.",
                  exe_dir.c_str(), im.build.c_str(), (unsigned)r.lo, (unsigned)r.hi);
    for (const Since& s : FilledSince())
        for (const Range& r : im.ranges)
            if (r.how == "gap" && r.lo < s.hi && s.lo < r.hi)
                Fatal("This install's game data was built by an older release and is missing %s, which this "
                      "release needs (%s). Rebuild the install: in the "
                      "launcher's Sources page add the disc again, or run tools/importer.py build.\n\n"
                      "BOF3X_EXEIMAGE: %s (%s) leaves 0x%06X..0x%06X a gap (%s)",
                      s.what, s.effect, exe_dir.c_str(), im.build.c_str(), (unsigned)r.lo, (unsigned)r.hi,
                      r.from.c_str());
    if (const std::string why = AcceptedProblem(im); !why.empty())
        Fatal("BOF3X_EXEIMAGE: %sdata.toml (%s): %s - not a list tools/exe_tables.py wrote. Rebuild the cache "
              "(tools/importer.py build).",
              exe_dir.c_str(), im.build.c_str(), why.c_str());
    if (selftest) StrictSelfTest(toml_text);   // before the refusal below, so that the control runs on any image
    if (const std::string why = GapRefusal(im, mode, exe_dir); !why.empty()) Fatal("%s", why.c_str());

    std::memcpy(to, bin.data(), im.size);
    ExeOwn_Fill(static_cast<std::uint8_t*>(to));   // the engine's own regions, over the image's zeros

    struct Count {
        const char* how;
        std::uint32_t ranges, bytes;
    } by[sizeof kClasses / sizeof kClasses[0]] = {};
    for (std::size_t i = 0; i < sizeof kClasses / sizeof kClasses[0]; ++i) by[i].how = kClasses[i].how;
    std::uint32_t answered = 0, unanswered = 0;
    for (const Range& r : im.ranges) {
        const std::size_t i = static_cast<std::size_t>(ClassOf(r) - kClasses);
        by[i].ranges += 1;
        by[i].bytes += r.hi - r.lo;
        if (r.how != "gap") continue;
        const char* answer = AnswerOf(im, r);   // null here only under BOF3X_EXEIMAGE_STRICT=0 (GapRefusal)
        (answer ? answered : unanswered) += 1;
        Log("BOF3X_EXEIMAGE  gap 0x%06X..0x%06X %4u bytes  %s: zero (the PC has a value; no disc carries it), %s%s",
            (unsigned)r.lo, (unsigned)r.hi, (unsigned)(r.hi - r.lo), r.from.c_str(),
            answer ? "accepted: " : "NOT accepted - runs under BOF3X_EXEIMAGE_STRICT=0", answer ? answer : "");
    }
    for (const Count& c : by)
        if (c.ranges) Log("BOF3X_EXEIMAGE  %-8s %5u ranges %7u bytes", c.how, (unsigned)c.ranges, (unsigned)c.bytes);
    Log("BOF3X_EXEIMAGE: .data at 0x%X from base/exe/data.bin (%s, disc-built), %u bytes; the engine's own regions "
        "filled; the key table next (Shell_MapDefaultKeys); every gap zero, as logged: %u accepted, %u not%s "
        "(BOF3X_EXEIMAGE_STRICT%s)",
        (unsigned)va, im.build.c_str(), (unsigned)im.size, (unsigned)answered, (unsigned)unanswered,
        im.has_accepted ? "" : ", the image written before the accepted list", StrictName(mode));
    g_mapped = im;
    g_disc_mapped = true;
    return true;
}

bool ExeImage_DiscMapped() { return g_disc_mapped; }

const char* ExeImage_MappedBuild() { return g_disc_mapped ? g_mapped.build.c_str() : nullptr; }

int ExeImage_MappedRanges() { return g_disc_mapped ? static_cast<int>(g_mapped.ranges.size()) : 0; }

ExeImageRange ExeImage_MappedRange(int i) {
    if (i < 0 || i >= ExeImage_MappedRanges())
        Fatal("ExeImage_MappedRange(%d): the mapped image has %d ranges", i, ExeImage_MappedRanges());
    const Range& r = g_mapped.ranges[static_cast<std::size_t>(i)];
    return {r.lo, r.hi, r.how.c_str(), r.from.c_str()};
}

// --- DIV-0111: a layer's part over .data --------------------------------
//
// A layer's exe\ directory holds two things, either or both
// (docs/image-layer-round.md 3.2):
//   tables.toml + <key>.bin   one whole file a tables.toml table the layer
//                             changes, and `cells`, the byte runs of each that
//                             are the layer's own; only those are laid
//   place.toml + place.bin    byte ranges of .data outside every table, by
//                             address, `kind = "raw"`. A place.toml without
//                             that line is the form before the table files
//                             (psp-tables as tools/psp_layers.py wrote it until
//                             2026-11-11): refused, with the rebuild sentence.
// Layers are laid in BOF3X_OPT's order; where two write one byte with
// different values the later wins, and the exe host logs it (the owner,
// 2026-11-11).
namespace {

struct OptRange {
    std::uint32_t lo, hi;
    std::string what;
};
struct OptTables {
    std::string build, file, sha256, kind;
    std::uint32_t bytes = 0;
    std::vector<OptRange> ranges;
};

// place.toml: `key = value` lines, then one `[0x.., 0x.., "what"],` line a
// range. Only that shape; false with `why` otherwise. `old_form` is set when
// the file is whole but for the `kind = "raw"` line.
bool ParseOptTables(const std::string& text, OptTables* out, std::string* why, bool* old_form = nullptr) {
    *out = OptTables{};
    if (old_form) *old_form = false;
    std::size_t at = 0;
    while (at < text.size()) {
        std::size_t end = text.find('\n', at);
        if (end == std::string::npos) end = text.size();
        std::string line = text.substr(at, end - at);
        at = end + 1;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const char* p = line.c_str();
        while (*p == ' ') ++p;
        unsigned a = 0, b = 0;
        char s[128] = {};
        if (std::sscanf(p, "[0x%x, 0x%x, \"%127[^\"]\"]", &a, &b, s) == 3) out->ranges.push_back({a, b, s});
        else if (std::sscanf(p, "build = \"%127[^\"]\"", s) == 1) out->build = s;
        else if (std::sscanf(p, "file = \"%127[^\"]\"", s) == 1) out->file = s;
        else if (std::sscanf(p, "sha256 = \"%127[^\"]\"", s) == 1) out->sha256 = s;
        else if (std::sscanf(p, "kind = \"%127[^\"]\"", s) == 1) out->kind = s;
        else if (std::sscanf(p, "bytes = %u", &a) == 1) out->bytes = a;
    }
    if (out->build.empty() || out->file != "place.bin" || out->sha256.size() != 64 || out->ranges.empty()) {
        *why = "not a place.toml of byte ranges (build, kind = \"raw\", file = \"place.bin\", sha256, ranges)";
        return false;
    }
    if (out->kind != "raw") {
        if (old_form) *old_form = true;
        *why = "a place.toml written before the table files (no kind = \"raw\"): its tables are now one file each";
        return false;
    }
    return true;
}

// Who wrote each byte of .data among the layers so far (1-based, 0 nobody),
// and what two layers disagreed on.
struct Conflict {
    std::string what;        // a table's key, or "ranges"
    unsigned earlier, bytes; // the layer that lost (1-based), the bytes it lost
};
struct Laid {
    std::vector<std::uint8_t> owner;
    std::vector<Conflict> conflicts;
    void Note(const std::string& what, unsigned earlier) {
        for (Conflict& c : conflicts)
            if (c.what == what && c.earlier == earlier) {
                ++c.bytes;
                return;
            }
        conflicts.push_back({what, earlier, 1});
    }
};

// One byte of layer `index` (1-based) over .data; counted when it changes it.
void Put(unsigned char* data, std::uint32_t off, unsigned char v, unsigned index, const std::string& what, Laid* laid,
         int* changed) {
    if (laid && off < laid->owner.size()) {
        const unsigned was = laid->owner[off];
        if (was && was != index && data[off] != v) laid->Note(what, was);
        laid->owner[off] = static_cast<std::uint8_t>(index);
    }
    *changed += data[off] != v;
    data[off] = v;
}

// Whether [lo, hi) touches a range of the mapped image that must equal the
// PC's; `why` names it.
bool OnMustEqual(const Image* mapped, std::uint32_t lo, std::uint32_t hi, const char* what, std::string* why) {
    if (!mapped) return false;
    for (const Range& m : mapped->ranges) {
        if (m.hi <= lo || m.lo >= hi) continue;
        if (!ClassOf(m)->must_equal) continue;
        char msg[320];
        std::snprintf(msg, sizeof msg, "0x%X..0x%X (%s) lies on 0x%X..0x%X, class %s of the mapped %s image, which "
                      "must equal the PC's", (unsigned)lo, (unsigned)hi, what, (unsigned)m.lo, (unsigned)m.hi,
                      m.how.c_str(), mapped->build.c_str());
        *why = msg;
        return true;
    }
    return false;
}

// The raw ranges and bytes against .data [va, va + size), the mapped image
// (when one is: each range's bytes in a class that need not equal the PC's)
// and laid over `data` (va's byte at data[0]). Returns the bytes that changed,
// or -1 with `why`, laying nothing.
int LayOptTables(const OptTables& t, const std::vector<unsigned char>& bin, unsigned char* data, std::uint32_t va,
                  std::uint32_t size, const Image* mapped, std::string* why, unsigned index = 1, Laid* laid = nullptr) {
    char msg[256];
    std::uint32_t total = 0, prev = va;
    for (const OptRange& r : t.ranges) {
        if (r.lo < prev || r.hi <= r.lo || r.hi > va + size) {
            std::snprintf(msg, sizeof msg, "range 0x%X..0x%X (%s) out of order or outside .data 0x%X..0x%X",
                          (unsigned)r.lo, (unsigned)r.hi, r.what.c_str(), (unsigned)va, (unsigned)(va + size));
            *why = msg;
            return -1;
        }
        prev = r.hi;
        total += r.hi - r.lo;
        if (OnMustEqual(mapped, r.lo, r.hi, r.what.c_str(), why)) return -1;
    }
    if (total != bin.size() || total != t.bytes) {
        std::snprintf(msg, sizeof msg, "the ranges hold %u bytes, place.bin %u, place.toml says %u", (unsigned)total,
                      (unsigned)bin.size(), (unsigned)t.bytes);
        *why = msg;
        return -1;
    }
    std::string hex;
    if (!bof3x::Sha256HexBytes(bin.data(), bin.size(), hex) || hex != t.sha256) {
        *why = "place.bin is not the file place.toml's sha256 names";
        return -1;
    }
    int changed = 0;
    std::size_t off = 0;
    for (const OptRange& r : t.ranges)
        for (std::uint32_t a = r.lo; a < r.hi; ++a, ++off) Put(data, a - va, bin[off], index, "ranges", laid, &changed);
    return changed;
}

// A layer's tables.toml (tools/psp_layers.py build_exe, and a mod's builder):
//   tables = [ ["key", 0xPC, count, stride, "sha256 of <key>.bin"], ]
//   cells  = [ ["key", record, at, length], ]   - `length` bytes from byte
//                                                 `at` of `record`, the layer's own
struct LayerTable {
    std::string key, sha256;
    std::uint32_t lo, count, stride;
};
struct LayerCell {
    std::string key;
    std::uint32_t record, at, length;
};
struct LayerTables {
    std::string build;
    std::vector<LayerTable> tables;
    std::vector<LayerCell> cells;
};

bool ParseLayerTables(const std::string& text, LayerTables* out, std::string* why) {
    *out = LayerTables{};
    std::size_t at = 0;
    while (at < text.size()) {
        std::size_t end = text.find('\n', at);
        if (end == std::string::npos) end = text.size();
        std::string line = text.substr(at, end - at);
        at = end + 1;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const char* p = line.c_str();
        while (*p == ' ') ++p;
        unsigned a = 0, b = 0, c = 0;
        char s[128] = {}, h[128] = {};
        if (std::sscanf(p, "[\"%127[^\"]\", 0x%x, %u, %u, \"%127[^\"]\"]", s, &a, &b, &c, h) == 5)
            out->tables.push_back({s, h, a, b, c});
        else if (std::sscanf(p, "[\"%127[^\"]\", %u, %u, %u]", s, &a, &b, &c) == 4) out->cells.push_back({s, a, b, c});
        else if (std::sscanf(p, "build = \"%127[^\"]\"", s) == 1) out->build = s;
    }
    if (out->build.empty() || out->tables.empty() || out->cells.empty()) {
        *why = "not a layer's tables.toml (build, tables, cells)";
        return false;
    }
    return true;
}

// The layer's tables against .data, the base's table files (when the mapped
// base has them: the same table at the same place) and the mapped image, then
// its cells laid over `data`. `files[i]` is tables[i]'s <key>.bin. Returns the
// bytes that changed, or -1 with `why`, laying nothing.
int LayLayerTables(const LayerTables& t, const std::vector<std::vector<unsigned char>>& files, unsigned char* data,
                    std::uint32_t va, std::uint32_t size, const Image* mapped, const std::vector<TableRow>* base,
                    std::string* why, unsigned index = 1, Laid* laid = nullptr) {
    char msg[320];
    const auto fail = [&]() {
        *why = msg;
        return -1;
    };
    for (std::size_t i = 0; i < t.tables.size(); ++i) {
        const LayerTable& lt = t.tables[i];
        const std::uint64_t bytes = static_cast<std::uint64_t>(lt.count) * lt.stride;
        if (!lt.count || !lt.stride || lt.lo < va || bytes > size || lt.lo - va > size - bytes) {
            std::snprintf(msg, sizeof msg, "table %s (0x%X, %u records of %u bytes) is not inside .data 0x%X..0x%X",
                          lt.key.c_str(), (unsigned)lt.lo, (unsigned)lt.count, (unsigned)lt.stride, (unsigned)va,
                          (unsigned)(va + size));
            return fail();
        }
        for (std::size_t j = 0; j < i; ++j)
            if (t.tables[j].key == lt.key) {
                std::snprintf(msg, sizeof msg, "table %s is listed twice", lt.key.c_str());
                return fail();
            }
        if (base)
            for (const TableRow& b : *base)
                if (b.key == lt.key && (b.lo != lt.lo || b.count != lt.count || b.stride != lt.stride)) {
                    std::snprintf(msg, sizeof msg, "table %s is 0x%X, %u x %u in the layer and 0x%X, %u x %u in the "
                                  "base: the layer was built for another release", lt.key.c_str(), (unsigned)lt.lo,
                                  (unsigned)lt.count, (unsigned)lt.stride, (unsigned)b.lo, (unsigned)b.count,
                                  (unsigned)b.stride);
                    return fail();
                }
        if (i >= files.size() || files[i].size() != bytes) {
            std::snprintf(msg, sizeof msg, "%s.bin holds %u bytes, the table %u", lt.key.c_str(),
                          (unsigned)(i < files.size() ? files[i].size() : 0), (unsigned)bytes);
            return fail();
        }
        std::string hex;
        if (!bof3x::Sha256HexBytes(files[i].data(), files[i].size(), hex) || hex != lt.sha256) {
            std::snprintf(msg, sizeof msg, "%s.bin is not the file tables.toml's sha256 names", lt.key.c_str());
            return fail();
        }
    }
    struct Run {
        std::size_t table;
        std::uint32_t off, length;
    };
    std::vector<Run> runs;
    for (const LayerCell& c : t.cells) {
        std::size_t i = 0;
        while (i < t.tables.size() && t.tables[i].key != c.key) ++i;
        if (i == t.tables.size()) {
            std::snprintf(msg, sizeof msg, "a cell names table %s, which the layer does not carry", c.key.c_str());
            return fail();
        }
        const LayerTable& lt = t.tables[i];
        const std::uint64_t bytes = static_cast<std::uint64_t>(lt.count) * lt.stride;
        const std::uint64_t off = static_cast<std::uint64_t>(c.record) * lt.stride + c.at;
        if (c.record >= lt.count || c.at >= lt.stride || !c.length || off + c.length > bytes) {
            std::snprintf(msg, sizeof msg, "cell %s record %u +%u, %u bytes, is past the table (%u records of %u bytes)",
                          c.key.c_str(), (unsigned)c.record, (unsigned)c.at, (unsigned)c.length, (unsigned)lt.count,
                          (unsigned)lt.stride);
            return fail();
        }
        const std::uint32_t lo = lt.lo + static_cast<std::uint32_t>(off);
        if (OnMustEqual(mapped, lo, lo + c.length, c.key.c_str(), why)) return -1;
        runs.push_back({i, static_cast<std::uint32_t>(off), c.length});
    }
    int changed = 0;
    for (const Run& r : runs) {
        const LayerTable& lt = t.tables[r.table];
        for (std::uint32_t k = 0; k < r.length; ++k)
            Put(data, lt.lo - va + r.off + k, files[r.table][r.off + k], index, lt.key, laid, &changed);
    }
    return changed;
}

bool ReadText(const std::string& path, std::string* out) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    out->clear();
    char buf[4096];
    std::size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) out->append(buf, n);
    std::fclose(f);
    return true;
}

bool ReadBytes(const std::string& path, std::vector<unsigned char>* out) {
    std::string s;
    if (!ReadText(path, &s)) return false;
    out->assign(s.begin(), s.end());
    return true;
}

}  // namespace

void ExeImage_LayOptTables(void* to, std::uint32_t va, std::uint32_t size) {
    char opt[16 * 24];
    const DWORD n = bof3::os::GetEnv("BOF3X_OPT", opt, sizeof opt);
    if (n == 0 || n >= sizeof opt) return;   // none, or too long: ReadOptLayers refuses it at injection
    char data_mode[4];
    const DWORD m = bof3::os::GetEnv("BOF3X_CACHE_DATA", data_mode, sizeof data_mode);
    if (m == 1 && data_mode[0] == '0') return;   // the cache for the music only (DIV-0089)
    char root[MAX_PATH];
    const DWORD r = bof3::os::GetEnv("BOF3X_CACHE", root, sizeof root);
    if (r == 0 || r >= sizeof root) return;
    std::string base(root, r);
    while (base.size() > 1 && (base.back() == '\\' || base.back() == '/')) base.pop_back();
    std::string list(opt, n);
    std::vector<std::string> names;   // the layers with an exe part, in order: a conflict's line names them
    Laid laid;
    auto* data = static_cast<unsigned char*>(to);
    const Image* mapped = g_disc_mapped ? &g_mapped : nullptr;
    const char* over = g_disc_mapped ? g_mapped.build.c_str() : "the file's or a pc-zh image";
    std::size_t at = 0;
    while (at <= list.size()) {
        std::size_t end = list.find(',', at);
        if (end == std::string::npos) end = list.size();
        const std::string layer = list.substr(at, end - at);
        at = end + 1;
        // a malformed name is ReadOptLayers' to refuse; only a plain one is a directory here
        if (layer.empty() || layer.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-") !=
                                 std::string::npos)
            continue;
        const std::string dir = base + "\\opt\\" + layer + "\\exe\\";
        std::string tables_text, place_text;
        const bool has_tables = ReadText(dir + "tables.toml", &tables_text);
        const bool has_place = ReadText(dir + "place.toml", &place_text);
        if (!has_tables && !has_place) continue;   // no exe part
        if (names.size() >= 255) Fatal("DIV-0111: more than 255 layers with an exe part");
        names.push_back(layer);
        const unsigned index = static_cast<unsigned>(names.size());
        if (laid.owner.empty()) laid.owner.assign(size, 0);
        const std::size_t before = laid.conflicts.size();
        std::string why;
        if (has_tables) {
            LayerTables t;
            if (!ParseLayerTables(tables_text, &t, &why)) Fatal("DIV-0111: %stables.toml: %s", dir.c_str(), why.c_str());
            std::vector<std::vector<unsigned char>> files(t.tables.size());
            for (std::size_t i = 0; i < t.tables.size(); ++i) {
                const std::string& key = t.tables[i].key;
                if (key.empty() || key.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_") != std::string::npos)
                    Fatal("DIV-0111: %stables.toml names a table \"%s\": not a tables.toml key", dir.c_str(), key.c_str());
                if (!ReadBytes(dir + key + ".bin", &files[i])) Fatal("DIV-0111: %s%s.bin does not open", dir.c_str(), key.c_str());
            }
            const int changed = LayLayerTables(t, files, data, va, size, mapped, g_base_tables.empty() ? nullptr : &g_base_tables,
                                                &why, index, &laid);
            if (changed < 0) Fatal("DIV-0111: layer %s (%s): %s", layer.c_str(), dir.c_str(), why.c_str());
            std::uint32_t bytes = 0;
            for (const LayerCell& c : t.cells) bytes += c.length;
            Log("DIV-0111    layer %s: %u tables, %u cells, %u bytes of the %s image's tables laid over .data (%s); %d "
                "of them differ from what was there",
                layer.c_str(), (unsigned)t.tables.size(), (unsigned)t.cells.size(), (unsigned)bytes, t.build.c_str(),
                over, changed);
        }
        if (has_place) {
            OptTables t;
            bool old_form = false;
            if (!ParseOptTables(place_text, &t, &why, &old_form)) {
                if (old_form)
                    Fatal("This install's optional layer \"%s\" was built by an older release. This release reads a "
                          "layer's tables as one file each. Rebuild the install: in the launcher's Sources page add "
                          "the disc again, or run tools/importer.py build.\n\nDIV-0111: %splace.toml: %s",
                          layer.c_str(), dir.c_str(), why.c_str());
                Fatal("DIV-0111: %splace.toml: %s", dir.c_str(), why.c_str());
            }
            std::vector<unsigned char> bin;
            if (!ReadBytes(dir + t.file, &bin)) Fatal("DIV-0111: %s%s does not open", dir.c_str(), t.file.c_str());
            const int changed = LayOptTables(t, bin, data, va, size, mapped, &why, index, &laid);
            if (changed < 0) Fatal("DIV-0111: layer %s (%s): %s", layer.c_str(), dir.c_str(), why.c_str());
            Log("DIV-0111    layer %s: %u ranges, %u bytes of the %s image's .data laid over .data (%s); %d of them "
                "differ from what was there",
                layer.c_str(), (unsigned)t.ranges.size(), (unsigned)t.bytes, t.build.c_str(), over, changed);
        }
        for (std::size_t i = before; i < laid.conflicts.size(); ++i) {
            const Conflict& c = laid.conflicts[i];
            Log("DIV-0111    layers %s and %s both write %s: %u bytes with different values; %s is later in the order "
                "and wins",
                names[c.earlier - 1].c_str(), layer.c_str(), c.what.c_str(), c.bytes, layer.c_str());
        }
    }
}

void ExeImage_OptSelfTest() {
    // A synthetic .data of 64 bytes at 0x1000 and a layer of two ranges.
    constexpr std::uint32_t kVa = 0x1000, kSize = 64;
    std::vector<unsigned char> bin = {1, 2, 3, 4, 5, 6};
    std::string hex;
    if (!bof3x::Sha256HexBytes(bin.data(), bin.size(), hex)) Fatal("DIV-0111 self-test: no SHA-256");
    const std::string good = "build = \"psp-xx\"\nkind = \"raw\"\nimage_sha256 = \"x\"\nfile = \"place.bin\"\nsha256 = \"" +
                             hex + "\"\nbytes = 6\n\nranges = [\n  [0x001004, 0x001008, \"a\"],\n  [0x001010, 0x001012, \"b\"],\n]\n";
    OptTables t;
    std::string why;
    std::vector<unsigned char> data(kSize, 0xEE);
    data[0x10] = 5;   // one byte already the layer's
    const bool parsed = ParseOptTables(good, &t, &why) && t.ranges.size() == 2 && t.bytes == 6;
    const int changed = parsed ? LayOptTables(t, bin, data.data(), kVa, kSize, nullptr, &why) : -1;
    bool laid = changed == 5;
    for (std::uint32_t i = 0; i < kSize; ++i) {
        const int want = i >= 4 && i < 8 ? int(i - 3) : i == 0x10 ? 5 : i == 0x11 ? 6 : 0xEE;
        laid = laid && data[i] == want;
    }
    // The controls, each refused with nothing laid.
    unsigned refused = 0;
    const auto refuse = [&](std::string text, const std::vector<unsigned char>& b, const Image* mapped) {
        OptTables u;
        std::string w;
        std::vector<unsigned char> d(kSize, 0xEE);
        const bool no = !ParseOptTables(text, &u, &w) || LayOptTables(u, b, d.data(), kVa, kSize, mapped, &w) < 0;
        refused += no && d == std::vector<unsigned char>(kSize, 0xEE);
    };
    std::vector<unsigned char> other = bin;
    other[0] ^= 1;
    refuse(good, other, nullptr);                                                   // a wrong hash
    std::string outside = good;
    outside.replace(outside.find("0x001010, 0x001012"), 18, "0x001040, 0x001042");
    refuse(outside, bin, nullptr);                                                  // outside .data
    std::string order = good;
    order.replace(order.find("0x001010, 0x001012"), 18, "0x001002, 0x001004");
    refuse(order, bin, nullptr);                                                    // out of order
    std::string count = good;
    count.replace(count.find("bytes = 6"), 9, "bytes = 7");
    refuse(count, bin, nullptr);                                                    // a byte count that differs
    Image im;
    im.build = "psx-xx";
    im.ranges = {{0x1000, 0x1010, "map", ""}, {0x1010, 0x1040, "layout", ""}};
    refuse(good, bin, &im);                                                         // on a must-equal class
    std::string old = good;
    old.erase(old.find("kind = \"raw\"\n"), 13);
    bool old_form = false;
    OptTables o;
    const bool old_refused = !ParseOptTables(old, &o, &why, &old_form) && old_form;  // the form before the table files
    im.ranges[1].how = "table";
    std::vector<unsigned char> d2(kSize, 0xEE);
    OptTables v;
    const bool over_disc = ParseOptTables(good, &v, &why) && LayOptTables(v, bin, d2.data(), kVa, kSize, &im, &why) == 6;
    if (!parsed || !laid || refused != 5 || !over_disc || !old_refused)
        Fatal("DIV-0111 self-test: parse %s, laid %s (%d changed), over a disc image's table class %s, %u of 5 controls "
              "refused, the old form %s", parsed ? "ok" : "WRONG", laid ? "ok" : "WRONG", changed,
              over_disc ? "ok" : "WRONG", refused, old_refused ? "refused" : "NOT refused");
    Log("shadow      DIV-0111 self-test: a place.toml parsed, its two ranges laid (5 bytes changed, one already the "
        "layer's), and over a disc image's table class; 5 controls refused with nothing laid (a wrong hash, a range "
        "outside .data, ranges out of order, a byte count that differs, a range on a must-equal class); a place.toml "
        "without kind = \"raw\" refused as the old form");

    // --- The table files (the amendment of 2026-11-11). A table `tab` of 4 records of 4 bytes at
    // 0x1010, the base's file laid whole, then two layers on it.
    unsigned ok = 0, checks = 0;
    const auto check = [&](bool right, const char* what) {
        ++checks;
        if (right) ++ok;
        else Log("shadow      DIV-0111 table-files self-test: WRONG - %s", what);
    };
    const auto sha = [](const std::vector<unsigned char>& b) {
        std::string h;
        if (!bof3x::Sha256HexBytes(b.data(), b.size(), h)) Fatal("DIV-0111 table-files self-test: no SHA-256");
        return h;
    };
    std::vector<unsigned char> body(16);
    for (std::size_t i = 0; i < body.size(); ++i) body[i] = static_cast<unsigned char>(0x40 + i);
    const std::string rows_text = "build = \"psx-xx\"\n\ntables = [\n  [\"tab\", 0x001010, 0x001020, 4, 4, \"" + sha(body) +
                                  "\", \"table A@0x80000000\"],\n]\n";
    std::vector<TableRow> rows;
    check(ParseTableRows(rows_text, &rows, &why) && rows.size() == 1 && rows[0].key == "tab" && rows[0].lo == 0x1010 &&
              rows[0].hi == 0x1020 && rows[0].count == 4 && rows[0].stride == 4,
          "the base's tables.toml parsed: one table");
    const Range file_range{0x1010, 0x1020, "file", "tab"};
    std::vector<unsigned char> img(kSize, 0);
    img[0] = 0x11;
    const bool base_laid = !rows.empty() && LayBaseTable(rows[0], body, file_range, img.data(), kVa, &why);
    bool base_right = base_laid && img[0] == 0x11;
    for (std::uint32_t i = 0; i < 16 && base_right; ++i) base_right = img[0x10 + i] == body[i];
    check(base_right, "the base's table file laid at its range, nothing else touched");
    const auto base_refused = [&](const TableRow& row, const std::vector<unsigned char>& b, const Range& r, unsigned char at0x10) {
        std::vector<unsigned char> d(kSize, 0);
        d[0x10] = at0x10;
        const std::vector<unsigned char> was = d;
        std::string w;
        return !LayBaseTable(row, b, r, d.data(), kVa, &w) && d == was;
    };
    if (!rows.empty()) {
        std::vector<unsigned char> flipped = body, shorter(body.begin(), body.end() - 1);
        flipped[3] ^= 1;
        check(base_refused(rows[0], flipped, file_range, 0), "a base table file of another hash refused");
        check(base_refused(rows[0], shorter, file_range, 0), "a short base table file refused");
        check(base_refused(rows[0], body, Range{0x1014, 0x1024, "file", "tab"}, 0), "a base table whose range is not its row's refused");
        check(base_refused(rows[0], body, file_range, 7), "a base table over bytes data.bin does not leave zero refused");
        // a table with a hole: two file ranges of the one table, the bytes between them another range's
        std::vector<unsigned char> d(kSize, 0);
        d[0x14] = 0x77;   // the hole 0x1014..0x1016, as the image has it
        d[0x15] = 0x78;
        std::string w;
        bool holes = LayBaseTable(rows[0], body, Range{0x1010, 0x1014, "file", "tab"}, d.data(), kVa, &w) &&
                     LayBaseTable(rows[0], body, Range{0x1016, 0x1020, "file", "tab"}, d.data(), kVa, &w);
        for (std::uint32_t i = 0; i < 16 && holes; ++i)
            holes = d[0x10 + i] == (i == 4 ? 0x77 : i == 5 ? 0x78 : body[i]);
        check(holes, "a table with a hole: each file range lays its own part of the file, the hole left as the image has it");
        check(base_refused(rows[0], body, Range{0x100C, 0x1014, "file", "tab"}, 0), "a file range that starts before its table refused");
    }
    // Two layers. A changes record 1 whole and byte +2 of record 3; B changes
    // bytes +1..+2 of record 1 (two bytes A wrote, with other values) and
    // byte +0 of record 0.
    std::vector<unsigned char> a_file = body, b_file = body;
    for (int k = 0; k < 4; ++k) a_file[4 + k] = static_cast<unsigned char>(0xA0 + k);
    a_file[14] = 0xAE;
    b_file[5] = 0xB1;
    b_file[6] = 0xB2;
    b_file[0] = 0xB0;
    const auto layer_text = [&](const std::vector<unsigned char>& file, const char* cells) {
        return "build = \"psp-xx\"\n\ntables = [\n  [\"tab\", 0x001010, 4, 4, \"" + sha(file) + "\"],\n]\n\ncells = [\n" +
               cells + "]\n";
    };
    const std::string a_text = layer_text(a_file, "  [\"tab\", 1, 0, 4],\n  [\"tab\", 3, 2, 1],\n");
    const std::string b_text = layer_text(b_file, "  [\"tab\", 1, 1, 2],\n  [\"tab\", 0, 0, 1],\n");
    LayerTables la, lb;
    check(ParseLayerTables(a_text, &la, &why) && la.tables.size() == 1 && la.cells.size() == 2 &&
              ParseLayerTables(b_text, &lb, &why) && lb.cells.size() == 2,
          "two layers' tables.toml parsed");
    Laid both;
    both.owner.assign(kSize, 0);
    Image mapped_ok;
    mapped_ok.build = "psx-xx";
    mapped_ok.ranges = {{0x1000, 0x1010, "map", ""}, {0x1010, 0x1020, "file", "tab"}, {0x1020, 0x1040, "layout", ""}};
    const int ca = LayLayerTables(la, {a_file}, img.data(), kVa, kSize, &mapped_ok, &rows, &why, 1, &both);
    const int cb = LayLayerTables(lb, {b_file}, img.data(), kVa, kSize, &mapped_ok, &rows, &why, 2, &both);
    std::vector<unsigned char> want(kSize, 0);
    want[0] = 0x11;
    for (std::uint32_t i = 0; i < 16; ++i) want[0x10 + i] = body[i];
    want[0x10] = 0xB0;
    want[0x14] = 0xA0;
    want[0x15] = 0xB1;
    want[0x16] = 0xB2;
    want[0x17] = 0xA3;
    want[0x1E] = 0xAE;
    check(ca == 5 && cb == 3 && img == want, "two layers laid: only their cells, the later layer's value where both wrote");
    check(both.conflicts.size() == 1 && both.conflicts[0].what == "tab" && both.conflicts[0].earlier == 1 &&
              both.conflicts[0].bytes == 2,
          "the two bytes both layers wrote counted once, against the earlier layer");
    const auto layer_refused = [&](const std::string& text, const std::vector<unsigned char>& file, const Image* mapped) {
        LayerTables u;
        std::string w;
        std::vector<unsigned char> d = want;
        const bool no = !ParseLayerTables(text, &u, &w) || LayLayerTables(u, {file}, d.data(), kVa, kSize, mapped, &rows, &w) < 0;
        return no && d == want;
    };
    std::vector<unsigned char> a_flipped = a_file, a_short(a_file.begin(), a_file.end() - 1);
    a_flipped[9] ^= 1;
    check(layer_refused(a_text, a_flipped, nullptr), "a layer's table file of another hash refused");
    check(layer_refused(a_text, a_short, nullptr), "a short layer table file refused");
    check(layer_refused(layer_text(a_file, "  [\"tab\", 3, 2, 3],\n"), a_file, nullptr), "a cell past the table refused");
    check(layer_refused(layer_text(a_file, "  [\"tab\", 4, 0, 1],\n"), a_file, nullptr), "a cell of a record past the count refused");
    check(layer_refused(layer_text(a_file, "  [\"other\", 0, 0, 1],\n"), a_file, nullptr), "a cell of a table the layer does not carry refused");
    Image must;
    must.build = "psx-xx";
    must.ranges = {{0x1000, 0x1018, "map", ""}, {0x1018, 0x1040, "layout", ""}};
    check(layer_refused(a_text, a_file, &must), "a cell on a must-equal class refused");
    std::string moved = a_text;
    moved.replace(moved.find("0x001010"), 8, "0x001014");
    check(layer_refused(moved, a_file, nullptr), "a layer whose table is not at the base's place refused");
    std::string out_of_data = a_text;
    out_of_data.replace(out_of_data.find("0x001010"), 8, "0x001038");
    {
        LayerTables u;
        std::string w;
        std::vector<unsigned char> d = want;
        check(ParseLayerTables(out_of_data, &u, &w) && LayLayerTables(u, {a_file}, d.data(), kVa, kSize, nullptr, nullptr, &w) < 0 &&
                  d == want,
              "a table that runs past .data refused");
    }
    if (ok != checks) Fatal("DIV-0111 table-files self-test: %u of %u checks right (each WRONG line above)", ok, checks);
    Log("shadow      DIV-0111 table-files self-test: %u checks right - the base's tables.toml parsed and its file laid at its range; "
        "four bad base files refused (hash, size, range, bytes not zero under it); two layers' cells laid, the later "
        "winning two bytes, counted; eight bad layers refused with nothing laid (hash, size, a cell past the table, a "
        "record past the count, a table not carried, a must-equal class, a table not at the base's place, a table "
        "past .data)",
        checks);
}

void ExeImage_LogMapped(const void* at, std::uint32_t size) {
    std::string hex;
    if (!bof3x::Sha256HexBytes(static_cast<const unsigned char*>(at), size, hex)) Fatal("exe_image: no SHA-256 of .data");
    Log("exe_image   .data as mapped (the image or the file, the table files, the fills, the layers): %u bytes, sha256 %s",
        (unsigned)size, hex.c_str());
}

void ExeImage_CompareWithFile() {
    if (!g_disc_mapped) return;
    const Image& im = g_mapped;
    if (!ExeFile_Present()) {
        Log("BOF3X_EXEIMAGE: no BOF3.exe to compare the disc-built image with; recipes/exe-gaps.tsv is what says "
            "where it may differ from the PC's");
        return;
    }
    std::vector<unsigned char> pc(im.size);
    ExeFile_Read(im.va, pc.data(), im.size);
    const auto* live = reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(Img(im.va)));
    struct Count {
        std::uint32_t ranges, bytes, differ_ranges, differ;
    } by[sizeof kClasses / sizeof kClasses[0]] = {};
    std::uint32_t equal = 0;
    for (const Range& r : im.ranges) {
        const Class* c = ClassOf(r);
        Count& k = by[c - kClasses];
        std::uint32_t differ = 0, first = 0;
        for (std::uint32_t a = r.lo; a < r.hi; ++a)
            if (live[a - im.va] != pc[a - im.va]) {
                if (!differ) first = a;
                ++differ;
            }
        k.ranges += 1;
        k.bytes += r.hi - r.lo;
        equal += (r.hi - r.lo) - differ;
        if (!differ) continue;
        if (c->must_equal)
            Fatal("BOF3X_EXEIMAGE: 0x%06X..0x%06X (%s%s%s) must equal BOF3.exe's .data after the fills, but %u of its "
                  "%u bytes differ, the first at 0x%06X (ours 0x%02X, the file's 0x%02X)",
                  (unsigned)r.lo, (unsigned)r.hi, r.how.c_str(), r.from.empty() ? "" : " ", r.from.c_str(),
                  (unsigned)differ, (unsigned)(r.hi - r.lo), (unsigned)first, (unsigned)live[first - im.va],
                  (unsigned)pc[first - im.va]);
        k.differ_ranges += 1;
        k.differ += differ;
        Log("BOF3X_EXEIMAGE  differs  %-8s 0x%06X..0x%06X %5u of %5u bytes, the first 0x%06X  %s", r.how.c_str(),
            (unsigned)r.lo, (unsigned)r.hi, (unsigned)differ, (unsigned)(r.hi - r.lo), (unsigned)first, r.from.c_str());
    }
    for (std::size_t i = 0; i < sizeof kClasses / sizeof kClasses[0]; ++i)
        if (by[i].ranges)
            Log("BOF3X_EXEIMAGE  against BOF3.exe  %-8s %5u ranges %7u bytes: %5u ranges, %6u bytes differ",
                kClasses[i].how, (unsigned)by[i].ranges, (unsigned)by[i].bytes, (unsigned)by[i].differ_ranges,
                (unsigned)by[i].differ);
    Log("BOF3X_EXEIMAGE: the mapped disc-built image (%s) after the fills against BOF3.exe's .data: %u of %u bytes "
        "equal; every class that must equal does",
        im.build.c_str(), (unsigned)equal, (unsigned)im.size);
}

}  // namespace bof3

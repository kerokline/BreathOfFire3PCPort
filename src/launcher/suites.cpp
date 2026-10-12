#include "launcher/suites.h"

#include <windows.h>

#include <algorithm>
#include <string>
#include <vector>

#include "game/language_tags.h"
#include "launcher/config.h"

namespace bof3x {
namespace {

using suites_gen::Kind;
using suites_gen::OverPsx;
using suites_gen::Role;

std::string Trim(const std::string& s) {
    const size_t b = s.find_first_not_of(" \t\r");
    if (b == std::string::npos) return std::string();
    return s.substr(b, s.find_last_not_of(" \t\r") - b + 1);
}

// Every basic string ("...", JSON escapes as toml_str writes them) on `line`,
// in order. \" and \\ are unescaped; any other escape is kept as written, which
// is enough for build ids and paths.
std::vector<std::string> Strings(const std::string& line) {
    std::vector<std::string> out;
    size_t i = 0;
    while ((i = line.find('"', i)) != std::string::npos) {
        std::string s;
        size_t j = i + 1;
        for (; j < line.size() && line[j] != '"'; ++j) {
            if (line[j] == '\\' && j + 1 < line.size()) {
                const char e = line[++j];
                if (e != '"' && e != '\\') s += '\\';
                s += e;
            } else {
                s += line[j];
            }
        }
        if (j >= line.size()) break;   // unterminated: not a string
        out.push_back(s);
        i = j + 1;
    }
    return out;
}

// `value` as exactly one string, or false.
bool OneString(const std::string& value, std::string& out) {
    if (value.size() < 2 || value.front() != '"') return false;
    const std::vector<std::string> s = Strings(value);
    if (s.size() != 1) return false;
    out = s[0];
    return true;
}

// `value` as a one-line array of strings, or false.
bool StringList(const std::string& value, std::vector<std::string>& out) {
    if (value.size() < 2 || value.front() != '[' || value.back() != ']') return false;
    out = Strings(value);
    // Nothing but strings, commas and blanks between the brackets.
    std::string rest;
    bool in = false;
    for (size_t i = 1; i + 1 < value.size(); ++i) {
        const char c = value[i];
        if (c == '"' && (i == 0 || value[i - 1] != '\\')) in = !in;
        else if (!in && c != ',' && c != ' ' && c != '\t') return false;
    }
    return !in;
}

std::vector<std::string> SplitComma(const std::string& s) {
    std::vector<std::string> out;
    size_t at = 0;
    while (at <= s.size()) {
        size_t end = s.find(',', at);
        if (end == std::string::npos) end = s.size();
        if (end > at) out.push_back(s.substr(at, end - at));
        at = end + 1;
    }
    return out;
}

std::wstring Wide(const std::string& s) {
    std::wstring w;
    for (unsigned char c : s) w += static_cast<wchar_t>(c);
    return w;
}

// "a", "a or b", "a, b or c".
std::wstring OrList(const std::vector<std::string>& ids) {
    std::wstring out;
    for (size_t i = 0; i < ids.size(); ++i) {
        if (i) out += i + 1 == ids.size() ? L" or " : L", ";
        out += Wide(ids[i]);
    }
    return out;
}

// A text layer's language by its primary subtag (kLanguageTags' four), else the tag.
std::wstring LanguageName(const char* tag) {
    static constexpr struct { const char* primary; const wchar_t* name; } kNames[] = {
        {"en", L"English"}, {"fr", L"French"}, {"de", L"German"}, {"ja", L"Japanese"}};
    for (const auto& n : kNames)
        if (SamePrimaryLanguage(tag, n.primary)) return n.name;
    return Wide(tag);
}

bool Contains(const std::vector<std::string>& v, const std::string& s) {
    return std::find(v.begin(), v.end(), s) != v.end();
}

bool IsRole(const std::string& id, Role want) {
    Role r;
    return SuiteBuildRole(id, r) && r == want;
}

}  // namespace

bool SuiteBuildRole(const std::string& id, Role& role) {
    for (const suites_gen::BuildInfo& b : suites_gen::kBuilds)
        if (id == b.id) {
            role = b.role;
            return true;
        }
    return false;
}

CacheManifest ManifestParse(const std::string& text) {
    CacheManifest m;
    m.present = true;
    std::string table, array_key, exe_build;
    bool in_array = false, tables_given = false;
    size_t at = 0;
    while (at < text.size()) {
        size_t end = text.find('\n', at);
        if (end == std::string::npos) end = text.size();
        const std::string line = Trim(text.substr(at, end - at));
        at = end + 1;
        if (in_array) {
            if (line == "]") {
                in_array = false;
            } else if (table == "cache" && array_key == "bgm" && m.bgm_build.empty()) {
                // ["base/bgm/000.DAT", "psx-us:seq:BGM000.EMI#1", "<sha256>"]
                const std::vector<std::string> s = Strings(line);
                if (s.size() >= 2) m.bgm_build = s[1].substr(0, s[1].find(':'));
            }
            continue;
        }
        if (line.empty() || line[0] == '#') continue;
        if (line[0] == '[') {
            table = line;
            while (!table.empty() && table.front() == '[') table.erase(table.begin());
            while (!table.empty() && table.back() == ']') table.pop_back();
            table = Trim(table);
            continue;
        }
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = Trim(line.substr(0, eq)), value = Trim(line.substr(eq + 1));
        if (!value.empty() && value.front() == '[' && value.back() != ']') {
            in_array = true;
            array_key = key;
            continue;
        }
        std::string s;
        if (table == "source" && key == "build") {
            if (OneString(value, s) && !Contains(m.sources, s)) m.sources.push_back(s);
        } else if (table == "build" && key == "base_policy") {
            if (!OneString(value, s) || (s != "psp" && s != "psx"))
                m.error = "manifest.toml's [build] base_policy is " + value + ", neither \"psp\" nor \"psx\"";
            else
                m.base_policy = s;
        } else if (table == "build" && key == "exe_tables_from") {
            std::vector<std::string> list;
            if (OneString(value, s)) list.push_back(s);
            else if (!StringList(value, list)) m.error = "manifest.toml's [build] exe_tables_from is " + value +
                                                         ", neither a build id nor a list of them";
            m.exe_tables_from = list;
            tables_given = true;
        } else if (table == "exe" && key == "build") {
            if (OneString(value, s)) exe_build = s;
        } else if (table == "snd" && key == "build") {
            if (OneString(value, s)) m.snd_build = s;
        }
    }
    if (!tables_given && !exe_build.empty()) m.exe_tables_from.push_back(exe_build);
    return m;
}

CacheManifest ManifestRead(const std::wstring& cache) {
    if (cache.empty()) return CacheManifest();
    HANDLE f = CreateFileW((cache + L"\\manifest.toml").c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return CacheManifest();
    std::string text;
    char buf[65536];
    DWORD got = 0;
    while (ReadFile(f, buf, sizeof buf, &got, nullptr) && got > 0) text.append(buf, got);
    CloseHandle(f);
    return ManifestParse(text);
}

std::vector<std::string> SuiteLayers() {
    std::vector<std::string> out;
    for (const suites_gen::MemberInfo& m : suites_gen::kMembers)
        if (m.layer) out.push_back(m.layer);
    return out;
}

std::vector<SuiteState> SuitesEvaluate(const SuiteInputs& in) {
    const CacheManifest& man = in.manifest;
    const bool psp_base = man.error.empty() && man.base_policy == "psp";
    bool any_psx = false;
    for (const std::string& b : man.sources) any_psx = any_psx || IsRole(b, Role::kPsx) || IsRole(b, Role::kPc);
    std::vector<std::string> psx_discs;
    for (const suites_gen::BuildInfo& b : suites_gen::kBuilds)
        if (b.role == Role::kPsx) psx_discs.push_back(b.id);

    // What opt= plays now, as ConfigOptWanted reads it.
    std::vector<std::string> played;
    if (in.opt.empty()) played = in.defaults;
    else if (in.opt != "none") played = SplitComma(in.opt);

    // The base's own version of a member that lives outside opt/: present, or why not.
    auto base_note = [&](const std::string& build, const wchar_t* what) -> std::wstring {
        if (build.empty()) return std::wstring(L"no ") + what + L" in this cache";
        if (!IsRole(build, Role::kPsp)) return std::wstring(L"this cache's ") + what + L" are " + Wide(build) + L"'s";
        return std::wstring();
    };

    std::vector<SuiteState> out;
    for (int i = 0; i < static_cast<int>(std::size(suites_gen::kSuites)); ++i) {
        SuiteState s;
        s.info = &suites_gen::kSuites[i];
        s.psp_base = psp_base;
        std::vector<std::string> wanted_discs;   // every PSP build a member names
        bool any_from = false;
        for (const suites_gen::MemberInfo& m : suites_gen::kMembers) {
            if (m.suite != i) continue;
            SuiteMemberState ms;
            ms.info = &m;
            const std::vector<std::string> from = SplitComma(m.from);
            bool have = false;
            for (const std::string& b : from) {
                have = have || Contains(man.sources, b);
                if (!Contains(wanted_discs, b)) wanted_discs.push_back(b);
            }
            if (m.layer) s.layers_all.push_back(m.layer);
            if (!man.error.empty()) {
                ms.note = L"see the note above";
            } else if (psp_base) {
                std::wstring note;
                if (m.kind == Kind::kBgm) note = base_note(man.bgm_build, L"songs");
                else if (m.kind == Kind::kSnd) note = base_note(man.snd_build, L"effects");
                else if (m.kind == Kind::kExe) {
                    std::string psp;
                    for (const std::string& b : man.exe_tables_from)
                        if (IsRole(b, Role::kPsp)) psp = b;
                    note = man.exe_tables_from.empty() ? L"no exe tables in this cache"
                           : psp.empty()               ? L"this cache's tables are " + OrList(man.exe_tables_from) + L"'s"
                                                       : std::wstring();
                }
                ms.available = note.empty();
                ms.note = note;
            } else {
                // Without a manifest an installed layer (DAT\, importer.py install) is
                // its own evidence of the disc; nothing else is.
                const bool from_ok = man.present ? have : (m.layer && in.installed(m.layer));
                any_from = any_from || from_ok;
                if (!from_ok) {
                    ms.note = L"needs " + OrList(from);
                } else if (m.over_psx != OverPsx::kLayer) {
                    ms.note = m.why;
                } else if (!in.installed(m.layer)) {
                    ms.note = man.present ? L"not in the cache (build with --opt " + Wide(m.layer) + L")" : L"not installed";
                } else {
                    ms.available = true;
                    // A text layer plays under its language's primary subtag, any
                    // region (SamePrimaryLanguage, the DLL's test): named so.
                    const char* tag = LayerLanguage(m.layer);
                    if (tag && !SamePrimaryLanguage(in.language.c_str(), tag))
                        ms.note = L"plays under " + LanguageName(tag) + L" text only";
                    s.layers_on.push_back(m.layer);
                }
            }
            s.members.push_back(ms);
        }

        // What opt= plays of this suite.
        bool any_played = false, all_on = !s.layers_on.empty();
        for (const std::string& l : s.layers_all) any_played = any_played || Contains(played, l);
        for (const std::string& l : s.layers_on) all_on = all_on && Contains(played, l);
        s.side = !any_played ? SuiteSide::kPsx : all_on ? SuiteSide::kPsp : SuiteSide::kMixed;

        if (!man.error.empty()) {
            s.psx_why = s.psp_why = Wide(man.error);
        } else if (psp_base) {
            s.side = SuiteSide::kPsp;
            s.psp_enabled = true;
            s.psx_why = any_psx ? std::wstring(L"The PlayStation's version over a PSP base is not built yet.")
                                : L"The PlayStation's version needs a PlayStation disc (" + OrList(psx_discs) + L").";
        } else {
            s.psx_enabled = true;
            s.psp_enabled = !s.layers_on.empty();
            if (!s.psp_enabled)
                s.psp_why = !any_from ? L"The PSP's version needs a PSP disc (" + OrList(wanted_discs) + L")" +
                                            (man.present ? L"." : L" in a cache named in Cache folder.")
                                      : std::wstring(L"Nothing of the PSP's plays beside this base yet.");
        }
        out.push_back(s);
    }
    return out;
}

std::string SuitesOptEdit(const std::string& opt, const std::vector<std::string>& defaults,
                          const std::vector<SuiteState>& suites, const std::vector<int>& choice) {
    std::vector<std::pair<std::string, bool>> boxes;
    for (size_t i = 0; i < suites.size() && i < choice.size(); ++i) {
        const SuiteState& s = suites[i];
        if (s.psp_base || choice[i] < 0) continue;
        if (choice[i] == 1 && s.psp_enabled)
            for (const std::string& l : s.layers_on) boxes.emplace_back(l, true);
        if (choice[i] == 0 && s.psx_enabled)
            for (const std::string& l : s.layers_all) boxes.emplace_back(l, false);
    }
    return ConfigOptEdit(opt, defaults, boxes);
}

}  // namespace bof3x

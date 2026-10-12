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

    // What opt= plays now, as ConfigOptWanted reads it; and psp=, as the DLL
    // reads BOF3X_PSP (src/game/psp_code.cpp).
    std::vector<std::string> played;
    if (in.opt.empty()) played = in.defaults;
    else if (in.opt != "none") played = SplitComma(in.opt);
    const std::vector<std::string> codes_played = SuitesPspPlayed(in.psp, psp_base);

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
            if (m.code) s.codes_all.push_back(m.code);
            if (!man.error.empty()) {
                ms.note = L"see the note above";
            } else if (m.kind == Kind::kCode) {
                // The PSP's battle rules (DIV F draft): code of ours, the disc
                // only the evidence that the player has the PSP's version.
                ms.available = have;
                if (!have) ms.note = L"needs " + OrList(from);
                else if (psp_base && !Contains(codes_played, m.code)) ms.note = L"off in bof3x.ini's psp=";
                if (have && !psp_base) s.codes_on.push_back(m.code);
                any_from = any_from || have;
            } else if (psp_base) {
                std::wstring note;
                // A member of another PSP build than the cache's (the English
                // renames on a PSP-JP base) is not this base's.
                if (!have) note = L"needs " + OrList(from);
                else if (m.kind == Kind::kBgm) note = base_note(man.bgm_build, L"songs");
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
        bool any_played = false, all_on = !s.layers_on.empty() || !s.codes_on.empty();
        for (const std::string& l : s.layers_all) any_played = any_played || Contains(played, l);
        for (const std::string& l : s.layers_on) all_on = all_on && Contains(played, l);
        for (const std::string& c : s.codes_all) any_played = any_played || Contains(codes_played, c);
        for (const std::string& c : s.codes_on) all_on = all_on && Contains(codes_played, c);
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
            s.psp_enabled = !s.layers_on.empty() || !s.codes_on.empty();
            if (!s.psp_enabled)
                s.psp_why = !any_from ? L"The PSP's version needs a PSP disc (" + OrList(wanted_discs) + L")" +
                                            (man.present ? L"." : L" in a cache named in Cache folder.")
                                      : std::wstring(L"Nothing of the PSP's plays beside this base yet.");
        }
        out.push_back(s);
    }
    return out;
}

ChordState ChordEvaluate(const CacheManifest& man, bool installed, const std::string& opt,
                         const std::vector<std::string>& defaults) {
    ChordState c;
    std::vector<std::string> played;
    if (opt.empty()) played = defaults;
    else if (opt != "none") played = SplitComma(opt);
    c.checked = installed && Contains(played, kChordLayer);
    std::vector<std::string> psp;
    for (const suites_gen::BuildInfo& b : suites_gen::kBuilds)
        if (b.role == Role::kPsp) psp.push_back(b.id);
    bool have = false;
    for (const std::string& b : man.sources) have = have || IsRole(b, Role::kPsp);
    if (!man.error.empty()) {
        c.why = Wide(man.error);
    } else if (installed) {
        c.enabled = true;   // with no manifest an installed layer is its own evidence, as the suites take it
    } else if (!man.present) {
        c.why = L"Needs a cache built with a PSP disc (" + OrList(psp) + L") in Cache folder.";
    } else if (!have) {
        c.why = L"Needs a PSP disc (" + OrList(psp) + L") among the cache's sources.";
    } else {
        c.why = L"Not in this cache: build it again (the importer writes it whenever a PSP disc is a source).";
    }
    return c;
}

std::string SuitesOptEdit(const std::string& opt, const std::vector<std::string>& defaults,
                          const std::vector<SuiteState>& suites, const std::vector<int>& choice,
                          const std::vector<std::pair<std::string, bool>>& extra) {
    std::vector<std::pair<std::string, bool>> boxes = extra;
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

std::vector<std::string> SuitesPspPlayed(const std::string& psp, bool psp_base) {
    std::vector<std::string> out;
    if (psp == "none") return out;
    if (!psp.empty()) return SplitComma(psp);
    if (psp_base)
        for (const suites_gen::MemberInfo& m : suites_gen::kMembers)
            if (m.code) out.push_back(m.code);
    return out;
}

std::string SuitesPspEdit(const std::string& psp, const std::vector<SuiteState>& suites, const std::vector<int>& choice) {
    std::vector<std::string> now = SuitesPspPlayed(psp, false), want = now;
    for (size_t i = 0; i < suites.size() && i < choice.size(); ++i) {
        const SuiteState& s = suites[i];
        if (s.psp_base || choice[i] < 0) continue;
        if (choice[i] == 1 && s.psp_enabled)
            for (const std::string& c : s.codes_on)
                if (!Contains(want, c)) want.push_back(c);
        if (choice[i] == 0 && s.psx_enabled)
            for (const std::string& c : s.codes_all) want.erase(std::remove(want.begin(), want.end(), c), want.end());
    }
    bool same = want.size() == now.size();
    for (const std::string& c : want) same = same && Contains(now, c);
    if (same) return psp;
    std::string out;
    for (const suites_gen::MemberInfo& m : suites_gen::kMembers)
        if (m.code && Contains(want, m.code)) out += (out.empty() ? "" : ",") + std::string(m.code);
    for (const std::string& c : want) {
        bool known = false;
        for (const suites_gen::MemberInfo& m : suites_gen::kMembers) known = known || (m.code && c == m.code);
        if (!known) out += (out.empty() ? "" : ",") + c;
    }
    if (out.empty()) return psp.empty() ? std::string() : std::string("none");
    return out;
}

}  // namespace bof3x

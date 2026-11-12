#include "hook/behaviour.h"

#include <cstdint>
#include <cstring>

#include "hook/host.h"
#include "hook/log.h"
#include "hook/detour.h"
#include "os/os.h"

namespace bof3 {
namespace {

// The answers, fixed by Behaviour_Read. One spare slot: the generated list may
// be empty (no behaviour is built yet), and a zero-length array is not C++.
bool g_on[behaviours::kCount + 1];
bool g_read = false;

// One token of a list: [p, end), with the `-` of an exclusion stripped.
struct Token {
    const char* p;
    std::size_t n;
    bool minus;
    bool star;
};

// The next token at or after `p`; false at the end. Separators are ',' and ' ',
// as BOF3X_ORIGINAL's and BOF3X_OFF's (detour.cpp NameListed).
bool NextToken(const char*& p, Token& t) {
    while (*p == ',' || *p == ' ') ++p;
    if (!*p) return false;
    const char* end = p;
    while (*end && *end != ',' && *end != ' ') ++end;
    t.minus = *p == '-';
    t.p = t.minus ? p + 1 : p;
    t.n = static_cast<std::size_t>(end - t.p);
    t.star = !t.minus && t.n == 1 && *t.p == '*';
    p = end;
    return true;
}

int IndexOf(const char* const* names, int count, const char* s, std::size_t n) {
    for (int i = 0; i < count; ++i)
        if (std::strlen(names[i]) == n && std::memcmp(names[i], s, n) == 0) return i;
    return -1;
}

}  // namespace

bool BehaviourParse(const char* list, const char* const* names, int count, bool* on, bool strict,
                    const char** bad, std::size_t* bad_len) {
    for (int i = 0; i < count; ++i) on[i] = false;
    // First what is named or `*`, then the exclusions: "an exclusion wins
    // wherever it stands" (NameListed's rule).
    bool all = false;
    Token t;
    for (const char* p = list; NextToken(p, t);) {
        if (t.star) {
            all = true;
            continue;
        }
        const int i = IndexOf(names, count, t.p, t.n);
        if (i < 0) {
            if (!strict) continue;
            if (bad) *bad = t.minus ? t.p - 1 : t.p;
            if (bad_len) *bad_len = t.minus ? t.n + 1 : t.n;
            return false;
        }
        if (!t.minus) on[i] = true;
    }
    if (all)
        for (int i = 0; i < count; ++i) on[i] = true;
    for (const char* p = list; NextToken(p, t);) {
        if (!t.minus) continue;
        const int i = IndexOf(names, count, t.p, t.n);
        if (i >= 0) on[i] = false;
    }
    return true;
}

bool KnownBehaviour(const char* s, std::size_t n) {
    return IndexOf(behaviours::kNames, behaviours::kCount, s, n) >= 0;
}

bool BehaviourOn(behaviours::Id id) {
    const int i = static_cast<int>(id);
    if (!g_read)
        Fatal("the behaviour %s was asked about before BOF3X_ON was read (Behaviour_Read, first in InjectAll): the "
              "answer would be \"off\" whatever the list says. Ask it from game code, not from a constructor or "
              "an Inject that runs before InjectAll",
              i >= 0 && i < behaviours::kCount ? behaviours::kNames[i] : "?");
    return i >= 0 && i < behaviours::kCount && g_on[i];
}

void Behaviour_Read() {
    if (g_read) Fatal("Behaviour_Read called twice: BOF3X_ON is read once, first in InjectAll");
    static char on_list[8192];
    static char off_list[8192];
    std::uint32_t n = os::GetEnv("BOF3X_ON", on_list, sizeof on_list);
    if (n >= sizeof on_list) Fatal("BOF3X_ON is longer than %u bytes", (unsigned)sizeof on_list - 1);
    if (n == 0) on_list[0] = '\0';
    n = os::GetEnv("BOF3X_OFF", off_list, sizeof off_list);
    if (n >= sizeof off_list) Fatal("BOF3X_OFF is longer than %u bytes", (unsigned)sizeof off_list - 1);
    if (n == 0) off_list[0] = '\0';
    bool off[behaviours::kCount + 1] = {};
    const char* bad = nullptr;
    std::size_t bad_len = 0;
    if (!BehaviourParse(on_list, behaviours::kNames, behaviours::kCount, g_on, true, &bad, &bad_len))
        Fatal("BOF3X_ON names \"%.*s\", which is not a behaviour of this release. BOF3X_ON turns on the "
              "behaviours a mod needs (docs/mods.md section 11); this release has %d: python "
              "tools/ledger_switches.py lists them in src/hook/code_names.h. A mod that needs it needs a newer release",
              (int)bad_len, bad, behaviours::kCount);
    BehaviourParse(off_list, behaviours::kNames, behaviours::kCount, off, false, nullptr, nullptr);
    int count = 0;
    for (int i = 0; i < behaviours::kCount; ++i) {
        if (g_on[i] && off[i]) {
            g_on[i] = false;
            Log("behaviour OFF %-24s BOF3X_ON asks for it and BOF3X_OFF turns it off: off wins", behaviours::kNames[i]);
        } else if (g_on[i]) {
            Log("behaviour ON  %-24s (BOF3X_ON)", behaviours::kNames[i]);
            ++count;
        }
    }
    g_read = true;
    if (*on_list) Log("BOF3X_ON=%s", on_list);
    Log("behaviours: %d of %d on", count, behaviours::kCount);
    if (IsExeHost() ? os::GetEnv("BOF3X_SELFTEST_ONLY", nullptr, 0) > 0 : WantsShadow("behaviours"))
        Behaviour_SelfTest();
}

void Behaviour_SelfTest() {
    // Names of this test's own: no ledger entry has them, no game code asks
    // for them, and BOF3X_ON never reaches this table.
    static const char* const kNames[] = {"SelfTestAlpha", "SelfTestBeta", "SelfTestGamma"};
    constexpr int kN = 3;
    struct Case {
        const char* list;
        bool ok;
        bool on[kN];
        const char* bad;   // the token named when !ok
    };
    static const Case kCases[] = {
        {"", true, {false, false, false}, nullptr},
        {"SelfTestBeta", true, {false, true, false}, nullptr},
        {"SelfTestGamma,SelfTestAlpha", true, {true, false, true}, nullptr},
        {" SelfTestAlpha , SelfTestGamma ", true, {true, false, true}, nullptr},
        {"*", true, {true, true, true}, nullptr},
        {"*,-SelfTestBeta", true, {true, false, true}, nullptr},
        {"-SelfTestBeta,*", true, {true, false, true}, nullptr},        // an exclusion wins wherever it stands
        {"SelfTestAlpha,-SelfTestAlpha", true, {false, false, false}, nullptr},
        {"SelfTestAlph", false, {}, "SelfTestAlph"},                     // a prefix is no name
        {"SelfTestAlpha,NoSuchName", false, {}, "NoSuchName"},
        {"-NoSuchName", false, {}, "-NoSuchName"},
    };
    int checks = 0;
    for (const Case& c : kCases) {
        bool on[kN];
        const char* bad = nullptr;
        std::size_t bad_len = 0;
        const bool ok = BehaviourParse(c.list, kNames, kN, on, true, &bad, &bad_len);
        if (ok != c.ok) Fatal("behaviours self-test: \"%s\" parsed %s", c.list, ok ? "without a refusal" : "with a refusal");
        if (ok) {
            for (int i = 0; i < kN; ++i)
                if (on[i] != c.on[i])
                    Fatal("behaviours self-test: \"%s\" gives %s %s", c.list, kNames[i], on[i] ? "on" : "off");
        } else if (!bad || bad_len != std::strlen(c.bad) || std::memcmp(bad, c.bad, bad_len) != 0) {
            Fatal("behaviours self-test: \"%s\" named the wrong token", c.list);
        }
        ++checks;
    }
    // Not strict (BOF3X_OFF's walk): a token of another kind - a code name - is passed over.
    bool on[kN];
    if (!BehaviourParse("SunderLoop,SelfTestGamma", kNames, kN, on, false, nullptr, nullptr) || on[0] || on[1] || !on[2])
        Fatal("behaviours self-test: BOF3X_OFF's walk did not pass a code name over");
    ++checks;
    // Every generated name is itself and nothing else.
    for (int i = 0; i < behaviours::kCount; ++i) {
        if (!KnownBehaviour(behaviours::kNames[i], std::strlen(behaviours::kNames[i])))
            Fatal("behaviours self-test: the generated name %s is not known", behaviours::kNames[i]);
        if (i > 0 && std::strcmp(behaviours::kNames[i - 1], behaviours::kNames[i]) >= 0)
            Fatal("behaviours self-test: the generated names are not sorted at %s", behaviours::kNames[i]);
        ++checks;
    }
    if (KnownBehaviour("SelfTestAlpha", 13)) Fatal("behaviours self-test: a test name is in the generated list");
    ++checks;
    Log("behaviours self-test: %d checks passed", checks);
}

}  // namespace bof3

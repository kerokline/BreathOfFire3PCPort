#include "game/psp_code.h"

#include <windows.h>

#include <cstdint>
#include <cstring>
#include <string>

#include "hook/detour.h"
#include "hook/log.h"

// DIV-0108: the PSP's two battle rules and their switch. What each rule
// is, and the PSP instructions it reproduces, is in psp_code.h and
// docs/psp-battle-changes.md; this file holds the switch and the rules' own
// self-test.

namespace psp_code {
namespace {

unsigned g_on = 0;

struct Item {
    const char* name;
    unsigned bit;
};
constexpr Item kItems[] = {{"duel-loss", kDuelLoss}, {"myria-item", kMyriaItem}};

std::string Trim(const std::string& s) {
    const size_t b = s.find_first_not_of(" \t\r");
    if (b == std::string::npos) return std::string();
    return s.substr(b, s.find_last_not_of(" \t\r") - b + 1);
}

// The cache's [build] base_policy, as the launcher reads it (src/launcher/
// suites.cpp ManifestParse): "psp", "psx", or "" (no cache, no manifest, no
// key). Any other value is a Fatal - the launcher greys its suites with the
// same complaint, and the default cannot be guessed.
std::string BasePolicy(std::string& where) {
    wchar_t root[1024];
    const DWORD n = GetEnvironmentVariableW(L"BOF3X_CACHE", root, 1024);
    if (n == 0 || n >= 1024) return std::string();
    std::wstring path(root, n);
    while (!path.empty() && (path.back() == L'\\' || path.back() == L'/')) path.pop_back();
    path += L"\\manifest.toml";
    where = bof3::Utf8Path(path.c_str()).text;
    const HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                                 FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return std::string();
    std::string text;
    char buf[65536];
    DWORD got = 0;
    while (ReadFile(f, buf, sizeof buf, &got, nullptr) && got > 0) text.append(buf, got);
    CloseHandle(f);
    std::string table;
    size_t at = 0;
    while (at < text.size()) {
        size_t end = text.find('\n', at);
        if (end == std::string::npos) end = text.size();
        const std::string line = Trim(text.substr(at, end - at));
        at = end + 1;
        if (line.empty() || line[0] == '#') continue;
        if (line[0] == '[') {
            table = line;
            continue;
        }
        if (table != "[build]") continue;
        const size_t eq = line.find('=');
        if (eq == std::string::npos || Trim(line.substr(0, eq)) != "base_policy") continue;
        const std::string value = Trim(line.substr(eq + 1));
        if (value == "\"psp\"") return "psp";
        if (value == "\"psx\"") return "psx";
        bof3::Fatal("DIV-0108: %s's [build] base_policy is %s, neither \"psp\" nor \"psx\"", where.c_str(), value.c_str());
    }
    return std::string();
}

// BOF3X_PSP as a mask, or false when it is unset.
bool FromEnvironment(unsigned& mask) {
    char list[128];
    const DWORD n = GetEnvironmentVariableA("BOF3X_PSP", list, sizeof list);
    if (n == 0) return false;
    if (n >= sizeof list) bof3::Fatal("DIV-0108: BOF3X_PSP is %lu characters; at most %u", n, (unsigned)sizeof list - 1);
    mask = 0;
    if (std::strcmp(list, "none") == 0) return true;
    const char* p = list;
    while (true) {
        const char* const comma = std::strchr(p, ',');
        const size_t len = comma ? static_cast<size_t>(comma - p) : std::strlen(p);
        unsigned bit = 0;
        for (const Item& it : kItems)
            if (std::strlen(it.name) == len && std::strncmp(it.name, p, len) == 0) bit = it.bit;
        if (bit == 0)
            bof3::Fatal("DIV-0108: BOF3X_PSP names \"%.*s\"; the PSP's changes are duel-loss and myria-item, or none",
                        static_cast<int>(len), p);
        if (mask & bit) bof3::Fatal("DIV-0108: BOF3X_PSP names %.*s twice", static_cast<int>(len), p);
        mask |= bit;
        if (!comma) break;
        p = comma + 1;
    }
    return true;
}

// The rules against their statement, every byte value they read (the
// integration - where each is asked, and that nothing else moves - is the
// battle modules' shadows').
void SelfTest() {
    const unsigned saved = Force(0);
    unsigned bad = 0, duel = 0, item = 0;
    for (unsigned mask = 0; mask <= kAll; ++mask) {
        Force(mask);
        for (unsigned e = 0; e < 0x100; ++e)
            for (unsigned end = 0; end < 0x100; ++end) {
                const bool want = (mask & kDuelLoss) && e == 0x1B && (end & 4) == 0 && (end & 1) != 0;
                if (DuelLossGoesOn(e, end) != want) ++bad;
                duel += want ? 1u : 0u;
            }
        static const unsigned kAreas[] = {0, 197, 198, 199, 198 + 0x100, 198 + 0x8000, 0xFFFF, 0xC6C6};
        for (unsigned area : kAreas)
            for (unsigned e = 0; e < 0x100; ++e) {
                unsigned char record[0x128], before[0x128];
                for (unsigned i = 0; i < sizeof record; ++i) record[i] = static_cast<unsigned char>(i * 37 + e + area);
                std::memcpy(before, record, sizeof record);
                const bool want = (mask & kMyriaItem) && area == 198 && e == 0x37;
                if (MyriaItem(record, area, e) != want) ++bad;
                if (want) {
                    ++item;
                    before[kItemWordAt] = kMyriaItemWord & 0xFF;
                    before[kItemWordAt + 1] = kMyriaItemWord >> 8;
                }
                if (std::memcmp(record, before, sizeof record) != 0) ++bad;
            }
    }
    Force(saved);
    bof3::Log("shadow      psp_code self-test: duel-loss over 4 x 256 x 256 (event, end) pairs, %u went on; myria-item over "
              "4 x 8 areas x 256 events, %u stored the word, the record's other 0x126 bytes untouched; %u MISMATCHES",
              duel, item, bad);
    if (duel != 2 * 64 || item != 2) bof3::Fatal("psp_code self-test: %u duel cases, %u item cases (128, 2 wanted)", duel, item);
    if (bad) bof3::Fatal("psp_code: the rules differ from their statement in %u cases", bad);
}

}  // namespace

unsigned On() { return g_on; }

unsigned Force(unsigned mask) {
    const unsigned was = g_on;
    g_on = mask & kAll;
    return was;
}

bool DuelLossGoesOn(unsigned event_battle, unsigned battle_end) {
    if ((g_on & kDuelLoss) == 0) return false;
    return (event_battle & 0xFF) == kDuelEvent && (battle_end & 4) == 0 && (battle_end & 1) != 0;
}

bool MyriaItem(unsigned char* record, unsigned area, unsigned event_battle) {
    if ((g_on & kMyriaItem) == 0) return false;
    if ((area & 0xFFFF) != kMyriaArea || (event_battle & 0xFF) != kMyriaEvent) return false;
    record[kItemWordAt] = static_cast<unsigned char>(kMyriaItemWord & 0xFF);
    record[kItemWordAt + 1] = static_cast<unsigned char>(kMyriaItemWord >> 8);
    return true;
}

}  // namespace psp_code

void PspCode_Inject() {
    if (bof3::WantsShadow("psp_code")) psp_code::SelfTest();
    unsigned mask = 0;
    const char* why;
    std::string where;
    if (psp_code::FromEnvironment(mask)) {
        why = "BOF3X_PSP";
    } else {
        const std::string policy = psp_code::BasePolicy(where);
        mask = policy == "psp" ? psp_code::kAll : 0;
        why = policy == "psp"   ? "the default: the cache's base_policy is psp"
              : policy == "psx" ? "the default: the cache's base_policy is psx"
              : where.empty()   ? "the default: no cache named"
                                : "the default: the cache's manifest has no base_policy";
    }
    psp_code::Force(mask);
    bof3::Log("DIV-0108    the PSP's battle changes: duel-loss %s, myria-item %s (%s)", mask & psp_code::kDuelLoss ? "on" : "off",
              mask & psp_code::kMyriaItem ? "on" : "off", why);
}

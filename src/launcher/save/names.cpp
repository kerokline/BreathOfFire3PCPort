#include "launcher/save/names.h"

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>

namespace bof3x::save {

namespace {

constexpr std::size_t kNameBytes = 16;

std::uint32_t U32(const std::vector<char>& b, std::size_t at) {
    std::uint32_t v;
    std::memcpy(&v, b.data() + at, 4);
    return v;
}

// One 16-byte name as text. A two-byte glyph code (an accented letter) is
// shown as a question mark: these names are for the lists only, never written.
std::string NameText(const char* p) {
    std::string out;
    for (std::size_t i = 0; i < kNameBytes && p[i]; ++i) {
        const unsigned char c = static_cast<unsigned char>(p[i]);
        if (c & 0x80) {
            out += '?';
            ++i;
        } else {
            out += c == 0x2A ? '"' : static_cast<char>(c);
        }
    }
    return out;
}

}  // namespace

bool LanguageIsLatin(const std::string& language) {
    return language.compare(0, 2, "en") == 0 || language.compare(0, 2, "fr") == 0 || language.compare(0, 2, "de") == 0;
}

Names LoadNames(const std::wstring& first_dat, const NameTags& tags, bool latin) {
    Names names;
    names.latin = latin;
    if (!latin) return names;   // the other installs' glyph codes are not letters the editor can show
    std::ifstream in(std::filesystem::path(first_dat), std::ios::binary);
    if (!in) return names;
    const std::vector<char> b((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    // A .DAT is a flat run of chunks: a 16-byte header (kind, tag, size, a
    // fourth word), then the payload (docs/DAT_CONTAINER.md section 1).
    for (std::size_t p = 0; p + 16 <= b.size();) {
        const std::uint32_t kind = U32(b, p), tag = U32(b, p + 4), size = U32(b, p + 8);
        if (size > b.size() - p - 16) break;
        if (kind == 5 && size % kNameBytes == 0) {
            std::vector<std::string>* to = nullptr;
            std::size_t first = 0;
            if (tag == tags.items) to = &names.item[0];
            else if (tag == tags.weapons) to = &names.item[1];
            else if (tag == tags.armour) to = &names.item[2];
            else if (tag == tags.accessories) to = &names.item[3];
            else if (tag == tags.key_items) to = &names.key;
            else if (tag == tags.abilities) to = &names.ability, first = 1;
            if (to) {
                to->assign(first, std::string());
                for (std::size_t i = 0; i < size / kNameBytes; ++i) to->push_back(NameText(b.data() + p + 16 + i * kNameBytes));
            }
        }
        p += 16 + size;
    }
    return names;
}

void LoadReferenceNames(const std::wstring& tsv, Names& names) {
    std::ifstream in(std::filesystem::path(tsv), std::ios::binary);
    std::string line;
    while (std::getline(in, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        std::vector<std::string> col;
        for (std::size_t at = 0;;) {
            const std::size_t tab = line.find('\t', at);
            col.push_back(line.substr(at, tab == std::string::npos ? std::string::npos : tab - at));
            if (tab == std::string::npos) break;
            at = tab + 1;
        }
        if (col.size() < 3 || col[1].empty() || col[2].empty()) continue;
        char* end = nullptr;
        const auto id = std::strtoul(col[1].c_str(), &end, 0);
        if (*end) continue;
        const std::string& kind = col[0];
        std::map<unsigned, std::string>* to = kind == "master" ? &names.master
                                              : kind == "gene" ? &names.gene
                                              : kind == "formation" ? &names.formation
                                              : kind == "fish" ? &names.fish
                                              : kind == "chapter" ? &names.chapter
                                              : kind == "area" ? &names.area
                                              : nullptr;
        if (to) {
            (*to)[static_cast<unsigned>(id)] = col[2];
        } else if (kind == "entrance" && col.size() >= 5 && !col[3].empty() && !col[4].empty()) {
            Names::Entrance e{static_cast<unsigned>(id), col[2], 0.0, 0.0, -1};
            char* ex = nullptr;
            char* ez = nullptr;
            e.x = std::strtod(col[3].c_str(), &ex);
            e.z = std::strtod(col[4].c_str(), &ez);
            if (*ex || *ez) continue;
            if (col.size() >= 6 && !col[5].empty()) e.flags = static_cast<int>(std::strtol(col[5].c_str(), nullptr, 0)) & 0xFF;
            names.entrances.push_back(std::move(e));
        }
    }
}

}  // namespace bof3x::save

// The default names of each release: see namemap.h.
#include "launcher/save/namemap.h"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace bof3x::save {

namespace {

constexpr std::uint32_t kChars = 0x90, kCharStride = 0xA4, kCharName = 9;
constexpr std::uint32_t kFaeries = 0xCF0, kFaerieStride = 8, kFaerieNames = 0xF10, kFaerieName = 5;

// A name without what fills its field: zero on the PC, 0xFF on the
// PlayStation, a space after the name screen.
NameBytes Strip(const std::uint8_t* p, std::size_t size) {
    while (size && (p[size - 1] == 0 || p[size - 1] == 0xFF || p[size - 1] == 0x20)) --size;
    // A PC name ends at its first zero: what follows in the field is not the name.
    for (std::size_t i = 0; i < size; ++i)
        if (p[i] == 0) size = i;
    return NameBytes(p, p + size);
}

NameSet Empty(const std::string& id, std::uint8_t pad) {
    NameSet s;
    s.id = id;
    s.chars.assign(kNameChars, {});
    s.faeries.assign(kNameFaeries, {});
    s.faerie_pad = pad;
    return s;
}

const NameBytes* Match(const std::vector<NameSet>& known, bool faerie, std::size_t record, const NameBytes& name) {
    for (const NameSet& s : known) {
        const std::vector<NameBytes>& list = faerie ? s.faeries : s.chars;
        if (record >= list.size() || list[record].empty()) continue;
        const NameBytes& have = list[record];
        if (have == name) return &have;
        // The port's own faerie names are two-byte glyphs in a five-byte field: the fifth byte is the first
        // half of a third glyph, which our build does not store (DIV-0200). Both forms are that name.
        if (faerie && have.size() == kFaerieName && have[0] >= 0x80 && have[4] >= 0x80 && name.size() == 4 &&
            std::memcmp(have.data(), name.data(), 4) == 0)
            return &have;
    }
    return nullptr;
}

}  // namespace

bool NameSet::Known() const {
    if (chars.size() != kNameChars) return false;
    for (const NameBytes& n : chars)
        if (n.empty()) return false;
    return true;
}

namespace {

std::vector<char> ReadAll(const std::wstring& path) {
    std::ifstream in(std::filesystem::path(path), std::ios::binary);
    if (!in) return {};
    return std::vector<char>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

// Calls `take(kind, tag, payload, size)` for each chunk of a .DAT: a flat run
// of chunks, each kind, tag, size and a fourth word, then the payload.
template <typename Take>
void EachChunk(const std::vector<char>& b, Take take) {
    for (std::size_t p = 0; p + 16 <= b.size();) {
        std::uint32_t kind, tag, size;
        std::memcpy(&kind, b.data() + p, 4);
        std::memcpy(&tag, b.data() + p + 4, 4);
        std::memcpy(&size, b.data() + p + 8, 4);
        if (size > b.size() - p - 16) break;
        take(kind, tag, reinterpret_cast<const std::uint8_t*>(b.data()) + p + 16, static_cast<std::size_t>(size));
        p += 16 + size;
    }
}

}  // namespace

bool NameSetFromDisc(const std::wstring& first_dat, const std::string& id, NameSet& out) {
    // Version 1: the version byte, 20 bytes of file name, eight names of five bytes, sixty of five.
    constexpr std::size_t kFile = 20, kSize = 1 + kFile + (kNameChars + kNameFaeries) * kFaerieName;
    bool found = false;
    EachChunk(ReadAll(first_dat), [&](std::uint32_t kind, std::uint32_t tag, const std::uint8_t* q, std::size_t size) {
        if (kind != 20 || tag != 0 || size != kSize || q[0] != 1 || found) return;
        out = Empty(id, 0xFF);
        for (std::size_t i = 0; i < kFile && q[1 + i]; ++i) out.prefix += static_cast<char>(q[1 + i]);
        const std::uint8_t* names = q + 1 + kFile;
        for (int i = 0; i < kNameChars; ++i) out.chars[static_cast<std::size_t>(i)] = Strip(names + i * kFaerieName, kFaerieName);
        names += kNameChars * kFaerieName;
        for (int i = 0; i < kNameFaeries; ++i)
            out.faeries[static_cast<std::size_t>(i)] = Strip(names + i * kFaerieName, kFaerieName);
        found = !out.prefix.empty();
    });
    return found;
}

NameSet NameSetFromTables(const std::string& id, const std::uint8_t* chars, const std::uint8_t* faeries) {
    NameSet s = Empty(id, 0);
    for (int i = 0; chars && i < kNameChars; ++i) s.chars[static_cast<std::size_t>(i)] = Strip(chars + i * kCharName, kCharName - 1);
    for (int i = 0; faeries && i < kNameFaeries; ++i)
        s.faeries[static_cast<std::size_t>(i)] = Strip(faeries + i * kFaerieName, kFaerieName);
    return s;
}

bool NameSetFromLayer(const std::wstring& first_dat, const std::string& id, const NameSet& shipped, NameSet& out) {
    const std::vector<char> b = ReadAll(first_dat);
    if (b.empty()) return false;
    out = Empty(id, 0);
    if (shipped.faeries.size() == kNameFaeries) out.faeries = shipped.faeries;
    bool found = false;
    EachChunk(b, [&](std::uint32_t kind, std::uint32_t tag, const std::uint8_t* q, std::size_t size) {
        const bool chars = kind == 10, faeries = kind == 15 && tag == 16;
        if (!(chars || faeries) || size < 1) return;
        // A count, then that many strings, each with its zero.
        const std::uint8_t* end = q + size;
        const int count = *q++;
        std::vector<NameBytes>& list = chars ? out.chars : out.faeries;
        for (int i = 0; i < count && q < end; ++i) {
            const auto* zero = static_cast<const std::uint8_t*>(std::memchr(q, 0, static_cast<std::size_t>(end - q)));
            if (!zero) break;
            const std::size_t most = chars ? kCharName - 1 : kFaerieName;
            if (static_cast<std::size_t>(i) < list.size() && zero > q && static_cast<std::size_t>(zero - q) <= most)
                list[static_cast<std::size_t>(i)] = NameBytes(q, zero);
            q = zero + 1;
        }
        found = found || chars;
    });
    return found;
}

NameReport TranslateNames(Block& block, const std::vector<NameSet>& known, const NameSet& to) {
    NameReport report;
    for (std::uint32_t i = 0; i < kNameChars; ++i) {
        std::uint8_t* field = block.data() + kChars + i * kCharStride;
        const NameBytes name = Strip(field, kCharName);
        if (name.empty()) continue;
        const NameBytes& want = i < to.chars.size() ? to.chars[i] : NameBytes();
        if (want.empty() || !Match(known, false, i, name)) {
            report.kept.push_back("Character " + std::to_string(i + 1));
            continue;
        }
        NameBytes now(kCharName, 0);
        std::memcpy(now.data(), want.data(), want.size() < kCharName ? want.size() : kCharName - 1);
        ++report.chars;
        if (std::memcmp(field, now.data(), kCharName) != 0) {
            std::memcpy(field, now.data(), kCharName);
            ++report.changed;
        }
    }
    for (std::uint32_t r = 0; r < kNameFaeries; ++r) {
        if (!block[kFaeries + r * kFaerieStride]) continue;   // nobody in this record
        std::uint8_t* field = block.data() + kFaerieNames + r * kFaerieName;
        const NameBytes name = Strip(field, kFaerieName);
        if (name.empty()) continue;
        const NameBytes& want = r < to.faeries.size() ? to.faeries[r] : NameBytes();
        if (want.empty() || !Match(known, true, r, name)) {
            report.kept.push_back("Faerie " + std::to_string(r + 1));
            continue;
        }
        NameBytes now(kFaerieName, to.faerie_pad);
        std::memcpy(now.data(), want.data(), want.size() < kFaerieName ? want.size() : kFaerieName);
        ++report.faeries;
        if (std::memcmp(field, now.data(), kFaerieName) != 0) {
            std::memcpy(field, now.data(), kFaerieName);
            ++report.changed;
        }
    }
    return report;
}

}  // namespace bof3x::save

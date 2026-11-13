#include "launcher/save/finds.h"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <initializer_list>

namespace bof3x::save {

namespace {

constexpr std::uint32_t kEnemyTag = 0xC2000, kEnemySize = 0x4A8, kEnemyHead = 0x48, kEnemyStride = 0x8C;
constexpr unsigned kEnemyCount = 8, kEnemyName = 12, kEnemyKind = 0xC;
constexpr unsigned kOp9Size = 13;

// The structural walk (tools/event_scan.py Walk.block): ops until a byte of
// `ends`. Each op 9x found is added to the finds.
class Walk {
public:
    Walk(const ImageAt& at, const unsigned char* lengths, unsigned area, std::vector<Find>& out)
        : at_(at), lengths_(lengths), area_(area), out_(out) {}

    // The address of the byte of `ends` the ops stopped at, and that byte; -1 if the script does not decode.
    int Block(std::uint32_t& a, std::initializer_list<unsigned char> ends, bool in_case, int depth) {
        if (depth > 64) return -1;
        for (;;) {
            if (++steps_ > 100000) return -1;
            const unsigned char* p = at_(a, 1);
            if (!p) return -1;
            const unsigned char b = *p;
            for (const unsigned char e : ends)
                if (b == e) return b;
            if (b < 0xF0) {
                const unsigned n = lengths_[b >> 4] ? lengths_[b >> 4] : 1;
                if (b >> 4 == 9) {
                    Find f;
                    if (const unsigned char* op = at_(a, kOp9Size); op && ContentFind(op, area_, f)) out_.push_back(f);
                }
                a += n;
            } else if (in_case && b == 0xF6) {
                a += 2;
            } else if (in_case && b == 0xF7) {
                a += 1;
            } else if (b == 0xF0 || b == 0xF1) {
                a += 3;
                int e = Block(a, {0xFD, 0xFE}, false, depth + 1);
                if (e == 0xFD) {
                    ++a;
                    e = Block(a, {0xFE, 0xFD}, false, depth + 1);
                    if (e == 0xFD) {   // a second FD in one if: the interpreter only sets its flag again
                        ++a;
                        e = Block(a, {0xFE}, false, depth + 1);
                    }
                }
                if (e != 0xFE) return -1;
                ++a;
            } else if (b == 0xF4) {
                a += 2;
                for (;;) {
                    const unsigned char* k = at_(a, 2);
                    if (!k) return -1;
                    if (k[0] == 0xF5) {
                        ++a;
                        break;
                    }
                    if (k[0] == 0xF6) a += 2;
                    else if (k[0] == 0xF7) a += 1;
                    else return -1;   // a byte between cases: the interpreter hangs
                    if (Block(a, {0xF8}, true, depth + 1) != 0xF8) return -1;
                    ++a;
                }
            } else if (b == 0xF9) {
                a += 2;
            } else if (b == 0xFA) {
                a += 1;
            } else {
                return -1;
            }
        }
    }

private:
    const ImageAt& at_;
    const unsigned char* lengths_;
    unsigned area_;
    std::vector<Find>& out_;
    unsigned steps_ = 0;
};

// A name as the editor shows names (names.cpp NameText): an ASCII byte as its
// letter, 0x2A a quotation mark, a two-byte glyph code a question mark; empty
// when no letter or digit is in it.
std::string ShownName(const unsigned char* p, std::size_t n) {
    std::string out;
    bool letter = false;
    for (std::size_t i = 0; i < n && p[i]; ++i) {
        if (p[i] & 0x80) {
            out += '?';
            ++i;
            continue;
        }
        out += p[i] == 0x2A ? '"' : static_cast<char>(p[i]);
        letter = letter || std::isalnum(p[i]);
    }
    while (!out.empty() && out.back() == ' ') out.pop_back();
    while (!out.empty() && out.front() == ' ') out.erase(out.begin());
    return letter ? out : std::string();
}

bool Listed(const std::string& joined, const std::string& one) {
    for (std::size_t at = 0;;) {
        const std::size_t semi = joined.find("; ", at);
        if (joined.compare(at, semi == std::string::npos ? std::string::npos : semi - at, one) == 0) return true;
        if (semi == std::string::npos) return false;
        at = semi + 2;
    }
}

std::string Holds(const Find& f, const Names& names) {
    if (f.category == 0xFF) return std::to_string(f.value * 40) + " zenny";
    static const char* const kWord[5] = {"item", "weapon", "armor", "accessory", "key item"};
    const std::vector<std::string>* list = f.category == 4 ? &names.key : &names.item[f.category < 4 ? f.category : 0];
    if (f.value < list->size() && !(*list)[f.value].empty()) return (*list)[f.value];
    return std::string(kWord[f.category < 5 ? f.category : 0]) + " " + std::to_string(f.value);
}

}  // namespace

bool ScriptContentFinds(const ImageAt& at, std::uint32_t start, const unsigned char lengths[16], unsigned area,
                        std::vector<Find>& out) {
    Walk walk(at, lengths, area, out);
    std::uint32_t a = start;
    return walk.Block(a, {0xFF}, false, 0) == 0xFF;
}

bool ContentFind(const unsigned char* op, unsigned area, Find& out) {
    if (op[8] == 0xFF) return false;
    out = {area, op[8], op[5], op[7], op[0xA], op[9]};
    return true;
}

bool PassageFind(const unsigned char* e, unsigned area, Find& out) {
    if (e[4] == 0xFF || ((e[5] & 0x80) && e[6] != 0xFF)) return false;
    out = {area, e[4], e[0], e[1], e[6], e[5]};
    return true;
}

std::map<unsigned, std::string> FindTexts(const std::vector<Find>& finds, const Names& names) {
    struct Group {
        std::vector<unsigned> areas;
        unsigned x, z;
        std::string holds;
    };
    std::map<unsigned, std::vector<Group>> by_bit;
    for (const Find& f : finds) {
        std::vector<Group>& groups = by_bit[f.bit];
        const std::string holds = Holds(f, names);
        Group* g = nullptr;
        for (Group& have : groups)
            if (have.x == f.x && have.z == f.z && have.holds == holds) g = &have;
        if (!g) {
            groups.push_back({{}, f.x, f.z, holds});
            g = &groups.back();
        }
        bool listed = false;
        for (const unsigned a : g->areas) listed = listed || a == f.area;
        if (!listed) g->areas.push_back(f.area);
    }
    std::map<unsigned, std::string> out;
    for (const auto& [bit, groups] : by_bit) {
        std::string text;
        for (const Group& g : groups) {
            if (!text.empty()) text += "; ";
            for (std::size_t i = 0; i < g.areas.size(); ++i) {
                char area[16];
                std::snprintf(area, sizeof area, "%sarea%03u", i ? ", " : "", g.areas[i]);
                text += area;
            }
            text += " (" + std::to_string(g.x) + ", " + std::to_string(g.z) + "): " + g.holds;
        }
        out[bit] = text;
    }
    return out;
}

bool ReadEnemyChunks(const std::filesystem::path& dat, std::vector<EnemyChunk>& out) {
    std::ifstream in(dat, std::ios::binary);
    if (!in) return false;
    in.seekg(0, std::ios::end);
    const std::uint64_t size = static_cast<std::uint64_t>(in.tellg());
    // A .DAT is a flat run of chunks: a 16-byte header (kind, tag, size, a
    // fourth word), then the payload (docs/DAT_CONTAINER.md section 1). Only
    // the headers are read, and the payloads of the chunks wanted.
    for (std::uint64_t p = 0; p + 16 <= size;) {
        unsigned char h[16];
        in.seekg(static_cast<std::streamoff>(p));
        if (!in.read(reinterpret_cast<char*>(h), 16)) break;
        std::uint32_t kind, tag, n;
        std::memcpy(&kind, h, 4);
        std::memcpy(&tag, h + 4, 4);
        std::memcpy(&n, h + 8, 4);
        if (n > size - p - 16) break;
        if (kind == 0 && tag >= kEnemyTag && tag < kEnemyTag + kEnemySize && n <= kEnemySize) {
            EnemyChunk c{tag, std::vector<unsigned char>(n)};
            if (n && !in.read(reinterpret_cast<char*>(c.payload.data()), n)) break;
            out.push_back(std::move(c));
        }
        p += 16 + n;
    }
    return true;
}

void AddEnemyNames(const std::vector<EnemyChunk>& chunks, std::map<unsigned, std::string>& kinds) {
    std::vector<unsigned char> table;
    for (const EnemyChunk& c : chunks)
        if (c.tag == kEnemyTag && c.payload.size() == kEnemySize) table = c.payload;
    if (table.empty()) return;
    for (const EnemyChunk& c : chunks) {   // the names over the table, in file order
        if (c.tag == kEnemyTag) continue;
        const std::size_t at = c.tag - kEnemyTag;
        for (std::size_t i = 0; i < c.payload.size() && at + i < table.size(); ++i) table[at + i] = c.payload[i];
    }
    for (unsigned k = 0; k < kEnemyCount; ++k) {
        const unsigned char* rec = table.data() + kEnemyHead + kEnemyStride * k;
        bool used = false;
        for (unsigned i = 0; i < kEnemyStride; ++i) used = used || rec[i];
        if (!used) continue;
        const std::string name = ShownName(rec, kEnemyName);
        if (name.empty()) continue;
        std::string& have = kinds[rec[kEnemyKind]];
        if (have.empty()) have = name;
        else if (!Listed(have, name)) have += "; " + name;
    }
}

}  // namespace bof3x::save

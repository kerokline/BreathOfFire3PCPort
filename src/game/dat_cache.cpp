// DIV-0089: the importer's cache read in place of the install's DAT\ and SND\
// (dat_cache.h, docs/cache-read.md). Ours entirely: no byte of Capcom's code,
// and no game data - the self-test builds its own manifests and containers.
#include "game/dat_cache.h"

#include <windows.h>


#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "bof3/symbols.gen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace dat_cache {
namespace {

// The engine's base build is the PC port (pc-zh); its language layer is the
// cache's loc/zh-CN (fixtures.toml's tag for pc-zh, docs/importer.md 5). The
// shipped DAT\NAME is base/ and that layer interleaved (importer.py verify).
constexpr char kTarget[] = "pc-zh";
constexpr char kTargetLayer[] = "loc/zh-CN";
constexpr char kTargetDir[] = "loc\\zh-CN";

// fopen's limit (File_Open retries with File_CdRoot only a path that fits its
// buffer, DIV-0087). The longest path built from shipped names: <root> +
// \opt\ + a 23-character layer (dat_load.cpp kOptName - 1) + \dat\ + a
// 12-character name (Dat_FileNames' longest, read 2026-10-08; the recipe's
// 742 agree, 2026-10-10). Longer names are checked as each path is built.
constexpr std::size_t kPathMax = MAX_PATH - 1;
constexpr std::size_t kLongestTail = sizeof "\\opt\\" - 1 + 23 + sizeof "\\dat\\" - 1 + 12;
constexpr std::size_t kRootMax = kPathMax - kLongestTail;
// More chunks than any shipped container holds (16, recipes/pc-zh.toml).
constexpr int kChunksMax = 256;

struct Container {
    std::string order;   // 'b' (base) / 'z' (loc/zh-CN) per slot: the PC's file order
    bool zh = false;     // the zh file is needed: a zh slot, or enemy names (a names row, a base row by widen)
    bool missing = false;  // a row with no source: the build left a layer of it unwritten
    bool own = false;      // a row by `own`: a disc's own section standing in where no source carries the PC's
    bool held = false;   // every file it needs is in the cache, and checked
};
using Index = std::map<std::string, Container>;

bool FileExists(const char* path) {
    const DWORD a = GetFileAttributesA(path);
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

bool LayerDirHasDat(const char* dir) {
    char pattern[MAX_PATH + 8];
    if (std::snprintf(pattern, sizeof pattern, "%s\\*.DAT", dir) >= static_cast<int>(sizeof pattern)) return false;
    WIN32_FIND_DATAA fd;
    const HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return false;
    FindClose(h);
    return true;
}

// The start-up check's reader: the headers only, by seeking (the banks make
// base\dat some 300 MB; reading it whole would cost seconds).
int CountByWin32(const char* path, std::int8_t* kinds, int cap) {
    const HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return -1;
    LARGE_INTEGER size;
    if (!GetFileSizeEx(h, &size)) {
        CloseHandle(h);
        return -1;
    }
    long long off = 0;
    int n = 0;
    while (off < size.QuadPart) {
        Chunk c;
        LARGE_INTEGER at;
        at.QuadPart = off;
        DWORD got = 0;
        if (n == cap || off + static_cast<long long>(sizeof c) > size.QuadPart ||
            !SetFilePointerEx(h, at, nullptr, FILE_BEGIN) || !ReadFile(h, &c, sizeof c, &got, nullptr) ||
            got != sizeof c || c.size < 0) {
            CloseHandle(h);
            return -1;
        }
        kinds[n++] = c.kind;
        off += static_cast<long long>(sizeof c) + c.size;
    }
    CloseHandle(h);
    return off == size.QuadPart ? n : -1;
}

void __cdecl CrtFree(void* p) { Crt_free(p); }
void* __cdecl CrtMalloc(unsigned size) { return Crt_malloc(size); }

struct State {
    bool configured = false;
    bool armed = false;
    char root[kRootMax + 1] = {};
    Index index;
    unsigned held = 0, part = 0, own = 0;
    Io io{};
};
State g_c;

void Join(char* out, std::size_t cap, const char* a, const char* b, const char* c, const char* d) {
    const int n = std::snprintf(out, cap, "%s\\%s%s%s", a, b, c, d);
    if (n < 0 || static_cast<std::size_t>(n) >= cap)
        bof3::Fatal("DIV-0089: %s\\%s%s%s does not fit the %u-byte path buffer", a, b, c, d,
                    static_cast<unsigned>(cap));
}

void ShippedPath(char* out, std::size_t cap, char layer, const char* name) {
    Join(out, cap, g_c.root, layer == 'b' ? "base" : kTargetDir, "\\dat\\", name);
}

std::string Key(const char* name) {
    std::string k(name);
    for (char& c : k)
        if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    return k;
}

void SetError(char* error, std::size_t cap, const char* fmt, ...) __attribute__((format(printf, 3, 4)));
void SetError(char* error, std::size_t cap, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(error, cap, fmt, args);
    va_end(args);
}

// One quoted TOML string of the manifest (json.dumps of a plain name): no
// escape, no control character.
bool String(const char*& p, std::string* out) {
    if (*p != '"') return false;
    ++p;
    out->clear();
    while (*p != '"') {
        if (*p == 0 || *p == '\\' || static_cast<unsigned char>(*p) < 0x20) return false;
        out->push_back(*p++);
    }
    ++p;
    return true;
}

bool Comma(const char*& p) {
    if (*p != ',') return false;
    ++p;
    while (*p == ' ') ++p;
    return true;
}

// The manifest's `[cache] assets` rows, as importer.py write_manifest prints
// them: `  ["NAME", SLOT, "LAYER", "WHERE", "HASH"],` - WHERE empty for a chunk
// no source carried, else build:how[:EMI#section]; a names row (how `names`)
// follows its enemy table's row with the same slot.
bool Parse(const char* text, Index* out, char* error, std::size_t cap) {
    out->clear();
    enum { kMeta, kAssets, kRows, kDone } at = kMeta;
    bool target = false;
    std::string last;
    unsigned line_no = 0;
    for (const char* line = text; *line;) {
        const char* end = line;
        while (*end && *end != '\n') ++end;
        std::string row(line, static_cast<std::size_t>(end - line));
        if (!row.empty() && row.back() == '\r') row.pop_back();
        line = *end ? end + 1 : end;
        ++line_no;
        if (at == kMeta) {
            if (row == std::string("target = \"") + kTarget + "\"") target = true;
            else if (row.rfind("target = ", 0) == 0) {
                SetError(error, cap, "manifest.toml: %s - not a cache of %s", row.c_str(), kTarget);
                return false;
            }
            if (row == "[cache]") at = kAssets;
            continue;
        }
        if (at == kAssets) {
            if (row != "assets = [") {
                SetError(error, cap, "manifest.toml line %u: [cache] does not open with assets = [", line_no);
                return false;
            }
            at = kRows;
            continue;
        }
        if (at == kRows) {
            if (row == "]") {
                at = kDone;
                break;
            }
            const char* p = row.c_str();
            std::string name, layer, where, hash;
            while (*p == ' ') ++p;
            bool ok = *p++ == '[' && String(p, &name) && Comma(p);
            char* after = nullptr;
            const unsigned long slot = ok ? std::strtoul(p, &after, 10) : 0;
            ok = ok && after && after != p;
            if (ok) p = after;
            ok = ok && Comma(p) && String(p, &layer) && Comma(p) && String(p, &where) && Comma(p) && String(p, &hash) &&
                 *p++ == ']';
            if (ok && *p == ',') ++p;
            ok = ok && *p == 0 && !name.empty() && hash.size() == 64;
            if (!ok) {
                SetError(error, cap, "manifest.toml line %u is not an asset row: %s", line_no, row.c_str());
                return false;
            }
            const std::string key = Key(name.c_str());
            if (key != last && out->count(key)) {
                SetError(error, cap, "manifest.toml line %u: the rows of %s are not together", line_no, name.c_str());
                return false;
            }
            last = key;
            Container& c = (*out)[key];
            const std::size_t colon = where.find(':');
            const std::string how = colon == std::string::npos ? std::string()
                                                               : where.substr(colon + 1, where.find(':', colon + 1) - colon - 1);
            if (!where.empty() && (colon == std::string::npos || colon == 0 || how.empty())) {
                SetError(error, cap, "manifest.toml line %u: source \"%s\" is not build:how", line_no, where.c_str());
                return false;
            }
            if (how == "names") {
                if (layer != kTargetLayer || c.order.empty() || slot != c.order.size() - 1 || c.order.back() != 'b') {
                    SetError(error, cap, "manifest.toml line %u: a names row of %s not after its table's", line_no,
                             name.c_str());
                    return false;
                }
                c.zh = true;
                continue;
            }
            if (slot != c.order.size()) {
                SetError(error, cap, "manifest.toml line %u: %s's slots are not in order", line_no, name.c_str());
                return false;
            }
            if (layer == "base") {
                c.order.push_back('b');
                if (how == "widen") c.zh = true;  // the stats here, the names the language layer's
            } else if (layer == kTargetLayer) {
                c.order.push_back('z');
                c.zh = true;
            } else {
                SetError(error, cap, "manifest.toml line %u: layer \"%s\" (the shipped files are base and %s)", line_no,
                         layer.c_str(), kTargetLayer);
                return false;
            }
            if (where.empty()) c.missing = true;
            if (how == "own") c.own = true;
        }
    }
    if (!target) {
        SetError(error, cap, "manifest.toml: no target = \"%s\" in [meta]", kTarget);
        return false;
    }
    if (at != kDone) {
        SetError(error, cap, "manifest.toml: no whole [cache] assets table");
        return false;
    }
    return true;
}

// Which containers the cache holds whole, each checked: its base file has a
// chunk per base slot, its zh file one per zh slot then only kind-0 chunks
// (the enemy names), as importer.py build writes them and verify reads them.
bool Check(Index* index, char* error, std::size_t cap) {
    char path[kPathMax + 1];
    std::int8_t kinds[kChunksMax];
    g_c.held = g_c.part = g_c.own = 0;
    for (auto& [name, c] : *index) {
        const bool need_b = c.order.find('b') != std::string::npos;
        bool have_b = false, have_z = false;
        if (need_b) {
            ShippedPath(path, sizeof path, 'b', name.c_str());
            have_b = g_c.io.exists(path);
        }
        if (c.zh) {
            ShippedPath(path, sizeof path, 'z', name.c_str());
            have_z = g_c.io.exists(path);
        }
        c.held = !c.missing && (!need_b || have_b) && (!c.zh || have_z);
        if (!c.held) {
            if (have_b || have_z) ++g_c.part;
            continue;
        }
        for (const char layer : {'b', 'z'}) {
            if (layer == 'b' ? !need_b : !c.zh) continue;
            ShippedPath(path, sizeof path, layer, name.c_str());
            const int n = g_c.io.count(path, kinds, kChunksMax);
            int want = 0;
            for (const char o : c.order) want += o == layer;
            bool ok = n >= 0 && (layer == 'b' ? n == want : n >= want);
            for (int i = want; ok && i < n; ++i) ok = kinds[i] == 0;
            if (n < 0) {
                std::snprintf(error, cap, "%s: does not open, or a chunk runs past its end (more than %d chunks?)",
                              path, kChunksMax);
                return false;
            }
            if (!ok) {
                std::snprintf(error, cap, "%s: %d chunks where the manifest has %d%s", path, n, want,
                              layer == 'z' ? " (then only enemy names, kind 0)" : "");
                return false;
            }
        }
        ++g_c.held;
        g_c.own += c.own;
    }
    return true;
}

// The whole of `path` through the file layer into a fresh buffer.
std::uint8_t* ReadWhole(const char* path, int* size) {
    const int handle = g_c.io.open(path, 0, 0);
    if (handle == -1) bof3::Fatal("DIV-0089: %s is in the cache but does not open", path);
    *size = g_c.io.size(handle);
    if (*size < 0) bof3::Fatal("DIV-0089: %s has %d bytes", path, *size);
    auto* data = static_cast<std::uint8_t*>(g_c.io.malloc(static_cast<unsigned>(*size)));
    const unsigned got = g_c.io.read(handle, data, static_cast<unsigned>(*size));
    g_c.io.close(handle);
    if (got != static_cast<unsigned>(*size)) bof3::Fatal("DIV-0089: %s: read %u of %d bytes", path, got, *size);
    return data;
}

// The chunk at `off` of (data, size) to `fn`, `off` moved past it.
void Next(const char* path, std::uint8_t* data, int size, int* off, ChunkFn fn, void* ctx) {
    Chunk c;
    if (*off + static_cast<int>(sizeof c) > size)
        bof3::Fatal("DIV-0089: %s ends before the chunk the manifest has at 0x%X", path, static_cast<unsigned>(*off));
    std::memcpy(&c, data + *off, sizeof c);
    if (c.size < 0 || c.size > size - *off - static_cast<int>(sizeof c))
        bof3::Fatal("DIV-0089: %s: the chunk at 0x%X runs past the file's end", path, static_cast<unsigned>(*off));
    fn(ctx, c, data + *off + sizeof c);
    *off += static_cast<int>(sizeof c) + c.size;
}

Io DefaultIo() {
    Io io;
    io.exists = FileExists;
    io.layer_dir = LayerDirHasDat;
    io.open = File_Open;
    io.size = File_Size;
    io.read = File_Read;
    io.close = File_Close;
    io.malloc = CrtMalloc;
    io.free = CrtFree;
    io.count = CountByWin32;
    return io;
}

}  // namespace

Io GameIo() { return DefaultIo(); }

void Configure() {
    g_c = State{};
    g_c.io = DefaultIo();
    char mode[4];
    const DWORD m = GetEnvironmentVariableA("BOF3X_CACHE_DATA", mode, sizeof mode);
    if (m >= sizeof mode || (m != 0 && std::strcmp(mode, "0") != 0 && std::strcmp(mode, "1") != 0))
        bof3::Fatal("DIV-0089: BOF3X_CACHE_DATA must be 0 or 1");
    char root[MAX_PATH];
    const DWORD n = GetEnvironmentVariableA("BOF3X_CACHE", root, sizeof root);
    if (n >= sizeof root) bof3::Fatal("DIV-0089: BOF3X_CACHE is %lu characters", static_cast<unsigned long>(n));
    if (n == 0) return;  // no cache: the install's files, as the original's
    if (m != 0 && mode[0] == '0') {
        bof3::Log("DIV-0089    BOF3X_CACHE_DATA=0: DAT\\ and SND\\ from the install (the cache for the music only)");
        return;
    }
    std::size_t len = n;
    while (len > 1 && (root[len - 1] == '\\' || root[len - 1] == '/')) root[--len] = 0;
    const DWORD a = GetFileAttributesA(root);
    if (a == INVALID_FILE_ATTRIBUTES || !(a & FILE_ATTRIBUTE_DIRECTORY))
        bof3::Fatal("DIV-0089: BOF3X_CACHE=%s is not a directory", root);
    if (len > kRootMax)
        bof3::Fatal("DIV-0089: BOF3X_CACHE=%s is %u characters; its longest path (an optional layer's file) must stay "
                    "within MAX_PATH, so at most %u (BOF3X_CACHE_DATA=0 reads only its music)",
                    root, static_cast<unsigned>(len), static_cast<unsigned>(kRootMax));
    std::memcpy(g_c.root, root, len + 1);
    g_c.configured = true;

    char path[kPathMax + 1];
    Join(path, sizeof path, g_c.root, "manifest.toml", "", "");
    const HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        bof3::Log("DIV-0089    %s has no manifest.toml: every DAT\\ container from the install; its loc\\, opt\\ "
                  "layers and base\\snd read", g_c.root);
        return;
    }
    LARGE_INTEGER size;
    constexpr LONGLONG kManifestMax = 64ll << 20;  // far above a whole build's (some 600 KB)
    if (!GetFileSizeEx(h, &size) || size.QuadPart <= 0 || size.QuadPart > kManifestMax) {
        CloseHandle(h);
        bof3::Fatal("DIV-0089: %s has %lld bytes", path, static_cast<long long>(size.QuadPart));
    }
    std::vector<char> text(static_cast<std::size_t>(size.QuadPart) + 1, 0);
    DWORD got = 0;
    const BOOL ok = ReadFile(h, text.data(), static_cast<DWORD>(size.QuadPart), &got, nullptr);
    CloseHandle(h);
    if (!ok || got != size.QuadPart) bof3::Fatal("DIV-0089: %s: read %lu of %lld bytes", path, got, size.QuadPart);
    if (std::strlen(text.data()) != got) bof3::Fatal("DIV-0089: %s holds a NUL byte", path);
    char error[512];
    if (!Parse(text.data(), &g_c.index, error, sizeof error)) bof3::Fatal("DIV-0089: %s\\%s", g_c.root, error);
    if (!Check(&g_c.index, error, sizeof error)) bof3::Fatal("DIV-0089: the cache's %s", error);
    bof3::Log("DIV-0089    the cache %s checked: %u of the manifest's %u containers held whole, %u in part (those "
              "from the install's DAT\\)",
              g_c.root, g_c.held, static_cast<unsigned>(g_c.index.size()), g_c.part);
    // A cache built without the PC's DAT\ may hold containers in which a
    // disc's own section stands in for a chunk only the PC carries
    // (docs/importer-transforms.md 5): those play the disc's, DIV-0089.
    if (g_c.own)
        bof3::Log("DIV-0089    %u of them with a disc's own section standing in for the PC's (the manifest's `own` "
                  "rows; docs/cache-read.md section 3)",
                  g_c.own);
}

void Arm() {
    if (!g_c.configured) return;
    g_c.armed = true;
    bof3::Log("DIV-0089    reading the cache %s before the install: %u DAT\\ containers, the layers it has, base\\snd",
              g_c.root, g_c.held);
}

const char* Root() { return g_c.configured ? g_c.root : nullptr; }

bool Armed() { return g_c.armed; }

bool HasLayer(const char* kind, const char* layer) {
    if (!g_c.configured || !layer || !layer[0]) return false;
    char dir[kPathMax + 1];
    Join(dir, sizeof dir, g_c.root, kind, "\\", layer);
    const std::size_t n = std::strlen(dir);
    if (n + 4 >= sizeof dir) bof3::Fatal("DIV-0089: %s\\dat does not fit the path buffer", dir);
    std::memcpy(dir + n, "\\dat", 5);
    return g_c.io.layer_dir(dir);
}

From PickOverlay(bool armed, bool cache_has_layer, bool cache_has_file) {
    if (!armed || !cache_has_layer) return From::kInstall;
    return cache_has_file ? From::kCache : From::kNone;
}

void LayerPath(char* out, std::size_t cap, const char* kind, const char* layer, const char* name) {
    char dir[kPathMax + 1];
    Join(dir, sizeof dir, g_c.root, kind, "\\", layer);
    Join(out, cap, dir, "dat\\", name, "");
}

bool WalkShipped(const char* name, ChunkFn fn, void* ctx) {
    if (!g_c.armed) return false;
    const auto it = g_c.index.find(Key(name));
    if (it == g_c.index.end() || !it->second.held) return false;
    const Container& c = it->second;
    // static: LoadDatFile runs on a 16 KB coroutine stack, and no chunk's
    // handler loads a file
    static char base_path[kPathMax + 1], zh_path[kPathMax + 1];
    std::uint8_t* base = nullptr;
    std::uint8_t* zh = nullptr;
    int base_size = 0, zh_size = 0, base_off = 0, zh_off = 0;
    if (c.order.find('b') != std::string::npos) {
        ShippedPath(base_path, sizeof base_path, 'b', name);
        base = ReadWhole(base_path, &base_size);
    }
    if (c.zh) {
        ShippedPath(zh_path, sizeof zh_path, 'z', name);
        zh = ReadWhole(zh_path, &zh_size);
    }
    for (const char o : c.order) {
        if (o == 'b') Next(base_path, base, base_size, &base_off, fn, ctx);
        else Next(zh_path, zh, zh_size, &zh_off, fn, ctx);
    }
    // Past the zh file's slots: the enemy names, each a kind-0 chunk inside the
    // table its base slot loaded - laid on it here, as the PC's own chunk
    // carries them (importer.py verify composes them the same way).
    while (zh && zh_off < zh_size) Next(zh_path, zh, zh_size, &zh_off, fn, ctx);
    if (base && base_off != base_size)
        bof3::Fatal("DIV-0089: %s holds more chunks than its manifest rows", base_path);
    if (base) g_c.io.free(base);
    if (zh) g_c.io.free(zh);
    return true;
}

bool SoundPath(const char* name, char* out, std::size_t cap) {
    if (!g_c.armed) return false;
    Join(out, cap, g_c.root, "base\\snd\\", name, ".DAT");
    return g_c.io.exists(out);
}

// --- the self-tests' seam -----------------------------------------------------
namespace {
State* g_saved = nullptr;
}

bool TestBegin(const char* root, const char* manifest, const Io& io, char* error, std::size_t cap) {
    if (g_saved) bof3::Fatal("DIV-0089 self-test: TestBegin twice");
    g_saved = new State(g_c);
    g_c = State{};
    g_c.io = io;
    if (std::strlen(root) > kRootMax) bof3::Fatal("DIV-0089 self-test: a root of %u characters", (unsigned)std::strlen(root));
    std::strcpy(g_c.root, root);
    g_c.configured = true;
    g_c.armed = true;
    error[0] = 0;
    return Parse(manifest, &g_c.index, error, cap) && Check(&g_c.index, error, cap);
}

void TestEnd() {
    if (!g_saved) bof3::Fatal("DIV-0089 self-test: TestEnd without TestBegin");
    g_c = *g_saved;
    delete g_saved;
    g_saved = nullptr;
}

void TestSetArmed(bool armed) {
    if (!g_saved) bof3::Fatal("DIV-0089 self-test: TestSetArmed without TestBegin");
    g_c.armed = armed;
}

// --- the in-memory files -------------------------------------------------------
namespace {

const std::vector<TestFile>* g_tfiles = nullptr;
std::vector<std::string>* g_topened = nullptr;
struct Open {
    const TestFile* file = nullptr;
};
Open g_topen[4];

const TestFile* Find(const char* path) {
    for (const TestFile& f : *g_tfiles)
        if (lstrcmpiA(f.path.c_str(), path) == 0) return &f;
    return nullptr;
}
bool TExists(const char* path) { return Find(path) != nullptr; }
bool TLayerDir(const char* dir) {
    const std::size_t n = std::strlen(dir);
    for (const TestFile& f : *g_tfiles)
        if (f.path.size() > n + 1 && CompareStringA(LOCALE_INVARIANT, NORM_IGNORECASE, f.path.c_str(), static_cast<int>(n),
                                                    dir, static_cast<int>(n)) == CSTR_EQUAL &&
            f.path[n] == '\\')
            return true;
    return false;
}
int __cdecl TOpen(const char* path, int, int) {
    g_topened->push_back(path);
    const TestFile* f = Find(path);
    if (!f) return -1;
    for (int i = 0; i < 4; ++i)
        if (!g_topen[i].file) {
            g_topen[i].file = f;
            return i;
        }
    bof3::Fatal("DIV-0089 self-test: more than four files open");
}
const TestFile* Handle(int h) {
    if (h < 0 || h >= 4 || !g_topen[h].file) bof3::Fatal("DIV-0089 self-test: handle %d is not open", h);
    return g_topen[h].file;
}
int __cdecl TSize(int h) { return static_cast<int>(Handle(h)->bytes.size()); }
unsigned __cdecl TRead(int h, void* dst, unsigned size) {
    const TestFile* f = Handle(h);
    const unsigned n = size < f->bytes.size() ? size : static_cast<unsigned>(f->bytes.size());
    std::memcpy(dst, f->bytes.data(), n);
    return n;
}
void __cdecl TClose(int h) {
    Handle(h);
    g_topen[h].file = nullptr;
}
void* __cdecl TMalloc(unsigned size) { return std::malloc(size ? size : 1); }
void __cdecl TFree(void* p) { std::free(p); }
int TCount(const char* path, std::int8_t* kinds, int cap) {
    const TestFile* f = Find(path);
    if (!f) return -1;
    std::size_t off = 0;
    int n = 0;
    while (off < f->bytes.size()) {
        Chunk c;
        if (n == cap || off + sizeof c > f->bytes.size()) return -1;
        std::memcpy(&c, f->bytes.data() + off, sizeof c);
        if (c.size < 0) return -1;
        kinds[n++] = c.kind;
        off += sizeof c + static_cast<std::size_t>(c.size);
    }
    return off == f->bytes.size() ? n : -1;
}

}  // namespace

std::vector<std::uint8_t> TestContainer(const std::vector<TestChunk>& chunks) {
    std::vector<std::uint8_t> out;
    for (const TestChunk& t : chunks) {
        Chunk c{};
        c.kind = t.kind;
        c.tag = t.tag;
        c.size = t.size;
        const std::size_t at = out.size();
        out.resize(at + sizeof c + static_cast<std::size_t>(t.size), t.fill);
        std::memcpy(out.data() + at, &c, sizeof c);
    }
    return out;
}

Io TestIo(const std::vector<TestFile>* files, std::vector<std::string>* opened) {
    g_tfiles = files;
    g_topened = opened;
    for (Open& o : g_topen) o.file = nullptr;
    Io io;
    io.exists = TExists;
    io.layer_dir = TLayerDir;
    io.open = TOpen;
    io.size = TSize;
    io.read = TRead;
    io.close = TClose;
    io.malloc = TMalloc;
    io.free = TFree;
    io.count = TCount;
    return io;
}

// --- BOF3X_SHADOW=dat_cache ------------------------------------------------------
namespace {

constexpr char kTestRoot[] = "T:\\bof3x-dat-selftest";
const std::string kHash(64, 'a');

std::string Row(const char* name, int slot, const char* layer, const char* where) {
    char row[256];
    std::snprintf(row, sizeof row, "  [\"%s\", %d, \"%s\", \"%s\", \"%s\"],", name, slot, layer, where, kHash.c_str());
    return row;
}

// A manifest as importer.py write_manifest prints one: [meta], the sources,
// [cache] assets, then the language layers' overlays (not read).
std::string Manifest(const std::vector<std::string>& rows, const char* target = kTarget) {
    std::string m = "# The cache's provenance, written by tools/importer.py build.\n\n[meta]\ntarget = \"";
    m += target;
    m += "\"\nrecipe_sha256 = \"" + kHash + "\"\nbuilt = \"2026-10-10T00:00:00\"\n\n[[source]]\nbuild = \"pc-zh\"\n"
         "path = \"DAT\"\n\n[cache]\nassets = [\n";
    for (const std::string& r : rows) m += r + "\n";
    m += "]\n\n# Language layers\noverlays = [\n  [\"MIX.DAT\", \"loc/en-US\", \"psx-us\", \"" + kHash + "\"],\n]\n";
    return m;
}

// The good cache: MIX interleaves base and zh around an enemy table (stats from
// a disc, names from the PC) - the PC's slot order b z b(table) b; BASE and
// ZH are one layer each; NONAME's table came from a disc with no PC to name it
// (its zh file never written); GAP has a chunk no source carried.
std::vector<std::string> GoodRows() {
    return {Row("MIX.DAT", 0, "base", "pc-zh:chunk"),
            Row("MIX.DAT", 1, "loc/zh-CN", "pc-zh:chunk"),
            Row("MIX.DAT", 2, "base", "psx-jp:widen:WORLD00/AREA000.EMI#10"),
            Row("MIX.DAT", 2, "loc/zh-CN", "pc-zh:names"),
            Row("MIX.DAT", 3, "base", "psx-us:own:WORLD00/AREA000.EMI#5"),
            Row("BASE.DAT", 0, "base", "pc-zh:chunk"),
            Row("ZH.DAT", 0, "loc/zh-CN", "pc-zh:chunk"),
            Row("NONAME.DAT", 0, "base", "psx-us:widen:WORLD00/AREA001.EMI#10"),
            Row("GAP.DAT", 0, "base", ""),
            Row("GAP.DAT", 1, "base", "pc-zh:chunk")};
}

std::string Path(const char* rel) { return std::string(kTestRoot) + "\\" + rel; }

std::vector<TestFile> GoodFiles() {
    return {{Path("base\\dat\\MIX.DAT"),
             TestContainer({{0, 0x1000, 8, 0xB0}, {0, 0xC2000, 32, 0xB1}, {1, 0x0E001000, 0x800, 0xB2}})},
            {Path("loc\\zh-CN\\dat\\MIX.DAT"), TestContainer({{0, 0x4000, 8, 0xC0}, {0, 0xC2004, 12, 0xC1}})},
            {Path("base\\dat\\BASE.DAT"), TestContainer({{2, 2, 16, 0xD0}})},
            {Path("loc\\zh-CN\\dat\\ZH.DAT"), TestContainer({{3, 0, 0x120, 0xE0}})},
            {Path("base\\dat\\NONAME.DAT"), TestContainer({{0, 0xC2000, 32, 0xF0}})},
            {Path("base\\dat\\GAP.DAT"), TestContainer({{0, 0, 4, 1}, {0, 4, 4, 2}})},
            {Path("base\\snd\\ABC.DAT"), {1, 2, 3, 4}},
            {Path("loc\\en-US\\dat\\MIX.DAT"), TestContainer({{0, 0x4000, 8, 0x10}})}};
}

struct Want {
    const char* name;
    const char* order;
    bool zh, missing, held;
};
constexpr Want kWant[] = {{"MIX.DAT", "bzbb", true, false, true},
                          {"BASE.DAT", "b", false, false, true},
                          {"ZH.DAT", "z", true, false, true},
                          {"NONAME.DAT", "b", true, false, false},
                          {"GAP.DAT", "bb", false, true, false}};

// The parsed index against kWant: empty when equal, else what differs.
std::string Compare(const Index& index) {
    if (index.size() != sizeof kWant / sizeof kWant[0]) return "the container count";
    for (const Want& w : kWant) {
        const auto it = index.find(w.name);
        if (it == index.end()) return std::string(w.name) + " absent";
        const Container& c = it->second;
        if (c.order != w.order) return std::string(w.name) + " order " + c.order;
        if (c.zh != w.zh) return std::string(w.name) + " zh";
        if (c.missing != w.missing) return std::string(w.name) + " missing";
        if (c.held != w.held) return std::string(w.name) + " held";
    }
    return std::string();
}

struct Seen {
    std::int8_t kind;
    std::uint32_t tag;
    std::uint8_t first;
};
void Record(void* ctx, const Chunk& c, std::uint8_t* payload) {
    static_cast<std::vector<Seen>*>(ctx)->push_back({c.kind, c.tag, c.size > 0 ? payload[0] : std::uint8_t{0}});
}
// MIX.DAT as the PC walks it: base 0, zh 0, base 1 (the table), base 2, then
// the table's names laid on it (dat_cache.h: past the zh slots).
const std::vector<Seen> kMixWalk = {
    {0, 0x1000, 0xB0}, {0, 0x4000, 0xC0}, {0, 0xC2000, 0xB1}, {1, 0x0E001000, 0xB2}, {0, 0xC2004, 0xC1}};
bool Same(const std::vector<Seen>& a, const std::vector<Seen>& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (a[i].kind != b[i].kind || a[i].tag != b[i].tag || a[i].first != b[i].first) return false;
    return true;
}
// The control: a walk that ignores the manifest's slots - the base file, then
// the zh file - which reorders base 1 and zh 0 (FIRST.DAT's case, where it
// changes 3 VRAM tiles; docs/cache-read.md section 3).
std::vector<Seen> NaiveWalk(const std::vector<TestFile>& files) {
    std::vector<Seen> out;
    for (const char* rel : {"base\\dat\\MIX.DAT", "loc\\zh-CN\\dat\\MIX.DAT"}) {
        const TestFile* f = nullptr;
        for (const TestFile& t : files)
            if (t.path == Path(rel)) f = &t;
        for (std::size_t off = 0; f && off < f->bytes.size();) {
            Chunk c;
            std::memcpy(&c, f->bytes.data() + off, sizeof c);
            out.push_back({c.kind, c.tag, f->bytes[off + sizeof c]});
            off += sizeof c + static_cast<std::size_t>(c.size);
        }
    }
    return out;
}

From MixingPick(bool armed, bool has_layer, bool has_file) {  // control: the install's file where the cache's layer lacks one
    if (!armed || !has_layer || !has_file) return From::kInstall;
    return From::kCache;
}
From UnarmedPick(bool, bool has_layer, bool has_file) {  // control: the cache before the arming
    if (!has_layer) return From::kInstall;
    return has_file ? From::kCache : From::kNone;
}
bool PickTable(From (*pick)(bool, bool, bool)) {
    for (int i = 0; i < 8; ++i) {
        const bool armed = i & 4, layer = i & 2, file = i & 1;
        const From want = !armed || !layer ? From::kInstall : file ? From::kCache : From::kNone;
        if (pick(armed, layer, file) != want) return false;
    }
    return true;
}

}  // namespace

void SelfTest() {
    std::vector<TestFile> files = GoodFiles();
    std::vector<std::string> opened;
    char error[512];

    // 1. The good manifest: the index, then the controls - the index as a
    // parse that read the rows by layer, ignored widen, or ignored a row with
    // no source would make it, each of which the comparison must refuse.
    if (!TestBegin(kTestRoot, Manifest(GoodRows()).c_str(), TestIo(&files, &opened), error, sizeof error))
        bof3::Fatal("DIV-0089 self-test: the good manifest refused: %s", error);
    const std::string parsed = Compare(g_c.index);
    const bool counts = g_c.held == 3 && g_c.part == 2 && g_c.own == 1;
    unsigned refused = 0;
    {
        Index bad = g_c.index;
        bad["MIX.DAT"].order = "bbbz";
        refused += !Compare(bad).empty();
        bad = g_c.index;
        bad["NONAME.DAT"].zh = false;
        refused += !Compare(bad).empty();
        bad = g_c.index;
        bad["GAP.DAT"].missing = false;
        refused += !Compare(bad).empty();
    }

    // 2. The walk: MIX in the PC's order (asked in lower case, as Windows
    // finds it), BASE whole; NONAME, GAP and a name the manifest lacks not
    // walked and not opened; nothing before the arming. The control: the
    // walk without the slots.
    std::vector<Seen> seen;
    opened.clear();
    const bool mix = WalkShipped("mix.dat", Record, &seen) && Same(seen, kMixWalk) && opened.size() == 2;
    const bool naive_refused = !Same(NaiveWalk(files), kMixWalk);
    seen.clear();
    const bool base = WalkShipped("BASE.DAT", Record, &seen) && seen.size() == 1 && seen[0].kind == 2;
    opened.clear();
    const bool fallback = !WalkShipped("NONAME.DAT", Record, &seen) && !WalkShipped("GAP.DAT", Record, &seen) &&
                          !WalkShipped("OTHER.DAT", Record, &seen) && opened.empty();
    TestSetArmed(false);
    const bool unarmed = !WalkShipped("MIX.DAT", Record, &seen) && opened.empty();
    TestSetArmed(true);

    // 3. The paths and the layers.
    char path[MAX_PATH];
    const bool snd = SoundPath("abc", path, sizeof path) && Path("base\\snd\\abc.DAT") == path &&
                     !SoundPath("XYZ", path, sizeof path);
    LayerPath(path, sizeof path, "opt", "psp-art", "AREA067.DAT");
    const bool layer = Path("opt\\psp-art\\dat\\AREA067.DAT") == path && HasLayer("loc", "en-US") &&
                       HasLayer("loc", "zh-CN") && !HasLayer("loc", "fr-FR") && !HasLayer("opt", "psp-art") &&
                       !HasLayer("loc", "");
    TestSetArmed(false);
    const bool snd_unarmed = !SoundPath("ABC", path, sizeof path);
    TestEnd();

    // 4. The precedence against install's, and its two controls.
    const bool pick = PickTable(PickOverlay);
    refused += !PickTable(MixingPick);
    refused += !PickTable(UnarmedPick);

    // 5. What must be refused, each with the good files: another target, a row
    // cut short, slots out of order, a names row without its table, a layer
    // the shipped file has not, rows of one container apart, no assets table;
    // and against the files, a base file with a chunk more than its rows, a zh
    // tail that is not kind 0.
    struct Bad {
        const char* what;
        std::string manifest;
        std::vector<TestFile> files;
    };
    std::vector<Bad> bads;
    bads.push_back({"another target", Manifest(GoodRows(), "psx-us"), files});
    {
        std::vector<std::string> r = GoodRows();
        r[5] = "  [\"BASE.DAT\", 0, \"base\", \"pc-zh:chunk\"],";
        bads.push_back({"a short row", Manifest(r), files});
        r = GoodRows();
        std::swap(r[0], r[1]);
        bads.push_back({"slots out of order", Manifest(r), files});
        r = GoodRows();
        r[3] = Row("MIX.DAT", 1, "loc/zh-CN", "pc-zh:names");
        bads.push_back({"a names row not after its table", Manifest(r), files});
        r = GoodRows();
        r[6] = Row("ZH.DAT", 0, "loc/en-US", "psx-us:chunk");
        bads.push_back({"a layer not the shipped file's", Manifest(r), files});
        r = GoodRows();
        r.push_back(Row("MIX.DAT", 4, "base", "pc-zh:chunk"));
        bads.push_back({"rows apart", Manifest(r), files});
        bads.push_back({"no assets table", "[meta]\ntarget = \"pc-zh\"\n", files});
    }
    {
        std::vector<TestFile> f = files;
        f[2].bytes = TestContainer({{2, 2, 16, 0xD0}, {0, 0, 4, 0}});
        bads.push_back({"a base chunk more than its rows", Manifest(GoodRows()), f});
        f = files;
        f[1].bytes = TestContainer({{0, 0x4000, 8, 0xC0}, {1, 0x0E001000, 0x800, 0xC1}});
        bads.push_back({"a zh tail of kind 1", Manifest(GoodRows()), f});
    }
    std::string missed;
    for (const Bad& b : bads) {
        std::vector<std::string> o;
        const bool ok = TestBegin(kTestRoot, b.manifest.c_str(), TestIo(&b.files, &o), error, sizeof error);
        TestEnd();
        if (ok) missed += std::string(missed.empty() ? "" : ", ") + b.what;
        else bof3::Log("shadow      DIV-0089 refused (%s): %s", b.what, error);
    }
    g_tfiles = nullptr;
    g_topened = nullptr;

    if (!parsed.empty() || !counts || refused != 5 || !mix || !naive_refused || !base || !fallback || !unarmed ||
        !snd || !layer || !snd_unarmed || !pick || !missed.empty())
        bof3::Fatal("DIV-0089 self-test: index %s, counts %s, controls refused %u of 5, walk %s (naive %s), base %s, "
                    "fallback %s, unarmed %s, sound %s, layers %s, sound unarmed %s, precedence %s; not refused: %s",
                    parsed.empty() ? "ok" : parsed.c_str(), counts ? "ok" : "WRONG", refused, mix ? "ok" : "WRONG",
                    naive_refused ? "refused" : "NOT REFUSED", base ? "ok" : "WRONG", fallback ? "ok" : "WRONG",
                    unarmed ? "ok" : "WRONG", snd ? "ok" : "WRONG", layer ? "ok" : "WRONG",
                    snd_unarmed ? "ok" : "WRONG", pick ? "ok" : "WRONG", missed.empty() ? "none" : missed.c_str());
    bof3::Log("shadow      DIV-0089 self-test: a synthetic manifest parsed to 5 containers (3 held whole, 2 in part), "
              "MIX walked in the PC's slot order with the names last, the paths and layers, the overlay precedence "
              "against install's; 5 controls refused (by-layer order, widen and a sourceless row ignored, a mixing "
              "and an unarmed precedence), the walk without slots refused, %u bad caches refused",
              static_cast<unsigned>(bads.size()));
}

}  // namespace dat_cache

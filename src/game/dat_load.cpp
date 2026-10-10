// The DAT container loader (docs/asset-loading-path.md section 2,
// docs/DAT_CONTAINER.md).
//
// Runs on a 16 KB coroutine stack (docs/SCAFFOLDING.md section 3): no large
// locals here.
#include "game/dat_load.h"

#include "game/battle_text.h"
#include "game/char_names.h"
#include "game/dat_cache.h"
#include "game/config_text.h"
#include "game/fishing_text.h"
#include "game/labels.h"
#include "game/language_tags.h"
#include "game/map_layers.h"
#include "game/menu_verbs.h"

#include <windows.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "bof3/symbols.gen.h"
#include "game/msg_pool.h"
#include "game/name_tables.h"
#include "game/pause_text.h"
#include "game/title_menu.h"
#include "game/text_advance.h"
#include "game/text_draw.h"
#include "game/text_pairs.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

// One chunk header, 16 bytes; the payload follows. The dword at +0x0C is never
// read, by the original or by us. (dat_cache.h's, which the cache's walk hands
// to the same switch.)
using ChunkHeader = dat_cache::Chunk;

constexpr int kTileBytes = 0x800;  // 32 x 32 px at 16 bpp
constexpr int kTile = 0x20;

// Kind-0 payloads land at this base plus the chunk tag: the port's image of
// PSX RAM from 0x80010000 (symbols.toml, MessagePools).
std::uint8_t* Arena() { return reinterpret_cast<std::uint8_t*>(bof3::addr::MessagePools); }

// Kind 1. The tag packs a tile grid: byte 3 = x, byte 2 = y, byte 1 = columns,
// all in 32-px tiles. The payload is size >> 11 tiles, uploaded left to right
// and wrapped to a new row when x reaches x0 + columns*32.
//
// All arithmetic is on 16-bit values compared after sign extension, as in the
// original: rect.x is stored as s16, and the wrap test compares the
// sign-extended s16 with the sign-extended x0 plus the (unsigned) row width.
void LoadImageChunk(std::uint32_t tag, const std::uint8_t* payload, std::int32_t size) {
    std::int16_t rect[4];
    rect[0] = static_cast<std::int16_t>((tag >> 19) & 0x1FE0);
    rect[1] = static_cast<std::int16_t>((tag >> 11) & 0x1FE0);
    rect[2] = kTile;
    rect[3] = kTile;
    const int x0 = rect[0];
    const int wrap_at = x0 + static_cast<int>((tag >> 3) & 0x1FE0);

    for (int tiles = size >> 11; tiles > 0; --tiles) {
        Gfx_LoadImage(rect, payload);
        rect[0] = static_cast<std::int16_t>(rect[0] + kTile);
        if (rect[0] == wrap_at) {
            rect[1] = static_cast<std::int16_t>(rect[1] + kTile);
            rect[0] = static_cast<std::int16_t>(x0);
        }
        payload += kTileBytes;
    }
}

// DIV-0005. The language whose overlays are wanted: BOF3X_LANG, a BCP 47 tag
// (en-US, en-150, fr-FR, de-DE, ja-JP - fixtures.toml's `tag` per build; the
// longest is six characters, so 8 holds it), read once at
// injection because LoadDatFile runs on a coroutine stack. Empty = none, and
// then LoadDatFile does exactly what the original does. "original" is also
// none: the launcher only fills in an EMPTY variable from its settings file,
// so a harness that must not inherit the owner's language sets this
// (docs/launcher-settings.md section 4).
char g_lang[8];

// DIV-0086. The optional layers wanted: BOF3X_OPT, a comma-separated list of
// layer names (docs/opt-layers.md: psp-art, psp-tiles, psp-maps,
// psp-names-en-150, psp-names-ja-JP, area4-walls), in the order they land, read and
// checked once at injection. Each is a letter-or-digit-or-hyphen name of at
// most kOptName - 1 characters; the longest today is 16. Empty = none, and
// "original" and "none" are also none (as BOF3X_LANG's; "none" as the ini's opt=).
// The default of DIV-0080's layer is the launcher's (config.h kOptDefault): it
// names area4-walls here when that layer is installed and opt= is empty.
constexpr int kOptMax = 8;
constexpr int kOptName = 24;
// "DAT\" + a layer + "." + a file name + NUL: 4 + 23 + 1 + 35 + 1. The
// longest shipped name is 12 characters (Dat_FileNames, 742 entries, read
// 2026-10-08); a longer one is refused in LoadDatFile, not skipped.
constexpr int kOptPath = 0x40;
char g_opt[kOptMax][kOptName];
int g_opt_count;

void WalkDatFile(const char* path);

// DIV-0089: whether the cache has the language's layer and each optional
// layer (dat_cache::HasLayer, asked once at injection). Read only once the
// cache is armed; until then, and without a cache, the walk is DIV-0005's and
// DIV-0086's.
bool g_lang_cached;
bool g_opt_cached[kOptMax];

// The file layer the walk reads through and the install's overlay test: the
// game's - File_Open .. Crt_free and GetFileAttributesA, the calls the
// original makes - unless the DIV-0089 self-test stands them in, as it does
// the chunks' handler.
dat_cache::Io g_io = dat_cache::GameIo();
bool InstallHas(const char* path) { return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES; }
bool (*g_install_has)(const char*) = InstallHas;
void TakeChunk(void* ctx, const ChunkHeader& h, std::uint8_t* payload);
dat_cache::ChunkFn g_take = TakeChunk;

// One overlay of DAT\<name>, `kind` "loc" (DIV-0005) or "opt" (DIV-0086), by
// DIV-0089's precedence (dat_cache::PickOverlay): when armed and the cache has
// the layer, the cache's file or none; otherwise `install`, the install's
// DAT\<layer>.<name>, when it exists - the original's test.
void WalkOverlay(const char* kind, const char* layer, bool cached, const char* name, const char* install) {
    static char cache[MAX_PATH];  // static: the 16 KB coroutine stack
    const bool armed = dat_cache::Armed();
    bool has = false;
    if (armed && cached) {
        dat_cache::LayerPath(cache, sizeof cache, kind, layer, name);
        has = g_io.exists(cache);
    }
    switch (dat_cache::PickOverlay(armed, cached, has)) {
    case dat_cache::From::kCache:
        WalkDatFile(cache);
        break;
    case dat_cache::From::kNone:
        break;
    case dat_cache::From::kInstall:
        if (g_install_has(install)) WalkDatFile(install);
        break;
    }
}

// Set by the walk when a kind-0 chunk lands on the area block (tag 0xC8000,
// AreaMap_Header); LoadDatFile then snapshots its side faces (DIV-0085).
bool g_area_block_loaded;

}  // namespace

// original 0x454590. Reads DAT\<name> whole and walks its chunks.
//
// DIVERGENCE DIV-0089: with BOF3X_CACHE set (and not BOF3X_CACHE_DATA=0),
// DAT\<name> is the cache's base\dat\<name> and loc\zh-CN\dat\<name> walked
// chunk by chunk in the PC's slot order when the cache holds it whole, the
// install's otherwise; each overlay below is the cache's loc\<tag>\dat\<name>
// or opt\<layer>\dat\<name> when the cache has that layer at all, else the
// install's (dat_cache.h, docs/cache-read.md).
//
// DIVERGENCE DIV-0086: then, with BOF3X_OPT=<layer>[,<layer>...] set,
// DAT\<layer>.<name> for each layer in that order, when it exists - after the
// language overlay, so a layer lands on top of both. The layers are built by
// tools/importer.py from the player's PSP disc (area4-walls, DIV-0080: a
// Western PSX disc) and copied in by its
// `install`; none ships (docs/opt-layers.md).
//
// DIVERGENCE DIV-0005: with BOF3X_LANG=<tag> set, DAT\<tag>.<name> is walked
// after DAT\<name> when it exists, so its chunks land on top of the shipped ones - a
// kind-0 chunk over the same arena bytes, a kind-3 chunk replacing the glyph
// table (Font_SetGlyphData frees the shipped one, a branch no shipped data
// runs). The overlays are built locally by tools/loc_build.py; none ships
// (docs/dialogue-localisation.md).
//
// Kept from the original, deliberately:
//   - the path is sprintf'd into a 0x28-byte stack buffer with no length check
//     (the longest shipped name fits);
//   - the malloc results are not checked for null;
//   - a chunk whose kind is outside 0..3 (including negative: the byte is
//     sign-extended and compared unsigned) is skipped by its size, not
//     rejected - except kinds 4 to 15, which are ours (DIV-0006, DIV-0008,
//     DIV-0014, DIV-0015, DIV-0018, DIV-0019, DIV-0020, DIV-0052, DIV-0057,
//     DIV-0038, DIV-0064);
//   - the walk trusts each chunk's size; nothing checks that a payload lies
//     inside the file buffer or that a kind-0 tag lies inside the arena;
//   - the kind-3 copy is never freed here: Font_SetGlyphData owns it (and
//     frees the previous one).
extern "C" void __cdecl LoadDatFile(int file_index) {
    const char* name = Dat_FileNames[file_index];
    if (!name) return;

    if (!dat_cache::WalkShipped(name, g_take, nullptr)) {  // DIV-0089: the cache's when it holds it whole
        char path[0x28];
        Crt_sprintf(path, "DAT\\%s", name);
        WalkDatFile(path);
    }

    if (g_lang[0] && std::strlen(name) < 0x20) {  // DIV-0005
        char overlay[0x30];
        Crt_sprintf(overlay, "DAT\\%s.%s", g_lang, name);
        WalkOverlay("loc", g_lang, g_lang_cached, name, overlay);  // DIV-0089
    }
    for (int i = 0; i < g_opt_count; ++i) {  // DIV-0086
        char layer[kOptPath];
        if (4 + std::strlen(g_opt[i]) + 1 + std::strlen(name) + 1 > sizeof layer)
            bof3::Fatal("DIV-0086: DAT\\%s.%s does not fit the 0x%X-byte path buffer", g_opt[i], name,
                        static_cast<unsigned>(sizeof layer));
        Crt_sprintf(layer, "DAT\\%s.%s", g_opt[i], name);
        WalkOverlay("opt", g_opt[i], g_opt_cached[i], name, layer);  // DIV-0089
    }
    if (g_area_block_loaded) {
        g_area_block_loaded = false;
        map_layers::SnapshotSides();  // DIV-0085: the side faces the file's heights give (map_layers.h)
    }
}

namespace {

void WalkDatFile(const char* path) {
    const int handle = g_io.open(path, 0, 0);
    if (handle == -1) return;

    const int file_size = g_io.size(handle);
    auto* file = static_cast<std::uint8_t*>(g_io.malloc(file_size));
    g_io.read(handle, file, file_size);
    g_io.close(handle);

    for (int off = 0; off < file_size;) {
        ChunkHeader h;
        std::memcpy(&h, file + off, sizeof h);
        g_take(nullptr, h, file + off + sizeof h);
        off += h.size + static_cast<int>(sizeof h);
    }
    g_io.free(file);
}

// One chunk, by its kind: the original's switch and our kinds, for the
// install's file and the cache's walk alike.
void TakeChunk(void*, const ChunkHeader& h, std::uint8_t* payload) {
    switch (h.kind) {
    case 0:
        if (Gfx_UploadQueueCount && h.tag == 0x10000) Gfx_UploadQueueCount = 0;
        if (MsgPool_TakeChunk(h.tag, payload, static_cast<std::uint32_t>(h.size))) break;  // DIV-0007
        std::memcpy(Arena() + h.tag, payload, static_cast<std::uint32_t>(h.size));
        if (h.tag == 0xC8000) g_area_block_loaded = true;
        break;
    case 1:
        LoadImageChunk(h.tag, payload, h.size);
        break;
    case 2:
        Snd_LoadBank(h.tag, payload, h.size);
        break;
    case 3: {
        void* copy = Crt_malloc(h.size);
        std::memcpy(copy, payload, static_cast<std::uint32_t>(h.size));
        // DIV-0016: the string draw's glyph-index guard follows the table
        // loaded, set here as well as in our Font_SetGlyphData, so that it
        // holds when that one is Capcom's (an A/B side that keeps this
        // loader ours and the font setter original trapped on DIV-0052's
        // suffix glyph 0xA6B, 2026-09-25).
        TextDraw_SetGlyphCount(static_cast<unsigned>(h.size) / 0x120);
        Font_SetGlyphData(copy, h.size);
        break;
    }
    case 4:  // DIV-0006: ours. No shipped file has one (census of 742).
        TextAdvance_Set(payload, static_cast<std::uint32_t>(h.size), h.tag);
        break;
    case 5:  // DIV-0008: ours.
        NameTables_Apply(h.tag, payload, static_cast<std::uint32_t>(h.size));
        break;
    case 6:  // DIV-0014: ours.
        TitleMenu_SetWidths(h.tag, payload, static_cast<std::uint32_t>(h.size));
        break;
    case 7:  // DIV-0015: ours.
        ConfigText_Apply(h.tag, payload, static_cast<std::uint32_t>(h.size));
        break;
    case 8:  // DIV-0018: ours.
        MenuVerbs_Apply(h.tag, payload, static_cast<std::uint32_t>(h.size));
        break;
    case 9:  // DIV-0019: ours.
        BattleCommands_Apply(h.tag, payload, static_cast<std::uint32_t>(h.size));
        break;
    case 10:  // DIV-0020: ours.
        CharNames_Apply(h.tag, payload, static_cast<std::uint32_t>(h.size));
        break;
    case 11:  // DIV-0020: ours.
        CharNames_ApplyMerchant(h.tag, payload, static_cast<std::uint32_t>(h.size));
        break;
    case 12:  // DIV-0052: ours.
        BattleMessages_Apply(h.tag, payload, static_cast<std::uint32_t>(h.size));
        break;
    case 13:  // DIV-0057: ours.
        TextPairs_Apply(h.tag, payload, static_cast<std::uint32_t>(h.size));
        break;
    case 14:  // DIV-0038: ours.
        PauseText_Apply(h.tag, payload, static_cast<std::uint32_t>(h.size));
        break;
    case 15:  // DIV-0064: ours.
        Labels_Apply(h.tag, payload, static_cast<std::uint32_t>(h.size));
        break;
    case 16:  // DIV-0069: ours.
        FishingText_Apply(h.tag, payload, static_cast<std::uint32_t>(h.size));
        break;
    default:
        break;
    }
}

}  // namespace

namespace {

// BOF3X_OPT into g_opt, refusing - loudly, at start-up - what the walk could
// only skip: a name too long or with a character a file name must not carry,
// one named twice, more than kOptMax, a layer with no file installed, and a
// text layer under another language than BOF3X_LANG's (its names are glyph
// codes of that language's font, DIV-0008). Which layer is text, and of which
// language, is game/language_tags.h's rule, the launcher's too: it drops such
// a layer with a warning before the game starts, so this only fires for a
// BOF3X_OPT set by hand.
void ReadOptLayers() {
    char list[kOptMax * kOptName];
    const DWORD n = GetEnvironmentVariableA("BOF3X_OPT", list, sizeof list);
    if (n >= sizeof list) bof3::Fatal("DIV-0086: BOF3X_OPT is %lu characters; at most %u", n, (unsigned)sizeof list - 1);
    if (n == 0 || std::strcmp(list, "original") == 0 || std::strcmp(list, "none") == 0) return;
    for (char* p = list;;) {
        char* end = std::strchr(p, ',');
        const std::size_t len = end ? static_cast<std::size_t>(end - p) : std::strlen(p);
        if (g_opt_count == kOptMax) bof3::Fatal("DIV-0086: BOF3X_OPT names more than %d layers", kOptMax);
        if (len == 0 || len >= kOptName) bof3::Fatal("DIV-0086: BOF3X_OPT: a layer name of %u characters (1..%d)",
                                                     (unsigned)len, kOptName - 1);
        char* name = g_opt[g_opt_count];
        std::memcpy(name, p, len);
        name[len] = 0;
        for (const char* c = name; *c; ++c)
            if (!((*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') || (*c >= '0' && *c <= '9') || *c == '-'))
                bof3::Fatal("DIV-0086: BOF3X_OPT: layer \"%s\" has a character other than a letter, digit or -", name);
        for (int i = 0; i < g_opt_count; ++i)
            if (std::strcmp(g_opt[i], name) == 0) bof3::Fatal("DIV-0086: BOF3X_OPT names %s twice", name);
        char pattern[kOptPath];
        Crt_sprintf(pattern, "DAT\\%s.*.DAT", name);
        WIN32_FIND_DATAA fd;
        const HANDLE h = FindFirstFileA(pattern, &fd);
        if (h != INVALID_HANDLE_VALUE) FindClose(h);
        else if (!dat_cache::HasLayer("opt", name))  // DIV-0089: or the cache's opt\<layer>\dat
            bof3::Fatal("DIV-0086: BOF3X_OPT names %s, but there is no %s (tools/importer.py install --opt %s)%s%s%s",
                        name, pattern, name, dat_cache::Root() ? " and no " : "",
                        dat_cache::Root() ? dat_cache::Root() : "", dat_cache::Root() ? "\\opt\\<layer>\\dat" : "");
        if (const char* tag = bof3x::LayerLanguage(name)) {
            if (!bof3x::SamePrimaryLanguage(g_lang, tag))
                bof3::Fatal("DIV-0086: layer %s is %s text; BOF3X_LANG is \"%s\"", name, tag, g_lang);
        }
        ++g_opt_count;
        if (!end) break;
        p = end + 1;
    }
}

// DIV-0080's old switch. Area 4's walls were written by coordinate from a table
// in our code, on unless BOF3X_AREA4_WALLS=0; they are now the area4-walls
// layer, built from the player's Western PSX disc and named in BOF3X_OPT
// (docs/opt-layers.md section 1), which the launcher names by default when it
// is installed. A script still asking for the shipped map (0) gets it when
// the layer is not named, with a line, and is refused when it is (the
// launcher's default, or BOF3X_OPT): it would get walls it asked to be
// without. Anything else asked for walls this switch no longer gives, and is
// refused rather than ignored.
void RetiredAreaWallsSwitch() {
    char text[16];
    const DWORD n = GetEnvironmentVariableA("BOF3X_AREA4_WALLS", text, sizeof text);
    if (n == 0) return;
    if (n == 1 && text[0] == '0') {
        for (int i = 0; i < g_opt_count; ++i)
            if (std::strcmp(g_opt[i], "area4-walls") == 0)
                bof3::Fatal("DIV-0080: BOF3X_AREA4_WALLS=0 is retired, and BOF3X_OPT names area4-walls (the "
                            "launcher's default when it is installed); BOF3X_OPT=none, or an ini opt=none, "
                            "plays the shipped map");
        bof3::Log("DIV-0080: BOF3X_AREA4_WALLS is retired; area 4's map is the one loaded, BOF3X_OPT not naming "
                  "area4-walls");
        return;
    }
    bof3::Fatal("DIV-0080: BOF3X_AREA4_WALLS is retired - area 4's walls are the area4-walls layer from a Western "
                "PSX disc (tools/importer.py build and install, on by default with such a disc; BOF3X_OPT=area4-walls)");
}

// --- BOF3X_SHADOW=dat_cache: DIV-0089 through LoadDatFile ---------------------
//
// LoadDatFile itself on a synthetic cache and install (dat_cache's in-memory
// files), the chunks' handler stood in by a recorder: what is walked, in what
// order, from where. Two names the game's table holds (their strings only; no
// file of the game's is read). The install has DAT\<name> and three overlays;
// the cache holds <name> whole (base, zh, base by the manifest's slots), the
// language layer with a file for <name>, and the layer lay-a with a file for
// <name2> only - so its stale DAT\lay-a.<name> must not be walked, as
// importer.py install would have removed it - and not the layer lay-b.
struct Walked {
    std::uint32_t tag;
    std::uint8_t first;
};
std::vector<Walked>* g_walked;
void RecordChunk(void*, const ChunkHeader& h, std::uint8_t* payload) {
    g_walked->push_back({h.tag, h.size > 0 ? payload[0] : std::uint8_t{0}});
}
bool SameWalk(const std::vector<Walked>& a, const std::vector<Walked>& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (a[i].tag != b[i].tag || a[i].first != b[i].first) return false;
    return true;
}

void CacheSelfTest() {
    int index = -1, index2 = -1;
    for (unsigned i = 0; i < Dat_FileNames_count && index2 < 0; ++i) {
        const char* n = Dat_FileNames[i];
        if (!n || std::strlen(n) > 12) continue;
        if (index < 0) index = static_cast<int>(i);
        else if (lstrcmpiA(n, Dat_FileNames[index]) != 0) index2 = static_cast<int>(i);
    }
    if (index2 < 0) bof3::Fatal("DIV-0089 self-test: Dat_FileNames has no two names");
    const std::string name = Dat_FileNames[index], name2 = Dat_FileNames[index2];
    const std::string root = "T:\\bof3x-dat-selftest";
    using dat_cache::TestContainer;
    std::vector<dat_cache::TestFile> files = {
        {root + "\\base\\dat\\" + name, TestContainer({{0, 0x10, 4, 1}, {0, 0x20, 4, 2}})},
        {root + "\\loc\\zh-CN\\dat\\" + name, TestContainer({{0, 0x30, 4, 3}})},
        {root + "\\loc\\xx-TEST\\dat\\" + name, TestContainer({{0, 0x40, 4, 4}})},
        {root + "\\opt\\lay-a\\dat\\" + name2, TestContainer({{0, 0xA0, 4, 10}})},
        {"DAT\\" + name, TestContainer({{0, 0x50, 4, 5}})},
        {"DAT\\xx-TEST." + name, TestContainer({{0, 0x60, 4, 6}})},
        {"DAT\\lay-a." + name, TestContainer({{0, 0x70, 4, 7}})},
        {"DAT\\lay-b." + name, TestContainer({{0, 0x80, 4, 8}})},
        {"DAT\\" + name2, TestContainer({{0, 0x90, 4, 9}})}};
    const std::string hash(64, 'a');
    const std::string manifest = "[meta]\ntarget = \"pc-zh\"\n\n[cache]\nassets = [\n"
                                 "  [\"" + name + "\", 0, \"base\", \"pc-zh:chunk\", \"" + hash + "\"],\n"
                                 "  [\"" + name + "\", 1, \"loc/zh-CN\", \"pc-zh:chunk\", \"" + hash + "\"],\n"
                                 "  [\"" + name + "\", 2, \"base\", \"pc-zh:chunk\", \"" + hash + "\"],\n]\n";
    std::vector<std::string> opened;
    const dat_cache::Io io = dat_cache::TestIo(&files, &opened);
    char error[512];
    if (!dat_cache::TestBegin(root.c_str(), manifest.c_str(), io, error, sizeof error))
        bof3::Fatal("DIV-0089 self-test: the LoadDatFile cache refused: %s", error);

    char lang[sizeof g_lang];
    std::memcpy(lang, g_lang, sizeof lang);
    char opt[kOptMax][kOptName];
    std::memcpy(opt, g_opt, sizeof opt);
    const int opt_count = g_opt_count;
    const bool lang_cached = g_lang_cached;
    bool opt_cached[kOptMax];
    std::memcpy(opt_cached, g_opt_cached, sizeof opt_cached);
    const dat_cache::Io saved_io = g_io;
    const auto saved_has = g_install_has;
    const dat_cache::ChunkFn saved_take = g_take;

    std::strcpy(g_lang, "xx-TEST");
    std::strcpy(g_opt[0], "lay-a");
    std::strcpy(g_opt[1], "lay-b");
    g_opt_count = 2;
    g_lang_cached = dat_cache::HasLayer("loc", g_lang);
    g_opt_cached[0] = dat_cache::HasLayer("opt", "lay-a");
    g_opt_cached[1] = dat_cache::HasLayer("opt", "lay-b");
    g_io = io;
    g_install_has = io.exists;
    g_take = RecordChunk;
    std::vector<Walked> walked;
    g_walked = &walked;
    const auto opened_any = [&](const std::string& p) {
        for (const std::string& o : opened)
            if (lstrcmpiA(o.c_str(), p.c_str()) == 0) return true;
        return false;
    };

    // Armed: the cache's container in slot order, its language file, nothing
    // for lay-a (the cache's layer lacks <name>), the install's lay-b; none of
    // the install's files the cache stands for opened.
    const std::vector<Walked> want_cache = {{0x10, 1}, {0x30, 3}, {0x20, 2}, {0x40, 4}, {0x80, 8}};
    LoadDatFile(index);
    const bool cache = g_lang_cached && g_opt_cached[0] && !g_opt_cached[1] && SameWalk(walked, want_cache) &&
                       !opened_any("DAT\\" + name) && !opened_any("DAT\\xx-TEST." + name) &&
                       !opened_any("DAT\\lay-a." + name);
    // Armed, a name the manifest lacks: the install's container, the cache's
    // lay-a file; the cache's language layer has no <name2>, so none.
    walked.clear();
    opened.clear();
    LoadDatFile(index2);
    const bool other = SameWalk(walked, {{0x90, 9}, {0xA0, 10}}) && !opened_any("DAT\\xx-TEST." + name2);
    // Not armed: exactly the install's walk, DIV-0005's and DIV-0086's.
    walked.clear();
    opened.clear();
    dat_cache::TestSetArmed(false);
    const std::vector<Walked> want_install = {{0x50, 5}, {0x60, 6}, {0x70, 7}, {0x80, 8}};
    LoadDatFile(index);
    const bool install = SameWalk(walked, want_install) && opened.size() == 4;
    // The controls: the armed walk as each wrong precedence would give it -
    // the install's container, the install's stale lay-a file, the install's
    // language file over the cache's, the cache's base and zh files one after
    // the other - each refused by the comparison.
    unsigned refused = 0;
    refused += !SameWalk({{0x50, 5}, {0x40, 4}, {0x80, 8}}, want_cache);
    refused += !SameWalk({{0x10, 1}, {0x30, 3}, {0x20, 2}, {0x40, 4}, {0x70, 7}, {0x80, 8}}, want_cache);
    refused += !SameWalk({{0x10, 1}, {0x30, 3}, {0x20, 2}, {0x60, 6}, {0x80, 8}}, want_cache);
    refused += !SameWalk({{0x10, 1}, {0x20, 2}, {0x30, 3}, {0x40, 4}, {0x80, 8}}, want_cache);

    std::memcpy(g_lang, lang, sizeof lang);
    std::memcpy(g_opt, opt, sizeof opt);
    g_opt_count = opt_count;
    g_lang_cached = lang_cached;
    std::memcpy(g_opt_cached, opt_cached, sizeof opt_cached);
    g_io = saved_io;
    g_install_has = saved_has;
    g_take = saved_take;
    g_walked = nullptr;
    dat_cache::TestEnd();

    if (!cache || !other || !install || refused != 4)
        bof3::Fatal("DIV-0089 self-test: LoadDatFile from the cache %s, a name it lacks %s, unarmed %s; controls "
                    "refused %u of 4",
                    cache ? "ok" : "WRONG", other ? "ok" : "WRONG", install ? "ok" : "WRONG", refused);
    bof3::Log("shadow      DIV-0089 self-test: LoadDatFile(%d) %s from the cache in slot order with the cache's "
              "language file, no stale lay-a, the install's lay-b; %s from the install with the cache's lay-a; "
              "unarmed the install's four; 4 controls refused",
              index, name.c_str(), name2.c_str());
}

}  // namespace

void DatLoad_Inject() {
    if (bof3::WantsShadow("dat_cache")) {
        dat_cache::SelfTest();
        CacheSelfTest();
    }
    dat_cache::Configure();  // DIV-0089: before the layers are read, whose test it answers too
    const DWORD n = GetEnvironmentVariableA("BOF3X_LANG", g_lang, sizeof g_lang);
    if (n == 0 || n >= sizeof g_lang || std::strcmp(g_lang, "original") == 0) g_lang[0] = 0;
    // The bare codes of before 2026-10-08 are retired, not read (DIV-0005):
    // DAT\en.* is no overlay this project builds any more.
    if (const char* use = bof3x::RetiredLanguageReplacement(g_lang))
        bof3::Fatal("DIV-0005: BOF3X_LANG=%s is retired; use %s", g_lang, use);
    if (g_lang[0]) MsgPool_Relocate();  // DIV-0007: English text runs past the pool's place
    if (g_lang[0]) bof3::Log("DIV-0005: language overlays DAT\\%s.*.DAT", g_lang);
    ReadOptLayers();
    for (int i = 0; i < g_opt_count; ++i) bof3::Log("DIV-0086: optional layer %d, DAT\\%s.*.DAT", i + 1, g_opt[i]);
    if (g_lang[0]) g_lang_cached = dat_cache::HasLayer("loc", g_lang);  // DIV-0089
    for (int i = 0; i < g_opt_count; ++i) g_opt_cached[i] = dat_cache::HasLayer("opt", g_opt[i]);
    if (dat_cache::Root()) {
        if (g_lang[0])
            bof3::Log("DIV-0089    language %s: %s", g_lang, g_lang_cached ? "the cache's loc layer" : "the install's");
        for (int i = 0; i < g_opt_count; ++i)
            bof3::Log("DIV-0089    layer %s: %s", g_opt[i], g_opt_cached[i] ? "the cache's opt layer" : "the install's");
    }
    RetiredAreaWallsSwitch();
    BOF3_INJECT(LoadDatFile);
}

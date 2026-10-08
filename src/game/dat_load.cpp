// The DAT container loader (docs/asset-loading-path.md section 2,
// docs/DAT_CONTAINER.md).
//
// Runs on a 16 KB coroutine stack (docs/SCAFFOLDING.md section 3): no large
// locals here.
#include "game/dat_load.h"

#include "game/area4_walls.h"
#include "game/battle_text.h"
#include "game/char_names.h"
#include "game/config_text.h"
#include "game/fishing_text.h"
#include "game/labels.h"
#include "game/map_layers.h"
#include "game/menu_verbs.h"

#include <windows.h>

#include <cstdint>
#include <cstring>

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
// read, by the original or by us.
struct ChunkHeader {
    std::int8_t kind;
    std::uint8_t pad[3];
    std::uint32_t tag;
    std::int32_t size;
    std::uint32_t unread;
};
static_assert(sizeof(ChunkHeader) == 0x10);

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
// psp-names-en-150, psp-names-ja-JP), in the order they land, read and
// checked once at injection. Each is a letter-or-digit-or-hyphen name of at
// most kOptName - 1 characters; the longest today is 16. Empty = none, and
// "original" is also none (as BOF3X_LANG's).
constexpr int kOptMax = 8;
constexpr int kOptName = 24;
// "DAT\" + a layer + "." + a file name + NUL: 4 + 23 + 1 + 35 + 1. The
// longest shipped name is 12 characters (Dat_FileNames, 742 entries, read
// 2026-10-08); a longer one is refused in LoadDatFile, not skipped.
constexpr int kOptPath = 0x40;
char g_opt[kOptMax][kOptName];
int g_opt_count;

void WalkDatFile(const char* path);

// Set by the walk when a kind-0 chunk lands on the area block (tag 0xC8000,
// AreaMap_Header); LoadDatFile then snapshots its side faces (DIV-0085).
bool g_area_block_loaded;

}  // namespace

// original 0x454590. Reads DAT\<name> whole and walks its chunks.
//
// DIVERGENCE DIV-0086: then, with BOF3X_OPT=<layer>[,<layer>...] set,
// DAT\<layer>.<name> for each layer in that order, when it exists - after the
// language overlay, so a layer lands on top of both. The layers are built by
// tools/importer.py from the player's PSP disc and copied in by its
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

    char path[0x28];
    Crt_sprintf(path, "DAT\\%s", name);
    WalkDatFile(path);

    if (g_lang[0] && std::strlen(name) < 0x20) {  // DIV-0005
        char overlay[0x30];
        Crt_sprintf(overlay, "DAT\\%s.%s", g_lang, name);
        if (GetFileAttributesA(overlay) != INVALID_FILE_ATTRIBUTES) WalkDatFile(overlay);
    }
    for (int i = 0; i < g_opt_count; ++i) {  // DIV-0086
        char layer[kOptPath];
        if (4 + std::strlen(g_opt[i]) + 1 + std::strlen(name) + 1 > sizeof layer)
            bof3::Fatal("DIV-0086: DAT\\%s.%s does not fit the 0x%X-byte path buffer", g_opt[i], name,
                        static_cast<unsigned>(sizeof layer));
        Crt_sprintf(layer, "DAT\\%s.%s", g_opt[i], name);
        if (GetFileAttributesA(layer) != INVALID_FILE_ATTRIBUTES) WalkDatFile(layer);
    }
    area4_walls::Apply(name);  // the later discs' walls in area 4 (BOF3X_AREA4_WALLS; area4_walls.h)
    if (g_area_block_loaded) {
        g_area_block_loaded = false;
        map_layers::SnapshotSides();  // DIV-0085: the side faces the file's heights give (map_layers.h)
    }
}

namespace {

void WalkDatFile(const char* path) {
    const int handle = File_Open(path, 0, 0);
    if (handle == -1) return;

    const int file_size = File_Size(handle);
    auto* file = static_cast<std::uint8_t*>(Crt_malloc(file_size));
    File_Read(handle, file, file_size);
    File_Close(handle);

    for (int off = 0; off < file_size;) {
        ChunkHeader h;
        std::memcpy(&h, file + off, sizeof h);
        std::uint8_t* payload = file + off + sizeof h;

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
        off += h.size + static_cast<int>(sizeof h);
    }
    Crt_free(file);
}

}  // namespace

namespace {

// The language a text layer's name ends in - "-<tag>" with the tag two
// lowercase letters, then optionally "-" and two capitals or three digits
// (en-150, ja-JP; fixtures.toml's tags) - or null for a layer that is not text.
const char* LayerLanguage(const char* layer) {
    for (const char* p = std::strchr(layer, '-'); p; p = std::strchr(p + 1, '-')) {
        const char* t = p + 1;
        auto lower = [](char c) { return c >= 'a' && c <= 'z'; };
        auto upper = [](char c) { return c >= 'A' && c <= 'Z'; };
        auto digit = [](char c) { return c >= '0' && c <= '9'; };
        if (!lower(t[0]) || !lower(t[1])) continue;
        if (t[2] == 0) return t;
        if (t[2] != '-') continue;
        if (upper(t[3]) && upper(t[4]) && t[5] == 0) return t;
        if (digit(t[3]) && digit(t[4]) && digit(t[5]) && t[6] == 0) return t;
    }
    return nullptr;
}

// BOF3X_OPT into g_opt, refusing - loudly, at start-up - what the walk could
// only skip: a name too long or with a character a file name must not carry,
// one named twice, more than kOptMax, a layer with no file installed, and a
// text layer under another language than BOF3X_LANG's (its names are glyph
// codes of that language's font, DIV-0008).
void ReadOptLayers() {
    char list[kOptMax * kOptName];
    const DWORD n = GetEnvironmentVariableA("BOF3X_OPT", list, sizeof list);
    if (n >= sizeof list) bof3::Fatal("DIV-0086: BOF3X_OPT is %lu characters; at most %u", n, (unsigned)sizeof list - 1);
    if (n == 0 || std::strcmp(list, "original") == 0) return;
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
        if (h == INVALID_HANDLE_VALUE)
            bof3::Fatal("DIV-0086: BOF3X_OPT names %s, but there is no %s (tools/importer.py install --opt %s)", name,
                        pattern, name);
        FindClose(h);
        if (const char* tag = LayerLanguage(name)) {
            const std::size_t primary = std::strcspn(g_lang, "-");
            if (primary != 2 || std::strncmp(g_lang, tag, 2) != 0)
                bof3::Fatal("DIV-0086: layer %s is %s text; BOF3X_LANG is \"%s\"", name, tag, g_lang);
        }
        ++g_opt_count;
        if (!end) break;
        p = end + 1;
    }
}

}  // namespace

void DatLoad_Inject() {
    const DWORD n = GetEnvironmentVariableA("BOF3X_LANG", g_lang, sizeof g_lang);
    if (n == 0 || n >= sizeof g_lang || std::strcmp(g_lang, "original") == 0) g_lang[0] = 0;
    if (g_lang[0]) MsgPool_Relocate();  // DIV-0007: English text runs past the pool's place
    if (g_lang[0]) bof3::Log("DIV-0005: language overlays DAT\\%s.*.DAT", g_lang);
    ReadOptLayers();
    for (int i = 0; i < g_opt_count; ++i) bof3::Log("DIV-0086: optional layer %d, DAT\\%s.*.DAT", i + 1, g_opt[i]);
    BOF3_INJECT(LoadDatFile);
}

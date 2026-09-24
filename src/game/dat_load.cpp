// The DAT container loader (docs/asset-loading-path.md section 2,
// docs/DAT_CONTAINER.md).
//
// Runs on a 16 KB coroutine stack (docs/SCAFFOLDING.md section 3): no large
// locals here.
#include "game/dat_load.h"

#include "game/char_names.h"
#include "game/config_text.h"
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

// DIV-0005. The language whose overlays are wanted: BOF3X_LANG, read once at
// injection because LoadDatFile runs on a coroutine stack. Empty = none, and
// then LoadDatFile does exactly what the original does. "original" is also
// none: the launcher only fills in an EMPTY variable from its settings file,
// so a harness that must not inherit the owner's language sets this
// (docs/launcher-settings.md section 4).
char g_lang[8];

void WalkDatFile(const char* path);

}  // namespace

// original 0x454590. Reads DAT\<name> whole and walks its chunks.
//
// DIVERGENCE DIV-0005: with BOF3X_LANG=xx set, DAT\xx.<name> is walked after
// DAT\<name> when it exists, so its chunks land on top of the shipped ones - a
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
//     rejected - except kinds 4 to 11, which are ours (DIV-0006, DIV-0008,
//     DIV-0014, DIV-0015, DIV-0018, DIV-0019, DIV-0020);
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
            Font_SetGlyphData(copy, h.size);
            break;
        }
        case 4:  // DIV-0006: ours. No shipped file has one (census of 742).
            TextAdvance_Set(payload, static_cast<std::uint32_t>(h.size), h.tag);
            PauseText_Apply();   // DIV-0038: the English glyphs are in the table now
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
        default:
            break;
        }
        off += h.size + static_cast<int>(sizeof h);
    }
    Crt_free(file);
}

}  // namespace

void DatLoad_Inject() {
    const DWORD n = GetEnvironmentVariableA("BOF3X_LANG", g_lang, sizeof g_lang);
    if (n == 0 || n >= sizeof g_lang || std::strcmp(g_lang, "original") == 0) g_lang[0] = 0;
    if (g_lang[0]) MsgPool_Relocate();  // DIV-0007: English text runs past the pool's place
    if (g_lang[0]) bof3::Log("DIV-0005: language overlays DAT\\%s.*.DAT", g_lang);
    BOF3_INJECT(LoadDatFile);
}

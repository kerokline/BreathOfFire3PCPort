// The DAT container loader (docs/asset-loading-path.md section 2,
// docs/DAT_CONTAINER.md).
//
// Runs on a 16 KB coroutine stack (docs/SCAFFOLDING.md section 3): no large
// locals here.
#include "game/dat_load.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
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

}  // namespace

// original 0x454590. Reads DAT\<name> whole and walks its chunks.
//
// Kept from the original, deliberately:
//   - the path is sprintf'd into a 0x28-byte stack buffer with no length check
//     (the longest shipped name fits);
//   - the malloc results are not checked for null;
//   - a chunk whose kind is outside 0..3 (including negative: the byte is
//     sign-extended and compared unsigned) is skipped by its size, not
//     rejected;
//   - the walk trusts each chunk's size; nothing checks that a payload lies
//     inside the file buffer or that a kind-0 tag lies inside the arena;
//   - the kind-3 copy is never freed here: Font_SetGlyphData owns it (and
//     frees the previous one).
extern "C" void __cdecl LoadDatFile(int file_index) {
    const char* name = Dat_FileNames[file_index];
    if (!name) return;

    char path[0x28];
    Crt_sprintf(path, "DAT\\%s", name);
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
            if (Dat_ClearedByTag10000 && h.tag == 0x10000) Dat_ClearedByTag10000 = 0;
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
        default:
            break;
        }
        off += h.size + static_cast<int>(sizeof h);
    }
    Crt_free(file);
}

void DatLoad_Inject() {
    BOF3_INJECT(LoadDatFile);
}

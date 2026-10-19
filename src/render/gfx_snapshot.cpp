// The quicksave's graphics participant (gfx_snapshot.h; docs/quicksave.md
// section 9). Nothing here replaces Capcom's code: it runs only at F6 / F7,
// on the snapshot core's own stack, between two logic frames.
#include "render/gfx_snapshot.h"

#include <windows.h>

#include <cstdint>
#include <cstring>
#include <vector>

#include "bof3/symbols.gen.h"
#include "game/layering.h"
#include "game/map_layers.h"
#include "game/wide_rand.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/host.h"
#include "hook/log.h"
#include "hook/snapshot.h"
#include "render/render_shim.h"

namespace {

using U = std::uint32_t;
using bof3::Fatal;
using bof3::Log;

unsigned char* At(U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }
template <class T>
U Va(T* p) {
    return static_cast<U>(reinterpret_cast<std::uintptr_t>(p));
}
U Long(U address) {
    U v;
    std::memcpy(&v, At(address), 4);
    return v;
}
void PutLong(U address, U v) { std::memcpy(At(address), &v, 4); }
std::uint16_t Word(U address) {
    std::uint16_t v;
    std::memcpy(&v, At(address), 2);
    return v;
}

constexpr U kVersion = 1;

// --- the display words the live process keeps ----------------------------------
// The snapshot core keeps Gfx_WindowRect .. 0x6BEA00, Gfx_ClutRows' table,
// D3d_DeviceDesc, Dd_DirectDraw, Dd_StageSurface and D3d_Device
// (docs/quicksave.md 3). These are the rest of what Display_Setup writes (and
// DIV-0042's Rescale on a resize, F8's teardown and set-up between a take and
// a load): this process's objects, window size, scale and formats. Equal to
// the take's within one process that was not resized; a quicksave made in a
// window of another size, or before an F8, must not bring that one's back.
// Not kept: D3d_ShadeModeCache (0x6C3A44), a cache of the pipe state the
// record carries - the take's value and the take's pipe go together.
struct Kept {
    U at, bytes;
    const char* name;
};
constexpr Kept kKept[] = {
    {0x66B710, 12, "Gfx_ScreenRect's right and bottom, the device count less one"},
    {0x6BEA00, 0x14, "Dd_CellBackdrop and the backdrop pairs"},
    {0x6C3A40, 4, "D3d_BackMaterialHandle"},
    {0x6C3A48, 0x4F8, "0x6C3A48, Gfx_RenderFlags, the device records"},
    {0x6C9F40, 4, "the desktop's height"},
    {0x7C9F44, 12, "the desktop's width, D3d_ScaleY, D3d_ScaleX"},
    {0x7CADF8, 12, "D3d_AfterDraw's three surfaces"},
    {0x7CAE20, 0x18, "Display_ReleaseOrphans' six objects"},
    {0x7CC338, 12, "DDraw_Primary, DDraw_BackBuffer, the Z-buffer"},
    {0x7CC348, 8, "the clipper, the IDirect3D"},
    {0x7CC354, 8, "D3d_Viewport, D3d_BackMaterial"},
    {0x7DED5C, 0xC4, "the back buffer's pitch, Gfx_PixelFormat's three records"},
};
constexpr std::size_t kKeptCount = sizeof kKept / sizeof kKept[0];

// --- the caches -----------------------------------------------------------------
// Gfx_TexCache: 32 pages of 32 entries of 0x18 (state +0, texture +0x14,
// surface +0x10), a page's list ending at its first state 0 (gfx_texcache.cpp).
constexpr U kPageEntries = 32, kPages = 32, kTexEntry = 0x18;
// Font_TexCache: 128 of 0x14 - glyph +0, CLUT +2, generation +4, surface +8,
// texture +0xC, used this frame +0x10 (symbols.toml).
constexpr U kFontEntries = 128, kFontEntry = 0x14;
// D3d_CellTexCache: 128 of 0x28 - surface +0x20, texture +0x24 (d3d_draw.cpp).
constexpr U kCellEntries = 128, kCellEntry = 0x28;
// Gfx_ClutRows: 512 of {generation, 0x1000-byte block} (gfx_clut.cpp).
constexpr U kRows = 512, kRowBytes = 0x1000;
static_assert(kPages * kPageEntries * kTexEntry == Gfx_TexCache_count, "Gfx_TexCache's size");
static_assert(kFontEntries * kFontEntry == Font_TexCache_count, "Font_TexCache's size");
static_assert(kCellEntries * kCellEntry == D3d_CellTexCache_count, "D3d_CellTexCache's size");
static_assert(kRows * 2 == Gfx_ClutRows_count, "Gfx_ClutRows' size");

U TexEntry(U page, U i) { return Va(Gfx_TexCache) + (page * kPageEntries + i) * kTexEntry; }
U FontEntry(U i) { return Va(Font_TexCache) + i * kFontEntry; }
U CellEntry(U i) { return Va(D3d_CellTexCache) + i * kCellEntry; }
U RowAt(U y) { return Va(Gfx_ClutRows) + y * 8; }

void* Object(U slot) { return reinterpret_cast<void*>(static_cast<std::uintptr_t>(Long(slot))); }

// Every object the live caches hold, in the order the teardown releases them
// (Gfx_ReleaseTexCache, Font_ReleaseTexCache, D3d_FreeCellTextures): the
// texture interface before its surface. Released after the next present.
U ReleaseLiveCaches() {
    U n = 0;
    const auto release = [&n](U slot) {
        if (void* o = Object(slot)) {
            render::ReleaseAfterPresent(o);
            ++n;
        }
    };
    for (U page = 0; page < kPages; ++page)
        for (U i = 0; i < kPageEntries; ++i) {
            const U e = TexEntry(page, i);
            if (At(e)[0] == 0) break;
            release(e + 0x14);
            release(e + 0x10);
        }
    for (U i = 0; i < kFontEntries; ++i) {
        release(FontEntry(i) + 0xC);
        release(FontEntry(i) + 8);
    }
    for (U i = 0; i < kCellEntries; ++i) {
        release(CellEntry(i) + 0x24);
        release(CellEntry(i) + 0x20);
    }
    return n;
}

// --- the participant ------------------------------------------------------------

std::vector<unsigned char> g_live[kKeptCount];
bool g_before_ran = false;
U g_released = 0;

void Before() {
    for (std::size_t i = 0; i < kKeptCount; ++i) g_live[i].assign(At(kKept[i].at), At(kKept[i].at + kKept[i].bytes));
    g_released = ReleaseLiveCaches();
    g_before_ran = true;
}

void Save(bof3::SnapshotWriter& w) {
    w.Put(kVersion);
    // The palette rows: every generation; each row's block as it stands.
    U n = 0;
    for (U y = 0; y < kRows; ++y) {
        w.Put(Long(RowAt(y)));
        n += Long(RowAt(y) + 4) != 0 ? 1u : 0u;
    }
    w.Put(n);
    for (U y = 0; y < kRows; ++y) {
        const U block = Long(RowAt(y) + 4);
        if (!block) continue;
        w.Put(static_cast<std::uint16_t>(y));
        w.Put(At(block), kRowBytes);
    }
    // The device's pipe state between draws, and the clear colour.
    const render::PipeState pipe = render::PipeForSnapshot();
    w.Put(static_cast<U>(sizeof pipe));
    w.Put(pipe);
    w.Put(render::ClearColor());
    // Ours: DIV-0071, DIV-0085, DIV-0041's two.
    layering::SnapshotSave(w);
    map_layers::QuickSaveSides(w);
    Widescreen_QuickSave(w);
    wide_rand::QuickSave(w);
}

struct Tally {
    U rows, rows_made, rows_freed, glyphs;
};

Tally RestoreRows(bof3::SnapshotReader& r) {
    Tally t = {};
    U generation[kRows];
    for (U y = 0; y < kRows; ++y) generation[y] = r.Get<U>();
    const U n = r.Get<U>();
    if (n > kRows) Fatal("gfx_snapshot: a quicksave with %u palette rows, more than %u", n, kRows);
    bool has[kRows] = {};
    static unsigned char block[kRowBytes];
    U last = 0;
    for (U k = 0; k < n; ++k) {
        const U y = r.Get<std::uint16_t>();
        if (y >= kRows || (k && y <= last)) Fatal("gfx_snapshot: a quicksave's palette row %u out of order or range", y);
        last = y;
        r.Get(block, kRowBytes);
        has[y] = true;
        U live = Long(RowAt(y) + 4);
        if (!live) {
            void* made = Crt_malloc(kRowBytes);   // as Gfx_ConvertRow makes a row's block
            if (!made) Fatal("gfx_snapshot: out of memory for palette row %u", y);
            live = Va(made);
            PutLong(RowAt(y) + 4, live);
            ++t.rows_made;
        }
        std::memcpy(At(live), block, kRowBytes);
        ++t.rows;
    }
    for (U y = 0; y < kRows; ++y) {
        if (!has[y]) {
            if (void* live = Object(RowAt(y) + 4)) {
                // Not wanted yet at the take: Gfx_ClutPixels converts it on
                // its first use again, as it did then.
                Crt_free(live);
                PutLong(RowAt(y) + 4, 0);
                ++t.rows_freed;
            }
        }
        PutLong(RowAt(y), generation[y]);
    }
    return t;
}

// Every entry built at the take, built again: the take's glyph in the take's
// CLUT (the rows are the take's by now), then the take's words over what the
// build wrote - glyph, CLUT, generation and the used word - so the cache's
// lookups and its choice of a free slot go as they would have.
U RebuildGlyphs() {
    U n = 0;
    for (U i = 0; i < kFontEntries; ++i) {
        const U e = FontEntry(i);
        unsigned char take[kFontEntry];
        std::memcpy(take, At(e), kFontEntry);
        PutLong(e + 8, 0);
        PutLong(e + 0xC, 0);
        U surface;
        std::memcpy(&surface, take + 8, 4);
        if (!surface) continue;
        Font_BuildGlyphTexture(static_cast<int>(i), Word(e), Word(e + 2));
        if (Long(e + 8) == 0 || Long(e + 0xC) == 0)
            Fatal("gfx_snapshot: Font_TexCache entry %u (glyph %04X, CLUT %04X) could not be built again", i, Word(e), Word(e + 2));
        std::memcpy(At(e), take, 8);
        std::memcpy(At(e + 0x10), take + 0x10, kFontEntry - 0x10);
        ++n;
    }
    return n;
}

void Restore(bof3::SnapshotReader& r) {
    if (!g_before_ran) Fatal("gfx_snapshot: a restore without the before-restore step: the live caches' objects are lost");
    g_before_ran = false;
    if (r.Get<U>() != kVersion) Fatal("gfx_snapshot: the record is another version");
    for (std::size_t i = 0; i < kKeptCount; ++i) std::memcpy(At(kKept[i].at), g_live[i].data(), kKept[i].bytes);
    Tally t = RestoreRows(r);
    // The page and cell textures are built again when a draw asks for them:
    // from the restored shadow and rows, into the empty tables (the take's
    // pointers name the other moment's surfaces; nothing releases them).
    std::memset(Gfx_TexCache, 0, Gfx_TexCache_count);
    std::memset(D3d_CellTexCache, 0, D3d_CellTexCache_count);
    t.glyphs = RebuildGlyphs();
    if (r.Get<U>() != sizeof(render::PipeState)) Fatal("gfx_snapshot: the record's pipe state is another size");
    const render::PipeState pipe = r.Get<render::PipeState>();
    const U clear = r.Get<U>();
    render::RestorePipe(pipe, clear);
    render::ForgetGrabs();
    layering::SnapshotRestore(r);
    map_layers::QuickRestoreSides(r);
    Widescreen_QuickRestore(r);
    wide_rand::QuickRestore(r);
    Log("quicksave   GFX: %u palette rows put back (%u blocks made, %u freed), %u glyph textures built again, page and cell "
        "caches emptied, %u live objects released after the next present; pipe state, grabs forgotten, DIV-0071 / 0085 / "
        "0041 state as at the take",
        t.rows, t.rows_made, t.rows_freed, t.glyphs, g_released);
}

// --- the self-test -----------------------------------------------------------
// Headless, before the game has a window: no device, no glyph table. What it
// checks: the palette rows as at the take whatever the live rows hold (a row
// made since, a row freed since, other pixels, other generations); the live
// caches' objects released only after a present, and the tables emptied; the
// kept words keeping their live values; the record read to its end. The glyph
// rebuild and the first frame are the live check's (docs/quicksave.md 9).

int g_fails = 0;
void Check(bool ok, const char* what) {
    if (ok) return;
    ++g_fails;
    Log("shadow      gfx_snapshot: FAIL %s", what);
}

void SelfTest() {
    // The game's words as they are (zero at start-up), put back at the end.
    std::vector<unsigned char> rows0(At(RowAt(0)), At(RowAt(0) + kRows * 8));
    std::vector<unsigned char> tex0(Gfx_TexCache, Gfx_TexCache + Gfx_TexCache_count);
    std::vector<unsigned char> font0(Font_TexCache, Font_TexCache + Font_TexCache_count);
    std::vector<unsigned char> cell0(D3d_CellTexCache, D3d_CellTexCache + D3d_CellTexCache_count);
    std::vector<std::vector<unsigned char>> kept0(kKeptCount);
    for (std::size_t i = 0; i < kKeptCount; ++i) kept0[i].assign(At(kKept[i].at), At(kKept[i].at + kKept[i].bytes));

    // The take: rows 3, 40 and 480 with blocks, 41 without.
    auto make_row = [](U y, U generation, unsigned char fill) {
        void* b = Crt_malloc(kRowBytes);
        std::memset(b, fill, kRowBytes);
        PutLong(RowAt(y), generation);
        PutLong(RowAt(y) + 4, Va(b));
    };
    make_row(3, 7, 0x33);
    make_row(40, 2, 0x40);
    make_row(480, 9, 0xE0);
    PutLong(RowAt(41), 5);
    std::vector<unsigned char> record;
    bof3::SnapshotWriter w(record);
    Save(w);

    // The live moment: row 40's block freed, 41 made, 3 and 480 rewritten,
    // the generations moved on; three surfaces in the caches; the kept words
    // other than the take's.
    Crt_free(Object(RowAt(40) + 4));
    PutLong(RowAt(40) + 4, 0);
    make_row(41, 6, 0x41);
    std::memset(At(Long(RowAt(3) + 4)), 0x99, kRowBytes);
    std::memset(At(Long(RowAt(480) + 4)), 0x98, kRowBytes);
    PutLong(RowAt(3), 8);
    const U block3 = Long(RowAt(3) + 4);
    render::Surface* page = render::MakeSurface(256, 256, 16, 0x1000, 0, false, false);
    render::Surface* glyph = render::MakeSurface(32, 32, 16, 0x1000, 0, false, false);
    render::Surface* cell = render::MakeSurface(64, 64, 16, 0x1000, 0, false, false);
    Check(page && glyph && cell, "three stand-in surfaces made");
    if (!page || !glyph || !cell) Fatal("gfx_snapshot self-test: no surfaces");
    page->refs = glyph->refs = cell->refs = 2;   // the surface and its texture interface, as a cache holds them
    At(TexEntry(5, 0))[0] = 1;
    PutLong(TexEntry(5, 0) + 0x10, Va(page));
    PutLong(TexEntry(5, 0) + 0x14, Va(page) + 4);
    PutLong(FontEntry(9) + 8, Va(glyph));
    PutLong(FontEntry(9) + 0xC, Va(glyph) + 4);
    PutLong(CellEntry(77) + 0x20, Va(cell));
    PutLong(CellEntry(77) + 0x24, Va(cell) + 4);
    for (std::size_t i = 0; i < kKeptCount; ++i) std::memset(At(kKept[i].at), 0x5A, kKept[i].bytes);
    std::vector<std::vector<unsigned char>> kept_live(kKeptCount);
    for (std::size_t i = 0; i < kKeptCount; ++i) kept_live[i].assign(At(kKept[i].at), At(kKept[i].at + kKept[i].bytes));

    // The restore as the core runs it: the before step on the live words,
    // the span (here: the caches' words as they stand, and the kept words
    // scribbled as a span from another moment would leave them), the record.
    const U deferred0 = render::DeferredReleases();
    Before();
    Check(render::DeferredReleases() == deferred0 + 6, "the before step defers the six objects the caches hold");
    Check(page->refs == 2 && glyph->refs == 2 && cell->refs == 2, "nothing released before the present");
    for (std::size_t i = 0; i < kKeptCount; ++i) std::memset(At(kKept[i].at), 0xC3, kKept[i].bytes);
    PutLong(FontEntry(9) + 8, 0);   // the take's font entry: no surface (no glyph table here to build one)
    PutLong(FontEntry(9) + 0xC, 0);
    bof3::SnapshotReader r("GFX", record.data(), record.size());
    Restore(r);
    Check(r.Left() == 0, "the record read to its end");
    Check(Long(RowAt(3)) == 7 && Long(RowAt(40)) == 2 && Long(RowAt(480)) == 9 && Long(RowAt(41)) == 5,
          "every generation as at the take");
    Check(Long(RowAt(3) + 4) == block3, "a row with a block then and now keeps the live block");
    bool pixels = Long(RowAt(3) + 4) && Long(RowAt(40) + 4) && Long(RowAt(480) + 4);
    if (pixels)
        for (U k = 0; k < kRowBytes; ++k)
            pixels = pixels && At(Long(RowAt(3) + 4))[k] == 0x33 && At(Long(RowAt(40) + 4))[k] == 0x40 &&
                     At(Long(RowAt(480) + 4))[k] == 0xE0;
    Check(pixels, "the take's converted pixels in rows 3, 40 (made again) and 480");
    Check(Long(RowAt(41) + 4) == 0, "a row made since the take is freed: converted on its first use again");
    bool empty = true;
    for (U k = 0; k < Gfx_TexCache_count; ++k) empty = empty && Gfx_TexCache[k] == 0;
    for (U k = 0; k < D3d_CellTexCache_count; ++k) empty = empty && D3d_CellTexCache[k] == 0;
    Check(empty, "Gfx_TexCache and D3d_CellTexCache empty");
    bool kept = true;
    for (std::size_t i = 0; i < kKeptCount; ++i) kept = kept && std::memcmp(At(kKept[i].at), kept_live[i].data(), kKept[i].bytes) == 0;
    Check(kept, "the display words keep the live process's values");
    Check(page->refs == 2 && glyph->refs == 2 && cell->refs == 2, "still nothing released before the present");
    render::ReleaseDeferredNow();
    Check(page->refs == 0 && glyph->refs == 0 && cell->refs == 0 && !page->pixels,
          "after the present every object the live caches held is released, once each");
    Check(render::DeferredReleases() == 0, "nothing left deferred");
    // A second record of the restored state equals the first: every section
    // puts back what it saved.
    std::vector<unsigned char> again;
    bof3::SnapshotWriter w2(again);
    Save(w2);
    Check(again == record, "a take after the restore writes the same record");

    // The game's words as they were.
    for (U y = 0; y < kRows; ++y)
        if (void* b = Object(RowAt(y) + 4)) Crt_free(b);
    std::memcpy(At(RowAt(0)), rows0.data(), rows0.size());
    std::memcpy(Gfx_TexCache, tex0.data(), tex0.size());
    std::memcpy(Font_TexCache, font0.data(), font0.size());
    std::memcpy(D3d_CellTexCache, cell0.data(), cell0.size());
    for (std::size_t i = 0; i < kKeptCount; ++i) std::memcpy(At(kKept[i].at), kept0[i].data(), kKept[i].bytes);
    if (g_fails) Fatal("gfx_snapshot self-test: %d check(s) failed (the log has each)", g_fails);
    Log("shadow      gfx_snapshot: self-test passed - palette rows as at the take (a block made again, one freed, "
        "pixels and generations back), three cache objects released after the present and not before, the page and cell "
        "caches emptied, %u display ranges kept live, a %u-byte record read to its end and written again the same",
        static_cast<unsigned>(kKeptCount), static_cast<unsigned>(record.size()));
}

}  // namespace

void GfxSnapshot_Inject() {
    // The kept ranges against symbols.toml's names where it has them.
    struct Named {
        U at, symbol;
    };
    const Named named[] = {
        {0x66B710, Va(Gfx_ScreenRect) + 8}, {0x6BEA00, Va(&Dd_CellBackdrop)},      {0x6C3A40, Va(&D3d_BackMaterialHandle)},
        {0x6C3A48, Va(&Gfx_RenderFlags) - 4}, {0x7C9F44, Va(&D3d_ScaleY) - 4},      {0x7CADF8, Va(&D3d_CaptureSurface)},
        {0x7CC338, Va(&DDraw_Primary)},       {0x7CC354, Va(&D3d_Viewport)},        {0x7DED5C, Va(Gfx_PixelFormat) - 4},
    };
    for (const Named& n : named) {
        std::size_t i = 0;
        while (i < kKeptCount && kKept[i].at != n.at) ++i;
        if (i == kKeptCount || n.at != n.symbol) Fatal("gfx_snapshot: kept range 0x%08X, symbols.toml says 0x%08X", n.at, n.symbol);
    }
    if (Va(&D3d_ShadeModeCache) != 0x6C3A44) Fatal("gfx_snapshot: D3d_ShadeModeCache moved");
    if (bof3::IsExeHost() ? GetEnvironmentVariableA("BOF3X_SELFTEST_ONLY", nullptr, 0) > 0 : bof3::WantsShadow("gfx_snapshot"))
        SelfTest();
    bof3::Snapshot_Register("GFX", &Save, &Restore);
    bof3::Snapshot_RegisterBeforeRestore("GFX", &Before);
}

#include "game/gfx_flush.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "hook/detour.h"
#include "hook/log.h"

// The two things WinMain's loop does to VRAM on a RENDERED frame, before the
// draw (docs/call-trace.md section 6): game logic only queues and marks.

// original 0x454960. The CLUT strip - VRAM rows 480..511, 256 cells wide - is
// kept by game logic in the DAT arena and marked dirty; here it goes up,
// row by changed row.
extern "C" void __cdecl Gfx_FlushDirtyStrip(void) {
    if (Gfx_ClutStripDirty == 0) return;
    const short rect[4] = {0, 0x1E0, 0x100, 0x20};
    Gfx_LoadImageIfChanged(rect, Gfx_ClutStrip);
    Gfx_ClutStripDirty = 0;
}

// original 0x461F00. Each queued record: s16 w, s16 h, u32 kind, then data.
// Kind 1 is 5-bit packed, kind 2 LZSS, anything else raw cells.
//
// As the original has it: the index is a byte; the count is read again after
// every record rather than once; and nothing bounds either against the 20
// slots the arrays have (docs/known-defects.md D4 - DIV-0004 is the fix, and
// it is in Gfx_BeginFrame, not here).
extern "C" void __cdecl Gfx_FlushUploadQueue(void) {
    for (std::uint8_t i = 0; i < Gfx_UploadQueueCount; ++i) {
        const unsigned char* record = Gfx_UploadQueueRecord[i];
        short rect[4];
        rect[0] = static_cast<short>(Gfx_UploadQueueX[i]);
        rect[1] = static_cast<short>(Gfx_UploadQueueY[i]);
        std::memcpy(&rect[2], record, 4);
        std::uint32_t kind;
        std::memcpy(&kind, record + 4, 4);
        if (kind == 1) Gfx_UploadPacked5(record + 8, rect);
        else if (kind == 2) Gfx_UploadLzss(record + 8, rect);
        else Gfx_LoadImage(rect, record + 8);
    }
    Gfx_UploadQueueCount = 0;
}

// --- BOF3X_SHADOW=gfx_flush: start-up differential fuzz against clones ----------
// (docs/SCAFFOLDING.md section 2.) Here the clones' calls are re-aimed at the
// ORIGINAL addresses, so both sides reach the same callees - ours where we own
// them - and the only thing that differs is the function under test.

namespace {

using Fn = void (__cdecl*)();

std::uint32_t g_rng = 0xC0FFEE11u;
std::uint32_t Rng() {
    g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5;
    return g_rng;
}
int RngIn(int lo, int hi) { return lo + static_cast<int>(Rng() % static_cast<unsigned>(hi - lo + 1)); }

constexpr int kShadowRows = 64;                       // uploads are kept inside these
constexpr unsigned kShadowBytes = kShadowRows * 0x800;
constexpr unsigned kScratchBytes = 0x8000;
std::uint8_t g_records[20][0x800];
std::uint8_t g_shadow_theirs[kShadowBytes], g_scratch_theirs[kScratchBytes];

// A VALID stream for Gfx_UploadLzss, of at least `out` output bytes. Random
// bytes will not do: the decoder zeroes 0x1EE of its 512 window bytes and
// leaves the last 18 as whatever the stack held, so a match that reads those
// before they are written copies garbage - different on every call, the
// original against itself included (found by this fuzz, 2026-09-19). An
// encoder never emits one. Matches here stay below 0x1EE - 18, which is always
// either the zeroes or bytes already written.
void WriteLzss(std::uint8_t* at, int out) {
    while (out > 0) {
        std::uint8_t* flags = at++;
        *flags = 0;
        for (int bit = 0; bit < 8; ++bit) {
            if (Rng() % 3) {
                *flags |= static_cast<std::uint8_t>(1u << bit);
                *at++ = static_cast<std::uint8_t>(Rng());
                out -= 1;
            } else {
                const unsigned offset = Rng() % (0x1EE - 18), len = Rng() % 16;
                *at++ = static_cast<std::uint8_t>(offset);
                *at++ = static_cast<std::uint8_t>((offset >> 4 & 0xF0) | len);
                out -= static_cast<int>(len) + 3;
            }
        }
    }
}

unsigned FuzzQueue(Fn theirs) {
    constexpr unsigned kRounds = 1500;
    auto* shadow = reinterpret_cast<std::uint8_t*>(Gfx_VramShadow);
    unsigned bad = 0, kinds[4] = {};
    for (unsigned round = 0; round < kRounds; ++round) {
        const int count = RngIn(0, 20);
        for (int i = 0; i < count; ++i) {
            for (auto& b : g_records[i]) b = static_cast<std::uint8_t>(Rng());
            // mostly inside the shadow; one in eight past its right edge, which
            // Gfx_LoadImage drops
            const short w = static_cast<short>(RngIn(0, 12)), h = static_cast<short>(RngIn(0, 12));
            const std::uint32_t kind = Rng() % 8 == 0 ? Rng() : static_cast<std::uint32_t>(RngIn(0, 3));
            std::memcpy(g_records[i], &w, 2);
            std::memcpy(g_records[i] + 2, &h, 2);
            std::memcpy(g_records[i] + 4, &kind, 4);
            if (kind == 2) WriteLzss(g_records[i] + 8 + 4, w * h * 2 + 18);
            ++kinds[kind == 1 ? 1 : kind == 2 ? 2 : kind == 0 ? 0 : 3];
            Gfx_UploadQueueX[i] = static_cast<unsigned short>(Rng() % 8 == 0 ? RngIn(1020, 1040) : RngIn(0, 1000));
            Gfx_UploadQueueY[i] = static_cast<unsigned short>(RngIn(0, kShadowRows - 12));
            Gfx_UploadQueueRecord[i] = g_records[i];
        }
        unsigned char* next_theirs = nullptr;
        for (int pass = 0; pass < 2; ++pass) {
            std::memset(shadow, 0, kShadowBytes);
            std::memset(Gfx_UnpackScratch, 0, kScratchBytes);
            Gfx_UnpackNext = Gfx_UnpackScratch;
            Gfx_UploadQueueCount = static_cast<unsigned char>(count);
            (pass ? &Gfx_FlushUploadQueue : theirs)();
            if (pass == 0) {
                std::memcpy(g_shadow_theirs, shadow, kShadowBytes);
                std::memcpy(g_scratch_theirs, Gfx_UnpackScratch, kScratchBytes);
                next_theirs = Gfx_UnpackNext;
                if (Gfx_UploadQueueCount != 0) ++bad;
            }
        }
        if (std::memcmp(g_shadow_theirs, shadow, kShadowBytes) != 0 ||
            std::memcmp(g_scratch_theirs, Gfx_UnpackScratch, kScratchBytes) != 0 ||
            next_theirs != Gfx_UnpackNext || Gfx_UploadQueueCount != 0) {
            if (++bad <= 8) {
                // which record's rect holds the first differing cell, and its kind
                unsigned at = 0;
                while (at < kShadowBytes && g_shadow_theirs[at] == shadow[at]) ++at;
                const int cx = static_cast<int>(at % 0x800) / 2, cy = static_cast<int>(at / 0x800);
                std::uint32_t kind = 0xFFFFFFFF;
                for (int i = 0; i < count; ++i) {
                    short w, h;
                    std::memcpy(&w, g_records[i], 2);
                    std::memcpy(&h, g_records[i] + 2, 2);
                    if (cx >= Gfx_UploadQueueX[i] && cx < Gfx_UploadQueueX[i] + w && cy >= Gfx_UploadQueueY[i] &&
                        cy < Gfx_UploadQueueY[i] + h)
                        std::memcpy(&kind, g_records[i] + 4, 4);   // the last one to cover it wins, as in the flush
                }
                bof3::Log("shadow      Gfx_FlushUploadQueue self-test MISMATCH round %u, %d records; first differing "
                          "cell (%d,%d) %s, last written by a record of kind %u", round, count, cx, cy,
                          at < kShadowBytes ? "in the shadow" : "- none, the scratch differs", (unsigned)kind);
            }
        }
    }
    std::memset(shadow, 0, kShadowBytes);
    std::memset(Gfx_UnpackScratch, 0, kScratchBytes);
    Gfx_UnpackNext = nullptr;
    for (int i = 0; i < 20; ++i) { Gfx_UploadQueueX[i] = 0; Gfx_UploadQueueY[i] = 0; Gfx_UploadQueueRecord[i] = nullptr; }
    bof3::Log("shadow      Gfx_FlushUploadQueue self-test: %u rounds, records of kind 0 / 1 / 2 / other: %u / %u / %u / %u, "
              "%u MISMATCHES; shadow rows, unpack scratch, Gfx_UnpackNext and the count compared",
              kRounds, kinds[0], kinds[1], kinds[2], kinds[3], bad);
    return bad;
}

// Rows 480..511 need converted-row buffers of ours: the game's heap does not
// exist yet, and Gfx_ConvertRow would ask it for one.
std::uint8_t g_row_buffers[32][0x1000], g_row_buffers_theirs[32][0x1000];
std::uint16_t g_strip_rows_in[32][1024], g_strip_rows_theirs[32][1024];

unsigned FuzzStrip(Fn theirs) {
    constexpr unsigned kRounds = 300;
    struct Row { std::uint32_t generation; void* pixels; };
    auto* rows = reinterpret_cast<Row*>(Gfx_ClutRows) + 480;
    const unsigned long format_8888[8] = {0x04000000, 0, 8, 16, 0xFF0000, 0x00FF00, 0x0000FF, 0xFF000000};
    std::memcpy(Gfx_PixelFormat, format_8888, sizeof format_8888);

    unsigned bad = 0, dirty_rounds = 0;
    for (unsigned round = 0; round < kRounds; ++round) {
        // most of the strip as the shadow already has it, some rows changed
        for (int r = 0; r < 32; ++r) {
            const bool change = Rng() % 3 == 0;
            for (int c = 0; c < 256; ++c) {
                const auto cell = static_cast<std::uint16_t>(Rng());
                Gfx_ClutStrip[r * 256 + c] = cell;
                Gfx_VramShadow[((480 + r) << 10) + c] = change && c == 17 ? static_cast<std::uint16_t>(~cell) : cell;
            }
        }
        const unsigned char dirty = Rng() % 4 ? 1 : 0;
        dirty_rounds += dirty;
        std::uint32_t gen_theirs[32];
        for (int r = 0; r < 32; ++r)
            std::memcpy(g_strip_rows_in[r], &Gfx_VramShadow[(480 + r) << 10], sizeof g_strip_rows_in[r]);
        for (int pass = 0; pass < 2; ++pass) {
            for (int r = 0; r < 32; ++r) {
                std::memset(g_row_buffers[r], 0xA5, sizeof g_row_buffers[r]);
                rows[r] = {static_cast<std::uint32_t>(r), g_row_buffers[r]};
                std::memcpy(&Gfx_VramShadow[(480 + r) << 10], g_strip_rows_in[r], sizeof g_strip_rows_in[r]);
            }
            Gfx_ClutStripDirty = dirty;
            (pass ? &Gfx_FlushDirtyStrip : theirs)();
            if (pass == 0) {
                std::memcpy(g_row_buffers_theirs, g_row_buffers, sizeof g_row_buffers);
                for (int r = 0; r < 32; ++r) {
                    gen_theirs[r] = rows[r].generation;
                    std::memcpy(g_strip_rows_theirs[r], &Gfx_VramShadow[(480 + r) << 10], sizeof g_strip_rows_theirs[r]);
                }
                if (Gfx_ClutStripDirty != 0) ++bad;
            }
        }
        bool same = Gfx_ClutStripDirty == 0 && std::memcmp(g_row_buffers_theirs, g_row_buffers, sizeof g_row_buffers) == 0;
        for (int r = 0; r < 32 && same; ++r)
            same = gen_theirs[r] == rows[r].generation &&
                   std::memcmp(g_strip_rows_theirs[r], &Gfx_VramShadow[(480 + r) << 10], sizeof g_strip_rows_theirs[r]) == 0;
        if (!same && ++bad <= 8) bof3::Log("shadow      Gfx_FlushDirtyStrip self-test MISMATCH round %u, dirty %u", round, dirty);
    }
    for (int r = 0; r < 32; ++r) {
        rows[r] = {0, nullptr};
        std::memset(&Gfx_VramShadow[(480 + r) << 10], 0, 2048);
    }
    std::memset(Gfx_ClutStrip, 0, sizeof(unsigned short) * Gfx_ClutStrip_count);
    std::memset(Gfx_PixelFormat, 0, sizeof format_8888);
    Gfx_ClutStripDirty = 0;
    bof3::Log("shadow      Gfx_FlushDirtyStrip self-test: %u rounds (%u dirty), %u MISMATCHES; "
              "converted rows, generations, the shadow strip and the flag compared", kRounds, dirty_rounds, bad);
    return bad;
}

}  // namespace

void GfxFlush_Inject() {
    if (bof3::WantsShadow("gfx_flush")) {
        // Disasm 2026-09-19: every jump in both is internal. Relative calls:
        // three in the queue flush, one in the strip flush.
        const bof3::CloneCall queue_calls[] = {{0x67, nullptr}, {0x7C, nullptr}, {0x8C, nullptr}};
        auto* queue = bof3::CloneOriginal("Gfx_FlushUploadQueue", bof3::addr::Gfx_FlushUploadQueue, 0xB3, queue_calls, 3);
        const bof3::CloneCall strip_calls[] = {{0x32, nullptr}};
        auto* strip = bof3::CloneOriginal("Gfx_FlushDirtyStrip", bof3::addr::Gfx_FlushDirtyStrip, 0x45, strip_calls, 1);
        const unsigned bad = FuzzQueue(reinterpret_cast<Fn>(queue)) + FuzzStrip(reinterpret_cast<Fn>(strip));
        if (bad) bof3::Fatal("gfx_flush differs from the original in %u self-test rounds", bad);
    }
    BOF3_INJECT(Gfx_FlushDirtyStrip);
    BOF3_INJECT(Gfx_FlushUploadQueue);
}

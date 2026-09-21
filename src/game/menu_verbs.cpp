// DIVERGENCE DIV-0018: the menu's short verbs from a language overlay.
//
// The buttons above a menu panel - the Config screen's two, and the rows of
// three other menu screens - are drawn by 0x574890 (read 2026-09-21,
// docs/config-screen.md section 8). It takes a set number; the sets are 5-byte
// records at 0x66383C, a count and up to four verb indices; a verb index goes
// through the pointer table 0x6637E4 to one of 22 NUL-padded 8-byte slots at
// 0x66A228. Each label is drawn through Text_DrawAt, centred in its button as
// `x0 + 0x16 + 48 * i - 6 * n`, with n its character count from 0x57D800 -
// half of 12 units a character.
//
// The strings are written in place: the US disc's verbs are at most four
// letters, one byte each in the single-byte slots the English overlay paints
// with the dialogue font (tools/loc_build.py), so they fit the slots with room
// to spare. And the one Text_DrawAt call in the row draw is re-aimed at a
// stand-in that moves the label by the difference between the width the
// original reckoned and the width the pen will really cover - which is zero
// for any string of 12-unit glyphs, so shipped Chinese text is placed exactly
// as before.
#include "game/menu_verbs.h"

#include <windows.h>

#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/text_advance.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

constexpr std::uint32_t kVerbs = 22;
constexpr std::uint32_t kPointers = 0x6637E4;
constexpr std::uint32_t kSlots = 0x66A228;
constexpr std::uint32_t kSlotRoom = 8;

constexpr std::uint32_t kTextDrawAt = 0x516B30;   // its name is a macro here
constexpr std::uint32_t kRowLabelCall = 0x57499B; // 0x574890: the label draw

// The number of characters 0x57D800 counts - a byte below 0x80 is one, a byte
// with bit 7 and the one after it are one - over at most 16 bytes, as it does;
// and the width the pen covers drawing them.
void Measure(const unsigned char* text, int& chars, int& width) {
    chars = width = 0;
    for (int bytes = 0; bytes < 0x10 && *text;) {
        ++chars;
        width += TextAdvance_Of(text);
        const int step = (*text & 0x80) ? 2 : 1;
        text += step;
        bytes += step;
    }
}

extern "C" const unsigned char* __cdecl MenuVerbs_DrawLabel(int x, int y, int color, int count,
                                                            const unsigned char* text) {
    int chars, width;
    Measure(text, chars, width);
    // The original subtracted 6 * chars to centre; centre on the real width.
    return Text_DrawAt(x + 6 * chars - width / 2, y, color, count, text);
}

}  // namespace

void MenuVerbs_Apply(std::uint32_t tag, const std::uint8_t* payload, std::uint32_t size) {
    if (tag != 0) bof3::Fatal("verb chunk tag is 0x%X, expected 0", (unsigned)tag);
    const std::uint8_t* p = payload;
    const std::uint8_t* const end = payload + size;
    if (p >= end || *p++ != kVerbs) bof3::Fatal("verb chunk: count is not %u", (unsigned)kVerbs);
    for (std::uint32_t i = 0; i < kVerbs; ++i) {
        const auto* s = reinterpret_cast<const char*>(p);
        while (p < end && *p) ++p;
        if (p >= end) bof3::Fatal("verb chunk: ran out inside verb %u", (unsigned)i);
        ++p;
        const std::size_t len = std::strlen(s);
        if (len == 0 || len + 1 > kSlotRoom)
            bof3::Fatal("verb chunk: verb %u is %u bytes, the slot holds %u", (unsigned)i, (unsigned)len + 1,
                        (unsigned)kSlotRoom);
        // Every write is into BOF3.exe's .data, so check first that the slot
        // is the one the pointer table names.
        const auto* entry = reinterpret_cast<const std::uint32_t*>(
            static_cast<std::uintptr_t>(kPointers + i * sizeof(std::uint32_t)));
        if (*entry != kSlots + i * kSlotRoom)
            bof3::Fatal("verbs: pointer %u holds 0x%08X, expected 0x%08X", (unsigned)i, (unsigned)*entry,
                        (unsigned)(kSlots + i * kSlotRoom));
        auto* slot = reinterpret_cast<char*>(static_cast<std::uintptr_t>(kSlots + i * kSlotRoom));
        std::memset(slot, 0, kSlotRoom);
        std::memcpy(slot, s, len);
    }
    if (p != end) bof3::Fatal("verb chunk: %u bytes left over", (unsigned)(end - p));
    bof3::Log("DIV-0018: %u menu verbs", (unsigned)kVerbs);
}

void MenuVerbs_Inject() {
    char lang[16];
    if (GetEnvironmentVariableA("BOF3X_LANG", lang, sizeof lang) == 0) return;
    bof3::RetargetCall("MenuVerbs", kRowLabelCall, kTextDrawAt, reinterpret_cast<void*>(&MenuVerbs_DrawLabel));
}

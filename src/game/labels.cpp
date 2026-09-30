// DIVERGENCE DIV-0064: the short labels from a language overlay.
//
// Five groups of NUL-padded slots in BOF3.exe's .data, each of which the US
// disc carries beside bytes the PC still has (read 2026-09-29,
// docs/dialogue-localisation.md section 8):
//
//   1  the status words, 2 x 8 at 0x66A0E8 - drawn by Text_DrawSmall after the
//      level in a member's panel (Menu_DrawMemberStatus, PartyForm_DrawReserve
//      and two more), their addresses immediates in the code;
//   2  the menu's stats, 4 x 8 at 0x66A0F8 - Text_DrawAt, immediates too;
//   3  the item types, 0x66A120: four slots of 8 and one of 12, behind the
//      pointer table 0x663970 (and its copies 0x663994, 0x66AF10, 0x66B58C,
//      0x66B5C4), the item lists' titles;
//   4  the skill types, 5 x 8 at 0x66A200 behind 0x663984 (the copy
//      0x66B5A0), the fifth behind 0x66B5B0 alone - the skill lists' titles;
//   5  the battle's stats, 4 x 8 at 0x669CF0 behind 0x64AE08.
//
// The strings are written in place, one byte a letter in the single-byte
// slots the overlay paints with the dialogue font, so a draw through
// Text_DrawAt shows them as it shows every other overlay string. The status
// words go through the 8 px draw, which samples a whole glyph into an 8-unit
// quad: for them Text_DrawSmall asks Labels_SmallGlyph, and draws the
// overlay's 8 x 8 cell of the same letter. Two bytes a letter, as the Config
// screen's text has them, would not fit: four letters and a NUL are 9 bytes
// of a slot's 8.
#include "game/labels.h"

#include <cstring>

#include "hook/log.h"

namespace {

// A slot, and a place in the image that must hold its address: an entry of a
// pointer table, or the operand of a push. Every address here was read, not
// guessed; a chunk may only write the slots listed in this file.
struct Slot {
    std::uint32_t va, room, named_at;
};

struct Table {
    std::uint32_t tag;
    const char* what;
    std::uint32_t count;
    Slot slots[5];
};

constexpr std::uint32_t kStatusSlots = 0x66A0E8, kStatusEnd = 0x66A0F8;
constexpr std::uint32_t kStatusTag = 1;

constexpr Table kTables[] = {
    {kStatusTag, "status words", 2, {{0x66A0E8, 8, 0x573661}, {0x66A0F0, 8, 0x57368F}}},
    {2, "menu stats", 4, {{0x66A0F8, 8, 0x5738CD}, {0x66A100, 8, 0x573928}, {0x66A108, 8, 0x573965},
                          {0x66A110, 8, 0x5739AB}}},
    {3, "item types", 5, {{0x66A120, 8, 0x663970}, {0x66A128, 8, 0x663974}, {0x66A130, 8, 0x663978},
                          {0x66A138, 8, 0x66397C}, {0x66A140, 12, 0x663980}}},
    {4, "skill types", 5, {{0x66A200, 8, 0x663984}, {0x66A208, 8, 0x663988}, {0x66A210, 8, 0x66398C},
                           {0x66A218, 8, 0x663990}, {0x66A220, 8, 0x66B5B0}}},
    {5, "battle stats", 4, {{0x669CF0, 8, 0x64AE08}, {0x669CF8, 8, 0x64AE0C}, {0x669D00, 8, 0x64AE10},
                            {0x669D08, 8, 0x64AE14}}},
};

// The overlay's 8 x 8 set: the US disc's code - 0x30 from here, and a letter
// or a digit has its ASCII byte for a code (tools/loc_build.py,
// SMALL_APPEND_AT).
constexpr unsigned kSmallSet = 0xA00;

bool g_status_written = false;

}  // namespace

void Labels_Apply(std::uint32_t tag, const std::uint8_t* payload, std::uint32_t size) {
    const Table* t = nullptr;
    for (const Table& k : kTables)
        if (k.tag == tag) t = &k;
    if (!t) bof3::Fatal("labels chunk tag is 0x%X, no table of that number", (unsigned)tag);
    const std::uint8_t* p = payload;
    const std::uint8_t* const end = payload + size;
    if (p >= end || *p++ != t->count) bof3::Fatal("%s chunk: count is not %u", t->what, (unsigned)t->count);
    std::uint32_t written = 0;
    for (std::uint32_t i = 0; i < t->count; ++i) {
        const Slot& slot = t->slots[i];
        const auto* s = reinterpret_cast<const char*>(p);
        while (p < end && *p) ++p;
        if (p >= end) bof3::Fatal("%s chunk: ran out inside string %u", t->what, (unsigned)i);
        ++p;
        const std::size_t len = std::strlen(s);
        if (len + 1 > slot.room)
            bof3::Fatal("%s chunk: string %u is %u bytes, the slot holds %u", t->what, (unsigned)i,
                        (unsigned)len + 1, (unsigned)slot.room);
        // Every write is into BOF3.exe's .data, so check first that the slot
        // is the one the image names there.
        std::uint32_t named;
        std::memcpy(&named, reinterpret_cast<const void*>(static_cast<std::uintptr_t>(slot.named_at)), sizeof named);
        if (named != slot.va)
            bof3::Fatal("%s: 0x%08X holds 0x%08X, expected 0x%08X", t->what, (unsigned)slot.named_at,
                        (unsigned)named, (unsigned)slot.va);
        if (len == 0) continue;   // the overlay's builder had no string that fits: as shipped
        auto* dest = reinterpret_cast<char*>(static_cast<std::uintptr_t>(slot.va));
        std::memset(dest, 0, slot.room);
        std::memcpy(dest, s, len);
        ++written;
    }
    if (p != end) bof3::Fatal("%s chunk: %u bytes left over", t->what, (unsigned)(end - p));
    if (tag == kStatusTag && written == t->count) g_status_written = true;
    bof3::Log("DIV-0064: %u of %u %s", (unsigned)written, (unsigned)t->count, t->what);
}

unsigned Labels_SmallGlyph(const unsigned char* text) {
    if (!g_status_written) return 0;
    const auto at = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(text));
    if (at < kStatusSlots || at >= kStatusEnd) return 0;
    const unsigned c = text[0];
    const bool plain = (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
    return plain ? kSmallSet + c - 0x30 : 0;
}

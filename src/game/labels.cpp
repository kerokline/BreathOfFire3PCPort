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
//      pointer table 0x663970 and its copies 0x663994, 0x66AF10, 0x66B58C,
//      0x66B5C4 - the item lists' titles;
//   4  the skill types, 5 x 8 at 0x66A200 behind 0x663984 and its copy
//      0x66B5A0, the fifth behind 0x66B5B0 alone - the skill lists' titles;
//   5  the battle's stats, 4 x 8 at 0x669CF0 behind 0x64AE08;
//   6  the camp's master list (2026-10-07; src/game/rest_2h.cpp's
//      MasterWin_DrawList): its title, 8 bytes at 0x66A1F0, and the mark
//      beside a completed master, 4 bytes at 0x66A2D8 - shipped as the one
//      byte `t`, whose single-byte slot of the shipped font holds a star,
//      which the overlay's repaint of that slot made a lowercase t (the
//      owner's cross, 2026-10-06); the disc's own strings (MSTR and its
//      star, SHOP.EMI), both immediates in the code.
//
// Groups 1, 2, 5 and 6 are written in place, one byte a letter in the
// single-byte slots the overlay paints with the dialogue font, so a draw
// through Text_DrawAt shows them as it shows every other overlay string.
// Groups 3 and 4 are reached by nothing but their pointer tables (a scan of
// the image for each slot's address, 2026-09-29), so their strings go into
// buffers of ours, 16 bytes each, and every table is re-aimed: the French
// disc's ARMEMENT and CAPACITE and the German RUESTUNG are eight letters, one
// over an 8-byte slot (the owner's question, 2026-09-29). The status words go
// through the 8 px draw, which samples a whole glyph into an 8-unit quad: for
// them Text_DrawSmall asks Labels_SmallGlyph, and draws the overlay's 8 x 8
// cell of the same letter. Two bytes a letter, as the Config screen's text
// has them, would not fit: four letters and a NUL are 9 bytes of a slot's 8.
#include "game/labels.h"

#include <cstring>

#include "hook/log.h"

namespace {

// A slot, and a place in the image that must hold its address: an entry of a
// pointer table, or the operand of a push. Every address here was read, not
// guessed; a chunk may only write the slots and the tables listed in this
// file.
struct Slot {
    std::uint32_t va, room, named_at;
};

// A group whose strings are repointed: the pointer tables that name its
// slots, four bytes an entry, `count` entries from slot `first` (the skill
// types' fifth table holds the fifth alone).
struct PointerTable {
    std::uint32_t at, first, count;
};

constexpr std::uint32_t kRoom = 16;   // our buffers: a title of 15 letters, which the 0x99-wide box holds

struct Table {
    std::uint32_t tag;
    const char* what;
    std::uint32_t count;
    Slot slots[5];
    PointerTable tables[5];   // none for a group written in place
    char (*buffers)[kRoom];   // ours, for a repointed group
};

char g_item_types[5][kRoom], g_skill_types[5][kRoom];

constexpr std::uint32_t kStatusSlots = 0x66A0E8, kStatusEnd = 0x66A0F8;
constexpr std::uint32_t kStatusTag = 1;

constexpr Table kTables[] = {
    {kStatusTag, "status words", 2, {{0x66A0E8, 8, 0x573661}, {0x66A0F0, 8, 0x57368F}}, {}, nullptr},
    {2, "menu stats", 4, {{0x66A0F8, 8, 0x5738CD}, {0x66A100, 8, 0x573928}, {0x66A108, 8, 0x573965},
                          {0x66A110, 8, 0x5739AB}}, {}, nullptr},
    {3, "item types", 5, {{0x66A120, 8, 0x663970}, {0x66A128, 8, 0x663974}, {0x66A130, 8, 0x663978},
                          {0x66A138, 8, 0x66397C}, {0x66A140, 12, 0x663980}},
     {{0x663970, 0, 5}, {0x663994, 0, 5}, {0x66AF10, 0, 5}, {0x66B58C, 0, 5}, {0x66B5C4, 0, 5}}, g_item_types},
    {4, "skill types", 5, {{0x66A200, 8, 0x663984}, {0x66A208, 8, 0x663988}, {0x66A210, 8, 0x66398C},
                           {0x66A218, 8, 0x663990}, {0x66A220, 8, 0x66B5B0}},
     {{0x663984, 0, 4}, {0x66B5A0, 0, 4}, {0x66B5B0, 4, 1}}, g_skill_types},
    {5, "battle stats", 4, {{0x669CF0, 8, 0x64AE08}, {0x669CF8, 8, 0x64AE0C}, {0x669D00, 8, 0x64AE10},
                            {0x669D08, 8, 0x64AE14}}, {}, nullptr},
    {6, "master list", 2, {{0x66A1F0, 8, 0x59C5B6}, {0x66A2D8, 4, 0x59C465}}, {}, nullptr},
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
        const std::uint32_t room = t->buffers ? kRoom : slot.room;
        if (len + 1 > room)
            bof3::Fatal("%s chunk: string %u is %u bytes, the slot holds %u", t->what, (unsigned)i,
                        (unsigned)len + 1, (unsigned)room);
        // Every write is into BOF3.exe's .data, so check first that the slot
        // is the one the image names there.
        // For a repointed group the witness is its first pointer table's
        // entry, which an earlier load of the overlay re-aimed at our
        // buffer: the title's FIRST.DAT is loaded again after a game over
        // (the owner's gameover recipe, 2026-09-30: the second load aborted
        // here), so that address is the slot's too.
        std::uint32_t named;
        std::memcpy(&named, reinterpret_cast<const void*>(static_cast<std::uintptr_t>(slot.named_at)), sizeof named);
        const auto ours = t->buffers ? static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(t->buffers[i])) : slot.va;
        if (named != slot.va && named != ours)
            bof3::Fatal("%s: 0x%08X holds 0x%08X, expected 0x%08X", t->what, (unsigned)slot.named_at,
                        (unsigned)named, (unsigned)slot.va);
        if (len == 0) continue;   // the overlay's builder had no string that fits: as shipped
        auto* dest = t->buffers ? t->buffers[i] : reinterpret_cast<char*>(static_cast<std::uintptr_t>(slot.va));
        std::memset(dest, 0, room);
        std::memcpy(dest, s, len);
        ++written;
    }
    if (p != end) bof3::Fatal("%s chunk: %u bytes left over", t->what, (unsigned)(end - p));
    if (t->buffers) {
        // Every table entry must name the shipped slot (or our buffer, from
        // an earlier load of the overlay) before it is re-aimed; a slot kept
        // as shipped keeps its entries.
        for (const PointerTable& table : t->tables) {
            for (std::uint32_t k = 0; k < table.count; ++k) {
                const std::uint32_t i = table.first + k;
                auto* entry = reinterpret_cast<std::uint32_t*>(static_cast<std::uintptr_t>(table.at + 4 * k));
                const auto ours = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(t->buffers[i]));
                if (*entry != t->slots[i].va && *entry != ours)
                    bof3::Fatal("%s: table 0x%08X entry %u holds 0x%08X, expected 0x%08X", t->what,
                                (unsigned)table.at, (unsigned)k, (unsigned)*entry, (unsigned)t->slots[i].va);
                if (t->buffers[i][0]) *entry = ours;
            }
        }
    }
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

// DIVERGENCE DIV-0052: the battle banner's words from a language overlay.
//
// The banner at the top of a fight shows either an actor's name or one of
// twelve short messages. BattleBanner_SetMessage 0x44A8E0 copies message k
// through the pointer table 0x669DE0 into the banner text 0x904EC0 with
// Str_CopyN(.., .., 8); BattleBanner_ShowName 0x44A990 copies a name and,
// while 0x904B7A is set, appends message 1 - the pointer at 0x669DE4, read by
// nothing else (pe_xref, 2026-09-24). Message 1 is " EX", the suffix an actor
// wears on an extra turn (owner's US PlayStation screenshot, "Momo EX"), and
// message 3 is a second suffix of the same shape. Both are a space and two
// picture glyphs, 0x8050 0x8051 and 0x8052 0x8053 - glyphs 0x50..0x53, which
// the English font paints over with `v` `w` `x` `y` (single bytes 0x76..0x79
// draw glyph b - 0x26). So under English the suffix drew "vw".
//
// The US BATTLE.EMI has the same twelve messages as 13-byte slots at
// 0x801EB000, copied with Str_CopyN(.., .., 12) (0x801DE988), in English:
// "Attack", " EX", "Examine", ..., "Lucky Strike", "Instant Kill", ...
// tools/loc_build.py carries them in a kind-12 chunk, the suffixes re-encoded
// to the glyphs it adds for them at 0xA6B..0xA6E (whose art each is: DIV-0052).
// The PC's own strings are packed 8 and 12 bytes apart, too small for "Lucky Strike", so
// they are not written in place: they live here, the table is repointed, and
// the copy grows to 12. The banner text has 32 bytes before the next global
// (0x904EE0, 68 references; nothing references 0x904EC4..0x904EDC), which
// holds 12 and a NUL, and 8 of a name with " EX".
#include "game/battle_text.h"

#include <cstring>

#include "hook/log.h"

namespace {

constexpr std::uint32_t kCount = 12;
constexpr std::uint32_t kTable = 0x669DE0;
constexpr std::uint32_t kRoom = 12;       // bytes a message; the US copy's count
constexpr std::uint32_t kPcRoom = 8;      // the PC's (0x44A8E4, push 8)

// Where each pointer of the shipped table points (read 2026-09-24): the
// strings are packed, 8 bytes apart but for string 9 (0x669DC4), 12.
constexpr std::uint32_t kShipped[kCount] = {0x669D7C, 0x669D84, 0x669D8C, 0x669D94, 0x669D9C, 0x669DA4,
                                            0x669DAC, 0x669DB4, 0x669DBC, 0x669DC4, 0x669DD0, 0x669DD8};

char g_messages[kCount][kRoom + 1];
std::uint32_t g_room = kPcRoom;

}  // namespace

void BattleMessages_Apply(std::uint32_t tag, const std::uint8_t* payload, std::uint32_t size) {
    if (tag != 0) bof3::Fatal("battle message chunk tag is 0x%X, expected 0", (unsigned)tag);
    auto* const table = reinterpret_cast<std::uint32_t*>(static_cast<std::uintptr_t>(kTable));
    const std::uint8_t* p = payload;
    const std::uint8_t* const end = payload + size;
    if (p >= end || *p++ != kCount) bof3::Fatal("battle message chunk: count is not %u", (unsigned)kCount);
    for (std::uint32_t i = 0; i < kCount; ++i) {
        const auto* s = reinterpret_cast<const char*>(p);
        while (p < end && *p) ++p;
        if (p >= end) bof3::Fatal("battle message chunk: ran out inside string %u", (unsigned)i);
        ++p;
        const std::size_t len = std::strlen(s);
        if (len == 0 || len > kRoom)
            bof3::Fatal("battle message %u is %u bytes, at most %u are copied", (unsigned)i, (unsigned)len,
                        (unsigned)kRoom);
        // Once repointed the table names ours; a second overlay walk may
        // apply the chunk again.
        const auto ours = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(g_messages[i]));
        if (table[i] != kShipped[i] && table[i] != ours)
            bof3::Fatal("battle messages: pointer %u holds 0x%08X, expected 0x%08X", (unsigned)i,
                        (unsigned)table[i], (unsigned)kShipped[i]);
        std::memset(g_messages[i], 0, sizeof g_messages[i]);
        std::memcpy(g_messages[i], s, len);
        table[i] = ours;
    }
    if (p != end) bof3::Fatal("battle message chunk: %u bytes left over", (unsigned)(end - p));
    g_room = kRoom;
    bof3::Log("DIV-0052: %u battle messages", (unsigned)kCount);
}

std::uint32_t BattleMessages_CopyRoom() { return g_room; }

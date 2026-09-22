// DIVERGENCE DIV-0020: the characters' default names from a language overlay.
//
// New Game gives the party its names by copying records, not by any name
// entry (the port has none - docs/save-interchange.md section 2): 0x437820
// copies seven 0xA4-byte default records from 0x64B390 into the live table at
// 0x903A70 (`lea esi, [ebp + 0x64B390]`, 0x437834), then the eighth, the
// whelp's, from 0x64B80C into the slot 0x669736 names - 7 (0x437891). Each
// record starts with a 9-byte name field. Read 2026-09-21.
//
// So the English names go into those eight fields once, when FIRST.DAT loads,
// and New Game hands them on. Before any write, the two instructions that
// read the table are checked for the addresses above: if they are not the
// ones this file was written against, nothing is written.
//
// The whelp's name has one more home: 0x42E09D copies five bytes from the
// 8-byte slot 0x669CE0 into character 7's name at some event - a reset of the
// name to its default. It gets the same name, and at most five bytes of it
// reach the record, as the original's copy has it; no terminator is copied,
// which is right only while bytes 5..8 of the field are zero, as a default
// record leaves them.
//
// Not touched: saves. A loaded save brings its own names, so a game begun
// before the overlay keeps the Chinese ones - and one begun with it shows
// gibberish without it. Decided by the owner, 2026-09-21: acceptable until a
// language-independent name system exists.
//
// Also here: Manillo, the fish merchant (the owner's identification), whose
// name the PC keeps once in the 8-byte slot 0x669CD8. The battle copies 16
// bytes from it (0x52D1EC, combatant 0x16) and seven field functions do the
// same (0x401C88 and six more, `mov eax, 0x669CD8`), so the slot is read as a
// 16-byte name that ends at its NUL: "Manillo" and its NUL are exactly the
// 8 bytes, and what follows - the whelp's reset copy - is never reached.
#include "game/char_names.h"

#include <cstring>

#include "hook/log.h"

namespace {

constexpr std::uint32_t kCount = 8;
constexpr std::uint32_t kRecords = 0x64B390;
constexpr std::uint32_t kStride = 0xA4;
constexpr std::uint32_t kNameRoom = 9;
constexpr std::uint32_t kWhelpSlot = 0x669CE0;
constexpr std::uint32_t kWhelpSlotRoom = 8;

bool CodeReads(std::uint32_t at, const std::uint8_t* bytes, std::size_t n) {
    return std::memcmp(reinterpret_cast<const void*>(static_cast<std::uintptr_t>(at)), bytes, n) == 0;
}

}  // namespace

void CharNames_Apply(std::uint32_t tag, const std::uint8_t* payload, std::uint32_t size) {
    if (tag != 0) bof3::Fatal("character name chunk tag is 0x%X, expected 0", (unsigned)tag);

    // lea esi, [ebp + 0x64B390] / mov esi, 0x64B80C / push 0x669CE0
    static const std::uint8_t kLoop[] = {0x8D, 0xB5, 0x90, 0xB3, 0x64, 0x00};
    static const std::uint8_t kWhelp[] = {0xBE, 0x0C, 0xB8, 0x64, 0x00};
    static const std::uint8_t kReset[] = {0x68, 0xE0, 0x9C, 0x66, 0x00};
    if (!CodeReads(0x437834, kLoop, sizeof kLoop) || !CodeReads(0x437891, kWhelp, sizeof kWhelp) ||
        !CodeReads(0x42E09D, kReset, sizeof kReset))
        bof3::Fatal("character names: New Game does not read the records this build expects");
    static_assert(kRecords + 7 * kStride == 0x64B80C, "the whelp's record follows the seven");

    const std::uint8_t* p = payload;
    const std::uint8_t* const end = payload + size;
    if (p >= end || *p++ != kCount) bof3::Fatal("character name chunk: count is not %u", (unsigned)kCount);
    for (std::uint32_t i = 0; i < kCount; ++i) {
        const auto* s = reinterpret_cast<const char*>(p);
        while (p < end && *p) ++p;
        if (p >= end) bof3::Fatal("character name chunk: ran out inside name %u", (unsigned)i);
        ++p;
        const std::size_t len = std::strlen(s);
        if (len == 0 || len + 1 > kNameRoom)
            bof3::Fatal("character name %u is %u bytes, the field holds %u", (unsigned)i, (unsigned)len + 1,
                        (unsigned)kNameRoom);
        auto* field = reinterpret_cast<char*>(static_cast<std::uintptr_t>(kRecords + i * kStride));
        std::memset(field, 0, kNameRoom);
        std::memcpy(field, s, len);
        if (i == kCount - 1) {
            if (len + 1 > kWhelpSlotRoom)
                bof3::Fatal("the whelp's name is %u bytes, its reset slot holds %u", (unsigned)len + 1,
                            (unsigned)kWhelpSlotRoom);
            auto* slot = reinterpret_cast<char*>(static_cast<std::uintptr_t>(kWhelpSlot));
            std::memset(slot, 0, kWhelpSlotRoom);
            std::memcpy(slot, s, len);
        }
    }
    if (p != end) bof3::Fatal("character name chunk: %u bytes left over", (unsigned)(end - p));
    bof3::Log("DIV-0020: %u default character names", (unsigned)kCount);
}

void CharNames_ApplyMerchant(std::uint32_t tag, const std::uint8_t* payload, std::uint32_t size) {
    constexpr std::uint32_t kSlot = 0x669CD8, kRoom = 8;
    // mov ecx, dword ptr [0x669CD8] in the battle's copy
    static const std::uint8_t kBattleCopy[] = {0x8B, 0x0D, 0xD8, 0x9C, 0x66, 0x00};
    if (tag != 0) bof3::Fatal("merchant name chunk tag is 0x%X, expected 0", (unsigned)tag);
    if (!CodeReads(0x52D1EC, kBattleCopy, sizeof kBattleCopy))
        bof3::Fatal("merchant name: the battle does not read the slot this build expects");
    if (size < 2 || size > kRoom || payload[size - 1] != 0 || std::strlen(reinterpret_cast<const char*>(payload)) != size - 1)
        bof3::Fatal("merchant name chunk: %u bytes is not one name of at most %u with its NUL", (unsigned)size,
                    (unsigned)kRoom);
    auto* slot = reinterpret_cast<char*>(static_cast<std::uintptr_t>(kSlot));
    std::memset(slot, 0, kRoom);
    std::memcpy(slot, payload, size - 1);
    bof3::Log("DIV-0020: the merchant's name");
}

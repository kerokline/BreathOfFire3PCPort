// The save editor's PlayStation side (docs/save-editor-spec.md sections 5.2
// and 11; docs/save-editor.md section 7): the raw 128 KiB memory card image,
// its directory, the save inside one card block, and the one difference
// between the PlayStation's block and the PC's (the character name is 5 bytes
// there and 9 here, docs/save-interchange.md).
//
// The editor's Block is always the PC layout. A card save is changed into it
// when it is read and back when it is written, so the field table has one
// set of offsets. No window code here.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "launcher/save/block.h"

namespace bof3x::save {

constexpr std::size_t kCardSize = 0x20000;
constexpr int kCardSlots = 15;          // directory frames 1..15, one card block each
constexpr int kCardSavesPerRelease = 3; // the game lists the file names that end 00..02 only

using Card = std::vector<std::uint8_t>;   // kCardSize bytes

// The file names the game gives its saves: the prefix, then two digits.
extern const char kCardPrefixUs[];   // "BASLUS-00422BOF3"
extern const char kCardPrefixJp[];   // "BISLPS-00990BOF3"

struct CardEntry {
    int slot = 0;          // 1..15
    std::string name;      // the directory's file name
    bool bof3 = false;     // a save of this game: the name ends "BOF3" and two digits, in one card block
    std::string prefix;    // the name without the two digits, for a save of this game
    int number = -1;       // the two digits
};

// A card with no saves, as an emulator makes one.
Card EmptyCard();

// Refuses, with the reason, a file that is not a raw card image.
bool ReadCardFile(const std::wstring& path, Card& card, std::string& why);
// The whole image, by block.h's WriteKept: the file from before is kept.
bool WriteCardFile(const std::wstring& path, const Card& card, const std::wstring& backup_dir, std::wstring& backup,
                   std::string& why);

// The saves the directory holds, any game's. A deleted entry is not one.
std::vector<CardEntry> CardEntries(const Card& card);
// The first directory frame a new save can take, or 0.
int CardFreeSlot(const Card& card);
// The first of 00..02 that no save with this prefix has, or -1.
int CardFreeNumber(const Card& card, const std::string& prefix);

// The save in `slot`, in the PC layout. A wrong checksum is not a refusal.
bool CardGet(const Card& card, int slot, Block& block, std::string& why);
// Replaces the block of the save in `slot`. Only the block's bytes change,
// and the title when the play time or the level it shows changed.
bool CardPut(Card& card, int slot, const Block& block, std::string& why);
// A new save named `prefix` + `number`. `header`, when not null, is the 0x200
// bytes before the block of a save of this game (its icon goes with it);
// with none the save gets an icon of the editor's own. Returns the slot, or 0.
int CardAdd(Card& card, const std::string& prefix, int number, const Block& block, const std::uint8_t* header,
            std::string& why);
// The 0x200 bytes before the block of the save in `slot`.
const std::uint8_t* CardHeader(const Card& card, int slot);

// The two layouts. PsxToPc refuses a record that uses its last four bytes,
// which have no place in the PC record; PcToPsx refuses a name of more than
// five bytes. Neither changes the checksum's value: the bytes that move are
// the same bytes, and the bytes that go are zero.
bool PsxToPc(const std::uint8_t* psx, Block& pc, std::string& why);
bool PcToPsx(const Block& pc, std::uint8_t* psx, std::string& why);

// What a block made by the PC game loses on the way to a card (the spec's
// section 13): the two bytes this project added (DIV-0191, DIV-0228) and the two options no
// PC screen sets. Then the checksum.
void ClearPcOnly(Block& block);

}  // namespace bof3x::save

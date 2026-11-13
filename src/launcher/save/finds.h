// The names of the save's records-and-finds bits (docs/save-editor.md
// "Records and finds"), from the player's install: nothing here holds a value
// of the game's. The layouts are read from our own code (each cited where it
// is used); tools/bank_names.py is the same reader in Python.
//
// No window code and no file of the exe: launcher/exe_image.cpp hands these
// functions the image's bytes, save_dialog.cpp the files' paths, and the host
// tests (tools/launcher_host/save_tests.cpp) bytes of their own.
#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "launcher/save/block.h"

namespace bof3x::save {

// A find: a chest or field object (a content object's bit, +5 in the bank
// +0x6EC) or a passage entry (its bit, +4 in the bank +0x72C), where it is
// and what it gives.
struct Find {
    unsigned area;
    unsigned bit;
    unsigned x, z;        // the cell
    unsigned category;    // 0..4 an item category (Item_NamePtr's), 0xFF zenny
    unsigned value;       // the item id, or the zenny / 40
};

// `size` bytes at virtual address `va` of the image, or null.
using ImageAt = std::function<const unsigned char*(std::uint32_t va, std::uint32_t size)>;

// The op-9x placements (EventOp_9x 0x57B530, 13 bytes) in one event script
// at `start`, on every path the script could take: the structural walk of
// tools/event_scan.py, which visits both arms of every branch and evaluates
// no condition (docs/event-script.md). `lengths` is EventScript_OpLengths,
// the length of an op below F0 by its high nibble. A script that does not
// decode stops the walk there, with what came before kept; false then.
bool ScriptContentFinds(const ImageAt& at, std::uint32_t start, const unsigned char lengths[16], unsigned area,
                        std::vector<Find>& out);

// One 13-byte op 9x as a find: op[5] / op[7] the cell, op[8] the object's
// +5, op[9]:op[0xA] its dword +0x18 (so +0x18, the category, is op[0xA] and
// +0x19, the id, op[9]: Field_ContentTake 0x52FBD0). False for a bit of 0xFF,
// which the game never sets.
bool ContentFind(const unsigned char* op, unsigned area, Find& out);

// One 8-byte passage entry (an area descriptor's +0x2C list; Area_PassageAhead
// 0x530600) as a find: +0 x, +1 z, +4 the bit, +5 the id or zenny / 40, +6
// the category. False where the take step never runs - word +4's bit 15 set
// and +6 not 0xFF opens a message or a script (Field_PassageOpen 0x52F600) -
// or where it sets no bit (+4 0xFF; Field_PassageTake 0x52F760).
bool PassageFind(const unsigned char* entry, unsigned area, Find& out);

// The finds as row text, by bit: "area004 (34, 56): <what it holds>", the
// same cell and find in several areas as "area005, area010 (50, 8): ...",
// different places joined by "; ". What it holds is the item's name from
// `names`, else its category and number, or the zenny.
std::map<unsigned, std::string> FindTexts(const std::vector<Find>& finds, const Names& names);

// A kind-0 chunk of an area file that writes into the enemy table: the
// table itself (tag 0xC2000, 0x4A8 bytes, loaded at 0x8C5580) or a language
// layer's name for one record (tag 0xC2000 + 0x48 + k * 0x8C, 12 bytes;
// tools/loc_build.py convert_enemy_names, DIV-0053).
struct EnemyChunk {
    std::uint32_t tag;
    std::vector<unsigned char> payload;
};

// The enemy-table chunks of one .DAT, in file order, added to `out`. False
// if the file cannot be opened; a container cut short ends where it breaks.
bool ReadEnemyChunks(const std::filesystem::path& dat, std::vector<EnemyChunk>& out);

// One area's enemy names by kind, from its chunks in the order the game reads
// them (the shipped file, then the language layer): the table, then each
// name chunk over its record. A record's kind is its byte +0xC (the bit of
// the banks +0x688 and +0x6C8: Battle_SetupEnemy 0x494320), its name the
// first 12 bytes (Battle_CopyEnemyData 0x4946C0). A name with no letter (an
// empty record, or a name still in another script's glyph codes) is skipped;
// a kind with a second, different name gets "; " and that name.
void AddEnemyNames(const std::vector<EnemyChunk>& chunks, std::map<unsigned, std::string>& kinds);

}  // namespace bof3x::save

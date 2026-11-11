// The default names of each release, and the change of a save's names from
// one release's to another's (docs/save-editor.md section 8).
//
// A save holds text in three places only: the eight character names, the
// summary's copy of the first one, and the sixty faerie names. Each release
// writes them in its own letters, and each gives them at the start from a
// table of its own: the eight default records of New Game, and the sixty
// trait records a faerie's name is copied from when she arrives. So a name
// the game gave can be told (it is that table's entry for the same record)
// and written as the other release's entry; a name the player gave cannot.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "launcher/save/block.h"

namespace bof3x::save {

constexpr int kNameChars = 8, kNameFaeries = 60;

using NameBytes = std::vector<std::uint8_t>;

// One release's default names, each without its padding. An entry that is
// empty is not known: it matches nothing and is never written.
struct NameSet {
    std::string id;                    // a language tag ("en-US"): that disc's own; "pc": this install as it plays now;
                                       // "pc-<tag>": another language layer of it; "pc-original": the exe's
    std::string prefix;                // a disc's set: the card file name it gives its saves, less the two digits
    std::vector<NameBytes> chars;      // by character record, 0..7
    std::vector<NameBytes> faeries;    // by faerie record, 0..59
    std::uint8_t faerie_pad = 0;       // what fills a faerie name to five bytes: 0xFF on the PlayStation, 0 on the PC
    bool Known() const;                // has at least the eight character names
};

// A PlayStation disc's own set, from the language layer the player built
// from that disc: the save names chunk of its FIRST.DAT (kind 20,
// tools/loc_build.py save_names_chunk), which holds the card file name and
// the names as the disc stores them. False when the file has no such chunk
// (a layer built before 2026-11-11, or from a PSP disc).
bool NameSetFromDisc(const std::wstring& first_dat, const std::string& id, NameSet& out);

// A PC set from the tables of BOF3.exe: eight names of nine bytes (the seven
// default records' and the whelp's) and sixty of five (the trait records').
NameSet NameSetFromTables(const std::string& id, const std::uint8_t* chars, const std::uint8_t* faeries);

// The set of a language layer as the PC plays it, from its FIRST.DAT: the
// default names chunk (kind 10, DIV-0020) and the faerie names (kind 15,
// group 16), where an empty entry is the exe's own (`shipped`). False when
// the file has no default names chunk.
bool NameSetFromLayer(const std::wstring& first_dat, const std::string& id, const NameSet& shipped, NameSet& out);

struct NameReport {
    int chars = 0, faeries = 0;        // names that are a set's default: written as the destination's
    int changed = 0;                   // of those, the fields whose bytes changed
    std::vector<std::string> kept;     // names that are no set's: "Character 1", "Faerie 12"
};

// Writes `to`'s name over each name of `block` that is the default of any
// set in `known` for the same record. Characters: every record that has a
// name. Faeries: the records in use. Does not write the checksum.
NameReport TranslateNames(Block& block, const std::vector<NameSet>& known, const NameSet& to);

}  // namespace bof3x::save

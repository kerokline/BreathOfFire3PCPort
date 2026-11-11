// Host tests of the save editor's format core (src/launcher/save/block.cpp;
// docs/save-editor-spec.md "Tests"). The fixtures are the player's own save
// files and are never in the repository: give one or more folders, and every
// BISLPS*.DAT and *.DAT of 0x12B0 bytes in them is a fixture, and so is every
// save of this game on a memory card file of 0x20000 bytes.
//   save_tests.exe [--dump <folder>] [--layers <a DAT folder>] <folder> [<folder> ...]
// --layers reads each release's default names from the language layers the
// player built from their discs (<tag>.FIRST.DAT in the folder), and each
// card save's names are then changed to another release's and back.
// --dump writes each card save as a PC file into a folder outside the
// repository, for the comparison with tools/save_convert.py.
// With no folder the tests that need no save still run.
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include "launcher/save/block.h"
#include "launcher/save/card.h"
#include "launcher/save/namemap.h"

namespace fs = std::filesystem;
using namespace bof3x::save;

namespace {

int g_checks = 0, g_failures = 0;

void Expect(bool ok, const std::string& what) {
    ++g_checks;
    if (ok) return;
    ++g_failures;
    std::printf("FAIL: %s\n", what.c_str());
}

// The offsets where two blocks differ.
std::vector<std::size_t> Differ(const Block& a, const Block& b) {
    std::vector<std::size_t> at;
    for (std::size_t i = 0; i < kBlockSize; ++i)
        if (a[i] != b[i]) at.push_back(i);
    return at;
}

bool Within(const std::vector<std::size_t>& at, std::initializer_list<std::pair<std::size_t, std::size_t>> ranges) {
    for (const std::size_t i : at) {
        bool in = false;
        for (const auto& r : ranges) in = in || (i >= r.first && i < r.first + r.second);
        if (!in) return false;
    }
    return true;
}

const Field* Find(const std::vector<Field>& fields, const char* label) {
    for (const Field& f : fields)
        if (f.label == label) return &f;
    return nullptr;
}

int SectionNamed(const char* name) {
    const std::vector<Section>& all = Sections();
    for (std::size_t i = 0; i < all.size(); ++i)
        if (std::strcmp(all[i].name, name) == 0) return static_cast<int>(i);
    return -1;
}

// The table itself: no two editable fields share a bit, and every field is inside the block.
void TestTable() {
    std::vector<int> owner(kBlockSize * 8, -1);
    int overlaps = 0, outside = 0, fields = 0;
    const std::vector<Section>& all = Sections();
    for (std::size_t s = 0; s < all.size(); ++s)
        for (int r = 0; r < all[s].records; ++r)
            for (const Field& f : SectionFields(static_cast<int>(s), r, Names())) {
                ++fields;
                const std::size_t bytes = f.type == Type::kBit ? 1 : f.size;
                if (f.at + bytes > kBlockSize || (f.type == Type::kStack && f.at2 >= kBlockSize)) {
                    ++outside;
                    continue;
                }
                if (FieldReadOnly(f)) continue;
                const auto claim = [&](std::size_t bit) {
                    if (owner[bit] >= 0) ++overlaps;
                    owner[bit] = static_cast<int>(s);
                };
                if (f.type == Type::kBit) {
                    claim(f.at * 8 + f.size);
                } else {
                    for (std::size_t i = 0; i < f.size * 8; ++i) claim(f.at * 8 + i);
                    if (f.type == Type::kStack)
                        for (std::size_t i = 0; i < 8; ++i) claim(f.at2 * 8 + i);
                    if (f.type == Type::kPosition)
                        for (std::size_t i = 0; i < 32; ++i) claim(f.at2 * 8 + i);
                }
            }
    Expect(fields > 2000, "the table has its fields");
    Expect(outside == 0, "every field is inside the block");
    Expect(overlaps == 0, "no two editable fields share a bit (" + std::to_string(overlaps) + " shared)");
    // The checksum word is no editable field.
    Expect(owner[0x70 * 8] < 0 && owner[0x71 * 8] < 0, "the checksum is not an editable field");
}

void TestText() {
    Block b{};
    Names latin;
    latin.latin = true;
    latin.item[1] = {"", "Dagger", "BallockKnife"};
    std::string why;
    const int chars = SectionNamed("Characters"), game = SectionNamed("Game"), items = SectionNamed("Inventory");
    const std::vector<Field> c0 = SectionFields(chars, 0, latin);
    const Field* name = Find(c0, "Name");
    Expect(name && FieldSet(b, *name, "Ryu", latin, why) && FieldText(b, *name, latin) == "Ryu", "a Latin name");
    Expect(name && !FieldSet(b, *name, "NineChars", latin, why), "a name of nine letters is refused");
    Expect(name && !FieldSet(b, *name, "R*u", latin, why), "a letter the font lacks is refused");
    Expect(name && FieldSet(b, *name, "hex: 8A 88 8A 89", latin, why) && FieldText(b, *name, latin) == "hex:8A 88 8A 89",
           "a name as codes");
    Names other;
    Expect(name && !FieldSet(b, *name, "Ryu", other, why), "letters are refused on an install that is not Latin");

    const Field* weapon = Find(c0, "Weapon");
    Expect(weapon && FieldSet(b, *weapon, "ballockknife", latin, why) && FieldText(b, *weapon, latin) == "2 - BallockKnife",
           "an item by its name");
    Expect(weapon && FieldSet(b, *weapon, "1 - Dagger", latin, why) && b[weapon->at] == 1, "an item as the list writes it");
    const Field* level = Find(c0, "Level");
    Expect(level && !FieldSet(b, *level, "100", latin, why) && !FieldSet(b, *level, "-1", latin, why), "a level outside 0 to 99");
    const Field* growth = Find(c0, "Growth: power");
    Expect(growth && FieldSet(b, *growth, "-3", latin, why) && b[growth->at] == 0xFD && FieldText(b, *growth, latin) == "-3",
           "a signed byte");

    // The reference names: an area's name, its entrances, a master.
    latin.area[10] = "A place";
    latin.master[3] = "A master";
    latin.entrances.push_back({10, "from the west", 47.0, 15.5, 3});
    latin.entrances.push_back({11, "another area's", 1.0, 2.0, 1});
    const std::vector<Field> g = SectionFields(game, 0, latin);
    const Field* x = Find(g, "Position");
    Expect(x && FieldSet(b, *x, "-12.5, 3", latin, why) && FieldText(b, *x, latin) == "-12.5, 3", "a position");
    Expect(x && !FieldSet(b, *x, "12", latin, why) && !FieldSet(b, *x, "from the west", latin, why),
           "one number, and an entrance of a different area, are refused");
    const Field* area = Find(g, "Area");
    Expect(area && FieldSet(b, *area, "a place", latin, why) && FieldText(b, *area, latin) == "10 - A place", "an area by its name");
    Expect(x && FieldChoices(b, *x, latin) == std::vector<std::string>{"47, 15.5 - from the west"}, "the area's entrances");
    Expect(x && FieldSet(b, *x, "47, 15.5 - from the west", latin, why) && b[0x27] == 3 &&
               FieldText(b, *x, latin) == "47, 15.5 - from the west",
           "an entrance sets the position and the entry flags");
    Expect(x && FieldSet(b, *x, "from the west", latin, why), "an entrance by its name");
    const Field* master = Find(c0, "Master");
    Expect(master && FieldSet(b, *master, "A master", latin, why) && FieldText(b, *master, latin) == "3 - A master" &&
               FieldSet(b, *master, "None", latin, why) && b[master->at] == 0xFF,
           "a master by its name, and none");
    const Field* clock = Find(g, "Play time");
    Expect(clock && FieldSet(b, *clock, "12:34:56", latin, why) && b[clock->at] == 12 && b[clock->at + 2] == 56, "a clock");
    const Field* zenny = Find(g, "Zenny");
    Expect(zenny && !FieldSet(b, *zenny, "10000000", latin, why), "zenny above the most");

    const std::vector<Field> w = SectionFields(items, 1, latin);
    Expect(FieldSet(b, w[0], "Dagger x 3", latin, why) && b[w[0].at] == 1 && b[w[0].at2] == 3, "a stack by name");
    Expect(FieldText(b, w[0], latin) == "1 - Dagger x 3", "a stack's text");
    Expect(FieldSet(b, w[0], "2", latin, why) && b[w[0].at] == 2 && b[w[0].at2] == 3, "a stack keeps its count");
    Expect(!FieldSet(b, w[0], "2 x 100", latin, why), "a count above 99");
    Expect(FieldSet(b, w[0], "0", latin, why) && b[w[0].at] == 0 && b[w[0].at2] == 0, "an empty place");
}

void TestFixture(const fs::path& path, const Block& b) {
    const std::string who = path.filename().string() + ": ";
    const Names names;   // codes, not letters: every name must still round-trip
    const PartyTable none;
    const bool sealed = Checksum(b) == StoredChecksum(b);

    // Round trip: nothing edited, nothing changes.
    Block out = b;
    Finish(b, out, none);
    if (sealed) Expect(out == b, who + "an unedited block is written byte for byte");
    else Expect(Within(Differ(b, out), {{0x70, 2}}), who + "an unedited damaged block changes in its checksum only");
    Expect(Check(b, b, none).empty(), who + "an unedited block has no new issue");

    // Every editable field: its own text set back changes nothing.
    int changed = 0, refused = 0;
    std::string first;
    const std::vector<Section>& all = Sections();
    for (std::size_t s = 0; s < all.size(); ++s)
        for (int r = 0; r < all[s].records; ++r)
            for (const Field& f : SectionFields(static_cast<int>(s), r, names)) {
                if (FieldReadOnly(f)) continue;
                Block copy = b;
                std::string why;
                const std::string text = FieldText(b, f, names);
                // A value the field's own limits refuse is a save the game made that way: counted, not a failure
                // of the round trip (the text is still exact).
                if (!FieldSet(copy, f, text, names, why)) {
                    ++refused;
                    if (first.empty()) first = std::string(all[s].name) + " / " + f.label + " = " + text + " (" + why + ")";
                } else if (copy != b) {
                    ++changed;
                    if (first.empty()) first = std::string(all[s].name) + " / " + f.label + " = " + text;
                }
            }
    Expect(changed == 0, who + "a field's own text set back changes nothing (" + std::to_string(changed) + "; " + first + ")");
    if (refused) std::printf("note: %s%d field value(s) outside the field's limits; first: %s\n", who.c_str(), refused, first.c_str());

    // One edit: zenny changes itself and the checksum.
    const std::vector<Field> g = SectionFields(SectionNamed("Game"), 0, names);
    std::string why;
    Block edit = b;
    FieldSet(edit, *Find(g, "Zenny"), b[0x678] == 0x39 ? "12346" : "12345", names, why);
    out = edit;
    Finish(b, out, none);
    Expect(Within(Differ(b, out), {{0x678, 4}, {0x70, 2}}), who + "zenny changes itself and the checksum only");
    Expect(Checksum(out) == StoredChecksum(out), who + "the checksum is correct after an edit");

    // Record 0's level and EXP go to the summary.
    const std::vector<Field> c0 = SectionFields(SectionNamed("Characters"), 0, names);
    edit = b;
    FieldSet(edit, *Find(c0, "Level"), b[0x9A] == 42 ? "43" : "42", names, why);
    FieldSet(edit, *Find(c0, "EXP"), "123456", names, why);
    out = edit;
    Finish(b, out, none);
    Expect(out[0xCA8] == out[0x9A] && std::memcmp(out.data() + 0xCB0, out.data() + 0x9C, 4) == 0,
           who + "the summary takes record 1's level and EXP");
    Expect(Within(Differ(b, out), {{0x9A, 1}, {0x9C, 4}, {0xCA8, 1}, {0xCB0, 4}, {0x70, 2}}),
           who + "a level and EXP edit changes those, the summary and the checksum only");

    // The battle count moves the stamps and never puts one above it.
    edit = b;
    FieldSet(edit, *Find(g, "Battle count"), std::to_string(std::uint32_t(b[0x754]) + 1000), names, why);
    out = edit;
    Finish(b, out, none);
    bool error = false;
    for (const Issue& i : Check(b, out, none)) error = error || i.error;
    Expect(!error, who + "a battle count edit leaves no error");
}

void TestFile(const fs::path& temp) {
    Block b{};
    b[0x6E8] = 1;
    b[0x678] = 7;
    b[0x70] = static_cast<std::uint8_t>(Checksum(b));
    b[0x71] = static_cast<std::uint8_t>(Checksum(b) >> 8);
    std::error_code ec;
    fs::remove_all(temp, ec);
    fs::create_directories(temp, ec);
    const std::wstring file = (temp / "BISLPS03.DAT").wstring(), keep = (temp / "save-backups").wstring();
    std::wstring backup;
    std::string why;
    Expect(WritePcFile(file, b, keep, backup, why) && backup.empty(), "a new file is written with no backup");
    Expect(fs::file_size(file, ec) == kPcFileSize, "the file is 0x12B0 bytes");
    Block back{};
    Expect(ReadPcFile(file, back, why) && back == b, "the file reads back");
    b[0x678] = 9;
    Expect(WritePcFile(file, b, keep, backup, why) && !backup.empty() && fs::exists(backup, ec), "a second write keeps the first");
    Block old{};
    Expect(ReadPcFile(backup, old, why) && old[0x678] == 7, "the backup holds the file from before");
    Expect(!fs::exists(file + L".tmp", ec), "no temporary file is left");
    fs::remove_all(temp, ec);
}


std::uint8_t XorOf(const std::uint8_t* p, std::size_t n) {
    std::uint8_t v = 0;
    for (std::size_t i = 0; i < n; ++i) v ^= p[i];
    return v;
}

// The card with no fixture: an empty card, a save added, replaced, read back.
void TestCard(const fs::path& temp) {
    Card card = EmptyCard();
    Expect(card.size() == kCardSize && CardEntries(card).empty() && CardFreeSlot(card) == 1, "an empty card has no saves");
    Expect(CardFreeNumber(card, kCardPrefixUs) == 0, "an empty card's first number is 00");

    Block b{};
    b[0x6E8] = 3;      // 3:07
    b[0x6E9] = 7;
    b[0xCA8] = 12;     // the summary's level
    b[0x90] = 'R';     // record 1's name and, after it, its id and level in the PC layout
    b[0x91] = 'y';
    b[0x92] = 'u';
    b[0x90 + 9] = 9;
    b[0x90 + 10] = 12;
    Seal(b);
    std::string why;
    std::uint8_t psx[kBlockSize];
    Expect(PcToPsx(b, psx, why) && psx[0x90 + 5] == 9 && psx[0x90 + 6] == 12, "the record moves four bytes for the card");
    Block back{};
    Expect(PsxToPc(psx, back, why) && back == b, "the two layouts go there and back");
    Expect(Checksum(back) == StoredChecksum(back), "the checksum holds in both layouts");
    Block longer = b;
    longer[0x90 + 5] = 'x';
    Expect(!PcToPsx(longer, psx, why) && !why.empty(), "a name of six bytes is refused for a card");

    const int slot = CardAdd(card, kCardPrefixUs, 0, b, nullptr, why);
    Expect(slot == 1, "a new save takes the first directory frame");
    const std::vector<CardEntry> entries = CardEntries(card);
    Expect(entries.size() == 1 && entries[0].bof3 && entries[0].name == "BASLUS-00422BOF300" && entries[0].number == 0,
           "the new save is a save of this game");
    const std::uint8_t* frame = card.data() + 0x80;
    Expect(frame[0x7F] == XorOf(frame, 0x7F) && frame[4] == 0 && frame[5] == 0x20, "the directory frame has its size and check byte");
    Expect(CardGet(card, slot, back, why) && back == b, "the new save reads back");
    Expect(CardFreeNumber(card, kCardPrefixUs) == 1 && CardFreeNumber(card, kCardPrefixJp) == 0 && CardFreeSlot(card) == 2,
           "the next save is 01 in frame 2");
    // The title: full-width "3(1) 03h07m Lv.12" ends the 64 bytes.
    const std::uint8_t* title = card.data() + 0x2000 + 4;
    const std::uint8_t want_tail[] = {0x82, 0x4F, 0x82, 0x52, 0x82, 0x88, 0x82, 0x4F, 0x82, 0x56, 0x82, 0x8D,
                                      0x81, 0x40, 0x82, 0x6B, 0x82, 0x96, 0x81, 0x44, 0x82, 0x50, 0x82, 0x51};
    Expect(std::memcmp(title + 64 - sizeof want_tail, want_tail, sizeof want_tail) == 0, "the title shows the time and the level");

    const Card before = card;
    Expect(CardPut(card, slot, b, why) && card == before, "the same block put back changes nothing");
    Block richer = b;
    richer[0x678] = 50;
    Seal(richer);
    Expect(CardPut(card, slot, richer, why), "a block is replaced");
    std::size_t outside = 0;
    for (std::size_t i = 0; i < kCardSize; ++i)
        if (card[i] != before[i] && (i < 0x2000 + 0x200 || i >= 0x2000 + 0x200 + kBlockSize)) ++outside;
    Expect(outside == 0, "a replace with the same time and level changes the block only");
    Expect(CardAdd(card, kCardPrefixUs, 0, b, nullptr, why) == 0, "a second save of one name is refused");
    Expect(CardAdd(card, kCardPrefixJp, 0, b, CardHeader(card, slot), why) == 2, "a save of the other release is added");
    Expect(std::memcmp(card.data() + 0x4000 + 0x60, card.data() + 0x2000 + 0x60, 0x1A0) == 0, "the icon comes with the header given");

    std::error_code ec;
    fs::remove_all(temp, ec);
    fs::create_directories(temp, ec);
    const std::wstring file = (temp / "card.mcr").wstring(), keep = (temp / "save-backups").wstring();
    std::wstring backup;
    Expect(WriteCardFile(file, card, keep, backup, why) && backup.empty(), "a new card file is written");
    Card read;
    Expect(ReadCardFile(file, read, why) && read == card, "the card file reads back");
    Expect(WriteCardFile(file, before, keep, backup, why) && !backup.empty(), "a second write keeps the first card");
    Block pc{};
    Expect(!ReadPcFile(file, pc, why), "a card file is not a PC save file");
    Expect(WritePcFile((temp / "BISLPS00.DAT").wstring(), b, keep, backup, why) &&
               !ReadCardFile((temp / "BISLPS00.DAT").wstring(), read, why),
           "a PC save file is not a card file");
    fs::remove_all(temp, ec);
}


NameBytes Bytes(const char* text) { return NameBytes(text, text + std::strlen(text)); }

// The name map with sets of the test's own.
void TestNames(const fs::path& temp) {
    NameSet a, b;
    a.id = "a";
    b.id = "b";
    a.faerie_pad = 0xFF;
    b.faerie_pad = 0;
    a.chars.assign(kNameChars, Bytes("Aaa"));
    b.chars.assign(kNameChars, Bytes("\x84\x46\x84\xD6\x84\xC7"));   // six bytes: too long for a card, fine on the PC
    a.chars[1] = Bytes("Nina");
    b.chars[1] = Bytes("\x84\xD6");
    a.faeries.assign(kNameFaeries, Bytes("Fae"));
    b.faeries.assign(kNameFaeries, Bytes("\x80\x95\x84\x07\x81"));   // two glyphs and half of a third
    Expect(a.Known() && b.Known(), "a set with eight names is known");
    const std::vector<NameSet> known = {a, b};

    Block x{};
    const auto name = [&x](int record) { return x.data() + 0x90 + record * 0xA4; };
    std::memcpy(name(0), "Aaa", 3);
    std::memcpy(name(1), "Nina", 4);
    std::memcpy(name(2), "Bob", 3);        // the player's
    x[0xCF0 + 8 * 3] = 1;                  // faerie 4 is there, with the set's name and the card's padding
    std::memcpy(x.data() + 0xF10 + 5 * 3, "Fae\xFF\xFF", 5);
    x[0xCF0 + 8 * 4] = 1;                  // faerie 5 is there, renamed
    std::memcpy(x.data() + 0xF10 + 5 * 4, "Zed  ", 5);
    std::memcpy(x.data() + 0xF10 + 5 * 5, "Fae\xFF\xFF", 5);   // faerie 6 is not there: her bytes stay
    const Block before = x;
    NameReport r = TranslateNames(x, known, b);
    Expect(r.chars == 2 && r.faeries == 1 && r.changed == 3, "two characters and one faerie take the other set's names");
    Expect(r.kept.size() == 2 && r.kept[0] == "Character 3" && r.kept[1] == "Faerie 5", "the player's names are reported and kept");
    Expect(std::memcmp(name(0), "\x84\x46\x84\xD6\x84\xC7\0\0\0", 9) == 0 && std::memcmp(name(1), "\x84\xD6\0\0\0\0\0\0\0", 9) == 0,
           "a character's name is written with zeros after it");
    Expect(std::memcmp(name(2), "Bob", 4) == 0, "the player's name is as it was");
    Expect(std::memcmp(x.data() + 0xF10 + 5 * 3, "\x80\x95\x84\x07\x81", 5) == 0, "a faerie's name is the other set's");
    Expect(std::memcmp(x.data() + 0xF10 + 5 * 5, "Fae\xFF\xFF", 5) == 0, "a faerie record nobody is in is not touched");
    // The half glyph our build does not store (DIV-0200) is still that name.
    x[0xF10 + 5 * 3 + 4] = 0;
    r = TranslateNames(x, known, a);
    Expect(r.chars == 2 && r.faeries == 1 && x == before, "the names go back, with the card's padding");
    r = TranslateNames(x, known, a);
    Expect(r.chars == 2 && r.faeries == 1 && r.changed == 0 && x == before, "names that are the set's already do not change");

    std::error_code ec;
    fs::create_directories(temp, ec);
    // A layer's FIRST.DAT: the default names chunk, and the faerie names where an empty one is the exe's.
    const fs::path dat = temp / "FIRST.DAT";
    {
        std::string names(1, '\x08');
        for (const char* n : {"Ryu", "Nina", "Garr", "Teepo", "Rei", "Momo", "Peco", "Whelp"}) names += std::string(n) + '\0';
        std::string fae(1, '\x03');
        fae += std::string("Palce") + '\0' + '\0' + std::string("Candy") + '\0';   // the second is empty
        std::string file;
        const auto chunk = [&file](std::uint32_t kind, std::uint32_t tag, const std::string& payload) {
            const std::uint32_t head[4] = {kind, tag, static_cast<std::uint32_t>(payload.size()), 0};
            file.append(reinterpret_cast<const char*>(head), 16);
            file += payload;
        };
        chunk(5, 0x1234, std::string(32, 'x'));
        chunk(10, 0, names);
        chunk(15, 16, fae);
        // The disc's own: version 1, the card file name in 20 bytes, eight names and sixty of five bytes.
        std::string disc(1, '\x01');
        disc += std::string("BASLUS-00422BOF3") + std::string(4, '\0');
        for (const char* n : {"Ryu\0\0", "Nina\0", "Garr\0", "Teepo", "Rei\0\0", "Momo\0", "Peco\0", "Whelp"}) disc.append(n, 5);
        for (int r = 0; r < kNameFaeries; ++r) disc.append(r == 54 ? "Dr>K\xFF" : "Pao\xFF\xFF", 5);
        chunk(20, 0, disc);
        std::FILE* f = std::fopen(dat.string().c_str(), "wb");
        std::fwrite(file.data(), 1, file.size(), f);
        std::fclose(f);
    }
    NameSet layer;
    Expect(NameSetFromLayer(dat.wstring(), "pc", b, layer) && layer.Known() && layer.id == "pc" && layer.chars[3] == Bytes("Teepo") &&
               layer.prefix.empty(),
           "a layer gives the install's eight names");
    NameSet disc;
    Expect(NameSetFromDisc(dat.wstring(), "en-US", disc) && disc.Known() && disc.id == "en-US" && disc.prefix == "BASLUS-00422BOF3" &&
               disc.faerie_pad == 0xFF,
           "a layer gives its disc's set and card file name");
    Expect(disc.chars[0] == Bytes("Ryu") && disc.chars[3] == Bytes("Teepo") && disc.faeries[0] == Bytes("Pao") &&
               disc.faeries[54] == Bytes("Dr>K"),
           "the disc's names are without their filler");
    Expect(layer.faeries[0] == Bytes("Palce") && layer.faeries[1] == b.faeries[1] && layer.faeries[2] == Bytes("Candy"),
           "an empty faerie name of a layer is the exe's");
    Expect(!NameSetFromLayer((temp / "none.DAT").wstring(), "pc", b, layer) && !NameSetFromDisc((temp / "none.DAT").wstring(), "x", disc),
           "no file, no set");
    fs::remove_all(temp, ec);

    std::uint8_t chars[8 * 9] = {}, faeries[60 * 5] = {};
    std::memcpy(chars, "\x84\x46", 2);
    std::memcpy(faeries, "\x80\x67\x83\xE1\0", 5);
    const NameSet exe = NameSetFromTables("pc-original", chars, faeries);
    Expect(exe.chars[0] == Bytes("\x84\x46") && exe.faeries[0] == Bytes("\x80\x67\x83\xE1") && !exe.Known(),
           "the exe's tables give names without their zeros");
}

// One card save's names to another release and back: its own is the set of
// its card file name, the other the next release the layers give.
void TestNamesFixture(const std::string& what, const std::string& prefix, const Block& b, const std::vector<NameSet>& sets,
                      int& all_default) {
    const NameSet* own = nullptr;
    const NameSet* other = nullptr;
    // By its letters, not its file name alone: the sibling's imports are one release's block under another's name.
    for (const NameSet& s : sets)
        if (!s.prefix.empty() && s.Known() && std::memcmp(b.data() + 0x90, s.chars[0].data(), s.chars[0].size()) == 0 &&
            (!own || s.prefix == prefix)) {
            bool all = true;
            for (int i = 0; i < kNameChars; ++i) {
                const NameBytes& n = s.chars[static_cast<std::size_t>(i)];
                all = all && std::memcmp(b.data() + 0x90 + i * 0xA4, n.data(), n.size()) == 0 && b[0x90 + i * 0xA4 + n.size()] == 0;
            }
            if (all) own = &s;
        }
    if (!own) return;
    for (const NameSet& s : sets)
        if (!s.prefix.empty() && s.Known() && s.chars != own->chars && !other) other = &s;
    if (!other) return;
    const bool is_us = true;
    const NameSet* us = own;
    const NameSet* jp = other;
    Block x = b;
    NameReport there = TranslateNames(x, sets, is_us ? *jp : *us);
    bool right = true;
    for (int i = 0; i < kNameChars; ++i) {
        const NameBytes& want = (is_us ? *jp : *us).chars[static_cast<std::size_t>(i)];
        const std::uint8_t* f = x.data() + 0x90 + i * 0xA4;
        right = right && std::memcmp(f, want.data(), want.size()) == 0 && f[want.size()] == 0;
    }
    Expect(there.chars == kNameChars && right, what + ": the eight names become the other release's");
    std::uint8_t psx[kBlockSize];
    std::string why;
    Expect(PcToPsx(x, psx, why), what + ": the names of the other release fit a card");
    NameReport back = TranslateNames(x, sets, is_us ? *us : *jp);
    if (there.kept.empty() && back.kept.empty()) {
        ++all_default;
        // A save of mixed names (the sibling's import: Japan's characters, the US faeries) comes back in one release.
        Block again = x;
        TranslateNames(again, sets, is_us ? *jp : *us);
        TranslateNames(again, sets, is_us ? *us : *jp);
        Expect(again == x, what + ": there and back again is the same block");
        bool same_chars = true;
        for (int i = 0; i < kNameChars; ++i)
            same_chars = same_chars && std::memcmp(x.data() + 0x90 + i * 0xA4, b.data() + 0x90 + i * 0xA4, 9) == 0;
        Expect(same_chars, what + ": the eight names come back");
    }
}

// One memory card of the player's: every save of this game on it.
int TestCardFixture(const fs::path& path, const fs::path& dump, const std::vector<NameSet>& sets, int& all_default) {
    Card card;
    std::string why;
    if (!ReadCardFile(path.wstring(), card, why)) return 0;
    const std::string name = path.filename().string();
    int saves = 0;
    for (const CardEntry& e : CardEntries(card)) {
        if (!e.bof3) continue;
        ++saves;
        const std::string what = name + " frame " + std::to_string(e.slot);
        Block b;
        Expect(CardGet(card, e.slot, b, why), what + ": opens");
        if (Checksum(b) != StoredChecksum(b)) continue;   // a save the card holds damaged
        const std::uint8_t* raw = card.data() + static_cast<std::size_t>(e.slot) * 0x2000 + 0x200;
        std::uint8_t psx[kBlockSize];
        Expect(PcToPsx(b, psx, why) && std::memcmp(psx, raw, kBlockSize) == 0, what + ": the layouts go there and back");
        Block out = b;
        Finish(b, out, PartyTable());
        Expect(out == b, what + ": an unedited save is byte for byte");
        Expect(Check(b, out, PartyTable()).empty(), what + ": an unedited save has no new issue");
        Card copy = card;
        Expect(CardPut(copy, e.slot, out, why) && copy == card, what + ": the card is byte for byte after a put");
        // One edit: zenny. The block's zenny and checksum change on the card, nothing else.
        Block rich = b;
        rich[0x678] = static_cast<std::uint8_t>(rich[0x678] + 1);
        Finish(b, rich, PartyTable());
        Expect(CardPut(copy, e.slot, rich, why), what + ": an edited save is put");
        const std::size_t base = static_cast<std::size_t>(e.slot) * 0x2000 + 0x200;
        bool only = true;
        for (std::size_t i = 0; i < kCardSize; ++i)
            if (copy[i] != card[i] && !(i == base + 0x678 || i == base + 0x70 || i == base + 0x71)) only = false;
        Expect(only, what + ": a zenny edit changes zenny and the checksum only");
        // To a new card and back: the block is the same.
        Card fresh = EmptyCard();
        Block again;
        const int slot = CardAdd(fresh, e.prefix, 0, b, CardHeader(card, e.slot), why);
        Expect(slot == 1 && CardGet(fresh, slot, again, why) && again == b, what + ": the save moves to a new card");
        if (!sets.empty()) TestNamesFixture(what, e.prefix, b, sets, all_default);
        if (!dump.empty()) {
            std::wstring backup;
            WritePcFile((dump / (path.stem().string() + "-" + std::to_string(e.slot) + ".DAT")).wstring(), b,
                        (dump / "kept").wstring(), backup, why);
        }
    }
    return saves;
}

}  // namespace

int main(int argc, char** argv) {
    TestTable();
    TestText();
    TestFile(fs::temp_directory_path() / "bof3x_save_tests");
    TestCard(fs::temp_directory_path() / "bof3x_save_tests_card");
    TestNames(fs::temp_directory_path() / "bof3x_save_tests_names");
    int fixtures = 0, cards = 0, card_saves = 0, empty_same = 0, all_default = 0;
    fs::path dump;
    std::vector<NameSet> sets;
    for (int i = 1; i < argc; ++i) {
        std::error_code ec;
        if (std::strcmp(argv[i], "--dump") == 0 && i + 1 < argc) {
            dump = argv[++i];
            fs::create_directories(dump, ec);
            continue;
        }
        if (std::strcmp(argv[i], "--layers") == 0 && i + 1 < argc) {
            const fs::path dir = argv[++i];
            for (const char* tag : {"en-US", "en-150", "fr-FR", "de-DE", "ja-JP"}) {
                const fs::path file = dir / (std::string(tag) + ".FIRST.DAT");
                if (!fs::exists(file, ec)) continue;
                NameSet disc, layer;
                Expect(NameSetFromDisc(file.wstring(), tag, disc) && disc.Known(), std::string("--layers: ") + tag + " has its disc's names");
                bool sixty = disc.faeries.size() == kNameFaeries;
                for (const NameBytes& n : disc.faeries) sixty = sixty && !n.empty() && n.size() <= 5;
                Expect(sixty, std::string("--layers: ") + tag + " has sixty faerie names");
                Expect(NameSetFromLayer(file.wstring(), std::string("pc-") + tag, NameSet(), layer) && layer.Known(),
                       std::string("--layers: ") + tag + " has the names as the PC plays them");
                std::printf("  %s: card file name %s\n", tag, disc.prefix.c_str());
                sets.push_back(disc);
                sets.push_back(layer);
            }
            continue;
        }
        for (const fs::directory_entry& e : fs::recursive_directory_iterator(argv[i], ec)) {
            if (e.is_regular_file(ec) && e.file_size(ec) == kCardSize) {
                Card card;
                std::string why;
                if (!ReadCardFile(e.path().wstring(), card, why)) continue;
                ++cards;
                card_saves += TestCardFixture(e.path(), dump, sets, all_default);
                // An emulator's empty card whose blocks are all zero is the card EmptyCard makes.
                bool zero = CardEntries(card).empty() && CardFreeSlot(card) == 1;
                for (std::size_t k = 0x2000; k < kCardSize && zero; ++k) zero = card[k] == 0;
                if (zero) {
                    ++empty_same;
                    Expect(card == EmptyCard(), e.path().filename().string() + ": EmptyCard is this empty card");
                }
                continue;
            }
            if (!e.is_regular_file(ec) || e.file_size(ec) != kPcFileSize) continue;
            Block b;
            std::string why;
            if (!ReadPcFile(e.path().wstring(), b, why)) continue;
            ++fixtures;
            TestFixture(e.path(), b);
        }
    }
    std::printf("%d checks, %d failures, %d fixture(s); %d card(s) with %d save(s), %d empty card(s) compared; "
                "%d card save(s) with default names only\n",
                g_checks, g_failures, fixtures, cards, card_saves, empty_same, all_default);
    return g_failures ? 1 : 0;
}

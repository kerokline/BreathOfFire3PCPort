// Host tests of the save editor's format core (src/launcher/save/block.cpp;
// docs/save-editor-spec.md "Tests"). The fixtures are the player's own save
// files and are never in the repository: give one or more folders, and every
// BISLPS*.DAT and *.DAT of 0x12B0 bytes in them is a fixture.
//   save_tests.exe <folder> [<folder> ...]
// With no folder the tests that need no save still run.
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include "launcher/save/block.h"

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

}  // namespace

int main(int argc, char** argv) {
    TestTable();
    TestText();
    TestFile(fs::temp_directory_path() / "bof3x_save_tests");
    int fixtures = 0;
    for (int i = 1; i < argc; ++i) {
        std::error_code ec;
        for (const fs::directory_entry& e : fs::recursive_directory_iterator(argv[i], ec)) {
            if (!e.is_regular_file(ec) || e.file_size(ec) != kPcFileSize) continue;
            Block b;
            std::string why;
            if (!ReadPcFile(e.path().wstring(), b, why)) continue;
            ++fixtures;
            TestFixture(e.path(), b);
        }
    }
    std::printf("%d checks, %d failures, %d fixture(s)\n", g_checks, g_failures, fixtures);
    return g_failures ? 1 : 0;
}

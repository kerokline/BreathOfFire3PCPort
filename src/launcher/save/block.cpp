// The save editor's format core: see block.h. Every offset here is from
// docs/save-editor-spec.md sections 7 and 8 and the range files under
// docs/save-block/, which hold the evidence for each byte.
#include "launcher/save/block.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>

namespace bof3x::save {

namespace {

constexpr std::uint32_t kChars = 0x90, kCharStride = 0xA4, kCharCount = 8;
constexpr std::uint32_t kChapterFlags = 0x5B0, kEventFlags = 0x650;
constexpr std::uint32_t kFieldOrder = 0x682, kBattleOrder = 0x685, kPartySet = 0x74C;
constexpr std::uint32_t kBattleCount = 0x754, kClock = 0x6E8;
constexpr std::uint32_t kGeneRows = 0xC28, kGeneRowCount = 18;
constexpr std::uint32_t kFaeries = 0xCF0, kFaerieCount = 60, kBuildings = 0xED0, kBuildingCount = 8,
                        kFaerieNames = 0xF10;
constexpr std::uint32_t kSummary = 0xCA0;
constexpr unsigned kPartyRows = 19;

std::uint64_t Get(const Block& b, std::uint32_t at, std::uint32_t size) {
    std::uint64_t v = 0;
    for (std::uint32_t i = 0; i < size; ++i) v |= static_cast<std::uint64_t>(b[at + i]) << (8 * i);
    return v;
}
void Put(Block& b, std::uint32_t at, std::uint32_t size, std::uint64_t v) {
    for (std::uint32_t i = 0; i < size; ++i) b[at + i] = static_cast<std::uint8_t>(v >> (8 * i));
}
std::int64_t GetSigned(const Block& b, std::uint32_t at, std::uint32_t size) {
    const std::uint64_t v = Get(b, at, size);
    const std::uint64_t sign = 1ull << (8 * size - 1);
    return static_cast<std::int64_t>(v ^ sign) - static_cast<std::int64_t>(sign);
}
bool Same(const Block& a, const Block& b, std::uint32_t at, std::uint32_t size) {
    return std::memcmp(a.data() + at, b.data() + at, size) == 0;
}

// ---- the field table ----------------------------------------------------------

Field Num(std::string label, std::uint32_t at, std::uint32_t size, char cls = 'E', std::string note = {}) {
    Field f;
    f.label = std::move(label);
    f.at = at;
    f.size = size;
    f.hi = static_cast<std::int64_t>((1ull << (8 * size)) - 1);
    f.cls = cls;
    f.note = std::move(note);
    return f;
}
Field Rng(std::string label, std::uint32_t at, std::uint32_t size, std::int64_t lo, std::int64_t hi, char cls = 'E',
          std::string note = {}) {
    Field f = Num(std::move(label), at, size, cls, std::move(note));
    f.lo = lo;
    f.hi = hi;
    return f;
}
Field Sgn(std::string label, std::uint32_t at, std::uint32_t size, char cls = 'E', std::string note = {}) {
    Field f = Num(std::move(label), at, size, cls, std::move(note));
    f.type = Type::kSigned;
    f.lo = -(1ll << (8 * size - 1));
    f.hi = (1ll << (8 * size - 1)) - 1;
    return f;
}
Field Lst(std::string label, std::uint32_t at, List list, char cls = 'E', std::string note = {}) {
    Field f = Num(std::move(label), at, 1, cls, std::move(note));
    f.list = list;
    return f;
}
Field Bit(std::string label, std::uint32_t at, std::uint32_t bit, char cls = 'E', std::string note = {}) {
    Field f = Num(std::move(label), at, bit, cls, std::move(note));
    f.type = Type::kBit;
    f.hi = 1;
    return f;
}
// Bit `n` of a bank of bits that starts at `at`.
Field BankBit(std::string label, std::uint32_t at, unsigned n, char cls = 'E', std::string note = {}) {
    return Bit(std::move(label), at + n / 8, n % 8, cls, std::move(note));
}
Field Typed(std::string label, Type type, std::uint32_t at, std::uint32_t size, char cls = 'E',
            std::string note = {}) {
    Field f = Num(std::move(label), at, size, cls, std::move(note));
    f.type = type;
    return f;
}
Field Name(std::string label, std::uint32_t at, std::uint32_t size, std::int64_t most, std::string note = {}) {
    Field f = Typed(std::move(label), Type::kName, at, size, 'E', std::move(note));
    f.hi = most;
    return f;
}

std::string N(unsigned n) { return std::to_string(n); }

enum SectionId {
    kSecGame,
    kSecParty,
    kSecCharacters,
    kSecItems,
    kSecKeyItems,
    kSecAbilities,
    kSecDragon,
    kSecMasters,
    kSecFishing,
    kSecVillage,
    kSecFaeries,
    kSecBuildings,
    kSecOptions,
    kSecStory,
    kSecChapterFlags,
    kSecEventFlags,
    kSecBanks,
    kSecCounters,
    kSecOther,
    kSecCount
};

const char* const kItemCategory[4] = {"Items", "Weapons", "Armor", "Accessories"};
const char* const kBankName[5] = {"Enemies defeated", "Skills learned", "Enemies identified",
                                  "Chests and field objects", "Passage finds"};
constexpr std::uint32_t kBankAt[5] = {0x688, 0x6A8, 0x6C8, 0x6EC, 0x72C};

void GameFields(std::vector<Field>& f) {
    f.push_back(Rng("Zenny", 0x678, 4, 0, 9999999));
    f.push_back(Typed("Play time", Type::kClock, kClock, 4, 'E',
                      "Hours:minutes:seconds.frames. Do not set all to zero: the game reads that as a new game."));
    f.push_back(Num("Area", 0x24, 2, 'E', "The area number where the game continues."));
    f.push_back(Typed("Position X", Type::kFixed, 0x28, 4));
    f.push_back(Typed("Position Z", Type::kFixed, 0x2C, 4));
    f.push_back(Num("Entry flags", 0x27, 1, 'E', "The low bits are the direction. 128 drops the party in."));
    f.push_back(Rng("Chapter", 0x00, 1, 0, 19, 'A',
                    "0 to 19, not 16. This is the row of chapter flags that the game uses. If you change it, "
                    "set Chapter status bit 7 to Off."));
    f.push_back(Num("Battle count", kBattleCount, 4, 'A',
                    "When you change it, the editor moves each faerie village stamp by the same amount."));
}

void PartyFields(std::vector<Field>& f) {
    const std::string order = "A member id, or 255 for an empty place. Empty places come last. "
                              "Each member must have its Joined bit.";
    for (unsigned i = 0; i < 3; ++i) f.push_back(Lst("Field order " + N(i + 1), kFieldOrder + i, List::kMember, 'E', order));
    for (unsigned i = 0; i < 3; ++i)
        f.push_back(Lst("Battle order " + N(i + 1), kBattleOrder + i, List::kMember, 'E',
                        "The same members as the field order. The editor copies the field order when they differ."));
    f.push_back(Rng("Formation", 0x680, 1, 0, 5, 'E',
                    "A formation number from 0 to 5. It must be unlocked. One member: 0. Two members: 0 to 2."));
    for (unsigned i = 0; i < 6; ++i) f.push_back(Bit("Formation " + N(i) + " unlocked", 0x681, i));
    f.push_back(Num("Party row", kPartySet, 1, 'D', "Set by the editor from the field order."));
}

void CharacterFields(std::vector<Field>& f, unsigned record) {
    const std::uint32_t b = kChars + kCharStride * record;
    f.push_back(Name("Name", b + 0x00, 9, 8));
    // 0 is the level of a record that holds nobody yet (measured: one record in every PC save).
    f.push_back(Rng("Level", b + 0x0A, 1, 0, 99, 'E', "1 to 99. Change the EXP also. The game gives levels from the EXP."));
    f.push_back(Rng("EXP", b + 0x0C, 4, 0, 9999999));
    f.push_back(Num("HP", b + 0x18, 2, 'E', "The game decreases this to the maximum HP at load."));
    f.push_back(Num("AP", b + 0x1A, 2, 'E', "Keep this at or below the maximum AP."));
    f.push_back(Lst("Weapon", b + 0x12, List::kItem1));
    f.push_back(Lst("Shield", b + 0x13, List::kItem2));
    f.push_back(Lst("Helmet", b + 0x14, List::kItem2));
    f.push_back(Lst("Armor", b + 0x15, List::kItem2));
    f.push_back(Lst("Accessory 1", b + 0x16, List::kItem3));
    f.push_back(Lst("Accessory 2", b + 0x17, List::kItem3));
    f.push_back(Rng("Base maximum HP", b + 0x40, 2, 0, 999));
    f.push_back(Rng("Base maximum AP", b + 0x42, 2, 0, 999));
    f.push_back(Rng("Base power", b + 0x44, 2, 0, 999));
    f.push_back(Rng("Base defense", b + 0x46, 2, 0, 999));
    f.push_back(Rng("Base agility", b + 0x48, 2, 0, 999));
    f.push_back(Rng("Base intelligence", b + 0x4A, 2, 0, 999));
    f.push_back(Num("Willpower", b + 0x1C, 1, 'E', "The current value. An inn sets it again."));
    f.push_back(Num("Base willpower", b + 0x4E, 1, 'E', "0 to 99."));
    f.push_back(Num("Fatigue", b + 0x1E, 1, 'E', "0 to 9. Each step decreases the maximum HP. An inn clears it."));
    f.push_back(Num("Master", b + 0x1F, 1, 'E',
                    "A master number, or 255 for no master. Change the level at apprenticing and the growth also."));
    f.push_back(Num("Level at apprenticing", b + 0x88, 1, 'E', "0 with no master. Not above the level."));
    const char* const growth[6] = {"maximum HP", "maximum AP", "power", "defense", "agility", "intelligence"};
    for (unsigned i = 0; i < 6; ++i)
        f.push_back(Sgn(std::string("Growth: ") + growth[i], b + 0x89 + i, 1, 'E',
                        "Added to the gain at each level. 0 with no master."));
    const char* const resist[9] = {"fire", "ice", "lightning", "earth", "wind", "holy", "status 1", "status 2",
                                   "status 3"};
    for (unsigned i = 0; i < 9; ++i)
        f.push_back(Num(std::string("Base resistance: ") + resist[i], b + 0x4F + i, 1, 'E', "0 to 7."));
    const char* const rate[5] = {"surprise", "reprisal", "critical", "dodge", "accuracy"};
    for (unsigned i = 0; i < 5; ++i)
        f.push_back(Num(std::string("Base ") + rate[i], b + 0x58 + i, 1, 'E', "A percentage, 0 to 100."));
    const char* const kind[4] = {"Heal", "Assist", "Attack", "Skill"};
    for (unsigned k = 0; k < 4; ++k)
        for (unsigned i = 0; i < 10; ++i) {
            std::string note = "Put the abilities at the start of the list, with no empty place between them.";
            if (record == 7 && k == 3 && i == 0) note = "On this record the first place is a minigame rank, not an ability.";
            f.push_back(Lst(std::string(kind[k]) + " ability " + N(i + 1), b + 0x60 + 10 * k + i, List::kAbility, 'E',
                            std::move(note)));
        }
    f.push_back(Num("Character id", b + 0x09, 1, 'A', "The character that this record holds. Change it only to a "
                                                      "value that the game puts in this record."));
    f.push_back(Bit("Joined", b + 0x0B, 0, 'A', "Each member in the party lists must have this bit."));
    f.push_back(Bit("Fixed in the party screen", b + 0x0B, 1, 'A'));
    f.push_back(Bit("Flag bit 2", b + 0x0B, 2, 'A', "The game clears this after a battle that the member ended down."));
    f.push_back(Num("Status bits", b + 0x10, 2, 'A', "16384 (down) goes with HP 0."));
    f.push_back(Num("Halving bits", b + 0x1D, 1, 'A', "Each bit increases one resistance by 2."));
    const std::string made = "The game makes this again at each load.";
    f.push_back(Num("Maximum HP", b + 0x20, 2, 'D', made));
    f.push_back(Num("Maximum AP", b + 0x22, 2, 'D', made));
    f.push_back(Num("Power", b + 0x24, 2, 'D', made));
    f.push_back(Num("Defense", b + 0x26, 2, 'D', made));
    f.push_back(Num("Agility", b + 0x28, 2, 'D', made));
    f.push_back(Num("Intelligence", b + 0x2A, 2, 'D', made));
}

void DragonFields(std::vector<Field>& f) {
    for (unsigned i = 0; i < 18; ++i) f.push_back(BankBit("Gene " + N(i + 1), 0xC70, i));
    const std::string row = "Four bytes: three gene numbers (0 to 17, then FF) and the form. An empty row is FF FF FF FF.";
    for (unsigned i = 0; i < 6; ++i) f.push_back(Typed("Recent combination " + N(i + 1), Type::kHex, 0xC28 + 4 * i, 4, 'A', row));
    for (unsigned i = 0; i < 12; ++i) f.push_back(Typed("Stored combination " + N(i + 1), Type::kHex, 0xC40 + 4 * i, 4, 'A', row));
}

void MasterFields(std::vector<Field>& f) {
    for (unsigned i = 0; i < 17; ++i)
        f.push_back(BankBit("Master " + N(i) + " met", 0xC74, i, 'E', "Set when the introduction of the master starts."));
    for (unsigned i = 0; i < 17; ++i)
        f.push_back(BankBit("Master " + N(i) + " available", 0xC77, i, 'E',
                            "The masters window shows the masters that have this bit."));
    f.push_back(Typed("Countdown 1", Type::kClock, 0xC7C, 4, 'E', "While this runs, the game refuses ability 62."));
    f.push_back(Typed("Countdown 2", Type::kClock, 0xC80, 4, 'E', "While this runs, the game refuses ability 21."));
    f.push_back(Num("Battles after master 4 was met", 0xC1B, 1, 'A', "Counts to 30."));
}

void FishingFields(std::vector<Field>& f) {
    f.push_back(Num("Rod", 0x750, 1, 'E', "An item number. The rod is not in the inventory."));
    f.push_back(Num("Lure", 0x74E, 1, 'E', "An item number. The lure is not in the inventory."));
    f.push_back(Rng("Lure count", 0x74F, 1, 0, 1, 'E', "1 with a lure, 0 with no lure."));
    for (unsigned i = 0; i < 23; ++i)
        f.push_back(Num("Largest fish, kind " + N(i + 1), 0x70C + i, 1, 'E', "0: not caught."));
    f.push_back(Num("Fishing sessions", 0x764, 4, 'A'));
}

void VillageFields(std::vector<Field>& f) {
    f.push_back(Num("Mood", 0xCEA, 1, 'E', "0 to 99."));
    f.push_back(Num("Dark level", 0xCEB, 1, 'E', "1 to 10 after the village starts."));
    f.push_back(Num("Lit level", 0xCEC, 1, 'E', "0 to 7."));
    const std::string stamp = "A battle count. It must be at or below the battle count of the save.";
    f.push_back(Num("Battles toward the next story flag", 0xCD0, 4, 'A'));
    f.push_back(Num("Stamp: last growth", 0xCD4, 4, 'A', stamp));
    f.push_back(Num("Stamp: last decrease", 0xCD8, 4, 'A', stamp));
    f.push_back(Num("Stamp: last mood update", 0xCDC, 4, 'A', stamp));
    f.push_back(Num("Stamp: lit level", 0xCE0, 4, 'A', stamp));
    f.push_back(Num("Stamp: dark level", 0xCE4, 4, 'A', stamp));
    f.push_back(Num("Highest area seen", 0xCE8, 2, 'A'));
    f.push_back(Num("Bias 1", 0xCED, 1, 'D', "The game makes this again when you go into the village."));
    f.push_back(Num("Bias 2", 0xCEE, 1, 'D', "The game makes this again when you go into the village."));
}

void FaerieFields(std::vector<Field>& f, unsigned record) {
    const std::uint32_t b = kFaeries + 8 * record;
    f.push_back(Name("Name", kFaerieNames + 5 * record, 5, 5));
    f.push_back(Rng("In the village", b + 0, 1, 0, 1));
    f.push_back(Rng("Place", b + 1, 1, 0, 11, 'E', "0: no place. 1 to 8: a building. 9, 10, 11: the three open places."));
    f.push_back(Num("State", b + 2, 1, 'A', "Its meaning depends on the kind of building."));
    f.push_back(Num("Status", b + 3, 1, 'A', "For building kind 13 only."));
    f.push_back(Num("Stamp", b + 4, 4, 'A', "A battle count. It must be at or below the battle count of the save."));
}

void BuildingFields(std::vector<Field>& f, unsigned record) {
    const std::uint32_t b = kBuildings + 8 * record;
    f.push_back(Num("Kind", b + 0, 1, 'E', "0: empty. 4 to 13: the kinds of building."));
    f.push_back(Num("First choice", b + 1, 1));
    f.push_back(Num("Second choice", b + 2, 1));
    f.push_back(Num("Level", b + 3, 1));
    f.push_back(Num("Stamp", b + 4, 4, 'A', "A battle count. It must be at or below the battle count of the save."));
}

void OptionFields(std::vector<Field>& f) {
    f.push_back(Rng("Message speed", 0x78, 1, 0, 2));
    f.push_back(Rng("Sound", 0x79, 1, 0, 1, 'E', "0: stereo. 1: mono."));
    f.push_back(Rng("Window color", 0x7A, 1, 0, 3));
    f.push_back(Rng("Window background", 0x7B, 1, 0, 3));
    f.push_back(Rng("Dash", 0x7E, 1, 0, 1, 'E', "0: manual. 1: automatic."));
    f.push_back(Rng("World map display", 0xC1A, 1, 0, 2));
}

void StoryFields(std::vector<Field>& f) {
    for (unsigned i = 0; i < 8; ++i)
        f.push_back(Bit("Chapter status bit " + N(i), 0x01, i, 'A',
                        i == 7 ? "On: the next chapter starts when the party goes into an area." : ""));
    f.push_back(Num("Chapter state", 0x02, 1, 'A', "The game sets this to 1 when the party goes into an area after a load."));
    f.push_back(Num("Chapter byte", 0x03, 1, 'A', "Each chapter uses this byte differently."));
    f.push_back(Num("Chapter run", 0x04, 1, 'A'));
    f.push_back(Num("Chapter step", 0x05, 1, 'A'));
    f.push_back(Num("Chapter timer", 0x06, 2, 'A'));
    f.push_back(Num("Step count", 0x08, 2, 'A'));
    f.push_back(Num("Field tail kind", 0x13, 1, 'A', "Clear the field tail and script flag bit 6 together or not at all."));
    f.push_back(Num("Field tail state", 0x14, 1, 'A'));
    f.push_back(Num("Field tail argument", 0x15, 1, 'A'));
    f.push_back(Num("Field tail timer", 0x16, 2, 'A'));
    for (unsigned i = 0; i < 4; ++i) f.push_back(Num("Script counter " + N(i + 1), 0x20 + i, 1, 'A'));
    for (unsigned i = 0; i < 8; ++i) f.push_back(Num("Chapter scratch " + N(i + 1), 0x30 + i, 1, 'A'));
    for (unsigned i = 0; i < 8; ++i) f.push_back(Bit("Script flag bit " + N(i), 0x74D, i, 'A'));
    f.push_back(Num("Music to start after the load", 0x751, 1, 'A', "A track number, or 255 for none."));
    f.push_back(Typed("Return trip: position X", Type::kFixed, 0x768, 4, 'A'));
    f.push_back(Typed("Return trip: position Z", Type::kFixed, 0x76C, 4, 'A'));
    f.push_back(Num("Return trip: area", 0x770, 2, 'A'));
    f.push_back(Num("Return trip: away", 0x772, 1, 'A', "0 for a save that is not on a trip."));
    f.push_back(Num("Return trip: direction", 0x773, 1, 'A'));
    f.push_back(Num("Desert walk: frames", 0x67C, 2, 'A'));
    f.push_back(Num("Desert walk: count", 0x67E, 1, 'A'));
    f.push_back(Num("Desert walk: reserve", 0x67F, 1, 'A'));
    f.push_back(Bit("Mountain passes open", 0x1040, 0, 'A', "From this project, not from the original game (DIV-0191)."));
}

void CounterFields(std::vector<Field>& f) {
    f.push_back(Num("Zenny tally", 0x758, 4, 'A', "The game does not count each gain."));
    f.push_back(Num("Passage finds taken", 0x75C, 4, 'A'));
    f.push_back(Num("Chest contents taken", 0x760, 4, 'A'));
    f.push_back(Rng("Rank for area 113", 0xC14, 1, 0, 12, 'A'));
    for (unsigned i = 0; i < 9; ++i) f.push_back(Num("Battle item window memory " + N(i + 1), 0xC1C + i, 1, 'A'));
    for (unsigned i = 0; i < 3; ++i) f.push_back(Num("Item list memory " + N(i + 1), 0xC25 + i, 1, 'A'));
}

void OtherFields(std::vector<Field>& f) {
    const std::string made = "The editor makes this from the record it copies.";
    f.push_back(Typed("Summary: name, bytes 1 to 5", Type::kHex, kSummary + 0x00, 5, 'D', made));
    f.push_back(Typed("Summary: name, bytes 6 to 9", Type::kHex, kSummary + 0x16, 4, 'D', made));
    f.push_back(Typed("Summary: portraits", Type::kHex, kSummary + 0x05, 3, 'D', made));
    f.push_back(Num("Summary: level", kSummary + 0x08, 1, 'D', made));
    f.push_back(Typed("Summary: play time", Type::kClock, kSummary + 0x0C, 4, 'D', made));
    f.push_back(Num("Summary: EXP", kSummary + 0x10, 4, 'D', made));
    f.push_back(Num("Summary: game cleared", kSummary + 0x14, 1, 'D', "Event flag 146."));
    f.push_back(Typed("Checksum", Type::kHex, 0x70, 2, 'D', "The editor makes this last."));
    f.push_back(Num("Event flags seeded", 0x12, 1, 'D', "1 in each save that the game writes."));
    for (unsigned i = 0; i < 9; ++i)
        f.push_back(Typed("Button word " + N(i + 1), Type::kHex, 0xCBC + 2 * i, 2, 'P', "A copy of the button mapping at the save."));
    f.push_back(Num("Option byte +0x7C", 0x7C, 1, 'P'));
    f.push_back(Num("Option byte +0x7D", 0x7D, 1, 'P'));
    // The bytes that no code reaches (UNTOUCHED in docs/save-block.md): kept as they are.
    struct Range {
        std::uint32_t at, size;
    };
    constexpr Range untouched[] = {{0x00A, 6},  {0x010, 2},  {0x018, 2},  {0x01A, 6},  {0x026, 1},  {0x038, 56},
                                   {0x072, 2},  {0x074, 4},  {0x07F, 17}, {0x670, 8},  {0x752, 2},  {0xC15, 5},
                                   {0xC73, 1},  {0xC7A, 2},  {0xC84, 28}, {0xCAB, 1},  {0xCB5, 1},  {0xCBA, 2},
                                   {0xCCE, 2},  {0xCEF, 1},  {0x103C, 4}, {0x1041, 111}};
    for (const Range& r : untouched) {
        char label[40];
        std::snprintf(label, sizeof label, "Bytes +0x%03X (%u)", static_cast<unsigned>(r.at), static_cast<unsigned>(r.size));
        f.push_back(Typed(label, Type::kHex, r.at, r.size, 'P', "The editor keeps these bytes as they are."));
    }
}

// ---- text ------------------------------------------------------------------------

std::string Trim(const std::string& s) {
    const std::size_t b = s.find_first_not_of(" \t"), e = s.find_last_not_of(" \t");
    return b == std::string::npos ? std::string() : s.substr(b, e - b + 1);
}
bool SameText(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) return false;
    return true;
}

// A name's bytes as text. A Latin install writes a letter as its ASCII byte
// (tools/loc_build.py ASCII_OF; the double quote is 0x2A); any other byte, and
// any name of an install that is not Latin, is shown as its codes.
std::string DecodeName(const std::uint8_t* p, std::size_t size, bool latin) {
    std::size_t len = 0;
    while (len < size && p[len]) ++len;
    if (len == 0) return {};
    bool plain = latin;
    for (std::size_t i = 0; i < len; ++i)
        if (p[i] < 0x20 || p[i] > 0x7E) plain = false;
    std::string out;
    if (plain) {
        for (std::size_t i = 0; i < len; ++i) out += p[i] == 0x2A ? '"' : static_cast<char>(p[i]);
        return out;
    }
    out = "hex:";
    char two[4];
    for (std::size_t i = 0; i < len; ++i) {
        std::snprintf(two, sizeof two, "%s%02X", i ? " " : "", p[i]);
        out += two;
    }
    return out;
}

bool ParseHex(const std::string& text, std::vector<std::uint8_t>& out) {
    out.clear();
    std::size_t i = 0;
    while (i < text.size()) {
        if (text[i] == ' ' || text[i] == '\t' || text[i] == ',') {
            ++i;
            continue;
        }
        if (i + 1 >= text.size() || !std::isxdigit(static_cast<unsigned char>(text[i])) ||
            !std::isxdigit(static_cast<unsigned char>(text[i + 1])))
            return false;
        out.push_back(static_cast<std::uint8_t>(std::strtoul(text.substr(i, 2).c_str(), nullptr, 16)));
        i += 2;
    }
    return true;
}

bool EncodeName(const std::string& text, std::size_t most, bool latin, std::vector<std::uint8_t>& out, std::string& why) {
    out.clear();
    if (text.empty()) return true;   // no name: the field is zeroed
    if (text.compare(0, 4, "hex:") == 0) {
        if (!ParseHex(text.substr(4), out)) {
            why = "After hex:, write the codes as pairs of hexadecimal digits.";
            return false;
        }
    } else if (!latin) {
        why = "The editor cannot write letters for the text of this install. Write the glyph codes: hex: then pairs of "
              "hexadecimal digits.";
        return false;
    } else {
        static const char kMarks[] = " (),-./=?&':;";
        for (const char c : text) {
            if (c == '"') {
                out.push_back(0x2A);
            } else if (std::isalnum(static_cast<unsigned char>(c)) || std::strchr(kMarks, c)) {
                out.push_back(static_cast<std::uint8_t>(c));
            } else {
                why = std::string("The game font has no glyph for the character ") + c + ".";
                return false;
            }
        }
    }
    if (out.size() > most) {
        why = "The name is too long. The field holds " + std::to_string(most) + " bytes.";
        return false;
    }
    return true;
}

const std::vector<std::string>* ListOf(List list, const Names& names) {
    switch (list) {
    case List::kItem0:
    case List::kItem1:
    case List::kItem2:
    case List::kItem3:
        return &names.item[static_cast<int>(list) - static_cast<int>(List::kItem0)];
    case List::kKeyItem:
        return &names.key;
    case List::kAbility:
        return &names.ability;
    default:
        return nullptr;
    }
}

// The record that holds character `id`, or -1.
int RecordOfMember(const Block& b, unsigned id) {
    for (unsigned i = 0; i < kCharCount; ++i)
        if (b[kChars + kCharStride * i + 0x09] == id) return static_cast<int>(i);
    return -1;
}

std::string MemberName(const Block& b, unsigned id, const Names& names) {
    if (id == 0xFF) return "None";
    const int r = RecordOfMember(b, id);
    if (r < 0) return {};
    const std::string name = DecodeName(b.data() + kChars + kCharStride * static_cast<unsigned>(r), 9, names.latin);
    return name.compare(0, 4, "hex:") == 0 ? std::string() : name;
}

// The name a list gives a value, or empty.
std::string ListName(const Block& b, List list, std::uint64_t v, const Names& names) {
    if (list == List::kMember) return MemberName(b, static_cast<unsigned>(v), names);
    const std::vector<std::string>* l = ListOf(list, names);
    if (v == 0 && list != List::kNone) return "None";
    return l && v < l->size() ? (*l)[static_cast<std::size_t>(v)] : std::string();
}

std::string WithName(std::uint64_t v, const std::string& name) {
    return name.empty() ? std::to_string(v) : std::to_string(v) + " - " + name;
}

// "12", "0x0C", "12 - Dagger", or a list's name: the number.
bool ParseNumber(const Block& b, const std::string& raw, List list, const Names& names, std::int64_t& out) {
    const std::string text = Trim(raw);
    if (text.empty()) return false;
    char* end = nullptr;
    const std::int64_t v = std::strtoll(text.c_str(), &end, 0);
    if (end != text.c_str()) {
        const std::string rest = Trim(end);
        if (rest.empty() || rest[0] == '-') {
            out = v;
            return true;
        }
    }
    if (list == List::kMember) {
        if (SameText(text, "None")) {
            out = 0xFF;
            return true;
        }
        for (unsigned i = 0; i < kCharCount; ++i) {
            const unsigned id = b[kChars + kCharStride * i + 0x09];
            if (SameText(text, MemberName(b, id, names))) {
                out = id;
                return true;
            }
        }
        return false;
    }
    if (list != List::kNone && (SameText(text, "None") || SameText(text, "Empty"))) {
        out = 0;
        return true;
    }
    if (const std::vector<std::string>* l = ListOf(list, names))
        for (std::size_t i = 0; i < l->size(); ++i)
            if (!(*l)[i].empty() && SameText(text, (*l)[i])) {
                out = static_cast<std::int64_t>(i);
                return true;
            }
    return false;
}

std::string RangeWhy(const Field& f) {
    return "Write a number from " + std::to_string(f.lo) + " to " + std::to_string(f.hi) + ".";
}

// ---- the party table -----------------------------------------------------------

struct Party {
    std::uint8_t ids[3];
    unsigned count;
};
Party PartyOf(const Block& b) {
    Party p{};
    for (unsigned i = 0; i < 3; ++i) {
        p.ids[i] = b[kFieldOrder + i];
        if (p.ids[i] != 0xFF) ++p.count;
    }
    return p;
}
bool RowHolds(const std::uint8_t* row, const Party& p) {
    for (unsigned i = 0; i < 3; ++i) {
        if (p.ids[i] == 0xFF) continue;
        if (row[0] != p.ids[i] && row[1] != p.ids[i] && row[2] != p.ids[i]) return false;
    }
    return true;
}
// PartySet_Find's answer (src/game/field/mode_11.cpp): the row now set when it
// holds every member; else the row whose first columns are the members in
// order of id; -1 for none.
int PartyRow(const PartyTable& table, const Party& p, std::uint8_t current) {
    if (table.size() < kPartyRows * 3 || p.count == 0) return -1;
    if (current != 0xFF && (current & 0x7Fu) < kPartyRows && RowHolds(table.data() + (current & 0x7Fu) * 3, p))
        return current & 0x7F;
    std::uint8_t sorted[3] = {p.ids[0], p.ids[1], p.ids[2]};
    std::sort(sorted, sorted + 3);
    for (unsigned r = 0; r < kPartyRows; ++r)
        if (std::memcmp(table.data() + r * 3, sorted, p.count) == 0) return static_cast<int>(r);
    for (unsigned r = 0; r < kPartyRows; ++r)
        if (RowHolds(table.data() + r * 3, p)) return static_cast<int>(r);
    return -1;
}
bool SameMembers(const Block& a, const Block& b) {
    std::uint8_t x[3], y[3];
    std::memcpy(x, a.data() + kFieldOrder, 3);
    std::memcpy(y, b.data() + kFieldOrder, 3);
    std::sort(x, x + 3);
    std::sort(y, y + 3);
    return std::memcmp(x, y, 3) == 0;
}

// Every battle-count stamp of the faerie village (docs/save-block/range-e-tail.md).
std::vector<std::uint32_t> Stamps(const Block& b, bool live_only) {
    std::vector<std::uint32_t> at = {0xCD4, 0xCD8, 0xCDC, 0xCE0, 0xCE4};
    for (unsigned i = 0; i < kFaerieCount; ++i)
        if (!live_only || b[kFaeries + 8 * i]) at.push_back(kFaeries + 8 * i + 4);
    for (unsigned i = 0; i < kBuildingCount; ++i)
        if (!live_only || b[kBuildings + 8 * i]) at.push_back(kBuildings + 8 * i + 4);
    return at;
}

bool EventFlag(const Block& b, unsigned n) { return (b[kEventFlags + n / 8] >> (n % 8)) & 1; }

void AllIssues(const Block& b, const PartyTable& table, const Block* before, std::vector<Issue>& out) {
    const auto error = [&out](std::string text) { out.push_back({true, std::move(text)}); };
    const auto warn = [&out](std::string text) { out.push_back({false, std::move(text)}); };

    if (b[0x00] > 19 || b[0x00] == 16) error("Chapter must be 0 to 19, and not 16.");
    if (b[0x78] > 2) error("Message speed must be 0 to 2.");
    if (b[0x7A] > 3) error("Window color must be 0 to 3.");
    if (b[0x7B] > 3) error("Window background must be 0 to 3.");
    if (Get(b, kClock, 4) == 0) error("Play time must not be all zero. The game reads that as a new game.");

    // The party lists.
    const Party p = PartyOf(b);
    bool party_ok = p.count > 0 && p.ids[0] != 0xFF && !(p.ids[1] == 0xFF && p.ids[2] != 0xFF);
    for (unsigned i = 0; i < 3 && party_ok; ++i)
        for (unsigned j = i + 1; j < 3; ++j)
            if (p.ids[i] != 0xFF && p.ids[i] == p.ids[j]) party_ok = false;
    if (!party_ok) {
        error("Field order must have 1 to 3 different members, with the empty places last.");
    } else {
        for (unsigned i = 0; i < 3; ++i) {
            if (p.ids[i] == 0xFF) continue;
            const int r = RecordOfMember(b, p.ids[i]);
            if (r < 0)
                error("Field order has member " + N(p.ids[i]) + ", but no character record holds that member.");
            else if (!(b[kChars + kCharStride * static_cast<unsigned>(r) + 0x0B] & 1))
                warn("Member " + N(p.ids[i]) + " is in the party, but its record does not have the Joined bit.");
        }
        if (before && !SameMembers(*before, b)) {
            if (table.empty())
                error("The party members changed, but this install has no BOF3.exe from which the editor reads the "
                      "party table. Keep the same members. You can change their order.");
            else if (PartyRow(table, p, b[kPartySet]) < 0)
                error("The game has no party with these members.");
        }
        const unsigned limit = p.count == 1 ? 1 : p.count == 2 ? 3 : 6;
        if (b[0x680] >= limit) error("Formation " + N(b[0x680]) + " is not possible with " + N(p.count) + " member(s).");
    }
    if (b[0x680] < 8 && !((b[0x681] >> b[0x680]) & 1)) error("Formation " + N(b[0x680]) + " is not unlocked.");

    // The gene combinations: packed from the start, FF after the last, an empty row all FF.
    for (unsigned i = 0; i < kGeneRowCount; ++i) {
        const std::uint8_t* r = b.data() + kGeneRows + 4 * i;
        const bool gap = (r[0] == 0xFF && (r[1] != 0xFF || r[2] != 0xFF)) || (r[1] == 0xFF && r[2] != 0xFF);
        const bool bad = gap || (r[0] == 0xFF) != (r[3] == 0xFF);
        if (bad) error("Gene combination row " + N(i + 1) + " is not a correct row. An empty row is FF FF FF FF.");
    }
    if (b[0xC73] != 0) error("The byte after the gene bits (+0xC73) must be 0.");
    if (b[0x70B] & 0x80) error("The last chest flag (255) must be Off. On, each object that has no flag reads as taken.");
    if (b[0x74B] & 0x80) error("The last passage flag (255) must be Off. On, each passage that has no flag reads as taken.");

    const std::uint64_t battles = Get(b, kBattleCount, 4);
    unsigned above = 0;
    for (const std::uint32_t at : Stamps(b, false))
        if (Get(b, at, 4) > battles) ++above;
    if (above) error(N(above) + " faerie village stamp(s) are above the battle count.");

    if ((b[0x13] != 0) != ((b[0x74D] & 0x40) != 0))
        warn("The field tail and script flag bit 6 do not agree. Clear them together or not at all.");

    for (unsigned i = 0; i < kCharCount; ++i) {
        const std::uint32_t c = kChars + kCharStride * i;
        const std::string who = "Record " + N(i + 1) + ": ";
        if (b[c + 0x88] > b[c + 0x0A]) warn(who + "the level at apprenticing is above the level.");
        if (b[c + 0x1F] == 0xFF && b[c + 0x88] != 0) warn(who + "there is a level at apprenticing but no master.");
        if (Get(b, c + 0x1A, 2) > Get(b, c + 0x22, 2)) warn(who + "AP is above the maximum AP.");
        if (b[c + 0x1E] > 9) warn(who + "fatigue is above 9.");
    }

    // One stack for each item.
    for (unsigned k = 0; k < 4; ++k) {
        bool seen[256] = {};
        for (unsigned i = 0; i < 128; ++i) {
            const std::uint8_t id = b[0x774 + 0x80 * k + i], count = b[0x974 + 0x80 * k + i];
            if (id == 0) continue;
            if (seen[id]) warn(std::string(kItemCategory[k]) + ": item " + N(id) + " is in two places. The game keeps one stack for each item.");
            seen[id] = true;
            if (count < 1 || count > 99) error(std::string(kItemCategory[k]) + ", place " + N(i + 1) + ": the count must be 1 to 99.");
        }
    }
    if (b[0xB74 + 31] != 0) warn("All 32 key item places are full. The game puts the next key item outside the list.");
}

}  // namespace

// ---- the block --------------------------------------------------------------------

std::uint16_t Checksum(const Block& block) {
    std::uint32_t sum = 0;
    for (std::size_t i = 0; i < kBlockSize; ++i)
        if (i != 0x70 && i != 0x71) sum += block[i];
    return static_cast<std::uint16_t>(sum);
}

std::uint16_t StoredChecksum(const Block& block) { return static_cast<std::uint16_t>(Get(block, 0x70, 2)); }

const std::vector<Section>& Sections() {
    static const std::vector<Section> sections = {
        {"Game", 1, false},
        {"Party", 1, false},
        {"Characters", static_cast<int>(kCharCount), false},
        {"Inventory", 4, false},
        {"Key items", 1, false},
        {"Party skills", 1, false},
        {"Dragon genes", 1, false},
        {"Masters", 1, false},
        {"Fishing", 1, false},
        {"Faerie village", 1, false},
        {"Faeries", static_cast<int>(kFaerieCount), false},
        {"Faerie buildings", static_cast<int>(kBuildingCount), false},
        {"Options", 1, false},
        {"Story state", 1, true},
        {"Chapter flags", 20, true},
        {"Event flags", 1, true},
        {"Records and finds", 5, true},
        {"Counters", 1, true},
        {"Other bytes", 1, true},
    };
    return sections;
}

std::vector<Field> SectionFields(int section, int record) {
    std::vector<Field> f;
    const unsigned r = record < 0 ? 0u : static_cast<unsigned>(record);
    switch (section) {
    case kSecGame:
        GameFields(f);
        break;
    case kSecParty:
        PartyFields(f);
        break;
    case kSecCharacters:
        if (r < kCharCount) CharacterFields(f, r);
        break;
    case kSecItems:
        if (r < 4)
            for (unsigned i = 0; i < 128; ++i) {
                Field s = Typed("Place " + N(i + 1), Type::kStack, 0x774 + 0x80 * r + i, 1, 'E',
                                "Write an item and a count, as in: 12 x 3. Write 0 for an empty place.");
                s.at2 = 0x974 + 0x80 * r + i;
                s.list = static_cast<List>(static_cast<int>(List::kItem0) + static_cast<int>(r));
                s.hi = 255;
                f.push_back(std::move(s));
            }
        break;
    case kSecKeyItems:
        for (unsigned i = 0; i < 32; ++i)
            f.push_back(Lst("Place " + N(i + 1), 0xB74 + i, List::kKeyItem, 'E', "Do not fill all 32 places."));
        break;
    case kSecAbilities:
        for (unsigned i = 0; i < 128; ++i)
            f.push_back(Lst("Place " + N(i + 1), 0xB94 + i, List::kAbility, 'E',
                            "Keep an ability here or in the list of a member, not in the two."));
        break;
    case kSecDragon:
        DragonFields(f);
        break;
    case kSecMasters:
        MasterFields(f);
        break;
    case kSecFishing:
        FishingFields(f);
        break;
    case kSecVillage:
        VillageFields(f);
        break;
    case kSecFaeries:
        if (r < kFaerieCount) FaerieFields(f, r);
        break;
    case kSecBuildings:
        if (r < kBuildingCount) BuildingFields(f, r);
        break;
    case kSecOptions:
        OptionFields(f);
        break;
    case kSecStory:
        StoryFields(f);
        break;
    case kSecChapterFlags:
        if (r < 20)
            for (unsigned i = 0; i < 64; ++i) f.push_back(BankBit("Flag " + N(i), kChapterFlags + 8 * r, i, 'A'));
        break;
    case kSecEventFlags:
        for (unsigned i = 0; i < 256; ++i)
            f.push_back(BankBit("Event flag " + N(i), kEventFlags, i, 'A', i == 146 ? "The game is cleared." : ""));
        break;
    case kSecBanks:
        if (r < 5)
            for (unsigned i = 0; i < 256; ++i) f.push_back(BankBit("Bit " + N(i), kBankAt[r], i, 'A'));
        break;
    case kSecCounters:
        CounterFields(f);
        break;
    case kSecOther:
        OtherFields(f);
        break;
    default:
        break;
    }
    return f;
}

std::string RecordLabel(const Block& block, int section, int record, const Names& names) {
    const unsigned r = static_cast<unsigned>(record);
    switch (section) {
    case kSecCharacters: {
        const std::string name = DecodeName(block.data() + kChars + kCharStride * r, 9, names.latin);
        return "Record " + N(r + 1) + (name.empty() ? "" : " - " + name);
    }
    case kSecItems:
        return kItemCategory[r & 3];
    case kSecFaeries: {
        const std::string name = DecodeName(block.data() + kFaerieNames + 5 * r, 5, names.latin);
        return "Faerie " + N(r + 1) + (name.empty() ? "" : " - " + name) + (block[kFaeries + 8 * r] ? "" : " (not in the village)");
    }
    case kSecBuildings:
        return "Building " + N(r + 1);
    case kSecChapterFlags:
        return "Chapter " + N(r);
    case kSecBanks:
        return kBankName[r < 5 ? r : 0];
    default:
        return {};
    }
}

bool FieldReadOnly(const Field& field) { return field.cls == 'D' || field.cls == 'P'; }

std::string Describe(const Block& block, const Names& names) {
    std::string name = DecodeName(block.data() + kChars, 9, names.latin);
    if (name.empty() || name.compare(0, 4, "hex:") == 0) name = "Save";
    char rest[64];
    std::snprintf(rest, sizeof rest, ", level %u, %u:%02u:%02u", block[kChars + 0x0A], block[kClock], block[kClock + 1],
                  block[kClock + 2]);
    return name + rest;
}

std::string FieldText(const Block& block, const Field& f, const Names& names) {
    char text[64];
    switch (f.type) {
    case Type::kUnsigned: {
        const std::uint64_t v = Get(block, f.at, f.size);
        return f.list == List::kNone ? std::to_string(v) : WithName(v, ListName(block, f.list, v, names));
    }
    case Type::kSigned:
        return std::to_string(GetSigned(block, f.at, f.size));
    case Type::kBit:
        return (block[f.at] >> f.size) & 1 ? "On" : "Off";
    case Type::kName:
        return DecodeName(block.data() + f.at, f.size, names.latin);
    case Type::kClock:
        std::snprintf(text, sizeof text, "%u:%02u:%02u.%02u", block[f.at], block[f.at + 1], block[f.at + 2], block[f.at + 3]);
        return text;
    case Type::kFixed: {
        std::snprintf(text, sizeof text, "%.6f", static_cast<double>(GetSigned(block, f.at, 4)) / 65536.0);
        std::string s = text;
        while (s.back() == '0') s.pop_back();
        if (s.back() == '.') s.pop_back();
        return s;
    }
    case Type::kHex: {
        std::string s;
        for (std::uint32_t i = 0; i < f.size; ++i) {
            std::snprintf(text, sizeof text, "%s%02X", i ? " " : "", block[f.at + i]);
            s += text;
        }
        return s;
    }
    case Type::kStack: {
        const std::uint64_t id = block[f.at];
        if (id == 0) return "Empty";
        return WithName(id, ListName(block, f.list, id, names)) + " x " + std::to_string(block[f.at2]);
    }
    }
    return {};
}

bool FieldSet(Block& block, const Field& f, const std::string& raw, const Names& names, std::string& why) {
    if (FieldReadOnly(f)) {
        why = "You cannot change this field.";
        return false;
    }
    const std::string text = Trim(raw);
    switch (f.type) {
    case Type::kUnsigned:
    case Type::kSigned: {
        std::int64_t v = 0;
        if (!ParseNumber(block, text, f.list, names, v)) {
            why = f.list == List::kNone ? RangeWhy(f) : "Write a number or a name from the list.";
            return false;
        }
        if (v < f.lo || v > f.hi) {
            why = RangeWhy(f);
            return false;
        }
        Put(block, f.at, f.size, static_cast<std::uint64_t>(v));
        return true;
    }
    case Type::kBit: {
        bool on;
        if (SameText(text, "on") || text == "1" || SameText(text, "yes")) on = true;
        else if (SameText(text, "off") || text == "0" || SameText(text, "no")) on = false;
        else {
            why = "Write On or Off.";
            return false;
        }
        const std::uint8_t mask = static_cast<std::uint8_t>(1u << f.size);
        block[f.at] = static_cast<std::uint8_t>(on ? block[f.at] | mask : block[f.at] & ~mask);
        return true;
    }
    case Type::kName: {
        std::vector<std::uint8_t> bytes;
        if (!EncodeName(text, static_cast<std::size_t>(f.hi), names.latin, bytes, why)) return false;
        std::memset(block.data() + f.at, 0, f.size);
        std::memcpy(block.data() + f.at, bytes.data(), bytes.size());
        return true;
    }
    case Type::kClock: {
        unsigned h = 0, m = 0, s = 0, fr = 0;
        const int n = std::sscanf(text.c_str(), "%u:%u:%u.%u", &h, &m, &s, &fr);
        if (n < 3 || h > 255 || m > 59 || s > 59 || fr > 59) {
            why = "Write hours:minutes:seconds, as in 12:34:56.";
            return false;
        }
        block[f.at] = static_cast<std::uint8_t>(h);
        block[f.at + 1] = static_cast<std::uint8_t>(m);
        block[f.at + 2] = static_cast<std::uint8_t>(s);
        block[f.at + 3] = static_cast<std::uint8_t>(fr);
        return true;
    }
    case Type::kFixed: {
        char* end = nullptr;
        const double v = std::strtod(text.c_str(), &end);
        if (end == text.c_str() || !Trim(end).empty() || v < -32768.0 || v >= 32768.0) {
            why = "Write a number from -32768 to 32767. It can have a fraction.";
            return false;
        }
        const double scaled = v * 65536.0;
        Put(block, f.at, 4, static_cast<std::uint64_t>(static_cast<std::int64_t>(scaled < 0 ? scaled - 0.5 : scaled + 0.5)));
        return true;
    }
    case Type::kHex: {
        std::vector<std::uint8_t> bytes;
        if (!ParseHex(text, bytes) || bytes.size() != f.size) {
            why = "Write " + std::to_string(f.size) + " bytes as pairs of hexadecimal digits.";
            return false;
        }
        std::memcpy(block.data() + f.at, bytes.data(), f.size);
        return true;
    }
    case Type::kStack: {
        std::string item = text;
        std::int64_t count = block[f.at2] ? block[f.at2] : 1;
        const std::size_t x = text.rfind(" x ");
        if (x != std::string::npos) {
            const std::string tail = Trim(text.substr(x + 3));
            if (!tail.empty() && tail.find_first_not_of("0123456789") == std::string::npos) {
                count = std::atoi(tail.c_str());
                item = text.substr(0, x);
            }
        }
        std::int64_t id = 0;
        if (!ParseNumber(block, item, f.list, names, id) || id < 0 || id > 255) {
            why = "Write an item number or name, then x and a count, as in: 12 x 3. Write 0 for an empty place.";
            return false;
        }
        if (id == 0) {
            block[f.at] = 0;
            block[f.at2] = 0;
            return true;
        }
        if (count < 1 || count > 99) {
            why = "The count must be 1 to 99.";
            return false;
        }
        block[f.at] = static_cast<std::uint8_t>(id);
        block[f.at2] = static_cast<std::uint8_t>(count);
        return true;
    }
    }
    return false;
}

std::vector<std::string> FieldChoices(const Block& block, const Field& f, const Names& names) {
    std::vector<std::string> out;
    if (FieldReadOnly(f)) return out;
    if (f.type == Type::kBit) return {"On", "Off"};
    if (f.type != Type::kUnsigned && f.type != Type::kStack) return out;
    if (f.list == List::kMember) {
        for (unsigned i = 0; i < kCharCount; ++i) {
            const unsigned id = block[kChars + kCharStride * i + 0x09];
            out.push_back(WithName(id, MemberName(block, id, names)));
        }
        out.push_back("255 - None");
        return out;
    }
    const std::vector<std::string>* l = ListOf(f.list, names);
    if (!l) return out;
    out.push_back(f.type == Type::kStack ? "0 - Empty" : "0 - None");
    for (std::size_t i = 1; i < l->size(); ++i)
        if (!(*l)[i].empty()) out.push_back(WithName(i, (*l)[i]));
    return out;
}

void Finish(const Block& before, Block& now, const PartyTable& table) {
    // The party: the battle order holds the field order's members, and the
    // party row is the one the game would choose for them.
    if (!Same(before, now, kFieldOrder, 3) || !Same(before, now, kBattleOrder, 3)) {
        std::uint8_t f[3], bt[3];
        std::memcpy(f, now.data() + kFieldOrder, 3);
        std::memcpy(bt, now.data() + kBattleOrder, 3);
        std::sort(f, f + 3);
        std::sort(bt, bt + 3);
        if (std::memcmp(f, bt, 3) != 0) std::memcpy(now.data() + kBattleOrder, now.data() + kFieldOrder, 3);
    }
    if (!SameMembers(before, now)) {
        const int row = PartyRow(table, PartyOf(now), now[kPartySet]);
        if (row >= 0) now[kPartySet] = static_cast<std::uint8_t>(row | 0x80);
    }

    // A change of the battle count moves every stamp with it.
    const std::int64_t was = static_cast<std::int64_t>(Get(before, kBattleCount, 4));
    const std::int64_t is = static_cast<std::int64_t>(Get(now, kBattleCount, 4));
    if (was != is)
        for (const std::uint32_t at : Stamps(now, true)) {
            const std::int64_t moved = static_cast<std::int64_t>(Get(now, at, 4)) + (is - was);
            Put(now, at, 4, static_cast<std::uint64_t>(std::clamp<std::int64_t>(moved, 0, is)));
        }

    // The slot summary (the spec's section 7): each part where its source changed.
    const std::uint32_t r0 = kChars;
    if (!Same(before, now, r0, 9)) {
        std::memcpy(now.data() + kSummary + 0x00, now.data() + r0, 5);
        std::memcpy(now.data() + kSummary + 0x16, now.data() + r0 + 5, 4);
    }
    if (!Same(before, now, r0 + 0x0A, 1)) now[kSummary + 0x08] = now[r0 + 0x0A];
    if (!Same(before, now, r0 + 0x0C, 4)) std::memcpy(now.data() + kSummary + 0x10, now.data() + r0 + 0x0C, 4);
    if (!Same(before, now, kClock, 4)) std::memcpy(now.data() + kSummary + 0x0C, now.data() + kClock, 4);
    if (!Same(before, now, 0x7A, 1)) now[kSummary + 0x09] = now[0x7A];
    if (!Same(before, now, 0x7B, 1)) now[kSummary + 0x0A] = now[0x7B];
    if (EventFlag(before, 146) != EventFlag(now, 146)) now[kSummary + 0x14] = EventFlag(now, 146) ? 1 : 0;
    if (!Same(before, now, kFieldOrder, 3) || !Same(before, now, kPartySet, 1)) {
        const unsigned set = now[kPartySet] & 0x7Fu;
        for (unsigned i = 0; i < 3; ++i) {
            const std::uint8_t id = now[kFieldOrder + i];
            now[kSummary + 0x05 + i] = id == 4 && set != 5 && set != 0xC ? 0x0B : id;
        }
    }
    Put(now, 0x70, 2, Checksum(now));
}

std::vector<Issue> Check(const Block& before, const Block& now, const PartyTable& table) {
    std::vector<Issue> was, is, out;
    AllIssues(before, table, nullptr, was);
    AllIssues(now, table, &before, is);
    for (Issue& i : is) {
        const bool old = std::any_of(was.begin(), was.end(), [&i](const Issue& w) { return w.text == i.text; });
        if (!old) out.push_back(std::move(i));
    }
    return out;
}

// ---- the PC file --------------------------------------------------------------------

bool ReadPcFile(const std::wstring& path, Block& block, std::string& why) {
    std::ifstream in(std::filesystem::path(path), std::ios::binary);
    if (!in) {
        why = "The file cannot be opened.";
        return false;
    }
    std::vector<char> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (bytes.size() == 0x20000 && bytes[0] == 'M' && bytes[1] == 'C') {
        why = "This is a memory card file. This version of the editor opens the save files of the PC game only.";
        return false;
    }
    if (bytes.size() != kPcFileSize) {
        why = "This is not a save file of the PC game: its size is " + std::to_string(bytes.size()) +
              " bytes, not 4784.";
        return false;
    }
    std::memcpy(block.data(), bytes.data(), kBlockSize);
    return true;
}

bool WritePcFile(const std::wstring& path, const Block& block, const std::wstring& backup_dir, std::wstring& backup,
                 std::string& why) {
    namespace fs = std::filesystem;
    backup.clear();
    std::vector<char> bytes(kPcFileSize, 0);
    std::memcpy(bytes.data(), block.data(), kBlockSize);
    const fs::path target(path), temp(path + L".tmp");
    std::error_code ec;
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        out.flush();
        if (!out) {
            why = "The editor cannot write in the folder of the save file.";
            fs::remove(temp, ec);
            return false;
        }
    }
    {
        std::ifstream in(temp, std::ios::binary);
        const std::vector<char> back((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        if (back != bytes) {
            why = "The file that the editor wrote does not read back correctly. The save file did not change.";
            fs::remove(temp, ec);
            return false;
        }
    }
    if (fs::exists(target, ec)) {
        fs::create_directories(fs::path(backup_dir), ec);
        const std::time_t t = std::time(nullptr);
        std::tm tm{};
        if (const std::tm* local = std::localtime(&t)) tm = *local;
        wchar_t stamp[32];
        std::wcsftime(stamp, 32, L"-%Y%m%d-%H%M%S", &tm);
        fs::path copy = fs::path(backup_dir) / (target.stem().wstring() + stamp + target.extension().wstring());
        for (int n = 2; fs::exists(copy, ec) && n < 100; ++n)
            copy = fs::path(backup_dir) / (target.stem().wstring() + stamp + L"-" + std::to_wstring(n) + target.extension().wstring());
        fs::copy_file(target, copy, fs::copy_options::none, ec);
        if (ec) {
            why = "The editor cannot keep a copy of the save file in the save-backups folder. The save file did not "
                  "change.";
            fs::remove(temp, ec);
            return false;
        }
        backup = copy.wstring();
    }
    fs::rename(temp, target, ec);
    if (ec) {
        why = "The editor cannot replace the save file. The save file did not change.";
        fs::remove(temp, ec);
        return false;
    }
    return true;
}

}  // namespace bof3x::save

#include "game/solo/solo_level.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/records.h"
#include "hook/log.h"

// The level-1 start (solo_level.h). What each address and offset is:
//
//   CharacterRecords    0x903A70  eight records of 0xA4 (records.h CharRecord)
//   Char_DefaultRecords 0x64B390  seven default records of 0xA4, the ones
//                                 NewGame_InitCharacters 0x437820 copies over
//                                 records 0..6 (battle.cpp)
//   Char_ExpTable       0x658F48  99 rows of 8 bytes per record index, row
//                                 99 r + L the step L -> L + 1 (char.cpp
//                                 Char_LevelUp, symbols.toml)
//
//   the record is records.h CharRecord: level +0x0A, exp +0x0C, hp +0x18, ap
//   +0x1A, master +0x1F (0xFF none), stats (+0x20, the effective block: max HP,
//   max AP), base (+0x40, the six base words), abilities +0x60..+0x87,
//   master_level +0x88, bias +0x89..+0x8E. Willpower is one byte
//   NewGame_InitCharacters derives into three places: +0x1C (CharRecord's
//   `survive`), +0x2E (stats.byte_0e) and +0x4E (base.byte_0e) (char.cpp;
//   range-b 1 of docs/save-block/range-b-characters.md; M10 section 1).

namespace solo {
namespace {

using move_script::At;
using move_script::Word;
using records::CharRecord;
using records::CharStatBlock;

constexpr unsigned kStride = 0xA4;
constexpr int kDefaultCount = 7;   // NewGame_InitCharacters copies records 0..6
constexpr int kSlotCount = 8;      // CharacterRecords' records
constexpr unsigned kRowsPerRecord = 99;
constexpr unsigned kRowSize = 8;
constexpr unsigned kMaxLevel = 99;
constexpr unsigned kStatCap = 999;  // Char_LevelUp's Grow caps a word at 999

// Byte offsets the whole-record loops compare by (CheckIsDefault, the
// self-test's byte compare): the three willpower bytes, current HP and AP,
// and the effective block's span.
constexpr unsigned kWillCurrent = offsetof(CharRecord, survive);
constexpr unsigned kWillEffective = offsetof(CharRecord, stats) + offsetof(CharStatBlock, byte_0e);
constexpr unsigned kWillBase = offsetof(CharRecord, base) + offsetof(CharStatBlock, byte_0e);
constexpr unsigned kHpOffset = offsetof(CharRecord, hp);
constexpr unsigned kStatsFrom = offsetof(CharRecord, stats);
constexpr unsigned kStatsTo = offsetof(CharRecord, base);
constexpr unsigned kBiasOffset = offsetof(CharRecord, bias);
constexpr unsigned kListSize = 10;
static_assert(sizeof(CharRecord) == kStride, "CharRecord is the record's stride");
static_assert(kWillCurrent == 0x1C && kWillEffective == 0x2E && kWillBase == 0x4E, "the willpower bytes");

// The six base words in the order the level table's gains and the bias bytes
// name them: max HP, max AP, power, defence, agility, intelligence.
constexpr std::uint16_t CharStatBlock::*kBaseWords[6] = {&CharStatBlock::max_hp, &CharStatBlock::max_ap, &CharStatBlock::atk,
                                                         &CharStatBlock::def,    &CharStatBlock::agi,    &CharStatBlock::intel};

constexpr const char* kStatNames[6] = {"HP", "AP", "power", "defence", "agility", "intelligence"};

CharRecord& Slot(int record) { return CharacterRecords[record]; }
// Char_DefaultRecords' records, laid end to end from 0x64B390 (symbols.toml
// types only the first as bytes; NewGame_InitCharacters copies n = 0..6).
const CharRecord& Default(int record) {
    return reinterpret_cast<const CharRecord*>(Char_DefaultRecords)[record];
}
unsigned char* Bytes(CharRecord& r) { return reinterpret_cast<unsigned char*>(&r); }
const unsigned char* Bytes(const CharRecord& r) { return reinterpret_cast<const unsigned char*>(&r); }
const unsigned char* Row(int record, unsigned level) {
    return At(::bof3::Img(bof3::addr::Char_ExpTable)) + (static_cast<unsigned>(record) * kRowsPerRecord + level) * kRowSize;
}

// Row `row`'s six gains, in the base words' order (Char_LevelUp's Grow calls).
void Gains(const unsigned char* row, int out[6]) {
    out[0] = row[2];
    out[1] = row[3];
    out[2] = row[4] >> 4;
    out[3] = row[4] & 0xF;
    out[4] = row[5] >> 4;
    out[5] = row[5] & 0xF;
}

bool IsWillpowerByte(unsigned offset) { return offset == kWillCurrent || offset == kWillEffective || offset == kWillBase; }

// The default record's level N and the six level-1 base words, derived; every
// precondition of the inverse checked (solo_level.h, Solo_LevelReset).
unsigned Derive(int record, int derived[6], const char* who) {
    const CharRecord& d = Default(record);
    const unsigned n = d.level;
    if (n < 1 || n > kMaxLevel)
        bof3::Fatal("%s: default record %d's level is %u, not 1..99 (Char_DefaultRecords 0x64B390 as laid)", who, record, n);
    if (d.master != 0xFF || d.master_level != 0)
        bof3::Fatal("%s: default record %d has a master (+0x1F %u, +0x88 %u): its growth is not the bare "
                    "table's, so the levels cannot be subtracted",
                    who, record, d.master, d.master_level);
    for (unsigned s = 0; s < 6; ++s)
        if (d.bias[s] != 0)
            bof3::Fatal("%s: default record %d has a growth bias %d on %s (+0x%X): the levels cannot be "
                        "subtracted",
                        who, record, d.bias[s], kStatNames[s], kBiasOffset + s);
    for (unsigned s = 0; s < 6; ++s) {
        const unsigned v = d.base.*kBaseWords[s];
        if (v >= kStatCap)
            bof3::Fatal("%s: default record %d's base %s is %u: Char_LevelUp caps a word at 999, so what "
                        "it added cannot be told",
                        who, record, kStatNames[s], v);
        derived[s] = static_cast<int>(v);
    }
    for (unsigned level = 1; level < n; ++level) {
        int g[6];
        Gains(Row(record, level), g);
        for (unsigned s = 0; s < 6; ++s) derived[s] -= g[s];
    }
    for (unsigned s = 0; s < 6; ++s)
        if (derived[s] < 0)
            bof3::Fatal("%s: record %d's level-1 base %s derives to %d (the default's %u less rows 1..%u of "
                        "Char_ExpTable 0x658F48 as laid): the table does not fit the default record",
                        who, record, kStatNames[s], derived[s], static_cast<unsigned>(d.base.*kBaseWords[s]), n - 1);
    return n;
}

// The live record against the default, all 0xA4 bytes but the three willpower
// bytes NewGame_InitCharacters derives.
void CheckIsDefault(int record) {
    const unsigned char* const live = Bytes(Slot(record));
    const unsigned char* const d = Bytes(Default(record));
    for (unsigned o = 0; o < kStride; ++o) {
        if (IsWillpowerByte(o)) continue;
        if (live[o] != d[o])
            bof3::Fatal("solo level-1 start: record %d is not its default record at +0x%02X (%u, the default %u). It runs "
                        "only at New Game, right after NewGame_InitCharacters 0x437820, where the record is the default "
                        "by construction",
                        record, o, live[o], d[o]);
    }
}

// One copy of `id`, the last, out of its list in the record at `rec` (the
// live slot at New Game, a copy of the default at the launch check); the
// entries after it move up one and the last slot becomes 0. The list is the
// one AbilityList_ForType 0x591EC0 picks by the id's type (pointer arithmetic
// on the slot and the type table; it reads no record byte). A Fatal when the
// list does not hold it.
void RemoveAbility(unsigned char* rec, int record, unsigned id, unsigned level, const char* who) {
    const unsigned off = static_cast<unsigned>(::AbilityList_ForType(static_cast<unsigned>(record), id, 0) - Bytes(Slot(record)));
    unsigned char* const list = rec + off;
    for (int i = static_cast<int>(kListSize) - 1; i >= 0; --i) {
        if (list[i] != id) continue;
        for (unsigned j = static_cast<unsigned>(i); j + 1 < kListSize; ++j) list[j] = list[j + 1];
        list[kListSize - 1] = 0;
        return;
    }
    bof3::Fatal("%s: record %d's row %u files ability %u, which its list (record +0x%X) does not hold: "
                "the default record was not built by this table",
                who, record, level, id, off);
}

// The abilities of rows N-1 down to 1 out of the record at `rec`, each row's
// +7 before its +6: the reverse of the order Char_LevelUp files them in, so
// with each removal taking the last copy, a list built by that filing is
// unbuilt exactly.
void RemoveRowAbilities(unsigned char* rec, int record, unsigned n, const char* who) {
    for (unsigned level = n - 1; level >= 1; --level) {
        const unsigned char* const row = Row(record, level);
        if (row[7] != 0) RemoveAbility(rec, record, row[7], level, who);
        if (row[6] != 0) RemoveAbility(rec, record, row[6], level, who);
    }
}

constexpr const char* kAtNewGame = "solo level-1 start";

void LogRecord(const char* when, int record) {
    const CharRecord& r = Slot(record);
    bof3::Log("DIV-0228 SoloCharacter level-1 start, record %d %s: level %u, EXP %d, base HP %u AP %u power %u defence %u "
              "agility %u intelligence %u, HP %u/%u, AP %u/%u, willpower +0x4E %u +0x1C %u +0x2E %u",
              record, when, r.level, static_cast<int>(r.exp), r.base.max_hp, r.base.max_ap, r.base.atk, r.base.def,
              r.base.agi, r.base.intel, r.hp, r.stats.max_hp, r.ap, r.stats.max_ap, r.base.byte_0e, r.survive,
              r.stats.byte_0e);
}

}  // namespace

void Solo_LevelReset(int record, bool log) {
    if (record < 0 || record >= kDefaultCount)
        bof3::Fatal("solo level-1 start: record %d has no default record (0..6)", record);
    CheckIsDefault(record);
    int derived[6];
    const unsigned n = Derive(record, derived, kAtNewGame);
    if (log) LogRecord("before", record);
    if (n == 1) {
        if (log) bof3::Log("DIV-0228 SoloCharacter level-1 start, record %d: its default is level 1, left as it is", record);
        return;
    }
    RemoveRowAbilities(Bytes(Slot(record)), record, n, kAtNewGame);
    CharRecord& r = Slot(record);
    r.level = 1;
    r.exp = 0;
    for (unsigned s = 0; s < 6; ++s) r.base.*kBaseWords[s] = static_cast<std::uint16_t>(derived[s]);
    ::Char_RecalcStats(Bytes(r));
    r.hp = r.stats.max_hp;
    r.ap = r.stats.max_ap;
    if (log) LogRecord("after", record);
}

void Solo_LevelCheckTables(int record) {
    constexpr const char* kAtLaunch =
        "solo level-1 start, checked at launch (SoloCharacter is on and BOF3X_SOLO names this champion, so a New Game "
        "takes the record to level 1 from the tables as they are now)";
    if (record < 0 || record >= kDefaultCount)
        bof3::Fatal("%s: record %d has no default record (0..6)", kAtLaunch, record);
    int derived[6];
    const unsigned n = Derive(record, derived, kAtLaunch);
    if (n == 1) return;
    unsigned char copy[kStride];
    std::memcpy(copy, Bytes(Default(record)), kStride);
    RemoveRowAbilities(copy, record, n, kAtLaunch);
}

int Solo_LevelSelfTest() {
    int checks = 0;
    unsigned char saved[kSlotCount * kStride];
    std::memcpy(saved, Bytes(Slot(0)), sizeof saved);
    // The New Game state itself: the game's own copy of the default records
    // and their willpower.
    ::NewGame_InitCharacters();
    unsigned char state[kSlotCount * kStride];
    std::memcpy(state, Bytes(Slot(0)), sizeof state);
    const CharRecord* const states = reinterpret_cast<const CharRecord*>(state);
    for (int record = 0; record < kDefaultCount; ++record) {
        CharRecord& r = Slot(record);
        const CharRecord& d = states[record];
        std::memcpy(Bytes(Slot(0)), state, sizeof state);
        int derived[6];
        const unsigned n = Derive(record, derived, "solo level self-test");
        // The launch check passes on the tables as laid, and leaves the
        // records it reads as they were (it works on a copy).
        Solo_LevelCheckTables(record);
        if (std::memcmp(Bytes(Slot(0)), state, sizeof state) != 0)
            bof3::Fatal("solo level self-test: the launch check of record %d changed a record slot", record);
        ++checks;
        for (unsigned s = 0; s < 6; ++s) {
            if (derived[s] < 0) bof3::Fatal("solo level self-test: record %d's %s derives negative", record, kStatNames[s]);
            ++checks;
        }
        // The EXP lies in the default level's window (Char_LevelUp's own sum).
        std::int32_t below = 0;
        for (unsigned level = 1; level < n; ++level) below += Word(Row(record, level));
        const std::int32_t exp = d.exp;
        if (exp < below || (n < kMaxLevel && exp >= below + Word(Row(record, n))))
            bof3::Fatal("solo level self-test: record %d's default EXP %d is not in level %u's window", record, exp, n);
        ++checks;

        Solo_LevelReset(record, false);
        if (r.level != 1 || r.exp != 0)
            bof3::Fatal("solo level self-test: record %d after the reset is level %u EXP %d", record, r.level,
                        static_cast<int>(r.exp));
        for (unsigned s = 0; s < 6; ++s)
            if (n > 1 && r.base.*kBaseWords[s] != static_cast<unsigned>(derived[s]))
                bof3::Fatal("solo level self-test: record %d's base %s after the reset", record, kStatNames[s]);
        if (r.hp != r.stats.max_hp || r.ap != r.stats.max_ap)
            bof3::Fatal("solo level self-test: record %d's HP / AP are not at their maxima after the reset", record);
        if (r.base.byte_0e != d.base.byte_0e || r.survive != d.survive || r.stats.byte_0e != d.stats.byte_0e)
            bof3::Fatal("solo level self-test: record %d's willpower %u/%u/%u after the reset, %u/%u/%u before", record,
                        r.base.byte_0e, r.survive, r.stats.byte_0e, d.base.byte_0e, d.survive, d.stats.byte_0e);
        checks += 4;

        // Back up with the game's own level-up, the default's EXP at once.
        r.exp = exp;
        ::Char_LevelUp(static_cast<unsigned>(record));
        // Game_Step 5 (TitleFlow_EnterGame 0x588800) recomputes every record
        // after New Game, so both sides are compared through that recompute:
        // a default-level-1 record, which the reset leaves alone and the
        // level-up does not reach, still holds Capcom's prebuilt effective
        // block here.
        ::Char_RecalcStats(Bytes(r));
        unsigned char back[kStride];
        std::memcpy(back, Bytes(r), kStride);
        const CharRecord& back_rec = *reinterpret_cast<const CharRecord*>(back);
        // The default put through the same recompute, in the same slot (the
        // roster bonus applies by slot).
        std::memcpy(Bytes(r), Bytes(d), kStride);
        ::Char_RecalcStats(Bytes(r));
        unsigned char want[kStride];
        std::memcpy(want, Bytes(r), kStride);
        for (unsigned o = 0; o < kStride; ++o) {
            if (o >= kHpOffset && o < kHpOffset + 4) continue;  // current HP and AP: a level-up does not refill them
            if (back[o] != want[o])
                bof3::Fatal("solo level self-test: record %d, level %u -> 1 -> %u, differs from its default at +0x%02X (%u, "
                            "want %u)",
                            record, n, back_rec.level, o, back[o], want[o]);
        }
        ++checks;
        // What the log line adds: current HP / AP after the climb (still the
        // level-1 maxima the reset set), and how far Capcom's prebuilt
        // effective block +0x20..+0x3F is from the recompute that
        // TitleFlow_EnterGame 0x588800 runs on every record at Game_Step 5.
        unsigned prebuilt = 0;
        for (unsigned o = kStatsFrom; o < kStatsTo; ++o)
            if (Bytes(d)[o] != want[o]) ++prebuilt;
        bof3::Log("solo level self-test: record %d level %u -> 1 -> %u equal to its default (HP %u/%u AP %u/%u after the "
                  "climb); its prebuilt effective block and Char_RecalcStats' differ in %u bytes",
                  record, n, back_rec.level, back_rec.hp, back_rec.stats.max_hp, back_rec.ap, back_rec.stats.max_ap,
                  prebuilt);
    }
    std::memcpy(Bytes(Slot(0)), saved, sizeof saved);
    return checks;
}

}  // namespace solo

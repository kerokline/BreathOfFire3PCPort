#include "game/solo/solo_level.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
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
//   record +0x0A level, +0x0C EXP (s32), +0x18 HP, +0x1A AP, +0x1C / +0x2E /
//   +0x4E willpower (NewGame_InitCharacters' one derived byte, three places),
//   +0x1F master (0xFF none), +0x20 / +0x22 effective max HP / AP, +0x40..
//   +0x4A the six base words, +0x60..+0x87 the four ability lists, +0x88 the
//   master's level, +0x89..+0x8E the six growth-bias bytes (char.cpp; range-b
//   1 of docs/save-block/range-b-characters.md; M10 section 1).

namespace solo {
namespace {

using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

constexpr unsigned kStride = 0xA4;
constexpr int kDefaultCount = 7;   // NewGame_InitCharacters copies records 0..6
constexpr int kSlotCount = 8;      // CharacterRecords' records
constexpr unsigned kRowsPerRecord = 99;
constexpr unsigned kRowSize = 8;
constexpr unsigned kMaxLevel = 99;
constexpr unsigned kStatCap = 999;  // Char_LevelUp's Grow caps a word at 999

constexpr unsigned kLevel = 0x0A;
constexpr unsigned kExp = 0x0C;
constexpr unsigned kHp = 0x18;
constexpr unsigned kAp = 0x1A;
constexpr unsigned kWillCurrent = 0x1C;
constexpr unsigned kMaster = 0x1F;
constexpr unsigned kMaxHp = 0x20;
constexpr unsigned kMaxAp = 0x22;
constexpr unsigned kWillEffective = 0x2E;
constexpr unsigned kBase = 0x40;  // six words: HP, AP, power, defence, agility, intelligence
constexpr unsigned kWillBase = 0x4E;
constexpr unsigned kListSize = 10;
constexpr unsigned kMasterLevel = 0x88;
constexpr unsigned kBias = 0x89;  // six signed bytes, in the base words' order

constexpr const char* kStatNames[6] = {"HP", "AP", "power", "defence", "agility", "intelligence"};

unsigned char* Slot(int record) { return At(::bof3::Img(CharacterRecords_addr) + static_cast<unsigned>(record) * kStride); }
const unsigned char* Default(int record) {
    return At(::bof3::Img(Char_DefaultRecords_addr) + static_cast<unsigned>(record) * kStride);
}
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
unsigned Derive(int record, int derived[6]) {
    const unsigned char* const d = Default(record);
    const unsigned n = d[kLevel];
    if (n < 1 || n > kMaxLevel)
        bof3::Fatal("solo level-1 start: default record %d's level is %u, not 1..99 (Char_DefaultRecords 0x64B390 as laid)", record, n);
    if (d[kMaster] != 0xFF || d[kMasterLevel] != 0)
        bof3::Fatal("solo level-1 start: default record %d has a master (+0x1F %u, +0x88 %u): its growth is not the bare "
                    "table's, so the levels cannot be subtracted",
                    record, d[kMaster], d[kMasterLevel]);
    for (unsigned s = 0; s < 6; ++s)
        if (d[kBias + s] != 0)
            bof3::Fatal("solo level-1 start: default record %d has a growth bias %d on %s (+0x%X): the levels cannot be "
                        "subtracted",
                        record, static_cast<signed char>(d[kBias + s]), kStatNames[s], kBias + s);
    for (unsigned s = 0; s < 6; ++s) {
        const unsigned v = Word(d + kBase + 2 * s);
        if (v >= kStatCap)
            bof3::Fatal("solo level-1 start: default record %d's base %s is %u: Char_LevelUp caps a word at 999, so what "
                        "it added cannot be told",
                        record, kStatNames[s], v);
        derived[s] = static_cast<int>(v);
    }
    for (unsigned level = 1; level < n; ++level) {
        int g[6];
        Gains(Row(record, level), g);
        for (unsigned s = 0; s < 6; ++s) derived[s] -= g[s];
    }
    for (unsigned s = 0; s < 6; ++s)
        if (derived[s] < 0)
            bof3::Fatal("solo level-1 start: record %d's level-1 base %s derives to %d (the default's %u less rows 1..%u of "
                        "Char_ExpTable 0x658F48 as laid): the table does not fit the default record",
                        record, kStatNames[s], derived[s], static_cast<unsigned>(Word(d + kBase + 2 * s)), n - 1);
    return n;
}

// The live record against the default, all 0xA4 bytes but the three willpower
// bytes NewGame_InitCharacters derives.
void CheckIsDefault(int record) {
    const unsigned char* const live = Slot(record);
    const unsigned char* const d = Default(record);
    for (unsigned o = 0; o < kStride; ++o) {
        if (IsWillpowerByte(o)) continue;
        if (live[o] != d[o])
            bof3::Fatal("solo level-1 start: record %d is not its default record at +0x%02X (%u, the default %u). It runs "
                        "only at New Game, right after NewGame_InitCharacters 0x437820, where the record is the default "
                        "by construction",
                        record, o, live[o], d[o]);
    }
}

// One copy of `id`, the last, out of its list; the entries after it move up
// one and the last slot becomes 0. A Fatal when the list does not hold it.
void RemoveAbility(int record, unsigned id, unsigned level) {
    unsigned char* const list = ::AbilityList_ForType(static_cast<unsigned>(record), id, 0);
    for (int i = static_cast<int>(kListSize) - 1; i >= 0; --i) {
        if (list[i] != id) continue;
        for (unsigned j = static_cast<unsigned>(i); j + 1 < kListSize; ++j) list[j] = list[j + 1];
        list[kListSize - 1] = 0;
        return;
    }
    bof3::Fatal("solo level-1 start: record %d's row %u files ability %u, which its list (record +0x%X) does not hold: "
                "the default record was not built by this table",
                record, level, id, static_cast<unsigned>(list - Slot(record)));
}

void LogRecord(const char* when, int record) {
    const unsigned char* const r = Slot(record);
    bof3::Log("DIV-0228 SoloCharacter level-1 start, record %d %s: level %u, EXP %d, base HP %u AP %u power %u defence %u "
              "agility %u intelligence %u, HP %u/%u, AP %u/%u, willpower +0x4E %u +0x1C %u +0x2E %u",
              record, when, r[kLevel], static_cast<int>(Long(r + kExp)), Word(r + kBase), Word(r + kBase + 2),
              Word(r + kBase + 4), Word(r + kBase + 6), Word(r + kBase + 8), Word(r + kBase + 10), Word(r + kHp),
              Word(r + kMaxHp), Word(r + kAp), Word(r + kMaxAp), r[kWillBase], r[kWillCurrent], r[kWillEffective]);
}

}  // namespace

void Solo_LevelReset(int record, bool log) {
    if (record < 0 || record >= kDefaultCount)
        bof3::Fatal("solo level-1 start: record %d has no default record (0..6)", record);
    CheckIsDefault(record);
    int derived[6];
    const unsigned n = Derive(record, derived);
    if (log) LogRecord("before", record);
    if (n == 1) {
        if (log) bof3::Log("DIV-0228 SoloCharacter level-1 start, record %d: its default is level 1, left as it is", record);
        return;
    }
    // The abilities of rows N-1 down to 1, each row's +7 before its +6: the
    // reverse of the order Char_LevelUp files them in, so with each removal
    // taking the last copy, a list built by that filing is unbuilt exactly.
    for (unsigned level = n - 1; level >= 1; --level) {
        const unsigned char* const row = Row(record, level);
        if (row[7] != 0) RemoveAbility(record, row[7], level);
        if (row[6] != 0) RemoveAbility(record, row[6], level);
    }
    unsigned char* const r = Slot(record);
    r[kLevel] = 1;
    SetLong(r + kExp, 0);
    for (unsigned s = 0; s < 6; ++s) SetWord(r + kBase + 2 * s, static_cast<unsigned>(derived[s]));
    ::Char_RecalcStats(r);
    SetWord(r + kHp, Word(r + kMaxHp));
    SetWord(r + kAp, Word(r + kMaxAp));
    if (log) LogRecord("after", record);
}

int Solo_LevelSelfTest() {
    int checks = 0;
    unsigned char saved[kSlotCount * kStride];
    std::memcpy(saved, Slot(0), sizeof saved);
    // The New Game state itself: the game's own copy of the default records
    // and their willpower.
    ::NewGame_InitCharacters();
    unsigned char state[kSlotCount * kStride];
    std::memcpy(state, Slot(0), sizeof state);
    for (int record = 0; record < kDefaultCount; ++record) {
        unsigned char* const r = Slot(record);
        const unsigned char* const d = state + static_cast<unsigned>(record) * kStride;
        std::memcpy(Slot(0), state, sizeof state);
        int derived[6];
        const unsigned n = Derive(record, derived);
        for (unsigned s = 0; s < 6; ++s) {
            if (derived[s] < 0) bof3::Fatal("solo level self-test: record %d's %s derives negative", record, kStatNames[s]);
            ++checks;
        }
        // The EXP lies in the default level's window (Char_LevelUp's own sum).
        std::int32_t below = 0;
        for (unsigned level = 1; level < n; ++level) below += Word(Row(record, level));
        const std::int32_t exp = Long(d + kExp);
        if (exp < below || (n < kMaxLevel && exp >= below + Word(Row(record, n))))
            bof3::Fatal("solo level self-test: record %d's default EXP %d is not in level %u's window", record, exp, n);
        ++checks;

        Solo_LevelReset(record, false);
        if (r[kLevel] != 1 || Long(r + kExp) != 0)
            bof3::Fatal("solo level self-test: record %d after the reset is level %u EXP %d", record, r[kLevel],
                        static_cast<int>(Long(r + kExp)));
        for (unsigned s = 0; s < 6; ++s)
            if (n > 1 && Word(r + kBase + 2 * s) != static_cast<unsigned>(derived[s]))
                bof3::Fatal("solo level self-test: record %d's base %s after the reset", record, kStatNames[s]);
        if (Word(r + kHp) != Word(r + kMaxHp) || Word(r + kAp) != Word(r + kMaxAp))
            bof3::Fatal("solo level self-test: record %d's HP / AP are not at their maxima after the reset", record);
        if (r[kWillBase] != d[kWillBase] || r[kWillCurrent] != d[kWillCurrent] || r[kWillEffective] != d[kWillEffective])
            bof3::Fatal("solo level self-test: record %d's willpower %u/%u/%u after the reset, %u/%u/%u before", record,
                        r[kWillBase], r[kWillCurrent], r[kWillEffective], d[kWillBase], d[kWillCurrent], d[kWillEffective]);
        checks += 4;

        // Back up with the game's own level-up, the default's EXP at once.
        SetLong(r + kExp, exp);
        ::Char_LevelUp(static_cast<unsigned>(record));
        // Game_Step 5 (TitleFlow_EnterGame 0x588800) recomputes every record
        // after New Game, so both sides are compared through that recompute:
        // a default-level-1 record, which the reset leaves alone and the
        // level-up does not reach, still holds Capcom's prebuilt effective
        // block here.
        ::Char_RecalcStats(r);
        unsigned char back[kStride];
        std::memcpy(back, r, kStride);
        // The default put through the same recompute, in the same slot (the
        // roster bonus applies by slot).
        std::memcpy(r, d, kStride);
        ::Char_RecalcStats(r);
        unsigned char want[kStride];
        std::memcpy(want, r, kStride);
        for (unsigned o = 0; o < kStride; ++o) {
            if (o >= kHp && o < kHp + 4) continue;  // current HP and AP: a level-up does not refill them
            if (back[o] != want[o])
                bof3::Fatal("solo level self-test: record %d, level %u -> 1 -> %u, differs from its default at +0x%02X (%u, "
                            "want %u)",
                            record, n, back[kLevel], o, back[o], want[o]);
        }
        ++checks;
        // What the log line adds: current HP / AP after the climb (still the
        // level-1 maxima the reset set), and how far Capcom's prebuilt
        // effective block +0x20..+0x3F is from the recompute that
        // TitleFlow_EnterGame 0x588800 runs on every record at Game_Step 5.
        unsigned prebuilt = 0;
        for (unsigned o = kMaxHp; o < kBase; ++o)
            if (d[o] != want[o]) ++prebuilt;
        bof3::Log("solo level self-test: record %d level %u -> 1 -> %u equal to its default (HP %u/%u AP %u/%u after the "
                  "climb); its prebuilt effective block and Char_RecalcStats' differ in %u bytes",
                  record, n, back[kLevel], Word(back + kHp), Word(back + kMaxHp), Word(back + kAp), Word(back + kMaxAp),
                  prebuilt);
    }
    std::memcpy(Slot(0), saved, sizeof saved);
    return checks;
}

}  // namespace solo

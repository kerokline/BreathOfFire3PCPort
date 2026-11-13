#include "game/solo/solo.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/records.h"
#include "game/solo/solo_level.h"
#include "game/solo/solo_party_set.h"
#include "game/solo/solo_rows.h"
#include "hook/behaviour.h"
#include "hook/detour.h"
#include "hook/host.h"
#include "hook/log.h"
#include "os/os.h"

// The solo-character mode's core (docs/solo-character-round.md, group SC;
// DIVERGENCE DIV-0228 SoloCharacter, DIV-0229 SoloChampionIfPresent). What
// each address is:
//
//   Cond_ByteFA       0x8034E0  the live scenario chapter, s8 (symbols.toml;
//                               docs/save-block/range-a-head.md +0x00: the
//                               block's +0x000 is its copy at save and load)
//   CharacterRecords  0x903A70  the eight character records, 0xA4 each
//                               (records.h CharRecord): +9 the id, +0xB bit 0
//                               joined, bit 1 must stay (solo-character-
//                               reads.md M6, M7)
//   Solo_ChampionMark 0x904A21  ours: save block +0x1041 (symbols.toml has
//                               the evidence that Capcom's code never
//                               addresses it)
//
// The module holds no game state of its own: the champion of a run is the
// mark, which is inside the image span the quicksave copies (snapshot.h), so
// no snapshot participant is needed. What it keeps - BOF3X_SOLO's record and
// the last mark logged - is fixed for the start or only governs a log line.

namespace solo {
namespace {

// The champions: the launcher's names (the contract with group LA) and the
// records they mean. Records 3 and 7 are never champions (round doc 6a item 7).
struct Champion {
    const char* name;
    int record;
};
constexpr Champion kChampions[] = {
    {"ryu", 0}, {"nina", 1}, {"garr", 2}, {"rei", 4}, {"momo", 5}, {"peco", 6},
};
constexpr int kRecordCount = 8;  // CharacterRecords' record_count

// The chapters the mode stands down in. 0 is the intro (the owner, 6a item 3:
// "Chapter 0 I mean for the intro"). 16 is the attract demo's own chapter:
// Title_StateStartDemo 0x462300 starts it with Scenario_Start(0x10), and
// Area_Enter's chapter step skips 16 in play (to 17; item_trade.cpp,
// docs/save-block/range-a-head.md "not 16 (the demo)"). The demo returns to
// the title without Boot_Task, so a mark stamped in it would reach the next
// New Game; and the demo is a recorded scene the mode must not change.
constexpr int kIntroChapter = 0;
constexpr int kAttractChapter = 16;

// The owner's open call (round doc 6a, "What this opens"): with "champion
// always" (SoloChampionIfPresent off), does the champion fight before the
// story has ever introduced them? Every record exists from New Game, so the
// answer this builds is yes: from chapter 1 on, joined bit or not.
//
// "No" needs a mark of "has joined once", which the game does not keep (bit 0
// is cleared when the story takes a character away, M6) and this module does
// not build. Set this to false and the build stops at the static_assert below
// until that mark exists - the one place the answer lives.
constexpr bool kChampionFightsBeforeFirstJoin = true;
static_assert(kChampionFightsBeforeFirstJoin,
              "the champion's \"not yet introduced\" answer needs a has-joined-once mark that is not built "
              "(solo.cpp, kChampionFightsBeforeFirstJoin)");

// What BOF3X_SOLO gave: -1 none, else the record. Fixed by Solo_Read.
int g_env_record = -1;
bool g_read = false;
// The last mark the "BOF3X_SOLO names another champion" line was logged for
// (0: none yet). Only governs the log.
unsigned g_logged_mark = 0;

bool IsChampionRecord(int record) {
    for (const Champion& c : kChampions)
        if (c.record == record) return true;
    return false;
}

const char* ChampionName(int record) {
    for (const Champion& c : kChampions)
        if (c.record == record) return c.name;
    return "?";
}

enum class Parse { kNone, kChampion, kBad };

// BOF3X_SOLO's value: empty is none; otherwise exactly one of the names.
Parse ParseChampion(const char* s, int* record) {
    if (!s || !*s) return Parse::kNone;
    for (const Champion& c : kChampions)
        if (std::strcmp(s, c.name) == 0) {
            *record = c.record;
            return Parse::kChampion;
        }
    return Parse::kBad;
}

// The decision table: everything the questions answer, from inputs alone.
// The live questions and the self-test both go through it.
struct Inputs {
    bool on;               // SoloCharacter
    bool if_present;       // SoloChampionIfPresent
    int chapter;           // Cond_ByteFA, signed
    unsigned mark;         // Solo_ChampionMark (not read, and 0 here, when !on)
    int env_record;        // BOF3X_SOLO's record, -1 none
    bool joined[kRecordCount];  // CharacterRecords[r] +0xB bit 0
};
struct Answer {
    bool bad_mark;   // the mark is neither 0 nor a champion's record + 1
    bool active;     // Solo_Active()
    int record;      // the champion's record when active, else -1
    bool joined;     // its joined bit when active
    bool fights;     // Solo_ChampionFights()
    unsigned stamp;  // the value Solo_Stamp writes, 0 none
    bool differs;    // a mark holds and BOF3X_SOLO names another champion
};

bool ChapterAllows(int chapter) { return chapter != kIntroChapter && chapter != kAttractChapter; }

Answer Decide(const Inputs& in) {
    Answer a{false, false, -1, false, false, 0, false};
    if (!in.on) return a;
    if (in.mark != 0 && (in.mark > static_cast<unsigned>(kRecordCount) || !IsChampionRecord(static_cast<int>(in.mark) - 1))) {
        a.bad_mark = true;
        return a;
    }
    if (!ChapterAllows(in.chapter)) return a;
    if (in.mark == 0) {
        if (in.env_record >= 0) a.stamp = static_cast<unsigned>(in.env_record) + 1;
        return a;
    }
    a.active = true;
    a.record = static_cast<int>(in.mark) - 1;
    a.joined = in.joined[a.record];
    a.differs = in.env_record >= 0 && in.env_record != a.record;
    // kChampionFightsBeforeFirstJoin is true (static_assert above): with
    // SoloChampionIfPresent off the joined bit is not asked.
    a.fights = !in.if_present || a.joined;
    return a;
}

// The live inputs. The mark and the records are read only with the behaviour
// on: off, this reads nothing of the save block.
Inputs LiveInputs() {
    Inputs in{};
    in.on = bof3::BehaviourOn(bof3::behaviours::Id::SoloCharacter);
    if (!in.on) return in;
    in.if_present = bof3::BehaviourOn(bof3::behaviours::Id::SoloChampionIfPresent);
    in.chapter = Cond_ByteFA;
    in.mark = Solo_ChampionMark;
    in.env_record = g_env_record;
    for (int r = 0; r < kRecordCount; ++r) in.joined[r] = (CharacterRecords[r].flags & 1) != 0;
    return in;
}

Answer LiveAnswer() {
    if (!g_read) bof3::Fatal("solo: a question was asked before Solo_Read (InjectAll, after Behaviour_Read)");
    const Inputs in = LiveInputs();
    const Answer a = Decide(in);
    if (a.bad_mark)
        bof3::Fatal("DIV-0228 SoloCharacter: the save's champion mark (block +0x1041, 0x904A21) is %u, which names no "
                    "champion: 0 is no solo run, 1, 2, 3, 5, 6 or 7 a champion's record + 1. The save was not made by "
                    "this mode, or was edited",
                    in.mark);
    return a;
}

Answer ActiveAnswer(const char* question) {
    const Answer a = LiveAnswer();
    if (!a.active)
        bof3::Fatal("solo: %s was asked while the mode is not active (SoloCharacter off, chapter 0 or 16, or no "
                    "champion in the save): ask Solo_Active() first",
                    question);
    return a;
}

}  // namespace

void Solo_Read() {
    if (g_read) bof3::Fatal("solo: Solo_Read called twice: BOF3X_SOLO is read once, in InjectAll");
    char value[64];
    const std::uint32_t n = bof3::os::GetEnv("BOF3X_SOLO", value, sizeof value);
    if (n >= sizeof value)
        bof3::Fatal("BOF3X_SOLO is longer than %u bytes: it names one champion, ryu, nina, garr, rei, momo or peco",
                    static_cast<unsigned>(sizeof value) - 1);
    if (n == 0) value[0] = '\0';
    int record = -1;
    const Parse p = ParseChampion(value, &record);
    if (p == Parse::kBad)
        bof3::Fatal("BOF3X_SOLO is \"%s\", which names no champion. It is one of ryu, nina, garr, rei, momo or peco, "
                    "in lower case (the launcher's solo champion, DIV-0228)",
                    value);
    g_env_record = p == Parse::kChampion ? record : -1;
    g_read = true;
    const bool on = bof3::BehaviourOn(bof3::behaviours::Id::SoloCharacter);
    const bool if_present = bof3::BehaviourOn(bof3::behaviours::Id::SoloChampionIfPresent);
    if (on && g_env_record >= 0)
        bof3::Log("DIV-0228 SoloCharacter on, BOF3X_SOLO=%s (record %d): a save with no champion takes it from chapter 1 on",
                  value, g_env_record);
    else if (on)
        bof3::Log("DIV-0228 SoloCharacter on, BOF3X_SOLO unset: no champion from the launcher; a save that holds one keeps it");
    else if (g_env_record >= 0)
        bof3::Log("solo: BOF3X_SOLO=%s is set and SoloCharacter is off: nothing reads it", value);
    if (if_present && !on) bof3::Log("DIV-0229 SoloChampionIfPresent is on and SoloCharacter off: it does nothing");
    else if (if_present) bof3::Log("DIV-0229 SoloChampionIfPresent on: the champion fights only while joined");
    // The level-1 start's preconditions on the tables as they are now, before
    // any New Game can need them (solo_level.h, Solo_LevelCheckTables): a mod
    // that changes the default records or the level table so that the start
    // cannot be made stops the game here, with the record and the cause.
    if (on && g_env_record >= 0) Solo_LevelCheckTables(g_env_record);
    if (SoloSelfTestWanted()) Solo_SelfTest();
}

bool Solo_Active() { return LiveAnswer().active; }

int Solo_ChampionRecord() { return ActiveAnswer("Solo_ChampionRecord").record; }

unsigned Solo_ChampionId() { return CharacterRecords[ActiveAnswer("Solo_ChampionId").record].char_id; }

bool Solo_ChampionJoined() { return ActiveAnswer("Solo_ChampionJoined").joined; }

bool Solo_ChampionFights() { return LiveAnswer().fights; }

bool Solo_MustStay(int record) {
    if (record < 0 || record >= kRecordCount) bof3::Fatal("solo: Solo_MustStay(%d): a record is 0..7", record);
    return (CharacterRecords[record].flags & 2) != 0;
}

void Solo_Stamp() {
    if (!bof3::BehaviourOn(bof3::behaviours::Id::SoloCharacter)) return;
    const Answer a = LiveAnswer();
    if (a.stamp != 0) {
        Solo_ChampionMark = static_cast<unsigned char>(a.stamp);
        bof3::Log("DIV-0228 SoloCharacter: the champion %s (record %u) is stamped into this game's save mark, chapter %d",
                  ChampionName(static_cast<int>(a.stamp) - 1), a.stamp - 1, static_cast<int>(Cond_ByteFA));
        return;
    }
    const unsigned mark = a.active ? static_cast<unsigned>(a.record) + 1 : 0;
    if (a.differs && mark != g_logged_mark) {
        g_logged_mark = mark;
        bof3::Log("DIV-0228 SoloCharacter: this save's champion is %s (record %d); BOF3X_SOLO names %s, which does not "
                  "change it",
                  ChampionName(a.record), a.record, ChampionName(g_env_record));
    }
}

void Solo_StampNewGame() {
    if (!bof3::BehaviourOn(bof3::behaviours::Id::SoloCharacter)) return;
    if (!g_read) bof3::Fatal("solo: Solo_StampNewGame before Solo_Read (InjectAll, after Behaviour_Read)");
    const unsigned old = Solo_ChampionMark;
    if (g_env_record < 0) {
        Solo_ChampionMark = 0;
        bof3::Log("DIV-0228 SoloCharacter: New Game with no launcher champion: the save mark (was %u) is 0, no solo run",
                  old);
        return;
    }
    Solo_ChampionMark = static_cast<unsigned char>(g_env_record + 1);
    bof3::Log("DIV-0228 SoloCharacter: New Game stamps the champion %s (record %d) into the save mark: %u (was %u)",
              ChampionName(g_env_record), g_env_record, static_cast<unsigned>(Solo_ChampionMark), old);
    Solo_LevelReset(g_env_record, true);
}

void Solo_SelfTest() {
    int checks = 0;
    // BOF3X_SOLO's names.
    struct NameCase {
        const char* value;
        Parse parse;
        int record;
    };
    static const NameCase kNames[] = {
        {"", Parse::kNone, -1},       {"ryu", Parse::kChampion, 0},  {"nina", Parse::kChampion, 1},
        {"garr", Parse::kChampion, 2}, {"rei", Parse::kChampion, 4},  {"momo", Parse::kChampion, 5},
        {"peco", Parse::kChampion, 6}, {"Ryu", Parse::kBad, -1},      {"teepo", Parse::kBad, -1},
        {"whelp", Parse::kBad, -1},    {"ryu ", Parse::kBad, -1},     {"3", Parse::kBad, -1},
        {"garrr", Parse::kBad, -1},
    };
    for (const NameCase& c : kNames) {
        int record = -1;
        const Parse p = ParseChampion(c.value, &record);
        if (p != c.parse || (p == Parse::kChampion && record != c.record))
            bof3::Fatal("solo self-test: BOF3X_SOLO=\"%s\" parsed wrong", c.value);
        ++checks;
    }
    // The champions are records 0, 1, 2, 4, 5, 6 and nothing else.
    for (int r = -1; r <= kRecordCount; ++r) {
        const bool want = r == 0 || r == 1 || r == 2 || r == 4 || r == 5 || r == 6;
        if (IsChampionRecord(r) != want) bof3::Fatal("solo self-test: record %d is %s a champion", r, want ? "not" : "");
        ++checks;
    }
    // The decision table. joined: records 0 and 2 joined, the rest not.
    struct Case {
        bool on, if_present;
        int chapter;
        unsigned mark;
        int env;
        // expected
        bool bad, active;
        int record;
        bool fights;
        unsigned stamp;
        bool differs;
    };
    static const Case kCases[] = {
        // off: nothing, whatever the rest says (the mark is not even read live)
        {false, false, 5, 0, 2, false, false, -1, false, 0, false},
        {false, true, 5, 3, 2, false, false, -1, false, 0, false},
        {false, false, 5, 99, 2, false, false, -1, false, 0, false},
        // on, intro and demo: stand down, no stamp
        {true, false, 0, 0, 2, false, false, -1, false, 0, false},
        {true, false, 0, 3, 2, false, false, -1, false, 0, false},
        {true, false, 16, 0, 2, false, false, -1, false, 0, false},
        {true, false, 16, 3, 2, false, false, -1, false, 0, false},
        // on, no mark: a stamp when BOF3X_SOLO names one, the mode not yet active
        {true, false, 1, 0, 2, false, false, -1, false, 3, false},
        {true, false, 17, 0, 6, false, false, -1, false, 7, false},
        {true, false, 1, 0, 0, false, false, -1, false, 1, false},
        {true, false, 1, 0, -1, false, false, -1, false, 0, false},
        // on, a mark: active; "champion always" fights joined or not
        {true, false, 1, 3, 2, false, true, 2, true, 0, false},
        {true, false, 1, 2, 2, false, true, 1, true, 0, true},
        {true, false, 19, 7, -1, false, true, 6, true, 0, false},
        {true, false, 8, 1, 4, false, true, 0, true, 0, true},
        // SoloChampionIfPresent: fights only while joined
        {true, true, 1, 3, 2, false, true, 2, true, 0, false},
        {true, true, 1, 2, 1, false, true, 1, false, 0, false},
        {true, true, 1, 1, -1, false, true, 0, true, 0, false},
        {true, true, 9, 6, 5, false, true, 5, false, 0, false},
        // a mark that names no champion: bad, wherever the chapter stands
        {true, false, 1, 4, 2, true, false, -1, false, 0, false},
        {true, false, 1, 8, 2, true, false, -1, false, 0, false},
        {true, false, 1, 9, 2, true, false, -1, false, 0, false},
        {true, false, 0, 0xFF, 2, true, false, -1, false, 0, false},
        {true, true, 5, 4, -1, true, false, -1, false, 0, false},
    };
    for (const Case& c : kCases) {
        Inputs in{};
        in.on = c.on;
        in.if_present = c.if_present;
        in.chapter = c.chapter;
        in.mark = c.mark;
        in.env_record = c.env;
        in.joined[0] = true;
        in.joined[2] = true;
        const Answer a = Decide(in);
        if (a.bad_mark != c.bad || a.active != c.active || a.record != c.record || a.fights != c.fights ||
            a.stamp != c.stamp || a.differs != c.differs)
            bof3::Fatal("solo self-test: on %d, if-present %d, chapter %d, mark %u, BOF3X_SOLO record %d gives bad %d "
                        "active %d record %d fights %d stamp %u differs %d",
                        c.on, c.if_present, c.chapter, c.mark, c.env, a.bad_mark, a.active, a.record, a.fights, a.stamp,
                        a.differs);
        if (a.active && a.joined != in.joined[a.record]) bof3::Fatal("solo self-test: the joined bit of record %d", a.record);
        ++checks;
    }
    // Every chapter but 0 and 16 lets the mode act.
    for (int ch = -128; ch < 128; ++ch) {
        if (ChapterAllows(ch) != (ch != 0 && ch != 16)) bof3::Fatal("solo self-test: chapter %d", ch);
    }
    ++checks;
    // The level-1 start's round trip (solo_level.cpp).
    checks += Solo_LevelSelfTest();
    // The party-set supplement's pure logic (solo_party_set.cpp).
    checks += Solo_PartySetSelfTest();
    bof3::Log("solo self-test: %d checks passed", checks);
}

}  // namespace solo

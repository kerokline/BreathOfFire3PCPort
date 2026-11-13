#include "game/solo/solo_battle.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "bof3/symbols.gen.h"
#include "game/records.h"
#include "game/solo/solo.h"
#include "hook/behaviour.h"
#include "hook/detour.h"
#include "hook/host.h"
#include "hook/log.h"
#include "hook/snapshot.h"
#include "os/os.h"

// The solo-character mode's combat half (solo_battle.h; docs/solo-character-
// combat.md has the reads behind every rule here). What each address is:
//
//   Cond_Flags + 0xD0  0x904060  the formation byte (Party_Remove sets it 0
//                                when the party drops to one, field_6.cpp's
//                                Party_Remove tail; solo-character-reads.md M4)
//   Cond_Flags + 0xD2  0x904062  the first party list, 3 ids, 0xFF none
//   Cond_Flags + 0xD5  0x904065  the second party list: Field_PartyLoad(1)
//                                builds the battle's members from it in
//                                GameMode5_Load (step 3)
//   Field_MemberCount  0x929EC0  the members count both Field_PartyLoad
//                                slots read
//   AreaW0b_PartySet   0x90412C  the party set: its low seven bits a row of
//                                kPartySetRows, bit 7 set by a mode-0 select
//   kPartySetRows      0x669750  19 rows of 3 ids (PartySet_Find's table;
//                                symbols.toml 0x5368F0's evidence; read
//                                2026-11-12, round doc section 2 M1)
//   Game_BattleCount   0x904134  +1 per battle (GameMode_Field, request 3)
//
// The module's state - the save - is outside the image span, so it is the
// quicksave participant "SOLOCB" below.

namespace solo {
namespace {

constexpr std::uint32_t kPartySetRows = 0x669750;
constexpr unsigned kPartySetRowCount = 0x13;
constexpr unsigned kFormation = 0xD0;   // Cond_Flags offsets
constexpr unsigned kFirstList = 0xD2;
constexpr unsigned kSecondList = 0xD5;
constexpr unsigned char kNone = 0xFF;

// The bytes a battle of one changes.
struct Bytes {
    std::uint8_t list2[3];   // 0x904065..67
    std::uint8_t count;      // Field_MemberCount
    std::uint8_t formation;  // 0x904060
    std::uint8_t set;        // 0x90412C
};

// What Arm saved and wrote. `set_written`: Arm changed the party set's row;
// otherwise `written.set` is unused (the battle's own selects change only its
// bit 7, and the row stays the field's). `party_written`: the list, count and
// formation are written (SoloBattle_WriteParty, step 3), not yet at step 1.
struct Save {
    std::uint8_t armed;
    std::uint8_t set_written;
    std::uint8_t party_written;
    std::uint32_t battle;   // Game_BattleCount when armed
    Bytes saved;
    Bytes written;
};

Save g_save{};
unsigned char g_place_leader = kNone;   // SoloBattle_Restore's answer's id

// What goes back at step 7 (SoloBattle_RestoreAtStep7), held from step 6.
// Until step 7's Field_PartyLoad(0) rebuilds the walkers, ObjTrio's second
// and third records are the walkers' records from before the battle -
// Field_PartyLoad(1) only cleared their flags - whose sprite pointers hold
// offsets into the set file loaded before the battle. With the count back at
// 3, Party_ScriptTicks and the members' draw tick and draw them (the draw
// tests bit 0x40, not bit 0), and when the battle's end has loaded another
// row's file there, they read garbage frames and upload records
// (docs/solo-character-combat.md 9). So after Party_PlaceAtSlots has placed
// them, the count is held at 1 - the champion, the one record the battle
// built - and the count and the set go back together right before
// Field_PartyLoad(0).
struct StepSeven {
    std::uint8_t pending;
    std::uint8_t count_back;  // the count is held at 1 until step 7
    std::uint8_t count;       // the count to put back
    std::uint8_t set_back;    // the set goes back at step 7
    std::uint8_t reload;      // its row differs: the row's field file is loaded again
    std::uint8_t set;         // the byte to write
};
StepSeven g_set_back{};

// The fights with a rule of their own (docs/solo-character-combat.md 10, 11).
// The battle the game is starting is EventBattle_Id 0x904AAA
// (Field_StartEventBattle's byte, set before mode 5; 0 for a random
// encounter), which is also the index of the fight's set-up in
// Boss_SetupTable 0x656954 (Battle_InitBossEncounter). One line a fight.
enum class Rule : unsigned char {
    kStandDown,      // the game's own party fights it, whoever the champion is
    kNeedsIdInRow,   // the fight's code draws character id `arg` from the loaded party set: the
                     // battle's row must hold it beside the champion, else the game's own party fights
    kNeedsRecord,    // the fight's code wants record `arg` among the members: the game's own party
                     // fights it unless the champion is that record
};
struct FightRule {
    unsigned char fight;
    Rule rule;
    unsigned char arg;
    const char* why;
};
constexpr FightRule kFightRules[] = {
    // The owner (round doc 6g): "the beyd fight is a minigame, not a true
    // boss - I would turn off the logic for those 2 fights (the beyd fight,
    // and the beyd vs zigg fight)". Read in the code (combat.md 10): three
    // Beyd fights, chapter 6, area 92.
    {0x19, Rule::kStandDown, 0, "chapter 6's Beyd bout, part one (fight 25: run 8 step 2; SortParty fields record 0 alone, Boss25's hooks script it)"},
    {0x1A, Rule::kStandDown, 0, "chapter 6's Beyd bout, part two (fight 26: run 8 step 0xC; the party of three for 21 turns, Boss26 tallies the blows)"},
    {0x1B, Rule::kStandDown, 0, "chapter 6's Beyd against Zig (fight 27: run 9 step 5; the party watches - a member's turn ends it, Boss27_Event)"},
    // combat.md 11: set-ups that read the party by id or by slot.
    {0x10, Rule::kNeedsIdInRow, 1, "fight 16 (chapter 3, area 41: Balio, Sunder, Nina): Boss16_Event's code 2 and BossNina_HookHit dress enemy 2 as id 1 by Field_MemberSprite(1, 1), from the loaded party set"},
    {0x22, Rule::kNeedsRecord, 4, "fight 34 (chapter 8, area 43: Mikba): Boss34_Setup picks the member whose record is 4 and Boss34_Event drives that member"},
};

const FightRule* RuleFor(unsigned char fight) {
    if (fight == 0) return nullptr;
    for (const FightRule& r : kFightRules)
        if (r.fight == fight) return &r;
    return nullptr;
}

// --- the logic, on bytes alone (the live hooks and the self-test) ----------

// Whether `row` (0..18) of `rows` holds `id`.
bool RowHolds(const unsigned char* rows, unsigned row, unsigned char id) {
    for (unsigned c = 0; c < 3; ++c)
        if (rows[row * 3 + c] == id) return true;
    return false;
}

// Whether the current set (`set`, 0xFF none) holds `id`, as PartySet_Find
// asks it first (mode_11.cpp: "the current set 0x90412C if it is not 0xFF and
// holds every one of them"). A row past the table is a Fatal: Capcom's Find
// would read past it.
bool SetHolds(const unsigned char* rows, unsigned char set, unsigned char id) {
    if (set == kNone) return false;
    const unsigned row = set & 0x7Fu;
    if (row >= kPartySetRowCount)
        bof3::Fatal("DIV-0228 SoloCharacter: the party set 0x90412C is 0x%02X, past the %u rows of 0x%X", set,
                    kPartySetRowCount, kPartySetRows);
    return RowHolds(rows, row, id);
}

// The battle's row when the current one will not do: among the rows holding
// the champion `id` (and `need`, unless 0xFF), the one that keeps the most of
// the current row's ids, the lowest row on a tie - so a set-up that wants a
// party id finds it wherever a row allows. kKeep: the current set holds them
// already; kNoRow: no row does (the caller stands down).
constexpr int kKeep = -1;
constexpr int kNoRow = -2;
int ChooseRow(const unsigned char* rows, unsigned char set, unsigned char id, unsigned char need) {
    if (SetHolds(rows, set, id) && (need == kNone || SetHolds(rows, set, need))) return kKeep;
    int best = kNoRow, best_score = -1;
    for (unsigned row = 0; row < kPartySetRowCount; ++row) {
        if (!RowHolds(rows, row, id) || (need != kNone && !RowHolds(rows, row, need))) continue;
        int score = 0;
        if (set != kNone)
            for (unsigned c = 0; c < 3; ++c) {
                const unsigned char other = rows[(set & 0x7Fu) * 3 + c];
                if (other != kNone && RowHolds(rows, row, other)) ++score;
            }
        if (score > best_score) {
            best_score = score;
            best = static_cast<int>(row);
        }
    }
    return best;
}

// The second list is already the champion alone (Capcom's own one-member
// fight, or a battle that never left one): nothing to arm.
bool Alone(const Bytes& b, unsigned char id) {
    return b.list2[0] == id && b.list2[1] == kNone && b.list2[2] == kNone && b.count == 1;
}

// The bytes Arm writes: the champion alone, count 1, formation 0; the set is
// `row` when Arm changes it (row >= 0), recorded as the step's own select
// records a mode 1 / 2 load (no bit 7), else the live set.
Bytes Written(const Bytes& live, unsigned char id, int row) {
    Bytes w = live;
    w.list2[0] = id;
    w.list2[1] = kNone;
    w.list2[2] = kNone;
    w.count = 1;
    w.formation = 0;
    if (row >= 0) w.set = static_cast<std::uint8_t>(row);
    return w;
}

// What Restore puts back: each byte that still holds what Arm wrote takes the
// saved value; any other byte is someone else's since and is left. The set
// is compared by its row (the battle's own selects change bit 7) and only
// when Arm changed it. `reload` = the set goes back, so its field file must be
// loaded again.
struct Restored {
    Bytes bytes;
    unsigned restored;  // a bit per field: list2[0..2] 1/2/4, count 8, formation 0x10, set 0x20
    bool reload;
};
Restored Restore(const Save& s, const Bytes& live) {
    Restored r{live, 0, false};
    for (unsigned i = 0; i < 3; ++i)
        if (live.list2[i] == s.written.list2[i]) {
            r.bytes.list2[i] = s.saved.list2[i];
            r.restored |= 1u << i;
        }
    if (live.count == s.written.count) {
        r.bytes.count = s.saved.count;
        r.restored |= 8;
    }
    if (live.formation == s.written.formation) {
        r.bytes.formation = s.saved.formation;
        r.restored |= 0x10;
    }
    if (s.set_written && live.set != kNone && (live.set & 0x7Fu) == (s.written.set & 0x7Fu)) {
        r.bytes.set = s.saved.set;
        r.restored |= 0x20;
        r.reload = (s.saved.set & 0x7Fu) != (s.written.set & 0x7Fu);
    }
    return r;
}

// Whether Party_PlaceAtSlots (field_6.cpp) would miss the leader: it looks
// for the first list's first id among the first `count` ObjTrio records and,
// with none, reads the record after them (past ObjTrio at a count of 3). In
// a battle of one, ObjTrio's first record is the champion and the others are
// the walkers' records Field_PartyLoad(1) turned off, so a walker leader is
// not found. Then the champion's record stands for the leader: it stood at
// the first battle spot, where a three-member battle's leader stands.
bool LeaderMissed(unsigned count, const unsigned char ids[3], unsigned char leader, unsigned char champion) {
    if (count == 0 || count > 3) return false;
    if (ids[0] != champion) return false;
    for (unsigned k = 0; k < count; ++k)
        if (ids[k] == leader) return false;
    return true;
}

// --- the live side ----------------------------------------------------------

const unsigned char* Rows() { return reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(::bof3::Img(kPartySetRows))); }

Bytes ReadLive() {
    Bytes b{};
    for (unsigned i = 0; i < 3; ++i) b.list2[i] = Cond_Flags[kSecondList + i];
    b.count = Field_MemberCount;
    b.formation = Cond_Flags[kFormation];
    b.set = AreaW0b_PartySet;
    return b;
}

// The party bytes but the set (the set is written beside its load).
void WriteParty(const Bytes& b) {
    for (unsigned i = 0; i < 3; ++i) Cond_Flags[kSecondList + i] = b.list2[i];
    Field_MemberCount = b.count;
    Cond_Flags[kFormation] = b.formation;
}

// PartySet_Load's wait (mode_11.cpp): File_LoadDone asked first, then a
// frame's Task_Sleep until it answers. Mode 5's steps run in task 0, which
// GameMode3_Leave's own wait shows may sleep.
void WaitLoad() {
    if (File_LoadDone() != 0) return;
    do Task_Sleep(1);
    while (File_LoadDone() == 0);
}

// The champion's record as the battle found and left it: what the game's own
// write-backs (Battle_WriteBackMember, BattleResult_AddExp, Char_LevelUp,
// BattleEnd_Finish) credited to it, read at the two ends of the battle.
void LogRecord(const char* when, int record) {
    const records::CharRecord& c = CharacterRecords[record];
    bof3::Log("DIV-0228 SoloCharacter: record %d %s the battle: id 0x%02X level %u EXP %d HP %u AP %u status 0x%04X joined %u",
              record, when, c.char_id, c.level, static_cast<int>(c.exp), c.hp, c.ap, c.status, c.flags & 1u);
}

// --- the quicksave participant ----------------------------------------------

constexpr std::uint32_t kSnapVersion = 2;   // 2: the count and the set put back at step 7 (g_set_back)

void PutBytes(bof3::SnapshotWriter& w, const Bytes& b) { w.Put(&b, sizeof b); }
Bytes GetBytes(bof3::SnapshotReader& r) {
    Bytes b{};
    r.Get(&b, sizeof b);
    return b;
}

void SnapSave(bof3::SnapshotWriter& w) {
    w.Put(kSnapVersion);
    w.Put(g_save.armed);
    w.Put(g_save.set_written);
    w.Put(g_save.party_written);
    w.Put(g_save.battle);
    PutBytes(w, g_save.saved);
    PutBytes(w, g_save.written);
    w.Put(g_set_back);
}

bool SnapCheck(bof3::SnapshotReader& r, std::string& why, std::string& detail) {
    if (r.Left() < sizeof(std::uint32_t) || r.Get<std::uint32_t>() != kSnapVersion) {
        why = "solo battle record";
        detail = "the SOLOCB record is another version";
        return false;
    }
    return true;
}

// The record back into a Save (SnapRestore's and the self-test's); its
// set-back goes to g_snap_set_back.
StepSeven g_snap_set_back{};
Save ReadRecord(bof3::SnapshotReader& r) {
    if (r.Get<std::uint32_t>() != kSnapVersion) bof3::Fatal("snapshot: SOLOCB's record is another version");
    Save s{};
    s.armed = r.Get<std::uint8_t>();
    s.set_written = r.Get<std::uint8_t>();
    s.party_written = r.Get<std::uint8_t>();
    s.battle = r.Get<std::uint32_t>();
    s.saved = GetBytes(r);
    s.written = GetBytes(r);
    g_snap_set_back = r.Get<StepSeven>();
    {
        const StepSeven& b = g_snap_set_back;
        if (b.pending > 1 || b.count_back > 1 || b.set_back > 1 || b.reload > 1 || b.pending != (b.count_back | b.set_back) ||
            (b.reload && !b.set_back) || (b.count_back && (b.count < 2 || b.count > 3)))
            bof3::Fatal("snapshot: SOLOCB's record holds a step-7 record of pending %u, count %u / %u, set %u / %u / %02X",
                        b.pending, b.count_back, b.count, b.set_back, b.reload, b.set);
    }
    if (s.armed > 1 || s.set_written > 1 || s.party_written > 1 || (!s.armed && (s.set_written || s.party_written)))
        bof3::Fatal("snapshot: SOLOCB's record holds armed %u, set %u, party %u", s.armed, s.set_written, s.party_written);
    return s;
}

void SnapRestore(bof3::SnapshotReader& r) {
    const Save s = ReadRecord(r);
    // A battle of one taken in the mode and loaded with SoloCharacter off
    // still has the champion alone in the game's own bytes: the save comes
    // back too, so GameMode5_Place puts the party back as it would have.
    if (s.armed && !bof3::BehaviourOn(bof3::behaviours::Id::SoloCharacter))
        bof3::Log("DIV-0228 SoloCharacter is off and this quicksave was taken in a battle of one: the party goes back "
                  "after the battle as the mode would have put it");
    g_save = s;
    g_set_back = g_snap_set_back;
}

}  // namespace

void SoloBattle_Arm(unsigned char event_flags) {
    if (!Solo_ChampionFights()) return;
    if (g_set_back.pending)
        bof3::Fatal("DIV-0228 SoloCharacter: battle %u starts with the last battle's party still waiting for step 7",
                    static_cast<unsigned>(Game_BattleCount));
    if (g_save.armed)
        bof3::Fatal("DIV-0228 SoloCharacter: battle %u starts while battle %u's party is still saved: a way out of mode 5 "
                    "that neither reaches GameMode5_Place nor restarts through GameMode_Start",
                    static_cast<unsigned>(Game_BattleCount), static_cast<unsigned>(g_save.battle));
    // A stand-down arms nothing, so the step-3 write, the step-6 restore and
    // hold and the step-7 restore (each acts only on what Arm saved) do
    // nothing either.
    const FightRule* const rule = RuleFor(EventBattle_Id);
    if (rule && rule->rule == Rule::kStandDown) {
        bof3::Log("DIV-0228 SoloCharacter: battle %u is event battle 0x%02X, %s: fought by the game's own party (the owner, "
                  "round doc 6g)",
                  static_cast<unsigned>(Game_BattleCount), static_cast<unsigned>(EventBattle_Id), rule->why);
        return;
    }
    const int record = Solo_ChampionRecord();
    const auto id = static_cast<unsigned char>(Solo_ChampionId());
    if (Character_IndexById[id] != record)
        bof3::Fatal("DIV-0228 SoloCharacter: the champion's record %d holds id 0x%02X, which Character_IndexById maps to "
                    "record %u",
                    record, id, static_cast<unsigned>(Character_IndexById[id]));
    const Bytes live = ReadLive();
    if (Alone(live, id)) {
        bof3::Log("DIV-0228 SoloCharacter: battle %u already has the champion (id 0x%02X) alone; nothing to change",
                  static_cast<unsigned>(Game_BattleCount), id);
        return;
    }
    // The gate's one stated stand-down (docs/solo-character-combat.md 7): a
    // record with no HP is not fought from - the battle would open with its
    // only member down. A battle's way back to the field leaves a fallen
    // member at 1 (BattleEnd_Finish), but a scene can leave a record at 0:
    // chapter 8's Scena08_Scene11 gives the record of id 4 HP 0 and status
    // 0x6000 (docs/save-block/range-b-characters.md 4).
    if (CharacterRecords[record].hp == 0) {
        bof3::Log("DIV-0228 SoloCharacter: the champion's record %d has 0 HP; battle %u is fought by the game's own party",
                  record, static_cast<unsigned>(Game_BattleCount));
        return;
    }
    if (rule && rule->rule == Rule::kNeedsRecord && record != rule->arg) {
        bof3::Log("DIV-0228 SoloCharacter: battle %u is event battle 0x%02X, %s; the champion is record %d, not %u: fought by "
                  "the game's own party",
                  static_cast<unsigned>(Game_BattleCount), static_cast<unsigned>(EventBattle_Id), rule->why, record,
                  static_cast<unsigned>(rule->arg));
        return;
    }
    const unsigned char need = rule && rule->rule == Rule::kNeedsIdInRow ? rule->arg : kNone;
    const int chosen = ChooseRow(Rows(), live.set, id, need);
    if (chosen == kNoRow) {
        if (need == kNone)
            bof3::Fatal("DIV-0228 SoloCharacter: no party-set row holds the champion's id 0x%02X", id);
        bof3::Log("DIV-0228 SoloCharacter: battle %u is event battle 0x%02X, %s; no party-set row holds the champion's id "
                  "0x%02X beside id 0x%02X: fought by the game's own party",
                  static_cast<unsigned>(Game_BattleCount), static_cast<unsigned>(EventBattle_Id), rule->why, id, need);
        return;
    }
    const int row = chosen == kKeep ? -1 : chosen;
    Save s{};
    s.armed = 1;
    s.set_written = row >= 0 ? 1 : 0;
    s.battle = Game_BattleCount;
    s.saved = live;
    s.written = Written(live, id, row);
    if (row >= 0) {
        if (event_flags & 0x10) {
            // The step skips its select for this event battle, so the battle
            // draws from the field's set file (the battle's end reloads that
            // file with a mode-0 select, BattleEnd_AwaitRestore, because the
            // battle file replaces it). The row's field file is loaded here, as
            // a mode-0 select loads it, and waited for.
            PartySet_Select(static_cast<unsigned>(row), 0);
            WaitLoad();
            s.written.set = AreaW0b_PartySet;
        } else {
            // The step's own PartySet_Select loads this row's battle file
            // (mode 1 / 2 by the facing's bit 1) and records the row; step 2
            // waits for it (File_LoadDone).
            AreaW0b_PartySet = s.written.set;
        }
    }
    g_save = s;
    bof3::Log("DIV-0228 SoloCharacter: battle %u fought by the champion alone (record %d, id 0x%02X): second list "
              "%02X %02X %02X count %u formation %u set %02X saved (the party is written at step 3), first list "
              "%02X %02X %02X; set %s",
              static_cast<unsigned>(s.battle), record, id, live.list2[0], live.list2[1], live.list2[2], live.count,
              live.formation, live.set, Cond_Flags[kFirstList], Cond_Flags[kFirstList + 1], Cond_Flags[kFirstList + 2],
              row >= 0 ? ((event_flags & 0x10) ? "row changed, its field file loaded" : "row changed, the step loads its battle file")
                       : "kept (it holds the champion)");
    LogRecord("before", record);
}

void SoloBattle_WriteParty() {
    if (!g_save.armed) return;
    if (g_save.party_written)
        bof3::Fatal("DIV-0228 SoloCharacter: battle %u's party is written a second time", static_cast<unsigned>(g_save.battle));
    if (Game_BattleCount != g_save.battle)
        bof3::Fatal("DIV-0228 SoloCharacter: GameMode5_Load of battle %u holds battle %u's saved party",
                    static_cast<unsigned>(Game_BattleCount), static_cast<unsigned>(g_save.battle));
    // Steps 1 and 2 write none of these (GameMode5_Turn, GameMode5_ToPlaces
    // and their Encounter_ callees only read them); a byte that differs from
    // step 1's read is a writer this module does not know.
    const Bytes live = ReadLive();
    if (std::memcmp(live.list2, g_save.saved.list2, 3) != 0 || live.count != g_save.saved.count ||
        live.formation != g_save.saved.formation)
        bof3::Fatal("DIV-0228 SoloCharacter: the party bytes changed between steps 1 and 3 of battle %u (second list "
                    "%02X %02X %02X count %u formation %u, saved %02X %02X %02X count %u formation %u)",
                    static_cast<unsigned>(g_save.battle), live.list2[0], live.list2[1], live.list2[2], live.count,
                    live.formation, g_save.saved.list2[0], g_save.saved.list2[1], g_save.saved.list2[2],
                    g_save.saved.count, g_save.saved.formation);
    WriteParty(g_save.written);
    g_save.party_written = 1;
}

bool SoloBattle_Restore() {
    g_place_leader = kNone;
    if (!g_save.armed) return false;
    if (!g_save.party_written)
        bof3::Fatal("DIV-0228 SoloCharacter: GameMode5_Place of battle %u before its party was written (step 3)",
                    static_cast<unsigned>(g_save.battle));
    if (Game_BattleCount != g_save.battle)
        bof3::Fatal("DIV-0228 SoloCharacter: GameMode5_Place of battle %u holds battle %u's saved party",
                    static_cast<unsigned>(Game_BattleCount), static_cast<unsigned>(g_save.battle));
    const Bytes live = ReadLive();
    const Restored r = Restore(g_save, live);
    WriteParty(r.bytes);
    if (g_set_back.pending)
        bof3::Fatal("DIV-0228 SoloCharacter: battle %u's party is still waiting for step 7", static_cast<unsigned>(g_save.battle));
    {
        // The count as it now stands - ours put back, or an exit hook's own
        // (Boss25_Exit's 3) - is held at 1 after Party_PlaceAtSlots whenever
        // it is above 1: those records were not built for this battle.
        StepSeven b{};
        const unsigned count = Field_MemberCount;
        if (count > 3) bof3::Fatal("DIV-0228 SoloCharacter: battle %u ends with a member count of %u", static_cast<unsigned>(g_save.battle), count);
        if (count > 1) {
            b.count_back = 1;
            b.count = static_cast<std::uint8_t>(count);
        }
        if (r.restored & 0x20) {
            b.set_back = 1;
            b.reload = r.reload ? 1 : 0;
            b.set = r.bytes.set;
        }
        b.pending = static_cast<std::uint8_t>(b.count_back | b.set_back);
        g_set_back = b;
    }
    const unsigned char champion = g_save.written.list2[0];
    g_save = Save{};
    LogRecord("after", Character_IndexById[champion]);
    bof3::Log("DIV-0228 SoloCharacter: battle over, the party put back (restored mask 0x%02X): second list %02X %02X %02X "
              "count %u formation %u set %02X%s; first list %02X %02X %02X",
              r.restored, r.bytes.list2[0], r.bytes.list2[1], r.bytes.list2[2], r.bytes.count, r.bytes.formation,
              static_cast<unsigned>(r.bytes.set), (r.restored & 0x20) ? " (at step 7)" : "", Cond_Flags[kFirstList],
              Cond_Flags[kFirstList + 1], Cond_Flags[kFirstList + 2]);
    const unsigned char ids[3] = {ObjTrio[0].char_id, ObjTrio[1].char_id, ObjTrio[2].char_id};
    if (LeaderMissed(Field_MemberCount, ids, Cond_Flags[kFirstList], champion)) {
        g_place_leader = champion;
        return true;
    }
    return false;
}

unsigned char SoloBattle_PlaceLeaderId() {
    if (g_place_leader == kNone) bof3::Fatal("solo battle: SoloBattle_PlaceLeaderId asked without SoloBattle_Restore's yes");
    return g_place_leader;
}

void SoloBattle_AfterPlaceAtSlots() {
    if (!g_set_back.count_back) return;
    Field_MemberCount = 1;
}

void SoloBattle_RestoreAtStep7() {
    if (!g_set_back.pending) return;
    if (g_set_back.count_back) {
        if (Field_MemberCount != 1)
            bof3::Fatal("DIV-0228 SoloCharacter: the member count held at 1 since step 6 is %u at step 7",
                        static_cast<unsigned>(Field_MemberCount));
        Field_MemberCount = g_set_back.count;
    }
    if (g_set_back.set_back) {
        if (g_set_back.reload) {
            // Undo the battle's end: its mode-0 select loaded the battle row's
            // field file; the field's own row is loaded the same way and waited
            // for, with no frame drawn between the load and Field_PartyLoad(0).
            PartySet_Select(g_set_back.set & 0x7Fu, 0);
            WaitLoad();
        }
        AreaW0b_PartySet = g_set_back.set;
    }
    bof3::Log("DIV-0228 SoloCharacter: at step 7, before the walkers are rebuilt: count %u%s, set %02X%s",
              static_cast<unsigned>(Field_MemberCount), g_set_back.count_back ? " (held at 1 since step 6)" : "",
              static_cast<unsigned>(AreaW0b_PartySet),
              g_set_back.set_back ? (g_set_back.reload ? " (put back, its field file loaded)" : " (put back)") : "");
    g_set_back = StepSeven{};
}

bool SoloBattle_ChampionFightsThis() {
    if (!Solo_ChampionFights()) return false;
    // 0x904AB0: the battle's member count, Party_Count(1) stored by
    // GameMode5_Script (mode_rest_callees.h kMemberCount).
    constexpr std::uint32_t kBattleMemberCount = 0x904AB0;
    const unsigned count = *reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(::bof3::Img(kBattleMemberCount)));
    return count == 1 && ObjTrio[0].char_index == static_cast<unsigned>(Solo_ChampionRecord());
}

void SoloBattle_Drop() {
    if (g_set_back.pending) {
        bof3::Log("DIV-0228 SoloCharacter: a count or set waiting for step 7 dropped at GameMode_Start");
        g_set_back = StepSeven{};
    }
    if (!g_save.armed) return;
    bof3::Log("DIV-0228 SoloCharacter: battle %u's saved party dropped at GameMode_Start (the battle did not return to the field)",
              static_cast<unsigned>(g_save.battle));
    g_save = Save{};
}

void SoloBattle_Inject() {
    if (bof3::IsExeHost() ? bof3::os::GetEnv("BOF3X_SELFTEST_ONLY", nullptr, 0) > 0 : bof3::WantsShadow("solo"))
        SoloBattle_SelfTest();
    bof3::Snapshot_Register("SOLOCB", &SnapSave, &SnapRestore, nullptr, &SnapCheck);
}

void SoloBattle_SelfTest() {
    int checks = 0;
    const auto check = [&checks](bool ok, const char* what) {
        if (!ok) bof3::Fatal("solo battle self-test: %s", what);
        ++checks;
    };
    // The stand-downs: the three Beyd fights and nothing else (a random
    // encounter, 0, and the fights beside them).
    check(RuleFor(0x19) && RuleFor(0x19)->rule == Rule::kStandDown && RuleFor(0x1A) &&
              RuleFor(0x1A)->rule == Rule::kStandDown && RuleFor(0x1B) && RuleFor(0x1B)->rule == Rule::kStandDown,
          "the Beyd fights stand down");
    check(RuleFor(0x10) && RuleFor(0x10)->rule == Rule::kNeedsIdInRow && RuleFor(0x10)->arg == 1, "fight 16 wants id 1");
    check(RuleFor(0x22) && RuleFor(0x22)->rule == Rule::kNeedsRecord && RuleFor(0x22)->arg == 4, "fight 34 wants record 4");
    {
        unsigned with_rule = 0;
        for (unsigned f = 0; f < 0x100; ++f)
            if (RuleFor(static_cast<unsigned char>(f))) ++with_rule;
        check(with_rule == 5 && !RuleFor(0), "five fights have a rule, a random encounter none");
    }
    {
        // The game's 19 rows as M1 read them (round doc section 2).
        static const unsigned char kGame[] = {0, 1, 2, 0, 1, 5, 0, 1, 6, 0, 2, 5, 0, 2, 6, 0, 3, 4, 0, 5, 6,
                                              2, 4, 7, 2, 5, 7, 2, 6, 7, 2, 7, 8, 2, 7, 0xA, 3, 4, 9, 4, 5, 7,
                                              4, 6, 7, 4, 7, 8, 5, 6, 7, 5, 7, 8, 6, 7, 8};
        // Fight 16 from row 0 (0, 1, 2), needing id 1: Ryu, Nina, Garr keep it;
        // Momo row 1 (0, 1, 5), Peco row 2 (0, 1, 6); Rei (4) has no row with 1.
        check(ChooseRow(kGame, 0x80, 0, 1) == kKeep && ChooseRow(kGame, 0x80, 1, 1) == kKeep &&
                  ChooseRow(kGame, 0x80, 2, 1) == kKeep,
              "fight 16: row 0 kept for ids 0, 1, 2");
        check(ChooseRow(kGame, 0x80, 5, 1) == 1 && ChooseRow(kGame, 0x80, 6, 1) == 2, "fight 16: Momo row 1, Peco row 2");
        check(ChooseRow(kGame, 0x80, 4, 1) == kNoRow, "fight 16: no row for Rei beside id 1");
        // No need: the most ids of the current row kept. Row 5 (0, 3, 4),
        // Garr (2): rows with 2 sharing 0 - row 0 first; Momo (5) from row 0:
        // rows 1 and 3 keep two each, row 1 first.
        check(ChooseRow(kGame, 0x85, 2, kNone) == 0, "Garr from row 5: row 0");
        check(ChooseRow(kGame, 0x80, 5, kNone) == 1, "Momo from row 0: row 1");
        check(ChooseRow(kGame, 0x85, 4, kNone) == kKeep, "Rei in row 5: kept");
        // Adult rows: Nina (8) from row 15 (4, 7, 8) kept; Momo (5) from row 15:
        // row 13 (4, 5, 7) and row 17 (5, 7, 8) keep two each, row 13 first.
        check(ChooseRow(kGame, 0x8F, 8, kNone) == kKeep && ChooseRow(kGame, 0x8F, 5, kNone) == 13, "adult rows");
        check(ChooseRow(kGame, kNone, 6, kNone) == 2, "no current set: the first row holding the id");
    }
    // A table of our own, shaped as the game's: rows (0,1,2) (0,3,4) (2,5,7) (6,7,8).
    static const unsigned char kRows[] = {0, 1, 2, 0, 3, 4, 2, 5, 7, 6, 7, 8};
    check(RowHolds(kRows, 0, 1) && !RowHolds(kRows, 0, 5) && RowHolds(kRows, 3, 8), "RowHolds");
    check(SetHolds(kRows, 0x80, 2) && SetHolds(kRows, 0x02, 5) && !SetHolds(kRows, 0x81, 5), "SetHolds by the low seven bits");
    check(!SetHolds(kRows, kNone, 0), "SetHolds: no set holds nothing");

    // Alone: the list and the count both.
    const Bytes alone{{5, kNone, kNone}, 1, 0, 0x82};
    check(Alone(alone, 5) && !Alone(alone, 4), "Alone");
    const Bytes alone3{{5, kNone, kNone}, 3, 0, 0x82};
    check(!Alone(alone3, 5), "Alone wants a count of 1");

    // Arm on a party of three, the set kept.
    const Bytes field{{0, 1, 2}, 3, 2, 0x80};
    const Bytes w = Written(field, 1, -1);
    check(w.list2[0] == 1 && w.list2[1] == kNone && w.list2[2] == kNone && w.count == 1 && w.formation == 0 && w.set == 0x80,
          "Written, set kept");
    // And with the set changed to row 2: recorded as the step's select records it.
    const Bytes w2 = Written(field, 5, 2);
    check(w2.set == 2 && w2.list2[0] == 5, "Written, set changed");

    // Restore after an untouched battle: every byte back; the battle's own
    // selects left the set's bit 7 different, and the row still counts.
    Save s{1, 0, 1, 7, field, w};
    Bytes live = w;
    live.set = 0x80;   // the battle's end select (mode 0) set bit 7 again
    Restored r = Restore(s, live);
    check(std::memcmp(&r.bytes, &field, sizeof field) == 0 && r.restored == 0x1F && !r.reload, "Restore, untouched, set kept");

    // Set changed: the row is put back and its field file reloaded.
    Save s2{1, 1, 1, 7, field, w2};
    Bytes live2 = w2;
    live2.set = 0x82;
    r = Restore(s2, live2);
    check(std::memcmp(&r.bytes, &field, sizeof field) == 0 && r.restored == 0x3F && r.reload, "Restore, set changed");

    // An exit hook that restored its own party (Boss25_Exit: the count 3 and
    // its list): those bytes are its; ours go back where still ours.
    Save s3{1, 0, 1, 7, Bytes{{0, kNone, kNone}, 1, 1, 0x80}, Bytes{{4, kNone, kNone}, 1, 0, 0x80}};
    Bytes live3{{0, 1, 2}, 3, 0, 0x80};
    r = Restore(s3, live3);
    check(r.bytes.list2[0] == 0 && r.bytes.list2[1] == 1 && r.bytes.list2[2] == 2 && r.bytes.count == 3 &&
              r.bytes.formation == 1 && r.restored == 0x10,
          "Restore beside Boss25_Exit");

    // The set changed by someone else since (a row Arm did not write): left.
    Bytes live4 = w2;
    live4.set = 0x85;
    r = Restore(s2, live4);
    check(r.bytes.set == 0x85 && !(r.restored & 0x20) && !r.reload, "Restore leaves a set someone else changed");

    // Arm's changed set back to the same row it came from (it held the champion
    // after all): restored, nothing to load.
    Save s5{1, 1, 1, 7, Bytes{{0, 1, 2}, 3, 0, 0x82}, Bytes{{5, kNone, kNone}, 1, 0, 0x02}};
    r = Restore(s5, Bytes{{5, kNone, kNone}, 1, 0, 0x82});
    check(r.bytes.set == 0x82 && (r.restored & 0x20) && !r.reload, "Restore, same row, no load");

    // The leader at Party_PlaceAtSlots.
    const unsigned char after[3] = {5, 1, 2};   // the champion, then walkers' records turned off
    check(LeaderMissed(3, after, 0, 5), "LeaderMissed: walker leader not among the records");
    check(!LeaderMissed(3, after, 1, 5), "LeaderMissed: a walker record of the leader's id is found first");
    check(!LeaderMissed(1, after, 5, 5), "LeaderMissed: the champion leads");
    check(LeaderMissed(1, after, 1, 5), "LeaderMissed: one walker, not the champion");
    check(!LeaderMissed(0, after, 0, 5) && !LeaderMissed(4, after, 0, 5), "LeaderMissed: counts 0 and 4 are Capcom's");
    const unsigned char sorted[3] = {0, 1, 2};   // Boss25's: ObjTrio's first is not ours
    check(!LeaderMissed(3, sorted, 7, 5), "LeaderMissed: not when the first record is not the champion");

    // The participant's record, round trip on state of its own.
    {
        const Save before = g_save;
        const StepSeven before_set = g_set_back;
        g_save = s2;
        g_set_back = StepSeven{1, 1, 3, 1, 1, 0x85};
        std::vector<unsigned char> buf;
        bof3::SnapshotWriter wr(buf);
        SnapSave(wr);
        g_save = Save{};
        g_set_back = StepSeven{};
        bof3::SnapshotReader probe("SOLOCB", buf.data(), buf.size());
        std::string why, detail;
        check(SnapCheck(probe, why, detail), "SnapCheck accepts its own record");
        bof3::SnapshotReader rd("SOLOCB", buf.data(), buf.size());
        g_save = ReadRecord(rd);
        check(rd.Left() == 0 && g_save.armed == 1 && g_save.set_written == 1 && g_save.party_written == 1 && g_save.battle == 7 &&
                  std::memcmp(&g_save.saved, &s2.saved, sizeof(Bytes)) == 0 &&
                  std::memcmp(&g_save.written, &s2.written, sizeof(Bytes)) == 0 && g_snap_set_back.pending == 1 &&
                  g_snap_set_back.count_back == 1 && g_snap_set_back.count == 3 && g_snap_set_back.set_back == 1 &&
                  g_snap_set_back.reload == 1 && g_snap_set_back.set == 0x85,
              "the SOLOCB record round trip");
        std::vector<unsigned char> bad(buf);
        bad[0] ^= 0xFF;
        bof3::SnapshotReader bp("SOLOCB", bad.data(), bad.size());
        check(!SnapCheck(bp, why, detail), "SnapCheck refuses another version");
        g_save = before;
        g_set_back = before_set;
        g_snap_set_back = StepSeven{};
    }
    bof3::Log("solo battle self-test: %d checks passed", checks);
}

}  // namespace solo

#include "game/solo/solo_field.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "game/solo/solo.h"
#include "hook/behaviour.h"
#include "hook/detour.h"
#include "hook/host.h"
#include "hook/log.h"
#include "os/clock.h"
#include "os/os.h"

// The solo-character mode's field half (solo_field.h; docs/solo-character-
// field.md has the reads behind each step). What each address is:
//
//   ObjTrio            0x802D40  the three walkers, 0x14C each: +1 the state,
//                                +0x89 the character id, +8 the facing
//   CharacterRecords   0x903A70  eight records, 0xA4 each: +9 the id, +0xB
//                                bit 0 joined, bit 1 must stay (solo-
//                                character-reads.md M6, M7)
//   Cond_Flags +0xD2   0x904062  the first party list, three ids, 0xFF after
//                                the last (who walks, in order)
//   Cond_Flags +0xD5   0x904065  the second list (the battle's order): the
//                                same members (Party_Join writes both)
//   Field_MemberCount            the walkers
//   AreaW0b_PartySet   0x90412C  the loaded party set's row, bit 7 when
//                                loaded with mode 0
//   MoveScript_EffectArg +0xC    0x669750: the 19 party-set rows of three
//                                ids that PartySet_Find searches
//   Character_IndexById 0x66972C an id to its record, 24 entries
//   Game_Mode                    2 is the field (symbols.toml)
//   Frame_Counter      0x937F94  WinMain's count of frames, read around a
//                                party-set load to measure it

namespace solo {
namespace {

constexpr int kRecords = 8;
constexpr unsigned kIds = 24;          // Character_IndexById's entries
constexpr unsigned kSetRows = 0x13;    // PartySet_Find's row count
constexpr unsigned kObjStride = 0x14C; // ObjTrio's record
constexpr unsigned kFieldMode = 2;     // Game_Mode: the field

// ---- The decisions, on state passed in (the self-test drives them) --------

// The party as the decisions see it.
struct Party {
    unsigned char first[3];
    unsigned char second[3];
    unsigned char count;
    unsigned char flags[kRecords];    // CharacterRecords[r] +0xB
    unsigned char char_id[kRecords];  // CharacterRecords[r] +9
};

// Whether the party is one the field half may change: the count 0..3, every
// id below the count a known id (Character_IndexById's 24), no record twice,
// and the second list holding the same members as the first below the count
// (Party_Join writes both; a scene that set the second list alone - chapter
// 6's SortParty - owns the party until it puts it back). `why` names the
// first test that failed.
bool PartyValid(const Party& p, const unsigned char* index_by_id, const char** why) {
    if (p.count > 3) {
        *why = "the member count is above 3";
        return false;
    }
    for (unsigned i = p.count; i < 3; ++i)
        if (p.first[i] != 0xFF || p.second[i] != 0xFF) {
            // The game's own one-walker states keep the lists whole and the
            // count at 1 (PartyForm_Reload's "lone" path; measured on the
            // fairyVillage save: lists 07 08 05, one walker): the game owns it.
            *why = "a list holds an id past the member count (the game walks fewer than its lists)";
            return false;
        }
    bool seen[kRecords] = {};
    for (unsigned i = 0; i < p.count; ++i) {
        const unsigned char id = p.first[i];
        if (id >= kIds) {
            *why = "a first-list id below the count is not a character id";
            return false;
        }
        const unsigned r = index_by_id[id];
        if (r >= static_cast<unsigned>(kRecords) || seen[r]) {
            *why = "a record is on the map twice, or an id maps to no record";
            return false;
        }
        seen[r] = true;
        unsigned k = 0;
        while (k < p.count && p.second[k] != id) ++k;
        if (k == p.count) {
            *why = "the second list does not hold the first list's members (a scene owns it)";
            return false;
        }
    }
    *why = nullptr;
    return true;
}

bool Marked(const Party& p, const unsigned char* index_by_id, unsigned slot) {
    return (p.flags[index_by_id[p.first[slot]]] & 2) != 0;
}

// The fold's decision: every marked member stays; of the members not marked,
// the first in the first list's order stays (the leader, when the leader is
// not marked); the others go to the reserve.
struct FoldPlan {
    bool valid;
    const char* why;
    unsigned drops;
    unsigned char drop[3];  // ids, in list order
};

FoldPlan DecideFold(const Party& p, const unsigned char* index_by_id) {
    FoldPlan f{};
    f.valid = PartyValid(p, index_by_id, &f.why);
    if (!f.valid) return f;
    bool kept_unmarked = false;
    for (unsigned i = 0; i < p.count; ++i) {
        if (Marked(p, index_by_id, i)) continue;
        if (!kept_unmarked) {
            kept_unmarked = true;
            continue;
        }
        f.drop[f.drops++] = p.first[i];
    }
    return f;
}

// The next joined character after record `from`, in record order and round
// to the start: joined (+0xB bit 0), not marked (a marked member is on the
// map already, or the story holds them elsewhere), not on the map, its id a
// known one that maps back to it. -1 when none.
int NextJoined(const Party& p, const unsigned char* index_by_id, int from) {
    bool on_map[kRecords] = {};
    for (unsigned i = 0; i < p.count; ++i) on_map[index_by_id[p.first[i]]] = true;
    for (int step = 1; step < kRecords; ++step) {
        const int r = (from + step) % kRecords;
        if (!(p.flags[r] & 1) || (p.flags[r] & 2) || on_map[r]) continue;
        const unsigned char id = p.char_id[r];
        if (id >= kIds || index_by_id[id] != r) continue;
        return r;
    }
    return -1;
}

// The L swap's bring-out: due when the member who leads now is not marked,
// is the map's only member not marked, and someone joined is in reserve.
// The leader is then replaced by the next joined character after them (the
// exchange has moved them off the lead by then).
struct Bring {
    bool due;
    unsigned char from_id, to_id;
    int to_record;
    unsigned char after[3];  // the first list's members after it (order not kept)
    unsigned char count;
};

Bring DecideBring(const Party& p, const unsigned char* index_by_id) {
    Bring b{};
    const char* why;
    if (!PartyValid(p, index_by_id, &why) || p.count < 2) return b;
    if (Marked(p, index_by_id, 0)) return b;
    for (unsigned i = 1; i < p.count; ++i)
        if (!Marked(p, index_by_id, i)) return b;
    const int next = NextJoined(p, index_by_id, index_by_id[p.first[0]]);
    if (next < 0) return b;
    b.due = true;
    b.from_id = p.first[0];
    b.to_id = p.char_id[next];
    b.to_record = next;
    b.count = p.count;
    for (unsigned i = 0; i < p.count; ++i) b.after[i] = i == 0 ? b.to_id : p.first[i];
    return b;
}

// PartySet_Find (0x5368F0, mode_11.cpp) without its side effects: the row
// for ids a, b, c (0xFF for none) as the game would find it, or 0xFF where
// the game would call PartySet_ErrorLoop, which never returns. The game's
// search can miss a row that holds the ids (it narrows on the lowest id's
// column-0 rows first), so a row is asked of this, never assumed. A current
// set past the 19 rows answers 0xFF (the game would read past the table).
unsigned char FindRow(const unsigned char* table, unsigned char current, unsigned char a, unsigned char b,
                      unsigned char c) {
    unsigned char ids[3] = {a, b, c};
    if (current != 0xFF) {
        if ((current & 0x7Fu) >= kSetRows) return 0xFF;
        const unsigned char* const row = table + (current & 0x7Fu) * 3u;
        unsigned i = 0;
        for (; i < 3; ++i) {
            if (ids[i] == 0xFF) continue;
            unsigned j = 0;
            while (ids[i] != row[j])
                if (++j >= 3) break;
            if (j >= 3) break;
        }
        if (i >= 3) return static_cast<unsigned char>(current & 0x7F);
    }
    unsigned char count = 0;
    for (unsigned k = 0; k < 3; ++k)
        if (ids[k] != 0xFF) ++count;
    for (unsigned x = 0; x < 2; ++x)
        for (unsigned y = x + 1; y < 3; ++y)
            if (ids[x] > ids[y]) {
                const unsigned char t = ids[x];
                ids[x] = ids[y];
                ids[y] = t;
            }
    unsigned char limit = kSetRows, start = 0;
    unsigned matched = 0;
    for (unsigned column = 0; column < 3; ++column) {
        unsigned row = start;
        if (row >= limit) continue;
        for (;;) {
            const unsigned char* cell = table + row * 3u + column;
            const unsigned char want = ids[matched];
            if (want == *cell) {
                if (matched == count - 1u) return static_cast<unsigned char>(row);
                start = static_cast<unsigned char>(row);
                while (row < kSetRows && want == *cell) {
                    ++row;
                    cell += 3;
                }
                limit = static_cast<unsigned char>(row);
                ++matched;
            }
            ++row;
            if (row >= limit) break;
        }
    }
    return 0xFF;
}

bool RowHolds(const unsigned char* table, unsigned row, unsigned char id) {
    if (row >= kSetRows) return false;
    for (unsigned k = 0; k < 3; ++k)
        if (table[row * 3u + k] == id) return true;
    return false;
}

// The ids a set must hold: the members (count of them), and the champion's id
// as well when it is not among them and there is room. `out` gets three
// bytes, 0xFF-filled.
unsigned SetIds(const unsigned char* members, unsigned count, int champion_id, unsigned char out[3]) {
    out[0] = out[1] = out[2] = 0xFF;
    unsigned n = 0;
    for (unsigned i = 0; i < count && n < 3; ++i) out[n++] = members[i];
    if (champion_id >= 0) {
        bool there = false;
        for (unsigned i = 0; i < n; ++i) there = there || out[i] == champion_id;
        if (!there && n < 3) out[n++] = static_cast<unsigned char>(champion_id);
    }
    return n;
}

// The row to load for `members` (plus the champion where a row holds all of
// them), given the current set: kSetRows when the current set already holds
// what is asked (no load), 0xFF when no row the game would find holds even
// the members. `with_champion` says which was found.
unsigned ChooseRow(const unsigned char* table, unsigned char current, const unsigned char* members, unsigned count,
                   int champion_id, bool* with_champion, unsigned char ids_out[3]) {
    unsigned char ids[3];
    const unsigned n_with = SetIds(members, count, champion_id, ids);
    const bool champion_added = champion_id >= 0 && n_with > count;
    const bool champion_on_map = champion_id >= 0 && !champion_added && n_with == count &&
                                 [&] {
                                     for (unsigned i = 0; i < count; ++i)
                                         if (members[i] == champion_id) return true;
                                     return false;
                                 }();
    *with_champion = champion_added || champion_on_map;
    unsigned char row = FindRow(table, current, ids[0], ids[1], ids[2]);
    if (row == 0xFF && champion_added) {
        *with_champion = false;
        SetIds(members, count, -1, ids);
        row = FindRow(table, current, ids[0], ids[1], ids[2]);
    }
    std::memcpy(ids_out, ids, 3);
    if (row == 0xFF) return 0xFF;
    if (current != 0xFF && row == (current & 0x7Fu)) return kSetRows;
    return row;
}

// ---- The live state ---------------------------------------------------------

unsigned char* First() { return move_script::At(::bof3::Img(Cond_Flags_addr) + 0xd2); }
unsigned char* Second() { return move_script::At(::bof3::Img(Cond_Flags_addr) + 0xd5); }
unsigned char* Walker(unsigned i) { return rv::Bytes(ObjTrio) + i * kObjStride; }
const unsigned char* SetTable() { return move_script::At(::bof3::Img(MoveScript_EffectArg_addr) + 0xc); }

Party ReadParty() {
    Party p{};
    std::memcpy(p.first, First(), 3);
    std::memcpy(p.second, Second(), 3);
    p.count = Field_MemberCount;
    for (int r = 0; r < kRecords; ++r) {
        p.flags[r] = CharacterRecords[r].flags;
        p.char_id[r] = CharacterRecords[r].char_id;
    }
    return p;
}

// The signature (round doc 0a item 8): both lists, the count, the eight
// records' +0xB bits 0 and 1.
struct Signature {
    unsigned char lists[6];
    unsigned char count;
    unsigned char bits[kRecords];
    bool operator==(const Signature& o) const { return std::memcmp(this, &o, sizeof *this) == 0; }
};

Signature ReadSignature() {
    Signature s{};
    std::memcpy(s.lists, First(), 6);
    s.count = Field_MemberCount;
    for (int r = 0; r < kRecords; ++r) s.bits[r] = static_cast<unsigned char>(CharacterRecords[r].flags & 3);
    return s;
}

// The cache (solo_field.h): the last signature the fold settled, and whether
// the mode was active then.
Signature g_signature{};
bool g_was_active = false;

void FormatParty(char* out, std::size_t size) {
    const unsigned char* f = First();
    const unsigned char* s = Second();
    std::snprintf(out, size, "first %02X %02X %02X second %02X %02X %02X count %u set %02X", f[0], f[1], f[2], s[0],
                  s[1], s[2], static_cast<unsigned>(Field_MemberCount), static_cast<unsigned>(AreaW0b_PartySet));
}

bool ModeOn() { return bof3::BehaviourOn(bof3::behaviours::Id::SoloCharacter); }

int ChampionIdOrNone() {
    const unsigned id = Solo_ChampionId();
    return id < kIds ? static_cast<int>(id) : -1;
}

// Every walker's sprite set again from the loaded set, as PartyForm_Reload
// does after its PartySet_Load (Field_MemberSprite by its +0x89 and slot,
// Sprite_SetAnimation by its facing): a new row moves the ids' columns.
void ResetWalkerSprites() {
    for (unsigned i = 0; i < Field_MemberCount && i < 3; ++i) {
        unsigned char* const w = Walker(i);
        Sprite_Current = w;
        SH_CALL(Field_MemberSprite)(w[0x89], i);
        SH_CALL(Sprite_SetAnimation)(rv::As<records::PartyMember>(w)->direction);
    }
}

// The party set for `ids` loaded as PartyForm_Reload loads one
// (PartySet_Load mode 0: both files, each waited out a frame at a time), the
// walkers' sprites set again, and the frames it took logged. Sprite_Current
// and Field_State are put back.
void LoadSet(const unsigned char ids[3], const char* why) {
    unsigned char* const sc = Sprite_Current;
    unsigned char* const fs = Field_State;
    const unsigned before = AreaW0b_PartySet;
    const unsigned start = Frame_Counter;
    const std::uint64_t t0 = bof3::os::Counter();
    SH_CALL(PartySet_Load)(ids[0], ids[1], ids[2], 0);
    const std::uint64_t t1 = bof3::os::Counter();
    const unsigned frames = Frame_Counter - start;
    const unsigned micro = static_cast<unsigned>((t1 - t0) * 1000000u / bof3::os::CounterFrequency());
    ResetWalkerSprites();
    Sprite_Current = sc;
    Field_State = fs;
    bof3::Log("DIV-0228 solo field: party set %02X -> %02X for ids %02X %02X %02X (%s): the load took %u frame(s), %u us",
              before, static_cast<unsigned>(AreaW0b_PartySet), ids[0], ids[1], ids[2], why, frames, micro);
}

}  // namespace

void SoloField_Stand() {
    if (!ModeOn()) return;
    if (!Solo_Active()) {
        g_was_active = false;
        return;
    }
    const Signature now = ReadSignature();
    if (g_was_active && now == g_signature) return;
    // Not while anything but the field owns the frame or a request is
    // pending (Field_Request 6 is the members screen a join or a scene
    // raised): the next frame asks again.
    if (Game_Mode != kFieldMode || Field_Request != 0) return;
    const bool entering = !g_was_active;
    g_was_active = true;
    g_signature = now;
    const Party p = ReadParty();
    const FoldPlan f = DecideFold(p, Character_IndexById);
    char before[96];
    FormatParty(before, sizeof before);
    if (!f.valid) {
        bof3::Log("DIV-0228 solo field: no fold, %s: %s", f.why, before);
        return;
    }
    unsigned char* const sc = Sprite_Current;
    unsigned char* const fs = Field_State;
    if (f.drops != 0) {
        for (unsigned k = 0; k < f.drops; ++k) SH_CALL(Party_Remove)(f.drop[k]);
        SH_CALL(Party_ReloadPalettes)();
        Sprite_Current = sc;
        Field_State = fs;
        char after[96];
        FormatParty(after, sizeof after);
        bof3::Log("DIV-0228 solo field: fold (%s): %s -> %s; to the reserve: %02X%s", entering ? "the mode enters" : "the party changed",
                  before, after, f.drop[0], f.drops > 1 ? " and another" : "");
    } else if (entering) {
        bof3::Log("DIV-0228 solo field: the mode is active, the map team is the rule's already: %s", before);
    }
    // The champion in the party set beside the walkers where a row holds
    // them all, so the battle needs no load (round doc M1, 6a): loaded here,
    // at the frame control comes back, rather than at the battle's start.
    bool with_champion = false;
    unsigned char ids[3];
    const unsigned row = ChooseRow(SetTable(), AreaW0b_PartySet, First(), Field_MemberCount, ChampionIdOrNone(),
                                   &with_champion, ids);
    if (row < kSetRows && with_champion) LoadSet(ids, "the walkers and the champion");
    g_signature = ReadSignature();
}

void SoloField_SwapStart() {
    if (!ModeOn() || !Solo_Active()) return;
    const Bring b = DecideBring(ReadParty(), Character_IndexById);
    if (!b.due) return;
    bool with_champion = false;
    unsigned char ids[3];
    const unsigned row = ChooseRow(SetTable(), AreaW0b_PartySet, b.after, b.count, ChampionIdOrNone(), &with_champion, ids);
    if (row == 0xFF) {
        bof3::Log("DIV-0228 solo field: no party-set row the game finds holds ids %02X %02X %02X: the swap brings no one out",
                  ids[0], ids[1], ids[2]);
        return;
    }
    if (row == kSetRows) return;
    LoadSet(ids, with_champion ? "the swap's bring-out, with the champion" : "the swap's bring-out");
}

unsigned SoloField_ExchangePlan() {
    if (!ModeOn() || !Solo_Active()) return kNoPlan;
    const Bring b = DecideBring(ReadParty(), Character_IndexById);
    if (!b.due) return kNoPlan;
    if (!RowHolds(SetTable(), AreaW0b_PartySet & 0x7Fu, b.to_id)) {
        bof3::Log("DIV-0228 solo field: the loaded set %02X does not hold id %02X: the swap brings no one out",
                  static_cast<unsigned>(AreaW0b_PartySet), b.to_id);
        return kNoPlan;
    }
    return (static_cast<unsigned>(b.from_id) << 8) | b.to_id;
}

void SoloField_ExchangeApply(unsigned plan) {
    if (plan == kNoPlan) return;
    const auto from = static_cast<unsigned char>(plan >> 8);
    const auto to = static_cast<unsigned char>(plan);
    unsigned j = 1;
    while (j < Field_MemberCount && Walker(j)[0x89] != from) ++j;
    if (j >= Field_MemberCount || Walker(j)[1] != 3) {
        bof3::Log("DIV-0228 solo field: the exchange left id %02X on the lead or did not gather it: no one brought out",
                  from);
        return;
    }
    Walker(j)[0x89] = to;
    unsigned char* const second = Second();
    for (unsigned k = 0; k < Field_MemberCount; ++k)
        if (second[k] == from) second[k] = to;
    bof3::Log("DIV-0228 solo field: the swap brings id %02X out in place of id %02X (walker %u)", to, from, j);
}

bool SoloField_JoinSkipsScreen() {
    if (!ModeOn() || !Solo_Active()) return false;
    char party[96];
    FormatParty(party, sizeof party);
    bof3::Log("DIV-0228 solo field: a join with three on the map goes to the reserve, no choose-who-leaves screen: %s",
              party);
    return true;
}

void SoloField_SelfTest() {
    int checks = 0;
    // An id map of its own: ids 0..7 to records 0..7, 9 and 0xA to records 0
    // and 7, the rest to record 0 (the shape of the game's, not its bytes).
    unsigned char idx[kIds];
    for (unsigned i = 0; i < kIds; ++i) idx[i] = i < 8 ? static_cast<unsigned char>(i) : 0;
    idx[0xA] = 7;
    const auto party = [](std::initializer_list<int> first, std::initializer_list<int> second, unsigned joined,
                          unsigned marked) {
        Party p{};
        std::memset(p.first, 0xFF, 3);
        std::memset(p.second, 0xFF, 3);
        unsigned n = 0;
        for (int id : first) p.first[n++] = static_cast<unsigned char>(id);
        p.count = static_cast<unsigned char>(n);
        n = 0;
        for (int id : second) p.second[n++] = static_cast<unsigned char>(id);
        for (int r = 0; r < kRecords; ++r) {
            p.flags[r] = static_cast<unsigned char>(((joined >> r) & 1) | (((marked >> r) & 1) << 1));
            p.char_id[r] = static_cast<unsigned char>(r == 7 ? 0xA : r);
        }
        return p;
    };
    const auto expect_fold = [&](const Party& p, bool valid, std::initializer_list<int> drops, const char* name) {
        const FoldPlan f = DecideFold(p, idx);
        bool ok = f.valid == valid && f.drops == drops.size();
        unsigned k = 0;
        for (int id : drops) ok = ok && k < f.drops && f.drop[k++] == id;
        if (!ok) bof3::Fatal("solo field self-test: fold case \"%s\" (valid %d, %u drops)", name, f.valid, f.drops);
        ++checks;
    };
    // Record 0 marked (the hero, from New Game), records 0..6 joined.
    const unsigned all = 0x7F, hero = 1;
    expect_fold(party({0, 1, 2}, {0, 1, 2}, all, hero), true, {2}, "hero leads, two others: the second other leaves");
    expect_fold(party({1, 0, 2}, {0, 1, 2}, all, hero), true, {2}, "an other leads: they stay, the third leaves");
    expect_fold(party({1, 2, 0}, {2, 0, 1}, all, hero), true, {2}, "the hero last: the leader and the hero stay");
    expect_fold(party({0, 1}, {0, 1}, all, hero), true, {}, "hero and one: nothing");
    expect_fold(party({0}, {0}, all, hero), true, {}, "the hero alone: nothing (the fold never adds)");
    expect_fold(party({}, {}, all, hero), true, {}, "no one on the map: nothing");
    expect_fold(party({1, 2}, {1, 2}, all, hero), true, {2}, "the hero away: one walker");
    expect_fold(party({1, 2, 4}, {1, 2, 4}, all, hero), true, {2, 4}, "the hero away, three: one walker");
    expect_fold(party({0, 4, 2}, {0, 4, 2}, all, hero | (1u << 4)), true, {}, "a scene's mark on record 4: three stay");
    expect_fold(party({2, 0, 4}, {0, 2, 4}, all, hero | (1u << 4)), true, {}, "the mark, an other leads: three stay");
    expect_fold(party({1, 2, 4}, {1, 2, 4}, all, (1u << 4)), true, {2}, "no hero mark: the mark and the first other");
    expect_fold(party({0, 1, 2}, {0, 1, 2}, all, 0x7), true, {}, "every member marked: all stay");
    expect_fold(party({9, 1, 2}, {9, 1, 2}, all, hero), true, {2}, "the hero by another id (9 to record 0)");
    expect_fold(party({0, 1, 2}, {0, 1}, all, hero), false, {}, "the second list short a member: not ours");
    {
        Party p = party({7, 1, 5}, {7, 1, 5}, all, hero);
        p.count = 1;
        expect_fold(p, false, {}, "one walker, the lists whole (the game's lone walk): not ours");
    }
    expect_fold(party({0, 1, 2}, {0, 1, 5}, all, hero), false, {}, "the second list another member: not ours");
    expect_fold(party({0, 9}, {0, 9}, all, hero), false, {}, "record 0 twice: not ours");
    expect_fold(party({0, 0x30}, {0, 0x30}, all, hero), false, {}, "an id past the 24: not ours");
    {
        Party p = party({0, 1, 2}, {0, 1, 2}, all, hero);
        p.count = 4;
        expect_fold(p, false, {}, "a count above 3: not ours");
    }
    // The bring-out and its fixed order.
    const auto expect_bring = [&](const Party& p, bool due, int from, int to, const char* name) {
        const Bring b = DecideBring(p, idx);
        if (b.due != due || (due && (b.from_id != from || b.to_id != to)))
            bof3::Fatal("solo field self-test: bring case \"%s\" (due %d, %02X -> %02X)", name, b.due, b.from_id, b.to_id);
        ++checks;
    };
    expect_bring(party({0, 1}, {0, 1}, all, hero), false, 0, 0, "the hero leads: the plain swap");
    {
        Party p = party({1, 0, 5}, {1, 0, 5}, all, hero);
        p.count = 2;
        expect_bring(p, false, 0, 0, "two walk, the list holds a third (the game's): no bring-out");
    }
    expect_bring(party({1, 0}, {0, 1}, all, hero), true, 1, 2, "record 1 leads: record 2 comes out");
    expect_bring(party({2, 0}, {0, 2}, all, hero), true, 2, 3, "record 2 leads: record 3 (any joined) comes out");
    expect_bring(party({6, 0}, {0, 6}, all, hero), true, 6, 1, "record 6 leads: round to record 1");
    expect_bring(party({1, 0}, {0, 1}, hero | 0x2 | 0x20, hero), true, 1, 5, "only 0, 1, 5 joined: 5 next");
    expect_bring(party({5, 0}, {0, 5}, hero | 0x2 | 0x20, hero), true, 5, 1, "only 0, 1, 5 joined: back to 1");
    expect_bring(party({1, 0}, {0, 1}, hero | 0x2, hero), false, 0, 0, "no one in reserve: the plain swap");
    expect_bring(party({1, 0}, {0, 1}, all, hero | 0x4), true, 1, 3, "record 2 marked elsewhere: skipped");
    expect_bring(party({1, 0, 4}, {0, 1, 4}, all, hero | 0x10), true, 1, 2, "three with a scene's mark: the unmarked one");
    expect_bring(party({0, 1, 4}, {0, 1, 4}, all, hero | 0x10), false, 0, 0, "three, the hero leads: the plain swap");
    expect_bring(party({1, 2}, {1, 2}, all, hero), false, 0, 0, "two unmarked (not folded yet): the plain swap");
    expect_bring(party({1}, {1}, all, hero), false, 0, 0, "one walker: no swap at all");
    expect_bring(party({1, 0}, {0, 1}, all | 0x80, hero), true, 1, 2, "the whelp joined too: in order");
    expect_bring(party({6, 0}, {0, 6}, all | 0x80, hero), true, 6, 0xA, "record 7 comes out by its own id");
    {
        // A record whose id maps elsewhere is skipped (its id byte stale).
        Party p = party({1, 0}, {0, 1}, all, hero);
        p.char_id[2] = 5;
        expect_bring(p, true, 1, 3, "record 2's id maps to record 5: skipped");
    }
    // A full cycle reaches every joined character as the walker beside the hero.
    {
        Party p = party({0, 1}, {0, 1}, all | 0x80, hero);
        unsigned seen = 0;
        for (int press = 0; press < 32; ++press) {
            const Bring b = DecideBring(p, idx);
            if (b.due) {
                // The exchange puts the hero on the lead; the apply writes
                // the newcomer over the old leader in both lists.
                p.first[0] = 0;
                p.first[1] = b.to_id;
                for (unsigned k = 0; k < 2; ++k)
                    if (p.second[k] == b.from_id) p.second[k] = b.to_id;
            } else {
                const unsigned char t = p.first[0];
                p.first[0] = p.first[1];
                p.first[1] = t;
            }
            for (unsigned i = 0; i < 2; ++i) seen |= 1u << idx[p.first[i]];
        }
        if (seen != 0xFF) bof3::Fatal("solo field self-test: the swap cycle reached records %02X, not all eight", seen);
        ++checks;
    }
    // FindRow on a table of its own, rows sorted inside and down the columns
    // as the game's are: (0,1,2) (0,3,4) (1,5,6) (2,4,7) (3,4,9) (4,5,7) (6,7,8).
    {
        unsigned char table[kSetRows * 3];
        std::memset(table, 0, sizeof table);
        const unsigned char rows[][3] = {{0, 1, 2}, {0, 3, 4}, {1, 5, 6}, {2, 4, 7}, {3, 4, 9}, {4, 5, 7}, {6, 7, 8}};
        std::memset(table, 0xEE, sizeof table);
        for (unsigned r = 0; r < 7; ++r) std::memcpy(table + r * 3, rows[r], 3);
        struct FindCase {
            unsigned char current, a, b, c, want;
        };
        static const FindCase kFind[] = {
            {0xFF, 0, 1, 2, 0},    {0xFF, 2, 1, 0, 0},       {0xFF, 0, 4, 0xFF, 1},     {0xFF, 4, 0xFF, 0xFF, 5},
            {0xFF, 7, 0xFF, 0xFF, 6}, {0xFF, 4, 7, 0xFF, 5}, {0xFF, 5, 7, 0xFF, 0xFF},     {0xFF, 7, 8, 0xFF, 6},
            {0xFF, 6, 8, 0xFF, 6}, {0xFF, 2, 7, 0xFF, 3},    {0xFF, 0, 5, 0xFF, 0xFF},  {0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
            // (4, 9) sits in (3,4,9), but the narrowing on 4's column-0 rows misses it
            {0xFF, 4, 9, 0xFF, 0xFF}, {0xFF, 3, 9, 0xFF, 4},
            // the current set wins when it holds them all, bit 7 or not
            {0x85, 4, 5, 0xFF, 5}, {0x05, 7, 0xFF, 0xFF, 5}, {0x84, 4, 9, 0xFF, 4},
            {0x80, 7, 0xFF, 0xFF, 6}, {0x93, 0, 0xFF, 0xFF, 0xFF},
        };
        for (const FindCase& c : kFind) {
            const unsigned char got = FindRow(table, c.current, c.a, c.b, c.c);
            if (got != c.want)
                bof3::Fatal("solo field self-test: FindRow(current %02X, %02X %02X %02X) is %02X, not %02X", c.current, c.a,
                            c.b, c.c, got, c.want);
            ++checks;
        }
        // ChooseRow: the champion added where a row holds all; dropped where not.
        bool with = false;
        unsigned char ids[3];
        const unsigned char team_a[] = {4, 2};  // with 7: (2,4,7)
        if (ChooseRow(table, 0x80, team_a, 2, 7, &with, ids) != 3 || !with) bof3::Fatal("solo field self-test: ChooseRow a");
        ++checks;
        const unsigned char team_b[] = {0, 1};
        if (ChooseRow(table, 0x81, team_b, 2, 7, &with, ids) != 0 || with) bof3::Fatal("solo field self-test: ChooseRow b");
        ++checks;
        const unsigned char team_c[] = {0, 4};
        if (ChooseRow(table, 0x81, team_c, 2, 3, &with, ids) != kSetRows || !with)
            bof3::Fatal("solo field self-test: ChooseRow c (the current set holds all: no load)");
        ++checks;
        const unsigned char team_d[] = {0, 5};
        if (ChooseRow(table, 0x80, team_d, 2, -1, &with, ids) != 0xFF) bof3::Fatal("solo field self-test: ChooseRow d");
        ++checks;
        const unsigned char team_e[] = {4, 5};
        if (ChooseRow(table, 0x80, team_e, 2, 5, &with, ids) != 5 || !with)
            bof3::Fatal("solo field self-test: ChooseRow e (the champion on the map)");
        ++checks;
    }
    bof3::Log("solo field self-test: %d checks passed", checks);
}

void SoloField_Inject() {
    if (bof3::IsExeHost() ? bof3::os::GetEnv("BOF3X_SELFTEST_ONLY", nullptr, 0) > 0 : bof3::WantsShadow("solo_field"))
        SoloField_SelfTest();
}

}  // namespace solo

#include "game/solo/solo_party_set.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "bof3/symbols.gen.h"
#include "game/dat_cache.h"
#include "game/dat_load.h"
#include "game/solo/solo.h"
#include "game/solo/solo_battle.h"
#include "game/solo/solo_rows.h"
#include "hook/host.h"
#include "hook/log.h"

// The party-set supplement (solo_party_set.h; the measurements behind every
// constant are docs/solo-character-party-set.md's). What each address is:
//
//   kPartySetRows 0x669750 the 19 party-set rows of 3 ids (solo_rows.h)
//   kSetData     0x813580  MessagePools 0x803580 + 0x10000: where a set file's
//                          one kind-0 chunk (tag 0x10000) lands - three dword
//                          offsets from itself, one per column, then the
//                          three character sections (field_event's kBankTable)
//   kFormation   0x904AAC  the byte whose bit 1 makes GameMode5_Turn's
//                          PartySet_Select load mode 2 (BPLU) rather than 1
//                          (BPLD) (transition.cpp; mode_rest kFormation)
//   kEventFlags  0x904AE5  bit 4: the step skips its select (the battle draws
//                          from the field file)
//
// PartySet_Select 0x536AC0 loads row + 0xFC (mode 1) or row + 0x10F (mode 2)
// from Dat_FileNames 0x64F368: BPLD<ids>.DAT / BPLU<ids>.DAT.

namespace solo {
namespace {

constexpr std::uint32_t kSetTag = 0x10000;
constexpr std::uint32_t kSetData = 0x813580;
constexpr std::uint32_t kFormation = 0x904AAC;
constexpr std::uint32_t kEventFlags = 0x904AE5;
constexpr int kBattleFileMode1 = 0xFC;    // PartySet_Select's mode-1 index base (BPLD)
constexpr int kBattleFileMode2 = 0x10F;   // mode 2's (BPLU)
// The window of arena offsets from kSetTag a set file's data may fill: the
// largest shipped battle data is 0x5A00C bytes, the largest field data 0x639EC
// (measured 2026-11-13 over the 19 rows' PL, BPLD, BPLU files).
constexpr std::uint32_t kWindow = 0x70000;

unsigned char* Image(std::uint32_t address) {
    return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(::bof3::Img(address)));
}

std::uint32_t Dword(const unsigned char* p) {
    std::uint32_t v;
    std::memcpy(&v, p, 4);
    return v;
}

// --- the logic, on bytes alone (the live side and the self-test) ----------

// The column of `ids` to put `need`'s section in: one not holding the
// champion (the battle of one draws only the champion's), `prefer` (the
// column `need` holds in its own row, so the section sits where the game's
// rows put it) when that one is free, else the lowest free one. -1: `ids`
// holds `need` already, or no column is free.
int ChooseColumn(const unsigned char* ids, unsigned char champion, unsigned char need, int prefer) {
    if (RowHolds(ids, 0, need)) return -1;
    if (prefer >= 0 && prefer < 3 && ids[prefer] != champion) return prefer;
    for (int c = 0; c < 3; ++c)
        if (ids[c] != champion) return c;
    return -1;
}

// Column `col`'s section of a set file's data (`len` bytes from kSetTag):
// [off, end). The header is three ascending offsets, the first 0xC (every
// shipped set file, measured). False when it is not that shape.
bool SectionBounds(const unsigned char* data, std::uint32_t len, int col, std::uint32_t& off, std::uint32_t& end) {
    if (len < 0xC || col < 0 || col > 2) return false;
    const std::uint32_t o[3] = {Dword(data), Dword(data + 4), Dword(data + 8)};
    if (o[0] != 0xC || !(o[0] < o[1] && o[1] < o[2] && o[2] < len)) return false;
    off = o[col];
    end = col < 2 ? o[col + 1] : len;
    return true;
}

// The section's bytes up to its last non-zero one (a battle file pads each
// section to its slot with zeros).
std::uint32_t UsedLength(const unsigned char* sec, std::uint32_t n) {
    while (n > 0 && sec[n - 1] == 0) --n;
    return n;
}

// Whether a section of `used` bytes reads only inside itself: its header's
// four dword offsets (the image table +0, the palettes +4 that
// Sprite_LoadPalette reads, the script words +8, the frame entries +0xC that
// Sprite_SetFrameQueueUpload reads) all fall inside it. Offsets are relative
// to the section, so a section is lifted whole.
bool SelfContained(const unsigned char* sec, std::uint32_t used) {
    if (used < 0x10) return false;
    for (unsigned k = 0; k < 4; ++k)
        if (Dword(sec + k * 4) >= used) return false;
    return true;
}

// --- reading a file as LoadDatFile would land it ----------------------------

// One set file's data window: what LoadDatFile(index)'s kind-0 chunks would
// leave in the arena from kSetTag on - the shipped file or the cache's, then
// each overlay, in LoadDatFile's own precedence (the walk is LoadDatFile's,
// with its chunk handler swapped, as DatLoad_TextBlockBytes does).
struct SetData {
    std::vector<unsigned char> bytes;
    std::uint32_t len = 0;   // the highest byte written, from kSetTag
    bool overflow = false;   // a kind-0 chunk ran past the window
    int chunks = 0;          // kind-0 chunks that landed in the window
};

SetData* g_into = nullptr;

void TakeSetData(void*, const dat_cache::Chunk& h, std::uint8_t* payload) {
    if (h.kind != 0 || h.size <= 0) return;
    const std::uint32_t from = h.tag, to = h.tag + static_cast<std::uint32_t>(h.size);
    if (to <= kSetTag || from >= kSetTag + kWindow) return;
    if (from < kSetTag || to > kSetTag + kWindow) {
        g_into->overflow = true;
        return;
    }
    std::memcpy(g_into->bytes.data() + (from - kSetTag), payload, static_cast<std::uint32_t>(h.size));
    if (to - kSetTag > g_into->len) g_into->len = to - kSetTag;
    ++g_into->chunks;
}

void ReadSet(int file_index, SetData& d) {
    d.bytes.assign(kWindow, 0);
    d.len = 0;
    d.overflow = false;
    d.chunks = 0;
    g_into = &d;
    const dat_cache::ChunkFn was = dat_load::g_take;
    dat_load::g_take = TakeSetData;
    LoadDatFile(file_index);
    dat_load::g_take = was;
    g_into = nullptr;
}

const char* FileName(int file_index) {
    const char* name = Dat_FileNames[file_index];
    return name ? name : "(none)";
}

// --- the plan ---------------------------------------------------------------

struct Plan {
    unsigned row = 0;          // the battle's row
    int column = -1;           // its column that takes the section
    int mode = 1;              // 1 BPLD, 2 BPLU
    unsigned src_row = 0;      // the first row holding `need`
    int src_column = -1;
    int dst_file = 0, src_file = 0;
    std::uint32_t dst_off = 0, dst_end = 0;   // the column's slot in the battle file's data
    std::uint32_t src_off = 0, used = 0;      // the section in the source file's data
};

SetData g_dst, g_src;

// The plan for fighting row `row` with `need`'s section beside `champion`,
// the battle files of mode `mode` read into g_dst / g_src. False with `why`.
bool MakePlan(unsigned char champion, unsigned char need, unsigned row, int mode, Plan& p, std::string& why) {
    const unsigned char* const rows = PartySetRows();
    p = Plan{};
    p.row = row;
    p.mode = mode;
    p.src_column = FirstColumnOf(rows, need, &p.src_row);
    if (p.src_column < 0) {
        why = "no party-set row holds the id at all";
        return false;
    }
    p.column = ChooseColumn(rows + row * 3, champion, need, p.src_column);
    if (p.column < 0) {
        why = "the row has no column the battle of one leaves free";
        return false;
    }
    const int base = mode == 2 ? kBattleFileMode2 : kBattleFileMode1;
    p.dst_file = static_cast<int>(row) + base;
    p.src_file = static_cast<int>(p.src_row) + base;
    ReadSet(p.dst_file, g_dst);
    ReadSet(p.src_file, g_src);
    if (g_dst.chunks == 0 || g_src.chunks == 0) {
        why = std::string("a battle file has no set data in this install (") + FileName(g_dst.chunks ? p.src_file : p.dst_file) + ")";
        return false;
    }
    if (g_dst.overflow || g_src.overflow) {
        why = "a battle file's data runs past the arena window this module reads";
        return false;
    }
    std::uint32_t src_end = 0;
    if (!SectionBounds(g_dst.bytes.data(), g_dst.len, p.column, p.dst_off, p.dst_end) ||
        !SectionBounds(g_src.bytes.data(), g_src.len, p.src_column, p.src_off, src_end)) {
        why = "a battle file's data is not three sections behind three offsets";
        return false;
    }
    p.used = UsedLength(g_src.bytes.data() + p.src_off, src_end - p.src_off);
    if (!SelfContained(g_src.bytes.data() + p.src_off, p.used)) {
        why = "the id's section reads outside itself";
        return false;
    }
    if (p.used > p.dst_end - p.dst_off) {
        why = "the id's section is larger than the free column's slot";
        return false;
    }
    return true;
}

// --- the plan held for its battle ---------------------------------------------

// What a plan needs at the dressing: the plan, the id's section (its used
// bytes), and the battle file's three offsets and each column's first 16
// bytes, to test the loaded set data against. Made once per battle (by Arm,
// or by the first dressing when Arm's was made in another process: a
// quick-load), and used only while Game_BattleCount and the plan's inputs are
// the same. What it holds is a function of those inputs and the player's
// files alone, so a quick-load back into the same battle finds the same
// bytes; it lives in this module's memory only and is replaced (its memory
// with it) by the next battle's plan.
struct Prepared {
    bool valid = false;
    unsigned battle = 0;
    unsigned char champion = 0, need = 0;
    Plan plan;
    std::vector<unsigned char> section;   // `plan.used` bytes
    unsigned char header[0xC] = {};
    unsigned char heads[3][0x10] = {};
};
Prepared g_prep;

bool Matches(const Prepared& h, unsigned battle, unsigned char champion, unsigned char need, unsigned row, int mode) {
    return h.valid && h.battle == battle && h.champion == champion && h.need == need && h.plan.row == row &&
           h.plan.mode == mode;
}

// The plan for this battle, made (two file reads) unless held already. Null
// with `why` when it cannot be made.
const Prepared* Prepare(unsigned char champion, unsigned char need, unsigned row, int mode, std::string& why) {
    const unsigned battle = Game_BattleCount;
    if (Matches(g_prep, battle, champion, need, row, mode)) return &g_prep;
    g_prep = Prepared{};
    Plan p;
    const bool ok = MakePlan(champion, need, row, mode, p, why);
    if (ok) {
        g_prep.battle = battle;
        g_prep.champion = champion;
        g_prep.need = need;
        g_prep.plan = p;
        g_prep.section.assign(g_src.bytes.begin() + p.src_off, g_src.bytes.begin() + p.src_off + p.used);
        std::memcpy(g_prep.header, g_dst.bytes.data(), 0xC);
        for (int c = 0; c < 3; ++c) std::memcpy(g_prep.heads[c], g_dst.bytes.data() + Dword(g_dst.bytes.data() + c * 4), 0x10);
        g_prep.valid = true;
    }
    // The two file windows are not kept past the plan.
    std::vector<unsigned char>().swap(g_dst.bytes);
    std::vector<unsigned char>().swap(g_src.bytes);
    return ok ? &g_prep : nullptr;
}

unsigned g_dressed_battle = 0xFFFFFFFFu;   // the battle whose dressing was logged

// The battle's row when the dressing or the join must act: the battle is the
// mode's (SoloBattle_ChampionFightsThis: the champion alone in it, read from
// the game's bytes, so a quick-load answers the same) and the loaded row
// lacks `member`. Else -1: the game's own call does what it always did.
int SupplyRow(bool ours, unsigned char set, const unsigned char* rows, unsigned char member) {
    if (!ours) return -1;
    if (set == kNone || (set & 0x7Fu) >= kPartySetRowCount) return -1;
    const unsigned row = set & 0x7Fu;
    if (RowHolds(rows, row, member)) return -1;
    return static_cast<int>(row);
}

int RowToSupply(unsigned char member) {
    // SoloBattle_ChampionFightsThis asks Solo_ChampionFights() first, which
    // reads nothing with SoloCharacter off.
    const bool ours = SoloBattle_ChampionFightsThis();
    return SupplyRow(ours, ours ? static_cast<unsigned char>(AreaW0b_PartySet) : kNone, PartySetRows(), member);
}

}  // namespace

int PartySetExtra_Arm(unsigned char need, unsigned char champion, int base, unsigned char event_flags) {
    const unsigned battle = Game_BattleCount;
    const unsigned fight = EventBattle_Id;
    auto stand_down = [&](const char* why) {
        bof3::Log("DIV-0228 SoloCharacter: battle %u (event battle 0x%02X): id 0x%02X's battle section cannot be supplied "
                  "beside the champion's id 0x%02X: %s",
                  battle, fight, need, champion, why);
        return kNoRow;
    };
    if (event_flags & 0x10)
        return stand_down("the step skips its select for this event battle, so the battle draws from the field file, "
                          "whose sections this module does not supply");
    unsigned row;
    if (base >= 0) row = static_cast<unsigned>(base);
    else if (base == kKeep && AreaW0b_PartySet != kNone) row = AreaW0b_PartySet & 0x7Fu;
    else return stand_down("no row for the champion");
    if (row >= kPartySetRowCount) return stand_down("the row is past the table");
    const int mode = (*Image(kFormation) & 2) ? 2 : 1;
    std::string why;
    const Prepared* const h = Prepare(champion, need, row, mode, why);
    if (!h) return stand_down(why.c_str());
    const Plan& p = h->plan;
    bof3::Log("DIV-0228 SoloCharacter: battle %u (event battle 0x%02X): no party-set row holds id 0x%02X beside the "
              "champion's id 0x%02X; fought in row %u (%s), id 0x%02X's section from %s column %d to be put in its "
              "column %d at the dressing (0x%X bytes of a 0x%X-byte slot)",
              battle, fight, need, champion, row, FileName(p.dst_file), need, FileName(p.src_file), p.src_column,
              p.column, p.used, p.dst_end - p.dst_off);
    return base;
}

bool PartySetExtra_Dress(unsigned member_arg, unsigned slot) {
    const unsigned char member = static_cast<unsigned char>(member_arg);
    const int at = RowToSupply(member);
    if (at < 0) return false;
    const unsigned row = static_cast<unsigned>(at);
    unsigned char* const ids = PartySetRows() + row * 3;
    const unsigned battle = Game_BattleCount;
    if (*Image(kEventFlags) & 0x10)
        bof3::Fatal("DIV-0228 SoloCharacter: battle %u dresses a sprite as id 0x%02X from row %u, which lacks it, in an "
                    "event battle that skipped its select (0x904AE5 bit 4): no plan was made for that",
                    battle, member, row);
    const auto champion = static_cast<unsigned char>(Solo_ChampionId());
    const int mode = (*Image(kFormation) & 2) ? 2 : 1;
    std::string why;
    const Prepared* const h = Prepare(champion, member, row, mode, why);
    if (!h)
        bof3::Fatal("DIV-0228 SoloCharacter: battle %u dresses a sprite as id 0x%02X from row %u, which lacks it, and the "
                    "section cannot be supplied: %s",
                    battle, member, row, why.c_str());
    const Plan& p = h->plan;
    // The loaded set data must be the row's battle file (what the step's
    // select loaded): its three offsets, and every other column's section header.
    unsigned char* const live = Image(kSetData);
    if (std::memcmp(live, h->header, 0xC) != 0)
        bof3::Fatal("DIV-0228 SoloCharacter: battle %u: the set data at 0x%X is not %s's (its column offsets differ)",
                    battle, kSetData, FileName(p.dst_file));
    for (int c = 0; c < 3; ++c) {
        const std::uint32_t o = Dword(h->header + c * 4);
        if (c != p.column && std::memcmp(live + o, h->heads[c], 0x10) != 0)
            bof3::Fatal("DIV-0228 SoloCharacter: battle %u: column %d of the set data at 0x%X is not %s's", battle, c,
                        kSetData, FileName(p.dst_file));
    }
    std::memcpy(live + p.dst_off, h->section.data(), p.used);
    std::memset(live + p.dst_off + p.used, 0, p.dst_end - p.dst_off - p.used);
    // Field_MemberSprite finds the member's column in the row: for this one
    // call the column holds its id, then the row is the game's again.
    const unsigned char was = ids[p.column];
    ids[p.column] = member;
    Field_MemberSprite(member, slot);
    ids[p.column] = was;
    if (g_dressed_battle != battle) {
        g_dressed_battle = battle;
        bof3::Log("DIV-0228 SoloCharacter: battle %u: the sprite dressed as id 0x%02X in slot %u from %s column %d, put in "
                  "row %u's column %d (%s, 0x%X bytes)",
                  battle, member, slot, FileName(p.src_file), p.src_column, row, p.column, FileName(p.dst_file), p.used);
    }
    return true;
}

void PartySetExtra_Around(unsigned member_arg, void (*call)(unsigned), unsigned arg) {
    const unsigned char member = static_cast<unsigned char>(member_arg);
    const int at = RowToSupply(member);
    if (at < 0) {
        call(arg);
        return;
    }
    const unsigned row = static_cast<unsigned>(at);
    unsigned char* const ids = PartySetRows() + row * 3;
    const int prefer = FirstColumnOf(PartySetRows(), member);
    const auto champion = static_cast<unsigned char>(Solo_ChampionId());
    const int column = ChooseColumn(ids, champion, member, prefer);
    if (column < 0)
        bof3::Fatal("DIV-0228 SoloCharacter: battle %u joins id 0x%02X from row %u, which lacks it and has no column "
                    "free of the champion's id 0x%02X",
                    static_cast<unsigned>(Game_BattleCount), member, row, champion);
    bof3::Log("DIV-0228 SoloCharacter: battle %u: id 0x%02X joins from row %u, which lacks it: its record is set up from "
              "column %d for the join (hidden, and rebuilt from the field's row at step 7)",
              static_cast<unsigned>(Game_BattleCount), member, row, column);
    const unsigned char was = ids[column];
    ids[column] = member;
    call(arg);
    ids[column] = was;
}

int Solo_PartySetSelfTest() {
    int checks = 0;
    auto check = [&](bool ok, const char* what) {
        if (!ok) bof3::Fatal("solo party-set self-test: %s", what);
        ++checks;
    };
    // The column: fight 16's case (row 5 (0, 3, 4), champion 4, id 1, which
    // sits in column 1 of rows 0..2) takes column 1.
    const unsigned char r5[3] = {0, 3, 4}, r0[3] = {0, 1, 2}, r7[3] = {2, 4, 7};
    check(ChooseColumn(r5, 4, 1, 1) == 1, "row 5, champion 4, id 1 -> column 1");
    check(ChooseColumn(r5, 3, 1, 1) == 0, "the preferred column is the champion's -> the lowest free");
    check(ChooseColumn(r0, 0, 1, 1) == -1, "a row holding the id needs no supplement");
    check(ChooseColumn(r7, 4, 1, 1) == 0, "row 7, champion 4 in column 1 -> column 0");
    check(ChooseColumn(r7, 4, 1, -1) == 0, "no preference -> the lowest free");
    // The bounds and the fit, on bytes of the shape the files have.
    std::vector<unsigned char> d(0x100, 0);
    auto put = [&](std::uint32_t at, std::uint32_t v) { std::memcpy(d.data() + at, &v, 4); };
    put(0, 0xC);
    put(4, 0x40);
    put(8, 0x80);
    std::uint32_t off = 0, end = 0;
    check(SectionBounds(d.data(), 0x100, 0, off, end) && off == 0xC && end == 0x40, "column 0's bounds");
    check(SectionBounds(d.data(), 0x100, 2, off, end) && off == 0x80 && end == 0x100, "the last column runs to the end");
    check(!SectionBounds(d.data(), 0x80, 2, off, end), "an offset at the end is refused");
    put(4, 0x90);
    check(!SectionBounds(d.data(), 0x100, 1, off, end), "offsets out of order are refused");
    put(4, 0x40);
    // A section at 0x40: header 0x10, 0x18, 0x20, 0x28, bytes to 0x40 + 0x2F.
    put(0x40, 0x10);
    put(0x44, 0x18);
    put(0x48, 0x20);
    put(0x4C, 0x28);
    d[0x40 + 0x2E] = 7;
    check(UsedLength(d.data() + 0x40, 0x40) == 0x2F, "the used length stops at the last non-zero byte");
    check(SelfContained(d.data() + 0x40, 0x2F), "a section whose offsets fall inside it");
    put(0x4C, 0x30);
    check(!SelfContained(d.data() + 0x40, 0x2F), "an offset past the used bytes");
    // The gate (note 1 of the round's review): only a battle of the mode's,
    // and only a row that lacks the id.
    {
        unsigned char rows[kPartySetRowCount * 3];
        std::memset(rows, kNone, sizeof rows);
        const unsigned char r0g[3] = {0, 1, 2}, r5g[3] = {0, 3, 4};
        std::memcpy(rows + 0, r0g, 3);
        std::memcpy(rows + 5 * 3, r5g, 3);
        check(SupplyRow(false, 0x05, rows, 1) == -1, "a battle the mode left alone: the game's call");
        check(SupplyRow(true, kNone, rows, 1) == -1, "no set: the game's call");
        check(SupplyRow(true, 0x80, rows, 1) == -1, "row 0 holds id 1: the game's call");
        check(SupplyRow(true, 0x05, rows, 1) == 5 && SupplyRow(true, 0x85, rows, 1) == 5, "row 5 lacks id 1: supplied");
        check(SupplyRow(true, 0x13, rows, 1) == -1, "a set past the table: the game's call");
        check(FirstColumnOf(rows, 4) == 2 && FirstColumnOf(rows, 9) == -1, "FirstColumnOf");
        unsigned r = 99;
        check(FirstColumnOf(rows, 3, &r) == 1 && r == 5, "FirstColumnOf's row");
    }
    // The held plan answers only its own battle and inputs.
    {
        Prepared h;
        h.valid = true;
        h.battle = 88;
        h.champion = 4;
        h.need = 1;
        h.plan.row = 5;
        h.plan.mode = 1;
        check(Matches(h, 88, 4, 1, 5, 1), "the held plan, same battle");
        check(!Matches(h, 89, 4, 1, 5, 1) && !Matches(h, 88, 5, 1, 5, 1) && !Matches(h, 88, 4, 1, 7, 1) &&
                  !Matches(h, 88, 4, 1, 5, 2),
              "another battle, champion, row or mode: made again");
        h.valid = false;
        check(!Matches(h, 88, 4, 1, 5, 1), "an empty hold");
    }
    return checks;
}

}  // namespace solo

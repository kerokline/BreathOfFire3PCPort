// The engine callees the chapter and area groups call by raw address and
// nobody owned - round ten, group SX, taken with the scenario harness
// (scenario_harness.h). docs/scena_sx.md has each function, every caller,
// and why one of the nineteen addresses the round listed is not here.
//
//   Char_LevelUp            0x498DE0  a record's level from its EXP, the gains of each level
//   Party_PlaceForBattle    0x532ED0  the members placed at an event battle's formation
//   Party_ReloadPalettes    0x533E00  each member's palette reloaded
//   Party_HealJoined        0x533E50  every joined record healed, the members' copies refreshed
//   Party_Remove            0x534030  a member out of both lists and the field objects
//   Sprite_FlashClut        0x534DB0  Sprite_Current's CLUT row one colour, a sound
//   Char_LoseHp             0x537480  a member's HP down, never below 1
//   Field_SetStatus80       0x56D6F0  Field_StatusBits |= 0x80
//   Field_CellTriggerAt     0x56D800  a chapter's 5-byte cell records searched
//   MapView_FillCells       0x56FCA0  the view's 56 x 28 cell items refilled
//   Camera_TurnToDegrees    0x57C550  Camera_Angles[0] turned toward an angle in degrees
//   Camera_EaseAngleFB      0x57C6B0  Cond_AngleFB eased toward an angle over frames
//   Sprite_FindFree         0x57CD90  the first free Sprite_Objects record
//   AbilityList_Add         0x590C90  an ability into a member's list or the shared one
//   KeyItem_Add             0x591900  a key item into the 32-byte list
//   Inventory_Remove        0x591B60  items out of a category's lists
//   Zenny_Sub               0x591BC0  the party's zenny down, if it has enough
//   Zenny_Add               0x591BE0  the party's zenny up, capped
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies. No
// divergence: each is a faithful replacement. The unchecked indexes the
// originals make (a member id past Character_IndexById's 24, a category
// past the inventory's five, a member count above three) are reproduced;
// where the original would fault or read its own stack frame (a CLUT colour
// past its four, a divide by 0), ours aborts with a message (docs/scena_sx.md
// section 6).
#include "game/scena_sx.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/map_rim.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scena_sx_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = scena_sx::at;
// move_script::At is named in full here (step B0, analysis/tu_renames.tsv)
using move_script::Long;
using move_script::SetLong;

// The record views (game/record_views.h): rv::As<records::SpriteObject>(p)
// views a
// sprite-family pointer; it is used below +0x80 only (the sprite head every
// family shares) unless the pointer is a Sprite_Objects / Sprite_ObjectsExtra /
// Sprite_Kind2 record. rv::As<records::PartyMember>(p) is given party members
// (ObjTrio, Field_State).
using move_script::SetWord;
using move_script::Word;

// CharacterRecords' record n (stride 0xA4), unchecked.
unsigned char* RecordScenaSx(unsigned n) { return move_script::At(::bof3::Img(CharacterRecords_addr) + n * scena_sx::at::kRecordStride); }
// A member id's record: CharacterRecords[Character_IndexById[id]], both
// indexes a whole byte, unchecked (the original reads them the same way).
unsigned char* MemberRecord(unsigned char id) { return RecordScenaSx(Character_IndexById[id]); }

// A signed byte's value.
int S8(unsigned char b) { return static_cast<signed char>(b); }

// C's truncating division by 360 of a value times 4096 - the imul
// 0xB60B60B7 / add / sar 8 / add sign idiom, exact for every s32.
std::int32_t Degrees(std::int32_t v) { return (v << 12) / 360; }

// One stat word of the level-up: + max(0, gain + bias), the 16-bit word
// capped at 999 (the add and the compare are 16-bit, unsigned).
void Grow(unsigned char* word, int gain, unsigned char bias) {
    int add = gain + S8(bias);
    if (add < 0) add = 0;
    unsigned v = static_cast<std::uint16_t>(Word(word) + static_cast<unsigned>(add));
    if (v > 999) v = 999;
    SetWord(word, v);
}

}  // namespace

// original 0x498DE0: a record's level-up (PSX Char_LevelUp 0x801AEDD4, the
// sibling's name; pairs_propagated "callers"). The record is the argument's
// low byte, unchecked; its EXP (+0xC, compared signed) against the sums of
// Char_ExpTable's words for levels 1.. (the member's 99 rows) gives the new
// level, 99 at most. At 99 already, or no higher, nothing. Else for each
// level gained, from the old one, the row's bytes +2..+5 raise the six stat
// words +0x40..+0x4A (bytes +2, +3 whole, +4, +5 by nibble, high first),
// each plus the record's signed bias byte +0x89..+0x8E, never below 0, the
// word capped at 999; then AbilityList_Add(row +6, the argument, 0, 0) and
// (row +7, ...). Then the level +0xA and Char_RecalcStats.
extern "C" void __cdecl Char_LevelUp(unsigned index_arg) {
    const unsigned id = index_arg & 0xFF;
    unsigned char* const rec = RecordScenaSx(id);
    const unsigned rows = id * 99;
    const auto* const exp_table = move_script::At(::bof3::Img(bof3::addr::Char_ExpTable));
    const std::int32_t exp = rv::As<records::CharRecord>(rec)->exp;
    unsigned char level = 1;
    std::int32_t sum = 0;
    for (;;) {
        sum += Word(exp_table + (level + rows) * 8);
        if (sum > exp) break;
        ++level;
        if (level >= 99) break;
    }
    const unsigned char old = rec[0xA];
    if (old == 99) return;
    if (static_cast<int>(level) - static_cast<int>(old) <= 0) return;
    if (old < level) {
        const unsigned char* row = exp_table + (old + rows) * 8;
        for (unsigned n = level - old; n != 0; --n, row += 8) {
            Grow(rec + 0x40, row[2], rec[0x89]);
            Grow(rec + 0x42, row[3], rec[0x8A]);
            Grow(rec + 0x44, row[4] >> 4, rec[0x8B]);
            Grow(rec + 0x46, row[4] & 0xF, rec[0x8C]);
            Grow(rec + 0x48, row[5] >> 4, rec[0x8D]);
            Grow(rec + 0x4A, row[5] & 0xF, rec[0x8E]);
            SH_CALL(AbilityList_Add)(row[6], index_arg, 0, 0);
            SH_CALL(AbilityList_Add)(row[7], index_arg, 0, 0);
        }
    }
    rec[0xA] = level;
    SH_CALL(Char_RecalcStats)(rec);
}

// original 0x56D6F0: Field_StatusBits |= 0x80. No PSX twin paired.
extern "C" void __cdecl Field_SetStatus80(void) { Field_StatusBits = static_cast<unsigned char>(Field_StatusBits | 0x80); }

// original 0x56D800: which of `count` (a byte) 5-byte cell records at `table`
// the cell (x, z) (bytes) stands in, facing right: +0 the area (a byte, to
// Game_AreaNumber), +1 x, +2 z, +3 bit 7 a run along z (else along x) and the
// facing in its low nibble (8: any; else ObjTrio's first record +8 must
// match), +4 the run's length (0: none). The first record matching answers
// its index; one whose cell matches but whose facing does not is passed over
// (the next record, not the next cell); none, 0xFF. The run's cells are
// tested from its far end; the sums are not wrapped at a byte. The chapters'
// cell hooks call it with their Scena<NN>_CellRecords. No PSX twin paired.
extern "C" std::uint32_t __cdecl Field_CellTriggerAt(unsigned char* table, unsigned count_arg, unsigned x_arg,
                                                      unsigned z_arg) {
    const unsigned count = count_arg & 0xFF;
    const unsigned x = x_arg & 0xFF, z = z_arg & 0xFF;
    for (unsigned i = 0; i < count; ++i) {
        const unsigned char* const r = table + 5 * i;
        bool hit = false;
        for (int k = static_cast<int>(r[4]) - 1; k >= 0; --k) {
            if (r[0] != Game_AreaNumber) break;
            if (r[3] & 0x80) {
                if (x == r[1] && z == static_cast<unsigned>(r[2] + k)) hit = true;
            } else {
                if (x == static_cast<unsigned>(r[1] + k) && z == r[2]) hit = true;
            }
            if (hit) break;
        }
        if (!hit) continue;
        const unsigned facing = r[3] & 0xF;
        if (facing == 8 || facing == ObjTrio[0].direction) return i;
    }
    return 0xFF;
}

// original 0x56FCA0: the view's cell items refilled (PSX 0x80155154,
// pairs_propagated "call"). From MapView_Row + 1 and, each of the 56 rows,
// MapView_Column + 1 (read again each row), both wrapping (row 0x37 -> 0,
// column 0x1B -> 0; a value outside the table is not wrapped and runs past
// it): MapView_CellToMap(row index, column index, &MapView_CellItems[row *
// 28 + column]) for each of 28 columns; then MapView_PlaceRuns.
extern "C" void __cdecl MapView_FillCells(void) {
    std::int32_t row = MapView_Row;
    for (int i = 0; i < 0x38; ++i) {
        row = row == 0x37 ? 0 : row + 1;
        std::int32_t col = MapView_Column;
        for (int j = 0; j < 0x1C; ++j) {
            col = col == 0x1B ? 0 : col + 1;
            const std::uint32_t cell = Low32(MapView_CellItems) + static_cast<std::uint32_t>(row * 28 + col) * 4;
            SH_CALL(MapView_CellToMap)(i, j, move_script::At(cell));
        }
    }
    // DIV-0041 (wide-ring): the rim refilled in place, its items kept, as Capcom's.
    if (map_rim::Active()) map_rim::RimFill(true);
    SH_CALL(MapView_PlaceRuns)();
}

// original 0x57C550: 0x57C5A0(angle, step), the angle (s16 degrees) and the
// step (s8 degrees) each as a 4096-step circle (x 4096 / 360, truncated; the
// angle & 0xFFF): Camera_Angles[0] turned by the step toward the angle,
// al 1 while it turns (0x57C5A0's answer, whole). No PSX twin paired.
extern "C" std::uint32_t __cdecl Camera_TurnToDegrees(unsigned angle_arg, unsigned step_arg) {
    const std::int32_t step = Degrees(S8(static_cast<unsigned char>(step_arg)));
    const std::int32_t angle = Degrees(static_cast<short>(angle_arg)) & 0xFFF;
    return SH_AT(std::uint32_t (__cdecl*)(std::int32_t, std::int32_t), at::kCameraTurnYaw.v)(angle, step);
}

// original 0x57C6B0: Cond_AngleFB eased toward an angle (s16 degrees, as a
// 4096-step circle) over frames. While frames are left (0x905B9C): the 16.16
// accumulator 0x905B98 plus the step 0x905B78, its high word to Cond_AngleFB,
// one frame less, al 1 while any are left. Else, with Sprite_Kind2 +0x84's
// Field_MoveSpeeds entry 0: Cond_AngleFB the angle at once, al 0. Else the
// frames 0x80 / (the speed x 4, or x 8 unless Sprite_Kind2 +8 is 2 or 6, as
// a byte), times the second argument (s8); the accumulator Cond_AngleFB <<
// 16; the step ((the angle - (Cond_AngleFB & 0x7FF)) << 16) / the frames; al
// 1 unless the frames are 0. Where the original divides by 0 (a byte product
// of 0, a second argument of 0) or overflows (-2^31 / -1) it faults; ours
// aborts. No PSX twin paired.
extern "C" unsigned char __cdecl Camera_EaseAngleFB(unsigned angle_arg, unsigned frames_arg) {
    std::int32_t left = static_cast<std::int32_t>(ScenaSx_EaseLeft);
    const unsigned char speed = Field_MoveSpeeds[Sprite_Kind2.speed];
    if (left != 0) {
        const std::int32_t acc = static_cast<std::int32_t>(static_cast<std::uint32_t>(CameraTurn_Angles[2]) +
                                                           static_cast<std::uint32_t>(CameraTurn_Steps[2]));
        CameraTurn_Angles[2] = acc;
        SetWord(move_script::At(::bof3::Img(Cond_AngleFB_addr)), static_cast<std::uint32_t>(acc >> 16));
        --left;
        ScenaSx_EaseLeft = left;
        return left != 0 ? 1 : 0;
    }
    if (speed == 0) {
        SetWord(move_script::At(::bof3::Img(Cond_AngleFB_addr)), static_cast<std::uint32_t>(Degrees(static_cast<short>(angle_arg))));
        return 0;
    }
    const unsigned char mode = Sprite_Kind2.direction;
    const int per = mode == 2 || mode == 6 ? 4 : 8;
    const auto product = static_cast<unsigned char>(per * S8(speed));
    if (product == 0) bof3::Fatal("Camera_EaseAngleFB: 0x80 / 0 (speed byte 0x%X): the original faults", speed);
    const std::int32_t frames = 0x80 / product;
    const auto cur = static_cast<short>(Word(move_script::At(::bof3::Img(Cond_AngleFB_addr))));
    const std::int32_t total = static_cast<std::int32_t>(static_cast<std::uint32_t>(frames) *
                                                         static_cast<std::uint32_t>(S8(static_cast<unsigned char>(frames_arg))));
    CameraTurn_Angles[2] = static_cast<std::int32_t>(static_cast<std::uint32_t>(cur) << 16);
    const std::int32_t target = Degrees(static_cast<short>(angle_arg));
    ScenaSx_EaseLeft = total;
    const auto num = static_cast<std::int32_t>(static_cast<std::uint32_t>(target - (cur & 0x7FF)) << 16);
    if (total == 0) bof3::Fatal("Camera_EaseAngleFB: a step over 0 frames (second argument 0): the original faults");
    if (num == INT32_MIN && total == -1) bof3::Fatal("Camera_EaseAngleFB: -2^31 / -1: the original faults");
    CameraTurn_Steps[2] = num / total;
    return 1;
}

// original 0x57CD90 (PSX 0x8015D438): the first Sprite_Objects record (of 30) whose +0 is 0,
// al its index; none, al 0xFF (the rest of eax is the caller's). PSX twin
// 0x8015D438 (pairs_propagated "call-anchored").
extern "C" unsigned char __cdecl Sprite_FindFree(void) {
    for (unsigned i = 0; i < at::kSpriteCount; ++i)
        if (Sprite_Objects[i].flags == 0) return static_cast<unsigned char>(i);
    return 0xFF;
}

// original 0x590C90: an ability id into a list (PSX AbilityList_Add
// 0x80165BCC, the sibling's name; pairs_propagated "gap44"). An id of 0
// (low byte): al 0. With the third argument's byte 0: the member's list,
// 0x591EC0(member, id, fourth) - one of four 10-byte lists of its record by
// the id's class - 10 slots; else the shared list 0x904574, 128 slots. The
// first 0 slot takes the id, al 1; none, al 0.
extern "C" unsigned char __cdecl AbilityList_Add(unsigned id_arg, unsigned member, unsigned shared, unsigned which) {
    const auto id = static_cast<unsigned char>(id_arg);
    if (id == 0) return 0;
    unsigned char* list;
    unsigned slots;
    if (static_cast<unsigned char>(shared) == 0) {
        list = SH_AT(unsigned char* (__cdecl*)(unsigned, unsigned, unsigned), at::kAbilityListOf.v)(member, id_arg, which);
        slots = 10;
    } else {
        list = move_script::At(::bof3::Img(at::kAbilityShared));
        slots = 0x80;
    }
    for (unsigned i = 0; i < slots; ++i) {
        if (list[i] != 0) continue;
        list[i] = id;
        return 1;
    }
    return 0;
}

// original 0x591900 (PSX 0x80166B10): a key item (the argument's low byte) into the first 0
// byte of the 32-byte key-item list 0x904554, al 1; full, al 0. PSX twin
// 0x80166B10 (pairs_propagated "call-anchored").
extern "C" unsigned char __cdecl KeyItem_Add(unsigned item) {
    unsigned char* const list = move_script::At(::bof3::Img(at::kKeyItems));
    for (unsigned i = 0; i < 0x20; ++i) {
        if (list[i] != 0) continue;
        list[i] = static_cast<unsigned char>(item);
        return 1;
    }
    return 0;
}

// original 0x591B60: `count` of `item` out of a category's lists (PSX
// Inventory_Remove 0x80166F30, the sibling's name; pairs_propagated
// "call-disputed"). Item or count 0 (low bytes): al 0. The category's id and
// count lists through Inventory_IdLists / Inventory_CountLists (its low
// byte, unbounded, as Inventory_Add reads them); the item's first slot of
// 128, none: al 0. Category 4 (key items): the id cleared, al 1. Else fewer
// than `count`: al 0; else the count less, the id cleared at 0, al 1.
extern "C" unsigned char __cdecl Inventory_Remove(unsigned category, unsigned item_arg, unsigned count_arg) {
    const auto item = static_cast<unsigned char>(item_arg);
    if (item == 0) return 0;
    if (static_cast<unsigned char>(count_arg) == 0) return 0;
    const unsigned cat = category & 0xFF;
    unsigned char* ids = move_script::At(static_cast<std::uint32_t>(Long(move_script::At(::bof3::Img(bof3::addr::Inventory_IdLists) + 4 * cat))));
    // the count list's pointer, stepped with the id's though category 4's is 0 and never read
    const auto counts = static_cast<std::uint32_t>(Long(move_script::At(::bof3::Img(bof3::addr::Inventory_CountLists) + 4 * cat)));
    for (unsigned i = 0; i < 0x80; ++i, ++ids) {
        if (*ids != item) continue;
        if (static_cast<unsigned char>(category) != 4) {
            unsigned char* const have = move_script::At(counts + i);
            const auto n = static_cast<unsigned char>(count_arg);
            if (*have < n) return 0;
            *have = static_cast<unsigned char>(*have - n);
            if (*have != 0) return 1;
        }
        *ids = 0;
        return 1;
    }
    return 0;
}

// original 0x591BC0 (PSX 0x80166FCC): the party's zenny (Party_Zenny, u32) down by the
// argument when it has at least that much, al 1; else al 0 (the rest of eax
// the argument's). PSX Zenny_Sub 0x80166FCC (the sibling's name;
// pairs_propagated "gap15").
extern "C" unsigned char __cdecl Zenny_Sub(std::uint32_t amount) {
    const auto money = static_cast<std::uint32_t>(Party_Zenny);
    if (money < amount) return 0;
    Party_Zenny = money - amount;
    return 1;
}

// original 0x591BE0 (PSX 0x80166FFC): the party's zenny up by the argument (u32, wrapping),
// and 0x904138 too when the second argument's byte is 0; then above
// 9,999,999 it is 9,999,999 and al 0, else al 1. PSX Zenny_Add 0x80166FFC
// (the sibling's name; pairs_propagated "call-anchored").
extern "C" unsigned char __cdecl Zenny_Add(std::uint32_t amount, unsigned tally) {
    unsigned char* const money = move_script::At(::bof3::Img(Party_Zenny_addr));
    SetLong(money, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(money)) + amount));
    if (static_cast<unsigned char>(tally) == 0)
        Rest4a_CountC = static_cast<std::int32_t>(Rest4a_CountC + amount);
    if (static_cast<std::uint32_t>(Long(money)) > at::kZennyCap) {
        SetLong(money, static_cast<std::int32_t>(at::kZennyCap));
        return 0;
    }
    return 1;
}

void ScenaSx_Inject() {
    if (bof3::WantsShadow("scena_sx")) scena_sx::SelfTest();
    BOF3_INJECT(Char_LevelUp);
    BOF3_INJECT(Field_SetStatus80);
    BOF3_INJECT(Field_CellTriggerAt);
    BOF3_INJECT(MapView_FillCells);
    BOF3_INJECT(Camera_TurnToDegrees);
    BOF3_INJECT(Camera_EaseAngleFB);
    BOF3_INJECT(Sprite_FindFree);
    BOF3_INJECT(AbilityList_Add);
    BOF3_INJECT(KeyItem_Add);
    BOF3_INJECT(Inventory_Remove);
    BOF3_INJECT(Zenny_Sub);
    BOF3_INJECT(Zenny_Add);
}

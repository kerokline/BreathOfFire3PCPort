// Moved here by tools/move_funcs.py from scena_sx.cpp (docs/source-layout-round.md).
#include "game/field/char.h"

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

}  // namespace

namespace {

// The record views (game/record_views.h): rv::As<records::SpriteObject>(p)
// views a
// sprite-family pointer; it is used below +0x80 only (the sprite head every
// family shares) unless the pointer is a Sprite_Objects / Sprite_ObjectsExtra /
// Sprite_Kind2 record. rv::As<records::PartyMember>(p) is given party members
// (ObjTrio, Field_State).
using move_script::SetWord;

using move_script::Word;

// CharacterRecords' record n (stride 0xA4), unchecked.
unsigned char* RecordScenaSx(unsigned n) { return move_script::At(::bof3::Img(CharacterRecords_addr) + n * at::kRecordStride); }

// A signed byte's value.
int S8(unsigned char b) { return static_cast<signed char>(b); }

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

void FieldChar_Inject() {
    BOF3_INJECT(Char_LevelUp);
}

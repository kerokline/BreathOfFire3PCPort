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
unsigned char* RecordScenaSx(unsigned n) { return move_script::At(::bof3::Img(CharacterRecords_addr) + n * at::kRecordStride); }
// A member id's record: CharacterRecords[Character_IndexById[id]], both
// indexes a whole byte, unchecked (the original reads them the same way).
unsigned char* MemberRecord(unsigned char id) { return RecordScenaSx(Character_IndexById[id]); }
unsigned char* ObjScenaSx(unsigned n) { return rv::Bytes(&ObjTrio[n]); }

// A signed byte's value.
int S8(unsigned char b) { return static_cast<signed char>(b); }

// Sprite_EnsureAnimation with the whole dword the original pushes (the
// callee hands it on to Sprite_SetAnimation), as scena_sc0.cpp's.
void EnsureAnimation(std::uint32_t eax) {
    const auto callee = reinterpret_cast<std::uintptr_t>(SH_CALL(Sprite_EnsureAnimation));
    reinterpret_cast<unsigned char (__cdecl*)(std::uint32_t)>(callee)(eax);
}

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

// original 0x532ED0: the members placed for an event battle. The x and z
// (16.16) to 0x903780 / 0x903784, the formation byte 0x904AAC from
// EventBattle_Records[n] +1 (n the third argument's low byte, unchecked).
// Then for each member i below Field_MemberCount (read again each time):
// Field_State and Sprite_Current its ObjTrio record, its slot j the index in
// the second list of the record's +0x89 (3 when absent); 0x532FD0(x, z, j) -
// the slot passed in the argument's dword, whose upper bytes stay the
// caller's; Sprite_Current's +0x34 / +0x38 to the slot's pair at 0x7E06E0,
// +0x48 0, +8 from BattleFormation_Anims[formation] and
// Sprite_EnsureAnimation of it. No PSX twin paired.
extern "C" void __cdecl Party_PlaceForBattle(std::int32_t x, std::int32_t z, unsigned formation_arg) {
    BossSi_CentreX = x;
    const unsigned char formation = move_script::At(::bof3::Img(bof3::addr::EventBattle_Records))[(formation_arg & 0xFF) * 4 + 1];
    BossSi_CentreZ = z;
    move_script::At(::bof3::Img(at::kFormation))[0] = formation;
    if (Field_MemberCount == 0) return;
    std::uint32_t slot_arg = formation_arg;
    for (unsigned i = 0; static_cast<int>(i) < Field_MemberCount; ++i) {
        unsigned char* const o = ObjScenaSx(i);
        Field_State = o;
        Sprite_Current = o;
        const unsigned char id = o[0x89];
        unsigned char j = 0;
        for (;;) {
            slot_arg = (slot_arg & 0xFFFFFF00u) | j;
            if (Cond_Flags[0xd5 + j] == id) break;
            ++j;
            slot_arg = (slot_arg & 0xFFFFFF00u) | j;
            if (j >= 3) break;
        }
        SH_AT(void (__cdecl*)(std::int32_t, std::int32_t, std::uint32_t), at::kFormationPlace.v)(
            static_cast<std::int32_t>(BossSi_CentreX), static_cast<std::int32_t>(BossSi_CentreZ), slot_arg);
        unsigned char* const s = Sprite_Current;
        unsigned char* const pos = move_script::At(::bof3::Img(at::kSlotPositions) + (slot_arg & 0xFF) * 8);
        move_script::SetLong(pos, rv::As<records::SpriteObject>(s)->x);
        move_script::SetLong(pos + 4, rv::As<records::SpriteObject>(s)->z);
        s[0x48] = 0;
        s[8] = move_script::At(::bof3::Img(bof3::addr::BattleFormation_Anims))[move_script::At(::bof3::Img(at::kFormation))[0]];
        const auto p = Low32(s);
        EnsureAnimation((p & 0xFFFFFF00u) | s[8]);
    }
}

// original 0x533E00: for each member i below Field_MemberCount (read again
// each time): Sprite_Current its ObjTrio record, Sprite_ReleaseTint(the
// record), Sprite_LoadPalette(0x80D380 + 0x40 i, 0). Called by 63 of the
// chapter call tables' entries (most as a tail jmp) and group SE's
// Scena08_PartyJoin784. No PSX twin paired.
extern "C" void __cdecl Party_ReloadPalettes(void) {
    if (Field_MemberCount == 0) return;
    for (unsigned i = 0; static_cast<int>(i) < Field_MemberCount; ++i) {
        unsigned char* const o = ObjScenaSx(i);
        Sprite_Current = o;
        SH_CALL(Sprite_ReleaseTint)(o);
        SH_CALL(Sprite_LoadPalette)(reinterpret_cast<unsigned short*>(move_script::At(::bof3::Img(Gfx_ClutStripSource_addr) + 0x1e00 + 0x40 * i)), 0);
    }
}

// original 0x533E50: each of the eight CharacterRecords with +0xB bit 0 (the
// joined bit): +0x1E 0, Char_RecalcStats(record), then +0x1C = +0x2E, HP +0x18
// = +0x20, AP +0x1A = +0x22, the state word +0x10 0 - read after the call.
// Then, for each member i below Field_MemberCount (read once, after the
// eight), the member's record (Character_IndexById of the first list's id,
// unchecked) copied whole (0xA4 bytes) over ObjTrio record i +0x80. No PSX
// twin paired; Party_RestoreAll (0x580630, the inn's) is its larger cousin.
extern "C" void __cdecl Party_HealJoined(void) {
    for (unsigned r = 0; r < 8; ++r) {
        unsigned char* const rec = RecordScenaSx(r);
        if (!(rec[0xB] & 1)) continue;
        rec[0x1E] = 0;
        SH_CALL(Char_RecalcStats)(rec);
        rec[0x1C] = rec[0x2E];
        rv::Store<unsigned>(rv::As<records::CharRecord>(rec)->hp, rv::As<records::CharRecord>(rec)->stats.max_hp);
        rv::Store<unsigned>(rv::As<records::CharRecord>(rec)->ap, rv::As<records::CharRecord>(rec)->stats.max_ap);
        rv::Store<unsigned>(rv::As<records::CharRecord>(rec)->status, 0);
    }
    const int n = Field_MemberCount;
    for (int i = 0; i < n; ++i)
        std::memmove(ObjScenaSx(static_cast<unsigned>(i)) + 0x80, MemberRecord(Cond_Flags[0xd2 + i]), at::kRecordStride);
}

// original 0x534030: a member out of the party (the argument's low byte, an
// id). Party_JoinReset; the id's slot in the second list (the count when it
// is absent, unchecked) becomes 0xFF and moves to the end below the count;
// the id's slot in the first list becomes 0xFF, and each 0xFF below the count
// - 1 (read again each time) takes the next slot's id, the next ObjTrio record
// copied over it (0x14C bytes; Sprite_Current it) and renumbered: +5 its slot,
// +0x26 the slot x 0x50 (a byte), +0x27 0x78 + the slot, +0x4B 0xFF, +0x148
// Character_IndexById[+0x89]. Then ObjTrio record count - 1 +0 cleared,
// Member_ClearState(count - 1), and with the count read again: three -
// 0x904060 cleared when 3 or more, two - cleared; the count one less. No PSX
// twin paired.
extern "C" void __cdecl Party_Remove(unsigned id_arg) {
    SH_CALL(Party_JoinReset)();
    const auto id = static_cast<unsigned char>(id_arg);
    unsigned char* const first = move_script::At(::bof3::Img(Cond_Flags_addr) + 0xd2);
    unsigned char* const second = move_script::At(::bof3::Img(Cond_Flags_addr) + 0xd5);
    const int count = Field_MemberCount;
    int k = 0;
    while (k < count && second[k] != id) ++k;
    const int last = count - 1;
    second[k] = 0xFF;
    for (; k < last; ++k) {
        if (second[k] != 0xFF) continue;
        second[k] = second[k + 1];
        second[k + 1] = 0xFF;
    }
    k = 0;
    while (k < count && first[k] != id) ++k;
    first[k] = 0xFF;
    if (last > 0) {
        for (int s = 0; s < Field_MemberCount - 1; ++s) {
            if (first[s] != 0xFF) continue;
            first[s] = first[s + 1];
            unsigned char* const o = ObjScenaSx(static_cast<unsigned>(s));
            Sprite_Current = o;
            first[s + 1] = 0xFF;
            std::memmove(o, o + at::kObjStride, at::kObjStride);
            o[5] = static_cast<unsigned char>(s);
            Sprite_Current[0x26] = static_cast<unsigned char>(s * 0x50);
            Sprite_Current[0x27] = static_cast<unsigned char>(s + 0x78);
            Sprite_Current[0x4B] = 0xFF;
            o[0x148] = Character_IndexById[o[0x89]];
        }
    }
    const unsigned char n = Field_MemberCount;
    const auto below = static_cast<unsigned char>(n - 1);
    ObjTrio[static_cast<int>(n) - 1].flags = 0;
    SH_CALL(Member_ClearState)(below);
    const unsigned char now = Field_MemberCount;
    if (now == 3) {
        if (Cond_Flags[0xd0] >= 3) Cond_Flags[0xd0] = 0;
    } else if (now == 2) {
        Cond_Flags[0xd0] = 0;
    }
    Field_MemberCount = static_cast<unsigned char>(now - 1);
}

// original 0x534DB0 (PSX 0x801C4084): Sprite_Current's CLUT row (+0x27) one colour: words 1..31
// of Gfx_ClutStrip row +0x27 = colour c of {0x0010, 0x4010, 0x0210, 0x4000}
// (c the argument's low byte; the original keeps the four on its stack and
// reads its own frame for c of 4 or more - ours aborts), Gfx_ClutStripDirty
// 1, Sound_PlayEffect(0x108), then Field_State +0x138 |= 8 (read after the
// call). PSX twin 0x801C4084 (pairs_propagated "call-anchored").
extern "C" void __cdecl Sprite_FlashClut(unsigned colour_arg) {
    static const std::uint16_t kColours[4] = {0x0010, 0x4010, 0x0210, 0x4000};
    const unsigned c = colour_arg & 0xFF;
    if (c >= 4)
        bof3::Fatal("Sprite_FlashClut: colour %u past the four (the original reads its own stack frame)", c);
    const unsigned row = Sprite_Current[0x27];
    for (unsigned i = 1; i < 0x20; ++i) Gfx_ClutStrip[row * 0x20 + i] = kColours[c];
    Gfx_ClutStripDirty = 1;
    SH_CALL(Sound_PlayEffect)(0x108);
    Field_State[0x138] = static_cast<unsigned char>(Field_State[0x138] | 8);
}

// original 0x537480 (PSX 0x80168AB4): a member's HP (+0x18 of its record: the second
// argument's low byte through Character_IndexById, unchecked) down by the
// first argument's word while it is more; else to 1 (an HP of 0 becomes 1
// too). Then the record Field_State +0x148 names (not the member's): with HP
// at most a quarter of +0x20 (>> 2), its state word +0x10 and Field_State's
// word +0x90 |= 0x2000. Answers what it took: the first argument whole, or
// HP - 1 (0xFFFFFFFF at HP 0); Field_Bit80Tick reads ax. PSX twin 0x80168AB4
// (pairs_propagated "call").
extern "C" std::uint32_t __cdecl Char_LoseHp(std::uint32_t amount, unsigned member_arg) {
    unsigned char* const rec = MemberRecord(static_cast<unsigned char>(member_arg));
    const unsigned hp = rv::As<records::CharRecord>(rec)->hp;
    std::uint32_t took = amount;
    if (hp <= (amount & 0xFFFF)) took = hp - 1;
    rv::Store<unsigned>(rv::As<records::CharRecord>(rec)->hp, hp - took);
    unsigned char* const actor = RecordScenaSx(Field_State[0x148]);
    if (rv::As<records::CharRecord>(actor)->hp <= (rv::As<records::CharRecord>(actor)->stats.max_hp >> 2)) {
        rv::Store<unsigned>(rv::As<records::CharRecord>(actor)->status, rv::As<records::CharRecord>(actor)->status | 0x2000);
        rv::Store<unsigned>(rv::As<records::PartyMember>(Field_State)->status, rv::As<records::PartyMember>(Field_State)->status | 0x2000);
    }
    return took;
}

void ScenaSx_Inject() {
    if (bof3::WantsShadow("scena_sx")) scena_sx::SelfTest();
    BOF3_INJECT(Char_LevelUp);
    BOF3_INJECT(Party_PlaceForBattle);
    BOF3_INJECT(Party_ReloadPalettes);
    BOF3_INJECT(Party_HealJoined);
    BOF3_INJECT(Party_Remove);
    BOF3_INJECT(Sprite_FlashClut);
    BOF3_INJECT(Char_LoseHp);
}

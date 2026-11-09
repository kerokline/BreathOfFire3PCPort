// Moved here by tools/move_funcs.py from field_hidden.cpp, rest_0a.cpp, rest_1a.cpp, rest_1b.cpp, rest_1c.cpp, rest_1d.cpp, rest_1e.cpp, rest_1f.cpp, scena_se.cpp (docs/source-layout-round.md).
#include "game/field/party_action.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/field/party_action_callees.h"
#include "game/field_hidden_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_0a.h"
#include "game/rest_1a.h"
#include "game/rest_1a_callees.h"
#include "game/rest_1b.h"
#include "game/rest_1d.h"
#include "game/rest_1d_callees.h"
#include "game/rest_1e.h"
#include "game/rest_1e_callees.h"
#include "game/rest_1f.h"
#include "game/scena_se_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = party_action::at;

}  // namespace

using namespace field_hidden;

namespace field_hidden {

using move_script::At;

using move_script::Long;

using move_script::SetLong;

using move_script::Word;

}  // namespace field_hidden

namespace {

unsigned char Al(std::uint32_t eax) { return static_cast<unsigned char>(eax); }

// A .data dispatch table's entry, read as the original reads it: in place,
// the index unchecked.
Handler Entry(std::uint32_t table, unsigned index) {
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(At(table + 4 * index)))));
}

// Field_DirectionSteps' row for a direction (two longs, x then z), the
// direction unchecked: the table has 8 rows.
long StepX(unsigned d) { return Long(At(::bof3::Img(Field_DirectionSteps_addr) + d * 8)); }

long StepZ(unsigned d) { return Long(At(::bof3::Img(Field_DirectionSteps_addr) + d * 8 + 4)); }

long Add(long a, long b) { return static_cast<long>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b)); }

// The animation offset of a direction as the original computes it: (d - 1)
// / 2 as a signed division (cdq, sub, sar), so direction 0 gives 0.
unsigned char HalfTurn(unsigned char d) { return static_cast<unsigned char>((static_cast<int>(d) - 1) / 2); }

}  // namespace

#define FH_EXPORT extern "C" __attribute__((disable_tail_calls))

namespace {

// PartyAction5_Form0Begin's side probe: the point one step in direction `d`
// (3 or 5) from the sprite. When the slope there (MapView_SlopeAt, the
// scratch flag set and more than 0x40 as a short) is steep and the ground
// there (MapView_GroundAt) is above the sprite's +0x3E, +0x2B = 0.
void ProbeSide(unsigned d) {
    const records::PartyMember* s = rv::Current<records::PartyMember>();
    const long x = Add(s->x, StepX(d));
    const long z = Add(s->z, StepZ(d));
    const long slope = g.slope_at(x, z, d);
    if (field_hidden::at::kScratch == 0 || static_cast<short>(slope) <= 0x40) return;
    const long ground = g.ground_at(x, z);
    s = rv::Current<records::PartyMember>();
    if (static_cast<short>(s->height_whole) < static_cast<short>(ground)) Sprite_Current[0x2B] = 0;
}

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

constexpr unsigned kEffectStride = 0x80;

constexpr unsigned kEffectCount = 20;

constexpr unsigned kMemberStride = 0x14C;

// Field_DirectionSteps' row for a direction (x then z, 16.16), read in place
// with the direction unmasked, as the originals index it (`shl eax, 3` on the
// zero-extended byte): the table has 8 rows; a byte above 7 reads the .data
// after it (docs/rest_0a.md section 5).
U StepXRest0a(unsigned d) { return static_cast<U>(Long(At(::bof3::Img(Field_DirectionSteps_addr) + d * 8))); }

U StepZRest0a(unsigned d) { return static_cast<U>(Long(At(::bof3::Img(Field_DirectionSteps_addr) + d * 8 + 4))); }

// A 16.16 coordinate's cell, the high word, as the originals pass it to
// AreaMap_ByteAt (a dword whose low word is the cell; the callee reads that
// word sign-extended), and the cell one on, wrapping in 16 bits.
short Cell(U v) { return static_cast<short>(static_cast<std::uint16_t>(v >> 16)); }

short CellPlusOne(U v) { return static_cast<short>(static_cast<std::uint16_t>((v >> 16) + 1)); }

// An Effect_Objects record by an index argument's low byte (`and esi, 0xFF;
// shl esi, 7`). The originals read the record unchecked; ours aborts past the
// 20 records (no caller hands one: each passes PartyAction_Kind30Ahead's 0..19).
const unsigned char* EffectByArgument(unsigned index, const char* who) {
    const unsigned i = index & 0xFF;
    if (i >= kEffectCount)
        bof3::Fatal("%s: effect object index %u is past Effect_Objects' 20 records; the original reads 0x%X unchecked", who, i,
                    static_cast<unsigned>(Effect_Objects_addr + i * kEffectStride));
    return rv::Bytes(Effect_Objects) + i * kEffectStride;
}

const unsigned char* Member(unsigned m) { return At(::bof3::Img(ObjTrio_addr) + 0x14c + (m - 1) * kMemberStride); }

bool MapTarget(unsigned char b) { return b == 0xF2; }

bool MapBlocked(unsigned char b) { return b == 0xF0 || b == 0xF1 || b == 0xF4 || b == 0xF6 || b == 0xF7; }

// The two steps ahead of Sprite_Current (x + 2 * step, z + 2 * step), read
// before any call: 0x51C390's and 0x522560's point.
void TwoAhead(U& x, U& z) {
    const unsigned char* const s = Sprite_Current;
    const unsigned d = s[8];
    x = static_cast<U>(rv::As<records::PartyMember>(s)->x) + 2u * StepXRest0a(d);
    z = static_cast<U>(rv::As<records::PartyMember>(s)->z) + 2u * StepZRest0a(d);
}

// 0x524DA0's side probe in direction d (3 or 5): the point one step that way
// from Sprite_Current (re-read); the slope there steep (the scratch flag set,
// the low word above 0x40 signed) and the ground there above the sprite's
// height word (both s16): +0x2B = 0.
void SideProbe(unsigned d) {
    const unsigned char* s = Sprite_Current;
    const U x = static_cast<U>(rv::As<records::PartyMember>(s)->x) + StepXRest0a(d);
    const U z = static_cast<U>(rv::As<records::PartyMember>(s)->z) + StepZRest0a(d);
    const long slope = SH_CALL(MapView_SlopeAt)(static_cast<long>(x), static_cast<long>(z), d);
    if (SharedScratch.b_00 == 0 || static_cast<short>(slope) <= 0x40) return;
    const long ground = SH_CALL(MapView_GroundAt)(static_cast<long>(x), static_cast<long>(z));
    s = Sprite_Current;
    if (static_cast<short>(rv::As<records::PartyMember>(s)->height_whole) < static_cast<short>(ground)) Sprite_Current[0x2B] = 0;
}

using URest1a = std::uint32_t;

using move_script::SetLong;

using move_script::Word;

unsigned char& B(URest1a a) { return *At(a); }

std::int32_t L(URest1a a) { return move_script::Long(move_script::At(a)); }

unsigned char* Sc() { return Sprite_Current; }

unsigned char* Fs() { return Field_State; }

// Field_DirectionSteps' row for a direction (x then z), read in place with the
// direction byte unmasked, as every reader of the table does.
URest1a StepXRest1a(unsigned d) { return static_cast<URest1a>(L(::bof3::Img(rest_1a::at::kSteps.v) + d * 8)); }

URest1a StepZRest1a(unsigned d) { return static_cast<URest1a>(L(::bof3::Img(rest_1a::at::kSteps.v) + d * 8 + 4)); }

// `cdq; xor eax, edx; sub eax, edx`: the absolute value with the original's
// wrap (the most negative stays negative).
std::int32_t Abs32(std::int32_t v) {
    const auto u = static_cast<URest1a>(v);
    const URest1a s = static_cast<URest1a>(v >> 31);
    return static_cast<std::int32_t>((u ^ s) - s);
}

// A .data dispatch table read in place. The original's index is unchecked:
// past the table it jumps through the next table's cell, or into data. Ours
// aborts at the table's own count (its symbols.toml count), before the read -
// the rule all seven of wave one's groups share (docs/rest_1b.md section 6).
// While the fuzz runs, the entries are its recorders.
using HandlerRest1a = void (__cdecl*)();

HandlerRest1a CodeAt(URest1a table, unsigned count, unsigned index, const char* who) {
    const URest1a cell = table + 4u * index;
    if (index >= count)
        bof3::Fatal("%s: index %u is past its table's %u entries at 0x%X; the original jumps through 0x%X", who, index,
                    count, static_cast<unsigned>(::bof3::VaOf(table)), static_cast<unsigned>(::bof3::VaOf(cell)));
    return reinterpret_cast<HandlerRest1a>(static_cast<std::uintptr_t>(static_cast<URest1a>(L(cell))));
}

// Sprite_ObjectAt's answer marked: bit 0 of the object's +0x80 - 0..0x1D a
// Sprite_Objects record, 0x1E..0x21 a Sprite_ObjectsExtra one. The original
// compares the index signed (0x51C190's three copies, 0x51D2F0: movsx, jge)
// or unsigned (0x51C530: jae) and writes whatever record that names; the
// callee answers 0..0x21 or 0xFF only, so ours aborts on any other.
void MarkObject(unsigned char found, const char* who) {
    if (found > 0x21) bof3::Fatal("%s: Sprite_ObjectAt answered 0x%X, no object (it answers 0..0x21 or 0xFF); the original marks a record past the tables", who, found);
    if (found < 0x1E) B(::bof3::Img(rest_1a::at::kObjectFlags.v) + found * rest_1a::at::kObjectStride) |= 1;
    else B(::bof3::Img(rest_1a::at::kExtraFlags.v) + (found - 0x1Eu) * rest_1a::at::kObjectStride) |= 1;
}

// An Effect_Objects record by an index a callee answered (Field_EffectAhead,
// PartyAction_Kind30Ahead: 0..19 or 0xFF, tested first). The original uses it
// unchecked; ours aborts past the 20 records.
unsigned char* EffectByAnswer(unsigned char i, const char* who) {
    if (i >= 20) bof3::Fatal("%s: effect object index 0x%X is past Effect_Objects' 20 records (its callee answers 0..19)", who, i);
    return rv::Bytes(&Effect_Objects[i]);
}

// The party sets' cell helpers, which their callers reach by E8.
using CellFn = unsigned char (__cdecl*)(unsigned, unsigned);

// 0x51BFD0 / 0x51C7C0 / 0x51CD10 (and set 5's 0x51E930, ours in field_hidden):
// an even direction +8 is turned one eighth back (-1, & 7); when
// PartyAction_TargetAhead finds nothing that way, two on (+2), and when it
// finds nothing there either, back to the first turn (-2). Then the slope one
// step ahead in +8 (MapView_SlopeAt with the whole direction byte): steep (the
// scratch flag set and the low word above 0x40, signed) - the pose
// (+8 - 1) / 2 + 0x46 and +2 one on; otherwise +0x2B = 1, the side probes in
// directions 3 and 5 (Field_DirectionSteps' rows read by address; each clears
// it again on a steep slope with the ground above the sprite's +0x3E),
// Sound_PlayEffect(u16 +0x2C + 0x100), the pose (+8 - 1) / 2 + 0x42 and +0xA =
// 5. Then +0xB = 0 and +2 one on - a steep slope moves +2 by two.
void SideProbeRest1a(URest1a row, unsigned d) {
    const unsigned char* const s = Sc();
    const URest1a x = static_cast<URest1a>(rv::As<records::PartyMember>(s)->x) + static_cast<URest1a>(L(row));
    const URest1a z = static_cast<URest1a>(rv::As<records::PartyMember>(s)->z) + static_cast<URest1a>(L(row + 4));
    const long slope = SH_CALL(MapView_SlopeAt)(static_cast<long>(x), static_cast<long>(z), d);
    if (rest_1a::at::kScratchFlag == 0 || static_cast<short>(slope) <= 0x40) return;
    const long ground = SH_CALL(MapView_GroundAt)(static_cast<long>(x), static_cast<long>(z));
    unsigned char* const c = Sc();
    if (static_cast<short>(rv::As<records::PartyMember>(c)->height_whole) < static_cast<short>(ground)) c[0x2B] = 0;
}

void Form0Begin() {
    if ((Sc()[8] & 1) == 0) {
        Sc()[8] = static_cast<unsigned char>((Sc()[8] - 1) & 7);
        if (SH_CALL(PartyAction_TargetAhead)() == 0) {
            Sc()[8] = static_cast<unsigned char>((Sc()[8] + 2) & 7);
            if (SH_CALL(PartyAction_TargetAhead)() == 0) Sc()[8] = static_cast<unsigned char>((Sc()[8] - 2) & 7);
        }
    }
    const unsigned char* const s = Sc();
    const unsigned d = s[8];
    const URest1a x = static_cast<URest1a>(rv::As<records::PartyMember>(s)->x) + StepXRest1a(d);
    const URest1a z = static_cast<URest1a>(rv::As<records::PartyMember>(s)->z) + StepZRest1a(d);
    const long slope = SH_CALL(MapView_SlopeAt)(static_cast<long>(x), static_cast<long>(z), d);
    if (rest_1a::at::kScratchFlag != 0 && static_cast<short>(slope) > 0x40) {
        SH_CALL(Sprite_EnsureAnimation)(static_cast<unsigned char>(HalfTurn(Sc()[8]) + 0x46));
        ++Sc()[2];
    } else {
        Sc()[0x2B] = 1;
        SideProbeRest1a(::bof3::Img(Field_DirectionSteps_addr) + 0x18, 3);
        SideProbeRest1a(::bof3::Img(Field_DirectionSteps_addr) + 0x28, 5);
        SH_CALL(Sound_PlayEffect)(static_cast<unsigned short>(Word(Sc() + 0x2C) + 0x100));
        SH_CALL(Sprite_EnsureAnimation)(static_cast<unsigned char>(HalfTurn(Sc()[8]) + 0x42));
        Sc()[0xA] = 5;
    }
    Sc()[0xB] = 0;
    ++Sc()[2];
}

// 0x51C190 / 0x51C980 / 0x51CED0 (set 5's 0x51EAF0): +0xA counted down; at 0,
// the point two steps ahead in +8. The object Sprite_ObjectAt(point, 0) finds
// there is marked (MarkObject, the index compared signed); then the set's cell
// pickup on the point's cell, and when that answers 0, on the cell one on in x
// (only when the point's x has a fraction), then, when that answers 0 too or
// was not asked, one on in z (when z has one); then +2 one on.
// Sprite_ScriptTickOnce every time. The cells are the point's high words; the
// original passes them as dwords whose upper halves are its own stack (the
// pickup reads only the low word: docs/rest_1a.md section 3).
void Form0Resolve(CellFn pickup, const char* who) {
    --Sc()[0xA];
    if (Sc()[0xA] == 0) {
        const unsigned char* const s = Sc();
        const unsigned d = s[8];
        const URest1a x = static_cast<URest1a>(rv::As<records::PartyMember>(s)->x) + 2u * StepXRest1a(d);
        const URest1a z = static_cast<URest1a>(Long(s + 0x38)) + 2u * StepZRest1a(d);
        const unsigned char found = SH_CALL(Sprite_ObjectAt)(static_cast<long>(x), static_cast<long>(z), 0);
        if (found != 0xFF) MarkObject(found, who);
        const unsigned cx = x >> 16, cz = z >> 16;
        if (pickup(cx, cz) == 0) {
            bool done = false;
            if ((x & 0xFFFF) != 0) done = pickup(cx + 1, cz) != 0;
            if (!done && (z & 0xFFFF) != 0) pickup(cx, cz + 1);
        }
        ++Sc()[2];
    }
    SH_CALL(Sprite_ScriptTickOnce)();
}

// 0x51C270 / 0x51CA60 / 0x51CFB0 (set 5's Field_CellPickup 0x51EBD0): what
// lies in the map cell (x, z) - AreaMap_ByteAt:
//   0xF2 - with an effect object free (Effect_FindFree): Effect_SpawnAtCell(0,
//          x, z); a Rand nibble of 13..15 finds zenny, 2 (5 on 15), ten times
//          that when Field_InputFlags has bit 1 or 2 and a second Rand & 3 is
//          0: Field_GiveZenny(the amount), Effect_SpawnAtCell(1, x, z); then
//          +0xB = 1. With no object free, none of that.
//   0xF8 - Effect_SpawnAtCell(0, x, z); the name of item 0x56 of category 0
//          (Item_NamePtr) copied, 16 bytes, into Text_Records; Inventory_Add(0,
//          0x56, 1): taken, Sound_PlayEffect(0x106) and Msg_OpenSystem(2), else
//          Msg_OpenSystem(3); Field_Request = 2, +0xB = 1.
// Both clear the cell (AreaMap_ClearCell) and answer 1; any other cell 0. x
// and z go on as given (every callee reads their low words); the original
// keeps the amount in its own z slot's low byte, which nothing reads again.
unsigned char CellPickup(unsigned x, unsigned z) {
    const unsigned char cell = SH_CALL(AreaMap_ByteAt)(static_cast<short>(x), static_cast<short>(z));
    if (cell == 0xF2) {
        if (SH_CALL(Effect_FindFree)() != 0xFF) {
            SH_CALL(Effect_SpawnAtCell)(0, x, z);
            const unsigned roll = static_cast<unsigned char>(SH_CALL(Rand)()) & 0xFu;
            if (roll >= 0xD) {
                unsigned char amount = roll < 0xF ? 2 : 5;
                if ((Field_InputFlags & 6) != 0 && (static_cast<unsigned char>(SH_CALL(Rand)()) & 3) == 0)
                    amount = static_cast<unsigned char>(amount * 10);
                SH_CALL(Field_GiveZenny)(amount);
                SH_CALL(Effect_SpawnAtCell)(1, x, z);
            }
            Sc()[0xB] = 1;
        }
    } else if (cell == 0xF8) {
        SH_CALL(Effect_SpawnAtCell)(0, x, z);
        const unsigned char* const name = SH_CALL(Item_NamePtr)(0, 0x56);
        for (unsigned k = 0; k < 16; k += 4) SetLong(At(::bof3::Img(bof3::addr::Text_Records) + k), Long(name + k));
        if (SH_CALL(Inventory_Add)(0, 0x56, 1) != 0) {
            SH_CALL(Sound_PlayEffect)(0x106);
            SH_CALL(Msg_OpenSystem)(2);
        } else {
            SH_CALL(Msg_OpenSystem)(3);
        }
        Field_Request = 2;
        Sc()[0xB] = 1;
    } else {
        return 0;
    }
    SH_CALL(AreaMap_ClearCell)(x, z);
    return 1;
}

// 0x51D160 (by facings 1 and 7) and 0x51C490 (by 3 and 5): when +8 has come
// round to +3, the pose for the facing reached (0x40 / 0x41, none for any
// other) and +2 one on; else +9 counted down and, at 0, +9 = 2, +8 += +0xB
// (Sprite_TurnSense's step), & 7, and the pose +8.
void TurnToward(unsigned char lo, unsigned char lo_pose, unsigned char hi, unsigned char hi_pose) {
    unsigned char* const s = Sc();
    const unsigned char target = s[3];
    if (s[8] == target) {
        if (target == lo) SH_CALL(Sprite_EnsureAnimation)(lo_pose);
        else if (target == hi) SH_CALL(Sprite_EnsureAnimation)(hi_pose);
        ++Sc()[2];
        return;
    }
    --s[9];
    if (Sc()[9] != 0) return;
    Sc()[9] = 2;
    Sc()[8] = static_cast<unsigned char>(Sc()[8] + Sc()[0xB]);
    Sc()[8] = static_cast<unsigned char>(Sc()[8] & 7);
    SH_CALL(Sprite_EnsureAnimation)(Sc()[8]);
}

constexpr unsigned kObjects = 0x1E;        // Sprite_Objects' 30, then Sprite_ObjectsExtra's 4

constexpr unsigned kObjectsAll = 0x22;

// kSteps: now Field_DirectionSteps (symbols.toml). Field_DirectionSteps: 8 rows of two longs
constexpr ::bof3::image::Va kScriptFlagsHigh{0x9039A3};                // Field_ScriptFlags' high byte (0x9039A2, u16)

unsigned char* S() { return Sprite_Current; }

// A .data table's address (its symbols.toml name is a pointer).
U TableAt(const unsigned long* table) { return Low32(table); }

// A .data dispatch table read in place. The original's index is unchecked:
// past the table it jumps through the next table's cell, or into data. Ours
// aborts at the table's own count (its symbols.toml count), before the read
// (docs/rest_1b.md section 6). While the fuzz runs, the entries are its
// recorders.
using Handler = void (__cdecl*)();

Handler CodeAtRest1b(U table, unsigned count, unsigned index, const char* who) {
    const U cell = table + 4u * index;
    if (index >= count)
        bof3::Fatal("%s: index %u is past its table's %u entries at 0x%X; the original jumps through 0x%X", who, index,
                    count, static_cast<unsigned>(::bof3::VaOf(table)), static_cast<unsigned>(::bof3::VaOf(cell)));
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<U>(Long(At(cell)))));
}

void ByForm(U table, unsigned count, const char* who) { CodeAtRest1b(table, count, rv::As<records::PartyMember>(S())->column, who)(); }

void ByState(U table, unsigned count, const char* who) { CodeAtRest1b(table, count, S()[2], who)(); }

void ByStep(U table, unsigned count, const char* who) { CodeAtRest1b(table, count, S()[3], who)(); }

// Field_DirectionSteps' row for a direction, read in place with the direction
// unmasked (`shl eax, 3` on the zero-extended byte): a byte above 7 reads the
// .data after the table, as the originals do.
U StepXRest1b(unsigned d) { return static_cast<U>(Long(At(::bof3::Img(Field_DirectionSteps_addr) + d * 8))); }

U StepZRest1b(unsigned d) { return static_cast<U>(Long(At(::bof3::Img(Field_DirectionSteps_addr) + d * 8 + 4))); }

// The sound of the leader's form: u16 +0x2C + 0x100.
void FormSound(const unsigned char* s) { SH_CALL(Sound_PlayEffect)(static_cast<unsigned short>(Word(s + 0x2C) + 0x100)); }

// An Effect_Objects record by an index the original uses unchecked: ours aborts
// past the 20 (no callee hands such an index: Effect_FindFree, Field_EffectAhead
// and PartyAction_Kind30Ahead answer 0..19 or 0xFF, tested first).
unsigned char* EffectAt(unsigned index, const char* who) {
    if (index >= kEffectCount)
        bof3::Fatal("%s: effect object index 0x%X is past Effect_Objects' 20 records; the original writes 0x%X unchecked", who,
                    index, static_cast<unsigned>(0x7E11E0 + index * kEffectStride));
    return rv::Bytes(Effect_Objects) + index * kEffectStride;
}

// Bit 0 of +0x80 of the object Sprite_ObjectAt found: below 0x1E Sprite_Objects,
// else Sprite_ObjectsExtra (index - 0x1E). `is_signed`: the original compares
// the byte signed (movsx; jge), so 0x80..0xFE index before Sprite_Objects;
// else unsigned (jae), so 0x22..0xFE index past Sprite_ObjectsExtra. Sprite_ObjectAt
// answers 0..0x21 or 0xFF, so neither is reached: ours aborts there.
void MarkObjectRest1b(unsigned char found, bool is_signed, const char* who) {
    const int i = is_signed ? static_cast<signed char>(found) : static_cast<int>(found);
    if (i < 0 || i >= static_cast<int>(kObjectsAll))
        bof3::Fatal("%s: Sprite_ObjectAt's answer 0x%X is past the 34 objects; the original marks it unchecked", who,
                    static_cast<unsigned>(found));
    if (i < static_cast<int>(kObjects)) Sprite_Objects[i].context |= 1;
    else Sprite_ObjectsExtra[i - kObjects].context |= 1;
}

// An even direction +8 turned one eighth back (-1, & 7); when `probe` answers
// 0, two on (+2), and when it answers 0 again, back to the first turn (-2).
// Sprite_Current re-read after each call; an odd direction is left alone.
template <typename P> void TurnUntil(P probe) {
    unsigned char* const s = S();
    const unsigned char d = s[8];
    if (d & 1) return;
    s[8] = static_cast<unsigned char>((d - 1) & 7);
    if (probe() != 0) return;
    S()[8] = static_cast<unsigned char>((S()[8] + 2) & 7);
    if (probe() != 0) return;
    S()[8] = static_cast<unsigned char>((S()[8] - 2) & 7);
}

// PartyAction5_Form0Begin's side probe (inline in the copies): the point one
// step in direction d (3 or 5; rows read by address, the direction pushed as
// an immediate); a steep slope there (the scratch flag set, the low word above
// 0x40 signed) and the ground above the sprite's height word: +0x2B = 0.
void ProbeSideRest1b(unsigned d) {
    const unsigned char* s = S();
    const U x = static_cast<U>(rv::As<records::PartyMember>(s)->x) + StepXRest1b(d);
    const U z = static_cast<U>(rv::As<records::PartyMember>(s)->z) + StepZRest1b(d);
    const long slope = SH_CALL(MapView_SlopeAt)(static_cast<long>(x), static_cast<long>(z), d);
    if (SharedScratch.b_00 == 0 || static_cast<short>(slope) <= 0x40) return;
    const long ground = SH_CALL(MapView_GroundAt)(static_cast<long>(x), static_cast<long>(z));
    s = S();
    if (static_cast<short>(rv::As<records::PartyMember>(s)->height_whole) < static_cast<short>(ground)) S()[0x2B] = 0;
}

// 0x51D790 / 0x51DF10 (PartyAction5_Form0Begin's code).
void Begin() {
    TurnUntil([] { return SH_CALL(PartyAction_TargetAhead)(); });
    const unsigned char* const s = S();
    const unsigned d = s[8];
    const U x = static_cast<U>(rv::As<records::PartyMember>(s)->x) + StepXRest1b(d);
    const U z = static_cast<U>(rv::As<records::PartyMember>(s)->z) + StepZRest1b(d);
    const long slope = SH_CALL(MapView_SlopeAt)(static_cast<long>(x), static_cast<long>(z), d);
    if (SharedScratch.b_00 != 0 && static_cast<short>(slope) > 0x40) {
        SH_CALL(Sprite_EnsureAnimation)(static_cast<unsigned char>(HalfTurn(S()[8]) + 0x46));
        ++S()[2];
    } else {
        S()[0x2B] = 1;
        ProbeSideRest1b(3);
        ProbeSideRest1b(5);
        FormSound(S());
        SH_CALL(Sprite_EnsureAnimation)(static_cast<unsigned char>(HalfTurn(S()[8]) + 0x42));
        S()[0xA] = 5;
    }
    S()[0xB] = 0;
    ++S()[2];
}

// The point two steps ahead of Sprite_Current (read now) and its cells.
struct Ahead {
    U x, z;
};

Ahead TwoAheadRest1b() {
    const unsigned char* const s = S();
    const unsigned d = s[8];
    return {static_cast<U>(rv::As<records::PartyMember>(s)->x) + 2u * StepXRest1b(d), static_cast<U>(rv::As<records::PartyMember>(s)->z) + 2u * StepZRest1b(d)};
}

// The cell handler on the point's cell; when it answers 0, on the cell one on
// in x (only when x has a fraction), and when that answers 0 too (or was not
// asked), on the cell one on in z (only when z has one). The cells are the
// point's high words; the original passes dwords whose upper halves are its
// own stack, and every handler reads the low words.
template <typename C> void Cells(const Ahead& p, C cell) {
    const unsigned cx = p.x >> 16, cz = p.z >> 16;
    if (cell(cx, cz) != 0) return;
    if ((p.x & 0xFFFF) != 0 && cell(cx + 1, cz) != 0) return;
    if ((p.z & 0xFFFF) != 0) cell(cx, cz + 1);
}

// 0x51D950 / 0x51E0D0 (PartyAction5_Form0Resolve's code).
template <typename C> void Resolve(C cell, const char* who) {
    --S()[0xA];
    if (S()[0xA] == 0) {
        const Ahead p = TwoAheadRest1b();
        const unsigned char found = SH_CALL(Sprite_ObjectAt)(static_cast<long>(p.x), static_cast<long>(p.z), 0);
        if (found != 0xFF) MarkObjectRest1b(found, true, who);
        Cells(p, cell);
        ++S()[2];
    }
    SH_CALL(Sprite_ScriptTickOnce)();
}

// 0x51DAA0 / 0x51E1B0 (Field_CellPickup's code): 0xF2 - with an effect object
// free, Effect_SpawnAtCell(0), three Rand draws in sixteen find zenny (2, or 5
// on 15; ten times that when Field_InputFlags has bit 1 or 2 and a second Rand
// & 3 is 0) and Effect_SpawnAtCell(1), +0xB = 1; 0xF8 - Effect_SpawnAtCell(0),
// item 0x56's name into Text_Records, Inventory_Add: taken, a sound and message
// 2, else message 3; Field_Request = 2, +0xB = 1. Both clear the cell; 1.
unsigned char CellPickupRest1b(unsigned x, unsigned z) {
    const unsigned char cell = SH_CALL(AreaMap_ByteAt)(static_cast<short>(x), static_cast<short>(z));
    if (cell == 0xF2) {
        if (SH_CALL(Effect_FindFree)() != 0xFF) {
            SH_CALL(Effect_SpawnAtCell)(0, x, z);
            const unsigned roll = static_cast<unsigned char>(SH_CALL(Rand)()) & 0xFu;
            if (roll >= 0xD) {
                unsigned char amount = roll < 0xF ? 2 : 5;
                if ((Field_InputFlags & 6) != 0 && (static_cast<unsigned char>(SH_CALL(Rand)()) & 3) == 0)
                    amount = static_cast<unsigned char>(amount * 10);
                SH_CALL(Field_GiveZenny)(amount);
                SH_CALL(Effect_SpawnAtCell)(1, x, z);
            }
            S()[0xB] = 1;
        }
    } else if (cell == 0xF8) {
        SH_CALL(Effect_SpawnAtCell)(0, x, z);
        const unsigned char* const name = SH_CALL(Item_NamePtr)(0, 0x56);
        for (unsigned k = 0; k < 16; k += 4) move_script::SetLong(At(::bof3::Img(bof3::addr::Text_Records) + k), Long(name + k));
        if (SH_CALL(Inventory_Add)(0, 0x56, 1) != 0) {
            SH_CALL(Sound_PlayEffect)(0x106);
            SH_CALL(Msg_OpenSystem)(2);
        } else {
            SH_CALL(Msg_OpenSystem)(3);
        }
        Field_Request = 2;
        S()[0xB] = 1;
    } else {
        return 0;
    }
    SH_CALL(AreaMap_ClearCell)(x, z);
    return 1;
}

// 0x51DC00 / 0x51E310.
void Form1Begin(const char* who) {
    const unsigned char k = SH_CALL(PartyAction_Kind30Ahead)();
    if (k != 0xFF) {
        if (SH_CALL(PartyAction_MemberOnEffect)(k) != 0 || SH_CALL(PartyAction_MemberBeyondEffect)(k) != 0) {
            Field_State[0x137] = 0;
            return;
        }
        const unsigned char* const s = S();
        EffectAt(k, who)[0xB] = 1;
        FormSound(s);
        SH_CALL(Sprite_EnsureAnimation)(static_cast<unsigned char>(S()[8] + 8));
        Field_State[0x128] = 2;
        At(::bof3::Img(kScriptFlagsHigh.v))[0] |= 0x10;
        SH_CALL(Field_JumpStart)();
        --S()[9];
        SH_CALL(Field_LeaderStepTick)();
        Field_State[0x137] = 1;
        ++S()[2];
        return;
    }
    const Ahead p = TwoAheadRest1b();
    if ((Field_State[0x138] & 1) == 0) {
        const unsigned char found = SH_CALL(Sprite_ObjectAt)(static_cast<long>(p.x), static_cast<long>(p.z), 1);
        if (found != 0xFF) MarkObjectRest1b(found, false, who);
    }
    Field_State[0x137] = 0;
}

// 0x51E570 / 0x51EDF0. `sounds_first`: set 4's makes the form's sound before
// Field_EffectAhead and its 0x10B before it marks the effect object; set 5's
// neither first.
template <typename C> void Hit(C cell, bool sounds_first, const char* who) {
    unsigned char* s = S();
    if (s[0xA] != 0) {
        s[0xA] = static_cast<unsigned char>(s[0xA] - 1);
        s = S();
        if (s[0xA] == 0) {
            if (sounds_first) FormSound(s);
            const unsigned char e = SH_CALL(Field_EffectAhead)();
            if (e != 0xFF) {
                unsigned char* const object = EffectAt(e, who);
                if (sounds_first) {
                    SH_CALL(Sound_PlayEffect)(0x10B);
                    unsigned char* const t = S();
                    object[8] = t[8];
                    object[0xA] = 1;
                    t[6] = e;
                } else {
                    object[8] = S()[8];
                    object[0xA] = 1;
                    SH_CALL(Sound_PlayEffect)(0x10B);
                    S()[6] = e;
                }
                ++S()[3];
            } else {
                const Ahead p = TwoAheadRest1b();
                const unsigned char found = SH_CALL(Sprite_ObjectAt)(static_cast<long>(p.x), static_cast<long>(p.z), 0);
                if (found != 0xFF) {
                    MarkObjectRest1b(found, true, who);
                    SH_CALL(Sound_PlayEffect)(0x10B);
                }
                Cells(p, cell);
            }
            ++S()[3];
        }
    }
    SH_CALL(Sprite_ScriptTickOnce)();
}

// 0x51E6C0 / 0x51EF30.
unsigned char CellHit(unsigned x, unsigned z) {
    const unsigned char cell = SH_CALL(AreaMap_ByteAt)(static_cast<short>(x), static_cast<short>(z));
    if (cell == 0xF0 || cell == 0xF1 || cell == 0xF4) {
        SH_CALL(Effect_SpawnAtCellHigh)(0, x, z);
        if ((static_cast<unsigned>(SH_CALL(Rand)()) & 7u) > 5) SH_CALL(Effect_SpawnAtCellHigh)(4, x, z);
        SH_CALL(Sound_PlayEffect)(0x10B);
        return 1;
    }
    if (cell != 0xF6 && cell != 0xF7) return 0;
    SH_CALL(Effect_SpawnAtCellHigh)(0, x, z);
    SH_CALL(Sound_PlayEffect)(0x10B);
    const unsigned roll = static_cast<unsigned char>(SH_CALL(Rand)()) & 0xFu;
    if (roll < 7) {
        SH_CALL(Effect_SpawnAtCellHigh)(3, x, z);
        const unsigned char* const name = SH_CALL(Item_NamePtr)(0, 0x29);
        for (unsigned k = 0; k < 16; k += 4) move_script::SetLong(At(::bof3::Img(bof3::addr::Text_Records) + k), Long(name + k));
        if (SH_CALL(Inventory_Add)(0, 0x29, 1) != 0) {
            SH_CALL(Sound_PlayEffect)(0x106);
            SH_CALL(Msg_OpenSystem)(2);
        } else {
            SH_CALL(Msg_OpenSystem)(3);
        }
        S()[0xB] = 2;
    } else if (roll > 0xB) {
        SH_CALL(Effect_SpawnAtCellHigh)(2, x, z);
        SH_CALL(Sprite_FlashClut)(0);
        SH_CALL(Char_LoseHp)(1, Field_State[0x89]);
        SH_CALL(Msg_OpenSystem)(0xD9);
        S()[0xB] = 1;
    }
    Field_Request = 2;
    return 1;
}

}  // namespace

#define R1B_TABLE(t) TableAt(t), t##_count   // the table and its count: ours aborts past it (section 6)

namespace {

// kSteps: now Field_DirectionSteps (symbols.toml). Field_DirectionSteps: 8 rows of two longs
// kStep3: now Field_DirectionSteps[6] (symbols.toml).
// kStep5: now Field_DirectionSteps[0xa] (symbols.toml). rows 3 and 5, read by address
inline auto& kSloped = SharedScratch.b_00;  // SharedScratch's first byte: AreaMap_Slope's "sloped" flag

// kObjectFlag: now Sprite_Objects[0].context (symbols.toml). Sprite_Objects + 0x80
// kExtraFlag: now Sprite_ObjectsExtra[0].context (symbols.toml). Sprite_ObjectsExtra + 0x80
constexpr U kObjectStride = 0xA4;

// The count of each of the 28 dispatch tables, by its symbols.toml entry.
struct TableCount { const unsigned long* table; unsigned count; };

unsigned CountOf(U table, const char* who) {
#define R1C_T(t) {t, t##_count}
    static const TableCount kCounts[] = {
        R1C_T(PartyFormAction6_Form1States), R1C_T(PartyAction6_Form1States), R1C_T(PartyFormAction6_Form2States),
        R1C_T(PartyAction6_Form2States), R1C_T(PartyAction6_Form2State0Steps), R1C_T(PartyAction6_Form2State1Steps),
        R1C_T(PartyFormAction6_Forms), R1C_T(PartyAction6_Forms), R1C_T(PartyFormAction7_Form0States),
        R1C_T(PartyAction7_Form0States), R1C_T(PartyFormAction7_Form1States), R1C_T(PartyFormAction7_Form2States),
        R1C_T(PartyAction7_Form2States), R1C_T(PartyFormAction7_Forms), R1C_T(PartyAction7_Forms),
        R1C_T(PartyFormAction8_Form0States), R1C_T(PartyAction8_Form0States), R1C_T(PartyFormAction8_Form1States),
        R1C_T(PartyAction8_Form1States), R1C_T(PartyFormAction8_Form2States), R1C_T(PartyAction8_Form2States),
        R1C_T(PartyFormAction8_Forms), R1C_T(PartyAction8_Forms), R1C_T(PartyFormAction9_Form0States),
        R1C_T(PartyAction9_Form0States), R1C_T(PartyFormAction9_Form1States), R1C_T(PartyAction9_Form1States),
        R1C_T(PartyAction9_Form1State0Steps),
    };
#undef R1C_T
    for (const TableCount& t : kCounts)
        if (Low32(t.table) == table) return t.count;
    bof3::Fatal("%s: 0x%X is not one of the group's 28 dispatch tables", who, static_cast<unsigned>(table));
}

// A dispatch table's entry, read in place. The original's index is unchecked:
// past the table it jumps through the next table's cell, or into data. Ours
// aborts at the table's own count, before the read - the rule all seven of
// wave one's groups share (docs/rest_1b.md section 6). While the fuzz runs the
// table holds the harness's recorders.
Handler EntryRest1c(U table, unsigned index, const char* who) {
    const U at = table + 4u * index;
    const unsigned count = CountOf(table, who);
    if (index >= count)
        bof3::Fatal("%s: index %u is past its table's %u entries at 0x%X; the original jumps through 0x%X", who, index,
                    count, (unsigned)table, (unsigned)at);
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<U>(Long(At(at)))));
}

void ByFormRest1c(U table, const char* who) { EntryRest1c(table, rv::Current<records::PartyMember>()->column, who)(); }

void ByStateRest1c(U table, const char* who) { EntryRest1c(table, Sprite_Current[2], who)(); }

void ByStepRest1c(U table, const char* who) { EntryRest1c(table, Sprite_Current[3], who)(); }

// Field_DirectionSteps' row for a direction, read in place with the direction
// unmasked (`shl eax, 3` on the zero-extended byte), as the originals read it.
U StepXRest1c(unsigned d) { return static_cast<U>(Long(At(::bof3::Img(Field_DirectionSteps_addr) + d * 8))); }

U StepZRest1c(unsigned d) { return static_cast<U>(Long(At(::bof3::Img(Field_DirectionSteps_addr) + d * 8 + 4))); }

// The animation offset of a direction as the originals compute it: (d - 1) / 2
// as a signed division (cdq, sub, sar), so direction 0 gives 0; plus a base, as
// a byte (`add al`). For any direction byte the upper bytes are 0.
unsigned char Turned(unsigned char d, unsigned base) {
    return static_cast<unsigned char>((static_cast<int>(d) - 1) / 2 + static_cast<int>(base));
}

// A direction byte turned and masked: (d + n) & 7, as `add` / `sub` and `and 7`.
void Turn(int n) { Sprite_Current[8] = static_cast<unsigned char>((Sprite_Current[8] + n) & 7); }

unsigned char Sloped() { return kSloped; }

// Bit 0 of +0x80 of the object Sprite_ObjectAt found: Sprite_Objects 0..0x1D,
// Sprite_ObjectsExtra 0x1E..0x21 (Sprite_ObjectAt answers those or 0xFF). The
// originals index unchecked (0x51F700 / Form0Resolve the byte signed, 0x51FAF0
// unsigned); ours aborts on any other byte.
void MarkObjectRest1c(unsigned char found, const char* who) {
    if (found > 0x21)
        bof3::Fatal("%s: Sprite_ObjectAt answered 0x%02X, past the 34 objects; the original writes bit 0 of +0x80 there unchecked",
                    who, found);
    if (found < 0x1E) At(::bof3::Img(Sprite_Objects_addr) + 0x80 + found * kObjectStride)[0] |= 1;
    else At(::bof3::Img(Sprite_ObjectsExtra_addr) + 0x80 + (found - 0x1E) * kObjectStride)[0] |= 1;
}

// An Effect_Objects record by an index the original writes through unchecked;
// ours aborts past the 20 (Field_EffectAhead and PartyAction_Kind30Ahead, the
// only sources, answer 0..19 or 0xFF, which the callers test first).
unsigned char* EffectToWrite(int index, const char* who) {
    if (index < 0 || index >= static_cast<int>(kEffectCount))
        bof3::Fatal("%s: effect object index %d is past Effect_Objects' 20 records; the original writes there unchecked", who,
                    index);
    return rv::Bytes(&Effect_Objects[index]);
}

// The side probe the Form0Begin shape makes inline in directions 3 and 5 (the
// rows read by address, the direction pushed as an immediate): the point one
// step that way from Sprite_Current (re-read); the slope there steep (the
// sloped flag set, its low word above 0x40 signed) and the ground there above
// the sprite's height word (both s16): +0x2B = 0. PartyAction_SideProbes'
// probe, and PartyAction5_Form0Begin's ProbeSideRest1c.
void ProbeSideRest1c(U row, unsigned d) {
    const unsigned char* s = Sprite_Current;
    const U x = static_cast<U>(rv::As<records::PartyMember>(s)->x) + static_cast<U>(Long(At(row)));
    const U z = static_cast<U>(rv::As<records::PartyMember>(s)->z) + static_cast<U>(Long(At(row + 4)));
    const long slope = SH_CALL(MapView_SlopeAt)(static_cast<long>(x), static_cast<long>(z), d);
    if (Sloped() == 0 || static_cast<short>(slope) <= 0x40) return;
    const long ground = SH_CALL(MapView_GroundAt)(static_cast<long>(x), static_cast<long>(z));
    s = Sprite_Current;
    if (static_cast<short>(rv::As<records::PartyMember>(s)->height_whole) < static_cast<short>(ground)) Sprite_Current[0x2B] = 0;
}

// The first turn of the Begin shapes: an even direction +8 is turned one
// eighth back; when `ahead` finds nothing (al 0) two on, and when it finds
// nothing there either, back to the first. An odd direction is kept.
void FirstTurn(unsigned char (__cdecl* ahead)(void)) {
    if ((Sprite_Current[8] & 1) != 0) return;
    Turn(-1);
    if (ahead() != 0) return;
    Turn(2);
    if (ahead() != 0) return;
    Turn(-2);
}

unsigned char __cdecl TargetAhead() { return SH_CALL(PartyAction_TargetAhead)(); }

unsigned char __cdecl BlockedAhead() { return SH_CALL(PartyAction_BlockedAhead)(); }

// The not-steep half of both Begin shapes: +0x2B = 1, the side probes in
// directions 3 and 5, Sound_PlayEffect(u16 +0x2C + 0x100),
// Sprite_EnsureAnimation(the turn + 0x42), +0xA = 5.
void BeginLevel() {
    Sprite_Current[0x2B] = 1;
    ProbeSideRest1c(::bof3::Img(Field_DirectionSteps_addr) + 0x18, 3);
    ProbeSideRest1c(::bof3::Img(Field_DirectionSteps_addr) + 0x28, 5);
    SH_CALL(Sound_PlayEffect)(static_cast<unsigned short>(Word(Sprite_Current + 0x2C) + 0x100));
    SH_CALL(Sprite_EnsureAnimation)(Turned(Sprite_Current[8], 0x42));
    Sprite_Current[0xA] = 5;
}

// The steep half: Sprite_EnsureAnimation(the turn + 0x46), +2 one on.
void BeginSteep() {
    SH_CALL(Sprite_EnsureAnimation)(Turned(Sprite_Current[8], 0x46));
    ++Sprite_Current[2];
}

// Both shapes' end: +0xB = 0, +2 one on (so a steep slope moves +2 by two).
void BeginEnd() {
    Sprite_Current[0xB] = 0;
    ++Sprite_Current[2];
}

// PartyAction5_Form0Begin's code (0x51F210 is a copy of 0x51E930 instruction
// for instruction): the first turn by PartyAction_TargetAhead, then the slope
// one step ahead in +8 (MapView_SlopeAt with the direction byte) decides.
void Form0BeginRest1c() {
    FirstTurn(&TargetAhead);
    const unsigned char* const s = Sprite_Current;
    const unsigned d = s[8];
    const U x = static_cast<U>(rv::As<records::PartyMember>(s)->x) + StepXRest1c(d);
    const U z = static_cast<U>(rv::As<records::PartyMember>(s)->z) + StepZRest1c(d);
    const long slope = SH_CALL(MapView_SlopeAt)(static_cast<long>(x), static_cast<long>(z), d);
    if (Sloped() != 0 && static_cast<short>(slope) > 0x40) BeginSteep();
    else BeginLevel();
    BeginEnd();
}

// 0x51FD40 / 0x520400: the same, but steep is the ground one step ahead
// (MapView_GroundAt, its low word) above the height word +0x3E by more than
// 0x40 (s16) with the sloped flag set; MapView_SlopeAt is called after the
// ground, with the direction re-read, for its flag alone (its answer is
// dropped). Sprite_Current is re-read after the ground call.
void Form2Begin() {
    FirstTurn(&TargetAhead);
    const unsigned char* s = Sprite_Current;
    const unsigned d = s[8];
    const U x = static_cast<U>(rv::As<records::PartyMember>(s)->x) + StepXRest1c(d);
    const U z = static_cast<U>(rv::As<records::PartyMember>(s)->z) + StepZRest1c(d);
    const long ground = SH_CALL(MapView_GroundAt)(static_cast<long>(x), static_cast<long>(z));
    s = Sprite_Current;
    const auto rise = static_cast<short>(static_cast<std::uint16_t>(static_cast<U>(ground) - Word(s + 0x3E)));
    SH_CALL(MapView_SlopeAt)(static_cast<long>(x), static_cast<long>(z), s[8]);
    if (Sloped() != 0 && rise > 0x40) BeginSteep();
    else BeginLevel();
    BeginEnd();
}

// PartyAction5_Form0Resolve's code (0x51F3D0, 0x51FF20, 0x5205E0 differ from
// 0x51EAF0 only in the pickup they call): +0xA counted down; at 0 the point two
// steps ahead; the object there marked; the pickup on the point's cell, then
// on the cell one on in x (x with a fraction), then in z (z with one), each
// only while none found; +2 one on. Sprite_ScriptTickOnce every time. The
// originals pass the cells as dwords over their own uninitialised stack; each
// pickup reads 16 bits of them (docs/field_hidden.md section 3).
void Form0ResolveRest1c(CellFn pickup, const char* who) {
    --Sprite_Current[0xA];
    if (Sprite_Current[0xA] == 0) {
        const unsigned char* const s = Sprite_Current;
        const unsigned d = s[8];
        const U x = static_cast<U>(rv::As<records::PartyMember>(s)->x) + 2u * StepXRest1c(d);
        const U z = static_cast<U>(Long(s + 0x38)) + 2u * StepZRest1c(d);
        const unsigned char found = SH_CALL(Sprite_ObjectAt)(static_cast<long>(x), static_cast<long>(z), 0);
        if (found != 0xFF) MarkObjectRest1c(found, who);
        const unsigned cx = x >> 16, cz = z >> 16;
        if (pickup(cx, cz) == 0) {
            bool done = false;
            if ((x & 0xFFFF) != 0) done = pickup(cx + 1, cz) != 0;
            if (!done && (z & 0xFFFF) != 0) pickup(cx, cz + 1);
        }
        ++Sprite_Current[2];
    }
    SH_CALL(Sprite_ScriptTickOnce)();
}

// Field_CellPickup's code (0x51F4B0 is a copy of 0x51EBD0; 0x520000 and
// 0x5206C0 differ only in the tenfold, `mov cl, 0x14`): what lies in the map
// cell (x, z) - 0xF2 with an effect object free: Effect_SpawnAtCell(0, x, z),
// Rand & 0xF of 13, 14, 15 finds zenny 2 (5 on 15), `times` that when
// Field_InputFlags has bit 1 or 2 and a second Rand & 3 is 0 (a byte product):
// Field_GiveZenny, Effect_SpawnAtCell(1, x, z); +0xB = 1. 0xF8:
// Effect_SpawnAtCell(0, x, z), item 0x56's name into Text_Records, Inventory_Add
// (0, 0x56, 1): Sound_PlayEffect(0x106) and Msg_OpenSystem(2), else
// Msg_OpenSystem(3); Field_Request 2, +0xB = 1. Both clear the cell and answer
// 1; anything else answers 0.
unsigned char CellPickupRest1c(unsigned x, unsigned z, unsigned times) {
    const unsigned char cell = SH_CALL(AreaMap_ByteAt)(static_cast<short>(x), static_cast<short>(z));
    if (cell == 0xF2) {
        if (SH_CALL(Effect_FindFree)() != 0xFF) {
            SH_CALL(Effect_SpawnAtCell)(0, x, z);
            const unsigned roll = static_cast<unsigned char>(SH_CALL(Rand)()) & 0xFu;
            if (roll >= 0xD) {
                unsigned char amount = roll < 0xF ? 2 : 5;
                if ((Field_InputFlags & 6) != 0 && (static_cast<unsigned char>(SH_CALL(Rand)()) & 3) == 0)
                    amount = static_cast<unsigned char>(amount * times);
                SH_CALL(Field_GiveZenny)(amount);
                SH_CALL(Effect_SpawnAtCell)(1, x, z);
            }
            Sprite_Current[0xB] = 1;
        }
    } else if (cell == 0xF8) {
        SH_CALL(Effect_SpawnAtCell)(0, x, z);
        const unsigned char* const name = SH_CALL(Item_NamePtr)(0, 0x56);
        for (unsigned k = 0; k < 16; k += 4) SetLong(At(::bof3::Img(bof3::addr::Text_Records) + k), Long(name + k));
        if (SH_CALL(Inventory_Add)(0, 0x56, 1) != 0) {
            SH_CALL(Sound_PlayEffect)(0x106);
            SH_CALL(Msg_OpenSystem)(2);
        } else {
            SH_CALL(Msg_OpenSystem)(3);
        }
        Field_Request = 2;
        Sprite_Current[0xB] = 1;
    } else {
        return 0;
    }
    SH_CALL(AreaMap_ClearCell)(x, z);
    return 1;
}

// 0x51F670 / 0x520AA0: the first turn by PartyAction_BlockedAhead (turned back
// while the way is blocked), PartyAction_SideProbes, Sprite_EnsureAnimation(
// the turn + 0x42), +0xB = 0, +0xA = 0xB, +3 = 1.
void BlockedBegin() {
    FirstTurn(&BlockedAhead);
    SH_CALL(PartyAction_SideProbes)();
    SH_CALL(Sprite_EnsureAnimation)(Turned(Sprite_Current[8], 0x42));
    Sprite_Current[0xB] = 0;
    Sprite_Current[0xA] = 0xB;
    Sprite_Current[3] = 1;
}

// 0x51F700 / 0x520B30 (they differ only in the cell hit they call): +0xA
// counted down when not 0; at 0: Sound_PlayEffect(u16 +0x2C + 0x100); an
// effect object of kind 0x17 ahead (Field_EffectAhead) gets Sound_PlayEffect(
// 0x10B), its +8 = the direction, its +0xA = 1, the sprite's +6 = its index
// and +3 one on; with none, the point two steps ahead: the object there marked
// (with Sound_PlayEffect(0x10B)), then the cell hit on the point's cell and
// the cells one on in x / z across a fraction, as Form0Resolve's pickup. Then
// +3 one on (so an effect object found moves it by two). Sprite_ScriptTickOnce
// every time.
void BlockedResolve(CellFn hit, const char* who) {
    unsigned char* const s = Sprite_Current;
    if (s[0xA] != 0) {
        s[0xA] = static_cast<unsigned char>(s[0xA] - 1);
        if (Sprite_Current[0xA] == 0) {
            SH_CALL(Sound_PlayEffect)(static_cast<unsigned short>(Word(Sprite_Current + 0x2C) + 0x100));
            const unsigned char k = SH_CALL(Field_EffectAhead)();
            if (k != 0xFF) {
                SH_CALL(Sound_PlayEffect)(0x10B);
                unsigned char* const c = Sprite_Current;
                unsigned char* const e = EffectToWrite(static_cast<signed char>(k), who);
                e[8] = c[8];
                e[0xA] = 1;
                c[6] = k;
                ++Sprite_Current[3];
            } else {
                const unsigned char* const c = Sprite_Current;
                const unsigned d = c[8];
                const U x = static_cast<U>(rv::As<records::PartyMember>(c)->x) + 2u * StepXRest1c(d);
                const U z = static_cast<U>(Long(c + 0x38)) + 2u * StepZRest1c(d);
                const unsigned char found = SH_CALL(Sprite_ObjectAt)(static_cast<long>(x), static_cast<long>(z), 0);
                if (found != 0xFF) {
                    MarkObjectRest1c(found, who);
                    SH_CALL(Sound_PlayEffect)(0x10B);
                }
                const unsigned cx = x >> 16, cz = z >> 16;
                if (hit(cx, cz) == 0) {
                    bool done = false;
                    if ((x & 0xFFFF) != 0) done = hit(cx + 1, cz) != 0;
                    if (!done && (z & 0xFFFF) != 0) hit(cx, cz + 1);
                }
            }
            ++Sprite_Current[3];
        }
    }
    SH_CALL(Sprite_ScriptTickOnce)();
}

// 0x51F880 / 0x520C80 (identical but for their address): what lies in the map
// cell (x, z) - 0xF0, 0xF1, 0xF4: Effect_SpawnAtCellHigh(0, x, z), a second
// (state 4) when Rand & 7 is above 5, Sound_PlayEffect(0x10B); 0xF6, 0xF7:
// Effect_SpawnAtCellHigh(0, x, z), Sound_PlayEffect(0x10B), then by Rand & 0xF
// - below 7: Effect_SpawnAtCellHigh(3, x, z), item 0x29's name into
// Text_Records, Inventory_Add(0, 0x29, 1): Sound_PlayEffect(0x106) and
// Msg_OpenSystem(2), else Msg_OpenSystem(3); +0xB = 2 - 7..0xB: nothing more -
// above 0xB: Effect_SpawnAtCellHigh(2, x, z), Sprite_FlashClut(0),
// Char_LoseHp(1, Field_State +0x89), Msg_OpenSystem(0xD9), +0xB = 1; Field_Request
// 2 in all three. Each answers 1; anything else 0. The original stores the cell
// byte over its own z argument's low byte and pushes that dword as a fourth
// argument Effect_SpawnAtCellHigh does not read; no caller reads it again.
unsigned char CellHitRest1c(unsigned x, unsigned z) {
    const unsigned char cell = SH_CALL(AreaMap_ByteAt)(static_cast<short>(x), static_cast<short>(z));
    if (cell == 0xF0 || cell == 0xF1 || cell == 0xF4) {
        SH_CALL(Effect_SpawnAtCellHigh)(0, x, z);
        if ((static_cast<unsigned>(SH_CALL(Rand)()) & 7) > 5) SH_CALL(Effect_SpawnAtCellHigh)(4, x, z);
        SH_CALL(Sound_PlayEffect)(0x10B);
        return 1;
    }
    if (cell != 0xF6 && cell != 0xF7) return 0;
    SH_CALL(Effect_SpawnAtCellHigh)(0, x, z);
    SH_CALL(Sound_PlayEffect)(0x10B);
    const unsigned roll = static_cast<unsigned char>(SH_CALL(Rand)()) & 0xFu;
    if (roll < 7) {
        SH_CALL(Effect_SpawnAtCellHigh)(3, x, z);
        const unsigned char* const name = SH_CALL(Item_NamePtr)(0, 0x29);
        for (unsigned k = 0; k < 16; k += 4) SetLong(At(::bof3::Img(bof3::addr::Text_Records) + k), Long(name + k));
        if (SH_CALL(Inventory_Add)(0, 0x29, 1) != 0) {
            SH_CALL(Sound_PlayEffect)(0x106);
            SH_CALL(Msg_OpenSystem)(2);
        } else {
            SH_CALL(Msg_OpenSystem)(3);
        }
        Sprite_Current[0xB] = 2;
    } else if (roll > 0xB) {
        SH_CALL(Effect_SpawnAtCellHigh)(2, x, z);
        SH_CALL(Sprite_FlashClut)(0);
        SH_CALL(Char_LoseHp)(1, Field_State[0x89]);
        SH_CALL(Msg_OpenSystem)(0xD9);
        Sprite_Current[0xB] = 1;
    }
    Field_Request = 2;
    return 1;
}

// 0x51FAF0 / 0x5201A0 / 0x5208D0 (identical but for their address): the
// kind-0x30 effect object lined up ahead (PartyAction_Kind30Ahead). With one:
// when a member stands on it or beyond it (PartyAction_MemberOnEffect, then
// PartyAction_MemberBeyondEffect, its index as given), Field_State +0x137 = 0
// and no more; else its +0xB = 1, Sound_PlayEffect(u16 +0x2C + 0x100),
// Sprite_EnsureAnimation(+8 + 8, a byte), Field_State +0x128 = 2,
// Field_ScriptFlags |= 0x1000, Field_JumpStart, +9 one down,
// Field_LeaderStepTick, Field_State +0x137 = 1, +2 one on. With none: unless
// bit 0 of Field_State +0x138 is set, the object two steps ahead
// (Sprite_ObjectAt, margin 1) is marked; Field_State +0x137 = 0. Field_State is
// re-read after every call.
void Kind30Begin(const char* who) {
    const unsigned char k = SH_CALL(PartyAction_Kind30Ahead)();
    if (k != 0xFF) {
        if (SH_CALL(PartyAction_MemberOnEffect)(k) != 0 || SH_CALL(PartyAction_MemberBeyondEffect)(k) != 0) {
            Field_State[0x137] = 0;
            return;
        }
        const unsigned char* const s = Sprite_Current;
        EffectToWrite(k, who)[0xB] = 1;
        SH_CALL(Sound_PlayEffect)(static_cast<unsigned short>(Word(s + 0x2C) + 0x100));
        SH_CALL(Sprite_EnsureAnimation)(static_cast<unsigned char>(Sprite_Current[8] + 8));
        Field_State[0x128] = 2;
        At(::bof3::Img(kScriptFlagsHigh.v))[0] |= 0x10;
        SH_CALL(Field_JumpStart)();
        --Sprite_Current[9];
        SH_CALL(Field_LeaderStepTick)();
        Field_State[0x137] = 1;
        ++Sprite_Current[2];
        return;
    }
    const unsigned char* const s = Sprite_Current;
    const unsigned d = s[8];
    const U x = static_cast<U>(rv::As<records::PartyMember>(s)->x) + 2u * StepXRest1c(d);
    const U z = static_cast<U>(rv::As<records::PartyMember>(s)->z) + 2u * StepZRest1c(d);
    if ((Field_State[0x138] & 1) == 0) {
        const unsigned char found = SH_CALL(Sprite_ObjectAt)(static_cast<long>(x), static_cast<long>(z), 1);
        if (found != 0xFF) MarkObjectRest1c(found, who);
    }
    Field_State[0x137] = 0;
}

}  // namespace

#define R1C_EXPORT extern "C" __attribute__((disable_tail_calls))

namespace {

constexpr unsigned kSpriteStride = 0xA4;

void Jump(U table, unsigned count, unsigned index, const char* who) {
    if (index >= count)
        bof3::Fatal("%s: state %u is past its table's %u entries at 0x%X; the original jumps through 0x%X", who, index, count,
                    static_cast<unsigned>(::bof3::VaOf(table)), static_cast<unsigned>(::bof3::VaOf(table + 4 * index)));
    const auto entry = static_cast<U>(Long(At(table + 4 * index)));
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(entry))();
}

// Field_DirectionSteps' row for a direction, read in place with the byte
// unmasked (`shl eax, 3` on the zero-extended byte), as every reader of it in
// the originals does (docs/rest_0a.md section 5).
U StepXRest1d(unsigned d) { return static_cast<U>(Long(At(::bof3::Img(Field_DirectionSteps_addr) + d * 8))); }

U StepZRest1d(unsigned d) { return static_cast<U>(Long(At(::bof3::Img(Field_DirectionSteps_addr) + d * 8 + 4))); }

// The pose offset of a direction as the originals compute it: (d - 1) / 2 as a
// signed division (cdq, sub, sar), so direction 0 gives 0; then `add al, k`.
unsigned char Pose(unsigned char d, unsigned k) {
    return static_cast<unsigned char>((static_cast<int>(d) - 1) / 2 + static_cast<int>(k));
}

std::uint16_t SoundOfForm() { return static_cast<std::uint16_t>(Word(Sprite_Current + 0x2C) + 0x100); }

// An effect record by an index the original takes unchecked from a callee's
// answer or a sprite byte that no path in play makes 20 or more (section 5):
// ours aborts there.
unsigned char* EffectChecked(unsigned index, const char* who) {
    if (index >= 20)
        bof3::Fatal("%s: effect object index %u is past Effect_Objects' 20 records; the original writes at 0x%X", who, index,
                    static_cast<unsigned>(Effect_Objects_addr + index * kEffectStride));
    return rv::Bytes(Effect_Objects) + index * kEffectStride;
}

// Sprite_ObjectAt's object marked: +0x80 |= 1 of Sprite_Objects' record below
// 0x1E, else of Sprite_ObjectsExtra's (index - 0x1E). `is_signed`: the
// original compares the byte signed (movsx, jge), else unsigned (jae). Either
// way the answer is 0..0x21 (the 30 and the 4 records); ours aborts on any
// other, where the original would write before Sprite_Objects (signed) or past
// Sprite_ObjectsExtra's four.
void MarkObjectRest1d(unsigned char found, bool is_signed, const char* who) {
    if (found > 0x21)
        bof3::Fatal("%s: Sprite_ObjectAt answered %u, past the 34 sprite records; the original marks it unchecked (%s)", who, found,
                    is_signed ? "signed" : "unsigned");
    if (found < 0x1E) At(::bof3::Img(Sprite_Objects_addr) + 0x80 + found * kSpriteStride)[0] |= 1;
    else At(::bof3::Img(Sprite_ObjectsExtra_addr) + 0x80 + (found - 0x1E) * kSpriteStride)[0] |= 1;
}

// The point two steps ahead of Sprite_Current (x + 2 * step, z + 2 * step).
void TwoAheadRest1d(U& x, U& z) {
    const unsigned char* const s = Sprite_Current;
    const unsigned d = s[8];
    x = static_cast<U>(rv::As<records::PartyMember>(s)->x) + 2u * StepXRest1d(d);
    z = static_cast<U>(rv::As<records::PartyMember>(s)->z) + 2u * StepZRest1d(d);
}

// The cells around a point the resolves try in turn: the point's cell; when
// that answers 0 and x has a fraction, the cell one on in x; when that also
// answers 0 (or x had none) and z has a fraction, the cell one on in z. The
// cells are the point's high words; the originals pass them as dwords whose
// upper halves are uninitialised stack, and every callee reads 16 bits.
using CellProbe = unsigned char (__cdecl*)(unsigned, unsigned);

void TryCells(CellProbe probe, U x, U z) {
    const unsigned cx = x >> 16, cz = z >> 16;
    if (probe(cx, cz) != 0) return;
    if ((x & 0xFFFF) != 0 && probe(cx + 1, cz) != 0) return;
    if ((z & 0xFFFF) != 0) probe(cx, cz + 1);
}

// The inline side probe of 0x520F40's copies, direction 3 or 5: the point one
// step that way (Sprite_Current re-read); the slope there steep (the scratch
// flag set and its low word above 0x40, signed) and the ground there above the
// sprite's height word (both s16): +0x2B = 0. PartyAction_SideProbes' probe.
void SideProbeRest1d(unsigned d) {
    const unsigned char* s = Sprite_Current;
    const U x = static_cast<U>(rv::As<records::PartyMember>(s)->x) + StepXRest1d(d);
    const U z = static_cast<U>(rv::As<records::PartyMember>(s)->z) + StepZRest1d(d);
    const long slope = SH_CALL(MapView_SlopeAt)(static_cast<long>(x), static_cast<long>(z), d);
    if (SharedScratch.b_00 == 0 || static_cast<short>(slope) <= 0x40) return;
    const long ground = SH_CALL(MapView_GroundAt)(static_cast<long>(x), static_cast<long>(z));
    s = Sprite_Current;
    if (static_cast<short>(rv::As<records::PartyMember>(s)->height_whole) < static_cast<short>(ground)) Sprite_Current[0x2B] = 0;
}

// 0x520F40 / 0x521600 / 0x521CC0. An even direction is turned one eighth back;
// when PartyAction_TargetAhead finds nothing that way, two on, and when it
// finds nothing there either, back to the first turn. Then, one step ahead in
// +8 (read after the turns): the ground (MapView_GroundAt) less the height word
// +0x3E (re-read after the call), as a short, and MapView_SlopeAt there with
// the direction re-read after the ground; the scratch flag set and that rise
// above 0x40 - the pose (+8 - 1) / 2 + 0x46 and +2 one on. Otherwise +0x2B = 1,
// the side probes in directions 3 and 5, Sound_PlayEffect(+0x2C + 0x100), the
// pose + 0x42 and +0xA = 5. Then +0xB = 0 and +2 one on. The slope's answer
// itself is not read; the rise is the ground's, unlike PartyAction5_Form0Begin.
void FormBegin() {
    {
        unsigned char* const s = Sprite_Current;
        if ((s[8] & 1) == 0) {
            s[8] = static_cast<unsigned char>((s[8] - 1) & 7);
            if (SH_CALL(PartyAction_TargetAhead)() == 0) {
                Sprite_Current[8] = static_cast<unsigned char>((Sprite_Current[8] + 2) & 7);
                if (SH_CALL(PartyAction_TargetAhead)() == 0) Sprite_Current[8] = static_cast<unsigned char>((Sprite_Current[8] - 2) & 7);
            }
        }
    }
    const unsigned char* const s = Sprite_Current;
    const unsigned d = s[8];
    const U x = static_cast<U>(rv::As<records::PartyMember>(s)->x) + StepXRest1d(d);
    const U z = static_cast<U>(rv::As<records::PartyMember>(s)->z) + StepZRest1d(d);
    const long ground = SH_CALL(MapView_GroundAt)(static_cast<long>(x), static_cast<long>(z));
    const auto rise = static_cast<short>(static_cast<std::uint16_t>(static_cast<U>(ground) - Word(Sprite_Current + 0x3E)));
    const unsigned direction = Sprite_Current[8];
    SH_CALL(MapView_SlopeAt)(static_cast<long>(x), static_cast<long>(z), direction);
    if (SharedScratch.b_00 != 0 && rise > 0x40) {
        SH_CALL(Sprite_EnsureAnimation)(Pose(Sprite_Current[8], 0x46));
        ++Sprite_Current[2];
    } else {
        Sprite_Current[0x2B] = 1;
        SideProbeRest1d(3);
        SideProbeRest1d(5);
        SH_CALL(Sound_PlayEffect)(SoundOfForm());
        SH_CALL(Sprite_EnsureAnimation)(Pose(Sprite_Current[8], 0x42));
        Sprite_Current[0xA] = 5;
    }
    Sprite_Current[0xB] = 0;
    ++Sprite_Current[2];
}

// 0x521120 / 0x5217E0 / 0x521EA0: PartyAction5_Form0Resolve's code (instruction
// for instruction but the pickup it calls). +0xA one down; at 0: the point two
// steps ahead; the object Sprite_ObjectAt(point, 0) finds gets +0x80 bit 0 (the
// byte compared signed); the cells tried with the set's pickup; +2 one on.
// Sprite_ScriptTickOnce every time.
void FormResolve(CellProbe pickup, const char* who) {
    --Sprite_Current[0xA];
    if (Sprite_Current[0xA] == 0) {
        U x, z;
        TwoAheadRest1d(x, z);
        const unsigned char found = SH_CALL(Sprite_ObjectAt)(static_cast<long>(x), static_cast<long>(z), 0);
        if (found != 0xFF) MarkObjectRest1d(found, true, who);
        TryCells(pickup, x, z);
        ++Sprite_Current[2];
    }
    SH_CALL(Sprite_ScriptTickOnce)();
}

// 0x521200 / 0x5218C0 / 0x521F80: Field_CellPickup's code with the bonus's
// multiplier 0x14 (imul cl; Field_CellPickup's is 0xA). AreaMap_ByteAt(x, z):
//   0xF2 - with an effect object free (Effect_FindFree): Effect_SpawnAtCell(0,
//          x, z); Rand & 0xF at least 0xD finds zenny, 2 (5 on 0xF), twenty
//          times that when Field_InputFlags (read after the draw) has bit 1 or
//          2 and a second Rand & 3 is 0: Field_GiveZenny, Effect_SpawnAtCell(1,
//          x, z); then +0xB = 1. With no object free, none of that.
//   0xF8 - Effect_SpawnAtCell(0, x, z); item 0x56's name (Item_NamePtr(0,
//          0x56), 16 bytes) into Text_Records; Inventory_Add(0, 0x56, 1): taken,
//          Sound_PlayEffect(0x106) and Msg_OpenSystem(2), else
//          Msg_OpenSystem(3); Field_Request = 2, +0xB = 1.
// Both clear the cell (AreaMap_ClearCell) and answer 1; anything else 0.
// x and z go on as the dwords given (every callee reads their low words).
unsigned char CellPickupRest1d(unsigned x, unsigned z) {
    const unsigned char cell = SH_CALL(AreaMap_ByteAt)(static_cast<short>(x), static_cast<short>(z));
    if (cell == 0xF2) {
        if (SH_CALL(Effect_FindFree)() != 0xFF) {
            SH_CALL(Effect_SpawnAtCell)(0, x, z);
            const unsigned roll = static_cast<unsigned>(SH_CALL(Rand)()) & 0xF;
            if (roll >= 0xD) {
                unsigned char amount = roll < 0xF ? 2 : 5;
                if ((Field_InputFlags & 6) != 0 && (static_cast<unsigned>(SH_CALL(Rand)()) & 3) == 0)
                    amount = static_cast<unsigned char>(amount * 0x14);
                SH_CALL(Field_GiveZenny)(amount);
                SH_CALL(Effect_SpawnAtCell)(1, x, z);
            }
            Sprite_Current[0xB] = 1;
        }
    } else if (cell == 0xF8) {
        SH_CALL(Effect_SpawnAtCell)(0, x, z);
        const unsigned char* const name = SH_CALL(Item_NamePtr)(0, 0x56);
        for (unsigned k = 0; k < 16; k += 4) move_script::SetLong(At(::bof3::Img(bof3::addr::Text_Records) + k), Long(name + k));
        if (SH_CALL(Inventory_Add)(0, 0x56, 1) != 0) {
            SH_CALL(Sound_PlayEffect)(0x106);
            SH_CALL(Msg_OpenSystem)(2);
        } else {
            SH_CALL(Msg_OpenSystem)(3);
        }
        Field_Request = 2;
        Sprite_Current[0xB] = 1;
    } else {
        return 0;
    }
    SH_CALL(AreaMap_ClearCell)(x, z);
    return 1;
}

// 0x5213A0 / 0x521AD0. The kind-0x30 effect object lined up one step ahead
// (PartyAction_Kind30Ahead): when PartyAction_MemberOnEffect and then
// PartyAction_MemberBeyondEffect both answer 0 for it - its +0xB = 1,
// Sound_PlayEffect(+0x2C + 0x100), the pose +8 + 8, Field_State +0x128 = 2,
// Field_ScriptFlags' bit 0x1000 set (its high byte | 0x10), Field_JumpStart,
// +9 one down, Field_LeaderStepTick, Field_State +0x137 = 1 and +2 one on; when
// either answers not 0, Field_State +0x137 = 0. With none lined up: the point
// two steps ahead; unless Field_State +0x138 has bit 0, the object
// Sprite_ObjectAt(point, margin 1) finds gets +0x80 bit 0 (the byte compared
// unsigned); Field_State +0x137 = 0.
void Kind30BeginRest1d(const char* who) {
    const unsigned char k = SH_CALL(PartyAction_Kind30Ahead)();
    if (k != 0xFF) {
        if (SH_CALL(PartyAction_MemberOnEffect)(k) != 0 || SH_CALL(PartyAction_MemberBeyondEffect)(k) != 0) {
            Field_State[0x137] = 0;
            return;
        }
        EffectChecked(k, who)[0xB] = 1;
        SH_CALL(Sound_PlayEffect)(SoundOfForm());
        SH_CALL(Sprite_EnsureAnimation)(static_cast<unsigned char>(Sprite_Current[8] + 8));
        Field_State[0x128] = 2;
        At(::bof3::Img(rest_1d::at::kScriptFlagsHi.v))[0] |= 0x10;
        SH_CALL(Field_JumpStart)();
        --Sprite_Current[9];
        SH_CALL(Field_LeaderStepTick)();
        Field_State[0x137] = 1;
        ++Sprite_Current[2];
        return;
    }
    U x, z;
    TwoAheadRest1d(x, z);
    if ((Field_State[0x138] & 1) == 0) {
        const unsigned char found = SH_CALL(Sprite_ObjectAt)(static_cast<long>(x), static_cast<long>(z), 1);
        if (found != 0xFF) MarkObjectRest1d(found, false, who);
    }
    Field_State[0x137] = 0;
}

using HandlerRest1e = scenario_harness::Handler;

using CellHelper = unsigned char(__cdecl*)(unsigned, unsigned);

unsigned char AlRest1e(U eax) { return static_cast<unsigned char>(eax); }

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + k] (or mov ax, [ecx +
// 0x2C]); jmp [table + eax * 4]: the entry read in place (the fuzz swaps the
// cells for recorders). No compare bounds the index; ours aborts past the
// table's own length, where the original jumps through the next table's dwords.
void Dispatch(const char* who, const char* by, U table, unsigned index, unsigned entries) {
    if (index >= entries)
        bof3::Fatal("%s: %s is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/rest_1e.md section 5)",
                    who, by, index, entries, static_cast<unsigned>(::bof3::VaOf(table)));
    reinterpret_cast<HandlerRest1e>(static_cast<std::uintptr_t>(static_cast<U>(Long(At(table + 4 * index)))))();
}

// A state table's address (symbols.toml [[data]]: the name is a pointer to its
// first dword).
U Table(const unsigned long* t) { return Low32(t); }

void ByState2(const char* who, U table, unsigned entries) { Dispatch(who, "the state byte +2", table, S()[2], entries); }

void ByState3(const char* who, U table, unsigned entries) { Dispatch(who, "the state byte +3", table, S()[3], entries); }

void ByFormRest1e(const char* who, U table) { Dispatch(who, "the form word +0x2C", table, rv::As<records::PartyMember>(S())->column, 3); }

// Field_DirectionSteps' row for a direction, read in place with the direction
// unmasked, as the originals index it.
U StepXRest1e(unsigned d) { return static_cast<U>(Long(At(::bof3::Img(Field_DirectionSteps_addr) + d * 8))); }

U StepZRest1e(unsigned d) { return static_cast<U>(Long(At(::bof3::Img(Field_DirectionSteps_addr) + d * 8 + 4))); }

// The animation offset of a direction as the originals compute it: dec eax;
// cdq; sub eax, edx; sar eax, 1 on the zero-extended byte - (d - 1) / 2
// truncated, so 0 gives 0; the add to al never carries (0..0x7F + 0x46).
unsigned HalfTurnRest1e(unsigned char d) { return static_cast<unsigned>((static_cast<int>(d) - 1) / 2); }

// An even direction turned one eighth back, then two on, then back again,
// while `probe` (PartyAction_TargetAhead or PartyAction_BlockedAhead) answers
// 0 - each turn on Sprite_Current re-read after the call. An odd direction is
// kept, unmasked.
void TurnWhileClear(unsigned char (__cdecl* probe)(void)) {
    unsigned char* const s = S();
    if ((s[8] & 1) != 0) return;
    s[8] = static_cast<unsigned char>((s[8] - 1) & 7);
    if (AlRest1e(SH_CALL(probe)()) != 0) return;
    S()[8] = static_cast<unsigned char>((S()[8] + 2) & 7);
    if (AlRest1e(SH_CALL(probe)()) != 0) return;
    S()[8] = static_cast<unsigned char>((S()[8] - 2) & 7);
}

// A side probe in direction d (3 or 5, the immediate the originals push; the
// row read by its address): the point one step that way from Sprite_Current;
// when MapView_SlopeAt there is steep (the scratch flag set and its low word,
// signed, above 0x40) and MapView_GroundAt there is above the height word
// (s16 against s16), +0x2B = 0. PartyAction_SideProbes' probe, inline.
void SideProbeRest1e(unsigned d, U row) {
    const unsigned char* const s = S();
    const U x = static_cast<U>(rv::As<records::PartyMember>(s)->x) + static_cast<U>(Long(At(row)));
    const U z = static_cast<U>(rv::As<records::PartyMember>(s)->z) + static_cast<U>(Long(At(row + 4)));
    const long slope = SH_CALL(MapView_SlopeAt)(static_cast<long>(x), static_cast<long>(z), d);
    if (SharedScratch.b_00 == 0 || static_cast<short>(slope) <= 0x40) return;
    const long ground = SH_CALL(MapView_GroundAt)(static_cast<long>(x), static_cast<long>(z));
    unsigned char* const c = S();
    if (static_cast<short>(rv::As<records::PartyMember>(c)->height_whole) < static_cast<short>(ground)) c[0x2B] = 0;
}

void SideProbes() {
    S()[0x2B] = 1;
    SideProbeRest1e(3, ::bof3::Img(Field_DirectionSteps_addr) + 0x18);
    SideProbeRest1e(5, ::bof3::Img(Field_DirectionSteps_addr) + 0x28);
}

// Sprite_ObjectAt's index, as the originals use it: bit 0 of the object's
// +0x80 - 0..0x1D a Sprite_Objects record, 0x1E and up (cmp al, 0x1E; jge, the
// byte signed) Sprite_ObjectsExtra's. The real one answers 0..0x21 or 0xFF; ours
// aborts on any other byte, which the original would use to write outside the
// records.
void FlagObject(const char* who, unsigned char found) {
    const int i = static_cast<signed char>(found);
    if (i < 0 || i >= static_cast<int>(rest_1e::at::kObjectCount + rest_1e::at::kExtraCount))
        bof3::Fatal("%s: Sprite_ObjectAt answered 0x%02X, past the 0x22 sprite records; the original sets bit 0 of a "
                    "byte outside them (docs/rest_1e.md section 5)",
                    who, static_cast<unsigned>(found));
    if (i < static_cast<int>(rest_1e::at::kObjectCount))
        At(::bof3::Img(Sprite_Objects_addr) + 0x80 + static_cast<U>(i) * rest_1e::at::kObjectStride)[0] |= 1;
    else
        At(::bof3::Img(Sprite_ObjectsExtra_addr) + 0x80 + static_cast<U>(i - static_cast<int>(rest_1e::at::kObjectCount)) * rest_1e::at::kObjectStride)[0] |= 1;
}

// An Effect_Objects record by an index the original uses unchecked; ours aborts
// past the 20 records.
unsigned char* EffectAtRest1e(const char* who, int index, const char* what) {
    if (index < 0 || index >= static_cast<int>(kEffectCount))
        bof3::Fatal("%s: %s is %d, past Effect_Objects' 20 records; the original reads 0x%X unchecked "
                    "(docs/rest_1e.md section 5)",
                    who, what, index, static_cast<unsigned>(0x7E11E0 + index * static_cast<int>(kEffectStride)));
    return rv::Bytes(Effect_Objects) + index * static_cast<int>(kEffectStride);
}

// The point two steps ahead of Sprite_Current (read now), as the resolve and
// strike states compute it (lea r, [pos + step * 2]).
void TwoAheadRest1e(U& x, U& z) {
    const unsigned char* const s = S();
    const unsigned d = s[8];
    x = static_cast<U>(rv::As<records::PartyMember>(s)->x) + 2u * StepXRest1e(d);
    z = static_cast<U>(rv::As<records::PartyMember>(s)->z) + 2u * StepZRest1e(d);
}

// The cell calls after a probe two steps ahead: on the point's cell; when that
// answers 0 and x has a fraction, on the cell one on in x; when still 0 and z
// has one, on the cell one on in z. The originals pass dwords whose low words
// are the cells and whose upper halves are their own uninitialised stack;
// every callee reads the low word only (docs/rest_1e.md section 2).
void CellsAhead(CellHelper helper, U x, U z) {
    const unsigned cx = x >> 16, cz = z >> 16;
    if (AlRest1e(SH_CALL(helper)(cx, cz)) != 0) return;
    if ((x & 0xFFFF) != 0 && AlRest1e(SH_CALL(helper)(cx + 1, cz)) != 0) return;
    if ((z & 0xFFFF) != 0) SH_CALL(helper)(cx, cz + 1);
}

// 0x522760 / 0x5230D0 / 0x523550.
void Form2BeginRest1e() {
    TurnWhileClear(&PartyAction_TargetAhead);
    const unsigned char* s = S();
    const unsigned d = s[8];
    const U x = static_cast<U>(rv::As<records::PartyMember>(s)->x) + StepXRest1e(d);
    const U z = static_cast<U>(rv::As<records::PartyMember>(s)->z) + StepZRest1e(d);
    const long ground = SH_CALL(MapView_GroundAt)(static_cast<long>(x), static_cast<long>(z));
    s = S();
    const auto rise = static_cast<short>(static_cast<std::uint16_t>(static_cast<U>(ground) - Word(s + 0x3E)));
    // push eax with al the direction and the rest of eax Sprite_Current itself.
    // The upper bits pass through an empty asm: without it this toolchain's
    // x86 back end dropped them (the IR keeps the `or`; the code pushed the
    // byte alone - 12,000 mismatches, docs/rest_1e.md section 4).
    U upper = Low32(s) & 0xFFFFFF00u;
    asm volatile("" : "+r"(upper));
    const U direction = upper | s[8];
    SH_CALL(MapView_SlopeAt)(static_cast<long>(x), static_cast<long>(z), direction);   // its answer unused
    if (SharedScratch.b_00 != 0 && rise > 0x40) {
        SH_CALL(Sprite_EnsureAnimation)(static_cast<unsigned char>(HalfTurnRest1e(S()[8]) + 0x46));
        ++S()[2];
    } else {
        SideProbes();
        SH_CALL(Sound_PlayEffect)(static_cast<unsigned short>(Word(S() + 0x2C) + 0x100));
        SH_CALL(Sprite_EnsureAnimation)(static_cast<unsigned char>(HalfTurnRest1e(S()[8]) + 0x42));
        S()[0xA] = 5;
    }
    S()[0xB] = 0;
    ++S()[2];
}

// 0x522940 / 0x5232B0 / 0x523730: PartyAction5_Form0Resolve's code calling the
// set's own cell pickup.
void Form2Resolve(const char* who, CellHelper pickup) {
    --S()[0xA];
    if (S()[0xA] == 0) {
        U x, z;
        TwoAheadRest1e(x, z);
        const unsigned char found = AlRest1e(SH_CALL(Sprite_ObjectAt)(static_cast<long>(x), static_cast<long>(z), 0));
        if (found != 0xFF) FlagObject(who, found);
        CellsAhead(pickup, x, z);
        ++S()[2];
    }
    SH_CALL(Sprite_ScriptTickOnce)();
}

// 0x522A20 / 0x523390 / 0x523810: Field_CellPickup's code, the bonus x 20.
unsigned char CellPickupRest1e(unsigned x, unsigned z) {
    const unsigned char cell = AlRest1e(SH_CALL(AreaMap_ByteAt)(static_cast<short>(x), static_cast<short>(z)));
    if (cell == 0xF2) {
        if (AlRest1e(SH_CALL(Effect_FindFree)()) != 0xFF) {
            SH_CALL(Effect_SpawnAtCell)(0, x, z);
            const unsigned roll = AlRest1e(static_cast<U>(SH_CALL(Rand)())) & 0xF;
            if (roll >= 0xD) {
                unsigned char amount = roll < 0xF ? 2 : 5;
                if ((Field_InputFlags & 6) != 0 && (AlRest1e(static_cast<U>(SH_CALL(Rand)())) & 3) == 0)
                    amount = static_cast<unsigned char>(amount * 20);
                SH_CALL(Field_GiveZenny)(amount);
                SH_CALL(Effect_SpawnAtCell)(1, x, z);
            }
            S()[0xB] = 1;
        }
    } else if (cell == 0xF8) {
        SH_CALL(Effect_SpawnAtCell)(0, x, z);
        const unsigned char* const name = SH_CALL(Item_NamePtr)(0, rest_1e::at::kPickupItem);
        for (unsigned k = 0; k < 16; k += 4) SetLong(At(::bof3::Img(bof3::addr::Text_Records) + k), Long(name + k));
        if (AlRest1e(SH_CALL(Inventory_Add)(0, rest_1e::at::kPickupItem, 1)) != 0) {
            SH_CALL(Sound_PlayEffect)(rest_1e::at::kSoundTaken);
            SH_CALL(Msg_OpenSystem)(2);
        } else {
            SH_CALL(Msg_OpenSystem)(3);
        }
        Field_Request = 2;
        S()[0xB] = 1;
    } else {
        return 0;
    }
    SH_CALL(AreaMap_ClearCell)(x, z);
    return 1;
}

// 0x522C00 / 0x523B40.
void StrikeBegin() {
    TurnWhileClear(&PartyAction_BlockedAhead);
    SH_CALL(PartyAction_SideProbes)();
    SH_CALL(Sprite_EnsureAnimation)(static_cast<unsigned char>(HalfTurnRest1e(S()[8]) + 0x42));
    S()[0xB] = 0;
    S()[0xA] = 0xB;
    S()[3] = 1;
}

// 0x522C90 / 0x523BD0.
void Strike(const char* who, CellHelper strike) {
    const unsigned char t = S()[0xA];
    if (t != 0) {
        S()[0xA] = static_cast<unsigned char>(t - 1);
        if (S()[0xA] == 0) {
            SH_CALL(Sound_PlayEffect)(static_cast<unsigned short>(Word(S() + 0x2C) + 0x100));
            const unsigned char ahead = AlRest1e(SH_CALL(Field_EffectAhead)());
            if (ahead != 0xFF) {
                SH_CALL(Sound_PlayEffect)(rest_1e::at::kSoundStrike);
                unsigned char* const s = S();
                unsigned char* const e = EffectAtRest1e(who, static_cast<signed char>(ahead), "Field_EffectAhead's answer");
                e[8] = s[8];
                e[0xA] = 1;
                s[6] = ahead;
                ++S()[3];
            } else {
                U x, z;
                TwoAheadRest1e(x, z);
                const unsigned char found = AlRest1e(SH_CALL(Sprite_ObjectAt)(static_cast<long>(x), static_cast<long>(z), 0));
                if (found != 0xFF) {
                    FlagObject(who, found);
                    SH_CALL(Sound_PlayEffect)(rest_1e::at::kSoundStrike);
                }
                CellsAhead(strike, x, z);
            }
            ++S()[3];
        }
    }
    SH_CALL(Sprite_ScriptTickOnce)();
}

// 0x522E20 / 0x523D20. Effect_SpawnAtCellHigh is pushed a fourth dword (the
// z argument's slot after the cell byte was stored into its low byte), which it
// never reads; ours passes the three it reads.
unsigned char StrikeCell(unsigned x, unsigned z) {
    const unsigned char cell = AlRest1e(SH_CALL(AreaMap_ByteAt)(static_cast<short>(x), static_cast<short>(z)));
    if (cell == 0xF0 || cell == 0xF1 || cell == 0xF4) {
        SH_CALL(Effect_SpawnAtCellHigh)(0, x, z);
        if (static_cast<signed char>(AlRest1e(static_cast<U>(SH_CALL(Rand)())) & 7) > 5) SH_CALL(Effect_SpawnAtCellHigh)(4, x, z);
        SH_CALL(Sound_PlayEffect)(rest_1e::at::kSoundStrike);
        return 1;
    }
    if (cell != 0xF6 && cell != 0xF7) return 0;
    SH_CALL(Effect_SpawnAtCellHigh)(0, x, z);
    SH_CALL(Sound_PlayEffect)(rest_1e::at::kSoundStrike);
    const unsigned roll = AlRest1e(static_cast<U>(SH_CALL(Rand)())) & 0xF;
    if (roll < 7) {
        SH_CALL(Effect_SpawnAtCellHigh)(3, x, z);
        const unsigned char* const name = SH_CALL(Item_NamePtr)(0, rest_1e::at::kStrikeItem);
        for (unsigned k = 0; k < 16; k += 4) SetLong(At(::bof3::Img(bof3::addr::Text_Records) + k), Long(name + k));
        if (AlRest1e(SH_CALL(Inventory_Add)(0, rest_1e::at::kStrikeItem, 1)) != 0) {
            SH_CALL(Sound_PlayEffect)(rest_1e::at::kSoundTaken);
            SH_CALL(Msg_OpenSystem)(2);
        } else {
            SH_CALL(Msg_OpenSystem)(3);
        }
        S()[0xB] = 2;
    } else if (roll > 0xB) {
        SH_CALL(Effect_SpawnAtCellHigh)(2, x, z);
        SH_CALL(Sprite_FlashClut)(0);
        const unsigned member = Field_State[0x89];
        SH_CALL(Char_LoseHp)(1, member);
        SH_CALL(Msg_OpenSystem)(rest_1e::at::kStrikeHurtMessage);
        S()[0xB] = 1;
    }
    Field_Request = 2;
    return 1;
}

using Pickup = unsigned char (__cdecl*)(unsigned, unsigned);

constexpr unsigned kObjectCount = 0x1E;   // Sprite_Objects; 0x1E.. Sprite_ObjectsExtra's four

// kEffectsAt: now Effect_Objects (symbols.toml). Effect_Objects (its name is a macro here)
// kObjectsAt: now Sprite_Objects (symbols.toml). Sprite_Objects
// kExtraAt: now Sprite_ObjectsExtra (symbols.toml). Sprite_ObjectsExtra
constexpr U kEffectStrideRest1f = 0x80;

// A Field_DirectionSteps row, read in place with the direction unmasked (the
// originals' `shl eax, 3` on the zero-extended byte): a byte above 7 reads the
// .data after the table, as they do.
U StepXRest1f(unsigned d) { return static_cast<U>(Long(At(::bof3::Img(Field_DirectionSteps_addr) + d * 8))); }

U StepZRest1f(unsigned d) { return static_cast<U>(Long(At(::bof3::Img(Field_DirectionSteps_addr) + d * 8 + 4))); }

std::uint16_t W16(unsigned v) { return static_cast<std::uint16_t>(v); }

// A dispatcher: the table's entry by `index`, called as the original jumps
// to it - a Fatal past the table's own entries, where the original jumps
// through the dword after (docs/rest_1f.md section 7).
void DispatchRest1f(const char* who, const char* by, const unsigned long* table, unsigned count, unsigned index) {
    if (index >= count)
        bof3::Fatal("%s: %s is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/rest_1f.md section 7)",
                    who, by, index, count, static_cast<unsigned>(::bof3::VaOf(Low32(table))));
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(table[index]))();
}

void ByStateRest1f(const char* who, const unsigned long* table, unsigned count) {
    DispatchRest1f(who, "Sprite_Current +2", table, count, Sc()[2]);
}

void ByStepRest1f(const char* who, const unsigned long* table, unsigned count) {
    DispatchRest1f(who, "Sprite_Current +3", table, count, Sc()[3]);
}

void ByFormRest1f(const char* who, const unsigned long* table, unsigned count) {
    DispatchRest1f(who, "Sprite_Current's word +0x2C", table, count, rv::As<records::PartyMember>(Sc())->column);
}

// An Effect_Objects record by an index byte the original uses unchecked
// (`shl reg, 7`): a Fatal past the 20 records.
unsigned char* EffectAtRest1f(unsigned index, const char* who) {
    if (index >= kEffectCount)
        bof3::Fatal("%s: effect object index %u is past Effect_Objects' 20 records; the original writes 0x%X "
                    "(docs/rest_1f.md section 7)",
                    who, index, static_cast<unsigned>(Effect_Objects_addr + index * kEffectStrideRest1f));
    return rv::Bytes(Effect_Objects) + index * kEffectStrideRest1f;
}

// The object Sprite_ObjectAt found gets bit 0 of its +0x80: 0..0x1D a
// Sprite_Objects record, 0x1E.. one of Sprite_ObjectsExtra's four (`cmp al,
// 0x1E; movsx eax, al; jge`, a signed byte). Sprite_ObjectAt answers 0..0x21
// or 0xFF; any other byte - a negative one indexes before Sprite_Objects -
// aborts here.
void MarkObjectRest1f(unsigned char found, const char* who) {
    const int i = static_cast<signed char>(found);
    if (i >= 0 && i < static_cast<int>(kObjectCount))
        Sprite_Objects[static_cast<unsigned>(i)].context |= 1;
    else if (i >= static_cast<int>(kObjectCount) && i < static_cast<int>(kObjectCount) + 4)
        Sprite_ObjectsExtra[static_cast<unsigned>(i - static_cast<int>(kObjectCount))].context |= 1;
    else
        bof3::Fatal("%s: Sprite_ObjectAt's answer 0x%02X is past Sprite_Objects' 30 and Sprite_ObjectsExtra's 4; the "
                    "original sets a bit at 0x%X (docs/rest_1f.md section 7)",
                    who, found,
                    static_cast<unsigned>(i < static_cast<int>(kObjectCount)
                                              ? Sprite_Objects_addr + 0x80u + static_cast<U>(i * static_cast<int>(kObjectStride))
                                              : Sprite_ObjectsExtra_addr + 0x80u +
                                                    static_cast<U>((i - static_cast<int>(kObjectCount)) * static_cast<int>(kObjectStride))));
}

// The 16 bytes of an item's name copied to Text_Records, as four dwords.
void CopyName(const unsigned char* name) {
    for (unsigned k = 0; k < 16; k += 4) SetLong(At(::bof3::Img(bof3::addr::Text_Records) + k), Long(name + k));
}

// The side probe the Begin states make inline (PartyAction_SideProbes' probe,
// not a call to it): the point one Field_DirectionSteps row `d` (3 or 5, the
// rows read by address) from Sprite_Current; the slope there steep (the
// scratch flag set and its low word above 0x40, signed) and the ground there
// above Sprite_Current's height word (s16 against s16): +0x2B = 0.
void SideProbeRest1f(unsigned d) {
    const unsigned char* const s = Sc();
    const U x = static_cast<U>(rv::As<records::PartyMember>(s)->x) + StepXRest1f(d);
    const U z = static_cast<U>(rv::As<records::PartyMember>(s)->z) + StepZRest1f(d);
    const long slope = SH_CALL(MapView_SlopeAt)(static_cast<long>(x), static_cast<long>(z), d);
    if (SharedScratch.b_00 == 0 || static_cast<short>(slope) <= 0x40) return;
    const long ground = SH_CALL(MapView_GroundAt)(static_cast<long>(x), static_cast<long>(z));
    if (static_cast<short>(rv::As<records::PartyMember>(Sc())->height_whole) < static_cast<short>(ground)) Sc()[0x2B] = 0;
}

// 0x523FF0 / 0x524490 / 0x524E90 (byte-identical): a form's state 0.
void BeginShape() {
    unsigned char* s = Sc();
    const unsigned char d0 = s[8];
    if ((d0 & 1) == 0) {
        s[8] = static_cast<unsigned char>((d0 - 1) & 7);
        if (SH_CALL(PartyAction_TargetAhead)() == 0) {
            Sc()[8] = static_cast<unsigned char>((Sc()[8] + 2) & 7);
            if (SH_CALL(PartyAction_TargetAhead)() == 0) Sc()[8] = static_cast<unsigned char>((Sc()[8] - 2) & 7);
        }
        s = Sc();
    }
    const unsigned d = s[8];
    const U x = static_cast<U>(rv::As<records::PartyMember>(s)->x) + StepXRest1f(d);
    const U z = static_cast<U>(rv::As<records::PartyMember>(s)->z) + StepZRest1f(d);
    const long ground = SH_CALL(MapView_GroundAt)(static_cast<long>(x), static_cast<long>(z));
    const auto rise = static_cast<short>(static_cast<std::uint16_t>(static_cast<U>(ground) - Word(Sc() + 0x3E)));
    SH_CALL(MapView_SlopeAt)(static_cast<long>(x), static_cast<long>(z), Sc()[8]);
    if (SharedScratch.b_00 != 0 && rise > 0x40) {
        SH_CALL(Sprite_EnsureAnimation)(static_cast<unsigned char>(HalfTurn(Sc()[8]) + 0x46));
        ++Sc()[2];
    } else {
        Sc()[0x2B] = 1;
        SideProbeRest1f(3);
        SideProbeRest1f(5);
        SH_CALL(Sound_PlayEffect)(W16(Word(Sc() + 0x2C) + 0x100));
        SH_CALL(Sprite_EnsureAnimation)(static_cast<unsigned char>(HalfTurn(Sc()[8]) + 0x42));
        Sc()[0xA] = 5;
    }
    Sc()[0xB] = 0;
    ++Sc()[2];
}

// 0x5241D0 / 0x524670 / 0x525070 (byte-identical but for the pickup they
// call): a form's state 1, PartyAction5_Form0Resolve's code with its own
// pickup.
void ResolveShape(Pickup pickup, const char* who) {
    --Sc()[0xA];
    if (Sc()[0xA] == 0) {
        const unsigned char* const s = Sc();
        const unsigned d = s[8];
        const U x = static_cast<U>(rv::As<records::PartyMember>(s)->x) + 2u * StepXRest1f(d);
        const U z = static_cast<U>(Long(s + 0x38)) + 2u * StepZRest1f(d);
        const unsigned char found = SH_CALL(Sprite_ObjectAt)(static_cast<long>(x), static_cast<long>(z), 0);
        if (found != 0xFF) MarkObjectRest1f(found, who);
        const unsigned cx = x >> 16, cz = z >> 16;
        if (scenario_harness::Call(pickup)(cx, cz) == 0) {
            bool done = false;
            if ((x & 0xFFFF) != 0) done = scenario_harness::Call(pickup)(cx + 1, cz) != 0;
            if (!done && (z & 0xFFFF) != 0) scenario_harness::Call(pickup)(cx, cz + 1);
        }
        ++Sc()[2];
    }
    SH_CALL(Sprite_ScriptTickOnce)();
}

// 0x5242B0 / 0x524750 / 0x525150 (byte-identical): Field_CellPickup's code
// with one constant changed - the found zenny times 20, not 10.
unsigned char PickupShape(unsigned x, unsigned z) {
    const unsigned char cell = SH_CALL(AreaMap_ByteAt)(static_cast<short>(x), static_cast<short>(z));
    if (cell == 0xF2) {
        if (SH_CALL(Effect_FindFree)() != 0xFF) {
            SH_CALL(Effect_SpawnAtCell)(0, x, z);
            const unsigned roll = static_cast<unsigned>(SH_CALL(Rand)()) & 0xF;
            if (roll >= 0xD) {
                unsigned char amount = roll < 0xF ? 2 : 5;
                if ((Field_InputFlags & 6) != 0 && (static_cast<unsigned>(SH_CALL(Rand)()) & 3) == 0)
                    amount = static_cast<unsigned char>(amount * 20);
                SH_CALL(Field_GiveZenny)(amount);
                SH_CALL(Effect_SpawnAtCell)(1, x, z);
            }
            Sc()[0xB] = 1;
        }
    } else if (cell == 0xF8) {
        SH_CALL(Effect_SpawnAtCell)(0, x, z);
        const unsigned char* const name = SH_CALL(Item_NamePtr)(0, 0x56);
        CopyName(name);
        if (SH_CALL(Inventory_Add)(0, 0x56, 1) != 0) {
            SH_CALL(Sound_PlayEffect)(0x106);
            SH_CALL(Msg_OpenSystem)(2);
        } else {
            SH_CALL(Msg_OpenSystem)(3);
        }
        Field_Request = 2;
        Sc()[0xB] = 1;
    } else {
        return 0;
    }
    SH_CALL(AreaMap_ClearCell)(x, z);
    return 1;
}

// 0x523ED0 / 0x524D60 (byte-identical): PartyAction_SideProbes, the pose
// (+8 - 1) / 2 + 0x42, +0xA = 0xB, +3 = 1.
void ProbeStartShape() {
    SH_CALL(PartyAction_SideProbes)();
    SH_CALL(Sprite_EnsureAnimation)(static_cast<unsigned char>(HalfTurn(Sc()[8]) + 0x42));
    Sc()[0xA] = 0xB;
    Sc()[3] = 1;
}

}  // namespace

// original 0x51BE90: 0x65F9B4 by +2 (PartyFormAction_Form0Begin, PartyFormAction_Form0Turn, PartyAction_ScriptEnd).
extern "C" void __cdecl PartyFormAction0_Form0(void) { CodeAt(::bof3::Img(PartyFormAction0_Form0States_addr), PartyFormAction0_Form0States_count, Sc()[2], "PartyFormAction0_Form0 (0x51BE90)")(); }

// original 0x51BEB0 (0xFC bytes): the form-0 tables' state 0 (seven cells,
// sets 0, 1, 2, 3 and 5). The pose +4: 0x41 unless bit 0x16 of Cond_Flags'
// row 1 (Flags_Test(0x903F98, 0x16)); with it, 0x50 when Party_Count(0) is 1
// and Field_InputFlags has no 0x40, else 0x40. Then by that pose: 0x40 -
// facing 3; 0x41 - facing 5; 0x50 - facing 3 when +8 is farther from 5 than
// from 3 (|d - 5| > |d - 3|), else facing 5 and the pose 0x52. A facing f:
// +0xB = Sprite_TurnSense(f), +3 = f. Then +9 = 2 and +2 one on.
extern "C" void __cdecl PartyFormAction_Form0Begin(void) {
    if (SH_CALL(Flags_Test)(At(::bof3::Img(Cond_Flags_addr) + 0x8), 0x16) != 0) {
        if (static_cast<unsigned char>(SH_CALL(Party_Count)(0)) == 1 && (Field_InputFlags & 0x40) == 0) Sc()[4] = 0x50;
        else Sc()[4] = 0x40;
    } else {
        Sc()[4] = 0x41;
    }
    unsigned facing = 0;
    bool pose52 = false;
    switch (Sc()[4]) {
    case 0x40: facing = 3; break;
    case 0x41: facing = 5; break;
    case 0x50: {
        const int d = Sc()[8];
        if (Abs32(d - 5) > Abs32(d - 3)) {
            facing = 3;
        } else {
            facing = 5;
            pose52 = true;
        }
        break;
    }
    default: break;
    }
    if (facing != 0) {
        const unsigned char sense = SH_CALL(Sprite_TurnSense)(facing);
        Sc()[0xB] = sense;
        Sc()[3] = static_cast<unsigned char>(facing);
        if (pose52) Sc()[4] = 0x52;
    }
    Sc()[9] = 2;
    ++Sc()[2];
}

// original 0x51BFB0: PartyAction0_Form0States 0x65F9C0 by +2.
extern "C" void __cdecl PartyAction0_Form0(void) { CodeAt(::bof3::Img(PartyAction0_Form0States_addr), PartyAction0_Form0States_count, Sc()[2], "PartyAction0_Form0 (0x51BFB0)")(); }

// originals 0x51BFD0 / 0x51C7C0 / 0x51CD10 (0x1B1 bytes each): sets 0, 1 and
// 2's form-0 state 0 (Form0Begin above).
extern "C" void __cdecl PartyAction0_Form0Begin(void) { Form0Begin(); }

// originals 0x51C190 / 0x51C980 / 0x51CED0 (0xDB bytes each): sets 0, 1 and
// 2's form-0 state 1 (Form0Resolve above), each with its own set's pickup.
extern "C" void __cdecl PartyAction0_Form0Resolve(void) {
    Form0Resolve([](unsigned x, unsigned z) -> unsigned char { return SH_CALL(PartyAction0_CellPickup)(x, z); },
                 "PartyAction0_Form0Resolve (0x51C190)");
}

// originals 0x51C270 / 0x51CA60 / 0x51CFB0 (0x11F bytes each): sets 0, 1 and
// 2's cell pickup (CellPickup above), each called by its own set's resolve.
extern "C" unsigned char __cdecl PartyAction0_CellPickup(unsigned x, unsigned z) { return CellPickup(x, z); }

// original 0x51C390 (0x9F bytes): the point two steps ahead; 1 when
// Sprite_ObjectAt(point, 0) is not 0xFF, or AreaMap_ByteAt is 0xF2 at its cell,
// at the cell one on in x (only when x has a fraction), or one on in z (only
// when z has one); every call made whatever the earlier answered. The cells
// are the point's high words; the original pushes dwords whose upper halves
// are its own uninitialised stack, and AreaMap_ByteAt reads 16 bits.
extern "C" unsigned char __cdecl PartyAction_TargetAhead(void) {
    U x, z;
    TwoAhead(x, z);
    unsigned char found = 0;
    if (SH_CALL(Sprite_ObjectAt)(static_cast<long>(x), static_cast<long>(z), 0) != 0xFF) found = 1;
    if (MapTarget(SH_CALL(AreaMap_ByteAt)(Cell(x), Cell(z)))) found = 1;
    if ((x & 0xFFFF) != 0 && MapTarget(SH_CALL(AreaMap_ByteAt)(CellPlusOne(x), Cell(z)))) found = 1;
    if ((z & 0xFFFF) != 0 && MapTarget(SH_CALL(AreaMap_ByteAt)(Cell(x), CellPlusOne(z)))) found = 1;
    return found;
}

// original 0x51C430: 0x65F9CC by +2 (PartyFormAction_Form1Begin, PartyFormAction_Form1Turn, PartyAction_ScriptEnd).
extern "C" void __cdecl PartyFormAction0_Form1(void) { CodeAt(::bof3::Img(PartyFormAction0_Form1States_addr), PartyFormAction0_Form1States_count, Sc()[2], "PartyFormAction0_Form1 (0x51C430)")(); }

// original 0x51C450: 0x65F9D8 by +2 (0x5252B0, 0x521A20).
extern "C" void __cdecl PartyAction0_Form1(void) { CodeAt(::bof3::Img(PartyAction0_Form1States_addr), PartyAction0_Form1States_count, Sc()[2], "PartyAction0_Form1 (0x51C450)")(); }

// original 0x51C470: 0x65F9E0 by +2 (0x520840, PartyFormAction_Form2Turn, PartyAction_ScriptEnd).
extern "C" void __cdecl PartyFormAction0_Form2(void) { CodeAt(::bof3::Img(PartyFormAction0_Form2States_addr), PartyFormAction0_Form2States_count, Sc()[2], "PartyFormAction0_Form2 (0x51C470)")(); }

// original 0x51C490 (0x7E bytes): state 1 of set 0's form-2 form action and of
// 13 more cells (sets 2 and 5) - TurnToward with facing 3 posing 0x41 and
// facing 5 posing 0x40.
extern "C" void __cdecl PartyFormAction_Form2Turn(void) { TurnToward(3, 0x41, 5, 0x40); }

// original 0x51C510: 0x65F9EC by +2 (PartyAction0_Form2Begin, 0x521C40).
extern "C" void __cdecl PartyAction0_Form2(void) { CodeAt(::bof3::Img(PartyAction0_Form2States_addr), PartyAction0_Form2States_count, Sc()[2], "PartyAction0_Form2 (0x51C510)")(); }

// original 0x51C530 (0x166 bytes): set 0's form-2 state 0. With an effect
// object of kind 0x30 lined up ahead (PartyAction_Kind30Ahead, not 0xFF):
// when a member stands on it (PartyAction_MemberOnEffect) or beyond it
// (PartyAction_MemberBeyondEffect), Field_State +0x137 = 0 and nothing more;
// else its +0xB = 1, Sound_PlayEffect(u16 +0x2C + 0x100), the pose +8 + 8,
// Field_State +0x128 = 2, Field_ScriptFlags' high byte (+1) |= 0x10, Field_JumpStart,
// +9 one down, Field_LeaderStepTick, Field_State +0x137 = 1, +2 one on. With
// none: the point two steps ahead (computed first); unless Field_State +0x138
// bit 0, the object Sprite_ObjectAt(point, margin 1) finds is marked (the
// index compared unsigned); Field_State +0x137 = 0.
extern "C" void __cdecl PartyAction0_Form2Begin(void) {
    const unsigned char k = SH_CALL(PartyAction_Kind30Ahead)();
    if (k != 0xFF) {
        if (SH_CALL(PartyAction_MemberOnEffect)(k) != 0 || SH_CALL(PartyAction_MemberBeyondEffect)(k) != 0) {
            Fs()[0x137] = 0;
            return;
        }
        EffectByAnswer(k, "PartyAction0_Form2Begin (0x51C530)")[0xB] = 1;
        SH_CALL(Sound_PlayEffect)(static_cast<unsigned short>(Word(Sc() + 0x2C) + 0x100));
        SH_CALL(Sprite_EnsureAnimation)(static_cast<unsigned char>(Sc()[8] + 8));
        Fs()[0x128] = 2;
        B(::bof3::Img(rest_1a::at::kScriptFlags3.v)) = static_cast<unsigned char>(B(::bof3::Img(rest_1a::at::kScriptFlags3.v)) | 0x10);
        SH_CALL(Field_JumpStart)();
        --Sc()[9];
        SH_CALL(Field_LeaderStepTick)();
        Fs()[0x137] = 1;
        ++Sc()[2];
        return;
    }
    const unsigned char* const s = Sc();
    const unsigned d = s[8];
    const URest1a x = static_cast<URest1a>(rv::As<records::PartyMember>(s)->x) + 2u * StepXRest1a(d);
    const URest1a z = static_cast<URest1a>(rv::As<records::PartyMember>(s)->z) + 2u * StepZRest1a(d);
    if ((Fs()[0x138] & 1) == 0) {
        const unsigned char found = SH_CALL(Sprite_ObjectAt)(static_cast<long>(x), static_cast<long>(z), 1);
        if (found != 0xFF) MarkObject(found, "PartyAction0_Form2Begin (0x51C530)");
    }
    Fs()[0x137] = 0;
}

// original 0x51C6A0 (0x92 bytes): the point two steps beyond effect object
// `index` in Sprite_Current's direction - the record's position and the
// direction read once, first - against ObjTrio's records 1 .. Field_MemberCount
// - 1, the count and the record's height word re-read for each:
// Sprite_PointInReach(x, z, height, 1, member) non-zero gives 1.
extern "C" unsigned char __cdecl PartyAction_MemberBeyondEffect(unsigned index) {
    const unsigned char* const e = EffectByArgument(index, "PartyAction_MemberBeyondEffect (0x51C6A0)");
    const unsigned d = Sprite_Current[8];
    const U x = static_cast<U>(rv::As<records::EffectObject>(e)->x) + 2u * StepXRest0a(d);
    const U z = static_cast<U>(rv::As<records::EffectObject>(e)->z) + 2u * StepZRest0a(d);
    for (unsigned m = 1; m < Field_MemberCount; ++m)
        if (SH_CALL(Sprite_PointInReach)(static_cast<int>(x), static_cast<int>(z), static_cast<short>(rv::As<records::EffectObject>(e)->height_whole), 1,
                                         Member(m)) != 0)
            return 1;
    return 0;
}

// original 0x51C740: Field_FormActions[0] - PartyFormAction0_Forms 0x65F9F4 by u16 +0x2C.
extern "C" void __cdecl PartyFormAction0_ByForm(void) { CodeAt(::bof3::Img(PartyFormAction0_Forms_addr), PartyFormAction0_Forms_count, rv::As<records::PartyMember>(Sc())->column, "PartyFormAction0_ByForm (0x51C740)")(); }

// original 0x51C760: Field_ActionBySet[0] - PartyAction0_Forms 0x65FA00 by u16 +0x2C.
extern "C" void __cdecl PartyAction0_ByForm(void) { CodeAt(::bof3::Img(PartyAction0_Forms_addr), PartyAction0_Forms_count, rv::As<records::PartyMember>(Sc())->column, "PartyAction0_ByForm (0x51C760)")(); }

// original 0x51C780: 0x65FA0C by +2 (PartyFormAction_Form0Begin, PartyFormAction_Form0Turn, PartyAction_ScriptEnd).
extern "C" void __cdecl PartyFormAction1_Form0(void) { CodeAt(::bof3::Img(PartyFormAction1_Form0States_addr), PartyFormAction1_Form0States_count, Sc()[2], "PartyFormAction1_Form0 (0x51C780)")(); }

// original 0x51C7A0: PartyAction1_Form0States 0x65FA18 by +2.
extern "C" void __cdecl PartyAction1_Form0(void) { CodeAt(::bof3::Img(PartyAction1_Form0States_addr), PartyAction1_Form0States_count, Sc()[2], "PartyAction1_Form0 (0x51C7A0)")(); }

extern "C" void __cdecl PartyAction1_Form0Begin(void) { Form0Begin(); }

extern "C" void __cdecl PartyAction1_Form0Resolve(void) {
    Form0Resolve([](unsigned x, unsigned z) -> unsigned char { return SH_CALL(PartyAction1_CellPickup)(x, z); },
                 "PartyAction1_Form0Resolve (0x51C980)");
}

extern "C" unsigned char __cdecl PartyAction1_CellPickup(unsigned x, unsigned z) { return CellPickup(x, z); }

// original 0x51CB80: 0x65FA24 by +2 (PartyFormAction_Form1Begin, PartyFormAction_Form1Turn, PartyAction_ScriptEnd).
extern "C" void __cdecl PartyFormAction1_Form1(void) { CodeAt(::bof3::Img(PartyFormAction1_Form1States_addr), PartyFormAction1_Form1States_count, Sc()[2], "PartyFormAction1_Form1 (0x51CB80)")(); }

// original 0x51CBA0: 0x65FA30 by +2 (0x5252B0, 0x521A20).
extern "C" void __cdecl PartyAction1_Form1(void) { CodeAt(::bof3::Img(PartyAction1_Form1States_addr), PartyAction1_Form1States_count, Sc()[2], "PartyAction1_Form1 (0x51CBA0)")(); }

// original 0x51CBC0: 0x65FA38 by +2 (0x520840, 0x51FC80, PartyAction_ScriptEnd).
extern "C" void __cdecl PartyFormAction1_Form2(void) { CodeAt(::bof3::Img(PartyFormAction1_Form2States_addr), PartyFormAction1_Form2States_count, Sc()[2], "PartyFormAction1_Form2 (0x51CBC0)")(); }

// original 0x51CBE0: 0x65FA44 by +2 (0x5239F0, 0x51DE20, 0x520350).
extern "C" void __cdecl PartyAction1_Form2(void) { CodeAt(::bof3::Img(PartyAction1_Form2States_addr), PartyAction1_Form2States_count, Sc()[2], "PartyAction1_Form2 (0x51CBE0)")(); }

// original 0x51CC00: Field_FormActions[1] - 0x65FA50 by u16 +0x2C.
extern "C" void __cdecl PartyFormAction1_ByForm(void) { CodeAt(::bof3::Img(PartyFormAction1_Forms_addr), PartyFormAction1_Forms_count, rv::As<records::PartyMember>(Sc())->column, "PartyFormAction1_ByForm (0x51CC00)")(); }

// original 0x51CC20: Field_ActionBySet[1] - 0x65FA5C by u16 +0x2C.
extern "C" void __cdecl PartyAction1_ByForm(void) { CodeAt(::bof3::Img(PartyAction1_Forms_addr), PartyAction1_Forms_count, rv::As<records::PartyMember>(Sc())->column, "PartyAction1_ByForm (0x51CC20)")(); }

// original 0x51CC40: 0x65FA68 by +2 (PartyFormAction_Form0Begin, PartyFormAction_Form0Turn, PartyAction_ScriptEnd).
extern "C" void __cdecl PartyFormAction2_Form0(void) { CodeAt(::bof3::Img(PartyFormAction2_Form0States_addr), PartyFormAction2_Form0States_count, Sc()[2], "PartyFormAction2_Form0 (0x51CC40)")(); }

// original 0x51CC60 (0x83 bytes): the form-0 tables' state 1. When +8 has come
// round to +3: the pose +4 - Sprite_SetAnimationAt(0x50, 2) for 0x50, else
// Sprite_SetAnimation(+4) - and +2 one on. Else +9 counted down and, at 0, +9 =
// 2, +8 += +0xB, & 7, and the pose +8 (Sprite_EnsureAnimation).
extern "C" void __cdecl PartyFormAction_Form0Turn(void) {
    unsigned char* const s = Sc();
    if (s[8] == s[3]) {
        const unsigned char pose = s[4];
        if (pose == 0x50) SH_CALL(Sprite_SetAnimationAt)(0x50, 2);
        else SH_CALL(Sprite_SetAnimation)(pose);
        ++Sc()[2];
        return;
    }
    --s[9];
    if (Sc()[9] != 0) return;
    Sc()[9] = 2;
    Sc()[8] = static_cast<unsigned char>(Sc()[8] + Sc()[0xB]);
    Sc()[8] = static_cast<unsigned char>(Sc()[8] & 7);
    SH_CALL(Sprite_EnsureAnimation)(Sc()[8]);
}

// original 0x51CCF0: PartyAction2_Form0States 0x65FA74 by +2.
extern "C" void __cdecl PartyAction2_Form0(void) { CodeAt(::bof3::Img(PartyAction2_Form0States_addr), PartyAction2_Form0States_count, Sc()[2], "PartyAction2_Form0 (0x51CCF0)")(); }

extern "C" void __cdecl PartyAction2_Form0Begin(void) { Form0Begin(); }

extern "C" void __cdecl PartyAction2_Form0Resolve(void) {
    Form0Resolve([](unsigned x, unsigned z) -> unsigned char { return SH_CALL(PartyAction2_CellPickup)(x, z); },
                 "PartyAction2_Form0Resolve (0x51CED0)");
}

extern "C" unsigned char __cdecl PartyAction2_CellPickup(unsigned x, unsigned z) { return CellPickup(x, z); }

// original 0x51D0D0: 0x65FA80 by +2 (PartyFormAction_Form1Begin, PartyFormAction_Form1Turn, PartyAction_ScriptEnd).
extern "C" void __cdecl PartyFormAction2_Form1(void) { CodeAt(::bof3::Img(PartyFormAction2_Form1States_addr), PartyFormAction2_Form1States_count, Sc()[2], "PartyFormAction2_Form1 (0x51D0D0)")(); }

// original 0x51D0F0 (0x70 bytes): the form-1 tables' state 0 (sets 0, 1, 2):
// facing 7 when +8 is farther from 1 than from 7 (|d - 1| > |d - 7|), else
// facing 1: +0xB = Sprite_TurnSense(facing), +3 = facing; +9 = 2, +2 one on.
extern "C" void __cdecl PartyFormAction_Form1Begin(void) {
    const int d = Sc()[8];
    const unsigned facing = Abs32(d - 1) > Abs32(d - 7) ? 7 : 1;
    const unsigned char sense = SH_CALL(Sprite_TurnSense)(facing);
    Sc()[0xB] = sense;
    Sc()[3] = static_cast<unsigned char>(facing);
    Sc()[9] = 2;
    ++Sc()[2];
}

// original 0x51D160 (0x7C bytes): the form-1 tables' state 1 - TurnToward
// with facing 1 posing 0x40 and facing 7 posing 0x41.
extern "C" void __cdecl PartyFormAction_Form1Turn(void) { TurnToward(1, 0x40, 7, 0x41); }

// original 0x51D1E0: 0x65FA8C by +2 (0x5252B0, 0x521A20).
extern "C" void __cdecl PartyAction2_Form1(void) { CodeAt(::bof3::Img(PartyAction2_Form1States_addr), PartyAction2_Form1States_count, Sc()[2], "PartyAction2_Form1 (0x51D1E0)")(); }

// original 0x51D200: 0x65FA94 by +2 (0x520840, 0x51FC80, BossOp_ScriptTick).
extern "C" void __cdecl PartyFormAction2_Form2(void) { CodeAt(::bof3::Img(PartyFormAction2_Form2States_addr), PartyFormAction2_Form2States_count, Sc()[2], "PartyFormAction2_Form2 (0x51D200)")(); }

// original 0x51D220: 0x65FAA0 by +2 (PartyAction2_Form2State0, PartyAction2_Form2State1).
extern "C" void __cdecl PartyAction2_Form2(void) { CodeAt(::bof3::Img(PartyAction2_Form2States_addr), PartyAction2_Form2States_count, Sc()[2], "PartyAction2_Form2 (0x51D220)")(); }

// original 0x51D240: 0x65FAA8 by +3 (PartyAction2_Form2Aim, PartyAction2_Form2Strike,
// PartyAction_FinishPalette, 0x51F850, 0x522DE0).
extern "C" void __cdecl PartyAction2_Form2State0(void) { CodeAt(::bof3::Img(PartyAction2_Form2State0Steps_addr), PartyAction2_Form2State0Steps_count, Sc()[3], "PartyAction2_Form2State0 (0x51D240)")(); }

// original 0x51D260 (0x87 bytes): set 2's form-2 step 0 (+3). An even
// direction +8 is turned one eighth back (-1, & 7); when
// PartyAction_BlockedAhead finds that way open, two on (+2), and when that is
// open too, back to the first turn (-2). Then PartyAction_SideProbes, the pose
// (+8 - 1) / 2 + 0x42, +0xB = 0, +0xA = 0xB, +3 = 1.
extern "C" void __cdecl PartyAction2_Form2Aim(void) {
    if ((Sc()[8] & 1) == 0) {
        Sc()[8] = static_cast<unsigned char>((Sc()[8] - 1) & 7);
        if (SH_CALL(PartyAction_BlockedAhead)() == 0) {
            Sc()[8] = static_cast<unsigned char>((Sc()[8] + 2) & 7);
            if (SH_CALL(PartyAction_BlockedAhead)() == 0) Sc()[8] = static_cast<unsigned char>((Sc()[8] - 2) & 7);
        }
    }
    SH_CALL(PartyAction_SideProbes)();
    SH_CALL(Sprite_EnsureAnimation)(static_cast<unsigned char>(HalfTurn(Sc()[8]) + 0x42));
    Sc()[0xB] = 0;
    Sc()[0xA] = 0xB;
    Sc()[3] = 1;
}

// original 0x51D2F0 (0x14C bytes): set 2's form-2 step 1 (+3). With +0xA not
// 0, it is counted down; reaching 0: Sound_PlayEffect(u16 +0x2C + 0x100); the
// effect object ahead (Field_EffectAhead): with one, Sound_PlayEffect(0x10B),
// its +8 = the sprite's +8 and +0xA = 1, the sprite's +6 = its index, and +3
// one on; with none, the object Sprite_ObjectAt(two steps ahead, margin 0)
// finds is marked (signed) with Sound_PlayEffect(0x10B), then
// PartyAction2_CellStrike on the point's cell, the cell one on in x (x with a
// fraction) and one on in z (z with one), each only while the earlier
// answered 0 - as the form-0 resolve. Then +3 one on (so two with an effect
// object ahead). Sprite_ScriptTickOnce every time.
extern "C" void __cdecl PartyAction2_Form2Strike(void) {
    const unsigned char t = Sc()[0xA];
    if (t != 0) {
        Sc()[0xA] = static_cast<unsigned char>(t - 1);
        if (Sc()[0xA] == 0) {
            SH_CALL(Sound_PlayEffect)(static_cast<unsigned short>(Word(Sc() + 0x2C) + 0x100));
            const unsigned char e = SH_CALL(Field_EffectAhead)();
            if (e != 0xFF) {
                SH_CALL(Sound_PlayEffect)(0x10B);
                unsigned char* const s = Sc();
                unsigned char* const rec = EffectByAnswer(e, "PartyAction2_Form2Strike (0x51D2F0)");
                rec[8] = s[8];
                rec[0xA] = 1;
                s[6] = e;
                ++Sc()[3];
            } else {
                const unsigned char* const s = Sc();
                const unsigned d = s[8];
                const URest1a x = static_cast<URest1a>(rv::As<records::PartyMember>(s)->x) + 2u * StepXRest1a(d);
                const URest1a z = static_cast<URest1a>(Long(s + 0x38)) + 2u * StepZRest1a(d);
                const unsigned char found = SH_CALL(Sprite_ObjectAt)(static_cast<long>(x), static_cast<long>(z), 0);
                if (found != 0xFF) {
                    MarkObject(found, "PartyAction2_Form2Strike (0x51D2F0)");
                    SH_CALL(Sound_PlayEffect)(0x10B);
                }
                const unsigned cx = x >> 16, cz = z >> 16;
                if (SH_CALL(PartyAction2_CellStrike)(cx, cz) == 0) {
                    bool done = false;
                    if ((x & 0xFFFF) != 0) done = SH_CALL(PartyAction2_CellStrike)(cx + 1, cz) != 0;
                    if (!done && (z & 0xFFFF) != 0) SH_CALL(PartyAction2_CellStrike)(cx, cz + 1);
                }
            }
            ++Sc()[3];
        }
    }
    SH_CALL(Sprite_ScriptTickOnce)();
}

// original 0x51D440 (0x94 bytes): an action's last state (nine cells: sets 2
// and 5; PSX twins in the PLP overlays, 0x801CEA04 among them, pairs by
// callers). With +0xB 1: Sprite_LoadPalette(0x80D380 + +5 * 0x40, 0) and +0xB
// = 2. Then by +0xB: 0 - Sprite_ScriptTickOnce, the end when it answers not 0;
// 2 - Sprite_ScriptTickOnce, and when it answers not 0 Sprite_SetAnimation(+8)
// and +0xB = 3; then the end unless Field_Request is set; any other -
// Sprite_ScriptTick, then the end unless Field_Request is set. The end: +0x2B
// = 0 and Field_State +0x137 = 0 (PartyAction_Finish with the palette put back
// first).
extern "C" void __cdecl PartyAction_FinishPalette(void) {
    if (Sc()[0xB] == 1) {
        SH_CALL(Sprite_LoadPalette)(reinterpret_cast<unsigned short*>(static_cast<std::uintptr_t>(::bof3::Img(Gfx_ClutStripSource_addr) + 0x1e00 + (static_cast<URest1a>(Sc()[5]) << 6))), 0);
        Sc()[0xB] = 2;
    }
    const unsigned char b = Sc()[0xB];
    if (b == 0) {
        if (SH_CALL(Sprite_ScriptTickOnce)() == 0) return;
    } else if (b == 2) {
        if (SH_CALL(Sprite_ScriptTickOnce)() != 0) {
            SH_CALL(Sprite_SetAnimation)(Sc()[8]);
            Sc()[0xB] = 3;
        }
        if (Field_Request != 0) return;
    } else {
        SH_CALL(Sprite_ScriptTick)();
        if (Field_Request != 0) return;
    }
    Sc()[0x2B] = 0;
    Fs()[0x137] = 0;
}

// original 0x51D4E0 (0x188 bytes): set 2's cell strike - what lies in the map
// cell (x, z), AreaMap_ByteAt:
//   0xF0, 0xF1, 0xF4 - Effect_SpawnAtCellHigh(0, x, z); a second, state 4,
//          when Rand & 7 is above 5; Sound_PlayEffect(0x10B); al 1.
//   0xF6, 0xF7 - Effect_SpawnAtCellHigh(0, x, z), Sound_PlayEffect(0x10B); a
//          Rand nibble below 7: Effect_SpawnAtCellHigh(3, x, z), the name of
//          item 0x29 of category 0 into Text_Records (16 bytes),
//          Inventory_Add(0, 0x29, 1): taken, Sound_PlayEffect(0x106) and
//          Msg_OpenSystem(2), else Msg_OpenSystem(3); +0xB = 2. Above 0xB:
//          Effect_SpawnAtCellHigh(2, x, z), Sprite_FlashClut(0), Char_LoseHp(1,
//          Field_State +0x89), Msg_OpenSystem(0xD9), +0xB = 1. 7..0xB nothing
//          more. Then Field_Request = 2; al 1.
//   any other - al 0.
// As the original has it: x and z go on as given (each callee reads a word);
// it stores the cell byte into its own z slot's low byte and pushes that dword
// as a fourth argument to Effect_SpawnAtCellHigh, which reads three;
// Inventory_Add is pushed a fourth dword 0, never read.
extern "C" unsigned char __cdecl PartyAction2_CellStrike(unsigned x, unsigned z) {
    const unsigned char cell = SH_CALL(AreaMap_ByteAt)(static_cast<short>(x), static_cast<short>(z));
    if (cell == 0xF0 || cell == 0xF1 || cell == 0xF4) {
        SH_CALL(Effect_SpawnAtCellHigh)(0, x, z);
        if ((static_cast<unsigned char>(SH_CALL(Rand)()) & 7u) > 5) SH_CALL(Effect_SpawnAtCellHigh)(4, x, z);
        SH_CALL(Sound_PlayEffect)(0x10B);
        return 1;
    }
    if (cell != 0xF6 && cell != 0xF7) return 0;
    SH_CALL(Effect_SpawnAtCellHigh)(0, x, z);
    SH_CALL(Sound_PlayEffect)(0x10B);
    const unsigned roll = static_cast<unsigned char>(SH_CALL(Rand)()) & 0xFu;
    if (roll < 7) {
        SH_CALL(Effect_SpawnAtCellHigh)(3, x, z);
        const unsigned char* const name = SH_CALL(Item_NamePtr)(0, 0x29);
        for (unsigned k = 0; k < 16; k += 4) SetLong(At(::bof3::Img(bof3::addr::Text_Records) + k), Long(name + k));
        if (SH_CALL(Inventory_Add)(0, 0x29, 1) != 0) {
            SH_CALL(Sound_PlayEffect)(0x106);
            SH_CALL(Msg_OpenSystem)(2);
        } else {
            SH_CALL(Msg_OpenSystem)(3);
        }
        Sc()[0xB] = 2;
    } else if (roll > 0xB) {
        SH_CALL(Effect_SpawnAtCellHigh)(2, x, z);
        SH_CALL(Sprite_FlashClut)(0);
        SH_CALL(Char_LoseHp)(1, Fs()[0x89]);
        SH_CALL(Msg_OpenSystem)(0xD9);
        Sc()[0xB] = 1;
    }
    Field_Request = 2;
    return 1;
}

// original 0x51D670: 0x65FABC by +3 (PartyAction2_Form2Reaim, 0x523F10, PartyAction_WaitEffect).
extern "C" void __cdecl PartyAction2_Form2State1(void) { CodeAt(::bof3::Img(PartyAction2_Form2State1Steps_addr), PartyAction2_Form2State1Steps_count, Sc()[3], "PartyAction2_Form2State1 (0x51D670)")(); }

// original 0x51D690 (0x35 bytes): set 2's form-2 state 1, step 0:
// PartyAction_SideProbes, the pose (+8 - 1) / 2 + 0x42, +0xA = 0xB, +3 = 1
// (PartyAction2_Form2Aim without the turn and +0xB).
extern "C" void __cdecl PartyAction2_Form2Reaim(void) {
    SH_CALL(PartyAction_SideProbes)();
    SH_CALL(Sprite_EnsureAnimation)(static_cast<unsigned char>(HalfTurn(Sc()[8]) + 0x42));
    Sc()[0xA] = 0xB;
    Sc()[3] = 1;
}

// original 0x51D6D0 (0x3D bytes): a state of seven cells (sets 2 and 5, set
// 2's form-2 state 1 step 2): the effect object +0xB names - when it is free
// (+0) or in its state 1 (+1), and Field_Kind2Hold is 0, +0x2B = 0 and
// Field_State +0x137 = 0. Sprite_ScriptTick every time. As the original has
// it: +0xB indexes Effect_Objects unchecked (a read; docs/rest_1a.md section 5).
extern "C" void __cdecl PartyAction_WaitEffect(void) {
    unsigned char* const s = Sc();
    const unsigned char* const e = rv::Bytes(Effect_Objects) + s[0xB] * 0x80u;
    if ((e[0] == 0 || e[1] == 1) && Field_Kind2Hold == 0) {
        s[0x2B] = 0;
        Fs()[0x137] = 0;
    }
    SH_CALL(Sprite_ScriptTick)();
}

extern "C" void __cdecl PartyFormAction2_ByForm(void) { ByForm(R1B_TABLE(PartyFormAction2_Forms), "PartyFormAction2_ByForm (0x51D710)"); }

extern "C" void __cdecl PartyAction2_ByForm(void) { ByForm(R1B_TABLE(PartyAction2_Forms), "PartyAction2_ByForm (0x51D730)"); }

extern "C" void __cdecl PartyFormAction3_Form0(void) { ByState(R1B_TABLE(PartyFormAction3_Form0States), "PartyFormAction3_Form0 (0x51D750)"); }

extern "C" void __cdecl PartyAction3_Form0(void) { ByState(R1B_TABLE(PartyAction3_Form0States), "PartyAction3_Form0 (0x51D770)"); }

// original 0x51D790 (0x1B1 bytes), set 3's form 0, state 0.
extern "C" void __cdecl PartyAction3_Form0Begin(void) { Begin(); }

// original 0x51D950 (0xDB bytes), set 3's form 0, state 1: its cell handler 0x51DAA0.
extern "C" void __cdecl PartyAction3_Form0Resolve(void) {
    Resolve([](unsigned x, unsigned z) { return SH_CALL(PartyAction3_CellPickup)(x, z); }, "PartyAction3_Form0Resolve (0x51D950)");
}

// original 0x51DA30: entry 2 of the party sets' state tables - 18 .data
// dwords hold it, PartyAction0/1/2/5_Form0States among them - by +0xB:
//   0 - Sprite_ScriptTickOnce; when it answers non-zero, the end;
//   1 - Sprite_ScriptTickOnce; when it answers non-zero,
//       Sprite_SetAnimation(+8) and +0xB = 2;
//   2.. - Sprite_ScriptTick; unless Field_Request is set, the end.
// The end: +0x2B = 0 and Field_State +0x137 = 0.
FH_EXPORT void __cdecl PartyAction_Finish(void) {
    const unsigned char b = Sprite_Current[0xB];
    if (b == 0) {
        if (Al(g.script_tick_once()) == 0) return;
    } else if (b == 1) {
        if (Al(g.script_tick_once()) == 0) return;
        g.set_animation(rv::Current<records::PartyMember>()->direction);
        Sprite_Current[0xB] = 2;
        return;
    } else {
        g.script_tick();
        if (Field_Request != 0) return;
    }
    Sprite_Current[0x2B] = 0;
    Field_State[0x137] = 0;
}

// original 0x51DAA0 (0x11F bytes) / 0x51E1B0 (0x11F bytes).
extern "C" unsigned char __cdecl PartyAction3_CellPickup(unsigned x, unsigned z) { return CellPickupRest1b(x, z); }

extern "C" void __cdecl PartyFormAction3_Form1(void) { ByState(R1B_TABLE(PartyFormAction3_Form1States), "PartyFormAction3_Form1 (0x51DBC0)"); }

extern "C" void __cdecl PartyAction3_Form1(void) { ByState(R1B_TABLE(PartyAction3_Form1States), "PartyAction3_Form1 (0x51DBE0)"); }

// original 0x51DC00 (0x166 bytes), set 3's form 1, state 0; 0x51E310 (0x166
// bytes), set 4's. PartyAction_Kind30Ahead's index (0..19, or 0xFF) handed to
// PartyAction_MemberOnEffect and PartyAction_MemberBeyondEffect (the dword the
// original pushes holds it in its low byte over its own ecx).
extern "C" void __cdecl PartyAction3_Form1Begin(void) { Form1Begin("PartyAction3_Form1Begin (0x51DC00)"); }

// original 0x51DD70 (0x6A bytes): effect object `index`'s own position and its
// height word + 0x200 (a 16-bit add), all re-read for each member, against the
// same members. With Field_MemberCount 1 or less the record is not read.
extern "C" unsigned char __cdecl PartyAction_MemberOnEffect(unsigned index) {
    for (unsigned m = 1; m < Field_MemberCount; ++m) {
        const unsigned char* const e = EffectByArgument(index, "PartyAction_MemberOnEffect (0x51DD70)");
        const auto height = static_cast<short>(static_cast<std::uint16_t>(rv::As<records::EffectObject>(e)->height_whole + 0x200));
        if (SH_CALL(Sprite_PointInReach)(rv::As<records::EffectObject>(e)->x, rv::As<records::EffectObject>(e)->z, height, 1, Member(m)) != 0) return 1;
    }
    return 0;
}

extern "C" void __cdecl PartyFormAction3_Form2(void) { ByState(R1B_TABLE(PartyFormAction3_Form2States), "PartyFormAction3_Form2 (0x51DDE0)"); }

extern "C" void __cdecl PartyAction3_Form2(void) { ByState(R1B_TABLE(PartyAction3_Form2States), "PartyAction3_Form2 (0x51DE00)"); }

// original 0x51DE20 (0x6E bytes): Sprite_ScriptTick; unless the script position
// word +0x58 is then 0xA, nothing more. Else +0xB = Effect_FindFree, read back
// from the record; not 0xFF: that effect object's +0 = 1 and kind +5 = 0x3A
// (+0xB read again for each). The form's sound either way, then +7 = 0 and +2
// one on, Sprite_Current re-read after the sound.
extern "C" void __cdecl PartyAction_SpawnKind3A(void) {
    SH_CALL(Sprite_ScriptTick)();
    if (rv::As<records::PartyMember>(S())->script_pos != 0xA) return;
    const unsigned char slot = SH_CALL(Effect_FindFree)();
    S()[0xB] = slot;
    unsigned char* const s = S();
    if (s[0xB] != 0xFF) {
        EffectAt(s[0xB], "PartyAction_SpawnKind3A (0x51DE20)")[0] = 1;
        EffectAt(s[0xB], "PartyAction_SpawnKind3A (0x51DE20)")[5] = 0x3A;
    }
    FormSound(s);
    S()[7] = 0;
    ++S()[2];
}

extern "C" void __cdecl PartyFormAction3_ByForm(void) { ByForm(R1B_TABLE(PartyFormAction3_Forms), "PartyFormAction3_ByForm (0x51DE90)"); }

extern "C" void __cdecl PartyAction3_ByForm(void) { ByForm(R1B_TABLE(PartyAction3_Forms), "PartyAction3_ByForm (0x51DEB0)"); }

extern "C" void __cdecl PartyFormAction4_Form0(void) { ByState(R1B_TABLE(PartyFormAction4_Form0States), "PartyFormAction4_Form0 (0x51DED0)"); }

extern "C" void __cdecl PartyAction4_Form0(void) { ByState(R1B_TABLE(PartyAction4_Form0States), "PartyAction4_Form0 (0x51DEF0)"); }

// original 0x51DF10 (0x1B1 bytes), set 4's form 0, state 0.
extern "C" void __cdecl PartyAction4_Form0Begin(void) { Begin(); }

// original 0x51E0D0 (0xDB bytes), set 4's form 0, state 1: its cell handler 0x51E1B0.
extern "C" void __cdecl PartyAction4_Form0Resolve(void) {
    Resolve([](unsigned x, unsigned z) { return SH_CALL(PartyAction4_CellPickup)(x, z); }, "PartyAction4_Form0Resolve (0x51E0D0)");
}

extern "C" unsigned char __cdecl PartyAction4_CellPickup(unsigned x, unsigned z) { return CellPickupRest1b(x, z); }

extern "C" void __cdecl PartyFormAction4_Form1(void) { ByState(R1B_TABLE(PartyFormAction4_Form1States), "PartyFormAction4_Form1 (0x51E2D0)"); }

extern "C" void __cdecl PartyAction4_Form1(void) { ByState(R1B_TABLE(PartyAction4_Form1States), "PartyAction4_Form1 (0x51E2F0)"); }

extern "C" void __cdecl PartyAction4_Form1Begin(void) { Form1Begin("PartyAction4_Form1Begin (0x51E310)"); }

extern "C" void __cdecl PartyFormAction4_Form2(void) { ByState(R1B_TABLE(PartyFormAction4_Form2States), "PartyFormAction4_Form2 (0x51E480)"); }

extern "C" void __cdecl PartyAction4_Form2(void) { ByState(R1B_TABLE(PartyAction4_Form2States), "PartyAction4_Form2 (0x51E4A0)"); }

extern "C" void __cdecl PartyAction4_Form2State0(void) { ByStep(R1B_TABLE(PartyAction4_Form2State0Steps), "PartyAction4_Form2State0 (0x51E4C0)"); }

// original 0x51E4E0 (0x87 bytes), set 4's form 2, state 0 step 0: an even
// direction turned toward a blocked cell (PartyAction_BlockedAhead answering 1
// keeps the turn); PartyAction_SideProbes; the pose (+8 - 1) / 2 + 0x42; +0xB
// = 0, +0xA = 0xB, +3 = 1.
extern "C" void __cdecl PartyAction4_Form2Aim(void) {
    TurnUntil([] { return SH_CALL(PartyAction_BlockedAhead)(); });
    SH_CALL(PartyAction_SideProbes)();
    SH_CALL(Sprite_EnsureAnimation)(static_cast<unsigned char>(HalfTurn(S()[8]) + 0x42));
    S()[0xB] = 0;
    S()[0xA] = 0xB;
    S()[3] = 1;
}

// original 0x51E570 (0x14C bytes), set 4's form 2, state 0 step 1: its cell handler 0x51E6C0.
extern "C" void __cdecl PartyAction4_Form2Hit(void) {
    Hit([](unsigned x, unsigned z) { return SH_CALL(PartyAction4_CellHit)(x, z); }, true, "PartyAction4_Form2Hit (0x51E570)");
}

// original 0x51E6C0 (0x188 bytes) / 0x51EF30 (0x188 bytes). The original also
// stores the cell's code into the low byte of its own z argument slot and
// pushes that dword as a fourth argument Effect_SpawnAtCellHigh never reads.
extern "C" unsigned char __cdecl PartyAction4_CellHit(unsigned x, unsigned z) { return CellHit(x, z); }

extern "C" void __cdecl PartyAction4_Form2State1(void) { ByStep(R1B_TABLE(PartyAction4_Form2State1Steps), "PartyAction4_Form2State1 (0x51E850)"); }

// original 0x51E870 (0x35 bytes), set 4's form 2, state 1 step 0:
// PartyAction_SideProbes, the pose (+8 - 1) / 2 + 0x42, +0xA = 0xB, +3 = 1.
extern "C" void __cdecl PartyAction4_Form2Again(void) {
    SH_CALL(PartyAction_SideProbes)();
    SH_CALL(Sprite_EnsureAnimation)(static_cast<unsigned char>(HalfTurn(S()[8]) + 0x42));
    S()[0xA] = 0xB;
    S()[3] = 1;
}

extern "C" void __cdecl PartyFormAction4_ByForm(void) { ByForm(R1B_TABLE(PartyFormAction4_Forms), "PartyFormAction4_ByForm (0x51E8B0)"); }

extern "C" void __cdecl PartyAction4_ByForm(void) { ByForm(R1B_TABLE(PartyAction4_Forms), "PartyAction4_ByForm (0x51E8D0)"); }

extern "C" void __cdecl PartyFormAction5_Form0(void) { ByState(R1B_TABLE(PartyFormAction5_Form0States), "PartyFormAction5_Form0 (0x51E8F0)"); }

// original 0x51E910: party set 5's form 0 - the entry of
// PartyAction5_Form0States 0x65FBC0 that +2 picks (0x51E930, 0x51EAF0,
// 0x51DA30), jumped to, the index unchecked.
FH_EXPORT void __cdecl PartyAction5_Form0(void) { Entry(::bof3::Img(PartyAction5_Form0States_addr), rv::Current<records::PartyMember>()->step)(); }

// original 0x51E930, form 0's state 0. An even direction +8 is turned one
// eighth back (-1, & 7); when 0x51C390 finds nothing ahead that way (no
// object two steps on, no 0xF2 cell), two on (+2), and when it finds nothing
// there either, back to the first turn (-2). Then the slope one
// step ahead in +8 (MapView_SlopeAt with the whole direction byte): steep
// (the scratch flag and more than 0x40 as a short) - Sprite_EnsureAnimation(
// (+8 - 1) / 2 + 0x46) and +2 one on; otherwise +0x2B = 1, the side probes
// in directions 3 and 5 (which clear it again), Sound_PlayEffect(u16 +0x2C +
// 0x100), Sprite_EnsureAnimation((+8 - 1) / 2 + 0x42) and +0xA = 5. Then +0xB
// = 0 and +2 one on - so a steep slope moves +2 by two, to PartyAction_Finish.
//
// As the original has it: an odd +8 is kept and not masked, and the steps
// table is read with it unchecked; Sprite_Current is re-read after each call.
FH_EXPORT void __cdecl PartyAction5_Form0Begin(void) {
    const records::PartyMember* s = rv::Current<records::PartyMember>();
    if ((s->direction & 1) == 0) {
        rv::Current<records::PartyMember>()->direction = static_cast<unsigned char>((s->direction - 1) & 7);
        if (Al(g.target_ahead()) == 0) {
            rv::Current<records::PartyMember>()->direction = static_cast<unsigned char>((rv::Current<records::PartyMember>()->direction + 2) & 7);
            if (Al(g.target_ahead()) == 0) rv::Current<records::PartyMember>()->direction = static_cast<unsigned char>((rv::Current<records::PartyMember>()->direction - 2) & 7);
        }
        s = rv::Current<records::PartyMember>();
    }
    const unsigned d = s->direction;
    const long x = Add(s->x, StepX(d));
    const long z = Add(s->z, StepZ(d));
    const long slope = g.slope_at(x, z, d);
    if (field_hidden::at::kScratch != 0 && static_cast<short>(slope) > 0x40) {
        g.ensure_animation(static_cast<unsigned char>(HalfTurn(rv::Current<records::PartyMember>()->direction) + 0x46));
        ++rv::Current<records::PartyMember>()->step;
    } else {
        Sprite_Current[0x2B] = 1;
        ProbeSide(3);
        ProbeSide(5);
        g.play_effect(static_cast<std::uint16_t>(Word(Sprite_Current + 0x2C) + 0x100));
        g.ensure_animation(static_cast<unsigned char>(HalfTurn(rv::Current<records::PartyMember>()->direction) + 0x42));
        Sprite_Current[0xA] = 5;
    }
    Sprite_Current[0xB] = 0;
    ++rv::Current<records::PartyMember>()->step;
}

// original 0x51EAF0, form 0's state 1. +0xA counted down; at 0: the point two
// steps ahead in +8 (unchecked). The object Sprite_ObjectAt(point, 0) finds
// there gets bit 0 of its +0x80 (0..0x1D Sprite_Objects, above that
// Sprite_ObjectsExtra, the index a signed byte); then Field_CellPickup on
// the point's cell, and when that finds nothing, on the cell one on in x
// (only when the point's x has a fraction), then one on in z (when z has
// one); then +2 one on. Sprite_ScriptTickOnce every time.
//
// As the original has it: the cells are the high words of the point, and
// the original passes them as dwords whose upper halves are its own
// uninitialised stack; every callee reads only the low word (docs section 3).
FH_EXPORT void __cdecl PartyAction5_Form0Resolve(void) {
    --Sprite_Current[0xA];
    if (Sprite_Current[0xA] == 0) {
        const records::PartyMember* const s = rv::Current<records::PartyMember>();
        const unsigned d = s->direction;
        const std::uint32_t x = static_cast<std::uint32_t>(s->x) + 2u * static_cast<std::uint32_t>(StepX(d));
        const std::uint32_t z = static_cast<std::uint32_t>(s->z) + 2u * static_cast<std::uint32_t>(StepZ(d));
        const unsigned char found = Al(g.object_at(static_cast<long>(x), static_cast<long>(z), 0));
        if (found != 0xFF) {
            const int i = static_cast<signed char>(found);
            if (i < 0x1E)
                At(::bof3::Img(Sprite_Objects_addr) + 0x80 + static_cast<std::uint32_t>(i * static_cast<int>(field_hidden::at::kObjectStride)))[0] |= 1;
            else
                At(::bof3::Img(Sprite_ObjectsExtra_addr) + 0x80 + static_cast<std::uint32_t>((i - 0x1E) * static_cast<int>(field_hidden::at::kObjectStride)))[0] |= 1;
        }
        const unsigned cx = x >> 16, cz = z >> 16;
        if (Al(g.cell_pickup(cx, cz)) == 0) {
            bool done = false;
            if ((x & 0xFFFF) != 0) done = Al(g.cell_pickup(cx + 1, cz)) != 0;
            if (!done && (z & 0xFFFF) != 0) g.cell_pickup(cx, cz + 1);
        }
        ++rv::Current<records::PartyMember>()->step;
    }
    g.script_tick_once();
}

// original 0x51EBD0: what lies in the map cell (x, z) - AreaMap_ByteAt:
//   0xF2 - with an effect object free (Effect_FindFree): 0x524870(0, x, z);
//          three Rand draws in sixteen (& 0xF of 13, 14, 15) find zenny, 2 (5
//          on 15), ten times that when Field_InputFlags has bit 1 or 2 and a
//          second Rand & 3 is 0: 0x5307C0(the amount), 0x524870(1, x, z);
//          then +0xB = 1. With no object free, none of that.
//   0xF8 - 0x524870(0, x, z); the name of item 0x56 of category 0
//          (Item_NamePtr) copied, 16 bytes, into Text_Records; Inventory_Add(0,
//          0x56, 1): taken, Sound_PlayEffect(0x106) and Msg_OpenSystem(2),
//          else Msg_OpenSystem(3); Field_Request = 2, +0xB = 1.
// Both clear the cell (0x5728D0) and answer 1; anything else answers 0.
//
// As the original has it: x and z are passed on as the dwords it was given
// (every callee reads their low words); the original also keeps the amount
// in the low byte of its own z argument's slot, which no caller reads again;
// Inventory_Add is pushed a fourth dword, 0, which it does not read.
FH_EXPORT unsigned char __cdecl Field_CellPickup(unsigned x, unsigned z) {
    const unsigned char cell = Al(g.byte_at(x, z));
    if (cell == 0xF2) {
        if (Al(g.effect_free()) != 0xFF) {
            g.spawn_at_cell(0, x, z);
            const unsigned roll = Al(static_cast<std::uint32_t>(g.rand())) & 0xF;
            if (roll >= 0xD) {
                unsigned char amount = roll < 0xF ? 2 : 5;
                if ((Field_InputFlags & 6) != 0 && (Al(static_cast<std::uint32_t>(g.rand())) & 3) == 0)
                    amount = static_cast<unsigned char>(amount * 10);
                g.found_zenny(amount);
                g.spawn_at_cell(1, x, z);
            }
            Sprite_Current[0xB] = 1;
        }
    } else if (cell == 0xF8) {
        g.spawn_at_cell(0, x, z);
        const unsigned char* const name = g.item_name(0, 0x56);
        for (unsigned k = 0; k < 16; k += 4) SetLong(At(::bof3::Img(bof3::addr::Text_Records) + k), Long(name + k));
        if (Al(g.inventory_add(0, 0x56, 1)) != 0) {
            g.play_effect(0x106);
            g.msg_open(2);
        } else {
            g.msg_open(3);
        }
        Field_Request = 2;
        Sprite_Current[0xB] = 1;
    } else {
        return 0;
    }
    g.clear_cell(x, z);
    return 1;
}

extern "C" void __cdecl PartyFormAction5_Form1(void) { ByState(R1B_TABLE(PartyFormAction5_Form1States), "PartyFormAction5_Form1 (0x51ECF0)"); }

extern "C" void __cdecl PartyAction5_Form1(void) { ByState(R1B_TABLE(PartyAction5_Form1States), "PartyAction5_Form1 (0x51ED10)"); }

extern "C" void __cdecl PartyAction5_Form1State0(void) { ByStep(R1B_TABLE(PartyAction5_Form1State0Steps), "PartyAction5_Form1State0 (0x51ED30)"); }

// original 0x51ED50 (0xA0 bytes), set 5's form 1, state 0 step 0: the same
// turn; the form's sound; the pose (+8 - 1) / 2 + 0x42; +0xB = 0, +6 = 0, +0xA
// = 8, +3 = 1.
extern "C" void __cdecl PartyAction5_Form1Aim(void) {
    TurnUntil([] { return SH_CALL(PartyAction_BlockedAhead)(); });
    FormSound(S());
    SH_CALL(Sprite_EnsureAnimation)(static_cast<unsigned char>(HalfTurn(S()[8]) + 0x42));
    S()[0xB] = 0;
    S()[6] = 0;
    S()[0xA] = 8;
    S()[3] = 1;
}

// original 0x51EDF0 (0x140 bytes), set 5's form 1, state 0 step 1: its cell handler 0x51EF30.
extern "C" void __cdecl PartyAction5_Form1Hit(void) {
    Hit([](unsigned x, unsigned z) { return SH_CALL(PartyAction5_CellHit)(x, z); }, false, "PartyAction5_Form1Hit (0x51EDF0)");
}

extern "C" unsigned char __cdecl PartyAction5_CellHit(unsigned x, unsigned z) { return CellHit(x, z); }

extern "C" void __cdecl PartyAction5_Form1State1(void) { ByStep(R1B_TABLE(PartyAction5_Form1State1Steps), "PartyAction5_Form1State1 (0x51F0C0)"); }

// original 0x51F0E0 (0x46 bytes), set 5's form 1, state 1 step 0 (and a cell
// of another set's table, 0x65FE88): the form's sound, the pose (+8 - 1) / 2 +
// 0x42, +0xA = 8, +3 = 1.
extern "C" void __cdecl PartyAction5_Form1Again(void) {
    FormSound(S());
    SH_CALL(Sprite_EnsureAnimation)(static_cast<unsigned char>(HalfTurn(S()[8]) + 0x42));
    S()[0xA] = 8;
    S()[3] = 1;
}

// original 0x51F130 (0x3C bytes), set 5's form 1, state 1 step 2 (and
// 0x65FE90): the effect object +0xB names free (+0) or in state 1 (+1), and
// Field_Kind2Hold 0: Field_State +0x137 = 0. Then Sprite_ScriptTick (a tail
// jump in the original; its answer is no caller's).
extern "C" void __cdecl PartyAction5_Form1Wait(void) {
    const unsigned char* const e = EffectAt(S()[0xB], "PartyAction5_Form1Wait (0x51F130)");
    if ((e[0] == 0 || e[1] == 1) && Field_Kind2Hold == 0) Field_State[0x137] = 0;
    SH_CALL(Sprite_ScriptTick)();
}

extern "C" void __cdecl PartyFormAction5_Form2(void) { ByState(R1B_TABLE(PartyFormAction5_Form2States), "PartyFormAction5_Form2 (0x51F170)"); }

extern "C" void __cdecl PartyFormAction5_ByForm(void) { ByForm(R1B_TABLE(PartyFormAction5_Forms), "PartyFormAction5_ByForm (0x51F190)"); }

// original 0x51F1B0: party set 5's action - the entry of PartyAction5_Forms
// 0x65FC18 that the u16 +0x2C picks (0x51E910, PartyAction5_Form1, PartyAction_NoAction), jumped
// to, the index unchecked.
FH_EXPORT void __cdecl PartyAction5_ByForm(void) { Entry(::bof3::Img(PartyAction5_Forms_addr), rv::Current<records::PartyMember>()->column)(); }

extern "C" void __cdecl PartyFormAction6_Form0(void) { ByState(R1B_TABLE(PartyFormAction6_Form0States), "PartyFormAction6_Form0 (0x51F1D0)"); }

extern "C" void __cdecl PartyAction6_Form0(void) { ByState(R1B_TABLE(PartyAction6_Form0States), "PartyAction6_Form0 (0x51F1F0)"); }

// original 0x51F210: set 6's action, form 0, state 0 (PartyAction5_Form0Begin's
// code): the first turn by PartyAction_TargetAhead; the slope one step ahead
// steep - Sprite_EnsureAnimation(the turn + 0x46), +2 one on - or not: +0x2B =
// 1, the side probes 3 and 5, Sound_PlayEffect(u16 +0x2C + 0x100),
// Sprite_EnsureAnimation(the turn + 0x42), +0xA = 5. Then +0xB = 0, +2 one on.
R1C_EXPORT void __cdecl PartyAction6_Form0Begin(void) { Form0BeginRest1c(); }

// original 0x51F3D0: set 6's action, form 0, state 1 (PartyAction5_Form0Resolve's
// code) with set 6's pickup.
R1C_EXPORT void __cdecl PartyAction6_Form0Resolve(void) {
    Form0ResolveRest1c(SH_CALL(PartyAction6_CellPickup), "PartyAction6_Form0Resolve (0x51F3D0)");
}

// original 0x51F4B0: set 6's cell pickup (Field_CellPickup's code; the zenny's
// tenfold), called by PartyAction6_Form0Resolve.
R1C_EXPORT unsigned char __cdecl PartyAction6_CellPickup(unsigned x, unsigned z) { return CellPickupRest1c(x, z, 10); }

// original 0x51F5D0: set 6's form action, form 1 - PartyFormAction6_Form1States 0x65FC3C by +2.
R1C_EXPORT void __cdecl PartyFormAction6_Form1(void) { ByStateRest1c(::bof3::Img(PartyFormAction6_Form1States_addr), "PartyFormAction6_Form1 (0x51F5D0)"); }

// original 0x51F5F0: set 6's action, form 1 - PartyAction6_Form1States 0x65FC48 by +2.
R1C_EXPORT void __cdecl PartyAction6_Form1(void) { ByStateRest1c(::bof3::Img(PartyAction6_Form1States_addr), "PartyAction6_Form1 (0x51F5F0)"); }

// original 0x51F610: set 6's form action, form 2 - PartyFormAction6_Form2States 0x65FC54 by +2.
R1C_EXPORT void __cdecl PartyFormAction6_Form2(void) { ByStateRest1c(::bof3::Img(PartyFormAction6_Form2States_addr), "PartyFormAction6_Form2 (0x51F610)"); }

// original 0x51F630: set 6's action, form 2 - PartyAction6_Form2States 0x65FC60 by +2.
R1C_EXPORT void __cdecl PartyAction6_Form2(void) { ByStateRest1c(::bof3::Img(PartyAction6_Form2States_addr), "PartyAction6_Form2 (0x51F630)"); }

// original 0x51F650: its state 0 - PartyAction6_Form2State0Steps 0x65FC68 by +3.
R1C_EXPORT void __cdecl PartyAction6_Form2State0(void) { ByStepRest1c(::bof3::Img(PartyAction6_Form2State0Steps_addr), "PartyAction6_Form2State0 (0x51F650)"); }

// original 0x51F670: set 6's action, form 2, state 0, step 0 - BlockedBegin above.
R1C_EXPORT void __cdecl PartyAction6_Form2Begin(void) { BlockedBegin(); }

// original 0x51F700: set 6's action, form 2, state 0, step 1, with set 6's cell hit.
R1C_EXPORT void __cdecl PartyAction6_Form2Resolve(void) {
    BlockedResolve(SH_CALL(PartyAction6_CellHit), "PartyAction6_Form2Resolve (0x51F700)");
}

// original 0x51F850 (nine .data cells, the step tables of sets 2, 5, 6, 9 and
// more): Sprite_ScriptTickOnce; when it answers non-zero,
// Sprite_SetAnimation(+8) and +3 one on.
R1C_EXPORT void __cdecl PartyAction_TickThenFace(void) {
    if (SH_CALL(Sprite_ScriptTickOnce)() == 0) return;
    SH_CALL(Sprite_SetAnimation)(Sprite_Current[8]);
    ++Sprite_Current[3];
}

// original 0x51F880: set 6's cell hit (CellHitRest1c above), called by
// PartyAction6_Form2Resolve.
R1C_EXPORT unsigned char __cdecl PartyAction6_CellHit(unsigned x, unsigned z) { return CellHitRest1c(x, z); }

// original 0x51FA10: its state 1 - PartyAction6_Form2State1Steps 0x65FC7C by +3.
R1C_EXPORT void __cdecl PartyAction6_Form2State1(void) { ByStepRest1c(::bof3::Img(PartyAction6_Form2State1Steps_addr), "PartyAction6_Form2State1 (0x51FA10)"); }

// original 0x51FA30 (code in no list): set 6's action, form 2, state 1, step 0 -
// PartyAction_SideProbes, Sprite_EnsureAnimation(the turn + 0x42), +0xA = 0xB,
// +3 = 1.
R1C_EXPORT void __cdecl PartyAction6_Form2Probe(void) {
    SH_CALL(PartyAction_SideProbes)();
    SH_CALL(Sprite_EnsureAnimation)(Turned(Sprite_Current[8], 0x42));
    Sprite_Current[0xA] = 0xB;
    Sprite_Current[3] = 1;
}

// original 0x51FA70: Field_FormActions[6] - PartyFormAction6_Forms 0x65FC88 by u16 +0x2C.
R1C_EXPORT void __cdecl PartyFormAction6_ByForm(void) { ByFormRest1c(::bof3::Img(PartyFormAction6_Forms_addr), "PartyFormAction6_ByForm (0x51FA70)"); }

// original 0x51FA90: Field_ActionBySet[6] - PartyAction6_Forms 0x65FC94 by u16 +0x2C.
R1C_EXPORT void __cdecl PartyAction6_ByForm(void) { ByFormRest1c(::bof3::Img(PartyAction6_Forms_addr), "PartyAction6_ByForm (0x51FA90)"); }

// original 0x51FAB0: set 7's form action, form 0 - 0x65FCA0 by +2.
R1C_EXPORT void __cdecl PartyFormAction7_Form0(void) { ByStateRest1c(::bof3::Img(PartyFormAction7_Form0States_addr), "PartyFormAction7_Form0 (0x51FAB0)"); }

// original 0x51FAD0: set 7's action, form 0 - 0x65FCAC by +2.
R1C_EXPORT void __cdecl PartyAction7_Form0(void) { ByStateRest1c(::bof3::Img(PartyAction7_Form0States_addr), "PartyAction7_Form0 (0x51FAD0)"); }

// original 0x51FAF0: set 7's action, form 0, state 0 - Kind30Begin above.
R1C_EXPORT void __cdecl PartyAction7_Form0Begin(void) { Kind30Begin("PartyAction7_Form0Begin (0x51FAF0)"); }

// original 0x51FC60: set 7's form action, form 1 - 0x65FCB4 by +2.
R1C_EXPORT void __cdecl PartyFormAction7_Form1(void) { ByStateRest1c(::bof3::Img(PartyFormAction7_Form1States_addr), "PartyFormAction7_Form1 (0x51FC60)"); }

// original 0x51FC80 (31 .data cells: state 1 of most form actions): at the side
// (+8 equal to +3): Sprite_EnsureAnimation(0x40) at 3, (0x41) at 5, then +2 one
// on; else +9 one down, and at 0: +9 = 2, +8 = (+8 + the sense +0xB) & 7,
// Sprite_EnsureAnimation(+8).
R1C_EXPORT void __cdecl PartyAction_TurnStep(void) {
    unsigned char* s = Sprite_Current;
    const unsigned char side = s[3];
    if (s[8] == side) {
        if (side == 3 || side == 5) {
            SH_CALL(Sprite_EnsureAnimation)(side == 3 ? 0x40 : 0x41);
            s = Sprite_Current;
        }
        ++s[2];
        return;
    }
    s[9] = static_cast<unsigned char>(s[9] - 1);
    if (Sprite_Current[9] != 0) return;
    Sprite_Current[9] = 2;
    Sprite_Current[8] = static_cast<unsigned char>(Sprite_Current[8] + Sprite_Current[0xB]);
    Sprite_Current[8] = static_cast<unsigned char>(Sprite_Current[8] & 7);
    SH_CALL(Sprite_EnsureAnimation)(Sprite_Current[8]);
}

// original 0x51FD00: set 7's form action, form 2 - 0x65FCC0 by +2.
R1C_EXPORT void __cdecl PartyFormAction7_Form2(void) { ByStateRest1c(::bof3::Img(PartyFormAction7_Form2States_addr), "PartyFormAction7_Form2 (0x51FD00)"); }

// original 0x51FD20: set 7's action, form 2 - 0x65FCCC by +2.
R1C_EXPORT void __cdecl PartyAction7_Form2(void) { ByStateRest1c(::bof3::Img(PartyAction7_Form2States_addr), "PartyAction7_Form2 (0x51FD20)"); }

// original 0x51FD40: set 7's action, form 2, state 0 - the steep test on the
// ground's rise one step ahead (Form2Begin above).
R1C_EXPORT void __cdecl PartyAction7_Form2Begin(void) { Form2Begin(); }

// original 0x51FF20: set 7's action, form 2, state 1, with set 7's pickup.
R1C_EXPORT void __cdecl PartyAction7_Form2Resolve(void) {
    Form0ResolveRest1c(SH_CALL(PartyAction7_CellPickup), "PartyAction7_Form2Resolve (0x51FF20)");
}

// original 0x520000: set 7's - the zenny's twentyfold.
R1C_EXPORT unsigned char __cdecl PartyAction7_CellPickup(unsigned x, unsigned z) { return CellPickupRest1c(x, z, 20); }

// original 0x520120: Field_FormActions[7] - PartyFormAction7_Forms 0x65FCD8.
R1C_EXPORT void __cdecl PartyFormAction7_ByForm(void) { ByFormRest1c(::bof3::Img(PartyFormAction7_Forms_addr), "PartyFormAction7_ByForm (0x520120)"); }

// original 0x520140: Field_ActionBySet[7] - PartyAction7_Forms 0x65FCE4.
R1C_EXPORT void __cdecl PartyAction7_ByForm(void) { ByFormRest1c(::bof3::Img(PartyAction7_Forms_addr), "PartyAction7_ByForm (0x520140)"); }

// original 0x520160: set 8's form action, form 0 - 0x65FCF0 by +2.
R1C_EXPORT void __cdecl PartyFormAction8_Form0(void) { ByStateRest1c(::bof3::Img(PartyFormAction8_Form0States_addr), "PartyFormAction8_Form0 (0x520160)"); }

// original 0x520180: set 8's action, form 0 - 0x65FCFC by +2.
R1C_EXPORT void __cdecl PartyAction8_Form0(void) { ByStateRest1c(::bof3::Img(PartyAction8_Form0States_addr), "PartyAction8_Form0 (0x520180)"); }

// original 0x5201A0: set 8's - 0x51FAF0's code.
R1C_EXPORT void __cdecl PartyAction8_Form0Begin(void) { Kind30Begin("PartyAction8_Form0Begin (0x5201A0)"); }

// original 0x520310: set 8's form action, form 1 - 0x65FD04 by +2.
R1C_EXPORT void __cdecl PartyFormAction8_Form1(void) { ByStateRest1c(::bof3::Img(PartyFormAction8_Form1States_addr), "PartyFormAction8_Form1 (0x520310)"); }

// original 0x520330: set 8's action, form 1 - 0x65FD10 by +2.
R1C_EXPORT void __cdecl PartyAction8_Form1(void) { ByStateRest1c(::bof3::Img(PartyAction8_Form1States_addr), "PartyAction8_Form1 (0x520330)"); }

// original 0x520350 (seven .data cells: state 2 of form 1 in sets 1, 2, 5 and
// 8 and more): with +7 set, Sprite_ScriptTickOnce and, when it answers
// non-zero, Sprite_EnsureAnimation(+8), +7 = 0; else Sprite_ScriptTickOnce and,
// when it answers non-zero and effect object +0xB is no longer in use (+0 0),
// +0x2B = 0 and Field_State +0x137 = 0.
//
// As the original has it: the effect index +0xB is read unchecked - the state
// before (R1B's 0x51DE20, and its likes) stores Effect_FindFree's answer there,
// 0xFF when no record is free, and moves on all the same; the byte 0xFF * 0x80
// past Effect_Objects is then read (docs section 5). Reproduced in place: the
// read does not fault and ordinary play can reach it.
R1C_EXPORT void __cdecl PartyAction_WaitEffectEnd(void) {
    if (Sprite_Current[7] != 0) {
        if (SH_CALL(Sprite_ScriptTickOnce)() == 0) return;
        SH_CALL(Sprite_EnsureAnimation)(Sprite_Current[8]);
        Sprite_Current[7] = 0;
        return;
    }
    if (SH_CALL(Sprite_ScriptTickOnce)() == 0) return;
    unsigned char* const s = Sprite_Current;
    if (Effect_Objects[static_cast<U>(s[0xB])].flags != 0) return;
    s[0x2B] = 0;
    Field_State[0x137] = 0;
}

// original 0x5203C0: set 8's form action, form 2 - 0x65FD1C by +2.
R1C_EXPORT void __cdecl PartyFormAction8_Form2(void) { ByStateRest1c(::bof3::Img(PartyFormAction8_Form2States_addr), "PartyFormAction8_Form2 (0x5203C0)"); }

// original 0x5203E0: set 8's action, form 2 - 0x65FD28 by +2.
R1C_EXPORT void __cdecl PartyAction8_Form2(void) { ByStateRest1c(::bof3::Img(PartyAction8_Form2States_addr), "PartyAction8_Form2 (0x5203E0)"); }

// original 0x520400: set 8's action, form 2, state 0 - 0x51FD40's code.
R1C_EXPORT void __cdecl PartyAction8_Form2Begin(void) { Form2Begin(); }

// original 0x5205E0: set 8's action, form 2, state 1, with set 8's pickup.
R1C_EXPORT void __cdecl PartyAction8_Form2Resolve(void) {
    Form0ResolveRest1c(SH_CALL(PartyAction8_CellPickup), "PartyAction8_Form2Resolve (0x5205E0)");
}

// original 0x5206C0: set 8's - the zenny's twentyfold.
R1C_EXPORT unsigned char __cdecl PartyAction8_CellPickup(unsigned x, unsigned z) { return CellPickupRest1c(x, z, 20); }

// original 0x5207E0: Field_FormActions[8] - PartyFormAction8_Forms 0x65FD34.
R1C_EXPORT void __cdecl PartyFormAction8_ByForm(void) { ByFormRest1c(::bof3::Img(PartyFormAction8_Forms_addr), "PartyFormAction8_ByForm (0x5207E0)"); }

// original 0x520800: Field_ActionBySet[8] - PartyAction8_Forms 0x65FD40.
R1C_EXPORT void __cdecl PartyAction8_ByForm(void) { ByFormRest1c(::bof3::Img(PartyAction8_Forms_addr), "PartyAction8_ByForm (0x520800)"); }

// original 0x520820: set 9's form action, form 0 (R1D's 0x521320 by +0x2C) - 0x65FD4C by +2.
R1C_EXPORT void __cdecl PartyFormAction9_Form0(void) { ByStateRest1c(::bof3::Img(PartyFormAction9_Form0States_addr), "PartyFormAction9_Form0 (0x520820)"); }

// original 0x520840 (34 .data cells: state 0 of most form actions): the side
// the direction +8 is nearer, 3 or 5 (|d - 3| against |d - 5| as ints; a tie
// is 3): +0xB = Sprite_TurnSense(that side), +3 = it; then +9 = 2, +2 one on.
R1C_EXPORT void __cdecl PartyAction_TurnToSide(void) {
    const int d = Sprite_Current[8];
    const int to3 = d - 3 < 0 ? 3 - d : d - 3;
    const int to5 = d - 5 < 0 ? 5 - d : d - 5;
    const unsigned char side = to3 > to5 ? 5 : 3;
    const unsigned char sense = SH_CALL(Sprite_TurnSense)(side);
    Sprite_Current[0xB] = sense;
    Sprite_Current[3] = side;
    Sprite_Current[9] = 2;
    ++Sprite_Current[2];
}

// original 0x5208B0: set 9's action, form 0 (R1D's 0x521340) - 0x65FD58 by +2.
R1C_EXPORT void __cdecl PartyAction9_Form0(void) { ByStateRest1c(::bof3::Img(PartyAction9_Form0States_addr), "PartyAction9_Form0 (0x5208B0)"); }

// original 0x5208D0: set 9's - 0x51FAF0's code.
R1C_EXPORT void __cdecl PartyAction9_Form0Begin(void) { Kind30Begin("PartyAction9_Form0Begin (0x5208D0)"); }

// original 0x520A40: set 9's form action, form 1 - 0x65FD60 by +2.
R1C_EXPORT void __cdecl PartyFormAction9_Form1(void) { ByStateRest1c(::bof3::Img(PartyFormAction9_Form1States_addr), "PartyFormAction9_Form1 (0x520A40)"); }

// original 0x520A60: set 9's action, form 1 - 0x65FD6C by +2.
R1C_EXPORT void __cdecl PartyAction9_Form1(void) { ByStateRest1c(::bof3::Img(PartyAction9_Form1States_addr), "PartyAction9_Form1 (0x520A60)"); }

// original 0x520A80: its state 0 - PartyAction9_Form1State0Steps 0x65FD74 by +3.
R1C_EXPORT void __cdecl PartyAction9_Form1State0(void) { ByStepRest1c(::bof3::Img(PartyAction9_Form1State0Steps_addr), "PartyAction9_Form1State0 (0x520A80)"); }

// original 0x520AA0: set 9's action, form 1, state 0, step 0 - 0x51F670's code.
R1C_EXPORT void __cdecl PartyAction9_Form1Begin(void) { BlockedBegin(); }

// original 0x520B30: set 9's action, form 1, state 0, step 1, with set 9's cell hit.
R1C_EXPORT void __cdecl PartyAction9_Form1Resolve(void) {
    BlockedResolve(SH_CALL(PartyAction9_CellHit), "PartyAction9_Form1Resolve (0x520B30)");
}

// original 0x520C80: set 9's - 0x51F880's code.
R1C_EXPORT unsigned char __cdecl PartyAction9_CellHit(unsigned x, unsigned z) { return CellHitRest1c(x, z); }

// The states with steps: by the byte +3.
extern "C" void __cdecl PartyAction9_Form1State1(void) {
    Jump(::bof3::Img(PartyAction9_Form1State1Steps_addr), 3, Sprite_Current[3], "PartyAction9_Form1State1 (0x520E10)");
}

// original 0x520E30 (0x35 bytes), PartyAction9_Form1State1Steps[0]:
// PartyAction_SideProbes, Sprite_EnsureAnimation((+8 - 1) / 2 + 0x42), +0xA =
// 0xB, +3 = 1.
extern "C" void __cdecl PartyAction9_Form1Start(void) {
    SH_CALL(PartyAction_SideProbes)();
    SH_CALL(Sprite_EnsureAnimation)(Pose(Sprite_Current[8], 0x42));
    Sprite_Current[0xA] = 0xB;
    Sprite_Current[3] = 1;
}

// The forms: by the state byte +2.
extern "C" void __cdecl PartyFormAction9_Form2(void) { Jump(::bof3::Img(PartyFormAction9_Form2States_addr), 3, Sprite_Current[2], "PartyFormAction9_Form2 (0x520E70)"); }

// original 0x520E90 (0x87 bytes), entry 0 of eleven state tables: Cond_ByteFA
// 0xF - Field_State +0x137 = 0 and nothing else. Otherwise, d = +8: the target
// side is 5 when |d - 3| > |d - 5| (signed, cdq), else 3; +0xB =
// Sprite_TurnSense(target), +3 = target, +9 = 2, +2 one on.
extern "C" void __cdecl PartyFormAction_TurnToSide(void) {
    if (static_cast<unsigned char>(Cond_ByteFA) == 0xF) {
        Field_State[0x137] = 0;
        return;
    }
    const int d = Sprite_Current[8];
    const unsigned char target = std::abs(d - 3) > std::abs(d - 5) ? 5 : 3;
    const unsigned char sense = SH_CALL(Sprite_TurnSense)(target);
    Sprite_Current[0xB] = sense;
    Sprite_Current[3] = target;
    Sprite_Current[9] = 2;
    ++Sprite_Current[2];
}

extern "C" void __cdecl PartyAction9_Form2(void) { Jump(::bof3::Img(PartyAction9_Form2States_addr), 3, Sprite_Current[2], "PartyAction9_Form2 (0x520F20)"); }

// originals 0x520F40 / 0x521600 / 0x521CC0 (0x1D4 bytes each, the same
// instructions): FormBegin above.
extern "C" void __cdecl PartyAction9_Form2Begin(void) { FormBegin(); }

// originals 0x521120 / 0x5217E0 / 0x521EA0 (0xDB bytes each): FormResolve,
// each calling its own set's pickup (its E8 sites at +0x8F, +0xA5, +0xB9).
extern "C" void __cdecl PartyAction9_Form2Resolve(void) {
    FormResolve(SH_CALL(PartyAction9_CellPickup), "PartyAction9_Form2Resolve (0x521120)");
}

// originals 0x521200 / 0x5218C0 / 0x521F80 (0x11F bytes each): CellPickupRest1d.
extern "C" unsigned char __cdecl PartyAction9_CellPickup(unsigned x, unsigned z) { return CellPickupRest1d(x, z); }

extern "C" void __cdecl PartyFormAction9_ByForm(void) {
    Jump(::bof3::Img(PartyFormAction9_Forms_addr), 3, rv::Current<records::PartyMember>()->column, "PartyFormAction9_ByForm (0x521320)");
}

// Field_ActionBySet's and Field_FormActions' entries for sets 9..12: by the u16
// form word +0x2C.
extern "C" void __cdecl PartyAction9_ByForm(void) { Jump(::bof3::Img(PartyAction9_Forms_addr), 3, rv::Current<records::PartyMember>()->column, "PartyAction9_ByForm (0x521340)"); }

extern "C" void __cdecl PartyFormAction10_Form0(void) { Jump(::bof3::Img(PartyFormAction10_Form0States_addr), 3, Sprite_Current[2], "PartyFormAction10_Form0 (0x521360)"); }

extern "C" void __cdecl PartyAction10_Form0(void) { Jump(::bof3::Img(PartyAction10_Form0States_addr), 2, Sprite_Current[2], "PartyAction10_Form0 (0x521380)"); }

// originals 0x5213A0 / 0x521AD0 (0x166 bytes each): Kind30BeginRest1d.
extern "C" void __cdecl PartyAction10_Form0Begin(void) { Kind30BeginRest1d("PartyAction10_Form0Begin (0x5213A0)"); }

// original 0x521510 (0xAA bytes): 0xFF unless Field_State +0x89 is 2 and its
// +0x138 bit 0 clear. The point one step ahead of Sprite_Current, read once;
// then each of the 20 Effect_Objects records in use, of kind 0x30, that
// Sprite_PointInReach(point, Sprite_Current's height word, 1, record) answers
// non-zero for: its index when its x equals Sprite_Current's x, or its z
// Sprite_Current's z (Sprite_Current re-read after each call, as the
// original's `mov ecx, [Sprite_Current]` there); else on. 0xFF at the end.
extern "C" unsigned char __cdecl PartyAction_Kind30Ahead(void) {
    const unsigned char* const state = Field_State;
    if (state[0x89] != 2 || (state[0x138] & 1) != 0) return 0xFF;
    const unsigned char* const s = Sprite_Current;
    const unsigned d = s[8];
    const U x = static_cast<U>(rv::As<records::PartyMember>(s)->x) + StepXRest0a(d);
    const U z = static_cast<U>(rv::As<records::PartyMember>(s)->z) + StepZRest0a(d);
    for (unsigned i = 0; i < kEffectCount; ++i) {
        const unsigned char* const e = rv::Bytes(&Effect_Objects[i]);
        if (e[0] == 0 || e[5] != 0x30) continue;
        if (SH_CALL(Sprite_PointInReach)(static_cast<int>(x), static_cast<int>(z),
                                         static_cast<short>(rv::As<records::PartyMember>(Sprite_Current)->height_whole), 1, e) == 0)
            continue;
        if (rv::As<records::EffectObject>(e)->x == rv::Current<records::PartyMember>()->x) return static_cast<unsigned char>(i);
        if (rv::As<records::EffectObject>(e)->z == rv::Current<records::PartyMember>()->z) return static_cast<unsigned char>(i);
    }
    return 0xFF;
}

extern "C" void __cdecl PartyFormAction10_Form1(void) { Jump(::bof3::Img(PartyFormAction10_Form1States_addr), 3, Sprite_Current[2], "PartyFormAction10_Form1 (0x5215C0)"); }

extern "C" void __cdecl PartyAction10_Form1(void) { Jump(::bof3::Img(PartyAction10_Form1States_addr), 3, Sprite_Current[2], "PartyAction10_Form1 (0x5215E0)"); }

extern "C" void __cdecl PartyAction10_Form1Begin(void) { FormBegin(); }

extern "C" void __cdecl PartyAction10_Form1Resolve(void) {
    FormResolve(SH_CALL(PartyAction10_CellPickup), "PartyAction10_Form1Resolve (0x5217E0)");
}

extern "C" unsigned char __cdecl PartyAction10_CellPickup(unsigned x, unsigned z) { return CellPickupRest1d(x, z); }

extern "C" void __cdecl PartyFormAction10_Form2(void) { Jump(::bof3::Img(PartyFormAction10_Form2States_addr), 3, Sprite_Current[2], "PartyFormAction10_Form2 (0x5219E0)"); }

extern "C" void __cdecl PartyAction10_Form2(void) { Jump(::bof3::Img(PartyAction10_Form2States_addr), 2, Sprite_Current[2], "PartyAction10_Form2 (0x521A00)"); }

// original 0x521A20 (0x2A bytes), entry 1 after 0x5252B0 in seven tables:
// Field_State +0x137 = 0 when effect object +0xB's in-use byte is 0; then a
// tail jmp to Sprite_ScriptTick. The index is read unchecked: 0x5252B0 stores
// Effect_FindFree's answer there, 0xFF when no record is free, and the
// original then reads 0x7E9160 (Gfx_PacketPools); ours reads it in place too
// (docs/rest_1d.md section 5, L1).
extern "C" void __cdecl PartyAction_WaitEffectDone(void) {
    const unsigned index = Sprite_Current[0xB];
    if (At(::bof3::Img(Effect_Objects_addr) + index * kEffectStride)[0] == 0) Field_State[0x137] = 0;
    SH_CALL(Sprite_ScriptTick)();
}

extern "C" void __cdecl PartyFormAction10_ByForm(void) {
    Jump(::bof3::Img(PartyFormAction10_Forms_addr), 3, rv::Current<records::PartyMember>()->column, "PartyFormAction10_ByForm (0x521A50)");
}

extern "C" void __cdecl PartyAction10_ByForm(void) {
    Jump(::bof3::Img(PartyAction10_Forms_addr), 3, rv::Current<records::PartyMember>()->column, "PartyAction10_ByForm (0x521A70)");
}

extern "C" void __cdecl PartyFormAction11_Form0(void) { Jump(::bof3::Img(PartyFormAction11_Form0States_addr), 3, Sprite_Current[2], "PartyFormAction11_Form0 (0x521A90)"); }

extern "C" void __cdecl PartyAction11_Form0(void) { Jump(::bof3::Img(PartyAction11_Form0States_addr), 2, Sprite_Current[2], "PartyAction11_Form0 (0x521AB0)"); }

extern "C" void __cdecl PartyAction11_Form0Begin(void) { Kind30BeginRest1d("PartyAction11_Form0Begin (0x521AD0)"); }

// original 0x521C40 (0x33 bytes), eight tables: +9 at 0 - Field_State +0x137 =
// 0 and Field_ScriptFlags &= 0xEFFF; else +9 one down and Field_LeaderStepTick.
// Then a tail jmp to Sprite_ScriptTick.
extern "C" void __cdecl PartyAction_StepCountdown(void) {
    unsigned char* const s = Sprite_Current;
    const unsigned char count = s[9];
    if (count == 0) {
        Field_State[0x137] = 0;
        Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags & 0xEFFF);
    } else {
        s[9] = static_cast<unsigned char>(count - 1);
        SH_CALL(Field_LeaderStepTick)();
    }
    SH_CALL(Sprite_ScriptTick)();
}

extern "C" void __cdecl PartyFormAction11_Form1(void) { Jump(::bof3::Img(PartyFormAction11_Form1States_addr), 3, Sprite_Current[2], "PartyFormAction11_Form1 (0x521C80)"); }

extern "C" void __cdecl PartyAction11_Form1(void) { Jump(::bof3::Img(PartyAction11_Form1States_addr), 3, Sprite_Current[2], "PartyAction11_Form1 (0x521CA0)"); }

extern "C" void __cdecl PartyAction11_Form1Begin(void) { FormBegin(); }

extern "C" void __cdecl PartyAction11_Form1Resolve(void) {
    FormResolve(SH_CALL(PartyAction11_CellPickup), "PartyAction11_Form1Resolve (0x521EA0)");
}

extern "C" unsigned char __cdecl PartyAction11_CellPickup(unsigned x, unsigned z) { return CellPickupRest1d(x, z); }

extern "C" void __cdecl PartyFormAction11_ByForm(void) {
    Jump(::bof3::Img(PartyFormAction11_Forms_addr), 3, rv::Current<records::PartyMember>()->column, "PartyFormAction11_ByForm (0x5220A0)");
}

extern "C" void __cdecl PartyAction11_ByForm(void) {
    Jump(::bof3::Img(PartyAction11_Forms_addr), 3, rv::Current<records::PartyMember>()->column, "PartyAction11_ByForm (0x5220C0)");
}

extern "C" void __cdecl PartyFormAction12_Form0(void) { Jump(::bof3::Img(PartyFormAction12_Form0States_addr), 3, Sprite_Current[2], "PartyFormAction12_Form0 (0x5220E0)"); }

extern "C" void __cdecl PartyAction12_Form0(void) { Jump(::bof3::Img(PartyAction12_Form0States_addr), 2, Sprite_Current[2], "PartyAction12_Form0 (0x522100)"); }

extern "C" void __cdecl PartyAction12_Form0State0(void) {
    Jump(::bof3::Img(PartyAction12_Form0State0Steps_addr), 5, Sprite_Current[3], "PartyAction12_Form0State0 (0x522120)");
}

// original 0x522140 (0xA0 bytes), PartyAction12_Form0State0Steps[0]: an even
// direction turned one eighth back; when PartyAction_BlockedAhead answers 0
// that way, two on, and when it answers 0 there too, back to the first turn.
// Sound_PlayEffect(+0x2C + 0x100), the pose (+8 - 1) / 2 + 0x42, then +0xB =
// 0, +6 = 0, +0xA = 8, +3 = 1.
extern "C" void __cdecl PartyAction12_Form0Begin(void) {
    unsigned char* const s = Sprite_Current;
    if ((s[8] & 1) == 0) {
        s[8] = static_cast<unsigned char>((s[8] - 1) & 7);
        if (SH_CALL(PartyAction_BlockedAhead)() == 0) {
            Sprite_Current[8] = static_cast<unsigned char>((Sprite_Current[8] + 2) & 7);
            if (SH_CALL(PartyAction_BlockedAhead)() == 0) Sprite_Current[8] = static_cast<unsigned char>((Sprite_Current[8] - 2) & 7);
        }
    }
    SH_CALL(Sound_PlayEffect)(SoundOfForm());
    SH_CALL(Sprite_EnsureAnimation)(Pose(Sprite_Current[8], 0x42));
    Sprite_Current[0xB] = 0;
    Sprite_Current[6] = 0;
    Sprite_Current[0xA] = 8;
    Sprite_Current[3] = 1;
}

// original 0x5221E0 (0x140 bytes), PartyAction12_Form0State0Steps[1]: with +0xA
// not 0, one down; when that reaches 0: Field_EffectAhead's object (the byte
// signed: movsx) gets +8 = the direction and +0xA = 1,
// Sound_PlayEffect(0x10B), +6 = its index and +3 one on - and one on again
// below; with none, the point two steps ahead: the object Sprite_ObjectAt(point,
// 0) finds gets +0x80 bit 0 (signed) and Sound_PlayEffect(0x10B), then the cells
// tried with PartyAction12_CellHit; +3 one on. Sprite_ScriptTickOnce every
// time.
extern "C" void __cdecl PartyAction12_Form0Resolve(void) {
    unsigned char* const s = Sprite_Current;
    const unsigned char count = s[0xA];
    if (count != 0) {
        s[0xA] = static_cast<unsigned char>(count - 1);
        if (Sprite_Current[0xA] == 0) {
            const unsigned char e = SH_CALL(Field_EffectAhead)();
            if (e != 0xFF) {
                unsigned char* const r = EffectChecked(static_cast<unsigned>(static_cast<int>(static_cast<signed char>(e))),
                                                       "PartyAction12_Form0Resolve (0x5221E0)");
                r[8] = Sprite_Current[8];
                r[0xA] = 1;
                SH_CALL(Sound_PlayEffect)(0x10B);
                Sprite_Current[6] = e;
                ++Sprite_Current[3];
            } else {
                U x, z;
                TwoAheadRest1d(x, z);
                const unsigned char found = SH_CALL(Sprite_ObjectAt)(static_cast<long>(x), static_cast<long>(z), 0);
                if (found != 0xFF) {
                    MarkObjectRest1d(found, true, "PartyAction12_Form0Resolve (0x5221E0)");
                    SH_CALL(Sound_PlayEffect)(0x10B);
                }
                TryCells(SH_CALL(PartyAction12_CellHit), x, z);
            }
            ++Sprite_Current[3];
        }
    }
    SH_CALL(Sprite_ScriptTickOnce)();
}

// original 0x522320 (0x188 bytes): AreaMap_ByteAt(x, z) -
//   0xF0, 0xF1, 0xF4 - Effect_SpawnAtCellHigh(0, x, z); Rand & 7 above 5: (4,
//          x, z) too; Sound_PlayEffect(0x10B); 1.
//   0xF6, 0xF7 - Effect_SpawnAtCellHigh(0, x, z), Sound_PlayEffect(0x10B); then
//          by Rand & 0xF: below 7 - (3, x, z), item 0x29's name (Item_NamePtr(0,
//          0x29), 16 bytes) into Text_Records, Inventory_Add(0, 0x29, 1): taken,
//          Sound_PlayEffect(0x106) and Msg_OpenSystem(2), else
//          Msg_OpenSystem(3); +0xB = 2. Above 0xB - (2, x, z),
//          Sprite_FlashClut(0), Char_LoseHp(1, Field_State +0x89 read after
//          it), Msg_OpenSystem(0xD9), +0xB = 1. 7..0xB - nothing more. Each:
//          Field_Request = 2; 1.
//   anything else - 0.
// The callers push a fourth dword to Effect_SpawnAtCellHigh (the cell's byte
// over the z argument's slot, or that slot), which it never reads.
extern "C" unsigned char __cdecl PartyAction12_CellHit(unsigned x, unsigned z) {
    const unsigned char cell = SH_CALL(AreaMap_ByteAt)(static_cast<short>(x), static_cast<short>(z));
    if (cell == 0xF0 || cell == 0xF1 || cell == 0xF4) {
        SH_CALL(Effect_SpawnAtCellHigh)(0, x, z);
        if ((static_cast<unsigned>(SH_CALL(Rand)()) & 7) > 5) SH_CALL(Effect_SpawnAtCellHigh)(4, x, z);
        SH_CALL(Sound_PlayEffect)(0x10B);
        return 1;
    }
    if (cell != 0xF6 && cell != 0xF7) return 0;
    SH_CALL(Effect_SpawnAtCellHigh)(0, x, z);
    SH_CALL(Sound_PlayEffect)(0x10B);
    const unsigned roll = static_cast<unsigned>(SH_CALL(Rand)()) & 0xF;
    if (roll < 7) {
        SH_CALL(Effect_SpawnAtCellHigh)(3, x, z);
        const unsigned char* const name = SH_CALL(Item_NamePtr)(0, 0x29);
        for (unsigned k = 0; k < 16; k += 4) move_script::SetLong(At(::bof3::Img(bof3::addr::Text_Records) + k), Long(name + k));
        if (SH_CALL(Inventory_Add)(0, 0x29, 1) != 0) {
            SH_CALL(Sound_PlayEffect)(0x106);
            SH_CALL(Msg_OpenSystem)(2);
        } else {
            SH_CALL(Msg_OpenSystem)(3);
        }
        Sprite_Current[0xB] = 2;
    } else if (roll > 0xB) {
        SH_CALL(Effect_SpawnAtCellHigh)(2, x, z);
        SH_CALL(Sprite_FlashClut)(0);
        const unsigned member = Field_State[0x89];
        SH_CALL(Char_LoseHp)(1, member);
        SH_CALL(Msg_OpenSystem)(0xD9);
        Sprite_Current[0xB] = 1;
    }
    Field_Request = 2;
    return 1;
}

extern "C" void __cdecl PartyAction12_Form0State1(void) {
    Jump(::bof3::Img(PartyAction12_Form0State1Steps_addr), 3, Sprite_Current[3], "PartyAction12_Form0State1 (0x5224B0)");
}

// original 0x5224D0 (0x81 bytes), PartyAction12_Form0State1Steps[1] (and
// 0x65FBF8's): Sprite_ScriptTickOnce answering not 0 - Sprite_EnsureAnimation(+8)
// and +3 = 2. Else with +0xA not 0, one down; reaching 0: effect object +0xB
// gets +8 = the direction; +7 = 1 when it is 0; the object (+0xB read again)
// gets +0xA = +7; Sound_PlayEffect(0x10B). +0xB is read unchecked; ours aborts
// at 20 and above (section 5, L2).
extern "C" void __cdecl PartyAction12_Form0EffectSet(void) {
    if (SH_CALL(Sprite_ScriptTickOnce)() != 0) {
        SH_CALL(Sprite_EnsureAnimation)(Sprite_Current[8]);
        Sprite_Current[3] = 2;
        return;
    }
    unsigned char* const s = Sprite_Current;
    const unsigned char count = s[0xA];
    if (count == 0) return;
    s[0xA] = static_cast<unsigned char>(count - 1);
    unsigned char* const c = Sprite_Current;
    if (c[0xA] != 0) return;
    const char* const who = "PartyAction12_Form0EffectSet (0x5224D0)";
    const unsigned char direction = c[8];
    EffectChecked(c[0xB], who)[8] = direction;
    if (c[7] == 0) c[7] = 1;
    const unsigned char* const t = Sprite_Current;
    const unsigned char seven = t[7];
    EffectChecked(t[0xB], who)[0xA] = seven;
    SH_CALL(Sound_PlayEffect)(0x10B);
}

// original 0x522560 (0xED bytes): the point two steps ahead; 1 when
// Sprite_ObjectAt(point, 0) is not 0xFF, Field_EffectAhead is not 0xFF, or
// AreaMap_ByteAt is 0xF0, 0xF1, 0xF4, 0xF6 or 0xF7 at its cell, at the cell
// one on in x (x with a fraction), or one on in z (z with one) - every call
// made whatever the earlier answered. The cells as PartyAction_TargetAhead's.
extern "C" unsigned char __cdecl PartyAction_BlockedAhead(void) {
    U x, z;
    TwoAhead(x, z);
    unsigned char blocked = 0;
    if (SH_CALL(Sprite_ObjectAt)(static_cast<long>(x), static_cast<long>(z), 0) != 0xFF) blocked = 1;
    if (SH_CALL(Field_EffectAhead)() != 0xFF) blocked = 1;
    if (MapBlocked(SH_CALL(AreaMap_ByteAt)(Cell(x), Cell(z)))) blocked = 1;
    if ((x & 0xFFFF) != 0 && MapBlocked(SH_CALL(AreaMap_ByteAt)(CellPlusOne(x), Cell(z)))) blocked = 1;
    if ((z & 0xFFFF) != 0 && MapBlocked(SH_CALL(AreaMap_ByteAt)(Cell(x), CellPlusOne(z)))) blocked = 1;
    return blocked;
}

extern "C" void __cdecl PartyFormAction12_Form1(void) { Jump(::bof3::Img(PartyFormAction12_Form1States_addr), 3, Sprite_Current[2], "PartyFormAction12_Form1 (0x522650)"); }

extern "C" void __cdecl PartyFormAction12_ByForm(void) {
    Jump(::bof3::Img(PartyFormAction12_Forms_addr), 3, rv::Current<records::PartyMember>()->column, "PartyFormAction12_ByForm (0x522670)");
}

extern "C" void __cdecl PartyAction12_ByForm(void) {
    Jump(::bof3::Img(PartyAction12_Forms_addr), 3, rv::Current<records::PartyMember>()->column, "PartyAction12_ByForm (0x522690)");
}

extern "C" void __cdecl PartyFormAction13_Form0(void) { Jump(::bof3::Img(PartyFormAction13_Form0States_addr), 3, Sprite_Current[2], "PartyFormAction13_Form0 (0x5226B0)"); }

// original 0x5226D0 (0xD bytes): Field_State +0x137 = 0.
extern "C" void __cdecl PartyAction_NoAction(void) { Field_State[0x137] = 0; }

extern "C" void __cdecl PartyFormAction13_Form1(void) { ByState2("PartyFormAction13_Form1 (0x5226E0)", Table(PartyFormAction13_Form1States), 3); }

extern "C" void __cdecl PartyAction13_Form1(void) { ByState2("PartyAction13_Form1 (0x522700)", Table(PartyAction13_Form1States), 3); }

extern "C" void __cdecl PartyFormAction13_Form2(void) { ByState2("PartyFormAction13_Form2 (0x522720)", Table(PartyFormAction13_Form2States), 3); }

extern "C" void __cdecl PartyAction13_Form2(void) { ByState2("PartyAction13_Form2 (0x522740)", Table(PartyAction13_Form2States), 3); }

// originals 0x522760, 0x5230D0, 0x523550 (0x1D4 bytes each).
extern "C" void __cdecl PartyAction13_Form2Begin(void) { Form2BeginRest1e(); }

// originals 0x522940, 0x5232B0, 0x523730 (0xDB bytes each).
extern "C" void __cdecl PartyAction13_Form2Resolve(void) {
    Form2Resolve("PartyAction13_Form2Resolve (0x522940)", &PartyAction13_CellPickup);
}

// originals 0x522A20, 0x523390, 0x523810 (0x11F bytes each). The original also
// keeps the amount in the low byte of its own z argument's slot (the caller's
// stack, which no caller reads again).
extern "C" unsigned char __cdecl PartyAction13_CellPickup(unsigned x, unsigned z) { return CellPickupRest1e(x, z); }

extern "C" void __cdecl PartyFormAction13_ByForm(void) { ByFormRest1e("PartyFormAction13_ByForm (0x522B40)", Table(PartyFormAction13_Forms)); }

extern "C" void __cdecl PartyAction13_ByForm(void) { ByFormRest1e("PartyAction13_ByForm (0x522B60)", Table(PartyAction13_Forms)); }

extern "C" void __cdecl PartyFormAction14_Form0(void) { ByState2("PartyFormAction14_Form0 (0x522B80)", Table(PartyFormAction14_Form0States), 3); }

extern "C" void __cdecl PartyFormAction14_Form1(void) { ByState2("PartyFormAction14_Form1 (0x522BA0)", Table(PartyFormAction14_Form1States), 3); }

extern "C" void __cdecl PartyAction14_Form1(void) { ByState2("PartyAction14_Form1 (0x522BC0)", Table(PartyAction14_Form1Modes), 2); }

extern "C" void __cdecl PartyAction14_Form1Mode0(void) { ByState3("PartyAction14_Form1Mode0 (0x522BE0)", Table(PartyAction14_Mode0States), 5); }

// originals 0x522C00, 0x523B40 (0x87 bytes each).
extern "C" void __cdecl PartyAction14_StrikeBegin(void) { StrikeBegin(); }

// originals 0x522C90, 0x523BD0 (0x14C bytes each).
extern "C" void __cdecl PartyAction14_Strike(void) { Strike("PartyAction14_Strike (0x522C90)", &PartyAction14_StrikeCell); }

// original 0x522DE0 (0x38 bytes): the effect object +6 names (the byte
// zero-extended, unchecked: ours aborts past the 20 records) free (+0 zero) or
// in state 1 (+1), and Field_Kind2Hold 0: +6 = 0, +3 = +3 - 2.
extern "C" void __cdecl PartyAction_StrikeWait(void) {
    unsigned char* const s = S();
    const unsigned char* const e = EffectAtRest1e("PartyAction_StrikeWait (0x522DE0)", s[6], "the sprite's +6");
    if (e[0] != 0 && e[1] != 1) return;
    if (Field_Kind2Hold != 0) return;
    s[6] = 0;
    S()[3] = static_cast<unsigned char>(S()[3] - 2);
}

// originals 0x522E20, 0x523D20 (0x188 bytes each). As CellPickupRest1e, the original
// stores the cell byte into the low byte of its z argument's slot.
extern "C" unsigned char __cdecl PartyAction14_StrikeCell(unsigned x, unsigned z) { return StrikeCell(x, z); }

// original 0x522FB0 (0x76 bytes): with Effect_FindFree's slot not 0xFF, the
// record gets +0 = 1, +5 = 0x34, +1 = the state's byte, +0x34 / +0x38 = the
// words x and z (movsx) shifted to 16.16, +0x3E = AreaMap_Elevation(x read
// back from the record, z) + 0x200 (its low word), +0xB = 1. The slot is used
// as FindFree answers it, unchecked (FindFree gives 0..19 or 0xFF), as
// Effect_SpawnAtCell uses it. The original's eax at its ret is no caller's.
extern "C" void __cdecl Effect_SpawnAtCellHigh(unsigned state, unsigned x, unsigned z) {
    const unsigned char slot = SH_CALL(Effect_FindFree)();
    if (slot == 0xFF) return;
    unsigned char* const e = rv::Bytes(Effect_Objects) + slot * kEffectStride;
    e[0] = 1;
    e[5] = 0x34;
    e[1] = static_cast<unsigned char>(state);
    rv::As<records::EffectObject>(e)->x = static_cast<std::int32_t>(static_cast<U>(static_cast<std::int32_t>(static_cast<short>(x))) << 16);
    const auto zz = static_cast<std::int32_t>(static_cast<U>(static_cast<std::int32_t>(static_cast<short>(z))) << 16);
    rv::As<records::EffectObject>(e)->z = zz;
    const long ground = SH_CALL(AreaMap_Elevation)(rv::As<records::EffectObject>(e)->x, zz);
    rv::As<records::EffectObject>(e)->height_whole = static_cast<unsigned>(ground) + 0x200;
    e[0xB] = 1;
}

extern "C" void __cdecl PartyAction14_Form1Mode1(void) { ByState3("PartyAction14_Form1Mode1 (0x523030)", Table(PartyAction14_Mode1States), 3); }

// original 0x523050 (0x35 bytes).
extern "C" void __cdecl PartyAction14_Mode1Begin(void) {
    SH_CALL(PartyAction_SideProbes)();
    SH_CALL(Sprite_EnsureAnimation)(static_cast<unsigned char>(HalfTurnRest1e(S()[8]) + 0x42));
    S()[0xA] = 0xB;
    S()[3] = 1;
}

extern "C" void __cdecl PartyFormAction14_Form2(void) { ByState2("PartyFormAction14_Form2 (0x523090)", Table(PartyFormAction14_Form2States), 3); }

extern "C" void __cdecl PartyAction14_Form2(void) { ByState2("PartyAction14_Form2 (0x5230B0)", Table(PartyAction14_Form2States), 3); }

extern "C" void __cdecl PartyAction14_Form2Begin(void) { Form2BeginRest1e(); }

extern "C" void __cdecl PartyAction14_Form2Resolve(void) {
    Form2Resolve("PartyAction14_Form2Resolve (0x5232B0)", &PartyAction14_CellPickup);
}

extern "C" unsigned char __cdecl PartyAction14_CellPickup(unsigned x, unsigned z) { return CellPickupRest1e(x, z); }

extern "C" void __cdecl PartyFormAction14_ByForm(void) { ByFormRest1e("PartyFormAction14_ByForm (0x5234B0)", Table(PartyFormAction14_Forms)); }

extern "C" void __cdecl PartyAction14_ByForm(void) { ByFormRest1e("PartyAction14_ByForm (0x5234D0)", Table(PartyAction14_Forms)); }

extern "C" void __cdecl PartyFormAction15_Form0(void) { ByState2("PartyFormAction15_Form0 (0x5234F0)", Table(PartyFormAction15_Form0States), 3); }

extern "C" void __cdecl PartyFormAction15_Form1(void) { ByState2("PartyFormAction15_Form1 (0x523510)", Table(PartyFormAction15_Form1States), 3); }

extern "C" void __cdecl PartyAction15_Form1(void) { ByState2("PartyAction15_Form1 (0x523530)", Table(PartyAction15_Form1States), 3); }

extern "C" void __cdecl PartyAction15_Form1Begin(void) { Form2BeginRest1e(); }

extern "C" void __cdecl PartyAction15_Form1Resolve(void) {
    Form2Resolve("PartyAction15_Form1Resolve (0x523730)", &PartyAction15_CellPickup);
}

extern "C" unsigned char __cdecl PartyAction15_CellPickup(unsigned x, unsigned z) { return CellPickupRest1e(x, z); }

extern "C" void __cdecl PartyFormAction15_Form2(void) { ByState2("PartyFormAction15_Form2 (0x523930)", Table(PartyFormAction15_Form2States), 3); }

extern "C" void __cdecl PartyAction15_Form2(void) { ByState2("PartyAction15_Form2 (0x523950)", Table(PartyAction15_Form2States), 2); }

extern "C" void __cdecl PartyFormAction15_ByForm(void) { ByFormRest1e("PartyFormAction15_ByForm (0x523970)", Table(PartyFormAction15_Forms)); }

extern "C" void __cdecl PartyAction15_ByForm(void) { ByFormRest1e("PartyAction15_ByForm (0x523990)", Table(PartyAction15_Forms)); }

extern "C" void __cdecl PartyFormAction16_Form0(void) { ByState2("PartyFormAction16_Form0 (0x5239B0)", Table(PartyFormAction16_Form0States), 3); }

extern "C" void __cdecl PartyAction16_Form0(void) { ByState2("PartyAction16_Form0 (0x5239D0)", Table(PartyAction16_Form0States), 3); }

// original 0x5239F0 (0xE7 bytes): an even +8 turned one eighth back (& 7, an
// odd one kept unmasked); +0x2B = 1 and the side probes in directions 3 and 5;
// Sprite_EnsureAnimation((+8 >> 1) + 0x42, a byte: shr cl, 1; add cl, 0x42 -
// the rest of the pushed ecx is Sprite_Current's, which the callee does not
// read); +2 one on.
extern "C" void __cdecl PartyAction_ProbeBegin(void) {
    unsigned char* const s = S();
    if ((s[8] & 1) == 0) s[8] = static_cast<unsigned char>((s[8] - 1) & 7);
    SideProbes();
    SH_CALL(Sprite_EnsureAnimation)(static_cast<unsigned char>((S()[8] >> 1) + 0x42));
    ++S()[2];
}

extern "C" void __cdecl PartyFormAction16_Form1(void) { ByState2("PartyFormAction16_Form1 (0x523AE0)", Table(PartyFormAction16_Form1States), 3); }

extern "C" void __cdecl PartyAction16_Form1(void) { ByState2("PartyAction16_Form1 (0x523B00)", Table(PartyAction16_Form1Modes), 2); }

extern "C" void __cdecl PartyAction16_Form1Mode0(void) { ByState3("PartyAction16_Form1Mode0 (0x523B20)", Table(PartyAction16_Mode0States), 5); }

extern "C" void __cdecl PartyAction16_StrikeBegin(void) { StrikeBegin(); }

extern "C" void __cdecl PartyAction16_Strike(void) { Strike("PartyAction16_Strike (0x523BD0)", &PartyAction16_StrikeCell); }

extern "C" unsigned char __cdecl PartyAction16_StrikeCell(unsigned x, unsigned z) { return StrikeCell(x, z); }

extern "C" void __cdecl PartyAction16_Form1Mode1(void) { ByState3("PartyAction16_Form1Mode1 (0x523EB0)", Table(PartyAction16_Mode1States), 3); }

// original 0x523ED0 (0x35 bytes), and 0x524D60 the same bytes:
// PartyAction_SideProbes, the pose (+8 - 1) / 2 + 0x42, +0xA = 0xB, +3 = 1.
extern "C" void __cdecl PartyAction_ProbeStart(void) { ProbeStartShape(); }

// original 0x523F10 (0x97 bytes), the step after a ProbeStart in seven state
// tables: Sprite_ScriptTickOnce; when it answers non-zero, the pose +8
// (Sprite_EnsureAnimation) and +3 = 2. Otherwise +0xA, when not 0, counted
// down; reaching 0: Sound_PlayEffect(u16 +0x2C + 0x100), then effect object
// +0xB's +8 = Sprite_Current's +8, +7 = 1 when it was 0, the object's +0xA =
// +7, Sound_PlayEffect(0x10B). The effect index +0xB is not checked by the
// original: ours aborts past the 20 records.
extern "C" void __cdecl PartyAction_EffectCountdown(void) {
    const char* const who = "PartyAction_EffectCountdown (0x523F10)";
    if (SH_CALL(Sprite_ScriptTickOnce)() != 0) {
        SH_CALL(Sprite_EnsureAnimation)(Sc()[8]);
        Sc()[3] = 2;
        return;
    }
    unsigned char* const s = Sc();
    if (s[0xA] == 0) return;
    --s[0xA];
    if (s[0xA] != 0) return;
    SH_CALL(Sound_PlayEffect)(W16(Word(s + 0x2C) + 0x100));
    unsigned char* const c = Sc();
    EffectAtRest1f(c[0xB], who)[8] = c[8];
    if (c[7] == 0) c[7] = 1;
    EffectAtRest1f(c[0xB], who)[0xA] = c[7];
    SH_CALL(Sound_PlayEffect)(0x10B);
}

// original 0x523FB0 (0x12 bytes): PartyFormAction16_Forms[2] - PartyFormAction16_Form2States by +2.
extern "C" void __cdecl PartyFormAction16_Form2(void) {
    ByStateRest1f("PartyFormAction16_Form2 (0x523FB0)", PartyFormAction16_Form2States, PartyFormAction16_Form2States_count);
}

// original 0x523FD0 (0x12 bytes): PartyAction16_Forms[2] - PartyAction16_Form2States by +2.
extern "C" void __cdecl PartyAction16_Form2(void) {
    ByStateRest1f("PartyAction16_Form2 (0x523FD0)", PartyAction16_Form2States, PartyAction16_Form2States_count);
}

// original 0x523FF0 (0x1D4 bytes), PartyAction16_Form2States[0]; 0x524490
// (PartyAction17_Form1States[0]) and 0x524E90 (PartyAction18_Form1States[0])
// are the same bytes. An even direction +8 is turned one eighth back (-1, &
// 7); when PartyAction_TargetAhead finds nothing that way, two on (+2), and
// when it finds nothing there either, back (-2). Then the point one step
// ahead in +8: MapView_GroundAt there, less Sprite_Current's height word (16
// bits), is the rise; MapView_SlopeAt there in +8 (its answer unused). With
// the scratch flag set and the rise above 0x40 (s16): the pose (+8 - 1) / 2 +
// 0x46 and +2 one on. Otherwise +0x2B = 1, the side probes in directions 3
// and 5 (inline), Sound_PlayEffect(u16 +0x2C + 0x100), the pose (+8 - 1) / 2
// + 0x42 and +0xA = 5. Then +0xB = 0 and +2 one on.
extern "C" void __cdecl PartyAction16_Form2Begin(void) { BeginShape(); }

// original 0x5241D0 (0xDB bytes), PartyAction16_Form2States[1]; 0x524670 and
// 0x525070 the same with their own pickups. +0xA counted down; at 0: the
// point two steps ahead in +8 (unchecked); the object Sprite_ObjectAt(point,
// 0) finds gets bit 0 of its +0x80; then the pickup on the point's cell, and
// when that finds nothing, on the cell one on in x (only when the point's x
// has a fraction), then one on in z (when z has one); then +2 one on.
// Sprite_ScriptTickOnce every time. The cells are the point's high words; the
// original passes them as dwords whose upper halves are its own uninitialised
// stack, and each pickup reads their low words only.
extern "C" void __cdecl PartyAction16_Form2Resolve(void) {
    ResolveShape(&PartyAction16_CellPickup, "PartyAction16_Form2Resolve (0x5241D0)");
}

// original 0x5242B0 (0x11F bytes); 0x524750 and 0x525150 are the same bytes.
// What lies in the map cell (x, z) - AreaMap_ByteAt:
//   0xF2 - with an effect object free (Effect_FindFree): Effect_SpawnAtCell(0,
//          x, z); three Rand nibbles in sixteen (13, 14, 15) find zenny, 2 (5
//          on 15), twenty times that when Field_InputFlags has bit 1 or 2 and
//          a second Rand & 3 is 0: Field_GiveZenny(the amount),
//          Effect_SpawnAtCell(1, x, z); then +0xB = 1. None free: none of it.
//   0xF8 - Effect_SpawnAtCell(0, x, z); item 0x56's name (Item_NamePtr(0,
//          0x56)), 16 bytes, to Text_Records; Inventory_Add(0, 0x56, 1)
//          taken: Sound_PlayEffect(0x106), Msg_OpenSystem(2), else
//          Msg_OpenSystem(3); Field_Request = 2, +0xB = 1.
// Both clear the cell (AreaMap_ClearCell) and answer 1; any other cell 0.
// Field_CellPickup 0x51EBD0 is the same code with ten times, not twenty.
extern "C" unsigned char __cdecl PartyAction16_CellPickup(unsigned x, unsigned z) { return PickupShape(x, z); }

// original 0x5243D0 (0x13 bytes): Field_FormActions[16] - PartyFormAction16_Forms by the word +0x2C.
extern "C" void __cdecl PartyFormAction16_ByForm(void) {
    ByFormRest1f("PartyFormAction16_ByForm (0x5243D0)", PartyFormAction16_Forms, PartyFormAction16_Forms_count);
}

// original 0x5243F0 (0x13 bytes): Field_ActionBySet[16] - PartyAction16_Forms by the word +0x2C.
extern "C" void __cdecl PartyAction16_ByForm(void) {
    ByFormRest1f("PartyAction16_ByForm (0x5243F0)", PartyAction16_Forms, PartyAction16_Forms_count);
}

// original 0x524410 (0x12 bytes): PartyFormAction17_Forms[0] - PartyFormAction17_Form0States by +2.
extern "C" void __cdecl PartyFormAction17_Form0(void) {
    ByStateRest1f("PartyFormAction17_Form0 (0x524410)", PartyFormAction17_Form0States, PartyFormAction17_Form0States_count);
}

// original 0x524430 (0x12 bytes): PartyAction17_Forms[0] - PartyAction17_Form0States by +2.
extern "C" void __cdecl PartyAction17_Form0(void) {
    ByStateRest1f("PartyAction17_Form0 (0x524430)", PartyAction17_Form0States, PartyAction17_Form0States_count);
}

// original 0x524450 (0x12 bytes): PartyFormAction17_Forms[1] - PartyFormAction17_Form1States by +2.
extern "C" void __cdecl PartyFormAction17_Form1(void) {
    ByStateRest1f("PartyFormAction17_Form1 (0x524450)", PartyFormAction17_Form1States, PartyFormAction17_Form1States_count);
}

// original 0x524470 (0x12 bytes): PartyAction17_Forms[1] - PartyAction17_Form1States by +2.
extern "C" void __cdecl PartyAction17_Form1(void) {
    ByStateRest1f("PartyAction17_Form1 (0x524470)", PartyAction17_Form1States, PartyAction17_Form1States_count);
}

extern "C" void __cdecl PartyAction17_Form1Begin(void) { BeginShape(); }

extern "C" void __cdecl PartyAction17_Form1Resolve(void) {
    ResolveShape(&PartyAction17_CellPickup, "PartyAction17_Form1Resolve (0x524670)");
}

extern "C" unsigned char __cdecl PartyAction17_CellPickup(unsigned x, unsigned z) { return PickupShape(x, z); }

// original 0x524870: an effect object of kind 0x34 at the cell (x, z). With
// Effect_FindFree's slot not 0xFF: +0 in use, +5 the kind, +1 the state from
// the first argument's byte, +0x34 / +0x38 the cell's words sign-extended as
// 16.16, +0x3E the ground there (AreaMap_Elevation of the two, x read back from
// the record) plus 0x100, +0xB 0. The slot is used as FindFree answers it,
// unchecked (FindFree gives 0..19 or 0xFF). The eax the original leaves is
// no caller's (the 18 cell pickups ignore it).
extern "C" void __cdecl Effect_SpawnAtCell(unsigned state, unsigned x, unsigned z) {
    const unsigned char slot = SH_CALL(Effect_FindFree)();
    if (slot == 0xFF) return;
    unsigned char* const e = rv::Bytes(Effect_Objects) + slot * scena_se::at::kEffectStride;
    e[0] = 1;
    e[5] = scena_se::at::kCellFxKind;
    e[1] = static_cast<unsigned char>(state);
    rv::As<records::EffectObject>(e)->x = static_cast<std::int32_t>(static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<short>(x))) << 16);
    const auto zz = static_cast<std::int32_t>(static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<short>(z))) << 16);
    rv::As<records::EffectObject>(e)->z = zz;
    const long ground = SH_CALL(AreaMap_Elevation)(rv::As<records::EffectObject>(e)->x, zz);
    rv::Store<unsigned>(rv::As<records::SpriteHead>(e)->height_whole, static_cast<unsigned>(ground) + 0x100);
    e[0xB] = 0;
}

// original 0x5248F0 (0x12 bytes): PartyFormAction17_Forms[2] - PartyFormAction17_Form2States by +2.
extern "C" void __cdecl PartyFormAction17_Form2(void) {
    ByStateRest1f("PartyFormAction17_Form2 (0x5248F0)", PartyFormAction17_Form2States, PartyFormAction17_Form2States_count);
}

// original 0x524910 (0x12 bytes): PartyAction17_Forms[2] - PartyAction17_Form2States by +2.
extern "C" void __cdecl PartyAction17_Form2(void) {
    ByStateRest1f("PartyAction17_Form2 (0x524910)", PartyAction17_Form2States, PartyAction17_Form2States_count);
}

// original 0x524930 (0x13 bytes): Field_FormActions[17] - PartyFormAction17_Forms by the word +0x2C.
extern "C" void __cdecl PartyFormAction17_ByForm(void) {
    ByFormRest1f("PartyFormAction17_ByForm (0x524930)", PartyFormAction17_Forms, PartyFormAction17_Forms_count);
}

// original 0x524950 (0x13 bytes): Field_ActionBySet[17] - PartyAction17_Forms by the word +0x2C.
extern "C" void __cdecl PartyAction17_ByForm(void) {
    ByFormRest1f("PartyAction17_ByForm (0x524950)", PartyAction17_Forms, PartyAction17_Forms_count);
}

// original 0x524970 (0x12 bytes): PartyFormAction18_Forms[0] - PartyFormAction18_Form0States by +2.
extern "C" void __cdecl PartyFormAction18_Form0(void) {
    ByStateRest1f("PartyFormAction18_Form0 (0x524970)", PartyFormAction18_Form0States, PartyFormAction18_Form0States_count);
}

// original 0x524990 (0x12 bytes): PartyAction18_Forms[0] - PartyAction18_Form0Subs by +2.
extern "C" void __cdecl PartyAction18_Form0(void) {
    ByStateRest1f("PartyAction18_Form0 (0x524990)", PartyAction18_Form0Subs, PartyAction18_Form0Subs_count);
}

// original 0x5249B0 (0x12 bytes): PartyAction18_Form0Subs[0] - PartyAction18_Form0Sub0Steps by +3.
extern "C" void __cdecl PartyAction18_Form0Sub0(void) {
    ByStepRest1f("PartyAction18_Form0Sub0 (0x5249B0)", PartyAction18_Form0Sub0Steps, PartyAction18_Form0Sub0Steps_count);
}

// original 0x5249D0 (0x87 bytes), PartyAction18_Form0Sub0Steps[0]: an even
// direction turned one eighth back; when PartyAction_BlockedAhead finds the
// way open, two on; open there too, back. Then PartyAction_SideProbes, the
// pose (+8 - 1) / 2 + 0x42, +0xB = 0, +0xA = 0xB, +3 = 1.
extern "C" void __cdecl PartyAction18_Form0Sub0Begin(void) {
    unsigned char* const s = Sc();
    const unsigned char d0 = s[8];
    if ((d0 & 1) == 0) {
        s[8] = static_cast<unsigned char>((d0 - 1) & 7);
        if (SH_CALL(PartyAction_BlockedAhead)() == 0) {
            Sc()[8] = static_cast<unsigned char>((Sc()[8] + 2) & 7);
            if (SH_CALL(PartyAction_BlockedAhead)() == 0) Sc()[8] = static_cast<unsigned char>((Sc()[8] - 2) & 7);
        }
    }
    SH_CALL(PartyAction_SideProbes)();
    SH_CALL(Sprite_EnsureAnimation)(static_cast<unsigned char>(HalfTurn(Sc()[8]) + 0x42));
    Sc()[0xB] = 0;
    Sc()[0xA] = 0xB;
    Sc()[3] = 1;
}

// original 0x524A60 (0x14C bytes), PartyAction18_Form0Sub0Steps[1]: +0xA,
// when not 0, counted down; reaching 0: Sound_PlayEffect(u16 +0x2C + 0x100);
// Field_EffectAhead - an effect object ahead: Sound_PlayEffect(0x10B), its +8
// = Sprite_Current's +8, its +0xA = 1, Sprite_Current +6 = its index, +3 one
// on (and one more below: two in all). None: the point two steps ahead; the
// object Sprite_ObjectAt(point, 0) finds gets bit 0 of its +0x80 and
// Sound_PlayEffect(0x10B); then PartyAction18_CellStrike on the point's cell,
// the cell one on in x (x with a fraction), one on in z (z with one), each
// only while the earlier found nothing; then +3 one on. Sprite_ScriptTickOnce
// every time. The effect index (a signed byte, 0..19 or 0xFF from
// Field_EffectAhead) and the object index are not checked by the original.
extern "C" void __cdecl PartyAction18_Form0Sub0Strike(void) {
    const char* const who = "PartyAction18_Form0Sub0Strike (0x524A60)";
    unsigned char* const s = Sc();
    if (s[0xA] != 0) {
        --s[0xA];
        if (s[0xA] == 0) {
            SH_CALL(Sound_PlayEffect)(W16(Word(s + 0x2C) + 0x100));
            const unsigned char ahead = SH_CALL(Field_EffectAhead)();
            if (ahead != 0xFF) {
                SH_CALL(Sound_PlayEffect)(0x10B);
                unsigned char* const c = Sc();
                // `movsx eax, bl; shl eax, 7`: 0x80..0xFE index before the
                // records; EffectAtRest1f aborts on any byte past 19
                unsigned char* const e = EffectAtRest1f(ahead, who);
                e[8] = c[8];
                e[0xA] = 1;
                c[6] = ahead;
                ++c[3];
            } else {
                const unsigned char* const c = Sc();
                const unsigned d = c[8];
                const U x = static_cast<U>(rv::As<records::PartyMember>(c)->x) + 2u * StepXRest1f(d);
                const U z = static_cast<U>(Long(c + 0x38)) + 2u * StepZRest1f(d);
                const unsigned char found = SH_CALL(Sprite_ObjectAt)(static_cast<long>(x), static_cast<long>(z), 0);
                if (found != 0xFF) {
                    MarkObjectRest1f(found, who);
                    SH_CALL(Sound_PlayEffect)(0x10B);
                }
                const unsigned cx = x >> 16, cz = z >> 16;
                if (SH_CALL(PartyAction18_CellStrike)(cx, cz) == 0) {
                    bool done = false;
                    if ((x & 0xFFFF) != 0) done = SH_CALL(PartyAction18_CellStrike)(cx + 1, cz) != 0;
                    if (!done && (z & 0xFFFF) != 0) SH_CALL(PartyAction18_CellStrike)(cx, cz + 1);
                }
            }
            ++Sc()[3];
        }
    }
    SH_CALL(Sprite_ScriptTickOnce)();
}

// original 0x524BB0 (0x188 bytes), set 18's strike on the map cell (x, z) -
// AreaMap_ByteAt:
//   0xF0, 0xF1, 0xF4 - Effect_SpawnAtCellHigh(0, x, z); when Rand & 7 is 6
//          or 7, Effect_SpawnAtCellHigh(4, x, z); Sound_PlayEffect(0x10B); 1.
//   0xF6, 0xF7 - Effect_SpawnAtCellHigh(0, x, z), Sound_PlayEffect(0x10B);
//          then Rand & 0xF: below 7 - Effect_SpawnAtCellHigh(3, x, z), item
//          0x29's name to Text_Records, Inventory_Add(0, 0x29, 1) taken:
//          Sound_PlayEffect(0x106) and Msg_OpenSystem(2), else
//          Msg_OpenSystem(3); +0xB = 2. Above 0xB - Effect_SpawnAtCellHigh(2,
//          x, z), Sprite_FlashClut(0), Char_LoseHp(1, Field_State +0x89),
//          Msg_OpenSystem(0xD9), +0xB = 1. Either way (7..0xB too)
//          Field_Request = 2; 1.
//   else 0.
// The callers push a fourth dword to Effect_SpawnAtCellHigh (here the z
// argument's slot, its low byte overwritten by the cell); it reads three.
extern "C" unsigned char __cdecl PartyAction18_CellStrike(unsigned x, unsigned z) {
    const unsigned char cell = SH_CALL(AreaMap_ByteAt)(static_cast<short>(x), static_cast<short>(z));
    if (cell == 0xF0 || cell == 0xF1 || cell == 0xF4) {
        SH_CALL(Effect_SpawnAtCellHigh)(0, x, z);
        const unsigned roll = static_cast<unsigned>(SH_CALL(Rand)()) & 7;
        if (roll > 5) SH_CALL(Effect_SpawnAtCellHigh)(4, x, z);
        SH_CALL(Sound_PlayEffect)(0x10B);
        return 1;
    }
    if (cell != 0xF6 && cell != 0xF7) return 0;
    SH_CALL(Effect_SpawnAtCellHigh)(0, x, z);
    SH_CALL(Sound_PlayEffect)(0x10B);
    const unsigned roll = static_cast<unsigned>(SH_CALL(Rand)()) & 0xF;
    if (roll < 7) {
        SH_CALL(Effect_SpawnAtCellHigh)(3, x, z);
        const unsigned char* const name = SH_CALL(Item_NamePtr)(0, 0x29);
        CopyName(name);
        if (SH_CALL(Inventory_Add)(0, 0x29, 1) != 0) {
            SH_CALL(Sound_PlayEffect)(0x106);
            SH_CALL(Msg_OpenSystem)(2);
        } else {
            SH_CALL(Msg_OpenSystem)(3);
        }
        Sc()[0xB] = 2;
        Field_Request = 2;
        return 1;
    }
    if (roll > 0xB) {
        SH_CALL(Effect_SpawnAtCellHigh)(2, x, z);
        SH_CALL(Sprite_FlashClut)(0);
        SH_CALL(Char_LoseHp)(1, Field_State[0x89]);
        SH_CALL(Msg_OpenSystem)(0xD9);
        Sc()[0xB] = 1;
    }
    Field_Request = 2;
    return 1;
}

// original 0x524D40 (0x12 bytes): PartyAction18_Form0Subs[1] - PartyAction18_Form0Sub1Steps by +3.
extern "C" void __cdecl PartyAction18_Form0Sub1(void) {
    ByStepRest1f("PartyAction18_Form0Sub1 (0x524D40)", PartyAction18_Form0Sub1Steps, PartyAction18_Form0Sub1Steps_count);
}

extern "C" void __cdecl PartyAction18_ProbeStart(void) { ProbeStartShape(); }

// original 0x524DA0 (0xAF bytes): Sprite_Current +0x2B = 1, then the side
// probes in directions 3 and 5 (the original reads Field_DirectionSteps' rows 3
// and 5 by their addresses, 0x6697C8 and 0x6697D8, and pushes the direction
// as an immediate). The original's eax at its ret is the last callee's, no
// caller's.
extern "C" void __cdecl PartyAction_SideProbes(void) {
    Sprite_Current[0x2B] = 1;
    SideProbe(3);
    SideProbe(5);
}

// original 0x524E50 (0x12 bytes): PartyFormAction18_Forms[1] - PartyFormAction18_Form1States by +2.
extern "C" void __cdecl PartyFormAction18_Form1(void) {
    ByStateRest1f("PartyFormAction18_Form1 (0x524E50)", PartyFormAction18_Form1States, PartyFormAction18_Form1States_count);
}

// original 0x524E70 (0x12 bytes): PartyAction18_Forms[1] - PartyAction18_Form1States by +2.
extern "C" void __cdecl PartyAction18_Form1(void) {
    ByStateRest1f("PartyAction18_Form1 (0x524E70)", PartyAction18_Form1States, PartyAction18_Form1States_count);
}

extern "C" void __cdecl PartyAction18_Form1Begin(void) { BeginShape(); }

extern "C" void __cdecl PartyAction18_Form1Resolve(void) {
    ResolveShape(&PartyAction18_CellPickup, "PartyAction18_Form1Resolve (0x525070)");
}

extern "C" unsigned char __cdecl PartyAction18_CellPickup(unsigned x, unsigned z) { return PickupShape(x, z); }

// original 0x525270 (0x12 bytes): PartyFormAction18_Forms[2] - PartyFormAction18_Form2States by +2.
extern "C" void __cdecl PartyFormAction18_Form2(void) {
    ByStateRest1f("PartyFormAction18_Form2 (0x525270)", PartyFormAction18_Form2States, PartyFormAction18_Form2States_count);
}

// original 0x525290 (0x12 bytes): PartyAction18_Forms[2] - PartyAction18_Form2States by +2.
extern "C" void __cdecl PartyAction18_Form2(void) {
    ByStateRest1f("PartyAction18_Form2 (0x525290)", PartyAction18_Form2States, PartyAction18_Form2States_count);
}

// original 0x5252B0 (0x7B bytes), a state in seven tables: an even direction
// +8 turned one eighth back (-1, & 7); +0xB = Effect_FindFree; when it is not
// 0xFF, that effect object's +0 = 1 and kind +5 = 0x1B (the index unchecked:
// ours aborts past the 20 records). The pose +8 >> 1 (unsigned) + 0x42,
// Sound_PlayEffect(u16 +0x2C + 0x100), +2 one on.
extern "C" void __cdecl PartyAction_SpawnKind1B(void) {
    const char* const who = "PartyAction_SpawnKind1B (0x5252B0)";
    unsigned char* const s = Sc();
    if ((s[8] & 1) == 0) s[8] = static_cast<unsigned char>((s[8] - 1) & 7);
    const unsigned char slot = SH_CALL(Effect_FindFree)();
    Sc()[0xB] = slot;
    unsigned char* const c = Sc();
    // Debt 3 (docs/rest_1d.md L1): with all 20 records in use the 0xFF is
    // stored as it stands and PartyAction_WaitEffectDone then polls a byte of
    // Gfx_PacketPools as the effect's in-use byte. Logged (not changed) so the
    // owner can see how often play reaches it (2026-10-06).
    if (slot == 0xFF)
        bof3::Log("debt3       PartyAction_SpawnKind1B: no effect record free (set %u, action %u) - the wait reads the packet pool",
                  static_cast<unsigned>(c[0]), static_cast<unsigned>(c[1]));
    if (c[0xB] != 0xFF) {
        EffectAtRest1f(c[0xB], who)[0] = 1;
        EffectAtRest1f(c[0xB], who)[5] = 0x1B;
    }
    SH_CALL(Sprite_EnsureAnimation)(static_cast<unsigned char>((c[8] >> 1) + 0x42));
    SH_CALL(Sound_PlayEffect)(W16(Word(Sc() + 0x2C) + 0x100));
    ++Sc()[2];
}

// original 0x525330 (0x13 bytes): Field_FormActions[18] - PartyFormAction18_Forms by the word +0x2C.
extern "C" void __cdecl PartyFormAction18_ByForm(void) {
    ByFormRest1f("PartyFormAction18_ByForm (0x525330)", PartyFormAction18_Forms, PartyFormAction18_Forms_count);
}

// original 0x525350 (0x13 bytes): Field_ActionBySet[18] - PartyAction18_Forms by the word +0x2C.
extern "C" void __cdecl PartyAction18_ByForm(void) {
    ByFormRest1f("PartyAction18_ByForm (0x525350)", PartyAction18_Forms, PartyAction18_Forms_count);
}

void PartyAction_Inject() {
    BOF3_INJECT(PartyFormAction0_Form0);
    BOF3_INJECT(PartyFormAction_Form0Begin);
    BOF3_INJECT(PartyAction0_Form0);
    BOF3_INJECT(PartyAction0_Form0Begin);
    BOF3_INJECT(PartyAction0_Form0Resolve);
    BOF3_INJECT(PartyAction0_CellPickup);
    BOF3_INJECT(PartyAction_TargetAhead);
    BOF3_INJECT(PartyFormAction0_Form1);
    BOF3_INJECT(PartyAction0_Form1);
    BOF3_INJECT(PartyFormAction0_Form2);
    BOF3_INJECT(PartyFormAction_Form2Turn);
    BOF3_INJECT(PartyAction0_Form2);
    BOF3_INJECT(PartyAction0_Form2Begin);
    BOF3_INJECT(PartyAction_MemberBeyondEffect);
    BOF3_INJECT(PartyFormAction0_ByForm);
    BOF3_INJECT(PartyAction0_ByForm);
    BOF3_INJECT(PartyFormAction1_Form0);
    BOF3_INJECT(PartyAction1_Form0);
    BOF3_INJECT(PartyAction1_Form0Begin);
    BOF3_INJECT(PartyAction1_Form0Resolve);
    BOF3_INJECT(PartyAction1_CellPickup);
    BOF3_INJECT(PartyFormAction1_Form1);
    BOF3_INJECT(PartyAction1_Form1);
    BOF3_INJECT(PartyFormAction1_Form2);
    BOF3_INJECT(PartyAction1_Form2);
    BOF3_INJECT(PartyFormAction1_ByForm);
    BOF3_INJECT(PartyAction1_ByForm);
    BOF3_INJECT(PartyFormAction2_Form0);
    BOF3_INJECT(PartyFormAction_Form0Turn);
    BOF3_INJECT(PartyAction2_Form0);
    BOF3_INJECT(PartyAction2_Form0Begin);
    BOF3_INJECT(PartyAction2_Form0Resolve);
    BOF3_INJECT(PartyAction2_CellPickup);
    BOF3_INJECT(PartyFormAction2_Form1);
    BOF3_INJECT(PartyFormAction_Form1Begin);
    BOF3_INJECT(PartyFormAction_Form1Turn);
    BOF3_INJECT(PartyAction2_Form1);
    BOF3_INJECT(PartyFormAction2_Form2);
    BOF3_INJECT(PartyAction2_Form2);
    BOF3_INJECT(PartyAction2_Form2State0);
    BOF3_INJECT(PartyAction2_Form2Aim);
    BOF3_INJECT(PartyAction2_Form2Strike);
    BOF3_INJECT(PartyAction_FinishPalette);
    BOF3_INJECT(PartyAction2_CellStrike);
    BOF3_INJECT(PartyAction2_Form2State1);
    BOF3_INJECT(PartyAction2_Form2Reaim);
    BOF3_INJECT(PartyAction_WaitEffect);
    BOF3_INJECT(PartyFormAction2_ByForm);
    BOF3_INJECT(PartyAction2_ByForm);
    BOF3_INJECT(PartyFormAction3_Form0);
    BOF3_INJECT(PartyAction3_Form0);
    BOF3_INJECT(PartyAction3_Form0Begin);
    BOF3_INJECT(PartyAction3_Form0Resolve);
    BOF3_INJECT(PartyAction_Finish);
    BOF3_INJECT(PartyAction3_CellPickup);
    BOF3_INJECT(PartyFormAction3_Form1);
    BOF3_INJECT(PartyAction3_Form1);
    BOF3_INJECT(PartyAction3_Form1Begin);
    BOF3_INJECT(PartyAction_MemberOnEffect);
    BOF3_INJECT(PartyFormAction3_Form2);
    BOF3_INJECT(PartyAction3_Form2);
    BOF3_INJECT(PartyAction_SpawnKind3A);
    BOF3_INJECT(PartyFormAction3_ByForm);
    BOF3_INJECT(PartyAction3_ByForm);
    BOF3_INJECT(PartyFormAction4_Form0);
    BOF3_INJECT(PartyAction4_Form0);
    BOF3_INJECT(PartyAction4_Form0Begin);
    BOF3_INJECT(PartyAction4_Form0Resolve);
    BOF3_INJECT(PartyAction4_CellPickup);
    BOF3_INJECT(PartyFormAction4_Form1);
    BOF3_INJECT(PartyAction4_Form1);
    BOF3_INJECT(PartyAction4_Form1Begin);
    BOF3_INJECT(PartyFormAction4_Form2);
    BOF3_INJECT(PartyAction4_Form2);
    BOF3_INJECT(PartyAction4_Form2State0);
    BOF3_INJECT(PartyAction4_Form2Aim);
    BOF3_INJECT(PartyAction4_Form2Hit);
    BOF3_INJECT(PartyAction4_CellHit);
    BOF3_INJECT(PartyAction4_Form2State1);
    BOF3_INJECT(PartyAction4_Form2Again);
    BOF3_INJECT(PartyFormAction4_ByForm);
    BOF3_INJECT(PartyAction4_ByForm);
    BOF3_INJECT(PartyFormAction5_Form0);
    BOF3_INJECT(PartyAction5_Form0);
    BOF3_INJECT(PartyAction5_Form0Begin);
    BOF3_INJECT(PartyAction5_Form0Resolve);
    BOF3_INJECT(Field_CellPickup);
    BOF3_INJECT(PartyFormAction5_Form1);
    BOF3_INJECT(PartyAction5_Form1);
    BOF3_INJECT(PartyAction5_Form1State0);
    BOF3_INJECT(PartyAction5_Form1Aim);
    BOF3_INJECT(PartyAction5_Form1Hit);
    BOF3_INJECT(PartyAction5_CellHit);
    BOF3_INJECT(PartyAction5_Form1State1);
    BOF3_INJECT(PartyAction5_Form1Again);
    BOF3_INJECT(PartyAction5_Form1Wait);
    BOF3_INJECT(PartyFormAction5_Form2);
    BOF3_INJECT(PartyFormAction5_ByForm);
    BOF3_INJECT(PartyAction5_ByForm);
    BOF3_INJECT(PartyFormAction6_Form0);
    BOF3_INJECT(PartyAction6_Form0);
    BOF3_INJECT(PartyAction6_Form0Begin);
    BOF3_INJECT(PartyAction6_Form0Resolve);
    BOF3_INJECT(PartyAction6_CellPickup);
    BOF3_INJECT(PartyFormAction6_Form1);
    BOF3_INJECT(PartyAction6_Form1);
    BOF3_INJECT(PartyFormAction6_Form2);
    BOF3_INJECT(PartyAction6_Form2);
    BOF3_INJECT(PartyAction6_Form2State0);
    BOF3_INJECT(PartyAction6_Form2Begin);
    BOF3_INJECT(PartyAction6_Form2Resolve);
    BOF3_INJECT(PartyAction_TickThenFace);
    BOF3_INJECT(PartyAction6_CellHit);
    BOF3_INJECT(PartyAction6_Form2State1);
    BOF3_INJECT(PartyAction6_Form2Probe);
    BOF3_INJECT(PartyFormAction6_ByForm);
    BOF3_INJECT(PartyAction6_ByForm);
    BOF3_INJECT(PartyFormAction7_Form0);
    BOF3_INJECT(PartyAction7_Form0);
    BOF3_INJECT(PartyAction7_Form0Begin);
    BOF3_INJECT(PartyFormAction7_Form1);
    BOF3_INJECT(PartyAction_TurnStep);
    BOF3_INJECT(PartyFormAction7_Form2);
    BOF3_INJECT(PartyAction7_Form2);
    BOF3_INJECT(PartyAction7_Form2Begin);
    BOF3_INJECT(PartyAction7_Form2Resolve);
    BOF3_INJECT(PartyAction7_CellPickup);
    BOF3_INJECT(PartyFormAction7_ByForm);
    BOF3_INJECT(PartyAction7_ByForm);
    BOF3_INJECT(PartyFormAction8_Form0);
    BOF3_INJECT(PartyAction8_Form0);
    BOF3_INJECT(PartyAction8_Form0Begin);
    BOF3_INJECT(PartyFormAction8_Form1);
    BOF3_INJECT(PartyAction8_Form1);
    BOF3_INJECT(PartyAction_WaitEffectEnd);
    BOF3_INJECT(PartyFormAction8_Form2);
    BOF3_INJECT(PartyAction8_Form2);
    BOF3_INJECT(PartyAction8_Form2Begin);
    BOF3_INJECT(PartyAction8_Form2Resolve);
    BOF3_INJECT(PartyAction8_CellPickup);
    BOF3_INJECT(PartyFormAction8_ByForm);
    BOF3_INJECT(PartyAction8_ByForm);
    BOF3_INJECT(PartyFormAction9_Form0);
    BOF3_INJECT(PartyAction_TurnToSide);
    BOF3_INJECT(PartyAction9_Form0);
    BOF3_INJECT(PartyAction9_Form0Begin);
    BOF3_INJECT(PartyFormAction9_Form1);
    BOF3_INJECT(PartyAction9_Form1);
    BOF3_INJECT(PartyAction9_Form1State0);
    BOF3_INJECT(PartyAction9_Form1Begin);
    BOF3_INJECT(PartyAction9_Form1Resolve);
    BOF3_INJECT(PartyAction9_CellHit);
    BOF3_INJECT(PartyAction9_Form1State1);
    BOF3_INJECT(PartyAction9_Form1Start);
    BOF3_INJECT(PartyFormAction9_Form2);
    BOF3_INJECT(PartyFormAction_TurnToSide);
    BOF3_INJECT(PartyAction9_Form2);
    BOF3_INJECT(PartyAction9_Form2Begin);
    BOF3_INJECT(PartyAction9_Form2Resolve);
    BOF3_INJECT(PartyAction9_CellPickup);
    BOF3_INJECT(PartyFormAction9_ByForm);
    BOF3_INJECT(PartyAction9_ByForm);
    BOF3_INJECT(PartyFormAction10_Form0);
    BOF3_INJECT(PartyAction10_Form0);
    BOF3_INJECT(PartyAction10_Form0Begin);
    BOF3_INJECT(PartyAction_Kind30Ahead);
    BOF3_INJECT(PartyFormAction10_Form1);
    BOF3_INJECT(PartyAction10_Form1);
    BOF3_INJECT(PartyAction10_Form1Begin);
    BOF3_INJECT(PartyAction10_Form1Resolve);
    BOF3_INJECT(PartyAction10_CellPickup);
    BOF3_INJECT(PartyFormAction10_Form2);
    BOF3_INJECT(PartyAction10_Form2);
    BOF3_INJECT(PartyAction_WaitEffectDone);
    BOF3_INJECT(PartyFormAction10_ByForm);
    BOF3_INJECT(PartyAction10_ByForm);
    BOF3_INJECT(PartyFormAction11_Form0);
    BOF3_INJECT(PartyAction11_Form0);
    BOF3_INJECT(PartyAction11_Form0Begin);
    BOF3_INJECT(PartyAction_StepCountdown);
    BOF3_INJECT(PartyFormAction11_Form1);
    BOF3_INJECT(PartyAction11_Form1);
    BOF3_INJECT(PartyAction11_Form1Begin);
    BOF3_INJECT(PartyAction11_Form1Resolve);
    BOF3_INJECT(PartyAction11_CellPickup);
    BOF3_INJECT(PartyFormAction11_ByForm);
    BOF3_INJECT(PartyAction11_ByForm);
    BOF3_INJECT(PartyFormAction12_Form0);
    BOF3_INJECT(PartyAction12_Form0);
    BOF3_INJECT(PartyAction12_Form0State0);
    BOF3_INJECT(PartyAction12_Form0Begin);
    BOF3_INJECT(PartyAction12_Form0Resolve);
    BOF3_INJECT(PartyAction12_CellHit);
    BOF3_INJECT(PartyAction12_Form0State1);
    BOF3_INJECT(PartyAction12_Form0EffectSet);
    BOF3_INJECT(PartyAction_BlockedAhead);
    BOF3_INJECT(PartyFormAction12_Form1);
    BOF3_INJECT(PartyFormAction12_ByForm);
    BOF3_INJECT(PartyAction12_ByForm);
    BOF3_INJECT(PartyFormAction13_Form0);
    BOF3_INJECT(PartyAction_NoAction);
    BOF3_INJECT(PartyFormAction13_Form1);
    BOF3_INJECT(PartyAction13_Form1);
    BOF3_INJECT(PartyFormAction13_Form2);
    BOF3_INJECT(PartyAction13_Form2);
    BOF3_INJECT(PartyAction13_Form2Begin);
    BOF3_INJECT(PartyAction13_Form2Resolve);
    BOF3_INJECT(PartyAction13_CellPickup);
    BOF3_INJECT(PartyFormAction13_ByForm);
    BOF3_INJECT(PartyAction13_ByForm);
    BOF3_INJECT(PartyFormAction14_Form0);
    BOF3_INJECT(PartyFormAction14_Form1);
    BOF3_INJECT(PartyAction14_Form1);
    BOF3_INJECT(PartyAction14_Form1Mode0);
    BOF3_INJECT(PartyAction14_StrikeBegin);
    BOF3_INJECT(PartyAction14_Strike);
    BOF3_INJECT(PartyAction_StrikeWait);
    BOF3_INJECT(PartyAction14_StrikeCell);
    BOF3_INJECT(Effect_SpawnAtCellHigh);
    BOF3_INJECT(PartyAction14_Form1Mode1);
    BOF3_INJECT(PartyAction14_Mode1Begin);
    BOF3_INJECT(PartyFormAction14_Form2);
    BOF3_INJECT(PartyAction14_Form2);
    BOF3_INJECT(PartyAction14_Form2Begin);
    BOF3_INJECT(PartyAction14_Form2Resolve);
    BOF3_INJECT(PartyAction14_CellPickup);
    BOF3_INJECT(PartyFormAction14_ByForm);
    BOF3_INJECT(PartyAction14_ByForm);
    BOF3_INJECT(PartyFormAction15_Form0);
    BOF3_INJECT(PartyFormAction15_Form1);
    BOF3_INJECT(PartyAction15_Form1);
    BOF3_INJECT(PartyAction15_Form1Begin);
    BOF3_INJECT(PartyAction15_Form1Resolve);
    BOF3_INJECT(PartyAction15_CellPickup);
    BOF3_INJECT(PartyFormAction15_Form2);
    BOF3_INJECT(PartyAction15_Form2);
    BOF3_INJECT(PartyFormAction15_ByForm);
    BOF3_INJECT(PartyAction15_ByForm);
    BOF3_INJECT(PartyFormAction16_Form0);
    BOF3_INJECT(PartyAction16_Form0);
    BOF3_INJECT(PartyAction_ProbeBegin);
    BOF3_INJECT(PartyFormAction16_Form1);
    BOF3_INJECT(PartyAction16_Form1);
    BOF3_INJECT(PartyAction16_Form1Mode0);
    BOF3_INJECT(PartyAction16_StrikeBegin);
    BOF3_INJECT(PartyAction16_Strike);
    BOF3_INJECT(PartyAction16_StrikeCell);
    BOF3_INJECT(PartyAction16_Form1Mode1);
    BOF3_INJECT(PartyAction_ProbeStart);
    BOF3_INJECT(PartyAction_EffectCountdown);
    BOF3_INJECT(PartyFormAction16_Form2);
    BOF3_INJECT(PartyAction16_Form2);
    BOF3_INJECT(PartyAction16_Form2Begin);
    BOF3_INJECT(PartyAction16_Form2Resolve);
    BOF3_INJECT(PartyAction16_CellPickup);
    BOF3_INJECT(PartyFormAction16_ByForm);
    BOF3_INJECT(PartyAction16_ByForm);
    BOF3_INJECT(PartyFormAction17_Form0);
    BOF3_INJECT(PartyAction17_Form0);
    BOF3_INJECT(PartyFormAction17_Form1);
    BOF3_INJECT(PartyAction17_Form1);
    BOF3_INJECT(PartyAction17_Form1Begin);
    BOF3_INJECT(PartyAction17_Form1Resolve);
    BOF3_INJECT(PartyAction17_CellPickup);
    BOF3_INJECT(Effect_SpawnAtCell);
    BOF3_INJECT(PartyFormAction17_Form2);
    BOF3_INJECT(PartyAction17_Form2);
    BOF3_INJECT(PartyFormAction17_ByForm);
    BOF3_INJECT(PartyAction17_ByForm);
    BOF3_INJECT(PartyFormAction18_Form0);
    BOF3_INJECT(PartyAction18_Form0);
    BOF3_INJECT(PartyAction18_Form0Sub0);
    BOF3_INJECT(PartyAction18_Form0Sub0Begin);
    BOF3_INJECT(PartyAction18_Form0Sub0Strike);
    BOF3_INJECT(PartyAction18_CellStrike);
    BOF3_INJECT(PartyAction18_Form0Sub1);
    BOF3_INJECT(PartyAction18_ProbeStart);
    BOF3_INJECT(PartyAction_SideProbes);
    BOF3_INJECT(PartyFormAction18_Form1);
    BOF3_INJECT(PartyAction18_Form1);
    BOF3_INJECT(PartyAction18_Form1Begin);
    BOF3_INJECT(PartyAction18_Form1Resolve);
    BOF3_INJECT(PartyAction18_CellPickup);
    BOF3_INJECT(PartyFormAction18_Form2);
    BOF3_INJECT(PartyAction18_Form2);
    BOF3_INJECT(PartyAction_SpawnKind1B);
    BOF3_INJECT(PartyFormAction18_ByForm);
    BOF3_INJECT(PartyAction18_ByForm);
}

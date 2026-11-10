// Moved here by tools/move_funcs.py from area_w2f.cpp (docs/source-layout-round.md).
#include "game/area/area_111.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w2f_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w2f::at;

}  // namespace

namespace {

using area_harness::Mem;

using move_script::Long;

using move_script::SetWord;

using move_script::Word;

using U = std::uint32_t;

unsigned char& B(U address) { return *Mem(address); }

void AddWord(unsigned char* p, int v) { SetWord(p, static_cast<unsigned>(Word(p) + v)); }

U Addr(const void* p) { return Low32(p); }

// Sprite_ObjectsExtra record k (0xA4 bytes; k not checked, as the originals).
unsigned char* Extra(unsigned k) { return Mem(::bof3::Img(Sprite_ObjectsExtra_addr) + k * at::kExtraStride); }

// Field object k (Sprite_Objects, 0xA4 bytes).
unsigned char* ObjectAt(unsigned k) { return rv::Bytes(Sprite_Objects) + k * 0xA4; }

void SetMessage(unsigned id) { SetWord(Mem(::bof3::Img(at::kMessage.v)), id); }

unsigned char* StoryFlags() { return Mem(::bof3::Img(Cond_Flags_addr) + 0xa0); }

bool Test(unsigned index) { return AH_CALL(Flags_Test)(StoryFlags(), index) != 0; }

void Set(unsigned index) { AH_CALL(Flags_Set)(StoryFlags(), index); }

void Clear(unsigned index) { AH_CALL(Flags_Clear)(StoryFlags(), index); }

// The tail's word timer counted down (a 16-bit dec): the new value.
unsigned DecTimer() {
    const unsigned v = (Word(Mem(::bof3::Img(at::kTailTimer.v))) - 1u) & 0xFFFFu;
    SetWord(Mem(::bof3::Img(at::kTailTimer.v)), v);
    return v;
}

// The movement script's position (MoveScript_Object's word +0xA) moved by v.
void ScriptStep(int v) { AddWord(MoveScript_Object + 0xA, v); }

// The map setters as the originals pass them: x and z words, a value byte.
void SetByte(unsigned x, unsigned z, unsigned value) { AH_CALL(AreaMap_SetByte)(x, z, value); }

// `(v - edx) >> 1` after cdq: the division by 2 that truncates toward 0.
int Half(int v) { return v / 2; }

// The x87-free `sar reg, 1`: the division by 2 that rounds down.
int Floor2(int v) { return v >> 1; }

// A signed 16-bit word read as an int.
int S16(const unsigned char* p) { return static_cast<std::int16_t>(Word(p)); }

// Field_State's party slot + 1 as the originals compute it: (Field_State -
// ObjTrio) / 0x14C (a signed divide), + 1, a byte.
unsigned char FieldStateSlot() {
    const auto d = static_cast<std::int32_t>(Addr(Field_State) - ::bof3::Img(ObjTrio_addr));
    return static_cast<unsigned char>(d / 0x14C + 1);
}

// Field_ActiveMember's index among Sprite_ObjectsExtra's records as the
// originals compute it: (Field_ActiveMember - 0x802000) / 0xA4, signed.
std::int32_t ActiveMemberExtra() {
    return static_cast<std::int32_t>(Addr(Field_ActiveMember) - ::bof3::Img(Sprite_ObjectsExtra_addr)) / static_cast<std::int32_t>(at::kExtraStride);
}

// The nibble of the 7 x 7 grid at (col, row): the byte (col >> 1) + row * 4,
// its low nibble for an odd column, its high one for an even column. The
// originals index by bytes they do not check; every such read stays inside
// .data (at most 0x675C00 + 0x7F + 0x3FC).
unsigned char Nibble(unsigned col, unsigned row) {
    const unsigned char b = B(::bof3::Img(Area111_Grid_addr) + (col >> 1) + row * 4);
    return (col & 1) != 0 ? static_cast<unsigned char>(b & 0xF) : static_cast<unsigned char>(b >> 4);
}

// The leader's grid cell, as three functions compute it: ((s16 x high word -
// 0xB) / 2, (s16 z high word - 0xB1) / 2), each truncated toward 0, as bytes.
struct Cell {
    unsigned char col, row;
};

Cell LeaderCell() {
    return {static_cast<unsigned char>(Half(S16(Mem(::bof3::Img(ObjTrio_addr) + 0x36)) - 0xB)), static_cast<unsigned char>(Half(S16(Mem(::bof3::Img(ObjTrio_addr) + 0x3a)) - 0xB1))};
}

// Area 110's init (0x417580, area 72's code over area 110's tables): a
// weighted draw - Rand() & 0x3F walked down the eight weights, the first it
// falls below chosen (none: 8); field objects 0..7 but the chosen one get +0
// = 0; the chosen one put at one of the eight cells (Rand() & 7: x and z
// bytes << 16), its word +0x3E the ground there; Field_StepCount = the
// leader's zone counter less 5 (either way).
void PlaceRandomObject(U cells, U weights) {
    auto roll = static_cast<unsigned char>(AH_CALL(Rand)() & 0x3F);
    unsigned chosen = 0;
    for (; chosen < 8; ++chosen) {
        const unsigned char weight = B(weights + chosen);
        if (roll < weight) break;
        roll = static_cast<unsigned char>(roll - weight);
    }
    for (unsigned k = 0; k < 8; ++k)
        if (k != chosen) ObjectAt(k)[0] = 0;
    if (chosen < 8) {
        const unsigned pick = static_cast<unsigned char>(AH_CALL(Rand)() & 7);
        unsigned char* const object = ObjectAt(chosen);
        auto* const tObject = rv::As<records::SpriteObject>(object);
        const auto x = static_cast<std::int32_t>(static_cast<U>(B(cells + pick * 2)) << 16);
        const auto z = static_cast<std::int32_t>(static_cast<U>(B(cells + pick * 2 + 1)) << 16);
        tObject->x = x;
        tObject->z = z;
        const long ground = AH_CALL(AreaMap_Elevation)(tObject->x, z);
        rv::Store<unsigned>(rv::As<records::SpriteObject>(object)->height_whole, static_cast<unsigned>(ground));
    }
    Field_StepCount = static_cast<unsigned short>(Long(Mem(::bof3::Img(ObjTrio_addr) + 0x134)) - 5);
}

// Area 111's arrive hook's edge tests: the half-cell high word `a` (the
// coordinate the edge lies on) at `at`, the other `b` within [lo, lo + span)
// (16 bits), and the cell step of the leader's direction along a (x: +0, z:
// +1) such that at - step is `from`.
bool Edge(std::int16_t a, int at, std::int16_t b, unsigned lo, unsigned span, unsigned step_offset, int from, const unsigned char* cur) {
    if (a != at) return false;
    if (static_cast<std::uint16_t>(b - lo) >= span) return false;
    const unsigned d = (cur[8] & 7u) * 2 + step_offset;
    return at - static_cast<signed char>(B(::bof3::Img(at::kCellDelta.v) + d)) == from;
}

}  // namespace

// original 0x417580 (area 110 +0x40; PSX 0x801F2D2C): a jmp over eleven nops
// to its body 0x417590, area 72's code (Area72_PlaceRandomObject) over
// Area110_Cells / Area110_Weights.
extern "C" void __cdecl Area110_PlaceRandomObject(void) { PlaceRandomObject(::bof3::Img(Area110_Cells_addr), ::bof3::Img(Area110_Weights_addr)); }

// original 0x417650 (area 111 +0x34[0]): the message word 3 for an answer,
// else 2.
extern "C" void __cdecl Area111_ChoiceMessage2(void) { SetMessage(B(::bof3::Img(AreaW0c_ChoiceAnswer_addr)) != 0 ? 3u : 2u); }

// original 0x417670 (area 111 +0x34[1]): the message word 0xFFFF; answer 0:
// ScriptFlags_Set40, MoveScript_Var7 = 5, the run's step 0.
extern "C" void __cdecl Area111_ChoiceStartVar7(void) {
    const unsigned char answer = B(::bof3::Img(AreaW0c_ChoiceAnswer_addr));
    SetMessage(0xFFFF);
    if (answer != 0) return;
    AH_CALL(ScriptFlags_Set40)();
    MoveScript_Var7 = 5;
    B(::bof3::Img(MoveScript_PartyRecords_addr) + 0x65) = 0;
}

// original 0x4176A0 (area 111 +0x3C[0]; PSX 0x801F3448): extra object 0's
// dword +0x6C = extra object 1's.
extern "C" void __cdecl Area111_CopyExtra6C(void) { rv::Store<std::int32_t>(rv::As<records::SpriteObject>(Extra(0))->angle[2], static_cast<std::int32_t>(rv::As<records::SpriteObject>(Extra(1))->angle[2])); }

// original 0x4176B0 (area 111 +0x3C[1]; PSX 0x801F3460): the running
// object's grid cell (x, z high words less 0xB / 0xB1, halved rounding down)
// marked 0x10 (Area111_MarkCells); then from it, by the object's direction
// (the cell steps, re-read after the call), cells stepped while inside the
// grid (16-bit tests, 0..6) and their nibble 0, the count + 4 each (a byte);
// a count: the script object's +7 = count + 1. The object's dword +0x14 = 0,
// the cell it stopped at (outside or blocked) marked 0, Sound_PlayEffect(0x203).
extern "C" void __cdecl Area111_SlideMark(void) {
    const unsigned char* const cur = Sprite_Current;
    int z = Floor2(S16(cur + 0x3A) - 0xB1);
    int x = Floor2(S16(cur + 0x36) - 0xB);
    unsigned char count = 0;
    AH_CALL(Area111_MarkCells)(x, z, 0x10);
    const unsigned d = (Sprite_Current[8] & 7u) * 2;
    for (;;) {
        x += static_cast<signed char>(B(::bof3::Img(at::kCellDelta.v) + d));
        if (static_cast<std::int16_t>(x) < 0 || static_cast<std::int16_t>(x) > 6) break;
        z += static_cast<signed char>(B(::bof3::Img(at::kCellDelta.v) + d + 1));
        if (static_cast<std::int16_t>(z) < 0 || static_cast<std::int16_t>(z) > 6) break;
        if (Nibble(static_cast<unsigned>(static_cast<std::int16_t>(x)), static_cast<unsigned>(static_cast<std::int16_t>(z))) != 0) break;
        count = static_cast<unsigned char>(count + 4);
    }
    if (count != 0) MoveScript_Object[7] = static_cast<unsigned char>(count + 1);
    rv::Current<records::SpriteObject>()->step_y = 0;
    AH_CALL(Area111_MarkCells)(x, z, 0);
    AH_CALL(Sound_PlayEffect)(0x203);
}

// original 0x417790 (area 111 +0x3C[2]; PSX 0x801F35F4): the grid's nibble at
// the leader's cell (bytes, unchecked; inside .data), less 1; 0xFF (an empty
// cell): nothing. Else MoveCmd_Attach(the script object, 7, that, Field_State's
// slot + 1, 0x10) with the script position kept across the call; the running
// object's direction = extra object that's +8 & 7, and Sprite_Kind2's +8 =
// it.
extern "C" void __cdecl Area111_AttachAtLeaderCell(void) {
    const Cell c = LeaderCell();
    const auto block = static_cast<unsigned char>(Nibble(c.col, c.row) - 1);
    if (block == 0xFF) return;
    const unsigned char slot = FieldStateSlot();
    unsigned char* const object = MoveScript_Object;
    const std::uint16_t position = rv::As<records::ScriptContext>(object)->position;
    AH_CALL(MoveCmd_Attach)(object, 7, block, slot, 0x10);
    rv::Store<unsigned>(rv::As<records::ScriptContext>(MoveScript_Object)->position, position);
    Sprite_Current[8] = static_cast<unsigned char>(Extra(block)[8] & 7);
    Sprite_Kind2.direction = Sprite_Current[8];
}

// original 0x417890 (area 111 +0x3C[3]; PSX 0x801F3768): counter 3 = 0x10 <<
// the running object's dword +0x18 (a byte shift, the count masked to 5
// bits).
extern "C" void __cdecl Area111_Counter3Bit(void) {
    const unsigned shift = static_cast<U>(rv::Current<records::SpriteObject>()->work_18) & 31;
    B(::bof3::Img(at::kCounter3.v)) = static_cast<unsigned char>(0x10u << shift);
}

// original 0x4178B0 (area 111 +0x3C[4]; PSX 0x801F3790): unless counter 3 is
// 0x10 << the active member's index among the extra objects (a dword shift,
// the count masked to 5 bits), the script position - 2.
extern "C" void __cdecl Area111_WaitMemberBit(void) {
    const unsigned shift = static_cast<U>(ActiveMemberExtra()) & 31;
    if (static_cast<U>(B(::bof3::Img(at::kCounter3.v))) != (0x10u << shift)) ScriptStep(-2);
}

// original 0x4178F0 (area 111 +0x3C[5]; PSX 0x801F3808): the key = the active
// member's extra index + 1 (a byte). The running object's cell (x, z high
// words less 0xB / 0xB1, halved toward 0) marked 0 (Area111_MarkCells); every
// nibble of the grid holding the key cleared (each byte seen once for each of
// its two columns: its low nibble first); the key or'd into the object's
// cell's nibble; Sound_PlayEffect(0x204). The cell is not checked: ours
// aborts where the byte written would lie outside the 28-byte grid.
extern "C" void __cdecl Area111_PlaceMemberInGrid(void) {
    const auto key = static_cast<unsigned char>(ActiveMemberExtra() + 1);
    const unsigned char* const cur = Sprite_Current;
    const int x = Half(S16(cur + 0x36) - 0xB);
    const int z = Half(S16(cur + 0x3A) - 0xB1);
    AH_CALL(Area111_MarkCells)(x, z, 0);
    for (unsigned row = 0; row < at::kArea111GridSide; ++row) {
        for (unsigned col = 0; col < at::kArea111GridSide; ++col) {
            unsigned char& b = B(::bof3::Img(Area111_Grid_addr) + (col >> 1) + row * 4);
            if ((b & 0xF) == key)
                b = static_cast<unsigned char>(b & 0xF0);
            else if ((b >> 4) == key)
                b = static_cast<unsigned char>(b & 0xF);
        }
    }
    const int offset = Half(static_cast<std::int16_t>(x)) + static_cast<std::int16_t>(z) * 4;
    if (offset < 0 || offset >= static_cast<int>(at::kArea111GridBytes))
        bof3::Fatal("Area111_PlaceMemberInGrid: the cell (%d, %d) lies outside the 7 x 7 grid 0x%X", x, z,
                    static_cast<unsigned>(::bof3::Img(Area111_Grid_addr)));
    unsigned char& b = B(::bof3::Img(Area111_Grid_addr) + static_cast<U>(offset));
    b = static_cast<unsigned char>(b | ((x & 1) != 0 ? key : static_cast<unsigned char>(key << 4)));
    AH_CALL(Sound_PlayEffect)(0x204);
}

// original 0x4179F0 (area 111 +0x3C[6]; PSX 0x801F39E0): extra object k's
// (k the running object's byte +0x18, unchecked; inside .data) cell, halved
// toward 0, stepped once by the running object's direction (bytes); outside
// 0..6 (unsigned): nothing. Its nibble less 1 at 4 or more: nothing. Else
// MoveCmd_Attach(the script object, 7, that block, Field_State's slot + 1,
// 0xC) with the script position kept across the call; extra object block's +8
// = the running object's direction & 7; the script position - 6.
extern "C" void __cdecl Area111_AttachAhead(void) {
    const unsigned char* const cur = Sprite_Current;
    const unsigned char k = cur[0x18];
    const unsigned d = (cur[8] & 7u) * 2;
    const unsigned char* const extra = Extra(k);
    const auto col = static_cast<unsigned char>(Half(S16(extra + 0x36) - 0xB) + B(::bof3::Img(at::kCellDelta.v) + d));
    const auto row = static_cast<unsigned char>(Half(S16(extra + 0x3A) - 0xB1) + B(::bof3::Img(at::kCellDelta.v) + d + 1));
    if (col >= 7 || row >= 7) return;
    const auto block = static_cast<unsigned char>(Nibble(col, row) - 1);
    if (block >= 4) return;
    const unsigned char slot = FieldStateSlot();
    unsigned char* const object = MoveScript_Object;
    const std::uint16_t position = rv::As<records::ScriptContext>(object)->position;
    AH_CALL(MoveCmd_Attach)(object, 7, block, slot, 0xC);
    rv::Store<unsigned>(rv::As<records::ScriptContext>(MoveScript_Object)->position, position);
    Extra(block)[8] = static_cast<unsigned char>(Sprite_Current[8] & 7);
    ScriptStep(-6);
}

// original 0x417B30 (area 111 +0x3C[7]; PSX 0x801F3BC0): from the running
// object's cell (the 16-bit high words less 0xB / 0xB1, shifted right once:
// bytes) cells stepped by its direction while inside the grid (signed bytes,
// 0..6) and their nibble not 0xF, the count + 4 each. A count: the script
// object's +7 = count + 1, MoveCmd_MoveKind2(the object's direction byte),
// MoveScript_FAWord 0, the script object's +0 bit 0x40, the running object's
// dword +0x14 = 0 and word +0x3E = 0xFB00.
extern "C" void __cdecl Area111_SlideAhead(void) {
    const unsigned char* const cur = Sprite_Current;
    auto col = static_cast<unsigned char>(static_cast<std::uint16_t>(rv::As<records::SpriteObject>(cur)->x_whole - 0xB) >> 1);
    auto row = static_cast<unsigned char>(static_cast<std::uint16_t>(rv::As<records::SpriteObject>(cur)->z_whole - 0xB1) >> 1);
    const unsigned d = (cur[8] & 7u) * 2;
    const unsigned char step = B(::bof3::Img(at::kCellDelta.v) + d);
    unsigned char count = 0;
    for (;;) {
        col = static_cast<unsigned char>(col + step);
        if (static_cast<signed char>(col) < 0 || static_cast<signed char>(col) > 6) break;
        row = static_cast<unsigned char>(row + B(::bof3::Img(at::kCellDelta.v) + d + 1));
        if (static_cast<signed char>(row) < 0 || static_cast<signed char>(row) > 6) break;
        if (Nibble(col, row) == 0xF) break;
        count = static_cast<unsigned char>(count + 4);
    }
    if (count == 0) return;
    MoveScript_Object[7] = static_cast<unsigned char>(count + 1);
    AH_CALL(MoveCmd_MoveKind2)(Sprite_Current[8]);
    MoveScript_FAWord = 0;
    MoveScript_Object[0] = static_cast<unsigned char>(MoveScript_Object[0] | 0x40);
    rv::Current<records::SpriteObject>()->step_y = 0;
    rv::Store<unsigned>(rv::Current<records::SpriteObject>()->height_whole, 0xFB00);
}

// original 0x417C10 (area 111 +0x3C[8]; PSX 0x801F3D3C): extra object k's
// (k the running object's dword +0x18) x and z, their high words stepped by
// twice the cell step of the object's direction; the ground there less the
// object's word +0x3E (16 bits). With the script object's +4 set: the object's
// +9 = 0x20 and its dwords +0xC, +0x10, +0x14 = the x, z and height
// differences / 32 (signed, toward 0). k is not checked: ours aborts where
// the record would lie outside the image's .data.
extern "C" void __cdecl Area111_StepToExtra(void) {
    const unsigned char* const cur = Sprite_Current;
    const U k = static_cast<U>(rv::As<records::SpriteHead>(cur)->work_18);
    const U record = ::bof3::Img(Sprite_ObjectsExtra_addr) + k * at::kExtraStride;
    if (record < ::bof3::Img(at::kDataStart) || record > ::bof3::Img(at::kDataEnd) - 0x3C)
        bof3::Fatal("Area111_StepToExtra: extra object %u lies at 0x%X (VA 0x%X), outside the image's .data", k, record,
                    ::bof3::VaOf(record));
    const unsigned char* const extra = Mem(record);
    const U z0 = static_cast<U>(Long(cur + 0x38));
    U x = static_cast<U>(rv::As<records::SpriteObject>(extra)->x);
    U z = static_cast<U>(rv::As<records::SpriteObject>(extra)->z);
    const unsigned d = (cur[8] & 7u) * 2;
    const auto sx = static_cast<std::uint16_t>(static_cast<signed char>(B(::bof3::Img(at::kCellDelta.v) + d)) * 2);
    const auto sz = static_cast<std::uint16_t>(static_cast<signed char>(B(::bof3::Img(at::kCellDelta.v) + d + 1)) * 2);
    x = (x & 0xFFFFu) | static_cast<U>(static_cast<std::uint16_t>((x >> 16) + sx)) << 16;
    z = (z & 0xFFFFu) | static_cast<U>(static_cast<std::uint16_t>((z >> 16) + sz)) << 16;
    const auto dx = static_cast<std::int32_t>(x - static_cast<U>(rv::As<records::SpriteHead>(cur)->x));
    const auto dz = static_cast<std::int32_t>(z - z0);
    const long ground = AH_CALL(AreaMap_Elevation)(static_cast<long>(x), static_cast<long>(z));
    const auto dy = static_cast<std::int16_t>(static_cast<std::uint16_t>(ground) - rv::Current<records::SpriteObject>()->height_whole);
    if (MoveScript_Object[4] == 0) return;
    Sprite_Current[9] = 0x20;
    rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->step_x, dx / 32);
    rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->step_z, dz / 32);
    rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->step_y, static_cast<std::int32_t>(dy) / 32);
}

// original 0x417CF0 (Field_ModeTailKinds[17], armed by Area111_ArriveHook and
// Area111_ArmTailAtLeaderCell): the s8 state less 1 through the byte table
// 0x417EB8 (24 entries) to the jump table 0x417E98 (8), unsigned above 0x17:
// nothing.
//   1, 3, 5, 7: Party_DropIn(0), the timer and the state 0xA.
//   0xA: the timer down to 0: kind, state and sub-kind 0, ScriptFlags_Clear40.
//   0x14: MoveCmd_TestFB(0x11, 0xB7), the timer 0x1E, state 0x15.
//   0x15: the timer down to 0: the divisor from Field_MoveSpeeds[4], Kind2 X /
//         Z = (0x118000, 0xAF0000), MoveScript_FAWord 0, state 0x16.
//   0x16: Field_Kind2Hold 0: MoveCmd_TestFB(0x11, 0xAD), the map bytes (0x11,
//         0xAD), (0x12, 0xAD) = 0xA1 and (0x11, 0xAE), (0x12, 0xAE) = 0xC0,
//         Sound_PlayEffect(0x205), story flag 0x28, the timer 0x1E, state 0x17.
//   0x17: the timer down to 0: the divisor from speed 4, Kind2 X / Z the
//         leader's, FAWord 0, state 0x18.
//   0x18: Kind2Hold 0: the timer 1, state 0xA.
//   Any other: nothing.
extern "C" void __cdecl Area111_TailGate(void) {
    const auto state = static_cast<signed char>(B(::bof3::Img(AreaW0c_TailState_addr)));
    switch (state) {
    case 1: case 3: case 5: case 7:
        AH_CALL(Party_DropIn)(0);
        SetWord(Mem(::bof3::Img(at::kTailTimer.v)), 0xA);
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0xA;
        return;
    case 0xA:
        if (DecTimer() != 0) return;
        B(::bof3::Img(AreaW0a_TailKind_addr)) = 0;
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0;
        B(::bof3::Img(AreaW0c_TailSub_addr)) = 0;
        AH_CALL(ScriptFlags_Clear40)();
        return;
    case 0x14:
        AH_CALL(MoveCmd_TestFB)(0x11, 0xB7);
        SetWord(Mem(::bof3::Img(at::kTailTimer.v)), 0x1E);
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0x15;
        return;
    case 0x15:
        if (DecTimer() != 0) return;
        Field_Kind2X = 0x118000;
        Field_Kind2Z = 0xAF0000;
        MoveScript_F3Divisor = static_cast<short>(B(::bof3::Img(Field_MoveSpeeds_addr) + 0x4) << 3);
        MoveScript_FAWord = 0;
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0x16;
        return;
    case 0x16:
        if (Field_Kind2Hold != 0) return;
        AH_CALL(MoveCmd_TestFB)(0x11, 0xAD);
        SetByte(0x11, 0xAD, 0xA1);
        SetByte(0x12, 0xAD, 0xA1);
        SetByte(0x11, 0xAE, 0xC0);
        SetByte(0x12, 0xAE, 0xC0);
        AH_CALL(Sound_PlayEffect)(0x205);
        Set(0x28);
        SetWord(Mem(::bof3::Img(at::kTailTimer.v)), 0x1E);
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0x17;
        return;
    case 0x17:
        if (DecTimer() != 0) return;
        Field_Kind2X = Long(Mem(::bof3::Img(ObjTrio_addr) + 0x34));
        Field_Kind2Z = Long(Mem(::bof3::Img(ObjTrio_addr) + 0x38));
        MoveScript_F3Divisor = static_cast<short>(B(::bof3::Img(Field_MoveSpeeds_addr) + 0x4) << 3);
        MoveScript_FAWord = 0;
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0x18;
        return;
    case 0x18:
        if (Field_Kind2Hold != 0) return;
        SetWord(Mem(::bof3::Img(at::kTailTimer.v)), 1);
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0xA;
        return;
    default: return;
    }
}

// original 0x417ED0 (Area_ArriveHook's case for area 111, 0x56E53B): with
// Cond_ByteFD 0, story flag 0x28 cleared; unless Cond_ByteFD (read again) is
// 1, al 0. The cell in half cells (x >> 15, z >> 15, 16 bits) on one of the
// grid's four edges, arriving across it (the leader's direction's step):
// Area111_ArmTailAtLeaderCell(3 / 7 / 5 / 1 by the edge), al 0; the gates
// (x = 0x20 / 0x26, z = 0x16C / 0x172 over three cells) likewise; else the
// cell (x high word 0x11..0x12, z 0xB7..0xB8) with flag 0x28 clear:
// ScriptFlags_Set40, tail kind 0x11, state 0x14. al 0 always.
extern "C" unsigned char __cdecl Area111_ArriveHook(long x, long z) {
    if (Cond_ByteFD == 0) Clear(0x28);
    if (Cond_ByteFD != 1) return 0;
    const unsigned char* const cur = Sprite_Current;
    const auto hx = static_cast<std::int16_t>(static_cast<std::int32_t>(x) >> 15);
    const auto hz = static_cast<std::int16_t>(static_cast<std::int32_t>(z) >> 15);
    const auto arm = [](unsigned char state) { AH_CALL(Area111_ArmTailAtLeaderCell)(state); };
    // x edges (0x16, 0x30) over z 0x162..0x17C
    if (Edge(hx, 0x16, hz, 0x162, 0x1B, 0, 0x15, cur)) {
        arm(3);
        return 0;
    }
    if (Edge(hx, 0x30, hz, 0x162, 0x1B, 0, 0x31, cur)) {
        arm(7);
        return 0;
    }
    // z edges (0x162, 0x17C) over x 0x16..0x30
    if (Edge(hz, 0x162, hx, 0x16, 0x1B, 1, 0x161, cur)) {
        arm(5);
        return 0;
    }
    if (Edge(hz, 0x17C, hx, 0x16, 0x1B, 1, 0x17D, cur)) {
        arm(1);
        return 0;
    }
    // the gates: x 0x20 / 0x26 over z 0x16E..0x170
    if (Edge(hx, 0x20, hz, 0x16E, 3, 0, 0x21, cur)) {
        arm(7);
        return 0;
    }
    if (Edge(hx, 0x26, hz, 0x16E, 3, 0, 0x25, cur)) {
        arm(3);
        return 0;
    }
    // z 0x16C / 0x172 over x 0x22..0x24
    if (Edge(hz, 0x16C, hx, 0x22, 3, 1, 0x16D, cur)) {
        arm(1);
        return 0;
    }
    if (Edge(hz, 0x172, hx, 0x22, 3, 1, 0x171, cur)) {
        arm(5);
        return 0;
    }
    const auto xh = static_cast<std::uint16_t>(static_cast<U>(x) >> 16);
    const auto zh = static_cast<std::uint16_t>(static_cast<U>(z) >> 16);
    if (static_cast<std::uint16_t>(xh - 0x11) < 2 && static_cast<std::uint16_t>(zh - 0xB7) < 2 && !Test(0x28)) {
        AH_CALL(ScriptFlags_Set40)();
        B(::bof3::Img(AreaW0a_TailKind_addr)) = 0x11;
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0x14;
    }
    return 0;
}

// original 0x418130 (called by Area111_ArriveHook): the grid's nibble at the
// leader's cell less 1 to the sub-kind; tail kind 0x11, the state `state`;
// extra object (that byte)'s +8 = state (unchecked: a nibble of 0 writes
// record 0xFF, inside .data); ScriptFlags_Set40.
extern "C" void __cdecl Area111_ArmTailAtLeaderCell(unsigned char state) {
    const Cell c = LeaderCell();
    const auto block = static_cast<unsigned char>(Nibble(c.col, c.row) - 1);
    B(::bof3::Img(AreaW0c_TailSub_addr)) = block;
    B(::bof3::Img(AreaW0a_TailKind_addr)) = 0x11;
    B(::bof3::Img(AreaW0c_TailState_addr)) = state;
    Extra(block)[8] = state;
    AH_CALL(ScriptFlags_Set40)();
}

// original 0x4181E0 (called by Area111_SlideMark, Area111_PlaceMemberInGrid,
// Area111_Init): the grid cell (x, z)'s four map bytes (2x + 0xB..0xC, 2z +
// 0xB1..0xB2) set to `value`.
extern "C" void __cdecl Area111_MarkCells(int x, int z, unsigned value) {
    const auto x0 = static_cast<unsigned>(x + x + 0xB);
    const auto x1 = static_cast<unsigned>(x + x + 0xC);
    const auto z0 = static_cast<unsigned>(z + z + 0xB1);
    const auto z1 = static_cast<unsigned>(z + z + 0xB2);
    SetByte(x0, z0, value);
    SetByte(x1, z0, value);
    SetByte(x0, z1, value);
    SetByte(x1, z1, value);
}

// original 0x418240 (area 111 +0x40; PSX 0x801F46A8): with Cond_ByteFD 0,
// story flag 0x28 cleared. Cond_ByteFD (read again) 1: every grid cell marked
// with its byte of Area111_CellBytes (rows of z, columns of x), the grid
// copied from Area111_GridStart, and the gate's map bytes (0x11..0x12, 0xAD)
// = 0x50 and (.., 0xAE) = 0 without flag 0x28, 0xA1 and 0xC0 with it.
// Cond_ByteFD 5: the map bytes (0xA..0xB, 8) = 0x50.
extern "C" void __cdecl Area111_Init(void) {
    if (Cond_ByteFD == 0) Clear(0x28);
    if (Cond_ByteFD == 1) {
        const unsigned char* bytes = Mem(::bof3::Img(Area111_CellBytes_addr));
        for (int row = 0; row < 7; ++row)
            for (int col = 0; col < 7; ++col) AH_CALL(Area111_MarkCells)(col, row, *bytes++);
        for (unsigned i = 0; i < at::kArea111GridBytes; ++i) B(::bof3::Img(Area111_Grid_addr) + i) = B(::bof3::Img(Area111_GridStart_addr) + i);
        if (!Test(0x28)) {
            SetByte(0x11, 0xAD, 0x50);
            SetByte(0x12, 0xAD, 0x50);
            SetByte(0x11, 0xAE, 0);
            SetByte(0x12, 0xAE, 0);
        } else {
            SetByte(0x11, 0xAD, 0xA1);
            SetByte(0x12, 0xAD, 0xA1);
            SetByte(0x11, 0xAE, 0xC0);
            SetByte(0x12, 0xAE, 0xC0);
        }
    }
    if (Cond_ByteFD == 5) {
        SetByte(0xA, 8, 0x50);
        SetByte(0xB, 8, 0x50);
    }
}

void Area111_Inject() {
    BOF3_INJECT(Area110_PlaceRandomObject);
    BOF3_INJECT(Area111_ChoiceMessage2);
    BOF3_INJECT(Area111_ChoiceStartVar7);
    BOF3_INJECT(Area111_CopyExtra6C);
    BOF3_INJECT(Area111_SlideMark);
    BOF3_INJECT(Area111_AttachAtLeaderCell);
    BOF3_INJECT(Area111_Counter3Bit);
    BOF3_INJECT(Area111_WaitMemberBit);
    BOF3_INJECT(Area111_PlaceMemberInGrid);
    BOF3_INJECT(Area111_AttachAhead);
    BOF3_INJECT(Area111_SlideAhead);
    BOF3_INJECT(Area111_StepToExtra);
    BOF3_INJECT(Area111_TailGate);
    BOF3_INJECT(Area111_ArriveHook);
    BOF3_INJECT(Area111_ArmTailAtLeaderCell);
    BOF3_INJECT(Area111_MarkCells);
    BOF3_INJECT(Area111_Init);
}

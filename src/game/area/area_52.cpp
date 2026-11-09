// Moved here by tools/move_funcs.py from area_w1c.cpp (docs/source-layout-round.md).
#include "game/area/area_52.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w1c.h"
#include "game/area_w1c_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w1c::at;

}  // namespace

namespace {

using area_harness::Mem;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

unsigned char& B(std::uint32_t address) { return *Mem(address); }

void AddWord(unsigned char* p, int v) { SetWord(p, static_cast<unsigned>(Word(p) + v)); }

std::uint16_t High(std::int32_t v) { return static_cast<std::uint16_t>(static_cast<std::uint32_t>(v) >> 16); }

// A dword shifted right as the originals' `sar`.
std::uint32_t Sar(std::int32_t v, unsigned n) { return static_cast<std::uint32_t>(v >> n); }

// Effect_Objects record `slot` (0x80 bytes).
unsigned char* EffectAt(unsigned slot) { return rv::Bytes(Effect_Objects) + slot * at::kEffectStride; }

// Party record `member` (ObjTrio, 0x14C bytes).
unsigned char* PartyAt(unsigned member) { return rv::Bytes(&ObjTrio[member]); }

// The four callees nobody owns, by their raw addresses (area_w1c_callees.h).
void FlagsToggle(unsigned char* bits, unsigned index) {
    AH_AT(void (__cdecl*)(unsigned char*, unsigned), at::kFlagsToggle.v)(bits, index);
}

void SetLayer(unsigned x, unsigned z, unsigned value) {
    AH_AT(void (__cdecl*)(unsigned long, unsigned long, unsigned), at::kSetLayerByte.v)(x, z, value);
}

// The map byte setter, as the originals pass it: x and z words, a value byte.
void SetByte(unsigned x, unsigned z, unsigned value) { AH_CALL(AreaMap_SetByte)(x, z, value); }

unsigned char ByteAt(unsigned x, unsigned z) {
    return AH_CALL(AreaMap_ByteAt)(static_cast<short>(x), static_cast<short>(z));
}

// Area 52 moves a block of 2 x 2 map cells: the object keeps the map bytes
// it covers in its +0x18 (row z) and +0x1C (row z + 1), the low byte column
// x, the next column x + 1; the cells it covers are marked 0x11 in the map
// and 0x10 in the second layer (0x572620), and a cell it leaves gets its
// byte back and a layer of 0.
//
// One column x of the block taken: rows z and z + 1 marked, their map bytes
// or'ed into +0x18 / +0x1C at `shift` (0 or 8). Sprite_Current read after
// each layer call, before the map read.
void StashColumn(unsigned x, unsigned z, unsigned shift) {
    SetLayer(x, z, 0x10);
    unsigned char* p = Sprite_Current + 0x18;
    unsigned b = ByteAt(x, z);
    SetLong(p, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(p)) | b << shift));
    SetByte(x, z, 0x11);
    SetLayer(x, z + 1, 0x10);
    p = Sprite_Current + 0x1C;
    b = ByteAt(x, z + 1);
    SetLong(p, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(p)) | b << shift));
    SetByte(x, z + 1, 0x11);
}

// One column x given back: its layer 0 and its bytes from +0x18 / +0x1C
// shifted right (arithmetically) by `shift`, Sprite_Current read after each
// layer call.
void RestoreColumn(unsigned x, unsigned z, unsigned shift) {
    SetLayer(x, z, 0);
    SetByte(x, z, Sar(rv::Current<records::SpriteHead>()->work_18, shift));
    SetLayer(x, z + 1, 0);
    SetByte(x, z + 1, Sar(rv::Current<records::SpriteHead>()->work_1c, shift));
}

// Area 52's handlers 3 and 4 (0x40A1D0, 0x40A260): Area52_BlockCell; with
// counter 3 at 0xA and the object's x not `stop`, Area52_DropColumn and the
// script position + 6; at 0x14, Area52_ShiftBlockX and, the object's x then
// 0x858000, the script position + 3 and the tail state 0x2D.
void MoveBlock(std::int32_t stop) {
    long x, z;
    if (!AH_CALL(Area52_BlockCell)(&x, &z)) return;
    const unsigned char counter = B(::bof3::Img(at::kCounter3.v));
    if (counter == 0xA) {
        if (rv::Current<records::SpriteObject>()->x == stop) return;
        AH_CALL(Area52_DropColumn)(x, z);
        AddWord(MoveScript_Object + 0xA, 6);
        return;
    }
    if (counter != 0x14) return;
    AH_CALL(Area52_ShiftBlockX)(x, z);
    if (rv::Current<records::SpriteObject>()->x != 0x858000) return;
    AddWord(MoveScript_Object + 0xA, 3);
    B(::bof3::Img(AreaW0c_TailState_addr)) = 0x2D;
}

// Area 52's handlers 6 and 7 (0x40A960, 0x40A9C0): Effect_FindFree; a slot
// gets z +0x38, x +0x34 (each read back), +0 = 1, kind +5 = 0x48, +0xB =
// `b`, and +0x3C the ground's elevation there (a signed word) << 8.
void SpawnEffect48(std::int32_t x, std::int32_t z, unsigned char b) {
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    if (slot == 0xFF) return;
    unsigned char* const e = EffectAt(slot);
    auto* const tE = rv::As<records::EffectObject>(e);
    tE->z = z;
    const std::int32_t zz = tE->z;
    tE->x = x;
    const std::int32_t xx = tE->x;
    e[0] = 1;
    e[5] = 0x48;
    const auto ground = static_cast<std::int16_t>(AH_CALL(AreaMap_Elevation)(xx, zz));
    e[0xB] = b;
    tE->height = static_cast<std::int32_t>(static_cast<std::uint32_t>(static_cast<std::int32_t>(ground)) << 8);
}

// Area 52's tail: a word timer 0x9039F6 counted down, below 4 the next state.
bool CountDownBelow4() {
    const auto left = static_cast<std::uint16_t>(Word(Mem(::bof3::Img(at::kTailTimer.v))) - 1);
    SetWord(Mem(::bof3::Img(at::kTailTimer.v)), left);
    return left < 4;
}

bool CountDownToZero() {
    const auto left = static_cast<std::uint16_t>(Word(Mem(::bof3::Img(at::kTailTimer.v))) - 1);
    SetWord(Mem(::bof3::Img(at::kTailTimer.v)), left);
    return left == 0;
}

// A timed scene begun: counter 3 = `counter`, extra object 0 shown (its
// byte +1 = 0), the timer 0x10, state `next`.
void BeginTimed(unsigned char counter, unsigned char next) {
    B(::bof3::Img(at::kCounter3.v)) = counter;
    B(::bof3::Img(Area52_ExtraObjects_addr) + 1) = 0;
    SetWord(Mem(::bof3::Img(at::kTailTimer.v)), 0x10);
    B(::bof3::Img(AreaW0c_TailState_addr)) = next;
}

// The tail disarmed: ScriptFlags_Clear40, kind and state 0.
void Disarm() {
    AH_CALL(ScriptFlags_Clear40)();
    B(::bof3::Img(AreaW0a_TailKind_addr)) = 0;
    B(::bof3::Img(AreaW0c_TailState_addr)) = 0;
}

// A timed scene ended: story flag `flag` cleared, extra object 0 hidden (its
// byte +1 = 0xFF), the tail disarmed.
void EndTimed(unsigned flag) {
    AH_CALL(Flags_Clear)(Mem(::bof3::Img(Cond_Flags_addr) + 0xa0), flag);
    B(::bof3::Img(Area52_ExtraObjects_addr) + 1) = 0xFF;
    Disarm();
}

}  // namespace

// original 0x409E20 (Area52_Handlers[0]; PSX 0x801F3280): with Field_Request
// 5, the script position back 2 and done. Else the running object's +0x18 and
// +0x1C cleared and the block under it taken: columns x and x + 1 (x, z the
// high words of its position less 0x8000) by StashColumn.
extern "C" void __cdecl Area52_StashBlock(void) {
    if (Field_Request == 5) {
        AddWord(MoveScript_Object + 0xA, -2);
        return;
    }
    unsigned char* const cur = Sprite_Current;
    auto* const tCur = rv::As<records::SpriteObject>(cur);
    const std::uint16_t z = High(static_cast<std::int32_t>(static_cast<std::uint32_t>(tCur->z) - 0x8000u));
    const std::uint16_t x = High(static_cast<std::int32_t>(static_cast<std::uint32_t>(tCur->x) - 0x8000u));
    tCur->work_18 = 0;
    rv::Current<records::SpriteObject>()->work_1c = 0;
    for (unsigned i = 0; i < 2; ++i) StashColumn(static_cast<std::uint16_t>(x + i), z, i * 8);
}

// original 0x409F20 (Area52_Handlers[1]; PSX 0x801F3414): Area52_BlockCell;
// if it answers, the block moves one row on: row z given back from +0x18
// (columns x, x + 1), +0x18 = +0x1C, +0x1C = 0, row z + 2 taken into +0x1C;
// the object's direction 5 and MoveCmd_Move(the script object, 5), its +7 =
// 2.
extern "C" void __cdecl Area52_ShiftBlockZ(void) {
    long xv, zv;
    if (!AH_CALL(Area52_BlockCell)(&xv, &zv)) return;
    std::uint16_t x = High(xv), z = High(zv);
    SetLayer(x, z, 0);
    SetByte(x, z, Sprite_Current[0x18]);
    SetLayer(static_cast<std::uint16_t>(x + 1), z, 0);
    SetByte(static_cast<std::uint16_t>(x + 1), z, Sar(rv::Current<records::SpriteObject>()->work_18, 8));
    unsigned char* const cur = Sprite_Current;
    auto* const tCur = rv::As<records::SpriteObject>(cur);
    tCur->work_18 = tCur->work_1c;
    rv::Current<records::SpriteObject>()->work_1c = 0;
    z = static_cast<std::uint16_t>(z + 2);
    SetLayer(x, z, 0x10);
    unsigned char* p = Sprite_Current + 0x1C;
    unsigned b = ByteAt(x, z);
    SetLong(p, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(p)) | b));
    SetByte(x, z, 0x11);
    x = static_cast<std::uint16_t>(x + 1);
    SetLayer(x, z, 0x10);
    p = Sprite_Current + 0x1C;
    b = ByteAt(x, z);
    SetLong(p, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(p)) | b << 8));
    SetByte(x, z, 0x11);
    Sprite_Current[8] = 5;
    AH_CALL(MoveCmd_Move)(MoveScript_Object, 5);
    MoveScript_Object[7] = 2;
}

// original 0x40A070 (Area52_Handlers[2]; PSX 0x801F35A0): with the running
// object exactly at (0x118000, 0x298000), the block goes back to its start:
// columns 0x11, 0x12 of rows 0x29, 0x2A given back (RestoreColumn), +0x18 and
// +0x1C cleared, z +0x38 = 0x78000, and the block at columns 0x11, 0x12 of
// the new z (high word of z less 0x8000) taken (StashColumn).
extern "C" void __cdecl Area52_ResetBlock(void) {
    const unsigned char* const cur = Sprite_Current;
    const auto* const tCur = rv::AsConst<records::SpriteObject>(cur);
    if (tCur->x != 0x118000 || tCur->z != 0x298000) return;
    for (unsigned i = 0; i < 2; ++i) RestoreColumn(0x11 + i, 0x29, i * 8);
    rv::Current<records::SpriteObject>()->work_18 = 0;
    rv::Current<records::SpriteObject>()->work_1c = 0;
    rv::Current<records::SpriteObject>()->z = 0x78000;
    const std::uint16_t z = High(static_cast<std::int32_t>(static_cast<std::uint32_t>(rv::Current<records::SpriteObject>()->z) - 0x8000u));
    for (unsigned i = 0; i < 2; ++i) StashColumn(0x11 + i, z, i * 8);
}

// originals 0x40A1D0 / 0x40A260 (Area52_Handlers[3], [4]; PSX 0x801F37A8,
// 0x801F3894): MoveBlock stopping at x 0x688000 / 0x668000.
extern "C" void __cdecl Area52_MoveBlockA(void) { MoveBlock(0x688000); }

extern "C" void __cdecl Area52_MoveBlockB(void) { MoveBlock(0x668000); }

// original 0x40A2F0 (called by area 52's handlers 1, 3, 4, 8, `(x, z)`
// pointers to the caller's two dwords, answering in al): *x = the running
// object's +0x34 less 0x8000, *z = its +0x38 less 0x8000; with
// Field_Request 5, Area52_RestoreBlock(*x, z). Counter 3 at 0: the script
// position back 2 and 0; else 1.
extern "C" unsigned char __cdecl Area52_BlockCell(long* x, long* z) {
    *x = static_cast<long>(static_cast<std::uint32_t>(rv::Current<records::SpriteHead>()->x) - 0x8000u);
    const auto zv = static_cast<long>(static_cast<std::uint32_t>(rv::Current<records::SpriteHead>()->z) - 0x8000u);
    *z = zv;
    if (Field_Request == 5) AH_CALL(Area52_RestoreBlock)(*x, zv);
    if (B(::bof3::Img(at::kCounter3.v)) != 0) return 1;
    AddWord(MoveScript_Object + 0xA, -2);
    return 0;
}

// original 0x40A350 (called by Area52_MoveBlockA / B, `(x, z)` the cell's
// dwords, their high words read): the block one column on in x: column x
// given back from the low bytes of +0x18 / +0x1C, both shifted right by 8
// (arithmetically), column x + 2 taken into their high bytes; the object's
// direction 3 and MoveCmd_Move(the script object, 3), its +7 = 2.
extern "C" void __cdecl Area52_ShiftBlockX(long xv, long zv) {
    const std::uint16_t x = High(xv), z = High(zv);
    const auto z1 = static_cast<std::uint16_t>(z + 1);
    SetLayer(x, z, 0);
    SetByte(x, z, Sprite_Current[0x18]);
    SetLayer(x, z1, 0);
    SetByte(x, z1, Sprite_Current[0x1C]);
    unsigned char* cur = Sprite_Current;
    rv::Store<std::int32_t>(rv::As<records::SpriteHead>(cur)->work_18, static_cast<std::int32_t>(Sar(rv::As<records::SpriteHead>(cur)->work_18, 8)));
    cur = Sprite_Current;
    rv::Store<std::int32_t>(rv::As<records::SpriteHead>(cur)->work_1c, static_cast<std::int32_t>(Sar(rv::As<records::SpriteHead>(cur)->work_1c, 8)));
    const auto x2 = static_cast<std::uint16_t>(x + 2);
    SetLayer(x2, z, 0x10);
    unsigned char* p = Sprite_Current + 0x18;
    unsigned b = ByteAt(x2, z);
    SetLong(p, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(p)) | b << 8));
    SetByte(x2, z, 0x11);
    SetLayer(x2, z1, 0x10);
    p = Sprite_Current + 0x1C;
    b = ByteAt(x2, z1);
    SetLong(p, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(p)) | b << 8));
    SetByte(x2, z1, 0x11);
    Sprite_Current[8] = 3;
    AH_CALL(MoveCmd_Move)(MoveScript_Object, 3);
    MoveScript_Object[7] = 2;
}

// original 0x40A450 (called by Area52_MoveBlockA / B, `(x, z)`, their high
// words read): column x + 1 given back from the high bytes of +0x18 / +0x1C
// (RestoreColumn at 8), then both shifted left by 8.
extern "C" void __cdecl Area52_DropColumn(long xv, long zv) {
    RestoreColumn(static_cast<std::uint16_t>(High(xv) + 1), High(zv), 8);
    rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->work_18, static_cast<std::int32_t>(static_cast<std::uint32_t>(rv::Current<records::SpriteHead>()->work_18) << 8));
    rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->work_1c, static_cast<std::int32_t>(static_cast<std::uint32_t>(rv::Current<records::SpriteHead>()->work_1c) << 8));
}

// original 0x40A4C0 (called by Area52_BlockCell, `(x, z)`, their high words
// read): the whole block given back, columns x and x + 1 (RestoreColumn at 0
// and 8).
extern "C" void __cdecl Area52_RestoreBlock(long xv, long zv) {
    for (unsigned i = 0; i < 2; ++i) RestoreColumn(static_cast<std::uint16_t>(High(xv) + i), High(zv), i * 8);
}

// original 0x40A530 (Area_CellHooks' entry for area 0x34, `(x, z)` answering
// in al): the first of Area52_CellSwitches' four records whose x and z bytes
// are the cell's and whose low nibble of byte 2 is the leader's byte +8;
// none: 0. Else ScriptFlags_Set40, the mode tail kind 15 (Area52_TailBlock)
// at the record's state; Area52_PartyInRect answering: state 0xA, answer 1;
// else the record's story flag toggled (0x57C160), Sound_PlayEffect(0x202),
// answer 1.
extern "C" int __cdecl Area52_CellHook(long x, long z) {
    const unsigned char pose = B(::bof3::Img(ObjTrio_addr) + 0x8);
    unsigned i = 0;
    for (; i < at::kArea52CellSwitchCount; ++i) {
        const unsigned char* const r = Mem(::bof3::Img(Area52_CellSwitches_addr) + i * 5u);
        if (static_cast<unsigned char>(x) == r[0] && static_cast<unsigned char>(z) == r[1] && (r[2] & 0xF) == pose) break;
    }
    if (i == at::kArea52CellSwitchCount) return 0;
    AH_CALL(ScriptFlags_Set40)();
    const unsigned char* const r = Mem(::bof3::Img(Area52_CellSwitches_addr) + i * 5u);
    B(::bof3::Img(AreaW0a_TailKind_addr)) = 0xF;
    B(::bof3::Img(AreaW0c_TailState_addr)) = r[4];
    if (AH_CALL(Area52_PartyInRect)()) {
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0xA;
        return 1;
    }
    FlagsToggle(Mem(::bof3::Img(Cond_Flags_addr) + 0xa0), r[3]);
    AH_CALL(Sound_PlayEffect)(0x202);
    return 1;
}

// original 0x40A5C0 (called by Area52_CellHook, answering in al): 1 when a
// member (below Field_MemberCount, read once) stands in Area52_Rects[the
// tail state not 0]: its x high word +0x36 less x0 below the width and its z
// high word +0x3A less z0 below the depth, as 16-bit unsigned compares; else
// 0.
extern "C" unsigned char __cdecl Area52_PartyInRect(void) {
    const bool armed = B(::bof3::Img(AreaW0c_TailState_addr)) != 0;
    const unsigned char count = Field_MemberCount;
    if (count == 0) return 0;
    const unsigned char* const rect = Mem(::bof3::Img(Area52_Rects_addr) + (armed ? 4u : 0u));
    for (unsigned char m = 0; m < count; ++m) {
        const unsigned char* const record = PartyAt(m);
        if (static_cast<std::uint16_t>(rv::As<records::PartyMember>(record)->x_whole - rect[0]) >= rect[2]) continue;
        if (static_cast<std::uint16_t>(rv::As<records::PartyMember>(record)->z_whole - rect[1]) < rect[3]) return 1;
    }
    return 0;
}

// original 0x40A670 (Field_ModeTailKinds[15], armed by Area52_CellHook): by
// the s8 state 0x9039F4 through a byte table and a 15-entry jump table in
// the body (0..0x2F; any other returns). Three timed scenes - states 0, 0x14,
// 0x28: counter 3 = 1 / 0xA / 0x14, extra object 0 shown, a 0x10 timer; then
// counter 3 = 0 once the timer is below 4; then at 0 story flag 0x24 / 0x26 /
// 0x25 cleared, extra object 0 hidden, the tail disarmed. 0xA:
// Msg_OpenScript(0x39) and Field_Request 2; 0xB: once Field_Request leaves 2,
// disarmed. 0x2D: counter 3 = 0; 0x2E: once counter 3 is 0x1E, 0x40A940 (area
// 52's second effect); 0x2F: once it is 0x28, columns 0x86 / 0x87 of rows
// 0x11, 0x12 given layer 0 and map bytes 0 / 0x10, the timer 0xA and state
// 0x29.
extern "C" void __cdecl Area52_TailBlock(void) {
    const auto state = static_cast<signed char>(B(::bof3::Img(AreaW0c_TailState_addr)));
    switch (state) {
    case 0: BeginTimed(1, 1); return;
    case 1:
        if (!CountDownBelow4()) return;
        B(::bof3::Img(at::kCounter3.v)) = 0;
        B(::bof3::Img(AreaW0c_TailState_addr)) = 2;
        return;
    case 2:
        if (CountDownToZero()) EndTimed(0x24);
        return;
    case 0xA:
        AH_CALL(Msg_OpenScript)(0x39);
        Field_Request = 2;
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0xB;
        return;
    case 0xB:
        if (Field_Request == 2) return;
        Disarm();
        return;
    case 0x14: BeginTimed(0xA, 0x15); return;
    case 0x15:
        if (!CountDownBelow4()) return;
        B(::bof3::Img(at::kCounter3.v)) = 0;
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0x16;
        return;
    case 0x16:
        if (CountDownToZero()) EndTimed(0x26);
        return;
    case 0x28: BeginTimed(0x14, 0x29); return;
    case 0x29:
        if (!CountDownBelow4()) return;
        B(::bof3::Img(at::kCounter3.v)) = 0;
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0x2A;
        return;
    case 0x2A:
        if (CountDownToZero()) EndTimed(0x25);
        return;
    case 0x2D:
        B(::bof3::Img(at::kCounter3.v)) = 0;
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0x2E;
        return;
    case 0x2E:
        if (B(::bof3::Img(at::kCounter3.v)) != 0x1E) return;
        AH_CALL(Area52_SpawnEffect48BJump)();
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0x2F;
        return;
    case 0x2F:
        if (B(::bof3::Img(at::kCounter3.v)) != 0x28) return;
        SetLayer(0x86, 0x11, 0);
        SetByte(0x86, 0x11, 0);
        SetLayer(0x86, 0x12, 0);
        SetByte(0x86, 0x12, 0);
        SetLayer(0x87, 0x11, 0);
        SetByte(0x87, 0x11, 0x10);
        SetLayer(0x87, 0x12, 0);
        SetByte(0x87, 0x12, 0x10);
        SetWord(Mem(::bof3::Img(at::kTailTimer.v)), 0xA);
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0x29;
        return;
    default: return;   // 3..9, 0xC..0x13, 0x17..0x27, 0x2B, 0x2C: the table's return; above 0x2F or negative: `ja` to it
    }
}

// original 0x40A940 (called by Area52_TailBlock's state 0x2E): a 5-byte
// `jmp 0x40A9C0` (Area52_SpawnEffect48B), a start of its own.
extern "C" void __cdecl Area52_SpawnEffect48BJump(void) { AH_CALL(Area52_SpawnEffect48B)(); }

// original 0x40A950 (Area52_Handlers[5]; PSX 0x801F4268): story flag 0x25
// cleared.
extern "C" void __cdecl Area52_ClearFlag25(void) { AH_CALL(Flags_Clear)(Mem(::bof3::Img(Cond_Flags_addr) + 0xa0), 0x25); }

// original 0x40A960 (Area52_Handlers[6]; PSX 0x801F4290): SpawnEffect48 at
// (0x880000, 0x110000), +0xB = 8.
extern "C" void __cdecl Area52_SpawnEffect48A(void) { SpawnEffect48(0x880000, 0x110000, 8); }

// original 0x40A9C0 (Area52_Handlers[7]; PSX 0x801F431C): SpawnEffect48 at
// (0x890000, 0x118000), +0xB = 0x10.
extern "C" void __cdecl Area52_SpawnEffect48B(void) { SpawnEffect48(0x890000, 0x118000, 0x10); }

// original 0x40AA20 (Area52_Handlers[8]; PSX 0x801F43B0): Area52_BlockCell
// (its answer not read); column x - 1 of rows z and z + 1 taken into the low
// bytes of +0x18 / +0x1C (or'ed, not shifted: Area52_DropColumn made room);
// the object's direction 7 and MoveCmd_Move(the script object, 7), its +7 =
// 2.
extern "C" void __cdecl Area52_ShiftBlockXBack(void) {
    long xv, zv;
    AH_CALL(Area52_BlockCell)(&xv, &zv);
    const auto x = static_cast<std::uint16_t>(High(xv) - 1);
    const std::uint16_t z = High(zv);
    const auto z1 = static_cast<std::uint16_t>(z + 1);
    SetLayer(x, z, 0x10);
    unsigned char* p = Sprite_Current + 0x18;
    unsigned b = ByteAt(x, z);
    SetLong(p, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(p)) | b));
    SetByte(x, z, 0x11);
    SetLayer(x, z1, 0x10);
    p = Sprite_Current + 0x1C;
    b = ByteAt(x, z1);
    SetLong(p, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(p)) | b));
    SetByte(x, z1, 0x11);
    Sprite_Current[8] = 7;
    AH_CALL(MoveCmd_Move)(MoveScript_Object, 7);
    MoveScript_Object[7] = 2;
}

void Area52_Inject() {
    BOF3_INJECT(Area52_StashBlock);
    BOF3_INJECT(Area52_ShiftBlockZ);
    BOF3_INJECT(Area52_ResetBlock);
    BOF3_INJECT(Area52_MoveBlockA);
    BOF3_INJECT(Area52_MoveBlockB);
    BOF3_INJECT(Area52_BlockCell);
    BOF3_INJECT(Area52_ShiftBlockX);
    BOF3_INJECT(Area52_DropColumn);
    BOF3_INJECT(Area52_RestoreBlock);
    BOF3_INJECT(Area52_CellHook);
    BOF3_INJECT(Area52_PartyInRect);
    BOF3_INJECT(Area52_TailBlock);
    BOF3_INJECT(Area52_SpawnEffect48BJump);
    BOF3_INJECT(Area52_ClearFlag25);
    BOF3_INJECT(Area52_SpawnEffect48A);
    BOF3_INJECT(Area52_SpawnEffect48B);
    BOF3_INJECT(Area52_ShiftBlockXBack);
}

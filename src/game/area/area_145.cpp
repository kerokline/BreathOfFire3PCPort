// Moved here by tools/move_funcs.py from area_w3f.cpp (docs/source-layout-round.md).
#include "game/area/area_145.h"

#include <bit>
#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w3f.h"
#include "game/area_w3f_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w3f::at;

}  // namespace

namespace {

using area_harness::Handler;

using area_harness::Mem;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

using U = std::uint32_t;

unsigned char& B(U address) { return *Mem(address); }

U Key(const void* p) { return Low32(p); }

void AddWord(unsigned char* p, int v) { SetWord(p, static_cast<unsigned>(Word(p) + v)); }

// Effect_Objects record `slot` (0x80 bytes; the slot is not checked, as the
// originals' `shl 7`).
unsigned char* EffectAt(unsigned slot) { return rv::Bytes(Effect_Objects) + slot * at::kEffectStride; }

// The high word of a 16.16 position, as the hooks read it (a word at +2).
std::uint16_t High(U v) { return static_cast<std::uint16_t>(v >> 16); }

void SetMessage(unsigned id) { SetWord(Mem(::bof3::Img(at::kMessage.v)), id); }

unsigned char Answer() { return B(::bof3::Img(AreaW0c_ChoiceAnswer_addr)); }

unsigned char* StoryFlags() { return Mem(::bof3::Img(Cond_Flags_addr) + 0xa0); }

// The movement script's position (MoveScript_Object's word +0xA) moved by `v`.
void ScriptStep(int v) { AddWord(MoveScript_Object + 0xA, v); }

// A state handler read from an area's .data table in place, as the
// originals' `jmp [index * 4 + table]`: the index is not checked there. Ours
// aborts past the table (what follows is data).
Handler StateEntry(const char* who, U table, unsigned index) {
    if (index >= at::kStateCount)
        bof3::Fatal("%s: state %u is past its %u-entry table 0x%X", who, index, at::kStateCount, static_cast<unsigned>(table));
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(table + index * 4u)))));
}

// The tail armed: ScriptFlags_Set40, then kind `kind`, state 0, sub-kind
// `sub` (the three object triggers, in that order).
unsigned char ArmTail(unsigned char kind, unsigned char sub) {
    AH_CALL(ScriptFlags_Set40)();
    B(::bof3::Img(AreaW0a_TailKind_addr)) = kind;
    B(::bof3::Img(AreaW0c_TailState_addr)) = 0;
    B(::bof3::Img(AreaW0c_TailSub_addr)) = sub;
    return 0;
}

// The leader's pose is one the hooks accept: 0, 7 or 6.
bool PoseStanding() {
    const unsigned char pose = B(::bof3::Img(ObjTrio_addr) + 0x8);
    return pose == 0 || pose == 7 || pose == 6;
}

// The step hooks' drop-in: counter 0 cleared, then Party_DropIn(entry), al 1.
unsigned char DropIn(unsigned entry) {
    B(::bof3::Img(at::kCounter0.v)) = 0;
    AH_CALL(Party_DropIn)(entry);
    return 1;
}

// Area 143's handler 2 and area 145's handlers 4..6 (0x420960, 0x421370,
// 0x421400, 0x421490, one body with its own two tables each): for each of
// the four keys in turn, each of Field_MemberCount's party records (the
// count read once, compared unsigned): the first record whose +0x89 is the
// key opens the key's message (Msg_OpenScript) and sets Field_Request 2.
// None: nothing. The records are not bounded by three (the count is).
void MessageByMember(U keys, U messages) {
    const unsigned char count = Field_MemberCount;
    for (unsigned i = 0; i < 4; ++i) {
        for (unsigned char j = 0; j < count; ++j) {
            if (B(::bof3::Img(ObjTrio_addr) + 0x89 + j * at::kPartyStride) != B(keys + i)) continue;
            AH_CALL(Msg_OpenScript)(static_cast<unsigned short>(Word(Mem(messages + i * 2))));
            Field_Request = 2;
            return;
        }
    }
}

// A leg: four bytes (a direction, a length, the next leg's table index, pad).
// The position `n` steps along the leg's direction from (x0, z0): the cell
// deltas (two signed bytes a direction) << 16, times n, 32-bit.
U AlongX(unsigned char dir, U n, U x0) {
    return static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(B(::bof3::Img(at::kCellDelta.v) + dir * 2u))) << 16) * n + x0;
}

U AlongZ(unsigned char dir, U n, U z0) {
    return static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(B(::bof3::Img(at::kCellDelta.v) + dir * 2u + 1))) << 16) * n + z0;
}

// Sprite_ObjectsExtra record k's dword +0x6C, bits 9..10: which of a leg
// table's four records.
unsigned Quarter(unsigned k) { return static_cast<unsigned>(Long(Mem(::bof3::Img(Sprite_ObjectsExtra_addr) + k * at::kObjectStride + 0x6C)) >> 9) & 3; }

// min(d, len) as the originals take it: 16-bit unsigned compare.
U Upto(std::uint16_t d, unsigned char len) { return d < len ? d : len; }

// Legs 1 and 2 (0x421C90, 0x421DB0): the same body with the bias `back`
// (5, 10), the tables for record 1 and for any other, and what follows (leg
// 2: none).
unsigned char TrailLeg(unsigned t, unsigned kk, unsigned back, U table1, U table_other, bool last) {
    const unsigned k = kk & 0xFF;
    const U off = k * at::kObjectStride;
    const unsigned q = Quarter(k);
    const U rec = (k == 1 ? table1 : table_other) + q * 4;
    const U d32 = t + (0x10000u - back);
    const auto d = static_cast<std::uint16_t>(d32);
    const unsigned char dir = B(rec);
    const U n = Upto(d, B(rec + 1));
    const U ox = static_cast<U>(Long(Mem(::bof3::Img(Sprite_ObjectsExtra_addr) + 0x34 + off)));
    const U x = AlongX(dir, n, ox);
    const U oz = static_cast<U>(Long(Mem(::bof3::Img(Sprite_ObjectsExtra_addr) + 0x38 + off)));
    const U z = AlongZ(dir, n, oz);
    AH_CALL(Area145_SpawnTrail)(static_cast<long>(ox), static_cast<long>(oz), static_cast<long>(x), static_cast<long>(z));
    const unsigned char len = B(rec + 1);
    if (d < len) return 1;
    if (last) return B(rec + 2);
    const unsigned char next = B(rec + 2);
    if (next == 0) return 1;
    if (d == len) AH_CALL(Sound_PlayEffect)(0x206);
    return AH_CALL(Area145_TrailLeg2)(d32, next);
}

// Area 145's effect spawn at SharedScratch: the slot byte stored there, read
// back as the originals do (a dword, masked).
unsigned char SpawnSlot() {
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    B(::bof3::Img(at::kSlotScratch.v)) = slot;
    return slot;
}

unsigned char* ScratchRecord() { return EffectAt(static_cast<U>(Long(Mem(::bof3::Img(at::kSlotScratch.v)))) & 0xFF); }

// Tail states 0xA and 0x14: an effect of kind 0x13, the camera turn, to the
// three target angles +0x64 / +0x68 / +0x6C (CameraTurn_Start turns each
// Camera_Angles[n] toward dword +0x64 + 4n over +9 frames: docs/frame-callees.md),
// here (a0, the camera's angle 1 kept, a2), +9 0x30. Its parameters were named
// x / y until the C3 naming pass.
void Kind13(unsigned char* e, U a0, std::int32_t angle, U a2) {
    e[0] = 1;
    e[5] = 0x13;
    rv::Store<std::int32_t>(rv::As<records::EffectObject>(e)->angle[0], static_cast<std::int32_t>(a0));
    rv::Store<std::int32_t>(rv::As<records::EffectObject>(e)->angle[1], angle);
    rv::Store<std::int32_t>(rv::As<records::EffectObject>(e)->angle[2], static_cast<std::int32_t>(a2));
    e[9] = 0x30;
}

std::int32_t CameraAngle1() { return static_cast<std::int16_t>(Word(Mem(::bof3::Img(Camera_Angles_addr) + 0x2))); }

void Disarm() {
    B(::bof3::Img(AreaW0a_TailKind_addr)) = 0;
    B(::bof3::Img(AreaW0c_TailState_addr)) = 0;
}

// The tail's word timer: not 0, one less and false; 0, true.
bool TimerOut() {
    const std::uint16_t t = Word(Mem(::bof3::Img(at::kTailTimer.v)));
    if (t != 0) {
        SetWord(Mem(::bof3::Img(at::kTailTimer.v)), t - 1u);
        return false;
    }
    return true;
}

}  // namespace

// original 0x421190 (area 145 +0x34[0]): message 0xFFFF; an answer: counter
// 0 = 8; 0: MoveScript_Var7 3, counter 0 = 0xA, the step 0xA.
extern "C" void __cdecl Area145_ChoiceRun3(void) {
    const unsigned char answer = Answer();
    SetMessage(0xFFFF);
    if (answer != 0) {
        B(::bof3::Img(at::kCounter0.v)) = 8;
        return;
    }
    MoveScript_Var7 = 3;
    B(::bof3::Img(at::kCounter0.v)) = 0xA;
    B(::bof3::Img(MoveScript_PartyRecords_addr) + 0x65) = 0xA;
}

// original 0x4211C0 (area 145 +0x3C[0] = +0x34[1]; PSX 0x801F3CE4): i = the
// active member's index in Sprite_ObjectsExtra ((pointer - 0x802000) / 0xA4,
// signed, truncated) as a byte, to 0x903851; MoveScript_PartyRecords record
// i: +2 = 0x10, +1 = 0, dword +0xC = 0x200 / (s8)+2 (read back: 0x20); the
// script object's +0 | 8.
extern "C" void __cdecl Area145_PartyRecord16(void) {
    const auto index = static_cast<unsigned char>(static_cast<std::int32_t>(Key(Field_ActiveMember) - ::bof3::Img(Sprite_ObjectsExtra_addr)) / 0xA4);
    B(::bof3::Img(at::kRecordIndex.v)) = index;
    unsigned char* const rec = Mem(::bof3::Img(MoveScript_PartyRecords_addr) + index * 16u);
    rec[2] = 0x10;
    rec[1] = 0;
    SetLong(rec + 0xC, 0x200 / static_cast<signed char>(rec[2]));
    MoveScript_Object[0] = static_cast<unsigned char>(MoveScript_Object[0] | 8);
}

// original 0x421220 (area 145 +0x3C[1] = +0x34[2]; PSX 0x801F3DCC):
// Area145_DropStates by the running object's +4 (unchecked there; ours
// aborts past its two).
extern "C" void __cdecl Area145_RunDrop(void) { StateEntry("Area145_RunDrop", ::bof3::Img(Area145_DropStates_addr), Sprite_Current[4])(); }

// original 0x421240 (Area145_DropStates[0]): the running object's bit 0x40
// cleared; its word +0x3E the ground at it (MapView_GroundAt) + 0x7D0; +0xC,
// +0x10, +0x14 0; +0x20 -8; state 1; Sprite_SetAnimation(0x3B); the script
// object's bit 0x40, its +1 = Field_State's party index (/ 0x14C) x 0x14,
// the script position - 2. Sprite_Current read again for every store.
extern "C" void __cdecl Area145_DropStart(void) {
    Sprite_Current[0] = static_cast<unsigned char>(Sprite_Current[0] & 0xBF);
    const long ground = AH_CALL(MapView_GroundAt)(rv::Current<records::SpriteObject>()->x, rv::Current<records::SpriteObject>()->z);
    rv::Store<unsigned>(rv::Current<records::SpriteObject>()->height_whole, static_cast<U>(ground) + 0x7D0u);
    rv::Current<records::SpriteObject>()->step_x = 0;
    rv::Current<records::SpriteObject>()->step_z = 0;
    rv::Current<records::SpriteObject>()->step_y = 0;
    rv::Store<std::int32_t>(rv::Current<records::SpriteObject>()->work_20, -8);
    Sprite_Current[4] = 1;
    AH_CALL(Sprite_SetAnimation)(0x3B);
    MoveScript_Object[0] = static_cast<unsigned char>(MoveScript_Object[0] | 0x40);
    const auto index = static_cast<std::int32_t>(Key(Field_State) - ::bof3::Img(0x802D40u)) / 0x14C;
    MoveScript_Object[1] = static_cast<unsigned char>(static_cast<U>(index) * 0x14u);
    ScriptStep(-2);
}

// original 0x4212F0 (Area145_DropStates[1]): +0x14 += +0x20;
// Field_LeaderStepTick; the ground at the running object above its word
// +0x3E (signed 16-bit): landed - +0x3E the ground, +0x14 and +0x20 0, state
// 2, Sprite_SetAnimation(0x39), the script object's +1 = 4. Else the script
// position - 2.
extern "C" void __cdecl Area145_DropFall(void) {
    rv::Current<records::SpriteObject>()->step_y = static_cast<std::int32_t>(static_cast<U>(rv::Current<records::SpriteObject>()->step_y) + static_cast<U>(rv::Current<records::SpriteObject>()->work_20));
    AH_CALL(Field_LeaderStepTick)();
    const long ground = AH_CALL(MapView_GroundAt)(rv::Current<records::SpriteObject>()->x, rv::Current<records::SpriteObject>()->z);
    unsigned char* const cur = Sprite_Current;
    if (static_cast<std::int16_t>(ground) <= static_cast<std::int16_t>(rv::As<records::SpriteObject>(cur)->height_whole)) {
        ScriptStep(-2);
        return;
    }
    rv::Store<unsigned>(rv::As<records::SpriteObject>(cur)->height_whole, static_cast<U>(ground));
    rv::Current<records::SpriteObject>()->step_y = 0;
    rv::Store<std::int32_t>(rv::Current<records::SpriteObject>()->work_20, 0);
    Sprite_Current[4] = 2;
    AH_CALL(Sprite_SetAnimation)(0x39);
    MoveScript_Object[1] = 4;
}

// original 0x421370 (area 145 +0x3C[4] = +0x34[5]; PSX 0x801F403C):
// MessageByMember with Area145_MemberKeys1 / Messages1.
extern "C" void __cdecl Area145_MessageByMember1(void) { MessageByMember(::bof3::Img(Area145_MemberKeys1_addr), ::bof3::Img(at::kArea145MemberMessages1.v)); }

// original 0x421400 (area 145 +0x3C[5] = +0x34[6]; PSX 0x801F4100): with
// Area145_MemberKeys5 / Messages5.
extern "C" void __cdecl Area145_MessageByMember5(void) { MessageByMember(::bof3::Img(Area145_MemberKeys5_addr), ::bof3::Img(at::kArea145MemberMessages5.v)); }

// original 0x421490 (area 145 +0x3C[6] = +0x34[7]; PSX 0x801F41C4): with
// Area145_MemberKeysB / MessagesB.
extern "C" void __cdecl Area145_MessageByMemberB(void) { MessageByMember(::bof3::Img(Area145_MemberKeysB_addr), ::bof3::Img(at::kArea145MemberMessagesB.v)); }

// original 0x421520 (area 145 +0x3C[7] = +0x34[8]; PSX 0x801F4288): the 16
// bytes of Item_NamePtr(4, 0xB) (key item 0xB's name) to Text_Records, dword
// by dword; KeyItem_Add(0xB); Msg_OpenSystem(2); Field_Request 2.
extern "C" void __cdecl Area145_GiveKeyItemB(void) {
    const unsigned char* const name = AH_CALL(Item_NamePtr)(4, 0xB);
    unsigned char* const text = Mem(::bof3::Img(at::kTextRecords.v));
    for (unsigned i = 0; i < 16; i += 4) SetLong(text + i, Long(name + i));
    AH_CALL(KeyItem_Add)(0xB);
    AH_CALL(Msg_OpenSystem)(2);
    Field_Request = 2;
}

// original 0x421570 (area 145 +0x3C[8] = +0x34[9]; PSX 0x801F4304): key item
// 0xB held, the script position + 3.
extern "C" void __cdecl Area145_SkipIfKeyItemB(void) {
    if (AH_CALL(KeyItem_Has)(0xB) != 0) ScriptStep(3);
}

// original 0x421590 (Field_ObjectTriggers[40]): ScriptFlags_Set40, tail kind
// 0x2C (engine) with state 0 and sub-kind 0xF. al 0.
extern "C" unsigned char __cdecl Area145_Trigger40(void) { return ArmTail(0x2C, 0xF); }

// original 0x4215B0 (Area_StepHook's case for area 0x91): Cond_ByteFD 0: z
// exactly 0x98000, x's high word 0x1B..0x1C and the leader's too (16-bit
// compares): ScriptFlags_Set40, tail kind 20 at state 0x19, al 1.
// Cond_ByteFD 4: story flag 0x2D set, x's high word 9..0xB and z's
// 0x62..0x64, the leader's pose 0, 7 or 6: counter 0 = 0, Party_DropIn(3),
// al 1. Else al 0.
extern "C" unsigned char __cdecl Area145_StepHook(long x, long z) {
    const unsigned char fd = Cond_ByteFD;
    if (fd == 0) {
        if (static_cast<U>(z) != 0x98000u) return 0;
        if (static_cast<std::uint16_t>(High(static_cast<U>(x)) - 0x1B) >= 2) return 0;
        if (static_cast<std::uint16_t>(Word(Mem(::bof3::Img(ObjTrio_addr) + 0x36)) - 0x1B) >= 2) return 0;
        AH_CALL(ScriptFlags_Set40)();
        B(::bof3::Img(AreaW0a_TailKind_addr)) = 0x14;
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0x19;
        return 1;
    }
    if (fd != 4) return 0;
    if (AH_CALL(Flags_Test)(StoryFlags(), 0x2D) == 0) return 0;
    if (static_cast<std::uint16_t>(High(static_cast<U>(x)) - 9) >= 3) return 0;
    if (static_cast<std::uint16_t>(High(static_cast<U>(z)) - 0x62) >= 3) return 0;
    if (!PoseStanding()) return 0;
    return DropIn(3);
}

// original 0x421660 (Area_CellHooks' record for area 0x91): the first of
// Area145_CellRecords whose x and z bytes are the cell's (low bytes) and
// whose low nibble of +2 is the leader's pose (read once, whole): with
// Cond_Flags row 13's flag 9, ScriptFlags_Set40, the tail's state = the
// record's +3 and kind 20, al 1. None, or the flag clear: al 0.
extern "C" unsigned char __cdecl Area145_CellHook(long x, long z) {
    const auto zb = static_cast<unsigned char>(z);
    const unsigned char pose = B(::bof3::Img(ObjTrio_addr) + 0x8);
    const auto xb = static_cast<unsigned char>(x);
    unsigned i = 0;
    for (; i < at::kArea145CellRecordCount; ++i) {
        const U rec = ::bof3::Img(Area145_CellRecords_addr) + i * 4;
        if (B(rec) == xb && B(rec + 1) == zb && (B(rec + 2) & 0xF) == pose) break;
    }
    if (i == at::kArea145CellRecordCount) return 0;
    if (AH_CALL(Flags_Test)(Mem(::bof3::Img(Cond_Flags_addr) + 0x68), 9) == 0) return 0;
    AH_CALL(ScriptFlags_Set40)();
    B(::bof3::Img(AreaW0c_TailState_addr)) = B(::bof3::Img(Area145_CellRecords_addr) + i * 4 + 3);
    B(::bof3::Img(AreaW0a_TailKind_addr)) = 0x14;
    return 1;
}

// original 0x4216D0 (Field_ModeTailKinds[20]; a jump table of 18 through a
// byte table of 35, by the s8 state - 2): the tail its cell hook arms at
// states 2, 4, 6 and 0xA and its step hook at 0x19. 2, 4, 6: counter 3 =
// the state, state 8, which waits for counter 3 to be 0 and disarms. 0xA:
// story flag 0x2C, sound 0x200, Kind2_Place(0) and an effect of kind 0x13
// (none: stays), then 0xB waits for counter 3 = 0x40 (timer 0, sound 0x204),
// 0xC runs the trails a frame at a time (Area145_TrailStart by the timer + 1)
// until they answer 0 (flag 0x2D clear: sounds 0x205, 0x207 and state 0x1E;
// set: as 2) or 2 (sound 0x205, state 0x14); 0x14 another kind 0x13 effect
// with the kind-2 target at the leader; 0x15 waits for Field_Kind2Hold, then
// 0x16 clears flag 0x2C and disarms. 0x19..0x1A: the leader's +1..+3, a
// 0x17-frame wait, the area 0x91 change and flag 0x2E (sound 0x20C the first
// time). 0x1E..0x24: transitions 8 and 9, an effect of kind 0x3E, waits,
// flag 0x2D, two area 0x91 changes, back to 0x16. Every other state: nothing.
extern "C" void __cdecl Area145_Tail20(void) {
    const auto state = static_cast<signed char>(B(::bof3::Img(AreaW0c_TailState_addr)));
    switch (state) {
    case 2: case 4: case 6:
        B(::bof3::Img(at::kCounter3.v)) = static_cast<unsigned char>(state);
        B(::bof3::Img(AreaW0c_TailState_addr)) = 8;
        return;
    case 8:
        if (B(::bof3::Img(at::kCounter3.v)) != 0) return;
        AH_CALL(ScriptFlags_Clear40)();
        Disarm();
        return;
    case 0xA: {
        AH_CALL(Flags_Set)(StoryFlags(), 0x2C);
        AH_CALL(Sound_PlayEffect)(0x200);
        AH_CALL(Kind2_Place)(0);
        if (SpawnSlot() == 0xFF) return;
        unsigned char* const e = ScratchRecord();
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0xB;
        Kind13(e, 0xFFFFFCE6u, CameraAngle1(), 0x260);
        return;
    }
    case 0xB:
        if (B(::bof3::Img(at::kCounter3.v)) != 0x40) return;
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0xC;
        SetWord(Mem(::bof3::Img(at::kTailTimer.v)), 0);
        AH_CALL(Sound_PlayEffect)(0x204);
        return;
    case 0xC: {
        const auto t = static_cast<std::uint16_t>(Word(Mem(::bof3::Img(at::kTailTimer.v))) + 1);
        SetWord(Mem(::bof3::Img(at::kTailTimer.v)), t);
        const unsigned char r = AH_CALL(Area145_TrailStart)(t);
        if (r == 1) return;
        if (r == 0 && AH_CALL(Flags_Test)(StoryFlags(), 0x2D) == 0) {
            AH_CALL(Sound_PlayEffect)(0x205);
            AH_CALL(Sound_PlayEffect)(0x207);
            B(::bof3::Img(AreaW0c_TailState_addr)) = 0x1E;
            return;
        }
        AH_CALL(Sound_PlayEffect)(0x205);
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0x14;
        return;
    }
    case 0x14: {
        if (SpawnSlot() == 0xFF) return;
        unsigned char* const e = ScratchRecord();
        const std::int32_t leader_z = Long(Mem(::bof3::Img(ObjTrio_addr) + 0x38));
        const std::int32_t angle = CameraAngle1();
        Field_Kind2Z = leader_z;
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0x15;
        Kind13(e, 0xFFFFFD56u, angle, 0x200);
        Field_Kind2X = Long(Mem(::bof3::Img(ObjTrio_addr) + 0x34));
        return;
    }
    case 0x15:
        if (Field_Kind2Hold != 0) return;
        B(::bof3::Img(at::kCounter3.v)) = 0;
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0x16;
        return;
    case 0x16:
        AH_CALL(Flags_Clear)(StoryFlags(), 0x2C);
        AH_CALL(ScriptFlags_Clear40)();
        Disarm();
        return;
    case 0x19:
        B(::bof3::Img(ObjTrio_addr) + 0x1) = 2;
        B(::bof3::Img(ObjTrio_addr) + 0x1 + 1) = 3;
        B(::bof3::Img(ObjTrio_addr) + 0x1 + 2) = 0;
        SetWord(Mem(::bof3::Img(at::kTailTimer.v)), 0x17);
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0x1A;
        return;
    case 0x1A:
        if (!TimerOut()) return;
        AH_CALL(ScriptFlags_Clear40)();
        AH_CALL(Field_ChangeArea)(0x91, 0x3B8000, 0x5D0000, 0x82);
        if (AH_CALL(Flags_Test)(StoryFlags(), 0x2E) == 0) {
            AH_CALL(Sound_PlayEffect)(0x20C);
            AH_CALL(Flags_Set)(StoryFlags(), 0x2E);
        }
        Disarm();
        return;
    case 0x1E:
        AH_CALL(Transition_Start)(8);
        Field_Kind2X = 0x308000;
        Field_Kind2Z = 0x638000;
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0x1F;
        return;
    case 0x1F:
        if (Transition_Busy != 0) return;
        AH_CALL(Transition_Start)(9);
        if (SpawnSlot() != 0xFF) {
            unsigned char* const e = ScratchRecord();
            e[0] = 1;
            e[5] = 0x3E;
        }
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0x20;
        return;
    case 0x20:
        if (Field_Kind2Hold != 0) return;
        SetWord(Mem(::bof3::Img(at::kTailTimer.v)), 0x1E);
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0x21;
        return;
    case 0x21:
        if (!TimerOut()) return;
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0x22;
        return;
    case 0x22:
        AH_CALL(Flags_Set)(StoryFlags(), 0x2D);
        AH_CALL(Field_ChangeArea)(0x91, 0xA0000, 0x630000, 0x80);
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0x23;
        return;
    case 0x23:
        if (Transition_Busy != 0) return;
        SetWord(Mem(::bof3::Img(at::kTailTimer.v)), 0x5A);
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0x24;
        return;
    case 0x24:
        if (!TimerOut()) return;
        AH_CALL(Field_ChangeArea)(0x91, 0x3E0000, 0x5F0000, 1);
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0x16;
        return;
    default:
        return;
    }
}

// original 0x421B20 (called by Area145_Tail20's state 0xC with the timer):
// t (16-bit) below 5: the start trail (0x428000, 0x638000) to (0x428000 -
// t << 16, 0x638000) and al 1. From 5: that trail at length 5, at 5 sound
// 0x206, then Area145_TrailLeg0(t); al its answer up to t 0x37, 2 past it.
extern "C" unsigned char __cdecl Area145_TrailStart(unsigned t) {
    const auto s = static_cast<std::uint16_t>(t);
    const U n = s < 5 ? s : 5;
    AH_CALL(Area145_SpawnTrail)(0x428000, 0x638000, static_cast<long>(0x428000u - (n << 16)), 0x638000);
    if (s < 5) return 1;
    if (s == 5) AH_CALL(Sound_PlayEffect)(0x206);
    const unsigned char r = AH_CALL(Area145_TrailLeg0)(t);
    return s <= 0x37 ? r : 2;
}

// original 0x421B90: the first leg, Area145_TrailLegs0's record by
// Sprite_ObjectsExtra record 0's quarter: d = t - 5 (16-bit), n = min(d, its
// length); the trail from (0x3D8000, 0x638000) to record 0's (x, z) + n
// cells along its direction. d below the length, or no next leg: al 1. Else
// (sound 0x206 at d equal to the length) Area145_TrailLeg1(t - 5, next).
extern "C" unsigned char __cdecl Area145_TrailLeg0(unsigned t) {
    const U rec = ::bof3::Img(Area145_TrailLegs0_addr) + Quarter(0) * 4;
    const U d32 = t + 0xFFFBu;
    const auto d = static_cast<std::uint16_t>(d32);
    const unsigned char dir = B(rec);
    const U n = Upto(d, B(rec + 1));
    const U x = AlongX(dir, n, static_cast<U>(Long(Mem(::bof3::Img(Sprite_ObjectsExtra_addr) + 0x34))));
    const U z = AlongZ(dir, n, static_cast<U>(Long(Mem(::bof3::Img(Sprite_ObjectsExtra_addr) + 0x38))));
    AH_CALL(Area145_SpawnTrail)(0x3D8000, 0x638000, static_cast<long>(x), static_cast<long>(z));
    const unsigned char len = B(rec + 1);
    if (d < len) return 1;
    const unsigned char next = B(rec + 2);
    if (next == 0) return 1;
    if (d == len) AH_CALL(Sound_PlayEffect)(0x206);
    return AH_CALL(Area145_TrailLeg1)(d32, next);
}

// original 0x421C90: the second leg of record k (its low byte): the table
// Area145_TrailLegs1A for record 1, 1B for any other, by record k's quarter;
// d = t - 5; the trail from record k's (x, z) to it + min(d, length) cells;
// d below the length or no next leg: al 1; else (sound 0x206 at d equal to
// the length) Area145_TrailLeg2(t - 5, next).
extern "C" unsigned char __cdecl Area145_TrailLeg1(unsigned t, unsigned k) {
    return TrailLeg(t, k, 5, ::bof3::Img(Area145_TrailLegs1A_addr), ::bof3::Img(Area145_TrailLegs1B_addr), false);
}

// original 0x421DB0: the third leg: tables Area145_TrailLegs2A / 2B, d = t -
// 10; the trail as the second's; d below the length: al 1, else al the
// record's +2 (0 or 2).
extern "C" unsigned char __cdecl Area145_TrailLeg2(unsigned t, unsigned k) {
    return TrailLeg(t, k, 10, ::bof3::Img(Area145_TrailLegs2A_addr), ::bof3::Img(Area145_TrailLegs2B_addr), true);
}

// original 0x421EA0: Effect_FindFree to SharedScratch; none: al 1. A slot:
// kind 0x3F from (x0, z0) (+0xC, +0x10) to (x1, z1) (+0x18, +0x1C), +0x20 and
// +0x14 0x500000; al 0.
extern "C" unsigned char __cdecl Area145_SpawnTrail(long x0, long z0, long x1, long z1) {
    if (SpawnSlot() == 0xFF) return 1;
    unsigned char* const e = ScratchRecord();
    auto* const tE = rv::As<records::EffectObject>(e);
    e[0] = 1;
    e[5] = 0x3F;
    tE->step_x = static_cast<std::int32_t>(x0);
    tE->step_z = static_cast<std::int32_t>(z0);
    rv::Store<std::int32_t>(rv::As<records::EffectObject>(e)->work_18, static_cast<std::int32_t>(x1));
    rv::Store<std::int32_t>(rv::As<records::EffectObject>(e)->work_1c, static_cast<std::int32_t>(z1));
    rv::Store<std::int32_t>(rv::As<records::EffectObject>(e)->work_20, 0x500000);
    rv::Store<std::int32_t>(rv::As<records::EffectObject>(e)->step_y, 0x500000);
    return 0;
}

// original 0x421F10 (EffectKind18_States[96]): while Cond_ByteFE: a
// draw-mode packet (dtd 1, tpage 0x95) committed to slot 7 (0xC), then a
// full-screen POLY_G4 (0, 0)..(320, 240) as floats, not semi-transparent,
// colour (0, 0xC8, 0xFF) at the top and (0, 0, 0x20) at the bottom,
// committed to slot 7 (0x44).
extern "C" void __cdecl Area145_DrawGradient(void) {
    if (Cond_ByteFE == 0) return;
    AH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x95, 0);
    AH_CALL(Gfx_CommitPrim)(7, 0xC);
    unsigned char* const prim = Gfx_PacketNext;
    AH_CALL(Gpu_SetPolyG4)(prim);
    AH_CALL(Gpu_SetSemiTrans)(prim, 0);
    constexpr std::int32_t k240 = 0x43700000, k320 = 0x43A00000;   // 240.0f, 320.0f
    SetLong(prim + 0x2C, k240);
    SetLong(prim + 0x3C, k240);
    prim[0x15] = prim[5] = 0xC8;
    prim[0x16] = prim[6] = 0xFF;
    // DIV-0041: the left corners at -53 and the right at 373 under the wide
    // picture (widescreen.h: 0 until every self-test has run).
    const auto left = std::bit_cast<std::int32_t>(Widescreen_FillX());
    const auto right = std::bit_cast<std::int32_t>(320.0f + static_cast<float>(Widescreen_Fill()));
    SetLong(prim + 8, left);
    SetLong(prim + 0xC, 0);
    SetLong(prim + 0x18, right);
    SetLong(prim + 0x1C, 0);
    SetLong(prim + 0x28, left);
    SetLong(prim + 0x38, right);
    prim[0x14] = prim[4] = 0;
    prim[0x34] = prim[0x24] = 0;
    prim[0x35] = prim[0x25] = 0;
    prim[0x36] = prim[0x26] = 0x20;
    AH_CALL(Gfx_CommitPrim)(7, 0x44);
}

// original 0x421FB0 (area 145 +0x3C[3] = +0x34[4]; also areas 36, 59, 100,
// 112, 116, 143 and 146's handler 1; PSX 0x801F4008): the running object's
// +0x48 = 0, Sprite_ReleaseTint(it).
extern "C" void __cdecl Area145_ClearTint(void) {
    Sprite_Current[0x48] = 0;
    AH_CALL(Sprite_ReleaseTint)(Sprite_Current);
}

void Area145_Inject() {
    BOF3_INJECT(Area145_ChoiceRun3);
    BOF3_INJECT(Area145_PartyRecord16);
    BOF3_INJECT(Area145_RunDrop);
    BOF3_INJECT(Area145_DropStart);
    BOF3_INJECT(Area145_DropFall);
    BOF3_INJECT(Area145_MessageByMember1);
    BOF3_INJECT(Area145_MessageByMember5);
    BOF3_INJECT(Area145_MessageByMemberB);
    BOF3_INJECT(Area145_GiveKeyItemB);
    BOF3_INJECT(Area145_SkipIfKeyItemB);
    BOF3_INJECT(Area145_Trigger40);
    BOF3_INJECT(Area145_StepHook);
    BOF3_INJECT(Area145_CellHook);
    BOF3_INJECT(Area145_Tail20);
    BOF3_INJECT(Area145_TrailStart);
    BOF3_INJECT(Area145_TrailLeg0);
    BOF3_INJECT(Area145_TrailLeg1);
    BOF3_INJECT(Area145_TrailLeg2);
    BOF3_INJECT(Area145_SpawnTrail);
    BOF3_INJECT(Area145_DrawGradient);
    BOF3_INJECT(Area145_ClearTint);
}

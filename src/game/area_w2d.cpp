// World 2's areas 95..100 and 103: the PSX's BIN/WORLD02/AREA095..100 and
// AREA103.EMI compiled into the exe at 0x4135B0..0x4146C0 (Area_Descriptors
// entries 95..103; 101 and 102 have no code here). Round ten, group AR2D: the
// band's 53 functions, none ours before, each read to its last instruction
// with capstone (2026-09-28) and taken through the area harness
// (area_harness.h). docs/area_w2d.md.
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. Reads
// by an unchecked index into the areas' .data tables are kept where they stay
// in .data; the dispatchers through area 99's and area 100's state tables
// abort past the table (what follows is data, which the original would jump
// into), and the two divides by a byte that can be 0 abort where the original
// faults (docs/area_w2d.md section 6). Every call goes through the harness
// (AH_CALL / AH_AT), so the start-up fuzz can stand recorders in for ours as
// for the originals' copies; the callees of the group's own are called the
// same way, so each function is fuzzed alone.
#include "game/area_w2d.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w2d_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w2d::at;
using area_harness::Handler;
using area_harness::Mem;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using U = std::uint32_t;

unsigned char& B(U address) { return *Mem(address); }
void AddWord(unsigned char* p, int v) { SetWord(p, static_cast<unsigned>(Word(p) + v)); }
void AddLong(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(static_cast<U>(Long(p)) + v)); }
unsigned char* Ptr(U cell) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(cell))))); }
// Effect_Objects record `slot` (0x80 bytes; the slot is not checked, as the
// originals' `shl 7`).
unsigned char* EffectAt(unsigned slot) { return Effect_Objects + slot * at::kEffectStride; }
// The high word of a 16.16 position, as the hooks read it (a word at +2).
std::uint16_t High(long v) { return static_cast<std::uint16_t>(static_cast<U>(v) >> 16); }

void SetMessage(unsigned id) { SetWord(Mem(at::kMessage), id); }
unsigned char* StoryFlags() { return Mem(at::kStoryFlags); }
// The chapter's flag row (a pointer the chapters keep at 0x929ED0).
unsigned char* FlagRow() { return Ptr(at::kFlagRow); }

// A state handler read from an area's .data table in place, as the
// originals' `jmp [index * 4 + table]`: the index is not checked there. Ours
// aborts past the table (what follows is data).
Handler StateEntry(const char* who, U table, unsigned count, unsigned index) {
    if (index >= count)
        bof3::Fatal("%s: state %u is past its %u-entry table 0x%X", who, index, count, static_cast<unsigned>(table));
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(table + index * 4u)))));
}

// The callee nobody owns (area_w2d_callees.h): story-flag toggle.
void FlagsToggle(unsigned char* bits, unsigned index) {
    AH_AT(void (__cdecl*)(unsigned char*, unsigned), at::kFlagsToggle)(bits, index);
}
// The map byte setter, as the originals pass it: x and z words, a value byte.
void SetByte(unsigned x, unsigned z, unsigned value) { AH_CALL(AreaMap_SetByte)(x, z, value); }

// The movement script's position (MoveScript_Object's word +0xA) moved by `v`.
void ScriptStep(int v) { AddWord(MoveScript_Object + 0xA, v); }
// Field_ActiveMember's word +0x8A less 2.
void MemberStep() { AddWord(Field_ActiveMember + 0x8A, -2); }
// Field_State's word +0x12E less 2.
void LeaderStep() { AddWord(Field_State + 0x12E, -2); }

// The tail armed: ScriptFlags_Set40, kind `kind`, state 0.
void ArmTail(unsigned char kind) {
    AH_CALL(ScriptFlags_Set40)();
    B(at::kTailKind) = kind;
    B(at::kTailState) = 0;
}
// The tail disarmed: ScriptFlags_Clear40, kind and state 0.
void Disarm() {
    AH_CALL(ScriptFlags_Clear40)();
    B(at::kTailKind) = 0;
    B(at::kTailState) = 0;
}

// Areas 95 and 96's handler 4 (0x4135B0, 0x413860, byte for byte the same):
// the running object at z word 0x3A and x word 0x40 turns to 3, at x 0x42 to
// 7, and runs Area95_TurnAndMove (the originals' tail jmp); anywhere else
// the script position + 2.
void TurnAtCell() {
    unsigned char* const cur = Sprite_Current;
    if (Word(cur + 0x3A) == 0x3A) {
        const auto d = static_cast<std::uint16_t>(Word(cur + 0x36) - 0x40);
        if (d == 0 || d == 2) {
            cur[8] = d == 0 ? 3 : 7;
            AH_CALL(Area95_TurnAndMove)();
            return;
        }
    }
    ScriptStep(2);
}

// Area 100's handlers 6 and 7 (0x4140A0, 0x4140F0) and its tail's state 1:
// an effect of `kind` at (0x460000, 0x340000), if a slot is free.
void SpawnAt46(unsigned char kind) {
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    if (slot == 0xFF) return;
    unsigned char* const e = EffectAt(slot);
    e[0] = 1;
    e[5] = kind;
    SetLong(e + 0x34, 0x460000);
    SetLong(e + 0x38, 0x340000);
}
// Area 100's handlers 6 and 7: Cond_ByteFE 1, MoveCmd_TestFB(0x4A, 0x2F),
// then the effect.
void TestAndSpawn(unsigned char kind) {
    Cond_ByteFE = 1;
    AH_CALL(MoveCmd_TestFB)(0x4A, 0x2F);
    SpawnAt46(kind);
}
// Area 100's four gate cells (0x49, 0x1E..0x1F) and (0x4D, 0x33..0x34) set
// to `value` (its init: 0x50, its trigger 16: 0xA1).
void GateCells(unsigned value) {
    SetByte(0x49, 0x1E, value);
    SetByte(0x49, 0x1F, value);
    SetByte(0x4D, 0x33, value);
    SetByte(0x4D, 0x34, value);
}

}  // namespace

// ===========================================================================
// Areas 95 and 96 (descriptors 0x617588, 0x617D48): eight handlers each, the
// same bodies but handler 4 (two identical copies, one in each block).
// ===========================================================================

// original 0x4135B0 (area 95 +0x3C[4]; PSX 0x801F351C): TurnAtCell.
extern "C" void __cdecl Area95_TurnAtCell(void) { TurnAtCell(); }

// original 0x4135F0 (areas 95 and 96 +0x3C[0]; PSX 0x801F30E8 / 0x801F3078):
// the first of the twenty effect records that is live (+0 bit 0) and stands
// at x word 0x41, z word 0x3A: while there is one, the script position - 2.
// None: with the leader's byte +0x137 set, the script position - 2 and
// ScriptFlags_Set40; clear, Party_DropIn(0).
extern "C" void __cdecl Area95_WaitEffectAtCell(void) {
    unsigned i = 0;
    for (; i < at::kEffectCount; ++i) {
        const unsigned char* const e = EffectAt(i);
        if ((e[0] & 1) != 0 && Word(e + 0x36) == 0x41 && Word(e + 0x3A) == 0x3A) break;
    }
    if (i != at::kEffectCount) {
        ScriptStep(-2);
        return;
    }
    if (B(at::kLeader137) != 0) {
        ScriptStep(-2);
        AH_CALL(ScriptFlags_Set40)();
        return;
    }
    AH_CALL(Party_DropIn)(0);
}

// original 0x413670 (areas 95 and 96 +0x3C[1]; PSX 0x801F31F8 / 0x801F3188):
// Effect_FindFree; none: the active member's word +0x8A - 2. A slot: kind
// 0x37 at (0x410000, 0x3C8000, 0x2000000), +0x2C 0x58, +9 1, +6 0, +7 0x12,
// +0x29 6; Sound_PlayEffect(0x202); then story flag 0x20 in area 0x60 (96),
// else 0x21 below chapter 7 (Cond_ByteFA signed), else 0x22.
extern "C" void __cdecl Area95_SpawnEffect37(void) {
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    if (slot == 0xFF) {
        MemberStep();
        return;
    }
    unsigned char* const e = EffectAt(slot);
    e[0] = 1;
    e[5] = 0x37;
    SetLong(e + 0x34, 0x410000);
    SetLong(e + 0x38, 0x3C8000);
    SetLong(e + 0x3C, 0x2000000);
    SetWord(e + 0x2C, 0x58);
    e[9] = 1;
    e[6] = 0;
    e[7] = 0x12;
    e[0x29] = 6;
    AH_CALL(Sound_PlayEffect)(0x202);
    if (Game_AreaNumber == 0x60) {
        AH_CALL(Flags_Set)(StoryFlags(), 0x20);
        return;
    }
    AH_CALL(Flags_Set)(StoryFlags(), Cond_ByteFA < 7 ? 0x21u : 0x22u);
}

// original 0x413750 (areas 95 and 96 +0x3C[2]; PSX 0x801F336C / 0x801F32FC):
// the script object's +7 = 1 and MoveCmd_Move(it, 5); then, n the running
// object's +9, the ground at (x + +0xC * n, z + +0x10 * n) (dwords, wrapping)
// and +0x14 = (ground - the s16 +0x3E + 0xE0) / n (idiv: n 0 faults, ours
// aborts); MoveScript_PartyRecords record 0's word +4 = 0x40 and its count +1
// = n. Sprite_Current read again after each call.
extern "C" void __cdecl Area95_LeapArc(void) {
    MoveScript_Object[7] = 1;
    AH_CALL(MoveCmd_Move)(MoveScript_Object, 5);
    const unsigned char* cur = Sprite_Current;
    const U n = cur[9];
    const U x = static_cast<U>(Long(cur + 0xC)) * n + static_cast<U>(Long(cur + 0x34));
    const U z = static_cast<U>(Long(cur + 0x10)) * n + static_cast<U>(Long(cur + 0x38));
    const auto ground = static_cast<std::int16_t>(AH_CALL(AreaMap_Elevation)(static_cast<long>(x), static_cast<long>(z)));
    unsigned char* const obj = Sprite_Current;
    const std::int32_t rise = static_cast<std::int32_t>(ground) - static_cast<std::int16_t>(Word(obj + 0x3E)) + 0xE0;
    const unsigned frames = obj[9];
    if (frames == 0) bof3::Fatal("Area95_LeapArc: +9 is 0, the original's idiv faults");
    SetLong(obj + 0x14, rise / static_cast<std::int32_t>(frames));
    SetLong(Mem(at::kPartyRecord0 + 4), 0x40);
    B(at::kPartyRecord0 + 1) = Sprite_Current[9];
}

// original 0x4137D0 (areas 95 and 96 +0x3C[3]; PSX 0x801F3450 / 0x801F33E0;
// also the tail of both handler 4 copies): the script object's +4 = 1 and
// bit 0x40; the running object's +7 bit 8; Sprite_SetAnimation(8 + its
// direction & 7); the script object's +7 = 2; then MoveCmd_Move(the script
// object, 3) and direction 3 when it faced 7, else 7 and 7.
extern "C" void __cdecl Area95_TurnAndMove(void) {
    MoveScript_Object[4] = 1;
    const auto dir = static_cast<unsigned char>(Sprite_Current[8] & 7);
    MoveScript_Object[0] = static_cast<unsigned char>(MoveScript_Object[0] | 0x40);
    Sprite_Current[7] = static_cast<unsigned char>(Sprite_Current[7] | 8);
    AH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>(dir + 8));
    MoveScript_Object[7] = 2;
    const unsigned char to = dir == 7 ? 3 : 7;
    AH_CALL(MoveCmd_Move)(MoveScript_Object, to);
    Sprite_Current[8] = to;
}

// original 0x413860 (area 96 +0x3C[4]; PSX 0x801F34AC): TurnAtCell, area
// 96's copy.
extern "C" void __cdecl Area96_TurnAtCell(void) { TurnAtCell(); }

// original 0x4138A0 (areas 95 and 96 +0x3C[5]; PSX 0x801F35A4 / 0x801F3534):
// the map cell (0x41, 0x39) set to 0x10 when it is 0, else to 0.
extern "C" void __cdecl Area96_ToggleCell(void) {
    if (AH_CALL(AreaMap_ByteAt)(0x41, 0x39) == 0) {
        SetByte(0x41, 0x39, 0x10);
        return;
    }
    SetByte(0x41, 0x39, 0);
}

// original 0x4138D0 (areas 95 and 96 +0x3C[6]; PSX 0x801F35F0 / 0x801F3580):
// the running object's direction 3 when it is 7, else 7.
extern "C" void __cdecl Area96_FaceBack(void) {
    unsigned char* const cur = Sprite_Current;
    cur[8] = cur[8] == 7 ? 3 : 7;
}

// original 0x4138F0 (areas 95 and 96 +0x3C[7]; PSX 0x801F3618 / 0x801F35A8):
// the ground at the leader's (x, z), sign-extended; MapView_Elevation already
// there: 0x20 lower, else set to it; MapView_Redraw + 2 either way.
extern "C" void __cdecl Area96_SnapElevation(void) {
    const long ground = static_cast<std::int16_t>(AH_CALL(AreaMap_Elevation)(Long(Mem(at::kLeaderX)), Long(Mem(at::kLeaderZ))));
    const long now = MapView_Elevation;
    MapView_Elevation = now == ground ? now - 0x20 : ground;
    MapView_Redraw = static_cast<unsigned char>(MapView_Redraw + 2);
}

// ===========================================================================
// Area 97 (descriptor 0x617E58: no tables): a step hook arming tail kind 13,
// that tail, and the world-map field hook for "no world map".
// ===========================================================================

// original 0x413940 (Field_ModeTailKinds[13]): by the s8 state, 0 -
// ScriptFlags_Set40 and state 1; 1 - once counter 3 is 2, ScriptFlags_Clear40,
// counter 3 0 and the tail disarmed.
extern "C" void __cdecl Area97_TailWaitCounter3(void) {
    const auto state = static_cast<signed char>(B(at::kTailState));
    if (state == 0) {
        AH_CALL(ScriptFlags_Set40)();
        B(at::kTailState) = 1;
        return;
    }
    if (state != 1 || B(at::kCounter3) != 2) return;
    AH_CALL(ScriptFlags_Clear40)();
    B(at::kCounter3) = 0;
    B(at::kTailKind) = 0;
    B(at::kTailState) = 0;
}

// original 0x413980 (Area_StepHook's case for area 97): z at most 0x3A0000
// (signed), x's high word 0x30..0x32 (a 16-bit compare), the leader's pose 0,
// 1 or 2, and key item 5 not held: tail kind 13 armed (state 0, counter 3 =
// 1), al 1. Else al 0.
extern "C" unsigned char __cdecl Area97_StepHook(long x, long z) {
    const unsigned char pose = B(at::kLeaderPose);
    if (z > 0x3A0000) return 0;
    if (static_cast<std::uint16_t>(High(x) - 0x30) >= 3) return 0;
    if (pose != 0 && pose != 1 && pose != 2) return 0;
    if (AH_CALL(KeyItem_Has)(5) != 0) return 0;
    B(at::kTailState) = 0;
    B(at::kCounter3) = 1;
    B(at::kTailKind) = 0xD;
    return 1;
}

// original 0x4139E0 (WorldMap_FieldHooks[11], the "no world map" entry that
// tail kind 27 0x56DE30 jumps to; in area 97's block): key item 5 held, story
// flag 0x23 set. al 0.
extern "C" unsigned char __cdecl Area97_FlagIfKeyItem5(void) {
    if (AH_CALL(KeyItem_Has)(5) != 0) AH_CALL(Flags_Set)(StoryFlags(), 0x23);
    return 0;
}

// ===========================================================================
// Area 98 (descriptor 0x6183B0): six choices (four its own), seven handlers
// (handlers 1..6 are choices 0..5), the init, tail kind 45, object trigger 44,
// and two camera handlers areas 39, 41 and 99 name.
// ===========================================================================

// original 0x413A00 (area 98 +0x34[0] = +0x3C[1]; PSX 0x801F59F4): message
// 0xFFFF, flag 9 of 0x904654 set; then the answer (read after the call) 0:
// counter 1 = 0; else the chapter row's flag 0x1A and counter 1 = 1.
extern "C" void __cdecl Area98_ChoiceFlag9(void) {
    SetMessage(0xFFFF);
    AH_CALL(Flags_Set)(Mem(at::kFlags904654), 9);
    if (B(at::kChoiceAnswer) == 0) {
        B(at::kCounter1) = 0;
        return;
    }
    AH_CALL(Flags_Set)(FlagRow(), 0x1A);
    B(at::kCounter1) = 1;
}

// original 0x413A50 (area 98 +0x34[3], [4] = +0x3C[4], [5]; PSX 0x801F5AC8,
// 0x801F5B0C): an answer: message 0x2F and the mark 0x9398CF 6; 0: message
// 0xFFFF.
extern "C" void __cdecl Area98_ChoiceAsk2F(void) {
    if (B(at::kChoiceAnswer) != 0) {
        SetMessage(0x2F);
        B(at::kAnswerMark) = 6;
        return;
    }
    SetMessage(0xFFFF);
}

// original 0x413A80 (area 98 +0x34[5] = +0x3C[6]; PSX 0x801F5B50): message
// 0xFFFF; answer 0: tail kind 45 armed.
extern "C" void __cdecl Area98_ChoiceArmTail45(void) {
    const unsigned char answer = B(at::kChoiceAnswer);
    SetMessage(0xFFFF);
    if (answer == 0) ArmTail(0x2D);
}

// original 0x413AB0 (area 98 +0x3C[0]; PSX 0x801F5BA0): Effect_FindFree to
// the running object's +0xB; a slot (read back from +0xB for every store):
// kind 0x37 at the object's +0x34 / +0x38 / +0x3C, +0x2C 0x11D, +6 0, +7 0,
// +0x29 2, +9 1; Sound_PlayEffect(0x206).
extern "C" void __cdecl Area98_SpawnEffect37(void) {
    Sprite_Current[0xB] = AH_CALL(Effect_FindFree)();
    const unsigned char* const cur = Sprite_Current;
    if (cur[0xB] == 0xFF) return;
    EffectAt(cur[0xB])[0] = 1;
    EffectAt(cur[0xB])[5] = 0x37;
    SetLong(EffectAt(cur[0xB]) + 0x34, Long(cur + 0x34));
    SetLong(EffectAt(cur[0xB]) + 0x38, Long(cur + 0x38));
    SetLong(EffectAt(cur[0xB]) + 0x3C, Long(cur + 0x3C));
    SetWord(EffectAt(cur[0xB]) + 0x2C, 0x11D);
    EffectAt(cur[0xB])[6] = 0;
    EffectAt(cur[0xB])[7] = 0;
    EffectAt(cur[0xB])[0x29] = 2;
    EffectAt(cur[0xB])[9] = 1;
    AH_CALL(Sound_PlayEffect)(0x206);
}

// original 0x413B90 (area 98 +0x40; PSX 0x801F5DD0): cells (4..7, 5) set to
// 0x50.
extern "C" void __cdecl Area98_InitCells(void) {
    for (unsigned i = 0; i < 4; ++i) SetByte(4 + i, 5, 0x50);
}

// original 0x413BC0 (Field_ModeTailKinds[45]): state 0 and Field_Request not
// 2: the return point = the leader's x, z and the area number;
// Field_ChangeArea(0x62, 0x380000, 0x2C0000, 1); the tail disarmed.
extern "C" void __cdecl Area98_TailChangeArea62(void) {
    if (B(at::kTailState) != 0 || Field_Request == 2) return;
    const std::int32_t x = Long(Mem(at::kLeaderX));
    const std::int32_t z = Long(Mem(at::kLeaderZ));
    const unsigned short area = Game_AreaNumber;
    SetLong(Mem(at::kReturnPoint), x);
    SetLong(Mem(at::kReturnPoint + 4), z);
    SetWord(Mem(at::kReturnPoint + 8), area);
    AH_CALL(Field_ChangeArea)(0x62, 0x380000, 0x2C0000, 1);
    Disarm();
}

// original 0x413C20 (Field_ObjectTriggers id 44; a gap of the tool, area 98's
// block): ScriptFlags_Set40; the focus object 0x903804 (read again for each)
// +1 = 4, +0x83 = 0, word +0x8A = 0; tail kind 4 (engine, 0x56D930) with
// sub-kind 9 and counter 3 the focus object's index in Sprite_Objects (a
// signed division by 0xA4). al 0.
extern "C" unsigned char __cdecl Area98_Trigger44(unsigned char*, unsigned char*) {
    AH_CALL(ScriptFlags_Set40)();
    Ptr(at::kFocusObject)[1] = 4;
    Ptr(at::kFocusObject)[0x83] = 0;
    SetWord(Ptr(at::kFocusObject) + 0x8A, 0);
    const auto offset = static_cast<std::int32_t>(static_cast<U>(Long(Mem(at::kFocusObject))) - at::kSpriteObjects);
    B(at::kTailKind) = 4;
    B(at::kTailSub) = 9;
    B(at::kCounter3) = static_cast<unsigned char>(offset / 0xA4);
    return 0;
}

// original 0x413C80 (areas 39 +0x3C[1], 41 +0x34[10] = +0x3C[0], 99
// +0x3C[0]; PSX 0x801F2C04 in area 99): Camera_Distance 0xFF00,
// MapView_Redraw 2.
extern "C" void __cdecl Area98_CameraDistanceFF00(void) {
    Camera_Distance = static_cast<short>(0xFF00);
    MapView_Redraw = 2;
}

// original 0x413CA0 (areas 39 +0x3C[2], 41 +0x34[11] = +0x3C[1], 99
// +0x3C[1]; PSX 0x801F2C24 in area 99): Camera_Distance 0, MapView_Redraw 2.
extern "C" void __cdecl Area98_CameraDistance0(void) {
    Camera_Distance = 0;
    MapView_Redraw = 2;
}

// ===========================================================================
// Area 99 (descriptor 0x619BF8): six handlers (0 and 1 area 98's block's),
// two two-state machines in its .data, the init.
// ===========================================================================

// original 0x413CC0 (area 99 +0x3C[2]; PSX 0x801F2C40): the running object's
// direction, if even, turned one step back (& 7); Sound_PlayEffect(its word
// +0x2C + 0x100); Sprite_EnsureAnimation(0x42 + (direction - 1) / 2, a
// signed halving); Field_State +0x124 bit 0x40; Effect_FindFree to its +0xB
// and, a slot (read back from +0xB), kind 0x1B. Sprite_Current read again
// after each call.
extern "C" void __cdecl Area99_TurnSoundEffect1B(void) {
    unsigned char* cur = Sprite_Current;
    if ((cur[8] & 1) == 0) {
        cur[8] = static_cast<unsigned char>((cur[8] - 1) & 7);
        cur = Sprite_Current;
    }
    AH_CALL(Sound_PlayEffect)(static_cast<unsigned short>(Word(cur + 0x2C) + 0x100));
    const int half = (static_cast<int>(Sprite_Current[8]) - 1) / 2;
    AH_CALL(Sprite_EnsureAnimation)(static_cast<unsigned char>(half + 0x42));
    Field_State[0x124] = static_cast<unsigned char>(Field_State[0x124] | 0x40);
    Sprite_Current[0xB] = AH_CALL(Effect_FindFree)();
    const unsigned char* const obj = Sprite_Current;
    if (obj[0xB] == 0xFF) return;
    EffectAt(obj[0xB])[0] = 1;
    EffectAt(obj[0xB])[5] = 0x1B;
}

// original 0x413D50 (area 99 +0x3C[3]; PSX 0x801F2D68): Area99_DriftStates
// by Sprite_Current[4].
extern "C" void __cdecl Area99_RunDrift(void) {
    StateEntry("Area99_RunDrift", at::kArea99DriftStates, at::kArea99StateCount, Sprite_Current[4])();
}

// original 0x413D70 (Area99_DriftStates[0]): +0xA = 0x48, state 1 (the object
// read again); the active member's word +0x8A - 2.
extern "C" void __cdecl Area99_DriftStart(void) {
    Sprite_Current[0xA] = 0x48;
    Sprite_Current[4] = 1;
    MemberStep();
}

// original 0x413DA0 (Area99_DriftStates[1]): while +0xA: z (+0x38) moved by
// the negated signed step Area99_DriftSteps[+0xA & 0xF] << 11, +0xA - 1 (the
// object read again), the active member's word +0x8A - 2; at 0, state 0.
extern "C" void __cdecl Area99_DriftStep(void) {
    unsigned char* const cur = Sprite_Current;
    if (cur[0xA] == 0) {
        cur[4] = 0;
        return;
    }
    const auto step = static_cast<std::int32_t>(static_cast<signed char>(B(at::kArea99DriftSteps + (cur[0xA] & 0xF))));
    AddLong(cur + 0x38, (0u - static_cast<U>(step)) << 11);
    unsigned char* const obj = Sprite_Current;
    obj[0xA] = static_cast<unsigned char>(obj[0xA] - 1);
    MemberStep();
}

// original 0x413DF0 (area 99 +0x3C[4]; PSX 0x801F2E58): Area99_LeapStates by
// Sprite_Current[4].
extern "C" void __cdecl Area99_RunLeap(void) {
    StateEntry("Area99_RunLeap", at::kArea99LeapStates, at::kArea99StateCount, Sprite_Current[4])();
}

// original 0x413E10 (Area99_LeapStates[0]): speed Field_MoveSpeeds[3]; +9 =
// 0, +0xB = 3, frames +0xA = 16 / speed, +0x14 = 0x4200, +0xC = 0x20000 /
// (frames * 4), +0x10 = -0x18000 / (frames * 4) (idivs: a speed of 0, or
// above 16, faults; ours aborts); then +0xA = 0xE, state 1; Field_State's
// word +0x12E - 2. Sprite_Current read again for every store.
extern "C" void __cdecl Area99_LeapStart(void) {
    const unsigned speed = B(at::kMoveSpeed3);
    Sprite_Current[9] = 0;
    Sprite_Current[0xB] = 3;
    if (speed == 0) bof3::Fatal("Area99_LeapStart: Field_MoveSpeeds[3] is 0, the original's idiv faults");
    Sprite_Current[0xA] = static_cast<unsigned char>(16 / static_cast<int>(speed));
    SetLong(Sprite_Current + 0x14, 0x4200);
    unsigned char* cur = Sprite_Current;
    std::int32_t d = static_cast<std::int32_t>(cur[0xA]) * 4;
    if (d == 0) bof3::Fatal("Area99_LeapStart: 16 / Field_MoveSpeeds[3] is 0, the original's idiv faults");
    SetLong(cur + 0xC, 0x20000 / d);
    cur = Sprite_Current;
    d = static_cast<std::int32_t>(cur[0xA]) * 4;
    if (d == 0) bof3::Fatal("Area99_LeapStart: 16 / Field_MoveSpeeds[3] is 0, the original's idiv faults");
    SetLong(cur + 0x10, -0x18000 / d);
    Sprite_Current[0xA] = 0xE;
    Sprite_Current[4] = 1;
    LeaderStep();
}

// original 0x413EC0 (Area99_LeapStates[1]): x += +0xC, z += +0x10, the word
// +0x3E += +0x14 >> 8 (arithmetic), +0x14 -= 0x700, +0xA - 1; rising before
// and falling now: Sprite_EnsureAnimation(0x51); then +0xA below 1: state 0,
// else Field_State's word +0x12E - 2. Sprite_Current read again for each.
extern "C" void __cdecl Area99_LeapStep(void) {
    unsigned char* cur = Sprite_Current;
    AddLong(cur + 0x34, static_cast<U>(Long(cur + 0xC)));
    cur = Sprite_Current;
    AddLong(cur + 0x38, static_cast<U>(Long(cur + 0x10)));
    cur = Sprite_Current;
    AddWord(cur + 0x3E, static_cast<int>(static_cast<std::uint16_t>(static_cast<U>(Long(cur + 0x14) >> 8))));
    cur = Sprite_Current;
    const std::int32_t rise = Long(cur + 0x14);
    SetLong(cur + 0x14, static_cast<std::int32_t>(static_cast<U>(rise) - 0x700u));
    cur = Sprite_Current;
    cur[0xA] = static_cast<unsigned char>(cur[0xA] - 1);
    if (rise > 0 && Long(Sprite_Current + 0x14) < 0) AH_CALL(Sprite_EnsureAnimation)(0x51);
    cur = Sprite_Current;
    if (cur[0xA] < 1) {
        cur[4] = 0;
        return;
    }
    LeaderStep();
}

// original 0x413F50 (area 99 +0x3C[5]; PSX 0x801F30A8): the chapter row's
// flag 0x1F toggled (0x57C160).
extern "C" void __cdecl Area99_ToggleRowFlag1F(void) { FlagsToggle(FlagRow(), 0x1F); }

// original 0x413F70 (area 99 +0x40; PSX 0x801F30D0): Cond_ByteFD 1: cell
// (0x23, 0x4F) = 0x51 when Cond row 3's flag 0x1C is set and 0x1D clear, else
// 0x50.
extern "C" void __cdecl Area99_InitCell(void) {
    if (Cond_ByteFD != 1) return;
    unsigned char* const row = Mem(at::kCondRow3);
    if (AH_CALL(Flags_Test)(row, 0x1C) != 0 && AH_CALL(Flags_Test)(row, 0x1D) == 0) {
        SetByte(0x23, 0x4F, 0x51);
        return;
    }
    SetByte(0x23, 0x4F, 0x50);
}

// ===========================================================================
// Area 100 (descriptor 0x61AAF0): one choice (handler 8), nine handlers (1
// and 5 other blocks'), the init, a step hook, tail kind 28, object triggers
// 13, 16 and 30, effect kind 0xB7's handler and its draw state.
// ===========================================================================

// original 0x413FC0 (area 100 +0x3C[0]; PSX 0x801F34FC): by story flags 0x94
// (1) and 0x93 (2): Field_ChangeArea to area 0x3B (neither), 0x24 (0x94),
// 0x74 (0x93) or 0x70 (both), flags 0x81.
extern "C" void __cdecl Area100_ChangeAreaByFlags(void) {
    const unsigned a = AH_CALL(Flags_Test)(StoryFlags(), 0x94) != 0 ? 1u : 0u;
    const unsigned b = AH_CALL(Flags_Test)(StoryFlags(), 0x93) != 0 ? 2u : 0u;
    switch (a | b) {
    case 1: AH_CALL(Field_ChangeArea)(0x24, 0x478000, 0x240000, 0x81); break;
    case 2: AH_CALL(Field_ChangeArea)(0x74, 0x478000, 0x340000, 0x81); break;
    case 3: AH_CALL(Field_ChangeArea)(0x70, 0xC8000, 0x580000, 0x81); break;
    default: AH_CALL(Field_ChangeArea)(0x3B, 0x478000, 0x2A0000, 0x81); break;
    }
}

// original 0x414070 (area 100 +0x3C[2]; PSX 0x801F3608): story flag 0x45.
extern "C" void __cdecl Area100_SetFlag45(void) { AH_CALL(Flags_Set)(StoryFlags(), 0x45); }

// original 0x414080 (areas 49 +0x34[17] = +0x3C[15], 100 +0x3C[3], 133
// +0x3C[2]; PSX 0x801F3630 in area 100): Camera_ShiftY + 4, MapView_Redraw 2.
extern "C" void __cdecl Area100_ShiftCameraUp4(void) {
    Camera_ShiftY = static_cast<short>(Camera_ShiftY + 4);
    MapView_Redraw = 2;
}

// original 0x414090 (areas 49 +0x34[16] = +0x3C[14], 100 +0x3C[4], 133
// +0x3C[1]; PSX 0x801F3658 in area 100): Camera_ShiftY - 4, MapView_Redraw 2.
extern "C" void __cdecl Area100_ShiftCameraDown4(void) {
    Camera_ShiftY = static_cast<short>(Camera_ShiftY - 4);
    MapView_Redraw = 2;
}

// original 0x4140A0 (area 100 +0x3C[6]; PSX 0x801F369C): TestAndSpawn, kind
// 0x9B.
extern "C" void __cdecl Area100_SpawnEffect9B(void) { TestAndSpawn(0x9B); }

// original 0x4140F0 (area 100 +0x3C[7]; PSX 0x801F3724): TestAndSpawn, kind
// 0xB7 (whose handler is Area100_EffectB7Run).
extern "C" void __cdecl Area100_SpawnEffectB7(void) { TestAndSpawn(0xB7); }

// original 0x414140 (area 100 +0x34[0] = +0x3C[8]; PSX 0x801F37AC): the
// message word Area100_ChoiceMessages[the s8 answer] (unchecked; it reads
// inside .data); then by the answer: 1 - flag 0x93 cleared, 0x94 and 0x33
// set; 2 - 0x93 and 0x94 cleared, 0x33 set; 3 - key item 6 held: 0x93, 0x94,
// 0x33 set, else message 9 and 0x33 cleared; 4 - nothing; any other - 0x93
// set, 0x94 cleared, 0x33 set.
extern "C" void __cdecl Area100_ChoiceFlags93(void) {
    const auto answer = static_cast<signed char>(B(at::kChoiceAnswer));
    SetMessage(Word(Mem(at::kArea100ChoiceMessages + static_cast<U>(static_cast<std::int32_t>(answer) * 2))));
    unsigned char* const flags = StoryFlags();
    switch (answer) {
    case 1:
        AH_CALL(Flags_Clear)(flags, 0x93);
        AH_CALL(Flags_Set)(flags, 0x94);
        AH_CALL(Flags_Set)(flags, 0x33);
        return;
    case 2:
        AH_CALL(Flags_Clear)(flags, 0x93);
        AH_CALL(Flags_Clear)(flags, 0x94);
        AH_CALL(Flags_Set)(flags, 0x33);
        return;
    case 3:
        if (AH_CALL(KeyItem_Has)(6) != 0) {
            AH_CALL(Flags_Set)(flags, 0x93);
            AH_CALL(Flags_Set)(flags, 0x94);
            AH_CALL(Flags_Set)(flags, 0x33);
            return;
        }
        SetMessage(9);
        AH_CALL(Flags_Clear)(flags, 0x33);
        return;
    case 4: return;
    default:
        AH_CALL(Flags_Set)(flags, 0x93);
        AH_CALL(Flags_Clear)(flags, 0x94);
        AH_CALL(Flags_Set)(flags, 0x33);
        return;
    }
}

// original 0x414240 (Field_ModeTailKinds[28], armed by Area100_Trigger13): by
// the s8 state (above 4 or negative: nothing), 0 - Kind2_Place(0), state 1;
// 1 - once counter 0 is 1, an effect of kind 0x9B at (0x460000, 0x340000),
// the timer 0x1E, state 2; 2 - the timer down, at 0 state 3 and counter 0 +
// 1; 3 - once counter 0 is 0, Msg_OpenScript(3), Field_Request 2, state 4; 4
// - once Field_Request is not 2, the tail disarmed. The table is 0x41431C (5
// entries, in .text).
extern "C" void __cdecl Area100_TailEffect9B(void) {
    switch (static_cast<signed char>(B(at::kTailState))) {
    case 0:
        AH_CALL(Kind2_Place)(0);
        B(at::kTailState) = 1;
        return;
    case 1:
        if (B(at::kCounter0) != 1) return;
        SpawnAt46(0x9B);
        SetWord(Mem(at::kTailTimer), 0x1E);
        B(at::kTailState) = 2;
        return;
    case 2: {
        const auto left = static_cast<std::uint16_t>(Word(Mem(at::kTailTimer)) - 1);
        SetWord(Mem(at::kTailTimer), left);
        if (left != 0) return;
        const unsigned char count = B(at::kCounter0);
        B(at::kTailState) = 3;
        B(at::kCounter0) = static_cast<unsigned char>(count + 1);
        return;
    }
    case 3:
        if (B(at::kCounter0) != 0) return;
        AH_CALL(Msg_OpenScript)(3);
        Field_Request = 2;
        B(at::kTailState) = 4;
        return;
    case 4:
        if (Field_Request == 2) return;
        Disarm();
        return;
    default: return;
    }
}

// original 0x414330 (Area_StepHook's case for area 100): Cond_ByteFD 3, story
// flag 0x33, x's high word 0x45..0x47 and z's 0x33..0x35 (16-bit compares),
// the leader's pose 0, 7 or 6: counter 0 = 0, Party_DropIn(0), al 1. Else al
// 0.
extern "C" unsigned char __cdecl Area100_StepHook(long x, long z) {
    if (Cond_ByteFD != 3) return 0;
    if (AH_CALL(Flags_Test)(StoryFlags(), 0x33) == 0) return 0;
    if (static_cast<std::uint16_t>(High(x) - 0x45) >= 3) return 0;
    if (static_cast<std::uint16_t>(High(z) - 0x33) >= 3) return 0;
    const unsigned char pose = B(at::kLeaderPose);
    if (pose != 0 && pose != 7 && pose != 6) return 0;
    B(at::kCounter0) = 0;
    AH_CALL(Party_DropIn)(0);
    return 1;
}

// original 0x414390 (Field_ObjectTriggers id 16; a gap of the tool, area
// 100's block): story flag 0x45, MoveCmd_TestFB(0x4A, 0x1E),
// Sound_PlayEffect(0x109), the four gate cells 0xA1. al 0.
extern "C" unsigned char __cdecl Area100_Trigger16(unsigned char*, unsigned char*) {
    AH_CALL(Flags_Set)(StoryFlags(), 0x45);
    AH_CALL(MoveCmd_TestFB)(0x4A, 0x1E);
    AH_CALL(Sound_PlayEffect)(0x109);
    GateCells(0xA1);
    return 0;
}

// original 0x4143F0 (Field_ObjectTriggers id 13; a gap of the tool): story
// flag 0x40, MoveCmd_TestFB(0x4A, 0x2F), tail kind 28 armed. al 0.
extern "C" unsigned char __cdecl Area100_Trigger13(unsigned char*, unsigned char*) {
    AH_CALL(Flags_Set)(StoryFlags(), 0x40);
    AH_CALL(MoveCmd_TestFB)(0x4A, 0x2F);
    ArmTail(0x1C);
    return 0;
}

// original 0x414420 (Field_ObjectTriggers id 30; a gap of the tool): tail kind
// 44 (engine, 0x56DE50) armed with sub-kind 2. al 0.
extern "C" unsigned char __cdecl Area100_Trigger30(unsigned char*, unsigned char*) {
    ArmTail(0x2C);
    B(at::kTailSub) = 2;
    return 0;
}

// original 0x414440 (area 100 +0x40; PSX 0x801F3C2C): story flag 0x45 clear:
// the four gate cells 0x50.
extern "C" void __cdecl Area100_InitCells(void) {
    if (AH_CALL(Flags_Test)(StoryFlags(), 0x45) != 0) return;
    GateCells(0x50);
}

// original 0x414490 (Effect_KindHandlers[0xB7], 0x65562C; a gap of the tool):
// Area100_EffectStates by Sprite_Current[1] (the running effect record).
extern "C" void __cdecl Area100_EffectB7Run(void) {
    StateEntry("Area100_EffectB7Run", at::kArea100EffectStates, at::kArea100EffectStateCount, Sprite_Current[1])();
}

// original 0x4144B0 (Area100_EffectStates[1]): the point (x +0x34, z +0x38,
// y +0x3C) of the running record copied to the stack and handed to 0x4220D0
// (area_w2d_callees.h).
extern "C" void __cdecl Area100_EffectB7Ring(void) {
    const unsigned char* const cur = Sprite_Current;
    long point[4];
    point[0] = Long(cur + 0x34);
    point[1] = Long(cur + 0x38);
    point[2] = Long(cur + 0x3C);
    point[3] = 0;
    AH_AT(void (__cdecl*)(const long*), at::kRingAt)(point);
}

// ===========================================================================
// Area 103 (descriptor 0x61B4A0): six handlers and their helper, object
// trigger 35.
// ===========================================================================

// original 0x4144E0 (area 103 +0x3C[0]; PSX 0x801F560C): the script object's
// word +8 = Area103_FirstListedMember + 2 (16 bits: none makes 0x101),
// MoveScript_SetTurnTarget(it), its bit 0x10 cleared; then its bit 0x20
// cleared when the running object's +7 has bit 4, else set.
extern "C" void __cdecl Area103_TalkByMember(void) {
    const unsigned char row = AH_CALL(Area103_FirstListedMember)();
    SetWord(MoveScript_Object + 8, static_cast<unsigned>(row) + 2);
    AH_CALL(MoveScript_SetTurnTarget)(MoveScript_Object);
    MoveScript_Object[0] = static_cast<unsigned char>(MoveScript_Object[0] & 0xEF);
    const unsigned char bits = Sprite_Current[7];
    unsigned char* const obj = MoveScript_Object;
    obj[0] = static_cast<unsigned char>((bits & 4) != 0 ? obj[0] & 0xDF : obj[0] | 0x20);
}

// original 0x414540 (called by Area103_TalkByMember): the first i of 0..2
// whose Area103_MemberKeys[i] is some member's byte +0x89 (members 0..
// Field_MemberCount - 1, the count read once), or 0xFF.
extern "C" unsigned char __cdecl Area103_FirstListedMember(void) {
    const unsigned char count = Field_MemberCount;
    for (unsigned i = 0; i < at::kArea103MemberKeyCount; ++i) {
        const unsigned char key = B(at::kArea103MemberKeys + i);
        for (unsigned m = 0; m < count; ++m)
            if (key == B(at::kLeader + m * at::kPartyStride + 0x89)) return static_cast<unsigned char>(i);
    }
    return 0xFF;
}

// original 0x4145B0 (area 103 +0x3C[1]; PSX 0x801F5738): Field_State +0x89
// not 0: the script position + 0x17.
extern "C" void __cdecl Area103_SkipIfLeader89(void) {
    if (Field_State[0x89] != 0) ScriptStep(0x17);
}

// original 0x4145D0 (area 103 +0x3C[2]; PSX 0x801F5778): Field_State +0x89
// 0: counter 0 + 1.
extern "C" void __cdecl Area103_CountIfLeader89Zero(void) {
    if (Field_State[0x89] == 0) B(at::kCounter0) = static_cast<unsigned char>(B(at::kCounter0) + 1);
}

// original 0x4145F0 (area 103 +0x3C[3]; PSX 0x801F57B4): MapView_Redraw 2,
// MapView_Elevation + Area103_Shake[Frame_Counter & 3] (signed) * 8.
extern "C" void __cdecl Area103_ShakeElevation(void) {
    const unsigned f = Frame_Counter & 3;
    const long now = MapView_Elevation;
    MapView_Redraw = 2;
    const auto step = static_cast<std::int32_t>(static_cast<signed char>(B(at::kArea103Shake + f)));
    MapView_Elevation = static_cast<long>(static_cast<U>(now) + static_cast<U>(step) * 8u);
}

// original 0x414620 (area 103 +0x3C[4]; PSX 0x801F57FC): the running object's
// word +0x3E = the ground at its (x, z) (the object read again).
extern "C" void __cdecl Area103_Ground(void) {
    const unsigned char* const cur = Sprite_Current;
    const long ground = AH_CALL(AreaMap_Elevation)(Long(cur + 0x34), Long(cur + 0x38));
    SetWord(Sprite_Current + 0x3E, static_cast<unsigned>(ground));
}

// original 0x414640 (area 103 +0x3C[5]; PSX 0x801F5834): Effect_FindFree; a
// slot: kind 0x4D at the running object's +0x34 / +0x38 / +0x3C; none: the
// script position - 2.
extern "C" void __cdecl Area103_SpawnEffect4D(void) {
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    if (slot == 0xFF) {
        ScriptStep(-2);
        return;
    }
    const unsigned char* const cur = Sprite_Current;
    unsigned char* const e = EffectAt(slot);
    e[0] = 1;
    e[5] = 0x4D;
    SetLong(e + 0x34, Long(cur + 0x34));
    SetLong(e + 0x38, Long(cur + 0x38));
    SetLong(e + 0x3C, Long(cur + 0x3C));
}

// original 0x4146A0 (Field_ObjectTriggers id 35; a gap of the tool, area
// 103's block): tail kind 44 (engine) armed with sub-kind 8. al 0.
extern "C" unsigned char __cdecl Area103_Trigger35(unsigned char*, unsigned char*) {
    ArmTail(0x2C);
    B(at::kTailSub) = 8;
    return 0;
}

void AreaW2d_Inject() {
    if (bof3::WantsShadow("area_w2d")) area_w2d::SelfTest();
    BOF3_INJECT(Area95_TurnAtCell);
    BOF3_INJECT(Area95_WaitEffectAtCell);
    BOF3_INJECT(Area95_SpawnEffect37);
    BOF3_INJECT(Area95_LeapArc);
    BOF3_INJECT(Area95_TurnAndMove);
    BOF3_INJECT(Area96_TurnAtCell);
    BOF3_INJECT(Area96_ToggleCell);
    BOF3_INJECT(Area96_FaceBack);
    BOF3_INJECT(Area96_SnapElevation);
    BOF3_INJECT(Area97_TailWaitCounter3);
    BOF3_INJECT(Area97_StepHook);
    BOF3_INJECT(Area97_FlagIfKeyItem5);
    BOF3_INJECT(Area98_ChoiceFlag9);
    BOF3_INJECT(Area98_ChoiceAsk2F);
    BOF3_INJECT(Area98_ChoiceArmTail45);
    BOF3_INJECT(Area98_SpawnEffect37);
    BOF3_INJECT(Area98_InitCells);
    BOF3_INJECT(Area98_TailChangeArea62);
    BOF3_INJECT(Area98_Trigger44);
    BOF3_INJECT(Area98_CameraDistanceFF00);
    BOF3_INJECT(Area98_CameraDistance0);
    BOF3_INJECT(Area99_TurnSoundEffect1B);
    BOF3_INJECT(Area99_RunDrift);
    BOF3_INJECT(Area99_DriftStart);
    BOF3_INJECT(Area99_DriftStep);
    BOF3_INJECT(Area99_RunLeap);
    BOF3_INJECT(Area99_LeapStart);
    BOF3_INJECT(Area99_LeapStep);
    BOF3_INJECT(Area99_ToggleRowFlag1F);
    BOF3_INJECT(Area99_InitCell);
    BOF3_INJECT(Area100_ChangeAreaByFlags);
    BOF3_INJECT(Area100_SetFlag45);
    BOF3_INJECT(Area100_ShiftCameraUp4);
    BOF3_INJECT(Area100_ShiftCameraDown4);
    BOF3_INJECT(Area100_SpawnEffect9B);
    BOF3_INJECT(Area100_SpawnEffectB7);
    BOF3_INJECT(Area100_ChoiceFlags93);
    BOF3_INJECT(Area100_TailEffect9B);
    BOF3_INJECT(Area100_StepHook);
    BOF3_INJECT(Area100_Trigger16);
    BOF3_INJECT(Area100_Trigger13);
    BOF3_INJECT(Area100_Trigger30);
    BOF3_INJECT(Area100_InitCells);
    BOF3_INJECT(Area100_EffectB7Run);
    BOF3_INJECT(Area100_EffectB7Ring);
    BOF3_INJECT(Area103_TalkByMember);
    BOF3_INJECT(Area103_FirstListedMember);
    BOF3_INJECT(Area103_SkipIfLeader89);
    BOF3_INJECT(Area103_CountIfLeader89Zero);
    BOF3_INJECT(Area103_ShakeElevation);
    BOF3_INJECT(Area103_Ground);
    BOF3_INJECT(Area103_SpawnEffect4D);
    BOF3_INJECT(Area103_Trigger35);
}

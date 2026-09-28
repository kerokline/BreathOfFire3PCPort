// World 3's area 135: the PSX's BIN/WORLD03/AREA135.EMI compiled into the exe
// at 0x41DAD0..0x41EFE0 (Area_Descriptors entry 135, descriptor 0x62D940).
// Round ten, group AR3D: the band's 46 functions, none ours before, each read
// to its last instruction with capstone (2026-09-28) and taken through the
// area harness (area_harness.h). docs/area_w3d.md.
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. Reads
// by an unchecked index into .data are kept (they stay in .data); where the
// original jumps through a pointer read past what its dispatcher's table
// reaches, divides by a byte that can be 0, or writes through an index past
// the 30 field objects or the 20 effect records, ours aborts with a message
// instead (round nine section 6: no ledger entry). Every call goes through
// the harness (AH_CALL / AH_AT), so the start-up fuzz can stand recorders in
// for ours as for the originals' copies; the group's own callees are called
// the same way, so each function is fuzzed alone.
#include "game/area_w3d.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w3d_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w3d::at;
using area_harness::Mem;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using U = std::uint32_t;

unsigned char& B(U address) { return *Mem(address); }
U D(U address) { return static_cast<U>(Long(Mem(address))); }
void SetD(U address, U v) { SetLong(Mem(address), static_cast<std::int32_t>(v)); }
void AddWord(unsigned char* p, unsigned v) { SetWord(p, Word(p) + v); }
void SetMessage(unsigned id) { SetWord(Mem(at::kMessage), id); }
U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
// The movement script back (or on) by `delta` words: the running script
// object's u16 +0xA, the pointer read at the store.
void ScriptStep(unsigned delta) { AddWord(MoveScript_Object + 0xA, delta); }
// An arithmetic shift of a dword, as the originals' sar.
std::int32_t Sar(U v, unsigned n) { return static_cast<std::int32_t>(v) >> n; }

// Field object `index` (Sprite_Objects, 30 of 0xA4 bytes). The tail writes
// through an index it keeps in the sub byte, unchecked: past the 30 objects
// the original writes into whatever follows them - ours aborts.
unsigned char* ObjectAt(const char* who, unsigned index) {
    if (index >= at::kObjectCount)
        bof3::Fatal("%s: the object index %u (tail sub byte 0x9039F5) is past the 30 field objects (the original writes "
                    "0x%X, past Sprite_Objects)",
                    who, index, static_cast<unsigned>(at::kSpriteObjects + index * at::kObjectStride));
    return Mem(at::kSpriteObjects + index * at::kObjectStride);
}
// Effect record `slot` (Effect_Objects, 20 of 0x80 bytes). Choice 2 stores
// Effect_FindFree's "none", 0xFF, as the slot the tail later writes through,
// unchecked: past the 20 records ours aborts.
unsigned char* EffectAt(const char* who, unsigned slot) {
    if (slot >= at::kEffectCount)
        bof3::Fatal("%s: the effect slot %u is past the 20 effect records (the original writes 0x%X, past "
                    "Effect_Objects)",
                    who, slot, static_cast<unsigned>(at::kEffectObjects + slot * at::kEffectStride));
    return Mem(at::kEffectObjects + slot * at::kEffectStride);
}

// A .data state table read in place by the running object's +4 and jumped
// through, unchecked. `reach` is how many dwords from the table's start are
// code pointers (the tables are contiguous and each dispatcher runs on into
// the next table's entries); past that the original jumps through a zero or a
// data word - ours aborts.
void RunState(const char* who, U table, unsigned reach) {
    const unsigned state = Sprite_Current[4];
    if (state >= reach)
        bof3::Fatal("%s: the running object's state %u is past the %u code pointers from 0x%X (the original jumps "
                    "through the dword after them)",
                    who, state, reach, static_cast<unsigned>(table));
    reinterpret_cast<area_harness::Handler>(static_cast<std::uintptr_t>(D(table + state * 4)))();
}

// Handlers 1 and 2's rise: the running object's word +0x3E += its word +0x14,
// the script back 2 - in zone 4 (Cond_ByteFD) or outside it.
void Rise(bool zone4) {
    if ((Cond_ByteFD == 4) != zone4) return;
    unsigned char* const cur = Sprite_Current;
    AddWord(cur + 0x3E, Word(cur + 0x14));
    ScriptStep(0xFFFE);
}

// Handlers 7 and 8: with the running object's word `word` at `least` or more
// (signed) and the button held (Input_Held bit 0x20): its direction +8 =
// `direction`, the script object's +7 = 2, MoveCmd_Move(the script object,
// +8), +0x14 = 0, the script back 2.
void HeldMove(unsigned word, short least, unsigned char direction) {
    unsigned char* const cur = Sprite_Current;
    if (static_cast<short>(Word(cur + word)) < least) return;
    if (!(B(at::kInputHeld) & 0x20)) return;
    cur[8] = direction;
    MoveScript_Object[7] = 2;
    unsigned char* const object = MoveScript_Object;
    AH_CALL(MoveCmd_Move)(object, Sprite_Current[8]);
    SetLong(Sprite_Current + 0x14, 0);
    ScriptStep(0xFFFE);
}

// The lift's steps: MapView_Elevation moved by `delta`, the redraw 2, the
// running object's state + 1, the script back 2.
void Lift(U delta) {
    const U elevation = D(at::kElevation);
    B(at::kRedraw) = 2;
    SetD(at::kElevation, elevation + delta);
    Sprite_Current[4] = static_cast<unsigned char>(Sprite_Current[4] + 1);
    ScriptStep(0xFFFE);
}

// The tail's exit (0x41E6AF): ScriptFlags_Clear40, the tail kind and state 0.
void Disarm() {
    AH_CALL(ScriptFlags_Clear40)();
    B(at::kTailKind) = 0;
    B(at::kTailState) = 0;
}
// The glide divisor the tail sets: Field_MoveSpeeds[3] << 3, as a word.
unsigned Speed8() { return static_cast<unsigned>(B(at::kMoveSpeed3)) << 3; }

}  // namespace

// ===========================================================================
// Choices 0..3 (Area135_Choices 0x62D8DC; choices 4..23 are the handlers).
// ===========================================================================

// original 0x41DAD0 (Area135_Choices[0]): message 0xFFFF; answer 0:
// ScriptFlags_Set40, tail kind 0x12 at state 0.
extern "C" void __cdecl Area135_ChoiceStartTail(void) {
    const unsigned char answer = B(at::kChoiceAnswer);
    SetMessage(0xFFFF);
    if (answer != 0) return;
    AH_CALL(ScriptFlags_Set40)();
    B(at::kTailKind) = 0x12;
    B(at::kTailState) = 0;
}

// original 0x41DB00 (Area135_Choices[1]): message 0x84; the tail's sub byte =
// the answer + 1, printed into Text_Records by area 8's format
// (Crt_sprintf); the marker's +0x83 = the answer (read again) + 4.
extern "C" void __cdecl Area135_ChoiceCount(void) {
    const auto count = static_cast<unsigned char>(B(at::kChoiceAnswer) + 1);
    SetMessage(0x84);
    B(at::kTailSub) = count;
    AH_CALL(Crt_sprintf)(reinterpret_cast<char*>(Mem(at::kTextRecords)),
                         reinterpret_cast<const char*>(Mem(at::kMessageFormat)), static_cast<unsigned>(count));
    B(at::kMarker83) = static_cast<unsigned char>(B(at::kChoiceAnswer) + 4);
}

// original 0x41DB40 (Area135_Choices[2]): the s8 answer 0 or 1: ScriptFlags_Set40,
// tail kind 0x12 at state 0x14, Effect_FindFree to the slot byte 0x675CC0 (the
// "none" 0xFF stored too); a slot: +0 = 1, kind +5 = 0x66, +6 = the answer.
// Any answer: message 0xFFFF.
extern "C" void __cdecl Area135_ChoiceStartTail14(void) {
    const auto answer = static_cast<signed char>(B(at::kChoiceAnswer));
    if (answer == 0 || answer == 1) {
        const auto which = static_cast<unsigned char>(answer);
        AH_CALL(ScriptFlags_Set40)();
        B(at::kTailKind) = 0x12;
        B(at::kTailState) = 0x14;
        const unsigned char slot = AH_CALL(Effect_FindFree)();
        B(at::kEffectSlot) = slot;
        if (slot != 0xFF) {
            unsigned char* const e = EffectAt("Area135_ChoiceStartTail14", B(at::kEffectSlot));
            e[0] = 1;
            e[5] = 0x66;
            e[6] = which;
        }
    }
    SetMessage(0xFFFF);
}

// original 0x41DBB0 (Area135_Choices[3]): message 0xFFFF; answer 0 sets story
// flag 0x37, any other clears it.
extern "C" void __cdecl Area135_ChoiceFlag37(void) {
    const unsigned char answer = B(at::kChoiceAnswer);
    SetMessage(0xFFFF);
    if (answer == 0)
        AH_CALL(Flags_Set)(Mem(at::kStoryFlags), 0x37);
    else
        AH_CALL(Flags_Clear)(Mem(at::kStoryFlags), 0x37);
}

// ===========================================================================
// Handlers 0..19 (Area135_Handlers 0x62D8EC = choices 4..23).
// ===========================================================================

// original 0x41DBE0 (Area135_Handlers[0]; PSX 0x801F40F8): story flag 0xC
// cleared, the tail's sub byte 0.
extern "C" void __cdecl Area135_ClearFlagC(void) {
    AH_CALL(Flags_Clear)(Mem(at::kStoryFlags), 0xC);
    B(at::kTailSub) = 0;
}

// original 0x41DC00 (Area135_Handlers[1]; PSX 0x801F4128): Rise in zone 4.
extern "C" void __cdecl Area135_RiseInZone4(void) { Rise(true); }
// original 0x41DC30 (Area135_Handlers[2]; PSX 0x801F4178): Rise outside zone 4.
extern "C" void __cdecl Area135_RiseElsewhere(void) { Rise(false); }

// original 0x41DC60 (Area135_Handlers[3]; PSX 0x801F41C8): the exit by the
// script object's +3 less 6 (a byte; unchecked, the tables stay in .data):
// Field_ChangeArea(0x87, x << 16, z << 16, 0x81) from Area135_ExitsZone4 in
// zone 4, else Area135_Exits; then Sprite_Kind2's +0x80 |= 8.
extern "C" void __cdecl Area135_LeaveByExit(void) {
    const auto exit = static_cast<unsigned char>(MoveScript_Object[3] - 6);
    const U table = Cond_ByteFD == 4 ? at::kArea135ExitsZone4 : at::kArea135Exits;
    const U z = static_cast<U>(B(table + exit * 2u + 1)) << 16;
    const U x = static_cast<U>(B(table + exit * 2u)) << 16;
    AH_CALL(Field_ChangeArea)(0x87, static_cast<int>(x), static_cast<int>(z), 0x81);
    B(at::kKind2Byte80) = static_cast<unsigned char>(B(at::kKind2Byte80) | 8);
}

// original 0x41DCE0 (Area135_Handlers[4]; PSX 0x801F4270): every one of the
// sixteen Area135_Routes whose cell is the marker's (its words +0x36 / +0x3A):
// with no target (a cell 0xFF) the script back 4 and, for the leader, counter
// 3 = 8; else the target x = cell << 16 (+ 0x8000 by its half flag), z the
// same, the running object's +0x24 bit 5 cleared (through the pointer last
// read), +2 = 0, MoveCmd_OpF7(x, z, 0x10, the script object), the script
// object's +7 = 1, +8 = the route's direction, and for the leader
// Field_Kind2X / Z = x / z and the F3 divisor 0x20. Every route is tried.
extern "C" void __cdecl Area135_MarkerRoute(void) {
    unsigned char* last = Sprite_Current;
    for (unsigned i = 0; i < at::kArea135RouteCount; ++i) {
        const unsigned char* const r = Mem(at::kArea135Routes + i * at::kArea135RouteStride);
        if (Word(Mem(at::kMarkerCellX)) != r[0]) continue;
        if (Word(Mem(at::kMarkerCellZ)) != r[1]) continue;
        if (r[3] == 0xFF || r[5] == 0xFF) {
            ScriptStep(0xFFFC);
            last = Sprite_Current;
            if (last == ObjTrio) B(at::kCounter3) = 8;
            continue;
        }
        const U x = (r[2] ? 0x8000u : 0u) | static_cast<U>(r[3]) << 16;
        const U z = (r[4] ? 0x8000u : 0u) | static_cast<U>(r[5]) << 16;
        last[0x24] = static_cast<unsigned char>(last[0x24] & 0xDF);
        Sprite_Current[2] = 0;
        AH_CALL(MoveCmd_OpF7)(static_cast<long>(x), static_cast<long>(z), 0x10, MoveScript_Object);
        MoveScript_Object[7] = 1;
        Sprite_Current[8] = r[6];
        last = Sprite_Current;
        if (last == ObjTrio) {
            SetD(at::kKind2X, x);
            SetD(at::kKind2Z, z);
            SetWord(Mem(at::kF3Divisor), 0x20);
        }
    }
}

// original 0x41DE00 (Area135_Handlers[5]; PSX 0x801F4464): the running object
// Sprite_Kind2: Field_Kind2X / Z = the marker's x / z and the F3 divisor 0x20;
// else its x, z, y dwords = the marker's.
extern "C" void __cdecl Area135_ToMarker(void) {
    if (Sprite_Current == Mem(at::kSpriteKind2)) {
        const U x = D(at::kMarkerX);
        const U z = D(at::kMarkerZ);
        SetWord(Mem(at::kF3Divisor), 0x20);
        SetD(at::kKind2X, x);
        SetD(at::kKind2Z, z);
        return;
    }
    SetLong(Sprite_Current + 0x34, static_cast<std::int32_t>(D(at::kMarkerX)));
    SetLong(Sprite_Current + 0x38, static_cast<std::int32_t>(D(at::kMarkerZ)));
    SetLong(Sprite_Current + 0x3C, static_cast<std::int32_t>(D(at::kMarkerY)));
}

// original 0x41DE60 (Area135_Handlers[6]; PSX 0x801F44E0): unless Field_State
// is party record 0 (its index, (Field_State - ObjTrio) / 0x14C as a signed
// dword, taken as a byte): the running object's +8 = the leader's direction,
// Sprite_SetAnimation(+8).
extern "C" void __cdecl Area135_FaceLeaderDir(void) {
    const auto delta = static_cast<std::int32_t>(Key(Field_State) - at::kLeader);
    const auto index = static_cast<unsigned char>(delta / static_cast<std::int32_t>(at::kPartyStride));
    if (index == 0) return;
    Sprite_Current[8] = B(at::kLeaderDir);
    AH_CALL(Sprite_SetAnimation)(Sprite_Current[8]);
}

// original 0x41DEA0 (Area135_Handlers[7]; PSX 0x801F456C): HeldMove on the
// cell z word +0x3A at 7 or more, direction 1.
extern "C" void __cdecl Area135_HeldMove1(void) { HeldMove(0x3A, 7, 1); }
// original 0x41DF00 (Area135_Handlers[8]; PSX 0x801F460C): the cell x word
// +0x36 at 6 or more, direction 7.
extern "C" void __cdecl Area135_HeldMove7(void) { HeldMove(0x36, 6, 7); }

// original 0x41DF60 (Area135_Handlers[9]; PSX 0x801F46AC): the highest
// MapView_GroundAt (signed words; from 0xFFFF8000) over eight points - x from
// the running object's x - 0x18000 in four steps of 0x10000 (x read again for
// each row), z from its z - 0x8000 in two - with MapView_HeightScale 1 before
// each; then Area135_ObjectAtCell(its x, z) to the tail's sub byte: an object
// there: Area135_FallTo(AreaMap_Elevation(x, z) + 0x280); none:
// Area135_FallTo(the highest + 0x180).
extern "C" void __cdecl Area135_FallToFloor(void) {
    U z = static_cast<U>(Long(Sprite_Current + 0x38)) - 0x8000;
    U best = 0xFFFF8000u;
    for (unsigned row = 0; row < 2; ++row) {
        U x = static_cast<U>(Long(Sprite_Current + 0x34)) - 0x18000;
        for (unsigned column = 0; column < 4; ++column) {
            MapView_HeightScale = 1;
            const auto ground = static_cast<U>(AH_CALL(MapView_GroundAt)(static_cast<long>(x), static_cast<long>(z)));
            if (static_cast<short>(best) < static_cast<short>(ground)) best = ground;
            x += 0x10000;
        }
        z += 0x10000;
    }
    const unsigned char found =
        AH_CALL(Area135_ObjectAtCell)(Long(Sprite_Current + 0x34), Long(Sprite_Current + 0x38));
    B(at::kTailSub) = found;
    if (found != 0xFF) {
        const long top = AH_CALL(AreaMap_Elevation)(Long(Sprite_Current + 0x34), Long(Sprite_Current + 0x38));
        AH_CALL(Area135_FallTo)(static_cast<unsigned>(top + 0x280));
        return;
    }
    AH_CALL(Area135_FallTo)(best + 0x180);
}

// original 0x41E040 (Area135_Handlers[10]; PSX 0x801F4828): Area135_FallTo(0x780).
extern "C" void __cdecl Area135_Fall780(void) { AH_CALL(Area135_FallTo)(0x780); }
// original 0x41E050 (Area135_Handlers[11]; PSX 0x801F4848): jmp Area135_StampCells.
extern "C" void __cdecl Area135_StampCellsRun(void) { AH_CALL(Area135_StampCells)(); }
// original 0x41E060 (Area135_Handlers[12]; PSX 0x801F4868): jmp
// Area135_RestoreCells.
extern "C" void __cdecl Area135_RestoreCellsRun(void) { AH_CALL(Area135_RestoreCells)(); }
// original 0x41E070 (Area135_Handlers[13]; PSX 0x801F4888): Field_Request 5:
// jmp Area135_RestoreCells; else the script back 2.
extern "C" void __cdecl Area135_RestoreOnRequest5(void) {
    if (Field_Request == 5) {
        AH_CALL(Area135_RestoreCells)();
        return;
    }
    ScriptStep(0xFFFE);
}

// original 0x41E090 (Area135_Handlers[14]; PSX 0x801F48DC): for each of the
// four Area135_JumpCells (cell << 16 | 0x8000) the running object stands on
// exactly, Area135_ObjectAtCell there: an object stops the walk (the running
// object read again after a "none"); the script moves on 3 x the cells
// passed (12 when none stopped it).
extern "C" void __cdecl Area135_JumpByOccupied(void) {
    const unsigned char* cur = Sprite_Current;
    unsigned k = 0;
    for (; k < 4; ++k) {
        const U x = static_cast<U>(B(at::kArea135JumpCells + k * 2)) << 16 | 0x8000;
        const U z = static_cast<U>(B(at::kArea135JumpCells + k * 2 + 1)) << 16 | 0x8000;
        if (static_cast<U>(Long(cur + 0x34)) != x) continue;
        if (static_cast<U>(Long(cur + 0x38)) != z) continue;
        if (AH_CALL(Area135_ObjectAtCell)(static_cast<long>(x), static_cast<long>(z)) != 0xFF) break;
        cur = Sprite_Current;
    }
    ScriptStep(k * 3);
}

// original 0x41E110 (Area135_Handlers[15]; PSX 0x801F49A8): jmp through
// Area135_QueueStates by the running object's +4.
extern "C" void __cdecl Area135_QueueRun(void) {
    RunState("Area135_QueueRun", at::kArea135QueueStates, at::kArea135QueueReach);
}

// original 0x41E130 (Area135_QueueStates[0]): n = Area135_CountQueue(); the
// running object's +8 = 5; the script object's +7 = ((n << 17) - its z +
// 0xC8000) >> 15 (arithmetic); a negative step is negated and +8 ^= 4; a step
// not 0: MoveCmd_Move(the script object, +8), +0x14 = 0; the state + 1, the
// script back 2.
extern "C" void __cdecl Area135_QueueStepZ(void) {
    const U n = AH_CALL(Area135_CountQueue)() & 0xFFu;
    Sprite_Current[8] = 5;
    const std::int32_t step = Sar((n << 17) - static_cast<U>(Long(Sprite_Current + 0x38)) + 0xC8000, 15);
    MoveScript_Object[7] = static_cast<unsigned char>(step);
    unsigned char* object = MoveScript_Object;
    if (static_cast<signed char>(object[7]) < 0) {
        object[7] = static_cast<unsigned char>(0u - object[7]);
        Sprite_Current[8] = static_cast<unsigned char>(Sprite_Current[8] ^ 4);
        object = MoveScript_Object;
    }
    if (object[7] != 0) {
        AH_CALL(MoveCmd_Move)(object, Sprite_Current[8]);
        SetLong(Sprite_Current + 0x14, 0);
    }
    Sprite_Current[4] = static_cast<unsigned char>(Sprite_Current[4] + 1);
    ScriptStep(0xFFFE);
}

// original 0x41E1E0 (Area135_QueueStates[1]): n = Area135_CountQueue(); at 4
// nothing; else +8 = 3, the script object's +7 = ((Area135_QueueCells[n] <<
// 16 | 0x8000) - its x) >> 15 (n unchecked, the table stays in .data); a step
// not 0: MoveCmd_Move, +0x14 = 0; the state 0.
extern "C" void __cdecl Area135_QueueStepX(void) {
    const unsigned char n = AH_CALL(Area135_CountQueue)();
    if (n == 4) return;
    Sprite_Current[8] = 3;
    const U cell = static_cast<U>(B(at::kArea135QueueCells + n)) << 16 | 0x8000;
    MoveScript_Object[7] = static_cast<unsigned char>(Sar(cell - static_cast<U>(Long(Sprite_Current + 0x34)), 15));
    unsigned char* const object = MoveScript_Object;
    if (object[7] != 0) {
        AH_CALL(MoveCmd_Move)(object, Sprite_Current[8]);
        SetLong(Sprite_Current + 0x14, 0);
    }
    Sprite_Current[4] = 0;
}

// original 0x41E260 (Area135_Handlers[16]; PSX 0x801F4BD4): jmp through
// Area135_WalkStates by +4 (which runs on into the lift's and the spawn's).
extern "C" void __cdecl Area135_WalkRun(void) {
    RunState("Area135_WalkRun", at::kArea135WalkStates, at::kArea135WalkReach);
}

// original 0x41E280 (Area135_WalkStates[0]): +8 = 7; the script object's +7 =
// (x - 0x1C8000) >> 15; negative: +8 ^= 4 and the step negated; MoveCmd_Move,
// +0x14 = 0, the state + 1, the script back 2.
extern "C" void __cdecl Area135_WalkToX(void) {
    Sprite_Current[8] = 7;
    MoveScript_Object[7] = static_cast<unsigned char>(Sar(static_cast<U>(Long(Sprite_Current + 0x34)) - 0x1C8000, 15));
    if (static_cast<signed char>(MoveScript_Object[7]) < 0) {
        Sprite_Current[8] = static_cast<unsigned char>(Sprite_Current[8] ^ 4);
        MoveScript_Object[7] = static_cast<unsigned char>(0u - MoveScript_Object[7]);
    }
    unsigned char* const object = MoveScript_Object;
    AH_CALL(MoveCmd_Move)(object, Sprite_Current[8]);
    SetLong(Sprite_Current + 0x14, 0);
    Sprite_Current[4] = static_cast<unsigned char>(Sprite_Current[4] + 1);
    ScriptStep(0xFFFE);
}

// original 0x41E310 (Area135_WalkStates[1]): +8 = 5; the script object's +7 =
// (0x148000 - z) >> 15; MoveCmd_Move, +0x14 = 0, the state 0.
extern "C" void __cdecl Area135_WalkToZ(void) {
    Sprite_Current[8] = 5;
    MoveScript_Object[7] = static_cast<unsigned char>(Sar(0x148000u - static_cast<U>(Long(Sprite_Current + 0x38)), 15));
    unsigned char* const object = MoveScript_Object;
    AH_CALL(MoveCmd_Move)(object, Sprite_Current[8]);
    SetLong(Sprite_Current + 0x14, 0);
    Sprite_Current[4] = 0;
}

// original 0x41E370 (Area135_Handlers[17]; PSX 0x801F4D88): jmp through
// Area135_LiftStates by +4 (which runs on into the spawn's).
extern "C" void __cdecl Area135_LiftRun(void) {
    RunState("Area135_LiftRun", at::kArea135LiftStates, at::kArea135LiftReach);
}
// original 0x41E390 (Area135_LiftStates[0], [3], [4], [7]): Lift(+4).
extern "C" void __cdecl Area135_LiftUp(void) { Lift(4); }
// original 0x41E3C0 (Area135_LiftStates[1], [2], [5], [6]): Lift(-4).
extern "C" void __cdecl Area135_LiftDown(void) { Lift(0xFFFFFFFCu); }
// original 0x41E3F0 (Area135_LiftStates[8]): at x 0x168000, z 0x128000 the
// script on by 1 (the running object read again); the state 0.
extern "C" void __cdecl Area135_LiftDone(void) {
    unsigned char* cur = Sprite_Current;
    if (Long(cur + 0x34) == 0x168000 && Long(cur + 0x38) == 0x128000) {
        ScriptStep(1);
        cur = Sprite_Current;
    }
    cur[4] = 0;
}

// original 0x41E420 (Area135_Handlers[18]; PSX 0x801F4ED4): jmp through
// Area135_SpawnStates by +4.
extern "C" void __cdecl Area135_SpawnRun(void) {
    RunState("Area135_SpawnRun", at::kArea135SpawnStates, at::kArea135SpawnReach);
}

// original 0x41E440 (Area135_SpawnStates[0]): the script back 2;
// Sprite_FindFree to the word 0x903850; a slot: Area135_RestoreCells, then
// EventOp_9x(Area135_SpawnOps + 13 x the tail's sub byte) (unchecked; it
// stays in .data), the new Sprite_Current given the old one's x, z, y (read
// again for each), the old one made current again, 0x46D710, then +0xA = +9,
// +9 = 0, the state + 1.
extern "C" void __cdecl Area135_SpawnCopy(void) {
    ScriptStep(0xFFFE);
    const unsigned char slot = AH_CALL(Sprite_FindFree)();
    SetWord(Mem(at::kScratch), slot);
    if (slot == 0xFF) return;
    AH_CALL(Area135_RestoreCells)();
    unsigned char* const self = Sprite_Current;
    const unsigned sub = B(at::kTailSub);
    AH_CALL(EventOp_9x)(Mem(at::kArea135SpawnOps + sub * at::kArea135SpawnOpStride));
    SetLong(Sprite_Current + 0x34, Long(self + 0x34));
    SetLong(Sprite_Current + 0x38, Long(self + 0x38));
    SetLong(Sprite_Current + 0x3C, Long(self + 0x3C));
    Sprite_Current = self;
    AH_AT(void (__cdecl*)(), area_w3d::kEngine46D710)();
    Sprite_Current[0xA] = Sprite_Current[9];
    Sprite_Current[9] = 0;
    Sprite_Current[4] = static_cast<unsigned char>(Sprite_Current[4] + 1);
}

// original 0x41E4E0 (Area135_SpawnStates[1]): the script back 2; the count
// +0xA less 1; it was 0: +0 = 0 and the state 0; else +9 = the count, 0x46D770,
// +9 = 0.
extern "C" void __cdecl Area135_SpawnCountdown(void) {
    ScriptStep(0xFFFE);
    unsigned char* const cur = Sprite_Current;
    const unsigned char count = cur[0xA];
    cur[0xA] = static_cast<unsigned char>(count - 1);
    unsigned char* const again = Sprite_Current;
    if (count == 0) {
        again[0] = 0;
        Sprite_Current[4] = 0;
        return;
    }
    again[9] = again[0xA];
    AH_AT(void (__cdecl*)(), area_w3d::kEngine46D770)();
    Sprite_Current[9] = 0;
}

// original 0x41E530 (Area135_Handlers[19]; PSX 0x801F50B0): Sprite_Current =
// the leader; Effect_Spawn(1, 0, Area135_EffectKinds[party list 1's byte]
// (unchecked, .data), the leader's x / z words); a slot to +0xB.
extern "C" void __cdecl Area135_SpawnKind1AtLeader(void) {
    const auto z = static_cast<short>(Word(Mem(at::kLeaderZ16)));
    const auto x = static_cast<short>(Word(Mem(at::kLeaderX16)));
    const unsigned char list = B(at::kPartyList1);
    Sprite_Current = ObjTrio;
    const unsigned char kind = B(at::kArea135EffectKinds + list);
    const unsigned char slot = AH_CALL(Effect_Spawn)(1, 0, static_cast<signed char>(kind), x, z);
    if (slot != 0xFF) Sprite_Current[0xB] = slot;
}

// ===========================================================================
// The hooks, the tail and the init.
// ===========================================================================

// original 0x41E580 (the cell hook's pair for area 0x87, 0x662F5C): the first
// of Area135_CellEntries' two whose x and z bytes are the cell's and whose
// direction nibble is the leader's direction byte: ScriptFlags_Set40, tail
// kind 0x12 at the entry's state, al 1. None: al 0.
extern "C" unsigned char __cdecl Area135_CellHook(unsigned x, unsigned z) {
    const auto zb = static_cast<unsigned char>(z);
    const unsigned char direction = B(at::kLeaderDir);
    unsigned i = 0;
    for (; at::kArea135CellEntries + i * 4 + 1 < at::kArea135CellEntriesEnd; ++i) {
        const unsigned char* const e = Mem(at::kArea135CellEntries + i * 4);
        if (static_cast<unsigned char>(x) == e[0] && zb == e[1] && (e[2] & 0xF) == direction) break;
    }
    if (i == 2) return 0;
    AH_CALL(ScriptFlags_Set40)();
    B(at::kTailState) = B(at::kArea135CellEntries + i * 4 + 3);
    B(at::kTailKind) = 0x12;
    return 1;
}

// original 0x41E5E0 (Area_StepHook's case for area 135, 0x56E162): in zones 4,
// 6 and 7 with story flag 0xC set and Area135_NearMarker(x, z):
// Party_DropIn(0), al 1; else al 0.
extern "C" unsigned char __cdecl Area135_StepHook(long x, long z) {
    const unsigned char zone = Cond_ByteFD;
    if (zone != 4 && zone != 6 && zone != 7) return 0;
    if (AH_CALL(Flags_Test)(Mem(at::kStoryFlags), 0xC) == 0) return 0;
    if (AH_CALL(Area135_NearMarker)(x, z) == 0) return 0;
    AH_CALL(Party_DropIn)(0);
    return 1;
}

// original 0x41E630 (Field_ModeTailKinds[18]): the s8 state through the byte
// table 0x41EB14 (32 entries) and the jump table 0x41EAC8 (19) in the body;
// a state below 0 or above 0x1F, and 2..4, 7..9, 0xC..0x13, returns.
extern "C" void __cdecl Area135_Tail18(void) {
    static const char kWho[] = "Area135_Tail18";
    const auto state = static_cast<signed char>(B(at::kTailState));
    if (static_cast<unsigned>(static_cast<int>(state)) > 0x1F) return;
    switch (state) {
    case 0:   // the key item, message 0x81, the music stopped, stream 2 loaded
        if (Field_Request == 2) return;
        AH_CALL(KeyItem_Add)(6);
        AH_CALL(Msg_OpenScript)(0x81);
        Field_Request = 2;
        AH_CALL(Sound_StopMusic)();
        AH_CALL(Sound_LoadStream)(2);
        B(at::kTailState) = 1;
        return;
    case 1:   // the stream done and the message closed: the sound resumed, the tail disarmed
        if (AH_CALL(Sound_StreamDone)() == 0) return;
        if (Field_Request == 2) return;
        AH_CALL(Sound_ResumeAll)();
        Disarm();
        return;
    case 5:   // the party in the box: message 0x87; else flag 0x2A toggled, sound 0x203, a 10-frame wait
        if (AH_CALL(Area135_PartyInBox)() != 0) {
            AH_CALL(Msg_OpenScript)(0x87);
            Field_Request = 2;
            B(at::kTailState) = 0xB;
            return;
        }
        AH_CALL(Flags_Toggle)(Mem(at::kStoryFlags), 0x2A);
        AH_CALL(Sound_PlayEffect)(0x203);
        SetWord(Mem(at::kTailTimer), 0xA);
        B(at::kTailState) = 6;
        return;
    case 6:   // the wait
        if (Word(Mem(at::kTailTimer)) == 0) {
            Disarm();
            return;
        }
        SetWord(Mem(at::kTailTimer), Word(Mem(at::kTailTimer)) - 1u);
        return;
    case 0xA:   // flag 0xC: the count printed, message 0x84; else message 0x83 and flag 0xC set
        if (AH_CALL(Flags_Test)(Mem(at::kStoryFlags), 0xC) != 0) {
            AH_CALL(Crt_sprintf)(reinterpret_cast<char*>(Mem(at::kTextRecords)),
                                 reinterpret_cast<const char*>(Mem(at::kMessageFormat)),
                                 static_cast<unsigned>(B(at::kTailSub)));
            AH_CALL(Msg_OpenScript)(0x84);
        } else {
            AH_CALL(Msg_OpenScript)(0x83);
            AH_CALL(Flags_Set)(Mem(at::kStoryFlags), 0xC);
        }
        Field_Request = 2;
        B(at::kTailState) = 0xB;
        return;
    case 0xB:   // the message closed: disarmed
        if (Field_Request == 2) return;
        Disarm();
        return;
    case 0x14:   // the camera glides to extra object 0
        if (Field_Request == 2) return;
        AH_CALL(Area135_Kind2ToObject0)(Speed8());
        SetWord(Mem(at::kFAWord), 0);
        B(at::kTailState) = 0x15;
        return;
    case 0x15: {   // the glide done: message 0x8B, the effect's state 2, the F3 divisor
        if (B(at::kKind2Hold) != 0) return;
        AH_CALL(Area135_OpenMessage)(0x8B);
        const unsigned slot = B(at::kEffectSlot);
        const unsigned divisor = Speed8();
        B(at::kTailState) = 0x16;
        EffectAt(kWho, slot)[1] = 2;
        SetWord(Mem(at::kF3Divisor), divisor);
        return;
    }
    case 0x16:   // the button held: message 0x8C, the effect's state 3, sound 0x209, counter 3 = 0xA
        if (!(B(at::kInputHeld) & 0x20)) return;
        AH_CALL(Area135_OpenMessage)(0x8C);
        EffectAt(kWho, B(at::kEffectSlot))[1] = 3;
        AH_CALL(Sound_PlayEffect)(0x209);
        B(at::kCounter3) = 0xA;
        B(at::kTailState) = 0x17;
        return;
    case 0x17:   // counter 3 at 0xB: message 0x8D, the effect's state 2
        AH_CALL(Area135_Kind2ToObject0)(0);
        if (B(at::kCounter3) != 0xB) return;
        AH_CALL(Area135_OpenMessage)(0x8D);
        {
            const unsigned slot = B(at::kEffectSlot);
            B(at::kTailState) = 0x18;
            EffectAt(kWho, slot)[1] = 2;
        }
        return;
    case 0x18:   // a press
        if (B(at::kInputPressed) & 0x20) B(at::kTailState) = 0x19;
        return;
    case 0x19:   // the button held: message 0x8E, the effect's state 3, sound 0x209, counter 3 = 0xC
        if (!(B(at::kInputHeld) & 0x20)) return;
        AH_CALL(Area135_OpenMessage)(0x8E);
        EffectAt(kWho, B(at::kEffectSlot))[1] = 3;
        AH_CALL(Sound_PlayEffect)(0x209);
        B(at::kCounter3) = 0xC;
        B(at::kTailState) = 0x1A;
        return;
    case 0x1A:   // counter 3 at 0xD: the effect's state 4
        AH_CALL(Area135_Kind2ToObject0)(0);
        if (B(at::kCounter3) != 0xD) return;
        B(at::kTailState) = 0x1B;
        EffectAt(kWho, B(at::kEffectSlot))[1] = 4;
        return;
    case 0x1B: {   // counter 3 at 0x10: the object in the sub byte set going (none: message 0x8F)
        AH_CALL(Area135_Kind2ToObject0)(0);
        if (B(at::kCounter3) != 0x10) return;
        const unsigned char sub = B(at::kTailSub);
        if (sub == 0xFF) {
            AH_CALL(Area135_OpenMessage)(0x8F);
            B(at::kTailState) = 0x1E;
            return;
        }
        B(at::kTailState) = 0x1C;
        unsigned char* const o = ObjectAt(kWho, sub);
        o[1] = 7;
        SetLong(o + 0x18, 0);
        SetLong(o + 0x1C, 5);
        AddWord(o + 0x8A, 3);
        return;
    }
    case 0x1C:   // counter 3 at 0x11: the map flags' bit 4 set
        if (B(at::kCounter3) != 0x11) return;
        B(at::kTailState) = 0x1D;
        B(at::kArea135MapFlags) = static_cast<unsigned char>(B(at::kArea135MapFlags) | 0x10);
        return;
    case 0x1D: {   // counter 3 at 0x12: the object stopped, the map flags' bit 4 cleared, message 0x8F
        AH_CALL(Area135_Kind2ToObject0)(0);
        if (B(at::kCounter3) != 0x12) return;
        const unsigned char flags = B(at::kArea135MapFlags);
        unsigned char* const o = ObjectAt(kWho, B(at::kTailSub));
        o[1] = 4;
        o[0x24] = static_cast<unsigned char>(o[0x24] & 0xDF);
        AddWord(o + 0x8A, 1);
        B(at::kArea135MapFlags) = static_cast<unsigned char>(flags & 0xEF);
        AH_CALL(Area135_OpenMessage)(0x8F);
        B(at::kTailState) = 0x1E;
        return;
    }
    case 0x1E: {   // counter 3 at 0x15: the camera to the leader, the effect's state 5
        AH_CALL(Area135_Kind2ToObject0)(0);
        if (B(at::kCounter3) != 0x15) return;
        const U x = D(at::kLeaderX);
        const U z = D(at::kLeaderZ);
        const unsigned divisor = Speed8();
        SetD(at::kKind2X, x);
        const unsigned slot = B(at::kEffectSlot);
        SetD(at::kKind2Z, z);
        SetWord(Mem(at::kF3Divisor), divisor);
        B(at::kTailState) = 0x1F;
        EffectAt(kWho, slot)[1] = 5;
        return;
    }
    case 0x1F:   // the glide done: disarmed, counter 3 and the sub byte 0
        if (B(at::kKind2Hold) != 0) return;
        AH_CALL(ScriptFlags_Clear40)();
        B(at::kCounter3) = 0;
        B(at::kTailKind) = 0;
        B(at::kTailState) = 0;
        B(at::kTailSub) = 0;
        return;
    default:   // 2..4, 7..9, 0xC..0x13
        return;
    }
}

// original 0x41EB40 (Area_Descriptors[135] +0x40; PSX 0x801F5A44): zone 4
// (Cond_ByteFD): entered from zone 1 (0x905E68): EventOp_Bx(Area135_InitOp),
// story flag 0xC cleared, the zone read again, the tail's sub byte 0; then
// the byte 0x8034B0 |= 1. Zone (as last read) 4, 6 or 7: the marker's +0 = 1.
// Zone 0: Effect_FindFree; a slot: +0 = 1, kind +5 = 0x53, dwords +0x18 = 0,
// +0x1C = 6; counter 3 = 0.
extern "C" void __cdecl Area135_Init(void) {
    unsigned char zone = Cond_ByteFD;
    if (zone == 4) {
        if (B(at::kEntryZone) == 1) {
            AH_CALL(EventOp_Bx)(Mem(at::kArea135InitOp));
            AH_CALL(Flags_Clear)(Mem(at::kStoryFlags), 0xC);
            zone = Cond_ByteFD;
            B(at::kTailSub) = 0;
        }
        B(at::kInitBit) = static_cast<unsigned char>(B(at::kInitBit) | 1);
    }
    if (zone == 4 || zone == 6 || zone == 7) B(at::kMarker) = 1;
    if (zone != 0) return;
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    if (slot != 0xFF) {
        unsigned char* const e = EffectAt("Area135_Init", slot);
        e[0] = 1;
        e[5] = 0x53;
        SetLong(e + 0x18, 0);
        SetLong(e + 0x1C, 6);
    }
    B(at::kCounter3) = 0;
}

// ===========================================================================
// The helpers.
// ===========================================================================

// original 0x41EBE0: d = (the height word - the running object's word +0x3E)
// as s16; its +9 = (|d / 128| << 4) as a byte (C's truncating division); +0x14
// = d / +9 (idiv; +9 read again); dwords +0x10 and +0xC = 0. A +9 of 0 (|d|
// below 128, or |d / 128| a multiple of 16) faults the original's idiv: ours
// aborts.
extern "C" void __cdecl Area135_FallTo(unsigned height) {
    unsigned char* cur = Sprite_Current;
    const auto d = static_cast<std::int32_t>(static_cast<short>(static_cast<unsigned short>(height - Word(cur + 0x3E))));
    std::int32_t steps = d / 128;
    if (steps < 0) steps = -steps;
    cur[9] = static_cast<unsigned char>(static_cast<unsigned>(steps) << 4);
    cur = Sprite_Current;
    const std::int32_t divisor = cur[9];
    if (divisor == 0)
        bof3::Fatal("Area135_FallTo: the fall's frame count +9 is 0 (height %u against +0x3E %u, a difference of %d): "
                    "the original divides by it and faults",
                    height & 0xFFFF, static_cast<unsigned>(Word(cur + 0x3E)), static_cast<int>(d));
    SetLong(cur + 0x14, d / divisor);
    SetLong(Sprite_Current + 0x10, 0);
    SetLong(Sprite_Current + 0xC, 0);
}

// original 0x41EC40: the first field object (0..29) in use (+0 bit 0), not
// the running object (read at entry), of kind +6 = 0xA, at exactly (x, z);
// al its index, else 0xFF.
extern "C" unsigned char __cdecl Area135_ObjectAtCell(long x, long z) {
    const unsigned char* const self = Sprite_Current;
    for (unsigned i = 0; i < at::kObjectCount; ++i) {
        const unsigned char* const o = Mem(at::kSpriteObjects + i * at::kObjectStride);
        if (!(o[0] & 1)) continue;
        if (o == self) continue;
        if (o[6] != 0xA) continue;
        if (Long(o + 0x34) != x) continue;
        if (Long(o + 0x38) != z) continue;
        return static_cast<unsigned char>(i);
    }
    return 0xFF;
}

// original 0x41ECB0: how many of the queue's four cells (x 0x208000, z
// 0xC8000 + n << 17) hold an object (Area135_ObjectAtCell), counted until the
// first empty one: al 0..4.
extern "C" unsigned char __cdecl Area135_CountQueue(void) {
    unsigned char n = 0;
    for (; n < 4; ++n)
        if (AH_CALL(Area135_ObjectAtCell)(0x208000, static_cast<long>((static_cast<U>(n) << 17) + 0xC8000)) == 0xFF) break;
    return n;
}

// original 0x41ECF0: Field_Request 5: the script back 2. Else the 2 x 2 cells
// under the running object (x - 0x8000, z - 0x8000 as cells; +1 in each):
// its dword +0x20 = 0, then for each cell AreaMap_SetHeight(cell, 0x10), its
// AreaMap_ByteAt or'd into +0x20 (the running object read again before each;
// bytes 0, 2 for row z, 1, 3 for row z + 1), AreaMap_SetByte(cell, 0x11).
extern "C" void __cdecl Area135_StampCells(void) {
    if (Field_Request == 5) {
        ScriptStep(0xFFFE);
        return;
    }
    unsigned char* const cur = Sprite_Current;
    const U x = static_cast<U>(Long(cur + 0x34)) - 0x8000;
    const U z = static_cast<U>(Long(cur + 0x38)) - 0x8000;
    const unsigned cx = (x >> 16) & 0xFFFF;
    SetLong(cur + 0x20, 0);
    for (unsigned k = 0; k < 2; ++k) {
        const unsigned cz = ((z >> 16) + k) & 0xFFFF;
        const unsigned shift = k * 8;
        AH_CALL(AreaMap_SetHeight)(cx, cz, 0x10);
        unsigned char* p = Sprite_Current + 0x20;
        U v = AH_CALL(AreaMap_ByteAt)(static_cast<short>(cx), static_cast<short>(cz)) & 0xFFu;
        SetLong(p, static_cast<std::int32_t>(static_cast<U>(Long(p)) | v << shift));
        AH_CALL(AreaMap_SetByte)(cx, cz, 0x11);
        const unsigned cx1 = (cx + 1) & 0xFFFF;
        AH_CALL(AreaMap_SetHeight)(cx1, cz, 0x10);
        p = Sprite_Current + 0x20;
        v = AH_CALL(AreaMap_ByteAt)(static_cast<short>(cx1), static_cast<short>(cz)) & 0xFFu;
        SetLong(p, static_cast<std::int32_t>(static_cast<U>(Long(p)) | v << (shift + 16)));
        AH_CALL(AreaMap_SetByte)(cx1, cz, 0x11);
    }
}

// original 0x41EDE0: Area135_StampCells undone at the running object's own
// cells (x, z; +1 in each, no half-cell offset): for each cell
// AreaMap_SetHeight(cell, 0), AreaMap_SetByte(cell, +0x20 >> 0 / 8 / 16 / 24,
// arithmetic, the running object read again).
extern "C" void __cdecl Area135_RestoreCells(void) {
    unsigned char* const cur = Sprite_Current;
    const auto x = static_cast<U>(Long(cur + 0x34));
    const auto z = static_cast<U>(Long(cur + 0x38));
    const unsigned cx = (x >> 16) & 0xFFFF;
    for (unsigned k = 0; k < 2; ++k) {
        const unsigned cz = ((z >> 16) + k) & 0xFFFF;
        const unsigned shift = k * 8;
        AH_CALL(AreaMap_SetHeight)(cx, cz, 0);
        AH_CALL(AreaMap_SetByte)(cx, cz, static_cast<unsigned>(Long(Sprite_Current + 0x20) >> shift));
        const unsigned cx1 = (cx + 1) & 0xFFFF;
        AH_CALL(AreaMap_SetHeight)(cx1, cz, 0);
        AH_CALL(AreaMap_SetByte)(cx1, cz, static_cast<unsigned>(Long(Sprite_Current + 0x20) >> (shift + 16)));
    }
}

// original 0x41EE70: (x, z) within r = (the running object's +0x70 byte + the
// marker's + 2) << 15 of the marker's position less its step (+0xC, +0x10)
// times its +9 on both axes (|d| < r, signed, |INT_MIN| staying negative):
// al 1, else 0.
extern "C" unsigned char __cdecl Area135_NearMarker(long x, long z) {
    const U marker = static_cast<U>(Long(Mem(at::kMarker70))) & 0xFF;
    const U self = static_cast<U>(Long(Sprite_Current + 0x70)) & 0xFF;
    const U frames = B(at::kMarker9);
    const auto reach = static_cast<std::int32_t>((self + marker + 2) << 15);
    auto dx = static_cast<std::int32_t>(static_cast<U>(x) - frames * D(at::kMarkerStepX) - D(at::kMarkerX));
    const U sx = dx < 0 ? 0xFFFFFFFFu : 0u;
    dx = static_cast<std::int32_t>((static_cast<U>(dx) ^ sx) - sx);
    if (dx >= reach) return 0;
    auto dz = static_cast<std::int32_t>(static_cast<U>(z) - frames * D(at::kMarkerStepZ) - D(at::kMarkerZ));
    const U sz = dz < 0 ? 0xFFFFFFFFu : 0u;
    dz = static_cast<std::int32_t>((static_cast<U>(dz) ^ sz) - sz);
    if (dz >= reach) return 0;
    return 1;
}

// original 0x41EEF0: a party record 0 .. Field_MemberCount - 1 (unchecked
// past the three: it reads on) whose cell x word +0x36 is 0x2C or 0x2D and
// cell z word +0x3A 0x30..0x37 (signed): al 1, else 0.
extern "C" unsigned char __cdecl Area135_PartyInBox(void) {
    const unsigned char count = Field_MemberCount;
    for (unsigned char i = 0; i < count; ++i) {
        const auto cx = static_cast<short>(Word(Mem(at::kPartyCellX + i * at::kPartyStride)));
        if (cx != 0x2C && cx != 0x2D) continue;
        const auto cz = static_cast<short>(Word(Mem(at::kPartyCellZ + i * at::kPartyStride)));
        if (cz > 0x2F && cz < 0x38) return 1;
    }
    return 0;
}

// original 0x41EF60: Field_Kind2X / Z = Sprite_ObjectsExtra[0]'s x / z; a
// divisor word not 0 also the F3 divisor.
extern "C" void __cdecl Area135_Kind2ToObject0(unsigned divisor) {
    const auto word = static_cast<unsigned short>(divisor);
    if (word != 0) {
        const U z = D(at::kExtra0Z);
        SetWord(Mem(at::kF3Divisor), word);
        const U x = D(at::kExtra0X);
        SetD(at::kKind2Z, z);
        SetD(at::kKind2X, x);
        return;
    }
    SetD(at::kKind2X, D(at::kExtra0X));
    SetD(at::kKind2Z, D(at::kExtra0Z));
}

// original 0x41EFA0: Msg_OpenScript(the id byte); the window pool pointer
// 0x905B84 = MessagePools; Window_FreeCurrent; MessagePools' words +4 / +6 =
// 0x3C0 / 0xAA0.
extern "C" void __cdecl Area135_OpenMessage(unsigned id) {
    AH_CALL(Msg_OpenScript)(static_cast<unsigned short>(id & 0xFF));
    SetD(at::kWindowPoolPtr, at::kMessagePools);
    AH_CALL(Window_FreeCurrent)();
    SetWord(Mem(at::kPoolWords), 0x3C0);
    SetWord(Mem(at::kPoolWords + 2), 0xAA0);
}

void AreaW3d_Inject() {
    if (bof3::WantsShadow("area_w3d")) area_w3d::SelfTest();
    BOF3_INJECT(Area135_ChoiceStartTail);
    BOF3_INJECT(Area135_ChoiceCount);
    BOF3_INJECT(Area135_ChoiceStartTail14);
    BOF3_INJECT(Area135_ChoiceFlag37);
    BOF3_INJECT(Area135_ClearFlagC);
    BOF3_INJECT(Area135_RiseInZone4);
    BOF3_INJECT(Area135_RiseElsewhere);
    BOF3_INJECT(Area135_LeaveByExit);
    BOF3_INJECT(Area135_MarkerRoute);
    BOF3_INJECT(Area135_ToMarker);
    BOF3_INJECT(Area135_FaceLeaderDir);
    BOF3_INJECT(Area135_HeldMove1);
    BOF3_INJECT(Area135_HeldMove7);
    BOF3_INJECT(Area135_FallToFloor);
    BOF3_INJECT(Area135_Fall780);
    BOF3_INJECT(Area135_StampCellsRun);
    BOF3_INJECT(Area135_RestoreCellsRun);
    BOF3_INJECT(Area135_RestoreOnRequest5);
    BOF3_INJECT(Area135_JumpByOccupied);
    BOF3_INJECT(Area135_QueueRun);
    BOF3_INJECT(Area135_QueueStepZ);
    BOF3_INJECT(Area135_QueueStepX);
    BOF3_INJECT(Area135_WalkRun);
    BOF3_INJECT(Area135_WalkToX);
    BOF3_INJECT(Area135_WalkToZ);
    BOF3_INJECT(Area135_LiftRun);
    BOF3_INJECT(Area135_LiftUp);
    BOF3_INJECT(Area135_LiftDown);
    BOF3_INJECT(Area135_LiftDone);
    BOF3_INJECT(Area135_SpawnRun);
    BOF3_INJECT(Area135_SpawnCopy);
    BOF3_INJECT(Area135_SpawnCountdown);
    BOF3_INJECT(Area135_SpawnKind1AtLeader);
    BOF3_INJECT(Area135_CellHook);
    BOF3_INJECT(Area135_StepHook);
    BOF3_INJECT(Area135_Tail18);
    BOF3_INJECT(Area135_Init);
    BOF3_INJECT(Area135_FallTo);
    BOF3_INJECT(Area135_ObjectAtCell);
    BOF3_INJECT(Area135_CountQueue);
    BOF3_INJECT(Area135_StampCells);
    BOF3_INJECT(Area135_RestoreCells);
    BOF3_INJECT(Area135_NearMarker);
    BOF3_INJECT(Area135_PartyInBox);
    BOF3_INJECT(Area135_Kind2ToObject0);
    BOF3_INJECT(Area135_OpenMessage);
}

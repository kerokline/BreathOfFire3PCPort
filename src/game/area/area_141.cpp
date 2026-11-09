// Moved here by tools/move_funcs.py from area_w3e.cpp (docs/source-layout-round.md).
#include "game/area/area_141.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w3e_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w3e::at;

}  // namespace

namespace {

using area_harness::Handler;

using area_harness::Mem;

using move_script::Long;

using move_script::SetWord;

using move_script::Word;

using U = std::uint32_t;

unsigned char& B(U address) { return *Mem(address); }

void AddWord(unsigned char* p, int v) { SetWord(p, static_cast<unsigned>(Word(p) + v)); }

unsigned char* ObjectAt(unsigned slot) { return rv::Bytes(Sprite_Objects) + slot * at::kObjectStride; }

void SetMessage(unsigned id) { SetWord(Mem(::bof3::Img(at::kMessage.v)), id); }

unsigned char* StoryFlags() { return Mem(::bof3::Img(Cond_Flags_addr) + 0xa0); }

// The movement script's position (MoveScript_Object's word +0xA) moved by `v`.
void ScriptStep(int v) { AddWord(MoveScript_Object + 0xA, v); }

// A state handler read from an area's .data table in place, as the
// originals' `jmp [index * 4 + table]`: the index is not checked there. Ours
// aborts past the table (what follows is data or the next table).
Handler StateEntry(const char* who, U table, unsigned count, unsigned index) {
    if (index >= count)
        bof3::Fatal("%s: state %u is past its %u-entry table 0x%X", who, index, count, static_cast<unsigned>(table));
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(table + index * 4u)))));
}

// The first of `count` entries of `stride` bytes at `table` whose x byte is
// x's low byte, whose z byte is z's, and whose direction (the third byte's low
// nibble) is the leader's direction byte (the whole byte: 0x10 or more matches
// none); `count` when none - the search the three cell hooks open with.
unsigned FindCell(U table, unsigned count, unsigned stride, long x, long z) {
    const unsigned char pose = B(::bof3::Img(ObjTrio_addr) + 0x8);
    for (unsigned i = 0; i < count; ++i) {
        const unsigned char* const e = Mem(table + i * stride);
        if (e[0] == static_cast<unsigned char>(x) && e[1] == static_cast<unsigned char>(z) && (e[2] & 0xF) == pose) return i;
    }
    return count;
}

// Area 141's two object shades: Sprite_Current's position moved by its step
// (+0xC to x, +0x10 to z, dwords), then the three shade bytes +0x5D..+0x5F.
void DriftStep() {
    unsigned char* cur = Sprite_Current;
    auto* tCur = rv::As<records::SpriteObject>(cur);
    tCur->x = static_cast<std::int32_t>(static_cast<U>(tCur->x) + static_cast<U>(tCur->step_x));
    cur = Sprite_Current;
    tCur = rv::As<records::SpriteObject>(cur);
    tCur->z = static_cast<std::int32_t>(static_cast<U>(tCur->z) + static_cast<U>(tCur->step_z));
}

}  // namespace

// Area 141's init and tail: the two runs `a`, `a + 1` painted on (flag clear)
// or off (flag set) - the init's loops of two.
static void PaintPairByFlag(unsigned flag, unsigned a) {
    const unsigned char on = AH_CALL(Flags_Test)(StoryFlags(), flag) == 0 ? 1 : 0;
    AH_CALL(Area141_PaintCells)(Mem(at::Run(a)), on);
    AH_CALL(Area141_PaintCells)(Mem(at::Run(a + 1)), on);
}

// Tail 52's three flag states (0x1E, 0x28 and the choice's 0xE for 0x7A read
// apart): the flag set already: state 0x14; else set, runs `a`, `a + 1` off,
// the timer 0x3C, state `next`.
static void FlagRunsOff(unsigned flag, unsigned a, unsigned char next) {
    if (AH_CALL(Flags_Test)(StoryFlags(), flag) != 0) {
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0x14;
        return;
    }
    AH_CALL(Flags_Set)(StoryFlags(), flag);
    AH_CALL(Area141_PaintCells)(Mem(at::Run(a)), 0);
    AH_CALL(Area141_PaintCells)(Mem(at::Run(a + 1)), 0);
    SetWord(Mem(::bof3::Img(at::kTailTimer.v)), 0x3C);
    B(::bof3::Img(AreaW0c_TailState_addr)) = next;
}

// The word timer less 1; true when it reaches 0 (16-bit).
static bool TimerOut() {
    const auto t = static_cast<std::uint16_t>(Word(Mem(::bof3::Img(at::kTailTimer.v))) - 1);
    SetWord(Mem(::bof3::Img(at::kTailTimer.v)), t);
    return t == 0;
}

// Area 141's placers: a free object (Sprite_FindFree) marked live (+0 = 1),
// the event ops' object word 0x903850 = its index, the event op on `script`.
static void PlaceOn(unsigned char slot, U script, bool op6x) {
    ObjectAt(slot)[0] = 1;
    SetWord(Mem(::bof3::Img(at::kScratch850.v)), slot);
    if (op6x)
        AH_CALL(EventOp_6x)(Mem(script));
    else
        AH_CALL(EventOp_0x)(Mem(script));
}

// Two free objects, or none: the first is marked live before the second is
// looked for, and let go (+0 = 0) when there is no second. Then the first's
// word 0x903850 is set before the second is marked live.
static bool TwoFree(unsigned char& a, unsigned char& b) {
    a = AH_CALL(Sprite_FindFree)();
    if (a == 0xFF) return false;
    ObjectAt(a)[0] = 1;
    b = AH_CALL(Sprite_FindFree)();
    if (b == 0xFF) {
        ObjectAt(a)[0] = 0;
        return false;
    }
    return true;
}

// The pair's placement as the originals order it: 0x903850 = a, b live, the
// op on `first` (for a); [animation]; 0x903850 = b, the op on `second`.
static void PlacePair(unsigned char a, unsigned char b, U first, U second, bool op6x, int anim_a, int anim_b) {
    SetWord(Mem(::bof3::Img(at::kScratch850.v)), a);
    ObjectAt(b)[0] = 1;
    if (op6x)
        AH_CALL(EventOp_6x)(Mem(first));
    else
        AH_CALL(EventOp_0x)(Mem(first));
    if (anim_a >= 0) AH_CALL(Sprite_SetAnimationAt)(static_cast<unsigned char>(anim_a), 0);
    SetWord(Mem(::bof3::Img(at::kScratch850.v)), b);
    if (op6x)
        AH_CALL(EventOp_6x)(Mem(second));
    else
        AH_CALL(EventOp_0x)(Mem(second));
    if (anim_b >= 0) AH_CALL(Sprite_SetAnimationAt)(static_cast<unsigned char>(anim_b), 0);
}

// original 0x41FC10 (area 141 +0x34[2] = +0x3C[0]; PSX 0x801F3588):
// Sprite_Current's +0 bit 0x20 cleared, +0x5F..+0x5C = 0 (the pointer read
// again for each), Sprite_ReleaseTint(it).
extern "C" void __cdecl Area141_ShadeOff(void) {
    Sprite_Current[0] = static_cast<unsigned char>(Sprite_Current[0] & 0xDF);
    Sprite_Current[0x5F] = 0;
    Sprite_Current[0x5E] = 0;
    Sprite_Current[0x5D] = 0;
    Sprite_Current[0x5C] = 0;
    AH_CALL(Sprite_ReleaseTint)(Sprite_Current);
}

// original 0x41FC50 (area 141 +0x34[3] = +0x3C[1]; PSX 0x801F35E4): d =
// (Sprite_Current's x - 0x450000) >> 15 (arithmetic); 0: nothing. Else the
// script object's +7 = |d| (a byte), Sprite_Current's +8 = 7, and the move
// in direction +8: MoveCmd_MoveKind2 when Sprite_Current is Sprite_Kind2,
// else MoveCmd_Move(the script object, it).
extern "C" void __cdecl Area141_MoveToX45(void) {
    const std::int32_t d = static_cast<std::int32_t>(static_cast<U>(rv::Current<records::SpriteObject>()->x) - 0x450000u) >> 15;
    if (d == 0) return;
    MoveScript_Object[7] = static_cast<unsigned char>(d < 0 ? -d : d);
    Sprite_Current[8] = 7;
    unsigned char* const cur = Sprite_Current;
    if (cur == rv::Bytes(&Sprite_Kind2)) {
        AH_CALL(MoveCmd_MoveKind2)(cur[8]);
        return;
    }
    AH_CALL(MoveCmd_Move)(MoveScript_Object, cur[8]);
}

// original 0x41FCB0 (area 141 +0x34[4] = +0x3C[2]; PSX 0x801F3688):
// Area141_ShadeDownStates by Sprite_Current[4]. Unchecked there: indices 2
// and 3 read Area141_ShadeUpStates, which follows, and run its states (kept);
// past those, data (ours aborts).
extern "C" void __cdecl Area141_RunShadeDown(void) {
    StateEntry("Area141_RunShadeDown", ::bof3::Img(Area141_ShadeDownStates_addr), at::kArea141ShadeDownReach, Sprite_Current[4])();
}

// original 0x41FCD0 (Area141_ShadeDownStates[0]): +0 bit 0x20, +0x5F..+0x5D =
// 0xC4, +0x5C = 1; Area141_ShadeDownStep; +4 = 1.
extern "C" void __cdecl Area141_ShadeDownStart(void) {
    Sprite_Current[0] = static_cast<unsigned char>(Sprite_Current[0] | 0x20);
    Sprite_Current[0x5F] = 0xC4;
    Sprite_Current[0x5E] = 0xC4;
    Sprite_Current[0x5D] = 0xC4;
    Sprite_Current[0x5C] = 1;
    AH_CALL(Area141_ShadeDownStep)();
    Sprite_Current[4] = 1;
}

// original 0x41FD20 (Area141_ShadeDownStates[1]): DriftStep; each of
// +0x5D..+0x5F not 0x80 less 2; all three at 0x80: +0 bit 0x40, +4 = 0; else
// the script position - 2.
extern "C" void __cdecl Area141_ShadeDownStep(void) {
    DriftStep();
    unsigned char* const cur = Sprite_Current;
    for (unsigned k = 0x5D; k <= 0x5F; ++k)
        if (cur[k] != 0x80) cur[k] = static_cast<unsigned char>(cur[k] - 2);
    if (cur[0x5D] == 0x80 && cur[0x5E] == 0x80 && cur[0x5F] == 0x80) {
        cur[0] = static_cast<unsigned char>(cur[0] | 0x40);
        cur[4] = 0;
        return;
    }
    ScriptStep(-2);
}

// original 0x41FDB0 (area 141 +0x34[5] = +0x3C[3]; PSX 0x801F3838):
// Area141_ShadeUpStates by Sprite_Current[4].
extern "C" void __cdecl Area141_RunShadeUp(void) {
    StateEntry("Area141_RunShadeUp", ::bof3::Img(Area141_ShadeUpStates_addr), at::kArea141StateCount, Sprite_Current[4])();
}

// original 0x41FDD0 (Area141_ShadeUpStates[0]): +0 bit 0x20 set, bit 0x40
// cleared, +0x5C = 1, +0x5F..+0x5D = 0x80; Area141_ShadeUpStep; +4 = 1.
extern "C" void __cdecl Area141_ShadeUpStart(void) {
    Sprite_Current[0] = static_cast<unsigned char>(Sprite_Current[0] | 0x20);
    Sprite_Current[0] = static_cast<unsigned char>(Sprite_Current[0] & 0xBF);
    Sprite_Current[0x5C] = 1;
    Sprite_Current[0x5F] = 0x80;
    Sprite_Current[0x5E] = 0x80;
    Sprite_Current[0x5D] = 0x80;
    AH_CALL(Area141_ShadeUpStep)();
    Sprite_Current[4] = 1;
}

// original 0x41FE20 (Area141_ShadeUpStates[1]): DriftStep; each of
// +0x5D..+0x5F below 0xC0 (signed bytes: below -64) plus 4; all three at 0xC0:
// +0 bit 0x20 cleared, +0x5C, +0x5F..+0x5D and +4 = 0; else the active
// member's word +0x8A - 2.
extern "C" void __cdecl Area141_ShadeUpStep(void) {
    DriftStep();
    unsigned char* const cur = Sprite_Current;
    for (unsigned k = 0x5D; k <= 0x5F; ++k)
        if (static_cast<signed char>(cur[k]) < static_cast<signed char>(0xC0)) cur[k] = static_cast<unsigned char>(cur[k] + 4);
    if (cur[0x5D] == 0xC0 && cur[0x5E] == 0xC0 && cur[0x5F] == 0xC0) {
        cur[0] = static_cast<unsigned char>(cur[0] & 0xDF);
        cur[0x5C] = 0;
        cur[0x5F] = 0;
        cur[0x5E] = 0;
        cur[0x5D] = 0;
        cur[4] = 0;
        return;
    }
    AddWord(Field_ActiveMember + 0x8A, -2);
}

// original 0x41FEE0 (area 141 +0x34[0]): story flag 0x7D; then the answer
// (read after the call) 0: ScriptFlags_Set40, Kind2_Place(3),
// Area141_PaintCells(run 8, off), tail kind 52 at state 0x32, message 5;
// else message 6.
extern "C" void __cdecl Area141_ChoiceFlag7D(void) {
    AH_CALL(Flags_Set)(StoryFlags(), 0x7D);
    if (B(::bof3::Img(AreaW0c_ChoiceAnswer_addr)) != 0) {
        SetMessage(6);
        return;
    }
    AH_CALL(ScriptFlags_Set40)();
    AH_CALL(Kind2_Place)(3);
    AH_CALL(Area141_PaintCells)(Mem(at::Run(8)), 0);
    B(::bof3::Img(AreaW0a_TailKind_addr)) = 0x34;
    B(::bof3::Img(AreaW0c_TailState_addr)) = 0x32;
    SetMessage(5);
}

// original 0x41FF40 (area 141 +0x40; PSX 0x801F3B04): Cond_ByteFD 0: runs 0
// and 1 on. Cond_ByteFD (read again) 1: runs 2, 3 by story flag 0x7A, runs 4,
// 5 by 0x7B, runs 6, 7 by 0x7C (on while the flag is clear). Cond_ByteFD
// (again) 2: run 8 on.
extern "C" void __cdecl Area141_InitCells(void) {
    if (Cond_ByteFD == 0) {
        AH_CALL(Area141_PaintCells)(Mem(at::Run(0)), 1);
        AH_CALL(Area141_PaintCells)(Mem(at::Run(1)), 1);
    }
    if (Cond_ByteFD == 1) {
        PaintPairByFlag(0x7A, 2);
        PaintPairByFlag(0x7B, 4);
        PaintPairByFlag(0x7C, 6);
    }
    if (Cond_ByteFD == 2) AH_CALL(Area141_PaintCells)(Mem(at::Run(8)), 1);
}

// original 0x420060 (Field_ModeTailKinds 52): by the signed state (0..0x33;
// any other nothing), through the byte map 0x420328 and the fifteen-entry jump
// table 0x4202EC inside the function:
//   0     Field_Request not 2: ScriptFlags_Clear40, Party_DropIn(5), kind and
//         state 0;
//   0xA   Msg_OpenScript(3), Field_ScriptFlags' low byte | 7, Field_Request
//         2, the timer 0, state 0xB;
//   0xB   Field_Request not 2: state 0xC;
//   0xC   the timer + 1 (16-bit); at 0x1C2 or more state 0xD; else a button
//         pressed (Input_Pressed not 0): state 0x14;
//   0xD   Field_Request not 2: Msg_OpenScript(4), Field_Request 2,
//         Kind2_Place(2), state 0xE;
//   0xE   counter 0 at 1: story flag 0x7A, runs 2, 3 off, the timer 0x3C,
//         state 0xF;
//   0xF   the timer - 1 at 0: Field_Kind2X / Z = the leader's x / z, state
//         0x10;
//   0x10  Field_Kind2Hold 0: the message box's flag byte | 0x80, state 0x14;
//   0x14  ScriptFlags_Clear40, Field_ScriptFlags & 0xFFF8, kind and state 0;
//   0x1E  FlagRunsOff(0x7B, runs 4, 5, state 0x1F);
//   0x1F, 0x29  the timer - 1 at 0: state 0x14;
//   0x28  FlagRunsOff(0x7C, runs 6, 7, state 0x29);
//   0x32  counter 0 at 1: Cond_ByteFE 1, the timer 0x3C, state 0x33;
//   0x33  the timer - 1 at 0: kind and state 0, counter 0 + 1;
//   the rest nothing.
extern "C" void __cdecl Area141_Tail52(void) {
    const auto state = static_cast<signed char>(B(::bof3::Img(AreaW0c_TailState_addr)));
    switch (state) {
    case 0:
        if (Field_Request == 2) return;
        AH_CALL(ScriptFlags_Clear40)();
        AH_CALL(Party_DropIn)(5);
        B(::bof3::Img(AreaW0a_TailKind_addr)) = 0;
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0;
        return;
    case 0xA:
        AH_CALL(Msg_OpenScript)(3);
        B(::bof3::Img(Field_ScriptFlags_addr)) = static_cast<unsigned char>(B(::bof3::Img(Field_ScriptFlags_addr)) | 7);
        Field_Request = 2;
        SetWord(Mem(::bof3::Img(at::kTailTimer.v)), 0);
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0xB;
        return;
    case 0xB:
        if (Field_Request == 2) return;
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0xC;
        return;
    case 0xC: {
        const auto t = static_cast<std::uint16_t>(Word(Mem(::bof3::Img(at::kTailTimer.v))) + 1);
        SetWord(Mem(::bof3::Img(at::kTailTimer.v)), t);
        if (t >= 0x1C2) {
            B(::bof3::Img(AreaW0c_TailState_addr)) = 0xD;
            return;
        }
        if (Input_Pressed != 0) B(::bof3::Img(AreaW0c_TailState_addr)) = 0x14;
        return;
    }
    case 0xD:
        if (Field_Request == 2) return;
        AH_CALL(Msg_OpenScript)(4);
        Field_Request = 2;
        AH_CALL(Kind2_Place)(2);
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0xE;
        return;
    case 0xE:
        if (B(::bof3::Img(at::kCounter0.v)) != 1) return;
        AH_CALL(Flags_Set)(StoryFlags(), 0x7A);
        AH_CALL(Area141_PaintCells)(Mem(at::Run(2)), 0);
        AH_CALL(Area141_PaintCells)(Mem(at::Run(3)), 0);
        SetWord(Mem(::bof3::Img(at::kTailTimer.v)), 0x3C);
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0xF;
        return;
    case 0xF:
        if (!TimerOut()) return;
        Field_Kind2X = Long(Mem(::bof3::Img(ObjTrio_addr) + 0x34));
        Field_Kind2Z = Long(Mem(::bof3::Img(ObjTrio_addr) + 0x38));
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0x10;
        return;
    case 0x10:
        if (Field_Kind2Hold != 0) return;
        B(::bof3::Img(at::kMsgFlags.v)) = static_cast<unsigned char>(B(::bof3::Img(at::kMsgFlags.v)) | 0x80);
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0x14;
        return;
    case 0x14:
        AH_CALL(ScriptFlags_Clear40)();
        Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags & 0xFFF8);
        B(::bof3::Img(AreaW0a_TailKind_addr)) = 0;
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0;
        return;
    case 0x1E: FlagRunsOff(0x7B, 4, 0x1F); return;
    case 0x1F: case 0x29:
        if (TimerOut()) B(::bof3::Img(AreaW0c_TailState_addr)) = 0x14;
        return;
    case 0x28: FlagRunsOff(0x7C, 6, 0x29); return;
    case 0x32:
        if (B(::bof3::Img(at::kCounter0.v)) != 1) return;
        Cond_ByteFE = 1;
        SetWord(Mem(::bof3::Img(at::kTailTimer.v)), 0x3C);
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0x33;
        return;
    case 0x33: {
        if (!TimerOut()) return;
        const unsigned char c = B(::bof3::Img(at::kCounter0.v));
        B(::bof3::Img(AreaW0a_TailKind_addr)) = 0;
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0;
        B(::bof3::Img(at::kCounter0.v)) = static_cast<unsigned char>(c + 1);
        return;
    }
    default: return;   // the byte map's entry 14 (a ret), and past 0x33
    }
}

// original 0x420360 (Area_CellHooks, area 0x8D): FindCell over
// Area141_CellEntries; a match: ScriptFlags_Set40, tail kind 52 at the
// entry's state (+3), al 1; none: al 0.
extern "C" unsigned char __cdecl Area141_CellHook(long x, long z) {
    const unsigned i = FindCell(::bof3::Img(Area141_CellEntries_addr), at::kArea141CellCount, 4, x, z);
    if (i == at::kArea141CellCount) return 0;
    AH_CALL(ScriptFlags_Set40)();
    B(::bof3::Img(AreaW0c_TailState_addr)) = B(::bof3::Img(Area141_CellEntries_addr) + i * 4 + 3);
    B(::bof3::Img(AreaW0a_TailKind_addr)) = 0x34;
    return 1;
}

// original 0x4203C0 (Field_ObjectTriggers id 57; a gap of the tool, area
// 141's block): by the object's word +0x88: 0xC000 runs 0 and 1 off; 0xC001
// ScriptFlags_Set40, Party_HealJoined, tail kind 52 at state 0; 0xC002
// ScriptFlags_Set40, tail kind 52 at state 0xA; any other nothing. al 0.
extern "C" unsigned char __cdecl Area141_Trigger57(unsigned char* object, unsigned char*) {
    switch (rv::As<records::SpriteObject>(object)->trigger_word) {
    case 0xC000:
        AH_CALL(Area141_PaintCells)(Mem(at::Run(0)), 0);
        AH_CALL(Area141_PaintCells)(Mem(at::Run(1)), 0);
        return 0;
    case 0xC001:
        AH_CALL(ScriptFlags_Set40)();
        AH_CALL(Party_HealJoined)();
        B(::bof3::Img(AreaW0a_TailKind_addr)) = 0x34;
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0;
        return 0;
    case 0xC002:
        AH_CALL(ScriptFlags_Set40)();
        B(::bof3::Img(AreaW0a_TailKind_addr)) = 0x34;
        B(::bof3::Img(AreaW0c_TailState_addr)) = 0xA;
        return 0;
    default: return 0;
    }
}

// original 0x420430 (called by area 141's choice 0, init, tail and trigger
// 57): a run of n = run[2] & 0x7F map cells from (run[0], run[1]) - along x
// with bit 0x80, else along z - each AreaMap_SetByte to run[3] when `on`
// (its low byte, read for each cell) is not 0, else run[4]. n 0: none.
extern "C" void __cdecl Area141_PaintCells(const unsigned char* run, unsigned on) {
    const unsigned char n = run[2] & 0x7F;
    const bool along_x = (run[2] & 0x80) != 0;
    for (unsigned char i = 0; i < n; ++i) {
        const unsigned value = static_cast<unsigned char>(on) != 0 ? run[3] : run[4];
        if (along_x)
            AH_CALL(AreaMap_SetByte)(static_cast<unsigned>(run[0] + i), run[1], value);
        else
            AH_CALL(AreaMap_SetByte)(run[0], static_cast<unsigned>(run[1] + i), value);
    }
}

// original 0x4204D0 (called by chapter 14's 0x567145; a gap of the tool): two
// objects placed by EventOp_6x on the scripts 0x62F630 / 0x62F640, animations
// 0x6B and 0x58 after each.
extern "C" void __cdecl Area141_PlacePairAnimated(void) {
    unsigned char a, b;
    if (!TwoFree(a, b)) return;
    PlacePair(a, b, ::bof3::Img(at::kArea141Script630.v), ::bof3::Img(at::kArea141Script640.v), true, 0x6B, 0x58);
}

// original 0x420580 (called by chapter 14's 0x567179; a gap): one object by
// EventOp_6x on 0x62F650, animation 3.
extern "C" void __cdecl Area141_PlaceOneAnimated(void) {
    const unsigned char s = AH_CALL(Sprite_FindFree)();
    if (s == 0xFF) return;
    PlaceOn(s, ::bof3::Img(at::kArea141Script650.v), true);
    AH_CALL(Sprite_SetAnimationAt)(3, 0);
}

// original 0x4205D0 (called by chapter 14's 0x5648B2; a gap): two objects by
// EventOp_6x on 0x62F660 / 0x62F670.
extern "C" void __cdecl Area141_PlacePair6x(void) {
    unsigned char a, b;
    if (!TwoFree(a, b)) return;
    PlacePair(a, b, ::bof3::Img(at::kArea141Script660.v), ::bof3::Img(at::kArea141Script670.v), true, -1, -1);
}

// original 0x420670 (called by chapter 14's 0x5671AC; a gap): two objects by
// EventOp_0x on 0x62F680 / 0x62F691.
extern "C" void __cdecl Area141_PlacePair0x(void) {
    unsigned char a, b;
    if (!TwoFree(a, b)) return;
    PlacePair(a, b, ::bof3::Img(at::kArea141Script680.v), ::bof3::Img(at::kArea141Script691.v), false, -1, -1);
}

// original 0x420710 (called by chapter 14's 0x5671E0; a gap): one object by
// EventOp_0x on 0x62F6A8.
extern "C" void __cdecl Area141_PlaceOne0x(void) {
    const unsigned char s = AH_CALL(Sprite_FindFree)();
    if (s == 0xFF) return;
    PlaceOn(s, ::bof3::Img(at::kArea141Script6A8.v), false);
}

void Area141_Inject() {
    BOF3_INJECT(Area141_ShadeOff);
    BOF3_INJECT(Area141_MoveToX45);
    BOF3_INJECT(Area141_RunShadeDown);
    BOF3_INJECT(Area141_ShadeDownStart);
    BOF3_INJECT(Area141_ShadeDownStep);
    BOF3_INJECT(Area141_RunShadeUp);
    BOF3_INJECT(Area141_ShadeUpStart);
    BOF3_INJECT(Area141_ShadeUpStep);
    BOF3_INJECT(Area141_ChoiceFlag7D);
    BOF3_INJECT(Area141_InitCells);
    BOF3_INJECT(Area141_Tail52);
    BOF3_INJECT(Area141_CellHook);
    BOF3_INJECT(Area141_Trigger57);
    BOF3_INJECT(Area141_PaintCells);
    BOF3_INJECT(Area141_PlacePairAnimated);
    BOF3_INJECT(Area141_PlaceOneAnimated);
    BOF3_INJECT(Area141_PlacePair6x);
    BOF3_INJECT(Area141_PlacePair0x);
    BOF3_INJECT(Area141_PlaceOne0x);
}

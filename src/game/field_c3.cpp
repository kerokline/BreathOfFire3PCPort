// Group FC3 of round twelve (wave two, the field side): 63 functions of the
// field engine's resident code, 0x5172C0..0x5195F9 and 0x525390..0x526DAF -
// analysis/round12_cut.tsv's 57 rows for FC3, 0x5254A0 (docs/scenario_harness.md
// section 7.6) and five dispatchers no list had (FieldCore_State2Steps entries
// 4..8, docs/field_c3.md section 1) - each read to its last instruction with
// capstone (2026-09-29) and taken through the scenario harness's field mode
// (scenario_harness.h, Group::field). docs/field_c3.md has them one row each.
//
//   object steering        op E7, the approach / avoid directions and the best
//                          of four turns, the fade cases of Field_ObjectFadeOut
//                          / FadeIn
//   two mode frames        mode 11's frame, mode 8's steps 5 and 8
//   the field core         FieldCore_State2Steps' sub-states 0 (the move
//                          script's walk), 1 (attached to an extra object), 3
//                          (the hop), 4 (the vertical moves), 5 (the jump out
//                          and in), 6 (the 0xD0 cells), 7 (the fall), 8 (the
//                          recoil), with every step of theirs
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. The
// dispatchers abort past their tables (the original jumps through the dword
// after, another table's entry), and the hop's frame count aborts on a speed
// of 0 (the original's idiv faults) - the owner's rule for an unchecked index
// and a divide, round9 doc section 6; docs/field_c3.md section 6. Every call
// goes through the harness (SH_CALL / SH_AT), so the start-up fuzz can stand
// recorders in for the callees; the callees of other groups of this wave are
// raw addresses in field_c3_callees.h.
//
// Sprite_Current, Field_State and Field_ActiveMember are re-read after every
// call, as the originals re-read them, and held where the original holds them
// in a register. A value the original pushes with a register's leftover upper
// bytes goes out as the byte or word its callee reads (each callee read,
// docs/field_c3.md section 3).
#include "game/field_c3.h"

#include <cstdint>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/field_c3_callees.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = field_c3::at;
using U = std::uint32_t;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using Handler = void (__cdecl*)();

unsigned char* S() { return Sprite_Current; }
unsigned char* F() { return Field_State; }
unsigned char& B(U address) { return At(address)[0]; }
U ULong(const unsigned char* p) { return static_cast<U>(Long(p)); }
void SetULong(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }
short SWord(const unsigned char* p) { return static_cast<short>(Word(p)); }
template <typename T> U AddressOf(T* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
// A word's low byte, stored alone as the original's byte `or` does.
unsigned char& LowByte(unsigned short& w) { return *reinterpret_cast<unsigned char*>(&w); }

// cdq / xor / sub: the absolute value in 32 bits (0x80000000 stays itself).
std::int32_t Abs32(U d) {
    const U sign = static_cast<U>(static_cast<std::int32_t>(d) >> 31);
    return static_cast<std::int32_t>((d ^ sign) - sign);
}

// jmp / call [table + 4 * Sprite_Current[at]]: the table's `entries` handlers,
// read in place (the fuzz swaps the cells for recorders); a Fatal past them,
// where the original jumps through the dword after - the next table's entry
// (docs/field_c3.md section 6).
void Run(const char* who, unsigned long* table, unsigned entries, unsigned at) {
    const unsigned state = Sprite_Current[at];
    if (state >= entries)
        bof3::Fatal("%s: state byte +%u is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/field_c3.md section 6)",
                    who, at, state, entries, (unsigned)AddressOf(table));
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(table[state]))();
}

// Sprite_EnsureAnimation with the byte it compares and hands on (the callers
// push whole registers; Sprite_SetAnimationAt reads the animation as a byte).
void Animate(unsigned animation) { SH_CALL(Sprite_EnsureAnimation)(static_cast<unsigned char>(animation)); }
unsigned char ScriptTick() { return SH_CALL(Sprite_ScriptTick)(); }
unsigned char StepTick() { return SH_CALL(Field_LeaderStepTick)(); }
long GroundAt(U x, U z) { return SH_CALL(MapView_GroundAt)(static_cast<long>(x), static_cast<long>(z)); }
long SlopeAt(U x, U z, unsigned direction) {
    return SH_CALL(MapView_SlopeAt)(static_cast<long>(x), static_cast<long>(z), direction);
}
// AreaMap_Slope's "sloped" byte, which the callers read straight after MapView_SlopeAt.
bool Sloped() { return B(at::kSloped) != 0; }

// The state-1 reset the steps end with: +1 = 1, +2 = +3 = 0 (each through
// Sprite_Current read again, as the originals store).
void BackToState1() {
    S()[1] = 1;
    S()[2] = 0;
    S()[3] = 0;
}

// The shade fade the steps begin with +5 set: Sprite_ShadeFadeBegin, the three
// shade bytes 0x80 and +0x5C 1, +0 bit 6 cleared, then `state` at +at.
void ShadeBegin(unsigned at, unsigned char state) {
    SH_CALL(Sprite_ShadeFadeBegin)();
    S()[0x5D] = 0x80;
    S()[0x5E] = 0x80;
    S()[0x5F] = 0x80;
    S()[0x5C] = 1;
    S()[0] = static_cast<unsigned char>(S()[0] & 0xBF);
    S()[at] = state;
}

// The jump table's step pair (8 bytes a direction) for direction d, unchecked
// (a byte: the image's table read past its eight, as the original's).
U JumpStepX(unsigned d) { return ULong(At(at::kJumpSteps + d * 8)); }
U JumpStepZ(unsigned d) { return ULong(At(at::kJumpSteps + 4 + d * 8)); }
U DirStepX(unsigned d) { return ULong(At(AddressOf(Field_DirectionSteps) + d * 8)); }
U DirStepZ(unsigned d) { return ULong(At(AddressOf(Field_DirectionSteps) + 4 + d * 8)); }

}  // namespace

// ===========================================================================
// The mode frames
// ===========================================================================

// original 0x5172C0 (PSX twin 0x8019A308): mode 11's frame.
extern "C" void __cdecl Mode11_FieldFrame(void) {
    SH_CALL(Field_MembersFrame)();
    SH_CALL(AreaMap_Frame)();
    SH_AT(void (__cdecl*)(), at::kEventObjectFrame)();
    SH_CALL(Party_ExtraScreens)();
    SH_CALL(Party_UpdateScreens)();
    SH_AT(void (__cdecl*)(), at::kPartyScreens)();
    SH_CALL(Effect_RunObjects)();
    SH_CALL(MoveScript_TintFrame)();
    SH_CALL(Field_DrawFrame)();
}

// original 0x517330: mode 8's step 5 (0x656AB8[5]).
extern "C" void __cdecl Mode8_Step5(void) {
    SH_AT(void (__cdecl*)(), at::kMenuDispatchA)();
    SH_CALL(Field_RunTaskRecords)();
}

// original 0x517340: mode 8's step 8 (0x656AB8[8]).
extern "C" void __cdecl Mode8_Step8(void) {
    SH_AT(void (__cdecl*)(), at::kMenuDispatchB)();
    SH_CALL(Field_RunTaskRecords)();
}

// ===========================================================================
// Object steering
// ===========================================================================

// original 0x518B20 (PSX twin 0x801A3790): op E7 - Field_ObjectHandlers[n]
// called with Field_ActiveMember.
extern "C" void __cdecl MoveCmd_OpE7(unsigned char n) {
    unsigned char* const member = Field_ActiveMember;
    if (n >= Field_ObjectHandlers_count)
        bof3::Fatal("MoveCmd_OpE7: handler %u is past the %u of Field_ObjectHandlers (0x65F5F8) - the original calls "
                    "through the dword after (docs/field_c3.md section 6)",
                    (unsigned)n, Field_ObjectHandlers_count);
    reinterpret_cast<void (__cdecl*)(unsigned char*)>(static_cast<std::uintptr_t>(Field_ObjectHandlers[n]))(member);
}

namespace {

// Field_ObjectApproachDirection / AvoidDirection: they differ only in the
// order of each subtraction (the absolute values agree) and in `toward`.
unsigned char ChaseDirection(unsigned char* object, unsigned char toward) {
    unsigned char* const s = Sprite_Current;   // held for both tests and +8
    const std::int32_t dx = Abs32(ULong(At(at::kLeaderX)) - ULong(s + 0x34)) >> 16;
    const unsigned char direction = s[8];
    if (static_cast<int>(SWord(object + 0x98)) > dx) {
        const std::int32_t dz = Abs32(ULong(At(at::kLeaderZ)) - ULong(s + 0x38)) >> 16;
        if (static_cast<int>(SWord(object + 0x9A)) > dz) {
            const unsigned char best = SH_CALL(Field_ObjectBestDirection)(object, toward);
            if (best == 0xFF) {
                S()[9] = 0;
                return 0xFF;
            }
            if ((direction & 7) != best) {
                S()[8] = best;
                object[0x80] = static_cast<unsigned char>(object[0x80] | 8);
            }
            return S()[8];
        }
    }
    SH_CALL(Field_ObjectRandomTurn)(object);
    return SH_CALL(Field_ObjectOpenDirection)(object);
}

}  // namespace

// original 0x518E20 (PSX twin 0x801A3C9C): Field_ObjectApproach's pick.
extern "C" unsigned char __cdecl Field_ObjectApproachDirection(unsigned char* object) { return ChaseDirection(object, 1); }

// original 0x518ED0 (PSX twin 0x801A3DBC): Field_ObjectAvoid's pick.
extern "C" unsigned char __cdecl Field_ObjectAvoidDirection(unsigned char* object) { return ChaseDirection(object, 0); }

// original 0x518F80 (PSX twin 0x801A3EDC): of four quarter turns, the
// unblocked one whose step lands farthest (toward not 0) or nearest (0) to the
// leader's next point, by the sum of the two distances; 0xFF for none.
extern "C" unsigned char __cdecl Field_ObjectBestDirection(unsigned char* object, unsigned char toward) {
    const U pace = B(at::kLeaderPace);
    const U point_z = pace * ULong(At(at::kLeaderStepZ)) + ULong(At(at::kLeaderZ));
    const U point_x = pace * ULong(At(at::kLeaderStepX)) + ULong(At(at::kLeaderX));
    unsigned char* s = Sprite_Current;
    if ((s[8] & 1) == 0) s[8] = static_cast<unsigned char>(s[8] + 1);
    unsigned char best = 0xFF;
    SetULong(object + 0x94, toward != 0 ? 0u : 0x1000000u);
    for (int turn = 4; turn != 0; --turn) {
        const unsigned char blocked = SH_CALL(Field_ObjectBlockedAhead)(object);
        s = Sprite_Current;
        if (blocked == 0) {
            const unsigned d = s[8] & 7u;
            const U score = static_cast<U>(Abs32(DirStepZ(d) + ULong(s + 0x38) - point_z)) +
                            static_cast<U>(Abs32(DirStepX(d) + ULong(s + 0x34) - point_x));
            const U kept = ULong(object + 0x94);
            if (toward != 0 ? kept < score : kept > score) {
                SetULong(object + 0x94, score);
                s = Sprite_Current;
                best = s[8];
            }
        }
        s[8] = static_cast<unsigned char>((s[8] + 2) & 7);
    }
    return best;
}

// The fade cases (Field_ObjectFadeOutSteps / Field_ObjectFadeInSteps). Ours'
// Field_ObjectFadeOut / FadeIn (object_kinds.cpp) run the same code inline;
// these are the starts the tables name. The tint record is the one
// Sprite_SetTint's result names, kept in Field_ActiveMember +0x9F.
namespace {
unsigned char* TintRecord(unsigned index) { return MoveScript_TintRecords + index * 12u; }
int TintSum(const unsigned char* r) {   // signed bytes, as the movsx
    return static_cast<signed char>(r[4]) + static_cast<signed char>(r[3]) + static_cast<signed char>(r[2]);
}
// The step's end: the tint released, the member's context bit cleared, the pose
// back (+1 = +3) and +4 = 0.
void FadeEnd(unsigned char index, unsigned char keep) {
    SH_CALL(Tint_Release)(index);
    unsigned char* const member = Field_ActiveMember;
    member[0x80] = static_cast<unsigned char>(member[0x80] & keep);
    unsigned char* const s = Sprite_Current;
    s[1] = s[3];
    S()[4] = 0;
}
}  // namespace

// original 0x5193D0 (PSX twin 0x801A45E0): Field_ObjectFadeOutSteps[0].
extern "C" void __cdecl Field_ObjectFadeOutStart(void) {
    const unsigned char index = SH_CALL(Sprite_SetTint)(Sprite_Current, 0x1F, 0x1F, 0x1F, 1);
    Field_ActiveMember[0x9F] = index;
    S()[0] = static_cast<unsigned char>(S()[0] & 0xBF);
    S()[4] = 1;
}

// original 0x519410 (PSX twin 0x801A4650): Field_ObjectFadeOutSteps[1].
extern "C" void __cdecl Field_ObjectFadeOutStep(void) {
    unsigned char* const member = Field_ActiveMember;   // held in ecx by the original
    for (const unsigned at : {2u, 3u, 4u}) {
        unsigned char* const r = TintRecord(member[0x9F]);
        if (r[at] != 0) r[at] = static_cast<unsigned char>(r[at] - 1);
    }
    const unsigned char index = member[0x9F];
    if (TintSum(TintRecord(index)) != 0) return;
    FadeEnd(index, 0xFD);
}

// original 0x519500 (PSX twin 0x801A4820): Field_ObjectFadeInSteps[0].
extern "C" void __cdecl Field_ObjectFadeInStart(void) {
    const unsigned char index = SH_CALL(Sprite_SetTint)(Sprite_Current, 0, 0, 0, 1);
    Field_ActiveMember[0x9F] = index;
    S()[4] = 1;
}

// original 0x519530 (PSX twin 0x801A4874): Field_ObjectFadeInSteps[1].
extern "C" void __cdecl Field_ObjectFadeInStep(void) {
    unsigned char* const member = Field_ActiveMember;
    for (const unsigned at : {2u, 3u, 4u}) {
        unsigned char* const r = TintRecord(member[0x9F]);
        if (static_cast<signed char>(r[at]) < 0x1F) r[at] = static_cast<unsigned char>(r[at] + 1);
    }
    const unsigned char index = member[0x9F];
    if (TintSum(TintRecord(index)) != 0x5D) return;
    FadeEnd(index, 0xFB);
}

// ===========================================================================
// The field core, state 2: sub-state 0, the move script's walk
// ===========================================================================

// original 0x525390 (PSX twin 0x801B79FC): FieldCore_State2Steps[0].
extern "C" void __cdecl FieldCore_ScriptMove(void) {
    F()[0x138] = static_cast<unsigned char>(F()[0x138] | 4);
    Run("FieldCore_ScriptMove", FieldCore_ScriptMoveSteps, FieldCore_ScriptMoveSteps_count, 3);
    unsigned char* const s = Sprite_Current;
    if (s[7] & 0x20) return;
    if (s[0] & 0x10)
        SH_CALL(Sprite_ScriptTick)();
    else
        SH_CALL(Sprite_ScriptTickOnce)();
}

// original 0x5253E0 (PSX twin 0x801B7BC0): FieldCore_ScriptMoveSteps[0].
extern "C" void __cdecl FieldCore_ScriptMoveAlign(void) {
    unsigned char* s = Sprite_Current;
    const U x = ULong(s + 0x34);
    if (((x & 0x7FFF) != 0 || (ULong(s + 0x38) & 0x7FFF) != 0) && (s[0x24] & 0x20) == 0) {
        SetULong(s + 0x34, (x + 0x4800) & 0xFFFF8000u);
        s = S();
        SetULong(s + 0x38, (ULong(s + 0x38) + 0x4800) & 0xFFFF8000u);
        s = S();
        const long slope = SlopeAt(ULong(s + 0x34), ULong(s + 0x38), s[8]);
        SetWord(S() + 0x3E, static_cast<unsigned>(slope));
        Animate(S()[8] + 8u);
        s = S();
        if ((ULong(s + 0x34) & 0x7FFF) == 0 && (ULong(s + 0x38) & 0x7FFF) == 0) Animate(s[8]);
        return;
    }
    SH_CALL(Sprite_ClearSteps)();
    S()[3] = static_cast<unsigned char>(S()[3] + 1);
    SH_CALL(FieldCore_ScriptMoveNext)();
}

// original 0x5254A0: FieldCore_ScriptMoveSteps[1], and the tail jumps of
// FieldCore_ScriptMoveAlign and FieldCore_ScriptMoveStep.
extern "C" void __cdecl FieldCore_ScriptMoveNext(void) {
    unsigned char* f = Field_State;   // held in ecx by the original
    if (f[0x125] != 0) {
        f[0x125] = static_cast<unsigned char>(f[0x125] - 1);
        return;
    }
    const unsigned char step = SH_CALL(MoveScript_Step)(f + 0x124, At(ULong(f + 0x130)));
    unsigned char animation = step;   // the byte the original keeps at [esp + 8]
    if (step == 0xFF) return;
    f = Field_State;   // esi, from here to the end
    const unsigned char flags = f[0x124];
    if (flags & 8) {
        f[0x124] = static_cast<unsigned char>(flags & 0xF7);
        if ((Field_StatusBits & 0x40) == 0) Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags & 0xFEFF);
        Animate(S()[8]);
        F()[0x138] = static_cast<unsigned char>(F()[0x138] & 0xFB);
        SH_CALL(Field_MemberTimers)();
        S()[1] = 1;
        S()[2] = 0;
        S()[3] = 0;
        S()[4] = 0;
        S()[7] = 0;
        S()[6] = 0;
        return;
    }
    unsigned char* s = Sprite_Current;   // edx
    if (flags & 0x20) {
        const unsigned char bits = s[7];
        if ((bits & 4) == 0) {
            if ((bits & 8) == 0) {
                s[8] = step;
                Animate(S()[8]);
                s = S();
            }
            s[3] = 3;
            return;
        }
    }
    if (flags & 2) {
        s[8] = step;
        SH_CALL(Sprite_ShadeFadeBegin)();
        S()[0x5D] = 0xC0;
        S()[0x5E] = 0xC0;
        S()[0x5F] = 0xC0;
        S()[0x5C] = 1;
        Animate(S()[8]);
        S()[3] = 4;
        return;
    }
    if (flags & 4) {
        s[8] = step;
        SH_CALL(Sprite_ShadeFadeBegin)();
        S()[0x5C] = 1;
        Animate(S()[8]);
        S()[3] = 5;
        return;
    }
    if (s[9] != 0) {
        if (f[0x12B] != 0) {
            f[0x12B] = static_cast<unsigned char>(f[0x12B] - 1);
            s = S();
        }
        s[8] = step;
        S()[3] = 2;
        animation = static_cast<unsigned char>(step + 8);
        S()[9] = static_cast<unsigned char>(S()[9] - 1);
        StepTick();
    } else {
        s[8] = step;
        S()[3] = 1;
    }
    if ((F()[0x124] & 0x40) == 0) Animate(animation);
}

// original 0x5256A0 (PSX twin 0x801B7F14): FieldCore_ScriptMoveSteps[2].
extern "C" void __cdecl FieldCore_ScriptMoveStep(void) {
    unsigned char* const s = Sprite_Current;   // held in ecx
    if (s[9] != 0) {
        s[9] = static_cast<unsigned char>(s[9] - 1);
        StepTick();
        return;
    }
    unsigned char* const f = Field_State;
    if (f[0x12B] == 0) {
        SH_CALL(FieldCore_ScriptMoveNext)();
        return;
    }
    const U rise = ULong(s + 0x14);
    f[0x12B] = static_cast<unsigned char>(f[0x12B] - 1);
    SH_CALL(Field_JumpStart)();
    if (F()[0x124] & 0x80) SetULong(S() + 0x14, rise);
    S()[9] = static_cast<unsigned char>(S()[9] - 1);
    StepTick();
}

// original 0x525710 (PSX twin 0x801B7FD4): FieldCore_ScriptMoveSteps[3].
extern "C" void __cdecl FieldCore_ScriptMoveWait(void) {
    if (Field_Request == 2) return;
    F()[0x124] = static_cast<unsigned char>(F()[0x124] & 0xDF);
    S()[3] = 0;
}

// original 0x525740 (PSX twin 0x801B801C): FieldCore_ScriptMoveSteps[4].
extern "C" void __cdecl FieldCore_ScriptMoveShadeLower(void) {
    if (SH_CALL(Sprite_ShadeLower)(4) == 0) return;
    F()[0x124] = static_cast<unsigned char>(F()[0x124] & 0xFD);
    S()[3] = 1;
}

// original 0x525770 (PSX twin 0x801B8074): FieldCore_ScriptMoveSteps[5].
extern "C" void __cdecl FieldCore_ScriptMoveShadeFade(void) {
    if (SH_CALL(Sprite_ShadeFadeStep)(4) == 0) return;
    F()[0x124] = static_cast<unsigned char>(F()[0x124] & 0xFB);
    S()[3] = 1;
}

// ===========================================================================
// Sub-state 1: attached to an extra object
// ===========================================================================

// original 0x5257A0 (PSX twin 0x801B80CC): FieldCore_State2Steps[1].
extern "C" void __cdecl FieldCore_Attached(void) {
    if (S()[9] == 0) {
        S()[0x24] = static_cast<unsigned char>(S()[0x24] | 0x20);
        // Sprite_ObjectsExtra[+0x18] (unchecked, as the original), each field
        // through +0x18 read again
        static const unsigned kFields[] = {0x34, 0x38, 0x3C, 0x64, 0x68, 0x6C};
        for (const unsigned field : kFields) {
            unsigned char* const s = S();
            SetULong(s + field, ULong(At(at::kExtraPositions + (field - 0x34) + ULong(s + 0x18) * 0xA4u)));
        }
        long offset[3];
        SH_CALL(MoveCmd_AttachOffset)(offset, S()[0x1C]);
        SetULong(S() + 0x34, ULong(S() + 0x34) + static_cast<U>(offset[0]));
        SetULong(S() + 0x38, ULong(S() + 0x38) + static_cast<U>(offset[1]));
        SetWord(S() + 0x3E, Word(S() + 0x3E) + static_cast<unsigned>(offset[2]));
    }
    SH_CALL(FieldCore_ScriptMove)();
}

// ===========================================================================
// Sub-state 3: the hop
// ===========================================================================

// original 0x525960 (PSX twin 0x801B83A8): FieldCore_State2Steps[3].
extern "C" void __cdecl FieldCore_Hop(void) { Run("FieldCore_Hop", FieldCore_HopSteps, FieldCore_HopSteps_count, 3); }

// original 0x525980 (PSX twin 0x801B83EC): FieldCore_HopSteps[0].
extern "C" void __cdecl FieldCore_HopBegin(void) {
    Animate(S()[8] == 3 ? 0x38 : 0x39);
    F()[0x128] = 3;
    S()[3] = 1;
}

// original 0x5259D0 (PSX twin 0x801B8444): FieldCore_HopSteps[1].
extern "C" void __cdecl FieldCore_HopLaunch(void) {
    if (SH_CALL(Sprite_ScriptTickOnce)() == 0) return;
    const unsigned pace = F()[0x128];
    const U speed = Field_MoveSpeeds[pace];   // a byte index: the image's six read past, as the original's
    if (speed == 0)
        bof3::Fatal("FieldCore_HopLaunch: speed 0 (Field_MoveSpeeds[%u]) - the original divides 0x20 by it and faults "
                    "(docs/field_c3.md section 6)",
                    pace);
    S()[9] = static_cast<unsigned char>(0x20u / speed);
    unsigned char* s = S();
    U n = (ULong(s + 0x70) & 0xFF) + 1;
    SetULong(s + 0xC, JumpStepX(s[8]) * n * speed);
    s = S();
    n = (ULong(s + 0x70) & 0xFF) + 1;
    SetULong(s + 0x10, JumpStepZ(s[8]) * n * speed);
    s = S();
    SetULong(s + 0x14, ((ULong(s + 0x70) & 0xFF) + 1) << 6);
    s = S();
    SetULong(s + 0x20, (0xFFFFFFFFu - (ULong(s + 0x70) & 0xFF)) << 3);
    s = S();
    if (s[5] == 0) {
        Field_Kind2X = static_cast<long>(ULong(s + 0xC) * s[9] + ULong(s + 0x34));
        Field_Kind2Z = static_cast<long>(ULong(s + 0x10) * s[9] + ULong(s + 0x38));
        MoveScript_F3Divisor = static_cast<short>(speed << 3);
    }
    Animate(s[8] == 3 ? 0x3A : 0x3B);
    SH_CALL(Sound_PlayEffect)(0x104);
    S()[9] = static_cast<unsigned char>(S()[9] - 1);
    s = S();
    SetULong(s + 0x14, ULong(s + 0x14) + ULong(s + 0x20));
    StepTick();
    S()[3] = 2;
}

// original 0x525B20 (PSX twin 0x801B8650): FieldCore_HopSteps[2].
extern "C" void __cdecl FieldCore_HopRise(void) {
    unsigned char* s = Sprite_Current;
    if (s[9] == 0) {
        SetULong(s + 0xC, 0);
        SetULong(S() + 0x10, 0);
        S()[3] = 3;
    } else {
        s[9] = static_cast<unsigned char>(s[9] - 1);
    }
    s = S();
    SetULong(s + 0x14, ULong(s + 0x14) + ULong(s + 0x20));
    StepTick();
    s = S();
    if (s[5] == 0) SH_CALL(MapView_SetElevation)(SWord(s + 0x3E));
    ScriptTick();
}

// original 0x525B90 (PSX twin 0x801B86EC): FieldCore_HopSteps[3].
extern "C" void __cdecl FieldCore_HopFall(void) {
    unsigned char* s = Sprite_Current;
    SetULong(s + 0x14, ULong(s + 0x14) + ULong(s + 0x20));
    StepTick();
    s = S();
    const long ground = GroundAt(ULong(s + 0x34), ULong(s + 0x38));
    s = S();   // ecx
    if (static_cast<short>(ground) > SWord(s + 0x3E)) {
        SetWord(s + 0x3E, static_cast<unsigned>(ground));
        s = S();
        if (Long(s + 0x14) < -0x80)
            Animate(static_cast<unsigned char>((s[8] >> 1) + 0x34));
        else
            Animate(s[8] == 3 ? 0x38 : 0x39);
        SetULong(S() + 0x14, 0);
        SetULong(S() + 0x20, 0);
        S()[3] = 4;
        s = S();
    }
    if (s[5] == 0 && ((static_cast<unsigned>(Field_ScriptFlags2) | Field_ScriptFlags) & 8) == 0)
        SH_CALL(MapView_SetElevation)(SWord(s + 0x3E));
    ScriptTick();
}

// original 0x525C50 (PSX twin 0x801B8814): FieldCore_HopSteps[4].
extern "C" void __cdecl FieldCore_HopLand(void) {
    if (SH_CALL(Sprite_ScriptTickOnce)() == 0) return;
    if (SH_CALL(Field_TileD0)() != 0) return;
    Animate(S()[8]);
    F()[0x137] = 0;
    BackToState1();
}

// ===========================================================================
// Sub-state 4: the vertical moves
// ===========================================================================

// original 0x525CA0: FieldCore_State2Steps[4].
extern "C" void __cdecl FieldCore_Vertical(void) {
    F()[0x137] = 3;
    Run("FieldCore_Vertical", FieldCore_VerticalSteps, FieldCore_VerticalSteps_count, 3);
}

// original 0x525CC0: FieldCore_VerticalSteps[0].
extern "C" void __cdecl FieldCore_Up(void) { Run("FieldCore_Up", FieldCore_UpSteps, FieldCore_UpSteps_count, 4); }

// original 0x525EF0: FieldCore_VerticalSteps[1].
extern "C" void __cdecl FieldCore_Down(void) { Run("FieldCore_Down", FieldCore_DownSteps, FieldCore_DownSteps_count, 4); }

// original 0x525CE0: FieldCore_UpSteps[0].
extern "C" void __cdecl FieldCore_UpBegin(void) {
    SH_AT(void (__cdecl*)(), at::kUpFace)();
    S()[4] = static_cast<unsigned char>(S()[4] + 1);
}

// original 0x525CF0: FieldCore_UpSteps[1].
extern "C" void __cdecl FieldCore_UpOut(void) {
    SetWord(S() + 0x3E, Word(S() + 0x3E) + 0x10u);
    unsigned char* const s = S();
    const long ground = GroundAt(ULong(s + 0x34), ULong(s + 0x38));
    if (static_cast<int>(SWord(S() + 0x3E)) > static_cast<int>(static_cast<short>(ground)) + 0x200) Field_Request = 5;
    ScriptTick();
}

// original 0x525D40: FieldCore_UpSteps[2].
extern "C" void __cdecl FieldCore_UpArrive(void) {
    unsigned char* s = S();
    const long ground = GroundAt(ULong(s + 0x34), ULong(s + 0x38));
    SetWord(S() + 0x3E, static_cast<unsigned>(ground) - 0x200u);
    s = S();
    SetULong(s + 0x34, ULong(s + 0x34) + ((0u - JumpStepX(s[8]) * ((ULong(s + 0x70) & 0xFF) + 1)) << 5));
    s = S();
    SetULong(s + 0x38, ULong(s + 0x38) + ((0u - JumpStepZ(s[8]) * ((ULong(s + 0x70) & 0xFF) + 1)) << 5));
    Animate(S()[8] == 7 ? 0x3C : 0x3D);
    s = S();
    if (s[5] != 0) {
        ShadeBegin(4, 3);
        return;
    }
    s[4] = 4;
}

// original 0x525E30: FieldCore_UpSteps[4].
extern "C" void __cdecl FieldCore_UpIn(void) {
    SetWord(S() + 0x3E, Word(S() + 0x3E) + 0x10u);
    const long ground = GroundAt(ULong(At(at::kTargetX)), ULong(At(at::kTargetZ)));
    const int stop = static_cast<int>(static_cast<short>(ground)) -
                     static_cast<int>(SWord(At(at::kTargetOffsets + F()[0x89] * 2u)));
    if (static_cast<int>(SWord(S() + 0x3E)) >= stop) {
        SH_AT(void (__cdecl*)(), at::kUpLanded)();
        S()[4] = static_cast<unsigned char>(S()[4] + 1);
        return;
    }
    ScriptTick();
}

// original 0x525E90: FieldCore_UpSteps[5].
extern "C" void __cdecl FieldCore_UpWait5(void) {
    if (SH_AT(unsigned char (__cdecl*)(), at::kUpWait5)() != 0) S()[4] = static_cast<unsigned char>(S()[4] + 1);
}

// original 0x525EB0: FieldCore_UpSteps[6].
extern "C" void __cdecl FieldCore_UpWait6(void) {
    if (SH_AT(unsigned char (__cdecl*)(), at::kUpWait6)() != 0) S()[4] = static_cast<unsigned char>(S()[4] + 1);
}

// original 0x525ED0: FieldCore_UpSteps[7].
extern "C" void __cdecl FieldCore_UpEnd(void) {
    if (SH_AT(unsigned char (__cdecl*)(), at::kUpWait7)() == 0) return;
    if (S()[5] == 0) Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 & 0xFFF7);
}

// original 0x525F10: FieldCore_DownSteps[0].
extern "C" void __cdecl FieldCore_DownBegin(void) {
    SH_AT(void (__cdecl*)(), at::kDownJumpSetUp)();
    unsigned char* const s = S();
    LowByte(Field_ScriptFlags2) = static_cast<unsigned char>(LowByte(Field_ScriptFlags2) | 8);   // or byte ptr [0x905BA4], 8
    s[4] = 1;
}

namespace {
// FieldCore_DownSteps[1..4]: +4 = next when the FE2 step answers al.
void DownWait(U callee, unsigned char next) {
    if (SH_AT(unsigned char (__cdecl*)(), callee)() != 0) S()[4] = next;
}
}  // namespace

// originals 0x525F30, 0x525F50, 0x525F70, 0x525F90: FieldCore_DownSteps[1..4].
extern "C" void __cdecl FieldCore_DownWait1(void) { DownWait(at::kDownWait1, 2); }
extern "C" void __cdecl FieldCore_DownWait2(void) { DownWait(at::kDownWait2, 3); }
extern "C" void __cdecl FieldCore_DownWait3(void) { DownWait(at::kDownWait3, 4); }
extern "C" void __cdecl FieldCore_DownWait4(void) { DownWait(at::kDownWait4, 5); }

// original 0x525FB0: FieldCore_DownSteps[5].
extern "C" void __cdecl FieldCore_DownOut(void) {
    SetWord(S() + 0x3E, Word(S() + 0x3E) - 0x10u);
    const long ground = GroundAt(ULong(At(at::kTargetX)), ULong(At(at::kTargetZ)));
    if (static_cast<int>(SWord(S() + 0x3E)) < static_cast<int>(static_cast<short>(ground)) - 0x200) Field_Request = 5;
    ScriptTick();
}

// original 0x526000: FieldCore_DownSteps[6].
extern "C" void __cdecl FieldCore_DownArrive(void) {
    unsigned char* s = S();
    const long ground = GroundAt(ULong(s + 0x34), ULong(s + 0x38));
    SetWord(S() + 0x3E, static_cast<unsigned>(ground) + 0x200u);
    Animate(S()[8] == 3 ? 0x3C : 0x3D);
    s = S();
    if (s[5] != 0) {
        ShadeBegin(4, 7);
        return;
    }
    s[4] = 8;
}

// original 0x5260A0: FieldCore_DownSteps[8].
extern "C" void __cdecl FieldCore_DownIn(void) {
    SetWord(S() + 0x3E, Word(S() + 0x3E) - 0x10u);
    unsigned char* s = S();
    const long ground = GroundAt(ULong(s + 0x34), ULong(s + 0x38));
    s = S();
    if (SWord(s + 0x3E) < static_cast<short>(ground)) {
        const long again = GroundAt(ULong(s + 0x34), ULong(s + 0x38));
        SetWord(S() + 0x3E, static_cast<unsigned>(again));
        S()[9] = 2;
        S()[0xA] = 0;
        Animate(S()[8] == 3 ? 7 : 1);
        S()[4] = 9;
    }
    ScriptTick();
}

// original 0x526120: FieldCore_DownSteps[9].
extern "C" void __cdecl FieldCore_DownLand(void) {
    S()[9] = static_cast<unsigned char>(S()[9] - 1);
    unsigned char* s = S();
    if (s[9] != 0) return;
    s[0xA] = static_cast<unsigned char>(s[0xA] + 1);
    s = S();
    Animate(s[8] == 3 ? static_cast<unsigned char>(7 - s[0xA]) : static_cast<unsigned char>(s[0xA] + 1));
    s = S();
    if (s[0xA] != 4) {
        s[9] = 2;
        return;
    }
    if (s[5] == 0) Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 & 0xFFF7);
    s[9] = 0;
    S()[0xA] = 0;
    BackToState1();
    S()[4] = 0;
}

// original 0x5261C0: FieldCore_UpSteps[3] and FieldCore_DownSteps[7].
extern "C" void __cdecl FieldCore_VerticalShade(void) {
    if (SH_CALL(Sprite_ShadeFadeStep)(8) != 0) S()[4] = static_cast<unsigned char>(S()[4] + 1);
}

// ===========================================================================
// Sub-state 5: the jump out and back in
// ===========================================================================

// original 0x5261E0: FieldCore_State2Steps[5].
extern "C" void __cdecl FieldCore_JumpExit(void) {
    F()[0x137] = 2;
    Run("FieldCore_JumpExit", FieldCore_JumpExitSteps, FieldCore_JumpExitSteps_count, 3);
}

// original 0x526200: FieldCore_JumpExitSteps[0].
extern "C" void __cdecl FieldCore_JumpExitBegin(void) {
    unsigned char* const s = S();
    LowByte(Field_ScriptFlags2) = static_cast<unsigned char>(LowByte(Field_ScriptFlags2) | 8);   // or byte ptr [0x905BA4], 8
    Animate(s[8] + 8u);
    S()[9] = 0;
    S()[0xA] = 0;
    S()[3] = static_cast<unsigned char>(S()[3] + 1);
}

// original 0x526240: FieldCore_JumpExitSteps[1].
extern "C" void __cdecl FieldCore_JumpExitOut(void) {
    unsigned char* s = S();
    if (s[9] == 0) {
        s[0xA] = static_cast<unsigned char>(s[0xA] + 1);
        if (S()[0xA] == 3) Field_Request = 5;
        SH_CALL(Field_JumpStart)();
        s = S();
    }
    s[9] = static_cast<unsigned char>(s[9] - 1);
    StepTick();
    ScriptTick();
}

// original 0x526280: FieldCore_JumpExitSteps[2].
extern "C" void __cdecl FieldCore_JumpExitArrive(void) {
    unsigned char* s = S();
    if (s[0x70] != 0) {
        SetULong(s + 0x34, ULong(s + 0x34) + ((0u - JumpStepX(s[8]) * 3u) << 5));
        s = S();
        SetULong(s + 0x38, ULong(s + 0x38) + ((0u - JumpStepZ(s[8]) * 3u) << 5));
        S()[0xA] = 7;
    } else {
        SetULong(s + 0x34, ULong(s + 0x34) + ((0u - JumpStepX(s[8])) << 6));
        s = S();
        SetULong(s + 0x38, ULong(s + 0x38) + ((0u - JumpStepZ(s[8])) << 6));
        S()[0xA] = 5;
    }
    S()[9] = 0;
    s = S();
    const long ground = GroundAt(ULong(s + 0x34), ULong(s + 0x38));
    SetWord(S() + 0x3E, static_cast<unsigned>(ground));
    F()[0x128] = 2;
    Animate(S()[8] + 8u);
    s = S();
    if (s[5] != 0) {
        ShadeBegin(3, 3);
        return;
    }
    s[3] = 4;
}

// original 0x5263C0: FieldCore_JumpExitSteps[3].
extern "C" void __cdecl FieldCore_JumpExitShade(void) {
    if (SH_CALL(Sprite_ShadeFadeStep)(8) != 0) S()[3] = static_cast<unsigned char>(S()[3] + 1);
}

// original 0x5263E0: FieldCore_JumpExitSteps[4].
extern "C" void __cdecl FieldCore_JumpExitIn(void) {
    unsigned char* s = S();
    if (s[9] == 0) {
        s[0xA] = static_cast<unsigned char>(s[0xA] - 1);
        s = S();
        if (s[0xA] == 0) {
            if (s[5] == 0) Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 & 0xFFF7);
            F()[0x128] = 3;
            SetULong(S() + 0xC, 0);
            SetULong(S() + 0x10, 0);
            F()[0x137] = 0;
            BackToState1();
            S()[9] = 0;
            S()[0xA] = 0;
            Animate(S()[8]);
            return;
        }
        SH_CALL(Field_JumpStart)();
        s = S();
    }
    s[9] = static_cast<unsigned char>(s[9] - 1);
    StepTick();
    ScriptTick();
}

// ===========================================================================
// Sub-state 6: the 0xD0 cells
// ===========================================================================

// original 0x526490: FieldCore_State2Steps[6].
extern "C" void __cdecl FieldCore_TileD0(void) {
    F()[0x137] = 5;
    Run("FieldCore_TileD0", FieldCore_TileD0Steps, FieldCore_TileD0Steps_count, 3);
    ScriptTick();
}

// original 0x5264C0: FieldCore_TileD0Steps[0].
extern "C" void __cdecl FieldCore_TileD0Begin(void) {
    S()[9] = 0;
    unsigned char* const s = S();
    s[0xB] = s[8];
    F()[0x128] = 4;
    S()[3] = 1;
}

// original 0x5264F0: FieldCore_TileD0Steps[1].
extern "C" void __cdecl FieldCore_TileD0Move(void) {
    unsigned char* s = S();
    if (s[9] == 0) {
        if (SH_CALL(AreaMap_CellsNone)(Long(s + 0x34), Long(s + 0x38), ULong(s + 0x70) & 0xFF, 0xD0, 0) != 0) {
            // off the cells: the pace back to 3, the facing kept, state 1
            F()[0x128] = 3;
            s = S();
            s[8] = s[0xB];
            Animate(S()[8]);
            F()[0x137] = 0;
            BackToState1();
            return;
        }
        SH_CALL(FieldCore_TileD0Exit)();
        if (SH_CALL(FieldCore_TileD0Slope)() != 0) {
            Animate(S()[8]);
            S()[2] = 3;
            S()[3] = 0;
            return;
        }
        if (SH_CALL(Field_CellAhead)() == 0) {
            S()[8] = static_cast<unsigned char>(S()[8] + 1);
            SH_CALL(Field_CellAhead)();
        }
        switch (ULong(reinterpret_cast<const unsigned char*>(&Input_Held)) & 0xF000) {
        case 0x1000: S()[0xB] = 0; break;
        case 0x2000: S()[0xB] = 2; break;
        case 0x3000: S()[0xB] = 1; break;
        case 0x4000: S()[0xB] = 4; break;
        case 0x6000: S()[0xB] = 3; break;
        case 0x8000: S()[0xB] = 6; break;
        case 0x9000: S()[0xB] = 7; break;
        case 0xC000: S()[0xB] = 5; break;
        default: break;
        }
        Animate(S()[0xB] + 8u);
        SH_CALL(Field_JumpStart)();
        s = S();
    }
    s[9] = static_cast<unsigned char>(s[9] - 1);
    StepTick();
    ScriptTick();
}

// original 0x5266B0: called by FieldCore_TileD0Move - the way off the cells.
extern "C" void __cdecl FieldCore_TileD0Exit(void) {
    for (unsigned i = 0; i < 8; ++i) {
        unsigned char* const s = S();
        const unsigned d = (s[8] + i) & 7u;
        const U n = (ULong(s + 0x70) & 0xFF) + 1;
        const U x = DirStepX(d) * n + ULong(s + 0x34);
        const U z = DirStepZ(d) * n + ULong(s + 0x38);
        if (SH_CALL(Field_WayBlocked)(static_cast<long>(x), static_cast<long>(z), s[0x70], SWord(s + 0x3E)) != 0) continue;
        if (SH_CALL(AreaMap_CellsNone)(static_cast<long>(x), static_cast<long>(z), ULong(S() + 0x70) & 0xFF, 0xD0, 0) != 0) {
            S()[8] = static_cast<unsigned char>(d);
            return;
        }
    }
    unsigned char* s = S();
    unsigned char best_direction = s[8];
    short best = SWord(s + 0x3E);
    for (unsigned i = 0; i < 4; ++i) {
        if (i != 0) s = S();
        const unsigned d = i * 2 + 1;
        const U x = DirStepX(d) + ULong(s + 0x34);
        const U z = DirStepZ(d) + ULong(s + 0x38);
        short ground = 0;
        if (SH_CALL(FieldCore_TileD0Probe)(static_cast<long>(x), static_cast<long>(z), &ground, 1) != 0 && best > ground) {
            best = ground;
            best_direction = static_cast<unsigned char>(d);
        }
    }
    S()[8] = best_direction;
}

// original 0x526820: called by FieldCore_TileD0Exit - the ground at (x, z), or
// al 0 for a steep slope above the object.
extern "C" unsigned char __cdecl FieldCore_TileD0Probe(long x, long z, short* ground, unsigned direction) {
    const long slope = SlopeAt(static_cast<U>(x), static_cast<U>(z), direction);
    *ground = static_cast<short>(slope);
    if (Sloped() && static_cast<short>(slope) > 0x40) {
        const long height = GroundAt(static_cast<U>(x), static_cast<U>(z));
        if (SWord(S() + 0x3E) < static_cast<short>(height)) return 0;
    }
    *ground = static_cast<short>(GroundAt(static_cast<U>(x), static_cast<U>(z)));
    return 1;
}

namespace {
// MapView_SlopeAt's answer steep (above 0x40, a signed word) with the sloped byte set.
bool Steep(U x, U z, unsigned direction) {
    const long slope = SlopeAt(x, z, direction);
    return Sloped() && static_cast<short>(slope) > 0x40;
}
}  // namespace

// original 0x526880: called by FieldCore_TileD0Move - a steep edge ahead.
extern "C" unsigned char __cdecl FieldCore_TileD0Slope(void) {
    unsigned char* const s = S();
    const unsigned char direction = s[8];
    if (s[0x70] == 0) {
        if (direction == 3) {
            if (Word(s + 0x34) != 0) return 0;
            U z = ULong(s + 0x38) & 0xFFFF0000u;
            const U x = ULong(s + 0x34) + 0x8000;
            if (!Steep(x, z, 3)) return 0;
            if (Word(S() + 0x38) == 0) return 1;
            z += 0x8000;
            return Steep(x, z, 3) ? 1 : 0;
        }
        if (direction == 5) {
            if (Word(s + 0x38) != 0) return 0;
            const U z = ULong(s + 0x38) + 0x8000;
            U x = ULong(s + 0x34) & 0xFFFF0000u;
            if (!Steep(x, z, 5)) return 0;
            if (Word(S() + 0x34) == 0) return 1;
            x += 0x8000;
            return Steep(x, z, 5) ? 1 : 0;
        }
        return 0;
    }
    if (direction == 3) {
        if (Word(s + 0x34) == 0) return 0;
        U z = ULong(s + 0x38) & 0xFFFF0000u;
        const U x = ULong(s + 0x34) + 0x10000;
        if (!Steep(x, z, 3)) return 0;
        z += 0x8000;
        if (!Steep(x, z, 3)) return 0;
        if (Word(S() + 0x38) != 0) return 1;
        z -= 0x10000;
        return Steep(x, z, 3) ? 1 : 0;
    }
    if (direction == 5) {
        if (Word(s + 0x38) == 0) return 0;
        const U z = ULong(s + 0x38) + 0x10000;
        U x = ULong(s + 0x34) & 0xFFFF0000u;
        if (!Steep(x, z, 5)) return 0;
        x += 0x8000;
        if (!Steep(x, z, 5)) return 0;
        if (Word(S() + 0x34) != 0) return 1;
        x -= 0x10000;
        return Steep(x, z, 3) ? 1 : 0;   // direction 3 here, as the original pushes
    }
    return 0;
}

// ===========================================================================
// Sub-state 7: the fall
// ===========================================================================

// original 0x526A90: FieldCore_State2Steps[7].
extern "C" void __cdecl FieldCore_Fall(void) {
    F()[0x137] = 6;
    Run("FieldCore_Fall", FieldCore_FallSteps, FieldCore_FallSteps_count, 3);
}

// original 0x526AB0: FieldCore_FallSteps[0].
extern "C" void __cdecl FieldCore_FallBegin(void) {
    SH_CALL(Sound_PlayEffect)(0x107);
    unsigned char* const s = S();
    SH_CALL(Area_LinkAt)(Word(s + 0x36), Word(s + 0x3A));
    SetULong(S() + 0x14, 0);
    Animate(S()[8]);
    S()[3] = 1;
}

// original 0x526B00: FieldCore_FallSteps[1].
extern "C" void __cdecl FieldCore_FallSpin(void) {
    unsigned char* s = S();
    SetULong(s + 0x14, ULong(s + 0x14) - 8);
    s = S();
    SetWord(s + 0x3E, Word(s + 0x3E) + Word(s + 0x14));
    s = S();
    s[8] = static_cast<unsigned char>((s[8] + 1) & 7);
    Animate(S()[8]);
    s = S();
    const long ground = GroundAt(ULong(s + 0x34), ULong(s + 0x38));
    if (static_cast<int>(static_cast<short>(ground)) - static_cast<int>(SWord(S() + 0x3E)) > 0x100 && Field_Request != 5) {
        B(at::kScriptFlags2High) = static_cast<unsigned char>(B(at::kScriptFlags2High) | 2);
        Field_Request = 5;
    }
}

// ===========================================================================
// Sub-state 8: the recoil
// ===========================================================================

// original 0x526B80: FieldCore_State2Steps[8].
extern "C" void __cdecl FieldCore_Recoil(void) { Run("FieldCore_Recoil", FieldCore_RecoilSteps, FieldCore_RecoilSteps_count, 3); }

// original 0x526BA0 (PSX twin 0x801BA208): FieldCore_RecoilSteps[0].
extern "C" void __cdecl FieldCore_RecoilBegin(void) {
    unsigned char* s = S();
    if (s[0x5C] != 0) {
        SH_CALL(Sprite_LoadPalette)(reinterpret_cast<unsigned short*>(At(at::kPalettes + s[5] * 64u)), 0);
        S()[0] = static_cast<unsigned char>(S()[0] & 0xDF);
        S()[0x5C] = 0;
        S()[0x5D] = 0;
        S()[0x5E] = 0;
        S()[0x5F] = 0;
        s = S();
    }
    s[0] = static_cast<unsigned char>(s[0] & 0xBF);
    s = S();
    const U x = ULong(s + 0x34);
    if ((x & 0x7FFF) != 0 || (ULong(s + 0x38) & 0x7FFF) != 0) {
        SetULong(s + 0x34, (x + 0x4800) & 0xFFFF8000u);
        s = S();
        SetULong(s + 0x38, (ULong(s + 0x38) + 0x4800) & 0xFFFF8000u);
        s = S();
        const long slope = SlopeAt(ULong(s + 0x34), ULong(s + 0x38), s[8]);
        SetWord(S() + 0x3E, static_cast<unsigned>(slope));
        s = S();
        if (s[5] == 0) {
            Field_Kind2X = Long(s + 0x34);
            Field_Kind2Z = Long(s + 0x38);
            SH_CALL(Field_ViewReset)();
            s = S();
        }
    }
    const unsigned char facing = s[8];
    s[8] = static_cast<unsigned char>(facing ^ 4);
    const unsigned char ahead = SH_CALL(Field_CellAhead)();
    if (ahead != 1 && ahead != 3) {
        SH_CALL(Sprite_ClearSteps)();
    } else if (SH_CALL(Field_LeaderPushObjects)() != 0) {
        S()[9] = 0;
        SH_CALL(Sprite_ClearSteps)();
    } else {
        SH_CALL(Field_JumpStart)();
        S()[9] = static_cast<unsigned char>(S()[9] - 1);
        SH_CALL(Sprite_ApplyVelocity)();
    }
    SH_AT(void (__cdecl*)(unsigned char), at::kRecoilFace)(S()[0xB]);
    S()[8] = facing;
    S()[0xA] = 8;
    Animate(S()[8]);
    SH_CALL(Sound_PlayEffect)(0x108);
    S()[3] = static_cast<unsigned char>(S()[3] + 1);
}

// original 0x526D30 (PSX twin 0x801BA470): FieldCore_RecoilSteps[1].
extern "C" void __cdecl FieldCore_RecoilBlink(void) {
    unsigned char* s = S();
    if (s[9] != 0) {
        SH_CALL(Sprite_ApplyVelocity)();
        S()[9] = static_cast<unsigned char>(S()[9] - 1);
        s = S();
        if (s[9] == 0) {
            SH_CALL(Sprite_ClearSteps)();
            s = S();
        }
    }
    if (s[0xA] != 0) {
        s[0xA] = static_cast<unsigned char>(s[0xA] - 1);
        s = S();
        if (s[0xA] & 1)
            s[0] = static_cast<unsigned char>(s[0] & 0xBF);
        else
            s[0] = static_cast<unsigned char>(s[0] | 0x40);
        return;
    }
    s[0] = static_cast<unsigned char>(s[0] & 0xBF);
    BackToState1();
}

void FieldC3_Inject() {
    if (bof3::WantsShadow("field_c3")) field_c3::SelfTest();
    BOF3_INJECT(Mode11_FieldFrame);
    BOF3_INJECT(Mode8_Step5);
    BOF3_INJECT(Mode8_Step8);
    BOF3_INJECT(MoveCmd_OpE7);
    BOF3_INJECT(Field_ObjectApproachDirection);
    BOF3_INJECT(Field_ObjectAvoidDirection);
    BOF3_INJECT(Field_ObjectBestDirection);
    BOF3_INJECT(Field_ObjectFadeOutStart);
    BOF3_INJECT(Field_ObjectFadeOutStep);
    BOF3_INJECT(Field_ObjectFadeInStart);
    BOF3_INJECT(Field_ObjectFadeInStep);
    BOF3_INJECT(FieldCore_ScriptMove);
    BOF3_INJECT(FieldCore_ScriptMoveAlign);
    BOF3_INJECT(FieldCore_ScriptMoveNext);
    BOF3_INJECT(FieldCore_ScriptMoveStep);
    BOF3_INJECT(FieldCore_ScriptMoveWait);
    BOF3_INJECT(FieldCore_ScriptMoveShadeLower);
    BOF3_INJECT(FieldCore_ScriptMoveShadeFade);
    BOF3_INJECT(FieldCore_Attached);
    BOF3_INJECT(FieldCore_Hop);
    BOF3_INJECT(FieldCore_HopBegin);
    BOF3_INJECT(FieldCore_HopLaunch);
    BOF3_INJECT(FieldCore_HopRise);
    BOF3_INJECT(FieldCore_HopFall);
    BOF3_INJECT(FieldCore_HopLand);
    BOF3_INJECT(FieldCore_Vertical);
    BOF3_INJECT(FieldCore_Up);
    BOF3_INJECT(FieldCore_UpBegin);
    BOF3_INJECT(FieldCore_UpOut);
    BOF3_INJECT(FieldCore_UpArrive);
    BOF3_INJECT(FieldCore_UpIn);
    BOF3_INJECT(FieldCore_UpWait5);
    BOF3_INJECT(FieldCore_UpWait6);
    BOF3_INJECT(FieldCore_UpEnd);
    BOF3_INJECT(FieldCore_Down);
    BOF3_INJECT(FieldCore_DownBegin);
    BOF3_INJECT(FieldCore_DownWait1);
    BOF3_INJECT(FieldCore_DownWait2);
    BOF3_INJECT(FieldCore_DownWait3);
    BOF3_INJECT(FieldCore_DownWait4);
    BOF3_INJECT(FieldCore_DownOut);
    BOF3_INJECT(FieldCore_DownArrive);
    BOF3_INJECT(FieldCore_DownIn);
    BOF3_INJECT(FieldCore_DownLand);
    BOF3_INJECT(FieldCore_VerticalShade);
    BOF3_INJECT(FieldCore_JumpExit);
    BOF3_INJECT(FieldCore_JumpExitBegin);
    BOF3_INJECT(FieldCore_JumpExitOut);
    BOF3_INJECT(FieldCore_JumpExitArrive);
    BOF3_INJECT(FieldCore_JumpExitShade);
    BOF3_INJECT(FieldCore_JumpExitIn);
    BOF3_INJECT(FieldCore_TileD0);
    BOF3_INJECT(FieldCore_TileD0Begin);
    BOF3_INJECT(FieldCore_TileD0Move);
    BOF3_INJECT(FieldCore_TileD0Exit);
    BOF3_INJECT(FieldCore_TileD0Probe);
    BOF3_INJECT(FieldCore_TileD0Slope);
    BOF3_INJECT(FieldCore_Fall);
    BOF3_INJECT(FieldCore_FallBegin);
    BOF3_INJECT(FieldCore_FallSpin);
    BOF3_INJECT(FieldCore_Recoil);
    BOF3_INJECT(FieldCore_RecoilBegin);
    BOF3_INJECT(FieldCore_RecoilBlink);
}

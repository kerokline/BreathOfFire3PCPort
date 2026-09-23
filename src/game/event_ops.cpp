// The event script's shop and scene ops (group V1 of the sixth round) -
// docs/event-ops.md for each function, its PSX twin, the fuzz and what the
// shop route reaches.
//
//   - EventOp_5x / 9x / Dx, the three placement ops of the event script that
//     round five left Capcom's (docs/event-script.md section 1).
//   - The party leader's walk: Field_LeaderWalk 0x52DB90 is sub-state 1 of
//     Field_LeaderControl (the table 0x660954, docs/field-event.md), with the
//     step's target and cell, the pace, the encounter test, the six button
//     tests Field_LeaderStand and Field_LeaderWalk make first, and what they
//     call: the object and cell the leader faces, the passage list, the talk.
//   - The chapter's and the area's step hooks the step's target consults.
//
// Terms. "SC" is Sprite_Current and "FS" Field_State, read afresh at every
// use as the original reads [0x937F88] / [0x905D98] (a callee may move
// either). The leader is ObjTrio's member 0, which Field_State points at.
// Every call goes through event_ops::g (event_ops_callees.h), so that the
// start-up fuzz can stand recorders in for them.
#include "game/event_ops.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/event_ops_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace event_ops {

using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

namespace {
template <typename T> T Fn(std::uint32_t address) { return reinterpret_cast<T>(static_cast<std::uintptr_t>(address)); }
}  // namespace

const std::uint32_t kStepHandlers[kStepCases] = {
    0x404F80, 0x406DE0, 0x408EB0, 0x409440, 0x40B410, 0x40E120, 0x40EB90, 0x413980, 0x414330, 0x4163B0,
    0x416770, 0x418620, 0x419DA0, 0x41E5E0, 0x41F960, 0x420A10, 0x4215B0, 0x422000, 0x423910, 0x427270,
    0x427A80, 0x4281A0, 0x539AC0, 0x429DC0, 0x429DC0, 0x429DC0, 0x429DC0, 0x429DC0, 0x429DC0, 0x429DC0,
    0x429DC0, 0x429DC0, 0x429DC0, 0x429DC0, 0x42B9F0, 0x42C000, 0x42C700, 0x42CE30,
};
const std::uint32_t kArriveHandlers[kArriveCases] = {
    0x405B00, 0x409340, 0x417ED0, 0x422790, 0x426470, 0x426B20, 0x427B50, 0x4287E0,
};

const Callees kOriginals = {
    Field_LeaderTalkTest, Field_LeaderSwapTest, Field_LeaderMenuTest, Field_LeaderCheckTest,
    Field_LeaderRequest4Test, Field_LeaderRequest9Test,
    Field_LeaderDirection, Field_LeaderStepTarget, Field_LeaderPushObjects, Field_LeaderPushCount,
    Field_LeaderSetPace, Field_EncounterDue, Field_LeaderStepTick, Field_LeaderEffectTest, Field_LeaderStepCell,
    Party_CanSwap, Field_EffectAhead, Area_PassageAhead, Field_FacingObject, Field_ObjectInPath, Field_CellAround,
    Field_CellAroundNear, Field_LeaderTalkTo, Scenario_StepHook, Scenario_ArriveHook, Area_StepHook,
    Area_ArriveHook, Area_ReturnGate,
    reinterpret_cast<const ChapterHooks* const*>(static_cast<std::uintptr_t>(at::kChapterHooks)),
    Field_LeaderAnimation, Sprite_ClearSteps, Sprite_EnsureAnimation, AreaMap_ByteAt, Field_ZoneCounterRoll,
    Party_Count, Area_ZoneAt, Rand, Sound_PlayEffect, Area_LinkAt, MapView_GroundAt, Sprite_ScriptTick,
    Sprite_ObjectAt, Sprite_PointInReach, Field_ChangeArea, Flags_Test, Flags_Clear, EventObj_Reset,
    Sprite_SetAnimationBank, AreaMap_Elevation, EventObj_SetFlags, Sprite_SetAnimation,
    Fn<void (__cdecl*)()>(fn::kFaceObject),
    Fn<void (__cdecl*)()>(fn::kEncounterArea),
    Fn<unsigned char (__cdecl*)()>(fn::kExitGateway),
    Fn<void (__cdecl*)()>(fn::kExitFromCell),
    Fn<unsigned char (__cdecl*)(unsigned)>(fn::kCellAroundLarge),
    Fn<int (__cdecl*)(unsigned, unsigned)>(fn::kCellHook),
    Fn<void (__cdecl*)(unsigned, unsigned, unsigned)>(fn::kSetCell),
    Fn<unsigned char (__cdecl*)(unsigned, unsigned)>(fn::kPartyVisible),
    Fn<unsigned char (__cdecl*)(long, long, unsigned, unsigned, unsigned)>(fn::kMemberFits),
    Fn<void (__cdecl*)()>(fn::kLeaderMove),
    Fn<void (__cdecl*)()>(fn::kLeaderFollow),
    Fn<void (__cdecl*)()>(fn::kLeaderGround),
    Fn<unsigned char (__cdecl*)()>(fn::kStepCode),
    Fn<long (__cdecl*)(long, long, std::uint32_t)>(fn::kSlopeAt),
    Fn<unsigned char (__cdecl*)(unsigned, unsigned)>(fn::kTestFB),
    {Fn<Hook>(kStepHandlers[0]), Fn<Hook>(kStepHandlers[1]), Fn<Hook>(kStepHandlers[2]), Fn<Hook>(kStepHandlers[3]),
     Fn<Hook>(kStepHandlers[4]), Fn<Hook>(kStepHandlers[5]), Fn<Hook>(kStepHandlers[6]), Fn<Hook>(kStepHandlers[7]),
     Fn<Hook>(kStepHandlers[8]), Fn<Hook>(kStepHandlers[9]), Fn<Hook>(kStepHandlers[10]), Fn<Hook>(kStepHandlers[11]),
     Fn<Hook>(kStepHandlers[12]), Fn<Hook>(kStepHandlers[13]), Fn<Hook>(kStepHandlers[14]),
     Fn<Hook>(kStepHandlers[15]), Fn<Hook>(kStepHandlers[16]), Fn<Hook>(kStepHandlers[17]),
     Fn<Hook>(kStepHandlers[18]), Fn<Hook>(kStepHandlers[19]), Fn<Hook>(kStepHandlers[20]),
     Fn<Hook>(kStepHandlers[21]), Fn<Hook>(kStepHandlers[22]), Fn<Hook>(kStepHandlers[23]),
     Fn<Hook>(kStepHandlers[24]), Fn<Hook>(kStepHandlers[25]), Fn<Hook>(kStepHandlers[26]),
     Fn<Hook>(kStepHandlers[27]), Fn<Hook>(kStepHandlers[28]), Fn<Hook>(kStepHandlers[29]),
     Fn<Hook>(kStepHandlers[30]), Fn<Hook>(kStepHandlers[31]), Fn<Hook>(kStepHandlers[32]),
     Fn<Hook>(kStepHandlers[33]), Fn<Hook>(kStepHandlers[34]), Fn<Hook>(kStepHandlers[35]),
     Fn<Hook>(kStepHandlers[36]), Fn<Hook>(kStepHandlers[37])},
    {Fn<Hook>(kArriveHandlers[0]), Fn<Hook>(kArriveHandlers[1]), Fn<Hook>(kArriveHandlers[2]),
     Fn<Hook>(kArriveHandlers[3]), Fn<Hook>(kArriveHandlers[4]), Fn<Hook>(kArriveHandlers[5]),
     Fn<Hook>(kArriveHandlers[6]), Fn<Hook>(kArriveHandlers[7])},
};
Callees g = kOriginals;

}  // namespace event_ops

using namespace event_ops;

namespace {

std::uint32_t Address(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
unsigned char& Byte(std::uint32_t a) { return *At(a); }

// Field_ScriptFlags' two bytes and Field_ScriptFlags2's, as the original
// tests them (`test ah, n` on a dword read, `test byte ptr`).
constexpr std::uint32_t kFlagsLo = 0x9039A2, kFlagsHi = 0x9039A3, kFlags2Lo = 0x905BA4, kFlags2Hi = 0x905BA5;

// The count and the bank word (event_script.cpp's terms).
short Count() {
    short v;
    std::memcpy(&v, At(at::kScratch), sizeof v);
    return v;
}
void SetCount(unsigned v) { SetWord(At(at::kScratch), v); }
unsigned short Bank() { return Word(At(at::kScratch2)); }

unsigned char* Object(int n) { return Sprite_Objects + n * 0xA4; }
unsigned char* Extra(int n) { return Sprite_ObjectsExtra + n * 0xA4; }

// A placement's x or z (event_script.cpp): the whole part from `whole`, the
// half (0x8000) if `half` is non-zero.
std::uint32_t Coordinate(unsigned char half, unsigned char whole) {
    return (half ? 0x8000u : 0u) | static_cast<std::uint32_t>(whole) << 16;
}

// Field_DirectionSteps' pair and the walk's unit pair for a direction - a
// whole byte into tables of eight, as the original indexes them.
std::uint32_t StepX(unsigned d) { return static_cast<std::uint32_t>(Long(reinterpret_cast<const unsigned char*>(Field_DirectionSteps) + d * 8u)); }
std::uint32_t StepZ(unsigned d) { return static_cast<std::uint32_t>(Long(reinterpret_cast<const unsigned char*>(Field_DirectionSteps) + d * 8u + 4)); }
std::int32_t WalkX(unsigned d) { return Long(At(at::kWalkDelta) + d * 8u); }
std::int32_t WalkZ(unsigned d) { return Long(At(at::kWalkDelta) + d * 8u + 4); }
// The cell step pair: two signed bytes per direction.
signed char CellDX(unsigned d) { return static_cast<signed char>(At(at::kCellDelta)[d * 2u]); }
signed char CellDZ(unsigned d) { return static_cast<signed char>(At(at::kCellDelta)[d * 2u + 1]); }

// The whole part of a 16.16 position, as the original pushes it: the high
// half of the dword, which the callee reads as a short.
short Whole(std::uint32_t v) { return static_cast<short>(v >> 16); }

// The original's eax at the slope call: Sprite_Current with its low byte
// replaced by the direction (`mov eax, [0x937F88]` / `mov al, [eax + 8]`).
// In inline assembly because clang 22.1.8 at -O2 folds the C++ spelling,
// `(p & 0xFFFFFF00) | p[8]`, to `p[8]` alone - a miscompile the start-up
// fuzz caught (docs/event-ops.md section 8).
std::uint32_t DirectionInPointer(const unsigned char* sc) {
    std::uint32_t v = Address(sc);
    __asm__ volatile("movb 8(%1), %b0" : "+q"(v) : "r"(sc) : "memory");
    return v;
}

// `cdq` / `idiv`: the original's signed division, the divide fault included.
std::int32_t Idiv(std::int32_t dividend, std::int32_t divisor) {
    std::int32_t quotient;
    __asm__ volatile("cltd\n\tidivl %2" : "=a"(quotient) : "a"(dividend), "r"(divisor) : "edx", "cc");
    return quotient;
}

}  // namespace

// --- the event script's placements ----------------------------------------------

// original 0x57B310: op 5x, 18 bytes - places Sprite_Objects[count] (PSX
// FUN_801A7F60, store for store). op[1]:op[2] the animation bank, op[3] /
// op[4] words +0x98 / +0x9A, op[5..8] x and z, op[9] +1, op[0xA] the speed
// index +0x84, op[0xB] +2, op[0xC] the flags (EventObj_SetFlags),
// op[0xD]:op[0xE] word +0x88, op[0xF] +0x83, op[0x10] / op[0x11] +0x18 / +0x1C
// as dwords; op[0]'s nibbles the direction +8 and the kind +6; +0x5C is 2.
// As the original has it: the object's own bytes are written by the count
// read again after each call, the sprite's through Sprite_Current read again
// (a callee that moved either moves what follows), and nothing past the
// count's test of 30 checks it.
extern "C" void __cdecl EventOp_5x(const unsigned char* op) {
    if (Count() >= 30) return;
    unsigned char* const o = Object(Count());
    Sprite_Current = o;
    Field_ActiveMember = o;
    SetWord(At(at::kScratch2), static_cast<unsigned>(op[1]) << 8 | op[2]);
    g.reset();
    g.set_bank(Bank());
    Sprite_Current[0] = 1;
    SetLong(Sprite_Current + 0x34, static_cast<std::int32_t>(Coordinate(op[5], op[6])));
    SetLong(Object(Count()) + 0x8C, Long(Sprite_Current + 0x34));
    SetLong(Sprite_Current + 0x38, static_cast<std::int32_t>(Coordinate(op[7], op[8])));
    SetLong(Object(Count()) + 0x90, Long(Sprite_Current + 0x38));
    SetLong(Object(Count()) + 0x94, 0);
    const long elevation = g.elevation(Long(Sprite_Current + 0x34), Long(Sprite_Current + 0x38));
    SetWord(Sprite_Current + 0x3E, static_cast<unsigned>(elevation));
    Sprite_Current[1] = op[9];
    Sprite_Current[2] = op[0xB];
    Sprite_Current[8] = op[0] & 0xF;
    Sprite_Current[6] = op[0] >> 4;
    SetLong(Sprite_Current + 0x18, op[0x10]);
    SetLong(Sprite_Current + 0x1C, op[0x11]);
    unsigned char* const n = Object(Count());
    SetWord(n + 0x98, op[3]);
    SetWord(n + 0x9A, op[4]);
    SetWord(n + 0x88, static_cast<unsigned>(op[0xD]) << 8 | op[0xE]);
    n[0x83] = op[0xF];
    n[0x84] = op[0xA];
    n[0xA0] = 0x7F;
    g.set_flags(op + 0xC);
    Sprite_Current[0x5D] = 0;
    Sprite_Current[0x5F] = 0;
    Sprite_Current[0x5E] = 0;
    Sprite_Current[0x5C] = 2;
    g.face();
    SetCount(static_cast<unsigned>(Count() + 1));
}

// original 0x57B500: op Dx, 4 bytes (PSX FUN_801A82F0) - with op[1],
// Field_ActiveMember +0x86 = op[3]; without, its word +0x88 = op[2]:op[3].
extern "C" void __cdecl EventOp_Dx(const unsigned char* op) {
    if (op[1] != 0) {
        Field_ActiveMember[0x86] = op[3];
        return;
    }
    SetWord(Field_ActiveMember + 0x88, static_cast<unsigned>(op[2]) << 8 | op[3]);
}

// original 0x57B530: op 9x, 13 bytes - places Sprite_Objects[count] as an
// object of state 6 (PSX FUN_801A8338, store for store). op[1]:op[2] the
// bank, op[3] bit 7 sets +0 bit 0x20 and its low nibble is +0x5C, op[4..7] x
// and z, op[8] +5, op[9]:op[0xA] the dword +0x18, op[0xB] the flags, op[0xC]
// +0xB; +0x2A is bit 4 of +7. Then bit +5 of the bits at 0x9040CC: clear -
// Sprite_SetAnimation(0); set - with op[0xC] bit 0 the object is switched
// off (+0 = 0) and the count NOT advanced, else a sprite on a whole cell
// stamps 0x10 into the area map at its cell (0x579F00) and
// Sprite_SetAnimation(1).
// As the original has it: the count and Sprite_Current are read again after
// each call, as in EventOp_5x; the flag's byte is +5 >> 3, a whole byte's
// worth (32 bytes of bits).
extern "C" void __cdecl EventOp_9x(const unsigned char* op) {
    if (Count() >= 30) return;
    unsigned char* const o = Object(Count());
    Sprite_Current = o;
    Field_ActiveMember = o;
    SetWord(At(at::kScratch2), static_cast<unsigned>(op[1]) << 8 | op[2]);
    g.reset();
    g.set_bank(Bank());
    Sprite_Current[0] = 1;
    SetLong(Sprite_Current + 0x34, static_cast<std::int32_t>(Coordinate(op[4], op[5])));
    SetLong(Object(Count()) + 0x8C, Long(Sprite_Current + 0x34));
    SetLong(Sprite_Current + 0x38, static_cast<std::int32_t>(Coordinate(op[6], op[7])));
    SetLong(Object(Count()) + 0x90, Long(Sprite_Current + 0x38));
    SetLong(Object(Count()) + 0x94, 0);
    const long elevation = g.elevation(Long(Sprite_Current + 0x34), Long(Sprite_Current + 0x38));
    SetWord(Sprite_Current + 0x3E, static_cast<unsigned>(elevation));
    Sprite_Current[1] = 6;
    Sprite_Current[2] = 0;
    Sprite_Current[8] = op[0] & 0xF;
    Sprite_Current[6] = op[0] >> 4;
    {
        unsigned char* const n = Object(Count());
        n[0x84] = 0;
        n[0xA0] = 0x7F;
    }
    g.set_flags(op + 0xB);
    if (op[3] & 0x80) Sprite_Current[0] |= 0x20;
    Sprite_Current[0x5D] = 0;
    Sprite_Current[0x5F] = 0;
    Sprite_Current[0x5E] = 0;
    Sprite_Current[0x5C] = op[3] & 0xF;
    Sprite_Current[0x29] = 6;
    {
        unsigned char* const sc = Sprite_Current;
        SetWord(Object(Count()) + 0x88, 0);
        SetLong(sc + 0x18, static_cast<std::int32_t>(static_cast<unsigned>(op[9]) << 8 | op[0xA]));
    }
    Sprite_Current[5] = op[8];
    Sprite_Current[0xB] = op[0xC];
    Sprite_Current[0x2A] = (Sprite_Current[7] >> 4) & 1;
    unsigned char* const sc = Sprite_Current;
    const unsigned bit = sc[5];
    if (At(at::kPlacedFlags)[bit >> 3] & (1u << (bit & 7))) {
        if (op[0xC] & 1) {
            sc[0] = 0;
            return;
        }
        if (Word(sc + 0x34) == 0 && Word(sc + 0x38) == 0) g.set_cell(Word(sc + 0x36), Word(sc + 0x3A), 0x10);
        g.set_animation(1);
    } else {
        g.set_animation(0);
    }
    SetCount(static_cast<unsigned>(Count() + 1));
}

// --- the leader's walk ---------------------------------------------------------

// original 0x52DB90: the leader's sub-state 1, walking (PSX FUN_801B1560) -
// Field_LeaderControl's table 0x660954 entry 1. Nothing while bit 8 of
// Field_ScriptFlags or bit 6 of Field_ScriptFlags2 is set; the six button
// tests in order, any one ending the frame; then the frame count +0xA down to
// 0; with no direction held, standing (sub-state 0); else the step's target
// by Field_LeaderStepTarget, and by its answer: 0 a step (or a push, if
// something stands in the way), 1 the effect test, 2 / 3 / 5 / 6 the
// leader's states 2 and 8 with sub-states, 0xFF stop.
// As the original has it: answer 4, and any from 7 to 0xFE, does nothing.
extern "C" void __cdecl Field_LeaderWalk(void) {
    if (Byte(kFlagsHi) & 1) return;
    if (Byte(kFlags2Lo) & 0x40) return;
    if (g.talk_test()) return;
    if (g.swap_test()) return;
    if (g.menu_test()) return;
    if (g.check_test()) return;
    if (g.request4_test()) return;
    if (g.request9_test()) return;
    {
        unsigned char* const sc = Sprite_Current;
        if (sc[0xA] != 0) {
            sc[0xA] = static_cast<unsigned char>(sc[0xA] - 1);
            return;
        }
    }
    if (!g.direction()) {
        g.leader_animation(Sprite_Current[8]);
        Field_State[0x128] = 3;
        Field_State[0x137] = 0;
        g.clear_steps();
        Sprite_Current[7] = 0;
        Sprite_Current[9] = 0;
        Sprite_Current[0xB] = 0;
        unsigned char* const sc = Sprite_Current;
        MoveScript_F3Divisor = 0;
        MoveScript_FAWord = 0;
        sc[2] = 0;
        return;
    }
    switch (g.step_target()) {
    case 0:
        if (g.push_objects()) {
            Field_State[0x137] = 0;
            g.leader_animation(Sprite_Current[8]);
            Sprite_Current[2] = 1;
            g.push_count();
            return;
        }
        Sprite_Current[0xB] = 0;
        g.set_pace();
        g.leader_follow();
        g.leader_ground();
        Sprite_Current[0xB] = 0;
        if (Field_InputFlags & 1) {
            if (Byte(kFlags2Hi) & 0x30) {
                // bits 12 / 13: a step counter +6 runs down, or the cell ahead
                // (+9 steps on) is not 0 - either ends them.
                unsigned char* const sc = Sprite_Current;
                const std::uint32_t steps = sc[9];
                const std::uint32_t x = static_cast<std::uint32_t>(Long(sc + 0xC)) * steps + static_cast<std::uint32_t>(Long(sc + 0x34));
                const std::uint32_t z = static_cast<std::uint32_t>(Long(sc + 0x10)) * steps + static_cast<std::uint32_t>(Long(sc + 0x38));
                sc[6] = static_cast<unsigned char>(sc[6] - 1);
                if (Sprite_Current[6] == 0 || g.byte_at(Whole(x), Whole(z)) != 0) {
                    Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 & 0xCFFF);
                    Field_EdgeBits = 0;
                    g.zone_roll(0);
                }
            } else if (g.encounter_due()) {
                g.encounter_area();
                unsigned char* const sc = Sprite_Current;
                SetLong(At(at::kExitX), 0x190000);
                SetLong(At(at::kExitZ), 0x190000);
                Byte(at::kExitKind) = 4;
                sc[6] = 4;
                Byte(kFlags2Hi) |= 0x10;
            }
        }
        g.leader_animation(static_cast<unsigned char>(Sprite_Current[8] + 8));
        Sprite_Current[9] = static_cast<unsigned char>(Sprite_Current[9] - 1);
        g.step_tick();
        Field_State[0x137] = 1;
        Sprite_Current[2] = 2;
        return;
    case 1:
        if (g.effect_test(Field_State[0x128])) return;
        Field_State[0x137] = 0;
        Field_State[0x128] = 3;
        Sprite_Current[9] = 0;
        g.leader_animation(Sprite_Current[8]);
        Field_State[0x137] = 0;
        Sprite_Current[2] = 1;
        g.push_count();
        return;
    case 2: {
        Field_State[0x128] = 3;
        g.ensure_animation(Sprite_Current[8]);
        Sprite_Current[1] = 2;
        Sprite_Current[2] = 4;
        unsigned char* const sc = Sprite_Current;
        sc[3] = (sc[8] == 1 || sc[8] == 7) ? 0 : 1;
        Field_State[0x137] = 3;
        Field_State[0x138] |= 2;
        Byte(kFlags2Lo) |= 0x80;
        return;
    }
    case 3:
        Field_State[0x128] = 2;
        g.ensure_animation(Sprite_Current[8]);
        Sprite_Current[1] = 2;
        Sprite_Current[2] = 5;
        Field_State[0x137] = 2;
        Field_State[0x138] |= 2;
        Byte(kFlags2Hi) |= 1;
        return;
    case 5:
        Field_State[0x128] = 3;
        g.ensure_animation(Sprite_Current[8]);
        Field_State[0x137] = 4;
        Sprite_Current[1] = 2;
        Sprite_Current[2] = 3;
        return;
    case 6: {
        Field_State[0x128] = 3;
        g.ensure_animation(Sprite_Current[8]);
        Field_State[0x137] = 3;
        Sprite_Current[1] = 8;
        unsigned char* const sc = Sprite_Current;
        sc[2] = (sc[8] == 1 || sc[8] == 7) ? 0 : 1;
        return;
    }
    case 0xFF:
        Field_State[0x137] = 0;
        Field_State[0x128] = 3;
        Sprite_Current[9] = 0;
        Sprite_Current[0xB] = 0;
        g.leader_animation(Sprite_Current[8]);
        Field_State[0x137] = 0;
        return;
    default: return;
    }
}

// original 0x52E060: the leader's pace, Field_State +0x128 (PSX
// FUN_801B1C80) - 3 walking, 4 running. Walking if any member of the first
// party list has bit 7 of its record's +0x10; left as it is under bit 9 of
// Field_ScriptFlags; walking when "the run button is held" equals the byte
// 0x903A5E, or under bit 5 of Field_ScriptFlags2; else running.
// As the original has it: the byte is compared whole with 0 / 1, so a value
// of 2 or more never walks.
extern "C" void __cdecl Field_LeaderSetPace(void) {
    const unsigned n = static_cast<unsigned>(g.party_count(0)) & 0xFF;
    for (unsigned i = 0; i < n; ++i) {
        const unsigned record = At(at::kMemberRecord)[At(at::kPartyLists)[i]];
        if (At(at::kActorRecords + 0x10)[record * at::kActorStride] & 0x80) {
            Field_State[0x128] = 3;
            return;
        }
    }
    if (Byte(kFlagsHi) & 2) return;
    const unsigned held = (Word(At(at::kButtonRun)) & Field_InputHeld) != 0 ? 1u : 0u;
    if ((held ^ Byte(at::kRunOption)) == 0 || (Byte(kFlags2Lo) & 0x20)) {
        Field_State[0x128] = 3;
        return;
    }
    Field_State[0x128] = 4;
}

// original 0x52E140: one step's movement (0x536670, group V2's), then the
// sprite's script tick while running (PSX FUN_801B1DF4). The original's al is
// the tick's answer, or Field_State's low byte.
extern "C" unsigned char __cdecl Field_LeaderStepTick(void) {
    g.leader_move();
    unsigned char* const fs = Field_State;
    if (fs[0x128] >= 4) return g.script_tick();
    return static_cast<unsigned char>(Address(fs));
}

// original 0x52E510: the step target's cell, words 0x903854 / 0x903856 - the
// whole parts of 0x903858 / 0x90385C, one more on an axis the walk moves
// positively along when that position has a fraction. Called only by
// Field_LeaderStepTarget; pointer-less, found by its three calls (the list
// had it inside 0x52E160's 0x420 bytes).
extern "C" void __cdecl Field_LeaderStepCell(void) {
    const unsigned short x = Word(At(at::kTargetX) + 2);
    unsigned char* const sc = Sprite_Current;
    SetWord(At(at::kCellX), x);
    if (Word(At(at::kTargetX)) != 0 && WalkX(sc[8]) > 0) SetWord(At(at::kCellX), x + 1u);
    const unsigned short z = Word(At(at::kTargetZ) + 2);
    SetWord(At(at::kCellZ), z);
    if (Word(At(at::kTargetZ)) != 0 && WalkZ(sc[8]) > 0) SetWord(At(at::kCellZ), z + 1u);
}

// original 0x52E160: where this step goes, and what lies there (PSX
// FUN_801B1E3C). The count +9 is 0x20 on a diagonal (directions 2 and 6),
// 0x10 else, divided by Field_MoveSpeeds[pace]; the target 0x903858 /
// 0x90385C is the position plus the walk unit times +9 times the speed
// (twice that for a sprite with +0x70). 0x526DB0's code says what lies ahead;
// unless Field_Request is 9 the chapter's step hook may take the step (then
// 0xFF for a sprite in state 2, else code 0), and the target is read back.
// By the code: 0 -> 1 (walk), 1 -> 0, 2 a slope or a drop (0x5725C0,
// AreaMap_Slope's wrapper), 3 a slope back, 4 / 5 an edge (-> 2 / 3 unless a
// link is there), 6 -> 5, 7 -> 6, anything else -> 0.
// As the original has it: the pace indexes Field_MoveSpeeds whole, and a zero
// there is a divide fault (Idiv). The slope's third argument is the direction
// in the low byte of Sprite_Current's address - AreaMap_Slope reads bytes 1..3
// of it for a direction of 8..15 (area_slope.cpp); for 16 and up it reads its
// caller's stack, which no reimplementation reproduces (no direction the
// field sets is 8 or more).
extern "C" unsigned char __cdecl Field_LeaderStepTarget(void) {
    const unsigned char facing = Sprite_Current[8];
    unsigned char code = g.step_code();
    {
        unsigned char* const sc = Sprite_Current;
        sc[9] = (sc[8] == 2 || sc[8] == 6) ? 0x20 : 0x10;
    }
    const std::int32_t speed = Field_MoveSpeeds[Field_State[0x128]];
    Sprite_Current[9] = static_cast<unsigned char>(Idiv(Sprite_Current[9], speed));
    std::uint32_t x, z;
    {
        unsigned char* const sc = Sprite_Current;
        const unsigned d = sc[8];
        const std::uint32_t n = sc[9];
        const std::uint32_t dx = static_cast<std::uint32_t>(WalkX(d)) * n * static_cast<std::uint32_t>(speed);
        const std::uint32_t dz = static_cast<std::uint32_t>(WalkZ(d)) * n * static_cast<std::uint32_t>(speed);
        if (sc[0x70] == 0) {
            x = dx + static_cast<std::uint32_t>(Long(sc + 0x34));
            SetLong(At(at::kTargetX), static_cast<std::int32_t>(x));
            z = dz + static_cast<std::uint32_t>(Long(sc + 0x38));
            SetLong(At(at::kTargetZ), static_cast<std::int32_t>(z));
        } else {
            x = static_cast<std::uint32_t>(Long(sc + 0x34)) + 2 * dx;
            SetLong(At(at::kTargetX), static_cast<std::int32_t>(x));
            z = static_cast<std::uint32_t>(Long(sc + 0x38)) + 2 * dz;
            SetLong(At(at::kTargetZ), static_cast<std::int32_t>(z));
        }
    }
    if (Field_Request != 9) {
        const int taken = g.step_hook(static_cast<long>(x), static_cast<long>(z));
        if (taken != 0) {
            if (Sprite_Current[1] == 2) return 0xFF;
            code = 0;
        }
        z = static_cast<std::uint32_t>(Long(At(at::kTargetZ)));
        x = static_cast<std::uint32_t>(Long(At(at::kTargetX)));
    }
    const auto link = [] { return g.link_at(Word(At(at::kCellX)), Word(At(at::kCellZ))); };
    switch (code) {
    case 0: return 1;
    case 2: {
        const auto slope = static_cast<short>(g.slope_at(static_cast<long>(x), static_cast<long>(z),
                                                          DirectionInPointer(Sprite_Current)));
        g.step_cell();
        if (Byte(at::kScratch) == 0) {
            const int rise = static_cast<short>(Word(Sprite_Current + 0x3E)) - slope;
            if (std::abs(rise) > 0x80) {
                if (!link()) Field_Request = 5;
                MoveScript_F3Divisor = 0;
                return 1;
            }
            if (link()) return 0;
            Byte(kFlags2Lo) |= 0x60;
            Field_Request = 5;
            return 0;
        }
        if (slope > 0x40) {
            if (!link() && Sprite_Current == At(at::kLeader)) {
                if (g.test_fb(Word(Sprite_Current + 0x36), Word(Sprite_Current + 0x3A))) {
                    g.play_effect(Word(At(at::kStepSounds) + At(at::kScratch)[1] * 2u));
                    g.ensure_animation(static_cast<unsigned char>((Sprite_Current[8] >> 1) + 0x10));
                } else {
                    g.ensure_animation(Sprite_Current[8]);
                }
                Field_Request = 5;
            }
            MoveScript_F3Divisor = 0;
            return 4;
        }
        if (link()) return 0;
        Byte(kFlags2Lo) |= 0x60;
        Field_Request = 5;
        return 0;
    }
    case 3: {
        const auto slope = static_cast<short>(g.slope_at(static_cast<long>(x), static_cast<long>(z),
                                                          DirectionInPointer(Sprite_Current)));
        if (Byte(at::kScratch) == 0) return 0;
        if (slope > 0x40) {
            unsigned char* const sc = Sprite_Current;
            MoveScript_F3Divisor = 0;
            sc[8] = facing;
            return 1;
        }
        g.ground_at(Long(At(at::kTargetX)), Long(At(at::kTargetZ)));
        return 0;
    }
    case 4:
        g.step_cell();
        if (link()) return 1;
        Byte(kFlags2Lo) |= 0xC0;
        return 2;
    case 5:
        g.step_cell();
        if (link()) return 1;
        Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 | 0x140);
        return 3;
    case 6: return 5;
    case 7: return 6;
    default: return 0;
    }
}

// --- the tests Field_LeaderStand and Field_LeaderWalk make ---------------------

// original 0x530030: an encounter due? (PSX FUN_801B4EA8). Bit 5 of
// Field_ScriptFlags clears the zone counter ObjTrio +0x134 first; the zone
// the leader stands in (Area_ZoneAt) must have +4, the counter must be set
// and at most Field_EdgeBits (7/10 of it while walking, pace below 4), ObjTrio
// +0x138 bit 0 clear, the word 0x929EE0 at most 0xF0 unless bit 0 of
// Field_InputFlags, and no member with bit 6. Then on foot (bit 0): the cell
// +9 steps on is 0; else 0x591F30(4, 0) - which moves Sprite_Current, put
// back - and 0x535C50 (group V2's) for every member at its 0x7E06E0 position.
// As the original has it: the member count is read again after each call,
// and the answer is the last call's al.
extern "C" unsigned char __cdecl Field_EncounterDue(void) {
    if (Byte(kFlagsLo) & 0x20) SetWord(At(at::kZoneCounter), 0);
    const unsigned char* const zone = g.zone_at(Word(At(at::kLeaderZone)), Word(At(at::kLeaderZone) + 4));
    if (zone[4] == 0) {
        SetWord(At(at::kZoneCounter), 0);
        return 0;
    }
    if (Word(At(at::kZoneCounter)) == 0) return 0;
    std::uint32_t edge = Field_EdgeBits;
    if (Byte(at::kLeaderPace) < 4) edge = edge * 7 / 10;
    if (edge < Word(At(at::kZoneCounter))) return 0;
    if (Byte(at::kLeaderFlags) & 1) return 0;
    const bool on_foot = (Field_InputFlags & 1) != 0;
    if (!on_foot && Long(At(at::kScreenCounter)) > 0xF0) return 0;
    const int members = Field_MemberCount;
    for (int i = 0; i < members; ++i)
        if (At(at::kLeader)[i * at::kMemberStride] & 0x40) return 0;
    if (on_foot) {
        unsigned char* const sc = Sprite_Current;
        const std::uint32_t steps = sc[9];
        const std::uint32_t x = static_cast<std::uint32_t>(Long(sc + 0xC)) * steps + static_cast<std::uint32_t>(Long(sc + 0x34));
        const std::uint32_t z = static_cast<std::uint32_t>(Long(sc + 0x10)) * steps + static_cast<std::uint32_t>(Long(sc + 0x38));
        return g.byte_at(Whole(x), Whole(z)) == 0;
    }
    unsigned char* const saved = Sprite_Current;
    unsigned char r = g.party_visible(4, 0);
    Sprite_Current = saved;
    if (r == 0) return r;
    for (unsigned i = 0; i < Field_MemberCount; ++i) {
        const unsigned char* const at = At(at::kMemberPositions) + i * 8;
        const unsigned slot = At(at::kMemberSlot)[At(at::kPartyLists + 3)[i]];
        r = g.member_fits(Long(at), Long(at + 4), slot, 0x10, 1);
        if (r == 0) return r;
    }
    return r;
}

// original 0x5301F0: the idle fidget (PSX FUN_801B6444) - not for a record
// with bit 5 of +0x10 or on foot; Field_State +0x136 counts frames past 0xF0,
// then back to 0, and unless a member other than the leader is in state 9,
// one Rand in two: +0x137 = 9, the pose, +9 = 2, state 6, sub-state 0.
extern "C" unsigned char __cdecl Field_LeaderIdleTest(void) {
    if (At(at::kActorRecords + 0x10)[Field_State[0x148] * at::kActorStride] & 0x20) return 0;
    if (Field_InputFlags & 1) return 0;
    Field_State[0x136] = static_cast<unsigned char>(Field_State[0x136] + 1);
    if (Field_State[0x136] <= 0xF0) return 0;
    Field_State[0x136] = 0;
    const int members = Field_MemberCount;
    if (members > 1) {
        bool busy = false;
        for (int i = 1; i < members; ++i)
            if (At(at::kLeader)[i * at::kMemberStride + 0x137] == 9) busy = true;
        if (busy) return 0;
    }
    if (!(g.rand() & 1)) return 0;
    Field_State[0x137] = 9;
    g.ensure_animation(Sprite_Current[8]);
    Sprite_Current[9] = 2;
    Sprite_Current[1] = 6;
    Sprite_Current[2] = 0;
    return 1;
}

// original 0x5302C0: the swap button (PSX FUN_801B667C) - with two members or
// more, bit 4 of neither flags byte, the button 0x903588 pressed and
// Party_CanSwap: state 3, sub-state 0, +3 = 0, Field_ScriptFlags2 bit 10.
extern "C" unsigned char __cdecl Field_LeaderSwapTest(void) {
    if (Field_MemberCount <= 1) return 0;
    if ((Byte(kFlags2Lo) | Byte(kFlagsLo)) & 0x10) return 0;
    if (!(Input_Pressed & Word(At(at::kButtonSwap)))) return 0;
    if (!g.can_swap()) return 0;
    Sprite_Current[1] = 3;
    Sprite_Current[2] = 0;
    Sprite_Current[3] = 0;
    Byte(kFlags2Hi) |= 4;
    return 1;
}

// original 0x530320: whether the party may swap (called only by
// Field_LeaderSwapTest; pointer-less, found by that call) - not under bits
// 0..2 of either flags byte, and no member after the leader in state 2 whose
// +2 is none of the three bytes at 0x660A34.
extern "C" unsigned char __cdecl Party_CanSwap(void) {
    if ((Byte(kFlags2Lo) | Byte(kFlagsLo)) & 7) return 0;
    const int members = Field_MemberCount;
    for (int i = 1; i < members; ++i) {
        const unsigned char* const m = At(at::kLeader) + i * at::kMemberStride;
        if (m[1] != 2) continue;
        int k = 0;
        while (k < 3 && m[2] != At(at::kSwapKinds)[k]) ++k;
        if (k == 3) return 0;
    }
    return 1;
}

// original 0x530380: the menu button (PSX FUN_801B6750) - Field_MenuButton
// pressed and bit 6 of Field_ScriptFlags clear: sound 0x105,
// Field_ScriptFlags2 bit 11, the pose, state 4, sub-state 0.
extern "C" unsigned char __cdecl Field_LeaderMenuTest(void) {
    if (!(Input_Pressed & Field_MenuButton)) return 0;
    if (Byte(kFlagsLo) & 0x40) return 0;
    g.play_effect(0x105);
    Byte(kFlags2Hi) |= 8;
    g.leader_animation(Sprite_Current[8]);
    Sprite_Current[1] = 4;
    Sprite_Current[2] = 0;
    return 1;
}

// original 0x5303E0 (PSX FUN_801B687C): the button 0x903586 held and bit 13
// of Field_ScriptFlags clear - Field_Request 4, the pose, state 1 sub-state 0.
extern "C" unsigned char __cdecl Field_LeaderRequest4Test(void) {
    if (Byte(kFlagsHi) & 0x20) return 0;
    if (!(Field_InputHeld & Word(At(at::kButtonHeld)))) return 0;
    Field_Request = 4;
    g.leader_animation(Sprite_Current[8]);
    Sprite_Current[1] = 1;
    Sprite_Current[2] = 0;
    return 1;
}

// original 0x530430 (PSX FUN_801B6908): the pad's 0x800 pressed, bit 13 of
// Field_ScriptFlags and bits 0x69 of Field_InputFlags clear - the pose,
// Field_Request 9, state 0xE sub-state 0.
extern "C" unsigned char __cdecl Field_LeaderRequest9Test(void) {
    if (Byte(kFlagsHi) & 0x20) return 0;
    if (Field_InputFlags & 0x69) return 0;
    if (!(Input_Pressed & 0x800)) return 0;
    g.leader_animation(Sprite_Current[8]);
    Field_Request = 9;
    Sprite_Current[1] = 0xE;
    Sprite_Current[2] = 0;
    return 1;
}

// original 0x530480: the direction held (PSX FUN_801B69AC) - Field_InputHeld's
// top four bits to +8 (0x1000 -> 0, 0x2000 -> 2, 0x3000 -> 1, 0x4000 -> 4,
// 0x6000 -> 3, 0x8000 -> 6, 0x9000 -> 7, 0xC000 -> 5), 1; any other
// combination 0 with +8 left as it was.
extern "C" unsigned char __cdecl Field_LeaderDirection(void) {
    unsigned char d;
    switch (Field_InputHeld & 0xF000) {
    case 0x1000: d = 0; break;
    case 0x2000: d = 2; break;
    case 0x3000: d = 1; break;
    case 0x4000: d = 4; break;
    case 0x6000: d = 3; break;
    case 0x8000: d = 6; break;
    case 0x9000: d = 7; break;
    case 0xC000: d = 5; break;
    default: return 0;
    }
    Sprite_Current[8] = d;
    return 1;
}

// original 0x530530: the first Effect_Objects record in use, of kind 0x17,
// that the point one Field_DirectionSteps ahead is in reach of - its index, or
// 0xFF. Pointer-less and in no list; called by Field_LeaderEffectTest and ten
// other sites (0x51D32E..0x524A9E, E8 scan).
// As the original has it: the direction indexes Field_DirectionSteps whole.
extern "C" unsigned char __cdecl Field_EffectAhead(void) {
    const unsigned char* const sc = Sprite_Current;
    const unsigned d = sc[8];
    const auto x = static_cast<int>(StepX(d) + static_cast<std::uint32_t>(Long(sc + 0x34)));
    const auto z = static_cast<int>(StepZ(d) + static_cast<std::uint32_t>(Long(sc + 0x38)));
    for (unsigned i = 0; i < 20; ++i) {
        const unsigned char* const e = Effect_Objects + i * 0x80;
        if (e[0] == 0 || e[5] != 0x17) continue;
        if (g.point_in_reach(x, z, static_cast<short>(Word(Sprite_Current + 0x3E)), 0, e)) return static_cast<unsigned char>(i);
    }
    return 0xFF;
}

// original 0x530600: the area's passage entry the leader faces, or null (PSX
// FUN_801B6C4C). The descriptor's +0x2C list of 8-byte entries, +0x31 the last
// index, walked from the last: entry[3] cells from entry[0], entry[1], along
// z when entry[2] bit 7, along x else, against the cell ahead - one or two
// cells of it (two when the sprite is between cells across the way it faces;
// with +0x70, a large sprite, the step ahead is 2 where the table says 1).
// As the original has it: cells compare as bytes, but a run's end
// (entry[0] or entry[1] + k) as a whole int, so a run past 0xFF never
// matches there; the direction indexes the cell table whole.
extern "C" const unsigned char* __cdecl Area_PassageAhead(void) {
    const unsigned char* const descriptor = Area_Descriptors[Game_AreaNumber];
    int n = descriptor[0x31];
    const unsigned char* list;
    std::memcpy(&list, descriptor + 0x2C, sizeof list);
    const unsigned char* const sc = Sprite_Current;
    for (const unsigned char* e = list + n * 8; n >= 0; --n, e -= 8) {
        for (int k = e[3] - 1; k >= 0; --k) {
            const unsigned large = static_cast<unsigned>(Long(sc + 0x70)) & 0xFF;
            const unsigned char d = sc[8];
            unsigned x, z;
            int span;
            if (large == 0) {
                x = Word(sc + 0x36) + static_cast<unsigned>(CellDX(d));
                z = Word(sc + 0x3A) + static_cast<unsigned>(CellDZ(d));
                span = (Word(sc + 0x34) == 0 && Word(sc + 0x38) == 0) ? 0 : 1;
            } else {
                const signed char cdx = CellDX(d), cdz = CellDZ(d);
                x = Word(sc + 0x36) + static_cast<unsigned>(cdx == 1 ? 2 : cdx);
                z = Word(sc + 0x3A) + static_cast<unsigned>(cdz == 1 ? 2 : cdz);
                span = (d == 1 || d == 5) ? Word(sc + 0x34) != 0 : Word(sc + 0x38) != 0;
            }
            const bool along_z = (e[2] & 0x80) != 0;
            for (int j = 0; j <= span; ++j) {
                if (along_z) {
                    if ((x & 0xFF) == e[0] && static_cast<int>(z & 0xFF) == e[1] + k) return e;
                } else if (static_cast<int>(x & 0xFF) == e[0] + k && (z & 0xFF) == e[1]) {
                    return e;
                }
                if (d == 1 || d == 5) ++x;
                else ++z;
            }
        }
    }
    return nullptr;
}

// original 0x530800 (PSX FUN_801B6EB4): the button 0x90358C pressed, not on
// foot, and Field_State +0x89 none of 2, 4, 9, 0xA - state 0xA sub-state 0,
// +0x137 = 8.
extern "C" unsigned char __cdecl Field_LeaderCheckTest(void) {
    const unsigned char kind = Field_State[0x89];
    if (kind == 2 || kind == 4 || kind == 0xA || kind == 9) return 0;
    if (!(Input_Pressed & Word(At(at::kButtonCheck)))) return 0;
    if (Field_InputFlags & 1) return 0;
    Sprite_Current[1] = 0xA;
    Sprite_Current[2] = 0;
    Field_State[0x137] = 8;
    return 1;
}

// original 0x530860 (PSX FUN_801B6F60): Field_State +0x89 3 or 6, an odd
// direction, running (the argument's low byte 4), and an effect ahead
// (Field_EffectAhead) - +0x137 = 8, +0xB the effect, state 0xA sub-state 1,
// +3 = 0.
extern "C" unsigned char __cdecl Field_LeaderEffectTest(unsigned pace) {
    const unsigned char kind = Field_State[0x89];
    if (kind != 3 && kind != 6) return 0;
    if (!(Sprite_Current[8] & 1)) return 0;
    if ((pace & 0xFF) != 4) return 0;
    const unsigned char effect = g.effect_ahead();
    if (effect == 0xFF) return 0;
    Field_State[0x137] = 8;
    Sprite_Current[0xB] = effect;
    Sprite_Current[1] = 0xA;
    Sprite_Current[2] = 1;
    Sprite_Current[3] = 0;
    return 1;
}

// original 0x5308D0 (PSX FUN_801B703C): with Field_State +0x89 2 and an odd
// direction, +0xB counts up; at 0x10, state 0xA sub-state 0, +0x137 = 8.
extern "C" unsigned char __cdecl Field_LeaderPushCount(void) {
    if (Field_State[0x89] != 2) return 0;
    unsigned char* sc = Sprite_Current;
    if (!(sc[8] & 1)) return 0;
    sc[0xB] = static_cast<unsigned char>(sc[0xB] + 1);
    sc = Sprite_Current;
    if (sc[0xB] != 0x10) return 0;
    sc[1] = 0xA;
    Sprite_Current[2] = 0;
    Field_State[0x137] = 8;
    return 1;
}

// original 0x530920: the confirm button (PSX FUN_801B7250) - unless the byte
// 0x905B82 is set: the cells around for 0x30 (answer unused); the object
// faced (tried twice, the facing put back between) is talked to; else a 0x51
// cell whose chapter hook (0x56D7A0) takes it; else a 0x50 / 0x53 / 0x55 /
// 0x52 cell and the passage faced, whose entry[2] low nibble is the facing or
// 8 - state 7. The facing is put back on every path that fails.
// The port's own: a passage search that finds nothing is tested (null);
// the PSX reads entry[2] through the null pointer.
extern "C" unsigned char __cdecl Field_LeaderTalkTest(void) {
    if (Byte(at::kBlockByte) != 0) return 0;
    if (!(Input_Pressed & Word(At(at::kButtonConfirm)))) return 0;
    const unsigned char facing = Sprite_Current[8];
    g.cell_around(0x30);
    unsigned char object = g.facing_object();
    if (object == 0xFF) {
        Sprite_Current[8] = facing;
        object = g.facing_object();
    }
    if (object != 0xFF) {
        g.talk_to(object);
        return 1;
    }
    if (g.cell_around(0x51) && g.cell_hook(Word(At(at::kScratch)), Word(At(at::kScratch2)))) {
        g.leader_animation(Sprite_Current[8]);
        Sprite_Current[2] = 0;
        return 1;
    }
    Sprite_Current[8] = facing;
    if (!g.cell_around(0x50) && !g.cell_around(0x53) && !g.cell_around(0x55) && !g.cell_around(0x52)) return 0;
    const unsigned char* const passage = g.passage_ahead();
    unsigned char* const sc = Sprite_Current;
    if (passage) {
        const unsigned char way = passage[2] & 0xF;
        if (sc[8] == way || way == 8) {
            sc[1] = 7;
            Sprite_Current[2] = 0;
            return 1;
        }
    }
    sc[8] = facing;
    return 0;
}

// original 0x530A50: the object the leader faces (PSX FUN_801B5148). Even
// directions try the two neighbouring odd ones first - turning to the one
// that answers - then their own; odd ones look (+0x70 + 2) steps ahead, and
// unless that cell is 0x30, one step (for a large sprite on a 0x30 cell the
// far point; for a small one, one step further on).
// As the original has it: the last try's direction and the odd one index
// Field_DirectionSteps whole.
extern "C" unsigned char __cdecl Field_FacingObject(void) {
    const unsigned char* sc = Sprite_Current;
    const unsigned char d = sc[8];
    if (!(d & 1)) {
        const unsigned left = (d - 1u) & 7;
        unsigned char r = g.object_ahead(static_cast<long>(StepX(left) + static_cast<std::uint32_t>(Long(sc + 0x34))),
                                         static_cast<long>(StepZ(left) + static_cast<std::uint32_t>(Long(sc + 0x38))), left);
        if (r != 0xFF) {
            Sprite_Current[8] = (Sprite_Current[8] - 1) & 7;
            return r;
        }
        sc = Sprite_Current;
        const unsigned right = (sc[8] + 1u) & 7;
        r = g.object_ahead(static_cast<long>(StepX(right) + static_cast<std::uint32_t>(Long(sc + 0x34))),
                           static_cast<long>(StepZ(right) + static_cast<std::uint32_t>(Long(sc + 0x38))), right);
        if (r != 0xFF) {
            Sprite_Current[8] = (Sprite_Current[8] + 1) & 7;
            return r;
        }
        sc = Sprite_Current;
        const unsigned ahead = sc[8];
        return g.object_ahead(static_cast<long>(StepX(ahead) + static_cast<std::uint32_t>(Long(sc + 0x34))),
                              static_cast<long>(StepZ(ahead) + static_cast<std::uint32_t>(Long(sc + 0x38))), ahead);
    }
    const std::uint32_t reach = sc[0x70] + 2u;
    std::uint32_t x = StepX(d) * reach + static_cast<std::uint32_t>(Long(sc + 0x34));
    std::uint32_t z = StepZ(d) * reach + static_cast<std::uint32_t>(Long(sc + 0x38));
    const unsigned char cell = g.byte_at(Whole(x), Whole(z));
    sc = Sprite_Current;
    if (cell != 0x30) {
        const unsigned e = sc[8];
        x = StepX(e) + static_cast<std::uint32_t>(Long(sc + 0x34));
        z = StepZ(e) + static_cast<std::uint32_t>(Long(sc + 0x38));
    } else if (sc[0x70] == 0) {
        const unsigned e = sc[8];
        x += StepX(e);
        z += StepZ(e);
    }
    return g.object_ahead(static_cast<long>(x), static_cast<long>(z), sc[8]);
}

// original 0x530BF0: the object at (x, z) the leader could reach (PSX
// FUN_801B53C8) - Sprite_ObjectAt with the leader's +0x70; 0xFF for none, for
// a kind-8 object, one whose word +0x88 is 0xFFFF, or one more than 0x100
// above or below. The third argument is not read.
// As the original has it: the index is compared SIGNED with 30, so an
// answer of 0x80 or more reads an object before Sprite_Objects.
extern "C" unsigned char __cdecl Field_ObjectInPath(long x, long z, unsigned) {
    const unsigned char r = g.object_at(x, z, Sprite_Current[0x70]);
    if (r == 0xFF) return 0xFF;
    const auto s = static_cast<signed char>(r);
    const unsigned char* const o = s < 30 ? Object(s) : Extra(s - 30);
    if (o[6] == 8 || Word(o + 0x88) == 0xFFFF) return 0xFF;
    const int rise = static_cast<short>(Word(o + 0x3E)) - static_cast<short>(Word(Sprite_Current + 0x3E));
    if (std::abs(rise) > 0x100) return 0xFF;
    return r;
}

// original 0x530C90 (PSX FUN_801B548C): Field_CellAroundNear, or 0x531120
// (Capcom's) for a sprite with +0x70.
extern "C" unsigned char __cdecl Field_CellAround(unsigned cell) {
    return Sprite_Current[0x70] == 0 ? g.cell_around_near(cell) : g.cell_around_large(cell);
}

namespace {
unsigned short W0() { return Word(At(at::kScratch)); }
unsigned short W2() { return Word(At(at::kScratch2)); }
void SetW0(unsigned v) { SetWord(At(at::kScratch), v); }
void SetW2(unsigned v) { SetWord(At(at::kScratch2), v); }
// The leader turned to face a cell found by an even direction's probe.
unsigned char TurnFromEven(unsigned char a, unsigned char b, unsigned char to_ab, unsigned char other) {
    unsigned char* const sc = Sprite_Current;
    sc[8] = (sc[8] == a || sc[8] == b) ? to_ab : other;
    return 1;
}
}  // namespace

// original 0x530CC0: a cell of the argument's value next to the leader, for
// a sprite without +0x70 (PSX FUN_801B5D74). Its whole cell (the word 0x903850
// / 0x903852 left at the one found): between cells on both axes, none; on a
// whole cell, the cell ahead's z column then its x row - an even direction
// turns to the diagonal between (1 / 5, 7 / 3), an odd one must already be
// that diagonal - then for an odd direction the two turned 90 degrees
// (turning to it); between cells on x, the two cells of the row ahead (the
// diagonals 7 / 3 look one row back and one on, turning to 1 / 5); on z, the
// same by columns (1 / 5 turning to 7 / 3).
// As the original has it: a match on a row whose direction is odd and not
// the diagonal wanted goes on to the next cell rather than failing; the
// direction indexes the cell table whole.
extern "C" unsigned char __cdecl Field_CellAroundNear(unsigned cell_arg) {
    const unsigned char cell = static_cast<unsigned char>(cell_arg);
    const unsigned char* const sc = Sprite_Current;
    const unsigned short fx = Word(sc + 0x34);
    if (fx != 0 && Word(sc + 0x38) != 0) return 0;
    const unsigned char d = sc[8];
    const unsigned short x = Word(sc + 0x36);
    const auto ahead_x = static_cast<unsigned short>(x + CellDX(d));
    const auto ahead_z = static_cast<unsigned short>(Word(sc + 0x3A) + CellDZ(d));
    if (fx == 0 && Word(sc + 0x38) == 0) {
        SetW0(x);
        SetW2(ahead_z);
        if (g.byte_at(static_cast<short>(Word(sc + 0x36)), static_cast<short>(ahead_z)) == cell) {
            const unsigned char* const s = Sprite_Current;
            SetW2(ahead_z);
            SetW0(Word(s + 0x36));
            const unsigned char e = s[8];
            if (!(e & 1)) return TurnFromEven(0, 2, 1, 5);
            return e == 1 || e == 5;
        }
        if (g.byte_at(static_cast<short>(ahead_x), static_cast<short>(Word(Sprite_Current + 0x3A))) == cell) {
            const unsigned char* const s = Sprite_Current;
            SetW0(ahead_x);
            SetW2(Word(s + 0x3A));
            const unsigned char e = s[8];
            if (!(e & 1)) return TurnFromEven(0, 6, 7, 3);
            return e == 7 || e == 3;
        }
        {
            const unsigned char* const s = Sprite_Current;
            if (!(s[8] & 1)) return 0;
            const unsigned char t = (s[8] + 2) & 7;
            const auto cx = static_cast<unsigned short>(Word(s + 0x36) + CellDX(t));
            SetW0(cx);
            const auto cz = static_cast<unsigned short>(Word(s + 0x3A) + CellDZ(t));
            SetW2(cz);
            if (g.byte_at(static_cast<short>(cx), static_cast<short>(cz)) == cell) {
                Sprite_Current[8] = t;
                return 1;
            }
        }
        const unsigned char* const s = Sprite_Current;
        const unsigned char t = (s[8] - 2) & 7;
        const auto cx = static_cast<unsigned short>(Word(s + 0x36) + CellDX(t));
        SetW0(cx);
        const auto cz = static_cast<unsigned short>(Word(s + 0x3A) + CellDZ(t));
        SetW2(cz);
        if (g.byte_at(static_cast<short>(cx), static_cast<short>(cz)) != cell) return 0;
        Sprite_Current[8] = t;
        return 1;
    }
    if (fx != 0) {   // between cells on x
        if (d == 7 || d == 3) {
            SetW0(x);
            SetW2(Word(sc + 0x3A) - 1u);
            for (int i = 0;;) {
                if (g.byte_at(static_cast<short>(W0()), static_cast<short>(W2())) == cell) {
                    Sprite_Current[8] = 1;
                    return 1;
                }
                SetW0(W0() + 1u);
                if (++i > 1) break;
            }
            const unsigned char* const s = Sprite_Current;
            SetW0(Word(s + 0x36));
            SetW2(Word(s + 0x3A) + 1u);
            for (int i = 0;;) {
                if (g.byte_at(static_cast<short>(W0()), static_cast<short>(W2())) == cell) {
                    Sprite_Current[8] = 5;
                    return 1;
                }
                ++i;
                SetW0(W0() + 1u);
                if (i > 1) return 0;
            }
        }
        SetW2(ahead_z);
        SetW0(x);
        for (int i = 0;;) {
            if (g.byte_at(static_cast<short>(W0()), static_cast<short>(W2())) == cell) {
                const unsigned char e = Sprite_Current[8];
                if (!(e & 1)) return TurnFromEven(0, 2, 1, 5);
                if (e == 1 || e == 5) return 1;
            }
            SetW0(W0() + 1u);
            if (++i > 1) return 0;
        }
    }
    // between cells on z
    if (d == 1 || d == 5) {
        SetW0(x - 1u);
        SetW2(Word(sc + 0x3A));
        for (int i = 0;;) {
            if (g.byte_at(static_cast<short>(W0()), static_cast<short>(W2())) == cell) {
                Sprite_Current[8] = 7;
                return 1;
            }
            SetW2(W2() + 1u);
            if (++i > 1) break;
        }
        const unsigned char* const s = Sprite_Current;
        SetW0(Word(s + 0x36) + 1u);
        SetW2(Word(s + 0x3A));
        for (int i = 0;;) {
            if (g.byte_at(static_cast<short>(W0()), static_cast<short>(W2())) == cell) {
                Sprite_Current[8] = 3;
                return 1;
            }
            ++i;
            SetW2(W2() + 1u);
            if (i > 1) return 0;
        }
    }
    SetW0(ahead_x);
    SetW2(Word(sc + 0x3A));
    for (int i = 0;;) {
        if (g.byte_at(static_cast<short>(W0()), static_cast<short>(W2())) == cell) {
            const unsigned char e = Sprite_Current[8];
            if (!(e & 1)) return TurnFromEven(0, 6, 7, 3);
            if (e == 7 || e == 3) return 1;
        }
        SetW2(W2() + 1u);
        if (++i > 1) return 0;
    }
}

// original 0x531660: talking to object n (PSX FUN_801B70D4) - 0..29
// Sprite_Objects, 30 and up Sprite_ObjectsExtra. Not of kind 9: its script
// context's +0x80 bit 5 (the "touched" bit Field_ObjectIdle 0x517F30 acts on), the pose,
// sub-state 0, Field_ScriptFlags bit 8. Of kind 9: unless on
// foot without bit 5 of Field_InputFlags, the facing pose (by 0x660A38[+0x89])
// unless bit 5, then Field_State +0x139 = n, state 0xB sub-state 0.
// As the original has it: n is not bounded above - an n past 33 is an object
// past the extra four.
extern "C" void __cdecl Field_LeaderTalkTo(unsigned index) {
    const unsigned char n = static_cast<unsigned char>(index);
    unsigned char* const o = n < 30 ? Object(n) : Extra(n - 30);
    if (o[6] != 9) {
        o[0x80] |= 0x20;
        g.leader_animation(Sprite_Current[8]);
        Sprite_Current[2] = 0;
        Byte(kFlagsHi) |= 1;
        return;
    }
    const unsigned char flags = Field_InputFlags;
    if ((flags & 1) && !(flags & 0x20)) return;
    if (!(flags & 0x20)) {
        if (At(at::kFacingPose)[Field_State[0x89]])
            g.ensure_animation(static_cast<unsigned char>((Sprite_Current[8] >> 1) + 0x34));
        else
            g.ensure_animation(Sprite_Current[8]);
    }
    Field_State[0x139] = n;
    Sprite_Current[1] = 0xB;
    Sprite_Current[2] = 0;
}

// original 0x531950: the cell the leader stands on, on foot (PSX
// FUN_801B766C). Confirm on 0xAE: 0x9039F3 = 0x37, sub-state 0; on a 0xAx
// cell (or under bit 12 of Field_ScriptFlags2): the pose unless bit 5, the
// cell's exit (0x531AF0), bit 12 cleared, Field_ChangeArea. The pad's 0x100
// cycles the byte 0x9045FA 0..2 (answer 0). The pad's 0x800 with an exit from
// 0x531820: the pose unless bit 5, the return point 0x904148 (x, z, area, a
// counter byte), bit 12 cleared, Field_ChangeArea. The button 0x90358C on
// 0xA0 / 0xA1: 0x9039F3 = 0x1B, the pose unless bit 5, sub-state 0.
extern "C" unsigned char __cdecl Field_LeaderCellEvent(void) {
    const unsigned char cell = g.byte_at(static_cast<short>(Word(Sprite_Current + 0x36)),
                                         static_cast<short>(Word(Sprite_Current + 0x3A)));
    const unsigned char flags = Field_InputFlags;
    if (!(flags & 1)) return 0;
    const unsigned pressed = Input_Pressed;
    if (Word(At(at::kButtonConfirm)) & pressed) {
        if (cell == 0xAE) {
            unsigned char* const sc = Sprite_Current;
            Byte(at::kTalkMode) = 0x37;
            sc[2] = 0;
            return 1;
        }
        if ((cell & 0xF0) != 0xA0 && !(Byte(kFlags2Hi) & 0x10)) return 0;
        if (!(flags & 0x20)) g.leader_animation(Sprite_Current[8]);
        g.exit_from_cell();
        const unsigned kind = Byte(at::kExitKind);
        const long z = Long(At(at::kExitZ));
        const long x = Long(At(at::kExitX));
        Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 & 0xEFFF);
        g.change_area(Word(At(at::kExitArea)), x, z, kind);
        return 1;
    }
    if (pressed & 0x100) {
        const auto cycle = static_cast<unsigned char>(Byte(at::kCellCycle) + 1);
        Byte(at::kCellCycle) = cycle;
        if (cycle > 2) Byte(at::kCellCycle) = 0;
        return 0;
    }
    if (pressed & 0x800) {
        if (!g.exit_gateway()) return 0;
        if (!(Field_InputFlags & 0x20)) g.leader_animation(Sprite_Current[8]);
        const unsigned char* const sc = Sprite_Current;
        unsigned char* const back = At(at::kReturnPoint);
        SetLong(back, Long(sc + 0x34));
        const auto count = static_cast<unsigned char>(back[0xA] + 1);
        const long z = Long(sc + 0x38);
        const unsigned short area = Game_AreaNumber;
        Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 & 0xEFFF);
        back[0xA] = count;
        const unsigned kind = Byte(at::kExitKind);
        SetLong(back + 4, z);
        const long exit_z = Long(At(at::kExitZ));
        SetWord(back + 8, area);
        const long exit_x = Long(At(at::kExitX));
        g.change_area(Word(At(at::kExitArea)), exit_x, exit_z, kind);
        return 1;
    }
    if (!(Word(At(at::kButtonCheck)) & pressed)) return 0;
    if (cell != 0xA1 && cell != 0xA0) return 0;
    Byte(at::kTalkMode) = 0x1B;
    if (!(flags & 0x20)) g.leader_animation(Sprite_Current[8]);
    Sprite_Current[2] = 0;
    return 1;
}

// original 0x531DF0: the objects the leader walks into (PSX FUN_801BEF00) -
// one Field_DirectionSteps ahead (two under bit 12 of Field_ScriptFlags), at
// the ground's height there unless that is more than 0x100 from the leader's;
// every object in use without +7 bit 7 in reach (Sprite_PointInReach, the
// leader's +0x70) has its word +0x9C counted up. If any: pace 3, +9 = 0.
// As the original has it: the direction indexes Field_DirectionSteps whole.
extern "C" unsigned char __cdecl Field_LeaderPushObjects(void) {
    unsigned char hit = 0;
    const unsigned char* const sc = Sprite_Current;
    const unsigned d = sc[8];
    const std::uint32_t dx = StepX(d), dz = StepZ(d);
    std::uint32_t x = static_cast<std::uint32_t>(Long(sc + 0x34)) + dx;
    std::uint32_t z = static_cast<std::uint32_t>(Long(sc + 0x38)) + dz;
    if (Byte(kFlagsHi) & 0x10) {
        x += dx;
        z += dz;
    }
    auto height = static_cast<short>(g.ground_at(static_cast<long>(x), static_cast<long>(z)));
    const auto own = static_cast<short>(Word(Sprite_Current + 0x3E));
    if (std::abs(static_cast<int>(own) - height) > 0x100) height = own;
    for (int i = 0; i < 30; ++i) {
        unsigned char* const o = Object(i);
        if (o[0] == 0 || (o[7] & 0x80)) continue;
        if (g.point_in_reach(static_cast<int>(x), static_cast<int>(z), height, Sprite_Current[0x70], o)) {
            SetWord(o + 0x9C, Word(o + 0x9C) + 1u);
            hit = 1;
        }
    }
    for (int i = 0; i < 4; ++i) {
        unsigned char* const o = Extra(i);
        if (o[0] == 0 || (o[7] & 0x80)) continue;
        if (g.point_in_reach(static_cast<int>(x), static_cast<int>(z), height, Sprite_Current[0x70], o)) {
            SetWord(o + 0x9C, Word(o + 0x9C) + 1u);
            hit = 1;
        }
    }
    if (hit) {
        Field_State[0x128] = 3;
        Sprite_Current[9] = 0;
    }
    return hit;
}

// --- the chapter's and the area's hooks ------------------------------------------

// original 0x539AC0: a chapter hook that declines - `xor al, al`. Thirteen
// .data references (chapter records and hook tables) and Area_StepHook's case
// for area 0xAE.
extern "C" unsigned char __cdecl Scenario_NoHook(long, long) { return 0; }

// original 0x56D700: the step's hook (no PSX twin found) - the chapter
// record's +8 (by Cond_ByteFA, signed), and if that declines, Area_StepHook.
// Sprite_Current is put back after. The answer is the hook's al,
// sign-extended, or Area_StepHook's.
extern "C" int __cdecl Scenario_StepHook(long x, long z) {
    const ChapterHooks* const record = g.chapter_hooks[Cond_ByteFA];
    unsigned char* const saved = Sprite_Current;
    int r = static_cast<signed char>(record->step(x, z));
    if (r == 0) r = g.area_step(x, z);
    Sprite_Current = saved;
    return r;
}

// original 0x56D750: the same with the record's +0xC and Area_ArriveHook -
// called when a walk lands (0x52E580's path; nothing in this file).
extern "C" int __cdecl Scenario_ArriveHook(long x, long z) {
    const ChapterHooks* const record = g.chapter_hooks[Cond_ByteFA];
    unsigned char* const saved = Sprite_Current;
    int r = static_cast<signed char>(record->arrive(x, z));
    if (r == 0) r = g.area_arrive(x, z);
    Sprite_Current = saved;
    return r;
}

// original 0x56E050: the area's step hook - Area_ReturnGate first (1 if it
// fires), then the handler of 38 areas (docs/event-ops.md section 6), its al
// sign-extended; 0 for any other area.
// As the original has it: area 0x4C's handler 0x40EB90 is called with no
// arguments pushed (it reads none).
extern "C" int __cdecl Area_StepHook(long x, long z) {
    if (g.return_gate(x, z)) return 1;
    static const unsigned short kAreas[kStepCases] = {
        0x24, 0x2A, 0x2E, 0x31, 0x3B, 0x4B, 0x4C, 0x61, 0x64, 0x69, 0x6A, 0x70, 0x74, 0x87, 0x8C, 0x8F, 0x91, 0x92, 0x96,
        0xAA, 0xAB, 0xAC, 0xAE, 0xAF, 0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBF, 0xC0, 0xC1, 0xC5,
    };
    const unsigned short area = Game_AreaNumber;
    for (unsigned c = 0; c < kStepCases; ++c) {
        if (kAreas[c] != area) continue;
        if (c == kNoArgsCase) return static_cast<signed char>(reinterpret_cast<unsigned char (__cdecl*)()>(reinterpret_cast<void*>(g.area_step_handlers[c]))());
        return static_cast<signed char>(g.area_step_handlers[c](x, z));
    }
    return 0;
}

// original 0x56E440: the return gate (PSX FUN_801A9A98, test for test) - outside area 0xBB,
// with Field_InputFlags exactly 0x40 and Cond_ByteFD 1, a step onto a 0xA1
// cell at z 0x10.8 or more while story flag 0x77 is set: the flag cleared,
// the return point's counter byte 0, Field_ChangeArea to the return point
// 0x904148 with 4.
extern "C" unsigned char __cdecl Area_ReturnGate(long x, long z) {
    if (Game_AreaNumber == 0xBB) return 0;
    if (Field_InputFlags != 0x40) return 0;
    if (Cond_ByteFD != 1) return 0;
    const auto row = static_cast<std::int32_t>(static_cast<std::uint32_t>(z) + 0x8000u) >> 16;
    if (g.byte_at(Whole(static_cast<std::uint32_t>(x)), static_cast<short>(row)) != 0xA1) return 0;
    if (z < 0x108000) return 0;
    if (!g.flags_test(At(at::kStoryFlags), 0x77)) return 0;
    At(at::kReturnPoint)[0xA] = 0;
    g.flags_clear(At(at::kStoryFlags), 0x77);
    const unsigned char* const back = At(at::kReturnPoint);
    g.change_area(Word(back + 8), Long(back), Long(back + 4), 4);
    return 1;
}

// original 0x56E4E0: the area's arrive hook - the handler of 8 areas, its al
// sign-extended; 0 for any other area.
extern "C" int __cdecl Area_ArriveHook(long x, long z) {
    static const unsigned short kAreas[kArriveCases] = {0x28, 0x30, 0x6F, 0x94, 0xA7, 0xA9, 0xAB, 0xAD};
    const unsigned short area = Game_AreaNumber;
    for (unsigned c = 0; c < kArriveCases; ++c)
        if (kAreas[c] == area) return static_cast<signed char>(g.area_arrive_handlers[c](x, z));
    return 0;
}

void EventOps_Inject() {
    if (bof3::WantsShadow("event_ops")) event_ops::SelfTest();
    BOF3_INJECT(EventOp_5x);
    BOF3_INJECT(EventOp_Dx);
    BOF3_INJECT(EventOp_9x);
    BOF3_INJECT(Field_LeaderWalk);
    BOF3_INJECT(Field_LeaderSetPace);
    BOF3_INJECT(Field_LeaderStepTick);
    BOF3_INJECT(Field_LeaderStepTarget);
    BOF3_INJECT(Field_LeaderStepCell);
    BOF3_INJECT(Field_EncounterDue);
    BOF3_INJECT(Field_LeaderIdleTest);
    BOF3_INJECT(Field_LeaderSwapTest);
    BOF3_INJECT(Party_CanSwap);
    BOF3_INJECT(Field_LeaderMenuTest);
    BOF3_INJECT(Field_LeaderRequest4Test);
    BOF3_INJECT(Field_LeaderRequest9Test);
    BOF3_INJECT(Field_LeaderDirection);
    BOF3_INJECT(Field_EffectAhead);
    BOF3_INJECT(Area_PassageAhead);
    BOF3_INJECT(Field_LeaderCheckTest);
    BOF3_INJECT(Field_LeaderEffectTest);
    BOF3_INJECT(Field_LeaderPushCount);
    BOF3_INJECT(Field_LeaderTalkTest);
    BOF3_INJECT(Field_FacingObject);
    BOF3_INJECT(Field_ObjectInPath);
    BOF3_INJECT(Field_CellAround);
    BOF3_INJECT(Field_CellAroundNear);
    BOF3_INJECT(Field_LeaderTalkTo);
    BOF3_INJECT(Field_LeaderCellEvent);
    BOF3_INJECT(Field_LeaderPushObjects);
    BOF3_INJECT(Scenario_NoHook);
    BOF3_INJECT(Scenario_StepHook);
    BOF3_INJECT(Scenario_ArriveHook);
    BOF3_INJECT(Area_StepHook);
    BOF3_INJECT(Area_ReturnGate);
    BOF3_INJECT(Area_ArriveHook);
}

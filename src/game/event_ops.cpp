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
#include "game/event_script.h"   // DIV-0191: g_mountain_passes
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace event_ops {

using move_script::At;
using move_script::Long;
using move_script::SetWord;
using move_script::Word;

namespace {
template <typename T> T Fn(std::uint32_t address) { return reinterpret_cast<T>(static_cast<std::uintptr_t>(address)); }
}  // namespace

const std::uint32_t kStepHandlers[kStepCases] = {
    bof3::addr::Area36_StepHook, bof3::addr::Area42_StepHook, bof3::addr::Area46_StepHook, bof3::addr::Area49_StepHook, bof3::addr::Area59_StepHook, bof3::addr::Area75_StepHook, bof3::addr::Area76_StepDisarmTail5, bof3::addr::Area97_StepHook, bof3::addr::Area100_StepHook, bof3::addr::Area105_StepHook,
    bof3::addr::Area106_StepHook, bof3::addr::Area112_StepHook, bof3::addr::Area116_StepHook, bof3::addr::Area135_StepHook, bof3::addr::Area140_StepHook, bof3::addr::Area143_StepHook, bof3::addr::Area145_StepHook, bof3::addr::Area146_StepHook, bof3::addr::Area150_StepHook, bof3::addr::Area170_StepHook,
    bof3::addr::Area171_StepHook, bof3::addr::Area172_StepHook, bof3::addr::Scenario_NoHook, bof3::addr::Area185_StepHook, bof3::addr::Area185_StepHook, bof3::addr::Area185_StepHook, bof3::addr::Area185_StepHook, bof3::addr::Area185_StepHook, bof3::addr::Area185_StepHook, bof3::addr::Area185_StepHook,
    bof3::addr::Area185_StepHook, bof3::addr::Area185_StepHook, bof3::addr::Area185_StepHook, bof3::addr::Area185_StepHook, bof3::addr::Area191_StepHook, bof3::addr::Area192_StepHook, bof3::addr::Area193_StepHook, bof3::addr::Area197_StepHook,
};
const std::uint32_t kArriveHandlers[kArriveCases] = {
    bof3::addr::Area40_ArriveHook, bof3::addr::Area48_ArriveHook, bof3::addr::Area111_ArriveHook, bof3::addr::Area148_ArriveHook, bof3::addr::Area167_ArriveHook, bof3::addr::Area169_ArriveHook, bof3::addr::Area171_ArriveHook, bof3::addr::Area173_ArriveHook,
};

const Callees kOriginals = {
    Field_LeaderTalkTest, Field_LeaderSwapTest, Field_LeaderMenuTest, Field_LeaderCheckTest,
    Field_LeaderRequest4Test, Field_LeaderRequest9Test,
    Field_LeaderDirection, Field_LeaderStepTarget, Field_LeaderPushObjects, Field_LeaderPushCount,
    Field_LeaderSetPace, Field_EncounterDue, Field_LeaderStepTick, Field_LeaderEffectTest, Field_LeaderStepCell,
    Party_CanSwap, Field_EffectAhead, Area_PassageAhead, Field_FacingObject, Field_ObjectInPath, Field_CellAround,
    Field_CellAroundNear, Field_LeaderTalkTo, Scenario_StepHook, Scenario_ArriveHook, Area_StepHook,
    Area_ArriveHook, Area_ReturnGate,
    reinterpret_cast<const ::bof3::Slot32<const ChapterHooks>*>(static_cast<std::uintptr_t>(::bof3::Img(Scenario_Hooks_addr))),
    Field_LeaderAnimation, Sprite_ClearSteps, Sprite_EnsureAnimation, AreaMap_ByteAt, Field_EncounterThresholdRoll,
    Party_Count, Area_ZoneAt, Rand, Sound_PlayEffect, Area_LinkAt, MapView_GroundAt, Sprite_ScriptTick,
    Sprite_ObjectAt, Sprite_PointInReach, Field_ChangeArea, Flags_Test, Flags_Clear, EventObj_Reset,
    Sprite_SetAnimationBank, AreaMap_Elevation, EventObj_SetFlags, Sprite_SetAnimation,
    Fn<void (__cdecl*)()>(::bof3::Code(fn::kFaceObject.v)),
    Fn<void (__cdecl*)()>(::bof3::Code(fn::kEncounterArea.v)),
    Fn<unsigned char (__cdecl*)()>(::bof3::Code(fn::kExitGateway.v)),
    Fn<void (__cdecl*)()>(::bof3::Code(fn::kExitFromCell.v)),
    Fn<unsigned char (__cdecl*)(unsigned)>(::bof3::Code(fn::kCellAroundLarge.v)),
    Fn<int (__cdecl*)(unsigned, unsigned)>(::bof3::Code(fn::kCellHook.v)),
    Fn<void (__cdecl*)(unsigned, unsigned, unsigned)>(::bof3::Code(fn::kSetCell.v)),
    Fn<unsigned char (__cdecl*)(unsigned, unsigned)>(::bof3::Code(fn::kPartyVisible.v)),
    Fn<unsigned char (__cdecl*)(long, long, unsigned, unsigned, unsigned)>(::bof3::Code(fn::kMemberFits.v)),
    Fn<void (__cdecl*)()>(::bof3::Code(fn::kLeaderMove.v)),
    Fn<void (__cdecl*)()>(::bof3::Code(fn::kLeaderFollow.v)),
    Fn<void (__cdecl*)()>(::bof3::Code(fn::kLeaderGround.v)),
    Fn<unsigned char (__cdecl*)()>(::bof3::Code(fn::kStepCode.v)),
    Fn<long (__cdecl*)(long, long, std::uint32_t)>(::bof3::Code(fn::kSlopeAt.v)),
    Fn<unsigned char (__cdecl*)(unsigned, unsigned)>(::bof3::Code(fn::kTestFB.v)),
    {Fn<Hook>(::bof3::CodePlace(kStepHandlers[0])), Fn<Hook>(::bof3::CodePlace(kStepHandlers[1])), Fn<Hook>(::bof3::CodePlace(kStepHandlers[2])), Fn<Hook>(::bof3::CodePlace(kStepHandlers[3])),
     Fn<Hook>(::bof3::CodePlace(kStepHandlers[4])), Fn<Hook>(::bof3::CodePlace(kStepHandlers[5])), Fn<Hook>(::bof3::CodePlace(kStepHandlers[6])), Fn<Hook>(::bof3::CodePlace(kStepHandlers[7])),
     Fn<Hook>(::bof3::CodePlace(kStepHandlers[8])), Fn<Hook>(::bof3::CodePlace(kStepHandlers[9])), Fn<Hook>(::bof3::CodePlace(kStepHandlers[10])), Fn<Hook>(::bof3::CodePlace(kStepHandlers[11])),
     Fn<Hook>(::bof3::CodePlace(kStepHandlers[12])), Fn<Hook>(::bof3::CodePlace(kStepHandlers[13])), Fn<Hook>(::bof3::CodePlace(kStepHandlers[14])),
     Fn<Hook>(::bof3::CodePlace(kStepHandlers[15])), Fn<Hook>(::bof3::CodePlace(kStepHandlers[16])), Fn<Hook>(::bof3::CodePlace(kStepHandlers[17])),
     Fn<Hook>(::bof3::CodePlace(kStepHandlers[18])), Fn<Hook>(::bof3::CodePlace(kStepHandlers[19])), Fn<Hook>(::bof3::CodePlace(kStepHandlers[20])),
     Fn<Hook>(::bof3::CodePlace(kStepHandlers[21])), Fn<Hook>(::bof3::CodePlace(kStepHandlers[22])), Fn<Hook>(::bof3::CodePlace(kStepHandlers[23])),
     Fn<Hook>(::bof3::CodePlace(kStepHandlers[24])), Fn<Hook>(::bof3::CodePlace(kStepHandlers[25])), Fn<Hook>(::bof3::CodePlace(kStepHandlers[26])),
     Fn<Hook>(::bof3::CodePlace(kStepHandlers[27])), Fn<Hook>(::bof3::CodePlace(kStepHandlers[28])), Fn<Hook>(::bof3::CodePlace(kStepHandlers[29])),
     Fn<Hook>(::bof3::CodePlace(kStepHandlers[30])), Fn<Hook>(::bof3::CodePlace(kStepHandlers[31])), Fn<Hook>(::bof3::CodePlace(kStepHandlers[32])),
     Fn<Hook>(::bof3::CodePlace(kStepHandlers[33])), Fn<Hook>(::bof3::CodePlace(kStepHandlers[34])), Fn<Hook>(::bof3::CodePlace(kStepHandlers[35])),
     Fn<Hook>(::bof3::CodePlace(kStepHandlers[36])), Fn<Hook>(::bof3::CodePlace(kStepHandlers[37]))},
    {Fn<Hook>(::bof3::CodePlace(kArriveHandlers[0])), Fn<Hook>(::bof3::CodePlace(kArriveHandlers[1])), Fn<Hook>(::bof3::CodePlace(kArriveHandlers[2])),
     Fn<Hook>(::bof3::CodePlace(kArriveHandlers[3])), Fn<Hook>(::bof3::CodePlace(kArriveHandlers[4])), Fn<Hook>(::bof3::CodePlace(kArriveHandlers[5])),
     Fn<Hook>(::bof3::CodePlace(kArriveHandlers[6])), Fn<Hook>(::bof3::CodePlace(kArriveHandlers[7]))},
};
Callees g = kOriginals;

}  // namespace event_ops

using namespace event_ops;

namespace {

// The count and the bank word (event_script.cpp's terms).
short Count() {
    short v;
    std::memcpy(&v, At(::bof3::Img(at::kScratch_addr)), sizeof v);
    return v;
}
void SetCount(unsigned v) { at::kScratch.w_00 = v; }
unsigned short Bank() { return at::kScratch2; }

unsigned char* Object(int n) { return rv::Bytes(&Sprite_Objects[n]); }
unsigned char* Extra(int n) { return rv::Bytes(&Sprite_ObjectsExtra[n]); }

// The record views (game/record_views.h): rv::Current<T>() reads
// Sprite_Current afresh at each use, as the original does.
// rv::As<records::SpriteObject>(p) views a
// sprite-family pointer; it is used below +0x80 only (the sprite head every
// family shares) unless the pointer is a Sprite_Objects / Sprite_ObjectsExtra /
// Sprite_Kind2 record: Object(n) is a Sprite_Objects record.

// A placement's x or z (event_script.cpp): the whole part from `whole`, the
// half (0x8000) if `half` is non-zero.
std::uint32_t Coordinate(unsigned char half, unsigned char whole) {
    return (half ? 0x8000u : 0u) | static_cast<std::uint32_t>(whole) << 16;
}

// The whole part of a 16.16 position, as the original pushes it: the high
// half of the dword, which the callee reads as a short.
short Whole(std::uint32_t v) { return static_cast<short>(v >> 16); }

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
    at::kScratch2 = static_cast<unsigned>(op[1]) << 8 | op[2];
    g.reset();
    g.set_bank(Bank());
    Sprite_Current[0] = 1;
    rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->x, static_cast<std::int32_t>(Coordinate(op[5], op[6])));
    rv::Store<std::int32_t>(rv::As<records::SpriteObject>(Object(Count()))->target_x, rv::Current<records::SpriteObject>()->x);
    rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->z, static_cast<std::int32_t>(Coordinate(op[7], op[8])));
    rv::Store<std::int32_t>(rv::As<records::SpriteObject>(Object(Count()))->target_z, rv::Current<records::SpriteObject>()->z);
    rv::As<records::SpriteObject>(Object(Count()))->best_score = 0;
    const long elevation = g.elevation(rv::Current<records::SpriteObject>()->x, rv::Current<records::SpriteObject>()->z);
    rv::Store<unsigned>(rv::Current<records::SpriteHead>()->height_whole, static_cast<unsigned>(elevation));
    Sprite_Current[1] = op[9];
    Sprite_Current[2] = op[0xB];
    Sprite_Current[8] = op[0] & 0xF;
    Sprite_Current[6] = op[0] >> 4;
    rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->work_18, op[0x10]);
    rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->work_1c, op[0x11]);
    unsigned char* const n = Object(Count());
    rv::As<records::SpriteObject>(n)->half_w = op[3];
    rv::As<records::SpriteObject>(n)->half_d = op[4];
    rv::As<records::SpriteObject>(n)->trigger_word = static_cast<unsigned>(op[0xD]) << 8 | op[0xE];
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

// original 0x57B530 (PSX 0x801A8338): op 9x, 13 bytes - places Sprite_Objects[count] as an
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
    at::kScratch2 = static_cast<unsigned>(op[1]) << 8 | op[2];
    g.reset();
    g.set_bank(Bank());
    Sprite_Current[0] = 1;
    rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->x, static_cast<std::int32_t>(Coordinate(op[4], op[5])));
    rv::Store<std::int32_t>(rv::As<records::SpriteObject>(Object(Count()))->target_x, rv::Current<records::SpriteObject>()->x);
    rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->z, static_cast<std::int32_t>(Coordinate(op[6], op[7])));
    rv::Store<std::int32_t>(rv::As<records::SpriteObject>(Object(Count()))->target_z, rv::Current<records::SpriteObject>()->z);
    rv::As<records::SpriteObject>(Object(Count()))->best_score = 0;
    const long elevation = g.elevation(rv::Current<records::SpriteObject>()->x, rv::Current<records::SpriteObject>()->z);
    rv::Store<unsigned>(rv::Current<records::SpriteHead>()->height_whole, static_cast<unsigned>(elevation));
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
        rv::As<records::SpriteObject>(Object(Count()))->trigger_word = 0;
        rv::As<records::SpriteObject>(sc)->work_18 = static_cast<std::int32_t>(static_cast<unsigned>(op[9]) << 8 | op[0xA]);
    }
    Sprite_Current[5] = op[8];
    Sprite_Current[0xB] = op[0xC];
    Sprite_Current[0x2A] = (Sprite_Current[7] >> 4) & 1;
    unsigned char* const sc = Sprite_Current;
    const unsigned bit = sc[5];
    if (At(::bof3::Img(at::kPlacedFlags))[bit >> 3] & (1u << (bit & 7))) {
        if (op[0xC] & 1) {
            sc[0] = 0;
            return;
        }
        if (rv::As<records::SpriteHead>(sc)->x_frac == 0 && rv::As<records::SpriteHead>(sc)->z_frac == 0) g.set_cell(rv::As<records::SpriteHead>(sc)->x_whole, rv::As<records::SpriteHead>(sc)->z_whole, 0x10);
        g.set_animation(1);
    } else {
        g.set_animation(0);
    }
    SetCount(static_cast<unsigned>(Count() + 1));
}

// --- the leader's walk ---------------------------------------------------------

// --- the tests Field_LeaderStand and Field_LeaderWalk make ---------------------

// --- the chapter's and the area's hooks ------------------------------------------

// original 0x56D700: the step's hook (no PSX twin found) - the chapter
// record's +8 (by Cond_ByteFA, signed), and if that declines, Area_StepHook.
// Sprite_Current is put back after. The answer is the hook's al,
// sign-extended, or Area_StepHook's.
extern "C" int __cdecl Scenario_StepHook(long x, long z) {
    const ChapterHooks* const record = event_ops::g.chapter_hooks[Cond_ByteFA];
    unsigned char* const saved = Sprite_Current;
    int r = static_cast<signed char>(reinterpret_cast<Hook>(static_cast<std::uintptr_t>(record->step))(x, z));
    if (r == 0) r = event_ops::g.area_step(x, z);
    Sprite_Current = saved;
    return r;
}

// original 0x56D750: the same with the record's +0xC and Area_ArriveHook -
// called when a walk lands (0x52E580's path; nothing in this file).
extern "C" int __cdecl Scenario_ArriveHook(long x, long z) {
    const ChapterHooks* const record = event_ops::g.chapter_hooks[Cond_ByteFA];
    unsigned char* const saved = Sprite_Current;
    int r = static_cast<signed char>(reinterpret_cast<Hook>(static_cast<std::uintptr_t>(record->arrive))(x, z));
    if (r == 0) r = event_ops::g.area_arrive(x, z);
    Sprite_Current = saved;
    return r;
}

// original 0x56E050: the area's step hook - Area_ReturnGate first (1 if it
// fires), then the handler of 38 areas (docs/event-ops.md section 6), its al
// sign-extended; 0 for any other area.
// As the original has it: area 0x4C's handler 0x40EB90 is called with no
// arguments pushed (it reads none).
// DIV-0191 (switch MountainPasses, docs/mountain-passes.md section 2): on
// the two maps with a new road, a step whose target is a door cell, a Mt.
// Levett band cell or the Zublo stored exit arms that place's own link
// (Area_LinkAt of its link cell), so the plate, confirm and the stored
// exit act on the right place whichever cell the party came from. The
// original arms only on arrival at the link cell itself.
static void MountainPasses_StepArm(unsigned short area, int cx, int cz) {
    struct Arm { unsigned char x, z, lx, lz; };
    static constexpr Arm kZublo[] = {{0x37, 0x1D, 0x37, 0x1E}, {0x36, 0x1B, 0x35, 0x1B}, {0x33, 0x1A, 0x33, 0x1B}};
    static constexpr Arm kLevett[] = {{0x38, 0x38, 0x39, 0x38}, {0x37, 0x39, 0x39, 0x38}, {0x38, 0x39, 0x39, 0x38},
                                      {0x37, 0x3A, 0x36, 0x3B}, {0x36, 0x3A, 0x36, 0x3B}};
    const Arm* arms = nullptr;
    unsigned n = 0;
    if (area == 87) { arms = kZublo; n = sizeof kZublo / sizeof kZublo[0]; }
    else if (area == 16) { arms = kLevett; n = sizeof kLevett / sizeof kLevett[0]; }
    for (unsigned i = 0; i < n; ++i)
        if (arms[i].x == cx && arms[i].z == cz) { event_ops::g.link_at(arms[i].lx, arms[i].lz); return; }
}

extern "C" int __cdecl Area_StepHook(long x, long z) {
    if (event_ops::g.return_gate(x, z)) return 1;
    if (g_mountain_passes) MountainPasses_StepArm(Game_AreaNumber, Whole(static_cast<std::uint32_t>(x)), Whole(static_cast<std::uint32_t>(z)));
    static const unsigned short kAreas[kStepCases] = {
        0x24, 0x2A, 0x2E, 0x31, 0x3B, 0x4B, 0x4C, 0x61, 0x64, 0x69, 0x6A, 0x70, 0x74, 0x87, 0x8C, 0x8F, 0x91, 0x92, 0x96,
        0xAA, 0xAB, 0xAC, 0xAE, 0xAF, 0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBF, 0xC0, 0xC1, 0xC5,
    };
    const unsigned short area = Game_AreaNumber;
    for (unsigned c = 0; c < kStepCases; ++c) {
        if (kAreas[c] != area) continue;
        if (c == kNoArgsCase) return static_cast<signed char>(reinterpret_cast<unsigned char (__cdecl*)()>(reinterpret_cast<void*>(event_ops::g.area_step_handlers[c]))());
        return static_cast<signed char>(event_ops::g.area_step_handlers[c](x, z));
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
    if (event_ops::g.byte_at(Whole(static_cast<std::uint32_t>(x)), static_cast<short>(row)) != 0xA1) return 0;
    if (z < 0x108000) return 0;
    if (!event_ops::g.flags_test(event_ops::At(::bof3::Img(Cond_Flags_addr) + 0xa0), 0x77)) return 0;
    event_ops::At(::bof3::Img(AreaW2d_ReturnPoint_addr))[0xA] = 0;
    event_ops::g.flags_clear(event_ops::At(::bof3::Img(Cond_Flags_addr) + 0xa0), 0x77);
    const unsigned char* const back = event_ops::At(::bof3::Img(AreaW2d_ReturnPoint_addr));
    event_ops::g.change_area(event_ops::Word(back + 8), event_ops::Long(back), event_ops::Long(back + 4), 4);
    return 1;
}

// original 0x56E4E0: the area's arrive hook - the handler of 8 areas, its al
// sign-extended; 0 for any other area.
extern "C" int __cdecl Area_ArriveHook(long x, long z) {
    static const unsigned short kAreas[kArriveCases] = {0x28, 0x30, 0x6F, 0x94, 0xA7, 0xA9, 0xAB, 0xAD};
    const unsigned short area = Game_AreaNumber;
    for (unsigned c = 0; c < kArriveCases; ++c)
        if (kAreas[c] == area) return static_cast<signed char>(event_ops::g.area_arrive_handlers[c](x, z));
    return 0;
}

void EventOps_Inject() {
    if (bof3::WantsShadow("event_ops")) event_ops::SelfTest();
    BOF3_INJECT(EventOp_5x);
    BOF3_INJECT(EventOp_Dx);
    BOF3_INJECT(EventOp_9x);
    BOF3_INJECT(Scenario_StepHook);
    BOF3_INJECT(Scenario_ArriveHook);
    BOF3_INJECT(Area_StepHook);
    BOF3_INJECT(Area_ReturnGate);
    BOF3_INJECT(Area_ArriveHook);
}

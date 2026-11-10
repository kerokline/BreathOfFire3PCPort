// Moved here by tools/move_funcs.py from event_leader.cpp, event_ops.cpp, field_e1.cpp, field_event.cpp, inventory_ops.cpp (docs/source-layout-round.md).
#include "game/field/field_4.h"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/event_leader.h"
#include "game/event_leader_callees.h"
#include "game/event_ops_callees.h"
#include "game/event_script.h"   // DIV-0191: g_mountain_passes
#include "game/field/field_4_callees.h"
#include "game/field_e1_callees.h"
#include "game/field_event.h"
#include "game/field_event_callees.h"
#include "game/inventory_ops.h"
#include "game/inventory_ops_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = field_4::at;

}  // namespace

using namespace event_leader;

using namespace event_ops;

namespace {

using namespace move_script;

}  // namespace

using namespace inventory_ops;

namespace event_leader {

using move_script::At;

using move_script::Long;

using move_script::SetWord;

using move_script::Word;

}  // namespace event_leader

namespace {

unsigned char& ByteEventLeader(std::uint32_t a) { return *event_leader::At(a); }

// A .data dispatch table, read at the call as the original reads it: the
// index is a whole byte and unchecked, so an index past the end calls what
// the next table holds (docs/event_leader.md section 3).
const ::bof3::CodeSlot<Handler>* TableEventLeader(std::uint32_t address) {   // 32-bit code words
    return reinterpret_cast<const ::bof3::CodeSlot<Handler>*>(static_cast<std::uintptr_t>(address));
}

unsigned char* Member(unsigned i) { return rv::Bytes(&ObjTrio[i]); }

// Field_DirectionSteps' pair for a direction - a whole byte into a table of
// eight, as the original indexes it.
std::uint32_t StepXEventLeader(unsigned d) { return static_cast<std::uint32_t>(event_leader::Long(reinterpret_cast<const unsigned char*>(Field_DirectionSteps) + d * 8u)); }

std::uint32_t StepZEventLeader(unsigned d) { return static_cast<std::uint32_t>(event_leader::Long(reinterpret_cast<const unsigned char*>(Field_DirectionSteps) + d * 8u + 4)); }

// `cdq / xor / sub`: the absolute value with the original's wrap (the most
// negative dword stays negative).
std::int32_t Abs(std::uint32_t v) {
    const std::uint32_t s = static_cast<std::uint32_t>(static_cast<std::int32_t>(v) >> 31);
    return static_cast<std::int32_t>((v ^ s) - s);
}

// A formation spot: the s8 pair at 0x6608F6 + 2 * (slot + 2 * direction),
// each << 15, plus the leader's position. The direction is a whole byte.
std::uint32_t FormationX(int slot, unsigned d) {
    const auto dx = static_cast<signed char>(event_leader::At(::bof3::Img(event_leader::at::kFormation))[(slot + static_cast<int>(d) * 2) * 2]);
    return (static_cast<std::uint32_t>(static_cast<std::int32_t>(dx)) << 15) + static_cast<std::uint32_t>(ObjTrio[0].x);
}

std::uint32_t FormationZ(int slot, unsigned d) {
    const auto dz = static_cast<signed char>(event_leader::At(::bof3::Img(event_leader::at::kFormation))[(slot + static_cast<int>(d) * 2) * 2 + 1]);
    return (static_cast<std::uint32_t>(static_cast<std::int32_t>(dz)) << 15) + static_cast<std::uint32_t>(ObjTrio[0].z);
}

// A member's shade bytes +0x5D..+0x5F: the first set, the other two copied
// from it through Sprite_Current read again each time.
void SetShade(unsigned char* m, unsigned char v) {
    m[0x5D] = v;
    {
        unsigned char* const sc = Sprite_Current;
        sc[0x5E] = sc[0x5D];
    }
    unsigned char* const sc = Sprite_Current;
    sc[0x5F] = sc[0x5D];
}

}  // namespace

namespace event_ops {

using move_script::At;

using move_script::Long;

using move_script::SetWord;

using move_script::Word;

}  // namespace event_ops

namespace {

std::uint32_t AddressEventOps(const void* p) { return Low32(p); }

unsigned char& Byte(std::uint32_t a) { return *event_ops::At(a); }

// Field_ScriptFlags' two bytes and Field_ScriptFlags2's, as the original
// tests them (`test ah, n` on a dword read, `test byte ptr`).
constexpr std::uint32_t kFlagsLo = 0x9039A2, kFlagsHi = 0x9039A3, kFlags2Lo = 0x905BA4, kFlags2Hi = 0x905BA5;

// Field_DirectionSteps' pair and the walk's unit pair for a direction - a
// whole byte into tables of eight, as the original indexes them.
std::uint32_t StepX(unsigned d) { return static_cast<std::uint32_t>(event_ops::Long(reinterpret_cast<const unsigned char*>(Field_DirectionSteps) + d * 8u)); }

std::uint32_t StepZ(unsigned d) { return static_cast<std::uint32_t>(event_ops::Long(reinterpret_cast<const unsigned char*>(Field_DirectionSteps) + d * 8u + 4)); }

std::int32_t WalkX(unsigned d) { return event_ops::Long(event_ops::At(::bof3::Img(MoveCmd_F9Steps_addr)) + d * 8u); }

std::int32_t WalkZ(unsigned d) { return event_ops::Long(event_ops::At(::bof3::Img(MoveCmd_F9Steps_addr)) + d * 8u + 4); }

// The cell step pair: two signed bytes per direction.
signed char CellDX(unsigned d) { return static_cast<signed char>(event_ops::At(::bof3::Img(event_ops::at::kCellDelta))[d * 2u]); }

signed char CellDZ(unsigned d) { return static_cast<signed char>(event_ops::At(::bof3::Img(event_ops::at::kCellDelta))[d * 2u + 1]); }

// The whole part of a 16.16 position, as the original pushes it: the high
// half of the dword, which the callee reads as a short.
short Whole(std::uint32_t v) { return static_cast<short>(v >> 16); }

// The original's eax at the slope call: Sprite_Current with its low byte
// replaced by the direction (`mov eax, [0x937F88]` / `mov al, [eax + 8]`).
// In inline assembly because clang 22.1.8 at -O2 folds the C++ spelling,
// `(p & 0xFFFFFF00) | p[8]`, to `p[8]` alone - a miscompile the start-up
// fuzz caught (docs/event-ops.md section 8).
std::uint32_t DirectionInPointer(const unsigned char* sc) {
    std::uint32_t v = AddressEventOps(sc);
    __asm__ volatile("movb 8(%1), %b0" : "+q"(v) : "r"(sc) : "memory");
    return v;
}

// `cdq` / `idiv`: the original's signed division, the divide fault included.
std::int32_t Idiv(std::int32_t dividend, std::int32_t divisor) {
    std::int32_t quotient;
    __asm__ volatile("cltd\n\tidivl %2" : "=a"(quotient) : "a"(dividend), "r"(divisor) : "edx", "cc");
    return quotient;
}

unsigned char& BFieldE1(std::uint32_t a) { return *move_script::At(a); }

std::uint16_t W(std::uint32_t a) { return move_script::Word(move_script::At(a)); }

std::int32_t L(std::uint32_t a) { return move_script::Long(move_script::At(a)); }

unsigned char* Sc() { return Sprite_Current; }

unsigned char* Fs() { return Field_State; }

unsigned char* ObjectFieldE1(unsigned i) { return rv::Bytes(&Sprite_Objects[i]); }

// The record views (game/record_views.h): in the leader states Sprite_Current
// is an ObjTrio record, and Field_State always is.
short S16(std::uint32_t v) { return static_cast<short>(v); }

// `cdq / xor / sub`: the absolute value with the original's wrap (the most
// negative stays negative).
std::int32_t Abs32(std::int32_t v) {
    const auto u = static_cast<std::uint32_t>(v);
    const std::uint32_t s = static_cast<std::uint32_t>(v >> 31);
    return static_cast<std::int32_t>((u ^ s) - s);
}

// --- the callees nobody owns, and FE2's, by address -----------------------------
using Void0 = void (__cdecl*)();

using Byte0 = unsigned char (__cdecl*)();

void Fe2(std::uint32_t address) { SH_WORD(Void0, address)(); }

unsigned char Fe2Al(std::uint32_t address) { return SH_WORD(Byte0, address)(); }

// A .data dispatch table read in place: the index is a whole byte and
// unchecked, as the original's; where the entry is not code (past the run of
// code-pointer tables the entry lies in) the original jumps into data - ours
// aborts. While the fuzz runs, the entries are its recorders (outside .text).
using Handler = void (__cdecl*)();

Handler CodeAt(std::uint32_t table, unsigned index, const char* who) {
    const std::uint32_t cell = table + 4u * index;
    const auto entry = static_cast<std::uint32_t>(L(cell));
    // A code word: hosted a VA in BOF3.exe's .text, in bof3x.exe a function of ours
    // the bind table names (common/image_base.h IsCodeWord); not the trap, not data.
    if (!scenario_harness::g_active && !::bof3::IsCodeWord(entry))
        bof3::Fatal("%s: index %u reads 0x%X at 0x%X (VA 0x%X), not code - past its table (the original jumps there)", who,
                    index, (unsigned)entry, (unsigned)cell, (unsigned)::bof3::VaOf(cell));
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(entry));
}

bool ScriptFlag8() { return ((BFieldE1(::bof3::Img(field_e1::at::kFlags2Lo)) | BFieldE1(::bof3::Img(field_e1::at::kFlagsLo))) & 8) != 0; }

unsigned char* ContentObject() { return ObjectFieldE1(Fs()[0x139]); }

using HandlerFieldEvent = field_event::Handler;

constexpr unsigned kMemberBytes = 0x14C, kActorBytes = 0xA4;

std::uint32_t Address(const volatile void* p) { return Low32(p); }

void Dispatch(const std::uint32_t* table, unsigned index) {
    reinterpret_cast<HandlerFieldEvent>(static_cast<std::uintptr_t>(table[index]))();
}

using u32 = std::uint32_t;

}  // namespace

// event_ops's callee seam, moved here from event_ops.cpp by tools/retire_shells.py
// (docs/source-layout-round.md 5): the group's lowest function lives in this file.
namespace event_ops {
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

// event_leader's callee seam, moved here from event_leader.cpp by tools/retire_shells.py
// (docs/source-layout-round.md 5): the group's lowest function lives in this file.
namespace event_leader {
namespace {

template <typename T> T Fn(std::uint32_t address) { return reinterpret_cast<T>(static_cast<std::uintptr_t>(address)); }

}  // namespace

const Callees kOriginals = {
    Field_LeaderStepLands, Field_SpotFree, Field_WayBlocked4, Field_CellsBlock, Field_WayBlocked,
    Field_LeaderStepTick, Field_EquipTick, Field_Bit20Tick, Field_FloorDamage, Field_Tile89, Field_Tile8A,
    Field_TileD0, Field_TileA4, MoveCmd_TestFB, AreaMap_ByteAt, Field_LeaderAnimation, Field_ChangeArea,
    Area_LinkAt, AreaMap_CellsAll, AreaMap_CellsNone, Sprite_EnsureAnimation, Field_JumpStart, Sprite_ClearSteps,
    Field_EncounterDue, Field_LeaderEffectTest, Scenario_ArriveHook, Field_Bit80Tick, Field_LeaderWalk,
    Member_ClearState, Sprite_ShadeFadeBegin, MapView_GroundAt, Sprite_SetAnimation, ObjTrio_SwapFields,
    Field_MemberSprite, Sprite_LoadPalette, MapView_SetElevation, Encounter_RollInitiative, Sprite_ScriptTick,
    Area_PassageAhead, EventScript_Run, Msg_OpenScript, Msg_OpenSystem, Flags_Test, Flags_Set, Sound_PlayEffect,
    Item_NamePtr, Inventory_Add, MoveCmd_TestFC, MapView_SlopeAt, Sprite_ObjectAt,
    Fn<unsigned char (__cdecl*)(long, long, unsigned)>(::bof3::Code(fn::kPathClear.v)),
    Fn<void (__cdecl*)(int)>(::bof3::Code(fn::kGiveZenny.v)),
    Fn<const unsigned char* (__cdecl*)()>(::bof3::Code(fn::kAreaExits.v)),
    Fn<unsigned char (__cdecl*)(long, long, long)>(::bof3::Code(fn::kWayBlockedWide.v)),
};
Callees g = kOriginals;

}  // namespace event_leader

// inventory_ops's callee seam, moved here from inventory_ops.cpp by tools/retire_shells.py
// (docs/source-layout-round.md 5): the group's lowest function lives in this file.
namespace inventory_ops {
namespace {

template <typename F>
F Fn(std::uint32_t address) { return reinterpret_cast<F>(static_cast<std::uintptr_t>(address)); }

}  // namespace

const Callees kOriginals = {
    Party_Count,
    AreaMap_Elevation,
    Sprite_ObjectAt,
    Gte_RotTransPers,
    Gte_RotMatrix,
    Gte_ApplyMatrix,
    Gte_SetRotMatrix,
    Gte_SetTransMatrix,
    Rand,
    MapView_GroundAt,
    Effect_FindFree,
    Fn<void (__cdecl*)(unsigned, unsigned, unsigned char*, unsigned)>(::bof3::Code(kSetAnimFrom.v)),
    Sprite_ScriptTick,
    Sprite_ShadeFadeBegin,
    Sprite_ShadeRaise,
    Sprite_ScriptTickOnce,
    Sprite_SetAnimation,
    Encounter_PlaceMember,
    Encounter_MemberClear,
    Encounter_MemberStands,
    Encounter_EnemyClear,
    AreaMap_CellNibble,
    Encounter_Project,
    Encounter_Apart,
    Encounter_CellFits,
    Encounter_PathClear,
    Short_Abs,
    Short_Sign,
    Encounter_StepOpen,
    Sprite_TurnSense,
    Sprite_ShadeLower,
};
Callees g = kOriginals;

}  // namespace inventory_ops

// original 0x52D8F0: the leader's frame (PSX 0x801B10F0), called by
// Field_MembersFrame for member 0 and by Field_PartyFirstFrame.
//
// As the original has it: the state is Sprite_Current +1, read after the two
// calls, a whole byte and unchecked - the table at 0x660918 has 15 entries,
// so a state of 15 or more jumps through whatever .data follows it. The
// original jumps (a tail call); the handler's eax is not kept, which no caller
// reads (docs/field-event.md, section 1).
extern "C" void __cdecl Field_LeaderFrame(void) {
    field_event::g.restore_clut();
    unsigned char* const timer = move_script::At(::bof3::Img(EventOps_BlockByte_addr));
    if (*timer != 0) *timer = static_cast<unsigned char>(*timer - 1);
    field_event::g.copy_input();
    Dispatch(field_event::g.leader_states, rv::Current<records::PartyMember>()->state);
}

// original 0x52D920, the leader's state 0 (PSX 0x801B1164): its counters
// cleared, grounded, and on to state 1 with its standing animation - or to
// state 0xC (Field_InputFlags bit 0x20) or 0xD (area 0xBD).
extern "C" void __cdecl Field_LeaderStart(void) {
    field_event::g.clear_steps();
    rv::Store<std::int32_t>(rv::Current<records::PartyMember>()->work_18, 0);
    rv::Store<std::int32_t>(rv::Current<records::PartyMember>()->work_1c, 0);
    rv::Store<std::int32_t>(rv::Current<records::PartyMember>()->work_20, 0);
    rv::Current<records::PartyMember>()->step_frames = 0;
    Sprite_Current[0xA] = 0;
    Sprite_Current[6] = 0;
    Sprite_Current[7] = 0;
    Sprite_Current[0xB] = 0;
    {
        const records::PartyMember* const c = rv::Current<records::PartyMember>();
        const long z = c->z, x = c->x;
        const long ground = field_event::g.ground_at(x, z);
        rv::Store<unsigned>(rv::Current<records::PartyMember>()->height_whole, static_cast<std::uint16_t>(ground));
    }
    Field_State[0x128] = 3;
    Field_State[0x136] = 0;
    Field_State[0x137] = 0;
    Field_State[0x138] = 0;
    rv::Current<records::PartyMember>()->animation = 0xFF;
    if (Field_InputFlags & 0x20) {
        rv::Current<records::PartyMember>()->flags = static_cast<unsigned char>(rv::Current<records::PartyMember>()->flags | 0x40);
        rv::Current<records::PartyMember>()->state = 0xC;
        rv::Current<records::PartyMember>()->step = 0;
        return;
    }
    if (Game_AreaNumber == 0xBD) {
        rv::Current<records::PartyMember>()->flags = static_cast<unsigned char>(rv::Current<records::PartyMember>()->flags | 0x40);
        rv::Current<records::PartyMember>()->state = 0xD;
        rv::Current<records::PartyMember>()->step = 0;
        return;
    }
    field_event::g.leader_animation(rv::Current<records::PartyMember>()->direction);
    rv::Current<records::PartyMember>()->state = static_cast<unsigned char>(rv::Current<records::PartyMember>()->state + 1);
    rv::Current<records::PartyMember>()->step = 0;
}

// original 0x52DA50, the leader's state 1 (PSX 0x801B1318): its sub-state
// through the three-entry table 0x660954, then Sprite_ScriptTick.
//
// As the original has it: the sub-state Sprite_Current +2 is unchecked (3 and
// above reach the .data after the table); Sprite_ScriptTick is a tail jump,
// whose al the original returns and no caller reads.
extern "C" void __cdecl Field_LeaderControl(void) {
    Dispatch(field_event::g.control_states, rv::Current<records::PartyMember>()->step);
    field_event::g.script_tick();
}

// original 0x52DA70, sub-state 0 of state 1 (PSX 0x801B1364): the leader
// standing. Each test in turn may take the frame; with a direction held and
// none of them, the leader starts walking (sub-state 1).
//
// As the original has it: the actor record tested for bit 0x20 is indexed by
// Field_State +0x148 directly, a whole byte (Field_PartySetUp and
// Field_PartyLoad store an actor index there).
extern "C" void __cdecl Field_LeaderStand(void) {
    if (Field_ScriptFlags & 0x100) return;
    if (Field_ScriptFlags2 & 0x40) return;
    if (Field_Request == 5) return;
    if (field_event::g.test_535120(1)) return;
    if (field_event::g.test_535240(1)) return;
    if (field_event::g.test_5301f0()) {
        Field_State[0x136] = 0;
        return;
    }
    if (Field_InputHeld != 0) {
        records::PartyMember* const state = rv::As<records::PartyMember>(Field_State);
        if (!(Field_ActorStates[state->char_index * kActorBytes] & 0x20)) rv::Bytes(state)[0x136] = 0;
    }
    for (auto* const test : field_event::g.tests)
        if (test()) return;
    if (Field_InputHeld == 0) return;
    field_event::g.walk();
    Sprite_Current[7] = 0;
    Sprite_Current[0xA] = 2;
    Sprite_Current[0xB] = 0;
    field_event::g.leader_animation(rv::Current<records::PartyMember>()->direction);
    rv::Current<records::PartyMember>()->states[1] = 1;
}

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
    if (Byte(::bof3::Img(kFlagsHi)) & 1) return;
    if (Byte(::bof3::Img(kFlags2Lo)) & 0x40) return;
    if (event_ops::g.talk_test()) return;
    if (event_ops::g.swap_test()) return;
    if (event_ops::g.menu_test()) return;
    if (event_ops::g.check_test()) return;
    if (event_ops::g.request4_test()) return;
    if (event_ops::g.request9_test()) return;
    {
        unsigned char* const sc = Sprite_Current;
        if (sc[0xA] != 0) {
            sc[0xA] = static_cast<unsigned char>(sc[0xA] - 1);
            return;
        }
    }
    if (!event_ops::g.direction()) {
        event_ops::g.leader_animation(Sprite_Current[8]);
        Field_State[0x128] = 3;
        Field_State[0x137] = 0;
        event_ops::g.clear_steps();
        Sprite_Current[7] = 0;
        Sprite_Current[9] = 0;
        Sprite_Current[0xB] = 0;
        unsigned char* const sc = Sprite_Current;
        MoveScript_F3Divisor = 0;
        MoveScript_FAWord = 0;
        sc[2] = 0;
        return;
    }
    switch (event_ops::g.step_target()) {
    case 0:
        if (event_ops::g.push_objects()) {
            Field_State[0x137] = 0;
            event_ops::g.leader_animation(Sprite_Current[8]);
            Sprite_Current[2] = 1;
            event_ops::g.push_count();
            return;
        }
        Sprite_Current[0xB] = 0;
        event_ops::g.set_pace();
        event_ops::g.leader_follow();
        event_ops::g.leader_ground();
        Sprite_Current[0xB] = 0;
        if (Field_InputFlags & 1) {
            if (Byte(::bof3::Img(kFlags2Hi)) & 0x30) {
                // bits 12 / 13: a step counter +6 runs down, or the cell ahead
                // (+9 steps on) is not 0 - either ends them.
                unsigned char* const sc = Sprite_Current;
                const std::uint32_t steps = sc[9];
                const std::uint32_t x = static_cast<std::uint32_t>(rv::As<records::SpriteObject>(sc)->step_x) * steps + static_cast<std::uint32_t>(rv::As<records::SpriteObject>(sc)->x);
                const std::uint32_t z = static_cast<std::uint32_t>(rv::As<records::SpriteObject>(sc)->step_z) * steps + static_cast<std::uint32_t>(rv::As<records::SpriteObject>(sc)->z);
                sc[6] = static_cast<unsigned char>(sc[6] - 1);
                if (Sprite_Current[6] == 0 || event_ops::g.byte_at(Whole(x), Whole(z)) != 0) {
                    Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 & 0xCFFF);
                    Field_StepCount = 0;
                    event_ops::g.zone_roll(0);
                }
            } else if (event_ops::g.encounter_due()) {
                event_ops::g.encounter_area();
                unsigned char* const sc = Sprite_Current;
                event_ops::at::kExitX = 0x190000;
                AreaEntry_PendingZ = 0x190000;
                AreaEntry_PendingFlags = 4;
                sc[6] = 4;
                Byte(::bof3::Img(kFlags2Hi)) |= 0x10;
            }
        }
        event_ops::g.leader_animation(static_cast<unsigned char>(Sprite_Current[8] + 8));
        Sprite_Current[9] = static_cast<unsigned char>(Sprite_Current[9] - 1);
        event_ops::g.step_tick();
        Field_State[0x137] = 1;
        Sprite_Current[2] = 2;
        return;
    case 1:
        if (event_ops::g.effect_test(Field_State[0x128])) return;
        Field_State[0x137] = 0;
        Field_State[0x128] = 3;
        Sprite_Current[9] = 0;
        event_ops::g.leader_animation(Sprite_Current[8]);
        Field_State[0x137] = 0;
        Sprite_Current[2] = 1;
        event_ops::g.push_count();
        return;
    case 2: {
        Field_State[0x128] = 3;
        event_ops::g.ensure_animation(Sprite_Current[8]);
        Sprite_Current[1] = 2;
        Sprite_Current[2] = 4;
        unsigned char* const sc = Sprite_Current;
        sc[3] = (sc[8] == 1 || sc[8] == 7) ? 0 : 1;
        Field_State[0x137] = 3;
        Field_State[0x138] |= 2;
        Byte(::bof3::Img(kFlags2Lo)) |= 0x80;
        return;
    }
    case 3:
        Field_State[0x128] = 2;
        event_ops::g.ensure_animation(Sprite_Current[8]);
        Sprite_Current[1] = 2;
        Sprite_Current[2] = 5;
        Field_State[0x137] = 2;
        Field_State[0x138] |= 2;
        Byte(::bof3::Img(kFlags2Hi)) |= 1;
        return;
    case 5:
        Field_State[0x128] = 3;
        event_ops::g.ensure_animation(Sprite_Current[8]);
        Field_State[0x137] = 4;
        Sprite_Current[1] = 2;
        Sprite_Current[2] = 3;
        return;
    case 6: {
        Field_State[0x128] = 3;
        event_ops::g.ensure_animation(Sprite_Current[8]);
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
        event_ops::g.leader_animation(Sprite_Current[8]);
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
    const unsigned n = static_cast<unsigned>(event_ops::g.party_count(0)) & 0xFF;
    for (unsigned i = 0; i < n; ++i) {
        const unsigned record = Character_IndexById[Cond_Flags[0xd2 + i]];
        if (event_ops::At(::bof3::Img(CharacterRecords_addr) + 0x10)[record * event_ops::at::kActorStride] & 0x80) {
            Field_State[0x128] = 3;
            return;
        }
    }
    if (Byte(::bof3::Img(kFlagsHi)) & 2) return;
    const unsigned held = (event_ops::Word(event_ops::At(::bof3::Img(Gfx_UnpackScratch_addr) + 0x1f002)) & Field_InputHeld) != 0 ? 1u : 0u;
    if ((held ^ FieldE2_RunDefault) == 0 || (Byte(::bof3::Img(kFlags2Lo)) & 0x20)) {
        Field_State[0x128] = 3;
        return;
    }
    Field_State[0x128] = 4;
}

// original 0x52E110 (PSX 0x801B1D98): the leader's sub-state 2 (Field_LeaderControl's table
// 0x660954 entry 2) - Field_State +0x137 = 1; with steps left in +9, one fewer
// and Field_LeaderStepTick; at 0, where the step lands. Both are tail jumps.
extern "C" void __cdecl Field_LeaderStepping(void) {
    Field_State[0x137] = 1;
    unsigned char* const sc = Sprite_Current;
    const unsigned char left = sc[9];
    if (left == 0) {
        event_leader::g.step_lands();
        return;
    }
    sc[9] = static_cast<unsigned char>(left - 1);
    event_leader::g.step_tick();
}

// original 0x52E140 (PSX 0x801B1DF4): one step's movement (0x536670, group V2's), then the
// sprite's script tick while running (PSX FUN_801B1DF4). The original's al is
// the tick's answer, or Field_State's low byte.
extern "C" unsigned char __cdecl Field_LeaderStepTick(void) {
    event_ops::g.leader_move();
    unsigned char* const fs = Field_State;
    if (fs[0x128] >= 4) return event_ops::g.script_tick();
    return static_cast<unsigned char>(AddressEventOps(fs));
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
    unsigned char code = event_ops::g.step_code();
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
            x = dx + static_cast<std::uint32_t>(rv::As<records::SpriteObject>(sc)->x);
            event_ops::at::kTargetX = static_cast<std::int32_t>(x);
            z = dz + static_cast<std::uint32_t>(rv::As<records::SpriteObject>(sc)->z);
            Scratch_Swap = static_cast<std::int32_t>(z);
        } else {
            x = static_cast<std::uint32_t>(rv::As<records::SpriteObject>(sc)->x) + 2 * dx;
            event_ops::at::kTargetX = static_cast<std::int32_t>(x);
            z = static_cast<std::uint32_t>(rv::As<records::SpriteObject>(sc)->z) + 2 * dz;
            Scratch_Swap = static_cast<std::int32_t>(z);
        }
    }
    if (Field_Request != 9) {
        const int taken = event_ops::g.step_hook(static_cast<long>(x), static_cast<long>(z));
        if (taken != 0) {
            if (Sprite_Current[1] == 2) return 0xFF;
            code = 0;
        }
        z = Scratch_Swap;
        x = static_cast<std::uint32_t>(event_ops::at::kTargetX);
    }
    const auto link = [] { return event_ops::g.link_at(event_ops::at::kCellX, event_ops::at::kCellZ); };
    switch (code) {
    case 0: return 1;
    case 2: {
        const auto slope = static_cast<short>(event_ops::g.slope_at(static_cast<long>(x), static_cast<long>(z),
                                                          DirectionInPointer(Sprite_Current)));
        event_ops::g.step_cell();
        if (event_ops::at::kScratch.b_00 == 0) {
            const int rise = static_cast<short>(rv::Current<records::SpriteHead>()->height_whole) - slope;
            if (std::abs(rise) > 0x80) {
                if (!link()) Field_Request = 5;
                MoveScript_F3Divisor = 0;
                return 1;
            }
            if (link()) return 0;
            Byte(::bof3::Img(kFlags2Lo)) |= 0x60;
            Field_Request = 5;
            return 0;
        }
        if (slope > 0x40) {
            if (!link() && Sprite_Current == event_ops::At(::bof3::Img(ObjTrio_addr))) {
                if (event_ops::g.test_fb(rv::Current<records::SpriteHead>()->x_whole, rv::Current<records::SpriteHead>()->z_whole)) {
                    event_ops::g.play_effect(event_ops::Word(event_ops::At(::bof3::Img(event_ops::at::kStepSounds)) + event_ops::at::kScratch.b_01 * 2u));
                    event_ops::g.ensure_animation(static_cast<unsigned char>((Sprite_Current[8] >> 1) + 0x10));
                } else {
                    event_ops::g.ensure_animation(Sprite_Current[8]);
                }
                Field_Request = 5;
            }
            MoveScript_F3Divisor = 0;
            return 4;
        }
        if (link()) return 0;
        Byte(::bof3::Img(kFlags2Lo)) |= 0x60;
        Field_Request = 5;
        return 0;
    }
    case 3: {
        const auto slope = static_cast<short>(event_ops::g.slope_at(static_cast<long>(x), static_cast<long>(z),
                                                          DirectionInPointer(Sprite_Current)));
        if (event_ops::at::kScratch.b_00 == 0) return 0;
        if (slope > 0x40) {
            unsigned char* const sc = Sprite_Current;
            MoveScript_F3Divisor = 0;
            sc[8] = facing;
            return 1;
        }
        event_ops::g.ground_at(event_ops::at::kTargetX, static_cast<std::int32_t>(Scratch_Swap));
        return 0;
    }
    case 4:
        event_ops::g.step_cell();
        if (link()) return 1;
        Byte(::bof3::Img(kFlags2Lo)) |= 0xC0;
        return 2;
    case 5:
        event_ops::g.step_cell();
        if (link()) return 1;
        Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 | 0x140);
        return 3;
    case 6: return 5;
    case 7: return 6;
    default: return 0;
    }
}

// original 0x52E510: the step target's cell, words 0x903854 / 0x903856 - the
// whole parts of 0x903858 / 0x90385C, one more on an axis the walk moves
// positively along when that position has a fraction. Called only by
// Field_LeaderStepTarget; pointer-less, found by its three calls (the list
// had it inside 0x52E160's 0x420 bytes).
extern "C" void __cdecl Field_LeaderStepCell(void) {
    const unsigned short x = SharedScratch.w_0a;
    unsigned char* const sc = Sprite_Current;
    event_ops::at::kCellX = x;
    if (SharedScratch.w_08 != 0 && WalkX(sc[8]) > 0) event_ops::at::kCellX = x + 1u;
    const unsigned short z = event_ops::Word(event_ops::At(::bof3::Img(Scratch_Swap_addr)) + 2);
    event_ops::at::kCellZ = z;
    if (event_ops::Word(event_ops::At(::bof3::Img(Scratch_Swap_addr))) != 0 && WalkZ(sc[8]) > 0) event_ops::at::kCellZ = z + 1u;
}

namespace event_leader {

// DIV-0121: 1 through PatchBytes("EdgeNoArea") in EventLeader_Inject, after
// the fuzz, which runs Capcom's rule.
std::uint32_t g_edge_no_area = 0;

}  // namespace event_leader

// original 0x52E580 (PSX 0x801B2464): where a step lands - reached only by 0x52E110's tail
// jump, in no list (event-ops.md section 11). The run counter +7 (up to 0x11
// while running, else 0); unless on foot the equipment tick; the two actor
// ticks and the floor; the four turn tiles, any one ending it (0xA4 with
// MoveCmd_TestFB); then by Field_InputFlags: on foot, cell 0xAF is the
// stored exit (Field_ChangeArea from 0x937F82.., 0x904EE0 = 0, 0x937F98 =
// 0xC), Field_ScriptFlags2 bit 13 stops (Field_ScriptFlags bit 8), cell 0xC0
// asks Area_LinkAt; with bit 1, the map's edge (0x20000, or the header's size
// less 3, on either axis, +0x70 steps on) leaves by 0x802290... Then a cell
// all 0xA7 is a link (Field_Request 5), none 0xA6 the return point 0x904148;
// Field_ScriptFlags2 bit 5 a jump; else Field_StepCount counted (twice on a
// diagonal), the steps cleared, the encounter test (Field_ScriptFlags2 bit 12,
// state 5), the effect test with the pace from before, the chapter's arrive
// hook (a stop), Field_Bit80Tick, and while a direction or the confirm button
// is held, Field_LeaderWalk again; else standing.
// As the original has it: the pace the effect test gets is the one read
// before the ticks, though Field_State +0x128 is set to 3 just before it.
extern "C" void __cdecl Field_LeaderStepLands(void) {
    {
        unsigned char* const sc = Sprite_Current;
        if (Field_State[0x128] == 4) {
            if (sc[7] <= 0x10) sc[7] = static_cast<unsigned char>(sc[7] + 1);
        } else {
            sc[7] = 0;
        }
    }
    const unsigned char pace = Field_State[0x128];
    if (!(Field_InputFlags & 1)) event_leader::g.equip_tick();
    event_leader::g.bit20_tick();
    event_leader::g.floor_damage();
    if (event_leader::g.tile89(0)) return;
    if (event_leader::g.tile8a(0)) return;
    if (event_leader::g.tile_d0()) return;
    if (event_leader::g.tile_a4()) {
        unsigned char* const sc = Sprite_Current;
        event_leader::g.test_fb(static_cast<short>(rv::As<records::SpriteHead>(sc)->x_whole), static_cast<short>(rv::As<records::SpriteHead>(sc)->z_whole));
        return;
    }
    const unsigned char input = Field_InputFlags;
    unsigned char* sc;
    bool stop = false;
    if (input & 1) {
        unsigned char code;
        {
            unsigned char* const s = Sprite_Current;
            code = event_leader::g.byte_at(static_cast<short>(rv::As<records::SpriteHead>(s)->x_whole), static_cast<short>(rv::As<records::SpriteHead>(s)->z_whole));
        }
        if (code == 0xAF) {
            event_leader::g.leader_animation(Sprite_Current[8]);
            event_leader::g.change_area(AreaW0b_Place, event_leader::at::kExitX, static_cast<std::int32_t>(AreaEntry_PendingZ), AreaEntry_PendingFlags);
            ModeFlow_AreaTransition = 0;
            Area_EnterTransition = 0xC;
            return;
        }
        if (ByteEventLeader(::bof3::Img(event_leader::at::kFlags2Hi)) & 0x20) {
            ByteEventLeader(::bof3::Img(event_leader::at::kFlagsHi)) |= 1;
            stop = true;
        } else {
            {
                unsigned char* const s = Sprite_Current;
                code = event_leader::g.byte_at(static_cast<short>(rv::As<records::SpriteHead>(s)->x_whole), static_cast<short>(rv::As<records::SpriteHead>(s)->z_whole));
            }
            if (code == 0xC0) {
                unsigned char* const s = Sprite_Current;
                event_leader::g.link_at(rv::As<records::SpriteHead>(s)->x_whole, rv::As<records::SpriteHead>(s)->z_whole);
            }
            sc = Sprite_Current;
        }
    } else {
        sc = Sprite_Current;
        if (input & 2) {
            const unsigned char d = sc[8];
            const std::uint32_t n = sc[0x70];
            const std::uint32_t x = StepXEventLeader(d) * n + static_cast<std::uint32_t>(rv::As<records::SpriteObject>(sc)->x);
            const std::uint32_t z = StepZEventLeader(d) * n + static_cast<std::uint32_t>(rv::As<records::SpriteObject>(sc)->z);
            const std::uint32_t edge_x = (static_cast<std::uint32_t>(AreaMap_Header[0]) - 3u) << 16;
            const std::uint32_t edge_z = (static_cast<std::uint32_t>(AreaMap_Header[1]) - 3u) << 16;
            if (x == 0x20000 || z == 0x20000 || x == edge_x || z == edge_z) {
                event_leader::g.ensure_animation(d);
                // DIV-0121: the area the edge leads to is the one the party
                // came from, and a game loaded here came from none (0xFFFF,
                // GameMode_Start's) - Capcom's code asks for area 0xFFFF and
                // Area_ZoneAtIn reads a null zone list. The edge holds instead.
                if (g_edge_no_area && AreaW2c_LastArea == 0xFFFF) {
                    Sprite_Current[2] = 1;
                    return;
                }
                event_leader::g.change_area(AreaW2c_LastArea, event_leader::Long(event_leader::At(::bof3::Img(MoveScript_TintRecords_addr) + 0x21c)), event_leader::Long(event_leader::At(::bof3::Img(MoveScript_TintRecords_addr) + 0x220)), 4);
                Sprite_Current[2] = 1;
                return;
            }
        }
    }
    if (!stop) {
        if (event_leader::g.cells_all(rv::As<records::SpriteObject>(sc)->x, rv::As<records::SpriteObject>(sc)->z, sc[0x70], 0xA7, 0)) {
            {
                unsigned char* const s = Sprite_Current;
                event_leader::g.link_at(rv::As<records::SpriteHead>(s)->x_whole, rv::As<records::SpriteHead>(s)->z_whole);
            }
            Field_Request = 5;
            event_leader::g.ensure_animation(Sprite_Current[8]);
            Sprite_Current[2] = 1;
            return;
        }
        {
            unsigned char* const s = Sprite_Current;
            if (!event_leader::g.cells_none(rv::As<records::SpriteObject>(s)->x, rv::As<records::SpriteObject>(s)->z, s[0x70], 0xA6, 0)) {
                event_leader::g.ensure_animation(Sprite_Current[8]);
                const std::int32_t z = event_leader::Long(event_leader::At(::bof3::Img(AreaW2d_ReturnPoint_addr) + 4));
                const std::int32_t x = static_cast<std::int32_t>(AreaW2d_ReturnPoint);
                const unsigned short area = event_leader::Word(event_leader::At(::bof3::Img(AreaW2d_ReturnPoint_addr) + 8));
                ByteEventLeader(::bof3::Img(event_leader::at::kReturnPoint) + 0xA) = 0;
                event_leader::g.change_area(area, x, z, 4);
                return;
            }
        }
        if (ByteEventLeader(::bof3::Img(event_leader::at::kFlags2Lo)) & 0x20) {
            Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 ^ 0x20);
            event_leader::g.jump_start();
            Sprite_Current[9] = static_cast<unsigned char>(Sprite_Current[9] - 1);
            event_leader::g.step_tick();
            Sprite_Current[2] = 2;
            return;
        }
        {
            unsigned char* const s = Sprite_Current;
            Field_StepCount = static_cast<unsigned short>(Field_StepCount + 1);
            if (s[8] == 2 || s[8] == 6) Field_StepCount = static_cast<unsigned short>(Field_StepCount + 1);
        }
        event_leader::g.clear_steps();
        const unsigned char input2 = Field_InputFlags;
        MoveScript_F3Divisor = 0;
        MoveScript_FAWord = 0;
        if (!(input2 & 1) && !(ByteEventLeader(::bof3::Img(event_leader::at::kFlags2Lo)) & 0x60) && event_leader::g.encounter_due()) {
            Field_State[0x128] = 3;
            event_leader::g.ensure_animation(Sprite_Current[8]);
            unsigned char* const s = Sprite_Current;
            ByteEventLeader(::bof3::Img(event_leader::at::kFlags2Hi)) |= 0x10;
            s[1] = 5;
            Sprite_Current[2] = 0;
            return;
        }
        Field_State[0x128] = 3;
        if (event_leader::g.effect_test(pace)) return;
        {
            unsigned char* const s = Sprite_Current;
            stop = event_leader::g.arrive_hook(rv::As<records::SpriteObject>(s)->x, rv::As<records::SpriteObject>(s)->z) != 0;
        }
        if (!stop) {
            event_leader::g.bit80_tick();
            if (!(ByteEventLeader(::bof3::Img(event_leader::at::kFlagsHi)) & 1) && !(ByteEventLeader(::bof3::Img(event_leader::at::kFlags2Lo)) & 0x40) &&
                (Field_InputHeld & (event_leader::Word(event_leader::At(::bof3::Img(Gfx_UnpackScratch_addr) + 0x1f000)) | 0xF000u)) != 0) {
                event_leader::g.leader_walk();
                return;
            }
            event_leader::g.leader_animation(Sprite_Current[8]);
            Field_State[0x137] = 0;
            Sprite_Current[2] = 1;
            return;
        }
    }
    event_leader::g.leader_animation(Sprite_Current[8]);
    Sprite_Current[2] = 1;
}

// original 0x52E9D0 (PSX 0x801B2AE0): the leader's state 3 (Field_LeaderSwapTest sets it) - a
// tail jump through the five sub-states at 0x660968 by Sprite_Current +2.
extern "C" void __cdecl Field_SwapState(void) { TableEventLeader(::bof3::Img(Field_SwapSteps_addr))[Sprite_Current[2]](); }

// original 0x52E9F0: sub-state 0 - each member in turn (Sprite_Current set to
// it) looks for its spot: the leader, and every member while ObjTrio +0x138
// bit 0 is set, on the leader's own spot; the others at their formation spot
// (slot i - 1, the leader's direction) if Field_SpotFree and the way there
// (0x52EC20) allow, else the leader's. A member other than the leader that
// found one is cleared, put in state 3, and starts its shade fade (+0x5C =
// 1). Then the leader's sub-state 1 while any member after it is in state 3,
// else 4.
// As the original has it: the member count is read again after each member;
// the leader's slot is the count at the start less one (the byte decremented
// in place, then read again); the first spot's height byte is the member's
// own +0x70 and the fallback's Sprite_Current's.
extern "C" void __cdecl Field_SwapGather(void) {
    unsigned char count = Field_MemberCount;
    const unsigned char dir = ObjTrio[0].direction;
    if (count != 0) {
        unsigned i = 0;
        do {
            unsigned char* const m = Member(i);
            Sprite_Current = m;
            auto slot = static_cast<signed char>(i - 1);
            if (slot < 0) {
                count = static_cast<unsigned char>(count - 1);
                slot = static_cast<signed char>(count);
            }
            bool placed;
            if ((rv::Bytes(ObjTrio)[0x138] & 1) || slot == 0) {
                placed = event_leader::g.spot_free(ObjTrio[0].x, ObjTrio[0].z, dir, m[0x70]) != 0;
            } else {
                placed = false;
                const std::uint32_t x = FormationX(slot, dir), z = FormationZ(slot, dir);
                if (event_leader::g.spot_free(static_cast<long>(x), static_cast<long>(z), dir, m[0x70])) {
                    unsigned char* const sc = Sprite_Current;
                    placed = event_leader::g.path_clear(event_leader::at::kTargetX, static_cast<std::int32_t>(Scratch_Swap), sc[0x70]) != 0;
                }
                if (!placed) {
                    unsigned char* const sc = Sprite_Current;
                    placed = event_leader::g.spot_free(ObjTrio[0].x, ObjTrio[0].z, dir, sc[0x70]) != 0;
                }
            }
            if (placed && i != 0) {
                event_leader::g.clear_state(i);
                Sprite_Current[1] = 3;
                event_leader::g.shade_begin();
                Sprite_Current[0x5C] = 1;
            }
            count = Field_MemberCount;
            ++i;
        } while (static_cast<int>(i) < static_cast<int>(count));
    }
    Sprite_Current = rv::Bytes(ObjTrio);
    for (unsigned j = 1; static_cast<int>(j) < static_cast<int>(count); ++j) {
        if (Member(j)[1] == 3) {
            ObjTrio[0].step = static_cast<unsigned char>(ObjTrio[0].step + 1);
            return;
        }
    }
    ObjTrio[0].step = 4;
}

// original 0x52EBA0: whether a member can stand at (x, z) - Sprite_Current +9
// held at 0 over Field_WayBlocked(x, z, raised, the ground there), and the
// ground within 0x40 of the leader's height; then the spot and its ground
// (called again) to 0x903858 / 0x90385C / 0x903852, +9 put back, 1. The
// third argument is not read.
// As the original has it: +9 is put back only on success, and into
// Sprite_Current read again after the second ground call.
extern "C" unsigned char __cdecl Field_SpotFree(long x, long z, unsigned, unsigned raised) {
    const long ground = event_leader::g.ground_at(x, z);
    unsigned char* const sc = Sprite_Current;
    const unsigned char steps = sc[9];
    sc[9] = 0;
    if (event_leader::g.way_blocked(x, z, raised, ground)) return 0;
    const int rise = static_cast<std::int16_t>(ObjTrio[0].height_whole) - static_cast<short>(ground);
    if (std::abs(rise) > 0x40) return 0;
    event_leader::at::kTargetX = static_cast<std::int32_t>(x);
    Scratch_Swap = static_cast<std::int32_t>(z);
    const long at_spot = event_leader::g.ground_at(x, z);
    unsigned char* const sc2 = Sprite_Current;
    event_leader::at::kScratch2 = static_cast<unsigned>(at_spot);
    sc2[9] = steps;
    return 1;
}

// original 0x52EC20: whether the way from the leader to (x, z) is open - from
// the leader's x (read again each step) in steps of half a unit toward x,
// |x - leader| >> 15 + 1 times, at z; then from the leader's z toward z at x.
// Each step: the ground (MapView_GroundAt) within 0x40 of the leader's height
// +0x3E, and not Field_WayBlocked(.., raised, the ground). al 1 open, 0 not.
// Called by Field_SwapGather and Field_SwapExchange (event_leader.cpp).
extern "C" unsigned char __cdecl Field_PathClear(long x, long z, unsigned raised) {
    const std::int32_t leader_z = ObjTrio[0].z;
    const std::int32_t along_x = Abs32(static_cast<std::int32_t>(x) - ObjTrio[0].x) >> 15;
    const std::int32_t along_z = Abs32(static_cast<std::int32_t>(z) - leader_z) >> 15;
    const short xh = S16(static_cast<std::uint32_t>(x) >> 16), zh = S16(static_cast<std::uint32_t>(z) >> 16);
    const short lxh = static_cast<short>(ObjTrio[0].x_whole), lzh = static_cast<short>(ObjTrio[0].z_whole);
    const int sx = xh > lxh ? 1 : xh == lxh ? 0 : -1;
    const int sz = zh > lzh ? 1 : zh == lzh ? 0 : -1;
    std::int32_t offset = 0;
    for (std::int32_t i = 0; i <= along_x; ++i) {
        const auto px = static_cast<long>(static_cast<std::uint32_t>(ObjTrio[0].x) + static_cast<std::uint32_t>(offset));
        const long ground = SH_CALL(MapView_GroundAt)(px, z);
        if (Abs32(static_cast<short>(ObjTrio[0].height_whole) - S16(static_cast<std::uint32_t>(ground))) > 0x40) return 0;
        if (SH_CALL(Field_WayBlocked)(px, z, raised, ground)) return 0;
        offset = static_cast<std::int32_t>(static_cast<std::uint32_t>(offset) + (static_cast<std::uint32_t>(sx) << 15));
    }
    offset = 0;
    for (std::int32_t i = 0; i <= along_z; ++i) {
        const auto pz = static_cast<long>(static_cast<std::uint32_t>(ObjTrio[0].z) + static_cast<std::uint32_t>(offset));
        const long ground = SH_CALL(MapView_GroundAt)(x, pz);
        if (Abs32(static_cast<short>(ObjTrio[0].height_whole) - S16(static_cast<std::uint32_t>(ground))) > 0x40) return 0;
        if (SH_CALL(Field_WayBlocked)(x, pz, raised, ground)) return 0;
        offset = static_cast<std::int32_t>(static_cast<std::uint32_t>(offset) + (static_cast<std::uint32_t>(sz) << 15));
    }
    return 1;
}

// original 0x52ED80: sub-state 1 - the leader's +9 counts up and it turns one
// direction on (Sprite_SetAnimation); each member after it in state 3 whose
// shade +0x5D (s8) is above -32 * +9 gets (+9 * 0xE0) in all three; at +9 =
// 4, sub-state 2.
extern "C" void __cdecl Field_SwapSpinOut(void) {
    Sprite_Current[9] = static_cast<unsigned char>(Sprite_Current[9] + 1);
    {
        unsigned char* const sc = Sprite_Current;
        sc[8] = static_cast<unsigned char>((sc[8] + 1) & 7);
    }
    event_leader::g.set_animation(Sprite_Current[8]);
    const unsigned char n = Sprite_Current[9];
    if (Field_MemberCount > 1) {
        const auto shade = static_cast<unsigned char>(n * 0xE0u);
        unsigned char i = 1;
        do {
            unsigned char* const m = Member(i);
            Sprite_Current = m;
            if (m[1] == 3 && static_cast<signed char>(m[0x5D]) > -static_cast<int>(n) * 32) SetShade(m, shade);
            i = static_cast<unsigned char>(i + 1);
        } while (i < Field_MemberCount);
    }
    Sprite_Current = rv::Bytes(ObjTrio);
    if (ObjTrio[0].step_frames == 4) ObjTrio[0].step = static_cast<unsigned char>(ObjTrio[0].step + 1);
}

// original 0x52EE70: sub-state 2, the exchange - ObjTrio_SwapFields(0, 1, 1)
// when member 1 is in state 3 (and (1, 2, 1) when member 2 is too, with three
// members), or (0, 2, 1) when only member 2 is; then each member in state 3:
// Sprite_Current and Field_State to it, Field_MemberSprite(its +0x89, slot),
// +0x138 from the Field_State of the start (+0x29 = 4 if its bit 0), its actor
// record copied to +0x80 and its index to +0x148, the party list's slot; the
// ones after the leader shade-fade, lose +0 bit 6, +0x5C = 1 and face the
// leader's direction ^ 4, and (unless ObjTrio +0x138 bit 0) find their spot as
// Field_SwapGather does - the leader and the others on the leader's; the spot
// found to +0x34 / +0x38, the ground +0x3E, the animation. Then Field_Kind2X
// / Z follow the leader, MoveScript_F3Divisor 0x20, MoveScript_FAWord the
// height left over (the larger distance moved >> 13) frames, the leader's +9
// = 4 and sub-state 3.
// As the original has it: the fallback's answer is not read - the spot is
// what 0x903858 held, found or not.
extern "C" void __cdecl Field_SwapExchange(void) {
    const unsigned char saved = Field_State[0x138];
    if (Member(1)[1] == 3) {
        event_leader::g.swap_fields(0, 1, 1);
        if (Field_MemberCount == 3 && Member(2)[1] == 3) event_leader::g.swap_fields(1, 2, 1);
    } else if (Field_MemberCount == 3 && Member(2)[1] == 3) {
        event_leader::g.swap_fields(0, 2, 1);
    }
    for (unsigned i = 0; static_cast<int>(i) < static_cast<int>(Field_MemberCount); ++i) {
        unsigned char* const m = Member(i);
        if (m[1] != 3) continue;
        Sprite_Current = m;
        Field_State = m;
        event_leader::g.member_sprite(m[0x89], i);
        Field_State[0x138] = saved;
        if (Field_State[0x138] & 1) Sprite_Current[0x29] = 4;
        {
            unsigned char* const fs = Field_State;
            std::memcpy(fs + 0x80, event_leader::At(::bof3::Img(CharacterRecords_addr) + Character_IndexById[fs[0x89]] * event_leader::at::kActorStride), event_leader::at::kActorStride);
        }
        {
            unsigned char* const fs = Field_State;
            fs[0x148] = Character_IndexById[fs[0x89]];
        }
        Cond_Flags[0xd2 + i] = Field_State[0x89];
        if (i != 0) {
            event_leader::g.shade_begin();
            Sprite_Current[0] = static_cast<unsigned char>(Sprite_Current[0] & 0xBF);
            Sprite_Current[0x5C] = 1;
            Sprite_Current[8] = static_cast<unsigned char>(ObjTrio[0].direction ^ 4);
        }
        if (!(rv::Bytes(ObjTrio)[0x138] & 1) && i != 0) {
            unsigned char* const sc = Sprite_Current;
            const unsigned char d = sc[8];
            const unsigned char raised = sc[0x70];
            const std::uint32_t x = FormationX(static_cast<int>(i), d), z = FormationZ(static_cast<int>(i), d);
            bool found = false;
            if (event_leader::g.spot_free(static_cast<long>(x), static_cast<long>(z), d, raised)) {
                unsigned char* const s = Sprite_Current;
                found = event_leader::g.path_clear(event_leader::at::kTargetX, static_cast<std::int32_t>(Scratch_Swap), s[0x70]) != 0;
            }
            if (!found) {
                unsigned char* const s = Sprite_Current;
                event_leader::g.spot_free(ObjTrio[0].x, ObjTrio[0].z, s[8], s[0x70]);
            }
        } else {
            unsigned char* const s = Sprite_Current;
            event_leader::g.spot_free(ObjTrio[0].x, ObjTrio[0].z, s[8], s[0x70]);
        }
        rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->x, event_leader::at::kTargetX);
        rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->z, static_cast<std::int32_t>(Scratch_Swap));
        long ground;
        {
            unsigned char* const s = Sprite_Current;
            ground = event_leader::g.ground_at(rv::As<records::SpriteObject>(s)->x, rv::As<records::SpriteObject>(s)->z);
        }
        rv::Store<unsigned>(rv::Current<records::SpriteHead>()->height_whole, static_cast<unsigned>(ground));
        event_leader::g.set_animation(Sprite_Current[8]);
    }
    Sprite_Current = rv::Bytes(ObjTrio);
    Field_State = rv::Bytes(ObjTrio);
    const auto lx = static_cast<std::uint32_t>(ObjTrio[0].x);
    const auto lz = static_cast<std::uint32_t>(ObjTrio[0].z);
    std::int32_t far = Abs(static_cast<std::uint32_t>(Field_Kind2X) - lx);
    Field_Kind2X = static_cast<long>(lx);
    const std::int32_t far_z = Abs(static_cast<std::uint32_t>(Field_Kind2Z) - lz);
    Field_Kind2Z = static_cast<long>(lz);
    MoveScript_F3Divisor = 0x20;
    if (far < far_z) far = far_z;
    const auto frames = static_cast<unsigned char>(far >> 13);
    if (frames != 0) {
        const auto rise = static_cast<short>(ObjTrio[0].height_whole - static_cast<unsigned short>(MapView_Elevation));
        MoveScript_FAWord = static_cast<unsigned short>(static_cast<int>(rise) / static_cast<int>(frames));
    } else {
        MoveScript_FAWord = 0;
    }
    ObjTrio[0].step_frames = 4;
    ObjTrio[0].step = static_cast<unsigned char>(ObjTrio[0].step + 1);
}

// original 0x52F1B0: sub-state 3 - the leader's +9 counts down and it turns
// on; each member after it in state 3 gets the shade (+9 * 0xE0); at +9 = 0
// each of them has its palette back (Sprite_LoadPalette from 0x80D380 + +5 *
// 64), +0 bit 5 and the shade, +0x5C and +9 cleared, the steps and +0x18 /
// +0x1C / +0x20 zeroed, and the leader goes to sub-state 4.
// As the original has it: Field_State is set to the leader before the +9
// test. (The member count is read again only after a member in state 3; as
// every call there is followed by that read, this cannot show - control I3.)
extern "C" void __cdecl Field_SwapSpinIn(void) {
    Sprite_Current[9] = static_cast<unsigned char>(Sprite_Current[9] - 1);
    {
        unsigned char* const sc = Sprite_Current;
        sc[8] = static_cast<unsigned char>((sc[8] + 1) & 7);
    }
    event_leader::g.set_animation(Sprite_Current[8]);
    unsigned char count = Field_MemberCount;
    const unsigned char n = Sprite_Current[9];
    if (count > 1) {
        unsigned char i = 1;
        do {
            unsigned char* const m = Member(i);
            Sprite_Current = m;
            if (m[1] == 3) {
                SetShade(m, static_cast<unsigned char>(n * 0xE0u));
                count = Field_MemberCount;
            }
            i = static_cast<unsigned char>(i + 1);
        } while (i < count);
    }
    Field_State = rv::Bytes(ObjTrio);
    Sprite_Current = rv::Bytes(ObjTrio);
    if (ObjTrio[0].step_frames != 0) return;
    if (count > 1) {
        unsigned char i = 1;
        do {
            unsigned char* const m = Member(i);
            Sprite_Current = m;
            if (m[1] == 3) {
                event_leader::g.load_palette(reinterpret_cast<unsigned short*>(event_leader::At(::bof3::Img(Gfx_ClutStripSource_addr) + 0x1e00 + m[5] * 64u)), 0);
                Sprite_Current[0] = static_cast<unsigned char>(Sprite_Current[0] & 0xDF);
                Sprite_Current[0x5D] = 0;
                Sprite_Current[0x5E] = 0;
                Sprite_Current[0x5F] = 0;
                Sprite_Current[0x5C] = 0;
                Sprite_Current[9] = 0;
                event_leader::g.clear_steps();
                rv::Current<records::SpriteObject>()->work_18 = 0;
                rv::Current<records::SpriteObject>()->work_1c = 0;
                rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->work_20, 0);
                count = Field_MemberCount;
            }
            i = static_cast<unsigned char>(i + 1);
        } while (i < count);
    }
    Sprite_Current = rv::Bytes(ObjTrio);
    ObjTrio[0].step = static_cast<unsigned char>(ObjTrio[0].step + 1);
}

// original 0x52F350 (PSX 0x801B3948): sub-state 4 - once Field_Kind2Hold is 0: Field_ScriptFlags2
// bit 10 (Field_LeaderSwapTest's) cleared, the view's elevation to the
// leader's height, state 1 sub-state 0.
extern "C" void __cdecl Field_SwapEnd(void) {
    if (Field_Kind2Hold != 0) return;
    unsigned char* const sc = Sprite_Current;
    Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 & 0xFBFF);
    event_leader::g.set_elevation(static_cast<int>(static_cast<std::uint16_t>(rv::As<records::SpriteHead>(sc)->height_whole)));
    Sprite_Current[1] = 1;
    Sprite_Current[2] = 0;
}

// original 0x52F390 (PSX 0x801B39B4): state 4 (Field_LeaderMenuTest sets it) - a tail jump
// through 0x66097C's two sub-states.
extern "C" void __cdecl Field_MenuState(void) { TableEventLeader(::bof3::Img(Field_MenuSteps_addr))[Sprite_Current[2]](); }

// original 0x52F3B0 (PSX 0x801B39F8): sub-state 0 - sub-state 1 and Field_Request 1 (the menu).
extern "C" void __cdecl Field_MenuOpen(void) {
    Sprite_Current[2] = 1;
    Field_Request = 1;
}

// original 0x52F3D0 (PSX 0x801B3A18): sub-state 1 - while Field_Request is 1 nothing; then
// Field_ScriptFlags2 bit 11 (Field_LeaderMenuTest's) cleared, state 1 sub-state 0.
extern "C" void __cdecl Field_MenuWait(void) {
    if (Field_Request == 1) return;
    unsigned char* const sc = Sprite_Current;
    Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 & 0xF7FF);
    sc[1] = 1;
    Sprite_Current[2] = 0;
}

// original 0x52F400 (PSX 0x801B3A68): state 5 (Field_LeaderStepLands sets it when
// Field_EncounterDue) - a tail jump through 0x660984's two sub-states.
extern "C" void __cdecl Field_EncounterState(void) { TableEventLeader(::bof3::Img(Field_EncounterSteps_addr))[Sprite_Current[2]](); }

// original 0x52F420 (PSX 0x801B3AAC): sub-state 0 - outside an event battle (0x904AAA 0),
// Encounter_RollInitiative; in one, every member after the leader cleared
// and put in state 5, and +0xB = 0xFF. Then sub-state 1.
// As the original has it: the member count is read again after each clear.
extern "C" void __cdecl Field_EncounterStart(void) {
    if (EventBattle_Id == 0) {
        event_leader::g.roll_initiative();
        Sprite_Current[2] = 1;
        return;
    }
    if (Field_MemberCount > 1) {
        unsigned i = 1;
        unsigned count;
        do {
            event_leader::g.clear_state(i);
            count = Field_MemberCount;
            Member(i)[1] = 5;
            ++i;
        } while (static_cast<int>(i) < static_cast<int>(count));
    }
    Sprite_Current[0xB] = 0xFF;
    Sprite_Current[2] = 1;
}

// original 0x52F490 (PSX 0x801B3B64): sub-state 1 - with an effect object in +0xB, wait for it
// to end (Effect_Objects[+0xB] +0 zero), then +0xB = 0xFF; with none, wait
// for member 1 (and member 2, with three) to be in state 5; then +0xB = 0,
// Field_ScriptFlags2 bit 12 cleared and Field_Request 3.
// As the original has it: +0xB indexes the 128-byte effect objects whole.
extern "C" void __cdecl Field_EncounterWait(void) {
    unsigned char* const sc = Sprite_Current;
    const unsigned char effect = sc[0xB];
    if (effect != 0xFF) {
        if (Effect_Objects[effect].flags != 0) return;
        sc[0xB] = 0xFF;
        return;
    }
    const unsigned char count = Field_MemberCount;
    if (count > 1) {
        if (Member(1)[1] != 5) return;
        if (count == 3 && Member(2)[1] != 5) return;
    }
    sc[0xB] = 0;
    Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 & 0xEFFF);
    Field_Request = 3;
}

// original 0x52F4F0 (PSX 0x801B3C30): leader state 6 - with Field_InputHeld not 0, Field_State
// +0x137 = 0; else the handler of 0x660A44 for the loaded party set (0x90412C
// & 0x7F; 19 entries, unchecked). Then, once +0x137 is 0: the pose +8, +9 =
// 0, member 0 cleared, state 1, +0xB = 0, Field_LeaderStand (a tail jump).
extern "C" void __cdecl Field_FormActionState(void) {
    if (Field_InputHeld != 0) {
        Fs()[0x137] = 0;
    } else {
        const unsigned set = AreaW0b_PartySet & 0x7Fu;
        CodeAt(::bof3::Img(Field_FormActions_addr), set, "Field_FormActionState")();
    }
    if (Fs()[0x137] != 0) return;
    SH_CALL(Sprite_EnsureAnimation)(rv::As<records::PartyMember>(Sc())->direction);
    rv::As<records::PartyMember>(Sc())->step_frames = 0;
    SH_CALL(Member_ClearState)(0);
    rv::As<records::PartyMember>(Sc())->state = 1;
    Sc()[0xB] = 0;
    SH_CALL(Field_LeaderStand)();
}

// original 0x52F570 (PSX 0x801B3CF8): which way Sprite_Current turns to face
// direction `target` (its byte): its direction +8 (plus 8, as a byte, when
// below the target) 1..4 past the target: 0xFF (anticlockwise, by the
// numbers), else 1. Ten call sites; each stores al in the sprite's +0xB.
extern "C" unsigned char __cdecl Sprite_TurnSense(unsigned target) {
    auto d = Sprite_Current[8];
    const auto a = static_cast<unsigned char>(target);
    if (d < a) d = static_cast<unsigned char>(d + 8);
    const u32 v = d, t = a;
    if (v >= t + 1 && v <= t + 4) return 0xFF;
    return 1;
}

// original 0x52F5C0 (PSX 0x801B3D4C): a party action's step (48 cells of the PartyAction run
// tables 0x65F998..): Sprite_ScriptTickOnce, and when it answers not 0,
// Field_State +0x137 = 0 (the action done).
extern "C" void __cdecl PartyAction_ScriptEnd(void) {
    if (SH_CALL(Sprite_ScriptTickOnce)() != 0) Fs()[0x137] = 0;
}

// original 0x52F5E0 (PSX 0x801B3D88): state 7 (Field_LeaderTalkTest sets it on a passage) -
// one of 0x66098C's four sub-states, then Sprite_ScriptTick (a tail jump).
extern "C" void __cdecl Field_PassageState(void) {
    TableEventLeader(::bof3::Img(Field_PassageSteps_addr))[Sprite_Current[2]]();
    event_leader::g.script_tick();
}

// original 0x52F600 (PSX 0x801B3DD4): sub-state 0 - the passage faced (Area_PassageAhead); the
// standing pose on foot, else the reaching one ((+8 >> 1) + 0x10); then by
// the entry's word +4 and kind +6: bit 15 clear or kind 0xFF, the system
// message 0 and sub-state 1; kind 0 the system message and kind 1 the
// script's (the word & 0xFFF), sub-state 2; kind 2 the area's event script
// (Area_Descriptors[area] +4's list, by the word & 0xFFF) run with
// Field_ActiveMember at a buffer of this frame's (+0x86 = 0xFF), its +0x88
// word's message unless 0xFFFF, +0x86 to Field_State +0x12A and the word to
// +0x12C, sub-state 3; any other kind sub-state 2. Field_Request 2.
// As the original has it: Field_ActiveMember is left pointing at the dead
// buffer, and its +0x88 is whatever the script left there (a stack slot the
// original does not initialise); a null passage is read through (ours aborts).
extern "C" void __cdecl Field_PassageOpen(void) {
    volatile unsigned char buffer[0xA4];
    const unsigned char* const e = event_leader::g.passage_ahead();
    if (Field_InputFlags & 1) event_leader::g.leader_animation(Sprite_Current[8]);
    else event_leader::g.ensure_animation(static_cast<unsigned char>((Sprite_Current[8] >> 1) + 0x10));
    if (e == nullptr) bof3::Fatal("Field_PassageOpen: no passage ahead (the original reads through null)");
    const unsigned short word = event_leader::Word(e + 4);
    if (!(word & 0x8000) || e[6] == 0xFF) {
        event_leader::g.open_system(0);
    } else {
        switch (e[6]) {
        case 0:
            event_leader::g.open_system(word & 0xFFFu);
            Sprite_Current[2] = static_cast<unsigned char>(Sprite_Current[2] + 1);
            break;
        case 1:
            event_leader::g.open_script(static_cast<unsigned short>(word & 0xFFFu));
            Sprite_Current[2] = static_cast<unsigned char>(Sprite_Current[2] + 1);
            break;
        case 2: {
            const unsigned area = Game_AreaNumber;
            Field_ActiveMember = const_cast<unsigned char*>(buffer);
            buffer[0x86] = 0xFF;
            const unsigned char* const descriptor = Area_Descriptors[area];
            const auto scripts = static_cast<std::uint32_t>(event_leader::Long(descriptor + 4));   // a table of 32-bit places
            event_leader::g.script_run(event_leader::At(static_cast<std::uint32_t>(event_leader::Long(event_leader::At(scripts + 4u * (event_leader::Word(e + 4) & 0xFFFu))))));
            const unsigned short message = static_cast<unsigned short>(buffer[0x88] | buffer[0x89] << 8);
            if (message != 0xFFFF) event_leader::g.open_script(static_cast<unsigned short>(message & 0xFFFu));
            Field_State[0x12A] = Field_ActiveMember[0x86];
            event_leader::SetWord(Field_State + 0x12C, event_leader::Word(e + 4));
            Sprite_Current[2] = static_cast<unsigned char>(Sprite_Current[2] + 1);
            Sprite_Current[2] = static_cast<unsigned char>(Sprite_Current[2] + 1);
            break;
        }
        default:
            Sprite_Current[2] = static_cast<unsigned char>(Sprite_Current[2] + 1);
            break;
        }
    }
    Field_Request = 2;
    Sprite_Current[2] = static_cast<unsigned char>(Sprite_Current[2] + 1);
}

// original 0x52F760 (PSX 0x801B3FC0): sub-state 1 - once Field_Request is 0, the passage's
// content: its flag (+4, in 0x90410C) already set, system message 1; kind
// 0xFF zenny (+5 * 40, 0x5307C0) with the count 0x90413C and the flag (unless
// 0xFF) and sound 0x106; else the item (category +6, id +5): its name to
// Text_Records, and Inventory_Add - taken: the flag and the count, sound
// 0x106, message 2; no room: message 3. Field_Request 2, sub-state 2.
// As the original has it: the zenny path counts before it sets the flag, the
// item path after; the entry is read again after every call; a null passage
// is read through (ours aborts).
extern "C" void __cdecl Field_PassageTake(void) {
    if (Field_Request != 0) return;
    const unsigned char* const e = event_leader::g.passage_ahead();
    if (e == nullptr) bof3::Fatal("Field_PassageTake: no passage ahead (the original reads through null)");
    if (event_leader::g.flags_test(event_leader::At(::bof3::Img(event_leader::at::kPassageFlags)), e[4])) {
        event_leader::g.open_system(1);
    } else if (e[6] == 0xFF) {
        Rest4a_CountD = static_cast<std::int32_t>(Rest4a_CountD) + 1;
        if (e[4] != 0xFF) event_leader::g.flags_set(event_leader::At(::bof3::Img(event_leader::at::kPassageFlags)), e[4]);
        event_leader::g.play_effect(0x106);
        event_leader::g.give_zenny(e[5] * 40);
    } else {
        const unsigned char* const name = event_leader::g.item_name(e[6], static_cast<unsigned>(event_leader::Word(e + 4)) >> 8);
        std::memcpy(event_leader::At(::bof3::Img(bof3::addr::Text_Records)), name, 16);
        if (event_leader::g.inventory_add(e[6], static_cast<unsigned>(event_leader::Word(e + 4)) >> 8, 1)) {
            if (e[4] != 0xFF) {
                event_leader::g.flags_set(event_leader::At(::bof3::Img(event_leader::at::kPassageFlags)), e[4]);
                Rest4a_CountD = static_cast<std::int32_t>(Rest4a_CountD) + 1;
            }
            event_leader::g.play_effect(0x106);
            event_leader::g.open_system(2);
        } else {
            event_leader::g.open_system(3);
        }
    }
    unsigned char* const sc = Sprite_Current;
    Field_Request = 2;
    sc[2] = 2;
}

// original 0x52F8A0 (PSX 0x801B4154): sub-state 2 - once Field_Request is 0, the standing pose
// (Field_LeaderAnimation on foot, else Sprite_EnsureAnimation) and state 1.
extern "C" void __cdecl Field_PassageEnd(void) {
    if (Field_Request != 0) return;
    if (Field_InputFlags & 1) event_leader::g.leader_animation(Sprite_Current[8]);
    else event_leader::g.ensure_animation(Sprite_Current[8]);
    Sprite_Current[1] = 1;
    Sprite_Current[2] = 0;
}

// original 0x52F8F0 (PSX 0x801B41E8): passage step 3 (Field_PassageSteps[3]) - unless
// Field_Request is 2: an object record on the stack (0xA4 bytes, only +0x86
// and +0x88 written: Field_State +0x12A and the word +0x12C), handed to
// Field_ObjectTrigger unless +0x86 is 0xFF; step 2.
// As the original has it: the rest of the record is whatever the frame held
// (the trigger reads +0x89, the word's high byte, and hands the record on).
extern "C" void __cdecl Field_PassageTrigger(void) {
    if (Field_Request == 2) return;
    alignas(4) unsigned char object[0xA4];
    std::memset(object, 0, sizeof object);
    const unsigned char* const fs = Fs();
    object[0x86] = fs[0x12A];
    move_script::SetWord(object + 0x88, move_script::Word(fs + 0x12C));
    if (object[0x86] != 0xFF) SH_AT(void (__cdecl*)(unsigned char*), field_e1::at::kObjectTrigger.v)(object);
    rv::As<records::PartyMember>(Sc())->step = 2;
}

// original 0x52F950 (PSX 0x801B4258): leader state 8, the jump - jmp Field_JumpSteps[+2].
extern "C" void __cdecl Field_JumpState(void) { CodeAt(::bof3::Img(Field_JumpSteps_addr), rv::As<records::PartyMember>(Sc())->step, "Field_JumpState")(); }

// original 0x52F970 (PSX 0x801B429C): jump step 0 - 0x535FC0 (the first pose), step 2.
extern "C" void __cdecl Field_JumpBegin(void) {
    Fe2(field_e1::at::kJumpPose.v);
    rv::As<records::PartyMember>(Sc())->step = 2;
}

// original 0x52F980 (PSX 0x801B42C8): jump step 1 - jmp Field_JumpOutSteps[+3].
extern "C" void __cdecl Field_JumpOut(void) { CodeAt(::bof3::Img(Field_JumpOutSteps_addr), rv::As<records::PartyMember>(Sc())->state3, "Field_JumpOut")(); }

// original 0x52F9A0 (PSX 0x801B430C): jump-out 0 - 0x535FE0 (Field_JumpSetUp's caller); with
// +5 0 and neither script flag's bit 3, Field_JumpCamera; +3 = 1.
extern "C" void __cdecl Field_JumpOut0(void) {
    Fe2(field_e1::at::kJumpSetUp.v);
    if (rv::As<records::PartyMember>(Sc())->actor == 0 && !ScriptFlag8()) SH_CALL(Field_JumpCamera)();
    rv::As<records::PartyMember>(Sc())->state3 = 1;
}

// original 0x52F9E0 (PSX 0x801B4380) / 0x52FA00 / 0x52FA20: jump-out 1..3 - FE2's step, and
// when it answers al not 0, +3 one on.
extern "C" void __cdecl Field_JumpOut1(void) {
    if (Fe2Al(field_e1::at::kJumpOut1.v)) rv::As<records::PartyMember>(Sc())->state3 = 2;
}

extern "C" void __cdecl Field_JumpOut2(void) {
    if (Fe2Al(field_e1::at::kJumpOut2.v)) rv::As<records::PartyMember>(Sc())->state3 = 3;
}

extern "C" void __cdecl Field_JumpOut3(void) {
    if (Fe2Al(field_e1::at::kJumpOut3.v)) rv::As<records::PartyMember>(Sc())->state3 = 4;
}

// original 0x52FA40 (PSX 0x801B4434): jump-out 4 - 0x536170; when al: step 2, +3 = 0.
extern "C" void __cdecl Field_JumpOut4(void) {
    if (!Fe2Al(field_e1::at::kJumpOut4.v)) return;
    rv::As<records::PartyMember>(Sc())->step = 2;
    rv::As<records::PartyMember>(Sc())->state3 = 0;
}

// original 0x52FA60 (PSX 0x801B4480): jump step 2 - by Field_InputHeld: bit 12 0x5364D0, else
// bit 14 0x536550, else any bit of the button map word 0x903580 0x5365D0; then
// unless a script flag's bit 3, MapView_SetElevation(+0x3E) and
// Field_JumpCamera (a tail jump).
extern "C" void __cdecl Field_JumpAir(void) {
    const std::uint16_t held = Field_InputHeld;
    if (held & 0x1000) Fe2(field_e1::at::kJumpAirA.v);
    else if (held & 0x4000) Fe2(field_e1::at::kJumpAirB.v);
    else if (W(::bof3::Img(field_e1::at::kButtonMap)) & held) Fe2(field_e1::at::kJumpAirC.v);
    if (ScriptFlag8()) return;
    SH_CALL(MapView_SetElevation)(static_cast<short>(rv::As<records::PartyMember>(Sc())->height_whole));
    SH_CALL(Field_JumpCamera)();
}

// original 0x52FAC0 (PSX 0x801B4538): jump step 3 - jmp Field_JumpInSteps[+3].
extern "C" void __cdecl Field_JumpIn(void) { CodeAt(::bof3::Img(Field_JumpInSteps_addr), rv::As<records::PartyMember>(Sc())->state3, "Field_JumpIn")(); }

// original 0x52FAE0 (PSX 0x801B457C): jump-in 0 - 0x536290, +3 = 1.
extern "C" void __cdecl Field_JumpIn0(void) {
    Fe2(field_e1::at::kJumpIn0.v);
    rv::As<records::PartyMember>(Sc())->state3 = 1;
}

// original 0x52FAF0 (PSX 0x801B45A8): jump-in 1 - 0x5362D0; when al, +3 = 2.
extern "C" void __cdecl Field_JumpIn1(void) {
    if (Fe2Al(field_e1::at::kJumpIn1.v)) rv::As<records::PartyMember>(Sc())->state3 = 2;
}

// original 0x52FB10 (PSX 0x801B45E4): jump-in 2 - 0x5363C0; when al: with +5 0 and neither
// script flag's bit 3, Field_JumpCamera; +3 = 3.
extern "C" void __cdecl Field_JumpIn2(void) {
    if (!Fe2Al(field_e1::at::kJumpIn2.v)) return;
    if (rv::As<records::PartyMember>(Sc())->actor == 0 && !ScriptFlag8()) SH_CALL(Field_JumpCamera)();
    rv::As<records::PartyMember>(Sc())->state3 = 3;
}

// original 0x52FB50 (PSX 0x801B4668): jump-in 3 - a tail jump to 0x536440.
extern "C" void __cdecl Field_JumpIn3(void) { Fe2(field_e1::at::kJumpIn3.v); }

// original 0x52FB60 (PSX 0x801B4688): state 10 (Field_LeaderCheckTest and
// Field_LeaderEffectTest set it) - the handler at 0x6609D0 for the loaded
// party set (0x90412C & 0x7F); then, when it left Field_State +0x137 at 0,
// the pose, member 0 cleared and state 1.
extern "C" void __cdecl Field_ActionState(void) {
    TableEventLeader(::bof3::Img(Field_ActionBySet_addr))[AreaW0b_PartySet & 0x7F]();
    if (Field_State[0x137] != 0) return;
    event_leader::g.ensure_animation(Sprite_Current[8]);
    event_leader::g.clear_state(0);
    Sprite_Current[1] = 1;
}

// original 0x52FBB0 (PSX 0x801B4714): leader state 11 - the step of Field_ContentSteps[+2],
// then Sprite_ScriptTick (a tail jump).
extern "C" void __cdecl Field_ContentState(void) {
    CodeAt(::bof3::Img(Field_ContentSteps_addr), rv::As<records::PartyMember>(Sc())->step, "Field_ContentState")();
    SH_CALL(Sprite_ScriptTick)();
}

// original 0x52FBD0: content step 0 - the object's flag (+5, in the bank
// 0x9040CC) set: system message 1. Else by +0x18: 0xFF the zenny +0x19 * 40
// (Field_GiveZenny), the flag (unless 0xFF) and the count 0x904140; an item
// (category +0x18, id +0x19): its 16-byte name to Text_Records and
// Inventory_Add(.., 1): taken - the flag (unless 0xFF), sound 0x106, message
// 2, the count; no room - message 3. Then, except after message 1, unless the
// object's +0xB bit 0 its animation 1 (Sprite_Current lent to it). Field_Request
// 2, +2 one on.
// As the original has it: the category and id reach Item_NamePtr and
// Inventory_Add as dwords whose upper bytes are the frame's (both read the low
// byte only).
extern "C" void __cdecl Field_ContentTake(void) {
    if (SH_CALL(Flags_Test)(move_script::At(::bof3::Img(field_e1::at::kObjectFlags)), ContentObject()[5])) {
        SH_CALL(Msg_OpenSystem)(1);
    } else {
        const unsigned char* const o = ContentObject();
        const unsigned char item = o[0x19];
        const unsigned char category = o[0x18];
        if (category == 0xFF) {
            SH_CALL(Field_GiveZenny)(item * 40u);
            const unsigned char flag = ContentObject()[5];
            if (flag != 0xFF) SH_CALL(Flags_Set)(move_script::At(::bof3::Img(field_e1::at::kObjectFlags)), flag);
            FieldE1_ContentCount = static_cast<std::int32_t>(FieldE1_ContentCount) + 1;
        } else {
            const unsigned char* const name = SH_CALL(Item_NamePtr)(category, item);
            std::memcpy(move_script::At(::bof3::Img(bof3::addr::Text_Records)), name, 16);
            if (SH_CALL(Inventory_Add)(category, item, 1)) {
                const unsigned char flag = ContentObject()[5];
                if (flag != 0xFF) SH_CALL(Flags_Set)(move_script::At(::bof3::Img(field_e1::at::kObjectFlags)), flag);
                SH_CALL(Sound_PlayEffect)(0x106);
                SH_CALL(Msg_OpenSystem)(2);
                FieldE1_ContentCount = static_cast<std::int32_t>(FieldE1_ContentCount) + 1;
            } else {
                SH_CALL(Msg_OpenSystem)(3);
            }
        }
        unsigned char* const object = ContentObject();
        if (!(object[0xB] & 1)) {
            unsigned char* const kept = Sprite_Current;
            Sprite_Current = object;
            SH_CALL(Sprite_SetAnimation)(1);
            Sprite_Current = kept;
        }
    }
    records::PartyMember* const sc = rv::As<records::PartyMember>(Sc());
    Field_Request = 2;
    sc->step = static_cast<unsigned char>(sc->step + 1);
}

// original 0x52FD90 (PSX 0x801B4A34): content step 1 - once Field_Request is 0: after system
// message 3 (the message word 0x7DEE48), the object back to animation 0
// unless its +0xB bit 0; else bit 0 removes the object (+0 = 0), or with its
// +0x34 and +0x38 fractions 0 its cell (+0x36, +0x3A) becomes 0x10
// (AreaMap_SetByte). Then with Field_InputFlags bit 5 state 0xC, else the pose
// +8 and state 1; step 0.
extern "C" void __cdecl Field_ContentEnd(void) {
    if (Field_Request != 0) return;
    if (W(::bof3::Img(field_e1::at::kMessageWord)) == 3) {
        unsigned char* const object = ContentObject();
        if (!(object[0xB] & 1)) {
            unsigned char* const kept = Sprite_Current;
            Sprite_Current = object;
            SH_CALL(Sprite_SetAnimation)(0);
            Sprite_Current = kept;
        }
    } else {
        records::SpriteObject* const object = rv::As<records::SpriteObject>(ContentObject());
        if (rv::Bytes(object)[0xB] & 1) {
            object->flags = 0;
        } else if (object->x_frac == 0 && object->z_frac == 0) {
            SH_CALL(AreaMap_SetByte)(object->x_whole, object->z_whole, 0x10);
        }
    }
    if (Field_InputFlags & 0x20) {
        rv::As<records::PartyMember>(Sc())->state = 0xC;
        rv::As<records::PartyMember>(Sc())->step = 0;
        return;
    }
    SH_CALL(Sprite_EnsureAnimation)(rv::As<records::PartyMember>(Sc())->direction);
    rv::As<records::PartyMember>(Sc())->state = 1;
    rv::As<records::PartyMember>(Sc())->step = 0;
}

// original 0x52FE90 (PSX 0x801B4BF4): leader state 12 - in area 0x68 Area104_LeaderRun, else
// Area121_LeaderRun (tail jumps).
extern "C" void __cdecl Field_AreaRunState(void) {
    if (Game_AreaNumber == 0x68) SH_CALL(Area104_LeaderRun)();
    else SH_CALL(Area121_LeaderRun)();
}

// DIVERGENCE DIV-0153: 1 through PatchBytes("MantleCap") in
// FieldEvent_Inject, after the self-test. Accessory 0x15 (the Holy Mantle)
// doubles the threshold in a byte; zone 1's 120..151 wraps to 0..46. With
// the flag the doubling stops at 0xFF.
namespace field_event { std::uint8_t g_mantle_cap = 0; }

// original 0x52FEB0 (PSX 0x801B4CC4): the word at 0x802E74 set from the zone
// the leader stands in - see docs/field-event.md section 1 for what it may be.
//
// As the original has it: the zone is found from the low bytes of the words
// at 0x802D76 / 0x802D7A (the leader's x and z, integer part). The sum, the
// doubling and the halving are 8-bit and wrap. Rand is called only on the
// roll path; the zone record's byte +4 is read again after it.
extern "C" void __cdecl Field_EncounterThresholdRoll(unsigned keep) {
    unsigned char* const counter = move_script::At(::bof3::Img(ObjTrio_addr) + 0x134);
    if (static_cast<unsigned char>(Field_ScriptFlags) & 0x20) {
        move_script::SetWord(counter, 0);
        return;
    }
    const unsigned char* const zone = field_event::g.zone_at(rv::Bytes(ObjTrio)[0x36], rv::Bytes(ObjTrio)[0x3a]);
    const unsigned char* const bases = move_script::At(::bof3::Img(field_event::at::kZoneBases));
    unsigned char value = bases[zone[4] * 2u];
    if (value == 0) {
        move_script::SetWord(counter, 0);
        return;
    }
    if ((keep & 0xFF) != 0) {
        const std::uint16_t now = move_script::Word(counter);
        if (now != 0) {
            if (now < value) return;
            move_script::SetWord(counter, value);
            return;
        }
    }
    unsigned char roll = static_cast<unsigned char>(field_event::g.rand() & 0x1F);
    const unsigned char most = bases[zone[4] * 2u + 1];
    if (roll > most) roll = most;
    value = static_cast<unsigned char>(value + roll);
    const unsigned members = static_cast<unsigned char>(field_event::g.party_count(0));
    unsigned i = 0;
    for (; i < members; ++i)
        if (field_event::g.has_item(ObjTrio[i].char_index, 3, 0x15)) break;
    if (i < members) value = (field_event::g_mantle_cap && value >= 0x80) ? 0xFF : static_cast<unsigned char>(value << 1);
    for (i = 0; i < members; ++i)
        if (field_event::g.has_item(ObjTrio[i].char_index, 3, 0x14)) break;
    if (i < members) value = static_cast<unsigned char>(value >> 1);
    move_script::SetWord(counter, value);
}

// original 0x52FFD0 (PSX 0x801B4C34): the first of the area's 8-byte records
// whose box [r0, r2] x [r1, r3] holds (x, z).
//
// As the original has it: there is no end test - a list with no record that
// matches loops for ever. The comparisons are 16-bit signed on zero-extended
// bytes, the same as unsigned.
extern "C" const unsigned char* __cdecl Area_ZoneAt(unsigned x_arg, unsigned z_arg) {
    const unsigned x = x_arg & 0xFF, z = z_arg & 0xFF;
    const unsigned char* record = move_script::At(static_cast<std::uint32_t>(move_script::Long(move_script::At(::bof3::Img(Area_StateRecords_addr)) + Game_AreaNumber * 4u)));   // a 32-bit word
    for (;; record += 8) {
        if (x < record[0] || z < record[1] || x > record[2] || z > record[3]) continue;
        return record;
    }
}

// original 0x530030: an encounter due? (PSX FUN_801B4EA8). Bit 5 of
// Field_ScriptFlags clears the zone counter ObjTrio +0x134 first; the zone
// the leader stands in (Area_ZoneAt) must have +4, the counter must be set
// and at most Field_StepCount (7/10 of it while walking, pace below 4), ObjTrio
// +0x138 bit 0 clear, the word 0x929EE0 at most 0xF0 unless bit 0 of
// Field_InputFlags, and no member with bit 6. Then on foot (bit 0): the cell
// +9 steps on is 0; else 0x591F30(4, 0) - which moves Sprite_Current, put
// back - and 0x535C50 (group V2's) for every member at its 0x7E06E0 position.
// As the original has it: the member count is read again after each call,
// and the answer is the last call's al.
extern "C" unsigned char __cdecl Field_EncounterDue(void) {
    if (Byte(::bof3::Img(kFlagsLo)) & 0x20) event_ops::SetWord(event_ops::At(::bof3::Img(ObjTrio_addr) + 0x134), 0);
    const unsigned char* const zone = event_ops::g.zone_at(ObjTrio[0].x_whole, ObjTrio[0].z_whole);
    if (zone[4] == 0) {
        event_ops::SetWord(event_ops::At(::bof3::Img(ObjTrio_addr) + 0x134), 0);
        return 0;
    }
    if (event_ops::Word(event_ops::At(::bof3::Img(ObjTrio_addr) + 0x134)) == 0) return 0;
    std::uint32_t edge = Field_StepCount;
    if (rv::Bytes(ObjTrio)[0x128] < 4) edge = edge * 7 / 10;
    if (edge < event_ops::Word(event_ops::At(::bof3::Img(ObjTrio_addr) + 0x134))) return 0;
    if (rv::Bytes(ObjTrio)[0x138] & 1) return 0;
    const bool on_foot = (Field_InputFlags & 1) != 0;
    if (!on_foot && static_cast<std::int32_t>(EventOps_ScreenCounter) > 0xF0) return 0;
    const int members = Field_MemberCount;
    for (int i = 0; i < members; ++i)
        if (ObjTrio[i].flags & 0x40) return 0;
    if (on_foot) {
        unsigned char* const sc = Sprite_Current;
        const std::uint32_t steps = sc[9];
        const std::uint32_t x = static_cast<std::uint32_t>(rv::As<records::SpriteObject>(sc)->step_x) * steps + static_cast<std::uint32_t>(rv::As<records::SpriteObject>(sc)->x);
        const std::uint32_t z = static_cast<std::uint32_t>(rv::As<records::SpriteObject>(sc)->step_z) * steps + static_cast<std::uint32_t>(rv::As<records::SpriteObject>(sc)->z);
        return event_ops::g.byte_at(Whole(x), Whole(z)) == 0;
    }
    unsigned char* const saved = Sprite_Current;
    unsigned char r = event_ops::g.party_visible(4, 0);
    Sprite_Current = saved;
    if (r == 0) return r;
    for (unsigned i = 0; i < Field_MemberCount; ++i) {
        const unsigned char* const at = event_ops::At(::bof3::Img(event_ops::at::kMemberPositions)) + i * 8;
        const unsigned slot = Character_IndexById[0xc + Cond_Flags[0xd5 + i]];
        r = event_ops::g.member_fits(event_ops::Long(at), event_ops::Long(at + 4), slot, 0x10, 1);
        if (r == 0) return r;
    }
    return r;
}

// original 0x5301F0: the idle fidget (PSX FUN_801B6444) - not for a record
// with bit 5 of +0x10 or on foot; Field_State +0x136 counts frames past 0xF0,
// then back to 0, and unless a member other than the leader is in state 9,
// one Rand in two: +0x137 = 9, the pose, +9 = 2, state 6, sub-state 0.
extern "C" unsigned char __cdecl Field_LeaderIdleTest(void) {
    if (event_ops::At(::bof3::Img(CharacterRecords_addr) + 0x10)[Field_State[0x148] * event_ops::at::kActorStride] & 0x20) return 0;
    if (Field_InputFlags & 1) return 0;
    Field_State[0x136] = static_cast<unsigned char>(Field_State[0x136] + 1);
    if (Field_State[0x136] <= 0xF0) return 0;
    Field_State[0x136] = 0;
    const int members = Field_MemberCount;
    if (members > 1) {
        bool busy = false;
        for (int i = 1; i < members; ++i)
            if (rv::Bytes(&ObjTrio[i])[0x137] == 9) busy = true;
        if (busy) return 0;
    }
    if (!(event_ops::g.rand() & 1)) return 0;
    Field_State[0x137] = 9;
    event_ops::g.ensure_animation(Sprite_Current[8]);
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
    if ((Byte(::bof3::Img(kFlags2Lo)) | Byte(::bof3::Img(kFlagsLo))) & 0x10) return 0;
    if (!(Input_Pressed & event_ops::Word(event_ops::At(::bof3::Img(Gfx_UnpackScratch_addr) + 0x1f008)))) return 0;
    if (!event_ops::g.can_swap()) return 0;
    Sprite_Current[1] = 3;
    Sprite_Current[2] = 0;
    Sprite_Current[3] = 0;
    Byte(::bof3::Img(kFlags2Hi)) |= 4;
    return 1;
}

// original 0x530320 (PSX 0x801B65B8): whether the party may swap (called only by
// Field_LeaderSwapTest; pointer-less, found by that call) - not under bits
// 0..2 of either flags byte, and no member after the leader in state 2 whose
// +2 is none of the three bytes at 0x660A34.
extern "C" unsigned char __cdecl Party_CanSwap(void) {
    if ((Byte(::bof3::Img(kFlags2Lo)) | Byte(::bof3::Img(kFlagsLo))) & 7) return 0;
    const int members = Field_MemberCount;
    for (int i = 1; i < members; ++i) {
        const unsigned char* const m = event_ops::At(::bof3::Img(ObjTrio_addr)) + i * event_ops::at::kMemberStride;
        if (m[1] != 2) continue;
        int k = 0;
        while (k < 3 && m[2] != event_ops::At(::bof3::Img(event_ops::at::kSwapKinds))[k]) ++k;
        if (k == 3) return 0;
    }
    return 1;
}

// original 0x530380: the menu button (PSX FUN_801B6750) - Field_MenuButton
// pressed and bit 6 of Field_ScriptFlags clear: sound 0x105,
// Field_ScriptFlags2 bit 11, the pose, state 4, sub-state 0.
extern "C" unsigned char __cdecl Field_LeaderMenuTest(void) {
    if (!(Input_Pressed & Field_MenuButton)) return 0;
    if (Byte(::bof3::Img(kFlagsLo)) & 0x40) return 0;
    event_ops::g.play_effect(0x105);
    Byte(::bof3::Img(kFlags2Hi)) |= 8;
    event_ops::g.leader_animation(Sprite_Current[8]);
    Sprite_Current[1] = 4;
    Sprite_Current[2] = 0;
    return 1;
}

// original 0x5303E0 (PSX FUN_801B687C): the button 0x903586 held and bit 13
// of Field_ScriptFlags clear - Field_Request 4, the pose, state 1 sub-state 0.
extern "C" unsigned char __cdecl Field_LeaderRequest4Test(void) {
    if (Byte(::bof3::Img(kFlagsHi)) & 0x20) return 0;
    if (!(Field_InputHeld & event_ops::Word(event_ops::At(::bof3::Img(Gfx_UnpackScratch_addr) + 0x1f006)))) return 0;
    Field_Request = 4;
    event_ops::g.leader_animation(Sprite_Current[8]);
    Sprite_Current[1] = 1;
    Sprite_Current[2] = 0;
    return 1;
}

// original 0x530430 (PSX FUN_801B6908): the pad's 0x800 pressed, bit 13 of
// Field_ScriptFlags and bits 0x69 of Field_InputFlags clear - the pose,
// Field_Request 9, state 0xE sub-state 0.
extern "C" unsigned char __cdecl Field_LeaderRequest9Test(void) {
    if (Byte(::bof3::Img(kFlagsHi)) & 0x20) return 0;
    if (Field_InputFlags & 0x69) return 0;
    if (!(Input_Pressed & 0x800)) return 0;
    event_ops::g.leader_animation(Sprite_Current[8]);
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

// original 0x530530 (PSX 0x801B6ADC): the first Effect_Objects record in use, of kind 0x17,
// that the point one Field_DirectionSteps ahead is in reach of - its index, or
// 0xFF. Pointer-less and in no list; called by Field_LeaderEffectTest and ten
// other sites (0x51D32E..0x524A9E, E8 scan).
// As the original has it: the direction indexes Field_DirectionSteps whole.
extern "C" unsigned char __cdecl Field_EffectAhead(void) {
    const unsigned char* const sc = Sprite_Current;
    const unsigned d = sc[8];
    const auto x = static_cast<int>(StepX(d) + static_cast<std::uint32_t>(rv::As<records::SpriteObject>(sc)->x));
    const auto z = static_cast<int>(StepZ(d) + static_cast<std::uint32_t>(rv::As<records::SpriteObject>(sc)->z));
    for (unsigned i = 0; i < 20; ++i) {
        const unsigned char* const e = rv::Bytes(&Effect_Objects[i]);
        if (e[0] == 0 || e[5] != 0x17) continue;
        if (event_ops::g.point_in_reach(x, z, static_cast<short>(rv::Current<records::SpriteHead>()->height_whole), 0, e)) return static_cast<unsigned char>(i);
    }
    return 0xFF;
}

// original 0x5305B0 (PSX 0x801B6BD4): the leader's animation through 0x589330
// (which sets it unless it is already +0x4B); with Field_InputFlags bit 0, by
// way of the pair at 0x6608D8 + animation * 2, whose second byte goes to
// Sprite_Current +0x2A. Returns 0x589330's al on the first path; on the
// second the original's eax is Sprite_Current, loaded after the call for the
// store - so al is that pointer's low byte (kept here; the fuzz compares al).
// No caller checked reads it.
//
// As the original has it: the pair's second byte is read after the call. The
// original pushes a dword whose upper bytes are the caller's (first path) or
// whatever ecx held (second); 0x589330 compares al and hands eax to
// Sprite_SetAnimation, typed as a byte - unchecked past that.
extern "C" unsigned char __cdecl Field_LeaderAnimation(unsigned animation) {
    if (!(Field_InputFlags & 1)) return field_event::g.set_animation(animation);
    const unsigned char* const pair = move_script::At(::bof3::Img(field_event::at::kAnimationRemap)) + (animation & 0xFF) * 2u;
    field_event::g.set_animation(pair[0]);
    unsigned char* const c = Sprite_Current;
    c[0x2A] = pair[1];
    return static_cast<unsigned char>(Address(c));
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
// DIVERGENCE DIV-0197 (D399, issue #177): an area with no list (+0x2C null -
// 105 of the 200 descriptors, recipes/exe-layout.tsv) answers "no passage".
// The original walks the null list - entry 0's count at address 3 - which the
// console's low RAM tolerates and the PC's protected page 0 does not: the
// faerie village's area 185 crashed on the talk button at the board.
extern "C" const unsigned char* __cdecl Area_PassageAhead(void) {
    const unsigned char* const descriptor = Area_Descriptors[Game_AreaNumber];
    int n = descriptor[0x31];
    const unsigned char* const list = event_ops::At(static_cast<std::uint32_t>(event_ops::Long(descriptor + 0x2C)));   // a 32-bit word
    if (list == nullptr) {
        static unsigned short logged = 0xFFFF;
        if (logged != Game_AreaNumber) {
            logged = Game_AreaNumber;
            bof3::Log("DIV-0197    Area_PassageAhead: area 0x%02X has no passage list (+0x2C null, +0x31 %d), none ahead",
                      static_cast<unsigned>(Game_AreaNumber), n);
        }
        return nullptr;
    }
    const unsigned char* const sc = Sprite_Current;
    for (const unsigned char* e = list + n * 8; n >= 0; --n, e -= 8) {
        for (int k = e[3] - 1; k >= 0; --k) {
            const unsigned large = static_cast<unsigned>(event_ops::Long(sc + 0x70)) & 0xFF;
            const unsigned char d = sc[8];
            unsigned x, z;
            int span;
            if (large == 0) {
                x = rv::As<records::SpriteHead>(sc)->x_whole + static_cast<unsigned>(CellDX(d));
                z = rv::As<records::SpriteHead>(sc)->z_whole + static_cast<unsigned>(CellDZ(d));
                span = (rv::As<records::SpriteHead>(sc)->x_frac == 0 && rv::As<records::SpriteHead>(sc)->z_frac == 0) ? 0 : 1;
            } else {
                const signed char cdx = CellDX(d), cdz = CellDZ(d);
                x = rv::As<records::SpriteHead>(sc)->x_whole + static_cast<unsigned>(cdx == 1 ? 2 : cdx);
                z = rv::As<records::SpriteHead>(sc)->z_whole + static_cast<unsigned>(cdz == 1 ? 2 : cdz);
                span = (d == 1 || d == 5) ? rv::As<records::SpriteHead>(sc)->x_frac != 0 : rv::As<records::SpriteHead>(sc)->z_frac != 0;
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

// original 0x5307C0 (PSX 0x801B6E50, the sibling's Field_GiveZenny): sound
// 0x106; the amount printed into Text_Records by Area08_MessageFormat; system
// message 5; Field_Request 2; Zenny_Add(amount, 0).
extern "C" void __cdecl Field_GiveZenny(unsigned amount) {
    SH_CALL(Sound_PlayEffect)(0x106);
    SH_CALL(Crt_sprintf)(reinterpret_cast<char*>(move_script::At(::bof3::Img(bof3::addr::Text_Records))), reinterpret_cast<const char*>(move_script::At(::bof3::Img(field_e1::at::kZennyFormat))),
                         amount);
    SH_CALL(Msg_OpenSystem)(5);
    Field_Request = 2;
    SH_CALL(Zenny_Add)(amount, 0);
}

// original 0x530800 (PSX FUN_801B6EB4): the button 0x90358C pressed, not on
// foot, and Field_State +0x89 none of 2, 4, 9, 0xA - state 0xA sub-state 0,
// +0x137 = 8.
extern "C" unsigned char __cdecl Field_LeaderCheckTest(void) {
    const unsigned char kind = Field_State[0x89];
    if (kind == 2 || kind == 4 || kind == 0xA || kind == 9) return 0;
    if (!(Input_Pressed & event_ops::Word(event_ops::At(::bof3::Img(Gfx_UnpackScratch_addr) + 0x1f00c)))) return 0;
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
    const unsigned char effect = event_ops::g.effect_ahead();
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
    if (EventOps_BlockByte != 0) return 0;
    if (!(Input_Pressed & event_ops::Word(event_ops::At(::bof3::Img(Gfx_UnpackScratch_addr) + 0x1f000)))) return 0;
    const unsigned char facing = Sprite_Current[8];
    event_ops::g.cell_around(0x30);
    unsigned char object = event_ops::g.facing_object();
    if (object == 0xFF) {
        Sprite_Current[8] = facing;
        object = event_ops::g.facing_object();
    }
    if (object != 0xFF) {
        event_ops::g.talk_to(object);
        return 1;
    }
    if (event_ops::g.cell_around(0x51) && event_ops::g.cell_hook(event_ops::at::kScratch.w_00, event_ops::at::kScratch2)) {
        event_ops::g.leader_animation(Sprite_Current[8]);
        Sprite_Current[2] = 0;
        return 1;
    }
    Sprite_Current[8] = facing;
    if (!event_ops::g.cell_around(0x50) && !event_ops::g.cell_around(0x53) && !event_ops::g.cell_around(0x55) && !event_ops::g.cell_around(0x52)) return 0;
    const unsigned char* const passage = event_ops::g.passage_ahead();
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

void Field4_Inject() {
    BOF3_INJECT(Field_LeaderFrame);
    BOF3_INJECT(Field_LeaderStart);
    BOF3_INJECT(Field_LeaderControl);
    BOF3_INJECT(Field_LeaderStand);
    BOF3_INJECT(Field_LeaderWalk);
    BOF3_INJECT(Field_LeaderSetPace);
    BOF3_INJECT(Field_LeaderStepping);
    BOF3_INJECT(Field_LeaderStepTick);
    BOF3_INJECT(Field_LeaderStepTarget);
    BOF3_INJECT(Field_LeaderStepCell);
    BOF3_INJECT(Field_LeaderStepLands);
    BOF3_INJECT(Field_SwapState);
    BOF3_INJECT(Field_SwapGather);
    BOF3_INJECT(Field_SpotFree);
    BOF3_INJECT(Field_PathClear);
    BOF3_INJECT(Field_SwapSpinOut);
    BOF3_INJECT(Field_SwapExchange);
    BOF3_INJECT(Field_SwapSpinIn);
    BOF3_INJECT(Field_SwapEnd);
    BOF3_INJECT(Field_MenuState);
    BOF3_INJECT(Field_MenuOpen);
    BOF3_INJECT(Field_MenuWait);
    BOF3_INJECT(Field_EncounterState);
    BOF3_INJECT(Field_EncounterStart);
    BOF3_INJECT(Field_EncounterWait);
    BOF3_INJECT(Field_FormActionState);
    BOF3_INJECT(Sprite_TurnSense);
    BOF3_INJECT(PartyAction_ScriptEnd);
    BOF3_INJECT(Field_PassageState);
    BOF3_INJECT(Field_PassageOpen);
    BOF3_INJECT(Field_PassageTake);
    BOF3_INJECT(Field_PassageEnd);
    BOF3_INJECT(Field_PassageTrigger);
    BOF3_INJECT(Field_JumpState);
    BOF3_INJECT(Field_JumpBegin);
    BOF3_INJECT(Field_JumpOut);
    BOF3_INJECT(Field_JumpOut0);
    BOF3_INJECT(Field_JumpOut1);
    BOF3_INJECT(Field_JumpOut2);
    BOF3_INJECT(Field_JumpOut3);
    BOF3_INJECT(Field_JumpOut4);
    BOF3_INJECT(Field_JumpAir);
    BOF3_INJECT(Field_JumpIn);
    BOF3_INJECT(Field_JumpIn0);
    BOF3_INJECT(Field_JumpIn1);
    BOF3_INJECT(Field_JumpIn2);
    BOF3_INJECT(Field_JumpIn3);
    BOF3_INJECT(Field_ActionState);
    BOF3_INJECT(Field_ContentState);
    BOF3_INJECT(Field_ContentTake);
    BOF3_INJECT(Field_ContentEnd);
    BOF3_INJECT(Field_AreaRunState);
    BOF3_INJECT(Field_EncounterThresholdRoll);
    BOF3_INJECT(Area_ZoneAt);
    BOF3_INJECT(Field_EncounterDue);
    BOF3_INJECT(Field_LeaderIdleTest);
    BOF3_INJECT(Field_LeaderSwapTest);
    BOF3_INJECT(Party_CanSwap);
    BOF3_INJECT(Field_LeaderMenuTest);
    BOF3_INJECT(Field_LeaderRequest4Test);
    BOF3_INJECT(Field_LeaderRequest9Test);
    BOF3_INJECT(Field_LeaderDirection);
    BOF3_INJECT(Field_EffectAhead);
    BOF3_INJECT(Field_LeaderAnimation);
    BOF3_INJECT(Area_PassageAhead);
    BOF3_INJECT(Field_GiveZenny);
    BOF3_INJECT(Field_LeaderCheckTest);
    BOF3_INJECT(Field_LeaderEffectTest);
    BOF3_INJECT(Field_LeaderPushCount);
    BOF3_INJECT(Field_LeaderTalkTest);
}

void Field4FromEventLeader_Inject() {
    {
        // DIV-0121: armed after the fuzz above, which sees Capcom's rule.
        const std::uint8_t was[] = {0, 0, 0, 0}, is[] = {1, 0, 0, 0};
        bof3::PatchBytes("EdgeNoArea", Low32(&event_leader::g_edge_no_area),
                         was, is, 4);
        bof3::Log("DIV-0121    a map edge that leads to no area (a game loaded in an encounter field) holds (on unless the line above says OFF)");
    }
}

void Field4FromFieldEvent_Inject() {
    {
        // DIV-0153: after the self-test, which compared Capcom's roll.
        const std::uint8_t was = 0, is = 1;
        bof3::PatchBytes("MantleCap", Low32(&field_event::g_mantle_cap), &was, &is, 1);
        bof3::Log("DIV-0153    the Holy Mantle's doubled encounter threshold stops at 255 instead of wrapping (on unless the line above says OFF)");
    }
}

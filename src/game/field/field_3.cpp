// Moved here by tools/move_funcs.py from battle_misc.cpp, field_blocked.cpp, field_c3.cpp, field_frame.cpp, field_misc.cpp, field_objects.cpp, mode_states.cpp, move_cmds.cpp, object_kinds.cpp (docs/source-layout-round.md).
#include "game/field/field_3.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <utility>

#include "bof3/symbols.gen.h"
#include "common/asm_name.h"
#include "game/battle_misc.h"
#include "game/battle_misc_callees.h"
#include "game/battle_text.h"
#include "game/d3d_lines.h"
#include "game/field_blocked.h"
#include "game/field_c3_callees.h"
#include "game/field_frame.h"
#include "game/field_misc.h"
#include "game/field_misc_callees.h"
#include "game/field_objects.h"
#include "game/labels.h"
#include "game/map_rim.h"
#include "game/mode_states_callees.h"
#include "game/move_cmds_callees.h"
#include "game/move_script_bytes.h"
#include "game/object_kinds.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/code_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = field_c3::at;

}  // namespace

namespace {

using namespace battle_misc;

using namespace move_script;

using namespace field_misc;

}  // namespace

using namespace mode_states;

namespace {

using namespace move_cmds;

}  // namespace

namespace {

unsigned char* AtBattleMisc(battle_misc::U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }

void PutLong(unsigned char* p, battle_misc::U v) { std::memcpy(p, &v, sizeof v); }

void SetSpriteCurrent(battle_misc::U v) { PutLong(AtBattleMisc(battle_misc::at::Sprite_CurrentAt()), v); }

// The high nibbles of an AreaMap_Bytes cell that stop a step, a bit each. The
// original looks the nibble up in a byte table at 0x518660 (0 or 2: open,
// 1: blocked) and jumps through 0x518654: the switch it was compiled from,
// the function's logic, ours since AreaMap_CellBlocked is (the disc-only
// round's rule for a table the compiler put in the code, docs/operand-store.md;
// it was read from the image until then, docs/exe-table-audit.md). Hosted,
// FieldBlocked_Inject registers CellTableIsOurs, which reads both tables and
// compares, for Operands_Check.
bool NibbleBlocks(unsigned nibble) {
    switch (nibble) {
    case 0x1: case 0x2: case 0x3: case 0x4: case 0x5: case 0xA: case 0xB: case 0xF: return true;
    default: return false;
    }
}

std::uint16_t BlockingNibbles() {
    static const std::uint16_t mask = [] {
        std::uint16_t m = 0;
        for (unsigned nibble = 0; nibble < 16; ++nibble)
            if (NibbleBlocks(nibble)) m = static_cast<std::uint16_t>(m | 1u << nibble);
        return m;
    }();
    return mask;
}

// Only al of a byte result is meaningful: the originals (and the recorders
// standing in for them in the fuzz) leave the rest of eax undefined.
unsigned char Al(std::uint32_t eax) { return static_cast<unsigned char>(eax); }

// The point `n` steps ahead in direction d & 7: 32-bit wrapping, as the
// originals' add and imul.
std::uint32_t StepX(unsigned d, std::uint32_t n) { return static_cast<std::uint32_t>(Field_DirectionSteps[(d & 7) * 2]) * n; }

std::uint32_t StepY(unsigned d, std::uint32_t n) { return static_cast<std::uint32_t>(Field_DirectionSteps[(d & 7) * 2 + 1]) * n; }

long Ahead(const unsigned char* object, unsigned at, std::uint32_t step) {
    return static_cast<long>(static_cast<std::uint32_t>(move_script::Long(object + at)) + step);
}

using U = std::uint32_t;

// move_script::At is named in full here (step B0, analysis/tu_renames.tsv)
using move_script::Long;

using move_script::SetLong;

using move_script::Word;

unsigned char* S() { return Sprite_Current; }

U ULong(const unsigned char* p) { return static_cast<U>(move_script::Long(p)); }

void SetULong(unsigned char* p, U v) { move_script::SetLong(p, static_cast<std::int32_t>(v)); }

short SWord(const unsigned char* p) { return static_cast<short>(move_script::Word(p)); }

template <typename T> U AddressOf(T* p) { return Low32(p); }

// cdq / xor / sub: the absolute value in 32 bits (0x80000000 stays itself).
std::int32_t Abs32(U d) {
    const U sign = static_cast<U>(static_cast<std::int32_t>(d) >> 31);
    return static_cast<std::int32_t>((d ^ sign) - sign);
}

U DirStepX(unsigned d) { return ULong(move_script::At(AddressOf(Field_DirectionSteps) + d * 8)); }

U DirStepZ(unsigned d) { return ULong(move_script::At(AddressOf(Field_DirectionSteps) + 4 + d * 8)); }

// Field_ObjectFleeDirection / ChaseDirection: they differ only in the
// order of each subtraction (the absolute values agree) and in `farthest`.
unsigned char LeaderDirection(unsigned char* object, unsigned char farthest) {
    records::SpriteHead* const s = rv::Current<records::SpriteHead>();   // held for both tests and +8
    const std::int32_t dx = Abs32(static_cast<std::uint32_t>(ObjTrio[0].x) - static_cast<U>(s->x)) >> 16;
    const unsigned char direction = s->direction;
    if (static_cast<int>(SWord(object + 0x98)) > dx) {
        const std::int32_t dz = Abs32(static_cast<std::uint32_t>(ObjTrio[0].z) - static_cast<U>(s->z)) >> 16;
        if (static_cast<int>(SWord(object + 0x9A)) > dz) {
            const unsigned char best = SH_CALL(Field_ObjectBestDirection)(object, farthest);
            if (best == 0xFF) {
                rv::Current<records::SpriteHead>()->step_frames = 0;
                return 0xFF;
            }
            if ((direction & 7) != best) {
                rv::Current<records::SpriteHead>()->direction = best;
                object[0x80] = static_cast<unsigned char>(object[0x80] | 8);
            }
            return rv::Current<records::SpriteHead>()->direction;
        }
    }
    SH_CALL(Field_ObjectRandomTurn)(object);
    return SH_CALL(Field_ObjectOpenDirection)(object);
}

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
    records::SpriteHead* const s = rv::Current<records::SpriteHead>();
    s->state = rv::Bytes(s)[3];
    S()[4] = 0;
}

constexpr unsigned kObjectBytes = 0xA4, kObjects = 30, kExtra = 4, kMemberBytes = 0x14C;

using Handler = void (__cdecl*)(unsigned char*);

void CallHandler(unsigned char* object) {
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(Field_ObjectHandlers[object[1]]))(object);
}

std::uint32_t Address(const volatile void* p) { return Low32(p); }

// As MoveScript_GroupD's: Field_ActiveMember's party slot, as a byte.
unsigned char PartySlot() {
    const auto distance = static_cast<std::int32_t>(Low32(Field_ActiveMember.Native()) - Address(rv::Bytes(Sprite_ObjectsExtra)));
    return static_cast<unsigned char>(distance / 0xA4);
}

std::uint32_t AddressMoveCmds(const volatile void* p) { return Low32(p); }

std::int32_t Add(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b));
}

std::int32_t Sub(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) - static_cast<std::uint32_t>(b));
}

// The angle a party member turns to for a direction: Sprite_DirectionAngles,
// read at any index as the original reads it (the table has 8).
std::uint16_t DirectionAngle(unsigned char direction) { return move_script::Word(move_script::At(AddressMoveCmds(Sprite_DirectionAngles) + direction * 2u)); }

constexpr std::uint32_t kAsideRows = 0x65F624;      // 4 directions per leader direction

unsigned char Pace(unsigned char* object, unsigned speed, unsigned flags) {
    return object_kinds::gObjectKinds.pace[Sprite_Current[2]](object, speed, flags);
}

// The four wander kinds end alike: a direction `turn_to` (0xFF for none)
// becomes the target of a turn in pose 10 if a timed move began - returning
// while that pose holds - or the move is cancelled; then the settle and the
// motion. `direction` is Sprite_Current[8] as the handler found it.
void TurnOrCancel(unsigned char* object, unsigned char direction, bool turn, unsigned char turn_to) {
    auto* const obj = rv::As<records::SpriteObject>(object);
    if (turn) {
        Sprite_Current[3] = rv::Current<records::SpriteObject>()->state;
        rv::Current<records::SpriteObject>()->state = 0x0A;
        obj->saved_facing = turn_to;
        rv::Current<records::SpriteObject>()->direction = direction & 7;
        Sprite_Current[0xA] = 0;
        rv::Current<records::SpriteObject>()->step_frames = 0;
        object_kinds::gObjectKinds.turn(object);
        if (rv::Current<records::SpriteObject>()->state == 0x0A) return;   // no settle, no motion this frame
        rv::Current<records::SpriteObject>()->direction = direction;
    } else {
        rv::Current<records::SpriteObject>()->step_frames = 0;
        rv::Current<records::SpriteObject>()->direction = direction;
    }
    object_kinds::gObjectKinds.settle(object, direction);
    object_kinds::gObjectKinds.motion(object);
}

// Entries 0 and 3: a random walk, the second kept inside a home box.
void Wander(unsigned char* object, bool home) {
    auto* const obj = rv::As<records::SpriteObject>(object);
    const unsigned char direction = rv::Current<records::SpriteObject>()->direction;
    Pace(object, 0, 1);
    if (rv::Current<records::SpriteObject>()->step_frames != 0) {   // a timed move runs
        object_kinds::gObjectKinds.motion(object);
        return;
    }
    const unsigned char speed_index = obj->speed;
    const unsigned char speed = Field_MoveSpeeds[speed_index];
    if (obj->move_steps == 0) {
        if (speed_index != 0 && Pace(object, speed, 2)) {
            object_kinds::gObjectKinds.random_turn(object);
            if (rv::Current<records::SpriteObject>()->step_frames != 0) obj->move_steps = static_cast<unsigned char>((object_kinds::gObjectKinds.rand() & 3) + 3);
        }
    } else {
        // Not guarded against speed 0, as the original: Field_Pace0 then
        // divides by zero.
        Pace(object, speed, 2);
        --obj->move_steps;
    }
    const unsigned char turn_to = object_kinds::gObjectKinds.open_direction(object);
    const bool turn = turn_to != 0xFF && rv::Current<records::SpriteObject>()->step_frames != 0 && (!home || object_kinds::gObjectKinds.in_home(object));
    TurnOrCancel(object, direction, turn, turn_to);
}

// Entries 1 and 2: away from the leader or toward it. With no steps left and
// no speed they return before the settle and the motion, which the wander
// kinds do not (the PSX's twins alike).
void LeaderMove(unsigned char* object, unsigned char (__cdecl* pick)(unsigned char*), unsigned char farthest) {
    auto* const obj = rv::As<records::SpriteObject>(object);
    const unsigned char direction = rv::Current<records::SpriteObject>()->direction;
    Pace(object, 0, 1);
    if (rv::Current<records::SpriteObject>()->step_frames != 0) {
        object_kinds::gObjectKinds.motion(object);
        return;
    }
    const unsigned char speed_index = obj->speed;
    const unsigned char speed = Field_MoveSpeeds[speed_index];
    unsigned char turn_to = 0xFF;
    if (obj->move_steps == 0) {
        if (speed_index == 0) return;
        if (Pace(object, speed, 2)) {
            turn_to = pick(object);
            if (rv::Current<records::SpriteObject>()->step_frames != 0) obj->move_steps = static_cast<unsigned char>((object_kinds::gObjectKinds.rand() & 3) + 3);
        }
    } else {
        Pace(object, speed, 2);
        turn_to = object_kinds::gObjectKinds.best(object, farthest);
        --obj->move_steps;
    }
    const bool turn = turn_to != 0xFF && rv::Current<records::SpriteObject>()->step_frames != 0;
    TurnOrCancel(object, direction, turn, static_cast<unsigned char>(turn_to & 7));
}

// Field_ObjectFadeOut / FadeIn's cases. The tint record is the one
// Sprite_SetTint's result names, kept in Field_ActiveMember +0x9F.
unsigned char* TintRecordObjectKinds(unsigned char index) { return MoveScript_TintRecords + index * 12u; }

void FadeOutStart() {
    const unsigned char index = object_kinds::gObjectKinds.set_tint(Sprite_Current, 0x1F, 0x1F, 0x1F, 1);
    Field_ActiveMember[0x9F] = index;
    rv::Current<records::SpriteObject>()->flags &= 0xBF;
    Sprite_Current[4] = 1;
}

void FadeOutStep() {
    unsigned char* const member = Field_ActiveMember;
    for (const unsigned at : {2u, 3u, 4u}) {
        unsigned char* const r = TintRecordObjectKinds(member[0x9F]);   // read each time, as the original (nothing between changes it)
        if (r[at] != 0) --r[at];
    }
    const unsigned char index = member[0x9F];
    if (TintSum(TintRecordObjectKinds(index)) != 0) return;
    object_kinds::gObjectKinds.tint_release(index);
    Field_ActiveMember[0x80] &= 0xFD;   // context bit 1, which chose this pose
    rv::Current<records::SpriteObject>()->state = Sprite_Current[3];
    Sprite_Current[4] = 0;
}

void FadeInStart() {
    const unsigned char index = object_kinds::gObjectKinds.set_tint(Sprite_Current, 0, 0, 0, 1);
    Field_ActiveMember[0x9F] = index;
    Sprite_Current[4] = 1;
}

void FadeInStep() {
    unsigned char* const member = Field_ActiveMember;
    for (const unsigned at : {2u, 3u, 4u}) {
        unsigned char* const r = TintRecordObjectKinds(member[0x9F]);
        if (static_cast<signed char>(r[at]) < 0x1F) ++r[at];   // signed: 0x80..0xFF climb too
    }
    const unsigned char index = member[0x9F];
    if (TintSum(TintRecordObjectKinds(index)) != 0x5D) return;
    object_kinds::gObjectKinds.tint_release(index);
    Field_ActiveMember[0x80] &= 0xFB;   // context bit 2
    rv::Current<records::SpriteObject>()->state = Sprite_Current[3];
    Sprite_Current[4] = 0;
}

// Field_Pace4..6: on a pass where the chosen counter bit changed, count a
// step up (to 0xFA, by `add` - so Field_Pace5's 0xF9 reaches 0xFB) and
// return 1; else with flags bit 1 and steps left, a move's frames.
unsigned char PaceCounted(unsigned char* object, unsigned speed, unsigned flags, unsigned char bit, unsigned char add) {
    auto* const obj = rv::As<records::SpriteObject>(object);
    if ((flags & 1) && ((move_script::At(::bof3::Img(Field_StepCount_addr))[0] ^ move_script::At(::bof3::Img(Field_StepCountPrev_addr))[0]) & bit)) {
        if (obj->move_steps < 0xFA) obj->move_steps = static_cast<unsigned char>(obj->move_steps + add);
        return 1;
    }
    if ((flags & 2) && obj->move_steps != 0) {
        rv::Current<records::SpriteObject>()->step_frames = static_cast<unsigned char>(0x20 / static_cast<int>(speed & 0xFF));   // 0 faults, as the idiv
        Sprite_Current[0xA] = static_cast<unsigned char>(rv::Current<records::SpriteObject>()->step_frames >> 1);
    }
    return 0;
}

template <typename T, typename F> T As(F f) { return reinterpret_cast<T>(reinterpret_cast<void*>(f)); }

}  // namespace

// move_cmds's callee seam, moved here from move_cmds.cpp by tools/retire_shells.py
// (docs/source-layout-round.md 5): the group's lowest function lives in this file.
namespace move_cmds {

const Callees kOriginals = {
    AreaMap_Elevation, Math_Ratan2, ScriptContext_Reset, MapView_SetElevation, MoveCmd_AttachOffset,
    MoveScript_ObjectKind, MoveCmd_Detach, MoveCmd_AttachMove, MoveCmd_HandlePosition, PartySet_Load, Party_Join,
    AreaMap_Slope,
};
Callees g = kOriginals;

}  // namespace move_cmds

// field_misc's callee seam, moved here from field_misc.cpp by tools/retire_shells.py
// (docs/source-layout-round.md 5): the group's lowest function lives in this file.
namespace field_misc {

const Callees kOriginals = {
    AreaMap_CellBlocked,
    AreaMap_TooSteep,
    AreaMap_TooSteepAt,
    Gpu_SetPolyF4,
    Gfx_CommitPrim,
    Area_TestCondition,
    MapView_ItemAt,
    Prim_SetTexture,
    MapView_CheckHeightScale,
    AreaMap_Slope,
    D3d_PrimColor,
    reinterpret_cast<void (__cdecl*)(unsigned)>(static_cast<std::uintptr_t>(::bof3::Code(kRetOnly.v))),
    D3d_SetBlend,
    D3d_SetShadeMode,
};
Callees g = kOriginals;

}  // namespace field_misc

// original 0x517200: the field's frame (PSX FUN_8019A1B8, call for call).
extern "C" void __cdecl Field_Frame(void) {
    field_frame::g.kind2();
    field_frame::g.extra_frame();
    field_frame::g.members();
    field_frame::g.objects();
    field_frame::g.area_map();
    field_frame::g.extra_screens();
    field_frame::g.party_screens();
    field_frame::g.objects_screen();
    field_frame::g.effects();
    field_frame::g.slots();
    field_frame::g.tint();
    field_frame::g.draw();   // a tail jmp in the original
}

// original 0x517240 (PSX 0x8019A230): the same frame with the area's mode dispatch and the
// task records first (PSX FUN_8019A230, call for call).
extern "C" void __cdecl Field_FrameScripted(void) {
    field_frame::g.mode_dispatch();
    field_frame::g.task_records();
    field_frame::g.kind2();
    field_frame::g.extra_frame();
    field_frame::g.members();
    field_frame::g.objects();
    field_frame::g.area_map();
    field_frame::g.extra_screens();
    field_frame::g.party_screens();
    field_frame::g.objects_screen();
    field_frame::g.effects();
    field_frame::g.slots();
    field_frame::g.tint();
    field_frame::g.draw();   // a tail jmp in the original
}

// original 0x517290 (PSX 0x8019A2B8): a field frame without the event
// script and without the objects' logic, for the frames a load waits -
// AreaMap_Frame, Party_ExtraScreens, Party_UpdateScreens,
// Field_ObjectsScreen, Effect_RunObjects, MoveScript_TintFrame, then a tail
// jump to Field_DrawFrame. Sixteen callers (the mode handlers' waits,
// Scenario_Start, 0x544B1D, 0x551C60, 0x56D645).
extern "C" __attribute__((disable_tail_calls)) void __cdecl Field_LoadingFrame(void) {
    mode_states::g.areamap_frame();
    mode_states::g.extra_screens();
    mode_states::g.update_screens();
    mode_states::g.objects_screen();
    mode_states::g.run_effects();
    mode_states::g.tint_frame();
    mode_states::g.draw_frame();
}

// original 0x5172C0 (PSX twin 0x8019A308): mode 11's frame.
extern "C" void __cdecl Mode11_FieldFrame(void) {
    SH_CALL(Field_MembersFrame)();
    SH_CALL(AreaMap_Frame)();
    SH_AT(void (__cdecl*)(), field_c3::at::kEventObjectFrame.v)();
    SH_CALL(Party_ExtraScreens)();
    SH_CALL(Party_UpdateScreens)();
    SH_AT(void (__cdecl*)(), field_c3::at::kPartyScreens.v)();
    SH_CALL(Effect_RunObjects)();
    SH_CALL(MoveScript_TintFrame)();
    SH_CALL(Field_DrawFrame)();
}

// original 0x5172F0 (PSX 0x8019A368): the field menu's frame (mode 3's
// step 1, GameMode_Handlers' neighbour 0x656A74 entry 1) - Field_ModeDispatch,
// the menu's state dispatch 0x589970 (group DH's), then a tail jump to
// Field_RunTaskRecords.
extern "C" __attribute__((disable_tail_calls)) void __cdecl Menu_Frame(void) {
    mode_states::g.mode_dispatch();
    mode_states::g.menu_states();
    mode_states::g.task_records();
}

// original 0x517300 (no PSX twin paired; PSX 0x8019A398): the shop's step 1, once a frame -
// the field drawn without its objects' logic (AreaMap_Frame,
// Party_ExtraScreens, Party_UpdateScreens, Field_ObjectsScreen,
// Effect_RunObjects, Field_DrawFrame, Field_RunTaskRecords), then a tail
// jump into the shop overlay's state dispatch 0x57F500 (group DF's).
extern "C" __attribute__((disable_tail_calls)) void __cdecl Shop_Frame(void) {
    mode_states::g.areamap_frame();
    mode_states::g.extra_screens();
    mode_states::g.update_screens();
    mode_states::g.objects_screen();
    mode_states::g.run_effects();
    mode_states::g.draw_frame();
    mode_states::g.task_records();
    mode_states::g.shop_states();
}

// original 0x517330 (PSX 0x8019A3F0): mode 8's step 5 (0x656AB8[5]).
extern "C" void __cdecl Mode8_Step5(void) {
    SH_AT(void (__cdecl*)(), field_c3::at::kMenuDispatchA.v)();
    SH_CALL(Field_RunTaskRecords)();
}

// original 0x517340 (PSX 0x8019A418): mode 8's step 8 (0x656AB8[8]).
extern "C" void __cdecl Mode8_Step8(void) {
    SH_AT(void (__cdecl*)(), field_c3::at::kMenuDispatchB.v)();
    SH_CALL(Field_RunTaskRecords)();
}

// original 0x517350: the party members' frames (PSX FUN_8019A440). Bit n of
// the two flag words' low bytes holds member n still; the second member runs
// only with more than one, the third only with exactly three - four or more
// run two, as the original has it. Flags and count are re-read after every
// call, as the original's loads are.
extern "C" void __cdecl Field_MembersFrame(void) {
    field_frame::g.pending();
    const auto held = [] { return static_cast<unsigned char>(Field_ScriptFlags2 | Field_ScriptFlags); };
    if (!(held() & 1)) {
        Field_State = rv::Bytes(ObjTrio);
        Sprite_Current = rv::Bytes(ObjTrio);
        field_frame::g.leader();
    }
    if (Field_MemberCount <= 1) return;
    if (!(held() & 2)) {
        Field_State = rv::Bytes(&ObjTrio[1]);
        Sprite_Current = rv::Bytes(&ObjTrio[1]);
        field_frame::g.member();
    }
    if (Field_MemberCount != 3) return;
    if (!(held() & 4)) {
        Field_State = rv::Bytes(&ObjTrio[2]);
        Sprite_Current = rv::Bytes(&ObjTrio[2]);
        field_frame::g.member();   // a tail jmp in the original
    }
}

// original 0x5173E0: the 30 field objects' screen update (PSX FUN_801A151C).
//
// As the original has it: a type-9 object keeps whatever slot byte +0x29 it
// had (and is skipped entirely under Draw_OtSlot 4); the type-0x0A test reads
// Sprite_Current, which is the object here since nothing runs in between.
extern "C" void __cdecl Field_ObjectsScreen(void) {
    unsigned char* object = rv::Bytes(Sprite_Objects);
    for (unsigned n = kObjects; n != 0; --n, object += kObjectBytes) {
        Sprite_Current = object;
        if (!(object[0] & 1)) continue;
        if (object[6] == 9) {
            if (Draw_OtSlot == 4) continue;
        } else {
            object[0x29] = (object[0x24] & 0x10) ? 4 : Draw_OtSlot;
        }
        if (rv::Current<records::SpriteObject>()->type == 0x0A) field_frame::g.screen_a();
        else field_frame::g.update_screen();
    }
}

// original 0x517440 (PSX 0x801A1624, by its callers): each of the 30
// Sprite_Objects in turn becomes Sprite_Current; one of type (byte +6, read
// after that store) 7 or 8 and up, present (byte 0 bit 0), gets its screen
// update - Sprite_UpdateScreenA for type 0xA, none for 9,
// Sprite_UpdateScreenSlot for the rest. The type is read once per object.
// Called from the battle's frame (0x42E3D1), which calls on at once.
extern "C" void __cdecl Sprite_UpdateObjectScreens(void) {
    for (battle_misc::U i = 0; i < 30; ++i) {
        const battle_misc::U object = battle_misc::at::Sprite_ObjectsAt() + i * 0xA4;
        SetSpriteCurrent(object);
        const unsigned char type = AtBattleMisc(object + 6)[0];
        if (type < 8 && type != 7) continue;
        if (!(AtBattleMisc(object)[0] & 1)) continue;
        if (type == 0xA) battle_misc::g.update_screen_a();
        else if (type != 9) battle_misc::g.update_screen_slot();
    }
}

// original 0x517490: the 30 field objects' frame (PSX FUN_801A16E0). The
// object's script context is +0x80; +9 counts a timed move's frames.
//
// As the original has it: Field_ObjectLinked ends an object's turn with
// nothing after it; everything after a callee reads Sprite_Current and the
// context byte afresh, since the callee may move or change them; the tick
// count is max(+0x84 - 2, 0) + 1, +0x84 read once before the ticks; after the
// turn target, the face-ObjTrio branch stores (ObjTrio[8] ^ 4) & 7 - the
// original also compares the sprite's direction with ObjTrio's there and
// branches on (differ ^ 4), which is never 0: a dead test with no effect,
// not reproduced.
extern "C" void __cdecl Field_ObjectsFrame(void) {
    if (Field_ScriptFlags2 & 0x400) return;
    unsigned char* object = rv::Bytes(Sprite_Objects);
    for (unsigned n = kObjects; n != 0; --n, object += kObjectBytes) {
        Sprite_Current = object;
        Field_ActiveMember = object;
        if (!(object[0] & 1)) continue;
        Scratch()[3] = 0;
        unsigned char* const context = object + 0x80;
        const unsigned char timed = object[9];
        const unsigned char flags = context[0];
        if (timed == 0 && (flags & 0x20)) {
            if (flags & 0x10) {
                const unsigned char turned = field_frame::g.turn_target(context);
                records::SpriteObject* const sprite = rv::Current<records::SpriteObject>();
                if (turned != 0 && !(sprite->flags2 & 8)) {
                    object[0x85] = static_cast<unsigned char>(sprite->direction & 7);
                    if (sprite->flags2 & 2) {
                        field_frame::g.face(static_cast<unsigned char>(sprite->direction & 7));
                    } else {   // turn to face ObjTrio's first member
                        sprite->direction = static_cast<unsigned char>((ObjTrio[0].direction ^ 4) & 7);
                        field_frame::g.face(rv::Current<records::SpriteObject>()->direction);
                    }
                    if (rv::Current<records::SpriteObject>()->state == 0x0A) object[0x85] |= 1;
                }
            } else {
                field_frame::g.idle(object);
            }
        } else {
            if (context[0] & 1) {
                field_frame::g.linked();
                continue;
            }
            if (move_script::Word(object + 0x9C) > 0x1E && timed == 0) field_frame::g.idle_long(object);
            else CallHandler(object);
        }
        records::SpriteObject* const sprite = rv::Current<records::SpriteObject>();
        if (sprite->type == 0x0A) continue;
        const unsigned char after = context[0];
        if (after & 0x40) {
            if (sprite->flags2 & 0x20) continue;
            if (sprite->flags & 0x10) field_frame::g.tick();
            else field_frame::g.tick_once();
        } else if (after & 8) {
            field_frame::g.face(sprite->direction);
            context[0] &= 0xF7;
        } else if (!(sprite->flags2 & 0x20)) {
            const int extra = static_cast<int>(object[0x84]) - 2;
            const unsigned ticks = (extra < 0 ? 0u : static_cast<unsigned>(extra) & 0xFF) + 1;
            for (unsigned i = 0; i < ticks; ++i) field_frame::g.tick();
        }
    }
    Field_StepCountPrev = Field_StepCount;
}

// original 0x517640, entry 0 (PSX FUN_801A1AEC): a random walk.
extern "C" void __cdecl Field_ObjectWander(unsigned char* object) { Wander(object, false); }

// original 0x517790, entry 1 (PSX FUN_801A1D04): away from the leader.
extern "C" void __cdecl Field_ObjectFlee(unsigned char* object) { LeaderMove(object, object_kinds::gObjectKinds.flee, 1); }

// original 0x517900, entry 2 (PSX FUN_801A1F2C): toward the leader.
extern "C" void __cdecl Field_ObjectChase(unsigned char* object) { LeaderMove(object, object_kinds::gObjectKinds.chase, 0); }

// original 0x517A70, entry 3 (PSX FUN_801A2154): a random walk inside the
// home box object +0x8E / +0x92, half-widths +0x98 / +0x9A.
extern "C" void __cdecl Field_ObjectWanderHome(unsigned char* object) { Wander(object, true); }

// original 0x517BF0: one frame of a field object (PSX FUN_801A238C). Its script
// context is object +0x80; +0x83 picks the area's script, +0x84 the speed,
// +0x87 counts a timed move's steps.
//
// As the original has it: 16 is divided by Field_MoveSpeeds[+0x84] unchecked
// - entry 0 with steps left faults, as the original does (by reading; the
// fuzz keeps a speed with steps); a step result of 0xFF, a blocked move and
// a blocked step return before the motion - a blocked move with the step
// undone: its script position and loop counter put back.
extern "C" void __cdecl Field_ObjectUpdate(unsigned char* object) {
    auto* const obj = rv::As<records::SpriteObject>(object);
    unsigned char* const context = object + 0x80;
    const unsigned char direction = rv::Current<records::SpriteObject>()->direction;
    // The step's two effects a blocked move undoes: the script position
    // (context +0xA) and the loop counter (context +2).
    const unsigned char loop = obj->loop;
    const std::uint16_t position = obj->script_position;
    if (rv::Current<records::SpriteObject>()->step_frames == 0) {
        const unsigned char speed_index = obj->speed;
        const unsigned char speed = Field_MoveSpeeds[speed_index];
        if (obj->move_steps == 0) {
            if (speed_index != 0) {
                const unsigned char* const descriptor = Area_Descriptors[Game_AreaNumber];
                const auto scripts = static_cast<std::uint32_t>(move_script::Long(descriptor + 0x10));   // a table of 32-bit places
                const unsigned char result = field_objects::g.step(context, move_script::At(static_cast<std::uint32_t>(move_script::Long(move_script::At(scripts + 4u * obj->area_script)))));
                if (obj->pose_wait) field_objects::g.pose_wait(context);
                if (result == 0xFF) return;
                if (rv::Current<records::SpriteObject>()->direction != result) {
                    rv::Current<records::SpriteObject>()->direction = result;
                    context[0] |= 8;
                }
                if (obj->move_steps != 0) {   // the step began a timed move
                    Sprite_Current[0xA] = static_cast<unsigned char>(0x10 / speed);
                    --obj->move_steps;
                    if (!(rv::Current<records::SpriteObject>()->flags2 & 0x80) && field_objects::g.blocked(object)) {
                        rv::Current<records::SpriteObject>()->direction &= 7;
                        context[0] |= 0x18;
                        obj->script_position = position;
                        obj->loop = loop;
                        obj->move_steps = 0;
                        rv::Current<records::SpriteObject>()->step_frames = 0;
                        return;
                    }
                } else if (rv::Current<records::SpriteObject>()->type == 0x0A && (context[0] & 8) && !(context[0] & 0x80)) {
                    Sprite_Current[0xA] = static_cast<unsigned char>(0x10 / speed);
                    rv::Current<records::SpriteObject>()->step_x = 0;
                    rv::Current<records::SpriteObject>()->step_z = 0;
                    rv::Current<records::SpriteObject>()->step_y = 0;
                }
                for (const unsigned char bit : {2, 4}) {
                    if (context[0] & bit) {
                        Sprite_Current[3] = rv::Current<records::SpriteObject>()->state;
                        rv::Current<records::SpriteObject>()->state = bit == 2 ? 8 : 9;
                        Sprite_Current[4] = 0;
                    }
                }
            }
        } else {   // steps left: the next one, unless blocked
            if (!(rv::Current<records::SpriteObject>()->flags2 & 0x80) && field_objects::g.blocked(object)) {
                rv::Current<records::SpriteObject>()->direction &= 7;
                obj->context |= 0x18;
                rv::Current<records::SpriteObject>()->step_frames = 0;
                return;
            }
            Sprite_Current[0xA] = static_cast<unsigned char>(0x10 / speed);
            --obj->move_steps;
            field_objects::g.move(context, rv::Current<records::SpriteObject>()->direction & 7);
            rv::Current<records::SpriteObject>()->direction |= 8;
            context[0] |= 8;
        }
        field_objects::g.settle(object, direction);
    }
    field_objects::g.motion(object);
}

// original 0x517E90 (PSX FUN_801A27A8): the object's talk - the word at
// context +8 is a script (bit 15, the area descriptor's +4 table), a system
// message (bit 13) or a script message. The word is read again after the
// script, which may change it; the one found on entry is put back.
extern "C" unsigned char __cdecl Field_ObjectTalk(unsigned char* context) {
    const std::uint16_t word = move_script::Word(context + 8);
    context[0] &= 0xEF;
    if (word == 0xFFFF) return 0;
    if (word & 0x8000) {
        const unsigned char* const descriptor = Area_Descriptors[Game_AreaNumber];
        const auto scripts = static_cast<std::uint32_t>(move_script::Long(descriptor + 4));   // a table of 32-bit places
        object_kinds::gObjectKinds.event_script(move_script::At(static_cast<std::uint32_t>(move_script::Long(move_script::At(scripts + 4u * (word & 0xFFF))))));
    }
    const std::uint16_t now = move_script::Word(context + 8);
    if (!(now & 0x8000)) {
        if (now & 0x2000) object_kinds::gObjectKinds.msg_system(now & 0xFFFu);
        else object_kinds::gObjectKinds.msg_script(now);
        Field_Request = 2;
    }
    move_script::SetWord(context + 8, word);
    if (rv::Current<records::SpriteObject>()->flags2 & 4) context[0] &= 0xDF;
    unsigned char* const member = Field_ActiveMember;
    std::memcpy(move_script::At(::bof3::Img(AreaW1a_FocusObject_addr)), &member, sizeof member);
    return 1;
}

// original 0x517F30 (PSX FUN_801A28D0): an object touched (context bit 5)
// with no talk (bit 4).
extern "C" void __cdecl Field_ObjectIdle(unsigned char* object) {
    auto* const obj = rv::As<records::SpriteObject>(object);
    if (Field_Request == 2) return;
    if (obj->trigger != 0xFF) object_kinds::gObjectKinds.trigger(object);
    if (Field_Request == 2) return;
    unsigned char* const sprite = Sprite_Current;
    if (sprite[6] == 5) {
        BattleE1_Mode = 0;
        move_script::At(::bof3::Img(BattleE1_Mode_addr))[1] = 0;
        Game_Mode = 7;
        // (object - Sprite_Objects) / 0xA4, signed as the original's imul.
        const auto offset = static_cast<std::int32_t>(Address(object) - Address(rv::Bytes(Sprite_Objects)));
        move_script::At(::bof3::Img(BattleE1_Mode_addr))[0xC] = static_cast<unsigned char>(offset / 0xA4);
        return;
    }
    if (!(Field_StatusBits & 0x40)) {
        if (!(sprite[7] & 8)) {
            sprite[8] = obj->saved_facing;
            obj->context |= 8;
        }
        Field_ScriptFlags &= 0xFEFF;
    }
    obj->context &= 0xDF;
}

// original 0x517FE0, entry 6 (PSX 0x801A2A2C): standing still.
extern "C" void __cdecl Field_ObjectStill(unsigned char* object) {
    auto* const obj = rv::As<records::SpriteObject>(object);
    if (rv::Current<records::SpriteObject>()->type != 8) obj->context |= 0x10;
}

// original 0x518000 (PSX FUN_801A2A60): whether the next step stays in the
// home box. Everything from the argument, not Sprite_Current.
extern "C" unsigned char __cdecl Field_ObjectInHome(unsigned char* object) {
    auto* const obj = rv::As<records::SpriteObject>(object);
    const unsigned char* const step = move_script::At(::bof3::Img(Field_DirectionSteps_addr)) + (object[8] & 7) * 8u;
    const std::uint32_t x = static_cast<std::uint32_t>(move_script::Long(step)) + static_cast<std::uint32_t>(obj->x);
    const std::uint32_t z = static_cast<std::uint32_t>(move_script::Long(step + 4)) + static_cast<std::uint32_t>(obj->z);
    int dx = static_cast<short>(x >> 16) - static_cast<short>(move_script::Word(object + 0x8E));
    if (dx < 0) dx = -dx;
    if (static_cast<short>(obj->half_w) < dx) return 0;
    int dz = static_cast<short>(z >> 16) - static_cast<short>(move_script::Word(object + 0x92));
    if (dz < 0) dz = -dz;
    return static_cast<short>(obj->half_d) >= dz ? 1 : 0;
}

// original 0x518080 (PSX FUN_801A2AEC): the point one step ahead of the object
// - its position plus the step of ITS direction, taken before any call - is
// blocked by the map (for a sprite of Sprite_Current's size and direction), by
// a sprite object, or by a party member. The size is re-read from
// Sprite_Current for each object test, as the original re-reads it.
extern "C" unsigned char __cdecl Field_ObjectBlockedAhead(unsigned char* object) {
    const unsigned d = object[8] & 7;
    const long x = Ahead(object, 0x34, StepX(d, 1));
    const long y = Ahead(object, 0x38, StepY(d, 1));
    if (Al(field_blocked::gFieldBlocked.map_ahead(object))) return 1;
    if (Al(field_blocked::gFieldBlocked.object_at(x, y, Sprite_Current[0x70])) != 0xFF) return 1;
    return Al(field_blocked::gFieldBlocked.member_at(x, y, Sprite_Current[0x70])) != 0xFF ? 1 : 0;
}

// original 0x518100 (PSX FUN_801A2BC8): the map test, (size + 1) steps ahead -
// the object's position, but Sprite_Current's direction and size. A sized
// sprite goes to AreaMap_BlockedWide, a size-0 one to AreaMap_BlockedNarrow
// (Capcom's, unreached by the attract cycle). The original passes the
// direction as a dword whose low byte alone it stored - the rest is whatever
// its stack slot held - and both callees mask it to a byte; ours passes 0
// above it.
extern "C" unsigned char __cdecl Field_MapBlockedAhead(unsigned char* object) {
    const unsigned d = Sprite_Current[8] & 7;
    const unsigned char size = Sprite_Current[0x70];
    const std::uint32_t n = size + 1u;
    const long x = Ahead(object, 0x34, StepX(d, n));
    const long y = Ahead(object, 0x38, StepY(d, n));
    return Al(size != 0 ? field_blocked::gFieldBlocked.wide(x, y, d) : field_blocked::gFieldBlocked.narrow(x, y, d));
}

// original 0x518180 (PSX FUN_801A2C64, not 0x801A2EE0 as psx_pair.py paired
// it): for a sized sprite, whether its leading edge at (x, y) can move in
// `direction`. Only the four axes (odd directions) are tested; any other byte
// is blocked. With X, Y the cell (the high words) and fx, fy the fractions:
//   1: row Y;                              cells X, X+1 and, if fx == 0, X-1
//   5: row Y+1 if fy != 0, else row Y;     the same cells
//   7: column X;                           cells Y, Y+1 and, if fy == 0, Y-1
//   3: column X+1 if fx != 0, else X;      the same cells
// then AreaMap_TooSteep at (x, y) and half a cell to either side across the
// direction (x +- 0x8000 for rows, y +- 0x8000 for columns). The results are
// summed as a byte (so 0x80 + 0x80 would be open - the callees return 0 or 1)
// and the sum's non-zero-ness returned. Cell coordinates are 16-bit: X + 1
// wraps as the original's low word does (the dwords it pushes carry the
// neighbouring argument's bytes above them; AreaMap_ByteAt reads 16 bits).
extern "C" unsigned char __cdecl AreaMap_BlockedWide(long x, long y, unsigned direction) {
    const auto ux = static_cast<std::uint32_t>(x), uy = static_cast<std::uint32_t>(y);
    const auto cx = static_cast<short>(ux >> 16), cy = static_cast<short>(uy >> 16);
    const bool fx = (ux & 0xFFFFu) != 0, fy = (uy & 0xFFFFu) != 0;
    const auto plus = [](short v, int by) { return static_cast<short>(static_cast<std::uint16_t>(v + by)); };
    unsigned char sum = 0;
    switch (direction & 0xFF) {
    case 1:
    case 5: {
        const short row = (direction & 0xFF) == 5 && fy ? plus(cy, 1) : cy;
        sum = static_cast<unsigned char>(sum + Al(field_blocked::gFieldBlocked.cell(cx, row)));
        sum = static_cast<unsigned char>(sum + Al(field_blocked::gFieldBlocked.cell(plus(cx, 1), row)));
        if (!fx) sum = static_cast<unsigned char>(sum + Al(field_blocked::gFieldBlocked.cell(plus(cx, -1), row)));
        sum = static_cast<unsigned char>(sum + Al(field_blocked::gFieldBlocked.steep(x, y)));
        sum = static_cast<unsigned char>(sum + Al(field_blocked::gFieldBlocked.steep(static_cast<long>(ux + 0x8000u), y)));
        sum = static_cast<unsigned char>(sum + Al(field_blocked::gFieldBlocked.steep(static_cast<long>(ux - 0x8000u), y)));
        break;
    }
    case 3:
    case 7: {
        const short column = (direction & 0xFF) == 3 && fx ? plus(cx, 1) : cx;
        sum = static_cast<unsigned char>(sum + Al(field_blocked::gFieldBlocked.cell(column, cy)));
        sum = static_cast<unsigned char>(sum + Al(field_blocked::gFieldBlocked.cell(column, plus(cy, 1))));
        if (!fy) sum = static_cast<unsigned char>(sum + Al(field_blocked::gFieldBlocked.cell(column, plus(cy, -1))));
        sum = static_cast<unsigned char>(sum + Al(field_blocked::gFieldBlocked.steep(x, y)));
        sum = static_cast<unsigned char>(sum + Al(field_blocked::gFieldBlocked.steep(x, static_cast<long>(uy + 0x8000u))));
        sum = static_cast<unsigned char>(sum + Al(field_blocked::gFieldBlocked.steep(x, static_cast<long>(uy - 0x8000u))));
        break;
    }
    default:
        return 1;   // 0, the diagonals and anything past 7, by the jump table's bound
    }
    return sum != 0 ? 1 : 0;
}

// original 0x5183C0 (PSX 0x801A2EE0): Field_MapBlockedAhead's test for a sprite
// of size 0. X, Y the cells (high words of x, y), fx, fy the fractions. By
// direction & 0xFF - 1 through the table 0x518600:
//   1: cell (X, Y); if fx: (X+1, Y), the slopes at (x & ~0xFFFF, y), (x + 0x8000, y)
//   7: cell (X, Y); if fy: (X, Y+1), the slopes at (x, y & ~0xFFFF), (x, y + 0x8000)
//   5: row Y+1 if fy, else Y: cell (X, row); if fx: (X+1, row) and direction 1's slopes
//   3: column X+1 if fx, else X: cell (col, Y); if fy: (col, Y+1) and direction 7's slopes
// then AreaMap_TooSteep(x, y) in each - its slopes through AreaMap_TooSteepAt;
// any other direction: 1, nothing called. The answers are summed as a byte
// (AreaMap_BlockedWide's shape, docs/field-blocked.md 1.1) and its non-zero-ness
// returned in al. The original passes the cells as dwords whose upper halves
// are the neighbouring argument's bytes (it reads its arguments unaligned, and
// X + 1 is added to the whole dword); AreaMap_CellBlocked reads 16 bits. Its one
// caller, Field_MapBlockedAhead (ours), reads al.
extern "C" unsigned char __cdecl AreaMap_BlockedNarrow(long x_arg, long y_arg, unsigned direction) {
    const field_misc::U x = static_cast<field_misc::U>(x_arg), y = static_cast<field_misc::U>(y_arg);
    const auto X = static_cast<short>(x >> 16), Y = static_cast<short>(y >> 16);
    const auto X1 = static_cast<short>((x >> 16) + 1), Y1 = static_cast<short>((y >> 16) + 1);
    const bool fx = (x & 0xFFFF) != 0, fy = (y & 0xFFFF) != 0;
    const auto xl = static_cast<long>(x), yl = static_cast<long>(y);
    const auto x_cell = static_cast<long>(x & 0xFFFF0000u), y_cell = static_cast<long>(y & 0xFFFF0000u);
    const auto x_half = static_cast<long>(x + 0x8000u), y_half = static_cast<long>(y + 0x8000u);
    unsigned char sum;
    switch (direction & 0xFF) {
    case 1:
        sum = field_misc::g.cell_blocked(X, Y);
        if (fx) {
            sum += field_misc::g.cell_blocked(X1, Y);
            sum += field_misc::g.too_steep_at(x_cell, yl);
            sum += field_misc::g.too_steep_at(x_half, yl);
        }
        break;
    case 7:
        sum = field_misc::g.cell_blocked(X, Y);
        if (fy) {
            sum += field_misc::g.cell_blocked(X, Y1);
            sum += field_misc::g.too_steep_at(xl, y_cell);
            sum += field_misc::g.too_steep_at(xl, y_half);
        }
        break;
    case 5: {
        const short row = fy ? Y1 : Y;
        sum = field_misc::g.cell_blocked(X, row);
        if (fx) {
            sum += field_misc::g.cell_blocked(X1, row);
            sum += field_misc::g.too_steep_at(x_cell, yl);
            sum += field_misc::g.too_steep_at(x_half, yl);
        }
        break;
    }
    case 3: {
        const short column = fx ? X1 : X;
        sum = field_misc::g.cell_blocked(column, Y);
        if (fy) {
            sum += field_misc::g.cell_blocked(column, Y1);
            sum += field_misc::g.too_steep_at(xl, y_cell);
            sum += field_misc::g.too_steep_at(xl, y_half);
        }
        break;
    }
    default:
        return 1;
    }
    sum += field_misc::g.too_steep(xl, yl);
    return sum != 0;
}

// original 0x518620 (PSX FUN_801A3100): whether the map cell (x, y) stops a
// step, by the high nibble of its AreaMap_Bytes byte.
extern "C" unsigned char __cdecl AreaMap_CellBlocked(short x, short y) {
    const unsigned nibble = (Al(field_blocked::gFieldBlocked.byte_at(x, y)) >> 4) & 0xF;
    return (BlockingNibbles() >> nibble) & 1u;
}

// original 0x518760 (PSX FUN_801A31F4): whether the ground at (x, y) is too
// steep - AreaMap_Slope for Sprite_Current's direction, sloped (scratch byte
// 0, read after the call) and more than 0x40 in its low 16 bits, signed. The
// original's direction dword carries its caller's ecx above the byte;
// AreaMap_Slope reads the byte alone (and, the byte being 0..7, never indexes
// past its four corners).
extern "C" unsigned char __cdecl AreaMap_TooSteep(long x, long y) {
    const unsigned d = Sprite_Current[8] & 7;
    const long slope = field_blocked::gFieldBlocked.slope(x, y, d);
    if (Scratch()[0] == 0) return 0;
    return static_cast<short>(slope) > 0x40 ? 1 : 0;
}

// original 0x5187A0 (PSX 0x801A3244, a wrapper there too): AreaMap_TooSteep(x,
// y), its eax passed through. Called only by AreaMap_BlockedNarrow (ten sites,
// E8 scan), which adds al.
extern "C" unsigned char __cdecl AreaMap_TooSteepAt(long x, long y) { return field_misc::g.too_steep(x, y); }

// original 0x5187C0, entries 0..3 (PSX FUN_801A3264). `speed` and `flags` are
// read as bytes. Speed 0 with flags bit 1 divides by zero, as the original.
extern "C" unsigned char __cdecl Field_Pace0(unsigned char* object, unsigned speed, unsigned flags) {
    (void)object;
    if (flags & 2) {
        rv::Current<records::SpriteObject>()->step_frames = static_cast<unsigned char>(0x20 / static_cast<int>(speed & 0xFF));
        Sprite_Current[0xA] = static_cast<unsigned char>(rv::Current<records::SpriteObject>()->step_frames >> 1);
    }
    return 1;
}

// originals 0x518800 / 0x518880 / 0x518900, entries 4..6 (PSX 0x801A32D4,
// 0x801A33A4, 0x801A3474).
extern "C" unsigned char __cdecl Field_Pace4(unsigned char* object, unsigned speed, unsigned flags) {
    return PaceCounted(object, speed, flags, 1, 1);
}

extern "C" unsigned char __cdecl Field_Pace5(unsigned char* object, unsigned speed, unsigned flags) {
    return PaceCounted(object, speed, flags, 1, 2);
}

extern "C" unsigned char __cdecl Field_Pace6(unsigned char* object, unsigned speed, unsigned flags) {
    return PaceCounted(object, speed, flags, 2, 1);
}

// original 0x518980: the timed move's motion, every frame (PSX FUN_801A3544).
extern "C" void __cdecl Field_ObjectMotion(unsigned char* object) {
    auto* const obj = rv::As<records::SpriteObject>(object);
    unsigned char* const context = object + 0x80;
    if (rv::Current<records::SpriteObject>()->step_frames != 0) {
        rv::Current<records::SpriteObject>()->x = static_cast<std::int32_t>(static_cast<std::uint32_t>(rv::Current<records::SpriteObject>()->x) +
                                                                 static_cast<std::uint32_t>(rv::Current<records::SpriteObject>()->step_x));
        rv::Current<records::SpriteObject>()->z = static_cast<std::int32_t>(static_cast<std::uint32_t>(rv::Current<records::SpriteObject>()->z) +
                                                                 static_cast<std::uint32_t>(rv::Current<records::SpriteObject>()->step_z));
        rv::Store<unsigned>(rv::Current<records::SpriteObject>()->height_whole, static_cast<unsigned>(rv::Current<records::SpriteObject>()->height_whole + move_script::Word(Sprite_Current + 0x14)));
        --rv::Current<records::SpriteObject>()->step_frames;
        if (rv::Current<records::SpriteObject>()->step_frames == 0) {   // arrived
            if (!(context[0] & 0x80))
                rv::Store<unsigned>(rv::Current<records::SpriteObject>()->height_whole, static_cast<unsigned>(field_objects::g.elevation(rv::Current<records::SpriteObject>()->x, rv::Current<records::SpriteObject>()->z)));
            obj->counter = 0;
            const unsigned char after = Sprite_Current[2];
            if (static_cast<int>(after) - 1 < 3) {
                const auto frames = static_cast<unsigned char>((after & 3) * 0x1E);
                obj->pose_wait = frames;
                if (frames != 0) {   // a wait of 30, 60 or 90 frames in pose 5
                    field_objects::g.pose_wait(context);
                    if (!(rv::Current<records::SpriteObject>()->flags2 & 8)) {
                        rv::Current<records::SpriteObject>()->direction &= 7;
                        context[0] |= 8;
                    }
                    object[0xA1] = (context[0] & 0x40) ? 0xFF : (rv::Current<records::SpriteObject>()->script_pos < 8 ? 6 : 0);
                }
            }
        }
    }
    field_objects::g.party_apply(context);
    if (Field_Request != 2) {
        if (rv::Current<records::SpriteObject>()->step_frames > Sprite_Current[0xA]) context[0] &= 0xEF;
        else context[0] |= 0x10;
    }
}

// original 0x518AC0, entry 5 (PSX 0x801A3724): the wait Sprite_SetPoseWait
// begins; context bit 4 while it counts, then the pose back.
extern "C" void __cdecl Field_ObjectWait(unsigned char* object) {
    auto* const obj = rv::As<records::SpriteObject>(object);
    if (obj->pose_wait == 0) {
        rv::Current<records::SpriteObject>()->state = Sprite_Current[3];
        return;
    }
    --obj->pose_wait;
    obj->context |= 0x10;
}

// original 0x518B00: a pose held while the context's +1 counts down (PSX
// FUN_801A3764): Sprite_Current[3] = [1], [1] = 5. The argument is unused.
extern "C" void __cdecl Sprite_SetPoseWait(unsigned char* context) {
    (void)context;
    Sprite_Current[3] = rv::Current<records::SpriteHead>()->state;
    rv::Current<records::SpriteHead>()->state = 5;
}

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

// original 0x518B40 (PSX FUN_801A37CC): an object pushed for more than 30
// frames (+0x9C) steps aside - to the first of four directions, by the
// leader's facing, that is open (and, for pose 3, inside the home box).
extern "C" void __cdecl Field_ObjectIdleLong(unsigned char* object) {
    auto* const obj = rv::As<records::SpriteObject>(object);
    const unsigned char speed_index = obj->speed;
    obj->counter = 0;
    if (speed_index == 0) return;
    if (rv::Current<records::SpriteObject>()->state == 6 || rv::Current<records::SpriteObject>()->state == 4 || Sprite_Current[3] == 4) return;
    object_kinds::gObjectKinds.pace0(object, Field_MoveSpeeds[speed_index], 2);
    const unsigned char direction = rv::Current<records::SpriteObject>()->direction;
    unsigned char n = 0;
    for (; n < 4; ++n) {
        rv::Current<records::SpriteObject>()->direction = move_script::At(::bof3::Img(kAsideRows))[n + (ObjTrio[0].direction & 7) * 4u];
        if (object_kinds::gObjectKinds.blocked_ahead(object)) continue;
        if (rv::Current<records::SpriteObject>()->state == 3 || Sprite_Current[3] == 3) {
            if (object_kinds::gObjectKinds.in_home(object)) break;
            continue;
        }
        break;
    }
    // (The original tests n < 4 again after a found direction - never false.)
    if (n >= 4 || rv::Current<records::SpriteObject>()->step_frames == 0) {
        rv::Current<records::SpriteObject>()->direction = direction & 7;
        rv::Current<records::SpriteObject>()->step_frames = 0;
        return;
    }
    if (obj->pose_wait != 0) {
        // [3] was not 4 on entry; only a callee changing it reaches this.
        if (Sprite_Current[3] == 4) {
            rv::Current<records::SpriteObject>()->direction = direction;   // unmasked here
            return;
        }
        obj->pose_wait = 0;
        rv::Current<records::SpriteObject>()->state = Sprite_Current[3];
    }
    unsigned char* const context = object + 0x80;
    object_kinds::gObjectKinds.move(context, rv::Current<records::SpriteObject>()->direction);
    rv::Current<records::SpriteObject>()->direction |= 8;
    if (!(rv::Current<records::SpriteObject>()->flags2 & 8)) context[0] |= 8;
}

// original 0x518CA0 (PSX FUN_801A3A0C): up to four quarter turns off a blocked
// direction. Context bit 3 marks a change against the entry direction & 7 -
// compared with the unmasked byte, so a moving sprite (bit 3) always counts
// as changed; kept, the PSX compares the same way.
extern "C" unsigned char __cdecl Field_ObjectOpenDirection(unsigned char* object) {
    auto* const obj = rv::As<records::SpriteObject>(object);
    const unsigned char entry = rv::Current<records::SpriteObject>()->direction & 7;
    unsigned char turns = 0;
    do {
        if (!object_kinds::gObjectKinds.blocked_ahead(object)) break;
        rv::Current<records::SpriteObject>()->direction = static_cast<unsigned char>((rv::Current<records::SpriteObject>()->direction + 2) & 7);
        ++turns;
    } while (turns < 4);
    if (entry != rv::Current<records::SpriteObject>()->direction) obj->context |= 8;
    if (turns >= 4) return 0xFF;
    return rv::Current<records::SpriteObject>()->direction & 7;
}

// original 0x518D10: the direction settled after the step (PSX FUN_801A3AEC);
// `direction` is Sprite_Current[8] as Field_ObjectUpdate found it.
extern "C" void __cdecl Field_ObjectSettle(unsigned char* object, unsigned char direction) {
    auto* const obj = rv::As<records::SpriteObject>(object);
    records::SpriteObject* const sprite = rv::Current<records::SpriteObject>();
    if (sprite->type == 0x0A) {
        if (sprite->step_frames == 0) return;
        if (!(Field_ActiveMember[0x80] & 8)) return;
        field_objects::g.party_move(object, sprite->direction & 7);
        obj->context &= 0xF7;
        return;
    }
    if (sprite->flags2 & 8) {
        obj->context &= 0xF7;
        return;
    }
    obj->context |= 8;
    if (rv::Current<records::SpriteObject>()->step_frames != 0) rv::Current<records::SpriteObject>()->direction |= 8;
    else rv::Current<records::SpriteObject>()->direction &= 7;
    if (Scratch()[3] == 0xFF) {   // op E4's flag, consumed
        Scratch()[3] = 0;
        return;
    }
    if (direction == rv::Current<records::SpriteObject>()->direction) obj->context &= 0xF7;
}

// original 0x518DD0 (PSX FUN_801A3C18): a random odd direction. The original
// pushes 0x80 for Rand, which takes nothing.
extern "C" unsigned char __cdecl Field_ObjectRandomTurn(unsigned char* object) {
    auto* const obj = rv::As<records::SpriteObject>(object);
    const auto direction = static_cast<unsigned char>((object_kinds::gObjectKinds.rand() & 6) | 1);
    if ((rv::Current<records::SpriteObject>()->direction & 7) == direction) return rv::Current<records::SpriteObject>()->direction;
    rv::Current<records::SpriteObject>()->direction = direction;
    obj->context |= 8;
    return rv::Current<records::SpriteObject>()->direction;
}

// original 0x518E20 (PSX twin 0x801A3C9C): Field_ObjectFlee's pick.
extern "C" unsigned char __cdecl Field_ObjectFleeDirection(unsigned char* object) { return LeaderDirection(object, 1); }

// original 0x518ED0 (PSX twin 0x801A3DBC): Field_ObjectChase's pick.
extern "C" unsigned char __cdecl Field_ObjectChaseDirection(unsigned char* object) { return LeaderDirection(object, 0); }

// original 0x518F80 (PSX twin 0x801A3EDC): of four quarter turns, the
// unblocked one whose step lands farthest (farthest not 0) or nearest (0) to the
// leader's next point, by the sum of the two distances; 0xFF for none.
extern "C" unsigned char __cdecl Field_ObjectBestDirection(unsigned char* object, unsigned char farthest) {
    const U pace = ObjTrio[0].step_frames;
    const U point_z = pace * static_cast<std::uint32_t>(ObjTrio[0].step_z) + static_cast<std::uint32_t>(ObjTrio[0].z);
    const U point_x = pace * static_cast<std::uint32_t>(ObjTrio[0].step_x) + static_cast<std::uint32_t>(ObjTrio[0].x);
    records::SpriteHead* s = rv::Current<records::SpriteHead>();
    if ((s->direction & 1) == 0) s->direction = static_cast<unsigned char>(s->direction + 1);
    unsigned char best = 0xFF;
    SetULong(object + 0x94, farthest != 0 ? 0u : 0x1000000u);
    for (int turn = 4; turn != 0; --turn) {
        const unsigned char blocked = SH_CALL(Field_ObjectBlockedAhead)(object);
        s = rv::Current<records::SpriteHead>();
        if (blocked == 0) {
            const unsigned d = s->direction & 7u;
            const U score = static_cast<U>(Abs32(DirStepZ(d) + static_cast<U>(s->z) - point_z)) +
                            static_cast<U>(Abs32(DirStepX(d) + static_cast<U>(s->x) - point_x));
            const U kept = ULong(object + 0x94);
            if (farthest != 0 ? kept < score : kept > score) {
                SetULong(object + 0x94, score);
                s = rv::Current<records::SpriteHead>();
                best = s->direction;
            }
        }
        s->direction = static_cast<unsigned char>((s->direction + 2) & 7);
    }
    return best;
}

// original 0x5190A0: a party member's move toward `direction` (PSX
// FUN_801A4088): the velocity op D4's record of its party slot filled for the
// frames Sprite_Current +0xA says - the two tilts and the turn, each a step
// per frame. The slot is (object - Sprite_ObjectsExtra) / 0xA4, its low byte
// taken signed and 4 when negative, as Party_ApplyRecord's is; a record with
// bit 0 of +0 set is left alone. With a velocity (+0xC + +0x10 non-zero) the
// ground's rise one step ahead, `Field_DirectionSteps[direction]`, becomes a
// tilt through Math_Ratan2(rise, 128): directions with bit 1 (2, 3, 6, 7) tilt
// +0x68's axis and zero +0x64, the others the reverse; the rise's sign is
// flipped except for direction 3 and direction 1 respectively.
//
// As the original has it: the object pointer is only arithmetic; the slot
// byte can index up to record 127; the tilts' targets are the sprite's +0x64 /
// +0x68 as found on entry unless replaced, the one zeroed included - so the
// zeroed axis steps from its old value to its old value, a count of 0; the
// divisions are by n * 2 and n * 4 with n the byte +0xA, never 0 there (the
// record's counts are n << 1 and n << 2 as bytes, which wrap); directions 5
// and 6 turn the long way when the sprite's angle & 0xFFF is past direction
// 3's; any direction byte indexes both tables (callers pass 0..7).
extern "C" void __cdecl Party_MoveMember(unsigned char* object, unsigned char direction) {
    records::SpriteHead* sprite = rv::Current<records::SpriteHead>();
    std::int32_t tilt_x = static_cast<std::int32_t>(sprite->angle[0]);
    std::int32_t tilt_y = static_cast<std::int32_t>(sprite->angle[1]);
    const std::int32_t index = static_cast<std::int32_t>(AddressMoveCmds(object) - AddressMoveCmds(rv::Bytes(Sprite_ObjectsExtra))) / 0xA4;
    signed char slot = static_cast<signed char>(index & 0xFF);
    if (slot < 0) slot = 4;
    unsigned char* const record = MoveScript_PartyRecords + slot * 16;
    if (record[0] & 1) return;
    if (Add(sprite->step_z, sprite->step_x) != 0) {
        const unsigned char* const step = move_script::At(AddressMoveCmds(Field_DirectionSteps) + direction * 8u);
        const long ground = move_cmds::g.elevation(Add(move_script::Long(step), sprite->x), Add(move_script::Long(step + 4), sprite->z));
        sprite = rv::Current<records::SpriteHead>();
        const auto rise = static_cast<std::uint16_t>(static_cast<std::uint32_t>(ground) - sprite->height_whole);
        if (direction & 2) {
            sprite->angle[0] = 0;
            const auto slope = static_cast<short>(direction == 3 ? rise : 0u - rise);
            tilt_y = move_cmds::g.ratan2(static_cast<float>(slope), 128.0f);
        } else {
            const auto slope = static_cast<short>(direction == 1 ? rise : 0u - rise);
            tilt_x = move_cmds::g.ratan2(static_cast<float>(slope), 128.0f);
            rv::Current<records::SpriteHead>()->angle[1] = 0;
        }
        sprite = rv::Current<records::SpriteHead>();
    }
    const unsigned char frames = rv::Bytes(sprite)[0xA];
    if (frames == 0) {
        move_script::SetLong(record + 0xC, 0);
        move_script::SetLong(record + 8, 0);
        move_script::SetLong(record + 4, 0);
        sprite->angle[2] = DirectionAngle(direction);
        return;
    }
    record[1] = static_cast<unsigned char>(frames << 1);
    record[2] = static_cast<unsigned char>(rv::Bytes(sprite)[0xA] << 2);
    move_script::SetLong(record + 4, Sub(tilt_x, static_cast<std::int32_t>(sprite->angle[0])) / static_cast<std::int32_t>(rv::Bytes(sprite)[0xA] * 2u));
    move_script::SetLong(record + 8, Sub(tilt_y, static_cast<std::int32_t>(sprite->angle[1])) / static_cast<std::int32_t>(rv::Bytes(sprite)[0xA] * 2u));
    std::uint16_t turn_to = DirectionAngle(direction);
    if ((direction == 5 || direction == 6) &&
        static_cast<std::int32_t>(DirectionAngle(3)) < (static_cast<std::int32_t>(sprite->angle[2]) & 0xFFF))
        turn_to = static_cast<std::uint16_t>(turn_to + 0x1000);
    move_script::SetLong(record + 0xC, (static_cast<short>(turn_to) - (static_cast<std::int32_t>(sprite->angle[2]) & 0xFFF)) /
                              static_cast<std::int32_t>(rv::Bytes(sprite)[0xA] * 4u));
}

// original 0x5192A0: the update of an attached object (PSX ov_entry_801A4418):
// it stands where the object its handle names stands, plus an offset turned
// by its own angles, then updates as any object does. The handle's object is
// Sprite_Objects[n], or Sprite_ObjectsExtra[n - 0x1E] - the PSX's one array
// of 0x98-byte objects is two of 0xA4 on the PC.
extern "C" void __cdecl Field_ObjectFollow(unsigned char* object) {
    const unsigned char n = field_objects::g.by_handle(Sprite_Current[0x18]);
    const unsigned char* const other = n >= 0x1E ? rv::Bytes(&Sprite_ObjectsExtra[n - 0x1Eu]) : rv::Bytes(&Sprite_Objects[n]);
    if (rv::Current<records::SpriteObject>()->step_frames == 0) {
        rv::Current<records::SpriteObject>()->draw_flags |= 0x20;
        for (const unsigned at : {0x34u, 0x38u, 0x3Cu}) move_script::SetLong(Sprite_Current + at, move_script::Long(other + at));
        if (rv::Current<records::SpriteObject>()->type != 0x0A)
            for (const unsigned at : {0x64u, 0x68u, 0x6Cu}) move_script::SetLong(Sprite_Current + at, move_script::Long(other + at));
        long offset[3];
        field_objects::g.attach_offset(offset, Sprite_Current[0x1C]);
        rv::Current<records::SpriteObject>()->x = static_cast<std::int32_t>(static_cast<std::uint32_t>(rv::Current<records::SpriteObject>()->x) + static_cast<std::uint32_t>(offset[0]));
        rv::Current<records::SpriteObject>()->z = static_cast<std::int32_t>(static_cast<std::uint32_t>(rv::Current<records::SpriteObject>()->z) + static_cast<std::uint32_t>(offset[1]));
        rv::Store<unsigned>(rv::Current<records::SpriteObject>()->height_whole, static_cast<unsigned>(rv::Current<records::SpriteObject>()->height_whole + static_cast<std::uint16_t>(offset[2])));
    }
    field_objects::g.update(object);
    rv::Current<records::SpriteObject>()->direction &= 7;
}

// original 0x5193B0, entry 8 (PSX 0x801A459C): a fade to black, the pose for
// context bit 1. The original jumps through 0x65F654 by Sprite_Current[4]
// with no bound; entries 2 and 3 of that table are 0x65F65C's - FadeIn's
// cases - and 4 on are the cases of a function at 0x5198xx, which this code
// cannot run in its frame: it stops instead.
extern "C" void __cdecl Field_ObjectFadeOut(unsigned char* object) {
    (void)object;   // unused, as the original's
    switch (Sprite_Current[4]) {
    case 0: FadeOutStart(); return;
    case 1: FadeOutStep(); return;
    case 2: FadeInStart(); return;
    case 3: FadeInStep(); return;
    default:
        bof3::Fatal("Field_ObjectFadeOut: sub-state %u is past the original's jump table (0x65F654)", (unsigned)Sprite_Current[4]);
    }
}

// original 0x5193D0 (PSX twin 0x801A45E0): Field_ObjectFadeOutSteps[0].
extern "C" void __cdecl Field_ObjectFadeOutStart(void) {
    const unsigned char index = SH_CALL(Sprite_SetTint)(Sprite_Current, 0x1F, 0x1F, 0x1F, 1);
    Field_ActiveMember[0x9F] = index;
    rv::Current<records::SpriteHead>()->flags = static_cast<unsigned char>(rv::Current<records::SpriteHead>()->flags & 0xBF);
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

// original 0x5194E0, entry 9 (PSX 0x801A47DC): a fade from black, the pose for
// context bit 2. Its table 0x65F65C has no bound either; 2 and up are the
// cases of 0x65F664's function.
extern "C" void __cdecl Field_ObjectFadeIn(unsigned char* object) {
    (void)object;
    switch (Sprite_Current[4]) {
    case 0: FadeInStart(); return;
    case 1: FadeInStep(); return;
    default:
        bof3::Fatal("Field_ObjectFadeIn: sub-state %u is past the original's jump table (0x65F65C)", (unsigned)Sprite_Current[4]);
    }
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

// original 0x519600 (PSX FUN_801A4A10): an object with context bit 0, instead
// of its kind. No argument.
extern "C" void __cdecl Field_ObjectLinked() {
    const unsigned char* const sprite = Sprite_Current;
    bool call;
    if (!(sprite[0] & 0x40)) {
        const unsigned char type = sprite[6];
        call = type == 1 || type == 4 || type == 3 || (Field_ActiveMember[0xA0] & 0x7F) != 0x7F;
    } else {
        call = sprite[6] == 4 || (Field_ActiveMember[0xA0] & 0x7F) != 0x7F;
    }
    if (call) {
        object_kinds::gObjectKinds.area_handler(ObjTrio[0].char_id);
        return;
    }
    Field_ActiveMember[0x80] &= 0xFE;
}

// original 0x519670 (PSX FUN_801A4AD0): the same without the map.
extern "C" unsigned char __cdecl Field_ObjectBlocked(unsigned char* object) {
    const unsigned d = object[8] & 7;
    const long x = Ahead(object, 0x34, StepX(d, 1));
    const long y = Ahead(object, 0x38, StepY(d, 1));
    if (Al(field_blocked::gFieldBlocked.object_at(x, y, Sprite_Current[0x70])) != 0xFF) return 1;
    return Al(field_blocked::gFieldBlocked.member_at(x, y, Sprite_Current[0x70])) != 0xFF ? 1 : 0;
}

// original 0x5196E0, entry 10 (PSX FUN_801A4B84): turning to +0x85 an eighth
// every four frames, then one step. The original takes an absolute value of
// (target - direction) & 7, which cannot be negative - kept as the plain
// masked difference.
extern "C" void __cdecl Field_ObjectTurn(unsigned char* object) {
    auto* const obj = rv::As<records::SpriteObject>(object);
    if (rv::Current<records::SpriteObject>()->flags2 & 8) rv::Current<records::SpriteObject>()->direction = obj->saved_facing;
    if (Sprite_Current[0xA] != 0) {
        --Sprite_Current[0xA];
        return;
    }
    const unsigned char target = obj->saved_facing;
    const unsigned char facing = rv::Current<records::SpriteObject>()->direction;
    if (target == facing) {
        rv::Current<records::SpriteObject>()->state = Sprite_Current[3];
        if (!(rv::Current<records::SpriteObject>()->flags2 & 0x80) && object_kinds::gObjectKinds.blocked_ahead(object)) {
            rv::Current<records::SpriteObject>()->direction &= 7;
            Sprite_Current[0xA] = 0;
            return;
        }
        unsigned char* const context = object + 0x80;
        object_kinds::gObjectKinds.move(context, rv::Current<records::SpriteObject>()->direction);   // the whole byte, bit 3 and all
        rv::Current<records::SpriteObject>()->direction |= 8;
        context[0] |= 8;
        Sprite_Current[0xA] = static_cast<unsigned char>(rv::Current<records::SpriteObject>()->step_frames >> 1);
        return;
    }
    Sprite_Current[0xA] = 3;
    const int step = ((target - facing) & 7) < 4 ? 1 : -1;
    rv::Current<records::SpriteObject>()->step_z = 0;
    rv::Current<records::SpriteObject>()->step_x = 0;
    rv::Current<records::SpriteObject>()->direction = static_cast<unsigned char>((rv::Current<records::SpriteObject>()->direction + step) & 7);
    obj->context |= 8;
}

// original 0x5197F0 (PSX 0x801A4D24): the velocity op D4 left in the party slot's record
// (PSX FUN_801A4D24). Its argument is unused.
extern "C" void __cdecl Party_ApplyRecord(unsigned char* context) {
    (void)context;
    records::SpriteObject* sprite = rv::Current<records::SpriteObject>();
    if (sprite->type != 0x0A) return;
    unsigned char slot = PartySlot();
    if (static_cast<signed char>(slot) < 0) slot = 4;
    unsigned char* const r = MoveScript_PartyRecords + static_cast<signed char>(slot) * 16;
    if (r[1] != 0) {
        --r[1];
        sprite->angle[0] = static_cast<std::int32_t>(static_cast<std::uint32_t>(static_cast<std::int32_t>(sprite->angle[0])) + static_cast<std::uint32_t>(move_script::Long(r + 4)));
        rv::Current<records::SpriteObject>()->angle[1] = static_cast<std::int32_t>(static_cast<std::uint32_t>(static_cast<std::int32_t>(rv::Current<records::SpriteObject>()->angle[1])) +
                                                                 static_cast<std::uint32_t>(move_script::Long(r + 8)));
        sprite = rv::Current<records::SpriteObject>();
    }
    if (r[2] != 0) {
        --r[2];
        sprite->angle[2] = static_cast<std::int32_t>(static_cast<std::uint32_t>(static_cast<std::int32_t>(sprite->angle[2])) + static_cast<std::uint32_t>(move_script::Long(r + 0xC)));
    }
}

void Field3_Inject() {
    BOF3_INJECT(Field_Frame);
    BOF3_INJECT(Field_FrameScripted);
    BOF3_INJECT(Field_LoadingFrame);
    BOF3_INJECT(Mode11_FieldFrame);
    BOF3_INJECT(Menu_Frame);
    BOF3_INJECT(Shop_Frame);
    BOF3_INJECT(Mode8_Step5);
    BOF3_INJECT(Mode8_Step8);
    BOF3_INJECT(Field_MembersFrame);
    BOF3_INJECT(Field_ObjectsScreen);
    BOF3_INJECT(Sprite_UpdateObjectScreens);
    BOF3_INJECT(Field_ObjectsFrame);
    BOF3_INJECT(Field_ObjectWander);
    BOF3_INJECT(Field_ObjectFlee);
    BOF3_INJECT(Field_ObjectChase);
    BOF3_INJECT(Field_ObjectWanderHome);
    BOF3_INJECT(Field_ObjectUpdate);
    BOF3_INJECT(Field_ObjectTalk);
    BOF3_INJECT(Field_ObjectIdle);
    BOF3_INJECT(Field_ObjectStill);
    BOF3_INJECT(Field_ObjectInHome);
    BOF3_INJECT(Field_ObjectBlockedAhead);
    BOF3_INJECT(Field_MapBlockedAhead);
    BOF3_INJECT(AreaMap_BlockedWide);
    BOF3_INJECT(AreaMap_BlockedNarrow);
    BOF3_INJECT(AreaMap_CellBlocked);
    BOF3_INJECT(AreaMap_TooSteep);
    BOF3_INJECT(AreaMap_TooSteepAt);
    BOF3_INJECT(Field_Pace0);
    BOF3_INJECT(Field_Pace4);
    BOF3_INJECT(Field_Pace5);
    BOF3_INJECT(Field_Pace6);
    BOF3_INJECT(Field_ObjectMotion);
    BOF3_INJECT(Field_ObjectWait);
    BOF3_INJECT(Sprite_SetPoseWait);
    BOF3_INJECT(MoveCmd_OpE7);
    BOF3_INJECT(Field_ObjectIdleLong);
    BOF3_INJECT(Field_ObjectOpenDirection);
    BOF3_INJECT(Field_ObjectSettle);
    BOF3_INJECT(Field_ObjectRandomTurn);
    BOF3_INJECT(Field_ObjectFleeDirection);
    BOF3_INJECT(Field_ObjectChaseDirection);
    BOF3_INJECT(Field_ObjectBestDirection);
    BOF3_INJECT(Party_MoveMember);
    BOF3_INJECT(Field_ObjectFollow);
    BOF3_INJECT(Field_ObjectFadeOut);
    BOF3_INJECT(Field_ObjectFadeOutStart);
    BOF3_INJECT(Field_ObjectFadeOutStep);
    BOF3_INJECT(Field_ObjectFadeIn);
    BOF3_INJECT(Field_ObjectFadeInStart);
    BOF3_INJECT(Field_ObjectFadeInStep);
    BOF3_INJECT(Field_ObjectLinked);
    BOF3_INJECT(Field_ObjectBlocked);
    BOF3_INJECT(Field_ObjectTurn);
    BOF3_INJECT(Party_ApplyRecord);
}

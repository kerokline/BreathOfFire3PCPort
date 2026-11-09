// The event script's field side: the party leader's frame (0x52D8F0) and its
// first two states (0x52D920, 0x52DA50 with its sub-state 0x52DA70), the area
// set-up's party placement (0x533110, 0x5334A0, 0x533580, 0x533BA0, 0x533CE0),
// a member joining (0x533EF0), the party-set loader (0x5367E0..0x536AC0), and
// the small helpers they share. docs/field-event.md has each function, its
// evidence, and the fuzz below with its negative controls.
//
// "Member i" is ObjTrio + i * 0x14C, the party's three field objects; their
// +0x80..+0x123 is a copy of an actor record 0x903A70 + a * 0xA4, a the
// member's Character_IndexById entry (kept in the member's +0x148).
//
// As in the original, every global is read again after each call and every
// object through Sprite_Current or Field_State each time it is used.
#include "game/field_event.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/field_event_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace field_event {

namespace {
// A function pointer as another function type, through an integer.
template <class To> To Fn(std::uint32_t address) { return reinterpret_cast<To>(static_cast<std::uintptr_t>(address)); }
}  // namespace

// Every callee. Those this module implements are ours (their names are bound
// to our functions by symbols.gen.h); the unnamed ones are Capcom's addresses,
// typed from their call sites and, where read, their first instructions
// (docs/field-event.md, section 3).
const Callees kOriginals = {
    Sprite_RestoreClut, Field_CopyInput, reinterpret_cast<const std::uint32_t*>(::bof3::Img(Field_LeaderStates_addr)),
    Sprite_ClearSteps, MapView_GroundAt, Field_LeaderAnimation,
    reinterpret_cast<const std::uint32_t*>(::bof3::Img(Field_LeaderControlSteps_addr)), Sprite_ScriptTick,
    Fn<unsigned char (__cdecl*)(unsigned)>(::bof3::Code(bof3::addr::Field_Tile89)), Fn<unsigned char (__cdecl*)(unsigned)>(::bof3::Code(bof3::addr::Field_Tile8A)),
    Fn<unsigned char (__cdecl*)()>(::bof3::Code(bof3::addr::Field_LeaderIdleTest)),
    {Fn<unsigned char (__cdecl*)()>(::bof3::Code(bof3::addr::Field_LeaderCellEvent)), Fn<unsigned char (__cdecl*)()>(::bof3::Code(bof3::addr::Field_LeaderTalkTest)),
     Fn<unsigned char (__cdecl*)()>(::bof3::Code(bof3::addr::Field_LeaderSwapTest)), Fn<unsigned char (__cdecl*)()>(::bof3::Code(bof3::addr::Field_LeaderMenuTest)),
     Fn<unsigned char (__cdecl*)()>(::bof3::Code(bof3::addr::Field_LeaderCheckTest)), Fn<unsigned char (__cdecl*)()>(::bof3::Code(bof3::addr::Field_LeaderRequest4Test)),
     Fn<unsigned char (__cdecl*)()>(::bof3::Code(bof3::addr::Field_LeaderRequest9Test))},
    Fn<void (__cdecl*)()>(::bof3::Code(bof3::addr::Field_LeaderDirection)),
    Area_ZoneAt, Rand, Party_Count, Fn<unsigned char (__cdecl*)(unsigned, unsigned, unsigned)>(::bof3::Code(bof3::addr::Actor_EquipCount)),
    Fn<unsigned char (__cdecl*)(unsigned)>(::bof3::Code(bof3::addr::Sprite_EnsureAnimation)),
    Flags_Test, Sprite_SetAnimationBank, Field_MemberSprite, Field_PartyLoad, Sprite_ReleaseTint, Sprite_LoadPalette,
    Field_PartyPosition, Fn<void (__cdecl*)(long, long, unsigned)>(::bof3::Code(bof3::addr::Field_LeaderPlaceOffset)),
    Party_ClearActive, Field_LeaderFrame, Field_MemberFrame,
    reinterpret_cast<const std::uint32_t*>(::bof3::Img(Field_PendingJumps_addr)),
    Party_ClearAll, Sprite_SetTint, Field_MemberTimers,
    Party_JoinReset, Member_ClearState,
    PartySet_LoadFirst, PartySet_LoadSecond, File_LoadDone, Task_Sleep, PartySet_Find, PartySet_Select,
    Fn<void (__cdecl*)()>(::bof3::Code(bof3::addr::PartySet_ErrorLoop)), LoadDatFile,
    MapView_CheckHeightScale, AreaMap_Elevation,
    Tint_Release,
};
Callees g = kOriginals;

}  // namespace field_event

namespace {

using namespace move_script;
namespace at = field_event::at;
using HandlerFieldEvent = field_event::Handler;

constexpr unsigned kMemberBytes = 0x14C, kActorBytes = 0xA4;

std::uint32_t Address(const volatile void* p) { return Low32(p); }
unsigned char* Member(unsigned i) { return rv::Bytes(&ObjTrio[i]); }

void Dispatch(const std::uint32_t* table, unsigned index) {
    reinterpret_cast<HandlerFieldEvent>(static_cast<std::uintptr_t>(table[index]))();
}

}  // namespace

// --- the party leader's frame and its states --------------------------------

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

// --- the zone counter ------------------------------------------------------

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

// --- small helpers -----------------------------------------------------------

// original 0x572590 (PSX 0x8015598C): MapView_HeightScale = 1 for the object
// at 0x905DA0 with its +0xB bit 0, or for any other Sprite_Current when
// Field_State +0x138 bit 0 is set; otherwise left as it is.
extern "C" void __cdecl MapView_CheckHeightScale(void) {
    unsigned char* const c = Sprite_Current;
    if (c == At(::bof3::Img(FieldEvent_Object905DA0_addr))) {
        if (c[0xB] & 1) MapView_HeightScale = 1;
        return;
    }
    if (Field_State[0x138] & 1) MapView_HeightScale = 1;
}

// original 0x572570 (PSX 0x801559E4): the ground at (x, z), with the height
// scale set for the call and cleared after it.
extern "C" long __cdecl MapView_GroundAt(long x, long z) {
    field_event::g.height_check();
    const long ground = field_event::g.elevation(x, z);
    MapView_HeightScale = 0;
    return ground;
}

// original 0x454DC0 (PSX 0x80197718): every tint record of `sprite` released,
// the 32 records of MoveScript_TintRecords in order, compared as whole dwords.
extern "C" void __cdecl Sprite_ReleaseTint(unsigned char* sprite) {
    for (unsigned i = 0; i < 32; ++i)
        if (static_cast<std::uint32_t>(move_script::Long(move_script::At(::bof3::Img(MoveScript_TintRecords_addr) + 0x8 + i * 12))) == Address(sprite))
            field_event::g.tint_release(static_cast<unsigned char>(i));
}

// --- the party's placement ---------------------------------------------------

// --- the party set -------------------------------------------------------------

void FieldEvent_Inject() {
    if (bof3::WantsShadow("field_event")) field_event::SelfTest();
    {
        // DIV-0153: after the self-test, which compared Capcom's roll.
        const std::uint8_t was = 0, is = 1;
        bof3::PatchBytes("MantleCap", Low32(&field_event::g_mantle_cap), &was, &is, 1);
        bof3::Log("DIV-0153    the Holy Mantle's doubled encounter threshold stops at 255 instead of wrapping (on unless the line above says OFF)");
    }
    BOF3_INJECT(Field_LeaderFrame);
    BOF3_INJECT(Field_LeaderStart);
    BOF3_INJECT(Field_LeaderControl);
    BOF3_INJECT(Field_LeaderStand);
    BOF3_INJECT(Field_EncounterThresholdRoll);
    BOF3_INJECT(Area_ZoneAt);
    BOF3_INJECT(Field_LeaderAnimation);
    BOF3_INJECT(MapView_GroundAt);
    BOF3_INJECT(MapView_CheckHeightScale);
    BOF3_INJECT(Sprite_ReleaseTint);
}

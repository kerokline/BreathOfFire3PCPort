// The event script's field side: the party leader's frame (0x52D8F0) and its
// first two states (0x52D920, 0x52DA50 with its sub-state 0x52DA70), the area
// set-up's party placement (0x533110, 0x5334A0, 0x533580, 0x533BA0, 0x533CE0),
// a member joining (0x533EF0), the party-set loader (0x5367E0..0x536AC0), and
// the small helpers they share. docs/field-event.md has each function, its
// evidence, and the fuzz below with its negative controls.
//
// "Member i" is ObjTrio + i * 0x14C, the party's three field objects; their
// +0x80..+0x123 is a copy of an actor record 0x903A70 + a * 0xA4, a the
// member's MoveScript_EffectState entry (kept in the member's +0x148).
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
    Sprite_RestoreClut, Field_CopyInput, reinterpret_cast<const std::uint32_t*>(at::kLeaderStates),
    Sprite_ClearSteps, MapView_GroundAt, Field_LeaderAnimation,
    reinterpret_cast<const std::uint32_t*>(at::kControlStates), Sprite_ScriptTick,
    Fn<unsigned char (__cdecl*)(unsigned)>(0x535120), Fn<unsigned char (__cdecl*)(unsigned)>(0x535240),
    Fn<unsigned char (__cdecl*)()>(0x5301F0),
    {Fn<unsigned char (__cdecl*)()>(0x531950), Fn<unsigned char (__cdecl*)()>(0x530920),
     Fn<unsigned char (__cdecl*)()>(0x5302C0), Fn<unsigned char (__cdecl*)()>(0x530380),
     Fn<unsigned char (__cdecl*)()>(0x530800), Fn<unsigned char (__cdecl*)()>(0x5303E0),
     Fn<unsigned char (__cdecl*)()>(0x530430)},
    Fn<void (__cdecl*)()>(0x530480),
    Area_ZoneAt, Rand, Party_Count, Fn<unsigned char (__cdecl*)(unsigned, unsigned, unsigned)>(0x535310),
    Fn<unsigned char (__cdecl*)(unsigned)>(0x589330),
    Flags_Test, Sprite_SetAnimationBank, Field_MemberSprite, Field_PartyLoad, Sprite_ReleaseTint, Sprite_LoadPalette,
    Field_PartyPosition, Fn<void (__cdecl*)(long, long, unsigned)>(0x533690),
    Party_ClearActive, Field_LeaderFrame, Field_MemberFrame,
    reinterpret_cast<const std::uint32_t*>(at::kPendingJumps),
    Party_ClearAll, Sprite_SetTint, Field_MemberTimers,
    Party_JoinReset, Member_ClearState,
    PartySet_LoadFirst, PartySet_LoadSecond, File_LoadDone, Task_Sleep, PartySet_Find, PartySet_Select,
    Fn<void (__cdecl*)()>(0x536A60), LoadDatFile,
    MapView_CheckHeightScale, AreaMap_Elevation,
    Tint_Release,
};
Callees g = kOriginals;

}  // namespace field_event

namespace {

using namespace move_script;
using field_event::g;
namespace at = field_event::at;
using field_event::Handler;

constexpr unsigned kMemberBytes = 0x14C, kActorBytes = 0xA4;

std::uint32_t Address(const volatile void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
unsigned char* Member(unsigned i) { return ObjTrio + i * kMemberBytes; }
unsigned char* Actor(unsigned a) { return At(at::kActorRecords) + a * kActorBytes; }
void Dispatch(const std::uint32_t* table, unsigned index) {
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(table[index]))();
}
// `rep movsd` of 0x29 dwords: an actor record into a member's +0x80.
void CopyActor(unsigned char* to, const unsigned char* from) { std::memcpy(to, from, kActorBytes); }

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
    g.restore_clut();
    unsigned char* const timer = At(at::kLeaderTimer);
    if (*timer != 0) *timer = static_cast<unsigned char>(*timer - 1);
    g.copy_input();
    Dispatch(g.leader_states, Sprite_Current[1]);
}

// original 0x52D920, the leader's state 0 (PSX 0x801B1164): its counters
// cleared, grounded, and on to state 1 with its standing animation - or to
// state 0xC (Field_InputFlags bit 0x20) or 0xD (area 0xBD).
extern "C" void __cdecl Field_LeaderStart(void) {
    g.clear_steps();
    SetLong(Sprite_Current + 0x18, 0);
    SetLong(Sprite_Current + 0x1C, 0);
    SetLong(Sprite_Current + 0x20, 0);
    Sprite_Current[9] = 0;
    Sprite_Current[0xA] = 0;
    Sprite_Current[6] = 0;
    Sprite_Current[7] = 0;
    Sprite_Current[0xB] = 0;
    {
        const unsigned char* const c = Sprite_Current;
        const long z = Long(c + 0x38), x = Long(c + 0x34);
        const long ground = g.ground_at(x, z);
        SetWord(Sprite_Current + 0x3E, static_cast<std::uint16_t>(ground));
    }
    Field_State[0x128] = 3;
    Field_State[0x136] = 0;
    Field_State[0x137] = 0;
    Field_State[0x138] = 0;
    Sprite_Current[0x4B] = 0xFF;
    if (Field_InputFlags & 0x20) {
        Sprite_Current[0] = static_cast<unsigned char>(Sprite_Current[0] | 0x40);
        Sprite_Current[1] = 0xC;
        Sprite_Current[2] = 0;
        return;
    }
    if (Game_AreaNumber == 0xBD) {
        Sprite_Current[0] = static_cast<unsigned char>(Sprite_Current[0] | 0x40);
        Sprite_Current[1] = 0xD;
        Sprite_Current[2] = 0;
        return;
    }
    g.leader_animation(Sprite_Current[8]);
    Sprite_Current[1] = static_cast<unsigned char>(Sprite_Current[1] + 1);
    Sprite_Current[2] = 0;
}

// original 0x52DA50, the leader's state 1 (PSX 0x801B1318): its sub-state
// through the three-entry table 0x660954, then Sprite_ScriptTick.
//
// As the original has it: the sub-state Sprite_Current +2 is unchecked (3 and
// above reach the .data after the table); Sprite_ScriptTick is a tail jump,
// whose al the original returns and no caller reads.
extern "C" void __cdecl Field_LeaderControl(void) {
    Dispatch(g.control_states, Sprite_Current[2]);
    g.script_tick();
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
    if (g.test_535120(1)) return;
    if (g.test_535240(1)) return;
    if (g.test_5301f0()) {
        Field_State[0x136] = 0;
        return;
    }
    if (Field_InputHeld != 0) {
        unsigned char* const state = Field_State;
        if (!(Field_ActorStates[state[0x148] * kActorBytes] & 0x20)) state[0x136] = 0;
    }
    for (auto* const test : g.tests)
        if (test()) return;
    if (Field_InputHeld == 0) return;
    g.walk();
    Sprite_Current[7] = 0;
    Sprite_Current[0xA] = 2;
    Sprite_Current[0xB] = 0;
    g.leader_animation(Sprite_Current[8]);
    Sprite_Current[2] = 1;
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
    if (!(Field_InputFlags & 1)) return g.set_animation(animation);
    const unsigned char* const pair = At(at::kAnimationRemap) + (animation & 0xFF) * 2u;
    g.set_animation(pair[0]);
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
    const unsigned char* record;
    std::memcpy(&record, At(at::kZoneLists) + Game_AreaNumber * 4u, sizeof record);
    for (;; record += 8) {
        if (x < record[0] || z < record[1] || x > record[2] || z > record[3]) continue;
        return record;
    }
}

// original 0x52FEB0 (PSX 0x801B4CC4): the word at 0x802E74 set from the zone
// the leader stands in - see docs/field-event.md section 1 for what it may be.
//
// As the original has it: the zone is found from the low bytes of the words
// at 0x802D76 / 0x802D7A (the leader's x and z, integer part). The sum, the
// doubling and the halving are 8-bit and wrap. Rand is called only on the
// roll path; the zone record's byte +4 is read again after it.
extern "C" void __cdecl Field_ZoneCounterRoll(unsigned keep) {
    unsigned char* const counter = At(at::kZoneCounter);
    if (static_cast<unsigned char>(Field_ScriptFlags) & 0x20) {
        SetWord(counter, 0);
        return;
    }
    const unsigned char* const zone = g.zone_at(*At(at::kLeaderXHigh), *At(at::kLeaderZHigh));
    const unsigned char* const bases = At(at::kZoneBases);
    unsigned char value = bases[zone[4] * 2u];
    if (value == 0) {
        SetWord(counter, 0);
        return;
    }
    if ((keep & 0xFF) != 0) {
        const std::uint16_t now = Word(counter);
        if (now != 0) {
            if (now < value) return;
            SetWord(counter, value);
            return;
        }
    }
    unsigned char roll = static_cast<unsigned char>(g.rand() & 0x1F);
    const unsigned char most = bases[zone[4] * 2u + 1];
    if (roll > most) roll = most;
    value = static_cast<unsigned char>(value + roll);
    const unsigned members = static_cast<unsigned char>(g.party_count(0));
    unsigned i = 0;
    for (; i < members; ++i)
        if (g.has_item(Field_Members[i * kMemberBytes], 3, 0x15)) break;
    if (i < members) value = static_cast<unsigned char>(value << 1);
    for (i = 0; i < members; ++i)
        if (g.has_item(Field_Members[i * kMemberBytes], 3, 0x14)) break;
    if (i < members) value = static_cast<unsigned char>(value >> 1);
    SetWord(counter, value);
}

// --- small helpers -----------------------------------------------------------

// original 0x531BB0 (PSX 0x801BEB74): entries before the first 0xFF of the
// party list `slot` (0x904062 + slot * 3), at most 3. As the original has it:
// slot is unchecked.
extern "C" int __cdecl Party_Count(unsigned slot) {
    const unsigned char* const list = At(at::kPartyLists) + (slot & 0xFF) * 3u;
    int n = 0;
    while (list[n] != 0xFF)
        if (++n >= 3) break;
    return n;
}

// original 0x5322B0 (PSX 0x801BF708): a script context back to its start, at
// speed index 3.
extern "C" void __cdecl ScriptContext_Reset(unsigned char* context) {
    SetWord(context + 0xA, 0);
    context[7] = 0;
    context[2] = 0;
    context[5] = 0;
    context[0] = 0;
    context[1] = 0;
    context[3] = 0;
    context[4] = 3;
}

// original 0x536650: Sprite_Current's dwords +0xC, +0x10, +0x14 cleared. The
// original returns with eax 0, which none of the 14 call sites reads (checked
// 2026-09-22); ours does not promise it.
extern "C" void __cdecl Sprite_ClearSteps(void) {
    SetLong(Sprite_Current + 0xC, 0);
    SetLong(Sprite_Current + 0x10, 0);
    SetLong(Sprite_Current + 0x14, 0);
}

// original 0x5366A0: 32 words of palette `index` from Sprite_Current's bank to
// dst and to its twin in Gfx_ClutStrip, and the strip marked dirty.
//
// As the original has it: the twin is 0x80F580 + ((dst - 0x80B580) >> 1) * 2,
// an arithmetic shift - an odd dst loses its low bit there, a dst below the
// source strip lands below the strip. Each word is read, then stored to dst,
// then to the twin. Nothing is checked: the bank pointer, the index, dst.
extern "C" void __cdecl Sprite_LoadPalette(unsigned short* dst, unsigned index) {
    const unsigned char* bank;
    std::memcpy(&bank, Sprite_Current + 0x4C, sizeof bank);
    const unsigned char* const offsets = bank + Long(bank + 4);
    const std::uint32_t d = Address(dst);
    const auto half = static_cast<std::int32_t>(d - at::kClutSource) >> 1;
    unsigned char* twin = At(at::kClutMirror + static_cast<std::uint32_t>(half) * 2u);
    const unsigned char* from = offsets + Long(offsets + (index & 0xFF) * 4u);
    auto* to = reinterpret_cast<unsigned char*>(dst);
    for (int k = 0; k < 32; ++k) {
        const std::uint16_t w = Word(from);
        from += 2;
        SetWord(to, w);
        SetWord(twin, w);
        to += 2;
        twin += 2;
    }
    Gfx_ClutStripDirty = 1;
}

// original 0x536730 (PSX 0x80167664): member `member`'s bytes +1..+4 cleared.
// As the original has it: member is unchecked.
extern "C" void __cdecl Member_ClearState(unsigned member) {
    unsigned char* const m = Member(member & 0xFF);
    m[1] = 0;
    m[2] = 0;
    m[3] = 0;
    m[4] = 0;
}

// original 0x536760 (PSX 0x801676AC): all three members off, their state cleared.
extern "C" void __cdecl Party_ClearAll(void) {
    for (unsigned i = 0; i < 3; ++i) {
        Member(i)[0] = 0;
        g.clear_state(i);
    }
}

// original 0x5367A0 (PSX 0x80167704): the state of each member that is on cleared.
extern "C" void __cdecl Party_ClearActive(void) {
    for (unsigned i = 0; i < 3; ++i)
        if (Member(i)[0] != 0) g.clear_state(i);
}

// original 0x534010 (PSX 0x801C2838).
extern "C" void __cdecl Party_JoinReset(void) {
    SetLong(At(at::kJoinState), 0);
    SetLong(At(at::kJoinState + 4), 0);
    *At(at::kJoinState + 8) = 0;
}

// original 0x534EC0 (PSX 0x801C41FC): two of Field_State's timers from its
// actor record's bits 0x80 and 0x20. As the original has it: the record is
// indexed by Field_State +0x148 directly, a whole byte; Field_State is read
// again only after the first store.
extern "C" void __cdecl Field_MemberTimers(void) {
    unsigned char* state = Field_State;
    if (Field_ActorStates[state[0x148] * kActorBytes] & 0x80) {
        state[0x124] = 10;
        state = Field_State;
    }
    if (Field_ActorStates[state[0x148] * kActorBytes] & 0x20) state[0x125] = 0x28;
}

// original 0x572590 (PSX 0x8015598C): MapView_HeightScale = 1 for the object
// at 0x905DA0 with its +0xB bit 0, or for any other Sprite_Current when
// Field_State +0x138 bit 0 is set; otherwise left as it is.
extern "C" void __cdecl MapView_CheckHeightScale(void) {
    unsigned char* const c = Sprite_Current;
    if (c == At(at::kObject905DA0)) {
        if (c[0xB] & 1) MapView_HeightScale = 1;
        return;
    }
    if (Field_State[0x138] & 1) MapView_HeightScale = 1;
}

// original 0x572570 (PSX 0x801559E4): the ground at (x, z), with the height
// scale set for the call and cleared after it.
extern "C" long __cdecl MapView_GroundAt(long x, long z) {
    g.height_check();
    const long ground = g.elevation(x, z);
    MapView_HeightScale = 0;
    return ground;
}

// original 0x454DC0 (PSX 0x80197718): every tint record of `sprite` released,
// the 32 records of MoveScript_TintRecords in order, compared as whole dwords.
extern "C" void __cdecl Sprite_ReleaseTint(unsigned char* sprite) {
    for (unsigned i = 0; i < 32; ++i)
        if (static_cast<std::uint32_t>(Long(At(at::kTintSprites + i * 12))) == Address(sprite))
            g.tint_release(static_cast<unsigned char>(i));
}

// --- the party's placement ---------------------------------------------------

// original 0x533BA0 (PSX 0x801C224C): Sprite_Current set up as party member
// `member` in slot `slot` - its column in the loaded party set, its bank, its
// draw fields, and its palette loaded into the slot's 64 bytes.
//
// As the original has it: the column is 3 when the member is not in the set
// row (and +0x4C is then read from the bank table's fourth dword); the row is
// (0x90412C & 0x7F) * 3, unchecked against the table's 19 rows; +0x26 is the
// low byte of slot * 0x50; +0x70 keeps its upper 24 bits and gets
// (+0x2B != 0) as its low byte, both read back from the object.
extern "C" void __cdecl Field_MemberSprite(unsigned member_arg, unsigned slot_arg) {
    const unsigned member = member_arg & 0xFF, slot = slot_arg & 0xFF;
    const unsigned char* const row = At(at::kPartySets) + (*At(at::kPartySetCurrent) & 0x7Fu) * 3u;
    unsigned char column = 0;
    while (row[column] != member)
        if (++column >= 3) break;
    g.release_tint(Sprite_Current);
    SetWord(Sprite_Current + 0x2C, column);
    {
        unsigned char* const c = Sprite_Current;
        const unsigned index = Word(c + 0x2C);
        SetLong(c + 0x4C, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(At(at::kBankTable) + index * 4u)) + at::kBankTable));
    }
    Sprite_Current[0x24] = 1;
    Sprite_Current[0x27] = static_cast<unsigned char>(slot + 0x78);
    Sprite_Current[0x28] = 2;
    Sprite_Current[0x25] = 5;
    Sprite_Current[0x26] = static_cast<unsigned char>(slot * 0x50);
    Sprite_Current[0x2B] = At(at::kMemberFlags)[member];
    {
        unsigned char* const c = Sprite_Current;
        SetLong(c + 0x70, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(c + 0x70)) & 0xFFFFFF00u));
    }
    {
        unsigned char* const c = Sprite_Current;
        SetLong(c + 0x70, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(c + 0x70)) | (c[0x2B] != 0 ? 1u : 0u)));
    }
    Sprite_Current[0x29] = Draw_OtSlot;
    Sprite_Current[0x5D] = 0;
    Sprite_Current[0x5E] = 0;
    Sprite_Current[0x5F] = 0;
    Sprite_Current[0x5C] = 0;
    g.load_palette(reinterpret_cast<unsigned short*>(At(at::kPaletteBase + slot * 64u)), 0);
}

// original 0x533CE0 (PSX 0x801C23F8): Field_MemberCount members from the
// party list `slot` - each on, its sprite, scale 1.0, its actor record, and a
// tint (slot non-zero, actor bit 0x80) or its timers (slot 0).
//
// As the original has it: Party_ClearAll is skipped when Field_Request is 6
// (a member joining); Field_MemberCount is read again at every turn of the
// loop and bounds nothing else - more than 3 walks past ObjTrio; the tint's
// -4 and -8 are pushed as dwords, and Sprite_SetTint reads them as bytes.
extern "C" void __cdecl Field_PartyLoad(unsigned slot_arg) {
    const unsigned slot = slot_arg & 0xFF;
    if (Field_Request != 6) g.clear_all();
    if (Field_MemberCount == 0) return;
    const unsigned char* const list = At(at::kPartyLists) + slot * 3u;
    for (unsigned i = 0;;) {
        unsigned char* const m = Member(i);
        Field_State = m;
        Sprite_Current = m;
        m[0] = 1;
        Sprite_Current[5] = static_cast<unsigned char>(i);
        g.member_sprite(list[i], i);
        Sprite_Current[0x48] = 0;
        SetLong(Sprite_Current + 0x40, 0x10000);
        SetLong(Sprite_Current + 0x44, 0x10000);
        const unsigned char actor = MoveScript_EffectState[list[i]];
        m[0x148] = actor;
        CopyActor(m + 0x80, Actor(actor));
        m[0x138] = static_cast<unsigned char>(m[0x138] & 0xFE);
        if (slot != 0) {
            if (Field_ActorStates[m[0x148] * kActorBytes] & 0x80) g.set_tint(Sprite_Current, 0xFC, 0xF8, 0, 0);
        } else {
            g.member_timers();
        }
        ++i;
        if (i >= Field_MemberCount) break;
    }
}

// original 0x533580 (PSX 0x801C1750): each member at (x, z) facing
// `direction` - or, with its +0x70 byte non-zero, at the offset 0x660B40
// gives for direction / 2 (subtracted with Field_ScriptFlags2 bit 0x4000) -
// grounded; bit 0x40 set on members 1 and 2, cleared on the leader.
//
// As the original has it: only the low byte of direction is used, the offset
// index is unchecked (directions above 7 read past the four pairs), the
// arithmetic wraps, and the ground is read with the position just stored.
extern "C" void __cdecl Field_PartyPosition(long x, long z, unsigned direction) {
    if (Field_MemberCount == 0) return;
    const auto d = static_cast<unsigned char>(direction);
    for (unsigned i = 0;;) {
        unsigned char* const m = Member(i);
        if (m[0x70] == 0) {
            SetLong(m + 0x34, x);
            SetLong(m + 0x38, z);
        } else {
            const unsigned char* const offset = At(at::kMemberOffsets) + (d >> 1) * 8u;
            const auto ux = static_cast<std::uint32_t>(x), uz = static_cast<std::uint32_t>(z);
            if (Field_ScriptFlags2 & 0x4000) {
                SetLong(m + 0x34, static_cast<std::int32_t>(ux - static_cast<std::uint32_t>(Long(offset))));
                SetLong(m + 0x38, static_cast<std::int32_t>(uz - static_cast<std::uint32_t>(Long(offset + 4))));
            } else {
                SetLong(m + 0x34, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(offset)) + ux));
                SetLong(m + 0x38, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(offset + 4)) + uz));
            }
        }
        m[8] = d;
        const long ground = g.ground_at(Long(m + 0x34), Long(m + 0x38));
        SetWord(m + 0x3E, static_cast<std::uint16_t>(ground));
        if (i != 0)
            m[0] = static_cast<unsigned char>(m[0] | 0x40);
        else
            ObjTrio[0] = static_cast<unsigned char>(ObjTrio[0] & 0xBF);
        ++i;
        if (i >= Field_MemberCount) break;
    }
}

// original 0x533110 (PSX 0x801C0F94): the area set-up's party placement,
// called once per area by 0x594E60 with the entry point and its flags.
//
// As the original has it: `flags` is handed on whole to Field_PartyPosition /
// 0x533690, as the original pushes it; the area number is read once for its
// two tests at the start and again for 0xBD at the end of the leader-alone
// branch; the actor record's byte +9 (which member the leader is) is read
// again for each use; Field_MemberCount bounds the member-by-member loop and
// is read again at each turn; Party_Count's count is stored first as the
// member count, and the loop's first test is on that count, not re-read.
extern "C" void __cdecl Field_PartySetUp(long x, long z, unsigned flags) {
    Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags & 0xEFFF);
    bool whole_party = (Field_ScriptFlags2 & 0x8000) != 0;
    if (!whole_party) {
        const unsigned char input = Field_InputFlags;
        const unsigned short area = Game_AreaNumber;
        if (!(input & 9) && area != 0xBD && area != 0xBB) {
            if (!(input & 0x40))
                whole_party = true;
            else if (g.flags_test(At(at::kFlagBits), 0x77))
                whole_party = true;
        }
    }
    if (!whole_party) {
        // The leader alone.
        const unsigned count = static_cast<unsigned char>(g.party_count(0));
        const unsigned char* const list = At(at::kPartyLists);
        for (unsigned i = 0; i < count; ++i) {
            unsigned char* const word = Field_ActorStates + MoveScript_EffectState[list[i]] * kActorBytes;
            SetWord(word, Word(word) & 0xFFDF);
        }
        for (unsigned i = 0; i < count; ++i) {
            if (Field_ActorStates[MoveScript_EffectState[list[i]] * kActorBytes] & 0x80) {
                ObjTrio[0x124] = 10;
                break;
            }
        }
        const unsigned char input = Field_InputFlags;
        ObjTrio[5] = 0;
        Sprite_Current = ObjTrio;
        CopyActor(ObjTrio + 0x80, Actor(0));
        const unsigned char bits = ObjTrio[0x138];
        Field_MemberCount = 1;
        ObjTrio[0x148] = 0;
        ObjTrio[0x138] = static_cast<unsigned char>(bits & 0xFE);
        ObjTrio[0x48] = 0;
        if (input & 1) {
            const unsigned char who = Actor(0)[9];
            if (who == 0)
                g.set_bank(0x52);
            else if (who == 9)
                g.set_bank(0xCB);
            else if (who == 7)
                g.set_bank(0x27A);
            Sprite_Current[0x24] = 0;
            *(reinterpret_cast<unsigned char*>(&Field_ScriptFlags) + 1) |= 0x10;
        } else if (input & 8) {
            g.set_bank(Actor(0)[9] == 0 ? 0x47 : 0x1FD);
            Sprite_Current[0x2A] = 0;
            Sprite_Current[0x24] = 0;
        } else if (input & 0x40) {
            g.member_sprite(Actor(0)[9], 0);
        } else if (Game_AreaNumber == 0xBD) {
            ObjTrio[0x2B] = 0;
            SetLong(ObjTrio + 0x70, 0);
        }
    } else {
        // The whole party.
        const auto count = static_cast<unsigned char>(g.party_count(0));
        Field_MemberCount = count;
        if (!(Field_ScriptFlags2 & 0x8000)) {
            g.party_load(0);
        } else if (count != 0) {
            for (unsigned i = 0;;) {
                unsigned char* const m = Member(i);
                Sprite_Current = m;
                m[0] = static_cast<unsigned char>(m[0] | 1);
                m[0x138] = static_cast<unsigned char>(m[0x138] & 0xFE);
                SetLong(Sprite_Current + 0x34, 0);
                SetLong(Sprite_Current + 0x38, 0);
                SetLong(Sprite_Current + 0x3C, 0);
                const unsigned char id = At(at::kPartyLists)[i];
                const unsigned char actor = MoveScript_EffectState[id];
                m[0x148] = actor;
                CopyActor(m + 0x80, Actor(actor));
                if (!(Sprite_Current[0x24] & 1)) {
                    g.member_sprite(id, i);
                } else {
                    g.member_sprite(id, i);
                    g.release_tint(Sprite_Current);
                    Sprite_Current[0x27] = static_cast<unsigned char>(Sprite_Current[5] + 0x78);
                    g.load_palette(reinterpret_cast<unsigned short*>(At(at::kPaletteBase + Sprite_Current[5] * 64u)), 0);
                }
                ++i;
                if (i >= Field_MemberCount) break;
            }
        }
    }
    // Where the kind-2 object and the camera start.
    if (Field_InputFlags & 8) {
        SetLong(ObjTrio + 0x38, 0x410000);
        SetLong(ObjTrio + 0x34, 0xF0000);
        Field_Kind2X = 0xF0000;
        Field_Kind2Z = 0x3C0000;
        return;
    }
    if (flags & 0x80) {
        Field_Kind2X = x;
        Field_Kind2Z = z;
        return;
    }
    if (Field_ScriptFlags & 0x800)
        g.position_alt(x, z, flags);
    else
        g.position(x, z, flags);
    const long leader_x = Long(ObjTrio + 0x34), leader_z = Long(ObjTrio + 0x38);
    Field_Kind2X = leader_x;
    Field_Kind2Z = leader_z;
}

// original 0x5334A0 (PSX 0x801C15A4): the placed party's first frame - each
// member's frame once (the leader's through Field_LeaderFrame), unless
// Field_ScriptFlags2 bit 0x8000 holds them, which clears their bit 0x40.
//
// As the original has it: the +0x29 loop runs to the member count read before
// it; the frame loop's bound is read again at each turn, and its mask is
// 1 << i with x86's shift (the count mod 32).
extern "C" void __cdecl Field_PartyFirstFrame(void) {
    if (Field_ScriptFlags2 & 0x8000) {
        const unsigned count = Field_MemberCount;
        for (unsigned i = 0; i < count; ++i) Member(i)[0] = static_cast<unsigned char>(Member(i)[0] & 0xBF);
        Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 & 0xBFFF);
        return;
    }
    g.clear_active();
    const unsigned char input = Field_InputFlags;
    const unsigned char count = Field_MemberCount;
    if (input & 8) {
        ObjTrio[1] = 9;
    } else {
        for (unsigned i = 0; i < count; ++i) Member(i)[0x29] = 6;
    }
    if (count != 0) {
        for (unsigned i = 0;;) {
            if (!(Field_ScriptFlags & (1u << (i & 31)))) {
                unsigned char* const m = Member(i);
                const unsigned char slot = m[5];
                Field_State = m;
                Sprite_Current = m;
                if (slot == 0)
                    g.leader_frame();
                else
                    g.member_frame();
            }
            ++i;
            if (i >= Field_MemberCount) break;
        }
    }
    Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 & 0xBFFF);
}

// original 0x533760 (PSX 0x801C1A60): a pending jump - while the byte
// 0x904EF0 is set, through the four-entry table 0x660B60 by the byte after
// it. As the original has it: the index is unchecked and the call is a tail
// jump whose eax no caller keeps (Field_MembersFrame reads al only after
// loading it).
extern "C" void __cdecl Field_PendingJump(void) {
    if (*At(at::kPending) == 0) return;
    Dispatch(g.pending_jumps, *At(at::kPending + 1));
}

// original 0x533EF0 (PSX 0x801C2874): member `member` joins the party as
// member Field_MemberCount - 0, or 1 (and Field_Request 6) when there are
// three already.
//
// As the original has it: the actor record's +0xB bit 0 is set before the
// test, so even a refused join marks it; the member's slot and the count are
// read again after each call; the actor index is read again for the copy and
// the +0x148 store; `member` is not checked against MoveScript_EffectState's
// 24 entries.
extern "C" unsigned char __cdecl Party_Join(unsigned member_arg) {
    const auto member = static_cast<unsigned char>(member_arg);
    g.join_reset();
    const unsigned char* const actor_of = MoveScript_EffectState + member;
    unsigned char* const joined = Actor(*actor_of) + 0xB;
    *joined = static_cast<unsigned char>(*joined | 1);
    const unsigned char n = Field_MemberCount;
    if (n >= 3) {
        Field_Request = 6;
        return 1;
    }
    At(at::kPartyLists)[n] = member;
    At(at::kPartyLists)[n + 3] = member;
    Member(n)[0] = 1;
    g.clear_state(n);
    unsigned char* const slot = Member(Field_MemberCount);
    Sprite_Current = slot;
    slot[0x24] = 1;
    g.member_sprite(member, Field_MemberCount);
    const unsigned char actor = *actor_of;
    const unsigned char count = Field_MemberCount;
    CopyActor(Member(count) + 0x80, Actor(actor));
    Sprite_Current[5] = count;
    Sprite_Current[0x4B] = 0xFF;
    const unsigned char k = Field_MemberCount;
    Field_MemberCount = static_cast<unsigned char>(k + 1);
    Field_Members[k * kMemberBytes] = *actor_of;
    return 0;
}

// --- the party set -------------------------------------------------------------

// original 0x5368F0: the party-set row for the members a, b, c (0xFF for
// none). The current set 0x90412C if it is not 0xFF and holds every one of
// them; else the three sorted and looked up column by column in the 19
// sorted rows at 0x669750, narrowing the rows at each match; none found calls
// 0x536A60 (which prints the three and never returns) and gives 0xFF.
//
// As the original has it: the three ids and the sort's swap byte are left at
// 0x903850..0x903854; the match is tested against count - 1 where count is
// the ids that are not 0xFF, so all three 0xFF can never match; the matched
// count indexes the ids unchecked; the row is returned as a byte, and a row
// of 0xFF would go on to the next column (unreachable: rows stop at 0x13).
extern "C" unsigned char __cdecl PartySet_Find(unsigned a, unsigned b, unsigned c) {
    unsigned char* const ids = At(at::kFindIds);
    const unsigned char current = *At(at::kPartySetCurrent);
    ids[0] = static_cast<unsigned char>(a);
    ids[1] = static_cast<unsigned char>(b);
    ids[2] = static_cast<unsigned char>(c);
    if (current != 0xFF) {
        unsigned i = 0;
        for (; i < 3; ++i) {
            const unsigned char id = ids[i];
            if (id == 0xFF) continue;
            const unsigned char* const row = At(at::kPartySets) + (current & 0x7Fu) * 3u;
            unsigned j = 0;
            while (id != row[j])
                if (++j >= 3) break;
            if (j >= 3) break;
        }
        if (i >= 3) return static_cast<unsigned char>(current & 0x7F);
    }
    unsigned char count = 0;
    for (unsigned k = 0; k < 3; ++k)
        if (ids[k] != 0xFF) ++count;
    unsigned char* const swap = ids + 4;
    for (unsigned p = 0; p < 2; ++p)
        for (unsigned q = p + 1; q < 3; ++q) {
            const unsigned char low = ids[p];
            if (low > ids[q]) {
                *swap = low;
                ids[p] = ids[q];
                ids[q] = *swap;
            }
        }
    unsigned char limit = 0x13, start = 0, result = 0xFF;
    unsigned matched = 0;
    const unsigned char* const table = At(at::kPartySets);
    for (unsigned column = 0; column < 3; ++column) {
        unsigned row = start;
        if (row >= limit) continue;
        for (;;) {
            const unsigned char* cell = table + row * 3u + column;
            const unsigned char want = ids[matched];
            if (want == *cell) {
                if (matched == count - 1u) {
                    result = static_cast<unsigned char>(row);
                    if (result != 0xFF) return result;
                    break;
                }
                start = static_cast<unsigned char>(row);
                while (row < 0x13 && want == *cell) {
                    ++row;
                    cell += 3;
                }
                limit = static_cast<unsigned char>(row);
                ++matched;
            }
            ++row;
            if (row >= limit) break;
        }
    }
    g.set_error();
    return result;
}

// original 0x536AC0 (PSX 0x80167C04): party set `set` recorded in 0x90412C
// (with bit 7 for mode 0, untouched for mode 3) and one of its four files
// loaded - LoadDatFile(set + 0x2C2 / 0xFC / 0x10F / 0x2D5) for mode 0..3;
// above 3, nothing is loaded.
extern "C" void __cdecl PartySet_Select(unsigned set_arg, unsigned mode_arg) {
    const unsigned set = set_arg & 0xFF, mode = mode_arg & 0xFF;
    if (mode != 3) *At(at::kPartySetCurrent) = static_cast<unsigned char>(mode == 0 ? set | 0x80 : set);
    switch (mode) {
    case 0: g.load_dat(set + 0x2C2); break;
    case 1: g.load_dat(set + 0xFC); break;
    case 2: g.load_dat(set + 0x10F); break;
    case 3: g.load_dat(set + 0x2D5); break;
    default: break;
    }
}

// original 0x536850: the set for a, b, c selected with mode 3 (its file
// without recording it) unless it is the current one.
extern "C" void __cdecl PartySet_LoadFirst(unsigned a, unsigned b, unsigned c) {
    const unsigned char set = g.find(a, b, c);
    if ((*At(at::kPartySetCurrent) & 0x7F) != set) g.select(set, 3);
}

// original 0x536890: the set for a, b, c selected with `mode` unless
// 0x90412C already holds it as that mode would record it.
extern "C" void __cdecl PartySet_LoadSecond(unsigned a, unsigned b, unsigned c, unsigned mode) {
    const unsigned char set = g.find(a, b, c);
    const unsigned want = (mode & 0xFF) != 0 ? set : (set | 0x80u);
    if (*At(at::kPartySetCurrent) != want) g.select(set, mode);
}

// original 0x5367E0: the party set for a, b, c loaded - first with mode 3
// when `mode` is 0 - each load waited out a frame at a time.
//
// As the original has it: File_LoadDone is asked before the first sleep, so
// a load already done costs no frame.
extern "C" void __cdecl PartySet_Load(unsigned a, unsigned b, unsigned c, unsigned mode) {
    if ((mode & 0xFF) == 0) {
        g.load_first(a, b, c);
        if (g.load_done() == 0) {
            do g.sleep(1);
            while (g.load_done() == 0);
        }
    }
    g.load_second(a, b, c, mode);
    if (g.load_done() == 0) {
        do g.sleep(1);
        while (g.load_done() == 0);
    }
}

void FieldEvent_Inject() {
    if (bof3::WantsShadow("field_event")) field_event::SelfTest();
    BOF3_INJECT(Field_LeaderFrame);
    BOF3_INJECT(Field_LeaderStart);
    BOF3_INJECT(Field_LeaderControl);
    BOF3_INJECT(Field_LeaderStand);
    BOF3_INJECT(Field_ZoneCounterRoll);
    BOF3_INJECT(Area_ZoneAt);
    BOF3_INJECT(Field_LeaderAnimation);
    BOF3_INJECT(Party_Count);
    BOF3_INJECT(ScriptContext_Reset);
    BOF3_INJECT(Field_PartySetUp);
    BOF3_INJECT(Field_PartyFirstFrame);
    BOF3_INJECT(Field_PartyPosition);
    BOF3_INJECT(Field_PendingJump);
    BOF3_INJECT(Field_MemberSprite);
    BOF3_INJECT(Field_PartyLoad);
    BOF3_INJECT(Party_Join);
    BOF3_INJECT(Party_JoinReset);
    BOF3_INJECT(Field_MemberTimers);
    BOF3_INJECT(Sprite_ClearSteps);
    BOF3_INJECT(Sprite_LoadPalette);
    BOF3_INJECT(Member_ClearState);
    BOF3_INJECT(Party_ClearAll);
    BOF3_INJECT(Party_ClearActive);
    BOF3_INJECT(PartySet_Load);
    BOF3_INJECT(PartySet_LoadFirst);
    BOF3_INJECT(PartySet_LoadSecond);
    BOF3_INJECT(PartySet_Find);
    BOF3_INJECT(PartySet_Select);
    BOF3_INJECT(MapView_GroundAt);
    BOF3_INJECT(MapView_CheckHeightScale);
    BOF3_INJECT(Sprite_ReleaseTint);
}

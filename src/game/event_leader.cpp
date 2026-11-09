// The event script's leader, steps and placements (group DC of the eighth
// round) - docs/event_leader.md for each function, its evidence, the tables
// and the fuzz.
//
//   - The leader's sub-state 2 (Field_LeaderStepping, table 0x660954 entry 2)
//     and where a step lands (Field_LeaderStepLands), round six's leftovers.
//   - Five of the leader's states (the table 0x660918): 3 the party swap, 4
//     the menu, 5 the encounter, 7 a passage's content, 10 the action by
//     party set - the three dispatchers and their sub-states.
//   - The spot a member can stand on (Field_SpotFree) and the way-blocked
//     tests under it (Field_WayBlocked, Field_WayBlocked4, Field_CellsBlock).
//   - The encounter's area, the event cells and the cell's exit (0x5317F0,
//     0x531920, 0x531AF0), and the pending jump's walk-in (0x533780).
//
// Terms as event_ops.cpp's: "SC" is Sprite_Current and "FS" Field_State, read
// afresh at every use as the original reads [0x937F88] / [0x905D98] (a callee
// may move either). Every call goes through event_leader::g
// (event_leader_callees.h), so that the start-up fuzz can stand recorders in.
#include "game/event_leader.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/event_leader_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace event_leader {

using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

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

// DIV-0121: 1 through PatchBytes("EdgeNoArea") in EventLeader_Inject, after
// the fuzz, which runs Capcom's rule.
std::uint32_t g_edge_no_area = 0;

}  // namespace event_leader

using namespace event_leader;

// The record views (game/record_views.h): rv::Current<T>() reads
// Sprite_Current afresh at each use, as the original does.
// rv::As<records::SpriteObject>(p) views a
// sprite-family pointer; it is used below +0x80 only (the sprite head every
// family shares) unless the pointer is a Sprite_Objects / Sprite_ObjectsExtra /
// Sprite_Kind2 record.

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

// --- the leader's step ------------------------------------------------------------

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

// --- state 3: the party swap -----------------------------------------------------

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

// --- state 4: the menu --------------------------------------------------------

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

// --- state 5: the encounter ----------------------------------------------------

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

// --- state 7: a passage's content ------------------------------------------------

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

// --- state 10: the action by party set ------------------------------------------

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

// --- the cells and the exits -----------------------------------------------------

// --- the way blocked -------------------------------------------------------------

void EventLeader_Inject() {
    if (bof3::WantsShadow("event_leader")) event_leader::SelfTest();
    {
        // DIV-0121: armed after the fuzz above, which sees Capcom's rule.
        const std::uint8_t was[] = {0, 0, 0, 0}, is[] = {1, 0, 0, 0};
        bof3::PatchBytes("EdgeNoArea", Low32(&event_leader::g_edge_no_area),
                         was, is, 4);
        bof3::Log("DIV-0121    a map edge that leads to no area (a game loaded in an encounter field) holds (on unless the line above says OFF)");
    }
    BOF3_INJECT(Field_LeaderStepping);
    BOF3_INJECT(Field_LeaderStepLands);
    BOF3_INJECT(Field_SwapState);
    BOF3_INJECT(Field_SwapGather);
    BOF3_INJECT(Field_SpotFree);
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
    BOF3_INJECT(Field_PassageState);
    BOF3_INJECT(Field_PassageOpen);
    BOF3_INJECT(Field_PassageTake);
    BOF3_INJECT(Field_PassageEnd);
    BOF3_INJECT(Field_ActionState);
}

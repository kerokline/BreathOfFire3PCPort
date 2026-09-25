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
    Fn<unsigned char (__cdecl*)(long, long, unsigned)>(fn::kPathClear),
    Fn<void (__cdecl*)(int)>(fn::kGiveZenny),
    Fn<const unsigned char* (__cdecl*)()>(fn::kAreaExits),
    Fn<unsigned char (__cdecl*)(long, long, long)>(fn::kWayBlockedWide),
};
Callees g = kOriginals;

}  // namespace event_leader

using namespace event_leader;

namespace {

unsigned char& Byte(std::uint32_t a) { return *At(a); }

// A .data dispatch table, read at the call as the original reads it: the
// index is a whole byte and unchecked, so an index past the end calls what
// the next table holds (docs/event_leader.md section 3).
const Handler* Table(std::uint32_t address) { return reinterpret_cast<const Handler*>(static_cast<std::uintptr_t>(address)); }

unsigned char* Member(unsigned i) { return ObjTrio + i * at::kMemberStride; }

// Field_DirectionSteps' pair for a direction - a whole byte into a table of
// eight, as the original indexes it.
std::uint32_t StepX(unsigned d) { return static_cast<std::uint32_t>(Long(reinterpret_cast<const unsigned char*>(Field_DirectionSteps) + d * 8u)); }
std::uint32_t StepZ(unsigned d) { return static_cast<std::uint32_t>(Long(reinterpret_cast<const unsigned char*>(Field_DirectionSteps) + d * 8u + 4)); }

// `cdq / xor / sub`: the absolute value with the original's wrap (the most
// negative dword stays negative).
std::int32_t Abs(std::uint32_t v) {
    const std::uint32_t s = static_cast<std::uint32_t>(static_cast<std::int32_t>(v) >> 31);
    return static_cast<std::int32_t>((v ^ s) - s);
}

// A formation spot: the s8 pair at 0x6608F6 + 2 * (slot + 2 * direction),
// each << 15, plus the leader's position. The direction is a whole byte.
std::uint32_t FormationX(int slot, unsigned d) {
    const auto dx = static_cast<signed char>(At(at::kFormation)[(slot + static_cast<int>(d) * 2) * 2]);
    return (static_cast<std::uint32_t>(static_cast<std::int32_t>(dx)) << 15) + static_cast<std::uint32_t>(Long(At(at::kLeaderX)));
}
std::uint32_t FormationZ(int slot, unsigned d) {
    const auto dz = static_cast<signed char>(At(at::kFormation)[(slot + static_cast<int>(d) * 2) * 2 + 1]);
    return (static_cast<std::uint32_t>(static_cast<std::int32_t>(dz)) << 15) + static_cast<std::uint32_t>(Long(At(at::kLeaderZ)));
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

// original 0x52E110: the leader's sub-state 2 (Field_LeaderControl's table
// 0x660954 entry 2) - Field_State +0x137 = 1; with steps left in +9, one fewer
// and Field_LeaderStepTick; at 0, where the step lands. Both are tail jumps.
extern "C" void __cdecl Field_LeaderStepping(void) {
    Field_State[0x137] = 1;
    unsigned char* const sc = Sprite_Current;
    const unsigned char left = sc[9];
    if (left == 0) {
        g.step_lands();
        return;
    }
    sc[9] = static_cast<unsigned char>(left - 1);
    g.step_tick();
}

// original 0x52E580: where a step lands - reached only by 0x52E110's tail
// jump, in no list (event-ops.md section 11). The run counter +7 (up to 0x11
// while running, else 0); unless on foot the equipment tick; the two actor
// ticks and the floor; the four turn tiles, any one ending it (0xA4 with
// MoveCmd_TestFB); then by Field_InputFlags: on foot, cell 0xAF is the
// stored exit (Field_ChangeArea from 0x937F82.., 0x904EE0 = 0, 0x937F98 =
// 0xC), Field_ScriptFlags2 bit 13 stops (Field_ScriptFlags bit 8), cell 0xC0
// asks Area_LinkAt; with bit 1, the map's edge (0x20000, or the header's size
// less 3, on either axis, +0x70 steps on) leaves by 0x802290... Then a cell
// all 0xA7 is a link (Field_Request 5), none 0xA6 the return point 0x904148;
// Field_ScriptFlags2 bit 5 a jump; else Field_EdgeBits counted (twice on a
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
    if (!(Field_InputFlags & 1)) g.equip_tick();
    g.bit20_tick();
    g.floor_damage();
    if (g.tile89(0)) return;
    if (g.tile8a(0)) return;
    if (g.tile_d0()) return;
    if (g.tile_a4()) {
        unsigned char* const sc = Sprite_Current;
        g.test_fb(static_cast<short>(Word(sc + 0x36)), static_cast<short>(Word(sc + 0x3A)));
        return;
    }
    const unsigned char input = Field_InputFlags;
    unsigned char* sc;
    bool stop = false;
    if (input & 1) {
        unsigned char code;
        {
            unsigned char* const s = Sprite_Current;
            code = g.byte_at(static_cast<short>(Word(s + 0x36)), static_cast<short>(Word(s + 0x3A)));
        }
        if (code == 0xAF) {
            g.leader_animation(Sprite_Current[8]);
            g.change_area(Word(At(at::kExitArea)), Long(At(at::kExitX)), Long(At(at::kExitZ)), Byte(at::kExitKind));
            Byte(at::kLandByte) = 0;
            Byte(at::kLandMode) = 0xC;
            return;
        }
        if (Byte(at::kFlags2Hi) & 0x20) {
            Byte(at::kFlagsHi) |= 1;
            stop = true;
        } else {
            {
                unsigned char* const s = Sprite_Current;
                code = g.byte_at(static_cast<short>(Word(s + 0x36)), static_cast<short>(Word(s + 0x3A)));
            }
            if (code == 0xC0) {
                unsigned char* const s = Sprite_Current;
                g.link_at(Word(s + 0x36), Word(s + 0x3A));
            }
            sc = Sprite_Current;
        }
    } else {
        sc = Sprite_Current;
        if (input & 2) {
            const unsigned char d = sc[8];
            const std::uint32_t n = sc[0x70];
            const std::uint32_t x = StepX(d) * n + static_cast<std::uint32_t>(Long(sc + 0x34));
            const std::uint32_t z = StepZ(d) * n + static_cast<std::uint32_t>(Long(sc + 0x38));
            const std::uint32_t edge_x = (static_cast<std::uint32_t>(AreaMap_Header[0]) - 3u) << 16;
            const std::uint32_t edge_z = (static_cast<std::uint32_t>(AreaMap_Header[1]) - 3u) << 16;
            if (x == 0x20000 || z == 0x20000 || x == edge_x || z == edge_z) {
                g.ensure_animation(d);
                g.change_area(Word(At(at::kEdgeArea)), Long(At(at::kEdgeX)), Long(At(at::kEdgeZ)), 4);
                Sprite_Current[2] = 1;
                return;
            }
        }
    }
    if (!stop) {
        if (g.cells_all(Long(sc + 0x34), Long(sc + 0x38), sc[0x70], 0xA7, 0)) {
            {
                unsigned char* const s = Sprite_Current;
                g.link_at(Word(s + 0x36), Word(s + 0x3A));
            }
            Field_Request = 5;
            g.ensure_animation(Sprite_Current[8]);
            Sprite_Current[2] = 1;
            return;
        }
        {
            unsigned char* const s = Sprite_Current;
            if (!g.cells_none(Long(s + 0x34), Long(s + 0x38), s[0x70], 0xA6, 0)) {
                g.ensure_animation(Sprite_Current[8]);
                const std::int32_t z = Long(At(at::kReturnPoint + 4));
                const std::int32_t x = Long(At(at::kReturnPoint));
                const unsigned short area = Word(At(at::kReturnPoint + 8));
                Byte(at::kReturnPoint + 0xA) = 0;
                g.change_area(area, x, z, 4);
                return;
            }
        }
        if (Byte(at::kFlags2Lo) & 0x20) {
            Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 ^ 0x20);
            g.jump_start();
            Sprite_Current[9] = static_cast<unsigned char>(Sprite_Current[9] - 1);
            g.step_tick();
            Sprite_Current[2] = 2;
            return;
        }
        {
            unsigned char* const s = Sprite_Current;
            Field_EdgeBits = static_cast<unsigned short>(Field_EdgeBits + 1);
            if (s[8] == 2 || s[8] == 6) Field_EdgeBits = static_cast<unsigned short>(Field_EdgeBits + 1);
        }
        g.clear_steps();
        const unsigned char input2 = Field_InputFlags;
        MoveScript_F3Divisor = 0;
        MoveScript_FAWord = 0;
        if (!(input2 & 1) && !(Byte(at::kFlags2Lo) & 0x60) && g.encounter_due()) {
            Field_State[0x128] = 3;
            g.ensure_animation(Sprite_Current[8]);
            unsigned char* const s = Sprite_Current;
            Byte(at::kFlags2Hi) |= 0x10;
            s[1] = 5;
            Sprite_Current[2] = 0;
            return;
        }
        Field_State[0x128] = 3;
        if (g.effect_test(pace)) return;
        {
            unsigned char* const s = Sprite_Current;
            stop = g.arrive_hook(Long(s + 0x34), Long(s + 0x38)) != 0;
        }
        if (!stop) {
            g.bit80_tick();
            if (!(Byte(at::kFlagsHi) & 1) && !(Byte(at::kFlags2Lo) & 0x40) &&
                (Field_InputHeld & (Word(At(at::kButtonConfirm)) | 0xF000u)) != 0) {
                g.leader_walk();
                return;
            }
            g.leader_animation(Sprite_Current[8]);
            Field_State[0x137] = 0;
            Sprite_Current[2] = 1;
            return;
        }
    }
    g.leader_animation(Sprite_Current[8]);
    Sprite_Current[2] = 1;
}

// --- state 3: the party swap -----------------------------------------------------

// original 0x52E9D0: the leader's state 3 (Field_LeaderSwapTest sets it) - a
// tail jump through the five sub-states at 0x660968 by Sprite_Current +2.
extern "C" void __cdecl Field_SwapState(void) { Table(at::kSwapSteps)[Sprite_Current[2]](); }

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
    const unsigned char dir = ObjTrio[8];
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
            if ((ObjTrio[0x138] & 1) || slot == 0) {
                placed = g.spot_free(Long(At(at::kLeaderX)), Long(At(at::kLeaderZ)), dir, m[0x70]) != 0;
            } else {
                placed = false;
                const std::uint32_t x = FormationX(slot, dir), z = FormationZ(slot, dir);
                if (g.spot_free(static_cast<long>(x), static_cast<long>(z), dir, m[0x70])) {
                    unsigned char* const sc = Sprite_Current;
                    placed = g.path_clear(Long(At(at::kTargetX)), Long(At(at::kTargetZ)), sc[0x70]) != 0;
                }
                if (!placed) {
                    unsigned char* const sc = Sprite_Current;
                    placed = g.spot_free(Long(At(at::kLeaderX)), Long(At(at::kLeaderZ)), dir, sc[0x70]) != 0;
                }
            }
            if (placed && i != 0) {
                g.clear_state(i);
                Sprite_Current[1] = 3;
                g.shade_begin();
                Sprite_Current[0x5C] = 1;
            }
            count = Field_MemberCount;
            ++i;
        } while (static_cast<int>(i) < static_cast<int>(count));
    }
    Sprite_Current = ObjTrio;
    for (unsigned j = 1; static_cast<int>(j) < static_cast<int>(count); ++j) {
        if (Member(j)[1] == 3) {
            ObjTrio[2] = static_cast<unsigned char>(ObjTrio[2] + 1);
            return;
        }
    }
    ObjTrio[2] = 4;
}

// original 0x52EBA0: whether a member can stand at (x, z) - Sprite_Current +9
// held at 0 over Field_WayBlocked(x, z, raised, the ground there), and the
// ground within 0x40 of the leader's height; then the spot and its ground
// (called again) to 0x903858 / 0x90385C / 0x903852, +9 put back, 1. The
// third argument is not read.
// As the original has it: +9 is put back only on success, and into
// Sprite_Current read again after the second ground call.
extern "C" unsigned char __cdecl Field_SpotFree(long x, long z, unsigned, unsigned raised) {
    const long ground = g.ground_at(x, z);
    unsigned char* const sc = Sprite_Current;
    const unsigned char steps = sc[9];
    sc[9] = 0;
    if (g.way_blocked(x, z, raised, ground)) return 0;
    const int rise = static_cast<short>(Word(At(at::kLeaderHeight))) - static_cast<short>(ground);
    if (std::abs(rise) > 0x40) return 0;
    SetLong(At(at::kTargetX), static_cast<std::int32_t>(x));
    SetLong(At(at::kTargetZ), static_cast<std::int32_t>(z));
    const long at_spot = g.ground_at(x, z);
    unsigned char* const sc2 = Sprite_Current;
    SetWord(At(at::kScratch2), static_cast<unsigned>(at_spot));
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
    g.set_animation(Sprite_Current[8]);
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
    Sprite_Current = ObjTrio;
    if (ObjTrio[9] == 4) ObjTrio[2] = static_cast<unsigned char>(ObjTrio[2] + 1);
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
        g.swap_fields(0, 1, 1);
        if (Field_MemberCount == 3 && Member(2)[1] == 3) g.swap_fields(1, 2, 1);
    } else if (Field_MemberCount == 3 && Member(2)[1] == 3) {
        g.swap_fields(0, 2, 1);
    }
    for (unsigned i = 0; static_cast<int>(i) < static_cast<int>(Field_MemberCount); ++i) {
        unsigned char* const m = Member(i);
        if (m[1] != 3) continue;
        Sprite_Current = m;
        Field_State = m;
        g.member_sprite(m[0x89], i);
        Field_State[0x138] = saved;
        if (Field_State[0x138] & 1) Sprite_Current[0x29] = 4;
        {
            unsigned char* const fs = Field_State;
            std::memcpy(fs + 0x80, At(at::kActorRecords + MoveScript_EffectState[fs[0x89]] * at::kActorStride), at::kActorStride);
        }
        {
            unsigned char* const fs = Field_State;
            fs[0x148] = MoveScript_EffectState[fs[0x89]];
        }
        At(at::kPartyList)[i] = Field_State[0x89];
        if (i != 0) {
            g.shade_begin();
            Sprite_Current[0] = static_cast<unsigned char>(Sprite_Current[0] & 0xBF);
            Sprite_Current[0x5C] = 1;
            Sprite_Current[8] = static_cast<unsigned char>(ObjTrio[8] ^ 4);
        }
        if (!(ObjTrio[0x138] & 1) && i != 0) {
            unsigned char* const sc = Sprite_Current;
            const unsigned char d = sc[8];
            const unsigned char raised = sc[0x70];
            const std::uint32_t x = FormationX(static_cast<int>(i), d), z = FormationZ(static_cast<int>(i), d);
            bool found = false;
            if (g.spot_free(static_cast<long>(x), static_cast<long>(z), d, raised)) {
                unsigned char* const s = Sprite_Current;
                found = g.path_clear(Long(At(at::kTargetX)), Long(At(at::kTargetZ)), s[0x70]) != 0;
            }
            if (!found) {
                unsigned char* const s = Sprite_Current;
                g.spot_free(Long(At(at::kLeaderX)), Long(At(at::kLeaderZ)), s[8], s[0x70]);
            }
        } else {
            unsigned char* const s = Sprite_Current;
            g.spot_free(Long(At(at::kLeaderX)), Long(At(at::kLeaderZ)), s[8], s[0x70]);
        }
        SetLong(Sprite_Current + 0x34, Long(At(at::kTargetX)));
        SetLong(Sprite_Current + 0x38, Long(At(at::kTargetZ)));
        long ground;
        {
            unsigned char* const s = Sprite_Current;
            ground = g.ground_at(Long(s + 0x34), Long(s + 0x38));
        }
        SetWord(Sprite_Current + 0x3E, static_cast<unsigned>(ground));
        g.set_animation(Sprite_Current[8]);
    }
    Sprite_Current = ObjTrio;
    Field_State = ObjTrio;
    const auto lx = static_cast<std::uint32_t>(Long(At(at::kLeaderX)));
    const auto lz = static_cast<std::uint32_t>(Long(At(at::kLeaderZ)));
    std::int32_t far = Abs(static_cast<std::uint32_t>(Field_Kind2X) - lx);
    Field_Kind2X = static_cast<long>(lx);
    const std::int32_t far_z = Abs(static_cast<std::uint32_t>(Field_Kind2Z) - lz);
    Field_Kind2Z = static_cast<long>(lz);
    MoveScript_F3Divisor = 0x20;
    if (far < far_z) far = far_z;
    const auto frames = static_cast<unsigned char>(far >> 13);
    if (frames != 0) {
        const auto rise = static_cast<short>(Word(At(at::kLeaderHeight)) - static_cast<unsigned short>(MapView_Elevation));
        MoveScript_FAWord = static_cast<unsigned short>(static_cast<int>(rise) / static_cast<int>(frames));
    } else {
        MoveScript_FAWord = 0;
    }
    ObjTrio[9] = 4;
    ObjTrio[2] = static_cast<unsigned char>(ObjTrio[2] + 1);
}

// original 0x52F1B0: sub-state 3 - the leader's +9 counts down and it turns
// on; each member after it in state 3 gets the shade (+9 * 0xE0); at +9 = 0
// each of them has its palette back (Sprite_LoadPalette from 0x80D380 + +5 *
// 64), +0 bit 5 and the shade, +0x5C and +9 cleared, the steps and +0x18 /
// +0x1C / +0x20 zeroed, and the leader goes to sub-state 4.
// As the original has it: the member count is read again only after a member
// in state 3, and Field_State is set to the leader before the +9 test.
extern "C" void __cdecl Field_SwapSpinIn(void) {
    Sprite_Current[9] = static_cast<unsigned char>(Sprite_Current[9] - 1);
    {
        unsigned char* const sc = Sprite_Current;
        sc[8] = static_cast<unsigned char>((sc[8] + 1) & 7);
    }
    g.set_animation(Sprite_Current[8]);
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
    Field_State = ObjTrio;
    Sprite_Current = ObjTrio;
    if (ObjTrio[9] != 0) return;
    if (count > 1) {
        unsigned char i = 1;
        do {
            unsigned char* const m = Member(i);
            Sprite_Current = m;
            if (m[1] == 3) {
                g.load_palette(reinterpret_cast<unsigned short*>(At(at::kPalettes + m[5] * 64u)), 0);
                Sprite_Current[0] = static_cast<unsigned char>(Sprite_Current[0] & 0xDF);
                Sprite_Current[0x5D] = 0;
                Sprite_Current[0x5E] = 0;
                Sprite_Current[0x5F] = 0;
                Sprite_Current[0x5C] = 0;
                Sprite_Current[9] = 0;
                g.clear_steps();
                SetLong(Sprite_Current + 0x18, 0);
                SetLong(Sprite_Current + 0x1C, 0);
                SetLong(Sprite_Current + 0x20, 0);
                count = Field_MemberCount;
            }
            i = static_cast<unsigned char>(i + 1);
        } while (i < count);
    }
    Sprite_Current = ObjTrio;
    ObjTrio[2] = static_cast<unsigned char>(ObjTrio[2] + 1);
}

// original 0x52F350: sub-state 4 - once Field_Kind2Hold is 0: Field_ScriptFlags2
// bit 10 (Field_LeaderSwapTest's) cleared, the view's elevation to the
// leader's height, state 1 sub-state 0.
extern "C" void __cdecl Field_SwapEnd(void) {
    if (Field_Kind2Hold != 0) return;
    unsigned char* const sc = Sprite_Current;
    Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 & 0xFBFF);
    g.set_elevation(static_cast<int>(Word(sc + 0x3E)));
    Sprite_Current[1] = 1;
    Sprite_Current[2] = 0;
}

// --- state 4: the menu --------------------------------------------------------

// original 0x52F390: state 4 (Field_LeaderMenuTest sets it) - a tail jump
// through 0x66097C's two sub-states.
extern "C" void __cdecl Field_MenuState(void) { Table(at::kMenuSteps)[Sprite_Current[2]](); }

// original 0x52F3B0: sub-state 0 - sub-state 1 and Field_Request 1 (the menu).
extern "C" void __cdecl Field_MenuOpen(void) {
    Sprite_Current[2] = 1;
    Field_Request = 1;
}

// original 0x52F3D0: sub-state 1 - while Field_Request is 1 nothing; then
// Field_ScriptFlags2 bit 11 (Field_LeaderMenuTest's) cleared, state 1 sub-state 0.
extern "C" void __cdecl Field_MenuWait(void) {
    if (Field_Request == 1) return;
    unsigned char* const sc = Sprite_Current;
    Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 & 0xF7FF);
    sc[1] = 1;
    Sprite_Current[2] = 0;
}

// --- state 5: the encounter ----------------------------------------------------

// original 0x52F400: state 5 (Field_LeaderStepLands sets it when
// Field_EncounterDue) - a tail jump through 0x660984's two sub-states.
extern "C" void __cdecl Field_EncounterState(void) { Table(at::kEncounterSteps)[Sprite_Current[2]](); }

// original 0x52F420: sub-state 0 - outside an event battle (0x904AAA 0),
// Encounter_RollInitiative; in one, every member after the leader cleared
// and put in state 5, and +0xB = 0xFF. Then sub-state 1.
// As the original has it: the member count is read again after each clear.
extern "C" void __cdecl Field_EncounterStart(void) {
    if (Byte(at::kEventBattle) == 0) {
        g.roll_initiative();
        Sprite_Current[2] = 1;
        return;
    }
    if (Field_MemberCount > 1) {
        unsigned i = 1;
        unsigned count;
        do {
            g.clear_state(i);
            count = Field_MemberCount;
            Member(i)[1] = 5;
            ++i;
        } while (static_cast<int>(i) < static_cast<int>(count));
    }
    Sprite_Current[0xB] = 0xFF;
    Sprite_Current[2] = 1;
}

// original 0x52F490: sub-state 1 - with an effect object in +0xB, wait for it
// to end (Effect_Objects[+0xB] +0 zero), then +0xB = 0xFF; with none, wait
// for member 1 (and member 2, with three) to be in state 5; then +0xB = 0,
// Field_ScriptFlags2 bit 12 cleared and Field_Request 3.
// As the original has it: +0xB indexes the 128-byte effect objects whole.
extern "C" void __cdecl Field_EncounterWait(void) {
    unsigned char* const sc = Sprite_Current;
    const unsigned char effect = sc[0xB];
    if (effect != 0xFF) {
        if (Effect_Objects[effect * 0x80u] != 0) return;
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

// original 0x52F5E0: state 7 (Field_LeaderTalkTest sets it on a passage) -
// one of 0x66098C's four sub-states, then Sprite_ScriptTick (a tail jump).
extern "C" void __cdecl Field_PassageState(void) {
    Table(at::kPassageSteps)[Sprite_Current[2]]();
    g.script_tick();
}

// original 0x52F600: sub-state 0 - the passage faced (Area_PassageAhead); the
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
    const unsigned char* const e = g.passage_ahead();
    if (Field_InputFlags & 1) g.leader_animation(Sprite_Current[8]);
    else g.ensure_animation(static_cast<unsigned char>((Sprite_Current[8] >> 1) + 0x10));
    if (e == nullptr) bof3::Fatal("Field_PassageOpen: no passage ahead (the original reads through null)");
    const unsigned short word = Word(e + 4);
    if (!(word & 0x8000) || e[6] == 0xFF) {
        g.open_system(0);
    } else {
        switch (e[6]) {
        case 0:
            g.open_system(word & 0xFFFu);
            Sprite_Current[2] = static_cast<unsigned char>(Sprite_Current[2] + 1);
            break;
        case 1:
            g.open_script(static_cast<unsigned short>(word & 0xFFFu));
            Sprite_Current[2] = static_cast<unsigned char>(Sprite_Current[2] + 1);
            break;
        case 2: {
            const unsigned area = Game_AreaNumber;
            Field_ActiveMember = const_cast<unsigned char*>(buffer);
            buffer[0x86] = 0xFF;
            const unsigned char* const descriptor = Area_Descriptors[area];
            const unsigned char* const* scripts;
            std::memcpy(&scripts, descriptor + 4, sizeof scripts);
            g.script_run(scripts[Word(e + 4) & 0xFFFu]);
            const unsigned short message = static_cast<unsigned short>(buffer[0x88] | buffer[0x89] << 8);
            if (message != 0xFFFF) g.open_script(static_cast<unsigned short>(message & 0xFFFu));
            Field_State[0x12A] = Field_ActiveMember[0x86];
            SetWord(Field_State + 0x12C, Word(e + 4));
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

// original 0x52F760: sub-state 1 - once Field_Request is 0, the passage's
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
    const unsigned char* const e = g.passage_ahead();
    if (e == nullptr) bof3::Fatal("Field_PassageTake: no passage ahead (the original reads through null)");
    if (g.flags_test(At(at::kPassageFlags), e[4])) {
        g.open_system(1);
    } else if (e[6] == 0xFF) {
        SetLong(At(at::kPassageCount), Long(At(at::kPassageCount)) + 1);
        if (e[4] != 0xFF) g.flags_set(At(at::kPassageFlags), e[4]);
        g.play_effect(0x106);
        g.give_zenny(e[5] * 40);
    } else {
        const unsigned char* const name = g.item_name(e[6], static_cast<unsigned>(Word(e + 4)) >> 8);
        std::memcpy(At(bof3::addr::Text_Records), name, 16);
        if (g.inventory_add(e[6], static_cast<unsigned>(Word(e + 4)) >> 8, 1)) {
            if (e[4] != 0xFF) {
                g.flags_set(At(at::kPassageFlags), e[4]);
                SetLong(At(at::kPassageCount), Long(At(at::kPassageCount)) + 1);
            }
            g.play_effect(0x106);
            g.open_system(2);
        } else {
            g.open_system(3);
        }
    }
    unsigned char* const sc = Sprite_Current;
    Field_Request = 2;
    sc[2] = 2;
}

// original 0x52F8A0: sub-state 2 - once Field_Request is 0, the standing pose
// (Field_LeaderAnimation on foot, else Sprite_EnsureAnimation) and state 1.
extern "C" void __cdecl Field_PassageEnd(void) {
    if (Field_Request != 0) return;
    if (Field_InputFlags & 1) g.leader_animation(Sprite_Current[8]);
    else g.ensure_animation(Sprite_Current[8]);
    Sprite_Current[1] = 1;
    Sprite_Current[2] = 0;
}

// --- state 10: the action by party set ------------------------------------------

// original 0x52FB60: state 10 (Field_LeaderCheckTest and
// Field_LeaderEffectTest set it) - the handler at 0x6609D0 for the loaded
// party set (0x90412C & 0x7F); then, when it left Field_State +0x137 at 0,
// the pose, member 0 cleared and state 1.
extern "C" void __cdecl Field_ActionState(void) {
    Table(at::kActionBySet)[Byte(at::kPartySet) & 0x7F]();
    if (Field_State[0x137] != 0) return;
    g.ensure_animation(Sprite_Current[8]);
    g.clear_state(0);
    Sprite_Current[1] = 1;
}

// --- the cells and the exits -----------------------------------------------------

// original 0x5317F0: the encounter's area - the word after Game_AreaNumber's
// in the ten pairs at 0x660A90, to 0x937F82; nothing when it is not listed.
// Called only by Field_LeaderWalk.
extern "C" void __cdecl Field_EncounterArea(void) {
    const unsigned short area = Game_AreaNumber;
    for (std::uint32_t p = at::kEncounterAreas; p < at::kEncounterAreasEnd; p += 4) {
        if (Word(At(p)) == area) {
            SetWord(At(at::kExitArea), Word(At(p + 2)));
            return;
        }
    }
}

// original 0x531920: whether the cell (x, z) has an event - its code
// (AreaMap_ByteAt) in the 0xFF-ended list at 0x660B10. Called by the world
// map's frame (eleven sites) and Field_LeaderCellEvent.
extern "C" unsigned char __cdecl Field_CellHasEvent(long x, long z) {
    const unsigned char code = g.byte_at(static_cast<short>(x), static_cast<short>(z));
    for (const unsigned char* p = At(at::kEventCells); *p != 0xFF; ++p)
        if (code == *p) return 1;
    return 0;
}

// original 0x531AF0: the exit of the cell the leader stands on - a 0xA0 cell's
// record in the area's exit list (0x462AC0) by the whole x and z: its area to
// 0x937F82, its kind to 0x905B88, and the fixed entry point (0xE8000,
// 0x410000). Called only by Field_LeaderCellEvent.
// As the original has it: the list has no end test - a cell with no record
// walks on.
extern "C" void __cdecl Field_ExitFromCell(void) {
    {
        unsigned char* const sc = Sprite_Current;
        if (g.byte_at(static_cast<short>(Word(sc + 0x36)), static_cast<short>(Word(sc + 0x3A))) != 0xA0) return;
    }
    const unsigned char* r = g.area_exits();
    const unsigned char* const sc = Sprite_Current;
    const unsigned short x = Word(sc + 0x36);
    while (!(x == r[0] && Word(sc + 0x3A) == r[1])) r += 4;
    SetWord(At(at::kExitArea), r[2]);
    SetLong(At(at::kExitX), 0xE8000);
    SetLong(At(at::kExitZ), 0x410000);
    Byte(at::kExitKind) = r[3];
}

// original 0x533780: the pending jump's entry 0 (0x660B60, Field_PendingJump
// jumps to it while 0x904EF0 is set; Area_Enter 0x533710 sets it) - the party
// walking in. First frame (0x904EF2 = 0): the leader's +0 bit 6 and state
// cleared, +0x138 bit 1 cleared, the count 1. Later, once the leader is a
// whole unit or more from the stored point (0x904EF4 / 0x904EF8) on either
// axis: each member after the leader whose bit (1 << the count) is clear in
// Field_ScriptFlags is released - +0 bit 6, state 2 sub-state 2, +3 / +4 0,
// +0x138 bit 1, and its bit cleared in Field_ScriptFlags2 - counting one per
// member. When the count reaches the member count: MoveCmd_TestFC at the
// point's whole parts, Field_ScriptFlags2 bit 4 cleared, 0x904EF0 = 0.
// As the original has it: the bit tested is the count's, not the member's (so
// it starts at 1 << 1 and goes on past the members), shifted mod 32, and
// cleared in the 16-bit flags only.
extern "C" void __cdecl Field_PendingWalkIn(void) {
    unsigned char n = Byte(at::kPendingCount);
    if (n == 0) {
        ObjTrio[0] = static_cast<unsigned char>(ObjTrio[0] & 0xBF);
        g.clear_state(0);
        const unsigned char f = ObjTrio[0x138];
        n = Byte(at::kPendingCount);
        ObjTrio[0x138] = static_cast<unsigned char>(f & 0xFD);
        n = static_cast<unsigned char>(n + 1);
        Byte(at::kPendingCount) = n;
    } else {
        const std::int32_t dx = Abs(static_cast<std::uint32_t>(Long(At(at::kLeaderX))) - static_cast<std::uint32_t>(Long(At(at::kPendingX))));
        if (dx < 0x10000) {
            const std::int32_t dz = Abs(static_cast<std::uint32_t>(Long(At(at::kLeaderZ))) - static_cast<std::uint32_t>(Long(At(at::kPendingZ))));
            if (dz < 0x10000) return;
        }
        const unsigned count = Field_MemberCount;
        if (static_cast<int>(count) > 1) {
            unsigned short flags2 = Field_ScriptFlags2;
            const unsigned flags = Field_ScriptFlags;
            unsigned left = count - 1;
            unsigned char* m = Member(1);
            do {
                const std::uint32_t bit = 1u << (n & 31u);
                if (!(flags & bit)) {
                    m[0] = static_cast<unsigned char>(m[0] & 0xBF);
                    m[1] = 2;
                    m[2] = 2;
                    m[3] = 0;
                    m[4] = 0;
                    m[0x138] = static_cast<unsigned char>(m[0x138] & 0xFD);
                    flags2 = static_cast<unsigned short>(flags2 & ~bit);
                }
                n = static_cast<unsigned char>(n + 1);
                m += at::kMemberStride;
            } while (--left != 0);
            Field_ScriptFlags2 = flags2;
            Byte(at::kPendingCount) = n;
        }
    }
    if (n == Field_MemberCount) {
        g.test_fc(static_cast<short>(Word(At(at::kPendingX + 2))), static_cast<short>(Word(At(at::kPendingZ + 2))));
        Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 & 0xFFEF);
        Byte(at::kPendingFlag) = 0;
    }
}

// --- the way blocked -------------------------------------------------------------

// original 0x535610: whether the way to (x, z) is blocked for a sprite of
// ground height `ground` - 0x535830 (a raised sprite's) when the byte
// `raised` is set, else Field_WayBlocked4. Their al.
extern "C" unsigned char __cdecl Field_WayBlocked(long x, long z, unsigned raised, long ground) {
    if (raised & 0xFF) return g.way_blocked_wide(x, z, ground);
    return g.way_blocked4(x, z, ground);
}

// original 0x535640: the 2 x 2 footprint's - blocking cells (Field_CellsBlock);
// a slope above 0x40 (MapView_SlopeAt's word, with its "slope" byte 0x903850)
// to the right when x has a fraction (direction 3), below when z has one (5),
// diagonally when both (4); the ground more than 0xC0 from `ground` (words);
// or an object there (Sprite_ObjectAt, with Sprite_Current's height +0x3E
// set to `ground` for the call and put back into Sprite_Current read again).
extern "C" unsigned char __cdecl Field_WayBlocked4(long x, long z, long ground) {
    if (g.cells_block(x, z, 0)) return 1;
    const bool fx = (x & 0xFFFF) != 0, fz = (z & 0xFFFF) != 0;
    if (fx && static_cast<short>(g.slope_at(x, z, 3)) > 0x40 && Byte(at::kScratch) != 0) return 1;
    if (fz && static_cast<short>(g.slope_at(x, z, 5)) > 0x40 && Byte(at::kScratch) != 0) return 1;
    if (fx && fz && static_cast<short>(g.slope_at(x, z, 4)) > 0x40 && Byte(at::kScratch) != 0) return 1;
    const long here = g.ground_at(x, z);
    const int rise = static_cast<short>(here) - static_cast<short>(ground);
    if (std::abs(rise) > 0xC0) return 1;
    unsigned char* const sc = Sprite_Current;
    const unsigned short height = Word(sc + 0x3E);
    SetWord(sc + 0x3E, static_cast<unsigned>(ground));
    const unsigned char object = g.object_at(x, z, 0);
    SetWord(Sprite_Current + 0x3E, height);
    return object != 0xFF;
}

// original 0x535730: whether the footprint at (x, z) (`wide` as
// AreaMap_CellsNone takes it) touches a blocking cell - any of the codes at
// 0x660C24 with the low nibble dropped; 0x10; 0x11 unless Field_State +0x138
// bit 0; 0x20 and 0x21 both; 0xC0 and 0xA0 (nibble dropped) both; 0xA2.
extern "C" unsigned char __cdecl Field_CellsBlock(long x, long z, unsigned wide) {
    for (const unsigned char* p = At(at::kBlockingCells); *p != 0xFF; ++p)
        if (!g.cells_none(x, z, wide, *p, 1)) return 1;
    if (!g.cells_none(x, z, wide, 0x10, 0)) return 1;
    if (!(Field_State[0x138] & 1) && !g.cells_none(x, z, wide, 0x11, 0)) return 1;
    if (!g.cells_none(x, z, wide, 0x20, 0) && !g.cells_none(x, z, wide, 0x21, 0)) return 1;
    if (!g.cells_none(x, z, wide, 0xC0, 0) && !g.cells_none(x, z, wide, 0xA0, 1)) return 1;
    return g.cells_none(x, z, wide, 0xA2, 0) == 0 ? 1 : 0;
}

void EventLeader_Inject() {
    if (bof3::WantsShadow("event_leader")) event_leader::SelfTest();
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
    BOF3_INJECT(Field_EncounterArea);
    BOF3_INJECT(Field_CellHasEvent);
    BOF3_INJECT(Field_ExitFromCell);
    BOF3_INJECT(Field_PendingWalkIn);
    BOF3_INJECT(Field_WayBlocked);
    BOF3_INJECT(Field_WayBlocked4);
    BOF3_INJECT(Field_CellsBlock);
}

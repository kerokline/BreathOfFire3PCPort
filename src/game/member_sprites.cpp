// The party members' sprites on the field - the second and third members
// following the leader, walking and running: the member's frame (0x51AC50),
// its states 0 and 1 (0x51AC70, 0x51AD40) with state 1's two sub-states
// (0x51AD60, 0x51AFC0 and its tail 0x51AFE0), the follow step and its helpers
// (0x51B050, 0x51B430, 0x51B5D0, 0x51B9D0, 0x51BDA0), and the cell tests the
// leader shares (0x526DB0, 0x526DD0, 0x527470, 0x528070 .. 0x528770).
// docs/member-sprites.md has each function, its evidence, and the fuzz below
// with its negative controls.
//
// "Member i" is ObjTrio + i * 0x14C. The "cells" are the bytes at 0x903850:
// cell 0 is the flag 0x5725C0 leaves (an object ahead), cells 1..5 what
// Field_ReadCells found around the sprite, cells 8..10 Field_CellAheadFlat's
// classes - and cells 8..0xF double as the two longs of the aim point.
//
// As in the original, every global is read again after each call and every
// object through Sprite_Current or Field_State each time it is used.
#include "game/member_sprites.h"

#include <cstddef>
#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/member_sprites_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace member_sprites {

namespace {
template <class To> To Fn(std::uint32_t address) { return reinterpret_cast<To>(static_cast<std::uintptr_t>(address)); }
}  // namespace

// Every callee. Those this module implements are ours (their names are bound
// to our functions by symbols.gen.h); the unnamed ones are other groups' or
// nobody's, Capcom's addresses typed from their call sites and, where read,
// their first instructions (docs/member-sprites.md, section 3).
const Callees kOriginals = {
    Sprite_RestoreClut, reinterpret_cast<const std::uint32_t*>(at::kMemberStates),
    Sprite_ClearSteps, MapView_GroundAt, Sprite_SetAnimation,
    reinterpret_cast<const std::uint32_t*>(at::kControlStates), Sprite_ScriptTick,
    Fn<unsigned char (__cdecl*)(unsigned)>(0x535120), Fn<unsigned char (__cdecl*)(unsigned)>(0x535240),
    Sprite_EnsureAnimation, Member_Idle, Member_FollowStep,
    Fn<unsigned char (__cdecl*)()>(0x531DF0), Fn<void (__cdecl*)()>(0x534610), Fn<void (__cdecl*)()>(0x535F50),
    Fn<void (__cdecl*)()>(0x52E140),
    Member_WalkEnd,
    Fn<void (__cdecl*)()>(0x535270), Fn<void (__cdecl*)()>(0x5350C0), Fn<void (__cdecl*)()>(0x534F10),
    Fn<void (__cdecl*)()>(0x534A00), Fn<unsigned char (__cdecl*)()>(0x534920),
    Member_Follow,
    Member_CatchUp, Field_DirectionTo, Member_StepAhead,
    Fn<unsigned char (__cdecl*)(long, long, unsigned, long)>(0x535610),
    Field_CellAhead, Fn<long (__cdecl*)(long, long, unsigned)>(0x5725C0),
    Fn<unsigned char (__cdecl*)(unsigned, unsigned)>(0x592890),
    Rand,
    Fn<unsigned char (__cdecl*)()>(0x527640), Field_CellAheadFlat,
    Field_ReadCells, Field_CellClass, Field_TurnUnless, Field_CellSlope, Field_CellPairTurn,
    Field_CellKind,
    AreaMap_ByteAt, Field_CellFacing, Field_ObjectAhead,
};
Callees g = kOriginals;

}  // namespace member_sprites

namespace {

using namespace move_script;
using member_sprites::g;
namespace at = member_sprites::at;
using member_sprites::Handler;

constexpr std::uint32_t kMemberBytes = 0x14C, kActorBytes = 0xA4;

std::uint32_t Address(const volatile void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
// Member i by address, i unchecked (the original indexes past the three).
unsigned char* Member(unsigned i) { return At(Address(ObjTrio) + i * kMemberBytes); }
unsigned char& Cell(unsigned i) { return At(at::kCells)[i & 0xFF]; }
std::uint32_t U(const unsigned char* at) { return static_cast<std::uint32_t>(Long(at)); }
void Dispatch(const std::uint32_t* table, unsigned index) {
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(table[index]))();
}
// `cdq; xor eax, edx; sub eax, edx`: INT_MIN stays INT_MIN.
std::int32_t Abs(std::uint32_t v) {
    const std::uint32_t s = static_cast<std::uint32_t>(static_cast<std::int32_t>(v) >> 31);
    return static_cast<std::int32_t>((v ^ s) - s);
}
// `cdq; sub eax, edx; sar eax, 1`: a signed halving that rounds toward zero.
std::int32_t Half(std::uint32_t v) {
    const std::int32_t s = static_cast<std::int32_t>(v);
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(s) + (s < 0 ? 1u : 0u)) >> 1;
}
// A 16-bit value as the upper half of a 16.16 long.
std::uint32_t High(unsigned v) { return (v & 0xFFFF) << 16; }
// The two longs of a direction's row, index unchecked.
std::uint32_t RowX(std::uint32_t table, unsigned dir) { return U(At(table + dir * 8)); }
std::uint32_t RowZ(std::uint32_t table, unsigned dir) { return U(At(table + dir * 8 + 4)); }
// The low bytes of the two flag words, ORed (`mov al, [0x905BA4]; or al, [0x9039A2]`).
unsigned FlagsLow() { return (Field_ScriptFlags2 | Field_ScriptFlags) & 0xFF; }

}  // namespace

// --- the member's frame and its states ----------------------------------------

// original 0x51AC50: a party member's frame (PSX 0x801BCE40), called by
// Field_MembersFrame for members 1 and 2 and by Field_PartyFirstFrame.
//
// As the original has it: the state is Sprite_Current +1, a whole byte and
// unchecked - the table at 0x65F960 has 9 entries, then zeros. The original
// jumps (a tail call).
extern "C" void __cdecl Field_MemberFrame(void) {
    g.restore_clut();
    Dispatch(g.member_states, Sprite_Current[1]);
}

// original 0x51AC70, state 0 (PSX 0x801BCE90): the member's counters cleared,
// +6 its index less one, grounded, walking speed 3, its animation, state 1.
extern "C" void __cdecl Member_Start(void) {
    g.clear_steps();
    SetLong(Sprite_Current + 0x18, 0);
    SetLong(Sprite_Current + 0x1C, 0);
    SetLong(Sprite_Current + 0x20, 0);
    Sprite_Current[6] = static_cast<unsigned char>(Sprite_Current[5] - 1);
    Sprite_Current[7] = 0;
    {
        const unsigned char* const c = Sprite_Current;
        const long z = Long(c + 0x38), x = Long(c + 0x34);
        const long ground = g.ground_at(x, z);
        SetWord(Sprite_Current + 0x3E, static_cast<std::uint16_t>(ground));
    }
    Sprite_Current[0x5D] = 0;
    Sprite_Current[0x5E] = 0;
    Sprite_Current[0x5F] = 0;
    Sprite_Current[0x5C] = 2;
    Field_State[0x128] = 3;
    Field_State[0x137] = 0;
    Field_State[0x136] = 0;
    g.set_animation(Sprite_Current[8]);
    Sprite_Current[1] = static_cast<unsigned char>(Sprite_Current[1] + 1);
    Sprite_Current[2] = 0;
}

// original 0x51AD40, state 1 (PSX 0x801BCFAC): its sub-state through the
// two-entry table 0x65F99C, then Sprite_ScriptTick.
//
// As the original has it: the sub-state Sprite_Current +2 is unchecked; the
// tick is a tail jump whose al no caller reads.
extern "C" void __cdecl Member_Control(void) {
    Dispatch(g.control_states, Sprite_Current[2]);
    g.script_tick();
}

// original 0x51AD60, sub-state 0 of state 1 (PSX 0x801BCFF8): the member
// standing, or deciding to follow. Also called by Member_WalkEnd each time a
// step ends, and jumped to from 0x51BBBF (the end of state 6, 0x51BAA0 - the
// fidget Member_Idle starts).
//
// Member_FollowStep's answer picks: 0 walk a step (sub-state 1), 1..4 stand,
// 5 hop to state 2, 6 to state 8; anything above 6 nothing.
//
// As the original has it: the ground at the step's end is fetched when
// Field_State +0x89 is 2 and thrown away (MapView_GroundAt's height-scale
// side effect is all it leaves); the animation 8 on is a byte add.
extern "C" void __cdecl Member_Follow(void) {
    if (Field_Request == 9) {
        Sprite_Current[1] = 0xE;
        Sprite_Current[2] = 0;
        return;
    }
    const unsigned flags = Field_ScriptFlags2;
    if (flags & 0x1000) {
        Sprite_Current[1] = 5;
        Sprite_Current[2] = 0;
        Sprite_Current[0xB] = 0;
        return;
    }
    if (flags & 0xC00) return;
    if (g.test_535120(1)) return;
    if (g.test_535240(1)) return;
    if (ObjTrio[1] == 0xA) {
        g.ensure_animation(Sprite_Current[8]);
        Sprite_Current[1] = 7;
        Sprite_Current[2] = 0;
        return;
    }
    if (g.idle()) return;
    switch (g.follow_step()) {
    case 0: {
        if (g.test_531df0()) {
            g.ensure_animation(Sprite_Current[8]);
            Sprite_Current[2] = 0;
            return;
        }
        unsigned char* const state = Field_State;
        if (!(Field_ActorStates[state[0x148] * kActorBytes] & 0x20)) state[0x136] = 0;
        g.call_534610();
        g.call_535f50();
        g.ensure_animation(static_cast<unsigned char>(Sprite_Current[8] + 8));
        if (Field_State[0x89] == 2) {
            const unsigned char* const c = Sprite_Current;
            const std::uint32_t steps = c[9];
            const std::uint32_t x = U(c + 0xC) * steps + U(c + 0x34);
            const std::uint32_t z = U(c + 0x10) * steps + U(c + 0x38);
            g.ground_at(static_cast<long>(x), static_cast<long>(z));
        }
        Sprite_Current[9] = static_cast<unsigned char>(Sprite_Current[9] - 1);
        g.call_52e140();
        Field_State[0x137] = 1;
        Sprite_Current[2] = 1;
        return;
    }
    case 1:
    case 2:
    case 3:
    case 4:
        g.clear_steps();
        g.ensure_animation(Sprite_Current[8]);
        Field_State[0x137] = 0;
        Sprite_Current[2] = 0;
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
        unsigned char* const c = Sprite_Current;
        c[2] = (c[8] == 1 || c[8] == 7) ? 0 : 1;
        return;
    }
    default:
        return;
    }
}

// original 0x51AFC0, sub-state 1 of state 1 (PSX 0x801BD3B0): a step's
// frames counted down in +9 through 0x52E140 (tail jumps both).
extern "C" void __cdecl Member_Walk(void) {
    unsigned char* const c = Sprite_Current;
    const unsigned char left = c[9];
    if (left == 0) {
        g.walk_end();
        return;
    }
    c[9] = static_cast<unsigned char>(left - 1);
    g.call_52e140();
}

// original 0x51AFE0 (PSX 0x801BD3F8): the step's end - four event checks, two
// more tests that may take the frame, then stopped and Member_Follow asked
// again; standing still in state 1 afterwards, back to sub-state 0.
extern "C" void __cdecl Member_WalkEnd(void) {
    g.call_535270();
    g.call_5350c0();
    g.call_534f10();
    g.call_534a00();
    if (g.test_535120(0)) return;
    if (g.test_535240(0)) return;
    if (g.test_534920()) return;
    g.clear_steps();
    Field_State[0x128] = 3;
    g.follow();
    unsigned char* const c = Sprite_Current;
    if (Long(c + 0xC) != 0) return;
    if (Long(c + 0x10) != 0) return;
    if (c[1] != 1) return;
    c[2] = 0;
}

// --- following the leader -------------------------------------------------------

// original 0x51B050 (PSX 0x801BD758): who this member follows (+6: the leader,
// or member 1 or 2), whether it is already close enough (1), else a catch-up,
// the walking speed from the distance (Field_State +0x128, 3..5), a direction
// and Member_StepAhead's answer - and with the sprite's bit 0x80 and the
// flags' bit 0 clear, a jump to the leader's next half-cell when that is free
// (0xFF, state 2 sub-state 2).
//
// As the original has it: the member indices (+5 ^ 3, +6) are unchecked; the
// distances are the `cdq` absolute values, INT_MIN staying negative, compared
// signed; the leader's next position is read twice, before and after the
// calls; Field_State is the leader's (ObjTrio) for the two calls of the free
// test and put back after, whatever they did to it.
extern "C" unsigned char __cdecl Member_FollowStep(void) {
    unsigned char* c = Sprite_Current;
    const unsigned char* const leader = Member(0);
    const unsigned char facing = c[8];
    const std::uint32_t leader_steps = leader[9];
    const std::uint32_t tx = leader_steps * U(leader + 0xC) + U(leader + 0x34);
    const std::uint32_t tz = leader_steps * U(leader + 0x10) + U(leader + 0x38);
    const unsigned char who = c[5];
    if (Member((who & 0xFFu) ^ 3u)[0x137] != 9) {
        if (who == 2) {
            c[6] = Member(1)[6] != 0 ? 0 : 1;
        } else if (Field_MemberCount <= 2) {
            c[6] = 0;
        } else {
            const unsigned char* const third = Member(2);
            const std::uint32_t steps = third[9];
            const std::int32_t mine = static_cast<std::int32_t>(static_cast<std::uint32_t>(Abs(U(c + 0x38) - tz)) +
                                                                static_cast<std::uint32_t>(Abs(U(c + 0x34) - tx)));
            const std::int32_t theirs = static_cast<std::int32_t>(
                static_cast<std::uint32_t>(Abs(steps * U(third + 0xC) - tx + U(third + 0x34))) +
                static_cast<std::uint32_t>(Abs(steps * U(third + 0x10) - tz + U(third + 0x38))));
            c[6] = mine > theirs ? 2 : 0;
        }
        c = Sprite_Current;
    }
    // The one followed: where its step ends, and two half-cells behind that.
    const unsigned char* const target = Member(c[6]);
    const std::uint32_t target_steps = target[9];
    const std::uint32_t gx = U(target + 0xC) * target_steps + U(target + 0x34);
    const std::uint32_t gz = U(target + 0x10) * target_steps + U(target + 0x38);
    const unsigned dir = target[8];
    const std::int32_t near = target[0x70] ? 0x20000 : 0x18000;
    const std::uint32_t px = gx - (RowX(at::kDirSteps, dir) << 1);
    const std::uint32_t pz = gz - (RowZ(at::kDirSteps, dir) << 1);
    if (Abs(U(c + 0x34) - gx) <= near && Abs(U(c + 0x38) - gz) <= near) return 1;

    g.catch_up(static_cast<long>(px), static_cast<long>(pz));
    {
        const unsigned char* const s = Sprite_Current;
        const std::int32_t dx = Abs(U(s + 0x34) - px);
        std::int32_t dz = 0;
        if (dx > 0x60000 || (dz = Abs(U(s + 0x38) - pz)) > 0x60000) {
            Field_State[0x128] = 5;
        } else if (dx <= 0x20000 && dz <= 0x20000) {
            const unsigned char speed = Member(s[6])[0x128];
            Field_State[0x128] = speed;
        } else {
            const unsigned char speed = static_cast<unsigned char>(Member(s[6])[0x128] + 1);
            Field_State[0x128] = speed;
        }
    }
    {
        unsigned char* const state = Field_State;
        const unsigned char speed = state[0x128];
        state[0x128] = speed < 3 ? 3 : speed > 5 ? 5 : speed;
    }
    g.direction_to(static_cast<long>(px), static_cast<long>(pz), Sprite_Current + 8);
    unsigned char answer = g.step_ahead();
    if (answer != 1) {
        c = Sprite_Current;
        const std::int32_t mine = static_cast<std::int32_t>(static_cast<std::uint32_t>(Abs(U(c + 0x38) - pz)) +
                                                            static_cast<std::uint32_t>(Abs(U(c + 0x34) - px)));
        const std::int32_t aim = static_cast<std::int32_t>(static_cast<std::uint32_t>(Abs(U(At(at::kAimZ)) - pz)) +
                                                           static_cast<std::uint32_t>(Abs(U(At(at::kAimX)) - px)));
        if (mine >= aim) return answer;
        c[8] = facing;
        answer = 1;
    }
    if (FlagsLow() & 1) return answer;
    if (!(Sprite_Current[0] & 0x80)) return answer;

    // The leader's next position, to the half-cell: free for this member?
    const std::uint32_t nz = (leader[9] * U(leader + 0x10) + U(leader + 0x38)) & 0xFFFF8000u;
    const std::uint32_t nx = (leader[9] * U(leader + 0xC) + U(leader + 0x34)) & 0xFFFF8000u;
    unsigned char* const saved = Field_State;
    Field_State = Member(0);
    const long ground = g.ground_at(static_cast<long>(nx), static_cast<long>(nz));
    const unsigned char blocked = g.blocked_at(static_cast<long>(nx), static_cast<long>(nz), Sprite_Current[0x70], ground);
    Field_State = saved;
    if (blocked) return answer;
    SetLong(Sprite_Current + 0x34, static_cast<std::int32_t>(nx));
    SetLong(Sprite_Current + 0x38, static_cast<std::int32_t>(nz));
    Sprite_Current[0] = static_cast<unsigned char>(Sprite_Current[0] | 0x40);
    Field_State[0x138] = static_cast<unsigned char>(Field_State[0x138] & 0xFE);
    Field_State[0x138] = static_cast<unsigned char>(Field_State[0x138] | (Member(0)[0x138] & 1));
    const long here = g.ground_at(static_cast<long>(nx), static_cast<long>(nz));
    SetWord(Sprite_Current + 0x3E, static_cast<std::uint16_t>(here));
    Sprite_Current[0x29] = Member(0)[0x29];
    Sprite_Current[1] = 2;
    Sprite_Current[2] = 2;
    return 0xFF;
}

// original 0x51B430 (PSX 0x801BD4C8): the step about to be taken - +9 the
// frames it lasts (0x10, or 0x20 facing 2 or 6, over the walking speed), the
// aim point where it ends in 0x903858 / 0x90385C (doubled on a raised sprite,
// +0x70) - and what Field_CellAhead says of the cell ahead as 0 no step, 1
// step, 5 or 6 (state 2 or 8); on 3 a step only if nothing is in the way,
// turning back to the direction it had when something is.
//
// As the original has it: the speed is Field_MoveSpeeds[Field_State +0x128],
// unchecked - a speed of 0 (entry 0) faults on the division, as the original's
// idiv does; Member_FollowStep clamps +0x128 to 3..5 first. The ground at the
// aim point is fetched when there is no object there and thrown away.
extern "C" unsigned char __cdecl Member_StepAhead(void) {
    const unsigned char facing = Sprite_Current[8];
    const unsigned char kind = g.cell_ahead();
    {
        unsigned char* const c = Sprite_Current;
        c[9] = (c[8] == 2 || c[8] == 6) ? 0x20 : 0x10;
    }
    const unsigned speed = At(at::kMoveSpeeds)[Field_State[0x128]];
    {
        unsigned char* const c = Sprite_Current;
        const volatile int divisor = static_cast<int>(speed);
        c[9] = static_cast<unsigned char>(static_cast<int>(c[9]) / divisor);
    }
    unsigned char* const c = Sprite_Current;
    const std::uint32_t frames = c[9];
    std::uint32_t ax, az;
    if (c[0x70] == 0) {
        ax = RowX(at::kDirUnits, c[8]) * frames * speed + U(c + 0x34);
        az = RowZ(at::kDirUnits, c[8]) * frames * speed + U(c + 0x38);
    } else {
        ax = U(c + 0x34) + ((RowX(at::kDirUnits, c[8]) * frames * speed) << 1);
        az = U(c + 0x38) + ((RowZ(at::kDirUnits, c[8]) * frames * speed) << 1);
    }
    SetLong(At(at::kAimX), static_cast<std::int32_t>(ax));
    SetLong(At(at::kAimZ), static_cast<std::int32_t>(az));
    switch (kind) {
    case 0:
    case 2:
    case 4:
    case 5:
        return 1;
    case 3: {
        const long r = g.object_at(static_cast<long>(ax), static_cast<long>(az), c[8]);
        if (Cell(0) == 0) return 0;
        if (static_cast<short>(r) > 0x40) {
            Sprite_Current[8] = facing;
            return 1;
        }
        g.ground_at(Long(At(at::kAimX)), Long(At(at::kAimZ)));
        return 0;
    }
    case 6: return 5;
    case 7: return 6;
    default: return 0;
    }
}

namespace {
// Member_CatchUp's free test at the sprite's position.
unsigned char BlockedHere() {
    const unsigned char* c = Sprite_Current;
    const long ground = g.ground_at(Long(c + 0x34), Long(c + 0x38));
    c = Sprite_Current;
    return g.blocked_at(Long(c + 0x34), Long(c + 0x38), c[0x70], ground);
}
}  // namespace

// original 0x51B5D0 (PSX 0x801BDD58): a member left far behind (x, z) jumps
// toward it: n tries (one per 0x10000 of the larger distance past 0xF0000,
// less a 256th of the ground's height difference, at least 1), the k-th from
// the end putting the sprite k double half-cells from where it stood in the
// direction of (x, z), and after the first two, as many cells either side on
// x and then on z. The first free place is kept if the sprite is then more
// than 6 cells from the leader and 0x592890 has a cell there (taking the
// direction, Draw_OtSlot, and clearing Field_State +0x138 bit 0); otherwise,
// and when nothing was free, it stays where it was. Either way the position
// is cut to the half-cell and grounded.
//
// As the original has it: the direction is a byte the first
// Field_DirectionTo leaves on the stack, and it writes none when (x, z) is
// the sprite's own position - the original then reads what the stack held.
// Member_FollowStep, the only caller, never asks that: it calls only when a
// coordinate is more than 0x18000 away, and the first try is made from where
// the sprite stands. Ours starts the byte at 0. A z-side try that fails
// leaves the sprite at its last tried z until the next try.
extern "C" void __cdecl Member_CatchUp(long x_arg, long z_arg) {
    const std::uint32_t x = static_cast<std::uint32_t>(x_arg), z = static_cast<std::uint32_t>(z_arg);
    const unsigned char* const c0 = Sprite_Current;
    if (!(c0[0] & 0x80)) return;
    if (FlagsLow() & 1) return;
    if (Member(0)[0x138] & 1) return;
    const std::uint32_t ox = U(c0 + 0x34), oz = U(c0 + 0x38);
    std::int32_t far = Abs(ox - x);
    const std::int32_t far_z = Abs(oz - z);
    if (far < far_z) far = far_z;
    std::int32_t tries = far >= 0x100000 ? (static_cast<std::int32_t>(static_cast<std::uint32_t>(far) - 0xF0000u) >> 16) + 1 : 1;
    const short here = static_cast<short>(g.ground_at(static_cast<long>(ox), static_cast<long>(oz)));
    const short there = static_cast<short>(g.ground_at(x_arg, z_arg));
    tries -= Abs(static_cast<std::uint32_t>(static_cast<std::int32_t>(here) - static_cast<std::int32_t>(there))) >> 8;
    if (tries <= 0) tries = 1;

    std::int32_t left = tries;   // the tries still to make, the k of this one
    std::int32_t side = -1;      // how far the side tries reach, one more each time
    unsigned char dir = 0;       // the original's stack byte (see above)
    bool found = false;
    for (;;) {
        g.direction_to(x_arg, z_arg, &dir);
        SetLong(Sprite_Current + 0x34, static_cast<std::int32_t>(((RowX(at::kDirSteps, dir) << 1) * static_cast<std::uint32_t>(left)) + ox));
        SetLong(Sprite_Current + 0x38, static_cast<std::int32_t>(((RowZ(at::kDirSteps, dir) << 1) * static_cast<std::uint32_t>(left)) + oz));
        {
            const unsigned char* c = Sprite_Current;
            const long object = g.object_at(Long(c + 0x34), Long(c + 0x38), dir);
            c = Sprite_Current;
            if (!g.blocked_at(Long(c + 0x34), Long(c + 0x38), c[0x70], object)) {
                found = true;
                break;
            }
        }
        // Either side on x.
        const std::uint32_t tried_x = U(Sprite_Current + 0x34);
        if (side > 1) {
            std::int32_t i = 1;
            std::uint32_t plus = tried_x + 0x10000, minus = tried_x - 0x10000;
            bool free = false;
            for (;;) {
                SetLong(Sprite_Current + 0x34, static_cast<std::int32_t>(plus));
                if (!BlockedHere()) { free = true; break; }
                SetLong(Sprite_Current + 0x34, static_cast<std::int32_t>(minus));
                if (!BlockedHere()) { free = true; break; }
                ++i;
                plus += 0x10000;
                minus -= 0x10000;
                if (i >= side) break;
            }
            if (free && i < side) {
                found = true;
                break;
            }
        }
        SetLong(Sprite_Current + 0x34, static_cast<std::int32_t>(tried_x));
        // Either side on z.
        const std::uint32_t tried_z = U(Sprite_Current + 0x38);
        if (side > 1) {
            std::int32_t i = 1;
            std::uint32_t plus = tried_z + 0x10000, minus = tried_z - 0x10000;
            bool free = false;
            for (;;) {
                SetLong(Sprite_Current + 0x38, static_cast<std::int32_t>(plus));
                if (!BlockedHere()) { free = true; break; }
                SetLong(Sprite_Current + 0x38, static_cast<std::int32_t>(minus));
                if (!BlockedHere()) { free = true; break; }
                ++i;
                plus += 0x10000;
                minus -= 0x10000;
                if (i >= side) break;
            }
            if (free && i < side) {
                found = true;
                break;
            }
        }
        --left;
        ++side;
        if (left <= 0) break;
    }

    unsigned char* c = Sprite_Current;
    if (!found) {
        SetLong(c + 0x34, static_cast<std::int32_t>(ox));
        SetLong(Sprite_Current + 0x38, static_cast<std::int32_t>(oz));
    } else if (Abs(U(c + 0x34) - U(Member(0) + 0x34)) <= 0x60000 && Abs(U(c + 0x38) - U(Member(0) + 0x38)) <= 0x60000) {
        SetLong(c + 0x34, static_cast<std::int32_t>(ox));
        SetLong(Sprite_Current + 0x38, static_cast<std::int32_t>(oz));
    } else if (g.map_cell(Word(c + 0x36), Word(c + 0x3A))) {
        Field_State[0x138] = static_cast<unsigned char>(Field_State[0x138] & 0xFE);
        Sprite_Current[0x29] = Draw_OtSlot;
        Sprite_Current[8] = dir;
    } else {
        SetLong(Sprite_Current + 0x34, static_cast<std::int32_t>(ox));
        SetLong(Sprite_Current + 0x38, static_cast<std::int32_t>(oz));
    }
    SetLong(Sprite_Current + 0x34, static_cast<std::int32_t>(U(Sprite_Current + 0x34) & 0xFFFF8000u));
    SetLong(Sprite_Current + 0x38, static_cast<std::int32_t>(U(Sprite_Current + 0x38) & 0xFFFF8000u));
    c = Sprite_Current;
    const long ground = g.ground_at(Long(c + 0x34), Long(c + 0x38));
    SetWord(Sprite_Current + 0x3E, static_cast<std::uint16_t>(ground));
}

// original 0x51B9D0 (PSX 0x801BE288): the direction from the sprite to
// (x, z) in eight - 0 (-,-), 1 (0,-), 2 (+,-), 3 (+,0), 4 (+,+), 5 (0,+),
// 6 (-,+), 7 (-,0) - written to *direction; nothing written when (x, z) is
// the sprite's own position.
extern "C" void __cdecl Field_DirectionTo(long x, long z, unsigned char* direction) {
    const unsigned char* const c = Sprite_Current;
    const std::int32_t dx = static_cast<std::int32_t>(static_cast<std::uint32_t>(x) - U(c + 0x34));
    const std::int32_t dz = static_cast<std::int32_t>(static_cast<std::uint32_t>(z) - U(c + 0x38));
    if (dx < 0) {
        *direction = dz < 0 ? 0 : dz > 0 ? 6 : 7;
    } else if (dx > 0) {
        *direction = dz < 0 ? 2 : dz > 0 ? 4 : 3;
    } else if (dz > 0) {
        *direction = 5;
    } else if (dz < 0) {
        *direction = 1;
    }
}

// original 0x51BDA0 (PSX 0x801BE930): a member left standing fidgets - each
// frame counts Field_State +0x136 up, and past 0xF0 it is reset and, unless
// either member is already in state 9 (+0x137) or the leader is, one of the
// two members (Rand's bit 0) goes to state 6 with 2 in +9. 1 when it did.
//
// As the original has it: the other member is +5 ^ 3, unchecked; the actor
// record is indexed by Field_State +0x148 directly.
extern "C" unsigned char __cdecl Member_Idle(void) {
    if (ObjTrio[0x137] == 8) return 0;
    if (Field_Request != 0) return 0;
    unsigned char* const state = Field_State;
    if (Field_ActorStates[state[0x148] * kActorBytes] & 0x20) return 0;
    state[0x136] = static_cast<unsigned char>(state[0x136] + 1);
    unsigned char* const s = Field_State;
    if (s[0x136] <= 0xF0) return 0;
    s[0x136] = 0;
    if (ObjTrio[0x137] == 9) return 0;
    if (Member((Sprite_Current[5] & 0xFFu) ^ 3u)[0x137] == 9) return 0;
    const unsigned pick = (static_cast<unsigned>(g.rand()) & 1u) + 1u;
    if (Sprite_Current[5] != pick) return 0;
    Field_State[0x137] = 9;
    g.ensure_animation(Sprite_Current[8]);
    Sprite_Current[9] = 2;
    Sprite_Current[1] = 6;
    Sprite_Current[2] = 0;
    return 1;
}

// --- the cell ahead ----------------------------------------------------------------

// original 0x526DB0 (PSX 0x801BA590): the cell ahead, by 0x527640 for a raised
// sprite (+0x70) and Field_CellAheadFlat otherwise (tail jumps). Also called by
// the leader (0x52E160) and 0x526530 / 0x526C80.
extern "C" unsigned char __cdecl Field_CellAhead(void) {
    if (Sprite_Current[0x70] != 0) return g.cell_ahead_raised();
    return g.cell_ahead_flat();
}

// original 0x526DD0 (PSX: the flat half of 0x801BA590's pair): what the cell
// ahead of a sprite on the ground means for a step - 1 go on (turned along a
// slope or a wall), 3 nothing in the way that this knows, 0 stop, 6 or 7 a
// state change (Field_CellSlope's 2, 4 or 5 on a slope), after turning the
// sprite (+8) where the cells say.
//
// As the original has it: mid-cell on both axes (the 16.16 fractions both
// non-zero) is 1 at once; the classes are kept in cells 8..10 and read back
// from memory where the original reads memory; the half-cell points are the
// original's rounding-toward-zero halvings of 16.16 values that may wrap.
extern "C" unsigned char __cdecl Field_CellAheadFlat(void) {
    if (Field_ScriptFlags & 0x400) return 1;
    const unsigned char* c = Sprite_Current;
    const unsigned char facing = c[8];
    if (Word(c + 0x34) != 0 && Word(c + 0x38) != 0) return 1;
    const unsigned char* const offsets = At(at::kCellOffsets) + facing * 2u;
    const std::uint16_t ahead_z = static_cast<std::uint16_t>(static_cast<signed char>(offsets[1]) + Word(c + 0x3A));
    const std::uint16_t ahead_x = static_cast<std::uint16_t>(static_cast<signed char>(offsets[0]) + Word(c + 0x36));
    g.read_cells(ahead_x, ahead_z);
    c = Sprite_Current;
    const unsigned char dir = c[8];
    if (dir & 1) {
        // Diagonal.
        const std::uint16_t fx = Word(c + 0x34);
        if (fx == 0 && Word(c + 0x38) == 0) {
            const unsigned char k = g.cell_class(1, 0, 0);
            Cell(8) = k;
            if (k == 0xB0) return 6;
            if (k == 0x10) return 0;
            if ((k & 0xF0) != 0xA0) return 3;
            return g.cell_slope(8);
        }
        if (fx != 0 && Word(c + 0x38) == 0) {
            if (dir & 2) return 3;
            const unsigned char k = g.cell_class(2, 4, 0);
            Cell(8) = k;
            if (k == 0xB0) return 6;
            if (k == 0x70) return 7;
            if (k == 0x20) return 0;
            if (k == 0x10) {
                if (g.cell_pair_turn(2, 4, 3, 0xFF)) return 0;
                if (g.cell_pair_turn(2, 4, 3, 0x70)) return 0;
                if (g.cell_pair_turn(2, 4, 7, 0xA0)) return 0;
                if (g.cell_pair_turn(2, 4, 7, 0xA1)) return 0;
                if (g.cell_pair_turn(2, 4, 7, 0xA2)) return 0;
                return g.cell_pair_turn(2, 4, 7, 0xA3) == 0 ? 1 : 0;
            }
            if ((k & 0xF0) == 0xA0) return g.cell_slope(8);
            const unsigned char* const s = Sprite_Current;
            const long ax = Half(static_cast<std::uint32_t>(static_cast<short>(Word(s + 0x36))) * 0x20000u + 0x10000u);
            const long az = static_cast<long>(High(ahead_z));
            const long r = g.object_at(ax, az, 1);
            if (Cell(0) == 0) return 3;
            if (static_cast<short>(r) <= 0x40) return 3;
            Sprite_Current[8] = 7;
            return 1;
        }
        // z mid-cell (or both whole with x whole and z not): the z side.
        if (!(dir & 2)) return 3;
        const unsigned char k = g.cell_class(3, 5, 0);
        Cell(8) = k;
        if (k == 0xB0) return 6;
        if (k == 0x70) return 7;
        if (k == 0x20) return 0;
        if (k == 0x10) {
            if (g.cell_pair_turn(3, 5, 5, 0xFF)) return 0;
            if (g.cell_pair_turn(3, 5, 5, 0x70)) return 0;
            if (g.cell_pair_turn(3, 5, 1, 0xA0)) return 0;
            if (g.cell_pair_turn(3, 5, 1, 0xA1)) return 0;
            if (g.cell_pair_turn(3, 5, 1, 0xA2)) return 0;
            return g.cell_pair_turn(3, 5, 1, 0xA3) != 0 ? 0 : 3;
        }
        if ((k & 0xF0) == 0xA0) return g.cell_slope(8);
        const unsigned char* const s = Sprite_Current;
        const long az = Half(static_cast<std::uint32_t>(static_cast<short>(Word(s + 0x3A))) * 0x20000u + 0x10000u);
        const long ax = static_cast<long>(High(ahead_x));
        const long r = g.object_at(ax, az, 1);
        if (Cell(0) == 0) return 3;
        if (static_cast<short>(r) <= 0x40) return 3;
        Sprite_Current[8] = 1;
        return 1;
    }

    // Straight.
    const std::uint16_t fx = Word(c + 0x34);
    if (fx == 0 && Word(c + 0x38) == 0) {
        // On a cell's corner: ahead, and either side.
        Cell(8) = g.cell_class(1, 2, 3);
        Cell(9) = g.cell_class(2, 0, 0);
        const unsigned char third = g.cell_class(3, 0, 0);
        Cell(10) = third;
        const unsigned char first = Cell(8);
        if (first == 0xB0) return 6;
        if (first == 0x10 || first == 0x20) {
            if (Cell(9) == 0x10) {
                if (third == 0x10) return 0;
                g.turn_unless(2, 4, 3);
            } else if (third == 0x10) {
                g.turn_unless(4, 6, 5);
            } else {
                unsigned char* const s = Sprite_Current;
                s[8] = static_cast<unsigned char>((s[8] + 1) & 7);
            }
        }
        unsigned char* const s = Sprite_Current;
        if (!(Field_InputFlags & 1)) {
            const unsigned char side_x = Cell(9) & 0xF0;
            if (side_x == 0xA0 && (s[8] == 1 || s[8] == 5)) return g.cell_slope(9);
            const unsigned char side_z = Cell(10) & 0xF0;
            if (side_z == 0xA0 && (s[8] == 3 || s[8] == 7)) return g.cell_slope(0xA);
            if (side_x == 0xA0) {
                s[8] = facing;
                g.turn_unless(4, 6, 5);
                return g.cell_slope(9);
            }
            if (side_z == 0xA0) {
                s[8] = facing;
                g.turn_unless(2, 4, 3);
                return g.cell_slope(0xA);
            }
        }
        if (s[8] & 1) return 3;
        {
            const long ax = Half((static_cast<std::uint32_t>(static_cast<short>(Word(s + 0x36))) +
                                  static_cast<std::uint32_t>(static_cast<short>(ahead_x))) << 16);
            const long r = g.object_at(ax, static_cast<long>(High(ahead_z)), 3);
            if (Cell(0) != 0 && static_cast<short>(r) > 0x40) {
                g.turn_unless(4, 6, 5);
                return 1;
            }
        }
        const unsigned char* const t = Sprite_Current;
        if (t[8] & 1) return 3;
        const long az = Half((static_cast<std::uint32_t>(static_cast<short>(Word(t + 0x3A))) +
                              static_cast<std::uint32_t>(static_cast<short>(ahead_z))) << 16);
        const long r = g.object_at(static_cast<long>(High(ahead_x)), az, 1);
        if (Cell(0) == 0) return 3;
        if (static_cast<short>(r) <= 0x40) return 3;
        g.turn_unless(2, 4, 3);
        return 1;
    }
    if (fx != 0 && Word(c + 0x38) == 0) {
        // On an x edge: ahead on x.
        unsigned char k = g.cell_class(2, 4, 0);
        Cell(8) = k;
        if (k == 0x70) {
            g.turn_unless(4, 6, 5);
            return 7;
        }
        if (k == 0x10 || k == 0x20) {
            g.turn_unless(2, 4, 3);
            k = Cell(8);
        }
        if ((k & 0xF0) != 0xA0) return 3;
        if (!(Sprite_Current[8] & 1)) g.turn_unless(4, 6, 5);
        return g.cell_slope(8);
    }
    // On a z edge: ahead on z.
    unsigned char k = g.cell_class(3, 5, 0);
    Cell(8) = k;
    if (k == 0x70) {
        g.turn_unless(2, 4, 3);
        return 7;
    }
    if (k == 0xB0) return 6;
    if (k == 0x10 || k == 0x20) {
        g.turn_unless(4, 6, 5);
        k = Cell(8);
    }
    if ((k & 0xF0) != 0xA0) return 3;
    if (!(Sprite_Current[8] & 1)) g.turn_unless(2, 4, 3);
    return g.cell_slope(8);
}

// original 0x527470 (PSX 0x801BA5D8's helper): the class of up to three cells
// (indices a, b, c into the cells, the list ending at the first 0) - 0xB0 all
// 0xB0 (or no cell), 0x10 one of them 0x70 (with two, both 0x70 is 0x70) or
// 0xFF, a slope 0xA_ by the pairs' rules, 0x20 two different 0x2_, else 0.
//
// As the original has it: a list of one takes a 0xA_ cell as it is; with
// three, only the first two positions start the 0x2_ search and the search
// ends after the first 0x2_ found; the facing's bit 0 is read once.
extern "C" unsigned char __cdecl Field_CellClass(unsigned a, unsigned b, unsigned c) {
    unsigned char k[3] = {static_cast<unsigned char>(a), static_cast<unsigned char>(b), static_cast<unsigned char>(c)};
    int n = 0;
    for (int i = 0; i < 3 && k[i] != 0; ++i) ++n;
    for (int i = 0; i < n; ++i) k[i] = Cell(k[i]);
    unsigned char first = k[0], second = k[1];
    if (n == 2 && first == 0x70 && second == first) return 0x70;
    for (int i = 0; i < n; ++i)
        if (k[i] == 0x70) {
            k[i] = 0xFF;
            second = k[1];
            first = k[0];
        }
    for (int i = 0; i < n; ++i)
        if (k[i] == 0xFF) return 0x10;
    {
        int i = 0;
        while (i < n && k[i] == 0xB0) ++i;
        if (i == n) return 0xB0;
    }
    if (n & 1) {
        const unsigned char odd = Sprite_Current[8] & 1;
        for (int i = 0; i < n; ++i) {
            if (!odd && k[i] == 0xA2) return 0x10;
            if (k[i] == 0xA3) return 0x10;
            if (k[i] == 0xA0) return 0x10;
        }
        if (n == 1) return (first & 0xF0) == 0xA0 ? first : 0;
    } else if ((first & 0xF0) == 0xA0 || (second & 0xF0) == 0xA0) {
        const unsigned char high = first & 0xF0;
        if (first == 0xA2) {
            if (second == first) return (Sprite_Current[8] & 1) ? 0xA2 : 0x10;
        } else if (first == 0xA3) {
            if (second == first) return 0xA3;
        }
        if (high != 0xA0) return 0x10;
        return (second & 0xF0) == high ? 0xA1 : 0x10;
    } else if (n <= 1) {
        return 0;
    }
    for (int i = 0; i < n - 1; ++i) {
        if ((k[i] & 0xF0) != 0x20) continue;
        for (int j = 0; j < n; ++j)
            if ((k[j] & 0xF0) == 0x20 && k[i] != k[j]) return 0x20;
        i = n;
    }
    return 0;
}

// original 0x528070: a slope cell's answer by its low nibble - 2 for 1 or 0xF,
// 5 for 2, else 4.
extern "C" unsigned char __cdecl Field_CellSlope(unsigned cell) {
    const unsigned low = Cell(cell) & 0xF;
    if (low == 1 || low == 0xF) return 2;
    if (low == 2) return 5;
    return 4;
}

// original 0x5280F0: a straight-facing sprite turned to `to` when it faces a
// or b, else to the opposite of `to`; a diagonal facing is left alone.
extern "C" void __cdecl Field_TurnUnless(unsigned a, unsigned b, unsigned to) {
    unsigned char* const c = Sprite_Current;
    const unsigned char facing = c[8];
    if (facing & 1) return;
    if (facing == static_cast<unsigned char>(a) || facing == static_cast<unsigned char>(b))
        c[8] = static_cast<unsigned char>(to);
    else
        c[8] = static_cast<unsigned char>(to ^ 4);
}

// original 0x528120: 1 when cells a and b both hold `value`; when only a
// does, the sprite turned to `to`, when only b does, to its opposite; 0.
extern "C" unsigned char __cdecl Field_CellPairTurn(unsigned a, unsigned b, unsigned to, unsigned value) {
    const unsigned char v = static_cast<unsigned char>(value);
    if (Cell(a) == v) {
        if (Cell(b) == v) return 1;
        Sprite_Current[8] = static_cast<unsigned char>(to);
        return 0;
    }
    if (Cell(b) == v) Sprite_Current[8] = static_cast<unsigned char>(to ^ 4);
    return 0;
}

// original 0x5282C0: cells 1..5 - the kind of the cell (x, z) ahead, then of
// the two cells beside the way there, then of the two one further on - each
// as Field_CellKind of it from the sprite's own cell.
extern "C" void __cdecl Field_ReadCells(unsigned x, unsigned z) {
    const unsigned char* c = Sprite_Current;
    Cell(1) = g.cell_kind(x, z, Word(c + 0x36), Word(c + 0x3A));
    c = Sprite_Current;
    Cell(2) = g.cell_kind(Word(c + 0x36), z, Word(c + 0x36), Word(c + 0x3A));
    c = Sprite_Current;
    Cell(3) = g.cell_kind(x, Word(c + 0x3A), Word(c + 0x36), Word(c + 0x3A));
    c = Sprite_Current;
    {
        const unsigned x1 = static_cast<std::uint16_t>(Word(c + 0x36) + 1);
        Cell(4) = g.cell_kind(x1, z, x1, Word(c + 0x3A));
    }
    c = Sprite_Current;
    {
        const unsigned z1 = static_cast<std::uint16_t>(Word(c + 0x3A) + 1);
        Cell(5) = g.cell_kind(x, z1, Word(c + 0x36), z1);
    }
}

// original 0x528370: the kind of the map cell (x, z) seen from (x0, z0) -
// the map byte, with 0xFF for a wall, 0xB0 a way out the sprite faces, 0 or
// 0xA1 some slopes and steps - by the byte's high nibble, the byte of the
// cell stepped from, and Field_ObjectAhead at the point between the two (or
// at the cell, by the sprite's fractions and +0x70).
//
// As the original has it: the arguments are 16-bit; the midpoint is a 16.16
// sum halved toward zero; 0x80 is a wall only for a member (+5 non-zero);
// the byte 0x11 asks the object at 0x905DA0 its +0xB bit 0 when that is the
// sprite, Field_State +0x138 bit 0 otherwise.
extern "C" unsigned char __cdecl Field_CellKind(unsigned x, unsigned z, unsigned x0, unsigned z0) {
    const std::uint32_t X1 = High(x), Z1 = High(z), X0 = High(x0), Z0 = High(z0);
    const unsigned char* const c = Sprite_Current;
    const bool whole_x = Word(c + 0x34) == 0, whole_z = Word(c + 0x38) == 0, ground = c[0x70] == 0;
    const long px = whole_x == ground ? Half(X1 + X0) : static_cast<long>(X1);
    const long pz = whole_z == ground ? Half(Z1 + Z0) : static_cast<long>(Z1);
    const unsigned char to = g.byte_at(static_cast<short>(x), static_cast<short>(z));
    const unsigned char from = g.byte_at(static_cast<short>(x0), static_cast<short>(z0));
    switch (to & 0xF0) {
    case 0x80:
        if (Sprite_Current[5] != 0) return 0xFF;
        [[fallthrough]];
    case 0x00:
    case 0x60:
    case 0x90:
    case 0xB0:
    case 0xC0:
    case 0xD0:
    case 0xE0:
        if (g.cell_facing(from)) return 0xB0;
        if (g.object_ahead(px, pz)) return 0xFF;
        return to;
    case 0x10:
        if (to == 0x11) {
            const unsigned char* const s = Sprite_Current;
            if (Address(s) == at::kObject905DA0) return (s[0xB] & 1) ? 0 : 0xFF;
            return (Field_State[0x138] & 1) ? 0 : 0xFF;
        }
        [[fallthrough]];
    case 0x30:
    case 0x40:
    case 0x50:
        if (to != 0x52) return 0xFF;
        if (g.object_ahead(px, pz)) return 0xFF;
        return 0;
    case 0x70:
        if (from == 0x70 && g.object_ahead(px, pz)) return from;
        return g.object_ahead(px, pz) ? 0xFF : 0;
    case 0x20:
        if (g.object_ahead(px, pz)) return 0xFF;
        if ((from & 0xF0) == 0x20 && ((from ^ to) & 0xF1) != 0) return 0xFF;
        return to & 0xF1;
    case 0xA0:
        if (from == 0xC0) {
            if (Field_InputFlags & 1) {
                if (to == 0xA0) return 0;
                if (to == 0xAE) return 0;
            }
            if (to == 0xAF) return 0xA1;
            if (Sprite_Current[5] != 0) return 0xFF;
            return to;
        }
        if (to == 0xA2 || to == 0xA3) return 0xFF;
        return g.object_ahead(px, pz) ? 0xFF : 0;
    default:   // 0xF0
        return 0xFF;
    }
}

// original 0x528730: 1 when the cell byte is a way out (0xB_) the sprite
// faces - low nibble 0..5 its facing, 8 facing 3 or 5.
extern "C" unsigned char __cdecl Field_CellFacing(unsigned cell) {
    const unsigned char v = static_cast<unsigned char>(cell);
    if ((v & 0xF0) != 0xB0) return 0;
    const unsigned char low = v & 0xF;
    if (low < 6) return Sprite_Current[8] == low ? 1 : 0;
    if (low == 8) return (Sprite_Current[8] == 3 || Sprite_Current[8] == 5) ? 1 : 0;
    return 0;
}

// original 0x528770: 1 when 0x5725C0 finds an object at (x, z) in the
// sprite's facing that stands more than 0x40 high.
extern "C" unsigned char __cdecl Field_ObjectAhead(long x, long z) {
    const long r = g.object_at(x, z, Sprite_Current[8]);
    if (Cell(0) == 0) return 0;
    return static_cast<short>(r) > 0x40 ? 1 : 0;
}

void MemberSprites_Inject() {
    if (bof3::WantsShadow("member_sprites")) member_sprites::SelfTest();
    BOF3_INJECT(Field_MemberFrame);
    BOF3_INJECT(Member_Start);
    BOF3_INJECT(Member_Control);
    BOF3_INJECT(Member_Follow);
    BOF3_INJECT(Member_Walk);
    BOF3_INJECT(Member_WalkEnd);
    BOF3_INJECT(Member_FollowStep);
    BOF3_INJECT(Member_StepAhead);
    BOF3_INJECT(Member_CatchUp);
    BOF3_INJECT(Field_DirectionTo);
    BOF3_INJECT(Member_Idle);
    BOF3_INJECT(Field_CellAhead);
    BOF3_INJECT(Field_CellAheadFlat);
    BOF3_INJECT(Field_CellClass);
    BOF3_INJECT(Field_CellSlope);
    BOF3_INJECT(Field_TurnUnless);
    BOF3_INJECT(Field_CellPairTurn);
    BOF3_INJECT(Field_ReadCells);
    BOF3_INJECT(Field_CellKind);
    BOF3_INJECT(Field_CellFacing);
    BOF3_INJECT(Field_ObjectAhead);
}

// Moved here by tools/move_funcs.py from field_hidden.cpp, member_sprites.cpp, rest_1a.cpp (docs/source-layout-round.md).
#include "game/field/member.h"

#include <cstddef>
#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/field/member_callees.h"
#include "game/field_hidden.h"
#include "game/field_hidden_callees.h"
#include "game/member_sprites.h"
#include "game/member_sprites_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_0a.h"
#include "game/rest_1a.h"
#include "game/rest_1a_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = member::at;

}  // namespace

using namespace field_hidden;

namespace {

using namespace move_script;

}  // namespace

namespace {

unsigned char Al(std::uint32_t eax) { return static_cast<unsigned char>(eax); }

// An effect object by its index: Effect_Objects + index * 0x80, unchecked.
unsigned char* Effect(unsigned index) { return rv::Bytes(Effect_Objects) + (index << 7); }

}  // namespace

#define FH_EXPORT extern "C" __attribute__((disable_tail_calls))

namespace {

using member_sprites::g;

using member_sprites::Handler;

constexpr std::uint32_t kMemberBytes = 0x14C, kActorBytes = 0xA4;

std::uint32_t Address(const volatile void* p) { return Low32(p); }

// Member i by address, i unchecked (the original indexes past the three).
unsigned char* Member(unsigned i) { return move_script::At(Address(rv::Bytes(ObjTrio)) + i * kMemberBytes); }

// The member records' views (game/record_views.h): Sprite_Current
// and Field_State are ObjTrio records here.
unsigned char& Cell(unsigned i) { return move_script::At(::bof3::Img(member_sprites::at::kCells.v))[i & 0xFF]; }

std::uint32_t U(const unsigned char* at) { return static_cast<std::uint32_t>(move_script::Long(at)); }

void Dispatch(const std::uint32_t* table, unsigned index) {
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(table[index]))();
}

// `cdq; xor eax, edx; sub eax, edx`: INT_MIN stays INT_MIN.
std::int32_t Abs(std::uint32_t v) {
    const std::uint32_t s = static_cast<std::uint32_t>(static_cast<std::int32_t>(v) >> 31);
    return static_cast<std::int32_t>((v ^ s) - s);
}

// The two longs of a direction's row, index unchecked.
std::uint32_t RowX(std::uint32_t table, unsigned dir) { return U(move_script::At(table + dir * 8)); }

std::uint32_t RowZ(std::uint32_t table, unsigned dir) { return U(move_script::At(table + dir * 8 + 4)); }

// The low bytes of the two flag words, ORed (`mov al, [0x905BA4]; or al, [0x9039A2]`).
unsigned FlagsLow() { return (Field_ScriptFlags2 | Field_ScriptFlags) & 0xFF; }

// Member_CatchUp's free test at the sprite's position.
unsigned char BlockedHere() {
    const records::PartyMember* c = rv::Current<records::PartyMember>();
    const long ground = member_sprites::g.ground_at(c->x, c->z);
    c = rv::Current<records::PartyMember>();
    return member_sprites::g.blocked_at(c->x, c->z, rv::Bytes(c)[0x70], ground);
}

using URest1a = std::uint32_t;

using move_script::At;

using move_script::Long;

std::int32_t L(URest1a a) { return move_script::Long(move_script::At(a)); }

unsigned char* Sc() { return Sprite_Current; }

unsigned char* Fs() { return Field_State; }

// ObjTrio's record i (stride 0x14C), the index unchecked as the original's.
const unsigned char* MemberRest1a(unsigned i) { return rv::Bytes(ObjTrio) + i * rest_1a::at::kMemberStride; }

// `cdq; xor eax, edx; sub eax, edx`: the absolute value with the original's
// wrap (the most negative stays negative).
std::int32_t Abs32(std::int32_t v) {
    const auto u = static_cast<URest1a>(v);
    const URest1a s = static_cast<URest1a>(v >> 31);
    return static_cast<std::int32_t>((u ^ s) - s);
}

// A .data dispatch table read in place. The original's index is unchecked:
// past the table it jumps through the next table's cell, or into data. Ours
// aborts at the table's own count (its symbols.toml count), before the read -
// the rule all seven of wave one's groups share (docs/rest_1b.md section 6).
// While the fuzz runs, the entries are its recorders.
using HandlerRest1a = void (__cdecl*)();

HandlerRest1a CodeAt(URest1a table, unsigned count, unsigned index, const char* who) {
    const URest1a cell = table + 4u * index;
    if (index >= count)
        bof3::Fatal("%s: index %u is past its table's %u entries at 0x%X; the original jumps through 0x%X", who, index,
                    count, static_cast<unsigned>(::bof3::VaOf(table)), static_cast<unsigned>(::bof3::VaOf(cell)));
    return reinterpret_cast<HandlerRest1a>(static_cast<std::uintptr_t>(static_cast<URest1a>(L(cell))));
}

}  // namespace

// original 0x51AC50: a party member's frame (PSX 0x801BCE40), called by
// Field_MembersFrame for members 1 and 2 and by Field_PartyFirstFrame.
//
// As the original has it: the state is Sprite_Current +1, a whole byte and
// unchecked - the table at 0x65F960 has 9 entries, then zeros. The original
// jumps (a tail call).
extern "C" void __cdecl Field_MemberFrame(void) {
    member_sprites::g.restore_clut();
    Dispatch(member_sprites::g.member_states, rv::Current<records::PartyMember>()->state);
}

// original 0x51AC70, state 0 (PSX 0x801BCE90): the member's counters cleared,
// +6 its index less one, grounded, walking speed 3, its animation, state 1.
extern "C" void __cdecl Member_Start(void) {
    member_sprites::g.clear_steps();
    rv::Store<std::int32_t>(rv::Current<records::PartyMember>()->work_18, 0);
    rv::Store<std::int32_t>(rv::Current<records::PartyMember>()->work_1c, 0);
    rv::Store<std::int32_t>(rv::Current<records::PartyMember>()->work_20, 0);
    Sprite_Current[6] = static_cast<unsigned char>(rv::Current<records::PartyMember>()->actor - 1);
    Sprite_Current[7] = 0;
    {
        const records::PartyMember* const c = rv::Current<records::PartyMember>();
        const long z = c->z, x = c->x;
        const long ground = member_sprites::g.ground_at(x, z);
        rv::Store<unsigned>(rv::Current<records::PartyMember>()->height_whole, static_cast<std::uint16_t>(ground));
    }
    Sprite_Current[0x5D] = 0;
    Sprite_Current[0x5E] = 0;
    Sprite_Current[0x5F] = 0;
    Sprite_Current[0x5C] = 2;
    Field_State[0x128] = 3;
    Field_State[0x137] = 0;
    Field_State[0x136] = 0;
    member_sprites::g.set_animation(rv::Current<records::PartyMember>()->direction);
    rv::Current<records::PartyMember>()->state = static_cast<unsigned char>(rv::Current<records::PartyMember>()->state + 1);
    rv::Current<records::PartyMember>()->step = 0;
}

// original 0x51AD40, state 1 (PSX 0x801BCFAC): its sub-state through the
// two-entry table 0x65F99C, then Sprite_ScriptTick.
//
// As the original has it: the sub-state Sprite_Current +2 is unchecked; the
// tick is a tail jump whose al no caller reads.
extern "C" void __cdecl Member_Control(void) {
    Dispatch(member_sprites::g.control_states, rv::Current<records::PartyMember>()->step);
    member_sprites::g.script_tick();
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
        rv::Current<records::PartyMember>()->state = 0xE;
        rv::Current<records::PartyMember>()->step = 0;
        return;
    }
    const unsigned flags = Field_ScriptFlags2;
    if (flags & 0x1000) {
        rv::Current<records::PartyMember>()->state = 5;
        rv::Current<records::PartyMember>()->step = 0;
        Sprite_Current[0xB] = 0;
        return;
    }
    if (flags & 0xC00) return;
    if (member_sprites::g.test_535120(1)) return;
    if (member_sprites::g.test_535240(1)) return;
    if (ObjTrio[0].state == 0xA) {
        member_sprites::g.ensure_animation(rv::Current<records::PartyMember>()->direction);
        rv::Current<records::PartyMember>()->state = 7;
        rv::Current<records::PartyMember>()->step = 0;
        return;
    }
    if (member_sprites::g.idle()) return;
    switch (member_sprites::g.follow_step()) {
    case 0: {
        if (member_sprites::g.test_531df0()) {
            member_sprites::g.ensure_animation(rv::Current<records::PartyMember>()->direction);
            rv::Current<records::PartyMember>()->step = 0;
            return;
        }
        records::PartyMember* const state = rv::As<records::PartyMember>(Field_State);
        if (!(Field_ActorStates[state->char_index * kActorBytes] & 0x20)) rv::Bytes(state)[0x136] = 0;
        member_sprites::g.call_534610();
        member_sprites::g.call_535f50();
        member_sprites::g.ensure_animation(static_cast<unsigned char>(rv::Current<records::PartyMember>()->direction + 8));
        if (rv::As<records::PartyMember>(Field_State)->char_id == 2) {
            const records::PartyMember* const c = rv::Current<records::PartyMember>();
            const std::uint32_t steps = c->step_frames;
            const std::uint32_t x = static_cast<std::uint32_t>(c->step_x) * steps + static_cast<std::uint32_t>(c->x);
            const std::uint32_t z = static_cast<std::uint32_t>(c->step_z) * steps + static_cast<std::uint32_t>(c->z);
            member_sprites::g.ground_at(static_cast<long>(x), static_cast<long>(z));
        }
        rv::Current<records::PartyMember>()->step_frames = static_cast<unsigned char>(rv::Current<records::PartyMember>()->step_frames - 1);
        member_sprites::g.call_52e140();
        Field_State[0x137] = 1;
        rv::Current<records::PartyMember>()->step = 1;
        return;
    }
    case 1:
    case 2:
    case 3:
    case 4:
        member_sprites::g.clear_steps();
        member_sprites::g.ensure_animation(rv::Current<records::PartyMember>()->direction);
        Field_State[0x137] = 0;
        rv::Current<records::PartyMember>()->step = 0;
        return;
    case 5:
        Field_State[0x128] = 3;
        member_sprites::g.ensure_animation(rv::Current<records::PartyMember>()->direction);
        Field_State[0x137] = 4;
        rv::Current<records::PartyMember>()->state = 2;
        rv::Current<records::PartyMember>()->step = 3;
        return;
    case 6: {
        Field_State[0x128] = 3;
        member_sprites::g.ensure_animation(rv::Current<records::PartyMember>()->direction);
        Field_State[0x137] = 3;
        rv::Current<records::PartyMember>()->state = 8;
        records::PartyMember* const c = rv::Current<records::PartyMember>();
        c->step = (c->direction == 1 || c->direction == 7) ? 0 : 1;
        return;
    }
    default:
        return;
    }
}

// original 0x51AFC0, sub-state 1 of state 1 (PSX 0x801BD3B0): a step's
// frames counted down in +9 through 0x52E140 (tail jumps both).
extern "C" void __cdecl Member_Walk(void) {
    records::PartyMember* const c = rv::Current<records::PartyMember>();
    const unsigned char left = c->step_frames;
    if (left == 0) {
        member_sprites::g.walk_end();
        return;
    }
    c->step_frames = static_cast<unsigned char>(left - 1);
    member_sprites::g.call_52e140();
}

// original 0x51AFE0 (PSX 0x801BD3F8): the step's end - four event checks, two
// more tests that may take the frame, then stopped and Member_Follow asked
// again; standing still in state 1 afterwards, back to sub-state 0.
extern "C" void __cdecl Member_WalkEnd(void) {
    member_sprites::g.call_535270();
    member_sprites::g.call_5350c0();
    member_sprites::g.call_534f10();
    member_sprites::g.call_534a00();
    if (member_sprites::g.test_535120(0)) return;
    if (member_sprites::g.test_535240(0)) return;
    if (member_sprites::g.test_534920()) return;
    member_sprites::g.clear_steps();
    Field_State[0x128] = 3;
    member_sprites::g.follow();
    records::PartyMember* const c = rv::Current<records::PartyMember>();
    if (c->step_x != 0) return;
    if (c->step_z != 0) return;
    if (c->state != 1) return;
    c->step = 0;
}

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
    records::PartyMember* c = rv::Current<records::PartyMember>();
    const records::PartyMember* const leader = rv::AsConst<records::PartyMember>(Member(0));
    const unsigned char facing = c->direction;
    const std::uint32_t leader_steps = leader->step_frames;
    const std::uint32_t tx = leader_steps * static_cast<std::uint32_t>(leader->step_x) + static_cast<std::uint32_t>(leader->x);
    const std::uint32_t tz = leader_steps * static_cast<std::uint32_t>(leader->step_z) + static_cast<std::uint32_t>(leader->z);
    const unsigned char who = c->actor;
    if (Member((who & 0xFFu) ^ 3u)[0x137] != 9) {
        if (who == 2) {
            rv::Bytes(c)[6] = Member(1)[6] != 0 ? 0 : 1;
        } else if (Field_MemberCount <= 2) {
            rv::Bytes(c)[6] = 0;
        } else {
            const records::PartyMember* const third = rv::AsConst<records::PartyMember>(Member(2));
            const std::uint32_t steps = third->step_frames;
            const std::int32_t mine = static_cast<std::int32_t>(static_cast<std::uint32_t>(Abs(static_cast<std::uint32_t>(c->z) - tz)) +
                                                                static_cast<std::uint32_t>(Abs(static_cast<std::uint32_t>(c->x) - tx)));
            const std::int32_t theirs = static_cast<std::int32_t>(
                static_cast<std::uint32_t>(Abs(steps * static_cast<std::uint32_t>(third->step_x) - tx + static_cast<std::uint32_t>(third->x))) +
                static_cast<std::uint32_t>(Abs(steps * static_cast<std::uint32_t>(third->step_z) - tz + static_cast<std::uint32_t>(third->z))));
            rv::Bytes(c)[6] = mine > theirs ? 2 : 0;
        }
        c = rv::Current<records::PartyMember>();
    }
    // The one followed: where its step ends, and two half-cells behind that.
    const records::PartyMember* const target = rv::AsConst<records::PartyMember>(Member(rv::Bytes(c)[6]));
    const std::uint32_t target_steps = target->step_frames;
    const std::uint32_t gx = static_cast<std::uint32_t>(target->step_x) * target_steps + static_cast<std::uint32_t>(target->x);
    const std::uint32_t gz = static_cast<std::uint32_t>(target->step_z) * target_steps + static_cast<std::uint32_t>(target->z);
    const unsigned dir = target->direction;
    const std::int32_t near = rv::Bytes(target)[0x70] ? 0x20000 : 0x18000;
    const std::uint32_t px = gx - (RowX(::bof3::Img(Field_DirectionSteps_addr), dir) << 1);
    const std::uint32_t pz = gz - (RowZ(::bof3::Img(Field_DirectionSteps_addr), dir) << 1);
    if (Abs(static_cast<std::uint32_t>(c->x) - gx) <= near && Abs(static_cast<std::uint32_t>(c->z) - gz) <= near) return 1;

    member_sprites::g.catch_up(static_cast<long>(px), static_cast<long>(pz));
    {
        const records::PartyMember* const s = rv::Current<records::PartyMember>();
        const std::int32_t dx = Abs(static_cast<std::uint32_t>(s->x) - px);
        std::int32_t dz = 0;
        if (dx > 0x60000 || (dz = Abs(static_cast<std::uint32_t>(s->z) - pz)) > 0x60000) {
            Field_State[0x128] = 5;
        } else if (dx <= 0x20000 && dz <= 0x20000) {
            const unsigned char speed = Member(rv::Bytes(s)[6])[0x128];
            Field_State[0x128] = speed;
        } else {
            const unsigned char speed = static_cast<unsigned char>(Member(rv::Bytes(s)[6])[0x128] + 1);
            Field_State[0x128] = speed;
        }
    }
    {
        unsigned char* const state = Field_State;
        const unsigned char speed = state[0x128];
        state[0x128] = speed < 3 ? 3 : speed > 5 ? 5 : speed;
    }
    member_sprites::g.direction_to(static_cast<long>(px), static_cast<long>(pz), Sprite_Current + 8);
    unsigned char answer = member_sprites::g.step_ahead();
    if (answer != 1) {
        c = rv::Current<records::PartyMember>();
        const std::int32_t mine = static_cast<std::int32_t>(static_cast<std::uint32_t>(Abs(static_cast<std::uint32_t>(c->z) - pz)) +
                                                            static_cast<std::uint32_t>(Abs(static_cast<std::uint32_t>(c->x) - px)));
        const std::int32_t aim = static_cast<std::int32_t>(static_cast<std::uint32_t>(Abs(Scratch_Swap - pz)) +
                                                           static_cast<std::uint32_t>(Abs(static_cast<std::uint32_t>(member_sprites::at::kAimX) - px)));
        if (mine >= aim) return answer;
        c->direction = facing;
        answer = 1;
    }
    if (FlagsLow() & 1) return answer;
    if (!(rv::Current<records::PartyMember>()->flags & 0x80)) return answer;

    // The leader's next position, to the half-cell: free for this member?
    const std::uint32_t nz = (leader->step_frames * static_cast<std::uint32_t>(leader->step_z) + static_cast<std::uint32_t>(leader->z)) & 0xFFFF8000u;
    const std::uint32_t nx = (leader->step_frames * static_cast<std::uint32_t>(leader->step_x) + static_cast<std::uint32_t>(leader->x)) & 0xFFFF8000u;
    unsigned char* const saved = Field_State;
    Field_State = Member(0);
    const long ground = member_sprites::g.ground_at(static_cast<long>(nx), static_cast<long>(nz));
    const unsigned char blocked = member_sprites::g.blocked_at(static_cast<long>(nx), static_cast<long>(nz), Sprite_Current[0x70], ground);
    Field_State = saved;
    if (blocked) return answer;
    rv::Current<records::PartyMember>()->x = static_cast<std::int32_t>(nx);
    rv::Current<records::PartyMember>()->z = static_cast<std::int32_t>(nz);
    rv::Current<records::PartyMember>()->flags = static_cast<unsigned char>(rv::Current<records::PartyMember>()->flags | 0x40);
    Field_State[0x138] = static_cast<unsigned char>(Field_State[0x138] & 0xFE);
    Field_State[0x138] = static_cast<unsigned char>(Field_State[0x138] | (Member(0)[0x138] & 1));
    const long here = member_sprites::g.ground_at(static_cast<long>(nx), static_cast<long>(nz));
    rv::Store<unsigned>(rv::Current<records::PartyMember>()->height_whole, static_cast<std::uint16_t>(here));
    rv::Current<records::PartyMember>()->ot_slot = Member(0)[0x29];
    rv::Current<records::PartyMember>()->state = 2;
    rv::Current<records::PartyMember>()->step = 2;
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
    const unsigned char facing = rv::Current<records::PartyMember>()->direction;
    const unsigned char kind = member_sprites::g.cell_ahead();
    {
        records::PartyMember* const c = rv::Current<records::PartyMember>();
        c->step_frames = (c->direction == 2 || c->direction == 6) ? 0x20 : 0x10;
    }
    const unsigned speed = Field_MoveSpeeds[Field_State[0x128]];
    {
        records::PartyMember* const c = rv::Current<records::PartyMember>();
        const volatile int divisor = static_cast<int>(speed);
        c->step_frames = static_cast<unsigned char>(static_cast<int>(c->step_frames) / divisor);
    }
    records::PartyMember* const c = rv::Current<records::PartyMember>();
    const std::uint32_t frames = c->step_frames;
    std::uint32_t ax, az;
    if (rv::Bytes(c)[0x70] == 0) {
        ax = RowX(::bof3::Img(MoveCmd_F9Steps_addr), c->direction) * frames * speed + static_cast<std::uint32_t>(c->x);
        az = RowZ(::bof3::Img(MoveCmd_F9Steps_addr), c->direction) * frames * speed + static_cast<std::uint32_t>(c->z);
    } else {
        ax = static_cast<std::uint32_t>(c->x) + ((RowX(::bof3::Img(MoveCmd_F9Steps_addr), c->direction) * frames * speed) << 1);
        az = static_cast<std::uint32_t>(c->z) + ((RowZ(::bof3::Img(MoveCmd_F9Steps_addr), c->direction) * frames * speed) << 1);
    }
    member_sprites::at::kAimX = static_cast<std::int32_t>(ax);
    Scratch_Swap = static_cast<std::int32_t>(az);
    switch (kind) {
    case 0:
    case 2:
    case 4:
    case 5:
        return 1;
    case 3: {
        const long r = member_sprites::g.object_at(static_cast<long>(ax), static_cast<long>(az), c->direction);
        if (Cell(0) == 0) return 0;
        if (static_cast<short>(r) > 0x40) {
            rv::Current<records::PartyMember>()->direction = facing;
            return 1;
        }
        member_sprites::g.ground_at(member_sprites::at::kAimX, static_cast<std::int32_t>(Scratch_Swap));
        return 0;
    }
    case 6: return 5;
    case 7: return 6;
    default: return 0;
    }
}

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
    const records::PartyMember* const c0 = rv::Current<records::PartyMember>();
    if (!(c0->flags & 0x80)) return;
    if (FlagsLow() & 1) return;
    if (Member(0)[0x138] & 1) return;
    const std::uint32_t ox = static_cast<std::uint32_t>(c0->x), oz = static_cast<std::uint32_t>(c0->z);
    std::int32_t far = Abs(ox - x);
    const std::int32_t far_z = Abs(oz - z);
    if (far < far_z) far = far_z;
    std::int32_t tries = far >= 0x100000 ? (static_cast<std::int32_t>(static_cast<std::uint32_t>(far) - 0xF0000u) >> 16) + 1 : 1;
    const short here = static_cast<short>(member_sprites::g.ground_at(static_cast<long>(ox), static_cast<long>(oz)));
    const short there = static_cast<short>(member_sprites::g.ground_at(x_arg, z_arg));
    tries -= Abs(static_cast<std::uint32_t>(static_cast<std::int32_t>(here) - static_cast<std::int32_t>(there))) >> 8;
    if (tries <= 0) tries = 1;

    std::int32_t left = tries;   // the tries still to make, the k of this one
    std::int32_t side = -1;      // how far the side tries reach, one more each time
    unsigned char dir = 0;       // the original's stack byte (see above)
    bool found = false;
    for (;;) {
        member_sprites::g.direction_to(x_arg, z_arg, &dir);
        rv::Store<std::int32_t>(rv::Current<records::PartyMember>()->x, static_cast<std::int32_t>(((RowX(::bof3::Img(Field_DirectionSteps_addr), dir) << 1) * static_cast<std::uint32_t>(left)) + ox));
        rv::Store<std::int32_t>(rv::Current<records::PartyMember>()->z, static_cast<std::int32_t>(((RowZ(::bof3::Img(Field_DirectionSteps_addr), dir) << 1) * static_cast<std::uint32_t>(left)) + oz));
        {
            const records::PartyMember* c = rv::Current<records::PartyMember>();
            const long object = member_sprites::g.object_at(c->x, c->z, dir);
            c = rv::Current<records::PartyMember>();
            if (!member_sprites::g.blocked_at(c->x, c->z, rv::Bytes(c)[0x70], object)) {
                found = true;
                break;
            }
        }
        // Either side on x.
        const std::uint32_t tried_x = static_cast<std::uint32_t>(rv::Current<records::PartyMember>()->x);
        if (side > 1) {
            std::int32_t i = 1;
            std::uint32_t plus = tried_x + 0x10000, minus = tried_x - 0x10000;
            bool free = false;
            for (;;) {
                rv::Current<records::PartyMember>()->x = static_cast<std::int32_t>(plus);
                if (!BlockedHere()) { free = true; break; }
                rv::Current<records::PartyMember>()->x = static_cast<std::int32_t>(minus);
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
        rv::Current<records::PartyMember>()->x = static_cast<std::int32_t>(tried_x);
        // Either side on z.
        const std::uint32_t tried_z = static_cast<std::uint32_t>(rv::Current<records::PartyMember>()->z);
        if (side > 1) {
            std::int32_t i = 1;
            std::uint32_t plus = tried_z + 0x10000, minus = tried_z - 0x10000;
            bool free = false;
            for (;;) {
                rv::Current<records::PartyMember>()->z = static_cast<std::int32_t>(plus);
                if (!BlockedHere()) { free = true; break; }
                rv::Current<records::PartyMember>()->z = static_cast<std::int32_t>(minus);
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

    records::PartyMember* c = rv::Current<records::PartyMember>();
    if (!found) {
        c->x = static_cast<std::int32_t>(ox);
        rv::Current<records::PartyMember>()->z = static_cast<std::int32_t>(oz);
    } else if (Abs(static_cast<std::uint32_t>(c->x) - U(Member(0) + 0x34)) <= 0x60000 && Abs(static_cast<std::uint32_t>(c->z) - U(Member(0) + 0x38)) <= 0x60000) {
        c->x = static_cast<std::int32_t>(ox);
        rv::Current<records::PartyMember>()->z = static_cast<std::int32_t>(oz);
    } else if (member_sprites::g.map_cell(c->x_whole, c->z_whole)) {
        Field_State[0x138] = static_cast<unsigned char>(Field_State[0x138] & 0xFE);
        rv::Current<records::PartyMember>()->ot_slot = Draw_OtSlot;
        rv::Current<records::PartyMember>()->direction = dir;
    } else {
        rv::Current<records::PartyMember>()->x = static_cast<std::int32_t>(ox);
        rv::Current<records::PartyMember>()->z = static_cast<std::int32_t>(oz);
    }
    rv::Current<records::PartyMember>()->x = static_cast<std::int32_t>(static_cast<std::uint32_t>(rv::Current<records::PartyMember>()->x) & 0xFFFF8000u);
    rv::Current<records::PartyMember>()->z = static_cast<std::int32_t>(static_cast<std::uint32_t>(rv::Current<records::PartyMember>()->z) & 0xFFFF8000u);
    c = rv::Current<records::PartyMember>();
    const long ground = member_sprites::g.ground_at(c->x, c->z);
    rv::Store<unsigned>(rv::Current<records::PartyMember>()->height_whole, static_cast<std::uint16_t>(ground));
}

// original 0x51B9D0 (PSX 0x801BE288): the direction from the sprite to
// (x, z) in eight - 0 (-,-), 1 (0,-), 2 (+,-), 3 (+,0), 4 (+,+), 5 (0,+),
// 6 (-,+), 7 (-,0) - written to *direction; nothing written when (x, z) is
// the sprite's own position.
extern "C" void __cdecl Field_DirectionTo(long x, long z, unsigned char* direction) {
    const records::PartyMember* const c = rv::Current<records::PartyMember>();
    const std::int32_t dx = static_cast<std::int32_t>(static_cast<std::uint32_t>(x) - static_cast<std::uint32_t>(c->x));
    const std::int32_t dz = static_cast<std::int32_t>(static_cast<std::uint32_t>(z) - static_cast<std::uint32_t>(c->z));
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

// original 0x51BA60, state 3: back to state 1 (Member_Control) unless bit 10
// of Field_ScriptFlags2 holds the member.
FH_EXPORT void __cdecl Member_ResumeUnlessHeld(void) {
    if ((Field_ScriptFlags2 & 0x400) == 0) rv::Current<records::PartyMember>()->state = 1;
}

// original 0x51BA80 (0x14 bytes), state 4: back to state 1 (Member_Control)
// unless bit 11 of Field_ScriptFlags2 (test ah, 8 of the dword 0x905BA4) holds
// the member - Member_ResumeUnlessHeld (state 3) with bit 10.
extern "C" void __cdecl Member_ResumeUnlessHeld800(void) {
    if ((Field_ScriptFlags2 & 0x800) == 0) Sc()[1] = 1;
}

// original 0x51BAA0 (0x125 bytes), state 6, the member's form action: it
// stops (Field_State +0x137 = 0) when the leader (ObjTrio +0x89) is of kind 3
// or 6 in its state 0xA step 1, when Field_Request is set, when the member is
// more than a reach from the point the member it follows (+6) will be at -
// that record's +0x34 / +0x38 plus its +0xC / +0x10 times its +9; the reach
// 0x20000 when its +0x70 byte is set, else 0x18000 (each axis, |d| signed with
// the original's wrap) -, or when Field_ScriptFlags2 has a bit of 0x1C00; else
// the handler of Field_FormActions for the loaded party set (0x90412C & 0x7F,
// 19 entries, unchecked). Then, once Field_State +0x137 is 0: the pose +8, +9
// = 0, the member +5 cleared (Member_ClearState), +1 = 1, +0xB = 0 and
// Member_Follow (a tail jump). FE1's Field_FormActionState 0x52F4F0 is the
// leader's. As the original has it: +6 indexes ObjTrio unchecked.
extern "C" void __cdecl Member_FormActionState(void) {
    unsigned char stop = 0;
    const unsigned char kind = ObjTrio[0].char_id;
    if ((kind == 3 || kind == 6) && ObjTrio[0].state == 0xA && ObjTrio[0].step == 1) stop = 1;
    if (Field_Request != 0) stop = 1;
    const unsigned char* const s = Sc();
    const unsigned char* const r = MemberRest1a(s[6]);
    const URest1a k = r[9];
    const URest1a ax = static_cast<URest1a>(rv::As<records::PartyMember>(r)->step_x) * k + static_cast<URest1a>(rv::As<records::PartyMember>(r)->x);
    const URest1a az = static_cast<URest1a>(rv::As<records::PartyMember>(r)->step_z) * k + static_cast<URest1a>(rv::As<records::PartyMember>(r)->z);
    const std::int32_t reach = r[0x70] != 0 ? 0x20000 : 0x18000;
    if (Abs32(static_cast<std::int32_t>(static_cast<URest1a>(rv::As<records::PartyMember>(s)->x) - ax)) > reach ||
        Abs32(static_cast<std::int32_t>(static_cast<URest1a>(rv::As<records::PartyMember>(s)->z) - az)) > reach)
        stop = 1;
    if ((Field_ScriptFlags2 & 0x1C00) == 0 && stop == 0)
        CodeAt(::bof3::Img(Field_FormActions_addr), Field_FormActions_count, AreaW0b_PartySet & 0x7Fu, "Member_FormActionState (0x51BAA0)")();
    else
        Fs()[0x137] = 0;
    if (Fs()[0x137] != 0) return;
    SH_CALL(Sprite_EnsureAnimation)(Sc()[8]);
    Sc()[9] = 0;
    SH_CALL(Member_ClearState)(Sc()[5]);
    Sc()[1] = 1;
    Sc()[0xB] = 0;
    SH_CALL(Member_Follow)();
}

// original 0x51BBD0, state 7, by its sub-state +2:
//   0 - once the leader's (ObjTrio's) +0x137 is not 8: +1 = 1, +2 = 0;
//   1 - an effect object from Effect_FindFree into +0xB; if there is none,
//       +2 = 0; else the object gets +0 = 1, kind +5 = 6, +6 = 1, +0xC = 0,
//       +0x10 = MoveScript_EffectArg[Field_State +0x89] (as a byte), +0x2E /
//       +0x30 the member's own words there, and +2 = 2;
//   2 - once that object's +0 is 0, +2 = 0;
// and Sprite_ScriptTick in every case (the original's tail).
//
// As the original has it: the sub-state and the effect index are not
// checked; in sub-state 1 the index is re-read from +0xB for every store.
FH_EXPORT void __cdecl Member_EffectState(void) {
    records::PartyMember* const s = rv::Current<records::PartyMember>();
    switch (s->step) {
    case 0:
        if (rv::Bytes(ObjTrio)[0x137] != 8) {
            s->state = 1;
            rv::Current<records::PartyMember>()->step = 0;
        }
        break;
    case 1: {
        const unsigned char i = Al(field_hidden::g.effect_free());
        Sprite_Current[0xB] = i;
        records::PartyMember* const c = rv::Current<records::PartyMember>();
        if (rv::Bytes(c)[0xB] == 0xFF) {
            c->step = 0;
            break;
        }
        rv::As<records::EffectObject>(Effect(rv::Bytes(c)[0xB]))->flags = 1;
        rv::As<records::EffectObject>(Effect(rv::Bytes(c)[0xB]))->kind = 6;
        rv::As<records::EffectObject>(Effect(rv::Bytes(c)[0xB]))->param = 1;
        rv::As<records::EffectObject>(Effect(rv::Bytes(c)[0xB]))->step_x = 0;
        const unsigned arg = static_cast<unsigned char>(MoveScript_EffectArg[rv::As<records::PartyMember>(Field_State)->char_id]);
        rv::As<records::EffectObject>(Effect(rv::Bytes(c)[0xB]))->step_z = static_cast<std::int32_t>(arg);
        rv::Store<unsigned>(rv::As<records::EffectObject>(Effect(rv::Bytes(c)[0xB]))->screen_x, static_cast<std::uint16_t>(c->screen_x));
        rv::Store<unsigned>(rv::As<records::EffectObject>(Effect(rv::Bytes(c)[0xB]))->screen_y, static_cast<std::uint16_t>(c->screen_y));
        c->step = 2;
        break;
    }
    case 2:
        if (rv::As<records::EffectObject>(Effect(rv::Bytes(s)[0xB]))->flags == 0) s->step = 0;
        break;
    default:
        break;
    }
    field_hidden::g.script_tick();
}

// original 0x51BCF0 (0x12 bytes), state 8, the member's jump: jmp
// Member_JumpSteps 0x65F9A4[+2] (Field_JumpBegin, Field_JumpOut,
// Member_JumpAir, Field_JumpIn - the leader's Field_JumpSteps with this
// group's step 2).
extern "C" void __cdecl Member_JumpState(void) { CodeAt(::bof3::Img(Member_JumpSteps_addr), Member_JumpSteps_count, Sc()[2], "Member_JumpState (0x51BCF0)")(); }

// original 0x51BD10 (0x8D bytes), jump step 2: the member against the one it
// follows (ObjTrio record +6, unchecked) - when that one's +0x137 is 3: more
// than 0x80 apart in height (+0x3E, s16; |d| of the sign-extended words),
// above it Leader_Sink, below or level Leader_Rise; within 0x80 nothing. Any
// other +0x137: at or below its height Leader_Rise, above it Leader_TurnBack.
extern "C" void __cdecl Member_JumpAir(void) {
    const unsigned char* const s = Sc();
    const unsigned char* const r = MemberRest1a(s[6]);
    const auto theirs = static_cast<short>(rv::As<records::PartyMember>(r)->height_whole);
    unsigned char how;
    if (r[0x137] == 3) {
        const auto mine = static_cast<short>(rv::As<records::PartyMember>(s)->height_whole);
        if (Abs32(static_cast<std::int32_t>(mine) - static_cast<std::int32_t>(theirs)) > 0x80) how = mine > theirs ? 1 : 0;
        else how = 3;
    } else {
        how = static_cast<short>(rv::As<records::PartyMember>(s)->height_whole) <= theirs ? 0 : 2;
    }
    switch (how) {
    case 0: SH_CALL(Leader_Rise)(); break;
    case 1: SH_CALL(Leader_Sink)(); break;
    case 2: SH_CALL(Leader_TurnBack)(); break;
    default: break;
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
    if (rv::Bytes(ObjTrio)[0x137] == 8) return 0;
    if (Field_Request != 0) return 0;
    records::PartyMember* const state = rv::As<records::PartyMember>(Field_State);
    if (Field_ActorStates[state->char_index * kActorBytes] & 0x20) return 0;
    rv::Bytes(state)[0x136] = static_cast<unsigned char>(rv::Bytes(state)[0x136] + 1);
    unsigned char* const s = Field_State;
    if (s[0x136] <= 0xF0) return 0;
    s[0x136] = 0;
    if (rv::Bytes(ObjTrio)[0x137] == 9) return 0;
    if (Member((rv::Current<records::PartyMember>()->actor & 0xFFu) ^ 3u)[0x137] == 9) return 0;
    const unsigned pick = (static_cast<unsigned>(member_sprites::g.rand()) & 1u) + 1u;
    if (rv::Current<records::PartyMember>()->actor != pick) return 0;
    Field_State[0x137] = 9;
    member_sprites::g.ensure_animation(rv::Current<records::PartyMember>()->direction);
    rv::Current<records::PartyMember>()->step_frames = 2;
    rv::Current<records::PartyMember>()->state = 6;
    rv::Current<records::PartyMember>()->step = 0;
    return 1;
}

void Member_Inject() {
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
    BOF3_INJECT(Member_ResumeUnlessHeld);
    BOF3_INJECT(Member_ResumeUnlessHeld800);
    BOF3_INJECT(Member_FormActionState);
    BOF3_INJECT(Member_EffectState);
    BOF3_INJECT(Member_JumpState);
    BOF3_INJECT(Member_JumpAir);
    BOF3_INJECT(Member_Idle);
}

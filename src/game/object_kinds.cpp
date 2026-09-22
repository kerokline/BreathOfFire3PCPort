// The field objects' kind handlers - the table at 0x65F5F8, indexed by the
// sprite's pose byte +1, less entries 4 and 7 (Field_ObjectUpdate and
// Field_ObjectFollow, field_objects.cpp) - with the pace table 0x65F5DC they
// call, the field object loops' three other per-object calls, and the small
// helpers only these use. docs/object-kinds.md.
//
// Every handler is called with the object, which in the loops is also
// Sprite_Current: the sprite fields ([1] pose, [2] pace, [3] pose to go back
// to, [4] sub-state, [6] type, [7] flags, [8] direction, [9] a timed move's
// frames, [0xA] a countdown) and the object fields (+0x80 the script context's
// flags, +0x81 a wait, +0x84 a speed index, +0x85 a target direction, +0x87
// steps) are one block there. The originals reload Sprite_Current after every
// call, and so does this code, by naming it each time.
#include "game/object_kinds.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <utility>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

using namespace move_script;

std::uint32_t Address(const volatile void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }

// Data these read that symbols.toml does not name.
constexpr std::uint32_t kPaceTable = 0x65F5DC;      // 7 handlers, by Sprite_Current[2], no bound
constexpr std::uint32_t kAsideRows = 0x65F624;      // 4 directions per leader direction
constexpr std::uint32_t kSteps = 0x6697B0;          // per direction: x, z step (16.16)
constexpr std::uint32_t kCounter = 0x905B80;        // byte: a counter the loop copies...
constexpr std::uint32_t kCounterSeen = 0x8034E8;    // ...here after each pass
constexpr std::uint32_t kBlockFlags = 0x8034E1;     // bit 6: blocks the idle turn
constexpr std::uint32_t kEncounter = 0x929F00;      // bytes +0, +1 and +0xC: a type-5 touch
constexpr std::uint32_t kTalker = 0x903804;         // dword: Field_ActiveMember of the last talk
constexpr std::uint32_t kLeaderDirection = 0x802D48;  // ObjTrio +8
constexpr std::uint32_t kLinkArgument = 0x802DC9;   // ObjTrio +0x89

using PaceFn = unsigned char (__cdecl*)(unsigned char*, unsigned, unsigned);

// Every callee, through pointers so that the start-up fuzz can stand
// recording functions in for them - for the originals' copies and for ours
// alike. Ours among them (the pace entries, the turn, the three helpers) are
// tested alone.
struct Callees {
    const PaceFn* pace;   // the table itself, read at the call as the original reads it
    PaceFn pace0;         // Field_ObjectIdleLong's direct call of entry 0
    int (__cdecl* rand)();
    unsigned char (__cdecl* random_turn)(unsigned char*);
    unsigned char (__cdecl* open_direction)(unsigned char*);
    unsigned char (__cdecl* in_home)(unsigned char*);
    void (__cdecl* turn)(unsigned char*);
    void (__cdecl* settle)(unsigned char*, unsigned char);
    void (__cdecl* motion)(unsigned char*);
    unsigned char (__cdecl* approach)(unsigned char*);
    unsigned char (__cdecl* avoid)(unsigned char*);
    unsigned char (__cdecl* best)(unsigned char*, unsigned char);
    unsigned char (__cdecl* blocked_ahead)(unsigned char*);
    void (__cdecl* move)(unsigned char*, unsigned char);
    unsigned char (__cdecl* set_tint)(unsigned char*, unsigned char, unsigned char, unsigned char, unsigned char);
    void (__cdecl* tint_release)(unsigned char);
    void (__cdecl* area_handler)(unsigned short);
    void (__cdecl* trigger)(unsigned char*);
    void (__cdecl* event_script)(const unsigned char*);
    void (__cdecl* msg_script)(unsigned short);   // Msg_OpenScript reads a word
    void (__cdecl* msg_system)(unsigned);         // Msg_OpenSystem a dword
};
Callees Originals() {
    return {reinterpret_cast<const PaceFn*>(static_cast<std::uintptr_t>(kPaceTable)),
            Field_Pace0,
            Rand,
            Field_ObjectRandomTurn,
            Field_ObjectOpenDirection,
            Field_ObjectInHome,
            Field_ObjectTurn,
            Field_ObjectSettle,
            Field_ObjectMotion,
            Field_ObjectApproachDirection,
            Field_ObjectAvoidDirection,
            Field_ObjectBestDirection,
            Field_ObjectBlockedAhead,
            MoveCmd_Move,
            Sprite_SetTint,
            Tint_Release,
            Area_ObjectHandler,
            Field_ObjectTrigger,
            EventScript_Run,
            reinterpret_cast<void (__cdecl*)(unsigned short)>(static_cast<std::uintptr_t>(bof3::addr::Msg_OpenScript)),
            reinterpret_cast<void (__cdecl*)(unsigned)>(static_cast<std::uintptr_t>(bof3::addr::Msg_OpenSystem))};
}
Callees g = Originals();

unsigned char Pace(unsigned char* object, unsigned speed, unsigned flags) {
    return g.pace[Sprite_Current[2]](object, speed, flags);
}

// The four wander kinds end alike: a direction `turn_to` (0xFF for none)
// becomes the target of a turn in pose 10 if a timed move began - returning
// while that pose holds - or the move is cancelled; then the settle and the
// motion. `direction` is Sprite_Current[8] as the handler found it.
void TurnOrCancel(unsigned char* object, unsigned char direction, bool turn, unsigned char turn_to) {
    if (turn) {
        Sprite_Current[3] = Sprite_Current[1];
        Sprite_Current[1] = 0x0A;
        object[0x85] = turn_to;
        Sprite_Current[8] = direction & 7;
        Sprite_Current[0xA] = 0;
        Sprite_Current[9] = 0;
        g.turn(object);
        if (Sprite_Current[1] == 0x0A) return;   // no settle, no motion this frame
        Sprite_Current[8] = direction;
    } else {
        Sprite_Current[9] = 0;
        Sprite_Current[8] = direction;
    }
    g.settle(object, direction);
    g.motion(object);
}

// Entries 0 and 3: a random walk, the second kept inside a home box.
void Wander(unsigned char* object, bool home) {
    const unsigned char direction = Sprite_Current[8];
    Pace(object, 0, 1);
    if (Sprite_Current[9] != 0) {   // a timed move runs
        g.motion(object);
        return;
    }
    const unsigned char speed_index = object[0x84];
    const unsigned char speed = Field_MoveSpeeds[speed_index];
    if (object[0x87] == 0) {
        if (speed_index != 0 && Pace(object, speed, 2)) {
            g.random_turn(object);
            if (Sprite_Current[9] != 0) object[0x87] = static_cast<unsigned char>((g.rand() & 3) + 3);
        }
    } else {
        // Not guarded against speed 0, as the original: Field_Pace0 then
        // divides by zero.
        Pace(object, speed, 2);
        --object[0x87];
    }
    const unsigned char turn_to = g.open_direction(object);
    const bool turn = turn_to != 0xFF && Sprite_Current[9] != 0 && (!home || g.in_home(object));
    TurnOrCancel(object, direction, turn, turn_to);
}

// Entries 1 and 2: toward the leader or away from it. With no steps left and
// no speed they return before the settle and the motion, which the wander
// kinds do not (the PSX's twins alike).
void Chase(unsigned char* object, unsigned char (__cdecl* pick)(unsigned char*), unsigned char toward) {
    const unsigned char direction = Sprite_Current[8];
    Pace(object, 0, 1);
    if (Sprite_Current[9] != 0) {
        g.motion(object);
        return;
    }
    const unsigned char speed_index = object[0x84];
    const unsigned char speed = Field_MoveSpeeds[speed_index];
    unsigned char turn_to = 0xFF;
    if (object[0x87] == 0) {
        if (speed_index == 0) return;
        if (Pace(object, speed, 2)) {
            turn_to = pick(object);
            if (Sprite_Current[9] != 0) object[0x87] = static_cast<unsigned char>((g.rand() & 3) + 3);
        }
    } else {
        Pace(object, speed, 2);
        turn_to = g.best(object, toward);
        --object[0x87];
    }
    const bool turn = turn_to != 0xFF && Sprite_Current[9] != 0;
    TurnOrCancel(object, direction, turn, static_cast<unsigned char>(turn_to & 7));
}

// Field_ObjectFadeOut / FadeIn's cases. The tint record is the one
// Sprite_SetTint's result names, kept in Field_ActiveMember +0x9F.
unsigned char* TintRecord(unsigned char index) { return MoveScript_TintRecords + index * 12u; }
int TintSum(const unsigned char* r) {   // signed bytes, as the original's movsx
    return static_cast<signed char>(r[4]) + static_cast<signed char>(r[3]) + static_cast<signed char>(r[2]);
}
void FadeOutStart() {
    const unsigned char index = g.set_tint(Sprite_Current, 0x1F, 0x1F, 0x1F, 1);
    Field_ActiveMember[0x9F] = index;
    Sprite_Current[0] &= 0xBF;
    Sprite_Current[4] = 1;
}
void FadeOutStep() {
    unsigned char* const member = Field_ActiveMember;
    for (const unsigned at : {2u, 3u, 4u}) {
        unsigned char* const r = TintRecord(member[0x9F]);   // read each time, as the original (nothing between changes it)
        if (r[at] != 0) --r[at];
    }
    const unsigned char index = member[0x9F];
    if (TintSum(TintRecord(index)) != 0) return;
    g.tint_release(index);
    Field_ActiveMember[0x80] &= 0xFD;   // context bit 1, which chose this pose
    Sprite_Current[1] = Sprite_Current[3];
    Sprite_Current[4] = 0;
}
void FadeInStart() {
    const unsigned char index = g.set_tint(Sprite_Current, 0, 0, 0, 1);
    Field_ActiveMember[0x9F] = index;
    Sprite_Current[4] = 1;
}
void FadeInStep() {
    unsigned char* const member = Field_ActiveMember;
    for (const unsigned at : {2u, 3u, 4u}) {
        unsigned char* const r = TintRecord(member[0x9F]);
        if (static_cast<signed char>(r[at]) < 0x1F) ++r[at];   // signed: 0x80..0xFF climb too
    }
    const unsigned char index = member[0x9F];
    if (TintSum(TintRecord(index)) != 0x5D) return;
    g.tint_release(index);
    Field_ActiveMember[0x80] &= 0xFB;   // context bit 2
    Sprite_Current[1] = Sprite_Current[3];
    Sprite_Current[4] = 0;
}

// Field_Pace4..6: on a pass where the chosen counter bit changed, count a
// step up (to 0xFA, by `add` - so Field_Pace5's 0xF9 reaches 0xFB) and
// return 1; else with flags bit 1 and steps left, a move's frames.
unsigned char PaceCounted(unsigned char* object, unsigned speed, unsigned flags, unsigned char bit, unsigned char add) {
    if ((flags & 1) && ((At(kCounter)[0] ^ At(kCounterSeen)[0]) & bit)) {
        if (object[0x87] < 0xFA) object[0x87] = static_cast<unsigned char>(object[0x87] + add);
        return 1;
    }
    if ((flags & 2) && object[0x87] != 0) {
        Sprite_Current[9] = static_cast<unsigned char>(0x20 / static_cast<int>(speed & 0xFF));   // 0 faults, as the idiv
        Sprite_Current[0xA] = static_cast<unsigned char>(Sprite_Current[9] >> 1);
    }
    return 0;
}

}  // namespace

// --- the handler table 0x65F5F8 ------------------------------------------------

// original 0x517640, entry 0 (PSX FUN_801A1AEC): a random walk.
extern "C" void __cdecl Field_ObjectWander(unsigned char* object) { Wander(object, false); }

// original 0x517790, entry 1 (PSX FUN_801A1D04): toward the leader.
extern "C" void __cdecl Field_ObjectApproach(unsigned char* object) { Chase(object, g.approach, 1); }

// original 0x517900, entry 2 (PSX FUN_801A1F2C): away from the leader.
extern "C" void __cdecl Field_ObjectAvoid(unsigned char* object) { Chase(object, g.avoid, 0); }

// original 0x517A70, entry 3 (PSX FUN_801A2154): a random walk inside the
// home box object +0x8E / +0x92, half-widths +0x98 / +0x9A.
extern "C" void __cdecl Field_ObjectWanderHome(unsigned char* object) { Wander(object, true); }

// original 0x518AC0, entry 5 (PSX 0x801A3724): the wait Sprite_SetPoseWait
// begins; context bit 4 while it counts, then the pose back.
extern "C" void __cdecl Field_ObjectWait(unsigned char* object) {
    if (object[0x81] == 0) {
        Sprite_Current[1] = Sprite_Current[3];
        return;
    }
    --object[0x81];
    object[0x80] |= 0x10;
}

// original 0x517FE0, entry 6 (PSX 0x801A2A2C): standing still.
extern "C" void __cdecl Field_ObjectStill(unsigned char* object) {
    if (Sprite_Current[6] != 8) object[0x80] |= 0x10;
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

// original 0x5196E0, entry 10 (PSX FUN_801A4B84): turning to +0x85 an eighth
// every four frames, then one step. The original takes an absolute value of
// (target - direction) & 7, which cannot be negative - kept as the plain
// masked difference.
extern "C" void __cdecl Field_ObjectTurn(unsigned char* object) {
    if (Sprite_Current[7] & 8) Sprite_Current[8] = object[0x85];
    if (Sprite_Current[0xA] != 0) {
        --Sprite_Current[0xA];
        return;
    }
    const unsigned char target = object[0x85];
    const unsigned char facing = Sprite_Current[8];
    if (target == facing) {
        Sprite_Current[1] = Sprite_Current[3];
        if (!(Sprite_Current[7] & 0x80) && g.blocked_ahead(object)) {
            Sprite_Current[8] &= 7;
            Sprite_Current[0xA] = 0;
            return;
        }
        unsigned char* const context = object + 0x80;
        g.move(context, Sprite_Current[8]);   // the whole byte, bit 3 and all
        Sprite_Current[8] |= 8;
        context[0] |= 8;
        Sprite_Current[0xA] = static_cast<unsigned char>(Sprite_Current[9] >> 1);
        return;
    }
    Sprite_Current[0xA] = 3;
    const int step = ((target - facing) & 7) < 4 ? 1 : -1;
    SetLong(Sprite_Current + 0x10, 0);
    SetLong(Sprite_Current + 0xC, 0);
    Sprite_Current[8] = static_cast<unsigned char>((Sprite_Current[8] + step) & 7);
    object[0x80] |= 8;
}

// --- the pace table 0x65F5DC ---------------------------------------------------

// original 0x5187C0, entries 0..3 (PSX FUN_801A3264). `speed` and `flags` are
// read as bytes. Speed 0 with flags bit 1 divides by zero, as the original.
extern "C" unsigned char __cdecl Field_Pace0(unsigned char* object, unsigned speed, unsigned flags) {
    (void)object;
    if (flags & 2) {
        Sprite_Current[9] = static_cast<unsigned char>(0x20 / static_cast<int>(speed & 0xFF));
        Sprite_Current[0xA] = static_cast<unsigned char>(Sprite_Current[9] >> 1);
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

// --- helpers only the kinds use ------------------------------------------------

// original 0x518CA0 (PSX FUN_801A3A0C): up to four quarter turns off a blocked
// direction. Context bit 3 marks a change against the entry direction & 7 -
// compared with the unmasked byte, so a moving sprite (bit 3) always counts
// as changed; kept, the PSX compares the same way.
extern "C" unsigned char __cdecl Field_ObjectOpenDirection(unsigned char* object) {
    const unsigned char entry = Sprite_Current[8] & 7;
    unsigned char turns = 0;
    do {
        if (!g.blocked_ahead(object)) break;
        Sprite_Current[8] = static_cast<unsigned char>((Sprite_Current[8] + 2) & 7);
        ++turns;
    } while (turns < 4);
    if (entry != Sprite_Current[8]) object[0x80] |= 8;
    if (turns >= 4) return 0xFF;
    return Sprite_Current[8] & 7;
}

// original 0x518DD0 (PSX FUN_801A3C18): a random odd direction. The original
// pushes 0x80 for Rand, which takes nothing.
extern "C" unsigned char __cdecl Field_ObjectRandomTurn(unsigned char* object) {
    const auto direction = static_cast<unsigned char>((g.rand() & 6) | 1);
    if ((Sprite_Current[8] & 7) == direction) return Sprite_Current[8];
    Sprite_Current[8] = direction;
    object[0x80] |= 8;
    return Sprite_Current[8];
}

// original 0x518000 (PSX FUN_801A2A60): whether the next step stays in the
// home box. Everything from the argument, not Sprite_Current.
extern "C" unsigned char __cdecl Field_ObjectInHome(unsigned char* object) {
    const unsigned char* const step = At(kSteps) + (object[8] & 7) * 8u;
    const std::uint32_t x = static_cast<std::uint32_t>(Long(step)) + static_cast<std::uint32_t>(Long(object + 0x34));
    const std::uint32_t z = static_cast<std::uint32_t>(Long(step + 4)) + static_cast<std::uint32_t>(Long(object + 0x38));
    int dx = static_cast<short>(x >> 16) - static_cast<short>(Word(object + 0x8E));
    if (dx < 0) dx = -dx;
    if (static_cast<short>(Word(object + 0x98)) < dx) return 0;
    int dz = static_cast<short>(z >> 16) - static_cast<short>(Word(object + 0x92));
    if (dz < 0) dz = -dz;
    return static_cast<short>(Word(object + 0x9A)) >= dz ? 1 : 0;
}

// --- the loops' other calls ----------------------------------------------------

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
        g.area_handler(At(kLinkArgument)[0]);
        return;
    }
    Field_ActiveMember[0x80] &= 0xFE;
}

// original 0x518B40 (PSX FUN_801A37CC): an object pushed for more than 30
// frames (+0x9C) steps aside - to the first of four directions, by the
// leader's facing, that is open (and, for pose 3, inside the home box).
extern "C" void __cdecl Field_ObjectIdleLong(unsigned char* object) {
    const unsigned char speed_index = object[0x84];
    SetWord(object + 0x9C, 0);
    if (speed_index == 0) return;
    if (Sprite_Current[1] == 6 || Sprite_Current[1] == 4 || Sprite_Current[3] == 4) return;
    g.pace0(object, Field_MoveSpeeds[speed_index], 2);
    const unsigned char direction = Sprite_Current[8];
    unsigned char n = 0;
    for (; n < 4; ++n) {
        Sprite_Current[8] = At(kAsideRows)[n + (At(kLeaderDirection)[0] & 7) * 4u];
        if (g.blocked_ahead(object)) continue;
        if (Sprite_Current[1] == 3 || Sprite_Current[3] == 3) {
            if (g.in_home(object)) break;
            continue;
        }
        break;
    }
    // (The original tests n < 4 again after a found direction - never false.)
    if (n >= 4 || Sprite_Current[9] == 0) {
        Sprite_Current[8] = direction & 7;
        Sprite_Current[9] = 0;
        return;
    }
    if (object[0x81] != 0) {
        // [3] was not 4 on entry; only a callee changing it reaches this.
        if (Sprite_Current[3] == 4) {
            Sprite_Current[8] = direction;   // unmasked here
            return;
        }
        object[0x81] = 0;
        Sprite_Current[1] = Sprite_Current[3];
    }
    unsigned char* const context = object + 0x80;
    g.move(context, Sprite_Current[8]);
    Sprite_Current[8] |= 8;
    if (!(Sprite_Current[7] & 8)) context[0] |= 8;
}

// original 0x517F30 (PSX FUN_801A28D0): an object touched (context bit 5)
// with no talk (bit 4).
extern "C" void __cdecl Field_ObjectIdle(unsigned char* object) {
    if (Field_Request == 2) return;
    if (object[0x86] != 0xFF) g.trigger(object);
    if (Field_Request == 2) return;
    unsigned char* const sprite = Sprite_Current;
    if (sprite[6] == 5) {
        At(kEncounter)[0] = 0;
        At(kEncounter)[1] = 0;
        Game_Mode = 7;
        // (object - Sprite_Objects) / 0xA4, signed as the original's imul.
        const auto offset = static_cast<std::int32_t>(Address(object) - Address(Sprite_Objects));
        At(kEncounter)[0xC] = static_cast<unsigned char>(offset / 0xA4);
        return;
    }
    if (!(At(kBlockFlags)[0] & 0x40)) {
        if (!(sprite[7] & 8)) {
            sprite[8] = object[0x85];
            object[0x80] |= 8;
        }
        Field_ScriptFlags &= 0xFEFF;
    }
    object[0x80] &= 0xDF;
}

// original 0x517E90 (PSX FUN_801A27A8): the object's talk - the word at
// context +8 is a script (bit 15, the area descriptor's +4 table), a system
// message (bit 13) or a script message. The word is read again after the
// script, which may change it; the one found on entry is put back.
extern "C" unsigned char __cdecl MoveScript_SetTurnTarget(unsigned char* context) {
    const std::uint16_t word = Word(context + 8);
    context[0] &= 0xEF;
    if (word == 0xFFFF) return 0;
    if (word & 0x8000) {
        const unsigned char* const descriptor = Area_Descriptors[Game_AreaNumber];
        const unsigned char* const* scripts;
        std::memcpy(&scripts, descriptor + 4, sizeof scripts);
        g.event_script(scripts[word & 0xFFF]);
    }
    const std::uint16_t now = Word(context + 8);
    if (!(now & 0x8000)) {
        if (now & 0x2000) g.msg_system(now & 0xFFFu);
        else g.msg_script(now);
        Field_Request = 2;
    }
    SetWord(context + 8, word);
    if (Sprite_Current[7] & 4) context[0] &= 0xDF;
    unsigned char* const member = Field_ActiveMember;
    std::memcpy(At(kTalker), &member, sizeof member);
    return 1;
}

namespace {

// --- BOF3X_SHADOW=object_kinds: a differential fuzz, once at start-up --------
// Nineteen byte-copies (the two fades are one), every relative call re-aimed at
// a recorder, every `call [reg*4 + 0x65F5DC]` at a table of 256 recording pace
// entries, the fades' two jump tables moved into the copy. One round: one
// function, a random object / sprite / member (the sprite often the object,
// as in the loops), the globals they touch, boundaries seeded; theirs, then
// from the same state ours; the result byte, all of it and the recorders' log
// compared.

constexpr unsigned kBuf = 0x100, kLog = 48;
unsigned char g_object[kBuf], g_sprite[kBuf], g_member[kBuf], g_descriptor[0x44];
const unsigned char* g_scripts[0x1000];   // the descriptor's +4 table
unsigned char* g_current_object;
struct Entry { std::uint32_t what, a, b, c; };
Entry g_log[kLog];
unsigned g_log_n, g_seed;

std::uint32_t Hash() {
    std::uint32_t h = (g_seed + g_log_n) * 0x9E3779B1u;
    h ^= h >> 15;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    return h;
}
void Record(std::uint32_t what, std::uint32_t a = 0, std::uint32_t b = 0, std::uint32_t c = 0) {
    if (g_log_n < kLog) g_log[g_log_n] = {what, a, b, c};
    ++g_log_n;
}
std::uint32_t Id(const void* p) {
    const unsigned char* const bufs[3] = {g_object, g_sprite, g_member};
    for (unsigned i = 0; i < 3; ++i) {
        if (p == bufs[i]) return 1 + i;
        if (p == bufs[i] + 0x80) return 4 + i;
    }
    return Address(p);
}
// A callee may change what the caller reads after it: the fields the real
// callees write (steps, frames, pose, direction, countdown, flags), the talk
// word, Field_Request, and - rarely - which object and member are current.
void Disturb() {
    const std::uint32_t h = Hash();
    unsigned char* const o = g_current_object;
    if (h % 4 == 0) o[0x80] = static_cast<unsigned char>(h >> 8);
    if (h % 5 == 0) Sprite_Current[9] = static_cast<unsigned char>((h >> 12) % 3);
    if (h % 6 == 0) Sprite_Current[8] = static_cast<unsigned char>(h >> 16);
    if (h % 7 == 0) o[0x87] = static_cast<unsigned char>((h >> 20) % 3);
    if (h % 9 == 0) Sprite_Current[1] = static_cast<unsigned char>(h % 2 ? 0x0A : h >> 24);
    if (h % 11 == 0) Sprite_Current[0xA] = static_cast<unsigned char>((h >> 8) % 3);
    if (h % 13 == 0) o[0x89] ^= 0x80;   // the talk word's bit 15
    if (h % 17 == 0) Field_Request = 2;
    if (h % 19 == 0) o[0x81] = static_cast<unsigned char>(h >> 4);
    if (h % 23 == 0) Field_ActiveMember = (h >> 9) % 2 ? g_member : g_object;
    if (h % 29 == 0) Sprite_Current = (h >> 9) % 2 ? g_sprite : g_object;
    if (h % 31 == 0) Sprite_Current[3] = static_cast<unsigned char>((h >> 5) % 2 ? 4 : h >> 16);
}
unsigned char Direction() {   // 0xFF a quarter of the time, else any byte
    const std::uint32_t h = Hash();
    return static_cast<unsigned char>(h % 4 == 0 ? 0xFF : h >> 8);
}

template <int K>
unsigned char __cdecl StubPace(unsigned char* o, unsigned speed, unsigned flags) {
    Record(40 + K, Id(o), speed & 0xFF, flags & 0xFF);
    Disturb();
    return static_cast<unsigned char>(Hash() % 3 ? 0 : Hash() >> 7 | 1);
}
PaceFn g_pace_stubs[256];

unsigned char __cdecl StubPace0(unsigned char* o, unsigned speed, unsigned flags) { Record(1, Id(o), speed & 0xFF, flags & 0xFF); Disturb(); return static_cast<unsigned char>(Hash()); }
int __cdecl StubRand() { Record(2); return static_cast<int>(Hash()); }
unsigned char __cdecl StubRandomTurn(unsigned char* o) { Record(3, Id(o)); Disturb(); return static_cast<unsigned char>(Hash()); }
unsigned char __cdecl StubOpenDirection(unsigned char* o) { Record(4, Id(o)); Disturb(); return Direction(); }
unsigned char __cdecl StubInHome(unsigned char* o) { Record(5, Id(o)); return static_cast<unsigned char>(Hash() % 2 ? Hash() | 1 : 0); }
void __cdecl StubTurn(unsigned char* o) { Record(6, Id(o)); Disturb(); }
void __cdecl StubSettle(unsigned char* o, unsigned d) { Record(7, Id(o), d & 0xFF); }
void __cdecl StubMotion(unsigned char* o) { Record(8, Id(o)); }
unsigned char __cdecl StubApproach(unsigned char* o) { Record(9, Id(o)); Disturb(); return Direction(); }
unsigned char __cdecl StubAvoid(unsigned char* o) { Record(10, Id(o)); Disturb(); return Direction(); }
unsigned char __cdecl StubBest(unsigned char* o, unsigned toward) { Record(11, Id(o), toward & 0xFF); Disturb(); return Direction(); }
unsigned char __cdecl StubBlockedAhead(unsigned char* o) { Record(12, Id(o), Sprite_Current[8]); Disturb(); return static_cast<unsigned char>(Hash() % 2 ? Hash() | 1 : 0); }
void __cdecl StubMove(unsigned char* c, unsigned d) { Record(13, Id(c), d & 0xFF); Disturb(); }
unsigned char __cdecl StubSetTint(unsigned char* s, unsigned r, unsigned gr, unsigned b, unsigned a) {
    Record(14, Id(s), (r & 0xFF) | (gr & 0xFF) << 8 | (b & 0xFF) << 16, a & 0xFF);
    return static_cast<unsigned char>(Hash());
}
void __cdecl StubTintRelease(unsigned i) { Record(15, i & 0xFF); Disturb(); }
void __cdecl StubAreaHandler(unsigned n) { Record(16, n & 0xFFFF); }
void __cdecl StubTrigger(unsigned char* o) { Record(17, Id(o)); Disturb(); }
void __cdecl StubEventScript(const unsigned char* p) { Record(18, Address(p)); Disturb(); }
void __cdecl StubMsgScript(unsigned id) { Record(19, id & 0xFFFF); }
void __cdecl StubMsgSystem(unsigned id) { Record(20, id); }

template <typename T, typename F> T As(F f) { return reinterpret_cast<T>(reinterpret_cast<void*>(f)); }
Callees Stubs() {
    Callees s{};
    s.pace = g_pace_stubs;
    s.pace0 = StubPace0;
    s.rand = StubRand;
    s.random_turn = StubRandomTurn;
    s.open_direction = StubOpenDirection;
    s.in_home = StubInHome;
    s.turn = StubTurn;
    s.settle = As<decltype(s.settle)>(&StubSettle);
    s.motion = StubMotion;
    s.approach = StubApproach;
    s.avoid = StubAvoid;
    s.best = As<decltype(s.best)>(&StubBest);
    s.blocked_ahead = StubBlockedAhead;
    s.move = As<decltype(s.move)>(&StubMove);
    s.set_tint = As<decltype(s.set_tint)>(&StubSetTint);
    s.tint_release = As<decltype(s.tint_release)>(&StubTintRelease);
    s.area_handler = As<decltype(s.area_handler)>(&StubAreaHandler);
    s.trigger = StubTrigger;
    s.event_script = StubEventScript;
    s.msg_script = As<decltype(s.msg_script)>(&StubMsgScript);
    s.msg_system = StubMsgSystem;
    return s;
}

const void* StubFor(std::uint32_t target) {
    const auto f = [](auto p) { return reinterpret_cast<const void*>(p); };
    switch (target) {
    case 0x5187C0: return f(&StubPace0);
    case 0x5B93D2: return f(&StubRand);
    case 0x518DD0: return f(&StubRandomTurn);
    case 0x518CA0: return f(&StubOpenDirection);
    case 0x518000: return f(&StubInHome);
    case 0x5196E0: return f(&StubTurn);
    case 0x518D10: return f(&StubSettle);
    case 0x518980: return f(&StubMotion);
    case 0x518E20: return f(&StubApproach);
    case 0x518ED0: return f(&StubAvoid);
    case 0x518F80: return f(&StubBest);
    case 0x518080: return f(&StubBlockedAhead);
    case 0x578C10: return f(&StubMove);
    case 0x454CC0: return f(&StubSetTint);
    case 0x454D60: return f(&StubTintRelease);
    case 0x537540: return f(&StubAreaHandler);
    case 0x56D6B0: return f(&StubTrigger);
    case 0x5797C0: return f(&StubEventScript);
    case 0x4976D0: return f(&StubMsgScript);
    case 0x497710: return f(&StubMsgSystem);
    default: bof3::Fatal("object_kinds: no stand-in for a call to 0x%X", (unsigned)target); return nullptr;
    }
}

// The copies and their calls out, by capstone 2026-09-22 (every jump stays
// inside); `pace` lists the disp32 of each `call [reg*4 + 0x65F5DC]`.
struct Call { std::uint32_t offset, target; };
struct Clone {
    const char* name;
    std::uint32_t base, size;
    Call calls[7];
    int n_calls;
    std::uint32_t pace[3];
    int n_pace;
};
constexpr std::uint32_t kFadeBase = 0x5193B0, kFadeSize = 0x249, kFadeInOffset = 0x130;
const Clone kClones[] = {
    {"Field_ObjectWander", 0x517640, 0x14F,
     {{0x7b, 0x518dd0}, {0x90, 0x5b93d2}, {0xc1, 0x518ca0}, {0x112, 0x5196e0}, {0x139, 0x518d10}, {0x142, 0x518980}}, 6, {0x21, 0x6f, 0xab}, 3},
    {"Field_ObjectApproach", 0x517790, 0x16A,
     {{0x82, 0x518e20}, {0x98, 0x5b93d2}, {0xba, 0x518f80}, {0x121, 0x5196e0}, {0x155, 0x518d10}, {0x15e, 0x518980}}, 6, {0x20, 0x72, 0xb3}, 3},
    {"Field_ObjectAvoid", 0x517900, 0x16A,
     {{0x82, 0x518ed0}, {0x98, 0x5b93d2}, {0xba, 0x518f80}, {0x121, 0x5196e0}, {0x155, 0x518d10}, {0x15e, 0x518980}}, 6, {0x20, 0x72, 0xb3}, 3},
    {"Field_ObjectWanderHome", 0x517A70, 0x173,
     {{0x7a, 0x518dd0}, {0x8f, 0x5b93d2}, {0xc0, 0x518ca0}, {0xdd, 0x518000}, {0x129, 0x5196e0}, {0x15e, 0x518d10}, {0x167, 0x518980}}, 7, {0x20, 0x6e, 0xaa}, 3},
    {"Field_ObjectWait", 0x518AC0, 0x31, {}, 0, {}, 0},
    {"Field_ObjectStill", 0x517FE0, 0x17, {}, 0, {}, 0},
    {"Field_ObjectFadeOut", kFadeBase, kFadeSize, {{0x2e, 0x454cc0}, {0xf4, 0x454d60}, {0x15e, 0x454cc0}, {0x217, 0x454d60}}, 4, {}, 0},
    {"Field_ObjectFadeIn", 0, 0, {}, 0, {}, 0},   // FadeOut's copy + 0x130
    {"Field_ObjectTurn", 0x5196E0, 0x103, {{0x4d, 0x518080}, {0x84, 0x578c10}}, 2, {}, 0},
    {"Field_ObjectLinked", 0x519600, 0x6F, {{0x35, 0x537540}, {0x68, 0x537540}}, 2, {}, 0},
    {"Field_ObjectIdleLong", 0x518B40, 0x15A, {{0x52, 0x5187c0}, {0x8f, 0x518080}, {0xad, 0x518000}, {0x12e, 0x578c10}}, 4, {}, 0},
    {"Field_ObjectIdle", 0x517F30, 0xA7, {{0x1c, 0x56d6b0}}, 1, {}, 0},
    {"MoveScript_SetTurnTarget", 0x517E90, 0x95, {{0x3f, 0x5797c0}, {0x56, 0x4976d0}, {0x63, 0x497710}}, 3, {}, 0},
    {"Field_Pace0", 0x5187C0, 0x32, {}, 0, {}, 0},
    {"Field_Pace4", 0x518800, 0x71, {}, 0, {}, 0},
    {"Field_Pace5", 0x518880, 0x71, {}, 0, {}, 0},
    {"Field_Pace6", 0x518900, 0x71, {}, 0, {}, 0},
    {"Field_ObjectOpenDirection", 0x518CA0, 0x6A, {{0x18, 0x518080}}, 1, {}, 0},
    {"Field_ObjectRandomTurn", 0x518DD0, 0x44, {{0x5, 0x5b93d2}}, 1, {}, 0},
    {"Field_ObjectInHome", 0x518000, 0x71, {}, 0, {}, 0},
};
constexpr unsigned kCount = sizeof kClones / sizeof kClones[0];
enum : unsigned {
    kWander, kApproach, kAvoid, kWanderHome, kWait, kStill, kFadeOut, kFadeIn, kTurn, kLinked, kIdleLong, kIdle, kTalk,
    kPace0, kPace4, kPace5, kPace6, kOpenDirection, kRandomTurn, kInHome
};
static_assert(kInHome + 1 == kCount, "one enumerator per clone");

// The fades' jump tables: 0x65F654 (FadeOut, 2 entries, then FadeIn's) and
// 0x65F65C (FadeIn, 2): four entries, moved into the copy.
std::uint32_t g_fade_table[4];

void Patch32(void* copy, std::uint32_t at, std::uint32_t expected, std::uint32_t value, const char* name) {
    auto* code = static_cast<std::uint8_t*>(copy);
    std::uint32_t old;
    std::memcpy(&old, code + at, sizeof old);
    if (old != expected) bof3::Fatal("object_kinds: %s +0x%X holds 0x%X, not 0x%X", name, (unsigned)at, (unsigned)old, (unsigned)expected);
    std::memcpy(code + at, &value, sizeof value);
}

// What the functions touch outside the three buffers. The first region runs
// from below Sprite_Objects (Field_ObjectIdle's object may sit anywhere in or
// just below it) through MoveScript_TintRecords.
struct Region { std::uint32_t at, size; };
constexpr std::uint32_t kObjectsLow = 0x7DEE80 - 0x200;
const Region kRegions[] = {
    {kObjectsLow, 0x7E1300 - kObjectsLow},
    {Address(&Field_Request), 1},
    {Address(&Game_Mode), 2},
    {kEncounter, 0x10},
    {0x8034E0, 0x10},   // kBlockFlags, kCounterSeen
    {Address(&Field_ScriptFlags), 2},
    {kTalker, 4},
    {kCounter, 1},
    {Address(ObjTrio), 0x100},   // kLeaderDirection, kLinkArgument
};
constexpr unsigned kRegionBytes = (0x7E1300 - kObjectsLow) + 1 + 2 + 0x10 + 0x10 + 2 + 4 + 1 + 0x100;

struct State {
    unsigned char memory[kRegionBytes];
    unsigned char object[kBuf], sprite[kBuf], member[kBuf];
    unsigned char* current;
    unsigned char* active;
    std::uint16_t area;
    std::uint32_t result;
    Entry log[kLog];
    unsigned log_n;
};
void Capture(State& s, std::uint32_t result) {
    unsigned at = 0;
    for (const Region& r : kRegions) { std::memcpy(s.memory + at, At(r.at), r.size); at += r.size; }
    std::memcpy(s.object, g_object, kBuf);
    std::memcpy(s.sprite, g_sprite, kBuf);
    std::memcpy(s.member, g_member, kBuf);
    s.current = Sprite_Current;
    s.active = Field_ActiveMember;
    s.area = Game_AreaNumber;
    s.result = result;
    std::memcpy(s.log, g_log, sizeof s.log);
    s.log_n = g_log_n;
}
void Apply(const State& s) {
    unsigned at = 0;
    for (const Region& r : kRegions) { std::memcpy(At(r.at), s.memory + at, r.size); at += r.size; }
    std::memcpy(g_object, s.object, kBuf);
    std::memcpy(g_sprite, s.sprite, kBuf);
    std::memcpy(g_member, s.member, kBuf);
    Sprite_Current = s.current;
    Field_ActiveMember = s.active;
    Game_AreaNumber = s.area;
    std::memset(g_log, 0, sizeof g_log);
    g_log_n = 0;
}

std::uint32_t g_rng = 0x3C6EF372u;
std::uint32_t Next() { g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5; return g_rng; }
bool Often() { return Next() % 3 == 0; }
unsigned char Pick(std::initializer_list<unsigned char> values) { return values.begin()[Next() % values.size()]; }

using ObjectFn = void (__cdecl*)(unsigned char*);
using ByteFn = std::uint32_t (__cdecl*)(unsigned char*);   // al is the result; the rest of eax is noise
using PaceCallFn = std::uint32_t (__cdecl*)(unsigned char*, unsigned, unsigned);
using VoidFn = void (__cdecl*)();

const void* Ours(unsigned k) {
    const auto f = [](auto p) { return reinterpret_cast<const void*>(p); };
    switch (k) {
    case kWander: return f(&Field_ObjectWander);
    case kApproach: return f(&Field_ObjectApproach);
    case kAvoid: return f(&Field_ObjectAvoid);
    case kWanderHome: return f(&Field_ObjectWanderHome);
    case kWait: return f(&Field_ObjectWait);
    case kStill: return f(&Field_ObjectStill);
    case kFadeOut: return f(&Field_ObjectFadeOut);
    case kFadeIn: return f(&Field_ObjectFadeIn);
    case kTurn: return f(&Field_ObjectTurn);
    case kLinked: return f(&Field_ObjectLinked);
    case kIdleLong: return f(&Field_ObjectIdleLong);
    case kIdle: return f(&Field_ObjectIdle);
    case kTalk: return f(&MoveScript_SetTurnTarget);
    case kPace0: return f(&Field_Pace0);
    case kPace4: return f(&Field_Pace4);
    case kPace5: return f(&Field_Pace5);
    case kPace6: return f(&Field_Pace6);
    case kOpenDirection: return f(&Field_ObjectOpenDirection);
    case kRandomTurn: return f(&Field_ObjectRandomTurn);
    default: return f(&Field_ObjectInHome);
    }
}

// Sets the round's input: random bytes everywhere, then the boundaries each
// branch turns on. Returns the object argument.
unsigned char* Seed(unsigned k, State& input, unsigned& speed, unsigned& flags) {
    auto* bytes = reinterpret_cast<unsigned char*>(&input);
    for (unsigned i = 0; i < offsetof(State, current); ++i) bytes[i] = static_cast<unsigned char>(Next());
    input.current = Next() % 2 ? g_object : g_sprite;   // the loops' own case half the time
    const unsigned m = Next() % 3;
    input.active = m == 0 ? g_object : m == 1 ? g_member : g_sprite;
    input.area = static_cast<std::uint16_t>(Next() % Area_Descriptors_count);
    g_seed = Next();
    Apply(input);

    unsigned char* object = g_object;
    if (k == kIdle && Often()) object = At(kObjectsLow + Next() % (0x200 + 29 * 0xA4));   // in, and below, Sprite_Objects
    g_current_object = object;
    unsigned char* const s = Sprite_Current;
    unsigned char* const member = Field_ActiveMember;
    // Timed move, steps, speed (0..5 mostly), wait.
    s[9] = static_cast<unsigned char>(Next() % 2 ? 0 : Next() % 3);
    object[0x87] = Pick({0, 0, 0, 1, 2, 0xF8, 0xF9, 0xFA, 0xFB, static_cast<unsigned char>(Next())});
    object[0x84] = static_cast<unsigned char>(Next() % 4 ? Next() % 6 : Next());
    object[0x81] = static_cast<unsigned char>(Next() % 2 ? 0 : Next() % 3 ? 1 : Next());
    object[0x86] = static_cast<unsigned char>(Next() % 2 ? 0xFF : Next());
    // Poses, type, flags, sub-state.
    if (Often()) s[1] = Pick({3, 4, 6, 0x0A});
    if (Often()) s[3] = Pick({3, 4, 6, 0x0A});
    if (Often()) s[6] = Pick({1, 3, 4, 5, 8, 0x0A});
    if (Often()) s[7] &= static_cast<unsigned char>(Next());
    if (Often()) s[0] ^= 0x40;
    s[4] = static_cast<unsigned char>(Next() % (k == kFadeOut ? 4 : 2));
    // Directions: the target equal to the facing, half a turn off (the turn's
    // boundary), bit 3 set; the countdown at 0.
    if (Often()) s[8] &= 7;
    if (Often()) object[0x85] = s[8];
    else if (Often()) object[0x85] = static_cast<unsigned char>((s[8] + Pick({3, 4, 5})) & 7);
    if (Often()) s[0xA] = 0;
    // The member's area handler index and tint record, its bytes near the
    // fades' ends and signs.
    if (Often()) member[0xA0] = Pick({0x7F, 0xFF, 0x7E, 0});
    unsigned char* const record = MoveScript_TintRecords + member[0x9F] * 12u;
    if (Often()) {
        const unsigned char near = Pick({0, 1, 2, 3, 0x1D, 0x1E, 0x1F, 0x20, 0x7F, 0x80, 0xFE, 0xFF});
        for (unsigned i = 2; i < 5; ++i) record[i] = Often() ? near : Pick({0, 1, 3, 0x1E, 0x1F, 0x80, 0xFF, 0xFE});
    }
    // Globals.
    if (Often()) Field_Request = 2;
    if (Often()) At(kBlockFlags)[0] &= 0xBF;
    if (Often()) At(kCounter)[0] = At(kCounterSeen)[0];
    // The talk word: none, a script, a system message, a script message.
    const unsigned talk = Next() % 4;
    SetWord(object + 0x88, talk == 0 ? 0xFFFFu : talk == 1 ? (0x8000u | Next()) : talk == 2 ? ((Next() & 0x5FFFu) | 0x2000u) : Next() & 0x5FFFu);
    // The home box: the next step's high words at the edge, just in, just out.
    if (k == kInHome && Next() % 4) {
        const unsigned char* const step = At(kSteps) + (object[8] & 7) * 8u;
        for (unsigned axis = 0; axis < 2; ++axis) {
            const auto home = static_cast<std::int16_t>(Next() % 4 ? Next() % 64 : Next());
            const auto half = static_cast<std::int16_t>(Next() % 8 ? Next() % 8 : Next());
            SetWord(object + 0x8E + axis * 4, static_cast<std::uint16_t>(home));
            SetWord(object + 0x98 + axis * 2, static_cast<std::uint16_t>(half));
            const int side = Next() % 2 ? 1 : -1;
            const auto hi = static_cast<std::uint32_t>(home + side * half + static_cast<int>(Next() % 3) - 1);
            const std::uint32_t target = hi << 16 | (Next() & 0xFFFF);
            SetLong(object + 0x34 + axis * 4, static_cast<std::int32_t>(target - static_cast<std::uint32_t>(Long(step + axis * 4))));
        }
    }
    // A pace entry's arguments: bytes with noise above them; a speed never 0
    // where it would divide.
    flags = (Next() & ~0xFFu) | (Next() % 4);
    speed = (Next() & ~0xFFu) | (1 + Next() % 32);
    if (Next() % 8 == 0) speed = (speed & ~0xFFu) | (1 + Next() % 255);
    if (!(flags & 2) && Next() % 4 == 0) speed &= ~0xFFu;
    return object;
}

void SelfTest(void* const (&theirs)[kCount]) {
    constexpr unsigned kRounds = 30000;
    unsigned region_bytes = 0;
    for (const Region& r : kRegions) region_bytes += r.size;
    if (region_bytes != kRegionBytes) bof3::Fatal("object_kinds: regions are %u bytes, the state holds %u", region_bytes, kRegionBytes);
    static State saved, input, their_out, our_out;
    unsigned char* const saved_object = g_current_object;
    Capture(saved, 0);
    const unsigned char* const* const scripts = g_scripts;
    std::memcpy(g_descriptor + 4, &scripts, sizeof scripts);
    for (unsigned i = 0; i < 0x1000; ++i) g_scripts[i] = At(0x20000u + i * 0x10u);
    g = Stubs();

    unsigned bad = 0, calls = 0, per[kCount] = {};
    for (unsigned round = 0; round < kRounds; ++round) {
        const unsigned k = round % kCount;
        ++per[k];
        unsigned speed = 0, flags = 0;
        unsigned char* const object = Seed(k, input, speed, flags);
        Capture(input, 0);
        unsigned char* const saved_descriptor = Area_Descriptors[input.area];
        Area_Descriptors[input.area] = g_descriptor;
        for (int pass = 0; pass < 2; ++pass) {
            Apply(input);
            g_current_object = object;
            const void* const fn = pass ? Ours(k) : theirs[k];
            std::uint32_t result = 0;
            if (k == kLinked) {
                reinterpret_cast<VoidFn>(const_cast<void*>(fn))();
            } else if (k >= kPace0 && k <= kPace6) {
                result = reinterpret_cast<PaceCallFn>(const_cast<void*>(fn))(object, speed, flags) & 0xFF;
            } else if (k == kTalk) {
                result = reinterpret_cast<ByteFn>(const_cast<void*>(fn))(object + 0x80) & 0xFF;
            } else if (k >= kOpenDirection) {
                result = reinterpret_cast<ByteFn>(const_cast<void*>(fn))(object) & 0xFF;
            } else {
                reinterpret_cast<ObjectFn>(const_cast<void*>(fn))(object);
            }
            Capture(pass ? our_out : their_out, result);
        }
        Area_Descriptors[input.area] = saved_descriptor;
        calls += their_out.log_n;
        if (std::memcmp(&their_out, &our_out, sizeof their_out) != 0 && ++bad <= 12)
            bof3::Log("shadow      object_kinds self-test MISMATCH: round %u, %s, log %u / %u, result 0x%X / 0x%X", round, kClones[k].name,
                      their_out.log_n, our_out.log_n, (unsigned)their_out.result, (unsigned)our_out.result);
    }
    g = Originals();
    Apply(saved);
    g_current_object = saved_object;
    bof3::Log("shadow      object_kinds self-test: %u rounds (%u per function, %u functions), %u calls to the stand-ins, %u MISMATCHES; "
              "the object, sprite, member, Sprite_Objects to MoveScript_TintRecords, the globals, the result byte and the stand-ins' "
              "log compared", kRounds, per[0], kCount, calls, bad);
    if (bad) bof3::Fatal("the object kind handlers differ from the original in %u of %u self-test rounds", bad, kRounds);
}

template <int... K>
void FillPaceStubs(std::integer_sequence<int, K...>) {
    const PaceFn eight[] = {&StubPace<K>...};
    for (unsigned i = 0; i < 256; ++i) g_pace_stubs[i] = eight[i % 8];
}

}  // namespace

void ObjectKinds_Inject() {
    if (bof3::WantsShadow("object_kinds")) {
        FillPaceStubs(std::make_integer_sequence<int, 8>{});
        void* clones[kCount];
        for (unsigned k = 0; k < kCount; ++k) {
            const Clone& c = kClones[k];
            if (k == kFadeIn) {
                clones[k] = static_cast<std::uint8_t*>(clones[kFadeOut]) + kFadeInOffset;
                continue;
            }
            bof3::CloneCall calls[7];
            for (int i = 0; i < c.n_calls; ++i) calls[i] = {c.calls[i].offset, StubFor(c.calls[i].target)};
            clones[k] = bof3::CloneOriginal(c.name, c.base, c.size, calls, c.n_calls);
            for (int i = 0; i < c.n_pace; ++i) Patch32(clones[k], c.pace[i], kPaceTable, Address(g_pace_stubs), c.name);
        }
        // The fades: the four case addresses of 0x65F654 / 0x65F65C, moved.
        const std::uint32_t moved = Address(clones[kFadeOut]) - kFadeBase;
        for (unsigned i = 0; i < 4; ++i) {
            std::uint32_t target;
            std::memcpy(&target, At(0x65F654 + 4 * i), sizeof target);
            if (target < kFadeBase || target >= kFadeBase + kFadeSize) bof3::Fatal("object_kinds: fade case %u is 0x%X", i, (unsigned)target);
            g_fade_table[i] = target + moved;
        }
        Patch32(clones[kFadeOut], 0xE, 0x65F654, Address(&g_fade_table[0]), "Field_ObjectFadeOut");
        Patch32(clones[kFadeOut], 0x13E, 0x65F65C, Address(&g_fade_table[2]), "Field_ObjectFadeIn");
        SelfTest(clones);
    }
    BOF3_INJECT(Field_ObjectWander);
    BOF3_INJECT(Field_ObjectApproach);
    BOF3_INJECT(Field_ObjectAvoid);
    BOF3_INJECT(Field_ObjectWanderHome);
    BOF3_INJECT(Field_ObjectWait);
    BOF3_INJECT(Field_ObjectStill);
    BOF3_INJECT(Field_ObjectFadeOut);
    BOF3_INJECT(Field_ObjectFadeIn);
    BOF3_INJECT(Field_ObjectTurn);
    BOF3_INJECT(Field_ObjectLinked);
    BOF3_INJECT(Field_ObjectIdleLong);
    BOF3_INJECT(Field_ObjectIdle);
    BOF3_INJECT(MoveScript_SetTurnTarget);
    BOF3_INJECT(Field_Pace0);
    BOF3_INJECT(Field_Pace4);
    BOF3_INJECT(Field_Pace5);
    BOF3_INJECT(Field_Pace6);
    BOF3_INJECT(Field_ObjectOpenDirection);
    BOF3_INJECT(Field_ObjectRandomTurn);
    BOF3_INJECT(Field_ObjectInHome);
}

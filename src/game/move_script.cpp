#include "game/move_script.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>

#include "bof3/symbols.gen.h"
#include "hook/detour.h"
#include "hook/log.h"

#include "game/move_script_bytes.h"

namespace {

using namespace move_script;

using AreaHandler = void (__cdecl*)();
using CounterOp = signed char (__cdecl*)(const unsigned char*);
using GroupFn = unsigned char (__cdecl*)(unsigned char*, const unsigned char*, unsigned char);

// Every callee that is not one of these three, through pointers so that the
// start-up fuzz can stand recording functions in for them - for the
// original's copy and for ours alike. The two tables the flow pass calls
// through (the area's handlers, MoveScript_CounterOps) are read from memory,
// as the original reads them; the fuzz swaps their contents instead.
struct Callees {
    int (__cdecl* kind)();
    void (__cdecl* move)(unsigned char*, unsigned char);
    void (__cdecl* move_kind2)(unsigned char);
    GroupFn group6, group8, group9, group_c, group_d, group_e;
    void (__cdecl* group_b)(unsigned char*, const unsigned char*);
    unsigned short (__cdecl* find_label)(unsigned char*, unsigned char, const unsigned char*);
    int (__cdecl* variable)(unsigned char);
    void (__cdecl* trio_clear)();
    void (__cdecl* trio_set)();
    long (__cdecl* elevation)(long, long);
    void (__cdecl* set_animation)(unsigned char);
    void (__cdecl* set_animation_at)(unsigned char, unsigned char);
    void (__cdecl* flags_clear)();
    void (__cdecl* flags_set)();
    int (__cdecl* wait_test)(unsigned char, unsigned char);
    unsigned char (__cdecl* tick_once)();
    unsigned short (__cdecl* peek)();
    void (__cdecl* sound)(unsigned short);
    void (__cdecl* op_f7)(long, long, unsigned char, unsigned char*);
    void (__cdecl* attach)(unsigned char*, unsigned char, unsigned char, unsigned char, unsigned char);
    unsigned char (__cdecl* test_fb)(short, short);
    unsigned char (__cdecl* test_fc)(short, short);
};
const Callees kOriginals = {
    MoveScript_ObjectKind, MoveCmd_Move, MoveCmd_MoveKind2,
    MoveScript_Group6, MoveScript_Group8, MoveScript_Group9, MoveScript_GroupC, MoveScript_GroupD, MoveScript_GroupE,
    MoveScript_GroupB, MoveScript_FindLabel, MoveScript_Variable, ObjTrio_ClearBit40, ObjTrio_SetBit40,
    AreaMap_Elevation, Sprite_SetAnimation, Sprite_SetAnimationAt, ScriptFlags_Clear40, ScriptFlags_Set40,
    MoveScript_WaitTest, Sprite_ScriptTickOnce, Sprite_ScriptPeek, Sound_PlayEffect, MoveCmd_OpF7,
    MoveCmd_Attach, MoveCmd_TestFB, MoveCmd_TestFC,
};
Callees g = kOriginals;

// Ops 0D and F3 place an object: the coordinate words of Sprite_Current, or
// of Sprite_Kind2 itself on the kind-2 path.
void PlaceAndGround(unsigned char* sprite, const unsigned char* operands) {
    SetWord(sprite + 0x36, operands[1]);
    SetWord(sprite + 0x34, operands[2] ? 0x8000u : 0u);
    SetWord(sprite + 0x3A, operands[3]);
    SetWord(sprite + 0x38, operands[4] ? 0x8000u : 0u);
}

}  // namespace

// original 0x576E00: the flow-control pass that runs before every step
// (PSX FUN_801A9F94). It loops until the byte at the position has a high
// nibble other than 0x00, 0xA0 and 0xB0, and returns `value` - replaced by
// Sprite_Current[8] after op 03 and by 0xFF after op 0D on the kind-2 path.
//
// As the original has it: byte 00 is not one of the cases and loops for ever
// on the same position (unreachable by the fuzz; by reading); the position is
// read again after every call that could move it, and every position store
// is 16 bits; the area number indexes Area_Descriptors unchecked.
extern "C" unsigned char __cdecl MoveScript_Flow(unsigned char* object, const unsigned char* script, unsigned char value) {
    unsigned char result = value;
    for (;;) {
        const std::uint16_t pos = Pos(object);
        const unsigned char* const p = script + pos;
        const unsigned char op = p[0];
        const unsigned group = op & 0xF0u;
        if (group >= 0x10) {
            if (group == 0xA0) {
                const auto ops = reinterpret_cast<CounterOp*>(MoveScript_CounterOps);
                const signed char skip = ops[op & 3](p);
                if (skip < 0) return result;
                SetPos(object, Pos(object) + static_cast<unsigned>(skip) + 1u);
            } else if (group == 0xB0) {
                g.group_b(object, script);
                SetPos(object, Pos(object) + 1u);
            } else {
                return result;
            }
            continue;
        }

        switch (op) {
        case 0x01: {   // 01 hi lo: jump by hi:lo, or to label lo when hi's top two bits are 01
            const unsigned char hi = p[1];
            const unsigned target = static_cast<unsigned>(hi) << 8 | p[2];
            if ((hi & 0xC0) == 0x40) {
                SetPos(object, target);
                SetPos(object, g.find_label(object, p[2], script));
            } else {
                SetPos(object, target + pos);
            }
            break;
        }
        case 0x02: {   // 02 d n: loop back by d, n times, counting in object +2
            const unsigned char counter = object[2];
            if (counter == 0) {
                object[2] = p[2];
                SetPos(object, pos + static_cast<unsigned>(static_cast<signed char>(p[1])));
            } else {
                object[2] = static_cast<unsigned char>(counter - 1);
                if (object[2] == 0) SetPos(object, pos + 3u);
                else SetPos(object, pos + static_cast<unsigned>(static_cast<signed char>(p[1])));
            }
            break;
        }
        case 0x03: {   // 03 n: the area's handler n
            const unsigned char* const descriptor = Area_Descriptors[Game_AreaNumber];
            AreaHandler const* handlers;
            std::memcpy(&handlers, descriptor + 0x3C, sizeof handlers);
            handlers[p[1]]();
            result = Sprite_Current[8];
            SetPos(object, Pos(object) + 2u);
            break;
        }
        case 0x04: case 0x05: case 0x06: case 0x07: case 0x08: case 0x09: {
            // op a b l1h l1l l2h l2l: compare, then go to label l1l or l2l. The
            // even ops take a as a constant, the odd ones as a variable.
            const int want = (op & 1) ? g.variable(p[1]) : p[1];
            const int have = g.variable(script[Pos(object) + 2u]);
            bool first;
            if (op <= 0x05) first = have == want;
            else if (op <= 0x07) first = have < want;
            else first = (want & have) != 0;
            const unsigned at = Pos(object);
            const unsigned char hi = script[at + (first ? 3u : 5u)];
            const unsigned char lo = script[at + (first ? 4u : 6u)];
            SetPos(object, static_cast<unsigned>(hi) << 8 | lo);
            SetPos(object, g.find_label(object, lo, script));
            break;
        }
        case 0x0A:
            SetPos(object, pos + 2u);
            break;
        case 0x0B:
            SetPos(object, 0);
            break;
        case 0x0C:
            if (p[1]) g.trio_clear();
            else g.trio_set();
            SetPos(object, Pos(object) + 2u);
            break;
        case 0x0D: {   // 0D x xh z zh: place at (x.xh, z.zh) and take the ground's elevation
            if (g.kind() == 2) {
                PlaceAndGround(Sprite_Kind2, script + Pos(object));
                SetWord(Sprite_Kind2 + 0x3E, static_cast<unsigned>(g.elevation(Long(Sprite_Kind2 + 0x34), Long(Sprite_Kind2 + 0x38))));
                result = 0xFF;
            } else {
                PlaceAndGround(Sprite_Current, script + Pos(object));
                const long h = g.elevation(Long(Sprite_Current + 0x34), Long(Sprite_Current + 0x38));
                SetWord(Sprite_Current + 0x3E, static_cast<unsigned>(h));
            }
            SetPos(object, Pos(object) + 5u);
            break;
        }
        case 0x0E: case 0x0F: {   // op a key flags [start]: an animation, by number or by the field's state
            const unsigned char animation = op == 0x0E
                ? p[1] : MoveScript_AnimByState[p[1] * 11u + Field_State[0x89]];
            object[0] |= 0x40;
            Sprite_Current[7] |= 8;
            if (script[Pos(object) + 3u] & 1) Sprite_Current[0] |= 0x10;
            else Sprite_Current[0] &= 0xEF;
            Sprite_Current[0x2A] = script[Pos(object) + 2u];
            const unsigned at = Pos(object);
            if (script[at + 3] & 2) {
                g.set_animation_at(animation, script[at + 4]);
                SetPos(object, Pos(object) + 1u);
                SetPos(object, Pos(object) + 4u);
            } else {
                g.set_animation(animation);
                SetPos(object, Pos(object) + 4u);
            }
            break;
        }
        default:
            break;   // 00: the same position again, for ever
        }
    }
}

// original 0x577760: the 0xF0 group (PSX FUN_801AADE8), called by the step
// with the position on the op; the step adds one after it. Returns `value`,
// replaced by b1 for F2 / F3 / FA and b6 for F7; F9 returns 0xFF while it
// waits, else Sprite_Current[8] & 7.
//
// As the original has it: F3 divides by Sprite_Current[9] times object +7 on
// one path and by (0x80 / MoveScript_F3Divisor) times Sprite_Kind2 +0x87 on
// the other, unguarded - a zero there faults, as the original does (by
// reading; the fuzz keeps them non-zero); the kind-2 path shifts a 16-bit
// difference into the top half, so a difference past 0x7FFF wraps.
extern "C" unsigned char __cdecl MoveScript_GroupF(unsigned char* object, const unsigned char* script, unsigned char value) {
    const std::uint16_t pos = Pos(object);
    const unsigned char* const p = script + pos;
    if (p[0] < 0xF0) return value;   // unreachable from the step
    switch (p[0]) {
    case 0xF0:
        g.flags_clear();
        return value;
    case 0xF1:
        g.flags_set();
        return value;
    case 0xF2: case 0xFA: case 0xF3: {
        value = p[1];
        object[7] = p[2];
        const bool moves = p[2] != 0;
        if (p[0] == 0xF2) {
            if (moves) {
                if (g.kind() == 2) g.move_kind2(value);
                else g.move(object, value);
            }
            SetPos(object, Pos(object) + 2u);
        } else if (p[0] == 0xFA) {
            if (moves) {
                if (g.kind() == 2) {
                    g.move_kind2(value);
                    MoveScript_FAWord = 0;
                    object[0] |= 0x40;
                } else {
                    g.move(object, value);
                    object[0] |= 0x80;
                }
            }
            SetLong(Sprite_Current + 0x14, 0);
            const unsigned at = Pos(object);
            SetWord(Sprite_Current + 0x3E, static_cast<unsigned>(script[at + 3]) << 8 | script[at + 4]);
            SetPos(object, Pos(object) + 4u);
        } else if (moves) {   // F3 d m th tl: move toward height th.tl, over the move's frames
            if (g.kind() != 2) {
                g.move(object, value);
                const unsigned at = Pos(object);
                const auto target = static_cast<std::int16_t>(script[at + 3] << 8 | script[at + 4]);
                unsigned char* const sprite = Sprite_Current;
                const std::int32_t rise = target - static_cast<std::int16_t>(Word(sprite + 0x3E));
                const std::int32_t frames = static_cast<std::int32_t>(sprite[9]) * object[7];
                SetLong(sprite + 0x14, rise / frames);
                object[0] |= 0x80;
                SetPos(object, Pos(object) + 4u);
            } else {
                unsigned char* const scratch = Scratch();
                SetWord(scratch, Word(Sprite_Current + 0x3E));
                const unsigned at = Pos(object);
                SetWord(scratch + 2, static_cast<unsigned>(script[at + 3]) << 8 | script[at + 4]);
                g.move_kind2(value);
                const std::int32_t frames = 0x80 / static_cast<std::int32_t>(MoveScript_F3Divisor) * Sprite_Kind2[0x87];
                const std::int32_t rise = static_cast<std::int16_t>(Word(scratch + 2)) - static_cast<std::int16_t>(Word(scratch));
                SetLong(Sprite_Kind2 + 0x14, static_cast<std::int32_t>(static_cast<std::uint32_t>(rise) << 16) / frames);
                object[0] |= 0x40;
                SetPos(object, Pos(object) + 4u);
            }
        } else {
            SetPos(object, Pos(object) + 4u);
        }
        return value;
    }
    case 0xF4: {   // F4 th tl n: rise to th.tl over n frames (0 counts as 1)
        object[0] |= 0x80;
        Sprite_Current[9] = p[3];
        if (Sprite_Current[9] == 0) Sprite_Current[9] = 1;
        unsigned char* const sprite = Sprite_Current;
        const unsigned at = Pos(object);
        const auto target = static_cast<std::int16_t>(script[at + 1] << 8 | script[at + 2]);
        const std::int32_t rise = target - static_cast<std::int16_t>(Word(sprite + 0x3E));
        SetLong(sprite + 0x14, rise / static_cast<std::int32_t>(sprite[9]));
        SetLong(Sprite_Current + 0xC, 0);
        SetLong(Sprite_Current + 0x10, 0);
        SetPos(object, Pos(object) + 3u);
        return value;
    }
    case 0xF5:
        object[0] &= 0x7F;
        return value;
    case 0xF6:
        MoveScript_Var7 = static_cast<signed char>(p[1]);
        SetPos(object, Pos(object) + 1u);
        return value;
    case 0xF7: {
        const long x = static_cast<long>((static_cast<unsigned>(p[1]) << 8 | p[2]) << 8);
        const long z = static_cast<long>((static_cast<unsigned>(p[3]) << 8 | p[4]) << 8);
        g.op_f7(x, z, p[5], object);
        value = script[Pos(object) + 6u];
        if (g.kind() == 1) object[7] = 1;
        SetPos(object, Pos(object) + 6u);
        return value;
    }
    case 0xF8:
        g.attach(object, p[1], p[2], p[3], p[4]);
        SetPos(object, Pos(object) + 1u);
        return value;
    case 0xF9:
        if (g.wait_test(p[1], p[2]) != 0) {
            SetPos(object, Pos(object) - 1u);
            return 0xFF;
        }
        SetPos(object, Pos(object) + 2u);
        return static_cast<unsigned char>(Sprite_Current[8] & 7);
    case 0xFB: case 0xFC: {
        unsigned char* const sprite = Sprite_Current;
        const auto x = static_cast<short>(Word(sprite + 0x36)), z = static_cast<short>(Word(sprite + 0x3A));
        if ((p[0] == 0xFB ? g.test_fb : g.test_fc)(x, z)) g.sound(0x103);
        return value;
    }
    case 0xFD:
        if (!g.tick_once()) {
            Sprite_Current[7] |= 0x20;
            SetPos(object, Pos(object) - 1u);
        }
        return value;
    case 0xFE:
        if (Field_Request != 3) {
            const std::uint16_t next = g.peek();
            SetWord(Scratch(), next);
            if (Word(Sprite_Current + 0x58) != next) {
                const unsigned at = Pos(object);
                if (next == script[at + 2]) g.sound(static_cast<unsigned short>(script[at + 1] + 0x200u));
            }
        }
        SetPos(object, Pos(object) + 2u);
        return value;
    default:   // FF: the end - the script stays on it
        SetPos(object, pos - 1u);
        return value;
    }
}

// original 0x576B50: one step of an object's movement script (PSX
// FUN_801A9D38). The flow pass first, then one op by its high nibble, then
// position + 1 - read again after the op, since every handler may move it.
// Returns al: the flow pass's value, the op's group for 0x10-0x4F, or what
// the group handler returned.
extern "C" unsigned char __cdecl MoveScript_Step(unsigned char* object, const unsigned char* script) {
    const unsigned char start = Sprite_Current[8];
    MoveScript_Object = object;
    unsigned char result = MoveScript_Flow(object, script, start);
    const std::uint16_t pos = Pos(object);
    const unsigned char op = script[pos];
    switch (op & 0xF0) {
    case 0x10: case 0x20: case 0x30: case 0x40: {   // move in direction (op - 0x10) >> 3, op & 7 steps
        result = static_cast<unsigned char>(static_cast<unsigned char>(op - 0x10) >> 3);
        object[7] = op & 7;
        if (op & 7) {
            if (g.kind() == 2) g.move_kind2(result);
            else g.move(object, result);
        }
        break;
    }
    case 0x50:
        object[1] = op & 0xF;
        break;
    case 0x60: result = g.group6(object, script, result); break;
    case 0x80: result = g.group8(object, script, result); break;
    case 0x90: result = g.group9(object, script, result); break;
    case 0xC0: result = g.group_c(object, script, result); break;
    case 0xD0: result = g.group_d(object, script, result); break;
    case 0xE0: result = g.group_e(object, script, result); break;
    case 0xF0: result = MoveScript_GroupF(object, script, result); break;
    case 0xA0:   // A0-AF that the flow pass stopped on: stay
        SetPos(object, pos - 1u);
        break;
    default:     // 0x70, and the high nibbles the flow pass never stops on
        break;
    }
    SetPos(object, Pos(object) + 1u);
    return result;
}

// originals 0x57C460, 0x57C480, 0x57C4A0: the counters of flow ops A0..AF,
// reached through MoveScript_CounterOps by op & 3. Counter (op >> 2) & 3 of
// the four bytes at 0x903848, below the PSX scratchpad's words. Set returns 1
// (skip the operand), test 1 when the counter equals the operand and -1 -
// stop here - when not, step 0 (no operand).
namespace {
unsigned char* Counter(const unsigned char* op) { return Scratch() - 8 + ((op[0] >> 2) & 3); }
}  // namespace
extern "C" signed char __cdecl MoveScript_CounterSet(const unsigned char* op) {
    *Counter(op) = op[1];
    return 1;
}
extern "C" signed char __cdecl MoveScript_CounterTest(const unsigned char* op) {
    return *Counter(op) == op[1] ? 1 : -1;
}
extern "C" signed char __cdecl MoveScript_CounterStep(const unsigned char* op) {
    ++*Counter(op);
    return 0;
}

namespace {

// --- BOF3X_SHADOW=move_script: a differential fuzz, once at start-up ---------
// Two byte-copies: 0x576B50..0x577280 (the step and the flow pass, with their
// two jump tables moved into the copy) and 0x577760..0x577BD0 (the 0xF0 group
// and its table). The step's call of the group goes to the second copy;
// every other call, and both tables of pointers the flow pass calls through,
// go to the recorders below. One round: a generated script whose flow
// control always reaches an op the step dispatches, a random script context,
// sprite, kind-2 object and field state; theirs, then from the same state
// ours; everything either side writes, the result and the recorders' log
// compared.
//
// What the fuzz must NOT generate, because the original never returns: a
// flow path that reaches byte 00 or runs round a loop. So the flow ops form a
// chain whose every branch goes forward - the main branch to the next flow op,
// the other to a stop byte - and label searches and A0-A3 skips come from
// tables the generator fills. Divisors of F3 are kept non-zero.

constexpr unsigned kScript = 0x10010, kBuf = 0x100, kLog = 256, kWindow = 0x180;
unsigned char g_script[kScript];
unsigned char g_object[kBuf], g_sprite[kBuf], g_state[kBuf];
unsigned char g_descriptor[0x44];
std::uint16_t g_label_target[256];
signed char g_skip[0x10000];   // what MoveScript_CounterOps returns for an A op at a position
struct Entry { std::uint32_t what, a, b, c, d; };
Entry g_log[kLog];
unsigned g_log_n, g_seed;

std::uint32_t Hash() {
    std::uint32_t h = (g_seed + g_log_n) * 0x9E3779B1u;
    h ^= h >> 15;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    return h;
}
void Record(std::uint32_t what, std::uint32_t a = 0, std::uint32_t b = 0, std::uint32_t c = 0, std::uint32_t d = 0) {
    if (g_log_n < kLog) g_log[g_log_n] = {what, a, b, c, d};
    ++g_log_n;
}
std::uint32_t Rel(const void* p) {
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p) - reinterpret_cast<std::uintptr_t>(g_script));
}
// A single-shot callee may move the position: the caller must read it again.
void Nudge(unsigned char* object) {
    const std::uint32_t h = Hash();
    if (h % 3 == 0) SetPos(object, Pos(object) + (h >> 8) % 7);
}

int __cdecl StubKind() {
    const std::uint32_t h = Hash();
    Record(1);
    return h % 8 < 3 ? 2 : static_cast<int>(h % 4);
}
void __cdecl StubMove(unsigned char* object, unsigned char direction) {
    Record(2, Rel(object), direction);
    const std::uint32_t h = Hash();
    Sprite_Current[9] = static_cast<unsigned char>(1 + h % 255);
    if (h & 0x100) object[7] = static_cast<unsigned char>(1 + (h >> 9) % 255);
    Nudge(object);
}
void __cdecl StubMoveKind2(unsigned char direction) {
    Record(3, direction);
    const std::uint32_t h = Hash();
    MoveScript_F3Divisor = static_cast<short>(1 + h % 0x80);
    Sprite_Kind2[0x87] = static_cast<unsigned char>(1 + (h >> 8) % 255);
    if (h & 0x10000) SetWord(Scratch() + 2, h >> 17);
    Nudge(MoveScript_Object);
}
template <unsigned Id>
unsigned char __cdecl StubGroup(unsigned char* object, const unsigned char* script, unsigned char value) {
    Record(Id, Rel(object), Rel(script), value, Pos(object));
    Nudge(object);
    return static_cast<unsigned char>(Hash());
}
void __cdecl StubGroupB(unsigned char* object, const unsigned char* script) { Record(20, Rel(object), Rel(script), Pos(object)); }
unsigned short __cdecl StubFindLabel(unsigned char* object, unsigned char label, const unsigned char* script) {
    Record(21, Rel(object), label, Rel(script), Pos(object));
    return g_label_target[label];
}
int __cdecl StubVariable(unsigned char index) {
    Record(22, index);
    const std::uint32_t h = Hash();
    return h % 4 ? static_cast<int>(h >> 8) % 4 : static_cast<int>(h);
}
void __cdecl StubTrioClear() { Record(23); }
void __cdecl StubTrioSet() { Record(24); }
long __cdecl StubElevation(long x, long y) {
    Record(25, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y));
    return static_cast<long>(Hash());
}
void __cdecl StubSetAnimation(unsigned char a) { Record(26, a); }
void __cdecl StubSetAnimationAt(unsigned char a, unsigned char s) { Record(27, a, s); }
void __cdecl StubFlagsClear() { Record(28); }
void __cdecl StubFlagsSet() { Record(29); }
int __cdecl StubWaitTest(unsigned char a, unsigned char b) {
    Record(30, a, b);
    return Hash() % 2 ? static_cast<int>(Hash() | 0x100u) : 0;   // any non-zero, not only al
}
unsigned char __cdecl StubTickOnce() {
    Record(31);
    return static_cast<unsigned char>(Hash() % 2);
}
// Often the value the op compares with - the sprite's own +0x58, or the op's
// second operand, or both at once (it stores the operand to +0x58 first) - so
// that FE's two tests pass and fail in every combination. Without the third
// case a control that drops the +0x58 test went unseen: a random word rarely
// equals a byte.
unsigned short __cdecl StubPeek() {
    Record(32);
    const std::uint32_t h = Hash();
    const unsigned char operand = g_script[Pos(MoveScript_Object) + 2u];
    switch (h % 4) {
    case 0: return Word(Sprite_Current + 0x58);
    case 1: return operand;
    case 2: SetWord(Sprite_Current + 0x58, operand); return operand;
    default: return static_cast<unsigned short>(h >> 8);
    }
}
void __cdecl StubSound(unsigned short id) { Record(33, id); }
void __cdecl StubOpF7(long x, long z, unsigned char c, unsigned char* object) {
    Record(34, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(z), c, Rel(object));
    Nudge(object);
}
void __cdecl StubAttach(unsigned char* object, unsigned char m, unsigned char h, unsigned char s, unsigned char t) {
    Record(35, Rel(object), m, h, static_cast<std::uint32_t>(s) << 8 | t);
    Nudge(object);
}
unsigned char __cdecl StubTestFB(short x, short z) {
    Record(36, static_cast<std::uint16_t>(x), static_cast<std::uint16_t>(z));
    return static_cast<unsigned char>(Hash() % 2);
}
unsigned char __cdecl StubTestFC(short x, short z) {
    Record(37, static_cast<std::uint16_t>(x), static_cast<std::uint16_t>(z));
    return static_cast<unsigned char>(Hash() % 2);
}
template <unsigned I>
void __cdecl StubArea() {
    Record(40, I);
    Sprite_Current[8] = static_cast<unsigned char>(Hash());
}
template <unsigned I>
signed char __cdecl StubCounter(const unsigned char* p) {
    Record(50 + I, Rel(p));
    return g_skip[Rel(p) & 0xFFFF];
}
template <std::size_t... I>
constexpr std::array<void*, sizeof...(I)> AreaStubs(std::index_sequence<I...>) {
    return {{reinterpret_cast<void*>(&StubArea<I>)...}};
}
const auto kAreaStubs = AreaStubs(std::make_index_sequence<256>());
void* const kCounterStubs[4] = {reinterpret_cast<void*>(&StubCounter<0>), reinterpret_cast<void*>(&StubCounter<1>),
                                reinterpret_cast<void*>(&StubCounter<2>), reinterpret_cast<void*>(&StubCounter<3>)};

const Callees kStubs = {
    StubKind, StubMove, StubMoveKind2,
    StubGroup<6>, StubGroup<8>, StubGroup<9>, StubGroup<12>, StubGroup<13>, StubGroup<14>,
    StubGroupB, StubFindLabel, StubVariable, StubTrioClear, StubTrioSet,
    StubElevation, StubSetAnimation, StubSetAnimationAt, StubFlagsClear, StubFlagsSet,
    StubWaitTest, StubTickOnce, StubPeek, StubSound, StubOpF7, StubAttach, StubTestFB, StubTestFC,
};

// Where each copy's calls go: the E8 offsets, by capstone 2026-09-22 (every
// relative call of the two ranges; every jump stays inside).
constexpr std::uint32_t kStep = 0x576B50, kStepSize = 0x730, kGroupF = 0x577760, kGroupFSize = 0x470;
constexpr Table kStepTables[] = {{0x64, 0x1A0, 11}, {0x327, 0x6E8, 15}};
constexpr Table kGroupFTable = {0x2E, 0x430, 16};

struct State {
    unsigned char object[kBuf], sprite[kBuf], kind2[Sprite_Kind2_count], scratch[12];
    short divisor;
    std::uint16_t fa_word, area;
    signed char var7;
    unsigned char request;
    unsigned char* current;
    unsigned char* move_object;
    Entry log[kLog];
    unsigned log_n;
    unsigned char result;
};
unsigned char* ScratchLow() { return Scratch() - 8; }   // 0x903848: the counters of the A ops, then the two words

void Capture(State& s) {
    std::memcpy(s.object, g_object, kBuf);
    std::memcpy(s.sprite, g_sprite, kBuf);
    std::memcpy(s.kind2, Sprite_Kind2, sizeof s.kind2);
    std::memcpy(s.scratch, ScratchLow(), sizeof s.scratch);
    s.divisor = MoveScript_F3Divisor; s.fa_word = MoveScript_FAWord; s.area = Game_AreaNumber;
    s.var7 = MoveScript_Var7; s.request = Field_Request;
    s.current = Sprite_Current; s.move_object = MoveScript_Object;
    std::memcpy(s.log, g_log, sizeof s.log);
    s.log_n = g_log_n;
}
void Apply(const State& s) {
    std::memcpy(g_object, s.object, kBuf);
    std::memcpy(g_sprite, s.sprite, kBuf);
    std::memcpy(Sprite_Kind2, s.kind2, sizeof s.kind2);
    std::memcpy(ScratchLow(), s.scratch, sizeof s.scratch);
    MoveScript_F3Divisor = s.divisor; MoveScript_FAWord = s.fa_word; Game_AreaNumber = s.area;
    MoveScript_Var7 = s.var7; Field_Request = s.request;
    Sprite_Current = s.current; MoveScript_Object = s.move_object;
    std::memset(g_log, 0, sizeof g_log);
    g_log_n = 0;
}

std::uint32_t g_rng = 0x6D2B79F5u;
std::uint32_t Next() { g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5; return g_rng; }

// A byte the flow pass stops on: any high nibble but 0x00, 0xA0 and 0xB0,
// half of them the 0xF0 group.
unsigned char StopByte() {
    if (Next() % 2) return static_cast<unsigned char>(0xF0 | Next() % 16);
    static const unsigned char kGroups[] = {0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80, 0x90, 0xC0, 0xD0, 0xE0};
    return static_cast<unsigned char>(kGroups[Next() % 12] | Next() % 16);
}

// Lays out the flow chain from `start` and returns its op count. Main
// branches go to the next op; other branches to a stop byte of their own.
unsigned g_ops_seen[18];   // 01..0F by op, 16 the A ops, 17 the B ops
unsigned Generate(unsigned start) {
    unsigned at = start, ops = 0;
    unsigned label = 0;
    const unsigned chain = Next() % 8;
    for (unsigned k = 0; k < chain && at < 0xFF00; ++k) {
        static const unsigned char kOps[] = {1, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF, 0xA0, 0xB0};
        unsigned char op = kOps[Next() % sizeof kOps];
        if (op == 0xA0) op = static_cast<unsigned char>(0xA0 | Next() % 16);
        if (op == 0xB0) op = static_cast<unsigned char>(0xB0 | Next() % 16);
        unsigned char* const p = g_script + at;
        ++ops;
        ++g_ops_seen[op < 0x10 ? op : (op >> 4) + 6];
        switch (op) {
        case 0x01: {
            const unsigned next = at + 3 + Next() % 8;
            if (Next() % 2) {   // a label
                p[1] = static_cast<unsigned char>(0x40 | Next() % 0x40);
                p[2] = static_cast<unsigned char>(label);
                g_label_target[label++] = static_cast<std::uint16_t>(next);
            } else {
                const unsigned delta = next - at;
                p[1] = static_cast<unsigned char>(delta >> 8);
                p[2] = static_cast<unsigned char>(delta);
            }
            at = next;
            break;
        }
        case 0x02: {
            // The loop back is forward here: to the next op, 3 bytes on - or
            // further, with a stop byte at pos + 3 for the counter's end.
            const unsigned d = Next() % 2 ? 3 : 4 + Next() % 8;
            p[1] = static_cast<unsigned char>(d);
            p[2] = static_cast<unsigned char>(Next());
            if (d != 3) g_script[at + 3] = StopByte();
            at += d;
            break;
        }
        case 0x03:
            p[1] = static_cast<unsigned char>(Next());
            at += 2;
            break;
        case 0x04: case 0x05: case 0x06: case 0x07: case 0x08: case 0x09: {
            for (unsigned i = 1; i <= 6; ++i) p[i] = static_cast<unsigned char>(Next());
            const unsigned other = at + 7, next = at + 8 + Next() % 4;
            p[4] = static_cast<unsigned char>(label);
            g_label_target[label++] = static_cast<std::uint16_t>(next);
            p[6] = static_cast<unsigned char>(label);
            g_label_target[label++] = static_cast<std::uint16_t>(other);
            g_script[other] = StopByte();
            at = next;
            break;
        }
        case 0x0A: at += 2; break;
        case 0x0B:
            g_script[0] = StopByte();
            p[0] = op;
            return ops;
        case 0x0C:
            p[1] = Next() % 2 ? 0 : static_cast<unsigned char>(Next());
            at += 2;
            break;
        case 0x0D:
            for (unsigned i = 1; i <= 4; ++i) p[i] = Next() % 3 ? static_cast<unsigned char>(Next()) : 0;
            at += 5;
            break;
        case 0x0E: case 0x0F:
            for (unsigned i = 1; i <= 4; ++i) p[i] = static_cast<unsigned char>(Next());
            if (op == 0x0F) p[1] = static_cast<unsigned char>(Next() % 3);
            at += (p[3] & 2) ? 5 : 4;
            break;
        default:
            if ((op & 0xF0) == 0xA0) {
                if (Next() % 4 == 0) { g_skip[at] = -1 - static_cast<signed char>(Next() % 100); p[0] = op; return ops; }
                const unsigned skip = Next() % 6;
                g_skip[at] = static_cast<signed char>(skip);
                p[1] = static_cast<unsigned char>(Next());
                at += skip + 1;
            } else {   // B0
                at += 1;
            }
            break;
        }
        p[0] = op;
        // A chain op's operands must not be taken for the next op: the next is always past them.
    }
    g_script[at] = StopByte();
    return ops;
}

void SelfTest(unsigned char (__cdecl* theirs)(unsigned char*, const unsigned char*)) {
    constexpr unsigned kRounds = 20000;
    static State saved, input, their_out, our_out;
    Capture(saved);
    const std::uint16_t saved_area = Game_AreaNumber;
    unsigned char* const saved_state = Field_State;
    void* saved_counter_ops[4];
    std::memcpy(saved_counter_ops, MoveScript_CounterOps, sizeof saved_counter_ops);
    std::memcpy(MoveScript_CounterOps, kCounterStubs, sizeof kCounterStubs);
    const void* const area_table = kAreaStubs.data();
    std::memcpy(g_descriptor + 0x3C, &area_table, sizeof area_table);
    Field_State = g_state;
    g = kStubs;

    unsigned bad = 0, flow_ops = 0, log_calls = 0, kind2_rounds = 0;
    unsigned dispatched[16] = {}, group_f[16] = {};   // what the step's own dispatch saw
    for (unsigned round = 0; round < kRounds; ++round) {
        // The window a round can reach: its chain, what the ops after it
        // read, and position 0 for 0B.
        const unsigned start = 1 + Next() % 0xFE00;
        for (unsigned i = start; i < start + kWindow; ++i) g_script[i] = static_cast<unsigned char>(Next());
        std::memset(g_skip + start, -1, kWindow);
        for (auto& b : g_state) b = static_cast<unsigned char>(Next());
        for (auto& t : g_label_target) t = 0;

        auto* bytes = reinterpret_cast<unsigned char*>(&input);
        for (unsigned i = 0; i < offsetof(State, log); ++i) bytes[i] = static_cast<unsigned char>(Next());
        SetPos(input.object, start);
        input.object[2] = static_cast<unsigned char>(Next() % 3 ? Next() % 3 : Next());
        flow_ops += Generate(start);
        input.area = static_cast<std::uint16_t>(Next() % Area_Descriptors_count);
        input.divisor = static_cast<short>(1 + Next() % 0x80);
        input.kind2[0x87] = static_cast<unsigned char>(1 + Next() % 255);
        input.request = static_cast<unsigned char>(Next() % 4 ? 3 : Next());
        input.current = Next() % 4 ? g_sprite : Sprite_Kind2;
        if (input.current == Sprite_Kind2) ++kind2_rounds;
        input.move_object = nullptr;
        g_seed = Next();
        unsigned char* const saved_descriptor = Area_Descriptors[input.area];
        Area_Descriptors[input.area] = g_descriptor;

        // Which op the step dispatches: ours alone, once, from the same state.
        Apply(input);
        MoveScript_Flow(g_object, g_script, 0);
        const unsigned char stop = g_script[Pos(g_object)];
        ++dispatched[stop >> 4];
        if (stop >= 0xF0) ++group_f[stop & 0xF];
        for (int pass = 0; pass < 2; ++pass) {
            Apply(input);
            const unsigned char r = (pass ? &MoveScript_Step : theirs)(g_object, g_script);
            Capture(pass ? our_out : their_out);
            (pass ? our_out : their_out).result = r;
        }
        Area_Descriptors[input.area] = saved_descriptor;
        log_calls += their_out.log_n;
        if (std::memcmp(&their_out, &our_out, sizeof their_out) != 0 && ++bad <= 8)
            bof3::Log("shadow      move_script self-test MISMATCH: round %u, start 0x%04X, result %02X / %02X, "
                      "position %04X / %04X, log %u / %u", round, start, their_out.result, our_out.result,
                      Pos(their_out.object), Pos(our_out.object), their_out.log_n, our_out.log_n);
    }
    g = kOriginals;
    std::memcpy(MoveScript_CounterOps, saved_counter_ops, sizeof saved_counter_ops);
    Field_State = saved_state;
    Apply(saved);
    Game_AreaNumber = saved_area;
    bof3::Log("shadow      move_script self-test: %u rounds, %u flow ops (01..0F: %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u, "
              "A: %u, B: %u), %u rounds on Sprite_Kind2, %u calls to the stand-ins, %u MISMATCHES; the context, "
              "the sprite, Sprite_Kind2, 0x903848..0x903853, five globals, the result and the stand-ins' log compared",
              kRounds, flow_ops, g_ops_seen[1], g_ops_seen[2], g_ops_seen[3], g_ops_seen[4], g_ops_seen[5],
              g_ops_seen[6], g_ops_seen[7], g_ops_seen[8], g_ops_seen[9], g_ops_seen[10], g_ops_seen[11],
              g_ops_seen[12], g_ops_seen[13], g_ops_seen[14], g_ops_seen[15], g_ops_seen[16], g_ops_seen[17],
              kind2_rounds, log_calls, bad);
    bof3::Log("shadow      move_script self-test: the step dispatched by high nibble 0..F: %u %u %u %u %u %u %u %u %u %u %u %u "
              "%u %u %u %u; F0..FF: %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u",
              dispatched[0], dispatched[1], dispatched[2], dispatched[3], dispatched[4], dispatched[5], dispatched[6],
              dispatched[7], dispatched[8], dispatched[9], dispatched[10], dispatched[11], dispatched[12], dispatched[13],
              dispatched[14], dispatched[15], group_f[0], group_f[1], group_f[2], group_f[3], group_f[4], group_f[5],
              group_f[6], group_f[7], group_f[8], group_f[9], group_f[10], group_f[11], group_f[12], group_f[13],
              group_f[14], group_f[15]);
    if (bad) bof3::Fatal("MoveScript_Step differs from the original in %u of %u self-test rounds", bad, kRounds);
}

// --- BOF3X_SHADOW=move_counters: a differential fuzz of the three counter ops ---
// Byte-copies of all three (no calls, no jumps out). One round: an op byte and
// operand, the four counters random or with the one addressed equal to the
// operand; theirs, then ours; the counters and al compared.
void CounterSelfTest(void* const (&theirs)[3]) {
    using CounterFn = signed char (__cdecl*)(const unsigned char*);
    const CounterFn ours[3] = {&MoveScript_CounterSet, &MoveScript_CounterTest, &MoveScript_CounterStep};
    constexpr unsigned kRounds = 6000;
    unsigned char* const counters = Scratch() - 8;
    unsigned char saved[4];
    std::memcpy(saved, counters, 4);
    std::uint32_t rng = 0x2F6B2A51u;
    auto next = [&rng] { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; };
    unsigned bad = 0, equal = 0;
    for (unsigned round = 0; round < kRounds; ++round) {
        const unsigned k = round % 3;
        unsigned char op[2] = {static_cast<unsigned char>(next()), static_cast<unsigned char>(next())};
        unsigned char in[4];
        for (auto& c : in) c = static_cast<unsigned char>(next());
        if (next() % 2) { in[(op[0] >> 2) & 3] = op[1]; ++equal; }
        unsigned char out[2][4];
        signed char r[2];
        for (int pass = 0; pass < 2; ++pass) {
            std::memcpy(counters, in, 4);
            r[pass] = (pass ? ours[k] : reinterpret_cast<CounterFn>(theirs[k]))(op);
            std::memcpy(out[pass], counters, 4);
        }
        if ((r[0] != r[1] || std::memcmp(out[0], out[1], 4) != 0) && ++bad <= 8)
            bof3::Log("shadow      move_counters self-test MISMATCH: round %u, op %02X %02X, %d / %d", round, op[0], op[1], r[0], r[1]);
    }
    std::memcpy(counters, saved, 4);
    bof3::Log("shadow      move_counters self-test: %u rounds, %u with the counter equal to the operand, %u MISMATCHES; "
              "the four counters and al compared", kRounds, equal, bad);
    if (bad) bof3::Fatal("the movement script's counter ops differ from the original in %u of %u rounds", bad, kRounds);
}

}  // namespace

void MoveScript_Inject() {
    if (bof3::WantsShadow("move_script")) {
        const auto cast = [](auto f) { return reinterpret_cast<const void*>(f); };
        const bof3::CloneCall f_calls[] = {
            {0x32, cast(&StubFlagsClear)}, {0x3F, cast(&StubFlagsSet)}, {0x61, cast(&StubKind)}, {0x71, cast(&StubMove)},
            {0x8B, cast(&StubMoveKind2)}, {0xB5, cast(&StubKind)}, {0xC5, cast(&StubMove)}, {0x148, cast(&StubMoveKind2)},
            {0x22A, cast(&StubWaitTest)}, {0x268, cast(&StubKind)}, {0x278, cast(&StubMove)}, {0x28B, cast(&StubMoveKind2)},
            {0x2D8, cast(&StubTickOnce)}, {0x308, cast(&StubPeek)}, {0x33E, cast(&StubSound)}, {0x385, cast(&StubOpF7)},
            {0x39B, cast(&StubKind)}, {0x3C6, cast(&StubAttach)}, {0x3E9, cast(&StubTestFB)}, {0x3FF, cast(&StubTestFC)},
            {0x410, cast(&StubSound)},
        };
        void* const group_f = bof3::CloneOriginal("MoveScript_GroupF", kGroupF, kGroupFSize, f_calls,
                                                  static_cast<int>(sizeof f_calls / sizeof f_calls[0]));
        Relocate(group_f, kGroupF, kGroupFSize, kGroupFTable);
        const bof3::CloneCall step_calls[] = {
            {0x7F, cast(&StubKind)}, {0x8F, cast(&StubMove)}, {0xA8, cast(&StubMoveKind2)},
            {0xD4, cast(&StubGroup<6>)}, {0xEF, cast(&StubGroup<8>)}, {0x10A, cast(&StubGroup<9>)},
            {0x136, cast(&StubGroup<12>)}, {0x151, cast(&StubGroup<13>)}, {0x16C, cast(&StubGroup<14>)},
            {0x187, group_f},
            {0x348, cast(&StubFindLabel)}, {0x3B8, cast(&StubVariable)}, {0x3CD, cast(&StubVariable)},
            {0x3F8, cast(&StubFindLabel)}, {0x416, cast(&StubVariable)}, {0x42B, cast(&StubVariable)},
            {0x446, cast(&StubVariable)}, {0x45B, cast(&StubVariable)}, {0x4CD, cast(&StubTrioClear)},
            {0x4DC, cast(&StubTrioSet)}, {0x4EB, cast(&StubKind)}, {0x561, cast(&StubElevation)},
            {0x5EF, cast(&StubElevation)}, {0x693, cast(&StubSetAnimationAt)}, {0x6A7, cast(&StubSetAnimation)},
            {0x6B8, cast(&StubGroupB)},
        };
        void* const step = bof3::CloneOriginal("MoveScript_Step", kStep, kStepSize, step_calls,
                                               static_cast<int>(sizeof step_calls / sizeof step_calls[0]));
        for (const Table& t : kStepTables) Relocate(step, kStep, kStepSize, t);
        SelfTest(reinterpret_cast<unsigned char (__cdecl*)(unsigned char*, const unsigned char*)>(step));
    }
    if (bof3::WantsShadow("move_counters")) {
        void* const theirs[3] = {bof3::CloneOriginal("MoveScript_CounterSet", 0x57C460, 0x18),
                                 bof3::CloneOriginal("MoveScript_CounterTest", 0x57C480, 0x20),
                                 bof3::CloneOriginal("MoveScript_CounterStep", 0x57C4A0, 0x1F)};
        CounterSelfTest(theirs);
    }
    BOF3_INJECT(MoveScript_Step);
    BOF3_INJECT(MoveScript_Flow);
    BOF3_INJECT(MoveScript_GroupF);
    BOF3_INJECT(MoveScript_CounterSet);
    BOF3_INJECT(MoveScript_CounterTest);
    BOF3_INJECT(MoveScript_CounterStep);
}

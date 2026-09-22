#include "game/event_script.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/event_script_callees.h"
#include "hook/detour.h"
#include "hook/log.h"

// The event script interpreter and the flag helpers - see docs/event-script.md
// for each function, its PSX twin and the fuzz.
//
// Terms. "The count" is the word at DamageScratch (0x903850; the PSX
// scratchpad's 0x1F800000): the next Sprite_Objects entry a placement fills.
// The word after it (0x903852, PSX 0x1F800002) holds the animation bank a
// placement passes on. Both are general scratch words, used all over the exe
// (2,114 and 712 references) - here they mean only this. "SC" is
// Sprite_Current, read afresh at every use as the original reads [0x937F88]
// (a stand-in may move it); "FAM" is Field_ActiveMember, whose +0x80.. is the
// object's script context.

namespace event_script {

const Callees kOriginals = {
    EventScript_Op, EventScript_Control, EventScript_If, EventScript_IfNot, EventScript_Switch,
    EventScript_IfRunStep, EventScript_IfSkipStep, EventScript_SkipControl, EventScript_SkipIf,
    EventScript_SkipSwitch, EventScript_CaseRun, EventScript_CaseSkip,
    reinterpret_cast<const Condition*>(EventScript_Conditions),
    {EventOp_0x, EventOp_1x, EventOp_2x, EventOp_3x, EventOp_4x, EventOp_5x, EventOp_6x, EventOp_7x, EventOp_8x,
     EventOp_9x, EventOp_Ax, EventOp_Bx, EventOp_Cx, EventOp_Dx, EventOp_Ex, EventOp_0x},
    EventObj_Reset, Sprite_SetAnimationBank, AreaMap_Elevation, EventObj_SetFlags, EventObj_Face,
    PartyRecord_Clear, Sprite_InitFromEntry, Kind2_Place,
    EventScript_Run, Flags_Test,
};
Callees g = kOriginals;

}  // namespace event_script

namespace {

using event_script::g;

unsigned char* At(std::uint32_t a) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a)); }
void SetWord(unsigned char* p, unsigned v) {
    const auto w = static_cast<std::uint16_t>(v);
    std::memcpy(p, &w, sizeof w);
}
void SetDword(unsigned char* p, std::uint32_t v) { std::memcpy(p, &v, sizeof v); }
std::uint32_t Dword(const unsigned char* p) {
    std::uint32_t v;
    std::memcpy(&v, p, sizeof v);
    return v;
}

// The count and the bank word (see the top of the file).
short Count() {
    short v;
    std::memcpy(&v, At(bof3::addr::DamageScratch), sizeof v);
    return v;
}
void SetCount(unsigned v) { SetWord(At(bof3::addr::DamageScratch), v); }
unsigned short Bank() {
    unsigned short v;
    std::memcpy(&v, At(bof3::addr::DamageScratch) + 2, sizeof v);
    return v;
}
void SetBank(unsigned v) { SetWord(At(bof3::addr::DamageScratch) + 2, v); }

// Data these read that symbols.toml does not name.
constexpr std::uint32_t kBlockFlags = 0x8034E1;   // bit 6: ScriptFlags_Set40 / Clear40 (object_kinds.cpp)

unsigned char* Object(int n) { return Sprite_Objects + n * 0xA4; }
unsigned char* Extra(unsigned n) { return Sprite_ObjectsExtra + n * 0xA4u; }

// A placement's x or z: the whole part from `whole`, and the half (0x8000) if
// `half` is non-zero - `neg / sbb / and 0x8000`.
std::uint32_t Coordinate(unsigned char half, unsigned char whole) {
    return (half ? 0x8000u : 0u) | static_cast<std::uint32_t>(whole) << 16;
}

// `cdq` / `idiv`: the original's signed division, the divide fault included.
std::int32_t Idiv(std::int32_t dividend, std::int32_t divisor) {
    std::int32_t quotient;
    __asm__ volatile("cltd\n\tidivl %2" : "=a"(quotient) : "a"(dividend), "r"(divisor) : "edx", "cc");
    return quotient;
}

}  // namespace

// --- the interpreter -------------------------------------------------------------

// original 0x5797C0: runs an event script to its FF (PSX FUN_801A4F44).
// As the original has it: the flag bank starts as Cond_ByteFA sign-extended;
// every byte below F0 is an op and every other byte but FF a control op - a
// control op that does not advance (F2, F3, F5..F8, FB..FE) never ends.
extern "C" void __cdecl EventScript_Run(const unsigned char* script) {
    EventScript_FlagBank = static_cast<unsigned short>(static_cast<short>(Cond_ByteFA));
    const unsigned char* at = script;
    while (*at != 0xFF) at = *at < 0xF0 ? g.op(at) : g.control(at);
}

// original 0x579F30: one op below F0 (PSX FUN_801A5C00). The handler by the
// high nibble - 0 and F both reach EventOp_0x, the jump table's default -
// then the op's length by the high nibble of the byte read AGAIN, after the
// handler.
extern "C" const unsigned char* __cdecl EventScript_Op(const unsigned char* at) {
    g.handlers[at[0] >> 4](at);
    return at + EventScript_OpLengths[at[0] >> 4];
}

// original 0x579800: a control op, F0 and up (PSX FUN_801A5408). Anything but
// F0 F1 F4 F9 FA - and any byte below F0 - comes back where it was.
extern "C" const unsigned char* __cdecl EventScript_Control(const unsigned char* at) {
    switch (at[0]) {
    case 0xF0: return g.if_(at + 1);
    case 0xF1: return g.if_not(at + 1);
    case 0xF4: return g.switch_(at + 1);
    case 0xF9:
        EventScript_FlagBank = at[1];
        return at + 2;
    case 0xFA:
        EventScript_FlagBank = static_cast<unsigned short>(static_cast<short>(Cond_ByteFA));
        return at + 1;
    default: return at;
    }
}

namespace {

// EventScript_If and IfNot, which differ only in which arm runs first:
// `taken` is the arm-runs answer. The condition gets the position of its
// operand (it may move it) and its index masked to five bits - 17..31 reach
// MoveScript_CounterOps, the E9 handlers and data words, which take other
// arguments; no shipped script has one (tools/event_scan.py). One byte past
// the operand, the arms: the depth starts at 1, FE at depth 0 ends the if and
// is stepped over; the else byte chooses the step each time round, so an FD
// swaps them once (a second FD only sets it again).
const unsigned char* If(const unsigned char* at, bool run_on_true) {
    unsigned char depth = 0, else_seen = 0;
    const unsigned char* q = at;
    const unsigned c = *q++;
    const bool answer = (g.conditions[c & 0x1F](&q) & 0xFF) != 0;
    ++depth;
    ++q;
    const bool run_first = answer == run_on_true;
    for (;;) {
        if (depth == 0 && *q == 0xFE) return q + 1;
        const bool run = (else_seen == 0) == run_first;
        q = run ? g.run_step(q, &else_seen, &depth) : g.skip_step(q, &else_seen, &depth);
    }
}

}  // namespace

// original 0x579890: F0 c x (PSX FUN_801A4FC8), `at` past the F0.
extern "C" const unsigned char* __cdecl EventScript_If(const unsigned char* at) { return If(at, true); }

// original 0x579950: F1 c x (PSX FUN_801A5160), `at` past the F1.
extern "C" const unsigned char* __cdecl EventScript_IfNot(const unsigned char* at) { return If(at, false); }

// original 0x579A10: one step of an arm that runs (PSX FUN_801A52F8). FE
// counts the depth down and stays; FD sets the else byte; FD and FF step one.
extern "C" const unsigned char* __cdecl EventScript_IfRunStep(const unsigned char* at, unsigned char* else_seen,
                                                             unsigned char* depth) {
    const unsigned char b = *at;
    if (b < 0xF0) return g.op(at);
    if (b < 0xFD) return g.control(at);
    if (b == 0xFE) {
        --*depth;
        return at;
    }
    if (b == 0xFD) *else_seen = 1;
    return at + 1;
}

// original 0x579A50: one step of an arm that is skipped (PSX FUN_801A537C):
// an op by its length, a control op through EventScript_SkipControl, FE and
// FD as EventScript_IfRunStep.
extern "C" const unsigned char* __cdecl EventScript_IfSkipStep(const unsigned char* at, unsigned char* else_seen,
                                                              unsigned char* depth) {
    const unsigned char b = *at;
    if (b < 0xF0) return at + EventScript_OpLengths[b >> 4];
    if (b < 0xFD) return g.skip_control(at);
    if (b == 0xFE) {
        --*depth;
        return at;
    }
    if (b == 0xFD) *else_seen = 1;
    return at + 1;
}

// original 0x579AA0: steps over a control op without running it (PSX
// FUN_801A54AC). The nested if and switch are skipped by Capcom's
// EventScript_SkipIf / SkipSwitch from ON their first byte; F2 F3 F5..F8 and
// FB..FF - and any byte below F0 - come back where they were.
extern "C" const unsigned char* __cdecl EventScript_SkipControl(const unsigned char* at) {
    switch (at[0]) {
    case 0xF0:
    case 0xF1: return g.skip_if(at);
    case 0xF4: return g.skip_switch(at);
    case 0xF9: return at + 2;
    case 0xFA: return at + 1;
    default: return at;
    }
}

// original 0x579B00: F4 c (PSX FUN_801A5520), `at` past the F4.
// As the original has it: the condition index is the byte SIGNED and not
// masked - the table read from -128 to 127 entries - where the if masks it;
// the loop takes F6 x (a case), F7 (the default) and F5 (the end), and any
// other byte there loops for ever. Once a case has run, later cases are not
// tested: EventScript_CaseSkip starts past their F6 x. A case's body - run
// or skipped - starts one byte past where the condition left the position
// (the conditions reached leave it on x).
extern "C" const unsigned char* __cdecl EventScript_Switch(const unsigned char* at) {
    bool open = true;
    const unsigned char* q = at;
    const auto c = static_cast<signed char>(*q++);
    for (;;) {
        const unsigned char b = *q;
        if (b == 0xF5) return q + 1;
        if (b == 0xF6) {
            ++q;
            bool ran = false;
            if (open && (g.conditions[c](&q) & 0xFF) != 0) {
                ++q;
                q = g.case_run(q);
                open = false;
                ran = true;
            }
            if (!ran) {
                ++q;
                q = g.case_skip(q);
            }
        }
        if (*q != 0xF7) continue;
        ++q;
        if (open) {
            q = g.case_run(q);
            open = false;
        } else {
            q = g.case_skip(q);
        }
    }
}

// original 0x579BA0: a case body that runs, to past its F8 (PSX
// FUN_801A5648). F6 steps 2 and F7 1 - further labels of the same body; F2
// F3 F5 and FB..FF do nothing and are met again for ever.
extern "C" const unsigned char* __cdecl EventScript_CaseRun(const unsigned char* at) {
    bool go = true;
    do {
        const unsigned char b = *at;
        if (b < 0xF0) {
            at = g.op(at);
            continue;
        }
        switch (b) {
        case 0xF0:
        case 0xF1:
        case 0xF4:
        case 0xF9:
        case 0xFA: at = g.control(at); break;
        case 0xF6: at += 2; break;
        case 0xF7: at += 1; break;
        case 0xF8:
            go = false;
            at += 1;
            break;
        default: break;
        }
    } while (go);
    return at;
}

// original 0x579C20: a case body skipped (PSX FUN_801A56F0) - to past its F8,
// or back AT the next F6 (the next case, which the switch then tests). F7
// steps 1; F2 F3 F5 and FB..FF are met again for ever.
extern "C" const unsigned char* __cdecl EventScript_CaseSkip(const unsigned char* at) {
    bool go = true;
    do {
        const unsigned char b = *at;
        if (b < 0xF0) {
            at += EventScript_OpLengths[b >> 4];
            continue;
        }
        switch (b) {
        case 0xF0:
        case 0xF1:
        case 0xF4:
        case 0xF9:
        case 0xFA: at = g.skip_control(at); break;
        case 0xF6: return at;
        case 0xF7: at += 1; break;
        case 0xF8:
            go = false;
            at += 1;
            break;
        default: break;
        }
    } while (go);
    return at;
}

// --- the placements ---------------------------------------------------------------------

// original 0x579740: an area's placement script (PSX FUN_801A4E58), from the
// area descriptor +0. As the original has it: the count is signed - a count
// the script left negative clears from that (negative) entry, before the
// array, up to 29; one of 30 or more clears nothing and is left as it is.
extern "C" void __cdecl Area_RunPlacement(const unsigned char* script) {
    SetCount(0);
    for (unsigned i = 0; i < 4; ++i) Extra(i)[0] = 0;
    SetBank(4);
    g.run(script);
    const short n = Count();
    if (n >= 30) return;
    SetCount(30);   // stored before the loop, as the original stores it
    for (int i = n; i < 30; ++i) Object(i)[0] = 0;
}

// original 0x579E30: the current object's reset (PSX FUN_801A5AB0) - its
// sprite fields, then its script context through Field_ActiveMember.
extern "C" void __cdecl EventObj_Reset(void) {
    Sprite_Current[4] = 0;
    Sprite_Current[3] = 0;
    Sprite_Current[0x24] = 0;
    Sprite_Current[0xA] = 0;
    Sprite_Current[9] = 0;
    SetDword(Sprite_Current + 0x44, 0x10000);
    SetDword(Sprite_Current + 0x40, 0x10000);
    SetDword(Sprite_Current + 0x6C, 0);
    SetDword(Sprite_Current + 0x68, 0);
    SetDword(Sprite_Current + 0x64, 0);
    Field_ActiveMember[0x80] = 0;
    Field_ActiveMember[0x81] = 0;
    Field_ActiveMember[0x82] = 0;
    Field_ActiveMember[0x85] = 0;
    Field_ActiveMember[0x86] = 0xFF;
    Field_ActiveMember[0x87] = 0;
    SetWord(Field_ActiveMember + 0x8A, 0);
    SetWord(Field_ActiveMember + 0x9C, 0);
}

// original 0x579DB0: the current object's flags byte (PSX FUN_801A5998).
// As the original has it: flags[0] is read again for each test (it is
// stored into the object first); the speed index Field_ActiveMember +0x84 is
// a whole byte into Field_MoveSpeeds' six - 16 is divided by the byte there,
// and a zero there (indices 6, 7, 9..11, 13, and others past the table)
// raises the divide fault the original does (Idiv). Index 0 skips it.
extern "C" void __cdecl EventObj_SetFlags(const unsigned char* flags) {
    Sprite_Current[7] = flags[0];
    if (flags[0] & 1) Sprite_Current[0] |= 0x40;
    if (flags[0] & 4) Field_ActiveMember[0x80] &= 0xDF;
    Sprite_Current[0x48] = (flags[0] & 0x40) ? 0 : 1;
    const unsigned char speed = Field_ActiveMember[0x84];
    if (speed != 0) Sprite_Current[0xA] = static_cast<unsigned char>(Idiv(16, Field_MoveSpeeds[speed]));
}

namespace {

// EventOp_1x and EventOp_2x, which differ in 2x's +0x18 / +0x1C and object
// +0x83. The object's own bytes are written by the count read again each
// time, the sprite's through Sprite_Current read again each time - the same
// object unless a callee moved one of them.
void Place(const unsigned char* op, bool two) {
    if (Count() >= 30) return;
    unsigned char* const o = Object(Count());
    Sprite_Current = o;
    Field_ActiveMember = o;
    SetBank(static_cast<unsigned>(op[1]) << 8 | op[2]);
    g.reset();
    g.set_bank(Bank());
    Sprite_Current[0] = 1;
    SetDword(Sprite_Current + 0x34, Coordinate(op[6], op[7]));
    SetDword(Object(Count()) + 0x8C, Dword(Sprite_Current + 0x34));
    SetDword(Sprite_Current + 0x38, Coordinate(op[8], op[9]));
    SetDword(Object(Count()) + 0x90, Dword(Sprite_Current + 0x38));
    SetDword(Object(Count()) + 0x94, 0);
    const long elevation = g.elevation(static_cast<long>(Dword(Sprite_Current + 0x34)),
                                       static_cast<long>(Dword(Sprite_Current + 0x38)));
    SetWord(Sprite_Current + 0x3E, static_cast<unsigned>(elevation));
    Sprite_Current[1] = op[0xA];
    Sprite_Current[2] = op[0xC];
    if (two) {
        SetDword(Sprite_Current + 0x18, op[0x11]);
        SetDword(Sprite_Current + 0x1C, op[0x12]);
    }
    Sprite_Current[8] = op[0] & 0xF;
    Sprite_Current[6] = op[0] >> 4;
    unsigned char* const n = Object(Count());
    SetWord(n + 0x98, op[4]);
    SetWord(n + 0x9A, op[5]);
    SetWord(n + 0x88, static_cast<unsigned>(op[0xE]) << 8 | op[0xF]);
    n[0x84] = op[0xB];
    if (two) n[0x83] = op[0x10];
    n[0xA0] = 0x7F;
    g.set_flags(op + 0xD);
    if (op[3] & 0x80) Sprite_Current[0] |= 0x20;
    Sprite_Current[0x5D] = 0;
    Sprite_Current[0x5F] = 0;
    Sprite_Current[0x5E] = 0;
    Sprite_Current[0x5C] = op[3] & 0xF;
    g.face();
    SetCount(static_cast<unsigned>(Count() + 1));
}

}  // namespace

// original 0x57A1E0: op 1x, 16 bytes - places Sprite_Objects[count] (PSX
// FUN_801A60C8). op[1]:op[2] the animation bank, op[3] bit 7 sets +0 bit
// 0x20 and its low nibble is +0x5C, op[4] / op[5] words +0x98 / +0x9A, op[6..9]
// x and z, op[0xA] +1, op[0xB] the speed index +0x84, op[0xC] +2, op[0xD] the
// flags (EventObj_SetFlags), op[0xE]:op[0xF] word +0x88; op[0]'s nibbles the
// direction +8 and the kind +6.
// As the original has it: nothing past the count's test of 30 checks it -
// a callee that moved the count moves the object's own writes with it.
extern "C" void __cdecl EventOp_1x(const unsigned char* op) { Place(op, false); }

// original 0x57A5E0: op 2x, 19 bytes (PSX FUN_801A67F8) - EventOp_1x, and
// op[0x10] to object +0x83, op[0x11] / op[0x12] to +0x18 / +0x1C as dwords.
extern "C" void __cdecl EventOp_2x(const unsigned char* op) { Place(op, true); }

// original 0x57B100: a party record's four bytes, in the order +1 +2 +3 +0
// (PSX FUN_801A5B88). Callers push the index with the register's other bits.
extern "C" void __cdecl PartyRecord_Clear(unsigned index) {
    unsigned char* const r = MoveScript_PartyRecords + (index & 0xFFu) * 16u;
    r[1] = 0;
    r[2] = 0;
    r[3] = 0;
    r[0] = 0;
}

// original 0x57BA60: a sprite from an area descriptor +8 entry (PSX
// 0x8015B67C). As the original has it: the entry's +4 is read after the first
// five stores; the angle's index is the direction +8, a whole byte into
// Sprite_DirectionAngles' eight (past them: what follows in .data).
extern "C" void __cdecl Sprite_InitFromEntry(unsigned char* entry) {
    Sprite_Current[0] = 1;
    Sprite_Current[0x24] = 0x40;
    Sprite_Current[0x48] = 1;
    SetDword(Sprite_Current + 0x40, 0x1000);
    SetDword(Sprite_Current + 0x44, 0x1000);
    SetDword(Sprite_Current + 0x50, Dword(entry + 4));
    SetDword(Sprite_Current + 0x54, static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(entry)));
    SetDword(Sprite_Current + 0x68, 0);
    SetDword(Sprite_Current + 0x64, 0);
    SetDword(Sprite_Current + 0x6C, Sprite_DirectionAngles[Sprite_Current[8]]);
}

// original 0x57B130: op Bx, 14 bytes - places Sprite_ObjectsExtra[op[0xB]]
// (PSX FUN_801A7CA0). op[1..4] x and z, op[5] +1, op[6] the speed index,
// op[7] +2, op[8] the flags, op[9] the descriptor +8 entry, op[0xA] object
// +0x83, op[0xC]:op[0xD] word +0x88; op[0]'s nibbles the direction and the
// kind plus one.
// As the original has it: op[0xB] is not checked against the four extra
// objects (the shipped scripts use 0..3); the descriptor is Game_AreaNumber's
// with no check either; +0x70 takes byte 2 of whatever +0x54 holds after
// Sprite_InitFromEntry - the PSX stores it as a word.
extern "C" void __cdecl EventOp_Bx(const unsigned char* op) {
    const unsigned char* const descriptor = Area_Descriptors[Game_AreaNumber];
    unsigned char* table;
    std::memcpy(&table, descriptor + 8, sizeof table);
    unsigned char* const entry = table + op[9] * 8u;
    unsigned char* const o = Extra(op[0xB]);
    Sprite_Current = o;
    Field_ActiveMember = Extra(op[0xB]);
    o[8] = op[0] & 0xF;
    g.reset();
    g.clear_record(op[0xB]);
    g.init_entry(entry);
    SetDword(Sprite_Current + 0x34, Coordinate(op[1], op[2]));
    SetDword(Sprite_Current + 0x38, Coordinate(op[3], op[4]));
    const long elevation = g.elevation(static_cast<long>(Dword(Sprite_Current + 0x34)),
                                       static_cast<long>(Dword(Sprite_Current + 0x38)));
    SetWord(Sprite_Current + 0x3E, static_cast<unsigned>(elevation));
    Sprite_Current[1] = op[5];
    Sprite_Current[2] = op[7];
    Sprite_Current[6] = static_cast<unsigned char>((op[0] >> 4) - 1);
    SetDword(Sprite_Current + 0x20, 0);
    SetDword(Sprite_Current + 0x1C, 0);
    SetDword(Sprite_Current + 0x18, 0);
    {
        unsigned char* const sc = Sprite_Current;
        unsigned char* source;
        std::memcpy(&source, sc + 0x54, sizeof source);
        sc[0x70] = source[2];
    }
    SetDword(Extra(op[0xB]) + 0x94, 0);
    SetDword(Extra(op[0xB]) + 0x90, 0);
    SetDword(Extra(op[0xB]) + 0x8C, 0);
    Extra(op[0xB])[0x83] = op[0xA];
    Extra(op[0xB])[0x84] = op[6];
    SetWord(Extra(op[0xB]) + 0x88, static_cast<unsigned>(op[0xC]) << 8 | op[0xD]);
    Extra(op[0xB])[0xA0] = 0x7F;
    g.set_flags(op + 8);
    Sprite_Current[0x5D] = 0x80;
    Sprite_Current[0x5F] = 0x80;
    Sprite_Current[0x5E] = 0x80;
    Sprite_Current[0x5C] = 2;
}

// original 0x57B4E0: op Cx, 2 bytes - the kind-2 object's kind is the op's
// high nibble (always 0xC), then Kind2_Place(op[1]) (PSX FUN_801A82B8).
extern "C" void __cdecl EventOp_Cx(const unsigned char* op) {
    Sprite_Kind2[6] = op[0] >> 4;
    g.kind2_place(op[1]);
}

// --- the flags --------------------------------------------------------------------------

// original 0x57C0F0 (PSX 0x8015BF70).
extern "C" void __cdecl Flags_Set(unsigned char* bits, unsigned index) {
    const unsigned i = index & 0xFFu;
    bits[i >> 3] |= static_cast<unsigned char>(1u << (i & 7));
}

// original 0x57C110 (PSX 0x8015BF98).
extern "C" void __cdecl Flags_Clear(unsigned char* bits, unsigned index) {
    const unsigned i = index & 0xFFu;
    bits[i >> 3] &= static_cast<unsigned char>(~(1u << (i & 7)));
}

// original 0x57C140 (PSX 0x8015BFC4). The original's eax is exactly 0 or 1
// (setne over a value below 0x20); this returns an unsigned char, which the
// compiler widens in eax for its callers (checked in docs/event-script.md).
extern "C" unsigned char __cdecl Flags_Test(const unsigned char* bits, unsigned index) {
    const unsigned i = index & 0xFFu;
    return (bits[i >> 3] & (1u << (i & 7))) != 0;
}

// original 0x57C180: condition 0 - the operand equals Cond_ByteFA (PSX
// 0x8015C00C). The position is not moved.
extern "C" unsigned char __cdecl EventCond_ByteFA(const unsigned char** at) {
    return static_cast<unsigned char>(Cond_ByteFA) == **at;
}

// original 0x57C1C0: condition 2 - flag `operand` of row EventScript_FlagBank
// (its low byte) of Cond_Flags (PSX 0x8015C05C).
extern "C" unsigned char __cdecl EventCond_Flag(const unsigned char** at) {
    return g.flags_test(Cond_Flags + (EventScript_FlagBank & 0xFFu) * 8u, **at);
}

// original 0x57C210: condition 8 - the operand equals Cond_ByteFD (PSX
// 0x8015C0C0).
extern "C" unsigned char __cdecl EventCond_ByteFD(const unsigned char** at) { return Cond_ByteFD == **at; }

// original 0x57C7C0 (PSX 0x8015CA78): bit 6 of byte 0x8034E1 - read before,
// stored after - and bit 8 of Field_ScriptFlags, by its high byte.
extern "C" void __cdecl ScriptFlags_Set40(void) {
    const unsigned char b = *At(kBlockFlags);
    reinterpret_cast<unsigned char*>(&Field_ScriptFlags)[1] |= 1;
    *At(kBlockFlags) = b | 0x40;
}

// original 0x57C810 (PSX 0x8015CAF0): bit 6 of three objects' first byte.
extern "C" void __cdecl ObjTrio_SetBit40(void) {
    ObjTrio[0] |= 0x40;
    ObjTrio[0x14C] |= 0x40;
    ObjTrio[0x298] |= 0x40;
}

void EventScript_Inject() {
    if (bof3::WantsShadow("event_script")) event_script::SelfTest();
    BOF3_INJECT(Area_RunPlacement);
    BOF3_INJECT(EventScript_Run);
    BOF3_INJECT(EventScript_Control);
    BOF3_INJECT(EventScript_If);
    BOF3_INJECT(EventScript_IfNot);
    BOF3_INJECT(EventScript_IfRunStep);
    BOF3_INJECT(EventScript_IfSkipStep);
    BOF3_INJECT(EventScript_SkipControl);
    BOF3_INJECT(EventScript_Switch);
    BOF3_INJECT(EventScript_CaseRun);
    BOF3_INJECT(EventScript_CaseSkip);
    BOF3_INJECT(EventObj_SetFlags);
    BOF3_INJECT(EventObj_Reset);
    BOF3_INJECT(EventScript_Op);
    BOF3_INJECT(EventOp_1x);
    BOF3_INJECT(EventOp_2x);
    BOF3_INJECT(PartyRecord_Clear);
    BOF3_INJECT(EventOp_Bx);
    BOF3_INJECT(EventOp_Cx);
    BOF3_INJECT(Sprite_InitFromEntry);
    BOF3_INJECT(Flags_Set);
    BOF3_INJECT(Flags_Clear);
    BOF3_INJECT(Flags_Test);
    BOF3_INJECT(EventCond_ByteFA);
    BOF3_INJECT(EventCond_Flag);
    BOF3_INJECT(EventCond_ByteFD);
    BOF3_INJECT(ScriptFlags_Set40);
    BOF3_INJECT(ObjTrio_SetBit40);
}

// The scenario round's shared harness (scenario_harness.h;
// docs/scenario_harness.md).
//
// A copy of magic_harness.cpp (the spell round's, group HX's fold) adapted to
// the scenario banks, self-contained so that the spell groups' fuzzes are
// untouched (round ten's rule: groups do not edit a harness; a later fold may
// factor the three):
//
//   - one pool of recording stand-ins of its own, assigned at start-up to
//     every callee a group's clones call (the standard set below, plus the
//     group's own) and to every handler they dispatch to (stack-table
//     immediates, .data table entries) - ours reaches the same recorders
//     through SH_CALL / SH_AT / Phase while g_active is set;
//   - one frame of the scenario engine's state: the chapter bytes, the flag
//     rows and the row pointer, the wait word, Field_Request, Game_AreaNumber,
//     the chapters' counters, the camera words, the script and pass flags,
//     the pending area change, the field and party objects, the sprite
//     records, the effect records, Sprite_Current / Frame_Counter, and the
//     group's regions - random bytes, the pointers put back inside, the
//     group's chapter written, then the group's seed;
//   - theirs (the copy) then ours from the same state, called in the clone's
//     shape, the regions and the recorders' log compared.
//
// The recorders are louder than the real callees: after recording, any call
// may move the step, the run, the timer, the wait word, a flag of the
// chapter's row, Sprite_Current, Frame_Counter, the counter byte, the request
// byte or a script flag (and a group's own cells, through its disturb) - so a
// value ours keeps where the original reads memory again, or the other way
// round, shows.
#include "game/scenario_harness.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <utility>

#include "bof3/symbols.gen.h"
#include "game/effect_gte.h"
#include "game/effect_5d.h"
#include "game/effect_5f.h"
#include "game/effect_4b.h"
#include "game/effect_2f.h"
#include "game/effect_1b.h"
#include "game/effect_1f.h"
#include "game/effect_1g.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace scenario_harness {

bool g_active = false;

namespace {

std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> std::uint32_t KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }

// --- the random source and the recorders' log ---------------------------------

std::uint32_t g_rng = 0x5CE7A410u;

constexpr unsigned kLog = 32768;
struct Entry { std::uint32_t what, a, b, c, d; };
Entry g_log[kLog];
unsigned g_log_n;
std::uint32_t g_seed;        // the recorders' own stream: the same on both passes
std::uint32_t g_salt;        // Noise's count within the pass
std::uint32_t g_rand_hint;
int g_rand_first = -1;
int g_rand_pending = -1;

// What an entry is (`what`): a call or a phase counts in the coverage line.
constexpr std::uint32_t kPhaseTag = 1000;   // + slot: a handler's or a kPhase callee's
constexpr std::uint32_t kMoreTag = 2000;    // + slot: a call's arguments past the fourth
constexpr std::uint32_t kNoteTag = 3000;    // Note / NoteBytes
constexpr std::uint32_t kReturnTag = 4000;  // a clone's answer (Clone::ret_mask, a hook's al)

std::uint32_t Hash() {
    std::uint32_t h = (g_seed + g_log_n * 0x2545F491u) * 0x9E3779B1u;
    h ^= h >> 15;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    return h;
}
void Log5(std::uint32_t what, std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t d) {
    if (g_log_n < kLog) g_log[g_log_n] = {what, a, b, c, d};
    ++g_log_n;
}

bool g_calm = false;

// --- the harness's own memory -------------------------------------------------

// What MapView_CornerPtr and a group's pointers may point at: a buffer of the
// harness's own, compared as a region.
alignas(16) unsigned char g_own[0x100];

// Round twelve's field mode (docs/scenario_harness.md section 7): the packet
// buffer Gfx_PacketNext points into, the text buffer the pointer-answering
// stand-ins answer into, the script buffer and its cursor cell, the scratch a
// kCall's pointer arguments point into. Each is a region of field mode.
alignas(16) unsigned char g_packets[0x800];
alignas(16) unsigned char g_text[0x200];
alignas(16) unsigned char g_script[0x100];
alignas(16) unsigned char* g_cursor[4];      // [0] the cursor a kCursor function is handed; the rest compared as padding
// Ten arguments' scratch, as before round thirteen's kArgs of 12: the buffer is
// a region, and its size is in every field group's random fill.
constexpr unsigned kScratchSlots = 10;
alignas(16) unsigned char g_scratch[kScratchSlots * 0x40];

const Group* g_group = nullptr;
bool g_field = false;        // field mode for the group being run
bool g_effect = false;       // effect mode (round thirteen) for the group being run
unsigned g_state_span = 0;   // effect mode: the spans of +1 / +2 for the function being fuzzed
unsigned g_sub_span = 0;

unsigned char* Byte(std::uint32_t address) { return Mem(address); }
std::uint16_t Word(std::uint32_t address) { return static_cast<std::uint16_t>(move_script::Word(Mem(address))); }

// The chapter bytes a handler runs with, for a handler's or kPhase's entry.
std::uint32_t Chapter() {
    return Byte(at::kState)[0] | static_cast<std::uint32_t>(Byte(at::kRun)[0]) << 8 |
           static_cast<std::uint32_t>(Byte(at::kStep)[0]) << 16 | static_cast<std::uint32_t>(Byte(at::kChapter)[0]) << 24;
}

// Field mode's fourth word of a handler's entry: Sprite_Current's state bytes
// +1..+3 (0xFFFFFF when it points outside the regions) and the menu block's
// state byte 0x929F01 on top.
std::uint32_t FieldPhase() {
    const unsigned char* const s = static_cast<const unsigned char*>(Sprite_Current);
    const std::uint32_t bytes = InRegions(s, 8) ? (s[1] | static_cast<std::uint32_t>(s[2]) << 8 | static_cast<std::uint32_t>(s[3]) << 16)
                                                : 0xFFFFFFu;
    return bytes | static_cast<std::uint32_t>(Byte(at::kMenuState)[0]) << 24;
}

// The calling thread's stack from just below the caller's frame to its base
// (the TIB's StackBase): what a caller's locals are.
bool OnStack(const void* p, unsigned n) {
    std::uint32_t base;
    __asm__("movl %%fs:4, %0" : "=r"(base));
    const auto here = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&base));
    const auto at = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p));
    return at > here && at + n > at && at + n <= base;
}
// What a guarded dereference may read: the regions, the stack, or the image
// (0x400000 .. the end of .rsrc; every section of BOF3.exe is readable).
bool Readable(const void* p, unsigned n) {
    const auto at = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p));
    return InRegions(p, n) || OnStack(p, n) || (at >= 0x400000 && at + n > at && at + n <= 0x93F000);
}
// A kDerefString argument: the string at p hashed to its NUL, 64 bytes at
// most, each byte read only if Readable; the length mixed in last.
std::uint32_t HashString(const void* p) {
    const auto* s = static_cast<const unsigned char*>(p);
    std::uint32_t h = 0x811C9DC5u;
    unsigned n = 0;
    while (n < 64 && Readable(s + n, 1) && s[n] != 0) h = (h ^ s[n++]) * 0x01000193u;
    return (h ^ n) * 0x01000193u;
}
// What a field-standard stand-in's effect may write: the regions or the stack.
bool Writable(const void* p, unsigned n) { return InRegions(p, n) || OnStack(p, n); }

bool InFlags(const unsigned char* p) {
    return p >= Mem(at::kCondFlags) && p + 8 <= Mem(at::kCondFlags + at::kCondFlagsSize);
}

// Every cell below is one some chapter handler reads again after a call.
void Disturb() {
    const std::uint32_t h = Hash();
    if (g_calm || h % 3 == 0) return;
    const unsigned v = (h >> 12) & 0xFF;
    const auto b = static_cast<unsigned char>(h >> 20);
    const unsigned span = g_group ? g_group->phase_span : 0;
    switch ((h >> 4) % 16) {
    case 0: Byte(at::kStep)[0] = span ? static_cast<unsigned char>(b % span) : b; break;
    case 1: Byte(at::kRun)[0] = span ? static_cast<unsigned char>(b % span) : b; break;
    case 2: move_script::SetWord(Mem(at::kWait), b & 1 ? 0u : h >> 16); break;
    case 3: {
        // a flag of the chapter's row (the pointer is the seed's: moved only
        // while it points into the rows)
        unsigned char* const row = FlagRow();
        if (InFlags(row)) row[v % 8] ^= static_cast<unsigned char>(1u << (b & 7));
        break;
    }
    // effect mode (round thirteen): among the 20 effect records, never onto a sprite
    case 4: Sprite_Current = g_effect ? EffectRecord(v) : SpriteRecord(v); break;
    case 5: Frame_Counter = h >> 6; break;
    case 6: Byte(at::kCounter)[0] = b; break;
    case 7: Byte(at::kRequest)[0] = b % 3 ? 2 : b; break;
    case 8: move_script::SetWord(Mem(at::kTimer), h >> 16); break;
    case 9: Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags ^ (1u << (b & 15))); break;
    // 10..13: field mode only (round twelve); without it these stay the
    // scenario round's no-ops, so its groups draw exactly what they drew.
    case 10:
        // effect mode (round thirteen): the kind's state +1 or its sub-state +2
        // of Sprite_Current, each only below its span (an unbounded index is
        // Capcom's dispatcher jumping through what is not its table)
        if (g_effect) {
            unsigned char* const s = static_cast<unsigned char*>(Sprite_Current);
            const unsigned span = v & 1 ? g_sub_span : g_state_span;
            if (span && InRegions(s, 8)) s[v & 1 ? 2 : 1] = static_cast<unsigned char>(b % span);
            break;
        }
        // a state byte +1..+4 of Sprite_Current (below sprite_span when set)
        if (g_field) {
            unsigned char* const s = static_cast<unsigned char*>(Sprite_Current);
            const unsigned span = g_group ? g_group->sprite_span : 0;
            if (InRegions(s, 8)) s[1 + v % 4] = span ? static_cast<unsigned char>(b % span) : b;
        }
        break;
    case 11:
        // the menu block's state or step byte (below menu_span when set), or its timer
        if (g_field) {
            const unsigned span = g_group ? g_group->menu_span : 0;
            if (v % 3 == 2) Byte(at::kMenuTimer)[0] = b;
            else Byte(v & 1 ? at::kMenuStep : at::kMenuState)[0] = span ? static_cast<unsigned char>(b % span) : b;
        }
        break;
    case 12:
        // the packet cursor, as a callee that drew moves it
        if (g_field) SetPointer(at::kPacketNext, g_packets + ((h >> 8) & 0x3F0));
        break;
    case 13:
        // a bit of Field_ScriptFlags2, or the pad's pressed word
        if (g_field) {
            if (b & 1) move_script::SetWord(Mem(0x905BA4), Word(0x905BA4) ^ (1u << ((b >> 1) & 15)));
            else move_script::SetWord(Mem(0x7E1BEC), h >> 16);
        }
        break;
    case 14:
        if (g_group && g_group->disturb) g_group->disturb(h);
        break;
    default: break;
    }
    if (g_group && g_group->settle) g_group->settle();
}

// --- the stand-ins ------------------------------------------------------------

struct Slot {
    const char* name;
    std::uint32_t address;   // what a clone's call site or table holds
    std::uint32_t key;       // what ours passes to Call
    unsigned nargs;
    std::uint32_t masks[kArgs];
    Answer answer;
    std::uint8_t lo, hi;
    std::uint8_t deref[kArgs];
    Effect effect;
    const void* custom;      // the group's own stand-in, or null for the pool's
    bool guard;              // deref only what Readable says (the field-standard callees)
    bool handler;            // a table's handler: logs the chapter bytes, answers garbage
    unsigned calls;          // the original's side, for the coverage line
};
constexpr unsigned kSlots = 768;   // 256 before round twelve's field-standard set, 512 before round thirteen's effect set
Slot g_slots[kSlots];
unsigned g_slot_n;

// kFlag's and kBool's draw: Hash() remixed, so that a 0 is also seen after a
// disturbance (magic_harness.cpp's fix, group E's finding).
std::uint32_t Remixed(std::uint32_t h) {
    h *= 0x2C1B3C6Du;
    h ^= h >> 12;
    h *= 0x297A2D39u;
    h ^= h >> 15;
    return h;
}

std::uint32_t Answering(const Slot& s) {
    const std::uint32_t h = s.answer == Answer::kFlag || s.answer == Answer::kBool ? Remixed(Hash()) : Hash();
    switch (s.answer) {
    case Answer::kByte: {
        const unsigned span = ((static_cast<unsigned>(s.hi) - s.lo) & 0xFFu) + 1;
        return (h & 0xFFFFFF00u) | ((s.lo + (h >> 8) % span) & 0xFFu);
    }
    case Answer::kFlag: return h % 3 == 0 ? (h & 0x100 ? 0u : h & 0xFFFFFF00u) : h | 0x10;
    case Answer::kBool: return h % 3 == 0 ? 0u : 1u;
    case Answer::kRand:
        if (g_rand_pending >= 0) {
            const int v = g_rand_pending;
            g_rand_pending = -1;
            return ((h >> 8) & 0x7F00u) | static_cast<std::uint32_t>(v);
        }
        if (h % 3 == 0) return ((h >> 8) & 0x7F00u) | ((g_rand_hint + (h >> 4) % 3 - 1) & 0xFF);
        return h % 4 == 0 ? h : (h >> 1) & 0x7FFF;
    case Answer::kGarbage:
    default: return h;
    }
}

template <unsigned I>
std::uint32_t __cdecl Stub(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2, std::uint32_t a3, std::uint32_t a4,
                           std::uint32_t a5, std::uint32_t a6, std::uint32_t a7, std::uint32_t a8, std::uint32_t a9,
                           std::uint32_t a10, std::uint32_t a11) {
    const Slot& s = g_slots[I];
    if (s.handler) {
        Log5(kPhaseTag + I, Chapter(), Word(at::kTimer), Key(Sprite_Current), g_field ? FieldPhase() : 0);
        Disturb();
        return Hash();
    }
    if (s.answer == Answer::kPhase) {
        Log5(I, Chapter(), Word(at::kTimer), Key(Sprite_Current),
             s.masks[0] ? static_cast<std::uint32_t>(move_script::Long(Mem(s.masks[0]))) : 0);
        Disturb();
        return Hash();
    }
    const std::uint32_t a[kArgs] = {a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11};
    std::uint32_t r[kArgs] = {};
    for (unsigned i = 0; i < s.nargs && i < kArgs; ++i) {
        const void* const p = reinterpret_cast<const void*>(static_cast<std::uintptr_t>(a[i]));
        if (s.deref[i] == kDerefString)
            r[i] = Readable(p, 1) ? HashString(p) : a[i] & s.masks[i];
        else
            r[i] = s.deref[i] && (!s.guard || Readable(p, s.deref[i])) ? HashBytes(p, s.deref[i]) : a[i] & s.masks[i];
    }
    const unsigned entry = g_log_n;
    Log5(I, r[0], r[1], r[2], r[3]);
    if (s.nargs > 4) Log5(kMoreTag + I, r[4], r[5], r[6], r[7]);
    if (s.nargs > 8) Log5(kMoreTag + I, r[8], r[9], r[10], r[11]);   // r[10], r[11] 0 below eleven arguments, as before
    Disturb();
    std::uint32_t answer = Answering(s);
    if (s.effect) answer = s.effect(a, answer);
    if (s.answer != Answer::kGarbage && entry < kLog) g_log[entry].d ^= answer & 0xFF;   // the answer in the log
    return answer;
}

using StubFn = std::uint32_t (__cdecl*)(std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t,
                                        std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t,
                                        std::uint32_t, std::uint32_t);
template <std::size_t... I> constexpr auto MakeStubs(std::index_sequence<I...>) {
    struct T { StubFn f[sizeof...(I)]; };
    return T{{&Stub<I>...}};
}
constexpr auto kStubs = MakeStubs(std::make_index_sequence<kSlots>{});

// --- the standard callees -------------------------------------------------------
//
// The named part of the scenario walk's frontier (analysis/scenario_roots.json
// frontier_functions; docs/scenario-roots.md section 4) less the draw
// primitives, plus Scenario_CallB and the unnamed functions chapter 0 calls,
// by address. SH_OURS for a callee that is ours (its name is our function;
// `address` the one it displaced), SH_THEIRS for one that is Capcom's (its
// name is the address), as symbols.toml says on 2026-09-27. A callee that
// changes hands is caught at start-up (Register) and moves line. Masks by what
// the callee reads (its declared type; the evidence where it says more).
#define SH_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
#define SH_THEIRS(name) #name, KeyOf(name), KeyOf(name)
constexpr std::uint32_t kAll = 0xFFFFFFFFu, kU8 = 0xFFu, kU16 = 0xFFFFu;

const Callee kStandard[] = {
    // messages
    {SH_OURS(Msg_OpenScript), 1, {kU16}, Answer::kGarbage, 0, 0},
    {SH_OURS(Msg_OpenSystem), 1, {kU16}, Answer::kGarbage, 0, 0},
    {SH_OURS(Text_DrawAt), 5, {kU16, kU16, kU8, kU8, kAll}, Answer::kGarbage, 0, 0},   // shorts, Text_DrawString's bytes, the text by value (round twelve's fold)
    // the area, the party
    {SH_OURS(Field_ChangeArea), 4, {kU16, kAll, kAll, kU8}, Answer::kGarbage, 0, 0},
    {SH_OURS(Party_DropIn), 1, {kU8}, Answer::kGarbage, 0, 0},
    {SH_OURS(Party_Join), 1, {kU8}, Answer::kFlag, 0, 0},
    {SH_OURS(PartySet_Load), 4, {kU8, kU8, kU8, kU8}, Answer::kGarbage, 0, 0},   // bytes (FS, round twelve's fold)
    {SH_OURS(PartySet_LoadFirst), 3, {kAll, kAll, kAll}, Answer::kGarbage, 0, 0},
    {SH_OURS(PartySet_LoadSecond), 4, {kAll, kAll, kAll, kAll}, Answer::kGarbage, 0, 0},
    {SH_OURS(Char_HealHp), 3, {kAll, kAll, kAll}, Answer::kFlag, 0, 0},
    {SH_OURS(Char_HealAp), 3, {kAll, kAll, kAll}, Answer::kFlag, 0, 0},
    {SH_OURS(Inventory_Add), 3, {kU8, kU8, kU8}, Answer::kFlag, 0, 0},
    {SH_OURS(Inventory_Count), 3, {kU8, kU8, kU8}, Answer::kGarbage, 0, 0},
    {SH_OURS(Item_NamePtr), 2, {kAll, kAll}, Answer::kGarbage, 0, 0},
    // the flags
    {SH_OURS(Flags_Test), 2, {kAll, kU8}, Answer::kBool, 0, 0},
    {SH_OURS(Flags_Set), 2, {kAll, kU8}, Answer::kGarbage, 0, 0},
    {SH_OURS(Flags_Clear), 2, {kAll, kU8}, Answer::kGarbage, 0, 0},
    {SH_OURS(ScriptFlags_Set40), 0, {}, Answer::kGarbage, 0, 0},
    {SH_OURS(ScriptFlags_Clear40), 0, {}, Answer::kGarbage, 0, 0},
    {SH_OURS(ObjTrio_SetBit40), 0, {}, Answer::kGarbage, 0, 0},
    {"ObjTrio_ClearBit40", bof3::addr::ObjTrio_ClearBit40, KeyOf(ObjTrio_ClearBit40), 0, {}, Answer::kGarbage, 0, 0},   // FO takes it (round twelve)
    // the scenario engine
    {SH_OURS(Scenario_CallA), 1, {kU8}, Answer::kGarbage, 0, 0},
    {"Scenario_CallB", bof3::addr::Scenario_CallB, KeyOf(Scenario_CallB), 1, {kU8}, Answer::kGarbage, 0, 0},   // FE2 takes it (round twelve)
    {SH_OURS(Transition_Start), 1, {kU8}, Answer::kGarbage, 0, 0},
    {SH_OURS(ClutStrip_FadeTo), 1, {kAll}, Answer::kGarbage, 0, 0},
    {SH_OURS(ClutStrip_Restore), 0, {}, Answer::kGarbage, 0, 0},
    {SH_OURS(Field_LoadingFrame), 0, {}, Answer::kGarbage, 0, 0},
    {SH_OURS(Port_DroppedCall), 1, {kU8}, Answer::kGarbage, 0, 0},
    // sprites, event objects, effects
    {SH_OURS(Sprite_EnsureAnimation), 1, {kU8}, Answer::kFlag, 0, 0},
    {SH_OURS(Sprite_SetAnimation), 1, {kU8}, Answer::kGarbage, 0, 0},
    {SH_OURS(Sprite_SetAnimationAt), 2, {kU8, kU16}, Answer::kGarbage, 0, 0},
    {SH_OURS(Sprite_SetAnimationBank), 1, {kU16}, Answer::kFlag, 0, 0},
    {SH_OURS(Sprite_FaceDirection), 1, {kU8}, Answer::kGarbage, 0, 0},
    {SH_OURS(EventObj_Face), 0, {}, Answer::kGarbage, 0, 0},
    {SH_OURS(EventObj_SetFlags), 1, {kAll}, Answer::kGarbage, 0, 0},
    {SH_OURS(EventObj_Reset), 0, {}, Answer::kGarbage, 0, 0},
    {"EventOp_6x", bof3::addr::EventOp_6x, KeyOf(EventOp_6x), 1, {kAll}, Answer::kGarbage, 0, 0},   // FO takes it (round twelve)
    {SH_OURS(Field_ObjectInHome), 1, {kAll}, Answer::kFlag, 0, 0},
    {SH_OURS(Effect_FindFree), 0, {}, Answer::kByte, 0xFF, 0x13},
    {SH_OURS(Effect_SpawnAt), 6, {kU8, kU8, kU8, kAll, kAll, kAll}, Answer::kByte, 0xFF, 0x13},
    // the map and the camera
    {SH_OURS(MapView_SetElevation), 1, {kU16}, Answer::kGarbage, 0, 0},
    {SH_OURS(Kind2_Place), 1, {kU8}, Answer::kGarbage, 0, 0},
    {SH_OURS(AreaMap_Elevation), 2, {kAll, kAll}, Answer::kGarbage, 0, 0},
    {SH_OURS(AreaMap_SetupEntries), 0, {}, Answer::kGarbage, 0, 0},
    {SH_OURS(AreaMap_ByteAt), 2, {kU16, kU16}, Answer::kGarbage, 0, 0},
    {SH_OURS(AreaMap_SetByte), 3, {kU16, kU16, kU8}, Answer::kGarbage, 0, 0},
    {SH_OURS(MoveCmd_TestFB), 2, {kU16, kU16}, Answer::kFlag, 0, 0},
    {SH_OURS(Field_ViewReset), 0, {}, Answer::kGarbage, 0, 0},
    // sound and music
    {SH_OURS(Sound_PlayEffect), 1, {kU16}, Answer::kGarbage, 0, 0},
    {SH_OURS(Sound_PlayById), 1, {kU16}, Answer::kGarbage, 0, 0},
    {SH_OURS(Sound_LoadStream), 1, {kAll}, Answer::kGarbage, 0, 0},
    {SH_OURS(Sound_StreamDone), 0, {}, Answer::kBool, 0, 0},
    {SH_OURS(Sound_ResumeAll), 0, {}, Answer::kGarbage, 0, 0},
    {SH_OURS(Music_LoadFile), 1, {kAll}, Answer::kGarbage, 0, 0},
    {SH_OURS(Music_Play), 2, {kAll, kAll}, Answer::kGarbage, 0, 0},
    {SH_OURS(Music_FadeOutStop), 1, {kAll}, Answer::kGarbage, 0, 0},
    {SH_OURS(Music_FadeOut), 1, {kAll}, Answer::kGarbage, 0, 0},
    {SH_OURS(Music_FadeIn), 1, {kAll}, Answer::kGarbage, 0, 0},
    // files and tasks
    {SH_OURS(LoadDatFile), 1, {kAll}, Answer::kGarbage, 0, 0},
    {SH_OURS(File_LoadDone), 0, {}, Answer::kBool, 0, 0},
    {SH_OURS(Task_Sleep), 1, {kU16}, Answer::kGarbage, 0, 0},
    {SH_OURS(Task_Create), 2, {kAll, kAll}, Answer::kGarbage, 0, 0},
    {SH_OURS(Task_Exit), 0, {}, Answer::kGarbage, 0, 0},
    {SH_OURS(Task_Restart), 1, {kAll}, Answer::kGarbage, 0, 0},
    {SH_OURS(Rand), 0, {}, Answer::kRand, 0, 0},
    // unnamed, by address (docs/scena_sc0.md section 6)
    // SE's (round ten): the event battle's set-up by index - 0x904AAA, 0x802D41 = 5
    {SH_OURS(Field_StartEventBattle), 1, {kU8}, Answer::kGarbage, 0, 0},
    // x, z (dwords to 0x903780 / 84) and an index: an event battle's party placement
    {SH_OURS(Party_PlaceForBattle), 3, {kAll, kAll, kU8}, Answer::kGarbage, 0, 0},
    // the camera turned toward an angle (s16) at a speed (s8), al 1 while turning
    {SH_OURS(Camera_EaseAngleFB), 2, {kU16, kU8}, Answer::kFlag, 0, 0},
    // the view shift after a focus test (PSX 0x80155154, docs/field-modes.md)
    {SH_OURS(MapView_FillCells), 0, {}, Answer::kGarbage, 0, 0},
    // Field_StatusBits |= 0x80
    {SH_OURS(Field_SetStatus80), 0, {}, Answer::kGarbage, 0, 0},
};
#undef SH_OURS
#undef SH_THEIRS

// --- round twelve: the field-standard callees ------------------------------------
//
// The frontier of the field runs (docs/scenario_harness.md section 7.4): every
// function the 323 functions of groups FC1..FS call that is not one of the 323
// and not in kStandard above, 174 of them, by FH's pass over the cut table
// analysis/round12_cut.tsv at 430f34b. Registered for every group after its
// handlers, so a handler address stays a handler; a scenario group that never
// calls them sees no difference. Typed from symbols.toml's ret / params (masks
// by the parameter types, kFlag for a byte answer), the 26 unnamed ones by
// reading; `guard` set on all: a pointer argument is hashed (16 bytes of
// char / void, 8 of short, 12 of long) only where Readable says, else logged
// as its value. Louder where the caller reads back: the pointer answers land
// in the harness's buffers, the out-parameters are written with noise, the
// packet cursor advances, Party_Zenny moves.
//
// FIELD_OURS as SH_OURS. FIELD_THEIRS names the address as well, so the entry
// holds whichever side has the name: when a group takes the function the key
// becomes ours and the entry still registers (Register's checks pass both ways).
#define FIELD_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
#define FIELD_THEIRS(name, address) #name, address, KeyOf(name)

// Effects (the same on both passes: they draw from Noise() and the state only).
void FillNoise(std::uint32_t at, unsigned n) {
    void* const p = reinterpret_cast<void*>(static_cast<std::uintptr_t>(at));
    if (Writable(p, n)) FillBytes(p, n);
}
void FillFloats(std::uint32_t at, unsigned n) {
    // small whole numbers, not raw bytes: an x87 caller converts them back
    void* const p = reinterpret_cast<void*>(static_cast<std::uintptr_t>(at));
    if (!Writable(p, 4 * n)) return;
    for (unsigned i = 0; i < n; ++i) {
        const float f = static_cast<float>(static_cast<std::int16_t>(Noise() >> 9));
        std::memcpy(static_cast<unsigned char*>(p) + 4 * i, &f, 4);
    }
}
std::uint32_t FxText(const std::uint32_t*, std::uint32_t answer) { return Key(g_text + (answer & 0xF0)); }
std::uint32_t FxArg0(const std::uint32_t* a, std::uint32_t) { return a[0]; }
std::uint32_t FxArg4(const std::uint32_t* a, std::uint32_t) { return a[4]; }
std::uint32_t FxPacket(const std::uint32_t*, std::uint32_t) { return Key(Pointer(at::kPacketNext)); }
std::uint32_t FxRotMatrix(const std::uint32_t* a, std::uint32_t) { FillNoise(a[1], 18); return a[1]; }
std::uint32_t FxMulMatrix0(const std::uint32_t* a, std::uint32_t) { FillNoise(a[2], 18); return a[2]; }
std::uint32_t FxOut0_12(const std::uint32_t* a, std::uint32_t answer) { FillNoise(a[0], 12); return answer; }
std::uint32_t FxOut1_12(const std::uint32_t* a, std::uint32_t answer) { FillNoise(a[1], 12); return answer; }
std::uint32_t FxOut1_6(const std::uint32_t* a, std::uint32_t answer) { FillNoise(a[1], 6); return answer; }
std::uint32_t FxRotTransPers(const std::uint32_t* a, std::uint32_t answer) {
    FillNoise(a[1], 4);
    FillNoise(a[2], 4);
    return answer;
}
std::uint32_t FxRotTransPers3(const std::uint32_t* a, std::uint32_t answer) {
    for (unsigned i = 3; i < 6; ++i) FillFloats(a[i], 2);
    FillNoise(a[6], 4);
    return answer;
}
std::uint32_t FxRotTransPers4(const std::uint32_t* a, std::uint32_t answer) {
    for (unsigned i = 4; i < 8; ++i) FillFloats(a[i], 2);
    FillNoise(a[8], 4);
    return answer;
}
std::uint32_t FxStoreDepthF4(const std::uint32_t* a, std::uint32_t answer) {
    for (unsigned i = 0; i < 4; ++i) FillFloats(a[i], 1);
    return answer;
}
std::uint32_t FxSprintf(const std::uint32_t* a, std::uint32_t answer) {
    // up to seven letters and a NUL where the caller's buffer is ours to write
    auto* const dst = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a[0]));
    if (!Writable(dst, 8)) return answer;
    const unsigned n = Noise() % 8;
    for (unsigned i = 0; i < n; ++i) dst[i] = static_cast<unsigned char>('0' + Noise() % 43);
    dst[n] = 0;
    return n;
}
std::uint32_t FxCommitPrim(const std::uint32_t* a, std::uint32_t answer) {
    // Gfx_PacketNext += size (a byte) while the packet stays in the buffer
    unsigned char* const next = Pointer(at::kPacketNext);
    const unsigned size = a[1] & 0xFF;
    if (next >= g_packets && next + size + 0x40 <= g_packets + sizeof g_packets) SetPointer(at::kPacketNext, next + size);
    return answer;
}
std::uint32_t FxZennyAdd(const std::uint32_t* a, std::uint32_t answer) {
    // Party_Zenny += amount (and the tally 0x904138 when the second argument's
    // byte is 0), held at 9,999,999 with al 0 - as Zenny_Add (scena_sx.cpp,
    // docs/scena_sx.md; the test was inverted until 2026-10-01, FE1's finding)
    unsigned char* const z = Mem(at::kZenny);
    if (!InRegions(z, 4)) return answer;
    std::uint32_t v = static_cast<std::uint32_t>(move_script::Long(z)) + a[0];
    if ((a[1] & 0xFF) == 0 && InRegions(Mem(0x904138), 4))
        move_script::SetLong(Mem(0x904138), static_cast<std::int32_t>(static_cast<std::uint32_t>(move_script::Long(Mem(0x904138))) + a[0]));
    const bool held = v > 9999999u;
    if (held) v = 9999999u;
    move_script::SetLong(z, static_cast<std::int32_t>(v));
    return (answer & 0xFFFFFF00u) | (held ? 0u : 1u);
}
std::uint32_t FxSwap(const std::uint32_t* a, std::uint32_t answer) {
    auto* const x = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a[0]));
    auto* const y = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a[1]));
    if (InRegions(x, 1) && InRegions(y, 1)) std::swap(*x, *y);
    return answer;
}
// Round twelve's folds (2026-10-01; docs/scenario_harness.md section 8.6): the
// louder forms the wave-two groups kept to themselves, in place.
// Gte_SetTransMatrix reads the MATRIX's translation, +0x14..+0x1F (psx_gte.cpp):
// noted where readable; the rotation it never reads is not hashed (FC2, FE2).
std::uint32_t FxSetTrans(const std::uint32_t* a, std::uint32_t answer) {
    const auto* const t = reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(a[0] + 0x14));
    if (Readable(t, 12)) NoteBytes(t, 12);
    else Note(0xFFFFFFFFu);
    return answer;
}
// AreaMap_Slope's "sloped" byte 0x903850 (DamageScratch's first; the effect slot
// region), which its callers and MapView_SlopeAt's read straight after the call
// (FC1, FC3): 0 (flat) a third of the time, else 1.
std::uint32_t FxSloped(const std::uint32_t*, std::uint32_t answer) {
    if (InRegions(Mem(0x903850), 1)) Mem(0x903850)[0] = static_cast<unsigned char>(Noise() % 3 == 0 ? 0 : 1);
    return answer;
}
// MoveScript_Step moves the context's flag byte it is handed (Field_State
// +0x124), which FieldCore_ScriptMoveNext reads after it (FC3).
std::uint32_t FxStepFlag(const std::uint32_t* a, std::uint32_t answer) {
    const std::uint32_t n = Noise();
    auto* const p = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a[0]));
    if ((n & 1) && InRegions(p, 1)) p[0] = static_cast<unsigned char>(n >> 8);
    return answer;
}
// Menu_ListScroll writes both out-bytes on every path (menu_windows.cpp): the
// offset any byte, moving 0 or 1; the top and the state as a step can leave
// them. The panels read all four after it (FS).
std::uint32_t FxListScroll(const std::uint32_t* a, std::uint32_t answer) {
    auto* const top = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a[0]));
    auto* const offset = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a[1]));
    auto* const moving = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a[2]));
    auto* const state = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a[3]));
    const std::uint32_t n = Noise();
    if (Writable(offset, 1)) *offset = static_cast<unsigned char>(n);
    if (Writable(moving, 1)) *moving = static_cast<unsigned char>((n >> 8) & 1);
    if (Writable(top, 1) && (n & 0x10000)) *top = static_cast<unsigned char>(*top + ((n >> 17) & 1 ? 1 : 0xFF));
    if (Writable(state, 1) && (n & 0x40000)) *state = static_cast<unsigned char>(n >> 24);
    return (answer & 0xFFFFFF00u) | (Readable(top, 1) ? *top : answer & 0xFF);
}
// Equip_PreviewSet fills the caller's marks (0, 1, 2 or 4 a stat: the callers
// compare with 4 and 1) and values (eight bytes) as the real one does, and
// notes both, so the draws after read the same bytes on both passes (FO;
// the standard row hashed the two buffers before anything had filled them).
std::uint32_t FxPreviewSet(const std::uint32_t* a, std::uint32_t answer) {
    auto* const marks = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a[2]));
    auto* const values = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a[3]));
    static const unsigned char kMarks[] = {0, 1, 2, 4};
    if (Writable(marks, 4)) {
        for (unsigned i = 0; i < 4; ++i) marks[i] = kMarks[Noise() % 4];
        NoteBytes(marks, 4);
    }
    if (Writable(values, 8)) {
        FillBytes(values, 8);
        NoteBytes(values, 8);
    }
    return answer;
}
// Round fourteen's fold (docs/scenario_harness.md section 8.11).
// Sprite_LoadPalette writes 32 words at dst (and the same place of
// Gfx_ClutStrip) and reads nothing there: the row logs dst by value - R1A's
// listing (rest_1a_fuzz.cpp): the old 8-byte hash of what dst held before the
// call passed a wrong stride wherever the palettes were alike (C74) - and
// writes the 0x40 bytes with noise where they are in the regions, so the
// destination is compared after the call when a group keeps it.
std::uint32_t FxPalette(const std::uint32_t* a, std::uint32_t answer) {
    unsigned char* const dst = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a[0]));
    if (InRegions(dst, 0x40)) FillBytes(dst, 0x40);
    return answer;
}

// Field mode's re-listing of a kStandard entry (registered before kStandard,
// so it stands, in field mode only): answers the field code dereferences.
const Callee kFieldOverrides[] = {
    // callers read the name through the answer (mov ecx, [eax])
    {FIELD_OURS(Item_NamePtr), 2, {kAll, kAll}, Answer::kGarbage, 0, 0, {}, FxText, nullptr, true},
};

const Callee kField[] = {
    {FIELD_OURS(Menu_DrawPiece), 4, {kU16, kU16, kU8, kU8}, Answer::kGarbage, 0, 0, {0, 0, 0, 0}, nullptr, nullptr, true},   // FO:31 FS:17: void(int x, int y, unsigned id, unsigned flags)
    {FIELD_OURS(Sprite_ScriptTick), 0, {}, Answer::kFlag, 0, 0, {}, nullptr, nullptr, true},   // FC1:12 FC2:4 FC3:10 FE1:2 FE2:5: unsigned char(void)
    {FIELD_OURS(Crt_sprintf), 3, {kAll, kAll, kAll}, Answer::kGarbage, 0, 0, {0, 16}, FxSprintf, nullptr, true},   // FE1:4 FE2:1 FO:12 FS:10: int(char *dst, const char *fmt, ...)
    {FIELD_OURS(UiSprite_Draw), 4, {kU8, kAll, kU16, kU16}, Answer::kGarbage, 0, 0, {}, FxPacket, nullptr, true},   // FE1:25: a sprite primitive at Gfx_PacketNext, committed; eax the primitive (callers write through it)
    {FIELD_OURS(Sprite_UpdateScreenSlot), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FC1:15 FC2:9: void(void)
    {FIELD_OURS(MapView_SlopeAt), 3, {kAll, kAll, kU8}, Answer::kGarbage, 0, 0, {0, 0, 0}, FxSloped, nullptr, true},   // FC3:10 FE2:14: long(long x, long y, unsigned long direction)
    {FIELD_OURS(MapView_GroundAt), 2, {kAll, kAll}, Answer::kGarbage, 0, 0, {0, 0}, nullptr, nullptr, true},   // FC3:12 FE1:4 FE2:7: long(long x, long z)
    {FIELD_OURS(Text_DrawFont8), 4, {kU16, kU16, 0x3Fu, kAll}, Answer::kGarbage, 0, 0, {0, 0, 0, kDerefString}, nullptr, nullptr, true},   // FE2:2 FO:10 FS:8: void(int x, int y, int colour, const unsigned char *text)
    {FIELD_OURS(Menu_DrawPieces), 4, {kU16, kU16, kAll, kU8}, Answer::kGarbage, 0, 0, {0, 0, 16, 0}, nullptr, nullptr, true},   // FO:12 FS:7: void(int x, int y, const unsigned char *list, int flags)
    {FIELD_OURS(Menu_DrawBox), 6, {kU16, kU16, kU16, kU16, kU8, kU8}, Answer::kGarbage, 0, 0, {0, 0, 0, 0, 0, 0}, nullptr, nullptr, true},   // FC1:1 FE1:2 FO:8 FS:7: void(int x, int y, int w, int h, int flags, int colour)
    {FIELD_OURS(Effect_Release), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FC1:4 FC2:14: void(void)
    {FIELD_OURS(Gfx_CommitPrim), 2, {kU8, kU8}, Answer::kGarbage, 0, 0, {0, 0}, FxCommitPrim, nullptr, true},   // FC1:1 FE1:1 FE2:8 FO:4: void(unsigned slot, unsigned size)
    {FIELD_OURS(Party_Count), 1, {kU8}, Answer::kByte, 0, 3, {0}, nullptr, nullptr, true},   // FS:14: int(unsigned slot)
    {FIELD_OURS(Sprite_ScriptTickOnce), 0, {}, Answer::kFlag, 0, 0, {}, nullptr, nullptr, true},   // FC1:3 FC2:2 FC3:3 FE1:1 FE2:4: unsigned char(void)
    {FIELD_OURS(Input_AutoRepeat), 1, {kAll}, Answer::kGarbage, 0, 0, {0}, nullptr, nullptr, true},   // FE2:3 FS:10: unsigned(unsigned pressed)
    {FIELD_OURS(Msg_SystemPtr), 1, {kU16}, Answer::kGarbage, 0, 0, {0}, FxText, nullptr, true},   // FE2:1 FO:2 FS:9: const unsigned char *(unsigned id)
    {FIELD_OURS(Field_LeaderStepTick), 0, {}, Answer::kFlag, 0, 0, {}, nullptr, nullptr, true},   // FC3:9 FE2:1: unsigned char(void)
    {FIELD_OURS(Sprite_SetTint), 5, {kAll, kU8, kU8, kU8, kU8}, Answer::kFlag, 0, 0, {16, 0, 0, 0, 0}, nullptr, nullptr, true},   // FC1:4 FC3:2 FO:3: unsigned char(unsigned char *sprite, unsigned char r, unsigned char g, unsigned char b, unsigned char a)
    {FIELD_OURS(UiSprite_SetMode), 2, {kAll, kAll}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FE1:9: a draw-mode primitive, committed
    {FIELD_OURS(Menu_DrawSkillRow), 7, {kU16, kU16, kU8, kU8, kAll, kU8, kU8}, Answer::kGarbage, 0, 0, {0, 0, 0, 0, kDerefString, 0, 0}, nullptr, nullptr, true},   // FO:3 FS:6: void(int x, int y, int colour, unsigned kind, const unsigned char *name, unsigned cost, int dim)
    {FIELD_OURS(AreaMap_SetHeight), 3, {kU16, kU16, kU8}, Answer::kGarbage, 0, 0, {0, 0, 0}, nullptr, nullptr, true},   // FC2:8: void(unsigned x, unsigned z, unsigned value)
    {FIELD_OURS(Text_CharCount), 1, {kAll}, Answer::kFlag, 0, 0, {kDerefString}, nullptr, nullptr, true},   // FO:6 FS:2: unsigned char(const unsigned char *text)
    {FIELD_OURS(Sprite_ClearSteps), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FC3:4 FE1:1 FE2:2: void(void)
    {FIELD_OURS(Menu_DrawCursorBox), 6, {kU16, kU16, kU16, kU16, kU8, kU8}, Answer::kGarbage, 0, 0, {0, 0, 0, 0, 0, 0}, nullptr, nullptr, true},   // FS:7: void(int x, int y, int w, int h, int blink, int flags)
    {FIELD_OURS(Sprite_ApplyVelocity), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FC3:2 FE2:4: void(void)
    {FIELD_OURS(Math_Cos), 1, {kAll}, Answer::kGarbage, 0, 0, {0}, nullptr, nullptr, true},   // FE2:2 FO:4: int(int angle)
    {FIELD_OURS(Math_Sin), 1, {kAll}, Answer::kGarbage, 0, 0, {0}, nullptr, nullptr, true},   // FE2:2 FO:4: int(int angle)
    {FIELD_OURS(Menu_DrawHand), 3, {kU16, kU16, 0}, Answer::kGarbage, 0, 0, {0, 0, 0}, nullptr, nullptr, true},   // FC1:1 FE2:3 FS:1: void(int x, int y, int unused)
    {FIELD_OURS(Sprite_ObjectAt), 3, {kAll, kAll, kAll}, Answer::kFlag, 0, 0, {0, 0, 0}, nullptr, nullptr, true},   // FC1:2 FC2:2 FE2:1: unsigned char(long x, long y, unsigned margin)
    {FIELD_OURS(Sprite_LoadPalette), 2, {kAll, kAll}, Answer::kGarbage, 0, 0, {0, 0}, FxPalette, nullptr, true},   // FC1:1 FC3:1 FE2:3: void(unsigned short *dst, unsigned index); dst logged by value and written (FxPalette), not hashed - the callee only writes there (R1A's C74; round fourteen's fold)
    {FIELD_OURS(Gpu_SetSemiTrans), 2, {kAll, kAll}, Answer::kGarbage, 0, 0, {16, 0}, nullptr, nullptr, true},   // FC2:1 FE1:1 FE2:2 FO:1: void(unsigned char *prim, unsigned abe)
    {FIELD_OURS(Sprite_ShadeFadeBegin), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FC3:5: void(void)
    {FIELD_OURS(Field_JumpStart), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FC3:5: void(void)
    {FIELD_OURS(Gpu_SetDrawMode), 5, {kAll, kAll, kAll, kAll, kAll}, Answer::kGarbage, 0, 0, {16, 0, 0, 0, 0}, nullptr, nullptr, true},   // FE2:3 FO:2: void(unsigned char *prim, int dfe, int dtd, unsigned tpage, unsigned long tw)
    {FIELD_OURS(Menu_DrawIcon8), 4, {kU16, kU16, kU8, kU8}, Answer::kGarbage, 0, 0, {0, 0, 0, 0}, nullptr, nullptr, true},   // FO:5: void(int x, int y, int icon, int dim)
    {FIELD_OURS(KeyItem_Has), 1, {kU8}, Answer::kFlag, 0, 0, {0}, nullptr, nullptr, true},   // FO:1 FS:4: unsigned char(unsigned item)
    {FIELD_OURS(FieldMenu_SwapBytes), 2, {kAll, kAll}, Answer::kGarbage, 0, 0, {}, FxSwap, nullptr, true},   // 0x58BD50, FS:5: swaps the bytes its two pointers name
    {FIELD_OURS(Sprite_UpdateScreen), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FC1:4: void(void)
    {FIELD_OURS(Sprite_FindFree), 0, {}, Answer::kFlag, 0, 0, {}, nullptr, nullptr, true},   // FC1:4: unsigned char(void)
    {FIELD_OURS(Sprite_UpdateScreenA), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FC1:1 FC2:3: void(void)
    {FIELD_OURS(Gpu_SetPolyFT4), 1, {kAll}, Answer::kGarbage, 0, 0, {16}, nullptr, nullptr, true},   // FC2:1 FE2:3: void(unsigned char *prim)
    {FIELD_OURS(MapView_LinkPrimAt), 4, {kAll, kAll, kU8, kU8}, Answer::kGarbage, 0, 0, {0, 0, 0, 0}, nullptr, nullptr, true},   // FC2:1 FE2:3: void(unsigned long x, unsigned long z, int dy, unsigned size); x, z whole (the low word tested, the high word summed), dy `movsx edx, byte [esp+0x14]`, size `and edi, 0xFF` (0x572FD6, 0x572FF6; round thirteen's end fold)
    {FIELD_OURS(Tint_Release), 1, {kU8}, Answer::kGarbage, 0, 0, {0}, nullptr, nullptr, true},   // FC3:2 FO:2: void(unsigned char index)
    {FIELD_OURS(Panel_DrawEdgeQuad), 4, {kAll, kAll, kAll, kAll}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FE1:4: a textured quad, committed
    {FIELD_OURS(Panel_DrawWindow), 5, {kAll, kAll, kAll, kAll, kAll}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FE2:4: five words, calls 0x469790 / 0x469960
    {FIELD_OURS(ItemTrade_DrawList), 1, {kAll}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FE2:4: a list window drawn (Text_DrawAt, Item_NamePtr)
    {FIELD_OURS(Field_MemberSprite), 2, {kU8, kU8}, Answer::kGarbage, 0, 0, {0, 0}, nullptr, nullptr, true},   // FO:1 FS:3: void(unsigned member, unsigned slot)
    {FIELD_OURS(Menu_DrawTitleBox), 5, {kU16, kU16, kU16, kU16, kU8}, Answer::kGarbage, 0, 0, {0, 0, 0, 0, 0}, nullptr, nullptr, true},   // FS:4: void(int x, int y, int w, int h, int colour)
    {FIELD_OURS(Menu_DrawBlackScreen), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FS:4: void(void)
    {FIELD_OURS(Text_DrawSmall), 5, {kU16, kU16, kU8, kU8, kAll}, Answer::kGarbage, 0, 0, {0, 0, 0, 0, 16}, FxArg4, nullptr, true},   // FC1:1 FO:1 FS:1: const unsigned char *(int x, int y, unsigned colour, unsigned count, const unsigned char *text)
    {FIELD_OURS(Gte_PushMatrix), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FC2:1 FE2:2: void(void)
    {FIELD_OURS(Gte_RotMatrix), 2, {kAll, 0}, Answer::kGarbage, 0, 0, {6, 0}, FxRotMatrix, nullptr, true},   // FC2:1 FE2:2: short *(const short *angles, short *matrix)
    {FIELD_OURS(Gte_SetTransMatrix), 1, {0}, Answer::kGarbage, 0, 0, {}, FxSetTrans, nullptr, true},   // FC2:1 FE2:2: void(const unsigned long *matrix)
    {FIELD_OURS(Gte_SetRotMatrix), 1, {kAll}, Answer::kGarbage, 0, 0, {18}, nullptr, nullptr, true},   // FC2:1 FE2:2: void(const unsigned long *matrix)
    {FIELD_OURS(Gte_RotTrans), 2, {kAll, 0}, Answer::kGarbage, 0, 0, {6, 0}, FxOut1_12, nullptr, true},   // FC2:1 FE2:2: void(const short *vector, long *out)
    {FIELD_OURS(Gte_PopMatrix), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FC2:1 FE2:2: void(void)
    {FIELD_OURS(Gpu_GetClut), 2, {kAll, kAll}, Answer::kGarbage, 0, 0, {0, 0}, nullptr, nullptr, true},   // FC2:1 FO:2: unsigned(int x, int y)
    {FIELD_OURS(EffectKind41_DrawNumber), 4, {kU16, kU16, 0, kU8}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // 0x46D5F0, FC2:3: a number drawn (sprintf, sprites)
    {FIELD_OURS(Sprite_ShadeFadeStep), 1, {kAll}, Answer::kFlag, 0, 0, {0}, nullptr, nullptr, true},   // FC3:3: unsigned char(unsigned step)
    {FIELD_OURS(Field_CellAhead), 0, {}, Answer::kByte, 0, 4, {}, nullptr, nullptr, true},   // FC3:3: unsigned char(void)
    {FIELD_OURS(Field_WayBlocked), 4, {kAll, kAll, kU8, kU16}, Answer::kFlag, 0, 0, {0, 0, 0, 0}, nullptr, nullptr, true},   // FC3:1 FE1:2: unsigned char(long x, long z, unsigned raised, long ground)
    {FIELD_OURS(Field_JumpCamera), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FE1:3: void(void)
    {FIELD_OURS(Area_TestCondition), 1, {kU16}, Answer::kFlag, 0, 0, {0}, nullptr, nullptr, true},   // FE2:3: unsigned char(unsigned long code)
    {FIELD_OURS(Gte_RotTransPers4), 9, {kAll, kAll, kAll, kAll, 0, 0, 0, 0, 0}, Answer::kGarbage, 0, 0, {6, 6, 6, 6, 0, 0, 0, 0, 0}, FxRotTransPers4, nullptr, true},   // FE2:3: long(const short *v0, const short *v1, const short *v2, const short *v3, float *sxy0, float *sxy1, float *sxy2, float *sxy3, long *p)
    {FIELD_OURS(Gte_StoreDepthF4), 4, {kAll, kAll, kAll, kAll}, Answer::kGarbage, 0, 0, {0, 0, 0, 0}, FxStoreDepthF4, nullptr, true},   // FE2:3: void(float *out0, float *out1, float *out2, float *out3)
    {FIELD_OURS(TextRecord_Set), 3, {kU8, kU8, kAll}, Answer::kGarbage, 0, 0, {0, 0, 16}, nullptr, nullptr, true},   // FO:1 FS:2: void(unsigned slot, unsigned length, const unsigned char *text)
    {FIELD_OURS(Text_DrawFont12), 4, {kU16, kU16, 0x3Fu, kAll}, Answer::kGarbage, 0, 0, {0, 0, 0, kDerefString}, nullptr, nullptr, true},   // FO:1 FS:2: void(int x, int y, int colour, const unsigned char *text)
    {FIELD_OURS(Skill_FlagIndex), 1, {kU8}, Answer::kFlag, 0, 0, {0}, nullptr, nullptr, true},   // FO:1 FS:2: unsigned char(unsigned id)
    {FIELD_OURS(Menu_DrawItemRow), 7, {kU16, kU16, kU8, kU8, kU8, kU8, kU8}, Answer::kGarbage, 0, 0, {0, 0, 0, 0, 0, 0, 0}, nullptr, nullptr, true},   // FO:3: void(int x, int y, int colour, unsigned category, unsigned id, unsigned count, int dim)
    {FIELD_OURS(Menu_DrawBackdrop), 1, {kU8}, Answer::kGarbage, 0, 0, {0}, nullptr, nullptr, true},   // FS:3: void(unsigned kind)
    {FIELD_OURS(Menu_DrawMemberStatus), 4, {kU16, kU16, kU8, kU8}, Answer::kGarbage, 0, 0, {0, 0, 0, 0}, nullptr, nullptr, true},   // FS:3: unsigned long(int x, int y, unsigned member, unsigned highlight)
    {FIELD_OURS(Sprite_QueueOverlay), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FC1:2: void(void)
    {FIELD_OURS(EventOp_0x), 1, {kAll}, Answer::kGarbage, 0, 0, {16}, nullptr, nullptr, true},   // FC1:2: void(const unsigned char *op)
    {FIELD_OURS(AreaMap_Slope), 3, {kAll, kAll, kU8}, Answer::kGarbage, 0, 0, {0, 0, 0}, FxSloped, nullptr, true},   // FC1:2: long(long x, long y, unsigned long direction)
    {FIELD_OURS(Sprite_InitFromEntry), 1, {kAll}, Answer::kGarbage, 0, 0, {16}, nullptr, nullptr, true},   // FC1:1 FO:1: void(unsigned char *entry)
    {FIELD_OURS(Field_MembersFrame), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FC3:1 FE1:1: void(void)
    {FIELD_OURS(Field_RunTaskRecords), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FC3:2: void(void)
    {FIELD_OURS(Field_ObjectRandomTurn), 1, {kAll}, Answer::kFlag, 0, 0, {16}, nullptr, nullptr, true},   // FC3:2: unsigned char(unsigned char *object)
    {FIELD_OURS(Field_ObjectOpenDirection), 1, {kAll}, Answer::kFlag, 0, 0, {16}, nullptr, nullptr, true},   // FC3:2: unsigned char(unsigned char *object)
    {FIELD_OURS(Field_MemberTimers), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FC3:1 FE1:1: void(void)
    {FIELD_OURS(AreaMap_CellsNone), 5, {kAll, kAll, kAll, kAll, kAll}, Answer::kFlag, 0, 0, {0, 0, 0, 0, 0}, nullptr, nullptr, true},   // FC3:2: unsigned char(long x, long z, unsigned wide, unsigned code, unsigned mask)
    {FIELD_OURS(Field_JumpSetUp), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FE2:2: void(void)
    {"0x5B9550", 0x5B9550, 0x5B9550, 0, {}, Answer::kThrough, 0, 0, {}, nullptr, nullptr, true},   // FE2:2: the CRT's _ftol: pops st(0), answers edx:eax - called for real on both sides
    {FIELD_OURS(Gte_MulMatrix0), 3, {kAll, kAll, 0}, Answer::kGarbage, 0, 0, {18, 18, 0}, FxMulMatrix0, nullptr, true},   // FE2:2: short *(const short *a, const short *b, short *out)
    {FIELD_OURS(Gpu_SetPolyG3), 1, {kAll}, Answer::kGarbage, 0, 0, {16}, nullptr, nullptr, true},   // FE2:1 FO:1: void(unsigned char *prim)
    {FIELD_OURS(Gpu_SetShadeTex), 2, {kAll, kAll}, Answer::kGarbage, 0, 0, {16, 0}, nullptr, nullptr, true},   // FE2:2: void(unsigned char *prim, unsigned tge)
    {FIELD_OURS(Prim_SetTexture), 3, {kAll, kAll, kAll}, Answer::kGarbage, 0, 0, {0, 16, 0}, nullptr, nullptr, true},   // FE2:2: void(unsigned long texture, unsigned char *prim, int count)
    {FIELD_OURS(ItemTrade_DrawNeeds), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FE2:2: a window drawn
    {FIELD_OURS(ItemTrade_DrawBackground), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FE2:2: a window frame drawn
    {FIELD_OURS(ItemTrade_Lacks), 2, {kU8, kU8}, Answer::kFlag, 0, 0, {}, nullptr, nullptr, true},   // FE2:2: al: an inventory test (Inventory_Count)
    {FIELD_OURS(Item_HelpMessage), 2, {kU8, kU8}, Answer::kGarbage, 0, 0, {0, 0}, nullptr, nullptr, true},   // FE2:1 FS:1: unsigned(unsigned category, unsigned item)
    {FIELD_OURS(ItemTrade_DrawCount), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FE2:2: a window drawn
    {FIELD_OURS(Menu_DrawCell8), 6, {kU16, kU16, kU8, kU8, kU16, kU8}, Answer::kGarbage, 0, 0, {0, 0, 0, 0, 0, 0}, nullptr, nullptr, true},   // FO:2: unsigned long(unsigned x, unsigned y, unsigned u, unsigned v, unsigned clut, unsigned shade)
    {FIELD_OURS(Menu_ListScroll), 4, {kAll, 0, 0, kAll}, Answer::kFlag, 0, 0, {}, FxListScroll, nullptr, true},   // FO:1 FS:1: unsigned char(unsigned char *top, unsigned char *offset, unsigned char *moving, unsigned char *state)
    {FIELD_OURS(Menu_DrawScrollBar), 7, {kAll, kU8, kU16, kU16, kU8, kU8, kU8}, Answer::kGarbage, 0, 0, {16, 0, 0, 0, 0, 0, 0}, nullptr, nullptr, true},   // FO:1 FS:1: void(const unsigned char *items, unsigned top, int x, int y, unsigned rows, unsigned total, unsigned height)
    {FIELD_OURS(Party_ApplyRecord), 1, {kAll}, Answer::kGarbage, 0, 0, {16}, nullptr, nullptr, true},   // FO:2: void(unsigned char *context)
    {FIELD_OURS(Menu_YesNo), 0, {}, Answer::kFlag, 0, 0, {}, nullptr, nullptr, true},   // FS:2: unsigned char(void)
    {FIELD_OURS(Field_PartyLoad), 1, {kAll}, Answer::kGarbage, 0, 0, {0}, nullptr, nullptr, true},   // FS:2: void(unsigned slot)
    {FIELD_OURS(Char_RecalcStats), 1, {kAll}, Answer::kGarbage, 0, 0, {16}, nullptr, nullptr, true},   // FS:2: void(unsigned char *record)
    {FIELD_OURS(Gpu_SetLineF2), 1, {kAll}, Answer::kGarbage, 0, 0, {16}, nullptr, nullptr, true},   // FC1:1: void(unsigned char *prim)
    {FIELD_OURS(CameraTurn_Start), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FC1:1: void(void)
    {FIELD_OURS(CameraTurn_Step), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FC1:1: void(void)
    {FIELD_OURS(CameraTurn_End), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FC1:1: void(void)
    {FIELD_THEIRS(MoveCmd_Move, 0x578C10), 2, {0, kU8}, Answer::kGarbage, 0, 0, {16, 0}, nullptr, nullptr, true},   // FC1:1: void(unsigned char *object, unsigned char direction)
    {FIELD_OURS(Party_MemberAt), 3, {kAll, kAll, kAll}, Answer::kFlag, 0, 0, {0, 0, 0}, nullptr, nullptr, true},   // FC1:1: unsigned char(long x, long y, unsigned margin)
    {FIELD_OURS(Gte_VectorNormalS), 2, {kAll, kAll}, Answer::kGarbage, 0, 0, {12, 0}, FxOut1_6, nullptr, true},   // FC2:1: long(const long *in, short *out)
    {FIELD_OURS(Gte_VectorNormal), 2, {kAll, kAll}, Answer::kGarbage, 0, 0, {12, 0}, FxOut1_12, nullptr, true},   // FC2:1: long(const long *in, long *out)
    {FIELD_OURS(EffectGte_LoadMapCamera), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FC2:1: the camera matrices from Camera_Angles and the focus
    {FIELD_OURS(EffectGte_ProjectPoint), 2, {kAll, kAll}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FC2:1: a point projected (Gte_RotTransPers)
    {FIELD_OURS(EffectGte_ProjectSize), 3, {kAll, kAll, kAll}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FC2:1: a vector turned (Gte_RotTrans)
    {FIELD_OURS(Gpu_GetTPage), 4, {kAll, kAll, kAll, kAll}, Answer::kGarbage, 0, 0, {0, 0, 0, 0}, nullptr, nullptr, true},   // FC2:1: unsigned(unsigned tp, unsigned abr, int x, int y)
    {FIELD_OURS(AreaMap_Frame), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FC3:1: void()
    {FIELD_OURS(Party_ExtraScreens), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FC3:1: void(void)
    {FIELD_OURS(Party_UpdateScreens), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FC3:1: void(void)
    {FIELD_OURS(Mode11_ListedSpriteScreens), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // 0x5372E0, FC3:1: the party screens updated
    {FIELD_OURS(Effect_RunObjects), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FC3:1: void(void)
    {FIELD_OURS(MoveScript_TintFrame), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FC3:1: void(void)
    {FIELD_OURS(Field_DrawFrame), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FC3:1: void(void)
    {FIELD_OURS(BattleExtra_Dispatch), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // 0x42D710, FC3:1: jmp through 0x64ADAC by the menu byte 0x929F00
    {FIELD_OURS(Shisu_ModeDispatch), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // 0x57DFF0, FC3:1: jmp through 0x663DD0 by the menu byte 0x929F00
    {FIELD_OURS(Field_ObjectBlockedAhead), 1, {kAll}, Answer::kFlag, 0, 0, {16}, nullptr, nullptr, true},   // FC3:1: unsigned char(unsigned char *object)
    {FIELD_OURS(MoveScript_Step), 2, {kAll, kAll}, Answer::kByte, 0xFF, 0x0F, {16, 16}, FxStepFlag, nullptr, true},   // FC3:1: unsigned char(unsigned char *object, const unsigned char *script)
    {FIELD_OURS(Sprite_ShadeLower), 1, {kAll}, Answer::kFlag, 0, 0, {0}, nullptr, nullptr, true},   // FC3:1: unsigned char(unsigned step)
    {FIELD_OURS(MoveCmd_AttachOffset), 2, {0, kU8}, Answer::kGarbage, 0, 0, {0, 0}, FxOut0_12, nullptr, true},   // FC3:1: void(long *out, unsigned char index)
    {FIELD_OURS(Field_TileD0), 0, {}, Answer::kFlag, 0, 0, {}, nullptr, nullptr, true},   // FC3:1: unsigned char(void)
    {FIELD_OURS(Area_LinkAt), 2, {kU8, kU8}, Answer::kFlag, 0, 0, {0, 0}, nullptr, nullptr, true},   // FC3:1: unsigned char(unsigned x, unsigned z)
    {FIELD_OURS(Field_LeaderPushObjects), 0, {}, Answer::kFlag, 0, 0, {}, nullptr, nullptr, true},   // FC3:1: unsigned char(void)
    {FIELD_OURS(FieldPanel_KindPoints), 2, {kU8, kU16}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FE1:1: a word from the 36-byte records at 0x66A6AC scaled by the second argument
    {FIELD_OURS(FieldPanel_KindTotal), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FE1:1: the sum of 0x52CE60 over the 32 bytes at 0x9040EC
    {FIELD_OURS(Gpu_SetTile), 1, {kAll}, Answer::kGarbage, 0, 0, {16}, nullptr, nullptr, true},   // FE1:1: void(unsigned char *prim)
    {FIELD_OURS(Member_ClearState), 1, {kAll}, Answer::kGarbage, 0, 0, {0}, nullptr, nullptr, true},   // FE1:1: void(unsigned member)
    {FIELD_OURS(Field_LeaderStand), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FE1:1: void(void)
    {FIELD_OURS(Area104_LeaderRun), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FE1:1: void(void)
    {FIELD_OURS(Area121_LeaderRun), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FE1:1: void(void)
    {FIELD_OURS(Zenny_Add), 2, {kAll, kAll}, Answer::kFlag, 0, 0, {0, 0}, FxZennyAdd, nullptr, true},   // FE1:1: unsigned char(unsigned amount, unsigned tally)
    {FIELD_OURS(Field_CellHasEvent), 2, {kAll, kAll}, Answer::kFlag, 0, 0, {0, 0}, nullptr, nullptr, true},   // FE1:1: unsigned char(long x, long z)
    {FIELD_OURS(Char_LoseHp), 2, {kU16, kU8}, Answer::kGarbage, 0, 0, {0, 0}, nullptr, nullptr, true},   // FE2:1: unsigned(unsigned amount, unsigned member)
    {FIELD_OURS(Char_LoseAp), 2, {kU16, kU8}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // 0x537500, FE2:1: two words, no calls
    {FIELD_OURS(Actor_EquipCount), 3, {kAll, kAll, kAll}, Answer::kFlag, 0, 0, {0, 0, 0}, nullptr, nullptr, true},   // FE2:1: unsigned char(unsigned member, unsigned kind, unsigned value)
    {FIELD_OURS(Field_CellsBlock), 3, {kAll, kAll, kAll}, Answer::kFlag, 0, 0, {0, 0, 0}, nullptr, nullptr, true},   // FE2:1: unsigned char(long x, long z, unsigned wide)
    {FIELD_OURS(Gte_RotTransPers), 3, {kAll, 0, 0}, Answer::kGarbage, 0, 0, {6, 0, 0}, FxRotTransPers, nullptr, true},   // FE2:1: long(const short *vertex, unsigned long *sxy, long *p)
    {FIELD_OURS(Gte_PrimDepthFlat4_10), 1, {kAll}, Answer::kGarbage, 0, 0, {16}, nullptr, nullptr, true},   // FE2:1: void(void *prim)
    {FIELD_OURS(Gte_RotTransPers3), 7, {kAll, kAll, kAll, 0, 0, 0, 0}, Answer::kGarbage, 0, 0, {6, 6, 6, 0, 0, 0, 0}, FxRotTransPers3, nullptr, true},   // FE2:1: long(const short *v0, const short *v1, const short *v2, float *sxy0, float *sxy1, float *sxy2, long *p)
    {FIELD_OURS(Gte_PrimDepths3_10B), 1, {kAll}, Answer::kGarbage, 0, 0, {16}, nullptr, nullptr, true},   // FE2:1: void(void *prim)
    {FIELD_OURS(Scena17_DrawLogo), 2, {kAll, kAll}, Answer::kGarbage, 0, 0, {0, 0}, nullptr, nullptr, true},   // FE2:1: void(int x, int y)
    {FIELD_OURS(Area_CellHook), 2, {kAll, kAll}, Answer::kGarbage, 0, 0, {0, 0}, nullptr, nullptr, true},   // FE2:1: int(unsigned x, unsigned z)
    {FIELD_OURS(Snd_LoadBankFile), 1, {kAll}, Answer::kGarbage, 0, 0, {0}, nullptr, nullptr, true},   // FE2:1: void(unsigned index)
    {FIELD_OURS(MasterTalk_Dispatch), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // 0x586670, FE2:1: jmp through 0x66450C by the byte 0x9398CF
    {FIELD_OURS(Gfx_ClutStripCopyRow), 1, {kAll}, Answer::kGarbage, 0, 0, {0}, nullptr, nullptr, true},   // FE2:1: void(unsigned row)
    {FIELD_OURS(MasterTalk_Reset), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // 0x585A00, FE2:1: no arguments, no calls
    {FIELD_OURS(Gpu_SetPolyG4), 1, {kAll}, Answer::kGarbage, 0, 0, {16}, FxArg0, nullptr, true},   // FE2:1: unsigned char *(unsigned char *prim)
    {FIELD_OURS(ItemTrade_RowCount), 0, {}, Answer::kFlag, 0, 0, {}, nullptr, nullptr, true},   // FE2:1: al (the caller stores it at 0x6BE08D)
    {FIELD_OURS(ItemTrade_TakeNeeds), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // 0x594D90, FE2:1: Inventory_Remove behind a test
    {FIELD_OURS(Char_ExpForLevel), 2, {kU8, kU8}, Answer::kGarbage, 0, 0, {0, 0}, nullptr, nullptr, true},   // FO:1: int(unsigned member, unsigned level)
    {FIELD_OURS(Gpu_SetSprt), 1, {kAll}, Answer::kGarbage, 0, 0, {16}, nullptr, nullptr, true},   // FO:1: void(unsigned char *prim)
    {FIELD_OURS(Equip_PreviewSet), 4, {kU8, kAll, 0, 0}, Answer::kGarbage, 0, 0, {0, 6, 0, 0}, FxPreviewSet, nullptr, true},   // FO:1: void(unsigned id, const unsigned char *set, unsigned char *marks, unsigned short *values)
    {FIELD_OURS(Char_AbilityList), 3, {kU8, kU8, kU8}, Answer::kGarbage, 0, 0, {0, 0, 0}, FxText, nullptr, true},   // FO:1: unsigned char *(unsigned member, unsigned type, unsigned battle)
    {FIELD_OURS(Skill_CanUse), 3, {kU8, kU8, kU8}, Answer::kFlag, 0, 0, {0, 0, 0}, nullptr, nullptr, true},   // FO:1: unsigned char(unsigned mode, unsigned member, unsigned id)
    {FIELD_OURS(Skill_ApCost), 3, {kU8, kU8, kU8}, Answer::kFlag, 0, 0, {0, 0, 0}, nullptr, nullptr, true},   // FO:1: unsigned char(unsigned member, unsigned id, unsigned battle)
    {FIELD_OURS(Item_CanUse), 4, {kU8, kU8, kU8, kU8}, Answer::kFlag, 0, 0, {0, 0, 0, 0}, nullptr, nullptr, true},   // FO:1: unsigned char(unsigned mode, unsigned member, unsigned category, unsigned item)
    {FIELD_OURS(Item_IconKind), 2, {kU8, kU8}, Answer::kByte, 2, 7, {0, 0}, nullptr, nullptr, true},   // FO:1: unsigned(unsigned category, unsigned item)
    {FIELD_OURS(MoveCmd_TestFC), 2, {kU16, kU16}, Answer::kFlag, 0, 0, {0, 0}, nullptr, nullptr, true},   // FO:1: unsigned char(short x, short z)
    {FIELD_THEIRS(Effect_Spawn, 0x57CE10), 5, {kU8, kU8, kU8, kU16, kU16}, Answer::kFlag, 0, 0, {0, 0, 0, 0, 0}, nullptr, nullptr, true},   // FO:1: unsigned char(unsigned char kind, signed char a, signed char b, short x, short z)
    {FIELD_OURS(Party_MoveMember), 2, {kAll, kU8}, Answer::kGarbage, 0, 0, {16, 0}, nullptr, nullptr, true},   // FO:1: void(unsigned char *object, unsigned char direction)
    {FIELD_OURS(PartyRecord_Clear), 1, {kAll}, Answer::kGarbage, 0, 0, {0}, nullptr, nullptr, true},   // FO:1: void(unsigned index)
    {FIELD_OURS(MoveScript_ObjectKind), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FO:1: int(void)
    {FIELD_OURS(SaveMenu_DrawSlots), 3, {kAll, kAll, kAll}, Answer::kGarbage, 0, 0, {0, 0, 0}, nullptr, nullptr, true},   // FS:1: void(int x, int y, unsigned highlight)
    {FIELD_OURS(Area_RunPlacement), 1, {kAll}, Answer::kGarbage, 0, 0, {16}, nullptr, nullptr, true},   // FS:1: void(const unsigned char *script)
    {FIELD_OURS(Menu_DrawItemIcon), 4, {kU16, kU16, kU8, kU8}, Answer::kGarbage, 0, 0, {0, 0, 0, 0}, nullptr, nullptr, true},   // FS:1: void(int x, int y, int icon, int shade)
    {FIELD_OURS(Menu_DrawBorder), 4, {kU16, kU16, kU8, kU8}, Answer::kGarbage, 0, 0, {0, 0, 0, 0}, nullptr, nullptr, true},   // FS:1: void(int x, int y, int w, int h)
    {FIELD_OURS(Menu_DrawMoneyBox), 4, {kU16, kU16, 0, kAll}, Answer::kGarbage, 0, 0, {0, 0, 0, 0}, nullptr, nullptr, true},   // FS:1: void(int x, int y, int unused, unsigned value)
    {FIELD_OURS(Inventory_Remove), 3, {kAll, kAll, kAll}, Answer::kFlag, 0, 0, {0, 0, 0}, nullptr, nullptr, true},   // FS:1: unsigned char(unsigned category, unsigned item, unsigned count)
    {FIELD_OURS(AbilityList_Add), 4, {kU8, kU8, kU8, kU8}, Answer::kFlag, 0, 0, {0, 0, 0, 0}, nullptr, nullptr, true},   // FS:1: unsigned char(unsigned id, unsigned member, unsigned shared, unsigned which)
    {FIELD_OURS(AbilityList_CountSet), 3, {kU8, kU8, kU8}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // 0x591AC0, FS:1: a record's list pointer by member and page (the caller keeps a byte)
    // Round fourteen's fold (docs/scenario_harness.md section 8.11): R1C's six
    // field callees no standard row had (rest_1c_fuzz.cpp's listing, the masks
    // R1A, R1B, R1D, R1E and R1F agree on), so later groups need not re-list them
    {FIELD_OURS(Effect_SpawnAtCellHigh), 3, {kU8, kU16, kU16}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // void(unsigned state, unsigned x, unsigned z): the state's byte, x and z movsx words
    {FIELD_OURS(Effect_SpawnAtCell), 3, {kU8, kU16, kU16}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // the same
    {FIELD_OURS(Field_GiveZenny), 1, {kAll}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // void(unsigned amount): a byte product pushed whole
    {FIELD_OURS(AreaMap_ClearCell), 2, {kU16, kU16}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // void(unsigned x, unsigned z): 16 bits each (docs/field_hidden.md section 3)
    {FIELD_OURS(Field_EffectAhead), 0, {}, Answer::kByte, 0xFF, 0x13, {}, nullptr, nullptr, true},   // unsigned char(void): a record 0..19 or none 0xFF (all it answers; event_ops.cpp)
    {FIELD_OURS(Sprite_TurnSense), 1, {kU8}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // unsigned char(unsigned target): the target's byte (static_cast<unsigned char> in inventory_ops.cpp; R1A, R1D); the callers store al, whatever it is
    // ... and the two effect-standard draws the field runs reach (R4B: rest_2d.cpp,
    // rest_4b.cpp, rest_4e.cpp call them in field mode, each group listing them
    // itself): the same rows as kEffectStd's, which they stand over in effect mode
    {FIELD_OURS(Gpu_SetLineF3), 1, {0}, Answer::kGarbage, 0, 0, {16}, FxArg0, nullptr, true},   // unsigned char *(unsigned char *prim)
    {FIELD_OURS(Gpu_SetSprt16), 1, {0}, Answer::kGarbage, 0, 0, {16}, nullptr, nullptr, true},   // void(unsigned char *prim)
};
#undef FIELD_OURS
#undef FIELD_THEIRS

// --- round thirteen: the effect-standard callees -------------------------------------
//
// The frontier of the effect runs (docs/scenario_harness.md section 8.5): every
// function the 1,695 rows of analysis/round13_cut.tsv call or tail-jump to that
// is not a row of the cut, not in kStandard and not in kField - by EKH's pass
// at d19d803 (capstone, each row to its extent). Registered in effect mode only,
// after kField, so no group before round thirteen sees a difference. Ours typed
// from symbols.toml's ret / params (as FH typed kField: a pointer argument
// hashed where Readable - 16 bytes of char / void, 8 of short, 12 of long - an
// out-parameter not logged and filled with noise); Capcom's unnamed ones typed
// by reading each to its last instruction: the stack words it reads (their
// widths give the masks), which it dereferences (hashed to the furthest byte
// read) and writes through (not logged, filled to the furthest byte written),
// and whether the callers read al or eax. `guard` on all.
//
// kEffectOverrides re-list, in effect mode only and before kStandard / kField,
// the entries round thirteen's callers need louder: Effect_FindFree answers a
// record that is free, Effect_Release clears what the real one clears, and the
// five rows of the cut that other groups call raw until their owners merge
// (0x52CFE0, 0x52CF60 of E1F; 0x469750, 0x468AC0 of E1B; 0x503FA0 of E5D) draw
// into the packet buffer and move the cursor as the originals' commits do.
#define FX_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
#define FX_RAW(address) #address, address, address

// The packet cursor moved by n bytes, the n filled with noise, while the packet
// stays inside the harness's buffer (0x40 to spare); answers where it was.
std::uint32_t Drew(unsigned n) {
    unsigned char* const next = Pointer(at::kPacketNext);
    if (next >= g_packets && next + n + 0x40 <= g_packets + sizeof g_packets) {
        FillBytes(next, n);
        SetPointer(at::kPacketNext, next + n);
    }
    return Key(next);
}
std::uint32_t FxFindFree(const std::uint32_t*, std::uint32_t answer) {
    // a quarter of the time none (0xFF); else a free record (+0 == 0) looked for
    // from a start the answer picks - not always the first, so a caller that
    // finds its own record instead of using the answer shows - 0xFF when none
    // is free; the rest of eax the answer's (the callers read al)
    const std::uint32_t high = answer & 0xFFFFFF00u;
    if ((answer >> 8) % 4 == 0) return high | 0xFF;
    const unsigned from = (answer >> 12) % at::kEffectCount;
    for (unsigned i = 0; i < at::kEffectCount; ++i) {
        const unsigned k = (from + i) % at::kEffectCount;
        if (EffectRecord(k)[0] == 0) return high | k;
    }
    return high | 0xFF;
}
std::uint32_t FxRelease(const std::uint32_t*, std::uint32_t answer) {
    // bytes 0..4 of Sprite_Current to 0 (symbols.toml Effect_Release)
    auto* const s = static_cast<unsigned char*>(Sprite_Current);
    if (InRegions(s, 5)) std::memset(s, 0, 5);
    return answer;
}
std::uint32_t FxReleaseAt(const std::uint32_t* a, std::uint32_t answer) {
    // bytes 0..4 of record (index & 0xFF) to 0; an index past the 20 (the real
    // one writes past the pool) is left alone and logged
    if ((a[0] & 0xFF) < at::kEffectCount) std::memset(EffectRecord(a[0] & 0xFF), 0, 5);
    return answer;
}
// 0x52CFE0 (id, slot, x, y): a sprite primitive of 0x1C at the cursor, committed;
// eax the primitive. 0x52CF60 (id, slot): a draw mode of 0xC, committed.
std::uint32_t FxSprtPrim(const std::uint32_t*, std::uint32_t) { return Drew(0x1C); }
std::uint32_t FxModePrim(const std::uint32_t*, std::uint32_t answer) { Drew(0xC); return answer; }
// 0x469750 (x, y, w, h, colour): a frame 0x469790 (x, y, w, h) and a fill
// 0x469960 (x + 2, y + 2, w - 5, h - 5, colour), each several primitives: the
// stand-in moves the cursor by 0x80, a size of its own (the originals' sum
// depends on the path), so that a caller reading the cursor after it sees it move.
std::uint32_t FxBoxPrims(const std::uint32_t*, std::uint32_t answer) { Drew(0x80); return answer; }
// 0x468AC0 (x, y, bits): three boxes 0x468BB0 (x + 0x30 i, y, bit i of the byte)
// and three Text_DrawAt lines; 0xC0 of its own, as above.
std::uint32_t FxPanelPrims(const std::uint32_t*, std::uint32_t answer) { Drew(0xC0); return answer; }
// 0x503FA0 (variant): nothing unless Draw_PassFlags has bit 2 (E5D's reading,
// 2026-10-03: EKH had the test the other way round); then sixteen
// textured quads around Sprite_Current's point (a draw mode of 0xC and a quad
// linked at 0x48 each), the four vertices in Prim_VertexScratch 0x9037A0.. and
// MapView_ScreenXY 0x903820 written on the way - both filled here.
std::uint32_t FxShadow(const std::uint32_t*, std::uint32_t answer) {
    if (!(Byte(0x7E0918)[0] & 4)) return answer;
    if (InRegions(Mem(at::kVertexScratch), 0x20)) FillBytes(Mem(at::kVertexScratch), 0x20);
    FillFloats(0x903820, 2);
    Drew(16 * 0x54);
    return answer;
}
std::uint32_t FxItemAt(const std::uint32_t*, std::uint32_t answer) {
    // MapView_ItemAt: 0 (outside the view) a third of the time, else a small
    // draw item (the callers index DrawItems by it; a random 12 bits would reach
    // 0x90 * 0xFFF past it)
    return answer % 3 == 0 ? 0u : 1u + (answer >> 8) % 0x3F;
}
std::uint32_t FxItemHalf(const std::uint32_t*, std::uint32_t answer) {
    // MapView_ItemHalfAt: 0 a third of the time, else a 0x48-byte half the
    // callers read and write, in the harness's own buffer
    return answer % 3 == 0 ? 0u : Key(g_own + (answer & 0x70));
}
std::uint32_t FxRecordIndex(const std::uint32_t*, std::uint32_t answer) { return answer % 12; }   // 0..10, 11 none
std::uint32_t FxMemberState2_8(const std::uint32_t* a, std::uint32_t answer) {
    // ObjTrio + member * 0x14C: +1 = 2, +2 = 8, +3 = 0, +0xB = the value's byte
    // (the member unchecked in the original; past the three it is left alone)
    if ((a[0] & 0xFF) < 3) {
        unsigned char* const o = ObjectOf(a[0] & 0xFF);
        o[1] = 2;
        o[2] = 8;
        o[3] = 0;
        o[0xB] = static_cast<unsigned char>(a[1]);
    }
    return answer;
}
std::uint32_t FxToggle(const std::uint32_t* a, std::uint32_t answer) {
    // bits[(index & 0xFF) >> 3] ^= 1 << (index & 7), inside the regions
    auto* const p = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a[0])) + ((a[1] & 0xFF) >> 3);
    if (InRegions(p, 1)) *p ^= static_cast<unsigned char>(1u << (a[1] & 7));
    return answer;
}
std::uint32_t FxLinkPrim(const std::uint32_t* a, std::uint32_t answer) {
    // *tail = item
    auto* const p = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a[0]));
    if (Writable(p, 4)) std::memcpy(p, &a[1], 4);
    return answer;
}
std::uint32_t FxObjectMatrix(const std::uint32_t* a, std::uint32_t) {
    // the rotation (nine s16) and the translation (three s32 at +0x14), not the
    // padding word at +0x12 (DIV-0021)
    FillNoise(a[0], 18);
    FillNoise(a[0] + 0x14, 12);
    return a[0];
}
std::uint32_t FxOut0_F1(const std::uint32_t* a, std::uint32_t answer) { FillFloats(a[0], 1); return answer; }
std::uint32_t FxOut012_F1(const std::uint32_t* a, std::uint32_t answer) {
    for (unsigned i = 0; i < 3; ++i) FillFloats(a[i], 1);
    return answer;
}
std::uint32_t FxOut0_8(const std::uint32_t* a, std::uint32_t answer) { FillNoise(a[0], 8); return answer; }
std::uint32_t FxOut0_18(const std::uint32_t* a, std::uint32_t answer) { FillNoise(a[0], 18); return answer; }
std::uint32_t FxOut0_24(const std::uint32_t* a, std::uint32_t answer) { FillNoise(a[0], 24); return answer; }
std::uint32_t FxOut0_32(const std::uint32_t* a, std::uint32_t answer) { FillNoise(a[0], 32); return answer; }
std::uint32_t FxOut1_4(const std::uint32_t* a, std::uint32_t answer) { FillNoise(a[1], 4); return answer; }
std::uint32_t FxOut2_6(const std::uint32_t* a, std::uint32_t answer) { FillNoise(a[2], 6); return answer; }
// Round twelve's behaviour folds (its doc section 7 items 1 and 6), beside the
// standard rows, in effect mode only - in place they would change what the
// wave-two groups that use the standard rows draw:
std::uint32_t FxSpriteSlot(const std::uint32_t*, std::uint32_t answer) {
    // Sprite_FindFree: a sprite slot 0..29, or 0xFF (none) a third of the time (FC1)
    return (answer & 0xFFFFFF00u) | (answer % 3 == 0 ? 0xFFu : (answer >> 8) % 30);
}
std::uint32_t FxMemberAt(const std::uint32_t*, std::uint32_t answer) {
    // Party_MemberAt: a member 0..2, or 0xFF a third of the time (FC1)
    return (answer & 0xFFFFFF00u) | (answer % 3 == 0 ? 0xFFu : (answer >> 8) % 3);
}
std::uint32_t FxRotTransPers2F(const std::uint32_t* a, std::uint32_t answer) {
    // Gte_RotTransPers: the screen point as two floats, eight bytes (FE2: the
    // standard FxRotTransPers fills four and a caller reads the second float),
    // the depth a word
    FillFloats(a[1], 2);
    FillNoise(a[2], 4);
    return answer;
}
std::uint32_t FxPrim0_44(const std::uint32_t* a, std::uint32_t answer) { FillNoise(a[0], 0x2C); return answer; }
std::uint32_t FxPrim0_12(const std::uint32_t* a, std::uint32_t answer) { FillNoise(a[0], 12); return answer; }
// Wave two's fold (round thirteen's doc section 13): the rows five groups
// re-listed in their own fuzz files, here for the waves after them.
// A finite float with a fraction, 2^-17..2^18 in size, either sign: the callers
// round (_ftol) and compare, which FillFloats' whole numbers do not test.
void FillFractions(std::uint32_t at, unsigned n) {
    void* const p = reinterpret_cast<void*>(static_cast<std::uintptr_t>(at));
    if (!Writable(p, 4 * n)) return;
    for (unsigned i = 0; i < n; ++i) {
        const std::uint32_t bits = (Noise() & 0x807FFFFFu) | ((0x6Eu + Noise() % 0x24u) << 23);
        std::memcpy(static_cast<unsigned char*>(p) + 4 * i, &bits, 4);
    }
}
std::uint32_t FxProjectPoint(const std::uint32_t* a, std::uint32_t answer) {
    // EffectGte_ProjectPoint(in, out): out[0..2] the screen x, y and depth, floats
    FillFractions(a[1], 3);
    return answer;
}
std::uint32_t FxProjectSize(const std::uint32_t* a, std::uint32_t answer) {
    // EffectGte_ProjectSize(in, size, out): out's two s16, small and positive
    // half the time, as a radius is
    void* const p = reinterpret_cast<void*>(static_cast<std::uintptr_t>(a[2]));
    if (Writable(p, 4)) {
        const std::uint32_t n = Noise();
        const std::uint32_t v = (n & 1) ? (n >> 1) : ((n >> 1) & 0x003F003Fu);
        std::memcpy(p, &v, 4);
    }
    return answer;
}
std::uint32_t FxCosNot0(const std::uint32_t*, std::uint32_t answer) {
    // Math_Cos: anything but 0 and -1 (E2B: a caller divides by Math_Cos(0x80),
    // and the original's idiv faults on a 0 and on 0x80000000 / -1)
    return answer == 0 || answer == 0xFFFFFFFFu ? 1u : answer;
}
std::uint32_t FxOnCurrent80(const std::uint32_t*, std::uint32_t answer) {
    // Sprite_UpdateScreen draws Sprite_Current: its address and record logged
    auto* const s = static_cast<unsigned char*>(Sprite_Current);
    Note(Key(s));
    if (InRegions(s, 0x80)) NoteBytes(s, 0x80);
    return answer;
}
std::uint32_t FxLinkAdvance(const std::uint32_t* a, std::uint32_t answer) {
    // MapView_LinkPrimAt: the cursor += size & 0xFF when the row is on the map,
    // two times in three here; nothing filled - the caller wrote the primitive
    if (Noise() % 3 != 0) {
        unsigned char* const next = Pointer(at::kPacketNext);
        const unsigned n = a[3] & 0xFF;
        if (next >= g_packets && next + n + 0x40 <= g_packets + sizeof g_packets) SetPointer(at::kPacketNext, next + n);
    }
    return answer;
}
std::uint32_t FirstFree(std::uint32_t base, unsigned stride, unsigned n, std::uint32_t answer) {
    // as the real finds: the first record whose +0 is 0, or null; null also a
    // quarter of the time, and whenever the pool is not in the regions
    auto* const p = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(base));
    if (answer % 4 == 0 || !InRegions(p, stride * n)) return 0;
    for (unsigned i = 0; i < n; ++i)
        if (p[i * stride] == 0) return base + i * stride;
    return 0;
}
// EffectSpark_FindFree 0x47CF20: 8 records of 0x1C at EffectKind30_Shards;
// 0x47A130: 64 of 0x20 at 0x92D1DC (E2D's reading of both)
std::uint32_t FxSparkFindFree(const std::uint32_t*, std::uint32_t answer) { return FirstFree(at::kShards, 0x1C, 8, answer); }
std::uint32_t FxDustFindFree(const std::uint32_t*, std::uint32_t answer) { return FirstFree(0x92D1DC, 0x20, 64, answer); }
// The round's end fold (docs/scenario_harness.md section 8.10).
// 0x4FEE70: the three story flags of 0x65DE60 as bits, plus 1 - lea eax,
// [edi + 1] at 0x4FEE9F, so the whole eax is 1..8 and its caller
// EffectKind18_09 (0x4FEDE3: cmp ecx, eax) compares all of it with the record's
// +2. The kByte answer's low byte 1..8 with the rest of eax cleared; a quarter
// of the time Sprite_Current's +2 when that is 1..8, so the "unchanged" path
// runs (E5A's FxPattern, effect_5a_fuzz.cpp).
std::uint32_t FxPattern(const std::uint32_t*, std::uint32_t answer) {
    const auto* const s = static_cast<const unsigned char*>(Sprite_Current);
    if ((answer >> 16) % 4 == 0 && InRegions(s, 0x80) && s[2] >= 1 && s[2] <= 8) return s[2];
    return answer & 0xFF;
}

const Callee kEffectOverrides[] = {
    {FX_OURS(Effect_FindFree), 0, {}, Answer::kByte, 0xFF, 0x13, {}, FxFindFree, nullptr, true},   // E1A:4 E1B:6 E1D:3 E2C:1 E3A:1 E3B:4 E5C:2 E5D:2: unsigned char(void)
    {FX_OURS(Effect_Release), 0, {}, Answer::kGarbage, 0, 0, {}, FxRelease, nullptr, true},   // 29 groups, 135 sites: void(void)
    {FX_OURS(UiSprite_Draw), 4, {kU8, kAll, kU16, kU16}, Answer::kGarbage, 0, 0, {}, FxSprtPrim, nullptr, true},   // E1F's; 100 sites in the cut: (id byte, slot, s16 x, s16 y) -> the primitive
    {FX_OURS(UiSprite_SetMode), 2, {kU8, kAll}, Answer::kGarbage, 0, 0, {}, FxModePrim, nullptr, true},   // E1F's; 31 sites: (id byte, slot)
    {FX_OURS(Panel_DrawWindow), 5, {kAll, kAll, kAll, kAll, kAll}, Answer::kGarbage, 0, 0, {}, FxBoxPrims, nullptr, true},   // E1B's; 33 sites: (int x, int y, int w, int h, colour)
    {FX_OURS(EffectKind0F_DrawToggles), 3, {kAll, kAll, kU8}, Answer::kGarbage, 0, 0, {}, FxPanelPrims, nullptr, true},   // E1B's; 19 sites: (int x, int y, bits byte)
    {FX_OURS(EffectKind18Sub17_DrawPatch), 1, {kAll}, Answer::kGarbage, 0, 0, {}, FxShadow, nullptr, true},   // E5D's; 15 sites: (variant, a whole word added to a table address)
    // round twelve's behaviour folds (section 8.6), for the effect groups that call them
    {FX_OURS(Sprite_FindFree), 0, {}, Answer::kFlag, 0, 0, {}, FxSpriteSlot, nullptr, true},   // E2D:2 E2F:2 E4A:2: unsigned char(void)
    {FX_OURS(Party_MemberAt), 3, {kAll, kAll, kAll}, Answer::kFlag, 0, 0, {}, FxMemberAt, nullptr, true},   // E2A:2 E3C:2 E5D:1: unsigned char(long x, long y, unsigned margin)
    {FX_OURS(Gte_RotTransPers), 3, {kAll, 0, 0}, Answer::kGarbage, 0, 0, {8, 0, 0}, FxRotTransPers2F, nullptr, true},   // 22 sites, 11 groups: long(const short *vertex, unsigned long *sxy, long *p)
    // wave two's fold (round thirteen's doc section 13): no pointer logged by value - the callers hand locals, whose addresses differ between the copy and ours
    {FX_OURS(EffectGte_ProjectPoint), 2, {0, 0}, Answer::kGarbage, 0, 0, {12, 0}, FxProjectPoint, nullptr, true},   // the point hashed, out three fractional floats
    {FX_OURS(EffectGte_ProjectSize), 3, {0, 0, 0}, Answer::kGarbage, 0, 0, {12, 2, 0}, FxProjectSize, nullptr, true},   // the point hashed, the size's first word only (the second is stack the original never wrote in several callers), out two s16
    {FX_OURS(Gte_VectorNormal), 2, {0, 0}, Answer::kGarbage, 0, 0, {12, 0}, FxOut1_12, nullptr, true},   // in hashed, out three longs filled
    {FX_OURS(Math_Cos), 1, {kAll}, Answer::kGarbage, 0, 0, {}, FxCosNot0, nullptr, true},   // never 0 or -1
    {FX_OURS(Sprite_UpdateScreen), 0, {}, Answer::kGarbage, 0, 0, {}, FxOnCurrent80, nullptr, true},   // Sprite_Current and its 0x80 bytes logged
    {FX_OURS(MapView_LinkPrimAt), 4, {kAll, kAll, kU8, kU8}, Answer::kGarbage, 0, 0, {}, FxLinkAdvance, nullptr, true},   // the cursor moves by the size two times in three; x, z whole (the low word compared with 0, the high word summed), dy a signed byte (movsx edx, byte [esp+0x14] at 0x572FD6), size its low byte (and edi, 0xFF at 0x572FF6) - world_map.cpp's reading, E5A's (round thirteen's end fold)
};

const Callee kEffectStd[] = {
    // ours, typed from symbols.toml (sites in the cut: groups)
    {FX_OURS(Gte_PrimDepths4_10), 1, {0}, Answer::kGarbage, 0, 0, {16}, nullptr, nullptr, true},   // 53: void(void *prim)
    {FX_OURS(Gpu_SetTile1), 1, {0}, Answer::kGarbage, 0, 0, {16}, nullptr, nullptr, true},   // 23: void(unsigned char *prim)
    {FX_OURS(FieldPanel_DrawKindIcon), 3, {kAll, kAll, kAll}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // 18: void(int x, int y, unsigned kind)
    {FX_OURS(Gpu_SetPolyF4), 1, {0}, Answer::kGarbage, 0, 0, {16}, FxArg0, nullptr, true},   // 17: unsigned char *(unsigned char *prim)
    {FX_OURS(FieldPanel_DrawHeader), 2, {kAll, kAll}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // 16: void(int x, int y)
    {FX_OURS(FieldPanel_DrawShade), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // 16 (13 tail jumps): void(void)
    {FX_OURS(FieldPanel_DrawKindRow), 3, {kAll, kAll, kAll}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // 15: void(unsigned kind, unsigned count, unsigned row)
    {FX_OURS(FieldPanel_DrawTotal), 2, {kAll, kAll}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // 12: void(int x, int y)
    {FX_OURS(Gpu_SetDrawMove), 4, {0, 0, kAll, kAll}, Answer::kGarbage, 0, 0, {16, 8}, FxArg0, nullptr, true},   // 12: unsigned char *(unsigned char *prim, const unsigned char *rect, unsigned long x, unsigned long y)
    {FX_OURS(Math_Ratan2), 2, {kAll, kAll}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // 12: int(float y, float x)
    {FX_OURS(Gte_StoreDepthF), 1, {0}, Answer::kGarbage, 0, 0, {}, FxOut0_F1, nullptr, true},   // 11: void(float *out)
    {FX_OURS(FieldPanel_DrawMessage), 3, {kAll, kAll, kAll}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // 10: void(int x, int y, unsigned id)
    {FX_OURS(Gpu_SetPolyGT4), 1, {0}, Answer::kGarbage, 0, 0, {16}, nullptr, nullptr, true},   // 10: void(unsigned char *prim)
    {FX_OURS(Gte_RotMatrixZ), 2, {kAll, 0}, Answer::kGarbage, 0, 0, {}, FxRotMatrix, nullptr, true},   // 7: short *(int angle, short *matrix)
    {FX_OURS(Gte_RotMatrixX), 2, {kAll, 0}, Answer::kGarbage, 0, 0, {}, FxRotMatrix, nullptr, true},   // 6: short *(int angle, short *matrix)
    {FX_OURS(Gte_RotMatrixY), 2, {kAll, 0}, Answer::kGarbage, 0, 0, {}, FxRotMatrix, nullptr, true},   // 6: short *(int angle, short *matrix)
    {FX_OURS(MapView_ItemAt), 2, {kAll, kAll}, Answer::kGarbage, 0, 0, {}, FxItemAt, nullptr, true},   // 7: unsigned long(long x, long y)
    {FX_OURS(Member_SetState2_8), 2, {kU8, kU8}, Answer::kGarbage, 0, 0, {}, FxMemberState2_8, nullptr, true},   // 4: void(unsigned member, unsigned value)
    {FX_OURS(Sprite_ReleaseTint), 1, {0}, Answer::kGarbage, 0, 0, {16}, nullptr, nullptr, true},   // 4: void(unsigned char *sprite)
    {FX_OURS(Window_DrawFrame), 4, {kAll, kAll, kAll, kAll}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // 3: void(int x, int y, int w, int h)
    {FX_OURS(Gte_StoreDepthF3), 3, {0, 0, 0}, Answer::kGarbage, 0, 0, {}, FxOut012_F1, nullptr, true},   // 3: void(float *out0, float *out1, float *out2)
    {FX_OURS(Gte_PrimDepths4_14), 1, {0}, Answer::kGarbage, 0, 0, {16}, nullptr, nullptr, true},   // 3: void(void *prim)
    {FX_OURS(FieldPanel_DrawBox2), 2, {kAll, kAll}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // 3: void(int x, int y)
    {FX_OURS(Gpu_SetLineG2), 1, {0}, Answer::kGarbage, 0, 0, {16}, nullptr, nullptr, true},   // 5: void(unsigned char *prim)
    {FX_OURS(Gte_PrimDepths4_10B), 1, {0}, Answer::kGarbage, 0, 0, {16}, nullptr, nullptr, true},   // 5: void(void *prim)
    {FX_OURS(WorldMap_RecordIndex), 0, {}, Answer::kGarbage, 0, 0, {}, FxRecordIndex, nullptr, true},   // 2: unsigned(void), 0..11
    {FX_OURS(Gpu_SetLineF3), 1, {0}, Answer::kGarbage, 0, 0, {16}, FxArg0, nullptr, true},   // 2: unsigned char *(unsigned char *prim)
    {FX_OURS(Area146_DrawGlowCylinder), 1, {0}, Answer::kGarbage, 0, 0, {12}, nullptr, nullptr, true},   // 2: void(const long *point)
    {FX_OURS(Gfx_ClearRect), 4, {kAll, kAll, kAll, kAll}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // 2: void(int x, int y, int w, int h)
    {FX_OURS(Window_DrawOutline), 4, {kAll, kAll, kAll, kAll}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // 2: void(int x, int y, int w, int h)
    {FX_OURS(BareRet), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // 2: void(void)
    {FX_OURS(Gte_PrimDepthFlat4_14), 1, {0}, Answer::kGarbage, 0, 0, {16}, nullptr, nullptr, true},   // 2: void(void *prim)
    {FX_OURS(Gte_PrimDepths4_0C), 1, {0}, Answer::kGarbage, 0, 0, {16}, nullptr, nullptr, true},   // 2: void(void *prim)
    {FX_OURS(Gpu_LinkPrim), 2, {kAll, kAll}, Answer::kGarbage, 0, 0, {}, FxLinkPrim, nullptr, true},   // 2: void(unsigned long *tail, unsigned long item)
    {FX_OURS(MapView_ItemHalfAt), 2, {kAll, kAll}, Answer::kGarbage, 0, 0, {}, FxItemHalf, nullptr, true},   // 2: unsigned char *(long x, long y)
    {FX_OURS(FieldPanel_DrawBox3), 2, {kAll, kAll}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // 2: void(int x, int y)
    {FX_OURS(FieldPanel_DrawBlink), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // 2: void(void)
    {FX_OURS(Area104_Kind5CRun), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // 1 (tail jump): void(void)
    {FX_OURS(Area121_Kind5CRun), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // 1 (tail jump): void(void)
    {FX_OURS(Gpu_SetSprt16), 1, {0}, Answer::kGarbage, 0, 0, {16}, nullptr, nullptr, true},   // 1: void(unsigned char *prim)
    {FX_OURS(Gpu_SetSprt8), 1, {0}, Answer::kGarbage, 0, 0, {16}, nullptr, nullptr, true},   // 1: void(unsigned char *prim)
    {FX_OURS(Gpu_SetCode6C), 1, {0}, Answer::kGarbage, 0, 0, {16}, nullptr, nullptr, true},   // 1: void(unsigned char *prim)
    {FX_OURS(Area49_EffectFrame), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // 1: void(void)
    {FX_OURS(Area117_MembersFrame), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // 1: void(void)
    {FX_OURS(Area118_MembersFrame), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // 1: void(void)
    {FX_OURS(Area169_MembersFrame), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // 1: void(void)
    {FX_OURS(Area171_MembersFrame), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // 1: void(void)
    {FX_OURS(Sprite_ObjectMatrix), 1, {0}, Answer::kGarbage, 0, 0, {}, FxObjectMatrix, nullptr, true},   // 1: short *(short *matrix)
    {FX_OURS(Camera_LoadMatrix), 1, {0}, Answer::kGarbage, 0, 0, {18}, nullptr, nullptr, true},   // 1: void(short *matrix)
    {FX_OURS(Gpu_SetLineF4), 1, {0}, Answer::kGarbage, 0, 0, {16}, nullptr, nullptr, true},   // 1: void(unsigned char *prim)
    {FX_OURS(Area85_ClutShift), 1, {kAll}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // 1: void(int delta)
    {FX_OURS(MsgBox_FrameTask), 0, {}, Answer::kFlag, 0, 0, {}, nullptr, nullptr, true},   // 1: unsigned char(void)
    {FX_OURS(Flags_Toggle), 2, {kAll, kU8}, Answer::kGarbage, 0, 0, {}, FxToggle, nullptr, true},   // 1: void(unsigned char *bits, unsigned index)
    {FX_OURS(Gte_SetGeomOffset), 2, {kAll, kAll}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // 1: void(long x, long y)
    {FX_OURS(Gte_LoadVertex), 1, {0}, Answer::kGarbage, 0, 0, {8}, nullptr, nullptr, true},   // 1: void(const unsigned long *vertex)
    {FX_OURS(Gte_Rtps), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // 1: void(void)
    {FX_OURS(Gte_StoreScreenXY), 1, {0}, Answer::kGarbage, 0, 0, {}, FxOut0_8, nullptr, true},   // 1: void(unsigned long *out)
    {FX_OURS(Sprite_FlashClut), 1, {kAll}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // 1: void(unsigned colour)
    {FX_OURS(Field_FloorHurt), 1, {kAll}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // 1: void(unsigned kind)
    {FX_OURS(DrawItemPool_Alloc), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // 1: unsigned short(void)
    {FX_OURS(Gfx_ClutAdjust), 5, {kAll, kAll, kAll, kAll, kAll}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // 1: long(int columns, int rows, int red, int green, int blue)
    {FX_OURS(Effect_ReleaseAt), 1, {kU8}, Answer::kGarbage, 0, 0, {}, FxReleaseAt, nullptr, true},   // 1: void(unsigned char index)
    // Capcom's, unnamed, read to the last instruction (EKH, 2026-09-29)
    {FX_RAW(0x5A7C70), 3, {0, 0, 0}, Answer::kGarbage, 0, 0, {18, 6}, FxOut2_6, nullptr, true},   // 12, library layer: a matrix (18 read), a vector (6 read) -> an out vector (6 written)
    {FX_OURS(EffectGlowTrail_Update), 1, {0}, Answer::kGarbage, 0, 0, {16}, nullptr, nullptr, true},   // 0x4794D0, 9: a record read to +0x440 and written to +0x402 (the first 16 hashed); calls 0x479970, the projection helpers
    {FX_OURS(EffectGlowTrail_Draw), 1, {0}, Answer::kGarbage, 0, 0, {16}, nullptr, nullptr, true},   // 0x4796B0, 8: a record read to +0x400; draws G4 quads
    {FX_RAW(0x5A7570), 1, {kAll}, Answer::kGarbage, 0, 0, {}, FxPrim0_44, nullptr, true},   // 7, library layer: 0x2C bytes of a primitive written (the packet pointer logged)
    {FX_RAW(0x5A7840), 2, {kAll, kAll}, Answer::kGarbage, 0, 0, {}, FxPrim0_12, nullptr, true},   // 6, library layer: 12 bytes of a primitive written
    {FX_OURS(EffectKind87_Midpoint), 3, {kAll, kAll, kAll}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // 6: three words, no calls
    {FX_OURS(EffectKind9C_DrawTrail), 3, {0, 0, kU8}, Answer::kGarbage, 0, 0, {12, 12}, nullptr, nullptr, true},   // 0x48ED80, 6: two points (12 read each) and a byte; draws
    {FX_OURS(EffectKind18Sub42_Draw), 2, {kAll, kU16}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // 6: (variant, height) - not a pointer: the variant a whole word indexing the piece lists (lea eax, [ebp + ebp*2] at 0x509A83) and the lift words, the height's low word alone reaching the vertex word it is subtracted into (sub edx, ebp; mov [eax - 6], dx at 0x509B62); E5F's reading, effect_5f.cpp (round thirteen's end fold)
    {FX_OURS(EffectKind21_ArmPoints), 1, {0}, Answer::kGarbage, 0, 0, {84}, nullptr, nullptr, true},   // 0x46F570, 5: a record read to +0x54; the GTE rotations
    {FX_OURS(EffectKind21_DrawArm), 1, {kAll}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // 0x46F690, 5: a word; a draw mode, 0x46F6F0
    {FX_OURS(LeaderPanel_Effect3Mode), 1, {kU8}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // 0x52B2A0, 5: a byte, no calls
    {FX_OURS(LeaderPanel_PoseSound), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // 0x52B1B0, 5: Sound_PlayEffect behind a test
    {FX_OURS(EffectKind0F_CharCount), 1, {0}, Answer::kGarbage, 0, 0, {kDerefString}, nullptr, nullptr, true},   // 0x5171E0, 4: a string's characters counted (a byte above 0x7F takes two), eax the count
    {FX_RAW(0x5A7A90), 1, {kAll}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // 4, library layer: a word through _ftol, eax read
    {FX_OURS(EffectGlowSparks_Run), 0, {}, Answer::kFlag, 0, 0, {}, nullptr, nullptr, true},   // 0x479260, 4: al; calls through 0x654660 by a byte
    {FX_OURS(EffectRing_Draw), 1, {0}, Answer::kGarbage, 0, 0, {18}, nullptr, nullptr, true},   // 0x479EE0, 3: a record read to +0x12; G4 quads
    {FX_OURS(EffectSpiral_StepDraw), 1, {0}, Answer::kGarbage, 0, 0, {16}, nullptr, nullptr, true},   // 0x479B70, 3: a record read to +0xD20 (the first 16 hashed)
    {FX_OURS(Menu_DrawPanelBox), 5, {kAll, kAll, kAll, kAll, kAll}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // 0x586160, 3: five words; Menu_DrawOutline, FT4 quads
    {"Gfx_StoreImage", bof3::addr::Gfx_StoreImage, bof3::addr::Gfx_StoreImage, 2, {0, 0}, Answer::kGarbage, 0, 0, {8}, FxOut1_4, nullptr, true},   // 2, renderer: 8 read at the first, 4 written at the second
    {FX_OURS(EffectGlowSparks_Clear), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // 0x4790C0, 2: no arguments, no calls
    {FX_OURS(EffectSpark_FindFree), 0, {}, Answer::kGarbage, 0, 0, {}, FxSparkFindFree, nullptr, true},   // 2: the first free record or null, as the real one (wave two's fold)
    {FX_OURS(EffectDust_Run), 0, {}, Answer::kFlag, 0, 0, {}, nullptr, nullptr, true},   // 0x47A200, 2: al; draws
    {FX_OURS(EffectSpiral_Init), 1, {0}, Answer::kGarbage, 0, 0, {16}, nullptr, nullptr, true},   // 0x4799C0, 2: a record read and written to +0xD20 (the first 16 hashed)
    {FX_OURS(EffectKind53_TexWindow), 4, {kU16, kU16, kU16, kU16}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // 2: four s16
    {FX_OURS(Area109_SwitchPattern), 0, {}, Answer::kByte, 1, 8, {}, FxPattern, nullptr, true},   // 0x4FEE70, 2: the three story flags 0x65DE60 as bits, plus 1 (lea eax, [edi + 1] at 0x4FEE9F): a whole eax 1..8, which EffectKind18_09 (0x4FEDE3) compares whole with +2 (round thirteen's end fold; E5A's reading)
    {FX_OURS(EffectKind07_DrawSprite), 1, {kAll}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // 0x462F10, 1: a word; a sprite primitive
    {FX_OURS(EffectKind1D_DrawSpeck), 1, {kAll}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // 0x46E190, 1: a word; tiles, Rand
    {FX_OURS(EffectKind24_DrawRay), 1, {0}, Answer::kGarbage, 0, 0, {13}, nullptr, nullptr, true},   // 0x46FAE0, 1: a record read to +0xD; lines
    {FX_OURS(EffectGte_SetDiagonalOne), 1, {0}, Answer::kGarbage, 0, 0, {}, FxOut0_18, nullptr, true},   // EGT (round thirteen): short*(short *matrix), 18 bytes written (docs/effect_gte.md section 7)
    {FX_OURS(Screen_TriangleWinding), 3, {0, 0, 0}, Answer::kGarbage, 0, 0, {8, 8, 8}, nullptr, nullptr, true},   // 0x4941B0, 1: three points of 8 read, nothing written (E2C's reading; EKH had them written); eax read (beside EGT's 0x494180, not EGT's)
    {FX_OURS(EffectGlowSparks_StartRise), 1, {0}, Answer::kGarbage, 0, 0, {}, FxOut0_24, nullptr, true},   // 0x4790F0, 1: 24 bytes written; Rand
    {FX_OURS(EffectDust_Clear), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // 0x47A110, 1: no arguments, no calls
    {FX_OURS(EffectDust_FindFree), 0, {}, Answer::kGarbage, 0, 0, {}, FxDustFindFree, nullptr, true},   // 0x47A130, 1: the first free dust record or null (wave two's fold)
    {FX_OURS(EffectDust_Start), 1, {0}, Answer::kGarbage, 0, 0, {8}, FxOut0_32, nullptr, true},   // 0x47A150, 1: 8 read then 32 written; AreaMap_Elevation, Rand
    {FX_OURS(EffectGlowSparks_StartBurst), 1, {0}, Answer::kGarbage, 0, 0, {}, FxOut0_24, nullptr, true},   // 0x479160, 1: 24 bytes written; Rand
    {FX_OURS(EffectKind52_MoveSparks), 0, {}, Answer::kFlag, 0, 0, {}, nullptr, nullptr, true},   // 1: al; calls through 0x65472C by a byte
    {FX_OURS(EffectKind69_DrawLines), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // 0x4837B0, 1: lines; Rand
    {FX_OURS(EffectKindA7_DrawGlow), 3, {kAll, kU16, kU8}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // 0x491E30, 1: the point, the size's low word (short sz[2] = {size, size} in rest_3f.cpp; mov ax, word [esp + 0x60] at 0x491E81) and the colour's byte; G3 (round fourteen's fold)
    {FX_OURS(EffectKindA9_DrawDisc), 3, {kU16, kU16, kU8}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // 0x492260, 1: two s16 and a byte; G3
    {FX_OURS(EffectKindA8_DrawBar), 1, {0}, Answer::kGarbage, 0, 0, {6}, nullptr, nullptr, true},   // 0x4920F0, 1: a record read to +6; G4
    {FX_OURS(EffectKind18Sub41_DrawPanels), 2, {kU16, kU16}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // 0x5100B0, 1: the lift's low word (imul ecx, ebx at 0x510123, the product stored as a word) and the texture's ((texture << 16) | 0xBB009120 in rest_3g.cpp); eax read (round fourteen's fold)
    {FX_OURS(EffectKind18Sub41_DrawRings), 1, {kAll}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // 0x5101C0, 1: a word; lines
    {FX_OURS(LeaderPanel_HoldTest), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // 0x52B2E0, 1: no arguments, no calls
    {FX_OURS(LeaderPanel_EffectsStep), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // 0x52B370, 1: calls 0x52B460
    {FX_OURS(LeaderPanel_UseItemEnd), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // 0x52B200, 1 (a tail jump; a hidden start): sound, animation
    {FX_OURS(LeaderPanel_LeaveOnPress), 1, {kU16}, Answer::kByte, 0, 1, {}, nullptr, nullptr, true},   // 0x52B330, 1: the buttons' low word (and eax, edx; test ax, ax at 0x52B33A); al 1 (mov al, 1 at 0x52B351) or 0 (xor al, al at 0x52B362), the rest of eax what it was; Transition_Start (R1G's reading; round fourteen's fold)
    {FX_OURS(Fish_RunAll), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // 0x52B6C0, 1: calls through 0x660324 by a byte
    {FX_RAW(0x593950), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // 1 (a tail jump): jmp [0x66A470 + byte 0x93985C * 4] - the dispatcher of EKP's run
};
#undef FX_OURS
#undef FX_RAW

constexpr std::uint32_t kImageLo = 0x401000, kImageHi = 0x5C3000;   // .text

void Register(const Callee& c) {
    if (c.key == c.address) {
        if (c.key < kImageLo || c.key >= kImageHi)
            bof3::Fatal("scenario_harness: callee %s at 0x%X is not Capcom's code", c.name, (unsigned)c.key);
    } else if (c.key >= kImageLo && c.key < kImageHi) {
        bof3::Fatal("scenario_harness: callee %s is Capcom's now (0x%X): list it as such", c.name, (unsigned)c.key);
    }
    if (c.nargs > kArgs) bof3::Fatal("scenario_harness: callee %s takes %u arguments, the stand-ins %u", c.name, c.nargs, kArgs);
    for (unsigned i = 0; i < g_slot_n; ++i)
        if (g_slots[i].address == c.address) return;   // listed twice: the first (the group's) stands
    if (g_slot_n == kSlots) bof3::Fatal("scenario_harness: more than %u stand-ins", kSlots);
    Slot& s = g_slots[g_slot_n++];
    s = {};
    s.name = c.name;
    s.address = c.address;
    s.key = c.key;
    s.nargs = c.nargs;
    std::memcpy(s.masks, c.masks, sizeof s.masks);
    s.answer = c.answer;
    s.lo = c.lo;
    s.hi = c.hi;
    std::memcpy(s.deref, c.deref, sizeof s.deref);
    s.effect = c.effect;
    s.custom = c.custom;
    s.guard = c.guard;
}
void RegisterHandler(std::uint32_t address) {
    for (unsigned i = 0; i < g_slot_n; ++i)
        if (g_slots[i].address == address) return;
    if (g_slot_n == kSlots) bof3::Fatal("scenario_harness: more than %u stand-ins", kSlots);
    Slot& s = g_slots[g_slot_n++];
    s = {};
    s.name = "handler";
    s.address = s.key = address;
    s.handler = true;
}
const void* StubOf(unsigned i) {
    const Slot& s = g_slots[i];
    if (s.custom) return s.custom;
    if (s.answer == Answer::kThrough) return reinterpret_cast<const void*>(static_cast<std::uintptr_t>(s.address));
    return reinterpret_cast<const void*>(kStubs.f[i]);
}
unsigned SlotFor(std::uint32_t address, const char* who) {
    for (unsigned i = 0; i < g_slot_n; ++i)
        if (g_slots[i].address == address) return i;
    bof3::Fatal("scenario_harness: %s calls 0x%X, which no stand-in covers: list it in the group's callees", who,
                (unsigned)address);
}
const void* StubFor(std::uint32_t address, const char* who) { return StubOf(SlotFor(address, who)); }

// --- the state both passes start from ------------------------------------------

constexpr unsigned kMaxRegions = 64;   // 48 before round twelve's field regions
Region g_regions[kMaxRegions];
unsigned g_region_n;
constexpr unsigned kMaxBytes = 0x10000;

struct State {
    unsigned char memory[kMaxBytes];
    Entry log[kLog];
    unsigned log_n;
};
unsigned g_bytes;

unsigned Used(unsigned log_n) { return log_n < kLog ? log_n : kLog; }

void Capture(State& s) {
    unsigned n = 0;
    for (unsigned i = 0; i < g_region_n; ++i) {
        std::memcpy(s.memory + n, Mem(g_regions[i].at), g_regions[i].size);
        n += g_regions[i].size;
    }
    s.log_n = g_log_n;
    std::memcpy(s.log, g_log, Used(g_log_n) * sizeof(Entry));
}
void Apply(const State& s) {
    unsigned n = 0;
    for (unsigned i = 0; i < g_region_n; ++i) {
        std::memcpy(Mem(g_regions[i].at), s.memory + n, g_regions[i].size);
        n += g_regions[i].size;
    }
    g_log_n = 0;
    g_salt = 0;
    g_rand_pending = g_rand_first;
}
bool Same(const State& a, const State& b) {
    return a.log_n == b.log_n && std::memcmp(a.memory, b.memory, g_bytes) == 0 &&
           std::memcmp(a.log, b.log, Used(a.log_n) * sizeof(Entry)) == 0;
}
unsigned FirstDifference(const State& a, const State& b) {
    for (unsigned i = 0; i < g_bytes; ++i)
        if (a.memory[i] != b.memory[i]) return i;
    return g_bytes;
}
unsigned FirstLogDifference(const State& a, const State& b) {
    const unsigned n = Used(a.log_n < b.log_n ? a.log_n : b.log_n);
    for (unsigned i = 0; i < n; ++i)
        if (std::memcmp(&a.log[i], &b.log[i], sizeof(Entry)) != 0) return i;
    return n;
}
void Where(unsigned byte, std::uint32_t& region, std::uint32_t& offset) {
    unsigned n = 0;
    for (unsigned i = 0; i < g_region_n; ++i) {
        if (byte < n + g_regions[i].size) {
            region = g_regions[i].at;
            offset = byte - n;
            return;
        }
        n += g_regions[i].size;
    }
    region = offset = 0;
}

// Random bytes put back inside what every chapter dereferences, and the
// group's chapter where Scenario_Start leaves it.
void Fix(int chapter) {
    Byte(at::kChapter)[0] = static_cast<unsigned char>(chapter);
    SetPointer(at::kFlagRow, Mem(at::kCondFlags + 8u * static_cast<std::uint32_t>(static_cast<int>(static_cast<signed char>(chapter)))));
    Sprite_Current = SpriteRecord(Next());
    SetPointer(at::kFieldState, ObjectOf(Next()));
    MapView_CornerPtr = g_own + (Next() & 0x7E);
}

// Field mode's put-backs, after Fix (docs/scenario_harness.md section 7.3):
// the packet cursor into the packet buffer, AreaMap_Bytes into the area block,
// the area header's width and height below 0x20 and its offset word below
// 0x100 (so a cell index from a bounded x, z stays near the 8 KiB compared),
// the script cursor into the script buffer, MapView_Row / MapView_Column
// inside the view's 0x38 x 0x1C.
void FixField() {
    SetPointer(at::kPacketNext, g_packets + (Next() & 0x1F0));
    SetPointer(at::kAreaBytes, Mem(at::kAreaBlock + 0x800));
    unsigned char* const header = Mem(at::kAreaBlock);
    header[0] &= 0x1F;
    header[1] &= 0x1F;
    header[3] = 0;
    g_cursor[0] = g_script + (Next() & 0x3F);
    // the view's row and column as the view keeps them (MapView_Cells is 0x38
    // rows of 0x1C; its readers wrap an index once, not twice)
    move_script::SetWord(Mem(0x929F24), Next() % 0x38);   // MapView_Row
    move_script::SetWord(Mem(0x929F20), Next() % 0x1C);   // MapView_Column
}
// The span draws of kSprite and kMenu, before the group's seed.
void DrawSpans(const Group& g, const Clone& c) {
    if (c.shape == Shape::kSprite && g.sprite_span) {
        unsigned char* const s = static_cast<unsigned char*>(Sprite_Current);
        for (unsigned i = 1; i <= 4; ++i) s[i] = static_cast<unsigned char>(Next() % g.sprite_span);
    }
    if (c.shape == Shape::kMenu && g.menu_span) {
        Byte(at::kMenuState)[0] = static_cast<unsigned char>(Next() % g.menu_span);
        Byte(at::kMenuStep)[0] = static_cast<unsigned char>(Next() % g.menu_span);
    }
}
bool FieldShape(Shape s) { return s >= Shape::kSprite; }

// The field runs (docs/takeover-queue-field-battle.md section 1) and the
// chapter bank (tools/scenario_rows.py's BANK_LO..BANK_HI).
struct Band { std::uint32_t lo, hi; };
constexpr Band kFieldRuns[] = {
    {0x461800, 0x461980}, {0x469D10, 0x46D5F0}, {0x5172C0, 0x519600}, {0x525390, 0x526DB0}, {0x52D080, 0x5372D9},
    {0x56D240, 0x5729F9}, {0x5738A0, 0x57CD8A}, {0x57FF80, 0x5859FA}, {0x58C7A0, 0x58C900}, {0x593960, 0x594061},
};
constexpr Band kChapterBank = {0x537F20, 0x56D5E0};
// Round thirteen's effect runs (docs/takeover-queue-round13.md section 10, the
// EKH brief): 0x470000..0x4A0000 is 0x470000..0x4941E0 and its callees, as the
// brief draws it (it reaches past the spell band's first entry 0x498FE0; the
// test only names a clone in the log, it refuses nothing).
constexpr Band kEffectRuns[] = {
    {0x462B00, 0x470000}, {0x470000, 0x4A0000}, {0x4FD2E0, 0x517000}, {0x528CD0, 0x52D080}, {0x594060, 0x594D8A},
};

// Effect mode's draws (round thirteen), after FixField and before the group's
// seed: every one of the 20 records' +5 one of the kinds (the clone's own, or
// one of the group's), +1 / +2 below the spans when set, a third of the other
// records free (+0 0) and the rest in use; Sprite_Current one of them, in use.
// Nothing is drawn outside effect mode.
void FixEffect(const Group& g, const Clone& c) {
    g_state_span = c.state_span ? c.state_span : g.state_span;
    g_sub_span = c.sub_span ? c.sub_span : g.sub_span;
    for (unsigned k = 0; k < at::kEffectCount; ++k) {
        unsigned char* const r = EffectRecord(k);
        if (c.kind >= 0) r[5] = static_cast<unsigned char>(c.kind);
        else if (g.kinds && g.n_kinds) r[5] = g.kinds[Next() % g.n_kinds];
        if (g_state_span) r[1] = static_cast<unsigned char>(Next() % g_state_span);
        if (g_sub_span) r[2] = static_cast<unsigned char>(Next() % g_sub_span);
        const std::uint32_t use = Next();
        r[0] = use % 3 == 0 ? 0 : static_cast<unsigned char>(1 + (use >> 8) % 0xFF);
    }
    unsigned char* const s = EffectRecord(Next());
    if (s[0] == 0) s[0] = 1;
    Sprite_Current = s;
}

void PatchImms(void* copy, const Clone& c) {
    auto* code = static_cast<std::uint8_t*>(copy);
    for (int i = 0; i < c.n_imms; ++i) {
        std::uint32_t had;
        std::memcpy(&had, code + c.imms[i].offset, sizeof had);
        if (had != c.imms[i].value)
            bof3::Fatal("scenario_harness: %s +0x%X holds 0x%X, not the handler 0x%X", c.name, (unsigned)c.imms[i].offset,
                        (unsigned)had, (unsigned)c.imms[i].value);
        const std::uint32_t to = Key(StubFor(had, c.name));
        std::memcpy(code + c.imms[i].offset, &to, sizeof to);
    }
    for (int i = 0; i < c.n_tables; ++i)
        move_script::Relocate(copy, c.base, c.size, {c.tables[i].jmp_disp, c.tables[i].table, c.tables[i].entries});
}

using FnArgs = std::uint32_t (__cdecl*)(std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t,
                                        std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t,
                                        std::uint32_t, std::uint32_t);

}  // namespace

// --- the public helpers -------------------------------------------------------

std::uint32_t Next() { g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5; return g_rng; }
bool Often() { return Next() % 3 != 0; }
bool Half() { return (Next() & 1) != 0; }
std::uint32_t Pick(const std::uint32_t* v, unsigned n) { return v[Next() % n]; }
unsigned char* Mem(std::uint32_t address) { return move_script::At(address); }
unsigned char* SpriteRecord(unsigned k) { return Mem(at::kSprites + (k % 4) * at::kSpriteStride); }
unsigned char* EffectRecord(unsigned k) { return Mem(at::kEffects + (k % at::kEffectCount) * at::kEffectStride); }
unsigned char* ObjectOf(unsigned k) { return Mem(at::kObjTrio + (k % 3) * at::kObjStride); }
unsigned char* FlagRow() { return Pointer(at::kFlagRow); }
unsigned char* TaskAt(unsigned k) { return SpriteRecord(k); }
unsigned char* EnemyOf(unsigned char target) {
    return Mem(at::kEnemies + static_cast<std::uint32_t>((static_cast<int>(target) - 3) * static_cast<int>(at::kEnemyStride)));
}
unsigned char* PartyOf(unsigned char actor) { return ObjectOf(actor); }
unsigned char* Script() { return g_cursor[0]; }
unsigned char** Cursor() { return &g_cursor[0]; }
unsigned char* Scratch(unsigned i) { return g_scratch + (i % kScratchSlots) * 0x40; }
unsigned char* Packets() { return g_packets; }
unsigned char* Text() { return g_text; }
bool InRegions(const void* p, unsigned n) {
    const auto at = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p));
    if (at + n < at) return false;
    for (unsigned i = 0; i < g_region_n; ++i)
        if (at >= g_regions[i].at && at + n <= g_regions[i].at + g_regions[i].size) return true;
    return false;
}
bool InFieldRuns(std::uint32_t address) {
    for (const Band& b : kFieldRuns)
        if (address >= b.lo && address < b.hi) return true;
    return false;
}
bool InChapterBank(std::uint32_t address) { return address >= kChapterBank.lo && address < kChapterBank.hi; }
bool InEffectRuns(std::uint32_t address) {
    for (const Band& b : kEffectRuns)
        if (address >= b.lo && address < b.hi) return true;
    return false;
}
void SetPointer(std::uint32_t cell, const void* p) { move_script::SetLong(Mem(cell), static_cast<std::int32_t>(Key(p))); }
unsigned char* Pointer(std::uint32_t cell) {
    return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(move_script::Long(Mem(cell)))));
}
void SetRandHint(std::uint32_t hint) { g_rand_hint = hint; }
void SetRandFirst(int first) { g_rand_first = first; }

void Record(std::uint32_t address, std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t d) {
    for (unsigned i = 0; i < g_slot_n; ++i)
        if (g_slots[i].address == address && !g_slots[i].handler) {
            Log5(i, a, b, c, d);
            return;
        }
    bof3::Fatal("scenario_harness: a custom stand-in records 0x%X, which no callee lists", (unsigned)address);
}
void Stir() { Disturb(); }
std::uint32_t Noise() {
    std::uint32_t h = Hash() ^ (++g_salt * 0x9E3779B9u);
    h ^= h >> 16;
    h *= 0x7FEB352Du;
    h ^= h >> 15;
    return h;
}
void Note(std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t d) { Log5(kNoteTag, a, b, c, d); }
void NoteBytes(const void* p, unsigned n) {
    std::uint32_t w[4] = {};
    if (n <= sizeof w) {
        std::memcpy(w, p, n);
    } else {
        w[0] = HashBytes(p, n);
        w[1] = n;
    }
    Log5(kNoteTag + 1, w[0], w[1], w[2], w[3]);
}
void FillBytes(void* p, unsigned n) {
    for (unsigned i = 0; i < n; ++i) static_cast<unsigned char*>(p)[i] = static_cast<unsigned char>(Noise() >> 7);
}
std::uint32_t HashBytes(const void* p, unsigned n) {
    std::uint32_t h = 0x811C9DC5u;
    for (unsigned i = 0; i < n; ++i) h = (h ^ static_cast<const unsigned char*>(p)[i]) * 0x01000193u;
    return h;
}

const void* ForOurs(unsigned i, std::uint32_t key) {
    return g_slots[i].answer == Answer::kThrough ? reinterpret_cast<const void*>(static_cast<std::uintptr_t>(key)) : StubOf(i);
}

const void* StandIn(std::uint32_t key) {
    for (unsigned i = 0; i < g_slot_n; ++i)
        if (g_slots[i].key == key) return ForOurs(i, key);
    std::uint32_t address = key;
    for (const Callee& c : kStandard)
        if (c.key == key) address = c.address;
    for (const Callee& c : kField)
        if (c.key == key) address = c.address;
    for (const Callee& c : kEffectOverrides)
        if (c.key == key) address = c.address;
    for (const Callee& c : kEffectStd)
        if (c.key == key) address = c.address;
    for (unsigned i = 0; i < g_slot_n; ++i)
        if (g_slots[i].address == address) return ForOurs(i, key);
    bof3::Fatal("scenario_harness: ours calls 0x%X, which no stand-in covers: list it in the group's callees", (unsigned)key);
}

void Run(const Group& group) {
    const unsigned per = group.rounds ? group.rounds : 2000;
    g_group = &group;
    g_field = group.field;
    g_effect = group.effect;
    for (unsigned k = 0; k < group.n_clones; ++k) {
        if (FieldShape(group.clones[k].shape)) g_field = true;
        if (group.clones[k].shape == Shape::kEffect) g_effect = true;
    }
    if (g_effect) g_field = true;
    g_slot_n = 0;
    for (unsigned i = 0; i < group.n_callees; ++i) Register(group.callees[i]);
    if (g_effect)
        for (const Callee& c : kEffectOverrides) Register(c);
    if (g_field)
        for (const Callee& c : kFieldOverrides) Register(c);
    for (const Callee& c : kStandard) Register(c);
    for (unsigned k = 0; k < group.n_clones; ++k)
        for (int i = 0; i < group.clones[k].n_imms; ++i) RegisterHandler(group.clones[k].imms[i].value);
    for (unsigned t = 0; t < group.n_data_tables; ++t)
        for (unsigned i = 0; i < group.data_tables[t].entries; ++i)
            RegisterHandler(static_cast<std::uint32_t>(move_script::Long(Mem(group.data_tables[t].at + 4 * i))));
    // round twelve's field-standard set, after the handlers (a handler stays one)
    for (const Callee& c : kField) Register(c);
    // round thirteen's effect-standard set, in effect mode only
    if (g_effect)
        for (const Callee& c : kEffectStd) Register(c);

    // the regions: the standard ones (docs/scenario_harness.md section 4), then the group's
    g_region_n = 0;
    const Region standard[] = {
        {at::kChapter, 0x14},                              // the chapter bytes 0x8034E0..0x8034F3
        {at::kCondFlags, at::kCondFlagsSize},              // Cond_Flags, the story flags 0x904030, the party lists 0x904061..
        {0x929EC0, 0x14},                                  // Field_MemberCount, Camera_Angles, Cond_AngleFB, the row pointer 0x929ED0
        {at::kWait, 4},                                    // MoveScript_WaitWordDA
        {at::kRequest, 4},                                 // Field_Request
        {at::kArea, 4},                                    // Game_AreaNumber, MoveScript_FAWord
        {0x903840, 0x20},                                  // Camera_Distance, the counters 0x903848..0x90384B, the effect slot 0x903850
        {0x9039A0, 8},                                     // Field_ScriptFlags
        {0x903A04, 0x10},                                  // the pending area change
        {0x937F88, 0x14},                                  // Sprite_Current, Gfx_ClutStripDirty, Frame_Counter, 0x937F98
        {0x7E0918, 4},                                     // Draw_PassFlags
        {0x7E0940, 0xA4},                                  // Sprite_Kind2
        {at::kObjTrio, 3 * at::kObjStride},                // ObjTrio: the party's field objects
        {at::kFieldState, 4},                              // Field_State
        {at::kSprites, at::kSpriteCount * at::kSpriteStride},   // Sprite_Objects
        {at::kSpritesExtra, 4 * at::kSpriteStride},        // Sprite_ObjectsExtra
        {at::kEffects, 20 * 0x80},                         // Effect_Objects
        {0x905E60, 0xC},                                   // Field_Kind2Z / X, MapView_Redraw
        {0x929F14, 0x14},                                  // MapView_FocusX / Z, Elevation, Column, Row
        {0x92A0C0, 4},                                     // MapView_CornerPtr
        {0x7E1BE8, 8},                                     // Input_Held, _Previous, _Pressed
        {Key(g_own), sizeof g_own},
    };
    for (const Region& r : standard) g_regions[g_region_n++] = r;
    if (g_field) {
        // field mode's standard regions (docs/scenario_harness.md section 7.3)
        const Region field[] = {
            {at::kPacketNext, 4},                          // Gfx_PacketNext, into the packet buffer
            {at::kCameraTurn, at::kCameraTurnSize},        // CameraTurn_Steps, Field_EdgeBits, Field_InputFlags, Field_ScriptFlags2, Field_InputHeld
            {at::kMenuBlock, at::kMenuBlockSize},          // MapView_BuildFlags, the menu block 0x929F00.., Field_Kind2Hold
            {at::kStyle, at::kStyleSize},                  // the window style byte 0x903A5A, the records 0x903A70.., Field_ActorStates
            {at::kTextBuffer, at::kTextBufferSize},        // the text scratch 0x904BA0
            {at::kSaveFlags, at::kSaveFlagsSize},          // the save block past Cond_Flags, to 0x904160
            {at::kSaveBytes, at::kSaveBytesSize},          // the save block's bytes 0x904560..0x904700
            {at::kAreaBlock, at::kAreaBlockSize},          // AreaMap_Header and the area block's first 8 KiB
            {at::kAreaBytes, 4},                           // AreaMap_Bytes, into the area block
            {Key(g_packets), sizeof g_packets},
            {Key(g_text), sizeof g_text},
            {Key(g_script), sizeof g_script},
            {Key(g_cursor), sizeof g_cursor},
            {Key(g_scratch), sizeof g_scratch},
        };
        for (const Region& r : field) g_regions[g_region_n++] = r;
        // round twelve's folds (2026-10-01; docs/scenario_harness.md section 8.6):
        // the confirm and cancel words the panels test, and MessagePools' first
        // 0x200 offset words (empty at start-up: a script-pool message drawn by
        // id cannot show otherwise, FE1). Effect mode holds both inside its own
        // regions (kMenuButtons, kMessagePools), so only the field runs add them.
        if (!g_effect) {
            g_regions[g_region_n++] = {0x90358C, 8};      // Field_ConfirmButtons, Field_CancelButtons
            g_regions[g_region_n++] = {0x803580, 0x400};  // MessagePools' offset words
        }
    }
    if (g_effect) {
        // effect mode's standard regions (docs/scenario_harness.md section 8.4):
        // the cells three or more of round thirteen's groups name
        const Region effect[] = {
            {at::kVertexScratch, at::kVertexScratchSize},  // Prim_VertexScratch 0x9037A0.. (15 groups write it)
            {at::kScreenXY, at::kScreenXYSize},            // Camera_ShiftX / Y, MapView_ScreenXY 0x903820 (9)
            {at::kCameraCells, at::kCameraCellsSize},      // Cond_ByteFE .., Camera_Matrix 0x905E40 (13)
            {at::kShards, at::kShardsSize},                // EffectKind30_Shards and the sparks (15)
            {at::kMenuButtons, at::kMenuButtonsSize},      // Field_MenuButton .. (6)
            {at::kMessagePools, at::kMessagePoolsSize},    // MessagePools (4)
            {at::kGameMode, 4},                            // Game_Mode, Game_Step (6)
            {at::kPanelCells, at::kPanelCellsSize},        // 0x939A00..0x939A2F (3)
            {at::kMessageCells, at::kMessageCellsSize},    // MsgBoxState 0x7DEE40.. inside the message cells (3)
        };
        for (const Region& r : effect) g_regions[g_region_n++] = r;
    }
    for (unsigned i = 0; i < group.n_regions; ++i) {
        if (g_region_n == kMaxRegions) bof3::Fatal("scenario_harness: %s: more than %u regions", group.shadow, kMaxRegions);
        g_regions[g_region_n++] = group.regions[i];
    }
    g_bytes = 0;
    for (unsigned i = 0; i < g_region_n; ++i) g_bytes += g_regions[i].size;
    if (g_bytes > kMaxBytes) bof3::Fatal("scenario_harness: %s: the regions are %u bytes, the state holds %u", group.shadow, g_bytes, kMaxBytes);

    static void* clones[256];
    if (group.n_clones > 256) bof3::Fatal("scenario_harness: %s: more than 256 clones", group.shadow);
    for (unsigned k = 0; k < group.n_clones; ++k) {
        const Clone& c = group.clones[k];
        bof3::CloneCall calls[64];
        if (c.n_calls > 64) bof3::Fatal("scenario_harness: %s has %d calls", c.name, c.n_calls);
        for (int i = 0; i < c.n_calls; ++i) {
            const unsigned slot = SlotFor(c.calls[i].target, c.name);
            calls[i] = {c.calls[i].offset, g_slots[slot].answer == Answer::kThrough ? nullptr : StubOf(slot), c.calls[i].target};
        }
        clones[k] = bof3::CloneOriginal(c.name, c.base, c.size, calls, c.n_calls);
        PatchImms(clones[k], c);
        if (g_field && !InFieldRuns(c.base) && !InChapterBank(c.base) && !InEffectRuns(c.base))
            bof3::Log("shadow      %s: %s at 0x%X lies outside the field runs, the chapter bank and the effect runs",
                      group.shadow, c.name, (unsigned)c.base);
    }

    // 128 entries a table since round thirteen (32 before): kind 0xF's state
    // table has 58, EffectKind18_States runs past 100
    static std::uint32_t kept[64][128];
    if (group.n_data_tables > 64) bof3::Fatal("scenario_harness: %s: more than 64 .data tables", group.shadow);
    for (unsigned t = 0; t < group.n_data_tables; ++t) {
        if (group.data_tables[t].entries > 128) bof3::Fatal("scenario_harness: %s: a .data table of more than 128", group.shadow);
        for (unsigned i = 0; i < group.data_tables[t].entries; ++i) {
            const std::uint32_t cell = group.data_tables[t].at + 4 * i;
            kept[t][i] = static_cast<std::uint32_t>(move_script::Long(Mem(cell)));
            move_script::SetLong(Mem(cell), static_cast<std::int32_t>(Key(StubFor(kept[t][i], group.shadow))));
        }
    }

    static State saved, input, theirs, ours;
    Capture(saved);
    for (unsigned i = 0; i < g_slot_n; ++i) g_slots[i].calls = 0;

    unsigned bad = 0, calls = 0;
    static unsigned bad_per[256];
    std::memset(bad_per, 0, sizeof bad_per);
    for (unsigned round = 0; round < per * group.n_clones; ++round) {
        const unsigned k = round % group.n_clones;
        const Clone& c = group.clones[k];
        for (unsigned i = 0; i < g_bytes; i += 4) {
            const std::uint32_t v = Next();
            std::memcpy(input.memory + i, &v, g_bytes - i < 4 ? g_bytes - i : 4);
        }
        input.log_n = 0;
        g_rand_first = -1;
        Apply(input);
        Fix(group.chapter);
        if (g_field) {
            FixField();
            DrawSpans(group, c);
        }
        if (g_effect) FixEffect(group, c);
        g_seed = Next();
        g_rand_hint = Next();
        if (group.seed) group.seed(k);
        Capture(input);

        // the arguments: random, then the shape's, then the group's
        std::uint32_t a[kArgs];
        for (unsigned i = 0; i < kScratchSlots; ++i) a[i] = Next();
        // arguments 10 and 11 (round thirteen's twelve) derived, not drawn: the
        // stream every group before them drew is unchanged
        a[10] = (a[0] ^ a[9]) * 0x9E3779B1u;
        a[11] = (a[1] ^ a[8]) * 0x85EBCA6Bu;
        if (c.shape == Shape::kHook) {
            a[0] = (Next() & 0x7F) << 16 | (a[0] & 0xFFFF);
            a[1] = (Next() & 0x7F) << 16 | (a[1] & 0xFFFF);
        } else if (c.shape == Shape::kObject) {
            a[0] = Key(SpriteRecord(Next()));
        } else if (c.shape == Shape::kScript) {
            a[0] = Key(g_cursor[0]);
        } else if (c.shape == Shape::kCursor) {
            a[0] = Key(&g_cursor[0]);
        }
        for (unsigned i = 0; i < kScratchSlots && c.pointers; ++i) {
            switch (static_cast<Arg>((c.pointers >> (3 * i)) & 7)) {
            case Arg::kSprite: a[i] = Key(SpriteRecord(Next())); break;
            case Arg::kScratch: a[i] = Key(Scratch(i)); break;
            case Arg::kScript: a[i] = Key(g_cursor[0]); break;
            case Arg::kEffect: a[i] = Key(EffectRecord(Next())); break;
            case Arg::kWord: break;
            default: bof3::Fatal("scenario_harness: %s: argument %u has no kind %u", c.name, i, (unsigned)((c.pointers >> (3 * i)) & 7));
            }
        }
        if (group.args) group.args(k, a);
        const std::uint32_t mask = c.ret_mask                                                ? c.ret_mask
                                   : c.shape == Shape::kHook || c.shape == Shape::kCursor ? 0xFFu
                                                                                          : 0u;
        g_calm = c.calm;
        for (int pass = 0; pass < 2; ++pass) {
            Apply(input);
            State& out = pass ? ours : theirs;
            const void* const fn = pass ? c.ours : clones[k];
            g_active = pass == 1;
            const std::uint32_t answer = reinterpret_cast<FnArgs>(const_cast<void*>(fn))(a[0], a[1], a[2], a[3], a[4], a[5], a[6],
                                                                                         a[7], a[8], a[9], a[10], a[11]);
            g_active = false;
            if (mask) Log5(kReturnTag, answer & mask, 0, 0, 0);
            Capture(out);
        }
        g_calm = false;
        for (unsigned i = 0; i < Used(theirs.log_n); ++i) {
            const std::uint32_t w = theirs.log[i].what;
            const unsigned s = w >= kPhaseTag ? w - kPhaseTag : w;
            if (w < kMoreTag && s < g_slot_n) {
                ++g_slots[s].calls;
                ++calls;
            }
        }
        if (theirs.log_n > kLog)
            bof3::Fatal("scenario_harness: %s made %u calls, the log holds %u", c.name, theirs.log_n, kLog);
        if (!Same(theirs, ours)) {
            ++bad_per[k];
            if (++bad <= 12) {
                std::uint32_t region, offset;
                const unsigned first = FirstDifference(theirs, ours);
                Where(first, region, offset);
                bof3::Log("shadow      %s self-test MISMATCH: round %u, %s, log %u / %u (first differing entry %u), first "
                          "differing byte %u (0x%X + 0x%X)",
                          group.shadow, round, c.name, theirs.log_n, ours.log_n, FirstLogDifference(theirs, ours), first,
                          (unsigned)region, (unsigned)offset);
            }
        }
    }
    Apply(saved);
    for (unsigned t = 0; t < group.n_data_tables; ++t)
        for (unsigned i = 0; i < group.data_tables[t].entries; ++i)
            move_script::SetLong(Mem(group.data_tables[t].at + 4 * i), static_cast<std::int32_t>(kept[t][i]));

    bof3::Log("shadow      %s self-test: %u rounds over %u functions (%u each), %u calls to the stand-ins, %u MISMATCHES; "
              "%u bytes of state (%u regions) and the stand-ins' log compared",
              group.shadow, per * group.n_clones, group.n_clones, per, calls, bad, g_bytes, g_region_n);
    char line[900];
    unsigned n = 0;
    bool more = false;
    for (unsigned i = 0; i < g_slot_n; ++i) {
        if (g_slots[i].calls == 0) continue;
        if (n + 64 >= sizeof line) {
            bof3::Log("shadow      %s coverage%s: %s,", group.shadow, more ? ", continued" : " (calls the originals made)", line);
            n = 0;
            more = true;
        }
        const int w = g_slots[i].handler
                          ? std::snprintf(line + n, sizeof line - n, "%sphase 0x%X %u", n ? ", " : "",
                                          (unsigned)g_slots[i].address, g_slots[i].calls)
                          : std::snprintf(line + n, sizeof line - n, "%s%s %u", n ? ", " : "", g_slots[i].name,
                                          g_slots[i].calls);
        if (w > 0) n += static_cast<unsigned>(w);
    }
    if (n || !more)
        bof3::Log("shadow      %s coverage%s: %s", group.shadow, more ? ", continued" : " (calls the originals made)", n ? line : "none");
    if (g_field)
        bof3::Log("shadow      %s field mode: %u stand-ins (%u of the field-standard set), the field regions and the packet, "
                  "text, script and scratch buffers compared",
                  group.shadow, g_slot_n, static_cast<unsigned>(sizeof kField / sizeof kField[0]));
    if (g_effect)
        bof3::Log("shadow      %s effect mode: Sprite_Current one of the %u effect records, %u effect-standard callees and %u "
                  "louder re-listings, the effect regions compared",
                  group.shadow, at::kEffectCount, static_cast<unsigned>(sizeof kEffectStd / sizeof kEffectStd[0]),
                  static_cast<unsigned>(sizeof kEffectOverrides / sizeof kEffectOverrides[0]));
    if (bad) {
        for (unsigned k = 0; k < group.n_clones; ++k)
            if (bad_per[k]) bof3::Log("shadow      %s: %s mismatched in %u rounds", group.shadow, group.clones[k].name, bad_per[k]);
        bof3::Fatal("%s differs from the original in %u self-test rounds", group.shadow, bad);
    }
    g_group = nullptr;
    g_field = false;
    g_effect = false;
    g_state_span = g_sub_span = 0;
}

}  // namespace scenario_harness

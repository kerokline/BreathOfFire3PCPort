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
alignas(16) unsigned char g_scratch[kArgs * 0x40];

const Group* g_group = nullptr;
bool g_field = false;        // field mode for the group being run

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
    case 4: Sprite_Current = SpriteRecord(v); break;
    case 5: Frame_Counter = h >> 6; break;
    case 6: Byte(at::kCounter)[0] = b; break;
    case 7: Byte(at::kRequest)[0] = b % 3 ? 2 : b; break;
    case 8: move_script::SetWord(Mem(at::kTimer), h >> 16); break;
    case 9: Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags ^ (1u << (b & 15))); break;
    // 10..13: field mode only (round twelve); without it these stay the
    // scenario round's no-ops, so its groups draw exactly what they drew.
    case 10:
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
constexpr unsigned kSlots = 512;   // 256 before round twelve's field-standard set
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
                           std::uint32_t a5, std::uint32_t a6, std::uint32_t a7, std::uint32_t a8, std::uint32_t a9) {
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
    const std::uint32_t a[kArgs] = {a0, a1, a2, a3, a4, a5, a6, a7, a8, a9};
    std::uint32_t r[kArgs] = {};
    for (unsigned i = 0; i < s.nargs && i < kArgs; ++i) {
        const void* const p = reinterpret_cast<const void*>(static_cast<std::uintptr_t>(a[i]));
        r[i] = s.deref[i] && (!s.guard || Readable(p, s.deref[i])) ? HashBytes(p, s.deref[i]) : a[i] & s.masks[i];
    }
    const unsigned entry = g_log_n;
    Log5(I, r[0], r[1], r[2], r[3]);
    if (s.nargs > 4) Log5(kMoreTag + I, r[4], r[5], r[6], r[7]);
    if (s.nargs > 8) Log5(kMoreTag + I, r[8], r[9], 0, 0);
    Disturb();
    std::uint32_t answer = Answering(s);
    if (s.effect) answer = s.effect(a, answer);
    if (s.answer != Answer::kGarbage && entry < kLog) g_log[entry].d ^= answer & 0xFF;   // the answer in the log
    return answer;
}

using StubFn = std::uint32_t (__cdecl*)(std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t,
                                        std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t);
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
    {SH_OURS(Text_DrawAt), 5, {kAll, kAll, kAll, kAll, kAll}, Answer::kGarbage, 0, 0},
    // the area, the party
    {SH_OURS(Field_ChangeArea), 4, {kU16, kAll, kAll, kU8}, Answer::kGarbage, 0, 0},
    {SH_OURS(Party_DropIn), 1, {kU8}, Answer::kGarbage, 0, 0},
    {SH_OURS(Party_Join), 1, {kU8}, Answer::kFlag, 0, 0},
    {SH_OURS(PartySet_Load), 4, {kAll, kAll, kAll, kAll}, Answer::kGarbage, 0, 0},
    {SH_OURS(PartySet_LoadFirst), 3, {kAll, kAll, kAll}, Answer::kGarbage, 0, 0},
    {SH_OURS(PartySet_LoadSecond), 4, {kAll, kAll, kAll, kAll}, Answer::kGarbage, 0, 0},
    {SH_OURS(Char_HealHp), 3, {kAll, kAll, kAll}, Answer::kFlag, 0, 0},
    {SH_OURS(Char_HealAp), 3, {kAll, kAll, kAll}, Answer::kFlag, 0, 0},
    {SH_OURS(Inventory_Add), 3, {kAll, kAll, kAll}, Answer::kFlag, 0, 0},
    {SH_OURS(Inventory_Count), 3, {kAll, kAll, kAll}, Answer::kGarbage, 0, 0},
    {SH_OURS(Item_NamePtr), 2, {kAll, kAll}, Answer::kGarbage, 0, 0},
    // the flags
    {SH_OURS(Flags_Test), 2, {kAll, kU8}, Answer::kBool, 0, 0},
    {SH_OURS(Flags_Set), 2, {kAll, kU8}, Answer::kGarbage, 0, 0},
    {SH_OURS(Flags_Clear), 2, {kAll, kU8}, Answer::kGarbage, 0, 0},
    {SH_OURS(ScriptFlags_Set40), 0, {}, Answer::kGarbage, 0, 0},
    {SH_OURS(ScriptFlags_Clear40), 0, {}, Answer::kGarbage, 0, 0},
    {SH_OURS(ObjTrio_SetBit40), 0, {}, Answer::kGarbage, 0, 0},
    {"ObjTrio_ClearBit40", 0x57C7E0, KeyOf(ObjTrio_ClearBit40), 0, {}, Answer::kGarbage, 0, 0},   // FO takes it (round twelve)
    // the scenario engine
    {SH_OURS(Scenario_CallA), 1, {kU8}, Answer::kGarbage, 0, 0},
    {"Scenario_CallB", 0x5341C0, KeyOf(Scenario_CallB), 1, {kU8}, Answer::kGarbage, 0, 0},   // FE2 takes it (round twelve)
    {SH_OURS(Transition_Start), 1, {kU8}, Answer::kGarbage, 0, 0},
    {SH_OURS(ClutStrip_FadeTo), 1, {kAll}, Answer::kGarbage, 0, 0},
    {SH_OURS(ClutStrip_Restore), 0, {}, Answer::kGarbage, 0, 0},
    {SH_OURS(Field_LoadingFrame), 0, {}, Answer::kGarbage, 0, 0},
    {SH_OURS(Port_DroppedCall), 1, {kU8}, Answer::kGarbage, 0, 0},
    // sprites, event objects, effects
    {SH_OURS(Sprite_EnsureAnimation), 1, {kAll}, Answer::kFlag, 0, 0},
    {SH_OURS(Sprite_SetAnimation), 1, {kU8}, Answer::kGarbage, 0, 0},
    {SH_OURS(Sprite_SetAnimationAt), 2, {kU8, kU16}, Answer::kGarbage, 0, 0},
    {SH_OURS(Sprite_SetAnimationBank), 1, {kU16}, Answer::kFlag, 0, 0},
    {SH_OURS(Sprite_FaceDirection), 1, {kU8}, Answer::kGarbage, 0, 0},
    {SH_OURS(EventObj_Face), 0, {}, Answer::kGarbage, 0, 0},
    {SH_OURS(EventObj_SetFlags), 1, {kAll}, Answer::kGarbage, 0, 0},
    {SH_OURS(EventObj_Reset), 0, {}, Answer::kGarbage, 0, 0},
    {"EventOp_6x", 0x57AD10, KeyOf(EventOp_6x), 1, {kAll}, Answer::kGarbage, 0, 0},   // FO takes it (round twelve)
    {SH_OURS(Field_ObjectInHome), 1, {kAll}, Answer::kFlag, 0, 0},
    {SH_OURS(Effect_FindFree), 0, {}, Answer::kByte, 0xFF, 0x13},
    {SH_THEIRS(Effect_SpawnAt), 6, {kU8, kU8, kU8, kAll, kAll, kAll}, Answer::kByte, 0xFF, 0x13},
    // the map and the camera
    {SH_OURS(MapView_SetElevation), 1, {kAll}, Answer::kGarbage, 0, 0},
    {SH_OURS(Kind2_Place), 1, {kU8}, Answer::kGarbage, 0, 0},
    {SH_OURS(AreaMap_Elevation), 2, {kAll, kAll}, Answer::kGarbage, 0, 0},
    {SH_OURS(AreaMap_SetupEntries), 0, {}, Answer::kGarbage, 0, 0},
    {SH_OURS(AreaMap_ByteAt), 2, {kU16, kU16}, Answer::kGarbage, 0, 0},
    {SH_OURS(AreaMap_SetByte), 3, {kAll, kAll, kAll}, Answer::kGarbage, 0, 0},
    {SH_OURS(MoveCmd_TestFB), 2, {kU16, kU16}, Answer::kFlag, 0, 0},
    {SH_OURS(Field_ViewReset), 0, {}, Answer::kGarbage, 0, 0},
    // sound and music
    {SH_OURS(Sound_PlayEffect), 1, {kU16}, Answer::kGarbage, 0, 0},
    {SH_OURS(Sound_PlayById), 1, {kU16}, Answer::kGarbage, 0, 0},
    {SH_OURS(Sound_LoadStream), 1, {kAll}, Answer::kGarbage, 0, 0},
    {SH_OURS(Sound_StreamDone), 0, {}, Answer::kBool, 0, 0},
    {SH_THEIRS(Sound_ResumeAll), 0, {}, Answer::kGarbage, 0, 0},
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
    {SH_THEIRS(Rand), 0, {}, Answer::kRand, 0, 0},
    // unnamed, by address (docs/scena_sc0.md section 6)
    // SE's (round ten): the event battle's set-up by index - 0x904AAA, 0x802D41 = 5
    {"0x4410B0", 0x4410B0, 0x4410B0, 1, {kU8}, Answer::kGarbage, 0, 0},
    // x, z (dwords to 0x903780 / 84) and an index: an event battle's party placement
    {"0x532ED0", 0x532ED0, 0x532ED0, 3, {kAll, kAll, kU8}, Answer::kGarbage, 0, 0},
    // the camera turned toward an angle (s16) at a speed (s8), al 1 while turning
    {"0x57C6B0", 0x57C6B0, 0x57C6B0, 2, {kU16, kU8}, Answer::kFlag, 0, 0},
    // the view shift after a focus test (PSX 0x80155154, docs/field-modes.md)
    {"0x56FCA0", 0x56FCA0, 0x56FCA0, 0, {}, Answer::kGarbage, 0, 0},
    // Field_StatusBits |= 0x80
    {"0x56D6F0", 0x56D6F0, 0x56D6F0, 0, {}, Answer::kGarbage, 0, 0},
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
    // Party_Zenny += amount (and the tally 0x904138 with the second argument's
    // byte), held at 9,999,999 with al 0 - as Zenny_Add (docs/scena_sx.md)
    unsigned char* const z = Mem(at::kZenny);
    if (!InRegions(z, 4)) return answer;
    std::uint32_t v = static_cast<std::uint32_t>(move_script::Long(z)) + a[0];
    if ((a[1] & 0xFF) && InRegions(Mem(0x904138), 4))
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

// Field mode's re-listing of a kStandard entry (registered before kStandard,
// so it stands, in field mode only): answers the field code dereferences.
const Callee kFieldOverrides[] = {
    // callers read the name through the answer (mov ecx, [eax])
    {FIELD_OURS(Item_NamePtr), 2, {kAll, kAll}, Answer::kGarbage, 0, 0, {}, FxText, nullptr, true},
};

const Callee kField[] = {
    {FIELD_OURS(Menu_DrawPiece), 4, {kAll, kAll, kAll, kAll}, Answer::kGarbage, 0, 0, {0, 0, 0, 0}, nullptr, nullptr, true},   // FO:31 FS:17: void(int x, int y, unsigned id, unsigned flags)
    {FIELD_OURS(Sprite_ScriptTick), 0, {}, Answer::kFlag, 0, 0, {}, nullptr, nullptr, true},   // FC1:12 FC2:4 FC3:10 FE1:2 FE2:5: unsigned char(void)
    {FIELD_THEIRS(Crt_sprintf, 0x5B9380), 4, {kAll, kAll, kAll, kAll}, Answer::kGarbage, 0, 0, {0, 16}, FxSprintf, nullptr, true},   // FE1:4 FE2:1 FO:12 FS:10: int(char *dst, const char *fmt, ...)
    {"0x52CFE0", 0x52CFE0, 0x52CFE0, 4, {kAll, kAll, kAll, kAll}, Answer::kGarbage, 0, 0, {}, FxPacket, nullptr, true},   // FE1:25: a sprite primitive at Gfx_PacketNext, committed; eax the primitive (callers write through it)
    {FIELD_OURS(Sprite_UpdateScreenSlot), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FC1:15 FC2:9: void(void)
    {FIELD_OURS(MapView_SlopeAt), 3, {kAll, kAll, kAll}, Answer::kGarbage, 0, 0, {0, 0, 0}, nullptr, nullptr, true},   // FC3:10 FE2:14: long(long x, long y, unsigned long direction)
    {FIELD_OURS(MapView_GroundAt), 2, {kAll, kAll}, Answer::kGarbage, 0, 0, {0, 0}, nullptr, nullptr, true},   // FC3:12 FE1:4 FE2:7: long(long x, long z)
    {FIELD_OURS(Text_DrawFont8), 4, {kAll, kAll, kAll, kAll}, Answer::kGarbage, 0, 0, {0, 0, 0, 16}, nullptr, nullptr, true},   // FE2:2 FO:10 FS:8: void(int x, int y, int colour, const unsigned char *text)
    {FIELD_OURS(Menu_DrawPieces), 4, {kAll, kAll, kAll, kAll}, Answer::kGarbage, 0, 0, {0, 0, 16, 0}, nullptr, nullptr, true},   // FO:12 FS:7: void(int x, int y, const unsigned char *list, int flags)
    {FIELD_OURS(Menu_DrawBox), 6, {kAll, kAll, kAll, kAll, kAll, kAll}, Answer::kGarbage, 0, 0, {0, 0, 0, 0, 0, 0}, nullptr, nullptr, true},   // FC1:1 FE1:2 FO:8 FS:7: void(int x, int y, int w, int h, int flags, int colour)
    {FIELD_OURS(Effect_Release), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FC1:4 FC2:14: void(void)
    {FIELD_OURS(Gfx_CommitPrim), 2, {kAll, kAll}, Answer::kGarbage, 0, 0, {0, 0}, FxCommitPrim, nullptr, true},   // FC1:1 FE1:1 FE2:8 FO:4: void(unsigned slot, unsigned size)
    {FIELD_OURS(Party_Count), 1, {kAll}, Answer::kGarbage, 0, 0, {0}, nullptr, nullptr, true},   // FS:14: int(unsigned slot)
    {FIELD_OURS(Sprite_ScriptTickOnce), 0, {}, Answer::kFlag, 0, 0, {}, nullptr, nullptr, true},   // FC1:3 FC2:2 FC3:3 FE1:1 FE2:4: unsigned char(void)
    {FIELD_OURS(Input_AutoRepeat), 1, {kAll}, Answer::kGarbage, 0, 0, {0}, nullptr, nullptr, true},   // FE2:3 FS:10: unsigned(unsigned pressed)
    {FIELD_OURS(Msg_SystemPtr), 1, {kAll}, Answer::kGarbage, 0, 0, {0}, FxText, nullptr, true},   // FE2:1 FO:2 FS:9: const unsigned char *(unsigned id)
    {FIELD_OURS(Field_LeaderStepTick), 0, {}, Answer::kFlag, 0, 0, {}, nullptr, nullptr, true},   // FC3:9 FE2:1: unsigned char(void)
    {FIELD_OURS(Sprite_SetTint), 5, {kAll, kU8, kU8, kU8, kU8}, Answer::kFlag, 0, 0, {16, 0, 0, 0, 0}, nullptr, nullptr, true},   // FC1:4 FC3:2 FO:3: unsigned char(unsigned char *sprite, unsigned char r, unsigned char g, unsigned char b, unsigned char a)
    {"0x52CF60", 0x52CF60, 0x52CF60, 2, {kAll, kAll}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FE1:9: a draw-mode primitive, committed
    {FIELD_OURS(Menu_DrawSkillRow), 7, {kAll, kAll, kAll, kAll, kAll, kAll, kAll}, Answer::kGarbage, 0, 0, {0, 0, 0, 0, 16, 0, 0}, nullptr, nullptr, true},   // FO:3 FS:6: void(int x, int y, int colour, unsigned kind, const unsigned char *name, unsigned cost, int dim)
    {FIELD_OURS(AreaMap_SetHeight), 3, {kAll, kAll, kAll}, Answer::kGarbage, 0, 0, {0, 0, 0}, nullptr, nullptr, true},   // FC2:8: void(unsigned x, unsigned z, unsigned value)
    {FIELD_OURS(Text_CharCount), 1, {kAll}, Answer::kFlag, 0, 0, {16}, nullptr, nullptr, true},   // FO:6 FS:2: unsigned char(const unsigned char *text)
    {FIELD_OURS(Sprite_ClearSteps), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FC3:4 FE1:1 FE2:2: void(void)
    {FIELD_OURS(Menu_DrawCursorBox), 6, {kAll, kAll, kAll, kAll, kAll, kAll}, Answer::kGarbage, 0, 0, {0, 0, 0, 0, 0, 0}, nullptr, nullptr, true},   // FS:7: void(int x, int y, int w, int h, int blink, int flags)
    {FIELD_OURS(Sprite_ApplyVelocity), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FC3:2 FE2:4: void(void)
    {FIELD_OURS(Math_Cos), 1, {kAll}, Answer::kGarbage, 0, 0, {0}, nullptr, nullptr, true},   // FE2:2 FO:4: int(int angle)
    {FIELD_OURS(Math_Sin), 1, {kAll}, Answer::kGarbage, 0, 0, {0}, nullptr, nullptr, true},   // FE2:2 FO:4: int(int angle)
    {FIELD_OURS(Menu_DrawHand), 3, {kAll, kAll, kAll}, Answer::kGarbage, 0, 0, {0, 0, 0}, nullptr, nullptr, true},   // FC1:1 FE2:3 FS:1: void(int x, int y, int unused)
    {FIELD_OURS(Sprite_ObjectAt), 3, {kAll, kAll, kAll}, Answer::kFlag, 0, 0, {0, 0, 0}, nullptr, nullptr, true},   // FC1:2 FC2:2 FE2:1: unsigned char(long x, long y, unsigned margin)
    {FIELD_OURS(Sprite_LoadPalette), 2, {kAll, kAll}, Answer::kGarbage, 0, 0, {8, 0}, nullptr, nullptr, true},   // FC1:1 FC3:1 FE2:3: void(unsigned short *dst, unsigned index)
    {FIELD_OURS(Gpu_SetSemiTrans), 2, {kAll, kAll}, Answer::kGarbage, 0, 0, {16, 0}, nullptr, nullptr, true},   // FC2:1 FE1:1 FE2:2 FO:1: void(unsigned char *prim, unsigned abe)
    {FIELD_OURS(Sprite_ShadeFadeBegin), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FC3:5: void(void)
    {FIELD_OURS(Field_JumpStart), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FC3:5: void(void)
    {FIELD_OURS(Gpu_SetDrawMode), 5, {kAll, kAll, kAll, kAll, kAll}, Answer::kGarbage, 0, 0, {16, 0, 0, 0, 0}, nullptr, nullptr, true},   // FE2:3 FO:2: void(unsigned char *prim, int dfe, int dtd, unsigned tpage, unsigned long tw)
    {FIELD_OURS(Menu_DrawIcon8), 4, {kAll, kAll, kAll, kAll}, Answer::kGarbage, 0, 0, {0, 0, 0, 0}, nullptr, nullptr, true},   // FO:5: void(int x, int y, int icon, int dim)
    {FIELD_OURS(KeyItem_Has), 1, {kAll}, Answer::kFlag, 0, 0, {0}, nullptr, nullptr, true},   // FO:1 FS:4: unsigned char(unsigned item)
    {"0x58BD50", 0x58BD50, 0x58BD50, 2, {kAll, kAll}, Answer::kGarbage, 0, 0, {}, FxSwap, nullptr, true},   // FS:5: swaps the bytes its two pointers name
    {FIELD_OURS(Sprite_UpdateScreen), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FC1:4: void(void)
    {FIELD_OURS(Sprite_FindFree), 0, {}, Answer::kFlag, 0, 0, {}, nullptr, nullptr, true},   // FC1:4: unsigned char(void)
    {FIELD_OURS(Sprite_UpdateScreenA), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FC1:1 FC2:3: void(void)
    {FIELD_OURS(Gpu_SetPolyFT4), 1, {kAll}, Answer::kGarbage, 0, 0, {16}, nullptr, nullptr, true},   // FC2:1 FE2:3: void(unsigned char *prim)
    {FIELD_OURS(MapView_LinkPrimAt), 4, {kAll, kAll, kAll, kAll}, Answer::kGarbage, 0, 0, {0, 0, 0, 0}, nullptr, nullptr, true},   // FC2:1 FE2:3: void(unsigned long x, unsigned long z, int dy, unsigned size)
    {FIELD_OURS(Tint_Release), 1, {kU8}, Answer::kGarbage, 0, 0, {0}, nullptr, nullptr, true},   // FC3:2 FO:2: void(unsigned char index)
    {"0x468950", 0x468950, 0x468950, 4, {kAll, kAll, kAll, kAll}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FE1:4: a textured quad, committed
    {"0x469750", 0x469750, 0x469750, 5, {kAll, kAll, kAll, kAll, kAll}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FE2:4: five words, calls 0x469790 / 0x469960
    {"0x594410", 0x594410, 0x594410, 1, {kAll}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FE2:4: a list window drawn (Text_DrawAt, Item_NamePtr)
    {FIELD_OURS(Field_MemberSprite), 2, {kAll, kAll}, Answer::kGarbage, 0, 0, {0, 0}, nullptr, nullptr, true},   // FO:1 FS:3: void(unsigned member, unsigned slot)
    {FIELD_OURS(Menu_DrawTitleBox), 5, {kAll, kAll, kAll, kAll, kAll}, Answer::kGarbage, 0, 0, {0, 0, 0, 0, 0}, nullptr, nullptr, true},   // FS:4: void(int x, int y, int w, int h, int colour)
    {FIELD_OURS(Menu_DrawBlackScreen), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FS:4: void(void)
    {FIELD_OURS(Text_DrawSmall), 5, {kAll, kAll, kAll, kAll, kAll}, Answer::kGarbage, 0, 0, {0, 0, 0, 0, 16}, FxArg4, nullptr, true},   // FC1:1 FO:1 FS:1: const unsigned char *(int x, int y, unsigned colour, unsigned count, const unsigned char *text)
    {FIELD_OURS(Gte_PushMatrix), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FC2:1 FE2:2: void(void)
    {FIELD_OURS(Gte_RotMatrix), 2, {kAll, kAll}, Answer::kGarbage, 0, 0, {8, 0}, FxRotMatrix, nullptr, true},   // FC2:1 FE2:2: short *(const short *angles, short *matrix)
    {FIELD_OURS(Gte_SetTransMatrix), 1, {kAll}, Answer::kGarbage, 0, 0, {12}, nullptr, nullptr, true},   // FC2:1 FE2:2: void(const unsigned long *matrix)
    {FIELD_OURS(Gte_SetRotMatrix), 1, {kAll}, Answer::kGarbage, 0, 0, {12}, nullptr, nullptr, true},   // FC2:1 FE2:2: void(const unsigned long *matrix)
    {FIELD_OURS(Gte_RotTrans), 2, {kAll, kAll}, Answer::kGarbage, 0, 0, {8, 0}, FxOut1_12, nullptr, true},   // FC2:1 FE2:2: void(const short *vector, long *out)
    {FIELD_OURS(Gte_PopMatrix), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FC2:1 FE2:2: void(void)
    {FIELD_OURS(Gpu_GetClut), 2, {kAll, kAll}, Answer::kGarbage, 0, 0, {0, 0}, nullptr, nullptr, true},   // FC2:1 FO:2: unsigned(int x, int y)
    {"0x46D5F0", 0x46D5F0, 0x46D5F0, 4, {kAll, kAll, kAll, kAll}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FC2:3: a number drawn (sprintf, sprites)
    {FIELD_OURS(Sprite_ShadeFadeStep), 1, {kAll}, Answer::kFlag, 0, 0, {0}, nullptr, nullptr, true},   // FC3:3: unsigned char(unsigned step)
    {FIELD_OURS(Field_CellAhead), 0, {}, Answer::kFlag, 0, 0, {}, nullptr, nullptr, true},   // FC3:3: unsigned char(void)
    {FIELD_OURS(Field_WayBlocked), 4, {kAll, kAll, kAll, kAll}, Answer::kFlag, 0, 0, {0, 0, 0, 0}, nullptr, nullptr, true},   // FC3:1 FE1:2: unsigned char(long x, long z, unsigned raised, long ground)
    {FIELD_OURS(Field_JumpCamera), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FE1:3: void(void)
    {FIELD_OURS(Area_TestCondition), 1, {kAll}, Answer::kFlag, 0, 0, {0}, nullptr, nullptr, true},   // FE2:3: unsigned char(unsigned long code)
    {FIELD_OURS(Gte_RotTransPers4), 9, {kAll, kAll, kAll, kAll, kAll, kAll, kAll, kAll, kAll}, Answer::kGarbage, 0, 0, {8, 8, 8, 8, 0, 0, 0, 0, 0}, FxRotTransPers4, nullptr, true},   // FE2:3: long(const short *v0, const short *v1, const short *v2, const short *v3, float *sxy0, float *sxy1, float *sxy2, float *sxy3, long *p)
    {FIELD_OURS(Gte_StoreDepthF4), 4, {kAll, kAll, kAll, kAll}, Answer::kGarbage, 0, 0, {0, 0, 0, 0}, FxStoreDepthF4, nullptr, true},   // FE2:3: void(float *out0, float *out1, float *out2, float *out3)
    {FIELD_OURS(TextRecord_Set), 3, {kAll, kAll, kAll}, Answer::kGarbage, 0, 0, {0, 0, 16}, nullptr, nullptr, true},   // FO:1 FS:2: void(unsigned slot, unsigned length, const unsigned char *text)
    {FIELD_OURS(Text_DrawFont12), 4, {kAll, kAll, kAll, kAll}, Answer::kGarbage, 0, 0, {0, 0, 0, 16}, nullptr, nullptr, true},   // FO:1 FS:2: void(int x, int y, int colour, const unsigned char *text)
    {FIELD_OURS(Skill_FlagIndex), 1, {kAll}, Answer::kFlag, 0, 0, {0}, nullptr, nullptr, true},   // FO:1 FS:2: unsigned char(unsigned id)
    {FIELD_OURS(Menu_DrawItemRow), 7, {kAll, kAll, kAll, kAll, kAll, kAll, kAll}, Answer::kGarbage, 0, 0, {0, 0, 0, 0, 0, 0, 0}, nullptr, nullptr, true},   // FO:3: void(int x, int y, int colour, unsigned category, unsigned id, unsigned count, int dim)
    {FIELD_OURS(Menu_DrawBackdrop), 1, {kAll}, Answer::kGarbage, 0, 0, {0}, nullptr, nullptr, true},   // FS:3: void(unsigned kind)
    {FIELD_OURS(Menu_DrawMemberStatus), 4, {kAll, kAll, kAll, kAll}, Answer::kGarbage, 0, 0, {0, 0, 0, 0}, nullptr, nullptr, true},   // FS:3: unsigned long(int x, int y, unsigned member, unsigned highlight)
    {FIELD_OURS(Sprite_QueueOverlay), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FC1:2: void(void)
    {FIELD_OURS(EventOp_0x), 1, {kAll}, Answer::kGarbage, 0, 0, {16}, nullptr, nullptr, true},   // FC1:2: void(const unsigned char *op)
    {FIELD_OURS(AreaMap_Slope), 3, {kAll, kAll, kAll}, Answer::kGarbage, 0, 0, {0, 0, 0}, nullptr, nullptr, true},   // FC1:2: long(long x, long y, unsigned long direction)
    {FIELD_OURS(Sprite_InitFromEntry), 1, {kAll}, Answer::kGarbage, 0, 0, {16}, nullptr, nullptr, true},   // FC1:1 FO:1: void(unsigned char *entry)
    {FIELD_OURS(Field_MembersFrame), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FC3:1 FE1:1: void(void)
    {FIELD_OURS(Field_RunTaskRecords), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FC3:2: void(void)
    {FIELD_OURS(Field_ObjectRandomTurn), 1, {kAll}, Answer::kFlag, 0, 0, {16}, nullptr, nullptr, true},   // FC3:2: unsigned char(unsigned char *object)
    {FIELD_OURS(Field_ObjectOpenDirection), 1, {kAll}, Answer::kFlag, 0, 0, {16}, nullptr, nullptr, true},   // FC3:2: unsigned char(unsigned char *object)
    {FIELD_OURS(Field_MemberTimers), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FC3:1 FE1:1: void(void)
    {FIELD_OURS(AreaMap_CellsNone), 5, {kAll, kAll, kAll, kAll, kAll}, Answer::kFlag, 0, 0, {0, 0, 0, 0, 0}, nullptr, nullptr, true},   // FC3:2: unsigned char(long x, long z, unsigned wide, unsigned code, unsigned mask)
    {FIELD_OURS(Field_JumpSetUp), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FE2:2: void(void)
    {"0x5B9550", 0x5B9550, 0x5B9550, 0, {}, Answer::kThrough, 0, 0, {}, nullptr, nullptr, true},   // FE2:2: the CRT's _ftol: pops st(0), answers edx:eax - called for real on both sides
    {FIELD_OURS(Gte_MulMatrix0), 3, {kAll, kAll, kAll}, Answer::kGarbage, 0, 0, {8, 8, 0}, FxMulMatrix0, nullptr, true},   // FE2:2: short *(const short *a, const short *b, short *out)
    {FIELD_OURS(Gpu_SetPolyG3), 1, {kAll}, Answer::kGarbage, 0, 0, {16}, nullptr, nullptr, true},   // FE2:1 FO:1: void(unsigned char *prim)
    {FIELD_OURS(Gpu_SetShadeTex), 2, {kAll, kAll}, Answer::kGarbage, 0, 0, {16, 0}, nullptr, nullptr, true},   // FE2:2: void(unsigned char *prim, unsigned tge)
    {FIELD_OURS(Prim_SetTexture), 3, {kAll, kAll, kAll}, Answer::kGarbage, 0, 0, {0, 16, 0}, nullptr, nullptr, true},   // FE2:2: void(unsigned long texture, unsigned char *prim, int count)
    {"0x5947D0", 0x5947D0, 0x5947D0, 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FE2:2: a window drawn
    {"0x5942C0", 0x5942C0, 0x5942C0, 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FE2:2: a window frame drawn
    {"0x594700", 0x594700, 0x594700, 2, {kAll, kAll}, Answer::kFlag, 0, 0, {}, nullptr, nullptr, true},   // FE2:2: al: an inventory test (Inventory_Count)
    {FIELD_OURS(Item_HelpMessage), 2, {kAll, kAll}, Answer::kGarbage, 0, 0, {0, 0}, nullptr, nullptr, true},   // FE2:1 FS:1: unsigned(unsigned category, unsigned item)
    {"0x594AD0", 0x594AD0, 0x594AD0, 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FE2:2: a window drawn
    {FIELD_OURS(Menu_DrawCell8), 6, {kAll, kAll, kAll, kAll, kAll, kAll}, Answer::kGarbage, 0, 0, {0, 0, 0, 0, 0, 0}, nullptr, nullptr, true},   // FO:2: unsigned long(unsigned x, unsigned y, unsigned u, unsigned v, unsigned clut, unsigned shade)
    {FIELD_OURS(Menu_ListScroll), 4, {kAll, kAll, kAll, kAll}, Answer::kFlag, 0, 0, {16, 16, 16, 16}, nullptr, nullptr, true},   // FO:1 FS:1: unsigned char(unsigned char *top, unsigned char *offset, unsigned char *moving, unsigned char *state)
    {FIELD_OURS(Menu_DrawScrollBar), 7, {kAll, kAll, kAll, kAll, kAll, kAll, kAll}, Answer::kGarbage, 0, 0, {16, 0, 0, 0, 0, 0, 0}, nullptr, nullptr, true},   // FO:1 FS:1: void(const unsigned char *items, unsigned top, int x, int y, unsigned rows, unsigned total, unsigned height)
    {FIELD_OURS(Party_ApplyRecord), 1, {kAll}, Answer::kGarbage, 0, 0, {16}, nullptr, nullptr, true},   // FO:2: void(unsigned char *context)
    {FIELD_OURS(Menu_YesNo), 0, {}, Answer::kFlag, 0, 0, {}, nullptr, nullptr, true},   // FS:2: unsigned char(void)
    {FIELD_OURS(Field_PartyLoad), 1, {kAll}, Answer::kGarbage, 0, 0, {0}, nullptr, nullptr, true},   // FS:2: void(unsigned slot)
    {FIELD_OURS(Char_RecalcStats), 1, {kAll}, Answer::kGarbage, 0, 0, {16}, nullptr, nullptr, true},   // FS:2: void(unsigned char *record)
    {FIELD_OURS(Gpu_SetLineF2), 1, {kAll}, Answer::kGarbage, 0, 0, {16}, nullptr, nullptr, true},   // FC1:1: void(unsigned char *prim)
    {FIELD_OURS(CameraTurn_Start), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FC1:1: void(void)
    {FIELD_OURS(CameraTurn_Step), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FC1:1: void(void)
    {FIELD_OURS(CameraTurn_End), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FC1:1: void(void)
    {FIELD_THEIRS(MoveCmd_Move, 0x578C10), 2, {kAll, kU8}, Answer::kGarbage, 0, 0, {16, 0}, nullptr, nullptr, true},   // FC1:1: void(unsigned char *object, unsigned char direction)
    {FIELD_OURS(Party_MemberAt), 3, {kAll, kAll, kAll}, Answer::kFlag, 0, 0, {0, 0, 0}, nullptr, nullptr, true},   // FC1:1: unsigned char(long x, long y, unsigned margin)
    {FIELD_OURS(Gte_VectorNormalS), 2, {kAll, kAll}, Answer::kGarbage, 0, 0, {12, 0}, FxOut1_6, nullptr, true},   // FC2:1: long(const long *in, short *out)
    {FIELD_OURS(Gte_VectorNormal), 2, {kAll, kAll}, Answer::kGarbage, 0, 0, {12, 0}, FxOut1_12, nullptr, true},   // FC2:1: long(const long *in, long *out)
    {"0x494060", 0x494060, 0x494060, 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FC2:1: the camera matrices from Camera_Angles and the focus
    {"0x494110", 0x494110, 0x494110, 2, {kAll, kAll}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FC2:1: a point projected (Gte_RotTransPers)
    {"0x4941E0", 0x4941E0, 0x4941E0, 3, {kAll, kAll, kAll}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FC2:1: a vector turned (Gte_RotTrans)
    {FIELD_OURS(Gpu_GetTPage), 4, {kAll, kAll, kAll, kAll}, Answer::kGarbage, 0, 0, {0, 0, 0, 0}, nullptr, nullptr, true},   // FC2:1: unsigned(unsigned tp, unsigned abr, int x, int y)
    {FIELD_OURS(AreaMap_Frame), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FC3:1: void()
    {FIELD_OURS(Party_ExtraScreens), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FC3:1: void(void)
    {FIELD_OURS(Party_UpdateScreens), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FC3:1: void(void)
    {"0x5372E0", 0x5372E0, 0x5372E0, 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FC3:1: the party screens updated
    {FIELD_OURS(Effect_RunObjects), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FC3:1: void(void)
    {FIELD_OURS(MoveScript_TintFrame), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FC3:1: void(void)
    {FIELD_OURS(Field_DrawFrame), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FC3:1: void(void)
    {"0x42D710", 0x42D710, 0x42D710, 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FC3:1: jmp through 0x64ADAC by the menu byte 0x929F00
    {"0x57DFF0", 0x57DFF0, 0x57DFF0, 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FC3:1: jmp through 0x663DD0 by the menu byte 0x929F00
    {FIELD_OURS(Field_ObjectBlockedAhead), 1, {kAll}, Answer::kFlag, 0, 0, {16}, nullptr, nullptr, true},   // FC3:1: unsigned char(unsigned char *object)
    {FIELD_OURS(MoveScript_Step), 2, {kAll, kAll}, Answer::kFlag, 0, 0, {16, 16}, nullptr, nullptr, true},   // FC3:1: unsigned char(unsigned char *object, const unsigned char *script)
    {FIELD_OURS(Sprite_ShadeLower), 1, {kAll}, Answer::kFlag, 0, 0, {0}, nullptr, nullptr, true},   // FC3:1: unsigned char(unsigned step)
    {FIELD_OURS(MoveCmd_AttachOffset), 2, {kAll, kU8}, Answer::kGarbage, 0, 0, {0, 0}, FxOut0_12, nullptr, true},   // FC3:1: void(long *out, unsigned char index)
    {FIELD_OURS(Field_TileD0), 0, {}, Answer::kFlag, 0, 0, {}, nullptr, nullptr, true},   // FC3:1: unsigned char(void)
    {FIELD_OURS(Area_LinkAt), 2, {kAll, kAll}, Answer::kFlag, 0, 0, {0, 0}, nullptr, nullptr, true},   // FC3:1: unsigned char(unsigned x, unsigned z)
    {FIELD_OURS(Field_LeaderPushObjects), 0, {}, Answer::kFlag, 0, 0, {}, nullptr, nullptr, true},   // FC3:1: unsigned char(void)
    {"0x52CE60", 0x52CE60, 0x52CE60, 2, {kU8, kU16}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FE1:1: a word from the 36-byte records at 0x66A6AC scaled by the second argument
    {"0x52CED0", 0x52CED0, 0x52CED0, 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FE1:1: the sum of 0x52CE60 over the 32 bytes at 0x9040EC
    {FIELD_OURS(Gpu_SetTile), 1, {kAll}, Answer::kGarbage, 0, 0, {16}, nullptr, nullptr, true},   // FE1:1: void(unsigned char *prim)
    {FIELD_OURS(Member_ClearState), 1, {kAll}, Answer::kGarbage, 0, 0, {0}, nullptr, nullptr, true},   // FE1:1: void(unsigned member)
    {FIELD_OURS(Field_LeaderStand), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FE1:1: void(void)
    {FIELD_OURS(Area104_LeaderRun), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FE1:1: void(void)
    {FIELD_OURS(Area121_LeaderRun), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FE1:1: void(void)
    {FIELD_OURS(Zenny_Add), 2, {kAll, kAll}, Answer::kFlag, 0, 0, {0, 0}, FxZennyAdd, nullptr, true},   // FE1:1: unsigned char(unsigned amount, unsigned tally)
    {FIELD_OURS(Field_CellHasEvent), 2, {kAll, kAll}, Answer::kFlag, 0, 0, {0, 0}, nullptr, nullptr, true},   // FE1:1: unsigned char(long x, long z)
    {FIELD_OURS(Char_LoseHp), 2, {kAll, kAll}, Answer::kGarbage, 0, 0, {0, 0}, nullptr, nullptr, true},   // FE2:1: unsigned(unsigned amount, unsigned member)
    {"0x537500", 0x537500, 0x537500, 2, {kAll, kAll}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FE2:1: two words, no calls
    {FIELD_OURS(Actor_EquipCount), 3, {kAll, kAll, kAll}, Answer::kFlag, 0, 0, {0, 0, 0}, nullptr, nullptr, true},   // FE2:1: unsigned char(unsigned member, unsigned kind, unsigned value)
    {FIELD_OURS(Field_CellsBlock), 3, {kAll, kAll, kAll}, Answer::kFlag, 0, 0, {0, 0, 0}, nullptr, nullptr, true},   // FE2:1: unsigned char(long x, long z, unsigned wide)
    {FIELD_OURS(Gte_RotTransPers), 3, {kAll, kAll, kAll}, Answer::kGarbage, 0, 0, {8, 0, 0}, FxRotTransPers, nullptr, true},   // FE2:1: long(const short *vertex, unsigned long *sxy, long *p)
    {FIELD_OURS(Gte_PrimDepthFlat4_10), 1, {kAll}, Answer::kGarbage, 0, 0, {16}, nullptr, nullptr, true},   // FE2:1: void(void *prim)
    {FIELD_OURS(Gte_RotTransPers3), 7, {kAll, kAll, kAll, kAll, kAll, kAll, kAll}, Answer::kGarbage, 0, 0, {8, 8, 8, 0, 0, 0, 0}, FxRotTransPers3, nullptr, true},   // FE2:1: long(const short *v0, const short *v1, const short *v2, float *sxy0, float *sxy1, float *sxy2, long *p)
    {FIELD_OURS(Gte_PrimDepths3_10B), 1, {kAll}, Answer::kGarbage, 0, 0, {16}, nullptr, nullptr, true},   // FE2:1: void(void *prim)
    {FIELD_OURS(Scena17_DrawLogo), 2, {kAll, kAll}, Answer::kGarbage, 0, 0, {0, 0}, nullptr, nullptr, true},   // FE2:1: void(int x, int y)
    {FIELD_OURS(Area_CellHook), 2, {kAll, kAll}, Answer::kGarbage, 0, 0, {0, 0}, nullptr, nullptr, true},   // FE2:1: int(unsigned x, unsigned z)
    {FIELD_OURS(Snd_LoadBankFile), 1, {kAll}, Answer::kGarbage, 0, 0, {0}, nullptr, nullptr, true},   // FE2:1: void(unsigned index)
    {"0x586670", 0x586670, 0x586670, 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FE2:1: jmp through 0x66450C by the byte 0x9398CF
    {FIELD_OURS(Gfx_ClutStripCopyRow), 1, {kAll}, Answer::kGarbage, 0, 0, {0}, nullptr, nullptr, true},   // FE2:1: void(unsigned row)
    {"0x585A00", 0x585A00, 0x585A00, 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FE2:1: no arguments, no calls
    {FIELD_OURS(Gpu_SetPolyG4), 1, {kAll}, Answer::kGarbage, 0, 0, {16}, FxArg0, nullptr, true},   // FE2:1: unsigned char *(unsigned char *prim)
    {"0x594790", 0x594790, 0x594790, 0, {}, Answer::kFlag, 0, 0, {}, nullptr, nullptr, true},   // FE2:1: al (the caller stores it at 0x6BE08D)
    {"0x594D90", 0x594D90, 0x594D90, 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FE2:1: Inventory_Remove behind a test
    {FIELD_OURS(Char_ExpForLevel), 2, {kAll, kAll}, Answer::kGarbage, 0, 0, {0, 0}, nullptr, nullptr, true},   // FO:1: int(unsigned member, unsigned level)
    {FIELD_OURS(Gpu_SetSprt), 1, {kAll}, Answer::kGarbage, 0, 0, {16}, nullptr, nullptr, true},   // FO:1: void(unsigned char *prim)
    {FIELD_OURS(Equip_PreviewSet), 4, {kAll, kAll, kAll, kAll}, Answer::kGarbage, 0, 0, {0, 16, 16, 8}, nullptr, nullptr, true},   // FO:1: void(unsigned id, const unsigned char *set, unsigned char *marks, unsigned short *values)
    {FIELD_OURS(Char_AbilityList), 3, {kAll, kAll, kAll}, Answer::kGarbage, 0, 0, {0, 0, 0}, FxText, nullptr, true},   // FO:1: unsigned char *(unsigned member, unsigned type, unsigned battle)
    {FIELD_OURS(Skill_CanUse), 3, {kAll, kAll, kAll}, Answer::kFlag, 0, 0, {0, 0, 0}, nullptr, nullptr, true},   // FO:1: unsigned char(unsigned mode, unsigned member, unsigned id)
    {FIELD_OURS(Skill_ApCost), 3, {kAll, kAll, kAll}, Answer::kFlag, 0, 0, {0, 0, 0}, nullptr, nullptr, true},   // FO:1: unsigned char(unsigned member, unsigned id, unsigned battle)
    {FIELD_OURS(Item_CanUse), 4, {kAll, kAll, kAll, kAll}, Answer::kFlag, 0, 0, {0, 0, 0, 0}, nullptr, nullptr, true},   // FO:1: unsigned char(unsigned mode, unsigned member, unsigned category, unsigned item)
    {FIELD_OURS(Item_IconKind), 2, {kAll, kAll}, Answer::kGarbage, 0, 0, {0, 0}, nullptr, nullptr, true},   // FO:1: unsigned(unsigned category, unsigned item)
    {FIELD_OURS(MoveCmd_TestFC), 2, {kU16, kU16}, Answer::kFlag, 0, 0, {0, 0}, nullptr, nullptr, true},   // FO:1: unsigned char(short x, short z)
    {FIELD_THEIRS(Effect_Spawn, 0x57CE10), 5, {kU8, kU8, kU8, kU16, kU16}, Answer::kFlag, 0, 0, {0, 0, 0, 0, 0}, nullptr, nullptr, true},   // FO:1: unsigned char(unsigned char kind, signed char a, signed char b, short x, short z)
    {FIELD_OURS(Party_MoveMember), 2, {kAll, kU8}, Answer::kGarbage, 0, 0, {16, 0}, nullptr, nullptr, true},   // FO:1: void(unsigned char *object, unsigned char direction)
    {FIELD_OURS(PartyRecord_Clear), 1, {kAll}, Answer::kGarbage, 0, 0, {0}, nullptr, nullptr, true},   // FO:1: void(unsigned index)
    {FIELD_OURS(MoveScript_ObjectKind), 0, {}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FO:1: int(void)
    {FIELD_OURS(SaveMenu_DrawSlots), 3, {kAll, kAll, kAll}, Answer::kGarbage, 0, 0, {0, 0, 0}, nullptr, nullptr, true},   // FS:1: void(int x, int y, unsigned highlight)
    {FIELD_OURS(Area_RunPlacement), 1, {kAll}, Answer::kGarbage, 0, 0, {16}, nullptr, nullptr, true},   // FS:1: void(const unsigned char *script)
    {FIELD_OURS(Menu_DrawItemIcon), 4, {kAll, kAll, kAll, kAll}, Answer::kGarbage, 0, 0, {0, 0, 0, 0}, nullptr, nullptr, true},   // FS:1: void(int x, int y, int icon, int shade)
    {FIELD_OURS(Menu_DrawBorder), 4, {kAll, kAll, kAll, kAll}, Answer::kGarbage, 0, 0, {0, 0, 0, 0}, nullptr, nullptr, true},   // FS:1: void(int x, int y, int w, int h)
    {FIELD_OURS(Menu_DrawMoneyBox), 4, {kAll, kAll, kAll, kAll}, Answer::kGarbage, 0, 0, {0, 0, 0, 0}, nullptr, nullptr, true},   // FS:1: void(int x, int y, int unused, unsigned value)
    {FIELD_OURS(Inventory_Remove), 3, {kAll, kAll, kAll}, Answer::kFlag, 0, 0, {0, 0, 0}, nullptr, nullptr, true},   // FS:1: unsigned char(unsigned category, unsigned item, unsigned count)
    {FIELD_OURS(AbilityList_Add), 4, {kAll, kAll, kAll, kAll}, Answer::kFlag, 0, 0, {0, 0, 0, 0}, nullptr, nullptr, true},   // FS:1: unsigned char(unsigned id, unsigned member, unsigned shared, unsigned which)
    {"0x591AC0", 0x591AC0, 0x591AC0, 3, {kU8, kU8, kU8}, Answer::kGarbage, 0, 0, {}, nullptr, nullptr, true},   // FS:1: a record's list pointer by member and page (the caller keeps a byte)
};
#undef FIELD_OURS
#undef FIELD_THEIRS

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
// the script cursor into the script buffer.
void FixField() {
    SetPointer(at::kPacketNext, g_packets + (Next() & 0x1F0));
    SetPointer(at::kAreaBytes, Mem(at::kAreaBlock + 0x800));
    unsigned char* const header = Mem(at::kAreaBlock);
    header[0] &= 0x1F;
    header[1] &= 0x1F;
    header[3] = 0;
    g_cursor[0] = g_script + (Next() & 0x3F);
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
                                        std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t);

}  // namespace

// --- the public helpers -------------------------------------------------------

std::uint32_t Next() { g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5; return g_rng; }
bool Often() { return Next() % 3 != 0; }
bool Half() { return (Next() & 1) != 0; }
std::uint32_t Pick(const std::uint32_t* v, unsigned n) { return v[Next() % n]; }
unsigned char* Mem(std::uint32_t address) { return move_script::At(address); }
unsigned char* SpriteRecord(unsigned k) { return Mem(at::kSprites + (k % 4) * at::kSpriteStride); }
unsigned char* ObjectOf(unsigned k) { return Mem(at::kObjTrio + (k % 3) * at::kObjStride); }
unsigned char* FlagRow() { return Pointer(at::kFlagRow); }
unsigned char* TaskAt(unsigned k) { return SpriteRecord(k); }
unsigned char* EnemyOf(unsigned char target) {
    return Mem(at::kEnemies + static_cast<std::uint32_t>((static_cast<int>(target) - 3) * static_cast<int>(at::kEnemyStride)));
}
unsigned char* PartyOf(unsigned char actor) { return ObjectOf(actor); }
unsigned char* Script() { return g_cursor[0]; }
unsigned char** Cursor() { return &g_cursor[0]; }
unsigned char* Scratch(unsigned i) { return g_scratch + (i % kArgs) * 0x40; }
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
    for (unsigned i = 0; i < g_slot_n; ++i)
        if (g_slots[i].address == address) return ForOurs(i, key);
    bof3::Fatal("scenario_harness: ours calls 0x%X, which no stand-in covers: list it in the group's callees", (unsigned)key);
}

void Run(const Group& group) {
    const unsigned per = group.rounds ? group.rounds : 2000;
    g_group = &group;
    g_field = group.field;
    for (unsigned k = 0; k < group.n_clones; ++k)
        if (FieldShape(group.clones[k].shape)) g_field = true;
    g_slot_n = 0;
    for (unsigned i = 0; i < group.n_callees; ++i) Register(group.callees[i]);
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
        if (g_field && !InFieldRuns(c.base) && !InChapterBank(c.base))
            bof3::Log("shadow      %s: %s at 0x%X lies outside the field runs and the chapter bank", group.shadow, c.name,
                      (unsigned)c.base);
    }

    static std::uint32_t kept[64][32];
    if (group.n_data_tables > 64) bof3::Fatal("scenario_harness: %s: more than 64 .data tables", group.shadow);
    for (unsigned t = 0; t < group.n_data_tables; ++t) {
        if (group.data_tables[t].entries > 32) bof3::Fatal("scenario_harness: %s: a .data table of more than 32", group.shadow);
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
        g_seed = Next();
        g_rand_hint = Next();
        if (group.seed) group.seed(k);
        Capture(input);

        // the arguments: random, then the shape's, then the group's
        std::uint32_t a[kArgs];
        for (unsigned i = 0; i < kArgs; ++i) a[i] = Next();
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
        for (unsigned i = 0; i < kArgs && c.pointers; ++i) {
            switch (static_cast<Arg>((c.pointers >> (2 * i)) & 3)) {
            case Arg::kSprite: a[i] = Key(SpriteRecord(Next())); break;
            case Arg::kScratch: a[i] = Key(Scratch(i)); break;
            case Arg::kScript: a[i] = Key(g_cursor[0]); break;
            case Arg::kWord: break;
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
            const std::uint32_t answer =
                reinterpret_cast<FnArgs>(const_cast<void*>(fn))(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8], a[9]);
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
    if (bad) {
        for (unsigned k = 0; k < group.n_clones; ++k)
            if (bad_per[k]) bof3::Log("shadow      %s: %s mismatched in %u rounds", group.shadow, group.clones[k].name, bad_per[k]);
        bof3::Fatal("%s differs from the original in %u self-test rounds", group.shadow, bad);
    }
    g_group = nullptr;
    g_field = false;
}

}  // namespace scenario_harness

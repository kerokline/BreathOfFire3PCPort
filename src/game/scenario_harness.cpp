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

const Group* g_group = nullptr;

unsigned char* Byte(std::uint32_t address) { return Mem(address); }
std::uint16_t Word(std::uint32_t address) { return static_cast<std::uint16_t>(move_script::Word(Mem(address))); }

// The chapter bytes a handler runs with, for a handler's or kPhase's entry.
std::uint32_t Chapter() {
    return Byte(at::kState)[0] | static_cast<std::uint32_t>(Byte(at::kRun)[0]) << 8 |
           static_cast<std::uint32_t>(Byte(at::kStep)[0]) << 16 | static_cast<std::uint32_t>(Byte(at::kChapter)[0]) << 24;
}

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
    bool handler;            // a table's handler: logs the chapter bytes, answers garbage
    unsigned calls;          // the original's side, for the coverage line
};
constexpr unsigned kSlots = 256;
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
        Log5(kPhaseTag + I, Chapter(), Word(at::kTimer), Key(Sprite_Current), 0);
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
    for (unsigned i = 0; i < s.nargs && i < kArgs; ++i)
        r[i] = s.deref[i] ? HashBytes(reinterpret_cast<const void*>(static_cast<std::uintptr_t>(a[i])), s.deref[i])
                          : a[i] & s.masks[i];
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
    {SH_THEIRS(ObjTrio_ClearBit40), 0, {}, Answer::kGarbage, 0, 0},
    // the scenario engine
    {SH_OURS(Scenario_CallA), 1, {kU8}, Answer::kGarbage, 0, 0},
    {SH_THEIRS(Scenario_CallB), 1, {kU8}, Answer::kGarbage, 0, 0},
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
    {SH_THEIRS(EventOp_6x), 1, {kAll}, Answer::kGarbage, 0, 0},
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

constexpr unsigned kMaxRegions = 48;
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
    for (unsigned i = 0; i < g_slot_n; ++i)
        if (g_slots[i].address == address) return ForOurs(i, key);
    bof3::Fatal("scenario_harness: ours calls 0x%X, which no stand-in covers: list it in the group's callees", (unsigned)key);
}

void Run(const Group& group) {
    const unsigned per = group.rounds ? group.rounds : 2000;
    g_group = &group;
    g_slot_n = 0;
    for (unsigned i = 0; i < group.n_callees; ++i) Register(group.callees[i]);
    for (const Callee& c : kStandard) Register(c);
    for (unsigned k = 0; k < group.n_clones; ++k)
        for (int i = 0; i < group.clones[k].n_imms; ++i) RegisterHandler(group.clones[k].imms[i].value);
    for (unsigned t = 0; t < group.n_data_tables; ++t)
        for (unsigned i = 0; i < group.data_tables[t].entries; ++i)
            RegisterHandler(static_cast<std::uint32_t>(move_script::Long(Mem(group.data_tables[t].at + 4 * i))));

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
        }
        if (group.args) group.args(k, a);
        const std::uint32_t mask = c.ret_mask ? c.ret_mask : c.shape == Shape::kHook ? 0xFFu : 0u;
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
    if (bad) {
        for (unsigned k = 0; k < group.n_clones; ++k)
            if (bad_per[k]) bof3::Log("shadow      %s: %s mismatched in %u rounds", group.shadow, group.clones[k].name, bad_per[k]);
        bof3::Fatal("%s differs from the original in %u self-test rounds", group.shadow, bad);
    }
    g_group = nullptr;
}

}  // namespace scenario_harness

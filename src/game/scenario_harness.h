// The scenario round's shared harness: how ours calls out of a chapter's
// code, and the start-up differential fuzz every scenario group runs its
// functions through. docs/scenario_harness.md has the usage;
// docs/takeover-queue-scenario.md the round.
//
// A copy and adaptation of the spell round's magic_harness (its API one for
// one - the round-ten contract, analysis/round10_wave1_brief.md): a group file
// written for magic_harness with magic_harness -> scenario_harness and MH_ ->
// SH_ substituted compiles against this. What differs is what a bank function
// is (docs/takeover-queue-scenario.md section 2): one of three call shapes -
// a vtable slot or a state handler (void), a hook (x, z) answering in al, a
// call-table entry of 0..3 cdecl words - reading the chapter bytes
// 0x8034E0.., the flag rows Cond_Flags and the row pointer 0x929ED0, the
// field and party objects, the sprites the hooks touch. So the regions, the
// disturbance and the standard callees are the scenario engine's, and two
// fields are new: Clone::shape and Group::chapter. A group writes:
//
//   1. ours, calling out only through SH_CALL(name) / SH_AT(type, address) /
//      Phase(address) below, so the fuzz can stand recorders in for ours as
//      for the originals' copies;
//   2. a scenario_harness::Group: its clones (tools/scenario_rows.py --unit
//      GROUP --clones prints them), any callee the standard set lacks, any
//      .data table the functions dispatch through, any region beyond the
//      standard ones, a Seed(k) that puts function k's boundaries in, and its
//      chapter;
//   3. one call, scenario_harness::Run(group), under its BOF3X_SHADOW name,
//      before it injects.
//
// The recorder pool and the active flag are this harness's own, not the spell
// harness's: both can run in one process (BOF3X_SHADOW='*').
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"

namespace scenario_harness {

namespace at {

// The chapter bytes (docs/scenario-roots.md section 2): Cond_ByteFA the
// chapter (s8), 0x8034E1 Field_StatusBits, 0x8034E2 the chapter's state (its
// frame dispatches on it), 0x8034E4 MoveScript_Var7 (the run a state 2
// dispatches on), 0x8034E5 the step within a run, 0x8034E6 a word timer,
// 0x8034E8 a word, ..., 0x8034F1 Cond_ByteFD.
constexpr std::uint32_t kChapter = 0x8034E0;
constexpr std::uint32_t kState = 0x8034E2;
constexpr std::uint32_t kRun = 0x8034E4;            // MoveScript_Var7
constexpr std::uint32_t kStep = 0x8034E5;
constexpr std::uint32_t kTimer = 0x8034E6;          // u16
constexpr std::uint32_t kByteFD = 0x8034F1;         // Cond_ByteFD
// The flag rows (8 bytes each; row chapter = the chapter's flags; +0xA0 the
// story flags 0x904030) and the pointer to the current row, which
// Scenario_Start sets to Cond_Flags + 8 * chapter.
constexpr std::uint32_t kCondFlags = 0x903F90;      // Cond_Flags
constexpr std::uint32_t kCondFlagsSize = 0x108;
constexpr std::uint32_t kFlagRow = 0x929ED0;        // unsigned char *
// The wait word the handlers poll (0 = nothing waiting), Field_Request (2 =
// a message is open, 5 = an area change pending), Game_AreaNumber.
constexpr std::uint32_t kWait = 0x66C810;           // MoveScript_WaitWordDA, u16
constexpr std::uint32_t kRequest = 0x66C7D8;        // Field_Request, u8
constexpr std::uint32_t kArea = 0x904EFC;           // Game_AreaNumber, u16
// The chapters' counters: 0x903848 a dword whose low byte the handlers wait
// on (an event script's count), 0x90384A / 0x90384B bytes, 0x903850 the
// effect slot a handler just took.
constexpr std::uint32_t kCounter = 0x903848;
// The field and party objects: ObjTrio (three of 0x14C; Field_State points
// at one), Sprite_Objects (30 of 0xA4) and Sprite_ObjectsExtra (4 of 0xA4).
constexpr std::uint32_t kObjTrio = 0x802D40;
constexpr std::uint32_t kObjStride = 0x14C;
constexpr std::uint32_t kFieldState = 0x905D98;     // unsigned char *
constexpr std::uint32_t kSprites = 0x7DEE80;        // Sprite_Objects
constexpr std::uint32_t kSpriteStride = 0xA4;
constexpr unsigned kSpriteCount = 30;
constexpr std::uint32_t kSpritesExtra = 0x802000;   // Sprite_ObjectsExtra
constexpr std::uint32_t kEffects = 0x7E11E0;        // Effect_Objects, 20 of 0x80

// The spell harness's names, kept so a group file written for it compiles
// (the contract). Nothing of the scenario engine reads them; the harness
// neither seeds nor compares them.
constexpr std::uint32_t kTasks = 0x93A000;
constexpr std::uint32_t kTaskStride = 0x84;
constexpr unsigned kTaskCount = 48;
constexpr std::uint32_t kCurrentSlot = 0x93B8C4;
constexpr std::uint32_t kOwner = 0x93B940;
constexpr std::uint32_t kBattle = 0x904AA0;
constexpr std::uint32_t kBattleSize = 0xB0;
constexpr std::uint32_t kFlags = 0x904AA8;
constexpr std::uint32_t kActor = 0x904B34;
constexpr std::uint32_t kTarget = 0x904B44;
constexpr std::uint32_t kSource = 0x904B4C;
constexpr std::uint32_t kMessageUp = 0x939F60;
constexpr std::uint32_t kParty = 0x802D40;
constexpr std::uint32_t kPartyStride = 0x14C;
constexpr std::uint32_t kEnemies = 0x93B960;
constexpr std::uint32_t kEnemyStride = 0x128;

}  // namespace at

// --- for ours ----------------------------------------------------------------
//
// In the game every one of these is the callee itself. While a group's fuzz
// runs ours, each answers the recorder standing in for that callee (a Fatal if
// none does: the group lists it).

extern bool g_active;
const void* StandIn(std::uint32_t key);

template <typename F> inline F Call(F callee) {
    if (!g_active) return callee;
    return reinterpret_cast<F>(const_cast<void*>(StandIn(static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(callee)))));
}

// A named callee, Capcom's or ours alike: SH_CALL(Flags_Test)(row, 3).
#define SH_CALL(name) ::scenario_harness::Call(name)
// An unnamed one, by its address: SH_AT(void (__cdecl*)(unsigned), 0x4410B0)(1).
#define SH_AT(type, address) ::scenario_harness::Call(reinterpret_cast<type>(static_cast<std::uintptr_t>(address)))

// A handler a chapter dispatches to through a table (a .data state or run
// table, a stack table's immediate): called by the address the table holds,
// which in the game is Capcom's function or the jmp Inject put there to ours.
using Handler = void (__cdecl*)();
inline Handler Phase(std::uint32_t address) { return SH_AT(Handler, address); }

// --- for a group's fuzz --------------------------------------------------------

// A relative call or tail jmp leaving a clone: the offset of its E8 / E9 and
// the callee the disassembly showed (checked, then re-aimed at its recorder).
struct CallSite { std::uint32_t offset, target; };
// A stack table's immediate in a clone: `mov [esp + k], imm32` at offset - 4
// (the offset is the imm32's): checked, then re-aimed at the recorder for
// that handler.
struct Imm { std::uint32_t offset, value; };
// A jump table inside a clone: the jmp's disp32, the table (offsets from the
// function's start) and its entries - moved into the copy.
struct JumpTable { std::uint32_t jmp_disp, table, entries; };

// How the engine calls a bank function (docs/takeover-queue-scenario.md
// section 2), and so how the harness calls the clone and ours:
//   kSlot    a vtable slot 0 (Field_ModeDispatch's) - void, no arguments;
//   kState   a state or run handler its chapter's frame jumps to through a
//            table - void, no arguments (the same call as kSlot);
//   kHook    a vtable slot 2, 3 or 4: (x, z), 16.16 each, answering in al
//            (the callers movsx it) - the answer is compared (ret_mask 0xFF
//            unless the clone says otherwise); x and z are drawn as a cell
//            0..0x7F and a fraction unless the group's args say;
//   kEntry   a call-table entry (Scenario_CallA / B): 0..3 cdecl words where
//            the caller left them - random unless the group's args say;
//   kObject  a vtable slot 1: the field object (0x56D6D0 pushes it) - one of
//            the first four Sprite_Objects records unless the group's args say.
enum class Shape : std::uint8_t { kSlot, kState, kHook, kEntry, kObject };

struct Clone {
    const char* name;
    std::uint32_t base, size;
    const CallSite* calls;
    int n_calls;
    const Imm* imms;
    int n_imms;
    const JumpTable* tables;
    int n_tables;
    const void* ours;
    // For a function that answers: what of eax its callers read (0xFF for a
    // byte in al), logged after each pass and compared; 0 for a handler (a
    // kHook compares 0xFF when this is 0).
    std::uint32_t ret_mask = 0;
    // No recorder moves anything while this one runs.
    bool calm = false;
    // Added for the scenario round: the call shape (above).
    Shape shape = Shape::kSlot;
};

// The most arguments a recorder takes. A caller that pushes fewer leaves its
// own frame in the rest, never logged.
constexpr unsigned kArgs = 10;

// How a recorder answers (magic_harness.h's kinds, the same semantics):
//   kGarbage  any eax;
//   kByte     a byte in lo..hi with garbage above it (lo above hi wraps
//             through 0xFF: Effect_FindFree's 0xFF..0x13 is "none, or 0..19");
//   kFlag     a byte of 0 (a third of the time; half of those a whole eax of
//             0, half garbage above) or not 0, garbage above;
//   kBool     a whole eax of exactly 0 (a third of the time) or 1 (Flags_Test,
//             File_LoadDone);
//   kRand     the harness's Rand (SetRandHint, SetRandFirst);
//   kPhase    a function of the group's own called directly (not through a
//             table): answers garbage and logs, like a handler, the chapter
//             bytes it ran with - the state, run and step, the timer,
//             Sprite_Current - and the dword at masks[0] when that is not 0;
//   kThrough  no recorder: both sides call the callee for real, nothing logged.
enum class Answer : std::uint8_t { kGarbage, kByte, kFlag, kBool, kRand, kPhase, kThrough };

// A callee the standard set (scenario_harness.cpp, kStandard) lacks, or one of
// the standard set the group needs recorded differently (the group's listing
// is registered first and stands). `key` is the pointer ours calls and
// `address` the original's; masks[i] is what of argument i the callee reads.
// deref / effect / custom as magic_harness.h has them.
using Effect = std::uint32_t (*)(const std::uint32_t* args, std::uint32_t answer);
struct Callee {
    const char* name;
    std::uint32_t address, key;
    unsigned nargs;
    std::uint32_t masks[kArgs];
    Answer answer;
    std::uint8_t lo, hi;
    std::uint8_t deref[kArgs] = {};
    Effect effect = nullptr;
    const void* custom = nullptr;
};

// A .data table of handlers the functions read in place (a chapter's state or
// run table): its entries are swapped for recorders while the fuzz runs, then
// put back.
struct DataTable { std::uint32_t at; unsigned entries; };

struct Region { std::uint32_t at, size; };

struct Group {
    const char* shadow;             // the BOF3X_SHADOW name, and the log's
    const Clone* clones;
    unsigned n_clones;
    const Callee* callees;          // beyond the standard set; may be null
    unsigned n_callees;
    const DataTable* data_tables;
    unsigned n_data_tables;
    const Region* regions;          // beyond the standard ones; may be null
    unsigned n_regions;
    void (*seed)(unsigned k);       // function k's boundaries, after the random fill
    void (*disturb)(std::uint32_t h);   // optional: move a group cell after a call
    unsigned rounds;                // per function; 0 is 2,000
    // Optional: called after every disturbance, to put back what the function
    // being fuzzed reads again after a call and cannot survive garbage in.
    void (*settle)() = nullptr;
    // Non-zero: the step and run bytes (0x8034E5, 0x8034E4) the disturbance
    // moves stay below this. 0, the default: any byte.
    unsigned phase_span = 0;
    // Optional: the kArgs words function k is called with (random on entry,
    // then the shape's defaults), filled after the seed; both passes get the
    // same.
    void (*args)(unsigned k, std::uint32_t* a) = nullptr;
    // Added for the scenario round: the chapter, written into Cond_ByteFA
    // (0x8034E0) every round before the seed, and the flag-row pointer
    // 0x929ED0 set to Cond_Flags + 8 * chapter, as Scenario_Start sets both.
    int chapter = 0;
};

// Clones every function (before the caller injects), fuzzes each against ours
// for g.rounds rounds, logs the counts, and is a Fatal on any difference.
void Run(const Group& g);

// --- seeding helpers, for Seed and Disturb -------------------------------------

std::uint32_t Next();
bool Often();                        // two in three
bool Half();
std::uint32_t Pick(const std::uint32_t* values, unsigned n);
#define SH_PICK(...) [] { static const std::uint32_t kV[] = {__VA_ARGS__}; return ::scenario_harness::Pick(kV, sizeof kV / sizeof kV[0]); }()

unsigned char* Mem(std::uint32_t address);
unsigned char* SpriteRecord(unsigned k);   // Sprite_Objects record k % 4
unsigned char* ObjectOf(unsigned k);       // ObjTrio record k % 3
unsigned char* FlagRow();                  // what 0x929ED0 points at
void SetPointer(std::uint32_t cell, const void* p);
unsigned char* Pointer(std::uint32_t cell);
// Rand's recorder: `hint` is where Rand & 0xFF lands a third of the time;
// `first` (0..255) is the round's first answer exactly.
void SetRandHint(std::uint32_t hint);
void SetRandFirst(int first);
// The spell harness's helpers, kept for the contract: TaskAt is SpriteRecord,
// PartyOf is ObjectOf (by actor), EnemyOf is the spell harness's record
// address (not a region here).
unsigned char* TaskAt(unsigned k);
unsigned char* EnemyOf(unsigned char target);
unsigned char* PartyOf(unsigned char actor);

// --- for a Callee's effect or custom stand-in ------------------------------------
//
// Everything here depends only on the log so far and the state, so the two
// passes see the same.

void Record(std::uint32_t address, std::uint32_t a = 0, std::uint32_t b = 0, std::uint32_t c = 0, std::uint32_t d = 0);
void Stir();
std::uint32_t Noise();
void Note(std::uint32_t a, std::uint32_t b = 0, std::uint32_t c = 0, std::uint32_t d = 0);
void NoteBytes(const void* p, unsigned n);
void FillBytes(void* p, unsigned n);
std::uint32_t HashBytes(const void* p, unsigned n);

}  // namespace scenario_harness

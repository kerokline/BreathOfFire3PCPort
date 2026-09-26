// The spell round's shared harness: how ours calls out of an effect, and the
// start-up differential fuzz every spell group runs its functions through.
// docs/magic_harness.md has the usage; docs/takeover-queue-round9-spells.md
// the round.
//
// Every function of a BMAGIC overlay runs as a battle task (BattleTask_RunAll's
// slots, Sprite_Current the slot, 0x93B940 its owner) and takes and returns
// nothing (docs/magic_fx_reached.md section 9). So one frame of state - the
// task slots, the battle bytes, the party and enemy records, the overlay's own
// .data - is every input, and one recorder per callee is every output. A
// group writes:
//
//   1. ours, calling out only through MH_CALL(name) / MH_AT(type, address) /
//      Phase(address) below, so the fuzz can stand recorders in for ours as
//      for the originals' copies;
//   2. a magic_harness::Group: its clones (tools/magic_rows.py --clones prints
//      them), any callee the standard set lacks (or records too coarsely: a
//      pointer argument, an answer through memory), any .data table the
//      functions dispatch through, any region beyond the standard ones, and a
//      Seed(k) that puts function k's boundaries in;
//   3. one call, magic_harness::Run(group), under its BOF3X_SHADOW name, before
//      it injects.
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"

namespace magic_harness {

namespace at {

// BattleTask_Create's slots: 48 of 0x84 bytes. Sprite_Current is the slot
// being run; 0x93B8C4 is the same slot as BattleTask_RunAll sets it (some
// effects read one, some the other: docs/battle_fx_tasks.md section 1), and
// 0x93B940 the slot's +0x80, its owner.
constexpr std::uint32_t kTasks = 0x93A000;
constexpr std::uint32_t kTaskStride = 0x84;
constexpr unsigned kTaskCount = 48;
constexpr std::uint32_t kCurrentSlot = 0x93B8C4;
constexpr std::uint32_t kOwner = 0x93B940;

// The battle bytes (docs/magic_fx_reached.md section 2).
constexpr std::uint32_t kBattle = 0x904AA0;         // 0x904AA0..0x904B50, the battle globals
constexpr std::uint32_t kBattleSize = 0xB0;
constexpr std::uint32_t kFlags = 0x904AA8;          // u8: bit 2 "the effect is done"; the dword's 0x800 the rows' gate
constexpr std::uint32_t kActor = 0x904B34;          // u8: the acting actor, 0..2 a party member
constexpr std::uint32_t kTarget = 0x904B44;         // u8: the target, 3.. an enemy
constexpr std::uint32_t kSource = 0x904B4C;         // unsigned char *: the sprite the effect starts from
constexpr std::uint32_t kMessageUp = 0x939F60;      // u8: the battle message window is up

// The party (ObjTrio, stride 0x14C) and the enemies (stride 0x128, by the
// battle index - 3, unchecked by the originals).
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

// A named callee, Capcom's or ours alike: MH_CALL(Rand)().
#define MH_CALL(name) ::magic_harness::Call(name)
// An unnamed one, by its address: MH_AT(void (__cdecl*)(unsigned, unsigned), 0x4B58F0)(item, category).
#define MH_AT(type, address) ::magic_harness::Call(reinterpret_cast<type>(static_cast<std::uintptr_t>(address)))

// A phase handler an effect dispatches to through a table (a stack table's
// immediate, a .data entry): called by the address the table holds, which in
// the game is Capcom's function or the jmp Inject put there to ours.
using Handler = void (__cdecl*)();
inline Handler Phase(std::uint32_t address) { return MH_AT(Handler, address); }

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
    // byte in al), logged after each pass and compared; 0 for a phase.
    std::uint32_t ret_mask = 0;
    // No recorder moves anything while this one runs: for a function whose
    // callees touch no battle state and which indexes its own stack by a cell
    // it reads again (0x4C0E30, group S18), where a moved cell would send the
    // original's store out of its frame.
    bool calm = false;
};

// The most arguments a recorder takes (Gte_RotTransPers4 takes ten). A
// caller that pushes fewer leaves its own frame in the rest, never logged.
constexpr unsigned kArgs = 10;

// How a recorder answers:
//   kGarbage  any eax;
//   kByte     a byte in lo..hi with garbage above it (an index, a count; lo
//             above hi wraps through 0xFF, so 0xFF..0x3F is "none, or 0..0x3F");
//   kFlag     a byte of 0 (a third of the time) or not 0, garbage above;
//   kBool     a whole eax of exactly 0 (a third of the time) or 1, for a
//             callee whose callers test all 32 bits;
//   kRand     the harness's Rand (SetRandHint, SetRandFirst);
//   kPhase    a function of the group's own called directly (not through a
//             table): answers garbage and logs, like a handler, the task it
//             ran for - Sprite_Current, the owner, the phase bytes +1 / +2,
//             and the dword at masks[0] when that is not 0 (a pool's
//             "current" cell);
//   kThrough  no recorder: the copy's site keeps its target and ours gets the
//             callee back from Call, so both sides call it for real. For a
//             deterministic callee of state the regions hold that answers
//             through pointers - the GTE and GPU library, Math_Sin - so what
//             it computes lands in the compared state. Nothing is logged.
enum class Answer : std::uint8_t { kGarbage, kByte, kFlag, kBool, kRand, kPhase, kThrough };

// A callee the standard set (magic_harness.cpp, kStandard) lacks, or one of
// the standard set the group needs recorded differently (the group's listing
// is registered first and stands). `key` is the pointer ours calls -
// (std::uint32_t)name, which is Capcom's address or our function - and
// `address` the original's, the one clones call; for a callee that is
// Capcom's the two are equal. masks[i] is what of argument i the callee
// reads (a u8 argument is pushed with garbage above it; a pointer into the
// caller's stack differs between the passes: mask it 0).
//
// The three optional fields, from least to most the group's own:
//   deref[i]  not 0: argument i is logged as a hash of that many bytes at
//             it instead of the pointer (an input the caller built in its own
//             frame: a GTE vector, an angle triple);
//   effect    called after the recorder logs and disturbs, with every argument
//             slot and the answer its kind chose; what it returns is the
//             answer. It may log what the callee reads (Note, NoteBytes),
//             fill what it writes (FillBytes), move a cell the real callee
//             moves (Gfx_PacketNext), or put back a cell the caller cannot
//             survive garbage in. Only the callee's own arguments are the
//             caller's (see kArgs);
//   custom    the group's own stand-in, used in place of the harness's
//             recorder on both sides: a function of the callee's exact type
//             that logs through Record, disturbs through Stir, and answers
//             from Noise. nargs / masks / answer / deref / effect are then
//             unused.
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

// A .data table of handlers the functions read in place: its entries are
// swapped for recorders while the fuzz runs, then put back.
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
    // being fuzzed reads again after a call and cannot survive garbage in (a
    // phase byte it indexes a table with past a call).
    void (*settle)() = nullptr;
    // Non-zero: a phase byte (+1, +2) the disturbance moves stays below this -
    // for a group whose dispatchers read the phase after a call through a
    // table of that many entries (MAGIC104's 0x4D12A0: 2). 0, the default:
    // any byte.
    unsigned phase_span = 0;
    // Optional: for functions that take arguments, the kArgs words function
    // k is called with (random on entry), filled after the seed; both passes
    // get the same. Null: a task step, called with three ignored words.
    void (*args)(unsigned k, std::uint32_t* a) = nullptr;
};

// Clones every function (before the caller injects), fuzzes each against ours
// for g.rounds rounds, logs the counts, and is a Fatal on any difference.
void Run(const Group& g);

// --- seeding helpers, for Seed and Disturb -------------------------------------

std::uint32_t Next();
bool Often();                        // two in three
bool Half();
std::uint32_t Pick(const std::uint32_t* values, unsigned n);
#define MH_PICK(...) [] { static const std::uint32_t kV[] = {__VA_ARGS__}; return ::magic_harness::Pick(kV, sizeof kV / sizeof kV[0]); }()

unsigned char* Mem(std::uint32_t address);
unsigned char* TaskAt(unsigned k);   // one of the first four slots
unsigned char* SpriteRecord(unsigned k);   // one of the harness's two sprite records
unsigned char* EnemyOf(unsigned char target);   // as the originals index it
unsigned char* PartyOf(unsigned char actor);
void SetPointer(std::uint32_t cell, const void* p);
unsigned char* Pointer(std::uint32_t cell);
// Rand's recorder: `hint` is where Rand & 0xFF lands a third of the time;
// `first` (0..255) is the round's first answer exactly.
void SetRandHint(std::uint32_t hint);
void SetRandFirst(int first);

// --- for a Callee's effect or custom stand-in ------------------------------------
//
// Everything here depends only on the log so far and the state, so the two
// passes see the same.

// A custom stand-in's call: one log entry, counted in the coverage line
// against the callee at `address` (Callee::address).
void Record(std::uint32_t address, std::uint32_t a = 0, std::uint32_t b = 0, std::uint32_t c = 0, std::uint32_t d = 0);
// The disturbance the harness's recorders make after recording.
void Stir();
// The recorders' stream: a new value at each call, the same on both passes.
std::uint32_t Noise();
// One more entry in the log, compared like a call: a value the callee read.
void Note(std::uint32_t a, std::uint32_t b = 0, std::uint32_t c = 0, std::uint32_t d = 0);
// n bytes at p into the log: up to 16 as they are, more as a hash.
void NoteBytes(const void* p, unsigned n);
// n bytes at p from the recorders' stream (what a callee writes).
void FillBytes(void* p, unsigned n);
// FNV-1a over n bytes.
std::uint32_t HashBytes(const void* p, unsigned n);

}  // namespace magic_harness

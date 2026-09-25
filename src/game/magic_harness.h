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
//      them), any callee the standard set lacks, any .data table the
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
};

// How a recorder answers: a byte in lo..hi with garbage above it (an index,
// a count), a byte of 0 or not 0 (a flag), the harness's Rand, or garbage.
enum class Answer : std::uint8_t { kGarbage, kByte, kFlag, kRand };

// A callee the standard set (magic_harness.cpp, kStandard) lacks. `key` is
// the pointer ours calls - (std::uint32_t)name, which is Capcom's address or
// our function - and `address` the original's, the one clones call; for a
// callee that is Capcom's the two are equal. masks[i] is what of argument i
// the callee reads (a u8 argument is pushed with garbage above it).
struct Callee {
    const char* name;
    std::uint32_t address, key;
    unsigned nargs;
    std::uint32_t masks[4];
    Answer answer;
    std::uint8_t lo, hi;
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

}  // namespace magic_harness

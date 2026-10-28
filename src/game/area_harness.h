// The area round's shared harness: how ours calls out of an area's code, and
// the start-up differential fuzz every area group runs its functions through.
// docs/area_harness.md has the usage; docs/takeover-queue-areas.md the round.
//
// A copy of the spell round's magic_harness.h, adapted (round ten, group
// ARH): the same API one for one - AH_CALL / AH_AT / Phase, Group, Clone,
// Callee, Answer, DataTable, Run, the same fields, defaults and semantics - so
// that a group file written for magic_harness compiles here with the names
// substituted. What is different is what an area function is (the six call
// shapes, Clone::shape), the state (one frame of field state, not of battle
// state), the disturbance, and the standard callees (the area band's
// frontier). The harness keeps its own recorders and its own g_active, so the
// spell groups' fuzzes run as before.
//
// Every function of an area overlay is reached from one of seven tables
// (docs/takeover-queue-areas.md section 1.2) and runs in the field frame: the
// leader's record and Field_State, Sprite_Current (the object running a
// script, or the leader), the party records, the flags, the area map, the
// message word, the area's own .data. So one frame of that state is every
// input, and one recorder per callee is every output. A group writes:
//
//   1. ours, calling out only through AH_CALL(name) / AH_AT(type, address) /
//      Phase(address) below;
//   2. an area_harness::Group: its clones (hand-built or by tools/area_rows.py
//      --clones), each with its call shape; any callee the standard set lacks;
//      any .data table the functions dispatch through (DataTable); any region
//      beyond the standard ones (the area's own .data state); a Seed(k); its
//      area number (Group::area);
//   3. one call, area_harness::Run(group), under its BOF3X_SHADOW name, before
//      it injects.
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"

namespace area_harness {

namespace at {

// --- the field frame (docs/area_harness.md section 4) ---

// The party's records (ObjTrio: three of 0x14C; record 0 the leader, which
// Field_State points at in the field).
constexpr std::uint32_t kParty = 0x802D40;
constexpr std::uint32_t kPartyStride = 0x14C;
constexpr unsigned kPartyCount = 3;
// The field objects (Sprite_Objects: 30 of 0xA4 bytes).
constexpr std::uint32_t kObjects = 0x7DEE80;
constexpr std::uint32_t kObjectStride = 0xA4;
constexpr unsigned kObjectCount = 30;
constexpr std::uint32_t kObjectsExtra = 0x802000;   // Sprite_ObjectsExtra: four more of 0xA4
// The message word (u16): the message a choice handler opened, 0xFFFF none
// (MsgBox_ChoiceCommit reads it after the handler; docs/item-use.md section 5).
constexpr std::uint32_t kMessage = 0x7DEE48;
constexpr std::uint32_t kFieldState = 0x905D98;     // Field_State: unsigned char *, the leader's record
constexpr std::uint32_t kMapBytes = 0x905D94;       // AreaMap_Bytes: const unsigned char *
constexpr std::uint32_t kAreaNumber = 0x904EFC;     // Game_AreaNumber: u16
constexpr std::uint32_t kCondFlags = 0x903F90;      // Cond_Flags: rows of 8 bytes
constexpr std::uint32_t kStoryFlags = 0x904030;     // the story flags, then the party lists 0x904062 / 0x904065
constexpr std::uint32_t kFieldRequest = 0x66C7D8;   // Field_Request: u8
constexpr std::uint32_t kMemberCount = 0x929EC0;    // Field_MemberCount: u8
constexpr std::uint32_t kMapHeader = 0x8CB580;      // AreaMap_Header: the loaded area block
constexpr std::uint32_t kMapHeaderBytes = 0x2000;   // as much of it as the harness holds
constexpr std::uint32_t kMapEntryBase = 0x8CB5A2;   // AreaMap_EntryBase: u16, a dword index into the block
constexpr std::uint32_t kDescriptors = 0x667590;    // Area_Descriptors: 200 pointers, left in place

// --- the spell harness's cells, kept so that a group file written for
// magic_harness compiles unchanged (docs/area_harness.md section 3). None of
// them is in the area harness's regions. ---
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
constexpr std::uint32_t kEnemies = 0x93B960;
constexpr std::uint32_t kEnemyStride = 0x128;

}  // namespace at

// --- for ours ----------------------------------------------------------------
//
// In the game every one of these is the callee itself. While a group's fuzz
// runs ours, each answers the recorder standing in for that callee (a Fatal if
// none does: the group lists it). The flag is this harness's own:
// magic_harness::g_active is another.

extern bool g_active;
const void* StandIn(std::uint32_t key);

template <typename F> inline F Call(F callee) {
    if (!g_active) return callee;
    return reinterpret_cast<F>(const_cast<void*>(StandIn(static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(callee)))));
}

// A named callee, Capcom's or ours alike: AH_CALL(Flags_Test)(bits, 0x13).
#define AH_CALL(name) ::area_harness::Call(name)
// An unnamed one, by its address: AH_AT(void (__cdecl*)(unsigned, unsigned, unsigned), 0x579F00)(x, z, 0).
#define AH_AT(type, address) ::area_harness::Call(reinterpret_cast<type>(static_cast<std::uintptr_t>(address)))

// A phase an area function dispatches to through a table (a stack table's
// immediate, a .data entry): called by the address the table holds, which in
// the game is Capcom's function or the jmp Inject put there to ours.
using Handler = void (__cdecl*)();
inline Handler Phase(std::uint32_t address) { return AH_AT(Handler, address); }
// A hook's shape (Area_StepHook's, Area_ArriveHook's, the cell hook's
// entries): (x, z) in, al out.
using Hook = int (__cdecl*)(unsigned, unsigned);

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

// How the engine calls an area function: which of the roots of
// docs/takeover-queue-areas.md section 2 it hangs from. The shape decides how
// the harness calls the clone and what it reads after.
//   kHandler  Area_Descriptors[area] +0x3C [n]: the movement-script ops 03 /
//             DE (MoveScript_GroupD), Sprite_Current the object running the
//             script; void (void). DE then reads Sprite_Current[8]: logged
//             after the call (the pointer, and the byte when it lies in the
//             regions).
//   kChoice   +0x34 [id]: MsgBox_ChoiceCommit / MsgBox_MenuCommit for a
//             choice id below 0x80; void (void). The commit reads the message
//             word 0x7DEE48 after (0xFFFF: no new message): logged after.
//   kInit     +0x40: Area_Enter, once per area entry, after the map and the
//             party are placed; void (void).
//   kHook     Area_StepHook / Area_ArriveHook's per-area handlers and the cell
//             hook's (Area_CellHook 0x56E670): (x, z) in, al out. Called with
//             the group's args, or with a cell (x, z) of the harness's draw;
//             the answer compared as ret_mask says (0: 0xFF).
//   kTail     Field_ModeTailRun's table 0x662CE8 (by the s8 0x9039F3): a
//             phase, void (void).
//   kState    a state handler the area's own frame function reaches through a
//             table in the area's .data; void (void).
//   kCallee   not a root: a function of the area's own that its roots call
//             directly (arguments the group's args, answer its ret_mask).
// The default is kHandler, the commonest root (678 of the band's entries).
enum class Shape : std::uint8_t { kHandler, kChoice, kInit, kHook, kTail, kState, kCallee };

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
    // byte in al), logged after each pass and compared; 0 for a phase (a
    // kHook's 0 is 0xFF).
    std::uint32_t ret_mask = 0;
    // No recorder moves anything while this one runs.
    bool calm = false;
    // How the engine calls it (above).
    Shape shape = Shape::kHandler;
};

// The most arguments a recorder takes (Gte_RotTransPers4 takes nine). A
// caller that pushes fewer leaves its own frame in the rest, never logged.
constexpr unsigned kArgs = 10;

// How a recorder answers (magic_harness.h has the same list):
//   kGarbage  any eax;
//   kByte     a byte in lo..hi with garbage above it (lo above hi wraps
//             through 0xFF, so 0xFF..0x07 is "none, or 0..7");
//   kFlag     a byte of 0 (a third of the time; half of those a whole eax
//             of 0, half garbage above) or not 0, garbage above;
//   kBool     a whole eax of exactly 0 (a third of the time) or 1;
//   kRand     the harness's Rand (SetRandHint, SetRandFirst);
//   kPhase    a function of the group's own called directly (not through a
//             table): answers garbage and logs, like a handler, the object it
//             ran for - Sprite_Current, Field_State, the phase bytes +1 / +2,
//             and the dword at masks[0] when that is not 0;
//   kThrough  no recorder: both sides call the callee for real, nothing is
//             logged (GTE, GPU setters, Math_*).
enum class Answer : std::uint8_t { kGarbage, kByte, kFlag, kBool, kRand, kPhase, kThrough };

// A callee the standard set (area_harness.cpp, kStandard) lacks, or one of
// the standard set the group needs recorded differently (the group's listing
// is registered first and stands). `key` is the pointer ours calls -
// (std::uint32_t)name, which is Capcom's address or our function - and
// `address` the original's, the one clones call. masks[i] is what of argument
// i the callee reads (a pointer into the caller's stack differs between the
// passes: mask it 0, or deref it). deref / effect / custom as in
// magic_harness.h.
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
// swapped for recorders while the fuzz runs, then put back. Added for the
// areas (defaults keep magic_harness's meaning):
//   stride  bytes from one entry's pointer to the next (the cell hook's
//           (area, fn) pairs: 8, `at` the first fn);
//   nargs   how many argument words its handlers take (a hook's 2: logged
//           with the handler's entry).
struct DataTable {
    std::uint32_t at;
    unsigned entries;
    unsigned stride = 4;
    unsigned nargs = 0;
};

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
    void (*settle)() = nullptr;     // after every disturbance
    unsigned phase_span = 0;        // a phase byte (+1, +2) the disturbance moves stays below this
    // Optional: the kArgs words function k is called with, filled after the
    // seed; both passes get the same. Null: a root called with three words (a
    // kHook's first two a cell).
    void (*args)(unsigned k, std::uint32_t* a) = nullptr;
    // The area whose code this is: written into Game_AreaNumber every round,
    // after the random fill and before the seed, with the real descriptor
    // and the real tables left in place. -1: not written (the seed says).
    int area = -1;
};

// Clones every function (before the caller injects), fuzzes each against ours
// for g.rounds rounds, logs the counts, and is a Fatal on any difference.
void Run(const Group& g);

// --- seeding helpers, for Seed and Disturb -------------------------------------

std::uint32_t Next();
bool Often();                        // two in three
bool Half();
std::uint32_t Pick(const std::uint32_t* values, unsigned n);
#define AH_PICK(...) [] { static const std::uint32_t kV[] = {__VA_ARGS__}; return ::area_harness::Pick(kV, sizeof kV / sizeof kV[0]); }()

unsigned char* Mem(std::uint32_t address);
unsigned char* Object(unsigned k);        // field object k % 30 (Sprite_Objects)
unsigned char* TaskAt(unsigned k);        // one of the first four field objects (magic_harness's slot)
unsigned char* SpriteRecord(unsigned k);  // one of the harness's two records of 0x140 bytes
unsigned char* EnemyOf(unsigned char target);   // as magic_harness computes it; not in the regions
unsigned char* PartyOf(unsigned char member);   // party record member % 3 (ObjTrio)
const unsigned char* Descriptor(unsigned area); // Area_Descriptors[area], the image's
void SetPointer(std::uint32_t cell, const void* p);
unsigned char* Pointer(std::uint32_t cell);
void SetRandHint(std::uint32_t hint);
void SetRandFirst(int first);

// --- for a Callee's effect or custom stand-in ------------------------------------

void Record(std::uint32_t address, std::uint32_t a = 0, std::uint32_t b = 0, std::uint32_t c = 0, std::uint32_t d = 0);
void Stir();
std::uint32_t Noise();
void Note(std::uint32_t a, std::uint32_t b = 0, std::uint32_t c = 0, std::uint32_t d = 0);
void NoteBytes(const void* p, unsigned n);
void FillBytes(void* p, unsigned n);
std::uint32_t HashBytes(const void* p, unsigned n);

}  // namespace area_harness

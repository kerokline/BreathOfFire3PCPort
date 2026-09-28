// The boss round's shared harness: how ours calls out of a boss function, and
// the start-up differential fuzz every boss group runs its functions through.
// docs/boss_harness.md has the usage; docs/takeover-queue-bosses.md the round.
//
// A copy of the spell round's magic_harness.h and the area round's
// area_harness.h, adapted (round eleven, group BH): the same API one for one -
// BH_CALL / BH_AT / Phase, Group, Clone, Callee, Answer, DataTable, Run, the
// same fields, defaults and semantics - so that a group file written for either
// compiles here with the names substituted. What is different is what a boss
// function is (the eight call shapes of the plan's section 2, Clone::shape),
// the state (one frame of battle state with the event battle's cells: the
// fight byte 0x904AAA, the three hooks 0x904B64..0x904B6C, the enemies' +0xF4
// hooks and +0xF8 / +0xFC tables, the field actors the set-ups find by tag,
// the chapter bytes), the disturbance, the standard callees (the boss band's
// frontier), and two things neither harness had: a clone driven through the
// unit's own dispatcher (Clone::via) and a dispatcher called with each state
// byte (Clone::states). The harness keeps its own recorders and its own
// g_active, so the spell, scenario and area groups' fuzzes run as before.
//
// Every function of the boss band (0x437A00..0x441000) is reached from one of
// three root sets - Boss_SetupTable by the fight byte, BossKind_Table by an
// enemy's kind byte, the effect dispatchers' stack tables - and the hooks and
// state tables those store (docs/takeover-queue-bosses.md section 1.2). A
// group writes:
//
//   1. ours, calling out only through BH_CALL(name) / BH_AT(type, address) /
//      Phase(address) below;
//   2. a boss_harness::Group: its clones (tools/boss_rows.py --clones prints
//      them), each with its call shape; any callee the standard set lacks;
//      any .data table its functions dispatch through (DataTable); any region
//      beyond the standard ones; a Seed(k); its fight id (Group::fight) and,
//      for a kind, the kind (Group::kind);
//   3. one call, boss_harness::Run(group), per fight id or kind, under its
//      BOF3X_SHADOW name, before it injects.
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"

namespace boss_harness {

namespace at {

// --- the battle frame (docs/boss_harness.md section 4) ---

// BattleTask_Create's slots: 48 of 0x84 bytes. For an effect task
// Sprite_Current is the slot being run and 0x93B8C4 the same slot as
// BattleTask_RunAll sets it; 0x93B940 the slot's owner.
constexpr std::uint32_t kTasks = 0x93A000;
constexpr std::uint32_t kTaskStride = 0x84;
constexpr unsigned kTaskCount = 48;
constexpr std::uint32_t kCurrentSlot = 0x93B8C4;
constexpr std::uint32_t kOwner = 0x93B940;

// The battle bytes, to 0x904BA0 so that the three hooks are in them.
constexpr std::uint32_t kBattle = 0x904AA0;
constexpr std::uint32_t kBattleSize = 0x100;
constexpr std::uint32_t kPhase = 0x904AA0;          // u8: Battle_PhaseDispatch's phase
constexpr std::uint32_t kStep = 0x904AA1;           // u8: the phase's step (5's: 1 the win, 2 the other way out)
constexpr std::uint32_t kSubStep = 0x904AA2;        // u8
constexpr std::uint32_t kFlags = 0x904AA8;          // u16: the round flags
constexpr std::uint32_t kFight = 0x904AAA;          // u8: the event battle - Boss_SetupTable's index; 0 none
constexpr std::uint32_t kEnemyTotal = 0x904AB2;     // u8: the enemies set up
constexpr std::uint32_t kEnemiesLeft = 0x904AB3;    // u8: Battle_EnemyDefeated counts it down
constexpr std::uint32_t kMusicFlags = 0x904AE5;     // u8: bit 0x40 keeps the battle's music
constexpr std::uint32_t kBattleEnd = 0x904AE8;      // u8: bit 0 the loss, bit 1 the win (the last enemy fallen)
constexpr std::uint32_t kActor = 0x904B34;          // u8: the acting actor, 0..2 a member, 3.. an enemy
constexpr std::uint32_t kTarget = 0x904B44;         // u8: the target, 3..10 an enemy
constexpr std::uint32_t kSource = 0x904B4C;         // unsigned char *
// The three hooks a set-up stores (symbols.toml BattleHook_End / _Exit /
// _Event): End is called once, as the battle's way out is picked
// (BattleEnd_AwaitMemberTasks 0x431464, nothing pushed); Exit on the way out
// (BattleEnd_ExitHook 0x4317B9, nothing pushed); Event with a phase code
// 0..6 (Battle_PhaseDispatch 3, Battle_Init 6, the action phases 1 / 4 / 0 /
// 5, BattleRoundEnd_NextRound 2, whose al 0xFF holds the round).
constexpr std::uint32_t kHookEnd = 0x904B64;
constexpr std::uint32_t kHookExit = 0x904B68;
constexpr std::uint32_t kHookEvent = 0x904B6C;
constexpr std::uint32_t kMessageUp = 0x939F60;      // u8: the battle message window is up

// The chapter bytes a boss hook hands the field back through (the run
// 0x8034E4 and its step 0x8034E5).
constexpr std::uint32_t kChapterRun = 0x8034E4;
constexpr std::uint32_t kChapterStep = 0x8034E5;

// The party (ObjTrio, stride 0x14C) and the enemies' objects (stride 0x128,
// eight). An enemy's working record is its object + 0x80 (0x93B9E0 + n *
// 0x128, Battle_CopyEnemyData's view); the offsets the boss code uses are the
// object's: +1..+4 the state bytes, +9 / +0xA counters, +0x92 / +0x93 the
// status, +0xA4 HP, +0xF4 the hook (called with 0, 1 or 2), +0xF8 / +0xFC
// tables, +0x100 the kind byte (BossKind_Table's index), +0x110 flags.
constexpr std::uint32_t kParty = 0x802D40;
constexpr std::uint32_t kPartyStride = 0x14C;
constexpr unsigned kPartyCount = 3;
constexpr std::uint32_t kEnemies = 0x93B960;
constexpr std::uint32_t kEnemyStride = 0x128;
constexpr unsigned kEnemyCount = 8;
constexpr std::uint32_t kEnemyWork = 0x93B9E0;      // kEnemies + 0x80
constexpr std::uint32_t kEnemyCurrent = 0x939AD8;   // the enemy EnemyRunAll is running

// The field's objects (Sprite_Objects: 30 of 0xA4 bytes). A set-up finds its
// actors among them by tag: +6 == 7 and +0x9E the tag (BossActor_Find).
constexpr std::uint32_t kObjects = 0x7DEE80;
constexpr std::uint32_t kObjectStride = 0xA4;
constexpr unsigned kObjectCount = 30;

// The loaded area's enemy formation rows (8 of 9 bytes) and its eight enemy
// data records (stride 0x8C; +0xC the byte EnemyData_FindByTag compares).
constexpr std::uint32_t kEnemyRows = 0x8C5580;
constexpr std::uint32_t kEnemyData = 0x8C55C8;
constexpr std::uint32_t kEnemyDataStride = 0x8C;

constexpr std::uint32_t kFlagBits = 0x929ED0;       // unsigned char *: the chapter's flag bits (Cond_Flags + 8 * chapter)
constexpr std::uint32_t kCondFlags = 0x903F90;      // Cond_Flags
constexpr std::uint32_t kPacketNext = 0x7E0670;     // Gfx_PacketNext

// The roots, left in place (docs/boss_harness.md section 2).
constexpr std::uint32_t kSetupTable = 0x656954;     // Boss_SetupTable: 56 set-ups by the fight byte
constexpr std::uint32_t kKindTable = 0x64B088;      // BossKind_Table: 63 dispatchers by the kind byte

}  // namespace at

// --- for ours ----------------------------------------------------------------
//
// In the game every one of these is the callee itself. While a group's fuzz
// runs ours, each answers the recorder standing in for that callee (a Fatal if
// none does: the group lists it). The flag is this harness's own.

extern bool g_active;
const void* StandIn(std::uint32_t key);

template <typename F> inline F Call(F callee) {
    if (!g_active) return callee;
    return reinterpret_cast<F>(const_cast<void*>(StandIn(static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(callee)))));
}

// A named callee, Capcom's or ours alike: BH_CALL(Sprite_ScriptTick)().
#define BH_CALL(name) ::boss_harness::Call(name)
// An unnamed one, by its address: BH_AT(void (__cdecl*)(), 0x446E20)().
#define BH_AT(type, address) ::boss_harness::Call(reinterpret_cast<type>(static_cast<std::uintptr_t>(address)))

// A handler a boss function reaches by an address in its own code (a stack
// table's immediate, a direct call to a function of its group): called by that
// address, which in the game is Capcom's function or the jmp Inject put there
// to ours. A handler read from a .data table (a kind's state table, a hook
// table) is called as read - the fuzz swaps the table's cells for recorders
// (DataTable) - and so is a hook stored in a cell (0x904B64..6C, an enemy's
// +0xF4): the fuzz keeps recorders there.
using Handler = void (__cdecl*)();
inline Handler Phase(std::uint32_t address) { return BH_AT(Handler, address); }
// A handler that answers in al (a kind's state that tail-jumps to
// Sprite_ScriptTick), and a hook's shape: one word in, al out.
using AnswerHandler = std::uint32_t (__cdecl*)();
using Hook = std::uint32_t (__cdecl*)(unsigned);
inline AnswerHandler PhaseA(std::uint32_t address) { return BH_AT(AnswerHandler, address); }
inline Hook PhaseHook(std::uint32_t address) { return BH_AT(Hook, address); }

// --- for a group's fuzz --------------------------------------------------------

// A relative call or tail jmp leaving a clone: the offset of its E8 / E9 and
// the callee the disassembly showed (checked, then re-aimed at its recorder).
struct CallSite { std::uint32_t offset, target; };
// A stack table's immediate in a clone: `mov [esp + k], imm32` at offset - 4
// (the offset is the imm32's): checked, then re-aimed at the recorder for
// that handler. An immediate a function STORES (a hook, a table pointer) is
// not one of these: it is left alone, and ours stores the same literal.
struct Imm { std::uint32_t offset, value; };
// A jump table inside a clone: the jmp's disp32, the table (offsets from the
// function's start) and its entries - moved into the copy.
struct JumpTable { std::uint32_t jmp_disp, table, entries; };

// How the engine calls a boss function: the eight shapes of
// docs/takeover-queue-bosses.md section 2, and one for a function its unit
// calls directly. The shape decides the arguments, where Sprite_Current
// points, and what is logged after.
//   kSetup     a set-up: Boss_SetupTable[fight], entered by
//              Battle_InitBossEncounter's jmp; void (void). Stores the hooks.
//   kEnd       the hook 0x904B64; void (void).
//   kExit      the hook 0x904B68; void (void).
//   kEvent     the hook 0x904B6C; one word, the phase code (0..6 drawn), al
//              out (ret_mask 0 means 0xFF).
//   kDispatch  a kind's dispatcher (BossKind_Table[kind]) or a dispatcher its
//              state table reaches: `jmp [table + 4 * Sprite_Current[state_at]]`;
//              void (void), Sprite_Current and 0x939AD8 an enemy. With
//              Clone::states set, the state byte is drawn below it each round.
//   kState     a kind's state handler (a table entry); void (void) - or al out
//              where it tail-jumps to Sprite_ScriptTick (ret_mask 0xFF).
//   kEnemyHook an enemy's +0xF4: one word, 0, 1 or 2 (0x435BF5, 0x4367EE,
//              EnemyRunAll), drawn; Sprite_Current and 0x939AD8 the enemy.
//   kTask      an effect task: BattleTask_RunAll's slot (Sprite_Current and
//              0x93B8C4 a task slot); void (void).
//   kCallee    not a root: a function its unit calls directly (arguments the
//              group's args, answer its ret_mask); Sprite_Current an enemy.
// After every call the three hooks 0x904B64..0x904B6C are read back and
// logged (they are in the compared state too): a set-up stores them, a hook
// re-points itself.
enum class Shape : std::uint8_t { kState, kSetup, kEnd, kExit, kEvent, kDispatch, kEnemyHook, kTask, kCallee };

// Driving a function the way its unit does, through the unit's dispatcher:
// the harness writes Sprite_Current[state_at] = state (or, with state_at 0,
// the call's first argument = state: a hook table dispatched by its argument),
// plants the pass's function (the copy, then ours) in `cell` - the entry of
// the dispatcher's .data table that state selects - and calls `dispatcher`
// (Capcom's code, or whoever owns it by then: the same on both passes). The
// cell is put back after each pass. For a helper reached only through
// another unit's table; the plan's "the unit's table entry swapped for the
// helper's clone". dispatcher 0: called directly.
struct Via {
    std::uint32_t dispatcher = 0;
    std::uint32_t cell = 0;
    std::uint8_t state_at = 0;
    std::uint8_t state = 0;
};

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
    // byte in al), logged after each pass and compared; 0 for none (a kEvent's
    // 0 is 0xFF).
    std::uint32_t ret_mask = 0;
    // No recorder moves anything while this one runs.
    bool calm = false;
    // How the engine calls it (above).
    Shape shape = Shape::kState;
    // For a kDispatch: the byte of Sprite_Current it dispatches by (1..4) and
    // how many entries its table has - the byte is drawn below that each round,
    // before the seed (a byte past the table would run whatever follows it, on
    // both sides). 0: left to the fill and the seed.
    std::uint8_t state_at = 1;
    std::uint8_t states = 0;
    Via via = {};
};

// The most arguments a recorder takes (MoveCmd_OpE9 takes seven). A caller
// that pushes fewer leaves its own frame in the rest, never logged.
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
//             table): answers garbage and logs, like a handler, the sprite it
//             ran for - Sprite_Current, 0x939AD8, the state bytes +1..+4, and
//             the dword at masks[0] when that is not 0;
//   kThrough  no recorder: both sides call the callee for real, nothing is
//             logged (GTE, GPU setters, Math_*).
enum class Answer : std::uint8_t { kGarbage, kByte, kFlag, kBool, kRand, kPhase, kThrough };

// A callee the standard set (boss_harness.cpp, kStandard) lacks, or one of
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

// A .data table of handlers the functions read in place (a kind's state
// table, a hook table): its entries are swapped for recorders while the fuzz
// runs, then put back.
//   stride  bytes from one entry to the next (4);
//   nargs   how many argument words its handlers take (a +0xF4 hook table's
//           1: logged with the handler's entry).
// A table with a flag header (bytes, FF padding, then pointers) is listed
// from its first pointer.
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
    unsigned phase_span = 0;        // a state byte (+1..+4) the disturbance moves stays below this
    // Optional: the kArgs words function k is called with, filled after the
    // seed; both passes get the same. Null: the shape's words (a kEvent's
    // phase code, a kEnemyHook's 0..2) and garbage after.
    void (*args)(unsigned k, std::uint32_t* a) = nullptr;
    // The fight: written into 0x904AAA every round, after the random fill and
    // before the seed. -1: an id 1..55 drawn each round.
    int fight = -1;
    // The kind: written into the current enemy's (0x939AD8's) +0x100 every
    // round, before the seed. -1: left to the fill.
    int kind = -1;
};

// Clones every function (before the caller injects), fuzzes each against ours
// for g.rounds rounds, logs the counts, and is a Fatal on any difference.
void Run(const Group& g);

// --- seeding helpers, for Seed and Disturb -------------------------------------

std::uint32_t Next();
bool Often();                        // two in three
bool Half();
std::uint32_t Pick(const std::uint32_t* values, unsigned n);
#define BH_PICK(...) [] { static const std::uint32_t kV[] = {__VA_ARGS__}; return ::boss_harness::Pick(kV, sizeof kV / sizeof kV[0]); }()

unsigned char* Mem(std::uint32_t address);
unsigned char* EnemyAt(unsigned k);       // enemy object k % 8
unsigned char* EnemyOf(unsigned char target);   // as the originals index it: target - 3 (3..10 an enemy)
unsigned char* PartyOf(unsigned char member);   // party record member % 3 (ObjTrio)
unsigned char* TaskAt(unsigned k);        // one of the first four task slots
unsigned char* Object(unsigned k);        // field object k % 30 (Sprite_Objects)
unsigned char* SpriteRecord(unsigned k);  // one of the harness's two records of 0x140 bytes
unsigned char* Packets();                 // the harness's packet buffer (Gfx_PacketNext points into it)
// The recorder a hook cell holds at the round's start (at::kHookEnd /
// kHookExit / kHookEvent, or 0 for the enemies' +0xF4): for a seed that
// moved a cell and wants it back.
std::uint32_t HookStub(std::uint32_t cell);
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

}  // namespace boss_harness

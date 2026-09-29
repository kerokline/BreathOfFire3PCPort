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
//
// Round twelve (group EH, docs/boss_harness.md section 10) widened it to the
// battle engine's address runs (at::kEngineBands) for groups BE1..BE7: four
// engine shapes (kStep, kWindow, kMember, kHelper), Clone::state_cell and
// Via::state_cell, and Group::engine - the engine frame's regions, pointers,
// disturbance and standard callees. Every addition defaults to round
// eleven's behaviour; boss_harness_eh.cpp is the harness's own self-test of
// them (BOF3X_SHADOW=boss_harness_eh).
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

// --- the battle engine's frame (round twelve, group EH; docs/boss_harness.md
// section 10). Used only by a Group with `engine` set: a boss group's state,
// draws and stand-ins are what they were.

// The bands. The boss round's is 0x437A00..0x441000; round twelve's battle
// groups (BE1..BE7, analysis/round12_cut.tsv) own functions in the union of
// the battle engine's address runs (docs/takeover-queue-field-battle.md
// section 1), which holds the boss band. An engine group's clone outside
// kEngineBands is a Fatal at Run (a cut mistake, not a fuzz result).
struct Band { std::uint32_t lo, hi; };
constexpr Band kBossBand = {0x437A00, 0x441000};
constexpr Band kEngineBands[] = {
    {0x42D7A0, 0x4552F6},   // BATE's windows 0x42D7A0 .. the last debt row 0x455290 (0x66 bytes)
    {0x4CEB40, 0x4CF4A4},   // BMAGIC's four MapCell_Handlers slots
    {0x597FC0, 0x59DB61},   // the battle windows: the gene screen, the result windows, 0x59D640
};

// The window records (WindowRecords, symbols.toml block: 22 of 0x24 bytes;
// +0 in use, +1 the kind Field_RunTaskRecords dispatches on, +2 / +3 the
// kind's and the state's byte a window handler's stack table is indexed by,
// +4 / +6 x / y in 12.4) and the dword naming the record a window handler runs
// for (Field_RunTaskRecords stores it; Window_FreeCurrent reads it).
constexpr std::uint32_t kWindows = 0x803160;
constexpr std::uint32_t kWindowStride = 0x24;
constexpr unsigned kWindowCount = 22;
constexpr std::uint32_t kWindowCurrent = 0x905B84;
// Field_State: the party member a battle object's code runs for (with
// Sprite_Current the member's object; both ObjTrio records).
constexpr std::uint32_t kMemberCurrent = 0x905D98;
// The command menu's actor (an ObjTrio member) and its command record, the
// member's +0x124 (battle_phases.md, battle_menu_states.md).
constexpr std::uint32_t kMenuActor = 0x939EC4;
constexpr std::uint32_t kMenuCommand = 0x939FA0;
constexpr std::uint32_t kMenuCommandAt = 0x124;
// Pointer cells among the battle bytes the engine code dereferences: the
// acting sprite 0x904B3C and 0x904B40 (seeded at the harness's records, as the
// spell and boss groups did), and the result record 0x904B60 (at an actor's
// +0x104: a member's or an enemy's; Effect_ApplyResult reads it).
constexpr std::uint32_t kActingSprite = 0x904B3C;
constexpr std::uint32_t kActingSprite2 = 0x904B40;
constexpr std::uint32_t kResultRecord = 0x904B60;
constexpr std::uint32_t kResultAt = 0x104;
// The battle's step bytes after kPhase / kStep / kSubStep: the step tables'
// indices (Battle_InputSteps by kSubStep 0x904AA2, BattleAction_KindSteps by
// 0x904AA3, BattleItemCmd_* and the Dragon run's tables by 0x904AA4 ...).
constexpr std::uint32_t kSubStep2 = 0x904AA3;
constexpr std::uint32_t kSubStep3 = 0x904AA4;
// BATE's mode bytes (the extra battle's 0x42D710 dispatches by 0x929F00, its
// sub-tables by 0x929F01 / 0x929F02).
constexpr std::uint32_t kModeBytes = 0x929F00;

}  // namespace at

bool InEngineBands(std::uint32_t address);

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
//              0x93B8C4 a task slot); void (void). With Clone::states set, the
//              byte at state_at is drawn below it each round, after the seed
//              (so the task's own byte wins when its owner is Sprite_Current
//              itself - BSJ's first fault, round 11 doc section 5.5).
//   kCallee    not a root: a function its unit calls directly (arguments the
//              group's args, answer its ret_mask); Sprite_Current an enemy.
// After every call the three hooks 0x904B64..0x904B6C are read back and
// logged (they are in the compared state too): a set-up stores them, a hook
// re-points itself.
//
// The battle engine's shapes (round twelve, group EH; docs/boss_harness.md
// section 10). Each wants Group::engine (a Fatal otherwise): the engine
// frame points 0x905B84 at a window record, Field_State and the menu actor at
// party members, the pointer cells among the battle bytes at records.
//   kStep      a step of a phase, menu or mode table the engine indexes by an
//              absolute byte (Battle_InputSteps by 0x904AA2, BATE's by
//              0x929F01 ...): void (void); Sprite_Current a member or an enemy.
//              With Clone::states and Clone::state_cell set, that byte is
//              drawn below states before the seed.
//   kWindow    a window's state handler: void (void), 0x905B84 one of the 22
//              window records; with Clone::states set, the record's byte at
//              state_at (the stack table's index, +2 or +3) drawn below it
//              before the seed (a dispatcher's own stack table: its Imms).
//   kMember    a battle object's state (BattleObj_RunState's tables by +1,
//              their sub-tables by +2): void (void), Sprite_Current a party
//              member's object and Field_State the same member three times in
//              four; with Clone::states set, Sprite_Current[state_at] drawn
//              below it before the seed.
//   kHelper    an engine function its callers call directly, cdecl, 0..10
//              words (the group's args, garbage otherwise), answer by
//              ret_mask (0xFF for al, 0xFFFF for ax, 0xFFFFFFFF for eax):
//              kCallee's contract in the engine frame - Sprite_Current a
//              member or an enemy, not always an enemy.
// kDispatch and kTask take Clone::state_cell too: a dispatcher by an absolute
// byte (0x42ED90's `jmp [0x64AE48 + 4 * byte 0x904AA2]`) is a kDispatch with
// state_cell 0x904AA2 and states its table's length.
enum class Shape : std::uint8_t { kState, kSetup, kEnd, kExit, kEvent, kDispatch, kEnemyHook, kTask, kCallee,
                                  kStep, kWindow, kMember, kHelper };

// Driving a function the way its unit does, through the unit's dispatcher:
// the harness writes Sprite_Current[state_at] = state (or, with state_at 0,
// the call's first argument = state: a hook table dispatched by its argument),
// plants the pass's function (the copy, then ours) in `cell` - the entry of
// the dispatcher's .data table that state selects - and calls `dispatcher`
// (Capcom's code, or whoever owns it by then: the same on both passes). The
// cell is put back after each pass. For a helper reached only through
// another unit's table; the plan's "the unit's table entry swapped for the
// helper's clone". dispatcher 0: called directly.
// state_cell (round twelve): a dispatcher that indexes by an absolute byte
// (a step table's 0x904AA2, BATE's 0x929F01) - the harness writes the byte
// there instead (state_at is then unused). For a kWindow clone, state_at
// names a byte of the window record 0x905B84 points at.
struct Via {
    std::uint32_t dispatcher = 0;
    std::uint32_t cell = 0;
    std::uint8_t state_at = 0;
    std::uint8_t state = 0;
    std::uint32_t state_cell = 0;
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
    // both sides). For a kTask the same, drawn after the seed (Shape above).
    // 0: left to the fill and the seed. The other state bytes are the seed's:
    // OtherStates below draws them inside their tables.
    //
    // A kDispatch's contract (round 11 doc section 5.1): the original's `jmp`
    // leaves the caller's stack word and the entry's eax in place, so ours
    // hands its first argument on to the entry and answers the entry's eax.
    // Port_DroppedCall (the standard listing, one argument) is what refuses a
    // dropped word when it sits in the table; ret_mask 0xFF compares the
    // answer. A group whose ours does not forward lists Port_DroppedCall with
    // 0 arguments itself (BSA, BSE) - an override, not the rule.
    std::uint8_t state_at = 1;
    std::uint8_t states = 0;
    Via via = {};
    // Round twelve: the dispatched byte at an absolute address instead of
    // Sprite_Current[state_at] (kDispatch, kStep: drawn below states before
    // the seed; kTask: after it). 0: Sprite_Current's, as before.
    std::uint32_t state_cell = 0;
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
// magic_harness.h. An effect that makes a stand-in louder by moving a cell
// the caller may have stored before the call must Note() the old value
// first, or the store is wiped rather than compared (BSH's 0x446DE0, round
// 11 doc section 5.3; EndWinEffect below is that form).
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
// Order matters when one address sits in tables with different nargs (BareRet
// in a hook table and a state table): the first-listed table's nargs stands,
// and an address that is also a Callee takes the Callee's. Run logs every
// such conflict ("handler ... in tables with nargs"); list the table whose
// nargs the run wants first (BSA's k39, BSF's K33, BSI's k58 do).
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
    // Round twelve: the battle engine's frame (docs/boss_harness.md section
    // 10) on top of the boss frame - the engine regions (the window records
    // and 0x905B84, Field_State, the command menu's cells, the transformation's
    // and the result screen's cells, BATE's mode bytes, the input words, a
    // text buffer), their pointers put back each round, the engine's standard
    // callees (kEngineStandard, registered before the boss set so their louder
    // forms stand), and the engine cells in the disturbance (its case 12, the
    // chapter bytes' for a boss group). The new shapes need it; a boss group
    // leaves it false and fuzzes exactly as before.
    bool engine = false;
    // State put back before each of the two passes but never compared: what
    // a group leaves out of its regions because the two sides may differ
    // there harmlessly (BE6's Gte_Vertices, whose fourth shorts are stale
    // stack bytes), but which one side must not inherit from the other - the
    // original's pass leaving it loaded would let ours read it unloaded. Up
    // to kMaxKeptBytes in all. (The capture review of 2026-09-29.)
    const Region* kept = nullptr;
    unsigned n_kept = 0;
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
// The dispatched byte's neighbours drawn inside their tables (round 11 doc
// section 5.2), for a seed: the first writes Sprite_Current's +1..+4 but
// `drawn` below `below`; the second +1..+3 but `at`, each below its own bound
// (0: left alone). Both were every stage-B group's static copy, folded here.
void OtherStates(unsigned drawn, unsigned below);
void OtherStates(unsigned at, unsigned n1, unsigned n2, unsigned n3);
// The recorder a hook cell holds at the round's start (at::kHookEnd /
// kHookExit / kHookEvent, or 0 for the enemies' +0xF4): for a seed that
// moved a cell and wants it back.
std::uint32_t HookStub(std::uint32_t cell);
// The engine frame's (Group::engine): window record k % 22, the record
// 0x905B84 points at, and the harness's text buffer (0x200 bytes, a region of
// an engine group, a NUL every 16th byte each round: the strings the
// engine's text stand-ins answer).
unsigned char* WindowAt(unsigned k);
unsigned char* CurrentWindow();
unsigned char* TextBuffer();
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

// --- the louder stand-ins the stage-B groups wrote, folded (round 11 doc 5.3) ----
// Each is an Effect a group listing names, or the standard set's (kStandard).
std::uint32_t TurnOrderEffect(const std::uint32_t*, std::uint32_t);   // Battle_RemoveFromTurnOrder: moves a party record's +0x91 bit 0x40, +8 or +0
std::uint32_t EndWinEffect(const std::uint32_t*, std::uint32_t);      // 0x446DE0: Notes the chapter step, then moves it
std::uint32_t ScriptBitsEffect(const std::uint32_t*, std::uint32_t);  // Msg_OpenScript: moves the script bits 0x904AAD half the time
std::uint32_t MoveCounterEffect(const std::uint32_t*, std::uint32_t); // Scenario_CallA: moves 0x903848 half the time (a group region)
std::uint32_t BannerCharEffect(const std::uint32_t*, std::uint32_t);  // Battle_OpenMsgWindow: moves the banner's character 0x66972D (a group region)
// BattleTask_Create answering "none" (0xFF) a third of the time: opt in by
// listing the callee with it - seven originals index by the answer untested
// (known-defects D163), and ours aborts where they would write past the pool.
std::uint32_t CreateMayFail(const std::uint32_t*, std::uint32_t);

// --- the engine's typed stand-ins (round twelve, group EH; the engine set) ------
// Each is kEngineStandard's for the callee named; a group may list the callee
// again with one of these or its own.
std::uint32_t WindowAllocEffect(const std::uint32_t*, std::uint32_t);   // Window_Alloc(slot, kind): the record claimed as the real one does, slot or 0xFF
std::uint32_t WindowFreeEffect(const std::uint32_t*, std::uint32_t);    // Window_FreeCurrent: +0, +2, +3 of 0x905B84's record zeroed
std::uint32_t TextPtrEffect(const std::uint32_t*, std::uint32_t);       // answers a string of the text buffer (Msg_SystemPtr, Item_NamePtr)
std::uint32_t SprintfEffect(const std::uint32_t*, std::uint32_t);       // Crt_sprintf(dst, fmt, ...): the format's first 15 bytes and a NUL into dst, noted
std::uint32_t TextArg0Effect(const std::uint32_t*, std::uint32_t);      // Notes the string at argument 0 (to its NUL, 64 at most)
std::uint32_t TextArg3Effect(const std::uint32_t*, std::uint32_t);      // ... at argument 3 (Text_DrawFont8 / Font12)
std::uint32_t TextArg4Effect(const std::uint32_t*, std::uint32_t);      // ... at argument 4, answering where it ends (Text_DrawAt / DrawSmall)
std::uint32_t BannerTextEffect(const std::uint32_t*, std::uint32_t);    // ... at argument 4 (BattleBanner_Add)

}  // namespace boss_harness

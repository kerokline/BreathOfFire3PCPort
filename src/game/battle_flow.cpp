// The battle task and the turn flow: the phase dispatch the battle's frame
// calls (0x42E400); the 48 battle-task slots - run, create, free, clear
// (0x435110..0x435295) - and two of their kinds' helpers (the rolling digits
// 0x432F10, the magic starters 0x437780 / 0x437930); the eight enemy objects'
// state dispatch and screen update (0x435830, 0x4358A0) and their animation
// helpers (0x4358D0, 0x436090, 0x4360C0, 0x436B50); an enemy's defeat and its
// drops (0x437470, 0x437580); the magic file loaders (0x4377D0, 0x4379D0);
// the number and label draws (0x444480, 0x4445A0); and the "actor is out"
// test (0x4456C0). docs/battle_flow.md.
//
// Every call goes through battle_flow::g (battle_flow_callees.h), so that the
// start-up fuzz can stand recorders in for them - for ours and for the
// originals' copies alike. Everything here is a faithful replacement.
#include "game/battle_flow.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/battle_flow_callees.h"
#include "game/cheats.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace battle_flow {

using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

namespace {
template <typename T> T Fn(std::uint32_t address) { return reinterpret_cast<T>(static_cast<std::uintptr_t>(address)); }
}  // namespace

const Callees kOriginals = {
    {Fn<Handler>(0x42E470), Fn<Handler>(0x42E990), Fn<Handler>(0x42F070), Fn<Handler>(0x42F220),
     Fn<Handler>(0x4302B0), Fn<Handler>(0x4311E0)},
    {Fn<Handler>(0x4352A0), Fn<Handler>(0x435350), Fn<Handler>(0x4378B0), Fn<Handler>(0x4357D0)},
    BattleTask_Create, Battle_RollDrops, Battle_DrawNumber,
    Sprite_UpdateScreen, Sprite_EnsureAnimation, Sprite_ScriptTick, Sprite_ScriptTickOnce, Rand,
    Fn<void (__cdecl*)(unsigned)>(kRemoveFromTurnOrder), Fn<void (__cdecl*)(unsigned)>(kSetFlagBit),
    Sprite_ReleaseTint, Fn<void (__cdecl*)(unsigned)>(kClearTurnBit), LoadDatFile, Crt_sprintf,
    Gpu_GetTPage, Gpu_SetDrawMode, Gfx_CommitPrim, Gpu_GetClut, Gpu_SetSprt,
};
Callees g = kOriginals;

}  // namespace battle_flow

using namespace battle_flow;

namespace {

// A pointer the original keeps in a dword of .data / .bss, read afresh.
unsigned char* PtrAt(std::uint32_t address) {
    return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(At(address)))));
}
void SetPtrAt(std::uint32_t address, const unsigned char* p) {
    SetLong(At(address), static_cast<std::int32_t>(reinterpret_cast<std::uintptr_t>(p)));
}
// The enemy being run (0x939AD8) and the battle-task slot being run (0x93B8C4),
// each re-read wherever the original re-reads it.
unsigned char* Enemy() { return PtrAt(at::kEnemyCurrent); }
unsigned char* CurrentTask() { return PtrAt(at::kTaskCurrent); }
unsigned char* Task(unsigned index) { return At(at::kTasks + index * at::kTaskSize); }
unsigned char* EnemyObject(unsigned index) { return At(at::kEnemies + index * at::kEnemySize); }

// A float's bits for a primitive's corner: the original's fild of a sign-
// extended 16-bit value, then fstp.
std::int32_t FloatOfShort(int v) { return std::bit_cast<std::int32_t>(static_cast<float>(static_cast<short>(v))); }

// The item's magic row: (id >> 8) picks one of the four u8 tables at
// 0x64B274, (id & 0xFF) the byte. As the original has it, the high byte is
// not checked (items use 0..3).
unsigned ItemRow(unsigned id) {
    const unsigned w = id & 0xFFFF;
    const auto* const tables = reinterpret_cast<const unsigned char* const*>(static_cast<std::uintptr_t>(at::kItemRows));
    return tables[w >> 8][w & 0xFF];
}

// The magic file of a row: the u16 at the head of its 8-byte record; 0xFFFF
// is none. A file asked for sets bit 2 of 0x904AA9 (the round flags' 0x400).
void LoadMagicRow(unsigned row) {
    const unsigned file = Word(At(at::kMagicFiles + row * 8));
    if (file == 0xFFFF) return;
    g.load_dat(static_cast<int>(file));
    At(at::kLoadFlags)[0] = static_cast<unsigned char>(At(at::kLoadFlags)[0] | 4);
}

// The event hook 0x904B6C and the enemies' +0xF4 hooks.
using HookFn = void (__cdecl*)(int);

}  // namespace

// ===========================================================================
// The battle's frame and the task slots
// ===========================================================================

// original 0x42E400 (no PSX twin paired): once a frame from the battle's frame
// 0x42E2F0. While the byte 0x904AAA is set and the phase is not 0, the
// pointer at 0x904B6C is called with 3 first; then entry (0x904AA0 & 0xFF) of
// a six-entry table the original builds on its own stack - 0x42E470,
// 0x42E990, 0x42F070, 0x42F220, 0x4302B0, 0x4311E0, the battle's phases
// (unread).
//
// As the original has it: `test al, al` twice, the second after `mov eax,
// [0x904AA0]` has replaced al - so the second test is of the phase's low
// byte, and phase 0 never runs the hook (the fuzz found it; section 2 of the
// doc). 0x904AAA is read once; the phase dword before the tests and again
// after the hook (which may move it); the hook pointer at the call. The
// index is not checked: 6..255 would call through the words above the table
// on the original's stack, its own return address first; ours aborts
// instead (CLAUDE.md rule 4, as mode_flow's Transition_Task).
extern "C" __attribute__((disable_tail_calls)) void __cdecl Battle_PhaseDispatch(void) {
    const unsigned char event = At(at::kEventBattle)[0];
    std::uint32_t phase = static_cast<std::uint32_t>(Long(At(at::kPhase)));
    if (event != 0 && (phase & 0xFF) != 0) {
        reinterpret_cast<HookFn>(PtrAt(at::kEventHook))(3);
        phase = static_cast<std::uint32_t>(Long(At(at::kPhase)));
    }
    const unsigned index = phase & 0xFF;
    if (index >= 6) bof3::Fatal("Battle_PhaseDispatch: phase %u, past the six-entry table", index);
    g.phases[index]();
}

// original 0x435110: each of the 48 battle-task slots whose byte +0 is not
// zero - the slot to 0x93B8C4 and to Sprite_Current, its +0x80 dword to
// 0x93B940 - runs the handler of its kind (+6) from a four-entry table the
// original builds on its own stack: 0x4352A0, 0x435350, 0x4378B0 (the magic
// effect BattleTask_Create's callers start), 0x4357D0.
//
// As the original has it: a slot runs when its byte +0 is non-zero, whatever
// the bits (BattleTask_Create takes a slot by bit 0 alone); byte +0 and the
// kind are read afresh for each slot, so a handler that frees or takes a
// later slot is seen the same frame. The kind is not checked: 4..255 would
// call through the original's stack above the table; ours aborts.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleTask_RunAll(void) {
    for (unsigned i = 0; i < at::kTaskCount; ++i) {
        unsigned char* const t = Task(i);
        if (t[0] == 0) continue;
        const std::int32_t owner = Long(t + 0x80);
        SetPtrAt(at::kTaskCurrent, t);
        Sprite_Current = t;
        SetLong(At(at::kTaskOwner), owner);
        const unsigned kind = t[6];
        if (kind >= 4) bof3::Fatal("BattleTask_RunAll: slot %u has kind %u, past the four-entry table", i, kind);
        g.tasks[kind]();
    }
}

// original 0x435180 (PSX 0x801E584C, the same): the first of the 48 slots
// whose bit 0 is clear gets bit 0, the kind at +6 and the parameter at +5;
// its index is returned, or 0xFF when every slot is taken.
//
// As the original has it: kind and parameter are the arguments' low bytes;
// the other bits of byte +0 are kept. The callers do not test for 0xFF.
extern "C" unsigned char __cdecl BattleTask_Create(unsigned kind, unsigned parameter) {
    for (unsigned i = 0; i < at::kTaskCount; ++i) {
        unsigned char* const t = Task(i);
        if ((t[0] & 1) != 0) continue;
        t[0] = static_cast<unsigned char>(t[0] | 1);
        t[6] = static_cast<unsigned char>(kind);
        t[5] = static_cast<unsigned char>(parameter);
        return static_cast<unsigned char>(i);
    }
    return 0xFF;
}

// original 0x4351F0 (PSX 0x801E58C8): the running slot (0x93B8C4) freed -
// bytes +0, +5, +6, +1..+4, +0x48, +0x5D..+0x5F zeroed.
extern "C" void __cdecl BattleTask_FreeCurrent(void) {
    static constexpr unsigned kBytes[] = {0, 5, 6, 1, 2, 3, 4, 0x48, 0x5D, 0x5E, 0x5F};
    for (const unsigned b : kBytes) CurrentTask()[b] = 0;
}

// original 0x435260 (PSX 0x801E5978): the same eleven bytes of all 48 slots.
extern "C" void __cdecl BattleTask_ClearAll(void) {
    static constexpr unsigned kBytes[] = {0, 5, 6, 1, 2, 3, 4, 0x48, 0x5D, 0x5E, 0x5F};
    for (unsigned i = 0; i < at::kTaskCount; ++i)
        for (const unsigned b : kBytes) Task(i)[b] = 0;
}

// ===========================================================================
// The enemy objects
// ===========================================================================

// original 0x435830 (no PSX twin paired): once a frame from the battle's
// frame. Each of the eight enemy objects whose byte +0 is not zero becomes
// Sprite_Current and 0x939AD8, and runs the entry of the state table 0x64B084
// that its byte +0x100 picks. While the byte 0x904AAA is set it instead calls
// its +0xF4 with 2 (when its byte +1 is not zero) and then the entry of
// 0x64B088 - the same table one entry on - picked by +0x100 of the object
// 0x939AD8 holds after that call.
//
// As the original has it: 0x904AAA is read afresh for each object; the
// state is not checked (0x64B088 with state 0 is 0x64B084's second dword, a
// null). The event path re-reads 0x939AD8 after the hook; the other reads
// the loop's own object.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleEnemy_RunAll(void) {
    for (unsigned i = 0; i < at::kEnemyCount; ++i) {
        unsigned char* const e = EnemyObject(i);
        if (e[0] == 0) continue;
        const unsigned char event = At(at::kEventBattle)[0];
        Sprite_Current = e;
        SetPtrAt(at::kEnemyCurrent, e);
        if (event != 0) {
            if (e[1] != 0) reinterpret_cast<HookFn>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(e + 0xF4))))(2);
            const unsigned state = Enemy()[0x100];
            reinterpret_cast<const Handler*>(static_cast<std::uintptr_t>(at::kEnemyStates + 4))[state]();
        } else {
            const unsigned state = e[0x100];
            reinterpret_cast<const Handler*>(static_cast<std::uintptr_t>(at::kEnemyStates))[state]();
        }
    }
}

// original 0x4358A0 (no PSX twin paired): once a frame from the battle's
// frame, after BattleEnemy_RunAll. Each enemy object whose byte +0 is not
// zero becomes Sprite_Current and 0x939AD8 and gets Sprite_UpdateScreen.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleEnemy_UpdateScreenAll(void) {
    for (unsigned i = 0; i < at::kEnemyCount; ++i) {
        unsigned char* const e = EnemyObject(i);
        if (e[0] == 0) continue;
        Sprite_Current = e;
        SetPtrAt(at::kEnemyCurrent, e);
        g.update_screen();
    }
}

// original 0x4358D0 (PSX 0x801E2298): the enemy's animation `animation` -
// the byte of 0x939AD8's table +0xFC at `animation`, or at `animation` + 1
// when Sprite_Current's byte +8 is 2 or 3 - through Sprite_EnsureAnimation
// (its low seven bits), and the sprite's flip byte +0x2A from its bit 7: set
// for modes 0 and 2, inverted for 1 and 3. A mode above 3 does nothing.
//
// As the original has it: the mode is read first; 0x939AD8, its table
// pointer and the byte are read again after the call; the flip goes to the
// Sprite_Current of after the call. The index is the argument's low byte, not
// wrapped when 1 is added.
extern "C" void __cdecl BattleEnemy_SetAnimation(unsigned animation) {
    const unsigned mode = Sprite_Current[8];
    if (mode > 3) return;
    const unsigned index = (animation & 0xFF) + (mode >= 2 ? 1u : 0u);
    const auto table = [] { return PtrAt(static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(Enemy() + 0xFC))); };
    g.ensure_animation(static_cast<unsigned char>(table()[index] & 0x7F));
    const bool high = (table()[index] & 0x80) != 0;
    const bool flip = (mode == 0 || mode == 2) ? high : !high;
    Sprite_Current[0x2A] = flip ? 1 : 0;
}

// original 0x436090 (PSX 0x801E3070, the same): an enemy state's animation
// tick. 1 without a tick while 0x939AD8's status byte +0x92 has 0x04 or
// 0x40, or while 0x904B8E is set and its +0x114 lacks 0x10; else a tail jump
// to Sprite_ScriptTick, whose answer it is.
extern "C" unsigned char __cdecl BattleEnemy_ScriptTick(void) {
    const unsigned char* const e = Enemy();
    if ((e[0x92] & 0x44) != 0) return 1;
    if (At(at::kAnimGate)[0] != 0 && (e[0x114] & 0x10) == 0) return 1;
    return g.script_tick();
}

// original 0x4360C0 (PSX 0x801E30D8, the same): BattleEnemy_ScriptTick with
// Sprite_ScriptTickOnce.
extern "C" unsigned char __cdecl BattleEnemy_ScriptTickOnce(void) {
    const unsigned char* const e = Enemy();
    if ((e[0x92] & 0x44) != 0) return 1;
    if (At(at::kAnimGate)[0] != 0 && (e[0x114] & 0x10) == 0) return 1;
    return g.script_tick_once();
}

// original 0x436B50 (PSX 0x801E42C0, the same gates - the sibling's STEAL.md
// "false lead"): 0 when 0x939AD8's status word +0x92 has any of 0x4064, when
// 0x904B34 is 3 or more, when 0x904B35 is 4 and the word 0x904B80 is 0xA1,
// when 0x904B8E is set and +0x114 lacks 0x10, or when +0x90 lacks bit 1.
// Otherwise 1 when bit 15 of the dword +0x110 is set, else 1 for
// Rand() % 100 below 70 (signed, as idiv). What its callers (0x436AA1,
// 0x436AD1) do with it is unread.
extern "C" unsigned char __cdecl BattleEnemy_Chance70(void) {
    const unsigned char* const e = Enemy();
    if ((Word(e + 0x92) & 0x4064) != 0) return 0;
    if (At(at::kTurnGate)[0] >= 3) return 0;
    if (At(at::kFormation)[0] == 4 && Word(At(at::kMagicId)) == 0xA1) return 0;
    if (At(at::kAnimGate)[0] != 0 && (e[0x114] & 0x10) == 0) return 0;
    if ((e[0x90] & 2) == 0) return 0;
    if ((Long(e + 0x110) & 0x8000) != 0) return 1;
    return g.crt_rand() % 100 < 70 ? 1 : 0;
}

// ===========================================================================
// A kill
// ===========================================================================

// original 0x437470 (PSX Battle_EnemyDefeated 0x801E542C, the same order):
// Sprite_Current's byte +0 |= 0x40; the EXP total 0x904AEC += 0x939AD8's u16
// +0x96, the zenny total 0x904AF0 += its u16 +0x94, which is zeroed; its
// status +0x93 |= 0x40 (0x4000 of the word +0x92, dead);
// Battle_RemoveFromTurnOrder(Sprite_Current +5); Battle_RollDrops; the
// enemies left 0x904AB3 less one, and 0x904AE8 |= 2 at none; with round flag
// 0x4000 set and 0x904B8A equal to the actor, that flag cleared; the enemy's
// flag bit +0x8C set (0x494ED0); Sprite_ReleaseTint(Sprite_Current); bits
// 0x70 of +0x110 cleared; 0x446FD0(actor) clears the actor's bit of 0x904B82;
// Sprite_Current's state bytes +1..+3 to 2, 0, 0.
//
// As the original has it: Sprite_Current and 0x939AD8 are read afresh after
// every call; the actor byte goes to the callees zero-extended (they read a
// byte, or a word for 0x494ED0's flag number).
extern "C" __attribute__((disable_tail_calls)) void __cdecl Battle_EnemyDefeated(void) {
    Sprite_Current[0] = static_cast<unsigned char>(Sprite_Current[0] | 0x40);
    unsigned char* e = Enemy();
    // DIV-0045: the yields times the launcher's multipliers, 1 unless set
    // (src/game/cheats.cpp); the enemy's record keeps its own numbers.
    SetLong(At(at::kExpTotal), static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(At(at::kExpTotal))) + Word(e + 0x96) * Cheats_ExpMultiplier()));
    SetLong(At(at::kZennyTotal), static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(At(at::kZennyTotal))) + Word(e + 0x94) * Cheats_ZennyMultiplier()));
    SetWord(e + 0x94, 0);
    e = Enemy();
    e[0x93] = static_cast<unsigned char>(e[0x93] | 0x40);
    g.remove_from_turn_order(Sprite_Current[5]);
    g.roll_drops();
    const auto left = static_cast<unsigned char>(At(at::kEnemiesLeft)[0] - 1);
    At(at::kEnemiesLeft)[0] = left;
    if (left == 0) At(at::kBattleEnd)[0] = static_cast<unsigned char>(At(at::kBattleEnd)[0] | 2);
    if ((Long(At(at::kRoundFlags)) & 0x4000) != 0 && At(at::kActorAt)[0] == Sprite_Current[5])
        SetWord(At(at::kRoundFlags), Word(At(at::kRoundFlags)) & 0xBFFF);
    g.set_flag_bit(Enemy()[0x8C]);
    g.release_tint(Sprite_Current);
    e = Enemy();
    SetLong(e + 0x110, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(e + 0x110)) & 0xFFFFFF8Fu));
    g.clear_turn_bit(Sprite_Current[5]);
    Sprite_Current[1] = 2;
    Sprite_Current[2] = 0;
    Sprite_Current[3] = 0;
}

// original 0x437580 (PSX Battle_RollDrops 0x801E525C): for each of 0x939AD8's
// two drops - an item u16 at +0xA8 / +0xAC and its class byte at +0xAA /
// +0xAE, both non-zero - a drop when (Rand() & 0xFF) is at most
// {0, 0, 1, 3, 7, 0x1F, 0x7F, 0xFF}[class] (a table on the stack). A drop is
// looked for in the list 0x904AF4 (u16 items, counts at 0x904B14, 0x904AE7
// entries): each entry equal to it counts one more and zeroes the enemy's
// item word. Then - found or not - the enemy's word as it now stands is
// appended with count 1 (the entry count grows while below 15) and the item
// word zeroed.
//
// Not the PSX's: there a found drop is counted and the loop left; the port
// runs on and appends the zeroed word as an item 0 with count 1, and the
// entries after the match are compared with 0 - defect, docs/battle_flow.md
// section 5. Kept, as every latent defect is.
//
// As the original has it: 0x939AD8 is read afresh after Rand, after a match
// and after the append; the class is read after Rand; the entry count is
// read before the search, again after each match, and again for the count
// byte's index and for the increment. A class above 7 would read the
// original's saved registers and caller's stack; ours aborts (rule 4).
extern "C" void __cdecl Battle_RollDrops(void) {
    static constexpr unsigned char kChance[8] = {0, 0, 1, 3, 7, 0x1F, 0x7F, 0xFF};
    unsigned char* e = Enemy();
    for (unsigned slot = 0xA8; slot < 0xB0; slot += 4) {
        if (Word(e + slot) == 0) continue;
        if (e[slot + 2] == 0) continue;
        const unsigned roll = static_cast<unsigned>(g.crt_rand()) & 0xFF;
        e = Enemy();
        const unsigned cls = e[slot + 2];
        if (cls >= 8) bof3::Fatal("Battle_RollDrops: drop class %u, past the eight-entry table", cls);
        if (kChance[cls] < roll) continue;
        unsigned count = At(at::kDropCount)[0];
        if (count != 0) {
            unsigned i = 0;
            do {
                if (Word(At(at::kDropItems + i * 2)) == Word(e + slot)) {
                    At(at::kDropCounts)[i] = static_cast<unsigned char>(At(at::kDropCounts)[i] + 1);
                    SetWord(e + slot, 0);
                    e = Enemy();
                    count = At(at::kDropCount)[0];
                }
                i = (i + 1) & 0xFF;
            } while (i < count);
        }
        SetWord(At(at::kDropItems + count * 2), Word(e + slot));
        At(at::kDropCounts)[At(at::kDropCount)[0]] = 1;
        const unsigned char entries = At(at::kDropCount)[0];
        if (entries < 0xF) At(at::kDropCount)[0] = static_cast<unsigned char>(entries + 1);
        SetWord(e + slot, 0);
        e = Enemy();
    }
}

// ===========================================================================
// Magic: the task and the file
// ===========================================================================

// original 0x437780 (PSX 0x800AB188): an item's magic effect. The id word to
// 0x904B80; a task of kind 2 with the item's magic row; the task's +0x80 =
// `owner`.
//
// As the original has it: the returned index is not tested - 0xFF (every
// slot taken) writes `owner` 0x80 bytes into the 256th slot's place,
// 0x9423FC, past the array.
extern "C" __attribute__((disable_tail_calls)) void __cdecl Battle_StartItemMagic(unsigned id, unsigned owner) {
    SetWord(At(at::kMagicId), id);
    const unsigned index = g.task_create(2, ItemRow(id)) & 0xFF;
    SetLong(Task(index) + 0x80, static_cast<std::int32_t>(owner));
}

// original 0x4377D0 (PSX Magic_LoadForItem 0x800AB1FC): the item's magic
// row's file loaded, unless it is 0xFFFF; then 0x904AA9 |= 4.
extern "C" __attribute__((disable_tail_calls)) void __cdecl Magic_LoadForItem(unsigned id) { LoadMagicRow(ItemRow(id)); }

// original 0x437930 (PSX 0x800AB038): an ability's magic effect. The
// ability byte to the word 0x904B80; a task of kind 2 with the ability's row
// (0x64C1D0), its +0x80 = `owner`; then, if the ability's record (24 bytes
// at 0x65C4D0 + 0xD, by the word 0x904B80 read again) has bit 0x10, a
// second task, kind 1 with parameter 0x43, whose +0x80 is the first task's
// address and +0xB the ability.
//
// As the original has it: neither index is tested (0xFF writes past the
// array, as Battle_StartItemMagic).
extern "C" __attribute__((disable_tail_calls)) void __cdecl Battle_StartAbilityMagic(unsigned ability, unsigned owner) {
    const unsigned a = ability & 0xFF;
    SetWord(At(at::kMagicId), a);
    const unsigned first = g.task_create(2, At(at::kAbilityRows)[a]) & 0xFF;
    SetLong(Task(first) + 0x80, static_cast<std::int32_t>(owner));
    const unsigned id = Word(At(at::kMagicId));
    if ((At(at::kAbilityFlags + id * 24)[0] & 0x10) == 0) return;
    const unsigned second = g.task_create(1, 0x43) & 0xFF;
    SetPtrAt(at::kTasks + second * at::kTaskSize + 0x80, Task(first));
    Task(second)[0xB] = static_cast<unsigned char>(a);
}

// original 0x4379D0 (PSX Magic_LoadForAbility 0x800AB120): the ability's
// row's file loaded, unless it is 0xFFFF; then 0x904AA9 |= 4.
extern "C" __attribute__((disable_tail_calls)) void __cdecl Magic_LoadForAbility(unsigned ability) {
    LoadMagicRow(At(at::kAbilityRows)[ability & 0xFF]);
}

// ===========================================================================
// Digits and labels
// ===========================================================================

// original 0x432F10 (PSX 0x801E61FC): a battle task's rolling digits. The
// running slot's word +0x32 steps on by one and is taken mod 10; then for n
// = the s8 +0xA down to 0: Battle_DrawNumber(x +0x36 - 8n - 12, y +0x3A,
// clut, (+0x32 + n) mod 10).
//
// As the original has it: the slot pointer is read afresh for each store and
// each digit; +0x32 is a s16 and the remainders are C's (a negative phase
// gives a negative digit, which Battle_DrawNumber prints as its u16); the
// count is read once. The original pushes x and y with the quotient's upper
// 16 bits above them; Battle_DrawNumber reads their low words only.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleFx_RollingDigits(unsigned clut) {
    unsigned char* s = CurrentTask();
    SetWord(s + 0x32, Word(s + 0x32) + 1u);
    s = CurrentTask();
    SetWord(s + 0x32, static_cast<unsigned>(static_cast<short>(Word(s + 0x32)) % 10));
    s = CurrentTask();
    auto n = static_cast<signed char>(s[0xA]);
    while (n >= 0) {
        const int digit = (static_cast<short>(Word(s + 0x32)) + n) % 10;
        const int x = static_cast<short>(static_cast<std::uint16_t>(Word(s + 0x36) - n * 8 - 0xC));
        g.draw_number(x, static_cast<short>(Word(s + 0x3A)), clut, static_cast<unsigned>(digit));
        n = static_cast<signed char>(n - 1);
        s = CurrentTask();
    }
}

// original 0x444480 (PSX 0x801D9654): a number at (x, y). The value's u16
// through sprintf("%3d") into 0x904BA0; a draw mode under tpage (0, 0, 0x3C0,
// 0) committed to slot 1; then from the first character to the next NUL,
// each but a space an 8 x 8 SPRT at the pen: the character less '0' stored
// back into the text, CLUT (clut << 4, 0x1E0), shade 0x80, u = (digit +
// 0x16) * 8 as a byte, v 0xD0; committed to slot 1. The pen moves 8 for
// every character, spaces too.
//
// As the original has it: the first character is drawn before any NUL test
// (the loop is a do-while); the packet cursor is read before Gpu_GetClut and
// the digit re-read from the text after it; x and y are the arguments' low
// 16 bits, the pen as 32 bits; the tpage call's texture window 0 is a push
// left under it.
extern "C" __attribute__((disable_tail_calls)) void __cdecl Battle_DrawNumber(int x, int y, unsigned clut, unsigned value) {
    g.crt_sprintf(reinterpret_cast<char*>(At(at::kNumberText)), reinterpret_cast<const char*>(At(at::kNumberFormat)),
              value & 0xFFFF);
    const unsigned tpage = g.get_tpage(0, 0, 0x3C0, 0) & 0xFFFF;
    g.draw_mode(Gfx_PacketNext, 0, 0, tpage, 0);
    g.commit(1, 0xC);
    unsigned i = 0;
    int pen = x;
    unsigned char* c = At(at::kNumberText);
    do {
        if (c[0] != ' ') {
            unsigned char* const p = Gfx_PacketNext;
            c[0] = static_cast<unsigned char>(c[0] - 0x30);
            const unsigned clut_word = g.get_clut(static_cast<int>((clut & 0xFF) << 4), 0x1E0);
            SetWord(p + 0x16, clut_word);
            p[4] = 0x80;
            p[5] = 0x80;
            p[6] = 0x80;
            SetLong(p + 8, FloatOfShort(pen));
            SetLong(p + 0xC, FloatOfShort(y));
            p[0x15] = 0xD0;
            SetWord(p + 0x18, 8);
            p[0x14] = static_cast<unsigned char>((c[0] + 0x16) << 3);
            SetWord(p + 0x1A, 8);
            g.set_sprt(p);
            g.commit(1, 0x1C);
        }
        pen += 8;
        i = (i + 1) & 0xFF;
        c = At(at::kNumberText + i);
    } while (c[0] != 0);
}

// original 0x4445A0 (PSX 0x801D97D4): a 24 x 8 label at (x, y) - cell `cell`
// of the strip at u 0x68, v 0xD0 (u = cell * 24 + 0x68 as a byte), CLUT
// (clut << 4, 0x1E0), shade 0x80 - under the same draw mode as
// Battle_DrawNumber.
extern "C" __attribute__((disable_tail_calls)) void __cdecl Battle_DrawLabel(int x, int y, unsigned clut, unsigned cell) {
    const unsigned tpage = g.get_tpage(0, 0, 0x3C0, 0) & 0xFFFF;
    g.draw_mode(Gfx_PacketNext, 0, 0, tpage, 0);
    g.commit(1, 0xC);
    unsigned char* const p = Gfx_PacketNext;
    const unsigned clut_word = g.get_clut(static_cast<int>((clut & 0xFF) << 4), 0x1E0);
    SetWord(p + 0x16, clut_word);
    p[4] = 0x80;
    p[5] = 0x80;
    p[6] = 0x80;
    SetLong(p + 8, FloatOfShort(x));
    SetLong(p + 0xC, FloatOfShort(y));
    p[0x15] = 0xD0;
    p[0x14] = static_cast<unsigned char>(cell * 0x18 + 0x68);
    SetWord(p + 0x18, 0x18);
    SetWord(p + 0x1A, 8);
    g.set_sprt(p);
    g.commit(1, 0x1C);
}

// ===========================================================================

// original 0x4456C0 (PSX 0x801DB4EC, the sibling's Battle_ActorFrameUpdate
// hypothesis - it is this test): 1 when the actor is out of the fight -
// its object's bit 0 clear, or its status 0x4000 set (party: ObjTrio + 0x14C
// * actor, byte +0x91; enemy: 0x93B960 + 0x128 * (actor - 3), byte +0x93).
//
// As the original has it: the actor is the argument's low byte, and the
// enemy index is not checked.
extern "C" unsigned char __cdecl Battle_ActorIsOut(unsigned actor) {
    const unsigned a = actor & 0xFF;
    if (a < 3) {
        const unsigned char* const m = At(at::kMembers + a * at::kMemberSize);
        if ((m[0] & 1) == 0) return 1;
        return (m[0x91] & 0x40) != 0 ? 1 : 0;
    }
    const unsigned char* const e = EnemyObject((a - 3) & 0xFF);
    if ((e[0] & 1) == 0) return 1;
    return (e[0x93] & 0x40) != 0 ? 1 : 0;
}

// ===========================================================================

void BattleFlow_Inject() {
    if (bof3::WantsShadow("battle_flow")) battle_flow::SelfTest();
    BOF3_INJECT(Battle_PhaseDispatch);
    BOF3_INJECT(BattleTask_RunAll);
    BOF3_INJECT(BattleTask_Create);
    BOF3_INJECT(BattleTask_FreeCurrent);
    BOF3_INJECT(BattleTask_ClearAll);
    BOF3_INJECT(BattleEnemy_RunAll);
    BOF3_INJECT(BattleEnemy_UpdateScreenAll);
    BOF3_INJECT(BattleEnemy_SetAnimation);
    BOF3_INJECT(BattleEnemy_ScriptTick);
    BOF3_INJECT(BattleEnemy_ScriptTickOnce);
    BOF3_INJECT(BattleEnemy_Chance70);
    BOF3_INJECT(Battle_EnemyDefeated);
    BOF3_INJECT(Battle_RollDrops);
    BOF3_INJECT(Battle_StartItemMagic);
    BOF3_INJECT(Magic_LoadForItem);
    BOF3_INJECT(Battle_StartAbilityMagic);
    BOF3_INJECT(Magic_LoadForAbility);
    BOF3_INJECT(BattleFx_RollingDigits);
    BOF3_INJECT(Battle_DrawNumber);
    BOF3_INJECT(Battle_DrawLabel);
    BOF3_INJECT(Battle_ActorIsOut);
}

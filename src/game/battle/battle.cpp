// Moved here by tools/move_funcs.py from battle_flow.cpp, battle_fx_tasks.cpp, rest_3a.cpp (docs/source-layout-round.md).
#include "game/battle/battle.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/battle_flow_callees.h"
#include "game/battle_fx_tasks_callees.h"
#include "game/boss_harness.h"
#include "game/cheats.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_3a_callees.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = rest_3a::at;

}  // namespace

using namespace battle_flow;

using namespace battle_fx_tasks;

namespace battle_flow {

using move_script::At;

using move_script::Long;

using move_script::SetWord;

using move_script::Word;

}  // namespace battle_flow

namespace {

// A pointer the original keeps in a dword of .data / .bss, read afresh.
unsigned char* PtrAtBattleFlow(std::uint32_t address) {
    return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(battle_flow::Long(battle_flow::At(address)))));
}

unsigned char* Task(unsigned index) { return battle_flow::At(::bof3::Img(BattleTask_Slots_addr) + index * battle_flow::at::kTaskSize); }

// The item's magic row: (id >> 8) picks one of the four u8 tables at
// 0x64B274, (id & 0xFF) the byte. As the original has it, the high byte is
// not checked (items use 0..3).
unsigned ItemRow(unsigned id) {
    const unsigned w = id & 0xFFFF;
    return PtrAtBattleFlow(::bof3::Img(battle_flow::at::kItemRows.v) + 4u * (w >> 8))[w & 0xFF];   // four 32-bit table places
}

// The magic file of a row: the u16 at the head of its 8-byte record; 0xFFFF
// is none. A file asked for sets bit 2 of 0x904AA9 (the round flags' 0x400).
void LoadMagicRow(unsigned row) {
    const unsigned file = battle_flow::Word(battle_flow::At(::bof3::Img(Magic_Rows_addr) + row * 8));
    if (file == 0xFFFF) return;
    battle_flow::g.load_dat(static_cast<int>(file));
    battle_flow::At(::bof3::Img(battle_flow::at::kLoadFlags.v))[0] = static_cast<unsigned char>(battle_flow::At(::bof3::Img(battle_flow::at::kLoadFlags.v))[0] | 4);
}

}  // namespace

namespace battle_fx_tasks {

using move_script::At;

using move_script::Long;

}  // namespace battle_fx_tasks

namespace {

// A pointer the original keeps in a dword, read afresh.
unsigned char* PtrAtBattleFxTasks(std::uint32_t address) { return battle_fx_tasks::At(static_cast<std::uint32_t>(battle_fx_tasks::Long(battle_fx_tasks::At(address)))); }

// Sprite_Current, read afresh at every use.
unsigned char* Sc() { return Sprite_Current; }

using U = std::uint32_t;

using move_script::At;

unsigned char& B(U address) { return move_script::At(address)[0]; }

}  // namespace

// original 0x437780 (PSX 0x800AB188): an item's magic effect. The id word to
// 0x904B80; a task of kind 2 with the item's magic row; the task's +0x80 =
// `owner`.
//
// As the original has it: the returned index is not tested - 0xFF (every
// slot taken) writes `owner` 0x80 bytes into the 256th slot's place,
// 0x9423FC, past the array.
extern "C" __attribute__((disable_tail_calls)) void __cdecl Battle_StartItemMagic(unsigned id, unsigned owner) {
    battle_flow::SetWord(battle_flow::At(::bof3::Img(battle_flow::at::kMagicId.v)), id);
    const unsigned index = battle_flow::g.task_create(2, ItemRow(id)) & 0xFF;
    rv::As<records::BattleTask>(Task(index))->owner = static_cast<std::int32_t>(owner);
}

// original 0x4377D0 (PSX Magic_LoadForItem 0x800AB1FC): the item's magic
// row's file loaded, unless it is 0xFFFF; then 0x904AA9 |= 4.
extern "C" __attribute__((disable_tail_calls)) void __cdecl Magic_LoadForItem(unsigned id) { LoadMagicRow(ItemRow(id)); }

// original 0x437820 (TitleFlow_NewGame's call): the seven default records
// (Char_DefaultRecords, 0xA4 bytes each, dword by dword) over CharacterRecords
// 0..6, each followed by one byte derived from its level (+0xA): (level << 4)
// / the pair's divisor (a signed idiv) + its addend, the low byte stored at
// +0x4E, +0x1C and +0x2E; then the whelp's record (0x64B80C) over the record
// Char_WhelpSlot names. DIV-0020's names flow through the records it copies
// (char_names.cpp checks the two instructions that read them, which the
// inject's jmp at 0x437820 leaves in place). Ours aborts on a divisor of 0 and
// on a whelp slot past the eight records (section 7).
extern "C" void __cdecl NewGame_InitCharacters(void) {
    for (unsigned n = 0; n < 7; ++n) {
        unsigned char* const record = move_script::At(::bof3::Img(CharacterRecords_addr) + n * rest_3a::at::kCharSize);
        std::memcpy(record, move_script::At(::bof3::Img(Char_DefaultRecords_addr) + n * rest_3a::at::kCharSize), rest_3a::at::kCharSize);
        const std::int32_t level = record[0xA];
        const unsigned divisor = B(::bof3::Img(rest_3a::at::kLevelDivisors.v) + 2 * n);
        if (divisor == 0) bof3::Fatal("NewGame_InitCharacters: record %u's divisor is 0 - the original faults", n);
        const auto derived = static_cast<unsigned char>((level << 4) / static_cast<std::int32_t>(divisor) + B(::bof3::Img(rest_3a::at::kLevelDivisors.v) + 2 * n + 1));
        record[0x4E] = derived;
        record[0x1C] = derived;
        record[0x2E] = derived;
    }
    const unsigned slot = Char_WhelpSlot;
    if (slot >= rest_3a::at::kCharCount)
        bof3::Fatal("NewGame_InitCharacters: Char_WhelpSlot is %u, past the %u records - the original writes on", slot, rest_3a::at::kCharCount);
    std::memcpy(move_script::At(::bof3::Img(CharacterRecords_addr) + slot * rest_3a::at::kCharSize), move_script::At(::bof3::Img(rest_3a::at::kWhelpRecord.v)), rest_3a::at::kCharSize);
}

// original 0x4378B0 (no PSX twin paired): battle-task kind 2, the task
// Battle_StartItemMagic / Battle_StartAbilityMagic start with a magic row as
// its parameter. While the round flags 0x904AA8 have 0x800, it tail-jumps to
// the code pointer of row Sprite_Current +5 of Magic_Rows (0x64C2B8 + 8 row
// + 4, the dword after the row's DAT file); otherwise it returns.
//
// As the original has it: the row is read through Sprite_Current, not the
// slot pointer, and not checked; ours aborts past row 150 (the table's 151
// rows all hold .text pointers; row 151's is 0x9A1A9B1B, not code).
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleMagicRow_Run(void) {
    if ((static_cast<std::uint32_t>(battle_fx_tasks::Long(battle_fx_tasks::At(::bof3::Img(battle_fx_tasks::at::kRoundFlags.v)))) & 0x800) == 0) return;
    const unsigned row = Sc()[5];
    if (row >= battle_fx_tasks::at::kMagicRowCount) bof3::Fatal("BattleMagicRow_Run: row %u, past Magic_Rows' 151", row);
    reinterpret_cast<battle_fx_tasks::Handler>(PtrAtBattleFxTasks(::bof3::Img(battle_fx_tasks::at::kMagicRows.v) + row * 8 + 4))();
}

void Battle_Inject() {
    BOF3_INJECT(Battle_StartItemMagic);
    BOF3_INJECT(Magic_LoadForItem);
    BOF3_INJECT(NewGame_InitCharacters);
    BOF3_INJECT(BattleMagicRow_Run);
}

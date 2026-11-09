// Moved here by tools/move_funcs.py from battle_flow.cpp (docs/source-layout-round.md).
#include "game/battle/battle_2.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/battle_flow.h"
#include "game/battle_flow_callees.h"
#include "game/cheats.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

using namespace battle_flow;

namespace battle_flow {

using move_script::At;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

}  // namespace battle_flow

namespace {

void SetPtrAtBattleFlow(std::uint32_t address, const unsigned char* p) {
    battle_flow::SetLong(battle_flow::At(address), static_cast<std::int32_t>(Low32(p)));
}

unsigned char* Task(unsigned index) { return battle_flow::At(::bof3::Img(BattleTask_Slots_addr) + index * battle_flow::at::kTaskSize); }

// The magic file of a row: the u16 at the head of its 8-byte record; 0xFFFF
// is none. A file asked for sets bit 2 of 0x904AA9 (the round flags' 0x400).
void LoadMagicRow(unsigned row) {
    const unsigned file = battle_flow::Word(battle_flow::At(::bof3::Img(Magic_Rows_addr) + row * 8));
    if (file == 0xFFFF) return;
    battle_flow::g.load_dat(static_cast<int>(file));
    battle_flow::At(::bof3::Img(battle_flow::at::kLoadFlags.v))[0] = static_cast<unsigned char>(battle_flow::At(::bof3::Img(battle_flow::at::kLoadFlags.v))[0] | 4);
}

}  // namespace

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
    SetWord(At(::bof3::Img(at::kMagicId.v)), a);
    const unsigned first = g.task_create(2, At(::bof3::Img(at::kAbilityRows.v))[a]) & 0xFF;
    rv::As<records::BattleTask>(Task(first))->owner = static_cast<std::int32_t>(owner);
    const unsigned id = Word(At(::bof3::Img(at::kMagicId.v)));
    if ((At(::bof3::Img(at::kAbilityFlags.v) + id * 24)[0] & 0x10) == 0) return;
    const unsigned second = g.task_create(1, 0x43) & 0xFF;
    SetPtrAtBattleFlow(::bof3::Img(at::kTasks.v) + second * at::kTaskSize + 0x80, Task(first));
    Task(second)[0xB] = static_cast<unsigned char>(a);
}

// original 0x4379D0 (PSX Magic_LoadForAbility 0x800AB120): the ability's
// row's file loaded, unless it is 0xFFFF; then 0x904AA9 |= 4.
extern "C" __attribute__((disable_tail_calls)) void __cdecl Magic_LoadForAbility(unsigned ability) {
    LoadMagicRow(At(::bof3::Img(at::kAbilityRows.v))[ability & 0xFF]);
}

void Battle2_Inject() {
    BOF3_INJECT(Battle_StartAbilityMagic);
    BOF3_INJECT(Magic_LoadForAbility);
}

// Moved here by tools/move_funcs.py from magic_fx_reached.cpp, magic_s14.cpp (docs/source-layout-round.md).
#include "game/magic/steal.h"

#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/cheats.h"
#include "game/draw_pool.h"
#include "game/magic_fx_reached_callees.h"
#include "game/magic_harness.h"
#include "game/map_rim.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/records_gpu.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = magic_harness::at;

}  // namespace

using namespace magic_fx_reached;

namespace magic_fx_reached {

using move_script::At;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

namespace {

template <typename T, typename F> T As(F* f) { return reinterpret_cast<T>(reinterpret_cast<void*>(f)); }

}  // namespace

}  // namespace magic_fx_reached

namespace {

unsigned char* Ptr(std::uint32_t cell) {
    return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(magic_fx_reached::Long(magic_fx_reached::At(cell)))));
}

std::int32_t PtrValue(const unsigned char* p) {
    return static_cast<std::int32_t>(Low32(p));
}

// A task slot by its index, unchecked as the originals index it.
unsigned char* Task(unsigned index) { return magic_fx_reached::At(::bof3::Img(BattleTask_Slots_addr) + index * magic_fx_reached::at::kTaskStride); }

unsigned char Byte(std::uint32_t address) { return magic_fx_reached::At(address)[0]; }

// An enemy by the battle index (3 and up), as the originals compute it: the
// index's byte minus 3, times 0x128 - below the records for 0..2.
unsigned char* Enemy(unsigned char index) {
    return At(::bof3::Img(Battle_Enemies_addr) + static_cast<std::uint32_t>((static_cast<int>(index) - 3) * static_cast<int>(at::kEnemyStride)));
}

unsigned char* Party(unsigned char index) { return At(::bof3::Img(ObjTrio_addr) + index * at::kPartyStride); }

// A .data dispatch table's entry, read as the original reads it.
Handler Entry(std::uint32_t table, unsigned index) {
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(magic_fx_reached::Long(magic_fx_reached::At(table + 4 * index)))));
}

// Sprite_Current's position copied from `from`: the actor byte +8 and the
// three dwords +0x34 / +0x38 / +0x3C, Sprite_Current read again for each.
void TakePosition(const unsigned char* from) {
    Sprite_Current[8] = from[8];
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(from)->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(from)->z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(from)->height);
}

void Bump(unsigned char& b, int by) { b = static_cast<unsigned char>(b + by); }

}  // namespace

#define MFX_EXPORT extern "C" __attribute__((disable_tail_calls))

namespace {

// The thief's multiplier by the speed difference (its +0xA8 less the
// enemy's +0xB8): 12 from 49 up, down one at each of 29, 19, 9, -10, -20,
// -30 and -50 - 4 below -50.
int StealMultiplier(int diff) {
    int m = 0xC;
    if (diff < 0x31) m = 0xB;
    if (diff < 0x1D) m = 0xA;
    if (diff < 0x13) m = 9;
    if (diff < 9) m = 8;
    if (diff < -0xA) m = 7;
    if (diff < -0x14) m = 6;
    if (diff < -0x1E) m = 5;
    if (diff < -0x32) m = 4;
    return m;
}

using magic_harness::Mem;

}  // namespace

#define S14_EXPORT extern "C" __attribute__((disable_tail_calls))

// original 0x4B54B0: the steal task. Its phase +1 through a four-entry table
// on the original's stack (Steal_Start, _Wait, _Report, MagicFx_EndWhenIdle);
// 4..255 would call through that stack, ours aborts.
MFX_EXPORT void __cdecl Steal_Task(void) {
    const unsigned phase = Sprite_Current[1];
    if (phase >= 4) bof3::Fatal("Steal_Task: phase %u, past the four-entry table", phase);
    g.steal[phase]();
}

// original 0x4B54F0 (PSX 0x801EEC90's slot): the start. The owner's position;
// +0xB 0, the wait +9 = 0x1E, the phase on; the thief's animation 0xC; the
// thief's double - a kind-1 task (parameter 0x45) owned by this one, the
// first 0x80 bytes of the acting party record copied into it, then its +1 /
// +2 cleared and its kind and parameter set again (the copy overwrote them)
// - counted in +0xB; the owner's +0 bit 6. Then the roll: Rand & 0xFF below
// the enemy's rate (Steal_RateTable by its +0xAA, signed) times the speed
// multiplier steals the enemy's item (+0xA8) through Inventory_Add - message
// 0x38 and the item kept in +0x2C, the enemy's item and rate cleared - or,
// the bag full, 0x39; nothing to steal is 0x3A; a failed roll 0x39, or 0x3A
// when the enemy's rate is 0. The message id goes to +0xA.
//
// This is Pilfer's copy of the routine (the PSX's MAGIC065.EMI; Steal's is
// 0x4F5140, not ours - docs/cheats.md). DIV-0046's cheat lives in its roll:
// see Cheats_PilferRollMask.
//
// As the original has it: the double's index is not tested; the target is
// not checked to be an enemy (0..2 reads below the enemy records); the rate
// row is unbounded; the target byte is read again after Rand and after
// Inventory_Add; Inventory_Add gets a fourth 0 it does not read.
MFX_EXPORT void __cdecl Steal_Start(void) {
    TakePosition(Ptr(::bof3::Img(magic_fx_reached::at::kOwner)));
    Sprite_Current[0xB] = 0;
    Sprite_Current[9] = 0x1E;
    Bump(Sprite_Current[1], 1);
    g.set_animation(0xC, 2);
    const unsigned index = g.task_create(1, 0x45);
    unsigned char* const sc = Sprite_Current;
    unsigned char* const t = Task(index);
    const unsigned char actor = Byte(::bof3::Img(magic_fx_reached::at::kActor));
    SetLong(t + 0x80, PtrValue(sc));
    {
        const unsigned char* const from = Party(actor);
        for (unsigned k = 0; k < 0x80; k += 4) SetLong(t + k, Long(from + k));   // rep movsd, dword by dword
    }
    t[1] = 0;
    t[2] = 0;
    t[6] = 1;
    t[5] = 0x45;
    Bump(sc[0xB], 1);
    unsigned char* const owner = Ptr(::bof3::Img(magic_fx_reached::at::kOwner));
    owner[0] = static_cast<unsigned char>(owner[0] | 0x40);

    const unsigned char* enemy = Enemy(Byte(::bof3::Img(magic_fx_reached::at::kTarget)));
    const int diff = static_cast<int>(Word(Party(Byte(::bof3::Img(magic_fx_reached::at::kActor))) + magic_fx_reached::at::kThiefSpeed)) -
                     static_cast<int>(Word(enemy + magic_fx_reached::at::kEnemySpeed));
    const int multiplier = StealMultiplier(diff);
    const int rate = Steal_RateTable[enemy[magic_fx_reached::at::kStealRate]];
    const int r = g.rand();
    unsigned char* const e = Enemy(Byte(::bof3::Img(magic_fx_reached::at::kTarget)));
    // DIV-0046 (BOF3X_STEAL=1): the mask is 0 when the cheat's patch is in
    // the original's `and eax, 0xFF` at 0x4B5690 - the roll then passes
    // whenever the rate times the multiplier is above 0.
    const std::uint32_t mask = Cheats_PilferRollMask();
    if (static_cast<int>(static_cast<std::uint32_t>(r) & mask) >= static_cast<int>(static_cast<std::uint32_t>(rate) * static_cast<std::uint32_t>(multiplier))) {
        Sprite_Current[0xA] = e[magic_fx_reached::at::kStealRate] != 0 ? 0x39 : 0x3A;
        return;
    }
    const unsigned item = Word(e + magic_fx_reached::at::kStealItem);
    if (item == 0) {
        Sprite_Current[0xA] = 0x3A;
        return;
    }
    if (g.inventory_add(item >> 8, item, 1) == 0) {
        Sprite_Current[0xA] = 0x39;
        return;
    }
    Sprite_Current[0xA] = 0x38;
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->column, item);
    unsigned char* const e2 = Enemy(Byte(::bof3::Img(magic_fx_reached::at::kTarget)));
    e2[magic_fx_reached::at::kStealRate] = 0;
    SetWord(e2 + magic_fx_reached::at::kStealItem, 0);
}

// original 0x4B5770: the wait for the double (+0xB counts it). When it is
// gone: at the first frame (+9 = 0x1E) the thief's animation 4 and the
// owner's +0 bit 6 cleared; +9 down, and at 0 the phase on.
MFX_EXPORT void __cdecl Steal_Wait(void) {
    unsigned char* sc = Sprite_Current;
    if (sc[0xB] != 0) return;
    if (sc[9] == 0x1E) {
        g.set_animation(4, 0);
        unsigned char* const owner = Ptr(::bof3::Img(magic_fx_reached::at::kOwner));
        owner[0] = static_cast<unsigned char>(owner[0] & 0xBF);
        sc = Sprite_Current;
    }
    Bump(sc[9], -1);
    sc = Sprite_Current;
    if (sc[9] == 0) Bump(sc[1], 1);
}

// original 0x4B57C0: the report, once the message window is down
// (0x939F60). A theft first puts the item's name in Text_Records (0x4B58F0,
// index +0x2C, category +0x2D); the system message +0xA is queued
// (BattleQueue_Push(1, 0x1E, text)); the phase on.
//
// As the original has it: the message id travels in the low word of a dword
// whose high word is Sprite_Current's (Msg_SystemPtr reads the word).
MFX_EXPORT void __cdecl Steal_Report(void) {
    if (BattleE1_MessageUp != 0) return;
    const unsigned char* sc = Sprite_Current;
    if (sc[0xA] == 0x38) {
        g.item_name(sc[0x2C], sc[0x2D]);
        sc = Sprite_Current;
    }
    const unsigned char* const text = g.msg_system(sc[0xA]);
    g.queue_push(1, 0x1E, Low32(text));
    Bump(Sprite_Current[1], 1);
}

// original 0x4B5810: the thief's double (kind 1, parameter 0x45): a jmp
// through StealClone_Types 0x65AC28 by +1, read in place, unchecked (two
// entries; +1 stays 0 - Steal_Start clears it and nothing steps it).
MFX_EXPORT void __cdecl StealClone_Dispatch(void) { Entry(::bof3::Img(StealClone_Types_addr), Sprite_Current[1])(); }

// original 0x4B5830: the double's type 0. Its phase +2 through a four-entry
// table on the original's stack (0x4ED5C0 its size, StealClone_Run,
// StealClone_Finish, 0x4AEE90 the free), then while it lives its sprite's
// screen update. 4..255 would call through that stack; ours aborts.
MFX_EXPORT void __cdecl StealClone_Task(void) {
    const unsigned phase = Sprite_Current[2];
    if (phase >= 4) bof3::Fatal("StealClone_Task: phase %u, past the four-entry table", phase);
    g.clone[phase]();
    if (Sprite_Current[0] != 0) g.update_screen();
}

// original 0x4B5880: the double runs its animation once a frame; +9 down,
// and at 0 the thief's sounds (0, 4) and the phase +2 on.
MFX_EXPORT void __cdecl StealClone_Run(void) {
    g.script_tick_once();
    Bump(Sprite_Current[9], -1);
    if (Sprite_Current[9] != 0) return;
    g.play_sound(0, 4);
    Bump(Sprite_Current[2], 1);
}

// original 0x4B58C0: the double's animation to its end; then the target's
// flag 0x40, the owner's count +0xB down and the phase +2 on.
MFX_EXPORT void __cdecl StealClone_Finish(void) {
    if (g.script_tick_once() == 0) return;
    g.set_target_flag40(Byte(::bof3::Img(magic_fx_reached::at::kTarget)));
    Bump(Ptr(::bof3::Img(magic_fx_reached::at::kOwner))[0xB], -1);
    Bump(Sprite_Current[2], 1);
}

// original 0x4B58F0: an item's name into Text_Records (16 bytes, Str_CopyN)
// by its index (low byte) and category (low byte): 1 a weapon (NameTable_
// Weapons, 0x1C a name), 2 armour (NameTable_Armour, 0x1A), 3 an accessory
// (NameTable_Accessories, 0x18), any other a consumable (NameTable_
// Consumables, 0x16). Answers what Str_CopyN answers.
S14_EXPORT char* __cdecl Item_CopyName(unsigned index, unsigned category) {
    const unsigned i = index & 0xFF;
    const unsigned char* name;
    switch (category & 0xFF) {
    case 1: name = Mem(::bof3::Img(bof3::addr::NameTable_Weapons)) + i * 0x1C; break;
    case 2: name = Mem(::bof3::Img(bof3::addr::NameTable_Armour)) + i * 0x1A; break;
    case 3: name = Mem(::bof3::Img(bof3::addr::NameTable_Accessories)) + i * 0x18; break;
    default: name = Mem(::bof3::Img(bof3::addr::NameTable_Consumables)) + i * 0x16; break;
    }
    return MH_CALL(Str_CopyN)(reinterpret_cast<char*>(Mem(::bof3::Img(bof3::addr::Text_Records))), reinterpret_cast<const char*>(name), 0x10);
}

void Steal_Inject() {
    BOF3_INJECT(Steal_Task);
    BOF3_INJECT(Steal_Start);
    BOF3_INJECT(Steal_Wait);
    BOF3_INJECT(Steal_Report);
    BOF3_INJECT(StealClone_Dispatch);
    BOF3_INJECT(StealClone_Task);
    BOF3_INJECT(StealClone_Run);
    BOF3_INJECT(StealClone_Finish);
    BOF3_INJECT(Item_CopyName);
}

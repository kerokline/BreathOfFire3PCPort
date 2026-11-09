// Moved here by tools/move_funcs.py from battle_e2.cpp, battle_flow.cpp, battle_fx_tasks.cpp, effect_4a.cpp, rest_3a.cpp (docs/source-layout-round.md).
#include "game/battle/battle_fx.h"

#include <bit>
#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/battle/battle_fx_callees.h"
#include "game/battle_e2.h"
#include "game/battle_e2_callees.h"
#include "game/battle_flow.h"
#include "game/battle_flow_callees.h"
#include "game/battle_fx_tasks.h"
#include "game/battle_fx_tasks_callees.h"
#include "game/boss_harness.h"
#include "game/cheats.h"
#include "game/effect_4a.h"
#include "game/effect_4a_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_3a.h"
#include "game/rest_3a_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = battle_fx::at;

}  // namespace

using namespace battle_flow;

using namespace battle_fx_tasks;

namespace {

using U = std::uint32_t;

using boss_harness::Phase;

using move_script::At;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

unsigned char& B(U address) { return move_script::At(address)[0]; }

U UL(const unsigned char* p) { return static_cast<U>(move_script::Long(p)); }

U Key(const void* p) { return Low32(p); }

unsigned char* PtrAt(U cell) { return move_script::At(UL(move_script::At(cell))); }

unsigned char* Sc() { return Sprite_Current; }

unsigned char* Owner() { return PtrAt(::bof3::Img(battle_e2::at::kOwner.v)); }

// rv::Current<T>() is typed by who runs: a battle task slot, or the enemy an
// enemy op runs on (docs/battle_e2.md 1.1, 1.3).
// An enemy object and a party member by an actor's index, as the originals
// address them (32-bit wrap: an index below its base reads below the array).
unsigned char* EnemyObj(U index) { return move_script::At(::bof3::Img(Battle_Enemies_addr) + index * battle_e2::at::kEnemyStride); }

unsigned char* Member(U index) { return move_script::At(::bof3::Img(ObjTrio_addr) + index * battle_e2::at::kPartyStride); }

unsigned char* Task(U slot) { return move_script::At(::bof3::Img(BattleTask_Slots_addr) + slot * battle_e2::at::kTaskStride); }

// A party member the restore steps write through (or read from before they
// write): the original indexes the three records by a byte it never checks;
// past 2 it writes into the window records and beyond. Ours aborts.
unsigned char* Rec(const char* who, unsigned index) {
    if (index > 2)
        bof3::Fatal("%s: member %u, past the three party records - the original writes past them (docs/battle_e2.md "
                    "section 7)",
                    who, index);
    return Member(index);
}

// A task slot BattleTask_Create answered: 0xFF (none free) is not tested by
// the original, which then writes past the 48 slots (known-defects D163's
// form). Ours aborts.
unsigned char* NewTask(const char* who, unsigned slot) {
    slot &= 0xFF;
    if (slot >= 48)
        bof3::Fatal("%s: BattleTask_Create answered %u - the original writes past the 48 task slots (docs/battle_e2.md "
                    "section 7)",
                    who, slot);
    return Task(slot);
}

// A stack table of handlers called by a state byte (`call [esp + 4 * byte]`):
// the entry by its address (the fuzz's recorder, or Capcom's jmp to ours), a
// Fatal past the table (the original calls through whatever its frame holds).
void CallState(const char* who, const U* entries, unsigned n, unsigned state) {
    if (state >= n)
        bof3::Fatal("%s: state %u, past the %u entries of its stack table - the original calls through its frame "
                    "(docs/battle_e2.md section 7)",
                    who, state, n);
    Phase(entries[state])();
}

// The animation set pointer 0x9039D8 at the battle's pose pool around a task's
// state call, the field's after (as BattleFx_PoseTask and the actor watch).
void WithBattlePoses(const char* who, const U* entries, unsigned n) {
    auto* const cur_task = rv::Current<records::BattleTask>();   // read before the store, as the original
    BattleE2_AnimSet = ::bof3::Img(battle_e2::at::kAnimSetBattle.v);
    CallState(who, entries, n, cur_task->state);
    BattleE2_AnimSet = ::bof3::Img(battle_e2::at::kAnimSetField.v);
}

// The ten sprite fields the four markers' start states set, Sprite_Current
// read afresh for each: +0x29 1, +0x25 0x1E, +0x26 0, +0x24 0x80, +0x27 0x1B,
// +0x28 1, u16 +0x2C 2, +0x2B 0, +0x2A 0, u32 +0x3C 0.
void MarkerLook() {
    Sc()[0x29] = 1;
    Sc()[0x25] = 0x1E;
    Sc()[0x26] = 0;
    Sc()[0x24] = 0x80;
    rv::Current<records::BattleTask>()->clut = 0x1B;
    Sc()[0x28] = 1;
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->column, 2);
    Sc()[0x2B] = 0;
    Sc()[0x2A] = 0;
    rv::Store<U>(rv::Current<records::BattleTask>()->height, 0);
}

// The grid cell +0xB of window record 21's six columns: x = the record's +4
// dword + 30 (b % 6) + 0x13, y = 32 (b / 6) + its +6 + 0x30 (words stored).
void GridPlace() {
    unsigned char* s = Sprite_Current;
    unsigned b = s[0xB];
    rv::Store<unsigned>(rv::As<records::BattleTask>(s)->screen_x, UL(move_script::At(::bof3::Img(Window21_addr) + 4)) + (b % 6) * 30 + 0x13);
    s = Sprite_Current;
    b = s[0xB];
    rv::Store<unsigned>(rv::As<records::BattleTask>(s)->screen_y, (b / 6) * 32 + WindowRecords[0x15].y + 0x30);
}

// The animation byte of a cursor's grid: the 18 dwords of `grid` as bytes,
// row (+9 plus the record's s16 +0x12) times 4 plus the column +0xA.
unsigned char GridByte(U grid, U window, const unsigned char* s) {
    const std::int32_t row = static_cast<std::int32_t>(s[9]) + static_cast<std::int16_t>(move_script::Word(move_script::At(window + 0x12)));
    return move_script::At(grid + static_cast<U>(row) * 4 + s[0xA])[0];
}

// The restore's common part, from the member's record put back to its
// status tint: the owner `o` is Sprite_Current; the record of its +5 is
// copied from the backup (0x14C bytes, dword by dword); +1..+4 = 6, 4, 4, 1
// (and +8 kept across the copy for slot 12's step), +0x48 = 2, u32 +0x40 =
// 0, +0x130 |= 0x2000; task slot +5 placed at the member's 0x7E06E0 spot on
// the ground (AreaMap_Elevation << 16 into +0x3C); for a character +0x89 7 or
// 0 the kept cells 0x675ECC..0x675ED7 back into +0x9A, +0x90, +0x130, +0x134,
// +0x9E from Field_ActorStates (+0x148's, stride 0xA4), +0x138..+0x13F
// cleared and 0x442310 (BE3's); then 0x453300(+5) (BE6's), the member's window
// (0xD + i) values +0x14 / +0x16 and gauges +0xB / +0xC (55 x HP / max HP, 55
// x AP / max AP), animation +8 + 4, the palette from 0x80D380 + 0x40 x +5,
// Battle_StatusTint(+0x90), Sprite_SetClutStp.
//
// As the original has it: the owner's +5 is re-read before every store
// (the copy can overwrite the owner itself - it is the member's record in
// the game); Sprite_Current is re-read after 0x453300 and after each call.
void RestoreBody(const char* who, unsigned char* o, bool keep8) {
    auto* const owner_rec = rv::As<records::PartyMember>(o);
    auto rec = [&]() { return Rec(who, owner_rec->actor); };
    const unsigned char kept8 = rv::As<records::PartyMember>(rec())->direction;
    {
        unsigned char* const to = rec();
        const unsigned char* const from = move_script::At(::bof3::Img(battle_e2::at::kBackup.v) + static_cast<U>(owner_rec->actor) * battle_e2::at::kPartyStride);
        for (unsigned k = 0; k < battle_e2::at::kPartyStride; k += 4) move_script::SetLong(to + k, move_script::Long(from + k));   // rep movsd, forward
    }
    rv::As<records::PartyMember>(rec())->state = 6;
    rv::As<records::PartyMember>(rec())->step = 4;
    rv::As<records::PartyMember>(rec())->state3 = 4;
    rv::As<records::PartyMember>(rec())->state4 = 1;
    if (keep8) rv::As<records::PartyMember>(rec())->direction = kept8;
    rec()[0x48] = 2;
    rv::Store<U>(rv::As<records::PartyMember>(rec())->scale_x, 0);
    rv::As<records::PartyMember>(rec())->flags_130 = static_cast<U>(rv::As<records::PartyMember>(rec())->flags_130) | 0x2000;
    rv::Store<U>(rv::As<records::BattleTask>(Task(owner_rec->actor))->x, UL(move_script::At(::bof3::Img(battle_e2::at::kMemberPositions.v) + static_cast<U>(owner_rec->actor) * 8)));
    rv::Store<U>(rv::As<records::BattleTask>(Task(owner_rec->actor))->z, UL(move_script::At(::bof3::Img(battle_e2::at::kMemberPositions.v) + 4 + static_cast<U>(owner_rec->actor) * 8)));
    {
        unsigned char* const t = Task(owner_rec->actor);
        auto* const task_rec = rv::As<records::BattleTask>(t);
        const long ground = BH_CALL(AreaMap_Elevation)(task_rec->x, task_rec->z);
        rv::Store<U>(rv::As<records::BattleTask>(Task(owner_rec->actor))->height, static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(ground))) << 16);
    }
    const unsigned char character = rv::As<records::PartyMember>(rec())->char_id;
    if (character == 7 || character == 0) {
        rv::As<records::PartyMember>(rec())->ap = BattleE2_KeptAp;
        rv::Store<unsigned>(rv::As<records::PartyMember>(rec())->status, BattleE2_KeptStatus);
        {
            unsigned char* const r = rec();
            auto* const member_rec = rv::As<records::PartyMember>(r);
            r[0x9E] = move_script::At(::bof3::Img(Field_ActorStates_addr) + 0xe + static_cast<U>(member_rec->char_index) * 0xA4)[0];
        }
        rv::Store<U>(rv::As<records::PartyMember>(rec())->flags_130, BattleE2_KeptFlags);
        rv::Store<U>(rv::As<records::PartyMember>(rec())->flags_134, BattleE2_KeptFlags2);
        for (unsigned k = 1; k <= 8; ++k) rec()[0x137 + k] = 0;
        BH_AT(void (__cdecl*)(), battle_e2::at::kMemberRecalc.v)();
    }
    BH_AT(void (__cdecl*)(unsigned), battle_e2::at::kMemberStatusSet.v)(rv::Current<records::PartyMember>()->actor);
    unsigned char* const s = Sprite_Current;
    auto* const cur_member = rv::As<records::PartyMember>(s);
    {
        unsigned i = cur_member->actor;
        move_script::SetWord(move_script::At(::bof3::Img(WindowRecords_addr) + 0x1e8 + i * 0x24), rv::As<records::PartyMember>(Rec(who, i))->hp);
        i = cur_member->actor;
        move_script::SetWord(move_script::At(::bof3::Img(WindowRecords_addr) + 0x1e8 + 2 + i * 0x24), rv::As<records::PartyMember>(Rec(who, i))->ap);
        i = cur_member->actor;
        const unsigned hp = rv::As<records::PartyMember>(Rec(who, i))->hp, max_hp = rv::As<records::PartyMember>(Rec(who, i))->max_hp;
        if (max_hp == 0) bof3::Fatal("%s: member %u's max HP is 0 - the original divides by it (docs/battle_e2.md section 7)", who, i);
        move_script::At(::bof3::Img(WindowRecords_addr) + 0x1df + i * 0x24)[0] = static_cast<unsigned char>(static_cast<std::int32_t>(55 * hp) / static_cast<std::int32_t>(max_hp));
        i = cur_member->actor;
        const unsigned ap = rv::As<records::PartyMember>(Rec(who, i))->ap, max_ap = rv::As<records::PartyMember>(Rec(who, i))->max_ap;
        if (max_ap == 0) bof3::Fatal("%s: member %u's max AP is 0 - the original divides by it (docs/battle_e2.md section 7)", who, i);
        move_script::At(::bof3::Img(WindowRecords_addr) + 0x1df + 1 + i * 0x24)[0] = static_cast<unsigned char>(static_cast<std::int32_t>(55 * ap) / static_cast<std::int32_t>(max_ap));
    }
    BH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>(cur_member->direction + 4));
    BH_CALL(Sprite_LoadPalette)(reinterpret_cast<unsigned short*>(move_script::At(::bof3::Img(Gfx_ClutStripSource_addr) + 0x1e00 + (static_cast<U>(rv::Current<records::PartyMember>()->actor) << 6))), 0);
    BH_CALL(Battle_StatusTint)(rv::As<records::PartyMember>(Rec(who, rv::Current<records::PartyMember>()->actor))->status);
    BH_CALL(Sprite_SetClutStp)();
}

// The cursors of slots 13 and 14 over window record `window` (18 or 19) and
// its animation grid `grid` (0x904608 or 0x904620).
//
// Start: the marker's look, +0x4B 0xFF, the grid's byte at (+9 + the
// record's s16 +0x12, +0xA) as the animation unless 0xFF, the record's +0x1E
// = its +0x12, +1 on.
void CursorStart(U window, U grid) {
    MarkerLook();
    Sc()[0x4B] = 0xFF;
    unsigned char* s = Sprite_Current;
    const unsigned char a = GridByte(grid, window, s);
    if (a != 0xFF) {
        BH_CALL(Sprite_SetAnimation)(a);
        s = Sprite_Current;
    }
    move_script::SetWord(move_script::At(window + 0x1E), move_script::Word(move_script::At(window + 0x12)));
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// Run: the record not in use, +1 = 2 and +2 = 0; its row base moved (+0x1E
// not +0x12), +1 = 0; else x = its +4 + 22 (+0xA) + 0x44, y = 32 (+9) + its +6
// + 0x22, and unless the grid's byte is 0xFF, Sprite_EnsureAnimation of it,
// the script ticked and the overlay queued.
void CursorRun(U window, U grid) {
    if ((B(window) & 1) == 0) {
        rv::Current<records::BattleTask>()->state = 2;
        Sc()[2] = 0;
        return;
    }
    if (move_script::Word(move_script::At(window + 0x1E)) != move_script::Word(move_script::At(window + 0x12))) {
        rv::Current<records::BattleTask>()->state = 0;
        return;
    }
    unsigned char* s = Sprite_Current;
    rv::Store<unsigned>(rv::As<records::BattleTask>(s)->screen_x, UL(move_script::At(window + 4)) + s[0xA] * 22u + 0x44);
    s = Sprite_Current;
    rv::Store<unsigned>(rv::As<records::BattleTask>(s)->screen_y, s[9] * 32u + move_script::Word(move_script::At(window + 6)) + 0x22);
    s = Sprite_Current;
    const unsigned char a = GridByte(grid, window, s);
    if (a == 0xFF) return;
    BH_CALL(Sprite_EnsureAnimation)(a);
    BH_CALL(Sprite_ScriptTick)();
    BH_CALL(Sprite_QueueOverlay)();
}

}  // namespace

namespace battle_flow {

using move_script::At;

using move_script::Long;

}  // namespace battle_flow

namespace {

// A pointer the original keeps in a dword of .data / .bss, read afresh.
unsigned char* PtrAtBattleFlow(std::uint32_t address) {
    return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(battle_flow::Long(battle_flow::At(address)))));
}

unsigned char* CurrentTask() { return PtrAtBattleFlow(::bof3::Img(BattleFlow_TaskCurrent_addr)); }

}  // namespace

namespace battle_fx_tasks {

using move_script::At;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

}  // namespace battle_fx_tasks

namespace {

std::uint32_t Address(const void* p) { return Low32(p); }

// A pointer the original keeps in a dword, read afresh.
unsigned char* PtrAtBattleFxTasks(std::uint32_t address) { return battle_fx_tasks::At(static_cast<std::uint32_t>(battle_fx_tasks::Long(battle_fx_tasks::At(address)))); }

// The battle-task slot being run (0x93B8C4), and a slot's owner (+0x80).
unsigned char* Cur() { return rv::Bytes(BattleFlow_TaskCurrent.Native()); }

unsigned char* OwnerOf(const unsigned char* slot) { return battle_fx_tasks::At(static_cast<std::uint32_t>(battle_fx_tasks::Long(slot + 0x80))); }

// An actor's flag dword: a member's ObjTrio +0x130, an enemy's +0x110 (its
// low byte's bit 7 is set while a popup of its is up). As the original has
// it, the enemy index (actor - 3) is not checked, and the arithmetic is the
// original's 32-bit wrap.
unsigned char* ActorFlagByte(unsigned actor) {
    if (actor <= 2) return battle_fx_tasks::At(::bof3::Img(ObjTrio_addr) + 0x130 + actor * battle_fx_tasks::at::kMemberSize);
    return battle_fx_tasks::At(::bof3::Img(Battle_Enemies_addr) + 0x110 + (actor - 3u) * battle_fx_tasks::at::kEnemySize);
}

void CallIndex(const battle_fx_tasks::Handler* table, unsigned n, unsigned index, const char* who) {
    if (index >= n) bof3::Fatal("%s: index %u, past the %u-entry table the original builds on its stack", who, index, n);
    table[index]();
}

unsigned char* S() { return Sprite_Current; }

U LRest3a(U address) { return static_cast<U>(move_script::Long(move_script::At(address))); }

unsigned char* PtrAtRest3a(U cell) { return move_script::At(LRest3a(cell)); }

U Addr(const void* p) { return Low32(p); }

unsigned char* Slot() { return rv::Bytes(BattleFlow_TaskCurrent.Native()); }

unsigned char* OwnerRest3a() { return PtrAtRest3a(::bof3::Img(rest_3a::at::kTaskOwner.v)); }

// call [esp + 4 * index] through a table the original builds on its stack:
// the entry by the address the original stores (Phase: Capcom's, or the jmp
// to ours), a Fatal past `n` (the original calls through its own frame above
// the table).
void CallStack(const char* who, const U* table, unsigned n, unsigned index) {
    if (index >= n)
        bof3::Fatal("%s: index %u, past the %u-entry table the original builds on its stack (docs/rest_3a.md section 7)", who,
                    index, n);
    Phase(table[index])();
}

// A party member by its index, for a write through it: past the three the
// original writes into the window records (docs/rest_3a.md section 7).
unsigned char* MemberForWrite(unsigned index, const char* who) {
    if (index > 2) bof3::Fatal("%s: the member index is %u, past ObjTrio's three - the original writes on past them", who, index);
    return move_script::At(::bof3::Img(ObjTrio_addr) + index * rest_3a::at::kMemberSize);
}

// The flash's draw: a draw mode (Gpu_GetTPage(0, 0, 0x3C0, 0); the fifth word
// its caller leaves is Gpu_SetDrawMode's last), committed (1, 0xC); then
// BattleWin_DrawTileRgb(0, 0, 0xB, colour, 1): for a party actor (0x904B34 <
// 3) the slot's word +0x10 times 0x421 (grey: the same five bits in r, g and
// b), else that word << 10 (red). The colour is pushed with a high half the
// callee does not read (bits 0..14 only).
void FlashTile() {
    const unsigned page = BH_CALL(Gpu_GetTPage)(0, 0, 0x3C0, 0);
    BH_CALL(Gpu_SetDrawMode)(PtrAtRest3a(::bof3::Img(0x7E0670)), 0, 0, page & 0xFFFF, 0);
    BH_CALL(Gfx_CommitPrim)(1, 0xC);
    const unsigned char actor = B(::bof3::Img(rest_3a::at::kActor.v));
    unsigned colour;
    if (actor < 3)
        colour = (move_script::Word(Slot() + 0x10) * 0x421u) & 0xFFFF;
    else
        colour = (move_script::Word(Slot() + 0x10) << 10) & 0xFFFF;
    BH_CALL(BattleWin_DrawTileRgb)(0, 0, 0xB, static_cast<int>(colour), 1);
}

// Sprite_Current's +9 into the tint's three bytes (b, g, r as stored), then
// BattleWin_DrawTileTint(0, 0, 0xB).
void TintTile() {
    const unsigned char v = Sc()[9];
    rest_3a::at::kTint.b_02 = v;
    rest_3a::at::kTint.b_01 = v;
    rest_3a::at::kTint.b_00 = v;
    BH_CALL(BattleWin_DrawTileTint)(0, 0, 0xB);
}

}  // namespace

// original 0x432B70 (PSX 0x801E5BE4 by the catalogue's pairing, not read):
// state +1 of the running slot through a five-entry stack table (start,
// hold, rise, bounce, end - 0x432C40 .. 0x432EA0); then, while
// Sprite_Current's bit 0 is set, the slot's face by its mode +7 (a 4-entry
// jump table at 0x432C28):
//   0  a number: during states 0..2 the rolling digits (BattleFx_RollingDigits
//      with the CLUT row +0x27), from state 3 on Battle_DrawNumber(+0x36 - 12,
//      +0x3A, +0x27, the value +0x60);
//   1, 2, 3  a 24 x 8 label: Battle_DrawLabel(+0x36, +0x3A, +0x27, cell 2, 1, 0).
// Modes above 3 draw nothing.
//
// As the original has it: the slot pointer is re-read after the state call
// and Sprite_Current tested then; the state index is not checked (ours
// aborts past 4). The registers it pushes carry the upper bits of whatever
// they held before - the slot's or Sprite_Current's address above a byte or
// a word; the callees read only the byte or the word, and ours pushes the
// same bits.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleFx_DamagePopup(void) {
    CallIndex(battle_fx_tasks::g.popup, 5, rv::As<records::BattleTask>(Cur())->state, "BattleFx_DamagePopup");
    const unsigned char* const sp = Sc();
    const std::uint32_t sc = Address(sp);
    if ((sp[0] & 1) == 0) return;
    unsigned char* const t = Cur();
    auto* const task_rec = rv::As<records::BattleTask>(t);
    const std::uint32_t ta = Address(t);
    switch (task_rec->mode) {
    case 0:
        if (task_rec->state < 3) {
            battle_fx_tasks::g.rolling_digits((ta & 0xFFFFFF00u) | task_rec->clut);
            return;
        }
        battle_fx_tasks::g.draw_number(static_cast<int>((sc & 0xFFFF0000u) | ((task_rec->x_whole - 0xCu) & 0xFFFFu)),
                      static_cast<int>(task_rec->z_whole), (sc & 0xFFFFFF00u) | task_rec->clut, battle_fx_tasks::Word(t + 0x60));
        return;
    case 1:
    case 2:
    case 3:
        battle_fx_tasks::g.draw_label(static_cast<int>((ta & 0xFFFF0000u) | task_rec->x_whole),
                     static_cast<int>((sc & 0xFFFF0000u) | task_rec->z_whole), task_rec->clut, 3u - task_rec->mode);
        return;
    default:
        return;
    }
}

// original 0x432C40 (state 0; inside 0x432B70's catalogue extent, so it was
// never listed on its own): the popup placed over its owner (+0x80) and
// sized.
//   - x +0x36 and y +0x3A: the owner's +0x2E / +0x30 plus a signed offset -
//     for a member (owner +5 <= 2) the s8 pair at 0x64DF70 by (owner +0x2C)
//     * 4 + owner +8; for an enemy its object's s8 +0xF2 / +0xF3.
//   - y 9 higher when +0xB is 1.
//   - the digit count +0xA: 2 for a value (+0x60, s32) of 100 or more, 1 for
//     10 or more, else 0 (the original divides by 100, then by 10, signed).
//   - the owner's flag byte (a member's +0x130, an enemy's +0x110) |= 0x80.
//   - the timer +9 = 8, the digit phase +0x32 = 0, the state + 1.
//
// As the original has it: the slot and the owner are re-read between x and
// y, the enemy index is not checked, and the count is stored through the
// slot pointer read before the owner's flag.
extern "C" void __cdecl BattleFx_PopupStart(void) {
    unsigned char* s = Cur();
    unsigned char* o = OwnerOf(s);
    unsigned actor = o[5];
    if (actor <= 2) {
        std::uint32_t k = battle_fx_tasks::Word(o + 0x2C) * 4u + o[8];
        rv::Store<unsigned>(rv::As<records::BattleTask>(s)->x_whole, static_cast<unsigned>(static_cast<signed char>(battle_fx_tasks::At(::bof3::Img(BattleWin_MemberTargetOffsets_addr) + k * 2)[0])) + battle_fx_tasks::Word(o + 0x2E));
        s = Cur();
        o = OwnerOf(s);
        k = battle_fx_tasks::Word(o + 0x2C) * 4u + o[8];
        rv::Store<unsigned>(rv::As<records::BattleTask>(s)->z_whole, static_cast<unsigned>(static_cast<signed char>(battle_fx_tasks::At(::bof3::Img(BattleWin_MemberTargetOffsets_addr) + 1 + k * 2)[0])) + battle_fx_tasks::Word(o + 0x30));
    } else {
        rv::Store<unsigned>(rv::As<records::BattleTask>(s)->x_whole, static_cast<unsigned>(static_cast<signed char>(battle_fx_tasks::At(::bof3::Img(Battle_Enemies_addr) + 0xF2 + (actor - 3u) * battle_fx_tasks::at::kEnemySize)[0])) +
                              battle_fx_tasks::Word(o + 0x2E));
        s = Cur();
        o = OwnerOf(s);
        actor = o[5];
        rv::Store<unsigned>(rv::As<records::BattleTask>(s)->z_whole, static_cast<unsigned>(static_cast<signed char>(battle_fx_tasks::At(::bof3::Img(Battle_Enemies_addr) + 0xF3 + (actor - 3u) * battle_fx_tasks::at::kEnemySize)[0])) +
                              battle_fx_tasks::Word(o + 0x30));
    }
    unsigned char* d = Cur();
    records::BattleTask* task_d = rv::As<records::BattleTask>(d);
    if (d[0xB] == 1) {
        rv::Store<unsigned>(task_d->z_whole, task_d->z_whole - 9u);
        d = Cur();
        task_d = rv::As<records::BattleTask>(d);
    }
    const std::int32_t value = task_d->value;
    const unsigned char digits = value / 100 > 0 ? 2 : value / 10 > 0 ? 1 : 0;
    unsigned char* const flag = ActorFlagByte(OwnerOf(d)[5]);
    flag[0] = static_cast<unsigned char>(flag[0] | 0x80);
    task_d->digits = digits;
    rv::As<records::BattleTask>(Cur())->timer = 8;
    rv::As<records::BattleTask>(Cur())->digit_phase = 0;
    rv::As<records::BattleTask>(Cur())->state = static_cast<unsigned char>(rv::As<records::BattleTask>(Cur())->state + 1);
}

// original 0x432DB0 (state 1): the timer +9 counts down; on the frame it is
// found 0 (it wraps to 0xFF first) the state + 1 and the timer 4.
extern "C" void __cdecl BattleFx_PopupHold(void) {
    auto* const task_rec = rv::As<records::BattleTask>(Cur());
    const unsigned char timer = task_rec->timer;
    task_rec->timer = static_cast<unsigned char>(timer - 1);
    if (timer != 0) return;
    rv::As<records::BattleTask>(Cur())->state = static_cast<unsigned char>(rv::As<records::BattleTask>(Cur())->state + 1);
    rv::As<records::BattleTask>(Cur())->timer = 4;
}

// original 0x432DE0 (state 2): the dword +0x38 + 0x40000 (y +0x3A down 4)
// every frame; the timer as the hold's, and when it was 0: the state + 1,
// the velocity +0xC = 0x8000 (x, 16.16) and +0x10 = -6.0 (y), and the start
// +0x1C / +0x18 = the s16 y / x.
extern "C" void __cdecl BattleFx_PopupRise(void) {
    unsigned char* s = Cur();
    records::BattleTask* task_rec = rv::As<records::BattleTask>(s);
    task_rec->z = static_cast<std::int32_t>(static_cast<std::uint32_t>(task_rec->z) + 0x40000u);
    s = Cur();
    task_rec = rv::As<records::BattleTask>(s);
    const unsigned char timer = task_rec->timer;
    task_rec->timer = static_cast<unsigned char>(timer - 1);
    if (timer != 0) return;
    rv::As<records::BattleTask>(Cur())->state = static_cast<unsigned char>(rv::As<records::BattleTask>(Cur())->state + 1);
    rv::As<records::BattleTask>(Cur())->step_x = 0x8000;
    rv::As<records::BattleTask>(Cur())->step_z = static_cast<std::int32_t>(0xFFFA0000u);
    s = Cur();
    task_rec = rv::As<records::BattleTask>(s);
    task_rec->work_1c = static_cast<short>(task_rec->z_whole);
    s = Cur();
    task_rec = rv::As<records::BattleTask>(s);
    task_rec->work_18 = static_cast<short>(task_rec->x_whole);
}

// original 0x432E50 (state 3): the bounce. It ends - timer 10, state + 1 -
// once y +0x3A is back to or below its start (+0x1C <= the s16 y) while x has
// moved (+0x18 != the s16 x); otherwise the 16.16 position +0x34 / +0x38
// moves by the velocity +0xC / +0x10, and +0x10 gains 0x14000 (1.25).
//
// As the original has it: the three adds are 32-bit and wrap; the slot is
// re-read before the second and the third.
extern "C" void __cdecl BattleFx_PopupBounce(void) {
    unsigned char* s = Cur();
    records::BattleTask* task_rec = rv::As<records::BattleTask>(s);
    if (!(task_rec->work_1c > static_cast<short>(task_rec->z_whole)) && task_rec->work_18 != static_cast<short>(task_rec->x_whole)) {
        task_rec->timer = 0xA;
        rv::As<records::BattleTask>(Cur())->state = static_cast<unsigned char>(rv::As<records::BattleTask>(Cur())->state + 1);
        return;
    }
    task_rec->x = static_cast<std::int32_t>(static_cast<std::uint32_t>(task_rec->x) + static_cast<std::uint32_t>(task_rec->step_x));
    s = Cur();
    task_rec = rv::As<records::BattleTask>(s);
    task_rec->z = static_cast<std::int32_t>(static_cast<std::uint32_t>(task_rec->z) + static_cast<std::uint32_t>(task_rec->step_z));
    s = Cur();
    task_rec = rv::As<records::BattleTask>(s);
    task_rec->step_z = static_cast<std::int32_t>(static_cast<std::uint32_t>(task_rec->step_z) + 0x14000u);
}

// original 0x432EA0 (state 4): the timer as the hold's; when it was 0 the
// owner's flag byte loses 0x80 again and the slot is freed
// (BattleTask_FreeCurrent, tail-jumped to).
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleFx_PopupEnd(void) {
    auto* const task_rec = rv::As<records::BattleTask>(Cur());
    const unsigned char timer = task_rec->timer;
    task_rec->timer = static_cast<unsigned char>(timer - 1);
    if (timer != 0) return;
    unsigned char* const flag = ActorFlagByte(OwnerOf(Cur())[5]);
    flag[0] = static_cast<unsigned char>(flag[0] & 0x7F);
    battle_fx_tasks::g.free_current();
}

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
    records::BattleTask* task_rec = rv::As<records::BattleTask>(s);
    task_rec->digit_phase = task_rec->digit_phase + 1u;
    s = CurrentTask();
    task_rec = rv::As<records::BattleTask>(s);
    task_rec->digit_phase = static_cast<unsigned>(static_cast<short>(task_rec->digit_phase) % 10);
    s = CurrentTask();
    task_rec = rv::As<records::BattleTask>(s);
    auto n = static_cast<signed char>(task_rec->digits);
    while (n >= 0) {
        const int digit = (static_cast<short>(task_rec->digit_phase) + n) % 10;
        const int x = static_cast<short>(static_cast<std::uint16_t>(task_rec->x_whole - n * 8 - 0xC));
        battle_flow::g.draw_number(x, static_cast<short>(task_rec->z_whole), clut, static_cast<unsigned>(digit));
        n = static_cast<signed char>(n - 1);
        s = CurrentTask();
        task_rec = rv::As<records::BattleTask>(s);
    }
}

// original 0x432F90 (BattleFx_Dispatch's slot 2): by the running slot's +1
// through a table it builds on its stack: BattleFxFlash_Banner,
// BattleFxFlash_Rise, BattleFxFlash_Fall.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleFxFlash_Dispatch(void) {
    static const U kTable[] = {bof3::addr::BattleFxFlash_Banner, bof3::addr::BattleFxFlash_Rise, bof3::addr::BattleFxFlash_Fall};
    CallStack("BattleFxFlash_Dispatch", kTable, 3, Slot()[1]);
}

// original 0x432FC0 (state 0): BattleBanner_Add(1, 0, 0, 0x1E, the string at
// 0x669DFC for a party actor, 0x669E00 for an enemy); window 4's +3 = 1; the
// slot's +0xC = 0x100, +0x10 = 0, +9 = 2, +1 up (the slot read again each).
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleFxFlash_Banner(void) {
    const U text = B(::bof3::Img(rest_3a::at::kActor.v)) < 3 ? Rest3a_BannerParty : Rest3a_BannerEnemy;
    BH_CALL(BattleBanner_Add)(1, 0, 0, 0x1E, reinterpret_cast<const char*>(static_cast<std::uintptr_t>(text)));
    unsigned char* const s = Slot();
    B(::bof3::Img(0x8031F3)) = 1;
    move_script::SetLong(s + 0xC, 0x100);
    move_script::SetLong(Slot() + 0x10, 0);
    Slot()[9] = 2;
    unsigned char* const t = Slot();
    t[1] = static_cast<unsigned char>(t[1] + 1);
}

// original 0x433020 (state 1): Camera_Distance down by the slot's word +0xC;
// the flash's tile; the slot's +0x10 up 8; MapView_Redraw = 2; +9 counted
// down, at 0 +1 up and +9 = 2.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleFxFlash_Rise(void) {
    const unsigned zoom = move_script::Word(Slot() + 0xC);
    Camera_Distance = static_cast<std::uint16_t>(Camera_Distance) - zoom;
    FlashTile();
    unsigned char* s = Slot();
    move_script::SetLong(s + 0x10, static_cast<std::int32_t>(LRest3a(Addr(s + 0x10)) + 8));
    s = Slot();
    MapView_Redraw = 2;
    if (s[9] != 0) {
        s[9] = static_cast<unsigned char>(s[9] - 1);
        return;
    }
    s[1] = static_cast<unsigned char>(s[1] + 1);
    Slot()[9] = 2;
}

// original 0x4330E0 (state 2): Camera_Distance up by +0xC; the tile; +0x10
// down 8; MapView_Redraw = 2; +9 counted down, at 0 a tail jmp to
// BattleTask_FreeCurrent.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleFxFlash_Fall(void) {
    const unsigned zoom = move_script::Word(Slot() + 0xC);
    Camera_Distance = static_cast<std::uint16_t>(Camera_Distance) + zoom;
    FlashTile();
    unsigned char* s = Slot();
    move_script::SetLong(s + 0x10, static_cast<std::int32_t>(LRest3a(Addr(s + 0x10)) - 8));
    s = Slot();
    MapView_Redraw = 2;
    if (s[9] != 0) {
        s[9] = static_cast<unsigned char>(s[9] - 1);
        return;
    }
    BH_CALL(BattleTask_FreeCurrent)();
}

// original 0x433190 (no PSX twin paired): the sprite animation set pointer
// 0x9039D8 = 0x8C5D80 around state Sprite_Current +1 of a two-entry stack
// table (0x4331D0, 0x433290), and 0x8B3580 after it.
//
// As the original has it: the pointer is set before the index is read and
// put back unconditionally; the index is not checked (ours aborts past 1).
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleFx_PoseTask(void) {
    auto* const task_rec = rv::As<records::BattleTask>(Sc());
    BattleE2_AnimSet = static_cast<std::int32_t>(::bof3::Img(battle_fx_tasks::at::kAnimSetBattle.v));
    CallIndex(battle_fx_tasks::g.pose, 2, task_rec->state, "BattleFx_PoseTask");
    BattleE2_AnimSet = static_cast<std::int32_t>(::bof3::Img(battle_fx_tasks::at::kAnimSetField.v));
}

// original 0x4331D0 (state 0): Sprite_Current's draw bytes - +0x29 = 0, +0x25
// = 0x1D, +0x26 = 0, +0x24 = 0x84, the CLUT row +0x27 = byte 0x64B058 [mode
// +7] - 0x50, +0x28 = 0, the word +0x2C = 0, +0x2B = 0; the flip +0x2A = 1
// when (mode 6: +8 odd; any other mode: +8 is 1 or 2), else 0; the state + 1;
// then Sprite_SetAnimation(byte 0x64B064 [mode * 2 + (+8 >> 1)]) and
// Sprite_QueueOverlay (tail-jumped to).
//
// As the original has it: Sprite_Current is re-read for every store; neither
// table index is checked (both tables are constant data in the image).
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleFx_PoseStart(void) {
    Sc()[0x29] = 0;
    Sc()[0x25] = 0x1D;
    Sc()[0x26] = 0;
    Sc()[0x24] = 0x84;
    {
        unsigned char* const sc = Sc();
        auto* const task_rec = rv::As<records::BattleTask>(sc);
        task_rec->clut = static_cast<unsigned char>(battle_fx_tasks::At(::bof3::Img(battle_fx_tasks::at::kPoseClut.v) + sc[7])[0] - 0x50);
    }
    Sc()[0x28] = 0;
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->column, 0);
    Sc()[0x2B] = 0;
    {
        unsigned char* const sc = Sc();
        const unsigned mode = sc[7], b = sc[8];
        if (mode == 6) sc[0x2A] = (b & 1) != 0 ? 1 : 0;
        else sc[0x2A] = b == 1 || b == 2 ? 1 : 0;
    }
    rv::As<records::BattleTask>(Sc())->state = static_cast<unsigned char>(rv::As<records::BattleTask>(Sc())->state + 1);
    unsigned char* const sc = Sc();
    battle_fx_tasks::g.set_animation(battle_fx_tasks::At(::bof3::Img(battle_fx_tasks::at::kPoseAnim.v) + (sc[8] >> 1) + sc[7] * 2u)[0]);
    battle_fx_tasks::g.queue_overlay();
}

// original 0x433290 (state 1): Sprite_ScriptTick; the slot freed when it
// answers non-zero (the animation ended), else Sprite_QueueOverlay - each
// tail-jumped to.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleFx_PosePlay(void) {
    if (battle_fx_tasks::g.script_tick() != 0) battle_fx_tasks::g.free_current();
    else battle_fx_tasks::g.queue_overlay();
}

// original 0x4332B0 (BattleFx_Dispatch's slot 4): by the running slot's +1:
// BattleFx_StepReset, BattleFxTint_Brighten, BattleFxTint_Hold.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleFxTint_Dispatch(void) {
    static const U kTable[] = {bof3::addr::BattleFx_StepReset, bof3::addr::BattleFxTint_Brighten, bof3::addr::BattleFxTint_Hold};
    CallStack("BattleFxTint_Dispatch", kTable, 3, Slot()[1]);
}

// original 0x4332E0: Sprite_Current's timer +9 = 0 and its state + 1. State
// 0 of 0x4332B0's stack table (kind 0, effect 4), and entry 0 of the .data
// state table at 0x65C3A0 whose other three entries are 0x4FB010, 0x4FB050,
// 0x4FB070 (group CJ's magic effect).
extern "C" void __cdecl BattleFx_StepReset(void) {
    Sc()[9] = 0;
    rv::As<records::BattleTask>(Sc())->state = static_cast<unsigned char>(rv::As<records::BattleTask>(Sc())->state + 1);
}

// original 0x433300 (state 1): the tile at +9's shade; +9 up by one, and at
// 0xFF +1 up (Sprite_Current read again).
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleFxTint_Brighten(void) {
    TintTile();
    unsigned char* s = Sc();
    s[9] = static_cast<unsigned char>(s[9] + 1);
    s = Sc();
    if (s[9] == 0xFF) s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x433350 (state 2): the tile at +9's shade, held.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleFxTint_Hold(void) { TintTile(); }

// original 0x433380 (BattleFx_Dispatch's slot 5): by Sprite_Current's +1:
// BattleFxAnim_Start, BattleFxAnim_Run, BattleFxAnim_Linger; then, while
// Sprite_Current's +0 has bit 0, Sprite_QueueOverlay.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleFxAnim_Dispatch(void) {
    static const U kTable[] = {bof3::addr::BattleFxAnim_Start, bof3::addr::BattleFxAnim_Run, bof3::addr::BattleFxAnim_Linger};
    CallStack("BattleFxAnim_Dispatch", kTable, 3, Sc()[1]);
    if (Sc()[0] & 1) BH_CALL(Sprite_QueueOverlay)();
}

// original 0x4333C0 (state 0): Sprite_SetAnimationBank(0x18); +0x24 = 0x80,
// +0x2A = 0; animation 0 when the action's ability (the word +2 of the action
// record 0x904B40) has bit 1 in its record's byte +0x15, else 2; +1 up. The
// ability id indexes Ability_Records unchecked (kept: a read).
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleFxAnim_Start(void) {
    BH_CALL(Sprite_SetAnimationBank)(0x18);
    Sc()[0x24] = 0x80;
    Sc()[0x2A] = 0;
    const unsigned id = rv::As<records::ActionRecord>(PtrAtRest3a(::bof3::Img(rest_3a::at::kAction.v)))->action_id;
    const unsigned char animation = (B(::bof3::Img(rest_3a::at::kAbilityFlags.v) + id * 24) & 2) ? 0 : 2;
    BH_CALL(Sprite_SetAnimation)(animation);
    unsigned char* const s = Sc();
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x433410 (state 1): once Sprite_ScriptTick answers (al) not 0, +1
// up and +9 = 0x10.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleFxAnim_Run(void) {
    if (BH_CALL(Sprite_ScriptTick)() == 0) return;
    unsigned char* const s = Sc();
    s[1] = static_cast<unsigned char>(s[1] + 1);
    Sc()[9] = 0x10;
}

// original 0x433430 (state 2): Sprite_ScriptTickOnce; +9 down by one; at 0 the
// round flags |= 0x20 and a tail jmp to BattleTask_FreeCurrent.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleFxAnim_Linger(void) {
    BH_CALL(Sprite_ScriptTickOnce)();
    unsigned char* const s = Sc();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    if (Sc()[9] != 0) return;
    B(::bof3::Img(rest_3a::at::kFlags.v)) = static_cast<unsigned char>(B(::bof3::Img(rest_3a::at::kFlags.v)) | 0x20);
    BH_CALL(BattleTask_FreeCurrent)();
}

// original 0x433460 (no PSX twin paired): nothing in phase 5 (byte
// 0x904AA0); otherwise state Sprite_Current +1 of a five-entry stack table -
// 0x4334C0, then 0x433550, Sprite_StateRestart 0x433640 (E4A's), BattleFx_WatchIcon 0x433650 (group BE2's),
// 0x433790 (unread) - with the
// animation set pointer 0x9039D8 at 0x8C5D80 around it, as the pose task.
//
// As the original has it: the phase is read before Sprite_Current; the
// index is not checked (ours aborts past 4).
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleFx_ActorWatch(void) {
    if (battle_fx_tasks::At(::bof3::Img(battle_fx_tasks::at::kPhase.v))[0] == 5) return;
    auto* const task_rec = rv::As<records::BattleTask>(Sc());
    BattleE2_AnimSet = static_cast<std::int32_t>(::bof3::Img(battle_fx_tasks::at::kAnimSetBattle.v));
    CallIndex(battle_fx_tasks::g.watch, 5, task_rec->state, "BattleFx_ActorWatch");
    BattleE2_AnimSet = static_cast<std::int32_t>(::bof3::Img(battle_fx_tasks::at::kAnimSetField.v));
}

// original 0x4334C0 (state 0): waits on its owner's actor (the owner
// 0x93B940's +5). Nothing while the round flags have 0x400 and the actor is
// byte 0x904B34, or while Battle_ActorIsOut(actor); otherwise the state + 1
// once the actor's status byte (a member's ObjTrio +0x90, an enemy's +0x92)
// has any of 0x58.
//
// As the original has it: the owner pointer is read before the flags test
// and again after Battle_ActorIsOut, and the actor is re-read from it then -
// the branch (member or enemy) is the one taken before the call, and the
// enemy index is not checked. Battle_ActorIsOut is handed the owner's
// address with the actor in its low byte; it reads the byte.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleFx_ActorWatchTest(void) {
    const std::uint32_t flags = static_cast<std::uint32_t>(battle_fx_tasks::Long(battle_fx_tasks::At(::bof3::Img(battle_fx_tasks::at::kRoundFlags.v))));
    const unsigned char* o = PtrAtBattleFxTasks(::bof3::Img(battle_fx_tasks::at::kTaskOwner.v));
    if ((flags & 0x400) != 0 && o[5] == battle_fx_tasks::At(::bof3::Img(battle_fx_tasks::at::kTurnGate.v))[0]) return;
    const unsigned actor = o[5];
    const unsigned arg = (Address(o) & 0xFFFFFF00u) | actor;
    if (actor <= 2) {
        if (battle_fx_tasks::g.actor_is_out(arg) != 0) return;
        const unsigned now = PtrAtBattleFxTasks(::bof3::Img(battle_fx_tasks::at::kTaskOwner.v))[5];
        if ((battle_fx_tasks::At(::bof3::Img(ObjTrio_addr) + 0x90 + now * battle_fx_tasks::at::kMemberSize)[0] & 0x58) == 0) return;
    } else {
        if (battle_fx_tasks::g.actor_is_out(arg) != 0) return;
        const unsigned now = PtrAtBattleFxTasks(::bof3::Img(battle_fx_tasks::at::kTaskOwner.v))[5];
        if ((battle_fx_tasks::At(::bof3::Img(Battle_Enemies_addr) + 0x92 + (now - 3u) * battle_fx_tasks::at::kEnemySize)[0] & 0x58) == 0) return;
    }
    rv::As<records::BattleTask>(Sc())->state = static_cast<unsigned char>(rv::As<records::BattleTask>(Sc())->state + 1);
}

// original 0x433550 (state 1): +0xB = 0; i = BattleFx_NextStatusIcon (al);
// unless i is 0xFF and its animation 0x64B048[i] is 0xFF: +0xB = i, the icon
// sprite's fields (+0x29 2, +0x25 5, +0x26 0xF0, +0x24 0x80, +0x27 0xFF, +0x28
// 0, word +0x2C 1, +0x2B 0, +0x48 0, +0x44 and +0x40 0x10000, +0xC 0, +0x18
// 0x666, +0xA 0x3C), BattleFx_PlaceOverOwner, Sprite_SetAnimation(the byte
// read again), +1 up by 2. The table has 16 entries; ours aborts past them.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleFx_WatchIconStart(void) {
    Sc()[0xB] = 0;
    const unsigned char icon = BH_CALL(BattleFx_NextStatusIcon)();
    if (icon == 0xFF) return;
    if (icon >= rest_3a::at::kStatusIconCount)
        bof3::Fatal("BattleFx_WatchIconStart: icon %u, past the %u-entry table 0x%X (docs/rest_3a.md section 7)", icon,
                    rest_3a::at::kStatusIconCount, (unsigned)rest_3a::at::kStatusIcons.v);
    if (B(::bof3::Img(rest_3a::at::kStatusIcons.v) + icon) == 0xFF) return;
    Sc()[0xB] = icon;
    Sc()[0x29] = 2;
    Sc()[0x25] = 5;
    Sc()[0x26] = 0xF0;
    Sc()[0x24] = 0x80;
    Sc()[0x27] = 0xFF;
    Sc()[0x28] = 0;
    rv::Store<std::uint32_t>(rv::As<records::BattleTask>(Sc())->column, 1);
    Sc()[0x2B] = 0;
    Sc()[0x48] = 0;
    rv::Store<std::int32_t>(rv::As<records::BattleTask>(Sc())->scale_y, 0x10000);
    rv::Store<std::int32_t>(rv::As<records::BattleTask>(Sc())->scale_x, 0x10000);
    move_script::SetLong(Sc() + 0xC, 0);
    rv::Store<std::int32_t>(rv::As<records::BattleTask>(Sc())->work_18, 0x666);
    Sc()[0xA] = 0x3C;
    BH_CALL(BattleFx_PlaceOverOwner)();
    BH_CALL(Sprite_SetAnimation)(B(::bof3::Img(rest_3a::at::kStatusIcons.v) + icon));
    unsigned char* const s = Sc();
    s[1] = static_cast<unsigned char>(s[1] + 2);
}

// original 0x433640 (EffectKind83_States[12], and the stack table of the
// battle's BattleFx_ActorWatch 0x433460, its state 2; hidden in 0x432F10): +1 0.
extern "C" void __cdecl Sprite_StateRestart(void) { S()[1] = 0; }

// original 0x433650 (PSX 0x801E682C): the actor watch's state 3. While the
// round flags have 0x400: state + 1 when the owner's actor is 0x904B34, or
// the action kind is 4 with the ability 0x97 or 0xE1. Otherwise every 0x3C
// frames (+0xA counting down) the next status icon
// (BattleFx_NextStatusIcon: 0xFF frees the slot; else +0x27 0xFF, +0xA 0x3C,
// and a new icon's animation from 0x64B048 into +0xB); the state back to 0
// once the owner's status has none of 0x58; placed over the owner, the
// script ticked, the overlay queued; the state 0 too when the owner's actor
// is out and its +0 bit 0 is clear.
//
// As the original has it: Sprite_Current is re-read after each call and the
// owner at each use; the icon test reads the owner's actor afresh; an
// enemy's index is not checked.
extern "C" void __cdecl BattleFx_WatchIcon(void) {
    if ((UL(move_script::At(::bof3::Img(battle_e2::at::kRoundFlags.v))) & 0x400) != 0) {
        if (Owner()[5] == B(::bof3::Img(battle_e2::at::kActor.v))) {
            ++rv::Current<records::BattleTask>()->state;
            return;
        }
        if (B(::bof3::Img(battle_e2::at::kActKind.v)) == 4) {
            const unsigned id = move_script::Word(move_script::At(::bof3::Img(battle_e2::at::kMagicId.v)));
            if (id == 0x97 || id == 0xE1) {
                ++rv::Current<records::BattleTask>()->state;
                return;
            }
        }
    }
    unsigned char* const s = Sprite_Current;
    if (s[0xA] != 0) {
        s[0xA] = static_cast<unsigned char>(s[0xA] - 1);
    } else {
        const unsigned char icon = BH_CALL(BattleFx_NextStatusIcon)();
        if (icon == 0xFF) {
            BH_CALL(BattleTask_FreeCurrent)();
            return;
        }
        rv::Current<records::BattleTask>()->clut = 0xFF;
        Sc()[0xA] = 0x3C;
        if (Sc()[0xB] != icon) {
            BH_CALL(Sprite_SetAnimation)(move_script::At(::bof3::Img(battle_e2::at::kStatusIcons.v) + icon)[0]);
            Sc()[0xB] = icon;
        }
    }
    const unsigned actor = Owner()[5];
    const bool shown = actor <= 2 ? (Member(actor)[0x90] & 0x58) != 0 : (EnemyObj(actor - 3)[0x92] & 0x58) != 0;
    if (!shown) rv::Current<records::BattleTask>()->state = 0;
    BH_CALL(BattleFx_PlaceOverOwner)();
    BH_CALL(Sprite_ScriptTick)();
    BH_CALL(Sprite_QueueOverlay)();
    if (BH_CALL(Battle_ActorIsOut)(Owner()[5]) != 0 && (Owner()[0] & 1) == 0) rv::Current<records::BattleTask>()->state = 0;
}

// original 0x433790 (state 4): with the round flags' bit 2, the owner's actor
// (+5): a member's status byte +0x90, an enemy's +0x92 (the index - 3,
// unchecked: kept, a read); with any of 0x58 set Sprite_Current's +1 down by
// one (back to state 3), else +1 = 0.
extern "C" void __cdecl BattleFx_WatchRecheck(void) {
    if ((B(::bof3::Img(rest_3a::at::kFlags.v)) & 4) == 0) return;
    const unsigned actor = OwnerRest3a()[5];
    const unsigned char status = actor <= 2 ? B(::bof3::Img(rest_3a::at::kMembers.v) + 0x90 + actor * rest_3a::at::kMemberSize)
                                            : B(::bof3::Img(rest_3a::at::kEnemies.v) + 0x92 + (actor - 3) * rest_3a::at::kEnemySize);
    unsigned char* const s = Sc();
    if (status & 0x58)
        s[1] = static_cast<unsigned char>(s[1] - 1);
    else
        s[1] = 0;
}

// original 0x4337F0 (no PSX twin paired): state Sprite_Current +1 of a
// one-entry stack table, 0x433810. Any state but 0 would call through the
// original's stack; ours aborts.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleFx_Follow(void) {
    CallIndex(battle_fx_tasks::g.follow, 1, rv::As<records::BattleTask>(Sc())->state, "BattleFx_Follow");
}

// original 0x433810 (PSX 0x801E722C; not 0x800A5360, the pairing's guess):
// Sprite_Current takes its owner's (0x93B940) place and look - byte +0 with
// 0x20 added, then +0x48, the dwords +0x40 / +0x44, the words +0x58 / +0x5A,
// the bytes +0x4B / +0x4A / +0x49, the dwords +0x4C / +0x50 / +0x54, the
// bytes +0x27 / +0x28 / +0x2A, the dwords +0x34 / +0x38 / +0x3C, in that
// order; Sprite_UpdateScreen; then the slot is freed when the owner's bit 0
// is clear, and freed (again, tail-jumped) outside phase 1.
//
// As the original has it: both pointers are re-read for every copy, the
// owner again after Sprite_UpdateScreen, the phase after the first free; a
// dead owner outside phase 1 frees the slot twice.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleFx_FollowOwner(void) {
    struct Copy { unsigned char offset, size; };
    static constexpr Copy kCopies[] = {{0x48, 1}, {0x40, 4}, {0x44, 4}, {0x58, 2}, {0x5A, 2}, {0x4B, 1}, {0x4A, 1},
                                       {0x49, 1}, {0x4C, 4}, {0x50, 4}, {0x54, 4}, {0x27, 1}, {0x28, 1}, {0x2A, 1},
                                       {0x34, 4}, {0x38, 4}, {0x3C, 4}};
    {
        const unsigned char b = PtrAtBattleFxTasks(::bof3::Img(battle_fx_tasks::at::kTaskOwner.v))[0];
        rv::As<records::BattleTask>(Sc())->flags = static_cast<unsigned char>(b | 0x20);
    }
    for (const Copy& c : kCopies) {
        const unsigned char* const o = PtrAtBattleFxTasks(::bof3::Img(battle_fx_tasks::at::kTaskOwner.v));
        unsigned char* const sc = Sc();
        if (c.size == 1) sc[c.offset] = o[c.offset];
        else if (c.size == 2) battle_fx_tasks::SetWord(sc + c.offset, battle_fx_tasks::Word(o + c.offset));
        else battle_fx_tasks::SetLong(sc + c.offset, battle_fx_tasks::Long(o + c.offset));
    }
    battle_fx_tasks::g.update_screen();
    if ((PtrAtBattleFxTasks(::bof3::Img(battle_fx_tasks::at::kTaskOwner.v))[0] & 1) == 0) battle_fx_tasks::g.free_current();
    if (battle_fx_tasks::At(::bof3::Img(battle_fx_tasks::at::kPhase.v))[0] != 1) battle_fx_tasks::g.free_current();
}

// original 0x433970 (BattleFx_Dispatch's slot 11): by Sprite_Current's +1:
// BattleFxReform_Begin, _Shrink, _LoadDat, _Reload, _Grow.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleFxReform_Dispatch(void) {
    static const U kTable[] = {bof3::addr::BattleFxReform_Begin, bof3::addr::BattleFxReform_Shrink, bof3::addr::BattleFxReform_LoadDat,
                               bof3::addr::BattleFxReform_Reload, bof3::addr::BattleFxReform_Grow};
    CallStack("BattleFxReform_Dispatch", kTable, 5, Sc()[1]);
}

// original 0x4339B0 (slot 11's state 0): the owner's +0x48 = 2; +1 up.
extern "C" void __cdecl BattleFxReform_Begin(void) {
    OwnerRest3a()[0x48] = 2;
    unsigned char* const s = Sc();
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x4339D0 (state 1 of slots 11 and 12): the owner's u32 +0x40 down
// 0x2000 a frame while not 0; at 0 its +0 |= 0x40 and +1 up.
extern "C" void __cdecl BattleFxReform_Shrink(void) {
    unsigned char* const o = OwnerRest3a();
    const U scale = static_cast<U>(rv::As<records::SpriteHead>(o)->scale_x);
    if (scale != 0) {
        rv::As<records::SpriteHead>(o)->scale_x = static_cast<std::int32_t>(scale - 0x2000);
        return;
    }
    o[0] = static_cast<unsigned char>(o[0] | 0x40);
    unsigned char* const s = Sc();
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x433A00 (slot 11's state 2): LoadDatFile 0x2E8 (the owner's word
// +0x2C 0, its +8 0 or 1), 0x2EA (+0x2C 0, other +8), 0x2E9 / 0x2EB (+0x2C not
// 0); +1 up.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleFxReform_LoadDat(void) {
    const unsigned char* const o = OwnerRest3a();
    const bool plain = rv::As<records::SpriteHead>(o)->column == 0;
    const unsigned char form = o[8];
    const bool low = form == 0 || form == 1;
    BH_CALL(LoadDatFile)(plain ? (low ? 0x2E8 : 0x2EA) : (low ? 0x2E9 : 0x2EB));
    unsigned char* const s = Sc();
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x433A50 (slot 11's state 3): once File_LoadDone answers - the
// owner made Sprite_Current (the slot kept) and its member (+5) Field_State;
// Sprite_SetAnimation(the owner's +8 + 4); Sprite_ReleaseTint(the member of
// Sprite_Current's +5), Sprite_LoadPalette(0x80D380 + 0x40 * +5, 0),
// Battle_StatusTint(that member's word +0x90) (Sprite_Current read again for
// each); Sprite_SetClutStp; Field_State's +0x134 bit 0 cleared;
// BattleParty_RecalcStats; Sprite_Current back, and the slot's +1 up. Ours
// aborts on a member index past 2 where it is written through (section 7).
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleFxReform_Reload(void) {
    if (BH_CALL(File_LoadDone)() == 0) return;
    unsigned char* const owner = OwnerRest3a();
    unsigned char* const kept = Sc();
    Sprite_Current = owner;
    move_script::SetLong(move_script::At(::bof3::Img(Field_State_addr)), static_cast<std::int32_t>(Addr(MemberForWrite(owner[5], "BattleFxReform_Reload"))));
    BH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>(owner[8] + 4));
    BH_CALL(Sprite_ReleaseTint)(MemberForWrite(Sc()[5], "BattleFxReform_Reload"));
    const unsigned palette = Sc()[5];
    MemberForWrite(palette, "BattleFxReform_Reload");   // the palette slot is the member's: past 2 it writes on
    BH_CALL(Sprite_LoadPalette)(reinterpret_cast<unsigned short*>(move_script::At(::bof3::Img(Gfx_ClutStripSource_addr) + 0x1e00 + (palette << 6))), 0);
    const unsigned member = Sc()[5];
    BH_CALL(Battle_StatusTint)(move_script::Word(move_script::At(::bof3::Img(ObjTrio_addr) + 0x90 + member * rest_3a::at::kMemberSize)));
    BH_CALL(Sprite_SetClutStp)();
    unsigned char* const field = Field_State;
    rv::As<records::PartyMember>(field)->flags_134 = static_cast<std::int32_t>(rv::As<records::PartyMember>(field)->flags_134 & 0xFFFFFFFEu);
    BH_CALL(BattleParty_RecalcStats)();
    Sprite_Current = kept;
    kept[1] = static_cast<unsigned char>(kept[1] + 1);
}

// original 0x433B20 (slot 11's state 4): the owner's +0 bit 0x40 cleared; its
// u32 +0x40 up 0x2000 a frame to 0x10000; there +0x48 = 0, the member of its
// +5 (the owner read again) loses +0x130 bit 0x2000, and a tail jmp to
// BattleTask_FreeCurrent.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleFxReform_Grow(void) {
    unsigned char* o = OwnerRest3a();
    o[0] = static_cast<unsigned char>(o[0] & 0xBF);
    o = OwnerRest3a();
    const U scale = static_cast<U>(rv::As<records::SpriteHead>(o)->scale_x);
    if (scale != 0x10000) {
        rv::As<records::SpriteHead>(o)->scale_x = static_cast<std::int32_t>(scale + 0x2000);
        return;
    }
    o[0x48] = 0;
    unsigned char* const m = MemberForWrite(OwnerRest3a()[5], "BattleFxReform_Grow");
    rv::As<records::PartyMember>(m)->flags_130 = static_cast<std::int32_t>(rv::As<records::PartyMember>(m)->flags_130 & 0xFFFFDFFFu);
    BH_CALL(BattleTask_FreeCurrent)();
}

// original 0x433B80 (BattleFx_Dispatch's slot 12): by Sprite_Current's +1:
// BattleFxRestore_Begin, BattleFxReform_Shrink, BattleFxRestore_LoadDat,
// BattleFx_RestoreParty, BattleFx_RestoreFade.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleFxRestore_Dispatch(void) {
    static const U kTable[] = {bof3::addr::BattleFxRestore_Begin, bof3::addr::BattleFxReform_Shrink, bof3::addr::BattleFxRestore_LoadDat,
                               bof3::addr::BattleFx_RestoreParty, bof3::addr::BattleFx_RestoreFade};
    CallStack("BattleFxRestore_Dispatch", kTable, 5, Sc()[1]);
}

// original 0x433BC0 (slot 12's state 0): the member of the owner's +5 gets
// +0x130 bit 0x2000; the owner's +0x48 = 2; +1 up.
extern "C" void __cdecl BattleFxRestore_Begin(void) {
    unsigned char* const o = OwnerRest3a();
    unsigned char* const m = MemberForWrite(o[5], "BattleFxRestore_Begin");
    rv::As<records::PartyMember>(m)->flags_130 = static_cast<std::int32_t>(rv::As<records::PartyMember>(m)->flags_130 | 0x2000);
    o[0x48] = 2;
    unsigned char* const s = Sc();
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x433C00 (slot 12's state 2): with the round flags' bit 15 -
// Battle_BackupFlagged (al) not 0: for the party set 0x90412C 7, 0xD, 0xE
// and 0xF (read again for each) LoadDatFile 0x122..0x125 (the owner's +8 0 or
// 1) or 0x126..0x129; al 0: LoadDatFile(set + 0xFC) or (set + 0x10F). Without
// bit 15: LoadDatFile(the word of the party set's entry of the 20 at
// [0x64EA84] (+8 0 or 1) or [0x64EA88]); ours aborts past the 20. +1 up.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleFxRestore_LoadDat(void) {
    const auto low = [] {
        const unsigned char form = OwnerRest3a()[8];
        return form == 0 || form == 1;
    };
    if (LRest3a(::bof3::Img(rest_3a::at::kFlags.v)) & 0x8000) {
        if (BH_CALL(Battle_BackupFlagged)() != 0) {
            if (AreaW0b_PartySet == 7) BH_CALL(LoadDatFile)(low() ? 0x122 : 0x126);
            if (AreaW0b_PartySet == 0xD) BH_CALL(LoadDatFile)(low() ? 0x123 : 0x127);
            if (AreaW0b_PartySet == 0xE) BH_CALL(LoadDatFile)(low() ? 0x124 : 0x128);
            if (AreaW0b_PartySet == 0xF) BH_CALL(LoadDatFile)(low() ? 0x125 : 0x129);
        } else {
            const bool first = low();
            const unsigned set = AreaW0b_PartySet;
            BH_CALL(LoadDatFile)(static_cast<int>(set + (first ? 0xFCu : 0x10Fu)));
        }
    } else {
        const bool first = low();
        const unsigned set = AreaW0b_PartySet;
        const U table = LRest3a(first ? ::bof3::Img(Rest3a_RestoreFilesA_addr) : ::bof3::Img(Rest3a_RestoreFilesB_addr));
        if (set >= rest_3a::at::kPartySetCount)
            bof3::Fatal("BattleFxRestore_LoadDat: party set %u, past the %u entries of 0x%X (docs/rest_3a.md section 7)", set,
                        rest_3a::at::kPartySetCount, (unsigned)table);
        BH_CALL(LoadDatFile)(static_cast<int>(move_script::Word(move_script::At(table + set * 2))));
    }
    unsigned char* const s = Sc();
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x433D60 (no PSX twin paired): 1 when, of the backup records
// 0x939AE0 (the party's stride) from the first on, one in use (+0 bit 0)
// has +0x134 bit 0 before a record not in use is met; else 0.
extern "C" unsigned char __cdecl Battle_BackupFlagged(void) {
    for (unsigned i = 0; i < 3; ++i) {
        auto* const member_rec = rv::AsConst<records::PartyMember>(move_script::At(::bof3::Img(battle_e2::at::kBackup.v) + i * battle_e2::at::kPartyStride));
        if ((member_rec->flags & 1) == 0) return 0;
        if ((move_script::At(::bof3::Img(battle_e2::at::kBackupFlags.v) + i * battle_e2::at::kPartyStride)[0] & 1) != 0) return 1;
    }
    return 0;
}

// original 0x433DA0 (PSX 0x801E7AD8; hidden in 0x433D60's catalogue extent):
// slot 12's state 0 (0x433B80's stack table, Capcom's), once the DAT file is
// loaded: the owner's tint released; after Accession's single-member form
// (the round flags' 0x8000) the party count back from 0x904B8F into
// 0x904AB0 and window record 1, 0x904AB1 the members whose backup +0x91 lacks
// 0x40, and one task of slot 17 (BattleTask_Create(0, 0x11)) owned by each
// other member; then on the owner as Sprite_Current (Field_State its
// member), a character +0x89 7 or 0 keeps +0x9A, +0x90 without 0x4000,
// +0x130 without 0x18000 and +0x134 & 0xFFF884FD in 0x675ECC..; the common
// restore (RestoreBody, +8 kept); 0x9045FD / FE (or the member's pair, 3
// apart) = 0x904B94 / 95; the round flags' 0x8000 cleared, Sprite_Current
// back and its state + 1.
//
// As the original has it: the owner and the counts are re-read as the
// original re-reads them; BattleTask_Create's 0xFF is not tested (ours
// aborts); the member index is not checked (ours aborts past 2).
extern "C" void __cdecl BattleFx_RestoreParty(void) {
    static const char* const kWho = "BattleFx_RestoreParty";
    if (BH_CALL(File_LoadDone)() == 0) return;
    BH_CALL(Sprite_ReleaseTint)(Rec(kWho, Owner()[5]));
    if ((UL(move_script::At(::bof3::Img(battle_e2::at::kRoundFlags.v))) & 0x8000) != 0) {
        const unsigned char n = BattleE2_SavedCount;
        B(::bof3::Img(battle_e2::at::kPartyCount.v)) = n;
        Battle_PartyStanding = 0;
        if (n > 0) {
            unsigned char in = 0;
            for (unsigned k = 0; k < n; ++k)
                if ((move_script::At(::bof3::Img(battle_e2::at::kBackupBit40.v) + k * battle_e2::at::kPartyStride)[0] & 0x40) == 0) ++in;
            Battle_PartyStanding = in;
        }
        Window1_ParamA = n;
        WindowRecords[1].x = move_script::At(::bof3::Img(battle_e2::at::kCountX.v) + n)[0];
        if (n > 1) {
            unsigned char m = 1;
            do {
                unsigned char* const member = Member(m);
                const unsigned slot = BH_CALL(BattleTask_Create)(0, 0x11);
                rv::As<records::BattleTask>(NewTask(kWho, slot))->owner = Key(member);
                ++m;
            } while (m < B(::bof3::Img(battle_e2::at::kPartyCount.v)));
        }
    }
    unsigned char* const saved = Sprite_Current;
    unsigned char* const o = Owner();
    auto* const owner_rec = rv::As<records::PartyMember>(o);
    Sprite_Current = o;
    Field_State = Rec(kWho, owner_rec->actor);
    const unsigned char character = Rec(kWho, owner_rec->actor)[0x89];
    if (character == 7 || character == 0) {
        auto* const member_rec = rv::AsConst<records::PartyMember>(Rec(kWho, owner_rec->actor));
        BattleE2_KeptAp = member_rec->ap;
        BattleE2_KeptStatus = member_rec->status & 0xBFFF;
        BattleE2_KeptFlags = static_cast<U>(member_rec->flags_130) & 0xFFFE7FFFu;
        BattleE2_KeptFlags2 = static_cast<U>(member_rec->flags_134) & 0xFFF884FDu;
    }
    RestoreBody(kWho, o, true);
    if ((UL(move_script::At(::bof3::Img(battle_e2::at::kRoundFlags.v))) & 0x8000) != 0) {
        const unsigned char a = BattleE2_Place94, b = BattleE2_Place95;
        B(::bof3::Img(battle_e2::at::kPlaceCells.v)) = a;
        B(::bof3::Img(battle_e2::at::kPlaceCells.v) + 1) = b;
    } else {
        auto* const cur_member = rv::Current<records::PartyMember>();
        B(::bof3::Img(battle_e2::at::kPlaceCells.v) + static_cast<U>(cur_member->actor) * 3) = BattleE2_Place94;
        B(::bof3::Img(battle_e2::at::kPlaceCells.v) + 1 + static_cast<U>(cur_member->actor) * 3) = BattleE2_Place95;
    }
    move_script::SetWord(move_script::At(::bof3::Img(battle_e2::at::kRoundFlags.v)), move_script::Word(move_script::At(::bof3::Img(battle_e2::at::kRoundFlags.v))) & 0x7FFF);
    Sprite_Current = saved;
    ++saved[1];
}

// original 0x434310 (no PSX twin paired; hidden in 0x433D60's extent):
// BattleFx_Dispatch slot 17: by +1 through {BattleFx_RestoreMember,
// BattleFx_RestoreFade} (a stack table; ours aborts past 1).
extern "C" void __cdecl BattleFx_RestoreMemberTask(void) {
    static constexpr U kSteps[2] = {bof3::addr::BattleFx_RestoreMember, bof3::addr::BattleFx_RestoreFade};
    CallState("BattleFx_RestoreMemberTask", kSteps, 2, rv::Current<records::BattleTask>()->state);
}

// original 0x434340 (no PSX twin paired; hidden in 0x433D60's extent): slot
// 17's state 0, a task per other member: its owner's u32 +0x40 = 0, then on
// the owner as Sprite_Current (Field_State its member) the common restore
// (RestoreBody, +8 not kept); the owner's +0 bit 0x40 cleared,
// Sprite_Current back and its state + 1.
extern "C" void __cdecl BattleFx_RestoreMember(void) {
    static const char* const kWho = "BattleFx_RestoreMember";
    unsigned char* const o = Owner();
    auto* const owner_rec = rv::As<records::PartyMember>(o);
    unsigned char* const saved = Sprite_Current;
    Sprite_Current = o;
    Field_State = Rec(kWho, owner_rec->actor);
    rv::Store<U>(owner_rec->scale_x, 0);
    RestoreBody(kWho, Sprite_Current, false);
    rv::Current<records::PartyMember>()->flags = static_cast<unsigned char>(rv::Current<records::PartyMember>()->flags & 0xBF);
    Sprite_Current = saved;
    ++saved[1];
}

// original 0x4346C0 (no PSX twin paired; hidden in 0x433D60's extent): slot
// 12's state 1 and slot 17's: the owner's +0 bit 0x40 cleared; its u32 +0x40
// up by 0x2000 a frame until 0x10000; then its +0x48 = 0, its member's +0x130
// loses 0x2000 and +0x134 & 0xFFFCFFF9, and the slot is freed (a tail jmp).
extern "C" void __cdecl BattleFx_RestoreFade(void) {
    Owner()[0] = static_cast<unsigned char>(Owner()[0] & 0xBF);
    unsigned char* const o = Owner();
    const U v = static_cast<U>(rv::As<records::PartyMember>(o)->scale_x);
    if (v != 0x10000) {
        rv::Store<U>(rv::As<records::PartyMember>(o)->scale_x, v + 0x2000);
        return;
    }
    o[0x48] = 0;
    auto* const owner2 = rv::As<records::PartyMember>(Owner());
    rv::As<records::PartyMember>(Rec("BattleFx_RestoreFade", owner2->actor))->flags_130 = static_cast<U>(rv::As<records::PartyMember>(Rec("BattleFx_RestoreFade", owner2->actor))->flags_130) & ~0x2000u;
    rv::As<records::PartyMember>(Rec("BattleFx_RestoreFade", owner2->actor))->flags_134 = static_cast<U>(rv::As<records::PartyMember>(Rec("BattleFx_RestoreFade", owner2->actor))->flags_134) & 0xFFFCFFF9u;
    BH_CALL(BattleTask_FreeCurrent)();
}

// original 0x434730 (PSX 0x801E8D10, call-anchored): Sprite_Current over its
// owner. A member (+5 0..2): x = the owner's +0x2E + the s8 of
// BattleWin_MemberTargetOffsets at (+8 + 4 x the member's +0x89) x 2, y = its
// +0x30 - the byte 0x64DFC8 (+8 0..1) or 0x64DFC9 at the member's +0x89 x 2,
// - 8; an enemy: x = +0x2E + the s8 enemy +0xF2, y = +0x30 - the area's enemy
// data record +0xF0's byte +0x87, - 8. The owner's +0x32 copied.
//
// As the original has it: 0x93B940 is read afresh after the x store, the
// actor re-read from it; an enemy's index is not checked.
extern "C" void __cdecl BattleFx_PlaceOverOwner(void) {
    unsigned char* o = Owner();
    const unsigned actor = o[5];
    if (actor <= 2) {
        const unsigned row = rv::As<records::PartyMember>(Member(actor))->char_id;
        const auto dx = static_cast<std::int8_t>(move_script::At(::bof3::Img(BattleWin_MemberTargetOffsets_addr) + (o[8] + row * 4) * 2)[0]);
        rv::Store<unsigned>(rv::Current<records::BattleTask>()->screen_x, static_cast<unsigned>(dx + static_cast<std::uint16_t>(rv::As<records::SpriteHead>(o)->screen_x)));
        o = Owner();
        const unsigned row2 = Member(o[5])[0x89];
        const unsigned dy = o[8] <= 1 ? move_script::At(::bof3::Img(battle_e2::at::kMemberRowY.v) + row2 * 2)[0] : move_script::At(::bof3::Img(battle_e2::at::kMemberRowY.v) + 1 + row2 * 2)[0];
        rv::Store<unsigned>(rv::Current<records::BattleTask>()->screen_y, move_script::Word(o + 0x30) - dy - 8);
    } else {
        const auto dx = static_cast<std::int8_t>(EnemyObj(actor - 3)[0xF2]);
        rv::Store<unsigned>(rv::Current<records::BattleTask>()->screen_x, static_cast<unsigned>(dx + static_cast<std::uint16_t>(rv::As<records::SpriteHead>(o)->screen_x)));
        o = Owner();
        const unsigned kind = EnemyObj(static_cast<U>(o[5]) - 3)[0xF0];
        const unsigned dy = move_script::At(::bof3::Img(battle_e2::at::kEnemyDataY.v) + kind * 0x8C)[0];
        rv::Store<unsigned>(rv::Current<records::BattleTask>()->screen_y, move_script::Word(o + 0x30) - dy - 8);
    }
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->draw_key, rv::As<records::SpriteHead>(Owner())->draw_key);
}

// original 0x434870 (PSX 0x801E8EE0, call-anchored): the next status icon for
// the watch's owner (0x93B940): the bits 0x58 of its status (a member's
// ObjTrio +0x90, an enemy's +0x92), searched from Sprite_Current +0xB + 1
// upward modulo 16; the first bit set, else +0xB itself.
//
// As the original has it: with no bit set and +0xB at 16 or more the search
// never meets +0xB again and spins for ever; ours aborts there. An enemy's
// index (+5 - 3) is not checked.
extern "C" unsigned char __cdecl BattleFx_NextStatusIcon(void) {
    const unsigned actor = Owner()[5];
    const unsigned status = actor <= 2 ? move_script::Word(Member(actor) + 0x90) : move_script::Word(EnemyObj(actor - 3) + 0x92);
    const unsigned bits = status & 0x58;
    const unsigned char cur = Sprite_Current[0xB];
    for (unsigned char i = static_cast<unsigned char>((cur + 1) & 0xF); i != cur; i = static_cast<unsigned char>((i + 1) & 0xF)) {
        if ((bits >> i) & 1) return i;
        if (cur >= 16 && i == 15 && bits == 0)
            bof3::Fatal("BattleFx_NextStatusIcon: no status bit and +0xB %u - the original searches for ever "
                        "(docs/battle_e2.md section 7)",
                        cur);
    }
    return cur;
}

// original 0x4348E0 (no PSX twin paired; hidden in 0x434870's extent):
// BattleFx_Dispatch slot 9, with the battle's pose pool: by +1 through
// {_GridMarkStart, _GridMarkRun, BattleFx_FreeTask} (ours aborts past 2).
extern "C" void __cdecl BattleFx_GridMark(void) {
    static constexpr U kSteps[3] = {bof3::addr::BattleFx_GridMarkStart, bof3::addr::BattleFx_GridMarkRun, bof3::addr::BattleFx_FreeTask};
    WithBattlePoses("BattleFx_GridMark", kSteps, 3);
}

// original 0x434930 (hidden in 0x434870's extent): the marker's look
// (MarkerLook), placed at grid cell +0xB of window record 21 (GridPlace),
// +1 on, animation +0x4B, the overlay queued.
extern "C" void __cdecl BattleFx_GridMarkStart(void) {
    MarkerLook();
    GridPlace();
    ++rv::Current<records::BattleTask>()->state;
    BH_CALL(Sprite_SetAnimation)(Sprite_Current[0x4B]);
    BH_CALL(Sprite_QueueOverlay)();
}

// original 0x434A10 (hidden in 0x434870's extent): by +2 through
// {_GridMarkCount, _GridMarkPlace} (ours aborts past 1); then with record 21
// not in use (+0 bit 0) +1 = 2 and +2 = 0, else the script ticked and the
// overlay queued.
extern "C" void __cdecl BattleFx_GridMarkRun(void) {
    static constexpr U kSteps[2] = {bof3::addr::BattleFx_GridMarkCount, bof3::addr::BattleFx_GridMarkPlace};
    CallState("BattleFx_GridMarkRun", kSteps, 2, Sprite_Current[2]);
    if ((WindowRecords[21].in_use & 1) == 0) {
        rv::Current<records::BattleTask>()->state = 2;
        Sc()[2] = 0;
        return;
    }
    BH_CALL(Sprite_ScriptTick)();
    BH_CALL(Sprite_QueueOverlay)();
}

// original 0x434A60 (hidden in 0x434870's extent): placed at its grid cell;
// then +2 up by one for each of the 0x904B87 bytes of 0x904B84 equal to +0xB
// (the count re-read each time).
extern "C" void __cdecl BattleFx_GridMarkCount(void) {
    GridPlace();
    unsigned char i = 0;
    if (BattleE5_GeneCount == 0) return;
    unsigned char* s = Sprite_Current;
    do {
        if (move_script::At(::bof3::Img(battle_e2::at::kPickList.v) + i)[0] == s[0xB]) {
            s[2] = static_cast<unsigned char>(s[2] + 1);
            s = Sprite_Current;
        }
        ++i;
    } while (i < BattleE5_GeneCount);
}

// original 0x434B10 (hidden in 0x434870's extent): the first of the 0x904B87
// bytes of 0x904B84 equal to +0xB (both read once): x = record 21's +4 + 24 i
// + 0x41, y = its +6 + 0x12; none: +2 down by one.
extern "C" void __cdecl BattleFx_GridMarkPlace(void) {
    const unsigned char n = BattleE5_GeneCount;
    unsigned char* const s = Sprite_Current;
    if (n > 0) {
        const unsigned char b = s[0xB];
        unsigned char i = 0;
        do {
            if (move_script::At(::bof3::Img(battle_e2::at::kPickList.v) + i)[0] == b) {
                rv::Store<unsigned>(rv::As<records::BattleTask>(s)->screen_x, UL(move_script::At(::bof3::Img(Window21_addr) + 4)) + i * 24u + 0x41);
                rv::Store<unsigned>(rv::Current<records::BattleTask>()->screen_y, WindowRecords[0x15].y + 0x12);
                return;
            }
            ++i;
        } while (i < n);
    }
    s[2] = static_cast<unsigned char>(s[2] - 1);
}

// original 0x434B90 (hidden in 0x434870's extent): BattleFx_Dispatch slot 10,
// with the battle's pose pool: by +1 through {_ListHandStart, _ListHandRun,
// BattleFx_FreeTask} (ours aborts past 2).
extern "C" void __cdecl BattleFx_ListHand(void) {
    static constexpr U kSteps[3] = {bof3::addr::BattleFx_ListHandStart, bof3::addr::BattleFx_ListHandRun, bof3::addr::BattleFx_FreeTask};
    WithBattlePoses("BattleFx_ListHand", kSteps, 3);
}

// original 0x434BE0 (hidden in 0x434870's extent): the marker's look, x =
// record 21's +4 + 0x14, y = its +6 + 0x12, +1 on, animation 0x13, the
// overlay queued (a tail jmp).
extern "C" void __cdecl BattleFx_ListHandStart(void) {
    MarkerLook();
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->screen_x, UL(move_script::At(::bof3::Img(Window21_addr) + 4)) + 0x14);
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->screen_y, WindowRecords[0x15].y + 0x12);
    ++rv::Current<records::BattleTask>()->state;
    BH_CALL(Sprite_SetAnimation)(0x13);
    BH_CALL(Sprite_QueueOverlay)();
}

// original 0x434C80 (hidden in 0x434870's extent): by +2 through
// {_ListHandPress, _ListHandWait} (ours aborts past 1); then, drawn (+0 bit
// 0): record 21 not in use, +1 = 2 and +2 = 0; else placed as at the start
// and the overlay queued.
extern "C" void __cdecl BattleFx_ListHandRun(void) {
    static constexpr U kSteps[2] = {bof3::addr::BattleFx_ListHandPress, bof3::addr::BattleFx_ListHandWait};
    CallState("BattleFx_ListHandRun", kSteps, 2, Sprite_Current[2]);
    unsigned char* const s = Sprite_Current;
    auto* const cur_task = rv::As<records::BattleTask>(s);
    if ((cur_task->flags & 1) == 0) return;
    if ((WindowRecords[21].in_use & 1) == 0) {
        cur_task->state = 2;
        Sprite_Current[2] = 0;
        return;
    }
    rv::Store<unsigned>(cur_task->screen_x, UL(move_script::At(::bof3::Img(Window21_addr) + 4)) + 0x14);
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->screen_y, WindowRecords[0x15].y + 0x12);
    BH_CALL(Sprite_QueueOverlay)();
}

// original 0x434D00 (hidden in 0x434870's extent): animation 0x12; with no
// command tapped (0x904AA6 0xFF) animation 0x13 and +2 on.
extern "C" void __cdecl BattleFx_ListHandPress(void) {
    BH_CALL(Sprite_SetAnimation)(0x12);
    if (BattleE5_GeneRow != 0xFF) return;
    BH_CALL(Sprite_SetAnimation)(0x13);
    ++Sprite_Current[2];
}

// original 0x434D30 (hidden in 0x434870's extent): +2 back by one once a
// command is tapped (0x904AA6 not 0xFF); record 21 not in use: +1 on, +2 = 0.
extern "C" void __cdecl BattleFx_ListHandWait(void) {
    if (BattleE5_GeneRow != 0xFF) Sprite_Current[2] = static_cast<unsigned char>(Sprite_Current[2] - 1);
    if ((WindowRecords[21].in_use & 1) != 0) return;
    ++rv::Current<records::BattleTask>()->state;
    Sprite_Current[2] = 0;
}

// original 0x434D70 (hidden in 0x434870's extent): BattleFx_Dispatch slot 13,
// with the battle's pose pool: by +1 through {_Win18CursorStart,
// _Win18CursorRun, BattleFx_FreeTask} (ours aborts past 2).
extern "C" void __cdecl BattleFx_Win18Cursor(void) {
    static constexpr U kSteps[3] = {bof3::addr::BattleFx_Win18CursorStart, bof3::addr::BattleFx_Win18CursorRun, bof3::addr::BattleFx_FreeTask};
    WithBattlePoses("BattleFx_Win18Cursor", kSteps, 3);
}

// original 0x434DC0 (hidden in 0x434870's extent): CursorStart over record
// 18 (0x8033E8) and the grid 0x904608.
extern "C" void __cdecl BattleFx_Win18CursorStart(void) { CursorStart(::bof3::Img(Window18_addr), ::bof3::Img(battle_e2::at::kGrid18.v)); }

// original 0x434E80 (hidden in 0x434870's extent): CursorRun over record 18
// and the grid 0x904608.
extern "C" void __cdecl BattleFx_Win18CursorRun(void) { CursorRun(::bof3::Img(Window18_addr), ::bof3::Img(battle_e2::at::kGrid18.v)); }

// original 0x434F40 (hidden in 0x434870's extent): BattleFx_Dispatch slot 14,
// with the battle's pose pool: by +1 through {_Win19CursorStart,
// _Win19CursorRun, BattleFx_FreeTask} (ours aborts past 2).
extern "C" void __cdecl BattleFx_Win19Cursor(void) {
    static constexpr U kSteps[3] = {bof3::addr::BattleFx_Win19CursorStart, bof3::addr::BattleFx_Win19CursorRun, bof3::addr::BattleFx_FreeTask};
    WithBattlePoses("BattleFx_Win19Cursor", kSteps, 3);
}

// original 0x434F90 (hidden in 0x434870's extent): CursorStart over record
// 19 (0x80340C) and the grid 0x904620.
extern "C" void __cdecl BattleFx_Win19CursorStart(void) { CursorStart(::bof3::Img(Window19_addr), ::bof3::Img(battle_e2::at::kGrid19.v)); }

// original 0x435050 (hidden in 0x434870's extent): CursorRun over record 19
// and the grid 0x904620.
extern "C" void __cdecl BattleFx_Win19CursorRun(void) { CursorRun(::bof3::Img(Window19_addr), ::bof3::Img(battle_e2::at::kGrid19.v)); }

void BattleFx_Inject() {
    BOF3_INJECT(BattleFx_DamagePopup);
    BOF3_INJECT(BattleFx_PopupStart);
    BOF3_INJECT(BattleFx_PopupHold);
    BOF3_INJECT(BattleFx_PopupRise);
    BOF3_INJECT(BattleFx_PopupBounce);
    BOF3_INJECT(BattleFx_PopupEnd);
    BOF3_INJECT(BattleFx_RollingDigits);
    BOF3_INJECT(BattleFxFlash_Dispatch);
    BOF3_INJECT(BattleFxFlash_Banner);
    BOF3_INJECT(BattleFxFlash_Rise);
    BOF3_INJECT(BattleFxFlash_Fall);
    BOF3_INJECT(BattleFx_PoseTask);
    BOF3_INJECT(BattleFx_PoseStart);
    BOF3_INJECT(BattleFx_PosePlay);
    BOF3_INJECT(BattleFxTint_Dispatch);
    BOF3_INJECT(BattleFx_StepReset);
    BOF3_INJECT(BattleFxTint_Brighten);
    BOF3_INJECT(BattleFxTint_Hold);
    BOF3_INJECT(BattleFxAnim_Dispatch);
    BOF3_INJECT(BattleFxAnim_Start);
    BOF3_INJECT(BattleFxAnim_Run);
    BOF3_INJECT(BattleFxAnim_Linger);
    BOF3_INJECT(BattleFx_ActorWatch);
    BOF3_INJECT(BattleFx_ActorWatchTest);
    BOF3_INJECT(BattleFx_WatchIconStart);
    BOF3_INJECT(Sprite_StateRestart);
    BOF3_INJECT(BattleFx_WatchIcon);
    BOF3_INJECT(BattleFx_WatchRecheck);
    BOF3_INJECT(BattleFx_Follow);
    BOF3_INJECT(BattleFx_FollowOwner);
    BOF3_INJECT(BattleFxReform_Dispatch);
    BOF3_INJECT(BattleFxReform_Begin);
    BOF3_INJECT(BattleFxReform_Shrink);
    BOF3_INJECT(BattleFxReform_LoadDat);
    BOF3_INJECT(BattleFxReform_Reload);
    BOF3_INJECT(BattleFxReform_Grow);
    BOF3_INJECT(BattleFxRestore_Dispatch);
    BOF3_INJECT(BattleFxRestore_Begin);
    BOF3_INJECT(BattleFxRestore_LoadDat);
    BOF3_INJECT(Battle_BackupFlagged);
    BOF3_INJECT(BattleFx_RestoreParty);
    BOF3_INJECT(BattleFx_RestoreMemberTask);
    BOF3_INJECT(BattleFx_RestoreMember);
    BOF3_INJECT(BattleFx_RestoreFade);
    BOF3_INJECT(BattleFx_PlaceOverOwner);
    BOF3_INJECT(BattleFx_NextStatusIcon);
    BOF3_INJECT(BattleFx_GridMark);
    BOF3_INJECT(BattleFx_GridMarkStart);
    BOF3_INJECT(BattleFx_GridMarkRun);
    BOF3_INJECT(BattleFx_GridMarkCount);
    BOF3_INJECT(BattleFx_GridMarkPlace);
    BOF3_INJECT(BattleFx_ListHand);
    BOF3_INJECT(BattleFx_ListHandStart);
    BOF3_INJECT(BattleFx_ListHandRun);
    BOF3_INJECT(BattleFx_ListHandPress);
    BOF3_INJECT(BattleFx_ListHandWait);
    BOF3_INJECT(BattleFx_Win18Cursor);
    BOF3_INJECT(BattleFx_Win18CursorStart);
    BOF3_INJECT(BattleFx_Win18CursorRun);
    BOF3_INJECT(BattleFx_Win19Cursor);
    BOF3_INJECT(BattleFx_Win19CursorStart);
    BOF3_INJECT(BattleFx_Win19CursorRun);
}

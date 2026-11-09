// Moved here by tools/move_funcs.py from battle_e5.cpp, battle_e6.cpp, battle_sprites.cpp, dat_load.cpp, magic_engine.cpp, mode_flow.cpp, rest_3d.cpp, rest_4a.cpp, save_menu.cpp (docs/source-layout-round.md).
#include "game/battle/dragon_cmd.h"

#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <io.h>
#include <string>
#include <vector>
#include <windows.h>

#include "bof3/symbols.gen.h"
#include "common/asm_name.h"
#include "game/area_data_reload.h"
#include "game/battle/dragon_cmd_callees.h"
#include "game/battle_e5.h"
#include "game/battle_e5_callees.h"
#include "game/battle_e6.h"
#include "game/battle_e6_callees.h"
#include "game/battle_sprites.h"
#include "game/battle_sprites_callees.h"
#include "game/battle_text.h"
#include "game/boss_harness.h"
#include "game/char_names.h"
#include "game/cheats.h"
#include "game/config_text.h"
#include "game/dat_cache.h"
#include "game/dat_load.h"
#include "game/fishing_text.h"
#include "game/labels.h"
#include "game/language_tags.h"
#include "game/magic_engine.h"
#include "game/magic_harness.h"
#include "game/map_layers.h"
#include "game/menu_verbs.h"
#include "game/mode_flow.h"
#include "game/mode_flow_callees.h"
#include "game/move_script_bytes.h"
#include "game/msg_pool.h"
#include "game/music_seq.h"
#include "game/name_tables.h"
#include "game/pause_text.h"
#include "game/quick_menu.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_3d.h"
#include "game/rest_3d_callees.h"
#include "game/rest_4a.h"
#include "game/rest_4a_callees.h"
#include "game/save_menu.h"
#include "game/save_menu_callees.h"
#include "game/scena_sc15.h"
#include "game/scenario_harness.h"
#include "game/sound_in.h"
#include "game/sound_sp.h"
#include "game/text_advance.h"
#include "game/text_draw.h"
#include "game/text_pairs.h"
#include "game/title_menu.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/exe_host.h"
#include "hook/exe_image.h"
#include "hook/host.h"
#include "hook/log.h"
#include "hook/run_speed.h"
#include "input/sigma.h"

namespace {

namespace at = dragon_cmd::at;

}  // namespace

using namespace battle_sprites;

using namespace mode_flow;

using namespace save_menu;

// Group HT of the button round: a face or shoulder bit the original tests by
// a constant is Japan's, asked through σ (docs/button-tests.md).
using bof3x::input::sigma::Sigma;

namespace {

using U = std::uint32_t;

using S32BattleE5 = std::int32_t;

using move_script::At;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

unsigned char& B(U address) { return move_script::At(address)[0]; }

std::uint16_t W(U address) { return move_script::Word(move_script::At(address)); }

void SetW(U address, unsigned v) { move_script::SetWord(move_script::At(address), v); }

U L(U address) { return static_cast<U>(move_script::Long(move_script::At(address))); }

S32BattleE5 S16BattleE5(std::uint16_t v) { return static_cast<std::int16_t>(v); }

unsigned char* Ptr(U cell) { return move_script::At(L(cell)); }

unsigned char* PartyBattleE5(unsigned member) { return move_script::At(::bof3::Img(ObjTrio_addr) + member * battle_e5::at::kPartyStride); }

// The callees nobody owns yet, by address (battle_e5_callees.h).
using Void0 = void (__cdecl*)();

U Key(const unsigned char* p) { return Low32(p); }

using Entry = U (__cdecl*)(U);

Entry EntryAt(U table, unsigned index) { return reinterpret_cast<Entry>(static_cast<std::uintptr_t>(L(table + 4 * index))); }

// jmp [table + 4 * byte 0x904AA4]: the table's `entries` steps, a Fatal past
// them (the original jumps through whatever follows: the next table's steps,
// or for 0x64ED50 the data after it). The jmp leaves the caller's stack word
// and the step's eax as they were: ours hands the word on and answers the
// step's eax.
U Dispatch(const char* who, U table, unsigned entries, U through) {
    const unsigned step = BattleE4_Step4;
    if (step >= entries)
        bof3::Fatal("%s: 0x904AA4 is %u, past the %u steps of 0x%X - the original jumps through the dword after "
                    "(docs/battle_e5.md section 7)",
                    who, step, entries, (unsigned)table);
    return EntryAt(table, step)(through);   // the step as read: the fuzz swaps the table's cells for its recorders
}

void Sound(unsigned id) { BH_CALL(Sound_PlayEffect)(static_cast<unsigned short>(id)); }

void NextStep() { BattleE4_Step4 = static_cast<unsigned char>(BattleE4_Step4 + 1); }

const unsigned char* Line(unsigned id) { return BH_CALL(Msg_SystemPtr)(id); }

// The message window's last line (0x93C2C4 + ((0x93C2A1 - 1) & 15) * 8),
// the count read after the Msg_SystemPtr call that made the text.
void ShowLine(const unsigned char* text) {
    move_script::SetLong(move_script::At(::bof3::Img(battle_e5::at::kMsgText.v) + ((BattleActorCopies_QueueWrite - 1u) & 0xF) * 8), static_cast<S32BattleE5>(Key(text)));
}

unsigned char* Member() { return reinterpret_cast<unsigned char*>(BattleE4_MenuActor.Native()); }

U Pressed() { return Input_Pressed; }

bool Cancel(U pressed) { return (Field_CancelButtons & pressed & 0xFFFF) != 0; }

bool Confirm(U pressed) { return (Field_ConfirmButtons & pressed & 0xFFFF) != 0; }

U Repeat(U mask) { return BH_CALL(Input_AutoRepeat)(Pressed() & mask); }

void CloseList(U window) { B(window + 3) = 2; }

// A list's slot: 4 bytes at `table` + 4 * (the cursor's two signed words,
// summed: +0x12 and +0x10 of its window record).
U SlotOf(U table, U window) { return table + 4u * static_cast<U>(S16BattleE5(W(window + 0x12)) + S16BattleE5(W(window + 0x10))); }

// A list's cursor move (the four copies at 0x450412, 0x450C64, 0x45105F,
// 0x45137E): a = the window's +0x12, b = its +0x10, by the repeat keys -
//   0x1000  a + b above 0: b 0 -> a - 1; b above 0 -> b - 1;
//   0x0004  a at least 3 -> a - 3; a 0 -> b = 0; else a = 0;
//   0x4000  a + b below `sum_max`: b 2 -> a + 1; b below 2 -> b + 1;
//   0x0008  a `a_max` -> b = 2; else a + 3, or `a_max` when a + b + 3 runs past it;
// the first that holds, then 0x101 when a + b (signed) is not the old sum
// (the old one as an unsigned 16-bit value, as the original compares them).
void MoveCursor(U window, U keys, S32BattleE5 sum_max, S32BattleE5 a_max) {
    const U wa = window + 0x12, wb = window + 0x10;
    std::uint16_t a = W(wa), b = W(wb);
    const U before = static_cast<std::uint16_t>(a + b);
    if (keys & 0x1000) {
        if (S16BattleE5(a) + S16BattleE5(b) > 0) {
            if (b == 0) SetW(wa, a = static_cast<std::uint16_t>(a - 1));
            else if (S16BattleE5(b) > 0) SetW(wb, b = static_cast<std::uint16_t>(b - 1));
        }
    } else if (keys & Sigma(4)) {
        if (S16BattleE5(a) >= 3) SetW(wa, a = static_cast<std::uint16_t>(a - 3));
        else if (a == 0) SetW(wb, b = 0);
        else SetW(wa, a = 0);
    } else if (keys & 0x4000) {
        if (S16BattleE5(a) + S16BattleE5(b) < sum_max) {
            if (b == 2) SetW(wa, a = static_cast<std::uint16_t>(a + 1));
            else if (S16BattleE5(b) < 2) SetW(wb, b = static_cast<std::uint16_t>(b + 1));
        }
    } else if (keys & Sigma(8)) {
        if (S16BattleE5(a) == a_max) {
            SetW(wb, b = 2);
        } else {
            a = S16BattleE5(a) + S16BattleE5(b) + 3 > a_max ? static_cast<std::uint16_t>(a_max) : static_cast<std::uint16_t>(a + 3);
            SetW(wa, a);
        }
    }
    if (static_cast<U>(S16BattleE5(a) + S16BattleE5(b)) != before) Sound(0x101);
}

// A list's slot priced (0x450510, 0x450D60): 0 for an empty slot (+3 0xFF);
// else the cost bytes 0x64EC9C of its gene +0 and of +1 / +2 unless 0xFF,
// summed in a byte, against the member's AP (0x802DDA by the menu actor's
// +5): 1 when the member has it.
unsigned char Affordable(U slot) {
    if (B(slot + 3) == 0xFF) return 0;
    unsigned char cost = B(::bof3::Img(battle_e5::at::kGeneCostTable.v) + B(slot));
    if (B(slot + 1) != 0xFF) cost = static_cast<unsigned char>(cost + B(::bof3::Img(battle_e5::at::kGeneCostTable.v) + B(slot + 1)));
    if (B(slot + 2) != 0xFF) cost = static_cast<unsigned char>(cost + B(::bof3::Img(battle_e5::at::kGeneCostTable.v) + B(slot + 2)));
    return cost <= W(::bof3::Img(0x802DDA) + rv::As<records::PartyMember>(Member())->actor * battle_e5::at::kPartyStride);
}

// A slot's genes into 0x904B84..86 (0x450610, 0x450E70): the count 0x904B87
// zeroed, then each of the slot's three bytes not 0xFF stored at its own
// place and counted.
void TakeSlot(U slot) {
    BattleE5_GeneCount = 0;
    for (unsigned j = 0; j < 3; ++j) {
        const unsigned char g = B(slot + j);
        if (g == 0xFF) continue;
        B(::bof3::Img(battle_e5::at::kGenes.v) + j) = g;
        BattleE5_GeneCount = static_cast<unsigned char>(BattleE5_GeneCount + 1);
    }
}

// The list windows' opening and closing (0x4502C0 / 0x450B60, 0x450680 /
// 0x450EF0, 0x4505A0 / 0x450DF0): see each.
void OpenList(U slot, U window, unsigned char kind, unsigned x, unsigned line) {
    if (WindowRecords[21].in_use & 1) return;
    WindowRecords[21].in_use = 0;
    BH_CALL(Window_Alloc)(slot, 5);
    B(window + 2) = kind;
    B(window + 3) = 0;
    SetW(window + 4, x);
    SetW(window + 6, 0x3F);
    SetW(window + 0x10, 0);
    SetW(window + 0x12, 0);
    ShowLine(Line(line));
    BossSb_Countdown = static_cast<unsigned char>(BossSb_Countdown | 0x80);
    NextStep();
}

void ReopenMenu(unsigned x) {
    BH_CALL(Window_Alloc)(0x15, 5);
    B(::bof3::Img(battle_e5::at::kWin21.v) + 2) = 0;
    B(::bof3::Img(battle_e5::at::kWin21.v) + 3) = 0;
    SetW(::bof3::Img(battle_e5::at::kWin21.v) + 4, x);
    SetW(::bof3::Img(battle_e5::at::kWin21.v) + 6, 0x3F);
}

void BackToMenu(U window) {
    if (B(window) & 1) return;
    const unsigned char pick = BossSb_Countdown;
    BattleE4_Step3 = 2;
    BattleE4_Step4 = 0;
    BossSb_Countdown = static_cast<unsigned char>(pick & 0x7F);
}

U LBattleE6(const unsigned char* p) { return static_cast<U>(move_script::Long(p)); }

void SetL(unsigned char* p, U v) { move_script::SetLong(p, static_cast<std::int32_t>(v)); }

std::int32_t S8(unsigned char v) { return static_cast<std::int8_t>(v); }

std::int32_t S16BattleE6(unsigned v) { return static_cast<std::int16_t>(static_cast<std::uint16_t>(v)); }

U Addr(const void* p) { return Low32(p); }

// The party's records and the enemies' objects as the originals index them:
// unchecked (an actor past 2 reaches past the three members, as there).
unsigned char* PartyBattleE6(unsigned member) { return move_script::At(::bof3::Img(ObjTrio_addr) + (member & 0xFFu) * battle_e6::at::kPartyStride); }

unsigned char* EnemyBattleE6(unsigned index) { return move_script::At(::bof3::Img(Battle_Enemies_addr) + index * battle_e6::at::kEnemyStride); }

unsigned Actor() { return B(::bof3::Img(battle_e6::at::kActor.v)); }

unsigned char* Owner() { return move_script::At(BattleE2_Owner); }

// The three pure percent helpers nobody owns: (value * percent) / 100 clamped.
using Percent = U (__cdecl*)(U value, std::int32_t percent);

U Percent999(U value, std::int32_t percent) { return BH_AT(Percent, battle_e6::at::kPercent999.v)(value, percent); }

U Percent9999(U value, std::int32_t percent) { return BH_AT(Percent, battle_e6::at::kPercent9999.v)(value, percent); }

U Percent100(U value, std::int32_t percent) { return BH_AT(Percent, battle_e6::at::kPercent100.v)(value, percent); }

// The party records copied to 0x939AE0 before a transformation changes them:
// 0x904AB0 records of 0x14C bytes, a dword at a time.
void BackUpParty() {
    const unsigned n = B(::bof3::Img(battle_e6::at::kPartyCount.v));
    if (n == 0) return;
    for (U i = 0; i < n * battle_e6::at::kPartyStride; i += 4) SetL(move_script::At(::bof3::Img(battle_e6::at::kPartyBackup.v) + i), LBattleE6(move_script::At(::bof3::Img(ObjTrio_addr) + i)));
}

// The first stat of a form (the u16 at +0xC0, +0xA0 and +0x98): `percent` of
// the character record's u16 +0x40 through 0x446F50, less (that times the
// record's byte +0x1E, + 5) / 10; 0x939EE0 keeps the value, 0x939EE2 the
// member's u16 +0xC4 as it was.
void FirstStat(unsigned char* member, const unsigned char* character, std::uint16_t value) {
    auto* const member_rec = rv::As<records::PartyMember>(member);
    move_script::SetWord(move_script::At(::bof3::Img(battle_e6::at::kStatDeltas.v)), value);
    const std::int32_t cut = (static_cast<std::int32_t>(character[0x1E]) * S16BattleE6(value) + 5) / 10;
    const auto stat = static_cast<std::uint16_t>(LBattleE6(move_script::At(::bof3::Img(battle_e6::at::kStatDeltas.v))) - static_cast<U>(cut));
    const std::uint16_t next = move_script::Word(member + 0xC4);
    move_script::SetWord(member + 0xC0, stat);
    member_rec->max_hp = stat;
    member_rec->hp = stat;
    move_script::SetWord(move_script::At(::bof3::Img(battle_e6::at::kStatDeltas.v) + 2), next);
}

// Kept by the recipe path: the stat at `field` (0xC4 .. 0xCA) through
// 0x446F20 by `percent`, mirrored at field - 0x20, its change into the cell
// `delta` (which held the old value).
void RecipeStat(unsigned char* member, unsigned field, unsigned percent, U delta) {
    move_script::SetWord(move_script::At(delta), move_script::Word(member + field));
    const auto v = static_cast<std::uint16_t>(Percent999(move_script::Word(member + field), static_cast<std::int32_t>(percent) * 10));
    move_script::SetWord(member + field, v);
    move_script::SetWord(member + field - 0x20, v);
    move_script::SetWord(move_script::At(delta), static_cast<std::uint16_t>(v - move_script::Word(move_script::At(delta))));
}

// A mixed form's list end: 0xD9 at the count, the rest of the ten zeroed.
void EndAbilities(unsigned char* list, unsigned char count) {
    list[count] = 0xD9;
    const auto next = static_cast<unsigned char>(count + 1);
    for (unsigned i = next; i < 10; ++i) list[i] = 0;
}

// Picks a party form when the actor holds `gene` (the group code then 1).
unsigned char PartyPick(unsigned char gene, unsigned char held, unsigned char plain) {
    if (BH_CALL(DragonGenes_Held)(gene) != 0) {
        BattleE6_FormGroupWork = 1;
        return held;
    }
    return plain;
}

// A task's `call [table + 4 * +1]`, a Fatal past its entries (the original
// calls through whatever follows), then Sprite_UpdateScreen while the slot's
// +0 is not 0 (Sprite_Current read again after the call).
void TaskDispatch(const char* who, U table, unsigned entries) {
    const unsigned state = Sprite_Current[1];
    if (state >= entries)
        bof3::Fatal("%s: the task's state +1 is %u, past the %u entries of 0x%X - the original calls through the dword after "
                    "(docs/battle_e6.md section 7)",
                    who, state, entries, (unsigned)table);
    reinterpret_cast<void (__cdecl*)()>(static_cast<std::uintptr_t>(LBattleE6(move_script::At(table + 4 * state))))();   // as read (the fuzz swaps the cells)
    if (Sprite_Current[0] != 0) BH_CALL(Sprite_UpdateScreen)();
}

// A dash state's velocity toward the actor: (member - sprite) / 16, toward zero.
U Sixteenth(U to, U from) { return static_cast<U>(static_cast<std::int32_t>(to - from) / 16); }

unsigned Clamp4(unsigned sum) {
    std::int32_t i = S8(static_cast<unsigned char>(sum + 2));
    if (i < 0) return 0;
    return i > 4 ? 4u : static_cast<unsigned>(i);
}

void MixByte(unsigned char* record, unsigned at, U cell, unsigned char v) {
    record[at + 0x80] = v;   // +0xCF ..: the form's
    record[at + 0x60] = v;   // +0xAF ..
    B(cell) = v;
}

}  // namespace

namespace battle_sprites {

using move_script::At;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

namespace {

// Actor records, indexed in 32 bits as the originals' lea chains do.
unsigned char* Party(std::uint32_t i) { return At(::bof3::Img(ObjTrio_addr) + i * at::kPartyStride); }

unsigned char* Enemy(std::uint32_t i) { return At(::bof3::Img(Battle_Enemies_addr) + i * at::kEnemyStride); }

unsigned char* Ptr(const unsigned char* at) { return At(static_cast<std::uint32_t>(Long(at))); }

std::int32_t Address(const void* p) { return static_cast<std::int32_t>(Low32(p)); }

unsigned char* ActionRecord(std::uint32_t action) { return At(::bof3::Img(at::kActions.v) + action * at::kActionStride); }

// 0x904B40 holds the acting block's +0xC: the actor's action record (a
// member's +0x124, an enemy's +0x104; records.h ActionRecord).
std::uint16_t CurrentAction() { return rv::As<records::ActionRecord>(Ptr(At(::bof3::Img(BattleSprites_Action_addr))))->action_id; }

unsigned char TargetBattleSprites() { return At(::bof3::Img(at::kTarget.v))[0]; }

void SetTarget(unsigned char t) { At(::bof3::Img(at::kTarget.v))[0] = t; }

void OrLong(unsigned char* at, std::uint32_t bits) { SetLong(at, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(at)) | bits)); }

// The four action ids that want a target that is out (Battle_ActionSuitsTarget,
// Battle_MemberOutAction): by their shape revivals - unconfirmed.
bool OutAction(std::uint16_t action) { return action == 0x4C || action == 0x4D || action == 0xB4 || action == 0xB5; }

}  // namespace

}  // namespace battle_sprites

namespace {

// One chunk header, 16 bytes; the payload follows. The dword at +0x0C is never
// read, by the original or by us. (dat_cache.h's, which the cache's walk hands
// to the same switch.)
using ChunkHeader = dat_cache::Chunk;

// "DAT\" + a layer + "." + a file name + NUL: 4 + 23 + 1 + 35 + 1. The
// longest shipped name is 12 characters (Dat_FileNames, 742 entries, read
// 2026-10-08); a longer one is refused in LoadDatFile, not skipped.
constexpr int kOptPath = 0x40;

void WalkDatFile(const char* path);

void WalkDatFile(const char* path) {
    const int handle = dat_load::g_io.open(path, 0, 0);
    if (handle == -1) return;
    if (dat_load::g_datlog) dat_load::DatLog("file", path);

    const int file_size = dat_load::g_io.size(handle);
    auto* file = static_cast<std::uint8_t*>(dat_load::g_io.malloc(file_size));
    dat_load::g_io.read(handle, file, file_size);
    dat_load::g_io.close(handle);

    for (int off = 0; off < file_size;) {
        ChunkHeader h;
        std::memcpy(&h, file + off, sizeof h);
        dat_load::g_take(nullptr, h, file + off + sizeof h);
        off += h.size + static_cast<int>(sizeof h);
    }
    dat_load::g_io.free(file);
}

using magic_harness::Mem;

using magic_harness::PartyOf;

constexpr unsigned char kReacting = 6;

void Bump(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

// The effect-done bit: `or byte [0x904AA8], 4`.
void SetDone() { Mem(::bof3::Img(magic_harness::at::kFlags))[0] = static_cast<unsigned char>(Mem(::bof3::Img(magic_harness::at::kFlags))[0] | 4); }

// A stack-table dispatcher: the phase byte through the table the original
// builds on its stack, unchecked there; past the table ours aborts.
void DispatchMagicEngine(const char* who, const std::uint32_t* phases, unsigned n, unsigned phase) {
    if (phase >= n) bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, n);
    magic_harness::Phase(phases[phase])();
}

}  // namespace

#define MENGINE_EXPORT extern "C" __attribute__((disable_tail_calls))

namespace {

using S32Rest3d = std::int32_t;

S32Rest3d S16Rest3d(U v) { return static_cast<std::int16_t>(static_cast<std::uint16_t>(v)); }

// An actor's record as the originals index it: the low byte, 0..2 a party
// member (ObjTrio, 0x14C), else an enemy object at (actor - 3) * 0x128 - in
// 32 bits, unbounded either way (an index past the records reads or writes
// whatever lies there, on both sides).
bool IsMember(U actor) { return (actor & 0xFF) <= 2; }

U PartyAt(U actor) { return ::bof3::Img(ObjTrio_addr) + (actor & 0xFF) * rest_3d::at::kPartyStride; }

U EnemyAt(U actor) { return ::bof3::Img(Battle_Enemies_addr) + ((actor & 0xFF) - 3u) * rest_3d::at::kEnemyStride; }

unsigned char* PartyRest3d(U actor) { return move_script::At(PartyAt(actor)); }

unsigned char* EnemyRest3d(U actor) { return move_script::At(EnemyAt(actor)); }

// The cells by side: HP (party +0x98, enemy +0xA4); the flag words +0x130 /
// +0x110 (0x10000: no damage; 0x200: no hit pose) and +0x134 / +0x114.
unsigned char* Hp(U actor) { return IsMember(actor) ? PartyRest3d(actor) + 0x98 : EnemyRest3d(actor) + 0xA4; }

unsigned char* Flags(U actor) { return IsMember(actor) ? PartyRest3d(actor) + 0x130 : EnemyRest3d(actor) + 0x110; }

void OrLongRest3d(unsigned char* p, U bits) { move_script::SetLong(p, static_cast<S32Rest3d>(static_cast<U>(move_script::Long(p)) | bits)); }

unsigned ActorRest3d() { return B(::bof3::Img(rest_3d::at::kActor.v)); }

unsigned Target() { return B(::bof3::Img(rest_3d::at::kTarget.v)); }

U Ability() { return L(::bof3::Img(rest_3d::at::kAbility.v)) & 0xFFFF; }

// NameTable_Abilities (24 bytes an ability): +3 the s8 stat step, +4 the word
// whose low nine bits are the status / element mask.
U AbilityMask() { return W(::bof3::Img(bof3::addr::NameTable_Abilities) + 4 + Ability() * 24) & 0x1FF; }

unsigned char AbilityStep() { return B(::bof3::Img(bof3::addr::NameTable_Abilities) + 3 + Ability() * 24); }

// The two terms of the status roll, read before the rate lookup:
// a = min(u16 0x939FEA * 100 / 500 + 50, 100) and
// d = max(125 - u16 0x939F8A * 100 / 500, 50) (signed 32-bit, as there).
S32Rest3d RollAttack() {
    const S32Rest3d a = BattleDamage_AttackerInt * 100 / 500 + 50;
    return a > 100 ? 100 : a;
}

S32Rest3d RollDefence() {
    const S32Rest3d d = 125 - BattleDamage_TargetInt * 100 / 500;
    return d < 50 ? 50 : d;
}

// The roll: the rate's ax (movsx) of -1 is "cannot be resisted" (al 0);
// otherwise Rand, and al 1 (resisted) when Rand % 100 is at least
// (rate * d * a, in 32 bits) / 10000, both signed.
unsigned char Roll(S32Rest3d a, S32Rest3d d, U rate_eax) {
    const S32Rest3d rate = S16Rest3d(rate_eax);
    if (rate == -1) return 0;
    const S32Rest3d r = static_cast<S32Rest3d>(BH_CALL(Rand)());
    const S32Rest3d p = static_cast<S32Rest3d>(static_cast<U>(rate) * static_cast<U>(d) * static_cast<U>(a));
    return (r % 100) >= p / 10000 ? 1 : 0;
}

// The class lookups (0x44F030, 0x44F770): eax 0 to start; mask bit 0x40 the
// table's s16 by the actor's class byte +0xB5 (enemy +0xC5), 0x80 by +0xB6
// (+0xC6), 0x100 by +0xB7 (+0xC7), each later bit overriding.
S32Rest3d ClassLookup(U table, unsigned actor, unsigned mask) {
    const unsigned char* const c = IsMember(actor) ? PartyRest3d(actor) + 0xB5 : EnemyRest3d(actor) + 0xC5;
    S32Rest3d v = 0;
    if (mask & 0x40) v = S16Rest3d(W(table + 2u * c[0]));
    if (mask & 0x80) v = S16Rest3d(W(table + 2u * c[1]));
    if (mask & 0x100) v = S16Rest3d(W(table + 2u * c[2]));
    return v;
}

// 0x44FBB0 / 0x44FC10's shared body after the mark: the roll; resisted al 1;
// else the ability's s8 step (+3) through Effect_StepStatByte(step, stat),
// Battle_RecalcStats(the target, with the stat word's upper bytes above it as
// the original's register holds it), al 0.
unsigned char RollStatStep(unsigned stat) {
    if (BH_CALL(Battle_StatusResisted)(ActorRest3d(), Target())) return 1;
    const auto step = static_cast<signed char>(AbilityStep());
    BH_CALL(Effect_StepStatByte)(static_cast<U>(static_cast<S32Rest3d>(step)), stat);
    BH_CALL(Battle_RecalcStats)((stat & 0xFFFFFF00u) | Target());
    return 0;
}

// 0x44FC60 / 0x44FCA0's: the roll; resisted al 1; else
// Battle_InflictStatus(the target, status), al 0.
unsigned char RollInflict(unsigned status) {
    if (BH_CALL(Battle_StatusResisted)(ActorRest3d(), Target())) return 1;
    BH_CALL(Battle_InflictStatus)(Target(), status);
    return 0;
}

U UL(const unsigned char* p) { return static_cast<U>(move_script::Long(p)); }

U UL(U a) { return UL(move_script::At(a)); }

unsigned char* AtSaveMenu(save_menu::U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }

save_menu::U AddrSaveMenu(const volatile void* p) { return Low32(p); }

save_menu::U Byte(save_menu::U address) { return AtSaveMenu(address)[0]; }

save_menu::U LongSaveMenu(save_menu::U address) {
    save_menu::U v;
    std::memcpy(&v, AtSaveMenu(address), sizeof v);
    return v;
}

}  // namespace

#pragma clang attribute push(__attribute__((disable_tail_calls)), apply_to = function)

namespace {

void TakeBank(void*, const dat_cache::Chunk& c, std::uint8_t* payload) {
    if (c.kind == 2) save_menu::g.load_bank(c.tag, payload, static_cast<U>(c.size));
}

}  // namespace

#pragma clang attribute pop

// original 0x44F770: the status rate - as Battle_PsiStatusDeathAffinity with
// the table 0x64E96C, and 0xFFFF (the rolls' "cannot be resisted") when the
// mask has none of 0x1C0.
extern "C" int __cdecl Battle_StatusResistRate(unsigned actor, unsigned mask) {
    const S32Rest3d v = ClassLookup(::bof3::Img(Battle_StatusResistRates_addr), actor, mask);
    return (mask & 0x1C0) == 0 ? 0xFFFF : v;
}

// original 0x44F880: the same roll with the caller's mask (its low nine bits).
extern "C" unsigned char __cdecl Battle_StatusResistedMask(unsigned actor, unsigned target, unsigned mask) {
    (void)actor;
    const S32Rest3d a = RollAttack();
    const S32Rest3d d = RollDefence();
    const U rate = static_cast<U>(BH_CALL(Battle_StatusResistRate)(target, mask & 0x1FF));
    return Roll(a, d, rate);
}

// original 0x44F940: the roll on Battle_StatusResistRate20 (the ability's mask).
extern "C" unsigned char __cdecl Battle_StatusResisted20(unsigned actor, unsigned target) {
    (void)actor;
    const S32Rest3d a = RollAttack();
    const S32Rest3d d = RollDefence();
    const U rate = static_cast<U>(BH_CALL(Battle_StatusResistRate20)(target, AbilityMask()));
    return Roll(a, d, rate);
}

// original 0x44FA10: the other rate - the s16 of 0x64E98C by the class byte
// +0xB4 (enemy +0xC4) when the mask's byte has 0x20, else 0 (so its roll is
// never "cannot be resisted").
extern "C" int __cdecl Battle_StatusResistRate20(unsigned actor, unsigned mask) {
    if ((mask & 0x20) == 0) return 0;
    const unsigned char c = IsMember(actor) ? PartyRest3d(actor)[0xB4] : EnemyRest3d(actor)[0xC4];
    return S16Rest3d(W(::bof3::Img(rest_3d::at::kResistRates20.v) + 2u * c));
}

// original 0x44FA70 (Battle_ApplyDamage's weapon-status test): the roll with
// the mask 0x80.
extern "C" unsigned char __cdecl Battle_StatusResisted80(unsigned attacker, unsigned target) {
    (void)attacker;
    const S32Rest3d a = RollAttack();
    const S32Rest3d d = RollDefence();
    const U rate = static_cast<U>(BH_CALL(Battle_StatusResistRate)(target, 0x80));
    return Roll(a, d, rate);
}

// original 0x44FB30 (23 sites in R3B and R3C, ten here, Effect_QuarterAttack's
// tail): the round flags' 0x2000 (0x904AA9 bit 5: no hit sound or pop-up), and
// the target's +0x130 bit 9 set (no hit pose) and +0x12C bit 0 cleared (the
// damage pop-up's flag; an enemy's +0x110 and +0x10C).
extern "C" void __cdecl Effect_NoHitReaction(void) {
    const unsigned t = Target();
    B(::bof3::Img(rest_3d::at::kRoundFlagsHi.v)) = static_cast<unsigned char>(B(::bof3::Img(rest_3d::at::kRoundFlagsHi.v)) | 0x20);
    if (IsMember(t)) {
        unsigned char* const p = PartyRest3d(t);
        OrLongRest3d(p + 0x130, 0x200);
        p[0x12C] = static_cast<unsigned char>(p[0x12C] & 0xFE);
    } else {
        unsigned char* const e = EnemyRest3d(t);
        OrLongRest3d(e + 0x110, 0x200);
        e[0x10C] = static_cast<unsigned char>(e[0x10C] & 0xFE);
    }
}

// original 0x44FBB0 (seven sites in R3B and R3C): Effect_NoHitReaction, then
// RollStatStep.
extern "C" unsigned char __cdecl Effect_RollStatStepQuiet(unsigned stat) {
    BH_CALL(Effect_NoHitReaction)();
    return RollStatStep(stat);
}

// original 0x44FC10 (MagicFx_ApplyBuff's roll): RollStatStep without the mark.
extern "C" unsigned char __cdecl Effect_RollStatStep(unsigned stat) { return RollStatStep(stat); }

// original 0x44FC60 (seven sites in R3B and R3C): Effect_NoHitReaction, then
// RollInflict.
extern "C" unsigned char __cdecl Effect_RollInflictQuiet(unsigned status) {
    BH_CALL(Effect_NoHitReaction)();
    return RollInflict(status);
}

// original 0x44FCA0 (twelve sites, R3B, R3C and slots 122 and 128):
// RollInflict without the mark.
extern "C" unsigned char __cdecl Effect_RollInflict(unsigned status) { return RollInflict(status); }

// original 0x44FCE0: an HP-based amount. The actor's (0x904B34) HP / divisor
// (signed, the whole word), times the s16 0x64E9AC[Rand & 7], times the
// affinity (100 when the ability's mask has none of 0x1F, else
// Battle_ElementAffinity(the target, the mask)'s ax), / 10000 signed; ax
// zeroed (the upper half kept) when the target (read again) has flag 0x10000.
// A divisor of 0 faults in the original (idiv): ours aborts. Its callers pass
// 1, 2 and 3 (R3C's slots 64..66 pass 3, 1, 2).
extern "C" int __cdecl Effect_HpBasedDamage(int divisor) {
    const U hp = move_script::Word(Hp(ActorRest3d()));
    const U mask = AbilityMask();
    S32Rest3d affinity = 100;
    if (mask & 0x1F) affinity = S16Rest3d(static_cast<U>(BH_CALL(Battle_ElementAffinity)(Target(), mask)));
    const U roll = static_cast<U>(BH_CALL(Rand)()) & 7;
    const S32Rest3d variance = S16Rest3d(W(::bof3::Img(rest_3d::at::kHpVariance.v) + 2 * roll));
    if (divisor == 0)
        bof3::Fatal("Effect_HpBasedDamage: a divisor of 0 - the original's idiv faults (docs/rest_3d.md section 7)");
    const S32Rest3d quotient = static_cast<S32Rest3d>(hp) / divisor;
    const S32Rest3d product = static_cast<S32Rest3d>(static_cast<U>(variance) * static_cast<U>(quotient) * static_cast<U>(affinity));
    S32Rest3d v = product / 10000;
    if (static_cast<U>(move_script::Long(Flags(Target()))) & 0x10000) v = static_cast<S32Rest3d>(static_cast<U>(v) & 0xFFFF0000u);
    return v;
}

// original 0x44FDE0 (PSX 0x800A13C0; called by 0x442310 (BE3), 0x449A00 / 0x449C70 (BE4),
// 0x44D450, 0x44DFD0, 0x44E720): for each party member whose +0x134 has bit
// 1, the transformation's block 0x939EE0.. over its stats. With c the
// member's +0x9E: c +0x22 and +2 = w0 - (c[0] * w0 + 5) / 10 (w0 the signed
// word 0x939EE0, the division signed and truncated); the words c +0x26,
// +0x28, +0x2A, +0x2C raised by the words 0x939EE2, 0x939EE4, 0x939EE6,
// 0x939EE8 and copied to c +6, +8, +0xA, +0xC; the bytes 0x939EEA..0x939EF2
// into c +0x31..+0x39 and c +0x11..+0x19. The words 0x939EE0 / EE4 / EE8
// and the dword 0x939EF0 are read once, before the members (no call in the
// loop, and the members do not overlap them).
extern "C" void __cdecl BattleForm_ApplyStats(void) {
    const U ef0 = L(::bof3::Img(0x939EF0));
    const std::uint16_t ee8 = W(::bof3::Img(0x939EE8));
    const std::uint16_t ee0 = W(::bof3::Img(0x939EE0));
    const std::uint16_t ee4 = W(::bof3::Img(0x939EE4));
    for (unsigned m = 0; m < 3; ++m) {
        unsigned char* const c = PartyBattleE5(m) + 0x9E;
        auto* const member_rec = rv::As<records::PartyMember>(c - 0x9E);
        if ((c[0x96] & 2) == 0) continue;
        const S32BattleE5 product = static_cast<S32BattleE5>(static_cast<U>(c[0]) * static_cast<U>(S16BattleE5(ee0)) + 5u);
        const std::uint16_t hp = static_cast<std::uint16_t>(S16BattleE5(ee0) - product / 10);
        rv::Store<unsigned>(member_rec->base_max_hp, hp);
        member_rec->max_hp = hp;
        rv::Store<unsigned>(member_rec->base_atk, member_rec->base_atk + W(::bof3::Img(0x939EE2)));
        member_rec->atk = member_rec->base_atk;
        rv::Store<unsigned>(member_rec->base_def, member_rec->base_def + ee4);
        member_rec->def = member_rec->base_def;
        rv::Store<unsigned>(member_rec->base_agi, member_rec->base_agi + W(::bof3::Img(0x939EE6)));
        member_rec->agi = member_rec->base_agi;
        rv::Store<unsigned>(member_rec->base_intel, member_rec->base_intel + ee8);
        member_rec->intel = member_rec->base_intel;
        for (unsigned i = 0; i < 6; ++i) {
            c[0x31 + i] = B(::bof3::Img(0x939EEA) + i);
            c[0x11 + i] = c[0x31 + i];
        }
        const unsigned char last = B(::bof3::Img(0x939EF2));
        c[0x37] = static_cast<unsigned char>(ef0);
        c[0x17] = static_cast<unsigned char>(ef0);
        c[0x38] = static_cast<unsigned char>(ef0 >> 8);
        c[0x18] = static_cast<unsigned char>(ef0 >> 8);
        c[0x39] = last;
        c[0x19] = last;
    }
}

// original 0x44FF00 (Battle_MenuSteps[7], BattleMenu_ConfirmDispatch's entry
// for the Dragon command): jmp [DragonCmd_Parts + 4 * byte 0x904AA3]. Seven
// parts (BE5's dispatchers and DragonCmd_Open); a part past them is a Fatal
// where the original jumps through what follows (DragonCmd_LoadSteps' cells).
// The jmp leaves the caller's word and the part's eax in place: ours hands
// the word on and answers the part's eax.
extern "C" unsigned long __cdecl DragonCmd_PartDispatch(unsigned long through) {
    const unsigned part = BattleE4_Step3;
    if (part >= DragonCmd_Parts_count)
        bof3::Fatal("DragonCmd_PartDispatch: 0x904AA3 is %u, past the %u parts of DragonCmd_Parts - the original jumps "
                    "through the dword after (docs/rest_3d.md section 7)",
                    part, DragonCmd_Parts_count);
    using Part = unsigned long (__cdecl*)(unsigned long);
    const auto entry = reinterpret_cast<Part>(static_cast<std::uintptr_t>(DragonCmd_Parts[part]));
    return entry(through);   // as read: the fuzz swaps the table's cells for its recorders
}

// original 0x44FF10 (0x64ECCC[0]; PSX 0x800A162C): jmp [0x64ECE8 + 4 * 0x904AA4], two steps.
extern "C" unsigned long __cdecl DragonCmd_LoadDispatch(unsigned long through) {
    return Dispatch("DragonCmd_LoadDispatch", ::bof3::Img(DragonCmd_LoadSteps_addr), 2, through);
}

// original 0x44FF30 (0x64ECE8[0]; PSX 0x800A1668): window 16's +3 zero: LoadDatFile(0xCC) and
// the next step; else nothing.
extern "C" void __cdecl DragonCmd_LoadStart(void) {
    if (B(::bof3::Img(battle_e5::at::kWin16.v) + 3) != 0) return;
    BH_CALL(LoadDatFile)(0xCC);
    NextStep();
}

// original 0x44FF60 (0x64ECE8[1]; not in the cut; PSX 0x800A16AC): File_LoadDone: the CLUT
// rows 0x1A and 0x1B copied, Gfx_ClutStripDirty + 1, the next part (0x904AA3
// + 1, 0x904AA4 = 0).
extern "C" void __cdecl DragonCmd_LoadWait(void) {
    if (BH_CALL(File_LoadDone)() == 0) return;
    BH_CALL(Gfx_ClutStripCopyRow)(0x1A);
    BH_CALL(Gfx_ClutStripCopyRow)(0x1B);
    const unsigned char dirty = static_cast<unsigned char>(Gfx_ClutStripDirty + 1);
    const unsigned char part = static_cast<unsigned char>(BattleE4_Step3 + 1);
    Gfx_ClutStripDirty = dirty;
    BattleE4_Step3 = part;
    BattleE4_Step4 = 0;
}

// original 0x44FFA0 (0x64ECCC[1]; PSX 0x800A1714): window 21 (the menu: kind 5, +2 / +3 0, at
// 0x142, 0x3F, +0xA 0) and window 17 (+2 1, +3 0) claimed; sound 0x102; the
// 18 gene bits of 0x904650 spread into 0x939A60; Input_AutoRepeat's latch,
// 0x904AA6, 0x904AA7, the words 0x904B72 / 0x904B74, 0x904B87, 0x904B78 zeroed
// and the menu's line 0x904AA5 = 1; 0x4525B0; the next part.
extern "C" void __cdecl DragonCmd_Open(void) {
    BH_CALL(Window_Alloc)(0x15, 5);
    B(::bof3::Img(battle_e5::at::kWin21.v) + 2) = 0;
    B(::bof3::Img(battle_e5::at::kWin21.v) + 3) = 0;
    SetW(::bof3::Img(battle_e5::at::kWin21.v) + 4, 0x142);
    SetW(::bof3::Img(battle_e5::at::kWin21.v) + 6, 0x3F);
    B(::bof3::Img(battle_e5::at::kWin21.v) + 0xA) = 0;
    BH_CALL(Window_Alloc)(0x11, 5);
    B(::bof3::Img(battle_e5::at::kWin17.v) + 2) = 1;
    B(::bof3::Img(battle_e5::at::kWin17.v) + 3) = 0;
    Sound(0x102);
    const U genes = Gene_Bits;
    for (unsigned i = 0; i < 18; ++i) B(::bof3::Img(battle_e5::at::kGeneFlags.v) + i) = static_cast<unsigned char>((genes >> i) & 1);
    SetW(::bof3::Img(battle_e5::at::kRepeatLatch.v), 0);
    BossSb_Countdown = 1;
    BattleE5_GeneRow = 0;
    BattleE1_Roster = 0;
    SetW(::bof3::Img(battle_e5::at::kWord72.v), 0);
    SetW(::bof3::Img(battle_e5::at::kAsk.v), 0);
    BattleE5_GeneCount = 0;
    BattleActions_ApCost = 0;
    BH_AT(Void0, battle_e5::at::kDragonTask.v)();
    const unsigned char part = static_cast<unsigned char>(BattleE4_Step3 + 1);
    BattleE4_Step4 = 0;
    BattleE4_Step3 = part;
}

// original 0x450070 (0x64ECCC[2]; PSX 0x800A1868): jmp [0x64ECF0 + 4 * 0x904AA4], five steps.
extern "C" unsigned long __cdecl DragonCmd_MenuDispatch(unsigned long through) {
    return Dispatch("DragonCmd_MenuDispatch", ::bof3::Img(DragonCmd_MenuSteps_addr), 5, through);
}

// original 0x450090 (0x64ECF0[0]; PSX 0x800A18A4): the menu's line 0x42 + 0x904AA5 shown;
// the repeat keys of 0xE000; cancel: sound 0x106, two steps on (the close);
// confirm: sound 0x104, the next step (the pick); else 0x2000 / 0x8000 move
// the line down / up (sound 0x101 each), 0x4000 picks line 1 at once (sound
// 0x104, the next step); the line wrapped: below 0 (signed) 2, above 2 0.
extern "C" void __cdecl DragonCmd_MenuInput(void) {
    ShowLine(Line(static_cast<std::uint16_t>(BossSb_Countdown + 0x42)));
    const U keys = Repeat(0xE000);
    const U pressed = Pressed();
    if (Cancel(pressed)) {
        Sound(0x106);
        BattleE4_Step4 = static_cast<unsigned char>(BattleE4_Step4 + 2);
        return;
    }
    if (Confirm(pressed)) {
        Sound(0x104);
        NextStep();
        return;
    }
    if (keys & 0x2000) {
        Sound(0x101);
        BossSb_Countdown = static_cast<unsigned char>(BossSb_Countdown + 1);
    }
    if (keys & 0x8000) {
        Sound(0x101);
        BossSb_Countdown = static_cast<unsigned char>(BossSb_Countdown - 1);
    }
    if (keys & 0x4000) {
        Sound(0x104);
        const unsigned char step = static_cast<unsigned char>(BattleE4_Step4 + 1);
        BossSb_Countdown = 1;
        BattleE4_Step4 = step;
        return;
    }
    const unsigned char pick = BossSb_Countdown;
    if (static_cast<signed char>(pick) < 0) BossSb_Countdown = 2;
    else if (pick > 2) BossSb_Countdown = 0;
}

// original 0x4501B0 (0x64ECF0[1]; PSX 0x800A1A38): the line picked - 0x904AA5 |= 0x80, the
// part 3 + the line (0x904AA3), 0x904AA4 and 0x904AA6 zeroed.
extern "C" void __cdecl DragonCmd_MenuPick(void) {
    const unsigned char pick = BossSb_Countdown;
    BossSb_Countdown = static_cast<unsigned char>(pick | 0x80);
    BattleE4_Step3 = static_cast<unsigned char>(pick + 3);
    BattleE4_Step4 = 0;
    BattleE5_GeneRow = 0;
}

// original 0x4501E0 (0x64ECF0[2]; PSX 0x800A1A70): window 21's +3 = 2 (closing), the next step.
extern "C" void __cdecl DragonCmd_MenuClose(void) {
    CloseList(::bof3::Img(Window21_addr));
    NextStep();
}

// original 0x450200 (0x64ECF0[3]; PSX 0x800A1A94): window 21 closed (+0 bit 0 clear): 0x447F40,
// windows 17 and 21's +0 zeroed, BattleBanner_ShowName(the menu actor),
// window 4's +3 = 1, LoadDatFile(0xCF), the next step.
extern "C" void __cdecl DragonCmd_MenuCancel(void) {
    if (WindowRecords[21].in_use & 1) return;
    BH_AT(Void0, battle_e5::at::kTargetPrompt.v)();
    WindowRecords[17].in_use = 0;
    WindowRecords[21].in_use = 0;
    BH_CALL(BattleBanner_ShowName)(Member());
    WindowRecords[4].state = 1;
    BH_CALL(LoadDatFile)(0xCF);
    NextStep();
}

// original 0x450250 (0x64ECF0[4]; not in the cut; PSX 0x800A1B1C): File_LoadDone: CLUT row
// 0x1B copied, Gfx_ClutStripDirty = 1, the command step 0x904AA2 and the part
// 0x904AA3 = 1, 0x904AA4 = 0.
extern "C" void __cdecl DragonCmd_MenuCancelLoad(void) {
    if (BH_CALL(File_LoadDone)() == 0) return;
    BH_CALL(Gfx_ClutStripCopyRow)(0x1B);
    Gfx_ClutStripDirty = 1;
    BattleE4_Step2 = 1;
    BattleE4_Step3 = 1;
    BattleE4_Step4 = 0;
}

// original 0x450280 (0x64ECCC[3]; PSX 0x800A1B70): jmp [0x64ED04 + 4 * 0x904AA4], seven steps.
extern "C" unsigned long __cdecl DragonCmd_SlotsDispatch(unsigned long through) {
    return Dispatch("DragonCmd_SlotsDispatch", ::bof3::Img(DragonCmd_SlotsSteps_addr), 7, through);
}

// original 0x4502A0 (0x64ED04[0]; PSX 0x800A1BAC): window 21's +3 = 2, +0xA = 0; the next step.
extern "C" void __cdecl DragonCmd_SlotsCloseMenu(void) {
    const unsigned char step = static_cast<unsigned char>(BattleE4_Step4 + 1);
    B(::bof3::Img(battle_e5::at::kWin21.v) + 3) = 2;
    B(::bof3::Img(battle_e5::at::kWin21.v) + 0xA) = 0;
    BattleE4_Step4 = step;
}

// original 0x4502C0 (0x64ED04[1]; PSX 0x800A1BD8): window 21 closed: its +0 zeroed, window 18
// claimed (kind 5; +2 2, +3 0, at 0xFF5B, 0x3F, its cursor 0, 0), line 0x59,
// 0x904AA5 |= 0x80, the next step.
extern "C" void __cdecl DragonCmd_SlotsOpen(void) { OpenList(0x12, ::bof3::Img(Window18_addr), 2, 0xFF5B, 0x59); }

// original 0x450340 (0x64ED04[2]): the first list's cursor, once window 18's
// x is 0x5B. The repeat keys of 0x500C; cancel: sound 0x106, the next step
// (3, back); confirm: the slot affordable (DragonCmd_SlotAffordable) - sound
// 0x104, step 5 (take it) - else sound 0x107 and Input_Pressed read again;
// then its bit 0x10: the slot not empty - sound 0x104, part 6 (the store),
// step 0 - else sound 0x107; then the cursor moved in six slots (MoveCursor:
// a + b below 5, a at most 3).
extern "C" void __cdecl DragonCmd_SlotsCursor(void) {
    if (W(::bof3::Img(battle_e5::at::kWin18.v) + 4) != 0x5B) return;
    const U keys = Repeat(Sigma(0x500C));
    U pressed = Pressed();
    if (Cancel(pressed)) {
        Sound(0x106);
        NextStep();
        return;
    }
    if (Confirm(pressed)) {
        if (BH_CALL(DragonCmd_SlotAffordable)() & 0xFF) {
            Sound(0x104);
            BattleE4_Step4 = 5;
            return;
        }
        Sound(0x107);
        pressed = Pressed();
    }
    if (pressed & Sigma(0x10)) {
        if (B(SlotOf(::bof3::Img(battle_e5::at::kSlots.v), ::bof3::Img(Window18_addr)) + 3) != 0xFF) {
            Sound(0x104);
            BattleE4_Step3 = 6;
            BattleE4_Step4 = 0;
            return;
        }
        Sound(0x107);
    }
    MoveCursor(::bof3::Img(Window18_addr), keys, 5, 3);
}

// original 0x450510 (called by DragonCmd_SlotsCursor; PSX 0x800A1F40): the first list's slot
// at the cursor priced against the member's AP (Affordable). al out.
extern "C" unsigned char __cdecl DragonCmd_SlotAffordable(void) { return Affordable(SlotOf(::bof3::Img(battle_e5::at::kSlots.v), ::bof3::Img(Window18_addr))); }

// original 0x4505A0 (0x64ED04[3]; PSX 0x800A2020): window 21 claimed again (+2 / +3 0, at
// 0x142, 0x3F), window 18's +3 = 2, the next step.
extern "C" void __cdecl DragonCmd_SlotsCancel(void) {
    ReopenMenu(0x142);
    const unsigned char step = static_cast<unsigned char>(BattleE4_Step4 + 1);
    B(::bof3::Img(battle_e5::at::kWin18.v) + 3) = 2;
    BattleE4_Step4 = step;
}

// original 0x4505E0 (0x64ED04[4]; PSX 0x800A2088): window 18 closed: back to the menu (part 2,
// step 0, 0x904AA5's bit 7 cleared).
extern "C" void __cdecl DragonCmd_SlotsBack(void) { BackToMenu(::bof3::Img(Window18_addr)); }

// original 0x450610 (0x64ED04[5]; PSX 0x800A20CC): the slot at the cursor taken (TakeSlot),
// 0x4525B0, window 18's +3 = 2, the member's +0x134 |= 4, the next step.
extern "C" void __cdecl DragonCmd_SlotsConfirm(void) {
    TakeSlot(SlotOf(::bof3::Img(battle_e5::at::kSlots.v), ::bof3::Img(Window18_addr)));
    BH_AT(Void0, battle_e5::at::kDragonTask.v)();
    auto* const member_rec = rv::As<records::PartyMember>(Member());
    B(::bof3::Img(battle_e5::at::kWin18.v) + 3) = 2;
    member_rec->flags_134 = static_cast<S32BattleE5>(static_cast<U>(static_cast<std::int32_t>(member_rec->flags_134)) | 4u);
    NextStep();
}

// original 0x450680 (0x64ED04[6]; PSX 0x800A21AC): window 18 closed: 0x447F40, windows 17,
// 18, 16, 19's +0 zeroed, LoadDatFile(0xCF), then part 4's step 3 (the
// gene grid's close and commit).
extern "C" void __cdecl DragonCmd_SlotsClose(void) {
    if (WindowRecords[18].in_use & 1) return;
    BH_AT(Void0, battle_e5::at::kTargetPrompt.v)();
    WindowRecords[17].in_use = 0;
    WindowRecords[18].in_use = 0;
    WindowRecords[16].in_use = 0;
    WindowRecords[19].in_use = 0;
    BH_CALL(LoadDatFile)(0xCF);
    BattleE4_Step3 = 4;
    BattleE4_Step4 = 3;
}

// original 0x4506C0 (0x64ECCC[4]; PSX 0x800A222C): jmp [0x64ED20 + 4 * 0x904AA4], five steps.
extern "C" unsigned long __cdecl DragonCmd_GenesDispatch(unsigned long through) {
    return Dispatch("DragonCmd_GenesDispatch", ::bof3::Img(DragonCmd_GenesSteps_addr), 5, through);
}

// original 0x4506E0 (0x64ED20[0]; PSX 0x800A2268): window 17's +3 = 2, window 4's +3 = 1, the
// next step.
extern "C" void __cdecl DragonCmd_GenesOpen(void) {
    const unsigned char step = static_cast<unsigned char>(BattleE4_Step4 + 1);
    B(::bof3::Img(battle_e5::at::kWin17.v) + 3) = 2;
    WindowRecords[4].state = 1;
    BattleE4_Step4 = step;
}

// original 0x450700 (0x64ED20[1]): the gene grid, three rows of six (0x904AA6,
// 0x904AA7; row 0xFF the "done" line), genes picked into 0x904B84.. (three at
// most).
//   The banner: line 0x4172 + 6 * row + column for a gene held, else 0x4000,
//   through BattleBanner_Set(0, 1, 1, 0, 0xFF, text).
//   Cancel: a gene picked - the last put back (its flag set again), 0x4525B0,
//   sound 0x106; none - windows 17 / 4's +3 = 1 / 2, back to the menu (part
//   2), sound 0x106.
//   Confirm on the done line: genes picked and the member's AP (+0x9A) at
//   least 0x904B78 - the next step; else sound 0x107. On a gene: not held -
//   sound 0x107, nothing more; three picked - sound 0x106; else the gene
//   (6 * row + column) picked, its flag cleared, 0x4525B0, sound 0x103, and
//   at three the row set to the done line with line 0x57.
//   The message line: on the done line 0x57 with genes picked, 0x58 without;
//   on a gene 0x45 + 6 * row + column when held, else 0x4000.
//   The repeat keys of 0xF000: 0x1000 a row up while it is 0..0x7F, 0x4000 a
//   row down while it is below 2 (signed: from 0xFF to 0); off the done line
//   0x2000 / 0x8000 a column right below 5 / left above 0; sound 0x101 each.
extern "C" void __cdecl DragonCmd_GenesPick(void) {
    {
        unsigned id = 0x4000;
        const unsigned char row = BattleE5_GeneRow;
        if (row != 0xFF) {
            const unsigned col = BattleE1_Roster;
            if (B(::bof3::Img(battle_e5::at::kGeneFlags.v) + col + 6u * row) != 0) id = (col + 6u * row + 0x4172) & 0xFFFF;
        }
        const unsigned char* const text = Line(id);
        BH_CALL(BattleBanner_Set)(0, 1, 1, 0, 0xFF, reinterpret_cast<const char*>(text));
    }
    const U pressed = Pressed();
    bool shown = false;
    if (Cancel(pressed)) {
        const unsigned char n = BattleE5_GeneCount;
        if (n != 0) {
            const unsigned char last = static_cast<unsigned char>(n - 1);
            BattleE5_GeneCount = last;
            B(::bof3::Img(battle_e5::at::kGeneFlags.v) + B(::bof3::Img(battle_e5::at::kGenes.v) + last)) = 1;
            BH_AT(Void0, battle_e5::at::kDragonTask.v)();
            Sound(0x106);
            return;
        }
        const unsigned char pick = static_cast<unsigned char>(BossSb_Countdown & 0x7F);
        B(::bof3::Img(battle_e5::at::kWin17.v) + 3) = 1;
        WindowRecords[4].state = 2;
        BossSb_Countdown = pick;
        BattleE4_Step3 = 2;
        BattleE4_Step4 = 0;
        Sound(0x106);
        return;
    }
    if (Confirm(pressed)) {
        const unsigned char row = BattleE5_GeneRow;
        if (row == 0xFF) {
            if (BattleE5_GeneCount != 0 && rv::As<records::PartyMember>(Member())->ap >= BattleActions_ApCost) {
                NextStep();
                return;
            }
            Sound(0x107);
        } else {
            const unsigned char col = BattleE1_Roster;
            unsigned char* const flag = move_script::At(::bof3::Img(battle_e5::at::kGeneFlags.v) + col + 6u * row);
            if (*flag == 0) {
                Sound(0x107);
                return;
            }
            const unsigned char n = BattleE5_GeneCount;
            if (n >= 3) {
                Sound(0x106);
            } else {
                *flag = 0;
                B(::bof3::Img(battle_e5::at::kGenes.v) + n) = static_cast<unsigned char>(row * 6 + col);
                BattleE5_GeneCount = static_cast<unsigned char>(BattleE5_GeneCount + 1);
                BH_AT(Void0, battle_e5::at::kDragonTask.v)();
                Sound(0x103);
                if (BattleE5_GeneCount == 3) {
                    BattleE5_GeneRow = 0xFF;
                    ShowLine(Line(0x57));
                    shown = true;
                }
            }
        }
    }
    if (!shown) {
        const unsigned char row = BattleE5_GeneRow;
        unsigned id;
        if (row == 0xFF) {
            id = BattleE5_GeneCount != 0 ? 0x57 : 0x58;
        } else {
            const unsigned char col = BattleE1_Roster;
            id = B(::bof3::Img(battle_e5::at::kGeneFlags.v) + col + 6u * row) != 0 ? (col + 6u * row + 0x45) & 0xFFFF : 0x4000;
        }
        ShowLine(Line(id));
    }
    const U keys = Repeat(0xF000);
    if ((keys & 0x1000) && static_cast<signed char>(BattleE5_GeneRow) > -1) {
        Sound(0x101);
        BattleE5_GeneRow = static_cast<unsigned char>(BattleE5_GeneRow - 1);
    }
    if ((keys & 0x4000) && static_cast<signed char>(BattleE5_GeneRow) < 2) {
        Sound(0x101);
        BattleE5_GeneRow = static_cast<unsigned char>(BattleE5_GeneRow + 1);
    }
    if (BattleE5_GeneRow == 0xFF) return;
    if ((keys & 0x2000) && BattleE1_Roster < 5) {
        Sound(0x101);
        BattleE1_Roster = static_cast<unsigned char>(BattleE1_Roster + 1);
    }
    if ((keys & 0x8000) && BattleE1_Roster != 0) {
        Sound(0x101);
        BattleE1_Roster = static_cast<unsigned char>(BattleE1_Roster - 1);
    }
}

// original 0x450A30 (0x64ED20[2]; PSX 0x800A2784): sound 0x104, window 21's +3 = 2, the
// member's +0x134 |= 4, the next step.
extern "C" void __cdecl DragonCmd_GenesConfirm(void) {
    Sound(0x104);
    auto* const member_rec = rv::As<records::PartyMember>(Member());
    B(::bof3::Img(battle_e5::at::kWin21.v) + 3) = 2;
    member_rec->flags_134 = static_cast<S32BattleE5>(static_cast<U>(static_cast<std::int32_t>(member_rec->flags_134)) | 4u);
    NextStep();
}

// original 0x450A70 (0x64ED20[3]; PSX 0x800A27DC): window 21 closed: 0x447F40, windows 17, 21,
// 16, 19's +0 zeroed, LoadDatFile(0xCF), the next step.
extern "C" void __cdecl DragonCmd_GenesClose(void) {
    if (WindowRecords[21].in_use & 1) return;
    BH_AT(Void0, battle_e5::at::kTargetPrompt.v)();
    WindowRecords[17].in_use = 0;
    WindowRecords[21].in_use = 0;
    WindowRecords[16].in_use = 0;
    WindowRecords[19].in_use = 0;
    BH_CALL(LoadDatFile)(0xCF);
    NextStep();
}

// original 0x450AB0 (0x64ED20[4]; PSX 0x800A285C): File_LoadDone: CLUT row 0x1B copied; the
// command record 0x939FA0's +1 = 4, Gfx_ClutStripDirty = 1, 0x904AAF = 0, the
// menu actor's +1 = 2, 0x904AC3 + 1, the phase's step 0x904AA1 = 1 and
// 0x904AA2..AA4 zeroed, the message window's last line's +1 byte = 1.
extern "C" void __cdecl DragonCmd_Commit(void) {
    if (BH_CALL(File_LoadDone)() == 0) return;
    BH_CALL(Gfx_ClutStripCopyRow)(0x1B);
    unsigned char* const command = Ptr(::bof3::Img(battle_e5::at::kCommandRecord.v));
    Gfx_ClutStripDirty = 1;
    BattleE4_Picking = 0;
    command[1] = 4;
    rv::As<records::PartyMember>(Member())->state = 2;
    const unsigned char count = static_cast<unsigned char>(static_cast<unsigned char>(BattleActorCopies_EntryCount) + 1);
    BattleActions_Step = 1;
    BattleActorCopies_EntryCount = count;
    const unsigned line = (BattleActorCopies_QueueWrite - 1u) & 0xF;
    BattleE4_Step2 = 0;
    BattleE4_Step3 = 0;
    BattleE4_Step4 = 0;
    B(::bof3::Img(battle_e5::at::kMsgFlag.v) + line * 8) = 1;
}

// original 0x450B20 (0x64ECCC[5]; PSX 0x800A2914): jmp [0x64ED34 + 4 * 0x904AA4], seven steps.
extern "C" unsigned long __cdecl DragonCmd_Slots2Dispatch(unsigned long through) {
    return Dispatch("DragonCmd_Slots2Dispatch", ::bof3::Img(DragonCmd_Slots2Steps_addr), 7, through);
}

// original 0x450B40 (0x64ED34[0]; PSX 0x800A2950): window 21's +3 = 2, +0xA = 1; the next step.
extern "C" void __cdecl DragonCmd_Slots2CloseMenu(void) {
    const unsigned char step = static_cast<unsigned char>(BattleE4_Step4 + 1);
    B(::bof3::Img(battle_e5::at::kWin21.v) + 3) = 2;
    B(::bof3::Img(battle_e5::at::kWin21.v) + 0xA) = 1;
    BattleE4_Step4 = step;
}

// original 0x450B60 (0x64ED34[1]; PSX 0x800A2980): as DragonCmd_SlotsOpen for window 19 (slot
// 0x13; +2 3, at 0x15B) and line 0x63.
extern "C" void __cdecl DragonCmd_Slots2Open(void) { OpenList(0x13, ::bof3::Img(Window19_addr), 3, 0x15B, 0x63); }

// original 0x450BE0 (0x64ED34[2]; PSX 0x800A2A4C): the second list's cursor, once window 19's
// x is 0x5B: as DragonCmd_SlotsCursor without the bit-0x10 branch - cancel:
// sound 0x106, the next step; confirm: DragonCmd_Slot2Affordable - sound
// 0x104, step 5 - else sound 0x107; the cursor in twelve slots (a + b below
// 0xB, a at most 9).
extern "C" void __cdecl DragonCmd_Slots2Cursor(void) {
    if (W(::bof3::Img(battle_e5::at::kWin19.v) + 4) != 0x5B) return;
    const U keys = Repeat(Sigma(0x500C));
    const U pressed = Pressed();
    if (Cancel(pressed)) {
        Sound(0x106);
        NextStep();
        return;
    }
    if (Confirm(pressed)) {
        if (BH_CALL(DragonCmd_Slot2Affordable)() & 0xFF) {
            Sound(0x104);
            BattleE4_Step4 = 5;
            return;
        }
        Sound(0x107);
    }
    MoveCursor(::bof3::Img(Window19_addr), keys, 0xB, 9);
}

// original 0x450D60 (called by DragonCmd_Slots2Cursor; PSX 0x800A2C68): the second list's
// slot at the cursor priced against the member's AP. al out.
extern "C" unsigned char __cdecl DragonCmd_Slot2Affordable(void) { return Affordable(SlotOf(::bof3::Img(battle_e5::at::kSlots2.v), ::bof3::Img(Window19_addr))); }

// original 0x450DF0 (0x64ED34[3]; PSX 0x800A2D48): window 21 claimed again (at 0xFF42, 0x3F,
// +0xA 1), window 19's +3 = 2, the next step.
extern "C" void __cdecl DragonCmd_Slots2Cancel(void) {
    ReopenMenu(0xFF42);
    const unsigned char step = static_cast<unsigned char>(BattleE4_Step4 + 1);
    B(::bof3::Img(battle_e5::at::kWin21.v) + 0xA) = 1;
    B(::bof3::Img(battle_e5::at::kWin19.v) + 3) = 2;
    BattleE4_Step4 = step;
}

// original 0x450E40 (0x64ED34[4]; PSX 0x800A2DBC): window 19 closed: back to the menu.
extern "C" void __cdecl DragonCmd_Slots2Back(void) { BackToMenu(::bof3::Img(Window19_addr)); }

// original 0x450E70 (0x64ED34[5]; PSX 0x800A2E00): the second list's slot taken (TakeSlot),
// 0x4525B0, sound 0x104, window 19's +3 = 2, the member's +0x134 |= 4, the
// next step.
extern "C" void __cdecl DragonCmd_Slots2Confirm(void) {
    TakeSlot(SlotOf(::bof3::Img(battle_e5::at::kSlots2.v), ::bof3::Img(Window19_addr)));
    BH_AT(Void0, battle_e5::at::kDragonTask.v)();
    Sound(0x104);
    auto* const member_rec = rv::As<records::PartyMember>(Member());
    B(::bof3::Img(battle_e5::at::kWin19.v) + 3) = 2;
    member_rec->flags_134 = static_cast<S32BattleE5>(static_cast<U>(static_cast<std::int32_t>(member_rec->flags_134)) | 4u);
    NextStep();
}

// original 0x450EF0 (0x64ED34[6]; PSX 0x800A2EE8): window 18 closed (sic: the first list's
// record is the one tested): 0x447F40, windows 17, 16, 19's +0 zeroed,
// LoadDatFile(0xCF), part 4's step 3.
extern "C" void __cdecl DragonCmd_Slots2Close(void) {
    if (WindowRecords[18].in_use & 1) return;
    BH_AT(Void0, battle_e5::at::kTargetPrompt.v)();
    WindowRecords[17].in_use = 0;
    WindowRecords[16].in_use = 0;
    WindowRecords[19].in_use = 0;
    BH_CALL(LoadDatFile)(0xCF);
    BattleE4_Step3 = 4;
    BattleE4_Step4 = 3;
}

// original 0x450F30 (0x64ECCC[6]; PSX 0x800A2F58): jmp [0x64ED50 + 4 * 0x904AA4], seven steps
// (the last, 0x451480, BE6's).
extern "C" unsigned long __cdecl DragonCmd_StoreDispatch(unsigned long through) {
    return Dispatch("DragonCmd_StoreDispatch", ::bof3::Img(DragonCmd_StoreSteps_addr), 7, through);
}

// original 0x450F50 (0x64ED50[0]; PSX 0x800A2F94): window 18's +3 = 3, window 19 claimed
// (slot 0x13: +3 0, its cursor 0, 0, +2 4, at 0x15B, 0x3F), the next step.
extern "C" void __cdecl DragonCmd_StoreOpen(void) {
    B(::bof3::Img(battle_e5::at::kWin18.v) + 3) = 3;
    BH_CALL(Window_Alloc)(0x13, 5);
    B(::bof3::Img(battle_e5::at::kWin19.v) + 3) = 0;
    SetW(::bof3::Img(battle_e5::at::kWin19.v) + 0x10, 0);
    SetW(::bof3::Img(battle_e5::at::kWin19.v) + 0x12, 0);
    const unsigned char step = static_cast<unsigned char>(BattleE4_Step4 + 1);
    B(::bof3::Img(battle_e5::at::kWin19.v) + 2) = 4;
    SetW(::bof3::Img(battle_e5::at::kWin19.v) + 4, 0x15B);
    SetW(::bof3::Img(battle_e5::at::kWin19.v) + 6, 0x3F);
    BattleE4_Step4 = step;
}

// original 0x450FA0 (0x64ED50[1]; PSX 0x800A3010): window 19's x 0xA3: the next step.
extern "C" void __cdecl DragonCmd_StoreWait(void) {
    if (W(::bof3::Img(battle_e5::at::kWin19.v) + 4) == 0xA3) NextStep();
}

// original 0x450FC0 (0x64ED50[2]; PSX 0x800A3044): the destination's cursor: line 0x61; the
// repeat keys of 0x500C; cancel: sound 0x106, step 5; confirm: step 4 for an
// empty second-list slot, else the prompt's answer 0x904B74 = 0 and step 3,
// sound 0x104 - and the cursor still moves; the cursor in twelve slots.
extern "C" void __cdecl DragonCmd_StoreCursor(void) {
    ShowLine(Line(0x61));
    const U keys = Repeat(Sigma(0x500C));
    const U pressed = Pressed();
    if (Cancel(pressed)) {
        Sound(0x106);
        BattleE4_Step4 = 5;
        return;
    }
    if (Confirm(pressed)) {
        if (B(SlotOf(::bof3::Img(battle_e5::at::kSlots2.v), ::bof3::Img(Window19_addr)) + 3) == 0xFF) {
            BattleE4_Step4 = 4;
        } else {
            SetW(::bof3::Img(battle_e5::at::kAsk.v), 0);
            BattleE4_Step4 = 3;
        }
        Sound(0x104);
    }
    MoveCursor(::bof3::Img(Window19_addr), keys, 0xB, 9);
}

extern "C" void __cdecl DragonCmd_StoreAsk(void) {
    ShowLine(Line(0x62));
    const U keys = Repeat(0xA000);
    const U pressed = Pressed();
    if (Cancel(pressed) || (Confirm(pressed) && W(::bof3::Img(battle_e5::at::kAsk.v)) != 0)) {
        Sound(0x106);
        BattleE4_Step4 = static_cast<unsigned char>(BattleE4_Step4 - 1);
        return;
    }
    if (Confirm(pressed)) {
        Sound(0x104);
        NextStep();
        return;
    }
    if (keys & 0xA000) SetW(::bof3::Img(battle_e5::at::kAsk.v), W(::bof3::Img(battle_e5::at::kAsk.v)) ^ 1u);
    const U ask = L(::bof3::Img(battle_e5::at::kAsk.v));
    const U answer = battle_e5::g_store_ask_word ? (ask & 0xFFFFu) : ask;
    BH_CALL(Menu_DrawHand)(static_cast<int>(answer * 0x22 + 0xD5), 0x18, 0);
}

// original 0x451220 (0x64ED50[4]; PSX 0x800A33F0): the first list's slot at its cursor copied
// into the second list's at its cursor, byte by byte (each read before its
// write, as the original), the next step.
extern "C" void __cdecl DragonCmd_StoreCopy(void) {
    const U from = SlotOf(::bof3::Img(battle_e5::at::kSlots.v), ::bof3::Img(Window18_addr));
    const U to = SlotOf(::bof3::Img(battle_e5::at::kSlots2.v), ::bof3::Img(Window19_addr));
    for (unsigned j = 0; j < 4; ++j) B(to + j) = B(from + j);
    NextStep();
}

// original 0x451290 (0x64ED50[5]): the source's cursor on the first list:
// line 0x60; the repeat keys of 0x500C; cancel: sound 0x106, the next step
// (BE6's 0x451480); confirm (with bit 0x10 of the pressed keys counted as
// one): the slot not empty - sound 0x104, step 2 - else sound 0x107 and
// Input_Pressed read again; then its bit 0x10 the same test once more; the
// cursor in six slots.
extern "C" void __cdecl DragonCmd_StoreSource(void) {
    ShowLine(Line(0x60));
    const U keys = Repeat(Sigma(0x500C));
    U pressed = Pressed();
    if (Cancel(pressed)) {
        Sound(0x106);
        NextStep();
        return;
    }
    // DIVERGENCE DIV-0139: Japan's triangle through σ (0x4512EB `or cl, 0x10`)
    if (((Field_ConfirmButtons | Sigma(0x10)) & pressed & 0xFFFF) != 0) {
        if (B(SlotOf(::bof3::Img(battle_e5::at::kSlots.v), ::bof3::Img(Window18_addr)) + 3) != 0xFF) {
            Sound(0x104);
            BattleE4_Step4 = 2;
            return;
        }
        Sound(0x107);
        pressed = Pressed();
    }
    if (pressed & Sigma(0x10)) {
        if (B(SlotOf(::bof3::Img(battle_e5::at::kSlots.v), ::bof3::Img(Window18_addr)) + 3) != 0xFF) {
            Sound(0x104);
            BattleE4_Step4 = 2;
            return;
        }
        Sound(0x107);
    }
    MoveCursor(::bof3::Img(Window18_addr), keys, 5, 3);
}

// original 0x451480 (PSX 0x800A37D8; the gene history's step table 0x64ED50, entry 6, by
// 0x904AA4): windows 18 and 19's +3 = 4 and 2, 0x904AA3 = 3, 0x904AA4 = 2.
extern "C" void __cdecl DragonHistory_Leave(void) {
    WindowRecords[18].state = 4;
    WindowRecords[19].state = 2;
    BattleE4_Step3 = 3;
    BattleE4_Step4 = 2;
}

// original 0x4514A0 (called by Accession_Start 0x4EAEA0): the actor's
// record's +0x138 / +0x13C = 0, +0x130 &= ~0x18000, +0x134 &= 0xFFFB84FF;
// Battle_RecalcStats(actor); 0x675F56 / 57 = 0; +0x134 |= 2 (the actor read
// again). A recipe (DragonForm_FindRecipe): DragonForm_ApplyRecipe(it), the
// form code = it + 4 into 0x675F56, and +0x134 |= 0x10000 for code 8,
// 0x20000 for 7 and 4 (the actor read again). None: DragonForm_Mix, the code
// read back. Then the history's five older records move down one, the genes
// (0x904B87 of them) copied into record 0 and the rest of its three 0xFF;
// 0x904B79 = the group code, record 0's byte 3 = (group << 5) + (code &
// 0x1F), 0x904B89 = the code; Battle_RecalcStats(the actor read again).
extern "C" void __cdecl DragonForm_Transform(void) {
    {
        unsigned char* const member = PartyBattleE6(Actor());
        auto* const member_rec = rv::As<records::PartyMember>(member);
        const unsigned who = Actor();
        SetL(member + 0x138, 0);
        SetL(member + 0x13C, 0);
        member_rec->flags_130 = static_cast<U>(member_rec->flags_130) & 0xFFFE7FFFu;
        member_rec->flags_134 = static_cast<U>(member_rec->flags_134) & 0xFFFB84FFu;
        BH_CALL(Battle_RecalcStats)(who);
    }
    auto* const member_rec = rv::As<records::PartyMember>(PartyBattleE6(Actor()));
    BattleE6_FormCode = 0;
    BattleE6_FormGroupWork = 0;
    member_rec->flags_134 = static_cast<U>(member_rec->flags_134) | 2;
    unsigned char form = BH_CALL(DragonForm_FindRecipe)();
    if (form != 0xFF) {
        BH_CALL(DragonForm_ApplyRecipe)(form);
        form = static_cast<unsigned char>(form + 4);
        BattleE6_FormCode = form;
        if (form == 8) {
            auto* const member_rec = rv::As<records::PartyMember>(PartyBattleE6(Actor()));
            member_rec->flags_134 = static_cast<U>(member_rec->flags_134) | 0x10000;
        } else if (form == 7 || form == 4) {
            auto* const member_rec = rv::As<records::PartyMember>(PartyBattleE6(Actor()));
            member_rec->flags_134 = static_cast<U>(member_rec->flags_134) | 0x20000;
        }
    } else {
        BH_CALL(DragonForm_Mix)(0xFF);
        form = BattleE6_FormCode;
    }
    for (U record = ::bof3::Img(battle_e6::at::kHistoryLast.v); record > ::bof3::Img(battle_e6::at::kHistory.v); record -= 4)
        for (U i = 0; i < 4; ++i) B(record + i) = B(record - 4 + i);
    const unsigned count = BattleE5_GeneCount;
    unsigned kept = 0;
    if (count > 0) {
        for (unsigned i = 0; i < count; ++i) B(::bof3::Img(battle_e6::at::kHistory.v) + i) = B(::bof3::Img(battle_e6::at::kGenes.v) + i);
        kept = count;
    }
    for (unsigned i = kept; i < 3; ++i) B(::bof3::Img(battle_e6::at::kHistory.v) + i) = 0xFF;
    const unsigned char group = BattleE6_FormGroupWork;
    BattleE6_FormGroup = group;
    const unsigned who = Actor();
    B(::bof3::Img(battle_e6::at::kHistoryForm.v)) = static_cast<unsigned char>((group << 5) + (form & 0x1F));
    BattlePhases_B89 = form;
    BH_CALL(Battle_RecalcStats)(who);
}

// original 0x451670 (PSX 0x800A3B94): the first recipe 0..10 whose three genes the actor holds
// (DragonForm_RecipeSlotHeld for each), its group code (0x64ED6C) into
// 0x675F57, the recipe the answer; at 6, DragonForm_TryPartyRecipe first -
// its answer when not 0, else recipe 6 is skipped. 0xFF for none.
extern "C" unsigned char __cdecl DragonForm_FindRecipe(void) {
    unsigned recipe = 0;
    for (;;) {
        if (recipe == 6) {
            const unsigned char party = BH_CALL(DragonForm_TryPartyRecipe)();
            if (party != 0) return party;
            recipe = 7;
        }
        unsigned slot = 0;
        while (slot < 3 && BH_CALL(DragonForm_RecipeSlotHeld)(recipe, slot) != 0) ++slot;
        if (slot == 3) {
            BattleE6_FormGroupWork = move_script::At(::bof3::Img(battle_e6::at::kRecipeGroups.v))[recipe];
            return static_cast<unsigned char>(recipe);
        }
        if (++recipe > 10) return 0xFF;
    }
}

// original 0x4516F0 (PSX 0x800A3C60): recipe `recipe`'s gene `slot` (0x64ED78 + 3 recipe +
// slot): 1 when it is 0xFF (any) or among the chosen genes, else 0.
extern "C" unsigned char __cdecl DragonForm_RecipeSlotHeld(unsigned recipe, unsigned slot) {
    const unsigned char gene = move_script::At(::bof3::Img(battle_e6::at::kRecipeGenes.v))[(recipe & 0xFF) * 3 + (slot & 0xFF)];
    if (gene == 0xFF) return 1;
    const unsigned count = BattleE5_GeneCount;
    for (unsigned i = 0; i < count; ++i)
        if (B(::bof3::Img(battle_e6::at::kGenes.v) + i) == gene) return 1;
    return 0;
}

// original 0x451750 (PSX 0x800A3CEC): when a chosen gene is 0x10 and the party's size byte
// 0x904AB1 (read once) is 3, DragonForm_PartyRecipe's answer; else 0.
extern "C" unsigned char __cdecl DragonForm_TryPartyRecipe(void) {
    const unsigned count = BattleE5_GeneCount;
    if (count == 0) return 0;
    const unsigned char size = Battle_PartyStanding;
    for (unsigned i = 0; i < count; ++i)
        if (B(::bof3::Img(battle_e6::at::kGenes.v) + i) == 0x10 && size == 3) return BH_CALL(DragonForm_PartyRecipe)();
    return 0;
}

// original 0x4517A0 (its word unread): the mix. The party records backed up;
// the 14 sums 0x675F48.. zeroed, each chosen gene but 0xA / 0xB / 0xC adding
// its row (0x64ED9C + 14 gene), each 0xB negating the first ten, each 0xA
// doubling all fourteen; each 0xC moving every sum by one either way with
// Rand (a zero one time in ten, another half the time; the count read again
// after). The form code 3 / 2 / 1 / 0 by sums 13, 11 (at least 1) and 10 (at
// least 2); five stats from sums 5..9 through DragonForm_StatShift and the
// code's column of 0x64EE98 (the first as FirstStat with 1 below 1, the
// others through 0x446F20 with 0 below 1, each change kept at 0x939EE2 ..);
// the group code the last of sums 0..4 above 0 (1..5), 6 for two or more;
// DragonForm_SetMixBytes(sums 0..4), DragonForm_MixAbilities.
extern "C" void __cdecl DragonForm_Mix(unsigned) {
    BackUpParty();
    unsigned char* const member = PartyBattleE6(Actor());
    auto* const member_rec = rv::As<records::PartyMember>(member);
    const unsigned char* const character = move_script::At(::bof3::Img(CharacterRecords_addr) + member_rec->char_index * battle_e6::at::kCharStride);
    static_assert(battle_e6::at::kCharStride == sizeof(records::CharRecord));
    unsigned char* const sums = move_script::At(::bof3::Img(battle_e6::at::kMixSums.v));
    for (unsigned i = 0; i < 14; ++i) sums[i] = 0;
    const unsigned count = BattleE5_GeneCount;
    if (count > 0) {
        for (unsigned k = 0; k < count; ++k) {
            const unsigned char gene = B(::bof3::Img(battle_e6::at::kGenes.v) + k);
            if (gene == 0xA || gene == 0xB || gene == 0xC) continue;
            for (unsigned i = 0; i < 14; ++i) sums[i] = static_cast<unsigned char>(sums[i] + move_script::At(::bof3::Img(battle_e6::at::kGeneRows.v) + gene * 14u)[i]);
        }
        for (unsigned k = 0; k < count; ++k)
            if (B(::bof3::Img(battle_e6::at::kGenes.v) + k) == 0xB)
                for (unsigned i = 0; i < 10; ++i) sums[i] = static_cast<unsigned char>(-sums[i]);
        for (unsigned k = 0; k < count; ++k)
            if (B(::bof3::Img(battle_e6::at::kGenes.v) + k) == 0xA)
                for (unsigned i = 0; i < 14; ++i) sums[i] = static_cast<unsigned char>(sums[i] << 1);
    }
    unsigned limit = count;
    for (unsigned k = 0; k < limit; ++k) {
        if (B(::bof3::Img(battle_e6::at::kGenes.v) + k) != 0xC) continue;
        for (unsigned i = 0; i < 14; ++i) {
            const bool move = sums[i] == 0 ? BH_CALL(Rand)() % 10 == 0 : (BH_CALL(Rand)() & 1) != 0;
            if (!move) continue;
            const int way = BH_CALL(Rand)();
            const unsigned char v = sums[i];
            sums[i] = static_cast<unsigned char>(way & 1 ? v + 1 : v - 1);
        }
        limit = BattleE5_GeneCount;
    }
    const unsigned char s13 = sums[13];
    BattleE6_FormCode = 0;
    if (S8(s13) >= 1) BattleE6_FormCode = 3;
    else if (S8(sums[11]) >= 1) BattleE6_FormCode = 2;
    else if (S8(sums[10]) >= 2) BattleE6_FormCode = 1;

    // the first stat: 1 below 1
    {
        const unsigned char shift = BH_CALL(DragonForm_StatShift)(sums[5]);
        const unsigned column = move_script::At(::bof3::Img(battle_e6::at::kShiftColumns.v))[BattleE6_FormCode * 5];
        const auto sum = static_cast<std::uint16_t>(S8(shift) + static_cast<std::int32_t>(column));
        std::uint16_t value = 1;
        if (S16BattleE6(sum) > 0) value = static_cast<std::uint16_t>(Percent9999(rv::As<records::CharRecord>(character)->base.max_hp, S16BattleE6(sum) * 10));
        FirstStat(member, character, value);
    }
    // the other four: 0 below 1; the old value's cell takes the change
    const U deltas[4] = {::bof3::Img(battle_e6::at::kStatDeltas.v) + 2, ::bof3::Img(battle_e6::at::kStatDeltas.v) + 4, ::bof3::Img(battle_e6::at::kStatDeltas.v) + 6, ::bof3::Img(battle_e6::at::kStatDeltas.v) + 8};
    for (unsigned n = 0; n < 4; ++n) {
        const unsigned field = 0xC4 + 2 * n;
        const unsigned char shift = BH_CALL(DragonForm_StatShift)(sums[6 + n]);
        const unsigned column = move_script::At(::bof3::Img(battle_e6::at::kShiftColumns.v))[BattleE6_FormCode * 5 + 1 + n];
        const auto sum = static_cast<std::uint16_t>(S8(shift) + static_cast<std::int32_t>(column));
        std::uint16_t value = 0;
        if (S16BattleE6(sum) > 0) value = static_cast<std::uint16_t>(Percent999(move_script::Word(member + field), S16BattleE6(sum) * 10));
        move_script::SetWord(move_script::At(deltas[n]), static_cast<std::uint16_t>(value - move_script::Word(move_script::At(deltas[n]))));
        const std::uint16_t next = n < 3 ? move_script::Word(member + field + 2) : 0;
        move_script::SetWord(member + field, value);
        move_script::SetWord(member + field - 0x20, value);
        if (n < 3) move_script::SetWord(move_script::At(deltas[n + 1]), next);
    }
    unsigned char positive = 0;
    BattleE6_FormGroupWork = 0;
    for (unsigned i = 0; i < 5; ++i)
        if (S8(sums[i]) >= 1) {
            ++positive;
            BattleE6_FormGroupWork = static_cast<unsigned char>(i + 1);
        }
    if (positive >= 2) BattleE6_FormGroupWork = 6;
    BH_CALL(DragonForm_SetMixBytes)(sums[0], sums[1], sums[2], sums[3], sums[4]);
    BH_CALL(DragonForm_MixAbilities)();
}

// original 0x451C50 (PSX 0x800A45B0): 0x64EEAC's step by a signed sum + 2, held to 0..4.
extern "C" unsigned char __cdecl DragonForm_StatShift(unsigned sum) {
    std::int32_t i = S8(static_cast<unsigned char>(sum + 2));
    if (i < 0) i = 0;
    else if (i > 4) i = 4;
    return move_script::At(::bof3::Img(battle_e6::at::kShiftSteps.v))[i];
}

// original 0x451C80 (PSX 0x800A45FC): the actor's nine form bytes (+0xCF .. +0xD7, mirrored
// at +0xAF .., kept at 0x939EEA ..): sums 0, 1, 2 through 0x64EEB4, 2, 2, sum
// 4 through 0x64EEBE, 2, sum 3 through 0x64EEB9 twice - each by the sum + 2
// held to 0..4. Answers 0.
extern "C" unsigned char __cdecl DragonForm_SetMixBytes(unsigned s0, unsigned s1, unsigned s2, unsigned s3, unsigned s4) {
    unsigned char* const record = PartyBattleE6(Actor());
    MixByte(record, 0x4F, ::bof3::Img(battle_e6::at::kStatDeltas.v) + 0xA, move_script::At(::bof3::Img(battle_e6::at::kMixBytesA.v))[Clamp4(s0)]);
    MixByte(record, 0x50, ::bof3::Img(battle_e6::at::kStatDeltas.v) + 0xB, move_script::At(::bof3::Img(battle_e6::at::kMixBytesA.v))[Clamp4(s1)]);
    MixByte(record, 0x51, ::bof3::Img(battle_e6::at::kStatDeltas.v) + 0xC, move_script::At(::bof3::Img(battle_e6::at::kMixBytesA.v))[Clamp4(s2)]);
    MixByte(record, 0x52, ::bof3::Img(battle_e6::at::kStatDeltas.v) + 0xD, 2);
    MixByte(record, 0x53, ::bof3::Img(battle_e6::at::kStatDeltas.v) + 0xE, 2);
    MixByte(record, 0x54, ::bof3::Img(battle_e6::at::kStatDeltas.v) + 0xF, move_script::At(::bof3::Img(battle_e6::at::kMixBytesC.v))[Clamp4(s4)]);
    MixByte(record, 0x55, ::bof3::Img(battle_e6::at::kStatDeltas.v) + 0x10, 2);
    const unsigned third = Clamp4(s3);
    MixByte(record, 0x56, ::bof3::Img(battle_e6::at::kStatDeltas.v) + 0x11, move_script::At(::bof3::Img(battle_e6::at::kMixBytesB.v))[third]);
    MixByte(record, 0x57, ::bof3::Img(battle_e6::at::kStatDeltas.v) + 0x12, move_script::At(::bof3::Img(battle_e6::at::kMixBytesB.v))[third]);
    return 0;
}

// original 0x451DA0 (PSX 0x800A4820): the mixed form's abilities into the actor's list
// (+0xF4, ten bytes): the form code's three of 0x64EEC4 to the first 0, then
// DragonForm_AddAbilityRow by the sums - 0 (and 5, 14 by sums 9 and 3), 1
// (6), 2 (7), 3 (8), 4 (9), 12, 11, 13 - then 0xD9 and zeros.
extern "C" void __cdecl DragonForm_MixAbilities(void) {
    unsigned char* const list = PartyBattleE6(Actor()) + 0xF4;
    unsigned char count = 0;
    for (unsigned j = 0; j < 3; ++j) {
        const unsigned char ability = move_script::At(::bof3::Img(battle_e6::at::kFormAbilities.v))[BattleE6_FormCode * 3u + j];
        if (ability == 0) break;
        list[count++] = ability;
    }
    auto sum = [](unsigned i) { return S8(move_script::At(::bof3::Img(battle_e6::at::kMixSums.v))[i]); };
    auto add = [&count](unsigned row) { count = BH_CALL(DragonForm_AddAbilityRow)(row, count); };
    if (sum(0) >= 1) {
        add(0);
        if (sum(9) >= 1) add(5);
        if (sum(3) >= 1) add(0xE);
    }
    if (sum(1) >= 1) {
        add(1);
        if (sum(9) >= 1) add(6);
    }
    if (sum(2) >= 1) {
        add(2);
        if (sum(9) >= 1) add(7);
    }
    if (sum(3) >= 1) {
        add(3);
        if (sum(9) >= 1) add(8);
    }
    if (sum(4) >= 1) {
        add(4);
        if (sum(9) >= 1) add(9);
    }
    if (sum(12) >= 1) add(0xC);
    if (sum(11) >= 1) add(0xB);
    if (sum(9) >= 1) add(0xD);
    EndAbilities(list, count);
}

// original 0x451FE0 (PSX 0x800A4AA8): row `row`'s three abilities (0x64EED0 + 3 row) added to
// the actor's list at `count` (a 0 ends the row; one already there skipped;
// at 9 the answer is 9), the new count the answer.
extern "C" unsigned char __cdecl DragonForm_AddAbilityRow(unsigned row, unsigned count) {
    unsigned char* const list = PartyBattleE6(Actor()) + 0xF4;
    const unsigned base = (row & 0xFF) * 3;
    auto n = static_cast<unsigned char>(count);
    for (unsigned k = 0; k < 3; ++k) {
        const unsigned char ability = move_script::At(::bof3::Img(battle_e6::at::kAbilityRows.v))[base + k];
        if (ability == 0) return n;
        bool held = false;
        for (unsigned char d = 0; d < n; ++d)
            if (list[d] == ability) {
                held = true;
                break;
            }
        if (held) continue;
        if (n == 9) return 9;
        list[n] = ability;
        n = static_cast<unsigned char>(n + 1);
    }
    return n;
}

// original 0x452080: a recipe's form (0..20). The party backed up; the first
// stat by the recipe's percent of the character's +0x40 (FirstStat), four more
// through 0x446F20 (RecipeStat); nine bytes into +0xCF.., +0xAF.. and
// 0x939EEA..; its abilities (0x64F028 + 8 recipe, to a 0, eight at most),
// then 0xD9 and zeros.
extern "C" void __cdecl DragonForm_ApplyRecipe(unsigned recipe) {
    BackUpParty();
    unsigned char* const member = PartyBattleE6(Actor());
    auto* const member_rec = rv::As<records::PartyMember>(member);
    const unsigned r = recipe & 0xFF;
    const unsigned char* const row = move_script::At(::bof3::Img(battle_e6::at::kRecipeStats.v) + r * 14);
    const unsigned char* const character = move_script::At(::bof3::Img(CharacterRecords_addr) + member_rec->char_index * battle_e6::at::kCharStride);
    static_assert(battle_e6::at::kCharStride == sizeof(records::CharRecord));
    FirstStat(member, character,
              static_cast<std::uint16_t>(Percent9999(rv::As<records::CharRecord>(character)->base.max_hp, static_cast<std::int32_t>(row[0]) * 10)));
    // RecipeStat's first store is the old value: the first stat left it at 0x939EE2 already
    {
        const auto v = static_cast<std::uint16_t>(Percent999(member_rec->base_atk, static_cast<std::int32_t>(row[1]) * 10));
        rv::Store<unsigned>(member_rec->base_atk, v);
        member_rec->atk = v;
        move_script::SetWord(move_script::At(::bof3::Img(battle_e6::at::kStatDeltas.v) + 2), static_cast<std::uint16_t>(v - move_script::Word(move_script::At(::bof3::Img(battle_e6::at::kStatDeltas.v) + 2))));
    }
    RecipeStat(member, 0xC6, row[2], ::bof3::Img(battle_e6::at::kStatDeltas.v) + 4);
    RecipeStat(member, 0xC8, row[3], ::bof3::Img(battle_e6::at::kStatDeltas.v) + 6);
    RecipeStat(member, 0xCA, row[4], ::bof3::Img(battle_e6::at::kStatDeltas.v) + 8);
    for (unsigned i = 0; i < 9; ++i) MixByte(member, 0x4F + i, ::bof3::Img(battle_e6::at::kStatDeltas.v) + 0xA + i, row[5 + i]);
    unsigned char* const list = member + 0xF4;
    unsigned char n = 0;
    for (;;) {
        const unsigned char ability = move_script::At(::bof3::Img(battle_e6::at::kRecipeAbilities.v))[n + r * 8];
        if (ability == 0) break;
        list[n] = ability;
        n = static_cast<unsigned char>(n + 1);
        if (n >= 8) break;
    }
    EndAbilities(list, n);
}

// original 0x4523C0 (PSX 0x800A4FC4): the other members' +0x89 bytes (not the actor, not out
// by Battle_ActorIsOut), the first two as a pair tried both ways round: 1 or
// 8 with 2 or 6 - gene 7 held ? 0xC : 0xB; 2 with 4 or 5 - 0; 4 with 5, 8 or
// 1 - gene 5 ? 0x12 : 0x11; 5 with 8, 1 or 6 - gene 0xD ? 0x14 : 0x13; 6 with
// 2 or 4 - gene 3 ? 0xE : 0xD (a held gene sets the group code 1); else
// 0xFF. With fewer than two members found the original reads stack bytes it
// never wrote (docs/battle_e6.md section 7, L1); ours answers 0, the answer
// the failing pairings give - no party form, recipe 6 skipped (DIV-0063).
extern "C" unsigned char __cdecl DragonForm_PartyRecipe(void) {
    unsigned char found[3] = {};
    unsigned n = 0;
    for (unsigned m = 0; m < 3; ++m) {
        if (B(::bof3::Img(battle_e6::at::kActor.v)) == m) continue;
        if (BH_CALL(Battle_ActorIsOut)(m) != 0) continue;
        found[n++] = rv::As<records::PartyMember>(PartyBattleE6(m))->char_id;
    }
    if (n < 2) return 0;   // DIV-0063: no pair to try
    unsigned char key = found[0], other = found[1];
    for (int pass = 0; pass < 2; ++pass) {
        switch (key) {
        case 1:
        case 8:
            if (other == 2 || other == 6) return PartyPick(7, 0xC, 0xB);
            break;
        case 2:
            if (other == 4 || other == 5) return 0;
            break;
        case 4:
            if (other == 5 || other == 8 || other == 1) return PartyPick(5, 0x12, 0x11);
            break;
        case 5:
            if (other == 8 || other == 1 || other == 6) return PartyPick(0xD, 0x14, 0x13);
            break;
        case 6:
            if (other == 2 || other == 4) return PartyPick(3, 0xE, 0xD);
            break;
        default:
            break;
        }
        const unsigned char was = key;
        key = other;
        other = was;
    }
    return 0xFF;
}

// original 0x452570: 1 when `gene` is among the chosen genes, else 0.
extern "C" unsigned char __cdecl DragonGenes_Held(unsigned gene) {
    const unsigned count = BattleE5_GeneCount;
    for (unsigned i = 0; i < count; ++i)
        if (B(::bof3::Img(battle_e6::at::kGenes.v) + i) == static_cast<unsigned char>(gene)) return 1;
    return 0;
}

// original 0x4525B0 (PSX 0x800A5260; called by BE5's Dragon run steps 0x44FFA0, 0x450610,
// 0x450700, 0x450E70): 0x904B78 = the chosen genes' costs (0x64EC9C) summed,
// a byte, stored after each.
extern "C" void __cdecl DragonGenes_SumCost(void) {
    const unsigned count = BattleE5_GeneCount;
    unsigned char sum = 0;
    BattleActions_ApCost = 0;
    for (unsigned i = 0; i < count; ++i) {
        sum = static_cast<unsigned char>(sum + move_script::At(::bof3::Img(battle_e6::at::kGeneCosts.v))[B(::bof3::Img(battle_e6::at::kGenes.v) + i)]);
        BattleActions_ApCost = sum;
    }
}

// original 0x4525F0 (Magic_Rows row 108): phase +1 through a two-entry stack
// table - RestoreForm_Start, RestoreForm_Wait.
MENGINE_EXPORT void __cdecl RestoreForm_Task(void) {
    static constexpr std::uint32_t kPhases[2] = {bof3::addr::RestoreForm_Start, bof3::addr::RestoreForm_Wait};
    DispatchMagicEngine("RestoreForm_Task", kPhases, 2, Sprite_Current[1]);
}

// original 0x452620: the acting actor's (0x904B34) party record gets state 6,
// sub-states 4 and 4 and +4 = 0 - the member's own state-6 handler 0x441A10,
// entry 4 of its table (0x442080); the phase on.
//
// As the original has it: the actor indexes the party records (0x802D40 +
// 0x14C actor), unchecked - an enemy actor writes past the three members.
MENGINE_EXPORT void __cdecl RestoreForm_Start(void) {
    unsigned char* const member = PartyOf(Mem(::bof3::Img(magic_harness::at::kActor))[0]);
    member[1] = kReacting;
    member[2] = 4;
    member[3] = 4;
    member[4] = 0;
    Bump(Sprite_Current[1]);
}

// original 0x452660: nothing while any actor is pending (the word 0x904B82);
// then the effect-done bit and the slot freed (a tail jmp).
MENGINE_EXPORT void __cdecl RestoreForm_Wait(void) {
    if (move_script::Word(Mem(::bof3::Img(BattleActions_Pending_addr))) != 0) return;
    SetDone();
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x452680: slot 15 (a copy of the acting member, 0x442E60's task):
// through BattleFxDash_Steps by +1 (7).
extern "C" void __cdecl BattleFxDash_Dispatch(void) { TaskDispatch("BattleFxDash_Dispatch", ::bof3::Img(BattleFxDash_Steps_addr), 7); }

// original 0x4526B0: state 0: +0x20 = +0x3C + 0x4000000; pose +8 + 0x38;
// +0xB = 0x10; on.
extern "C" void __cdecl BattleFxDash_Start(void) {
    unsigned char* const s = Sprite_Current;
    rv::Store<U>(rv::As<records::BattleTask>(s)->work_20, static_cast<U>(rv::As<records::BattleTask>(s)->height) + 0x4000000u);
    BH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>(Sprite_Current[8] + 0x38));
    Sprite_Current[0xB] = 0x10;
    rv::Current<records::BattleTask>()->state = static_cast<unsigned char>(rv::Current<records::BattleTask>()->state + 1);
}

// original 0x4526F0: state 1: when BattleObj_ScriptTickOnce answers, pose
// +8 + 0x3C and on.
extern "C" void __cdecl BattleFxDash_WaitPose(void) {
    if (BH_CALL(BattleObj_ScriptTickOnce)() == 0) return;
    BH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>(Sprite_Current[8] + 0x3C));
    rv::Current<records::BattleTask>()->state = static_cast<unsigned char>(rv::Current<records::BattleTask>()->state + 1);
}

// original 0x452720: state 2: +0x3C up 0x1000000 while below +0x20; there,
// x and z at the target's side (Field_Kind2X / _Kind2Z + the pair of
// 0x64E4F4 by 0x904AAC - its other pair, and +8 ^= 2, when the target is a
// member), pose +8 + 0x40, on. A tail jump to BattleObj_ScriptTickOnce.
extern "C" unsigned char __cdecl BattleFxDash_Rise(void) {
    unsigned char* const s = Sprite_Current;
    auto* const cur_task = rv::As<records::BattleTask>(s);
    const auto z = cur_task->height;
    if (z < cur_task->work_20) {
        rv::Store<U>(cur_task->height, static_cast<U>(z) + 0x1000000u);
    } else {
        unsigned side;
        if (B(::bof3::Img(battle_e6::at::kTarget.v)) >= 3) {
            side = B(::bof3::Img(battle_e6::at::kFormTable.v));
            cur_task->x = static_cast<U>(S8(move_script::At(::bof3::Img(battle_e6::at::kDashSides.v))[side * 2])) + static_cast<std::uint32_t>(Field_Kind2X);
            side = B(::bof3::Img(battle_e6::at::kFormTable.v));
        } else {
            s[8] ^= 2;
            side = B(::bof3::Img(battle_e6::at::kFormTable.v)) ^ 2u;
            rv::Current<records::BattleTask>()->x = static_cast<U>(S8(move_script::At(::bof3::Img(battle_e6::at::kDashSides.v))[side * 2])) + static_cast<std::uint32_t>(Field_Kind2X);
            side = B(::bof3::Img(battle_e6::at::kFormTable.v)) ^ 2u;
        }
        rv::Store<U>(rv::Current<records::BattleTask>()->z, static_cast<U>(S8(move_script::At(::bof3::Img(battle_e6::at::kDashSides.v))[side * 2 + 1])) + static_cast<std::uint32_t>(Field_Kind2Z));
        BH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>(Sprite_Current[8] + 0x40));
        rv::Current<records::BattleTask>()->state = static_cast<unsigned char>(rv::Current<records::BattleTask>()->state + 1);
    }
    return BH_CALL(BattleObj_ScriptTickOnce)();
}

// original 0x4527F0: state 3: a trail task (BattleTask_Create(0, 0x12), slot
// 18) copied from this slot's first 0x80 bytes with +0 bit 6 clear, +1..+4 /
// +6 = 0, +5 = 0x12, +0x80 = this slot, +0xB = this +0xB (this +0xB down one
// when not 0); a step toward the target (MagicFx_StepToward 0x50); near it
// (MagicFx_NearSprite3D 0x10000, the target read again), pose +8 + 0x44,
// Battle_SetTargetFlag40(target), on. BattleObj_ScriptTickOnce's answer.
extern "C" unsigned char __cdecl BattleFxDash_Advance(void) {
    const unsigned char slot = BH_CALL(BattleTask_Create)(0, 0x12);
    if (slot != 0xFF) {
        unsigned char* const s = Sprite_Current;
        unsigned char* const t = move_script::At(::bof3::Img(BattleTask_Slots_addr) + slot * battle_e6::at::kTaskStride);
        auto* const task_rec = rv::As<records::BattleTask>(t);
        for (U i = 0; i < 0x80; i += 4) SetL(t + i, LBattleE6(s + i));
        task_rec->flags &= 0xBF;
        task_rec->runner = 0;
        task_rec->kind = 0x12;
        task_rec->state = 0;
        t[2] = 0;
        t[3] = 0;
        t[4] = 0;
        task_rec->owner = Addr(s);
        t[0xB] = s[0xB];
        if (s[0xB] != 0) s[0xB] = static_cast<unsigned char>(s[0xB] - 1);
    }
    const unsigned target = B(::bof3::Img(battle_e6::at::kTarget.v));
    int is_near;
    if (target <= 2) {
        BH_CALL(MagicFx_StepToward)(PartyBattleE6(target), 0x50);
        is_near = BH_CALL(MagicFx_NearSprite3D)(PartyBattleE6(B(::bof3::Img(battle_e6::at::kTarget.v))), 0x10000);
    } else {
        BH_CALL(MagicFx_StepToward)(EnemyBattleE6(target - 3), 0x50);
        is_near = BH_CALL(MagicFx_NearSprite3D)(EnemyBattleE6(B(::bof3::Img(battle_e6::at::kTarget.v)) - 3u), 0x10000);
    }
    if (is_near != 0) {
        BH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>(Sprite_Current[8] + 0x44));
        BH_CALL(Battle_SetTargetFlag40)(B(::bof3::Img(battle_e6::at::kTarget.v)));
        rv::Current<records::BattleTask>()->state = static_cast<unsigned char>(rv::Current<records::BattleTask>()->state + 1);
    }
    return BH_CALL(BattleObj_ScriptTickOnce)();
}

// original 0x452950: state 4: when BattleObj_ScriptTickOnce answers, the way
// back: +0xC / +0x10 / +0x18 = (the actor's +0x34 / +0x38 / +0x3C less
// this's) / 16, +0x14 = 0x40, +0x20 = -8 (the actor and Sprite_Current read
// again for each); on and a tail jump to BattleObj_ScriptTickOnce. Else 0.
extern "C" unsigned char __cdecl BattleFxDash_Return(void) {
    const unsigned char ticked = BH_CALL(BattleObj_ScriptTickOnce)();
    if (ticked == 0) return ticked;
    rv::Store<U>(rv::Current<records::BattleTask>()->step_x, Sixteenth(static_cast<U>(rv::As<records::PartyMember>(PartyBattleE6(Actor()))->x), static_cast<U>(rv::Current<records::BattleTask>()->x)));
    rv::Store<U>(rv::Current<records::BattleTask>()->step_z, Sixteenth(static_cast<U>(rv::As<records::PartyMember>(PartyBattleE6(Actor()))->z), static_cast<U>(rv::Current<records::BattleTask>()->z)));
    rv::Store<U>(rv::Current<records::BattleTask>()->step_y, 0x40);
    rv::Store<U>(rv::Current<records::BattleTask>()->work_20, 0xFFFFFFF8u);
    rv::Store<U>(rv::Current<records::BattleTask>()->work_18, Sixteenth(static_cast<U>(rv::As<records::PartyMember>(PartyBattleE6(Actor()))->height), static_cast<U>(rv::Current<records::BattleTask>()->height)));
    rv::Current<records::BattleTask>()->state = static_cast<unsigned char>(rv::Current<records::BattleTask>()->state + 1);
    return BH_CALL(BattleObj_ScriptTickOnce)();
}

// original 0x452A20: state 5: +0x34 += +0xC, +0x38 += +0x10, the word +0x3E
// += the word +0x14, +0x3C += +0x18 (over +0x3E's new word), +0x14 += +0x20;
// at +0x14 == -0x40 on. A tail jump to BattleObj_ScriptTickOnce.
extern "C" unsigned char __cdecl BattleFxDash_Arc(void) {
    unsigned char* s = Sprite_Current;
    records::BattleTask* cur_task = rv::As<records::BattleTask>(s);
    rv::Store<U>(cur_task->x, static_cast<U>(cur_task->x) + static_cast<U>(cur_task->step_x));
    s = Sprite_Current;
    cur_task = rv::As<records::BattleTask>(s);
    rv::Store<U>(cur_task->z, static_cast<U>(cur_task->z) + static_cast<U>(cur_task->step_z));
    s = Sprite_Current;
    cur_task = rv::As<records::BattleTask>(s);
    rv::Store<unsigned>(cur_task->height_whole, static_cast<std::uint16_t>(cur_task->height_whole + move_script::Word(s + 0x14)));
    s = Sprite_Current;
    cur_task = rv::As<records::BattleTask>(s);
    rv::Store<U>(cur_task->height, static_cast<U>(cur_task->height) + static_cast<U>(cur_task->work_18));
    s = Sprite_Current;
    cur_task = rv::As<records::BattleTask>(s);
    rv::Store<U>(cur_task->step_y, static_cast<U>(cur_task->step_y) + static_cast<U>(cur_task->work_20));
    s = Sprite_Current;
    cur_task = rv::As<records::BattleTask>(s);
    if (static_cast<U>(cur_task->step_y) == 0xFFFFFFC0u) cur_task->state = static_cast<unsigned char>(cur_task->state + 1);
    return BH_CALL(BattleObj_ScriptTickOnce)();
}

// original 0x452A80: state 6: the actor's +0 bit 6 cleared; the owner (0x93B940)
// given pose +8 + 4 as Sprite_Current, which is put back; a tail jump to
// BattleTask_FreeCurrent.
extern "C" void __cdecl BattleFxDash_Land(void) {
    auto* const member_rec = rv::As<records::PartyMember>(PartyBattleE6(Actor()));
    unsigned char* const self = Sprite_Current;
    member_rec->flags &= 0xBF;
    unsigned char* const owner = Owner();
    Sprite_Current = owner;
    BH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>(owner[8] + 4));
    Sprite_Current = self;
    BH_CALL(BattleTask_FreeCurrent)();
}

// original 0x452AD0: slot 16: through BattleFxPose_Steps by +1 (2).
extern "C" void __cdecl BattleFxPose_Dispatch(void) { TaskDispatch("BattleFxPose_Dispatch", ::bof3::Img(BattleFxPose_Steps_addr), 2); }

// original 0x452B00: state 0: pose +8 + 0x50, BattleObj_ScriptTickOnce (not
// read), +0x29 = 3, +9 = 8, on.
extern "C" void __cdecl BattleFxPose_Start(void) {
    BH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>(Sprite_Current[8] + 0x50));
    BH_CALL(BattleObj_ScriptTickOnce)();
    Sprite_Current[0x29] = 3;
    Sprite_Current[9] = 8;
    rv::Current<records::BattleTask>()->state = static_cast<unsigned char>(rv::Current<records::BattleTask>()->state + 1);
}

// original 0x452B40: state 1: BattleObj_ScriptTickOnce (not read); the slot
// freed (a tail jump) once the owner's +1 is 5.
extern "C" void __cdecl BattleFxPose_WaitOwner(void) {
    BH_CALL(BattleObj_ScriptTickOnce)();
    if (Owner()[1] == 5) BH_CALL(BattleTask_FreeCurrent)();
}

// original 0x452B60: slot 18 (the trail BattleFxDash_Advance creates): through
// BattleFxTrail_Steps by +1 (2: this group's start, then
// BossWeretigrFx_TrailFade).
extern "C" void __cdecl BattleFxTrail_Dispatch(void) { TaskDispatch("BattleFxTrail_Dispatch", ::bof3::Img(BattleFxTrail_Steps_addr), 2); }

// original 0x452B90: state 0: +0 |= 0x20, +0x5C = 3, the shade bytes +0x5D,
// +0x5F, +0x5E = +0xB * 0xF0 (a byte), +9 = 8, on; a tail jump to
// Sprite_ScriptTickOnce.
extern "C" unsigned char __cdecl BattleFxTrail_Start(void) {
    rv::Current<records::BattleTask>()->flags |= 0x20;
    Sprite_Current[0x5C] = 3;
    Sprite_Current[0x5D] = static_cast<unsigned char>(Sprite_Current[0xB] * 0xF0u);
    Sprite_Current[0x5F] = static_cast<unsigned char>(Sprite_Current[0xB] * 0xF0u);
    Sprite_Current[0x5E] = static_cast<unsigned char>(Sprite_Current[0xB] * 0xF0u);
    Sprite_Current[9] = 8;
    rv::Current<records::BattleTask>()->state = static_cast<unsigned char>(rv::Current<records::BattleTask>()->state + 1);
    return BH_CALL(Sprite_ScriptTickOnce)();
}

// original 0x452BF0 (BATTLE.EMI#15 0x800A5EC0, branch for branch): for the
// kinds 0x1B, 0x3F and 0x44 of the byte at Field_State +0x92 (PSX +0x82), a
// one-in-twelve roll per target that is not out and whose byte +0xB7 (party)
// or +0xC7 (enemy) - the status class of the resistance grid - is under 6;
// a hit sets bit 0x100 (PSX: pending effect) of its flags. A side target
// (0xC0 bits) rolls every member of that side - bit 0x80 the party, else the
// enemies - and answers 1 if any hit; a single target answers 1 on a hit.
// The single target is read again after each call, as the original does.
// Rand's remainder is the signed one of idiv.
extern "C" unsigned char __cdecl Battle_RollPendingFlag(void) {
    const unsigned char kind = Field_State[0x92];
    if (kind != 0x1B && kind != 0x3F && kind != 0x44) return 0;
    const unsigned char t = TargetBattleSprites();
    if (t & 0xC0) {
        unsigned char hits = 0;
        if (t & 0x80) {
            for (unsigned i = 0; i <= 2; ++i) {
                if (battle_sprites::g.actor_out(i)) continue;
                if (Party(i)[0xB7] >= 6) continue;
                if (battle_sprites::g.rand() % 12 != 0) continue;
                OrLong(Party(i) + 0x130, 0x100);
                ++hits;
            }
        } else {
            for (unsigned i = 3; i <= 10; ++i) {
                if (battle_sprites::g.actor_out(i)) continue;
                if (Enemy(i - 3)[0xC7] >= 6) continue;
                if (battle_sprites::g.rand() % 12 != 0) continue;
                OrLong(Enemy(i - 3) + 0x110, 0x100);
                ++hits;
            }
        }
        return hits ? 1 : 0;
    }
    if (t > 2) {
        if (battle_sprites::g.actor_out(t)) return 0;
        if (Enemy(TargetBattleSprites() - 3u)[0xC7] >= 6) return 0;
        if (battle_sprites::g.rand() % 12 != 0) return 0;
        OrLong(Enemy(TargetBattleSprites() - 3u) + 0x110, 0x100);
        return 1;
    }
    if (battle_sprites::g.actor_out(t)) return 0;
    if (Party(TargetBattleSprites())[0xB7] >= 6) return 0;
    if (battle_sprites::g.rand() % 12 != 0) return 0;
    OrLong(Party(TargetBattleSprites()) + 0x130, 0x100);
    return 1;
}

// original 0x452DD0 (PSX 0x800A626C, call-anchored; its name not in the
// sibling): the auto-target check for an actor. 1 at once when the battle
// flags 0x904AA8 lack bit 14, when the actor in 0x904B8B is out
// (Battle_ActorIsOut of the flags' dword with that byte in al, as pushed), or
// when that actor is the argument's low byte. A party argument stores 0x904B8B
// as the target 0x904B44 and answers 0. An enemy argument answers 1 when its
// record has +0xBA above 1, +0x92 bit 5 clear and +0x114 bit 14 clear; else,
// with 0x904B35 not 4, the target is 0x904B8B and the answer 0; with it 4, the
// enemy's ability (+0x106) flags byte: bit 4 - target 0xC0 with bit 7, else
// 0x80 when 0x904B8B is an enemy, 0x40 a member, answer 1; bit 4 and 6 clear -
// the target is the argument, answer 1; bit 6 alone - 0x904B8B, 0.
// The enemy index is not checked (argument 3..255 indexes 0x128 records).
extern "C" unsigned char __cdecl Battle_AutoTargetCheck(unsigned actor) {
    const U flags = UL(::bof3::Img(rest_4a::at::kBattleFlags.v));
    if (!(flags & 0x4000)) return 1;
    const unsigned char out = SH_CALL(Battle_ActorIsOut)((flags & 0xFFFFFF00u) | BattlePhases_B8B);
    if (out) return 1;
    const unsigned char forced = BattlePhases_B8B;
    const auto a = static_cast<unsigned char>(actor);
    if (forced == a) return 1;
    if (a < 3) {
        move_script::At(::bof3::Img(rest_4a::at::kTarget.v))[0] = forced;
        return 0;
    }
    const U e = rest_4a::at::kEnemyStride * (static_cast<U>(a) - 3);
    if (move_script::Word(move_script::At(::bof3::Img(rest_4a::at::kEnemyIntel.v) + e)) > 1 && !(move_script::At(::bof3::Img(Battle_Enemies_addr) + 0x92 + e)[0] & 0x20) && !(UL(::bof3::Img(rest_4a::at::kEnemyFlags.v) + e) & 0x4000))
        return 1;
    if (move_script::At(::bof3::Img(rest_4a::at::kAutoMode.v))[0] != 4) {
        move_script::At(::bof3::Img(rest_4a::at::kTarget.v))[0] = forced;
        return 0;
    }
    const unsigned char bits = move_script::At(::bof3::Img(rest_4a::at::kAbilityFlags.v) + rest_4a::at::kAbilityStride * move_script::Word(move_script::At(::bof3::Img(Battle_Enemies_addr) + 0x106 + e)))[0];
    if (bits & 0x10) {
        if (bits & 0x80) move_script::At(::bof3::Img(rest_4a::at::kTarget.v))[0] = 0xC0;
        else move_script::At(::bof3::Img(rest_4a::at::kTarget.v))[0] = forced >= 3 ? 0x80 : 0x40;
        return 1;
    }
    if (!(bits & 0x40)) {
        move_script::At(::bof3::Img(rest_4a::at::kTarget.v))[0] = a;
        return 1;
    }
    move_script::At(::bof3::Img(rest_4a::at::kTarget.v))[0] = forced;
    return 0;
}

// original 0x452EB0 (PSX 0x800A6408, call-anchored; the two twins were cited swapped until 2026-10-23): of members 0..2, those
// Battle_ActorIsOut rules in, the one at Rand() % their count. The count is
// not tested (0 divides by zero: ours aborts); the CRT's Rand is 0..0x7FFF
// (a negative answer would read below the list: ours aborts).
extern "C" unsigned char __cdecl Battle_RandomLiveMember(void) {
    unsigned char list[4] = {};
    unsigned char count = 0;
    for (unsigned char m = 0; m <= 2; ++m)
        if (SH_CALL(Battle_ActorIsOut)(m) == 0) list[count++] = m;
    const int r = SH_CALL(Rand)();
    if (count == 0) bof3::Fatal("Battle_RandomLiveMember: every member is out - the original divides by 0 (docs/rest_4a.md section 7)");
    const int k = r % static_cast<int>(count);
    if (k < 0) bof3::Fatal("Battle_RandomLiveMember: Rand answered %d - the original reads its frame below the list", r);
    return list[k];
}

// original 0x452F10 (PSX 0x800A64BC, call-anchored; the two twins were cited swapped until 2026-10-23): of enemies 3..10, those
// Battle_ActorIsOut rules in, the one at Rand() % their count - in a list of
// four bytes. The original keeps the list at esp + 4 of its frame, the count
// in the dword after it and the loop byte in the dword after that, so a fifth
// enemy standing writes the count byte with the actor (the count becomes the
// actor's number) and later ones the count's upper bytes and the loop dword's;
// Rand() % that count then reads any of the twelve bytes. Ours keeps the same
// twelve bytes, so every pick the original can make is ours too - but the
// upper bytes of the count and loop dwords (frame bytes 5..7 and 9..11), which
// the original never writes before it may read them, are 0 here, where the
// original has whatever the stack held (DIVERGENCE DIV-0074; docs/rest_4a.md
// section 7, L1). With four or fewer enemies standing nothing differs.
extern "C" unsigned char __cdecl Battle_RandomLiveEnemy(void) {
    unsigned char f[12] = {};   // the list f[0..3], the count dword f[4..7], the loop dword f[8..11]
    unsigned char i = 3;
    f[8] = i;
    do {
        const U pushed = f[8] | static_cast<U>(f[9]) << 8 | static_cast<U>(f[10]) << 16 | static_cast<U>(f[11]) << 24;
        if (SH_CALL(Battle_ActorIsOut)(pushed) == 0) {
            const unsigned char c = f[4];
            if (c >= 12) bof3::Fatal("Battle_RandomLiveEnemy: the list's index %u reaches the return address", (unsigned)c);
            f[4] = static_cast<unsigned char>(c + 1);
            f[c] = i;
        }
        ++i;
        f[8] = i;
    } while (i <= 10);
    const int r = SH_CALL(Rand)();
    const unsigned char count = f[4];
    if (count == 0) bof3::Fatal("Battle_RandomLiveEnemy: every enemy is out - the original divides by 0 (docs/rest_4a.md section 7)");
    const int k = r % static_cast<int>(count);
    if (k < 0) bof3::Fatal("Battle_RandomLiveEnemy: Rand answered %d - the original reads its frame below the list", r);
    return f[k];
}

// original 0x452F70 (0x800A6570): ORs the low 16 bits of `bits` into the
// flags of the target (or of every member of a side that is not out: 0x80
// the party, then 0x40 the enemies - both for 0xC0), sets its state byte +1
// to 0xB and marks it (0x446FB0). A single target is not tested for being
// out: only the call to 0x4456C0 decides, and it is made for the single
// target too.
extern "C" void __cdecl Battle_SetTargetFlags(unsigned target, unsigned bits) {
    const unsigned char t = static_cast<unsigned char>(target);
    const std::uint32_t low = bits & 0xFFFFu;
    if (t & 0xC0) {
        if (t & 0x80) {
            for (unsigned i = 0; i <= 2; ++i) {
                if (battle_sprites::g.actor_out(i)) continue;
                OrLong(Party(i) + 0x130, low);
                rv::As<records::PartyMember>(Party(i))->state = 0xB;
                battle_sprites::g.mark_actor(i);
            }
        }
        if (t & 0x40) {
            for (unsigned i = 3; i <= 10; ++i) {
                if (battle_sprites::g.actor_out(i)) continue;
                OrLong(Enemy(i - 3) + 0x110, low);
                rv::As<records::BattleEnemy>(Enemy(i - 3))->state = 0xB;
                battle_sprites::g.mark_actor(i);
            }
        }
        return;
    }
    if (battle_sprites::g.actor_out(target)) return;
    if (t <= 2) {
        OrLong(Party(t) + 0x130, low);
        rv::As<records::PartyMember>(Party(t))->state = 0xB;
    } else {
        OrLong(Enemy(t - 3u) + 0x110, low);
        rv::As<records::BattleEnemy>(Enemy(t - 3u))->state = 0xB;
    }
    battle_sprites::g.mark_actor(target);
}

// original 0x4530D0 (0x800A6780): flag bit 0x40 on the target, or on every
// member of a side that is not out (0x80 the party, then 0x40 the enemies).
// A single target gets it without the out test.
extern "C" void __cdecl Battle_SetTargetFlag40(unsigned target) {
    const unsigned char t = static_cast<unsigned char>(target);
    if (t & 0xC0) {
        if (t & 0x80)
            for (unsigned i = 0; i <= 2; ++i)
                if (!battle_sprites::g.actor_out(i)) OrLong(Party(i) + 0x130, 0x40);
        if (t & 0x40)
            for (unsigned i = 3; i <= 10; ++i)
                if (!battle_sprites::g.actor_out(i)) OrLong(Enemy(i - 3) + 0x110, 0x40);
        return;
    }
    if (t <= 2) OrLong(Party(t) + 0x130, 0x40);
    else OrLong(Enemy(t - 3u) + 0x110, 0x40);
}

// original 0x453190 (0x800A690C): 1 if any present actor (byte +0 bit 0) has
// any of bits 4..7 in its flags' low byte, party first; else 0.
extern "C" unsigned char __cdecl Battle_AnyFlagF0(void) {
    for (unsigned i = 0; i <= 2; ++i)
        if ((rv::As<records::PartyMember>(Party(i))->flags & 1) && (Party(i)[0x130] & 0xF0)) return 1;
    for (unsigned i = 3; i <= 10; ++i)
        if ((rv::As<records::BattleEnemy>(Enemy(i - 3))->flags & 1) && (Enemy(i - 3)[0x110] & 0xF0)) return 1;
    return 0;
}

// original 0x453210 (0x800A69E4): whether the current action suits its
// target: 0 when the action record's flags have bit 4; otherwise, for a
// target that is out (0x4456C0), 1 unless the action is one of the four out
// actions, and for one that is not, 1 only if it is.
extern "C" unsigned char __cdecl Battle_ActionSuitsTarget(void) {
    const std::uint16_t action = CurrentAction();
    if (ActionRecord(action)[0] & 0x10) return 0;
    if (battle_sprites::g.actor_out(TargetBattleSprites())) return OutAction(action) ? 0 : 1;
    return OutAction(action) ? 1 : 0;
}

// original 0x4532A0 (0x800A6A9C): the same for an item: 0x591810(the id's high
// byte, the id) with bit 4 answers 0; then the ids 0x0E and 0x128 play the
// out actions' part.
extern "C" unsigned char __cdecl Battle_ItemSuitsTarget(void) {
    const std::uint16_t item = CurrentAction();
    if (battle_sprites::g.item_class(item >> 8, item) & 0x10) return 0;
    const bool out_item = item == 0x0E || item == 0x128;
    if (battle_sprites::g.actor_out(TargetBattleSprites())) return out_item ? 0 : 1;
    return out_item ? 1 : 0;
}

// original 0x453300 (PSX 0x800A6B30): an actor's stats rebuilt from the buffs. A member (0..2):
// Battle_RecalcMemberStats(its record + 0x80, actor). An enemy (actor - 3):
// four words +0xB4.. = 0x446F20(+0xD4.., the s8 buff +0x118.. * 2 + 100),
// four bytes +0xC9.. = 0x446F80(+0xE9.., +0x11C.. likewise), eight bytes
// copied (+0xBF.. from +0xDF.., not +0xC6), +0x114 bit 14 triples +0xB4, bit
// 12 moves +0xB6 into it; +0xB4 and +0xB6 through 0x446F20(v, 100). Then,
// for either, ENEMY index (the member's own number for a member) +0x114 bit
// 11 zeroes its +0xC4 and bit 13 its +0xBF - latent: a member's rebuild
// clears enemy 0..2's bytes (docs/battle_e6.md section 7).
extern "C" void __cdecl Battle_RecalcStats(unsigned actor) {
    unsigned index;
    if ((actor & 0xFF) <= 2) {
        index = actor & 0xFF;
        BH_CALL(Battle_RecalcMemberStats)(move_script::At(::bof3::Img(ObjTrio_addr) + 0x80 + index * battle_e6::at::kPartyStride), actor);
    } else {
        index = (actor - 3) & 0xFF;
        unsigned char* const e = EnemyBattleE6(index);
        auto* const enemy_rec = rv::As<records::BattleEnemy>(e);
        for (unsigned i = 0; i < 4; ++i)
            move_script::SetWord(e + 0xB4 + 2 * i,
                    static_cast<std::uint16_t>(Percent999(move_script::Word(e + 0xD4 + 2 * i), S8(e[0x118 + i]) * 2 + 100)));
        for (unsigned i = 0; i < 4; ++i)
            e[0xC9 + i] = static_cast<unsigned char>(Percent100(e[0xE9 + i], S8(e[0x11C + i]) * 2 + 100));
        for (unsigned i = 0; i < 7; ++i) e[0xBF + i] = e[0xDF + i];
        e[0xC7] = e[0xE7];
        if (static_cast<U>(enemy_rec->flags_114) & 0x4000) enemy_rec->atk = static_cast<std::uint16_t>(enemy_rec->atk * 3);
        if (static_cast<U>(enemy_rec->flags_114) & 0x1000) {
            enemy_rec->atk = static_cast<std::uint16_t>(enemy_rec->atk + enemy_rec->def);
            enemy_rec->def = 0;
        }
        enemy_rec->atk = static_cast<std::uint16_t>(Percent999(enemy_rec->atk, 100));
        enemy_rec->def = static_cast<std::uint16_t>(Percent999(enemy_rec->def, 100));
    }
    auto* const enemy_rec = rv::As<records::BattleEnemy>(EnemyBattleE6(index));
    if (static_cast<U>(enemy_rec->flags_114) & 0x800) enemy_rec->holy = 0;
    if (static_cast<U>(enemy_rec->flags_114) & 0x2000) enemy_rec->elements[0] = 0;
}

// original 0x453560 (PSX 0x800A6F1C): a member's stats rebuilt: `view` (its record + 0x80)
// four words +0x24.. = 0x446F20(+0x44.., the member's s8 buff +0x138.. * 2 +
// 100), four bytes +0x39.. = 0x446F80(+0x59.., +0x13C..), eight bytes +0x2F..
// from +0x4F.. (not +0x36); then on the member's record: +0x134 bit 0 and bit
// 14 each triple +0xA4, bit 12 moves +0xA6 into it; with +0x134 bit 1 and form
// 0x16 (0x904B89) +0xBB and +0xBA up 0x1E each and through 0x446F80(v, 100);
// bit 11 zeroes +0xB4, bit 13 +0xAF; +0xA4 and +0xA6 through 0x446F20(v, 100).
extern "C" void __cdecl Battle_RecalcMemberStats(unsigned char* view, unsigned member) {
    unsigned char* const r = PartyBattleE6(member);
    auto* const member_rec = rv::As<records::PartyMember>(r);
    for (unsigned i = 0; i < 4; ++i)
        move_script::SetWord(view + 0x24 + 2 * i,
                static_cast<std::uint16_t>(Percent999(move_script::Word(view + 0x44 + 2 * i), S8(r[0x138 + i]) * 2 + 100)));
    for (unsigned i = 0; i < 3; ++i)
        view[0x39 + i] = static_cast<unsigned char>(Percent100(view[0x59 + i], S8(r[0x13C + i]) * 2 + 100));
    const auto fourth = static_cast<unsigned char>(Percent100(view[0x5C], S8(r[0x13F]) * 2 + 100));
    const unsigned char b50 = view[0x50], b51 = view[0x51];   // read before the fourth is stored, as there
    view[0x3C] = fourth;
    view[0x2F] = view[0x4F];
    const unsigned char b52 = view[0x52];
    view[0x30] = b50;
    view[0x31] = b51;
    const unsigned char b53 = view[0x53], b54 = view[0x54];
    view[0x32] = b52;
    const unsigned char b55 = view[0x55];
    view[0x33] = b53;
    const unsigned char b57 = view[0x57];
    view[0x34] = b54;
    view[0x35] = b55;
    view[0x37] = b57;
    if (r[0x134] & 1) member_rec->atk = static_cast<std::uint16_t>(member_rec->atk * 3);
    if (static_cast<U>(member_rec->flags_134) & 0x4000) member_rec->atk = static_cast<std::uint16_t>(member_rec->atk * 3);
    if (static_cast<U>(member_rec->flags_134) & 0x1000) {
        member_rec->atk = static_cast<std::uint16_t>(member_rec->atk + member_rec->def);
        member_rec->def = 0;
    }
    if ((r[0x134] & 2) && BattlePhases_B89 == 0x16) {
        r[0xBB] = static_cast<unsigned char>(r[0xBB] + 0x1E);
        r[0xBA] = static_cast<unsigned char>(r[0xBA] + 0x1E);
        r[0xBB] = static_cast<unsigned char>(Percent100(r[0xBB], 100));
        r[0xBA] = static_cast<unsigned char>(Percent100(r[0xBA], 100));
    }
    if (static_cast<U>(member_rec->flags_134) & 0x800) member_rec->holy = 0;
    if (static_cast<U>(member_rec->flags_134) & 0x2000) member_rec->elements[0] = 0;
    member_rec->atk = static_cast<std::uint16_t>(Percent999(member_rec->atk, 100));
    member_rec->def = static_cast<std::uint16_t>(Percent999(member_rec->def, 100));
}

// original 0x4537A0 (no PSX twin found): picks a party member for the current
// action and sets its flag bit 3 and state 0xA, answering 1; 0 when none
// qualifies. The test is 0x453910 when bit 9 of the action record's u16 +4
// is set, else Battle_MemberCoinFlip. A single party target that passes is
// taken (the target byte read again as a dword); otherwise every member is
// tested and one of those that pass is drawn by Rand, signed remainder.
extern "C" unsigned char __cdecl Battle_PickFlag8Member(void) {
    unsigned char passed[3] = {};
    unsigned char count = 0;
    const bool second = ((battle_sprites::Word(ActionRecord(CurrentAction()) + 4) >> 9) & 1) != 0;
    const unsigned char t = TargetBattleSprites();
    auto test = [second](unsigned i) { return second ? battle_sprites::g.member_chance_b(i) : battle_sprites::g.member_chance(i); };
    unsigned char pick = 0;
    bool taken = false;
    if (!(t & 0xC0) && t <= 2 && test(t)) {
        pick = static_cast<unsigned char>(battle_sprites::Long(battle_sprites::At(::bof3::Img(battle_sprites::at::kTarget.v))));
        taken = true;
    }
    if (!taken) {
        for (unsigned i = 0; i <= 2; ++i)
            if (test(i)) passed[count++] = static_cast<unsigned char>(i);
        if (!count) return 0;
        pick = passed[battle_sprites::g.rand() % static_cast<int>(count)];
    }
    OrLong(Party(pick) + 0x130, 8);
    rv::As<records::PartyMember>(Party(pick))->state = 0xA;
    return 1;
}

// original 0x453910 (Battle_PickFlag8Member's test when the action record's
// +4 has bit 9): 0 when the member is out, lacks +0x130 bit 0, its +0x124 is
// not the actor, the action's bit (Battle_ActionBitSet of 0x904B40's byte +2)
// is set or its list is full (Battle_MemberListFull); else 1 when Rand leaves
// no remainder by the odds of the action row's high nibble (0x65C4D9 + 0x18
// by 0x904B40's word +2; 0x64F104 when the member's character index
// (Character_IndexById 0x66972C by its +0x89) is 6, else 0x64F0FC). The original divides by the
// odds unchecked: ours aborts on 0.
extern "C" unsigned char __cdecl Battle_MemberRollByAction(unsigned member) {
    if (BH_CALL(Battle_ActorIsOut)(member) != 0) return 0;
    unsigned char* const r = PartyBattleE6(member);
    auto* const member_rec = rv::As<records::PartyMember>(r);
    if (!(r[0x130] & 1)) return 0;
    if (r[0x124] != B(::bof3::Img(battle_e6::at::kActor.v))) return 0;
    if (BH_CALL(Battle_ActionBitSet)(move_script::At(BattleSprites_Action)[2]) != 0) return 0;
    if (BH_CALL(Battle_MemberListFull)(member) != 0) return 0;
    const U table = Character_IndexById[member_rec->char_id] == 6 ? ::bof3::Img(battle_e6::at::kRollOddsB.v) : ::bof3::Img(battle_e6::at::kRollOdds.v);
    const unsigned id = move_script::Word(move_script::At(BattleSprites_Action) + 2);
    const unsigned nibble = move_script::At(::bof3::Img(battle_e6::at::kActionRows.v) + id * battle_e6::at::kActionRowStride)[0] >> 4;
    const unsigned odds = move_script::At(table)[nibble];
    const int roll = BH_CALL(Rand)();
    if (odds == 0)
        bof3::Fatal("Battle_MemberRollByAction: the odds for action 0x%X (nibble %u of 0x%X) are 0 - the original divides by "
                    "them (docs/battle_e6.md section 7)",
                    id, nibble, (unsigned)table);
    return (static_cast<unsigned>(roll % static_cast<int>(odds)) & 0xFF) == 0 ? 1 : 0;
}

// original 0x453A10 (0x800A76E0; PSX 0x800A76E0): 0 if member `i` is out, lacks flag bit 0,
// its byte +0x124 is not the acting actor, or 0x453AC0 says so; else a coin
// flip, 1 on an even Rand (the signed remainder's low byte tested).
extern "C" unsigned char __cdecl Battle_MemberCoinFlip(unsigned i) {
    if (battle_sprites::g.actor_out(i)) return 0;
    unsigned char* const p = Party(i & 0xFFu);
    if (!(p[0x130] & 1)) return 0;
    if (p[0x124] != battle_sprites::At(::bof3::Img(battle_sprites::at::kActor.v))[0]) return 0;
    if (battle_sprites::g.slots_full(i)) return 0;
    return battle_sprites::g.rand() % 2 == 0 ? 1 : 0;
}

// original 0x453A90 (PSX 0x800A77B4): bit (action & 31) of the dword 0x904088 + 4 (action >> 5)
// set: 1, else 0 (the action's low byte).
extern "C" unsigned char __cdecl Battle_ActionBitSet(unsigned action) {
    const unsigned a = action & 0xFF;
    return (LBattleE6(move_script::At(::bof3::Img(Cond_Flags_addr) + 0xf8 + (a >> 5) * 4)) >> (a & 31)) & 1 ? 1 : 0;
}

// original 0x453AC0 (PSX 0x800A77E4): 1 when every one of the member's ten bytes +0xFE.. is
// not 0, else 0.
extern "C" unsigned char __cdecl Battle_MemberListFull(unsigned member) {
    const unsigned char* const r = PartyBattleE6(member);
    for (unsigned i = 0; i < 10; ++i)
        if (r[0xFE + i] == 0) return 0;
    return 1;
}

// original 0x453B10 (0x800A783C): for each party member not out: with flag
// bit 3, unless 0x453A90 has the action's low byte, the action (when its
// record's byte +5 has bit 1) is handed to 0x590C90 twice - (id, member, 0,
// 1), then (id's low byte, member +0x148, 0, 0) - and its bit set in the
// 0x904088 set, and the answer becomes 1; without bit 3, the flags are
// stored back unchanged (the original's `and al, 0xF7` of a clear bit). The
// action pointer and id are read again after every call.
extern "C" unsigned char __cdecl Battle_SettleFlag8(void) {
    unsigned char answer = 0;
    for (unsigned i = 0; i <= 2; ++i) {
        auto* const member_rec = rv::As<records::PartyMember>(Party(i));
        if (battle_sprites::g.actor_out(i)) continue;
        const std::uint32_t flags = static_cast<std::uint32_t>(static_cast<std::int32_t>(member_rec->flags_130));
        if (!(flags & 8)) {
            member_rec->flags_130 = static_cast<std::int32_t>(flags & ~8u);
            continue;
        }
        if (battle_sprites::g.action_flag(Ptr(battle_sprites::At(::bof3::Img(BattleSprites_Action_addr)))[2])) continue;
        const std::uint16_t action = CurrentAction();
        if (ActionRecord(action)[5] & 2) {
            battle_sprites::g.inventory_put(action, i, 0, 1);
            battle_sprites::g.inventory_put(Ptr(battle_sprites::At(::bof3::Img(BattleSprites_Action_addr)))[2], member_rec->char_index, 0, 0);
            const std::uint16_t again = CurrentAction();
            OrLong(battle_sprites::At(::bof3::Img(Cond_Flags_addr) + 0xf8 + (again >> 5) * 4u), 1u << (again & 31));
        }
        answer = 1;
    }
    return answer;
}

// original 0x453C00 (0x800A79AC, the PSX's Formation_ApplyStatMods): by the
// formation byte, 1..4 (anything else nothing), through Stat_AddClamped and
// Stat_AddCap100 on the party records' stats (+0xC4.. the base block):
//   1: member 0's +0xC4 up by half, +0xC6 down by a quarter, +0xD9 doubled
//      (capped at 100);
//   2: every member up to the party count not out: +0xC8 down by a quarter;
//   3: the same members take member 0's +0xC8 (read each time), then +0xC6
//      down by half;
//   4: member 2's +0xCA up by half, members 0 and 1's down by a quarter.
// The party count is read again every turn. Deltas are 16 bits.
extern "C" void __cdecl Formation_ApplyStatMods(void) {
    auto stat = [](std::uint32_t address) { return reinterpret_cast<unsigned short*>(battle_sprites::At(address)); };
    auto half = [](std::uint32_t address) { return static_cast<unsigned>(battle_sprites::Word(battle_sprites::At(address)) >> 1); };
    auto quarter = [](std::uint32_t address) { return static_cast<unsigned>(battle_sprites::Word(battle_sprites::At(address)) >> 2); };
    switch (static_cast<std::uint32_t>(battle_sprites::Long(battle_sprites::At(::bof3::Img(Cond_Flags_addr) + 0xd0)) & 0xFF) - 1) {
    case 0:
        battle_sprites::g.add_clamped(stat(::bof3::Img(0x802E04)), half(::bof3::Img(0x802E04)));
        battle_sprites::g.add_clamped(stat(::bof3::Img(0x802E06)), 0u - quarter(::bof3::Img(0x802E06)));
        battle_sprites::g.add_cap100(battle_sprites::At(::bof3::Img(0x802E19)), battle_sprites::At(::bof3::Img(0x802E19))[0]);
        return;
    case 1:
        for (unsigned char i = 0; i < battle_sprites::At(::bof3::Img(battle_sprites::at::kPartyCount.v))[0]; ++i)
            if (!battle_sprites::g.actor_out(i)) battle_sprites::g.add_clamped(stat(::bof3::Img(0x802E08) + i * battle_sprites::at::kPartyStride), 0u - quarter(::bof3::Img(0x802E08) + i * battle_sprites::at::kPartyStride));
        return;
    case 2:
        for (unsigned char i = 0; i < battle_sprites::At(::bof3::Img(battle_sprites::at::kPartyCount.v))[0]; ++i) {
            if (battle_sprites::g.actor_out(i)) continue;
            battle_sprites::SetWord(battle_sprites::At(::bof3::Img(0x802E08) + i * battle_sprites::at::kPartyStride), battle_sprites::Word(battle_sprites::At(::bof3::Img(0x802E08))));
            battle_sprites::g.add_clamped(stat(::bof3::Img(0x802E06) + i * battle_sprites::at::kPartyStride), 0u - half(::bof3::Img(0x802E06) + i * battle_sprites::at::kPartyStride));
        }
        return;
    case 3:
        battle_sprites::g.add_clamped(stat(::bof3::Img(0x8030A2)), half(::bof3::Img(0x8030A2)));
        battle_sprites::g.add_clamped(stat(::bof3::Img(0x802E0A)), 0u - quarter(::bof3::Img(0x802E0A)));
        battle_sprites::g.add_clamped(stat(::bof3::Img(0x802F56)), 0u - quarter(::bof3::Img(0x802F56)));
        return;
    default:
        return;
    }
}

// original 0x453DA0 (0x800A7BC0): a pop-up for an amount on an actor: a free
// 0x84-byte record from 0x435180(0, 1) - not checked for 0xFF, which would
// land past the 48 records - gets +0xB = 0, +0x80 = the actor's record, +0x60
// = the amount's magnitude (signed 16), +0x27 = 6 for a negative amount else
// 0, and +7 by the actor's byte +0x12C (party) / +0x10C (enemy): 4 without
// bit 0; 2 (and +0x27 = 6) with bit 2 and not bit 4; else 0 for a non-zero
// amount or with bit 4, and 3 for zero.
extern "C" void __cdecl Battle_SetDamagePopup(unsigned amount, unsigned actor) {
    const unsigned slot = battle_sprites::g.popup_slot(0, 1);
    unsigned char* const rec = battle_sprites::At(::bof3::Img(BattleTask_Slots_addr) + (slot & 0xFFu) * battle_sprites::at::kPopupStride);
    auto* const popup = rv::As<records::BattleTask>(rec);
    rec[0xB] = 0;
    const unsigned char who = static_cast<unsigned char>(actor);
    unsigned char bits;
    if (who <= 2) {
        bits = Party(who)[0x12C];
        popup->owner = Address(Party(who));
    } else {
        bits = rv::As<records::BattleEnemy>(Enemy(who - 3u))->shown;
        popup->owner = Address(Enemy(who - 3u));
    }
    const std::int32_t value = static_cast<short>(amount);
    if (value < 0) {
        popup->value = -value;
        popup->clut = 6;
    } else {
        popup->value = value;
        popup->clut = 0;
    }
    if (!(bits & 1)) {
        popup->mode = 4;
        return;
    }
    if (!(bits & 0x10) && (bits & 4)) {
        popup->clut = 6;
        popup->mode = 2;
        return;
    }
    if (value != 0 || (bits & 0x10)) {
        popup->mode = 0;
        return;
    }
    popup->mode = 3;
}

// original 0x453EB0 (EnemyOp_ReceiveAction and three more: an AP change): a
// pop-up task (BattleTask_Create(0, 1) - not checked for 0xFF, as
// Battle_SetDamagePopup, D54) with +0xB = 1, +0x80 the actor's record, +0x60
// the amount's magnitude (s16), +0x27 1 for a negative amount else 2; +7 by
// the actor's +0x12C (member) / +0x10C (enemy): 4 without bit 1; 2 (and +0x27
// = 1) with bit 3 and not bit 5; else 0.
extern "C" void __cdecl Battle_SetApPopup(unsigned amount, unsigned actor) {
    const unsigned char slot = BH_CALL(BattleTask_Create)(0, 1);
    unsigned char* const t = move_script::At(::bof3::Img(BattleTask_Slots_addr) + slot * battle_e6::at::kTaskStride);
    auto* const task_rec = rv::As<records::BattleTask>(t);
    t[0xB] = 1;
    const unsigned who = actor & 0xFF;
    unsigned char bits;
    if (who <= 2) {
        bits = PartyBattleE6(who)[0x12C];
        task_rec->owner = Addr(PartyBattleE6(who));
    } else {
        bits = rv::As<records::BattleEnemy>(EnemyBattleE6(who - 3))->shown;
        task_rec->owner = Addr(EnemyBattleE6(who - 3));
    }
    const std::int32_t value = S16BattleE6(amount);
    if (value < 0) {
        rv::Store<U>(task_rec->value, static_cast<U>(-value));
        task_rec->clut = 1;
    } else {
        rv::Store<U>(task_rec->value, static_cast<U>(value));
        task_rec->clut = 2;
    }
    if (!(bits & 2)) {
        t[7] = 4;
        return;
    }
    if (!(bits & 0x20) && (bits & 8)) {
        task_rec->clut = 1;
        t[7] = 2;
        return;
    }
    t[7] = 0;
}

// original 0x453FA0 (0x800A7FF0): a party member's automatic target. +0x125
// is copied to 0x904B35 first. With +0x90 bit 5, flags +0x134 bits 14 or 16,
// or +0x134 bit 0: unless 0x452DD0 allows it, nothing but the settle below;
// without +0x134 bit 0, a random pick - Rand & 7 of 3 and up takes
// 0x454310(member) when 0x904AB1 is not 1, else 0x445730((Rand & 7) + 3) -
// then the settle: +0x125 = 1 and 0x904B35 = 1. Otherwise: +0x134 bit 0
// hands over to 0x454290; battle flag bit 4 to 0x446B00; a side in +0x124 is
// the target; then by +0x125: 0 and 1 take +0x124 unless it is out (then
// 0x446B00), 2 and 3 nothing, 4 and 5 fall back on 0x454310 / 0x445730 /
// Battle_RandomEnemy 0x435C80 by the tests in docs/battle_sprites.md section 1; above 5
// nothing.
extern "C" void __cdecl Battle_MemberAutoTarget(unsigned member) {
    unsigned char* const p = Party(member & 0xFFu);
    auto* const member_rec = rv::As<records::PartyMember>(p);
    battle_sprites::At(::bof3::Img(battle_sprites::at::kAutoMode.v))[0] = p[0x125];
    if ((p[0x90] & 0x20) || (static_cast<std::int32_t>(member_rec->flags_134) & 0x14000) || (p[0x134] & 1)) {
        if (!battle_sprites::g.auto_allowed(member)) {
            p[0x125] = 1;
            battle_sprites::At(::bof3::Img(battle_sprites::at::kAutoMode.v))[0] = 1;
            return;
        }
        if (!(p[0x134] & 1)) {
            const unsigned char roll = static_cast<unsigned char>(battle_sprites::g.rand() & 7);
            if (static_cast<signed char>(roll) >= 3 && Battle_PartyStanding != 1)
                SetTarget(battle_sprites::g.pick_party(member));
            else
                SetTarget(battle_sprites::g.pick_enemy_b(static_cast<unsigned char>((battle_sprites::g.rand() & 7) + 3)));
            p[0x125] = 1;
            battle_sprites::At(::bof3::Img(battle_sprites::at::kAutoMode.v))[0] = 1;
            return;
        }
    }
    if (p[0x134] & 1) {
        battle_sprites::g.auto_fixed(member);
        return;
    }
    if (battle_sprites::At(::bof3::Img(battle_sprites::at::kBattleFlags.v))[0] & 0x10) {
        battle_sprites::g.no_target();
        return;
    }
    const unsigned char first = p[0x124];
    if (first & 0xC0) {
        SetTarget(first);
        return;
    }
    // A party target of the member's own choosing: 0x454310 unless 0x904AB1
    // is 1, then the member itself.
    auto party_pick = [member]() {
        if (Battle_PartyStanding != 1) SetTarget(battle_sprites::g.pick_party(member));
        else SetTarget(static_cast<unsigned char>(member));
    };
    switch (p[0x125]) {
    case 0:
    case 1:
        if (battle_sprites::g.actor_out(first)) {
            battle_sprites::g.no_target();
            return;
        }
        SetTarget(first);
        return;
    case 4: {
        if (!battle_sprites::g.actor_out(first) || battle_sprites::g.out_action(member)) {
            SetTarget(p[0x124]);
            return;
        }
        const unsigned char t = p[0x124];
        if (t > 2) {
            SetTarget(battle_sprites::g.pick_enemy_b(t));
            return;
        }
        if (ActionRecord(rv::As<records::PartyMember>(p)->queued_id)[0] & 0x20) {
            battle_sprites::g.no_target();
            return;
        }
        party_pick();
        return;
    }
    case 5: {
        if (!battle_sprites::g.actor_out(first)) {
            SetTarget(p[0x124]);
            return;
        }
        const unsigned char same = battle_sprites::g.action_is_e(member);
        const unsigned char t = p[0x124];
        if (same) {
            SetTarget(t);
            return;
        }
        if (t > 2) {
            SetTarget(battle_sprites::g.pick_enemy_b(t));
            return;
        }
        if (battle_sprites::g.item_class(p[0x127], p[0x126]) & 0x20) {
            battle_sprites::g.no_target();
            return;
        }
        const unsigned char again = p[0x124];
        if (again <= 2) {
            party_pick();
            return;
        }
        if (Battle_EnemiesLeft != 1) SetTarget(battle_sprites::g.pick_enemy_a(again));
        else SetTarget(battle_sprites::g.pick_enemy_b(again));
        return;
    }
    default:
        return;
    }
}

// original 0x454220 (no twin found; it follows 0x453FA0's jump table): 1 if
// party member `i`'s action +0x126 is one of the four out actions.
extern "C" unsigned char __cdecl Battle_MemberOutAction(unsigned i) {
    return OutAction(rv::As<records::PartyMember>(Party(i & 0xFFu))->queued_id) ? 1 : 0;
}

// original 0x454260 (no PSX twin paired): 1 when the member's action word
// (ObjTrio + 0x126 of the argument's low byte, unchecked) is 0xE or 0x128.
extern "C" unsigned char __cdecl Battle_MemberActionIs0E(unsigned member) {
    const unsigned short w = move_script::Word(move_script::At(::bof3::Img(ObjTrio_addr) + 0x126 + rest_4a::at::kMemberStride * (member & 0xFFu)));
    if (w == 0xE) return 1;
    return w == 0x128 ? 1 : 0;
}

// original 0x454290 (PSX 0x800A84A0, Weretiger's target roll): a member's fixed auto-target. The
// member's odds byte +0x142 (signed, at most 5) against Rand() % 20: below it,
// and 0x904AB1 not 1, Battle_RandomOtherMember(member); else
// Battle_DefaultTarget((Rand() & 7) + 3). Then +0x125 = 1, 0x904B35 = 1 and
// the target 0x904B44 the answer.
extern "C" void __cdecl Battle_MemberAutoFixed(unsigned member) {
    const U m = member & 0xFFu;
    auto odds = static_cast<signed char>(move_script::At(::bof3::Img(ObjTrio_addr) + 0x142 + rest_4a::at::kMemberStride * m)[0]);
    if (odds > 5) odds = 5;
    const int r = SH_CALL(Rand)();
    const int roll = r % 20;
    unsigned char target;
    if (roll < odds && Battle_PartyStanding != 1) {
        target = SH_CALL(Battle_RandomOtherMember)(member);
    } else {
        const int r2 = SH_CALL(Rand)();
        target = SH_CALL(Battle_DefaultTarget)((static_cast<U>(r2) & 0xFFFFFF00u) | static_cast<unsigned char>((r2 & 7) + 3));
    }
    move_script::At(::bof3::Img(ObjTrio_addr) + 0x125 + rest_4a::at::kMemberStride * m)[0] = 1;
    move_script::At(::bof3::Img(rest_4a::at::kAutoMode.v))[0] = 1;
    move_script::At(::bof3::Img(rest_4a::at::kTarget.v))[0] = target;
}

// original 0x454310 (no PSX twin paired): of members 0..2, those
// Battle_ActorIsOut rules in and that are not the argument's low byte, the one
// at Rand() % their count. As Battle_RandomLiveMember: 0 divides by zero (a
// member alone and standing, picking another: ours aborts).
extern "C" unsigned char __cdecl Battle_RandomOtherMember(unsigned exclude) {
    unsigned char list[4] = {};
    unsigned char count = 0;
    for (unsigned char m = 0; m <= 2; ++m)
        if (SH_CALL(Battle_ActorIsOut)(m) == 0 && static_cast<unsigned char>(exclude) != m) list[count++] = m;
    const int r = SH_CALL(Rand)();
    if (count == 0) bof3::Fatal("Battle_RandomOtherMember: no other member standing - the original divides by 0 (docs/rest_4a.md section 7)");
    const int k = r % static_cast<int>(count);
    if (k < 0) bof3::Fatal("Battle_RandomOtherMember: Rand answered %d - the original reads its frame below the list", r);
    return list[k];
}

// original 0x454380 (0x800A8764, the PSX's Battle_PlayHitSound): the sound
// Sound_PlayEffect plays for a hit: the class is the table 0x657465 at the
// party actor's +0x92 (every 0x1C bytes), or the enemy's +0x101; the id the
// u16 at 0x64F10C + (class + 7 * critical) * 2, critical bit 7 of the battle
// flags.
extern "C" void __cdecl Battle_PlayHitSound(void) {
    const unsigned critical = battle_sprites::At(::bof3::Img(battle_sprites::at::kBattleFlags.v))[0] >> 7;
    const unsigned char actor = battle_sprites::At(::bof3::Img(battle_sprites::at::kActor.v))[0];
    const unsigned hit_class = actor <= 2 ? battle_sprites::At(::bof3::Img(battle_sprites::at::kHitClass.v))[rv::As<records::PartyMember>(Party(actor))->weapon * 0x1Cu] : rv::As<records::BattleEnemy>(Enemy(actor - 3u))->hit_class;
    battle_sprites::g.play_effect(battle_sprites::Word(battle_sprites::At(::bof3::Img(battle_sprites::at::kHitSounds.v) + (hit_class + critical * 7) * 2)));
}

// original 0x454410 (0x800A8840): a hit pop-up: a free record from
// 0x435180(0, 3), +0x80 = Sprite_Current, +7 = 7 when the block at 0x904B50
// has +0xC bit 1, else the actor's hit class; +0x2E / +0x30 = the sprite's
// +0x2E / +0x30 plus a signed offset - the enemy block's (0x939AD8) +0xF2 /
// +0xF3 for an enemy sprite (+5 above 2), the pair at 0x64DF70 by the
// sprite's +8 and Field_State +0x89 for a party one - +0x32 the sprite's,
// and +8 the owning record's byte +8. The sprite and the two block pointers
// are read once; the table indices again for the second offset.
//
// DIVERGENCE DIV-0185 (D397): 0x904B50 is the target's block, written by
// BattleAction_BeginStep / AbilityCheck only for a target of 0..10 and
// cleared by nothing, so a side target (0x40 / 0x80 / 0xC0) leaves the last
// one-target action's block there - and in a session with no such action
// yet, 0. The original tests +0xC unchecked and faults reading 0x0000000C:
// an enemy that acts first in a fresh session and casts Giant Growth (game
// byte 0x12, side target 0x80, the pop-up path) - measured 2026-10-29 on the
// owner's crash and forced on giantGrowth (Capcom's function the same,
// 0x45448D). Ours: a null block counts as bit 1 clear, the actor's hit class;
// a written one takes the original's path unchanged, stale or not.
// BOF3X_ORIGINAL=Battle_SetHitPopup leaves Capcom's function, its read with it.
extern "C" void __cdecl Battle_SetHitPopup(void) {
    const unsigned slot = battle_sprites::g.popup_slot(0, 3) & 0xFFu;
    const unsigned char actor = battle_sprites::At(::bof3::Img(battle_sprites::at::kActor.v))[0];
    const unsigned char* const block = Ptr(battle_sprites::At(::bof3::Img(BattleSprites_HitBlock_addr)));
    unsigned char* const rec = battle_sprites::At(::bof3::Img(BattleTask_Slots_addr) + slot * battle_sprites::at::kPopupStride);
    auto* const popup = rv::As<records::BattleTask>(rec);
    unsigned char* const s = Sprite_Current;
    popup->owner = Address(s);
    if (block == nullptr)   // DIV-0185: no target block yet, the bit read as clear
        bof3::Log("DIV-0185    Battle_SetHitPopup: no target block (actor %u, target 0x%02X), the actor's hit class", actor,
                  TargetBattleSprites());
    const bool fixed = block != nullptr && (block[0xC] & 2) != 0;
    if (actor <= 2) popup->mode = fixed ? 7 : battle_sprites::At(::bof3::Img(battle_sprites::at::kHitClass.v))[rv::As<records::PartyMember>(Party(actor))->weapon * 0x1Cu];
    else popup->mode = fixed ? 7 : rv::As<records::BattleEnemy>(Enemy(actor - 3u))->hit_class;
    if (s[5] > 2) {
        const unsigned char* const offsets = Ptr(battle_sprites::At(::bof3::Img(BattleE3_CurrentEnemy_addr)));
        rv::Store<unsigned>(popup->screen_x, static_cast<unsigned>(static_cast<signed char>(offsets[0xF2]) + battle_sprites::Word(s + 0x2E)));
        rv::Store<unsigned>(popup->screen_y, static_cast<unsigned>(static_cast<signed char>(offsets[0xF3]) + battle_sprites::Word(s + 0x30)));
        popup->digit_phase = battle_sprites::Word(s + 0x32);
        rec[8] = Enemy(s[5] - 3u)[8];
        return;
    }
    const unsigned char* const field = Field_State;
    rv::Store<unsigned>(popup->screen_x, static_cast<unsigned>(BattleWin_MemberTargetOffsets[(s[8] + field[0x89] * 4u) * 2] + battle_sprites::Word(s + 0x2E)));
    rv::Store<unsigned>(popup->screen_y, static_cast<unsigned>(BattleWin_MemberTargetOffsets[0x1 + (s[8] + field[0x89] * 4u) * 2] + battle_sprites::Word(s + 0x30)));
    popup->digit_phase = battle_sprites::Word(s + 0x32);
    rec[8] = Party(s[5])[8];
}

// original 0x454590. Reads DAT\<name> whole and walks its chunks.
//
// DIVERGENCE DIV-0089: with BOF3X_CACHE set (and not BOF3X_CACHE_DATA=0),
// DAT\<name> is the cache's base\dat\<name> and loc\zh-CN\dat\<name> walked
// chunk by chunk in the PC's slot order when the cache holds it whole, the
// install's otherwise; each overlay below is the cache's loc\<tag>\dat\<name>
// or opt\<layer>\dat\<name> when the cache has that layer at all, else the
// install's (dat_cache.h, docs/cache-read.md).
//
// DIVERGENCE DIV-0086: then, with BOF3X_OPT=<layer>[,<layer>...] set,
// DAT\<layer>.<name> for each layer in that order, when it exists - after the
// language overlay, so a layer lands on top of both. The layers are built by
// tools/importer.py from the player's PSP disc (area4-walls, DIV-0080: a
// Western PSX disc) and copied in by its
// `install`; none ships (docs/opt-layers.md).
//
// DIVERGENCE DIV-0005: with BOF3X_LANG=<tag> set, DAT\<tag>.<name> is walked
// after DAT\<name> when it exists, so its chunks land on top of the shipped ones - a
// kind-0 chunk over the same arena bytes, a kind-3 chunk replacing the glyph
// table (Font_SetGlyphData frees the shipped one, a branch no shipped data
// runs). The overlays are built locally by tools/loc_build.py; none ships
// (docs/dialogue-localisation.md).
//
// Kept from the original, deliberately:
//   - the path is sprintf'd into a 0x28-byte stack buffer with no length check
//     (the longest shipped name fits);
//   - the malloc results are not checked for null;
//   - a chunk whose kind is outside 0..3 (including negative: the byte is
//     sign-extended and compared unsigned) is skipped by its size, not
//     rejected - except kinds 4 to 19, which are ours (DIV-0006, DIV-0008,
//     DIV-0014, DIV-0015, DIV-0018, DIV-0019, DIV-0020, DIV-0052, DIV-0057,
//     DIV-0038, DIV-0064, DIV-0069, DIV-0148, kind 18 the reading row's,
//     docs/reading-row.md 3, taken only from an optional layer, and kind 19
//     DIV-0203's corrected text block);
//   - the walk trusts each chunk's size; nothing checks that a payload lies
//     inside the file buffer or that a kind-0 tag lies inside the arena;
//   - the kind-3 copy is never freed here: Font_SetGlyphData owns it (and
//     frees the previous one).
extern "C" void __cdecl LoadDatFile(int file_index) {
    const char* name = Dat_FileNames[file_index];
    if (dat_load::g_datlog) {
        dat_load::g_datlog_caller = __builtin_return_address(0);
        dat_load::g_datlog_index = file_index;
        if (!name) dat_load::DatLog("none", "(no name: returns)");
    }
    if (!name) return;

    if (dat_cache::WalkShipped(name, dat_load::g_take, nullptr)) {  // DIV-0089: the cache's when it holds it whole
        if (dat_load::g_datlog) dat_load::DatLog("cache", name);
    } else {
        char path[0x28];
        Crt_sprintf(path, "DAT\\%s", name);
        WalkDatFile(path);
    }

    if (dat_load::g_lang[0] && std::strlen(name) < 0x20) {  // DIV-0005
        char overlay[0x30];
        Crt_sprintf(overlay, "DAT\\%s.%s", dat_load::g_lang, name);
        dat_load::WalkOverlay("loc", dat_load::g_lang, dat_load::g_lang_cached, name, overlay);  // DIV-0089
    }
    for (int i = 0; i < dat_load::g_opt_count; ++i) {  // DIV-0086
        char layer[kOptPath];
        if (4 + std::strlen(dat_load::g_opt[i]) + 1 + std::strlen(name) + 1 > sizeof layer)
            bof3::Fatal("DIV-0086: DAT\\%s.%s does not fit the 0x%X-byte path buffer", dat_load::g_opt[i], name,
                        static_cast<unsigned>(sizeof layer));
        Crt_sprintf(layer, "DAT\\%s.%s", dat_load::g_opt[i], name);
        dat_load::g_walking_opt_layer = true;   // the reading row's kind 18 may arrive (ReadingRowTake)
        dat_load::WalkOverlay("opt", dat_load::g_opt[i], dat_load::g_opt_cached[i], name, layer);  // DIV-0089
        dat_load::g_walking_opt_layer = false;
    }
    if (dat_load::g_area_block_loaded) {
        dat_load::g_area_block_loaded = false;
        map_layers::SnapshotSides();  // DIV-0085: the side faces the file's heights give (map_layers.h)
    }
    // DIV-0147: the quick menu's text is the title's load menu's, kept from
    // the title's pool (a battle's pool has other messages at its ids).
    if (_stricmp(name, "FIRST.DAT") == 0) bof3::QuickMenu_TitlePoolLoaded();
}

#pragma clang attribute push(__attribute__((disable_tail_calls)), apply_to = function)

//
// DIVERGENCE DIV-0113: then each optional layer's DAT\<name> (BOF3X_OPT, by
// LoadDatFile's precedence), its kind-2 chunks to Snd_LoadBank too, so a
// layer's bank (psp-chord-one-tone) replaces the container's here as it does
// in LoadDatFile. No layer named: nothing more.
extern "C" void __cdecl Snd_LoadBankFile(unsigned index) {
    const U name = LongSaveMenu(::bof3::Img(save_menu::at::kDatNames.v) + index * 4);
    if (name == 0) return;
    const char* const text = reinterpret_cast<const char*>(AtSaveMenu(name));
    if (dat_cache::WalkShipped(text, TakeBank, nullptr)) {  // DIV-0089
        DatLoad_OptLayerChunks(text, TakeBank);             // DIV-0113
        return;
    }
    char path[0x28];
    save_menu::g.sprintf(path, reinterpret_cast<const char*>(AtSaveMenu(::bof3::Img(save_menu::at::kDatFormat.v))), reinterpret_cast<const char*>(AtSaveMenu(name)));
    const int handle = save_menu::g.file_open(path, 0, 0);
    if (handle == -1) return;
    const int size = save_menu::g.file_size(handle);
    unsigned char* const data = static_cast<unsigned char*>(save_menu::g.malloc(static_cast<unsigned>(size)));
    save_menu::g.file_read(handle, data, static_cast<unsigned>(size));
    save_menu::g.file_close(handle);
    if (size > 0) {
        int offset = 0;
        do {
            const U chunk = AddrSaveMenu(data) + static_cast<U>(offset);
            const U bytes = LongSaveMenu(chunk + 8);
            if (Byte(chunk) == 2) save_menu::g.load_bank(LongSaveMenu(chunk + 4), AtSaveMenu(chunk + 0x10), bytes);
            offset = static_cast<int>(static_cast<U>(offset) + bytes + 0x10);
        } while (offset < size);
    }
    save_menu::g.free(data);
    DatLoad_OptLayerChunks(text, TakeBank);  // DIV-0113
}

#pragma clang attribute pop

// original 0x454810: `mov eax, 1; ret`. Where the PSX polled its CD load
// (File_LoadDone 0x801636F0), the PC's loads are synchronous, so every wait
// loop's first question is answered "done".
extern "C" int __cdecl File_LoadDone(void) { return 1; }

void DragonCmd_Inject() {
    BOF3_INJECT(Battle_StatusResistRate);
    BOF3_INJECT(Battle_StatusResistedMask);
    BOF3_INJECT(Battle_StatusResisted20);
    BOF3_INJECT(Battle_StatusResistRate20);
    BOF3_INJECT(Battle_StatusResisted80);
    BOF3_INJECT(Effect_NoHitReaction);
    BOF3_INJECT(Effect_RollStatStepQuiet);
    BOF3_INJECT(Effect_RollStatStep);
    BOF3_INJECT(Effect_RollInflictQuiet);
    BOF3_INJECT(Effect_RollInflict);
    BOF3_INJECT(Effect_HpBasedDamage);
    BOF3_INJECT(BattleForm_ApplyStats);
    BOF3_INJECT(DragonCmd_PartDispatch);
    BOF3_INJECT(DragonCmd_LoadDispatch);
    BOF3_INJECT(DragonCmd_LoadStart);
    BOF3_INJECT(DragonCmd_LoadWait);
    BOF3_INJECT(DragonCmd_Open);
    BOF3_INJECT(DragonCmd_MenuDispatch);
    BOF3_INJECT(DragonCmd_MenuInput);
    BOF3_INJECT(DragonCmd_MenuPick);
    BOF3_INJECT(DragonCmd_MenuClose);
    BOF3_INJECT(DragonCmd_MenuCancel);
    BOF3_INJECT(DragonCmd_MenuCancelLoad);
    BOF3_INJECT(DragonCmd_SlotsDispatch);
    BOF3_INJECT(DragonCmd_SlotsCloseMenu);
    BOF3_INJECT(DragonCmd_SlotsOpen);
    BOF3_INJECT(DragonCmd_SlotsCursor);
    BOF3_INJECT(DragonCmd_SlotAffordable);
    BOF3_INJECT(DragonCmd_SlotsCancel);
    BOF3_INJECT(DragonCmd_SlotsBack);
    BOF3_INJECT(DragonCmd_SlotsConfirm);
    BOF3_INJECT(DragonCmd_SlotsClose);
    BOF3_INJECT(DragonCmd_GenesDispatch);
    BOF3_INJECT(DragonCmd_GenesOpen);
    BOF3_INJECT(DragonCmd_GenesPick);
    BOF3_INJECT(DragonCmd_GenesConfirm);
    BOF3_INJECT(DragonCmd_GenesClose);
    BOF3_INJECT(DragonCmd_Commit);
    BOF3_INJECT(DragonCmd_Slots2Dispatch);
    BOF3_INJECT(DragonCmd_Slots2CloseMenu);
    BOF3_INJECT(DragonCmd_Slots2Open);
    BOF3_INJECT(DragonCmd_Slots2Cursor);
    BOF3_INJECT(DragonCmd_Slot2Affordable);
    BOF3_INJECT(DragonCmd_Slots2Cancel);
    BOF3_INJECT(DragonCmd_Slots2Back);
    BOF3_INJECT(DragonCmd_Slots2Confirm);
    BOF3_INJECT(DragonCmd_Slots2Close);
    BOF3_INJECT(DragonCmd_StoreDispatch);
    BOF3_INJECT(DragonCmd_StoreOpen);
    BOF3_INJECT(DragonCmd_StoreWait);
    BOF3_INJECT(DragonCmd_StoreCursor);
    BOF3_INJECT(DragonCmd_StoreAsk);
    BOF3_INJECT(DragonCmd_StoreCopy);
    BOF3_INJECT(DragonCmd_StoreSource);
    BOF3_INJECT(DragonHistory_Leave);
    BOF3_INJECT(DragonForm_Transform);
    BOF3_INJECT(DragonForm_FindRecipe);
    BOF3_INJECT(DragonForm_RecipeSlotHeld);
    BOF3_INJECT(DragonForm_TryPartyRecipe);
    BOF3_INJECT(DragonForm_Mix);
    BOF3_INJECT(DragonForm_StatShift);
    BOF3_INJECT(DragonForm_SetMixBytes);
    BOF3_INJECT(DragonForm_MixAbilities);
    BOF3_INJECT(DragonForm_AddAbilityRow);
    BOF3_INJECT(DragonForm_ApplyRecipe);
    BOF3_INJECT(DragonForm_PartyRecipe);
    BOF3_INJECT(DragonGenes_Held);
    BOF3_INJECT(DragonGenes_SumCost);
    BOF3_INJECT(RestoreForm_Task);
    BOF3_INJECT(RestoreForm_Start);
    BOF3_INJECT(RestoreForm_Wait);
    BOF3_INJECT(BattleFxDash_Dispatch);
    BOF3_INJECT(BattleFxDash_Start);
    BOF3_INJECT(BattleFxDash_WaitPose);
    BOF3_INJECT(BattleFxDash_Rise);
    BOF3_INJECT(BattleFxDash_Advance);
    BOF3_INJECT(BattleFxDash_Return);
    BOF3_INJECT(BattleFxDash_Arc);
    BOF3_INJECT(BattleFxDash_Land);
    BOF3_INJECT(BattleFxPose_Dispatch);
    BOF3_INJECT(BattleFxPose_Start);
    BOF3_INJECT(BattleFxPose_WaitOwner);
    BOF3_INJECT(BattleFxTrail_Dispatch);
    BOF3_INJECT(BattleFxTrail_Start);
    BOF3_INJECT(Battle_RollPendingFlag);
    BOF3_INJECT(Battle_AutoTargetCheck);
    BOF3_INJECT(Battle_RandomLiveMember);
    BOF3_INJECT(Battle_RandomLiveEnemy);
    BOF3_INJECT(Battle_SetTargetFlags);
    BOF3_INJECT(Battle_SetTargetFlag40);
    BOF3_INJECT(Battle_AnyFlagF0);
    BOF3_INJECT(Battle_ActionSuitsTarget);
    BOF3_INJECT(Battle_ItemSuitsTarget);
    BOF3_INJECT(Battle_RecalcStats);
    BOF3_INJECT(Battle_RecalcMemberStats);
    BOF3_INJECT(Battle_PickFlag8Member);
    BOF3_INJECT(Battle_MemberRollByAction);
    BOF3_INJECT(Battle_MemberCoinFlip);
    BOF3_INJECT(Battle_ActionBitSet);
    BOF3_INJECT(Battle_MemberListFull);
    BOF3_INJECT(Battle_SettleFlag8);
    BOF3_INJECT(Formation_ApplyStatMods);
    BOF3_INJECT(Battle_SetDamagePopup);
    BOF3_INJECT(Battle_SetApPopup);
    BOF3_INJECT(Battle_MemberAutoTarget);
    BOF3_INJECT(Battle_MemberOutAction);
    BOF3_INJECT(Battle_MemberActionIs0E);
    BOF3_INJECT(Battle_MemberAutoFixed);
    BOF3_INJECT(Battle_RandomOtherMember);
    BOF3_INJECT(Battle_PlayHitSound);
    BOF3_INJECT(Battle_SetHitPopup);
    BOF3_INJECT(LoadDatFile);
    BOF3_INJECT(Snd_LoadBankFile);
    BOF3_INJECT(File_LoadDone);
}

// Round twelve group BE4 (docs/takeover-queue-field-battle.md section 3): the
// battle engine's 56 functions of 0x444660..0x44AAC9 that
// analysis/round12_cut.tsv lists for BE4 - each read to its last instruction
// with capstone (2026-09-29) and taken through the boss harness as an engine
// group (boss_harness.h, docs/boss_harness.md section 10). docs/battle_e4.md
// has every function one row each.
//
//   - two screen tiles (BattleWin_DimScreen, BattleWin_DrawTileTint);
//   - the turn and damage helpers: the enemy side's pace test, the downward
//     target search, the two hit-or-miss rolls, the party's mean +0xA6, the
//     HP tenth, the records reloaded and written back, the turn order's
//     front, the auto-battle's commands, the two vector turns, a member's
//     reaction roll, the enemy target pick, the status counter's roll, an
//     item given back to its slot, the end phase's three entries;
//   - the item command's states 5..9 (BattleItemCmd_States by 0x904AA3):
//     a whole side's target, the equipment window (its two options, the six
//     slots, the candidates, the equipment put on and taken off) and the
//     equipped item's own target pick, with their two dispatchers;
//   - the escape (0x44A000's table by 0x904AA3, unowned): the roll and its
//     chance, the two paths' steps and their two dispatchers;
//   - three text helpers (a name into Text_Records[0], a banner line).
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. Where
// the original divides by a count that can be 0 (Battle_PartyDefenceMean,
// Escape_Roll), indexes its own six stack pointers past the sixth
// (BattleEquip_RemoveSlot) or jumps through a table past its end (the four
// dispatchers), ours aborts with a message (round9 doc section 6). Every call
// goes through the harness (BH_CALL / BH_AT), so the start-up fuzz can stand
// recorders in for the callees.
#include "game/battle_e4.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/battle_e4_callees.h"
#include "game/boss_harness.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = battle_e4::at;
using U = std::uint32_t;
// A named .data table's address (symbols.gen.h binds the name to a typed pointer).
template <typename T> U AddressOf(T* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

unsigned char& B(U address) { return At(address)[0]; }
unsigned char* Ptr(U cell) { return At(static_cast<U>(Long(At(cell)))); }
// The party's records by a byte (0..2 in play), the enemies' objects by an
// actor byte (3..10: object actor - 3), as every original indexes them - the
// byte alone, the product wrapping as 32-bit arithmetic does.
unsigned char* Party(unsigned member) { return At(at::kParty + (member & 0xFF) * at::kPartyStride); }
unsigned char* Enemy(unsigned actor) { return At(at::kEnemies + ((actor & 0xFF) - 3u) * at::kEnemyStride); }
unsigned char* CharRecord(unsigned index) { return At(at::kCharRecords + (index & 0xFF) * at::kCharStride); }
unsigned char* MenuActor() { return Ptr(at::kMenuActor); }
unsigned char* Command() { return Ptr(at::kCommand); }
std::int32_t S32(U v) { return static_cast<std::int32_t>(v); }
short S16(unsigned v) { return static_cast<short>(static_cast<unsigned short>(v)); }
unsigned Low(U v) { return v & 0xFF; }

// fild dword / fstp dword: an int to the nearest float.
void PutFloat(unsigned char* p, std::int32_t v) {
    const float f = static_cast<float>(v);
    std::memcpy(p, &f, sizeof f);
}

// The last-pushed entry of the battle message queue: ((write - 1) & 15) * 8.
unsigned LastQueueEntry() { return ((B(at::kQueueWrite) - 1u) & 0xF) * 8; }

using Handler = void (__cdecl*)();
using ItemFlagsFn = unsigned char (__cdecl*)(unsigned, unsigned);
using VoidFn = void (__cdecl*)();
using ActorFn = void (__cdecl*)(unsigned);

// The item's flag byte (0x591810, Item_UseFlags - ours since group TWO, 2026-10-06).
unsigned char ItemFlags(unsigned category, unsigned item) { return BH_AT(ItemFlagsFn, at::kItemFlags)(category, item); }

// jmp [table + 4 * (dword 0x904AA4 & 0xFF)]: the table's `entries`
// handlers, a Fatal past them (the original jumps through the dword after).
void DispatchStep4(const char* who, U table, unsigned entries) {
    const unsigned state = static_cast<U>(Long(At(at::kStep4))) & 0xFF;
    if (state >= entries)
        bof3::Fatal("%s: the sub-state 0x904AA4 is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/battle_e4.md section 7)",
                    who, state, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<U>(Long(At(table + 4 * state)))))();
}

// The screen tiles' shared head: Gpu_GetTPage(0, 2, 0x3C0, 0); Gpu_SetDrawMode
// at Gfx_PacketNext with the page (the fifth word is the 0 the original left
// on the stack from the first call's pushes); Gfx_CommitPrim(1, 0xC); then
// Gpu_SetTile at the pointer read again. Answers the tile.
unsigned char* TileHead() {
    const unsigned tpage = BH_CALL(Gpu_GetTPage)(0, 2, 0x3C0, 0) & 0xFFFF;
    BH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, tpage, 0);
    BH_CALL(Gfx_CommitPrim)(1, 0xC);
    unsigned char* const p = Gfx_PacketNext;
    BH_CALL(Gpu_SetTile)(p);
    return p;
}

// The members' records refreshed from their character records after an
// equipment change (BattleEquip_Apply, BattleEquip_RemoveSlot, both the same
// code): for each of the party count's members, +0x92..+0x97 from the
// character's +0x12..+0x17 and the eight dwords +0xC0 from its +0x20;
// Formation_ApplyStatMods; the present members' +0xC0 block into +0xA0; BE5's
// 0x44FDE0; BE6's 0x453300 for each member, the count read again after each.
void RefreshMembers() {
    const unsigned n = static_cast<U>(Long(At(at::kPartyCount))) & 0xFF;
    for (unsigned k = 0; k < n; ++k) {
        unsigned char* const p = Party(k);
        const unsigned char* const r = CharRecord(p[0x148]);
        for (unsigned i = 0; i < 6; ++i) p[0x92 + i] = r[0x12 + i];
        std::memcpy(p + 0xC0, r + 0x20, 0x20);
    }
    BH_CALL(Formation_ApplyStatMods)();
    for (unsigned k = 0; k < 3; ++k) {
        unsigned char* const p = Party(k);
        if (p[0] & 1) std::memcpy(p + 0xA0, p + 0xC0, 0x20);
    }
    BH_AT(VoidFn, at::kAfterEquip)();
    unsigned char n2 = B(at::kPartyCount);
    if (n2 == 0) return;
    for (unsigned char m = 0;;) {
        BH_AT(ActorFn, at::kMemberRefresh)(m);
        n2 = B(at::kPartyCount);
        ++m;
        if (m >= n2) break;
    }
}

// The character record of the member the slots window shows: record 18's +0xC
// through 0x904065 and 0x66972C.
unsigned char* SlotsCharacter() {
    const unsigned member = static_cast<U>(Long(At(at::kSlots + 0xC))) & 0xFF;
    return CharRecord(B(at::kCharOf + B(at::kMemberChar + member)));
}

// Up (0x1000) and down (0x4000) on the six slots: record 18's cursor +0xA
// one back (below 0 to 5) or on (above 5 to 0), cue 0x100 each.
void SlotCursor(unsigned repeat) {
    if (repeat & 0x1000) {
        BH_CALL(Sound_PlayEffect)(0x100);
        const auto c = static_cast<unsigned char>(B(at::kSlots + 0xA) - 1);
        B(at::kSlots + 0xA) = c;
        if (static_cast<signed char>(c) < 0) B(at::kSlots + 0xA) = 5;
    }
    if (repeat & 0x4000) {
        BH_CALL(Sound_PlayEffect)(0x100);
        const auto c = static_cast<unsigned char>(B(at::kSlots + 0xA) + 1);
        B(at::kSlots + 0xA) = c;
        if (static_cast<signed char>(c) > 5) B(at::kSlots + 0xA) = 0;
    }
}

// The hand beside the six slots (record 19 on unless record 18's +3 is set):
// x = record 18's x + 6, y = 13 * cursor + record 18's y + 0x57.
void SlotHand() {
    if (B(at::kSlots + 3) == 0) {
        B(at::kHand) = 1;
        SetWord(At(at::kHand + 4), static_cast<unsigned>(Long(At(at::kSlots + 4))) + 6);
        SetWord(At(at::kHand + 6), 13u * B(at::kSlots + 0xA) + Word(At(at::kSlots + 6)) + 0x57);
    } else {
        B(at::kHand) = 0;
    }
}

// The equipped item's word (the member's +0x126: category << 8 | item) and its
// flag byte, for the member choosing.
unsigned MemberItem() { return Word(Party(MenuActor()[5]) + 0x126); }

// A side's target by the flag byte: bit 0x20 the enemies (0x40), else the
// party (0x80) - neg / sbb / and 0xC0 / add 0x80.
unsigned char SideTarget(unsigned char flags) { return (flags & 0x20) ? 0x40 : 0x80; }

// Left and right on the enemy side (0x449540), as BattleItemCmd_PickEnemy's:
// the target one on / one back wrapped to 3 .. enemy count + 2, through
// Battle_DefaultTarget / Battle_PrevTarget; the command pointer read before
// and again after the calls.
void StepEnemyTarget(unsigned repeat) {
    if (repeat & 0x2000) {
        const long high = static_cast<long>(B(at::kEnemyCount)) + 2;
        const long value = static_cast<long>(static_cast<signed char>(Command()[0])) + 1;
        const long wrapped = BH_CALL(Battle_WrapIndex)(high, 3, value);
        const unsigned char t = BH_CALL(Battle_DefaultTarget)(static_cast<unsigned>(wrapped));
        Command()[0] = t;
        BH_CALL(Sound_PlayEffect)(0x101);
    }
    if (repeat & 0x8000) {
        const long high = static_cast<long>(B(at::kEnemyCount)) + 2;
        const long value = static_cast<long>(static_cast<signed char>(Command()[0])) - 1;
        const long wrapped = BH_CALL(Battle_WrapIndex)(high, 3, value);
        const unsigned char t = BH_CALL(Battle_PrevTarget)(static_cast<unsigned>(wrapped));
        Command()[0] = t;
        BH_CALL(Sound_PlayEffect)(0x101);
    }
}

// The escape's field step: the kind-2 x / z high words (0x905E66, 0x905E62)
// moved by the pair of s8 at 0x64E4F4 + 2 * (dword 0x904AAC & 0xFF), `sign`
// +1 or -1, 16-bit.
void FieldStep(int sign) {
    const unsigned d = static_cast<U>(Long(At(at::kFaceStep))) & 0xFF;
    const auto dx = static_cast<short>(static_cast<signed char>(B(at::kFacePairs + 2 * d)));
    const auto dz = static_cast<short>(static_cast<signed char>(B(at::kFacePairs + 2 * d + 1)));
    SetWord(At(at::kKind2X + 2), static_cast<unsigned>(Word(At(at::kKind2X + 2)) + sign * dx));
    SetWord(At(at::kKind2Z + 2), static_cast<unsigned>(Word(At(at::kKind2Z + 2)) + sign * dz));
}

// The two option messages above the list (Msg 0x2F / 0x31, or 0x30 / 0x32
// with `base` 0x30, by record 21's option +0xA) into the last queue entry.
const unsigned char* OptionMessage(unsigned base) { return BH_CALL(Msg_SystemPtr)(B(at::kAbove + 0xA) ? base + 2 : base); }

}  // namespace

// ===========================================================================
// The screen tiles
// ===========================================================================

// original 0x444660 (PSX 0x801D98D0, gap44): the whole screen dimmed - a TILE
// at (0, 0) of 960.0 x 240.0 in grey 0x28, semi-transparent (mode 1), after
// the draw mode for the page (0x3C0, 0). BE1's 0x42EE00 calls it.
extern "C" void __cdecl BattleWin_DimScreen(void) {
    unsigned char* const p = TileHead();
    SetLong(p + 8, 0);
    SetLong(p + 0xC, 0);
    SetLong(p + 0x14, 0x44700000);   // 960.0f
    SetLong(p + 0x18, 0x43700000);   // 240.0f
    p[4] = 0x28;
    p[5] = 0x28;
    p[6] = 0x28;
    BH_CALL(Gpu_SetSemiTrans)(p, 1);
    BH_CALL(Gfx_CommitPrim)(1, 0x1C);
}

// original 0x4446E0 (PSX 0x801D997C, gap44): BattleWin_DrawTile's tile (s16 x,
// s16 y; w, h the u16 pair of 0x64E268 by the size byte) in the colour of the
// three bytes 0x903850..0x903852, semi-transparent (mode 1).
extern "C" void __cdecl BattleWin_DrawTileTint(int x, int y, int size) {
    unsigned char* const p = TileHead();
    PutFloat(p + 8, S16(static_cast<U>(x)));
    PutFloat(p + 0xC, S16(static_cast<U>(y)));
    const unsigned char* const t = At(at::kTileSizes + (Low(static_cast<U>(size)) << 2));
    PutFloat(p + 0x14, Word(t));
    PutFloat(p + 0x18, Word(t + 2));
    p[4] = B(at::kTint);
    p[5] = B(at::kTint + 1);
    p[6] = B(at::kTint + 2);
    BH_CALL(Gpu_SetSemiTrans)(p, 1);
    BH_CALL(Gfx_CommitPrim)(1, 0x1C);
}

// ===========================================================================
// Turn and damage helpers
// ===========================================================================

// original 0x445600 (PSX 0x801DB3AC, gap74): Battle_PartyOutpaces' enemy twin -
// 1 when the enemy's u16 +0xB8 is at least twice the average's low word
// (signed compare of the two ints) and at least the highest's low word.
extern "C" unsigned char __cdecl Battle_EnemyOutpaces(unsigned actor, unsigned average, unsigned highest) {
    const unsigned pace = Word(Enemy(actor) + 0xB8);
    if (S32((average & 0xFFFF) << 1) > S32(pace)) return 0;
    if ((highest & 0xFFFF) > pace) return 0;
    return 1;
}

// original 0x4457F0 (PSX 0x801DB6BC, gap74): Battle_DefaultTarget's downward
// twin. From the actor byte down within its side (0..2, or 3..10), the first
// actor Battle_ActorIsOut answers 0 for; then from the side's top (2, or 10)
// down to just above the byte; 0xFF if none. The comparisons are signed bytes,
// as the original's: a byte of 0x80..0xFF counts as below 3 and searches 10
// down through the bytes that follow 0 (0xFF, 0xFE ...) to just above it.
extern "C" unsigned char __cdecl Battle_PrevTarget(unsigned actor) {
    const auto start = static_cast<unsigned char>(actor);
    const auto s = static_cast<signed char>(start);
    if (start < 3) {
        for (signed char i = s;; --i) {
            if (BH_CALL(Battle_ActorIsOut)(static_cast<unsigned char>(i)) == 0) return static_cast<unsigned char>(i);
            if (static_cast<signed char>(i - 1) < 0) break;
        }
        if (s >= 2) return 0xFF;
        for (signed char i = 2;; --i) {
            if (BH_CALL(Battle_ActorIsOut)(static_cast<unsigned char>(i)) == 0) return static_cast<unsigned char>(i);
            if (!(static_cast<signed char>(i - 1) > s)) return 0xFF;
        }
    }
    if (s >= 3) {
        for (signed char i = s;; --i) {
            if (BH_CALL(Battle_ActorIsOut)(static_cast<unsigned char>(i)) == 0) return static_cast<unsigned char>(i);
            if (!(static_cast<signed char>(i - 1) >= 3)) break;
        }
    }
    if (s >= 10) return 0xFF;
    for (signed char i = 10;; --i) {
        if (BH_CALL(Battle_ActorIsOut)(static_cast<unsigned char>(i)) == 0) return static_cast<unsigned char>(i);
        if (!(static_cast<signed char>(i - 1) > s)) return 0xFF;
    }
}

// original 0x446110 (no PSX twin in the pairs): an attack on a member, hit or
// missed. With the round flag 0x80 (0x904AA8) the amount stands. Else a miss
// when the attacker's status bit 8 (a member's +0x90, an enemy's +0x92) is set
// and Rand's bit 1 is set, or when 0x939F9B is not below Rand() % 100; a
// miss clears the target member's +0x12C bit 0x10 and answers 0. ax out.
extern "C" int __cdecl Battle_HitOrMissParty(int amount, unsigned attacker, unsigned target) {
    const auto hit = static_cast<int>(static_cast<U>(amount) & 0xFFFF);
    if (B(at::kFlags) & 0x80) return hit;
    const unsigned a = Low(attacker);
    const bool dazed = a <= 2 ? (Party(a)[0x90] & 8) != 0 : (Enemy(a)[0x92] & 8) != 0;
    if (!dazed || (BH_CALL(Rand)() & 2) == 0) {
        const int roll = BH_CALL(Rand)() % 100;
        if (static_cast<int>(B(at::kEvadeParty)) < roll) return hit;
    }
    Party(target)[0x12C] &= 0xEF;
    return 0;
}

// original 0x4461B0 (no PSX twin in the pairs): the same on an enemy. The
// round flag 0x80: the amount. The attacker's status bit 8 and Rand's bit 1: a
// miss. A member attacking with +0x134 bit 0x100: a hit. The target's +0x90
// bit 4 and Rand's bit 1: a miss. An enemy attacking: a hit. Else a hit when
// the low byte of 0x939FFC is above Rand() % 100. A miss clears the target
// enemy's +0x10C bit 0x10 and answers 0. ax out.
extern "C" int __cdecl Battle_HitOrMissEnemy(int amount, unsigned attacker, unsigned target) {
    const auto hit = static_cast<int>(static_cast<U>(amount) & 0xFFFF);
    if (B(at::kFlags) & 0x80) return hit;
    const unsigned a = Low(attacker);
    const bool dazed = a <= 2 ? (Party(a)[0x90] & 8) != 0 : (Enemy(a)[0x92] & 8) != 0;
    if (dazed && (BH_CALL(Rand)() & 2)) {
        Enemy(target)[0x10C] &= 0xEF;
        return 0;
    }
    if (a <= 2 && (static_cast<U>(Long(Party(a) + 0x134)) & 0x100)) return hit;
    unsigned char* const e = Enemy(target);
    if ((e[0x90] & 4) == 0 || (BH_CALL(Rand)() & 2) == 0) {
        if (a > 2) return hit;
        const int roll = BH_CALL(Rand)() % 100;
        if (static_cast<int>(static_cast<U>(Long(At(at::kHitEnemy))) & 0xFF) > roll) return hit;
    }
    e[0x10C] &= 0xEF;
    return 0;
}

// original 0x4463E0 (no PSX twin in the pairs): the party's mean u16 +0xA6
// over the count (the dword 0x904AB0's low byte; the sum kept to 16 bits, as
// the original's `and eax, 0xFFFF`), plus the u16 0x939F86, halved toward 0.
// The original divides by the count unchecked: ours aborts at 0.
extern "C" int __cdecl Battle_PartyDefenceMean(void) {
    const unsigned n = static_cast<U>(Long(At(at::kPartyCount))) & 0xFF;
    U sum = 0;
    for (unsigned i = 0; i < n; ++i) sum += Word(Party(i) + 0xA6);
    if (n == 0)
        bof3::Fatal("Battle_PartyDefenceMean: the party count 0x904AB0 is 0 - the original divides by it (docs/battle_e4.md "
                    "section 7)");
    const int mean = static_cast<int>(sum & 0xFFFF) / static_cast<int>(n);
    const int v = mean + static_cast<int>(Word(At(at::kPartyDefBonus)));
    return v / 2;
}

// original 0x446540 (PSX 0x801DCEC0): with status 0x80 (a member's +0x90, an
// enemy's +0x92) the HP change word (a member's +0x128, an enemy's +0x108) is
// a tenth of the HP (+0x98 / +0xA4), rounded: ((hp * 10) / 10 + 5) / 10.
extern "C" void __cdecl Battle_SetHpChange(unsigned actor) {
    const unsigned a = Low(actor);
    auto tenth = [](unsigned hp) {
        const int x = static_cast<int>(hp) * 10;
        return static_cast<unsigned>((x / 10 + 5) / 10);
    };
    if (a <= 2) {
        unsigned char* const p = Party(a);
        if (p[0x90] & 0x80) SetWord(p + 0x128, tenth(Word(p + 0x98)));
        return;
    }
    unsigned char* const e = Enemy(a);
    if (e[0x92] & 0x80) SetWord(e + 0x108, tenth(Word(e + 0xA4)));
}

// original 0x446600 (PSX Battle_ReloadPartyRecords 0x801DD054, the sibling's
// name): for each of the count's members (the dword 0x904AB0's low byte),
// 0x29 dwords of its character record (by its +0x148) over its +0x80.
extern "C" void __cdecl Battle_ReloadPartyRecords(void) {
    const unsigned n = static_cast<U>(Long(At(at::kPartyCount))) & 0xFF;
    for (unsigned i = 0; i < n; ++i) {
        unsigned char* const p = Party(i) + 0x80;
        std::memcpy(p, CharRecord(p[0xC8]), 0x29 * 4);
    }
}

// original 0x446700 (no PSX twin in the pairs): the turn order's front moved
// down one (0x904AE2 - 1, stored first) and the actor put there (0x904ACC +
// the new front).
extern "C" void __cdecl Battle_OrderPushFront(unsigned actor) {
    const auto front = static_cast<unsigned char>(B(at::kOrderCursor) - 1);
    B(at::kOrderCursor) = front;
    B(at::kOrder + front) = static_cast<unsigned char>(actor);
}

// original 0x446720 (PSX AutoBattle_FillCommands 0x801DD264, the sibling's
// name): from the count of commands chosen 0x904AC3 while it is below 3
// (signed), the member at that place of the entry order 0x904AB6 (a signed
// index) - to the first 0xFF - is made 0x904AAE and given command 3 (+0x124)
// with +0x125 = 1; the count is stored after each.
extern "C" void __cdecl AutoBattle_FillCommands(void) {
    auto c = static_cast<signed char>(B(at::kCommandsChosen));
    if (c >= 3) return;
    for (;;) {
        const unsigned char m = B(at::kEntryOrder + static_cast<U>(static_cast<std::int32_t>(c)));
        if (m == 0xFF) return;
        B(at::kAutoMember) = m;
        unsigned char* const p = Party(m);
        c = static_cast<signed char>(c + 1);
        p[0x125] = 1;
        p[0x124] = 3;
        B(at::kCommandsChosen) = static_cast<unsigned char>(c);
        if (c >= 3) return;
    }
}

namespace {
// The pair of dwords at +x / +x + 4 turned by the direction byte +8: 1 (x, z)
// to (-z, x), 2 to (-x, -z), 3 to (z, -x); other values leave them.
void TurnPair(unsigned char* r, unsigned x) {
    const U a = static_cast<U>(Long(r + x));
    const U b = static_cast<U>(Long(r + x + 4));
    switch (r[8]) {
    case 1:
        SetLong(r + x, S32(0u - b));
        SetLong(r + x + 4, S32(a));
        break;
    case 2:
        SetLong(r + x, S32(0u - a));
        SetLong(r + x + 4, S32(0u - b));
        break;
    case 3:
        SetLong(r + x, S32(b));
        SetLong(r + x + 4, S32(0u - a));
        break;
    default:
        break;
    }
}
}  // namespace

// original 0x446770 (PSX 0x801DD318): an effect task's (dx, dz) +0xC / +0x10
// turned by its direction +8 (TurnPair). 70-odd callers: the spells' and the
// action tasks'.
extern "C" void __cdecl Battle_TurnVectorC(unsigned char* record) { TurnPair(record, 0xC); }

// original 0x4467C0 (PSX 0x801DD394, gap31): the same on +0x18 / +0x1C.
extern "C" void __cdecl Battle_TurnVector18(unsigned char* record) { TurnPair(record, 0x18); }

// original 0x446810 (PSX 0x801DD410): whether the member Field_State reacts,
// its callers BE3's 0x441D80 (which set the round flag 0x40 on a 1). The flag
// already set: cleared (a 16-bit and), 0. Else 0 when Field_State's +0x130
// bit 0, an acting actor below 3, +0x90 & 0x4864, kind 4 with ability 0xA1,
// or 0x904B8E set without +0x134 bit 0x10; 1 with +0x130 bit 15; else 1 when
// +0xB9 is above Rand() % 100 (Field_State read again after Rand).
extern "C" unsigned char __cdecl Battle_MemberReactRoll(void) {
    if (B(at::kFlags) & 0x40) {
        SetWord(At(at::kFlags), Word(At(at::kFlags)) & 0xFFBF);
        return 0;
    }
    unsigned char* const fs = Field_State;
    const U word = static_cast<U>(Long(fs + 0x130));
    if (word & 1) return 0;
    if (B(at::kActor) < 3) return 0;
    if (Word(fs + 0x90) & 0x4864) return 0;
    if (B(at::kActionKind) == 4 && Word(At(at::kAbility)) == 0xA1) return 0;
    if (B(at::kCharged) != 0 && (fs[0x134] & 0x10) == 0) return 0;
    if (word & 0x8000) return 1;
    const int roll = BH_CALL(Rand)() % 100;
    return static_cast<int>(Field_State[0xB9]) > roll ? 1 : 0;
}

// original 0x446A80 (PSX Battle_WriteBackMember 0x801DD874, the sibling's
// name): a present member's (+0 bit 0) HP +0x98, +0x9A and +0x9C into its
// character record's +0x18, +0x1A, +0x1C; its +0x90 kept to bits 0x60A0 and
// copied to the record's +0x10. The member byte alone indexes (0..2 by its
// only caller, Battle_WriteBackParty).
extern "C" void __cdecl Battle_WriteBackMember(unsigned member) {
    unsigned char* const p = Party(member);
    if ((p[0] & 1) == 0) return;
    unsigned char* const r = CharRecord(p[0x148]);
    SetWord(r + 0x18, Word(p + 0x98));
    SetWord(r + 0x1A, Word(p + 0x9A));
    const unsigned char c = p[0x9C];
    SetWord(p + 0x90, Word(p + 0x90) & 0x60A0);
    const unsigned s = Word(p + 0x90);
    r[0x1C] = c;
    SetWord(r + 0x10, s);
}

// original 0x446B00 (PSX 0x801DD940): the target 0x904B44 among the enemies not
// out (actors 3..10): first the highest u16 +0x98 - kept as a byte, compared
// with each word zero-extended, as the original has it - then, among those
// whose +0x98 equals that byte, the one with the lowest HP +0xA4 (the last of
// equals); 0 when none. battle_sprites' fallback when an action has no target.
extern "C" void __cdecl Battle_PickEnemyTarget(void) {
    unsigned char top = 0;
    for (unsigned a = 3; a <= 10; ++a) {
        if (BH_CALL(Battle_ActorIsOut)(a) != 0) continue;
        const unsigned char* const e = Enemy(a);
        if (top < Word(e + 0x98)) top = e[0x98];
    }
    unsigned lowest = 0xFFFF;
    unsigned char pick = 0;
    for (unsigned a = 3; a <= 10; ++a) {
        if (BH_CALL(Battle_ActorIsOut)(a) != 0) continue;
        const unsigned char* const e = Enemy(a);
        if (top != Word(e + 0x98)) continue;
        const unsigned hp = Word(e + 0xA4);
        if (lowest < hp) continue;
        lowest = hp;
        pick = static_cast<unsigned char>(a);
    }
    B(at::kTarget) = pick;
}

// original 0x446CB0 (PSX 0x801DDC7C): the status counter's roll (a member's
// +0x12D, an enemy's +0x10D). 0 at 0. The percentage 75 from 3 on, else
// 0x64E3E8's by the counter: 1 when it is not below Rand() % 100. Else 1 when
// (200 - 0x64E3E0's by the actor's +0xB6 / +0xC6) * 50 is not below Rand() %
// 10000.
extern "C" unsigned char __cdecl Battle_WakeRoll(unsigned actor) {
    const unsigned a = Low(actor);
    const unsigned char count = a <= 2 ? Party(a)[0x12D] : Enemy(a)[0x10D];
    if (count == 0) return 0;
    const int chance = count >= 3 ? 0x4B : B(at::kWakeChance + count);
    if (chance >= BH_CALL(Rand)() % 100) return 1;
    const unsigned level = a <= 2 ? Party(a)[0xB6] : Enemy(a)[0xC6];
    const int roll = BH_CALL(Rand)() % 10000;
    return (200 - static_cast<int>(B(at::kWakeResist + level))) * 50 >= roll ? 1 : 0;
}

// original 0x446D90 (PSX 0x801DDE44): an item word (category << 8 | item)
// given back to its inventory slot: the category's count at the slot, 1 at 99
// (0x63) and nothing else; otherwise one more and the id put at the slot, 0.
// The category indexes the list pointers unchecked (4's count pointer is 0).
extern "C" unsigned char __cdecl Battle_ReturnItem(unsigned slot, unsigned item) {
    const unsigned s = Low(slot);
    const unsigned category = (item & 0xFFFF) >> 8;
    unsigned char* const counts = Ptr(at::kInventoryCounts + 4 * category);
    const unsigned char c = counts[s];
    if (c == 0x63) return 1;
    counts[s] = static_cast<unsigned char>(c + 1);
    Ptr(at::kInventoryIds + 4 * category)[s] = static_cast<unsigned char>(item);
    return 0;
}

// original 0x446DE0 (no PSX twin in the pairs): the end phase (0x904AA0 = 5)
// at BattleEnd_Steps' step 1 (0x904AA1), 0x904AA2 = 0. The boss hooks'
// way out on a win (round eleven's 0x446DE0).
extern "C" void __cdecl BattleEnd_EnterStep1(void) {
    B(at::kPhase) = 5;
    B(at::kStep) = 1;
    B(at::kStep2) = 0;
}

// original 0x446E00: the same at step 2.
extern "C" void __cdecl BattleEnd_EnterStep2(void) {
    B(at::kPhase) = 5;
    B(at::kStep) = 2;
    B(at::kStep2) = 0;
}

// original 0x446E20: the same at step 3.
extern "C" void __cdecl BattleEnd_EnterStep3(void) {
    B(at::kPhase) = 5;
    B(at::kStep) = 3;
    B(at::kStep2) = 0;
}

// ===========================================================================
// The item command's states 5..9 and the equipment window
// ===========================================================================

// original 0x447F40 (PSX 0x80094FB0): window record 16 set up as a list for
// the member choosing: +0 1, +1 8, +2 2, +3 2, +0xB 2, +0xA the member's +5,
// +0xD 0xFF, +8 2, +9 0, word +4 0x140, word +6 0x3F, word +0x14 the command
// record's +0x10 & 2; 0x929F06 the member's +5 again; the hand (record 19)
// off, kind 8. BE5's four Dragon-run steps call it.
extern "C" void __cdecl ItemMenu_SetupForMember(void) {
    unsigned char* const actor = MenuActor();
    B(at::kList) = 1;
    B(at::kList + 1) = 8;
    B(at::kList + 2) = 2;
    B(at::kList + 3) = 2;
    B(at::kList + 0xB) = 2;
    B(at::kList + 0xA) = actor[5];
    unsigned char* const command = Command();
    B(at::kList + 0xD) = 0xFF;
    B(at::kList + 8) = 2;
    B(at::kList + 9) = 0;
    const unsigned bits = command[0x10] & 2u;
    SetWord(At(at::kList + 4), 0x140);
    SetWord(At(at::kList + 6), 0x3F);
    SetWord(At(at::kList + 0x14), bits);
    B(at::kGeneMember) = actor[5];
    B(at::kHand) = 0;
    B(at::kHand + 1) = 8;
    B(at::kHand + 2) = 0;
}

// original 0x448BA0 (hidden, 0x64E4A0's entry 0; PSX none in the pairs):
// state 5 begun - a whole side's target by the list item's flag byte (bit
// 0x20 the enemies 0x40, else the party 0x80), sub-state 1, the auto-repeat
// latch zeroed, the pick flag set.
extern "C" void __cdecl BattleItemCmd_SideBegin(void) {
    const unsigned category = B(at::kList + 0xA);
    const unsigned row = static_cast<U>(Long(At(at::kList + 0xC))) & 0xFF;
    const unsigned item = Ptr(at::kInventoryIds + 4 * category)[row];
    const unsigned char flags = ItemFlags(category, item);
    Command()[0] = SideTarget(flags);
    B(at::kStep4) = 1;
    SetWord(At(at::kRepeatLatch), 0);
    B(at::kPicking) = 1;
}

// original 0x448C00 (hidden, 0x64E4A0's entry 1): the side pick. Cancel ->
// sub-state 3, confirm -> 2; any direction Input_AutoRepeat lets through
// (0xF000) with the list item's flag bit 0x80 flips the side (target ^ 0xC0),
// cue 0x101.
extern "C" void __cdecl BattleItemCmd_SidePick(void) {
    const unsigned pad = Input_Pressed;
    if (Field_CancelButtons & pad) {
        B(at::kStep4) = 3;
        return;
    }
    if (Field_ConfirmButtons & pad) {
        B(at::kStep4) = 2;
        return;
    }
    const unsigned repeat = BH_CALL(Input_AutoRepeat)(pad & 0xF000);
    if ((repeat & 0xF000) == 0) return;
    const unsigned category = B(at::kList + 0xA);
    const unsigned row = static_cast<U>(Long(At(at::kList + 0xC))) & 0xFF;
    const unsigned item = Ptr(at::kInventoryIds + 4 * category)[row];
    if (ItemFlags(category, item) & 0x80) {
        Command()[0] ^= 0xC0;
        BH_CALL(Sound_PlayEffect)(0x101);
    }
}

// original 0x448CA0 (hidden, 0x64E4B0's entry 0; PSX 0x8009632C): state 6,
// the window above the list with its two options (record 21's +0xA). Message
// 0x2F / 0x31 by the option into the last queue entry; the hand beside the
// option unless record 21's +3 is set (x = record 21's dword +4 + 48 *
// option, y its +6 + 4); 0x80345F = 0xFF. Left / right (0xA000) flip the
// option, cue 0x101 (Input_Pressed read again). Cancel, or down (0x4000, cue
// 0x100): the window off, record 4 on, back to the list (0x904AA2 = 2,
// 0x904AA3 = 1). Confirm, cue 0x103, the option kept in 0x80345F: option 0
// opens the slots for use (BattleEquip_OpenUse) at sub-state + 3, option 1 the
// slots for a change (BattleEquip_OpenChange) at sub-state + 1, record 17's
// +9 / +0xC the slot cursor and the member.
extern "C" void __cdecl BattleItemCmd_EquipMenu(void) {
    const unsigned char* const text = OptionMessage(0x2F);
    SetLong(At(at::kQueue + 4 + LastQueueEntry()), S32(static_cast<U>(reinterpret_cast<std::uintptr_t>(text))));
    if (B(at::kAbove + 3) == 0) {
        B(at::kHand) = 1;
        SetWord(At(at::kHand + 4), B(at::kAbove + 0xA) * 48u + static_cast<U>(Long(At(at::kAbove + 4))));
        SetWord(At(at::kHand + 6), Word(At(at::kAbove + 6)) + 4u);
    } else {
        B(at::kHand) = 0;
    }
    U pad = static_cast<U>(Long(At(at::kInputPressed)));
    B(at::kAbove + 0xB) = 0xFF;
    if (pad & 0xA000) {
        BH_CALL(Sound_PlayEffect)(0x101);
        B(at::kAbove + 0xA) ^= 1;
        pad = static_cast<U>(Long(At(at::kInputPressed)));
    }
    bool back = false;
    if (Field_CancelButtons & pad) {
        back = true;
    } else if (Field_ConfirmButtons & pad) {
        BH_CALL(Sound_PlayEffect)(0x103);
        const unsigned char option = B(at::kAbove + 0xA);
        B(at::kAbove + 0xB) = option;
        if (option == 0) {
            BH_CALL(BattleEquip_OpenUse)();
            B(at::kStep4) = static_cast<unsigned char>(B(at::kStep4) + 3);
            return;
        }
        BH_CALL(BattleEquip_OpenChange)();
        const auto sub = static_cast<unsigned char>(B(at::kStep4) + 1);
        const unsigned char cursor = B(at::kSlots + 0xA);
        const unsigned char member = B(at::kSlots + 0xC);
        B(at::kCands + 9) = cursor;
        B(at::kCands + 0xC) = member;
        B(at::kStep4) = sub;
        return;
    } else if (pad & 0x4000) {
        BH_CALL(Sound_PlayEffect)(0x100);
        back = true;
    }
    if (!back) return;
    B(at::kAbove) = 0;
    B(at::kRecord4) = 1;
    B(at::kStep2) = 2;
    B(at::kStep3) = 1;
}

// original 0x448DE0 (hidden, 0x64E4B0's entry 1; PSX 0x80096530): the six
// slots for a change. The hand at the cursor; message 0x30 / 0x32 by the
// option into the last queue entry; Input_AutoRepeat(pressed & 0x5000).
// Cancel (cue 0x106): the slots closed (record 18 +3 = 1), the list back on
// at its kept place (record 16 +0 = 1, +3 = 3, words +4 / +6 from record 17's
// +4 / +6), the candidates, the window above and the hand off, record 4 on,
// state 1, sub-state 0. Button 0x10: BattleEquip_RemoveSlot, cue 0x103 when it
// took the slot off, else 0x107. Confirm (cue 0x103): the slot chosen (record
// 18 +0xB), record 17's category +8 and item +0xD 0, its cursor +0xB = its
// top +0xA, 0x929F04 = 0, sub-state + 1. Otherwise up / down move the slot
// (SlotCursor) and record 17's +9 takes it.
extern "C" void __cdecl BattleItemCmd_EquipSlotPick(void) {
    SlotHand();
    const unsigned char* const text = OptionMessage(0x30);
    const unsigned entry = LastQueueEntry();
    const unsigned pressed = Input_Pressed;
    SetLong(At(at::kQueue + 4 + entry), S32(static_cast<U>(reinterpret_cast<std::uintptr_t>(text))));
    const unsigned repeat = BH_CALL(Input_AutoRepeat)(pressed & 0x5000);
    const unsigned pad = Input_Pressed;
    if (Field_CancelButtons & pad) {
        BH_CALL(Sound_PlayEffect)(0x106);
        const unsigned x = Word(At(at::kCands + 4));
        const unsigned y = Word(At(at::kCands + 6));
        B(at::kSlots + 3) = 1;
        B(at::kList) = 1;
        B(at::kRecord4) = 1;
        B(at::kStep3) = 1;
        B(at::kList + 3) = 3;
        SetWord(At(at::kList + 4), x);
        SetWord(At(at::kList + 6), y);
        B(at::kCands) = 0;
        B(at::kAbove) = 0;
        B(at::kHand) = 0;
        B(at::kStep4) = 0;
        return;
    }
    if (pad & 0x10) {
        BH_CALL(Sound_PlayEffect)(BH_CALL(BattleEquip_RemoveSlot)() != 0 ? 0x103 : 0x107);
        return;
    }
    if (Field_ConfirmButtons & pad) {
        BH_CALL(Sound_PlayEffect)(0x103);
        const unsigned char cursor = B(at::kSlots + 0xA);
        const unsigned char top = B(at::kCands + 0xA);
        B(at::kSlots + 0xB) = cursor;
        const auto sub = static_cast<unsigned char>(B(at::kStep4) + 1);
        B(at::kCands + 8) = 0;
        B(at::kCands + 0xD) = 0;
        B(at::kCands + 0xB) = top;
        B(at::kListReset) = 0;
        B(at::kStep4) = sub;
        return;
    }
    SlotCursor(repeat);
    B(at::kCands + 9) = B(at::kSlots + 0xA);
}

// original 0x448FC0 (hidden, 0x64E4B0's entry 2; PSX 0x800967F8): the
// candidates (record 17). The hand at its cursor unless its +3 is set;
// Input_AutoRepeat(pressed & 0xF00C) for its latch, then (pressed & 0x500C)
// for the moves: up / down the cursor +0xB one back (not below 0) / on (not
// above 0x7F), asking a scroll (word +0x10 = 0xF0 above the top, 0x10 at top
// + 7); 4 / 8 a page of 7 (the top +0xA capped at 0x79, the cursor at 0x7F);
// cue 0x100 on a move. The help line of record 17's item (Item_HelpMessage by
// +8, +0xD) into the last queue entry. Cancel (cue 0x106) or a confirmed
// change back to the slots: 0x8033F5 = 1, the cursor and record 18's chosen
// slot 0xFF, sub-state - 1. Else BattleEquip_Preview; confirm with an item:
// cue 0x103, BattleEquip_Apply and back; without: cue 0x107.
extern "C" void __cdecl BattleItemCmd_EquipItemPick(void) {
    if (B(at::kCands + 3) == 0) {
        B(at::kHand) = 1;
        SetWord(At(at::kHand + 4), static_cast<U>(Long(At(at::kCands + 4))) + 7);
        const U rows = static_cast<U>(B(at::kCands + 0xB)) - B(at::kCands + 0xA) + 2;
        SetWord(At(at::kHand + 6), rows * 13 + Word(At(at::kCands + 6)));
    } else {
        B(at::kHand) = 0;
    }
    BH_CALL(Input_AutoRepeat)(Input_Pressed & 0xF00Cu);
    const unsigned repeat = BH_CALL(Input_AutoRepeat)(Input_Pressed & 0x500Cu);
    unsigned char cursor = B(at::kCands + 0xB);
    const unsigned char was = cursor;
    if (repeat & 0x1000) {
        if (cursor > 0) {
            --cursor;
            B(at::kCands + 0xB) = cursor;
        }
        if (cursor < B(at::kCands + 0xA)) SetWord(At(at::kCands + 0x10), 0xF0);
    } else if (repeat & 0x4000) {
        if (cursor < 0x7F) {
            ++cursor;
            B(at::kCands + 0xB) = cursor;
        }
        if (static_cast<int>(cursor) >= static_cast<int>(B(at::kCands + 0xA)) + 7) SetWord(At(at::kCands + 0x10), 0x10);
    } else if (repeat & 4) {
        const unsigned char top = B(at::kCands + 0xA);
        if (top == 0) {
            cursor = 0;
            B(at::kCands + 0xB) = cursor;
        } else if (top < 7) {
            cursor = static_cast<unsigned char>(cursor - top);
            B(at::kCands + 0xA) = 0;
            B(at::kCands + 0xB) = cursor;
        } else {
            cursor = static_cast<unsigned char>(cursor - 7);
            B(at::kCands + 0xB) = cursor;
            B(at::kCands + 0xA) = static_cast<unsigned char>(top - 7);
        }
    } else if (repeat & 8) {
        const unsigned char top = B(at::kCands + 0xA);
        if (top == 0x79) {
            cursor = 0x7F;
            B(at::kCands + 0xB) = cursor;
        } else if (top > 0x72) {
            B(at::kCands + 0xA) = 0x79;
            cursor = static_cast<unsigned char>(cursor + (0x79 - top));
            B(at::kCands + 0xB) = cursor;
        } else {
            cursor = static_cast<unsigned char>(cursor + 7);
            B(at::kCands + 0xB) = cursor;
            B(at::kCands + 0xA) = static_cast<unsigned char>(top + 7);
        }
    }
    if (was != cursor) BH_CALL(Sound_PlayEffect)(0x100);
    const unsigned char item = B(at::kCands + 0xD);
    const unsigned char category = B(at::kCands + 8);
    const unsigned help = BH_CALL(Item_HelpMessage)(category, item);
    const unsigned char* const text = BH_CALL(Msg_SystemPtr)(help);
    SetLong(At(at::kQueue + 4 + LastQueueEntry()), S32(static_cast<U>(reinterpret_cast<std::uintptr_t>(text))));
    bool back = false;
    if (Field_CancelButtons & Input_Pressed) {
        BH_CALL(Sound_PlayEffect)(0x106);
        back = true;
    } else {
        BH_CALL(BattleEquip_Preview)();
        if (Field_ConfirmButtons & Input_Pressed) {
            if (item != 0) {
                BH_CALL(Sound_PlayEffect)(0x103);
                BH_CALL(BattleEquip_Apply)();
                back = true;
            } else {
                BH_CALL(Sound_PlayEffect)(0x107);
            }
        }
    }
    if (!back) return;
    B(at::kSlots + 0xD) = 1;
    B(at::kCands + 0xB) = 0xFF;
    B(at::kSlots + 0xB) = 0xFF;
    B(at::kStep4) = static_cast<unsigned char>(B(at::kStep4) - 1);
}

// original 0x4491D0 (hidden, 0x64E4B0's entry 3; PSX 0x80096B3C, gap86): the
// six slots for use. The hand and the message as the change's slots; cancel
// (cue 0x106): the slots closed, record 4 on, the list at +3 = 3, the window
// above and the hand off, state 1, sub-state 0. Confirm with a word in record
// 18's +0x10 (cue 0x103): the member's +0x130 bit 0x4000, its +0x126 and the
// command's word +2 that word, the item's help line (Item_HelpMessage by the
// command's +3, +2) into the last queue entry, the slots closed, record 17's
// +3 = 2, the hand and the window above off, record 4 on, state 7; without
// one, cue 0x107. Then up / down move the slot either way (SlotCursor).
extern "C" void __cdecl BattleItemCmd_EquipUsePick(void) {
    SlotHand();
    const unsigned char* const text = OptionMessage(0x30);
    const unsigned entry = LastQueueEntry();
    const unsigned pressed = Input_Pressed;
    SetLong(At(at::kQueue + 4 + entry), S32(static_cast<U>(reinterpret_cast<std::uintptr_t>(text))));
    const unsigned repeat = BH_CALL(Input_AutoRepeat)(pressed & 0x5000);
    const unsigned pad = Input_Pressed;
    if (Field_CancelButtons & pad) {
        BH_CALL(Sound_PlayEffect)(0x106);
        B(at::kSlots + 3) = 1;
        B(at::kRecord4) = 1;
        B(at::kStep3) = 1;
        B(at::kList + 3) = 3;
        B(at::kAbove) = 0;
        B(at::kHand) = 0;
        B(at::kStep4) = 0;
        return;
    }
    if (Field_ConfirmButtons & pad) {
        if (Word(At(at::kSlots + 0x10)) != 0) {
            BH_CALL(Sound_PlayEffect)(0x103);
            unsigned char* const actor = MenuActor();
            unsigned char* const m = Party(actor[5]);
            SetLong(m + 0x130, S32(static_cast<U>(Long(m + 0x130)) | 0x4000));
            unsigned char* const m2 = Party(actor[5]);
            const unsigned word = Word(At(at::kSlots + 0x10));
            SetWord(m2 + 0x126, word);
            SetWord(Command() + 2, word);
            const unsigned char item = Command()[2];
            const unsigned char category = Command()[3];
            const unsigned help = BH_CALL(Item_HelpMessage)(category, item);
            const unsigned char* const line = BH_CALL(Msg_SystemPtr)(help);
            const unsigned last = LastQueueEntry();
            B(at::kSlots + 3) = 1;
            B(at::kCands + 3) = 2;
            B(at::kHand) = 0;
            B(at::kAbove) = 0;
            SetLong(At(at::kQueue + 4 + last), S32(static_cast<U>(reinterpret_cast<std::uintptr_t>(line))));
            B(at::kRecord4) = 1;
            B(at::kStep3) = 7;
        } else {
            BH_CALL(Sound_PlayEffect)(0x107);
        }
    }
    SlotCursor(repeat);
}

// original 0x4493E0 (hidden, BattleItemCmd_States entry 7; PSX 0x80096E48,
// gap86): state 7, the equipped item's target kind by its flag byte (0x591810
// of the member's +0x126 word) - BattleItemCmd_Choose's for the list item.
// Bit 0x40: state 8 or 9 ((f & 0x10 | 0x8F) >> 4) at sub-state 0, cue 0x103.
// Bit 0x80: every target (0xC0), state 8, sub-state 3. Else the side (bit
// 0x10: 0x40 / 0x80 by bit 0x20) or the member itself (its +5), state 8,
// sub-state 3.
extern "C" void __cdecl BattleItemCmd_EquipUseKind(void) {
    const unsigned word = MemberItem();
    const unsigned char f = ItemFlags(word >> 8, word);
    if (f & 0x40) {
        B(at::kStep4) = 0;
        B(at::kStep3) = static_cast<unsigned char>(((f & 0x10) | 0x8F) >> 4);
        BH_CALL(Sound_PlayEffect)(0x103);
        return;
    }
    if (f & 0x80) {
        Command()[0] = 0xC0;
        B(at::kStep3) = 8;
        B(at::kStep4) = 3;
        return;
    }
    const unsigned char target = (f & 0x10) ? SideTarget(f) : MenuActor()[5];
    Command()[0] = target;
    B(at::kStep3) = 8;
    B(at::kStep4) = 3;
}

// original 0x449480 (hidden, BattleItemCmd_States entry 8; PSX 0x80096F58):
// state 8 by the sub-state (the dword 0x904AA4's low byte) through 0x64E4C0 -
// EquipTargetBegin, EquipPickEnemy, EquipPickMember, EquipCommit,
// EquipCancel; a Fatal past five.
extern "C" void __cdecl BattleItemCmd_EquipTargetDispatch(void) {
    DispatchStep4("BattleItemCmd_EquipTargetDispatch", 0x64E4C0, 5);
}

// original 0x4494A0 (hidden, 0x64E4C0's entry 0; PSX Item_TargetSetup
// 0x80096F94, the sibling's hypothesis name, read the same): the equipped
// item's target pick begun, as BattleItemCmd_TargetBegin for the list item.
// Flag bit 0x20: Battle_DefaultTarget(3), sub-state 1. Else target 0 while
// Battle_ReturnTrue answers non-zero, else Battle_DefaultTarget(0); sub-state
// 2. Both: the latch zeroed, the pick flag set.
extern "C" void __cdecl BattleItemCmd_EquipTargetBegin(void) {
    const unsigned word = MemberItem();
    if (ItemFlags(word >> 8, word) & 0x20) {
        const unsigned char t = BH_CALL(Battle_DefaultTarget)(3);
        Command()[0] = t;
        B(at::kStep4) = 1;
        SetWord(At(at::kRepeatLatch), 0);
        B(at::kPicking) = 1;
        return;
    }
    if (BH_CALL(Battle_ReturnTrue)() != 0) {
        Command()[0] = 0;
    } else {
        const unsigned char t = BH_CALL(Battle_DefaultTarget)(0);
        Command()[0] = t;
    }
    B(at::kStep4) = 2;
    SetWord(At(at::kRepeatLatch), 0);
    B(at::kPicking) = 1;
}

// original 0x449540 (hidden, 0x64E4C0's entry 1; PSX 0x8009706C, gap86): an
// enemy, as BattleItemCmd_PickEnemy with the equipped item's flags. Cancel ->
// sub-state 4, confirm -> 3; up / down (0x5000) with flag bit 0x80 cross to
// the party (target 0 while Battle_ReturnTrue answers, else
// Battle_DefaultTarget(0)), sub-state + 1, cue 0x101; then right / left.
extern "C" void __cdecl BattleItemCmd_EquipPickEnemy(void) {
    unsigned char* const actor = MenuActor();
    const unsigned pad = Input_Pressed;
    const unsigned word = Word(Party(actor[5]) + 0x126);
    if (Field_CancelButtons & pad) {
        B(at::kStep4) = 4;
        return;
    }
    if (Field_ConfirmButtons & pad) {
        B(at::kStep4) = 3;
        return;
    }
    const unsigned repeat = BH_CALL(Input_AutoRepeat)(pad & 0xF000);
    if ((repeat & 0x5000) && (ItemFlags(word >> 8, word) & 0x80)) {
        if (BH_CALL(Battle_ReturnTrue)() != 0) {
            Command()[0] = 0;
        } else {
            const unsigned char t = BH_CALL(Battle_DefaultTarget)(0);
            Command()[0] = t;
        }
        B(at::kStep4) = static_cast<unsigned char>(B(at::kStep4) + 1);
        BH_CALL(Sound_PlayEffect)(0x101);
        return;
    }
    StepEnemyTarget(repeat);
}

// original 0x449680 (hidden, 0x64E4C0's entry 2; PSX 0x8009721C, gap86): a
// member, as BattleItemCmd_PickMember with the equipped item's flags. Cancel
// -> 4, confirm -> 3; up / down with flag bit 0x80 cross to the enemies
// (Battle_DefaultTarget(3), sub-state - 1, cue 0x101). Right: the target one
// on wrapped to 0 .. party count - 1, as it is while Battle_ReturnTrue
// answers, else through Battle_DefaultTarget; cue 0x101. Left: one back, as it
// is (cue 0x101, done) or through Battle_PrevTarget (cue 0x101).
extern "C" void __cdecl BattleItemCmd_EquipPickMember(void) {
    unsigned char* const actor = MenuActor();
    const unsigned pad = Input_Pressed;
    const unsigned word = Word(Party(actor[5]) + 0x126);
    if (Field_CancelButtons & pad) {
        B(at::kStep4) = 4;
        return;
    }
    if (Field_ConfirmButtons & pad) {
        B(at::kStep4) = 3;
        return;
    }
    const unsigned repeat = BH_CALL(Input_AutoRepeat)(pad & 0xF000);
    if ((repeat & 0x5000) && (ItemFlags(word >> 8, word) & 0x80)) {
        const unsigned char t = BH_CALL(Battle_DefaultTarget)(3);
        Command()[0] = t;
        B(at::kStep4) = static_cast<unsigned char>(B(at::kStep4) - 1);
        BH_CALL(Sound_PlayEffect)(0x101);
        return;
    }
    auto high = [] { return static_cast<long>(static_cast<U>(Long(At(at::kPartyCount))) & 0xFF) - 1; };
    if (repeat & 0x2000) {
        if (BH_CALL(Battle_ReturnTrue)() != 0) {
            const long h = high();
            const long value = static_cast<long>(static_cast<signed char>(Command()[0])) + 1;
            const long w = BH_CALL(Battle_WrapIndex)(h, 0, value);
            Command()[0] = static_cast<unsigned char>(w);
        } else {
            const long h = high();
            const long value = static_cast<long>(static_cast<signed char>(Command()[0])) + 1;
            const long w = BH_CALL(Battle_WrapIndex)(h, 0, value);
            const unsigned char t = BH_CALL(Battle_DefaultTarget)(static_cast<unsigned>(w));
            Command()[0] = t;
        }
        BH_CALL(Sound_PlayEffect)(0x101);
    }
    if (repeat & 0x8000) {
        if (BH_CALL(Battle_ReturnTrue)() != 0) {
            const long h = high();
            const long value = static_cast<long>(static_cast<signed char>(Command()[0])) - 1;
            const long w = BH_CALL(Battle_WrapIndex)(h, 0, value);
            Command()[0] = static_cast<unsigned char>(w);
            BH_CALL(Sound_PlayEffect)(0x101);
            return;
        }
        const long h = high();
        const long value = static_cast<long>(static_cast<signed char>(Command()[0])) - 1;
        const long w = BH_CALL(Battle_WrapIndex)(h, 0, value);
        const unsigned char t = BH_CALL(Battle_PrevTarget)(static_cast<unsigned>(w));
        Command()[0] = t;
        BH_CALL(Sound_PlayEffect)(0x101);
    }
}

// original 0x449830 (hidden, 0x64E4C0's entry 3 and 0x64E4D4's entry 2; PSX
// 0x80097430, gap86): the commit, as BattleItemCmd_Commit without spending an
// item: cue 0x104; the pick flag cleared; the command's +1 = 5; the member's
// +1 = 2; one more command chosen; the last queue entry's +1 = 1;
// ItemMenu_FreeWindows; 0x904AA2..4 = 0, 0x904AA1 = 1.
extern "C" void __cdecl BattleItemCmd_EquipCommit(void) {
    BH_CALL(Sound_PlayEffect)(0x104);
    unsigned char* const command = Command();
    B(at::kPicking) = 0;
    command[1] = 5;
    MenuActor()[1] = 2;
    B(at::kCommandsChosen) = static_cast<unsigned char>(B(at::kCommandsChosen) + 1);
    B(at::kQueue + 1 + LastQueueEntry()) = 1;
    BH_CALL(ItemMenu_FreeWindows)();
    B(at::kStep2) = 0;
    B(at::kStep3) = 0;
    B(at::kStep4) = 0;
    B(at::kStep) = 1;
}

// original 0x4498A0 (hidden, 0x64E4C0's entry 4 and 0x64E4D4's entry 3; PSX
// 0x800974E0, gap86): the pick cancelled, as 0x448B40 for the list item: cue
// 0x106, BattleBanner_ShowName(the member), the pick flag cleared, the list's
// +3 = 2 and word +4 = 0xFF38, 0x904AA1 = 4, 0x904AA2 = 2, 0x904AA3 = 1,
// 0x904AA4 = 0.
extern "C" void __cdecl BattleItemCmd_EquipCancel(void) {
    BH_CALL(Sound_PlayEffect)(0x106);
    BH_CALL(BattleBanner_ShowName)(MenuActor());
    B(at::kPicking) = 0;
    B(at::kList + 3) = 2;
    SetWord(At(at::kList + 4), 0xFF38);
    B(at::kStep) = 4;
    B(at::kStep2) = 2;
    B(at::kStep3) = 1;
    B(at::kStep4) = 0;
}

// original 0x4498F0 (hidden, BattleItemCmd_States entry 9; PSX 0x80097558):
// state 9 by the sub-state through 0x64E4D4 - EquipSideBegin, EquipSidePick,
// EquipCommit, EquipCancel; a Fatal past four.
extern "C" void __cdecl BattleItemCmd_EquipSideDispatch(void) {
    DispatchStep4("BattleItemCmd_EquipSideDispatch", 0x64E4D4, 4);
}

// original 0x449910 (hidden, 0x64E4D4's entry 0; PSX 0x80097594, gap86): a
// whole side for the equipped item, as BattleItemCmd_SideBegin.
extern "C" void __cdecl BattleItemCmd_EquipSideBegin(void) {
    const unsigned word = MemberItem();
    const unsigned char flags = ItemFlags(word >> 8, word);
    Command()[0] = SideTarget(flags);
    B(at::kStep4) = 1;
    SetWord(At(at::kRepeatLatch), 0);
    B(at::kPicking) = 1;
}

// original 0x449970 (hidden, 0x64E4D4's entry 1; PSX 0x8009762C, gap86): the
// side pick, as BattleItemCmd_SidePick: cancel -> 3, confirm -> 2, a direction
// with flag bit 0x80 flips the side, cue 0x101.
extern "C" void __cdecl BattleItemCmd_EquipSidePick(void) {
    unsigned char* const actor = MenuActor();
    const unsigned pad = Input_Pressed;
    const unsigned word = Word(Party(actor[5]) + 0x126);
    if (Field_CancelButtons & pad) {
        B(at::kStep4) = 3;
        return;
    }
    if (Field_ConfirmButtons & pad) {
        B(at::kStep4) = 2;
        return;
    }
    const unsigned repeat = BH_CALL(Input_AutoRepeat)(pad & 0xF000);
    if ((repeat & 0xF000) == 0) return;
    if (ItemFlags(word >> 8, word) & 0x80) {
        Command()[0] ^= 0xC0;
        BH_CALL(Sound_PlayEffect)(0x101);
    }
}

// original 0x449A00 (PSX 0x8009770C): the previewed equipment put on. For each
// of the six slots whose preview byte (0x675F18) is set and differs from the
// character's (+0x12 + slot): the new one taken from the inventory
// (Inventory_Remove(0x64E4E4's category, new, 1)), the old one given back
// (Inventory_Add(category, old, 1)), the slot set. Then Char_RecalcStats of the
// record and the members refreshed (RefreshMembers). The original pushes a
// fourth word, 0, to both inventory calls; neither reads it.
extern "C" void __cdecl BattleEquip_Apply(void) {
    unsigned char* const r = SlotsCharacter();
    for (unsigned i = 0; i < 6; ++i) {
        const unsigned char n = B(at::kPreview + i);
        if (n == 0 || n == r[0x12 + i]) continue;
        BH_CALL(Inventory_Remove)(B(at::kEquipCats + i), n, 1);
        const unsigned char old = r[0x12 + i];
        BH_CALL(Inventory_Add)(B(at::kEquipCats + i), old, 1);
        r[0x12 + i] = B(at::kPreview + i);
    }
    BH_CALL(Char_RecalcStats)(r);
    RefreshMembers();
}

// original 0x449BB0 (PSX 0x80097AB0): the candidate previewed. 0x8033F5 = 1
// when Item_EquipMask(record 17's category, item) has no bit for the
// character (1 << its index, an 8-bit shift: none from index 8 on); the six
// preview bytes 0x675F18 the character's +0x12..+0x17, the one at the slot
// cursor the candidate.
extern "C" void __cdecl BattleEquip_Preview(void) {
    const unsigned member = static_cast<U>(Long(At(at::kSlots + 0xC))) & 0xFF;
    const unsigned char item = B(at::kCands + 0xD);
    const unsigned char c = B(at::kCharOf + B(at::kMemberChar + member));
    const unsigned mask = BH_CALL(Item_EquipMask)(B(at::kCands + 8), item);
    const auto bit = static_cast<unsigned char>((1u << (c & 0x1F)) & 0xFF);
    B(at::kSlots + 0xD) = (bit & mask & 0xFF) == 0 ? 1 : 0;
    const unsigned char* const r = CharRecord(c);
    for (unsigned i = 0; i < 6; ++i) B(at::kPreview + i) = r[0x12 + i];
    B(at::kPreview + B(at::kSlots + 0xA)) = item;
}

// original 0x449C70 (PSX 0x80097BE0): the slot at the cursor taken off - not
// slot 0, and not an empty one (al 0). The item given back
// (Inventory_Add(0x64E4EC's category, item, 1); a fourth word 0 unread), the
// slot cleared at the cursor read again, Char_RecalcStats, the members
// refreshed; al 1. The original indexes six stack pointers by the cursor
// unchecked (past 5 it follows whatever the stack holds): ours aborts.
extern "C" unsigned char __cdecl BattleEquip_RemoveSlot(void) {
    unsigned char* const r = SlotsCharacter();
    auto slot = [](const char* when) {
        const unsigned char s = B(at::kSlots + 0xA);
        if (s > 5)
            bof3::Fatal("BattleEquip_RemoveSlot: the slot cursor 0x8033F2 is %u %s - the original indexes its six stack "
                        "pointers past the sixth (docs/battle_e4.md section 7)",
                        s, when);
        return s;
    };
    const unsigned char s = slot("at the start");
    const unsigned char item = r[0x12 + s];
    if (item == 0 || s == 0) return 0;
    BH_CALL(Inventory_Add)(B(at::kUnequipCats + s), item, 1);
    r[0x12 + slot("after Inventory_Add")] = 0;
    BH_CALL(Char_RecalcStats)(r);
    RefreshMembers();
    return 1;
}

// original 0x449E90 (PSX 0x80098004): the slots opened for a change - record
// 18 (+0 1, +1 8, +2 3, +3 2; cursor +0xA 0, chosen +0xB 0xFF, member +0xC the
// member's +5, +0xD 1; dword +0x20 0x675F18, words +4 0xFF56, +6 0x3F) and
// record 17 (+0 1, +3 1, +1 8, +2 4; +8..+0xD 0; words +4 / +6 the list's),
// the list off with its word +4 = 0x140, 0x929F06 the member's +5.
extern "C" void __cdecl BattleEquip_OpenChange(void) {
    unsigned char* const actor = MenuActor();
    B(at::kSlots) = 1;
    B(at::kSlots + 1) = 8;
    B(at::kSlots + 2) = 3;
    B(at::kSlots + 3) = 2;
    B(at::kSlots + 0xA) = 0;
    B(at::kSlots + 0xB) = 0xFF;
    const unsigned char member = actor[5];
    B(at::kSlots + 0xD) = 1;
    B(at::kCands) = 1;
    B(at::kCands + 3) = 1;
    const unsigned x = Word(At(at::kList + 4));
    B(at::kSlots + 0xC) = member;
    SetWord(At(at::kCands + 4), x);
    const unsigned y = Word(At(at::kList + 6));
    SetLong(At(at::kSlots + 0x20), static_cast<std::int32_t>(at::kPreview));
    SetWord(At(at::kSlots + 4), 0xFF56);
    SetWord(At(at::kSlots + 6), 0x3F);
    B(at::kCands + 1) = 8;
    B(at::kCands + 2) = 4;
    B(at::kCands + 0xC) = 0;
    B(at::kCands + 0xA) = 0;
    B(at::kCands + 0xB) = 0;
    B(at::kCands + 8) = 0;
    B(at::kCands + 0xD) = 0;
    B(at::kCands + 9) = 0;
    SetWord(At(at::kCands + 6), y);
    B(at::kList) = 0;
    SetWord(At(at::kList + 4), 0x140);
    B(at::kGeneMember) = actor[5];
}

// original 0x449F60 (PSX 0x80098128): the slots opened for use - record 18 as
// for a change but +3 3, and +0xD left 3; the list's +3 = 4; 0x929F06 the
// member's +5.
extern "C" void __cdecl BattleEquip_OpenUse(void) {
    unsigned char* const actor = MenuActor();
    B(at::kSlots) = 1;
    B(at::kSlots + 1) = 8;
    B(at::kSlots + 2) = 3;
    B(at::kSlots + 3) = 3;
    B(at::kSlots + 0xA) = 0;
    B(at::kSlots + 0xB) = 0xFF;
    const unsigned char member = actor[5];
    B(at::kSlots + 0xD) = 1;
    B(at::kSlots + 0xC) = member;
    SetLong(At(at::kSlots + 0x20), static_cast<std::int32_t>(at::kPreview));
    SetWord(At(at::kSlots + 4), 0xFF56);
    SetWord(At(at::kSlots + 6), 0x3F);
    B(at::kList + 3) = 4;
    B(at::kGeneMember) = actor[5];
    B(at::kSlots + 0xD) = 3;
}

// ===========================================================================
// The escape (0x44A000's 0x64E4FC by 0x904AA3: Escape_Roll, the failed path,
// the escaped path)
// ===========================================================================

// original 0x44A010 (hidden, 0x64E4FC's entry 0; PSX Escape_Roll 0x80098278,
// the sibling's name): the attempts 0x904AE6 counted. In an event battle
// (0x904AAA set) state + 1 - the failed path. From the third attempt on,
// state 2 - the escaped path. Else the enemies' mean u16 +0xB8 (actors 3..10
// not out, over 0x904AB3) against the party's mean +0xA8 (the members not out,
// over 0x904AB1): Escape_Chance of the difference; state 2 when Rand's low six
// bits are not above it, else 2 with 0x904AE4 == 1 and 1 without. As the
// original has it: the enemies' sum reads object actor (not actor - 3) - the
// objects 3..10, five of them past the eight - while Battle_ActorIsOut is asked
// of the actor; and both divisors are unchecked (ours aborts at 0).
extern "C" void __cdecl Escape_Roll(void) {
    const auto tries = static_cast<unsigned char>(B(at::kEscapeTries) + 1);
    const unsigned char fight = B(at::kFight);
    B(at::kEscapeTries) = tries;
    if (fight != 0) {
        B(at::kStep3) = static_cast<unsigned char>(B(at::kStep3) + 1);
        return;
    }
    if (B(at::kEscapeTries) >= 3) {
        B(at::kStep3) = 2;
        return;
    }
    U enemies = 0;
    for (unsigned a = 3; a <= 10; ++a) {
        const bool out = BH_CALL(Battle_ActorIsOut)(a) != 0;
        if (!out) enemies += Word(At(at::kEnemies + a * at::kEnemyStride + 0xB8));
    }
    const unsigned e_div = B(at::kEnemyDivisor);
    if (e_div == 0)
        bof3::Fatal("Escape_Roll: the enemies left 0x904AB3 is 0 - the original divides by it (docs/battle_e4.md section 7)");
    const U e_mean = enemies / e_div;
    U party = 0;
    if (B(at::kPartyCount) != 0) {
        for (unsigned m = 0;;) {
            if (BH_CALL(Battle_ActorIsOut)(m) == 0) party += Word(Party(m) + 0xA8);
            ++m;
            if (!(static_cast<int>(m) < static_cast<int>(static_cast<U>(Long(At(at::kPartyCount))) & 0xFF))) break;
        }
    }
    const unsigned p_div = B(at::kPartyDivisor);
    if (p_div == 0)
        bof3::Fatal("Escape_Roll: the party divisor 0x904AB1 is 0 - the original divides by it (docs/battle_e4.md section 7)");
    const U diff = party / p_div - e_mean;
    const unsigned chance = BH_CALL(Escape_Chance)(static_cast<int>(diff)) & 0xFF;
    const int roll = BH_CALL(Rand)() & 0x3F;
    if (roll > static_cast<int>(chance)) {
        B(at::kStep3) = static_cast<unsigned char>((B(at::kEscapeRun) == 1 ? 1 : 0) + 1);
        return;
    }
    B(at::kStep3) = 2;
}

// original 0x44A130 (hidden, 0x64E4FC's entry 1; PSX 0x80098414): state 1,
// the failed path, by the sub-state through 0x64E508 - Escape_Begin,
// Escape_StepBack, Escape_StepOn, Escape_Failed; a Fatal past four.
extern "C" void __cdecl Escape_FailDispatch(void) { DispatchStep4("Escape_FailDispatch", AddressOf(Escape_FailSteps), 4); }

// original 0x44A150 (hidden, 0x64E508's entry 0 and 0x64E518's entry 0; PSX
// Escape_Begin 0x80098450, the sibling's name): the banner of 0x669E08's text
// (BattleBanner_Add(1, 0, 0, 0x3C, it); 0x93B8E0 = 0 first); window 4's +3 =
// 1; MoveScript_F3Divisor 0x20; the field kind-2 x / z high words stepped by
// 0x64E4F4's pair (FieldStep); MoveScript_FAWord the elevation there less
// MapView_Elevation, >> 5, on the escaped path (state 2), else 0;
// LoadDatFile(0xD1); each of the three members with command 5 (+0x125) has
// its item given back (Battle_ReturnItem(+0x12E, +0x126)); then each of the
// count's members not out turns (+8 ^ 2) with +0x125 = 0, +1 = 2, +2..+4 = 0;
// sub-state + 1.
extern "C" void __cdecl Escape_Begin(void) {
    const auto text = reinterpret_cast<const char*>(static_cast<std::uintptr_t>(static_cast<U>(Long(At(at::kFleeText)))));
    B(at::kFlee) = 0;
    BH_CALL(BattleBanner_Add)(1, 0, 0, 0x3C, text);
    B(at::kRecord4 + 3) = 1;
    SetWord(At(at::kF3Divisor), 0x20);
    const unsigned char state = B(at::kStep3);
    FieldStep(1);
    if (state == 2) {
        const long e = BH_CALL(AreaMap_Elevation)(Long(At(at::kKind2X)), Long(At(at::kKind2Z)));
        const auto d = static_cast<short>(static_cast<unsigned short>(static_cast<U>(e) - Word(At(at::kElevation))));
        SetWord(At(at::kFaWord), static_cast<unsigned short>(static_cast<short>(d >> 5)));
    } else {
        SetWord(At(at::kFaWord), 0);
    }
    BH_CALL(LoadDatFile)(0xD1);
    for (unsigned k = 0; k < 3; ++k) {
        unsigned char* const p = Party(k);
        if (p[0x125] == 5) BH_CALL(Battle_ReturnItem)(p[0x12E], Word(p + 0x126));
    }
    if (B(at::kPartyCount) != 0) {
        for (unsigned char m = 0;;) {
            if (BH_CALL(Battle_ActorIsOut)(m) == 0) {
                unsigned char* const p = Party(m);
                p[8] ^= 2;
                p[0x125] = 0;
                p[1] = 2;
                p[2] = 0;
                p[3] = 0;
                p[4] = 0;
            }
            ++m;
            if (m >= B(at::kPartyCount)) break;
        }
    }
    B(at::kStep4) = static_cast<unsigned char>(B(at::kStep4) + 1);
}

// original 0x44A2E0 (hidden, 0x64E508's entry 1; PSX 0x800986C8): unless
// Field_Kind2Hold is set, MoveScript_F3Divisor 0x80, the step taken back
// (FieldStep(-1)), sub-state + 2.
extern "C" void __cdecl Escape_StepBack(void) {
    if (B(at::kKind2Hold) != 0) return;
    SetWord(At(at::kF3Divisor), 0x80);
    const unsigned char sub = B(at::kStep4);
    FieldStep(-1);
    B(at::kStep4) = static_cast<unsigned char>(sub + 2);
}

// original 0x44A330 (hidden, 0x64E508's entry 2; PSX 0x8009875C): unless
// Field_Kind2Hold is set, MoveScript_F3Divisor 0x40, the step taken again,
// sub-state + 1.
extern "C" void __cdecl Escape_StepOn(void) {
    if (B(at::kKind2Hold) != 0) return;
    SetWord(At(at::kF3Divisor), 0x40);
    const unsigned char sub = B(at::kStep4);
    FieldStep(1);
    B(at::kStep4) = static_cast<unsigned char>(sub + 1);
}

// original 0x44A380 (hidden, 0x64E508's entry 3; PSX Escape_Failed
// 0x800987F0, the sibling's hypothesis name, read the same): unless
// Field_Kind2Hold is set, Battle_BuildTurnOrder; each of the count's members
// not out turns back (+8 ^ 2), +1 = 2, its queued item returned
// (Battle_ReturnQueuedItem) and it leaves the turn order
// (Battle_RemoveFromTurnOrder); the message window, the banner of system
// message 0xF; the round flag 8; phase 3 with its four steps 0.
extern "C" void __cdecl Escape_Failed(void) {
    if (B(at::kKind2Hold) != 0) return;
    BH_CALL(Battle_BuildTurnOrder)();
    if (B(at::kPartyCount) != 0) {
        for (unsigned char m = 0;;) {
            if (BH_CALL(Battle_ActorIsOut)(m) == 0) {
                unsigned char* const p = Party(m);
                p[8] ^= 2;
                p[1] = 2;
                BH_CALL(Battle_ReturnQueuedItem)(m);
                BH_CALL(Battle_RemoveFromTurnOrder)(m);
            }
            ++m;
            if (m >= B(at::kPartyCount)) break;
        }
    }
    BH_CALL(Battle_OpenMsgWindow)();
    const auto* const line = reinterpret_cast<const char*>(BH_CALL(Msg_SystemPtr)(0xF));
    BH_CALL(BattleBanner_Add)(2, 0, 0, 0x2D, line);
    B(at::kFlags) |= 8;
    B(at::kPhase) = 3;
    B(at::kStep) = 0;
    B(at::kStep2) = 0;
    B(at::kStep3) = 0;
    B(at::kStep4) = 0;
}

// original 0x44A450 (hidden, 0x64E4FC's entry 2; PSX 0x8009892C, gap44):
// state 2, the escaped path, through 0x64E518 - Escape_Begin, Escape_Leave,
// Escape_End; a Fatal past three.
extern "C" void __cdecl Escape_WinDispatch(void) { DispatchStep4("Escape_WinDispatch", AddressOf(Escape_WinSteps), 3); }

// original 0x44A470 (hidden, 0x64E518's entry 1; PSX 0x80098968): unless
// Field_Kind2Hold is set, MapView_SetElevation of the elevation at the kind-2
// x / z, every enemy's tint released (Sprite_ReleaseTint on the eight
// objects), 0x494E70 (the eight enemies' +0..+3 zeroed), the message window,
// the banner of system message 0x10, sub-state + 1.
extern "C" void __cdecl Escape_Leave(void) {
    if (B(at::kKind2Hold) != 0) return;
    const long e = BH_CALL(AreaMap_Elevation)(Long(At(at::kKind2X)), Long(At(at::kKind2Z)));
    BH_CALL(MapView_SetElevation)(static_cast<int>(e));
    for (unsigned k = 0; k < 8; ++k) BH_CALL(Sprite_ReleaseTint)(At(at::kEnemies + k * at::kEnemyStride));
    BH_AT(VoidFn, at::kEnemiesClear)();
    BH_CALL(Battle_OpenMsgWindow)();
    const auto* const line = reinterpret_cast<const char*>(BH_CALL(Msg_SystemPtr)(0x10));
    BH_CALL(BattleBanner_Add)(2, 0, 0, 0x2D, line);
    B(at::kStep4) = static_cast<unsigned char>(B(at::kStep4) + 1);
}

// original 0x44A4F0 (hidden, 0x64E518's entry 2; PSX 0x80098A44): once
// 0x904AE9 is 0, MoveScript_F3Divisor 0 and the end phase at step 3 (phase 5,
// 0x904AA1 = 3, the other three steps 0).
extern "C" void __cdecl Escape_End(void) {
    if (B(at::kEndHold) != 0) return;
    SetWord(At(at::kF3Divisor), 0);
    B(at::kPhase) = 5;
    B(at::kStep) = 3;
    B(at::kStep2) = 0;
    B(at::kStep3) = 0;
    B(at::kStep4) = 0;
}

// original 0x44A520 (PSX Escape_Chance 0x80098A94, the sibling's name, read
// the same): the row index (diff + 0x20) / 16 (toward 0): below 0 gives 0x1C,
// above 5 0x38, else 0x64E524's byte; + 8 on the second attempt (0x904AE6 ==
// 2); + 8 once for the first member not out with +0x91 bit 0x20; capped at
// 0x40. al out.
extern "C" unsigned char __cdecl Escape_Chance(int diff) {
    const int index = S32(static_cast<U>(diff) + 0x20) / 16;   // the add wraps, as the original's
    unsigned char c = 0;
    if (index < 0) c = 0x1C;
    if (index > 5) c = 0x38;
    else if (c == 0) c = B(at::kChanceRow + static_cast<U>(index));
    if (B(at::kEscapeTries) == 2) c = static_cast<unsigned char>(c + 8);
    if (B(at::kPartyCount) != 0) {
        for (unsigned char m = 0;;) {
            if (BH_CALL(Battle_ActorIsOut)(m) == 0 && (Party(m)[0x91] & 0x20)) {
                c = static_cast<unsigned char>(c + 8);
                break;
            }
            ++m;
            if (m >= B(at::kPartyCount)) break;
        }
    }
    return c > 0x40 ? 0x40 : c;
}

// ===========================================================================
// Text
// ===========================================================================

// original 0x44A910 (PSX 0x801DE964): the member's name (its character record
// by 0x66972C of its +0x89) into Text_Records[0], 8 bytes (Str_CopyN).
extern "C" void __cdecl Battle_MemberNameToText(unsigned member) {
    const unsigned char c = B(at::kCharOf + Party(member)[0x89]);
    BH_CALL(Str_CopyN)(reinterpret_cast<char*>(At(at::kTextRecord0)), reinterpret_cast<const char*>(CharRecord(c)), 8);
}

// original 0x44A960 (PSX 0x801DE9D4, gap20): the enemy's 12-byte name (its
// working record's head) into Text_Records[0].
extern "C" void __cdecl Battle_EnemyNameToText(unsigned actor) {
    BH_CALL(Str_CopyN)(reinterpret_cast<char*>(At(at::kTextRecord0)),
                       reinterpret_cast<const char*>(At(at::kEnemyWork + ((actor & 0xFF) - 3u) * at::kEnemyStride)), 0xC);
}

// original 0x44AA90 (PSX 0x801DEB80): a banner line - system message
// 0x64E52C[row][column] (u16, ten to a row; neither bounded) through
// BattleBanner_Add(2, 0, 0, 0x2D, it).
extern "C" void __cdecl BattleBanner_AddLine(unsigned row, unsigned column) {
    const unsigned id = Word(At(at::kBannerLines + 2 * ((row & 0xFF) * 10 + (column & 0xFF))));
    const auto* const line = reinterpret_cast<const char*>(BH_CALL(Msg_SystemPtr)(id));
    BH_CALL(BattleBanner_Add)(2, 0, 0, 0x2D, line);
}

// ============================================================================

void BattleE4_Inject() {
    if (bof3::WantsShadow("battle_e4")) battle_e4::SelfTest();
    BOF3_INJECT(BattleWin_DimScreen);
    BOF3_INJECT(BattleWin_DrawTileTint);
    BOF3_INJECT(Battle_EnemyOutpaces);
    BOF3_INJECT(Battle_PrevTarget);
    BOF3_INJECT(Battle_HitOrMissParty);
    BOF3_INJECT(Battle_HitOrMissEnemy);
    BOF3_INJECT(Battle_PartyDefenceMean);
    BOF3_INJECT(Battle_SetHpChange);
    BOF3_INJECT(Battle_ReloadPartyRecords);
    BOF3_INJECT(Battle_OrderPushFront);
    BOF3_INJECT(AutoBattle_FillCommands);
    BOF3_INJECT(Battle_TurnVectorC);
    BOF3_INJECT(Battle_TurnVector18);
    BOF3_INJECT(Battle_MemberReactRoll);
    BOF3_INJECT(Battle_WriteBackMember);
    BOF3_INJECT(Battle_PickEnemyTarget);
    BOF3_INJECT(Battle_WakeRoll);
    BOF3_INJECT(Battle_ReturnItem);
    BOF3_INJECT(BattleEnd_EnterStep1);
    BOF3_INJECT(BattleEnd_EnterStep2);
    BOF3_INJECT(BattleEnd_EnterStep3);
    BOF3_INJECT(ItemMenu_SetupForMember);
    BOF3_INJECT(BattleItemCmd_SideBegin);
    BOF3_INJECT(BattleItemCmd_SidePick);
    BOF3_INJECT(BattleItemCmd_EquipMenu);
    BOF3_INJECT(BattleItemCmd_EquipSlotPick);
    BOF3_INJECT(BattleItemCmd_EquipItemPick);
    BOF3_INJECT(BattleItemCmd_EquipUsePick);
    BOF3_INJECT(BattleItemCmd_EquipUseKind);
    BOF3_INJECT(BattleItemCmd_EquipTargetDispatch);
    BOF3_INJECT(BattleItemCmd_EquipTargetBegin);
    BOF3_INJECT(BattleItemCmd_EquipPickEnemy);
    BOF3_INJECT(BattleItemCmd_EquipPickMember);
    BOF3_INJECT(BattleItemCmd_EquipCommit);
    BOF3_INJECT(BattleItemCmd_EquipCancel);
    BOF3_INJECT(BattleItemCmd_EquipSideDispatch);
    BOF3_INJECT(BattleItemCmd_EquipSideBegin);
    BOF3_INJECT(BattleItemCmd_EquipSidePick);
    BOF3_INJECT(BattleEquip_Apply);
    BOF3_INJECT(BattleEquip_Preview);
    BOF3_INJECT(BattleEquip_RemoveSlot);
    BOF3_INJECT(BattleEquip_OpenChange);
    BOF3_INJECT(BattleEquip_OpenUse);
    BOF3_INJECT(Escape_Roll);
    BOF3_INJECT(Escape_FailDispatch);
    BOF3_INJECT(Escape_Begin);
    BOF3_INJECT(Escape_StepBack);
    BOF3_INJECT(Escape_StepOn);
    BOF3_INJECT(Escape_Failed);
    BOF3_INJECT(Escape_WinDispatch);
    BOF3_INJECT(Escape_Leave);
    BOF3_INJECT(Escape_End);
    BOF3_INJECT(Escape_Chance);
    BOF3_INJECT(Battle_MemberNameToText);
    BOF3_INJECT(Battle_EnemyNameToText);
    BOF3_INJECT(BattleBanner_AddLine);
}

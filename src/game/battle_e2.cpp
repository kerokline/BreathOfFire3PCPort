// Round twelve group BE2 (docs/takeover-queue-field-battle.md section 3; the
// 48 rows BE2 of analysis/round12_cut.tsv): the battle engine's
// 0x433650..0x437030, each read to its last instruction with capstone
// (2026-09-29, tools/band_rows.py --group BE2 for the extents and call rows)
// and taken through the boss harness as an engine group (boss_harness.h,
// docs/boss_harness.md section 10). docs/battle_e2.md has every function one
// row each.
//
//   - the effect tasks: the actor watch's state 3 (a status icon over the
//     watched actor, cycled every 0x3C frames) with its two helpers; the
//     party put back from the backup records 0x939AE0 after Accession's
//     single-member form (BattleFx_Dispatch slot 12's two states and slot
//     17's task, one per other member); four markers and cursors drawn over
//     window records 18, 19 and 21 (slots 9, 10, 13, 14);
//   - the enemy's action pick (Battle_BeginAction's call for an enemy, the
//     transformation route's path) and its target helpers;
//   - the enemy ops of EnemyOp_Steps 4 and 5 (the turn's start, the creature
//     cue - PSX twin Battle_PlayCreatureCue), of EnemyOp_EnterSubs 1 (a slide
//     in) and of EnemyOp_ActSubs 3 and 5 (a knock-back, the HP change).
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. Where
// the original divides by a count or a maximum that can be 0, indexes past a
// table (the enemies by a byte above 7, the party by a byte above 2, a task
// slot BattleTask_Create answered 0xFF for, a dispatcher's state byte) or
// spins without end (BattleFx_NextStatusIcon with no status bit and +0xB of
// 16 or more), ours aborts with a message (the owner's rule for an
// unchecked index, round9 doc section 6); docs/battle_e2.md section 7 lists
// each. Every call goes through the harness (BH_CALL / BH_AT / Phase), so the
// start-up fuzz can stand recorders in for the callees.
#include "game/battle_e2.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/battle_e2_callees.h"
#include "game/boss_harness.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = battle_e2::at;
using U = std::uint32_t;
// A named .data table's address (symbols.gen.h binds the name to a typed pointer).
template <typename T> U AddressOf(T* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
using boss_harness::Phase;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

unsigned char& B(U address) { return At(address)[0]; }
U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }
void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }
U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
unsigned char* PtrIn(const unsigned char* p) { return At(UL(p)); }
unsigned char* PtrAt(U cell) { return At(UL(At(cell))); }
unsigned char* Sc() { return Sprite_Current; }
unsigned char* Owner() { return PtrAt(at::kOwner); }
unsigned char* Enemy() { return PtrAt(at::kEnemyCurrent); }
// An enemy object and a party member by an actor's index, as the originals
// address them (32-bit wrap: an index below its base reads below the array).
unsigned char* EnemyObj(U index) { return At(at::kEnemies + index * at::kEnemyStride); }
unsigned char* Member(U index) { return At(at::kParty + index * at::kPartyStride); }
unsigned char* Task(U slot) { return At(at::kTasks + slot * at::kTaskStride); }

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
// An enemy by its index 0..7 (an argument's low byte): past 7 the original
// reads and writes past the eight objects (and past .data above 25).
unsigned char* EnemyArg(const char* who, U word) {
    const unsigned index = word & 0xFF;
    if (index > 7)
        bof3::Fatal("%s: enemy %u, past the eight enemy objects - the original indexes past them (docs/battle_e2.md "
                    "section 7)",
                    who, index);
    return EnemyObj(index);
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

// jmp [table + 4 * Sprite_Current[at]] over a .data table of `n`: the entry as
// read (the fuzz swaps the cells for recorders), the caller's word handed on
// and the entry's eax answered (the jmp leaves both in place); a Fatal past
// the table (the original jumps through the next table's cells).
using Entry = U (__cdecl*)(U);
U Dispatch(const char* who, U table, unsigned n, unsigned at_, U word) {
    const unsigned state = Sprite_Current[at_];
    if (state >= n)
        bof3::Fatal("%s: state byte +%u is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/battle_e2.md section 7)",
                    who, at_, state, n, (unsigned)table);
    return reinterpret_cast<Entry>(static_cast<std::uintptr_t>(UL(At(table + 4 * state))))(word);
}

// The animation set pointer 0x9039D8 at the battle's pose pool around a task's
// state call, the field's after (as BattleFx_PoseTask and the actor watch).
void WithBattlePoses(const char* who, const U* entries, unsigned n) {
    unsigned char* const s = Sprite_Current;   // read before the store, as the original
    SetUL(At(at::kAnimSet), at::kAnimSetBattle);
    CallState(who, entries, n, s[1]);
    SetUL(At(at::kAnimSet), at::kAnimSetField);
}

// The ten sprite fields the four markers' start states set, Sprite_Current
// read afresh for each: +0x29 1, +0x25 0x1E, +0x26 0, +0x24 0x80, +0x27 0x1B,
// +0x28 1, u16 +0x2C 2, +0x2B 0, +0x2A 0, u32 +0x3C 0.
void MarkerLook() {
    Sc()[0x29] = 1;
    Sc()[0x25] = 0x1E;
    Sc()[0x26] = 0;
    Sc()[0x24] = 0x80;
    Sc()[0x27] = 0x1B;
    Sc()[0x28] = 1;
    SetWord(Sc() + 0x2C, 2);
    Sc()[0x2B] = 0;
    Sc()[0x2A] = 0;
    SetUL(Sc() + 0x3C, 0);
}

// The grid cell +0xB of window record 21's six columns: x = the record's +4
// dword + 30 (b % 6) + 0x13, y = 32 (b / 6) + its +6 + 0x30 (words stored).
void GridPlace() {
    unsigned char* s = Sprite_Current;
    unsigned b = s[0xB];
    SetWord(s + 0x2E, UL(At(at::kWindow21 + 4)) + (b % 6) * 30 + 0x13);
    s = Sprite_Current;
    b = s[0xB];
    SetWord(s + 0x30, (b / 6) * 32 + Word(At(at::kWindow21 + 6)) + 0x30);
}

// The animation byte of a cursor's grid: the 18 dwords of `grid` as bytes,
// row (+9 plus the record's s16 +0x12) times 4 plus the column +0xA.
unsigned char GridByte(U grid, U window, const unsigned char* s) {
    const std::int32_t row = static_cast<std::int32_t>(s[9]) + static_cast<std::int16_t>(Word(At(window + 0x12)));
    return At(grid + static_cast<U>(row) * 4 + s[0xA])[0];
}

}  // namespace

// ===========================================================================
// The actor watch's state 3: the status icon (BattleFx_ActorWatch slot 3)
// ===========================================================================

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
    const unsigned status = actor <= 2 ? Word(Member(actor) + 0x90) : Word(EnemyObj(actor - 3) + 0x92);
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
        const unsigned row = Member(actor)[0x89];
        const auto dx = static_cast<std::int8_t>(At(at::kMemberOffsets + (o[8] + row * 4) * 2)[0]);
        SetWord(Sc() + 0x2E, static_cast<unsigned>(dx + Word(o + 0x2E)));
        o = Owner();
        const unsigned row2 = Member(o[5])[0x89];
        const unsigned dy = o[8] <= 1 ? At(at::kMemberRowY + row2 * 2)[0] : At(at::kMemberRowY + 1 + row2 * 2)[0];
        SetWord(Sc() + 0x30, Word(o + 0x30) - dy - 8);
    } else {
        const auto dx = static_cast<std::int8_t>(EnemyObj(actor - 3)[0xF2]);
        SetWord(Sc() + 0x2E, static_cast<unsigned>(dx + Word(o + 0x2E)));
        o = Owner();
        const unsigned kind = EnemyObj(static_cast<U>(o[5]) - 3)[0xF0];
        const unsigned dy = At(at::kEnemyDataY + kind * 0x8C)[0];
        SetWord(Sc() + 0x30, Word(o + 0x30) - dy - 8);
    }
    SetWord(Sc() + 0x32, Word(Owner() + 0x32));
}

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
    if ((UL(At(at::kRoundFlags)) & 0x400) != 0) {
        if (Owner()[5] == B(at::kActor)) {
            ++Sprite_Current[1];
            return;
        }
        if (B(at::kActKind) == 4) {
            const unsigned id = Word(At(at::kMagicId));
            if (id == 0x97 || id == 0xE1) {
                ++Sprite_Current[1];
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
        Sc()[0x27] = 0xFF;
        Sc()[0xA] = 0x3C;
        if (Sc()[0xB] != icon) {
            BH_CALL(Sprite_SetAnimation)(At(at::kStatusIcons + icon)[0]);
            Sc()[0xB] = icon;
        }
    }
    const unsigned actor = Owner()[5];
    const bool shown = actor <= 2 ? (Member(actor)[0x90] & 0x58) != 0 : (EnemyObj(actor - 3)[0x92] & 0x58) != 0;
    if (!shown) Sc()[1] = 0;
    BH_CALL(BattleFx_PlaceOverOwner)();
    BH_CALL(Sprite_ScriptTick)();
    BH_CALL(Sprite_QueueOverlay)();
    if (BH_CALL(Battle_ActorIsOut)(Owner()[5]) != 0 && (Owner()[0] & 1) == 0) Sc()[1] = 0;
}

// ===========================================================================
// The party put back from its backup records (slots 12 and 17)
// ===========================================================================

// original 0x433D60 (no PSX twin paired): 1 when, of the backup records
// 0x939AE0 (the party's stride) from the first on, one in use (+0 bit 0)
// has +0x134 bit 0 before a record not in use is met; else 0.
extern "C" unsigned char __cdecl Battle_BackupFlagged(void) {
    for (unsigned i = 0; i < 3; ++i) {
        const unsigned char* const r = At(at::kBackup + i * at::kPartyStride);
        if ((r[0] & 1) == 0) return 0;
        if ((At(at::kBackupFlags + i * at::kPartyStride)[0] & 1) != 0) return 1;
    }
    return 0;
}

namespace {

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
    auto rec = [&]() { return Rec(who, o[5]); };
    const unsigned char kept8 = rec()[8];
    {
        unsigned char* const to = rec();
        const unsigned char* const from = At(at::kBackup + static_cast<U>(o[5]) * at::kPartyStride);
        for (unsigned k = 0; k < at::kPartyStride; k += 4) SetLong(to + k, Long(from + k));   // rep movsd, forward
    }
    rec()[1] = 6;
    rec()[2] = 4;
    rec()[3] = 4;
    rec()[4] = 1;
    if (keep8) rec()[8] = kept8;
    rec()[0x48] = 2;
    SetUL(rec() + 0x40, 0);
    SetUL(rec() + 0x130, UL(rec() + 0x130) | 0x2000);
    SetUL(Task(o[5]) + 0x34, UL(At(at::kMemberPositions + static_cast<U>(o[5]) * 8)));
    SetUL(Task(o[5]) + 0x38, UL(At(at::kMemberPositions + 4 + static_cast<U>(o[5]) * 8)));
    {
        unsigned char* const t = Task(o[5]);
        const long ground = BH_CALL(AreaMap_Elevation)(Long(t + 0x34), Long(t + 0x38));
        SetUL(Task(o[5]) + 0x3C, static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(ground))) << 16);
    }
    const unsigned char character = rec()[0x89];
    if (character == 7 || character == 0) {
        SetWord(rec() + 0x9A, Word(At(at::kKeptAp)));
        SetWord(rec() + 0x90, Word(At(at::kKeptStatus)));
        {
            unsigned char* const r = rec();
            r[0x9E] = At(at::kFieldActors + static_cast<U>(r[0x148]) * 0xA4)[0];
        }
        SetUL(rec() + 0x130, UL(At(at::kKeptFlags)));
        SetUL(rec() + 0x134, UL(At(at::kKeptFlags2)));
        for (unsigned k = 1; k <= 8; ++k) rec()[0x137 + k] = 0;
        BH_AT(void (__cdecl*)(), at::kMemberRecalc)();
    }
    BH_AT(void (__cdecl*)(unsigned), at::kMemberStatusSet)(Sprite_Current[5]);
    unsigned char* const s = Sprite_Current;
    {
        unsigned i = s[5];
        SetWord(At(at::kPartyValues + i * 0x24), Word(Rec(who, i) + 0x98));
        i = s[5];
        SetWord(At(at::kPartyValues + 2 + i * 0x24), Word(Rec(who, i) + 0x9A));
        i = s[5];
        const unsigned hp = Word(Rec(who, i) + 0x98), max_hp = Word(Rec(who, i) + 0xA0);
        if (max_hp == 0) bof3::Fatal("%s: member %u's max HP is 0 - the original divides by it (docs/battle_e2.md section 7)", who, i);
        At(at::kPartyGauges + i * 0x24)[0] = static_cast<unsigned char>(static_cast<std::int32_t>(55 * hp) / static_cast<std::int32_t>(max_hp));
        i = s[5];
        const unsigned ap = Word(Rec(who, i) + 0x9A), max_ap = Word(Rec(who, i) + 0xA2);
        if (max_ap == 0) bof3::Fatal("%s: member %u's max AP is 0 - the original divides by it (docs/battle_e2.md section 7)", who, i);
        At(at::kPartyGauges + 1 + i * 0x24)[0] = static_cast<unsigned char>(static_cast<std::int32_t>(55 * ap) / static_cast<std::int32_t>(max_ap));
    }
    BH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>(s[8] + 4));
    BH_CALL(Sprite_LoadPalette)(reinterpret_cast<unsigned short*>(At(at::kPalettes + (static_cast<U>(Sprite_Current[5]) << 6))), 0);
    BH_CALL(Battle_StatusTint)(Word(Rec(who, Sprite_Current[5]) + 0x90));
    BH_CALL(Sprite_SetClutStp)();
}

}  // namespace

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
    if ((UL(At(at::kRoundFlags)) & 0x8000) != 0) {
        const unsigned char n = B(at::kSavedCount);
        B(at::kPartyCount) = n;
        B(at::kPartyIn) = 0;
        if (n > 0) {
            unsigned char in = 0;
            for (unsigned k = 0; k < n; ++k)
                if ((At(at::kBackupBit40 + k * at::kPartyStride)[0] & 0x40) == 0) ++in;
            B(at::kPartyIn) = in;
        }
        B(at::kWindow1Count) = n;
        SetWord(At(at::kWindow1X), At(at::kCountX + n)[0]);
        if (n > 1) {
            unsigned char m = 1;
            do {
                unsigned char* const member = Member(m);
                const unsigned slot = BH_CALL(BattleTask_Create)(0, 0x11);
                SetUL(NewTask(kWho, slot) + 0x80, Key(member));
                ++m;
            } while (m < B(at::kPartyCount));
        }
    }
    unsigned char* const saved = Sprite_Current;
    unsigned char* const o = Owner();
    Sprite_Current = o;
    Field_State = Rec(kWho, o[5]);
    const unsigned char character = Rec(kWho, o[5])[0x89];
    if (character == 7 || character == 0) {
        const unsigned char* const r = Rec(kWho, o[5]);
        SetWord(At(at::kKeptAp), Word(r + 0x9A));
        SetWord(At(at::kKeptStatus), Word(r + 0x90) & 0xBFFF);
        SetUL(At(at::kKeptFlags), UL(r + 0x130) & 0xFFFE7FFFu);
        SetUL(At(at::kKeptFlags2), UL(r + 0x134) & 0xFFF884FDu);
    }
    RestoreBody(kWho, o, true);
    if ((UL(At(at::kRoundFlags)) & 0x8000) != 0) {
        const unsigned char a = B(at::kPlace94), b = B(at::kPlace95);
        B(at::kPlaceCells) = a;
        B(at::kPlaceCells + 1) = b;
    } else {
        unsigned char* const s = Sprite_Current;
        B(at::kPlaceCells + static_cast<U>(s[5]) * 3) = B(at::kPlace94);
        B(at::kPlaceCells + 1 + static_cast<U>(s[5]) * 3) = B(at::kPlace95);
    }
    SetWord(At(at::kRoundFlags), Word(At(at::kRoundFlags)) & 0x7FFF);
    Sprite_Current = saved;
    ++saved[1];
}

// original 0x434340 (no PSX twin paired; hidden in 0x433D60's extent): slot
// 17's state 0, a task per other member: its owner's u32 +0x40 = 0, then on
// the owner as Sprite_Current (Field_State its member) the common restore
// (RestoreBody, +8 not kept); the owner's +0 bit 0x40 cleared,
// Sprite_Current back and its state + 1.
extern "C" void __cdecl BattleFx_RestoreMember(void) {
    static const char* const kWho = "BattleFx_RestoreMember";
    unsigned char* const o = Owner();
    unsigned char* const saved = Sprite_Current;
    Sprite_Current = o;
    Field_State = Rec(kWho, o[5]);
    SetUL(o + 0x40, 0);
    RestoreBody(kWho, Sprite_Current, false);
    Sprite_Current[0] = static_cast<unsigned char>(Sprite_Current[0] & 0xBF);
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
    const U v = UL(o + 0x40);
    if (v != 0x10000) {
        SetUL(o + 0x40, v + 0x2000);
        return;
    }
    o[0x48] = 0;
    unsigned char* const o2 = Owner();
    SetUL(Rec("BattleFx_RestoreFade", o2[5]) + 0x130, UL(Rec("BattleFx_RestoreFade", o2[5]) + 0x130) & ~0x2000u);
    SetUL(Rec("BattleFx_RestoreFade", o2[5]) + 0x134, UL(Rec("BattleFx_RestoreFade", o2[5]) + 0x134) & 0xFFFCFFF9u);
    BH_CALL(BattleTask_FreeCurrent)();
}

// original 0x434310 (no PSX twin paired; hidden in 0x433D60's extent):
// BattleFx_Dispatch slot 17: by +1 through {BattleFx_RestoreMember,
// BattleFx_RestoreFade} (a stack table; ours aborts past 1).
extern "C" void __cdecl BattleFx_RestoreMemberTask(void) {
    static constexpr U kSteps[2] = {bof3::addr::BattleFx_RestoreMember, bof3::addr::BattleFx_RestoreFade};
    CallState("BattleFx_RestoreMemberTask", kSteps, 2, Sprite_Current[1]);
}

// ===========================================================================
// The markers over window records 21, 18 and 19 (slots 9, 10, 13, 14)
// ===========================================================================

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
    ++Sprite_Current[1];
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
    if ((B(at::kWindow21) & 1) == 0) {
        Sc()[1] = 2;
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
    if (B(at::kPickCount) == 0) return;
    unsigned char* s = Sprite_Current;
    do {
        if (At(at::kPickList + i)[0] == s[0xB]) {
            s[2] = static_cast<unsigned char>(s[2] + 1);
            s = Sprite_Current;
        }
        ++i;
    } while (i < B(at::kPickCount));
}

// original 0x434B10 (hidden in 0x434870's extent): the first of the 0x904B87
// bytes of 0x904B84 equal to +0xB (both read once): x = record 21's +4 + 24 i
// + 0x41, y = its +6 + 0x12; none: +2 down by one.
extern "C" void __cdecl BattleFx_GridMarkPlace(void) {
    const unsigned char n = B(at::kPickCount);
    unsigned char* const s = Sprite_Current;
    if (n > 0) {
        const unsigned char b = s[0xB];
        unsigned char i = 0;
        do {
            if (At(at::kPickList + i)[0] == b) {
                SetWord(s + 0x2E, UL(At(at::kWindow21 + 4)) + i * 24u + 0x41);
                SetWord(Sprite_Current + 0x30, Word(At(at::kWindow21 + 6)) + 0x12);
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
    SetWord(Sc() + 0x2E, UL(At(at::kWindow21 + 4)) + 0x14);
    SetWord(Sc() + 0x30, Word(At(at::kWindow21 + 6)) + 0x12);
    ++Sprite_Current[1];
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
    if ((s[0] & 1) == 0) return;
    if ((B(at::kWindow21) & 1) == 0) {
        s[1] = 2;
        Sprite_Current[2] = 0;
        return;
    }
    SetWord(s + 0x2E, UL(At(at::kWindow21 + 4)) + 0x14);
    SetWord(Sprite_Current + 0x30, Word(At(at::kWindow21 + 6)) + 0x12);
    BH_CALL(Sprite_QueueOverlay)();
}

// original 0x434D00 (hidden in 0x434870's extent): animation 0x12; with no
// command tapped (0x904AA6 0xFF) animation 0x13 and +2 on.
extern "C" void __cdecl BattleFx_ListHandPress(void) {
    BH_CALL(Sprite_SetAnimation)(0x12);
    if (B(at::kTapCommand) != 0xFF) return;
    BH_CALL(Sprite_SetAnimation)(0x13);
    ++Sprite_Current[2];
}

// original 0x434D30 (hidden in 0x434870's extent): +2 back by one once a
// command is tapped (0x904AA6 not 0xFF); record 21 not in use: +1 on, +2 = 0.
extern "C" void __cdecl BattleFx_ListHandWait(void) {
    if (B(at::kTapCommand) != 0xFF) Sprite_Current[2] = static_cast<unsigned char>(Sprite_Current[2] - 1);
    if ((B(at::kWindow21) & 1) != 0) return;
    ++Sprite_Current[1];
    Sprite_Current[2] = 0;
}

namespace {

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
    SetWord(At(window + 0x1E), Word(At(window + 0x12)));
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// Run: the record not in use, +1 = 2 and +2 = 0; its row base moved (+0x1E
// not +0x12), +1 = 0; else x = its +4 + 22 (+0xA) + 0x44, y = 32 (+9) + its +6
// + 0x22, and unless the grid's byte is 0xFF, Sprite_EnsureAnimation of it,
// the script ticked and the overlay queued.
void CursorRun(U window, U grid) {
    if ((B(window) & 1) == 0) {
        Sc()[1] = 2;
        Sc()[2] = 0;
        return;
    }
    if (Word(At(window + 0x1E)) != Word(At(window + 0x12))) {
        Sc()[1] = 0;
        return;
    }
    unsigned char* s = Sprite_Current;
    SetWord(s + 0x2E, UL(At(window + 4)) + s[0xA] * 22u + 0x44);
    s = Sprite_Current;
    SetWord(s + 0x30, s[9] * 32u + Word(At(window + 6)) + 0x22);
    s = Sprite_Current;
    const unsigned char a = GridByte(grid, window, s);
    if (a == 0xFF) return;
    BH_CALL(Sprite_EnsureAnimation)(a);
    BH_CALL(Sprite_ScriptTick)();
    BH_CALL(Sprite_QueueOverlay)();
}

}  // namespace

// original 0x434D70 (hidden in 0x434870's extent): BattleFx_Dispatch slot 13,
// with the battle's pose pool: by +1 through {_Win18CursorStart,
// _Win18CursorRun, BattleFx_FreeTask} (ours aborts past 2).
extern "C" void __cdecl BattleFx_Win18Cursor(void) {
    static constexpr U kSteps[3] = {bof3::addr::BattleFx_Win18CursorStart, bof3::addr::BattleFx_Win18CursorRun, bof3::addr::BattleFx_FreeTask};
    WithBattlePoses("BattleFx_Win18Cursor", kSteps, 3);
}

// original 0x434DC0 (hidden in 0x434870's extent): CursorStart over record
// 18 (0x8033E8) and the grid 0x904608.
extern "C" void __cdecl BattleFx_Win18CursorStart(void) { CursorStart(at::kWindow18, at::kGrid18); }

// original 0x434E80 (hidden in 0x434870's extent): CursorRun over record 18
// and the grid 0x904608.
extern "C" void __cdecl BattleFx_Win18CursorRun(void) { CursorRun(at::kWindow18, at::kGrid18); }

// original 0x434F40 (hidden in 0x434870's extent): BattleFx_Dispatch slot 14,
// with the battle's pose pool: by +1 through {_Win19CursorStart,
// _Win19CursorRun, BattleFx_FreeTask} (ours aborts past 2).
extern "C" void __cdecl BattleFx_Win19Cursor(void) {
    static constexpr U kSteps[3] = {bof3::addr::BattleFx_Win19CursorStart, bof3::addr::BattleFx_Win19CursorRun, bof3::addr::BattleFx_FreeTask};
    WithBattlePoses("BattleFx_Win19Cursor", kSteps, 3);
}

// original 0x434F90 (hidden in 0x434870's extent): CursorStart over record
// 19 (0x80340C) and the grid 0x904620.
extern "C" void __cdecl BattleFx_Win19CursorStart(void) { CursorStart(at::kWindow19, at::kGrid19); }

// original 0x435050 (hidden in 0x434870's extent): CursorRun over record 19
// and the grid 0x904620.
extern "C" void __cdecl BattleFx_Win19CursorRun(void) { CursorRun(at::kWindow19, at::kGrid19); }

// ===========================================================================
// The enemy's action and its target (the action's begin)
// ===========================================================================

// original 0x435C80 (no PSX twin paired): a random enemy actor: of 3..10,
// those Battle_ActorIsOut rules in and that are not `exclude` (its low
// byte), the one at Rand() % their count.
//
// As the original has it: the count is not tested (0 divides by zero; ours
// aborts); Rand's answer is taken signed (the CRT's is 0..0x7FFF).
extern "C" unsigned char __cdecl Battle_RandomEnemy(unsigned exclude) {
    unsigned char list[8] = {};
    unsigned char count = 0;
    for (unsigned char a = 3; a <= 10; ++a)
        if (BH_CALL(Battle_ActorIsOut)(a) == 0 && (exclude & 0xFF) != a) list[count++] = a;
    const int r = BH_CALL(Rand)();
    if (count == 0) bof3::Fatal("Battle_RandomEnemy: no enemy left to pick - the original divides by 0 (docs/battle_e2.md section 7)");
    const int k = r % static_cast<int>(count);
    if (k < 0) bof3::Fatal("Battle_RandomEnemy: Rand answered %d - the original reads its frame below the list", r);
    return list[k];
}

// original 0x435EF0 (PSX 0x801E2A0C's callee; no twin paired): a random party
// member by the formation's weights: with 3 members (0x904AB0's low byte),
// those not out weigh the bytes 0x64B18C[formation x 3 + member], with 2
// 0x64B184[formation x 2 + member] (the formation 0x904060's low byte, read
// afresh); Rand() % the sum (a byte) picks the member whose running sum first
// passes it; none: the count (3 or 2). Another count: 0.
//
// As the original has it: the sum is a byte and is not tested (0 divides by
// zero; ours aborts); each Battle_ActorIsOut is asked again on the second
// pass.
extern "C" unsigned char __cdecl Battle_RandomMember(void) {
    const unsigned n = UL(At(at::kPartyCount)) & 0xFF;
    if (n != 2 && n != 3) return 0;
    const U weights = n == 3 ? at::kFormationWeights3 : at::kFormationWeights2;
    unsigned char sum = 0;
    for (unsigned char m = 0; m < n; ++m)
        if (BH_CALL(Battle_ActorIsOut)(m) == 0) sum = static_cast<unsigned char>(sum + At(weights + (UL(At(at::kFormation)) & 0xFF) * n + m)[0]);
    const int r = BH_CALL(Rand)();
    if (sum == 0) bof3::Fatal("Battle_RandomMember: the weights sum to 0 - the original divides by 0 (docs/battle_e2.md section 7)");
    const auto pick = static_cast<unsigned char>(r % static_cast<int>(sum));
    unsigned char run = 0;
    for (unsigned char m = 0; m < n; ++m) {
        if (BH_CALL(Battle_ActorIsOut)(m) != 0) continue;
        run = static_cast<unsigned char>(run + At(weights + (UL(At(at::kFormation)) & 0xFF) * n + m)[0]);
        if (run > pick) return m;
    }
    return static_cast<unsigned char>(n);
}

// original 0x435E10 (no PSX twin paired): the enemy actor 3..10, not out,
// with the lowest HP (+0xA4) below 0xFFFF, the first of equals; 0 for none.
extern "C" unsigned char __cdecl Battle_EnemyLowestHp(void) {
    unsigned char best = 0;
    unsigned lowest = 0xFFFF;
    for (unsigned char a = 3; a <= 10; ++a) {
        if (BH_CALL(Battle_ActorIsOut)(a) != 0) continue;
        const unsigned hp = Word(EnemyObj(a - 3u) + 0xA4);
        if (lowest > hp) {
            lowest = hp;
            best = a;
        }
    }
    return best;
}

// original 0x435E90 (no PSX twin paired): the member 0..2, not out, with the
// lowest HP (+0x98) below 10000, the first of equals; 0 for none.
extern "C" unsigned char __cdecl Battle_MemberLowestHp(void) {
    unsigned char best = 0;
    unsigned lowest = 0x2710;
    for (unsigned char m = 0; m <= 2; ++m) {
        if (BH_CALL(Battle_ActorIsOut)(m) != 0) continue;
        const unsigned hp = Word(Member(m) + 0x98);
        if (lowest > hp) {
            lowest = hp;
            best = m;
        }
    }
    return best;
}

// original 0x435E70 (no PSX twin paired): with one enemy left (0x904AB3) a
// random member (a tail jmp to Battle_RandomMember), else another enemy
// (Battle_RandomEnemy(enemy + 3)).
extern "C" unsigned char __cdecl BattleEnemy_OtherOrMember(unsigned enemy) {
    if (B(at::kEnemiesLeft) == 1) return BH_CALL(Battle_RandomMember)();
    return BH_CALL(Battle_RandomEnemy)((enemy + 3) & 0xFF);
}

// original 0x436070 (no PSX twin paired): with one enemy left 0xFF, else
// another enemy (Battle_RandomEnemy(enemy + 3)).
extern "C" unsigned char __cdecl BattleEnemy_OtherEnemy(unsigned enemy) {
    if (B(at::kEnemiesLeft) == 1) return 0xFF;
    return BH_CALL(Battle_RandomEnemy)((enemy + 3) & 0xFF);
}

// original 0x435CF0 (PSX 0x801E28CC, call-anchored): the target of enemy
// `enemy` (its low byte) by the action kind 0x904B35. Kind 1: with the
// enemy's flag word +0x90 bit 0 another enemy (BattleEnemy_OtherEnemy, a
// member when that answers 0xFF), bit 4 the weakest member, else a random
// member. Kind 4: by the ability's (+0x106, NameTable_Abilities) flag byte:
// with 0x10 a side code (0xC0 for 0x80 without 0x40, else 0x80 / 0x40 by
// 0x20 - swapped when the enemy's +0x90 bit 0 is clear); without 0x40 itself
// (enemy + 3); with 0x20 (and +0x90 bit 0) BattleEnemy_OtherOrMember, (bit 0
// clear) the +0x90 bit 4 test below; without 0x20, bit 0 set: +0x90 bit 4 the
// weakest member else a random one; bit 0 clear: the weakest enemy. Other
// kinds: the enemy's actor byte +5.
extern "C" unsigned char __cdecl BattleEnemy_PickTarget(unsigned enemy) {
    static const char* const kWho = "BattleEnemy_PickTarget";
    const unsigned kind = B(at::kActKind);
    if (kind == 1) {
        const unsigned flags = Word(EnemyArg(kWho, enemy) + 0x90);
        if ((flags & 1) != 0) {
            const unsigned char t = BH_CALL(BattleEnemy_OtherEnemy)(enemy);
            if (t != 0xFF) return t;
            return BH_CALL(Battle_RandomMember)();
        }
        if ((flags & 0x10) != 0) return BH_CALL(Battle_MemberLowestHp)();
        return BH_CALL(Battle_RandomMember)();
    }
    if (kind != 4) return EnemyArg(kWho, enemy)[5];
    const unsigned char* const e = EnemyArg(kWho, enemy);
    const unsigned id = Word(e + 0x106);
    const unsigned flags = Word(e + 0x90);
    const unsigned char a = At(at::kAbilities + id * 24)[0];
    const bool one = (flags & 1) != 0;
    if ((a & 0x10) != 0) {
        if ((a & 0x80) != 0 && (a & 0x40) == 0) return 0xC0;
        if (one) return (a & 0x20) != 0 ? 0x40 : 0x80;
        return (a & 0x20) != 0 ? 0x80 : 0x40;
    }
    if ((a & 0x40) == 0) return static_cast<unsigned char>((enemy & 0xFF) + 3);
    if (one) {
        if ((a & 0x20) != 0) return BH_CALL(BattleEnemy_OtherOrMember)(enemy);
    } else if ((a & 0x20) == 0) {
        return BH_CALL(Battle_EnemyLowestHp)();
    }
    if ((flags & 0x10) == 0) return BH_CALL(Battle_RandomMember)();
    return BH_CALL(Battle_MemberLowestHp)();
}

// original 0x435C40 (PSX 0x801E2A0C, call-anchored): the target 0x904B44 of an
// enemy that acts at random: three times in eight (Rand() & 7 below 3), or
// with one enemy left, a random member; else another enemy
// (Battle_RandomEnemy(enemy + 3)). Answers it.
extern "C" unsigned char __cdecl BattleEnemy_PickAnyTarget(unsigned enemy) {
    const unsigned r = static_cast<unsigned>(BH_CALL(Rand)()) & 7;
    unsigned char t;
    if (r < 3 || B(at::kEnemiesLeft) == 1) t = BH_CALL(Battle_RandomMember)();
    else t = BH_CALL(Battle_RandomEnemy)((enemy + 3) & 0xFF);
    B(at::kTarget) = t;
    return t;
}

// original 0x435AB0 (PSX 0x801E2564, call; Battle_BeginAction's call for an
// enemy with actor - 3): the enemy's action kind 0x904B35 - two bits of
// 0x65563C[the enemy's +0x8E] chosen by Rand() & 3; 0 when the round flags
// have 0x4000 and the enemy's u16 +0xBA is at most 1. Then 0 becomes 1; 1
// becomes 2 with the flag word +0x90's 0x80 (and u32 +0x110 |= 2), else 1; 2
// becomes 3 with +0x90's 0x40, else 1; 3 becomes 4 with +0x90's 0x20 (the
// ability u16 +0x106 = one of the eight bytes +0x9C by Rand() & 7), else 1.
// An enemy with status +0x92's 0x20 or u32 +0x114's 0x4000 acts at random:
// kind 1 and, when 0x452DD0(actor) allows, BattleEnemy_PickAnyTarget; any
// other, when 0x452DD0 allows, 0x904B44 = BattleEnemy_PickTarget. In an
// event battle the enemy becomes 0x939AD8 and Sprite_Current and its +0xF4
// hook is called with 0. A target of 0xFF clears the kind and the enemy's
// +0x105; else +0x105 = the kind.
//
// As the original has it: the enemy index is not checked (ours aborts past
// 7); the target is read after the hook.
extern "C" void __cdecl BattleEnemy_PickAction(unsigned enemy) {
    static const char* const kWho = "BattleEnemy_PickAction";
    unsigned char* const e = EnemyArg(kWho, enemy);
    const unsigned r = static_cast<unsigned>(BH_CALL(Rand)());
    unsigned char kind = static_cast<unsigned char>((At(at::kActKinds + e[0x8E])[0] >> ((r & 3) * 2)) & 3);
    B(at::kActKind) = kind;
    if ((UL(At(at::kRoundFlags)) & 0x4000) != 0 && Word(e + 0xBA) <= 1) {
        kind = 0;
        B(at::kActKind) = 0;
    }
    switch (kind) {
    case 1:
        if ((e[0x90] & 0x80) != 0) {
            B(at::kActKind) = 2;
            SetUL(e + 0x110, UL(e + 0x110) | 2);
        } else {
            B(at::kActKind) = 1;
        }
        break;
    case 2:
        B(at::kActKind) = static_cast<unsigned char>(((e[0x90] & 0x40) | 0x20) >> 5);
        break;
    case 3:
        if ((e[0x90] & 0x20) != 0) {
            B(at::kActKind) = 4;
            const unsigned r2 = static_cast<unsigned>(BH_CALL(Rand)()) & 7;
            SetWord(e + 0x106, e[0x9C + r2]);
        } else {
            B(at::kActKind) = 1;
        }
        break;
    default:
        B(at::kActKind) = 1;
        break;
    }
    const auto may_act = BH_AT(unsigned char (__cdecl*)(unsigned), at::kActorMayAct);
    const unsigned actor = (enemy + 3) & 0xFF;
    if ((e[0x92] & 0x20) != 0 || (UL(e + 0x114) & 0x4000) != 0) {
        B(at::kActKind) = 1;
        if (may_act(actor) != 0) BH_CALL(BattleEnemy_PickAnyTarget)(enemy);
    } else if (may_act(actor) != 0) {
        B(at::kTarget) = BH_CALL(BattleEnemy_PickTarget)(enemy);
    }
    if (B(at::kFight) != 0) {
        SetUL(At(at::kEnemyCurrent), Key(e));
        Sprite_Current = e;
        reinterpret_cast<boss_harness::Hook>(static_cast<std::uintptr_t>(UL(e + 0xF4)))(0);
    }
    if (B(at::kTarget) == 0xFF) {
        e[0x105] = 0;
        B(at::kActKind) = 0;
    } else {
        e[0x105] = B(at::kActKind);
    }
}

// ===========================================================================
// The enemy ops (EnemyOp_Steps 4 and 5, EnemyOp_EnterSubs 1, EnemyOp_ActSubs 3
// and 5)
// ===========================================================================

// original 0x436270 (PSX 0x801E33B0, gap44; hidden in 0x4360C0's catalogue
// extent): EnemyOp_EnterSubs 1 - through EnemyOp_EnterSubs2 0x64B1E4 by +3
// (2 entries; ours aborts past them).
extern "C" unsigned long __cdecl EnemyOp_SlideDispatch(unsigned long word) {
    return Dispatch("EnemyOp_SlideDispatch", AddressOf(EnemyOp_EnterSubs2), 2, 3, word);
}

// original 0x436290 (PSX 0x801E33F4, gap44): EnemyOp_EnterSubs2 0: +0 bit
// 0x40 cleared, animation 0; the destination +0x18 / +0x1C = the place +0x34
// / +0x38; the velocity (0x80000, 0) turned by the facing (0x446770, BE4's)
// taken off +0x34, and its second half off +0x34 again; then the velocity
// (0x8000, 0) turned; +2 on.
//
// As the original has it: the second component is subtracted from +0x34, not
// +0x38 (described in docs/battle_e2.md section 7, not changed); +2 steps,
// not +3.
extern "C" void __cdecl EnemyOp_SlideStart(void) {
    Sprite_Current[0] = static_cast<unsigned char>(Sprite_Current[0] & 0xBF);
    BH_CALL(BattleEnemy_SetAnimation)(0);
    const auto turn = BH_AT(void (__cdecl*)(unsigned char*), at::kTurnVelocity);
    SetUL(Sc() + 0x18, UL(Sc() + 0x34));
    SetUL(Sc() + 0x1C, UL(Sc() + 0x38));
    SetUL(Sc() + 0xC, 0x80000);
    SetUL(Sc() + 0x10, 0);
    turn(Sprite_Current);
    SetUL(Sc() + 0x34, UL(Sc() + 0x34) - UL(Sc() + 0xC));
    SetUL(Sc() + 0x34, UL(Sc() + 0x34) - UL(Sc() + 0x10));
    SetUL(Sc() + 0xC, 0x8000);
    SetUL(Sc() + 0x10, 0);
    turn(Sprite_Current);
    ++Sprite_Current[2];
}

// original 0x436330 (PSX 0x801E34A4, gap44): EnemyOp_EnterSubs2 1:
// BattleEnemy_ScriptTick (its answer unused); the place +0x34 / +0x38 moved
// by the velocity +0xC / +0x10; u16 +0x3E = AreaMap_Elevation there; at the
// destination +0x18 / +0x1C the steps +1..+3 = 3, 0, 0.
extern "C" void __cdecl EnemyOp_SlideStep(void) {
    BH_CALL(BattleEnemy_ScriptTick)();
    SetUL(Sc() + 0x34, UL(Sc() + 0x34) + UL(Sc() + 0xC));
    SetUL(Sc() + 0x38, UL(Sc() + 0x38) + UL(Sc() + 0x10));
    const long ground = BH_CALL(AreaMap_Elevation)(Long(Sc() + 0x34), Long(Sc() + 0x38));
    SetWord(Sprite_Current + 0x3E, static_cast<unsigned>(ground));
    unsigned char* const s = Sprite_Current;
    if (UL(s + 0x34) != UL(s + 0x18) || UL(s + 0x38) != UL(s + 0x1C)) return;
    s[1] = 3;
    Sprite_Current[2] = 0;
    Sprite_Current[3] = 0;
}

// original 0x4365D0 (PSX 0x801E38F8, gap44): EnemyOp_Steps 4 (and every
// kind's Steps 4 in the boss tables): animation 2; +9 = the byte +0x8A of the
// area's enemy data record +0xF0 of 0x939AD8 (stride 0x8C); +1 on, +2 = 0.
extern "C" void __cdecl EnemyOp_TurnStart(void) {
    BH_CALL(BattleEnemy_SetAnimation)(2);
    const unsigned record = Enemy()[0xF0];
    Sprite_Current[9] = At(at::kEnemyDataCount + record * 0x8C)[0];
    ++Sprite_Current[1];
    Sprite_Current[2] = 0;
}

// original 0x436620 (PSX 0x801E3978, gap44): EnemyOp_Steps 5 (and every
// kind's Steps 5) - through EnemyOp_Step5Subs 0x64B1F4 by +2 (2 entries; ours
// aborts past them).
extern "C" unsigned long __cdecl EnemyOp_CueDispatch(unsigned long word) {
    return Dispatch("EnemyOp_CueDispatch", AddressOf(EnemyOp_Step5Subs), 2, 2, word);
}

// original 0x436640 (PSX 0x801E39BC, the sibling's Battle_PlayCreatureCue,
// verified by reading: its script tick, the +9 count down, then the cue):
// EnemyOp_Step5Subs 0: BattleEnemy_ScriptTick (answer unused); +9 down by
// one; at 0 the cue - in an event battle 0x437450 (BE3's) with the u16 at
// 0x939AD8's +0xF8 table, else Sound_PlayEffect(0x600 + 2 x its +0xF0) - and
// +2 on.
extern "C" void __cdecl EnemyOp_PlayCreatureCue(void) {
    BH_CALL(BattleEnemy_ScriptTick)();
    Sprite_Current[9] = static_cast<unsigned char>(Sprite_Current[9] - 1);
    if (Sprite_Current[9] != 0) return;
    const unsigned char fight = B(at::kFight);
    const unsigned char* const e = Enemy();
    if (fight != 0) {
        BH_AT(void (__cdecl*)(unsigned), at::kPlayCue)(Word(PtrIn(e + 0xF8)));
    } else {
        BH_CALL(Sound_PlayEffect)(static_cast<unsigned short>(0x600 + 2 * e[0xF0]));
    }
    ++Sprite_Current[2];
}

// original 0x4366B0 (PSX 0x801E3A7C, gap44): EnemyOp_Step5Subs 1: once
// BattleEnemy_ScriptTick answers its end, Battle_SetTargetFlag40(0x904B44),
// 0x4376F0, the round flags |= 4 and a tail jmp to 0x4376A0 (both BE3's).
// Answers the tick's answer, or 0x4376A0's.
extern "C" unsigned long __cdecl EnemyOp_CueEnd(void) {
    const auto done = BH_CALL(BattleEnemy_ScriptTick)();
    if (done == 0) return done;
    BH_CALL(Battle_SetTargetFlag40)(B(at::kTarget));
    BH_AT(void (__cdecl*)(), at::kEnemyTaskChance)();
    B(at::kRoundFlags) = static_cast<unsigned char>(B(at::kRoundFlags) | 4);
    return BH_AT(unsigned long (__cdecl*)(), at::kEnemyOpEnd)();
}

// original 0x436BC0 (PSX 0x801E43E8, table-anchored; hidden in 0x436B50's
// catalogue extent): EnemyOp_ActSubs 3 (and the kinds' ActSubs 3) - through
// EnemyOp_Act3Subs 0x64B220 by +3 (5 entries; ours aborts past them).
extern "C" unsigned long __cdecl EnemyOp_Act3Dispatch(unsigned long word) {
    return Dispatch("EnemyOp_Act3Dispatch", AddressOf(EnemyOp_Act3Subs), 5, 3, word);
}

// original 0x436BE0 (PSX 0x801E442C, table-anchored): EnemyOp_Act3Subs 0:
// 0x939AD8's u16 +0x108 = 0; the velocity (-0x2000, 0) turned by the facing
// (0x446770, BE4's); +0xA = 4; BattleEnemy_ScriptTick (answer unused);
// Sound_PlayEffect(0x205); +3 on.
extern "C" void __cdecl EnemyOp_KnockStart(void) {
    SetWord(Enemy() + 0x108, 0);
    SetUL(Sc() + 0xC, 0xFFFFE000u);
    SetUL(Sc() + 0x10, 0);
    BH_AT(void (__cdecl*)(unsigned char*), at::kTurnVelocity)(Sprite_Current);
    Sprite_Current[0xA] = 4;
    BH_CALL(BattleEnemy_ScriptTick)();
    BH_CALL(Sound_PlayEffect)(0x205);
    ++Sprite_Current[3];
}

// original 0x436C40 (PSX 0x801E449C, table-anchored): EnemyOp_Act3Subs 1:
// +0xA down by one; at 0 it is 4 again and +3 on; the place +0x34 / +0x38
// moved by the velocity; a tail jmp to BattleEnemy_ScriptTick (its answer
// answered).
extern "C" unsigned char __cdecl EnemyOp_KnockBack(void) {
    Sc()[0xA] = static_cast<unsigned char>(Sc()[0xA] - 1);
    if (Sc()[0xA] == 0) {
        Sc()[0xA] = 4;
        ++Sprite_Current[3];
    }
    SetUL(Sc() + 0x34, UL(Sc() + 0x34) + UL(Sc() + 0xC));
    SetUL(Sc() + 0x38, UL(Sc() + 0x38) + UL(Sc() + 0x10));
    return BH_CALL(BattleEnemy_ScriptTick)();
}

// original 0x436C90 (PSX 0x801E4534, table-anchored): EnemyOp_Act3Subs 2:
// +0xA down by one; at 0 the hit's number - Battle_SetDamagePopup(0x939AD8's
// u16 +0x108, +5) with its +0x10C bit 4, else a kind-0 task of effect 1
// (BattleTask_Create(0, 1)) owned by Sprite_Current, +7 = 3, +0x27 = 0 - and
// +3 on; the place moved back by the velocity; BattleEnemy_ScriptTick
// (answer unused).
//
// As the original has it: BattleTask_Create's 0xFF is not tested (ours
// aborts).
extern "C" void __cdecl EnemyOp_KnockReturn(void) {
    Sc()[0xA] = static_cast<unsigned char>(Sc()[0xA] - 1);
    unsigned char* s = Sprite_Current;
    if (s[0xA] == 0) {
        const unsigned char* const e = Enemy();
        if ((e[0x10C] & 0x10) != 0) {
            BH_CALL(Battle_SetDamagePopup)(Word(e + 0x108), s[5]);
            s = Sprite_Current;
        } else {
            const unsigned slot = BH_CALL(BattleTask_Create)(0, 1);
            unsigned char* const t = NewTask("EnemyOp_KnockReturn", slot);
            s = Sprite_Current;
            SetUL(t + 0x80, Key(s));
            t[7] = 3;
            t[0x27] = 0;
        }
        s[3] = static_cast<unsigned char>(s[3] + 1);
        s = Sprite_Current;
    }
    SetUL(s + 0x34, UL(s + 0x34) - UL(s + 0xC));
    s = Sprite_Current;
    SetUL(s + 0x38, UL(s + 0x38) - UL(s + 0x10));
    BH_CALL(BattleEnemy_ScriptTick)();
}

// original 0x436D50 (PSX 0x801E4644, table-anchored): EnemyOp_Act3Subs 3:
// once BattleEnemy_ScriptTickOnce answers its end, +3 on and animation 8
// (0x939AD8's +0x110 bit 1) or 0.
extern "C" void __cdecl EnemyOp_KnockPose(void) {
    if (BH_CALL(BattleEnemy_ScriptTickOnce)() == 0) return;
    ++Sprite_Current[3];
    BH_CALL(BattleEnemy_SetAnimation)((Enemy()[0x110] & 2) != 0 ? 8u : 0u);
}

// original 0x436F00 (PSX 0x801E4880, table-anchored; hidden in 0x436B50's
// extent): EnemyOp_ActSubs 5 (and the kinds' ActSubs 5) - through
// EnemyOp_Act5Subs 0x64B244 by +3 (3 entries; ours aborts past them).
extern "C" unsigned long __cdecl EnemyOp_Act5Dispatch(unsigned long word) {
    return Dispatch("EnemyOp_Act5Dispatch", AddressOf(EnemyOp_Act5Subs), 3, 3, word);
}

// original 0x436F20 (PSX 0x801E48C4, table-anchored): EnemyOp_Act5Subs 0,
// the HP change: 0x939AD8's u16 HP +0xA4 against the s16 +0x108 - at or
// below it, HP 0 and the status +0x92 = 0x4000; else (a negative change
// with Sound_PlayEffect(0x206)) HP minus it, held at the maximum +0xB0; then
// with +0x10C bit 0 Battle_SetDamagePopup(+0x108, Sprite_Current +5);
// BattleEnemy_ScriptTick (answer unused); +3 on.
extern "C" void __cdecl EnemyOp_ApplyHpChange(void) {
    unsigned char* e = Enemy();
    const auto change = static_cast<std::int16_t>(Word(e + 0x108));
    const auto hp = static_cast<std::int32_t>(Word(e + 0xA4));
    if (hp <= change) {
        SetWord(e + 0xA4, 0);
        SetWord(Enemy() + 0x92, 0x4000);
        e = Enemy();
    } else {
        if (change < 0) {
            BH_CALL(Sound_PlayEffect)(0x206);
            e = Enemy();
        }
        SetWord(e + 0xA4, Word(e + 0xA4) - Word(e + 0x108));
        e = Enemy();
        if (Word(e + 0xA4) > Word(e + 0xB0)) {
            SetWord(e + 0xA4, Word(e + 0xB0));
            e = Enemy();
        }
    }
    if ((e[0x10C] & 1) != 0) BH_CALL(Battle_SetDamagePopup)(Word(e + 0x108), Sprite_Current[5]);
    BH_CALL(BattleEnemy_ScriptTick)();
    ++Sprite_Current[3];
}

// original 0x436FD0 (PSX 0x801E49A8, table-anchored): EnemyOp_Act5Subs 2:
// with 0x939AD8's +0x93 bit 6 the steps +1..+3 = 6, 4, 0; else
// Battle_ClearActorBit(+5) and the steps 2, 0, 0.
extern "C" void __cdecl EnemyOp_Act5End(void) {
    const unsigned char* const e = Enemy();
    unsigned char* const s = Sprite_Current;
    if ((e[0x93] & 0x40) != 0) {
        s[1] = 6;
        Sprite_Current[2] = 4;
        Sprite_Current[3] = 0;
        return;
    }
    BH_CALL(Battle_ClearActorBit)(s[5]);
    Sprite_Current[1] = 2;
    Sprite_Current[2] = 0;
    Sprite_Current[3] = 0;
}

// ============================================================================

void BattleE2_Inject() {
    if (bof3::WantsShadow("battle_e2")) battle_e2::SelfTest();
    BOF3_INJECT(BattleFx_WatchIcon);
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
    BOF3_INJECT(BattleEnemy_PickAction);
    BOF3_INJECT(BattleEnemy_PickAnyTarget);
    BOF3_INJECT(Battle_RandomEnemy);
    BOF3_INJECT(BattleEnemy_PickTarget);
    BOF3_INJECT(Battle_EnemyLowestHp);
    BOF3_INJECT(BattleEnemy_OtherOrMember);
    BOF3_INJECT(Battle_MemberLowestHp);
    BOF3_INJECT(Battle_RandomMember);
    BOF3_INJECT(BattleEnemy_OtherEnemy);
    BOF3_INJECT(EnemyOp_SlideDispatch);
    BOF3_INJECT(EnemyOp_SlideStart);
    BOF3_INJECT(EnemyOp_SlideStep);
    BOF3_INJECT(EnemyOp_TurnStart);
    BOF3_INJECT(EnemyOp_CueDispatch);
    BOF3_INJECT(EnemyOp_PlayCreatureCue);
    BOF3_INJECT(EnemyOp_CueEnd);
    BOF3_INJECT(EnemyOp_Act3Dispatch);
    BOF3_INJECT(EnemyOp_KnockStart);
    BOF3_INJECT(EnemyOp_KnockBack);
    BOF3_INJECT(EnemyOp_KnockReturn);
    BOF3_INJECT(EnemyOp_KnockPose);
    BOF3_INJECT(EnemyOp_Act5Dispatch);
    BOF3_INJECT(EnemyOp_ApplyHpChange);
    BOF3_INJECT(EnemyOp_Act5End);
}
